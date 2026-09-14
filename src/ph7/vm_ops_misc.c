/**
 * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
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
 * OP_CONSUME: body moved verbatim from the OP_CONSUME arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpConsume(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_output_consumer *pCons = &pVm->sVmConsumer;
	ph7_value *pCur,*pOut = pTos;

	pOut = &pTos[-pInstr->iP1 + 1];
	pCur = pOut;
	/* Start the consume process  */
	while( pOut <= pTos ){
		/* Force a string cast */
		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){
			PH7_MemObjToString(pOut);
		}
		if( SyBlobLength(&pOut->sBlob) > 0 ){
			/*SyBlobNullAppend(&pOut->sBlob);*/
			/* Invoke the output consumer callback */
			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);
			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));
			SyBlobRelease(&pOut->sBlob);
			if( rc == SXERR_ABORT ){
				/* Output consumer callback request an operation abort. */
				VM_EXIT_ABORT;
			}
		}
		pOut++;
	}
	pTos = &pCur[-1];
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_MATCH: body moved verbatim from the OP_MATCH arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpMatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_match *pMatch = (ph7_match *)pInstr->p3;
	ph7_match_arm *aArm,*pArm,*pDefault = 0;
	ph7_value sSubject,sCond,sResult;
	sxu32 i,j,nArm,nCond;
	int matched = 0;
#ifdef UNTRUST
	if( pMatch == 0 || pTos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);
	nArm = SySetUsed(&pMatch->aArms);
	PH7_MemObjInit(pVm,&sSubject);
	PH7_MemObjInit(pVm,&sCond);
	PH7_MemObjInit(pVm,&sResult);
	PH7_MemObjLoad(pTos,&sSubject);
	for( i = 0; i < nArm && !matched; ++i ){
		pArm = &aArm[i];
		if( pArm->bDefault ){
			pDefault = pArm;
			continue;
		}
		nCond = SySetUsed(&pArm->aConds);
		for( j = 0; j < nCond; ++j ){
			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);
			if( pCondBc == 0 ){
				continue;
			}
			VmLocalExec(pVm,pCondBc,&sCond,FALSE);
			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);
			PH7_MemObjRelease(&sCond);
			if( rc == 0 ){
				VmLocalExec(pVm,&pArm->aResult,&sResult,FALSE);
				matched = 1;
				break;
			}
		}
	}
	if( !matched && pDefault ){
		VmLocalExec(pVm,&pDefault->aResult,&sResult,FALSE);
		matched = 1;
	}
	if( !matched ){
		const char *zType = "unknown";
		char zMsg[128];
		sxu32 nMsg;
		switch(sSubject.iFlags & MEMOBJ_ALL){
		case MEMOBJ_NULL:   zType = "null";   break;
		case MEMOBJ_BOOL:   zType = "bool";   break;
		case MEMOBJ_INT:    zType = "int";    break;
		case MEMOBJ_REAL:   zType = "float";  break;
		case MEMOBJ_STRING: zType = "string"; break;
		case MEMOBJ_HASHMAP:zType = "array";  break;
		case MEMOBJ_OBJ:    zType = "object"; break;
		case MEMOBJ_RES:    zType = "resource"; break;
		default: break;
		}
		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),
			"Unhandled match case of type %s",zType);
		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",
			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);
		PH7_MemObjRelease(&sSubject);
		PH7_MemObjRelease(&sResult);
		VM_EXIT_ABORT;
	}
	PH7_MemObjRelease(&sSubject);
	/* Replace subject on TOS with the arm result */
	PH7_MemObjStore(&sResult,pTos);
	PH7_MemObjRelease(&sResult);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_THROW: body moved verbatim from the OP_THROW arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpThrow(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	VmFrame *pFrameLocal = pVm->pFrame;
	sxu32 nJump = pInstr->iP2;
#ifdef UNTRUST
	if( pTos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);
	/* Tell the upper layer that an exception was thrown */
	pFrameLocal->iFlags |= VM_FRAME_THROW;
	if( pTos->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;
		ph7_class *pThrowable;
		/* Thrown object must implement the Throwable interface (PHP 7+). */
		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);
		if( pThrowable == 0 || !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){
			/* Not a Throwable: replace with Error(msg) matching PHP behavior.
			 * Error::__construct is defined in the built-in library and
			 * cannot realistically fail, so we do not check its return. */
			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);
			ph7_class_instance *pErrInst = 0;
			if( pErrorClass ){
				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);
			}
			if( pErrInst ){
				ph7_class_method *pCons;
				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);
				if( pCons ){
					ph7_value sArg;
					ph7_value *apArg[1];
					SyString sMsgStr;
					static const char zErrMsg[] =
						"Cannot throw objects that do not implement Throwable";
					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);
					PH7_MemObjInit(pVm,&sArg);
					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);
					apArg[0] = &sArg;
					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);
					PH7_MemObjRelease(&sArg);
				}
				rc = VmThrowException(&(*pVm),pErrInst);
				PH7_ClassInstanceUnref(pErrInst);
				if( rc == SXERR_ABORT ){
					VM_EXIT_ABORT;
				}
			}else{
				/* Bootstrap failure — fall back to uncaught reporting */
				rc = VmUncaughtException(&(*pVm),pThis);
				if( rc == SXERR_ABORT ){
					VM_EXIT_ABORT;
				}
			}
		}else{
			/* Throw the exception */
			rc = VmThrowException(&(*pVm),pThis);
			if( rc == SXERR_ABORT ){
				/* Abort processing immediately */
				VM_EXIT_ABORT;
			}
		}
	}else{
		/* Expecting a class instance */
		VmUncaughtException(&(*pVm),0);
		/* Pre-split latent bug preserved verbatim: the original arm discarded
		 * VmUncaughtException's status and tested the dispatch loop's STALE rc
		 * (whatever the previous instruction left), which in practice was never
		 * SXERR_ABORT — so this branch effectively never aborted. Keep that
		 * de-facto behavior deterministic here; the real fix (testing the
		 * call's own status) is a recorded correctness follow-up. */
		rc = SXRET_OK;
		if( rc == SXERR_ABORT ){
			/* Abort processing immediately */
			VM_EXIT_ABORT;
		}
	}
	/* Pop the top entry */
	VmPopOperand(&pTos,1);
	/* ROOT C: throw caught by an inline try in THIS exec -> jump to its catch/finally
	 * (draining any mid-expression operands back to the try's base). */
	PH7_INLINE_RESUME_BREAK()
	/* pInlineInstr still set here means an inline try in an OUTER exec caught it (e.g. a
	 * throwing sub-generator delegated via `yield from`): propagate so the owning exec lands. */
	if( rc == PH7_EXCEPTION || pVm->pResumeFrame || pVm->pInlineInstr ){
		/* The throw was handled by a `catch` that ran IN PLACE — either this try's own
		 * finally threw past itself superseding it (rc == PH7_EXCEPTION), or the catch
		 * sits several frames above this one (pVm->pResumeFrame recorded). Resume at the
		 * catching body's landing pad if that body is THIS exec (VmRecordedResume), else
		 * unwind so the owning exec lands. Without this a throw caught at an enclosing
		 * frame would blindly jump to nJump (this throw's lexically-nearest try) and run
		 * dead code after it (ROOT B, face a) or continue a callee as if it never threw
		 * (face c). */
		sxi32 iResumePc;
		if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){
			pc = iResumePc;
			VM_EXIT_BREAK;
		}
		VM_EXIT_EXCEPTION;
	}
	/* No in-place catch recorded: this throw's own enclosing try caught it (the
	 * common case; its landing pad is exactly nJump). Perform an unconditional jump
	 * to the try's OP_POP_EXCEPTION landing pad, which tears down the try frame, runs
	 * finally, and (when a catch/finally issued a `return`) materializes the body
	 * frame's pending return. Routing the return through OP_POP_EXCEPTION keeps the
	 * frame stack balanced. */
	pc = nJump - 1;
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}
