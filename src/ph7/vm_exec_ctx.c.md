# src/ph7/vm_exec_ctx.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1695/2019 lines (83.95%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `/* Darwin declares the ucontext routines only when _XOPEN_SOURCE is defined` |
|       - |    7 | ` * before the first system header; _DARWIN_C_SOURCE keeps the rest of the libc` |
|       - |    8 | ` * visible, which _XOPEN_SOURCE alone would hide. */` |
|       - |    9 | `#if defined(__APPLE__) && !defined(_XOPEN_SOURCE)` |
|       - |   10 | `#define _XOPEN_SOURCE 600` |
|       - |   11 | `#define _DARWIN_C_SOURCE 1` |
|       - |   12 | `#endif` |
|       - |   13 | `#include "ph7int.h"` |
|       - |   14 | `/*` |
|       - |   15 | ` * Section:` |
|       - |   16 | ` *    Execution contexts: the parked-stack machinery shared by Fibers and` |
|       - |   17 | ` *    Generators, closure creation/binding (Closure_* builtins), the` |
|       - |   18 | ` *    Fiber_* and Generator_* builtins and the PH7_VmFiber* API.` |
|       - |   19 | ` *    Registration happens via vm.c (aVmFunc[]/class installs).` |
|       - |   20 | ` * Status:` |
|       - |   21 | ` *    Stable.` |
|       - |   22 | ` */` |
|       - |   23 | `/*` |
|       - |   24 | ` * ---------------------------------------------------------------------------` |
|       - |   25 | ` * Real coroutine stacks: a fiber body on a native stack of its own.` |
|       - |   26 | ` * ---------------------------------------------------------------------------` |
|       - |   27 | ` * See the PH7_CORO_STACK block in ph7int.h for WHY. This is the machinery:` |
|       - |   28 | ` *` |
|       - |   29 | ` *   VmCoroNew / VmCoroFree        one switchable native stack` |
|       - |   30 | ` *   VmCoroEnter / VmCoroLeave     the switch itself, in both directions` |
|       - |   31 | ` *   VmCoroStateInit / Swap        the VM state that belongs to whichever runs` |
|       - |   32 | ` *   VmCoroRun                     the resumer's side of a start / resume / kill` |
|       - |   33 | ` *   VmCoroBody                    the fiber's side, entered once` |
|       - |   34 | ` *` |
|       - |   35 | ` * All of it compiles out where no stack-switch primitive exists, and a fiber` |
|       - |   36 | ` * that does not get a stack (an allocation failure; a generator, which never` |
|       - |   37 | ` * asks for one) falls back to the record-parking path. So the two models` |
|       - |   38 | `` * coexist and `pCtx->pCoro != 0` is the discriminator every caller tests.`` |
|       - |   39 | ` */` |
|       - |   40 | `#ifdef PH7_CORO_STACK` |
|       - |   41 | `#ifdef PH7_CORO_UCONTEXT` |
|       - |   42 | `#include <ucontext.h>` |
|       - |   43 | `#if defined(__APPLE__) && defined(__clang__)` |
|       - |   44 | `/* ...and marks every one of them deprecated, which -Werror turns into a` |
|       - |   45 | ` * failed build. */` |
|       - |   46 | `#pragma clang diagnostic ignored "-Wdeprecated-declarations"` |
|       - |   47 | `#endif` |
|       - |   48 | `#endif` |
|       - |   49 | `#if defined(PH7_CORO_UCONTEXT) \|\| defined(PH7_CORO_ASM_X64)` |
|       - |   50 | `#include <sys/mman.h>` |
|       - |   51 | `#include <unistd.h>` |
|       - |   52 | `#endif` |
|       - |   53 | `#ifdef PH7_CORO_WIN32` |
|       - |   54 | `#include <windows.h>` |
|       - |   55 | `#endif` |
|       - |   56 | `/* ASan keeps its own idea of where the stack is, and moves locals whose address` |
|       - |   57 | ` * escapes onto a "fake stack" it tracks per stack. A switch it is not told` |
|       - |   58 | ` * about makes it read the other side's frames as use-after-return, so every` |
|       - |   59 | ` * switch is announced. The protocol is the documented one: start_switch_fiber` |
|       - |   60 | ` * names where we are GOING and hands back a token; finish_switch_fiber runs on` |
|       - |   61 | ` * ARRIVAL with the token of the switch that got us there -- which, for a switch` |
|       - |   62 | ` * that eventually comes back, is a local of the function that made it. */` |
|       - |   63 | `#if defined(__has_feature)` |
|       - |   64 | `# if __has_feature(address_sanitizer)` |
|       - |   65 | `#  define PH7_CORO_ASAN 1` |
|       - |   66 | `# endif` |
|       - |   67 | `#endif` |
|       - |   68 | `#if defined(__SANITIZE_ADDRESS__) && !defined(PH7_CORO_ASAN)` |
|       - |   69 | `# define PH7_CORO_ASAN 1` |
|       - |   70 | `#endif` |
|       - |   71 | `#ifdef PH7_CORO_ASAN` |
|       - |   72 | `#include <sanitizer/common_interface_defs.h>` |
|       - |   73 | `#endif` |
|       - |   74 | `/*` |
|       - |   75 | ` * How big a fiber's native stack is, and why it is not a constant.` |
|       - |   76 | ` *` |
|       - |   77 | ` * A fiber's stack holds what the trampoline does NOT flatten -- eval/include` |
|       - |   78 | ` * towers, C->PHP callbacks, nested coroutine starts -- which is exactly what` |
|       - |   79 | ` * nMaxNativeDepth counts, and a fiber's depth counter starts at zero on its own` |
|       - |   80 | ` * stack. So the stack has to be big enough that the ENGINE's clean fatal` |
|       - |   81 | ` * ("Maximum native nesting depth reached") always fires before the guard page` |
|       - |   82 | ` * does; otherwise a deep enough callback tower inside a fiber is a SIGSEGV` |
|       - |   83 | ` * where the same tower outside one is a diagnostic.` |
|       - |   84 | ` *` |
|       - |   85 | ` * Hence: bytes per allowed level, times the cap. The per-level figure was` |
|       - |   86 | ``  * measured on the fattest thing in the tree -- a self-recursive `array_map` `` |
|       - |   87 | ` * callback, which spends a builtin frame, a native VmByteCodeExec activation` |
|       - |   88 | ` * and an operand stack on every level: ~5.7 KB at -O3 and ~14 KB under` |
|       - |   89 | ` * ASan+UBSan at -O1 -g. 32 KB carries better than a 2x margin over the worse of` |
|       - |   90 | ` * the two. At the host default cap of 256 that is 8 MB of ADDRESS SPACE per live` |
|       - |   91 | ` * fiber; both platforms map it lazily, so an idle fiber's resident cost is the` |
|       - |   92 | ` * page it is parked on.` |
|       - |   93 | ` */` |
|       - |   94 | `#ifndef PH7_CORO_STACK_PER_LEVEL` |
|       - |   95 | `#define PH7_CORO_STACK_PER_LEVEL (32 * 1024)` |
|       - |   96 | `#endif` |
|       - |   97 | `#ifndef PH7_CORO_STACK_MIN` |
|       - |   98 | `#define PH7_CORO_STACK_MIN (256 * 1024)` |
|       - |   99 | `#endif` |
|       - |  100 | `#ifndef PH7_CORO_STACK_MAX` |
|       - |  101 | `#define PH7_CORO_STACK_MAX (32 * 1024 * 1024)` |
|       - |  102 | `#endif` |
|     334 |  103 | `static sxu32 VmCoroStackBytes(ph7_vm *pVm)` |
|       5 |  104 | `{` |
|     673 |  105 | `	sxi64 nWant = (sxi64)(pVm->nMaxNativeDepth > 0 ? pVm->nMaxNativeDepth : 256)` |
|     334 |  106 | `		* (sxi64)PH7_CORO_STACK_PER_LEVEL;` |
|     339 |  107 | `	if( nWant < PH7_CORO_STACK_MIN ){` |
|     ! 0 |  108 | `		nWant = PH7_CORO_STACK_MIN;` |
|     ! 0 |  109 | `	}` |
|     339 |  110 | `	if( nWant > PH7_CORO_STACK_MAX ){` |
|     ! 0 |  111 | `		nWant = PH7_CORO_STACK_MAX;` |
|     ! 0 |  112 | `	}` |
|     339 |  113 | `	return (sxu32)nWant;` |
|       5 |  114 | `}` |
|       - |  115 | `struct VmCoro` |
|       - |  116 | `{` |
|       - |  117 | `#ifdef PH7_CORO_UCONTEXT` |
|       - |  118 | `	ucontext_t sBack;       /* the resumer's context: where a switch-out goes */` |
|       - |  119 | `	ucontext_t sSelf;       /* the fiber's own */` |
|       - |  120 | `#endif` |
|       - |  121 | `#ifdef PH7_CORO_ASM_X64` |
|       - |  122 | `	void *pBackSp;          /* the resumer's saved stack pointer (switch-out target) */` |
|       - |  123 | `	void *pSelfSp;          /* the fiber's own */` |
|       - |  124 | `#endif` |
|       - |  125 | `#if defined(PH7_CORO_UCONTEXT) \|\| defined(PH7_CORO_ASM_X64)` |
|       - |  126 | `	void *pMap;             /* mmap base: one guard page, then the stack */` |
|       - |  127 | `	sxu32 nMap;             /* its length, for munmap */` |
|       - |  128 | `#endif` |
|       - |  129 | `#ifdef PH7_CORO_WIN32` |
|       - |  130 | `	void *pFiber;           /* CreateFiber handle */` |
|       - |  131 | `	void *pBack;            /* the resumer's fiber handle, valid while we run */` |
|       - |  132 | `#endif` |
|       - |  133 | `	void *pStack;           /* usable stack, low address (0 where the platform owns it) */` |
|       - |  134 | `	sxu32 nStack;           /* usable stack bytes */` |
|       - |  135 | `#ifdef PH7_CORO_ASAN` |
|       - |  136 | `	const void *pHostStack; /* where the resumer's stack is: learned on arrival, */` |
|       - |  137 | `	sxu32 nHostStack;       /* and needed to announce the switch back to it */` |
|       - |  138 | `#endif` |
|       - |  139 | `};` |
|       - |  140 | `#ifdef PH7_CORO_ASAN` |
|       - |  141 | `/*` |
|       - |  142 | ` * Arriving on a stack. pTok closes the switch that brought us here (0 when the` |
|       - |  143 | ` * arrival is a fiber's FIRST entry: the enterer keeps its own token for its own` |
|       - |  144 | ` * return). The out-params say where we came from, which is the only way the` |
|       - |  145 | ` * fiber can learn the resumer's stack bounds to announce the switch back.` |
|       - |  146 | ` */` |
|       - |  147 | `static void VmCoroAsanArrive(VmCoro *pCoro, void *pTok)` |
|       - |  148 | `{` |
|       - |  149 | `	const void *pFrom = 0;` |
|       - |  150 | `	size_t nFrom = 0;` |
|       - |  151 | `	__sanitizer_finish_switch_fiber(pTok, &pFrom, &nFrom);` |
|       - |  152 | `	if( pCoro && pFrom ){` |
|       - |  153 | `		pCoro->pHostStack = pFrom;` |
|       - |  154 | `		pCoro->nHostStack = (sxu32)nFrom;` |
|       - |  155 | `	}` |
|       - |  156 | `}` |
|       - |  157 | `#define VM_CORO_ASAN_TOKEN            void *pTok = 0` |
|       - |  158 | `#define VM_CORO_ASAN_GO(TOK,BOT,SIZ)  __sanitizer_start_switch_fiber((TOK),(BOT),(size_t)(SIZ))` |
|       - |  159 | `#else` |
|       - |  160 | `#define VmCoroAsanArrive(C,T)         ((void)0)` |
|       - |  161 | `#define VM_CORO_ASAN_TOKEN            int iUnusedTok = 0; (void)iUnusedTok` |
|       - |  162 | `#define VM_CORO_ASAN_GO(TOK,BOT,SIZ)  ((void)0)` |
|       - |  163 | `#endif` |
|       - |  164 | `static void VmCoroBody(ph7_exec_ctx *pCtx);` |
|       - |  165 | `static ph7_vm_func * VmCtxEnforceRetFunc(ph7_exec_ctx *pCtx); /* defined below, with the rest of the shared coroutine epilogue */` |
|       - |  166 | `#ifdef PH7_CORO_ASM_X64` |
|       - |  167 | `/*` |
|       - |  168 | ` * The written switch, x86-64 System V.` |
|       - |  169 | ` *` |
|       - |  170 | ` * PH7_CoroSwitch(void **ppSave, void *pTarget) pushes the six callee-saved` |
|       - |  171 | ` * registers and the two floating-point control words, writes the resulting` |
|       - |  172 | ` * stack pointer through ppSave, adopts pTarget as the stack, and pops the` |
|       - |  173 | ` * mirror image -- so it "returns" wherever that other stack's saved frame says.` |
|       - |  174 | ` * That is the whole of a coroutine switch: everything else a C frame owns is` |
|       - |  175 | ` * already on the stack it lives on.` |
|       - |  176 | ` *` |
|       - |  177 | ` * PH7_CoroEntryStub is the address a FRESH stack's frame returns to. The seeded` |
|       - |  178 | ` * frame carries the ph7_exec_ctx in the r12 slot (a callee-saved register is` |
|       - |  179 | ` * the only place a value can ride through the switch), so the stub moves it` |
|       - |  180 | ` * into the first argument register and calls into C, which never comes back.` |
|       - |  181 | ` *` |
|       - |  182 | `` * `endbr64` on both so an IBT-enforcing loader is satisfied. A CET SHADOW stack`` |
|       - |  183 | ` * is the case this cannot serve -- a return to a frame the shadow stack never` |
|       - |  184 | ` * saw -- and PH7_DISABLE_CORO_ASM is the way back to glibc's swapcontext, which` |
|       - |  185 | ` * knows about it.` |
|       - |  186 | ` */` |
|       - |  187 | `extern void PH7_CoroSwitch(void **ppSave, void *pTarget);` |
|       - |  188 | `extern void PH7_CoroEntryStub(void);` |
|       - |  189 | `PH7_PRIVATE void PH7_CoroEntryC(ph7_exec_ctx *pCtx); /* named by the stub, so not static */` |
|       - |  190 | `__asm__(` |
|       - |  191 | `	".text\n"` |
|       - |  192 | `	".globl PH7_CoroSwitch\n"` |
|       - |  193 | `	".hidden PH7_CoroSwitch\n"` |
|       - |  194 | `	".type PH7_CoroSwitch,@function\n"` |
|       - |  195 | `	".align 16\n"` |
|       - |  196 | `	"PH7_CoroSwitch:\n"` |
|       - |  197 | `	"	endbr64\n"` |
|       - |  198 | `	"	pushq %rbp\n"` |
|       - |  199 | `	"	pushq %rbx\n"` |
|       - |  200 | `	"	pushq %r15\n"` |
|       - |  201 | `	"	pushq %r14\n"` |
|       - |  202 | `	"	pushq %r13\n"` |
|       - |  203 | `	"	pushq %r12\n"` |
|       - |  204 | `	"	subq $8, %rsp\n"` |
|       - |  205 | `	"	stmxcsr (%rsp)\n"` |
|       - |  206 | `	"	fnstcw 4(%rsp)\n"` |
|       - |  207 | `	"	movq %rsp, (%rdi)\n"` |
|       - |  208 | `	"	movq %rsi, %rsp\n"` |
|       - |  209 | `	"	ldmxcsr (%rsp)\n"` |
|       - |  210 | `	"	fldcw 4(%rsp)\n"` |
|       - |  211 | `	"	addq $8, %rsp\n"` |
|       - |  212 | `	"	popq %r12\n"` |
|       - |  213 | `	"	popq %r13\n"` |
|       - |  214 | `	"	popq %r14\n"` |
|       - |  215 | `	"	popq %r15\n"` |
|       - |  216 | `	"	popq %rbx\n"` |
|       - |  217 | `	"	popq %rbp\n"` |
|       - |  218 | `	"	ret\n"` |
|       - |  219 | `	".size PH7_CoroSwitch,.-PH7_CoroSwitch\n"` |
|       - |  220 | `	".globl PH7_CoroEntryStub\n"` |
|       - |  221 | `	".hidden PH7_CoroEntryStub\n"` |
|       - |  222 | `	".type PH7_CoroEntryStub,@function\n"` |
|       - |  223 | `	".align 16\n"` |
|       - |  224 | `	"PH7_CoroEntryStub:\n"` |
|       - |  225 | `	"	endbr64\n"` |
|       - |  226 | `	"	movq %r12, %rdi\n"` |
|       - |  227 | `	"	andq $-16, %rsp\n"` |
|       - |  228 | `	"	call PH7_CoroEntryC\n"` |
|       - |  229 | `	"	hlt\n"` |
|       - |  230 | `	".size PH7_CoroEntryStub,.-PH7_CoroEntryStub\n"` |
|       - |  231 | `);` |
|     167 |  232 | `PH7_PRIVATE void PH7_CoroEntryC(ph7_exec_ctx *pCtx)` |
|       - |  233 | `{` |
|     167 |  234 | `	VmCoroBody(pCtx);` |
|     ! 0 |  235 | `}` |
|       - |  236 | `/* Where the seeded frame's eight words sit, counted from the fiber's saved sp:` |
|       - |  237 | ` * the control-word pair, then r12 (which carries the ctx), r13, r14, r15, rbx,` |
|       - |  238 | `` * rbp, and finally the address PH7_CoroSwitch's `ret` will take. */`` |
|       - |  239 | `#define VM_CORO_X64_FRAME_WORDS  8` |
|       - |  240 | `#define VM_CORO_X64_SLOT_CTL     0` |
|       - |  241 | `#define VM_CORO_X64_SLOT_R12     1` |
|       - |  242 | `#define VM_CORO_X64_SLOT_RET     7` |
|       - |  243 | `#endif /* PH7_CORO_ASM_X64 */` |
|       - |  244 | `#ifdef PH7_CORO_UCONTEXT` |
|       - |  245 | `/*` |
|       - |  246 | ` * makecontext() passes int arguments only, so the context pointer travels as` |
|       - |  247 | ` * two of them -- the portable idiom, and the reason this is not a plain` |
|       - |  248 | ` * one-argument entry. A VM-wide "the fiber about to boot" field would be` |
|       - |  249 | ` * shorter and is not thread-safe, which PH7_ENABLE_THREADS builds care about.` |
|       - |  250 | ` */` |
|     167 |  251 | `static void VmCoroUcEntry(unsigned int iHi, unsigned int iLo)` |
|       - |  252 | `{` |
|     167 |  253 | `	sxu64 uPtr = (((sxu64)iHi) << 32) \| (sxu64)iLo;` |
|     167 |  254 | `	VmCoroBody((ph7_exec_ctx *)(size_t)uPtr);` |
|     167 |  255 | `}` |
|       - |  256 | `#endif` |
|       - |  257 | `#ifdef PH7_CORO_WIN32` |
|       - |  258 | `static VOID CALLBACK VmCoroWinEntry(PVOID pArg)` |
|       5 |  259 | `{` |
|       5 |  260 | `	VmCoroBody((ph7_exec_ctx *)pArg);` |
|     ! 0 |  261 | `}` |
|       - |  262 | `#endif` |
|       - |  263 | `/*` |
|       - |  264 | ` * Allocate one switchable stack. NULL is not fatal: the caller runs the body on` |
|       - |  265 | ` * the shared stack instead, where a suspend across a C boundary keeps raising` |
|       - |  266 | ` * the FiberError it always raised.` |
|       - |  267 | ` */` |
|     334 |  268 | `static VmCoro * VmCoroNew(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  269 | `{` |
|     339 |  270 | `	VmCoro *pCoro = (VmCoro *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(VmCoro));` |
|     339 |  271 | `	if( pCoro == 0 ){` |
|     ! 0 |  272 | `		return 0;` |
|       - |  273 | `	}` |
|     339 |  274 | `	SyZero(pCoro, sizeof(VmCoro));` |
|       - |  275 | `#if defined(PH7_CORO_UCONTEXT) \|\| defined(PH7_CORO_ASM_X64)` |
|       - |  276 | `	{` |
|     334 |  277 | `		long nPage = sysconf(_SC_PAGESIZE);` |
|     334 |  278 | `		sxu32 nGuard = (nPage > 0) ? (sxu32)nPage : 4096;` |
|     334 |  279 | `		sxu32 nStack = VmCoroStackBytes(pVm);` |
|       - |  280 | `		void *pMap;` |
|       - |  281 | `		/* One PROT_NONE page below the stack turns an overflow into a clean fault` |
|       - |  282 | `		 * at the guard rather than a silent write into whatever the allocator put` |
|       - |  283 | `		 * next door. nMaxNativeDepth is the engine's own net; this is the floor` |
|       - |  284 | `		 * under it. */` |
|     334 |  285 | `		pMap = mmap(0, (size_t)nGuard + (size_t)nStack, PROT_READ \| PROT_WRITE,` |
|       - |  286 | `			MAP_PRIVATE \| MAP_ANONYMOUS, -1, 0);` |
|     334 |  287 | `		if( pMap == MAP_FAILED ){` |
|     ! 0 |  288 | `			SyMemBackendFree(&pVm->sAllocator, pCoro);` |
|     ! 0 |  289 | `			return 0;` |
|       - |  290 | `		}` |
|     334 |  291 | `		(void)mprotect(pMap, (size_t)nGuard, PROT_NONE);` |
|     334 |  292 | `		pCoro->pMap = pMap;` |
|     334 |  293 | `		pCoro->nMap = nGuard + nStack;` |
|     334 |  294 | `		pCoro->pStack = (void *)((char *)pMap + nGuard);` |
|     334 |  295 | `		pCoro->nStack = nStack;` |
|       - |  296 | `	}` |
|       - |  297 | `#endif` |
|       - |  298 | `#ifdef PH7_CORO_ASM_X64` |
|       - |  299 | `	{` |
|       - |  300 | `		/* Seed the frame PH7_CoroSwitch will pop the first time this stack is` |
|       - |  301 | `		 * entered: the ctx in the r12 slot, the entry stub as the return address,` |
|       - |  302 | `		 * and THIS thread's floating-point control words so the fiber starts with` |
|       - |  303 | `		 * the rounding/precision modes its creator had. */` |
|     167 |  304 | `		void **aTop = (void **)(((size_t)pCoro->pStack + pCoro->nStack) & ~(size_t)15);` |
|     167 |  305 | `		void **aFrame = aTop - VM_CORO_X64_FRAME_WORDS;` |
|       - |  306 | `		unsigned int nMxcsr;` |
|       - |  307 | `		unsigned short nFcw;` |
|       - |  308 | `		sxu32 i;` |
|     167 |  309 | `		__asm__ __volatile__("stmxcsr %0" : "=m"(nMxcsr));` |
|     167 |  310 | `		__asm__ __volatile__("fnstcw %0" : "=m"(nFcw));` |
|    1503 |  311 | `		for( i = 0; i < VM_CORO_X64_FRAME_WORDS; i++ ){` |
|    1336 |  312 | `			aFrame[i] = 0;` |
|       - |  313 | `		}` |
|     167 |  314 | `		SyMemcpy((const void *)&nMxcsr, (void *)&aFrame[VM_CORO_X64_SLOT_CTL], sizeof(nMxcsr));` |
|     167 |  315 | `		SyMemcpy((const void *)&nFcw,` |
|       - |  316 | `			(void *)((char *)&aFrame[VM_CORO_X64_SLOT_CTL] + 4), sizeof(nFcw));` |
|     167 |  317 | `		aFrame[VM_CORO_X64_SLOT_R12] = (void *)pCtx;` |
|     167 |  318 | `		aFrame[VM_CORO_X64_SLOT_RET] = (void *)PH7_CoroEntryStub;` |
|     167 |  319 | `		pCoro->pSelfSp = (void *)aFrame;` |
|       - |  320 | `	}` |
|       - |  321 | `#endif` |
|       - |  322 | `#ifdef PH7_CORO_UCONTEXT` |
|       - |  323 | `	{` |
|       - |  324 | `		sxu64 uPtr;` |
|     167 |  325 | `		if( getcontext(&pCoro->sSelf) != 0 ){` |
|     ! 0 |  326 | `			munmap(pCoro->pMap, (size_t)pCoro->nMap);` |
|     ! 0 |  327 | `			SyMemBackendFree(&pVm->sAllocator, pCoro);` |
|     ! 0 |  328 | `			return 0;` |
|       - |  329 | `		}` |
|     167 |  330 | `		pCoro->sSelf.uc_stack.ss_sp = pCoro->pStack;` |
|     167 |  331 | `		pCoro->sSelf.uc_stack.ss_size = (size_t)pCoro->nStack;` |
|       - |  332 | `		/* uc_link stays NULL on purpose: the body never falls off its entry, it` |
|       - |  333 | `		 * makes the last switch itself (VmCoroBody), so that arrival is announced` |
|       - |  334 | `		 * to ASan like every other one. */` |
|     167 |  335 | `		pCoro->sSelf.uc_link = 0;` |
|     167 |  336 | `		uPtr = (sxu64)(size_t)pCtx;` |
|     334 |  337 | `		makecontext(&pCoro->sSelf, (void (*)(void))VmCoroUcEntry, 2,` |
|     167 |  338 | `			(unsigned int)(uPtr >> 32), (unsigned int)(uPtr & 0xFFFFFFFFu));` |
|       - |  339 | `	}` |
|       - |  340 | `#endif` |
|       - |  341 | `#ifdef PH7_CORO_WIN32` |
|       - |  342 | `	/* CreateFiber wants the calling THREAD to be a fiber before anything can be` |
|       - |  343 | `	 * switched to. That conversion is per-thread and is not undone: reversing it` |
|       - |  344 | `	 * is only safe with no fiber left alive anywhere, which one Fiber object` |
|       - |  345 | `	 * cannot know, and it costs an unconverted thread a few dozen bytes. */` |
|       5 |  346 | `	if( !IsThreadAFiber() ){` |
|       5 |  347 | `		if( ConvertThreadToFiber(0) == 0 ){` |
|     ! 0 |  348 | `			SyMemBackendFree(&pVm->sAllocator, pCoro);` |
|     ! 0 |  349 | `			return 0;` |
|       - |  350 | `		}` |
|       - |  351 | `	}` |
|       5 |  352 | `	pCoro->nStack = VmCoroStackBytes(pVm);` |
|       - |  353 | `	/* RESERVE the whole thing, COMMIT nothing (the 0): Windows grows a fiber` |
|       - |  354 | `	 * stack on demand exactly like a thread's, so the size above is address` |
|       - |  355 | `	 * space. FIBER_FLAG_FLOAT_SWITCH is what makes the switch carry the` |
|       - |  356 | `	 * floating-point state, which the written x86-64 switch does by hand. */` |
|       5 |  357 | `	pCoro->pFiber = (void *)CreateFiberEx((SIZE_T)0, (SIZE_T)pCoro->nStack,` |
|       - |  358 | `		FIBER_FLAG_FLOAT_SWITCH, VmCoroWinEntry, (LPVOID)pCtx);` |
|       5 |  359 | `	if( pCoro->pFiber == 0 ){` |
|     ! 0 |  360 | `		SyMemBackendFree(&pVm->sAllocator, pCoro);` |
|     ! 0 |  361 | `		return 0;` |
|       - |  362 | `	}` |
|       - |  363 | `	/* Win32 owns the mapping, so there is no bottom address to hand out -- and` |
|       - |  364 | `	 * the MSVC build has no ASan fiber annotations to hand it to. */` |
|       5 |  365 | `	pCoro->pStack = 0;` |
|       - |  366 | `#endif` |
|     339 |  367 | `	return pCoro;` |
|     172 |  368 | `}` |
|       - |  369 | `/*` |
|       - |  370 | ` * Give the stack back. Only ever reached with the fiber not running: its body` |
|       - |  371 | ` * ran off the end, or the teardown unwound it first (see the kill switch).` |
|       - |  372 | ` */` |
|     334 |  373 | `static void VmCoroFree(ph7_vm *pVm, VmCoro *pCoro)` |
|       5 |  374 | `{` |
|     339 |  375 | `	if( pCoro == 0 ){` |
|     ! 0 |  376 | `		return;` |
|       - |  377 | `	}` |
|       - |  378 | `#if defined(PH7_CORO_UCONTEXT) \|\| defined(PH7_CORO_ASM_X64)` |
|     334 |  379 | `	if( pCoro->pMap ){` |
|     334 |  380 | `		munmap(pCoro->pMap, (size_t)pCoro->nMap);` |
|     167 |  381 | `	}` |
|       - |  382 | `#endif` |
|       - |  383 | `#ifdef PH7_CORO_WIN32` |
|       5 |  384 | `	if( pCoro->pFiber ){` |
|       5 |  385 | `		DeleteFiber((LPVOID)pCoro->pFiber);` |
|       - |  386 | `	}` |
|       - |  387 | `#endif` |
|     339 |  388 | `	SyMemBackendFree(&pVm->sAllocator, pCoro);` |
|     172 |  389 | `}` |
|       - |  390 | `/*` |
|       - |  391 | ` * Switch onto the fiber's stack. Returns when the fiber switches back, because` |
|       - |  392 | ` * it suspended or because its body finished.` |
|       - |  393 | ` */` |
|     752 |  394 | `static void VmCoroEnter(VmCoro *pCoro)` |
|       5 |  395 | `{` |
|     757 |  396 | `	VM_CORO_ASAN_TOKEN;` |
|       - |  397 | `	VM_CORO_ASAN_GO(&pTok, pCoro->pStack, pCoro->nStack);` |
|       - |  398 | `#if defined(PH7_CORO_WIN32)` |
|       5 |  399 | `	pCoro->pBack = GetCurrentFiber();` |
|       5 |  400 | `	SwitchToFiber((LPVOID)pCoro->pFiber);` |
|       - |  401 | `#elif defined(PH7_CORO_ASM_X64)` |
|     376 |  402 | `	PH7_CoroSwitch(&pCoro->pBackSp, pCoro->pSelfSp);` |
|       - |  403 | `#else` |
|     376 |  404 | `	swapcontext(&pCoro->sBack, &pCoro->sSelf);` |
|       - |  405 | `#endif` |
|       - |  406 | `	VmCoroAsanArrive(0, pTok);` |
|     757 |  407 | `}` |
|       - |  408 | `/*` |
|       - |  409 | ` * ...and back, from inside the fiber. Returns when the fiber is entered again.` |
|       - |  410 | ` */` |
|     418 |  411 | `static void VmCoroLeave(VmCoro *pCoro)` |
|       5 |  412 | `{` |
|     423 |  413 | `	VM_CORO_ASAN_TOKEN;` |
|       - |  414 | `	VM_CORO_ASAN_GO(&pTok, pCoro->pHostStack, pCoro->nHostStack);` |
|       - |  415 | `#if defined(PH7_CORO_WIN32)` |
|       5 |  416 | `	SwitchToFiber((LPVOID)pCoro->pBack);` |
|       - |  417 | `#elif defined(PH7_CORO_ASM_X64)` |
|     209 |  418 | `	PH7_CoroSwitch(&pCoro->pSelfSp, pCoro->pBackSp);` |
|       - |  419 | `#else` |
|     209 |  420 | `	swapcontext(&pCoro->sSelf, &pCoro->sBack);` |
|       - |  421 | `#endif` |
|       - |  422 | `	VmCoroAsanArrive(pCoro, pTok);` |
|     423 |  423 | `}` |
|       - |  424 | `/*` |
|       - |  425 | ` * The last switch a fiber makes: the body is over, so the stack is spent and` |
|       - |  426 | ` * ASan is told to DISCARD this side's state rather than save it (the null` |
|       - |  427 | ` * token). Never returns.` |
|       - |  428 | ` */` |
|     334 |  429 | `static void VmCoroLeaveFinal(VmCoro *pCoro)` |
|       5 |  430 | `{` |
|       - |  431 | `	VM_CORO_ASAN_GO(0, pCoro->pHostStack, pCoro->nHostStack);` |
|       - |  432 | `#if defined(PH7_CORO_WIN32)` |
|       5 |  433 | `	SwitchToFiber((LPVOID)pCoro->pBack);` |
|       - |  434 | `#elif defined(PH7_CORO_ASM_X64)` |
|       - |  435 | `	{` |
|       - |  436 | `		/* Nothing on this stack will ever be resumed, so the save slot is a` |
|       - |  437 | `		 * scratch word rather than pSelfSp -- writing that would leave a live` |
|       - |  438 | `		 * frame pointer on a dead stack. */` |
|     167 |  439 | `		void *pDead = 0;` |
|     167 |  440 | `		PH7_CoroSwitch(&pDead, pCoro->pBackSp);` |
|       - |  441 | `	}` |
|       - |  442 | `#else` |
|     167 |  443 | `	setcontext(&pCoro->sBack);` |
|       - |  444 | `#endif` |
|     167 |  445 | `}` |
|       - |  446 | `/*` |
|       - |  447 | ` * The VM state that belongs to whichever side is running (see VmCoroVmState).` |
|       - |  448 | ` * One list, used three ways, so a field can never be saved and not restored.` |
|       - |  449 | ` */` |
|       - |  450 | `#define VM_CORO_STATE_FIELDS(_) \` |
|       - |  451 | `	_(aException) _(aFinallyAction) _(aSelf) \` |
|       - |  452 | `	_(nVmExecDepth) _(nRecursionDepth) _(nCurLine) _(nBoundaryRc) \` |
|       - |  453 | `	_(pCalleeName) _(pNativeFrameName) _(bHostDiscard) _(nErrSuppress) \` |
|       - |  454 | `	_(nExceptDepth) _(nExcCtorDepth) _(nMuteThrow) _(nSpeculative) \` |
|       - |  455 | `	_(nConstEvalDepth) _(nLazyInitLine) _(nLazyInitDepth) \` |
|       - |  456 | `	_(nObDepth) _(nObActive) _(pObFrame) _(pCoroCtx)` |
|       - |  457 | `/*` |
|       - |  458 | ` * Seed the fiber side for a body that has not run yet: three empty stacks of` |
|       - |  459 | ` * its own, a C stack nothing is live on, and every other scalar inherited from` |
|       - |  460 | ` * the site that is starting it -- which is what running the body inline used to` |
|       - |  461 | ` * give it.` |
|       - |  462 | ` */` |
|     334 |  463 | `static void VmCoroStateInit(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  464 | `{` |
|     339 |  465 | `	VmCoroVmState *pS = &pCtx->sSaved;` |
|       - |  466 | `#define VM_CORO_INHERIT(F)  pS->F = pVm->F;` |
|     339 |  467 | `	VM_CORO_STATE_FIELDS(VM_CORO_INHERIT)` |
|       - |  468 | `#undef VM_CORO_INHERIT` |
|     339 |  469 | `	SySetInit(&pS->aException, &pVm->sAllocator, sizeof(ph7_exception *));` |
|     339 |  470 | `	SySetInit(&pS->aFinallyAction, &pVm->sAllocator, sizeof(VmFinallyAction));` |
|     339 |  471 | `	SySetInit(&pS->aSelf, &pVm->sAllocator, sizeof(ph7_class *));` |
|       - |  472 | `	/* A fresh C stack: no native activation is live on it and no PHP call is` |
|       - |  473 | `	 * open, so both guards start from zero and measure THIS stack. */` |
|     339 |  474 | `	pS->nVmExecDepth = 0;` |
|     339 |  475 | `	pS->nRecursionDepth = 0;` |
|       - |  476 | `	/* Nothing of the resumer's in-flight C state is the fiber's: the parked` |
|       - |  477 | `	 * boundary throw belongs to the interrupted exec, the two callee-name latches` |
|       - |  478 | `	 * are consumed by the next call the RESUMER makes, and the lazy-initializer` |
|       - |  479 | `	 * line override is keyed on the other stack's native depth. */` |
|     339 |  480 | `	pS->nBoundaryRc = 0;` |
|     339 |  481 | `	pS->pCalleeName = 0;` |
|     339 |  482 | `	pS->pNativeFrameName = 0;` |
|     339 |  483 | `	pS->bHostDiscard = 0;` |
|     339 |  484 | `	pS->nLazyInitLine = 0;` |
|     339 |  485 | `	pS->nLazyInitDepth = 0;` |
|       - |  486 | `	/* ...and this side IS the fiber. */` |
|     339 |  487 | `	pS->pCoroCtx = pCtx;` |
|     339 |  488 | `}` |
|       - |  489 | `/*` |
|       - |  490 | ` * Release what the fiber side owns. Its three stacks die with the body: at a` |
|       - |  491 | ` * clean end they are empty, but a body that ABORTED can still be holding an` |
|       - |  492 | ` * unconsumed finally action (a queued return's value, a rethrow's exception` |
|       - |  493 | ` * reference) and the per-activation exception clones stage 2b made.` |
|       - |  494 | ` */` |
|    1366 |  495 | `static void VmCoroStateRelease(ph7_vm *pVm, VmCoroVmState *pS)` |
|       5 |  496 | `{` |
|    1371 |  497 | `	sxu32 n = SySetUsed(&pS->aFinallyAction);` |
|    1371 |  498 | `	if( n > 0 ){` |
|     ! 0 |  499 | `		VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pS->aFinallyAction);` |
|       - |  500 | `		sxu32 i;` |
|     ! 0 |  501 | `		for( i = 0; i < n; i++ ){` |
|     ! 0 |  502 | `			if( aA[i].eKind == PH7_FA_RETURN ){` |
|     ! 0 |  503 | `				PH7_MemObjRelease(&aA[i].sRet);` |
|     ! 0 |  504 | `			}else if( aA[i].eKind == PH7_FA_RETHROW && aA[i].pExc ){` |
|     ! 0 |  505 | `				PH7_ClassInstanceUnref(aA[i].pExc);` |
|     ! 0 |  506 | `			}` |
|     ! 0 |  507 | `		}` |
|     ! 0 |  508 | `	}` |
|    1371 |  509 | `	SySetRelease(&pS->aFinallyAction);` |
|    1371 |  510 | `	VmExcReleaseAll(pVm, &pS->aException);` |
|    1371 |  511 | `	SySetRelease(&pS->aException);` |
|    1371 |  512 | `	SySetRelease(&pS->aSelf);  /* borrowed class pointers */` |
|    1371 |  513 | `	SyZero(pS, sizeof(*pS));` |
|    1371 |  514 | `}` |
|       - |  515 | `/*` |
|       - |  516 | ` * Swap sides: what the VM holds now goes to *pOut, what *pIn holds becomes` |
|       - |  517 | ` * live. Called only from the RESUMER's stack, on both sides of the switch, so` |
|       - |  518 | ` * the fiber never has to know it is being saved.` |
|       - |  519 | ` */` |
|    1504 |  520 | `static void VmCoroStateSwap(ph7_vm *pVm, VmCoroVmState *pOut, VmCoroVmState *pIn)` |
|       5 |  521 | `{` |
|       - |  522 | `	VmCoroVmState sLive;` |
|       - |  523 | `#define VM_CORO_SAVE(F)  sLive.F = pVm->F;` |
|    1509 |  524 | `	VM_CORO_STATE_FIELDS(VM_CORO_SAVE)` |
|       - |  525 | `#undef VM_CORO_SAVE` |
|       - |  526 | `#define VM_CORO_LOAD(F)  pVm->F = pIn->F;` |
|    1509 |  527 | `	VM_CORO_STATE_FIELDS(VM_CORO_LOAD)` |
|       - |  528 | `#undef VM_CORO_LOAD` |
|    1509 |  529 | `	*pOut = sLive;` |
|    1509 |  530 | `}` |
|       - |  531 | `/*` |
|       - |  532 | ` * The resumer's side of one switch into the fiber: install the fiber's view of` |
|       - |  533 | ` * the VM, go, and take the resumer's back when control returns. PH7_SUSPEND` |
|       - |  534 | ` * when the fiber suspended again, else whatever its body returned.` |
|       - |  535 | ` */` |
|     752 |  536 | `static sxi32 VmCoroRun(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  537 | `{` |
|     757 |  538 | `	VmCoroStateSwap(pVm, &pCtx->sHost, &pCtx->sSaved);` |
|     757 |  539 | `	VmCoroEnter(pCtx->pCoro);` |
|     757 |  540 | `	VmCoroStateSwap(pVm, &pCtx->sSaved, &pCtx->sHost);` |
|     757 |  541 | `	return pCtx->bCoroDone ? pCtx->iCoroRc : PH7_SUSPEND;` |
|       5 |  542 | `}` |
|       - |  543 | `/*` |
|       - |  544 | ` * The fiber's side, entered exactly once. Every later resume comes back inside` |
|       - |  545 | ` * whatever Fiber::suspend() the stack is parked on, not here.` |
|       - |  546 | ` */` |
|     334 |  547 | `static void VmCoroBody(ph7_exec_ctx *pCtx)` |
|       5 |  548 | `{` |
|     339 |  549 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - |  550 | `	sxi32 rc;` |
|       - |  551 | `	VmCoroAsanArrive(pCtx->pCoro, 0);` |
|     506 |  552 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|     167 |  553 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|     167 |  554 | `		VmCtxEnforceRetFunc(pCtx), FALSE, 0, &pCtx->pStack, &pCtx->nStackCap,` |
|     167 |  555 | `		pCtx->nStackOrig);` |
|     339 |  556 | `	pCtx->iCoroRc = rc;` |
|     339 |  557 | `	pCtx->bCoroDone = 1;` |
|     339 |  558 | `	VmCoroLeaveFinal(pCtx->pCoro);` |
|     167 |  559 | `}` |
|       - |  560 | `/*` |
|       - |  561 | ` * Suspend from inside the fiber. Returns PH7_OK with the resume value waiting` |
|       - |  562 | ` * in sSuspendValue, PH7_EXCEPTION when the fiber was resumed by Fiber::throw(),` |
|       - |  563 | ` * or PH7_ABORT when it was resumed only to be unwound (the kill switch).` |
|       - |  564 | ` */` |
|     418 |  565 | `static sxi32 VmCoroSuspend(ph7_context *pCallCtx, ph7_exec_ctx *pCtx)` |
|       5 |  566 | `{` |
|     423 |  567 | `	ph7_vm *pVm = pCtx->pVm;` |
|     423 |  568 | `	VmCoroLeave(pCtx->pCoro);` |
|       - |  569 | `	/* Resumed. */` |
|     423 |  570 | `	if( pCtx->bCoroKill ){` |
|       - |  571 | `		/* The Fiber object died while we were parked here. Unwinding is the whole` |
|       - |  572 | `		 * point of coming back: every C frame between here and the body entry gets` |
|       - |  573 | `		 * to run its own abort path and free what it owns, which is the only way a` |
|       - |  574 | `		 * builtin's half-built result (array_map's output array, an open handle)` |
|       - |  575 | `		 * is ever released -- nothing outside this stack can reach them. */` |
|     224 |  576 | `		return PH7_ABORT;` |
|       - |  577 | `	}` |
|     203 |  578 | `	if( pCtx->pInjected ){` |
|       - |  579 | `		/* Fiber::throw(): php raises AT the suspension point, so the throw happens` |
|       - |  580 | `		 * here rather than at a body-entry redirect (there is no body entry on this` |
|       - |  581 | `		 * path -- the resume lands inside this C call). Same shape as any builtin's` |
|       - |  582 | `		 * own throw: stamp the frame, raise, and report the status on the call` |
|       - |  583 | `		 * context so OP_CALL does not treat the call as a normal return. */` |
|       9 |  584 | `		ph7_class_instance *pInj = pCtx->pInjected;` |
|       - |  585 | `		VmFrame *pFrame;` |
|       - |  586 | `		sxi32 rc;` |
|       9 |  587 | `		pCtx->pInjected = 0;   /* one-shot */` |
|       9 |  588 | `		pFrame = pVm->pFrame;` |
|       9 |  589 | `		if( pFrame ){` |
|       9 |  590 | `			pFrame = VmSkipExceptionFrames(pFrame);` |
|       9 |  591 | `			pFrame->iFlags \|= VM_FRAME_THROW;` |
|       4 |  592 | `		}` |
|       9 |  593 | `		rc = VmThrowException(pVm, pInj);` |
|       9 |  594 | `		if( rc == SXERR_ABORT ){` |
|     ! 0 |  595 | `			pCallCtx->nThrowRc = PH7_ABORT;` |
|     ! 0 |  596 | `			return PH7_ABORT;` |
|       - |  597 | `		}` |
|       9 |  598 | `		pCallCtx->nThrowRc = PH7_EXCEPTION;` |
|       9 |  599 | `		return PH7_EXCEPTION;` |
|       - |  600 | `	}` |
|     195 |  601 | `	return PH7_OK;` |
|     214 |  602 | `}` |
|       - |  603 | `/*` |
|       - |  604 | ` * Re-raise, in the RESUMER's frame, the exception a fiber body let escape.` |
|       - |  605 | ` * Called at the three doors that run a body (start / resume / throw) whenever` |
|       - |  606 | ` * the run came back PH7_EXCEPTION with an instance parked on the ctx. Same` |
|       - |  607 | ` * shape as any builtin's own throw: stamp the frame, raise, and report the` |
|       - |  608 | ` * status on the call context so OP_CALL does not read the call as a normal` |
|       - |  609 | ` * return.` |
|       - |  610 | ` */` |
|       8 |  611 | `static sxi32 VmFiberRaiseEscaped(ph7_context *pCtx, ph7_exec_ctx *pExecCtx)` |
|       2 |  612 | `{` |
|      10 |  613 | `	ph7_vm *pVm = pCtx->pVm;` |
|      10 |  614 | `	ph7_class_instance *pExc = pExecCtx->pEscaped;` |
|       - |  615 | `	VmFrame *pFrame;` |
|       - |  616 | `	sxi32 rc;` |
|      10 |  617 | `	pExecCtx->pEscaped = 0;` |
|      10 |  618 | `	pFrame = pVm->pFrame;` |
|      10 |  619 | `	if( pFrame ){` |
|      10 |  620 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|      10 |  621 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       4 |  622 | `	}` |
|      10 |  623 | `	rc = VmThrowException(pVm, pExc);` |
|      10 |  624 | `	PH7_ClassInstanceUnref(pExc);` |
|      10 |  625 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 |  626 | `		pCtx->nThrowRc = PH7_ABORT;` |
|     ! 0 |  627 | `		return PH7_ABORT;` |
|       - |  628 | `	}` |
|      10 |  629 | `	pCtx->nThrowRc = PH7_EXCEPTION;` |
|      10 |  630 | `	return PH7_EXCEPTION;` |
|       6 |  631 | `}` |
|       - |  632 | `#endif /* PH7_CORO_STACK */` |
|       - |  633 | `/*` |
|       - |  634 | ` * Allocate and initialize a new execution context for a fiber.` |
|       - |  635 | ` * The context is in CREATED state and ready to be started.` |
|       - |  636 | ` */` |
|    1034 |  637 | `PH7_PRIVATE ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)` |
|       5 |  638 | `{` |
|       - |  639 | `	ph7_exec_ctx *pCtx;` |
|       - |  640 | `	ph7_value *pStack;` |
|       - |  641 | `	VmFrame *pFrame;` |
|    1039 |  642 | `	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));` |
|    1039 |  643 | `	if( pCtx == 0 ){` |
|     ! 0 |  644 | `		return 0;` |
|       - |  645 | `	}` |
|    1039 |  646 | `	SyZero(pCtx, sizeof(ph7_exec_ctx));` |
|    1039 |  647 | `	pCtx->pVm = pVm;` |
|    1039 |  648 | `	pCtx->pFunc = pFunc;` |
|       - |  649 | `	/* A coroutine outlives the call that made it and reads pFunc for the whole of` |
|       - |  650 | `	 * its life -- including before its body frame exists, which VmStartCtx creates` |
|       - |  651 | `	 * LAZILY. For a run-time closure that is a hold of its own on the` |
|       - |  652 | ``	 * per-instantiation copy: `(function(){ yield 1; })()` drops the Closure object`` |
|       - |  653 | `	 * at the call, and without this the body was freed under the Generator that` |
|       - |  654 | `	 * still names it. */` |
|    1039 |  655 | `	PH7_VmClosureFuncRef(pFunc);` |
|    1039 |  656 | `	pCtx->iState = PH7_CTX_STATE_CREATED;` |
|    1039 |  657 | `	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */` |
|    1039 |  658 | `	pCtx->pc = 0;` |
|    1039 |  659 | `	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);` |
|    1039 |  660 | `	PH7_MemObjInit(pVm, &pCtx->sRetValue);` |
|    1039 |  661 | `	PH7_MemObjInit(pVm, &pCtx->sDelegate);` |
|       - |  662 | `	/* Container for this body's own exception handlers while suspended (borrowed` |
|       - |  663 | `	 * ph7_exception* pointers — never freed here, owned by the compiled func). */` |
|    1039 |  664 | `	SySetInit(&pCtx->aSavedException, &pVm->sAllocator, sizeof(ph7_exception *));` |
|       - |  665 | `	/* ROOT C: this body's own pending finally actions while suspended. */` |
|    1039 |  666 | `	SySetInit(&pCtx->aSavedFinally, &pVm->sAllocator, sizeof(VmFinallyAction));` |
|    1039 |  667 | `	pCtx->nFinallyBase = 0;` |
|       - |  668 | `	/* Stage 4: this coroutine's own aSelf entries (self::/static:: class context` |
|       - |  669 | `	 * pushed by nested method calls still open at suspend) parked while suspended,` |
|       - |  670 | `	 * so they don't pollute the resumer's aSelf. Borrowed ph7_class* pointers. */` |
|    1039 |  671 | `	SySetInit(&pCtx->aSavedSelf, &pVm->sAllocator, sizeof(ph7_class *));` |
|    1039 |  672 | `	pCtx->nSelfBase = 0;` |
|       - |  673 | ``	/* The class this body's `static::` means, taken from the CALL that is creating it`` |
|       - |  674 | `	 * (VmStartCtx pushes it back for the body's duration). */` |
|    1039 |  675 | `	pCtx->pLsbClass = PH7_VmPeekTopClass(pVm);` |
|       - |  676 | `	/* Caller slots this body's by-reference parameters alias (see the struct). */` |
|    1039 |  677 | `	SySetInit(&pCtx->aByRefArg, &pVm->sAllocator, sizeof(sxu32));` |
|    1039 |  678 | `	pCtx->pParkedSegment = 0;` |
|    1039 |  679 | `	pCtx->nBodyExecDepth = 0;` |
|       - |  680 | `	/* Allocate a private operand stack */` |
|    1039 |  681 | `	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));` |
|    1039 |  682 | `	if( pStack == 0 ){` |
|     ! 0 |  683 | `		PH7_VmClosureFuncUnref(pVm, pFunc);` |
|     ! 0 |  684 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     ! 0 |  685 | `		return 0;` |
|       - |  686 | `	}` |
|    1039 |  687 | `	pCtx->pStack = pStack;` |
|    1039 |  688 | `	pCtx->nStackCap = SySetUsed(&pFunc->aByteCode) + VM_STACK_GUARD; /* grows with pStack on OP_SPREAD */` |
|    1039 |  689 | `	pCtx->nStackOrig = pCtx->nStackCap; /* fixed original — headroom reference across resumes */` |
|       - |  690 | `	/* Create a detached frame for the fiber */` |
|    1039 |  691 | `	pFrame = VmNewFrame(pVm, pFunc, 0);` |
|    1039 |  692 | `	if( pFrame == 0 ){` |
|     ! 0 |  693 | `		SyMemBackendFree(&pVm->sAllocator, pStack);` |
|     ! 0 |  694 | `		PH7_VmClosureFuncUnref(pVm, pFunc);` |
|     ! 0 |  695 | `		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     ! 0 |  696 | `		return 0;` |
|       - |  697 | `	}` |
|       - |  698 | `	/* The frame's OWN hold, and not the one taken above. VmNewFrame stamps pFunc as` |
|       - |  699 | `	 * the frame's pUserData exactly as VmEnterFrame does, and this body frame's only` |
|       - |  700 | `	 * teardown -- VmFreeDetachedFrame, from VmCloseCtx -- gives that hold back the way` |
|       - |  701 | `	 * VmLeaveFrame does for a live frame. Without it one coroutine took one hold and` |
|       - |  702 | `	 * gave back two, so the first Generator or Fiber built from a run-time closure` |
|       - |  703 | `	 * freed the per-instantiation copy its own Closure object was still naming, and` |
|       - |  704 | `	 * calling that closure a second time said "Call to undefined function` |
|       - |  705 | `	 * [closure_N]()". */` |
|    1039 |  706 | `	PH7_VmClosureFuncRef(pFunc);` |
|    1039 |  707 | `	pCtx->pFrame = pFrame;` |
|    1039 |  708 | `	return pCtx;` |
|     522 |  709 | `}` |
|       - |  710 | `/*` |
|       - |  711 | ` * A suspended coroutine must not leave its own slices of the VM's shared stacks` |
|       - |  712 | ` * sitting above the caller's depth. Three stacks are affected, identically:` |
|       - |  713 | ` *   - pVm->aException: its exception handlers — else a generator/fiber suspended` |
|       - |  714 | ` *     inside a try leaves handlers referencing its now-detached frame on the` |
|       - |  715 | ` *     global stack, corrupting the caller's try/catch.` |
|       - |  716 | ` *   - pVm->aFinallyAction (ROOT C): its pending finally actions — else a yield` |
|       - |  717 | ` *     inside a finally (reached by return/break/rethrow) leaves a record where an` |
|       - |  718 | ` *     out-of-order-resumed sibling generator's OP_END_FINALLY would mis-pop it.` |
|       - |  719 | ` *   - pVm->aSelf (stage 4): its self::/static:: entries pushed by still-open` |
|       - |  720 | ` *     nested method calls — else they sit on the resumer's aSelf and corrupt its` |
|       - |  721 | ` *     self:: resolution.` |
|       - |  722 | ` * Each is the same operation: on suspend move the slice above a captured base` |
|       - |  723 | ` * into a per-ctx park buffer; on resume re-publish it at the (refreshed) caller` |
|       - |  724 | ` * depth. VmParkStackSlice / VmRestoreStackSlice factor it for any element type` |
|       - |  725 | ` * (size taken from the SySet); VmParkCtxState / VmRestoreCtxState drive all three.` |
|       - |  726 | ` *` |
|       - |  727 | ` * Stage 4: the whole suspended segment stays alive, so a parked handler's owner` |
|       - |  728 | ` * frame is never freed underneath it — the parked pointer stays valid and is kept` |
|       - |  729 | ` * (the old stage-2b lossy-path invalidation is gone with the discard). A` |
|       - |  730 | ` * body-level suspend only ever has body-owned handlers here, and its finally/self` |
|       - |  731 | ` * slices are empty (all nested calls already returned) — so those are no-ops.` |
|       - |  732 | ` */` |
|    5016 |  733 | `static void VmParkStackSlice(SySet *pFrom, SySet *pSaved, sxu32 nBase)` |
|       5 |  734 | `{` |
|    5021 |  735 | `	sxu32 nUsed = SySetUsed(pFrom);` |
|    5021 |  736 | `	if( nUsed > nBase ){` |
|     273 |  737 | `		const char *aBase = (const char *)SySetBasePtr(pFrom);` |
|       - |  738 | `		sxu32 i;` |
|     547 |  739 | `		for( i = nBase; i < nUsed; i++ ){` |
|     279 |  740 | `			SySetPut(pSaved, (const void *)(aBase + i * pFrom->eSize));` |
|     142 |  741 | `		}` |
|     273 |  742 | `		SySetTruncate(pFrom, nBase);` |
|     134 |  743 | `	}` |
|    5021 |  744 | `}` |
|    5010 |  745 | `static void VmRestoreStackSlice(SySet *pTo, SySet *pSaved)` |
|       5 |  746 | `{` |
|    5015 |  747 | `	sxu32 i, n = SySetUsed(pSaved);` |
|    5015 |  748 | `	if( n > 0 ){` |
|     273 |  749 | `		const char *aSaved = (const char *)SySetBasePtr(pSaved);` |
|     547 |  750 | `		for( i = 0; i < n; i++ ){` |
|     279 |  751 | `			SySetPut(pTo, (const void *)(aSaved + i * pSaved->eSize));` |
|     142 |  752 | `		}` |
|     273 |  753 | `		SySetReset(pSaved);` |
|     134 |  754 | `	}` |
|    5015 |  755 | `}` |
|    1672 |  756 | `static void VmParkCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  757 | `{` |
|    1677 |  758 | `	VmParkStackSlice(&pVm->aException, &pCtx->aSavedException, pCtx->nExceptionBase);` |
|    1677 |  759 | `	VmParkStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally, pCtx->nFinallyBase);` |
|    1677 |  760 | `	VmParkStackSlice(&pVm->aSelf, &pCtx->aSavedSelf, pCtx->nSelfBase);` |
|    1677 |  761 | `}` |
|    1670 |  762 | `static void VmRestoreCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  763 | `{` |
|    1675 |  764 | `	VmRestoreStackSlice(&pVm->aException, &pCtx->aSavedException);` |
|    1675 |  765 | `	VmRestoreStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally);` |
|    1675 |  766 | `	VmRestoreStackSlice(&pVm->aSelf, &pCtx->aSavedSelf);` |
|    1675 |  767 | `}` |
|       - |  768 | `/*` |
|       - |  769 | ` * On suspend, free the exception (try) frames the yield was nested in. They were` |
|       - |  770 | ` * pushed by OP_LOAD_EXCEPTION between the coroutine body frame (pCtx->pFrame) and` |
|       - |  771 | ` * the current suspend-point top frame. The generator/fiber frame model saves only` |
|       - |  772 | ` * the body frame, so these transparent wrappers would otherwise be orphaned and` |
|       - |  773 | ` * leak on every yield-that-sits-inside-a-try (unbounded for a generator looping` |
|       - |  774 | ` * with a yield in a try). Freeing them loses nothing the resume needs: this body's` |
|       - |  775 | ` * exception HANDLERS are parked separately (VmParkCtxState) and each` |
|       - |  776 | ` * try's landing pad lives on its ph7_exception (iLandingPc), while OP_POP_EXCEPTION` |
|       - |  777 | ` * on resume skips the (now absent) frame pop via its VM_FRAME_EXCEPTION guard and` |
|       - |  778 | ` * OP_LOAD_EXCEPTION re-creates a fresh wrapper when the try is next entered. Must` |
|       - |  779 | ` * run while pVm->pFrame still points at the suspend-time top (before the detach).` |
|       - |  780 | ` */` |
|    2658 |  781 | `static void VmFreeSuspendedExceptionFrames(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  782 | `{` |
|    2877 |  783 | `	while( pVm->pFrame != pCtx->pFrame && (pVm->pFrame->iFlags & VM_FRAME_EXCEPTION) ){` |
|     219 |  784 | `		VmLeaveFrame(&(*pVm));` |
|       5 |  785 | `	}` |
|    2663 |  786 | `}` |
|       - |  787 | `/*` |
|       - |  788 | ` * Stamp a coroutine body frame with the site that is starting or RESUMING it.` |
|       - |  789 | ` *` |
|       - |  790 | ` * An ordinary frame gets this in VmEnterFrame; a coroutine's body frame is built` |
|       - |  791 | ` * detached (VmNewExecCtx -> VmNewFrame) and never went through it, so a backtrace` |
|       - |  792 | ` * taken inside a generator reported the frame below it at line 0 -- printed as` |
|       - |  793 | ` * line 1, in whatever file the include stack happened to top out at. php answers` |
|       - |  794 | ``  * the CURRENT resume site rather than the creation site (`foreach (g() as $v)` `` |
|       - |  795 | `` * for the first step, the `yield from` line for a delegate), which is exactly`` |
|       - |  796 | ` * what this reads, so it is stamped on every start and resume rather than once.` |
|       - |  797 | ` * Must run BEFORE the frame is spliced onto the chain: the site is the resumer's.` |
|       - |  798 | ` */` |
|    3076 |  799 | `static void VmStampCoroutineCallSite(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 |  800 | `{` |
|       - |  801 | `	SyString *pFile;` |
|    3081 |  802 | `	if( pCtx->pFrame == 0 ){` |
|     ! 0 |  803 | `		return;` |
|       - |  804 | `	}` |
|    3081 |  805 | `	pCtx->pFrame->nCallLine = pVm->nCurLine;` |
|    3081 |  806 | `	pFile = PH7_VmExecutingUnitFile(&(*pVm));` |
|    3081 |  807 | `	if( pFile ){` |
|    3081 |  808 | `		pCtx->pFrame->sCallFile = *pFile;` |
|    1538 |  809 | `	}` |
|    1543 |  810 | `}` |
|       - |  811 | `/*` |
|       - |  812 | ` * Common suspend epilogue for VmStartCtx / VmResumeCtx: detach the suspended` |
|       - |  813 | ` * coroutine from the live VM chain and park its exception handlers. Two forms:` |
|       - |  814 | ` *   - Body-level (pParkedSegment == 0): a generator yield or a fiber suspending` |
|       - |  815 | ` *     directly in its body. The try wrappers the yield sat in are transient —` |
|       - |  816 | ` *     free them (OP_LOAD_EXCEPTION recreates them on re-entry) — and detach the` |
|       - |  817 | ` *     body frame alone.` |
|       - |  818 | ` *   - Deep fiber suspend (pParkedSegment != 0, stage 4): the whole segment (body` |
|       - |  819 | ` *     frame + the nested call/try frames above it) stays alive and is detached` |
|       - |  820 | ` *     as a unit; nothing is freed, so resume can continue inside the innermost` |
|       - |  821 | ` *     callee. Its handlers are parked the same way and rebased on resume.` |
|       - |  822 | ` */` |
|    2090 |  823 | `static void VmSuspendCtxDetach(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|       5 |  824 | `{` |
|       - |  825 | `#ifdef PH7_CORO_STACK` |
|    2095 |  826 | `	if( pCtx->pCoro ){` |
|       - |  827 | `		/* Third form: the fiber has a stack of its own and is still standing on` |
|       - |  828 | `		 * it. Its frames -- body, nested callees, open-try wrappers alike -- stay` |
|       - |  829 | `		 * exactly as they are, because that C stack still points into them; all` |
|       - |  830 | `		 * this has to remember is which one was current, so the resume can make it` |
|       - |  831 | `		 * current again. Nothing is parked and no depth is deducted: the three` |
|       - |  832 | `		 * stacks and both counters travelled with the VM-state swap the moment the` |
|       - |  833 | `		 * switch happened. */` |
|     423 |  834 | `		pCtx->pCoroTop = pVm->pFrame;` |
|     423 |  835 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|     423 |  836 | `		pCtx->pFrame->pParent = 0;` |
|     423 |  837 | `		if( pResult ){` |
|     423 |  838 | `			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|     209 |  839 | `		}` |
|     423 |  840 | `		return;` |
|       - |  841 | `	}` |
|       - |  842 | `#endif` |
|    1677 |  843 | `	if( pCtx->pParkedSegment == 0 ){` |
|    1677 |  844 | `		VmFreeSuspendedExceptionFrames(pVm, pCtx);` |
|     841 |  845 | `	}else{` |
|       - |  846 | `		/* The parked records' push-time accounting (one nRecursionDepth++ each,` |
|       - |  847 | `		 * plus VmCallFinish's aSelf pop, which never ran) leaves the segment` |
|       - |  848 | `		 * counted as active while the fiber is suspended — deactivate it. aSelf` |
|       - |  849 | `		 * is parked wholesale below (base-relative), so drop only the depth. */` |
|     ! 0 |  850 | `		pVm->nRecursionDepth -= ((VmParkedSegment *)pCtx->pParkedSegment)->nRecords;` |
|       - |  851 | `	}` |
|    1677 |  852 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|    1677 |  853 | `	pCtx->pFrame->pParent = 0;` |
|    1677 |  854 | `	VmParkCtxState(pVm, pCtx);` |
|    1677 |  855 | `	if( pResult ){` |
|     ! 0 |  856 | `		PH7_MemObjStore(&pCtx->sSuspendValue, pResult);` |
|     ! 0 |  857 | `	}` |
|    1050 |  858 | `}` |
|       - |  859 | `/*` |
|       - |  860 | ` * The return-type enforcement target for a coroutine body run. A GENERATOR` |
|       - |  861 | ` * function's declared return type belongs to the call site (always a Generator` |
|       - |  862 | ` * object, validated at compile time as "a supertype of Generator"); the body's` |
|       - |  863 | ` * own return value feeds getReturn() and is never type-checked. Gate on the` |
|       - |  864 | ` * VM_FUNC_GENERATOR flag (the semantic property), not pPrivate (a wrapper-linkage` |
|       - |  865 | ` * fact): a Fiber given a generator-flagged callable runs with pPrivate == 0 and` |
|       - |  866 | ` * must not enforce either. Ordinary fiber callables keep their declared` |
|       - |  867 | ` * return-type enforcement (php enforces it).` |
|       - |  868 | ` */` |
|    2658 |  869 | `static ph7_vm_func * VmCtxEnforceRetFunc(ph7_exec_ctx *pCtx)` |
|       5 |  870 | `{` |
|    1500 |  871 | `	return ((pCtx->pFunc->iFlags & VM_FUNC_GENERATOR) == 0 && VmFuncHasReturnType(pCtx->pFunc))` |
|    1495 |  872 | `		? pCtx->pFunc : 0;` |
|       5 |  873 | `}` |
|       - |  874 | `/*` |
|       - |  875 | ` * Common epilogue for VmStartCtx / VmResumeCtx once the body run returns rc:` |
|       - |  876 | ` * restore the previous active context, then park on suspend or detach the` |
|       - |  877 | ` * coroutine frame and record the terminal state. On normal completion the value` |
|       - |  878 | ` * belongs to getReturn() only — start()/resume() return the NEXT suspend value,` |
|       - |  879 | ` * which is null at completion (php parity), so pResult is left at its` |
|       - |  880 | ` * caller-initialized null.` |
|       - |  881 | ` */` |
|    3076 |  882 | `static sxi32 VmFinishCtxRun(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_exec_ctx *pOldCtx,` |
|       - |  883 | `	sxi32 rc, ph7_value *pResult)` |
|       5 |  884 | `{` |
|    3081 |  885 | `	pVm->pActiveCtx = pOldCtx;` |
|       - |  886 | `#ifdef PH7_CORO_STACK` |
|    3081 |  887 | `	if( pCtx->pCoro ){` |
|     757 |  888 | `		if( rc == PH7_SUSPEND ){` |
|       - |  889 | `			/* No saved pc or top-of-stack to record: the switch IS the state, and` |
|       - |  890 | `			 * VmSuspendCtx (which marks the ctx on the other path) never ran. */` |
|     423 |  891 | `			pCtx->iState = PH7_CTX_STATE_SUSPENDED;` |
|     423 |  892 | `			VmSuspendCtxDetach(pVm, pCtx, pResult);` |
|     423 |  893 | `			return SXRET_OK;` |
|       - |  894 | `		}` |
|       - |  895 | `		/* The body is over, so the stack is spent: free it now rather than at the` |
|       - |  896 | `		 * Fiber object's death (a completed fiber has no use for megabytes of` |
|       - |  897 | `		 * mapping), and with it the three stacks that were only ever this body's.` |
|       - |  898 | `		 * The frame chain it left is the body frame plus any try wrappers an` |
|       - |  899 | `		 * escaping exception never closed -- the same two steps the shared tail` |
|       - |  900 | `		 * below takes, which the ctx-owned bases in it do not apply to here. */` |
|     339 |  901 | `		VmFreeSuspendedExceptionFrames(pVm, pCtx);` |
|     339 |  902 | `		if( pVm->pFrame == pCtx->pFrame ){` |
|     339 |  903 | `			pVm->pFrame = pCtx->pFrame->pParent;` |
|     339 |  904 | `			pCtx->pFrame->pParent = 0;` |
|     167 |  905 | `		}` |
|     339 |  906 | `		VmCoroFree(pVm, pCtx->pCoro);` |
|     339 |  907 | `		pCtx->pCoro = 0;` |
|     339 |  908 | `		VmCoroStateRelease(pVm, &pCtx->sSaved);` |
|     339 |  909 | `		if( rc == PH7_ABORT ){` |
|     224 |  910 | `			pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|     224 |  911 | `			return PH7_ABORT;` |
|       - |  912 | `		}` |
|     119 |  913 | `		if( rc == PH7_EXCEPTION ){` |
|      10 |  914 | `			pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      10 |  915 | `			pCtx->bThrew = 1;` |
|      10 |  916 | `			return PH7_EXCEPTION;` |
|       - |  917 | `		}` |
|     111 |  918 | `		pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|     111 |  919 | `		return SXRET_OK;` |
|       - |  920 | `	}` |
|       - |  921 | `#endif` |
|    2329 |  922 | `	if( rc == PH7_SUSPEND ){` |
|       - |  923 | `		/* Handles a deep RE-suspend too (a resumed segment parking a fresh one):` |
|       - |  924 | `		 * VmSuspendCtxDetach deactivates the new segment's recursion accounting,` |
|       - |  925 | `		 * parks its aSelf, and skips VmFreeSuspendedExceptionFrames for a segment` |
|       - |  926 | `		 * so it can't free the still-live parked try wrappers. */` |
|    1677 |  927 | `		VmSuspendCtxDetach(pVm, pCtx, pResult);` |
|    1677 |  928 | `		return SXRET_OK;` |
|       - |  929 | `	}` |
|       - |  930 | ``	/* A finally entered via the throw redirect whose `return` short-circuited`` |
|       - |  931 | `	 * OP_END_FINALLY leaves the try's transparent wrapper ABOVE the body frame —` |
|       - |  932 | `	 * the detach below would then be skipped and the wrapper (plus the body` |
|       - |  933 | `	 * frame) leak into the RESUMER's frame chain, so the next try at that scope` |
|       - |  934 | `	 * records the wrong owner frame and its caught throw silently unwinds the` |
|       - |  935 | `	 * script. Free trailing exception wrappers exactly like the suspend path. */` |
|     657 |  936 | `	if( pCtx->pParkedSegment == 0 ){` |
|     657 |  937 | `		VmFreeSuspendedExceptionFrames(pVm, pCtx);` |
|     326 |  938 | `	}` |
|       - |  939 | `	/* Detach the coroutine frame from the live chain, unless a deeper unwind` |
|       - |  940 | `	 * already moved pVm->pFrame off it. */` |
|     657 |  941 | `	if( pVm->pFrame == pCtx->pFrame ){` |
|     657 |  942 | `		pVm->pFrame = pCtx->pFrame->pParent;` |
|     657 |  943 | `		pCtx->pFrame->pParent = 0;` |
|     326 |  944 | `	}` |
|       - |  945 | `	/* The body is over (it did not suspend): drop whatever it left on the shared` |
|       - |  946 | `	 * self stack, which is at least the LSB class VmStartCtx published for it. */` |
|     657 |  947 | `	if( SySetUsed(&pVm->aSelf) > pCtx->nSelfBase ){` |
|      44 |  948 | `		SySetTruncate(&pVm->aSelf, pCtx->nSelfBase);` |
|      20 |  949 | `	}` |
|     657 |  950 | `	if( rc == PH7_ABORT ){` |
|       3 |  951 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|       3 |  952 | `		return PH7_ABORT;` |
|       - |  953 | `	}` |
|     655 |  954 | `	if( rc == PH7_EXCEPTION ){` |
|      61 |  955 | `		pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|      61 |  956 | `		pCtx->bThrew = 1;` |
|      61 |  957 | `		return PH7_EXCEPTION;` |
|       - |  958 | `	}` |
|     599 |  959 | `	pCtx->iState = PH7_CTX_STATE_COMPLETED;` |
|     599 |  960 | `	return SXRET_OK;` |
|    1543 |  961 | `}` |
|       - |  962 | `/*` |
|       - |  963 | ` * Start executing a fiber context for the first time.` |
|       - |  964 | ` */` |
|     988 |  965 | `static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)` |
|       5 |  966 | `{` |
|       - |  967 | `	ph7_exec_ctx *pOldCtx;` |
|       - |  968 | `	sxi32 rc;` |
|     993 |  969 | `	if( pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|     ! 0 |  970 | `		return SXERR_INVALID;` |
|       - |  971 | `	}` |
|       - |  972 | `	/* A fiber/generator start is a native VmByteCodeExec re-entry, bounded by` |
|       - |  973 | `	 * nMaxNativeDepth. Reject HERE, before attaching the frame / mutating VM` |
|       - |  974 | `	 * state, so the abort is clean — the wrapper's own check fires only after` |
|       - |  975 | `	 * this function has spliced the coroutine into the frame chain, which its` |
|       - |  976 | `	 * post-exec detach cannot fully unwind. The PHP call-depth cap belongs to` |
|       - |  977 | `	 * OP_CALL only. */` |
|     993 |  978 | `	if( VmNativeNestingExceeded(pVm) ){` |
|     ! 0 |  979 | `		return VmNativeNestingFatal(pVm);` |
|       - |  980 | `	}` |
|       - |  981 | `#ifdef PH7_CORO_STACK` |
|       - |  982 | `	/* A FIBER gets a native stack of its own (pPrivate == 0 is what tells one from` |
|       - |  983 | ``	 * a generator, which never needs one: `yield` is lexically inside the body, so`` |
|       - |  984 | `	 * it can never have a C frame above it to park). Taken BEFORE anything below` |
|       - |  985 | `	 * mutates the VM, because the fiber's opening view of it is the resumer's.` |
|       - |  986 | `	 * A stack this build cannot give it leaves pCoro at 0 and the body runs inline` |
|       - |  987 | `	 * exactly as it did before. */` |
|     993 |  988 | `	if( pCtx->pPrivate == 0 ){` |
|     339 |  989 | `		pCtx->pCoro = VmCoroNew(pVm, pCtx);` |
|     339 |  990 | `		if( pCtx->pCoro ){` |
|     339 |  991 | `			VmCoroStateInit(pVm, pCtx);` |
|     167 |  992 | `		}` |
|     167 |  993 | `	}` |
|       - |  994 | `#endif` |
|       - |  995 | `	/* Attach the fiber's frame to the VM frame chain */` |
|     993 |  996 | `	VmStampCoroutineCallSite(pVm, pCtx);` |
|     993 |  997 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|     993 |  998 | `	pVm->pFrame = pCtx->pFrame;` |
|       - |  999 | `	/* Save and set the active context */` |
|     993 | 1000 | `	pOldCtx = pVm->pActiveCtx;` |
|     993 | 1001 | `	pVm->pActiveCtx = pCtx;` |
|     993 | 1002 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|       - | 1003 | `#ifdef PH7_CORO_STACK` |
|     993 | 1004 | `	if( pCtx->pCoro ){` |
|       - | 1005 | `		/* Its three stacks are its own and start empty, so every floor an` |
|       - | 1006 | `		 * activation of this body records is measured from zero and stays true` |
|       - | 1007 | `		 * however deep the resumer happens to be next time. */` |
|     339 | 1008 | `		pCtx->nExceptionBase = 0;` |
|     339 | 1009 | `		pCtx->nFinallyBase = 0;` |
|     339 | 1010 | `		pCtx->nSelfBase = 0;` |
|     339 | 1011 | `		if( pCtx->pLsbClass ){` |
|       3 | 1012 | `			SySetPut(&pCtx->sSaved.aSelf, (const void *)&pCtx->pLsbClass);` |
|       1 | 1013 | `		}` |
|     172 | 1014 | `	}else` |
|       - | 1015 | `#endif` |
|       - | 1016 | `	{` |
|     659 | 1017 | `		pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|     659 | 1018 | `		pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);` |
|     659 | 1019 | `		pCtx->nSelfBase = SySetUsed(&pVm->aSelf);` |
|       - | 1020 | `		/* Re-publish the creating call's late-static-binding class ABOVE that base, so` |
|       - | 1021 | ``		 * the body's `static::` resolves to what php resolves it to. It rides the`` |
|       - | 1022 | `		 * ordinary park/restore of this coroutine's own aSelf slice, so a suspend takes` |
|       - | 1023 | `		 * it off the shared stack and a resume puts it back; VmFinishCtxRun truncates it` |
|       - | 1024 | `		 * away when the body ends for good. */` |
|     659 | 1025 | `		if( pCtx->pLsbClass ){` |
|      44 | 1026 | `			SySetPut(&pVm->aSelf,(const void *)&pCtx->pLsbClass);` |
|      20 | 1027 | `		}` |
|       - | 1028 | `	}` |
|       - | 1029 | `	/* Native depth the body runs at (the VmByteCodeExec wrapper bumps +1): a` |
|       - | 1030 | `	 * Fiber::suspend() at a deeper depth is inside a C->PHP callback and gets a` |
|       - | 1031 | `	 * FiberError instead of parking across the native frame (stage 4). */` |
|     993 | 1032 | `	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1;` |
|       - | 1033 | `#ifdef PH7_CORO_STACK` |
|     993 | 1034 | `	if( pCtx->pCoro ){` |
|       - | 1035 | `		/* On its own stack the body starts at native depth zero, so it never reads` |
|       - | 1036 | `		 * nBodyExecDepth again -- the suspend that used to consult it now simply` |
|       - | 1037 | `		 * switches, from wherever it is. */` |
|     339 | 1038 | `		rc = VmCoroRun(pVm, pCtx);` |
|     339 | 1039 | `		return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|       - | 1040 | `	}` |
|       - | 1041 | `#endif` |
|       - | 1042 | `	/* Execute from the beginning (no parked segment). An OP_SPREAD in the body may` |
|       - | 1043 | `	 * realloc pCtx->pStack — pass &pCtx->pStack / &pCtx->nStackCap so the grown` |
|       - | 1044 | `	 * buffer + capacity persist for the next resume and for ctx teardown. */` |
|     986 | 1045 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|     327 | 1046 | `		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,` |
|     327 | 1047 | `		VmCtxEnforceRetFunc(pCtx), FALSE, 0, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);` |
|     659 | 1048 | `	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|     499 | 1049 | `}` |
|       - | 1050 | `/*` |
|       - | 1051 | ` * Resume a suspended fiber context.` |
|       - | 1052 | ` */` |
|    2088 | 1053 | `PH7_PRIVATE sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)` |
|       5 | 1054 | `{` |
|       - | 1055 | `	ph7_exec_ctx *pOldCtx;` |
|       - | 1056 | `	VmParkedSegment *pSeg;` |
|       - | 1057 | `	sxi32 rc;` |
|    2093 | 1058 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|     ! 0 | 1059 | `		return SXERR_INVALID;` |
|       - | 1060 | `	}` |
|       - | 1061 | `#ifdef PH7_CORO_STACK` |
|    2093 | 1062 | `	if( pCtx->pCoro ){` |
|       - | 1063 | `		/* A fiber on its own stack: nothing to re-push, rebase or adopt. The whole` |
|       - | 1064 | `		 * suspended activation chain is still standing on that stack, so a resume` |
|       - | 1065 | `		 * is the frame re-attach plus one switch.` |
|       - | 1066 | `		 *` |
|       - | 1067 | `		 * The nesting guard above is skipped on purpose: this re-entry adds a` |
|       - | 1068 | `		 * single frame to the RESUMER's stack and then leaves it for the fiber's,` |
|       - | 1069 | `		 * which carries its own depth count -- and a teardown unwind (bCoroKill)` |
|       - | 1070 | `		 * must go through even when the resumer is already at the cap, or the C` |
|       - | 1071 | `		 * frames it is there to unwind are freed underneath instead.` |
|       - | 1072 | `		 *` |
|       - | 1073 | `		 * The resume value reaches the parked Fiber::suspend() through the ctx's` |
|       - | 1074 | `		 * own slot rather than an operand stack: on this path the suspend is a C` |
|       - | 1075 | `		 * call about to RETURN a value, not a saved pc with a hole above its top. */` |
|     423 | 1076 | `		if( pResumeValue ){` |
|     133 | 1077 | `			PH7_MemObjStore(pResumeValue, &pCtx->sSuspendValue);` |
|      69 | 1078 | `		}else{` |
|     295 | 1079 | `			PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|       - | 1080 | `		}` |
|     423 | 1081 | `		VmStampCoroutineCallSite(pVm, pCtx);` |
|     423 | 1082 | `		pCtx->pFrame->pParent = pVm->pFrame;` |
|     423 | 1083 | `		pVm->pFrame = pCtx->pCoroTop;` |
|     423 | 1084 | `		pOldCtx = pVm->pActiveCtx;` |
|     423 | 1085 | `		pVm->pActiveCtx = pCtx;` |
|     423 | 1086 | `		pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|     423 | 1087 | `		rc = VmCoroRun(pVm, pCtx);` |
|     423 | 1088 | `		return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|       - | 1089 | `	}` |
|       - | 1090 | `#endif` |
|       - | 1091 | `	/* A resume is a native VmByteCodeExec re-entry, bounded by nMaxNativeDepth.` |
|       - | 1092 | `	 * Reject HERE, before re-attaching the (possibly deep) parked segment to the` |
|       - | 1093 | `	 * frame chain and re-adding its nRecords to nRecursionDepth — a wrapper-level` |
|       - | 1094 | `	 * abort past those mutations would leave pVm->pFrame pointing into the parked` |
|       - | 1095 | `	 * callee and the depth accounting un-reverted. The PHP call-depth cap is` |
|       - | 1096 | `	 * OP_CALL-only. */` |
|    1675 | 1097 | `	if( VmNativeNestingExceeded(pVm) ){` |
|     ! 0 | 1098 | `		return VmNativeNestingFatal(pVm);` |
|       - | 1099 | `	}` |
|       - | 1100 | `	/* Push the resume value onto the SUSPENDED activation's operand stack so it` |
|       - | 1101 | `	 * appears as Fiber::suspend()'s return value. For a deep suspend (stage 4)` |
|       - | 1102 | `	 * that stack is the innermost callee's — parked in the segment — not the` |
|       - | 1103 | `	 * body's. nTos was saved one below the return-value slot. */` |
|       - | 1104 | `	{` |
|       - | 1105 | `		ph7_value *pResumeStack;` |
|    1675 | 1106 | `		pSeg = (VmParkedSegment *)pCtx->pParkedSegment;` |
|    1675 | 1107 | `		pResumeStack = pSeg ? pSeg->sState.pStack : pCtx->pStack;` |
|    1675 | 1108 | `		if( pResumeValue ){` |
|     207 | 1109 | `			PH7_MemObjStore(pResumeValue, &pResumeStack[pCtx->nTos + 1]);` |
|     106 | 1110 | `		}else{` |
|    1473 | 1111 | `			PH7_MemObjRelease(&pResumeStack[pCtx->nTos + 1]);` |
|       - | 1112 | `		}` |
|    1675 | 1113 | `		pCtx->nTos++;` |
|       - | 1114 | `		/* Refresh the caller-depth base and re-publish this body's own exception` |
|       - | 1115 | `		 * handlers on top of pVm->aException at that depth, so the resumed body's` |
|       - | 1116 | `		 * try/catch and finally-drain bound line up (see VmByteCodeExec's base` |
|       - | 1117 | `		 * override). Must run before VmByteCodeExec recaptures its local base. */` |
|    1675 | 1118 | `		pCtx->nExceptionBase = SySetUsed(&pVm->aException);` |
|    1675 | 1119 | `		pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);` |
|    1675 | 1120 | `		pCtx->nSelfBase = SySetUsed(&pVm->aSelf);` |
|    1675 | 1121 | `		VmRestoreCtxState(pVm, pCtx);` |
|    1675 | 1122 | `		if( pSeg ){` |
|       - | 1123 | `			/* Reactivate the parked records' recursion accounting (mirror of the` |
|       - | 1124 | `			 * deactivate at suspend); aSelf was just restored above. */` |
|     ! 0 | 1125 | `			pVm->nRecursionDepth += pSeg->nRecords;` |
|       - | 1126 | `			/* Rebase the parked segment's absolute exception-floor indices: the` |
|       - | 1127 | `			 * fiber may resume at a different caller depth than it suspended at,` |
|       - | 1128 | `			 * so every activation's nExceptionBase shifts by the same delta the` |
|       - | 1129 | `			 * republished handlers moved (newBase - the park-time base). */` |
|     ! 0 | 1130 | `			sxi32 iDelta = (sxi32)pCtx->nExceptionBase - (sxi32)pSeg->nOldExcBase;` |
|       - | 1131 | `			/* nFinallyActBase floors rebase by their OWN delta — the exception and` |
|       - | 1132 | `			 * finally-action stacks move independently between suspend and resume` |
|       - | 1133 | `			 * (a fiber resumed from inside a generator's inline finally sees a` |
|       - | 1134 | `			 * DEEPER aFinallyAction with an unchanged aException, and a stale` |
|       - | 1135 | `			 * absolute floor would make the activation-end discard eat the` |
|       - | 1136 | `			 * resumer's pending action). */` |
|     ! 0 | 1137 | `			sxi32 iFinDelta = (sxi32)pCtx->nFinallyBase - (sxi32)pSeg->nOldFinBase;` |
|     ! 0 | 1138 | `			if( iDelta != 0 \|\| iFinDelta != 0 ){` |
|       - | 1139 | `				VmCallFrame *pRec;` |
|     ! 0 | 1140 | `				pSeg->sState.nExceptionBase =` |
|     ! 0 | 1141 | `					(sxu32)((sxi32)pSeg->sState.nExceptionBase + iDelta);` |
|     ! 0 | 1142 | `				pSeg->sState.nFinallyActBase =` |
|     ! 0 | 1143 | `					(sxu32)((sxi32)pSeg->sState.nFinallyActBase + iFinDelta);` |
|     ! 0 | 1144 | `				for( pRec = pSeg->pCallTop; pRec; pRec = pRec->pPrev ){` |
|     ! 0 | 1145 | `					pRec->sCaller.nExceptionBase =` |
|     ! 0 | 1146 | `						(sxu32)((sxi32)pRec->sCaller.nExceptionBase + iDelta);` |
|     ! 0 | 1147 | `					pRec->sCaller.nFinallyActBase =` |
|     ! 0 | 1148 | `						(sxu32)((sxi32)pRec->sCaller.nFinallyActBase + iFinDelta);` |
|     ! 0 | 1149 | `				}` |
|     ! 0 | 1150 | `			}` |
|     ! 0 | 1151 | `		}` |
|       - | 1152 | `		/* Re-attach the coroutine to the live VM frame chain: the body frame's` |
|       - | 1153 | `		 * parent becomes the resumer's current frame. For a deep segment the` |
|       - | 1154 | `		 * suspend-time top frame (the innermost callee / open-try wrapper) then` |
|       - | 1155 | `		 * becomes current so the adopt at VmByteCodeExec entry resumes inside the` |
|       - | 1156 | `		 * callee; body-level resumes make the body frame current. */` |
|    1675 | 1157 | `		VmStampCoroutineCallSite(pVm, pCtx);` |
|    1675 | 1158 | `		pCtx->pFrame->pParent = pVm->pFrame;` |
|    1675 | 1159 | `		pVm->pFrame = pSeg ? pSeg->pTopFrame : pCtx->pFrame;` |
|       - | 1160 | `	}` |
|       - | 1161 | `	/* The segment (if any) is handed to the body invocation explicitly below; it` |
|       - | 1162 | `	 * is no longer part of the suspended ctx state once resume owns it. */` |
|    1675 | 1163 | `	pCtx->pParkedSegment = 0;` |
|       - | 1164 | `	/* Save and set the active context */` |
|    1675 | 1165 | `	pOldCtx = pVm->pActiveCtx;` |
|    1675 | 1166 | `	pVm->pActiveCtx = pCtx;` |
|    1675 | 1167 | `	pCtx->iState = PH7_CTX_STATE_RUNNING;` |
|    1675 | 1168 | `	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1; /* see VmStartCtx */` |
|       - | 1169 | `	/* Resume execution from saved PC. pSeg, when non-NULL, makes this body` |
|       - | 1170 | `	 * re-enter inside the innermost parked callee (deep fiber resume, stage 4). */` |
|    2510 | 1171 | `	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),` |
|     835 | 1172 | `		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,` |
|     835 | 1173 | `		VmCtxEnforceRetFunc(pCtx), FALSE, pSeg, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);` |
|    1675 | 1174 | `	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);` |
|    1049 | 1175 | `}` |
|       - | 1176 | `/*` |
|       - | 1177 | ` * Force-close a suspended generator context at destruction time, running its` |
|       - | 1178 | `` * pending `finally` blocks (PHP runs finally when a generator is unset / goes out`` |
|       - | 1179 | ` * of scope / is GC'd before it completes; PHL previously freed the open try` |
|       - | 1180 | ` * handlers unexecuted in VmReleaseExecCtx). This is a "close", not a "resume":` |
|       - | 1181 | `` * the finally handler of every still-open `try` the generator was suspended`` |
|       - | 1182 | `` * inside runs innermost-first, but NO `catch` runs and no code past the finallys`` |
|       - | 1183 | ` * executes.` |
|       - | 1184 | ` *` |
|       - | 1185 | ` * Generators compile finally INLINE (ROOT C): the finally bytecode lives in the` |
|       - | 1186 | ` * body program at iFinallyPc, driven by the aException/aFinallyAction pc-redirect` |
|       - | 1187 | `` * machinery — not the legacy detached `sFinally` mini-program VmDrainFinally runs.`` |
|       - | 1188 | `` * So a close is expressed exactly like a `return` that crosses every enclosing`` |
|       - | 1189 | ` * finally (OP_SET_FINALLY_RET): set pCtx->bClosing and resume; the body-entry` |
|       - | 1190 | ` * redirect (see VmByteCodeExecBody) seeds a PH7_FA_RETURN action and jumps into` |
|       - | 1191 | ` * the innermost open try's finally, and OP_END_FINALLY threads it out through the` |
|       - | 1192 | ` * chain, then completes the body.` |
|       - | 1193 | ` *` |
|       - | 1194 | ` * Scope: a plain body-level suspend (pParkedSegment == 0). A deep fiber segment is` |
|       - | 1195 | ` * left to plain release (generators never park one — yield is body-level only). A` |
|       - | 1196 | `` * `yield` reached inside a finally during close is rejected by OP_YIELD via`` |
|       - | 1197 | ` * pCtx->bClosing (PHP-exact "Cannot yield from finally in a force-closed` |
|       - | 1198 | ` * generator"). Deferred edges remain.` |
|       - | 1199 | ` *` |
|       - | 1200 | ` * Returns whatever the body run returns: SXRET_OK on a clean close, PH7_ABORT if a` |
|       - | 1201 | ` * finally aborts, or PH7_EXCEPTION if a finally threw past itself (surfaced to the` |
|       - | 1202 | ` * destruct caller).` |
|       - | 1203 | ` */` |
|     686 | 1204 | `static sxi32 VmCloseCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 | 1205 | `{` |
|       - | 1206 | `	sxi32 rc;` |
|     691 | 1207 | `	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       - | 1208 | `		/* CREATED never ran, so has no open try; COMPLETED/CLOSED already ran theirs. */` |
|     473 | 1209 | `		return SXRET_OK;` |
|       - | 1210 | `	}` |
|     223 | 1211 | `	if( pCtx->pParkedSegment != 0 ){` |
|       - | 1212 | `		/* Deep fiber segment (never a generator) — leave to plain release. */` |
|     ! 0 | 1213 | `		return SXRET_OK;` |
|       - | 1214 | `	}` |
|       - | 1215 | ``	/* Suspended mid `yield from` over an inner generator: PHP closes innermost-first,`` |
|       - | 1216 | `	 * so run the delegate's finallys before this body's. Both delegate-object states` |
|       - | 1217 | `	 * carry the live iterator in sDelegate — state 3 (the delegate IS a Generator) and` |
|       - | 1218 | ``	 * state 2 (an Iterator, which for `yield from $aggregate` is the generator returned`` |
|       - | 1219 | `	 * by getIterator()); VmGeneratorExtractCtx returns 0 for a non-generator iterator,` |
|       - | 1220 | `	 * so a plain Iterator/array delegate is skipped. Recurses for nested delegation;` |
|       - | 1221 | `	 * the inner ends COMPLETED, so its own later __destruct close is a no-op. */` |
|     223 | 1222 | `	if( pCtx->iDelegateState >= 2 && (pCtx->sDelegate.iFlags & MEMOBJ_OBJ) ){` |
|      14 | 1223 | `		ph7_generator *pInner = VmGeneratorExtractCtx(pVm, &pCtx->sDelegate);` |
|      14 | 1224 | `		if( pInner && pInner->pCtx ){` |
|      14 | 1225 | `			sxi32 rcInner = VmCloseCtx(pVm, pInner->pCtx);` |
|      14 | 1226 | `			if( rcInner == PH7_ABORT ){ return PH7_ABORT; }` |
|       6 | 1227 | `		}` |
|       6 | 1228 | `	}` |
|       - | 1229 | `	/* Drive the pending finallys through a real body resume that the entry redirect` |
|       - | 1230 | `	 * turns into a finally-chain unwind. VmResumeCtx handles state restore, frame` |
|       - | 1231 | `	 * re-attach, active-ctx save/restore and the terminal-state bookkeeping. */` |
|     223 | 1232 | `	pCtx->bClosing = 1;` |
|     223 | 1233 | `	rc = VmResumeCtx(pVm, pCtx, 0, 0);` |
|     223 | 1234 | `	pCtx->bClosing = 0;` |
|     223 | 1235 | `	return rc;` |
|     348 | 1236 | `}` |
|       - | 1237 | `/*` |
|       - | 1238 | ` * Free one DETACHED frame (not in the live pVm->pFrame chain): the frame of a` |
|       - | 1239 | ` * suspended coroutine's body, or of a segment activation abandoned mid-call.` |
|       - | 1240 | ` * Mirrors VmLeaveFrame's teardown minus the chain pop (there is no chain to pop` |
|       - | 1241 | ` * from). Factored so the body-frame free and the stage-4 segment free share it.` |
|       - | 1242 | ` */` |
|    1032 | 1243 | `static void VmFreeDetachedFrame(ph7_vm *pVm, VmFrame *pFrame)` |
|       5 | 1244 | `{` |
|       - | 1245 | `	VmSlot *aSlot;` |
|       - | 1246 | `	sxu32 n;` |
|    1037 | 1247 | `	if( pFrame == 0 ){` |
|     ! 0 | 1248 | `		return;` |
|       - | 1249 | `	}` |
|       - | 1250 | `	/* The activation's hold on the function it was running, the same one` |
|       - | 1251 | `	 * VmLeaveFrame gives back for a live frame. */` |
|    1037 | 1252 | `	if( pFrame->pUserData ){` |
|    1037 | 1253 | `		PH7_VmClosureFuncUnref(pVm,(ph7_vm_func *)pFrame->pUserData);` |
|    1037 | 1254 | `		pFrame->pUserData = 0;` |
|     516 | 1255 | `	}` |
|       - | 1256 | `	/* End the foreach walks this (abandoned) activation never finished — the same` |
|       - | 1257 | `	 * teardown, at the same point, VmLeaveFrame does it at. */` |
|    1037 | 1258 | `	VmReleaseFrameForeachSteps(pVm,pFrame);` |
|       - | 1259 | `	/* Remove local references FIRST, then free the locals nothing else holds — the` |
|       - | 1260 | `	 * order and the holder test VmLeaveFrame explains. */` |
|    1037 | 1261 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);` |
|    2065 | 1262 | `	for( n = 0; n < SySetUsed(&pFrame->sRef); ++n ){` |
|    1033 | 1263 | `		PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);` |
|     519 | 1264 | `	}` |
|       - | 1265 | `	/* Free local variables */` |
|    1037 | 1266 | `	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);` |
|    2033 | 1267 | `	for( n = 0; n < SySetUsed(&pFrame->sLocal); ++n ){` |
|    1001 | 1268 | `		if( PH7_VmSlotHolderCount(pVm, aSlot[n].nIdx) > 0 ){` |
|     ! 0 | 1269 | `			continue;` |
|       - | 1270 | `		}` |
|    1001 | 1271 | `		PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);` |
|     503 | 1272 | `	}` |
|    1037 | 1273 | `	SyHashRelease(&pFrame->hVar);` |
|    1037 | 1274 | `	SySetRelease(&pFrame->sArg);` |
|    1037 | 1275 | `	SySetRelease(&pFrame->sLocal);` |
|    1037 | 1276 | `	SySetRelease(&pFrame->sRef);` |
|    1037 | 1277 | `	PH7_MemObjRelease(&pFrame->sRet);` |
|       - | 1278 | `	/* Drop a resume target pointing at this detached frame before we free it (ROOT B). */` |
|    1037 | 1279 | `	VmDropResumeTarget(pVm,pFrame);` |
|    1037 | 1280 | `	SyMemBackendPoolFree(&pVm->sAllocator, pFrame);` |
|     521 | 1281 | `}` |
|       - | 1282 | `/*` |
|       - | 1283 | ` * Free a parked deep-suspend segment (stage 4) whose fiber was abandoned while` |
|       - | 1284 | ` * suspended. Every record holds a callee's operand stack and VmFrame (the` |
|       - | 1285 | ` * topmost record's callee is the innermost activation, running on sState); walk` |
|       - | 1286 | ` * the chain releasing each callee stack's live entries then the stack and frame.` |
|       - | 1287 | ` * The body frame/stack are NOT here — they are freed by the caller` |
|       - | 1288 | ` * (VmReleaseExecCtx) as pCtx->pFrame / pCtx->pStack.` |
|       - | 1289 | ` */` |
|     ! 0 | 1290 | `static void VmFreeParkedSegment(ph7_vm *pVm, ph7_exec_ctx *pCtx, VmParkedSegment *pSeg)` |
|     ! 0 | 1291 | `{` |
|       - | 1292 | `	/* Live top-of-stack of the activation running on the current record's callee` |
|       - | 1293 | `	 * stack: the innermost (sState) for the topmost record, then each caller. */` |
|     ! 0 | 1294 | `	ph7_value *pTosAbove = pSeg->sState.pTos;` |
|     ! 0 | 1295 | `	VmCallFrame *pRec = pSeg->pCallTop, *pNext;` |
|     ! 0 | 1296 | `	while( pRec ){` |
|     ! 0 | 1297 | `		ph7_value *pStk = pRec->sCall.pFrameStack;` |
|     ! 0 | 1298 | `		if( pStk ){` |
|     ! 0 | 1299 | `			ph7_value *pTos = pTosAbove;` |
|     ! 0 | 1300 | `			while( pTos >= pStk ){` |
|     ! 0 | 1301 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 1302 | `				pTos--;` |
|     ! 0 | 1303 | `			}` |
|     ! 0 | 1304 | `			SyMemBackendFree(&pVm->sAllocator, pStk);` |
|     ! 0 | 1305 | `		}` |
|     ! 0 | 1306 | `		VmFreeDetachedFrame(pVm, pRec->sCall.pFrame);` |
|       - | 1307 | `		/* The caller recorded here runs on the NEXT-lower callee stack; grab its` |
|       - | 1308 | `		 * live tos before freeing this node. */` |
|     ! 0 | 1309 | `		pTosAbove = pRec->sCaller.pTos;` |
|     ! 0 | 1310 | `		pNext = pRec->pPrev;` |
|     ! 0 | 1311 | `		SyMemBackendPoolFree(&pVm->sAllocator, pRec);` |
|     ! 0 | 1312 | `		pRec = pNext;` |
|     ! 0 | 1313 | `	}` |
|       - | 1314 | `	/* pTosAbove now points at the BODY activation's live top (the bottom record's` |
|       - | 1315 | `	 * sCaller, which runs on pCtx->pStack). pCtx->nTos still holds the INNERMOST` |
|       - | 1316 | `	 * index (VmSuspendCtx saved it), which would over-index the body stack in` |
|       - | 1317 | `	 * VmReleaseExecCtx's release loop — correct it to the body's real top. */` |
|     ! 0 | 1318 | `	pCtx->nTos = (sxi32)(pTosAbove - pCtx->pStack);` |
|     ! 0 | 1319 | `	SyMemBackendFree(&pVm->sAllocator, pSeg);` |
|     ! 0 | 1320 | `}` |
|       - | 1321 | `/*` |
|       - | 1322 | ` * Release an execution context and all its resources.` |
|       - | 1323 | ` */` |
|    1032 | 1324 | `PH7_PRIVATE void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 | 1325 | `{` |
|    1037 | 1326 | `	if( pCtx == 0 ){` |
|     ! 0 | 1327 | `		return;` |
|       - | 1328 | `	}` |
|    1037 | 1329 | `	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|       - | 1330 | `		/* Cannot destroy a fiber that is currently executing */` |
|     ! 0 | 1331 | `		return;` |
|       - | 1332 | `	}` |
|       - | 1333 | `#ifdef PH7_CORO_STACK` |
|    1037 | 1334 | `	if( pCtx->pCoro && pCtx->iState == PH7_CTX_STATE_SUSPENDED && !pCtx->bCoroDone ){` |
|       - | 1335 | `		/* A fiber abandoned while suspended still has live C frames on its own` |
|       - | 1336 | `		 * stack -- a half-finished array_map, an open handle, an operand stack of` |
|       - | 1337 | `		 * its own -- and nothing outside that stack can reach them. Switch back in` |
|       - | 1338 | `		 * one last time with the kill flag set: the suspend it is parked on` |
|       - | 1339 | `		 * returns PH7_ABORT, every frame between there and the body entry runs its` |
|       - | 1340 | `		 * own abort path, and the body returns for good. VmFinishCtxRun then frees` |
|       - | 1341 | `		 * the stack. (php unwinds a dropped fiber for the same reason; that its` |
|       - | 1342 | ``		 * unwind also runs the body's `finally` blocks and this one does not is`` |
|       - | 1343 | `		 * recorded separately -- an abort is not a return.) */` |
|     224 | 1344 | `		pCtx->bCoroKill = 1;` |
|     224 | 1345 | `		(void)VmResumeCtx(pVm, pCtx, 0, 0);` |
|     110 | 1346 | `	}` |
|    1037 | 1347 | `	if( pCtx->pCoro ){` |
|       - | 1348 | `		/* Never started, or the unwind above could not run it: the stack holds` |
|       - | 1349 | `		 * nothing live either way. */` |
|     ! 0 | 1350 | `		VmCoroFree(pVm, pCtx->pCoro);` |
|     ! 0 | 1351 | `		pCtx->pCoro = 0;` |
|     ! 0 | 1352 | `	}` |
|    1037 | 1353 | `	if( pCtx->pEscaped ){` |
|       - | 1354 | `		/* The body threw and nobody was left to re-raise it (the resumer aborted` |
|       - | 1355 | `		 * between the two). Give the reference back. */` |
|     ! 0 | 1356 | `		PH7_ClassInstanceUnref(pCtx->pEscaped);` |
|     ! 0 | 1357 | `		pCtx->pEscaped = 0;` |
|     ! 0 | 1358 | `	}` |
|    1037 | 1359 | `	VmCoroStateRelease(pVm, &pCtx->sSaved);` |
|       - | 1360 | `#endif` |
|    1037 | 1361 | `	pCtx->iState = PH7_CTX_STATE_CLOSED;` |
|       - | 1362 | `	/* ...and give back the hold VmNewExecCtx took on the function this coroutine runs. */` |
|    1037 | 1363 | `	if( pCtx->pFunc ){` |
|    1037 | 1364 | `		PH7_VmClosureFuncUnref(pVm,pCtx->pFunc);` |
|     516 | 1365 | `	}` |
|       - | 1366 | `	/* Release values */` |
|    1037 | 1367 | `	PH7_MemObjRelease(&pCtx->sSuspendValue);` |
|    1037 | 1368 | `	PH7_MemObjRelease(&pCtx->sRetValue);` |
|    1037 | 1369 | `	PH7_MemObjRelease(&pCtx->sDelegate);` |
|       - | 1370 | `	/* Stage 2b: the parked entries are per-activation clones now — free them` |
|       - | 1371 | `	 * (an abandoned suspended coroutine is their last holder). */` |
|    1037 | 1372 | `	VmExcReleaseAll(pVm,&pCtx->aSavedException);` |
|    1037 | 1373 | `	SySetRelease(&pCtx->aSavedException);` |
|       - | 1374 | `	/* ROOT C: a generator abandoned while suspended inside a finally may carry parked` |
|       - | 1375 | `	 * finally actions holding owned values/refs (a RETURN's sRet, a RETHROW's pExc).` |
|       - | 1376 | `	 * Release them so the abandon path leaks nothing. */` |
|       - | 1377 | `	{` |
|    1037 | 1378 | `		sxu32 n = SySetUsed(&pCtx->aSavedFinally);` |
|    1037 | 1379 | `		if( n > 0 ){` |
|     ! 0 | 1380 | `			VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pCtx->aSavedFinally);` |
|       - | 1381 | `			sxu32 i;` |
|     ! 0 | 1382 | `			for( i = 0; i < n; i++ ){` |
|     ! 0 | 1383 | `				if( aA[i].eKind == PH7_FA_RETURN ){` |
|     ! 0 | 1384 | `					PH7_MemObjRelease(&aA[i].sRet);` |
|     ! 0 | 1385 | `				}else if( aA[i].eKind == PH7_FA_RETHROW && aA[i].pExc ){` |
|     ! 0 | 1386 | `					PH7_ClassInstanceUnref(aA[i].pExc);` |
|     ! 0 | 1387 | `				}` |
|     ! 0 | 1388 | `			}` |
|     ! 0 | 1389 | `		}` |
|    1037 | 1390 | `		SySetRelease(&pCtx->aSavedFinally);` |
|       - | 1391 | `	}` |
|       - | 1392 | `	/* Stage 4: parked aSelf entries are borrowed class pointers — just free the set. */` |
|    1037 | 1393 | `	SySetRelease(&pCtx->aSavedSelf);` |
|       - | 1394 | `	/* Free a parked deep-suspend segment (stage 4): the fiber was abandoned while` |
|       - | 1395 | `	 * suspended inside a nested call, so its record chain / frames / operand` |
|       - | 1396 | `	 * stacks are still alive and only this holder references them. Must run` |
|       - | 1397 | `	 * before the body frame/stack below (they are the segment's floor). */` |
|    1037 | 1398 | `	if( pCtx->pParkedSegment ){` |
|     ! 0 | 1399 | `		VmFreeParkedSegment(pVm, pCtx, (VmParkedSegment *)pCtx->pParkedSegment);` |
|     ! 0 | 1400 | `		pCtx->pParkedSegment = 0;` |
|     ! 0 | 1401 | `	}` |
|       - | 1402 | `	/* Release the frame if it's detached (not in the VM chain) */` |
|    1037 | 1403 | `	if( pCtx->pFrame ){` |
|    1037 | 1404 | `		VmFreeDetachedFrame(pVm, pCtx->pFrame);` |
|    1037 | 1405 | `		pCtx->pFrame = 0;` |
|     516 | 1406 | `	}` |
|       - | 1407 | `	/* A by-reference parameter aliased a CALLER's slot; the frame teardown above just` |
|       - | 1408 | `	 * dropped this body's name for it. Release it if nothing is left holding it — the` |
|       - | 1409 | `	 * caller may already be gone (it skipped the slot precisely because this frame` |
|       - | 1410 | `	 * held it), in which case this is its last holder. Runs after the frame so the` |
|       - | 1411 | `	 * body's own row is out of the count. */` |
|       - | 1412 | `	{` |
|       - | 1413 | `		sxu32 n;` |
|    1037 | 1414 | `		sxu32 *aIdx = (sxu32 *)SySetBasePtr(&pCtx->aByRefArg);` |
|    1071 | 1415 | `		for( n = 0; n < SySetUsed(&pCtx->aByRefArg); n++ ){` |
|      37 | 1416 | `			PH7_VmReleaseUnheldSlot(pVm, aIdx[n]);` |
|      20 | 1417 | `		}` |
|    1037 | 1418 | `		SySetRelease(&pCtx->aByRefArg);` |
|       - | 1419 | `	}` |
|       - | 1420 | `	/* Release individual operand stack entries (decrement refcounts,` |
|       - | 1421 | `	 * free string buffers, etc.) before bulk-freeing the stack memory.` |
|       - | 1422 | `	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */` |
|    1037 | 1423 | `	if( pCtx->pStack ){` |
|    1037 | 1424 | `		if( pCtx->nTos >= 0 ){` |
|     609 | 1425 | `			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];` |
|    1239 | 1426 | `			while( pTos >= pCtx->pStack ){` |
|     635 | 1427 | `				PH7_MemObjRelease(pTos);` |
|     635 | 1428 | `				pTos--;` |
|       5 | 1429 | `			}` |
|     302 | 1430 | `		}` |
|    1037 | 1431 | `		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);` |
|    1037 | 1432 | `		pCtx->pStack = 0;` |
|     516 | 1433 | `	}` |
|       - | 1434 | `	/* Free the context itself */` |
|    1037 | 1435 | `	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);` |
|     521 | 1436 | `}` |
|       - | 1437 | `/*` |
|       - | 1438 | ` * Helper: extract the ph7_exec_ctx from a Fiber class instance.` |
|       - | 1439 | ` * Returns NULL if the object is not a Fiber or has no context.` |
|       - | 1440 | ` */` |
|    1194 | 1441 | `static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)` |
|       5 | 1442 | `{` |
|       - | 1443 | `	ph7_class_instance *pThis;` |
|       - | 1444 | `	SyString sAttr;` |
|       - | 1445 | `	ph7_value *pAttr;` |
|    1199 | 1446 | `	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 1447 | `		return 0;` |
|       - | 1448 | `	}` |
|    1199 | 1449 | `	pThis = (ph7_class_instance *)pFiberObj->x.pOther;` |
|    1199 | 1450 | `	if( pThis->pClass != pVm->pFiberClass ){` |
|     ! 0 | 1451 | `		return 0;` |
|       - | 1452 | `	}` |
|    1199 | 1453 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    1199 | 1454 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    1199 | 1455 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|     387 | 1456 | `		return 0;` |
|       - | 1457 | `	}` |
|     817 | 1458 | `	return (ph7_exec_ctx *)pAttr->x.pOther;` |
|     602 | 1459 | `}` |
|       - | 1460 | `/* The three VM_INSTANCE_FCC_* Closure flags live in ph7int.h: vm_exec.c's OP_LOAD_FCC` |
|       - | 1461 | ` * stamps VM_INSTANCE_FCC_METHOD and this file reads all three. */` |
|       - | 1462 | `/*` |
|       - | 1463 | `` * A PHP closure (and a first-class callable `f(...)`) is a real object: an instance of`` |
|       - | 1464 | `` * the built-in final `Closure` class carrying its underlying callable in a private`` |
|       - | 1465 | `` * `$__fn` attribute (the callable NAME: a registered `[closure_N]`/`[lambda_N]` or a`` |
|       - | 1466 | ` * user/host function name) — plus, for a method/static first-class callable, a bound` |
|       - | 1467 | `` * `$__this` object and/or a `$__scope` class-name. Storing the callable as plain attributes`` |
|       - | 1468 | `` * (rather than a native resource pointer) means the object owns no `ph7_vm_func` — the`` |
|       - | 1469 | `` * per-closure function stays owned by `hFunction`/VM-release exactly as before, so there is`` |
|       - | 1470 | ` * no extra free path.` |
|       - | 1471 | ` *` |
|       - | 1472 | ` * Returns non-zero iff pVal is a Closure instance.` |
|       - | 1473 | ` */` |
| 9623279 | 1474 | `PH7_PRIVATE int VmValueIsClosure(ph7_vm *pVm, ph7_value *pVal)` |
|       5 | 1475 | `{` |
|       - | 1476 | `	ph7_class_instance *pThis;` |
|       - | 1477 | `	/* Flag test first: a non-object call target (the hot common case) bails before any` |
|       - | 1478 | `	 * pVm dereference; pClosureClass==0 is a one-time pre-init concern, so it goes last. */` |
| 9623284 | 1479 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
| 9301793 | 1480 | `		return 0;` |
|       - | 1481 | `	}` |
|  321496 | 1482 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|       - | 1483 | `	/* Closure is final, so an exact class match is correct (no subclasses possible). */` |
|  321496 | 1484 | `	return pThis->pClass == pVm->pClosureClass;` |
| 4811899 | 1485 | `}` |
|       - | 1486 | `/*` |
|       - | 1487 | ` * Unwrap a Closure value into the simple callable the existing dispatch machinery` |
|       - | 1488 | ` * already understands, written into pOut (which the caller must have initialised):` |
|       - | 1489 | `` *   - bound `$this` set (method first-class callable) -> [ $__this, $__fn ] array callable`` |
|       - | 1490 | `` *   - `$__scope` set (static first-class callable)     -> [ $__scope, $__fn ] array callable`` |
|       - | 1491 | `` *   - neither (plain function / real closure)          -> the `$__fn` name string`` |
|       - | 1492 | ` * The 2-element array form rides the existing MEMOBJ_HASHMAP dispatch, which binds $this` |
|       - | 1493 | ` * for an object first element and resolves the class for a class-name-string first element.` |
|       - | 1494 | ` * Returns SXRET_OK if pVal was a Closure (pOut filled), SXERR_NOTFOUND otherwise.` |
|       - | 1495 | ` */` |
|   39837 | 1496 | `PH7_PRIVATE sxi32 VmClosureUnwrap(ph7_vm *pVm, ph7_value *pVal, ph7_value *pOut)` |
|       5 | 1497 | `{` |
|       - | 1498 | `	ph7_class_instance *pThis;` |
|       - | 1499 | `	ph7_value *pFn;` |
|       - | 1500 | `	SyString sAttr;` |
|   39842 | 1501 | `	if( !VmValueIsClosure(pVm, pVal) ){` |
|     ! 0 | 1502 | `		return SXERR_NOTFOUND;` |
|       - | 1503 | `	}` |
|   39842 | 1504 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|   39842 | 1505 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|   39842 | 1506 | `	pFn = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|   39842 | 1507 | `	if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|     ! 0 | 1508 | `		return SXERR_NOTFOUND; /* malformed/uninitialised closure */` |
|       - | 1509 | `	}` |
|       - | 1510 | `	/* Only a bound/static first-class callable (rare) carries $__this/$__scope; the` |
|       - | 1511 | `	 * VM_INSTANCE_FCC_BOUND flag (set by VmCreateClosure) keeps the hot plain-closure path` |
|       - | 1512 | `	 * to the single $__fn lookup above instead of two extra attribute lookups per dispatch. */` |
|   39842 | 1513 | `	if( pThis->iFlags & VM_INSTANCE_FCC_BOUND ){` |
|       - | 1514 | `		ph7_value *pBound, *pScope;` |
|       - | 1515 | `		int bBoundObj, bScope;` |
|     281 | 1516 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     281 | 1517 | `		pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     281 | 1518 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|     281 | 1519 | `		pScope = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     281 | 1520 | `		bBoundObj = pBound && (pBound->iFlags & MEMOBJ_OBJ);` |
|     281 | 1521 | `		bScope = pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0;` |
|     281 | 1522 | `		if( bBoundObj \|\| bScope ){` |
|       - | 1523 | `			/* Method/static first-class callable -> [ target, "method" ] array callable. */` |
|     281 | 1524 | `			if( pThis->iFlags & VM_INSTANCE_FCC_INVOKE_OBJ ){` |
|       - | 1525 | `				/* Closure::fromCallable($obj): the engine named __invoke, so this` |
|       - | 1526 | `				 * dispatch is the engine's own and a non-public one still runs. */` |
|      12 | 1527 | `				pVm->bMagicDispatch = 1;` |
|       5 | 1528 | `			}` |
|       - | 1529 | `			ph7_hashmap *pMap;` |
|       - | 1530 | `			ph7_value sTarget, sMeth;` |
|       - | 1531 | `			sxi32 rc;` |
|     281 | 1532 | `			if( bBoundObj ){` |
|     203 | 1533 | `				ph7_class_instance *pBoundObj = (ph7_class_instance *)pBound->x.pOther;` |
|       - | 1534 | ``				/* A bound PLAIN closure (`function(){…}->bindTo($o)`): $__fn names a function, not a`` |
|       - | 1535 | `				 * method of the bound object's class, so a [obj,method] array callable would fail` |
|       - | 1536 | `				 * method resolution. Stash the bound object (own a ref) so the OP_CALL user-function` |
|       - | 1537 | `				 * frame setup injects it as $this, and return the plain $__fn string for a normal` |
|       - | 1538 | `				 * function dispatch. */` |
|     198 | 1539 | `				if( (pThis->iFlags & VM_INSTANCE_FCC_METHOD) == 0` |
|     146 | 1540 | `				 && PH7_ClassExtractMethod(pBoundObj->pClass,` |
|     126 | 1541 | `						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0 ){` |
|       - | 1542 | `					/* Only a USER function (anonymous closure / named fn in hFunction) reads $this and` |
|       - | 1543 | `					 * reaches the OP_CALL user-function frame-setup that consumes pClosureThis. A HOST` |
|       - | 1544 | `					 * function (e.g. Closure::fromCallable('strlen')->bindTo($o)) ignores $this and` |
|       - | 1545 | `					 * dispatches via the host path which never consumes the transient — setting it` |
|       - | 1546 | `					 * there would leak the ref and inject a stale $this into the next call. */` |
|     111 | 1547 | `					if( SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),` |
|      79 | 1548 | `							SyBlobLength(&pFn->sBlob)) != 0 ){` |
|      79 | 1549 | `						pBoundObj->iRef++;` |
|      79 | 1550 | `						pVm->pClosureThis = pBoundObj;` |
|       - | 1551 | `						/* Carry the bound scope (if set) so private/protected member access inside` |
|       - | 1552 | `						 * the closure body resolves against it — bindTo($o, Scope::class) / call($o). */` |
|      79 | 1553 | `						if( bScope ){` |
|      92 | 1554 | `							pVm->pClosureScope = PH7_VmExtractClass(pVm,` |
|      58 | 1555 | `								(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      29 | 1556 | `						}` |
|      37 | 1557 | `					}` |
|      79 | 1558 | `					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      79 | 1559 | `					return SXRET_OK;` |
|       - | 1560 | `				}` |
|      66 | 1561 | `			}else{` |
|       - | 1562 | ``				/* Scope-only rebind of a PLAIN closure (`bindTo(null, Scope::class)`):`` |
|       - | 1563 | `				 * $__fn names a function, not a static method of the scope class, so` |
|       - | 1564 | `				 * the [scope, method] array callable below would fail method` |
|       - | 1565 | `				 * resolution. Dispatch the plain $__fn string and carry the scope for` |
|       - | 1566 | `				 * private/protected visibility (pClosureThis stays unset — no $this).` |
|       - | 1567 | `				 * A static-method FCC ($__fn really is a method of the scope class)` |
|       - | 1568 | `				 * falls through to the array-callable path. */` |
|     120 | 1569 | `				ph7_class *pScopeClass = PH7_VmExtractClass(pVm,` |
|      78 | 1570 | `					(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      78 | 1571 | `				if( pScopeClass == 0` |
|      81 | 1572 | `				 \|\| ((pThis->iFlags & VM_INSTANCE_FCC_METHOD) == 0` |
|      43 | 1573 | `				  && PH7_ClassExtractMethod(pScopeClass,` |
|      12 | 1574 | `						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0) ){` |
|       8 | 1575 | `					if( pScopeClass` |
|       9 | 1576 | `					 && SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),` |
|       8 | 1577 | `							SyBlobLength(&pFn->sBlob)) != 0 ){` |
|       9 | 1578 | `						pVm->pClosureScope = pScopeClass;` |
|       4 | 1579 | `					}` |
|       9 | 1580 | `					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|       9 | 1581 | `					return SXRET_OK;` |
|       - | 1582 | `				}` |
|       - | 1583 | `			}` |
|     198 | 1584 | `			pMap = PH7_NewHashmap(&(*pVm), 0, 0);` |
|     198 | 1585 | `			if( pMap == 0 ){` |
|     ! 0 | 1586 | `				return SXERR_NOTFOUND;` |
|       - | 1587 | `			}` |
|     198 | 1588 | `			PH7_MemObjInit(pVm, &sTarget);` |
|     198 | 1589 | `			PH7_MemObjInit(pVm, &sMeth);` |
|     198 | 1590 | `			if( bBoundObj ){` |
|     128 | 1591 | `				PH7_MemObjStore(pBound, &sTarget); /* bound object (iRef++) -> binds $this */` |
|      66 | 1592 | `			}else{` |
|      73 | 1593 | `				PH7_MemObjStringAppend(&sTarget, SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob));` |
|       - | 1594 | `			}` |
|     198 | 1595 | `			PH7_MemObjStringAppend(&sMeth, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|     198 | 1596 | `			rc = PH7_HashmapInsert(pMap, 0, &sTarget);` |
|     198 | 1597 | `			if( rc == SXRET_OK ){` |
|     198 | 1598 | `				rc = PH7_HashmapInsert(pMap, 0, &sMeth);` |
|      97 | 1599 | `			}` |
|     198 | 1600 | `			PH7_MemObjRelease(&sTarget);` |
|     198 | 1601 | `			PH7_MemObjRelease(&sMeth);` |
|     198 | 1602 | `			if( rc != SXRET_OK ){` |
|     ! 0 | 1603 | `				PH7_HashmapRelease(pMap, TRUE); /* free the partial map, no leak */` |
|     ! 0 | 1604 | `				return SXERR_NOTFOUND;` |
|       - | 1605 | `			}` |
|     198 | 1606 | `			if( pThis->iFlags & VM_INSTANCE_FCC_SCREENED ){` |
|       - | 1607 | `				/* php resolves a method Closure's callee ONCE, where the closure is built, and` |
|       - | 1608 | ``				 * keeps the resolved function: an escaped `$this->priv(...)` runs anywhere. PHL`` |
|       - | 1609 | `				 * keeps only a NAME, so every dispatch site would re-decide visibility against the` |
|       - | 1610 | `				 * CALLER and refuse the closure php runs. The creation sites screen (OP_LOAD_FCC's` |
|       - | 1611 | `				 * VmFccMemberError, Closure::fromCallable's PH7_VmIsCallable gate, and reflection,` |
|       - | 1612 | `				 * which php lets past protection on purpose) and stamp the mark only when a real` |
|       - | 1613 | `				 * method answered — a name the class reaches through __call carries no mark and` |
|       - | 1614 | `				 * still routes to the catch-all. Armed only once the pair below really exists, so` |
|       - | 1615 | `				 * a failed build cannot leave it standing; the consumers clear it like` |
|       - | 1616 | `				 * pClosureThis/pClosureScope. */` |
|     159 | 1617 | `				pVm->bClosureScreened = 1;` |
|      78 | 1618 | `			}` |
|     198 | 1619 | `			pOut->x.pOther = pMap;` |
|     198 | 1620 | `			MemObjSetType(pOut, MEMOBJ_HASHMAP);` |
|     198 | 1621 | `			return SXRET_OK;` |
|       - | 1622 | `		}` |
|     ! 0 | 1623 | `	}` |
|   39566 | 1624 | `	PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|   39566 | 1625 | `	return SXRET_OK;` |
|   19696 | 1626 | `}` |
|       - | 1627 | `/*` |
|       - | 1628 | ` * php's SCOPE for a Closure — the class whose private members its body may reach, and the` |
|       - | 1629 | ` * class Reflection reports. For a plain closure that is the class it was bound to; for a` |
|       - | 1630 | ` * METHOD closure it is the class that DECLARED the method, which is not the class the` |
|       - | 1631 | `` * callable NAMED: `(new Kid)->mk()` returning `$this->basePriv(...)` is scoped to Base in`` |
|       - | 1632 | `` * php, while `$__scope` records Kid — the class the call goes THROUGH, which is the CALLED`` |
|       - | 1633 | `` * scope (`static::`) and a different question. A trait method is composed into the using`` |
|       - | 1634 | ` * class, so PH7_VmMethodScopeName answers for it exactly as it does at every refusal site.` |
|       - | 1635 | ` * Returns 0 when the closure carries no scope at all.` |
|       - | 1636 | ` */` |
|      92 | 1637 | `PH7_PRIVATE ph7_class * PH7_VmClosureScopeClass(ph7_vm *pVm, ph7_class_instance *pClosure)` |
|       2 | 1638 | `{` |
|       - | 1639 | `	SyString sAttr;` |
|       - | 1640 | `	ph7_value *pScope, *pFn;` |
|       - | 1641 | `	ph7_class *pClass;` |
|      94 | 1642 | `	if( pClosure == 0 ){` |
|     ! 0 | 1643 | `		return 0;` |
|       - | 1644 | `	}` |
|      94 | 1645 | `	SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      94 | 1646 | `	pScope = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|      94 | 1647 | `	if( pScope == 0 \|\| (pScope->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pScope->sBlob) == 0 ){` |
|     ! 0 | 1648 | `		return 0;` |
|       - | 1649 | `	}` |
|     140 | 1650 | `	pClass = PH7_VmExtractClass(pVm, (const char *)SyBlobData(&pScope->sBlob),` |
|      46 | 1651 | `		SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|      94 | 1652 | `	if( pClass == 0 \|\| (pClosure->iFlags & VM_INSTANCE_FCC_METHOD) == 0 ){` |
|       7 | 1653 | `		return pClass;` |
|       - | 1654 | `	}` |
|      88 | 1655 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|      88 | 1656 | `	pFn = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|      88 | 1657 | `	if( pFn && (pFn->iFlags & MEMOBJ_STRING) && SyBlobLength(&pFn->sBlob) > 0 ){` |
|     131 | 1658 | `		ph7_class_method *pMeth = PH7_ClassExtractMethod(pClass,` |
|      86 | 1659 | `			(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      88 | 1660 | `		if( pMeth ){` |
|      82 | 1661 | `			return PH7_VmMethodScopeName(pVm, pClass, pMeth);` |
|       - | 1662 | `		}` |
|       3 | 1663 | `	}` |
|       7 | 1664 | `	return pClass;` |
|      48 | 1665 | `}` |
|       - | 1666 | `/*` |
|       - | 1667 | `` * Resolve the scope class for a STATIC first-class callable `T::m(...)`, where T is a`` |
|       - | 1668 | ` * class-name STRING value: handles the self/static/parent keywords against the live class` |
|       - | 1669 | ` * context (so the actual class is bound at FCC-creation time, like PHP), and falls back to` |
|       - | 1670 | ` * an explicit class-name lookup. Mirrors the static OP_MEMBER resolution. Returns 0 if the` |
|       - | 1671 | ` * class cannot be resolved.` |
|       - | 1672 | ` */` |
|     160 | 1673 | `PH7_PRIVATE ph7_class * VmFccResolveScope(ph7_vm *pVm, ph7_value *pTarget)` |
|       4 | 1674 | `{` |
|     324 | 1675 | `	return PH7_VmResolveScopeName(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),` |
|     160 | 1676 | `		(sxu32)SyBlobLength(&pTarget->sBlob));` |
|       4 | 1677 | `}` |
|       - | 1678 | `/*` |
|       - | 1679 | ` * The same resolution over a raw (name, length) pair, for the callable machinery: php` |
|       - | 1680 | `` * resolves `self`/`parent`/`static` in a CALLBACK (is_callable, call_user_func, array_map …)`` |
|       - | 1681 | `` * against the live class context, and refuses them in the direct `$cb()` dispatch — so this`` |
|       - | 1682 | ` * is deliberately NOT wired into the OP_CALL resolve check, which must keep answering` |
|       - | 1683 | `` * `Class "self" not found`.`` |
|       - | 1684 | ` */` |
|       - | 1685 | `/*` |
|       - | 1686 | `` * A Closure object is going: give back its hold on the function `$__fn` names.`` |
|       - | 1687 | ` * A run-time closure's per-instantiation ph7_vm_func belongs to the objects that` |
|       - | 1688 | ` * name it, and this is where the last of them lets go.` |
|       - | 1689 | ` */` |
|   19280 | 1690 | `static void VmClosureRelease(ph7_vm *pVm, ph7_class_instance *pThis)` |
|       5 | 1691 | `{` |
|   19285 | 1692 | `	PH7_VmClosureInstanceRef(pVm, pThis, -1);` |
|   19285 | 1693 | `}` |
|  102152 | 1694 | `PH7_PRIVATE ph7_class * PH7_VmResolveScopeName(ph7_vm *pVm, const char *zCls, sxu32 nCls)` |
|       5 | 1695 | `{` |
|       - | 1696 | `	ph7_class *pClass;` |
|  102157 | 1697 | `	if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|     142 | 1698 | `		pClass = PH7_VmPeekSelfClass(&(*pVm)); /* self:: in a trait -> the USING class */` |
|  102088 | 1699 | `	}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|      82 | 1700 | `		pClass = PH7_VmPeekTopClass(&(*pVm));     /* late static binding */` |
|  101980 | 1701 | `	}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|      54 | 1702 | `		pClass = PH7_VmResolveParentClass(&(*pVm));` |
|      29 | 1703 | `	}else{` |
|  101891 | 1704 | `		pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|       - | 1705 | `	}` |
|  102157 | 1706 | `	return pClass;` |
|       5 | 1707 | `}` |
|       - | 1708 | `/*` |
|       - | 1709 | ` * Create a Closure object wrapping a callable name (+ optional bound $this object and/or` |
|       - | 1710 | `` * scope class-name, for the method/static first-class callables `$o->m(...)`/`C::m(...)`).`` |
|       - | 1711 | ` * Mirrors the Generator/Fiber "object carries its state in private attributes" pattern.` |
|       - | 1712 | ` * Returns the fresh instance holding ONE reference, which the caller's value TAKES --` |
|       - | 1713 | ` * the same handover PH7_NewClassInstance makes to OP_NEW. (It used to say the caller` |
|       - | 1714 | ` * had to add one, and every closure site did, so no Closure object ever reached zero.)` |
|       - | 1715 | ` */` |
|   19819 | 1716 | `PH7_PRIVATE ph7_class_instance * VmCreateClosure(ph7_vm *pVm, const SyString *pName,` |
|       - | 1717 | `	ph7_class_instance *pBoundThis, const SyString *pScope)` |
|       5 | 1718 | `{` |
|       - | 1719 | `	ph7_class_instance *pObj;` |
|       - | 1720 | `	ph7_value *pAttr;` |
|       - | 1721 | `	SyString sAttr;` |
|   19824 | 1722 | `	if( pVm->pClosureClass == 0 ){` |
|     ! 0 | 1723 | `		return 0;` |
|       - | 1724 | `	}` |
|   19824 | 1725 | `	pObj = PH7_NewClassInstance(&(*pVm), pVm->pClosureClass);` |
|   19824 | 1726 | `	if( pObj == 0 ){` |
|     ! 0 | 1727 | `		return 0;` |
|       - | 1728 | `	}` |
|   19824 | 1729 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|   19824 | 1730 | `	pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|   19824 | 1731 | `	if( pAttr ){` |
|   19824 | 1732 | `		PH7_MemObjStringAppend(pAttr, pName->zString, pName->nByte);` |
|       - | 1733 | `		/* This object is now a holder of the function it names. For a run-time` |
|       - | 1734 | `		 * closure that is what keeps the per-instantiation copy alive, and losing` |
|       - | 1735 | `		 * it is what frees the copy. */` |
|   19824 | 1736 | `		PH7_VmClosureInstanceRef(pVm, pObj, 1);` |
|    9788 | 1737 | `	}` |
|   19824 | 1738 | `	if( pBoundThis ){` |
|     167 | 1739 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     167 | 1740 | `		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|     167 | 1741 | `		if( pAttr ){` |
|     167 | 1742 | `			pAttr->x.pOther = pBoundThis;` |
|     167 | 1743 | `			MemObjSetType(pAttr, MEMOBJ_OBJ);` |
|     167 | 1744 | `			pBoundThis->iRef++; /* keep the bound object alive for the closure's lifetime */` |
|      81 | 1745 | `		}` |
|      81 | 1746 | `	}` |
|   19824 | 1747 | `	if( pScope && pScope->nByte ){` |
|     255 | 1748 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|     255 | 1749 | `		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);` |
|     255 | 1750 | `		if( pAttr ){` |
|     255 | 1751 | `			PH7_MemObjStringAppend(pAttr, pScope->zString, pScope->nByte);` |
|     125 | 1752 | `		}` |
|     125 | 1753 | `	}` |
|   19824 | 1754 | `	if( pBoundThis \|\| (pScope && pScope->nByte) ){` |
|       - | 1755 | `		/* Mark bound/static FCC closures so VmClosureUnwrap can skip the $__this/$__scope` |
|       - | 1756 | `		 * lookups on the hot plain-closure dispatch path. */` |
|     255 | 1757 | `		pObj->iFlags \|= VM_INSTANCE_FCC_BOUND;` |
|     125 | 1758 | `	}` |
|   19824 | 1759 | `	return pObj;` |
|    9793 | 1760 | `}` |
|       - | 1761 | `/*` |
|       - | 1762 | ` * Exported wrapper around VmCreateClosure for builtin libraries outside this` |
|       - | 1763 | ` * file (ReflectionFunction::getClosure / ReflectionMethod::getClosure).` |
|       - | 1764 | ` */` |
|      20 | 1765 | `PH7_PRIVATE ph7_class_instance * PH7_VmNewClosure(ph7_vm *pVm, const SyString *pName,` |
|       - | 1766 | `	ph7_class_instance *pBoundThis, const SyString *pScope)` |
|       2 | 1767 | `{` |
|      22 | 1768 | `	return VmCreateClosure(&(*pVm), pName, pBoundThis, pScope);` |
|       2 | 1769 | `}` |
|       - | 1770 | `/*` |
|       - | 1771 | ` * Exported wrapper around the typed/readonly property store enforcement for` |
|       - | 1772 | ` * ReflectionProperty::setValue (vm_builtin_reflection.c). Same contract:` |
|       - | 1773 | ` * SXRET_OK (value possibly coerced in place), PH7_EXCEPTION, or PH7_ABORT.` |
|       - | 1774 | ` */` |
|      10 | 1775 | `PH7_PRIVATE sxi32 PH7_VmEnforcePropStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)` |
|       1 | 1776 | `{` |
|      11 | 1777 | `	return VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pValue,0);` |
|       1 | 1778 | `}` |
|       - | 1779 | `/*` |
|       - | 1780 | ` * Exported reference-table probe for ReflectionReference::fromArrayElement` |
|       - | 1781 | ` * (vm_builtin_reflection.c). Returns the number of links (frame variables +` |
|       - | 1782 | ` * array entries) attached to the slot's reference record, 0 when the slot` |
|       - | 1783 | ` * has none — an array element is a PHP reference when this is >= 2.` |
|       - | 1784 | ` */` |
|      18 | 1785 | `PH7_PRIVATE int PH7_VmSlotRefCount(ph7_vm *pVm,sxu32 nIdx)` |
|       1 | 1786 | `{` |
|      19 | 1787 | `	return (int)(PH7_VmSlotEntryCount(&(*pVm),nIdx) + PH7_VmSlotNodeCount(&(*pVm),nIdx));` |
|       1 | 1788 | `}` |
|       - | 1789 | `/*` |
|       - | 1790 | `` * First-class callable over an arbitrary callable VALUE: `($expr)(...)`.`` |
|       - | 1791 | ` * Normalize a validated callable value into a fresh Closure, reusing VmCreateClosure (the same` |
|       - | 1792 | ` * object the method/static first-class-callable paths mint, so dispatch round-trips identically` |
|       - | 1793 | ` * via VmClosureUnwrap). Handles the three remaining callable shapes a value can hold:` |
|       - | 1794 | ` *   - a function-NAME string          -> plain closure ($__fn = name)` |
|       - | 1795 | ` *   - a [target, method] array callable -> bound (object target -> $this) / static (class-name` |
|       - | 1796 | ` *     target -> scope) closure, mirroring the [obj,m]/[class,m] decode in PH7_VmIsCallable` |
|       - | 1797 | ` *   - an __invoke object               -> closure bound to the object's __invoke` |
|       - | 1798 | ` * An existing Closure returns 0 here (it is already a Closure — the caller keeps it as-is), so this` |
|       - | 1799 | ` * stays idempotent even for a direct caller. Returns the fresh instance holding ONE reference,` |
|       - | 1800 | ` * which the caller's value TAKES (see VmCreateClosure) or 0 if the value is an existing Closure / not a normalizable callable / on OOM — in` |
|       - | 1801 | ` * which case the caller leaves the value untouched (graceful degradation). This is the generic` |
|       - | 1802 | ` * "callable value -> Closure" primitive: the body is FCC-agnostic and self-contained, so the future` |
|       - | 1803 | ` * Closure::bind/fromCallable work (Increment 2) can call it directly.` |
|       - | 1804 | ` */` |
|     262 | 1805 | `PH7_PRIVATE ph7_class_instance * VmFccWrapValue(ph7_vm *pVm, ph7_value *pValue)` |
|       4 | 1806 | `{` |
|       - | 1807 | `	/* A Closure is already callable; never double-wrap it (this also stops the __invoke branch` |
|       - | 1808 | `	 * below from binding a closure to its OWN __invoke). The OP_LOAD_FCC caller intercepts a` |
|       - | 1809 | `	 * Closure first too, but guarding here keeps the primitive safe for a direct Increment-2 caller. */` |
|     266 | 1810 | `	if( VmValueIsClosure(pVm, pValue) ){` |
|     ! 0 | 1811 | `		return 0;` |
|       - | 1812 | `	}` |
|     266 | 1813 | `	if( !PH7_VmIsCallable(pVm, pValue, TRUE) ){` |
|      55 | 1814 | `		return 0;` |
|       - | 1815 | `	}` |
|     212 | 1816 | `	if( pValue->iFlags & MEMOBJ_STRING ){` |
|       - | 1817 | `		SyString sName;` |
|     146 | 1818 | `		const char *zName = (const char *)SyBlobData(&pValue->sBlob);` |
|     146 | 1819 | `		sxu32 nName = SyBlobLength(&pValue->sBlob), nSep;` |
|       - | 1820 | ``		/* `"C::m"` is the SAME callable as `[C, 'm']`, and php mints the same`` |
|       - | 1821 | `		 * closure for it: scope C, name m. PHL kept the whole string as the` |
|       - | 1822 | `		 * function name, so the Closure ran (dispatch splits it) but described` |
|       - | 1823 | `		 * itself as nothing -- ReflectionFunction over it had no name, no scope` |
|       - | 1824 | `		 * and no parameters, and a library that reflects a callback before` |
|       - | 1825 | `		 * calling it (twig compiles every filter that way) died on the read. */` |
|    1252 | 1826 | `		for( nSep = 0 ; nSep + 1 < nName ; ++nSep ){` |
|    1128 | 1827 | `			if( zName[nSep] == ':' && zName[nSep+1] == ':' ){` |
|      19 | 1828 | `				break;` |
|       - | 1829 | `			}` |
|     557 | 1830 | `		}` |
|     146 | 1831 | `		if( nSep + 1 < nName ){` |
|       - | 1832 | `			ph7_class *pScopeCls;` |
|       - | 1833 | `			SyString sCls;` |
|      19 | 1834 | `			SyStringInitFromBuf(&sCls, zName, nSep);` |
|      19 | 1835 | `			pScopeCls = PH7_VmExtractClass(pVm, SyStringData(&sCls), SyStringLength(&sCls), FALSE, 0);` |
|      19 | 1836 | `			if( pScopeCls ){` |
|       - | 1837 | `				ph7_class_instance *pFccObj;` |
|      13 | 1838 | `				SyStringInitFromBuf(&sName, zName + nSep + 2, nName - (nSep + 2));` |
|      13 | 1839 | `				pFccObj = VmCreateClosure(pVm, &sName, 0, &pScopeCls->sName);` |
|      13 | 1840 | `				if( pFccObj ){` |
|      13 | 1841 | `					pFccObj->iFlags \|= VM_INSTANCE_FCC_METHOD; /* $__fn is a METHOD name */` |
|      19 | 1842 | `					if( PH7_VmFccMethodIsDirect(pVm,pScopeCls,` |
|       6 | 1843 | `							SyStringData(&sName),SyStringLength(&sName)) ){` |
|       7 | 1844 | `						pFccObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|       3 | 1845 | `					}` |
|       6 | 1846 | `				}` |
|      13 | 1847 | `				return pFccObj;` |
|       - | 1848 | `			}` |
|       3 | 1849 | `		}` |
|     134 | 1850 | `		SyStringInitFromBuf(&sName, zName, nName);` |
|     134 | 1851 | `		return VmCreateClosure(pVm, &sName, 0, 0);` |
|       - | 1852 | `	}` |
|      68 | 1853 | `	if( pValue->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 1854 | `		/* [target, method] — the same index-0/1 decode PH7_VmIsCallable uses to validate it` |
|       - | 1855 | `		 * (php reads the INTEGER indices, not insertion order). */` |
|      55 | 1856 | `		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;` |
|       - | 1857 | `		ph7_value *pTarget, *pMeth;` |
|       - | 1858 | `		SyString sName;` |
|      55 | 1859 | `		if( !PH7_VmArrayCallableParts(pVm, pMap, &pTarget, &pMeth) ){` |
|     ! 0 | 1860 | `			return 0;` |
|       - | 1861 | `		}` |
|      55 | 1862 | `		if( (pMeth->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pMeth->sBlob) == 0 ){` |
|     ! 0 | 1863 | `			return 0;` |
|       - | 1864 | `		}` |
|      55 | 1865 | `		SyStringInitFromBuf(&sName, SyBlobData(&pMeth->sBlob), SyBlobLength(&pMeth->sBlob));` |
|      55 | 1866 | `		if( pTarget->iFlags & MEMOBJ_OBJ ){` |
|      37 | 1867 | `			ph7_class_instance *pBoundThis = (ph7_class_instance *)pTarget->x.pOther;` |
|      55 | 1868 | `			ph7_class_instance *pFccObj = VmCreateClosure(pVm, &sName, pBoundThis,` |
|      36 | 1869 | `				&pBoundThis->pClass->sName);` |
|      37 | 1870 | `			if( pFccObj ){` |
|      37 | 1871 | `				pFccObj->iFlags \|= VM_INSTANCE_FCC_METHOD; /* $__fn is a METHOD name */` |
|      55 | 1872 | `				if( PH7_VmFccMethodIsDirect(pVm,pBoundThis->pClass,` |
|      18 | 1873 | `						SyStringData(&sName),SyStringLength(&sName)) ){` |
|       - | 1874 | `					/* The PH7_VmIsCallable gate above is php's creation-time screen; a pair it` |
|       - | 1875 | `					 * admitted because the class routes the name through __call is NOT settled. */` |
|      31 | 1876 | `					pFccObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|      15 | 1877 | `				}` |
|      18 | 1878 | `			}` |
|      37 | 1879 | `			return pFccObj;` |
|     ! 0 | 1880 | `		}else{` |
|       - | 1881 | `			/* [class-name, method] static callable -> bind the resolved scope. A runtime array` |
|       - | 1882 | `			 * callable carries a concrete class name (never self/static/parent), so a plain class` |
|       - | 1883 | ``			 * lookup is correct — unlike the syntactic `C::m(...)` path, which must resolve`` |
|       - | 1884 | `			 * self/static/parent via VmFccResolveScope. Matches PH7_VmIsCallable's own decode. */` |
|      19 | 1885 | `			ph7_class *pScopeCls = PH7_VmExtractClassFromValue(pVm, pTarget);` |
|      19 | 1886 | `			ph7_class_instance *pFccObj = pScopeCls` |
|      18 | 1887 | `				? VmCreateClosure(pVm, &sName, 0, &pScopeCls->sName) : 0;` |
|      19 | 1888 | `			if( pFccObj ){` |
|      19 | 1889 | `				pFccObj->iFlags \|= VM_INSTANCE_FCC_METHOD; /* $__fn is a METHOD name */` |
|      28 | 1890 | `				if( PH7_VmFccMethodIsDirect(pVm,pScopeCls,` |
|       9 | 1891 | `						SyStringData(&sName),SyStringLength(&sName)) ){` |
|      15 | 1892 | `					pFccObj->iFlags \|= VM_INSTANCE_FCC_SCREENED;` |
|       7 | 1893 | `				}` |
|       9 | 1894 | `			}` |
|      19 | 1895 | `			return pFccObj;` |
|       - | 1896 | `		}` |
|       - | 1897 | `	}` |
|      14 | 1898 | `	if( pValue->iFlags & MEMOBJ_OBJ ){` |
|       - | 1899 | `		/* __invoke object (a real Closure is intercepted by the caller before this point).` |
|       - | 1900 | ``		 * The `__invoke` name is the ENGINE's, so mark the closure: php dispatches a`` |
|       - | 1901 | ``		 * non-public __invoke through this wrapper exactly as it does through `$obj()`. */`` |
|      14 | 1902 | `		ph7_class_instance *pObj = (ph7_class_instance *)pValue->x.pOther;` |
|       - | 1903 | `		ph7_class_instance *pWrap;` |
|       - | 1904 | `		SyString sInvoke;` |
|      14 | 1905 | `		SyStringInitFromBuf(&sInvoke, "__invoke", sizeof("__invoke") - 1);` |
|      14 | 1906 | `		pWrap = VmCreateClosure(pVm, &sInvoke, pObj, &pObj->pClass->sName);` |
|      14 | 1907 | `		if( pWrap ){` |
|      14 | 1908 | `			pWrap->iFlags \|= VM_INSTANCE_FCC_INVOKE_OBJ;` |
|       6 | 1909 | `		}` |
|      14 | 1910 | `		return pWrap;` |
|       - | 1911 | `	}` |
|       - | 1912 | `	/* Unreachable in practice — the PH7_VmIsCallable gate admits only string/array/object, all` |
|       - | 1913 | `	 * handled above; kept to satisfy the non-void return path. */` |
|     ! 0 | 1914 | `	return 0;` |
|     135 | 1915 | `}` |
|       - | 1916 | `/*` |
|       - | 1917 | ` * Return a fresh Closure instance (iRef==0 from create/clone) as a builtin result, taking the` |
|       - | 1918 | ` * one reference into pCtx->pRet — mirrors the OP_LOAD_FCC store convention.` |
|       - | 1919 | ` */` |
|     168 | 1920 | `static int VmClosureResult(ph7_context *pCtx, ph7_class_instance *pClosure)` |
|       5 | 1921 | `{` |
|     173 | 1922 | `	if( pClosure == 0 ){` |
|     ! 0 | 1923 | `		ph7_result_null(pCtx);` |
|     ! 0 | 1924 | `		return PH7_OK;` |
|       - | 1925 | `	}` |
|     173 | 1926 | `	PH7_MemObjRelease(pCtx->pRet);` |
|       - | 1927 | `	/* Every caller hands over a FRESH instance, whose own reference is the one this` |
|       - | 1928 | `	 * return value takes (see OP_LOAD_CLOSURE). */` |
|     173 | 1929 | `	pCtx->pRet->x.pOther = pClosure;` |
|     173 | 1930 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|     173 | 1931 | `	return PH7_OK;` |
|      89 | 1932 | `}` |
|       - | 1933 | `/*` |
|       - | 1934 | ` * Overwrite a Closure clone's $__this (bound object, or 0 to unbind) and, when pScope != 0, its` |
|       - | 1935 | ` * $__scope (the class-name string; pScope->nByte==0 clears it). pScope==0 leaves $__scope as the` |
|       - | 1936 | ` * clone inherited it (PHP's "static" = keep-scope). Refreshes VM_INSTANCE_FCC_BOUND from the result.` |
|       - | 1937 | ` * The clone inherited a ref on the original $__this (PH7_CloneClassInstance copies via MemObjStore);` |
|       - | 1938 | ` * this drops that and takes one on pNewThis.` |
|       - | 1939 | ` */` |
|     112 | 1940 | `static void VmClosureRebind(ph7_class_instance *pClone,` |
|       - | 1941 | `	ph7_class_instance *pNewThis, const SyString *pScope)` |
|       5 | 1942 | `{` |
|       - | 1943 | `	SyString sAttr;` |
|       - | 1944 | `	ph7_value *pThisAttr, *pScopeAttr;` |
|     117 | 1945 | `	int bBound = 0;` |
|     117 | 1946 | `	SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     117 | 1947 | `	pThisAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|     117 | 1948 | `	if( pThisAttr ){` |
|       - | 1949 | `		/* PH7_MemObjRelease already drops the cloned-in object's refcount for an OBJ value —` |
|       - | 1950 | `		 * do NOT also decrement by hand (that double-frees the original bound object). */` |
|     117 | 1951 | `		PH7_MemObjRelease(pThisAttr);` |
|     117 | 1952 | `		if( pNewThis ){` |
|      99 | 1953 | `			pThisAttr->x.pOther = pNewThis;` |
|      99 | 1954 | `			MemObjSetType(pThisAttr, MEMOBJ_OBJ);` |
|      99 | 1955 | `			pNewThis->iRef++;` |
|      47 | 1956 | `		}` |
|      56 | 1957 | `	}` |
|     117 | 1958 | `	if( pScope ){` |
|      81 | 1959 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|      81 | 1960 | `		pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|      81 | 1961 | `		if( pScopeAttr ){` |
|      81 | 1962 | `			PH7_MemObjRelease(pScopeAttr);` |
|      81 | 1963 | `			if( pScope->nByte ){` |
|      81 | 1964 | `				PH7_MemObjStringAppend(pScopeAttr, pScope->zString, pScope->nByte);` |
|      38 | 1965 | `			}` |
|      38 | 1966 | `		}` |
|      38 | 1967 | `	}` |
|       - | 1968 | `	/* Refresh the FCC_BOUND fast-path flag from the resulting $__this/$__scope. pThisAttr already` |
|       - | 1969 | `	 * points at the final $__this slot; only $__scope needs a (re)fetch — the top half fetched it` |
|       - | 1970 | `	 * just for pScope != 0. */` |
|     117 | 1971 | `	SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|     117 | 1972 | `	pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);` |
|     112 | 1973 | `	if( (pThisAttr && (pThisAttr->iFlags & MEMOBJ_OBJ))` |
|      70 | 1974 | `		\|\| (pScopeAttr && (pScopeAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeAttr->sBlob) > 0) ){` |
|     109 | 1975 | `		bBound = 1;` |
|      52 | 1976 | `	}` |
|     117 | 1977 | `	if( bBound ){` |
|     109 | 1978 | `		pClone->iFlags \|= VM_INSTANCE_FCC_BOUND;` |
|      57 | 1979 | `	}else{` |
|      10 | 1980 | `		pClone->iFlags &= ~VM_INSTANCE_FCC_BOUND;` |
|       - | 1981 | `	}` |
|     117 | 1982 | `}` |
|       - | 1983 | `/*` |
|       - | 1984 | ` * Resolve the bindTo/bind/call $scope argument to a class-name SyString.` |
|       - | 1985 | ` * Returns 1 and fills *pOut if $__scope should be replaced (pOut->nByte==0 means "clear");` |
|       - | 1986 | ` * returns 0 to leave the scope unchanged (the PHP "static" sentinel / scope omitted).` |
|       - | 1987 | ` */` |
|     112 | 1988 | `static int VmClosureResolveScope(ph7_value *pScopeArg, SyString *pOut)` |
|       5 | 1989 | `{` |
|     117 | 1990 | `	if( pScopeArg == 0 ){` |
|      43 | 1991 | `		return 0; /* keep */` |
|       - | 1992 | `	}` |
|      72 | 1993 | `	if( (pScopeArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeArg->sBlob) == 6` |
|      41 | 1994 | `		&& SyMemcmp((const void *)SyBlobData(&pScopeArg->sBlob), (const void *)"static", 6) == 0 ){` |
|       6 | 1995 | `		return 0; /* "static" -> keep current scope */` |
|       - | 1996 | `	}` |
|      73 | 1997 | `	if( pScopeArg->iFlags & MEMOBJ_NULL ){` |
|       3 | 1998 | `		SyStringInitFromBuf(pOut, 0, 0); /* unscoped */` |
|       3 | 1999 | `		return 1;` |
|       - | 2000 | `	}` |
|      71 | 2001 | `	if( pScopeArg->iFlags & MEMOBJ_OBJ ){` |
|       6 | 2002 | `		ph7_class_instance *pScopeObj = (ph7_class_instance *)pScopeArg->x.pOther;` |
|       6 | 2003 | `		*pOut = pScopeObj->pClass->sName;` |
|       6 | 2004 | `		return 1;` |
|       - | 2005 | `	}` |
|      67 | 2006 | `	if( (pScopeArg->iFlags & MEMOBJ_STRING) == 0 && (pScopeArg->iFlags & MEMOBJ_SCALAR) ){` |
|       - | 2007 | ``		/* php declares `object\|string\|null $newScope` and coerces a scalar into it in weak`` |
|       - | 2008 | ``		 * mode, so `bindTo($o, 5)` reaches the lookup as the NAME "5" and warns that no such`` |
|       - | 2009 | `		 * class exists. Falling through as "keep the current scope" bound it silently. */` |
|       3 | 2010 | `		PH7_MemObjToString(pScopeArg);` |
|       1 | 2011 | `	}` |
|      67 | 2012 | `	if( pScopeArg->iFlags & MEMOBJ_STRING ){` |
|      67 | 2013 | `		SyStringInitFromBuf(pOut, (const char *)SyBlobData(&pScopeArg->sBlob), SyBlobLength(&pScopeArg->sBlob));` |
|      67 | 2014 | `		return 1;` |
|       - | 2015 | `	}` |
|     ! 0 | 2016 | `	return 0;` |
|      61 | 2017 | `}` |
|       - | 2018 | `/*` |
|       - | 2019 | ` * Fiber::getCurrent() — the fiber the running code is INSIDE, or null in the main` |
|       - | 2020 | ` * flow. php answers EG(active_fiber), which is the fiber whose body is on the` |
|       - | 2021 | ` * stack, not the innermost coroutine: a GENERATOR iterated from inside a fiber` |
|       - | 2022 | ` * leaves the fiber current, and code running after a fiber suspends back to its` |
|       - | 2023 | ` * caller is outside it again. pVm->pCurFiber is that name, saved and restored` |
|       - | 2024 | ` * around every start/resume, so the nesting is the call structure itself.` |
|       - | 2025 | ` *` |
|       - | 2026 | ` * Reached by every library that logs or schedules per-fiber: monolog asks for it` |
|       - | 2027 | ` * on EVERY record it writes.` |
|       - | 2028 | ` */` |
|      28 | 2029 | `PH7_PRIVATE int vm_builtin_Fiber_getCurrent(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 2030 | `{` |
|      29 | 2031 | `	ph7_vm *pVm = pCtx->pVm;` |
|      14 | 2032 | `	SXUNUSED(apArg);` |
|      14 | 2033 | `	SXUNUSED(nArg);` |
|      29 | 2034 | `	if( pVm->pCurFiber == 0 ){` |
|      13 | 2035 | `		ph7_result_null(pCtx);` |
|      13 | 2036 | `		return PH7_OK;` |
|       - | 2037 | `	}` |
|       - | 2038 | `	/* The result slot takes a reference of its own: pCurFiber is borrowed from the` |
|       - | 2039 | `	 * activation that set it, and the value handed back outlives that call. */` |
|      17 | 2040 | `	PH7_MemObjRelease(pCtx->pRet);` |
|      17 | 2041 | `	pVm->pCurFiber->iRef++;` |
|      17 | 2042 | `	pCtx->pRet->x.pOther = pVm->pCurFiber;` |
|      17 | 2043 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|      17 | 2044 | `	return PH7_OK;` |
|      15 | 2045 | `}` |
|       - | 2046 | `/*` |
|       - | 2047 | ``  * Fiber's C-bodied methods. Every one of them was a global `__fiber_verb($this,…)` `` |
|       - | 2048 | ` * thunk that a one-line prelude method forwarded to; the class body in the builtin` |
|       - | 2049 | ` * chunk now holds only its two private slots.` |
|       - | 2050 | ` */` |
|    7925 | 2051 | `PH7_PRIVATE sxi32 PH7_VmInstallFiberNative(ph7_vm *pVm)` |
|       5 | 2052 | `{` |
|       - | 2053 | `	/* php's own declaration ORDER, which is what get_class_methods() and` |
|       - | 2054 | `	 * ReflectionClass::getMethods() answer in. One row still differs from php's` |
|       - | 2055 | ``	 * list, and is recorded: `__destruct` is the engine's teardown hook`` |
|       - | 2056 | `	 * published as a method name php does not have (Generator's is the twin). */` |
|       - | 2057 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|       - | 2058 | `		{ "__construct",  PH7_MOD_PUBLIC, "callable $callback",  "",       vm_builtin_Fiber_construct },` |
|       - | 2059 | `		/* Variadic: the arguments now reach the C body directly instead of being` |
|       - | 2060 | `		 * repackaged by a func_get_args() call in the prelude. */` |
|       - | 2061 | `		{ "start",        PH7_MOD_PUBLIC, "mixed ...$args",      "mixed",  vm_builtin_Fiber_start },` |
|       - | 2062 | `		{ "resume",       PH7_MOD_PUBLIC, "mixed $value = null", "mixed",  vm_builtin_Fiber_resume },` |
|       - | 2063 | `		{ "throw",        PH7_MOD_PUBLIC, "Throwable $exception", "mixed", vm_builtin_Fiber_throw },` |
|       - | 2064 | `		{ "isStarted",    PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isStarted },` |
|       - | 2065 | `		{ "isSuspended",  PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isSuspended },` |
|       - | 2066 | `		{ "isRunning",    PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isRunning },` |
|       - | 2067 | `		{ "isTerminated", PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isTerminated },` |
|       - | 2068 | `		{ "getReturn",    PH7_MOD_PUBLIC, "",                    "mixed",  vm_builtin_Fiber_getReturn },` |
|       - | 2069 | `		/* Static like suspend, and the one method a program calls without holding a` |
|       - | 2070 | `		 * fiber at all — it is how code asks whether it is inside one. */` |
|       - | 2071 | `		{ "getCurrent",   PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "",                    "?Fiber",` |
|       - | 2072 | `		  vm_builtin_Fiber_getCurrent },` |
|       - | 2073 | `		/* Static, and the only one that never took a receiver even as a thunk:` |
|       - | 2074 | ``		 * `__fiber_suspend($value)` already read the value from argument #0. */`` |
|       - | 2075 | `		{ "suspend",      PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "mixed $value = null", "mixed",` |
|       - | 2076 | `		  vm_builtin_Fiber_suspend },` |
|       - | 2077 | `		{ "__destruct",   PH7_MOD_PUBLIC, "",                    "",       vm_builtin_Fiber_destruct },` |
|       - | 2078 | `	};` |
|       - | 2079 | `	/* The two private slots the methods above keep their state in: the execution` |
|       - | 2080 | `	 * context (a resource) and the callable handed to the constructor. */` |
|       - | 2081 | `	static const PH7_NativePropDef aProp[] = {` |
|       - | 2082 | `		{ "__ctx",      PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 2083 | `		{ "__callable", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 2084 | `	};` |
|       - | 2085 | `	static const PH7_NativeClassSpec sSpec = {` |
|       - | 2086 | `		"Fiber", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOSERIALIZE,` |
|       - | 2087 | `		aMethod, SX_ARRAYSIZE(aMethod),` |
|       - | 2088 | `		0, 0,` |
|       - | 2089 | `		aProp, SX_ARRAYSIZE(aProp),` |
|       - | 2090 | `		0, 0, 0` |
|       - | 2091 | `	};` |
|    7930 | 2092 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|       5 | 2093 | `}` |
|       - | 2094 | `/*` |
|       - | 2095 | `` * Generator's C-bodied methods, plus the `implements Iterator` the chunk can no`` |
|       - | 2096 | ` * longer carry: PH7_ClassImplement installs an abstract stub for any interface` |
|       - | 2097 | ` * method the class does not already declare, so it has to run AFTER the eight` |
|       - | 2098 | ` * methods below exist — at which point the stubs are skipped and the class is` |
|       - | 2099 | ` * concrete, exactly as the prelude declaration used to make it.` |
|       - | 2100 | ` */` |
|    7925 | 2101 | `PH7_PRIVATE sxi32 PH7_VmInstallGeneratorNative(ph7_vm *pVm)` |
|       5 | 2102 | `{` |
|       - | 2103 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|       - | 2104 | `		{ "current",    PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_current },` |
|       - | 2105 | `		{ "key",        PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_key },` |
|       - | 2106 | `		{ "next",       PH7_MOD_PUBLIC, "",                 "void",  vm_builtin_Generator_next },` |
|       - | 2107 | `		{ "rewind",     PH7_MOD_PUBLIC, "",                 "void",  vm_builtin_Generator_rewind },` |
|       - | 2108 | `		{ "valid",      PH7_MOD_PUBLIC, "",                 "bool",  vm_builtin_Generator_valid },` |
|       - | 2109 | ``		/* php REQUIRES the argument here; the prelude declared `$value = null`, so`` |
|       - | 2110 | ``		 * `$gen->send()` used to answer the first yielded value instead of raising. */`` |
|       - | 2111 | `		{ "send",       PH7_MOD_PUBLIC, "mixed $value",     "mixed", vm_builtin_Generator_send },` |
|       - | 2112 | `		{ "throw",      PH7_MOD_PUBLIC, "Throwable $exception", "mixed", vm_builtin_Generator_throw },` |
|       - | 2113 | `		{ "getReturn",  PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_getReturn },` |
|       - | 2114 | `		{ "__destruct", PH7_MOD_PUBLIC, "",                 "",      vm_builtin_Generator_destruct },` |
|       - | 2115 | `	};` |
|       - | 2116 | `	static const PH7_NativePropDef aProp[] = {` |
|       - | 2117 | `		{ "__ctx", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 2118 | `	};` |
|       - | 2119 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|       - | 2120 | `		{ "Generator", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOINSTANTIATE\|PH7_CLASS_NOSERIALIZE,` |
|       - | 2121 | `		  aMethod, SX_ARRAYSIZE(aMethod),` |
|       - | 2122 | `		  0, 0,` |
|       - | 2123 | `		  aProp, SX_ARRAYSIZE(aProp),` |
|       - | 2124 | `		  0, 0, 0 },` |
|       - | 2125 | `		/* php declares this one beside Generator and throws it from nowhere a` |
|       - | 2126 | `		 * script can reach: it is the exception a RESUME of a generator that has` |
|       - | 2127 | `		 * been closed would carry, and php's own paths answer null there. A` |
|       - | 2128 | `		 * program may still name it, catch it and throw it, so it is declared. */` |
|       - | 2129 | `		{ "ClosedGeneratorException", "Exception", 0, 0,` |
|       - | 2130 | `		  0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|       - | 2131 | `	};` |
|       - | 2132 | `	ph7_class *pClass;` |
|       - | 2133 | `	ph7_class *pIterator;` |
|    7930 | 2134 | `	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));` |
|    7930 | 2135 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 2136 | `		return rc;` |
|       - | 2137 | `	}` |
|       - | 2138 | ``	/* `implements Iterator` last, for the reason in this function's header. */`` |
|    7930 | 2139 | `	pClass = PH7_VmExtractClass(&(*pVm),"Generator",sizeof("Generator")-1,0,0);` |
|    7930 | 2140 | `	if( pClass ){` |
|       - | 2141 | ``		/* php refuses `new Generator` -- one only ever comes out of a call to a`` |
|       - | 2142 | `		 * function that yields -- and words the refusal per class. */` |
|    7930 | 2143 | `		pClass->zNewRefusal = "The \"Generator\" class is reserved for internal use "` |
|       - | 2144 | `			"and cannot be manually instantiated";` |
|    3957 | 2145 | `	}` |
|    7930 | 2146 | `	pIterator = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,0,0);` |
|    7930 | 2147 | `	if( pClass == 0 \|\| pIterator == 0 ){` |
|     ! 0 | 2148 | `		return SXERR_NOTFOUND;` |
|       - | 2149 | `	}` |
|    7930 | 2150 | `	return PH7_ClassImplement(pClass,pIterator);` |
|    3962 | 2151 | `}` |
|       - | 2152 | `/*` |
|       - | 2153 | ` * Closure::__construct() — php declares it PRIVATE and still words the refusal as` |
|       - | 2154 | ` * an instantiation error rather than a visibility one, so the body has to exist.` |
|       - | 2155 | ` */` |
|     ! 0 | 2156 | `PH7_PRIVATE int vm_builtin_Closure_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     ! 0 | 2157 | `{` |
|     ! 0 | 2158 | `	SXUNUSED(nArg);` |
|     ! 0 | 2159 | `	SXUNUSED(apArg);` |
|     ! 0 | 2160 | `	return PH7_VmThrowException(pCtx, "Error",` |
|       - | 2161 | `		"Instantiation of class Closure is not allowed");` |
|     ! 0 | 2162 | `}` |
|       - | 2163 | `/*` |
|       - | 2164 | ` * Clone a Closure for a rebind, carrying the marks that say WHAT it wraps. They live on the` |
|       - | 2165 | ` * instance rather than in an attribute, so PH7_CloneClassInstance — which copies attributes —` |
|       - | 2166 | `` * left them behind: the clone of a method callable forgot that its `$__fn` names a screened`` |
|       - | 2167 | `` * METHOD, and `$fcc->bindTo($other)` came back as a closure whose every dispatch re-resolved`` |
|       - | 2168 | ` * the name and re-decided its visibility (or, with no method of that name in reach, looked` |
|       - | 2169 | ` * for a global FUNCTION).` |
|       - | 2170 | ` */` |
|     112 | 2171 | `static ph7_class_instance * VmCloneClosureInstance(ph7_class_instance *pClosure)` |
|       5 | 2172 | `{` |
|     117 | 2173 | `	ph7_class_instance *pClone = PH7_CloneClassInstance(pClosure);` |
|     117 | 2174 | `	if( pClone ){` |
|     173 | 2175 | `		pClone->iFlags \|= pClosure->iFlags` |
|     112 | 2176 | `			& (VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SCREENED\|VM_INSTANCE_FCC_INVOKE_OBJ);` |
|      56 | 2177 | `	}` |
|     117 | 2178 | `	return pClone;` |
|       5 | 2179 | `}` |
|       - | 2180 | `/*` |
|       - | 2181 | `` * Is this closure STATIC — one that can never take a `$this`? For a plain closure that is the`` |
|       - | 2182 | `` * `static function(){}` declaration flag; for a METHOD callable it is the method's own`` |
|       - | 2183 | `` * staticness, which the flag cannot see because `$__fn` names a method and not a function in`` |
|       - | 2184 | `` * hFunction. `Base::stat(...)` is exactly as static as `static fn()` to php.`` |
|       - | 2185 | ` */` |
|     114 | 2186 | `static int VmClosureIsStatic(ph7_vm *pVm, ph7_class_instance *pClosure)` |
|       5 | 2187 | `{` |
|       - | 2188 | `	SyString sAttr;` |
|       - | 2189 | `	ph7_value *pFn;` |
|     119 | 2190 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|     119 | 2191 | `	pFn = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|     119 | 2192 | `	if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) == 0 ){` |
|     ! 0 | 2193 | `		return 0;` |
|       - | 2194 | `	}` |
|     119 | 2195 | `	if( pClosure->iFlags & VM_INSTANCE_FCC_METHOD ){` |
|      34 | 2196 | `		ph7_class *pScope = PH7_VmClosureScopeClass(pVm, pClosure);` |
|      34 | 2197 | `		ph7_class_method *pMeth = pScope` |
|      48 | 2198 | `			? PH7_ClassExtractMethod(pScope, (const char *)SyBlobData(&pFn->sBlob),` |
|      32 | 2199 | `				SyBlobLength(&pFn->sBlob)) : 0;` |
|      34 | 2200 | `		return (pMeth && (pMeth->iFlags & PH7_CLASS_ATTR_STATIC)) ? 1 : 0;` |
|       - | 2201 | `	}` |
|       - | 2202 | `	{` |
|     128 | 2203 | `		SyHashEntry *pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob),` |
|      41 | 2204 | `			SyBlobLength(&pFn->sBlob));` |
|      87 | 2205 | `		return (pEntry && (((ph7_vm_func *)pEntry->pUserData)->iFlags & VM_FUNC_STATIC_CL)) ? 1 : 0;` |
|       - | 2206 | `	}` |
|      62 | 2207 | `}` |
|       - | 2208 | `/*` |
|       - | 2209 | ` * php's four refusals for a rebind (zend_valid_closure_binding), in php's order. A closure` |
|       - | 2210 | `` * created from a METHOD is a "fake closure": it wraps a resolved function, so its `$this` may`` |
|       - | 2211 | ` * only move WITHIN the class that declared it and its scope may not move at all. PHL applied` |
|       - | 2212 | `` * none of them beyond the static-closure one, so `$fcc->bindTo($unrelated)` handed back a`` |
|       - | 2213 | `` * closure that could only fail later, and `->bindTo(null)` one with no receiver for a method`` |
|       - | 2214 | ` * that needs one. Each refusal is php's E_WARNING plus a NULL result; returns 0 when it fired.` |
|       - | 2215 | ` *` |
|       - | 2216 | `` * pScope is the resolved $scope ARGUMENT (0 when omitted or `"static"`, which both mean keep);`` |
|       - | 2217 | `` * an empty one is an explicit `null`, which php counts as a rebind like any other.`` |
|       - | 2218 | ` */` |
|     130 | 2219 | `static int VmClosureBindAllowed(ph7_vm *pVm, ph7_class_instance *pClosure,` |
|       - | 2220 | `	ph7_class_instance *pNewThis, const SyString *pScope)` |
|       5 | 2221 | `{` |
|     135 | 2222 | `	int bMethod = (pClosure->iFlags & VM_INSTANCE_FCC_METHOD) != 0;` |
|     135 | 2223 | `	ph7_class *pOwn = bMethod ? PH7_VmClosureScopeClass(pVm, pClosure) : 0;` |
|     135 | 2224 | `	if( pNewThis ){` |
|     113 | 2225 | `		if( VmClosureIsStatic(pVm, pClosure) ){` |
|       6 | 2226 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|       - | 2227 | `				"Cannot bind an instance to a static closure, this will be an error in PHP 9");` |
|       6 | 2228 | `			return 0;` |
|       - | 2229 | `		}` |
|     109 | 2230 | `		if( pOwn && !PH7_VmInstanceOf(pNewThis->pClass, pOwn) ){` |
|       - | 2231 | `			SyString sFn;` |
|       - | 2232 | `			ph7_value *pFn;` |
|       5 | 2233 | `			SyStringInitFromBuf(&sFn, "__fn", 4);` |
|       5 | 2234 | `			pFn = PH7_ClassInstanceFetchAttr(pClosure, &sFn);` |
|       5 | 2235 | `			SyStringInitFromBuf(&sFn, pFn ? (const char *)SyBlobData(&pFn->sBlob) : "",` |
|       - | 2236 | `				pFn ? SyBlobLength(&pFn->sBlob) : 0);` |
|       7 | 2237 | `			VmErrorFormat(pVm,PH7_CTX_WARNING,` |
|       - | 2238 | `				"Cannot bind method %z::%z() to object of class %z, this will be an error in PHP 9",` |
|       4 | 2239 | `				&pOwn->sDisp,&sFn,&pNewThis->pClass->sDisp);` |
|       5 | 2240 | `			return 0;` |
|       5 | 2241 | `		}` |
|      74 | 2242 | `	}else if( pOwn && !VmClosureIsStatic(pVm, pClosure) ){` |
|       5 | 2243 | `		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|       - | 2244 | `			"Cannot unbind $this of method, this will be an error in PHP 9");` |
|       5 | 2245 | `		return 0;` |
|       - | 2246 | `	}` |
|     123 | 2247 | `	if( pScope && bMethod ){` |
|      16 | 2248 | `		ph7_class *pWant = pScope->nByte` |
|       9 | 2249 | `			? PH7_VmResolveScopeName(pVm, pScope->zString, pScope->nByte) : 0;` |
|      11 | 2250 | `		if( pWant != pOwn ){` |
|       7 | 2251 | `			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,` |
|       - | 2252 | `				"Cannot rebind scope of closure created from method, this will be an error in PHP 9");` |
|       7 | 2253 | `			return 0;` |
|       - | 2254 | `		}` |
|       2 | 2255 | `	}` |
|     117 | 2256 | `	return 1;` |
|      70 | 2257 | `}` |
|       - | 2258 | `/*` |
|       - | 2259 | ` * Closure::call(object $newThis, mixed ...$args) — bind and invoke in one step.` |
|       - | 2260 | ` *` |
|       - | 2261 | `` * This was the last PHP left in the class: `$bound = $this->bindTo($newThis,`` |
|       - | 2262 | `` * get_class($newThis)); return $bound(...$args);`. That spelling leaked its own`` |
|       - | 2263 | `` * internals — a non-object argument reported `get_class(): Argument #1 ($object)`` |
|       - | 2264 | `` * must be of type object, string given` where php names THIS method's parameter,`` |
|       - | 2265 | `` * which is what declaring `object $newThis` buys (the shared screen words it).`` |
|       - | 2266 | ` * The scope php binds is the new $this's class, exactly as the PHP did.` |
|       - | 2267 | ` */` |
|      28 | 2268 | `PH7_PRIVATE int vm_builtin_Closure_call(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       3 | 2269 | `{` |
|      31 | 2270 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2271 | `	ph7_class_instance *pClosure, *pNewThis, *pClone;` |
|      31 | 2272 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       - | 2273 | `	ph7_value sBound;` |
|       - | 2274 | `	SyString sScope;` |
|       - | 2275 | `	sxi32 rc;` |
|      31 | 2276 | `	if( nArg < 1 ){` |
|     ! 0 | 2277 | `		return PH7_VmThrowException(pCtx, "ArgumentCountError",` |
|       - | 2278 | `			"Closure::call() expects at least 1 argument, 0 given");` |
|       - | 2279 | `	}` |
|      31 | 2280 | `	if( pRecv == 0 \|\| !VmValueIsClosure(pVm, pRecv) ){` |
|     ! 0 | 2281 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2282 | `		return PH7_OK;` |
|       - | 2283 | `	}` |
|      31 | 2284 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 \|\| apArg[0]->x.pOther == 0 ){` |
|       - | 2285 | ``		/* Unreachable while the declared `object $newThis` is screened; kept because`` |
|       - | 2286 | `		 * rule 44's family says a screen written for one body shape has not` |
|       - | 2287 | `		 * necessarily run for this one. */` |
|     ! 0 | 2288 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 2289 | `			"Closure::call(): Argument #1 ($newThis) must be of type object");` |
|       - | 2290 | `	}` |
|      31 | 2291 | `	pClosure = (ph7_class_instance *)pRecv->x.pOther;` |
|      31 | 2292 | `	pNewThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      31 | 2293 | `	SyStringInitFromBuf(&sScope, pNewThis->pClass->sName.zString, pNewThis->pClass->sName.nByte);` |
|       - | 2294 | `	/* call() BINDS before it invokes, so php's rebind refusals apply to it — and because the` |
|       - | 2295 | `	 * scope it asks for is the new $this's class, a method callable handed an instance of` |
|       - | 2296 | `	 * anything but its own declaring class is refused for the scope, not the receiver. */` |
|      31 | 2297 | `	if( !VmClosureBindAllowed(pVm, pClosure, pNewThis, &sScope) ){` |
|       3 | 2298 | `		ph7_result_null(pCtx);` |
|       3 | 2299 | `		return PH7_OK;` |
|       - | 2300 | `	}` |
|      29 | 2301 | `	pClone = VmCloneClosureInstance(pClosure);` |
|      29 | 2302 | `	if( pClone == 0 ){` |
|     ! 0 | 2303 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2304 | `		return PH7_OK;` |
|       - | 2305 | `	}` |
|      29 | 2306 | `	VmClosureRebind(pClone, pNewThis, &sScope);` |
|       - | 2307 | `	/* The bound closure is handed to the dispatcher through a STACK carrier that` |
|       - | 2308 | `	 * takes its own reference (rule 16): a context value would be released with the` |
|       - | 2309 | `	 * call context and unref the instance a second time. */` |
|      29 | 2310 | `	PH7_MemObjInit(pVm, &sBound);` |
|      29 | 2311 | `	sBound.x.pOther = pClone;` |
|      29 | 2312 | `	MemObjSetType(&sBound, MEMOBJ_OBJ);` |
|       - | 2313 | `	/* The clone's own reference is this carrier's, and releasing the carrier below` |
|       - | 2314 | `	 * is what ends the temporary (see OP_LOAD_CLOSURE). */` |
|      29 | 2315 | `	rc = PH7_VmCallUserFunction(pVm, &sBound, nArg - 1, apArg + 1, pCtx->pRet);` |
|      29 | 2316 | `	PH7_MemObjRelease(&sBound);` |
|      29 | 2317 | `	return rc;` |
|      17 | 2318 | `}` |
|       - | 2319 | `/*` |
|       - | 2320 | ` * Closure — declared entirely from C.` |
|       - | 2321 | ` *` |
|       - | 2322 | ` * Its three methods were the first in the engine whose body is a C routine rather` |
|       - | 2323 | `` * than bytecode (VM_FUNC_NATIVE), retiring the global `__closure_bindTo` /`` |
|       - | 2324 | `` * `__closure_fromCallable` thunks a prelude method used to forward to. The`` |
|       - | 2325 | ` * DECLARATION stayed in the builtin chunk until now, which cost three things: the` |
|       - | 2326 | `` * engine slots `$__fn`/`$__this`/`$__scope` were on every presentation surface`` |
|       - | 2327 | `` * (php's Closure has NO properties), `__construct` was public where php's is`` |
|       - | 2328 | `` * private, and `call()` was PHP that leaked `get_class()`'s diagnostic.`` |
|       - | 2329 | ` */` |
|    7925 | 2330 | `PH7_PRIVATE sxi32 PH7_VmInstallClosureNative(ph7_vm *pVm)` |
|       5 | 2331 | `{` |
|       - | 2332 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|       - | 2333 | `		/* Parameter names are php's own ($newScope, not $scope): this string is the` |
|       - | 2334 | `		 * declaration of record for arity, by-ref positions and the reported` |
|       - | 2335 | `		 * parameter list. */` |
|       - | 2336 | `		{ "__construct",  PH7_MOD_PRIVATE, "", 0,` |
|       - | 2337 | `		  vm_builtin_Closure_construct },` |
|       - | 2338 | `		/* php's own registration order, which is what get_class_methods() and` |
|       - | 2339 | ``		 * ReflectionClass::getMethods() answer in: `bind` before `bindTo`. */`` |
|       - | 2340 | `		{ "bind",         PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|       - | 2341 | `		  "Closure $closure, ?object $newThis, object\|string\|null $newScope = \"static\"", "?Closure",` |
|       - | 2342 | `		  vm_builtin_Closure_bindTo },` |
|       - | 2343 | `		{ "bindTo",       PH7_MOD_PUBLIC,` |
|       - | 2344 | `		  "?object $newThis, object\|string\|null $newScope = \"static\"", "?Closure",` |
|       - | 2345 | `		  vm_builtin_Closure_bindTo },` |
|       - | 2346 | `		{ "call",         PH7_MOD_PUBLIC,` |
|       - | 2347 | `		  "object $newThis, mixed ...$args", "mixed",` |
|       - | 2348 | `		  vm_builtin_Closure_call },` |
|       - | 2349 | `		{ "fromCallable", PH7_MOD_PUBLIC\|PH7_MOD_STATIC,` |
|       - | 2350 | `		  "callable $callback", "Closure",` |
|       - | 2351 | `		  vm_builtin_Closure_fromCallable },` |
|       - | 2352 | ``		/* A closure IS its own `__invoke`, and php says so: `$c->__invoke($x)` calls`` |
|       - | 2353 | ``		 * it, `method_exists($c,'__invoke')` is true, and reflecting it hands back`` |
|       - | 2354 | `		 * THE CLOSURE's parameter list. php does not keep it in the class's function` |
|       - | 2355 | `		 * table -- it fabricates one per closure -- which is why it is absent from` |
|       - | 2356 | `		 * get_class_methods() and from the class export, and why` |
|       - | 2357 | ``		 * `new ReflectionMethod('Closure','__invoke')` refuses while`` |
|       - | 2358 | ``		 * `(new ReflectionObject($c))->getMethod('__invoke')` answers. That whole`` |
|       - | 2359 | `		 * shape is PH7_MOD_FABRICATED. Declared with NO signature: the reflector` |
|       - | 2360 | `		 * built from a class NAME has nothing to describe (php reports zero` |
|       - | 2361 | `		 * parameters and no return type for it), and the one built from an OBJECT` |
|       - | 2362 | `		 * describes the closure instead. */` |
|       - | 2363 | `		{ "__invoke",     PH7_MOD_PUBLIC\|PH7_MOD_FABRICATED, 0, 0,` |
|       - | 2364 | `		  vm_builtin_Closure_invoke },` |
|       - | 2365 | `	};` |
|       - | 2366 | `	/* The engine's own slots: the callable NAME, the bound receiver and the bound` |
|       - | 2367 | `	 * scope. php presents no property at all for a Closure, so all three carry` |
|       - | 2368 | ``	 * PH7_MOD_HIDDEN — they keep working for `new`, `clone` and the C bodies (and`` |
|       - | 2369 | `	 * for serialize(), which this class refuses anyway) and disappear from` |
|       - | 2370 | `	 * var_dump/print_r/(array)/get_object_vars/foreach/json_encode and Reflection. */` |
|       - | 2371 | `	static const PH7_NativePropDef aProp[] = {` |
|       - | 2372 | `		{ "__fn",    PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 2373 | `		{ "__this",  PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 2374 | `		{ "__scope", PH7_MOD_PRIVATE\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|       - | 2375 | `	};` |
|       - | 2376 | `	/* php's get_debug_info for a Closure shows a SHAPE none of those three slots` |
|       - | 2377 | `	 * is (name/file/line or function, static, this, parameter) — see` |
|       - | 2378 | `	 * PH7_ClosurePresent, which lives beside the reflection machinery that already` |
|       - | 2379 | `	 * describes any callable's parameters. */` |
|       - | 2380 | `	static const PH7_NativeClassSpec sSpec = {` |
|       - | 2381 | `		"Closure", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOSERIALIZE\|PH7_CLASS_NOINSTANTIATE,` |
|       - | 2382 | `		aMethod, SX_ARRAYSIZE(aMethod),` |
|       - | 2383 | `		0, 0,` |
|       - | 2384 | `		aProp, SX_ARRAYSIZE(aProp),` |
|       - | 2385 | `		VmClosureRelease, 0, PH7_ClosurePresent` |
|       - | 2386 | `	};` |
|    7930 | 2387 | `	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);` |
|       5 | 2388 | `}` |
|       - | 2389 | `/*` |
|       - | 2390 | ` * Closure::bindTo($newThis, $scope='static') / Closure::bind($closure, $newThis, $scope='static').` |
|       - | 2391 | ` * Clone the receiver and rebind $this/$scope; returns the new Closure (NULL on a non-Closure` |
|       - | 2392 | ` * receiver, matching PHP's failure mode).` |
|       - | 2393 | ` */` |
|     112 | 2394 | `PH7_PRIVATE int vm_builtin_Closure_bindTo(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2395 | `{` |
|     117 | 2396 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2397 | `	ph7_class_instance *pClosure, *pNewThis, *pClone;` |
|       - | 2398 | `	ph7_value *pNewThisArg;` |
|       - | 2399 | `	ph7_value *pRecv;` |
|       - | 2400 | `	SyString sScope;` |
|     117 | 2401 | `	const SyString *pScopePtr = 0;` |
|       - | 2402 | `	/* One body, both spellings — as it always was, except the closure now arrives` |
|       - | 2403 | `	 * the way php passes it rather than as a hand-written first argument. Called as` |
|       - | 2404 | `	 * the instance method bindTo(), the receiver IS the closure and the arguments` |
|       - | 2405 | `	 * start at $newThis; called as the static bind(), the closure is argument #1.` |
|       - | 2406 | `	 * Normalizing here is what lets the two share an implementation. */` |
|     117 | 2407 | `	if( PH7_ContextThis(pCtx) ){` |
|      82 | 2408 | `		pRecv = PH7_ContextThisValue(pCtx);` |
|      43 | 2409 | `	}else{` |
|      37 | 2410 | `		if( nArg < 1 ){` |
|     ! 0 | 2411 | `			ph7_result_null(pCtx);` |
|     ! 0 | 2412 | `			return PH7_OK;` |
|       - | 2413 | `		}` |
|      37 | 2414 | `		pRecv = apArg[0];` |
|      37 | 2415 | `		apArg++;` |
|      37 | 2416 | `		nArg--;` |
|       - | 2417 | `	}` |
|     117 | 2418 | `	if( nArg < 1 \|\| !VmValueIsClosure(pVm, pRecv) ){` |
|     ! 0 | 2419 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2420 | `		return PH7_OK;` |
|       - | 2421 | `	}` |
|     117 | 2422 | `	pClosure = (ph7_class_instance *)pRecv->x.pOther;` |
|     117 | 2423 | `	pNewThisArg = apArg[0];` |
|     117 | 2424 | `	if( pNewThisArg->iFlags & MEMOBJ_NULL ){` |
|      27 | 2425 | `		pNewThis = 0;` |
|     105 | 2426 | `	}else if( pNewThisArg->iFlags & MEMOBJ_OBJ ){` |
|      93 | 2427 | `		pNewThis = (ph7_class_instance *)pNewThisArg->x.pOther;` |
|      49 | 2428 | `	}else{` |
|     ! 0 | 2429 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 2430 | `			"Closure::bindTo(): Argument #1 ($newThis) must be of type ?object");` |
|       - | 2431 | `	}` |
|     117 | 2432 | `	if( VmClosureResolveScope((nArg > 1) ? apArg[1] : 0, &sScope) ){` |
|      73 | 2433 | `		pScopePtr = &sScope;` |
|      34 | 2434 | `	}` |
|       - | 2435 | `	/* php RESOLVES the scope argument before it decides anything else, and a name no class` |
|       - | 2436 | `	 * answers to is its own warning — ahead of all four rebind refusals, for a plain closure` |
|       - | 2437 | `	 * as much as for a method one. PHL bound the unresolvable scope in silence and then had` |
|       - | 2438 | ``	 * no scope at all, so `bindTo($o, 'Typo')` produced a closure that could not reach the`` |
|       - | 2439 | `	 * private members it was being bound for. */` |
|     112 | 2440 | `	if( pScopePtr && pScopePtr->nByte` |
|      72 | 2441 | `	 && PH7_VmResolveScopeName(pVm, pScopePtr->zString, pScopePtr->nByte) == 0 ){` |
|      11 | 2442 | `		VmErrorFormat(pVm,PH7_CTX_WARNING,"Class \"%z\" not found",pScopePtr);` |
|      11 | 2443 | `		ph7_result_null(pCtx);` |
|      11 | 2444 | `		return PH7_OK;` |
|       - | 2445 | `	}` |
|     107 | 2446 | `	if( !VmClosureBindAllowed(pVm, pClosure, pNewThis, pScopePtr) ){` |
|      18 | 2447 | `		ph7_result_null(pCtx);` |
|      18 | 2448 | `		return PH7_OK;` |
|       - | 2449 | `	}` |
|      91 | 2450 | `	pClone = VmCloneClosureInstance(pClosure);` |
|      91 | 2451 | `	if( pClone == 0 ){` |
|     ! 0 | 2452 | `		ph7_result_null(pCtx);` |
|     ! 0 | 2453 | `		return PH7_OK;` |
|       - | 2454 | `	}` |
|      91 | 2455 | `	VmClosureRebind(pClone, pNewThis, pScopePtr);` |
|      91 | 2456 | `	return VmClosureResult(pCtx, pClone);` |
|      61 | 2457 | `}` |
|       - | 2458 | `/*` |
|       - | 2459 | ` * Closure::__invoke(...$args) — call the closure the receiver IS.` |
|       - | 2460 | ` *` |
|       - | 2461 | ` * php reaches the closure's own body through this name; here the receiver is a` |
|       - | 2462 | ` * Closure OBJECT and every call door already knows how to invoke one, so the whole` |
|       - | 2463 | ` * body is "call myself with what I was given". Argument binding, by-reference` |
|       - | 2464 | ` * parameters and the declared-type screens are the callee's own, exactly as they` |
|       - | 2465 | `` * are for `$c(...)`.`` |
|       - | 2466 | ` */` |
|       4 | 2467 | `PH7_PRIVATE int vm_builtin_Closure_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 2468 | `{` |
|       5 | 2469 | `	ph7_vm *pVm = pCtx->pVm;` |
|       5 | 2470 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       5 | 2471 | `	if( pRecv == 0 \|\| (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2472 | `		return PH7_OK;` |
|       - | 2473 | `	}` |
|       5 | 2474 | `	return PH7_VmCallUserFunction(pVm, pRecv, nArg, apArg, pCtx->pRet);` |
|       3 | 2475 | `}` |
|       - | 2476 | `/*` |
|       - | 2477 | ` * Closure::fromCallable($callable) — normalize any callable value to a Closure (reuses the` |
|       - | 2478 | ` * VmFccWrapValue primitive); idempotent on a Closure; TypeError on a non-callable.` |
|       - | 2479 | ` */` |
|     108 | 2480 | `PH7_PRIVATE int vm_builtin_Closure_fromCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       3 | 2481 | `{` |
|     111 | 2482 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2483 | `	ph7_class_instance *pClosure;` |
|     111 | 2484 | `	if( nArg < 1 ){` |
|     ! 0 | 2485 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 2486 | `			"Closure::fromCallable() expects exactly 1 argument, 0 given");` |
|       - | 2487 | `	}` |
|     111 | 2488 | `	if( VmValueIsClosure(pVm, apArg[0]) ){` |
|       3 | 2489 | `		ph7_result_value(pCtx, apArg[0]); /* already a Closure: idempotent */` |
|       3 | 2490 | `		return PH7_OK;` |
|       - | 2491 | `	}` |
|     109 | 2492 | `	pClosure = VmFccWrapValue(pVm, apArg[0]);` |
|     109 | 2493 | `	if( pClosure == 0 ){` |
|       - | 2494 | `		/* php says WHY, with the same reason taxonomy every callback argument uses —` |
|       - | 2495 | ``		 * `Failed to create closure from callable: class P does not have a method "zz"`.`` |
|       - | 2496 | `		 * PHL answered one flat "is not a valid callback" for all eight causes, so a typo` |
|       - | 2497 | `		 * in a method name, a private one, a missing class and a bad array shape were` |
|       - | 2498 | `		 * indistinguishable. PH7_VmCallableReason is the shared builder (its tails are` |
|       - | 2499 | `		 * already byte-exact for call_user_func & friends); the fallback covers the OOM` |
|       - | 2500 | `		 * path, where the value IS callable and the reason is 0. */` |
|       - | 2501 | `		char zWhy[192];` |
|      25 | 2502 | `		const char *zReason = PH7_VmCallableReason(pVm, apArg[0], zWhy, sizeof(zWhy));` |
|      25 | 2503 | `		if( zReason ){` |
|      37 | 2504 | `			return PH7_VmThrowException(pCtx, "TypeError",` |
|      12 | 2505 | `				"Failed to create closure from callable: %s", zReason);` |
|       - | 2506 | `		}` |
|     ! 0 | 2507 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|       - | 2508 | `			"Failed to create closure from callable");` |
|       - | 2509 | `	}` |
|      85 | 2510 | `	return VmClosureResult(pCtx, pClosure);` |
|      57 | 2511 | `}` |
|       - | 2512 | `/*` |
|       - | 2513 | ` * Fiber::suspend($value = null) — static method.` |
|       - | 2514 | ` * Suspends the currently running fiber and passes $value to the caller.` |
|       - | 2515 | ` */` |
|     418 | 2516 | `PH7_PRIVATE int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2517 | `{` |
|     423 | 2518 | `	ph7_vm *pVm = pCtx->pVm;` |
|     423 | 2519 | `	if( pVm->pActiveCtx == 0 ){` |
|     ! 0 | 2520 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2521 | `			"Cannot suspend outside of a fiber");` |
|       - | 2522 | `	}` |
|       - | 2523 | `#ifdef PH7_CORO_STACK` |
|     423 | 2524 | `	if( pVm->pActiveCtx->pCoro ){` |
|       - | 2525 | `		/* The fiber has a stack of its own, so this is not a return code that has` |
|       - | 2526 | `		 * to be threaded back out through every activation between here and the` |
|       - | 2527 | `		 * body -- it is one switch, and everything above it (this builtin's own C` |
|       - | 2528 | `		 * frame, the array_map/usort loop that called into PHP, the eval or catch` |
|       - | 2529 | `		 * body the call sits in) stays standing where it is. */` |
|     423 | 2530 | `		ph7_exec_ctx *pFiber = pVm->pActiveCtx;` |
|       - | 2531 | `		sxi32 rc;` |
|     423 | 2532 | `		if( nArg > 0 ){` |
|     415 | 2533 | `			PH7_MemObjStore(apArg[0], &pFiber->sSuspendValue);` |
|     210 | 2534 | `		}else{` |
|      12 | 2535 | `			PH7_MemObjRelease(&pFiber->sSuspendValue);` |
|       - | 2536 | `		}` |
|     423 | 2537 | `		rc = VmCoroSuspend(pCtx, pFiber);` |
|     423 | 2538 | `		if( rc != PH7_OK ){` |
|     232 | 2539 | `			return rc;  /* resumed by Fiber::throw(), or unwound by the teardown */` |
|       - | 2540 | `		}` |
|       - | 2541 | `		/* php: Fiber::suspend() ANSWERS what resume() was given. */` |
|     195 | 2542 | `		ph7_result_value(pCtx, &pFiber->sSuspendValue);` |
|     195 | 2543 | `		return PH7_OK;` |
|       - | 2544 | `	}` |
|       - | 2545 | `#endif` |
|       - | 2546 | `	/* Stage 4 scoped divergence: the trampoline only makes PHP->PHP CALLs` |
|       - | 2547 | `	 * iterative. Every OTHER re-entry runs on a fresh native VmByteCodeExec` |
|       - | 2548 | `	 * activation (nVmExecDepth bumped) that PH7_SUSPEND cannot unwind across` |
|       - | 2549 | `	 * without real coroutine stacks: a C->PHP callback` |
|       - | 2550 | `	 * (usort/array_map/preg_replace_callback comparator), and — because fibers` |
|       - | 2551 | `	 * use the LEGACY (non-inline) try/catch machinery — a suspend inside a` |
|       - | 2552 | `	 * fiber's catch/finally body, a match/switch arm, or eval()/include()'d` |
|       - | 2553 | `	 * code, all of which run via VmLocalExec. php does all of these via full` |
|       - | 2554 | `	 * native-stack switching; PHL raises a catchable FiberError instead of the` |
|       - | 2555 | `	 * old silent corruption. A suspend in a fiber's try BODY (not catch/finally)` |
|       - | 2556 | `	 * runs in the main dispatch loop and parks normally. A recorded` |
|       - | 2557 | `	 * residual; making the catch/finally case work needs fibers on the inline` |
|       - | 2558 | `	 * try machinery (the generator ROOT C path), a follow-up. */` |
|     ! 0 | 2559 | `	if( pVm->nVmExecDepth != pVm->pActiveCtx->nBodyExecDepth ){` |
|     ! 0 | 2560 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2561 | `			"Cannot suspend across an internal call boundary");` |
|       - | 2562 | `	}` |
|     ! 0 | 2563 | `	if( nArg > 0 ){` |
|     ! 0 | 2564 | `		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);` |
|     ! 0 | 2565 | `	}else{` |
|     ! 0 | 2566 | `		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);` |
|       - | 2567 | `	}` |
|     ! 0 | 2568 | `	return PH7_SUSPEND;` |
|     214 | 2569 | `}` |
|       - | 2570 | `/*` |
|       - | 2571 | ` * __fiber_construct($this, $callable) — validate and store the callable.` |
|       - | 2572 | ` * Actual resolution is deferred to start() so that overload selection` |
|       - | 2573 | ` * and closure-environment binding happen with the correct argument context.` |
|       - | 2574 | ` */` |
|     372 | 2575 | `PH7_PRIVATE int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 2576 | `{` |
|       - | 2577 | `	ph7_class_instance *pThis;` |
|       - | 2578 | `	ph7_value *pAttr;` |
|       - | 2579 | `	SyString sAttrName;` |
|     377 | 2580 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     377 | 2581 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|     377 | 2582 | `	if( nArg < 1 ){` |
|     ! 0 | 2583 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2584 | `			"Fiber::__construct() expects a callable argument");` |
|       - | 2585 | `	}` |
|     377 | 2586 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 2587 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2588 | `			"Fiber::__construct(): invalid $this");` |
|       - | 2589 | `	}` |
|     377 | 2590 | `	pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|     377 | 2591 | `	if( pThis->pClass != pCtx->pVm->pFiberClass ){` |
|     ! 0 | 2592 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2593 | `			"Fiber::__construct(): $this is not a Fiber instance");` |
|       - | 2594 | `	}` |
|       - | 2595 | ``	/* php validates `callable $callback` HERE, with the ordinary callback-argument`` |
|       - | 2596 | ``	 * screen and its whole reason taxonomy -- `new Fiber('nosuch')` is a TypeError at`` |
|       - | 2597 | `	 * CONSTRUCTION, naming the function it could not find. PHL had a hand-rolled shape` |
|       - | 2598 | `	 * check that only asked "string or object", with a FiberError of its own wording,` |
|       - | 2599 | `	 * and left an unresolvable NAME to fail at start() instead: the fiber constructed` |
|       - | 2600 | `	 * fine and the program learned about its typo one call later. */` |
|       - | 2601 | `	{` |
|     377 | 2602 | `		sxi32 rcCb = PH7_CheckCallbackArg(pCtx, apArg[0], 1, "callback", FALSE);` |
|     377 | 2603 | `		if( rcCb != PH7_OK ){` |
|      13 | 2604 | `			return rcCb;` |
|       - | 2605 | `		}` |
|       - | 2606 | `	}` |
|       - | 2607 | `	/* Store callable in $this->__callable for deferred resolution at start() */` |
|     365 | 2608 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     365 | 2609 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     365 | 2610 | `	if( pAttr ){` |
|     365 | 2611 | `		PH7_MemObjStore(apArg[0], pAttr);` |
|     180 | 2612 | `	}` |
|     365 | 2613 | `	return PH7_OK;` |
|     191 | 2614 | `}` |
|       - | 2615 | `/*` |
|       - | 2616 | ` * Resolve a fiber's stored callable to the BODY it runs and the receiver that body` |
|       - | 2617 | `` * needs -- for every shape php's `callable` covers, not just the two PHL used to take.`` |
|       - | 2618 | ` *` |
|       - | 2619 | ` * The constructor screens the argument with php's own callback rules now` |
|       - | 2620 | ` * (PH7_CheckCallbackArg), so what arrives here is a callable; this decides which body` |
|       - | 2621 | `` * it names. `[$obj,'m']`, `['Class','stat']` and `"Class::stat"` are the everyday way`` |
|       - | 2622 | ` * to run an object's method as a coroutine, and all three were refused outright --` |
|       - | 2623 | ` * the first two by the constructor's "string or closure" shape check, the third by a` |
|       - | 2624 | ` * plain-function lookup that could never find a method.` |
|       - | 2625 | ` *` |
|       - | 2626 | ` * Answers 0 for a callable this engine has no BYTECODE body for: a host builtin` |
|       - | 2627 | `` * (`new Fiber('strtoupper')`) and a name php routes through __call/__callStatic --`` |
|       - | 2628 | ` * which is where the visibility rule lives, and why this asks for it. php does not` |
|       - | 2629 | ` * reach a private method through a callable, it reaches __call INSTEAD, so running` |
|       - | 2630 | ` * the private body would be a hole rather than a shortcut. Both shapes run on php,` |
|       - | 2631 | ` * whose fiber switches a real stack; here they are a loud refusal (the scope policy divergence,` |
|       - | 2632 | ` * twin-paired). *pzWhy names the reason for the caller to report.` |
|       - | 2633 | ` */` |
|     348 | 2634 | `static ph7_vm_func * VmFiberCallableBody(ph7_vm *pVm, ph7_value *pCallable,` |
|       - | 2635 | `	ph7_class_instance **ppThis, const char **pzWhy)` |
|       5 | 2636 | `{` |
|     353 | 2637 | `	ph7_class_method *pMethod = 0;` |
|     353 | 2638 | `	ph7_class *pClass = 0;` |
|     353 | 2639 | `	*ppThis = 0;` |
|     353 | 2640 | `	*pzWhy = 0;` |
|     353 | 2641 | `	if( pCallable->iFlags & MEMOBJ_HASHMAP ){` |
|       - | 2642 | ``		/* php's `[target, method]` pair, decoded by the one shared reader so a fiber`` |
|       - | 2643 | `		 * agrees with is_callable() and with every dispatch site about what it is. */` |
|      16 | 2644 | `		ph7_value *pTarget = 0, *pName = 0;` |
|      14 | 2645 | `		if( !PH7_VmArrayCallableParts(pVm, (ph7_hashmap *)pCallable->x.pOther, &pTarget, &pName)` |
|      16 | 2646 | `		 \|\| (pName->iFlags & MEMOBJ_STRING) == 0 ){` |
|     ! 0 | 2647 | `			*pzWhy = "callable is not a valid [target, method] pair";` |
|     ! 0 | 2648 | `			return 0;` |
|       - | 2649 | `		}` |
|      16 | 2650 | `		pClass = PH7_VmExtractClassFromValue(pVm, pTarget);` |
|      16 | 2651 | `		if( pClass ){` |
|      23 | 2652 | `			pMethod = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pName->sBlob),` |
|      14 | 2653 | `				SyBlobLength(&pName->sBlob));` |
|       7 | 2654 | `		}` |
|      16 | 2655 | `		if( pMethod == 0 \|\| !PH7_VmCallableMethodAccessible(pVm, pClass, pMethod) ){` |
|       5 | 2656 | `			*pzWhy = "callable routes through __call(), which cannot be a fiber body here";` |
|       5 | 2657 | `			return 0;` |
|       - | 2658 | `		}` |
|      11 | 2659 | `		if( (pTarget->iFlags & MEMOBJ_OBJ) && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|       5 | 2660 | `			*ppThis = (ph7_class_instance *)pTarget->x.pOther;` |
|       2 | 2661 | `		}` |
|      11 | 2662 | `		return &pMethod->sFunc;` |
|       - | 2663 | `	}` |
|     339 | 2664 | `	if( pCallable->iFlags & MEMOBJ_STRING ){` |
|       - | 2665 | `		const char *zCls, *zMeth;` |
|       - | 2666 | `		sxu32 nCls, nMeth;` |
|       - | 2667 | `		SyString sName;` |
|       - | 2668 | `		SyHashEntry *pEntry;` |
|     339 | 2669 | `		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));` |
|       - | 2670 | ``		/* php's `"Class::method"` static-callable string is the same callee as the pair. */`` |
|     339 | 2671 | `		if( PH7_VmCallableStringParts(sName.zString, sName.nByte, &zCls, &nCls, &zMeth, &nMeth) ){` |
|       8 | 2672 | `			pClass = PH7_VmResolveScopeName(pVm, zCls, nCls);` |
|       8 | 2673 | `			pMethod = pClass ? PH7_ClassExtractMethod(pClass, zMeth, nMeth) : 0;` |
|       8 | 2674 | `			if( pMethod == 0 \|\| !PH7_VmCallableMethodAccessible(pVm, pClass, pMethod) ){` |
|       5 | 2675 | `				*pzWhy = "callable routes through __callStatic(), which cannot be a fiber body here";` |
|       5 | 2676 | `				return 0;` |
|       - | 2677 | `			}` |
|       3 | 2678 | `			return &pMethod->sFunc;` |
|       - | 2679 | `		}` |
|     497 | 2680 | `		pEntry = PH7_VmGetUserFunction(pVm, sName.zString, sName.nByte,` |
|     328 | 2681 | `			(pCallable->iFlags & MEMOBJ_AUX_ENGINEFN) != 0);` |
|     333 | 2682 | `		if( pEntry == 0 ){` |
|       3 | 2683 | `			*pzWhy = PH7_VmGetHostFunction(pVm, sName.zString, sName.nByte, FALSE)` |
|       - | 2684 | `				? "callable is an internal function, which cannot be a fiber body here"` |
|       1 | 2685 | `				: "callable names no such function";` |
|       3 | 2686 | `			return 0;` |
|       - | 2687 | `		}` |
|     331 | 2688 | `		return (ph7_vm_func *)pEntry->pUserData;` |
|       - | 2689 | `	}` |
|     ! 0 | 2690 | `	*pzWhy = "callable is not a string, array or object";` |
|     ! 0 | 2691 | `	return 0;` |
|     179 | 2692 | `}` |
|       - | 2693 | `/*` |
|       - | 2694 | ` * Resolve the callable stored in a Fiber's $__callable attribute.` |
|       - | 2695 | ` * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).` |
|       - | 2696 | ` * If the callable is a closure (object), *ppThis is set to the closure instance` |
|       - | 2697 | ` * so that start() can bind it as $this for the closure environment.` |
|       - | 2698 | ` */` |
|     350 | 2699 | `static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,` |
|       - | 2700 | `	ph7_class_instance **ppThis)` |
|       5 | 2701 | `{` |
|     355 | 2702 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 2703 | `	ph7_value *pCallable;` |
|       - | 2704 | `	SyString sAttrName;` |
|     355 | 2705 | `	*ppThis = 0;` |
|     355 | 2706 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     355 | 2707 | `	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);` |
|     355 | 2708 | `	if( pCallable == 0 \|\| (pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ\|MEMOBJ_HASHMAP)) == 0 ){` |
|     ! 0 | 2709 | `		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");` |
|     ! 0 | 2710 | `		return 0;` |
|       - | 2711 | `	}` |
|     355 | 2712 | `	if( pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP) ){` |
|     259 | 2713 | `		const char *zWhy = 0;` |
|     259 | 2714 | `		ph7_vm_func *pFunc = VmFiberCallableBody(pVm, pCallable, ppThis, &zWhy);` |
|     259 | 2715 | `		if( pFunc == 0 ){` |
|      11 | 2716 | `			PH7_VmThrowException(pCtx, "FiberError", "Fiber %s", zWhy);` |
|       5 | 2717 | `		}` |
|     259 | 2718 | `		return pFunc;` |
|     ! 0 | 2719 | `	}else{` |
|     101 | 2720 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|       - | 2721 | `		ph7_class_method *pMethod;` |
|     101 | 2722 | `		if( VmValueIsClosure(pVm, pCallable) ){` |
|       - | 2723 | `			/* A real Closure object: unwrap to its underlying callable name (the single` |
|       - | 2724 | `			 * source of truth, VmClosureUnwrap) and resolve that function. Its captured` |
|       - | 2725 | ``			 * environment (including any `$this`) rides along in the named function's`` |
|       - | 2726 | `			 * aClosureEnv, installed by VmFiberSetupFrame, so *ppThis stays 0. */` |
|       - | 2727 | `			ph7_value sName;` |
|      99 | 2728 | `			ph7_vm_func *pUnwrapped = 0;` |
|      99 | 2729 | `			const char *zWhyClo = 0;` |
|      99 | 2730 | `			PH7_MemObjInit(pVm, &sName);` |
|      99 | 2731 | `			if( VmClosureUnwrap(pVm, pCallable, &sName) == SXRET_OK ){` |
|       - | 2732 | ``				/* The engine's own `[closure_N]` key, which only this mark gets past the`` |
|       - | 2733 | `				 * script-facing name screen (PH7_VmGetUserFunction). */` |
|      99 | 2734 | `				sName.iFlags \|= MEMOBJ_AUX_ENGINEFN;` |
|       - | 2735 | `` 				/* The unwrap answers a NAME for a plain closure and a `[target, method]` `` |
|       - | 2736 | `				 * pair for a first-class callable taken from a method -- so it goes through` |
|       - | 2737 | `				 * the same body-finder as a callable the program wrote. Without it a` |
|       - | 2738 | ``				 * `$o->stat(...)` fiber could not be resolved at all. */`` |
|      99 | 2739 | `				pUnwrapped = VmFiberCallableBody(pVm, &sName, ppThis, &zWhyClo);` |
|      47 | 2740 | `			}` |
|      99 | 2741 | `			PH7_MemObjRelease(&sName);` |
|      99 | 2742 | `			if( pUnwrapped ){` |
|       - | 2743 | `				/* A BOUND closure parked its $this in the pClosureThis transient` |
|       - | 2744 | `				 * (VmClosureUnwrap): consume it as the fiber's $this — it wins over` |
|       - | 2745 | `				 * the creation-time env capture (VmFiberSetupFrame's skip). *ppThis` |
|       - | 2746 | `				 * is a borrow (the closure's $__this attr keeps the object alive` |
|       - | 2747 | `				 * through $__callable), so drop the parked ref. The scope transient` |
|       - | 2748 | `				 * is cleared alongside: fiber bodies don't model pBoundScope` |
|       - | 2749 | `				 * visibility (recorded residual), and a stale transient would` |
|       - | 2750 | `				 * poison the next OP_CALL's frame. */` |
|      99 | 2751 | `				if( pVm->pClosureThis ){` |
|     ! 0 | 2752 | `					*ppThis = pVm->pClosureThis;` |
|     ! 0 | 2753 | `					PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 2754 | `					pVm->pClosureThis = 0;` |
|     ! 0 | 2755 | `				}` |
|      99 | 2756 | `				pVm->pClosureScope = 0;` |
|      99 | 2757 | `				pVm->bClosureScreened = 0;` |
|      99 | 2758 | `				return pUnwrapped;` |
|       - | 2759 | `			}` |
|     ! 0 | 2760 | `			if( pVm->pClosureThis ){` |
|       - | 2761 | `				/* Failed resolution: drop the parked transient so it neither leaks` |
|       - | 2762 | `				 * nor poisons the next call. */` |
|     ! 0 | 2763 | `				PH7_ClassInstanceUnref(pVm->pClosureThis);` |
|     ! 0 | 2764 | `				pVm->pClosureThis = 0;` |
|     ! 0 | 2765 | `			}` |
|     ! 0 | 2766 | `			pVm->pClosureScope = 0;` |
|     ! 0 | 2767 | `			pVm->bClosureScreened = 0;` |
|     ! 0 | 2768 | `			PH7_VmThrowException(pCtx, "FiberError", zWhyClo` |
|     ! 0 | 2769 | `				? "Fiber %s" : "Fiber callable closure could not be resolved", zWhyClo);` |
|     ! 0 | 2770 | `			return 0;` |
|       - | 2771 | `		}` |
|       - | 2772 | `		/* Object callable — resolve __invoke method */` |
|       3 | 2773 | `		pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|       - | 2774 | `			sizeof("__invoke") - 1);` |
|       3 | 2775 | `		if( pMethod == 0 ){` |
|     ! 0 | 2776 | `			PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 2777 | `				"Fiber callable object has no __invoke method");` |
|     ! 0 | 2778 | `			return 0;` |
|       - | 2779 | `		}` |
|       3 | 2780 | `		*ppThis = pClosure;` |
|       3 | 2781 | `		return &pMethod->sFunc;` |
|       - | 2782 | `	}` |
|     180 | 2783 | `}` |
|       - | 2784 | `/*` |
|       - | 2785 | ` * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:` |
|       - | 2786 | ` * type casting, pass-by-reference handling, default values, and closure environment.` |
|       - | 2787 | ` * The fiber's frame must be at the top of pVm->pFrame when this is called.` |
|       - | 2788 | ` */` |
|       - | 2789 | `/*` |
|       - | 2790 | ` * Enforce one formal parameter's declared type on an argument being installed.` |
|       - | 2791 | ` * THE single implementation of the per-argument check, shared by the` |
|       - | 2792 | ` * generator/fiber initial-frame binder below (band A #2) and both OP_CALL` |
|       - | 2793 | ` * install paths (named-map and positional — they carried two verbatim copies` |
|       - | 2794 | ` * until the recorded(f) fold): union types via VmCoerceToUnion, class and` |
|       - | 2795 | ` * pseudo types (VmCheckPseudoType + VmResolveTypeClass + instanceof, so` |
|       - | 2796 | ``  * interfaces/abstract classes and self/parent resolve), the bare `object` `` |
|       - | 2797 | ` * hint, and scalars via VmEnforceScalarType (weak-mode coercion in place,` |
|       - | 2798 | ` * strict rejection otherwise) — with the VM_FUNC_ARG_NULLABLE guard letting` |
|       - | 2799 | `` * null through for `?type` and implicit-nullable `Type $x = null` params,`` |
|       - | 2800 | ` * and whole-real materialization on a mask match.` |
|       - | 2801 | ` * Returns SXRET_OK (value possibly coerced) or the VmThrowTypeErrorForArg` |
|       - | 2802 | ` * status for the caller to route — normalized so an INLINE-caught throw` |
|       - | 2803 | ` * (VmThrowInline records only a pc-redirect and reports SXRET_OK) still` |
|       - | 2804 | ` * comes back as PH7_EXCEPTION: the binding must stop; the OP_CALL sites` |
|       - | 2805 | ` * force rc = PH7_EXCEPTION on any non-OK status anyway, and the generator` |
|       - | 2806 | ` * block's PH7_INLINE_RESUME_BREAK consumes the redirect.` |
|       - | 2807 | ` */` |
|     412 | 2808 | `static sxi32 VmGenArgThrowStatus(ph7_vm *pVm, sxi32 rcThrow)` |
|       5 | 2809 | `{` |
|     417 | 2810 | `	if( rcThrow == SXRET_OK && pVm->pInlineInstr ){` |
|     ! 0 | 2811 | `		return PH7_EXCEPTION;` |
|       - | 2812 | `	}` |
|     417 | 2813 | `	return rcThrow;` |
|     211 | 2814 | `}` |
|  352946 | 2815 | `PH7_PRIVATE sxi32 VmEnforceArgType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_vm_func_arg *pFormal,` |
|       - | 2816 | `	sxu32 nArgPos, ph7_value *pVal, int bStrict, ph7_class *pSelfHint)` |
|       5 | 2817 | `{` |
|  352951 | 2818 | `	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){` |
|     288 | 2819 | `		if( VmCoerceToUnion(pVm,pVal,&pFormal->aUnionAlts,` |
|     293 | 2820 | `			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,bStrict,pSelfHint) != SXRET_OK ){` |
|       - | 2821 | `			const char *zGiven;` |
|      80 | 2822 | `			const char *zExpected = "union";` |
|       - | 2823 | `			char zBuf[128];` |
|       - | 2824 | `			char zTypeBuf[128];` |
|      80 | 2825 | `			if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      50 | 2826 | `				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));` |
|      57 | 2827 | `			}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|      10 | 2828 | `				zGiven = "null";` |
|       6 | 2829 | `			}else{` |
|      24 | 2830 | `				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));` |
|       - | 2831 | `			}` |
|      80 | 2832 | `			if( SyStringLength(&pFormal->sTypeName) > 0 ){` |
|     118 | 2833 | `				zExpected = VmHintTextResolved(pVm,&pFormal->sTypeName,pSelfHint,` |
|      38 | 2834 | `					zTypeBuf,sizeof(zTypeBuf));` |
|      38 | 2835 | `			}` |
|     118 | 2836 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      38 | 2837 | `				&pFormal->sName,zExpected,zGiven));` |
|       - | 2838 | `		}` |
|     120 | 2839 | `		return SXRET_OK;` |
|       - | 2840 | `	}` |
|  352754 | 2841 | `	if( pFormal->nType == 0` |
|  196976 | 2842 | `	 \|\| ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){` |
|  313891 | 2843 | `		return SXRET_OK;` |
|       - | 2844 | `	}` |
|   38873 | 2845 | `	if( pFormal->nType == SXU32_HIGH ){` |
|       - | 2846 | `		/* Class or pseudo type */` |
|    5569 | 2847 | `		SyString *pName = &pFormal->sClass;` |
|       - | 2848 | `		ph7_class *pClass;` |
|    5569 | 2849 | `		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);` |
|    5569 | 2850 | `		if( rcPseudo == 0 ){` |
|       - | 2851 | `			char zTypeBuf[128],zGivenBuf[128];` |
|     140 | 2852 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      34 | 2853 | `				&pFormal->sName,` |
|      68 | 2854 | `				VmClassHintTypeName(pName,0,` |
|      68 | 2855 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|      34 | 2856 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 2857 | `		}` |
|    5501 | 2858 | `		pClass = 0;` |
|    5501 | 2859 | `		if( rcPseudo != 1 && !VmClassHintMatches(&(*pVm),pName,pSelfHint,pVal,&pClass) ){` |
|       - | 2860 | `			char zTypeBuf[128],zGivenBuf[128];` |
|      99 | 2861 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      24 | 2862 | `				&pFormal->sName,` |
|      48 | 2863 | `				VmClassHintTypeName(pName,pClass,` |
|      48 | 2864 | `					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),` |
|      24 | 2865 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 2866 | `		}` |
|    5453 | 2867 | `		return SXRET_OK;` |
|       - | 2868 | `	}` |
|   33309 | 2869 | `	if( (pVal->iFlags & pFormal->nType) == 0 ){` |
|       - | 2870 | `		char zGivenBuf[128];` |
|     405 | 2871 | `		if( pFormal->nType == MEMOBJ_OBJ ){` |
|     124 | 2872 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|       8 | 2873 | `				&pFormal->sName,"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 2874 | `		}` |
|     389 | 2875 | `		if( VmEnforceScalarType(pVal,pFormal->nType,bStrict) != SXRET_OK ){` |
|       - | 2876 | `			char zTypeBuf[128];` |
|     299 | 2877 | `			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,` |
|      98 | 2878 | `				&pFormal->sName,` |
|      98 | 2879 | `				VmScalarTypeName(pFormal->nType,&pFormal->sTypeName,zTypeBuf,sizeof(zTypeBuf)),` |
|      98 | 2880 | `				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));` |
|       - | 2881 | `		}` |
|      98 | 2882 | `	}else{` |
|       - | 2883 | `		/* Mask matched — an int param accepting a whole-real materializes` |
|       - | 2884 | `		 * it (php: g(1.0) into int $x is int(1)). */` |
|   32909 | 2885 | `		VmMaterializeIntTyped(pVal,pFormal->nType);` |
|       - | 2886 | `	}` |
|   33097 | 2887 | `	return SXRET_OK;` |
|  177266 | 2888 | `}` |
|       - | 2889 | `/*` |
|       - | 2890 | ` * Record a caller slot this body's frame now ALIASES through a by-reference` |
|       - | 2891 | ` * parameter. The body outlives its caller, so the two frames cannot each own the` |
|       - | 2892 | ` * slot: the caller's teardown counts this frame's name binding as a holder and` |
|       - | 2893 | ` * leaves the value standing, and VmReleaseExecCtx asks PH7_VmReleaseUnheldSlot for` |
|       - | 2894 | ` * every row here once its own names are gone — whichever dies last frees it.` |
|       - | 2895 | ` */` |
|      34 | 2896 | `static void VmCtxAliasByRefArg(ph7_exec_ctx *pExecCtx,sxu32 nIdx)` |
|       3 | 2897 | `{` |
|      37 | 2898 | `	sxu32 *aIdx = (sxu32 *)SySetBasePtr(&pExecCtx->aByRefArg);` |
|       - | 2899 | `	sxu32 n;` |
|      37 | 2900 | `	for( n = 0 ; n < SySetUsed(&pExecCtx->aByRefArg) ; ++n ){` |
|     ! 0 | 2901 | `		if( aIdx[n] == nIdx ){` |
|       - | 2902 | ``			/* Two parameters over one actual (`g($x,$x)`) is ONE slot to give back. */`` |
|     ! 0 | 2903 | `			return;` |
|       - | 2904 | `		}` |
|     ! 0 | 2905 | `	}` |
|      37 | 2906 | `	SySetPut(&pExecCtx->aByRefArg,(const void *)&nIdx);` |
|      20 | 2907 | `}` |
|    1034 | 2908 | `PH7_PRIVATE sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,` |
|       - | 2909 | `	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg,` |
|       - | 2910 | `	int bStrict, ph7_class *pSelfHint, int bCallSiteInMsg, int bAliasByRef)` |
|       5 | 2911 | `{` |
|    1039 | 2912 | `	ph7_vm_func *pFunc = pExecCtx->pFunc;` |
|       - | 2913 | `	ph7_vm_func_arg *aFormalArg;` |
|       - | 2914 | `	sxu32 nFormal, n;` |
|    1039 | 2915 | `	sxu32 nReqGF = 0, nNonVarGF = 0; /* too-few-args watermark (0 = exempt) */` |
|       - | 2916 | `	VmSlot sSlot;` |
|       - | 2917 | `	sxi32 rc;` |
|       - | 2918 | `	/* Install $this for closure/method callables */` |
|    1039 | 2919 | `	if( pClosureThis ){` |
|       - | 2920 | `		static const SyString sThis = { "this", sizeof("this") - 1 };` |
|      34 | 2921 | `		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);` |
|      34 | 2922 | `		if( pObj ){` |
|      34 | 2923 | `			pObj->x.pOther = pClosureThis;` |
|      34 | 2924 | `			MemObjSetType(pObj, MEMOBJ_OBJ);` |
|      34 | 2925 | `			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */` |
|      15 | 2926 | `		}` |
|       - | 2927 | `		/* And on the FRAME, which is what everything asking "whose method is this` |
|       - | 2928 | `		 * activation" reads: a coroutine body installed the receiver only as a` |
|       - | 2929 | ``		 * variable, so a `debug_backtrace()` frame for a generator METHOD came back`` |
|       - | 2930 | ``		 * with its class but no `object` — and twig's error reporter, which finds the`` |
|       - | 2931 | `` 		 * template to blame by looking for `$trace['object'] instanceof Template` `` |
|       - | 2932 | ``		 * across the backtrace, found none and could not say `at line N` for any`` |
|       - | 2933 | `		 * template whose failing frame is a compiled generator. Borrowed exactly like` |
|       - | 2934 | `		 * VmEnterFrame's: the variable installed above owns the reference, and it` |
|       - | 2935 | `		 * lives and dies with this frame. */` |
|      34 | 2936 | `		if( pExecCtx->pFrame ){` |
|      34 | 2937 | `			pExecCtx->pFrame->pThis = pClosureThis;` |
|      15 | 2938 | `		}` |
|      15 | 2939 | `	}` |
|       - | 2940 | `	/* Install static variables */` |
|    1039 | 2941 | `	if( SySetUsed(&pFunc->aStatic) > 0 ){` |
|       - | 2942 | `		ph7_vm_func_static_var *aStatic;` |
|       - | 2943 | `		ph7_value *pVal;` |
|     ! 0 | 2944 | `		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|     ! 0 | 2945 | `		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){` |
|     ! 0 | 2946 | `			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);` |
|     ! 0 | 2947 | `			if( pVal ){` |
|     ! 0 | 2948 | `				sSlot.pUserData = 0;` |
|     ! 0 | 2949 | `				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);` |
|     ! 0 | 2950 | `				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,` |
|     ! 0 | 2951 | `					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));` |
|     ! 0 | 2952 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|     ! 0 | 2953 | `					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal,FALSE);` |
|     ! 0 | 2954 | `				}` |
|     ! 0 | 2955 | `			}` |
|     ! 0 | 2956 | `		}` |
|     ! 0 | 2957 | `	}` |
|       - | 2958 | `	/* Install arguments with type casting and default values (matching OP_CALL) */` |
|    1039 | 2959 | `	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|    1039 | 2960 | `	nFormal = SySetUsed(&pFunc->aArgs);` |
|       - | 2961 | `	/* Actual call arity for func_num_args()/func_get_args() (band A #4) */` |
|    1039 | 2962 | `	pExecCtx->pFrame->nActualArgs = nArg;` |
|       - | 2963 | `	{` |
|       - | 2964 | `		/* Too-few-arguments watermark — checked per formal INSIDE the install` |
|       - | 2965 | `		 * loop below, after the passed args' type checks, matching php's` |
|       - | 2966 | `		 * RECV order (a type error on a passed argument beats the count` |
|       - | 2967 | `		 * error). Generators throw at the g(...) call site (message embeds` |
|       - | 2968 | `		 * it); fibers at Fiber::start() (php omits the call-site segment).` |
|       - | 2969 | `		 * Prelude builtins are included — VmThrowBuiltinTooFewArgs words them` |
|       - | 2970 | `		 * as php words an internal callable. */` |
|    1039 | 2971 | `	nReqGF = VmFuncRequiredArgCount(pFunc,&nNonVarGF);` |
|       - | 2972 | `	}` |
|    1247 | 2973 | `	for( n = 0; n < nFormal; n++ ){` |
|       - | 2974 | `		ph7_value *pObj;` |
|     247 | 2975 | `		if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|       - | 2976 | `			/* Variadic formal: collect this and every remaining actual into a` |
|       - | 2977 | `			 * fresh array (php semantics — pre-fix nothing collected here, so a` |
|       - | 2978 | ``			 * `function g(int ...$xs)` generator saw a bare scalar in $xs).`` |
|       - | 2979 | `			 * Per-element checks mirror OP_CALL's variadic install: union via` |
|       - | 2980 | ``			 * the shared helper, scalar coerce/TypeError, `object` hint; a`` |
|       - | 2981 | `			 * class-typed variadic element is (like OP_CALL) not checked. */` |
|      12 | 2982 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      12 | 2983 | `			if( pObj ){` |
|       - | 2984 | `				sxu32 nVariadicIdx;` |
|       - | 2985 | `				ph7_hashmap *pMap;` |
|       - | 2986 | `				sxu32 k;` |
|      12 | 2987 | `				PH7_MemObjToHashmap(pObj);` |
|       - | 2988 | `				/* Capture the slot index now: PH7_HashmapInsert used to reallocate` |
|       - | 2989 | `				 * pVm->aMemObj and dangle pObj (same hazard as OP_CALL's path).` |
|       - | 2990 | `				 * Redundant now the table is segmented; left for the harvest sweep. */` |
|      12 | 2991 | `				nVariadicIdx = pObj->nIdx;` |
|      12 | 2992 | `				pMap = (ph7_hashmap *)pObj->x.pOther;` |
|      20 | 2993 | `				for( k = n; k < (sxu32)nArg; k++ ){` |
|       9 | 2994 | `					if( apArg[k] == 0 ){` |
|     ! 0 | 2995 | `						continue; /* a named call's hole: the formal it names is not this one */` |
|       - | 2996 | `					}` |
|      11 | 2997 | `					if( ((aFormalArg[n].iFlags & VM_FUNC_ARG_UNION)` |
|       9 | 2998 | `					   \|\| (aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH)) ){` |
|       7 | 2999 | `						rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,apArg[k],bStrict,pSelfHint);` |
|       7 | 3000 | `						if( rc != SXRET_OK ){` |
|     ! 0 | 3001 | `							return rc;` |
|       - | 3002 | `						}` |
|       3 | 3003 | `					}` |
|       8 | 3004 | `					if( bAliasByRef && (aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF)` |
|       6 | 3005 | `					 && apArg[k]->nIdx != SXU32_HIGH ){` |
|       - | 3006 | `						/* A by-ref variadic tail aliases its actuals here too — the` |
|       - | 3007 | `						 * ordinary call's rule, one container over. */` |
|       3 | 3008 | `						VmCtxAliasByRefArg(pExecCtx,apArg[k]->nIdx);` |
|       3 | 3009 | `						PH7_HashmapInsertByRef(pMap,0,apArg[k]->nIdx);` |
|       2 | 3010 | `					}else{` |
|       7 | 3011 | `						PH7_HashmapInsert(pMap,0,apArg[k]);` |
|       - | 3012 | `					}` |
|       5 | 3013 | `				}` |
|      12 | 3014 | `				sSlot.nIdx = nVariadicIdx;` |
|      12 | 3015 | `				sSlot.pUserData = 0;` |
|      12 | 3016 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|       5 | 3017 | `			}` |
|      12 | 3018 | `			break; /* All remaining actuals consumed */` |
|       - | 3019 | `		}` |
|     237 | 3020 | `		if( n < (sxu32)nArg && apArg[n] != 0 ){` |
|       - | 3021 | `			/* Argument provided — install with declared-type enforcement.` |
|       - | 3022 | ``			 * A NULL entry is a named call's HOLE: `g(a: 1, c: 9)` names the`` |
|       - | 3023 | `			 * first and third formals and says nothing about the second, so the` |
|       - | 3024 | `			 * list arrives one entry per formal and the hole falls through to the` |
|       - | 3025 | `			 * default branch below, exactly as php's binder does.` |
|       - | 3026 | `			 * php binds and type-checks generator arguments EAGERLY at the` |
|       - | 3027 | `			 * g(...) call site (and fiber arguments at Fiber::start()), so the` |
|       - | 3028 | `			 * enforcement lives here, mirroring the OP_CALL install path via` |
|       - | 3029 | `			 * VmEnforceArgType (TypeError on mismatch, weak coercion in` |
|       - | 3030 | `			 * place otherwise) instead of the old silent xCast. A variadic` |
|       - | 3031 | `			 * formal collects as-is (no per-element declared-type model). */` |
|     192 | 3032 | `			if( bAliasByRef && (aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF)` |
|     110 | 3033 | `			 && apArg[n]->nIdx != SXU32_HIGH ){` |
|       - | 3034 | `				/* php binds a generator's by-REFERENCE parameter to the CALLER's slot at` |
|       - | 3035 | `				 * the g(...) that builds the Generator, so the body's write reaches the` |
|       - | 3036 | `				 * caller's variable whenever it eventually runs. Copying it left the` |
|       - | 3037 | `				 * actual untouched for every resume. The type check runs on the actual,` |
|       - | 3038 | `				 * as OP_CALL's by-ref binder does, and never on a copy the alias` |
|       - | 3039 | `				 * replaces. Fiber::start() and the embedder entry pass by VALUE (php's` |
|       - | 3040 | `				 * own decision at those two boundaries), hence bAliasByRef. */` |
|      35 | 3041 | `				sxi32 iPreFlags = apArg[n]->iFlags;` |
|      35 | 3042 | `				rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,apArg[n],bStrict,pSelfHint);` |
|      35 | 3043 | `				if( rc != SXRET_OK ){` |
|     ! 0 | 3044 | `					return rc;` |
|       - | 3045 | `				}` |
|       - | 3046 | `				/* A declared type's conversion is what the reference holds (the ordinary` |
|       - | 3047 | `				 * call's rule; the check ran on the operand-stack copy). */` |
|      35 | 3048 | `				PH7_VmByRefArgWriteBack(pVm,apArg[n],iPreFlags);` |
|      51 | 3049 | `				PH7_VmBindVarSlot(pVm,pExecCtx->pFrame,` |
|      32 | 3050 | `					SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName),` |
|      32 | 3051 | `					apArg[n]->nIdx);` |
|      35 | 3052 | `				VmCtxAliasByRefArg(pExecCtx,apArg[n]->nIdx);` |
|      35 | 3053 | `				sSlot.nIdx = apArg[n]->nIdx;` |
|      35 | 3054 | `				sSlot.pUserData = 0;` |
|      35 | 3055 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      35 | 3056 | `				continue;` |
|       - | 3057 | `			}` |
|     165 | 3058 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|     165 | 3059 | `			if( pObj ){` |
|     165 | 3060 | `				PH7_MemObjStore(apArg[n], pObj);` |
|     165 | 3061 | `				if( (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){` |
|     165 | 3062 | `					rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,pObj,bStrict,pSelfHint);` |
|     165 | 3063 | `					if( rc != SXRET_OK ){` |
|      18 | 3064 | `						return rc;` |
|       - | 3065 | `					}` |
|      72 | 3066 | `				}` |
|     149 | 3067 | `				sSlot.nIdx = pObj->nIdx;` |
|     149 | 3068 | `				sSlot.pUserData = 0;` |
|     149 | 3069 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      77 | 3070 | `			}` |
|     115 | 3071 | `		}else if( n < nReqGF ){` |
|       - | 3072 | `			/* Required formal with no actual: php's ArgumentCountError, at` |
|       - | 3073 | `			 * this point in the install order (see the watermark comment). */` |
|      13 | 3074 | `			return VmGenArgThrowStatus(pVm,` |
|       8 | 3075 | `				(pFunc->iFlags & VM_FUNC_INTERNAL)` |
|     ! 0 | 3076 | `					? VmThrowBuiltinTooFewArgs(pVm,pSelfHint,&pFunc->sName,` |
|     ! 0 | 3077 | `						(sxu32)nArg,nReqGF,SySetUsed(&pFunc->aArgs))` |
|      12 | 3078 | `					: VmThrowTooFewArgs(pVm,pSelfHint,&pFunc->sName,pFunc,` |
|       4 | 3079 | `						(sxu32)nArg,nReqGF,nNonVarGF,bCallSiteInMsg));` |
|      35 | 3080 | `		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){` |
|       - | 3081 | `			/* Default value */` |
|      35 | 3082 | `			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);` |
|      35 | 3083 | `			if( pObj ){` |
|      35 | 3084 | `				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj,FALSE);` |
|      35 | 3085 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 | 3086 | `					return rc;` |
|       - | 3087 | `				}` |
|       - | 3088 | `` 				/* A null default on an implicitly-nullable `Type $x = null` `` |
|       - | 3089 | `				 * param must stay null (php); only non-null defaults keep the` |
|       - | 3090 | `				 * legacy shaping cast. */` |
|      32 | 3091 | `				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH` |
|      11 | 3092 | `				 && !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){` |
|       7 | 3093 | `					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){` |
|     ! 0 | 3094 | `						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);` |
|     ! 0 | 3095 | `						if( xCast ){` |
|     ! 0 | 3096 | `							xCast(pObj);` |
|     ! 0 | 3097 | `						}` |
|     ! 0 | 3098 | `					}else{` |
|       - | 3099 | `						/* Mask matched — a const-indirected whole-real default` |
|       - | 3100 | ``						 * (`int $x = FOO` with FOO = 2.0) materializes as int. */`` |
|       7 | 3101 | `						VmMaterializeIntTyped(pObj,aFormalArg[n].nType);` |
|       - | 3102 | `					}` |
|       3 | 3103 | `				}` |
|      35 | 3104 | `				sSlot.nIdx = pObj->nIdx;` |
|      35 | 3105 | `				sSlot.pUserData = 0;` |
|      35 | 3106 | `				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);` |
|      16 | 3107 | `			}` |
|      16 | 3108 | `		}` |
|      93 | 3109 | `	}` |
|       - | 3110 | `	/* Install closure environment (captured variables) */` |
|    1015 | 3111 | `	if( pFunc->iFlags & VM_FUNC_CLOSURE ){` |
|       - | 3112 | `		ph7_vm_func_closure_env *aEnv, *pEnv;` |
|       - | 3113 | `		ph7_value *pValue;` |
|       - | 3114 | `		sxu32 iEnv;` |
|     285 | 3115 | `		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     681 | 3116 | `		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){` |
|     401 | 3117 | `			pEnv = &aEnv[iEnv];` |
|     401 | 3118 | `			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){` |
|     263 | 3119 | `				continue;` |
|       - | 3120 | `			}` |
|     138 | 3121 | `			if( pClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1` |
|       7 | 3122 | `			 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){` |
|       - | 3123 | `				/* An explicit bound $this (bindTo/bind/call) wins over the` |
|       - | 3124 | `				 * creation-time captured $this, php-exact (mirrors OP_CALL). */` |
|       3 | 3125 | `				continue;` |
|       - | 3126 | `			}` |
|     141 | 3127 | `			if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){` |
|       - | 3128 | `				/* Captured by reference: link the name to the shared slot` |
|       - | 3129 | `				 * (no copy), mirroring the OP_CALL env install. */` |
|       8 | 3130 | `				if( SyHashGet(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){` |
|      11 | 3131 | `					SyHashInsert(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),` |
|       6 | 3132 | `						SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));` |
|       3 | 3133 | `				}` |
|       8 | 3134 | `				continue;` |
|       - | 3135 | `			}` |
|     135 | 3136 | `			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);` |
|     135 | 3137 | `			if( pValue == 0 ){` |
|     ! 0 | 3138 | `				continue;` |
|       - | 3139 | `			}` |
|     135 | 3140 | `			PH7_MemObjRelease(pValue);` |
|     135 | 3141 | `			PH7_MemObjStore(&pEnv->sValue, pValue);` |
|      70 | 3142 | `		}` |
|     140 | 3143 | `	}` |
|    1015 | 3144 | `	return SXRET_OK;` |
|     522 | 3145 | `}` |
|       - | 3146 | `/*` |
|       - | 3147 | ` * Fiber->start(...$args) — resolve callable, create exec context, install` |
|       - | 3148 | ` * arguments/closure-env/$this (matching OP_CALL semantics), and start.` |
|       - | 3149 | ` *` |
|       - | 3150 | ` * As a native VARIADIC method the arguments arrive directly as (nArg, apArg);` |
|       - | 3151 | ` * the prelude used to hand them over as a single func_get_args() array, which` |
|       - | 3152 | ` * this had to walk and snapshot out of pVm->aMemObj.` |
|       - | 3153 | ` */` |
|     352 | 3154 | `PH7_PRIVATE int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 3155 | `{` |
|     357 | 3156 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 3157 | `	ph7_class_instance *pThis;` |
|       - | 3158 | `	ph7_class_instance *pClosureThis;` |
|       - | 3159 | `	ph7_exec_ctx *pExecCtx;` |
|       - | 3160 | `	ph7_vm_func *pFunc;` |
|       - | 3161 | `	ph7_value sResult;` |
|       - | 3162 | `	ph7_value *pCtxAttr;` |
|       - | 3163 | `	SyString sAttrName;` |
|       - | 3164 | `	sxi32 rc;` |
|     357 | 3165 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     357 | 3166 | `	if( pRecv == 0 \|\| (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 3167 | `		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");` |
|       - | 3168 | `	}` |
|     357 | 3169 | `	pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|       - | 3170 | `	/* Check if already started (has a __ctx) */` |
|     357 | 3171 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|     357 | 3172 | `	if( pExecCtx != 0 ){` |
|       3 | 3173 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 3174 | `			"Cannot start a fiber that has already been started");` |
|       - | 3175 | `	}` |
|       - | 3176 | `	/* Resolve callable */` |
|     355 | 3177 | `	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);` |
|     355 | 3178 | `	if( pFunc == 0 ){` |
|      11 | 3179 | `		return PH7_EXCEPTION;` |
|       - | 3180 | `	}` |
|       - | 3181 | ``	/* Fiber::start()'s own `...$args` are by VALUE whatever the body declares, so php`` |
|       - | 3182 | `		 * warns for every by-reference parameter and the body operates on a copy — the` |
|       - | 3183 | `		 * value PHL already produced, without the one diagnostic that says so. Named off` |
|       - | 3184 | `		 * the stored callable, which is what carries the class for a method one. */` |
|     345 | 3185 | `	if( nArg > 0 ){` |
|       - | 3186 | `		SyString sCbName;` |
|       - | 3187 | `		ph7_value *pCbVal;` |
|      17 | 3188 | `		SyStringInitFromBuf(&sCbName, "__callable", 10);` |
|      17 | 3189 | `		pCbVal = PH7_ClassInstanceFetchAttr(pThis, &sCbName);` |
|      17 | 3190 | `		if( pCbVal ){` |
|      17 | 3191 | `			PH7_VmWarnByRefArgsGivenValue(pVm, pCbVal, nArg, 0, 0);` |
|       7 | 3192 | `		}` |
|       7 | 3193 | `	}` |
|       - | 3194 | `	/* Create execution context now that we know the function */` |
|     345 | 3195 | `	pExecCtx = VmNewExecCtx(pVm, pFunc);` |
|     345 | 3196 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 3197 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 3198 | `			"Fiber::start(): out of memory");` |
|       - | 3199 | `	}` |
|       - | 3200 | `	/* Store context in $this->__ctx */` |
|     345 | 3201 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     345 | 3202 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     345 | 3203 | `	if( pCtxAttr ){` |
|     345 | 3204 | `		pCtxAttr->x.pOther = pExecCtx;` |
|     345 | 3205 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|     170 | 3206 | `	}` |
|       - | 3207 | `	/* Temporarily attach the fiber's frame to the VM chain so that` |
|       - | 3208 | `	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables` |
|       - | 3209 | `	 * into the fiber's frame, not the caller's. */` |
|     345 | 3210 | `	pExecCtx->pFrame->pParent = pVm->pFrame;` |
|     345 | 3211 | `	pVm->pFrame = pExecCtx->pFrame;` |
|       - | 3212 | `	/* Unpack the args array and install into the frame */` |
|       - | 3213 | `	{` |
|       - | 3214 | `		/* The arguments are this call's own operand-stack slots, so they can be` |
|       - | 3215 | `		 * handed to the frame setup as-is. The old form had to snapshot them out of` |
|       - | 3216 | `		 * pVm->aMemObj first, because they arrived as a func_get_args() hashmap` |
|       - | 3217 | `		 * whose element values live in that pool — and VmFiberSetupFrame reserves` |
|       - | 3218 | `		 * memory objects (VmExtractMemObj) before reading its arguments, which back` |
|       - | 3219 | `		 * then reallocated the pool and dangled a raw pointer into it. Operand slots` |
|       - | 3220 | `		 * do not move, so the copy went with the array that made it necessary. (Pool` |
|       - | 3221 | `		 * slots do not move either since P1, which retires the hazard entirely.) */` |
|     345 | 3222 | `		ph7_value **apValues = (nArg > 0) ? apArg : 0;` |
|     345 | 3223 | `		int nActual = nArg;` |
|     345 | 3224 | `		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues,` |
|       - | 3225 | `			0 /* weak-mode arg binding, like call_user_func */, 0,` |
|       - | 3226 | `			FALSE/*Fiber::start(): php omits the call-site segment*/,` |
|       - | 3227 | `			FALSE/*php's Fiber::start() passes by VALUE and warns (recorded)*/);` |
|       - | 3228 | `		/* Nothing to free: apValues aliases the operand stack now, it is not a` |
|       - | 3229 | `		 * buffer this function allocated. */` |
|       - | 3230 | `	}` |
|       - | 3231 | `	/* Detach the frame — VmStartCtx will re-attach it */` |
|     345 | 3232 | `	pVm->pFrame = pExecCtx->pFrame->pParent;` |
|     345 | 3233 | `	pExecCtx->pFrame->pParent = 0;` |
|     345 | 3234 | `	if( rc != SXRET_OK ){` |
|       - | 3235 | `		/* Propagate the real status: a declared-type TypeError from the arg` |
|       - | 3236 | `		 * install (band A #2) must stay catchable, not become an abort. */` |
|       7 | 3237 | `		return (rc == PH7_EXCEPTION) ? PH7_EXCEPTION : PH7_ABORT;` |
|       - | 3238 | `	}` |
|     339 | 3239 | `	PH7_MemObjInit(pVm, &sResult);` |
|       - | 3240 | `	{` |
|       - | 3241 | `		/* php's EG(active_fiber): the fiber the running code is INSIDE, which is what` |
|       - | 3242 | `		 * Fiber::getCurrent() answers. Saved and restored around the body run, so a` |
|       - | 3243 | `		 * fiber that starts another one nests, and a fiber that finishes hands the` |
|       - | 3244 | `		 * name back to whoever was current before it. Borrowed for the duration: the` |
|       - | 3245 | `		 * receiver of this call owns the reference. */` |
|     339 | 3246 | `		ph7_class_instance *pOldFiber = pVm->pCurFiber;` |
|     339 | 3247 | `		pVm->pCurFiber = pThis;` |
|     339 | 3248 | `		rc = VmStartCtx(pVm, pExecCtx, &sResult);` |
|     339 | 3249 | `		pVm->pCurFiber = pOldFiber;` |
|       - | 3250 | `	}` |
|     339 | 3251 | `	if( rc == PH7_ABORT ){` |
|     ! 0 | 3252 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 | 3253 | `		return PH7_ABORT;` |
|       - | 3254 | `	}` |
|     339 | 3255 | `	if( rc == PH7_EXCEPTION ){` |
|       3 | 3256 | `		PH7_MemObjRelease(&sResult);` |
|       - | 3257 | `#ifdef PH7_CORO_STACK` |
|       3 | 3258 | `		if( pExecCtx->pEscaped ){` |
|       3 | 3259 | `			return VmFiberRaiseEscaped(pCtx, pExecCtx);` |
|       - | 3260 | `		}` |
|       - | 3261 | `#endif` |
|     ! 0 | 3262 | `		return PH7_EXCEPTION;` |
|       - | 3263 | `	}` |
|     337 | 3264 | `	ph7_result_value(pCtx, &sResult);` |
|     337 | 3265 | `	PH7_MemObjRelease(&sResult);` |
|     337 | 3266 | `	return PH7_OK;` |
|     181 | 3267 | `}` |
|       - | 3268 | `/*` |
|       - | 3269 | ` * Fiber->resume($value = null) — resume a suspended fiber.` |
|       - | 3270 | ` */` |
|     192 | 3271 | `PH7_PRIVATE int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 3272 | `{` |
|     197 | 3273 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 3274 | `	ph7_exec_ctx *pExecCtx;` |
|       - | 3275 | `	ph7_value sResult;` |
|       - | 3276 | `	ph7_value *pResumeVal;` |
|       - | 3277 | `	sxi32 rc;` |
|     197 | 3278 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     197 | 3279 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|     197 | 3280 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 3281 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");` |
|     ! 0 | 3282 | `		return PH7_OK;` |
|       - | 3283 | `	}` |
|     197 | 3284 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|     197 | 3285 | `	if( pExecCtx == 0 ){` |
|     ! 0 | 3286 | `		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");` |
|     ! 0 | 3287 | `		return PH7_OK;` |
|       - | 3288 | `	}` |
|     197 | 3289 | `	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       3 | 3290 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 3291 | `			"Cannot resume a fiber that is not suspended");` |
|       - | 3292 | `	}` |
|     195 | 3293 | `	pResumeVal = (nArg > 0) ? apArg[0] : 0;` |
|     195 | 3294 | `	PH7_MemObjInit(pVm, &sResult);` |
|       - | 3295 | `	{` |
|       - | 3296 | `		/* See Fiber::start(): the current fiber is this one for the length of the run. */` |
|     195 | 3297 | `		ph7_class_instance *pOldFiber = pVm->pCurFiber;` |
|     195 | 3298 | `		pVm->pCurFiber = (ph7_class_instance *)pRecv->x.pOther;` |
|     195 | 3299 | `		rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);` |
|     195 | 3300 | `		pVm->pCurFiber = pOldFiber;` |
|       - | 3301 | `	}` |
|     195 | 3302 | `	if( rc == PH7_ABORT ){` |
|     ! 0 | 3303 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 | 3304 | `		return PH7_ABORT;` |
|       - | 3305 | `	}` |
|     195 | 3306 | `	if( rc == PH7_EXCEPTION ){` |
|       6 | 3307 | `		PH7_MemObjRelease(&sResult);` |
|       - | 3308 | `#ifdef PH7_CORO_STACK` |
|       6 | 3309 | `		if( pExecCtx->pEscaped ){` |
|       6 | 3310 | `			return VmFiberRaiseEscaped(pCtx, pExecCtx);` |
|       - | 3311 | `		}` |
|       - | 3312 | `#endif` |
|     ! 0 | 3313 | `		return PH7_EXCEPTION;` |
|       - | 3314 | `	}` |
|     191 | 3315 | `	ph7_result_value(pCtx, &sResult);` |
|     191 | 3316 | `	PH7_MemObjRelease(&sResult);` |
|     191 | 3317 | `	return PH7_OK;` |
|     101 | 3318 | `}` |
|       - | 3319 | `/*` |
|       - | 3320 | ` * Fiber->throw(Throwable $exception) — resume the fiber by RAISING at its` |
|       - | 3321 | `` * suspension point, so `Fiber::suspend()` throws instead of returning. Same`` |
|       - | 3322 | ` * transport as Generator::throw(): the exception is parked on the context and` |
|       - | 3323 | ` * the resumed body raises it in its own frame at the top of the dispatch loop,` |
|       - | 3324 | ` * which is what lets a try/catch INSIDE the fiber catch it and carry on. The` |
|       - | 3325 | ` * answer is the next suspend value, or null if the body ran to completion --` |
|       - | 3326 | ` * symmetric with resume(). Every non-suspended state is php's one sentence.` |
|       - | 3327 | ` */` |
|      12 | 3328 | `PH7_PRIVATE int vm_builtin_Fiber_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 3329 | `{` |
|      13 | 3330 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 3331 | `	ph7_exec_ctx *pExecCtx;` |
|       - | 3332 | `	ph7_class_instance *pInj;` |
|       - | 3333 | `	ph7_value sResult;` |
|       - | 3334 | `	sxi32 rc;` |
|      13 | 3335 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      13 | 3336 | `	if( pRecv == 0 \|\| (pRecv->iFlags & MEMOBJ_OBJ) == 0 \|\| nArg < 1 ){` |
|     ! 0 | 3337 | `		return PH7_OK;` |
|       - | 3338 | `	}` |
|      13 | 3339 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 3340 | ``		return PH7_OK; /* the declared `Throwable $exception` screen already spoke */`` |
|       - | 3341 | `	}` |
|      13 | 3342 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|      13 | 3343 | `	if( pExecCtx == 0 \|\| pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       - | 3344 | `		/* php answers the same sentence for never-started, running and terminated. */` |
|       5 | 3345 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 3346 | `			"Cannot resume a fiber that is not suspended");` |
|       - | 3347 | `	}` |
|       - | 3348 | `	/* Hold a reference for the whole operation: the resumed body may bind the` |
|       - | 3349 | `	 * instance in a catch and release it again before we are back. */` |
|       9 | 3350 | `	pInj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|       9 | 3351 | `	pInj->iRef++;` |
|       9 | 3352 | `	pExecCtx->pInjected = pInj;   /* borrowed; consumed at the resume's loop top */` |
|       9 | 3353 | `	PH7_MemObjInit(pVm, &sResult);` |
|       - | 3354 | `	{` |
|       - | 3355 | `		/* See Fiber::start(): the current fiber is this one for the length of the run. */` |
|       9 | 3356 | `		ph7_class_instance *pOldFiber = pVm->pCurFiber;` |
|       9 | 3357 | `		pVm->pCurFiber = (ph7_class_instance *)pRecv->x.pOther;` |
|       9 | 3358 | `		rc = VmResumeCtx(pVm, pExecCtx, 0, &sResult);` |
|       9 | 3359 | `		pVm->pCurFiber = pOldFiber;` |
|       - | 3360 | `	}` |
|       - | 3361 | `	/* Normally consumed (cleared) at the loop top; clear it here too for the path` |
|       - | 3362 | `	 * where VmResumeCtx bails BEFORE entering the loop (the recursion-depth fatal),` |
|       - | 3363 | `	 * so no dangling borrowed pointer survives the Unref. */` |
|       9 | 3364 | `	pExecCtx->pInjected = 0;` |
|       9 | 3365 | `	PH7_ClassInstanceUnref(pInj);` |
|       9 | 3366 | `	if( rc == PH7_ABORT ){` |
|     ! 0 | 3367 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 | 3368 | `		return PH7_ABORT;` |
|       - | 3369 | `	}` |
|       9 | 3370 | `	if( rc == PH7_EXCEPTION ){` |
|       3 | 3371 | `		PH7_MemObjRelease(&sResult);` |
|       - | 3372 | `#ifdef PH7_CORO_STACK` |
|       3 | 3373 | `		if( pExecCtx->pEscaped ){` |
|       3 | 3374 | `			return VmFiberRaiseEscaped(pCtx, pExecCtx);` |
|       - | 3375 | `		}` |
|       - | 3376 | `#endif` |
|     ! 0 | 3377 | `		return PH7_EXCEPTION;` |
|       - | 3378 | `	}` |
|       7 | 3379 | `	ph7_result_value(pCtx, &sResult);` |
|       7 | 3380 | `	PH7_MemObjRelease(&sResult);` |
|       7 | 3381 | `	return PH7_OK;` |
|       7 | 3382 | `}` |
|       - | 3383 | `/*` |
|       - | 3384 | ` * Fiber->getReturn() — get the fiber's return value after it has terminated.` |
|       - | 3385 | ` */` |
|      84 | 3386 | `PH7_PRIVATE int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 3387 | `{` |
|      89 | 3388 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 3389 | `	ph7_exec_ctx *pExecCtx;` |
|      89 | 3390 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      42 | 3391 | `	SXUNUSED(apArg);` |
|      42 | 3392 | `	SXUNUSED(nArg);` |
|      89 | 3393 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|      89 | 3394 | `	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 3395 | `		ph7_result_null(pCtx);` |
|     ! 0 | 3396 | `		return PH7_OK;` |
|       - | 3397 | `	}` |
|      89 | 3398 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|      89 | 3399 | `	if( pExecCtx == 0 ){` |
|       - | 3400 | `		/* No context at all IS the never-started state -- the fiber's __ctx slot is` |
|       - | 3401 | `		 * filled by start(). php names it rather than answering null. */` |
|       3 | 3402 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 3403 | `			"Cannot get fiber return value: The fiber has not been started");` |
|       - | 3404 | `	}` |
|      87 | 3405 | `	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|       5 | 3406 | `		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){` |
|     ! 0 | 3407 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 3408 | `				"Cannot get fiber return value: The fiber has not been started");` |
|       - | 3409 | `		}` |
|       5 | 3410 | `		if( pExecCtx->bThrew ){` |
|       - | 3411 | `			/* Terminated, but with nothing to hand back: php's own third sentence. */` |
|       3 | 3412 | `			return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 3413 | `				"Cannot get fiber return value: The fiber threw an exception");` |
|       - | 3414 | `		}` |
|       3 | 3415 | `		return PH7_VmThrowException(pCtx, "FiberError",` |
|       - | 3416 | `			"Cannot get fiber return value: The fiber has not returned");` |
|       - | 3417 | `	}` |
|      83 | 3418 | `	ph7_result_value(pCtx, &pExecCtx->sRetValue);` |
|      83 | 3419 | `	return PH7_OK;` |
|      47 | 3420 | `}` |
|       - | 3421 | `/*` |
|       - | 3422 | ` * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()` |
|       - | 3423 | ` */` |
|      10 | 3424 | `PH7_PRIVATE int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       3 | 3425 | `{` |
|       - | 3426 | `	ph7_exec_ctx *pExecCtx;` |
|      13 | 3427 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       5 | 3428 | `	SXUNUSED(apArg);` |
|       5 | 3429 | `	SXUNUSED(nArg);` |
|      13 | 3430 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      13 | 3431 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|      13 | 3432 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);` |
|      13 | 3433 | `	return PH7_OK;` |
|       8 | 3434 | `}` |
|       2 | 3435 | `PH7_PRIVATE int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       1 | 3436 | `{` |
|       - | 3437 | `	ph7_exec_ctx *pExecCtx;` |
|       3 | 3438 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|       1 | 3439 | `	SXUNUSED(apArg);` |
|       1 | 3440 | `	SXUNUSED(nArg);` |
|       3 | 3441 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|       3 | 3442 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|       3 | 3443 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);` |
|       3 | 3444 | `	return PH7_OK;` |
|       2 | 3445 | `}` |
|     150 | 3446 | `PH7_PRIVATE int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 3447 | `{` |
|       - | 3448 | `	ph7_exec_ctx *pExecCtx;` |
|     155 | 3449 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      75 | 3450 | `	SXUNUSED(apArg);` |
|      75 | 3451 | `	SXUNUSED(nArg);` |
|     155 | 3452 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|     155 | 3453 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|     155 | 3454 | `	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|     155 | 3455 | `	return PH7_OK;` |
|      80 | 3456 | `}` |
|      32 | 3457 | `PH7_PRIVATE int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 3458 | `{` |
|       - | 3459 | `	ph7_exec_ctx *pExecCtx;` |
|      36 | 3460 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      16 | 3461 | `	SXUNUSED(apArg);` |
|      16 | 3462 | `	SXUNUSED(nArg);` |
|      36 | 3463 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|      36 | 3464 | `	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);` |
|       - | 3465 | `	/* php's DEAD state: a body that returned and a body that let an exception` |
|       - | 3466 | `	 * escape are both terminated -- only getReturn() tells them apart. */` |
|      55 | 3467 | `	ph7_result_bool(pCtx, pExecCtx && (pExecCtx->iState == PH7_CTX_STATE_COMPLETED` |
|      19 | 3468 | `		\|\| pExecCtx->iState == PH7_CTX_STATE_CLOSED));` |
|      36 | 3469 | `	return PH7_OK;` |
|      20 | 3470 | `}` |
|       - | 3471 | `/*` |
|       - | 3472 | ` * Fiber->__destruct() — clean up the execution context.` |
|       - | 3473 | ` */` |
|     360 | 3474 | `PH7_PRIVATE int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 3475 | `{` |
|     365 | 3476 | `	ph7_vm *pVm = pCtx->pVm;` |
|       - | 3477 | `	ph7_exec_ctx *pExecCtx;` |
|     365 | 3478 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     180 | 3479 | `	SXUNUSED(apArg);` |
|     180 | 3480 | `	SXUNUSED(nArg);` |
|     365 | 3481 | `	if( pRecv == 0 ){` |
|     ! 0 | 3482 | `		return PH7_OK;` |
|       - | 3483 | `	}` |
|     365 | 3484 | `	pExecCtx = VmFiberExtractCtx(pVm, pRecv);` |
|     365 | 3485 | `	if( pExecCtx ){` |
|     345 | 3486 | `		VmReleaseExecCtx(pVm, pExecCtx);` |
|       - | 3487 | `		/* Clear the attribute so double-free is prevented */` |
|     345 | 3488 | `		if( pRecv->iFlags & MEMOBJ_OBJ ){` |
|     345 | 3489 | `			ph7_class_instance *pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|       - | 3490 | `			SyString sAttrName;` |
|       - | 3491 | `			ph7_value *pAttr;` |
|     345 | 3492 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     345 | 3493 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     345 | 3494 | `			if( pAttr ){` |
|     345 | 3495 | `				PH7_MemObjRelease(pAttr);` |
|     170 | 3496 | `			}` |
|     170 | 3497 | `		}` |
|     170 | 3498 | `	}` |
|     365 | 3499 | `	return PH7_OK;` |
|     185 | 3500 | `}` |
|       - | 3501 | `/* ======================== Fiber Public API Helpers ======================== */` |
|     ! 0 | 3502 | `PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)` |
|     ! 0 | 3503 | `{` |
|       - | 3504 | `	ph7_class_instance *pThis;` |
|     ! 0 | 3505 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;` |
|     ! 0 | 3506 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     ! 0 | 3507 | `	return pThis->pClass == pVm->pFiberClass;` |
|     ! 0 | 3508 | `}` |
|     ! 0 | 3509 | `PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)` |
|     ! 0 | 3510 | `{` |
|       - | 3511 | `	ph7_class_instance *pThis;` |
|     ! 0 | 3512 | `	ph7_class_instance *pClosureThis = 0;` |
|       - | 3513 | `	ph7_exec_ctx *pCtx;` |
|       - | 3514 | `	ph7_vm_func *pFunc;` |
|       - | 3515 | `	ph7_value *pCallable;` |
|       - | 3516 | `	ph7_value *pCtxAttr;` |
|       - | 3517 | `	SyString sAttrName;` |
|       - | 3518 | `	sxi32 rc;` |
|       - | 3519 | `	/* Must not already be started */` |
|     ! 0 | 3520 | `	pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 3521 | `	if( pCtx != 0 ){` |
|     ! 0 | 3522 | `		return SXERR_INVALID;` |
|       - | 3523 | `	}` |
|     ! 0 | 3524 | `	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 3525 | `		return SXERR_INVALID;` |
|       - | 3526 | `	}` |
|     ! 0 | 3527 | `	pThis = (ph7_class_instance *)pFiber->x.pOther;` |
|       - | 3528 | `	/* Get the callable */` |
|     ! 0 | 3529 | `	SyStringInitFromBuf(&sAttrName, "__callable", 10);` |
|     ! 0 | 3530 | `	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     ! 0 | 3531 | `	if( pCallable == 0 ){` |
|     ! 0 | 3532 | `		return SXERR_INVALID;` |
|       - | 3533 | `	}` |
|       - | 3534 | `	/* Resolve callable, through the same body-finder the PHP-level start() uses --` |
|       - | 3535 | `	 * these were two copies of one decision, and only the other one grew php's array` |
|       - | 3536 | `	 * and "Class::method" shapes. An embedder has no context to throw through, so the` |
|       - | 3537 | `	 * reason comes back as this entry point's own status. */` |
|     ! 0 | 3538 | `	if( pCallable->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP) ){` |
|     ! 0 | 3539 | `		const char *zWhy = 0;` |
|     ! 0 | 3540 | `		pFunc = VmFiberCallableBody(pVm, pCallable, &pClosureThis, &zWhy);` |
|     ! 0 | 3541 | `		if( pFunc == 0 ){` |
|     ! 0 | 3542 | `			return SXERR_NOTFOUND;` |
|     ! 0 | 3543 | `		}` |
|     ! 0 | 3544 | `	}else if( pCallable->iFlags & MEMOBJ_OBJ ){` |
|     ! 0 | 3545 | `		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;` |
|     ! 0 | 3546 | `		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",` |
|       - | 3547 | `			sizeof("__invoke") - 1);` |
|     ! 0 | 3548 | `		if( pMethod == 0 ){` |
|     ! 0 | 3549 | `			return SXERR_INVALID;` |
|       - | 3550 | `		}` |
|     ! 0 | 3551 | `		pClosureThis = pClosure;` |
|     ! 0 | 3552 | `		pFunc = &pMethod->sFunc;` |
|     ! 0 | 3553 | `	}else{` |
|     ! 0 | 3554 | `		return SXERR_INVALID;` |
|       - | 3555 | `	}` |
|       - | 3556 | `	/* Create context */` |
|     ! 0 | 3557 | `	pCtx = VmNewExecCtx(pVm, pFunc);` |
|     ! 0 | 3558 | `	if( pCtx == 0 ){` |
|     ! 0 | 3559 | `		return SXERR_MEM;` |
|       - | 3560 | `	}` |
|       - | 3561 | `	/* Store in __ctx */` |
|     ! 0 | 3562 | `	SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     ! 0 | 3563 | `	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     ! 0 | 3564 | `	if( pCtxAttr ){` |
|     ! 0 | 3565 | `		pCtxAttr->x.pOther = pCtx;` |
|     ! 0 | 3566 | `		MemObjSetType(pCtxAttr, MEMOBJ_RES);` |
|     ! 0 | 3567 | `	}` |
|       - | 3568 | `	/* Set up frame with args */` |
|     ! 0 | 3569 | `	pCtx->pFrame->pParent = pVm->pFrame;` |
|     ! 0 | 3570 | `	pVm->pFrame = pCtx->pFrame;` |
|     ! 0 | 3571 | `	rc = VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg,` |
|       - | 3572 | `		0 /* weak-mode arg binding (embedder entry) */, 0,` |
|       - | 3573 | `		FALSE/*embedder entry: no userland call site*/,` |
|       - | 3574 | `		FALSE/*no source-level actuals to alias*/);` |
|     ! 0 | 3575 | `	pVm->pFrame = pCtx->pFrame->pParent;` |
|     ! 0 | 3576 | `	pCtx->pFrame->pParent = 0;` |
|     ! 0 | 3577 | `	if( rc != SXRET_OK ){` |
|     ! 0 | 3578 | `		return rc;` |
|       - | 3579 | `	}` |
|     ! 0 | 3580 | `	return VmStartCtx(pVm, pCtx, pResult);` |
|     ! 0 | 3581 | `}` |
|     ! 0 | 3582 | `PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)` |
|     ! 0 | 3583 | `{` |
|     ! 0 | 3584 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 3585 | `	if( pCtx == 0 ) return SXERR_INVALID;` |
|     ! 0 | 3586 | `	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);` |
|     ! 0 | 3587 | `}` |
|     ! 0 | 3588 | `PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 3589 | `{` |
|     ! 0 | 3590 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 3591 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;` |
|     ! 0 | 3592 | `}` |
|     ! 0 | 3593 | `PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 3594 | `{` |
|     ! 0 | 3595 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 3596 | `	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;` |
|     ! 0 | 3597 | `}` |
|     ! 0 | 3598 | `PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)` |
|     ! 0 | 3599 | `{` |
|     ! 0 | 3600 | `	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);` |
|     ! 0 | 3601 | `	if( pCtx == 0 \|\| pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;` |
|     ! 0 | 3602 | `	return &pCtx->sRetValue;` |
|     ! 0 | 3603 | `}` |
|       - | 3604 | `/* ======================== Generator Infrastructure ======================== */` |
|       - | 3605 | `/*` |
|       - | 3606 | ` * Allocate a new generator wrapper around an execution context.` |
|       - | 3607 | ` */` |
|     694 | 3608 | `PH7_PRIVATE ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)` |
|       5 | 3609 | `{` |
|       - | 3610 | `	ph7_generator *pGen;` |
|     699 | 3611 | `	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));` |
|     699 | 3612 | `	if( pGen == 0 ){` |
|     ! 0 | 3613 | `		return 0;` |
|       - | 3614 | `	}` |
|     699 | 3615 | `	SyZero(pGen, sizeof(ph7_generator));` |
|     699 | 3616 | `	pGen->pCtx = pCtx;` |
|     699 | 3617 | `	pGen->iImplicitKey = 0;` |
|     699 | 3618 | `	PH7_MemObjInit(pVm, &pGen->sYieldValue);` |
|     699 | 3619 | `	PH7_MemObjInit(pVm, &pGen->sYieldKey);` |
|       - | 3620 | `	/* Link the generator back to the exec context */` |
|     699 | 3621 | `	pCtx->pPrivate = pGen;` |
|     699 | 3622 | `	return pGen;` |
|     352 | 3623 | `}` |
|       - | 3624 | `/*` |
|       - | 3625 | ` * Release a generator and its execution context.` |
|       - | 3626 | ` */` |
|     692 | 3627 | `PH7_PRIVATE void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)` |
|       5 | 3628 | `{` |
|     697 | 3629 | `	if( pGen == 0 ){` |
|     ! 0 | 3630 | `		return;` |
|       - | 3631 | `	}` |
|     697 | 3632 | `	PH7_MemObjRelease(&pGen->sYieldValue);` |
|     697 | 3633 | `	PH7_MemObjRelease(&pGen->sYieldKey);` |
|     697 | 3634 | `	if( pGen->pCtx ){` |
|     697 | 3635 | `		pGen->pCtx->pPrivate = 0;` |
|     697 | 3636 | `		VmReleaseExecCtx(pVm, pGen->pCtx);` |
|     697 | 3637 | `		pGen->pCtx = 0;` |
|     346 | 3638 | `	}` |
|     697 | 3639 | `	SyMemBackendPoolFree(&pVm->sAllocator, pGen);` |
|     351 | 3640 | `}` |
|       - | 3641 | `/*` |
|       - | 3642 | ` * Extract ph7_generator from a Generator class instance.` |
|       - | 3643 | ` */` |
|    6206 | 3644 | `PH7_PRIVATE ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)` |
|       5 | 3645 | `{` |
|       - | 3646 | `	ph7_class_instance *pThis;` |
|       - | 3647 | `	SyString sAttr;` |
|       - | 3648 | `	ph7_value *pAttr;` |
|    6211 | 3649 | `	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 | 3650 | `		return 0;` |
|       - | 3651 | `	}` |
|    6211 | 3652 | `	pThis = (ph7_class_instance *)pGenObj->x.pOther;` |
|    6211 | 3653 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|     ! 0 | 3654 | `		return 0;` |
|       - | 3655 | `	}` |
|    6211 | 3656 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    6211 | 3657 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    6211 | 3658 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|     ! 0 | 3659 | `		return 0;` |
|       - | 3660 | `	}` |
|    6211 | 3661 | `	return (ph7_generator *)pAttr->x.pOther;` |
|    3108 | 3662 | `}` |
|       - | 3663 | `/*` |
|       - | 3664 | ` * php's zend_generator_ensure_initialized, which EVERY accessor on the class runs` |
|       - | 3665 | ` * first: a generator that has never executed is run to its first yield before it` |
|       - | 3666 | `` * is asked anything. `current()`/`key()`/`rewind()` did that here and the rest did`` |
|       - | 3667 | `` * not, so `valid()` answered FALSE for a generator sitting at its first yield —`` |
|       - | 3668 | `` * `while ($g->valid())` never entered the loop — and `next()`/`send()` did the`` |
|       - | 3669 | ` * PRIMING run and called it the advance, which delivers the first element twice` |
|       - | 3670 | `` * and drops what `send()` was given.`` |
|       - | 3671 | ` */` |
|    5470 | 3672 | `static sxi32 VmGeneratorEnsureInit(ph7_vm *pVm, ph7_generator *pGen)` |
|       5 | 3673 | `{` |
|       - | 3674 | `	sxi32 rc;` |
|    5475 | 3675 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_CREATED ){` |
|    4821 | 3676 | `		return PH7_OK;` |
|       - | 3677 | `	}` |
|     659 | 3678 | `	rc = VmStartCtx(pVm, pGen->pCtx, 0);` |
|     659 | 3679 | `	if( rc == PH7_OK ){` |
|       - | 3680 | `		/* php sets the flag on the INITIALIZING run itself, whatever it settled` |
|       - | 3681 | `		 * on — so a generator whose body never yields is still rewindable, and` |
|       - | 3682 | `		 * only a later resume takes the flag away. */` |
|     633 | 3683 | `		pGen->bAtFirstYield = 1;` |
|     314 | 3684 | `	}` |
|     659 | 3685 | `	return rc;` |
|    2740 | 3686 | `}` |
|       - | 3687 | `/*` |
|       - | 3688 | ` * Fetch the ph7_generator behind a Generator INSTANCE (the ph7_value-taking` |
|       - | 3689 | ` * VmGeneratorExtractCtx is the same lookup from the other side).` |
|       - | 3690 | ` */` |
|    1350 | 3691 | `static ph7_generator * VmGeneratorFromInstance(ph7_vm *pVm, ph7_class_instance *pThis)` |
|       5 | 3692 | `{` |
|       - | 3693 | `	SyString sAttr;` |
|       - | 3694 | `	ph7_value *pAttr;` |
|    1355 | 3695 | `	if( pThis == 0 \|\| pVm->pGeneratorClass == 0 \|\| pThis->pClass != pVm->pGeneratorClass ){` |
|     883 | 3696 | `		return 0;` |
|       - | 3697 | `	}` |
|     477 | 3698 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|     477 | 3699 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     477 | 3700 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|     ! 0 | 3701 | `		return 0;` |
|       - | 3702 | `	}` |
|     477 | 3703 | `	return (ph7_generator *)pAttr->x.pOther;` |
|     680 | 3704 | `}` |
|       - | 3705 | `/*` |
|       - | 3706 | ``  * Run a generator to its first yield if it has never executed. `yield from` `` |
|       - | 3707 | ` * needs this and NOT a rewind: php links the delegate as a child node and only` |
|       - | 3708 | ` * initializes it, so delegating to a generator that is already suspended` |
|       - | 3709 | ` * half-way CONTINUES from where it stands.` |
|       - | 3710 | ` */` |
|      46 | 3711 | `PH7_PRIVATE sxi32 PH7_VmGeneratorPrime(ph7_vm *pVm, ph7_class_instance *pThis)` |
|       5 | 3712 | `{` |
|      51 | 3713 | `	ph7_generator *pGen = VmGeneratorFromInstance(pVm, pThis);` |
|      51 | 3714 | `	if( pGen == 0 \|\| pGen->pCtx == 0 ){` |
|     ! 0 | 3715 | `		return PH7_OK;` |
|       - | 3716 | `	}` |
|      51 | 3717 | `	return VmGeneratorEnsureInit(pVm, pGen);` |
|      28 | 3718 | `}` |
|       - | 3719 | `/*` |
|       - | 3720 | ` * Whether a generator instance has already run to its end. php's` |
|       - | 3721 | ` * zend_generator_get_iterator refuses to start a foreach over one` |
|       - | 3722 | ` * ("Cannot traverse an already closed generator") BEFORE the rewind that would` |
|       - | 3723 | ` * otherwise report the coarser "already run" message.` |
|       - | 3724 | ` */` |
|    1304 | 3725 | `PH7_PRIVATE int PH7_VmGeneratorIsClosed(ph7_vm *pVm, ph7_class_instance *pThis)` |
|       5 | 3726 | `{` |
|    1309 | 3727 | `	ph7_generator *pGen = VmGeneratorFromInstance(pVm, pThis);` |
|    1309 | 3728 | `	if( pGen == 0 \|\| pGen->pCtx == 0 ){` |
|     883 | 3729 | `		return 0;` |
|       - | 3730 | `	}` |
|     640 | 3731 | `	return pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED` |
|     426 | 3732 | `		\|\| pGen->pCtx->iState == PH7_CTX_STATE_CLOSED;` |
|     657 | 3733 | `}` |
|       - | 3734 | `/*` |
|       - | 3735 | ` * Generator::rewind() — start if CREATED, no-op otherwise.` |
|       - | 3736 | ` */` |
|     408 | 3737 | `PH7_PRIVATE int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 3738 | `{` |
|       - | 3739 | `	ph7_generator *pGen;` |
|       - | 3740 | `	sxi32 rc;` |
|     413 | 3741 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     204 | 3742 | `	SXUNUSED(apArg);` |
|     204 | 3743 | `	SXUNUSED(nArg);` |
|     413 | 3744 | `	if( pRecv == 0 ) return PH7_OK;` |
|     413 | 3745 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     413 | 3746 | `	if( pGen == 0 ) return PH7_OK;` |
|     413 | 3747 | `	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|     413 | 3748 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     413 | 3749 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     393 | 3750 | `	if( !pGen->bAtFirstYield ){` |
|       - | 3751 | `		/* php: a generator is not rewindable, so rewind() is only allowed to mean` |
|       - | 3752 | `		 * "initialize". Once the body has moved past its first yield — or run to` |
|       - | 3753 | `		 * the end — php refuses, and PHL accepted in silence, so a foreach over a` |
|       - | 3754 | `		 * partly consumed generator carried on from where it stood while php` |
|       - | 3755 | `		 * stopped the program. */` |
|       5 | 3756 | `		return PH7_VmThrowException(pCtx, "Exception",` |
|       - | 3757 | `			"Cannot rewind a generator that was already run");` |
|       - | 3758 | `	}` |
|     389 | 3759 | `	return PH7_OK;` |
|     209 | 3760 | `}` |
|       - | 3761 | `/*` |
|       - | 3762 | ` * Generator::valid() — true if suspended at a yield point.` |
|       - | 3763 | ` */` |
|    1674 | 3764 | `PH7_PRIVATE int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 3765 | `{` |
|       - | 3766 | `	ph7_generator *pGen;` |
|    1679 | 3767 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     837 | 3768 | `	SXUNUSED(apArg);` |
|     837 | 3769 | `	SXUNUSED(nArg);` |
|    1679 | 3770 | `	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }` |
|    1679 | 3771 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|    1679 | 3772 | `	if( pGen ){` |
|    1679 | 3773 | `		sxi32 rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|    1679 | 3774 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|    1679 | 3775 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     837 | 3776 | `	}` |
|    1679 | 3777 | `	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);` |
|    1679 | 3778 | `	return PH7_OK;` |
|     842 | 3779 | `}` |
|       - | 3780 | `/*` |
|       - | 3781 | ` * Generator::current() — return the last yielded value.` |
|       - | 3782 | ` * Auto-starts the generator on first access (like PHP).` |
|       - | 3783 | ` */` |
|    1520 | 3784 | `PH7_PRIVATE int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 3785 | `{` |
|       - | 3786 | `	ph7_generator *pGen;` |
|       - | 3787 | `	sxi32 rc;` |
|    1525 | 3788 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     760 | 3789 | `	SXUNUSED(apArg);` |
|     760 | 3790 | `	SXUNUSED(nArg);` |
|    1525 | 3791 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    1525 | 3792 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|    1525 | 3793 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|    1525 | 3794 | `	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|    1525 | 3795 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|    1525 | 3796 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|    1525 | 3797 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|    1523 | 3798 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|     764 | 3799 | `	}else{` |
|       3 | 3800 | `		ph7_result_null(pCtx);` |
|       - | 3801 | `	}` |
|    1525 | 3802 | `	return PH7_OK;` |
|     765 | 3803 | `}` |
|       - | 3804 | `/*` |
|       - | 3805 | ` * Generator::key() — return the last yielded key.` |
|       - | 3806 | ` * Auto-starts the generator on first access (like PHP).` |
|       - | 3807 | ` */` |
|     400 | 3808 | `PH7_PRIVATE int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 3809 | `{` |
|       - | 3810 | `	ph7_generator *pGen;` |
|       - | 3811 | `	sxi32 rc;` |
|     405 | 3812 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     200 | 3813 | `	SXUNUSED(apArg);` |
|     200 | 3814 | `	SXUNUSED(nArg);` |
|     405 | 3815 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     405 | 3816 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     405 | 3817 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     405 | 3818 | `	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|     405 | 3819 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     405 | 3820 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     405 | 3821 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     405 | 3822 | `		ph7_result_value(pCtx, &pGen->sYieldKey);` |
|     205 | 3823 | `	}else{` |
|     ! 0 | 3824 | `		ph7_result_null(pCtx);` |
|       - | 3825 | `	}` |
|     405 | 3826 | `	return PH7_OK;` |
|     205 | 3827 | `}` |
|       - | 3828 | `/*` |
|       - | 3829 | ` * Generator::next() — advance to the next yield point.` |
|       - | 3830 | ` */` |
|    1192 | 3831 | `PH7_PRIVATE int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 3832 | `{` |
|       - | 3833 | `	ph7_generator *pGen;` |
|       - | 3834 | `	sxi32 rc;` |
|    1197 | 3835 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     596 | 3836 | `	SXUNUSED(apArg);` |
|     596 | 3837 | `	SXUNUSED(nArg);` |
|    1197 | 3838 | `	if( pRecv == 0 ) return PH7_OK;` |
|    1197 | 3839 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|    1197 | 3840 | `	if( pGen == 0 ) return PH7_OK;` |
|       - | 3841 | `	/* PRIMING is not the advance: php runs a never-executed body to its first` |
|       - | 3842 | ``	 * yield and then still resumes past it, so `$g->next()` on a fresh generator`` |
|       - | 3843 | `	 * lands on the SECOND element. Treating the start as the advance handed the` |
|       - | 3844 | `	 * first one out twice. */` |
|    1197 | 3845 | `	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|    1197 | 3846 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|    1197 | 3847 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|    1197 | 3848 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|     ! 0 | 3849 | `		return PH7_OK;` |
|       - | 3850 | `	}` |
|    1197 | 3851 | `	pGen->bAtFirstYield = 0;` |
|    1197 | 3852 | `	rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|    1197 | 3853 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|    1195 | 3854 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|    1185 | 3855 | `	return PH7_OK;` |
|     601 | 3856 | `}` |
|       - | 3857 | `/*` |
|       - | 3858 | ` * Generator::send($value) — resume and send a value into the generator.` |
|       - | 3859 | ` */` |
|     138 | 3860 | `PH7_PRIVATE int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 3861 | `{` |
|       - | 3862 | `	ph7_generator *pGen;` |
|       - | 3863 | `	ph7_value *pSendVal;` |
|       - | 3864 | `	sxi32 rc;` |
|     143 | 3865 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     143 | 3866 | `	if( pRecv == 0 ) return PH7_OK;` |
|     143 | 3867 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     143 | 3868 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|     143 | 3869 | `	pSendVal = (nArg > 0) ? apArg[0] : 0;` |
|       - | 3870 | `	/* php PRIMES a never-executed generator and THEN resumes it with the value, so` |
|       - | 3871 | ``	 * a first `send('S')` reaches the first `yield`'s left-hand side and answers the`` |
|       - | 3872 | `	 * SECOND yielded value. Stopping at the priming run dropped the value entirely` |
|       - | 3873 | `	 * and answered the first — the whole point of a coroutine's first send. */` |
|     143 | 3874 | `	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|     143 | 3875 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     143 | 3876 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     143 | 3877 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_SUSPENDED ){` |
|       3 | 3878 | `		ph7_result_null(pCtx);` |
|       3 | 3879 | `		return PH7_OK;` |
|       - | 3880 | `	}` |
|     141 | 3881 | `	pGen->bAtFirstYield = 0;` |
|     141 | 3882 | `	rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);` |
|     141 | 3883 | `	if( rc == PH7_ABORT ) return PH7_ABORT;` |
|     141 | 3884 | `	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     138 | 3885 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|     111 | 3886 | `		ph7_result_value(pCtx, &pGen->sYieldValue);` |
|      57 | 3887 | `	}else{` |
|      29 | 3888 | `		ph7_result_null(pCtx);` |
|       - | 3889 | `	}` |
|     138 | 3890 | `	return PH7_OK;` |
|      74 | 3891 | `}` |
|       - | 3892 | `/*` |
|       - | 3893 | ` * Generator::throw($exception) — throw an exception into the generator.` |
|       - | 3894 | ` *` |
|       - | 3895 | ` * PHP semantics: the exception is injected AT the suspended yield point so the` |
|       - | 3896 | ` * generator's OWN try/catch (if any wraps the yield) can handle it and the body` |
|       - | 3897 | ` * resumes after the try; otherwise it propagates to the throw() caller and the` |
|       - | 3898 | ` * generator closes. We implement this by resuming the body with a pending` |
|       - | 3899 | ` * injection (pCtx->pInjected) that the VM loop raises in the body's own frame via` |
|       - | 3900 | ` * the existing OP_THROW / ROOT B resume route — no exception frame is` |
|       - | 3901 | ` * reconstructed on pVm->pFrame, so the return/finally unwind path is untouched.` |
|       - | 3902 | ` * A never-started generator is first run to its first yield, then injected there;` |
|       - | 3903 | ` * a finished/closed generator has no suspend point, so the exception is simply` |
|       - | 3904 | ` * propagated to the caller (its return value stays readable via getReturn()).` |
|       - | 3905 | ` *` |
|       - | 3906 | ` * PHL does not enforce the Throwable parameter hint (interface/class hints are` |
|       - | 3907 | ` * not checked on call), so the Throwable validation — and its PHP TypeError —` |
|       - | 3908 | ` * are done here.` |
|       - | 3909 | ` */` |
|      62 | 3910 | `PH7_PRIVATE int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       4 | 3911 | `{` |
|       - | 3912 | `	ph7_generator *pGen;` |
|       - | 3913 | `	ph7_class_instance *pInj;` |
|       - | 3914 | `	ph7_class *pThrowable;` |
|       - | 3915 | `	VmFrame *pFrame;` |
|       - | 3916 | `	sxi32 rc;` |
|      66 | 3917 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      66 | 3918 | `	if( pRecv == 0 ){ return PH7_OK; }` |
|      66 | 3919 | `	if( nArg < 1 ) return PH7_OK;` |
|       - | 3920 | `	/* Argument #1 must be a Throwable; otherwise PHP raises a TypeError naming` |
|       - | 3921 | `	 * the given type (class name for objects, "null"/"string"/... for scalars). */` |
|      66 | 3922 | `	pThrowable = PH7_VmExtractClass(pCtx->pVm, "Throwable", sizeof("Throwable")-1, 0, 0);` |
|      62 | 3923 | `	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|      66 | 3924 | `	 \|\| (pThrowable && !PH7_VmInstanceOf(((ph7_class_instance *)apArg[0]->x.pOther)->pClass, pThrowable)) ){` |
|       - | 3925 | `		char zCls[128];` |
|     ! 0 | 3926 | `		const char *zGiven = VmValueGivenName(apArg[0], zCls, sizeof(zCls));` |
|     ! 0 | 3927 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|     ! 0 | 3928 | `			"Generator::throw(): Argument #1 ($exception) must be of type Throwable, %s given", zGiven);` |
|       - | 3929 | `	}` |
|      66 | 3930 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|      66 | 3931 | `	if( pGen == 0 ) return PH7_OK;` |
|       - | 3932 | `	/* PHP forbids resuming/throwing into a generator that is currently executing. */` |
|      66 | 3933 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_RUNNING ){` |
|       3 | 3934 | `		return PH7_VmThrowException(pCtx, "Error",` |
|       - | 3935 | `			"Cannot resume an already running generator");` |
|       - | 3936 | `	}` |
|       - | 3937 | `	/* Hold a reference to the injected instance for the whole operation: the VM loop` |
|       - | 3938 | `	 * (inject path) or VmThrowException (propagate path) may run catch blocks that bind` |
|       - | 3939 | `	 * and later release it. Dropped on every return path below. */` |
|      64 | 3940 | `	pInj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      64 | 3941 | `	pInj->iRef++;` |
|       - | 3942 | `	/* A never-started generator runs to its first yield, then the exception is injected` |
|       - | 3943 | `	 * there (PHP). Start it first; if it suspended at a yield, fall through to inject; if` |
|       - | 3944 | `	 * it ran to completion without yielding, drop through to the propagate path below. */` |
|       - | 3945 | `	{` |
|      64 | 3946 | `		rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|      64 | 3947 | `		if( rc == PH7_ABORT ){ PH7_ClassInstanceUnref(pInj); return PH7_ABORT; }` |
|      64 | 3948 | `		if( rc == PH7_EXCEPTION ){ PH7_ClassInstanceUnref(pInj); return PH7_EXCEPTION; }` |
|       - | 3949 | `	}` |
|      64 | 3950 | `	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|       - | 3951 | `		/* Inject at the suspended yield: the resume loop raises it in the body's own` |
|       - | 3952 | `		 * frame so the generator's try/catch can catch it and resume (path 2). */` |
|      58 | 3953 | `		pGen->pCtx->pInjected = pInj;   /* borrowed; ref held here across the resume */` |
|      58 | 3954 | `		pGen->bAtFirstYield = 0;` |
|      58 | 3955 | `		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);` |
|       - | 3956 | `		/* Normally the inject was consumed (cleared) at VmByteCodeExec entry; clear it` |
|       - | 3957 | `		 * here too for the path where VmResumeCtx bails BEFORE entering the loop (e.g. the` |
|       - | 3958 | `		 * recursion-depth fatal), so no dangling borrowed pointer survives the Unref. */` |
|      58 | 3959 | `		pGen->pCtx->pInjected = 0;` |
|      58 | 3960 | `		PH7_ClassInstanceUnref(pInj);` |
|      58 | 3961 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      58 | 3962 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       - | 3963 | `		/* Caught inside the generator and it resumed: return the next yielded value (or` |
|       - | 3964 | `		 * null if it then completed) — symmetric with Generator::send(). */` |
|      46 | 3965 | `		if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){` |
|      43 | 3966 | `			ph7_result_value(pCtx, &pGen->sYieldValue);` |
|      23 | 3967 | `		}else{` |
|       3 | 3968 | `			ph7_result_null(pCtx);` |
|       - | 3969 | `		}` |
|      46 | 3970 | `		return PH7_OK;` |
|       - | 3971 | `	}` |
|       - | 3972 | `	/* COMPLETED/CLOSED (incl. a CREATED generator that ran to completion without a` |
|       - | 3973 | `	 * yield): no suspend point to inject at. Propagate the real object to the throw()` |
|       - | 3974 | `	 * caller through the normal dispatch path (class/message/trace preserved); its` |
|       - | 3975 | `	 * terminal state — and thus getReturn() — is left intact. */` |
|       8 | 3976 | `	pFrame = pCtx->pVm->pFrame;` |
|       8 | 3977 | `	if( pFrame ){` |
|       8 | 3978 | `		pFrame = VmSkipExceptionFrames(pFrame);` |
|       8 | 3979 | `		pFrame->iFlags \|= VM_FRAME_THROW;` |
|       3 | 3980 | `	}` |
|       8 | 3981 | `	rc = VmThrowException(pCtx->pVm, pInj);` |
|       8 | 3982 | `	PH7_ClassInstanceUnref(pInj);` |
|       8 | 3983 | `	if( rc == SXERR_ABORT ){` |
|     ! 0 | 3984 | `		return PH7_ABORT;` |
|       - | 3985 | `	}` |
|       8 | 3986 | `	return PH7_EXCEPTION;` |
|      35 | 3987 | `}` |
|       - | 3988 | `/*` |
|       - | 3989 | ` * Generator::getReturn() — get the return value after the generator has finished.` |
|       - | 3990 | ` */` |
|      32 | 3991 | `PH7_PRIVATE int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 3992 | `{` |
|       - | 3993 | `	ph7_generator *pGen;` |
|      37 | 3994 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|      16 | 3995 | `	SXUNUSED(apArg);` |
|      16 | 3996 | `	SXUNUSED(nArg);` |
|      37 | 3997 | `	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|      37 | 3998 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|      37 | 3999 | `	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }` |
|       - | 4000 | `	{` |
|       - | 4001 | `		/* php initializes here too, so a generator asked for its return value` |
|       - | 4002 | `		 * before anything else has RUN its body up to the first yield — the side` |
|       - | 4003 | `		 * effects before that yield happen either way. */` |
|      37 | 4004 | `		sxi32 rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);` |
|      37 | 4005 | `		if( rc == PH7_ABORT ) return PH7_ABORT;` |
|      37 | 4006 | `		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|       - | 4007 | `	}` |
|      37 | 4008 | `	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){` |
|       - | 4009 | `		/* php's class here is Exception, not Error. */` |
|       3 | 4010 | `		return PH7_VmThrowException(pCtx, "Exception",` |
|       - | 4011 | `			"Cannot get return value of a generator that hasn't returned");` |
|       - | 4012 | `	}` |
|      35 | 4013 | `	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);` |
|      35 | 4014 | `	return PH7_OK;` |
|      21 | 4015 | `}` |
|       - | 4016 | `/*` |
|       - | 4017 | ` * Generator::__destruct() — clean up.` |
|       - | 4018 | ` */` |
|     674 | 4019 | `PH7_PRIVATE int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|       5 | 4020 | `{` |
|       - | 4021 | `	ph7_generator *pGen;` |
|     679 | 4022 | `	sxi32 rcClose = SXRET_OK;` |
|     679 | 4023 | `	ph7_value *pRecv = PH7_ContextThisValue(pCtx);` |
|     337 | 4024 | `	SXUNUSED(apArg);` |
|     337 | 4025 | `	SXUNUSED(nArg);` |
|     679 | 4026 | `	if( pRecv == 0 ) return PH7_OK;` |
|     679 | 4027 | `	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);` |
|     679 | 4028 | `	if( pGen ){` |
|       - | 4029 | `` 		/* A generator abandoned before it completes still runs its pending `finally` `` |
|       - | 4030 | `		 * blocks (PHP runs them at generator close/GC). Drive them before teardown. */` |
|     679 | 4031 | `		if( pGen->pCtx ){` |
|     679 | 4032 | `			rcClose = VmCloseCtx(pCtx->pVm, pGen->pCtx);` |
|     337 | 4033 | `		}` |
|     679 | 4034 | `		VmReleaseGenerator(pCtx->pVm, pGen);` |
|     679 | 4035 | `		if( pRecv->iFlags & MEMOBJ_OBJ ){` |
|     679 | 4036 | `			ph7_class_instance *pThis = (ph7_class_instance *)pRecv->x.pOther;` |
|       - | 4037 | `			SyString sAttrName;` |
|       - | 4038 | `			ph7_value *pAttr;` |
|     679 | 4039 | `			SyStringInitFromBuf(&sAttrName, "__ctx", 5);` |
|     679 | 4040 | `			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);` |
|     679 | 4041 | `			if( pAttr ){` |
|     679 | 4042 | `				PH7_MemObjRelease(pAttr);` |
|     337 | 4043 | `			}` |
|     337 | 4044 | `		}` |
|     337 | 4045 | `	}` |
|       - | 4046 | `	/* Surface an abort/exception raised by a finally that ran during close. */` |
|     679 | 4047 | `	if( rcClose == PH7_ABORT ) return PH7_ABORT;` |
|     679 | 4048 | `	if( rcClose == PH7_EXCEPTION ) return PH7_EXCEPTION;` |
|     679 | 4049 | `	return PH7_OK;` |
|     342 | 4050 | `}` |
|       - | 4051 | `/* ======================== End Generator Infrastructure ======================== */` |
|       - | 4052 | `/* ======================== End Fiber Infrastructure ======================== */` |
|       - | 4053 |  |
