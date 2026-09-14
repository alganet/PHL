# src/ph7/vm_ops_misc.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 213/247 lines (86.23%)

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
|  50848 |   28 | `PH7_PRIVATE VmOpRc VmExecOpConsume(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |   29 | `{` |
|  50853 |   30 | `	ph7_value *pTos = pState->pTos;` |
|  50853 |   31 | `	ph7_value *pStack = pState->pStack;` |
|  50853 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|  50853 |   33 | `	sxi32 pc = pState->pc;` |
|      - |   34 | `	sxi32 rc;` |
|  25424 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  50853 |   36 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|  50853 |   37 | `	ph7_value *pCur,*pOut = pTos;` |
|      - |   38 |  |
|  50853 |   39 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|  50853 |   40 | `	pCur = pOut;` |
|      - |   41 | `	/* Start the consume process  */` |
| 101701 |   42 | `	while( pOut <= pTos ){` |
|      - |   43 | `		/* Force a string cast */` |
|  50853 |   44 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|   3027 |   45 | `			PH7_MemObjToString(pOut);` |
|   1511 |   46 | `		}` |
|  50853 |   47 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|      - |   48 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|      - |   49 | `			/* Invoke the output consumer callback */` |
|  38277 |   50 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|  38277 |   51 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|  38277 |   52 | `			SyBlobRelease(&pOut->sBlob);` |
|  38277 |   53 | `			if( rc == SXERR_ABORT ){` |
|      - |   54 | `				/* Output consumer callback request an operation abort. */` |
|    ! 0 |   55 | `				VM_EXIT_ABORT;` |
|      - |   56 | `			}` |
|  19136 |   57 | `		}` |
|  50853 |   58 | `		pOut++;` |
|      5 |   59 | `	}` |
|  50853 |   60 | `	pTos = &pCur[-1];` |
|  50853 |   61 | `	VM_EXIT_BREAK;` |
|    ! 0 |   62 | `	VM_EXIT_BREAK;` |
|  25429 |   63 | `}` |
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
|      8 |  134 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|      2 |  135 | `			"Unhandled match case of type %s",zType);` |
|      8 |  136 | `		VmReportUncaughtException(&(*pVm),"UnhandledMatchError",` |
|      2 |  137 | `			sizeof("UnhandledMatchError")-1,zMsg,nMsg,0,0);` |
|      6 |  138 | `		PH7_MemObjRelease(&sSubject);` |
|      6 |  139 | `		PH7_MemObjRelease(&sResult);` |
|      6 |  140 | `		VM_EXIT_ABORT;` |
|      - |  141 | `	}` |
|    109 |  142 | `	PH7_MemObjRelease(&sSubject);` |
|      - |  143 | `	/* Replace subject on TOS with the arm result */` |
|    109 |  144 | `	PH7_MemObjStore(&sResult,pTos);` |
|    109 |  145 | `	PH7_MemObjRelease(&sResult);` |
|    109 |  146 | `	VM_EXIT_BREAK;` |
|    ! 0 |  147 | `	VM_EXIT_BREAK;` |
|     59 |  148 | `}` |
|      - |  149 |  |
|      - |  150 | `/*` |
|      - |  151 | ` * OP_THROW: body moved verbatim from the OP_THROW arm of` |
|      - |  152 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  153 | ` */` |
|    732 |  154 | `PH7_PRIVATE VmOpRc VmExecOpThrow(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  155 | `{` |
|    737 |  156 | `	ph7_value *pTos = pState->pTos;` |
|    737 |  157 | `	ph7_value *pStack = pState->pStack;` |
|    737 |  158 | `	VmInstr *aInstr = pState->aInstr;` |
|    737 |  159 | `	sxi32 pc = pState->pc;` |
|      - |  160 | `	sxi32 rc;` |
|    366 |  161 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    737 |  162 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|    737 |  163 | `	sxu32 nJump = pInstr->iP2;` |
|      - |  164 | `#ifdef UNTRUST` |
|      - |  165 | `	if( pTos < pStack ){` |
|      - |  166 | `		VM_EXIT_ABORT;` |
|      - |  167 | `	}` |
|      - |  168 | `#endif` |
|    737 |  169 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      - |  170 | `	/* Tell the upper layer that an exception was thrown */` |
|    737 |  171 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|    737 |  172 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|    737 |  173 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|      - |  174 | `		ph7_class *pThrowable;` |
|      - |  175 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|    737 |  176 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|    738 |  177 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|      - |  178 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|      - |  179 | `			 * Error::__construct is defined in the built-in library and` |
|      - |  180 | `			 * cannot realistically fail, so we do not check its return. */` |
|      3 |  181 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|      3 |  182 | `			ph7_class_instance *pErrInst = 0;` |
|      3 |  183 | `			if( pErrorClass ){` |
|      3 |  184 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|      1 |  185 | `			}` |
|      3 |  186 | `			if( pErrInst ){` |
|      - |  187 | `				ph7_class_method *pCons;` |
|      3 |  188 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|      3 |  189 | `				if( pCons ){` |
|      - |  190 | `					ph7_value sArg;` |
|      - |  191 | `					ph7_value *apArg[1];` |
|      - |  192 | `					SyString sMsgStr;` |
|      - |  193 | `					static const char zErrMsg[] =` |
|      - |  194 | `						"Cannot throw objects that do not implement Throwable";` |
|      3 |  195 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|      3 |  196 | `					PH7_MemObjInit(pVm,&sArg);` |
|      3 |  197 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|      3 |  198 | `					apArg[0] = &sArg;` |
|      3 |  199 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|      3 |  200 | `					PH7_MemObjRelease(&sArg);` |
|      1 |  201 | `				}` |
|      3 |  202 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|      3 |  203 | `				PH7_ClassInstanceUnref(pErrInst);` |
|      3 |  204 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  205 | `					VM_EXIT_ABORT;` |
|      - |  206 | `				}` |
|      2 |  207 | `			}else{` |
|      - |  208 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|    ! 0 |  209 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|    ! 0 |  210 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  211 | `					VM_EXIT_ABORT;` |
|      - |  212 | `				}` |
|      - |  213 | `			}` |
|      2 |  214 | `		}else{` |
|      - |  215 | `			/* Throw the exception */` |
|    735 |  216 | `			rc = VmThrowException(&(*pVm),pThis);` |
|    735 |  217 | `			if( rc == SXERR_ABORT ){` |
|      - |  218 | `				/* Abort processing immediately */` |
|     28 |  219 | `				VM_EXIT_ABORT;` |
|      - |  220 | `			}` |
|      - |  221 | `		}` |
|    359 |  222 | `	}else{` |
|      - |  223 | `		/* Expecting a class instance */` |
|    ! 0 |  224 | `		VmUncaughtException(&(*pVm),0);` |
|      - |  225 | `		/* Pre-split latent bug preserved verbatim: the original arm discarded` |
|      - |  226 | `		 * VmUncaughtException's status and tested the dispatch loop's STALE rc` |
|      - |  227 | `		 * (whatever the previous instruction left), which in practice was never` |
|      - |  228 | `		 * SXERR_ABORT — so this branch effectively never aborted. Keep that` |
|      - |  229 | `		 * de-facto behavior deterministic here; the real fix (testing the` |
|      - |  230 | `		 * call's own status) is a recorded correctness follow-up. */` |
|    ! 0 |  231 | `		rc = SXRET_OK;` |
|    ! 0 |  232 | `		if( rc == SXERR_ABORT ){` |
|      - |  233 | `			/* Abort processing immediately */` |
|    ! 0 |  234 | `			VM_EXIT_ABORT;` |
|      - |  235 | `		}` |
|      - |  236 | `	}` |
|      - |  237 | `	/* Pop the top entry */` |
|    713 |  238 | `	VmPopOperand(&pTos,1);` |
|      - |  239 | `	/* ROOT C: throw caught by an inline try in THIS exec -> jump to its catch/finally` |
|      - |  240 | `	 * (draining any mid-expression operands back to the try's base). */` |
|    713 |  241 | `	PH7_INLINE_RESUME_BREAK()` |
|      - |  242 | `	/* pInlineInstr still set here means an inline try in an OUTER exec caught it (e.g. a` |
|      - |  243 | ``	 * throwing sub-generator delegated via `yield from`): propagate so the owning exec lands. */`` |
|    699 |  244 | `	if( rc == PH7_EXCEPTION \|\| pVm->pResumeFrame \|\| pVm->pInlineInstr ){` |
|      - |  245 | ``		/* The throw was handled by a `catch` that ran IN PLACE — either this try's own`` |
|      - |  246 | `		 * finally threw past itself superseding it (rc == PH7_EXCEPTION), or the catch` |
|      - |  247 | `		 * sits several frames above this one (pVm->pResumeFrame recorded). Resume at the` |
|      - |  248 | `		 * catching body's landing pad if that body is THIS exec (VmRecordedResume), else` |
|      - |  249 | `		 * unwind so the owning exec lands. Without this a throw caught at an enclosing` |
|      - |  250 | `		 * frame would blindly jump to nJump (this throw's lexically-nearest try) and run` |
|      - |  251 | `		 * dead code after it (ROOT B, face a) or continue a callee as if it never threw` |
|      - |  252 | `		 * (face c). */` |
|      - |  253 | `		sxi32 iResumePc;` |
|    623 |  254 | `		if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){` |
|    213 |  255 | `			pc = iResumePc;` |
|    213 |  256 | `			VM_EXIT_BREAK;` |
|      - |  257 | `		}` |
|    415 |  258 | `		VM_EXIT_EXCEPTION;` |
|      - |  259 | `	}` |
|      - |  260 | `	/* No in-place catch recorded: this throw's own enclosing try caught it (the` |
|      - |  261 | `	 * common case; its landing pad is exactly nJump). Perform an unconditional jump` |
|      - |  262 | `	 * to the try's OP_POP_EXCEPTION landing pad, which tears down the try frame, runs` |
|      - |  263 | ``	 * finally, and (when a catch/finally issued a `return`) materializes the body`` |
|      - |  264 | `	 * frame's pending return. Routing the return through OP_POP_EXCEPTION keeps the` |
|      - |  265 | `	 * frame stack balanced. */` |
|     80 |  266 | `	pc = nJump - 1;` |
|     80 |  267 | `	VM_EXIT_BREAK;` |
|    ! 0 |  268 | `	VM_EXIT_BREAK;` |
|    371 |  269 | `}` |
|      - |  270 |  |
|      - |  271 | `/*` |
|      - |  272 | ` * OP_SWITCH: body moved verbatim from the OP_SWITCH arm of` |
|      - |  273 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  274 | ` */` |
|    324 |  275 | `PH7_PRIVATE VmOpRc VmExecOpSwitch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  276 | `{` |
|    329 |  277 | `	ph7_value *pTos = pState->pTos;` |
|    329 |  278 | `	ph7_value *pStack = pState->pStack;` |
|    329 |  279 | `	VmInstr *aInstr = pState->aInstr;` |
|    329 |  280 | `	sxi32 pc = pState->pc;` |
|      - |  281 | `	sxi32 rc;` |
|    162 |  282 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    329 |  283 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|      - |  284 | `	ph7_case_expr *aCase,*pCase;` |
|      - |  285 | `	ph7_value sValue,sCaseValue;` |
|      - |  286 | `	sxu32 n,nEntry;` |
|      - |  287 | `#ifdef UNTRUST` |
|      - |  288 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|      - |  289 | `		VM_EXIT_ABORT;` |
|      - |  290 | `	}` |
|      - |  291 | `#endif` |
|      - |  292 | `	/* Point to the case table  */` |
|    329 |  293 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|    329 |  294 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|      - |  295 | `	/* Select the appropriate case block to execute */` |
|    329 |  296 | `	PH7_MemObjInit(pVm,&sValue);` |
|    329 |  297 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|   1211 |  298 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|   1209 |  299 | `		pCase = &aCase[n];` |
|   1209 |  300 | `		PH7_MemObjLoad(pTos,&sValue);` |
|      - |  301 | `		/* Execute the case expression first */` |
|   1209 |  302 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue,FALSE);` |
|      - |  303 | `		/* Compare the two expression */` |
|   1209 |  304 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|   1209 |  305 | `		PH7_MemObjRelease(&sValue);` |
|   1209 |  306 | `		PH7_MemObjRelease(&sCaseValue);` |
|   1209 |  307 | `		if( rc == 0 ){` |
|      - |  308 | `			/* Value match,jump to this block */` |
|    327 |  309 | `			pc = pCase->nStart - 1;` |
|    327 |  310 | `			break;` |
|      - |  311 | `		}` |
|    446 |  312 | `	}` |
|    329 |  313 | `	VmPopOperand(&pTos,1);` |
|    329 |  314 | `	if( n >= nEntry ){` |
|      - |  315 | `		/* No approprite case to execute,jump to the default case */` |
|      3 |  316 | `		if( pSwitch->nDefault > 0 ){` |
|      3 |  317 | `			pc = pSwitch->nDefault - 1;` |
|      2 |  318 | `		}else{` |
|      - |  319 | `			/* No default case,jump out of this switch */` |
|    ! 0 |  320 | `			pc = pSwitch->nOut - 1;` |
|      - |  321 | `		}` |
|      1 |  322 | `	}` |
|    329 |  323 | `	VM_EXIT_BREAK;` |
|    ! 0 |  324 | `	VM_EXIT_BREAK;` |
|      5 |  325 | `}` |
|      - |  326 |  |
|      - |  327 | `/*` |
|      - |  328 | ` * OP_CATCH: body moved verbatim from the OP_CATCH arm of` |
|      - |  329 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  330 | ` */` |
|     66 |  331 | `PH7_PRIVATE VmOpRc VmExecOpCatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  332 | `{` |
|     71 |  333 | `	ph7_value *pTos = pState->pTos;` |
|     71 |  334 | `	ph7_value *pStack = pState->pStack;` |
|     71 |  335 | `	VmInstr *aInstr = pState->aInstr;` |
|     71 |  336 | `	sxi32 pc = pState->pc;` |
|      - |  337 | `	sxi32 rc;` |
|     33 |  338 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - |  339 | `	/* BYTECODE stage 2b: mutable state (pInflight) lives on the LIVE activation` |
|      - |  340 | `	 * of this try, not the compiled p3 (which VmThrowInline kept on aException` |
|      - |  341 | `	 * marked iInCatch). Compiled fields (sEntry) are shared either way. */` |
|     71 |  342 | `	ph7_exception *pExcC = (ph7_exception *)pInstr->p3;` |
|     71 |  343 | `	ph7_exception *pExc = VmExcLive(&(*pVm),pExcC);` |
|     71 |  344 | `	ph7_exception_block *pCatch = (ph7_exception_block *)SySetAt(&pExcC->sEntry,(sxu32)pInstr->iP1);` |
|     71 |  345 | `	ph7_class_instance *pBind = pExc ? pExc->pInflight : 0;` |
|     71 |  346 | `	VmFrame *pBody = VmSkipExceptionFrames(pVm->pFrame);` |
|     71 |  347 | `	pBody->iFlags &= ~VM_FRAME_THROW;` |
|     71 |  348 | `	if( pCatch && pBind && pCatch->sThis.nByte > 0 ){` |
|      - |  349 | `		/* sThis empty => PHP 8.0 non-capturing catch (catch (Type) {}): the` |
|      - |  350 | `		 * exception is caught but not bound to any variable. */` |
|     69 |  351 | `		ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|     69 |  352 | `		if( pObj ){` |
|      - |  353 | `			/* Overwrite-then-release (mirrors PH7_MemObjStore): pin the new instance,` |
|      - |  354 | `			 * free the slot's prior contents, then rebind. */` |
|     69 |  355 | `			pBind->iRef++;` |
|     69 |  356 | `			PH7_MemObjRelease(pObj);` |
|     69 |  357 | `			pObj->x.pOther = pBind;` |
|     69 |  358 | `			MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     32 |  359 | `		}` |
|     32 |  360 | `	}` |
|     71 |  361 | `	if( pBind ){` |
|      - |  362 | `		/* Drop the hold VmThrowInline took across the redirect. */` |
|     71 |  363 | `		PH7_ClassInstanceUnref(pBind);` |
|     33 |  364 | `	}` |
|     71 |  365 | `	if( pExc ){` |
|     71 |  366 | `		pExc->pInflight = 0;` |
|     33 |  367 | `	}` |
|     71 |  368 | `	VM_EXIT_BREAK;` |
|    ! 0 |  369 | `	VM_EXIT_BREAK;` |
|      5 |  370 | `}` |
|      - |  371 |  |
|      - |  372 | `/*` |
|      - |  373 | ` * OP_LOAD_EXCEPTION: body moved verbatim from the OP_LOAD_EXCEPTION arm of` |
|      - |  374 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  375 | ` */` |
|   2664 |  376 | `PH7_PRIVATE VmOpRc VmExecOpLoadException(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  377 | `{` |
|   2669 |  378 | `	ph7_value *pTos = pState->pTos;` |
|   2669 |  379 | `	ph7_value *pStack = pState->pStack;` |
|   2669 |  380 | `	VmInstr *aInstr = pState->aInstr;` |
|   2669 |  381 | `	sxi32 pc = pState->pc;` |
|      - |  382 | `	sxi32 rc;` |
|   1332 |  383 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - |  384 | `	/* BYTECODE stage 2b: push a fresh ACTIVATION of this lexical try (own` |
|      - |  385 | `	 * mutable state per entry — see VmExcActivate), never the shared` |
|      - |  386 | `	 * compiled object. */` |
|   2669 |  387 | `	ph7_exception *pException = VmExcActivate(&(*pVm),(ph7_exception *)pInstr->p3);` |
|      - |  388 | `	VmFrame *pFrameLocal;` |
|   2669 |  389 | `	if( pException == 0 ){` |
|    ! 0 |  390 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|    ! 0 |  391 | `		VM_EXIT_ABORT;` |
|      - |  392 | `	}` |
|      - |  393 | `	/* Create the exception frame BEFORE publishing the activation, so an OOM` |
|      - |  394 | `	 * abort cannot orphan a pushed entry with no frame behind it. */` |
|   2669 |  395 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|   2669 |  396 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  397 | `		VmExcRelease(&(*pVm),pException);` |
|    ! 0 |  398 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|    ! 0 |  399 | `		VM_EXIT_ABORT;` |
|      - |  400 | `	}` |
|   2669 |  401 | `	if( SXRET_OK != SySetPut(&pVm->aException,(const void *)&pException) ){` |
|    ! 0 |  402 | `		VmExcRelease(&(*pVm),pException);` |
|    ! 0 |  403 | `		VmLeaveFrame(&(*pVm));` |
|    ! 0 |  404 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|    ! 0 |  405 | `		VM_EXIT_ABORT;` |
|      - |  406 | `	}` |
|      - |  407 | `	/* Mark the special frame */` |
|   2669 |  408 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|   2669 |  409 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|      - |  410 | `	/* Record the landing pad on the exception too, so an in-place catch can resume` |
|      - |  411 | `	 * the throwing site at THIS try (survives the exception frame's teardown), plus` |
|      - |  412 | `	 * the bytecode array it indexes — the resume only fires in the exec running that` |
|      - |  413 | `	 * array, so a mini-program (inline try in a catch/finally) and the body that` |
|      - |  414 | `	 * shares its frame don't mis-apply each other's landing pad. iLandingPc mirrors the` |
|      - |  415 | `	 * frame's iExceptionJump just set above — reuse it so the two can't drift. */` |
|   2669 |  416 | `	pException->iLandingPc = pFrameLocal->iExceptionJump;` |
|   2669 |  417 | `	pException->pOwnerInstr = (void *)aInstr;` |
|      - |  418 | `	/* Operand-stack base at try entry (0-based TOS index; -1 when empty). The post-try` |
|      - |  419 | `	 * landing pad is reached with the stack back at this depth; Generator::throw()` |
|      - |  420 | `	 * inject-at-yield drains to it before landing (a mid-expression yield leaves the` |
|      - |  421 | `	 * abandoned expression's operands above this base). Normal throws are already here. */` |
|   2669 |  422 | `	pException->iStackDepth = (sxi32)(pTos - pStack);` |
|      - |  423 | `	/* '@' depth at try entry — see ph7_exception.iErrSuppress */` |
|   2669 |  424 | `	pException->iErrSuppress = pVm->nErrSuppress;` |
|      - |  425 | `	/* Point to the frame that trigger the exception */` |
|   2669 |  426 | `	pFrameLocal = pFrameLocal->pParent;` |
|   2669 |  427 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   2669 |  428 | `	pException->pFrame = pFrameLocal;` |
|   2669 |  429 | `	VM_EXIT_BREAK;` |
|    ! 0 |  430 | `	VM_EXIT_BREAK;` |
|   1337 |  431 | `}` |
|      - |  432 |  |
