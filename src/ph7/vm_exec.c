/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <math.h>
/*
 * Section:
 *    The bytecode interpreter: VmByteCodeExec, its dispatch loop
 *    (VmByteCodeExecBody) and the operand-stack growth + trampoline
 *    call-finish machinery the loop is welded to. Split from vm.c after the
 *    vm_ops_* extractions shrank the loop enough to fit its own unit; the
 *    remaining inline arms (CALL, YIELD/YIELD_FROM, the finally family,
 *    SPREAD, and the hot loads/stores/arith) share loop-invocation state
 *    (pCallTop, ppBaseOwner/pnBaseCap, Suspend/SkipFuncBody/Done labels)
 *    that must stay inside one function.
 * Status:
 *    Stable.
 */
/*
 * OP_SPREAD stack growth (removes the old VM_STACK_GUARD expansion cap).
 *
 * The operand stack of the CURRENTLY-RUNNING activation is about to receive more
 * spread elements than its remaining slack holds. Realloc the buffer so the
 * expansion — and the rest of the body's normal pushes — fit, then fix up every
 * pointer that aimed into the old buffer. Returns 1 on success (the caller may
 * proceed with the expansion), 0 on OOM (the caller keeps the old buffer and
 * raises the historical guard error, preserving pre-growth soundness).
 *
 * nNeed is the live-slot count the stack must hold after the pending expansion;
 * the grown capacity adds VM_STACK_GUARD back on top so the remainder of the body
 * keeps its slack. ONLY this activation's references move, and they are all here:
 *   - the dispatch locals pStack/pTos (via *ppStack / *ppTos) and the boundary
 *     copies in sState (pState->pStack/pTos + the tracked capacity nStackCap)
 *   - the owner slot: for a trampoline callee, its record's pFrameStack and the
 *     recycle capacity nStackCap; for the base activation (top-level, mini-program,
 *     coroutine body, callback), *ppBaseOwner / *pnBaseCap — whatever storage the
 *     native entry frees (a local, pVm->aOps, or pCtx->pStack/nStackCap)
 *   - aSpreadRun[].pStart entries anchored in the OLD buffer (this call's earlier
 *     spreads) — an unfixed pStart would desync PHP 8.1 named-arg replay after the
 *     realloc. Enclosing activations own DISTINCT buffers, so their runs (pStart
 *     outside [pOld, pOld+nOldCap)) are deliberately left untouched.
 */
static int VmGrowOperandStack(ph7_vm *pVm, sxu32 nNeed,
	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,
	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)
{
	ph7_value *pOld = *ppStack;
	sxu32 nOldCap = pState->nStackCap;
	sxu32 nNewCap, nReq, nMaxCap, i, nRun;
	ph7_value *pNew;
	VmSpreadRun *aRun;
	/* Size the grown buffer to the post-expansion live depth (nNeed) PLUS the
	 * activation's original full budget (nStackOrig = nMaxStack + VM_STACK_GUARD) as
	 * headroom. That headroom is essential: only OP_SPREAD re-checks capacity, so any
	 * ordinary arg pushes that FOLLOW this spread in the same call (e.g.
	 * `foo(...$big, a1..aN)`) must fit — and the rest of the body adds at most
	 * nMaxStack above the current point. Crucially the headroom is relative to the
	 * ORIGINAL capacity, NOT the grown nOldCap: basing it on nOldCap would ratchet
	 * capacity up on every spread (nOldCap already includes prior growth), leaking
	 * without bound across statements that share one operand stack until it pins at
	 * nMaxCap. nNeed resets between statements (the stack pops back), so this does not. */
	nReq = nNeed + pState->nStackOrig;
	if( nReq < nNeed ){ /* wrap guard (nNeed + nStackOrig overflowed sxu32) */
		nReq = SXU32_HIGH;
	}
	/* SyMemBackendRealloc's size argument is sxu32, so the byte count
	 * nNewCap*sizeof(ph7_value) must not overflow 32 bits — a huge unpack
	 * (~2^32/sizeof elements) would otherwise truncate to a tiny allocation and
	 * the init loop below would run off it. If the true requirement exceeds the
	 * representable cap, treat it as OOM (the caller raises the guard error). */
	nMaxCap = SXU32_HIGH / (sxu32)sizeof(ph7_value);
	if( nReq > nMaxCap ){
		return 0;
	}
	if( nReq <= nOldCap ){
		return 1; /* already fits — no growth needed */
	}
	nNewCap = nReq;
	/* Amortize repeated spreads in one argument list: at least double, but never
	 * past the byte-count cap. (nOldCap <= nMaxCap < 2^31, so nOldCap*2 can't
	 * itself overflow.) */
	if( nNewCap < nOldCap * 2 ){
		sxu32 nDbl = nOldCap * 2;
		if( nDbl > nMaxCap ){ nDbl = nMaxCap; }
		if( nNewCap < nDbl ){ nNewCap = nDbl; }
	}
	pNew = (ph7_value *)SyMemBackendRealloc(&pVm->sAllocator, pOld,
		nNewCap * sizeof(ph7_value));
	if( pNew == 0 ){
		return 0; /* OOM: caller keeps pOld and raises the guard error */
	}
	/* realloc preserves [0, nOldCap); initialize the freshly grown slots. */
	for( i = nOldCap; i < nNewCap; i++ ){
		PH7_MemObjInit(pVm, &pNew[i]);
		pNew[i].nIdx = SXU32_HIGH;
	}
	/* Fix up every pointer into the old buffer (delta = pNew - pOld). */
	*ppTos = pNew + (*ppTos - pOld);
	*ppStack = pNew;
	pState->pStack = pNew + (pState->pStack - pOld);
	pState->pTos = pNew + (pState->pTos - pOld);
	pState->nStackCap = nNewCap;
	if( pCallTop ){
		/* This activation is a trampoline callee: its record owns the buffer. */
		pCallTop->sCall.pFrameStack = pNew;
		pCallTop->sCall.nStackCap = nNewCap;
	}else{
		/* Base activation: the native entry frees *ppBaseOwner (and, for a
		 * resumable coroutine, persists *pnBaseCap across suspend/resume). */
		if( ppBaseOwner ){ *ppBaseOwner = pNew; }
		if( pnBaseCap ){ *pnBaseCap = nNewCap; }
	}
	/* Re-anchor this activation's captured spread runs (pStart into the old
	 * buffer). Enclosing-activation runs live in other buffers — leave them. */
	nRun = SySetUsed(&pVm->aSpreadRun);
	aRun = (VmSpreadRun *)SySetBasePtr(&pVm->aSpreadRun);
	for( i = 0; i < nRun; i++ ){
		if( aRun[i].pStart >= pOld && aRun[i].pStart < pOld + nOldCap ){
			aRun[i].pStart = pNew + (aRun[i].pStart - pOld);
		}
	}
	return 1;
}
/*
 * Ensure the running activation's operand stack can hold a spread of nEntry
 * elements pushed at *ppTos (the source slot becomes the first element, so the
 * net new slots are nEntry-1). Grows via VmGrowOperandStack when it can't.
 * Returns 1 if the caller may proceed with VmSpreadExpandMap, 0 on OOM.
 */
static int VmSpreadEnsureCapacity(ph7_vm *pVm, sxu32 nEntry,
	ph7_value **ppStack, ph7_value **ppTos, VmExecState *pState,
	VmCallFrame *pCallTop, ph7_value **ppBaseOwner, sxu32 *pnBaseCap)
{
	sxu32 nNeed;
	if( nEntry == 0 ){
		return 1; /* empty spread never grows the stack */
	}
	nNeed = (sxu32)(*ppTos - *ppStack + 1) + (nEntry - 1);
	return VmGrowOperandStack(pVm, nNeed, ppStack, ppTos, pState,
		pCallTop, ppBaseOwner, pnBaseCap);
}
/*
 * Terminal teardown of one VmByteCodeExec activation — the former
 * Done/Suspend/Abort/Exception label bodies, one home (BYTECODE.md stage 1).
 *
 * SXRET_OK (Done): whenever the REAL body returns, its pending-return slot
 * must be empty — the materialize at OP_DONE/OP_POP_EXCEPTION already moved
 * the value into pResult and cleared bHasRet, so the clear is normally a
 * no-op; it only fires on a path that reached Done with a stale slot,
 * preventing a leak. The !bReturnPropagates guard is essential: a
 * catch/finally MINI-PROGRAM runs in its body's own frame (VmLocalExec adds
 * no frame), so pEntryFrame is that body — wiping its slot would destroy the
 * return the body is about to take.
 * PH7_SUSPEND: a generator/fiber body never suspends mid-completion of a
 * catch/finally return, so its frame's slot is empty (nothing to clear) and
 * its operand stack is preserved in place — the ctx owns it.
 * PH7_ABORT / PH7_EXCEPTION: abnormal unwind — discard the body's pending
 * return (an escaping exception supersedes it, per PHP) and release every
 * live operand slot down to the activation's stack base.
 */
static void VmDiscardFinallyActions(ph7_vm *pVm, sxu32 nBase);
static sxi32 VmExecFinalize(ph7_vm *pVm,VmExecState *pState,SySet *pArg,ph7_value *pTos,sxi32 rcTerm)
{
	if( rcTerm != PH7_SUSPEND ){
		VmDiscardFinallyActions(&(*pVm),pState->nFinallyActBase);
	}
	if( rcTerm != PH7_SUSPEND && !pState->bReturnPropagates ){
		VmClearFrameReturn(pState->pEntryFrame);
	}
	SySetRelease(pArg);
	if( rcTerm == PH7_ABORT || rcTerm == PH7_EXCEPTION ){
		while( pTos >= pState->pStack ){
			PH7_MemObjRelease(pTos);
			pTos--;
		}
	}
	return rcTerm;
}
/*
 * Discard the pending finally ACTIONS an activation queued but never consumed,
 * releasing what they own (an FA_RETHROW's held exception ref; an FA_RETURN's
 * value). A `return` inside a finally that was ENTERED VIA THE THROW REDIRECT
 * (VmThrowInline queued an FA_RETHROW and jumped into the finally body)
 * short-circuits that finally's OP_END_FINALLY — OP_SET_FINALLY_RET finds no
 * remaining handler and completes the body directly — so the queued action was
 * ORPHANED on pVm->aFinallyAction. Left there, an ENCLOSING function's next
 * OP_END_FINALLY pops the orphan instead of its own action (re-raising a
 * swallowed exception / hijacking control), and on a coroutine body it leaks
 * into the resumer's scope. Called at every activation end (record pop and
 * exec finalize), never on SUSPEND (a suspended body's pending actions are
 * parked base-relative by VmParkCtxState and must survive).
 */
static void VmDiscardFinallyActions(ph7_vm *pVm, sxu32 nBase)
{
	while( SySetUsed(&pVm->aFinallyAction) > nBase ){
		VmFinallyAction *pAct = (VmFinallyAction *)SySetPeek(&pVm->aFinallyAction);
		if( pAct->eKind == PH7_FA_RETHROW && pAct->pExc ){
			PH7_ClassInstanceUnref(pAct->pExc);
		}else if( pAct->eKind == PH7_FA_RETURN ){
			PH7_MemObjRelease(&pAct->sRet);
		}
		(void)SySetPop(&pVm->aFinallyAction);
	}
}
/*
 * Finish one user-function call at the "pop" boundary of the callee's
 * activation: pop-time accounting (recursion depth, aSelf), by-ref-return
 * fixup, callee-threw routing (inline resume / recorded resume / propagate),
 * operand-stack free and frame teardown. Extracted verbatim from the OP_CALL
 * epilogue (BYTECODE.md stage 1) so the stage-2 trampoline can run the same
 * code when a record is popped at OP_DONE instead of after a native return.
 * pCaller->pc / pCaller->pTos are authoritative across this boundary; the
 * dispatch loop syncs its locals around the call. Returns PH7_OK (continue
 * the caller, possibly at a redirected pc), PH7_ABORT, PH7_SUSPEND (the ctx
 * state was re-saved at the caller's level) or PH7_EXCEPTION.
 */
static sxi32 VmCallFinish(ph7_vm *pVm,VmExecState *pCaller,VmCallRecord *pCallee,sxi32 rc)
{
	ph7_value *pObj;
	/* Decrement nesting level */
	pVm->nRecursionDepth--;
	if( pCallee->bSelfPushed ){
		/* Pop class name */
		(void)SySetPop(&pVm->aSelf);
	}
	if( (pCallee->pVmFunc->iFlags & VM_FUNC_REF_RETURN) && rc == SXRET_OK ){
		/* Return by reference,reflect that */
		if( pCallee->nLastRef != SXU32_HIGH ){
			VmSlot *aSlot = (VmSlot *)SySetBasePtr(&pCallee->pFrame->sLocal);
			sxu32 i;
			/* Make sure the referenced object is not a local variable */
			for( i = 0 ; i < SySetUsed(&pCallee->pFrame->sLocal) ; ++i ){
				if( pCallee->nLastRef == aSlot[i].nIdx ){
					pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pCallee->nLastRef);
					if( pObj && (pObj->iFlags & (MEMOBJ_NULL|MEMOBJ_OBJ|MEMOBJ_HASHMAP|MEMOBJ_RES)) == 0 ){
						VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,
							"Function '%z',return by reference: Cannot reference local variable,PH7 is switching to return by value",
							&pCallee->pVmFunc->sName);
					}
					pCallee->nLastRef = SXU32_HIGH;
					break;
				}
			}
		}else{
			if( (pCaller->pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_NULL|MEMOBJ_RES)) == 0 ){
				VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,
					"Function '%z',return by reference: Cannot reference constant expression,PH7 is switching to return by value",
					&pCallee->pVmFunc->sName);
			}
		}
		pCaller->pTos->nIdx = pCallee->nLastRef;
	}
	if( rc != PH7_ABORT && ((pCallee->pFrame->iFlags & VM_FRAME_THROW) || rc == PH7_EXCEPTION) ){
		/* The callee threw (or its finally threw past it). If an in-place catch
		 * recorded a resume target owned by THIS caller's body, resume there and
		 * consume the target (VmRecordedResume); when the catcher is an outer exec
		 * — or this is a callback with no bytecode to resume into — propagate so
		 * the owning exec lands. This replaces the old "is the caller's parent a
		 * resumable try frame" test, which resumed at the caller's OWN try even
		 * when the finally's throw was caught further out, losing that catch's
		 * return (ROOT B, face c). */
		sxi32 iResumePc;
		VmFrame *pParentFrame = pCallee->pFrame->pParent;
		if( !pCaller->is_callback && pVm->pInlineInstr == (void *)pCaller->aInstr ){
			/* ROOT C: the callee's throw was caught by an inline try in THIS caller
			 * (generator body). Drain the operand stack (incl. the unwritten result
			 * slot) to the try's base and land at its catch/finally. */
			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iInlineDrain ){
				PH7_MemObjRelease(pCaller->pTos);
				pCaller->pTos--;
			}
			pCaller->pc = (sxi32)pVm->iInlinePc - 1;
			pVm->pInlineInstr = 0;
			rc = PH7_OK;
		}else if( !pCaller->is_callback && VmRecordedResume(pVm,&iResumePc,pCaller->pEntryFrame,pCaller->aInstr) ){
			/* Pop the result, then drain any abandoned outer-expression operands
			 * to the catching try's base (like the inline branch above) — the
			 * ops that would have consumed them were abandoned by the throw, and
			 * leaving them leaks one slot per caught throw (`try { $a = 1 + f(); }`
			 * in a loop overflowed the operand stack). */
			VmPopOperand(&pCaller->pTos,1);
			while( (sxi32)(pCaller->pTos - pCaller->pStack) > pVm->iResumeStackDepth ){
				PH7_MemObjRelease(pCaller->pTos);
				pCaller->pTos--;
			}
			pCaller->pc = iResumePc;
			rc = PH7_OK;
		}else{
			if( pParentFrame->pParent ){
				rc = PH7_EXCEPTION;
			}else{
				/* Continue normal execution */
				rc = PH7_OK;
			}
		}
	}
	/* Recycle the operand stack for the next same-size call (BYTECODE stage 7),
	 * or free it if the pool is full. Its allocated size is tracked in
	 * pCallee->nStackCap (nMaxStack + VM_STACK_GUARD, or larger if an OP_SPREAD grew
	 * it) — exactly what the buffer holds. (NULL when the function body was skipped.)
	 *
	 * Never on rc == PH7_SUSPEND: that path (unreachable in the stage-4 model,
	 * where a deep suspend parks its whole record segment before reaching here)
	 * would leave the callee stack owned by the suspended ctx, so recycling it
	 * would hand a live fiber's operand stack to the next call. The guard keeps
	 * that invariant explicit and robust to future coroutine changes. */
	if( rc != PH7_SUSPEND && pCallee->pFrameStack ){
		/* nStackCap is nMaxStack+VM_STACK_GUARD unless an OP_SPREAD in this callee
		 * grew the buffer (VmGrowOperandStack updated the record) — recycle exactly
		 * the allocated slot count either way. */
		VmOperandStackRecycle(pVm,pCallee->pFrameStack,pCallee->nStackCap);
	}
	/* Leave the frame */
	VmLeaveFrame(&(*pVm));
	if( rc == PH7_ABORT ){
		return PH7_ABORT;
	}
	if( rc == PH7_SUSPEND && pVm->pActiveCtx ){
		/* A Fiber::suspend() was called somewhere inside this function.
		 * Re-save the fiber's state at THIS level (the fiber's body),
		 * overwriting the state saved by the inner level.
		 * pTos points to the result slot (not yet written).
		 * Save nTos one below so resume pushes at the result slot. */
		VmSuspendCtx(pVm,pVm->pActiveCtx,pCaller->pc + 1,(sxi32)(pCaller->pTos - pCaller->pStack) - 1);
		return PH7_SUSPEND;
	}
	if( rc == PH7_EXCEPTION ){
		return PH7_EXCEPTION;
	}
	return PH7_OK;
}
/*
 * Execute as much of a PH7 bytecode program as we can then return.
 *
 * [PH7_VmMakeReady()] must be called before this routine in order to
 * close the program with a final OP_DONE and to set up the default
 * consumer routines and other stuff. Refer to the implementation
 * of [PH7_VmMakeReady()] for additional information.
 * If the installed VM output consumer callback ever returns PH7_ABORT
 * then the program execution is halted.
 * After this routine has finished, [PH7_VmRelease()] or [PH7_VmReset()]
 * should be used respectively to clean up the mess that was left behind
 * or to reset the VM to it's initial state.
 */
static sxi32 VmByteCodeExecBody(ph7_vm *pVm,VmInstr *aInstr,ph7_value *pStack,int nTos,
	ph7_value *pResult,sxu32 *pLastRef,int is_callback,sxi32 nPc,
	ph7_vm_func *pEnforceRetFunc,int bReturnPropagates,VmParkedSegment *pAdoptSegment,
	ph7_value **ppBaseOwner,sxu32 *pnBaseCap,sxu32 nStackOrig);
/*
 * Native-nesting guard around the executor. PHP->PHP calls run iteratively
 * (the stage-2 trampoline), but every OTHER (re-)entry — mini-programs,
 * C->PHP callbacks, ctx start/resume, eval/include — is still one real C
 * activation of VmByteCodeExecBody. nMaxDepth no longer bounds them (it is
 * PHP call depth, raisable to memory-bound values since the clamp removal),
 * so this counter is what actually protects the C stack: recursive
 * eval/include towers, nested coroutine-resume chains and self-recursive
 * C-callback compositions hit a clean fatal instead of overflowing. The limit
 * lives in pVm->nMaxNativeDepth — a per-platform default (256 host / 16 small-
 * stack embedders, VmInit) overridable via PH7_VM_CONFIG_NATIVE_DEPTH. This is
 * still a coarse frame-count net rather than php's stack-byte measurement, so
 * the host default is conservative — well below the old config clamp's <1024
 * ceiling so it holds on the fattest frames (the callback path drags in
 * usort/mergesort/trampoline C frames per re-entry, and instrumented builds
 * inflate every frame), while far beyond any realistic eval/include/callback
 * nesting.
 */
PH7_PRIVATE sxi32 VmByteCodeExec(
	ph7_vm *pVm,         /* Target VM */
	VmInstr *aInstr,     /* PH7 bytecode program */
	ph7_value *pStack,   /* Operand stack */
	int nTos,            /* Top entry in the operand stack (usually -1) */
	ph7_value *pResult,  /* Store program return value here. NULL otherwise */
	sxu32 *pLastRef,     /* Last referenced ph7_value index */
	int is_callback,     /* TRUE if we are executing a callback */
	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */
	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */
	int bReturnPropagates, /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value onto the enclosing body frame's sRet slot for that body to return. */
	VmParkedSegment *pAdoptSegment, /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */
	ph7_value **ppBaseOwner, /* Storage slot the native entry frees for this invocation's BASE (pCallTop==0) operand stack — a local, pVm->aOps or pCtx->pStack. An OP_SPREAD that grows the base stack writes the new pointer here so the entry frees the right buffer. */
	sxu32 *pnBaseCap, /* Storage for the base stack's capacity (resumable coroutines persist it across suspend/resume); updated alongside *ppBaseOwner on base-stack growth. Also the initial capacity read at entry. */
	sxu32 nStackOrig /* The base stack's ORIGINAL (ungrown) allocation size. Unlike *pnBaseCap (which is the CURRENT, possibly-grown capacity on a coroutine resume), this is fixed, so OP_SPREAD growth headroom stays bounded across resumes. */
	)
{
	sxi32 rc;
	sxi32 nSavedBrc;
	sxu32 nSavedLine;
	if( VmNativeNestingExceeded(pVm) ){
		return VmNativeNestingFatal(pVm);
	}
	/* A fresh native exec entered while a C-boundary throw is parked
	 * (nBoundaryRc — e.g. a __destruct fired by an operand release inside the
	 * very opcode that swallowed the throw) must run CLEAN: the parked status
	 * belongs to the interrupted outer exec's fetch-point router, not to this
	 * one. Save+clear on entry, merge back on exit — the outer status is
	 * restored unless this exec parked its own unconsumed (newer) one, with
	 * PH7_ABORT dominating either way. */
	nSavedBrc = pVm->nBoundaryRc;
	pVm->nBoundaryRc = 0;
	/* The executing source line belongs to the ACTIVATION. A nested body -- a called
	 * function, but equally an attribute-default or default-argument mini-program --
	 * runs its own bytecode with its own lines, so it must not leave the caller
	 * reporting the callee's position: `new Exception` stamped line 1 because the
	 * class's `protected $message = '';` default ran (from the embedded chunk) between
	 * OP_NEW and the stamp. Save on entry, restore on exit. */
	nSavedLine = pVm->nCurLine;
	pVm->nVmExecDepth++;
	rc = VmByteCodeExecBody(&(*pVm),aInstr,pStack,nTos,pResult,pLastRef,is_callback,nPc,
		pEnforceRetFunc,bReturnPropagates,pAdoptSegment,ppBaseOwner,pnBaseCap,nStackOrig);
	pVm->nVmExecDepth--;
	pVm->nCurLine = nSavedLine;
	if( nSavedBrc != 0 && (nSavedBrc == PH7_ABORT || pVm->nBoundaryRc == 0) ){
		pVm->nBoundaryRc = nSavedBrc;
	}
	return rc;
}
/*
 * D1 commit 2: allocate a captured lvalue path for a deferred element/property call arg.
 * eRoot picks the root container: 0 = a real aMemObj slot (nRootIdx), 1 = an undefined
 * variable to vivify by name at resolve time (pName, a VM-lifetime bytecode string, borrowed),
 * 2 = a string base (subscripting a string -> php refuses a by-ref bind). The struct owns its
 * step array and each step's key/name; PH7_MemObjRelease frees it via VmFreeDeferredPath.
 */
PH7_PRIVATE VmDeferredPath * VmDeferPathNew(ph7_vm *pVm,int eRoot,sxu32 nRootIdx,const SyString *pName)
{
	VmDeferredPath *pPath = (VmDeferredPath *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmDeferredPath));
	if( pPath == 0 ){
		return 0;
	}
	SyZero(pPath,sizeof(VmDeferredPath));
	pPath->pAlloc = &pVm->sAllocator;
	pPath->eRoot = eRoot;
	pPath->nRootIdx = nRootIdx;
	if( eRoot == 1 && pName ){
		pPath->sRootName = *pName; /* borrowed VM-lifetime bytes, not copied */
	}
	return pPath;
}
static VmDeferStep * VmDeferPathGrow(VmDeferredPath *pPath)
{
	if( pPath->nStep >= pPath->nAlloc ){
		sxu32 nNew = pPath->nAlloc ? pPath->nAlloc * 2 : 4;
		VmDeferStep *aNew = (VmDeferStep *)SyMemBackendRealloc(pPath->pAlloc,pPath->aStep,
			nNew * sizeof(VmDeferStep));
		if( aNew == 0 ){
			return 0;
		}
		pPath->aStep = aNew;
		pPath->nAlloc = nNew;
	}
	return &pPath->aStep[pPath->nStep];
}
/* Append an array-element step, deep-copying the index value (the caller releases pKey). */
PH7_PRIVATE sxi32 VmDeferPathPushElem(VmDeferredPath *pPath,ph7_value *pKey)
{
	VmDeferStep *pStep = VmDeferPathGrow(pPath);
	if( pStep == 0 ){
		return SXERR_MEM;
	}
	pStep->isProp = 0;
	pStep->sProp.zString = 0; pStep->sProp.nByte = 0; pStep->zProp = 0;
	PH7_MemObjInit(pKey->pVm,&pStep->sKey);
	PH7_MemObjStore(pKey,&pStep->sKey);
	pPath->nStep++;
	return SXRET_OK;
}
/* Append an object-property step, owning a private copy of the name bytes. */
PH7_PRIVATE sxi32 VmDeferPathPushProp(VmDeferredPath *pPath,const SyString *pName)
{
	VmDeferStep *pStep = VmDeferPathGrow(pPath);
	char *zCopy;
	if( pStep == 0 ){
		return SXERR_MEM;
	}
	zCopy = SyMemBackendStrDup(pPath->pAlloc,pName->zString,pName->nByte);
	if( zCopy == 0 ){
		return SXERR_MEM;
	}
	pStep->isProp = 1;
	pStep->zProp = zCopy;
	SyStringInitFromBuf(&pStep->sProp,zCopy,pName->nByte);
	pPath->nStep++;
	return SXRET_OK;
}
/* Release a captured lvalue path and everything it owns (element keys, property names). */
PH7_PRIVATE void VmFreeDeferredPath(VmDeferredPath *pPath)
{
	sxu32 i;
	if( pPath == 0 ){
		return;
	}
	for( i = 0 ; i < pPath->nStep ; ++i ){
		VmDeferStep *pStep = &pPath->aStep[i];
		if( pStep->isProp ){
			if( pStep->zProp ){
				SyMemBackendFree(pPath->pAlloc,pStep->zProp);
			}
		}else{
			PH7_MemObjRelease(&pStep->sKey);
		}
	}
	if( pPath->aStep ){
		SyMemBackendFree(pPath->pAlloc,pPath->aStep);
	}
	SyMemBackendFree(pPath->pAlloc,pPath);
}
/* Map an op-handler VmOpRc into the main-loop rc convention used by VmByteCodeExecBody. */
static sxi32 VmOpRcToExecRc(VmOpRc rcOp)
{
	if( rcOp == VM_OP_ABORT ){
		return PH7_ABORT;
	}
	if( rcOp == VM_OP_EXCEPTION ){
		return PH7_EXCEPTION;
	}
	return SXRET_OK;
}
/*
 * D1 commit 2: re-drive ONE captured lvalue step by invoking the real LOAD_IDX / MEMBER
 * handler on a synthetic 2-slot stack. This reuses the proven COW / vivify / warning / magic
 * machinery instead of hand-walking it. pBase carries the current container (its nIdx must be
 * a real aMemObj slot for a by-ref write to vivify in place). iP2 selects the mode:
 * LOAD_IDX 1=write(vivify,by-ref) / 0=read(by-value); MEMBER PH7_MEMBER_READ=by-value read.
 * On success pOut receives the result value and its nIdx (the aliasable element slot for a
 * vivified by-ref element).
 */
static sxi32 VmReDriveStep(ph7_vm *pVm,sxi32 iOp,sxu32 iP2,ph7_value *pBase,ph7_value *pKey,ph7_value *pOut)
{
	ph7_value mini[2];
	VmInstr aI[2];
	VmExecState st;
	VmOpRc rcOp;
	PH7_MemObjInit(pVm,&mini[0]);
	PH7_MemObjInit(pVm,&mini[1]);
	PH7_MemObjLoad(pBase,&mini[0]);
	mini[0].nIdx = pBase->nIdx;
	PH7_MemObjStore(pKey,&mini[1]);
	SyZero((void *)aI,sizeof(aI));
	/* LOAD_IDX: iP1=1 means "an index is present". MEMBER: iP1=0 means an INSTANCE member
	 * (iP1=1 would be a static `::` access). */
	aI[0].iOp = (sxu8)iOp; aI[0].iP1 = (iOp == PH7_OP_LOAD_IDX) ? 1 : 0; aI[0].iP2 = iP2;
	SyZero((void *)&st,sizeof(st));
	st.pStack = mini; st.pTos = &mini[1]; st.aInstr = aI; st.pc = 0;
	if( iOp == PH7_OP_LOAD_IDX ){
		rcOp = VmExecOpLoadIdx(&(*pVm),&st,&aI[0]);
	}else{
		rcOp = VmExecOpMember(&(*pVm),&st,&aI[0]);
	}
	/* The handler popped the key/name and left the result in the (now top) base slot.
	 * DEEP-COPY it into pOut (MemObjStore, not MemObjLoad): the result is chained as the next
	 * step's base and must OWN its buffer — mini[0] is released immediately below, and a
	 * read-only view (MemObjLoad) would leave pOut dangling into freed memory. */
	PH7_MemObjStore(st.pTos,pOut);
	pOut->nIdx = st.pTos->nIdx;
	PH7_MemObjRelease(&mini[0]);
	return VmOpRcToExecRc(rcOp);
}
/*
 * D1 commit 2: resolve an object property as a by-ref target. Given the object's aMemObj
 * slot, return the property value's slot index in *pnOut so the by-ref binder can alias it.
 * A present property binds directly; a missing one is created (recreate a declared+unset
 * property, or a dynamic property on a dynamic-allowing class); a magic __get/__set property
 * emits php's Notice and does NOT bind (*pbNoBind). Mirrors VmExecOpMember's write-create.
 */
static sxi32 VmBindPropByRef(ph7_vm *pVm,sxu32 nObjIdx,const SyString *pName,sxu32 *pnOut,int *pbNoBind)
{
	ph7_value *pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nObjIdx);
	ph7_class_instance *pThis;
	ph7_class *pClass;
	SyHashEntry *pEntry;
	VmClassAttr *pAttr = 0;
	*pbNoBind = 0;
	if( pObj == 0 || (pObj->iFlags & MEMOBJ_OBJ) == 0 ){
		/* Base is not an object (e.g. a NULL intermediate): cannot bind a property by ref. */
		*pbNoBind = 1;
		return SXRET_OK;
	}
	pThis = (ph7_class_instance *)pObj->x.pOther;
	pClass = pThis->pClass;
	pEntry = SyHashGet(&pThis->hAttr,(const void *)pName->zString,pName->nByte);
	if( pEntry ){
		pAttr = (VmClassAttr *)pEntry->pUserData;
		*pnOut = pAttr->nIdx;
		return SXRET_OK;
	}
	if( PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)
	 || PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1) ){
		/* Overloaded (magic) property: php passes it by-value with a Notice and drops the
		 * write-back — "has no effect". */
		VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,
			"Indirect modification of overloaded property %z::$%z has no effect",
			&pClass->sName,pName);
		*pbNoBind = 1;
		return SXRET_OK;
	}
	{
		ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pClass,pName->zString,pName->nByte);
		if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT)) == 0 ){
			VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pAttr);
		}else if( VmClassAllowsDynamicProps(&(*pVm),pClass) ){
			PH7_VmCreateDynamicAttr(&(*pVm),pThis,pName->zString,pName->nByte,&pAttr);
		}else{
			SyBlob sMsg;
			sxi32 rcT;
			SyBlobInit(&sMsg,&pVm->sAllocator);
			SyBlobFormat(&sMsg,"Cannot create dynamic property %z::$%z",&pClass->sName,pName);
			rcT = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),SyBlobLength(&sMsg));
			SyBlobRelease(&sMsg);
			*pbNoBind = 1;
			return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;
		}
		if( pAttr ){
			*pnOut = pAttr->nIdx;
		}else{
			*pbNoBind = 1;
		}
	}
	return SXRET_OK;
}
/* Resolve a captured lvalue path as a BY-REF target: vivify the whole chain in place and
 * leave pSlot->nIdx pointing at the terminal (aliasable) slot for the by-ref binder. */
static sxi32 VmResolvePathByRef(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)
{
	sxu32 nCur;
	sxu32 i;
	sxi32 rc;
	if( pPath->eRoot == 2 ){
		/* Subscripting a string: php refuses a by-ref bind to a string offset. */
		sxi32 rcT = VmThrowFromVm(&(*pVm),"Error",
			"Cannot create references to/from string offsets",
			sizeof("Cannot create references to/from string offsets")-1);
		return (rcT == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;
	}
	if( pPath->eRoot == 1 ){
		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,TRUE); /* vivify $a */
		if( pRoot == 0 ){
			return SXRET_OK;
		}
		nCur = pRoot->nIdx;
	}else{
		nCur = pPath->nRootIdx;
	}
	for( i = 0 ; i < pPath->nStep ; ++i ){
		VmDeferStep *pStep = &pPath->aStep[i];
		if( pStep->isProp ){
			sxu32 nOut = SXU32_HIGH;
			int bNoBind = 0;
			rc = VmBindPropByRef(&(*pVm),nCur,&pStep->sProp,&nOut,&bNoBind);
			if( rc != SXRET_OK ){
				return rc;
			}
			if( bNoBind ){
				return SXRET_OK; /* magic/non-object: leave the slot a clean NULL, pass by value */
			}
			nCur = nOut;
		}else{
			ph7_value out;
			ph7_value *pContainer = (ph7_value *)SySetAt(&pVm->aMemObj,nCur);
			if( pContainer == 0 ){
				return SXRET_OK;
			}
			PH7_MemObjInit(&(*pVm),&out);
			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,1,pContainer,&pStep->sKey,&out);
			nCur = out.nIdx;
			PH7_MemObjRelease(&out);
			if( rc != SXRET_OK ){
				return rc;
			}
			if( nCur == SXU32_HIGH ){
				return SXRET_OK; /* no aliasable slot (e.g. a non-lvalue container): pass by value */
			}
		}
	}
	pSlot->nIdx = nCur;
	return SXRET_OK;
}
/* Resolve a captured lvalue path as a BY-VALUE argument: read the chain (emitting php's
 * undefined-key/property/offset warnings) WITHOUT vivifying, leaving the terminal value in
 * pSlot. Re-drives the read handlers so the warning sequence matches php exactly. */
static sxi32 VmResolvePathByValue(ph7_vm *pVm,VmDeferredPath *pPath,ph7_value *pSlot)
{
	ph7_value cur;
	sxu32 i;
	sxi32 rc = SXRET_OK;
	PH7_MemObjInit(&(*pVm),&cur);
	if( pPath->eRoot == 1 ){
		ph7_value *pRoot = VmExtractMemObj(&(*pVm),&pPath->sRootName,FALSE,FALSE);
		if( pRoot == 0 ){
			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&pPath->sRootName);
		}else{
			PH7_MemObjLoad(pRoot,&cur);
			cur.nIdx = pRoot->nIdx;
		}
	}else{
		ph7_value *pRoot = (ph7_value *)SySetAt(&pVm->aMemObj,pPath->nRootIdx);
		if( pRoot ){
			PH7_MemObjLoad(pRoot,&cur);
			cur.nIdx = pRoot->nIdx;
		}
	}
	for( i = 0 ; i < pPath->nStep ; ++i ){
		VmDeferStep *pStep = &pPath->aStep[i];
		ph7_value out;
		PH7_MemObjInit(&(*pVm),&out);
		if( pStep->isProp ){
			ph7_value nameVal;
			PH7_MemObjInitFromString(&(*pVm),&nameVal,&pStep->sProp);
			rc = VmReDriveStep(&(*pVm),PH7_OP_MEMBER,PH7_MEMBER_READ,&cur,&nameVal,&out);
			PH7_MemObjRelease(&nameVal);
		}else{
			rc = VmReDriveStep(&(*pVm),PH7_OP_LOAD_IDX,0,&cur,&pStep->sKey,&out);
		}
		PH7_MemObjRelease(&cur);
		cur = out;
		if( rc != SXRET_OK ){
			PH7_MemObjRelease(&cur);
			return rc;
		}
	}
	PH7_MemObjStore(&cur,pSlot);
	pSlot->nIdx = SXU32_HIGH;
	PH7_MemObjRelease(&cur);
	return SXRET_OK;
}
/*
 * D1: resolve deferred call arguments in [pArg, pTos) before the callee consumes them.
 *
 * A plain `$var` call argument whose callee signature is unknown at compile time is
 * emitted as a DEFERRED load (OP_LOAD iP1=1,iP2=3): when the variable does not exist it
 * yields a NULL slot tagged MEMOBJ_AUX_DEFERRED that carries the variable name in x.pOther
 * (a VM-lifetime bytecode string). This helper runs at each OP_CALL dispatch branch, while
 * pVm->pFrame is still the CALLER frame, once the callee's by-ref shape is known:
 *
 *   by-ref position  -> create the variable in the caller frame now and give the slot its
 *                       real nIdx so the ordinary by-ref binder aliases it (silent, as php).
 *   by-value position-> raise php's "Undefined variable $x" and pass a clean NULL WITHOUT
 *                       creating the variable in the caller.
 *
 * The by-ref decision for positional argument n comes from, in priority order:
 *   bAllByValue  -> everything by-value (an unresolvable/erroring callee);
 *   bAllByRef    -> everything by-ref (the indirect array-callable/__invoke dispatch paths,
 *                   which historically over-vivified every plain-var arg — preserved here
 *                   rather than regressed; their by-value refinement is a later slice);
 *   pFormal      -> a user function's formal-argument array (honoring a trailing variadic);
 *   nByRefMask   -> a builtin by-ref position bitmask (used when pFormal == 0).
 *
 * A DEFINED variable never carries the marker (it loads with its real nIdx), so this is a
 * no-op for it; a call with no deferred args pays only one flag test per slot.
 */
static sxi32 VmResolveDeferredArgs(
	ph7_vm *pVm,
	ph7_value *pArg,
	ph7_value *pTos,
	ph7_vm_func_arg *pFormal,
	sxu32 nFormal,
	sxu32 nByRefMask,
	int bAllByRef,
	int bAllByValue)
{
	ph7_value *p;
	sxu32 n = 0;
	for( p = pArg ; p < pTos ; ++p, ++n ){
		int bByRef = 0;
		SyString sName;
		if( (p->iFlags & (MEMOBJ_AUX_DEFERRED|MEMOBJ_AUX_DEFPATH)) == 0 ){
			continue;
		}
		if( bAllByValue ){
			bByRef = 0;
		}else if( bAllByRef ){
			/* bAllByRef comes ONLY from the array-callable-value and __invoke dispatch paths,
			 * which cannot expose the target's per-parameter by-ref flags here. Commit 1 chose
			 * to over-vivify every deferred PLAIN-VAR arg on those paths (harmless: it just
			 * materializes the caller variable), and that is preserved. But a deferred
			 * ELEMENT/PROPERTY (MEMOBJ_AUX_DEFPATH) must NOT be blanket-vivified there: a missing
			 * property would fatal ("Cannot create dynamic property") and a missing element would
			 * silently vivify + swallow php's "Undefined array key" warning. Resolve those
			 * by-value (warn + pass NULL), which matches php for a by-VALUE __invoke/callable —
			 * the genuine by-ref-out-param-into-an-element case stays unsupported here, exactly
			 * as it was before this slice. */
			bByRef = (p->iFlags & MEMOBJ_AUX_DEFPATH) ? 0 : 1;
		}else if( pFormal ){
			sxu32 idx = n;
			if( idx >= nFormal ){
				/* Beyond the declared formals: a trailing variadic absorbs the tail
				 * (and dictates its by-ref-ness); otherwise the extra arg is by-value. */
				idx = (nFormal > 0 && (pFormal[nFormal-1].iFlags & VM_FUNC_ARG_VARIADIC))
					? nFormal - 1 : SXU32_HIGH;
			}
			if( idx != SXU32_HIGH ){
				bByRef = (pFormal[idx].iFlags & VM_FUNC_ARG_BY_REF) != 0;
			}
		}else{
			bByRef = (n < 31 && (nByRefMask & (1u << n))) ? 1 : 0;
		}
		if( p->iFlags & MEMOBJ_AUX_DEFPATH ){
			/* D1 commit 2: a deferred array-element/property lvalue. Detach the descriptor
			 * FIRST (so an exception mid-resolve, or a later stack release, cannot double-free
			 * it) then re-walk it in the chosen mode. */
			VmDeferredPath *pPath = (VmDeferredPath *)p->x.pOther;
			sxi32 rc;
			p->iFlags &= ~MEMOBJ_AUX_DEFPATH;
			p->x.pOther = 0;
			MemObjSetType(p,MEMOBJ_NULL);
			p->nIdx = SXU32_HIGH;
			if( bByRef ){
				rc = VmResolvePathByRef(&(*pVm),pPath,p);
			}else{
				rc = VmResolvePathByValue(&(*pVm),pPath,p);
			}
			VmFreeDeferredPath(pPath);
			if( rc != SXRET_OK ){
				return rc;
			}
			continue;
		}
		/* Recover the deferred variable name and drop the marker + carrier. */
		SyStringInitFromBuf(&sName,(const char *)p->x.pOther,
			p->x.pOther ? SyStrlen((const char *)p->x.pOther) : 0);
		p->iFlags &= ~MEMOBJ_AUX_DEFERRED;
		p->x.pOther = 0;
		if( bByRef ){
			/* Materialize in the caller frame; the value stays NULL, only the slot
			 * back-reference (nIdx) matters for the by-ref binder / write-back. */
			ph7_value *pObj = VmExtractMemObj(&(*pVm),&sName,FALSE,TRUE);
			if( pObj ){
				p->nIdx = pObj->nIdx;
			}
		}else{
			/* php warns and passes NULL without creating the variable; the slot is
			 * already a clean NULL with nIdx == SXU32_HIGH from the deferred load. */
			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);
		}
	}
	return SXRET_OK;
}
/*
 * Did resolving a class NAME raise?
 *
 * The lookup can run an AUTOLOADER, and that autoloader can throw. The boundary rail
 * either parks the status in nBoundaryRc or — when a try caught it in place — records a
 * resume frame; either way the throw is already the engine's to land. A call site that
 * sees the class "missing" and piles its own `Class "X" not found` Error on top reports a
 * failure php never reports, and that second Error belongs to nobody: it came back
 * UNCAUGHT and killed the script right after the real exception had been handled.
 *
 * Snapshot (nBoundaryRc, pResumeFrame) before the lookup and pass them here after.
 */
PH7_PRIVATE int PH7_VmClassLookupRaised(ph7_vm *pVm,sxi32 nBrcBefore,const void *pResumeBefore)
{
	return pVm->nBoundaryRc != nBrcBefore || (const void *)pVm->pResumeFrame != pResumeBefore;
}
/*
 * Name php's error for a class+method callable that the DIRECT `$cb()` dispatch cannot
 * call, or return 0 when it resolves.
 *
 * The shared dispatcher (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL
 * result for an unresolvable pair — silence a caller cannot detect — so the direct call
 * site has to decide for itself. It used to do that only for the ARRAY form; the
 * `"Class::method"` STRING form went straight to the dispatcher, and `$cb='C::nosuch'`
 * evaluated to NULL with no diagnostic at all where php throws.
 *
 * pClass is the resolved target class (0 when the name named nothing); zCls/nCls is the
 * class name AS WRITTEN, which is what php's not-found message quotes. bStaticForm says the
 * target was a class NAME rather than an object. Messages that interpolate a name are built
 * into zBuf.
 *
 * Visibility is NOT decided here: an inaccessible method is diagnosed downstream by the
 * dispatch itself ("Call to private method C::p() from global scope"), php-exact already —
 * and php reports visibility BEFORE staticness, so the static rule below has to stay quiet
 * for a method this scope could not reach anyway.
 */
static const char * VmCallableClassMethodError(
	ph7_vm *pVm,
	ph7_class *pClass,             /* Resolved target class, or 0 */
	const char *zCls,sxu32 nCls,   /* Its name as the callable wrote it */
	const char *zMeth,sxu32 nMeth, /* The method name */
	int bStaticForm,               /* TRUE when the target is a class NAME, not an object */
	char *zBuf,int nBuf            /* Scratch for the messages that quote a name */
	)
{
	ph7_class_method *pMethod;
	ph7_class *pDecl;
	SyString sMeth;
	if( pClass == 0 ){
		SyBufferFormat(zBuf,nBuf,"Class \"%.*s\" not found",(int)nCls,zCls);
		return zBuf;
	}
	pMethod = PH7_ClassExtractMethod(pClass,zMeth,nMeth);
	if( pMethod == 0 ){
		/* A class that answers for unknown names through the catch-all has nothing to
		 * report: php runs __callStatic (class-name target) / __call (object target) for
		 * ANY method name, and the dispatcher below routes it. */
		const char *zMagic = bStaticForm ? "__callStatic" : "__call";
		if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) ){
			return 0;
		}
		SyBufferFormat(zBuf,nBuf,"Call to undefined method %z::%.*s()",
			&pClass->sName,(int)nMeth,zMeth);
		return zBuf;
	}
	/* An ABSTRACT method (an interface's included) has no body to call — the same message
	 * the `C::m()` SYNTAX raises in vm_ops_oo.c, which this dispatch reached only as a
	 * mangled internal function name. */
	SyStringInitFromBuf(&sMeth,zMeth,nMeth);
	if( pMethod->iFlags & PH7_CLASS_ATTR_ABSTRACT ){
		SyBufferFormat(zBuf,nBuf,"Cannot call abstract method %z::%z()",&pClass->sName,&sMeth);
		return zBuf;
	}
	/* Named through a class NAME, a non-static method is never callable: php refuses even
	 * when the CALLER has a compatible $this (unlike call_user_func, which binds it). The
	 * message names the DECLARING class and the method's declared spelling. */
	pDecl = pMethod->sFunc.pUserData ? (ph7_class *)pMethod->sFunc.pUserData : pClass;
	if( bStaticForm && (pMethod->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){
		SyString sDecl;
		SyStringInitFromBuf(&sDecl,SyStringData(&pMethod->sFunc.sName),
			SyStringLength(&pMethod->sFunc.sName));
		if( pMethod->iProtection == PH7_CLASS_PROT_PUBLIC
		 || PH7_VmClassMemberAccess(&(*pVm),pDecl,&sDecl,pMethod->iProtection,FALSE) ){
			SyBufferFormat(zBuf,nBuf,"Non-static method %z::%z() cannot be called statically",
				&pDecl->sName,&sDecl);
			return zBuf;
		}
	}
	return 0;
}
/*
 * The same check for the ARRAY form, whose two members carry php's own shape messages
 * before anything is resolved: the target must be an object or a class-name string, the
 * method must be a string. php probes them in that order (`[5,5]` names the FIRST member,
 * `['NoSuch',5]` the SECOND — the member shape decides before the class is looked up).
 */
static const char * VmDirectArrayCallableError(ph7_vm *pVm,ph7_value *pTarget,ph7_value *pMethod,
	char *zBuf,int nBuf)
{
	ph7_class *pClass;
	if( (pTarget->iFlags & (MEMOBJ_OBJ|MEMOBJ_STRING)) == 0 ){
		return "First array member is not a valid class name or object";
	}
	if( (pMethod->iFlags & MEMOBJ_STRING) == 0 ){
		return "Second array member is not a valid method";
	}
	pClass = PH7_VmExtractClassFromValue(&(*pVm),pTarget);
	return VmCallableClassMethodError(&(*pVm),pClass,
		(const char *)SyBlobData(&pTarget->sBlob),SyBlobLength(&pTarget->sBlob),
		(const char *)SyBlobData(&pMethod->sBlob),SyBlobLength(&pMethod->sBlob),
		/* An OBJECT target carries its own $this; only a class NAME is the static form. */
		(pTarget->iFlags & MEMOBJ_OBJ) ? FALSE : TRUE,
		zBuf,nBuf);
}
/*
 * Split a `"Class::method"` callable string. php scans for the LAST "::" (zend_memrchr),
 * so `"C::s::x"` names the class `C::s` — not `C` — and reports it not found. Returns TRUE
 * and the two halves (either may be empty: `"C::"` and `"::s"` are shapes php accepts here
 * and rejects further down), FALSE when the string carries no "::" at all.
 */
PH7_PRIVATE int PH7_VmCallableStringParts(const char *zName,sxu32 nName,
	const char **pzCls,sxu32 *pnCls,const char **pzMeth,sxu32 *pnMeth)
{
	sxu32 i;
	for( i = nName ; i >= 2 ; --i ){
		if( zName[i-2] == ':' && zName[i-1] == ':' ){
			*pzCls = zName;
			*pnCls = i - 2;
			*pzMeth = &zName[i];
			*pnMeth = nName - i;
			return TRUE;
		}
	}
	return FALSE;
}
static sxi32 VmByteCodeExecBody(
	ph7_vm *pVm,         /* Target VM */
	VmInstr *aInstr,     /* PH7 bytecode program */
	ph7_value *pStack,   /* Operand stack */
	int nTos,            /* Top entry in the operand stack (usually -1) */
	ph7_value *pResult,  /* Store program return value here. NULL otherwise */
	sxu32 *pLastRef,     /* Last referenced ph7_value index */
	int is_callback,     /* TRUE if we are executing a callback */
	sxi32 nPc,           /* Starting program counter (0 for normal, >0 for resume) */
	ph7_vm_func *pEnforceRetFunc, /* NULL except when this invocation is a user-fn body; when set, the terminating OP_DONE validates the return value against pEnforceRetFunc's declared type. */
	int bReturnPropagates, /* TRUE only for a catch/finally mini-program: an explicit-return OP_DONE (iP2=1) defers its value onto the enclosing body frame's sRet slot for that body to return. */
	VmParkedSegment *pAdoptSegment, /* NULL except on a deep-fiber RESUME (VmResumeCtx): the parked record segment this body invocation re-enters inside (BYTECODE stage 4). */
	ph7_value **ppBaseOwner, /* Base (pCallTop==0) operand-stack owner slot the native entry frees; OP_SPREAD growth of the base stack writes the new pointer here (see VmGrowOperandStack). */
	sxu32 *pnBaseCap, /* Base stack capacity (persisted across coroutine suspend/resume); read for the initial capacity and updated on base-stack growth. */
	sxu32 nStackOrig /* Base stack's ORIGINAL (ungrown) allocation size — the fixed headroom reference for OP_SPREAD growth (see nStackOrig in VmExecState). */
	)
{
	VmInstr *pInstr;
	ph7_value *pTos;
	SySet aArg;
	VmCallFrame *pCallTop = 0; /* Top of this invocation's in-loop call-record
	                            * stack (BYTECODE stage 2); NULL = executing the
	                            * bottom activation. */
	VmExecState sState; /* This activation's boundary state (BYTECODE.md stage 1):
	                     * everything a suspended/nested activation must restore.
	                     * pc/pTos stay in locals for the hot loop and are synced
	                     * into sState only around the call epilogue (stage 2 turns
	                     * that boundary into an explicit record push/pop). */
	sxi32 pc;
	sxi32 rc;
	sState.aInstr = aInstr;
	sState.pStack = pStack;
	sState.nStackCap = pnBaseCap ? *pnBaseCap : 0; /* CURRENT capacity (grown on a coroutine resume) */
	sState.nStackOrig = nStackOrig;                /* ORIGINAL capacity — fixed headroom reference */
	sState.pResult = pResult;
	sState.pLastRef = pLastRef;
	sState.pEnforceRetFunc = pEnforceRetFunc;
	sState.is_callback = (sxu8)(is_callback ? 1 : 0);
	sState.bReturnPropagates = (sxu8)(bReturnPropagates ? 1 : 0);
	/* Argument container */
	SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));
	if( nTos < 0 ){
		pTos = &pStack[-1];
	}else{
		pTos = &pStack[nTos];
	}
	sState.pTos = pTos;
	sState.pc = nPc;
	/* Finally-drain base. For a resumed generator/fiber TOP-LEVEL body, its own
	 * exception handlers were just re-published above the caller depth
	 * (VmRestoreCtxState), so the live SySetUsed over-counts; take the
	 * caller-depth base recorded on the ctx instead.
	 *
	 * The discriminator is the OPERAND STACK, not the frame: the body exec runs on
	 * the ctx's own pStack, whereas every nested frame-less mini-program run within
	 * it — a default-argument / constructor trampoline, or a catch/finally body via
	 * VmLocalExec — runs on a FRESH operand stack while SHARING pVm->pFrame (those
	 * mini-programs do not push a VM frame). A pFrame-only guard therefore misfires
	 * for such a mini-program and hands it the generator's low caller-base; its
	 * terminal OP_DONE then drains VmDrainFinally down to that base, tearing down a
	 * live try that the surrounding finally had just opened (e.g. `finally { try {
	 * throw new E(); } catch (E) {} }` in a generator: the `new E()` trampoline's
	 * OP_DONE popped the inner try before its OP_THROW ran, so the throw escaped
	 * uncaught). Requiring pStack == pCtx->pStack pins the override to the resumed
	 * body itself; nested mini-programs fall through to the correct live depth. */
	if( pVm->pActiveCtx && pVm->pActiveCtx->pFrame == pVm->pFrame
	 && pStack == pVm->pActiveCtx->pStack ){
		sState.nExceptionBase = pVm->pActiveCtx->nExceptionBase;
		sState.nFinallyActBase = pVm->pActiveCtx->nFinallyBase;
	}else{
		sState.nExceptionBase = SySetUsed(&pVm->aException);
		sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);
	}
	sState.pEntryFrame = pVm->pFrame;
	pc = nPc;
	/* BYTECODE stage 4: a resumed fiber whose suspend was deep in a nested call
	 * adopts its parked record segment here. VmResumeCtx hands the segment in
	 * explicitly (pAdoptSegment) — set only for the body invocation, never for a
	 * nested mini-program/callback run inside the resumed body — after it rebased
	 * the segment's exception floors and pushed the resume value into the innermost
	 * stack. Locals switch to the innermost activation so the dispatch loop
	 * continues inside the callee; the record chain is restored so its completion
	 * unwinds back through the body. */
	if( pAdoptSegment ){
		VmParkedSegment *pSeg = pAdoptSegment;
		pCallTop = pSeg->pCallTop;
		sState = pSeg->sState;
		aInstr = sState.aInstr;
		pStack = sState.pStack;
		/* pc is already nPc (== pCtx->pc, the innermost's post-suspend pc) from the
		 * init above; only the stack/top move to the innermost activation. */
		pTos = &pStack[nTos];   /* nTos == pCtx->nTos — innermost, resume value pushed */
		SyMemBackendFree(&pVm->sAllocator,pSeg); /* holder only; its contents are now live */
	}
/*
 * Route an enforcement helper's (or yield-from delegate's) return code from inside
 * the main switch: proceed on SXRET_OK, abort on PH7_ABORT, and on PH7_EXCEPTION
 * resume at the landing pad of the body that actually caught the exception in place
 * (VmRecordedResume) or, when it was caught by an outer exec, unwind out of the VM
 * loop. Replaces the old "jump to pVm->pFrame's nearest iExceptionJump" which ran
 * the statement after the try even when the catch was at an enclosing frame (ROOT B,
 * face b — `yield from` over a throwing sub-generator).
 */
/* Dispatch-routing macros: bodies live in vm_dispatch.h, written against the
 * VM_EXIT_* primitives. Here they bind to the loop's own control flow. */
#define VM_EXIT_BREAK break
#define VM_EXIT_ABORT goto Abort
#define VM_EXIT_EXCEPTION goto Exception
#include "vm_dispatch.h"
	/* Generator::throw() inject-at-yield: when this invocation is a resumed generator/fiber
	 * body carrying a pending injected exception, raise it HERE — once, before the dispatch
	 * loop (pc is already at the resume point) — so the existing OP_THROW route
	 * (VmThrowException + VmRecordedResume) lands it at the generator's own try/catch landing
	 * pad WITHOUT reconstructing the suspended exception frame on pVm->pFrame. Because
	 * pVm->pFrame stays the body, the return/finally/sRet subsystem is untouched (this is why
	 * the reverted frame-reconstruction approach's regression cannot recur). Behaviorally
	 * identical to a `throw` executed at the yield point: if the generator's own try catches
	 * it we resume after the try; otherwise it propagates to the throw() caller (ROOT B lands
	 * the caller's handler) and the ctx closes. pInjected is one-shot and only meaningful at
	 * the resume pc, so this is checked once at entry — NOT per-instruction — keeping the hot
	 * dispatch loop untouched for all normal code. The pFrame gate keeps a nested call / catch
	 * mini-program sharing the ctx (a separate VmByteCodeExec entry) from re-firing.
	 *
	 * Exception: when this body is suspended mid `yield from` over an inner Generator
	 * (iDelegateState==3), a Generator::throw() on the OUTER generator must be forwarded
	 * INTO the delegate (PHP yield-from transparency), not raised here. Leave pInjected
	 * set and skip; the resume pc is that OP_YIELD_FROM, which consumes and forwards it. */
	if( pVm->pActiveCtx && pVm->pActiveCtx->pInjected
	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame
	 && pVm->pActiveCtx->iDelegateState != 3 ){
		ph7_class_instance *pInj = pVm->pActiveCtx->pInjected;
		VmFrame *pThrowFrame;
		sxi32 iResumePc;
		pVm->pActiveCtx->pInjected = 0; /* one-shot consume */
		/* Raising the injection at the yield-from point abandons any array/Iterator
		 * delegation in progress (state 1/2 — state 3 was forwarded, not raised here).
		 * Tear the delegate down so that if the generator's own try catches this and
		 * runs on to a LATER `yield from`, that opcode classifies its operand fresh
		 * instead of resuming this now-stale delegate cursor. */
		if( pVm->pActiveCtx->iDelegateState != 0 ){
			PH7_MemObjRelease(&pVm->pActiveCtx->sDelegate);
			pVm->pActiveCtx->pDelegateNode = 0;
			pVm->pActiveCtx->iDelegateState = 0;
		}
		pThrowFrame = VmSkipExceptionFrames(pVm->pFrame);
		pThrowFrame->iFlags |= VM_FRAME_THROW;
		rc = VmThrowException(&(*pVm),pInj);
		if( rc == SXERR_ABORT ){
			goto Abort;
		}
		if( pVm->pInlineInstr == (void *)aInstr ){
			/* ROOT C: the inject was caught by an inline try in THIS generator. Drain the
			 * abandoned mid-expression operands and land at the catch/finally body. This is
			 * the pre-loop path (the first fetch uses pc directly), so no -1 adjustment. */
			while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){
				PH7_MemObjRelease(pTos);
				pTos--;
			}
			pc = (sxi32)pVm->iInlinePc;
			pVm->pInlineInstr = 0;
		}else if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){
			/* Caught by THIS generator's own try. (VmRecordedResume returns FALSE unless a
			 * catch recorded a resume target for this exec, so rc need not be pre-checked;
			 * unlike OP_THROW there is no lexical-try fallthrough here — the else just
			 * propagates.) Drain the abandoned mid-expression operand slots back to the
			 * catching try's base, then land at its pad (iResumePc is landing-1 for the
			 * dispatcher's trailing pc++; the loop below fetches at pc with no leading pc++,
			 * so add 1 to land on the pad itself). */
			while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){
				PH7_MemObjRelease(pTos);
				pTos--;
			}
			pc = iResumePc + 1;
		}else{
			/* Not caught in this generator (no match, or caught by an outer/caller frame
			 * that ROOT B will land at its own OP_CALL site): propagate out so the ctx
			 * closes and the caller sees the exception. */
			goto Exception;
		}
	}
	/* Force-close entry (VmCloseCtx): a suspended generator being destroyed runs its
	 * pending `finally` blocks. Instead of resuming at the yield, redirect straight
	 * into the innermost open try's finally as if a `return` had crossed every
	 * enclosing finally (mirrors OP_SET_FINALLY_RET; OP_END_FINALLY then threads the
	 * PH7_FA_RETURN out through the whole chain and completes the body). Gate on the
	 * BODY bytecode (aInstr == the function program) so nested mini-programs sharing
	 * this ctx/frame never re-fire it; bClosing stays set so OP_YIELD can reject a
	 * yield reached inside one of these finallys. */
	if( pVm->pActiveCtx && pVm->pActiveCtx->bClosing
	 && pVm->pActiveCtx->pFrame == sState.pEntryFrame
	 && aInstr == (VmInstr *)SySetBasePtr(&pVm->pActiveCtx->pFunc->aByteCode) ){
		VmFinallyAction sAct;
		sxu32 iFpc = 0;
		int nCross = -1; /* cross every enclosing finally of this body */
		SyZero(&sAct,sizeof(sAct));
		sAct.eKind = PH7_FA_RETURN;
		sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);
		PH7_MemObjInit(pVm,&sAct.sRet); /* discarded return; getReturn() is moot post-close */
		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){
			sAct.nCross = nCross;
			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);
			pc = (sxi32)iFpc; /* pre-loop: the first fetch uses pc directly (no -1) */
		}else{
			/* No open try had a finally: nothing to run, complete the body. */
			PH7_MemObjRelease(&sAct.sRet);
			goto Done;
		}
	}
	/* Execute as much as we can */
	for(;;){
VmLoopFetch:
		/* C-boundary throw routing (band A #1): a PHP callee invoked from a C
		 * site with no status channel — a __toString/__toInt cast, __get/__set/
		 * offsetGet/offsetSet, __clone, __destruct, a user callback inside a
		 * builtin — raised, and the site continued with a fallback value; the
		 * invocation boundary parked the status here (VmBoundaryPark). Route it
		 * exactly as the throw site's dispatch macro would have: abort, land at
		 * an inline-try redirect, resume at a recorded in-place catch (draining
		 * the abandoned mid-expression operands to the catching try's base), or
		 * propagate out of this exec. Checked at the fetch point, so a swallowed
		 * throw outlives at most the C remainder of ONE opcode instead of
		 * silently resuming the surrounding PHP code with a bogus value.
		 * The pending write-back sweep shares this one guard so the hot
		 * no-hooks path pays a single predicted branch per fetch. */
		if( pVm->nBoundaryRc != 0 || SySetUsed(&pVm->aHookRmw) > 0 ){
			if( pVm->nBoundaryRc != 0 ){
				sxi32 rcBr = pVm->nBoundaryRc;
				pVm->nBoundaryRc = 0;
				if( rcBr == PH7_ABORT ){
					goto Abort;
				}
				if( pVm->pInlineInstr == (void *)aInstr ){
					/* Caught by an inline try (generator body) THIS exec owns: drain
					 * and land (pre-fetch path: pc is used directly, no trailing ++). */
					while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){
						PH7_MemObjRelease(pTos);
						pTos--;
					}
					pc = (sxi32)pVm->iInlinePc;
					pVm->pInlineInstr = 0;
				}else{
					sxi32 iBrPc;
					if( VmRecordedResume(pVm,&iBrPc,sState.pEntryFrame,aInstr) ){
						/* Caught in place by a try THIS exec owns: drain the abandoned
						 * operands to the catching try's base and land at its pad
						 * (iBrPc is landing-1 for the dispatcher's trailing pc++;
						 * pre-fetch here, so +1 lands on the pad itself). */
						while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){
							PH7_MemObjRelease(pTos);
							pTos--;
						}
						pc = iBrPc + 1;
					}else{
						/* Caught by an outer exec (or uncaught-with-report pending):
						 * unwind out of this exec; the owner's macros/router land it. */
						goto Exception;
					}
				}
			}
			/* Stale write-back sweep: a pending entry whose OWNING activation is
			 * fetching any pc outside its armed window [nJmpPc, nPc] is dead — for
			 * an RMW entry (window = the modify op alone) a routed throw abandoned
			 * the arming statement mid-flight; for a ??= entry either a throw
			 * abandoned the RHS or the short-circuit jump landed past the
			 * OP_NULLC_STORE (the assign is skipped). Drop it — no set dispatch
			 * (php: the throw/skip discards the write). A nested exec (even a
			 * recursive one over the same bytecode) has a different operand-stack
			 * base and leaves enclosing entries alone; entries below a live top
			 * are reached as the drops expose them. */
			while( SySetUsed(&pVm->aHookRmw) > 0 ){
				VmHookRmw *pTopRmw = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);
				if( pTopRmw->pOwnerStack != (void *)pStack || pTopRmw->pInstrs != (void *)aInstr
				 || ((sxu32)pc >= pTopRmw->nJmpPc && (sxu32)pc <= pTopRmw->nPc) ){
					break; /* not ours, or legitimately in flight */
				}
				VmHookRmwDropTop(&(*pVm));
			}
		}
		/* Fetch the instruction to execute */
		pInstr = &aInstr[pc];
		if( pInstr->nLine ){
			/* Publish the source position for diagnostics, debug_backtrace() and
			 * Throwable. Instructions the compiler could not attribute (nLine 0)
			 * leave the last known line standing rather than reporting line 0. */
			pVm->nCurLine = pInstr->nLine;
		}
		rc = SXRET_OK;
/*
 * What follows here is a massive switch statement where each case implements a
 * separate instruction in the virtual machine.  If we follow the usual
 * indentation convention each case should be indented by 6 spaces.  But
 * that is a lot of wasted space on the left margin.  So the code within
 * the switch statement will break with convention and be flush-left.
 */
		switch(pInstr->iOp){
/*
 * DONE: P1 * *
 *
 * Program execution completed: Clean up the mess left behind
 * and return immediately.
 */
case PH7_OP_DONE:
	if( pInstr->iP2 && sState.bReturnPropagates ){
		/* Explicit `return` inside a catch/finally mini-program. Defer the value
		 * onto the body frame this catch/finally returns from (skip the transparent
		 * exception/catch wrappers); the enclosing body's OP_DONE / OP_POP_EXCEPTION
		 * materializes it into sState.pResult. Drain any finally opened within this body
		 * first (nested try/finally inside the catch), which may overwrite the same
		 * frame's slot (finally-over-catch). */
		VmFrame *pTgt = VmSkipExceptionFrames(pVm->pFrame);
		if( pInstr->iP1 && pTos >= pStack ){
			PH7_MemObjStore(pTos,&pTgt->sRet);
			VmPopOperand(&pTos,1);
		}else{
			PH7_MemObjRelease(&pTgt->sRet); /* bare `return;` -> null */
		}
		pTgt->bHasRet = 1;
		pTgt->nRetGen++;
		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);
		if( rc == SXERR_ABORT ){
			goto Abort;
		}
		if( rc == PH7_EXCEPTION ){
			/* A drained finally threw past itself — it discards this return. */
			goto Exception;
		}
		goto Done;
	}
	/* Return-type enforcement: only the user-function CALL handler (and
	 * the fiber start/resume paths) set sState.pEnforceRetFunc, so this branch is
	 * skipped for default-value bytecode, class-method mini-programs,
	 * callback trampolines, and the main script. */
	if( sState.pEnforceRetFunc && VmFuncHasReturnType(sState.pEnforceRetFunc)
	 && !(VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW) ){
		/* The VM_FRAME_THROW guard skips enforcement when the function is
		 * unwinding because an exception was thrown (the compiler routes an
		 * uncaught throw to this terminal OP_DONE): PHP does not type-check a
		 * value the function never actually returned, so enforcing here would
		 * raise a spurious "Return value must be of type X" over the real
		 * exception. */
		ph7_value *pRetVal = 0;
		if( pInstr->iP1 && pTos >= pStack ){
			pRetVal = pTos;
		}
		rc = VmEnforceReturnType(&(*pVm), sState.pEnforceRetFunc, pRetVal);
		if( rc == PH7_ABORT ) goto Abort;
		if( rc == PH7_EXCEPTION ){
			if( pInstr->iP1 && pTos >= pStack ){
				PH7_MemObjRelease(pTos);
				pTos--;
			}
			goto Exception;
		}
		/* Don't enforce twice if the function loops through multiple
		 * OP_DONEs (it shouldn't — compilers emit one terminal DONE — but
		 * defensively we clear the pointer after a successful check). */
		sState.pEnforceRetFunc = 0;
	}
	if( pInstr->iP1 && pTos >= pStack ){
		if( sState.pLastRef ){
			*sState.pLastRef = pTos->nIdx;
		}
		if( sState.pResult ){
			/* Execution result */
			PH7_MemObjStore(pTos,sState.pResult);
		}
		VmPopOperand(&pTos,1);
	}else if( sState.pLastRef ){
		/* Nothing referenced — also the throw-unwind path: the compiler routes
		 * an uncaught exception to this terminal OP_DONE with iP1 set but an
		 * empty operand stack (pTos == pStack-1), so there is no return value to
		 * store. Guarding on pTos >= pStack (matching the two sibling branches
		 * above) avoids the below-base read that crashed under glibc/ASan. */
		*sState.pLastRef = SXU32_HIGH;
	}
	/* Execute pending finally blocks for any try/catch contexts pushed during
	 * this execution. When 'return' is used inside a try block,
	 * PH7_OP_POP_EXCEPTION is bypassed. We must run finally blocks before
	 * returning. Only drain entries above sState.nExceptionBase to avoid interfering
	 * with exception contexts from an outer VmByteCodeExec invocation.
	 * This runs AFTER storing the return value so that 'return' in a finally
	 * block can override it (the finally writes this body frame's sRet slot,
	 * materialized below).
	 */
	rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);
	if( rc == SXERR_ABORT ){
		goto Abort;
	}
	if( rc == PH7_EXCEPTION ){
		/* A drained finally threw past itself, discarding the value this OP_DONE
		 * stored into sState.pResult. If an enclosing try IN THIS function caught the new
		 * exception in place, resume at its landing pad; otherwise unwind (the
		 * caller's exception-resume pops the stored result). */
		sxi32 iResumePc;
		if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){
			pc = iResumePc;
			break;
		}
		goto Exception;
	}
	if( sState.pEntryFrame->bHasRet && !sState.bReturnPropagates ){
		/* A catch/finally issued a 'return' targeting THIS body. If the body is
		 * actually unwinding because an exception escaped it (terminal OP_DONE on
		 * the throw-unwind path, VM_FRAME_THROW set — same guard as the return-type
		 * enforcement above), that exception supersedes the return: discard it.
		 * Otherwise materialize it as this function's result. */
		if( VmSkipExceptionFrames(pVm->pFrame)->iFlags & VM_FRAME_THROW ){
			VmClearFrameReturn(sState.pEntryFrame);
		}else{
			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);
		}
	}
	goto Done;
/*
 * HALT: P1 * *
 *
 * Program execution aborted: Clean up the mess left behind
 * and abort immediately.
 */
case PH7_OP_HALT:
	if( pInstr->iP1 ){
#ifdef UNTRUST
		if( pTos < pStack ){
			goto Abort;
		}
#endif
		if( sState.pLastRef ){
			*sState.pLastRef = pTos->nIdx;
		}
		if( pTos->iFlags & MEMOBJ_STRING ){
			if( SyBlobLength(&pTos->sBlob) > 0 ){
				/* Output the exit message */
				pVm->sVmConsumer.xConsumer(SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),
					pVm->sVmConsumer.pUserData);
				VmTrackOutput(pVm, SyBlobLength(&pTos->sBlob));
			}
		}else if(pTos->iFlags & MEMOBJ_INT ){
			/* Record exit status */
			pVm->iExitStatus = (sxi32)pTos->x.iVal;
		}
		VmPopOperand(&pTos,1);
	}else if( sState.pLastRef ){
		/* Nothing referenced */
		*sState.pLastRef = SXU32_HIGH;
	}
	/* Request a VM-wide halt so the abort cascades out of any enclosing
	 * include/require/eval execution unit; shutdown callbacks then run
	 * at the top level (PHP semantics) instead of hard-exiting here.
	 */
	pVm->bHaltRequested = 1;
	goto Abort;
/*
 * JMP: * P2 *
 *
 * Unconditional jump: The next instruction executed will be
 * the one at index P2 from the beginning of the program.
 */
case PH7_OP_JMP:
	pc = pInstr->iP2 - 1;
	break;
/*
 * JZ: P1 P2 *
 *
 * Take the jump if the top value is zero (FALSE jump).Pop the top most
 * entry in the stack if P1 is zero.
 */
case PH7_OP_JZ:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Get a boolean value */
	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){
		PH7_MemObjToBool(pTos);
	}
	if( !pTos->x.iVal ){
		/* Take the jump */
		pc = pInstr->iP2 - 1;
	}
	if( !pInstr->iP1 ){
		VmPopOperand(&pTos,1);
	}
	break;
/*
 * JNZ: P1 P2 *
 *
 * Take the jump if the top value is not zero (TRUE jump).Pop the top most
 * entry in the stack if P1 is zero.
 */
case PH7_OP_JNZ:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Get a boolean value */
	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){
		PH7_MemObjToBool(pTos);
	}
	if( pTos->x.iVal ){
		/* Take the jump */
		pc = pInstr->iP2 - 1;
	}
	if( !pInstr->iP1 ){
		VmPopOperand(&pTos,1);
	}
	break;
/*
 * NOOP: * * *
 *
 * Do nothing. This instruction is often useful as a jump
 * destination.
 */
case PH7_OP_NOOP:
	break;
/*
 * POP: P1 * *
 *
 * Pop P1 elements from the operand stack.
 */
case PH7_OP_POP: {
	sxi32 n = pInstr->iP1;
	if( &pTos[-n+1] < pStack ){
		/* TICKET 1433-51 Stack underflow must be handled at run-time */
		n = (sxi32)(pTos - pStack);
	}
	VmPopOperand(&pTos,n);
	break;
				 }
/*
 * DUP: * * *
 *
 * Duplicate the top of the stack.
 */
case PH7_OP_DUP:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	pTos++;
	PH7_MemObjInit(pVm,pTos);
	PH7_MemObjStore(pTos - 1,pTos);
	break;
/*
 * NSSWITCH: * * P3
 *
 * Switch the active namespace at runtime.
 * P3 points to the namespace string (pool-allocated, NULL for global).
 */
case PH7_OP_NSSWITCH:
	SyBlobReset(&pVm->sNamespace);
	if( pInstr->p3 ){
		const char *zNs = (const char *)pInstr->p3;
		SyBlobAppend(&pVm->sNamespace,zNs,SyStrlen(zNs));
	}
	break;
/*
 * CLASS_DEFER: * * P3
 *
 * Execute a class declaration whose parent/interface/trait could not be
 * resolved when the enclosing file compiled (its autoloader had not RUN yet).
 * P3 is the VmDeferredClass record captured by the compiler. A dependency
 * that is STILL missing throws php's catchable `... not found` Error; a
 * failed re-compile aborts (its errors were already reported). See the
 * deferral block comment in compile_class.c.
 */
case PH7_OP_CLASS_DEFER: {
	VmDeferredClass *pDefer = (VmDeferredClass *)pInstr->p3;
	VmDeferredReq *pMissing = 0;
	sxi32 rcDecl = SXRET_OK;
	if( pDefer ){
		rcDecl = VmExecDeferredClass(&(*pVm),pDefer,&pMissing);
	}
	if( pMissing ){
		static const char *azDeferKind[] = { "Class", "Interface", "Trait" };
		char zDeclMsg[520];
		sxu32 nDeclMsg = SyBufferFormat(zDeclMsg,sizeof(zDeclMsg),"%s \"%.*s\" not found",
			azDeferKind[pMissing->cKind < 3 ? pMissing->cKind : 0],
			(int)pMissing->sName.nByte,pMissing->sName.zString);
		rc = VmThrowFromVm(&(*pVm),"Error",zDeclMsg,nDeclMsg);
		if( rc == SXERR_ABORT ){
			goto Abort;
		}
		PH7_THROW_ROUTE_MIDEXPR(rc)
	}
	if( rcDecl == SXERR_ABORT ){
		goto Abort;
	}
	break;
				}
/*
 * CVT_INT: * * *
 *
 * Force the top of the stack to be an integer.
 */
case PH7_OP_CVT_INT:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if((pTos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pTos);
	}
	/* Invalidate any prior representation */
	MemObjSetType(pTos,MEMOBJ_INT);
	break;
/*
 * CVT_REAL: * * *
 *
 * Force the top of the stack to be a real.
 */
case PH7_OP_CVT_REAL:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if((pTos->iFlags & MEMOBJ_REAL) == 0 ){
		PH7_MemObjToReal(pTos);
	}
	/* Invalidate any prior representation */
	MemObjSetType(pTos,MEMOBJ_REAL);
	break;
/*
 * CVT_STR: * * *
 *
 * Force the top of the stack to be a string.
 */
case PH7_OP_CVT_STR:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* (string) cast + string interpolation "$arr": php's user-visible
	 * array->string warning site (§2). */
	PH7_MemObjToStringUV(pTos);
	break;
/*
 * CVT_BOOL: * * *
 *
 * Force the top of the stack to be a boolean.
 */
case PH7_OP_CVT_BOOL:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){
		PH7_MemObjToBool(pTos);
	}
	break;
/* PH7_OP_CVT_NULL, the '(unset)' cast, must never execute: emitting it
 * always raises "The (unset) cast is no longer supported" (php 8 removed
 * the cast), so a program containing it never compiles. The switch has no
 * default arm, so abort loudly rather than fall through as a silent no-op
 * if a future emitter ever produces one without the compile error. */
case PH7_OP_CVT_NULL:
	goto Abort;
/*
 * CVT_NUMC: * * *
 *
 * Force the top of the stack to be a numeric type (integer,real or both).
 */
case PH7_OP_CVT_NUMC:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Force a numeric cast */
	PH7_MemObjToNumeric(pTos);
	break;
/*
 * CVT_ARRAY: * * *
 *
 * Force the top of the stack to be a hashmap aka 'array'.
 */
case PH7_OP_CVT_ARRAY:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Force a hashmap cast */
	rc = PH7_MemObjToHashmap(pTos);
	if( rc != SXRET_OK ){
		/* Not so fatal,emit a simple warning */
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_WARNING,
			"PH7 engine is running out of memory while performing an array cast");
	}
	break;
/*
 * CVT_OBJ: * * *
 *
 * Force the top of the stack to be a class instance (Object in the PHP jargon).
 */
case PH7_OP_CVT_OBJ:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){
		/* Force a 'stdClass()' cast */
		PH7_MemObjToObject(pTos);
	}
	break;
/*
 * ERR_CTRL * * *
 *
 * Error control operator.
 */
case PH7_OP_UNSET_VAR: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpUnsetVar(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
case PH7_OP_ERR_CTRL:
	/*
	 * Error-control operator '@'. Emitted as a PAIR around the suppressed
	 * expression: iP1=1 opens the window (before the operand is evaluated),
	 * iP1=0 closes it (after). It was historically a no-op (ticket 1433-038),
	 * which went unnoticed only because the engine raised so few diagnostics;
	 * every warning/notice/deprecation the parity work added leaked straight
	 * through `@`. The window nests, and php 8 does NOT let '@' swallow
	 * fatals or exceptions — those unwind past the closing instruction, so
	 * the depth is reset on the exception path rather than decremented here.
	 */
	if( pInstr->iP1 ){
		pVm->nErrSuppress++;
	}else if( pVm->nErrSuppress > 0 ){
		pVm->nErrSuppress--;
	}
	break;
/*
 * IS_A * * *
 *
 * Pop the top two operands from the stack and check whether the first operand
 * is an object and is an instance of the second operand (which must be a string
 * holding a class name or an object).
 * Push TRUE on success. FALSE otherwise.
 */
case PH7_OP_IS_A:{
	ph7_value *pNos = &pTos[-1];
	sxi32 iRes = 0; /* assume false by default */
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	if( pNos->iFlags& MEMOBJ_OBJ ){
		ph7_class_instance *pThis = (ph7_class_instance *)pNos->x.pOther;
		ph7_class *pClass = 0;
		/* Extract the target class */
		if( pTos->iFlags & MEMOBJ_OBJ ){
			/* Instance already loaded */
			pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;
		}else if( pTos->iFlags & MEMOBJ_STRING && SyBlobLength(&pTos->sBlob) > 0 ){
			const char *zCls = (const char *)SyBlobData(&pTos->sBlob);
			sxu32 nCls = (sxu32)SyBlobLength(&pTos->sBlob);
			/* Handle self/static/parent keywords */
			if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){
				pClass = PH7_VmPeekDeclaringClass(&(*pVm));
			}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){
				pClass = PH7_VmPeekTopClass(&(*pVm));
			}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){
				pClass = PH7_VmResolveParentClass(&(*pVm));
			}else{
				pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);
			}
		}
		if( pClass ){
			/* Perform the query */
			iRes = PH7_VmInstanceOf(pThis->pClass,pClass);
		}
	}
	/* Push result */
	VmPopOperand(&pTos,1);
	PH7_MemObjRelease(pTos);
	pTos->x.iVal = iRes;
	MemObjSetType(pTos,MEMOBJ_BOOL);
	break;
				 }

/*
 * LOADC P1 P2 *
 *
 * Load a constant [i.e: PHP_EOL,PHP_OS,__TIME__,...] indexed at P2 in the constant pool.
 * If P1 is set,then this constant is candidate for expansion via user installable callbacks.
 */
case PH7_OP_LOADC: {
	ph7_value *pObj;
	/* Reserve a room */
	pTos++;
	if( pInstr->iP1 & PH7_LOADC_NOKEY ){
		/* Absent array-literal key: mark it so LOAD_MAP auto-indexes without a diagnostic */
		MemObjSetType(pTos,MEMOBJ_NULL);
		SyBlobReset(&pTos->sBlob);
		pTos->iFlags |= MEMOBJ_AUX_NOKEY;
		pTos->nIdx = SXU32_HIGH;
		break;
	}
	if( (pObj = (ph7_value *)SySetAt(&pVm->aLitObj,pInstr->iP2)) != 0 ){
		if( (pInstr->iP1 & PH7_LOADC_EXPAND) && SyBlobLength(&pObj->sBlob) <= 64 ){
			SyHashEntry *pEntry;
			/* The name php looks up FIRST for an unqualified constant is decided at
			 * COMPILE time and travels in p3: a `use const` import's FQN, or
			 * `current-namespace\NAME`. Absent (global scope, or an already-qualified
			 * literal), the bare literal is the only name there is. Resolving it here
			 * rather than against the RUNTIME namespace is what makes a function keep
			 * its own namespace when it is called from another one, and what lets a
			 * namespaced constant shadow a global one of the same short name. */
			const char *zCand = (const char *)pInstr->p3;
			const char *zLit = (const char *)SyBlobData(&pObj->sBlob);
			sxu32 nLit = (sxu32)SyBlobLength(&pObj->sBlob);
			if( zCand ){
				pEntry = SyHashGet(&pVm->hConstant,zCand,SyStrlen(zCand));
				if( pEntry ){
					ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;
					MemObjSetType(pTos,MEMOBJ_NULL);
					SyBlobReset(&pTos->sBlob);
					VmExpandConstantWithNotice(&(*pVm),pCons,pTos);
					pTos->nIdx = SXU32_HIGH;
					break;
				}
			}
			/* The GLOBAL step — skipped when the candidate came from an import, which
			 * php resolves without any fallback. */
			if( (pInstr->iP1 & PH7_LOADC_NOGLOBAL) == 0 ){
				pEntry = SyHashGet(&pVm->hConstant,(const void *)zLit,nLit);
				if( pEntry ){
					ph7_constant *pCons = (ph7_constant *)pEntry->pUserData;
					/* Set a NULL default value */
					MemObjSetType(pTos,MEMOBJ_NULL);
					SyBlobReset(&pTos->sBlob);
					/* Invoke the callback and deal with the expanded value */
					VmExpandConstantWithNotice(&(*pVm),pCons,pTos);
					/* Mark as constant */
					pTos->nIdx = SXU32_HIGH;
					break;
				}
			}
			{
				/*
				 * php 8 has no bare-word fallback: an unresolved constant is a catchable
				 * Error, not its own name as a string. PH7 answered "X" for an unknown
				 * X, so a typo — or a constant php REMOVED, like ASSERT_QUIET_EVAL —
				 * silently became a string and flowed on. php names the name it looked
				 * for FIRST, so the message reports the candidate when there was one
				 * ("Undefined constant \"B\NOPE\"" inside `namespace B;`).
				 *
				 * Routed through PH7_THROW_ROUTE_MIDEXPR: OP_LOADC is not a call
				 * boundary, so neither a bare `break` nor `goto Exception` is correct
				 * here (see the macro).
				 */
				SyBlob sMsg;
				SyBlobInit(&sMsg,&pVm->sAllocator);
				if( zCand ){
					SyBlobFormat(&sMsg,"Undefined constant \"%s\"",zCand);
				}else{
					SyBlobFormat(&sMsg,"Undefined constant \"%.*s\"",nLit,zLit);
				}
				MemObjSetType(pTos,MEMOBJ_NULL);
				SyBlobReset(&pTos->sBlob);
				pTos->nIdx = SXU32_HIGH;
				rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),
					SyBlobLength(&sMsg));
				SyBlobRelease(&sMsg);
				if( rc == SXERR_ABORT ){
					goto Abort;
				}
				PH7_THROW_ROUTE_MIDEXPR(rc)
			}
		}
		PH7_MemObjLoad(pObj,pTos);
	}else{
		/* Set a NULL value */
		MemObjSetType(pTos,MEMOBJ_NULL);
	}
	/* Mark as constant */
	pTos->nIdx = SXU32_HIGH;
	break;
				  }
/*
 * LOAD: P1 * P3
 *
 * Load a variable where it's name is taken from the top of the stack or
 * from the P3 operand.
 * If P1 is set,then perform a lookup only.In other words do not create
 * the variable if non existent and push the NULL constant instead.
 */
case PH7_OP_LOAD:{
	ph7_value *pObj;
	SyString sName;
	if( pInstr->p3 == 0 ){
		/* Take the variable name from the top of the stack */
#ifdef UNTRUST
		if( pTos < pStack ){
			goto Abort;
		}
#endif
		/* Force a string cast — variable-variable name $$arr (user-visible, §2) */
		PH7_MemObjToStringUV(pTos);
		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
	}else{
		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));
		/* Reserve a room for the target object */
		pTos++;
	}
	if( pInstr->iP2 == 2 ){
		/* Read-modify-write target (`$x++`, `$x .= 'a'`): php reads the variable
		 * before writing, so it warns when it does not exist and THEN seeds it.
		 * Peek first (no create) purely to raise that warning; the load below
		 * still creates the slot the operator needs. A plain `=` never gets here
		 * — it writes without reading, and stays silent, as php does. */
		if( VmExtractMemObj(&(*pVm),&sName,FALSE,FALSE) == 0 ){
			VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);
		}
	}
	/* Extract the requested memory object */
	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,pInstr->iP1 != 1);
	if( pObj == 0 ){
		if( pInstr->iP1 ){
			/* Reading a variable that does not exist. php raises E_WARNING
			 * "Undefined variable $x" and evaluates it as NULL; PHL used to
			 * yield NULL silently, which hid typo'd names. iP2 marks the reads
			 * that must stay quiet (isset/empty — see PH7_CompileVariable);
			 * vivifying contexts never get here because they pass iP1 = 0. */
			if( pInstr->iP2 == 0 ){
				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined variable $%z",&sName);
			}
			/* Variable not found,load NULL */
			if( !pInstr->p3 ){
				PH7_MemObjRelease(pTos);
			}else{
				MemObjSetType(pTos,MEMOBJ_NULL);
			}
			pTos->nIdx = SXU32_HIGH; /* Mark as constant */
			if( pInstr->iP2 == 3 ){
				/* D1 deferred call argument: the variable does not exist, but we do not
				 * know yet whether the callee wants it by-ref (materialize + bind) or
				 * by-value (warn + pass NULL). Tag the slot and stash the variable name
				 * (a VM-lifetime bytecode string, nothing to free) so OP_CALL's
				 * VmResolveDeferredArgs can decide once the callee is resolved. */
				pTos->iFlags |= MEMOBJ_AUX_DEFERRED;
				pTos->x.pOther = pInstr->p3;
			}
			break;
		}else{
			/* Fatal error */
			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);
			goto Abort;
		}
	}
	/* Load variable contents */
	PH7_MemObjLoad(pObj,pTos);
	pTos->nIdx = pObj->nIdx;
	break;
				   }
/*
 * LOAD_MAP P1 * *
 *
 * Allocate a new empty hashmap (array in the PHP jargon) and push it on the stack.
 * If the P1 operand is greater than zero then pop P1 elements from the
 * stack and insert them (key => value pair) in the new hashmap.
 */
case PH7_OP_LOAD_MAP: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpLoadMap(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * LOAD_LIST: P1 * *
 *
 * Assign hashmap entries values to the top P1 entries.
 * This is the VM implementation of the list() PHP construct.
 * Caveats:
 *  This implementation support only a single nesting level.
 */
case PH7_OP_LOAD_LIST: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpLoadList(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * LOAD_IDX: P1 P2 *
 *
 * Load a hasmap entry where it's index (either numeric or string) is taken
 * from the stack.
 * If the index does not refer to a valid element,then push the NULL constant
 * instead.
 */
case PH7_OP_LOAD_IDX: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpLoadIdx(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * LOAD_CLOSURE * * P3
 *
 * Set-up closure environment described by the P3 oeprand and push the closure
 * name in the stack.
 */
case PH7_OP_LOAD_CLOSURE: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpLoadClosure(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * LOAD_FCC P1 * *
 *
 * First-class callable: wrap the callee in a Closure object instead of calling it.
 *  P1 == 1: a plain function/host callable — its NAME string is already on the TOS
 *           (from the callee's OP_LOADC). Replace it in place with a Closure whose
 *           $__fn is that name; the existing string-callable dispatch resolves it.
 *           (OOM degrades to leaving the name string on the stack — still callable.)
 *  P1 == 2: a method/static callee — the stack is [ target (pTos[-1]), real-method-name
 *           (pTos) ] (the OP_MEMBER was popped at compile time). An object target binds
 *           $this (scope = its class); a class-name-string target is a static callable
 *           (scope = that class, self/parent/static resolved now). (OOM degrades to NULL —
 *           the popped target leaves no name string to keep.)
 */
case PH7_OP_LOAD_FCC:{
	if( pInstr->iP1 == 1 ){
		/* Plain-callee FCC: TOS holds the callee value. An existing Closure (`$closure(...)`)
		 * is idempotent (left unchanged, matching PHP). Any other validated callable VALUE —
		 * a function-NAME string (the common case, from the callee's OP_LOADC), a [target,
		 * method] array callable, or an __invoke object via `($expr)(...)` — is normalized to a
		 * fresh Closure (PHP always yields a Closure). A non-callable value is left as-is
		 * (graceful degradation), and so is the original on OOM — still whatever it was. */
		ph7_class_instance *pCloObj;
		if( VmValueIsClosure(pVm, pTos) ){
			break;
		}
		pCloObj = VmFccWrapValue(pVm, pTos);
		if( pCloObj ){
			PH7_MemObjRelease(pTos);
			pCloObj->iRef++;
			pTos->x.pOther = pCloObj;
			MemObjSetType(pTos, MEMOBJ_OBJ);
		}
	}else{
		/* iP1 == 2: method/static. Stack is [ target (pTos[-1]), real-method-name (pTos) ]
		 * left standing by the popped OP_MEMBER. An object target binds $this (scope = its
		 * class); a class-name string target is a static callable (scope = that class). */
		ph7_value *pTarget = &pTos[-1];
		SyString sName;
		ph7_class_instance *pCloObj;
		SyStringInitFromBuf(&sName, SyBlobData(&pTos->sBlob), SyBlobLength(&pTos->sBlob));
		if( pTarget->iFlags & MEMOBJ_OBJ ){
			ph7_class_instance *pBoundThis = (ph7_class_instance *)pTarget->x.pOther;
			pCloObj = VmCreateClosure(pVm, &sName, pBoundThis, &pBoundThis->pClass->sName);
		}else if( pTarget->iFlags & MEMOBJ_STRING ){
			/* Static `T::m(...)`: resolve T (incl. self/static/parent) to the real class
			 * now, so the closure binds the concrete scope (matching PHP). */
			ph7_class *pScopeCls = VmFccResolveScope(pVm, pTarget);
			pCloObj = pScopeCls ? VmCreateClosure(pVm, &sName, 0, &pScopeCls->sName) : 0;
		}else{
			pCloObj = 0;
		}
		/* Pop the method name and the target, push the Closure. */
		PH7_MemObjRelease(pTos);
		pTos--;
		PH7_MemObjRelease(pTos);
		if( pCloObj ){
			pCloObj->iRef++;
			pTos->x.pOther = pCloObj;
			MemObjSetType(pTos, MEMOBJ_OBJ);
		}else{
			pTos->nIdx = SXU32_HIGH; /* OOM: NULL */
		}
	}
	break;
					 }
/*
 * STORE * P2 P3
 *
 * Perform a store (Assignment) operation.
 */
case PH7_OP_STORE: {
	ph7_value *pObj;
	SyString sName;
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if( pInstr->iP2 ){
		sxu32 nIdx;
		sxi32 rcT;
		/* Member store operation */
		nIdx = pTos->nIdx;
		VmPopOperand(&pTos,1);
		if( pVm->pMagicSetThis ){
			/* Pending __set (band A #3b): the preceding OP_MEMBER detected a plain
			 * store to a missing/inaccessible property on a class declaring __set.
			 * Dispatch __set($name, $value) with the rvalue (pTos), which stays on
			 * the stack as the assignment expression's result — php's semantics
			 * (no property is created; a throw rides the boundary rail). */
			ph7_class_instance *pSetThis = pVm->pMagicSetThis;
			SyString sSetName;
			pVm->pMagicSetThis = 0;
			SyStringInitFromBuf(&sSetName,SyBlobData(&pVm->sMagicSetName),SyBlobLength(&pVm->sMagicSetName));
			VmMagicSetDispatch(&(*pVm),pSetThis,&sSetName,pTos);
			PH7_ClassInstanceUnref(pSetThis);
			SyBlobReset(&pVm->sMagicSetName);
			break;
		}
		if( pVm->pHookSetThis ){
			/* Pending property-hook set (PHP 8.4): the preceding OP_MEMBER found a
			 * plain store to a hooked property. Dispatch __phl_hook_set_NAME with
			 * the rvalue (which stays on the stack as the assignment expression's
			 * result); a `set => expr` hook's return value is stored into the
			 * BACKING slot with the ordinary typed enforcement. A property with
			 * only a get hook is php's catchable "is read-only" Error. */
			ph7_class_instance *pHThis = pVm->pHookSetThis;
			ph7_class_attr *pHAttr = pVm->pHookSetAttr;
			sxu32 nBackIdx = pVm->nHookSetIdx;
			sxi32 rcHs;
			pVm->pHookSetThis = 0;
			pVm->pHookSetAttr = 0;
			pVm->nHookSetIdx = SXU32_HIGH;
			rcHs = VmHookSetDispatch(&(*pVm),pHThis,pHAttr,nBackIdx,pTos);
			PH7_ClassInstanceUnref(pHThis);
			if( rcHs == PH7_ABORT ){
				goto Abort;
			}
			break;
		}
		if( nIdx == SXU32_HIGH ){
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
				"Cannot perform assignment on a constant class attribute,PH7 is loading NULL");
			pTos->nIdx = SXU32_HIGH;
		}else{
			/* Enforce typed property declaration if any. May coerce the
			 * incoming value in place (weak mode) or throw TypeError. */
			rcT = VmEnforcePropertyTypeOnStore(&(*pVm),nIdx,pTos,0);
			if( rcT == PH7_ABORT ){
				goto Abort;
			}
			if( rcT == PH7_EXCEPTION ){
				/* TypeError was thrown. Pop the rejected rvalue and hand
				 * control to the nearest catch block if any (draining any
				 * abandoned outer-expression operands to the try's base),
				 * otherwise propagate out of the VM loop. */
				VmPopOperand(&pTos,1);
				{
					sxi32 iRp;
					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){
						PH7_RESUME_DRAIN()
						pc = iRp;
						break;
					}
				}
				goto Exception;
			}
			/* Point to the desired memory object */
			pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx);
			if( pObj ){
				/* Perform the store operation */
				PH7_MemObjStore(pTos,pObj);
			}
		}
		break;
	}else if( pInstr->p3 == 0 ){
		/* Take the variable name from the next on the stack (user-visible: a
		 * variable-variable NAME $$arr warns on an array, §2) */
		PH7_MemObjToStringUV(pTos);
		SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
		pTos--;
#ifdef UNTRUST
		if( pTos < pStack  ){
			goto Abort;
		}
#endif
	}else{
		SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));
	}
	if( sName.nByte == sizeof("GLOBALS")-1
	 && SyMemcmp((const void *)sName.zString,(const void *)"GLOBALS",sName.nByte) == 0 ){
		if( pInstr->p3 ){
			/* php 8.1 forbids re-assigning the literal $GLOBALS (compile-time
			 * fatal there; raised at the store site here with the same
			 * message and the same non-catchable outcome). Element writes
			 * are unaffected. */
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
				"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");
			pVm->iExitStatus = 255;
			pVm->bHaltRequested = 1;
			goto Abort;
		}
		/* A DYNAMIC-name write (${'GLOBALS'} = v, $$n = v) is not php's
		 * compile-time case: php quietly creates an ordinary symbol-table
		 * entry named GLOBALS, leaving the auto-global view intact. */
		PH7_VmInstallGlobalVar(&(*pVm),"GLOBALS",sizeof("GLOBALS")-1,pTos,SXU32_HIGH);
		PH7_MemObjRelease(&pTos[1]);
		break;
	}
	/* Extract the desired variable and if not available dynamically create it */
	pObj = VmExtractMemObj(&(*pVm),&sName,pInstr->p3 ? FALSE : TRUE,TRUE);
	if( pObj == 0 ){
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,
			"Fatal, PH7 engine is running out of memory while loading variable '%z'",&sName);
		goto Abort;
	}
	if( !pInstr->p3 ){
		PH7_MemObjRelease(&pTos[1]);
	}
	/* Perform the store operation */
	PH7_MemObjStore(pTos,pObj);
	break;
				   }
/*
 * STORE_IDX:   P1 * P3
 * STORE_IDX_R: P1 * P3
 *
 * Perfrom a store operation an a hashmap entry.
 */
case PH7_OP_STORE_IDX:
case PH7_OP_STORE_IDX_REF: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpStoreIdxRef(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * INCR: P1 * *
 *
 * Force a numeric cast and increment the top of the stack by 1.
 * If the P1 operand is set then perform a duplication of the top of
 * the stack and increment after that.
 */
case PH7_OP_INCR:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* `++` on a readonly property is forbidden regardless of the current value's
	 * type (it bypasses the store path), so enforce before the type guard below
	 * — which otherwise skips object/array/resource operands. */
	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);
	/* A hooked property whose get hook returned an array/object/resource:
	 * php's TypeError, raised BEFORE any set dispatch (the type guard below
	 * would skip the mutation and the tail write-back would otherwise call
	 * the set hook with the unchanged value). */
	if( (pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES)) != 0
	 && SySetUsed(&pVm->aHookRmw) > 0 ){
		VmHookRmw *pTopInc = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);
		if( pTopInc->iKind == VM_HOOK_PEND_RMW && pTopInc->nScratchIdx == pTos->nIdx ){
			SyBlob sErrMsg;
			SyBlobInit(&sErrMsg,&pVm->sAllocator);
			if( pTos->iFlags & MEMOBJ_HASHMAP ){
				SyBlobAppend(&sErrMsg,"Cannot increment array",sizeof("Cannot increment array")-1);
			}else if( pTos->iFlags & MEMOBJ_OBJ ){
				SyBlobFormat(&sErrMsg,"Cannot increment %z",
					&((ph7_class_instance *)pTos->x.pOther)->pClass->sName);
			}else{
				SyBlobAppend(&sErrMsg,"Cannot increment resource",sizeof("Cannot increment resource")-1);
			}
			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));
			VmHookRmwDropTop(&(*pVm));
			pTos->nIdx = SXU32_HIGH;
			break;
		}
	}
	if( (pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES)) == 0 ){
		if( pTos->nIdx != SXU32_HIGH ){
			ph7_value *pObj;
			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
				if( VmStringWantsPerlIncr(pObj) ){
					/* php 8.3 only DEPRECATES the Perl-style increment of a non-numeric
					 * string (it points at str_increment() instead); PHL rejects it. */
					SyBlob sErrMsg;
					SyBlobInit(&sErrMsg,&pVm->sAllocator);
					SyBlobAppend(&sErrMsg,
						"Increment on a non-numeric string is not supported, use str_increment() instead",
						sizeof("Increment on a non-numeric string is not supported, use str_increment() instead")-1);
					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));
					VmHookRmwDropTop(&(*pVm));
					pTos->nIdx = SXU32_HIGH;
					break;
				}else{
					/* Numeric coercion. Post-increment must preserve pTos's
					 * original value: pTos may alias pObj's blob via
					 * SXBLOB_RDONLY (set by PH7_MemObjLoad), and
					 * PH7_MemObjToNumeric calls SyBlobRelease on a STRING
					 * pObj. Force pTos to take ownership of its blob first
					 * so its old-value view survives the coercion. */
					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){
						SyBlobNullAppend(&pTos->sBlob);
					}
					/* Force a numeric cast on the variable */
					PH7_MemObjToNumeric(pObj);
					if( pObj->iFlags & MEMOBJ_REAL ){
						pObj->rVal++;
						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it
						 * stays consistent with the new rVal; otherwise (int)$a,
						 * ===, intdiv() etc. read a stale int for an
						 * integer-valued real. */
						PH7_MemObjTryInteger(pObj);
					}else{
						/* PHP promotes PHP_INT_MAX++ to float; the integer-only
						 * build wraps like OP_POW's OMIT path. */
						sxi64 r;
						if( PH7_ADD_OVERFLOW64(pObj->x.iVal,1,&r) ){
#ifndef PH7_OMIT_FLOATING_POINT
							pObj->rVal = (ph7_real)pObj->x.iVal + 1.0;
							MemObjSetType(pObj,MEMOBJ_REAL);
#else
							pObj->x.iVal = r;
#endif
						}else{
							pObj->x.iVal = r;
						}
					}
					if( pInstr->iP1 ){
						/* Pre-increment: result is the new value. */
						PH7_MemObjStore(pObj,pTos);
					}
					/* Post-increment: pTos retains the old value (a string
					 * for "5"++, an int/float for direct numeric operands). */
				}
			}
		}else{
			if( pInstr->iP1 ){
				if( VmStringWantsPerlIncr(pTos) ){
					PH7_MemObjStringIncrement(pTos);
				}else{
					/* Force a numeric cast */
					PH7_MemObjToNumeric(pTos);
					/* Pre-increment */
					if( pTos->iFlags & MEMOBJ_REAL ){
						pTos->rVal++;
						/* Try to get an integer representation */
						PH7_MemObjTryInteger(pTos);
					}else{
						/* PHP promotes PHP_INT_MAX++ to float; the integer-only
						 * build wraps like OP_POW's OMIT path. */
						sxi64 r;
						if( PH7_ADD_OVERFLOW64(pTos->x.iVal,1,&r) ){
#ifndef PH7_OMIT_FLOATING_POINT
							pTos->rVal = (ph7_real)pTos->x.iVal + 1.0;
							MemObjSetType(pTos,MEMOBJ_REAL);
#else
							pTos->x.iVal = r;
							MemObjSetType(pTos,MEMOBJ_INT);
#endif
						}else{
							pTos->x.iVal = r;
							MemObjSetType(pTos,MEMOBJ_INT);
						}
					}
				}
			}
		}
	}
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);
	break;
/*
 * DECR: P1 * *
 *
 * Force a numeric cast and decrement the top of the stack by 1.
 * If the P1 operand is set then perform a duplication of the top of the stack
 * and decrement after that.
 */
case PH7_OP_DECR:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* `--` on a readonly property is forbidden regardless of the current value's
	 * type (it bypasses the store path), so enforce before the type guard below
	 * — which otherwise skips null/object/array/resource operands (e.g. a readonly
	 * property currently holding null). */
	PH7_ENFORCE_READONLY_MUTATE(pTos->nIdx);
	/* A hooked property whose get hook returned an array/object/resource:
	 * php's TypeError, raised BEFORE any set dispatch (null stays excluded —
	 * `--` on null is php's no-op and its write-back still dispatches set). */
	if( (pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES)) != 0
	 && SySetUsed(&pVm->aHookRmw) > 0 ){
		VmHookRmw *pTopDec = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);
		if( pTopDec->iKind == VM_HOOK_PEND_RMW && pTopDec->nScratchIdx == pTos->nIdx ){
			SyBlob sErrMsg;
			SyBlobInit(&sErrMsg,&pVm->sAllocator);
			if( pTos->iFlags & MEMOBJ_HASHMAP ){
				SyBlobAppend(&sErrMsg,"Cannot decrement array",sizeof("Cannot decrement array")-1);
			}else if( pTos->iFlags & MEMOBJ_OBJ ){
				SyBlobFormat(&sErrMsg,"Cannot decrement %z",
					&((ph7_class_instance *)pTos->x.pOther)->pClass->sName);
			}else{
				SyBlobAppend(&sErrMsg,"Cannot decrement resource",sizeof("Cannot decrement resource")-1);
			}
			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));
			VmHookRmwDropTop(&(*pVm));
			pTos->nIdx = SXU32_HIGH;
			break;
		}
	}
	/* NULL stays excluded: PHP leaves `--` on null untouched (no-op) -- but 8.3
	 * deprecates that no-op, same as the non-numeric-string one below. */
	if( pTos->iFlags & MEMOBJ_NULL ){
		/* E_WARNING, not E_DEPRECATED -- php reports this one at errno 2. */
		VmErrorFormat(&(*pVm),PH7_CTX_WARNING,
			"Decrement on type null has no effect, this will change in the next major version of PHP");
	}
	if( (pTos->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES|MEMOBJ_NULL)) == 0 ){
		if( pTos->nIdx != SXU32_HIGH ){
			ph7_value *pObj;
			if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
				if( VmStringWantsPerlIncr(pObj) ){
					/* php 8.3 only DEPRECATES the no-op `--` of a non-numeric string
					 * (php has no string decrement); PHL rejects it. */
					SyBlob sErrMsg;
					SyBlobInit(&sErrMsg,&pVm->sAllocator);
					SyBlobAppend(&sErrMsg,
						"Decrement on a non-numeric string is not supported",
						sizeof("Decrement on a non-numeric string is not supported")-1);
					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sErrMsg));
					VmHookRmwDropTop(&(*pVm));
					pTos->nIdx = SXU32_HIGH;
					break;
				}else{
					/* Numeric coercion. Mirror INCR's aliasing care: a
					 * post-decrement must preserve pTos's original value, which
					 * may alias pObj's blob via SXBLOB_RDONLY (PH7_MemObjLoad).
					 * Force pTos to own its blob before coercing pObj. */
					if( pInstr->iP1 == 0 && (pTos->iFlags & MEMOBJ_STRING) ){
						SyBlobNullAppend(&pTos->sBlob);
					}
					PH7_MemObjToNumeric(pObj);
					if( pObj->iFlags & MEMOBJ_REAL ){
						pObj->rVal--;
						/* Refresh the cached integer (x.iVal/MEMOBJ_INT) so it
						 * stays consistent with the new rVal; otherwise (int)$a,
						 * ===, intdiv() etc. read a stale int for an
						 * integer-valued real. */
						PH7_MemObjTryInteger(pObj);
					}else{
						/* PHP promotes PHP_INT_MIN-- to float; the integer-only
						 * build wraps like OP_POW's OMIT path. */
						sxi64 r;
						if( PH7_SUB_OVERFLOW64(pObj->x.iVal,1,&r) ){
#ifndef PH7_OMIT_FLOATING_POINT
							pObj->rVal = (ph7_real)pObj->x.iVal - 1.0;
							MemObjSetType(pObj,MEMOBJ_REAL);
#else
							pObj->x.iVal = r;
#endif
						}else{
							pObj->x.iVal = r;
						}
					}
					if( pInstr->iP1 ){
						/* Pre-decrement: result is the new value. */
						PH7_MemObjStore(pObj,pTos);
					}
					/* Post-decrement: pTos retains the old value. */
				}
			}
		}else{
			if( pInstr->iP1 ){
				if( VmStringWantsPerlIncr(pTos) ){
					/* Non-numeric string, no lvalue: no-op (value unchanged). */
				}else{
					/* Force a numeric cast */
					PH7_MemObjToNumeric(pTos);
					/* Pre-decrement */
					if( pTos->iFlags & MEMOBJ_REAL ){
						pTos->rVal--;
						/* Keep the cached int consistent with the new rVal. */
						PH7_MemObjTryInteger(pTos);
					}else{
						/* PHP promotes PHP_INT_MIN-- to float; the integer-only
						 * build wraps like OP_POW's OMIT path. */
						sxi64 r;
						if( PH7_SUB_OVERFLOW64(pTos->x.iVal,1,&r) ){
#ifndef PH7_OMIT_FLOATING_POINT
							pTos->rVal = (ph7_real)pTos->x.iVal - 1.0;
							MemObjSetType(pTos,MEMOBJ_REAL);
#else
							pTos->x.iVal = r;
							MemObjSetType(pTos,MEMOBJ_INT);
#endif
						}else{
							pTos->x.iVal = r;
							MemObjSetType(pTos,MEMOBJ_INT);
						}
					}
				}
			}
		}
	}
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,pTos);
	break;
/*
 * UMINUS: * * *
 *
 * Perform a unary minus operation.
 */
case PH7_OP_UMINUS:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Force a numeric (integer,real or both) cast */
	PH7_MemObjToNumeric(pTos);
	if( pTos->iFlags & MEMOBJ_REAL ){
		pTos->rVal = -pTos->rVal;
	}
	if( pTos->iFlags & MEMOBJ_INT ){
		if( pTos->x.iVal == SMALLEST_INT64 ){
			/* -PHP_INT_MIN overflows sxi64; PHP promotes it to float. When a
			 * REAL representation is already present it is the negated
			 * authoritative value, so just drop the now-stale cached int. The
			 * integer-only build has no float type, so it wraps (two's
			 * complement -INT_MIN == INT_MIN) like OP_POW's OMIT path. */
#ifndef PH7_OMIT_FLOATING_POINT
			if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){
				pTos->rVal = -(ph7_real)pTos->x.iVal;
				MemObjSetType(pTos,MEMOBJ_REAL);
			}else{
				pTos->iFlags &= ~MEMOBJ_INT;
			}
#else
			pTos->x.iVal = (sxi64)(0 - (sxu64)pTos->x.iVal);
#endif
		}else{
			pTos->x.iVal = -pTos->x.iVal;
		}
	}
	break;
/*
 * UPLUS: * * *
 *
 * Perform a unary plus operation.
 */
case PH7_OP_UPLUS:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Force a numeric (integer,real or both) cast */
	PH7_MemObjToNumeric(pTos);
	if( pTos->iFlags & MEMOBJ_REAL ){
		pTos->rVal = +pTos->rVal;
	}
	if( pTos->iFlags & MEMOBJ_INT ){
		pTos->x.iVal = +pTos->x.iVal;
	}
	break;
/*
 * OP_LNOT: * * *
 *
 * Interpret the top of the stack as a boolean value.  Replace it
 * with its complement.
 */
case PH7_OP_LNOT:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Force a boolean cast */
	if( (pTos->iFlags & MEMOBJ_BOOL) == 0 ){
		PH7_MemObjToBool(pTos);
	}
	pTos->x.iVal = !pTos->x.iVal;
	break;
/*
 * OP_BITNOT: * * *
 *
 * Interpret the top of the stack as an value.Replace it
 * with its ones-complement.
 */
case PH7_OP_BITNOT:
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Force an integer cast (php deprecates a lossy float here too) */
	rc = VmRejectFloatOperand(&(*pVm),pTos);
	PH7_DISPATCH_ENFORCE_RC(rc)
	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pTos);
	}
	pTos->x.iVal = ~pTos->x.iVal;
	break;
/* OP_MUL * * *
 * OP_MUL_STORE * * *
 *
 * Pop the top two elements from the stack, multiply them together,
 * and push the result back onto the stack.
 */
case PH7_OP_MUL:
case PH7_OP_MUL_STORE: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpMulStore(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/* OP_POW * * *
 * OP_POW_STORE * * *
 *
 * Pop the top two elements from the stack, raise the second to the
 * power of the first, and push the result. PHP semantics: int**int
 * stays integer iff the exponent is non-negative and the exact result
 * fits in sxi64; otherwise the result is a double.
 */
case PH7_OP_POW:
case PH7_OP_POW_STORE: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpPowStore(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/* OP_ADD * * *
 *
 * Pop the top two elements from the stack, add them together,
 * and push the result back onto the stack.
 */
case PH7_OP_ADD:{
	ph7_value *pNos = &pTos[-1];
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	{
		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,
		 * object or resource operand is a TypeError, not a silent 0. Settle the stack
		 * BEFORE throwing, so the catch does not run over the abandoned operands. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"+",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ goto Abort; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
	/* Perform the addition */
	PH7_MemObjAdd(pNos,pTos,FALSE);
	VmPopOperand(&pTos,1);
	break;
				}
/*
 * OP_ADD_STORE * * *
 *
 * Pop the top two elements from the stack, add them together,
 * and push the result back onto the stack.
 */
case PH7_OP_ADD_STORE:{
	ph7_value *pNos = &pTos[-1];
	ph7_value *pObj;
	sxu32 nIdx;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	{
		/* php's operand contract: a compound-assign with a non-numeric string,
		 * array, object or resource operand is a TypeError too. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"+",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ goto Abort; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
	/* Perform the addition */
	nIdx = pTos->nIdx;
	if( nIdx == pVm->nGlobalIdx ){
		/* php 8.1: $GLOBALS += [...] is forbidden like any re-assignment
		 * (a compile-time fatal in php; raised here, same message). */
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
			"$GLOBALS can only be modified using the $GLOBALS[$name] = $value syntax");
		pVm->iExitStatus = 255;
		pVm->bHaltRequested = 1;
		goto Abort;
	}
	PH7_MemObjAdd(pTos,pNos,TRUE);
	/* Peform the store operation */
	if( nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){
		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);
		PH7_MemObjStore(pTos,pObj);
	}
	PH7_HOOK_RMW_WRITEBACK(nIdx,0);
	/* Ticket 1433-35: Perform a stack dup */
	PH7_MemObjStore(pTos,pNos);
	VmPopOperand(&pTos,1);
	break;
				}
/* OP_SUB * * *
 *
 * Pop the top two elements from the stack, subtract the
 * first (what was next on the stack) from the second (the
 * top of the stack) and push the result back onto the stack.
 */
case PH7_OP_SUB: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpSub(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/* OP_SUB_STORE * * *
 *
 * Pop the top two elements from the stack, subtract the
 * first (what was next on the stack) from the second (the
 * top of the stack) and push the result back onto the stack.
 */
case PH7_OP_SUB_STORE: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpSubStore(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }

/*
 * OP_MOD * * *
 *
 * Pop the top two elements from the stack, divide the
 * first (what was next on the stack) from the second (the
 * top of the stack) and push the remainder after division
 * onto the stack.
 * Note: Only integer arithemtic is allowed.
 */
case PH7_OP_MOD: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpMod(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * OP_MOD_STORE * * *
 *
 * Pop the top two elements from the stack, divide the
 * first (what was next on the stack) from the second (the
 * top of the stack) and push the remainder after division
 * onto the stack.
 * Note: Only integer arithemtic is allowed.
 */
case PH7_OP_MOD_STORE: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpModStore(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * OP_DIV * * *
 *
 * Pop the top two elements from the stack, divide the
 * first (what was next on the stack) from the second (the
 * top of the stack) and push the result onto the stack.
 * Note: Only floating point arithemtic is allowed.
 */
case PH7_OP_DIV: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpDiv(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * OP_DIV_STORE * * *
 *
 * Pop the top two elements from the stack, divide the
 * first (what was next on the stack) from the second (the
 * top of the stack) and push the result onto the stack.
 * Note: Only floating point arithemtic is allowed.
 */
case PH7_OP_DIV_STORE:{
	ph7_value *pNos = &pTos[-1];
	ph7_value *pObj;
	ph7_real a,b,r;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	{
		/* php's operand contract: a compound-assign with a non-numeric string,
		 * array, object or resource operand is a TypeError too. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"/",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ goto Abort; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
	/* Force the operands to be real */
	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){
		PH7_MemObjToReal(pTos);
	}
	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){
		PH7_MemObjToReal(pNos);
	}
	/* Perform the requested operation */
	a = pTos->rVal;
	b = pNos->rVal;
	if( b == 0 ){
		/* Division by zero: php throws a catchable DivisionByZeroError (8.0),
		 * not the old non-catchable warning that continued with a 0 result. */
		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");
		PH7_DISPATCH_ENFORCE_RC(rc)
	}else{
		r = a/b;
		/* Push the result */
		pNos->rVal = r;
		MemObjSetType(pNos,MEMOBJ_REAL);
		/* Try to get an integer representation */
		PH7_MemObjTryInteger(pNos);
	}
	if( pTos->nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);
		PH7_MemObjStore(pNos,pObj);
	}
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);
	VmPopOperand(&pTos,1);
	break;
				}
/* OP_BAND * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the bit-wise AND of the
 * two elements.
*/
/* OP_BOR * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the bit-wise OR of the
 * two elements.
 */
/* OP_BXOR * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the bit-wise XOR of the
 * two elements.
 */
case PH7_OP_BAND:
case PH7_OP_BOR:
case PH7_OP_BXOR:{
	ph7_value *pNos = &pTos[-1];
	sxi64 a,b,r;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Force the operands to be integer (php deprecates a lossy float here) */
	rc = VmRejectFloatOperand(&(*pVm),pNos);
	PH7_DISPATCH_ENFORCE_RC(rc)
	rc = VmRejectFloatOperand(&(*pVm),pTos);
	PH7_DISPATCH_ENFORCE_RC(rc)
	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pTos);
	}
	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pNos);
	}
	/* Perform the requested operation */
	a = pNos->x.iVal;
	b = pTos->x.iVal;
	switch(pInstr->iOp){
	case PH7_OP_BOR_STORE:
	case PH7_OP_BOR:  r = a|b; break;
	case PH7_OP_BXOR_STORE:
	case PH7_OP_BXOR: r = a^b; break;
	case PH7_OP_BAND_STORE:
	case PH7_OP_BAND:
	default:          r = a&b; break;
	}
	/* Push the result */
	pNos->x.iVal = r;
	MemObjSetType(pNos,MEMOBJ_INT);
	VmPopOperand(&pTos,1);
	break;
				 }
/* OP_BAND_STORE * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the bit-wise AND of the
 * two elements.
*/
/* OP_BOR_STORE * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the bit-wise OR of the
 * two elements.
 */
/* OP_BXOR_STORE * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the bit-wise XOR of the
 * two elements.
 */
case PH7_OP_BAND_STORE:
case PH7_OP_BOR_STORE:
case PH7_OP_BXOR_STORE:{
	ph7_value *pNos = &pTos[-1];
	ph7_value *pObj;
	sxi64 a,b,r;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Force the operands to be integer (php deprecates a lossy float here) */
	rc = VmRejectFloatOperand(&(*pVm),pNos);
	PH7_DISPATCH_ENFORCE_RC(rc)
	rc = VmRejectFloatOperand(&(*pVm),pTos);
	PH7_DISPATCH_ENFORCE_RC(rc)
	if( (pTos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pTos);
	}
	if( (pNos->iFlags & MEMOBJ_INT) == 0 ){
		PH7_MemObjToInteger(pNos);
	}
	/* Perform the requested operation */
	a = pTos->x.iVal;
	b = pNos->x.iVal;
	switch(pInstr->iOp){
	case PH7_OP_BOR_STORE:
	case PH7_OP_BOR:  r = a|b; break;
	case PH7_OP_BXOR_STORE:
	case PH7_OP_BXOR: r = a^b; break;
	case PH7_OP_BAND_STORE:
	case PH7_OP_BAND:
	default:          r = a&b; break;
	}
	/* Push the result */
	pNos->x.iVal = r;
	MemObjSetType(pNos,MEMOBJ_INT);
	if( pTos->nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);
		PH7_MemObjStore(pNos,pObj);
	}
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);
	VmPopOperand(&pTos,1);
	break;
				 }
/* OP_SHL * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the second element shifted
 * left by N bits where N is the top element on the stack.
 * Note: Only integer arithmetic is allowed.
 */
/* OP_SHR * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the second element shifted
 * right by N bits where N is the top element on the stack.
 * Note: Only integer arithmetic is allowed.
 */
case PH7_OP_SHL:
case PH7_OP_SHR: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpShr(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*  OP_SHL_STORE * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the second element shifted
 * left by N bits where N is the top element on the stack.
 * Note: Only integer arithmetic is allowed.
 */
/* OP_SHR_STORE * * *
 *
 * Pop the top two elements from the stack.  Convert both elements
 * to integers.  Push back onto the stack the second element shifted
 * right by N bits where N is the top element on the stack.
 * Note: Only integer arithmetic is allowed.
 */
case PH7_OP_SHL_STORE:
case PH7_OP_SHR_STORE: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpShrStore(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/* CAT:  P1 * *
 *
 * Pop P1 elements from the stack. Concatenate them togeher and push the result
 * back.
 */
case PH7_OP_CAT:{
	ph7_value *pNos,*pCur;
	if( pInstr->iP1 < 1 ){
		pNos = &pTos[-1];
	}else{
		pNos = &pTos[-pInstr->iP1+1];
	}
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Force a string cast (user-visible: warns on an array operand, §2) */
	PH7_MemObjToStringUV(pNos);
	pCur = &pNos[1];
	while( pCur <= pTos ){
		PH7_MemObjToStringUV(pCur);
		/* Perform the concatenation */
		if( SyBlobLength(&pCur->sBlob) > 0 ){
			if( PH7_MemObjStringAppend(pNos,(const char *)SyBlobData(&pCur->sBlob),SyBlobLength(&pCur->sBlob)) != SXRET_OK ){
				/* Allocation failure: raise a fatal instead of a truncated concat */
				PH7_VmMemoryError(&(*pVm));
				goto Abort;
			}
		}
		SyBlobRelease(&pCur->sBlob);
		pCur++;
	}
	pTos = pNos;
	break;
				}
/*  CAT_STORE: * * *
 *
 * Pop two elements from the stack. Concatenate them togeher and push the result
 * back.
 */
case PH7_OP_CAT_STORE:{
	ph7_value *pNos = &pTos[-1];
	ph7_value *pObj;
	sxu32 nIdx;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* The right operand must be a string to append it (user-visible, §2) */
	PH7_MemObjToStringUV(pNos);
	nIdx = pTos->nIdx;
	/* Fast path: append straight into the lvalue's own (geometrically grown) buffer
	 * instead of copy-on-write-dup'ing the read-only-aliased stack value and then
	 * storing the whole buffer back twice. This turns `$s .= ...` (and the
	 * $a[$i] .= / $obj->prop .= forms) from O(n^2) into amortized O(1).
	 * Guards: a real owned slot; the right operand must NOT alias that same slot
	 * (`$s .= $s`, or a reference to it, would realloc the buffer out from under
	 * the source we copy from — references share the slot index, so one check
	 * covers both); and not a typed property, whose store-time type check/coercion
	 * must run before any mutation (left to the slow path).
	 * NOTE: the explicit `$s = $s . x` form (OP_CAT + OP_STORE) is not covered here
	 * and remains O(n^2) by design. */
	if( nIdx != SXU32_HIGH
	 && nIdx != pNos->nIdx
	 && (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0
	 && (SyHashTotalEntry(&pVm->hTypedSlot) == 0
	     || SyHashGet(&pVm->hTypedSlot,(const void *)&nIdx,sizeof(sxu32)) == 0) ){
		/* e.g. $x = 5; $x .= "a";  ->  "5a" (user-visible: warns if $x is an array) */
		PH7_MemObjToStringUV(pObj);
		if( SyBlobLength(&pNos->sBlob) > 0 ){
			if( PH7_MemObjStringAppend(pObj,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){
				/* Allocation failure: the grow happens before the copy, so pObj
				 * keeps its prior valid contents — raise the fatal uncorrupted. */
				PH7_VmMemoryError(&(*pVm));
				goto Abort;
			}
		}
		/* Produce the expression result. A `.=` result is a temporary, never an
		 * addressable lvalue, so nIdx is SXU32_HIGH (otherwise `f($s .= "x")` with a
		 * by-ref param, or `&($s .= "x")`, would alias the live variable).
		 * In the dominant statement form `$s .= "x";` the result is discarded by the
		 * very next opcode (OP_POP), so we skip building it and leave the (harmless)
		 * RHS operand for the POP to drop — keeping the hot path allocation-free.
		 * Otherwise the result is consumed, so materialize an INDEPENDENT owned copy
		 * of the updated value: a read-only alias into pObj's buffer would dangle if
		 * the same slot is appended to again later in the statement
		 * (e.g. `($s .= "a") . ($s .= "b")` reallocs the buffer the first result
		 * still points at). Peeking pInstr+1 is safe: the compiler always emits a
		 * terminating OP_DONE, so it is in-bounds inside any non-DONE opcode. */
		if( (pInstr+1)->iOp != PH7_OP_POP ){
			PH7_MemObjStore(pObj,pNos);
		}
		/* A hooked lvalue's scratch slot was appended in place — dispatch the
		 * set side now (the consume reads the computed value from the slot). */
		PH7_HOOK_RMW_WRITEBACK(nIdx,0);
		pNos->nIdx = SXU32_HIGH;
		VmPopOperand(&pTos,1);
		break;
	}
	/* Slow path: read-only/typed/constant-attribute/self-aliasing lvalues. */
	/* Force a string cast (user-visible: warns if the lvalue is an array, §2) */
	PH7_MemObjToStringUV(pTos);
	/* Perform the concatenation (Reverse order) */
	if( SyBlobLength(&pNos->sBlob) > 0 ){
		if( PH7_MemObjStringAppend(pTos,(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob)) != SXRET_OK ){
			/* Allocation failure: raise a fatal before committing the store so
			 * no partially-concatenated value is written to the lvalue. */
			PH7_VmMemoryError(&(*pVm));
			goto Abort;
		}
	}
	/* Perform the store operation */
	if( pTos->nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pTos);
		PH7_MemObjStore(pTos,pObj);
	}
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);
	PH7_MemObjStore(pTos,pNos);
	VmPopOperand(&pTos,1);
	break;
				}
/* OP_AND: * * *
 *
 * Pop two values off the stack.  Take the logical AND of the
 * two values and push the resulting boolean value back onto the
 * stack.
 */
/* OP_OR: * * *
 *
 * Pop two values off the stack.  Take the logical OR of the
 * two values and push the resulting boolean value back onto the
 * stack.
 */
case PH7_OP_LAND:
case PH7_OP_LOR: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpLor(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * OP_NULLC: * * *
 * Null coalescing operator '??'.
 * Pop two values (left=pNos, right=pTos). If left is not NULL, push left.
 * Otherwise push right. This is equivalent to: isset($a) ? $a : $b
 */
/*
 * OP_NULLC: * P2 *
 * Short-circuit null coalescing '??'.
 * If TOS is NOT null, jump to P2 (keeping TOS — the non-null value).
 * If TOS IS null, pop it and fall through to evaluate the RHS.
 */
case PH7_OP_NULLC: {
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){
		/* Left is not null — keep it and skip the RHS */
		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */
	}else{
		/* Left is null — discard it, fall through to evaluate RHS */
		VmPopOperand(&pTos, 1);
	}
	break;
}
/*
 * OP_NULLC_JMP: * P2 *
 * Null coalescing assignment short-circuit.
 * If TOS is NOT null, jump to P2 (keeping TOS as the expression result).
 * If TOS IS null, fall through with TOS retained — it carries the LHS's
 * nIdx so the upcoming NULLC_STORE can write back into the variable slot.
 */
case PH7_OP_NULLC_JMP: {
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if( (pTos->iFlags & MEMOBJ_NULL) == 0 ){
		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) —
		 * a pending ??= write-back entry armed by the preceding OP_MEMBER is
		 * dropped by the fetch-point sweep (the landing pc is past its
		 * OP_NULLC_STORE window): the skipped assign never dispatches. */
	}
	break;
}
/*
 * OP_NULLC_STORE: * * *
 * Null coalescing assignment store.
 * Stack: [..., LHS_null(nIdx=X), RHS_value]. Store RHS into aMemObj[X],
 * replace pNos with the RHS value, pop pTos. Leaves the RHS value as the
 * expression result.
 */
/*
 * OP_NULLSAFE_JMP: * P2 *
 * Nullsafe object operator short-circuit (PHP 8.0 `?->`).
 * Peek TOS (the object operand): if it is null, jump to P2 leaving NULL
 * on the stack as the result of the entire containing postfix chain. If
 * non-null, fall through without modifying the stack so the following
 * PH7_OP_MEMBER can consume the object as usual.
 */
case PH7_OP_NULLSAFE_JMP: {
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	if( (pTos->iFlags & MEMOBJ_NULL) || pTos->iFlags == 0 ){
		/* Object operand is NULL (or uninitialized) — short-circuit. The
		 * NULL slot already on TOS becomes the chain's final value. */
		pc = pInstr->iP2 - 1; /* Jump (will be incremented by the loop) */
	}
	break;
}
case PH7_OP_NULLC_STORE: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpNullcStore(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * OP_SPREAD: * * *
 * Argument unpacking.  TOS must be an array (hashmap).
 * Replace TOS with the array's individual elements pushed onto the stack.
 * Records one VmSpreadRun per expansion so the CALL/NEW that consumes this
 * argument list can derive its own argument-count growth (VmSpreadOwnExtra) —
 * the CALL may not be the next instruction, and may be an inner call whose own
 * spreads must stay scoped to it.
 * The expansion tail is shared between the plain-array and the materialized
 * Traversable paths — see VmSpreadExpandMap right above the dispatch loop.
 */
case PH7_OP_SPREAD: {
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Traversable argument unpacking f(...$it): materialize the iterator into a
	 * temp array (positional values), then expand it onto the operand stack
	 * like an array. Materialising first leaves the stack untouched until the
	 * walk succeeds; values are deep-copied (PH7_MemObjStore) so the temp can
	 * be freed immediately. */
	if( VmValueIsTraversable(pVm,pTos) ){
		ph7_hashmap *pTmpMap = PH7_NewHashmap(&(*pVm),0,0);
		sxi32 rcW;
		if( pTmpMap == 0 ){ goto Abort; }
		rcW = PH7_VmIteratorWalk(&(*pVm),pTos,VmSpreadValuesStep,pTmpMap);
		if( rcW == PH7_EXCEPTION || rcW == PH7_ABORT ){
			PH7_HashmapRelease(pTmpMap,TRUE);
			if( rcW == PH7_ABORT ){ goto Abort; }
			goto Exception;
		}
		/* Grow the operand stack if this expansion would overflow it (no longer a
		 * VM_STACK_GUARD cap). On OOM keep the old buffer and leave the source as a
		 * single argument — the historical bounded-fallback, now only under OOM. */
		if( !VmSpreadEnsureCapacity(pVm, pTmpMap->nEntry, &pStack, &pTos, &sState,
		                            pCallTop, ppBaseOwner, pnBaseCap) ){
			PH7_HashmapRelease(pTmpMap,TRUE);
			VmErrorFormat(&(*pVm), PH7_CTX_ERR,
				"Argument unpacking: out of memory while expanding %u elements",
				pTmpMap->nEntry);
			break;
		}
		VmSpreadExpandMap(pVm, &pTos, pTmpMap);
		PH7_HashmapRelease(pTmpMap,TRUE);
		break;
	}
	if( pTos->iFlags & MEMOBJ_HASHMAP ){
		ph7_hashmap *pMap = (ph7_hashmap *)pTos->x.pOther;
		if( !VmSpreadEnsureCapacity(pVm, pMap->nEntry, &pStack, &pTos, &sState,
		                            pCallTop, ppBaseOwner, pnBaseCap) ){
			VmErrorFormat(&(*pVm), PH7_CTX_ERR,
				"Argument unpacking: out of memory while expanding %u elements",
				pMap->nEntry);
			break;
		}
		VmSpreadExpandMap(pVm, &pTos, pMap);
	}
	/* else: not an array — leave as-is (single arg) */
	break;
}
/*
 * OP_FLAG_SPREAD: * * *
 * Mark the value at TOS as a spread source for the next LOAD_MAP.
 * Used by array literal unpacking '[...$arr]'.
 */
case PH7_OP_FLAG_SPREAD: {
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	pTos->iFlags |= MEMOBJ_AUX_SPREAD;
	break;
}
/* OP_LXOR: * * *
 *
 * Pop two values off the stack. Take the logical XOR of the
 * two values and push the resulting boolean value back onto the
 * stack.
 * According to the PHP language reference manual:
 *  $a xor $b is evaluated to TRUE if either $a or $b is
 *  TRUE,but not both.
 */
case PH7_OP_LXOR:{
	ph7_value *pNos = &pTos[-1];
	sxi32 v = 0;
#ifdef UNTRUST
	if( pNos < pStack ){
		goto Abort;
	}
#endif
	/* Force a boolean cast */
	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){
		PH7_MemObjToBool(pTos);
	}
	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){
		PH7_MemObjToBool(pNos);
	}
	if( (pNos->x.iVal && !pTos->x.iVal) || (pTos->x.iVal && !pNos->x.iVal) ){
		v = 1;
	}
	VmPopOperand(&pTos,1);
	pTos->x.iVal = v;
	MemObjSetType(pTos,MEMOBJ_BOOL);
	break;
				 }
/* OP_EQ P1 P2 P3
 *
 * Pop the top two elements from the stack.  If they are equal, then
 * jump to instruction P2.  Otherwise, continue to the next instruction.
 * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not.
 */
/* OP_NEQ P1 P2 P3
 *
 * Pop the top two elements from the stack. If they are not equal, then
 * jump to instruction P2. Otherwise, continue to the next instruction.
 * If P2 is zero, do not jump.  Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not.
 */
case PH7_OP_EQ:
case PH7_OP_NEQ: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpNeq(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/* OP_TEQ P1 P2 *
 *
 * Pop the top two elements from the stack. If they have the same type and are equal
 * then jump to instruction P2. Otherwise, continue to the next instruction.
 * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not.
 */
case PH7_OP_TEQ: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpTeq(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/* OP_TNE P1 P2 *
 *
 * Pop the top two elements from the stack.If they are not equal an they are not
 * of the same type, then jump to instruction P2. Otherwise, continue to the next
 * instruction.
 * If P2 is zero, do not jump. Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not.
 *
 */
case PH7_OP_TNE: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpTne(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/* OP_LT P1 P2 P3
 *
 * Pop the top two elements from the stack. If the second element (the top of stack)
 * is less than the first (next on stack),then jump to instruction P2.Otherwise
 * continue to the next instruction. In other words, jump if pNos<pTos.
 * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not.
 *
 */
/* OP_LE P1 P2 P3
 *
 * Pop the top two elements from the stack. If the second element (the top of stack)
 * is less than or equal to the first (next on stack),then jump to instruction P2.
 * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.
 * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not.
 *
 */
case PH7_OP_LT:
case PH7_OP_LE: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpLe(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/* OP_GT P1 P2 P3
 *
 * Pop the top two elements from the stack. If the second element (the top of stack)
 * is greater than the first (next on stack),then jump to instruction P2.Otherwise
 * continue to the next instruction. In other words, jump if pNos<pTos.
 * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not.
 *
 */
/* OP_GE P1 P2 P3
 *
 * Pop the top two elements from the stack. If the second element (the top of stack)
 * is greater than or equal to the first (next on stack),then jump to instruction P2.
 * Otherwise continue to the next instruction. In other words, jump if pNos<pTos.
 * If P2 is zero, do not jump.Instead, push a boolean 1 (TRUE) onto the
 * stack if the jump would have been taken, or a 0 (FALSE) if not.
 *
 */
case PH7_OP_GT:
case PH7_OP_GE: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpGe(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/* OP_SPACESHIP * * *
 *
 * Pop the top two elements from the stack. Push an integer result:
 *   -1 if left < right
 *    0 if left == right
 *    1 if left > right
 * Uses loose comparison (type juggling), same as <, >, ==.
 */
case PH7_OP_SPACESHIP: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpSpaceship(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * OP_LOAD_REF * * *
 * Push the index of a referenced object on the stack.
 */
case PH7_OP_LOAD_REF: {
	sxu32 nIdx;
#ifdef UNTRUST
	if( pTos < pStack ){
		goto Abort;
	}
#endif
	/* Extract memory object index */
	nIdx = pTos->nIdx;
	if( nIdx != SXU32_HIGH /* Not a constant */ ){
		/* Nullify the object */
		PH7_MemObjRelease(pTos);
		/* Mark as constant and store the index on the top of the stack */
		pTos->x.iVal = (sxi64)nIdx;
		pTos->nIdx = SXU32_HIGH;
		pTos->iFlags = MEMOBJ_INT|MEMOBJ_REFERENCE;
	}
	break;
					  }
/*
 * OP_STORE_REF * * P3
 * Perform an assignment operation by reference.
 */
case PH7_OP_STORE_REF: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpStoreRef(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * OP_UPLINK P1 * *
 * Link a variable to the top active VM frame.
 * This is used to implement the 'global' PHP construct.
 */
case PH7_OP_UPLINK: {
	if( pVm->pFrame->pParent ){
		ph7_value *pLink = &pTos[-pInstr->iP1+1];
		SyString sName;
		/* Perform the link */
		while( pLink <= pTos ){
			/* Force a string cast — global $$arr link name (user-visible, §2) */
			PH7_MemObjToStringUV(pLink);
			SyStringInitFromBuf(&sName,SyBlobData(&pLink->sBlob),SyBlobLength(&pLink->sBlob));
			if( sName.nByte > 0 ){
				VmFrameLink(&(*pVm),&sName);
			}
			pLink++;
		}
	}
	VmPopOperand(&pTos,pInstr->iP1);
	break;
					}
/*
 * OP_LOAD_EXCEPTION * P2 P3
 * Push an exception in the corresponding container so that
 * it can be thrown later by the OP_THROW instruction.
 */
case PH7_OP_LOAD_EXCEPTION: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpLoadException(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * OP_POP_EXCEPTION * * P3
 * Pop a previously pushed exception from the corresponding container.
 */
case PH7_OP_POP_EXCEPTION: {
	ph7_exception *pCompiledExc = (ph7_exception *)pInstr->p3;
	/* BYTECODE stage 2b: the stack holds ACTIVATIONS — pop ours when it is on
	 * top (matched by compiled origin). pException == NULL means this try's
	 * activation was already consumed (an in-place catch handled a throw and
	 * ran the finally itself); compiled fields keep coming from p3. */
	ph7_exception *pException = 0;
	if( SySetUsed(&pVm->aException) > 0 ){
		ph7_exception **apTop = (ph7_exception **)SySetBasePtr(&pVm->aException);
		ph7_exception *pTop = apTop[SySetUsed(&pVm->aException) - 1];
		/* Same compiled origin is NOT enough: under recursion, when THIS
		 * level's activation was already consumed by an in-place catch (the
		 * resume lands right here), the top can be an OUTER level's activation
		 * of the same lexical try — popping it would run that level's finally
		 * early and orphan its handler (probed: recursive try/catch/finally
		 * lost the outer catch entirely). The activation must also belong to
		 * the CURRENT body frame. */
		if( VmExcMatches(pTop,pCompiledExc)
		 && pTop->pFrame == VmSkipExceptionFrames(pVm->pFrame) ){
			pException = pTop;
			(void)SySetPop(&pVm->aException);
		}
	}
	if( pCompiledExc->iInlined ){
		/* ROOT C: end of a try body or a catch body on the NORMAL (non-throwing) path.
		 * Pop this try's handler off aException so a throw in the finally propagates to
		 * an outer handler. With a finally, seed a FALLTHROUGH action and KEEP the
		 * transparent frame (OP_END_FINALLY leaves it after the finally runs); the
		 * compiler-emitted JMP falls into the finally. Without a finally, this is the
		 * whole exit: leave the frame and let the JMP reach the post-construct landing. */
		VmExcRelease(&(*pVm),pException); /* activation consumed (may be NULL) */
		if( pCompiledExc->iHasFinally ){
			VmFinallyAction sAct;
			SyZero(&sAct,sizeof(sAct));
			sAct.eKind = PH7_FA_FALLTHROUGH;
			sAct.iNextPc = pCompiledExc->iEndCatchPc;
			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);
			/* keep the transparent frame for OP_END_FINALLY */
		}else if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){
			VmLeaveFrame(&(*pVm));
		}
		break;
	}
	/* Leave the exception frame. It is normally on top here (a try that fell through
	 * normally, or the frame VmRecordedResume left for an in-place catch). But for a
	 * RESUMED generator/fiber body whose yield was inside this try, that exception
	 * frame was discarded at suspend (VmStartCtx/VmResumeCtx save only the body frame),
	 * so pVm->pFrame is the coroutine body itself — popping it would destroy the
	 * coroutine (and defeat the bHasRet materialization just below, which must see the
	 * body). Only leave a genuine exception frame. */
	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){
		VmLeaveFrame(&(*pVm));
	}
	/* Execute the finally block if present and not already executed by the
	 * catch path. No live activation (pException == NULL) means an in-place
	 * catch consumed it — and that path runs the finally itself — so skip. */
	if( pException && pCompiledExc->iHasFinally && !pException->iFinallyDone ){
		sxi32 rcFinally;
		VmExcRelease(&(*pVm),pException);
		pException = 0;
		rcFinally = VmLocalExec(&(*pVm),&pCompiledExc->sFinally,0,TRUE);
		if( rcFinally == SXERR_ABORT ){
			goto Abort;
		}
		if( rcFinally == PH7_EXCEPTION ){
			/* The finally threw past itself. If an enclosing try IN THIS function
			 * caught the new exception in place, resume at its landing pad;
			 * otherwise it was caught at an outer frame, so unwind this function. */
			sxi32 iResumePc;
			if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){
				pc = iResumePc;
				break;
			}
			goto Exception;
		}
	}
	VmExcRelease(&(*pVm),pException); /* no-finally / already-done paths (may be NULL) */
	if( VmSkipExceptionFrames(pVm->pFrame)->bHasRet ){
		/* `return` inside the finally (normal try completion) returns from the
		 * function. The return targets the body frame this try belongs to. Drain
		 * outer finally blocks first, then — only in the real function body
		 * (sState.pEntryFrame IS that body) — materialize; inside a mini-program (an inline
		 * try within a catch/finally) propagate outward so the owning body returns. */
		rc = VmDrainFinally(&(*pVm),sState.nExceptionBase);
		if( rc == SXERR_ABORT ){
			goto Abort;
		}
		if( rc == PH7_EXCEPTION ){
			goto Exception;
		}
		if( !sState.bReturnPropagates ){
			VmMaterializeCatchReturn(&(*pVm),sState.pResult,sState.pEntryFrame);
		}
		goto Done;
	}
	break;
							}
/*
 * OP_CATCH iP1(catch-index) * P3(ph7_exception)
 * ROOT C: entry of an inline catch body. Bind the in-flight exception (held on
 * pException->pInflight by VmThrowInline) into the catch variable, resolved in the
 * enclosing body's scope (PHP: a catch shares the surrounding variable scope).
 */
case PH7_OP_CATCH: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpCatch(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * OP_END_FINALLY * P3(ph7_exception)
 * ROOT C: terminate an inline finally. Leave the try's transparent frame and dispatch
 * the pending action queued when the finally was entered (fall-through / re-throw /
 * return / break-continue). A return/break threads out through each enclosing finally
 * via pException->iNextFinallyPc.
 */
case PH7_OP_END_FINALLY: {
	ph7_exception *pExc = (ph7_exception *)pInstr->p3;
	VmFinallyAction sAct;
	int eKind = PH7_FA_FALLTHROUGH;
	/* Leave the try's transparent frame kept alive across the finally. */
	if( pVm->pFrame->iFlags & VM_FRAME_EXCEPTION ){
		VmLeaveFrame(&(*pVm));
	}
	if( SySetUsed(&pVm->aFinallyAction) > 0 ){
		VmFinallyAction *aA = (VmFinallyAction *)SySetBasePtr(&pVm->aFinallyAction);
		sAct = aA[SySetUsed(&pVm->aFinallyAction) - 1];
		(void)SySetPop(&pVm->aFinallyAction);
		eKind = sAct.eKind;
	}else{
		SyZero(&sAct,sizeof(sAct));
		sAct.iNextPc = pExc->iEndCatchPc;
	}
	if( eKind == PH7_FA_FALLTHROUGH ){
		pc = (sxi32)(sAct.iNextPc ? sAct.iNextPc : pExc->iEndCatchPc) - 1;
		break;
	}else if( eKind == PH7_FA_JMP ){
		/* break/continue: run the remaining crossed finallys, then take the jump. */
		sxu32 iFpc = 0;
		int nCross = sAct.nCross;
		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){
			sAct.nCross = nCross;
			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);
			pc = (sxi32)iFpc - 1;
			break;
		}
		pc = (sxi32)sAct.iNextPc - 1;
		break;
	}else if( eKind == PH7_FA_RETHROW ){
		ph7_class_instance *pRe = sAct.pExc;
		sxi32 _iRpE;
		rc = VmThrowException(&(*pVm),pRe);
		if( pRe ){ PH7_ClassInstanceUnref(pRe); }
		if( rc == SXERR_ABORT ){ goto Abort; }
		PH7_INLINE_RESUME_BREAK()
		if( VmRecordedResume(pVm,&_iRpE,sState.pEntryFrame,aInstr) ){ pc = _iRpE; break; }
		goto Exception;
	}else{ /* PH7_FA_RETURN */
		sxu32 iFpc = 0;
		int nCross = sAct.nCross;
		if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){
			/* Thread the return through the next enclosing finally. */
			sAct.nCross = nCross;
			SySetPut(&pVm->aFinallyAction,(const void *)&sAct);
			pc = (sxi32)iFpc - 1;
			break;
		}
		/* No enclosing finally left: materialize the return from this body. */
		if( sAct.bHasRetVal && sState.pResult ){
			PH7_MemObjStore(&sAct.sRet,sState.pResult);
		}
		PH7_MemObjRelease(&sAct.sRet);
		goto Done;
	}
						 }
/*
 * OP_SET_FINALLY_RET iP1(hasVal) * P3(ph7_exception first finally)
 * ROOT C: a `return` inside an inline try/catch. Pop the return value (if any) into a
 * pending RETURN action and enter the innermost enclosing finally (p3->iFinallyPc);
 * OP_END_FINALLY threads it out through the finally chain and then returns.
 */
case PH7_OP_SET_FINALLY_RET: {
	VmFinallyAction sAct;
	sxu32 iFpc = 0;
	int nCross = -1; /* a return crosses every enclosing finally in this function */
	SyZero(&sAct,sizeof(sAct));
	sAct.eKind = PH7_FA_RETURN;
	sAct.pTargetBody = (void *)VmSkipExceptionFrames(pVm->pFrame);
	PH7_MemObjInit(pVm,&sAct.sRet);
	if( pInstr->iP1 && pTos >= pStack ){
		PH7_MemObjStore(pTos,&sAct.sRet);
		sAct.bHasRetVal = 1;
		VmPopOperand(&pTos,1);
	}
	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){
		sAct.nCross = nCross;
		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);
		pc = (sxi32)iFpc - 1;
		break;
	}
	/* No enclosing finally left: return now. */
	if( sAct.bHasRetVal && sState.pResult ){
		PH7_MemObjStore(&sAct.sRet,sState.pResult);
	}
	PH7_MemObjRelease(&sAct.sRet);
	goto Done;
						 }
/*
 * OP_SET_FINALLY_JMP iP2(target pc) * P3(ph7_exception first finally)
 * ROOT C: a `break`/`continue` crossing an inline try-with-finally. Queue a JMP action
 * (resume at iP2 after the finally chain) and enter the innermost enclosing finally.
 */
case PH7_OP_SET_FINALLY_JMP: {
	VmFinallyAction sAct;
	sxu32 iFpc = 0;
	int nCross = (int)pInstr->iP1; /* number of enclosing trys to cross to reach the loop */
	SyZero(&sAct,sizeof(sAct));
	sAct.eKind = PH7_FA_JMP;
	sAct.iNextPc = pInstr->iP2;
	if( VmFinallyAdvance(&(*pVm),aInstr,&nCross,&iFpc) ){
		sAct.nCross = nCross;
		SySetPut(&pVm->aFinallyAction,(const void *)&sAct);
		pc = (sxi32)iFpc - 1;
		break;
	}
	/* No finally among the crossed trys: just take the break/continue jump. */
	pc = (sxi32)sAct.iNextPc - 1;
	break;
						 }
/*
 * OP_THROW * P2 *
 * Throw an user exception.
 */
case PH7_OP_THROW: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpThrow(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * OP_FOREACH_INIT * P2 P3
 * Prepare a foreach step.
 */
case PH7_OP_FOREACH_INIT: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpForeachInit(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * OP_FOREACH_STEP * P2 P3
 * Perform a foreach step. Jump to P2 at the end of the step.
 */
case PH7_OP_FOREACH_STEP: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpForeachStep(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
						  }
/*
 * OP_MEMBER P1 P2
 * Load class attribute/method on the stack.
 */
case PH7_OP_MEMBER: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpMember(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * OP_NEW P1 * * *
 *  Create a new class instance (Object in the PHP jargon) and push that object on the stack.
 */
case PH7_OP_NEW: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpNew(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * OP_CLONE * * *
 * Perfome a clone operation.
 */
case PH7_OP_CLONE: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpClone(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * OP_CLONE_APPLY * * *
 *  Apply the PHP 8.5 clone($obj, $withProperties) property updates. The updates
 *  array is on the stack top and the freshly-cloned object (from OP_CLONE) is
 *  directly below it. Each entry is applied as a scope-aware property write
 *  (AFTER __clone() has already run); the array is then popped, leaving the
 *  clone as the result.
 */
case PH7_OP_CLONE_APPLY: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpCloneApply(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * OP_SWITCH * * P3
 *  This is the bytecode implementation of the complex switch() PHP construct.
 */
case PH7_OP_SWITCH: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpSwitch(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * OP_MATCH * * P3
 *  PHP 8.0 match expression. P3 points to a ph7_match struct holding
 *  the compiled arms. On entry, the subject is on top of the stack.
 *  On exit, the stack slot holds the matched arm's result value.
 *  Comparison is strict (===). No fallthrough. When no arm matches and
 *  no default is present, a fatal UnhandledMatchError is raised.
 */
case PH7_OP_MATCH: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpMatch(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }
/*
 * OP_YIELD P1 P2 *
 *  Yield a value from a generator function.
 *  P1=1 if value on stack, P1=0 for bare yield.
 *  P2=1 if key=>value syntax (key below value on stack).
 */
case PH7_OP_YIELD: {
	ph7_generator *pGen;
	if( pVm->pActiveCtx == 0 || pVm->pActiveCtx->pPrivate == 0 ){
		VmErrorFormat(&(*pVm), PH7_CTX_ERR, "Cannot use yield outside of a generator");
		goto Abort;
	}
	if( pVm->pActiveCtx->bClosing ){
		/* A `finally` reached while VmCloseCtx force-drives this destroyed generator's
		 * pending finallys tried to yield — PHP forbids it. */
		if( VmThrowFixedError(&(*pVm), "Error",
			"Cannot yield from finally in a force-closed generator") == PH7_ABORT ){
			goto Abort;
		}
		goto Exception;
	}
	pGen = (ph7_generator *)pVm->pActiveCtx->pPrivate;
	if( pInstr->iP2 ){
		/* yield $key => $value: value on top, key below */
#ifdef UNTRUST
		if( pTos < &pStack[1] ) goto Abort;
#endif
		PH7_MemObjStore(pTos, &pGen->sYieldValue);
		VmPopOperand(&pTos, 1);
		PH7_MemObjStore(pTos, &pGen->sYieldKey);
		VmPopOperand(&pTos, 1);
		/* If explicit key is integer, advance iImplicitKey past it (PHP compat) */
		if( pGen->sYieldKey.iFlags & MEMOBJ_INT ){
			sxi64 nKey = pGen->sYieldKey.x.iVal;
			if( nKey >= pGen->iImplicitKey ){
				pGen->iImplicitKey = nKey + 1;
			}
		}
	}else if( pInstr->iP1 ){
		/* yield $value */
#ifdef UNTRUST
		if( pTos < pStack ) goto Abort;
#endif
		PH7_MemObjStore(pTos, &pGen->sYieldValue);
		VmPopOperand(&pTos, 1);
		/* Auto-increment key */
		PH7_MemObjRelease(&pGen->sYieldKey);
		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;
		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);
	}else{
		/* Bare yield — null value, auto-increment key */
		PH7_MemObjRelease(&pGen->sYieldValue);
		PH7_MemObjRelease(&pGen->sYieldKey);
		pGen->sYieldKey.x.iVal = pGen->iImplicitKey++;
		MemObjSetType(&pGen->sYieldKey, MEMOBJ_INT);
	}
	/* Suspend execution — resume will push the send() value as the yield result */
	VmSuspendCtx(pVm, pVm->pActiveCtx, pc + 1, (sxi32)(pTos - pStack));
	goto Suspend;
}
/*
 * OP_YIELD_FROM * * *
 *
 * Generator delegation: `yield from <iterable>`. Re-yield every (key,value) of an
 * array/Traversable/Generator from the OUTER generator, preserving the inner
 * keys; the expression evaluates to the inner Generator's return value (or NULL).
 *
 * This opcode is RE-ENTRANT: it suspends back to its own pc and, on each resume,
 * advances the per-instance delegate cursor stored on the exec context (never the
 * shared foreach aStep, so independent generator instances cannot clash). The
 * iterable operand is consumed on first entry; the expression result is pushed at
 * exhaustion — net stack effect +1, identical to OP_YIELD.
 */
case PH7_OP_YIELD_FROM: {
	ph7_generator *pGenFrom;
	ph7_exec_ctx *pCtxFrom;
	ph7_value sKey,sVal;
	sxi32 rcm = SXRET_OK;   /* delegate iterator-method status */
	int bExhausted = 0;
	if( pVm->pActiveCtx == 0 || pVm->pActiveCtx->pPrivate == 0 ){
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot use \"yield from\" outside of a generator");
		goto Abort;
	}
	if( pVm->pActiveCtx->bClosing ){
		/* A `yield from` reached while VmCloseCtx force-drives this destroyed
		 * generator's pending finallys — PHP forbids it (distinct message from a
		 * bare `yield`, mirroring the OP_YIELD guard above). */
		if( VmThrowFixedError(&(*pVm), "Error",
			"Cannot use \"yield from\" in a force-closed generator") == PH7_ABORT ){
			goto Abort;
		}
		goto Exception;
	}
	pCtxFrom = pVm->pActiveCtx;
	pGenFrom = (ph7_generator *)pCtxFrom->pPrivate;
	PH7_MemObjInit(pVm,&sKey);
	PH7_MemObjInit(pVm,&sVal);
	if( pCtxFrom->iDelegateState == 0 ){
		/* First entry: classify the iterable on the stack top. */
		int bIterable = 1;
#ifdef UNTRUST
		if( pTos < pStack ){ goto Abort; }
#endif
		if( pTos->iFlags & MEMOBJ_HASHMAP ){
			PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);
			pCtxFrom->pDelegateNode = ((ph7_hashmap *)pCtxFrom->sDelegate.x.pOther)->pFirst;
			pCtxFrom->iDelegateState = 1;
		}else if( pTos->iFlags & MEMOBJ_OBJ ){
			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;
			ph7_class *pIterCls = PH7_VmExtractClass(&(*pVm),"Iterator",sizeof("Iterator")-1,FALSE,0);
			if( pVm->pGeneratorClass && PH7_VmInstanceOf(pThis->pClass,pVm->pGeneratorClass) ){
				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);
				pCtxFrom->iDelegateState = 3;
			}else if( pIterCls && PH7_VmInstanceOf(pThis->pClass,pIterCls) ){
				PH7_MemObjStore(pTos,&pCtxFrom->sDelegate);
				pCtxFrom->iDelegateState = 2;
			}else{
				ph7_class *pAggCls = PH7_VmExtractClass(&(*pVm),"IteratorAggregate",
					sizeof("IteratorAggregate")-1,FALSE,0);
				if( pAggCls && PH7_VmInstanceOf(pThis->pClass,pAggCls) ){
					/* Delegate to the Iterator returned by getIterator() */
					ph7_value sIt;
					PH7_MemObjInit(pVm,&sIt);
					rcm = VmIterCallMethod(pVm,pThis,"getIterator",sizeof("getIterator")-1,&sIt);
					if( rcm == PH7_ABORT || rcm == PH7_EXCEPTION ){
						/* getIterator() threw/aborted: drop it, consume the
						 * operand, and propagate. */
						PH7_MemObjRelease(&sIt);
						VmPopOperand(&pTos,1);
						goto yf_propagate;
					}
					if( (sIt.iFlags & MEMOBJ_OBJ) && sIt.x.pOther && pIterCls
						&& PH7_VmInstanceOf(((ph7_class_instance *)sIt.x.pOther)->pClass,pIterCls) ){
						PH7_MemObjStore(&sIt,&pCtxFrom->sDelegate);
						pCtxFrom->iDelegateState = 2;
					}else{
						bIterable = 0;
					}
					PH7_MemObjRelease(&sIt);
				}else{
					bIterable = 0;
				}
			}
		}else{
			bIterable = 0;
		}
		VmPopOperand(&pTos,1); /* Consume the iterable operand */
		if( !bIterable ){
			/* Non-iterable source: throw a catchable Error (PHP 8.5), then
			 * funnel through the shared teardown/route path. */
			rc = VmThrowFromVm(&(*pVm),"Error",
				"Can use \"yield from\" only with arrays and Traversables",
				sizeof("Can use \"yield from\" only with arrays and Traversables")-1);
			rcm = (rc == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;
			goto yf_propagate;
		}
		if( pCtxFrom->iDelegateState >= 2 ){
			/* rewind() the delegate (also starts a fresh generator) */
			rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,
				"rewind",sizeof("rewind")-1,0);
			if( rcm == PH7_ABORT || rcm == PH7_EXCEPTION ){ goto yf_propagate; }
		}
	}else{
		/* Resume entry: the value VmResumeCtx pushed is the send() value (null for a
		 * plain next()). For a Generator delegate (state 3) PHP forwards send()/throw()
		 * transparently INTO the inner generator; arrays/plain Iterators (state 1/2)
		 * ignore send() and just advance with next(). */
#ifdef UNTRUST
		if( pTos < pStack ){ goto Abort; }
#endif
		if( pCtxFrom->iDelegateState == 3 ){
			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);
			/* A pending Generator::throw() on the outer was parked on pInjected by the
			 * body-entry gate (which skips its own raise for state 3); forward it as a
			 * throw into the delegate, else forward the sent value (pTos, which
			 * VmResumeCtx copies into the inner's own stack). */
			ph7_class_instance *pInjFwd = pCtxFrom->pInjected;
			pCtxFrom->pInjected = 0;
			if( pInner && pInner->pCtx && pInner->pCtx->iState == PH7_CTX_STATE_SUSPENDED ){
				if( pInjFwd ){
					/* Borrowed ref: the outer's Generator::throw() holds it across this
					 * whole resume, so the inner inject path must not unref it. */
					pInner->pCtx->pInjected = pInjFwd;
					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,0,0);
					pInner->pCtx->pInjected = 0;
				}else{
					rcm = VmResumeCtx(&(*pVm),pInner->pCtx,pTos,0);
				}
			}else if( pInjFwd ){
				/* No live delegate to receive the throw (inner already finished/closed):
				 * raise it at the yield-from in the outer generator's own frame. */
				VmFrame *pTF = VmSkipExceptionFrames(pVm->pFrame);
				pTF->iFlags |= VM_FRAME_THROW;
				rcm = (VmThrowException(&(*pVm),pInjFwd) == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;
			}
			PH7_MemObjRelease(pTos);
			pTos--;
			if( rcm == PH7_ABORT || rcm == PH7_EXCEPTION ){ goto yf_propagate; }
		}else{
			PH7_MemObjRelease(pTos);
			pTos--;
			if( pCtxFrom->iDelegateState >= 2 ){
				rcm = VmIterCallMethod(pVm,(ph7_class_instance *)pCtxFrom->sDelegate.x.pOther,
					"next",sizeof("next")-1,0);
				if( rcm == PH7_ABORT || rcm == PH7_EXCEPTION ){ goto yf_propagate; }
			}
		}
	}
	/* Fetch the current (key,value) of the delegate, or mark exhausted. */
	if( pCtxFrom->iDelegateState == 1 ){
		if( pCtxFrom->pDelegateNode == 0 ){
			bExhausted = 1;
		}else{
			PH7_HashmapExtractNodeKey(pCtxFrom->pDelegateNode,&sKey);
			PH7_HashmapExtractNodeValue(pCtxFrom->pDelegateNode,&sVal,TRUE);
			/* Forward traversal follows pPrev (the hashmap's "reverse link",
			 * matching PH7_HashmapGetNextEntry). */
			pCtxFrom->pDelegateNode = pCtxFrom->pDelegateNode->pPrev;
		}
	}else{
		ph7_class_instance *pThis = (ph7_class_instance *)pCtxFrom->sDelegate.x.pOther;
		ph7_value sValid;
		int isValid;
		PH7_MemObjInit(pVm,&sValid);
		rcm = VmIterCallMethod(pVm,pThis,"valid",sizeof("valid")-1,&sValid);
		PH7_MemObjToBool(&sValid);
		isValid = (sValid.x.iVal != 0);
		PH7_MemObjRelease(&sValid);
		if( rcm == PH7_ABORT || rcm == PH7_EXCEPTION ){ goto yf_propagate; }
		if( !isValid ){
			bExhausted = 1;
		}else{
			rcm = VmIterCallMethod(pVm,pThis,"current",sizeof("current")-1,&sVal);
			if( rcm == PH7_ABORT || rcm == PH7_EXCEPTION ){ goto yf_propagate; }
			rcm = VmIterCallMethod(pVm,pThis,"key",sizeof("key")-1,&sKey);
			if( rcm == PH7_ABORT || rcm == PH7_EXCEPTION ){ goto yf_propagate; }
		}
	}
	if( bExhausted ){
		/* Expression value: inner Generator's return value (state 3) or NULL. */
		ph7_value sResult;
		PH7_MemObjInit(pVm,&sResult);
		if( pCtxFrom->iDelegateState == 3 ){
			ph7_generator *pInner = VmGeneratorExtractCtx(&(*pVm),&pCtxFrom->sDelegate);
			if( pInner && pInner->pCtx ){
				PH7_MemObjStore(&pInner->pCtx->sRetValue,&sResult);
			}
		}
		PH7_MemObjRelease(&pCtxFrom->sDelegate);
		pCtxFrom->pDelegateNode = 0;
		pCtxFrom->iDelegateState = 0;
		pTos++;
		PH7_MemObjStore(&sResult,pTos);
		PH7_MemObjRelease(&sResult);
		PH7_MemObjRelease(&sKey);
		PH7_MemObjRelease(&sVal);
		break; /* fall through to pc+1 with the result on the stack top */
	}
	/* Re-yield (key,value) from the OUTER generator, preserving the inner key.
	 * The outer generator's implicit auto-key counter is NOT advanced by the
	 * delegated keys — PHP keeps it independent across `yield from`, so a later
	 * plain `yield` continues from the outer's own counter. */
	PH7_MemObjStore(&sVal,&pGenFrom->sYieldValue);
	PH7_MemObjStore(&sKey,&pGenFrom->sYieldKey);
	PH7_MemObjRelease(&sKey);
	PH7_MemObjRelease(&sVal);
	/* Suspend, re-entering this SAME opcode on resume (pc, not pc+1). */
	VmSuspendCtx(pVm,pCtxFrom,pc,(sxi32)(pTos - pStack));
	goto Suspend;
yf_propagate:
	/* A delegate iterator method threw/aborted (or the source was non-iterable):
	 * tear down the delegation, then route via the shared dispatch macro. rcm is
	 * always PH7_EXCEPTION or PH7_ABORT here. */
	PH7_MemObjRelease(&sKey);
	PH7_MemObjRelease(&sVal);
	PH7_MemObjRelease(&pCtxFrom->sDelegate);
	pCtxFrom->pDelegateNode = 0;
	pCtxFrom->iDelegateState = 0;
	PH7_DISPATCH_ENFORCE_RC(rcm)
	goto Exception; /* defensive default — unreachable for ABORT/EXCEPTION */
}
/*
 * OP_CALL P1 * *
 *  Call a PHP or a foreign function and push the return value of the called
 *  function on the stack.
 */
case PH7_OP_CALL: {
	/* iP2 = hasSpread (compile-time). Count only THIS call's own unpack
	 * expansion (VmSpreadOwnExtra, derived from the captured runs on top of the
	 * stack) — an INNER spread-bearing call evaluated inside this argument list
	 * (`f(...$a, k: g(...$b))`) owns its own runs below the boundary and must not
	 * be conflated, which a single shared accumulator could not express. */
	sxi32 nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);
	ph7_value *pArg;
	pArg = &pTos[-nCallArgs];
	/* PHP 8.1: an unpack whose elements carry string keys binds them as NAMED
	 * arguments, and a spread expanding to !=1 element shifts the actual positions
	 * of any following compile-time named args. The effective per-actual-slot name
	 * map (VmBuildEffectiveArgMap, which also truncates this call's captured runs)
	 * is built PER PATH against that path's finalized arg base — a method call pops
	 * its method-name slot below, shifting pArg — so it is deferred to each dispatch
	 * site rather than built once here. */
	VmCallArgMap sEffMap;
	VmCallArgMap *pEffCallMap = (VmCallArgMap *)pInstr->p3;
	SyHashEntry *pEntry;
	SyString sName;
	/* A Closure object is callable: unwrap it to its underlying string callable so the
	 * dispatch below handles it (rather than treating it as a generic object and looking
	 * for __invoke). pTos here is a stack copy of the call target. Gated on VmValueIsClosure
	 * so a plain __invoke object skips the temp-value work entirely. */
	if( VmValueIsClosure(pVm,pTos) ){
		ph7_value sCallable;
		PH7_MemObjInit(pVm,&sCallable);
		if( VmClosureUnwrap(pVm,pTos,&sCallable) == SXRET_OK ){
			PH7_MemObjRelease(pTos);
			PH7_MemObjStore(&sCallable,pTos);
		}
		PH7_MemObjRelease(&sCallable);
	}
	/* Extract function name */
	if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){
		if( pTos->iFlags & MEMOBJ_HASHMAP ){
			ph7_value sResult;
			sxi32 rcArr;
			{
				/* php validates the SHAPE of an array callable first: it must hold exactly
				 * two elements. PH7 handed any array to the dispatcher, which failed
				 * silently and left NULL behind, so `$a()` on [1] evaluated to nothing. */
				ph7_hashmap *pCbMap = (ph7_hashmap *)pTos->x.pOther;
				char zCbMsg[192];
				const char *zCbErr = 0;
				int bCbRaised = 0; /* the class lookup ran an autoloader that threw */
				if( pCbMap && pCbMap->nEntry == 2 ){
					/* Shape is right; now check it actually RESOLVES. The shared dispatcher
					 * (PH7_VmCallUserFunctionWithMap) answers SXRET_OK with a NULL result for
					 * an unresolvable [class,method] pair -- silence a caller cannot detect --
					 * and its contract is relied on by call_user_func/usort, so the throw
					 * belongs here at the call site. */
					ph7_value *pCbCls = 0;
					ph7_value *pCbMeth = 0;
					/* php reads the INTEGER indices 0 and 1, not the first two entries in
					 * insertion order. Two entries at other keys are a SHAPE error of their
					 * own ("has to contain indices 0 and 1"), distinct from the
					 * wrong-element-COUNT message below; `[1=>'m',0=>'C']` holds both, so
					 * the target is index 0 -- the reverse of what PH7 walked. */
					if( !PH7_VmArrayCallableParts(&(*pVm),pCbMap,&pCbCls,&pCbMeth) ){
						zCbErr = "Array callback has to contain indices 0 and 1";
					}else{
						/* The resolution below can run an autoloader that throws; when it does,
						 * php propagates THAT exception and never reports the class missing. */
						sxi32 nCbBrc = pVm->nBoundaryRc;
						const void *pCbRes = (const void *)pVm->pResumeFrame;
						zCbErr = VmDirectArrayCallableError(&(*pVm),pCbCls,pCbMeth,
							zCbMsg,sizeof(zCbMsg));
						if( zCbErr && PH7_VmClassLookupRaised(&(*pVm),nCbBrc,pCbRes) ){
							bCbRaised = 1;
						}
					}
				}
				if( pCbMap == 0 || pCbMap->nEntry != 2 || zCbErr ){
					sxi32 rcCb;
					if( pInstr->iP2 ){
						VmSpreadConsume(pVm);
					}
					if( nCallArgs > 0 ){
						VmPopOperand(&pTos,nCallArgs);
					}
					PH7_MemObjRelease(pTos);
					MemObjSetType(pTos,MEMOBJ_NULL);
					pTos->nIdx = SXU32_HIGH;
					if( bCbRaised ){
						/* Land the autoloader's own throw: consume the parked status (an
						 * in-place catch leaves 0 behind plus a recorded resume frame, which
						 * the router below picks up). */
						rcCb = pVm->nBoundaryRc;
						pVm->nBoundaryRc = 0;
						if( rcCb == PH7_ABORT ){ goto Abort; }
						rc = PH7_EXCEPTION;
						PH7_THROW_ROUTE_MIDEXPR(rc)
					}
					if( zCbErr == 0 ){
						zCbErr = "Array callback must have exactly two elements";
					}
					rcCb = VmThrowFromVm(&(*pVm),"Error",zCbErr,(sxu32)SyStrlen(zCbErr));
					if( rcCb == SXERR_ABORT ){ goto Abort; }
					rc = rcCb;
					/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch
					 * (SXRET_OK + recorded resume) FELL THROUGH and kept dispatching
					 * the malformed callable inside the try. Route like OP_THROW. */
					PH7_THROW_ROUTE_MIDEXPR(rc)
				}
			}
			/* Build the effective spread-key map (and consume this call's runs)
			 * against this path's arg base (the array-callable slot isn't popped). */
			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,
				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);
			/* D1: an array callable dispatches through the shared helper below, which does not
			 * expose the target's per-parameter by-ref flags here. This path over-vivified every
			 * plain-var argument before D1 (so `[$o,'m'](&$x)` out-params worked); preserve that
			 * by materializing every deferred arg as by-ref. Refining these to precise by-value
			 * semantics is a later slice. */
			{
				sxi32 rcDA = VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,/*bAllByRef*/1,0);
				if( rcDA == PH7_ABORT ){
					goto Abort;
				}else if( rcDA == PH7_EXCEPTION ){
					goto Exception;
				}
			}
			SySetReset(&aArg);
			while( pArg < pTos ){
				SySetPut(&aArg,(const void *)&pArg);
				pArg++;
			}
			PH7_MemObjInit(pVm,&sResult);
			/* May be a class instance and it's static method. Forward this call's named-arg map
			 * (pInstr->p3) so an FCC array callable invoked as `$c(name: …)` binds by name —
			 * mirroring the __invoke-object branch below. */
			rcArr = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);
			SySetReset(&aArg);
			/* Pop given arguments */
			if( nCallArgs > 0 ){
				VmPopOperand(&pTos,nCallArgs);
			}
			if( rcArr == PH7_ABORT ){
				PH7_MemObjRelease(&sResult);
				goto Abort;
			}
			if( rcArr == PH7_EXCEPTION ){
				/* An array callable ([$obj,'m']()) raised: resume after this frame's
				 * try if it caught the exception in-place, otherwise propagate. */
				sxi32 iResumePc;
				PH7_MemObjRelease(&sResult);
				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){
					PH7_MemObjRelease(pTos);
					/* Drain the abandoned outer-expression operands (`1 + $cb()`)
					 * to the try's base — one leaked slot per caught throw
					 * otherwise (ASan heap-buffer-overflow in a catch loop). */
					PH7_RESUME_DRAIN()
					pc = iResumePc;
					break;
				}
				goto Exception;
			}
			/* Copy result */
			PH7_MemObjStore(&sResult,pTos);
			PH7_MemObjRelease(&sResult);
		}else if( pTos->iFlags & MEMOBJ_OBJ ){
			ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;
			ph7_value sResult;
			sxi32 rcInv;
			/* __invoke object callable: the object slot isn't popped, so pArg is
			 * already this call's arg base — build the map + consume the runs. */
			pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,
				nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);
			/* D1: like the array-callable path above, __invoke dispatches through a shared
			 * helper that hides the target's by-ref flags here. Preserve the pre-D1
			 * over-vivification (so `$o(&$x)` out-params keep working) by materializing
			 * every deferred arg as by-ref. */
			{
				sxi32 rcDA = VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,/*bAllByRef*/1,0);
				if( rcDA == PH7_ABORT ){
					goto Abort;
				}else if( rcDA == PH7_EXCEPTION ){
					goto Exception;
				}
			}
			SySetReset(&aArg);
			while( pArg < pTos ){
				SySetPut(&aArg,(const void *)&pArg);
				pArg++;
			}
			PH7_MemObjInit(pVm,&sResult);
			rcInv = VmCallObjectInvoke(&(*pVm),pThis,
				(int)SySetUsed(&aArg),
				(ph7_value **)SySetBasePtr(&aArg),
				&sResult,
				pEffCallMap);
			SySetReset(&aArg);
			/* Pin pThis BEFORE popping operands: VmPopOperand releases the callable
			 * slot itself (it pops top-down and the callable IS pTos), which for a
			 * temporary like (new Plain())(...) holds the only reference — popping it
			 * would free pThis before VmRaiseNotCallable reads its class name below.
			 * Only the not-callable branch dereferences pThis afterwards, so pin just
			 * for that case; the matching PH7_ClassInstanceUnref drops it (destroying
			 * the temporary). The other branches let the pop free the temp as before. */
			if( rcInv == SXERR_INVALID ){
				pThis->iRef++;
			}
			if( nCallArgs > 0 ){
				VmPopOperand(&pTos,nCallArgs);
			}
			if( rcInv == SXERR_INVALID ){
				/* No __invoke: raise a catchable Error and route through try/catch.
				 * sResult was already released by VmCallObjectInvoke. */
				PH7_MemObjRelease(pTos);
				rc = VmRaiseNotCallable(&(*pVm),pThis);
				PH7_ClassInstanceUnref(pThis);
				if( rc == SXERR_ABORT ){
					goto Abort;
				}
				{
					sxi32 iRp;
					if( VmRecordedResume(pVm,&iRp,sState.pEntryFrame,aInstr) ){
						/* Drain the abandoned outer-expression operands
						 * (`1 + $plainObj()`) to the try's base — one leaked
						 * slot per caught throw otherwise. */
						PH7_RESUME_DRAIN()
						pc = iRp;
						break;
					}
				}
				goto Exception;
			}
			if( rcInv == PH7_ABORT ){
				PH7_MemObjRelease(&sResult);
				goto Abort;
			}
			if( rcInv == PH7_EXCEPTION ){
				/* __invoke raised. The catch body (if any) already ran in-place
				 * inside VmThrowException. If THIS frame's own try caught it,
				 * resume after the try/catch; otherwise propagate so the
				 * exception unwinds through intermediate frames with no handler. */
				sxi32 iResumePc;
				PH7_MemObjRelease(&sResult);
				if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){
					PH7_MemObjRelease(pTos);
					/* Drain the abandoned outer-expression operands (`1 + $inv()`)
					 * to the try's base — one leaked slot per caught throw
					 * otherwise (ASan heap-buffer-overflow in a catch loop). */
					PH7_RESUME_DRAIN()
					pc = iResumePc;
					break;
				}
				goto Exception;
			}
			PH7_MemObjStore(&sResult,pTos);
			PH7_MemObjRelease(&sResult);
		}else{
			/* php: calling a non-callable is a catchable Error naming the type
			 * ("Value of type int is not callable"), or -- for an array -- the shape it
			 * expected. PH7 printed "Invalid function name" and CONTINUED with NULL, so
			 * `$x()` on a number quietly evaluated to nothing. */
			sxi32 rcNc;
			char zMsg[128];
			if( pTos->iFlags & MEMOBJ_HASHMAP ){
				SyBufferFormat(zMsg,sizeof(zMsg),"Array callback must have exactly two elements");
			}else{
				SyBufferFormat(zMsg,sizeof(zMsg),"Value of type %s is not callable",
					VmArithTypeName(pTos));
			}
			/* Consume this call's captured spread runs — a non-callable target
			 * (int/float/bool/null) reaches no dispatch build site. */
			if( pInstr->iP2 ){
				VmSpreadConsume(pVm);
			}
			/* Pop given arguments */
			if( nCallArgs > 0 ){
				VmPopOperand(&pTos,nCallArgs);
			}
			/* Settle the call's result slot BEFORE throwing. */
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcNc = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));
			if( rcNc == SXERR_ABORT ){ goto Abort; }
			rc = rcNc;
			/* ENFORCE_RC never consulted pResumeFrame, so an in-place catch
			 * (SXRET_OK + recorded resume) FELL THROUGH and resumed the try body
			 * right after the failed call. Route like OP_THROW. */
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		break;
	}
	SyStringInitFromBuf(&sName,SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));
	/* php: a leading '\\' on a callable string name ("\\trim", "\\Foo\\bar") just
	 * anchors it to the global namespace — strip it before resolving so a
	 * dynamic call `$f()` / array_map('\\trim', …) finds the function. */
	if( sName.nByte > 0 && sName.zString[0] == '\\' ){
		sName.zString++;
		sName.nByte--;
	}
	/* Check for a compiled function first.
	 * Static names are already namespace-qualified by the compiler.
	 * Dynamic names (from variables) use exact match only, matching PHP behavior. */
	pEntry = SyHashGet(&pVm->hFunction,(const void *)sName.zString,sName.nByte);
	/* If the compiler qualified this call with a namespace, and the namespaced
	 * function is not found, retry with the global name (strip the namespace
	 * prefix up to the last backslash) before falling back to host functions.
	 * This mirrors PHP's lookup order for unqualified function calls inside
	 * namespaces. The namespace flag is stored in VmCallArgMap.bIsNamespaced. */
	{
	VmCallArgMap *pCallMap = pEffCallMap;
	if( pEntry == 0 && pCallMap && pCallMap->bIsNamespaced ){
		const char *zFunc;
		const char *zEnd;
		const char *z;
		SyString sGlobal;
		zFunc = sName.zString;
		zEnd  = zFunc + sName.nByte;
		z = zEnd;
		/* Find last namespace separator */
		while( z > zFunc ){
			if( z[-1] == '\\' ){
				break;
			}
			z--;
		}
		if( z > zFunc && z < zEnd ){
			/* Retry lookup using the unqualified/global function name */
			SyStringInitFromBuf(&sGlobal,z,(sxu32)(zEnd - z));
			pEntry = SyHashGet(&pVm->hFunction,(const void *)sGlobal.zString,sGlobal.nByte);
		}
	}
	} /* end VmCallArgMap namespace scope */
	if( pEntry ){
		ph7_vm_func_arg *aFormalArg;
		ph7_class_instance *pThis;
		ph7_value *pFrameStack;
		ph7_vm_func *pVmFunc;
		ph7_class *pSelf;
		ph7_class *pSelfHint;
		VmFrame *pFrame;
		ph7_value *pObj;
		VmSlot sArg;
		sxu32 n;
		int bClosureThis = 0;
		ph7_class *pClosureScope = 0;
		/* initialize fields */
		pVmFunc = (ph7_vm_func *)pEntry->pUserData;
		pThis = 0;
		pSelf = 0;
		/* A bound PLAIN closure stashed its $this in pVm->pClosureThis (VmClosureUnwrap); consume it
		 * here — transferring the owned reference to this frame's $this (teardown unrefs). Only ever
		 * set for a bound plain closure, which dispatches as a function, so the method branch below
		 * is skipped for it. The matching $__scope (private/protected visibility) rides alongside. */
		if( pVm->pClosureThis ){
			pThis = pVm->pClosureThis;
			pVm->pClosureThis = 0;
			bClosureThis = 1;
		}
		if( pVm->pClosureScope ){
			/* May ride alongside a bound $this, or stand alone for a
			 * scope-only rebind (`bindTo(null, Scope::class)`). */
			pClosureScope = pVm->pClosureScope;
			pVm->pClosureScope = 0;
		}
		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){
			ph7_class_method *pMeth;
			/* Class method call */
			ph7_value *pTarget = &pTos[-1];
			if( pTarget >= pStack && (pTarget->iFlags & (MEMOBJ_STRING|MEMOBJ_OBJ|MEMOBJ_NULL)) ){
				/* Extract the 'this' pointer */
				if(pTarget->iFlags & MEMOBJ_OBJ ){
					/* Instance already loaded */
					pThis = (ph7_class_instance *)pTarget->x.pOther;
					pThis->iRef++;
					pSelf = pThis->pClass;
				}
				if( pSelf == 0 ){
					if( (pTarget->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTarget->sBlob) > 0 ){
						/* "Late Static Binding" class name */
						pSelf = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTarget->sBlob),
							SyBlobLength(&pTarget->sBlob),FALSE,0);
					}
					if( pSelf == 0 ){
						pSelf = (ph7_class *)pVmFunc->pUserData;
					}
				}
				if( pThis == 0  ){
					VmFrame *pFrameLocal = pVm->pFrame;
					pFrameLocal = VmSkipExceptionFrames(pFrameLocal);
					if( pFrameLocal->pParent ){
						/* TICKET-1433-52: Make sure the '$this' variable is available to the current scope */
						pThis = pFrameLocal->pThis;
						if( pThis ){
							pThis->iRef++;
						}
					}
				}
				VmPopOperand(&pTos,1);
				PH7_MemObjRelease(pTos);
				/* Synchronize pointers. The method-name slot popped above sat BETWEEN
				 * this call's arguments and the (already-removed) target — so only now
				 * is pTos one past the last actual argument. Re-derive the unpack
				 * expansion against this corrected top: VmSpreadOwnExtra reconstructs
				 * from run ends, which the extra target slot would otherwise offset,
				 * undercounting a spread method's arguments (`$o->m(...$a, x: 1)`). */
				nCallArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(&(*pVm),pInstr->iP1,pTos) : 0);
				pArg = &pTos[-nCallArgs];
				/* TICKET 1433-50: This is a very very unlikely scenario that occurs when the 'genius'
				 * user have already computed the random generated unique class method name
				 * and tries to call it outside it's context [i.e: global scope]. In that
				 * case we have to synchronize pointers to avoid stack underflow.
				 */
				while( pArg < pStack ){
					pArg++;
				}
				if( pSelf && pVm->bReflectBypass ){
					/* ReflectionMethod::invoke()/getClosure(): PHP 8.1+ reflection
					 * ignores visibility. Consume-once so nested calls made by the
					 * invoked body are checked normally. */
					pVm->bReflectBypass = 0;
				}else
				if( pSelf ){ /* Paranoid edition */
					/* Check if the call is allowed. php binds non-public method
					 * access by the DECLARING class (pVmFunc->pUserData — the class
					 * the callee was compiled in), NOT the instance's class: an
					 * inherited base method calling $this->priv() on a child
					 * instance passes, a child's private SHADOW doesn't hijack the
					 * check for a parent callee, and the denial message names the
					 * declaring class like php. */
					ph7_class *pDeclClass = pVmFunc->pUserData ? (ph7_class *)pVmFunc->pUserData : pSelf;
					pMeth = PH7_ClassExtractMethod(pDeclClass,pVmFunc->sName.zString,pVmFunc->sName.nByte);
					if( pMeth == 0 && pDeclClass != pSelf ){
						pMeth = PH7_ClassExtractMethod(pSelf,pVmFunc->sName.zString,pVmFunc->sName.nByte);
					}
					if( pMeth && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){
						if( !PH7_VmClassMemberAccess(&(*pVm),pDeclClass,&pVmFunc->sName,pMeth->iProtection,FALSE) ){
							/* php throws a CATCHABLE Error here. The old code merely PRINTED an
							 * uncaught-exception report and aborted, so `try { $o->priv(); }
							 * catch (Error $e)` never caught it and the script died. */
							char zMsg[256];
							sxi32 rcVis;
							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";
							SyBufferFormat(zMsg,sizeof(zMsg),"Call to %s method %.*s::%.*s() from global scope",
								zVis,(int)pDeclClass->sName.nByte,pDeclClass->sName.zString,
								(int)pVmFunc->sName.nByte,pVmFunc->sName.zString);
							/* Consume this call's captured spread runs — this visibility
							 * error exits before the pVmFunc build below. */
							if( pInstr->iP2 ){
								VmSpreadConsume(pVm);
							}
							/* Pop given arguments, and leave the call's NULL result behind. */
							if( nCallArgs > 0 ){
								VmPopOperand(&pTos,nCallArgs);
							}
							PH7_MemObjRelease(pTos);
							MemObjSetType(pTos,MEMOBJ_NULL);
							pTos->nIdx = SXU32_HIGH;
							rcVis = VmThrowFromVm(&(*pVm),"Error",zMsg,(sxu32)SyStrlen(zMsg));
							if( rcVis == SXERR_ABORT ){ goto Abort; }
							rc = rcVis;
							/* ENFORCE_RC never consulted pResumeFrame, so an in-place
							 * catch (SXRET_OK + recorded resume) FELL THROUGH and
							 * dispatched the DENIED method anyway. Route like OP_THROW. */
							PH7_THROW_ROUTE_MIDEXPR(rc)
						}
					}
				}
			}
		}
		/* pArg is now finalized for every pVmFunc callee (a method call popped its
		 * method-name slot above; functions/closures/generators keep the top base).
		 * Build the PHP 8.1 effective spread-key map here so the generator and the
		 * install path below both see it — and so this call's captured runs are
		 * consumed exactly once, against the correct base. */
		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,
			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);
		/* Check the PHP call-depth cap (the sole site — BYTECODE.md stage 5).
		 * Default is unbounded (heap-bound recursion, decoupled from the C stack
		 * by the stage-2 trampoline); the C stack is guarded separately by
		 * nMaxNativeDepth. This fires only when an embedder configures a cap, and
		 * then raises a clean non-catchable fatal (was: silently set NULL and
		 * continue) and halts. */
		if( VmRecursionExceeded(pVm) ){
			/* Args and the function-name slot are released by the Abort label,
			 * which walks the whole operand stack — don't release them here. */
			VmRecursionFatal(&(*pVm));
			goto Abort;
		}
		if( pVmFunc->pNextName ){
			/* Function is candidate for overloading,select the appropriate function to call */
			pVmFunc = VmOverload(&(*pVm),pVmFunc,pArg,(int)(pTos-pArg));
		}
		/* Self class for a param type hint (resolves `self`/`parent` and qualifies the error's
		 * `Class::method`). Computed after overload resolution so it reflects the selected method.
		 * A normal method uses its DECLARING class (pVmFunc->pUserData), so an inherited `self`-typed
		 * param accepts a base instance — matching PHP. A TRAIT method shares one ph7_class_method
		 * whose pUserData is the TRAIT, but PHP resolves `self`/`parent` (and the message) to the
		 * USING class; we don't carry the using class on the shared struct, so fall back to the
		 * runtime class (pSelf) — correct when the trait is used directly (only slightly stricter for
		 * a trait used in a base class then called on a subclass). Free functions/closures → pSelf. */
		pSelfHint = pSelf;
		if( pVmFunc->iFlags & VM_FUNC_CLASS_METHOD ){
			ph7_class *pDecl = (ph7_class *)pVmFunc->pUserData;
			if( pDecl && (pDecl->iFlags & PH7_CLASS_TRAIT) == 0 ){
				pSelfHint = pDecl;
			}
		}
		if( pSelf == 0 && (pVmFunc->iFlags & VM_FUNC_CLOSURE) ){
			/* Push the closure's called-class as this frame's LSB class so
			 * `static::` inside the body resolves like php. An explicit
			 * Closure::bind/bindTo scope rebinds the called-class; otherwise use
			 * the class captured at the closure's creation site. self::/parent::
			 * still use the declaring-class scope (PH7_VmPeekDeclaringClass); done
			 * after pSelfHint so a `self`-typed param is unaffected. */
			if( pClosureScope ){
				pSelf = pClosureScope;
			}else if( pVmFunc->pLsbClass ){
				pSelf = (ph7_class *)pVmFunc->pLsbClass;
			}
		}
		if( SySetUsed(&pVmFunc->aAttrs) > 0 ){
			/* php 8.4 #[\Deprecated] runtime notice — once per call, before
			 * execution (generators: at the g(...) call site, like php). */
			VmDeprecatedAttrNotice(&(*pVm),pVmFunc,
				(pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) ? pSelfHint : 0);
		}
		/* D1: resolve deferred plain-var arguments against this callee's formal parameters
		 * now — BEFORE the generator split and VmEnterFrame, while pVm->pFrame is still the
		 * caller. pVmFunc is final here (post-overload). Covers plain functions, methods,
		 * closures, dynamic-name calls and generators uniformly. */
		{
			sxi32 rcDA = VmResolveDeferredArgs(&(*pVm),pArg,pTos,
				(ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs),SySetUsed(&pVmFunc->aArgs),
				0,0,0);
			if( rcDA == PH7_ABORT ){
				goto Abort;
			}else if( rcDA == PH7_EXCEPTION ){
				goto Exception;
			}
		}
		if( pVmFunc->iFlags & VM_FUNC_GENERATOR ){
			/* Generator function: return a Generator object instead of executing */
			ph7_exec_ctx *pExecCtx;
			ph7_generator *pGenerator;
			ph7_class_instance *pGenObj;
			ph7_value *pCtxAttr;
			SyString sAttrName;
			ph7_value **apCallArgs;
			int nGenArgs, iArg;
			/* Collect arguments from the operand stack */
			nGenArgs = (int)(pTos - pArg);
			apCallArgs = 0;
			if( nGenArgs > 0 ){
				apCallArgs = (ph7_value **)SyMemBackendAlloc(&pVm->sAllocator,
					nGenArgs * sizeof(ph7_value *));
				if( apCallArgs == 0 ){
					/* OOM: fall back to zero args rather than NULL-deref */
					nGenArgs = 0;
				}else{
					VmCallArgMap *pGenMap = pEffCallMap;
					int didReorder = 0;
					if( pGenMap && pGenMap->bHasNamed ){
						/* Named-argument reordering for generator */
						ph7_vm_func_arg *aFA = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);
						sxu32 nF = SySetUsed(&pVmFunc->aArgs);
						sxu32 nNV = nF;
						sxi32 iVIdx = -1;
						sxi32 *aGSlot;
						sxu8 *aGUsed;
						sxu32 gi;
						for( gi = 0; gi < nF; gi++ ){
							if( aFA[gi].iFlags & VM_FUNC_ARG_VARIADIC ){ nNV = gi; iVIdx = (sxi32)gi; break; }
						}
						aGSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,
							(sxu32)nGenArgs * sizeof(sxi32) + nNV * sizeof(sxu8));
						if( aGSlot ){
							aGUsed = (sxu8 *)&aGSlot[nGenArgs];
							rc = VmResolveNamedArgs(&(*pVm),pGenMap,aFA,nNV,iVIdx,
								(sxu32)nGenArgs,aGSlot,aGUsed);
							if( rc == PH7_ABORT ){
								SyMemBackendFree(&pVm->sAllocator, aGSlot);
								SyMemBackendFree(&pVm->sAllocator, apCallArgs);
								goto Abort;
							}
							if( rc == PH7_EXCEPTION ){
								/* A named-argument error is php's catchable \Error.
								 * No callee frame exists yet on this branch (the
								 * generator body never runs and VmEnterFrame is
								 * further down), so route it like the other
								 * pre-frame OP_CALL throws: drop the args + the
								 * function-name slot and land the enclosing try. */
								SyMemBackendFree(&pVm->sAllocator, aGSlot);
								SyMemBackendFree(&pVm->sAllocator, apCallArgs);
								PH7_INLINE_RESUME_BREAK()
								VmPopOperand(&pTos,nCallArgs + 1);
								{
									sxi32 iRpN;
									if( VmRecordedResume(pVm,&iRpN,sState.pEntryFrame,aInstr) ){
										pc = iRpN;
										break;
									}
								}
								goto Exception;
							}
							if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL|VM_FUNC_CLASS_METHOD)) != VM_FUNC_INTERNAL ){
								/* php's named-hole ArgumentCountError, checked BEFORE
								 * hole compaction: compacting first would report the
								 * positional wording with a fictitious count (g(b:2)
								 * must be `g(): Argument #1 ($a) not passed`, not
								 * "1 passed"). Implicit-required watermark, like the
								 * plain-call named path; a hole with NOTHING filled
								 * above it keeps php's count wording — fall through
								 * to VmFiberSetupFrame's check (the compacted count
								 * equals php's num_args there). */
								sxu32 nNVIgnored,nReqG,gHole,nMaxFilledG = 0;
								sxi32 iHole = -1;
								nReqG = VmFuncRequiredArgCount(pVmFunc,&nNVIgnored);
								for( gHole = 0; gHole < (sxu32)nGenArgs; gHole++ ){
									if( aGSlot[gHole] >= 0 && (sxu32)(aGSlot[gHole] + 1) > nMaxFilledG ){
										nMaxFilledG = (sxu32)(aGSlot[gHole] + 1);
									}
								}
								for( gHole = 0; gHole < nReqG && iHole < 0; gHole++ ){
									sxu32 gj;
									int bFound = 0;
									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){
										if( aGSlot[gj] == (sxi32)gHole ){ bFound = 1; break; }
									}
									if( !bFound && gHole + 1 <= nMaxFilledG ){
										iHole = (sxi32)gHole;
									}
								}
								if( iHole >= 0 ){
									rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,
										(sxu32)iHole+1,&aFA[iHole].sName);
									SyMemBackendFree(&pVm->sAllocator, aGSlot);
									SyMemBackendFree(&pVm->sAllocator, apCallArgs);
									if( rc == PH7_ABORT ){
										goto Abort;
									}
									/* Route like the VmFiberSetupFrame throw below */
									PH7_INLINE_RESUME_BREAK()
									VmPopOperand(&pTos,nCallArgs + 1);
									{
										sxi32 iRpH;
										if( VmRecordedResume(pVm,&iRpH,sState.pEntryFrame,aInstr) ){
											pc = iRpH;
											break;
										}
									}
									goto Exception;
								}
							}
							/* Build apCallArgs in formal-parameter order, then
							 * append overflow (variadic / positional beyond
							 * formals) so downstream sees every argument. */
							{
								int nOut = 0;
								for( gi = 0; gi < nNV; gi++ ){
									sxu32 gj;
									for( gj = 0; gj < (sxu32)nGenArgs; gj++ ){
										if( aGSlot[gj] == (sxi32)gi ){
											apCallArgs[nOut++] = &pArg[gj];
											break;
										}
									}
								}
								for( gi = 0; gi < (sxu32)nGenArgs; gi++ ){
									if( aGSlot[gi] == -1 || aGSlot[gi] == -2 ){
										apCallArgs[nOut++] = &pArg[gi];
									}
								}
								nGenArgs = nOut;
							}
							SyMemBackendFree(&pVm->sAllocator, aGSlot);
							didReorder = 1;
						}
						/* If aGSlot allocation failed, fall through to
						 * positional fill below — preserves arg order rather
						 * than passing an uninitialized apCallArgs. */
					}
					if( !didReorder ){
						for( iArg = 0; iArg < nGenArgs; iArg++ ){
							apCallArgs[iArg] = &pArg[iArg];
						}
					}
				}
			}
			/* Create execution context and generator wrapper */
			pExecCtx = VmNewExecCtx(pVm, pVmFunc);
			if( pExecCtx == 0 ){
				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);
				VmErrorFormat(&(*pVm), PH7_CTX_ERR,
					"Out of memory while creating generator for '%z'", &pVmFunc->sName);
				break;
			}
			pGenerator = VmNewGenerator(pVm, pExecCtx);
			if( pGenerator == 0 ){
				VmReleaseExecCtx(pVm, pExecCtx);
				if( apCallArgs ) SyMemBackendFree(&pVm->sAllocator, apCallArgs);
				VmErrorFormat(&(*pVm), PH7_CTX_ERR,
					"Out of memory while creating generator for '%z'", &pVmFunc->sName);
				break;
			}
			/* Set up the frame with arguments, closure env, $this */
			pExecCtx->pFrame->pParent = pVm->pFrame;
			pVm->pFrame = pExecCtx->pFrame;
			rc = VmFiberSetupFrame(pVm, pExecCtx, pThis, nGenArgs, apCallArgs,
				(pEffCallMap && pEffCallMap->bStrict) ? 1 : 0, pSelfHint,
				TRUE/*generator: the g(...) call site is in the message*/);
			pVm->pFrame = pExecCtx->pFrame->pParent;
			pExecCtx->pFrame->pParent = 0;
			if( apCallArgs ){
				SyMemBackendFree(&pVm->sAllocator, apCallArgs);
			}
			if( rc != SXRET_OK ){
				VmReleaseGenerator(pVm, pGenerator);
				if( pThis ){
					PH7_ClassInstanceUnref(pThis);
				}
				if( rc == SXERR_ABORT ){
					goto Abort;
				}
				if( rc == PH7_EXCEPTION ){
					/* A declared-type TypeError thrown while binding the
					 * generator's arguments (php binds + type-checks eagerly at
					 * the g(...) call site, before any resume — band A #2). If
					 * an inline try THIS exec owns caught it, land at its
					 * redirect (the drain subsumes the operand pops); else pop
					 * the args + function name and route like the other
					 * OP_CALL throw paths. */
					PH7_INLINE_RESUME_BREAK()
					VmPopOperand(&pTos,nCallArgs + 1);
					{
						sxi32 iRpG;
						if( VmRecordedResume(pVm,&iRpG,sState.pEntryFrame,aInstr) ){
							pc = iRpG;
							break;
						}
					}
					goto Exception;
				}
				break;
			}
			/* Create Generator class instance */
			pGenObj = PH7_NewClassInstance(pVm, pVm->pGeneratorClass);
			if( pGenObj == 0 ){
				VmReleaseGenerator(pVm, pGenerator);
				break;
			}
			/* Store generator in __ctx attribute */
			SyStringInitFromBuf(&sAttrName, "__ctx", 5);
			pCtxAttr = PH7_ClassInstanceFetchAttr(pGenObj, &sAttrName);
			if( pCtxAttr ){
				pCtxAttr->x.pOther = pGenerator;
				MemObjSetType(pCtxAttr, MEMOBJ_RES);
			}
			/* Pop args and function name, push Generator object. PH7_NewClassInstance
			 * already returns iRef==1, held by this stack value (mirrors OP_NEW) — do
			 * NOT bump again, or the object never unrefs to 0 on unset / out-of-scope
			 * and its __destruct (which runs pending `finally` blocks and frees the
			 * exec context) never fires. */
			PH7_MemObjRelease(pTos);
			pTos = &pTos[-nCallArgs];
			pTos->x.pOther = pGenObj;
			MemObjSetType(pTos, MEMOBJ_OBJ);
			if( pThis ){
				PH7_ClassInstanceUnref(pThis);
			}
			break;
		}
		/* Extract the formal argument set */
		aFormalArg = (ph7_vm_func_arg *)SySetBasePtr(&pVmFunc->aArgs);
		/* Create a new VM frame  */
		rc = VmEnterFrame(&(*pVm),pVmFunc,pThis,&pFrame);
		if( rc != SXRET_OK ){
			/* Raise exception: Out of memory */
			VmErrorFormat(&(*pVm),PH7_CTX_ERR,
				"PH7 is running out of memory while calling function '%z',NULL will be returned",
				&pVmFunc->sName);
			/* The frame that would own (and later release) $this never got created; for a bound
			 * plain closure the consumed transient is the object's ONLY ref, so release it here
			 * to avoid a leak (a method call's pThis stays owned by the receiver on the stack). */
			if( bClosureThis && pThis ){
				PH7_ClassInstanceUnref(pThis);
			}
			/* Pop given arguments */
			if( nCallArgs > 0 ){
				VmPopOperand(&pTos,nCallArgs);
			}
			/* Assume a null return value so that the program continue it's execution normally */
			PH7_MemObjRelease(pTos);
			break;
		}
		if( pClosureScope ){
			/* Plain closure with an explicit scope — bound (bindTo($o, Scope::class) /
			 * call($o)) or scope-only (bindTo(null, Scope::class)): private/protected
			 * access inside the body resolves against it. */
			pFrame->pBoundScope = pClosureScope;
		}
		/* Stamp the ACTUAL call arity for func_num_args()/func_get_args() (band
		 * A #4): sArg over-counts (defaulted params installed, variadic packed
		 * as one entry) so php's answers can't be derived from it. */
		pFrame->nActualArgs = (int)(pTos - pArg);
		if( pThis && ((pVmFunc->iFlags & VM_FUNC_CLASS_METHOD) || bClosureThis) ){
			/* Install the '$this' variable (a method call, or a bound plain closure — Increment 2). */
			static const SyString sThis = { "this" , sizeof("this") - 1 };
			pObj = VmExtractMemObj(&(*pVm),&sThis,FALSE,TRUE);
			if( pObj ){
				/* Reflect the change */
				pObj->x.pOther = pThis;
				MemObjSetType(pObj,MEMOBJ_OBJ);
			}
		}
		if( SySetUsed(&pVmFunc->aStatic) > 0 ){
			ph7_vm_func_static_var *pStatic,*aStatic;
			/* Install static variables */
			aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pVmFunc->aStatic);
			for( n = 0 ; n < SySetUsed(&pVmFunc->aStatic) ; ++n ){
				pStatic = &aStatic[n];
				if( pStatic->nIdx == SXU32_HIGH ){
					/* Initialize the static variables */
					pObj = VmReserveMemObj(&(*pVm),&pStatic->nIdx);
					if( pObj ){
						/* Assume a NULL initialization value */
						PH7_MemObjInit(&(*pVm),pObj);
						if( SySetUsed(&pStatic->aByteCode) > 0 ){
							/* Evaluate initialization expression (Any complex expression) */
							VmLocalExec(&(*pVm),&pStatic->aByteCode,pObj,FALSE);
						}
						pObj->nIdx = pStatic->nIdx;
					}else{
						continue;
					}
				}
				/* Install in the current frame */
				SyHashInsert(&pFrame->hVar,SyStringData(&pStatic->sName),SyStringLength(&pStatic->sName),
					SX_INT_TO_PTR(pStatic->nIdx));
			}
		}
		/* Push arguments in the local frame */
		{
		VmCallArgMap *pCallMap3 = pEffCallMap;
		/* Caller file's strict_types mode — governs parameter coercion
		 * (but NOT return coercion, which uses the callee's file). */
		int bCallIsStrict = (pCallMap3 && pCallMap3->bStrict) ? 1 : 0;
		if( pCallMap3 && pCallMap3->bHasNamed ){
			/* ============================================================
			 * Named-argument matching path (PHP 8.0)
			 *
			 * Resolve each actual argument to its formal parameter by name
			 * or position, then install them in the frame.
			 * ============================================================ */
			sxu32 nFormal = SySetUsed(&pVmFunc->aArgs);
			sxu32 nActual = (sxu32)(pTos - pArg);
			sxi32 iVariadicIdx = -1;
			sxu32 nNonVariadic;
			sxi32 *aSlot;
			sxu8  *aUsed;
			sxu32 i;
			/* Find variadic parameter index */
			for( i = 0; i < nFormal; i++ ){
				if( aFormalArg[i].iFlags & VM_FUNC_ARG_VARIADIC ){
					iVariadicIdx = (sxi32)i;
					break;
				}
			}
			nNonVariadic = iVariadicIdx >= 0 ? (sxu32)iVariadicIdx : nFormal;
			/* Allocate mapping arrays */
			aSlot = (sxi32 *)SyMemBackendAlloc(&pVm->sAllocator,
				nActual * sizeof(sxi32) + nNonVariadic * sizeof(sxu8));
			if( aSlot == 0 ){
				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Out of memory during named argument resolution");
				goto Abort;
			}
			aUsed = (sxu8 *)&aSlot[nActual];
			/* Resolve named arguments to formal parameters */
			rc = VmResolveNamedArgs(&(*pVm),pCallMap3,aFormalArg,
				nNonVariadic,iVariadicIdx,nActual,aSlot,aUsed);
			if( rc == PH7_ABORT ){
				SyMemBackendFree(&pVm->sAllocator, aSlot);
				goto Abort;
			}
			if( rc == PH7_EXCEPTION ){
				/* php's catchable \Error for a bad named argument. The callee
				 * frame is already entered but its body must not run: unwind
				 * exactly like the named-hole ArgumentCountError path below
				 * (release the not-yet-installed actuals — Pass 2's release
				 * loop has not run — pop the callee slot, mark that there is no
				 * callee operand stack, and let VmCallFinish route the throw). */
				sxu32 iRel;
				SyMemBackendFree(&pVm->sAllocator, aSlot);
				for( iRel = 0; iRel < nActual; iRel++ ){
					PH7_MemObjRelease(&pArg[iRel]);
				}
				PH7_MemObjRelease(pTos);
				pTos = &pTos[-nCallArgs];
				pFrameStack = 0;
				goto SkipFuncBody;
			}
			/* Pass 2: install arguments into the frame by formal parameter order */
			{
			/* php's required watermark for the hole check below (0 disables it
			 * for hosted builtin FUNCTIONS, which self-manage — hosted-class
			 * methods and all user code get php's named-hole error), plus the
			 * highest formal slot an actual resolved to: php words a hole
			 * BELOW a filled slot `Argument #N ($x) not passed`, but a hole
			 * with nothing filled above it gets the positional count message
			 * (zend's RECV arg_num > EX(num_args) distinction). */
			sxu32 nReqNamed = 0;
			sxu32 nNVNamed = 0;
			sxu32 nMaxFilled = 0;
			if( (pVmFunc->iFlags & (VM_FUNC_INTERNAL|VM_FUNC_CLASS_METHOD)) != VM_FUNC_INTERNAL ){
				nReqNamed = VmFuncRequiredArgCount(pVmFunc,&nNVNamed);
				for( i = 0; i < nActual; i++ ){
					if( aSlot[i] >= 0 && (sxu32)(aSlot[i] + 1) > nMaxFilled ){
						nMaxFilled = (sxu32)(aSlot[i] + 1);
					}
				}
			}
			for( n = 0; n < nNonVariadic; n++ ){
				/* Find the stack arg mapped to formal n */
				sxi32 iSrc = -1;
				for( i = 0; i < nActual; i++ ){
					if( aSlot[i] == (sxi32)n ){
						iSrc = (sxi32)i;
						break;
					}
				}
				if( iSrc >= 0 ){
					/* Argument was provided — install with type checking */
					ph7_value *pVal = &pArg[iSrc];
					/* An explicit null is NOT redirected to the default: PHP applies a
					 * default only for an OMITTED argument. An explicit null falls through
					 * to the type check below (TypeError for a non-nullable typed param,
					 * kept as null for a typeless one). An implicitly-nullable `Type $x =
					 * null` param carries VM_FUNC_ARG_NULLABLE, so the check accepts null. */
					/* Type checking (union / class / pseudo / scalar, with weak-mode
					 * coercion and whole-real materialization in place): the shared
					 * per-argument helper — one implementation for both OP_CALL
					 * paths and the generator/fiber binder (§7.1(f) fold). */
					rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pVal,bCallIsStrict,pSelfHint);
					if( rc != SXRET_OK ){
						if( rc == PH7_ABORT ) goto Abort;
						SyMemBackendFree(&pVm->sAllocator, aSlot);
						PH7_MemObjRelease(pTos);
						pTos = &pTos[-nCallArgs];
						pFrameStack = 0;
						rc = PH7_EXCEPTION;
						goto SkipFuncBody;
					}
					/* Install: by reference or by value */
					if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){
						if( pVal->nIdx == pVm->nGlobalIdx && pVal->nIdx != SXU32_HIGH ){
							/* php 8.1: $GLOBALS cannot be passed by reference */
							SyBlob sMsg;
							SyBlobInit(&sMsg,&pVm->sAllocator);
							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",
								&pVmFunc->sName,n+1,&aFormalArg[n].sName);
							rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);
							if( rc == PH7_ABORT ){
								goto Abort;
							}
							SyMemBackendFree(&pVm->sAllocator, aSlot);
							PH7_MemObjRelease(pTos);
							pTos = &pTos[-nCallArgs];
							pFrameStack = 0;
							rc = PH7_EXCEPTION;
							goto SkipFuncBody;
						}
						if( pVal->nIdx == SXU32_HIGH ){
							if( (pVal->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES|MEMOBJ_NULL)) == 0
							 && (pVal->iFlags & MEMOBJ_AUX_CUFVAL) == 0 ){
								/* A non-lvalue bound to a by-ref parameter is a catchable Error in
								 * php — f(5) where f(&$x). PH7 only warned and quietly passed by
								 * value, so the call ran with a copy and the caller never knew.
								 * The one legitimate copy is call_user_func()'s (MEMOBJ_AUX_CUFVAL),
								 * which php also permits, with its own warning. */
								SyBlob sMsg;
								sxi32 rcRef;
								SyBlobInit(&sMsg,&pVm->sAllocator);
								SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",
									&pVmFunc->sName,n+1,&aFormalArg[n].sName);
								rcRef = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),
									SyBlobLength(&sMsg));
								SyBlobRelease(&sMsg);
								if( rcRef == SXERR_ABORT ){
									pFrameStack = 0;
									rc = PH7_ABORT;
									goto SkipFuncBody;
								}
								pFrameStack = 0;
								rc = PH7_EXCEPTION;
								goto SkipFuncBody;
							}
							pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
						}else{
							SyHashEntry *pRefEntry = SyHashGet(&pFrame->hVar,
								SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));
							if( pRefEntry == 0 ){
								SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),
									SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pVal->nIdx));
								sArg.nIdx = pVal->nIdx;
								sArg.pUserData = 0;
								SySetPut(&pFrame->sArg,(const void *)&sArg);
							}
							pObj = 0;
						}
					}else{
						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
					}
					if( pObj ){
						PH7_MemObjStore(pVal,pObj);
						sArg.nIdx = pObj->nIdx;
						sArg.pUserData = 0;
						SySetPut(&pFrame->sArg,(const void *)&sArg);
					}
				}else{
					/* Argument was NOT provided — use default or leave unset */
					if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){
						/* Should not reach here; variadic handled separately below */
					}else if( n < nReqNamed ){
						/* php's implicit-required rule applies to named calls
						 * too: a hole below the required watermark throws even
						 * when the formal carries a default — f($a,$b=2,$c)
						 * called as f(a:1,c:3) is `Argument #2 ($b) not
						 * passed` (a hole with NO default is always below the
						 * watermark, so this subsumes the no-default case).
						 * A hole with nothing filled ABOVE it uses php's
						 * positional count wording instead. The passed stack
						 * args were not released yet on this path (that loop
						 * runs after Pass 2) — release them before the exit. */
						if( n + 1 > nMaxFilled ){
							rc = (pVmFunc->iFlags & VM_FUNC_INTERNAL)
								? VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,
									nMaxFilled,nReqNamed,nNVNamed)
								: VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,
									nMaxFilled,nReqNamed,nNVNamed,TRUE);
						}else{
							rc = VmThrowArgNotPassed(&(*pVm),pSelfHint,&pVmFunc->sName,n+1,&aFormalArg[n].sName);
						}
						SyMemBackendFree(&pVm->sAllocator, aSlot);
						for( i = 0; i < nActual; i++ ){
							PH7_MemObjRelease(&pArg[i]);
						}
						if( rc == PH7_ABORT ){
							goto Abort;
						}
						PH7_MemObjRelease(pTos);
						pTos = &pTos[-nCallArgs];
						pFrameStack = 0;
						rc = PH7_EXCEPTION;
						goto SkipFuncBody;
					}else if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){
						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
						if( pObj ){
							rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);
							if( rc == PH7_ABORT ) goto Abort;
							sArg.nIdx = pObj->nIdx;
							sArg.pUserData = 0;
							SySetPut(&pFrame->sArg,(const void *)&sArg);
							/* A null default on an implicitly-nullable param must stay null
							 * (see the positional-path note above). */
							if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ
								&& (pObj->iFlags & aFormalArg[n].nType) == 0
								&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){
								ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);
								if( xCast ) xCast(pObj);
							}else{
								/* Mask matched — a const-indirected whole-real default
								 * (`int $x = FOO` with FOO = 2.0) materializes as int. */
								VmMaterializeIntTyped(pObj,aFormalArg[n].nType);
							}
						}
					}
				}
			}
			} /* end nReqNamed scope */
			/* Handle variadic parameter */
			if( iVariadicIdx >= 0 ){
				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[iVariadicIdx].sName,FALSE,TRUE);
				if( pObj ){
					/* Capture the slot index now: PH7_HashmapInsert below can
					 * PH7_ReserveMemObj, reallocating pVm->aMemObj and dangling pObj
					 * (same latent UAF the positional path guards against). */
					sxu32 nVariadicSlot;
					PH7_MemObjToHashmap(pObj);
					nVariadicSlot = pObj->nIdx;
					{
						ph7_hashmap *pVarMap = (ph7_hashmap *)pObj->x.pOther;
						/* php numbers a failing NAMED variadic element as
						 * max(total positional args, declared non-variadic
						 * formals) + 1 — zend's RECV slots always count —
						 * whichever named element fails; a POSITIONAL element
						 * uses its own 1-based call position. */
						sxu32 nPositional = 0;
						for( i = 0; i < nActual; i++ ){
							if( !(i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0) ){
								nPositional++;
							}
						}
						for( i = 0; i < nActual; i++ ){
							if( aSlot[i] == -1 ){
								int bNamed = (i < pCallMap3->nTotal && pCallMap3->aNames[i].nByte > 0);
								/* Same per-element type check + weak coercion as the
								 * positional-only path (shared helper; no `($name)`). */
								rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,
									&aFormalArg[iVariadicIdx],&pArg[i],
									bNamed ? SXMAX(nPositional,nNonVariadic) + 1 : i + 1,bCallIsStrict);
								if( rc != SXRET_OK ){
									if( rc == PH7_ABORT ){
										goto Abort;
									}
									SyMemBackendFree(&pVm->sAllocator, aSlot);
									PH7_MemObjRelease(pTos);
									pTos = &pTos[-nCallArgs];
									pFrameStack = 0;
									rc = PH7_EXCEPTION;
									goto SkipFuncBody;
								}
								if( bNamed ){
									/* Named variadic entry: insert with string key */
									ph7_value sKey;
									PH7_MemObjInit(pVm, &sKey);
									PH7_MemObjStringAppend(&sKey,
										pCallMap3->aNames[i].zString,
										(sxu32)pCallMap3->aNames[i].nByte);
									PH7_HashmapInsert(pVarMap, &sKey, &pArg[i]);
									PH7_MemObjRelease(&sKey);
								}else{
									/* Positional variadic entry */
									PH7_HashmapInsert(pVarMap, 0, &pArg[i]);
								}
							}
						}
					}
					sArg.nIdx = nVariadicSlot; /* pObj may be stale here (aMemObj realloc) */
					sArg.pUserData = 0;
					SySetPut(&pFrame->sArg,(const void *)&sArg);
				}
			}else{
				/* No variadic — preserve unresolved positional overflow
				 * (aSlot[i] == -2) as anonymous frame args so
				 * func_get_args() / func_num_args() still see them, matching
				 * the positional-only path's behavior. */
				sxu32 nAnon = nNonVariadic;
				for( i = 0; i < nActual; i++ ){
					if( aSlot[i] == -2 ){
						char zAnonBuf[32];
						SyString sAnonName;
						sAnonName.nByte = SyBufferFormat(zAnonBuf,sizeof(zAnonBuf),
							"[%u]apArg",nAnon);
						sAnonName.zString = zAnonBuf;
						pObj = VmExtractMemObj(&(*pVm),&sAnonName,TRUE,TRUE);
						if( pObj ){
							PH7_MemObjStore(&pArg[i],pObj);
							sArg.nIdx = pObj->nIdx;
							sArg.pUserData = 0;
							SySetPut(&pFrame->sArg,(const void *)&sArg);
						}
						nAnon++;
					}
				}
			}
			/* Release all stack arguments */
			for( i = 0; i < nActual; i++ ){
				PH7_MemObjRelease(&pArg[i]);
			}
			SyMemBackendFree(&pVm->sAllocator, aSlot);
			/* Set n to nFormal so the defaults loop below is skipped */
			n = nFormal;
		}else{
		/* ============================================================
		 * Positional-only matching path (original)
		 * ============================================================ */
		/* Base of the actual argument stack, for php's per-ELEMENT argument
		 * number in a variadic type-error (php numbers a variadic-collected
		 * element by its overall 1-based call position, not the formal index). */
		ph7_value *pArgBase = pArg;
		n = 0;
		while( pArg < pTos ){
			if( n < SySetUsed(&pVmFunc->aArgs) && (aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC) ){
				/* Variadic parameter: collect all remaining args into an array */
				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
				if( pObj ){
					/* Capture the slot index now: PH7_HashmapInsert below can PH7_ReserveMemObj,
					 * reallocating pVm->aMemObj and dangling pObj — so don't read pObj->nIdx after
					 * the packing loop (pre-existing UAF, masked by the pool allocator). pMap is a
					 * separately-allocated hashmap and stays valid across the realloc. */
					sxu32 nVariadicIdx;
					/* Initialize as empty array */
					PH7_MemObjToHashmap(pObj);
					nVariadicIdx = pObj->nIdx;
					{
						ph7_hashmap *pMap = (ph7_hashmap *)pObj->x.pOther;
						while( pArg < pTos ){
							/* Per-element type check + weak coercion (shared helper,
							 * also used by the named-argument path). The argument
							 * number is the element's overall 1-based call position
							 * ((pArg - pArgBase) + 1) and, like php, the `($name)`
							 * clause is omitted. */
							rc = VmVariadicElementTypeCheck(&(*pVm),pSelfHint,pVmFunc,
								&aFormalArg[n],pArg,(sxu32)(pArg - pArgBase) + 1,bCallIsStrict);
							if( rc != SXRET_OK ){
								if( rc == PH7_ABORT ){
									goto Abort;
								}
								/* Skip function body, route through normal cleanup */
								PH7_MemObjRelease(pTos);
								pTos = &pTos[-nCallArgs];
								pFrameStack = 0;
								rc = PH7_EXCEPTION;
								goto SkipFuncBody;
							}
							PH7_HashmapInsert(pMap, 0, pArg);
							pArg++;
						}
					}
					sArg.nIdx = nVariadicIdx; /* pObj may be stale here (aMemObj realloc) — use the saved index */
					sArg.pUserData = 0;
					SySetPut(&pFrame->sArg,(const void *)&sArg);
				}
				break; /* All remaining args consumed */
			}
			if( n < SySetUsed(&pVmFunc->aArgs) ){
				/* An explicit null is NOT redirected to the default (PHP applies a
				 * default only for an omitted arg); it falls through to the type check
				 * below — TypeError for a non-nullable typed param, kept as null for a
				 * typeless one. A `Type $x = null` param is flagged VM_FUNC_ARG_NULLABLE
				 * at compile time so its check accepts null. */
				/* Type checking (union / class / pseudo / scalar, with weak-mode
				 * coercion and whole-real materialization in place — nullable
				 * types (?type) let null through): the shared per-argument
				 * helper, one implementation for both OP_CALL paths and the
				 * generator/fiber binder (§7.1(f) fold). */
				rc = VmEnforceArgType(&(*pVm),pVmFunc,&aFormalArg[n],n+1,pArg,bCallIsStrict,pSelfHint);
				if( rc != SXRET_OK ){
					if( rc == PH7_ABORT ){
						goto Abort;
					}
					/* Skip function body, route through normal cleanup */
					PH7_MemObjRelease(pTos);
					pTos = &pTos[-nCallArgs];
					pFrameStack = 0;
					rc = PH7_EXCEPTION;
					goto SkipFuncBody;
				}
				if( aFormalArg[n].iFlags & VM_FUNC_ARG_BY_REF ){
					/* Pass by reference */
					if( pArg->nIdx == pVm->nGlobalIdx && pArg->nIdx != SXU32_HIGH ){
						/* php 8.1: $GLOBALS cannot be passed by reference —
						 * a catchable Error with php's exact wording. */
						SyBlob sMsg;
						SyBlobInit(&sMsg,&pVm->sAllocator);
						SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",
							&pVmFunc->sName,n+1,&aFormalArg[n].sName);
						rc = VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sMsg);
						if( rc == PH7_ABORT ){
							goto Abort;
						}
						PH7_MemObjRelease(pTos);
						pTos = &pTos[-nCallArgs];
						pFrameStack = 0;
						rc = PH7_EXCEPTION;
						goto SkipFuncBody;
					}
					if( pArg->nIdx == SXU32_HIGH ){
						if((pArg->iFlags & (MEMOBJ_HASHMAP|MEMOBJ_OBJ|MEMOBJ_RES|MEMOBJ_NULL)) == 0
						 && (pArg->iFlags & MEMOBJ_AUX_CUFVAL) == 0 ){
							/* php: a non-lvalue bound to a by-ref parameter is a catchable Error.
							 * PH7 warned and silently passed by value (same site as the other
							 * binder above). call_user_func()'s deliberate copy is exempt. */
							SyBlob sMsg;
							sxi32 rcRef;
							SyBlobInit(&sMsg,&pVm->sAllocator);
							SyBlobFormat(&sMsg,"%z(): Argument #%d ($%z) could not be passed by reference",
								&pVmFunc->sName,n+1,&aFormalArg[n].sName);
							rcRef = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),
								SyBlobLength(&sMsg));
							SyBlobRelease(&sMsg);
							return (rcRef == SXERR_ABORT) ? PH7_ABORT : PH7_EXCEPTION;
						}
						/* Switch to pass by value */
						pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
					}else{
						SyHashEntry *pRefEntry;
						/* Install the referenced variable in the private function frame */
						pRefEntry = SyHashGet(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),SyStringLength(&aFormalArg[n].sName));
						if( pRefEntry == 0 ){
							SyHashInsert(&pFrame->hVar,SyStringData(&aFormalArg[n].sName),
								SyStringLength(&aFormalArg[n].sName),SX_INT_TO_PTR(pArg->nIdx));
							sArg.nIdx = pArg->nIdx;
							sArg.pUserData = 0;
							SySetPut(&pFrame->sArg,(const void *)&sArg);
						}
						pObj = 0;
					}
				}else{
					/* Pass by value,make a copy of the given argument */
					pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
				}
			}else{
				char zName[32];
				SyString sArgName;
				/* Set a dummy name */
				sArgName.nByte = SyBufferFormat(zName,sizeof(zName),"[%u]apArg",n);
				sArgName.zString = zName;
				/* Annonymous argument */
				pObj = VmExtractMemObj(&(*pVm),&sArgName,TRUE,TRUE);
			}
			if( pObj ){
				PH7_MemObjStore(pArg,pObj);
				/* Insert argument index  */
				sArg.nIdx = pObj->nIdx;
				sArg.pUserData = 0;
				SySetPut(&pFrame->sArg,(const void *)&sArg);
			}
			PH7_MemObjRelease(pArg);
			pArg++;
			++n;
		}
		} /* end named vs positional branch */
		/* Set up closure environment */
		if( pVmFunc->iFlags & VM_FUNC_CLOSURE ){
			ph7_vm_func_closure_env *aEnv,*pEnv;
			ph7_value *pValue;
			sxu32 iEnv;
			aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pVmFunc->aClosureEnv);
			for(iEnv = 0 ; iEnv < SySetUsed(&pVmFunc->aClosureEnv) ; ++iEnv ){
				pEnv = &aEnv[iEnv];
				if( (pEnv->iFlags & VM_FUNC_ARG_IGNORE) && (pEnv->sValue.iFlags & MEMOBJ_NULL) ){
					/* Do not install null value */
					continue;
				}
				if( bClosureThis && SyStringLength(&pEnv->sName) == sizeof("this")-1
				 && SyMemcmp(SyStringData(&pEnv->sName),"this",sizeof("this")-1) == 0 ){
					/* The Closure instance carries an explicit bound $this
					 * (bindTo/bind/call): it wins over the creation-time
					 * captured $this, php-exact. */
					continue;
				}
				if( (pEnv->iFlags & VM_FUNC_ARG_BY_REF) && pEnv->nIdx != SXU32_HIGH ){
					/* Captured by reference: link the name to the shared slot
					 * (no copy), mirroring the by-ref argument install above. */
					if( SyHashGet(&pFrame->hVar,SyStringData(&pEnv->sName),SyStringLength(&pEnv->sName)) == 0 ){
						SyHashInsert(&pFrame->hVar,SyStringData(&pEnv->sName),
							SyStringLength(&pEnv->sName),SX_INT_TO_PTR(pEnv->nIdx));
					}
					continue;
				}
				pValue = VmExtractMemObj(pVm,&pEnv->sName,FALSE,TRUE);
				if( pValue == 0 ){
					continue;
				}
				/* Invalidate any prior representation */
				PH7_MemObjRelease(pValue);
				/* Duplicate bound variable value */
				PH7_MemObjStore(&pEnv->sValue,pValue);
			}
		}
		/* Too-few-arguments check, placed AFTER the passed arguments were
		 * installed and type-checked: php's RECV order means a type error on
		 * a PASSED argument beats the count error (`f(int $x,$y)` called
		 * f("str") is a TypeError, not ArgumentCountError). The passed args
		 * were already released by the install loop, so the standard throw
		 * exit leaks nothing. The named path never fires this (its per-hole
		 * check ran in-loop; n == nNonVariadic >= nRequired here). Hosted
		 * builtin FUNCTIONS (VM_FUNC_INTERNAL) are exempt — their PHL
		 * signatures don't always mirror php's true arity and their in-body
		 * self-checks own php's wording (stage-2 family); hosted-class
		 * METHODS get php's ZPP wording via VmThrowBuiltinTooFewArgs. */
		if( n < SySetUsed(&pVmFunc->aArgs)
		 && (pVmFunc->iFlags & (VM_FUNC_INTERNAL|VM_FUNC_CLASS_METHOD)) != VM_FUNC_INTERNAL ){
			sxu32 nNonVar,nReq;
			nReq = VmFuncRequiredArgCount(pVmFunc,&nNonVar);
			if( n < nReq ){
				sxu32 nPassed = pFrame->nActualArgs >= 0 ? (sxu32)pFrame->nActualArgs : n;
				if( pVmFunc->iFlags & VM_FUNC_INTERNAL ){
					rc = VmThrowBuiltinTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,
						nPassed,nReq,nNonVar);
				}else{
					rc = VmThrowTooFewArgs(&(*pVm),pSelfHint,&pVmFunc->sName,
						nPassed,nReq,nNonVar,TRUE);
				}
				if( rc == PH7_ABORT ){
					goto Abort;
				}
				PH7_MemObjRelease(pTos);
				pTos = &pTos[-nCallArgs];
				pFrameStack = 0;
				rc = PH7_EXCEPTION;
				goto SkipFuncBody;
			}
		}
		/* Process default values for remaining formal parameters */
		while( n < SySetUsed(&pVmFunc->aArgs) ){
			if( aFormalArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){
				/* Variadic parameter with no extra args — create empty array */
				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
				if( pObj ){
					PH7_MemObjToHashmap(pObj);
					sArg.nIdx = pObj->nIdx;
					sArg.pUserData = 0;
					SySetPut(&pFrame->sArg,(const void *)&sArg);
				}
				n++;
				break; /* Variadic is always last */
			}
			if( SySetUsed(&aFormalArg[n].aByteCode) > 0 ){
				pObj = VmExtractMemObj(&(*pVm),&aFormalArg[n].sName,FALSE,TRUE);
				if( pObj ){
					/* Evaluate the default value and extract it's result */
					rc = VmLocalExec(&(*pVm),&aFormalArg[n].aByteCode,pObj,FALSE);
					if( rc == PH7_ABORT ){
						goto Abort;
					}
					/* Insert argument index */
					sArg.nIdx = pObj->nIdx;
					sArg.pUserData = 0;
					SySetPut(&pFrame->sArg,(const void *)&sArg);
					/* Make sure the default argument is of the correct type.
					 * A null default on an implicitly-nullable param (`int $x = null`)
					 * must stay null — casting it to 0/""/false would diverge from PHP
					 * and contradict the explicit-null path, which now keeps it null. */
					if( aFormalArg[n].nType > 0 && aFormalArg[n].nType != MEMOBJ_OBJ
						&& ((pObj->iFlags & aFormalArg[n].nType) == 0)
						&& !((aFormalArg[n].iFlags & VM_FUNC_ARG_NULLABLE) && (pObj->iFlags & MEMOBJ_NULL)) ){
						ProcMemObjCast xCast = PH7_MemObjCastMethod(aFormalArg[n].nType);
						/* Cast to the desired type */
						xCast(pObj);
					}else{
						/* Mask matched — a const-indirected whole-real default
						 * (`int $x = FOO` with FOO = 2.0) materializes as int. */
						VmMaterializeIntTyped(pObj,aFormalArg[n].nType);
					}
				}
			}
			++n;
		}
		} /* end VmCallArgMap scope */
		/* Pop arguments,function name from the operand stack and assume the function
		 * does not return anything.
		 */
		PH7_MemObjRelease(pTos);
		pTos = &pTos[-nCallArgs];
		/* Allocate an operand stack (via the recycling allocator) and evaluate the
		 * function body. Size it to a tight static bound when the body is statically
		 * modelable (BYTECODE.md stage 7) — the big memory win for deep recursion,
		 * where one such stack lives per frame — falling back to the safe
		 * instruction-count bound otherwise.
		 *
		 * The bound is computed LAZILY on the first call and cached on the func
		 * (nMaxStack == 0 = not yet computed). Deliberately not a compile-time pass:
		 * self-computing on first use is fail-safe against any body-creation path
		 * (an uncomputed body just computes, never uses a wrong 0 -> undersize),
		 * where undersizing is a heap overflow; the amortized cost is one analysis
		 * per function. */
		{
			sxu32 nSlots = pVmFunc->nMaxStack;
			if( nSlots == 0 ){
				sxu32 nInstr = SySetUsed(&pVmFunc->aByteCode);
				sxu32 nTight = VmComputeMaxStack(&(*pVm),
					(VmInstr *)SySetBasePtr(&pVmFunc->aByteCode),nInstr);
				nSlots = ( nTight == VM_STACK_UNMODELED ) ? nInstr : nTight;
				if( nSlots == 0 ){ nSlots = 1; } /* a 0-depth body still needs a valid, nonzero cache marker */
				pVmFunc->nMaxStack = nSlots;
			}
			pFrameStack = VmOperandStackAlloc(&(*pVm),nSlots);
		}
		if( pFrameStack == 0 ){
			/* Raise exception: Out of memory */
			VmErrorFormat(&(*pVm),PH7_CTX_ERR,"PH7 is running out of memory while calling function '%z',NULL will be returned",
				&pVmFunc->sName);
			if( nCallArgs > 0 ){
				VmPopOperand(&pTos,nCallArgs);
			}
			break;
		}
SkipFuncBody:
		if( pSelf ){
			/* Push class name */
			SySetPut(&pVm->aSelf,(const void *)&pSelf);
		}
		/* Increment nesting level */
		pVm->nRecursionDepth++;
		if( rc == PH7_EXCEPTION ){
			/* Arg-binding threw: there is no body to run — finish the call
			 * immediately (no record is pushed). */
			VmCallRecord sCallee;
			sCallee.pVmFunc = pVmFunc;
			sCallee.pFrame = pFrame;
			sCallee.pFrameStack = pFrameStack;
			sCallee.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;
			sCallee.nLastRef = SXU32_HIGH;
			sCallee.bSelfPushed = (sxu8)(pSelf ? 1 : 0);
			sState.pTos = pTos;
			sState.pc = pc;
			rc = VmCallFinish(&(*pVm),&sState,&sCallee,rc);
			pTos = sState.pTos;
			pc = sState.pc;
			if( rc == PH7_ABORT ){
				/* Abort processing immeditaley */
				goto Abort;
			}else if( rc == PH7_SUSPEND ){
				goto Suspend;
			}else if( rc == PH7_EXCEPTION ){
				goto Exception;
			}
		}else{
			/* BYTECODE stage 2: run the callee in THIS dispatch loop. Push a
			 * call record (caller activation + in-flight call) and switch the
			 * loop's locals to the callee — a PHP->PHP call no longer grows
			 * the native stack. The record node is pool-allocated so
			 * sState.pLastRef (aimed at sCall.nLastRef) stays stable. */
			VmCallFrame *pRec = (VmCallFrame *)pVm->pIdleCallFrames;
			if( pRec ){
				pVm->pIdleCallFrames = (void *)pRec->pPrev;
			}else{
				pRec = (VmCallFrame *)SyMemBackendPoolAlloc(&pVm->sAllocator,sizeof(VmCallFrame));
			}
			if( pRec == 0 ){
				/* OOM: undo the push-time accounting, tear the call down and
				 * raise the non-catchable fatal (the §3.1 OOM convention —
				 * never a silent NULL). */
				pVm->nRecursionDepth--;
				if( pSelf ){
					(void)SySetPop(&pVm->aSelf);
				}
				SyMemBackendFree(&pVm->sAllocator,pFrameStack);
				VmLeaveFrame(&(*pVm));
				PH7_VmMemoryError(&(*pVm));
				goto Abort;
			}
			sState.pTos = pTos;
			sState.pc = pc;
			pRec->sCaller = sState;
			pRec->sCall.pVmFunc = pVmFunc;
			pRec->sCall.pFrame = pFrame;
			pRec->sCall.pFrameStack = pFrameStack;
			pRec->sCall.nStackCap = pVmFunc->nMaxStack + VM_STACK_GUARD;
			pRec->sCall.nLastRef = SXU32_HIGH;
			pRec->sCall.bSelfPushed = (sxu8)(pSelf ? 1 : 0);
			pRec->pPrev = pCallTop;
			pCallTop = pRec;
			/* Switch to the callee activation (what the recursive
			 * VmByteCodeExec entry used to set up). */
			aInstr = (VmInstr *)SySetBasePtr(&pVmFunc->aByteCode);
			pStack = pFrameStack;
			pTos = &pStack[-1];
			pc = 0;
			sState.aInstr = aInstr;
			sState.pStack = pStack;
			sState.nStackCap = pRec->sCall.nStackCap; /* callee's operand-stack capacity for OP_SPREAD growth */
			sState.nStackOrig = pRec->sCall.nStackCap; /* fixed headroom reference (never grows) */
			sState.pTos = pTos;
			sState.pc = 0;
			sState.nExceptionBase = SySetUsed(&pVm->aException);
			sState.nFinallyActBase = SySetUsed(&pVm->aFinallyAction);
			sState.pEntryFrame = pVm->pFrame;
			sState.pResult = pRec->sCaller.pTos;
			sState.pLastRef = &pRec->sCall.nLastRef;
			sState.pEnforceRetFunc = VmFuncHasReturnType(pVmFunc) ? pVmFunc : 0;
			sState.is_callback = 0;
			sState.bReturnPropagates = 0;
			goto VmLoopFetch;
		}
	}else{
		ph7_user_func *pFunc;
		ph7_context sCtx;
		ph7_value sRet;
		/* Look for an installed foreign function.
		 * Host functions are registered with short names (strlen, etc.).
		 * If the compiler namespace-qualified the name, extract the short
		 * name (last component after \) and try that. This implements PHP's
		 * global fallback for unqualified function calls in namespaces. */
		pEntry = SyHashGet(&pVm->hHostFunction,(const void *)sName.zString,sName.nByte);
		{
		VmCallArgMap *pCallMap2 = pEffCallMap;
		if( pEntry == 0 && pCallMap2 && pCallMap2->bIsNamespaced ){
			/* Compiler-qualified: try short name as global fallback */
			const char *zShort = sName.zString;
			sxu32 i;
			for( i = 0; i < sName.nByte; i++ ){
				if( sName.zString[i] == '\\' ){
					zShort = &sName.zString[i + 1];
				}
			}
			if( zShort != sName.zString ){
				sxu32 nShort = (sxu32)(sName.nByte - (sxu32)(zShort - sName.zString));
				pEntry = SyHashGet(&pVm->hHostFunction,(const void *)zShort,nShort);
			}
		}
		} /* end VmCallArgMap namespace scope */
		if( pEntry == 0 ){
			/* php accepts the "Class::method" STATIC-callable string everywhere a
			 * callable goes ($f = "C::s"; $f(), call_user_func, array_map, …).
			 * Split on the LAST "::" (php's own zend_memrchr scan) and route through the
			 * shared array-callable machinery ([class-name, method-name]) instead of
			 * warning undefined. */
			const char *zCbCls = 0,*zCbMeth = 0;
			sxu32 nCbCls = 0,nCbMeth = 0;
			int bScoped = PH7_VmCallableStringParts(sName.zString,sName.nByte,
				&zCbCls,&nCbCls,&zCbMeth,&nCbMeth);
			if( bScoped ){
				/* Resolve BEFORE dispatching: the shared dispatcher answers SXRET_OK with
				 * a NULL result for a pair it cannot resolve, so `$cb='C::nosuch'; $cb();`
				 * evaluated to NULL with no diagnostic at all — where php throws the same
				 * Errors the ARRAY form of the same call already raised here. (Its own
				 * `if( bScoped )` block: this scratch buffer and the dispatch block's
				 * hashmap are both block-head declarations, and the check runs between
				 * them.) */
				char zSmMsg[192];
				sxi32 nSmBrc = pVm->nBoundaryRc;
				const void *pSmRes = (const void *)pVm->pResumeFrame;
				const char *zSmErr = VmCallableClassMethodError(&(*pVm),
					PH7_VmExtractClass(&(*pVm),zCbCls,nCbCls,FALSE,0),
					zCbCls,nCbCls,zCbMeth,nCbMeth,TRUE,zSmMsg,sizeof(zSmMsg));
				if( zSmErr ){
					sxi32 rcSmErr;
					int bSmRaised = PH7_VmClassLookupRaised(&(*pVm),nSmBrc,pSmRes);
					if( pInstr->iP2 ){
						VmSpreadConsume(pVm);
					}
					if( nCallArgs > 0 ){
						VmPopOperand(&pTos,nCallArgs);
					}
					PH7_MemObjRelease(pTos);
					MemObjSetType(pTos,MEMOBJ_NULL);
					pTos->nIdx = SXU32_HIGH;
					if( bSmRaised ){
						/* The class name's autoloader threw: land THAT, exactly as the array
						 * form does — php never reports the class missing in this case. */
						rcSmErr = pVm->nBoundaryRc;
						pVm->nBoundaryRc = 0;
						if( rcSmErr == PH7_ABORT ){
							goto Abort;
						}
						rc = PH7_EXCEPTION;
						PH7_THROW_ROUTE_MIDEXPR(rc)
					}
					rcSmErr = VmThrowFromVm(&(*pVm),"Error",zSmErr,(sxu32)SyStrlen(zSmErr));
					if( rcSmErr == SXERR_ABORT ){
						goto Abort;
					}
					rc = rcSmErr;
					PH7_THROW_ROUTE_MIDEXPR(rc)
				}
			}
			if( bScoped ){
				/* Resolved: hand the callable STRING itself to the shared dispatcher, which
				 * decodes `Class::method` the same way (it has to, for the callback-argument
				 * callers). This used to build a throwaway [class,method] map here and enter
				 * through the hashmap branch — two decoders for one spelling. */
				ph7_value sResult;
				sxi32 rcSm;
				pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,
					nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);
				SySetReset(&aArg);
				while( pArg < pTos ){
					SySetPut(&aArg,(const void *)&pArg);
					pArg++;
				}
				PH7_MemObjInit(pVm,&sResult);
				rcSm = PH7_VmCallUserFunctionWithMap(pVm,pTos,(int)SySetUsed(&aArg),
					(ph7_value **)SySetBasePtr(&aArg),&sResult,pEffCallMap);
				SySetReset(&aArg);
				if( nCallArgs > 0 ){
					VmPopOperand(&pTos,nCallArgs);
				}
				if( rcSm == PH7_ABORT ){
					PH7_MemObjRelease(&sResult);
					goto Abort;
				}
				if( rcSm == PH7_EXCEPTION ){
					sxi32 iResumePc;
					PH7_MemObjRelease(&sResult);
					if( VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){
						PH7_MemObjRelease(pTos);
						/* Drain the abandoned outer-expression operands
						 * (`1 + "C::m"()`) to the try's base — one leaked
						 * slot per caught throw otherwise. */
						PH7_RESUME_DRAIN()
						pc = iResumePc;
						break;
					}
					goto Exception;
				}
				PH7_MemObjStore(&sResult,pTos);
				PH7_MemObjRelease(&sResult);
				break;
			}
			/* Call to an undefined function is a catchable Error in php 8 — it does
			 * NOT warn and hand back null and carry on, which is what PH7 did (and
			 * which quietly turned a typo into a null-propagating program). */
			{
			SyBlob sMsg;
			SyBlobInit(&sMsg,&pVm->sAllocator);
			SyBlobFormat(&sMsg,"Call to undefined function %z()",&sName);
			/* Consume this call's captured spread runs so they don't leak into a
			 * later call (this path never reaches VmBuildEffectiveArgMap). */
			if( pInstr->iP2 ){
				VmSpreadConsume(pVm);
			}
			/* Pop given arguments. nCallArgs (not iP1) — an unpack expanded the
			 * compile-time arg count on the stack, and this early exit skips the
			 * arg-building loop that the normal path uses, so popping only iP1 would
			 * strand the expanded elements and corrupt the enclosing expression. */
			if( nCallArgs > 0 ){
				VmPopOperand(&pTos,nCallArgs);
			}
			PH7_MemObjRelease(pTos);
			rc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sMsg),
				SyBlobLength(&sMsg));
			SyBlobRelease(&sMsg);
			if( rc == SXERR_ABORT ){
				goto Abort;
			}
			/* A catch may have run IN PLACE inside VmThrowFromVm (SXRET_OK +
			 * recorded resume). An unconditional `goto Exception` here unwound the
			 * exec ANYWAY, so `try { nosuchfn(); } catch (Error $e) {} rest();`
			 * ran the catch and then silently dropped the rest of the script
			 * (exit 0). Route like every other in-exec throw site. */
			PH7_THROW_ROUTE_MIDEXPR(rc)
			}
		}
		pFunc = (ph7_user_func *)pEntry->pUserData;
		/* D1: resolve deferred plain-var arguments against the builtin's by-ref position
		 * mask (derived from its signature). A by-ref out-param (preg_match's $matches, …)
		 * is materialized so PH7_VmStoreArgByRef can write back — this is what keeps a
		 * DYNAMIC-name by-ref builtin (`$f='preg_match'; $f($p,$s,$m)`) working now that the
		 * compile-time mask no longer sees it. Every other (by-value) arg warns + passes
		 * NULL, which is also what call_user_func & friends want. pVm->pFrame is the caller. */
		{
			sxi32 rcDA = VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,pFunc->nByRefMask,0,0);
			if( rcDA == PH7_ABORT ){
				goto Abort;
			}else if( rcDA == PH7_EXCEPTION ){
				goto Exception;
			}
		}
		/* Host function (builtin): build the effective spread-key map so the
		 * name-forwarding builtins (call_user_func & friends) relay string keys as
		 * named args, and — critically — so this call's captured runs are consumed.
		 * pArg is the top base here (a builtin call pops no method-name slot). */
		pEffCallMap = VmEffCallArgMap(pVm,pInstr,pArg,
			nCallArgs > 0 ? (sxu32)nCallArgs : 0,&sEffMap);
		/* Start collecting function arguments */
		SySetReset(&aArg);
		while( pArg < pTos ){
			SySetPut(&aArg,(const void *)&pArg);
			pArg++;
		}
		/* Assume a null return value */
		PH7_MemObjInit(&(*pVm),&sRet);
		/* Init the call context */
		VmInitCallContext(&sCtx,&(*pVm),pFunc,&sRet,0);
		/* Hand the call-site named-argument map to the builtin so name-forwarding
		 * helpers (call_user_func & friends) can relay name: arguments — and the
		 * caller's strict_types mode — to the inner callback. Forwarded whole (not
		 * gated on bHasNamed) because call_user_func_array reads bStrict from it even
		 * when its own call site is purely positional; only the two forwarding
		 * builtins read pArgMap, so this is inert for every other host function. */
		sCtx.pArgMap = pEffCallMap;
		{
		int nGiven = (int)SySetUsed(&aArg);
		/* PHP-8 arity enforcement (band A #5): a builtin declaring a minimum
		 * argument count (aBuiltinArity[]) throws a catchable ArgumentCountError
		 * before the C routine runs when called with too few arguments — instead
		 * of the legacy PH7-ism of silently degrading to a bogus false/-1/""
		 * return. The message wording matches php's ZPP output byte-for-byte. */
		if( pFunc->nMinArg > 0 && nGiven < pFunc->nMinArg ){
			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",
				"%z() expects %s %d argument%s, %d given",
				&pFunc->sName,
				pFunc->bAtLeast ? "at least" : "exactly",
				(int)pFunc->nMinArg,
				pFunc->nMinArg == 1 ? "" : "s",
				nGiven);
		}else if( pFunc->bHasMaxArg && nGiven > (int)pFunc->nMaxArg ){
			/* php enforces the MAXIMUM as well, and PHL only did so where a
			 * builtin happened to hand-roll the check (51 of ~650), so
			 * `microtime(1,2)`, `strlen("a","b")` and friends silently ignored
			 * the extras. The count comes from the same signature table as the
			 * minimum; a variadic tail leaves nMaxArg at -1 and is exempt.
			 * php says "exactly" when the bounds coincide, "at most" otherwise. */
			rc = PH7_VmThrowException(&sCtx,"ArgumentCountError",
				"%z() expects %s %d argument%s, %d given",
				&pFunc->sName,
				((int)pFunc->nMinArg == (int)pFunc->nMaxArg && !pFunc->bAtLeast) ? "exactly" : "at most",
				(int)pFunc->nMaxArg,
				pFunc->nMaxArg == 1 ? "" : "s",
				nGiven);
		}else if( SXRET_OK != (rc = VmEnforceBuiltinArgTypes(&sCtx,pFunc,nGiven,
			(ph7_value **)SySetBasePtr(&aArg))) ){
			/* TypeError thrown: rc carries the caught/uncaught status */
		}else{
			/* Call the foreign function */
			rc = pFunc->xFunc(&sCtx,nGiven,(ph7_value **)SySetBasePtr(&aArg));
			/* A host function that RAISED a catchable throw (PH7_VmThrowException)
			 * and still returned PH7_OK reports it here — the throw's catch has
			 * already run in place, so treating the call as a normal return would
			 * resume execution INSIDE the try body the throw abandoned (and, when
			 * uncaught, run on past the reported fatal). The status is recorded on
			 * the call context, so this covers the shared validation helpers whose
			 * callers have no channel to thread a status back. */
			rc = VmHostFuncThrowRc(&sCtx,rc);
		}
		}
		/* Release the call context */
		VmReleaseCallContext(&sCtx);
		if( rc == PH7_ABORT ){
			/* Release the (possibly partially-built) result slot before unwinding;
			 * the Abort: label only frees the operand stack, not this local
			 * (mirrors the PH7_EXCEPTION branch below). */
			PH7_MemObjRelease(&sRet);
			goto Abort;
		}
		if( rc != PH7_SUSPEND && pVm->pInlineInstr == (void *)aInstr ){
			/* A throw raised inside this host function — directly
			 * (PH7_VmThrowException) or by a PHP callback it invoked — was
			 * caught by an INLINE try (generator body) THIS exec owns.
			 * VmThrowInline records only a pc-redirect: a direct builtin throw
			 * travels back as SXRET_OK and a callback throw as PH7_EXCEPTION
			 * with the redirect pending, so the rc branches below never land
			 * it (pre-existing hole: explode("") or a throwing usort
			 * comparator inside a generator's try lost the catch AND the
			 * yield). Land at the redirect now — its drain to the try's
			 * operand base subsumes the args + name pops. */
			PH7_MemObjRelease(&sRet);
			PH7_INLINE_RESUME_BREAK()
		}
		if( rc == PH7_EXCEPTION ){
			/* A callback invoked by this host function threw. If an in-place catch
			 * recorded a resume target owned by THIS body, resume at its landing pad
			 * (consuming the target); otherwise the exception was caught by an outer
			 * exec (or not caught here) — propagate. Replaces the old "VM_FRAME_THROW
			 * means uncaught, else jump to pVm->pFrame's nearest iExceptionJump", which
			 * resumed at the wrong try when the catcher was not the nearest (ROOT B). */
			sxi32 iResumePc;
			if( !VmRecordedResume(pVm,&iResumePc,sState.pEntryFrame,aInstr) ){
				/* Caught by an outer exec, or not caught here: propagate. */
				goto Exception;
			}
			/* Exception was caught in place by THIS body's try: pop args and the
			 * result slot, then drain any abandoned outer-expression operands to
			 * the try's base and resume. */
			PH7_MemObjRelease(&sRet);
			if( nCallArgs > 0 ){
				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */
			}
			VmPopOperand(&pTos,1);
			PH7_RESUME_DRAIN()
			pc = iResumePc;
			break;
		}
		if( rc == PH7_SUSPEND && pVm->pActiveCtx ){
			/* Fiber::suspend() was called from within a fiber.
			 * Pop arguments (like normal path) but don't push a return value.
			 * Propagate PH7_SUSPEND up. If this is the fiber's own
			 * VmByteCodeExec, the CALL was to a foreign function directly
			 * and we need to save state here. If it's a nested call (method
			 * body), the user-function path above will handle re-saving. */
			PH7_MemObjRelease(&sRet);
			if( nCallArgs > 0 ){
				VmPopOperand(&pTos,nCallArgs); /* spread-adjusted; see the normal-return path */
			}
			/* Save fiber state: pc+1 is the instruction after this CALL.
			 * nTos is one below pTos so resume pushes at the return-value slot. */
			VmSuspendCtx(pVm,pVm->pActiveCtx,pc + 1,(sxi32)(pTos - pStack) - 1);
			goto Suspend;
		}
		if( nCallArgs > 0 ){
			/* Pop the arguments. nCallArgs (spread-adjusted), NOT iP1: an unpack
			 * expanded the compile-time arg count on the stack, so popping iP1
			 * strands the extra elements (or, for an unpack that expanded to fewer
			 * than iP1 — e.g. array_merge(...[]) — underflows the operand stack,
			 * reading a bogus value whose stray flags sent MemObjStore into the
			 * hashmap-release path and hung). Mirrors every other CALL exit. The
			 * function-name slot (pTos) receives the return value below. */
			VmPopOperand(&pTos,nCallArgs);
		}
		/* Save foreign function return value into the (now top) function-name slot */
		PH7_MemObjStore(&sRet,pTos);
		PH7_MemObjRelease(&sRet);
	}
	break;
				  }
/*
 * OP_CONSUME: P1 * *
 * Consume (Invoke the installed VM output consumer callback) and POP P1 elements from the stack.
 */
case PH7_OP_CONSUME: {
	VmOpRc rcOp;
	sState.pTos = pTos;
	sState.pc = pc;
	rcOp = VmExecOpConsume(&(*pVm),&sState,pInstr);
	pTos = sState.pTos;
	pc = sState.pc;
	if( rcOp == VM_OP_ABORT ){
		goto Abort;
	}else if( rcOp == VM_OP_EXCEPTION ){
		goto Exception;
	}
	break;
					  }

		} /* Switch() */
		pc++; /* Next instruction in the stream */
	} /* For(;;) */
Done:
	/* A stacked callee completing lands here too (its result is already in
	 * sState.pResult — the caller's operand slot); Unwind's first iteration
	 * bottoms out identically for the record-less case. */
	rc = SXRET_OK;
	goto Unwind;
Suspend:
	rc = PH7_SUSPEND;
	if( pCallTop != 0 ){
		/* BYTECODE stage 4: deep Fiber::suspend() — park the record segment
		 * instead of the lossy unwind. pc/nTos of the innermost activation were
		 * already saved into the ctx by VmSuspendCtx; capture the rest (the
		 * record chain, the innermost activation, the suspend-time top frame)
		 * so resume re-enters HERE, inside the innermost callee, like php. The
		 * records / frames / operand stacks stay alive — nothing is freed. Only
		 * fibers reach this (generators yield only at their body level, pCallTop
		 * == 0); a suspend inside a C->PHP callback was already rejected with a
		 * FiberError before it could arrive here. */
		VmParkedSegment *pSeg = (VmParkedSegment *)SyMemBackendAlloc(&pVm->sAllocator,sizeof(VmParkedSegment));
		if( pSeg == 0 ){
			/* OOM on the park allocation. VmSuspendCtx already wrote the INNERMOST
			 * pc/nTos into the ctx, so the lossy body-level fallback would resume
			 * the callee's pc against the body stack — silent corruption, exactly
			 * what stage 4 removed. Take the non-catchable OOM fatal instead (the
			 * §3.1 convention shared with the stage-2 record-alloc OOM site). */
			PH7_VmMemoryError(&(*pVm));
			rc = PH7_ABORT;
			goto Unwind;
		}
		sState.pTos = pTos; /* innermost live top (args already popped at the CALL) */
		pSeg->sState = sState;
		pSeg->pCallTop = pCallTop;
		pSeg->pTopFrame = pVm->pFrame;
		pSeg->nOldExcBase = pVm->pActiveCtx ? pVm->pActiveCtx->nExceptionBase : 0;
		pSeg->nOldFinBase = pVm->pActiveCtx ? pVm->pActiveCtx->nFinallyBase : 0;
		{
			VmCallFrame *pRec;
			pSeg->nRecords = 0;
			for( pRec = pCallTop; pRec; pRec = pRec->pPrev ){
				pSeg->nRecords++;
			}
		}
		pVm->pActiveCtx->pParkedSegment = (void *)pSeg;
		/* Return straight to VmStartCtx/VmResumeCtx without touching the records. */
		SySetRelease(&aArg);
		return PH7_SUSPEND;
	}
	goto Unwind;
Abort:
	rc = PH7_ABORT;
	goto Unwind;
Exception:
	rc = PH7_EXCEPTION;
	goto Unwind;
Unwind:
	/* BYTECODE stage 2: unwind this invocation's call records. Each iteration
	 * finishes the top record exactly as the old per-level native return did:
	 * for ABORT/EXCEPTION, first run what the popped activation's own
	 * Abort/Exception label used to do (clear its pending return, release its
	 * operands — a stacked activation never has bReturnPropagates set), then
	 * VmCallFinish routes in the restored caller (an in-place catch there
	 * resumes dispatch; otherwise keep popping). SUSPEND pops with no cleanup —
	 * VmCallFinish re-saves the ctx per level ("last wins"), byte-compatible
	 * with the pre-stage-2 lossy deep-suspend (stage 4 replaces this).
	 * At the bottom, VmExecFinalize hands the status to the native caller. */
	for(;;){
		if( rc == PH7_ABORT || rc == PH7_EXCEPTION ){
			/* Drop any pending hook-RMW write-backs this activation armed — its
			 * statement is abandoned (only the innermost activation at throw time
			 * can own entries: the armed window spans exactly one instruction, so
			 * no OP_CALL record ever intervenes). */
			while( SySetUsed(&pVm->aHookRmw) > 0
			 && ((VmHookRmw *)SySetPeek(&pVm->aHookRmw))->pOwnerStack == (void *)pStack ){
				VmHookRmwDropTop(&(*pVm));
			}
		}
		if( pCallTop == 0 ){
			return VmExecFinalize(&(*pVm),&sState,&aArg,pTos,rc);
		}
		if( rc == PH7_ABORT || rc == PH7_EXCEPTION ){
			VmClearFrameReturn(sState.pEntryFrame);
			while( pTos >= pStack ){
				PH7_MemObjRelease(pTos);
				pTos--;
			}
		}
		if( rc != PH7_SUSPEND ){
			/* The finishing callee's own leaked finally actions must not survive
			 * into the caller (whose next OP_END_FINALLY would mis-pop them). */
			VmDiscardFinallyActions(&(*pVm),sState.nFinallyActBase);
		}
		{
			VmCallFrame *pRec = pCallTop;
			sState = pRec->sCaller;
			rc = VmCallFinish(&(*pVm),&sState,&pRec->sCall,rc);
			pCallTop = pRec->pPrev;
			pRec->pPrev = (VmCallFrame *)pVm->pIdleCallFrames;
			pVm->pIdleCallFrames = (void *)pRec;
			aInstr = sState.aInstr;
			pStack = sState.pStack;
			pTos = sState.pTos;
			pc = sState.pc;
		}
		if( rc == PH7_OK ){
			pc++; /* the loop-bottom increment this OP_CALL missed */
			goto VmLoopFetch;
		}
	}
}
