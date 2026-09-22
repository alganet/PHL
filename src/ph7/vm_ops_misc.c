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
		/* Force a string cast (echo/print: user-visible array->string warning, §2).
		 * A not-stringable object throws HERE, mid-list: php compiles `echo a,b,c`
		 * to one ECHO per operand, so everything left of the object is already
		 * out. Release what is left so the abandoned operands don't outlive the
		 * op, then route. */
		sxi32 rcSv = PH7_MemObjToStringUV(pOut);
		if( rcSv != SXRET_OK ){
			while( pOut <= pTos ){
				PH7_MemObjRelease(pOut);
				pOut++;
			}
			pTos = &pCur[-1];
			PH7_DISPATCH_TOSTRING_RC(rcSv)
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
	sxi32 rcArm = SXRET_OK;
	const void *pResumeBefore = (const void *)pVm->pResumeFrame;
	const void *pInlineBefore = (const void *)pVm->pInlineInstr;
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
	/* Each condition and each arm BODY is its own bytecode container, run by
	 * VmLocalExec. A throw inside one abandons the whole match expression in php,
	 * so its status is routed below (VmLocalExecThrew): ignoring it let a caught
	 * throw fall through to the NEXT condition — and then to the default arm,
	 * which php never reaches — and handed the abandoned statement a value. */
	for( i = 0; i < nArm && !matched && rcArm == SXRET_OK; ++i ){
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
			rcArm = VmLocalExec(pVm,pCondBc,&sCond,FALSE);
			if( rcArm == PH7_ABORT || VmLocalExecThrew(pVm,rcArm,pResumeBefore,pInlineBefore) ){
				break;
			}
			rcArm = SXRET_OK;
			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);
			PH7_MemObjRelease(&sCond);
			if( rc == 0 ){
				rcArm = VmLocalExec(pVm,&pArm->aResult,&sResult,FALSE);
				matched = 1;
				break;
			}
		}
	}
	if( !matched && pDefault && rcArm != PH7_ABORT && !VmLocalExecThrew(pVm,rcArm,pResumeBefore,pInlineBefore) ){
		rcArm = VmLocalExec(pVm,&pDefault->aResult,&sResult,FALSE);
		matched = 1;
	}
	if( rcArm == PH7_ABORT ){
		PH7_MemObjRelease(&sCond);
		PH7_MemObjRelease(&sSubject);
		PH7_MemObjRelease(&sResult);
		VM_EXIT_ABORT;
	}
	if( VmLocalExecThrew(pVm,rcArm,pResumeBefore,pInlineBefore) ){
		PH7_MemObjRelease(&sCond);
		PH7_MemObjRelease(&sSubject);
		PH7_MemObjRelease(&sResult);
		VmPopOperand(&pTos,1); /* the subject this OP_MATCH would have replaced */
		PH7_THROW_ROUTE_MIDEXPR(rcArm)
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
		SyBlob sErrMsg;
		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),
			"Unhandled match case of type %s",zType);
		PH7_MemObjRelease(&sSubject);
		PH7_MemObjRelease(&sResult);
		/* php raises a CATCHABLE \UnhandledMatchError here, so build a real
		 * exception object and route it like any mid-expression throw (an
		 * enclosing try in this body resumes at its landing pad; otherwise it
		 * propagates and renders as uncaught, as before). Reporting it straight
		 * to the uncaught renderer — what this site used to do — made the error
		 * unconditionally fatal even inside try/catch. */
		SyBlobInit(&sErrMsg,&pVm->sAllocator);
		SyBlobAppend(&sErrMsg,zMsg,nMsg);
		rc = VmThrowBuiltinError(&(*pVm),"UnhandledMatchError",
			sizeof("UnhandledMatchError")-1,&sErrMsg);
		PH7_THROW_ROUTE_MIDEXPR(rc)
	}
	PH7_MemObjRelease(&sSubject);
	/* Replace subject on TOS with the arm result */
	PH7_MemObjStore(&sResult,pTos);
	PH7_MemObjRelease(&sResult);
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * Build an \Error instance carrying zMsg (or NULL when the class is
 * unavailable). Shared by OP_THROW's two "operand cannot be thrown" cases:
 * php reports a non-object as "Can only throw objects" and a non-Throwable
 * object as "Cannot throw objects that do not implement Throwable", both as
 * ordinary catchable throws. The caller hands the instance to
 * VmThrowException so the routing below sees exactly the status a normal
 * throw produces. Error::__construct comes from the built-in library and
 * cannot realistically fail, so its return is not checked.
 */
static ph7_class_instance * VmNewThrowError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)
{
	ph7_class *pErrorClass;
	ph7_class_instance *pErrInst;
	ph7_class_method *pCons;
	pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);
	if( pErrorClass == 0 ){
		return 0;
	}
	pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);
	if( pErrInst == 0 ){
		return 0;
	}
	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);
	if( pCons ){
		ph7_value sArg;
		ph7_value *apArg[1];
		SyString sMsgStr;
		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);
		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);
		apArg[0] = &sArg;
		PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);
		PH7_MemObjRelease(&sArg);
	}
	return pErrInst;
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
			/* Not a Throwable: replace with Error(msg) matching PHP behavior. */
			static const char zErrMsg[] =
				"Cannot throw objects that do not implement Throwable";
			ph7_class_instance *pErrInst = VmNewThrowError(&(*pVm),zErrMsg,sizeof(zErrMsg)-1);
			if( pErrInst ){
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
		/* php raises a CATCHABLE Error for a non-object operand. This used to
		 * report a bogus uncaught "Exception" and then fall into the jump below,
		 * so no catch ran yet execution carried on past the try — and the branch
		 * tested a STALE rc left by the previous instruction, which is the
		 * latent bug the interpreter split surfaced. Build the same Error shape
		 * the not-Throwable case uses and let the routing below land it. */
		static const char zErrMsg[] = "Can only throw objects";
		ph7_class_instance *pErrInst = VmNewThrowError(&(*pVm),zErrMsg,sizeof(zErrMsg)-1);
		if( pErrInst ){
			rc = VmThrowException(&(*pVm),pErrInst);
			PH7_ClassInstanceUnref(pErrInst);
		}else{
			/* Bootstrap failure — fall back to uncaught reporting */
			rc = VmUncaughtException(&(*pVm),0);
		}
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
			PH7_RESUME_DRAIN()
			pc = iResumePc;
			VM_EXIT_BREAK;
		}
		VM_EXIT_EXCEPTION;
	}
	/* No in-place catch recorded: this throw's own enclosing try caught it (the
	 * common case; its landing pad is exactly nJump). A throw-EXPRESSION
	 * (`$q = 1 + throw new E`) abandons its outer expression's pending operands
	 * above the try's base — drain to the catching activation's recorded depth
	 * (matched by landing pad + bytecode array so an unrelated activation can
	 * never be consulted) before jumping, or each caught throw leaks a slot.
	 * Then jump to the try's OP_POP_EXCEPTION landing pad, which tears down the
	 * try frame, runs finally, and (when a catch/finally issued a `return`)
	 * materializes the body frame's pending return. Routing the return through
	 * OP_POP_EXCEPTION keeps the frame stack balanced. */
	if( SySetUsed(&pVm->aException) > 0 ){
		ph7_exception *pTopExc = ((ph7_exception **)SySetBasePtr(&pVm->aException))[SySetUsed(&pVm->aException)-1];
		if( pTopExc->iLandingPc == (sxu32)nJump && pTopExc->pOwnerInstr == (void *)aInstr ){
			while( (sxi32)(pTos - pStack) > pTopExc->iStackDepth ){
				PH7_MemObjRelease(pTos);
				pTos--;
			}
		}
	}
	pc = nJump - 1;
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_SWITCH: body moved verbatim from the OP_SWITCH arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpSwitch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;
	ph7_case_expr *aCase,*pCase;
	ph7_value sValue,sCaseValue;
	sxu32 n,nEntry;
	const void *pResumeBefore = (const void *)pVm->pResumeFrame;
	const void *pInlineBefore = (const void *)pVm->pInlineInstr;
#ifdef UNTRUST
	if( pSwitch == 0 || pTos < pStack ){
		VM_EXIT_ABORT;
	}
#endif
	/* Point to the case table  */
	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);
	nEntry = SySetUsed(&pSwitch->aCaseExpr);
	/* Select the appropriate case block to execute */
	PH7_MemObjInit(pVm,&sValue);
	PH7_MemObjInit(pVm,&sCaseValue);
	for( n = 0 ; n < nEntry ; ++n ){
		sxi32 rcCase;
		pCase = &aCase[n];
		PH7_MemObjLoad(pTos,&sValue);
		/* Execute the case expression first. It is its own bytecode container
		 * (VmLocalExec), so a throw inside it — `case boom():` — used to be caught
		 * in place and then IGNORED here: the scan carried on into the remaining
		 * cases and finally jumped to `default:`, running a branch php never
		 * reaches. Route the status the way any mid-expression throw is routed. */
		rcCase = VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue,FALSE);
		if( rcCase == PH7_ABORT || VmLocalExecThrew(pVm,rcCase,pResumeBefore,pInlineBefore) ){
			PH7_MemObjRelease(&sValue);
			PH7_MemObjRelease(&sCaseValue);
			if( rcCase == PH7_ABORT ){
				VM_EXIT_ABORT;
			}
			VmPopOperand(&pTos,1); /* the switch subject */
			PH7_THROW_ROUTE_MIDEXPR(rcCase)
		}
		/* Compare the two expression */
		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);
		PH7_MemObjRelease(&sValue);
		PH7_MemObjRelease(&sCaseValue);
		if( rc == 0 ){
			/* Value match,jump to this block */
			pc = pCase->nStart - 1;
			break;
		}
	}
	VmPopOperand(&pTos,1);
	if( n >= nEntry ){
		/* No approprite case to execute,jump to the default case */
		if( pSwitch->nDefault > 0 ){
			pc = pSwitch->nDefault - 1;
		}else{
			/* No default case,jump out of this switch */
			pc = pSwitch->nOut - 1;
		}
	}
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_CATCH: body moved verbatim from the OP_CATCH arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpCatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	/* BYTECODE stage 2b: mutable state (pInflight) lives on the LIVE activation
	 * of this try, not the compiled p3 (which VmThrowInline kept on aException
	 * marked iInCatch). Compiled fields (sEntry) are shared either way. */
	ph7_exception *pExcC = (ph7_exception *)pInstr->p3;
	ph7_exception *pExc = VmExcLive(&(*pVm),pExcC);
	ph7_exception_block *pCatch = (ph7_exception_block *)SySetAt(&pExcC->sEntry,(sxu32)pInstr->iP1);
	ph7_class_instance *pBind = pExc ? pExc->pInflight : 0;
	VmFrame *pBody = VmSkipExceptionFrames(pVm->pFrame);
	pBody->iFlags &= ~VM_FRAME_THROW;
	if( pCatch && pBind && pCatch->sThis.nByte > 0 ){
		/* sThis empty => PHP 8.0 non-capturing catch (catch (Type) {}): the
		 * exception is caught but not bound to any variable. */
		ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);
		if( pObj ){
			/* Overwrite-then-release (mirrors PH7_MemObjStore): pin the new instance,
			 * free the slot's prior contents, then rebind. */
			pBind->iRef++;
			PH7_MemObjRelease(pObj);
			pObj->x.pOther = pBind;
			MemObjSetType(pObj,MEMOBJ_OBJ);
		}
	}
	if( pBind ){
		/* Drop the hold VmThrowInline took across the redirect. */
		PH7_ClassInstanceUnref(pBind);
	}
	if( pExc ){
		pExc->pInflight = 0;
	}
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}

/*
 * OP_LOAD_EXCEPTION: body moved verbatim from the OP_LOAD_EXCEPTION arm of
 * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.
 */
PH7_PRIVATE VmOpRc VmExecOpLoadException(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)
{
	ph7_value *pTos = pState->pTos;
	ph7_value *pStack = pState->pStack;
	VmInstr *aInstr = pState->aInstr;
	sxi32 pc = pState->pc;
	sxi32 rc;
	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);
	/* BYTECODE stage 2b: push a fresh ACTIVATION of this lexical try (own
	 * mutable state per entry — see VmExcActivate), never the shared
	 * compiled object. */
	ph7_exception *pException = VmExcActivate(&(*pVm),(ph7_exception *)pInstr->p3);
	VmFrame *pFrameLocal;
	if( pException == 0 ){
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");
		VM_EXIT_ABORT;
	}
	/* Create the exception frame BEFORE publishing the activation, so an OOM
	 * abort cannot orphan a pushed entry with no frame behind it. */
	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);
	if( rc != SXRET_OK ){
		VmExcRelease(&(*pVm),pException);
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");
		VM_EXIT_ABORT;
	}
	if( SXRET_OK != SySetPut(&pVm->aException,(const void *)&pException) ){
		VmExcRelease(&(*pVm),pException);
		VmLeaveFrame(&(*pVm));
		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");
		VM_EXIT_ABORT;
	}
	/* Mark the special frame */
	pFrameLocal->iFlags |= VM_FRAME_EXCEPTION;
	pFrameLocal->iExceptionJump = pInstr->iP2;
	/* Record the landing pad on the exception too, so an in-place catch can resume
	 * the throwing site at THIS try (survives the exception frame's teardown), plus
	 * the bytecode array it indexes — the resume only fires in the exec running that
	 * array, so a mini-program (inline try in a catch/finally) and the body that
	 * shares its frame don't mis-apply each other's landing pad. iLandingPc mirrors the
	 * frame's iExceptionJump just set above — reuse it so the two can't drift. */
	pException->iLandingPc = pFrameLocal->iExceptionJump;
	pException->pOwnerInstr = (void *)aInstr;
	/* Operand-stack base at try entry (0-based TOS index; -1 when empty). The post-try
	 * landing pad is reached with the stack back at this depth; Generator::throw()
	 * inject-at-yield drains to it before landing (a mid-expression yield leaves the
	 * abandoned expression's operands above this base). Normal throws are already here. */
	pException->iStackDepth = (sxi32)(pTos - pStack);
	/* '@' depth at try entry — see ph7_exception.iErrSuppress */
	pException->iErrSuppress = pVm->nErrSuppress;
	/* Point to the frame that trigger the exception */
	pFrameLocal = pFrameLocal->pParent;
	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);
	pException->pFrame = pFrameLocal;
	VM_EXIT_BREAK;
	VM_EXIT_BREAK;
}
