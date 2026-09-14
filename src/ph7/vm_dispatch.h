/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef __VM_DISPATCH_H__
#define __VM_DISPATCH_H__
/*
 * The dispatch-routing macro family shared by vm.c's interpreter loop and the
 * vm_ops_*.c opcode handlers. Every macro body is written once here against
 * three exit primitives the including translation unit must define FIRST:
 *
 *   VM_EXIT_BREAK      end this opcode arm (vm.c: break -> trailing pc++;
 *                      handlers: sync pTos/pc into the VmExecState and
 *                      return VM_OP_NEXT)
 *   VM_EXIT_ABORT      vm.c: goto Abort;      handlers: return VM_OP_ABORT
 *   VM_EXIT_EXCEPTION  vm.c: goto Exception;  handlers: return VM_OP_EXCEPTION
 *
 * The bodies reference the loop locals pVm/pTos/pStack/pc/aInstr and
 * sState.pEntryFrame; handler files provide the same names (see vm_ops_*.c
 * prologues, which map sState onto their VmExecState pointer).
 */
#if !defined(VM_EXIT_BREAK) || !defined(VM_EXIT_ABORT) || !defined(VM_EXIT_EXCEPTION)
#error "define VM_EXIT_BREAK/VM_EXIT_ABORT/VM_EXIT_EXCEPTION before including vm_dispatch.h"
#endif
/*
 * ROOT C: if a throw was caught by an INLINE try owned by THIS exec, drain the
 * abandoned operand slots back to the try's stack base and jump to the catch/finally
 * body; the throw runs in this same dispatch loop (so a yield inside it suspends).
 * pInlineInstr != aInstr means an outer exec owns the handler → fall through and
 * propagate. Must be used inside a case of the main switch (uses break/pc/pTos).
 */
#define PH7_INLINE_RESUME_BREAK() \
	if( pVm->pInlineInstr == (void *)aInstr ){ \
		while( (sxi32)(pTos - pStack) > pVm->iInlineDrain ){ PH7_MemObjRelease(pTos); pTos--; } \
		pc = (sxi32)pVm->iInlinePc - 1; \
		pVm->pInlineInstr = 0; \
		pVm->nBoundaryRc = 0; /* redirect consumed: drop any parked copy of this throw */ \
		VM_EXIT_BREAK; \
	}
/*
 * Route a throw raised by the VM in the MIDDLE of an expression (an opcode that is not a
 * call boundary: OP_LOADC, OP_LOAD_IDX, ...). Mirrors OP_THROW's tail, which is the only
 * place that got this right, with one difference: OP_THROW is handed its enclosing try's
 * landing pad by the compiler (pInstr->iP2), and a mid-expression opcode has none. The
 * frame recorded exactly that value when the try was entered (iExceptionJump), so use it.
 *
 * Getting this wrong is subtle and was shipped twice: a plain `break` resumes at the NEXT
 * instruction, so the catch runs and then execution carries on INSIDE the try, running the
 * code the throw should have skipped; while `goto Exception` unwinds the whole invocation,
 * which is right at a call boundary but corrupts the frame here — even for a CAUGHT Error.
 *
 * Use as the last statement of the opcode's throw path; it always breaks or jumps.
 * rcVar is the status VmThrowFromVm/VmThrowException returned.
 */
#define PH7_THROW_ROUTE_MIDEXPR(rcVar) \
	PH7_INLINE_RESUME_BREAK() \
	if( (rcVar) == PH7_EXCEPTION || pVm->pResumeFrame || pVm->pInlineInstr ){ \
		sxi32 _iRpM; \
		if( VmRecordedResume(pVm,&_iRpM,sState.pEntryFrame,aInstr) ){ \
			pc = _iRpM; \
			VM_EXIT_BREAK; \
		} \
		VM_EXIT_EXCEPTION; \
	} \
	{ \
		VmFrame *_pFrM = VmSkipExceptionFrames(pVm->pFrame); \
		if( _pFrM && (_pFrM->iFlags & VM_FRAME_THROW) && _pFrM->iExceptionJump > 0 ){ \
			/* An enclosing try in THIS frame caught it: land on its OP_POP_EXCEPTION, \
			 * which tears the try frame down, runs finally and balances the stack. */ \
			pc = (sxi32)_pFrM->iExceptionJump - 1; \
			VM_EXIT_BREAK; \
		} \
	} \
	VM_EXIT_EXCEPTION;
#define PH7_DISPATCH_ENFORCE_RC(rcVar) \
	if( (rcVar) == PH7_ABORT ){ VM_EXIT_ABORT; } \
	if( (rcVar) == PH7_EXCEPTION || pVm->pInlineInstr ){ \
		sxi32 _iRpE; \
		PH7_INLINE_RESUME_BREAK() \
		if( VmRecordedResume(pVm,&_iRpE,sState.pEntryFrame,aInstr) ){ \
			pc = _iRpE; \
			VM_EXIT_BREAK; \
		} \
		if( (rcVar) == PH7_EXCEPTION ){ VM_EXIT_EXCEPTION; } \
	}
/*
 * Route the status of an iterator-protocol method call (foreach over an
 * Iterator/IteratorAggregate/Generator: rewind/valid/current/next/key/
 * getIterator) from inside the OP_FOREACH_INIT / OP_FOREACH_STEP cases.
 * Before invoking this, the site must have released its transients and
 * detached/freed its foreach step (each site's teardown differs) — these
 * calls used to IGNORE their status entirely, so an exception thrown by a
 * generator body (or a userland Iterator method) during foreach silently
 * ended the loop and execution continued after it (php: the exception
 * propagates; uncaught → fatal + exit 255). nPopOnResume operands are
 * consumed when an in-place catch resumes this body, mirroring the op's
 * normal stack effect. Must be used directly inside a case of the main
 * switch (uses break/pc/pTos, like PH7_DISPATCH_ENFORCE_RC).
 */
#define PH7_DISPATCH_ITER_RC(rcVar,nPopOnResume) \
	if( (rcVar) == PH7_ABORT ){ VM_EXIT_ABORT; } \
	{ \
		sxi32 _iRpI; \
		PH7_INLINE_RESUME_BREAK() \
		if( VmRecordedResume(pVm,&_iRpI,sState.pEntryFrame,aInstr) ){ \
			if( (nPopOnResume) > 0 ){ VmPopOperand(&pTos,(nPopOnResume)); } \
			pc = _iRpI; \
			VM_EXIT_BREAK; \
		} \
		if( (rcVar) == PH7_EXCEPTION ){ VM_EXIT_EXCEPTION; } \
	}
/*
 * The routing condition for the above: the iterator-protocol call aborted,
 * threw past this exec, or was caught by an inline try THIS exec owns
 * (pInlineInstr identity). Named once so the seven foreach sites cannot
 * drift; a missed edit here would silently re-swallow exceptions.
 */
#define VmIterCallThrew(rcVar) \
	((rcVar) == PH7_ABORT || (rcVar) == PH7_EXCEPTION || pVm->pInlineInstr == (void *)aInstr)
/*
 * Typed-property enforcement helper for compound stores. Called before
 * PH7_MemObjStore writes into a member memobj slot. On failure throws a
 * PHP TypeError and either jumps to the nearest catch block or propagates
 * out of the VM loop. Must be used inside a case of the main switch.
 */
#define PH7_ENFORCE_TYPED_STORE(nIdxArg, pSrcArg) \
	{ \
		sxi32 _rcT = VmEnforcePropertyTypeOnStore(&(*pVm),(nIdxArg),(pSrcArg),0); \
		PH7_DISPATCH_ENFORCE_RC(_rcT) \
	}
/*
 * Readonly enforcement helper for the in-place mutation opcodes (`++`/`--`),
 * which bypass the typed-store path. Throws "Cannot modify readonly property"
 * when the lvalue slot is a readonly property, otherwise proceeds. Must be used
 * inside a case of the main switch.
 */
#define PH7_ENFORCE_READONLY_MUTATE(nIdxArg) \
	{ \
		sxi32 _rcR = VmCheckReadonlyMutate(&(*pVm),(nIdxArg)); \
		PH7_DISPATCH_ENFORCE_RC(_rcR) \
	}
/*
 * Hook-RMW write-back for the tail of every read-modify-write opcode: if the
 * top pending VmHookRmw entry targets the slot this op just wrote (a SCRATCH
 * slot armed by the preceding OP_MEMBER on a hooked property), dispatch the
 * property's set hook with the computed value. pResArg (may be 0) is the
 * op's surviving stack result whose nIdx still points at the freed scratch
 * slot — sanitize it to a pure temp so no later op treats it as an lvalue.
 * Must be used inside a case of the main switch.
 */
#define PH7_HOOK_RMW_WRITEBACK(nIdxArg, pResArg) \
	if( SySetUsed(&pVm->aHookRmw) > 0 ){ \
		sxi32 _rcHk = VmHookRmwConsume(&(*pVm),(nIdxArg)); \
		if( _rcHk == PH7_ABORT ){ \
			VM_EXIT_ABORT; \
		} \
		if( _rcHk == SXRET_OK && (pResArg) != 0 ){ \
			((ph7_value *)(pResArg))->nIdx = SXU32_HIGH; \
		} \
	}
#endif /* __VM_DISPATCH_H__ */
