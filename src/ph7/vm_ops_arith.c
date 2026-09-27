/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
#include <math.h>  /* pow */
/*
 * Section:
 *    Opcode handlers extracted from vm.c's dispatch loop. Each handler runs
 *    one opcode arm against the caller's VmExecState: the loop syncs pTos/pc
 *    in, calls the handler, reloads them and routes the returned VmOpRc onto
 *    its labels (same idiom as VmCallFinish).
 * Status:
 *    Stable.
 */
/* Bind the dispatch-routing exits to handler semantics (see vm_dispatch.h),
 * and map the macros' sState references onto our state parameter. */
#define VM_EXIT_BREAK      { pState->pTos = pTos; pState->pc = pc; return VM_OP_NEXT; }
#define VM_EXIT_ABORT      { pState->pTos = pTos; pState->pc = pc; return VM_OP_ABORT; }
#define VM_EXIT_EXCEPTION  { pState->pTos = pTos; pState->pc = pc; return VM_OP_EXCEPTION; }
#include "vm_dispatch.h"
#define sState (*pState)

/*
 * A native compare handler REFUSED the pair the operator just asked about
 * (php throws DateException comparing two different KINDS of DateTimeZone).
 * PH7_MemObjCmp only recorded it -- it has no throw boundary of its own -- so
 * the operator raises it here, where the expression's value would have landed:
 * both operands go, a null stands in for the result the way every other
 * mid-expression throw leaves one, and the status routes to the catching try.
 * Used by the four LOOSE comparison arms; the strict pair never asks a handler.
 */
#define VM_CMP_REFUSAL_ROUTE() \
	if( PH7_CmpRefusalPending(pVm) ){ \
		sxi32 _rcCmp; \
		VmPopOperand(&pTos,1); \
		PH7_MemObjRelease(pTos); \
		MemObjSetType(pTos,MEMOBJ_NULL); \
		pTos->nIdx = SXU32_HIGH; \
		_rcCmp = PH7_CmpRefusalRaise(pVm); \
		if( _rcCmp == SXERR_ABORT ){ VM_EXIT_ABORT; } \
		PH7_THROW_ROUTE_MIDEXPR(_rcCmp) \
	}

/*
 * OP_NULLC_STORE: body moved verbatim from the OP_NULLC_STORE arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpNullcStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	ph7_value *pObj;
	sxu32 nIdx;
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	if( SySetUsed(&pVm->aHookRmw) > 0 ){
		/* `$o->p ??= v` whose test value was null: the OP_MEMBER pushed a
		 * COAL entry targeting exactly THIS store (owner + pc identity — a
		 * stale entry from an abandoned statement can never match, and nested
		 * arms in the RHS were consumed/dropped above this one). Dispatch the
		 * set hook (COAL_HOOK) or __set (COAL_MAGIC — the band A #3b ??=
		 * residual: pre-fix the assign bypassed __set through the normal slot
		 * path). The RHS stays as the expression result. */
		VmHookRmw *pTop = (VmHookRmw *)SySetPeek(&pVm->aHookRmw);
		if( pTop->pOwnerStack == (void *)pStack && pTop->pInstrs == (void *)aInstr
		 && pTop->nPc == (sxu32)pc
		 && (pTop->iKind == VM_HOOK_PEND_COAL_HOOK || pTop->iKind == VM_HOOK_PEND_COAL_MAGIC) ){
			VmHookRmw sPend = *pTop;
			(void)SySetPop(&pVm->aHookRmw);
			if( sPend.iKind == VM_HOOK_PEND_COAL_HOOK ){
				sxi32 rcHs = VmHookSetDispatch(&(*pVm),sPend.pThis,sPend.pAttr,sPend.nBackIdx,pTos);
				if( rcHs == PH7_ABORT ){
					SyBlobRelease(&sPend.sName);
					PH7_ClassInstanceUnref(sPend.pThis);
					VM_EXIT_ABORT;
				}
			}else{
				SyString sSetName;
				SyStringInitFromBuf(&sSetName,SyBlobData(&sPend.sName),SyBlobLength(&sPend.sName));
				VmMagicSetDispatch(&(*pVm),sPend.pThis,&sSetName,pTos);
			}
			SyBlobRelease(&sPend.sName);
			PH7_ClassInstanceUnref(sPend.pThis);
			PH7_MemObjStore(pTos,pNos);
			pNos->nIdx = SXU32_HIGH;
			VmPopOperand(&pTos,1);
			VM_EXIT_BREAK;
		}
	}
	/* ArrayAccess null-coalesce-assign target: the preceding LOAD_IDX iP2=3
	 * armed pVm with the (object, key) on a missing key. Dispatch to
	 * offsetSet instead of writing through the synthetic pNos->nIdx. */
	if( pVm->bCoalesceArmed && pVm->pCoalesceObj ){
		ph7_class_instance *pInst = pVm->pCoalesceObj;
		ph7_class_method *pSet = PH7_ClassExtractMethod(pInst->pClass,
			"offsetSet",sizeof("offsetSet")-1);
		ph7_value *apArg[2];
		apArg[0] = &pVm->sCoalesceKey;
		apArg[1] = pTos;
		if( pSet == 0 ){
			/* A container that answered the READ and has nowhere to put the write:
			 * a class carrying a native dimension handler (ph7_class::xDim) and no
			 * ArrayAccess, which is php's DOMNodeList. The store is the same one
			 * `$list[9] = 'x'` performs, so it takes the same Error. */
			char zMsg[256];
			SyString *pName = &pInst->pClass->sName;
			sxu32 nMsg = SyBufferFormat(zMsg,sizeof(zMsg),
				"Cannot use object of type %.*s as array",
				(int)pName->nByte,pName->zString);
			rc = VmThrowFromVm(&(*pVm),"Error",zMsg,nMsg);
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			VmCoalesceDisarm(pVm);
			if( rc == SXERR_ABORT ){ VM_EXIT_ABORT; }
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);
		/* Leave RHS as the expression result (replace pNos with pTos). */
		PH7_MemObjStore(pTos,pNos);
		VmPopOperand(&pTos,1);
		/* Disarm and release the cached instance ref + key. */
		VmCoalesceDisarm(pVm);
		VM_EXIT_BREAK;
	}
	if( (pNos->iFlags & MEMOBJ_AUX_COALSTROFF) != 0 && pNos->x.pOther != 0 ){
		/* `$s[k] ??= v` on a STRING: php performs a real string-OFFSET store —
		 * `??=` is not an assign-op — padding with spaces when the offset is past
		 * the end. Writing the RHS through pNos->nIdx, which is all a string offset
		 * carries (the BASE VARIABLE's slot), REPLACED the whole string with it:
		 * `$s = "abc"; $s[9] ??= "z";` left $s === "z". The offset rides here on the
		 * peek's own result (the MEMOBJ_AUX_COALSTROFF carrier it owns, so nested
		 * `??=`s cannot clobber each other), and php re-resolves it LOUDLY here: an
		 * offset the quiet peek let through raises at the store. */
		VmCoalStrOff *pCoalOff = (VmCoalStrOff *)pNos->x.pOther;
		ph7_value *pStrBase = pNos->nIdx != SXU32_HIGH
			? (ph7_value *)SySetAt(&pVm->aMemObj,pNos->nIdx) : 0;
		sxi64 iOfft = 0;
		SyBlob sTypeMsg;
		int eOfft;
		SyBlobInit(&sTypeMsg,&pVm->sAllocator);
		eOfft = VmStringOffsetResolve(&(*pVm),&pCoalOff->sKey,VM_STROFF_LOUD,
			&iOfft,&sTypeMsg);
		if( eOfft == VM_STROFF_REJECT ){
			sxi32 rcSo;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcSo = VmThrowBuiltinError(&(*pVm),"TypeError",sizeof("TypeError")-1,&sTypeMsg);
			if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }
			rc = rcSo;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sTypeMsg);
		/* The RHS takes the same user-visible string coercion as a plain store. */
		{
			sxi32 rcSv = PH7_MemObjToStringUV(pTos);
			PH7_DISPATCH_TOSTRING_RC(rcSv)
		}
		if( pStrBase && (pStrBase->iFlags & MEMOBJ_STRING) ){
			if( VmStringOffsetWrite(&(*pVm),pStrBase,iOfft,pTos) != SXRET_OK ){
				sxi32 rcEm;
				VmPopOperand(&pTos,1);
				PH7_MemObjRelease(pTos);
				MemObjSetType(pTos,MEMOBJ_NULL);
				pTos->nIdx = SXU32_HIGH;
				rcEm = VmThrowFromVm(&(*pVm),"Error",
					"Cannot assign an empty string to a string offset",
					sizeof("Cannot assign an empty string to a string offset")-1);
				if( rcEm == SXERR_ABORT ){ VM_EXIT_ABORT; }
				rc = rcEm;
				PH7_THROW_ROUTE_MIDEXPR(rc)
			}
		}
		/* Done with the offset. PH7_MemObjStore only STRIPS the AUX flag, it does
		 * not free what the carrier owns, so release it here — every other exit
		 * from this arm routes through PH7_MemObjRelease, which does. */
		VmFreeCoalStrOff(pCoalOff);
		pNos->x.pOther = 0;
		pNos->iFlags &= ~MEMOBJ_AUX_COALSTROFF;
		/* Leave the RHS as the expression's value, like every other arm. */
		PH7_MemObjStore(pTos,pNos);
		pNos->nIdx = SXU32_HIGH;
		VmPopOperand(&pTos,1);
		VM_EXIT_BREAK;
	}
	nIdx = pNos->nIdx;
	if( nIdx == SXU32_HIGH ){
		/* A read-modify-write THROUGH a temporary (`f()[0] .= "x"`, `mk()->p += 1`):
		 * php computes it, drops it with the temporary and stays silent. Every case
		 * that IS a refusal — a class constant, a hooked or handler-backed property —
		 * is decided before the VM sees it. */
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,nIdx)) != 0 ){
		PH7_ENFORCE_TYPED_STORE(nIdx,pTos);
		PH7_MemObjStore(pTos,pObj);
	}
	PH7_MemObjStore(pTos,pNos);
	VmPopOperand(&pTos,1);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_DIV: body moved verbatim from the OP_DIV arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpDiv(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	/* php's do_operation: an operand whose class declares one decides the pair. */
	int rcNa;
	const char *zArCls = "TypeError";
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	{
		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,
		 * object or resource operand is a TypeError, not a silent 0. Settle the stack
		 * BEFORE throwing, so the catch does not run over the abandoned operands. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		rcNa = VmArithOperandStep(&(*pVm),pNos,pTos,"/",pNos,&zArCls,&sArMsg);
		if( rcNa == PH7_ARITH_REFUSED ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
	if( rcNa == PH7_ARITH_HANDLED ){
		VmPopOperand(&pTos,1);
		VM_EXIT_BREAK;
	}
	ph7_real a,b,r;
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	/* php's `/`: an int/int division whose remainder is 0 yields an *int*
	 * (6/3 === 2, not 2.0); anything else -- a float operand, an inexact
	 * quotient, or PHP_INT_MIN/-1 which does not fit -- yields a float.
	 * PH7 always produced a float and then called PH7_MemObjTryInteger, which
	 * ORs MEMOBJ_INT onto a value that keeps rendering as a float. */
	PH7_MemObjToNumeric(pTos);
	PH7_MemObjToNumeric(pNos);
	if( ((pTos->iFlags|pNos->iFlags) & MEMOBJ_REAL) == 0 ){
		sxi64 ia = pNos->x.iVal;
		sxi64 ib = pTos->x.iVal;
		sxi64 iQuot = 0;
		int bExact = 0;
		if( ib == 0 ){
			rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");
			PH7_DISPATCH_ENFORCE_RC(rc)
		}else if( ib == -1 ){
			/* `a / -1` is the exact int -a for every a but PHP_INT_MIN, whose
			 * magnitude does not fit -- that one leaves bExact clear and takes the
			 * float path below, as php does. The divisor has to be screened BEFORE
			 * `ia % ib` runs: x86 computes the overflowing quotient PHP_INT_MIN/-1
			 * alongside the remainder, so testing the remainder first trapped
			 * (SIGFPE) on exactly the value the guard was written to protect.
			 * OP_MOD screens the same hazard the same way. */
#ifdef PH7_OMIT_FLOATING_POINT
			/* The integer-only build has no float to promote to (as OP_ADD's
			 * overflow arm) and its `real` division is this same trapping integer
			 * one, so answer the wrapped quotient -- which is PHP_INT_MIN. */
			iQuot = ( ia != SMALLEST_INT64 ) ? -ia : SMALLEST_INT64;
			bExact = 1;
#else
			if( ia != SMALLEST_INT64 ){
				iQuot = -ia;
				bExact = 1;
			}
#endif
		}else if( ia % ib == 0 ){
			iQuot = ia / ib;
			bExact = 1;
		}
		if( bExact ){
			pNos->x.iVal = iQuot;
			MemObjSetType(pNos,MEMOBJ_INT);
			VmPopOperand(&pTos,1);
			VM_EXIT_BREAK;
		}
	}
	/* Force the operands to be real */
	if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){
		PH7_MemObjToReal(pTos);
	}
	if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){
		PH7_MemObjToReal(pNos);
	}
	/* Perform the requested operation */
	a = pNos->rVal;
	b = pTos->rVal;
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
	}
	VmPopOperand(&pTos,1);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_MOD_STORE: body moved verbatim from the OP_MOD_STORE arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpModStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	/* php's do_operation: an operand whose class declares one decides the pair. */
	int rcNa;
	const char *zArCls = "TypeError";
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	ph7_value *pObj;
	sxi64 a,b,r;
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	{
		/* php's operand contract: a compound-assign with a non-numeric string,
		 * array, object or resource operand is a TypeError too. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		rcNa = VmArithOperandStep(&(*pVm),pTos,pNos,"%",pNos,&zArCls,&sArMsg);
		if( rcNa == PH7_ARITH_REFUSED ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
	if( rcNa == PH7_ARITH_HANDLED ){
		goto mod_store_write;
	}
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
	if( b == 0 ){
		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),
		 * not the old non-catchable warning that continued with a 0 result. */
		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");
		PH7_DISPATCH_ENFORCE_RC(rc)
		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */
	}else if( b == -1 ){
		/* `a % -1` is 0 for every a; see OP_MOD — computing `a%b` would trap
		 * (SIGFPE on x86) for a == PHP_INT_MIN. php's result here is 0. */
		r = 0;
	}else{
		r = a%b;
	}
	/* Push the result */
	pNos->x.iVal = r;
	MemObjSetType(pNos,MEMOBJ_INT);
mod_store_write:
	if( pTos->nIdx == SXU32_HIGH ){
		/* A read-modify-write THROUGH a temporary: php drops it in silence. */
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);
		PH7_MemObjStore(pNos,pObj);
	}
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);
	VmPopOperand(&pTos,1);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_MOD: body moved verbatim from the OP_MOD arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpMod(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	/* php's do_operation: an operand whose class declares one decides the pair. */
	int rcNa;
	const char *zArCls = "TypeError";
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	{
		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,
		 * object or resource operand is a TypeError, not a silent 0. Settle the stack
		 * BEFORE throwing, so the catch does not run over the abandoned operands. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		rcNa = VmArithOperandStep(&(*pVm),pNos,pTos,"%",pNos,&zArCls,&sArMsg);
		if( rcNa == PH7_ARITH_REFUSED ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
	if( rcNa == PH7_ARITH_HANDLED ){
		VmPopOperand(&pTos,1);
		VM_EXIT_BREAK;
	}
	sxi64 a,b,r;
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
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
	if( b == 0 ){
		/* Modulo by zero: php throws a catchable DivisionByZeroError (8.0),
		 * not the old non-catchable warning that continued with a 0 result. */
		rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Modulo by zero");
		PH7_DISPATCH_ENFORCE_RC(rc)
		r = 0; /* unreachable — ENFORCE_RC jumps/breaks for a throw */
	}else if( b == -1 ){
		/* `a % -1` is 0 for every a. Computing it as `a%b` would be a signed
		 * -overflow trap (SIGFPE on x86) when a == PHP_INT_MIN, since the CPU
		 * evaluates the overflowing quotient PHP_INT_MIN/-1 alongside the
		 * remainder. php's result here is 0. */
		r = 0;
	}else{
		r = a%b;
	}
	/* Push the result */
	pNos->x.iVal = r;
	MemObjSetType(pNos,MEMOBJ_INT);
	VmPopOperand(&pTos,1);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_SUB_STORE: body moved verbatim from the OP_SUB_STORE arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpSubStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	/* php's do_operation: an operand whose class declares one decides the pair. */
	int rcNa;
	const char *zArCls = "TypeError";
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	ph7_value *pObj;
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	{
		/* php's operand contract: a compound-assign with a non-numeric string,
		 * array, object or resource operand is a TypeError too. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		rcNa = VmArithOperandStep(&(*pVm),pTos,pNos,"-",pNos,&zArCls,&sArMsg);
		if( rcNa == PH7_ARITH_REFUSED ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
	if( rcNa == PH7_ARITH_HANDLED ){
		goto sub_store_write;
	}
	/* Force the operands to be numeric (see OP_SUB) */
	PH7_MemObjToNumeric(pTos);
	PH7_MemObjToNumeric(pNos);
	if( MEMOBJ_REAL & (pTos->iFlags|pNos->iFlags) ){
		/* Floating point arithemic */
		ph7_real a,b,r;
		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){
			PH7_MemObjToReal(pTos);
		}
		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){
			PH7_MemObjToReal(pNos);
		}
		a = pTos->rVal;
		b = pNos->rVal;
		r = a - b;
		/* Push the result */
		pNos->rVal = r;
		MemObjSetType(pNos,MEMOBJ_REAL);
		/* Try to get an integer representation */
		PH7_MemObjTryInteger(pNos);
	}else{
		/* Integer arithmetic; PHP promotes an overflowing difference to float.
		 * The integer-only build wraps like OP_POW's OMIT path. */
		sxi64 a,b,r;
		a = pTos->x.iVal;
		b = pNos->x.iVal;
		if( PH7_SUB_OVERFLOW64(a,b,&r) ){
#ifndef PH7_OMIT_FLOATING_POINT
			pNos->rVal = (ph7_real)a - (ph7_real)b;
			MemObjSetType(pNos,MEMOBJ_REAL);
#else
			pNos->x.iVal = r;
			MemObjSetType(pNos,MEMOBJ_INT);
#endif
		}else{
			pNos->x.iVal = r;
			MemObjSetType(pNos,MEMOBJ_INT);
		}
	}
sub_store_write:
	if( pTos->nIdx == SXU32_HIGH ){
		/* A read-modify-write THROUGH a temporary: php drops it in silence. */
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);
		PH7_MemObjStore(pNos,pObj);
	}
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);
	VmPopOperand(&pTos,1);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_SUB: body moved verbatim from the OP_SUB arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpSub(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	/* php's do_operation: an operand whose class declares one decides the pair. */
	int rcNa;
	const char *zArCls = "TypeError";
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	{
		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,
		 * object or resource operand is a TypeError, not a silent 0. Settle the stack
		 * BEFORE throwing, so the catch does not run over the abandoned operands. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		rcNa = VmArithOperandStep(&(*pVm),pNos,pTos,"-",pNos,&zArCls,&sArMsg);
		if( rcNa == PH7_ARITH_REFUSED ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
	if( rcNa == PH7_ARITH_HANDLED ){
		VmPopOperand(&pTos,1);
		VM_EXIT_BREAK;
	}
	/* Force the operands to be numeric. Without this a string operand fell through
	 * to the integer branch below, which read the raw x.iVal union member: "10" - "4"
	 * quietly evaluated to 0. */
	PH7_MemObjToNumeric(pTos);
	PH7_MemObjToNumeric(pNos);
	if( MEMOBJ_REAL & (pTos->iFlags|pNos->iFlags) ){
		/* Floating point arithemic */
		ph7_real a,b,r;
		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){
			PH7_MemObjToReal(pTos);
		}
		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){
			PH7_MemObjToReal(pNos);
		}
		a = pNos->rVal;
		b = pTos->rVal;
		r = a - b;
		/* Push the result */
		pNos->rVal = r;
		MemObjSetType(pNos,MEMOBJ_REAL);
		/* Try to get an integer representation */
		PH7_MemObjTryInteger(pNos);
	}else{
		/* Integer arithmetic; PHP promotes an overflowing difference to float.
		 * The integer-only build wraps like OP_POW's OMIT path. */
		sxi64 a,b,r;
		a = pNos->x.iVal;
		b = pTos->x.iVal;
		if( PH7_SUB_OVERFLOW64(a,b,&r) ){
#ifndef PH7_OMIT_FLOATING_POINT
			pNos->rVal = (ph7_real)a - (ph7_real)b;
			MemObjSetType(pNos,MEMOBJ_REAL);
#else
			pNos->x.iVal = r;
			MemObjSetType(pNos,MEMOBJ_INT);
#endif
		}else{
			pNos->x.iVal = r;
			MemObjSetType(pNos,MEMOBJ_INT);
		}
	}
	VmPopOperand(&pTos,1);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * php's `**`, as a value operation.
 *
 * In php pow() IS the exponentiation operator -- both compile to the same
 * ZEND_API pow_function -- so the operand contract, the int-stays-int rule and
 * every edge value have to come from ONE place here too. This is that place:
 * OP_POW/OP_POW_STORE below and PH7_builtin_pow() in builtin_math.c both call
 * it, after the caller has run VmArithOperandCheck() over the two operands (the
 * two sites word their throw differently -- one settles an operand stack first,
 * the other is inside a C builtin -- so the CHECK stays with the caller and only
 * the arithmetic is shared).
 *
 * pBase and pExp are converted in place, which is what the opcode arm already
 * did to its stack slots; pOut may alias either of them and is written last.
 */
PH7_PRIVATE void PH7_MemObjPow(ph7_value *pBase,ph7_value *pExp,ph7_value *pOut)
{
#ifndef PH7_OMIT_FLOATING_POINT
	int bBothInt;
	int usedInt = 0;
	ph7_real a, b, r;
#endif
	sxi64 base_i = 0, exp_i = 0;
	PH7_MemObjToNumeric(pBase);
	PH7_MemObjToNumeric(pExp);
#ifndef PH7_OMIT_FLOATING_POINT
	bBothInt = ((pBase->iFlags & MEMOBJ_REAL) == 0) &&
	           ((pExp->iFlags & MEMOBJ_REAL) == 0);
	if( bBothInt ){
		base_i = pBase->x.iVal;
		exp_i  = pExp->x.iVal;
	}
	if( (pBase->iFlags & MEMOBJ_REAL) == 0 ){
		PH7_MemObjToReal(pBase);
	}
	if( (pExp->iFlags & MEMOBJ_REAL) == 0 ){
		PH7_MemObjToReal(pExp);
	}
	a = pBase->rVal;
	b = pExp->rVal;
	r = pow(a, b);
	/* int ** non-negative int is php's OWN loop, not a call to pow(), and the
	 * difference is visible in the answer twice over. php multiplies in
	 * `pow_function_base`'s doubling loop and, the moment a step OVERFLOWS, finishes
	 * in DOUBLE space FROM THERE — `dval * pow(l2, i)` with whatever exponent is
	 * left — rather than re-computing pow(base, exp) from the original operands.
	 * That carries the accumulated SIGN, so `(-3) ** PHP_INT_MAX` is -INF where
	 * pow() answers +INF, and it rounds differently, so `3 ** 100` is
	 * 5.1537752073201141e+47 where pow() gives ...132e+47 and `10 ** 64` is
	 * 1.0000000000000002e+64 where pow() gives exactly 1e+64. 33 rows of a 380-row
	 * base×exponent sweep were wrong, sign included.
	 * The overflowing product is the DOUBLE product of the two operands, which is
	 * what php's ZEND_SIGNED_MULTIPLY_LONG hands back wherever it detects the
	 * overflow with a builtin (gcc/clang) or with _mul128 (MSVC) — the two
	 * platforms PHL builds on. */
	if( bBothInt && exp_i >= 0 ){
		sxi64 l1 = 1, l2 = base_i, i = exp_i, iProd;
		if( i == 0 ){
			/* Anything to the 0 is int 1 — php answers before it looks at the base. */
			pOut->x.iVal = 1;
			MemObjSetType(pOut, MEMOBJ_INT);
			usedInt = 1;
		}else if( l2 == 0 ){
			pOut->x.iVal = 0;
			MemObjSetType(pOut, MEMOBJ_INT);
			usedInt = 1;
		}else{
			while( i >= 1 ){
				if( i % 2 ){
					--i;
					if( PH7_MUL_OVERFLOW64(l1, l2, &iProd) ){
						r = ((ph7_real)l1 * (ph7_real)l2) * pow((ph7_real)l2,(ph7_real)i);
						break;
					}
					l1 = iProd;
				}else{
					i /= 2;
					if( PH7_MUL_OVERFLOW64(l2, l2, &iProd) ){
						r = (ph7_real)l1 * pow((ph7_real)l2 * (ph7_real)l2,(ph7_real)i);
						break;
					}
					l2 = iProd;
				}
				if( i == 0 ){
					pOut->x.iVal = l1;
					MemObjSetType(pOut, MEMOBJ_INT);
					usedInt = 1;
					break;
				}
			}
		}
	}
	if( !usedInt ){
		pOut->rVal = r;
		MemObjSetType(pOut, MEMOBJ_REAL);
	}
#else
	/* PH7_OMIT_FLOATING_POINT: integer-only build. No libm / no pow().
	 * Exponentiation by squaring with silent wrap on overflow, matching
	 * the integer-wrap semantics of PH7_OP_MUL in the same build mode.
	 * Negative exponents yield 0 since fractional results cannot be
	 * represented. */
	base_i = pBase->x.iVal;
	exp_i  = pExp->x.iVal;
	{
		sxi64 result_i = 1;
		sxi64 cur_base = base_i;
		sxi64 cur_exp  = exp_i;
		if( cur_exp < 0 ){
			result_i = 0;
		}else{
			while( cur_exp > 0 ){
				if( cur_exp & 1 ){
					result_i *= cur_base;
				}
				cur_exp >>= 1;
				if( cur_exp > 0 ){
					cur_base *= cur_base;
				}
			}
		}
		pOut->x.iVal = result_i;
		MemObjSetType(pOut, MEMOBJ_INT);
	}
#endif /* PH7_OMIT_FLOATING_POINT */
}
/*
 * OP_POW_STORE: body moved verbatim from the OP_POW_STORE arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpPowStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	/* php's do_operation: an operand whose class declares one decides the pair. */
	int rcNa;
	const char *zArCls = "TypeError";
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);
	/* Operand order convention (matches DIV/SUB_STORE):
	 *   POW:       base = pNos (evaluated first),   exp = pTos
	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos
	 */
	ph7_value *pBase = bStore ? pTos : pNos;
	ph7_value *pExp  = bStore ? pNos : pTos;
	{
		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,
		 * object or resource operand is a TypeError, not a silent 0. Settle the stack
		 * BEFORE throwing, so the catch does not run over the abandoned operands.
		 * The message names the operands in SOURCE order, so it takes the same
		 * base/exponent convention as the operation: `$x **= "abc"` is
		 * `int ** string`, the way `$x ** "abc"` is. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		rcNa = VmArithOperandStep(&(*pVm),pBase,pExp,"**",pNos,&zArCls,&sArMsg);
		if( rcNa == PH7_ARITH_REFUSED ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	if( rcNa == PH7_ARITH_ORDINARY ){
		PH7_MemObjPow(pBase,pExp,pNos);
	}
	if( bStore ){
		ph7_value *pObj;
		if( pTos->nIdx == SXU32_HIGH ){
			/* A read-modify-write THROUGH a temporary: php drops it in silence. */
		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);
			PH7_MemObjStore(pNos,pObj);
		}
	}
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);
	VmPopOperand(&pTos,1);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_SPACESHIP: body moved verbatim from the OP_SPACESHIP arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpSpaceship(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	/* php answers the UNORDERED comparison -- a NaN, or two arrays neither of
	 * which contains the other -- with 1 whichever way round it is asked, and
	 * PH7_MemObjCmp does the same, so the spaceship needs no case of its own. */
	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);
	VM_CMP_REFUSAL_ROUTE()
	rc = (rc > 0) - (rc < 0);   /* normalize to exactly -1, 0 or 1 */
	VmPopOperand(&pTos,1);
	PH7_MemObjRelease(pTos);
	pTos->x.iVal = rc;
	MemObjSetType(pTos,MEMOBJ_INT);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_GE: body moved verbatim from the OP_GE arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpGe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	/* Perform the comparison and act accordingly */
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	/* `$a > $b` is php's `$b < $a` -- asked from the OTHER SIDE, not read off
	 * this side's sign. The two differ exactly where the comparison is
	 * UNORDERED and php answers 1 both ways (a NaN against a number or a
	 * string; two same-sized arrays neither of which contains the other): a
	 * greater-than read off `rc > 0` calls both of those TRUE, where php --
	 * asking `$b < $a` and getting 1 again -- calls them false, as it does
	 * every other relational operator on such a pair. */
	rc = PH7_MemObjCmp(pTos,pNos,FALSE,0);
	VM_CMP_REFUSAL_ROUTE()
	if( pInstr->iOp == PH7_OP_GE ){
		rc = rc <= 0;
	}else{
		rc = rc < 0;
	}
	VmPopOperand(&pTos,1);
	if( !pInstr->iP2 ){
		/* Push comparison result without taking the jump */
		PH7_MemObjRelease(pTos);
		pTos->x.iVal = rc;
		/* Invalidate any prior representation */
		MemObjSetType(pTos,MEMOBJ_BOOL);
	}else{
		if( rc ){
			/* Jump to the desired location */
			pc = pInstr->iP2 - 1;
			VmPopOperand(&pTos,1);
		}
	}
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_LE: body moved verbatim from the OP_LE arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpLe(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	/* Perform the comparison and act accordingly */
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	/* An unordered pair answers 1 here too, so both spellings are false for it
	 * without a case of their own (see OP_GT/OP_GE above). */
	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);
	VM_CMP_REFUSAL_ROUTE()
	if( pInstr->iOp == PH7_OP_LE ){
		rc = rc < 1;
	}else{
		rc = rc < 0;
	}
	VmPopOperand(&pTos,1);
	if( !pInstr->iP2 ){
		/* Push comparison result without taking the jump */
		PH7_MemObjRelease(pTos);
		pTos->x.iVal = rc;
		/* Invalidate any prior representation */
		MemObjSetType(pTos,MEMOBJ_BOOL);
	}else{
		if( rc ){
			/* Jump to the desired location */
			pc = pInstr->iP2 - 1;
			VmPopOperand(&pTos,1);
		}
	}
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_TNE: body moved verbatim from the OP_TNE arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpTne(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	/* Perform the comparison and act accordingly */
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);
	rc = rc != 0;
	VmPopOperand(&pTos,1);
	if( !pInstr->iP2 ){
		/* Push comparison result without taking the jump */
		PH7_MemObjRelease(pTos);
		pTos->x.iVal = rc;
		/* Invalidate any prior representation */
		MemObjSetType(pTos,MEMOBJ_BOOL);
	}else{
		if( rc ){
			/* Jump to the desired location */
			pc = pInstr->iP2 - 1;
			VmPopOperand(&pTos,1);
		}
	}
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_TEQ: body moved verbatim from the OP_TEQ arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpTeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	/* Perform the comparison and act accordingly */
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	/* `NAN === NAN` is false in php, and the comparator says so: an unordered
	 * pair is 1, never 0. */
	rc = PH7_MemObjCmp(pNos,pTos,TRUE,0);
	rc = rc == 0;
	VmPopOperand(&pTos,1);
	if( !pInstr->iP2 ){
		/* Push comparison result without taking the jump */
		PH7_MemObjRelease(pTos);
		pTos->x.iVal = rc;
		/* Invalidate any prior representation */
		MemObjSetType(pTos,MEMOBJ_BOOL);
	}else{
		if( rc ){
			/* Jump to the desired location */
			pc = pInstr->iP2 - 1;
			VmPopOperand(&pTos,1);
		}
	}
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_NEQ: body moved verbatim from the OP_NEQ arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpNeq(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	/* Perform the comparison and act accordingly */
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	rc = PH7_MemObjCmp(pNos,pTos,FALSE,0);
	VM_CMP_REFUSAL_ROUTE()
	if( pInstr->iOp == PH7_OP_EQ ){
		rc = rc == 0;
	}else{
		rc = rc != 0;
	}
	VmPopOperand(&pTos,1);
	if( !pInstr->iP2 ){
		/* Push comparison result without taking the jump */
		PH7_MemObjRelease(pTos);
		pTos->x.iVal = rc;
		/* Invalidate any prior representation */
		MemObjSetType(pTos,MEMOBJ_BOOL);
	}else{
		if( rc ){
			/* Jump to the desired location */
			pc = pInstr->iP2 - 1;
			VmPopOperand(&pTos,1);
		}
	}
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_LOR: body moved verbatim from the OP_LOR arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpLor(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	sxi32 v1, v2;    /* 0==TRUE, 1==FALSE, 2==UNKNOWN or NULL */
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	/* Force a boolean cast */
	if((pTos->iFlags & MEMOBJ_BOOL) == 0 ){
		PH7_MemObjToBool(pTos);
	}
	if((pNos->iFlags & MEMOBJ_BOOL) == 0 ){
		PH7_MemObjToBool(pNos);
	}
	v1 = pNos->x.iVal == 0 ? 1 : 0;
	v2 = pTos->x.iVal == 0 ? 1 : 0;
	if( pInstr->iOp == PH7_OP_LAND ){
		static const unsigned char and_logic[] = { 0, 1, 2, 1, 1, 1, 2, 1, 2 };
		v1 = and_logic[v1*3+v2];
	}else{
		static const unsigned char or_logic[] = { 0, 0, 0, 0, 1, 2, 0, 2, 2 };
		v1 = or_logic[v1*3+v2];
	}
	if( v1 == 2 ){
		v1 = 1;
	}
	VmPopOperand(&pTos,1);
	pTos->x.iVal = v1 == 0 ? 1 : 0;
	MemObjSetType(pTos,MEMOBJ_BOOL);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_SHR_STORE: body moved verbatim from the OP_SHR_STORE arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpShrStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	ph7_value *pObj;
	sxi64 a,r;
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	/* (The string-offset lvalue rejection happens in the dispatch arm that calls
	 * this handler, beside the other eleven compound stores.) */
	PH7_SHIFT_ARITH_CONTRACT(pTos,pNos)
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
	PH7_SHIFT_COUNT_RULES(pNos->x.iVal,a,r,pInstr->iOp == PH7_OP_SHL_STORE)
	/* Push the result */
	pNos->x.iVal = r;
	MemObjSetType(pNos,MEMOBJ_INT);
	if( pTos->nIdx == SXU32_HIGH ){
		/* A read-modify-write THROUGH a temporary: php drops it in silence. */
	}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
		PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);
		PH7_MemObjStore(pNos,pObj);
	}
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);
	VmPopOperand(&pTos,1);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_SHR: body moved verbatim from the OP_SHR arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpShr(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	sxi64 a,r;
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	PH7_SHIFT_ARITH_CONTRACT(pNos,pTos)
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
	PH7_SHIFT_COUNT_RULES(pTos->x.iVal,a,r,pInstr->iOp == PH7_OP_SHL)
	/* Push the result */
	pNos->x.iVal = r;
	MemObjSetType(pNos,MEMOBJ_INT);
	VmPopOperand(&pTos,1);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_MUL_STORE: body moved verbatim from the OP_MUL_STORE arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpMulStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	/* php's do_operation: an operand whose class declares one decides the pair. */
	int rcNa;
	const char *zArCls = "TypeError";
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pVm); SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	{
		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,
		 * object or resource operand is a TypeError, not a silent 0. Settle the stack
		 * BEFORE throwing, so the catch does not run over the abandoned operands.
		 * MULTIPLICATION is commutative and the RESULT does not care which operand is
		 * which, but the MESSAGE does: it names them in SOURCE order, and a compound
		 * assign puts its lvalue on the TOP of the stack (`$x *= [1]` is `int * array`,
		 * the way `$x * [1]` is). */
		ph7_value *pMulL = (pInstr->iOp == PH7_OP_MUL_STORE) ? pTos : pNos;
		ph7_value *pMulR = (pInstr->iOp == PH7_OP_MUL_STORE) ? pNos : pTos;
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		rcNa = VmArithOperandStep(&(*pVm),pMulL,pMulR,"*",pNos,&zArCls,&sArMsg);
		if( rcNa == PH7_ARITH_REFUSED ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),zArCls,(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
	if( rcNa == PH7_ARITH_HANDLED ){
		goto mul_store_write;
	}
	/* Force the operand to be numeric */
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	PH7_MemObjToNumeric(pTos);
	PH7_MemObjToNumeric(pNos);
	/* Perform the requested operation */
	if( MEMOBJ_REAL & (pTos->iFlags|pNos->iFlags) ){
		/* Floating point arithemic */
		ph7_real a,b,r;
		if( (pTos->iFlags & MEMOBJ_REAL) == 0 ){
			PH7_MemObjToReal(pTos);
		}
		if( (pNos->iFlags & MEMOBJ_REAL) == 0 ){
			PH7_MemObjToReal(pNos);
		}
		a = pNos->rVal;
		b = pTos->rVal;
		r = a * b;
		/* Push the result */
		pNos->rVal = r;
		MemObjSetType(pNos,MEMOBJ_REAL);
		/* Try to get an integer representation */
		PH7_MemObjTryInteger(pNos);
	}else{
		/* Integer arithmetic; PHP promotes an overflowing product to float.
		 * The integer-only build wraps like OP_POW's OMIT path. */
		sxi64 a,b,r;
		a = pNos->x.iVal;
		b = pTos->x.iVal;
		if( PH7_MUL_OVERFLOW64(a,b,&r) ){
#ifndef PH7_OMIT_FLOATING_POINT
			pNos->rVal = (ph7_real)a * (ph7_real)b;
			MemObjSetType(pNos,MEMOBJ_REAL);
#else
			pNos->x.iVal = r;
			MemObjSetType(pNos,MEMOBJ_INT);
#endif
		}else{
			pNos->x.iVal = r;
			MemObjSetType(pNos,MEMOBJ_INT);
		}
	}
mul_store_write:
	if( pInstr->iOp == PH7_OP_MUL_STORE ){
		ph7_value *pObj;
		if( pTos->nIdx == SXU32_HIGH ){
			/* A read-modify-write THROUGH a temporary: php drops it in silence. */
		}else if( (pObj = (ph7_value *)SySetAt(&pVm->aMemObj,pTos->nIdx)) != 0 ){
			PH7_ENFORCE_TYPED_STORE(pTos->nIdx,pNos);
			PH7_MemObjStore(pNos,pObj);
		}
	}
	PH7_HOOK_RMW_WRITEBACK(pTos->nIdx,0);
	VmPopOperand(&pTos,1);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}
