# src/ph7/vm_exec_ctx.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 2226/2572 lines (86.55%)

[Root index](../../index.md) | [Directory index](index.md)

|     Hits | Line | Source |
| -------: | ---: | :--- |
|        - |    1 | `/**` |
|        - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|        - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|        - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|        - |    5 | ` */` |
|        - |    6 | `/* Darwin declares the ucontext routines only when _XOPEN_SOURCE is defined` |
|        - |    7 | ` * before the first system header; _DARWIN_C_SOURCE keeps the rest of the libc` |
|        - |    8 | ` * visible, which _XOPEN_SOURCE alone would hide. */` |
|        - |    9 | `#if defined(__APPLE__) && !defined(_XOPEN_SOURCE)` |
|        - |   10 | `#define _XOPEN_SOURCE 600` |
|        - |   11 | `#define _DARWIN_C_SOURCE 1` |
|        - |   12 | `#endif` |
|        - |   13 | `#include "ph7int.h"` |
|        - |   14 | `/*` |
|        - |   15 | ` * Section:` |
|        - |   16 | ` *    Execution contexts: the parked-stack machinery shared by Fibers and` |
|        - |   17 | ` *    Generators, closure creation/binding (Closure_* builtins), the` |
|        - |   18 | ` *    Fiber_* and Generator_* builtins and the PH7_VmFiber* API.` |
|        - |   19 | ` *    Registration happens via vm.c (aVmFunc[]/class installs).` |
|        - |   20 | ` * Status:` |
|        - |   21 | ` *    Stable.` |
|        - |   22 | ` */` |
|        - |   23 | `/*` |
|        - |   24 | ` * ---------------------------------------------------------------------------` |
|        - |   25 | ` * Real coroutine stacks: a fiber body on a native stack of its own.` |
|        - |   26 | ` * ---------------------------------------------------------------------------` |
|        - |   27 | ` * See the PH7_CORO_STACK block in ph7int.h for WHY. This is the machinery:` |
|        - |   28 | ` *` |
|        - |   29 | ` *   VmCoroNew / VmCoroFree        one switchable native stack` |
|        - |   30 | ` *   VmCoroEnter / VmCoroLeave     the switch itself, in both directions` |
|        - |   31 | ` *   VmCoroStateInit / Swap        the VM state that belongs to whichever runs` |
|        - |   32 | ` *   VmCoroRun                     the resumer's side of a start / resume / kill` |
|        - |   33 | ` *   VmCoroBody                    the fiber's side, entered once` |
|        - |   34 | ` *` |
|        - |   35 | ` * All of it compiles out where no stack-switch primitive exists, and a fiber` |
|        - |   36 | ` * that does not get a stack (an allocation failure; a generator, which never` |
|        - |   37 | ` * asks for one) falls back to the record-parking path. So the two models` |
|        - |   38 | `` * coexist and `pCtx->pCoro != 0` is the discriminator every caller tests.`` |
|        - |   39 | ` */` |
|        - |   40 | `#ifdef PH7_CORO_STACK` |
|        - |   41 | `#ifdef PH7_CORO_UCONTEXT` |
|        - |   42 | `#include <ucontext.h>` |
|        - |   43 | `#if defined(__APPLE__) && defined(__clang__)` |
|        - |   44 | `/* ...and marks every one of them deprecated, which -Werror turns into a` |
|        - |   45 | ` * failed build. */` |
|        - |   46 | `#pragma clang diagnostic ignored "-Wdeprecated-declarations"` |
|        - |   47 | `#endif` |
|        - |   48 | `#endif` |
|        - |   49 | `#if defined(PH7_CORO_UCONTEXT) \|\| defined(PH7_CORO_ASM_X64)` |
|        - |   50 | `#include <sys/mman.h>` |
|        - |   51 | `#include <unistd.h>` |
|        - |   52 | `#endif` |
|        - |   53 | `#ifdef PH7_CORO_WIN32` |
|        - |   54 | `#include <windows.h>` |
|        - |   55 | `#endif` |
|        - |   56 | `/* ASan keeps its own idea of where the stack is, and moves locals whose address` |
|        - |   57 | ` * escapes onto a "fake stack" it tracks per stack. A switch it is not told` |
|        - |   58 | ` * about makes it read the other side's frames as use-after-return, so every` |
|        - |   59 | ` * switch is announced. The protocol is the documented one: start_switch_fiber` |
|        - |   60 | ` * names where we are GOING and hands back a token; finish_switch_fiber runs on` |
|        - |   61 | ` * ARRIVAL with the token of the switch that got us there -- which, for a switch` |
|        - |   62 | ` * that eventually comes back, is a local of the function that made it. */` |
|        - |   63 | `#if defined(__has_feature)` |
|        - |   64 | `# if __has_feature(address_sanitizer)` |
|        - |   65 | `#  define PH7_CORO_ASAN 1` |
|        - |   66 | `# endif` |
|        - |   67 | `#endif` |
|        - |   68 | `#if defined(__SANITIZE_ADDRESS__) && !defined(PH7_CORO_ASAN)` |
|        - |   69 | `# define PH7_CORO_ASAN 1` |
|        - |   70 | `#endif` |
|        - |   71 | `#ifdef PH7_CORO_ASAN` |
|        - |   72 | `#include <sanitizer/common_interface_defs.h>` |
|        - |   73 | `#endif` |
|        - |   74 | `/*` |
|        - |   75 | ` * How big a fiber's native stack is, and why it is not a constant.` |
|        - |   76 | ` *` |
|        - |   77 | ` * A fiber's stack holds what the trampoline does NOT flatten -- eval/include` |
|        - |   78 | ` * towers, C->PHP callbacks, nested coroutine starts -- which is exactly what` |
|        - |   79 | ` * nMaxNativeDepth counts, and a fiber's depth counter starts at zero on its own` |
|        - |   80 | ` * stack. So the stack has to be big enough that the ENGINE's clean fatal` |
|        - |   81 | ` * ("Maximum native nesting depth reached") always fires before the guard page` |
|        - |   82 | ` * does; otherwise a deep enough callback tower inside a fiber is a SIGSEGV` |
|        - |   83 | ` * where the same tower outside one is a diagnostic.` |
|        - |   84 | ` *` |
|        - |   85 | ` * Hence: bytes per allowed level, times the cap. The per-level figure was` |
|        - |   86 | ``  * measured on the fattest thing in the tree -- a self-recursive `array_map` `` |
|        - |   87 | ` * callback, which spends a builtin frame, a native VmByteCodeExec activation` |
|        - |   88 | ` * and an operand stack on every level: ~5.7 KB at -O3 and ~14 KB under` |
|        - |   89 | ` * ASan+UBSan at -O1 -g. 32 KB carries better than a 2x margin over the worse of` |
|        - |   90 | ` * the two. At the host default cap of 256 that is 8 MB of ADDRESS SPACE per live` |
|        - |   91 | ` * fiber; both platforms map it lazily, so an idle fiber's resident cost is the` |
|        - |   92 | ` * page it is parked on.` |
|        - |   93 | ` */` |
|        - |   94 | `#ifndef PH7_CORO_STACK_PER_LEVEL` |
|        - |   95 | `#define PH7_CORO_STACK_PER_LEVEL (32 * 1024)` |
|        - |   96 | `#endif` |
|        - |   97 | `#ifndef PH7_CORO_STACK_MIN` |
|        - |   98 | `#define PH7_CORO_STACK_MIN (256 * 1024)` |
|        - |   99 | `#endif` |
|        - |  100 | `#ifndef PH7_CORO_STACK_MAX` |
|        - |  101 | `#define PH7_CORO_STACK_MAX (32 * 1024 * 1024)` |
|        - |  102 | `#endif` |
|      432 |  103 | `static sxu32 VmCoroStackBytes(ph7_vm *pVm)` |
|        5 |  104 | `{` |
|      869 |  105 | `	sxi64 nWant = (sxi64)(pVm->nMaxNativeDepth > 0 ? pVm->nMaxNativeDepth : 256)` |
|      432 |  106 | `		* (sxi64)PH7_CORO_STACK_PER_LEVEL;` |
|      437 |  107 | `	if( nWant < PH7_CORO_STACK_MIN ){` |
|      ! 0 |  108 | `		nWant = PH7_CORO_STACK_MIN;` |
|      ! 0 |  109 | `	}` |
|      437 |  110 | `	if( nWant > PH7_CORO_STACK_MAX ){` |
|      ! 0 |  111 | `		nWant = PH7_CORO_STACK_MAX;` |
|      ! 0 |  112 | `	}` |
|      437 |  113 | `	return (sxu32)nWant;` |
|        5 |  114 | `}` |
|        - |  115 | `struct VmCoro` |
|        - |  116 | `{` |
|        - |  117 | `#ifdef PH7_CORO_UCONTEXT` |
|        - |  118 | `	ucontext_t sBack;       /* the resumer's context: where a switch-out goes */` |
|        - |  119 | `	ucontext_t sSelf;       /* the fiber's own */` |
|        - |  120 | `#endif` |
|        - |  121 | `#ifdef PH7_CORO_ASM_X64` |
|        - |  122 | `	void *pBackSp;          /* the resumer's saved stack pointer (switch-out target) */` |
|        - |  123 | `	void *pSelfSp;          /* the fiber's own */` |
|        - |  124 | `#endif` |
|        - |  125 | `#if defined(PH7_CORO_UCONTEXT) \|\| defined(PH7_CORO_ASM_X64)` |
|        - |  126 | `	void *pMap;             /* mmap base: one guard page, then the stack */` |
|        - |  127 | `	sxu32 nMap;             /* its length, for munmap */` |
|        - |  128 | `#endif` |
|        - |  129 | `#ifdef PH7_CORO_WIN32` |
|        - |  130 | `	void *pFiber;           /* CreateFiber handle */` |
|        - |  131 | `	void *pBack;            /* the resumer's fiber handle, valid while we run */` |
|        - |  132 | `#endif` |
|        - |  133 | `	void *pStack;           /* usable stack, low address (0 where the platform owns it) */` |
|        - |  134 | `	sxu32 nStack;           /* usable stack bytes */` |
|        - |  135 | `#ifdef PH7_CORO_ASAN` |
|        - |  136 | `	const void *pHostStack; /* where the resumer's stack is: learned on arrival, */` |
|        - |  137 | `	sxu32 nHostStack;       /* and needed to announce the switch back to it */` |
|        - |  138 | `#endif` |
|        - |  139 | `};` |
|        - |  140 | `#ifdef PH7_CORO_ASAN` |
|        - |  141 | `/*` |
|        - |  142 | ` * Arriving on a stack. pTok closes the switch that brought us here (0 when the` |
|        - |  143 | ` * arrival is a fiber's FIRST entry: the enterer keeps its own token for its own` |
|        - |  144 | ` * return). The out-params say where we came from, which is the only way the` |
|        - |  145 | ` * fiber can learn the resumer's stack bounds to announce the switch back.` |
|        - |  146 | ` */` |
|        - |  147 | `static void VmCoroAsanArrive(VmCoro *pCoro, void *pTok)` |
|        - |  148 | `{` |
|        - |  149 | `	const void *pFrom = 0;` |
|        - |  150 | `	size_t nFrom = 0;` |
|        - |  151 | `	__sanitizer_finish_switch_fiber(pTok, &pFrom, &nFrom);` |
|        - |  152 | `	if( pCoro && pFrom ){` |
|        - |  153 | `		pCoro->pHostStack = pFrom;` |
|        - |  154 | `		pCoro->nHostStack = (sxu32)nFrom;` |
|        - |  155 | `	}` |
|        - |  156 | `}` |
|        - |  157 | `#define VM_CORO_ASAN_TOKEN            void *pTok = 0` |
|        - |  158 | `#define VM_CORO_ASAN_GO(TOK,BOT,SIZ)  __sanitizer_start_switch_fiber((TOK),(BOT),(size_t)(SIZ))` |
|        - |  159 | `#else` |
|        - |  160 | `#define VmCoroAsanArrive(C,T)         ((void)0)` |
|        - |  161 | `#define VM_CORO_ASAN_TOKEN            int iUnusedTok = 0; (void)iUnusedTok` |
|        - |  162 | `#define VM_CORO_ASAN_GO(TOK,BOT,SIZ)  ((void)0)` |
|        - |  163 | `#endif` |
|        - |  164 | `static void VmCoroBody(ph7_exec_ctx *pCtx);` |
|        - |  165 | `static ph7_vm_func * VmCtxEnforceRetFunc(ph7_exec_ctx *pCtx); /* defined below, with the rest of the shared coroutine epilogue */` |
|        - |  166 | `#ifdef PH7_CORO_ASM_X64` |
|        - |  167 | `/*` |
|        - |  168 | ` * The written switch, x86-64 System V.` |
|        - |  169 | ` *` |
|        - |  170 | ` * PH7_CoroSwitch(void **ppSave, void *pTarget) pushes the six callee-saved` |
|        - |  171 | ` * registers and the two floating-point control words, writes the resulting` |
|        - |  172 | ` * stack pointer through ppSave, adopts pTarget as the stack, and pops the` |
|        - |  173 | ` * mirror image -- so it "returns" wherever that other stack's saved frame says.` |
|        - |  174 | ` * That is the whole of a coroutine switch: everything else a C frame owns is` |
|        - |  175 | ` * already on the stack it lives on.` |
|        - |  176 | ` *` |
|        - |  177 | ` * PH7_CoroEntryStub is the address a FRESH stack's frame returns to. The seeded` |
|        - |  178 | ` * frame carries the ph7_exec_ctx in the r12 slot (a callee-saved register is` |
|        - |  179 | ` * the only place a value can ride through the switch), so the stub moves it` |
|        - |  180 | ` * into the first argument register and calls into C, which never comes back.` |
|        - |  181 | ` *` |
|        - |  182 | `` * `endbr64` on both so an IBT-enforcing loader is satisfied. A CET SHADOW stack`` |
|        - |  183 | ` * is the case this cannot serve -- a return to a frame the shadow stack never` |
|        - |  184 | ` * saw -- and PH7_DISABLE_CORO_ASM is the way back to glibc's swapcontext, which` |
|        - |  185 | ` * knows about it.` |
|        - |  186 | ` */` |
|        - |  187 | `extern void PH7_CoroSwitch(void **ppSave, void *pTarget);` |
|        - |  188 | `extern void PH7_CoroEntryStub(void);` |
|        - |  189 | `PH7_PRIVATE void PH7_CoroEntryC(ph7_exec_ctx *pCtx); /* named by the stub, so not static */` |
|        - |  190 | `__asm__(` |
|        - |  191 | `	".text\n"` |
|        - |  192 | `	".globl PH7_CoroSwitch\n"` |
|        - |  193 | `	".hidden PH7_CoroSwitch\n"` |
|        - |  194 | `	".type PH7_CoroSwitch,@function\n"` |
|        - |  195 | `	".align 16\n"` |
|        - |  196 | `	"PH7_CoroSwitch:\n"` |
|        - |  197 | `	"	endbr64\n"` |
|        - |  198 | `	"	pushq %rbp\n"` |
|        - |  199 | `	"	pushq %rbx\n"` |
|        - |  200 | `	"	pushq %r15\n"` |
|        - |  201 | `	"	pushq %r14\n"` |
|        - |  202 | `	"	pushq %r13\n"` |
|        - |  203 | `	"	pushq %r12\n"` |
|        - |  204 | `	"	subq $8, %rsp\n"` |
|        - |  205 | `	"	stmxcsr (%rsp)\n"` |
|        - |  206 | `	"	fnstcw 4(%rsp)\n"` |
|        - |  207 | `	"	movq %rsp, (%rdi)\n"` |
|        - |  208 | `	"	movq %rsi, %rsp\n"` |
|        - |  209 | `	"	ldmxcsr (%rsp)\n"` |
|        - |  210 | `	"	fldcw 4(%rsp)\n"` |
|        - |  211 | `	"	addq $8, %rsp\n"` |
|        - |  212 | `	"	popq %r12\n"` |
|        - |  213 | `	"	popq %r13\n"` |
|        - |  214 | `	"	popq %r14\n"` |
|        - |  215 | `	"	popq %r15\n"` |
|        - |  216 | `	"	popq %rbx\n"` |
|        - |  217 | `	"	popq %rbp\n"` |
|        - |  218 | `	"	ret\n"` |
|        - |  219 | `	".size PH7_CoroSwitch,.-PH7_CoroSwitch\n"` |
|        - |  220 | `	".globl PH7_CoroEntryStub\n"` |
|        - |  221 | `	".hidden PH7_CoroEntryStub\n"` |
|        - |  222 | `	".type PH7_CoroEntryStub,@function\n"` |
|        - |  223 | `	".align 16\n"` |
|        - |  224 | `	"PH7_CoroEntryStub:\n"` |
|        - |  225 | `	"	endbr64\n"` |
|        - |  226 | `	"	movq %r12, %rdi\n"` |
|        - |  227 | `	"	andq $-16, %rsp\n"` |
|        - |  228 | `	"	call PH7_CoroEntryC\n"` |
|        - |  229 | `	"	hlt\n"` |
|        - |  230 | `	".size PH7_CoroEntryStub,.-PH7_CoroEntryStub\n"` |
|        - |  231 | `);` |
|      216 |  232 | `PH7_PRIVATE void PH7_CoroEntryC(ph7_exec_ctx *pCtx)` |
|        - |  233 | `{` |
|      216 |  234 | `	VmCoroBody(pCtx);` |
|      ! 0 |  235 | `}` |
|        - |  236 | `/* Where the seeded frame's eight words sit, counted from the fiber's saved sp:` |
|        - |  237 | ` * the control-word pair, then r12 (which carries the ctx), r13, r14, r15, rbx,` |
|        - |  238 | `` * rbp, and finally the address PH7_CoroSwitch's `ret` will take. */`` |
|        - |  239 | `#define VM_CORO_X64_FRAME_WORDS  8` |
|        - |  240 | `#define VM_CORO_X64_SLOT_CTL     0` |
|        - |  241 | `#define VM_CORO_X64_SLOT_R12     1` |
|        - |  242 | `#define VM_CORO_X64_SLOT_RET     7` |
|        - |  243 | `#endif /* PH7_CORO_ASM_X64 */` |
|        - |  244 | `#ifdef PH7_CORO_UCONTEXT` |
|        - |  245 | `/*` |
|        - |  246 | ` * makecontext() passes int arguments only, so the context pointer travels as` |
|        - |  247 | ` * two of them -- the portable idiom, and the reason this is not a plain` |
|        - |  248 | ` * one-argument entry. A VM-wide "the fiber about to boot" field would be` |
|        - |  249 | ` * shorter and is not thread-safe, which PH7_ENABLE_THREADS builds care about.` |
|        - |  250 | ` */` |
|      216 |  251 | `static void VmCoroUcEntry(unsigned int iHi, unsigned int iLo)` |
|        - |  252 | `{` |
|      216 |  253 | `	sxu64 uPtr = (((sxu64)iHi) << 32) \| (sxu64)iLo;` |
|      216 |  254 | `	VmCoroBody((ph7_exec_ctx *)(size_t)uPtr);` |
|      216 |  255 | `}` |
|        - |  256 | `#endif` |
|        - |  257 | `#ifdef PH7_CORO_WIN32` |
|        - |  258 | `static VOID CALLBACK VmCoroWinEntry(PVOID pArg)` |
|        5 |  259 | `{` |
|        5 |  260 | `	VmCoroBody((ph7_exec_ctx *)pArg);` |
|      ! 0 |  261 | `}` |
|        - |  262 | `#endif` |
|        - |  263 | `/*` |
|        - |  264 | ` * Allocate one switchable stack. NULL is not fatal: the caller runs the body on` |
|        - |  265 | ` * the shared stack instead, where a suspend across a C boundary keeps raising` |
|        - |  266 | ` * the FiberError it always raised.` |
|        - |  267 | ` */` |
|      432 |  268 | `static VmCoro * VmCoroNew(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        5 |  269 | `{` |
|      437 |  270 | `	VmCoro *pCoro = (VmCoro *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(VmCoro));` |
|      437 |  271 | `	if( pCoro == 0 ){` |
|      ! 0 |  272 | `		return 0;` |
|        - |  273 | `	}` |
|      437 |  274 | `	SyZero(pCoro, sizeof(VmCoro));` |
|        - |  275 | `#if defined(PH7_CORO_UCONTEXT) \|\| defined(PH7_CORO_ASM_X64)` |
|        - |  276 | `	{` |
|      432 |  277 | `		long nPage = sysconf(_SC_PAGESIZE);` |
|      432 |  278 | `		sxu32 nGuard = (nPage > 0) ? (sxu32)nPage : 4096;` |
|      432 |  279 | `		sxu32 nStack = VmCoroStackBytes(pVm);` |
|        - |  280 | `		void *pMap;` |
|        - |  281 | `		/* One PROT_NONE page below the stack turns an overflow into a clean fault` |
|        - |  282 | `		 * at the guard rather than a silent write into whatever the allocator put` |
|        - |  283 | `		 * next door. nMaxNativeDepth is the engine's own net; this is the floor` |
|        - |  284 | `		 * under it. */` |
|      432 |  285 | `		pMap = mmap(0, (size_t)nGuard + (size_t)nStack, PROT_READ \| PROT_WRITE,` |
|        - |  286 | `			MAP_PRIVATE \| MAP_ANONYMOUS, -1, 0);` |
|      432 |  287 | `		if( pMap == MAP_FAILED ){` |
|      ! 0 |  288 | `			SyMemBackendFree(&pVm->sAllocator, pCoro);` |
|      ! 0 |  289 | `			return 0;` |
|        - |  290 | `		}` |
|      432 |  291 | `		(void)mprotect(pMap, (size_t)nGuard, PROT_NONE);` |
|      432 |  292 | `		pCoro->pMap = pMap;` |
|      432 |  293 | `		pCoro->nMap = nGuard + nStack;` |
|      432 |  294 | `		pCoro->pStack = (void *)((char *)pMap + nGuard);` |
|      432 |  295 | `		pCoro->nStack = nStack;` |
|        - |  296 | `	}` |
|        - |  297 | `#endif` |
|        - |  298 | `#ifdef PH7_CORO_ASM_X64` |
|        - |  299 | `	{` |
|        - |  300 | `		/* Seed the frame PH7_CoroSwitch will pop the first time this stack is` |
|        - |  301 | `		 * entered: the ctx in the r12 slot, the entry stub as the return address,` |
|        - |  302 | `		 * and THIS thread's floating-point control words so the fiber starts with` |
|        - |  303 | `		 * the rounding/precision modes its creator had. */` |
|      216 |  304 | `		void **aTop = (void **)(((size_t)pCoro->pStack + pCoro->nStack) & ~(size_t)15);` |
|      216 |  305 | `		void **aFrame = aTop - VM_CORO_X64_FRAME_WORDS;` |
|        - |  306 | `		unsigned int nMxcsr;` |
|        - |  307 | `		unsigned short nFcw;` |
|        - |  308 | `		sxu32 i;` |
|      216 |  309 | `		__asm__ __volatile__("stmxcsr %0" : "=m"(nMxcsr));` |
|      216 |  310 | `		__asm__ __volatile__("fnstcw %0" : "=m"(nFcw));` |
|     1944 |  311 | `		for( i = 0; i < VM_CORO_X64_FRAME_WORDS; i++ ){` |
|     1728 |  312 | `			aFrame[i] = 0;` |
|        - |  313 | `		}` |
|      216 |  314 | `		SyMemcpy((const void *)&nMxcsr, (void *)&aFrame[VM_CORO_X64_SLOT_CTL], sizeof(nMxcsr));` |
|      216 |  315 | `		SyMemcpy((const void *)&nFcw,` |
|        - |  316 | `			(void *)((char *)&aFrame[VM_CORO_X64_SLOT_CTL] + 4), sizeof(nFcw));` |
|      216 |  317 | `		aFrame[VM_CORO_X64_SLOT_R12] = (void *)pCtx;` |
|      216 |  318 | `		aFrame[VM_CORO_X64_SLOT_RET] = (void *)PH7_CoroEntryStub;` |
|      216 |  319 | `		pCoro->pSelfSp = (void *)aFrame;` |
|        - |  320 | `	}` |
|        - |  321 | `#endif` |
|        - |  322 | `#ifdef PH7_CORO_UCONTEXT` |
|        - |  323 | `	{` |
|        - |  324 | `		sxu64 uPtr;` |
|      216 |  325 | `		if( getcontext(&pCoro->sSelf) != 0 ){` |
|      ! 0 |  326 | `			munmap(pCoro->pMap, (size_t)pCoro->nMap);` |
|      ! 0 |  327 | `			SyMemBackendFree(&pVm->sAllocator, pCoro);` |
|      ! 0 |  328 | `			return 0;` |
|        - |  329 | `		}` |
|      216 |  330 | `		pCoro->sSelf.uc_stack.ss_sp = pCoro->pStack;` |
|      216 |  331 | `		pCoro->sSelf.uc_stack.ss_size = (size_t)pCoro->nStack;` |
|        - |  332 | `		/* uc_link stays NULL on purpose: the body never falls off its entry, it` |
|        - |  333 | `		 * makes the last switch itself (VmCoroBody), so that arrival is announced` |
|        - |  334 | `		 * to ASan like every other one. */` |
|      216 |  335 | `		pCoro->sSelf.uc_link = 0;` |
|      216 |  336 | `		uPtr = (sxu64)(size_t)pCtx;` |
|      432 |  337 | `		makecontext(&pCoro->sSelf, (void (*)(void))VmCoroUcEntry, 2,` |
|      216 |  338 | `			(unsigned int)(uPtr >> 32), (unsigned int)(uPtr & 0xFFFFFFFFu));` |
|        - |  339 | `	}` |
|        - |  340 | `#endif` |
|        - |  341 | `#ifdef PH7_CORO_WIN32` |
|        - |  342 | `	/* CreateFiber wants the calling THREAD to be a fiber before anything can be` |
|        - |  343 | `	 * switched to. That conversion is per-thread and is not undone: reversing it` |
|        - |  344 | `	 * is only safe with no fiber left alive anywhere, which one Fiber object` |
|        - |  345 | `	 * cannot know, and it costs an unconverted thread a few dozen bytes. */` |
|        5 |  346 | `	if( !IsThreadAFiber() ){` |
|        5 |  347 | `		if( ConvertThreadToFiber(0) == 0 ){` |
|      ! 0 |  348 | `			SyMemBackendFree(&pVm->sAllocator, pCoro);` |
|      ! 0 |  349 | `			return 0;` |
|        - |  350 | `		}` |
|        - |  351 | `	}` |
|        5 |  352 | `	pCoro->nStack = VmCoroStackBytes(pVm);` |
|        - |  353 | `	/* RESERVE the whole thing, COMMIT nothing (the 0): Windows grows a fiber` |
|        - |  354 | `	 * stack on demand exactly like a thread's, so the size above is address` |
|        - |  355 | `	 * space. FIBER_FLAG_FLOAT_SWITCH is what makes the switch carry the` |
|        - |  356 | `	 * floating-point state, which the written x86-64 switch does by hand. */` |
|        5 |  357 | `	pCoro->pFiber = (void *)CreateFiberEx((SIZE_T)0, (SIZE_T)pCoro->nStack,` |
|        - |  358 | `		FIBER_FLAG_FLOAT_SWITCH, VmCoroWinEntry, (LPVOID)pCtx);` |
|        5 |  359 | `	if( pCoro->pFiber == 0 ){` |
|      ! 0 |  360 | `		SyMemBackendFree(&pVm->sAllocator, pCoro);` |
|      ! 0 |  361 | `		return 0;` |
|        - |  362 | `	}` |
|        - |  363 | `	/* Win32 owns the mapping, so there is no bottom address to hand out -- and` |
|        - |  364 | `	 * the MSVC build has no ASan fiber annotations to hand it to. */` |
|        5 |  365 | `	pCoro->pStack = 0;` |
|        - |  366 | `#endif` |
|      437 |  367 | `	return pCoro;` |
|      221 |  368 | `}` |
|        - |  369 | `/*` |
|        - |  370 | ` * Give the stack back. Only ever reached with the fiber not running: its body` |
|        - |  371 | ` * ran off the end, or the teardown unwound it first (see the kill switch).` |
|        - |  372 | ` */` |
|      432 |  373 | `static void VmCoroFree(ph7_vm *pVm, VmCoro *pCoro)` |
|        5 |  374 | `{` |
|      437 |  375 | `	if( pCoro == 0 ){` |
|      ! 0 |  376 | `		return;` |
|        - |  377 | `	}` |
|        - |  378 | `#if defined(PH7_CORO_UCONTEXT) \|\| defined(PH7_CORO_ASM_X64)` |
|      432 |  379 | `	if( pCoro->pMap ){` |
|      432 |  380 | `		munmap(pCoro->pMap, (size_t)pCoro->nMap);` |
|      216 |  381 | `	}` |
|        - |  382 | `#endif` |
|        - |  383 | `#ifdef PH7_CORO_WIN32` |
|        5 |  384 | `	if( pCoro->pFiber ){` |
|        5 |  385 | `		DeleteFiber((LPVOID)pCoro->pFiber);` |
|        - |  386 | `	}` |
|        - |  387 | `#endif` |
|      437 |  388 | `	SyMemBackendFree(&pVm->sAllocator, pCoro);` |
|      221 |  389 | `}` |
|        - |  390 | `/*` |
|        - |  391 | ` * Switch onto the fiber's stack. Returns when the fiber switches back, because` |
|        - |  392 | ` * it suspended or because its body finished.` |
|        - |  393 | ` */` |
|      878 |  394 | `static void VmCoroEnter(VmCoro *pCoro)` |
|        5 |  395 | `{` |
|      883 |  396 | `	VM_CORO_ASAN_TOKEN;` |
|        - |  397 | `	VM_CORO_ASAN_GO(&pTok, pCoro->pStack, pCoro->nStack);` |
|        - |  398 | `#if defined(PH7_CORO_WIN32)` |
|        5 |  399 | `	pCoro->pBack = GetCurrentFiber();` |
|        5 |  400 | `	SwitchToFiber((LPVOID)pCoro->pFiber);` |
|        - |  401 | `#elif defined(PH7_CORO_ASM_X64)` |
|      439 |  402 | `	PH7_CoroSwitch(&pCoro->pBackSp, pCoro->pSelfSp);` |
|        - |  403 | `#else` |
|      439 |  404 | `	swapcontext(&pCoro->sBack, &pCoro->sSelf);` |
|        - |  405 | `#endif` |
|        - |  406 | `	VmCoroAsanArrive(0, pTok);` |
|      883 |  407 | `}` |
|        - |  408 | `/*` |
|        - |  409 | ` * ...and back, from inside the fiber. Returns when the fiber is entered again.` |
|        - |  410 | ` */` |
|      446 |  411 | `static void VmCoroLeave(VmCoro *pCoro)` |
|        5 |  412 | `{` |
|      451 |  413 | `	VM_CORO_ASAN_TOKEN;` |
|        - |  414 | `	VM_CORO_ASAN_GO(&pTok, pCoro->pHostStack, pCoro->nHostStack);` |
|        - |  415 | `#if defined(PH7_CORO_WIN32)` |
|        5 |  416 | `	SwitchToFiber((LPVOID)pCoro->pBack);` |
|        - |  417 | `#elif defined(PH7_CORO_ASM_X64)` |
|      223 |  418 | `	PH7_CoroSwitch(&pCoro->pSelfSp, pCoro->pBackSp);` |
|        - |  419 | `#else` |
|      223 |  420 | `	swapcontext(&pCoro->sSelf, &pCoro->sBack);` |
|        - |  421 | `#endif` |
|        - |  422 | `	VmCoroAsanArrive(pCoro, pTok);` |
|      451 |  423 | `}` |
|        - |  424 | `/*` |
|        - |  425 | ` * The last switch a fiber makes: the body is over, so the stack is spent and` |
|        - |  426 | ` * ASan is told to DISCARD this side's state rather than save it (the null` |
|        - |  427 | ` * token). Never returns.` |
|        - |  428 | ` */` |
|      432 |  429 | `static void VmCoroLeaveFinal(VmCoro *pCoro)` |
|        5 |  430 | `{` |
|        - |  431 | `	VM_CORO_ASAN_GO(0, pCoro->pHostStack, pCoro->nHostStack);` |
|        - |  432 | `#if defined(PH7_CORO_WIN32)` |
|        5 |  433 | `	SwitchToFiber((LPVOID)pCoro->pBack);` |
|        - |  434 | `#elif defined(PH7_CORO_ASM_X64)` |
|        - |  435 | `	{` |
|        - |  436 | `		/* Nothing on this stack will ever be resumed, so the save slot is a` |
|        - |  437 | `		 * scratch word rather than pSelfSp -- writing that would leave a live` |
|        - |  438 | `		 * frame pointer on a dead stack. */` |
|      216 |  439 | `		void *pDead = 0;` |
|      216 |  440 | `		PH7_CoroSwitch(&pDead, pCoro->pBackSp);` |
|        - |  441 | `	}` |
|        - |  442 | `#else` |
|      216 |  443 | `	setcontext(&pCoro->sBack);` |
|        - |  444 | `#endif` |
|      216 |  445 | `}` |
|        - |  446 | `/*` |
|        - |  447 | ` * The VM state that belongs to whichever side is running (see VmCoroVmState).` |
|        - |  448 | ` * One list, used three ways, so a field can never be saved and not restored.` |
|        - |  449 | ` */` |
|        - |  450 | `#define VM_CORO_STATE_FIELDS(_) \` |
|        - |  451 | `	_(aException) _(aFinallyAction) _(aSelf) \` |
|        - |  452 | `	_(nVmExecDepth) _(nRecursionDepth) _(nCurLine) _(nBoundaryRc) \` |
|        - |  453 | `	_(pCalleeName) _(pNativeFrameName) _(bHostDiscard) _(nErrSuppress) \` |
|        - |  454 | `	_(nExceptDepth) _(nExcCtorDepth) _(nMuteThrow) _(nSpeculative) \` |
|        - |  455 | `	_(nConstEvalDepth) _(nLazyInitLine) _(nLazyInitDepth) \` |
|        - |  456 | `	_(nObDepth) _(nObActive) _(pObFrame) _(pCoroCtx)` |
|        - |  457 | `/*` |
|        - |  458 | ` * Seed the fiber side for a body that has not run yet: three empty stacks of` |
|        - |  459 | ` * its own, a C stack nothing is live on, and every other scalar inherited from` |
|        - |  460 | ` * the site that is starting it -- which is what running the body inline used to` |
|        - |  461 | ` * give it.` |
|        - |  462 | ` */` |
|      432 |  463 | `static void VmCoroStateInit(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        5 |  464 | `{` |
|      437 |  465 | `	VmCoroVmState *pS = &pCtx->sSaved;` |
|        - |  466 | `#define VM_CORO_INHERIT(F)  pS->F = pVm->F;` |
|      437 |  467 | `	VM_CORO_STATE_FIELDS(VM_CORO_INHERIT)` |
|        - |  468 | `#undef VM_CORO_INHERIT` |
|      437 |  469 | `	SySetInit(&pS->aException, &pVm->sAllocator, sizeof(ph7_exception *));` |
|      437 |  470 | `	SySetInit(&pS->aFinallyAction, &pVm->sAllocator, sizeof(VmFinallyAction));` |
|      437 |  471 | `	SySetInit(&pS->aSelf, &pVm->sAllocator, sizeof(ph7_class *));` |
|        - |  472 | `	/* A fresh C stack: no native activation is live on it and no PHP call is` |
|        - |  473 | `	 * open, so both guards start from zero and measure THIS stack. */` |
|      437 |  474 | `	pS->nVmExecDepth = 0;` |
|      437 |  475 | `	pS->nRecursionDepth = 0;` |
|        - |  476 | `	/* Nothing of the resumer's in-flight C state is the fiber's: the parked` |
|        - |  477 | `	 * boundary throw belongs to the interrupted exec, the two callee-name latches` |
|        - |  478 | `	 * are consumed by the next call the RESUMER makes, and the lazy-initializer` |
|        - |  479 | `	 * line override is keyed on the other stack's native depth. */` |
|      437 |  480 | `	pS->nBoundaryRc = 0;` |
|      437 |  481 | `	pS->pCalleeName = 0;` |
|      437 |  482 | `	pS->pNativeFrameName = 0;` |
|      437 |  483 | `	pS->bHostDiscard = 0;` |
|      437 |  484 | `	pS->nLazyInitLine = 0;` |
|      437 |  485 | `	pS->nLazyInitDepth = 0;` |
|        - |  486 | `	/* ...and this side IS the fiber. */` |
|      437 |  487 | `	pS->pCoroCtx = pCtx;` |
|      437 |  488 | `}` |
|        - |  489 | `/*` |
|        - |  490 | ` * Release what the fiber side owns. Its three stacks die with the body: at a` |
|        - |  491 | ` * clean end they are empty, but a body that ABORTED can still be holding an` |
|        - |  492 | ` * unconsumed finally action (a queued return's value, a rethrow's exception` |
|        - |  493 | ` * reference) and the per-activation exception clones stage 2b made.` |
|        - |  494 | ` */` |
|     1722 |  495 | `static void VmCoroStateRelease(ph7_vm *pVm, VmCoroVmState *pS)` |
|        5 |  496 | `{` |
|     1727 |  497 | `	sxu32 n = SySetUsed(&pS->aFinallyAction);` |
|     1727 |  498 | `	if( n > 0 ){` |
|      ! 0 |  499 | `		VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pS->aFinallyAction);` |
|        - |  500 | `		sxu32 i;` |
|      ! 0 |  501 | `		for( i = 0; i < n; i++ ){` |
|      ! 0 |  502 | `			if( aA[i].eKind == PH7_FA_RETURN ){` |
|      ! 0 |  503 | `				PH7_MemObjRelease(&aA[i].sRet);` |
|      ! 0 |  504 | `			}else if( aA[i].eKind == PH7_FA_RETHROW && aA[i].pExc ){` |
|      ! 0 |  505 | `				PH7_ClassInstanceUnref(aA[i].pExc);` |
|      ! 0 |  506 | `			}` |
|      ! 0 |  507 | `		}` |
|      ! 0 |  508 | `	}` |
|     1727 |  509 | `	SySetRelease(&pS->aFinallyAction);` |
|     1727 |  510 | `	VmExcReleaseAll(pVm, &pS->aException);` |
|     1727 |  511 | `	SySetRelease(&pS->aException);` |
|     1727 |  512 | `	SySetRelease(&pS->aSelf);  /* borrowed class pointers */` |
|     1727 |  513 | `	SyZero(pS, sizeof(*pS));` |
|     1727 |  514 | `}` |
|        - |  515 | `/*` |
|        - |  516 | ` * Swap sides: what the VM holds now goes to *pOut, what *pIn holds becomes` |
|        - |  517 | ` * live. Called only from the RESUMER's stack, on both sides of the switch, so` |
|        - |  518 | ` * the fiber never has to know it is being saved.` |
|        - |  519 | ` */` |
|     1756 |  520 | `static void VmCoroStateSwap(ph7_vm *pVm, VmCoroVmState *pOut, VmCoroVmState *pIn)` |
|        5 |  521 | `{` |
|        - |  522 | `	VmCoroVmState sLive;` |
|        - |  523 | `#define VM_CORO_SAVE(F)  sLive.F = pVm->F;` |
|     1761 |  524 | `	VM_CORO_STATE_FIELDS(VM_CORO_SAVE)` |
|        - |  525 | `#undef VM_CORO_SAVE` |
|        - |  526 | `#define VM_CORO_LOAD(F)  pVm->F = pIn->F;` |
|     1761 |  527 | `	VM_CORO_STATE_FIELDS(VM_CORO_LOAD)` |
|        - |  528 | `#undef VM_CORO_LOAD` |
|     1761 |  529 | `	*pOut = sLive;` |
|     1761 |  530 | `}` |
|        - |  531 | `/*` |
|        - |  532 | ` * The resumer's side of one switch into the fiber: install the fiber's view of` |
|        - |  533 | ` * the VM, go, and take the resumer's back when control returns. PH7_SUSPEND` |
|        - |  534 | ` * when the fiber suspended again, else whatever its body returned.` |
|        - |  535 | ` */` |
|      878 |  536 | `static sxi32 VmCoroRun(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        5 |  537 | `{` |
|      883 |  538 | `	VmCoroStateSwap(pVm, &pCtx->sHost, &pCtx->sSaved);` |
|      883 |  539 | `	VmCoroEnter(pCtx->pCoro);` |
|      883 |  540 | `	VmCoroStateSwap(pVm, &pCtx->sSaved, &pCtx->sHost);` |
|      883 |  541 | `	return pCtx->bCoroDone ? pCtx->iCoroRc : PH7_SUSPEND;` |
|        5 |  542 | `}` |
|        - |  543 | `/*` |
|        - |  544 | ` * The fiber's side, entered exactly once. Every later resume comes back inside` |
|        - |  545 | ` * whatever Fiber::suspend() the stack is parked on, not here.` |
|        - |  546 | ` */` |
|      432 |  547 | `static void VmCoroBody(ph7_exec_ctx *pCtx)` |
|        5 |  548 | `{` |
|      437 |  549 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - |  550 | `	sxi32 rc;` |
|        - |  551 | `	VmCoroAsanArrive(pCtx->pCoro, 0);` |
|      437 |  552 | `	if( pCtx->bTramp ){` |
|        - |  553 | `		/* A body with no bytecode of its own is called the way any internal function` |
|        - |  554 | `		 * calls a callback, from this stack: a generator function hands back its` |
|        - |  555 | `		 * Generator, a C function runs its loop here (so a callback it reaches can` |
|        - |  556 | `		 * suspend), and a name php routes through __call/__callStatic reaches the` |
|        - |  557 | `		 * handler. The dispatch pushes the callee's own frame above the transparent` |
|        - |  558 | ``		 * body frame, so a trace shows php's `[internal function]: f()` under`` |
|        - |  559 | ``		 * `Fiber->start()`. The map is start()'s own and still live: this is the`` |
|        - |  560 | `		 * first switch in, and start() is waiting on it. */` |
|       53 |  561 | `		ph7_value **apTramp = 0;` |
|        - |  562 | `		sxu32 k;` |
|       53 |  563 | `		if( pCtx->nTrampArg > 0 ){` |
|       54 |  564 | `			apTramp = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       34 |  565 | `				pCtx->nTrampArg * sizeof(ph7_value *));` |
|       17 |  566 | `		}` |
|       53 |  567 | `		if( pCtx->nTrampArg > 0 && apTramp == 0 ){` |
|      ! 0 |  568 | `			rc = PH7_ABORT;` |
|      ! 0 |  569 | `		}else{` |
|      101 |  570 | `			for( k = 0 ; k < pCtx->nTrampArg ; ++k ){` |
|       51 |  571 | `				apTramp[k] = &pCtx->aTrampArg[k];` |
|       27 |  572 | `			}` |
|       53 |  573 | `			pVm->bCallbackWeak = 1;` |
|       78 |  574 | `			rc = PH7_VmCallUserFunctionWithMap(pVm, &pCtx->sTramp, (int)pCtx->nTrampArg,` |
|       25 |  575 | `				apTramp, &pCtx->sRetValue, pCtx->pTrampMap);` |
|       53 |  576 | `			pVm->bCallbackWeak = 0;` |
|       53 |  577 | `			pCtx->pTrampMap = 0;` |
|       53 |  578 | `			if( apTramp ){` |
|       37 |  579 | `				SyMemBackendFree(&pVm->sAllocator, apTramp);` |
|       17 |  580 | `			}` |
|       53 |  581 | `			if( rc != PH7_ABORT && pCtx->pEscaped ){` |
|       15 |  582 | `				rc = PH7_EXCEPTION;` |
|       47 |  583 | `			}else if( rc != PH7_ABORT && rc != PH7_EXCEPTION ){` |
|       41 |  584 | `				rc = SXRET_OK;` |
|       19 |  585 | `			}` |
|        - |  586 | `		}` |
|       28 |  587 | `	}else` |
|      578 |  588 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|      191 |  589 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|      191 |  590 | `		VmCtxEnforceRetFunc(pCtx), FALSE, 0, &pCtx->pStack, &pCtx->nStackCap,` |
|      191 |  591 | `		pCtx->nStackOrig);` |
|      437 |  592 | `	pCtx->iCoroRc = rc;` |
|      437 |  593 | `	pCtx->bCoroDone = 1;` |
|      437 |  594 | `	VmCoroLeaveFinal(pCtx->pCoro);` |
|      216 |  595 | `}` |
|        - |  596 | `/*` |
|        - |  597 | ` * Suspend from inside the fiber. Returns PH7_OK with the resume value waiting` |
|        - |  598 | ` * in sSuspendValue, PH7_EXCEPTION when the fiber was resumed by Fiber::throw(),` |
|        - |  599 | ` * or PH7_ABORT when it was resumed only to be unwound (the kill switch).` |
|        - |  600 | ` */` |
|      446 |  601 | `static sxi32 VmCoroSuspend(ph7_context *pCallCtx, ph7_exec_ctx *pCtx)` |
|        5 |  602 | `{` |
|      451 |  603 | `	ph7_vm *pVm = pCtx->pVm;` |
|      451 |  604 | `	VmCoroLeave(pCtx->pCoro);` |
|        - |  605 | `	/* Resumed. */` |
|      451 |  606 | `	if( pCtx->bCoroKill ){` |
|        - |  607 | `		/* The Fiber object died while we were parked here. Unwinding is the whole` |
|        - |  608 | `		 * point of coming back: every C frame between here and the body entry gets` |
|        - |  609 | `		 * to run its own abort path and free what it owns, which is the only way a` |
|        - |  610 | `		 * builtin's half-built result (array_map's output array, an open handle)` |
|        - |  611 | `		 * is ever released -- nothing outside this stack can reach them. */` |
|      223 |  612 | `		return PH7_ABORT;` |
|        - |  613 | `	}` |
|      231 |  614 | `	if( pCtx->pInjected ){` |
|        - |  615 | `		/* Fiber::throw(): php raises AT the suspension point, so the throw happens` |
|        - |  616 | `		 * here rather than at a body-entry redirect (there is no body entry on this` |
|        - |  617 | `		 * path -- the resume lands inside this C call). Same shape as any builtin's` |
|        - |  618 | `		 * own throw: stamp the frame, raise, and report the status on the call` |
|        - |  619 | `		 * context so OP_CALL does not treat the call as a normal return. */` |
|       12 |  620 | `		ph7_class_instance *pInj = pCtx->pInjected;` |
|        - |  621 | `		VmFrame *pFrame;` |
|        - |  622 | `		sxi32 rc;` |
|       12 |  623 | `		pCtx->pInjected = 0;   /* one-shot */` |
|       12 |  624 | `		pFrame = pVm->pFrame;` |
|       12 |  625 | `		if( pFrame ){` |
|       12 |  626 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|       12 |  627 | `			pFrame->iFlags \|= VM_FRAME_THROW;` |
|        5 |  628 | `		}` |
|       12 |  629 | `		rc = VmThrowException(pVm, pInj);` |
|       12 |  630 | `		if( rc == SXERR_ABORT ){` |
|      ! 0 |  631 | `			pCallCtx->nThrowRc = PH7_ABORT;` |
|      ! 0 |  632 | `			return PH7_ABORT;` |
|        - |  633 | `		}` |
|       12 |  634 | `		pCallCtx->nThrowRc = PH7_EXCEPTION;` |
|       12 |  635 | `		return PH7_EXCEPTION;` |
|        - |  636 | `	}` |
|      221 |  637 | `	return PH7_OK;` |
|      228 |  638 | `}` |
|        - |  639 | `/*` |
|        - |  640 | ` * Re-raise, in the RESUMER's frame, the exception a fiber body let escape.` |
|        - |  641 | ` * Called at the three doors that run a body (start / resume / throw) whenever` |
|        - |  642 | ` * the run came back PH7_EXCEPTION with an instance parked on the ctx. Same` |
|        - |  643 | ` * shape as any builtin's own throw: stamp the frame, raise, and report the` |
|        - |  644 | ` * status on the call context so OP_CALL does not read the call as a normal` |
|        - |  645 | ` * return.` |
|        - |  646 | ` */` |
|       24 |  647 | `static sxi32 VmFiberRaiseEscaped(ph7_context *pCtx, ph7_exec_ctx *pExecCtx)` |
|        4 |  648 | `{` |
|       28 |  649 | `	ph7_vm *pVm = pCtx->pVm;` |
|       28 |  650 | `	ph7_class_instance *pExc = pExecCtx->pEscaped;` |
|        - |  651 | `	VmFrame *pFrame;` |
|        - |  652 | `	sxi32 rc;` |
|       28 |  653 | `	pExecCtx->pEscaped = 0;` |
|       28 |  654 | `	pFrame = pVm->pFrame;` |
|       28 |  655 | `	if( pFrame ){` |
|       28 |  656 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       28 |  657 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       12 |  658 | `	}` |
|       28 |  659 | `	rc = VmThrowException(pVm, pExc);` |
|       28 |  660 | `	PH7_ClassInstanceUnref(pExc);` |
|       28 |  661 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 |  662 | `		pCtx->nThrowRc = PH7_ABORT;` |
|      ! 0 |  663 | `		return PH7_ABORT;` |
|        - |  664 | `	}` |
|       28 |  665 | `	pCtx->nThrowRc = PH7_EXCEPTION;` |
|       28 |  666 | `	return PH7_EXCEPTION;` |
|       16 |  667 | `}` |
|        - |  668 | `#endif /* PH7_CORO_STACK */` |
|        - |  669 | `/*` |
|        - |  670 | ` * Allocate and initialize a new execution context for a fiber.` |
|        - |  671 | ` * The context is in CREATED state and ready to be started.` |
|        - |  672 | ` */` |
|     1292 |  673 | `PH7_PRIVATE ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|        5 |  674 | `{` |
|        - |  675 | `	ph7_exec_ctx *pCtx;` |
|        - |  676 | `	ph7_value *pStack;` |
|        - |  677 | `	VmFrame *pFrame;` |
|     1297 |  678 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|     1297 |  679 | `	if( pCtx == 0 ){` |
|      ! 0 |  680 | `		return 0;` |
|        - |  681 | `	}` |
|     1297 |  682 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|     1297 |  683 | `	pCtx->pVm = pVm;` |
|     1297 |  684 | `	pCtx->pFunc = pFunc;` |
|        - |  685 | `	/* A coroutine outlives the call that made it and reads pFunc for the whole of` |
|        - |  686 | `	 * its life -- including before its body frame exists, which VmStartCtx creates` |
|        - |  687 | `	 * LAZILY. For a run-time closure that is a hold of its own on the` |
|        - |  688 | ``	 * per-instantiation copy: `(function(){ yield 1; })()` drops the Closure object`` |
|        - |  689 | `	 * at the call, and without this the body was freed under the Generator that` |
|        - |  690 | `	 * still names it. */` |
|     1297 |  691 | `	PH7_VmClosureFuncRef(pFunc);` |
|     1297 |  692 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|     1297 |  693 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|     1297 |  694 | `	pCtx->pc = 0;` |
|     1297 |  695 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|     1297 |  696 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|     1297 |  697 | `	PH7_MemObjInit(pVm, &pCtx->sDelegate);` |
|        - |  698 | `	/* Container for this body's own exception handlers while suspended (borrowed` |
|        - |  699 | `	 * ph7_exception* pointers — never freed here, owned by the compiled func). */` |
|     1297 |  700 | `	SySetInit(&pCtx->aSavedException, &pVm->sAllocator, sizeof(ph7_exception *));` |
|        - |  701 | `	/* ROOT C: this body's own pending finally actions while suspended. */` |
|     1297 |  702 | `	SySetInit(&pCtx->aSavedFinally, &pVm->sAllocator, sizeof(VmFinallyAction));` |
|     1297 |  703 | `	pCtx->nFinallyBase = 0;` |
|        - |  704 | `	/* Stage 4: this coroutine's own aSelf entries (self::/static:: class context` |
|        - |  705 | `	 * pushed by nested method calls still open at suspend) parked while suspended,` |
|        - |  706 | `	 * so they don't pollute the resumer's aSelf. Borrowed ph7_class* pointers. */` |
|     1297 |  707 | `	SySetInit(&pCtx->aSavedSelf, &pVm->sAllocator, sizeof(ph7_class *));` |
|     1297 |  708 | `	pCtx->nSelfBase = 0;` |
|        - |  709 | ``	/* The class this body's `static::` means, taken from the CALL that is creating it`` |
|        - |  710 | `	 * (VmStartCtx pushes it back for the body's duration). */` |
|     1297 |  711 | `	pCtx->pLsbClass = PH7_VmPeekTopClass(pVm);` |
|        - |  712 | `	/* Caller slots this body's by-reference parameters alias (see the struct). */` |
|     1297 |  713 | `	SySetInit(&pCtx->aByRefArg, &pVm->sAllocator, sizeof(sxu32));` |
|     1297 |  714 | `	pCtx->pParkedSegment = 0;` |
|     1297 |  715 | `	pCtx->nBodyExecDepth = 0;` |
|        - |  716 | `	/* Allocate a private operand stack */` |
|     1297 |  717 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|     1297 |  718 | `	if( pStack == 0 ){` |
|      ! 0 |  719 | `		PH7_VmClosureFuncUnref(pVm, pFunc);` |
|      ! 0 |  720 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  721 | `		return 0;` |
|        - |  722 | `	}` |
|     1297 |  723 | `	pCtx->pStack = pStack;` |
|     1297 |  724 | `	pCtx->nStackCap = SySetUsed(&pFunc->aByteCode) + VM_STACK_GUARD; /* grows with pStack on OP_SPREAD */` |
|     1297 |  725 | `	pCtx->nStackOrig = pCtx->nStackCap; /* fixed original — headroom reference across resumes */` |
|        - |  726 | `	/* Create a detached frame for the fiber */` |
|     1297 |  727 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|     1297 |  728 | `	if( pFrame == 0 ){` |
|      ! 0 |  729 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|      ! 0 |  730 | `		PH7_VmClosureFuncUnref(pVm, pFunc);` |
|      ! 0 |  731 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      ! 0 |  732 | `		return 0;` |
|        - |  733 | `	}` |
|        - |  734 | `	/* The frame's OWN hold, and not the one taken above. VmNewFrame stamps pFunc as` |
|        - |  735 | `	 * the frame's pUserData exactly as VmEnterFrame does, and this body frame's only` |
|        - |  736 | `	 * teardown -- VmFreeDetachedFrame, from VmCloseCtx -- gives that hold back the way` |
|        - |  737 | `	 * VmLeaveFrame does for a live frame. Without it one coroutine took one hold and` |
|        - |  738 | `	 * gave back two, so the first Generator or Fiber built from a run-time closure` |
|        - |  739 | `	 * freed the per-instantiation copy its own Closure object was still naming, and` |
|        - |  740 | `	 * calling that closure a second time said "Call to undefined function` |
|        - |  741 | `	 * [closure_N]()". */` |
|     1297 |  742 | `	PH7_VmClosureFuncRef(pFunc);` |
|     1297 |  743 | `	pCtx->pFrame = pFrame;` |
|     1297 |  744 | `	return pCtx;` |
|      651 |  745 | `}` |
|        - |  746 | `/*` |
|        - |  747 | ` * A suspended coroutine must not leave its own slices of the VM's shared stacks` |
|        - |  748 | ` * sitting above the caller's depth. Three stacks are affected, identically:` |
|        - |  749 | ` *   - pVm->aException: its exception handlers — else a generator/fiber suspended` |
|        - |  750 | ` *     inside a try leaves handlers referencing its now-detached frame on the` |
|        - |  751 | ` *     global stack, corrupting the caller's try/catch.` |
|        - |  752 | ` *   - pVm->aFinallyAction (ROOT C): its pending finally actions — else a yield` |
|        - |  753 | ` *     inside a finally (reached by return/break/rethrow) leaves a record where an` |
|        - |  754 | ` *     out-of-order-resumed sibling generator's OP_END_FINALLY would mis-pop it.` |
|        - |  755 | ` *   - pVm->aSelf (stage 4): its self::/static:: entries pushed by still-open` |
|        - |  756 | ` *     nested method calls — else they sit on the resumer's aSelf and corrupt its` |
|        - |  757 | ` *     self:: resolution.` |
|        - |  758 | ` * Each is the same operation: on suspend move the slice above a captured base` |
|        - |  759 | ` * into a per-ctx park buffer; on resume re-publish it at the (refreshed) caller` |
|        - |  760 | ` * depth. VmParkStackSlice / VmRestoreStackSlice factor it for any element type` |
|        - |  761 | ` * (size taken from the SySet); VmParkCtxState / VmRestoreCtxState drive all three.` |
|        - |  762 | ` *` |
|        - |  763 | ` * Stage 4: the whole suspended segment stays alive, so a parked handler's owner` |
|        - |  764 | ` * frame is never freed underneath it — the parked pointer stays valid and is kept` |
|        - |  765 | ` * (the old stage-2b lossy-path invalidation is gone with the discard). A` |
|        - |  766 | ` * body-level suspend only ever has body-owned handlers here, and its finally/self` |
|        - |  767 | ` * slices are empty (all nested calls already returned) — so those are no-ops.` |
|        - |  768 | ` */` |
|     5316 |  769 | `static void VmParkStackSlice(SySet *pFrom, SySet *pSaved, sxu32 nBase)` |
|        5 |  770 | `{` |
|     5321 |  771 | `	sxu32 nUsed = SySetUsed(pFrom);` |
|     5321 |  772 | `	if( nUsed > nBase ){` |
|      289 |  773 | `		const char *aBase = (const char *)SySetBasePtr(pFrom);` |
|        - |  774 | `		sxu32 i;` |
|      579 |  775 | `		for( i = nBase; i < nUsed; i++ ){` |
|      295 |  776 | `			SySetPut(pSaved, (const void *)(aBase + i * pFrom->eSize));` |
|      150 |  777 | `		}` |
|      289 |  778 | `		SySetTruncate(pFrom, nBase);` |
|      142 |  779 | `	}` |
|     5321 |  780 | `}` |
|     5310 |  781 | `static void VmRestoreStackSlice(SySet *pTo, SySet *pSaved)` |
|        5 |  782 | `{` |
|     5315 |  783 | `	sxu32 i, n = SySetUsed(pSaved);` |
|     5315 |  784 | `	if( n > 0 ){` |
|      289 |  785 | `		const char *aSaved = (const char *)SySetBasePtr(pSaved);` |
|      579 |  786 | `		for( i = 0; i < n; i++ ){` |
|      295 |  787 | `			SySetPut(pTo, (const void *)(aSaved + i * pSaved->eSize));` |
|      150 |  788 | `		}` |
|      289 |  789 | `		SySetReset(pSaved);` |
|      142 |  790 | `	}` |
|     5315 |  791 | `}` |
|     1772 |  792 | `static void VmParkCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        5 |  793 | `{` |
|     1777 |  794 | `	VmParkStackSlice(&pVm->aException, &pCtx->aSavedException, pCtx->nExceptionBase);` |
|     1777 |  795 | `	VmParkStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally, pCtx->nFinallyBase);` |
|     1777 |  796 | `	VmParkStackSlice(&pVm->aSelf, &pCtx->aSavedSelf, pCtx->nSelfBase);` |
|     1777 |  797 | `}` |
|     1770 |  798 | `static void VmRestoreCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        5 |  799 | `{` |
|     1775 |  800 | `	VmRestoreStackSlice(&pVm->aException, &pCtx->aSavedException);` |
|     1775 |  801 | `	VmRestoreStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally);` |
|     1775 |  802 | `	VmRestoreStackSlice(&pVm->aSelf, &pCtx->aSavedSelf);` |
|     1775 |  803 | `}` |
|        - |  804 | `/*` |
|        - |  805 | ` * On suspend, free the exception (try) frames the yield was nested in. They were` |
|        - |  806 | ` * pushed by OP_LOAD_EXCEPTION between the coroutine body frame (pCtx->pFrame) and` |
|        - |  807 | ` * the current suspend-point top frame. The generator/fiber frame model saves only` |
|        - |  808 | ` * the body frame, so these transparent wrappers would otherwise be orphaned and` |
|        - |  809 | ` * leak on every yield-that-sits-inside-a-try (unbounded for a generator looping` |
|        - |  810 | ` * with a yield in a try). Freeing them loses nothing the resume needs: this body's` |
|        - |  811 | ` * exception HANDLERS are parked separately (VmParkCtxState) and each` |
|        - |  812 | ` * try's landing pad lives on its ph7_exception (iLandingPc), while OP_POP_EXCEPTION` |
|        - |  813 | ` * on resume skips the (now absent) frame pop via its VM_FRAME_EXCEPTION guard and` |
|        - |  814 | ` * OP_LOAD_EXCEPTION re-creates a fresh wrapper when the try is next entered. Must` |
|        - |  815 | ` * run while pVm->pFrame still points at the suspend-time top (before the detach).` |
|        - |  816 | ` */` |
|     2928 |  817 | `static void VmFreeSuspendedExceptionFrames(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        5 |  818 | `{` |
|     3153 |  819 | `	while( pVm->pFrame != pCtx->pFrame && (pVm->pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|      225 |  820 | `		VmLeaveFrame(&(*pVm));` |
|        5 |  821 | `	}` |
|     2933 |  822 | `}` |
|        - |  823 | `/*` |
|        - |  824 | ` * Stamp a coroutine body frame with the site that is starting or RESUMING it.` |
|        - |  825 | ` *` |
|        - |  826 | ` * An ordinary frame gets this in VmEnterFrame; a coroutine's body frame is built` |
|        - |  827 | ` * detached (VmNewExecCtx -> VmNewFrame) and never went through it, so a backtrace` |
|        - |  828 | ` * taken inside a generator reported the frame below it at line 0 -- printed as` |
|        - |  829 | ` * line 1, in whatever file the include stack happened to top out at. php answers` |
|        - |  830 | ``  * the CURRENT resume site rather than the creation site (`foreach (g() as $v)` `` |
|        - |  831 | `` * for the first step, the `yield from` line for a delegate), which is exactly`` |
|        - |  832 | ` * what this reads, so it is stamped on every start and resume rather than once.` |
|        - |  833 | ` * Must run BEFORE the frame is spliced onto the chain: the site is the resumer's.` |
|        - |  834 | ` * A generator's creating call stamps it too (vm_exec.c): its arguments are bound` |
|        - |  835 | ` * on this frame there, and a refusal's trace names that call's line.` |
|        - |  836 | ` */` |
|     4606 |  837 | `PH7_PRIVATE void VmStampCoroutineCallSite(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        5 |  838 | `{` |
|        - |  839 | `	SyString *pFile;` |
|     4611 |  840 | `	if( pCtx->pFrame == 0 ){` |
|      ! 0 |  841 | `		return;` |
|        - |  842 | `	}` |
|     4611 |  843 | `	pCtx->pFrame->nCallLine = pVm->nCurLine;` |
|     4611 |  844 | `	pFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|     4611 |  845 | `	if( pFile ){` |
|     4611 |  846 | `		pCtx->pFrame->sCallFile = *pFile;` |
|     2303 |  847 | `	}` |
|     2308 |  848 | `}` |
|        - |  849 | `/*` |
|        - |  850 | ` * Record which Fiber method is entering a fiber's body (VM_FRAME_FIBER). php runs the` |
|        - |  851 | ` * body as that method's callback, so a trace taken inside it shows the body with no` |
|        - |  852 | `` * file or line and `Fiber->start()`, `Fiber->resume()` or `Fiber->throw()` as a frame`` |
|        - |  853 | ` * of its own at the resumer's site -- the method that entered it LAST, so this is` |
|        - |  854 | ` * re-stamped on every entry. The site itself is VmStampCoroutineCallSite's.` |
|        - |  855 | ` */` |
|      700 |  856 | `static void VmFiberStampEntry(ph7_exec_ctx *pCtx, SyString *pMethod, ph7_class_instance *pFiber)` |
|        5 |  857 | `{` |
|      705 |  858 | `	pCtx->pFrame->iFlags \|= VM_FRAME_FIBER;` |
|      705 |  859 | `	pCtx->pFrame->pNativeCaller = pMethod;` |
|      705 |  860 | `	pCtx->pFrame->pNativeCallerThis = pFiber;` |
|      705 |  861 | `	pCtx->pFrame->pNativeCallerRec = pCtx->pVm->pNativeCall;` |
|      705 |  862 | `}` |
|        - |  863 | `/*` |
|        - |  864 | ` * Record whether a GENERATOR's body is being entered by an internal function. php` |
|        - |  865 | ``  * links the body under whatever frame resumes it, so `$g->current()`, `$g->send()` `` |
|        - |  866 | `` * or `iterator_to_array($g)` put an internal frame there: a trace taken inside the`` |
|        - |  867 | `` * body shows it with no file or line (`[internal function]: gen()`) and that method`` |
|        - |  868 | `` * or function as a frame of its own at the resumer's site, while a `foreach` or a`` |
|        - |  869 | `` * `yield from` resumes it from bytecode and keeps the ordinary site. The resumer is`` |
|        - |  870 | ` * exactly the newest running internal call made from the current frame (the chain` |
|        - |  871 | ` * VmBuildBacktrace walks names it), so the mark is re-derived on every entry. Its` |
|        - |  872 | ` * other meaning -- how the arguments were bound -- is spent by then: a generator` |
|        - |  873 | ` * binds them at its creating call, never at a start or resume.` |
|        - |  874 | ` */` |
|     2496 |  875 | `static void VmGeneratorStampEntry(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        5 |  876 | `{` |
|     2501 |  877 | `	VmNativeCall *pNat = pVm->pNativeCall;` |
|     4459 |  878 | `	while( pNat && pNat->bElided ){` |
|     1963 |  879 | `		pNat = pNat->pPrev;` |
|        5 |  880 | `	}` |
|     2501 |  881 | `	if( pNat && pNat->pFrame == (void *)pVm->pFrame ){` |
|      729 |  882 | `		pCtx->pFrame->iFlags \|= VM_FRAME_NATIVE_CALLER;` |
|      729 |  883 | `		pCtx->pFrame->pNativeCaller = pNat->pName;` |
|      729 |  884 | `		pCtx->pFrame->pNativeCallerThis = 0;` |
|      367 |  885 | `	}else{` |
|     1777 |  886 | `		pCtx->pFrame->iFlags &= ~VM_FRAME_NATIVE_CALLER;` |
|     1777 |  887 | `		pCtx->pFrame->pNativeCaller = 0;` |
|     1777 |  888 | `		pCtx->pFrame->pNativeCallerThis = 0;` |
|        - |  889 | `	}` |
|     2501 |  890 | `}` |
|        - |  891 | `static SyString sFiberStartName = { "start", sizeof("start")-1 };` |
|        - |  892 | `static SyString sFiberResumeName = { "resume", sizeof("resume")-1 };` |
|        - |  893 | `static SyString sFiberThrowName = { "throw", sizeof("throw")-1 };` |
|        - |  894 | `/*` |
|        - |  895 | ` * Common suspend epilogue for VmStartCtx / VmResumeCtx: detach the suspended` |
|        - |  896 | ` * coroutine from the live VM chain and park its exception handlers. Two forms:` |
|        - |  897 | ` *   - Body-level (pParkedSegment == 0): a generator yield or a fiber suspending` |
|        - |  898 | ` *     directly in its body. The try wrappers the yield sat in are transient —` |
|        - |  899 | ` *     free them (OP_LOAD_EXCEPTION recreates them on re-entry) — and detach the` |
|        - |  900 | ` *     body frame alone.` |
|        - |  901 | ` *   - Deep fiber suspend (pParkedSegment != 0, stage 4): the whole segment (body` |
|        - |  902 | ` *     frame + the nested call/try frames above it) stays alive and is detached` |
|        - |  903 | ` *     as a unit; nothing is freed, so resume can continue inside the innermost` |
|        - |  904 | ` *     callee. Its handlers are parked the same way and rebased on resume.` |
|        - |  905 | ` */` |
|     2218 |  906 | `static void VmSuspendCtxDetach(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        5 |  907 | `{` |
|        - |  908 | `#ifdef PH7_CORO_STACK` |
|     2223 |  909 | `	if( pCtx->pCoro ){` |
|        - |  910 | `		/* Third form: the fiber has a stack of its own and is still standing on` |
|        - |  911 | `		 * it. Its frames -- body, nested callees, open-try wrappers alike -- stay` |
|        - |  912 | `		 * exactly as they are, because that C stack still points into them; all` |
|        - |  913 | `		 * this has to remember is which one was current, so the resume can make it` |
|        - |  914 | `		 * current again. Nothing is parked and no depth is deducted: the three` |
|        - |  915 | `		 * stacks and both counters travelled with the VM-state swap the moment the` |
|        - |  916 | `		 * switch happened. */` |
|      451 |  917 | `		pCtx->pCoroTop = pVm->pFrame;` |
|      451 |  918 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|      451 |  919 | `		pCtx->pFrame->pParent = 0;` |
|      451 |  920 | `		if( pResult ){` |
|      451 |  921 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|      223 |  922 | `		}` |
|      451 |  923 | `		return;` |
|        - |  924 | `	}` |
|        - |  925 | `#endif` |
|     1777 |  926 | `	if( pCtx->pParkedSegment == 0 ){` |
|     1777 |  927 | `		VmFreeSuspendedExceptionFrames(pVm, pCtx);` |
|      891 |  928 | `	}else{` |
|        - |  929 | `		/* The parked records' push-time accounting (one nRecursionDepth++ each,` |
|        - |  930 | `		 * plus VmCallFinish's aSelf pop, which never ran) leaves the segment` |
|        - |  931 | `		 * counted as active while the fiber is suspended — deactivate it. aSelf` |
|        - |  932 | `		 * is parked wholesale below (base-relative), so drop only the depth. */` |
|      ! 0 |  933 | `		pVm->nRecursionDepth -= ((VmParkedSegment *)pCtx->pParkedSegment)->nRecords;` |
|        - |  934 | `	}` |
|     1777 |  935 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|     1777 |  936 | `	pCtx->pFrame->pParent = 0;` |
|     1777 |  937 | `	VmParkCtxState(pVm, pCtx);` |
|     1777 |  938 | `	if( pResult ){` |
|      ! 0 |  939 | `		PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|      ! 0 |  940 | `	}` |
|     1114 |  941 | `}` |
|        - |  942 | `/*` |
|        - |  943 | ` * The return-type enforcement target for a coroutine body run. A GENERATOR` |
|        - |  944 | ` * function's declared return type belongs to the call site (always a Generator` |
|        - |  945 | ` * object, validated at compile time as "a supertype of Generator"); the body's` |
|        - |  946 | ` * own return value feeds getReturn() and is never type-checked. Gate on the` |
|        - |  947 | ` * VM_FUNC_GENERATOR flag (the semantic property), not pPrivate (a wrapper-linkage` |
|        - |  948 | ` * fact): a Fiber given a generator-flagged callable runs with pPrivate == 0 and` |
|        - |  949 | ` * must not enforce either. Ordinary fiber callables keep their declared` |
|        - |  950 | ` * return-type enforcement (php enforces it).` |
|        - |  951 | ` */` |
|     2878 |  952 | `static ph7_vm_func * VmCtxEnforceRetFunc(ph7_exec_ctx *pCtx)` |
|        5 |  953 | `{` |
|     1635 |  954 | `	return ((pCtx->pFunc->iFlags & VM_FUNC_GENERATOR) == 0 && VmFuncHasReturnType(pCtx->pFunc))` |
|     1630 |  955 | `		? pCtx->pFunc : 0;` |
|        5 |  956 | `}` |
|        - |  957 | `/*` |
|        - |  958 | ` * Common epilogue for VmStartCtx / VmResumeCtx once the body run returns rc:` |
|        - |  959 | ` * restore the previous active context, then park on suspend or detach the` |
|        - |  960 | ` * coroutine frame and record the terminal state. On normal completion the value` |
|        - |  961 | ` * belongs to getReturn() only — start()/resume() return the NEXT suspend value,` |
|        - |  962 | ` * which is null at completion (php parity), so pResult is left at its` |
|        - |  963 | ` * caller-initialized null.` |
|        - |  964 | ` */` |
|     3374 |  965 | `static sxi32 VmFinishCtxRun(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_exec_ctx *pOldCtx,` |
|        - |  966 | `	sxi32 rc, ph7_value *pResult)` |
|        5 |  967 | `{` |
|     3379 |  968 | `	pVm->pActiveCtx = pOldCtx;` |
|        - |  969 | `#ifdef PH7_CORO_STACK` |
|     3379 |  970 | `	if( pCtx->pCoro ){` |
|      883 |  971 | `		if( rc == PH7_SUSPEND ){` |
|        - |  972 | `			/* No saved pc or top-of-stack to record: the switch IS the state, and` |
|        - |  973 | `			 * VmSuspendCtx (which marks the ctx on the other path) never ran. */` |
|      451 |  974 | `			pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|      451 |  975 | `			VmSuspendCtxDetach(pVm, pCtx, pResult);` |
|      451 |  976 | `			return SXRET_OK;` |
|        - |  977 | `		}` |
|        - |  978 | `		/* The body is over, so the stack is spent: free it now rather than at the` |
|        - |  979 | `		 * Fiber object's death (a completed fiber has no use for megabytes of` |
|        - |  980 | `		 * mapping), and with it the three stacks that were only ever this body's.` |
|        - |  981 | `		 * The frame chain it left is the body frame plus any try wrappers an` |
|        - |  982 | `		 * escaping exception never closed -- the same two steps the shared tail` |
|        - |  983 | `		 * below takes, which the ctx-owned bases in it do not apply to here. */` |
|      437 |  984 | `		VmFreeSuspendedExceptionFrames(pVm, pCtx);` |
|      437 |  985 | `		if( pVm->pFrame == pCtx->pFrame ){` |
|      437 |  986 | `			pVm->pFrame = pCtx->pFrame->pParent;` |
|      437 |  987 | `			pCtx->pFrame->pParent = 0;` |
|      216 |  988 | `		}` |
|      437 |  989 | `		VmCoroFree(pVm, pCtx->pCoro);` |
|      437 |  990 | `		pCtx->pCoro = 0;` |
|      437 |  991 | `		VmCoroStateRelease(pVm, &pCtx->sSaved);` |
|      437 |  992 | `		if( rc == PH7_ABORT ){` |
|      223 |  993 | `			pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      223 |  994 | `			return PH7_ABORT;` |
|        - |  995 | `		}` |
|      217 |  996 | `		if( rc == PH7_EXCEPTION ){` |
|       28 |  997 | `			pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|       28 |  998 | `			pCtx->bThrew = 1;` |
|       28 |  999 | `			return PH7_EXCEPTION;` |
|        - | 1000 | `		}` |
|      193 | 1001 | `		pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|      193 | 1002 | `		return SXRET_OK;` |
|        - | 1003 | `	}` |
|        - | 1004 | `#endif` |
|     2501 | 1005 | `	if( rc == PH7_SUSPEND ){` |
|        - | 1006 | `		/* Handles a deep RE-suspend too (a resumed segment parking a fresh one):` |
|        - | 1007 | `		 * VmSuspendCtxDetach deactivates the new segment's recursion accounting,` |
|        - | 1008 | `		 * parks its aSelf, and skips VmFreeSuspendedExceptionFrames for a segment` |
|        - | 1009 | `		 * so it can't free the still-live parked try wrappers. */` |
|     1777 | 1010 | `		VmSuspendCtxDetach(pVm, pCtx, pResult);` |
|     1777 | 1011 | `		return SXRET_OK;` |
|        - | 1012 | `	}` |
|        - | 1013 | ``	/* A finally entered via the throw redirect whose `return` short-circuited`` |
|        - | 1014 | `	 * OP_END_FINALLY leaves the try's transparent wrapper ABOVE the body frame —` |
|        - | 1015 | `	 * the detach below would then be skipped and the wrapper (plus the body` |
|        - | 1016 | `	 * frame) leak into the RESUMER's frame chain, so the next try at that scope` |
|        - | 1017 | `	 * records the wrong owner frame and its caught throw silently unwinds the` |
|        - | 1018 | `	 * script. Free trailing exception wrappers exactly like the suspend path. */` |
|      729 | 1019 | `	if( pCtx->pParkedSegment == 0 ){` |
|      729 | 1020 | `		VmFreeSuspendedExceptionFrames(pVm, pCtx);` |
|      362 | 1021 | `	}` |
|        - | 1022 | `	/* Detach the coroutine frame from the live chain, unless a deeper unwind` |
|        - | 1023 | `	 * already moved pVm->pFrame off it. */` |
|      729 | 1024 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|      729 | 1025 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|      729 | 1026 | `		pCtx->pFrame->pParent = 0;` |
|      362 | 1027 | `	}` |
|        - | 1028 | `	/* The body is over (it did not suspend): drop whatever it left on the shared` |
|        - | 1029 | `	 * self stack, which is at least the LSB class VmStartCtx published for it. */` |
|      729 | 1030 | `	if( SySetUsed(&pVm->aSelf) > pCtx->nSelfBase ){` |
|       55 | 1031 | `		SySetTruncate(&pVm->aSelf, pCtx->nSelfBase);` |
|       25 | 1032 | `	}` |
|      729 | 1033 | `	if( rc == PH7_ABORT ){` |
|        3 | 1034 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        3 | 1035 | `		return PH7_ABORT;` |
|        - | 1036 | `	}` |
|      727 | 1037 | `	if( rc == PH7_EXCEPTION ){` |
|       73 | 1038 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|       73 | 1039 | `		pCtx->bThrew = 1;` |
|       73 | 1040 | `		return PH7_EXCEPTION;` |
|        - | 1041 | `	}` |
|      659 | 1042 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|      659 | 1043 | `	return SXRET_OK;` |
|     1692 | 1044 | `}` |
|        - | 1045 | `/*` |
|        - | 1046 | ` * Start executing a fiber context for the first time.` |
|        - | 1047 | ` */` |
|     1158 | 1048 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|        5 | 1049 | `{` |
|        - | 1050 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 1051 | `	sxi32 rc;` |
|     1163 | 1052 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 1053 | `		return SXERR_INVALID;` |
|        - | 1054 | `	}` |
|        - | 1055 | `	/* A fiber/generator start is a native VmByteCodeExec re-entry, bounded by` |
|        - | 1056 | `	 * nMaxNativeDepth. Reject HERE, before attaching the frame / mutating VM` |
|        - | 1057 | `	 * state, so the abort is clean — the wrapper's own check fires only after` |
|        - | 1058 | `	 * this function has spliced the coroutine into the frame chain, which its` |
|        - | 1059 | `	 * post-exec detach cannot fully unwind. The PHP call-depth cap belongs to` |
|        - | 1060 | `	 * OP_CALL only. */` |
|     1163 | 1061 | `	if( VmNativeNestingExceeded(pVm) ){` |
|      ! 0 | 1062 | `		return VmNativeNestingFatal(pVm);` |
|        - | 1063 | `	}` |
|        - | 1064 | `#ifdef PH7_CORO_STACK` |
|        - | 1065 | `	/* A FIBER gets a native stack of its own (pPrivate == 0 is what tells one from` |
|        - | 1066 | ``	 * a generator, which never needs one: `yield` is lexically inside the body, so`` |
|        - | 1067 | `	 * it can never have a C frame above it to park). Taken BEFORE anything below` |
|        - | 1068 | `	 * mutates the VM, because the fiber's opening view of it is the resumer's.` |
|        - | 1069 | `	 * A stack this build cannot give it leaves pCoro at 0 and the body runs inline` |
|        - | 1070 | `	 * exactly as it did before. */` |
|     1163 | 1071 | `	if( pCtx->pPrivate == 0 ){` |
|      437 | 1072 | `		pCtx->pCoro = VmCoroNew(pVm, pCtx);` |
|      437 | 1073 | `		if( pCtx->pCoro ){` |
|      437 | 1074 | `			VmCoroStateInit(pVm, pCtx);` |
|      216 | 1075 | `		}` |
|      216 | 1076 | `	}` |
|        - | 1077 | `#endif` |
|        - | 1078 | `	/* Attach the fiber's frame to the VM frame chain */` |
|     1163 | 1079 | `	VmStampCoroutineCallSite(pVm, pCtx);` |
|     1163 | 1080 | `	if( pCtx->pPrivate ){` |
|      731 | 1081 | `		VmGeneratorStampEntry(pVm, pCtx);` |
|      363 | 1082 | `	}` |
|     1163 | 1083 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|     1163 | 1084 | `	pVm->pFrame = pCtx->pFrame;` |
|        - | 1085 | `	/* Save and set the active context */` |
|     1163 | 1086 | `	pOldCtx = pVm->pActiveCtx;` |
|     1163 | 1087 | `	pVm->pActiveCtx = pCtx;` |
|     1163 | 1088 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|        - | 1089 | `#ifdef PH7_CORO_STACK` |
|     1163 | 1090 | `	if( pCtx->pCoro ){` |
|        - | 1091 | `		/* Its three stacks are its own and start empty, so every floor an` |
|        - | 1092 | `		 * activation of this body records is measured from zero and stays true` |
|        - | 1093 | `		 * however deep the resumer happens to be next time. */` |
|      437 | 1094 | `		pCtx->nExceptionBase = 0;` |
|      437 | 1095 | `		pCtx->nFinallyBase = 0;` |
|      437 | 1096 | `		pCtx->nSelfBase = 0;` |
|      437 | 1097 | `		if( pCtx->pLsbClass ){` |
|       15 | 1098 | `			SySetPut(&pCtx->sSaved.aSelf, (const void *)&pCtx->pLsbClass);` |
|        6 | 1099 | `		}` |
|      221 | 1100 | `	}else` |
|        - | 1101 | `#endif` |
|        - | 1102 | `	{` |
|      731 | 1103 | `		pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|      731 | 1104 | `		pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);` |
|      731 | 1105 | `		pCtx->nSelfBase = SySetUsed(&pVm->aSelf);` |
|        - | 1106 | `		/* Re-publish the creating call's late-static-binding class ABOVE that base, so` |
|        - | 1107 | ``		 * the body's `static::` resolves to what php resolves it to. It rides the`` |
|        - | 1108 | `		 * ordinary park/restore of this coroutine's own aSelf slice, so a suspend takes` |
|        - | 1109 | `		 * it off the shared stack and a resume puts it back; VmFinishCtxRun truncates it` |
|        - | 1110 | `		 * away when the body ends for good. */` |
|      731 | 1111 | `		if( pCtx->pLsbClass ){` |
|       55 | 1112 | `			SySetPut(&pVm->aSelf,(const void *)&pCtx->pLsbClass);` |
|       25 | 1113 | `		}` |
|        - | 1114 | `	}` |
|        - | 1115 | `	/* Native depth the body runs at (the VmByteCodeExec wrapper bumps +1): a` |
|        - | 1116 | `	 * Fiber::suspend() at a deeper depth is inside a C->PHP callback and gets a` |
|        - | 1117 | `	 * FiberError instead of parking across the native frame (stage 4). */` |
|     1163 | 1118 | `	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1;` |
|        - | 1119 | `#ifdef PH7_CORO_STACK` |
|     1163 | 1120 | `	if( pCtx->pCoro ){` |
|        - | 1121 | `		/* On its own stack the body starts at native depth zero, so it never reads` |
|        - | 1122 | `		 * nBodyExecDepth again -- the suspend that used to consult it now simply` |
|        - | 1123 | `		 * switches, from wherever it is. */` |
|      437 | 1124 | `		rc = VmCoroRun(pVm, pCtx);` |
|      437 | 1125 | `		return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|        - | 1126 | `	}` |
|        - | 1127 | `#endif` |
|        - | 1128 | `	/* Execute from the beginning (no parked segment). An OP_SPREAD in the body may` |
|        - | 1129 | `	 * realloc pCtx->pStack — pass &pCtx->pStack / &pCtx->nStackCap so the grown` |
|        - | 1130 | `	 * buffer + capacity persist for the next resume and for ctx teardown. */` |
|     1094 | 1131 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|      363 | 1132 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|      363 | 1133 | `		VmCtxEnforceRetFunc(pCtx), FALSE, 0, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);` |
|      731 | 1134 | `	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|      584 | 1135 | `}` |
|        - | 1136 | `/*` |
|        - | 1137 | ` * Resume a suspended fiber context.` |
|        - | 1138 | ` */` |
|     2216 | 1139 | `PH7_PRIVATE sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|        5 | 1140 | `{` |
|        - | 1141 | `	ph7_exec_ctx *pOldCtx;` |
|        - | 1142 | `	VmParkedSegment *pSeg;` |
|        - | 1143 | `	sxi32 rc;` |
|     2221 | 1144 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 | 1145 | `		return SXERR_INVALID;` |
|        - | 1146 | `	}` |
|        - | 1147 | `#ifdef PH7_CORO_STACK` |
|     2221 | 1148 | `	if( pCtx->pCoro ){` |
|        - | 1149 | `		/* A fiber on its own stack: nothing to re-push, rebase or adopt. The whole` |
|        - | 1150 | `		 * suspended activation chain is still standing on that stack, so a resume` |
|        - | 1151 | `		 * is the frame re-attach plus one switch.` |
|        - | 1152 | `		 *` |
|        - | 1153 | `		 * The nesting guard above is skipped on purpose: this re-entry adds a` |
|        - | 1154 | `		 * single frame to the RESUMER's stack and then leaves it for the fiber's,` |
|        - | 1155 | `		 * which carries its own depth count -- and a teardown unwind (bCoroKill)` |
|        - | 1156 | `		 * must go through even when the resumer is already at the cap, or the C` |
|        - | 1157 | `		 * frames it is there to unwind are freed underneath instead.` |
|        - | 1158 | `		 *` |
|        - | 1159 | `		 * The resume value reaches the parked Fiber::suspend() through the ctx's` |
|        - | 1160 | `		 * own slot rather than an operand stack: on this path the suspend is a C` |
|        - | 1161 | `		 * call about to RETURN a value, not a saved pc with a hole above its top. */` |
|      451 | 1162 | `		if( pResumeValue ){` |
|      145 | 1163 | `			PH7_MemObjStore(pResumeValue, &pCtx->sSuspendValue);` |
|       75 | 1164 | `		}else{` |
|      311 | 1165 | `			PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|        - | 1166 | `		}` |
|      451 | 1167 | `		VmStampCoroutineCallSite(pVm, pCtx);` |
|      451 | 1168 | `		pCtx->pFrame->pParent = pVm->pFrame;` |
|      451 | 1169 | `		pVm->pFrame = pCtx->pCoroTop;` |
|      451 | 1170 | `		pOldCtx = pVm->pActiveCtx;` |
|      451 | 1171 | `		pVm->pActiveCtx = pCtx;` |
|      451 | 1172 | `		pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|      451 | 1173 | `		rc = VmCoroRun(pVm, pCtx);` |
|      451 | 1174 | `		return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|        - | 1175 | `	}` |
|        - | 1176 | `#endif` |
|        - | 1177 | `	/* A resume is a native VmByteCodeExec re-entry, bounded by nMaxNativeDepth.` |
|        - | 1178 | `	 * Reject HERE, before re-attaching the (possibly deep) parked segment to the` |
|        - | 1179 | `	 * frame chain and re-adding its nRecords to nRecursionDepth — a wrapper-level` |
|        - | 1180 | `	 * abort past those mutations would leave pVm->pFrame pointing into the parked` |
|        - | 1181 | `	 * callee and the depth accounting un-reverted. The PHP call-depth cap is` |
|        - | 1182 | `	 * OP_CALL-only. */` |
|     1775 | 1183 | `	if( VmNativeNestingExceeded(pVm) ){` |
|      ! 0 | 1184 | `		return VmNativeNestingFatal(pVm);` |
|        - | 1185 | `	}` |
|        - | 1186 | `	/* Push the resume value onto the SUSPENDED activation's operand stack so it` |
|        - | 1187 | `	 * appears as Fiber::suspend()'s return value. For a deep suspend (stage 4)` |
|        - | 1188 | `	 * that stack is the innermost callee's — parked in the segment — not the` |
|        - | 1189 | `	 * body's. nTos was saved one below the return-value slot. */` |
|        - | 1190 | `	{` |
|        - | 1191 | `		ph7_value *pResumeStack;` |
|     1775 | 1192 | `		pSeg = (VmParkedSegment *)pCtx->pParkedSegment;` |
|     1775 | 1193 | `		pResumeStack = pSeg ? pSeg->sState.pStack : pCtx->pStack;` |
|     1775 | 1194 | `		if( pResumeValue ){` |
|      215 | 1195 | `			PH7_MemObjStore(pResumeValue, &pResumeStack[pCtx->nTos + 1]);` |
|      110 | 1196 | `		}else{` |
|     1565 | 1197 | `			PH7_MemObjRelease(&pResumeStack[pCtx->nTos + 1]);` |
|        - | 1198 | `		}` |
|     1775 | 1199 | `		pCtx->nTos++;` |
|        - | 1200 | `		/* Refresh the caller-depth base and re-publish this body's own exception` |
|        - | 1201 | `		 * handlers on top of pVm->aException at that depth, so the resumed body's` |
|        - | 1202 | `		 * try/catch and finally-drain bound line up (see VmByteCodeExec's base` |
|        - | 1203 | `		 * override). Must run before VmByteCodeExec recaptures its local base. */` |
|     1775 | 1204 | `		pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|     1775 | 1205 | `		pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);` |
|     1775 | 1206 | `		pCtx->nSelfBase = SySetUsed(&pVm->aSelf);` |
|     1775 | 1207 | `		VmRestoreCtxState(pVm, pCtx);` |
|     1775 | 1208 | `		if( pSeg ){` |
|        - | 1209 | `			/* Reactivate the parked records' recursion accounting (mirror of the` |
|        - | 1210 | `			 * deactivate at suspend); aSelf was just restored above. */` |
|      ! 0 | 1211 | `			pVm->nRecursionDepth += pSeg->nRecords;` |
|        - | 1212 | `			/* Rebase the parked segment's absolute exception-floor indices: the` |
|        - | 1213 | `			 * fiber may resume at a different caller depth than it suspended at,` |
|        - | 1214 | `			 * so every activation's nExceptionBase shifts by the same delta the` |
|        - | 1215 | `			 * republished handlers moved (newBase - the park-time base). */` |
|      ! 0 | 1216 | `			sxi32 iDelta = (sxi32)pCtx->nExceptionBase - (sxi32)pSeg->nOldExcBase;` |
|        - | 1217 | `			/* nFinallyActBase floors rebase by their OWN delta — the exception and` |
|        - | 1218 | `			 * finally-action stacks move independently between suspend and resume` |
|        - | 1219 | `			 * (a fiber resumed from inside a generator's inline finally sees a` |
|        - | 1220 | `			 * DEEPER aFinallyAction with an unchanged aException, and a stale` |
|        - | 1221 | `			 * absolute floor would make the activation-end discard eat the` |
|        - | 1222 | `			 * resumer's pending action). */` |
|      ! 0 | 1223 | `			sxi32 iFinDelta = (sxi32)pCtx->nFinallyBase - (sxi32)pSeg->nOldFinBase;` |
|      ! 0 | 1224 | `			if( iDelta != 0 \|\| iFinDelta != 0 ){` |
|        - | 1225 | `				VmCallFrame *pRec;` |
|      ! 0 | 1226 | `				pSeg->sState.nExceptionBase =` |
|      ! 0 | 1227 | `					(sxu32)((sxi32)pSeg->sState.nExceptionBase + iDelta);` |
|      ! 0 | 1228 | `				pSeg->sState.nFinallyActBase =` |
|      ! 0 | 1229 | `					(sxu32)((sxi32)pSeg->sState.nFinallyActBase + iFinDelta);` |
|      ! 0 | 1230 | `				for( pRec = pSeg->pCallTop; pRec; pRec = pRec->pPrev ){` |
|      ! 0 | 1231 | `					pRec->sCaller.nExceptionBase =` |
|      ! 0 | 1232 | `						(sxu32)((sxi32)pRec->sCaller.nExceptionBase + iDelta);` |
|      ! 0 | 1233 | `					pRec->sCaller.nFinallyActBase =` |
|      ! 0 | 1234 | `						(sxu32)((sxi32)pRec->sCaller.nFinallyActBase + iFinDelta);` |
|      ! 0 | 1235 | `				}` |
|      ! 0 | 1236 | `			}` |
|      ! 0 | 1237 | `		}` |
|        - | 1238 | `		/* Re-attach the coroutine to the live VM frame chain: the body frame's` |
|        - | 1239 | `		 * parent becomes the resumer's current frame. For a deep segment the` |
|        - | 1240 | `		 * suspend-time top frame (the innermost callee / open-try wrapper) then` |
|        - | 1241 | `		 * becomes current so the adopt at VmByteCodeExec entry resumes inside the` |
|        - | 1242 | `		 * callee; body-level resumes make the body frame current. */` |
|     1775 | 1243 | `		VmStampCoroutineCallSite(pVm, pCtx);` |
|     1775 | 1244 | `		if( pCtx->pPrivate ){` |
|     1775 | 1245 | `			VmGeneratorStampEntry(pVm, pCtx);` |
|      885 | 1246 | `		}` |
|     1775 | 1247 | `		pCtx->pFrame->pParent = pVm->pFrame;` |
|     1775 | 1248 | `		pVm->pFrame = pSeg ? pSeg->pTopFrame : pCtx->pFrame;` |
|        - | 1249 | `	}` |
|        - | 1250 | `	/* The segment (if any) is handed to the body invocation explicitly below; it` |
|        - | 1251 | `	 * is no longer part of the suspended ctx state once resume owns it. */` |
|     1775 | 1252 | `	pCtx->pParkedSegment = 0;` |
|        - | 1253 | `	/* Save and set the active context */` |
|     1775 | 1254 | `	pOldCtx = pVm->pActiveCtx;` |
|     1775 | 1255 | `	pVm->pActiveCtx = pCtx;` |
|     1775 | 1256 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|     1775 | 1257 | `	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1; /* see VmStartCtx */` |
|        - | 1258 | `	/* Resume execution from saved PC. pSeg, when non-NULL, makes this body` |
|        - | 1259 | `	 * re-enter inside the innermost parked callee (deep fiber resume, stage 4). */` |
|     2660 | 1260 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|      885 | 1261 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|      885 | 1262 | `		VmCtxEnforceRetFunc(pCtx), FALSE, pSeg, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);` |
|     1775 | 1263 | `	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|     1113 | 1264 | `}` |
|        - | 1265 | `/*` |
|        - | 1266 | ` * Force-close a suspended generator context at destruction time, running its` |
|        - | 1267 | `` * pending `finally` blocks (PHP runs finally when a generator is unset / goes out`` |
|        - | 1268 | ` * of scope / is GC'd before it completes; PHL previously freed the open try` |
|        - | 1269 | ` * handlers unexecuted in VmReleaseExecCtx). This is a "close", not a "resume":` |
|        - | 1270 | `` * the finally handler of every still-open `try` the generator was suspended`` |
|        - | 1271 | `` * inside runs innermost-first, but NO `catch` runs and no code past the finallys`` |
|        - | 1272 | ` * executes.` |
|        - | 1273 | ` *` |
|        - | 1274 | ` * Generators compile finally INLINE (ROOT C): the finally bytecode lives in the` |
|        - | 1275 | ` * body program at iFinallyPc, driven by the aException/aFinallyAction pc-redirect` |
|        - | 1276 | `` * machinery — not the legacy detached `sFinally` mini-program VmDrainFinally runs.`` |
|        - | 1277 | `` * So a close is expressed exactly like a `return` that crosses every enclosing`` |
|        - | 1278 | ` * finally (OP_SET_FINALLY_RET): set pCtx->bClosing and resume; the body-entry` |
|        - | 1279 | ` * redirect (see VmByteCodeExecBody) seeds a PH7_FA_RETURN action and jumps into` |
|        - | 1280 | ` * the innermost open try's finally, and OP_END_FINALLY threads it out through the` |
|        - | 1281 | ` * chain, then completes the body.` |
|        - | 1282 | ` *` |
|        - | 1283 | ` * Scope: a plain body-level suspend (pParkedSegment == 0). A deep fiber segment is` |
|        - | 1284 | ` * left to plain release (generators never park one — yield is body-level only). A` |
|        - | 1285 | `` * `yield` reached inside a finally during close is rejected by OP_YIELD via`` |
|        - | 1286 | ` * pCtx->bClosing (PHP-exact "Cannot yield from finally in a force-closed` |
|        - | 1287 | ` * generator"). Deferred edges remain.` |
|        - | 1288 | ` *` |
|        - | 1289 | ` * Returns whatever the body run returns: SXRET_OK on a clean close, PH7_ABORT if a` |
|        - | 1290 | ` * finally aborts, or PH7_EXCEPTION if a finally threw past itself (surfaced to the` |
|        - | 1291 | ` * destruct caller).` |
|        - | 1292 | ` */` |
|      764 | 1293 | `static sxi32 VmCloseCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        5 | 1294 | `{` |
|        - | 1295 | `	sxi32 rc;` |
|      769 | 1296 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        - | 1297 | `		/* CREATED never ran, so has no open try; COMPLETED/CLOSED already ran theirs. */` |
|      525 | 1298 | `		return SXRET_OK;` |
|        - | 1299 | `	}` |
|      249 | 1300 | `	if( pCtx->pParkedSegment != 0 ){` |
|        - | 1301 | `		/* Deep fiber segment (never a generator) — leave to plain release. */` |
|      ! 0 | 1302 | `		return SXRET_OK;` |
|        - | 1303 | `	}` |
|        - | 1304 | ``	/* Suspended mid `yield from` over an inner generator: PHP closes innermost-first,`` |
|        - | 1305 | `	 * so run the delegate's finallys before this body's. Both delegate-object states` |
|        - | 1306 | `	 * carry the live iterator in sDelegate — state 3 (the delegate IS a Generator) and` |
|        - | 1307 | ``	 * state 2 (an Iterator, which for `yield from $aggregate` is the generator returned`` |
|        - | 1308 | `	 * by getIterator()); VmGeneratorExtractCtx returns 0 for a non-generator iterator,` |
|        - | 1309 | `	 * so a plain Iterator/array delegate is skipped. Recurses for nested delegation;` |
|        - | 1310 | `	 * the inner ends COMPLETED, so its own later __destruct close is a no-op. */` |
|      249 | 1311 | `	if( pCtx->iDelegateState >= 2 && (pCtx->sDelegate.iFlags & MEMOBJ_OBJ) ){` |
|       18 | 1312 | `		ph7_generator *pInner = VmGeneratorExtractCtx(pVm, &pCtx->sDelegate);` |
|       18 | 1313 | `		if( pInner && pInner->pCtx ){` |
|       18 | 1314 | `			sxi32 rcInner = VmCloseCtx(pVm, pInner->pCtx);` |
|       18 | 1315 | `			if( rcInner == PH7_ABORT ){ return PH7_ABORT; }` |
|        7 | 1316 | `		}` |
|        7 | 1317 | `	}` |
|        - | 1318 | `	/* Drive the pending finallys through a real body resume that the entry redirect` |
|        - | 1319 | `	 * turns into a finally-chain unwind. VmResumeCtx handles state restore, frame` |
|        - | 1320 | `	 * re-attach, active-ctx save/restore and the terminal-state bookkeeping. */` |
|      249 | 1321 | `	pCtx->bClosing = 1;` |
|      249 | 1322 | `	rc = VmResumeCtx(pVm, pCtx, 0, 0);` |
|      249 | 1323 | `	pCtx->bClosing = 0;` |
|      249 | 1324 | `	return rc;` |
|      387 | 1325 | `}` |
|        - | 1326 | `/*` |
|        - | 1327 | ` * Free one DETACHED frame (not in the live pVm->pFrame chain): the frame of a` |
|        - | 1328 | ` * suspended coroutine's body, or of a segment activation abandoned mid-call.` |
|        - | 1329 | ` * Mirrors VmLeaveFrame's teardown minus the chain pop (there is no chain to pop` |
|        - | 1330 | ` * from). Factored so the body-frame free and the stage-4 segment free share it.` |
|        - | 1331 | ` */` |
|     1290 | 1332 | `static void VmFreeDetachedFrame(ph7_vm *pVm, VmFrame *pFrame)` |
|        5 | 1333 | `{` |
|        - | 1334 | `	VmSlot *aSlot;` |
|        - | 1335 | `	sxu32 n;` |
|     1295 | 1336 | `	if( pFrame == 0 ){` |
|      ! 0 | 1337 | `		return;` |
|        - | 1338 | `	}` |
|        - | 1339 | `	/* The activation's hold on the function it was running, the same one` |
|        - | 1340 | `	 * VmLeaveFrame gives back for a live frame. */` |
|     1295 | 1341 | `	if( pFrame->pUserData ){` |
|     1295 | 1342 | `		PH7_VmClosureFuncUnref(pVm,(ph7_vm_func *)pFrame->pUserData);` |
|     1295 | 1343 | `		pFrame->pUserData = 0;` |
|      645 | 1344 | `	}` |
|        - | 1345 | `	/* End the foreach walks this (abandoned) activation never finished — the same` |
|        - | 1346 | `	 * teardown, at the same point, VmLeaveFrame does it at. */` |
|     1295 | 1347 | `	VmReleaseFrameForeachSteps(pVm,pFrame);` |
|        - | 1348 | `	/* Remove local references FIRST, then free the locals nothing else holds — the` |
|        - | 1349 | `	 * order and the holder test VmLeaveFrame explains. */` |
|     1295 | 1350 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);` |
|     2503 | 1351 | `	for( n = 0; n < SySetUsed(&pFrame->sRef); ++n ){` |
|     1213 | 1352 | `		PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|      609 | 1353 | `	}` |
|        - | 1354 | `	/* Free local variables */` |
|     1295 | 1355 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|     2471 | 1356 | `	for( n = 0; n < SySetUsed(&pFrame->sLocal); ++n ){` |
|     1181 | 1357 | `		if( PH7_VmSlotHolderCount(pVm, aSlot[n].nIdx) > 0 ){` |
|      ! 0 | 1358 | `			continue;` |
|        - | 1359 | `		}` |
|     1181 | 1360 | `		PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|      593 | 1361 | `	}` |
|     1295 | 1362 | `	SyHashRelease(&pFrame->hVar);` |
|     1295 | 1363 | `	SySetRelease(&pFrame->sArg);` |
|     1295 | 1364 | `	SySetRelease(&pFrame->sLocal);` |
|     1295 | 1365 | `	SySetRelease(&pFrame->sRef);` |
|     1295 | 1366 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|        - | 1367 | `	/* Drop a resume target pointing at this detached frame before we free it (ROOT B). */` |
|     1295 | 1368 | `	VmDropResumeTarget(pVm,pFrame);` |
|     1295 | 1369 | `	SyMemBackendPoolFree(&pVm->sAllocator, pFrame);` |
|      650 | 1370 | `}` |
|        - | 1371 | `/*` |
|        - | 1372 | ` * Free a parked deep-suspend segment (stage 4) whose fiber was abandoned while` |
|        - | 1373 | ` * suspended. Every record holds a callee's operand stack and VmFrame (the` |
|        - | 1374 | ` * topmost record's callee is the innermost activation, running on sState); walk` |
|        - | 1375 | ` * the chain releasing each callee stack's live entries then the stack and frame.` |
|        - | 1376 | ` * The body frame/stack are NOT here — they are freed by the caller` |
|        - | 1377 | ` * (VmReleaseExecCtx) as pCtx->pFrame / pCtx->pStack.` |
|        - | 1378 | ` */` |
|      ! 0 | 1379 | `static void VmFreeParkedSegment(ph7_vm *pVm, ph7_exec_ctx *pCtx, VmParkedSegment *pSeg)` |
|      ! 0 | 1380 | `{` |
|        - | 1381 | `	/* Live top-of-stack of the activation running on the current record's callee` |
|        - | 1382 | `	 * stack: the innermost (sState) for the topmost record, then each caller. */` |
|      ! 0 | 1383 | `	ph7_value *pTosAbove = pSeg->sState.pTos;` |
|      ! 0 | 1384 | `	VmCallFrame *pRec = pSeg->pCallTop, *pNext;` |
|      ! 0 | 1385 | `	while( pRec ){` |
|      ! 0 | 1386 | `		ph7_value *pStk = pRec->sCall.pFrameStack;` |
|      ! 0 | 1387 | `		if( pStk ){` |
|      ! 0 | 1388 | `			ph7_value *pTos = pTosAbove;` |
|      ! 0 | 1389 | `			while( pTos >= pStk ){` |
|      ! 0 | 1390 | `				PH7_MemObjRelease(pTos);` |
|      ! 0 | 1391 | `				pTos--;` |
|      ! 0 | 1392 | `			}` |
|      ! 0 | 1393 | `			SyMemBackendFree(&pVm->sAllocator, pStk);` |
|      ! 0 | 1394 | `		}` |
|      ! 0 | 1395 | `		VmFreeDetachedFrame(pVm, pRec->sCall.pFrame);` |
|        - | 1396 | `		/* The caller recorded here runs on the NEXT-lower callee stack; grab its` |
|        - | 1397 | `		 * live tos before freeing this node. */` |
|      ! 0 | 1398 | `		pTosAbove = pRec->sCaller.pTos;` |
|      ! 0 | 1399 | `		pNext = pRec->pPrev;` |
|      ! 0 | 1400 | `		SyMemBackendPoolFree(&pVm->sAllocator, pRec);` |
|      ! 0 | 1401 | `		pRec = pNext;` |
|      ! 0 | 1402 | `	}` |
|        - | 1403 | `	/* pTosAbove now points at the BODY activation's live top (the bottom record's` |
|        - | 1404 | `	 * sCaller, which runs on pCtx->pStack). pCtx->nTos still holds the INNERMOST` |
|        - | 1405 | `	 * index (VmSuspendCtx saved it), which would over-index the body stack in` |
|        - | 1406 | `	 * VmReleaseExecCtx's release loop — correct it to the body's real top. */` |
|      ! 0 | 1407 | `	pCtx->nTos = (sxi32)(pTosAbove - pCtx->pStack);` |
|      ! 0 | 1408 | `	SyMemBackendFree(&pVm->sAllocator, pSeg);` |
|      ! 0 | 1409 | `}` |
|        - | 1410 | `/*` |
|        - | 1411 | ` * Release an execution context and all its resources.` |
|        - | 1412 | ` */` |
|     1290 | 1413 | `PH7_PRIVATE void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        5 | 1414 | `{` |
|     1295 | 1415 | `	if( pCtx == 0 ){` |
|      ! 0 | 1416 | `		return;` |
|        - | 1417 | `	}` |
|     1295 | 1418 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        - | 1419 | `		/* Cannot destroy a fiber that is currently executing */` |
|      ! 0 | 1420 | `		return;` |
|        - | 1421 | `	}` |
|        - | 1422 | `#ifdef PH7_CORO_STACK` |
|     1295 | 1423 | `	if( pCtx->pCoro && pCtx->iState == PH7_CTX_STATE_SUSPENDED && !pCtx->bCoroDone ){` |
|        - | 1424 | `		/* A fiber abandoned while suspended still has live C frames on its own` |
|        - | 1425 | `		 * stack -- a half-finished array_map, an open handle, an operand stack of` |
|        - | 1426 | `		 * its own -- and nothing outside that stack can reach them. Switch back in` |
|        - | 1427 | `		 * one last time with the kill flag set: the suspend it is parked on` |
|        - | 1428 | `		 * returns PH7_ABORT, every frame between there and the body entry runs its` |
|        - | 1429 | `		 * own abort path, and the body returns for good. VmFinishCtxRun then frees` |
|        - | 1430 | `		 * the stack. (php unwinds a dropped fiber for the same reason; that its` |
|        - | 1431 | ``		 * unwind also runs the body's `finally` blocks and this one does not is`` |
|        - | 1432 | `		 * recorded separately -- an abort is not a return.) */` |
|      223 | 1433 | `		pCtx->bCoroKill = 1;` |
|      223 | 1434 | `		if( pCtx->pFrame ){` |
|      223 | 1435 | `			pCtx->pFrame->pNativeCallerRec = 0;` |
|      110 | 1436 | `		}` |
|      223 | 1437 | `		(void)VmResumeCtx(pVm, pCtx, 0, 0);` |
|      110 | 1438 | `	}` |
|     1295 | 1439 | `	if( pCtx->pCoro ){` |
|        - | 1440 | `		/* Never started, or the unwind above could not run it: the stack holds` |
|        - | 1441 | `		 * nothing live either way. */` |
|      ! 0 | 1442 | `		VmCoroFree(pVm, pCtx->pCoro);` |
|      ! 0 | 1443 | `		pCtx->pCoro = 0;` |
|      ! 0 | 1444 | `	}` |
|     1295 | 1445 | `	if( pCtx->pEscaped ){` |
|        - | 1446 | `		/* The body threw and nobody was left to re-raise it (the resumer aborted` |
|        - | 1447 | `		 * between the two). Give the reference back. */` |
|      ! 0 | 1448 | `		PH7_ClassInstanceUnref(pCtx->pEscaped);` |
|      ! 0 | 1449 | `		pCtx->pEscaped = 0;` |
|      ! 0 | 1450 | `	}` |
|     1295 | 1451 | `	VmCoroStateRelease(pVm, &pCtx->sSaved);` |
|     1295 | 1452 | `	if( pCtx->bTramp ){` |
|        - | 1453 | `		sxu32 k;` |
|       53 | 1454 | `		PH7_MemObjRelease(&pCtx->sTramp);` |
|      101 | 1455 | `		for( k = 0 ; k < pCtx->nTrampArg ; ++k ){` |
|       51 | 1456 | `			PH7_MemObjRelease(&pCtx->aTrampArg[k]);` |
|       27 | 1457 | `		}` |
|       53 | 1458 | `		if( pCtx->aTrampArg ){` |
|       37 | 1459 | `			SyMemBackendFree(&pVm->sAllocator, pCtx->aTrampArg);` |
|       37 | 1460 | `			pCtx->aTrampArg = 0;` |
|       17 | 1461 | `		}` |
|       53 | 1462 | `		pCtx->nTrampArg = 0;` |
|       53 | 1463 | `		pCtx->bTramp = 0;` |
|       25 | 1464 | `	}` |
|        - | 1465 | `#endif` |
|     1295 | 1466 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|        - | 1467 | `	/* ...and give back the hold VmNewExecCtx took on the function this coroutine runs. */` |
|     1295 | 1468 | `	if( pCtx->pFunc ){` |
|     1295 | 1469 | `		PH7_VmClosureFuncUnref(pVm,pCtx->pFunc);` |
|      645 | 1470 | `	}` |
|        - | 1471 | `	/* Release values */` |
|     1295 | 1472 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|     1295 | 1473 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|     1295 | 1474 | `	PH7_MemObjRelease(&pCtx->sDelegate);` |
|        - | 1475 | `	/* Stage 2b: the parked entries are per-activation clones now — free them` |
|        - | 1476 | `	 * (an abandoned suspended coroutine is their last holder). */` |
|     1295 | 1477 | `	VmExcReleaseAll(pVm,&pCtx->aSavedException);` |
|     1295 | 1478 | `	SySetRelease(&pCtx->aSavedException);` |
|        - | 1479 | `	/* ROOT C: a generator abandoned while suspended inside a finally may carry parked` |
|        - | 1480 | `	 * finally actions holding owned values/refs (a RETURN's sRet, a RETHROW's pExc).` |
|        - | 1481 | `	 * Release them so the abandon path leaks nothing. */` |
|        - | 1482 | `	{` |
|     1295 | 1483 | `		sxu32 n = SySetUsed(&pCtx->aSavedFinally);` |
|     1295 | 1484 | `		if( n > 0 ){` |
|      ! 0 | 1485 | `			VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pCtx->aSavedFinally);` |
|        - | 1486 | `			sxu32 i;` |
|      ! 0 | 1487 | `			for( i = 0; i < n; i++ ){` |
|      ! 0 | 1488 | `				if( aA[i].eKind == PH7_FA_RETURN ){` |
|      ! 0 | 1489 | `					PH7_MemObjRelease(&aA[i].sRet);` |
|      ! 0 | 1490 | `				}else if( aA[i].eKind == PH7_FA_RETHROW && aA[i].pExc ){` |
|      ! 0 | 1491 | `					PH7_ClassInstanceUnref(aA[i].pExc);` |
|      ! 0 | 1492 | `				}` |
|      ! 0 | 1493 | `			}` |
|      ! 0 | 1494 | `		}` |
|     1295 | 1495 | `		SySetRelease(&pCtx->aSavedFinally);` |
|        - | 1496 | `	}` |
|        - | 1497 | `	/* Stage 4: parked aSelf entries are borrowed class pointers — just free the set. */` |
|     1295 | 1498 | `	SySetRelease(&pCtx->aSavedSelf);` |
|        - | 1499 | `	/* Free a parked deep-suspend segment (stage 4): the fiber was abandoned while` |
|        - | 1500 | `	 * suspended inside a nested call, so its record chain / frames / operand` |
|        - | 1501 | `	 * stacks are still alive and only this holder references them. Must run` |
|        - | 1502 | `	 * before the body frame/stack below (they are the segment's floor). */` |
|     1295 | 1503 | `	if( pCtx->pParkedSegment ){` |
|      ! 0 | 1504 | `		VmFreeParkedSegment(pVm, pCtx, (VmParkedSegment *)pCtx->pParkedSegment);` |
|      ! 0 | 1505 | `		pCtx->pParkedSegment = 0;` |
|      ! 0 | 1506 | `	}` |
|        - | 1507 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|     1295 | 1508 | `	if( pCtx->pFrame ){` |
|     1295 | 1509 | `		VmFreeDetachedFrame(pVm, pCtx->pFrame);` |
|     1295 | 1510 | `		pCtx->pFrame = 0;` |
|      645 | 1511 | `	}` |
|        - | 1512 | `	/* A by-reference parameter aliased a CALLER's slot; the frame teardown above just` |
|        - | 1513 | `	 * dropped this body's name for it. Release it if nothing is left holding it — the` |
|        - | 1514 | `	 * caller may already be gone (it skipped the slot precisely because this frame` |
|        - | 1515 | `	 * held it), in which case this is its last holder. Runs after the frame so the` |
|        - | 1516 | `	 * body's own row is out of the count. */` |
|        - | 1517 | `	{` |
|        - | 1518 | `		sxu32 n;` |
|     1295 | 1519 | `		sxu32 *aIdx = (sxu32 *)SySetBasePtr(&pCtx->aByRefArg);` |
|     1329 | 1520 | `		for( n = 0; n < SySetUsed(&pCtx->aByRefArg); n++ ){` |
|       36 | 1521 | `			PH7_VmReleaseUnheldSlot(pVm, aIdx[n]);` |
|       19 | 1522 | `		}` |
|     1295 | 1523 | `		SySetRelease(&pCtx->aByRefArg);` |
|        - | 1524 | `	}` |
|        - | 1525 | `	/* Release individual operand stack entries (decrement refcounts,` |
|        - | 1526 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|        - | 1527 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|     1295 | 1528 | `	if( pCtx->pStack ){` |
|     1295 | 1529 | `		if( pCtx->nTos >= 0 ){` |
|      677 | 1530 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|     1375 | 1531 | `			while( pTos >= pCtx->pStack ){` |
|      703 | 1532 | `				PH7_MemObjRelease(pTos);` |
|      703 | 1533 | `				pTos--;` |
|        5 | 1534 | `			}` |
|      336 | 1535 | `		}` |
|     1295 | 1536 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|     1295 | 1537 | `		pCtx->pStack = 0;` |
|      645 | 1538 | `	}` |
|        - | 1539 | `	/* Free the context itself */` |
|     1295 | 1540 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|      650 | 1541 | `}` |
|        - | 1542 | `/*` |
|        - | 1543 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|        - | 1544 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|        - | 1545 | ` */` |
|     1740 | 1546 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|        5 | 1547 | `{` |
|        - | 1548 | `	ph7_class_instance *pThis;` |
|        - | 1549 | `	SyString sAttr;` |
|        - | 1550 | `	ph7_value *pAttr;` |
|     1745 | 1551 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 1552 | `		return 0;` |
|        - | 1553 | `	}` |
|     1745 | 1554 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|     1745 | 1555 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|      ! 0 | 1556 | `		return 0;` |
|        - | 1557 | `	}` |
|     1745 | 1558 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|     1745 | 1559 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     1745 | 1560 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      511 | 1561 | `		return 0;` |
|        - | 1562 | `	}` |
|     1239 | 1563 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|      875 | 1564 | `}` |
|        - | 1565 | `/* The three VM_INSTANCE_FCC_* Closure flags live in ph7int.h: vm_exec.c's OP_LOAD_FCC` |
|        - | 1566 | ` * stamps VM_INSTANCE_FCC_METHOD and this file reads all three. */` |
|        - | 1567 | `/*` |
|        - | 1568 | `` * A PHP closure (and a first-class callable `f(...)`) is a real object: an instance of`` |
|        - | 1569 | `` * the built-in final `Closure` class carrying its underlying callable in a private`` |
|        - | 1570 | `` * `$__fn` attribute (the callable NAME: a registered `[closure_N]`/`[lambda_N]` or a`` |
|        - | 1571 | ` * user/host function name) — plus, for a method/static first-class callable, a bound` |
|        - | 1572 | `` * `$__this` object and/or a `$__scope` class-name. Storing the callable as plain attributes`` |
|        - | 1573 | `` * (rather than a native resource pointer) means the object owns no `ph7_vm_func` — the`` |
|        - | 1574 | `` * per-closure function stays owned by `hFunction`/VM-release exactly as before, so there is`` |
|        - | 1575 | ` * no extra free path.` |
|        - | 1576 | ` *` |
|        - | 1577 | ` * Returns non-zero iff pVal is a Closure instance.` |
|        - | 1578 | ` */` |
| 22248189 | 1579 | `PH7_PRIVATE int VmValueIsClosure(ph7_vm *pVm, ph7_value *pVal)` |
|        5 | 1580 | `{` |
|        - | 1581 | `	ph7_class_instance *pThis;` |
|        - | 1582 | `	/* Flag test first: a non-object call target (the hot common case) bails before any` |
|        - | 1583 | `	 * pVm dereference; pClosureClass==0 is a one-time pre-init concern, so it goes last. */` |
| 22248194 | 1584 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
| 21842373 | 1585 | `		return 0;` |
|        - | 1586 | `	}` |
|   405826 | 1587 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|        - | 1588 | `	/* Closure is final, so an exact class match is correct (no subclasses possible). */` |
|   405826 | 1589 | `	return pThis->pClass == pVm->pClosureClass;` |
| 11124229 | 1590 | `}` |
|        - | 1591 | `/*` |
|        - | 1592 | ` * Unwrap a Closure value into the simple callable the existing dispatch machinery` |
|        - | 1593 | ` * already understands, written into pOut (which the caller must have initialised):` |
|        - | 1594 | `` *   - bound `$this` set (method first-class callable) -> [ $__this, $__fn ] array callable`` |
|        - | 1595 | `` *   - `$__scope` set (static first-class callable)     -> [ $__scope, $__fn ] array callable`` |
|        - | 1596 | `` *   - neither (plain function / real closure)          -> the `$__fn` name string`` |
|        - | 1597 | ` * The 2-element array form rides the existing MEMOBJ_HASHMAP dispatch, which binds $this` |
|        - | 1598 | ` * for an object first element and resolves the class for a class-name-string first element.` |
|        - | 1599 | ` * Returns SXRET_OK if pVal was a Closure (pOut filled), SXERR_NOTFOUND otherwise.` |
|        - | 1600 | ` */` |
|    68637 | 1601 | `PH7_PRIVATE sxi32 VmClosureUnwrap(ph7_vm *pVm, ph7_value *pVal, ph7_value *pOut)` |
|        5 | 1602 | `{` |
|        - | 1603 | `	ph7_class_instance *pThis;` |
|        - | 1604 | `	ph7_value *pFn;` |
|        - | 1605 | `	SyString sAttr;` |
|    68642 | 1606 | `	if( !VmValueIsClosure(pVm, pVal) ){` |
|      ! 0 | 1607 | `		return SXERR_NOTFOUND;` |
|        - | 1608 | `	}` |
|    68642 | 1609 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    68642 | 1610 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    68642 | 1611 | `	pFn = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    68642 | 1612 | `	if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|      ! 0 | 1613 | `		return SXERR_NOTFOUND; /* malformed/uninitialised closure */` |
|        - | 1614 | `	}` |
|        - | 1615 | `	/* Only a bound/static first-class callable (rare) carries $__this/$__scope; the` |
|        - | 1616 | `	 * VM_INSTANCE_FCC_BOUND flag (set by VmCreateClosure) keeps the hot plain-closure path` |
|        - | 1617 | `	 * to the single $__fn lookup above instead of two extra attribute lookups per dispatch. */` |
|    68642 | 1618 | `	if( pThis->iFlags & VM_INSTANCE_FCC_BOUND ){` |
|        - | 1619 | `		ph7_value *pBound, *pScope;` |
|        - | 1620 | `		int bBoundObj, bScope;` |
|      865 | 1621 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|      865 | 1622 | `		pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      865 | 1623 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      865 | 1624 | `		pScope = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      865 | 1625 | `		bBoundObj = pBound && (pBound->iFlags & MEMOBJ_OBJ);` |
|      865 | 1626 | `		bScope = pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0;` |
|      865 | 1627 | `		if( bBoundObj \|\| bScope ){` |
|        - | 1628 | `			/* Method/static first-class callable -> [ target, "method" ] array callable. */` |
|      865 | 1629 | `			if( pThis->iFlags & VM_INSTANCE_FCC_INVOKE_OBJ ){` |
|        - | 1630 | `				/* Closure::fromCallable($obj): the engine named __invoke, so this` |
|        - | 1631 | `				 * dispatch is the engine's own and a non-public one still runs. */` |
|       12 | 1632 | `				pVm->bMagicDispatch = 1;` |
|        5 | 1633 | `			}` |
|        - | 1634 | `			ph7_hashmap *pMap;` |
|        - | 1635 | `			ph7_value sTarget, sMeth;` |
|        - | 1636 | `			sxi32 rc;` |
|      865 | 1637 | `			if( bBoundObj ){` |
|      441 | 1638 | `				ph7_class_instance *pBoundObj = (ph7_class_instance *)pBound->x.pOther;` |
|        - | 1639 | ``				/* A bound PLAIN closure (`function(){…}->bindTo($o)`): $__fn names a function, not a`` |
|        - | 1640 | `				 * method of the bound object's class, so a [obj,method] array callable would fail` |
|        - | 1641 | `				 * method resolution. Stash the bound object (own a ref) so the OP_CALL user-function` |
|        - | 1642 | `				 * frame setup injects it as $this, and return the plain $__fn string for a normal` |
|        - | 1643 | `				 * function dispatch. */` |
|      436 | 1644 | `				if( (pThis->iFlags & VM_INSTANCE_FCC_METHOD) == 0` |
|      297 | 1645 | `				 && PH7_ClassExtractMethod(pBoundObj->pClass,` |
|      222 | 1646 | `						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0 ){` |
|        - | 1647 | `					/* Only a USER function (anonymous closure / named fn in hFunction) reads $this and` |
|        - | 1648 | `					 * reaches the OP_CALL user-function frame-setup that consumes pClosureThis. A HOST` |
|        - | 1649 | `					 * function (e.g. Closure::fromCallable('strlen')->bindTo($o)) ignores $this and` |
|        - | 1650 | `					 * dispatches via the host path which never consumes the transient — setting it` |
|        - | 1651 | `					 * there would leak the ref and inject a stale $this into the next call. */` |
|      207 | 1652 | `					if( SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),` |
|      142 | 1653 | `							SyBlobLength(&pFn->sBlob)) != 0 ){` |
|      142 | 1654 | `						pBoundObj->iRef++;` |
|      142 | 1655 | `						pVm->pClosureThis = pBoundObj;` |
|        - | 1656 | `						/* Carry the bound scope (if set) so private/protected member access inside` |
|        - | 1657 | `						 * the closure body resolves against it — bindTo($o, Scope::class) / call($o).` |
|        - | 1658 | ``						 * A receiver with NO scope runs in php's dummy one, `Closure` itself: that is`` |
|        - | 1659 | ``						 * what `self::` and a trace's class answer after `bindTo($o, null)`. */`` |
|      142 | 1660 | `						if( bScope ){` |
|      169 | 1661 | `							pVm->pClosureScope = PH7_VmExtractClass(pVm,` |
|      110 | 1662 | `								(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|       59 | 1663 | `						}else{` |
|       31 | 1664 | `							pVm->pClosureScope = pVm->pClosureClass;` |
|        - | 1665 | `						}` |
|       69 | 1666 | `					}` |
|      142 | 1667 | `					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      142 | 1668 | `					return SXRET_OK;` |
|        - | 1669 | `				}` |
|      154 | 1670 | `			}else{` |
|        - | 1671 | ``				/* Scope-only rebind of a PLAIN closure (`bindTo(null, Scope::class)`):`` |
|        - | 1672 | `				 * $__fn names a function, not a static method of the scope class, so` |
|        - | 1673 | `				 * the [scope, method] array callable below would fail method` |
|        - | 1674 | `				 * resolution. Dispatch the plain $__fn string and carry the scope for` |
|        - | 1675 | `				 * private/protected visibility (pClosureThis stays unset — no $this).` |
|        - | 1676 | `				 * A static-method FCC ($__fn really is a method of the scope class)` |
|        - | 1677 | `				 * falls through to the array-callable path. */` |
|      641 | 1678 | `				ph7_class *pScopeClass = PH7_VmExtractClass(pVm,` |
|      424 | 1679 | `					(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      424 | 1680 | `				if( pScopeClass == 0` |
|      429 | 1681 | `				 \|\| ((pThis->iFlags & VM_INSTANCE_FCC_METHOD) == 0` |
|      233 | 1682 | `				  && PH7_ClassExtractMethod(pScopeClass,` |
|       63 | 1683 | `						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0) ){` |
|       42 | 1684 | `					if( pScopeClass` |
|       45 | 1685 | `					 && SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),` |
|       42 | 1686 | `							SyBlobLength(&pFn->sBlob)) != 0 ){` |
|       45 | 1687 | `						pVm->pClosureScope = pScopeClass;` |
|       45 | 1688 | `						pVm->bClosureUnbound = 1;` |
|       21 | 1689 | `					}` |
|       45 | 1690 | `					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|       45 | 1691 | `					return SXRET_OK;` |
|        - | 1692 | `				}` |
|        - | 1693 | `			}` |
|      685 | 1694 | `			pMap = PH7_NewHashmap(&(*pVm), 0, 0);` |
|      685 | 1695 | `			if( pMap == 0 ){` |
|      ! 0 | 1696 | `				return SXERR_NOTFOUND;` |
|        - | 1697 | `			}` |
|      685 | 1698 | `			PH7_MemObjInit(pVm, &sTarget);` |
|      685 | 1699 | `			PH7_MemObjInit(pVm, &sMeth);` |
|      685 | 1700 | `			if( bBoundObj ){` |
|      303 | 1701 | `				PH7_MemObjStore(pBound, &sTarget); /* bound object (iRef++) -> binds $this */` |
|      154 | 1702 | `			}else{` |
|        - | 1703 | `				ph7_value *pCalled;` |
|      387 | 1704 | `				SyStringInitFromBuf(&sAttr, "__called", 8);` |
|      387 | 1705 | `				pCalled = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      387 | 1706 | `				if( pCalled && (pCalled->iFlags & MEMOBJ_STRING) && SyBlobLength(&pCalled->sBlob) > 0 ){` |
|        - | 1707 | ``					/* A forwarding `self::sf(...)`/`parent::sf(...)` (OP_LOAD_FCC): the call goes`` |
|        - | 1708 | ``					 * THROUGH the caller's late-static-binding class, so `static::` answers it,`` |
|        - | 1709 | `					 * while the callee stays the one resolved in $__scope. */` |
|      189 | 1710 | `					pVm->pClosureMethodCls = PH7_VmExtractClass(pVm,` |
|       94 | 1711 | `						(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|       95 | 1712 | `					PH7_MemObjStringAppend(&sTarget, SyBlobData(&pCalled->sBlob), SyBlobLength(&pCalled->sBlob));` |
|       48 | 1713 | `				}else{` |
|      293 | 1714 | `					PH7_MemObjStringAppend(&sTarget, SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob));` |
|        - | 1715 | `				}` |
|        - | 1716 | `			}` |
|      685 | 1717 | `			PH7_MemObjStringAppend(&sMeth, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      685 | 1718 | `			rc = PH7_HashmapInsert(pMap, 0, &sTarget);` |
|      685 | 1719 | `			if( rc == SXRET_OK ){` |
|      685 | 1720 | `				rc = PH7_HashmapInsert(pMap, 0, &sMeth);` |
|      340 | 1721 | `			}` |
|      685 | 1722 | `			PH7_MemObjRelease(&sTarget);` |
|      685 | 1723 | `			PH7_MemObjRelease(&sMeth);` |
|      685 | 1724 | `			if( rc != SXRET_OK ){` |
|      ! 0 | 1725 | `				PH7_HashmapRelease(pMap, TRUE); /* free the partial map, no leak */` |
|      ! 0 | 1726 | `				return SXERR_NOTFOUND;` |
|        - | 1727 | `			}` |
|      685 | 1728 | `			if( pThis->iFlags & VM_INSTANCE_FCC_SCREENED ){` |
|        - | 1729 | `				/* php resolves a method Closure's callee ONCE, where the closure is built, and` |
|        - | 1730 | ``				 * keeps the resolved function: an escaped `$this->priv(...)` runs anywhere. PHL`` |
|        - | 1731 | `				 * keeps only a NAME, so every dispatch site would re-decide visibility against the` |
|        - | 1732 | `				 * CALLER and refuse the closure php runs. The creation sites screen (OP_LOAD_FCC's` |
|        - | 1733 | `				 * VmFccMemberError, Closure::fromCallable's PH7_VmIsCallable gate, and reflection,` |
|        - | 1734 | `				 * which php lets past protection on purpose) and stamp the mark only when a real` |
|        - | 1735 | `				 * method answered — a name the class reaches through __call carries no mark and` |
|        - | 1736 | `				 * still routes to the catch-all. Armed only once the pair below really exists, so` |
|        - | 1737 | `				 * a failed build cannot leave it standing; the consumers clear it like` |
|        - | 1738 | `				 * pClosureThis/pClosureScope. */` |
|      375 | 1739 | `				pVm->bClosureScreened = 1;` |
|      500 | 1740 | `			}else if( !bBoundObj && (pThis->iFlags & VM_INSTANCE_FCC_METHOD) ){` |
|        - | 1741 | `				/* No method answered and no receiver is bound: the __callStatic trampoline` |
|        - | 1742 | `				 * (PH7_VmClosureIsStatic's rule). php keeps it, so the pair must not re-pick` |
|        - | 1743 | ``				 * the catch-all against whatever `$this` the CALLER holds. */`` |
|      212 | 1744 | `				pVm->bClosureStaticTramp = 1;` |
|      104 | 1745 | `			}` |
|      680 | 1746 | `			if( (pThis->iFlags & (VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SCREENED\|VM_INSTANCE_FCC_SYNTAX))` |
|      345 | 1747 | `					== VM_INSTANCE_FCC_METHOD ){` |
|        - | 1748 | `				/* A trampoline Closure::fromCallable() minted (the reflection's rule): no` |
|        - | 1749 | `				 * parameters, so no name can bind. */` |
|      191 | 1750 | `				pVm->bClosureNoNamed = 1;` |
|       94 | 1751 | `			}` |
|      685 | 1752 | `			if( bBoundObj && bScope && (pThis->iFlags & VM_INSTANCE_FCC_METHOD) ){` |
|        - | 1753 | ``				/* A method closure whose `$__scope` is not its receiver's class names the class`` |
|        - | 1754 | ``				 * its callee was RESOLVED in: `parent::m(...)` and `A::m(...)` with a `$this`,`` |
|        - | 1755 | `				 * ReflectionMethod::getClosure(). php keeps that function, so the pair must not` |
|        - | 1756 | `				 * be looked up on the receiver again -- that found the receiver's own override. */` |
|      293 | 1757 | `				ph7_class *pRecvCls = ((ph7_class_instance *)pBound->x.pOther)->pClass;` |
|      437 | 1758 | `				ph7_class *pFromCls = PH7_VmExtractClass(pVm,` |
|      288 | 1759 | `					(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      288 | 1760 | `				if( pFromCls && pFromCls != pRecvCls && PH7_VmInstanceOf(pRecvCls,pFromCls)` |
|      111 | 1761 | `				 && PH7_ClassExtractMethod(pFromCls,` |
|      106 | 1762 | `						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) ){` |
|       97 | 1763 | `					pVm->pClosureMethodCls = pFromCls;` |
|       47 | 1764 | `				}` |
|      144 | 1765 | `			}` |
|      685 | 1766 | `			pOut->x.pOther = pMap;` |
|      685 | 1767 | `			MemObjSetType(pOut, MEMOBJ_HASHMAP);` |
|      685 | 1768 | `			return SXRET_OK;` |
|        - | 1769 | `		}` |
|      ! 0 | 1770 | `	}` |
|    67777 | 1771 | `	if( (pThis->iFlags & VM_INSTANCE_FCC_REBOUND)` |
|    33618 | 1772 | `	 && SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) ){` |
|        - | 1773 | ``		/* A rebind that left a plain closure with no `$this` (`bindTo(null)`): php has dropped`` |
|        - | 1774 | `		 * the receiver, so the one its function captured where it was created must not come` |
|        - | 1775 | `		 * back at the call. Only a USER function reaches the frame setup that consumes this,` |
|        - | 1776 | `		 * the same rule as the bound branch above. A rebind that kept a scope wrote it down` |
|        - | 1777 | `		 * (VmClosureRebind), so a non-method one with none here was given php's null scope. */` |
|       24 | 1778 | `		pVm->bClosureUnbound = (pThis->iFlags & VM_INSTANCE_FCC_METHOD) ? 1 : PH7_CLOSURE_UNSCOPED;` |
|       10 | 1779 | `	}` |
|    67782 | 1780 | `	PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    67782 | 1781 | `	return SXRET_OK;` |
|    34038 | 1782 | `}` |
|        - | 1783 | `/*` |
|        - | 1784 | ` * php's SCOPE for a Closure — the class whose private members its body may reach, and the` |
|        - | 1785 | ` * class Reflection reports. For a plain closure that is the class it was bound to; for a` |
|        - | 1786 | ` * METHOD closure it is the class that DECLARED the method, which is not the class the` |
|        - | 1787 | `` * callable NAMED: `(new Kid)->mk()` returning `$this->basePriv(...)` is scoped to Base in`` |
|        - | 1788 | `` * php, while `$__scope` records Kid — the class the call goes THROUGH, which is the CALLED`` |
|        - | 1789 | `` * scope (`static::`) and a different question. A trait method is composed into the using`` |
|        - | 1790 | ` * class, so PH7_VmMethodScopeName answers for it exactly as it does at every refusal site.` |
|        - | 1791 | ` * Returns 0 when the closure carries no scope at all.` |
|        - | 1792 | ` */` |
|      334 | 1793 | `PH7_PRIVATE ph7_class * PH7_VmClosureScopeClass(ph7_vm *pVm, ph7_class_instance *pClosure)` |
|        5 | 1794 | `{` |
|        - | 1795 | `	SyString sAttr;` |
|        - | 1796 | `	ph7_value *pScope, *pFn;` |
|        - | 1797 | `	ph7_class *pClass;` |
|      339 | 1798 | `	if( pClosure == 0 ){` |
|      ! 0 | 1799 | `		return 0;` |
|        - | 1800 | `	}` |
|      339 | 1801 | `	SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      339 | 1802 | `	pScope = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|      339 | 1803 | `	if( pScope == 0 \|\| (pScope->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pScope->sBlob) == 0 ){` |
|      ! 0 | 1804 | `		return 0;` |
|        - | 1805 | `	}` |
|      506 | 1806 | `	pClass = PH7_VmExtractClass(pVm, (const char *)SyBlobData(&pScope->sBlob),` |
|      167 | 1807 | `		SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      339 | 1808 | `	if( pClass == 0 \|\| (pClosure->iFlags & VM_INSTANCE_FCC_METHOD) == 0 ){` |
|       31 | 1809 | `		return pClass;` |
|        - | 1810 | `	}` |
|      309 | 1811 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|      309 | 1812 | `	pFn = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|      309 | 1813 | `	if( pFn && (pFn->iFlags & MEMOBJ_STRING) && SyBlobLength(&pFn->sBlob) > 0 ){` |
|      461 | 1814 | `		ph7_class_method *pMeth = PH7_ClassExtractMethod(pClass,` |
|      304 | 1815 | `			(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      309 | 1816 | `		if( pMeth ){` |
|      148 | 1817 | `			return PH7_VmMethodScopeName(pVm, pClass, pMeth);` |
|        - | 1818 | `		}` |
|        - | 1819 | `		/* No method answers the name: php's call TRAMPOLINE, whose function is the` |
|        - | 1820 | `		 * catch-all itself, so its scope is the class that declared __callStatic (no` |
|        - | 1821 | ``		 * receiver) or __call -- `B::miss(...)` is scoped to A when A declares it. */`` |
|      162 | 1822 | `		if( pClosure->iFlags & VM_INSTANCE_FCC_SCREENED ){` |
|      ! 0 | 1823 | `			return pClass;` |
|        - | 1824 | `		}` |
|      162 | 1825 | `		pMeth = PH7_VmClosureIsStatic(pVm, pClosure)` |
|       88 | 1826 | `			? PH7_ClassExtractMethod(pClass, "__callStatic", sizeof("__callStatic")-1)` |
|      114 | 1827 | `			: PH7_ClassExtractMethod(pClass, "__call", sizeof("__call")-1);` |
|      162 | 1828 | `		if( pMeth ){` |
|      162 | 1829 | `			return PH7_VmMethodScopeName(pVm, pClass, pMeth);` |
|        - | 1830 | `		}` |
|      ! 0 | 1831 | `	}` |
|      ! 0 | 1832 | `	return pClass;` |
|      172 | 1833 | `}` |
|        - | 1834 | `/*` |
|        - | 1835 | `` * Resolve the scope class for a STATIC first-class callable `T::m(...)`, where T is a`` |
|        - | 1836 | ` * class-name STRING value: handles the self/static/parent keywords against the live class` |
|        - | 1837 | ` * context (so the actual class is bound at FCC-creation time, like PHP), and falls back to` |
|        - | 1838 | ` * an explicit class-name lookup. Mirrors the static OP_MEMBER resolution. Returns 0 if the` |
|        - | 1839 | ` * class cannot be resolved.` |
|        - | 1840 | ` */` |
|      202 | 1841 | `PH7_PRIVATE ph7_class * VmFccResolveScope(ph7_vm *pVm, ph7_value *pTarget)` |
|        5 | 1842 | `{` |
|      409 | 1843 | `	return PH7_VmResolveScopeName(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|      202 | 1844 | `		(sxu32)SyBlobLength(&pTarget->sBlob));` |
|        5 | 1845 | `}` |
|        - | 1846 | `/*` |
|        - | 1847 | ` * The same resolution over a raw (name, length) pair, for the callable machinery: php` |
|        - | 1848 | `` * resolves `self`/`parent`/`static` in a CALLBACK (is_callable, call_user_func, array_map …)`` |
|        - | 1849 | `` * against the live class context, and refuses them in the direct `$cb()` dispatch — so this`` |
|        - | 1850 | ` * is deliberately NOT wired into the OP_CALL resolve check, which must keep answering` |
|        - | 1851 | `` * `Class "self" not found`.`` |
|        - | 1852 | ` */` |
|        - | 1853 | `/*` |
|        - | 1854 | `` * A Closure object is going: give back its hold on the function `$__fn` names.`` |
|        - | 1855 | ` * A run-time closure's per-instantiation ph7_vm_func belongs to the objects that` |
|        - | 1856 | ` * name it, and this is where the last of them lets go.` |
|        - | 1857 | ` */` |
|    23422 | 1858 | `static void VmClosureRelease(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        5 | 1859 | `{` |
|    23427 | 1860 | `	PH7_VmClosureInstanceRef(pVm, pThis, -1);` |
|    23427 | 1861 | `}` |
|   103418 | 1862 | `PH7_PRIVATE ph7_class * PH7_VmResolveScopeName(ph7_vm *pVm, const char *zCls, sxu32 nCls)` |
|        5 | 1863 | `{` |
|        - | 1864 | `	ph7_class *pClass;` |
|   103423 | 1865 | `	if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|      741 | 1866 | `		pClass = PH7_VmPeekSelfClass(&(*pVm)); /* self:: in a trait -> the USING class */` |
|   103055 | 1867 | `	}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|      225 | 1868 | `		pClass = PH7_VmPeekTopClass(&(*pVm));     /* late static binding */` |
|   102577 | 1869 | `	}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|      425 | 1870 | `		pClass = PH7_VmResolveParentClass(&(*pVm));` |
|      215 | 1871 | `	}else{` |
|   102047 | 1872 | `		pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|        - | 1873 | `	}` |
|   103423 | 1874 | `	return pClass;` |
|        5 | 1875 | `}` |
|        - | 1876 | `/*` |
|        - | 1877 | ` * Create a Closure object wrapping a callable name (+ optional bound $this object and/or` |
|        - | 1878 | `` * scope class-name, for the method/static first-class callables `$o->m(...)`/`C::m(...)`).`` |
|        - | 1879 | ` * Mirrors the Generator/Fiber "object carries its state in private attributes" pattern.` |
|        - | 1880 | ` * Returns the fresh instance holding ONE reference, which the caller's value TAKES --` |
|        - | 1881 | ` * the same handover PH7_NewClassInstance makes to OP_NEW. (It used to say the caller` |
|        - | 1882 | ` * had to add one, and every closure site did, so no Closure object ever reached zero.)` |
|        - | 1883 | ` */` |
|    24317 | 1884 | `PH7_PRIVATE ph7_class_instance * VmCreateClosure(ph7_vm *pVm, const SyString *pName,` |
|        - | 1885 | `	ph7_class_instance *pBoundThis, const SyString *pScope)` |
|        5 | 1886 | `{` |
|        - | 1887 | `	ph7_class_instance *pObj;` |
|        - | 1888 | `	ph7_value *pAttr;` |
|        - | 1889 | `	SyString sAttr;` |
|    24322 | 1890 | `	if( pVm->pClosureClass == 0 ){` |
|      ! 0 | 1891 | `		return 0;` |
|        - | 1892 | `	}` |
|    24322 | 1893 | `	pObj = PH7_NewClassInstance(&(*pVm), pVm->pClosureClass);` |
|    24322 | 1894 | `	if( pObj == 0 ){` |
|      ! 0 | 1895 | `		return 0;` |
|        - | 1896 | `	}` |
|    24322 | 1897 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    24322 | 1898 | `	pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|    24322 | 1899 | `	if( pAttr ){` |
|    24322 | 1900 | `		PH7_MemObjStringAppend(pAttr, pName->zString, pName->nByte);` |
|        - | 1901 | `		/* This object is now a holder of the function it names. For a run-time` |
|        - | 1902 | `		 * closure that is what keeps the per-instantiation copy alive, and losing` |
|        - | 1903 | `		 * it is what frees the copy. */` |
|    24322 | 1904 | `		PH7_VmClosureInstanceRef(pVm, pObj, 1);` |
|    12037 | 1905 | `	}` |
|    24322 | 1906 | `	if( pBoundThis ){` |
|      315 | 1907 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|      315 | 1908 | `		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|      315 | 1909 | `		if( pAttr ){` |
|      315 | 1910 | `			pAttr->x.pOther = pBoundThis;` |
|      315 | 1911 | `			MemObjSetType(pAttr, MEMOBJ_OBJ);` |
|      315 | 1912 | `			pBoundThis->iRef++; /* keep the bound object alive for the closure's lifetime */` |
|      155 | 1913 | `		}` |
|      155 | 1914 | `	}` |
|    24322 | 1915 | `	if( pScope && pScope->nByte ){` |
|      621 | 1916 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      621 | 1917 | `		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|      621 | 1918 | `		if( pAttr ){` |
|      621 | 1919 | `			PH7_MemObjStringAppend(pAttr, pScope->zString, pScope->nByte);` |
|      308 | 1920 | `		}` |
|      308 | 1921 | `	}` |
|    24322 | 1922 | `	if( pBoundThis \|\| (pScope && pScope->nByte) ){` |
|        - | 1923 | `		/* Mark bound/static FCC closures so VmClosureUnwrap can skip the $__this/$__scope` |
|        - | 1924 | `		 * lookups on the hot plain-closure dispatch path. */` |
|      621 | 1925 | `		pObj->iFlags \|= VM_INSTANCE_FCC_BOUND;` |
|      308 | 1926 | `	}` |
|    24322 | 1927 | `	return pObj;` |
|    12042 | 1928 | `}` |
|        - | 1929 | `/*` |
|        - | 1930 | ` * Exported wrapper around VmCreateClosure for builtin libraries outside this` |
|        - | 1931 | ` * file (ReflectionFunction::getClosure / ReflectionMethod::getClosure).` |
|        - | 1932 | ` */` |
|       32 | 1933 | `PH7_PRIVATE ph7_class_instance * PH7_VmNewClosure(ph7_vm *pVm, const SyString *pName,` |
|        - | 1934 | `	ph7_class_instance *pBoundThis, const SyString *pScope)` |
|        3 | 1935 | `{` |
|       35 | 1936 | `	return VmCreateClosure(&(*pVm), pName, pBoundThis, pScope);` |
|        3 | 1937 | `}` |
|        - | 1938 | `/*` |
|        - | 1939 | ` * Exported wrapper around the typed/readonly property store enforcement for` |
|        - | 1940 | ` * ReflectionProperty::setValue (vm_builtin_reflection.c). Same contract:` |
|        - | 1941 | ` * SXRET_OK (value possibly coerced in place), PH7_EXCEPTION, or PH7_ABORT.` |
|        - | 1942 | ` */` |
|       10 | 1943 | `PH7_PRIVATE sxi32 PH7_VmEnforcePropStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|        1 | 1944 | `{` |
|       11 | 1945 | `	return VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pValue,0);` |
|        1 | 1946 | `}` |
|        - | 1947 | `/*` |
|        - | 1948 | ` * Exported reference-table probe for ReflectionReference::fromArrayElement` |
|        - | 1949 | ` * (vm_builtin_reflection.c). Returns the number of links (frame variables +` |
|        - | 1950 | ` * array entries) attached to the slot's reference record, 0 when the slot` |
|        - | 1951 | ` * has none — an array element is a PHP reference when this is >= 2.` |
|        - | 1952 | ` */` |
|       18 | 1953 | `PH7_PRIVATE int PH7_VmSlotRefCount(ph7_vm *pVm,sxu32 nIdx)` |
|        1 | 1954 | `{` |
|       19 | 1955 | `	return (int)(PH7_VmSlotEntryCount(&(*pVm),nIdx) + PH7_VmSlotNodeCount(&(*pVm),nIdx));` |
|        1 | 1956 | `}` |
|        - | 1957 | `/*` |
|        - | 1958 | `` * First-class callable over an arbitrary callable VALUE: `($expr)(...)`.`` |
|        - | 1959 | ` * Normalize a validated callable value into a fresh Closure, reusing VmCreateClosure (the same` |
|        - | 1960 | ` * object the method/static first-class-callable paths mint, so dispatch round-trips identically` |
|        - | 1961 | ` * via VmClosureUnwrap). Handles the three remaining callable shapes a value can hold:` |
|        - | 1962 | ` *   - a function-NAME string          -> plain closure ($__fn = name)` |
|        - | 1963 | ` *   - a [target, method] array callable -> bound (object target -> $this) / static (class-name` |
|        - | 1964 | ` *     target -> scope) closure, mirroring the [obj,m]/[class,m] decode in PH7_VmIsCallable` |
|        - | 1965 | ` *   - an __invoke object               -> closure bound to the object's __invoke` |
|        - | 1966 | ` * An existing Closure returns 0 here (it is already a Closure — the caller keeps it as-is), so this` |
|        - | 1967 | ` * stays idempotent even for a direct caller. Returns the fresh instance holding ONE reference,` |
|        - | 1968 | ` * which the caller's value TAKES (see VmCreateClosure) or 0 if the value is an existing Closure / not a normalizable callable / on OOM — in` |
|        - | 1969 | ` * which case the caller leaves the value untouched (graceful degradation). This is the generic` |
|        - | 1970 | ` * "callable value -> Closure" primitive: the body is FCC-agnostic and self-contained, so the future` |
|        - | 1971 | ` * Closure::bind/fromCallable work (Increment 2) can call it directly.` |
|        - | 1972 | ` */` |
|        - | 1973 | `/*` |
|        - | 1974 | `` * A closure over a method named through a CLASS (`'C::m'`, `[C, 'm']`). With bBindCaller --`` |
|        - | 1975 | ` * Closure::fromCallable() -- php binds what zend_is_callable_ex resolves at creation, against` |
|        - | 1976 | `` * the calling frame: a non-static method keeps the caller's `$this` when that is an instance`` |
|        - | 1977 | `` * of C, as `C::m(...)` does, so the closure still runs on it once it leaves the method that`` |
|        - | 1978 | `` * made it; and whenever such a `$this` exists the called class is ITS class, which a static`` |
|        - | 1979 | `` * method sees as `static::`. The string spelling never reaches __call: php resolves its`` |
|        - | 1980 | ` * trampoline through the static-method lookup from fromCallable's own frame, which has no` |
|        - | 1981 | `` * `$this`, so a name only a catch-all answers is __callStatic there, unbound. A dynamic`` |
|        - | 1982 | `` * `$v(...)` binds nothing (bBindCaller FALSE).`` |
|        - | 1983 | ` */` |
|      190 | 1984 | `static ph7_class_instance * VmFccClassClosure(ph7_vm *pVm, ph7_class *pCls, SyString *pName,` |
|        - | 1985 | `	int bBindCaller, int bStrForm, int bForward)` |
|        4 | 1986 | `{` |
|        - | 1987 | `	ph7_class *pCalled;` |
|      194 | 1988 | `	int bDirect = PH7_VmFccMethodIsDirect(pVm, pCls, SyStringData(pName), SyStringLength(pName));` |
|      194 | 1989 | `	ph7_class_instance *pThis = bBindCaller ? PH7_VmCallerThisFor(pVm, pCls) : 0;` |
|      194 | 1990 | `	ph7_class_instance *pRecv = 0, *pFccObj;` |
|      194 | 1991 | `	if( pThis && (bDirect \|\| !bStrForm) ){` |
|       71 | 1992 | `		pRecv = PH7_VmFccClassReceiver(pVm, pCls, SyStringData(pName), SyStringLength(pName));` |
|       35 | 1993 | `	}` |
|      194 | 1994 | `	pFccObj = VmCreateClosure(pVm, pName, pRecv, &pCls->sName);` |
|      194 | 1995 | `	if( pFccObj == 0 ){` |
|      ! 0 | 1996 | `		return 0;` |
|        - | 1997 | `	}` |
|      194 | 1998 | `	pFccObj->iFlags \|= VM_INSTANCE_FCC_METHOD; /* $__fn is a METHOD name */` |
|      194 | 1999 | `	if( bDirect ){` |
|      125 | 2000 | `		pFccObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|       61 | 2001 | `	}` |
|        - | 2002 | ``	/* `self::`/`parent::` FORWARD the called class even with no `$this`: from a static`` |
|        - | 2003 | ``	 * D::st() inherited from B, `'self::s'` runs with static:: = D, where `'B::s'` spelled`` |
|        - | 2004 | `	 * out runs with B. */` |
|      194 | 2005 | `	pCalled = pThis ? pThis->pClass : (bBindCaller && bForward ? PH7_VmPeekTopClass(pVm) : 0);` |
|      194 | 2006 | `	if( pRecv == 0 && pCalled && pCalled != pCls ){` |
|        - | 2007 | `		SyString sAttr;` |
|        - | 2008 | `		ph7_value *pAttr;` |
|       45 | 2009 | `		SyStringInitFromBuf(&sAttr, "__called", 8);` |
|       45 | 2010 | `		pAttr = PH7_ClassInstanceFetchAttr(pFccObj, &sAttr);` |
|       45 | 2011 | `		if( pAttr ){` |
|       67 | 2012 | `			PH7_MemObjStringAppend(pAttr, SyStringData(&pCalled->sName),` |
|       22 | 2013 | `				SyStringLength(&pCalled->sName));` |
|       22 | 2014 | `		}` |
|       22 | 2015 | `	}` |
|      194 | 2016 | `	return pFccObj;` |
|       99 | 2017 | `}` |
|        - | 2018 | `/* Is a callable's class half one of the two keywords that forward the called class? */` |
|      190 | 2019 | `static int VmFccNameForwards(const char *zCls, sxu32 nCls)` |
|        4 | 2020 | `{` |
|      211 | 2021 | `	return (nCls == 4 && SyStrnicmp(zCls, "self", 4) == 0)` |
|      220 | 2022 | `		\|\| (nCls == 6 && SyStrnicmp(zCls, "parent", 6) == 0);` |
|        4 | 2023 | `}` |
|        - | 2024 | `/*` |
|        - | 2025 | ` * The class a callable's target half names. A NAME is resolved the way the callback` |
|        - | 2026 | `` * machinery resolves it -- `self`/`parent`/`static` against the calling frame, as`` |
|        - | 2027 | `` * PH7_VmIsCallable's gate already did -- so `'self::m'` and `['parent','m']` wrap the`` |
|        - | 2028 | `` * method they validated as. A plain class lookup found no class called `self` and left`` |
|        - | 2029 | `` * the whole string as a function name, which died `Class "self" not found` at the call.`` |
|        - | 2030 | `` * Only Closure::fromCallable() (bBindCaller) resolves them: `$cb(...)` is the direct`` |
|        - | 2031 | ` * dispatch, which php refuses the keywords in.` |
|        - | 2032 | ` */` |
|      266 | 2033 | `static ph7_class * VmFccTargetClass(ph7_vm *pVm, ph7_value *pTarget, int bBindCaller)` |
|        4 | 2034 | `{` |
|      270 | 2035 | `	if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|       96 | 2036 | `		return ((ph7_class_instance *)pTarget->x.pOther)->pClass;` |
|        - | 2037 | `	}` |
|      177 | 2038 | `	if( !bBindCaller \|\| (pTarget->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pTarget->sBlob) == 0 ){` |
|       17 | 2039 | `		return PH7_VmExtractClassFromValue(pVm, pTarget);` |
|        - | 2040 | `	}` |
|      240 | 2041 | `	return PH7_VmResolveCallableScope(pVm, (const char *)SyBlobData(&pTarget->sBlob),` |
|       79 | 2042 | `		SyBlobLength(&pTarget->sBlob));` |
|      137 | 2043 | `}` |
|      564 | 2044 | `PH7_PRIVATE ph7_class_instance * VmFccWrapValue(ph7_vm *pVm, ph7_value *pValue, int bBindCaller)` |
|        5 | 2045 | `{` |
|        - | 2046 | `	/* A Closure is already callable; never double-wrap it (this also stops the __invoke branch` |
|        - | 2047 | `	 * below from binding a closure to its OWN __invoke). The OP_LOAD_FCC caller intercepts a` |
|        - | 2048 | `	 * Closure first too, but guarding here keeps the primitive safe for a direct Increment-2 caller. */` |
|      569 | 2049 | `	if( VmValueIsClosure(pVm, pValue) ){` |
|      ! 0 | 2050 | `		return 0;` |
|        - | 2051 | `	}` |
|      569 | 2052 | `	if( !PH7_VmIsCallable(pVm, pValue, TRUE) ){` |
|       89 | 2053 | `		return 0;` |
|        - | 2054 | `	}` |
|      481 | 2055 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|        - | 2056 | `		SyString sName;` |
|      285 | 2057 | `		const char *zName = (const char *)SyBlobData(&pValue->sBlob);` |
|      285 | 2058 | `		sxu32 nName = SyBlobLength(&pValue->sBlob), nSep;` |
|        - | 2059 | ``		/* `"C::m"` is the SAME callable as `[C, 'm']`, and php mints the same`` |
|        - | 2060 | `		 * closure for it: scope C, name m. PHL kept the whole string as the` |
|        - | 2061 | `		 * function name, so the Closure ran (dispatch splits it) but described` |
|        - | 2062 | `		 * itself as nothing -- ReflectionFunction over it had no name, no scope` |
|        - | 2063 | `		 * and no parameters, and a library that reflects a callback before` |
|        - | 2064 | `		 * calling it (twig compiles every filter that way) died on the read. */` |
|     2171 | 2065 | `		for( nSep = 0 ; nSep + 1 < nName ; ++nSep ){` |
|     2003 | 2066 | `			if( zName[nSep] == ':' && zName[nSep+1] == ':' ){` |
|      116 | 2067 | `				break;` |
|        - | 2068 | `			}` |
|      948 | 2069 | `		}` |
|      285 | 2070 | `		if( nSep + 1 < nName ){` |
|        - | 2071 | `			ph7_class *pScopeCls;` |
|        - | 2072 | `			SyString sCls;` |
|      116 | 2073 | `			SyStringInitFromBuf(&sCls, zName, nSep);` |
|      116 | 2074 | `			pScopeCls = bBindCaller` |
|      102 | 2075 | `				? PH7_VmResolveCallableScope(pVm, SyStringData(&sCls), SyStringLength(&sCls))` |
|       61 | 2076 | `				: PH7_VmExtractClass(pVm, SyStringData(&sCls), SyStringLength(&sCls), FALSE, 0);` |
|      116 | 2077 | `			if( pScopeCls ){` |
|      110 | 2078 | `				SyStringInitFromBuf(&sName, zName + nSep + 2, nName - (nSep + 2));` |
|      163 | 2079 | `				return VmFccClassClosure(pVm, pScopeCls, &sName, bBindCaller, TRUE,` |
|       53 | 2080 | `					VmFccNameForwards(SyStringData(&sCls), SyStringLength(&sCls)));` |
|        - | 2081 | `			}` |
|        3 | 2082 | `		}` |
|      179 | 2083 | `		SyStringInitFromBuf(&sName, zName, nName);` |
|      179 | 2084 | `		return VmCreateClosure(pVm, &sName, 0, 0);` |
|        - | 2085 | `	}` |
|      201 | 2086 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 2087 | `		/* [target, method] — the same index-0/1 decode PH7_VmIsCallable uses to validate it` |
|        - | 2088 | `		 * (php reads the INTEGER indices, not insertion order). */` |
|      186 | 2089 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|        - | 2090 | `		ph7_value *pTarget, *pMeth;` |
|        - | 2091 | `		SyString sName;` |
|      186 | 2092 | `		if( !PH7_VmArrayCallableParts(pVm, pMap, &pTarget, &pMeth) ){` |
|      ! 0 | 2093 | `			return 0;` |
|        - | 2094 | `		}` |
|      186 | 2095 | `		if( (pMeth->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pMeth->sBlob) == 0 ){` |
|      ! 0 | 2096 | `			return 0;` |
|        - | 2097 | `		}` |
|      186 | 2098 | `		SyStringInitFromBuf(&sName, SyBlobData(&pMeth->sBlob), SyBlobLength(&pMeth->sBlob));` |
|        - | 2099 | `		{` |
|        - | 2100 | ``			/* A qualified method half (`[$b,'A::f']`) names the class the closure is scoped`` |
|        - | 2101 | `			 * to: the closure carries the method half and that class, so it runs A::f on $b` |
|        - | 2102 | ``			 * rather than looking `A::f` up as a method name of B's. */`` |
|      186 | 2103 | `			ph7_class *pOrg = VmFccTargetClass(pVm, pTarget, bBindCaller);` |
|      186 | 2104 | `			ph7_class *pQual = 0;` |
|      186 | 2105 | `			const char *zQCls = 0, *zQMeth = 0, *zWhy = 0;` |
|      186 | 2106 | `			sxu32 nQCls = 0, nQMeth = 0;` |
|        - | 2107 | `			char zWhyBuf[128];` |
|      186 | 2108 | `			int rcQual = pOrg ? PH7_VmQualifiedCallableMethod(pVm, pOrg, sName.zString, sName.nByte,` |
|      182 | 2109 | `				&pQual, &zQCls, &nQCls, &zQMeth, &nQMeth, zWhyBuf, (int)sizeof(zWhyBuf), &zWhy) : 0;` |
|      186 | 2110 | `			if( rcQual < 0 ){` |
|      ! 0 | 2111 | `				return 0;` |
|        - | 2112 | `			}` |
|      186 | 2113 | `			if( rcQual > 0 ){` |
|        - | 2114 | `				ph7_class_instance *pFccObj;` |
|       43 | 2115 | `				ph7_class_instance *pRecv = (pTarget->iFlags & MEMOBJ_OBJ)` |
|       25 | 2116 | `					? (ph7_class_instance *)pTarget->x.pOther : 0;` |
|       29 | 2117 | `				ph7_class *pCalled = 0;` |
|       29 | 2118 | `				int bDirect = PH7_VmFccMethodIsDirect(pVm, pQual, zQMeth, nQMeth);` |
|        - | 2119 | `				/* A name the qualified class does not answer directly goes, in php, through` |
|        - | 2120 | `				 * THAT class's static-method fallback, asked from fromCallable's own frame --` |
|        - | 2121 | ``				 * which has no `$this` -- so the catch-all is __callStatic even when __call`` |
|        - | 2122 | `				 * exists: an unbound static closure whose static:: is the object's class.` |
|        - | 2123 | `				 * (A class with only __call was already refused by the gate above.) */` |
|       28 | 2124 | `				if( bBindCaller && !bDirect && pRecv` |
|        9 | 2125 | `						&& PH7_ClassExtractMethod(pQual, "__callStatic", sizeof("__callStatic")-1) ){` |
|        9 | 2126 | `					pCalled = pRecv->pClass;` |
|        9 | 2127 | `					pRecv = 0;` |
|        4 | 2128 | `				}` |
|       29 | 2129 | `				SyStringInitFromBuf(&sName, zQMeth, nQMeth);` |
|       29 | 2130 | `				pFccObj = VmCreateClosure(pVm, &sName, pRecv, &pQual->sName);` |
|       29 | 2131 | `				if( pFccObj ){` |
|       29 | 2132 | `					pFccObj->iFlags \|= VM_INSTANCE_FCC_METHOD; /* $__fn is a METHOD name */` |
|       29 | 2133 | `					if( bDirect ){` |
|       21 | 2134 | `						pFccObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|       10 | 2135 | `					}` |
|       29 | 2136 | `					if( pCalled && pCalled != pQual ){` |
|        - | 2137 | `						SyString sAttr;` |
|        - | 2138 | `						ph7_value *pAttr;` |
|        9 | 2139 | `						SyStringInitFromBuf(&sAttr, "__called", 8);` |
|        9 | 2140 | `						pAttr = PH7_ClassInstanceFetchAttr(pFccObj, &sAttr);` |
|        9 | 2141 | `						if( pAttr ){` |
|       13 | 2142 | `							PH7_MemObjStringAppend(pAttr, SyStringData(&pCalled->sName),` |
|        4 | 2143 | `								SyStringLength(&pCalled->sName));` |
|        4 | 2144 | `						}` |
|        4 | 2145 | `					}` |
|       14 | 2146 | `				}` |
|       29 | 2147 | `				return pFccObj;` |
|        - | 2148 | `			}` |
|        - | 2149 | `		}` |
|      158 | 2150 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|       74 | 2151 | `			ph7_class_instance *pBoundThis = (ph7_class_instance *)pTarget->x.pOther;` |
|      109 | 2152 | `			ph7_class_instance *pFccObj = VmCreateClosure(pVm, &sName, pBoundThis,` |
|       70 | 2153 | `				&pBoundThis->pClass->sName);` |
|       74 | 2154 | `			if( pFccObj ){` |
|       74 | 2155 | `				pFccObj->iFlags \|= VM_INSTANCE_FCC_METHOD; /* $__fn is a METHOD name */` |
|      109 | 2156 | `				if( PH7_VmFccMethodIsDirect(pVm,pBoundThis->pClass,` |
|       35 | 2157 | `						SyStringData(&sName),SyStringLength(&sName)) ){` |
|        - | 2158 | `					/* The PH7_VmIsCallable gate above is php's creation-time screen; a pair it` |
|        - | 2159 | `					 * admitted because the class routes the name through __call is NOT settled. */` |
|       46 | 2160 | `					pFccObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|       21 | 2161 | `				}` |
|       35 | 2162 | `			}` |
|       74 | 2163 | `			return pFccObj;` |
|      ! 0 | 2164 | `		}else{` |
|        - | 2165 | `			/* [class-name, method] static callable -> bind the resolved scope. */` |
|       87 | 2166 | `			ph7_class *pScopeCls = VmFccTargetClass(pVm, pTarget, bBindCaller);` |
|      129 | 2167 | `			return pScopeCls ? VmFccClassClosure(pVm, pScopeCls, &sName, bBindCaller, FALSE,` |
|       84 | 2168 | `				(pTarget->iFlags & MEMOBJ_STRING) && VmFccNameForwards(` |
|      168 | 2169 | `					(const char *)SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob))) : 0;` |
|        - | 2170 | `		}` |
|        - | 2171 | `	}` |
|       17 | 2172 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|        - | 2173 | `		/* __invoke object (a real Closure is intercepted by the caller before this point).` |
|        - | 2174 | ``		 * The `__invoke` name is the ENGINE's, so mark the closure: php dispatches a`` |
|        - | 2175 | ``		 * non-public __invoke through this wrapper exactly as it does through `$obj()`. */`` |
|       17 | 2176 | `		ph7_class_instance *pObj = (ph7_class_instance *)pValue->x.pOther;` |
|        - | 2177 | `		ph7_class_instance *pWrap;` |
|        - | 2178 | `		SyString sInvoke;` |
|       17 | 2179 | `		SyStringInitFromBuf(&sInvoke, "__invoke", sizeof("__invoke") - 1);` |
|       17 | 2180 | `		pWrap = VmCreateClosure(pVm, &sInvoke, pObj, &pObj->pClass->sName);` |
|       17 | 2181 | `		if( pWrap ){` |
|       17 | 2182 | `			pWrap->iFlags \|= VM_INSTANCE_FCC_INVOKE_OBJ;` |
|        7 | 2183 | `		}` |
|       17 | 2184 | `		return pWrap;` |
|        - | 2185 | `	}` |
|        - | 2186 | `	/* Unreachable in practice — the PH7_VmIsCallable gate admits only string/array/object, all` |
|        - | 2187 | `	 * handled above; kept to satisfy the non-void return path. */` |
|      ! 0 | 2188 | `	return 0;` |
|      287 | 2189 | `}` |
|        - | 2190 | `/*` |
|        - | 2191 | ` * Return a fresh Closure instance (iRef==0 from create/clone) as a builtin result, taking the` |
|        - | 2192 | ` * one reference into pCtx->pRet — mirrors the OP_LOAD_FCC store convention.` |
|        - | 2193 | ` */` |
|      526 | 2194 | `static int VmClosureResult(ph7_context *pCtx, ph7_class_instance *pClosure)` |
|        5 | 2195 | `{` |
|      531 | 2196 | `	if( pClosure == 0 ){` |
|      ! 0 | 2197 | `		ph7_result_null(pCtx);` |
|      ! 0 | 2198 | `		return PH7_OK;` |
|        - | 2199 | `	}` |
|      531 | 2200 | `	PH7_MemObjRelease(pCtx->pRet);` |
|        - | 2201 | `	/* Every caller hands over a FRESH instance, whose own reference is the one this` |
|        - | 2202 | `	 * return value takes (see OP_LOAD_CLOSURE). */` |
|      531 | 2203 | `	pCtx->pRet->x.pOther = pClosure;` |
|      531 | 2204 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|      531 | 2205 | `	return PH7_OK;` |
|      268 | 2206 | `}` |
|        - | 2207 | `/*` |
|        - | 2208 | ` * Overwrite a Closure clone's $__this (bound object, or 0 to unbind) and, when pScope != 0, its` |
|        - | 2209 | ` * $__scope (the class-name string; pScope->nByte==0 clears it). pScope==0 leaves $__scope as the` |
|        - | 2210 | ` * clone inherited it (PHP's "static" = keep-scope). Refreshes VM_INSTANCE_FCC_BOUND from the result.` |
|        - | 2211 | ` * The clone inherited a ref on the original $__this (PH7_CloneClassInstance copies via MemObjStore);` |
|        - | 2212 | ` * this drops that and takes one on pNewThis.` |
|        - | 2213 | ` *` |
|        - | 2214 | ` * A closure EXPRESSION keeps the class it was written in on its function, not in $__scope, so the` |
|        - | 2215 | ` * first rebind that keeps the scope writes that class down: from then on an empty $__scope means` |
|        - | 2216 | `` * php's null scope (`bindTo($o, null)` runs as `Closure`, `bindTo(null, null)` as no class at all),`` |
|        - | 2217 | ` * which a keep-scope rebind left indistinguishable from the scope it kept.` |
|        - | 2218 | ` */` |
|      292 | 2219 | `static void VmClosureRebind(ph7_vm *pVm, ph7_class_instance *pClone,` |
|        - | 2220 | `	ph7_class_instance *pNewThis, const SyString *pScope)` |
|        5 | 2221 | `{` |
|        - | 2222 | `	SyString sAttr, sKeep;` |
|        - | 2223 | `	ph7_value *pThisAttr, *pScopeAttr;` |
|      297 | 2224 | `	int bBound = 0;` |
|      297 | 2225 | `	if( pScope == 0 && (pClone->iFlags & (VM_INSTANCE_FCC_REBOUND\|VM_INSTANCE_FCC_METHOD)) == 0 ){` |
|        - | 2226 | `		ph7_value *pFn;` |
|       67 | 2227 | `		SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|       67 | 2228 | `		pFn = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|       67 | 2229 | `		if( pFn && (pFn->iFlags & MEMOBJ_STRING) && SyBlobLength(&pFn->sBlob) > 0 ){` |
|       98 | 2230 | `			SyHashEntry *pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob),` |
|       31 | 2231 | `				SyBlobLength(&pFn->sBlob));` |
|       67 | 2232 | `			ph7_class *pWritten = pEntry` |
|       62 | 2233 | `				? PH7_VmClosureFuncScope(pVm, (ph7_vm_func *)pEntry->pUserData, 0, 0, 0) : 0;` |
|       67 | 2234 | `			if( pWritten ){` |
|       34 | 2235 | `				sKeep = pWritten->sName;` |
|       34 | 2236 | `				pScope = &sKeep;` |
|       16 | 2237 | `			}` |
|       31 | 2238 | `		}` |
|       31 | 2239 | `	}` |
|      297 | 2240 | `	pClone->iFlags \|= VM_INSTANCE_FCC_REBOUND;` |
|      297 | 2241 | `	SyStringInitFromBuf(&sAttr, "__this", 6);` |
|      297 | 2242 | `	pThisAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      297 | 2243 | `	if( pThisAttr ){` |
|        - | 2244 | `		/* PH7_MemObjRelease already drops the cloned-in object's refcount for an OBJ value —` |
|        - | 2245 | `		 * do NOT also decrement by hand (that double-frees the original bound object). */` |
|      297 | 2246 | `		PH7_MemObjRelease(pThisAttr);` |
|      297 | 2247 | `		if( pNewThis ){` |
|      199 | 2248 | `			pThisAttr->x.pOther = pNewThis;` |
|      199 | 2249 | `			MemObjSetType(pThisAttr, MEMOBJ_OBJ);` |
|      199 | 2250 | `			pNewThis->iRef++;` |
|       97 | 2251 | `		}` |
|      146 | 2252 | `	}` |
|      297 | 2253 | `	if( pScope ){` |
|      233 | 2254 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      233 | 2255 | `		pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      233 | 2256 | `		if( pScopeAttr ){` |
|      233 | 2257 | `			PH7_MemObjRelease(pScopeAttr);` |
|      233 | 2258 | `			if( pScope->nByte ){` |
|      213 | 2259 | `				PH7_MemObjStringAppend(pScopeAttr, pScope->zString, pScope->nByte);` |
|      104 | 2260 | `			}` |
|      114 | 2261 | `		}` |
|        - | 2262 | `		/* A new scope is a new called class too: the forwarded one no longer applies. */` |
|      233 | 2263 | `		SyStringInitFromBuf(&sAttr, "__called", 8);` |
|      233 | 2264 | `		pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      233 | 2265 | `		if( pScopeAttr ){` |
|      233 | 2266 | `			PH7_MemObjRelease(pScopeAttr);` |
|      114 | 2267 | `		}` |
|      114 | 2268 | `	}` |
|        - | 2269 | `	/* Refresh the FCC_BOUND fast-path flag from the resulting $__this/$__scope. pThisAttr already` |
|        - | 2270 | `	 * points at the final $__this slot; only $__scope needs a (re)fetch — the top half fetched it` |
|        - | 2271 | `	 * just for pScope != 0. */` |
|      297 | 2272 | `	SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      297 | 2273 | `	pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      292 | 2274 | `	if( (pThisAttr && (pThisAttr->iFlags & MEMOBJ_OBJ))` |
|      200 | 2275 | `		\|\| (pScopeAttr && (pScopeAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeAttr->sBlob) > 0) ){` |
|      275 | 2276 | `		bBound = 1;` |
|      135 | 2277 | `	}` |
|      297 | 2278 | `	if( bBound ){` |
|      275 | 2279 | `		pClone->iFlags \|= VM_INSTANCE_FCC_BOUND;` |
|      140 | 2280 | `	}else{` |
|       26 | 2281 | `		pClone->iFlags &= ~VM_INSTANCE_FCC_BOUND;` |
|        - | 2282 | `	}` |
|      297 | 2283 | `}` |
|        - | 2284 | `/*` |
|        - | 2285 | ` * Resolve the bindTo/bind/call $scope argument to a class-name SyString.` |
|        - | 2286 | ` * Returns 1 and fills *pOut if $__scope should be replaced (pOut->nByte==0 means "clear");` |
|        - | 2287 | ` * returns 0 to leave the scope unchanged (the PHP "static" sentinel / scope omitted).` |
|        - | 2288 | ` */` |
|      336 | 2289 | `static int VmClosureResolveScope(ph7_value *pScopeArg, SyString *pOut)` |
|        5 | 2290 | `{` |
|      341 | 2291 | `	if( pScopeArg == 0 ){` |
|       97 | 2292 | `		return 0; /* keep */` |
|        - | 2293 | `	}` |
|      244 | 2294 | `	if( (pScopeArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeArg->sBlob) == 6` |
|      140 | 2295 | `		&& SyMemcmp((const void *)SyBlobData(&pScopeArg->sBlob), (const void *)"static", 6) == 0 ){` |
|       42 | 2296 | `		return 0; /* "static" -> keep current scope */` |
|        - | 2297 | `	}` |
|      211 | 2298 | `	if( pScopeArg->iFlags & MEMOBJ_NULL ){` |
|       26 | 2299 | `		SyStringInitFromBuf(pOut, 0, 0); /* unscoped */` |
|       26 | 2300 | `		return 1;` |
|        - | 2301 | `	}` |
|      189 | 2302 | `	if( pScopeArg->iFlags & MEMOBJ_OBJ ){` |
|        6 | 2303 | `		ph7_class_instance *pScopeObj = (ph7_class_instance *)pScopeArg->x.pOther;` |
|        6 | 2304 | `		*pOut = pScopeObj->pClass->sName;` |
|        6 | 2305 | `		return 1;` |
|        - | 2306 | `	}` |
|      185 | 2307 | `	if( (pScopeArg->iFlags & MEMOBJ_STRING) == 0 && (pScopeArg->iFlags & MEMOBJ_SCALAR) ){` |
|        - | 2308 | ``		/* php declares `object\|string\|null $newScope` and coerces a scalar into it in weak`` |
|        - | 2309 | ``		 * mode, so `bindTo($o, 5)` reaches the lookup as the NAME "5" and warns that no such`` |
|        - | 2310 | `		 * class exists. Falling through as "keep the current scope" bound it silently. */` |
|        3 | 2311 | `		PH7_MemObjToString(pScopeArg);` |
|        1 | 2312 | `	}` |
|      185 | 2313 | `	if( pScopeArg->iFlags & MEMOBJ_STRING ){` |
|      185 | 2314 | `		SyStringInitFromBuf(pOut, (const char *)SyBlobData(&pScopeArg->sBlob), SyBlobLength(&pScopeArg->sBlob));` |
|      185 | 2315 | `		return 1;` |
|        - | 2316 | `	}` |
|      ! 0 | 2317 | `	return 0;` |
|      173 | 2318 | `}` |
|        - | 2319 | `/*` |
|        - | 2320 | ` * Fiber::getCurrent() — the fiber the running code is INSIDE, or null in the main` |
|        - | 2321 | ` * flow. php answers EG(active_fiber), which is the fiber whose body is on the` |
|        - | 2322 | ` * stack, not the innermost coroutine: a GENERATOR iterated from inside a fiber` |
|        - | 2323 | ` * leaves the fiber current, and code running after a fiber suspends back to its` |
|        - | 2324 | ` * caller is outside it again. pVm->pCurFiber is that name, saved and restored` |
|        - | 2325 | ` * around every start/resume, so the nesting is the call structure itself.` |
|        - | 2326 | ` *` |
|        - | 2327 | ` * Reached by every library that logs or schedules per-fiber: monolog asks for it` |
|        - | 2328 | ` * on EVERY record it writes.` |
|        - | 2329 | ` */` |
|       28 | 2330 | `PH7_PRIVATE int vm_builtin_Fiber_getCurrent(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        1 | 2331 | `{` |
|       29 | 2332 | `	ph7_vm *pVm = pCtx->pVm;` |
|       14 | 2333 | `	SXUNUSED(apArg);` |
|       14 | 2334 | `	SXUNUSED(nArg);` |
|       29 | 2335 | `	if( pVm->pCurFiber == 0 ){` |
|       13 | 2336 | `		ph7_result_null(pCtx);` |
|       13 | 2337 | `		return PH7_OK;` |
|        - | 2338 | `	}` |
|        - | 2339 | `	/* The result slot takes a reference of its own: pCurFiber is borrowed from the` |
|        - | 2340 | `	 * activation that set it, and the value handed back outlives that call. */` |
|       17 | 2341 | `	PH7_MemObjRelease(pCtx->pRet);` |
|       17 | 2342 | `	pVm->pCurFiber->iRef++;` |
|       17 | 2343 | `	pCtx->pRet->x.pOther = pVm->pCurFiber;` |
|       17 | 2344 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|       17 | 2345 | `	return PH7_OK;` |
|       15 | 2346 | `}` |
|        - | 2347 | `/*` |
|        - | 2348 | ``  * Fiber's C-bodied methods. Every one of them was a global `__fiber_verb($this,…)` `` |
|        - | 2349 | ` * thunk that a one-line prelude method forwarded to; the class body in the builtin` |
|        - | 2350 | ` * chunk now holds only its two private slots.` |
|        - | 2351 | ` */` |
|     8445 | 2352 | `PH7_PRIVATE sxi32 PH7_VmInstallFiberNative(ph7_vm *pVm)` |
|        5 | 2353 | `{` |
|        - | 2354 | `	/* php's own declaration ORDER, which is what get_class_methods() and` |
|        - | 2355 | `	 * ReflectionClass::getMethods() answer in. One row still differs from php's` |
|        - | 2356 | ``	 * list, and is recorded: `__destruct` is the engine's teardown hook`` |
|        - | 2357 | `	 * published as a method name php does not have (Generator's is the twin). */` |
|        - | 2358 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|        - | 2359 | `		{ "__construct",  PH7_MOD_PUBLIC, "callable $callback",  "",       vm_builtin_Fiber_construct },` |
|        - | 2360 | `		/* Variadic: the arguments now reach the C body directly instead of being` |
|        - | 2361 | `		 * repackaged by a func_get_args() call in the prelude. */` |
|        - | 2362 | `		{ "start",        PH7_MOD_PUBLIC, "mixed ...$args",      "mixed",  vm_builtin_Fiber_start },` |
|        - | 2363 | `		{ "resume",       PH7_MOD_PUBLIC, "mixed $value = null", "mixed",  vm_builtin_Fiber_resume },` |
|        - | 2364 | `		{ "throw",        PH7_MOD_PUBLIC, "Throwable $exception", "mixed", vm_builtin_Fiber_throw },` |
|        - | 2365 | `		{ "isStarted",    PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isStarted },` |
|        - | 2366 | `		{ "isSuspended",  PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isSuspended },` |
|        - | 2367 | `		{ "isRunning",    PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isRunning },` |
|        - | 2368 | `		{ "isTerminated", PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isTerminated },` |
|        - | 2369 | `		{ "getReturn",    PH7_MOD_PUBLIC, "",                    "mixed",  vm_builtin_Fiber_getReturn },` |
|        - | 2370 | `		/* Static like suspend, and the one method a program calls without holding a` |
|        - | 2371 | `		 * fiber at all — it is how code asks whether it is inside one. */` |
|        - | 2372 | `		{ "getCurrent",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "",                    "?Fiber",` |
|        - | 2373 | `		  vm_builtin_Fiber_getCurrent },` |
|        - | 2374 | `		/* Static, and the only one that never took a receiver even as a thunk:` |
|        - | 2375 | ``		 * `__fiber_suspend($value)` already read the value from argument #0. */`` |
|        - | 2376 | `		{ "suspend",      PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "mixed $value = null", "mixed",` |
|        - | 2377 | `		  vm_builtin_Fiber_suspend },` |
|        - | 2378 | `		{ "__destruct",   PH7_MOD_PUBLIC, "",                    "",       vm_builtin_Fiber_destruct },` |
|        - | 2379 | `	};` |
|        - | 2380 | `	/* The two private slots the methods above keep their state in: the execution` |
|        - | 2381 | `	 * context (a resource) and the callable handed to the constructor. */` |
|        - | 2382 | `	static const PH7_NativePropDef aProp[] = {` |
|        - | 2383 | `		{ "__ctx",      PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 2384 | `		{ "__callable", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 2385 | `	};` |
|        - | 2386 | `	static const PH7_NativeClassSpec sSpec = {` |
|        - | 2387 | `		"Fiber", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOSERIALIZE,` |
|        - | 2388 | `		aMethod, SX_ARRAYSIZE(aMethod),` |
|        - | 2389 | `		0, 0,` |
|        - | 2390 | `		aProp, SX_ARRAYSIZE(aProp),` |
|        - | 2391 | `		0, 0, 0` |
|        - | 2392 | `	};` |
|     8450 | 2393 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|        5 | 2394 | `}` |
|        - | 2395 | `/*` |
|        - | 2396 | `` * Generator's C-bodied methods, plus the `implements Iterator` the chunk can no`` |
|        - | 2397 | ` * longer carry: PH7_ClassImplement installs an abstract stub for any interface` |
|        - | 2398 | ` * method the class does not already declare, so it has to run AFTER the eight` |
|        - | 2399 | ` * methods below exist — at which point the stubs are skipped and the class is` |
|        - | 2400 | ` * concrete, exactly as the prelude declaration used to make it.` |
|        - | 2401 | ` */` |
|     8445 | 2402 | `PH7_PRIVATE sxi32 PH7_VmInstallGeneratorNative(ph7_vm *pVm)` |
|        5 | 2403 | `{` |
|        - | 2404 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|        - | 2405 | `		{ "current",    PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_current },` |
|        - | 2406 | `		{ "key",        PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_key },` |
|        - | 2407 | `		{ "next",       PH7_MOD_PUBLIC, "",                 "void",  vm_builtin_Generator_next },` |
|        - | 2408 | `		{ "rewind",     PH7_MOD_PUBLIC, "",                 "void",  vm_builtin_Generator_rewind },` |
|        - | 2409 | `		{ "valid",      PH7_MOD_PUBLIC, "",                 "bool",  vm_builtin_Generator_valid },` |
|        - | 2410 | ``		/* php REQUIRES the argument here; the prelude declared `$value = null`, so`` |
|        - | 2411 | ``		 * `$gen->send()` used to answer the first yielded value instead of raising. */`` |
|        - | 2412 | `		{ "send",       PH7_MOD_PUBLIC, "mixed $value",     "mixed", vm_builtin_Generator_send },` |
|        - | 2413 | `		{ "throw",      PH7_MOD_PUBLIC, "Throwable $exception", "mixed", vm_builtin_Generator_throw },` |
|        - | 2414 | `		{ "getReturn",  PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_getReturn },` |
|        - | 2415 | `		{ "__destruct", PH7_MOD_PUBLIC, "",                 "",      vm_builtin_Generator_destruct },` |
|        - | 2416 | `	};` |
|        - | 2417 | `	static const PH7_NativePropDef aProp[] = {` |
|        - | 2418 | `		{ "__ctx", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 2419 | `	};` |
|        - | 2420 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|        - | 2421 | `		{ "Generator", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOSERIALIZE,` |
|        - | 2422 | `		  aMethod, SX_ARRAYSIZE(aMethod),` |
|        - | 2423 | `		  0, 0,` |
|        - | 2424 | `		  aProp, SX_ARRAYSIZE(aProp),` |
|        - | 2425 | `		  0, 0, 0 },` |
|        - | 2426 | `		/* php declares this one beside Generator and throws it from nowhere a` |
|        - | 2427 | `		 * script can reach: it is the exception a RESUME of a generator that has` |
|        - | 2428 | `		 * been closed would carry, and php's own paths answer null there. A` |
|        - | 2429 | `		 * program may still name it, catch it and throw it, so it is declared. */` |
|        - | 2430 | `		{ "ClosedGeneratorException", "Exception", 0, 0,` |
|        - | 2431 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|        - | 2432 | `	};` |
|        - | 2433 | `	ph7_class *pClass;` |
|        - | 2434 | `	ph7_class *pIterator;` |
|     8450 | 2435 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|     8450 | 2436 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 2437 | `		return rc;` |
|        - | 2438 | `	}` |
|        - | 2439 | ``	/* `implements Iterator` last, for the reason in this function's header. */`` |
|     8450 | 2440 | `	pClass = PH7_VmExtractClass(&(*pVm),"Generator",sizeof("Generator")-1,0,0);` |
|     8450 | 2441 | `	if( pClass ){` |
|        - | 2442 | ``		/* php refuses `new Generator` -- one only ever comes out of a call to a`` |
|        - | 2443 | `		 * function that yields -- and words the refusal per class. */` |
|     8450 | 2444 | `		pClass->zNewRefusal = "The \"Generator\" class is reserved for internal use "` |
|        - | 2445 | `			"and cannot be manually instantiated";` |
|     4217 | 2446 | `	}` |
|     8450 | 2447 | `	pIterator = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,0,0);` |
|     8450 | 2448 | `	if( pClass == 0 \|\| pIterator == 0 ){` |
|      ! 0 | 2449 | `		return SXERR_NOTFOUND;` |
|        - | 2450 | `	}` |
|     8450 | 2451 | `	return PH7_ClassImplement(pClass,pIterator);` |
|     4222 | 2452 | `}` |
|        - | 2453 | `/*` |
|        - | 2454 | ` * Closure::__construct() — php declares it PRIVATE and still words the refusal as` |
|        - | 2455 | ` * an instantiation error rather than a visibility one, so the body has to exist.` |
|        - | 2456 | ` */` |
|      ! 0 | 2457 | `PH7_PRIVATE int vm_builtin_Closure_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      ! 0 | 2458 | `{` |
|      ! 0 | 2459 | `	SXUNUSED(nArg);` |
|      ! 0 | 2460 | `	SXUNUSED(apArg);` |
|      ! 0 | 2461 | `	return PH7_VmThrowException(pCtx, "Error",` |
|        - | 2462 | `		"Instantiation of class Closure is not allowed");` |
|      ! 0 | 2463 | `}` |
|        - | 2464 | `/*` |
|        - | 2465 | ` * Clone a Closure for a rebind, carrying the marks that say WHAT it wraps. They live on the` |
|        - | 2466 | ` * instance rather than in an attribute, so PH7_CloneClassInstance — which copies attributes —` |
|        - | 2467 | `` * left them behind: the clone of a method callable forgot that its `$__fn` names a screened`` |
|        - | 2468 | `` * METHOD, and `$fcc->bindTo($other)` came back as a closure whose every dispatch re-resolved`` |
|        - | 2469 | ` * the name and re-decided its visibility (or, with no method of that name in reach, looked` |
|        - | 2470 | ` * for a global FUNCTION). A rebind's clone stays one: its empty $__scope is php's null scope,` |
|        - | 2471 | ` * and only the FIRST rebind writes down the scope it kept (VmClosureRebind).` |
|        - | 2472 | ` */` |
|      292 | 2473 | `static ph7_class_instance * VmCloneClosureInstance(ph7_class_instance *pClosure)` |
|        5 | 2474 | `{` |
|      297 | 2475 | `	ph7_class_instance *pClone = PH7_CloneClassInstance(pClosure);` |
|      297 | 2476 | `	if( pClone ){` |
|      443 | 2477 | `		pClone->iFlags \|= pClosure->iFlags` |
|      292 | 2478 | `			& (VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SCREENED\|VM_INSTANCE_FCC_INVOKE_OBJ` |
|        - | 2479 | `			   \|VM_INSTANCE_FCC_SYNTAX\|VM_INSTANCE_FCC_REBOUND);` |
|      146 | 2480 | `	}` |
|      297 | 2481 | `	return pClone;` |
|        5 | 2482 | `}` |
|        - | 2483 | `/*` |
|        - | 2484 | `` * Is this closure STATIC — one that can never take a `$this`? For a plain closure that is the`` |
|        - | 2485 | `` * `static function(){}` declaration flag; for a METHOD callable it is the method's own`` |
|        - | 2486 | `` * staticness, which the flag cannot see because `$__fn` names a method and not a function in`` |
|        - | 2487 | `` * hFunction. `Base::stat(...)` is exactly as static as `static fn()` to php.`` |
|        - | 2488 | ` *` |
|        - | 2489 | ` * A method callable that resolved to the class's catch-all rather than a method (no SCREENED` |
|        - | 2490 | `` * mark) is php's call TRAMPOLINE, and php makes the `__callStatic` one static: the name it`` |
|        - | 2491 | ` * wraps is not a method to ask, and the only thing that decided between the two catch-alls` |
|        - | 2492 | `` * was whether a receiver was bound. So `A::missing(...)` from outside A is static, and`` |
|        - | 2493 | `` * `Closure::bind($it, null, A::class)` keeps it where the method rule refused the unbind.`` |
|        - | 2494 | ` */` |
|      488 | 2495 | `PH7_PRIVATE int PH7_VmClosureIsStatic(ph7_vm *pVm, ph7_class_instance *pClosure)` |
|        5 | 2496 | `{` |
|        - | 2497 | `	SyString sAttr;` |
|        - | 2498 | `	ph7_value *pFn;` |
|      493 | 2499 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|      493 | 2500 | `	pFn = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|      493 | 2501 | `	if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|      ! 0 | 2502 | `		return 0;` |
|        - | 2503 | `	}` |
|      493 | 2504 | `	if( (pClosure->iFlags & (VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SCREENED)) == VM_INSTANCE_FCC_METHOD ){` |
|        - | 2505 | `		ph7_value *pThis;` |
|      295 | 2506 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|      295 | 2507 | `		pThis = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|      295 | 2508 | `		return (pThis == 0 \|\| (pThis->iFlags & MEMOBJ_OBJ) == 0) ? 1 : 0;` |
|        - | 2509 | `	}` |
|      203 | 2510 | `	if( pClosure->iFlags & VM_INSTANCE_FCC_METHOD ){` |
|       38 | 2511 | `		ph7_class *pScope = PH7_VmClosureScopeClass(pVm, pClosure);` |
|       38 | 2512 | `		ph7_class_method *pMeth = pScope` |
|       54 | 2513 | `			? PH7_ClassExtractMethod(pScope, (const char *)SyBlobData(&pFn->sBlob),` |
|       36 | 2514 | `				SyBlobLength(&pFn->sBlob)) : 0;` |
|       38 | 2515 | `		return (pMeth && (pMeth->iFlags & PH7_CLASS_ATTR_STATIC)) ? 1 : 0;` |
|        - | 2516 | `	}` |
|        - | 2517 | `	{` |
|      248 | 2518 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob),` |
|       81 | 2519 | `			SyBlobLength(&pFn->sBlob));` |
|      167 | 2520 | `		return (pEntry && (((ph7_vm_func *)pEntry->pUserData)->iFlags & VM_FUNC_STATIC_CL)) ? 1 : 0;` |
|        - | 2521 | `	}` |
|      249 | 2522 | `}` |
|        - | 2523 | `/*` |
|        - | 2524 | `` * Does this plain closure HAVE a `$this` and a body that names it? php refuses to unbind`` |
|        - | 2525 | `` * exactly that pair; a closure that holds a `$this` it never reads drops it silently. The`` |
|        - | 2526 | `` * receiver is in one of two places: `$__this` once a rebind handed it one, else the capture`` |
|        - | 2527 | ` * OP_LOAD_CLOSURE took from the method that created it.` |
|        - | 2528 | ` */` |
|       96 | 2529 | `static int VmClosureUsesBoundThis(ph7_vm *pVm, ph7_class_instance *pClosure)` |
|        5 | 2530 | `{` |
|        - | 2531 | `	SyString sAttr;` |
|        - | 2532 | `	ph7_value *pVal;` |
|        - | 2533 | `	SyHashEntry *pEntry;` |
|        - | 2534 | `	ph7_vm_func *pFunc;` |
|        - | 2535 | `	ph7_vm_func_closure_env *aEnv;` |
|        - | 2536 | `	sxu32 n;` |
|      101 | 2537 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|      101 | 2538 | `	pVal = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|      101 | 2539 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pVal->sBlob) == 0 ){` |
|      ! 0 | 2540 | `		return 0;` |
|        - | 2541 | `	}` |
|      101 | 2542 | `	pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pVal->sBlob), SyBlobLength(&pVal->sBlob));` |
|      101 | 2543 | `	if( pEntry == 0 ){` |
|      ! 0 | 2544 | `		return 0;` |
|        - | 2545 | `	}` |
|      101 | 2546 | `	pFunc = (ph7_vm_func *)pEntry->pUserData;` |
|      101 | 2547 | `	if( (pFunc->iFlags & VM_FUNC_USES_THIS) == 0 ){` |
|       75 | 2548 | `		return 0;` |
|        - | 2549 | `	}` |
|       27 | 2550 | `	SyStringInitFromBuf(&sAttr, "__this", 6);` |
|       27 | 2551 | `	pVal = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|       27 | 2552 | `	if( pVal && (pVal->iFlags & MEMOBJ_OBJ) ){` |
|        3 | 2553 | `		return 1;` |
|        - | 2554 | `	}` |
|       25 | 2555 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|       25 | 2556 | `	for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; n++ ){` |
|       24 | 2557 | `		if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|       25 | 2558 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),"this",sizeof("this")-1) == 0 ){` |
|       25 | 2559 | `			return (aEnv[n].sValue.iFlags & MEMOBJ_OBJ) ? 1 : 0;` |
|        - | 2560 | `		}` |
|      ! 0 | 2561 | `	}` |
|      ! 0 | 2562 | `	return 0;` |
|       53 | 2563 | `}` |
|        - | 2564 | `/*` |
|        - | 2565 | ` * php's four refusals for a rebind (zend_valid_closure_binding), in php's order. A closure` |
|        - | 2566 | `` * created from a METHOD is a "fake closure": it wraps a resolved function, so its `$this` may`` |
|        - | 2567 | ` * only move WITHIN the class that declared it and its scope may not move at all. PHL applied` |
|        - | 2568 | `` * none of them beyond the static-closure one, so `$fcc->bindTo($unrelated)` handed back a`` |
|        - | 2569 | `` * closure that could only fail later, and `->bindTo(null)` one with no receiver for a method`` |
|        - | 2570 | ` * that needs one. Each refusal is php's E_WARNING plus a NULL result; returns 0 when it fired.` |
|        - | 2571 | ` *` |
|        - | 2572 | `` * pScope is the resolved $scope ARGUMENT (0 when omitted or `"static"`, which both mean keep);`` |
|        - | 2573 | `` * an empty one is an explicit `null`, which php counts as a rebind like any other.`` |
|        - | 2574 | ` */` |
|      378 | 2575 | `static int VmClosureBindAllowed(ph7_vm *pVm, ph7_class_instance *pClosure,` |
|        - | 2576 | `	ph7_class_instance *pNewThis, const SyString *pScope)` |
|        5 | 2577 | `{` |
|      383 | 2578 | `	int bMethod = (pClosure->iFlags & VM_INSTANCE_FCC_METHOD) != 0;` |
|      383 | 2579 | `	ph7_class *pOwn = bMethod ? PH7_VmClosureScopeClass(pVm, pClosure) : 0;` |
|      383 | 2580 | `	if( pNewThis ){` |
|      227 | 2581 | `		if( PH7_VmClosureIsStatic(pVm, pClosure) ){` |
|       13 | 2582 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - | 2583 | `				"Cannot bind an instance to a static closure, this will be an error in PHP 9");` |
|       13 | 2584 | `			return 0;` |
|        - | 2585 | `		}` |
|      217 | 2586 | `		if( pOwn && !PH7_VmInstanceOf(pNewThis->pClass, pOwn) ){` |
|        - | 2587 | `			SyString sFn;` |
|        - | 2588 | `			ph7_value *pFn;` |
|        5 | 2589 | `			SyStringInitFromBuf(&sFn, "__fn", 4);` |
|        5 | 2590 | `			pFn = PH7_ClassInstanceFetchAttr(pClosure, &sFn);` |
|        5 | 2591 | `			SyStringInitFromBuf(&sFn, pFn ? (const char *)SyBlobData(&pFn->sBlob) : "",` |
|        - | 2592 | `				pFn ? SyBlobLength(&pFn->sBlob) : 0);` |
|        7 | 2593 | `			VmErrorFormat(pVm,PH7_CTX_WARNING,` |
|        - | 2594 | `				"Cannot bind method %z::%z() to object of class %z, this will be an error in PHP 9",` |
|        4 | 2595 | `				&pOwn->sDisp,&sFn,&pNewThis->pClass->sDisp);` |
|        5 | 2596 | `			return 0;` |
|        5 | 2597 | `		}` |
|      265 | 2598 | `	}else if( pOwn && !PH7_VmClosureIsStatic(pVm, pClosure) ){` |
|       24 | 2599 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - | 2600 | `			"Cannot unbind $this of method, this will be an error in PHP 9");` |
|       24 | 2601 | `		return 0;` |
|      139 | 2602 | `	}else if( !bMethod && VmClosureUsesBoundThis(pVm, pClosure) ){` |
|       23 | 2603 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - | 2604 | `			"Cannot unbind $this of closure using $this, this will be an error in PHP 9");` |
|       23 | 2605 | `		return 0;` |
|        - | 2606 | `	}` |
|      325 | 2607 | `	if( pScope && bMethod ){` |
|       97 | 2608 | `		ph7_class *pWant = pScope->nByte` |
|       61 | 2609 | `			? PH7_VmExtractClass(pVm, pScope->zString, pScope->nByte, FALSE, 0) : 0;` |
|       66 | 2610 | `		if( pWant != pOwn ){` |
|       31 | 2611 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|        - | 2612 | `				"Cannot rebind scope of closure created from method, this will be an error in PHP 9");` |
|       31 | 2613 | `			return 0;` |
|        - | 2614 | `		}` |
|       17 | 2615 | `	}` |
|      297 | 2616 | `	return 1;` |
|      194 | 2617 | `}` |
|        - | 2618 | `/*` |
|        - | 2619 | `` * Run the closure pFunc for one of a Closure's own call doors (`__invoke`, `call`),`` |
|        - | 2620 | ` * handing on the door's call-site names from argument nSkip on. php reaches the body` |
|        - | 2621 | ` * from an internal function (zend_call_function), so a strict_types caller's mode does` |
|        - | 2622 | ` * not follow the arguments in -- the binding is WEAK, as for every internal callback --` |
|        - | 2623 | `` * but each `name:` does: `$c->__invoke(1, x: 2)` keys a variadic `x` and binds a named`` |
|        - | 2624 | `` * formal exactly as `$c(1, x: 2)` would.`` |
|        - | 2625 | ` */` |
|      102 | 2626 | `static sxi32 VmClosureDoorCall(ph7_context *pCtx, ph7_value *pFunc, int nArg, ph7_value **apArg,` |
|        - | 2627 | `	sxu32 nSkip)` |
|        4 | 2628 | `{` |
|      106 | 2629 | `	ph7_vm *pVm = pCtx->pVm;` |
|      106 | 2630 | `	VmCallArgMap *pOuter = pCtx->pArgMap;` |
|      106 | 2631 | `	VmCallArgMap sMap, *pMap = 0;` |
|        - | 2632 | `	sxi32 rc;` |
|      106 | 2633 | `	if( pOuter && pOuter->bHasNamed && pOuter->nTotal > nSkip ){` |
|       47 | 2634 | `		SyZero(&sMap,sizeof(sMap));` |
|       47 | 2635 | `		sMap.bHasNamed = 1;` |
|       47 | 2636 | `		sMap.bFromUnpack = pOuter->bFromUnpack;` |
|       47 | 2637 | `		sMap.nTotal = pOuter->nTotal - nSkip;` |
|       47 | 2638 | `		sMap.aNames = &pOuter->aNames[nSkip];` |
|       47 | 2639 | `		sMap.aRun = pOuter->aRun ? &pOuter->aRun[nSkip] : 0;` |
|       47 | 2640 | `		pMap = &sMap;` |
|       23 | 2641 | `	}` |
|      106 | 2642 | `	pVm->bCallbackWeak = 1;` |
|      106 | 2643 | `	rc = PH7_VmCallUserFunctionWithMap(pVm, pFunc, nArg, apArg, pCtx->pRet, pMap);` |
|      106 | 2644 | `	pVm->bCallbackWeak = 0; /* clear if the dispatch never reached an OP_CALL */` |
|      106 | 2645 | `	return rc;` |
|        4 | 2646 | `}` |
|        - | 2647 | `/*` |
|        - | 2648 | ` * Closure::call(object $newThis, mixed ...$args) — bind and invoke in one step.` |
|        - | 2649 | ` *` |
|        - | 2650 | `` * This was the last PHP left in the class: `$bound = $this->bindTo($newThis,`` |
|        - | 2651 | `` * get_class($newThis)); return $bound(...$args);`. That spelling leaked its own`` |
|        - | 2652 | `` * internals — a non-object argument reported `get_class(): Argument #1 ($object)`` |
|        - | 2653 | `` * must be of type object, string given` where php names THIS method's parameter,`` |
|        - | 2654 | `` * which is what declaring `object $newThis` buys (the shared screen words it).`` |
|        - | 2655 | ` * The scope php binds is the new $this's class, exactly as the PHP did.` |
|        - | 2656 | ` */` |
|       60 | 2657 | `PH7_PRIVATE int vm_builtin_Closure_call(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        4 | 2658 | `{` |
|       64 | 2659 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 2660 | `	ph7_class_instance *pClosure, *pNewThis, *pClone;` |
|       64 | 2661 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|        - | 2662 | `	ph7_value sBound;` |
|        - | 2663 | `	SyString sScope;` |
|        - | 2664 | `	sxi32 rc;` |
|       64 | 2665 | `	if( nArg < 1 ){` |
|      ! 0 | 2666 | `		return PH7_VmThrowException(pCtx, "ArgumentCountError",` |
|        - | 2667 | `			"Closure::call() expects at least 1 argument, 0 given");` |
|        - | 2668 | `	}` |
|       64 | 2669 | `	if( pRecv == 0 \|\| !VmValueIsClosure(pVm, pRecv) ){` |
|      ! 0 | 2670 | `		ph7_result_null(pCtx);` |
|      ! 0 | 2671 | `		return PH7_OK;` |
|        - | 2672 | `	}` |
|       64 | 2673 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 \|\| apArg[0]->x.pOther == 0 ){` |
|        - | 2674 | ``		/* Unreachable while the declared `object $newThis` is screened; kept because`` |
|        - | 2675 | `		 * rule 44's family says a screen written for one body shape has not` |
|        - | 2676 | `		 * necessarily run for this one. */` |
|      ! 0 | 2677 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|        - | 2678 | `			"Closure::call(): Argument #1 ($newThis) must be of type object");` |
|        - | 2679 | `	}` |
|       64 | 2680 | `	pClosure = (ph7_class_instance *)pRecv->x.pOther;` |
|       64 | 2681 | `	pNewThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       64 | 2682 | `	SyStringInitFromBuf(&sScope, pNewThis->pClass->sName.zString, pNewThis->pClass->sName.nByte);` |
|        - | 2683 | `	/* call() BINDS before it invokes, so php's rebind refusals apply to it — and because the` |
|        - | 2684 | `	 * scope it asks for is the new $this's class, a method callable handed an instance of` |
|        - | 2685 | `	 * anything but its own declaring class is refused for the scope, not the receiver. */` |
|       64 | 2686 | `	if( !VmClosureBindAllowed(pVm, pClosure, pNewThis, &sScope) ){` |
|        3 | 2687 | `		ph7_result_null(pCtx);` |
|        3 | 2688 | `		return PH7_OK;` |
|        - | 2689 | `	}` |
|       62 | 2690 | `	pClone = VmCloneClosureInstance(pClosure);` |
|       62 | 2691 | `	if( pClone == 0 ){` |
|      ! 0 | 2692 | `		ph7_result_null(pCtx);` |
|      ! 0 | 2693 | `		return PH7_OK;` |
|        - | 2694 | `	}` |
|       62 | 2695 | `	VmClosureRebind(pVm, pClone, pNewThis, &sScope);` |
|        - | 2696 | `	/* The bound closure is handed to the dispatcher through a STACK carrier that` |
|        - | 2697 | `	 * takes its own reference (rule 16): a context value would be released with the` |
|        - | 2698 | `	 * call context and unref the instance a second time. */` |
|       62 | 2699 | `	PH7_MemObjInit(pVm, &sBound);` |
|       62 | 2700 | `	sBound.x.pOther = pClone;` |
|       62 | 2701 | `	MemObjSetType(&sBound, MEMOBJ_OBJ);` |
|        - | 2702 | ``	/* call()'s own `...$args` are by value, so php warns for every by-reference formal`` |
|        - | 2703 | `	 * and binds a copy; named off the BOUND closure, whose class is the new $this's. */` |
|       91 | 2704 | `	rc = PH7_VmByRefArgsGivenValue(pVm, 0, 0, &sBound, nArg - 1, apArg + 1, 0,` |
|       58 | 2705 | `		(pCtx->pArgMap && pCtx->pArgMap->bHasNamed && pCtx->pArgMap->nTotal > 1)` |
|       26 | 2706 | `			? &pCtx->pArgMap->aNames[1] : 0);` |
|       62 | 2707 | `	if( rc != SXRET_OK ){` |
|        3 | 2708 | `		PH7_MemObjRelease(&sBound);` |
|        3 | 2709 | `		return rc;` |
|        - | 2710 | `	}` |
|        - | 2711 | `	/* The clone's own reference is this carrier's, and releasing the carrier below` |
|        - | 2712 | `	 * is what ends the temporary (see OP_LOAD_CLOSURE). */` |
|       60 | 2713 | `	rc = VmClosureDoorCall(pCtx, &sBound, nArg - 1, apArg + 1, 1);` |
|       60 | 2714 | `	PH7_MemObjRelease(&sBound);` |
|       60 | 2715 | `	return rc;` |
|       34 | 2716 | `}` |
|        - | 2717 | `/*` |
|        - | 2718 | ` * Closure — declared entirely from C.` |
|        - | 2719 | ` *` |
|        - | 2720 | ` * Its three methods were the first in the engine whose body is a C routine rather` |
|        - | 2721 | `` * than bytecode (VM_FUNC_NATIVE), retiring the global `__closure_bindTo` /`` |
|        - | 2722 | `` * `__closure_fromCallable` thunks a prelude method used to forward to. The`` |
|        - | 2723 | ` * DECLARATION stayed in the builtin chunk until now, which cost three things: the` |
|        - | 2724 | `` * engine slots `$__fn`/`$__this`/`$__scope` were on every presentation surface`` |
|        - | 2725 | `` * (php's Closure has NO properties), `__construct` was public where php's is`` |
|        - | 2726 | `` * private, and `call()` was PHP that leaked `get_class()`'s diagnostic.`` |
|        - | 2727 | ` */` |
|     8445 | 2728 | `PH7_PRIVATE sxi32 PH7_VmInstallClosureNative(ph7_vm *pVm)` |
|        5 | 2729 | `{` |
|        - | 2730 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|        - | 2731 | `		/* Parameter names are php's own ($newScope, not $scope): this string is the` |
|        - | 2732 | `		 * declaration of record for arity, by-ref positions and the reported` |
|        - | 2733 | `		 * parameter list. */` |
|        - | 2734 | `		{ "__construct",  PH7_MOD_PRIVATE, "", 0,` |
|        - | 2735 | `		  vm_builtin_Closure_construct },` |
|        - | 2736 | `		/* php's own registration order, which is what get_class_methods() and` |
|        - | 2737 | ``		 * ReflectionClass::getMethods() answer in: `bind` before `bindTo`. */`` |
|        - | 2738 | `		{ "bind",         PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|        - | 2739 | `		  "Closure $closure, ?object $newThis, object\|string\|null $newScope = \"static\"", "?Closure",` |
|        - | 2740 | `		  vm_builtin_Closure_bindTo },` |
|        - | 2741 | `		{ "bindTo",       PH7_MOD_PUBLIC,` |
|        - | 2742 | `		  "?object $newThis, object\|string\|null $newScope = \"static\"", "?Closure",` |
|        - | 2743 | `		  vm_builtin_Closure_bindTo },` |
|        - | 2744 | `		{ "call",         PH7_MOD_PUBLIC,` |
|        - | 2745 | `		  "object $newThis, mixed ...$args", "mixed",` |
|        - | 2746 | `		  vm_builtin_Closure_call },` |
|        - | 2747 | `		{ "fromCallable", PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|        - | 2748 | `		  "callable $callback", "Closure",` |
|        - | 2749 | `		  vm_builtin_Closure_fromCallable },` |
|        - | 2750 | ``		/* A closure IS its own `__invoke`, and php says so: `$c->__invoke($x)` calls`` |
|        - | 2751 | ``		 * it, `method_exists($c,'__invoke')` is true, and reflecting it hands back`` |
|        - | 2752 | `		 * THE CLOSURE's parameter list. php does not keep it in the class's function` |
|        - | 2753 | `		 * table -- it fabricates one per closure -- which is why it is absent from` |
|        - | 2754 | `		 * get_class_methods() and from the class export, and why` |
|        - | 2755 | ``		 * `new ReflectionMethod('Closure','__invoke')` refuses while`` |
|        - | 2756 | ``		 * `(new ReflectionObject($c))->getMethod('__invoke')` answers. That whole`` |
|        - | 2757 | `		 * shape is PH7_MOD_FABRICATED. Declared with NO signature: the reflector` |
|        - | 2758 | `		 * built from a class NAME has nothing to describe (php reports zero` |
|        - | 2759 | `		 * parameters and no return type for it), and the one built from an OBJECT` |
|        - | 2760 | `		 * describes the closure instead. */` |
|        - | 2761 | `		{ "__invoke",     PH7_MOD_PUBLIC\|PH7_MOD_FABRICATED, 0, 0,` |
|        - | 2762 | `		  vm_builtin_Closure_invoke },` |
|        - | 2763 | `	};` |
|        - | 2764 | `	/* The engine's own slots: the callable NAME, the bound receiver and the bound` |
|        - | 2765 | `	 * scope. php presents no property at all for a Closure, so all three carry` |
|        - | 2766 | ``	 * PH7_MOD_HIDDEN — they keep working for `new`, `clone` and the C bodies (and`` |
|        - | 2767 | `	 * for serialize(), which this class refuses anyway) and disappear from` |
|        - | 2768 | `	 * var_dump/print_r/(array)/get_object_vars/foreach/json_encode and Reflection. */` |
|        - | 2769 | `	static const PH7_NativePropDef aProp[] = {` |
|        - | 2770 | `		{ "__fn",    PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 2771 | `		{ "__this",  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 2772 | `		{ "__scope", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 2773 | `		{ "__called", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|        - | 2774 | `	};` |
|        - | 2775 | `	/* php's get_debug_info for a Closure shows a SHAPE none of those three slots` |
|        - | 2776 | `	 * is (name/file/line or function, static, this, parameter) — see` |
|        - | 2777 | `	 * PH7_ClosurePresent, which lives beside the reflection machinery that already` |
|        - | 2778 | `	 * describes any callable's parameters. */` |
|        - | 2779 | `	static const PH7_NativeClassSpec sSpec = {` |
|        - | 2780 | `		"Closure", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOINSTANTIATE,` |
|        - | 2781 | `		aMethod, SX_ARRAYSIZE(aMethod),` |
|        - | 2782 | `		0, 0,` |
|        - | 2783 | `		aProp, SX_ARRAYSIZE(aProp),` |
|        - | 2784 | `		VmClosureRelease, 0, PH7_ClosurePresent` |
|        - | 2785 | `	};` |
|     8450 | 2786 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|        5 | 2787 | `}` |
|        - | 2788 | `/*` |
|        - | 2789 | ` * Closure::bindTo($newThis, $scope='static') / Closure::bind($closure, $newThis, $scope='static').` |
|        - | 2790 | ` * Clone the receiver and rebind $this/$scope; returns the new Closure (NULL on a non-Closure` |
|        - | 2791 | ` * receiver, matching PHP's failure mode).` |
|        - | 2792 | ` */` |
|      336 | 2793 | `PH7_PRIVATE int vm_builtin_Closure_bindTo(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 2794 | `{` |
|      341 | 2795 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 2796 | `	ph7_class_instance *pClosure, *pNewThis, *pClone;` |
|        - | 2797 | `	ph7_value *pNewThisArg;` |
|        - | 2798 | `	ph7_value *pRecv;` |
|        - | 2799 | `	SyString sScope;` |
|      341 | 2800 | `	const SyString *pScopePtr = 0;` |
|        - | 2801 | `	/* One body, both spellings — as it always was, except the closure now arrives` |
|        - | 2802 | `	 * the way php passes it rather than as a hand-written first argument. Called as` |
|        - | 2803 | `	 * the instance method bindTo(), the receiver IS the closure and the arguments` |
|        - | 2804 | `	 * start at $newThis; called as the static bind(), the closure is argument #1.` |
|        - | 2805 | `	 * Normalizing here is what lets the two share an implementation. */` |
|      341 | 2806 | `	if( PH7_ContextThis(pCtx) ){` |
|      159 | 2807 | `		pRecv = PH7_ContextThisValue(pCtx);` |
|       82 | 2808 | `	}else{` |
|      187 | 2809 | `		if( nArg < 1 ){` |
|      ! 0 | 2810 | `			ph7_result_null(pCtx);` |
|      ! 0 | 2811 | `			return PH7_OK;` |
|        - | 2812 | `		}` |
|      187 | 2813 | `		pRecv = apArg[0];` |
|      187 | 2814 | `		apArg++;` |
|      187 | 2815 | `		nArg--;` |
|        - | 2816 | `	}` |
|      341 | 2817 | `	if( nArg < 1 \|\| !VmValueIsClosure(pVm, pRecv) ){` |
|      ! 0 | 2818 | `		ph7_result_null(pCtx);` |
|      ! 0 | 2819 | `		return PH7_OK;` |
|        - | 2820 | `	}` |
|      341 | 2821 | `	pClosure = (ph7_class_instance *)pRecv->x.pOther;` |
|      341 | 2822 | `	pNewThisArg = apArg[0];` |
|      341 | 2823 | `	if( pNewThisArg->iFlags & MEMOBJ_NULL ){` |
|      167 | 2824 | `		pNewThis = 0;` |
|      260 | 2825 | `	}else if( pNewThisArg->iFlags & MEMOBJ_OBJ ){` |
|      179 | 2826 | `		pNewThis = (ph7_class_instance *)pNewThisArg->x.pOther;` |
|       92 | 2827 | `	}else{` |
|      ! 0 | 2828 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|        - | 2829 | `			"Closure::bindTo(): Argument #1 ($newThis) must be of type ?object");` |
|        - | 2830 | `	}` |
|      341 | 2831 | `	if( VmClosureResolveScope((nArg > 1) ? apArg[1] : 0, &sScope) ){` |
|      211 | 2832 | `		pScopePtr = &sScope;` |
|      103 | 2833 | `	}` |
|        - | 2834 | `	/* php RESOLVES the scope argument before it decides anything else, and a name no class` |
|        - | 2835 | `	 * answers to is its own warning — ahead of all four rebind refusals, for a plain closure` |
|        - | 2836 | `	 * as much as for a method one. PHL bound the unresolvable scope in silence and then had` |
|        - | 2837 | ``	 * no scope at all, so `bindTo($o, 'Typo')` produced a closure that could not reach the`` |
|        - | 2838 | `	 * private members it was being bound for.` |
|        - | 2839 | `	 *` |
|        - | 2840 | ``	 * The lookup is a plain class-name one: `'self'` and `'parent'` are names no class`` |
|        - | 2841 | ``	 * can have, so php warns for them too (only the exact `'static'` is special, and`` |
|        - | 2842 | `	 * VmClosureResolveScope has already kept the scope for it). */` |
|      336 | 2843 | `	if( pScopePtr && pScopePtr->nByte` |
|      200 | 2844 | `	 && PH7_VmExtractClass(pVm, pScopePtr->zString, pScopePtr->nByte, FALSE, 0) == 0 ){` |
|       20 | 2845 | `		VmErrorFormat(pVm,PH7_CTX_WARNING,"Class \"%z\" not found",pScopePtr);` |
|       20 | 2846 | `		ph7_result_null(pCtx);` |
|       20 | 2847 | `		return PH7_OK;` |
|        - | 2848 | `	}` |
|      323 | 2849 | `	if( !VmClosureBindAllowed(pVm, pClosure, pNewThis, pScopePtr) ){` |
|       89 | 2850 | `		ph7_result_null(pCtx);` |
|       89 | 2851 | `		return PH7_OK;` |
|        - | 2852 | `	}` |
|      239 | 2853 | `	pClone = VmCloneClosureInstance(pClosure);` |
|      239 | 2854 | `	if( pClone == 0 ){` |
|      ! 0 | 2855 | `		ph7_result_null(pCtx);` |
|      ! 0 | 2856 | `		return PH7_OK;` |
|        - | 2857 | `	}` |
|      239 | 2858 | `	VmClosureRebind(pVm, pClone, pNewThis, pScopePtr);` |
|      239 | 2859 | `	return VmClosureResult(pCtx, pClone);` |
|      173 | 2860 | `}` |
|        - | 2861 | `/*` |
|        - | 2862 | ` * Closure::__invoke(...$args) — call the closure the receiver IS.` |
|        - | 2863 | ` *` |
|        - | 2864 | ` * php reaches the closure's own body through this name; here the receiver is a` |
|        - | 2865 | ` * Closure OBJECT and every call door already knows how to invoke one, so the whole` |
|        - | 2866 | ` * body is "call myself with what I was given". Argument binding, by-reference` |
|        - | 2867 | ` * parameters and the declared-type screens are the callee's own, exactly as they` |
|        - | 2868 | `` * are for `$c(...)`.`` |
|        - | 2869 | ` */` |
|       46 | 2870 | `PH7_PRIVATE int vm_builtin_Closure_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 2871 | `{` |
|       48 | 2872 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       48 | 2873 | `	if( pRecv == 0 \|\| (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 2874 | `		return PH7_OK;` |
|        - | 2875 | `	}` |
|       48 | 2876 | `	return VmClosureDoorCall(pCtx, pRecv, nArg, apArg, 0);` |
|       25 | 2877 | `}` |
|        - | 2878 | `/*` |
|        - | 2879 | ` * Closure::fromCallable($callable) — normalize any callable value to a Closure (reuses the` |
|        - | 2880 | ` * VmFccWrapValue primitive); idempotent on a Closure; TypeError on a non-callable.` |
|        - | 2881 | ` */` |
|      362 | 2882 | `PH7_PRIVATE int vm_builtin_Closure_fromCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 2883 | `{` |
|      367 | 2884 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 2885 | `	ph7_class_instance *pClosure;` |
|        - | 2886 | `	sxi32 rc;` |
|      367 | 2887 | `	if( nArg < 1 ){` |
|      ! 0 | 2888 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|        - | 2889 | `			"Closure::fromCallable() expects exactly 1 argument, 0 given");` |
|        - | 2890 | `	}` |
|      367 | 2891 | `	if( VmValueIsClosure(pVm, apArg[0]) ){` |
|        6 | 2892 | `		ph7_result_value(pCtx, apArg[0]); /* already a Closure: idempotent */` |
|        6 | 2893 | `		return PH7_OK;` |
|        - | 2894 | `	}` |
|        - | 2895 | `	{` |
|        - | 2896 | `		ph7_class_instance *pExc;` |
|      363 | 2897 | `		rc = PH7_VmCallableDeprecationFenced(pVm, apArg[0], &pExc);` |
|      363 | 2898 | `		if( pExc ){` |
|        - | 2899 | `			/* The error handler threw on the deprecation: php's refusal, with no reason. */` |
|        9 | 2900 | `			rc = PH7_VmThrowExceptionPrev(pCtx, pExc, "TypeError", "Failed to create closure from callable");` |
|        9 | 2901 | `			PH7_ClassInstanceUnref(pExc);` |
|        9 | 2902 | `			return rc;` |
|        - | 2903 | `		}` |
|      355 | 2904 | `		if( rc != PH7_OK ){` |
|      ! 0 | 2905 | `			return rc;` |
|        - | 2906 | `		}` |
|        - | 2907 | `	}` |
|      355 | 2908 | `	pClosure = VmFccWrapValue(pVm, apArg[0], TRUE);` |
|      355 | 2909 | `	if( pClosure == 0 ){` |
|        - | 2910 | `		/* php says WHY, with the same reason taxonomy every callback argument uses —` |
|        - | 2911 | ``		 * `Failed to create closure from callable: class P does not have a method "zz"`.`` |
|        - | 2912 | `		 * PHL answered one flat "is not a valid callback" for all eight causes, so a typo` |
|        - | 2913 | `		 * in a method name, a private one, a missing class and a bad array shape were` |
|        - | 2914 | `		 * indistinguishable. PH7_VmCallableReason is the shared builder (its tails are` |
|        - | 2915 | `		 * already byte-exact for call_user_func & friends); the fallback covers the OOM` |
|        - | 2916 | `		 * path, where the value IS callable and the reason is 0. */` |
|        - | 2917 | `		char zWhy[192];` |
|       59 | 2918 | `		const char *zReason = PH7_VmCallableReason(pVm, apArg[0], zWhy, sizeof(zWhy));` |
|       59 | 2919 | `		if( zReason ){` |
|       88 | 2920 | `			return PH7_VmThrowException(pCtx, "TypeError",` |
|       29 | 2921 | `				"Failed to create closure from callable: %s", zReason);` |
|        - | 2922 | `		}` |
|      ! 0 | 2923 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|        - | 2924 | `			"Failed to create closure from callable");` |
|        - | 2925 | `	}` |
|      297 | 2926 | `	return VmClosureResult(pCtx, pClosure);` |
|      186 | 2927 | `}` |
|        - | 2928 | `/*` |
|        - | 2929 | ` * Does a stored callback need the scope it was registered from? TRUE when it names a` |
|        - | 2930 | ` * method the global scope could not call -- a private or protected one -- or spells its` |
|        - | 2931 | `` * class half as `self`/`parent`/`static`, which only the registering frame can resolve.`` |
|        - | 2932 | ` */` |
|       14 | 2933 | `static int VmCallbackNeedsScope(ph7_vm *pVm, ph7_value *pCallback)` |
|        2 | 2934 | `{` |
|        - | 2935 | `	ph7_class_method *pMethod;` |
|       16 | 2936 | `	ph7_class *pCls = 0;` |
|       16 | 2937 | `	const char *zCls = 0, *zMeth = 0;` |
|       16 | 2938 | `	sxu32 nCls = 0, nMeth = 0;` |
|       16 | 2939 | `	if( pCallback->iFlags & MEMOBJ_STRING ){` |
|        6 | 2940 | `		const char *zName = (const char *)SyBlobData(&pCallback->sBlob);` |
|        6 | 2941 | `		sxu32 nName = SyBlobLength(&pCallback->sBlob), nSep;` |
|       16 | 2942 | `		for( nSep = 0 ; nSep + 1 < nName ; ++nSep ){` |
|       16 | 2943 | `			if( zName[nSep] == ':' && zName[nSep+1] == ':' ){` |
|        6 | 2944 | `				break;` |
|        - | 2945 | `			}` |
|        7 | 2946 | `		}` |
|        6 | 2947 | `		if( nSep + 1 >= nName ){` |
|      ! 0 | 2948 | `			return 0; /* A plain function name */` |
|        - | 2949 | `		}` |
|        6 | 2950 | `		zCls = zName;` |
|        6 | 2951 | `		nCls = nSep;` |
|        6 | 2952 | `		zMeth = zName + nSep + 2;` |
|        6 | 2953 | `		nMeth = nName - (nSep + 2);` |
|       13 | 2954 | `	}else if( pCallback->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 2955 | `		ph7_value *pTarget, *pMeth;` |
|       10 | 2956 | `		if( !PH7_VmArrayCallableParts(pVm, (ph7_hashmap *)pCallback->x.pOther, &pTarget, &pMeth)` |
|       11 | 2957 | `		 \|\| (pMeth->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 2958 | `			return 0;` |
|        - | 2959 | `		}` |
|       11 | 2960 | `		zMeth = (const char *)SyBlobData(&pMeth->sBlob);` |
|       11 | 2961 | `		nMeth = SyBlobLength(&pMeth->sBlob);` |
|       11 | 2962 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|        9 | 2963 | `			pCls = ((ph7_class_instance *)pTarget->x.pOther)->pClass;` |
|        7 | 2964 | `		}else if( pTarget->iFlags & MEMOBJ_STRING ){` |
|        3 | 2965 | `			zCls = (const char *)SyBlobData(&pTarget->sBlob);` |
|        3 | 2966 | `			nCls = SyBlobLength(&pTarget->sBlob);` |
|        2 | 2967 | `		}else{` |
|      ! 0 | 2968 | `			return 0;` |
|        - | 2969 | `		}` |
|        6 | 2970 | `	}else{` |
|      ! 0 | 2971 | `		return 0;` |
|        - | 2972 | `	}` |
|       16 | 2973 | `	if( pCls == 0 ){` |
|        6 | 2974 | `		if( (nCls == 4 && SyStrnicmp(zCls, "self", 4) == 0)` |
|        5 | 2975 | `		 \|\| (nCls == 6 && SyStrnicmp(zCls, "parent", 6) == 0)` |
|        6 | 2976 | `		 \|\| (nCls == 6 && SyStrnicmp(zCls, "static", 6) == 0) ){` |
|        3 | 2977 | `			return 1;` |
|        - | 2978 | `		}` |
|        5 | 2979 | `		pCls = PH7_VmExtractClass(pVm, zCls, nCls, FALSE, 0);` |
|        5 | 2980 | `		if( pCls == 0 ){` |
|      ! 0 | 2981 | `			return 0;` |
|        - | 2982 | `		}` |
|        2 | 2983 | `	}` |
|       13 | 2984 | `	pMethod = PH7_ClassExtractMethod(pCls, zMeth, nMeth);` |
|       13 | 2985 | `	return pMethod != 0 && pMethod->iProtection != PH7_CLASS_PROT_PUBLIC;` |
|        9 | 2986 | `}` |
|        - | 2987 | `/*` |
|        - | 2988 | ` * A callback STORED to run later -- register_shutdown_function(), spl_autoload_register() --` |
|        - | 2989 | ` * keeps the scope it was registered from. php resolves such a callback once, at registration,` |
|        - | 2990 | `` * and keeps the resolved function, so a `[$this,'priv']` registered inside its class runs at`` |
|        - | 2991 | ` * shutdown or from the autoloader; PHL kept only the value and re-resolved it from wherever the` |
|        - | 2992 | `` * call happened, which is the global scope, and died `Call to private method`. When the`` |
|        - | 2993 | ` * callback needs that scope, pOut receives the closure Closure::fromCallable() would mint here` |
|        - | 2994 | ` * and the caller invokes it instead, keeping the original value for introspection and removal.` |
|        - | 2995 | ` * pOut is left untouched (NULL) otherwise.` |
|        - | 2996 | ` */` |
|      176 | 2997 | `PH7_PRIVATE void PH7_VmBindCallbackScope(ph7_vm *pVm, ph7_value *pCallback, ph7_value *pOut)` |
|        5 | 2998 | `{` |
|        - | 2999 | `	ph7_class_instance *pClosure;` |
|      176 | 3000 | `	if( PH7_VmCallerScope(pVm) == 0 \|\| VmValueIsClosure(pVm, pCallback)` |
|       19 | 3001 | `	 \|\| !VmCallbackNeedsScope(pVm, pCallback) ){` |
|      167 | 3002 | `		return;` |
|        - | 3003 | `	}` |
|       16 | 3004 | `	pClosure = VmFccWrapValue(pVm, pCallback, TRUE);` |
|       16 | 3005 | `	if( pClosure == 0 ){` |
|      ! 0 | 3006 | `		return;` |
|        - | 3007 | `	}` |
|       16 | 3008 | `	PH7_MemObjRelease(pOut);` |
|        - | 3009 | `	/* The fresh instance's own reference is the one this value takes. */` |
|       16 | 3010 | `	pOut->x.pOther = pClosure;` |
|       16 | 3011 | `	MemObjSetType(pOut, MEMOBJ_OBJ);` |
|       93 | 3012 | `}` |
|        - | 3013 | `/*` |
|        - | 3014 | ` * Fiber::suspend($value = null) — static method.` |
|        - | 3015 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|        - | 3016 | ` */` |
|      446 | 3017 | `PH7_PRIVATE int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 3018 | `{` |
|      451 | 3019 | `	ph7_vm *pVm = pCtx->pVm;` |
|      451 | 3020 | `	if( pVm->pActiveCtx == 0 ){` |
|      ! 0 | 3021 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 3022 | `			"Cannot suspend outside of a fiber");` |
|        - | 3023 | `	}` |
|        - | 3024 | `#ifdef PH7_CORO_STACK` |
|      451 | 3025 | `	if( pVm->pActiveCtx->pCoro ){` |
|        - | 3026 | `		/* The fiber has a stack of its own, so this is not a return code that has` |
|        - | 3027 | `		 * to be threaded back out through every activation between here and the` |
|        - | 3028 | `		 * body -- it is one switch, and everything above it (this builtin's own C` |
|        - | 3029 | `		 * frame, the array_map/usort loop that called into PHP, the eval or catch` |
|        - | 3030 | `		 * body the call sits in) stays standing where it is. */` |
|      451 | 3031 | `		ph7_exec_ctx *pFiber = pVm->pActiveCtx;` |
|        - | 3032 | `		sxi32 rc;` |
|      451 | 3033 | `		if( nArg > 0 ){` |
|      437 | 3034 | `			PH7_MemObjStore(apArg[0], &pFiber->sSuspendValue);` |
|      221 | 3035 | `		}else{` |
|       19 | 3036 | `			PH7_MemObjRelease(&pFiber->sSuspendValue);` |
|        - | 3037 | `		}` |
|      451 | 3038 | `		rc = VmCoroSuspend(pCtx, pFiber);` |
|      451 | 3039 | `		if( rc != PH7_OK ){` |
|      233 | 3040 | `			return rc;  /* resumed by Fiber::throw(), or unwound by the teardown */` |
|        - | 3041 | `		}` |
|        - | 3042 | `		/* php: Fiber::suspend() ANSWERS what resume() was given. */` |
|      221 | 3043 | `		ph7_result_value(pCtx, &pFiber->sSuspendValue);` |
|      221 | 3044 | `		return PH7_OK;` |
|        - | 3045 | `	}` |
|        - | 3046 | `#endif` |
|        - | 3047 | `	/* Stage 4 scoped divergence: the trampoline only makes PHP->PHP CALLs` |
|        - | 3048 | `	 * iterative. Every OTHER re-entry runs on a fresh native VmByteCodeExec` |
|        - | 3049 | `	 * activation (nVmExecDepth bumped) that PH7_SUSPEND cannot unwind across` |
|        - | 3050 | `	 * without real coroutine stacks: a C->PHP callback` |
|        - | 3051 | `	 * (usort/array_map/preg_replace_callback comparator), and — because fibers` |
|        - | 3052 | `	 * use the LEGACY (non-inline) try/catch machinery — a suspend inside a` |
|        - | 3053 | `	 * fiber's catch/finally body, a match/switch arm, or eval()/include()'d` |
|        - | 3054 | `	 * code, all of which run via VmLocalExec. php does all of these via full` |
|        - | 3055 | `	 * native-stack switching; PHL raises a catchable FiberError instead of the` |
|        - | 3056 | `	 * old silent corruption. A suspend in a fiber's try BODY (not catch/finally)` |
|        - | 3057 | `	 * runs in the main dispatch loop and parks normally. A recorded` |
|        - | 3058 | `	 * residual; making the catch/finally case work needs fibers on the inline` |
|        - | 3059 | `	 * try machinery (the generator ROOT C path), a follow-up. */` |
|      ! 0 | 3060 | `	if( pVm->nVmExecDepth != pVm->pActiveCtx->nBodyExecDepth ){` |
|      ! 0 | 3061 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 3062 | `			"Cannot suspend across an internal call boundary");` |
|        - | 3063 | `	}` |
|      ! 0 | 3064 | `	if( nArg > 0 ){` |
|      ! 0 | 3065 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|      ! 0 | 3066 | `	}else{` |
|      ! 0 | 3067 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|        - | 3068 | `	}` |
|      ! 0 | 3069 | `	return PH7_SUSPEND;` |
|      228 | 3070 | `}` |
|        - | 3071 | `/*` |
|        - | 3072 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|        - | 3073 | ` * Actual resolution is deferred to start() so that overload selection` |
|        - | 3074 | ` * and closure-environment binding happen with the correct argument context.` |
|        - | 3075 | ` */` |
|      506 | 3076 | `PH7_PRIVATE int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 3077 | `{` |
|        - | 3078 | `	ph7_class_instance *pThis;` |
|        - | 3079 | `	ph7_value *pAttr;` |
|        - | 3080 | `	SyString sAttrName;` |
|      511 | 3081 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      511 | 3082 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|      511 | 3083 | `	if( nArg < 1 ){` |
|      ! 0 | 3084 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 3085 | `			"Fiber::__construct() expects a callable argument");` |
|        - | 3086 | `	}` |
|      511 | 3087 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 3088 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 3089 | `			"Fiber::__construct(): invalid $this");` |
|        - | 3090 | `	}` |
|      511 | 3091 | `	pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|      511 | 3092 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|      ! 0 | 3093 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 3094 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|        - | 3095 | `	}` |
|        - | 3096 | ``	/* php validates `callable $callback` HERE, with the ordinary callback-argument`` |
|        - | 3097 | ``	 * screen and its whole reason taxonomy -- `new Fiber('nosuch')` is a TypeError at`` |
|        - | 3098 | `	 * CONSTRUCTION, naming the function it could not find. PHL had a hand-rolled shape` |
|        - | 3099 | `	 * check that only asked "string or object", with a FiberError of its own wording,` |
|        - | 3100 | `	 * and left an unresolvable NAME to fail at start() instead: the fiber constructed` |
|        - | 3101 | `	 * fine and the program learned about its typo one call later. */` |
|        - | 3102 | `	{` |
|      511 | 3103 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx, apArg[0], 1, "callback", FALSE);` |
|      511 | 3104 | `		if( rcCb != PH7_OK ){` |
|       13 | 3105 | `			return rcCb;` |
|        - | 3106 | `		}` |
|        - | 3107 | `	}` |
|        - | 3108 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|      499 | 3109 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      499 | 3110 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      499 | 3111 | `	if( pAttr ){` |
|      499 | 3112 | `		PH7_MemObjStore(apArg[0], pAttr);` |
|      247 | 3113 | `	}` |
|      499 | 3114 | `	return PH7_OK;` |
|      258 | 3115 | `}` |
|        - | 3116 | `#ifdef PH7_CORO_STACK` |
|        - | 3117 | `/*` |
|        - | 3118 | ` * The stand-in body of a fiber whose callable has no bytecode for this engine to` |
|        - | 3119 | ``  * run as a coroutine: a GENERATOR function (running its body would meet `yield` `` |
|        - | 3120 | ` * outside the Generator that owns it -- php's fiber calls it and gets the` |
|        - | 3121 | ` * Generator back as its return value), an internal function, and a name php` |
|        - | 3122 | ` * routes through __call/__callStatic. Every one is simply CALLED, from the fiber's` |
|        - | 3123 | ` * own stack (VmCoroBody), which is all php's fiber does with any callable. An` |
|        - | 3124 | ` * empty function the ctx frame can name; that frame is transparent to every walk.` |
|        - | 3125 | ` */` |
|        - | 3126 | `static ph7_vm_func sFiberTrampFunc;` |
|        - | 3127 | `#endif` |
|        - | 3128 | `/*` |
|        - | 3129 | ` * Resolve a fiber's stored callable to the BODY it runs and the receiver that body` |
|        - | 3130 | `` * needs -- for every shape php's `callable` covers, not just the two PHL used to take.`` |
|        - | 3131 | ` *` |
|        - | 3132 | ` * The constructor screens the argument with php's own callback rules now` |
|        - | 3133 | ` * (PH7_CheckCallbackArg), so what arrives here is a callable; this decides which body` |
|        - | 3134 | `` * it names. `[$obj,'m']`, `['Class','stat']` and `"Class::stat"` are the everyday way`` |
|        - | 3135 | ` * to run an object's method as a coroutine, and all three were refused outright --` |
|        - | 3136 | ` * the first two by the constructor's "string or closure" shape check, the third by a` |
|        - | 3137 | ` * plain-function lookup that could never find a method.` |
|        - | 3138 | ` *` |
|        - | 3139 | ` * Answers 0 for a callable this engine has no BYTECODE body for: a host builtin` |
|        - | 3140 | `` * (`new Fiber('strtoupper')`) and a name php routes through __call/__callStatic --`` |
|        - | 3141 | ` * which is where the visibility rule lives, and why this asks for it. php does not` |
|        - | 3142 | ` * reach a private method through a callable, it reaches __call INSTEAD, so running` |
|        - | 3143 | ` * the private body would be a hole rather than a shortcut. Both shapes run on php,` |
|        - | 3144 | ` * whose fiber switches a real stack; here they are a loud refusal (the scope policy divergence,` |
|        - | 3145 | ` * twin-paired). *pzWhy names the reason for the caller to report.` |
|        - | 3146 | ` */` |
|        - | 3147 | `/* An internal class's method has a C body and no bytecode, so it is the trampoline's` |
|        - | 3148 | ` * like an internal function: run as the body, its empty instruction stream was` |
|        - | 3149 | ` * executed as whatever lay past it, and the fiber crashed on its first opcode. */` |
|        - | 3150 | `static const char VmFiberNativeMethodWhy[] =` |
|        - | 3151 | `	"callable is an internal method, which cannot be a fiber body here";` |
|      482 | 3152 | `static ph7_vm_func * VmFiberCallableBody(ph7_vm *pVm, ph7_value *pCallable,` |
|        - | 3153 | `	ph7_class_instance **ppThis, const char **pzWhy)` |
|        5 | 3154 | `{` |
|      487 | 3155 | `	ph7_class_method *pMethod = 0;` |
|      487 | 3156 | `	ph7_class *pClass = 0;` |
|      487 | 3157 | `	*ppThis = 0;` |
|      487 | 3158 | `	*pzWhy = 0;` |
|      487 | 3159 | `	if( pCallable->iFlags & MEMOBJ_HASHMAP ){` |
|        - | 3160 | ``		/* php's `[target, method]` pair, decoded by the one shared reader so a fiber`` |
|        - | 3161 | `		 * agrees with is_callable() and with every dispatch site about what it is. */` |
|       53 | 3162 | `		ph7_value *pTarget = 0, *pName = 0;` |
|       48 | 3163 | `		if( !PH7_VmArrayCallableParts(pVm, (ph7_hashmap *)pCallable->x.pOther, &pTarget, &pName)` |
|       53 | 3164 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|      ! 0 | 3165 | `			*pzWhy = "callable is not a valid [target, method] pair";` |
|      ! 0 | 3166 | `			return 0;` |
|        - | 3167 | `		}` |
|       53 | 3168 | `		pClass = PH7_VmExtractClassFromValue(pVm, pTarget);` |
|       53 | 3169 | `		if( pVm->pClosureMethodCls ){` |
|        - | 3170 | `			/* A method closure's pair resolved in a class of its own (VmClosureUnwrap). */` |
|        3 | 3171 | `			pClass = pVm->pClosureMethodCls;` |
|        3 | 3172 | `			pVm->pClosureMethodCls = 0;` |
|        1 | 3173 | `		}` |
|       53 | 3174 | `		if( pClass ){` |
|       77 | 3175 | `			pMethod = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pName->sBlob),` |
|       48 | 3176 | `				SyBlobLength(&pName->sBlob));` |
|       24 | 3177 | `		}` |
|       53 | 3178 | `		if( pMethod == 0 \|\| !PH7_VmCallableMethodAccessible(pVm, pClass, pMethod) ){` |
|        7 | 3179 | `			*pzWhy = "callable routes through __call(), which cannot be a fiber body here";` |
|        7 | 3180 | `			return 0;` |
|        - | 3181 | `		}` |
|       47 | 3182 | `		if( pMethod->sFunc.iFlags & VM_FUNC_NATIVE ){` |
|       15 | 3183 | `			*pzWhy = VmFiberNativeMethodWhy;` |
|       15 | 3184 | `			return 0;` |
|        - | 3185 | `		}` |
|       32 | 3186 | `		if( (pTarget->iFlags & MEMOBJ_OBJ) && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|       26 | 3187 | `			*ppThis = (ph7_class_instance *)pTarget->x.pOther;` |
|       11 | 3188 | `		}` |
|       32 | 3189 | `		return &pMethod->sFunc;` |
|        - | 3190 | `	}` |
|      439 | 3191 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|        - | 3192 | `		const char *zCls, *zMeth;` |
|        - | 3193 | `		sxu32 nCls, nMeth;` |
|        - | 3194 | `		SyString sName;` |
|        - | 3195 | `		SyHashEntry *pEntry;` |
|      439 | 3196 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|        - | 3197 | ``		/* php's `"Class::method"` static-callable string is the same callee as the pair. */`` |
|      439 | 3198 | `		if( PH7_VmCallableStringParts(sName.zString, sName.nByte, &zCls, &nCls, &zMeth, &nMeth) ){` |
|       18 | 3199 | `			pClass = PH7_VmResolveCallableScope(pVm, zCls, nCls);` |
|       18 | 3200 | `			pMethod = pClass ? PH7_ClassExtractMethod(pClass, zMeth, nMeth) : 0;` |
|       18 | 3201 | `			if( pMethod == 0 \|\| !PH7_VmCallableMethodAccessible(pVm, pClass, pMethod) ){` |
|        3 | 3202 | `				*pzWhy = "callable routes through __callStatic(), which cannot be a fiber body here";` |
|        3 | 3203 | `				return 0;` |
|        - | 3204 | `			}` |
|       16 | 3205 | `			if( pMethod->sFunc.iFlags & VM_FUNC_NATIVE ){` |
|        3 | 3206 | `				*pzWhy = VmFiberNativeMethodWhy;` |
|        3 | 3207 | `				return 0;` |
|        - | 3208 | `			}` |
|       13 | 3209 | `			return &pMethod->sFunc;` |
|        - | 3210 | `		}` |
|      635 | 3211 | `		pEntry = PH7_VmGetUserFunction(pVm, sName.zString, sName.nByte,` |
|      420 | 3212 | `			(pCallable->iFlags & MEMOBJ_AUX_ENGINEFN) != 0);` |
|      425 | 3213 | `		if( pEntry == 0 ){` |
|       18 | 3214 | `			*pzWhy = PH7_VmGetHostFunction(pVm, sName.zString, sName.nByte, FALSE)` |
|        - | 3215 | `				? "callable is an internal function, which cannot be a fiber body here"` |
|        8 | 3216 | `				: "callable names no such function";` |
|       18 | 3217 | `			return 0;` |
|        - | 3218 | `		}` |
|      409 | 3219 | `		return (ph7_vm_func *)pEntry->pUserData;` |
|        - | 3220 | `	}` |
|      ! 0 | 3221 | `	*pzWhy = "callable is not a string, array or object";` |
|      ! 0 | 3222 | `	return 0;` |
|      246 | 3223 | `}` |
|        - | 3224 | `/*` |
|        - | 3225 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|        - | 3226 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|        - | 3227 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|        - | 3228 | ` * so that start() can bind it as $this for the closure environment.` |
|        - | 3229 | ` */` |
|      484 | 3230 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|        - | 3231 | `	ph7_class_instance **ppThis, int *pbUnbound)` |
|        5 | 3232 | `{` |
|      489 | 3233 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 3234 | `	ph7_value *pCallable;` |
|        - | 3235 | `	SyString sAttrName;` |
|      489 | 3236 | `	*ppThis = 0;` |
|      489 | 3237 | `	*pbUnbound = 0;` |
|      489 | 3238 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      489 | 3239 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|      489 | 3240 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP)) == 0 ){` |
|      ! 0 | 3241 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|      ! 0 | 3242 | `		return 0;` |
|        - | 3243 | `	}` |
|      489 | 3244 | `	if( pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP) ){` |
|      351 | 3245 | `		const char *zWhy = 0;` |
|      351 | 3246 | `		ph7_vm_func *pFunc = VmFiberCallableBody(pVm, pCallable, ppThis, &zWhy);` |
|        - | 3247 | `#ifdef PH7_CORO_STACK` |
|      351 | 3248 | `		if( pFunc == 0 \|\| (pFunc->iFlags & VM_FUNC_GENERATOR) ){` |
|       43 | 3249 | `			*ppThis = 0;` |
|       43 | 3250 | `			return &sFiberTrampFunc;` |
|        - | 3251 | `		}` |
|        - | 3252 | `#endif` |
|      311 | 3253 | `		if( pFunc == 0 ){` |
|      ! 0 | 3254 | `			PH7_VmThrowException(pCtx, "FiberError", "Fiber %s", zWhy);` |
|      ! 0 | 3255 | `		}` |
|      311 | 3256 | `		return pFunc;` |
|      ! 0 | 3257 | `	}else{` |
|      143 | 3258 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|        - | 3259 | `		ph7_class_method *pMethod;` |
|      143 | 3260 | `		if( VmValueIsClosure(pVm, pCallable) ){` |
|        - | 3261 | `			/* A real Closure object: unwrap to its underlying callable name (the single` |
|        - | 3262 | `			 * source of truth, VmClosureUnwrap) and resolve that function. Its captured` |
|        - | 3263 | ``			 * environment (including any `$this`) rides along in the named function's`` |
|        - | 3264 | `			 * aClosureEnv, installed by VmFiberSetupFrame, so *ppThis stays 0. */` |
|        - | 3265 | `			ph7_value sName;` |
|      141 | 3266 | `			ph7_vm_func *pUnwrapped = 0;` |
|      141 | 3267 | `			const char *zWhyClo = 0;` |
|      141 | 3268 | `			PH7_MemObjInit(pVm, &sName);` |
|      141 | 3269 | `			if( VmClosureUnwrap(pVm, pCallable, &sName) == SXRET_OK ){` |
|        - | 3270 | ``				/* The engine's own `[closure_N]` key, which only this mark gets past the`` |
|        - | 3271 | `				 * script-facing name screen (PH7_VmGetUserFunction). */` |
|      141 | 3272 | `				sName.iFlags \|= MEMOBJ_AUX_ENGINEFN;` |
|        - | 3273 | `` 				/* The unwrap answers a NAME for a plain closure and a `[target, method]` `` |
|        - | 3274 | `				 * pair for a first-class callable taken from a method -- so it goes through` |
|        - | 3275 | `				 * the same body-finder as a callable the program wrote. Without it a` |
|        - | 3276 | ``				 * `$o->stat(...)` fiber could not be resolved at all. */`` |
|      141 | 3277 | `				pUnwrapped = VmFiberCallableBody(pVm, &sName, ppThis, &zWhyClo);` |
|       68 | 3278 | `			}` |
|      141 | 3279 | `			PH7_MemObjRelease(&sName);` |
|        - | 3280 | `#ifdef PH7_CORO_STACK` |
|      141 | 3281 | `			if( pUnwrapped && (pUnwrapped->iFlags & VM_FUNC_GENERATOR) ){` |
|        5 | 3282 | `				pUnwrapped = 0; /* the trampoline below calls the Closure itself */` |
|        5 | 3283 | `				*ppThis = 0;` |
|        5 | 3284 | `				zWhyClo = 0;` |
|        2 | 3285 | `			}` |
|        - | 3286 | `#endif` |
|      141 | 3287 | `			if( pUnwrapped ){` |
|        - | 3288 | `				/* A BOUND closure parked its $this in the pClosureThis transient` |
|        - | 3289 | `				 * (VmClosureUnwrap): consume it as the fiber's $this — it wins over` |
|        - | 3290 | `				 * the creation-time env capture (VmFiberSetupFrame's skip). *ppThis` |
|        - | 3291 | `				 * is a borrow (the closure's $__this attr keeps the object alive` |
|        - | 3292 | `				 * through $__callable), so drop the parked ref. The scope transient` |
|        - | 3293 | `				 * is cleared alongside: fiber bodies don't model pBoundScope` |
|        - | 3294 | `				 * visibility (recorded residual), and a stale transient would` |
|        - | 3295 | `				 * poison the next OP_CALL's frame. */` |
|      131 | 3296 | `				if( pVm->pClosureThis ){` |
|      ! 0 | 3297 | `					*ppThis = pVm->pClosureThis;` |
|      ! 0 | 3298 | `					PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|      ! 0 | 3299 | `					pVm->pClosureThis = 0;` |
|      ! 0 | 3300 | `				}` |
|      131 | 3301 | `				pVm->pClosureScope = 0;` |
|      131 | 3302 | `				*pbUnbound = pVm->bClosureUnbound;` |
|      131 | 3303 | `				pVm->bClosureUnbound = 0;` |
|      131 | 3304 | `				pVm->bClosureScreened = 0;` |
|      131 | 3305 | `				pVm->bClosureStaticTramp = 0;` |
|      131 | 3306 | `				pVm->bClosureNoNamed = 0;` |
|      131 | 3307 | `				pVm->pClosureMethodCls = 0;` |
|      131 | 3308 | `				return pUnwrapped;` |
|        - | 3309 | `			}` |
|       12 | 3310 | `			if( pVm->pClosureThis ){` |
|        - | 3311 | `				/* Failed resolution: drop the parked transient so it neither leaks` |
|        - | 3312 | `				 * nor poisons the next call. */` |
|      ! 0 | 3313 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|      ! 0 | 3314 | `				pVm->pClosureThis = 0;` |
|      ! 0 | 3315 | `			}` |
|       12 | 3316 | `			pVm->pClosureScope = 0;` |
|       12 | 3317 | `			pVm->bClosureUnbound = 0;` |
|       12 | 3318 | `			pVm->bClosureScreened = 0;` |
|       12 | 3319 | `			pVm->bClosureStaticTramp = 0;` |
|       12 | 3320 | `			pVm->bClosureNoNamed = 0;` |
|       12 | 3321 | `			pVm->pClosureMethodCls = 0;` |
|        - | 3322 | `#ifdef PH7_CORO_STACK` |
|       12 | 3323 | `			return &sFiberTrampFunc;` |
|        - | 3324 | `#endif` |
|      ! 0 | 3325 | `			PH7_VmThrowException(pCtx, "FiberError", zWhyClo` |
|        - | 3326 | `				? "Fiber %s" : "Fiber callable closure could not be resolved", zWhyClo);` |
|      ! 0 | 3327 | `			return 0;` |
|        - | 3328 | `		}` |
|        - | 3329 | `		/* Object callable — resolve __invoke method */` |
|        3 | 3330 | `		pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 3331 | `			sizeof("__invoke") - 1);` |
|        3 | 3332 | `		if( pMethod == 0 ){` |
|      ! 0 | 3333 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 3334 | `				"Fiber callable object has no __invoke method");` |
|      ! 0 | 3335 | `			return 0;` |
|        - | 3336 | `		}` |
|        - | 3337 | `#ifdef PH7_CORO_STACK` |
|        3 | 3338 | `		if( pMethod->sFunc.iFlags & VM_FUNC_GENERATOR ){` |
|      ! 0 | 3339 | `			return &sFiberTrampFunc;` |
|        - | 3340 | `		}` |
|        - | 3341 | `#endif` |
|        3 | 3342 | `		*ppThis = pClosure;` |
|        3 | 3343 | `		return &pMethod->sFunc;` |
|        - | 3344 | `	}` |
|      247 | 3345 | `}` |
|        - | 3346 | `/*` |
|        - | 3347 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|        - | 3348 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|        - | 3349 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|        - | 3350 | ` */` |
|        - | 3351 | `/*` |
|        - | 3352 | ` * Enforce one formal parameter's declared type on an argument being installed.` |
|        - | 3353 | ` * THE single implementation of the per-argument check, shared by the` |
|        - | 3354 | ` * generator/fiber initial-frame binder below (band A #2) and both OP_CALL` |
|        - | 3355 | ` * install paths (named-map and positional — they carried two verbatim copies` |
|        - | 3356 | ` * until the recorded(f) fold): union types via VmCoerceToUnion, class and` |
|        - | 3357 | ` * pseudo types (VmCheckPseudoType + VmResolveTypeClass + instanceof, so` |
|        - | 3358 | ``  * interfaces/abstract classes and self/parent resolve), the bare `object` `` |
|        - | 3359 | ` * hint, and scalars via VmEnforceScalarType (weak-mode coercion in place,` |
|        - | 3360 | ` * strict rejection otherwise) — with the VM_FUNC_ARG_NULLABLE guard letting` |
|        - | 3361 | `` * null through for `?type` and implicit-nullable `Type $x = null` params,`` |
|        - | 3362 | ` * and whole-real materialization on a mask match.` |
|        - | 3363 | ` * Returns SXRET_OK (value possibly coerced) or the VmThrowTypeErrorForArg` |
|        - | 3364 | ` * status for the caller to route — normalized so an INLINE-caught throw` |
|        - | 3365 | ` * (VmThrowInline records only a pc-redirect and reports SXRET_OK) still` |
|        - | 3366 | ` * comes back as PH7_EXCEPTION: the binding must stop; the OP_CALL sites` |
|        - | 3367 | ` * force rc = PH7_EXCEPTION on any non-OK status anyway, and the generator` |
|        - | 3368 | ` * block's PH7_INLINE_RESUME_BREAK consumes the redirect.` |
|        - | 3369 | ` */` |
|      558 | 3370 | `static sxi32 VmGenArgThrowStatus(ph7_vm *pVm, sxi32 rcThrow)` |
|        5 | 3371 | `{` |
|      563 | 3372 | `	if( rcThrow == SXRET_OK && pVm->pInlineInstr ){` |
|      ! 0 | 3373 | `		return PH7_EXCEPTION;` |
|        - | 3374 | `	}` |
|      563 | 3375 | `	return rcThrow;` |
|      284 | 3376 | `}` |
|   468894 | 3377 | `PH7_PRIVATE sxi32 VmEnforceArgType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_vm_func_arg *pFormal,` |
|        - | 3378 | `	sxu32 nArgPos, ph7_value *pVal, int bStrict, ph7_class *pSelfHint)` |
|        5 | 3379 | `{` |
|   468899 | 3380 | `	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){` |
|      504 | 3381 | `		if( VmCoerceToUnion(pVm,pVal,&pFormal->aUnionAlts,` |
|      509 | 3382 | `			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,bStrict,pSelfHint) != SXRET_OK ){` |
|        - | 3383 | `			const char *zGiven;` |
|       80 | 3384 | `			const char *zExpected = "union";` |
|        - | 3385 | `			char zBuf[128];` |
|        - | 3386 | `			char zTypeBuf[128];` |
|       80 | 3387 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       50 | 3388 | `				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|       57 | 3389 | `			}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|       10 | 3390 | `				zGiven = "null";` |
|        6 | 3391 | `			}else{` |
|       24 | 3392 | `				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));` |
|        - | 3393 | `			}` |
|       80 | 3394 | `			if( SyStringLength(&pFormal->sTypeName) > 0 ){` |
|      118 | 3395 | `				zExpected = VmHintTextResolved(pVm,&pFormal->sTypeName,pSelfHint,` |
|       38 | 3396 | `					zTypeBuf,sizeof(zTypeBuf));` |
|       38 | 3397 | `			}` |
|      118 | 3398 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|       38 | 3399 | `				&pFormal->sName,zExpected,zGiven));` |
|        - | 3400 | `		}` |
|      265 | 3401 | `		return SXRET_OK;` |
|        - | 3402 | `	}` |
|   468558 | 3403 | `	if( pFormal->nType == 0` |
|   258562 | 3404 | `	 \|\| ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|   422422 | 3405 | `		return SXRET_OK;` |
|        - | 3406 | `	}` |
|    46146 | 3407 | `	if( pFormal->nType == SXU32_HIGH ){` |
|        - | 3408 | `		/* Class or pseudo type */` |
|     7137 | 3409 | `		SyString *pName = &pFormal->sClass;` |
|        - | 3410 | `		ph7_class *pClass;` |
|     7137 | 3411 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|     7137 | 3412 | `		if( rcPseudo == 0 ){` |
|        - | 3413 | `			char zTypeBuf[128],zGivenBuf[128];` |
|      139 | 3414 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|       34 | 3415 | `				&pFormal->sName,` |
|       68 | 3416 | `				VmClassHintTypeName(pName,0,` |
|       68 | 3417 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|       34 | 3418 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|        - | 3419 | `		}` |
|     7069 | 3420 | `		pClass = 0;` |
|     7069 | 3421 | `		if( rcPseudo != 1 && !VmClassHintMatches(&(*pVm),pName,pSelfHint,pVal,&pClass) ){` |
|        - | 3422 | `			char zTypeBuf[128],zGivenBuf[128];` |
|      112 | 3423 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|       27 | 3424 | `				&pFormal->sName,` |
|       54 | 3425 | `				VmClassHintTypeName(pName,pClass,` |
|       54 | 3426 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|       27 | 3427 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|        - | 3428 | `		}` |
|     7015 | 3429 | `		return SXRET_OK;` |
|        - | 3430 | `	}` |
|    39014 | 3431 | `	if( (pVal->iFlags & pFormal->nType) == 0 ){` |
|        - | 3432 | `		char zGivenBuf[128];` |
|      601 | 3433 | `		if( pFormal->nType == MEMOBJ_OBJ ){` |
|      192 | 3434 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|        8 | 3435 | `				&pFormal->sName,"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|        - | 3436 | `		}` |
|      585 | 3437 | `		if( VmEnforceScalarType(pVal,pFormal->nType,bStrict) != SXRET_OK ){` |
|        - | 3438 | `			char zTypeBuf[128];` |
|      503 | 3439 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      166 | 3440 | `				&pFormal->sName,` |
|      166 | 3441 | `				VmScalarTypeName(pFormal->nType,&pFormal->sTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      166 | 3442 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|        - | 3443 | `		}` |
|      129 | 3444 | `	}else{` |
|        - | 3445 | `		/* Mask matched — an int param accepting a whole-real materializes` |
|        - | 3446 | `		 * it (php: g(1.0) into int $x is int(1)). */` |
|    38418 | 3447 | `		VmMaterializeIntTyped(pVal,pFormal->nType);` |
|        - | 3448 | `	}` |
|    38666 | 3449 | `	return SXRET_OK;` |
|   235257 | 3450 | `}` |
|        - | 3451 | `/*` |
|        - | 3452 | ` * Record a caller slot this body's frame now ALIASES through a by-reference` |
|        - | 3453 | ` * parameter. The body outlives its caller, so the two frames cannot each own the` |
|        - | 3454 | ` * slot: the caller's teardown counts this frame's name binding as a holder and` |
|        - | 3455 | ` * leaves the value standing, and VmReleaseExecCtx asks PH7_VmReleaseUnheldSlot for` |
|        - | 3456 | ` * every row here once its own names are gone — whichever dies last frees it.` |
|        - | 3457 | ` */` |
|       34 | 3458 | `static void VmCtxAliasByRefArg(ph7_exec_ctx *pExecCtx,sxu32 nIdx)` |
|        2 | 3459 | `{` |
|       36 | 3460 | `	sxu32 *aIdx = (sxu32 *)SySetBasePtr(&pExecCtx->aByRefArg);` |
|        - | 3461 | `	sxu32 n;` |
|       36 | 3462 | `	for( n = 0 ; n < SySetUsed(&pExecCtx->aByRefArg) ; ++n ){` |
|      ! 0 | 3463 | `		if( aIdx[n] == nIdx ){` |
|        - | 3464 | ``			/* Two parameters over one actual (`g($x,$x)`) is ONE slot to give back. */`` |
|      ! 0 | 3465 | `			return;` |
|        - | 3466 | `		}` |
|      ! 0 | 3467 | `	}` |
|       36 | 3468 | `	SySetPut(&pExecCtx->aByRefArg,(const void *)&nIdx);` |
|       19 | 3469 | `}` |
|        - | 3470 | `/*` |
|        - | 3471 | ` * Bind a named call's actuals for a body that runs in an execution context of its own` |
|        - | 3472 | ` * -- a generator built by its call, a fiber by Fiber::start() -- into the layout` |
|        - | 3473 | ` * VmFiberSetupFrame reads: one entry per formal up to the last one bound (NULL for a` |
|        - | 3474 | ` * hole the default answers), then the positional overflow, then the unknown-name` |
|        - | 3475 | ` * extras a variadic collects, with aOutName naming those extras alone.` |
|        - | 3476 | ` *` |
|        - | 3477 | ` * apOut and aOutName hold nActual plus the formal count. Answers SXRET_OK, php's` |
|        - | 3478 | ` * catchable Error/ArgumentCountError status (PH7_EXCEPTION or PH7_ABORT), or SXERR_MEM,` |
|        - | 3479 | ` * on which the caller keeps the positional binding it had.` |
|        - | 3480 | ` *` |
|        - | 3481 | `` * A required formal left unbound below a bound one is php's `Argument #N ($x) not`` |
|        - | 3482 | `` * passed`, which php raises from inside the body: its trace has the body's frame. So it`` |
|        - | 3483 | ` * is not thrown here, where that frame does not exist yet; *piHole names the formal (or` |
|        - | 3484 | ` * is -1), no layout is made, and the caller throws it through VmCtxThrowNamedHole once` |
|        - | 3485 | ` * the body's frame is on the chain.` |
|        - | 3486 | ` */` |
|      100 | 3487 | `PH7_PRIVATE sxi32 VmCtxBindNamedArgs(ph7_vm *pVm, ph7_vm_func *pFunc,` |
|        - | 3488 | `	VmCallArgMap *pMap, sxu32 nActual, ph7_value **apIn,` |
|        - | 3489 | `	ph7_value **apOut, SyString *aOutName, int *pnOut, sxi32 *piHole)` |
|        5 | 3490 | `{` |
|      105 | 3491 | `	ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|      105 | 3492 | `	sxu32 nF = SySetUsed(&pFunc->aArgs);` |
|      105 | 3493 | `	sxu32 nNV = nF, nReq, nNVIgnored, nMaxFilled = 0, i, j;` |
|      105 | 3494 | `	sxi32 iVIdx = -1, iHole = -1;` |
|        - | 3495 | `	sxi32 *aSlot;` |
|        - | 3496 | `	sxu8 *aUsed;` |
|      105 | 3497 | `	int nOut = 0, nFilled = 0;` |
|        - | 3498 | `	sxi32 rc;` |
|      301 | 3499 | `	for( i = 0 ; i < nF ; i++ ){` |
|      233 | 3500 | `		if( aFA[i].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       36 | 3501 | `			nNV = i;` |
|       36 | 3502 | `			iVIdx = (sxi32)i;` |
|       36 | 3503 | `			break;` |
|        - | 3504 | `		}` |
|      103 | 3505 | `	}` |
|      155 | 3506 | `	aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,` |
|      100 | 3507 | `		nActual * sizeof(sxi32) + nNV * sizeof(sxu8) + 1);` |
|      105 | 3508 | `	if( aSlot == 0 ){` |
|      ! 0 | 3509 | `		return SXERR_MEM;` |
|        - | 3510 | `	}` |
|      105 | 3511 | `	aUsed = (sxu8 *)&aSlot[nActual];` |
|      105 | 3512 | `	rc = VmResolveNamedArgs(&(*pVm),pMap,aFA,nNV,iVIdx,nActual,aSlot,aUsed);` |
|      105 | 3513 | `	if( rc != SXRET_OK ){` |
|       12 | 3514 | `		SyMemBackendFree(&pVm->sAllocator,aSlot);` |
|       12 | 3515 | `		return rc;` |
|        - | 3516 | `	}` |
|        - | 3517 | `	/* php's named-hole ArgumentCountError, checked BEFORE the holes are laid out:` |
|        - | 3518 | `	 * counting them would report the positional wording with a fictitious count` |
|        - | 3519 | ``	 * (g(b:2) is `g(): Argument #1 ($a) not passed`, not "1 passed"). A hole with`` |
|        - | 3520 | `	 * NOTHING bound above it keeps php's count wording, which VmFiberSetupFrame` |
|        - | 3521 | `	 * raises from the positional count. */` |
|       95 | 3522 | `	nReq = VmFuncRequiredArgCount(pFunc,&nNVIgnored);` |
|      255 | 3523 | `	for( i = 0 ; i < nActual ; i++ ){` |
|      165 | 3524 | `		if( aSlot[i] >= 0 && (sxu32)(aSlot[i] + 1) > nMaxFilled ){` |
|      107 | 3525 | `			nMaxFilled = (sxu32)(aSlot[i] + 1);` |
|       51 | 3526 | `		}` |
|       85 | 3527 | `	}` |
|      181 | 3528 | `	for( i = 0 ; i < nReq && i < nMaxFilled && iHole < 0 ; i++ ){` |
|       90 | 3529 | `		int bFound = 0;` |
|      144 | 3530 | `		for( j = 0 ; j < nActual ; j++ ){` |
|      116 | 3531 | `			if( aSlot[j] == (sxi32)i ){ bFound = 1; break; }` |
|       31 | 3532 | `		}` |
|       90 | 3533 | `		if( !bFound ){` |
|       31 | 3534 | `			iHole = (sxi32)i;` |
|       14 | 3535 | `		}` |
|       47 | 3536 | `	}` |
|       95 | 3537 | `	*piHole = iHole;` |
|       95 | 3538 | `	if( iHole >= 0 ){` |
|       31 | 3539 | `		SyMemBackendFree(&pVm->sAllocator,aSlot);` |
|       31 | 3540 | `		*pnOut = 0;` |
|       31 | 3541 | `		return SXRET_OK;` |
|        - | 3542 | `	}` |
|      187 | 3543 | `	for( i = 0 ; i < nNV ; i++ ){` |
|      125 | 3544 | `		ph7_value *pBound = 0;` |
|      213 | 3545 | `		for( j = 0 ; j < nActual ; j++ ){` |
|      183 | 3546 | `			if( aSlot[j] == (sxi32)i ){` |
|       95 | 3547 | `				pBound = apIn[j];` |
|       95 | 3548 | `				break;` |
|        - | 3549 | `			}` |
|       48 | 3550 | `		}` |
|      125 | 3551 | `		SyZero(&aOutName[nOut],sizeof(SyString));` |
|      125 | 3552 | `		apOut[nOut++] = pBound;` |
|      125 | 3553 | `		if( pBound ){` |
|       95 | 3554 | `			nFilled = nOut;` |
|       45 | 3555 | `		}` |
|       65 | 3556 | `	}` |
|       67 | 3557 | `	nOut = nFilled;` |
|        - | 3558 | `	/* The overflow: positional first, then the named extras. Not the order the call` |
|        - | 3559 | `	 * wrote them in -- a later unpack's positional actual may follow an earlier` |
|        - | 3560 | `	 * one's name -- so the names are a second pass. */` |
|      191 | 3561 | `	for( j = 0 ; j < 2 ; j++ ){` |
|      381 | 3562 | `		for( i = 0 ; i < nActual ; i++ ){` |
|      257 | 3563 | `			int bNamed = (i < pMap->nTotal && pMap->aNames[i].nByte > 0);` |
|      257 | 3564 | `			if( aSlot[i] < 0 && bNamed == (int)j ){` |
|       39 | 3565 | `				SyZero(&aOutName[nOut],sizeof(SyString));` |
|       39 | 3566 | `				if( bNamed ){` |
|       29 | 3567 | `					aOutName[nOut] = pMap->aNames[i];` |
|       13 | 3568 | `				}` |
|       39 | 3569 | `				apOut[nOut++] = apIn[i];` |
|       18 | 3570 | `			}` |
|      131 | 3571 | `		}` |
|       67 | 3572 | `	}` |
|       67 | 3573 | `	SyMemBackendFree(&pVm->sAllocator,aSlot);` |
|       67 | 3574 | `	*pnOut = nOut;` |
|       67 | 3575 | `	return SXRET_OK;` |
|       55 | 3576 | `}` |
|        - | 3577 | `/*` |
|        - | 3578 | ` * Throw the named hole VmCtxBindNamedArgs reported, with the body's frame on the chain.` |
|        - | 3579 | ` */` |
|       28 | 3580 | `PH7_PRIVATE sxi32 VmCtxThrowNamedHole(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_class *pSelfHint,` |
|        - | 3581 | `	sxi32 iHole)` |
|        3 | 3582 | `{` |
|       31 | 3583 | `	ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|       59 | 3584 | `	return VmThrowArgNotPassed(&(*pVm),pSelfHint,&pFunc->sName,pFunc,` |
|       28 | 3585 | `		(sxu32)iHole + 1,&aFA[iHole].sName);` |
|        3 | 3586 | `}` |
|        - | 3587 | `/*` |
|        - | 3588 | ` * aArgName, when given, runs parallel to apArg and names the trailing actuals a named` |
|        - | 3589 | ` * call could not bind to any formal (VmCtxBindNamedArgs lays them out last): php keeps` |
|        - | 3590 | ` * those out of the call's argument count and hands them to the variadic KEYED by the` |
|        - | 3591 | ` * name they were passed under. Every other entry has an empty name.` |
|        - | 3592 | ` */` |
|     1204 | 3593 | `static sxi32 VmFiberBindFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - | 3594 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg,` |
|        - | 3595 | `	const SyString *aArgName,` |
|        - | 3596 | `	int bStrict, ph7_class *pSelfHint, int bCallSiteInMsg, int bAliasByRef)` |
|        5 | 3597 | `{` |
|     1209 | 3598 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|        - | 3599 | `	ph7_vm_func_arg *aFormalArg;` |
|        - | 3600 | `	sxu32 nFormal, n;` |
|     1209 | 3601 | `	sxu32 nReqGF = 0, nNonVarGF = 0; /* too-few-args watermark (0 = exempt) */` |
|     1209 | 3602 | `	int nPos = nArg; /* the actuals before the first named extra */` |
|        - | 3603 | `	VmSlot sSlot;` |
|        - | 3604 | `	sxi32 rc;` |
|     1209 | 3605 | `	if( aArgName ){` |
|        - | 3606 | `		int k;` |
|      185 | 3607 | `		for( k = 0 ; k < nArg ; ++k ){` |
|      145 | 3608 | `			if( aArgName[k].nByte > 0 ){` |
|       25 | 3609 | `				nPos = k;` |
|       25 | 3610 | `				break;` |
|        - | 3611 | `			}` |
|       64 | 3612 | `		}` |
|       31 | 3613 | `	}` |
|        - | 3614 | `	/* Install $this for closure/method callables */` |
|     1209 | 3615 | `	if( pClosureThis ){` |
|        - | 3616 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|       55 | 3617 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|       55 | 3618 | `		if( pObj ){` |
|       55 | 3619 | `			pObj->x.pOther = pClosureThis;` |
|       55 | 3620 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|       55 | 3621 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|       25 | 3622 | `		}` |
|        - | 3623 | `		/* And on the FRAME, which is what everything asking "whose method is this` |
|        - | 3624 | `		 * activation" reads: a coroutine body installed the receiver only as a` |
|        - | 3625 | ``		 * variable, so a `debug_backtrace()` frame for a generator METHOD came back`` |
|        - | 3626 | ``		 * with its class but no `object` — and twig's error reporter, which finds the`` |
|        - | 3627 | `` 		 * template to blame by looking for `$trace['object'] instanceof Template` `` |
|        - | 3628 | ``		 * across the backtrace, found none and could not say `at line N` for any`` |
|        - | 3629 | `		 * template whose failing frame is a compiled generator. Borrowed exactly like` |
|        - | 3630 | `		 * VmEnterFrame's: the variable installed above owns the reference, and it` |
|        - | 3631 | `		 * lives and dies with this frame. */` |
|       55 | 3632 | `		if( pExecCtx->pFrame ){` |
|       55 | 3633 | `			pExecCtx->pFrame->pThis = pClosureThis;` |
|       25 | 3634 | `		}` |
|       25 | 3635 | `	}` |
|        - | 3636 | `	/* Install static variables */` |
|     1209 | 3637 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|        - | 3638 | `		ph7_vm_func_static_var *aStatic;` |
|        - | 3639 | `		ph7_value *pVal;` |
|      ! 0 | 3640 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|      ! 0 | 3641 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|      ! 0 | 3642 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|      ! 0 | 3643 | `			if( pVal ){` |
|      ! 0 | 3644 | `				sSlot.pUserData = 0;` |
|      ! 0 | 3645 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|      ! 0 | 3646 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|      ! 0 | 3647 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|      ! 0 | 3648 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      ! 0 | 3649 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal,FALSE);` |
|      ! 0 | 3650 | `				}` |
|      ! 0 | 3651 | `			}` |
|      ! 0 | 3652 | `		}` |
|      ! 0 | 3653 | `	}` |
|        - | 3654 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|     1209 | 3655 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|     1209 | 3656 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|        - | 3657 | `	/* Actual call arity for func_num_args()/func_get_args() (band A #4) */` |
|     1209 | 3658 | `	pExecCtx->pFrame->nActualArgs = nArg; /* named extras included, as OP_CALL stamps it */` |
|        - | 3659 | `	{` |
|        - | 3660 | `		/* Too-few-arguments watermark — checked per formal INSIDE the install` |
|        - | 3661 | `		 * loop below, after the passed args' type checks, matching php's` |
|        - | 3662 | `		 * RECV order (a type error on a passed argument beats the count` |
|        - | 3663 | `		 * error). Generators throw at the g(...) call site (message embeds` |
|        - | 3664 | `		 * it); fibers at Fiber::start() (php omits the call-site segment).` |
|        - | 3665 | `		 * Prelude builtins are included — VmThrowBuiltinTooFewArgs words them` |
|        - | 3666 | `		 * as php words an internal callable. */` |
|     1209 | 3667 | `	nReqGF = VmFuncRequiredArgCount(pFunc,&nNonVarGF);` |
|        - | 3668 | `	}` |
|     1487 | 3669 | `	for( n = 0; n < nFormal; n++ ){` |
|        - | 3670 | `		ph7_value *pObj;` |
|      383 | 3671 | `		if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|        - | 3672 | `			/* Variadic formal: collect this and every remaining actual into a` |
|        - | 3673 | `			 * fresh array (php semantics — pre-fix nothing collected here, so a` |
|        - | 3674 | ``			 * `function g(int ...$xs)` generator saw a bare scalar in $xs).`` |
|        - | 3675 | `			 * Per-element checks mirror OP_CALL's variadic install: union via` |
|        - | 3676 | ``			 * the shared helper, scalar coerce/TypeError, `object` hint; a`` |
|        - | 3677 | `			 * class-typed variadic element is (like OP_CALL) not checked. */` |
|       49 | 3678 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       49 | 3679 | `			if( pObj ){` |
|        - | 3680 | `				sxu32 nVariadicIdx;` |
|        - | 3681 | `				ph7_hashmap *pMap;` |
|        - | 3682 | `				sxu32 k;` |
|       49 | 3683 | `				PH7_MemObjToHashmap(pObj);` |
|        - | 3684 | `				/* Capture the slot index now: PH7_HashmapInsert used to reallocate` |
|        - | 3685 | `				 * pVm->aMemObj and dangle pObj (same hazard as OP_CALL's path).` |
|        - | 3686 | `				 * Redundant now the table is segmented; left for the harvest sweep. */` |
|       49 | 3687 | `				nVariadicIdx = pObj->nIdx;` |
|       49 | 3688 | `				pMap = (ph7_hashmap *)pObj->x.pOther;` |
|        - | 3689 | `				/* The named extras sit past every positional actual, which can be` |
|        - | 3690 | ``				 * BEFORE this formal when a hole sits between (`g(1, zz: 2)` against`` |
|        - | 3691 | ``				 * `g($a, $b = 5, ...$r)`), so the walk starts at whichever comes first. */`` |
|       99 | 3692 | `				for( k = (n < (sxu32)nPos ? n : (sxu32)nPos); k < (sxu32)nArg; k++ ){` |
|        - | 3693 | `					ph7_value sKey;` |
|       69 | 3694 | `					ph7_value *pKey = 0;` |
|       69 | 3695 | `					if( apArg[k] == 0 ){` |
|      ! 0 | 3696 | `						continue; /* a named call's hole: the formal it names is not this one */` |
|        - | 3697 | `					}` |
|       69 | 3698 | `					if( k >= (sxu32)nPos ){` |
|       29 | 3699 | `						PH7_MemObjInitFromString(pVm,&sKey,&aArgName[k]);` |
|       29 | 3700 | `						pKey = &sKey;` |
|       13 | 3701 | `					}` |
|        - | 3702 | `					/* php numbers a collected element by its place in the CALL, not by` |
|        - | 3703 | `					 * the formal's (a named extra reports one past the positionals),` |
|        - | 3704 | `					 * names no parameter, and checks a class-typed element too: the` |
|        - | 3705 | `					 * ordinary call's per-element check, which this path had its own` |
|        - | 3706 | `					 * narrower copy of. */` |
|      101 | 3707 | `					rc = VmVariadicElementTypeCheck(pVm,pSelfHint,pFunc,&aFormalArg[n],apArg[k],` |
|       64 | 3708 | `						k < (sxu32)nPos ? k+1 : (sxu32)nPos+1,bStrict);` |
|       69 | 3709 | `					if( rc != SXRET_OK ){` |
|       17 | 3710 | `						if( pKey ){` |
|        5 | 3711 | `							PH7_MemObjRelease(pKey);` |
|        2 | 3712 | `						}` |
|       17 | 3713 | `						return rc;` |
|        - | 3714 | `					}` |
|       50 | 3715 | `					if( bAliasByRef && (aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF)` |
|       18 | 3716 | `					 && apArg[k]->nIdx != SXU32_HIGH ){` |
|        - | 3717 | `						/* A by-ref variadic tail aliases its actuals here too — the` |
|        - | 3718 | `						 * ordinary call's rule, one container over. */` |
|        3 | 3719 | `						VmCtxAliasByRefArg(pExecCtx,apArg[k]->nIdx);` |
|        3 | 3720 | `						PH7_HashmapInsertByRef(pMap,pKey,apArg[k]->nIdx);` |
|        2 | 3721 | `					}else{` |
|       52 | 3722 | `						PH7_HashmapInsert(pMap,pKey,apArg[k]);` |
|        - | 3723 | `					}` |
|       54 | 3724 | `					if( pKey ){` |
|       25 | 3725 | `						PH7_MemObjRelease(pKey);` |
|       11 | 3726 | `					}` |
|       29 | 3727 | `				}` |
|       33 | 3728 | `				sSlot.nIdx = nVariadicIdx;` |
|       33 | 3729 | `				sSlot.pUserData = 0;` |
|       33 | 3730 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       15 | 3731 | `			}` |
|       33 | 3732 | `			break; /* All remaining actuals consumed */` |
|        - | 3733 | `		}` |
|      339 | 3734 | `		if( n < (sxu32)nPos && apArg[n] != 0 ){` |
|        - | 3735 | `			/* Argument provided — install with declared-type enforcement.` |
|        - | 3736 | ``			 * A NULL entry is a named call's HOLE: `g(a: 1, c: 9)` names the`` |
|        - | 3737 | `			 * first and third formals and says nothing about the second, so the` |
|        - | 3738 | `			 * list arrives one entry per formal and the hole falls through to the` |
|        - | 3739 | `			 * default branch below, exactly as php's binder does.` |
|        - | 3740 | `			 * php binds and type-checks generator arguments EAGERLY at the` |
|        - | 3741 | `			 * g(...) call site (and fiber arguments at Fiber::start()), so the` |
|        - | 3742 | `			 * enforcement lives here, mirroring the OP_CALL install path via` |
|        - | 3743 | `			 * VmEnforceArgType (TypeError on mismatch, weak coercion in` |
|        - | 3744 | `			 * place otherwise) instead of the old silent xCast. A variadic` |
|        - | 3745 | `			 * formal collects as-is (no per-element declared-type model). */` |
|      276 | 3746 | `			if( bAliasByRef && (aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF)` |
|      129 | 3747 | `			 && apArg[n]->nIdx != SXU32_HIGH ){` |
|        - | 3748 | `				/* php binds a generator's by-REFERENCE parameter to the CALLER's slot at` |
|        - | 3749 | `				 * the g(...) that builds the Generator, so the body's write reaches the` |
|        - | 3750 | `				 * caller's variable whenever it eventually runs. Copying it left the` |
|        - | 3751 | `				 * actual untouched for every resume. The type check runs on the actual,` |
|        - | 3752 | `				 * as OP_CALL's by-ref binder does, and never on a copy the alias` |
|        - | 3753 | `				 * replaces. Fiber::start() and the embedder entry pass by VALUE (php's` |
|        - | 3754 | `				 * own decision at those two boundaries), hence bAliasByRef. */` |
|       34 | 3755 | `				sxi32 iPreFlags = apArg[n]->iFlags;` |
|       34 | 3756 | `				rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,apArg[n],bStrict,pSelfHint);` |
|       34 | 3757 | `				if( rc != SXRET_OK ){` |
|      ! 0 | 3758 | `					return rc;` |
|        - | 3759 | `				}` |
|        - | 3760 | `				/* A declared type's conversion is what the reference holds (the ordinary` |
|        - | 3761 | `				 * call's rule; the check ran on the operand-stack copy). */` |
|       34 | 3762 | `				PH7_VmByRefArgWriteBack(pVm,apArg[n],iPreFlags);` |
|       50 | 3763 | `				PH7_VmBindVarSlot(pVm,pExecCtx->pFrame,` |
|       32 | 3764 | `					SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName),` |
|       32 | 3765 | `					apArg[n]->nIdx);` |
|       34 | 3766 | `				VmCtxAliasByRefArg(pExecCtx,apArg[n]->nIdx);` |
|       34 | 3767 | `				sSlot.nIdx = apArg[n]->nIdx;` |
|       34 | 3768 | `				sSlot.pUserData = 0;` |
|       34 | 3769 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       34 | 3770 | `				continue;` |
|        - | 3771 | `			}` |
|      249 | 3772 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      249 | 3773 | `			if( pObj ){` |
|      249 | 3774 | `				PH7_MemObjStore(apArg[n], pObj);` |
|      249 | 3775 | `				if( (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|      249 | 3776 | `					rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,pObj,bStrict,pSelfHint);` |
|      249 | 3777 | `					if( rc != SXRET_OK ){` |
|       46 | 3778 | `						return rc;` |
|        - | 3779 | `					}` |
|      101 | 3780 | `				}` |
|      207 | 3781 | `				sSlot.nIdx = pObj->nIdx;` |
|      207 | 3782 | `				sSlot.pUserData = 0;` |
|      207 | 3783 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      106 | 3784 | `			}` |
|      163 | 3785 | `		}else if( n < nReqGF ){` |
|        - | 3786 | `			/* Required formal with no actual: php's ArgumentCountError, at` |
|        - | 3787 | `			 * this point in the install order (see the watermark comment). */` |
|       21 | 3788 | `			return VmGenArgThrowStatus(pVm,` |
|       12 | 3789 | `				(pFunc->iFlags & VM_FUNC_INTERNAL)` |
|      ! 0 | 3790 | `					? VmThrowBuiltinTooFewArgs(pVm,pSelfHint,&pFunc->sName,` |
|      ! 0 | 3791 | `						(sxu32)nPos,nReqGF,SySetUsed(&pFunc->aArgs))` |
|       18 | 3792 | `					: VmThrowTooFewArgs(pVm,pSelfHint,&pFunc->sName,pFunc,` |
|        6 | 3793 | `						(sxu32)nPos,nReqGF,nNonVarGF,bCallSiteInMsg));` |
|       50 | 3794 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|        - | 3795 | `			/* Default value */` |
|       50 | 3796 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|       50 | 3797 | `			if( pObj ){` |
|       50 | 3798 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj,FALSE);` |
|       50 | 3799 | `				if( rc == SXERR_ABORT ){` |
|      ! 0 | 3800 | `					return rc;` |
|        - | 3801 | `				}` |
|       50 | 3802 | `				sSlot.nIdx = pObj->nIdx;` |
|       50 | 3803 | `				sSlot.pUserData = 0;` |
|       50 | 3804 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|        - | 3805 | `				/* Held to the type like a passed argument (php's RECV_INIT; see` |
|        - | 3806 | `				 * the OP_CALL defaults loop). */` |
|       50 | 3807 | `				rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,pObj,bStrict,pSelfHint);` |
|       50 | 3808 | `				if( rc != SXRET_OK ){` |
|        3 | 3809 | `					return rc;` |
|        - | 3810 | `				}` |
|       22 | 3811 | `			}` |
|       22 | 3812 | `		}` |
|      128 | 3813 | `	}` |
|        - | 3814 | `	/* Install closure environment (captured variables) */` |
|     1139 | 3815 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|        - | 3816 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|        - | 3817 | `		ph7_value *pValue;` |
|        - | 3818 | `		sxu32 iEnv;` |
|      321 | 3819 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      753 | 3820 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|      437 | 3821 | `			pEnv = &aEnv[iEnv];` |
|      437 | 3822 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|      293 | 3823 | `				continue;` |
|        - | 3824 | `			}` |
|      144 | 3825 | `			if( (pClosureThis \|\| pExecCtx->bUnboundThis) && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|       79 | 3826 | `			 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|        - | 3827 | `				/* An explicit bound $this (bindTo/bind/call) wins over the` |
|        - | 3828 | `				 * creation-time captured $this, php-exact (mirrors OP_CALL) --` |
|        - | 3829 | `				 * and so does an explicit unbind. */` |
|        7 | 3830 | `				continue;` |
|        - | 3831 | `			}` |
|      142 | 3832 | `			if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|        - | 3833 | `				/* Captured by reference: link the name to the shared slot` |
|        - | 3834 | `				 * (no copy), mirroring the OP_CALL env install. */` |
|        8 | 3835 | `				if( SyHashGet(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|       11 | 3836 | `					SyHashInsert(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),` |
|        6 | 3837 | `						SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|        3 | 3838 | `				}` |
|        8 | 3839 | `				continue;` |
|        - | 3840 | `			}` |
|      136 | 3841 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|      136 | 3842 | `			if( pValue == 0 ){` |
|      ! 0 | 3843 | `				continue;` |
|        - | 3844 | `			}` |
|      136 | 3845 | `			PH7_MemObjRelease(pValue);` |
|      136 | 3846 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|       70 | 3847 | `		}` |
|      158 | 3848 | `	}` |
|     1139 | 3849 | `	return SXRET_OK;` |
|      607 | 3850 | `}` |
|        - | 3851 | `/*` |
|        - | 3852 | `` * The class a fiber body's parameter types resolve `self` against, and its diagnostics`` |
|        - | 3853 | ` * are qualified by: OP_CALL's rule -- a method's DECLARING class, or for a trait's` |
|        - | 3854 | ` * method (one struct shared by every user) the receiver's. Fiber::start() passed none,` |
|        - | 3855 | ``  * so a method body's `self` parameter admitted anything and its TypeError read `m()` `` |
|        - | 3856 | `` * where php's reads `K::m()`.`` |
|        - | 3857 | ` */` |
|      424 | 3858 | `static ph7_class *VmFiberSelfHint(ph7_vm_func *pFunc, ph7_class_instance *pThis)` |
|        5 | 3859 | `{` |
|      429 | 3860 | `	if( pFunc->iFlags & VM_FUNC_CLASS_METHOD ){` |
|       42 | 3861 | `		ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|       42 | 3862 | `		if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){` |
|       38 | 3863 | `			return pDecl;` |
|        - | 3864 | `		}` |
|        5 | 3865 | `		if( pThis ){` |
|        5 | 3866 | `			return pThis->pClass;` |
|        - | 3867 | `		}` |
|      ! 0 | 3868 | `	}` |
|      391 | 3869 | `	return 0;` |
|      217 | 3870 | `}` |
|        - | 3871 | `/*` |
|        - | 3872 | ` * A fiber's body is entered by Fiber::start(), an INTERNAL function, and an embedder's` |
|        - | 3873 | ` * by no PHP code at all: php's prev_execute_data is not user code in either case, so an` |
|        - | 3874 | `` * argument TypeError raised while binding names no `, called in FILE on line N`. The`` |
|        - | 3875 | ` * ordinary call records that on the frame (VM_FRAME_NATIVE_CALLER) and the type error` |
|        - | 3876 | ` * reads it there; this frame says so only while its arguments are bound, because the` |
|        - | 3877 | ` * body it then runs is resumed from many call sites and a trace taken inside it is its` |
|        - | 3878 | ` * own question.` |
|        - | 3879 | ` */` |
|     1204 | 3880 | `PH7_PRIVATE sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|        - | 3881 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg,` |
|        - | 3882 | `	const SyString *aArgName,` |
|        - | 3883 | `	int bStrict, ph7_class *pSelfHint, int bCallSiteInMsg, int bAliasByRef)` |
|        5 | 3884 | `{` |
|     1209 | 3885 | `	VmFrame *pFrame = pExecCtx->pFrame;` |
|     1209 | 3886 | `	int bMark = !bCallSiteInMsg && (pFrame->iFlags & VM_FRAME_NATIVE_CALLER) == 0;` |
|        - | 3887 | `	sxi32 rc;` |
|     1209 | 3888 | `	if( bMark ){` |
|      415 | 3889 | `		pFrame->iFlags \|= VM_FRAME_NATIVE_CALLER;` |
|      205 | 3890 | `	}` |
|     1811 | 3891 | `	rc = VmFiberBindFrame(pVm,pExecCtx,pClosureThis,nArg,apArg,aArgName,` |
|      602 | 3892 | `		bStrict,pSelfHint,bCallSiteInMsg,bAliasByRef);` |
|     1209 | 3893 | `	if( bMark ){` |
|      415 | 3894 | `		pFrame->iFlags &= ~VM_FRAME_NATIVE_CALLER;` |
|      205 | 3895 | `	}` |
|     1209 | 3896 | `	return rc;` |
|        5 | 3897 | `}` |
|        - | 3898 | `#ifdef PH7_CORO_STACK` |
|        - | 3899 | `/*` |
|        - | 3900 | ` * Fiber::start() over a body that is CALLED rather than run as a coroutine` |
|        - | 3901 | ` * (sFiberTrampFunc). Nothing is bound into the ctx frame: the callee binds its own` |
|        - | 3902 | ` * arguments -- names, by-reference warnings, type checks and all -- exactly as a` |
|        - | 3903 | ` * callback dispatched by an internal function does, which is what php's fiber is.` |
|        - | 3904 | ` * The ctx frame is marked transparent so no walk ever reports it.` |
|        - | 3905 | ` */` |
|       50 | 3906 | `static int VmFiberStartTramp(ph7_context *pCtx, ph7_class_instance *pThis,` |
|        - | 3907 | `	int nArg, ph7_value **apArg)` |
|        3 | 3908 | `{` |
|       53 | 3909 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 3910 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 3911 | `	ph7_value *pCallable, *pCtxAttr;` |
|        - | 3912 | `	ph7_value sResult;` |
|        - | 3913 | `	SyString sAttrName;` |
|        - | 3914 | `	sxi32 rc;` |
|        - | 3915 | `	int k;` |
|       53 | 3916 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|       53 | 3917 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       53 | 3918 | `	pExecCtx = pCallable ? VmNewExecCtx(pVm, &sFiberTrampFunc) : 0;` |
|       53 | 3919 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 3920 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start(): out of memory");` |
|        - | 3921 | `	}` |
|       53 | 3922 | `	pExecCtx->pFrame->iFlags \|= VM_FRAME_EXCEPTION;` |
|       53 | 3923 | `	VmFiberStampEntry(pExecCtx, &sFiberStartName, pThis);` |
|       53 | 3924 | `	pExecCtx->bTramp = 1;` |
|       53 | 3925 | `	PH7_MemObjInit(pVm, &pExecCtx->sTramp);` |
|       53 | 3926 | `	PH7_MemObjStore(pCallable, &pExecCtx->sTramp);` |
|       53 | 3927 | `	if( nArg > 0 ){` |
|       71 | 3928 | `		pExecCtx->aTrampArg = (ph7_value *)SyMemBackendAlloc(&pVm->sAllocator,` |
|       34 | 3929 | `			(sxu32)nArg * sizeof(ph7_value));` |
|       37 | 3930 | `		if( pExecCtx->aTrampArg == 0 ){` |
|      ! 0 | 3931 | `			VmReleaseExecCtx(pVm, pExecCtx);` |
|      ! 0 | 3932 | `			return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start(): out of memory");` |
|        - | 3933 | `		}` |
|       85 | 3934 | `		for( k = 0 ; k < nArg ; ++k ){` |
|       51 | 3935 | `			PH7_MemObjInit(pVm, &pExecCtx->aTrampArg[k]);` |
|       51 | 3936 | `			PH7_MemObjStore(apArg[k], &pExecCtx->aTrampArg[k]);` |
|       27 | 3937 | `		}` |
|       37 | 3938 | `		pExecCtx->nTrampArg = (sxu32)nArg;` |
|       17 | 3939 | `	}` |
|       53 | 3940 | `	if( pCtx->pArgMap && pCtx->pArgMap->bHasNamed && pCtx->pArgMap->nTotal >= (sxu32)nArg ){` |
|        3 | 3941 | `		pExecCtx->pTrampMap = pCtx->pArgMap;` |
|        1 | 3942 | `	}` |
|       53 | 3943 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|       53 | 3944 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|       53 | 3945 | `	if( pCtxAttr ){` |
|       53 | 3946 | `		pCtxAttr->x.pOther = pExecCtx;` |
|       53 | 3947 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|       25 | 3948 | `	}` |
|       53 | 3949 | `	PH7_MemObjInit(pVm, &sResult);` |
|        - | 3950 | `	{` |
|       53 | 3951 | `		ph7_class_instance *pOldFiber = pVm->pCurFiber;` |
|       53 | 3952 | `		pVm->pCurFiber = pThis;` |
|       53 | 3953 | `		rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|       53 | 3954 | `		pVm->pCurFiber = pOldFiber;` |
|        - | 3955 | `	}` |
|       53 | 3956 | `	pExecCtx->pTrampMap = 0;` |
|       53 | 3957 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 3958 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 3959 | `		return PH7_ABORT;` |
|        - | 3960 | `	}` |
|       53 | 3961 | `	if( rc == PH7_EXCEPTION ){` |
|       15 | 3962 | `		PH7_MemObjRelease(&sResult);` |
|       15 | 3963 | `		if( pExecCtx->pEscaped ){` |
|       15 | 3964 | `			return VmFiberRaiseEscaped(pCtx, pExecCtx);` |
|        - | 3965 | `		}` |
|      ! 0 | 3966 | `		return PH7_EXCEPTION;` |
|        - | 3967 | `	}` |
|       41 | 3968 | `	ph7_result_value(pCtx, &sResult);` |
|       41 | 3969 | `	PH7_MemObjRelease(&sResult);` |
|       41 | 3970 | `	return PH7_OK;` |
|       28 | 3971 | `}` |
|        - | 3972 | `#endif` |
|        - | 3973 | `/*` |
|        - | 3974 | ` * A fiber whose arguments were refused never ran a line, but php refused them from` |
|        - | 3975 | ` * inside it: started, terminated, and getReturn() says it threw. Left CREATED, the` |
|        - | 3976 | ` * fiber answered isStarted() false and a second start() ran the body -- or refused it` |
|        - | 3977 | ` * as already started, which contradicted the first answer.` |
|        - | 3978 | ` */` |
|       52 | 3979 | `static int VmFiberRefusedStart(ph7_exec_ctx *pExecCtx, int rc)` |
|        3 | 3980 | `{` |
|       55 | 3981 | `	pExecCtx->iState = PH7_CTX_STATE_CLOSED;` |
|       55 | 3982 | `	if( rc == PH7_EXCEPTION ){` |
|       55 | 3983 | `		pExecCtx->bThrew = 1;` |
|       26 | 3984 | `	}` |
|       55 | 3985 | `	return rc;` |
|        3 | 3986 | `}` |
|        - | 3987 | `/*` |
|        - | 3988 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|        - | 3989 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|        - | 3990 | ` *` |
|        - | 3991 | ` * As a native VARIADIC method the arguments arrive directly as (nArg, apArg);` |
|        - | 3992 | ` * the prelude used to hand them over as a single func_get_args() array, which` |
|        - | 3993 | ` * this had to walk and snapshot out of pVm->aMemObj.` |
|        - | 3994 | ` */` |
|      500 | 3995 | `PH7_PRIVATE int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 3996 | `{` |
|      505 | 3997 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 3998 | `	ph7_class_instance *pThis;` |
|        - | 3999 | `	ph7_class_instance *pClosureThis;` |
|        - | 4000 | `	int bUnbound;` |
|        - | 4001 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 4002 | `	ph7_vm_func *pFunc;` |
|        - | 4003 | `	ph7_value sResult;` |
|        - | 4004 | `	ph7_value *pCtxAttr;` |
|        - | 4005 | `	SyString sAttrName;` |
|      505 | 4006 | `	ph7_value **apBound = 0;    /* a named call's layout (VmCtxBindNamedArgs) */` |
|      505 | 4007 | `	SyString *aBoundName = 0;   /* ...and its extras' names, in the same block */` |
|      505 | 4008 | `	sxi32 iHole = -1;           /* ...or the required formal it left unbound */` |
|        - | 4009 | `	sxi32 rc;` |
|      505 | 4010 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      505 | 4011 | `	if( pRecv == 0 \|\| (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 4012 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|        - | 4013 | `	}` |
|      505 | 4014 | `	pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|        - | 4015 | `	/* Check if already started (has a __ctx) */` |
|      505 | 4016 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|      505 | 4017 | `	if( pExecCtx != 0 ){` |
|       18 | 4018 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 4019 | `			"Cannot start a fiber that has already been started");` |
|        - | 4020 | `	}` |
|        - | 4021 | `	/* Resolve callable */` |
|      489 | 4022 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis, &bUnbound);` |
|      489 | 4023 | `	if( pFunc == 0 ){` |
|      ! 0 | 4024 | `		return PH7_EXCEPTION;` |
|        - | 4025 | `	}` |
|        - | 4026 | `#ifdef PH7_CORO_STACK` |
|      489 | 4027 | `	if( pFunc == &sFiberTrampFunc ){` |
|       53 | 4028 | `		return VmFiberStartTramp(pCtx, pThis, nArg, apArg);` |
|        - | 4029 | `	}` |
|        - | 4030 | `#endif` |
|        - | 4031 | ``	/* Fiber::start()'s own `...$args` are by VALUE whatever the body declares, so php`` |
|        - | 4032 | `		 * warns for every by-reference parameter and the body operates on a copy — the` |
|        - | 4033 | `		 * value PHL already produced, without the one diagnostic that says so. Named off` |
|        - | 4034 | `		 * the stored callable, which is what carries the class for a method one. */` |
|      439 | 4035 | `	if( nArg > 0 ){` |
|        - | 4036 | `		SyString sCbName;` |
|        - | 4037 | `		ph7_value *pCbVal;` |
|       91 | 4038 | `		SyStringInitFromBuf(&sCbName, "__callable", 10);` |
|       91 | 4039 | `		pCbVal = PH7_ClassInstanceFetchAttr(pThis, &sCbName);` |
|       91 | 4040 | `		if( pCbVal ){` |
|      134 | 4041 | `			PH7_VmWarnByRefArgsGivenValue(pVm, pCbVal, nArg, 0,` |
|       86 | 4042 | `				(pCtx->pArgMap && pCtx->pArgMap->bHasNamed && pCtx->pArgMap->nTotal >= (sxu32)nArg)` |
|       42 | 4043 | `					? pCtx->pArgMap->aNames : 0);` |
|       43 | 4044 | `		}` |
|       43 | 4045 | `	}` |
|        - | 4046 | `	/* Create execution context now that we know the function, and store it in` |
|        - | 4047 | `	 * $this->__ctx before a single argument is bound: php binds them INSIDE the` |
|        - | 4048 | `	 * fiber, so every argument refusal below is the body throwing on its first` |
|        - | 4049 | `	 * switch -- the fiber is started and dead from then on (VmFiberRefusedStart). */` |
|      439 | 4050 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|      439 | 4051 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 4052 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 4053 | `			"Fiber::start(): out of memory");` |
|        - | 4054 | `	}` |
|      439 | 4055 | `	pExecCtx->bUnboundThis = (sxu8)bUnbound;` |
|      439 | 4056 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      439 | 4057 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      439 | 4058 | `	if( pCtxAttr ){` |
|      439 | 4059 | `		pCtxAttr->x.pOther = pExecCtx;` |
|      439 | 4060 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      217 | 4061 | `	}` |
|        - | 4062 | ``	/* Fiber::start() is one of php's forwards: its `...$args` collects the call's`` |
|        - | 4063 | `	 * named arguments WITH their names and binds them to the body's parameters the` |
|        - | 4064 | ``	 * way a direct call would. Every name used to be dropped here, so `start(b: 1,`` |
|        - | 4065 | ``	 * a: 2)` bound positionally and an extra reached the variadic unkeyed. A named`` |
|        - | 4066 | `	 * hole is raised on the body's frame, so it is thrown further down. */` |
|      439 | 4067 | `	if( nArg > 0 && pCtx->pArgMap && pCtx->pArgMap->bHasNamed ){` |
|       46 | 4068 | `		sxu32 nSpan = (sxu32)nArg + SySetUsed(&pFunc->aArgs);` |
|       46 | 4069 | `		int nBound = 0;` |
|       67 | 4070 | `		apBound = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,` |
|       21 | 4071 | `			nSpan * (sizeof(ph7_value *) + sizeof(SyString)));` |
|       46 | 4072 | `		if( apBound ){` |
|       46 | 4073 | `			aBoundName = (SyString *)&apBound[nSpan];` |
|       67 | 4074 | `			rc = VmCtxBindNamedArgs(pVm, pFunc, pCtx->pArgMap, (sxu32)nArg, apArg,` |
|       21 | 4075 | `				apBound, aBoundName, &nBound, &iHole);` |
|       46 | 4076 | `			if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|       12 | 4077 | `				SyMemBackendFree(&pVm->sAllocator, apBound);` |
|       12 | 4078 | `				return VmFiberRefusedStart(pExecCtx, rc);` |
|        - | 4079 | `			}` |
|       36 | 4080 | `			if( rc == SXRET_OK ){` |
|       36 | 4081 | `				apArg = apBound;` |
|       36 | 4082 | `				nArg = nBound;` |
|       20 | 4083 | `			}else{` |
|      ! 0 | 4084 | `				aBoundName = 0; /* out of memory: the positional binding stands */` |
|        - | 4085 | `			}` |
|       16 | 4086 | `		}` |
|       16 | 4087 | `	}` |
|        - | 4088 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|        - | 4089 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|        - | 4090 | `	 * into the fiber's frame, not the caller's. */` |
|        - | 4091 | `	/* A TypeError raised while its arguments are bound is already the body's, so its` |
|        - | 4092 | `	 * trace shows the Fiber->start() frame at this site too. */` |
|      429 | 4093 | `	VmFiberStampEntry(pExecCtx, &sFiberStartName, pThis);` |
|      429 | 4094 | `	VmStampCoroutineCallSite(pVm, pExecCtx);` |
|      429 | 4095 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|      429 | 4096 | `	pVm->pFrame = pExecCtx->pFrame;` |
|        - | 4097 | `	/* Unpack the args array and install into the frame */` |
|        - | 4098 | `	{` |
|        - | 4099 | `		/* The arguments are this call's own operand-stack slots, so they can be` |
|        - | 4100 | `		 * handed to the frame setup as-is. The old form had to snapshot them out of` |
|        - | 4101 | `		 * pVm->aMemObj first, because they arrived as a func_get_args() hashmap` |
|        - | 4102 | `		 * whose element values live in that pool — and VmFiberSetupFrame reserves` |
|        - | 4103 | `		 * memory objects (VmExtractMemObj) before reading its arguments, which back` |
|        - | 4104 | `		 * then reallocated the pool and dangled a raw pointer into it. Operand slots` |
|        - | 4105 | `		 * do not move, so the copy went with the array that made it necessary. (Pool` |
|        - | 4106 | `		 * slots do not move either since P1, which retires the hazard entirely.) */` |
|      429 | 4107 | `		ph7_value **apValues = (nArg > 0) ? apArg : 0;` |
|      429 | 4108 | `		int nActual = nArg;` |
|      429 | 4109 | `		if( iHole >= 0 ){` |
|        - | 4110 | `			/* A named hole is the body's refusal as well: raised on its frame. */` |
|       17 | 4111 | `			rc = VmCtxThrowNamedHole(pVm, pFunc, VmFiberSelfHint(pFunc, pClosureThis), iHole);` |
|       10 | 4112 | `		}else{` |
|      620 | 4113 | `			rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues, aBoundName,` |
|        - | 4114 | `				0 /* weak-mode arg binding, like call_user_func */,` |
|      205 | 4115 | `				VmFiberSelfHint(pFunc, pClosureThis),` |
|        - | 4116 | `				FALSE/*Fiber::start(): php omits the call-site segment*/,` |
|        - | 4117 | `				FALSE/*php's Fiber::start() passes by VALUE and warns (recorded)*/);` |
|        - | 4118 | `		}` |
|        - | 4119 | `		/* apValues aliases the operand stack, or the named layout above, whose` |
|        - | 4120 | `		 * entries alias it too: only the layout's own block is freed. */` |
|      429 | 4121 | `		if( apBound ){` |
|       36 | 4122 | `			SyMemBackendFree(&pVm->sAllocator, apBound);` |
|       16 | 4123 | `		}` |
|        - | 4124 | `	}` |
|        - | 4125 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|      429 | 4126 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|      429 | 4127 | `	pExecCtx->pFrame->pParent = 0;` |
|      429 | 4128 | `	if( rc != SXRET_OK ){` |
|        - | 4129 | `		/* Propagate the real status: a declared-type TypeError from the arg` |
|        - | 4130 | `		 * install (band A #2) must stay catchable, not become an abort. */` |
|       45 | 4131 | `		return VmFiberRefusedStart(pExecCtx, (rc == PH7_EXCEPTION) ? PH7_EXCEPTION : PH7_ABORT);` |
|        - | 4132 | `	}` |
|      387 | 4133 | `	PH7_MemObjInit(pVm, &sResult);` |
|        - | 4134 | `	{` |
|        - | 4135 | `		/* php's EG(active_fiber): the fiber the running code is INSIDE, which is what` |
|        - | 4136 | `		 * Fiber::getCurrent() answers. Saved and restored around the body run, so a` |
|        - | 4137 | `		 * fiber that starts another one nests, and a fiber that finishes hands the` |
|        - | 4138 | `		 * name back to whoever was current before it. Borrowed for the duration: the` |
|        - | 4139 | `		 * receiver of this call owns the reference. */` |
|      387 | 4140 | `		ph7_class_instance *pOldFiber = pVm->pCurFiber;` |
|      387 | 4141 | `		pVm->pCurFiber = pThis;` |
|      387 | 4142 | `		rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|      387 | 4143 | `		pVm->pCurFiber = pOldFiber;` |
|        - | 4144 | `	}` |
|      387 | 4145 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 4146 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 4147 | `		return PH7_ABORT;` |
|        - | 4148 | `	}` |
|      387 | 4149 | `	if( rc == PH7_EXCEPTION ){` |
|        8 | 4150 | `		PH7_MemObjRelease(&sResult);` |
|        - | 4151 | `#ifdef PH7_CORO_STACK` |
|        8 | 4152 | `		if( pExecCtx->pEscaped ){` |
|        8 | 4153 | `			return VmFiberRaiseEscaped(pCtx, pExecCtx);` |
|        - | 4154 | `		}` |
|        - | 4155 | `#endif` |
|      ! 0 | 4156 | `		return PH7_EXCEPTION;` |
|        - | 4157 | `	}` |
|      381 | 4158 | `	ph7_result_value(pCtx, &sResult);` |
|      381 | 4159 | `	PH7_MemObjRelease(&sResult);` |
|      381 | 4160 | `	return PH7_OK;` |
|      255 | 4161 | `}` |
|        - | 4162 | `/*` |
|        - | 4163 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|        - | 4164 | ` */` |
|      230 | 4165 | `PH7_PRIVATE int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 4166 | `{` |
|      235 | 4167 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 4168 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 4169 | `	ph7_value sResult;` |
|        - | 4170 | `	ph7_value *pResumeVal;` |
|        - | 4171 | `	sxi32 rc;` |
|      235 | 4172 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      235 | 4173 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|      235 | 4174 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 4175 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|      ! 0 | 4176 | `		return PH7_OK;` |
|        - | 4177 | `	}` |
|      235 | 4178 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|      235 | 4179 | `	if( pExecCtx == 0 ){` |
|      ! 0 | 4180 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|      ! 0 | 4181 | `		return PH7_OK;` |
|        - | 4182 | `	}` |
|      235 | 4183 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       15 | 4184 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 4185 | `			"Cannot resume a fiber that is not suspended");` |
|        - | 4186 | `	}` |
|      221 | 4187 | `	pResumeVal = (nArg > 0) ? apArg[0] : 0;` |
|      221 | 4188 | `	PH7_MemObjInit(pVm, &sResult);` |
|        - | 4189 | `	{` |
|        - | 4190 | `		/* See Fiber::start(): the current fiber is this one for the length of the run. */` |
|      221 | 4191 | `		ph7_class_instance *pOldFiber = pVm->pCurFiber;` |
|      221 | 4192 | `		pVm->pCurFiber = (ph7_class_instance *)pRecv->x.pOther;` |
|      221 | 4193 | `		VmFiberStampEntry(pExecCtx, &sFiberResumeName, pVm->pCurFiber);` |
|      221 | 4194 | `		rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|      221 | 4195 | `		pVm->pCurFiber = pOldFiber;` |
|        - | 4196 | `	}` |
|      221 | 4197 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 4198 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 4199 | `		return PH7_ABORT;` |
|        - | 4200 | `	}` |
|      221 | 4201 | `	if( rc == PH7_EXCEPTION ){` |
|        6 | 4202 | `		PH7_MemObjRelease(&sResult);` |
|        - | 4203 | `#ifdef PH7_CORO_STACK` |
|        6 | 4204 | `		if( pExecCtx->pEscaped ){` |
|        6 | 4205 | `			return VmFiberRaiseEscaped(pCtx, pExecCtx);` |
|        - | 4206 | `		}` |
|        - | 4207 | `#endif` |
|      ! 0 | 4208 | `		return PH7_EXCEPTION;` |
|        - | 4209 | `	}` |
|      217 | 4210 | `	ph7_result_value(pCtx, &sResult);` |
|      217 | 4211 | `	PH7_MemObjRelease(&sResult);` |
|      217 | 4212 | `	return PH7_OK;` |
|      120 | 4213 | `}` |
|        - | 4214 | `/*` |
|        - | 4215 | ` * Fiber->throw(Throwable $exception) — resume the fiber by RAISING at its` |
|        - | 4216 | `` * suspension point, so `Fiber::suspend()` throws instead of returning. Same`` |
|        - | 4217 | ` * transport as Generator::throw(): the exception is parked on the context and` |
|        - | 4218 | ` * the resumed body raises it in its own frame at the top of the dispatch loop,` |
|        - | 4219 | ` * which is what lets a try/catch INSIDE the fiber catch it and carry on. The` |
|        - | 4220 | ` * answer is the next suspend value, or null if the body ran to completion --` |
|        - | 4221 | ` * symmetric with resume(). Every non-suspended state is php's one sentence.` |
|        - | 4222 | ` */` |
|       26 | 4223 | `PH7_PRIVATE int vm_builtin_Fiber_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        3 | 4224 | `{` |
|       29 | 4225 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 4226 | `	ph7_exec_ctx *pExecCtx;` |
|        - | 4227 | `	ph7_class_instance *pInj;` |
|        - | 4228 | `	ph7_value sResult;` |
|        - | 4229 | `	sxi32 rc;` |
|       29 | 4230 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       29 | 4231 | `	if( pRecv == 0 \|\| (pRecv->iFlags & MEMOBJ_OBJ) == 0 \|\| nArg < 1 ){` |
|      ! 0 | 4232 | `		return PH7_OK;` |
|        - | 4233 | `	}` |
|       29 | 4234 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 4235 | ``		return PH7_OK; /* the declared `Throwable $exception` screen already spoke */`` |
|        - | 4236 | `	}` |
|       29 | 4237 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|       29 | 4238 | `	if( pExecCtx == 0 \|\| pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        - | 4239 | `		/* php answers the same sentence for never-started, running and terminated. */` |
|       18 | 4240 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 4241 | `			"Cannot resume a fiber that is not suspended");` |
|        - | 4242 | `	}` |
|        - | 4243 | `	/* Hold a reference for the whole operation: the resumed body may bind the` |
|        - | 4244 | `	 * instance in a catch and release it again before we are back. */` |
|       12 | 4245 | `	pInj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       12 | 4246 | `	pInj->iRef++;` |
|       12 | 4247 | `	pExecCtx->pInjected = pInj;   /* borrowed; consumed at the resume's loop top */` |
|       12 | 4248 | `	PH7_MemObjInit(pVm, &sResult);` |
|        - | 4249 | `	{` |
|        - | 4250 | `		/* See Fiber::start(): the current fiber is this one for the length of the run. */` |
|       12 | 4251 | `		ph7_class_instance *pOldFiber = pVm->pCurFiber;` |
|       12 | 4252 | `		pVm->pCurFiber = (ph7_class_instance *)pRecv->x.pOther;` |
|       12 | 4253 | `		VmFiberStampEntry(pExecCtx, &sFiberThrowName, pVm->pCurFiber);` |
|       12 | 4254 | `		rc = VmResumeCtx(pVm, pExecCtx, 0, &sResult);` |
|       12 | 4255 | `		pVm->pCurFiber = pOldFiber;` |
|        - | 4256 | `	}` |
|        - | 4257 | `	/* Normally consumed (cleared) at the loop top; clear it here too for the path` |
|        - | 4258 | `	 * where VmResumeCtx bails BEFORE entering the loop (the recursion-depth fatal),` |
|        - | 4259 | `	 * so no dangling borrowed pointer survives the Unref. */` |
|       12 | 4260 | `	pExecCtx->pInjected = 0;` |
|       12 | 4261 | `	PH7_ClassInstanceUnref(pInj);` |
|       12 | 4262 | `	if( rc == PH7_ABORT ){` |
|      ! 0 | 4263 | `		PH7_MemObjRelease(&sResult);` |
|      ! 0 | 4264 | `		return PH7_ABORT;` |
|        - | 4265 | `	}` |
|       12 | 4266 | `	if( rc == PH7_EXCEPTION ){` |
|        3 | 4267 | `		PH7_MemObjRelease(&sResult);` |
|        - | 4268 | `#ifdef PH7_CORO_STACK` |
|        3 | 4269 | `		if( pExecCtx->pEscaped ){` |
|        3 | 4270 | `			return VmFiberRaiseEscaped(pCtx, pExecCtx);` |
|        - | 4271 | `		}` |
|        - | 4272 | `#endif` |
|      ! 0 | 4273 | `		return PH7_EXCEPTION;` |
|        - | 4274 | `	}` |
|       10 | 4275 | `	ph7_result_value(pCtx, &sResult);` |
|       10 | 4276 | `	PH7_MemObjRelease(&sResult);` |
|       10 | 4277 | `	return PH7_OK;` |
|       16 | 4278 | `}` |
|        - | 4279 | `/*` |
|        - | 4280 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|        - | 4281 | ` */` |
|      138 | 4282 | `PH7_PRIVATE int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 4283 | `{` |
|      143 | 4284 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 4285 | `	ph7_exec_ctx *pExecCtx;` |
|      143 | 4286 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       69 | 4287 | `	SXUNUSED(apArg);` |
|       69 | 4288 | `	SXUNUSED(nArg);` |
|      143 | 4289 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|      143 | 4290 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 4291 | `		ph7_result_null(pCtx);` |
|      ! 0 | 4292 | `		return PH7_OK;` |
|        - | 4293 | `	}` |
|      143 | 4294 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|      143 | 4295 | `	if( pExecCtx == 0 ){` |
|        - | 4296 | `		/* No context at all IS the never-started state -- the fiber's __ctx slot is` |
|        - | 4297 | `		 * filled by start(). php names it rather than answering null. */` |
|        3 | 4298 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 4299 | `			"Cannot get fiber return value: The fiber has not been started");` |
|        - | 4300 | `	}` |
|      141 | 4301 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|       18 | 4302 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|      ! 0 | 4303 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 4304 | `				"Cannot get fiber return value: The fiber has not been started");` |
|        - | 4305 | `		}` |
|       18 | 4306 | `		if( pExecCtx->bThrew ){` |
|        - | 4307 | `			/* Terminated, but with nothing to hand back: php's own third sentence. */` |
|       16 | 4308 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 4309 | `				"Cannot get fiber return value: The fiber threw an exception");` |
|        - | 4310 | `		}` |
|        3 | 4311 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|        - | 4312 | `			"Cannot get fiber return value: The fiber has not returned");` |
|        - | 4313 | `	}` |
|      125 | 4314 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|      125 | 4315 | `	return PH7_OK;` |
|       74 | 4316 | `}` |
|        - | 4317 | `/*` |
|        - | 4318 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|        - | 4319 | ` */` |
|       38 | 4320 | `PH7_PRIVATE int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        4 | 4321 | `{` |
|        - | 4322 | `	ph7_exec_ctx *pExecCtx;` |
|       42 | 4323 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       19 | 4324 | `	SXUNUSED(apArg);` |
|       19 | 4325 | `	SXUNUSED(nArg);` |
|       42 | 4326 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       42 | 4327 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|       42 | 4328 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|       42 | 4329 | `	return PH7_OK;` |
|       23 | 4330 | `}` |
|       30 | 4331 | `PH7_PRIVATE int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        2 | 4332 | `{` |
|        - | 4333 | `	ph7_exec_ctx *pExecCtx;` |
|       32 | 4334 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       15 | 4335 | `	SXUNUSED(apArg);` |
|       15 | 4336 | `	SXUNUSED(nArg);` |
|       32 | 4337 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       32 | 4338 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|       32 | 4339 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|       32 | 4340 | `	return PH7_OK;` |
|       17 | 4341 | `}` |
|      178 | 4342 | `PH7_PRIVATE int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 4343 | `{` |
|        - | 4344 | `	ph7_exec_ctx *pExecCtx;` |
|      183 | 4345 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       89 | 4346 | `	SXUNUSED(apArg);` |
|       89 | 4347 | `	SXUNUSED(nArg);` |
|      183 | 4348 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      183 | 4349 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|      183 | 4350 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|      183 | 4351 | `	return PH7_OK;` |
|       94 | 4352 | `}` |
|      106 | 4353 | `PH7_PRIVATE int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 4354 | `{` |
|        - | 4355 | `	ph7_exec_ctx *pExecCtx;` |
|      111 | 4356 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       53 | 4357 | `	SXUNUSED(apArg);` |
|       53 | 4358 | `	SXUNUSED(nArg);` |
|      111 | 4359 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      111 | 4360 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|        - | 4361 | `	/* php's DEAD state: a body that returned and a body that let an exception` |
|        - | 4362 | `	 * escape are both terminated -- only getReturn() tells them apart. */` |
|      187 | 4363 | `	ph7_result_bool(pCtx, pExecCtx && (pExecCtx->iState == PH7_CTX_STATE_COMPLETED` |
|       76 | 4364 | `		\|\| pExecCtx->iState == PH7_CTX_STATE_CLOSED));` |
|      111 | 4365 | `	return PH7_OK;` |
|       58 | 4366 | `}` |
|        - | 4367 | `/*` |
|        - | 4368 | ` * Fiber->__destruct() — clean up the execution context.` |
|        - | 4369 | ` */` |
|      494 | 4370 | `PH7_PRIVATE int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 4371 | `{` |
|      499 | 4372 | `	ph7_vm *pVm = pCtx->pVm;` |
|        - | 4373 | `	ph7_exec_ctx *pExecCtx;` |
|      499 | 4374 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      247 | 4375 | `	SXUNUSED(apArg);` |
|      247 | 4376 | `	SXUNUSED(nArg);` |
|      499 | 4377 | `	if( pRecv == 0 ){` |
|      ! 0 | 4378 | `		return PH7_OK;` |
|        - | 4379 | `	}` |
|      499 | 4380 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|      499 | 4381 | `	if( pExecCtx ){` |
|      489 | 4382 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|        - | 4383 | `		/* Clear the attribute so double-free is prevented */` |
|      489 | 4384 | `		if( pRecv->iFlags & MEMOBJ_OBJ ){` |
|      489 | 4385 | `			ph7_class_instance *pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|        - | 4386 | `			SyString sAttrName;` |
|        - | 4387 | `			ph7_value *pAttr;` |
|      489 | 4388 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      489 | 4389 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      489 | 4390 | `			if( pAttr ){` |
|      489 | 4391 | `				PH7_MemObjRelease(pAttr);` |
|      242 | 4392 | `			}` |
|      242 | 4393 | `		}` |
|      242 | 4394 | `	}` |
|      499 | 4395 | `	return PH7_OK;` |
|      252 | 4396 | `}` |
|        - | 4397 | `/* ======================== Fiber Public API Helpers ======================== */` |
|      ! 0 | 4398 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|      ! 0 | 4399 | `{` |
|        - | 4400 | `	ph7_class_instance *pThis;` |
|      ! 0 | 4401 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|      ! 0 | 4402 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|      ! 0 | 4403 | `	return pThis->pClass == pVm->pFiberClass;` |
|      ! 0 | 4404 | `}` |
|      ! 0 | 4405 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|      ! 0 | 4406 | `{` |
|        - | 4407 | `	ph7_class_instance *pThis;` |
|      ! 0 | 4408 | `	ph7_class_instance *pClosureThis = 0;` |
|        - | 4409 | `	ph7_exec_ctx *pCtx;` |
|        - | 4410 | `	ph7_vm_func *pFunc;` |
|        - | 4411 | `	ph7_value *pCallable;` |
|        - | 4412 | `	ph7_value *pCtxAttr;` |
|        - | 4413 | `	SyString sAttrName;` |
|        - | 4414 | `	sxi32 rc;` |
|        - | 4415 | `	/* Must not already be started */` |
|      ! 0 | 4416 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 4417 | `	if( pCtx != 0 ){` |
|      ! 0 | 4418 | `		return SXERR_INVALID;` |
|        - | 4419 | `	}` |
|      ! 0 | 4420 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 4421 | `		return SXERR_INVALID;` |
|        - | 4422 | `	}` |
|      ! 0 | 4423 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|        - | 4424 | `	/* Get the callable */` |
|      ! 0 | 4425 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|      ! 0 | 4426 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 4427 | `	if( pCallable == 0 ){` |
|      ! 0 | 4428 | `		return SXERR_INVALID;` |
|        - | 4429 | `	}` |
|        - | 4430 | `	/* Resolve callable, through the same body-finder the PHP-level start() uses --` |
|        - | 4431 | `	 * these were two copies of one decision, and only the other one grew php's array` |
|        - | 4432 | `	 * and "Class::method" shapes. An embedder has no context to throw through, so the` |
|        - | 4433 | `	 * reason comes back as this entry point's own status. */` |
|      ! 0 | 4434 | `	if( pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP) ){` |
|      ! 0 | 4435 | `		const char *zWhy = 0;` |
|      ! 0 | 4436 | `		pFunc = VmFiberCallableBody(pVm, pCallable, &pClosureThis, &zWhy);` |
|      ! 0 | 4437 | `		if( pFunc == 0 ){` |
|      ! 0 | 4438 | `			return SXERR_NOTFOUND;` |
|      ! 0 | 4439 | `		}` |
|      ! 0 | 4440 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|      ! 0 | 4441 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|      ! 0 | 4442 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|        - | 4443 | `			sizeof("__invoke") - 1);` |
|      ! 0 | 4444 | `		if( pMethod == 0 ){` |
|      ! 0 | 4445 | `			return SXERR_INVALID;` |
|        - | 4446 | `		}` |
|      ! 0 | 4447 | `		pClosureThis = pClosure;` |
|      ! 0 | 4448 | `		pFunc = &pMethod->sFunc;` |
|      ! 0 | 4449 | `	}else{` |
|      ! 0 | 4450 | `		return SXERR_INVALID;` |
|        - | 4451 | `	}` |
|        - | 4452 | `	/* Create context */` |
|      ! 0 | 4453 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|      ! 0 | 4454 | `	if( pCtx == 0 ){` |
|      ! 0 | 4455 | `		return SXERR_MEM;` |
|        - | 4456 | `	}` |
|        - | 4457 | `	/* Store in __ctx */` |
|      ! 0 | 4458 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      ! 0 | 4459 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      ! 0 | 4460 | `	if( pCtxAttr ){` |
|      ! 0 | 4461 | `		pCtxAttr->x.pOther = pCtx;` |
|      ! 0 | 4462 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|      ! 0 | 4463 | `	}` |
|        - | 4464 | `	/* Set up frame with args */` |
|      ! 0 | 4465 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|      ! 0 | 4466 | `	pVm->pFrame = pCtx->pFrame;` |
|      ! 0 | 4467 | `	rc = VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg, 0,` |
|      ! 0 | 4468 | `		0 /* weak-mode arg binding (embedder entry) */, VmFiberSelfHint(pFunc, pClosureThis),` |
|        - | 4469 | `		FALSE/*embedder entry: no userland call site*/,` |
|        - | 4470 | `		FALSE/*no source-level actuals to alias*/);` |
|      ! 0 | 4471 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|      ! 0 | 4472 | `	pCtx->pFrame->pParent = 0;` |
|      ! 0 | 4473 | `	if( rc != SXRET_OK ){` |
|      ! 0 | 4474 | `		return rc;` |
|        - | 4475 | `	}` |
|      ! 0 | 4476 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|      ! 0 | 4477 | `}` |
|      ! 0 | 4478 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|      ! 0 | 4479 | `{` |
|      ! 0 | 4480 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 4481 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|      ! 0 | 4482 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|      ! 0 | 4483 | `}` |
|      ! 0 | 4484 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 4485 | `{` |
|      ! 0 | 4486 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 4487 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|      ! 0 | 4488 | `}` |
|      ! 0 | 4489 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 4490 | `{` |
|      ! 0 | 4491 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 4492 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|      ! 0 | 4493 | `}` |
|      ! 0 | 4494 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|      ! 0 | 4495 | `{` |
|      ! 0 | 4496 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|      ! 0 | 4497 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|      ! 0 | 4498 | `	return &pCtx->sRetValue;` |
|      ! 0 | 4499 | `}` |
|        - | 4500 | `/* ======================== Generator Infrastructure ======================== */` |
|        - | 4501 | `/*` |
|        - | 4502 | ` * Allocate a new generator wrapper around an execution context.` |
|        - | 4503 | ` */` |
|      808 | 4504 | `PH7_PRIVATE ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|        5 | 4505 | `{` |
|        - | 4506 | `	ph7_generator *pGen;` |
|      813 | 4507 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|      813 | 4508 | `	if( pGen == 0 ){` |
|      ! 0 | 4509 | `		return 0;` |
|        - | 4510 | `	}` |
|      813 | 4511 | `	SyZero(pGen, sizeof(ph7_generator));` |
|      813 | 4512 | `	pGen->pCtx = pCtx;` |
|      813 | 4513 | `	pGen->iImplicitKey = 0;` |
|      813 | 4514 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|      813 | 4515 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|        - | 4516 | `	/* Link the generator back to the exec context */` |
|      813 | 4517 | `	pCtx->pPrivate = pGen;` |
|      813 | 4518 | `	return pGen;` |
|      409 | 4519 | `}` |
|        - | 4520 | `/*` |
|        - | 4521 | ` * Release a generator and its execution context.` |
|        - | 4522 | ` */` |
|      806 | 4523 | `PH7_PRIVATE void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|        5 | 4524 | `{` |
|      811 | 4525 | `	if( pGen == 0 ){` |
|      ! 0 | 4526 | `		return;` |
|        - | 4527 | `	}` |
|      811 | 4528 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|      811 | 4529 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|      811 | 4530 | `	if( pGen->pCtx ){` |
|      811 | 4531 | `		pGen->pCtx->pPrivate = 0;` |
|      811 | 4532 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|      811 | 4533 | `		pGen->pCtx = 0;` |
|      403 | 4534 | `	}` |
|      811 | 4535 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|      408 | 4536 | `}` |
|        - | 4537 | `/*` |
|        - | 4538 | ` * Extract ph7_generator from a Generator class instance.` |
|        - | 4539 | ` */` |
|     6606 | 4540 | `PH7_PRIVATE ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|        5 | 4541 | `{` |
|        - | 4542 | `	ph7_class_instance *pThis;` |
|        - | 4543 | `	SyString sAttr;` |
|        - | 4544 | `	ph7_value *pAttr;` |
|     6611 | 4545 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      ! 0 | 4546 | `		return 0;` |
|        - | 4547 | `	}` |
|     6611 | 4548 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|     6611 | 4549 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|      ! 0 | 4550 | `		return 0;` |
|        - | 4551 | `	}` |
|     6611 | 4552 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|     6611 | 4553 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     6611 | 4554 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 4555 | `		return 0;` |
|        - | 4556 | `	}` |
|     6611 | 4557 | `	return (ph7_generator *)pAttr->x.pOther;` |
|     3308 | 4558 | `}` |
|        - | 4559 | `/*` |
|        - | 4560 | ` * php's zend_generator_ensure_initialized, which EVERY accessor on the class runs` |
|        - | 4561 | ` * first: a generator that has never executed is run to its first yield before it` |
|        - | 4562 | `` * is asked anything. `current()`/`key()`/`rewind()` did that here and the rest did`` |
|        - | 4563 | `` * not, so `valid()` answered FALSE for a generator sitting at its first yield —`` |
|        - | 4564 | `` * `while ($g->valid())` never entered the loop — and `next()`/`send()` did the`` |
|        - | 4565 | ` * PRIMING run and called it the advance, which delivers the first element twice` |
|        - | 4566 | `` * and drops what `send()` was given.`` |
|        - | 4567 | ` */` |
|     5790 | 4568 | `static sxi32 VmGeneratorEnsureInit(ph7_vm *pVm, ph7_generator *pGen)` |
|        5 | 4569 | `{` |
|        - | 4570 | `	sxi32 rc;` |
|     5795 | 4571 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|     5069 | 4572 | `		return PH7_OK;` |
|        - | 4573 | `	}` |
|      731 | 4574 | `	rc = VmStartCtx(pVm, pGen->pCtx, 0);` |
|      731 | 4575 | `	if( rc == PH7_OK ){` |
|        - | 4576 | `		/* php sets the flag on the INITIALIZING run itself, whatever it settled` |
|        - | 4577 | `		 * on — so a generator whose body never yields is still rewindable, and` |
|        - | 4578 | `		 * only a later resume takes the flag away. */` |
|      701 | 4579 | `		pGen->bAtFirstYield = 1;` |
|      348 | 4580 | `	}` |
|      731 | 4581 | `	return rc;` |
|     2900 | 4582 | `}` |
|        - | 4583 | `/*` |
|        - | 4584 | ` * Fetch the ph7_generator behind a Generator INSTANCE (the ph7_value-taking` |
|        - | 4585 | ` * VmGeneratorExtractCtx is the same lookup from the other side).` |
|        - | 4586 | ` */` |
|     1950 | 4587 | `static ph7_generator * VmGeneratorFromInstance(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        5 | 4588 | `{` |
|        - | 4589 | `	SyString sAttr;` |
|        - | 4590 | `	ph7_value *pAttr;` |
|     1955 | 4591 | `	if( pThis == 0 \|\| pVm->pGeneratorClass == 0 \|\| pThis->pClass != pVm->pGeneratorClass ){` |
|     1433 | 4592 | `		return 0;` |
|        - | 4593 | `	}` |
|      527 | 4594 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|      527 | 4595 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|      527 | 4596 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|      ! 0 | 4597 | `		return 0;` |
|        - | 4598 | `	}` |
|      527 | 4599 | `	return (ph7_generator *)pAttr->x.pOther;` |
|      980 | 4600 | `}` |
|        - | 4601 | `/*` |
|        - | 4602 | ``  * Run a generator to its first yield if it has never executed. `yield from` `` |
|        - | 4603 | ` * needs this and NOT a rewind: php links the delegate as a child node and only` |
|        - | 4604 | ` * initializes it, so delegating to a generator that is already suspended` |
|        - | 4605 | ` * half-way CONTINUES from where it stands.` |
|        - | 4606 | ` */` |
|       50 | 4607 | `PH7_PRIVATE sxi32 PH7_VmGeneratorPrime(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        5 | 4608 | `{` |
|       55 | 4609 | `	ph7_generator *pGen = VmGeneratorFromInstance(pVm, pThis);` |
|       55 | 4610 | `	if( pGen == 0 \|\| pGen->pCtx == 0 ){` |
|      ! 0 | 4611 | `		return PH7_OK;` |
|        - | 4612 | `	}` |
|       55 | 4613 | `	return VmGeneratorEnsureInit(pVm, pGen);` |
|       30 | 4614 | `}` |
|        - | 4615 | `/*` |
|        - | 4616 | ` * Whether a generator instance has already run to its end. php's` |
|        - | 4617 | ` * zend_generator_get_iterator refuses to start a foreach over one` |
|        - | 4618 | ` * ("Cannot traverse an already closed generator") BEFORE the rewind that would` |
|        - | 4619 | ` * otherwise report the coarser "already run" message.` |
|        - | 4620 | ` */` |
|     1900 | 4621 | `PH7_PRIVATE int PH7_VmGeneratorIsClosed(ph7_vm *pVm, ph7_class_instance *pThis)` |
|        5 | 4622 | `{` |
|     1905 | 4623 | `	ph7_generator *pGen = VmGeneratorFromInstance(pVm, pThis);` |
|     1905 | 4624 | `	if( pGen == 0 \|\| pGen->pCtx == 0 ){` |
|     1433 | 4625 | `		return 0;` |
|        - | 4626 | `	}` |
|      709 | 4627 | `	return pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED` |
|      472 | 4628 | `		\|\| pGen->pCtx->iState == PH7_CTX_STATE_CLOSED;` |
|      955 | 4629 | `}` |
|        - | 4630 | `/*` |
|        - | 4631 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|        - | 4632 | ` */` |
|      454 | 4633 | `PH7_PRIVATE int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 4634 | `{` |
|        - | 4635 | `	ph7_generator *pGen;` |
|        - | 4636 | `	sxi32 rc;` |
|      459 | 4637 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      227 | 4638 | `	SXUNUSED(apArg);` |
|      227 | 4639 | `	SXUNUSED(nArg);` |
|      459 | 4640 | `	if( pRecv == 0 ) return PH7_OK;` |
|      459 | 4641 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|      459 | 4642 | `	if( pGen == 0 ) return PH7_OK;` |
|      459 | 4643 | `	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|      459 | 4644 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      459 | 4645 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      435 | 4646 | `	if( !pGen->bAtFirstYield ){` |
|        - | 4647 | `		/* php: a generator is not rewindable, so rewind() is only allowed to mean` |
|        - | 4648 | `		 * "initialize". Once the body has moved past its first yield — or run to` |
|        - | 4649 | `		 * the end — php refuses, and PHL accepted in silence, so a foreach over a` |
|        - | 4650 | `		 * partly consumed generator carried on from where it stood while php` |
|        - | 4651 | `		 * stopped the program. */` |
|        8 | 4652 | `		return PH7_VmThrowException(pCtx, "Exception",` |
|        - | 4653 | `			"Cannot rewind a generator that was already run");` |
|        - | 4654 | `	}` |
|      429 | 4655 | `	return PH7_OK;` |
|      232 | 4656 | `}` |
|        - | 4657 | `/*` |
|        - | 4658 | ` * Generator::valid() — true if suspended at a yield point.` |
|        - | 4659 | ` */` |
|     1770 | 4660 | `PH7_PRIVATE int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 4661 | `{` |
|        - | 4662 | `	ph7_generator *pGen;` |
|     1775 | 4663 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      885 | 4664 | `	SXUNUSED(apArg);` |
|      885 | 4665 | `	SXUNUSED(nArg);` |
|     1775 | 4666 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|     1775 | 4667 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     1775 | 4668 | `	if( pGen ){` |
|     1775 | 4669 | `		sxi32 rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|     1775 | 4670 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     1775 | 4671 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      885 | 4672 | `	}` |
|     1775 | 4673 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|     1775 | 4674 | `	return PH7_OK;` |
|      890 | 4675 | `}` |
|        - | 4676 | `/*` |
|        - | 4677 | ` * Generator::current() — return the last yielded value.` |
|        - | 4678 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 4679 | ` */` |
|     1600 | 4680 | `PH7_PRIVATE int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 4681 | `{` |
|        - | 4682 | `	ph7_generator *pGen;` |
|        - | 4683 | `	sxi32 rc;` |
|     1605 | 4684 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      800 | 4685 | `	SXUNUSED(apArg);` |
|      800 | 4686 | `	SXUNUSED(nArg);` |
|     1605 | 4687 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     1605 | 4688 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     1605 | 4689 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     1605 | 4690 | `	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|     1605 | 4691 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     1605 | 4692 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     1605 | 4693 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     1603 | 4694 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|      804 | 4695 | `	}else{` |
|        3 | 4696 | `		ph7_result_null(pCtx);` |
|        - | 4697 | `	}` |
|     1605 | 4698 | `	return PH7_OK;` |
|      805 | 4699 | `}` |
|        - | 4700 | `/*` |
|        - | 4701 | ` * Generator::key() — return the last yielded key.` |
|        - | 4702 | ` * Auto-starts the generator on first access (like PHP).` |
|        - | 4703 | ` */` |
|      424 | 4704 | `PH7_PRIVATE int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 4705 | `{` |
|        - | 4706 | `	ph7_generator *pGen;` |
|        - | 4707 | `	sxi32 rc;` |
|      429 | 4708 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      212 | 4709 | `	SXUNUSED(apArg);` |
|      212 | 4710 | `	SXUNUSED(nArg);` |
|      429 | 4711 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      429 | 4712 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|      429 | 4713 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      429 | 4714 | `	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|      429 | 4715 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      429 | 4716 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      429 | 4717 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      429 | 4718 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|      217 | 4719 | `	}else{` |
|      ! 0 | 4720 | `		ph7_result_null(pCtx);` |
|        - | 4721 | `	}` |
|      429 | 4722 | `	return PH7_OK;` |
|      217 | 4723 | `}` |
|        - | 4724 | `/*` |
|        - | 4725 | ` * Generator::next() — advance to the next yield point.` |
|        - | 4726 | ` */` |
|     1256 | 4727 | `PH7_PRIVATE int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 4728 | `{` |
|        - | 4729 | `	ph7_generator *pGen;` |
|        - | 4730 | `	sxi32 rc;` |
|     1261 | 4731 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      628 | 4732 | `	SXUNUSED(apArg);` |
|      628 | 4733 | `	SXUNUSED(nArg);` |
|     1261 | 4734 | `	if( pRecv == 0 ) return PH7_OK;` |
|     1261 | 4735 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     1261 | 4736 | `	if( pGen == 0 ) return PH7_OK;` |
|        - | 4737 | `	/* PRIMING is not the advance: php runs a never-executed body to its first` |
|        - | 4738 | ``	 * yield and then still resumes past it, so `$g->next()` on a fresh generator`` |
|        - | 4739 | `	 * lands on the SECOND element. Treating the start as the advance handed the` |
|        - | 4740 | `	 * first one out twice. */` |
|     1261 | 4741 | `	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|     1261 | 4742 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     1261 | 4743 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     1261 | 4744 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|      ! 0 | 4745 | `		return PH7_OK;` |
|        - | 4746 | `	}` |
|     1261 | 4747 | `	pGen->bAtFirstYield = 0;` |
|     1261 | 4748 | `	rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|     1261 | 4749 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     1259 | 4750 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     1241 | 4751 | `	return PH7_OK;` |
|      633 | 4752 | `}` |
|        - | 4753 | `/*` |
|        - | 4754 | ` * Generator::send($value) — resume and send a value into the generator.` |
|        - | 4755 | ` */` |
|      142 | 4756 | `PH7_PRIVATE int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 4757 | `{` |
|        - | 4758 | `	ph7_generator *pGen;` |
|        - | 4759 | `	ph7_value *pSendVal;` |
|        - | 4760 | `	sxi32 rc;` |
|      147 | 4761 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      147 | 4762 | `	if( pRecv == 0 ) return PH7_OK;` |
|      147 | 4763 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|      147 | 4764 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      147 | 4765 | `	pSendVal = (nArg > 0) ? apArg[0] : 0;` |
|        - | 4766 | `	/* php PRIMES a never-executed generator and THEN resumes it with the value, so` |
|        - | 4767 | ``	 * a first `send('S')` reaches the first `yield`'s left-hand side and answers the`` |
|        - | 4768 | `	 * SECOND yielded value. Stopping at the priming run dropped the value entirely` |
|        - | 4769 | `	 * and answered the first — the whole point of a coroutine's first send. */` |
|      147 | 4770 | `	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|      147 | 4771 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      147 | 4772 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      147 | 4773 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|        3 | 4774 | `		ph7_result_null(pCtx);` |
|        3 | 4775 | `		return PH7_OK;` |
|        - | 4776 | `	}` |
|      145 | 4777 | `	pGen->bAtFirstYield = 0;` |
|      145 | 4778 | `	rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|      145 | 4779 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      145 | 4780 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      143 | 4781 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      117 | 4782 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       61 | 4783 | `	}else{` |
|       29 | 4784 | `		ph7_result_null(pCtx);` |
|        - | 4785 | `	}` |
|      143 | 4786 | `	return PH7_OK;` |
|       76 | 4787 | `}` |
|        - | 4788 | `/*` |
|        - | 4789 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|        - | 4790 | ` *` |
|        - | 4791 | ` * PHP semantics: the exception is injected AT the suspended yield point so the` |
|        - | 4792 | ` * generator's OWN try/catch (if any wraps the yield) can handle it and the body` |
|        - | 4793 | ` * resumes after the try; otherwise it propagates to the throw() caller and the` |
|        - | 4794 | ` * generator closes. We implement this by resuming the body with a pending` |
|        - | 4795 | ` * injection (pCtx->pInjected) that the VM loop raises in the body's own frame via` |
|        - | 4796 | ` * the existing OP_THROW / ROOT B resume route — no exception frame is` |
|        - | 4797 | ` * reconstructed on pVm->pFrame, so the return/finally unwind path is untouched.` |
|        - | 4798 | ` * A never-started generator is first run to its first yield, then injected there;` |
|        - | 4799 | ` * a finished/closed generator has no suspend point, so the exception is simply` |
|        - | 4800 | ` * propagated to the caller (its return value stays readable via getReturn()).` |
|        - | 4801 | ` *` |
|        - | 4802 | ` * PHL does not enforce the Throwable parameter hint (interface/class hints are` |
|        - | 4803 | ` * not checked on call), so the Throwable validation — and its PHP TypeError —` |
|        - | 4804 | ` * are done here.` |
|        - | 4805 | ` */` |
|       64 | 4806 | `PH7_PRIVATE int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        4 | 4807 | `{` |
|        - | 4808 | `	ph7_generator *pGen;` |
|        - | 4809 | `	ph7_class_instance *pInj;` |
|        - | 4810 | `	ph7_class *pThrowable;` |
|        - | 4811 | `	VmFrame *pFrame;` |
|        - | 4812 | `	sxi32 rc;` |
|       68 | 4813 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       68 | 4814 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|       68 | 4815 | `	if( nArg < 1 ) return PH7_OK;` |
|        - | 4816 | `	/* Argument #1 must be a Throwable; otherwise PHP raises a TypeError naming` |
|        - | 4817 | `	 * the given type (class name for objects, "null"/"string"/... for scalars). */` |
|       68 | 4818 | `	pThrowable = PH7_VmExtractClass(pCtx->pVm, "Throwable", sizeof("Throwable")-1, 0, 0);` |
|       64 | 4819 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|       68 | 4820 | `	 \|\| (pThrowable && !PH7_VmInstanceOf(((ph7_class_instance *)apArg[0]->x.pOther)->pClass, pThrowable)) ){` |
|        - | 4821 | `		char zCls[128];` |
|      ! 0 | 4822 | `		const char *zGiven = VmValueGivenName(apArg[0], zCls, sizeof(zCls));` |
|      ! 0 | 4823 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|      ! 0 | 4824 | `			"Generator::throw(): Argument #1 ($exception) must be of type Throwable, %s given", zGiven);` |
|        - | 4825 | `	}` |
|       68 | 4826 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|       68 | 4827 | `	if( pGen == 0 ) return PH7_OK;` |
|        - | 4828 | `	/* PHP forbids resuming/throwing into a generator that is currently executing. */` |
|       68 | 4829 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|        3 | 4830 | `		return PH7_VmThrowException(pCtx, "Error",` |
|        - | 4831 | `			"Cannot resume an already running generator");` |
|        - | 4832 | `	}` |
|        - | 4833 | `	/* Hold a reference to the injected instance for the whole operation: the VM loop` |
|        - | 4834 | `	 * (inject path) or VmThrowException (propagate path) may run catch blocks that bind` |
|        - | 4835 | `	 * and later release it. Dropped on every return path below. */` |
|       66 | 4836 | `	pInj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       66 | 4837 | `	pInj->iRef++;` |
|        - | 4838 | `	/* A never-started generator runs to its first yield, then the exception is injected` |
|        - | 4839 | `	 * there (PHP). Start it first; if it suspended at a yield, fall through to inject; if` |
|        - | 4840 | `	 * it ran to completion without yielding, drop through to the propagate path below. */` |
|        - | 4841 | `	{` |
|       66 | 4842 | `		rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|       66 | 4843 | `		if( rc == PH7_ABORT ){ PH7_ClassInstanceUnref(pInj); return PH7_ABORT; }` |
|       66 | 4844 | `		if( rc == PH7_EXCEPTION ){ PH7_ClassInstanceUnref(pInj); return PH7_EXCEPTION; }` |
|        - | 4845 | `	}` |
|       66 | 4846 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|        - | 4847 | `		/* Inject at the suspended yield: the resume loop raises it in the body's own` |
|        - | 4848 | `		 * frame so the generator's try/catch can catch it and resume (path 2). */` |
|       60 | 4849 | `		pGen->pCtx->pInjected = pInj;   /* borrowed; ref held here across the resume */` |
|       60 | 4850 | `		pGen->bAtFirstYield = 0;` |
|       60 | 4851 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|        - | 4852 | `		/* Normally the inject was consumed (cleared) at VmByteCodeExec entry; clear it` |
|        - | 4853 | `		 * here too for the path where VmResumeCtx bails BEFORE entering the loop (e.g. the` |
|        - | 4854 | `		 * recursion-depth fatal), so no dangling borrowed pointer survives the Unref. */` |
|       60 | 4855 | `		pGen->pCtx->pInjected = 0;` |
|       60 | 4856 | `		PH7_ClassInstanceUnref(pInj);` |
|       60 | 4857 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       60 | 4858 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        - | 4859 | `		/* Caught inside the generator and it resumed: return the next yielded value (or` |
|        - | 4860 | `		 * null if it then completed) — symmetric with Generator::send(). */` |
|       48 | 4861 | `		if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       45 | 4862 | `			ph7_result_value(pCtx, &pGen->sYieldValue);` |
|       24 | 4863 | `		}else{` |
|        3 | 4864 | `			ph7_result_null(pCtx);` |
|        - | 4865 | `		}` |
|       48 | 4866 | `		return PH7_OK;` |
|        - | 4867 | `	}` |
|        - | 4868 | `	/* COMPLETED/CLOSED (incl. a CREATED generator that ran to completion without a` |
|        - | 4869 | `	 * yield): no suspend point to inject at. Propagate the real object to the throw()` |
|        - | 4870 | `	 * caller through the normal dispatch path (class/message/trace preserved); its` |
|        - | 4871 | `	 * terminal state — and thus getReturn() — is left intact. */` |
|        8 | 4872 | `	pFrame = pCtx->pVm->pFrame;` |
|        8 | 4873 | `	if( pFrame ){` |
|        8 | 4874 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|        8 | 4875 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|        3 | 4876 | `	}` |
|        8 | 4877 | `	rc = VmThrowException(pCtx->pVm, pInj);` |
|        8 | 4878 | `	PH7_ClassInstanceUnref(pInj);` |
|        8 | 4879 | `	if( rc == SXERR_ABORT ){` |
|      ! 0 | 4880 | `		return PH7_ABORT;` |
|        - | 4881 | `	}` |
|        8 | 4882 | `	return PH7_EXCEPTION;` |
|       36 | 4883 | `}` |
|        - | 4884 | `/*` |
|        - | 4885 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|        - | 4886 | ` */` |
|       32 | 4887 | `PH7_PRIVATE int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        4 | 4888 | `{` |
|        - | 4889 | `	ph7_generator *pGen;` |
|       36 | 4890 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       16 | 4891 | `	SXUNUSED(apArg);` |
|       16 | 4892 | `	SXUNUSED(nArg);` |
|       36 | 4893 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       36 | 4894 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|       36 | 4895 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|        - | 4896 | `	{` |
|        - | 4897 | `		/* php initializes here too, so a generator asked for its return value` |
|        - | 4898 | `		 * before anything else has RUN its body up to the first yield — the side` |
|        - | 4899 | `		 * effects before that yield happen either way. */` |
|       36 | 4900 | `		sxi32 rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|       36 | 4901 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|       36 | 4902 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|        - | 4903 | `	}` |
|       36 | 4904 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|        - | 4905 | `		/* php's class here is Exception, not Error. */` |
|        3 | 4906 | `		return PH7_VmThrowException(pCtx, "Exception",` |
|        - | 4907 | `			"Cannot get return value of a generator that hasn't returned");` |
|        - | 4908 | `	}` |
|       34 | 4909 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|       34 | 4910 | `	return PH7_OK;` |
|       20 | 4911 | `}` |
|        - | 4912 | `/*` |
|        - | 4913 | ` * Generator::__destruct() — clean up.` |
|        - | 4914 | ` */` |
|      750 | 4915 | `PH7_PRIVATE int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|        5 | 4916 | `{` |
|        - | 4917 | `	ph7_generator *pGen;` |
|      755 | 4918 | `	sxi32 rcClose = SXRET_OK;` |
|      755 | 4919 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      375 | 4920 | `	SXUNUSED(apArg);` |
|      375 | 4921 | `	SXUNUSED(nArg);` |
|      755 | 4922 | `	if( pRecv == 0 ) return PH7_OK;` |
|      755 | 4923 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|      755 | 4924 | `	if( pGen ){` |
|        - | 4925 | `` 		/* A generator abandoned before it completes still runs its pending `finally` `` |
|        - | 4926 | `		 * blocks (PHP runs them at generator close/GC). Drive them before teardown. */` |
|      755 | 4927 | `		if( pGen->pCtx ){` |
|        - | 4928 | `			/* php closes it from a destructor HANDLER, not a method call, so the body's` |
|        - | 4929 | `			 * finally runs with the releasing code as its resumer and no` |
|        - | 4930 | ``			 * `Generator->__destruct()` frame between them. */`` |
|      755 | 4931 | `			VmNativeCall *pRec = pCtx->pVm->pNativeCall;` |
|      755 | 4932 | `			if( pRec && pRec->pClass == pCtx->pVm->pGeneratorClass ){` |
|      755 | 4933 | `				pRec->bElided = 1;` |
|      375 | 4934 | `			}` |
|      755 | 4935 | `			rcClose = VmCloseCtx(pCtx->pVm, pGen->pCtx);` |
|      375 | 4936 | `		}` |
|      755 | 4937 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|      755 | 4938 | `		if( pRecv->iFlags & MEMOBJ_OBJ ){` |
|      755 | 4939 | `			ph7_class_instance *pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|        - | 4940 | `			SyString sAttrName;` |
|        - | 4941 | `			ph7_value *pAttr;` |
|      755 | 4942 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|      755 | 4943 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|      755 | 4944 | `			if( pAttr ){` |
|      755 | 4945 | `				PH7_MemObjRelease(pAttr);` |
|      375 | 4946 | `			}` |
|      375 | 4947 | `		}` |
|      375 | 4948 | `	}` |
|        - | 4949 | `	/* Surface an abort/exception raised by a finally that ran during close. */` |
|      755 | 4950 | `	if( rcClose == PH7_ABORT ) return PH7_ABORT;` |
|      755 | 4951 | `	if( rcClose == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|      755 | 4952 | `	return PH7_OK;` |
|      380 | 4953 | `}` |
|        - | 4954 | `/* ======================== End Generator Infrastructure ======================== */` |
|        - | 4955 | `/* ======================== End Fiber Infrastructure ======================== */` |
|        - | 4956 |  |
