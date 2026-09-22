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
 * Drain the abandoned mid-expression operand slots back to the catching try's
 * base after VmRecordedResume matched (iResumeStackDepth was recorded with the
 * resume target — the try-entry depth). Everything above it is dead: the ops
 * that would have consumed those slots were abandoned by the throw, and the
 * landing pad expects the try-entry depth. NOT draining leaks one slot per
 * caught throw — a try/catch loop around a throwing op eventually overflows
 * the operand stack (ASan heap-buffer-overflow; hit by the typed-property
 * throw paths). Mirrors the fetch-point router and the generator inject path,
 * which have always drained.
 */
#define PH7_RESUME_DRAIN() \
	while( (sxi32)(pTos - pStack) > pVm->iResumeStackDepth ){ PH7_MemObjRelease(pTos); pTos--; }
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
			PH7_RESUME_DRAIN() \
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
/*
 * Route the status of a USER-VISIBLE string coercion (PH7_MemObjToStringUV).
 * An object with no __toString() is php's catchable
 * "Object of class X could not be converted to string" Error, raised at the
 * coercion itself — echo/print, `.`/`.=`, the (string) cast, interpolation, a
 * variable-variable NAME, a string-offset store. None of those is a call
 * boundary, so the throw is routed mid-expression: parking it would let the
 * rest of the expression run on the value the abandoned coercion left behind.
 * Named once so the coercion sites cannot drift apart.
 */
#define PH7_DISPATCH_TOSTRING_RC(rcVar) \
	if( (rcVar) != SXRET_OK ){ \
		if( (rcVar) == PH7_ABORT ){ VM_EXIT_ABORT; } \
		PH7_THROW_ROUTE_MIDEXPR(rcVar) \
	}
#define PH7_DISPATCH_ENFORCE_RC(rcVar) \
	if( (rcVar) == PH7_ABORT ){ VM_EXIT_ABORT; } \
	if( (rcVar) == PH7_EXCEPTION || pVm->pInlineInstr ){ \
		sxi32 _iRpE; \
		PH7_INLINE_RESUME_BREAK() \
		if( VmRecordedResume(pVm,&_iRpE,sState.pEntryFrame,aInstr) ){ \
			PH7_RESUME_DRAIN() \
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
			PH7_RESUME_DRAIN() \
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
 * php's unary-arithmetic operand contract for OP_UMINUS / OP_UPLUS. php compiles
 * `-$x` as `$x * -1`, so both operators reject the operands multiplication
 * rejects, with multiplication's own wording ("Unsupported operand types:
 * P * int"). The operand IS the op's result slot, so settle it to NULL before
 * throwing -- an in-place catch resumes with a balanced stack. Must be used
 * directly inside a case of the main switch.
 */
#define PH7_UNARY_ARITH_CONTRACT() \
	{ \
		SyBlob _sUnMsg; \
		SyBlobInit(&_sUnMsg,&pVm->sAllocator); \
		if( VmUnaryArithOperandCheck(&(*pVm),pTos,&_sUnMsg) != SXRET_OK ){ \
			sxi32 _rcUn; \
			PH7_MemObjRelease(pTos); \
			MemObjSetType(pTos,MEMOBJ_NULL); \
			pTos->nIdx = SXU32_HIGH; \
			_rcUn = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&_sUnMsg), \
				SyBlobLength(&_sUnMsg)); \
			SyBlobRelease(&_sUnMsg); \
			if( _rcUn == SXERR_ABORT ){ VM_EXIT_ABORT; } \
			rc = _rcUn; \
			PH7_THROW_ROUTE_MIDEXPR(rc) \
		} \
		SyBlobRelease(&_sUnMsg); \
	}
/*
 * php's operand contract for the BITWISE binary operators (`&`, `|`, `^`, `<<`,
 * `>>`), which is the ARITHMETIC one with the operator's own name in the
 * message: an array, object, resource or non-numeric string operand is
 * "Unsupported operand types: array & int", and a leading-numeric string warns
 * and computes with the prefix. PHL cast every operand to an int and answered a
 * number. cOpArg is php's spelling of the operator; the stack effect on the
 * throw mirrors the arithmetic sites (pop one, settle the survivor to NULL).
 *
 * bOffenderFirstArg selects php's operand ORDER in the message, which is not
 * uniform (probed value-for-value): the plain `&`/`|`/`^` name a RESOURCE
 * operand first, then an OBJECT one, whichever side it is actually on — `1 & $o`
 * is "P & int", `$o & $res` and `$res & $o` are both "resource & P" — because
 * zend reaches the message through op2's do_operation attempt, which hands the
 * operands over in that order. The COMPOUND ASSIGNMENTS (`$x &= $o` is
 * "int & BsocP") and the SHIFTS (`1 << $o` is "int << P") have their own error
 * paths and stay positional, as do all the arithmetic operators. Pass 0 there.
 * Must be used directly inside a case of the main switch.
 */
#define PH7_BITWISE_ARITH_CONTRACT(pLeftArg,pRightArg,cOpArg,bOffenderFirstArg) \
	{ \
		SyBlob _sBwMsg; \
		ph7_value *_pBwL = (pLeftArg), *_pBwR = (pRightArg); \
		if( (bOffenderFirstArg) \
		 && (((_pBwR->iFlags & MEMOBJ_RES) != 0 && (_pBwL->iFlags & MEMOBJ_RES) == 0) \
		  || ((_pBwR->iFlags & MEMOBJ_OBJ) != 0 && (_pBwL->iFlags & (MEMOBJ_OBJ|MEMOBJ_RES)) == 0)) ){ \
			ph7_value *_pBwT = _pBwL; _pBwL = _pBwR; _pBwR = _pBwT; \
		} \
		SyBlobInit(&_sBwMsg,&pVm->sAllocator); \
		if( VmArithOperandCheck(&(*pVm),_pBwL,_pBwR,(cOpArg),&_sBwMsg) != SXRET_OK ){ \
			sxi32 _rcBw; \
			VmPopOperand(&pTos,1); \
			PH7_MemObjRelease(pTos); \
			MemObjSetType(pTos,MEMOBJ_NULL); \
			pTos->nIdx = SXU32_HIGH; \
			_rcBw = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&_sBwMsg), \
				SyBlobLength(&_sBwMsg)); \
			SyBlobRelease(&_sBwMsg); \
			if( _rcBw == SXERR_ABORT ){ VM_EXIT_ABORT; } \
			rc = _rcBw; \
			PH7_THROW_ROUTE_MIDEXPR(rc) \
		} \
		SyBlobRelease(&_sBwMsg); \
	}
/*
 * The shift operators' half of the contract above. `<<`/`>>` have no string arm
 * at all (`"abc" << 1` is "Unsupported operand types: string << int", while
 * `"12" << 2` is 48), and their message is POSITIONAL on both sides. pLeftArg is
 * the shifted value, pRightArg the count. Must be used directly inside a case of
 * the main switch (or an opcode handler with the same locals).
 */
#define PH7_SHIFT_ARITH_CONTRACT(pLeftArg,pRightArg) \
	{ \
		char _zShOp[3]; \
		_zShOp[0] = _zShOp[1] = pInstr->iOp == PH7_OP_SHL || pInstr->iOp == PH7_OP_SHL_STORE ? '<' : '>'; \
		_zShOp[2] = 0; \
		PH7_BITWISE_ARITH_CONTRACT((pLeftArg),(pRightArg),_zShOp,0) \
	}
/*
 * php's rules for the shift COUNT, none of which PHL applied — it truncated the
 * count to 32 bits and handed it to C's `<<`/`>>`, which is UNDEFINED for a
 * negative count or one past the operand width (x86 masks it to 6 bits, so
 * `1 << 64` answered 1 and `8 >> 64` answered 8):
 *   negative      a catchable ArithmeticError, php's "Bit shift by negative
 *                 number" — settled and routed mid-expression like the operand
 *                 contract, since the shift is not a call boundary.
 *   >= 64         php saturates instead of wrapping: `<<` shifts every bit out
 *                 (0), `>>` keeps the SIGN (0, or -1 for a negative operand).
 *   `<<` in range done in UNSIGNED space: `1 << 63` is php's PHP_INT_MIN, and a
 *                 signed left shift into the sign bit is UB.
 * iCountArg is the raw 64-bit count, aArg the value being shifted, rArg the
 * result lvalue, bLeftArg true for `<<`. The stack effect on the throw mirrors
 * the two-operand sites (pop one, settle the survivor to NULL).
 */
#define PH7_SHIFT_COUNT_RULES(iCountArg,aArg,rArg,bLeftArg) \
	{ \
		sxi64 _iShCount = (iCountArg); \
		if( _iShCount < 0 ){ \
			sxi32 _rcSh; \
			VmPopOperand(&pTos,1); \
			PH7_MemObjRelease(pTos); \
			MemObjSetType(pTos,MEMOBJ_NULL); \
			pTos->nIdx = SXU32_HIGH; \
			_rcSh = VmThrowFromVm(&(*pVm),"ArithmeticError","Bit shift by negative number", \
				sizeof("Bit shift by negative number")-1); \
			if( _rcSh == SXERR_ABORT ){ VM_EXIT_ABORT; } \
			rc = _rcSh; \
			PH7_THROW_ROUTE_MIDEXPR(rc) \
		} \
		if( _iShCount >= 64 ){ \
			(rArg) = (bLeftArg) ? 0 : ((aArg) < 0 ? -1 : 0); \
		}else if( bLeftArg ){ \
			(rArg) = (sxi64)((sxu64)(aArg) << _iShCount); \
		}else{ \
			(rArg) = (aArg) >> _iShCount; \
		} \
	}
/*
 * Replace pDestArg with the per-byte result of a two-STRING `&`/`|`/`^`
 * (VmStringBitwise). The destination may be one of the two operands, so the
 * bytes are built in a scratch blob first — and its own blob can be a READ-ONLY
 * view of a variable's buffer (PH7_MemObjLoad), which is why this releases
 * before appending rather than writing in place.
 */
#define PH7_STRING_BITWISE_RESULT(pDestArg,pLeftArg,pRightArg,cOpArg) \
	{ \
		SyBlob _sBwBuf; \
		SyBlobInit(&_sBwBuf,&pVm->sAllocator); \
		VmStringBitwise((pLeftArg),(pRightArg),(cOpArg),&_sBwBuf); \
		PH7_MemObjRelease(pDestArg); \
		MemObjSetType((pDestArg),MEMOBJ_STRING); \
		if( SyBlobLength(&_sBwBuf) > 0 ){ \
			SyBlobAppend(&(pDestArg)->sBlob,SyBlobData(&_sBwBuf),SyBlobLength(&_sBwBuf)); \
		} \
		SyBlobRelease(&_sBwBuf); \
	}
/*
 * php refuses to READ-MODIFY-WRITE a string offset: `$s[0]++`, `$s[0]--` and
 * every `$s[0] op= v` raise a catchable Error instead. The value read out of a
 * string still carries the BASE VARIABLE's slot index (a string offset is not a
 * slot of its own), so writing the computed result back through that index
 * replaced the WHOLE STRING with it — `$s = "5abc"; $s[0] += 1;` left
 * `$s === 6`, and `$s[0] .= "x"` appended to the whole string. Same marker the
 * reference-binding sites test (MEMOBJ_AUX_STROFFSET).
 *
 * PH7_REJECT_STROFFSET_INCDEC() is for the one-operand mutation opcodes (the
 * operand IS the result slot); PH7_REJECT_STROFFSET_ASSIGNOP() is for the
 * two-operand compound stores, whose lvalue is pTos and whose normal stack
 * effect pops one operand. Both must be used directly inside a case of the main
 * switch, BEFORE the op touches the operand.
 */
#define PH7_REJECT_STROFFSET_INCDEC() \
	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){ \
		sxi32 _rcSo; \
		PH7_MemObjRelease(pTos); \
		MemObjSetType(pTos,MEMOBJ_NULL); \
		pTos->nIdx = SXU32_HIGH; \
		_rcSo = VmThrowFromVm(&(*pVm),"Error","Cannot increment/decrement string offsets", \
			sizeof("Cannot increment/decrement string offsets")-1); \
		if( _rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; } \
		rc = _rcSo; \
		PH7_THROW_ROUTE_MIDEXPR(rc) \
	}
#define PH7_REJECT_STROFFSET_ASSIGNOP() \
	if( pTos->iFlags & MEMOBJ_AUX_STROFFSET ){ \
		sxi32 _rcSo; \
		VmPopOperand(&pTos,1); \
		PH7_MemObjRelease(pTos); \
		MemObjSetType(pTos,MEMOBJ_NULL); \
		pTos->nIdx = SXU32_HIGH; \
		_rcSo = VmThrowFromVm(&(*pVm),"Error", \
			"Cannot use assign-op operators with string offsets", \
			sizeof("Cannot use assign-op operators with string offsets")-1); \
		if( _rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; } \
		rc = _rcSo; \
		PH7_THROW_ROUTE_MIDEXPR(rc) \
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
