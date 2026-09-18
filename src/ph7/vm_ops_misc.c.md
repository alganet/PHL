# src/ph7/vm_ops_misc.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 223/257 lines (86.77%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|      - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    5 | ` */` |
|      - |    6 | `#include "ph7int.h"` |
|      - |    7 | `/*` |
|      - |    8 | ` * Section:` |
|      - |    9 | ` *    Opcode handlers extracted from vm.c's dispatch loop. Each handler runs` |
|      - |   10 | ` *    one opcode arm against the caller's VmExecState: the loop syncs pTos/pc` |
|      - |   11 | ` *    in, calls the handler, reloads them and routes the returned VmOpRc onto` |
|      - |   12 | ` *    its labels (same idiom as VmCallFinish).` |
|      - |   13 | ` * Status:` |
|      - |   14 | ` *    Stable.` |
|      - |   15 | ` */` |
|      - |   16 | `/* Bind the dispatch-routing exits to handler semantics (see vm_dispatch.h),` |
|      - |   17 | ` * and map the macros' sState references onto our state parameter. */` |
|      - |   18 | `#define VM_EXIT_BREAK      { pState->pTos = pTos; pState->pc = pc; return VM_OP_NEXT; }` |
|      - |   19 | `#define VM_EXIT_ABORT      { pState->pTos = pTos; pState->pc = pc; return VM_OP_ABORT; }` |
|      - |   20 | `#define VM_EXIT_EXCEPTION  { pState->pTos = pTos; pState->pc = pc; return VM_OP_EXCEPTION; }` |
|      - |   21 | `#include "vm_dispatch.h"` |
|      - |   22 | `#define sState (*pState)` |
|      - |   23 |  |
|      - |   24 | `/*` |
|      - |   25 | ` * OP_CONSUME: body moved verbatim from the OP_CONSUME arm of` |
|      - |   26 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |   27 | ` */` |
|  52368 |   28 | `PH7_PRIVATE VmOpRc VmExecOpConsume(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |   29 | `{` |
|  52373 |   30 | `	ph7_value *pTos = pState->pTos;` |
|  52373 |   31 | `	ph7_value *pStack = pState->pStack;` |
|  52373 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|  52373 |   33 | `	sxi32 pc = pState->pc;` |
|      - |   34 | `	sxi32 rc;` |
|  26184 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  52373 |   36 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|  52373 |   37 | `	ph7_value *pCur,*pOut = pTos;` |
|      - |   38 |  |
|  52373 |   39 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|  52373 |   40 | `	pCur = pOut;` |
|      - |   41 | `	/* Start the consume process  */` |
| 104741 |   42 | `	while( pOut <= pTos ){` |
|      - |   43 | `		/* Force a string cast */` |
|  52373 |   44 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|   3293 |   45 | `			PH7_MemObjToString(pOut);` |
|   1644 |   46 | `		}` |
|  52373 |   47 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|      - |   48 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|      - |   49 | `			/* Invoke the output consumer callback */` |
|  39999 |   50 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|  39999 |   51 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|  39999 |   52 | `			SyBlobRelease(&pOut->sBlob);` |
|  39999 |   53 | `			if( rc == SXERR_ABORT ){` |
|      - |   54 | `				/* Output consumer callback request an operation abort. */` |
|    ! 0 |   55 | `				VM_EXIT_ABORT;` |
|      - |   56 | `			}` |
|  19997 |   57 | `		}` |
|  52373 |   58 | `		pOut++;` |
|      5 |   59 | `	}` |
|  52373 |   60 | `	pTos = &pCur[-1];` |
|  52373 |   61 | `	VM_EXIT_BREAK;` |
|    ! 0 |   62 | `	VM_EXIT_BREAK;` |
|  26189 |   63 | `}` |
|      - |   64 |  |
|      - |   65 | `/*` |
|      - |   66 | ` * OP_MATCH: body moved verbatim from the OP_MATCH arm of` |
|      - |   67 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |   68 | ` */` |
|    112 |   69 | `PH7_PRIVATE VmOpRc VmExecOpMatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      3 |   70 | `{` |
|    115 |   71 | `	ph7_value *pTos = pState->pTos;` |
|    115 |   72 | `	ph7_value *pStack = pState->pStack;` |
|    115 |   73 | `	VmInstr *aInstr = pState->aInstr;` |
|    115 |   74 | `	sxi32 pc = pState->pc;` |
|      - |   75 | `	sxi32 rc;` |
|     56 |   76 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    115 |   77 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|    115 |   78 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|      - |   79 | `	ph7_value sSubject,sCond,sResult;` |
|      - |   80 | `	sxu32 i,j,nArm,nCond;` |
|    115 |   81 | `	int matched = 0;` |
|      - |   82 | `#ifdef UNTRUST` |
|      - |   83 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|      - |   84 | `		VM_EXIT_ABORT;` |
|      - |   85 | `	}` |
|      - |   86 | `#endif` |
|    115 |   87 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|    115 |   88 | `	nArm = SySetUsed(&pMatch->aArms);` |
|    115 |   89 | `	PH7_MemObjInit(pVm,&sSubject);` |
|    115 |   90 | `	PH7_MemObjInit(pVm,&sCond);` |
|    115 |   91 | `	PH7_MemObjInit(pVm,&sResult);` |
|    115 |   92 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|    361 |   93 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|    248 |   94 | `		pArm = &aArm[i];` |
|    248 |   95 | `		if( pArm->bDefault ){` |
|     13 |   96 | `			pDefault = pArm;` |
|     13 |   97 | `			continue;` |
|      - |   98 | `		}` |
|    236 |   99 | `		nCond = SySetUsed(&pArm->aConds);` |
|    406 |  100 | `		for( j = 0; j < nCond; ++j ){` |
|    268 |  101 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|    268 |  102 | `			if( pCondBc == 0 ){` |
|    ! 0 |  103 | `				continue;` |
|      - |  104 | `			}` |
|    268 |  105 | `			VmLocalExec(pVm,pCondBc,&sCond,FALSE);` |
|    268 |  106 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|    268 |  107 | `			PH7_MemObjRelease(&sCond);` |
|    268 |  108 | `			if( rc == 0 ){` |
|     97 |  109 | `				VmLocalExec(pVm,&pArm->aResult,&sResult,FALSE);` |
|     97 |  110 | `				matched = 1;` |
|     97 |  111 | `				break;` |
|      - |  112 | `			}` |
|     87 |  113 | `		}` |
|    119 |  114 | `	}` |
|    115 |  115 | `	if( !matched && pDefault ){` |
|     13 |  116 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult,FALSE);` |
|     13 |  117 | `		matched = 1;` |
|      6 |  118 | `	}` |
|    115 |  119 | `	if( !matched ){` |
|      6 |  120 | `		const char *zType = "unknown";` |
|      - |  121 | `		char zMsg[128];` |
|      - |  122 | `		sxu32 nMsg;` |
|      6 |  123 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|    ! 0 |  124 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|    ! 0 |  125 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|      6 |  126 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|    ! 0 |  127 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|    ! 0 |  128 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|    ! 0 |  129 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|    ! 0 |  130 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|    ! 0 |  131 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|    ! 0 |  132 | `		default: break;` |
|      - |  133 | `		}` |
|      - |  134 | `		SyBlob sErrMsg;` |
|      8 |  135 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|      2 |  136 | `			"Unhandled match case of type %s",zType);` |
|      6 |  137 | `		PH7_MemObjRelease(&sSubject);` |
|      6 |  138 | `		PH7_MemObjRelease(&sResult);` |
|      - |  139 | `		/* php raises a CATCHABLE \UnhandledMatchError here, so build a real` |
|      - |  140 | `		 * exception object and route it like any mid-expression throw (an` |
|      - |  141 | `		 * enclosing try in this body resumes at its landing pad; otherwise it` |
|      - |  142 | `		 * propagates and renders as uncaught, as before). Reporting it straight` |
|      - |  143 | `		 * to the uncaught renderer — what this site used to do — made the error` |
|      - |  144 | `		 * unconditionally fatal even inside try/catch. */` |
|      6 |  145 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      6 |  146 | `		SyBlobAppend(&sErrMsg,zMsg,nMsg);` |
|      6 |  147 | `		rc = VmThrowBuiltinError(&(*pVm),"UnhandledMatchError",` |
|      - |  148 | `			sizeof("UnhandledMatchError")-1,&sErrMsg);` |
|      6 |  149 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|      - |  150 | `	}` |
|    109 |  151 | `	PH7_MemObjRelease(&sSubject);` |
|      - |  152 | `	/* Replace subject on TOS with the arm result */` |
|    109 |  153 | `	PH7_MemObjStore(&sResult,pTos);` |
|    109 |  154 | `	PH7_MemObjRelease(&sResult);` |
|    109 |  155 | `	VM_EXIT_BREAK;` |
|    ! 0 |  156 | `	VM_EXIT_BREAK;` |
|     59 |  157 | `}` |
|      - |  158 |  |
|      - |  159 | `/*` |
|      - |  160 | ` * Build an \Error instance carrying zMsg (or NULL when the class is` |
|      - |  161 | ` * unavailable). Shared by OP_THROW's two "operand cannot be thrown" cases:` |
|      - |  162 | ` * php reports a non-object as "Can only throw objects" and a non-Throwable` |
|      - |  163 | ` * object as "Cannot throw objects that do not implement Throwable", both as` |
|      - |  164 | ` * ordinary catchable throws. The caller hands the instance to` |
|      - |  165 | ` * VmThrowException so the routing below sees exactly the status a normal` |
|      - |  166 | ` * throw produces. Error::__construct comes from the built-in library and` |
|      - |  167 | ` * cannot realistically fail, so its return is not checked.` |
|      - |  168 | ` */` |
|     18 |  169 | `static ph7_class_instance * VmNewThrowError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|      2 |  170 | `{` |
|      - |  171 | `	ph7_class *pErrorClass;` |
|      - |  172 | `	ph7_class_instance *pErrInst;` |
|      - |  173 | `	ph7_class_method *pCons;` |
|     20 |  174 | `	pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|     20 |  175 | `	if( pErrorClass == 0 ){` |
|    ! 0 |  176 | `		return 0;` |
|      - |  177 | `	}` |
|     20 |  178 | `	pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|     20 |  179 | `	if( pErrInst == 0 ){` |
|    ! 0 |  180 | `		return 0;` |
|      - |  181 | `	}` |
|     20 |  182 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|     20 |  183 | `	if( pCons ){` |
|      - |  184 | `		ph7_value sArg;` |
|      - |  185 | `		ph7_value *apArg[1];` |
|      - |  186 | `		SyString sMsgStr;` |
|     20 |  187 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|     20 |  188 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|     20 |  189 | `		apArg[0] = &sArg;` |
|     20 |  190 | `		PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|     20 |  191 | `		PH7_MemObjRelease(&sArg);` |
|      9 |  192 | `	}` |
|     20 |  193 | `	return pErrInst;` |
|     11 |  194 | `}` |
|      - |  195 | `/*` |
|      - |  196 | ` * OP_THROW: body moved verbatim from the OP_THROW arm of` |
|      - |  197 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  198 | ` */` |
|    754 |  199 | `PH7_PRIVATE VmOpRc VmExecOpThrow(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  200 | `{` |
|    759 |  201 | `	ph7_value *pTos = pState->pTos;` |
|    759 |  202 | `	ph7_value *pStack = pState->pStack;` |
|    759 |  203 | `	VmInstr *aInstr = pState->aInstr;` |
|    759 |  204 | `	sxi32 pc = pState->pc;` |
|      - |  205 | `	sxi32 rc;` |
|    377 |  206 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    759 |  207 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|    759 |  208 | `	sxu32 nJump = pInstr->iP2;` |
|      - |  209 | `#ifdef UNTRUST` |
|      - |  210 | `	if( pTos < pStack ){` |
|      - |  211 | `		VM_EXIT_ABORT;` |
|      - |  212 | `	}` |
|      - |  213 | `#endif` |
|    759 |  214 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      - |  215 | `	/* Tell the upper layer that an exception was thrown */` |
|    759 |  216 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|    759 |  217 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|    745 |  218 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|      - |  219 | `		ph7_class *pThrowable;` |
|      - |  220 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|    745 |  221 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|    747 |  222 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|      - |  223 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior. */` |
|      - |  224 | `			static const char zErrMsg[] =` |
|      - |  225 | `				"Cannot throw objects that do not implement Throwable";` |
|      6 |  226 | `			ph7_class_instance *pErrInst = VmNewThrowError(&(*pVm),zErrMsg,sizeof(zErrMsg)-1);` |
|      6 |  227 | `			if( pErrInst ){` |
|      6 |  228 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|      6 |  229 | `				PH7_ClassInstanceUnref(pErrInst);` |
|      6 |  230 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  231 | `					VM_EXIT_ABORT;` |
|      - |  232 | `				}` |
|      4 |  233 | `			}else{` |
|      - |  234 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|    ! 0 |  235 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|    ! 0 |  236 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  237 | `					VM_EXIT_ABORT;` |
|      - |  238 | `				}` |
|      - |  239 | `			}` |
|      4 |  240 | `		}else{` |
|      - |  241 | `			/* Throw the exception */` |
|    741 |  242 | `			rc = VmThrowException(&(*pVm),pThis);` |
|    741 |  243 | `			if( rc == SXERR_ABORT ){` |
|      - |  244 | `				/* Abort processing immediately */` |
|     32 |  245 | `				VM_EXIT_ABORT;` |
|      - |  246 | `			}` |
|      - |  247 | `		}` |
|    361 |  248 | `	}else{` |
|      - |  249 | `		/* php raises a CATCHABLE Error for a non-object operand. This used to` |
|      - |  250 | `		 * report a bogus uncaught "Exception" and then fall into the jump below,` |
|      - |  251 | `		 * so no catch ran yet execution carried on past the try — and the branch` |
|      - |  252 | `		 * tested a STALE rc left by the previous instruction, which is the` |
|      - |  253 | `		 * latent bug the interpreter split surfaced. Build the same Error shape` |
|      - |  254 | `		 * the not-Throwable case uses and let the routing below land it. */` |
|      - |  255 | `		static const char zErrMsg[] = "Can only throw objects";` |
|     15 |  256 | `		ph7_class_instance *pErrInst = VmNewThrowError(&(*pVm),zErrMsg,sizeof(zErrMsg)-1);` |
|     15 |  257 | `		if( pErrInst ){` |
|     15 |  258 | `			rc = VmThrowException(&(*pVm),pErrInst);` |
|     15 |  259 | `			PH7_ClassInstanceUnref(pErrInst);` |
|      8 |  260 | `		}else{` |
|      - |  261 | `			/* Bootstrap failure — fall back to uncaught reporting */` |
|    ! 0 |  262 | `			rc = VmUncaughtException(&(*pVm),0);` |
|      - |  263 | `		}` |
|     15 |  264 | `		if( rc == SXERR_ABORT ){` |
|      - |  265 | `			/* Abort processing immediately */` |
|    ! 0 |  266 | `			VM_EXIT_ABORT;` |
|      - |  267 | `		}` |
|      - |  268 | `	}` |
|      - |  269 | `	/* Pop the top entry */` |
|    731 |  270 | `	VmPopOperand(&pTos,1);` |
|      - |  271 | `	/* ROOT C: throw caught by an inline try in THIS exec -> jump to its catch/finally` |
|      - |  272 | `	 * (draining any mid-expression operands back to the try's base). */` |
|    731 |  273 | `	PH7_INLINE_RESUME_BREAK()` |
|      - |  274 | `	/* pInlineInstr still set here means an inline try in an OUTER exec caught it (e.g. a` |
|      - |  275 | ``	 * throwing sub-generator delegated via `yield from`): propagate so the owning exec lands. */`` |
|    717 |  276 | `	if( rc == PH7_EXCEPTION \|\| pVm->pResumeFrame \|\| pVm->pInlineInstr ){` |
|      - |  277 | ``		/* The throw was handled by a `catch` that ran IN PLACE — either this try's own`` |
|      - |  278 | `		 * finally threw past itself superseding it (rc == PH7_EXCEPTION), or the catch` |
|      - |  279 | `		 * sits several frames above this one (pVm->pResumeFrame recorded). Resume at the` |
|      - |  280 | `		 * catching body's landing pad if that body is THIS exec (VmRecordedResume), else` |
|      - |  281 | `		 * unwind so the owning exec lands. Without this a throw caught at an enclosing` |
|      - |  282 | `		 * frame would blindly jump to nJump (this throw's lexically-nearest try) and run` |
|      - |  283 | `		 * dead code after it (ROOT B, face a) or continue a callee as if it never threw` |
|      - |  284 | `		 * (face c). */` |
|      - |  285 | `		sxi32 iResumePc;` |
|    641 |  286 | `		if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){` |
|    231 |  287 | `			pc = iResumePc;` |
|    231 |  288 | `			VM_EXIT_BREAK;` |
|      - |  289 | `		}` |
|    415 |  290 | `		VM_EXIT_EXCEPTION;` |
|      - |  291 | `	}` |
|      - |  292 | `	/* No in-place catch recorded: this throw's own enclosing try caught it (the` |
|      - |  293 | `	 * common case; its landing pad is exactly nJump). Perform an unconditional jump` |
|      - |  294 | `	 * to the try's OP_POP_EXCEPTION landing pad, which tears down the try frame, runs` |
|      - |  295 | ``	 * finally, and (when a catch/finally issued a `return`) materializes the body`` |
|      - |  296 | `	 * frame's pending return. Routing the return through OP_POP_EXCEPTION keeps the` |
|      - |  297 | `	 * frame stack balanced. */` |
|     79 |  298 | `	pc = nJump - 1;` |
|     79 |  299 | `	VM_EXIT_BREAK;` |
|    ! 0 |  300 | `	VM_EXIT_BREAK;` |
|    382 |  301 | `}` |
|      - |  302 |  |
|      - |  303 | `/*` |
|      - |  304 | ` * OP_SWITCH: body moved verbatim from the OP_SWITCH arm of` |
|      - |  305 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  306 | ` */` |
|    324 |  307 | `PH7_PRIVATE VmOpRc VmExecOpSwitch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  308 | `{` |
|    329 |  309 | `	ph7_value *pTos = pState->pTos;` |
|    329 |  310 | `	ph7_value *pStack = pState->pStack;` |
|    329 |  311 | `	VmInstr *aInstr = pState->aInstr;` |
|    329 |  312 | `	sxi32 pc = pState->pc;` |
|      - |  313 | `	sxi32 rc;` |
|    162 |  314 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    329 |  315 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|      - |  316 | `	ph7_case_expr *aCase,*pCase;` |
|      - |  317 | `	ph7_value sValue,sCaseValue;` |
|      - |  318 | `	sxu32 n,nEntry;` |
|      - |  319 | `#ifdef UNTRUST` |
|      - |  320 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|      - |  321 | `		VM_EXIT_ABORT;` |
|      - |  322 | `	}` |
|      - |  323 | `#endif` |
|      - |  324 | `	/* Point to the case table  */` |
|    329 |  325 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|    329 |  326 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|      - |  327 | `	/* Select the appropriate case block to execute */` |
|    329 |  328 | `	PH7_MemObjInit(pVm,&sValue);` |
|    329 |  329 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|   1211 |  330 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|   1209 |  331 | `		pCase = &aCase[n];` |
|   1209 |  332 | `		PH7_MemObjLoad(pTos,&sValue);` |
|      - |  333 | `		/* Execute the case expression first */` |
|   1209 |  334 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue,FALSE);` |
|      - |  335 | `		/* Compare the two expression */` |
|   1209 |  336 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|   1209 |  337 | `		PH7_MemObjRelease(&sValue);` |
|   1209 |  338 | `		PH7_MemObjRelease(&sCaseValue);` |
|   1209 |  339 | `		if( rc == 0 ){` |
|      - |  340 | `			/* Value match,jump to this block */` |
|    327 |  341 | `			pc = pCase->nStart - 1;` |
|    327 |  342 | `			break;` |
|      - |  343 | `		}` |
|    446 |  344 | `	}` |
|    329 |  345 | `	VmPopOperand(&pTos,1);` |
|    329 |  346 | `	if( n >= nEntry ){` |
|      - |  347 | `		/* No approprite case to execute,jump to the default case */` |
|      3 |  348 | `		if( pSwitch->nDefault > 0 ){` |
|      3 |  349 | `			pc = pSwitch->nDefault - 1;` |
|      2 |  350 | `		}else{` |
|      - |  351 | `			/* No default case,jump out of this switch */` |
|    ! 0 |  352 | `			pc = pSwitch->nOut - 1;` |
|      - |  353 | `		}` |
|      1 |  354 | `	}` |
|    329 |  355 | `	VM_EXIT_BREAK;` |
|    ! 0 |  356 | `	VM_EXIT_BREAK;` |
|      5 |  357 | `}` |
|      - |  358 |  |
|      - |  359 | `/*` |
|      - |  360 | ` * OP_CATCH: body moved verbatim from the OP_CATCH arm of` |
|      - |  361 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  362 | ` */` |
|     66 |  363 | `PH7_PRIVATE VmOpRc VmExecOpCatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  364 | `{` |
|     71 |  365 | `	ph7_value *pTos = pState->pTos;` |
|     71 |  366 | `	ph7_value *pStack = pState->pStack;` |
|     71 |  367 | `	VmInstr *aInstr = pState->aInstr;` |
|     71 |  368 | `	sxi32 pc = pState->pc;` |
|      - |  369 | `	sxi32 rc;` |
|     33 |  370 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - |  371 | `	/* BYTECODE stage 2b: mutable state (pInflight) lives on the LIVE activation` |
|      - |  372 | `	 * of this try, not the compiled p3 (which VmThrowInline kept on aException` |
|      - |  373 | `	 * marked iInCatch). Compiled fields (sEntry) are shared either way. */` |
|     71 |  374 | `	ph7_exception *pExcC = (ph7_exception *)pInstr->p3;` |
|     71 |  375 | `	ph7_exception *pExc = VmExcLive(&(*pVm),pExcC);` |
|     71 |  376 | `	ph7_exception_block *pCatch = (ph7_exception_block *)SySetAt(&pExcC->sEntry,(sxu32)pInstr->iP1);` |
|     71 |  377 | `	ph7_class_instance *pBind = pExc ? pExc->pInflight : 0;` |
|     71 |  378 | `	VmFrame *pBody = VmSkipExceptionFrames(pVm->pFrame);` |
|     71 |  379 | `	pBody->iFlags &= ~VM_FRAME_THROW;` |
|     71 |  380 | `	if( pCatch && pBind && pCatch->sThis.nByte > 0 ){` |
|      - |  381 | `		/* sThis empty => PHP 8.0 non-capturing catch (catch (Type) {}): the` |
|      - |  382 | `		 * exception is caught but not bound to any variable. */` |
|     69 |  383 | `		ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|     69 |  384 | `		if( pObj ){` |
|      - |  385 | `			/* Overwrite-then-release (mirrors PH7_MemObjStore): pin the new instance,` |
|      - |  386 | `			 * free the slot's prior contents, then rebind. */` |
|     69 |  387 | `			pBind->iRef++;` |
|     69 |  388 | `			PH7_MemObjRelease(pObj);` |
|     69 |  389 | `			pObj->x.pOther = pBind;` |
|     69 |  390 | `			MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     32 |  391 | `		}` |
|     32 |  392 | `	}` |
|     71 |  393 | `	if( pBind ){` |
|      - |  394 | `		/* Drop the hold VmThrowInline took across the redirect. */` |
|     71 |  395 | `		PH7_ClassInstanceUnref(pBind);` |
|     33 |  396 | `	}` |
|     71 |  397 | `	if( pExc ){` |
|     71 |  398 | `		pExc->pInflight = 0;` |
|     33 |  399 | `	}` |
|     71 |  400 | `	VM_EXIT_BREAK;` |
|    ! 0 |  401 | `	VM_EXIT_BREAK;` |
|      5 |  402 | `}` |
|      - |  403 |  |
|      - |  404 | `/*` |
|      - |  405 | ` * OP_LOAD_EXCEPTION: body moved verbatim from the OP_LOAD_EXCEPTION arm of` |
|      - |  406 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  407 | ` */` |
|   2790 |  408 | `PH7_PRIVATE VmOpRc VmExecOpLoadException(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  409 | `{` |
|   2795 |  410 | `	ph7_value *pTos = pState->pTos;` |
|   2795 |  411 | `	ph7_value *pStack = pState->pStack;` |
|   2795 |  412 | `	VmInstr *aInstr = pState->aInstr;` |
|   2795 |  413 | `	sxi32 pc = pState->pc;` |
|      - |  414 | `	sxi32 rc;` |
|   1395 |  415 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - |  416 | `	/* BYTECODE stage 2b: push a fresh ACTIVATION of this lexical try (own` |
|      - |  417 | `	 * mutable state per entry — see VmExcActivate), never the shared` |
|      - |  418 | `	 * compiled object. */` |
|   2795 |  419 | `	ph7_exception *pException = VmExcActivate(&(*pVm),(ph7_exception *)pInstr->p3);` |
|      - |  420 | `	VmFrame *pFrameLocal;` |
|   2795 |  421 | `	if( pException == 0 ){` |
|    ! 0 |  422 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|    ! 0 |  423 | `		VM_EXIT_ABORT;` |
|      - |  424 | `	}` |
|      - |  425 | `	/* Create the exception frame BEFORE publishing the activation, so an OOM` |
|      - |  426 | `	 * abort cannot orphan a pushed entry with no frame behind it. */` |
|   2795 |  427 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|   2795 |  428 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  429 | `		VmExcRelease(&(*pVm),pException);` |
|    ! 0 |  430 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|    ! 0 |  431 | `		VM_EXIT_ABORT;` |
|      - |  432 | `	}` |
|   2795 |  433 | `	if( SXRET_OK != SySetPut(&pVm->aException,(const void *)&pException) ){` |
|    ! 0 |  434 | `		VmExcRelease(&(*pVm),pException);` |
|    ! 0 |  435 | `		VmLeaveFrame(&(*pVm));` |
|    ! 0 |  436 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|    ! 0 |  437 | `		VM_EXIT_ABORT;` |
|      - |  438 | `	}` |
|      - |  439 | `	/* Mark the special frame */` |
|   2795 |  440 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|   2795 |  441 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|      - |  442 | `	/* Record the landing pad on the exception too, so an in-place catch can resume` |
|      - |  443 | `	 * the throwing site at THIS try (survives the exception frame's teardown), plus` |
|      - |  444 | `	 * the bytecode array it indexes — the resume only fires in the exec running that` |
|      - |  445 | `	 * array, so a mini-program (inline try in a catch/finally) and the body that` |
|      - |  446 | `	 * shares its frame don't mis-apply each other's landing pad. iLandingPc mirrors the` |
|      - |  447 | `	 * frame's iExceptionJump just set above — reuse it so the two can't drift. */` |
|   2795 |  448 | `	pException->iLandingPc = pFrameLocal->iExceptionJump;` |
|   2795 |  449 | `	pException->pOwnerInstr = (void *)aInstr;` |
|      - |  450 | `	/* Operand-stack base at try entry (0-based TOS index; -1 when empty). The post-try` |
|      - |  451 | `	 * landing pad is reached with the stack back at this depth; Generator::throw()` |
|      - |  452 | `	 * inject-at-yield drains to it before landing (a mid-expression yield leaves the` |
|      - |  453 | `	 * abandoned expression's operands above this base). Normal throws are already here. */` |
|   2795 |  454 | `	pException->iStackDepth = (sxi32)(pTos - pStack);` |
|      - |  455 | `	/* '@' depth at try entry — see ph7_exception.iErrSuppress */` |
|   2795 |  456 | `	pException->iErrSuppress = pVm->nErrSuppress;` |
|      - |  457 | `	/* Point to the frame that trigger the exception */` |
|   2795 |  458 | `	pFrameLocal = pFrameLocal->pParent;` |
|   2795 |  459 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   2795 |  460 | `	pException->pFrame = pFrameLocal;` |
|   2795 |  461 | `	VM_EXIT_BREAK;` |
|    ! 0 |  462 | `	VM_EXIT_BREAK;` |
|   1400 |  463 | `}` |
|      - |  464 |  |
