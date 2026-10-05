/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
/* Darwin declares the ucontext routines only when _XOPEN_SOURCE is defined
 * before the first system header; _DARWIN_C_SOURCE keeps the rest of the libc
 * visible, which _XOPEN_SOURCE alone would hide. */
#if defined(__APPLE__) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 600
#define _DARWIN_C_SOURCE 1
#endif
#include "ph7int.h"
/*
 * Section:
 *    Execution contexts: the parked-stack machinery shared by Fibers and
 *    Generators, closure creation/binding (Closure_* builtins), the
 *    Fiber_* and Generator_* builtins and the PH7_VmFiber* API.
 *    Registration happens via vm.c (aVmFunc[]/class installs).
 * Status:
 *    Stable.
 */
/*
 * ---------------------------------------------------------------------------
 * Real coroutine stacks: a fiber body on a native stack of its own.
 * ---------------------------------------------------------------------------
 * See the PH7_CORO_STACK block in ph7int.h for WHY. This is the machinery:
 *
 *   VmCoroNew / VmCoroFree        one switchable native stack
 *   VmCoroEnter / VmCoroLeave     the switch itself, in both directions
 *   VmCoroStateInit / Swap        the VM state that belongs to whichever runs
 *   VmCoroRun                     the resumer's side of a start / resume / kill
 *   VmCoroBody                    the fiber's side, entered once
 *
 * All of it compiles out where no stack-switch primitive exists, and a fiber
 * that does not get a stack (an allocation failure; a generator, which never
 * asks for one) falls back to the record-parking path. So the two models
 * coexist and `pCtx->pCoro != 0` is the discriminator every caller tests.
 */
#ifdef PH7_CORO_STACK
#ifdef PH7_CORO_UCONTEXT
#include <ucontext.h>
#if defined(__APPLE__) && defined(__clang__)
/* ...and marks every one of them deprecated, which -Werror turns into a
 * failed build. */
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
#endif
#if defined(PH7_CORO_UCONTEXT) || defined(PH7_CORO_ASM_X64)
#include <sys/mman.h>
#include <unistd.h>
#endif
#ifdef PH7_CORO_WIN32
#include <windows.h>
#endif
/* ASan keeps its own idea of where the stack is, and moves locals whose address
 * escapes onto a "fake stack" it tracks per stack. A switch it is not told
 * about makes it read the other side's frames as use-after-return, so every
 * switch is announced. The protocol is the documented one: start_switch_fiber
 * names where we are GOING and hands back a token; finish_switch_fiber runs on
 * ARRIVAL with the token of the switch that got us there -- which, for a switch
 * that eventually comes back, is a local of the function that made it. */
#if defined(__has_feature)
# if __has_feature(address_sanitizer)
#  define PH7_CORO_ASAN 1
# endif
#endif
#if defined(__SANITIZE_ADDRESS__) && !defined(PH7_CORO_ASAN)
# define PH7_CORO_ASAN 1
#endif
#ifdef PH7_CORO_ASAN
#include <sanitizer/common_interface_defs.h>
#endif
/*
 * How big a fiber's native stack is, and why it is not a constant.
 *
 * A fiber's stack holds what the trampoline does NOT flatten -- eval/include
 * towers, C->PHP callbacks, nested coroutine starts -- which is exactly what
 * nMaxNativeDepth counts, and a fiber's depth counter starts at zero on its own
 * stack. So the stack has to be big enough that the ENGINE's clean fatal
 * ("Maximum native nesting depth reached") always fires before the guard page
 * does; otherwise a deep enough callback tower inside a fiber is a SIGSEGV
 * where the same tower outside one is a diagnostic.
 *
 * Hence: bytes per allowed level, times the cap. The per-level figure was
 * measured on the fattest thing in the tree -- a self-recursive `array_map`
 * callback, which spends a builtin frame, a native VmByteCodeExec activation
 * and an operand stack on every level: ~5.7 KB at -O3 and ~14 KB under
 * ASan+UBSan at -O1 -g. 32 KB carries better than a 2x margin over the worse of
 * the two. At the host default cap of 256 that is 8 MB of ADDRESS SPACE per live
 * fiber; both platforms map it lazily, so an idle fiber's resident cost is the
 * page it is parked on.
 */
#ifndef PH7_CORO_STACK_PER_LEVEL
#define PH7_CORO_STACK_PER_LEVEL (32 * 1024)
#endif
#ifndef PH7_CORO_STACK_MIN
#define PH7_CORO_STACK_MIN (256 * 1024)
#endif
#ifndef PH7_CORO_STACK_MAX
#define PH7_CORO_STACK_MAX (32 * 1024 * 1024)
#endif
static sxu32 VmCoroStackBytes(ph7_vm *pVm)
{
	sxi64 nWant = (sxi64)(pVm->nMaxNativeDepth > 0 ? pVm->nMaxNativeDepth : 256)
		* (sxi64)PH7_CORO_STACK_PER_LEVEL;
	if( nWant < PH7_CORO_STACK_MIN ){
		nWant = PH7_CORO_STACK_MIN;
	}
	if( nWant > PH7_CORO_STACK_MAX ){
		nWant = PH7_CORO_STACK_MAX;
	}
	return (sxu32)nWant;
}
struct VmCoro
{
#ifdef PH7_CORO_UCONTEXT
	ucontext_t sBack;       /* the resumer's context: where a switch-out goes */
	ucontext_t sSelf;       /* the fiber's own */
#endif
#ifdef PH7_CORO_ASM_X64
	void *pBackSp;          /* the resumer's saved stack pointer (switch-out target) */
	void *pSelfSp;          /* the fiber's own */
#endif
#if defined(PH7_CORO_UCONTEXT) || defined(PH7_CORO_ASM_X64)
	void *pMap;             /* mmap base: one guard page, then the stack */
	sxu32 nMap;             /* its length, for munmap */
#endif
#ifdef PH7_CORO_WIN32
	void *pFiber;           /* CreateFiber handle */
	void *pBack;            /* the resumer's fiber handle, valid while we run */
#endif
	void *pStack;           /* usable stack, low address (0 where the platform owns it) */
	sxu32 nStack;           /* usable stack bytes */
#ifdef PH7_CORO_ASAN
	const void *pHostStack; /* where the resumer's stack is: learned on arrival, */
	sxu32 nHostStack;       /* and needed to announce the switch back to it */
#endif
};
#ifdef PH7_CORO_ASAN
/*
 * Arriving on a stack. pTok closes the switch that brought us here (0 when the
 * arrival is a fiber's FIRST entry: the enterer keeps its own token for its own
 * return). The out-params say where we came from, which is the only way the
 * fiber can learn the resumer's stack bounds to announce the switch back.
 */
static void VmCoroAsanArrive(VmCoro *pCoro, void *pTok)
{
	const void *pFrom = 0;
	size_t nFrom = 0;
	__sanitizer_finish_switch_fiber(pTok, &pFrom, &nFrom);
	if( pCoro && pFrom ){
		pCoro->pHostStack = pFrom;
		pCoro->nHostStack = (sxu32)nFrom;
	}
}
#define VM_CORO_ASAN_TOKEN            void *pTok = 0
#define VM_CORO_ASAN_GO(TOK,BOT,SIZ)  __sanitizer_start_switch_fiber((TOK),(BOT),(size_t)(SIZ))
#else
#define VmCoroAsanArrive(C,T)         ((void)0)
#define VM_CORO_ASAN_TOKEN            int iUnusedTok = 0; (void)iUnusedTok
#define VM_CORO_ASAN_GO(TOK,BOT,SIZ)  ((void)0)
#endif
static void VmCoroBody(ph7_exec_ctx *pCtx);
static ph7_vm_func * VmCtxEnforceRetFunc(ph7_exec_ctx *pCtx); /* defined below, with the rest of the shared coroutine epilogue */
#ifdef PH7_CORO_ASM_X64
/*
 * The written switch, x86-64 System V.
 *
 * PH7_CoroSwitch(void **ppSave, void *pTarget) pushes the six callee-saved
 * registers and the two floating-point control words, writes the resulting
 * stack pointer through ppSave, adopts pTarget as the stack, and pops the
 * mirror image -- so it "returns" wherever that other stack's saved frame says.
 * That is the whole of a coroutine switch: everything else a C frame owns is
 * already on the stack it lives on.
 *
 * PH7_CoroEntryStub is the address a FRESH stack's frame returns to. The seeded
 * frame carries the ph7_exec_ctx in the r12 slot (a callee-saved register is
 * the only place a value can ride through the switch), so the stub moves it
 * into the first argument register and calls into C, which never comes back.
 *
 * `endbr64` on both so an IBT-enforcing loader is satisfied. A CET SHADOW stack
 * is the case this cannot serve -- a return to a frame the shadow stack never
 * saw -- and PH7_DISABLE_CORO_ASM is the way back to glibc's swapcontext, which
 * knows about it.
 */
extern void PH7_CoroSwitch(void **ppSave, void *pTarget);
extern void PH7_CoroEntryStub(void);
PH7_PRIVATE void PH7_CoroEntryC(ph7_exec_ctx *pCtx); /* named by the stub, so not static */
__asm__(
	".text\n"
	".globl PH7_CoroSwitch\n"
	".hidden PH7_CoroSwitch\n"
	".type PH7_CoroSwitch,@function\n"
	".align 16\n"
	"PH7_CoroSwitch:\n"
	"	endbr64\n"
	"	pushq %rbp\n"
	"	pushq %rbx\n"
	"	pushq %r15\n"
	"	pushq %r14\n"
	"	pushq %r13\n"
	"	pushq %r12\n"
	"	subq $8, %rsp\n"
	"	stmxcsr (%rsp)\n"
	"	fnstcw 4(%rsp)\n"
	"	movq %rsp, (%rdi)\n"
	"	movq %rsi, %rsp\n"
	"	ldmxcsr (%rsp)\n"
	"	fldcw 4(%rsp)\n"
	"	addq $8, %rsp\n"
	"	popq %r12\n"
	"	popq %r13\n"
	"	popq %r14\n"
	"	popq %r15\n"
	"	popq %rbx\n"
	"	popq %rbp\n"
	"	ret\n"
	".size PH7_CoroSwitch,.-PH7_CoroSwitch\n"
	".globl PH7_CoroEntryStub\n"
	".hidden PH7_CoroEntryStub\n"
	".type PH7_CoroEntryStub,@function\n"
	".align 16\n"
	"PH7_CoroEntryStub:\n"
	"	endbr64\n"
	"	movq %r12, %rdi\n"
	"	andq $-16, %rsp\n"
	"	call PH7_CoroEntryC\n"
	"	hlt\n"
	".size PH7_CoroEntryStub,.-PH7_CoroEntryStub\n"
);
PH7_PRIVATE void PH7_CoroEntryC(ph7_exec_ctx *pCtx)
{
	VmCoroBody(pCtx);
}
/* Where the seeded frame's eight words sit, counted from the fiber's saved sp:
 * the control-word pair, then r12 (which carries the ctx), r13, r14, r15, rbx,
 * rbp, and finally the address PH7_CoroSwitch's `ret` will take. */
#define VM_CORO_X64_FRAME_WORDS  8
#define VM_CORO_X64_SLOT_CTL     0
#define VM_CORO_X64_SLOT_R12     1
#define VM_CORO_X64_SLOT_RET     7
#endif /* PH7_CORO_ASM_X64 */
#ifdef PH7_CORO_UCONTEXT
/*
 * makecontext() passes int arguments only, so the context pointer travels as
 * two of them -- the portable idiom, and the reason this is not a plain
 * one-argument entry. A VM-wide "the fiber about to boot" field would be
 * shorter and is not thread-safe, which PH7_ENABLE_THREADS builds care about.
 */
static void VmCoroUcEntry(unsigned int iHi, unsigned int iLo)
{
	sxu64 uPtr = (((sxu64)iHi) << 32) | (sxu64)iLo;
	VmCoroBody((ph7_exec_ctx *)(size_t)uPtr);
}
#endif
#ifdef PH7_CORO_WIN32
static VOID CALLBACK VmCoroWinEntry(PVOID pArg)
{
	VmCoroBody((ph7_exec_ctx *)pArg);
}
#endif
/*
 * Allocate one switchable stack. NULL is not fatal: the caller runs the body on
 * the shared stack instead, where a suspend across a C boundary keeps raising
 * the FiberError it always raised.
 */
static VmCoro * VmCoroNew(ph7_vm *pVm, ph7_exec_ctx *pCtx)
{
	VmCoro *pCoro = (VmCoro *)SyMemBackendAlloc(&pVm->sAllocator, sizeof(VmCoro));
	if( pCoro == 0 ){
		return 0;
	}
	SyZero(pCoro, sizeof(VmCoro));
#if defined(PH7_CORO_UCONTEXT) || defined(PH7_CORO_ASM_X64)
	{
		long nPage = sysconf(_SC_PAGESIZE);
		sxu32 nGuard = (nPage > 0) ? (sxu32)nPage : 4096;
		sxu32 nStack = VmCoroStackBytes(pVm);
		void *pMap;
		/* One PROT_NONE page below the stack turns an overflow into a clean fault
		 * at the guard rather than a silent write into whatever the allocator put
		 * next door. nMaxNativeDepth is the engine's own net; this is the floor
		 * under it. */
		pMap = mmap(0, (size_t)nGuard + (size_t)nStack, PROT_READ | PROT_WRITE,
			MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if( pMap == MAP_FAILED ){
			SyMemBackendFree(&pVm->sAllocator, pCoro);
			return 0;
		}
		(void)mprotect(pMap, (size_t)nGuard, PROT_NONE);
		pCoro->pMap = pMap;
		pCoro->nMap = nGuard + nStack;
		pCoro->pStack = (void *)((char *)pMap + nGuard);
		pCoro->nStack = nStack;
	}
#endif
#ifdef PH7_CORO_ASM_X64
	{
		/* Seed the frame PH7_CoroSwitch will pop the first time this stack is
		 * entered: the ctx in the r12 slot, the entry stub as the return address,
		 * and THIS thread's floating-point control words so the fiber starts with
		 * the rounding/precision modes its creator had. */
		void **aTop = (void **)(((size_t)pCoro->pStack + pCoro->nStack) & ~(size_t)15);
		void **aFrame = aTop - VM_CORO_X64_FRAME_WORDS;
		unsigned int nMxcsr;
		unsigned short nFcw;
		sxu32 i;
		__asm__ __volatile__("stmxcsr %0" : "=m"(nMxcsr));
		__asm__ __volatile__("fnstcw %0" : "=m"(nFcw));
		for( i = 0; i < VM_CORO_X64_FRAME_WORDS; i++ ){
			aFrame[i] = 0;
		}
		SyMemcpy((const void *)&nMxcsr, (void *)&aFrame[VM_CORO_X64_SLOT_CTL], sizeof(nMxcsr));
		SyMemcpy((const void *)&nFcw,
			(void *)((char *)&aFrame[VM_CORO_X64_SLOT_CTL] + 4), sizeof(nFcw));
		aFrame[VM_CORO_X64_SLOT_R12] = (void *)pCtx;
		aFrame[VM_CORO_X64_SLOT_RET] = (void *)PH7_CoroEntryStub;
		pCoro->pSelfSp = (void *)aFrame;
	}
#endif
#ifdef PH7_CORO_UCONTEXT
	{
		sxu64 uPtr;
		if( getcontext(&pCoro->sSelf) != 0 ){
			munmap(pCoro->pMap, (size_t)pCoro->nMap);
			SyMemBackendFree(&pVm->sAllocator, pCoro);
			return 0;
		}
		pCoro->sSelf.uc_stack.ss_sp = pCoro->pStack;
		pCoro->sSelf.uc_stack.ss_size = (size_t)pCoro->nStack;
		/* uc_link stays NULL on purpose: the body never falls off its entry, it
		 * makes the last switch itself (VmCoroBody), so that arrival is announced
		 * to ASan like every other one. */
		pCoro->sSelf.uc_link = 0;
		uPtr = (sxu64)(size_t)pCtx;
		makecontext(&pCoro->sSelf, (void (*)(void))VmCoroUcEntry, 2,
			(unsigned int)(uPtr >> 32), (unsigned int)(uPtr & 0xFFFFFFFFu));
	}
#endif
#ifdef PH7_CORO_WIN32
	/* CreateFiber wants the calling THREAD to be a fiber before anything can be
	 * switched to. That conversion is per-thread and is not undone: reversing it
	 * is only safe with no fiber left alive anywhere, which one Fiber object
	 * cannot know, and it costs an unconverted thread a few dozen bytes. */
	if( !IsThreadAFiber() ){
		if( ConvertThreadToFiber(0) == 0 ){
			SyMemBackendFree(&pVm->sAllocator, pCoro);
			return 0;
		}
	}
	pCoro->nStack = VmCoroStackBytes(pVm);
	/* RESERVE the whole thing, COMMIT nothing (the 0): Windows grows a fiber
	 * stack on demand exactly like a thread's, so the size above is address
	 * space. FIBER_FLAG_FLOAT_SWITCH is what makes the switch carry the
	 * floating-point state, which the written x86-64 switch does by hand. */
	pCoro->pFiber = (void *)CreateFiberEx((SIZE_T)0, (SIZE_T)pCoro->nStack,
		FIBER_FLAG_FLOAT_SWITCH, VmCoroWinEntry, (LPVOID)pCtx);
	if( pCoro->pFiber == 0 ){
		SyMemBackendFree(&pVm->sAllocator, pCoro);
		return 0;
	}
	/* Win32 owns the mapping, so there is no bottom address to hand out -- and
	 * the MSVC build has no ASan fiber annotations to hand it to. */
	pCoro->pStack = 0;
#endif
	return pCoro;
}
/*
 * Give the stack back. Only ever reached with the fiber not running: its body
 * ran off the end, or the teardown unwound it first (see the kill switch).
 */
static void VmCoroFree(ph7_vm *pVm, VmCoro *pCoro)
{
	if( pCoro == 0 ){
		return;
	}
#if defined(PH7_CORO_UCONTEXT) || defined(PH7_CORO_ASM_X64)
	if( pCoro->pMap ){
		munmap(pCoro->pMap, (size_t)pCoro->nMap);
	}
#endif
#ifdef PH7_CORO_WIN32
	if( pCoro->pFiber ){
		DeleteFiber((LPVOID)pCoro->pFiber);
	}
#endif
	SyMemBackendFree(&pVm->sAllocator, pCoro);
}
/*
 * Switch onto the fiber's stack. Returns when the fiber switches back, because
 * it suspended or because its body finished.
 */
static void VmCoroEnter(VmCoro *pCoro)
{
	VM_CORO_ASAN_TOKEN;
	VM_CORO_ASAN_GO(&pTok, pCoro->pStack, pCoro->nStack);
#if defined(PH7_CORO_WIN32)
	pCoro->pBack = GetCurrentFiber();
	SwitchToFiber((LPVOID)pCoro->pFiber);
#elif defined(PH7_CORO_ASM_X64)
	PH7_CoroSwitch(&pCoro->pBackSp, pCoro->pSelfSp);
#else
	swapcontext(&pCoro->sBack, &pCoro->sSelf);
#endif
	VmCoroAsanArrive(0, pTok);
}
/*
 * ...and back, from inside the fiber. Returns when the fiber is entered again.
 */
static void VmCoroLeave(VmCoro *pCoro)
{
	VM_CORO_ASAN_TOKEN;
	VM_CORO_ASAN_GO(&pTok, pCoro->pHostStack, pCoro->nHostStack);
#if defined(PH7_CORO_WIN32)
	SwitchToFiber((LPVOID)pCoro->pBack);
#elif defined(PH7_CORO_ASM_X64)
	PH7_CoroSwitch(&pCoro->pSelfSp, pCoro->pBackSp);
#else
	swapcontext(&pCoro->sSelf, &pCoro->sBack);
#endif
	VmCoroAsanArrive(pCoro, pTok);
}
/*
 * The last switch a fiber makes: the body is over, so the stack is spent and
 * ASan is told to DISCARD this side's state rather than save it (the null
 * token). Never returns.
 */
static void VmCoroLeaveFinal(VmCoro *pCoro)
{
	VM_CORO_ASAN_GO(0, pCoro->pHostStack, pCoro->nHostStack);
#if defined(PH7_CORO_WIN32)
	SwitchToFiber((LPVOID)pCoro->pBack);
#elif defined(PH7_CORO_ASM_X64)
	{
		/* Nothing on this stack will ever be resumed, so the save slot is a
		 * scratch word rather than pSelfSp -- writing that would leave a live
		 * frame pointer on a dead stack. */
		void *pDead = 0;
		PH7_CoroSwitch(&pDead, pCoro->pBackSp);
	}
#else
	setcontext(&pCoro->sBack);
#endif
}
/*
 * The VM state that belongs to whichever side is running (see VmCoroVmState).
 * One list, used three ways, so a field can never be saved and not restored.
 */
#define VM_CORO_STATE_FIELDS(_) \
	_(aException) _(aFinallyAction) _(aSelf) \
	_(nVmExecDepth) _(nRecursionDepth) _(nCurLine) _(nBoundaryRc) \
	_(pCalleeName) _(pNativeFrameName) _(bHostDiscard) _(nErrSuppress) \
	_(nExceptDepth) _(nExcCtorDepth) _(nMuteThrow) _(nSpeculative) \
	_(nConstEvalDepth) _(nLazyInitLine) _(nLazyInitDepth) \
	_(nObDepth) _(nObActive) _(pObFrame) _(pCoroCtx)
/*
 * Seed the fiber side for a body that has not run yet: three empty stacks of
 * its own, a C stack nothing is live on, and every other scalar inherited from
 * the site that is starting it -- which is what running the body inline used to
 * give it.
 */
static void VmCoroStateInit(ph7_vm *pVm, ph7_exec_ctx *pCtx)
{
	VmCoroVmState *pS = &pCtx->sSaved;
#define VM_CORO_INHERIT(F)  pS->F = pVm->F;
	VM_CORO_STATE_FIELDS(VM_CORO_INHERIT)
#undef VM_CORO_INHERIT
	SySetInit(&pS->aException, &pVm->sAllocator, sizeof(ph7_exception *));
	SySetInit(&pS->aFinallyAction, &pVm->sAllocator, sizeof(VmFinallyAction));
	SySetInit(&pS->aSelf, &pVm->sAllocator, sizeof(ph7_class *));
	/* A fresh C stack: no native activation is live on it and no PHP call is
	 * open, so both guards start from zero and measure THIS stack. */
	pS->nVmExecDepth = 0;
	pS->nRecursionDepth = 0;
	/* Nothing of the resumer's in-flight C state is the fiber's: the parked
	 * boundary throw belongs to the interrupted exec, the two callee-name latches
	 * are consumed by the next call the RESUMER makes, and the lazy-initializer
	 * line override is keyed on the other stack's native depth. */
	pS->nBoundaryRc = 0;
	pS->pCalleeName = 0;
	pS->pNativeFrameName = 0;
	pS->bHostDiscard = 0;
	pS->nLazyInitLine = 0;
	pS->nLazyInitDepth = 0;
	/* ...and this side IS the fiber. */
	pS->pCoroCtx = pCtx;
}
/*
 * Release what the fiber side owns. Its three stacks die with the body: at a
 * clean end they are empty, but a body that ABORTED can still be holding an
 * unconsumed finally action (a queued return's value, a rethrow's exception
 * reference) and the per-activation exception clones stage 2b made.
 */
static void VmCoroStateRelease(ph7_vm *pVm, VmCoroVmState *pS)
{
	sxu32 n = SySetUsed(&pS->aFinallyAction);
	if( n > 0 ){
		VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pS->aFinallyAction);
		sxu32 i;
		for( i = 0; i < n; i++ ){
			if( aA[i].eKind == PH7_FA_RETURN ){
				PH7_MemObjRelease(&aA[i].sRet);
			}else if( aA[i].eKind == PH7_FA_RETHROW && aA[i].pExc ){
				PH7_ClassInstanceUnref(aA[i].pExc);
			}
		}
	}
	SySetRelease(&pS->aFinallyAction);
	VmExcReleaseAll(pVm, &pS->aException);
	SySetRelease(&pS->aException);
	SySetRelease(&pS->aSelf);  /* borrowed class pointers */
	SyZero(pS, sizeof(*pS));
}
/*
 * Swap sides: what the VM holds now goes to *pOut, what *pIn holds becomes
 * live. Called only from the RESUMER's stack, on both sides of the switch, so
 * the fiber never has to know it is being saved.
 */
static void VmCoroStateSwap(ph7_vm *pVm, VmCoroVmState *pOut, VmCoroVmState *pIn)
{
	VmCoroVmState sLive;
#define VM_CORO_SAVE(F)  sLive.F = pVm->F;
	VM_CORO_STATE_FIELDS(VM_CORO_SAVE)
#undef VM_CORO_SAVE
#define VM_CORO_LOAD(F)  pVm->F = pIn->F;
	VM_CORO_STATE_FIELDS(VM_CORO_LOAD)
#undef VM_CORO_LOAD
	*pOut = sLive;
}
/*
 * The resumer's side of one switch into the fiber: install the fiber's view of
 * the VM, go, and take the resumer's back when control returns. PH7_SUSPEND
 * when the fiber suspended again, else whatever its body returned.
 */
static sxi32 VmCoroRun(ph7_vm *pVm, ph7_exec_ctx *pCtx)
{
	VmCoroStateSwap(pVm, &pCtx->sHost, &pCtx->sSaved);
	VmCoroEnter(pCtx->pCoro);
	VmCoroStateSwap(pVm, &pCtx->sSaved, &pCtx->sHost);
	return pCtx->bCoroDone ? pCtx->iCoroRc : PH7_SUSPEND;
}
/*
 * The fiber's side, entered exactly once. Every later resume comes back inside
 * whatever Fiber::suspend() the stack is parked on, not here.
 */
static void VmCoroBody(ph7_exec_ctx *pCtx)
{
	ph7_vm *pVm = pCtx->pVm;
	sxi32 rc;
	VmCoroAsanArrive(pCtx->pCoro, 0);
	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),
		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,
		VmCtxEnforceRetFunc(pCtx), FALSE, 0, &pCtx->pStack, &pCtx->nStackCap,
		pCtx->nStackOrig);
	pCtx->iCoroRc = rc;
	pCtx->bCoroDone = 1;
	VmCoroLeaveFinal(pCtx->pCoro);
}
/*
 * Suspend from inside the fiber. Returns PH7_OK with the resume value waiting
 * in sSuspendValue, PH7_EXCEPTION when the fiber was resumed by Fiber::throw(),
 * or PH7_ABORT when it was resumed only to be unwound (the kill switch).
 */
static sxi32 VmCoroSuspend(ph7_context *pCallCtx, ph7_exec_ctx *pCtx)
{
	ph7_vm *pVm = pCtx->pVm;
	VmCoroLeave(pCtx->pCoro);
	/* Resumed. */
	if( pCtx->bCoroKill ){
		/* The Fiber object died while we were parked here. Unwinding is the whole
		 * point of coming back: every C frame between here and the body entry gets
		 * to run its own abort path and free what it owns, which is the only way a
		 * builtin's half-built result (array_map's output array, an open handle)
		 * is ever released -- nothing outside this stack can reach them. */
		return PH7_ABORT;
	}
	if( pCtx->pInjected ){
		/* Fiber::throw(): php raises AT the suspension point, so the throw happens
		 * here rather than at a body-entry redirect (there is no body entry on this
		 * path -- the resume lands inside this C call). Same shape as any builtin's
		 * own throw: stamp the frame, raise, and report the status on the call
		 * context so OP_CALL does not treat the call as a normal return. */
		ph7_class_instance *pInj = pCtx->pInjected;
		VmFrame *pFrame;
		sxi32 rc;
		pCtx->pInjected = 0;   /* one-shot */
		pFrame = pVm->pFrame;
		if( pFrame ){
			pFrame = VmSkipExceptionFrames(pFrame);
			pFrame->iFlags |= VM_FRAME_THROW;
		}
		rc = VmThrowException(pVm, pInj);
		if( rc == SXERR_ABORT ){
			pCallCtx->nThrowRc = PH7_ABORT;
			return PH7_ABORT;
		}
		pCallCtx->nThrowRc = PH7_EXCEPTION;
		return PH7_EXCEPTION;
	}
	return PH7_OK;
}
/*
 * Re-raise, in the RESUMER's frame, the exception a fiber body let escape.
 * Called at the three doors that run a body (start / resume / throw) whenever
 * the run came back PH7_EXCEPTION with an instance parked on the ctx. Same
 * shape as any builtin's own throw: stamp the frame, raise, and report the
 * status on the call context so OP_CALL does not read the call as a normal
 * return.
 */
static sxi32 VmFiberRaiseEscaped(ph7_context *pCtx, ph7_exec_ctx *pExecCtx)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pExc = pExecCtx->pEscaped;
	VmFrame *pFrame;
	sxi32 rc;
	pExecCtx->pEscaped = 0;
	pFrame = pVm->pFrame;
	if( pFrame ){
		pFrame = VmSkipExceptionFrames(pFrame);
		pFrame->iFlags |= VM_FRAME_THROW;
	}
	rc = VmThrowException(pVm, pExc);
	PH7_ClassInstanceUnref(pExc);
	if( rc == SXERR_ABORT ){
		pCtx->nThrowRc = PH7_ABORT;
		return PH7_ABORT;
	}
	pCtx->nThrowRc = PH7_EXCEPTION;
	return PH7_EXCEPTION;
}
#endif /* PH7_CORO_STACK */
/*
 * Allocate and initialize a new execution context for a fiber.
 * The context is in CREATED state and ready to be started.
 */
PH7_PRIVATE ph7_exec_ctx * VmNewExecCtx(ph7_vm *pVm, ph7_vm_func *pFunc)
{
	ph7_exec_ctx *pCtx;
	ph7_value *pStack;
	VmFrame *pFrame;
	pCtx = (ph7_exec_ctx *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_exec_ctx));
	if( pCtx == 0 ){
		return 0;
	}
	SyZero(pCtx, sizeof(ph7_exec_ctx));
	pCtx->pVm = pVm;
	pCtx->pFunc = pFunc;
	/* A coroutine outlives the call that made it and reads pFunc for the whole of
	 * its life -- including before its body frame exists, which VmStartCtx creates
	 * LAZILY. For a run-time closure that is a hold of its own on the
	 * per-instantiation copy: `(function(){ yield 1; })()` drops the Closure object
	 * at the call, and without this the body was freed under the Generator that
	 * still names it. */
	PH7_VmClosureFuncRef(pFunc);
	pCtx->iState = PH7_CTX_STATE_CREATED;
	pCtx->nTos = -1; /* Empty stack — matches VmByteCodeExec convention */
	pCtx->pc = 0;
	PH7_MemObjInit(pVm, &pCtx->sSuspendValue);
	PH7_MemObjInit(pVm, &pCtx->sRetValue);
	PH7_MemObjInit(pVm, &pCtx->sDelegate);
	/* Container for this body's own exception handlers while suspended (borrowed
	 * ph7_exception* pointers — never freed here, owned by the compiled func). */
	SySetInit(&pCtx->aSavedException, &pVm->sAllocator, sizeof(ph7_exception *));
	/* ROOT C: this body's own pending finally actions while suspended. */
	SySetInit(&pCtx->aSavedFinally, &pVm->sAllocator, sizeof(VmFinallyAction));
	pCtx->nFinallyBase = 0;
	/* Stage 4: this coroutine's own aSelf entries (self::/static:: class context
	 * pushed by nested method calls still open at suspend) parked while suspended,
	 * so they don't pollute the resumer's aSelf. Borrowed ph7_class* pointers. */
	SySetInit(&pCtx->aSavedSelf, &pVm->sAllocator, sizeof(ph7_class *));
	pCtx->nSelfBase = 0;
	/* The class this body's `static::` means, taken from the CALL that is creating it
	 * (VmStartCtx pushes it back for the body's duration). */
	pCtx->pLsbClass = PH7_VmPeekTopClass(pVm);
	/* Caller slots this body's by-reference parameters alias (see the struct). */
	SySetInit(&pCtx->aByRefArg, &pVm->sAllocator, sizeof(sxu32));
	pCtx->pParkedSegment = 0;
	pCtx->nBodyExecDepth = 0;
	/* Allocate a private operand stack */
	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));
	if( pStack == 0 ){
		PH7_VmClosureFuncUnref(pVm, pFunc);
		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);
		return 0;
	}
	pCtx->pStack = pStack;
	pCtx->nStackCap = SySetUsed(&pFunc->aByteCode) + VM_STACK_GUARD; /* grows with pStack on OP_SPREAD */
	pCtx->nStackOrig = pCtx->nStackCap; /* fixed original — headroom reference across resumes */
	/* Create a detached frame for the fiber */
	pFrame = VmNewFrame(pVm, pFunc, 0);
	if( pFrame == 0 ){
		SyMemBackendFree(&pVm->sAllocator, pStack);
		PH7_VmClosureFuncUnref(pVm, pFunc);
		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);
		return 0;
	}
	/* The frame's OWN hold, and not the one taken above. VmNewFrame stamps pFunc as
	 * the frame's pUserData exactly as VmEnterFrame does, and this body frame's only
	 * teardown -- VmFreeDetachedFrame, from VmCloseCtx -- gives that hold back the way
	 * VmLeaveFrame does for a live frame. Without it one coroutine took one hold and
	 * gave back two, so the first Generator or Fiber built from a run-time closure
	 * freed the per-instantiation copy its own Closure object was still naming, and
	 * calling that closure a second time said "Call to undefined function
	 * [closure_N]()". */
	PH7_VmClosureFuncRef(pFunc);
	pCtx->pFrame = pFrame;
	return pCtx;
}
/*
 * A suspended coroutine must not leave its own slices of the VM's shared stacks
 * sitting above the caller's depth. Three stacks are affected, identically:
 *   - pVm->aException: its exception handlers — else a generator/fiber suspended
 *     inside a try leaves handlers referencing its now-detached frame on the
 *     global stack, corrupting the caller's try/catch.
 *   - pVm->aFinallyAction (ROOT C): its pending finally actions — else a yield
 *     inside a finally (reached by return/break/rethrow) leaves a record where an
 *     out-of-order-resumed sibling generator's OP_END_FINALLY would mis-pop it.
 *   - pVm->aSelf (stage 4): its self::/static:: entries pushed by still-open
 *     nested method calls — else they sit on the resumer's aSelf and corrupt its
 *     self:: resolution.
 * Each is the same operation: on suspend move the slice above a captured base
 * into a per-ctx park buffer; on resume re-publish it at the (refreshed) caller
 * depth. VmParkStackSlice / VmRestoreStackSlice factor it for any element type
 * (size taken from the SySet); VmParkCtxState / VmRestoreCtxState drive all three.
 *
 * Stage 4: the whole suspended segment stays alive, so a parked handler's owner
 * frame is never freed underneath it — the parked pointer stays valid and is kept
 * (the old stage-2b lossy-path invalidation is gone with the discard). A
 * body-level suspend only ever has body-owned handlers here, and its finally/self
 * slices are empty (all nested calls already returned) — so those are no-ops.
 */
static void VmParkStackSlice(SySet *pFrom, SySet *pSaved, sxu32 nBase)
{
	sxu32 nUsed = SySetUsed(pFrom);
	if( nUsed > nBase ){
		const char *aBase = (const char *)SySetBasePtr(pFrom);
		sxu32 i;
		for( i = nBase; i < nUsed; i++ ){
			SySetPut(pSaved, (const void *)(aBase + i * pFrom->eSize));
		}
		SySetTruncate(pFrom, nBase);
	}
}
static void VmRestoreStackSlice(SySet *pTo, SySet *pSaved)
{
	sxu32 i, n = SySetUsed(pSaved);
	if( n > 0 ){
		const char *aSaved = (const char *)SySetBasePtr(pSaved);
		for( i = 0; i < n; i++ ){
			SySetPut(pTo, (const void *)(aSaved + i * pSaved->eSize));
		}
		SySetReset(pSaved);
	}
}
static void VmParkCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)
{
	VmParkStackSlice(&pVm->aException, &pCtx->aSavedException, pCtx->nExceptionBase);
	VmParkStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally, pCtx->nFinallyBase);
	VmParkStackSlice(&pVm->aSelf, &pCtx->aSavedSelf, pCtx->nSelfBase);
}
static void VmRestoreCtxState(ph7_vm *pVm, ph7_exec_ctx *pCtx)
{
	VmRestoreStackSlice(&pVm->aException, &pCtx->aSavedException);
	VmRestoreStackSlice(&pVm->aFinallyAction, &pCtx->aSavedFinally);
	VmRestoreStackSlice(&pVm->aSelf, &pCtx->aSavedSelf);
}
/*
 * On suspend, free the exception (try) frames the yield was nested in. They were
 * pushed by OP_LOAD_EXCEPTION between the coroutine body frame (pCtx->pFrame) and
 * the current suspend-point top frame. The generator/fiber frame model saves only
 * the body frame, so these transparent wrappers would otherwise be orphaned and
 * leak on every yield-that-sits-inside-a-try (unbounded for a generator looping
 * with a yield in a try). Freeing them loses nothing the resume needs: this body's
 * exception HANDLERS are parked separately (VmParkCtxState) and each
 * try's landing pad lives on its ph7_exception (iLandingPc), while OP_POP_EXCEPTION
 * on resume skips the (now absent) frame pop via its VM_FRAME_EXCEPTION guard and
 * OP_LOAD_EXCEPTION re-creates a fresh wrapper when the try is next entered. Must
 * run while pVm->pFrame still points at the suspend-time top (before the detach).
 */
static void VmFreeSuspendedExceptionFrames(ph7_vm *pVm, ph7_exec_ctx *pCtx)
{
	while( pVm->pFrame != pCtx->pFrame && (pVm->pFrame->iFlags & VM_FRAME_EXCEPTION) ){
		VmLeaveFrame(&(*pVm));
	}
}
/*
 * Stamp a coroutine body frame with the site that is starting or RESUMING it.
 *
 * An ordinary frame gets this in VmEnterFrame; a coroutine's body frame is built
 * detached (VmNewExecCtx -> VmNewFrame) and never went through it, so a backtrace
 * taken inside a generator reported the frame below it at line 0 -- printed as
 * line 1, in whatever file the include stack happened to top out at. php answers
 * the CURRENT resume site rather than the creation site (`foreach (g() as $v)`
 * for the first step, the `yield from` line for a delegate), which is exactly
 * what this reads, so it is stamped on every start and resume rather than once.
 * Must run BEFORE the frame is spliced onto the chain: the site is the resumer's.
 */
static void VmStampCoroutineCallSite(ph7_vm *pVm, ph7_exec_ctx *pCtx)
{
	SyString *pFile;
	if( pCtx->pFrame == 0 ){
		return;
	}
	pCtx->pFrame->nCallLine = pVm->nCurLine;
	pFile = PH7_VmExecutingUnitFile(&(*pVm));
	if( pFile ){
		pCtx->pFrame->sCallFile = *pFile;
	}
}
/*
 * Common suspend epilogue for VmStartCtx / VmResumeCtx: detach the suspended
 * coroutine from the live VM chain and park its exception handlers. Two forms:
 *   - Body-level (pParkedSegment == 0): a generator yield or a fiber suspending
 *     directly in its body. The try wrappers the yield sat in are transient —
 *     free them (OP_LOAD_EXCEPTION recreates them on re-entry) — and detach the
 *     body frame alone.
 *   - Deep fiber suspend (pParkedSegment != 0, stage 4): the whole segment (body
 *     frame + the nested call/try frames above it) stays alive and is detached
 *     as a unit; nothing is freed, so resume can continue inside the innermost
 *     callee. Its handlers are parked the same way and rebased on resume.
 */
static void VmSuspendCtxDetach(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)
{
#ifdef PH7_CORO_STACK
	if( pCtx->pCoro ){
		/* Third form: the fiber has a stack of its own and is still standing on
		 * it. Its frames -- body, nested callees, open-try wrappers alike -- stay
		 * exactly as they are, because that C stack still points into them; all
		 * this has to remember is which one was current, so the resume can make it
		 * current again. Nothing is parked and no depth is deducted: the three
		 * stacks and both counters travelled with the VM-state swap the moment the
		 * switch happened. */
		pCtx->pCoroTop = pVm->pFrame;
		pVm->pFrame = pCtx->pFrame->pParent;
		pCtx->pFrame->pParent = 0;
		if( pResult ){
			PH7_MemObjStore(&pCtx->sSuspendValue, pResult);
		}
		return;
	}
#endif
	if( pCtx->pParkedSegment == 0 ){
		VmFreeSuspendedExceptionFrames(pVm, pCtx);
	}else{
		/* The parked records' push-time accounting (one nRecursionDepth++ each,
		 * plus VmCallFinish's aSelf pop, which never ran) leaves the segment
		 * counted as active while the fiber is suspended — deactivate it. aSelf
		 * is parked wholesale below (base-relative), so drop only the depth. */
		pVm->nRecursionDepth -= ((VmParkedSegment *)pCtx->pParkedSegment)->nRecords;
	}
	pVm->pFrame = pCtx->pFrame->pParent;
	pCtx->pFrame->pParent = 0;
	VmParkCtxState(pVm, pCtx);
	if( pResult ){
		PH7_MemObjStore(&pCtx->sSuspendValue, pResult);
	}
}
/*
 * The return-type enforcement target for a coroutine body run. A GENERATOR
 * function's declared return type belongs to the call site (always a Generator
 * object, validated at compile time as "a supertype of Generator"); the body's
 * own return value feeds getReturn() and is never type-checked. Gate on the
 * VM_FUNC_GENERATOR flag (the semantic property), not pPrivate (a wrapper-linkage
 * fact): a Fiber given a generator-flagged callable runs with pPrivate == 0 and
 * must not enforce either. Ordinary fiber callables keep their declared
 * return-type enforcement (php enforces it).
 */
static ph7_vm_func * VmCtxEnforceRetFunc(ph7_exec_ctx *pCtx)
{
	return ((pCtx->pFunc->iFlags & VM_FUNC_GENERATOR) == 0 && VmFuncHasReturnType(pCtx->pFunc))
		? pCtx->pFunc : 0;
}
/*
 * Common epilogue for VmStartCtx / VmResumeCtx once the body run returns rc:
 * restore the previous active context, then park on suspend or detach the
 * coroutine frame and record the terminal state. On normal completion the value
 * belongs to getReturn() only — start()/resume() return the NEXT suspend value,
 * which is null at completion (php parity), so pResult is left at its
 * caller-initialized null.
 */
static sxi32 VmFinishCtxRun(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_exec_ctx *pOldCtx,
	sxi32 rc, ph7_value *pResult)
{
	pVm->pActiveCtx = pOldCtx;
#ifdef PH7_CORO_STACK
	if( pCtx->pCoro ){
		if( rc == PH7_SUSPEND ){
			/* No saved pc or top-of-stack to record: the switch IS the state, and
			 * VmSuspendCtx (which marks the ctx on the other path) never ran. */
			pCtx->iState = PH7_CTX_STATE_SUSPENDED;
			VmSuspendCtxDetach(pVm, pCtx, pResult);
			return SXRET_OK;
		}
		/* The body is over, so the stack is spent: free it now rather than at the
		 * Fiber object's death (a completed fiber has no use for megabytes of
		 * mapping), and with it the three stacks that were only ever this body's.
		 * The frame chain it left is the body frame plus any try wrappers an
		 * escaping exception never closed -- the same two steps the shared tail
		 * below takes, which the ctx-owned bases in it do not apply to here. */
		VmFreeSuspendedExceptionFrames(pVm, pCtx);
		if( pVm->pFrame == pCtx->pFrame ){
			pVm->pFrame = pCtx->pFrame->pParent;
			pCtx->pFrame->pParent = 0;
		}
		VmCoroFree(pVm, pCtx->pCoro);
		pCtx->pCoro = 0;
		VmCoroStateRelease(pVm, &pCtx->sSaved);
		if( rc == PH7_ABORT ){
			pCtx->iState = PH7_CTX_STATE_CLOSED;
			return PH7_ABORT;
		}
		if( rc == PH7_EXCEPTION ){
			pCtx->iState = PH7_CTX_STATE_CLOSED;
			pCtx->bThrew = 1;
			return PH7_EXCEPTION;
		}
		pCtx->iState = PH7_CTX_STATE_COMPLETED;
		return SXRET_OK;
	}
#endif
	if( rc == PH7_SUSPEND ){
		/* Handles a deep RE-suspend too (a resumed segment parking a fresh one):
		 * VmSuspendCtxDetach deactivates the new segment's recursion accounting,
		 * parks its aSelf, and skips VmFreeSuspendedExceptionFrames for a segment
		 * so it can't free the still-live parked try wrappers. */
		VmSuspendCtxDetach(pVm, pCtx, pResult);
		return SXRET_OK;
	}
	/* A finally entered via the throw redirect whose `return` short-circuited
	 * OP_END_FINALLY leaves the try's transparent wrapper ABOVE the body frame —
	 * the detach below would then be skipped and the wrapper (plus the body
	 * frame) leak into the RESUMER's frame chain, so the next try at that scope
	 * records the wrong owner frame and its caught throw silently unwinds the
	 * script. Free trailing exception wrappers exactly like the suspend path. */
	if( pCtx->pParkedSegment == 0 ){
		VmFreeSuspendedExceptionFrames(pVm, pCtx);
	}
	/* Detach the coroutine frame from the live chain, unless a deeper unwind
	 * already moved pVm->pFrame off it. */
	if( pVm->pFrame == pCtx->pFrame ){
		pVm->pFrame = pCtx->pFrame->pParent;
		pCtx->pFrame->pParent = 0;
	}
	/* The body is over (it did not suspend): drop whatever it left on the shared
	 * self stack, which is at least the LSB class VmStartCtx published for it. */
	if( SySetUsed(&pVm->aSelf) > pCtx->nSelfBase ){
		SySetTruncate(&pVm->aSelf, pCtx->nSelfBase);
	}
	if( rc == PH7_ABORT ){
		pCtx->iState = PH7_CTX_STATE_CLOSED;
		return PH7_ABORT;
	}
	if( rc == PH7_EXCEPTION ){
		pCtx->iState = PH7_CTX_STATE_CLOSED;
		pCtx->bThrew = 1;
		return PH7_EXCEPTION;
	}
	pCtx->iState = PH7_CTX_STATE_COMPLETED;
	return SXRET_OK;
}
/*
 * Start executing a fiber context for the first time.
 */
static sxi32 VmStartCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResult)
{
	ph7_exec_ctx *pOldCtx;
	sxi32 rc;
	if( pCtx->iState != PH7_CTX_STATE_CREATED ){
		return SXERR_INVALID;
	}
	/* A fiber/generator start is a native VmByteCodeExec re-entry, bounded by
	 * nMaxNativeDepth. Reject HERE, before attaching the frame / mutating VM
	 * state, so the abort is clean — the wrapper's own check fires only after
	 * this function has spliced the coroutine into the frame chain, which its
	 * post-exec detach cannot fully unwind. The PHP call-depth cap belongs to
	 * OP_CALL only. */
	if( VmNativeNestingExceeded(pVm) ){
		return VmNativeNestingFatal(pVm);
	}
#ifdef PH7_CORO_STACK
	/* A FIBER gets a native stack of its own (pPrivate == 0 is what tells one from
	 * a generator, which never needs one: `yield` is lexically inside the body, so
	 * it can never have a C frame above it to park). Taken BEFORE anything below
	 * mutates the VM, because the fiber's opening view of it is the resumer's.
	 * A stack this build cannot give it leaves pCoro at 0 and the body runs inline
	 * exactly as it did before. */
	if( pCtx->pPrivate == 0 ){
		pCtx->pCoro = VmCoroNew(pVm, pCtx);
		if( pCtx->pCoro ){
			VmCoroStateInit(pVm, pCtx);
		}
	}
#endif
	/* Attach the fiber's frame to the VM frame chain */
	VmStampCoroutineCallSite(pVm, pCtx);
	pCtx->pFrame->pParent = pVm->pFrame;
	pVm->pFrame = pCtx->pFrame;
	/* Save and set the active context */
	pOldCtx = pVm->pActiveCtx;
	pVm->pActiveCtx = pCtx;
	pCtx->iState = PH7_CTX_STATE_RUNNING;
#ifdef PH7_CORO_STACK
	if( pCtx->pCoro ){
		/* Its three stacks are its own and start empty, so every floor an
		 * activation of this body records is measured from zero and stays true
		 * however deep the resumer happens to be next time. */
		pCtx->nExceptionBase = 0;
		pCtx->nFinallyBase = 0;
		pCtx->nSelfBase = 0;
		if( pCtx->pLsbClass ){
			SySetPut(&pCtx->sSaved.aSelf, (const void *)&pCtx->pLsbClass);
		}
	}else
#endif
	{
		pCtx->nExceptionBase = SySetUsed(&pVm->aException);
		pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);
		pCtx->nSelfBase = SySetUsed(&pVm->aSelf);
		/* Re-publish the creating call's late-static-binding class ABOVE that base, so
		 * the body's `static::` resolves to what php resolves it to. It rides the
		 * ordinary park/restore of this coroutine's own aSelf slice, so a suspend takes
		 * it off the shared stack and a resume puts it back; VmFinishCtxRun truncates it
		 * away when the body ends for good. */
		if( pCtx->pLsbClass ){
			SySetPut(&pVm->aSelf,(const void *)&pCtx->pLsbClass);
		}
	}
	/* Native depth the body runs at (the VmByteCodeExec wrapper bumps +1): a
	 * Fiber::suspend() at a deeper depth is inside a C->PHP callback and gets a
	 * FiberError instead of parking across the native frame (stage 4). */
	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1;
#ifdef PH7_CORO_STACK
	if( pCtx->pCoro ){
		/* On its own stack the body starts at native depth zero, so it never reads
		 * nBodyExecDepth again -- the suspend that used to consult it now simply
		 * switches, from wherever it is. */
		rc = VmCoroRun(pVm, pCtx);
		return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);
	}
#endif
	/* Execute from the beginning (no parked segment). An OP_SPREAD in the body may
	 * realloc pCtx->pStack — pass &pCtx->pStack / &pCtx->nStackCap so the grown
	 * buffer + capacity persist for the next resume and for ctx teardown. */
	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),
		pCtx->pStack, -1, &pCtx->sRetValue, 0, FALSE, 0,
		VmCtxEnforceRetFunc(pCtx), FALSE, 0, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);
	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);
}
/*
 * Resume a suspended fiber context.
 */
PH7_PRIVATE sxi32 VmResumeCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx, ph7_value *pResumeValue, ph7_value *pResult)
{
	ph7_exec_ctx *pOldCtx;
	VmParkedSegment *pSeg;
	sxi32 rc;
	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){
		return SXERR_INVALID;
	}
#ifdef PH7_CORO_STACK
	if( pCtx->pCoro ){
		/* A fiber on its own stack: nothing to re-push, rebase or adopt. The whole
		 * suspended activation chain is still standing on that stack, so a resume
		 * is the frame re-attach plus one switch.
		 *
		 * The nesting guard above is skipped on purpose: this re-entry adds a
		 * single frame to the RESUMER's stack and then leaves it for the fiber's,
		 * which carries its own depth count -- and a teardown unwind (bCoroKill)
		 * must go through even when the resumer is already at the cap, or the C
		 * frames it is there to unwind are freed underneath instead.
		 *
		 * The resume value reaches the parked Fiber::suspend() through the ctx's
		 * own slot rather than an operand stack: on this path the suspend is a C
		 * call about to RETURN a value, not a saved pc with a hole above its top. */
		if( pResumeValue ){
			PH7_MemObjStore(pResumeValue, &pCtx->sSuspendValue);
		}else{
			PH7_MemObjRelease(&pCtx->sSuspendValue);
		}
		VmStampCoroutineCallSite(pVm, pCtx);
		pCtx->pFrame->pParent = pVm->pFrame;
		pVm->pFrame = pCtx->pCoroTop;
		pOldCtx = pVm->pActiveCtx;
		pVm->pActiveCtx = pCtx;
		pCtx->iState = PH7_CTX_STATE_RUNNING;
		rc = VmCoroRun(pVm, pCtx);
		return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);
	}
#endif
	/* A resume is a native VmByteCodeExec re-entry, bounded by nMaxNativeDepth.
	 * Reject HERE, before re-attaching the (possibly deep) parked segment to the
	 * frame chain and re-adding its nRecords to nRecursionDepth — a wrapper-level
	 * abort past those mutations would leave pVm->pFrame pointing into the parked
	 * callee and the depth accounting un-reverted. The PHP call-depth cap is
	 * OP_CALL-only. */
	if( VmNativeNestingExceeded(pVm) ){
		return VmNativeNestingFatal(pVm);
	}
	/* Push the resume value onto the SUSPENDED activation's operand stack so it
	 * appears as Fiber::suspend()'s return value. For a deep suspend (stage 4)
	 * that stack is the innermost callee's — parked in the segment — not the
	 * body's. nTos was saved one below the return-value slot. */
	{
		ph7_value *pResumeStack;
		pSeg = (VmParkedSegment *)pCtx->pParkedSegment;
		pResumeStack = pSeg ? pSeg->sState.pStack : pCtx->pStack;
		if( pResumeValue ){
			PH7_MemObjStore(pResumeValue, &pResumeStack[pCtx->nTos + 1]);
		}else{
			PH7_MemObjRelease(&pResumeStack[pCtx->nTos + 1]);
		}
		pCtx->nTos++;
		/* Refresh the caller-depth base and re-publish this body's own exception
		 * handlers on top of pVm->aException at that depth, so the resumed body's
		 * try/catch and finally-drain bound line up (see VmByteCodeExec's base
		 * override). Must run before VmByteCodeExec recaptures its local base. */
		pCtx->nExceptionBase = SySetUsed(&pVm->aException);
		pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);
		pCtx->nSelfBase = SySetUsed(&pVm->aSelf);
		VmRestoreCtxState(pVm, pCtx);
		if( pSeg ){
			/* Reactivate the parked records' recursion accounting (mirror of the
			 * deactivate at suspend); aSelf was just restored above. */
			pVm->nRecursionDepth += pSeg->nRecords;
			/* Rebase the parked segment's absolute exception-floor indices: the
			 * fiber may resume at a different caller depth than it suspended at,
			 * so every activation's nExceptionBase shifts by the same delta the
			 * republished handlers moved (newBase - the park-time base). */
			sxi32 iDelta = (sxi32)pCtx->nExceptionBase - (sxi32)pSeg->nOldExcBase;
			/* nFinallyActBase floors rebase by their OWN delta — the exception and
			 * finally-action stacks move independently between suspend and resume
			 * (a fiber resumed from inside a generator's inline finally sees a
			 * DEEPER aFinallyAction with an unchanged aException, and a stale
			 * absolute floor would make the activation-end discard eat the
			 * resumer's pending action). */
			sxi32 iFinDelta = (sxi32)pCtx->nFinallyBase - (sxi32)pSeg->nOldFinBase;
			if( iDelta != 0 || iFinDelta != 0 ){
				VmCallFrame *pRec;
				pSeg->sState.nExceptionBase =
					(sxu32)((sxi32)pSeg->sState.nExceptionBase + iDelta);
				pSeg->sState.nFinallyActBase =
					(sxu32)((sxi32)pSeg->sState.nFinallyActBase + iFinDelta);
				for( pRec = pSeg->pCallTop; pRec; pRec = pRec->pPrev ){
					pRec->sCaller.nExceptionBase =
						(sxu32)((sxi32)pRec->sCaller.nExceptionBase + iDelta);
					pRec->sCaller.nFinallyActBase =
						(sxu32)((sxi32)pRec->sCaller.nFinallyActBase + iFinDelta);
				}
			}
		}
		/* Re-attach the coroutine to the live VM frame chain: the body frame's
		 * parent becomes the resumer's current frame. For a deep segment the
		 * suspend-time top frame (the innermost callee / open-try wrapper) then
		 * becomes current so the adopt at VmByteCodeExec entry resumes inside the
		 * callee; body-level resumes make the body frame current. */
		VmStampCoroutineCallSite(pVm, pCtx);
		pCtx->pFrame->pParent = pVm->pFrame;
		pVm->pFrame = pSeg ? pSeg->pTopFrame : pCtx->pFrame;
	}
	/* The segment (if any) is handed to the body invocation explicitly below; it
	 * is no longer part of the suspended ctx state once resume owns it. */
	pCtx->pParkedSegment = 0;
	/* Save and set the active context */
	pOldCtx = pVm->pActiveCtx;
	pVm->pActiveCtx = pCtx;
	pCtx->iState = PH7_CTX_STATE_RUNNING;
	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1; /* see VmStartCtx */
	/* Resume execution from saved PC. pSeg, when non-NULL, makes this body
	 * re-enter inside the innermost parked callee (deep fiber resume, stage 4). */
	rc = VmByteCodeExec(pVm, (VmInstr *)SySetBasePtr(&pCtx->pFunc->aByteCode),
		pCtx->pStack, pCtx->nTos, &pCtx->sRetValue, 0, FALSE, pCtx->pc,
		VmCtxEnforceRetFunc(pCtx), FALSE, pSeg, &pCtx->pStack, &pCtx->nStackCap, pCtx->nStackOrig);
	return VmFinishCtxRun(pVm, pCtx, pOldCtx, rc, pResult);
}
/*
 * Force-close a suspended generator context at destruction time, running its
 * pending `finally` blocks (PHP runs finally when a generator is unset / goes out
 * of scope / is GC'd before it completes; PHL previously freed the open try
 * handlers unexecuted in VmReleaseExecCtx). This is a "close", not a "resume":
 * the finally handler of every still-open `try` the generator was suspended
 * inside runs innermost-first, but NO `catch` runs and no code past the finallys
 * executes.
 *
 * Generators compile finally INLINE (ROOT C): the finally bytecode lives in the
 * body program at iFinallyPc, driven by the aException/aFinallyAction pc-redirect
 * machinery — not the legacy detached `sFinally` mini-program VmDrainFinally runs.
 * So a close is expressed exactly like a `return` that crosses every enclosing
 * finally (OP_SET_FINALLY_RET): set pCtx->bClosing and resume; the body-entry
 * redirect (see VmByteCodeExecBody) seeds a PH7_FA_RETURN action and jumps into
 * the innermost open try's finally, and OP_END_FINALLY threads it out through the
 * chain, then completes the body.
 *
 * Scope: a plain body-level suspend (pParkedSegment == 0). A deep fiber segment is
 * left to plain release (generators never park one — yield is body-level only). A
 * `yield` reached inside a finally during close is rejected by OP_YIELD via
 * pCtx->bClosing (PHP-exact "Cannot yield from finally in a force-closed
 * generator"). Deferred edges remain.
 *
 * Returns whatever the body run returns: SXRET_OK on a clean close, PH7_ABORT if a
 * finally aborts, or PH7_EXCEPTION if a finally threw past itself (surfaced to the
 * destruct caller).
 */
static sxi32 VmCloseCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)
{
	sxi32 rc;
	if( pCtx->iState != PH7_CTX_STATE_SUSPENDED ){
		/* CREATED never ran, so has no open try; COMPLETED/CLOSED already ran theirs. */
		return SXRET_OK;
	}
	if( pCtx->pParkedSegment != 0 ){
		/* Deep fiber segment (never a generator) — leave to plain release. */
		return SXRET_OK;
	}
	/* Suspended mid `yield from` over an inner generator: PHP closes innermost-first,
	 * so run the delegate's finallys before this body's. Both delegate-object states
	 * carry the live iterator in sDelegate — state 3 (the delegate IS a Generator) and
	 * state 2 (an Iterator, which for `yield from $aggregate` is the generator returned
	 * by getIterator()); VmGeneratorExtractCtx returns 0 for a non-generator iterator,
	 * so a plain Iterator/array delegate is skipped. Recurses for nested delegation;
	 * the inner ends COMPLETED, so its own later __destruct close is a no-op. */
	if( pCtx->iDelegateState >= 2 && (pCtx->sDelegate.iFlags & MEMOBJ_OBJ) ){
		ph7_generator *pInner = VmGeneratorExtractCtx(pVm, &pCtx->sDelegate);
		if( pInner && pInner->pCtx ){
			sxi32 rcInner = VmCloseCtx(pVm, pInner->pCtx);
			if( rcInner == PH7_ABORT ){ return PH7_ABORT; }
		}
	}
	/* Drive the pending finallys through a real body resume that the entry redirect
	 * turns into a finally-chain unwind. VmResumeCtx handles state restore, frame
	 * re-attach, active-ctx save/restore and the terminal-state bookkeeping. */
	pCtx->bClosing = 1;
	rc = VmResumeCtx(pVm, pCtx, 0, 0);
	pCtx->bClosing = 0;
	return rc;
}
/*
 * Free one DETACHED frame (not in the live pVm->pFrame chain): the frame of a
 * suspended coroutine's body, or of a segment activation abandoned mid-call.
 * Mirrors VmLeaveFrame's teardown minus the chain pop (there is no chain to pop
 * from). Factored so the body-frame free and the stage-4 segment free share it.
 */
static void VmFreeDetachedFrame(ph7_vm *pVm, VmFrame *pFrame)
{
	VmSlot *aSlot;
	sxu32 n;
	if( pFrame == 0 ){
		return;
	}
	/* The activation's hold on the function it was running, the same one
	 * VmLeaveFrame gives back for a live frame. */
	if( pFrame->pUserData ){
		PH7_VmClosureFuncUnref(pVm,(ph7_vm_func *)pFrame->pUserData);
		pFrame->pUserData = 0;
	}
	/* End the foreach walks this (abandoned) activation never finished — the same
	 * teardown, at the same point, VmLeaveFrame does it at. */
	VmReleaseFrameForeachSteps(pVm,pFrame);
	/* Remove local references FIRST, then free the locals nothing else holds — the
	 * order and the holder test VmLeaveFrame explains. */
	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);
	for( n = 0; n < SySetUsed(&pFrame->sRef); ++n ){
		PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);
	}
	/* Free local variables */
	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);
	for( n = 0; n < SySetUsed(&pFrame->sLocal); ++n ){
		if( PH7_VmSlotHolderCount(pVm, aSlot[n].nIdx) > 0 ){
			continue;
		}
		PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);
	}
	SyHashRelease(&pFrame->hVar);
	SySetRelease(&pFrame->sArg);
	SySetRelease(&pFrame->sLocal);
	SySetRelease(&pFrame->sRef);
	PH7_MemObjRelease(&pFrame->sRet);
	/* Drop a resume target pointing at this detached frame before we free it (ROOT B). */
	VmDropResumeTarget(pVm,pFrame);
	SyMemBackendPoolFree(&pVm->sAllocator, pFrame);
}
/*
 * Free a parked deep-suspend segment (stage 4) whose fiber was abandoned while
 * suspended. Every record holds a callee's operand stack and VmFrame (the
 * topmost record's callee is the innermost activation, running on sState); walk
 * the chain releasing each callee stack's live entries then the stack and frame.
 * The body frame/stack are NOT here — they are freed by the caller
 * (VmReleaseExecCtx) as pCtx->pFrame / pCtx->pStack.
 */
static void VmFreeParkedSegment(ph7_vm *pVm, ph7_exec_ctx *pCtx, VmParkedSegment *pSeg)
{
	/* Live top-of-stack of the activation running on the current record's callee
	 * stack: the innermost (sState) for the topmost record, then each caller. */
	ph7_value *pTosAbove = pSeg->sState.pTos;
	VmCallFrame *pRec = pSeg->pCallTop, *pNext;
	while( pRec ){
		ph7_value *pStk = pRec->sCall.pFrameStack;
		if( pStk ){
			ph7_value *pTos = pTosAbove;
			while( pTos >= pStk ){
				PH7_MemObjRelease(pTos);
				pTos--;
			}
			SyMemBackendFree(&pVm->sAllocator, pStk);
		}
		VmFreeDetachedFrame(pVm, pRec->sCall.pFrame);
		/* The caller recorded here runs on the NEXT-lower callee stack; grab its
		 * live tos before freeing this node. */
		pTosAbove = pRec->sCaller.pTos;
		pNext = pRec->pPrev;
		SyMemBackendPoolFree(&pVm->sAllocator, pRec);
		pRec = pNext;
	}
	/* pTosAbove now points at the BODY activation's live top (the bottom record's
	 * sCaller, which runs on pCtx->pStack). pCtx->nTos still holds the INNERMOST
	 * index (VmSuspendCtx saved it), which would over-index the body stack in
	 * VmReleaseExecCtx's release loop — correct it to the body's real top. */
	pCtx->nTos = (sxi32)(pTosAbove - pCtx->pStack);
	SyMemBackendFree(&pVm->sAllocator, pSeg);
}
/*
 * Release an execution context and all its resources.
 */
PH7_PRIVATE void VmReleaseExecCtx(ph7_vm *pVm, ph7_exec_ctx *pCtx)
{
	if( pCtx == 0 ){
		return;
	}
	if( pCtx->iState == PH7_CTX_STATE_RUNNING ){
		/* Cannot destroy a fiber that is currently executing */
		return;
	}
#ifdef PH7_CORO_STACK
	if( pCtx->pCoro && pCtx->iState == PH7_CTX_STATE_SUSPENDED && !pCtx->bCoroDone ){
		/* A fiber abandoned while suspended still has live C frames on its own
		 * stack -- a half-finished array_map, an open handle, an operand stack of
		 * its own -- and nothing outside that stack can reach them. Switch back in
		 * one last time with the kill flag set: the suspend it is parked on
		 * returns PH7_ABORT, every frame between there and the body entry runs its
		 * own abort path, and the body returns for good. VmFinishCtxRun then frees
		 * the stack. (php unwinds a dropped fiber for the same reason; that its
		 * unwind also runs the body's `finally` blocks and this one does not is
		 * recorded separately -- an abort is not a return.) */
		pCtx->bCoroKill = 1;
		(void)VmResumeCtx(pVm, pCtx, 0, 0);
	}
	if( pCtx->pCoro ){
		/* Never started, or the unwind above could not run it: the stack holds
		 * nothing live either way. */
		VmCoroFree(pVm, pCtx->pCoro);
		pCtx->pCoro = 0;
	}
	if( pCtx->pEscaped ){
		/* The body threw and nobody was left to re-raise it (the resumer aborted
		 * between the two). Give the reference back. */
		PH7_ClassInstanceUnref(pCtx->pEscaped);
		pCtx->pEscaped = 0;
	}
	VmCoroStateRelease(pVm, &pCtx->sSaved);
#endif
	pCtx->iState = PH7_CTX_STATE_CLOSED;
	/* ...and give back the hold VmNewExecCtx took on the function this coroutine runs. */
	if( pCtx->pFunc ){
		PH7_VmClosureFuncUnref(pVm,pCtx->pFunc);
	}
	/* Release values */
	PH7_MemObjRelease(&pCtx->sSuspendValue);
	PH7_MemObjRelease(&pCtx->sRetValue);
	PH7_MemObjRelease(&pCtx->sDelegate);
	/* Stage 2b: the parked entries are per-activation clones now — free them
	 * (an abandoned suspended coroutine is their last holder). */
	VmExcReleaseAll(pVm,&pCtx->aSavedException);
	SySetRelease(&pCtx->aSavedException);
	/* ROOT C: a generator abandoned while suspended inside a finally may carry parked
	 * finally actions holding owned values/refs (a RETURN's sRet, a RETHROW's pExc).
	 * Release them so the abandon path leaks nothing. */
	{
		sxu32 n = SySetUsed(&pCtx->aSavedFinally);
		if( n > 0 ){
			VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pCtx->aSavedFinally);
			sxu32 i;
			for( i = 0; i < n; i++ ){
				if( aA[i].eKind == PH7_FA_RETURN ){
					PH7_MemObjRelease(&aA[i].sRet);
				}else if( aA[i].eKind == PH7_FA_RETHROW && aA[i].pExc ){
					PH7_ClassInstanceUnref(aA[i].pExc);
				}
			}
		}
		SySetRelease(&pCtx->aSavedFinally);
	}
	/* Stage 4: parked aSelf entries are borrowed class pointers — just free the set. */
	SySetRelease(&pCtx->aSavedSelf);
	/* Free a parked deep-suspend segment (stage 4): the fiber was abandoned while
	 * suspended inside a nested call, so its record chain / frames / operand
	 * stacks are still alive and only this holder references them. Must run
	 * before the body frame/stack below (they are the segment's floor). */
	if( pCtx->pParkedSegment ){
		VmFreeParkedSegment(pVm, pCtx, (VmParkedSegment *)pCtx->pParkedSegment);
		pCtx->pParkedSegment = 0;
	}
	/* Release the frame if it's detached (not in the VM chain) */
	if( pCtx->pFrame ){
		VmFreeDetachedFrame(pVm, pCtx->pFrame);
		pCtx->pFrame = 0;
	}
	/* A by-reference parameter aliased a CALLER's slot; the frame teardown above just
	 * dropped this body's name for it. Release it if nothing is left holding it — the
	 * caller may already be gone (it skipped the slot precisely because this frame
	 * held it), in which case this is its last holder. Runs after the frame so the
	 * body's own row is out of the count. */
	{
		sxu32 n;
		sxu32 *aIdx = (sxu32 *)SySetBasePtr(&pCtx->aByRefArg);
		for( n = 0; n < SySetUsed(&pCtx->aByRefArg); n++ ){
			PH7_VmReleaseUnheldSlot(pVm, aIdx[n]);
		}
		SySetRelease(&pCtx->aByRefArg);
	}
	/* Release individual operand stack entries (decrement refcounts,
	 * free string buffers, etc.) before bulk-freeing the stack memory.
	 * Matches the cleanup pattern at the Abort: label in VmByteCodeExec. */
	if( pCtx->pStack ){
		if( pCtx->nTos >= 0 ){
			ph7_value *pTos = &pCtx->pStack[pCtx->nTos];
			while( pTos >= pCtx->pStack ){
				PH7_MemObjRelease(pTos);
				pTos--;
			}
		}
		SyMemBackendFree(&pVm->sAllocator, pCtx->pStack);
		pCtx->pStack = 0;
	}
	/* Free the context itself */
	SyMemBackendPoolFree(&pVm->sAllocator, pCtx);
}
/*
 * Helper: extract the ph7_exec_ctx from a Fiber class instance.
 * Returns NULL if the object is not a Fiber or has no context.
 */
static ph7_exec_ctx * VmFiberExtractCtx(ph7_vm *pVm, ph7_value *pFiberObj)
{
	ph7_class_instance *pThis;
	SyString sAttr;
	ph7_value *pAttr;
	if( (pFiberObj->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	pThis = (ph7_class_instance *)pFiberObj->x.pOther;
	if( pThis->pClass != pVm->pFiberClass ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr, "__ctx", 5);
	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);
	if( pAttr == 0 || (pAttr->iFlags & MEMOBJ_RES) == 0 ){
		return 0;
	}
	return (ph7_exec_ctx *)pAttr->x.pOther;
}
/* The three VM_INSTANCE_FCC_* Closure flags live in ph7int.h: vm_exec.c's OP_LOAD_FCC
 * stamps VM_INSTANCE_FCC_METHOD and this file reads all three. */
/*
 * A PHP closure (and a first-class callable `f(...)`) is a real object: an instance of
 * the built-in final `Closure` class carrying its underlying callable in a private
 * `$__fn` attribute (the callable NAME: a registered `[closure_N]`/`[lambda_N]` or a
 * user/host function name) — plus, for a method/static first-class callable, a bound
 * `$__this` object and/or a `$__scope` class-name. Storing the callable as plain attributes
 * (rather than a native resource pointer) means the object owns no `ph7_vm_func` — the
 * per-closure function stays owned by `hFunction`/VM-release exactly as before, so there is
 * no extra free path.
 *
 * Returns non-zero iff pVal is a Closure instance.
 */
PH7_PRIVATE int VmValueIsClosure(ph7_vm *pVm, ph7_value *pVal)
{
	ph7_class_instance *pThis;
	/* Flag test first: a non-object call target (the hot common case) bails before any
	 * pVm dereference; pClosureClass==0 is a one-time pre-init concern, so it goes last. */
	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 || pVal->x.pOther == 0 || pVm->pClosureClass == 0 ){
		return 0;
	}
	pThis = (ph7_class_instance *)pVal->x.pOther;
	/* Closure is final, so an exact class match is correct (no subclasses possible). */
	return pThis->pClass == pVm->pClosureClass;
}
/*
 * Unwrap a Closure value into the simple callable the existing dispatch machinery
 * already understands, written into pOut (which the caller must have initialised):
 *   - bound `$this` set (method first-class callable) -> [ $__this, $__fn ] array callable
 *   - `$__scope` set (static first-class callable)     -> [ $__scope, $__fn ] array callable
 *   - neither (plain function / real closure)          -> the `$__fn` name string
 * The 2-element array form rides the existing MEMOBJ_HASHMAP dispatch, which binds $this
 * for an object first element and resolves the class for a class-name-string first element.
 * Returns SXRET_OK if pVal was a Closure (pOut filled), SXERR_NOTFOUND otherwise.
 */
PH7_PRIVATE sxi32 VmClosureUnwrap(ph7_vm *pVm, ph7_value *pVal, ph7_value *pOut)
{
	ph7_class_instance *pThis;
	ph7_value *pFn;
	SyString sAttr;
	if( !VmValueIsClosure(pVm, pVal) ){
		return SXERR_NOTFOUND;
	}
	pThis = (ph7_class_instance *)pVal->x.pOther;
	SyStringInitFromBuf(&sAttr, "__fn", 4);
	pFn = PH7_ClassInstanceFetchAttr(pThis, &sAttr);
	if( pFn == 0 || (pFn->iFlags & MEMOBJ_STRING) == 0 || SyBlobLength(&pFn->sBlob) == 0 ){
		return SXERR_NOTFOUND; /* malformed/uninitialised closure */
	}
	/* Only a bound/static first-class callable (rare) carries $__this/$__scope; the
	 * VM_INSTANCE_FCC_BOUND flag (set by VmCreateClosure) keeps the hot plain-closure path
	 * to the single $__fn lookup above instead of two extra attribute lookups per dispatch. */
	if( pThis->iFlags & VM_INSTANCE_FCC_BOUND ){
		ph7_value *pBound, *pScope;
		int bBoundObj, bScope;
		SyStringInitFromBuf(&sAttr, "__this", 6);
		pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);
		SyStringInitFromBuf(&sAttr, "__scope", 7);
		pScope = PH7_ClassInstanceFetchAttr(pThis, &sAttr);
		bBoundObj = pBound && (pBound->iFlags & MEMOBJ_OBJ);
		bScope = pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0;
		if( bBoundObj || bScope ){
			/* Method/static first-class callable -> [ target, "method" ] array callable. */
			if( pThis->iFlags & VM_INSTANCE_FCC_INVOKE_OBJ ){
				/* Closure::fromCallable($obj): the engine named __invoke, so this
				 * dispatch is the engine's own and a non-public one still runs. */
				pVm->bMagicDispatch = 1;
			}
			ph7_hashmap *pMap;
			ph7_value sTarget, sMeth;
			sxi32 rc;
			if( bBoundObj ){
				ph7_class_instance *pBoundObj = (ph7_class_instance *)pBound->x.pOther;
				/* A bound PLAIN closure (`function(){…}->bindTo($o)`): $__fn names a function, not a
				 * method of the bound object's class, so a [obj,method] array callable would fail
				 * method resolution. Stash the bound object (own a ref) so the OP_CALL user-function
				 * frame setup injects it as $this, and return the plain $__fn string for a normal
				 * function dispatch. */
				if( (pThis->iFlags & VM_INSTANCE_FCC_METHOD) == 0
				 && PH7_ClassExtractMethod(pBoundObj->pClass,
						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0 ){
					/* Only a USER function (anonymous closure / named fn in hFunction) reads $this and
					 * reaches the OP_CALL user-function frame-setup that consumes pClosureThis. A HOST
					 * function (e.g. Closure::fromCallable('strlen')->bindTo($o)) ignores $this and
					 * dispatches via the host path which never consumes the transient — setting it
					 * there would leak the ref and inject a stale $this into the next call. */
					if( SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),
							SyBlobLength(&pFn->sBlob)) != 0 ){
						pBoundObj->iRef++;
						pVm->pClosureThis = pBoundObj;
						/* Carry the bound scope (if set) so private/protected member access inside
						 * the closure body resolves against it — bindTo($o, Scope::class) / call($o). */
						if( bScope ){
							pVm->pClosureScope = PH7_VmExtractClass(pVm,
								(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);
						}
					}
					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));
					return SXRET_OK;
				}
			}else{
				/* Scope-only rebind of a PLAIN closure (`bindTo(null, Scope::class)`):
				 * $__fn names a function, not a static method of the scope class, so
				 * the [scope, method] array callable below would fail method
				 * resolution. Dispatch the plain $__fn string and carry the scope for
				 * private/protected visibility (pClosureThis stays unset — no $this).
				 * A static-method FCC ($__fn really is a method of the scope class)
				 * falls through to the array-callable path. */
				ph7_class *pScopeClass = PH7_VmExtractClass(pVm,
					(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);
				if( pScopeClass == 0
				 || ((pThis->iFlags & VM_INSTANCE_FCC_METHOD) == 0
				  && PH7_ClassExtractMethod(pScopeClass,
						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) == 0) ){
					if( pScopeClass
					 && SyHashGet(&pVm->hFunction, (const void *)SyBlobData(&pFn->sBlob),
							SyBlobLength(&pFn->sBlob)) != 0 ){
						pVm->pClosureScope = pScopeClass;
					}
					PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));
					return SXRET_OK;
				}
			}
			pMap = PH7_NewHashmap(&(*pVm), 0, 0);
			if( pMap == 0 ){
				return SXERR_NOTFOUND;
			}
			PH7_MemObjInit(pVm, &sTarget);
			PH7_MemObjInit(pVm, &sMeth);
			if( bBoundObj ){
				PH7_MemObjStore(pBound, &sTarget); /* bound object (iRef++) -> binds $this */
			}else{
				ph7_value *pCalled;
				SyStringInitFromBuf(&sAttr, "__called", 8);
				pCalled = PH7_ClassInstanceFetchAttr(pThis, &sAttr);
				if( pCalled && (pCalled->iFlags & MEMOBJ_STRING) && SyBlobLength(&pCalled->sBlob) > 0 ){
					/* A forwarding `self::sf(...)`/`parent::sf(...)` (OP_LOAD_FCC): the call goes
					 * THROUGH the caller's late-static-binding class, so `static::` answers it,
					 * while the callee stays the one resolved in $__scope. */
					pVm->pClosureMethodCls = PH7_VmExtractClass(pVm,
						(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);
					PH7_MemObjStringAppend(&sTarget, SyBlobData(&pCalled->sBlob), SyBlobLength(&pCalled->sBlob));
				}else{
					PH7_MemObjStringAppend(&sTarget, SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob));
				}
			}
			PH7_MemObjStringAppend(&sMeth, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));
			rc = PH7_HashmapInsert(pMap, 0, &sTarget);
			if( rc == SXRET_OK ){
				rc = PH7_HashmapInsert(pMap, 0, &sMeth);
			}
			PH7_MemObjRelease(&sTarget);
			PH7_MemObjRelease(&sMeth);
			if( rc != SXRET_OK ){
				PH7_HashmapRelease(pMap, TRUE); /* free the partial map, no leak */
				return SXERR_NOTFOUND;
			}
			if( pThis->iFlags & VM_INSTANCE_FCC_SCREENED ){
				/* php resolves a method Closure's callee ONCE, where the closure is built, and
				 * keeps the resolved function: an escaped `$this->priv(...)` runs anywhere. PHL
				 * keeps only a NAME, so every dispatch site would re-decide visibility against the
				 * CALLER and refuse the closure php runs. The creation sites screen (OP_LOAD_FCC's
				 * VmFccMemberError, Closure::fromCallable's PH7_VmIsCallable gate, and reflection,
				 * which php lets past protection on purpose) and stamp the mark only when a real
				 * method answered — a name the class reaches through __call carries no mark and
				 * still routes to the catch-all. Armed only once the pair below really exists, so
				 * a failed build cannot leave it standing; the consumers clear it like
				 * pClosureThis/pClosureScope. */
				pVm->bClosureScreened = 1;
			}
			if( bBoundObj && bScope && (pThis->iFlags & VM_INSTANCE_FCC_METHOD) ){
				/* A method closure whose `$__scope` is not its receiver's class names the class
				 * its callee was RESOLVED in: `parent::m(...)` and `A::m(...)` with a `$this`,
				 * ReflectionMethod::getClosure(). php keeps that function, so the pair must not
				 * be looked up on the receiver again -- that found the receiver's own override. */
				ph7_class *pRecvCls = ((ph7_class_instance *)pBound->x.pOther)->pClass;
				ph7_class *pFromCls = PH7_VmExtractClass(pVm,
					(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);
				if( pFromCls && pFromCls != pRecvCls && PH7_VmInstanceOf(pRecvCls,pFromCls)
				 && PH7_ClassExtractMethod(pFromCls,
						(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob)) ){
					pVm->pClosureMethodCls = pFromCls;
				}
			}
			pOut->x.pOther = pMap;
			MemObjSetType(pOut, MEMOBJ_HASHMAP);
			return SXRET_OK;
		}
	}
	PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));
	return SXRET_OK;
}
/*
 * php's SCOPE for a Closure — the class whose private members its body may reach, and the
 * class Reflection reports. For a plain closure that is the class it was bound to; for a
 * METHOD closure it is the class that DECLARED the method, which is not the class the
 * callable NAMED: `(new Kid)->mk()` returning `$this->basePriv(...)` is scoped to Base in
 * php, while `$__scope` records Kid — the class the call goes THROUGH, which is the CALLED
 * scope (`static::`) and a different question. A trait method is composed into the using
 * class, so PH7_VmMethodScopeName answers for it exactly as it does at every refusal site.
 * Returns 0 when the closure carries no scope at all.
 */
PH7_PRIVATE ph7_class * PH7_VmClosureScopeClass(ph7_vm *pVm, ph7_class_instance *pClosure)
{
	SyString sAttr;
	ph7_value *pScope, *pFn;
	ph7_class *pClass;
	if( pClosure == 0 ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr, "__scope", 7);
	pScope = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);
	if( pScope == 0 || (pScope->iFlags & MEMOBJ_STRING) == 0 || SyBlobLength(&pScope->sBlob) == 0 ){
		return 0;
	}
	pClass = PH7_VmExtractClass(pVm, (const char *)SyBlobData(&pScope->sBlob),
		SyBlobLength(&pScope->sBlob), FALSE, 0);
	if( pClass == 0 || (pClosure->iFlags & VM_INSTANCE_FCC_METHOD) == 0 ){
		return pClass;
	}
	SyStringInitFromBuf(&sAttr, "__fn", 4);
	pFn = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);
	if( pFn && (pFn->iFlags & MEMOBJ_STRING) && SyBlobLength(&pFn->sBlob) > 0 ){
		ph7_class_method *pMeth = PH7_ClassExtractMethod(pClass,
			(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));
		if( pMeth ){
			return PH7_VmMethodScopeName(pVm, pClass, pMeth);
		}
		/* No method answers the name: php's call TRAMPOLINE, whose function is the
		 * catch-all itself, so its scope is the class that declared __callStatic (no
		 * receiver) or __call -- `B::miss(...)` is scoped to A when A declares it. */
		if( pClosure->iFlags & VM_INSTANCE_FCC_SCREENED ){
			return pClass;
		}
		pMeth = PH7_VmClosureIsStatic(pVm, pClosure)
			? PH7_ClassExtractMethod(pClass, "__callStatic", sizeof("__callStatic")-1)
			: PH7_ClassExtractMethod(pClass, "__call", sizeof("__call")-1);
		if( pMeth ){
			return PH7_VmMethodScopeName(pVm, pClass, pMeth);
		}
	}
	return pClass;
}
/*
 * Resolve the scope class for a STATIC first-class callable `T::m(...)`, where T is a
 * class-name STRING value: handles the self/static/parent keywords against the live class
 * context (so the actual class is bound at FCC-creation time, like PHP), and falls back to
 * an explicit class-name lookup. Mirrors the static OP_MEMBER resolution. Returns 0 if the
 * class cannot be resolved.
 */
PH7_PRIVATE ph7_class * VmFccResolveScope(ph7_vm *pVm, ph7_value *pTarget)
{
	return PH7_VmResolveScopeName(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),
		(sxu32)SyBlobLength(&pTarget->sBlob));
}
/*
 * The same resolution over a raw (name, length) pair, for the callable machinery: php
 * resolves `self`/`parent`/`static` in a CALLBACK (is_callable, call_user_func, array_map …)
 * against the live class context, and refuses them in the direct `$cb()` dispatch — so this
 * is deliberately NOT wired into the OP_CALL resolve check, which must keep answering
 * `Class "self" not found`.
 */
/*
 * A Closure object is going: give back its hold on the function `$__fn` names.
 * A run-time closure's per-instantiation ph7_vm_func belongs to the objects that
 * name it, and this is where the last of them lets go.
 */
static void VmClosureRelease(ph7_vm *pVm, ph7_class_instance *pThis)
{
	PH7_VmClosureInstanceRef(pVm, pThis, -1);
}
PH7_PRIVATE ph7_class * PH7_VmResolveScopeName(ph7_vm *pVm, const char *zCls, sxu32 nCls)
{
	ph7_class *pClass;
	if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){
		pClass = PH7_VmPeekSelfClass(&(*pVm)); /* self:: in a trait -> the USING class */
	}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){
		pClass = PH7_VmPeekTopClass(&(*pVm));     /* late static binding */
	}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){
		pClass = PH7_VmResolveParentClass(&(*pVm));
	}else{
		pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);
	}
	return pClass;
}
/*
 * Create a Closure object wrapping a callable name (+ optional bound $this object and/or
 * scope class-name, for the method/static first-class callables `$o->m(...)`/`C::m(...)`).
 * Mirrors the Generator/Fiber "object carries its state in private attributes" pattern.
 * Returns the fresh instance holding ONE reference, which the caller's value TAKES --
 * the same handover PH7_NewClassInstance makes to OP_NEW. (It used to say the caller
 * had to add one, and every closure site did, so no Closure object ever reached zero.)
 */
PH7_PRIVATE ph7_class_instance * VmCreateClosure(ph7_vm *pVm, const SyString *pName,
	ph7_class_instance *pBoundThis, const SyString *pScope)
{
	ph7_class_instance *pObj;
	ph7_value *pAttr;
	SyString sAttr;
	if( pVm->pClosureClass == 0 ){
		return 0;
	}
	pObj = PH7_NewClassInstance(&(*pVm), pVm->pClosureClass);
	if( pObj == 0 ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr, "__fn", 4);
	pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);
	if( pAttr ){
		PH7_MemObjStringAppend(pAttr, pName->zString, pName->nByte);
		/* This object is now a holder of the function it names. For a run-time
		 * closure that is what keeps the per-instantiation copy alive, and losing
		 * it is what frees the copy. */
		PH7_VmClosureInstanceRef(pVm, pObj, 1);
	}
	if( pBoundThis ){
		SyStringInitFromBuf(&sAttr, "__this", 6);
		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);
		if( pAttr ){
			pAttr->x.pOther = pBoundThis;
			MemObjSetType(pAttr, MEMOBJ_OBJ);
			pBoundThis->iRef++; /* keep the bound object alive for the closure's lifetime */
		}
	}
	if( pScope && pScope->nByte ){
		SyStringInitFromBuf(&sAttr, "__scope", 7);
		pAttr = PH7_ClassInstanceFetchAttr(pObj, &sAttr);
		if( pAttr ){
			PH7_MemObjStringAppend(pAttr, pScope->zString, pScope->nByte);
		}
	}
	if( pBoundThis || (pScope && pScope->nByte) ){
		/* Mark bound/static FCC closures so VmClosureUnwrap can skip the $__this/$__scope
		 * lookups on the hot plain-closure dispatch path. */
		pObj->iFlags |= VM_INSTANCE_FCC_BOUND;
	}
	return pObj;
}
/*
 * Exported wrapper around VmCreateClosure for builtin libraries outside this
 * file (ReflectionFunction::getClosure / ReflectionMethod::getClosure).
 */
PH7_PRIVATE ph7_class_instance * PH7_VmNewClosure(ph7_vm *pVm, const SyString *pName,
	ph7_class_instance *pBoundThis, const SyString *pScope)
{
	return VmCreateClosure(&(*pVm), pName, pBoundThis, pScope);
}
/*
 * Exported wrapper around the typed/readonly property store enforcement for
 * ReflectionProperty::setValue (vm_builtin_reflection.c). Same contract:
 * SXRET_OK (value possibly coerced in place), PH7_EXCEPTION, or PH7_ABORT.
 */
PH7_PRIVATE sxi32 PH7_VmEnforcePropStore(ph7_vm *pVm,sxu32 nIdx,ph7_value *pValue)
{
	return VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pValue,0);
}
/*
 * Exported reference-table probe for ReflectionReference::fromArrayElement
 * (vm_builtin_reflection.c). Returns the number of links (frame variables +
 * array entries) attached to the slot's reference record, 0 when the slot
 * has none — an array element is a PHP reference when this is >= 2.
 */
PH7_PRIVATE int PH7_VmSlotRefCount(ph7_vm *pVm,sxu32 nIdx)
{
	return (int)(PH7_VmSlotEntryCount(&(*pVm),nIdx) + PH7_VmSlotNodeCount(&(*pVm),nIdx));
}
/*
 * First-class callable over an arbitrary callable VALUE: `($expr)(...)`.
 * Normalize a validated callable value into a fresh Closure, reusing VmCreateClosure (the same
 * object the method/static first-class-callable paths mint, so dispatch round-trips identically
 * via VmClosureUnwrap). Handles the three remaining callable shapes a value can hold:
 *   - a function-NAME string          -> plain closure ($__fn = name)
 *   - a [target, method] array callable -> bound (object target -> $this) / static (class-name
 *     target -> scope) closure, mirroring the [obj,m]/[class,m] decode in PH7_VmIsCallable
 *   - an __invoke object               -> closure bound to the object's __invoke
 * An existing Closure returns 0 here (it is already a Closure — the caller keeps it as-is), so this
 * stays idempotent even for a direct caller. Returns the fresh instance holding ONE reference,
 * which the caller's value TAKES (see VmCreateClosure) or 0 if the value is an existing Closure / not a normalizable callable / on OOM — in
 * which case the caller leaves the value untouched (graceful degradation). This is the generic
 * "callable value -> Closure" primitive: the body is FCC-agnostic and self-contained, so the future
 * Closure::bind/fromCallable work (Increment 2) can call it directly.
 */
/*
 * A closure over a method named through a CLASS (`'C::m'`, `[C, 'm']`). With bBindCaller --
 * Closure::fromCallable() -- php binds what zend_is_callable_ex resolves at creation, against
 * the calling frame: a non-static method keeps the caller's `$this` when that is an instance
 * of C, as `C::m(...)` does, so the closure still runs on it once it leaves the method that
 * made it; and whenever such a `$this` exists the called class is ITS class, which a static
 * method sees as `static::`. The string spelling never reaches __call: php resolves its
 * trampoline through the static-method lookup from fromCallable's own frame, which has no
 * `$this`, so a name only a catch-all answers is __callStatic there, unbound. A dynamic
 * `$v(...)` binds nothing (bBindCaller FALSE).
 */
static ph7_class_instance * VmFccClassClosure(ph7_vm *pVm, ph7_class *pCls, SyString *pName,
	int bBindCaller, int bStrForm, int bForward)
{
	ph7_class *pCalled;
	int bDirect = PH7_VmFccMethodIsDirect(pVm, pCls, SyStringData(pName), SyStringLength(pName));
	ph7_class_instance *pThis = bBindCaller ? PH7_VmCallerThisFor(pVm, pCls) : 0;
	ph7_class_instance *pRecv = 0, *pFccObj;
	if( pThis && (bDirect || !bStrForm) ){
		pRecv = PH7_VmFccClassReceiver(pVm, pCls, SyStringData(pName), SyStringLength(pName));
	}
	pFccObj = VmCreateClosure(pVm, pName, pRecv, &pCls->sName);
	if( pFccObj == 0 ){
		return 0;
	}
	pFccObj->iFlags |= VM_INSTANCE_FCC_METHOD; /* $__fn is a METHOD name */
	if( bDirect ){
		pFccObj->iFlags |= VM_INSTANCE_FCC_SCREENED;
	}
	/* `self::`/`parent::` FORWARD the called class even with no `$this`: from a static
	 * D::st() inherited from B, `'self::s'` runs with static:: = D, where `'B::s'` spelled
	 * out runs with B. */
	pCalled = pThis ? pThis->pClass : (bBindCaller && bForward ? PH7_VmPeekTopClass(pVm) : 0);
	if( pRecv == 0 && pCalled && pCalled != pCls ){
		SyString sAttr;
		ph7_value *pAttr;
		SyStringInitFromBuf(&sAttr, "__called", 8);
		pAttr = PH7_ClassInstanceFetchAttr(pFccObj, &sAttr);
		if( pAttr ){
			PH7_MemObjStringAppend(pAttr, SyStringData(&pCalled->sName),
				SyStringLength(&pCalled->sName));
		}
	}
	return pFccObj;
}
/* Is a callable's class half one of the two keywords that forward the called class? */
static int VmFccNameForwards(const char *zCls, sxu32 nCls)
{
	return (nCls == 4 && SyStrnicmp(zCls, "self", 4) == 0)
		|| (nCls == 6 && SyStrnicmp(zCls, "parent", 6) == 0);
}
/*
 * The class a callable's target half names. A NAME is resolved the way the callback
 * machinery resolves it -- `self`/`parent`/`static` against the calling frame, as
 * PH7_VmIsCallable's gate already did -- so `'self::m'` and `['parent','m']` wrap the
 * method they validated as. A plain class lookup found no class called `self` and left
 * the whole string as a function name, which died `Class "self" not found` at the call.
 * Only Closure::fromCallable() (bBindCaller) resolves them: `$cb(...)` is the direct
 * dispatch, which php refuses the keywords in.
 */
static ph7_class * VmFccTargetClass(ph7_vm *pVm, ph7_value *pTarget, int bBindCaller)
{
	if( pTarget->iFlags & MEMOBJ_OBJ ){
		return ((ph7_class_instance *)pTarget->x.pOther)->pClass;
	}
	if( !bBindCaller || (pTarget->iFlags & MEMOBJ_STRING) == 0 || SyBlobLength(&pTarget->sBlob) == 0 ){
		return PH7_VmExtractClassFromValue(pVm, pTarget);
	}
	return PH7_VmResolveCallableScope(pVm, (const char *)SyBlobData(&pTarget->sBlob),
		SyBlobLength(&pTarget->sBlob));
}
PH7_PRIVATE ph7_class_instance * VmFccWrapValue(ph7_vm *pVm, ph7_value *pValue, int bBindCaller)
{
	/* A Closure is already callable; never double-wrap it (this also stops the __invoke branch
	 * below from binding a closure to its OWN __invoke). The OP_LOAD_FCC caller intercepts a
	 * Closure first too, but guarding here keeps the primitive safe for a direct Increment-2 caller. */
	if( VmValueIsClosure(pVm, pValue) ){
		return 0;
	}
	if( !PH7_VmIsCallable(pVm, pValue, TRUE) ){
		return 0;
	}
	if( pValue->iFlags & MEMOBJ_STRING ){
		SyString sName;
		const char *zName = (const char *)SyBlobData(&pValue->sBlob);
		sxu32 nName = SyBlobLength(&pValue->sBlob), nSep;
		/* `"C::m"` is the SAME callable as `[C, 'm']`, and php mints the same
		 * closure for it: scope C, name m. PHL kept the whole string as the
		 * function name, so the Closure ran (dispatch splits it) but described
		 * itself as nothing -- ReflectionFunction over it had no name, no scope
		 * and no parameters, and a library that reflects a callback before
		 * calling it (twig compiles every filter that way) died on the read. */
		for( nSep = 0 ; nSep + 1 < nName ; ++nSep ){
			if( zName[nSep] == ':' && zName[nSep+1] == ':' ){
				break;
			}
		}
		if( nSep + 1 < nName ){
			ph7_class *pScopeCls;
			SyString sCls;
			SyStringInitFromBuf(&sCls, zName, nSep);
			pScopeCls = bBindCaller
				? PH7_VmResolveCallableScope(pVm, SyStringData(&sCls), SyStringLength(&sCls))
				: PH7_VmExtractClass(pVm, SyStringData(&sCls), SyStringLength(&sCls), FALSE, 0);
			if( pScopeCls ){
				SyStringInitFromBuf(&sName, zName + nSep + 2, nName - (nSep + 2));
				return VmFccClassClosure(pVm, pScopeCls, &sName, bBindCaller, TRUE,
					VmFccNameForwards(SyStringData(&sCls), SyStringLength(&sCls)));
			}
		}
		SyStringInitFromBuf(&sName, zName, nName);
		return VmCreateClosure(pVm, &sName, 0, 0);
	}
	if( pValue->iFlags & MEMOBJ_HASHMAP ){
		/* [target, method] — the same index-0/1 decode PH7_VmIsCallable uses to validate it
		 * (php reads the INTEGER indices, not insertion order). */
		ph7_hashmap *pMap = (ph7_hashmap *)pValue->x.pOther;
		ph7_value *pTarget, *pMeth;
		SyString sName;
		if( !PH7_VmArrayCallableParts(pVm, pMap, &pTarget, &pMeth) ){
			return 0;
		}
		if( (pMeth->iFlags & MEMOBJ_STRING) == 0 || SyBlobLength(&pMeth->sBlob) == 0 ){
			return 0;
		}
		SyStringInitFromBuf(&sName, SyBlobData(&pMeth->sBlob), SyBlobLength(&pMeth->sBlob));
		{
			/* A qualified method half (`[$b,'A::f']`) names the class the closure is scoped
			 * to: the closure carries the method half and that class, so it runs A::f on $b
			 * rather than looking `A::f` up as a method name of B's. */
			ph7_class *pOrg = VmFccTargetClass(pVm, pTarget, bBindCaller);
			ph7_class *pQual = 0;
			const char *zQCls = 0, *zQMeth = 0, *zWhy = 0;
			sxu32 nQCls = 0, nQMeth = 0;
			char zWhyBuf[128];
			int rcQual = pOrg ? PH7_VmQualifiedCallableMethod(pVm, pOrg, sName.zString, sName.nByte,
				&pQual, &zQCls, &nQCls, &zQMeth, &nQMeth, zWhyBuf, (int)sizeof(zWhyBuf), &zWhy) : 0;
			if( rcQual < 0 ){
				return 0;
			}
			if( rcQual > 0 ){
				ph7_class_instance *pFccObj;
				SyStringInitFromBuf(&sName, zQMeth, nQMeth);
				pFccObj = VmCreateClosure(pVm, &sName,
					(pTarget->iFlags & MEMOBJ_OBJ) ? (ph7_class_instance *)pTarget->x.pOther : 0,
					&pQual->sName);
				if( pFccObj ){
					pFccObj->iFlags |= VM_INSTANCE_FCC_METHOD; /* $__fn is a METHOD name */
					if( PH7_VmFccMethodIsDirect(pVm, pQual, zQMeth, nQMeth) ){
						pFccObj->iFlags |= VM_INSTANCE_FCC_SCREENED;
					}
				}
				return pFccObj;
			}
		}
		if( pTarget->iFlags & MEMOBJ_OBJ ){
			ph7_class_instance *pBoundThis = (ph7_class_instance *)pTarget->x.pOther;
			ph7_class_instance *pFccObj = VmCreateClosure(pVm, &sName, pBoundThis,
				&pBoundThis->pClass->sName);
			if( pFccObj ){
				pFccObj->iFlags |= VM_INSTANCE_FCC_METHOD; /* $__fn is a METHOD name */
				if( PH7_VmFccMethodIsDirect(pVm,pBoundThis->pClass,
						SyStringData(&sName),SyStringLength(&sName)) ){
					/* The PH7_VmIsCallable gate above is php's creation-time screen; a pair it
					 * admitted because the class routes the name through __call is NOT settled. */
					pFccObj->iFlags |= VM_INSTANCE_FCC_SCREENED;
				}
			}
			return pFccObj;
		}else{
			/* [class-name, method] static callable -> bind the resolved scope. */
			ph7_class *pScopeCls = VmFccTargetClass(pVm, pTarget, bBindCaller);
			return pScopeCls ? VmFccClassClosure(pVm, pScopeCls, &sName, bBindCaller, FALSE,
				(pTarget->iFlags & MEMOBJ_STRING) && VmFccNameForwards(
					(const char *)SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob))) : 0;
		}
	}
	if( pValue->iFlags & MEMOBJ_OBJ ){
		/* __invoke object (a real Closure is intercepted by the caller before this point).
		 * The `__invoke` name is the ENGINE's, so mark the closure: php dispatches a
		 * non-public __invoke through this wrapper exactly as it does through `$obj()`. */
		ph7_class_instance *pObj = (ph7_class_instance *)pValue->x.pOther;
		ph7_class_instance *pWrap;
		SyString sInvoke;
		SyStringInitFromBuf(&sInvoke, "__invoke", sizeof("__invoke") - 1);
		pWrap = VmCreateClosure(pVm, &sInvoke, pObj, &pObj->pClass->sName);
		if( pWrap ){
			pWrap->iFlags |= VM_INSTANCE_FCC_INVOKE_OBJ;
		}
		return pWrap;
	}
	/* Unreachable in practice — the PH7_VmIsCallable gate admits only string/array/object, all
	 * handled above; kept to satisfy the non-void return path. */
	return 0;
}
/*
 * Return a fresh Closure instance (iRef==0 from create/clone) as a builtin result, taking the
 * one reference into pCtx->pRet — mirrors the OP_LOAD_FCC store convention.
 */
static int VmClosureResult(ph7_context *pCtx, ph7_class_instance *pClosure)
{
	if( pClosure == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjRelease(pCtx->pRet);
	/* Every caller hands over a FRESH instance, whose own reference is the one this
	 * return value takes (see OP_LOAD_CLOSURE). */
	pCtx->pRet->x.pOther = pClosure;
	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);
	return PH7_OK;
}
/*
 * Overwrite a Closure clone's $__this (bound object, or 0 to unbind) and, when pScope != 0, its
 * $__scope (the class-name string; pScope->nByte==0 clears it). pScope==0 leaves $__scope as the
 * clone inherited it (PHP's "static" = keep-scope). Refreshes VM_INSTANCE_FCC_BOUND from the result.
 * The clone inherited a ref on the original $__this (PH7_CloneClassInstance copies via MemObjStore);
 * this drops that and takes one on pNewThis.
 */
static void VmClosureRebind(ph7_class_instance *pClone,
	ph7_class_instance *pNewThis, const SyString *pScope)
{
	SyString sAttr;
	ph7_value *pThisAttr, *pScopeAttr;
	int bBound = 0;
	SyStringInitFromBuf(&sAttr, "__this", 6);
	pThisAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);
	if( pThisAttr ){
		/* PH7_MemObjRelease already drops the cloned-in object's refcount for an OBJ value —
		 * do NOT also decrement by hand (that double-frees the original bound object). */
		PH7_MemObjRelease(pThisAttr);
		if( pNewThis ){
			pThisAttr->x.pOther = pNewThis;
			MemObjSetType(pThisAttr, MEMOBJ_OBJ);
			pNewThis->iRef++;
		}
	}
	if( pScope ){
		SyStringInitFromBuf(&sAttr, "__scope", 7);
		pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);
		if( pScopeAttr ){
			PH7_MemObjRelease(pScopeAttr);
			if( pScope->nByte ){
				PH7_MemObjStringAppend(pScopeAttr, pScope->zString, pScope->nByte);
			}
		}
		/* A new scope is a new called class too: the forwarded one no longer applies. */
		SyStringInitFromBuf(&sAttr, "__called", 8);
		pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);
		if( pScopeAttr ){
			PH7_MemObjRelease(pScopeAttr);
		}
	}
	/* Refresh the FCC_BOUND fast-path flag from the resulting $__this/$__scope. pThisAttr already
	 * points at the final $__this slot; only $__scope needs a (re)fetch — the top half fetched it
	 * just for pScope != 0. */
	SyStringInitFromBuf(&sAttr, "__scope", 7);
	pScopeAttr = PH7_ClassInstanceFetchAttr(pClone, &sAttr);
	if( (pThisAttr && (pThisAttr->iFlags & MEMOBJ_OBJ))
		|| (pScopeAttr && (pScopeAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeAttr->sBlob) > 0) ){
		bBound = 1;
	}
	if( bBound ){
		pClone->iFlags |= VM_INSTANCE_FCC_BOUND;
	}else{
		pClone->iFlags &= ~VM_INSTANCE_FCC_BOUND;
	}
}
/*
 * Resolve the bindTo/bind/call $scope argument to a class-name SyString.
 * Returns 1 and fills *pOut if $__scope should be replaced (pOut->nByte==0 means "clear");
 * returns 0 to leave the scope unchanged (the PHP "static" sentinel / scope omitted).
 */
static int VmClosureResolveScope(ph7_value *pScopeArg, SyString *pOut)
{
	if( pScopeArg == 0 ){
		return 0; /* keep */
	}
	if( (pScopeArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScopeArg->sBlob) == 6
		&& SyMemcmp((const void *)SyBlobData(&pScopeArg->sBlob), (const void *)"static", 6) == 0 ){
		return 0; /* "static" -> keep current scope */
	}
	if( pScopeArg->iFlags & MEMOBJ_NULL ){
		SyStringInitFromBuf(pOut, 0, 0); /* unscoped */
		return 1;
	}
	if( pScopeArg->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pScopeObj = (ph7_class_instance *)pScopeArg->x.pOther;
		*pOut = pScopeObj->pClass->sName;
		return 1;
	}
	if( (pScopeArg->iFlags & MEMOBJ_STRING) == 0 && (pScopeArg->iFlags & MEMOBJ_SCALAR) ){
		/* php declares `object|string|null $newScope` and coerces a scalar into it in weak
		 * mode, so `bindTo($o, 5)` reaches the lookup as the NAME "5" and warns that no such
		 * class exists. Falling through as "keep the current scope" bound it silently. */
		PH7_MemObjToString(pScopeArg);
	}
	if( pScopeArg->iFlags & MEMOBJ_STRING ){
		SyStringInitFromBuf(pOut, (const char *)SyBlobData(&pScopeArg->sBlob), SyBlobLength(&pScopeArg->sBlob));
		return 1;
	}
	return 0;
}
/*
 * Fiber::getCurrent() — the fiber the running code is INSIDE, or null in the main
 * flow. php answers EG(active_fiber), which is the fiber whose body is on the
 * stack, not the innermost coroutine: a GENERATOR iterated from inside a fiber
 * leaves the fiber current, and code running after a fiber suspends back to its
 * caller is outside it again. pVm->pCurFiber is that name, saved and restored
 * around every start/resume, so the nesting is the call structure itself.
 *
 * Reached by every library that logs or schedules per-fiber: monolog asks for it
 * on EVERY record it writes.
 */
PH7_PRIVATE int vm_builtin_Fiber_getCurrent(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	SXUNUSED(apArg);
	SXUNUSED(nArg);
	if( pVm->pCurFiber == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* The result slot takes a reference of its own: pCurFiber is borrowed from the
	 * activation that set it, and the value handed back outlives that call. */
	PH7_MemObjRelease(pCtx->pRet);
	pVm->pCurFiber->iRef++;
	pCtx->pRet->x.pOther = pVm->pCurFiber;
	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);
	return PH7_OK;
}
/*
 * Fiber's C-bodied methods. Every one of them was a global `__fiber_verb($this,…)`
 * thunk that a one-line prelude method forwarded to; the class body in the builtin
 * chunk now holds only its two private slots.
 */
PH7_PRIVATE sxi32 PH7_VmInstallFiberNative(ph7_vm *pVm)
{
	/* php's own declaration ORDER, which is what get_class_methods() and
	 * ReflectionClass::getMethods() answer in. One row still differs from php's
	 * list, and is recorded: `__destruct` is the engine's teardown hook
	 * published as a method name php does not have (Generator's is the twin). */
	static const PH7_NativeMethodDef aMethod[] = {
		{ "__construct",  PH7_MOD_PUBLIC, "callable $callback",  "",       vm_builtin_Fiber_construct },
		/* Variadic: the arguments now reach the C body directly instead of being
		 * repackaged by a func_get_args() call in the prelude. */
		{ "start",        PH7_MOD_PUBLIC, "mixed ...$args",      "mixed",  vm_builtin_Fiber_start },
		{ "resume",       PH7_MOD_PUBLIC, "mixed $value = null", "mixed",  vm_builtin_Fiber_resume },
		{ "throw",        PH7_MOD_PUBLIC, "Throwable $exception", "mixed", vm_builtin_Fiber_throw },
		{ "isStarted",    PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isStarted },
		{ "isSuspended",  PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isSuspended },
		{ "isRunning",    PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isRunning },
		{ "isTerminated", PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isTerminated },
		{ "getReturn",    PH7_MOD_PUBLIC, "",                    "mixed",  vm_builtin_Fiber_getReturn },
		/* Static like suspend, and the one method a program calls without holding a
		 * fiber at all — it is how code asks whether it is inside one. */
		{ "getCurrent",   PH7_MOD_PUBLIC|PH7_MOD_STATIC, "",                    "?Fiber",
		  vm_builtin_Fiber_getCurrent },
		/* Static, and the only one that never took a receiver even as a thunk:
		 * `__fiber_suspend($value)` already read the value from argument #0. */
		{ "suspend",      PH7_MOD_PUBLIC|PH7_MOD_STATIC, "mixed $value = null", "mixed",
		  vm_builtin_Fiber_suspend },
		{ "__destruct",   PH7_MOD_PUBLIC, "",                    "",       vm_builtin_Fiber_destruct },
	};
	/* The two private slots the methods above keep their state in: the execution
	 * context (a resource) and the callable handed to the constructor. */
	static const PH7_NativePropDef aProp[] = {
		{ "__ctx",      PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ "__callable", PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeClassSpec sSpec = {
		"Fiber", 0, 0, PH7_CLASS_FINAL|PH7_CLASS_NOSERIALIZE,
		aMethod, SX_ARRAYSIZE(aMethod),
		0, 0,
		aProp, SX_ARRAYSIZE(aProp),
		0, 0, 0
	};
	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);
}
/*
 * Generator's C-bodied methods, plus the `implements Iterator` the chunk can no
 * longer carry: PH7_ClassImplement installs an abstract stub for any interface
 * method the class does not already declare, so it has to run AFTER the eight
 * methods below exist — at which point the stubs are skipped and the class is
 * concrete, exactly as the prelude declaration used to make it.
 */
PH7_PRIVATE sxi32 PH7_VmInstallGeneratorNative(ph7_vm *pVm)
{
	static const PH7_NativeMethodDef aMethod[] = {
		{ "current",    PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_current },
		{ "key",        PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_key },
		{ "next",       PH7_MOD_PUBLIC, "",                 "void",  vm_builtin_Generator_next },
		{ "rewind",     PH7_MOD_PUBLIC, "",                 "void",  vm_builtin_Generator_rewind },
		{ "valid",      PH7_MOD_PUBLIC, "",                 "bool",  vm_builtin_Generator_valid },
		/* php REQUIRES the argument here; the prelude declared `$value = null`, so
		 * `$gen->send()` used to answer the first yielded value instead of raising. */
		{ "send",       PH7_MOD_PUBLIC, "mixed $value",     "mixed", vm_builtin_Generator_send },
		{ "throw",      PH7_MOD_PUBLIC, "Throwable $exception", "mixed", vm_builtin_Generator_throw },
		{ "getReturn",  PH7_MOD_PUBLIC, "",                 "mixed", vm_builtin_Generator_getReturn },
		{ "__destruct", PH7_MOD_PUBLIC, "",                 "",      vm_builtin_Generator_destruct },
	};
	static const PH7_NativePropDef aProp[] = {
		{ "__ctx", PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "Generator", 0, 0, PH7_CLASS_FINAL|PH7_CLASS_NOINSTANTIATE|PH7_CLASS_NOSERIALIZE,
		  aMethod, SX_ARRAYSIZE(aMethod),
		  0, 0,
		  aProp, SX_ARRAYSIZE(aProp),
		  0, 0, 0 },
		/* php declares this one beside Generator and throws it from nowhere a
		 * script can reach: it is the exception a RESUME of a generator that has
		 * been closed would carry, and php's own paths answer null there. A
		 * program may still name it, catch it and throw it, so it is declared. */
		{ "ClosedGeneratorException", "Exception", 0, 0,
		  0, 0, 0, 0, 0, 0, 0, 0, 0 },
	};
	ph7_class *pClass;
	ph7_class *pIterator;
	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),aSpec,SX_ARRAYSIZE(aSpec));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* `implements Iterator` last, for the reason in this function's header. */
	pClass = PH7_VmExtractClass(&(*pVm),"Generator",sizeof("Generator")-1,0,0);
	if( pClass ){
		/* php refuses `new Generator` -- one only ever comes out of a call to a
		 * function that yields -- and words the refusal per class. */
		pClass->zNewRefusal = "The \"Generator\" class is reserved for internal use "
			"and cannot be manually instantiated";
	}
	pIterator = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,0,0);
	if( pClass == 0 || pIterator == 0 ){
		return SXERR_NOTFOUND;
	}
	return PH7_ClassImplement(pClass,pIterator);
}
/*
 * Closure::__construct() — php declares it PRIVATE and still words the refusal as
 * an instantiation error rather than a visibility one, so the body has to exist.
 */
PH7_PRIVATE int vm_builtin_Closure_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return PH7_VmThrowException(pCtx, "Error",
		"Instantiation of class Closure is not allowed");
}
/*
 * Clone a Closure for a rebind, carrying the marks that say WHAT it wraps. They live on the
 * instance rather than in an attribute, so PH7_CloneClassInstance — which copies attributes —
 * left them behind: the clone of a method callable forgot that its `$__fn` names a screened
 * METHOD, and `$fcc->bindTo($other)` came back as a closure whose every dispatch re-resolved
 * the name and re-decided its visibility (or, with no method of that name in reach, looked
 * for a global FUNCTION).
 */
static ph7_class_instance * VmCloneClosureInstance(ph7_class_instance *pClosure)
{
	ph7_class_instance *pClone = PH7_CloneClassInstance(pClosure);
	if( pClone ){
		pClone->iFlags |= pClosure->iFlags
			& (VM_INSTANCE_FCC_METHOD|VM_INSTANCE_FCC_SCREENED|VM_INSTANCE_FCC_INVOKE_OBJ
			   |VM_INSTANCE_FCC_SYNTAX);
	}
	return pClone;
}
/*
 * Is this closure STATIC — one that can never take a `$this`? For a plain closure that is the
 * `static function(){}` declaration flag; for a METHOD callable it is the method's own
 * staticness, which the flag cannot see because `$__fn` names a method and not a function in
 * hFunction. `Base::stat(...)` is exactly as static as `static fn()` to php.
 *
 * A method callable that resolved to the class's catch-all rather than a method (no SCREENED
 * mark) is php's call TRAMPOLINE, and php makes the `__callStatic` one static: the name it
 * wraps is not a method to ask, and the only thing that decided between the two catch-alls
 * was whether a receiver was bound. So `A::missing(...)` from outside A is static, and
 * `Closure::bind($it, null, A::class)` keeps it where the method rule refused the unbind.
 */
PH7_PRIVATE int PH7_VmClosureIsStatic(ph7_vm *pVm, ph7_class_instance *pClosure)
{
	SyString sAttr;
	ph7_value *pFn;
	SyStringInitFromBuf(&sAttr, "__fn", 4);
	pFn = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);
	if( pFn == 0 || (pFn->iFlags & MEMOBJ_STRING) == 0 || SyBlobLength(&pFn->sBlob) == 0 ){
		return 0;
	}
	if( (pClosure->iFlags & (VM_INSTANCE_FCC_METHOD|VM_INSTANCE_FCC_SCREENED)) == VM_INSTANCE_FCC_METHOD ){
		ph7_value *pThis;
		SyStringInitFromBuf(&sAttr, "__this", 6);
		pThis = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);
		return (pThis == 0 || (pThis->iFlags & MEMOBJ_OBJ) == 0) ? 1 : 0;
	}
	if( pClosure->iFlags & VM_INSTANCE_FCC_METHOD ){
		ph7_class *pScope = PH7_VmClosureScopeClass(pVm, pClosure);
		ph7_class_method *pMeth = pScope
			? PH7_ClassExtractMethod(pScope, (const char *)SyBlobData(&pFn->sBlob),
				SyBlobLength(&pFn->sBlob)) : 0;
		return (pMeth && (pMeth->iFlags & PH7_CLASS_ATTR_STATIC)) ? 1 : 0;
	}
	{
		SyHashEntry *pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob),
			SyBlobLength(&pFn->sBlob));
		return (pEntry && (((ph7_vm_func *)pEntry->pUserData)->iFlags & VM_FUNC_STATIC_CL)) ? 1 : 0;
	}
}
/*
 * Does this plain closure HAVE a `$this` and a body that names it? php refuses to unbind
 * exactly that pair; a closure that holds a `$this` it never reads drops it silently. The
 * receiver is in one of two places: `$__this` once a rebind handed it one, else the capture
 * OP_LOAD_CLOSURE took from the method that created it.
 */
static int VmClosureUsesBoundThis(ph7_vm *pVm, ph7_class_instance *pClosure)
{
	SyString sAttr;
	ph7_value *pVal;
	SyHashEntry *pEntry;
	ph7_vm_func *pFunc;
	ph7_vm_func_closure_env *aEnv;
	sxu32 n;
	SyStringInitFromBuf(&sAttr, "__fn", 4);
	pVal = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_STRING) == 0 || SyBlobLength(&pVal->sBlob) == 0 ){
		return 0;
	}
	pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pVal->sBlob), SyBlobLength(&pVal->sBlob));
	if( pEntry == 0 ){
		return 0;
	}
	pFunc = (ph7_vm_func *)pEntry->pUserData;
	if( (pFunc->iFlags & VM_FUNC_USES_THIS) == 0 ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr, "__this", 6);
	pVal = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);
	if( pVal && (pVal->iFlags & MEMOBJ_OBJ) ){
		return 1;
	}
	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);
	for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; n++ ){
		if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1
			&& SyMemcmp(SyStringData(&aEnv[n].sName),"this",sizeof("this")-1) == 0 ){
			return (aEnv[n].sValue.iFlags & MEMOBJ_OBJ) ? 1 : 0;
		}
	}
	return 0;
}
/*
 * php's four refusals for a rebind (zend_valid_closure_binding), in php's order. A closure
 * created from a METHOD is a "fake closure": it wraps a resolved function, so its `$this` may
 * only move WITHIN the class that declared it and its scope may not move at all. PHL applied
 * none of them beyond the static-closure one, so `$fcc->bindTo($unrelated)` handed back a
 * closure that could only fail later, and `->bindTo(null)` one with no receiver for a method
 * that needs one. Each refusal is php's E_WARNING plus a NULL result; returns 0 when it fired.
 *
 * pScope is the resolved $scope ARGUMENT (0 when omitted or `"static"`, which both mean keep);
 * an empty one is an explicit `null`, which php counts as a rebind like any other.
 */
static int VmClosureBindAllowed(ph7_vm *pVm, ph7_class_instance *pClosure,
	ph7_class_instance *pNewThis, const SyString *pScope)
{
	int bMethod = (pClosure->iFlags & VM_INSTANCE_FCC_METHOD) != 0;
	ph7_class *pOwn = bMethod ? PH7_VmClosureScopeClass(pVm, pClosure) : 0;
	if( pNewThis ){
		if( PH7_VmClosureIsStatic(pVm, pClosure) ){
			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,
				"Cannot bind an instance to a static closure, this will be an error in PHP 9");
			return 0;
		}
		if( pOwn && !PH7_VmInstanceOf(pNewThis->pClass, pOwn) ){
			SyString sFn;
			ph7_value *pFn;
			SyStringInitFromBuf(&sFn, "__fn", 4);
			pFn = PH7_ClassInstanceFetchAttr(pClosure, &sFn);
			SyStringInitFromBuf(&sFn, pFn ? (const char *)SyBlobData(&pFn->sBlob) : "",
				pFn ? SyBlobLength(&pFn->sBlob) : 0);
			VmErrorFormat(pVm,PH7_CTX_WARNING,
				"Cannot bind method %z::%z() to object of class %z, this will be an error in PHP 9",
				&pOwn->sDisp,&sFn,&pNewThis->pClass->sDisp);
			return 0;
		}
	}else if( pOwn && !PH7_VmClosureIsStatic(pVm, pClosure) ){
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,
			"Cannot unbind $this of method, this will be an error in PHP 9");
		return 0;
	}else if( !bMethod && VmClosureUsesBoundThis(pVm, pClosure) ){
		PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,
			"Cannot unbind $this of closure using $this, this will be an error in PHP 9");
		return 0;
	}
	if( pScope && bMethod ){
		ph7_class *pWant = pScope->nByte
			? PH7_VmExtractClass(pVm, pScope->zString, pScope->nByte, FALSE, 0) : 0;
		if( pWant != pOwn ){
			PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,
				"Cannot rebind scope of closure created from method, this will be an error in PHP 9");
			return 0;
		}
	}
	return 1;
}
/*
 * Closure::call(object $newThis, mixed ...$args) — bind and invoke in one step.
 *
 * This was the last PHP left in the class: `$bound = $this->bindTo($newThis,
 * get_class($newThis)); return $bound(...$args);`. That spelling leaked its own
 * internals — a non-object argument reported `get_class(): Argument #1 ($object)
 * must be of type object, string given` where php names THIS method's parameter,
 * which is what declaring `object $newThis` buys (the shared screen words it).
 * The scope php binds is the new $this's class, exactly as the PHP did.
 */
PH7_PRIVATE int vm_builtin_Closure_call(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pClosure, *pNewThis, *pClone;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	ph7_value sBound;
	SyString sScope;
	sxi32 rc;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx, "ArgumentCountError",
			"Closure::call() expects at least 1 argument, 0 given");
	}
	if( pRecv == 0 || !VmValueIsClosure(pVm, pRecv) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 || apArg[0]->x.pOther == 0 ){
		/* Unreachable while the declared `object $newThis` is screened; kept because
		 * rule 44's family says a screen written for one body shape has not
		 * necessarily run for this one. */
		return PH7_VmThrowException(pCtx, "TypeError",
			"Closure::call(): Argument #1 ($newThis) must be of type object");
	}
	pClosure = (ph7_class_instance *)pRecv->x.pOther;
	pNewThis = (ph7_class_instance *)apArg[0]->x.pOther;
	SyStringInitFromBuf(&sScope, pNewThis->pClass->sName.zString, pNewThis->pClass->sName.nByte);
	/* call() BINDS before it invokes, so php's rebind refusals apply to it — and because the
	 * scope it asks for is the new $this's class, a method callable handed an instance of
	 * anything but its own declaring class is refused for the scope, not the receiver. */
	if( !VmClosureBindAllowed(pVm, pClosure, pNewThis, &sScope) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pClone = VmCloneClosureInstance(pClosure);
	if( pClone == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	VmClosureRebind(pClone, pNewThis, &sScope);
	/* The bound closure is handed to the dispatcher through a STACK carrier that
	 * takes its own reference (rule 16): a context value would be released with the
	 * call context and unref the instance a second time. */
	PH7_MemObjInit(pVm, &sBound);
	sBound.x.pOther = pClone;
	MemObjSetType(&sBound, MEMOBJ_OBJ);
	/* The clone's own reference is this carrier's, and releasing the carrier below
	 * is what ends the temporary (see OP_LOAD_CLOSURE). */
	rc = PH7_VmCallUserFunction(pVm, &sBound, nArg - 1, apArg + 1, pCtx->pRet);
	PH7_MemObjRelease(&sBound);
	return rc;
}
/*
 * Closure — declared entirely from C.
 *
 * Its three methods were the first in the engine whose body is a C routine rather
 * than bytecode (VM_FUNC_NATIVE), retiring the global `__closure_bindTo` /
 * `__closure_fromCallable` thunks a prelude method used to forward to. The
 * DECLARATION stayed in the builtin chunk until now, which cost three things: the
 * engine slots `$__fn`/`$__this`/`$__scope` were on every presentation surface
 * (php's Closure has NO properties), `__construct` was public where php's is
 * private, and `call()` was PHP that leaked `get_class()`'s diagnostic.
 */
PH7_PRIVATE sxi32 PH7_VmInstallClosureNative(ph7_vm *pVm)
{
	static const PH7_NativeMethodDef aMethod[] = {
		/* Parameter names are php's own ($newScope, not $scope): this string is the
		 * declaration of record for arity, by-ref positions and the reported
		 * parameter list. */
		{ "__construct",  PH7_MOD_PRIVATE, "", 0,
		  vm_builtin_Closure_construct },
		/* php's own registration order, which is what get_class_methods() and
		 * ReflectionClass::getMethods() answer in: `bind` before `bindTo`. */
		{ "bind",         PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "Closure $closure, ?object $newThis, object|string|null $newScope = \"static\"", "?Closure",
		  vm_builtin_Closure_bindTo },
		{ "bindTo",       PH7_MOD_PUBLIC,
		  "?object $newThis, object|string|null $newScope = \"static\"", "?Closure",
		  vm_builtin_Closure_bindTo },
		{ "call",         PH7_MOD_PUBLIC,
		  "object $newThis, mixed ...$args", "mixed",
		  vm_builtin_Closure_call },
		{ "fromCallable", PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "callable $callback", "Closure",
		  vm_builtin_Closure_fromCallable },
		/* A closure IS its own `__invoke`, and php says so: `$c->__invoke($x)` calls
		 * it, `method_exists($c,'__invoke')` is true, and reflecting it hands back
		 * THE CLOSURE's parameter list. php does not keep it in the class's function
		 * table -- it fabricates one per closure -- which is why it is absent from
		 * get_class_methods() and from the class export, and why
		 * `new ReflectionMethod('Closure','__invoke')` refuses while
		 * `(new ReflectionObject($c))->getMethod('__invoke')` answers. That whole
		 * shape is PH7_MOD_FABRICATED. Declared with NO signature: the reflector
		 * built from a class NAME has nothing to describe (php reports zero
		 * parameters and no return type for it), and the one built from an OBJECT
		 * describes the closure instead. */
		{ "__invoke",     PH7_MOD_PUBLIC|PH7_MOD_FABRICATED, 0, 0,
		  vm_builtin_Closure_invoke },
	};
	/* The engine's own slots: the callable NAME, the bound receiver and the bound
	 * scope. php presents no property at all for a Closure, so all three carry
	 * PH7_MOD_HIDDEN — they keep working for `new`, `clone` and the C bodies (and
	 * for serialize(), which this class refuses anyway) and disappear from
	 * var_dump/print_r/(array)/get_object_vars/foreach/json_encode and Reflection. */
	static const PH7_NativePropDef aProp[] = {
		{ "__fn",    PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ "__this",  PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ "__scope", PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ "__called", PH7_MOD_PRIVATE|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	/* php's get_debug_info for a Closure shows a SHAPE none of those three slots
	 * is (name/file/line or function, static, this, parameter) — see
	 * PH7_ClosurePresent, which lives beside the reflection machinery that already
	 * describes any callable's parameters. */
	static const PH7_NativeClassSpec sSpec = {
		"Closure", 0, 0, PH7_CLASS_FINAL|PH7_CLASS_NOSERIALIZE|PH7_CLASS_NOINSTANTIATE,
		aMethod, SX_ARRAYSIZE(aMethod),
		0, 0,
		aProp, SX_ARRAYSIZE(aProp),
		VmClosureRelease, 0, PH7_ClosurePresent
	};
	return PH7_InstallNativeClasses(&(*pVm),&sSpec,1);
}
/*
 * Closure::bindTo($newThis, $scope='static') / Closure::bind($closure, $newThis, $scope='static').
 * Clone the receiver and rebind $this/$scope; returns the new Closure (NULL on a non-Closure
 * receiver, matching PHP's failure mode).
 */
PH7_PRIVATE int vm_builtin_Closure_bindTo(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pClosure, *pNewThis, *pClone;
	ph7_value *pNewThisArg;
	ph7_value *pRecv;
	SyString sScope;
	const SyString *pScopePtr = 0;
	/* One body, both spellings — as it always was, except the closure now arrives
	 * the way php passes it rather than as a hand-written first argument. Called as
	 * the instance method bindTo(), the receiver IS the closure and the arguments
	 * start at $newThis; called as the static bind(), the closure is argument #1.
	 * Normalizing here is what lets the two share an implementation. */
	if( PH7_ContextThis(pCtx) ){
		pRecv = PH7_ContextThisValue(pCtx);
	}else{
		if( nArg < 1 ){
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		pRecv = apArg[0];
		apArg++;
		nArg--;
	}
	if( nArg < 1 || !VmValueIsClosure(pVm, pRecv) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pClosure = (ph7_class_instance *)pRecv->x.pOther;
	pNewThisArg = apArg[0];
	if( pNewThisArg->iFlags & MEMOBJ_NULL ){
		pNewThis = 0;
	}else if( pNewThisArg->iFlags & MEMOBJ_OBJ ){
		pNewThis = (ph7_class_instance *)pNewThisArg->x.pOther;
	}else{
		return PH7_VmThrowException(pCtx, "TypeError",
			"Closure::bindTo(): Argument #1 ($newThis) must be of type ?object");
	}
	if( VmClosureResolveScope((nArg > 1) ? apArg[1] : 0, &sScope) ){
		pScopePtr = &sScope;
	}
	/* php RESOLVES the scope argument before it decides anything else, and a name no class
	 * answers to is its own warning — ahead of all four rebind refusals, for a plain closure
	 * as much as for a method one. PHL bound the unresolvable scope in silence and then had
	 * no scope at all, so `bindTo($o, 'Typo')` produced a closure that could not reach the
	 * private members it was being bound for.
	 *
	 * The lookup is a plain class-name one: `'self'` and `'parent'` are names no class
	 * can have, so php warns for them too (only the exact `'static'` is special, and
	 * VmClosureResolveScope has already kept the scope for it). */
	if( pScopePtr && pScopePtr->nByte
	 && PH7_VmExtractClass(pVm, pScopePtr->zString, pScopePtr->nByte, FALSE, 0) == 0 ){
		VmErrorFormat(pVm,PH7_CTX_WARNING,"Class \"%z\" not found",pScopePtr);
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( !VmClosureBindAllowed(pVm, pClosure, pNewThis, pScopePtr) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pClone = VmCloneClosureInstance(pClosure);
	if( pClone == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	VmClosureRebind(pClone, pNewThis, pScopePtr);
	return VmClosureResult(pCtx, pClone);
}
/*
 * Closure::__invoke(...$args) — call the closure the receiver IS.
 *
 * php reaches the closure's own body through this name; here the receiver is a
 * Closure OBJECT and every call door already knows how to invoke one, so the whole
 * body is "call myself with what I was given". Argument binding, by-reference
 * parameters and the declared-type screens are the callee's own, exactly as they
 * are for `$c(...)`.
 */
PH7_PRIVATE int vm_builtin_Closure_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	if( pRecv == 0 || (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_OK;
	}
	return PH7_VmCallUserFunction(pVm, pRecv, nArg, apArg, pCtx->pRet);
}
/*
 * Closure::fromCallable($callable) — normalize any callable value to a Closure (reuses the
 * VmFccWrapValue primitive); idempotent on a Closure; TypeError on a non-callable.
 */
PH7_PRIVATE int vm_builtin_Closure_fromCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pClosure;
	sxi32 rc;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx, "TypeError",
			"Closure::fromCallable() expects exactly 1 argument, 0 given");
	}
	if( VmValueIsClosure(pVm, apArg[0]) ){
		ph7_result_value(pCtx, apArg[0]); /* already a Closure: idempotent */
		return PH7_OK;
	}
	{
		ph7_class_instance *pExc;
		rc = PH7_VmCallableDeprecationFenced(pVm, apArg[0], &pExc);
		if( pExc ){
			/* The error handler threw on the deprecation: php's refusal, with no reason. */
			rc = PH7_VmThrowExceptionPrev(pCtx, pExc, "TypeError", "Failed to create closure from callable");
			PH7_ClassInstanceUnref(pExc);
			return rc;
		}
		if( rc != PH7_OK ){
			return rc;
		}
	}
	pClosure = VmFccWrapValue(pVm, apArg[0], TRUE);
	if( pClosure == 0 ){
		/* php says WHY, with the same reason taxonomy every callback argument uses —
		 * `Failed to create closure from callable: class P does not have a method "zz"`.
		 * PHL answered one flat "is not a valid callback" for all eight causes, so a typo
		 * in a method name, a private one, a missing class and a bad array shape were
		 * indistinguishable. PH7_VmCallableReason is the shared builder (its tails are
		 * already byte-exact for call_user_func & friends); the fallback covers the OOM
		 * path, where the value IS callable and the reason is 0. */
		char zWhy[192];
		const char *zReason = PH7_VmCallableReason(pVm, apArg[0], zWhy, sizeof(zWhy));
		if( zReason ){
			return PH7_VmThrowException(pCtx, "TypeError",
				"Failed to create closure from callable: %s", zReason);
		}
		return PH7_VmThrowException(pCtx, "TypeError",
			"Failed to create closure from callable");
	}
	return VmClosureResult(pCtx, pClosure);
}
/*
 * Does a stored callback need the scope it was registered from? TRUE when it names a
 * method the global scope could not call -- a private or protected one -- or spells its
 * class half as `self`/`parent`/`static`, which only the registering frame can resolve.
 */
static int VmCallbackNeedsScope(ph7_vm *pVm, ph7_value *pCallback)
{
	ph7_class_method *pMethod;
	ph7_class *pCls = 0;
	const char *zCls = 0, *zMeth = 0;
	sxu32 nCls = 0, nMeth = 0;
	if( pCallback->iFlags & MEMOBJ_STRING ){
		const char *zName = (const char *)SyBlobData(&pCallback->sBlob);
		sxu32 nName = SyBlobLength(&pCallback->sBlob), nSep;
		for( nSep = 0 ; nSep + 1 < nName ; ++nSep ){
			if( zName[nSep] == ':' && zName[nSep+1] == ':' ){
				break;
			}
		}
		if( nSep + 1 >= nName ){
			return 0; /* A plain function name */
		}
		zCls = zName;
		nCls = nSep;
		zMeth = zName + nSep + 2;
		nMeth = nName - (nSep + 2);
	}else if( pCallback->iFlags & MEMOBJ_HASHMAP ){
		ph7_value *pTarget, *pMeth;
		if( !PH7_VmArrayCallableParts(pVm, (ph7_hashmap *)pCallback->x.pOther, &pTarget, &pMeth)
		 || (pMeth->iFlags & MEMOBJ_STRING) == 0 ){
			return 0;
		}
		zMeth = (const char *)SyBlobData(&pMeth->sBlob);
		nMeth = SyBlobLength(&pMeth->sBlob);
		if( pTarget->iFlags & MEMOBJ_OBJ ){
			pCls = ((ph7_class_instance *)pTarget->x.pOther)->pClass;
		}else if( pTarget->iFlags & MEMOBJ_STRING ){
			zCls = (const char *)SyBlobData(&pTarget->sBlob);
			nCls = SyBlobLength(&pTarget->sBlob);
		}else{
			return 0;
		}
	}else{
		return 0;
	}
	if( pCls == 0 ){
		if( (nCls == 4 && SyStrnicmp(zCls, "self", 4) == 0)
		 || (nCls == 6 && SyStrnicmp(zCls, "parent", 6) == 0)
		 || (nCls == 6 && SyStrnicmp(zCls, "static", 6) == 0) ){
			return 1;
		}
		pCls = PH7_VmExtractClass(pVm, zCls, nCls, FALSE, 0);
		if( pCls == 0 ){
			return 0;
		}
	}
	pMethod = PH7_ClassExtractMethod(pCls, zMeth, nMeth);
	return pMethod != 0 && pMethod->iProtection != PH7_CLASS_PROT_PUBLIC;
}
/*
 * A callback STORED to run later -- register_shutdown_function(), spl_autoload_register() --
 * keeps the scope it was registered from. php resolves such a callback once, at registration,
 * and keeps the resolved function, so a `[$this,'priv']` registered inside its class runs at
 * shutdown or from the autoloader; PHL kept only the value and re-resolved it from wherever the
 * call happened, which is the global scope, and died `Call to private method`. When the
 * callback needs that scope, pOut receives the closure Closure::fromCallable() would mint here
 * and the caller invokes it instead, keeping the original value for introspection and removal.
 * pOut is left untouched (NULL) otherwise.
 */
PH7_PRIVATE void PH7_VmBindCallbackScope(ph7_vm *pVm, ph7_value *pCallback, ph7_value *pOut)
{
	ph7_class_instance *pClosure;
	if( PH7_VmCallerScope(pVm) == 0 || VmValueIsClosure(pVm, pCallback)
	 || !VmCallbackNeedsScope(pVm, pCallback) ){
		return;
	}
	pClosure = VmFccWrapValue(pVm, pCallback, TRUE);
	if( pClosure == 0 ){
		return;
	}
	PH7_MemObjRelease(pOut);
	/* The fresh instance's own reference is the one this value takes. */
	pOut->x.pOther = pClosure;
	MemObjSetType(pOut, MEMOBJ_OBJ);
}
/*
 * Fiber::suspend($value = null) — static method.
 * Suspends the currently running fiber and passes $value to the caller.
 */
PH7_PRIVATE int vm_builtin_Fiber_suspend(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	if( pVm->pActiveCtx == 0 ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Cannot suspend outside of a fiber");
	}
#ifdef PH7_CORO_STACK
	if( pVm->pActiveCtx->pCoro ){
		/* The fiber has a stack of its own, so this is not a return code that has
		 * to be threaded back out through every activation between here and the
		 * body -- it is one switch, and everything above it (this builtin's own C
		 * frame, the array_map/usort loop that called into PHP, the eval or catch
		 * body the call sits in) stays standing where it is. */
		ph7_exec_ctx *pFiber = pVm->pActiveCtx;
		sxi32 rc;
		if( nArg > 0 ){
			PH7_MemObjStore(apArg[0], &pFiber->sSuspendValue);
		}else{
			PH7_MemObjRelease(&pFiber->sSuspendValue);
		}
		rc = VmCoroSuspend(pCtx, pFiber);
		if( rc != PH7_OK ){
			return rc;  /* resumed by Fiber::throw(), or unwound by the teardown */
		}
		/* php: Fiber::suspend() ANSWERS what resume() was given. */
		ph7_result_value(pCtx, &pFiber->sSuspendValue);
		return PH7_OK;
	}
#endif
	/* Stage 4 scoped divergence: the trampoline only makes PHP->PHP CALLs
	 * iterative. Every OTHER re-entry runs on a fresh native VmByteCodeExec
	 * activation (nVmExecDepth bumped) that PH7_SUSPEND cannot unwind across
	 * without real coroutine stacks: a C->PHP callback
	 * (usort/array_map/preg_replace_callback comparator), and — because fibers
	 * use the LEGACY (non-inline) try/catch machinery — a suspend inside a
	 * fiber's catch/finally body, a match/switch arm, or eval()/include()'d
	 * code, all of which run via VmLocalExec. php does all of these via full
	 * native-stack switching; PHL raises a catchable FiberError instead of the
	 * old silent corruption. A suspend in a fiber's try BODY (not catch/finally)
	 * runs in the main dispatch loop and parks normally. A recorded
	 * residual; making the catch/finally case work needs fibers on the inline
	 * try machinery (the generator ROOT C path), a follow-up. */
	if( pVm->nVmExecDepth != pVm->pActiveCtx->nBodyExecDepth ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Cannot suspend across an internal call boundary");
	}
	if( nArg > 0 ){
		PH7_MemObjStore(apArg[0], &pVm->pActiveCtx->sSuspendValue);
	}else{
		PH7_MemObjRelease(&pVm->pActiveCtx->sSuspendValue);
	}
	return PH7_SUSPEND;
}
/*
 * __fiber_construct($this, $callable) — validate and store the callable.
 * Actual resolution is deferred to start() so that overload selection
 * and closure-environment binding happen with the correct argument context.
 */
PH7_PRIVATE int vm_builtin_Fiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis;
	ph7_value *pAttr;
	SyString sAttrName;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	if( pRecv == 0 ){ return PH7_OK; }
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Fiber::__construct() expects a callable argument");
	}
	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Fiber::__construct(): invalid $this");
	}
	pThis = (ph7_class_instance *)pRecv->x.pOther;
	if( pThis->pClass != pCtx->pVm->pFiberClass ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Fiber::__construct(): $this is not a Fiber instance");
	}
	/* php validates `callable $callback` HERE, with the ordinary callback-argument
	 * screen and its whole reason taxonomy -- `new Fiber('nosuch')` is a TypeError at
	 * CONSTRUCTION, naming the function it could not find. PHL had a hand-rolled shape
	 * check that only asked "string or object", with a FiberError of its own wording,
	 * and left an unresolvable NAME to fail at start() instead: the fiber constructed
	 * fine and the program learned about its typo one call later. */
	{
		sxi32 rcCb = PH7_CheckCallbackArg(pCtx, apArg[0], 1, "callback", FALSE);
		if( rcCb != PH7_OK ){
			return rcCb;
		}
	}
	/* Store callable in $this->__callable for deferred resolution at start() */
	SyStringInitFromBuf(&sAttrName, "__callable", 10);
	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);
	if( pAttr ){
		PH7_MemObjStore(apArg[0], pAttr);
	}
	return PH7_OK;
}
/*
 * Resolve a fiber's stored callable to the BODY it runs and the receiver that body
 * needs -- for every shape php's `callable` covers, not just the two PHL used to take.
 *
 * The constructor screens the argument with php's own callback rules now
 * (PH7_CheckCallbackArg), so what arrives here is a callable; this decides which body
 * it names. `[$obj,'m']`, `['Class','stat']` and `"Class::stat"` are the everyday way
 * to run an object's method as a coroutine, and all three were refused outright --
 * the first two by the constructor's "string or closure" shape check, the third by a
 * plain-function lookup that could never find a method.
 *
 * Answers 0 for a callable this engine has no BYTECODE body for: a host builtin
 * (`new Fiber('strtoupper')`) and a name php routes through __call/__callStatic --
 * which is where the visibility rule lives, and why this asks for it. php does not
 * reach a private method through a callable, it reaches __call INSTEAD, so running
 * the private body would be a hole rather than a shortcut. Both shapes run on php,
 * whose fiber switches a real stack; here they are a loud refusal (the scope policy divergence,
 * twin-paired). *pzWhy names the reason for the caller to report.
 */
static ph7_vm_func * VmFiberCallableBody(ph7_vm *pVm, ph7_value *pCallable,
	ph7_class_instance **ppThis, const char **pzWhy)
{
	ph7_class_method *pMethod = 0;
	ph7_class *pClass = 0;
	*ppThis = 0;
	*pzWhy = 0;
	if( pCallable->iFlags & MEMOBJ_HASHMAP ){
		/* php's `[target, method]` pair, decoded by the one shared reader so a fiber
		 * agrees with is_callable() and with every dispatch site about what it is. */
		ph7_value *pTarget = 0, *pName = 0;
		if( !PH7_VmArrayCallableParts(pVm, (ph7_hashmap *)pCallable->x.pOther, &pTarget, &pName)
		 || (pName->iFlags & MEMOBJ_STRING) == 0 ){
			*pzWhy = "callable is not a valid [target, method] pair";
			return 0;
		}
		pClass = PH7_VmExtractClassFromValue(pVm, pTarget);
		if( pVm->pClosureMethodCls ){
			/* A method closure's pair resolved in a class of its own (VmClosureUnwrap). */
			pClass = pVm->pClosureMethodCls;
			pVm->pClosureMethodCls = 0;
		}
		if( pClass ){
			pMethod = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pName->sBlob),
				SyBlobLength(&pName->sBlob));
		}
		if( pMethod == 0 || !PH7_VmCallableMethodAccessible(pVm, pClass, pMethod) ){
			*pzWhy = "callable routes through __call(), which cannot be a fiber body here";
			return 0;
		}
		if( (pTarget->iFlags & MEMOBJ_OBJ) && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){
			*ppThis = (ph7_class_instance *)pTarget->x.pOther;
		}
		return &pMethod->sFunc;
	}
	if( pCallable->iFlags & MEMOBJ_STRING ){
		const char *zCls, *zMeth;
		sxu32 nCls, nMeth;
		SyString sName;
		SyHashEntry *pEntry;
		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));
		/* php's `"Class::method"` static-callable string is the same callee as the pair. */
		if( PH7_VmCallableStringParts(sName.zString, sName.nByte, &zCls, &nCls, &zMeth, &nMeth) ){
			pClass = PH7_VmResolveCallableScope(pVm, zCls, nCls);
			pMethod = pClass ? PH7_ClassExtractMethod(pClass, zMeth, nMeth) : 0;
			if( pMethod == 0 || !PH7_VmCallableMethodAccessible(pVm, pClass, pMethod) ){
				*pzWhy = "callable routes through __callStatic(), which cannot be a fiber body here";
				return 0;
			}
			return &pMethod->sFunc;
		}
		pEntry = PH7_VmGetUserFunction(pVm, sName.zString, sName.nByte,
			(pCallable->iFlags & MEMOBJ_AUX_ENGINEFN) != 0);
		if( pEntry == 0 ){
			*pzWhy = PH7_VmGetHostFunction(pVm, sName.zString, sName.nByte, FALSE)
				? "callable is an internal function, which cannot be a fiber body here"
				: "callable names no such function";
			return 0;
		}
		return (ph7_vm_func *)pEntry->pUserData;
	}
	*pzWhy = "callable is not a string, array or object";
	return 0;
}
/*
 * Resolve the callable stored in a Fiber's $__callable attribute.
 * Returns the resolved ph7_vm_func* or NULL on failure (with exception thrown).
 * If the callable is a closure (object), *ppThis is set to the closure instance
 * so that start() can bind it as $this for the closure environment.
 */
static ph7_vm_func * VmFiberResolveCallable(ph7_context *pCtx, ph7_class_instance *pFiberObj,
	ph7_class_instance **ppThis)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value *pCallable;
	SyString sAttrName;
	*ppThis = 0;
	SyStringInitFromBuf(&sAttrName, "__callable", 10);
	pCallable = PH7_ClassInstanceFetchAttr(pFiberObj, &sAttrName);
	if( pCallable == 0 || (pCallable->iFlags & (MEMOBJ_STRING|MEMOBJ_OBJ|MEMOBJ_HASHMAP)) == 0 ){
		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");
		return 0;
	}
	if( pCallable->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP) ){
		const char *zWhy = 0;
		ph7_vm_func *pFunc = VmFiberCallableBody(pVm, pCallable, ppThis, &zWhy);
		if( pFunc == 0 ){
			PH7_VmThrowException(pCtx, "FiberError", "Fiber %s", zWhy);
		}
		return pFunc;
	}else{
		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;
		ph7_class_method *pMethod;
		if( VmValueIsClosure(pVm, pCallable) ){
			/* A real Closure object: unwrap to its underlying callable name (the single
			 * source of truth, VmClosureUnwrap) and resolve that function. Its captured
			 * environment (including any `$this`) rides along in the named function's
			 * aClosureEnv, installed by VmFiberSetupFrame, so *ppThis stays 0. */
			ph7_value sName;
			ph7_vm_func *pUnwrapped = 0;
			const char *zWhyClo = 0;
			PH7_MemObjInit(pVm, &sName);
			if( VmClosureUnwrap(pVm, pCallable, &sName) == SXRET_OK ){
				/* The engine's own `[closure_N]` key, which only this mark gets past the
				 * script-facing name screen (PH7_VmGetUserFunction). */
				sName.iFlags |= MEMOBJ_AUX_ENGINEFN;
				/* The unwrap answers a NAME for a plain closure and a `[target, method]`
				 * pair for a first-class callable taken from a method -- so it goes through
				 * the same body-finder as a callable the program wrote. Without it a
				 * `$o->stat(...)` fiber could not be resolved at all. */
				pUnwrapped = VmFiberCallableBody(pVm, &sName, ppThis, &zWhyClo);
			}
			PH7_MemObjRelease(&sName);
			if( pUnwrapped ){
				/* A BOUND closure parked its $this in the pClosureThis transient
				 * (VmClosureUnwrap): consume it as the fiber's $this — it wins over
				 * the creation-time env capture (VmFiberSetupFrame's skip). *ppThis
				 * is a borrow (the closure's $__this attr keeps the object alive
				 * through $__callable), so drop the parked ref. The scope transient
				 * is cleared alongside: fiber bodies don't model pBoundScope
				 * visibility (recorded residual), and a stale transient would
				 * poison the next OP_CALL's frame. */
				if( pVm->pClosureThis ){
					*ppThis = pVm->pClosureThis;
					PH7_ClassInstanceUnref(pVm->pClosureThis);
					pVm->pClosureThis = 0;
				}
				pVm->pClosureScope = 0;
				pVm->bClosureScreened = 0;
				pVm->pClosureMethodCls = 0;
				return pUnwrapped;
			}
			if( pVm->pClosureThis ){
				/* Failed resolution: drop the parked transient so it neither leaks
				 * nor poisons the next call. */
				PH7_ClassInstanceUnref(pVm->pClosureThis);
				pVm->pClosureThis = 0;
			}
			pVm->pClosureScope = 0;
			pVm->bClosureScreened = 0;
			pVm->pClosureMethodCls = 0;
			PH7_VmThrowException(pCtx, "FiberError", zWhyClo
				? "Fiber %s" : "Fiber callable closure could not be resolved", zWhyClo);
			return 0;
		}
		/* Object callable — resolve __invoke method */
		pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",
			sizeof("__invoke") - 1);
		if( pMethod == 0 ){
			PH7_VmThrowException(pCtx, "FiberError",
				"Fiber callable object has no __invoke method");
			return 0;
		}
		*ppThis = pClosure;
		return &pMethod->sFunc;
	}
}
/*
 * Install arguments into a fiber's frame using the same semantics as PH7_OP_CALL:
 * type casting, pass-by-reference handling, default values, and closure environment.
 * The fiber's frame must be at the top of pVm->pFrame when this is called.
 */
/*
 * Enforce one formal parameter's declared type on an argument being installed.
 * THE single implementation of the per-argument check, shared by the
 * generator/fiber initial-frame binder below (band A #2) and both OP_CALL
 * install paths (named-map and positional — they carried two verbatim copies
 * until the recorded(f) fold): union types via VmCoerceToUnion, class and
 * pseudo types (VmCheckPseudoType + VmResolveTypeClass + instanceof, so
 * interfaces/abstract classes and self/parent resolve), the bare `object`
 * hint, and scalars via VmEnforceScalarType (weak-mode coercion in place,
 * strict rejection otherwise) — with the VM_FUNC_ARG_NULLABLE guard letting
 * null through for `?type` and implicit-nullable `Type $x = null` params,
 * and whole-real materialization on a mask match.
 * Returns SXRET_OK (value possibly coerced) or the VmThrowTypeErrorForArg
 * status for the caller to route — normalized so an INLINE-caught throw
 * (VmThrowInline records only a pc-redirect and reports SXRET_OK) still
 * comes back as PH7_EXCEPTION: the binding must stop; the OP_CALL sites
 * force rc = PH7_EXCEPTION on any non-OK status anyway, and the generator
 * block's PH7_INLINE_RESUME_BREAK consumes the redirect.
 */
static sxi32 VmGenArgThrowStatus(ph7_vm *pVm, sxi32 rcThrow)
{
	if( rcThrow == SXRET_OK && pVm->pInlineInstr ){
		return PH7_EXCEPTION;
	}
	return rcThrow;
}
PH7_PRIVATE sxi32 VmEnforceArgType(ph7_vm *pVm, ph7_vm_func *pFunc, ph7_vm_func_arg *pFormal,
	sxu32 nArgPos, ph7_value *pVal, int bStrict, ph7_class *pSelfHint)
{
	if( pFormal->iFlags & VM_FUNC_ARG_UNION ){
		if( VmCoerceToUnion(pVm,pVal,&pFormal->aUnionAlts,
			(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) ? 1 : 0,bStrict,pSelfHint) != SXRET_OK ){
			const char *zGiven;
			const char *zExpected = "union";
			char zBuf[128];
			char zTypeBuf[128];
			if( pVal->iFlags & MEMOBJ_OBJ ){
				zGiven = VmFormatValueClassName(pVal,zBuf,sizeof(zBuf));
			}else if( pVal->iFlags & MEMOBJ_NULL ){
				zGiven = "null";
			}else{
				zGiven = VmValueGivenName(pVal,zBuf,sizeof(zBuf));
			}
			if( SyStringLength(&pFormal->sTypeName) > 0 ){
				zExpected = VmHintTextResolved(pVm,&pFormal->sTypeName,pSelfHint,
					zTypeBuf,sizeof(zTypeBuf));
			}
			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,
				&pFormal->sName,zExpected,zGiven));
		}
		return SXRET_OK;
	}
	if( pFormal->nType == 0
	 || ((pFormal->iFlags & VM_FUNC_ARG_NULLABLE) && (pVal->iFlags & MEMOBJ_NULL)) ){
		return SXRET_OK;
	}
	if( pFormal->nType == SXU32_HIGH ){
		/* Class or pseudo type */
		SyString *pName = &pFormal->sClass;
		ph7_class *pClass;
		int rcPseudo = VmCheckPseudoType(&(*pVm),pVal,pName);
		if( rcPseudo == 0 ){
			char zTypeBuf[128],zGivenBuf[128];
			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,
				&pFormal->sName,
				VmClassHintTypeName(pName,0,
					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),
				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));
		}
		pClass = 0;
		if( rcPseudo != 1 && !VmClassHintMatches(&(*pVm),pName,pSelfHint,pVal,&pClass) ){
			char zTypeBuf[128],zGivenBuf[128];
			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,
				&pFormal->sName,
				VmClassHintTypeName(pName,pClass,
					(pFormal->iFlags & VM_FUNC_ARG_NULLABLE) != 0,zTypeBuf,sizeof(zTypeBuf)),
				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));
		}
		return SXRET_OK;
	}
	if( (pVal->iFlags & pFormal->nType) == 0 ){
		char zGivenBuf[128];
		if( pFormal->nType == MEMOBJ_OBJ ){
			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,
				&pFormal->sName,"object",VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));
		}
		if( VmEnforceScalarType(pVal,pFormal->nType,bStrict) != SXRET_OK ){
			char zTypeBuf[128];
			return VmGenArgThrowStatus(pVm,VmThrowTypeErrorForArg(&(*pVm),pSelfHint,pFunc,(int)nArgPos,
				&pFormal->sName,
				VmScalarTypeName(pFormal->nType,&pFormal->sTypeName,zTypeBuf,sizeof(zTypeBuf)),
				VmValueGivenName(pVal,zGivenBuf,sizeof(zGivenBuf))));
		}
	}else{
		/* Mask matched — an int param accepting a whole-real materializes
		 * it (php: g(1.0) into int $x is int(1)). */
		VmMaterializeIntTyped(pVal,pFormal->nType);
	}
	return SXRET_OK;
}
/*
 * Record a caller slot this body's frame now ALIASES through a by-reference
 * parameter. The body outlives its caller, so the two frames cannot each own the
 * slot: the caller's teardown counts this frame's name binding as a holder and
 * leaves the value standing, and VmReleaseExecCtx asks PH7_VmReleaseUnheldSlot for
 * every row here once its own names are gone — whichever dies last frees it.
 */
static void VmCtxAliasByRefArg(ph7_exec_ctx *pExecCtx,sxu32 nIdx)
{
	sxu32 *aIdx = (sxu32 *)SySetBasePtr(&pExecCtx->aByRefArg);
	sxu32 n;
	for( n = 0 ; n < SySetUsed(&pExecCtx->aByRefArg) ; ++n ){
		if( aIdx[n] == nIdx ){
			/* Two parameters over one actual (`g($x,$x)`) is ONE slot to give back. */
			return;
		}
	}
	SySetPut(&pExecCtx->aByRefArg,(const void *)&nIdx);
}
PH7_PRIVATE sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,
	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg,
	int bStrict, ph7_class *pSelfHint, int bCallSiteInMsg, int bAliasByRef)
{
	ph7_vm_func *pFunc = pExecCtx->pFunc;
	ph7_vm_func_arg *aFormalArg;
	sxu32 nFormal, n;
	sxu32 nReqGF = 0, nNonVarGF = 0; /* too-few-args watermark (0 = exempt) */
	VmSlot sSlot;
	sxi32 rc;
	/* Install $this for closure/method callables */
	if( pClosureThis ){
		static const SyString sThis = { "this", sizeof("this") - 1 };
		ph7_value *pObj = VmExtractMemObj(pVm, &sThis, FALSE, TRUE);
		if( pObj ){
			pObj->x.pOther = pClosureThis;
			MemObjSetType(pObj, MEMOBJ_OBJ);
			pClosureThis->iRef++; /* Take a strong reference; frame teardown will unref */
		}
		/* And on the FRAME, which is what everything asking "whose method is this
		 * activation" reads: a coroutine body installed the receiver only as a
		 * variable, so a `debug_backtrace()` frame for a generator METHOD came back
		 * with its class but no `object` — and twig's error reporter, which finds the
		 * template to blame by looking for `$trace['object'] instanceof Template`
		 * across the backtrace, found none and could not say `at line N` for any
		 * template whose failing frame is a compiled generator. Borrowed exactly like
		 * VmEnterFrame's: the variable installed above owns the reference, and it
		 * lives and dies with this frame. */
		if( pExecCtx->pFrame ){
			pExecCtx->pFrame->pThis = pClosureThis;
		}
	}
	/* Install static variables */
	if( SySetUsed(&pFunc->aStatic) > 0 ){
		ph7_vm_func_static_var *aStatic;
		ph7_value *pVal;
		aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);
		for( n = 0; n < SySetUsed(&pFunc->aStatic); ++n ){
			pVal = VmReserveMemObj(pVm, &sSlot.nIdx);
			if( pVal ){
				sSlot.pUserData = 0;
				SySetPut(&pExecCtx->pFrame->sLocal, &sSlot);
				SyHashInsert(&pExecCtx->pFrame->hVar, aStatic[n].sName.zString,
					aStatic[n].sName.nByte, SX_INT_TO_PTR(sSlot.nIdx));
				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){
					VmLocalExec(pVm, &aStatic[n].aByteCode, pVal,FALSE);
				}
			}
		}
	}
	/* Install arguments with type casting and default values (matching OP_CALL) */
	aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);
	nFormal = SySetUsed(&pFunc->aArgs);
	/* Actual call arity for func_num_args()/func_get_args() (band A #4) */
	pExecCtx->pFrame->nActualArgs = nArg;
	{
		/* Too-few-arguments watermark — checked per formal INSIDE the install
		 * loop below, after the passed args' type checks, matching php's
		 * RECV order (a type error on a passed argument beats the count
		 * error). Generators throw at the g(...) call site (message embeds
		 * it); fibers at Fiber::start() (php omits the call-site segment).
		 * Prelude builtins are included — VmThrowBuiltinTooFewArgs words them
		 * as php words an internal callable. */
	nReqGF = VmFuncRequiredArgCount(pFunc,&nNonVarGF);
	}
	for( n = 0; n < nFormal; n++ ){
		ph7_value *pObj;
		if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){
			/* Variadic formal: collect this and every remaining actual into a
			 * fresh array (php semantics — pre-fix nothing collected here, so a
			 * `function g(int ...$xs)` generator saw a bare scalar in $xs).
			 * Per-element checks mirror OP_CALL's variadic install: union via
			 * the shared helper, scalar coerce/TypeError, `object` hint; a
			 * class-typed variadic element is (like OP_CALL) not checked. */
			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);
			if( pObj ){
				sxu32 nVariadicIdx;
				ph7_hashmap *pMap;
				sxu32 k;
				PH7_MemObjToHashmap(pObj);
				/* Capture the slot index now: PH7_HashmapInsert used to reallocate
				 * pVm->aMemObj and dangle pObj (same hazard as OP_CALL's path).
				 * Redundant now the table is segmented; left for the harvest sweep. */
				nVariadicIdx = pObj->nIdx;
				pMap = (ph7_hashmap *)pObj->x.pOther;
				for( k = n; k < (sxu32)nArg; k++ ){
					if( apArg[k] == 0 ){
						continue; /* a named call's hole: the formal it names is not this one */
					}
					if( ((aFormalArg[n].iFlags & VM_FUNC_ARG_UNION)
					   || (aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH)) ){
						rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,apArg[k],bStrict,pSelfHint);
						if( rc != SXRET_OK ){
							return rc;
						}
					}
					if( bAliasByRef && (aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF)
					 && apArg[k]->nIdx != SXU32_HIGH ){
						/* A by-ref variadic tail aliases its actuals here too — the
						 * ordinary call's rule, one container over. */
						VmCtxAliasByRefArg(pExecCtx,apArg[k]->nIdx);
						PH7_HashmapInsertByRef(pMap,0,apArg[k]->nIdx);
					}else{
						PH7_HashmapInsert(pMap,0,apArg[k]);
					}
				}
				sSlot.nIdx = nVariadicIdx;
				sSlot.pUserData = 0;
				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);
			}
			break; /* All remaining actuals consumed */
		}
		if( n < (sxu32)nArg && apArg[n] != 0 ){
			/* Argument provided — install with declared-type enforcement.
			 * A NULL entry is a named call's HOLE: `g(a: 1, c: 9)` names the
			 * first and third formals and says nothing about the second, so the
			 * list arrives one entry per formal and the hole falls through to the
			 * default branch below, exactly as php's binder does.
			 * php binds and type-checks generator arguments EAGERLY at the
			 * g(...) call site (and fiber arguments at Fiber::start()), so the
			 * enforcement lives here, mirroring the OP_CALL install path via
			 * VmEnforceArgType (TypeError on mismatch, weak coercion in
			 * place otherwise) instead of the old silent xCast. A variadic
			 * formal collects as-is (no per-element declared-type model). */
			if( bAliasByRef && (aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF)
			 && apArg[n]->nIdx != SXU32_HIGH ){
				/* php binds a generator's by-REFERENCE parameter to the CALLER's slot at
				 * the g(...) that builds the Generator, so the body's write reaches the
				 * caller's variable whenever it eventually runs. Copying it left the
				 * actual untouched for every resume. The type check runs on the actual,
				 * as OP_CALL's by-ref binder does, and never on a copy the alias
				 * replaces. Fiber::start() and the embedder entry pass by VALUE (php's
				 * own decision at those two boundaries), hence bAliasByRef. */
				sxi32 iPreFlags = apArg[n]->iFlags;
				rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,apArg[n],bStrict,pSelfHint);
				if( rc != SXRET_OK ){
					return rc;
				}
				/* A declared type's conversion is what the reference holds (the ordinary
				 * call's rule; the check ran on the operand-stack copy). */
				PH7_VmByRefArgWriteBack(pVm,apArg[n],iPreFlags);
				PH7_VmBindVarSlot(pVm,pExecCtx->pFrame,
					SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName),
					apArg[n]->nIdx);
				VmCtxAliasByRefArg(pExecCtx,apArg[n]->nIdx);
				sSlot.nIdx = apArg[n]->nIdx;
				sSlot.pUserData = 0;
				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);
				continue;
			}
			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);
			if( pObj ){
				PH7_MemObjStore(apArg[n], pObj);
				if( (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) == 0 ){
					rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,pObj,bStrict,pSelfHint);
					if( rc != SXRET_OK ){
						return rc;
					}
				}
				sSlot.nIdx = pObj->nIdx;
				sSlot.pUserData = 0;
				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);
			}
		}else if( n < nReqGF ){
			/* Required formal with no actual: php's ArgumentCountError, at
			 * this point in the install order (see the watermark comment). */
			return VmGenArgThrowStatus(pVm,
				(pFunc->iFlags & VM_FUNC_INTERNAL)
					? VmThrowBuiltinTooFewArgs(pVm,pSelfHint,&pFunc->sName,
						(sxu32)nArg,nReqGF,SySetUsed(&pFunc->aArgs))
					: VmThrowTooFewArgs(pVm,pSelfHint,&pFunc->sName,pFunc,
						(sxu32)nArg,nReqGF,nNonVarGF,bCallSiteInMsg));
		}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){
			/* Default value */
			pObj = VmExtractMemObj(pVm, &aFormalArg[n].sName, FALSE, TRUE);
			if( pObj ){
				rc = VmLocalExec(pVm, &aFormalArg[n].aByteCode, pObj,FALSE);
				if( rc == SXERR_ABORT ){
					return rc;
				}
				/* A null default on an implicitly-nullable `Type $x = null`
				 * param must stay null (php); only non-null defaults keep the
				 * legacy shaping cast. */
				if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH
				 && !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){
					if( (pObj->iFlags & aFormalArg[n].nType) == 0 ){
						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);
						if( xCast ){
							xCast(pObj);
						}
					}else{
						/* Mask matched — a const-indirected whole-real default
						 * (`int $x = FOO` with FOO = 2.0) materializes as int. */
						VmMaterializeIntTyped(pObj,aFormalArg[n].nType);
					}
				}
				sSlot.nIdx = pObj->nIdx;
				sSlot.pUserData = 0;
				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);
			}
		}
	}
	/* Install closure environment (captured variables) */
	if( pFunc->iFlags & VM_FUNC_CLOSURE ){
		ph7_vm_func_closure_env *aEnv, *pEnv;
		ph7_value *pValue;
		sxu32 iEnv;
		aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);
		for( iEnv = 0; iEnv < SySetUsed(&pFunc->aClosureEnv); ++iEnv ){
			pEnv = &aEnv[iEnv];
			if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){
				continue;
			}
			if( pClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1
			 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){
				/* An explicit bound $this (bindTo/bind/call) wins over the
				 * creation-time captured $this, php-exact (mirrors OP_CALL). */
				continue;
			}
			if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){
				/* Captured by reference: link the name to the shared slot
				 * (no copy), mirroring the OP_CALL env install. */
				if( SyHashGet(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){
					SyHashInsert(&pExecCtx->pFrame->hVar,SyStringData(&pEnv->sName),
						SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));
				}
				continue;
			}
			pValue = VmExtractMemObj(pVm, &pEnv->sName, FALSE, TRUE);
			if( pValue == 0 ){
				continue;
			}
			PH7_MemObjRelease(pValue);
			PH7_MemObjStore(&pEnv->sValue, pValue);
		}
	}
	return SXRET_OK;
}
/*
 * Fiber->start(...$args) — resolve callable, create exec context, install
 * arguments/closure-env/$this (matching OP_CALL semantics), and start.
 *
 * As a native VARIADIC method the arguments arrive directly as (nArg, apArg);
 * the prelude used to hand them over as a single func_get_args() array, which
 * this had to walk and snapshot out of pVm->aMemObj.
 */
PH7_PRIVATE int vm_builtin_Fiber_start(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis;
	ph7_class_instance *pClosureThis;
	ph7_exec_ctx *pExecCtx;
	ph7_vm_func *pFunc;
	ph7_value sResult;
	ph7_value *pCtxAttr;
	SyString sAttrName;
	sxi32 rc;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	if( pRecv == 0 || (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_VmThrowException(pCtx, "FiberError", "Fiber::start() requires $this");
	}
	pThis = (ph7_class_instance *)pRecv->x.pOther;
	/* Check if already started (has a __ctx) */
	pExecCtx = VmFiberExtractCtx(pVm, pRecv);
	if( pExecCtx != 0 ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Cannot start a fiber that has already been started");
	}
	/* Resolve callable */
	pFunc = VmFiberResolveCallable(pCtx, pThis, &pClosureThis);
	if( pFunc == 0 ){
		return PH7_EXCEPTION;
	}
	/* Fiber::start()'s own `...$args` are by VALUE whatever the body declares, so php
		 * warns for every by-reference parameter and the body operates on a copy — the
		 * value PHL already produced, without the one diagnostic that says so. Named off
		 * the stored callable, which is what carries the class for a method one. */
	if( nArg > 0 ){
		SyString sCbName;
		ph7_value *pCbVal;
		SyStringInitFromBuf(&sCbName, "__callable", 10);
		pCbVal = PH7_ClassInstanceFetchAttr(pThis, &sCbName);
		if( pCbVal ){
			PH7_VmWarnByRefArgsGivenValue(pVm, pCbVal, nArg, 0, 0);
		}
	}
	/* Create execution context now that we know the function */
	pExecCtx = VmNewExecCtx(pVm, pFunc);
	if( pExecCtx == 0 ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Fiber::start(): out of memory");
	}
	/* Store context in $this->__ctx */
	SyStringInitFromBuf(&sAttrName, "__ctx", 5);
	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);
	if( pCtxAttr ){
		pCtxAttr->x.pOther = pExecCtx;
		MemObjSetType(pCtxAttr, MEMOBJ_RES);
	}
	/* Temporarily attach the fiber's frame to the VM chain so that
	 * VmExtractMemObj (used by VmFiberSetupFrame) installs variables
	 * into the fiber's frame, not the caller's. */
	pExecCtx->pFrame->pParent = pVm->pFrame;
	pVm->pFrame = pExecCtx->pFrame;
	/* Unpack the args array and install into the frame */
	{
		/* The arguments are this call's own operand-stack slots, so they can be
		 * handed to the frame setup as-is. The old form had to snapshot them out of
		 * pVm->aMemObj first, because they arrived as a func_get_args() hashmap
		 * whose element values live in that pool — and VmFiberSetupFrame reserves
		 * memory objects (VmExtractMemObj) before reading its arguments, which back
		 * then reallocated the pool and dangled a raw pointer into it. Operand slots
		 * do not move, so the copy went with the array that made it necessary. (Pool
		 * slots do not move either since P1, which retires the hazard entirely.) */
		ph7_value **apValues = (nArg > 0) ? apArg : 0;
		int nActual = nArg;
		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues,
			0 /* weak-mode arg binding, like call_user_func */, 0,
			FALSE/*Fiber::start(): php omits the call-site segment*/,
			FALSE/*php's Fiber::start() passes by VALUE and warns (recorded)*/);
		/* Nothing to free: apValues aliases the operand stack now, it is not a
		 * buffer this function allocated. */
	}
	/* Detach the frame — VmStartCtx will re-attach it */
	pVm->pFrame = pExecCtx->pFrame->pParent;
	pExecCtx->pFrame->pParent = 0;
	if( rc != SXRET_OK ){
		/* Propagate the real status: a declared-type TypeError from the arg
		 * install (band A #2) must stay catchable, not become an abort. */
		return (rc == PH7_EXCEPTION) ? PH7_EXCEPTION : PH7_ABORT;
	}
	PH7_MemObjInit(pVm, &sResult);
	{
		/* php's EG(active_fiber): the fiber the running code is INSIDE, which is what
		 * Fiber::getCurrent() answers. Saved and restored around the body run, so a
		 * fiber that starts another one nests, and a fiber that finishes hands the
		 * name back to whoever was current before it. Borrowed for the duration: the
		 * receiver of this call owns the reference. */
		ph7_class_instance *pOldFiber = pVm->pCurFiber;
		pVm->pCurFiber = pThis;
		rc = VmStartCtx(pVm, pExecCtx, &sResult);
		pVm->pCurFiber = pOldFiber;
	}
	if( rc == PH7_ABORT ){
		PH7_MemObjRelease(&sResult);
		return PH7_ABORT;
	}
	if( rc == PH7_EXCEPTION ){
		PH7_MemObjRelease(&sResult);
#ifdef PH7_CORO_STACK
		if( pExecCtx->pEscaped ){
			return VmFiberRaiseEscaped(pCtx, pExecCtx);
		}
#endif
		return PH7_EXCEPTION;
	}
	ph7_result_value(pCtx, &sResult);
	PH7_MemObjRelease(&sResult);
	return PH7_OK;
}
/*
 * Fiber->resume($value = null) — resume a suspended fiber.
 */
PH7_PRIVATE int vm_builtin_Fiber_resume(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_exec_ctx *pExecCtx;
	ph7_value sResult;
	ph7_value *pResumeVal;
	sxi32 rc;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	if( pRecv == 0 ){ return PH7_OK; }
	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){
		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Fiber::resume() requires $this");
		return PH7_OK;
	}
	pExecCtx = VmFiberExtractCtx(pVm, pRecv);
	if( pExecCtx == 0 ){
		ph7_context_throw_error(pCtx, PH7_CTX_ERR, "Invalid Fiber object");
		return PH7_OK;
	}
	if( pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Cannot resume a fiber that is not suspended");
	}
	pResumeVal = (nArg > 0) ? apArg[0] : 0;
	PH7_MemObjInit(pVm, &sResult);
	{
		/* See Fiber::start(): the current fiber is this one for the length of the run. */
		ph7_class_instance *pOldFiber = pVm->pCurFiber;
		pVm->pCurFiber = (ph7_class_instance *)pRecv->x.pOther;
		rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);
		pVm->pCurFiber = pOldFiber;
	}
	if( rc == PH7_ABORT ){
		PH7_MemObjRelease(&sResult);
		return PH7_ABORT;
	}
	if( rc == PH7_EXCEPTION ){
		PH7_MemObjRelease(&sResult);
#ifdef PH7_CORO_STACK
		if( pExecCtx->pEscaped ){
			return VmFiberRaiseEscaped(pCtx, pExecCtx);
		}
#endif
		return PH7_EXCEPTION;
	}
	ph7_result_value(pCtx, &sResult);
	PH7_MemObjRelease(&sResult);
	return PH7_OK;
}
/*
 * Fiber->throw(Throwable $exception) — resume the fiber by RAISING at its
 * suspension point, so `Fiber::suspend()` throws instead of returning. Same
 * transport as Generator::throw(): the exception is parked on the context and
 * the resumed body raises it in its own frame at the top of the dispatch loop,
 * which is what lets a try/catch INSIDE the fiber catch it and carry on. The
 * answer is the next suspend value, or null if the body ran to completion --
 * symmetric with resume(). Every non-suspended state is php's one sentence.
 */
PH7_PRIVATE int vm_builtin_Fiber_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_exec_ctx *pExecCtx;
	ph7_class_instance *pInj;
	ph7_value sResult;
	sxi32 rc;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	if( pRecv == 0 || (pRecv->iFlags & MEMOBJ_OBJ) == 0 || nArg < 1 ){
		return PH7_OK;
	}
	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_OK; /* the declared `Throwable $exception` screen already spoke */
	}
	pExecCtx = VmFiberExtractCtx(pVm, pRecv);
	if( pExecCtx == 0 || pExecCtx->iState != PH7_CTX_STATE_SUSPENDED ){
		/* php answers the same sentence for never-started, running and terminated. */
		return PH7_VmThrowException(pCtx, "FiberError",
			"Cannot resume a fiber that is not suspended");
	}
	/* Hold a reference for the whole operation: the resumed body may bind the
	 * instance in a catch and release it again before we are back. */
	pInj = (ph7_class_instance *)apArg[0]->x.pOther;
	pInj->iRef++;
	pExecCtx->pInjected = pInj;   /* borrowed; consumed at the resume's loop top */
	PH7_MemObjInit(pVm, &sResult);
	{
		/* See Fiber::start(): the current fiber is this one for the length of the run. */
		ph7_class_instance *pOldFiber = pVm->pCurFiber;
		pVm->pCurFiber = (ph7_class_instance *)pRecv->x.pOther;
		rc = VmResumeCtx(pVm, pExecCtx, 0, &sResult);
		pVm->pCurFiber = pOldFiber;
	}
	/* Normally consumed (cleared) at the loop top; clear it here too for the path
	 * where VmResumeCtx bails BEFORE entering the loop (the recursion-depth fatal),
	 * so no dangling borrowed pointer survives the Unref. */
	pExecCtx->pInjected = 0;
	PH7_ClassInstanceUnref(pInj);
	if( rc == PH7_ABORT ){
		PH7_MemObjRelease(&sResult);
		return PH7_ABORT;
	}
	if( rc == PH7_EXCEPTION ){
		PH7_MemObjRelease(&sResult);
#ifdef PH7_CORO_STACK
		if( pExecCtx->pEscaped ){
			return VmFiberRaiseEscaped(pCtx, pExecCtx);
		}
#endif
		return PH7_EXCEPTION;
	}
	ph7_result_value(pCtx, &sResult);
	PH7_MemObjRelease(&sResult);
	return PH7_OK;
}
/*
 * Fiber->getReturn() — get the fiber's return value after it has terminated.
 */
PH7_PRIVATE int vm_builtin_Fiber_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_exec_ctx *pExecCtx;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	SXUNUSED(apArg);
	SXUNUSED(nArg);
	if( pRecv == 0 ){ return PH7_OK; }
	if( (pRecv->iFlags & MEMOBJ_OBJ) == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pExecCtx = VmFiberExtractCtx(pVm, pRecv);
	if( pExecCtx == 0 ){
		/* No context at all IS the never-started state -- the fiber's __ctx slot is
		 * filled by start(). php names it rather than answering null. */
		return PH7_VmThrowException(pCtx, "FiberError",
			"Cannot get fiber return value: The fiber has not been started");
	}
	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){
		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){
			return PH7_VmThrowException(pCtx, "FiberError",
				"Cannot get fiber return value: The fiber has not been started");
		}
		if( pExecCtx->bThrew ){
			/* Terminated, but with nothing to hand back: php's own third sentence. */
			return PH7_VmThrowException(pCtx, "FiberError",
				"Cannot get fiber return value: The fiber threw an exception");
		}
		return PH7_VmThrowException(pCtx, "FiberError",
			"Cannot get fiber return value: The fiber has not returned");
	}
	ph7_result_value(pCtx, &pExecCtx->sRetValue);
	return PH7_OK;
}
/*
 * Fiber->isStarted() / isRunning() / isSuspended() / isTerminated()
 */
PH7_PRIVATE int vm_builtin_Fiber_isStarted(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_exec_ctx *pExecCtx;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	SXUNUSED(apArg);
	SXUNUSED(nArg);
	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }
	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);
	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState != PH7_CTX_STATE_CREATED);
	return PH7_OK;
}
PH7_PRIVATE int vm_builtin_Fiber_isRunning(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_exec_ctx *pExecCtx;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	SXUNUSED(apArg);
	SXUNUSED(nArg);
	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }
	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);
	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_RUNNING);
	return PH7_OK;
}
PH7_PRIVATE int vm_builtin_Fiber_isSuspended(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_exec_ctx *pExecCtx;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	SXUNUSED(apArg);
	SXUNUSED(nArg);
	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }
	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);
	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_SUSPENDED);
	return PH7_OK;
}
PH7_PRIVATE int vm_builtin_Fiber_isTerminated(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_exec_ctx *pExecCtx;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	SXUNUSED(apArg);
	SXUNUSED(nArg);
	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }
	pExecCtx = VmFiberExtractCtx(pCtx->pVm, pRecv);
	/* php's DEAD state: a body that returned and a body that let an exception
	 * escape are both terminated -- only getReturn() tells them apart. */
	ph7_result_bool(pCtx, pExecCtx && (pExecCtx->iState == PH7_CTX_STATE_COMPLETED
		|| pExecCtx->iState == PH7_CTX_STATE_CLOSED));
	return PH7_OK;
}
/*
 * Fiber->__destruct() — clean up the execution context.
 */
PH7_PRIVATE int vm_builtin_Fiber_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_exec_ctx *pExecCtx;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	SXUNUSED(apArg);
	SXUNUSED(nArg);
	if( pRecv == 0 ){
		return PH7_OK;
	}
	pExecCtx = VmFiberExtractCtx(pVm, pRecv);
	if( pExecCtx ){
		VmReleaseExecCtx(pVm, pExecCtx);
		/* Clear the attribute so double-free is prevented */
		if( pRecv->iFlags & MEMOBJ_OBJ ){
			ph7_class_instance *pThis = (ph7_class_instance *)pRecv->x.pOther;
			SyString sAttrName;
			ph7_value *pAttr;
			SyStringInitFromBuf(&sAttrName, "__ctx", 5);
			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);
			if( pAttr ){
				PH7_MemObjRelease(pAttr);
			}
		}
	}
	return PH7_OK;
}
/* ======================== Fiber Public API Helpers ======================== */
PH7_PRIVATE int PH7_VmIsFiber(ph7_vm *pVm, ph7_value *pVal)
{
	ph7_class_instance *pThis;
	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 ) return 0;
	pThis = (ph7_class_instance *)pVal->x.pOther;
	return pThis->pClass == pVm->pFiberClass;
}
PH7_PRIVATE sxi32 PH7_VmFiberStart(ph7_vm *pVm, ph7_value *pFiber, int nArg, ph7_value **apArg, ph7_value *pResult)
{
	ph7_class_instance *pThis;
	ph7_class_instance *pClosureThis = 0;
	ph7_exec_ctx *pCtx;
	ph7_vm_func *pFunc;
	ph7_value *pCallable;
	ph7_value *pCtxAttr;
	SyString sAttrName;
	sxi32 rc;
	/* Must not already be started */
	pCtx = VmFiberExtractCtx(pVm, pFiber);
	if( pCtx != 0 ){
		return SXERR_INVALID;
	}
	if( (pFiber->iFlags & MEMOBJ_OBJ) == 0 ){
		return SXERR_INVALID;
	}
	pThis = (ph7_class_instance *)pFiber->x.pOther;
	/* Get the callable */
	SyStringInitFromBuf(&sAttrName, "__callable", 10);
	pCallable = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);
	if( pCallable == 0 ){
		return SXERR_INVALID;
	}
	/* Resolve callable, through the same body-finder the PHP-level start() uses --
	 * these were two copies of one decision, and only the other one grew php's array
	 * and "Class::method" shapes. An embedder has no context to throw through, so the
	 * reason comes back as this entry point's own status. */
	if( pCallable->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP) ){
		const char *zWhy = 0;
		pFunc = VmFiberCallableBody(pVm, pCallable, &pClosureThis, &zWhy);
		if( pFunc == 0 ){
			return SXERR_NOTFOUND;
		}
	}else if( pCallable->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pClosure = (ph7_class_instance *)pCallable->x.pOther;
		ph7_class_method *pMethod = PH7_ClassExtractMethod(pClosure->pClass, "__invoke",
			sizeof("__invoke") - 1);
		if( pMethod == 0 ){
			return SXERR_INVALID;
		}
		pClosureThis = pClosure;
		pFunc = &pMethod->sFunc;
	}else{
		return SXERR_INVALID;
	}
	/* Create context */
	pCtx = VmNewExecCtx(pVm, pFunc);
	if( pCtx == 0 ){
		return SXERR_MEM;
	}
	/* Store in __ctx */
	SyStringInitFromBuf(&sAttrName, "__ctx", 5);
	pCtxAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);
	if( pCtxAttr ){
		pCtxAttr->x.pOther = pCtx;
		MemObjSetType(pCtxAttr, MEMOBJ_RES);
	}
	/* Set up frame with args */
	pCtx->pFrame->pParent = pVm->pFrame;
	pVm->pFrame = pCtx->pFrame;
	rc = VmFiberSetupFrame(pVm, pCtx, pClosureThis, nArg, apArg,
		0 /* weak-mode arg binding (embedder entry) */, 0,
		FALSE/*embedder entry: no userland call site*/,
		FALSE/*no source-level actuals to alias*/);
	pVm->pFrame = pCtx->pFrame->pParent;
	pCtx->pFrame->pParent = 0;
	if( rc != SXRET_OK ){
		return rc;
	}
	return VmStartCtx(pVm, pCtx, pResult);
}
PH7_PRIVATE sxi32 PH7_VmFiberResume(ph7_vm *pVm, ph7_value *pFiber, ph7_value *pSendValue, ph7_value *pResult)
{
	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);
	if( pCtx == 0 ) return SXERR_INVALID;
	return VmResumeCtx(pVm, pCtx, pSendValue, pResult);
}
PH7_PRIVATE int PH7_VmFiberIsSuspended(ph7_vm *pVm, ph7_value *pFiber)
{
	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);
	return pCtx && pCtx->iState == PH7_CTX_STATE_SUSPENDED;
}
PH7_PRIVATE int PH7_VmFiberIsTerminated(ph7_vm *pVm, ph7_value *pFiber)
{
	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);
	return pCtx && pCtx->iState == PH7_CTX_STATE_COMPLETED;
}
PH7_PRIVATE ph7_value * PH7_VmFiberReturnValue(ph7_vm *pVm, ph7_value *pFiber)
{
	ph7_exec_ctx *pCtx = VmFiberExtractCtx(pVm, pFiber);
	if( pCtx == 0 || pCtx->iState != PH7_CTX_STATE_COMPLETED ) return 0;
	return &pCtx->sRetValue;
}
/* ======================== Generator Infrastructure ======================== */
/*
 * Allocate a new generator wrapper around an execution context.
 */
PH7_PRIVATE ph7_generator * VmNewGenerator(ph7_vm *pVm, ph7_exec_ctx *pCtx)
{
	ph7_generator *pGen;
	pGen = (ph7_generator *)SyMemBackendPoolAlloc(&pVm->sAllocator, sizeof(ph7_generator));
	if( pGen == 0 ){
		return 0;
	}
	SyZero(pGen, sizeof(ph7_generator));
	pGen->pCtx = pCtx;
	pGen->iImplicitKey = 0;
	PH7_MemObjInit(pVm, &pGen->sYieldValue);
	PH7_MemObjInit(pVm, &pGen->sYieldKey);
	/* Link the generator back to the exec context */
	pCtx->pPrivate = pGen;
	return pGen;
}
/*
 * Release a generator and its execution context.
 */
PH7_PRIVATE void VmReleaseGenerator(ph7_vm *pVm, ph7_generator *pGen)
{
	if( pGen == 0 ){
		return;
	}
	PH7_MemObjRelease(&pGen->sYieldValue);
	PH7_MemObjRelease(&pGen->sYieldKey);
	if( pGen->pCtx ){
		pGen->pCtx->pPrivate = 0;
		VmReleaseExecCtx(pVm, pGen->pCtx);
		pGen->pCtx = 0;
	}
	SyMemBackendPoolFree(&pVm->sAllocator, pGen);
}
/*
 * Extract ph7_generator from a Generator class instance.
 */
PH7_PRIVATE ph7_generator * VmGeneratorExtractCtx(ph7_vm *pVm, ph7_value *pGenObj)
{
	ph7_class_instance *pThis;
	SyString sAttr;
	ph7_value *pAttr;
	if( (pGenObj->iFlags & MEMOBJ_OBJ) == 0 ){
		return 0;
	}
	pThis = (ph7_class_instance *)pGenObj->x.pOther;
	if( pThis->pClass != pVm->pGeneratorClass ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr, "__ctx", 5);
	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);
	if( pAttr == 0 || (pAttr->iFlags & MEMOBJ_RES) == 0 ){
		return 0;
	}
	return (ph7_generator *)pAttr->x.pOther;
}
/*
 * php's zend_generator_ensure_initialized, which EVERY accessor on the class runs
 * first: a generator that has never executed is run to its first yield before it
 * is asked anything. `current()`/`key()`/`rewind()` did that here and the rest did
 * not, so `valid()` answered FALSE for a generator sitting at its first yield —
 * `while ($g->valid())` never entered the loop — and `next()`/`send()` did the
 * PRIMING run and called it the advance, which delivers the first element twice
 * and drops what `send()` was given.
 */
static sxi32 VmGeneratorEnsureInit(ph7_vm *pVm, ph7_generator *pGen)
{
	sxi32 rc;
	if( pGen->pCtx->iState != PH7_CTX_STATE_CREATED ){
		return PH7_OK;
	}
	rc = VmStartCtx(pVm, pGen->pCtx, 0);
	if( rc == PH7_OK ){
		/* php sets the flag on the INITIALIZING run itself, whatever it settled
		 * on — so a generator whose body never yields is still rewindable, and
		 * only a later resume takes the flag away. */
		pGen->bAtFirstYield = 1;
	}
	return rc;
}
/*
 * Fetch the ph7_generator behind a Generator INSTANCE (the ph7_value-taking
 * VmGeneratorExtractCtx is the same lookup from the other side).
 */
static ph7_generator * VmGeneratorFromInstance(ph7_vm *pVm, ph7_class_instance *pThis)
{
	SyString sAttr;
	ph7_value *pAttr;
	if( pThis == 0 || pVm->pGeneratorClass == 0 || pThis->pClass != pVm->pGeneratorClass ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr, "__ctx", 5);
	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);
	if( pAttr == 0 || (pAttr->iFlags & MEMOBJ_RES) == 0 ){
		return 0;
	}
	return (ph7_generator *)pAttr->x.pOther;
}
/*
 * Run a generator to its first yield if it has never executed. `yield from`
 * needs this and NOT a rewind: php links the delegate as a child node and only
 * initializes it, so delegating to a generator that is already suspended
 * half-way CONTINUES from where it stands.
 */
PH7_PRIVATE sxi32 PH7_VmGeneratorPrime(ph7_vm *pVm, ph7_class_instance *pThis)
{
	ph7_generator *pGen = VmGeneratorFromInstance(pVm, pThis);
	if( pGen == 0 || pGen->pCtx == 0 ){
		return PH7_OK;
	}
	return VmGeneratorEnsureInit(pVm, pGen);
}
/*
 * Whether a generator instance has already run to its end. php's
 * zend_generator_get_iterator refuses to start a foreach over one
 * ("Cannot traverse an already closed generator") BEFORE the rewind that would
 * otherwise report the coarser "already run" message.
 */
PH7_PRIVATE int PH7_VmGeneratorIsClosed(ph7_vm *pVm, ph7_class_instance *pThis)
{
	ph7_generator *pGen = VmGeneratorFromInstance(pVm, pThis);
	if( pGen == 0 || pGen->pCtx == 0 ){
		return 0;
	}
	return pGen->pCtx->iState == PH7_CTX_STATE_COMPLETED
		|| pGen->pCtx->iState == PH7_CTX_STATE_CLOSED;
}
/*
 * Generator::rewind() — start if CREATED, no-op otherwise.
 */
PH7_PRIVATE int vm_builtin_Generator_rewind(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	sxi32 rc;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	SXUNUSED(apArg);
	SXUNUSED(nArg);
	if( pRecv == 0 ) return PH7_OK;
	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);
	if( pGen == 0 ) return PH7_OK;
	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);
	if( rc == PH7_ABORT ) return PH7_ABORT;
	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
	if( !pGen->bAtFirstYield ){
		/* php: a generator is not rewindable, so rewind() is only allowed to mean
		 * "initialize". Once the body has moved past its first yield — or run to
		 * the end — php refuses, and PHL accepted in silence, so a foreach over a
		 * partly consumed generator carried on from where it stood while php
		 * stopped the program. */
		return PH7_VmThrowException(pCtx, "Exception",
			"Cannot rewind a generator that was already run");
	}
	return PH7_OK;
}
/*
 * Generator::valid() — true if suspended at a yield point.
 */
PH7_PRIVATE int vm_builtin_Generator_valid(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	SXUNUSED(apArg);
	SXUNUSED(nArg);
	if( pRecv == 0 ){ ph7_result_bool(pCtx, 0); return PH7_OK; }
	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);
	if( pGen ){
		sxi32 rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);
		if( rc == PH7_ABORT ) return PH7_ABORT;
		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
	}
	ph7_result_bool(pCtx, pGen && pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED);
	return PH7_OK;
}
/*
 * Generator::current() — return the last yielded value.
 * Auto-starts the generator on first access (like PHP).
 */
PH7_PRIVATE int vm_builtin_Generator_current(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	sxi32 rc;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	SXUNUSED(apArg);
	SXUNUSED(nArg);
	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }
	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);
	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }
	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);
	if( rc == PH7_ABORT ) return PH7_ABORT;
	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){
		ph7_result_value(pCtx, &pGen->sYieldValue);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/*
 * Generator::key() — return the last yielded key.
 * Auto-starts the generator on first access (like PHP).
 */
PH7_PRIVATE int vm_builtin_Generator_key(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	sxi32 rc;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	SXUNUSED(apArg);
	SXUNUSED(nArg);
	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }
	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);
	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }
	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);
	if( rc == PH7_ABORT ) return PH7_ABORT;
	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){
		ph7_result_value(pCtx, &pGen->sYieldKey);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/*
 * Generator::next() — advance to the next yield point.
 */
PH7_PRIVATE int vm_builtin_Generator_next(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	sxi32 rc;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	SXUNUSED(apArg);
	SXUNUSED(nArg);
	if( pRecv == 0 ) return PH7_OK;
	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);
	if( pGen == 0 ) return PH7_OK;
	/* PRIMING is not the advance: php runs a never-executed body to its first
	 * yield and then still resumes past it, so `$g->next()` on a fresh generator
	 * lands on the SECOND element. Treating the start as the advance handed the
	 * first one out twice. */
	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);
	if( rc == PH7_ABORT ) return PH7_ABORT;
	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
	if( pGen->pCtx->iState != PH7_CTX_STATE_SUSPENDED ){
		return PH7_OK;
	}
	pGen->bAtFirstYield = 0;
	rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);
	if( rc == PH7_ABORT ) return PH7_ABORT;
	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
	return PH7_OK;
}
/*
 * Generator::send($value) — resume and send a value into the generator.
 */
PH7_PRIVATE int vm_builtin_Generator_send(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	ph7_value *pSendVal;
	sxi32 rc;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	if( pRecv == 0 ) return PH7_OK;
	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);
	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }
	pSendVal = (nArg > 0) ? apArg[0] : 0;
	/* php PRIMES a never-executed generator and THEN resumes it with the value, so
	 * a first `send('S')` reaches the first `yield`'s left-hand side and answers the
	 * SECOND yielded value. Stopping at the priming run dropped the value entirely
	 * and answered the first — the whole point of a coroutine's first send. */
	rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);
	if( rc == PH7_ABORT ) return PH7_ABORT;
	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
	if( pGen->pCtx->iState != PH7_CTX_STATE_SUSPENDED ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pGen->bAtFirstYield = 0;
	rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);
	if( rc == PH7_ABORT ) return PH7_ABORT;
	if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){
		ph7_result_value(pCtx, &pGen->sYieldValue);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/*
 * Generator::throw($exception) — throw an exception into the generator.
 *
 * PHP semantics: the exception is injected AT the suspended yield point so the
 * generator's OWN try/catch (if any wraps the yield) can handle it and the body
 * resumes after the try; otherwise it propagates to the throw() caller and the
 * generator closes. We implement this by resuming the body with a pending
 * injection (pCtx->pInjected) that the VM loop raises in the body's own frame via
 * the existing OP_THROW / ROOT B resume route — no exception frame is
 * reconstructed on pVm->pFrame, so the return/finally unwind path is untouched.
 * A never-started generator is first run to its first yield, then injected there;
 * a finished/closed generator has no suspend point, so the exception is simply
 * propagated to the caller (its return value stays readable via getReturn()).
 *
 * PHL does not enforce the Throwable parameter hint (interface/class hints are
 * not checked on call), so the Throwable validation — and its PHP TypeError —
 * are done here.
 */
PH7_PRIVATE int vm_builtin_Generator_throw(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	ph7_class_instance *pInj;
	ph7_class *pThrowable;
	VmFrame *pFrame;
	sxi32 rc;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	if( pRecv == 0 ){ return PH7_OK; }
	if( nArg < 1 ) return PH7_OK;
	/* Argument #1 must be a Throwable; otherwise PHP raises a TypeError naming
	 * the given type (class name for objects, "null"/"string"/... for scalars). */
	pThrowable = PH7_VmExtractClass(pCtx->pVm, "Throwable", sizeof("Throwable")-1, 0, 0);
	if( (apArg[0]->iFlags & MEMOBJ_OBJ) == 0
	 || (pThrowable && !PH7_VmInstanceOf(((ph7_class_instance *)apArg[0]->x.pOther)->pClass, pThrowable)) ){
		char zCls[128];
		const char *zGiven = VmValueGivenName(apArg[0], zCls, sizeof(zCls));
		return PH7_VmThrowException(pCtx, "TypeError",
			"Generator::throw(): Argument #1 ($exception) must be of type Throwable, %s given", zGiven);
	}
	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);
	if( pGen == 0 ) return PH7_OK;
	/* PHP forbids resuming/throwing into a generator that is currently executing. */
	if( pGen->pCtx->iState == PH7_CTX_STATE_RUNNING ){
		return PH7_VmThrowException(pCtx, "Error",
			"Cannot resume an already running generator");
	}
	/* Hold a reference to the injected instance for the whole operation: the VM loop
	 * (inject path) or VmThrowException (propagate path) may run catch blocks that bind
	 * and later release it. Dropped on every return path below. */
	pInj = (ph7_class_instance *)apArg[0]->x.pOther;
	pInj->iRef++;
	/* A never-started generator runs to its first yield, then the exception is injected
	 * there (PHP). Start it first; if it suspended at a yield, fall through to inject; if
	 * it ran to completion without yielding, drop through to the propagate path below. */
	{
		rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);
		if( rc == PH7_ABORT ){ PH7_ClassInstanceUnref(pInj); return PH7_ABORT; }
		if( rc == PH7_EXCEPTION ){ PH7_ClassInstanceUnref(pInj); return PH7_EXCEPTION; }
	}
	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){
		/* Inject at the suspended yield: the resume loop raises it in the body's own
		 * frame so the generator's try/catch can catch it and resume (path 2). */
		pGen->pCtx->pInjected = pInj;   /* borrowed; ref held here across the resume */
		pGen->bAtFirstYield = 0;
		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);
		/* Normally the inject was consumed (cleared) at VmByteCodeExec entry; clear it
		 * here too for the path where VmResumeCtx bails BEFORE entering the loop (e.g. the
		 * recursion-depth fatal), so no dangling borrowed pointer survives the Unref. */
		pGen->pCtx->pInjected = 0;
		PH7_ClassInstanceUnref(pInj);
		if( rc == PH7_ABORT ) return PH7_ABORT;
		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
		/* Caught inside the generator and it resumed: return the next yielded value (or
		 * null if it then completed) — symmetric with Generator::send(). */
		if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){
			ph7_result_value(pCtx, &pGen->sYieldValue);
		}else{
			ph7_result_null(pCtx);
		}
		return PH7_OK;
	}
	/* COMPLETED/CLOSED (incl. a CREATED generator that ran to completion without a
	 * yield): no suspend point to inject at. Propagate the real object to the throw()
	 * caller through the normal dispatch path (class/message/trace preserved); its
	 * terminal state — and thus getReturn() — is left intact. */
	pFrame = pCtx->pVm->pFrame;
	if( pFrame ){
		pFrame = VmSkipExceptionFrames(pFrame);
		pFrame->iFlags |= VM_FRAME_THROW;
	}
	rc = VmThrowException(pCtx->pVm, pInj);
	PH7_ClassInstanceUnref(pInj);
	if( rc == SXERR_ABORT ){
		return PH7_ABORT;
	}
	return PH7_EXCEPTION;
}
/*
 * Generator::getReturn() — get the return value after the generator has finished.
 */
PH7_PRIVATE int vm_builtin_Generator_getReturn(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	SXUNUSED(apArg);
	SXUNUSED(nArg);
	if( pRecv == 0 ){ ph7_result_null(pCtx); return PH7_OK; }
	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);
	if( pGen == 0 ){ ph7_result_null(pCtx); return PH7_OK; }
	{
		/* php initializes here too, so a generator asked for its return value
		 * before anything else has RUN its body up to the first yield — the side
		 * effects before that yield happen either way. */
		sxi32 rc = VmGeneratorEnsureInit(pCtx->pVm, pGen);
		if( rc == PH7_ABORT ) return PH7_ABORT;
		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
	}
	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){
		/* php's class here is Exception, not Error. */
		return PH7_VmThrowException(pCtx, "Exception",
			"Cannot get return value of a generator that hasn't returned");
	}
	ph7_result_value(pCtx, &pGen->pCtx->sRetValue);
	return PH7_OK;
}
/*
 * Generator::__destruct() — clean up.
 */
PH7_PRIVATE int vm_builtin_Generator_destruct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_generator *pGen;
	sxi32 rcClose = SXRET_OK;
	ph7_value *pRecv = PH7_ContextThisValue(pCtx);
	SXUNUSED(apArg);
	SXUNUSED(nArg);
	if( pRecv == 0 ) return PH7_OK;
	pGen = VmGeneratorExtractCtx(pCtx->pVm, pRecv);
	if( pGen ){
		/* A generator abandoned before it completes still runs its pending `finally`
		 * blocks (PHP runs them at generator close/GC). Drive them before teardown. */
		if( pGen->pCtx ){
			rcClose = VmCloseCtx(pCtx->pVm, pGen->pCtx);
		}
		VmReleaseGenerator(pCtx->pVm, pGen);
		if( pRecv->iFlags & MEMOBJ_OBJ ){
			ph7_class_instance *pThis = (ph7_class_instance *)pRecv->x.pOther;
			SyString sAttrName;
			ph7_value *pAttr;
			SyStringInitFromBuf(&sAttrName, "__ctx", 5);
			pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttrName);
			if( pAttr ){
				PH7_MemObjRelease(pAttr);
			}
		}
	}
	/* Surface an abort/exception raised by a finally that ran during close. */
	if( rcClose == PH7_ABORT ) return PH7_ABORT;
	if( rcClose == PH7_EXCEPTION ) return PH7_EXCEPTION;
	return PH7_OK;
}
/* ======================== End Generator Infrastructure ======================== */
/* ======================== End Fiber Infrastructure ======================== */
