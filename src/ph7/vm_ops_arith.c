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
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
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
		if( pSet ){
			PH7_VmCallClassMethod(&(*pVm),pInst,pSet,0,2,apArg);
		}
		/* Leave RHS as the expression result (replace pNos with pTos). */
		PH7_MemObjStore(pTos,pNos);
		VmPopOperand(&pTos,1);
		/* Disarm and release the cached instance ref + key. */
		VmCoalesceDisarm(pVm);
		VM_EXIT_BREAK;
	}
	nIdx = pNos->nIdx;
	if( nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,
			"Cannot perform assignment on a constant class attribute");
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
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	{
		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,
		 * object or resource operand is a TypeError, not a silent 0. Settle the stack
		 * BEFORE throwing, so the catch does not run over the abandoned operands. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"/",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
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
		if( ib == 0 ){
			rc = VmThrowFixedError(&(*pVm),"DivisionByZeroError","Division by zero");
			PH7_DISPATCH_ENFORCE_RC(rc)
		}else if( ia % ib == 0 && !(ib == -1 && ia == SMALLEST_INT64) ){
			pNos->x.iVal = ia / ib;
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
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
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
		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"%",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
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
	if( pTos->nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
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
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	{
		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,
		 * object or resource operand is a TypeError, not a silent 0. Settle the stack
		 * BEFORE throwing, so the catch does not run over the abandoned operands. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"%",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
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
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
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
		if( VmArithOperandCheck(&(*pVm),pTos,pNos,"-",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
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
	if( pTos->nIdx == SXU32_HIGH ){
		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
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
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
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
		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"-",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
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
 * OP_POW_STORE: body moved verbatim from the OP_POW_STORE arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpPowStore(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_value *pNos = &pTos[-1];
	{
		/* php's operand contract (VmArithOperandCheck): a non-numeric string, array,
		 * object or resource operand is a TypeError, not a silent 0. Settle the stack
		 * BEFORE throwing, so the catch does not run over the abandoned operands. */
		SyBlob sArMsg;
		SyBlobInit(&sArMsg,&pVm->sAllocator);
		if( VmArithOperandCheck(&(*pVm),pNos,pTos,"**",&sArMsg) != SXRET_OK ){
			sxi32 rcAr;
			VmPopOperand(&pTos,1);
			PH7_MemObjRelease(pTos);
			MemObjSetType(pTos,MEMOBJ_NULL);
			pTos->nIdx = SXU32_HIGH;
			rcAr = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sArMsg),
				SyBlobLength(&sArMsg));
			SyBlobRelease(&sArMsg);
			if( rcAr == SXERR_ABORT ){ VM_EXIT_ABORT; }
			rc = rcAr;
			PH7_THROW_ROUTE_MIDEXPR(rc)
		}
		SyBlobRelease(&sArMsg);
	}
	int bStore = (pInstr->iOp == PH7_OP_POW_STORE);
	/* Operand order convention (matches DIV/SUB_STORE):
	 *   POW:       base = pNos (evaluated first),   exp = pTos
	 *   POW_STORE: base = pTos (lvalue, last),       exp = pNos
	 */
	ph7_value *pBase = bStore ? pTos : pNos;
	ph7_value *pExp  = bStore ? pNos : pTos;
#ifndef PH7_OMIT_FLOATING_POINT
	int bBothInt;
	int usedInt = 0;
	ph7_real a, b, r;
#endif
	sxi64 base_i = 0, exp_i = 0;
#ifdef UNTRUST
	if( pNos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	PH7_MemObjToNumeric(pTos);
	PH7_MemObjToNumeric(pNos);
#ifndef PH7_OMIT_FLOATING_POINT
	bBothInt = ((pTos->iFlags & MEMOBJ_REAL) == 0) &&
	           ((pNos->iFlags & MEMOBJ_REAL) == 0);
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
	/* Match PHP: int**non-negative-int stays int when the exact result
	 * fits in sxi64. Use exponentiation by squaring with overflow checks
	 * rather than casting the double back, because the boundary 2^63 is
	 * representable as double but not as signed int64. */
	if( bBothInt && exp_i >= 0 ){
		sxi64 result_i = 1;
		sxi64 cur_base = base_i;
		sxi64 cur_exp  = exp_i;
		int overflow = 0;
		while( cur_exp > 0 ){
			if( cur_exp & 1 ){
				if( PH7_MUL_OVERFLOW64(result_i, cur_base, &result_i) ){
					overflow = 1;
					break;
				}
			}
			cur_exp >>= 1;
			if( cur_exp > 0 ){
				if( PH7_MUL_OVERFLOW64(cur_base, cur_base, &cur_base) ){
					overflow = 1;
					break;
				}
			}
		}
		if( !overflow ){
			pNos->x.iVal = result_i;
			MemObjSetType(pNos, MEMOBJ_INT);
			usedInt = 1;
		}
	}
	if( !usedInt ){
		pNos->rVal = r;
		MemObjSetType(pNos, MEMOBJ_REAL);
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
		pNos->x.iVal = result_i;
		MemObjSetType(pNos, MEMOBJ_INT);
	}
#endif /* PH7_OMIT_FLOATING_POINT */
	if( bStore ){
		ph7_value *pObj;
		if( pTos->nIdx == SXU32_HIGH ){
			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Cannot perform assignment on a constant class attribute");
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
