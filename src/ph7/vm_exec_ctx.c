/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
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
	pCtx->pParkedSegment = 0;
	pCtx->nBodyExecDepth = 0;
	/* Allocate a private operand stack */
	pStack = VmNewOperandStack(pVm, SySetUsed(&pFunc->aByteCode));
	if( pStack == 0 ){
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
		SyMemBackendPoolFree(&pVm->sAllocator, pCtx);
		return 0;
	}
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
	if( rc == PH7_ABORT ){
		pCtx->iState = PH7_CTX_STATE_CLOSED;
		return PH7_ABORT;
	}
	if( rc == PH7_EXCEPTION ){
		pCtx->iState = PH7_CTX_STATE_CLOSED;
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
	 * OP_CALL only (BYTECODE.md stage 5). */
	if( VmNativeNestingExceeded(pVm) ){
		return VmNativeNestingFatal(pVm);
	}
	/* Attach the fiber's frame to the VM frame chain */
	pCtx->pFrame->pParent = pVm->pFrame;
	pVm->pFrame = pCtx->pFrame;
	/* Save and set the active context */
	pOldCtx = pVm->pActiveCtx;
	pVm->pActiveCtx = pCtx;
	pCtx->iState = PH7_CTX_STATE_RUNNING;
	pCtx->nExceptionBase = SySetUsed(&pVm->aException);
	pCtx->nFinallyBase = SySetUsed(&pVm->aFinallyAction);
	pCtx->nSelfBase = SySetUsed(&pVm->aSelf);
	/* Native depth the body runs at (the VmByteCodeExec wrapper bumps +1): a
	 * Fiber::suspend() at a deeper depth is inside a C->PHP callback and gets a
	 * FiberError instead of parking across the native frame (stage 4). */
	pCtx->nBodyExecDepth = pVm->nVmExecDepth + 1;
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
	/* A resume is a native VmByteCodeExec re-entry, bounded by nMaxNativeDepth.
	 * Reject HERE, before re-attaching the (possibly deep) parked segment to the
	 * frame chain and re-adding its nRecords to nRecursionDepth — a wrapper-level
	 * abort past those mutations would leave pVm->pFrame pointing into the parked
	 * callee and the depth accounting un-reverted. The PHP call-depth cap is
	 * OP_CALL-only (BYTECODE.md stage 5). */
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
	/* Free local variables */
	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sLocal);
	for( n = 0; n < SySetUsed(&pFrame->sLocal); ++n ){
		PH7_VmUnsetMemObj(pVm, aSlot[n].nIdx, FALSE);
	}
	/* Remove local references */
	aSlot = (VmSlot *)SySetBasePtr(&pFrame->sRef);
	for( n = 0; n < SySetUsed(&pFrame->sRef); ++n ){
		PH7_VmRefObjRemove(pVm, aSlot[n].nIdx, (SyHashEntry *)aSlot[n].pUserData, 0);
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
	pCtx->iState = PH7_CTX_STATE_CLOSED;
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
				PH7_MemObjStringAppend(&sTarget, SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob));
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
			pOut->x.pOther = pMap;
			MemObjSetType(pOut, MEMOBJ_HASHMAP);
			return SXRET_OK;
		}
	}
	PH7_MemObjStringAppend(pOut, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));
	return SXRET_OK;
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
 * Returns the fresh instance (iRef == 0; caller takes the reference), or 0 on OOM.
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
	VmRefObj *pRef = VmRefObjExtract(&(*pVm),nIdx);
	if( pRef == 0 ){
		return 0;
	}
	return (int)(SySetUsed(&pRef->aReference) + SySetUsed(&pRef->aArrEntries));
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
 * stays idempotent even for a direct caller. Returns the fresh instance (iRef == 0; caller takes the
 * reference) or 0 if the value is an existing Closure / not a normalizable callable / on OOM — in
 * which case the caller leaves the value untouched (graceful degradation). This is the generic
 * "callable value -> Closure" primitive: the body is FCC-agnostic and self-contained, so the future
 * Closure::bind/fromCallable work (Increment 2) can call it directly.
 */
PH7_PRIVATE ph7_class_instance * VmFccWrapValue(ph7_vm *pVm, ph7_value *pValue)
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
		SyStringInitFromBuf(&sName, SyBlobData(&pValue->sBlob), SyBlobLength(&pValue->sBlob));
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
		if( pTarget->iFlags & MEMOBJ_OBJ ){
			ph7_class_instance *pBoundThis = (ph7_class_instance *)pTarget->x.pOther;
			ph7_class_instance *pFccObj = VmCreateClosure(pVm, &sName, pBoundThis,
				&pBoundThis->pClass->sName);
			if( pFccObj ){
				pFccObj->iFlags |= VM_INSTANCE_FCC_METHOD; /* $__fn is a METHOD name */
			}
			return pFccObj;
		}else{
			/* [class-name, method] static callable -> bind the resolved scope. A runtime array
			 * callable carries a concrete class name (never self/static/parent), so a plain class
			 * lookup is correct — unlike the syntactic `C::m(...)` path, which must resolve
			 * self/static/parent via VmFccResolveScope. Matches PH7_VmIsCallable's own decode. */
			ph7_class *pScopeCls = PH7_VmExtractClassFromValue(pVm, pTarget);
			ph7_class_instance *pFccObj = pScopeCls
				? VmCreateClosure(pVm, &sName, 0, &pScopeCls->sName) : 0;
			if( pFccObj ){
				pFccObj->iFlags |= VM_INSTANCE_FCC_METHOD; /* $__fn is a METHOD name */
			}
			return pFccObj;
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
	pClosure->iRef++;
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
	if( pScopeArg->iFlags & MEMOBJ_STRING ){
		SyStringInitFromBuf(pOut, (const char *)SyBlobData(&pScopeArg->sBlob), SyBlobLength(&pScopeArg->sBlob));
		return 1;
	}
	return 0;
}
/*
 * Fiber's C-bodied methods. Every one of them was a global `__fiber_verb($this,…)`
 * thunk that a one-line prelude method forwarded to; the class body in the builtin
 * chunk now holds only its two private slots.
 */
PH7_PRIVATE sxi32 PH7_VmInstallFiberNative(ph7_vm *pVm)
{
	static const PH7_NativeMethodDef aMethod[] = {
		{ "__construct",  PH7_MOD_PUBLIC, "callable $callback",  "",       vm_builtin_Fiber_construct },
		/* Variadic: the arguments now reach the C body directly instead of being
		 * repackaged by a func_get_args() call in the prelude. */
		{ "start",        PH7_MOD_PUBLIC, "mixed ...$args",      "mixed",  vm_builtin_Fiber_start },
		{ "resume",       PH7_MOD_PUBLIC, "mixed $value = null", "mixed",  vm_builtin_Fiber_resume },
		{ "getReturn",    PH7_MOD_PUBLIC, "",                    "mixed",  vm_builtin_Fiber_getReturn },
		{ "isStarted",    PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isStarted },
		{ "isRunning",    PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isRunning },
		{ "isSuspended",  PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isSuspended },
		{ "isTerminated", PH7_MOD_PUBLIC, "",                    "bool",   vm_builtin_Fiber_isTerminated },
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
		"Fiber", 0, 0, PH7_CLASS_NOSERIALIZE,
		aMethod, SX_ARRAYSIZE(aMethod),
		0, 0,
		aProp, SX_ARRAYSIZE(aProp),
		0, 0
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
	static const PH7_NativeClassSpec sSpec = {
		"Generator", 0, 0, PH7_CLASS_NOSERIALIZE,
		aMethod, SX_ARRAYSIZE(aMethod),
		0, 0,
		aProp, SX_ARRAYSIZE(aProp),
		0, 0
	};
	ph7_class *pClass;
	ph7_class *pIterator;
	sxi32 rc = PH7_InstallNativeClasses(&(*pVm),&sSpec,1);
	if( rc != SXRET_OK ){
		return rc;
	}
	/* `implements Iterator` last, for the reason in this function's header. */
	pClass = PH7_VmExtractClass(&(*pVm),"Generator",sizeof("Generator")-1,0,0);
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
	pClone = PH7_CloneClassInstance(pClosure);
	if( pClone == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	SyStringInitFromBuf(&sScope, pNewThis->pClass->sName.zString, pNewThis->pClass->sName.nByte);
	VmClosureRebind(pClone, pNewThis, &sScope);
	/* The bound closure is handed to the dispatcher through a STACK carrier that
	 * takes its own reference (rule 16): a context value would be released with the
	 * call context and unref the instance a second time. */
	PH7_MemObjInit(pVm, &sBound);
	sBound.x.pOther = pClone;
	MemObjSetType(&sBound, MEMOBJ_OBJ);
	pClone->iRef++;
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
		{ "bindTo",       PH7_MOD_PUBLIC,
		  "?object $newThis, object|string|null $newScope = \"static\"", "?Closure",
		  vm_builtin_Closure_bindTo },
		{ "bind",         PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "Closure $closure, ?object $newThis, object|string|null $newScope = \"static\"", "?Closure",
		  vm_builtin_Closure_bindTo },
		{ "call",         PH7_MOD_PUBLIC,
		  "object $newThis, mixed ...$args", "mixed",
		  vm_builtin_Closure_call },
		{ "fromCallable", PH7_MOD_PUBLIC|PH7_MOD_STATIC,
		  "callable $callback", "Closure",
		  vm_builtin_Closure_fromCallable },
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
	};
	static const PH7_NativeClassSpec sSpec = {
		"Closure", 0, 0, PH7_CLASS_FINAL|PH7_CLASS_NOSERIALIZE|PH7_CLASS_NOINSTANTIATE,
		aMethod, SX_ARRAYSIZE(aMethod),
		0, 0,
		aProp, SX_ARRAYSIZE(aProp),
		0, 0
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
	if( pNewThis ){
		/* php refuses to bind an instance to a static closure: warning + null */
		SyString sAttr;
		ph7_value *pFn;
		SyStringInitFromBuf(&sAttr, "__fn", 4);
		pFn = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);
		if( pFn && (pFn->iFlags & MEMOBJ_STRING) && SyBlobLength(&pFn->sBlob) > 0 ){
			SyHashEntry *pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));
			if( pEntry && (((ph7_vm_func *)pEntry->pUserData)->iFlags & VM_FUNC_STATIC_CL) ){
				PH7_VmThrowError(pVm,0,PH7_CTX_WARNING,
					"Cannot bind an instance to a static closure, this will be an error in PHP 9");
				ph7_result_null(pCtx);
				return PH7_OK;
			}
		}
	}
	if( VmClosureResolveScope((nArg > 1) ? apArg[1] : 0, &sScope) ){
		pScopePtr = &sScope;
	}
	pClone = PH7_CloneClassInstance(pClosure);
	if( pClone == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	VmClosureRebind(pClone, pNewThis, pScopePtr);
	return VmClosureResult(pCtx, pClone);
}
/*
 * Closure::fromCallable($callable) — normalize any callable value to a Closure (reuses the
 * VmFccWrapValue primitive); idempotent on a Closure; TypeError on a non-callable.
 */
PH7_PRIVATE int vm_builtin_Closure_fromCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pClosure;
	if( nArg < 1 ){
		return PH7_VmThrowException(pCtx, "TypeError",
			"Closure::fromCallable() expects exactly 1 argument, 0 given");
	}
	if( VmValueIsClosure(pVm, apArg[0]) ){
		ph7_result_value(pCtx, apArg[0]); /* already a Closure: idempotent */
		return PH7_OK;
	}
	pClosure = VmFccWrapValue(pVm, apArg[0]);
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
	/* Stage 4 scoped divergence: the trampoline only makes PHP->PHP CALLs
	 * iterative. Every OTHER re-entry runs on a fresh native VmByteCodeExec
	 * activation (nVmExecDepth bumped) that PH7_SUSPEND cannot unwind across
	 * without real coroutine stacks (BYTECODE.md §2.4): a C->PHP callback
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
	/* Basic validation: callable must be a string or closure (object) */
	if( (apArg[0]->iFlags & (MEMOBJ_STRING|MEMOBJ_OBJ)) == 0 ){
		return PH7_VmThrowException(pCtx, "FiberError",
			"Fiber::__construct() expects a callable (string or closure)");
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
	if( pCallable == 0 || (pCallable->iFlags & (MEMOBJ_STRING|MEMOBJ_OBJ)) == 0 ){
		PH7_VmThrowException(pCtx, "FiberError", "Fiber has no valid callable");
		return 0;
	}
	if( pCallable->iFlags & MEMOBJ_STRING ){
		/* String callable — look up in user functions with overload support */
		SyString sName;
		SyHashEntry *pEntry;
		ph7_vm_func *pFunc;
		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));
		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);
		if( pEntry == 0 ){
			PH7_VmThrowException(pCtx, "FiberError",
				"Fiber callable '%.*s' not found", (int)sName.nByte, sName.zString);
			return 0;
		}
		pFunc = (ph7_vm_func *)pEntry->pUserData;
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
			SyHashEntry *pEntry = 0;
			PH7_MemObjInit(pVm, &sName);
			if( VmClosureUnwrap(pVm, pCallable, &sName) == SXRET_OK ){
				pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&sName.sBlob), SyBlobLength(&sName.sBlob));
			}
			PH7_MemObjRelease(&sName);
			if( pEntry ){
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
				return (ph7_vm_func *)pEntry->pUserData;
			}
			if( pVm->pClosureThis ){
				/* Failed resolution: drop the parked transient so it neither leaks
				 * nor poisons the next call. */
				PH7_ClassInstanceUnref(pVm->pClosureThis);
				pVm->pClosureThis = 0;
			}
			pVm->pClosureScope = 0;
			PH7_VmThrowException(pCtx, "FiberError", "Fiber callable closure could not be resolved");
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
 * until the §7.1(f) fold): union types via VmCoerceToUnion, class and
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
PH7_PRIVATE sxi32 VmFiberSetupFrame(ph7_vm *pVm, ph7_exec_ctx *pExecCtx,
	ph7_class_instance *pClosureThis, int nArg, ph7_value **apArg,
	int bStrict, ph7_class *pSelfHint, int bCallSiteInMsg)
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
				/* Capture the slot index now: PH7_HashmapInsert can reallocate
				 * pVm->aMemObj and dangle pObj (same hazard as OP_CALL's path). */
				nVariadicIdx = pObj->nIdx;
				pMap = (ph7_hashmap *)pObj->x.pOther;
				for( k = n; k < (sxu32)nArg; k++ ){
					if( ((aFormalArg[n].iFlags & VM_FUNC_ARG_UNION)
					   || (aFormalArg[n].nType > 0 && aFormalArg[n].nType != SXU32_HIGH)) ){
						rc = VmEnforceArgType(pVm,pFunc,&aFormalArg[n],n+1,apArg[k],bStrict,pSelfHint);
						if( rc != SXRET_OK ){
							return rc;
						}
					}
					PH7_HashmapInsert(pMap,0,apArg[k]);
				}
				sSlot.nIdx = nVariadicIdx;
				sSlot.pUserData = 0;
				SySetPut(&pExecCtx->pFrame->sArg, &sSlot);
			}
			break; /* All remaining actuals consumed */
		}
		if( n < (sxu32)nArg ){
			/* Argument provided — install with declared-type enforcement.
			 * php binds and type-checks generator arguments EAGERLY at the
			 * g(...) call site (and fiber arguments at Fiber::start()), so the
			 * enforcement lives here, mirroring the OP_CALL install path via
			 * VmEnforceArgType (TypeError on mismatch, weak coercion in
			 * place otherwise) instead of the old silent xCast. A variadic
			 * formal collects as-is (no per-element declared-type model). */
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
					: VmThrowTooFewArgs(pVm,pSelfHint,&pFunc->sName,
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
		 * whose element values live in that set — and VmFiberSetupFrame reserves
		 * memory objects (VmExtractMemObj) before reading its arguments, which can
		 * reallocate the set and dangle a raw pool pointer. Operand slots do not
		 * move, so the copy is gone with the array that made it necessary. */
		ph7_value **apValues = (nArg > 0) ? apArg : 0;
		int nActual = nArg;
		rc = VmFiberSetupFrame(pVm, pExecCtx, pClosureThis, nActual, apValues,
			0 /* weak-mode arg binding, like call_user_func */, 0,
			FALSE/*Fiber::start(): php omits the call-site segment*/);
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
	rc = VmStartCtx(pVm, pExecCtx, &sResult);
	if( rc == PH7_ABORT ){
		PH7_MemObjRelease(&sResult);
		return PH7_ABORT;
	}
	if( rc == PH7_EXCEPTION ){
		PH7_MemObjRelease(&sResult);
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
	rc = VmResumeCtx(pVm, pExecCtx, pResumeVal, &sResult);
	if( rc == PH7_ABORT ){
		PH7_MemObjRelease(&sResult);
		return PH7_ABORT;
	}
	if( rc == PH7_EXCEPTION ){
		PH7_MemObjRelease(&sResult);
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
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( pExecCtx->iState != PH7_CTX_STATE_COMPLETED ){
		if( pExecCtx->iState == PH7_CTX_STATE_CREATED ){
			return PH7_VmThrowException(pCtx, "FiberError",
				"Cannot get fiber return value: The fiber has not been started");
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
	ph7_result_bool(pCtx, pExecCtx && pExecCtx->iState == PH7_CTX_STATE_COMPLETED);
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
	/* Resolve callable */
	if( pCallable->iFlags & MEMOBJ_STRING ){
		SyString sName;
		SyHashEntry *pEntry;
		SyStringInitFromBuf(&sName, SyBlobData(&pCallable->sBlob), SyBlobLength(&pCallable->sBlob));
		pEntry = SyHashGet(&pVm->hFunction, sName.zString, sName.nByte);
		if( pEntry == 0 ){
			return SXERR_NOTFOUND;
		}
		pFunc = (ph7_vm_func *)pEntry->pUserData;
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
		FALSE/*embedder entry: no userland call site*/);
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
	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){
		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);
		if( rc == PH7_ABORT ) return PH7_ABORT;
		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
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
	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){
		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);
		if( rc == PH7_ABORT ) return PH7_ABORT;
		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
	}
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
	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){
		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);
		if( rc == PH7_ABORT ) return PH7_ABORT;
		if( rc == PH7_EXCEPTION ) return PH7_EXCEPTION;
	}
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
	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){
		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);
	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){
		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, 0, 0);
	}else{
		return PH7_OK;
	}
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
	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){
		/* First send starts the generator; sent value is ignored per PHP semantics */
		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);
	}else if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){
		rc = VmResumeCtx(pCtx->pVm, pGen->pCtx, pSendVal, 0);
	}else{
		ph7_result_null(pCtx);
		return PH7_OK;
	}
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
	if( pGen->pCtx->iState == PH7_CTX_STATE_CREATED ){
		rc = VmStartCtx(pCtx->pVm, pGen->pCtx, 0);
		if( rc == PH7_ABORT ){ PH7_ClassInstanceUnref(pInj); return PH7_ABORT; }
		if( rc == PH7_EXCEPTION ){ PH7_ClassInstanceUnref(pInj); return PH7_EXCEPTION; }
	}
	if( pGen->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){
		/* Inject at the suspended yield: the resume loop raises it in the body's own
		 * frame so the generator's try/catch can catch it and resume (path 2). */
		pGen->pCtx->pInjected = pInj;   /* borrowed; ref held here across the resume */
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
	if( pGen->pCtx->iState != PH7_CTX_STATE_COMPLETED ){
		return PH7_VmThrowException(pCtx, "Error",
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
