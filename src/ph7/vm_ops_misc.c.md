# src/ph7/vm_ops_misc.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 214/248 lines (86.29%)

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
|  50882 |   28 | `PH7_PRIVATE VmOpRc VmExecOpConsume(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |   29 | `{` |
|  50887 |   30 | `	ph7_value *pTos = pState->pTos;` |
|  50887 |   31 | `	ph7_value *pStack = pState->pStack;` |
|  50887 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|  50887 |   33 | `	sxi32 pc = pState->pc;` |
|      - |   34 | `	sxi32 rc;` |
|  25441 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  50887 |   36 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|  50887 |   37 | `	ph7_value *pCur,*pOut = pTos;` |
|      - |   38 |  |
|  50887 |   39 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|  50887 |   40 | `	pCur = pOut;` |
|      - |   41 | `	/* Start the consume process  */` |
| 101769 |   42 | `	while( pOut <= pTos ){` |
|      - |   43 | `		/* Force a string cast */` |
|  50887 |   44 | `		if( (pOut->iFlags & MEMOBJ_STRING) == 0 ){` |
|   3027 |   45 | `			PH7_MemObjToString(pOut);` |
|   1511 |   46 | `		}` |
|  50887 |   47 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|      - |   48 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|      - |   49 | `			/* Invoke the output consumer callback */` |
|  38313 |   50 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|  38313 |   51 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|  38313 |   52 | `			SyBlobRelease(&pOut->sBlob);` |
|  38313 |   53 | `			if( rc == SXERR_ABORT ){` |
|      - |   54 | `				/* Output consumer callback request an operation abort. */` |
|    ! 0 |   55 | `				VM_EXIT_ABORT;` |
|      - |   56 | `			}` |
|  19154 |   57 | `		}` |
|  50887 |   58 | `		pOut++;` |
|      5 |   59 | `	}` |
|  50887 |   60 | `	pTos = &pCur[-1];` |
|  50887 |   61 | `	VM_EXIT_BREAK;` |
|    ! 0 |   62 | `	VM_EXIT_BREAK;` |
|  25446 |   63 | `}` |
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
|      - |  160 | ` * OP_THROW: body moved verbatim from the OP_THROW arm of` |
|      - |  161 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  162 | ` */` |
|    732 |  163 | `PH7_PRIVATE VmOpRc VmExecOpThrow(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  164 | `{` |
|    737 |  165 | `	ph7_value *pTos = pState->pTos;` |
|    737 |  166 | `	ph7_value *pStack = pState->pStack;` |
|    737 |  167 | `	VmInstr *aInstr = pState->aInstr;` |
|    737 |  168 | `	sxi32 pc = pState->pc;` |
|      - |  169 | `	sxi32 rc;` |
|    366 |  170 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    737 |  171 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
|    737 |  172 | `	sxu32 nJump = pInstr->iP2;` |
|      - |  173 | `#ifdef UNTRUST` |
|      - |  174 | `	if( pTos < pStack ){` |
|      - |  175 | `		VM_EXIT_ABORT;` |
|      - |  176 | `	}` |
|      - |  177 | `#endif` |
|    737 |  178 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|      - |  179 | `	/* Tell the upper layer that an exception was thrown */` |
|    737 |  180 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
|    737 |  181 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
|    737 |  182 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|      - |  183 | `		ph7_class *pThrowable;` |
|      - |  184 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
|    737 |  185 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
|    738 |  186 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|      - |  187 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior.` |
|      - |  188 | `			 * Error::__construct is defined in the built-in library and` |
|      - |  189 | `			 * cannot realistically fail, so we do not check its return. */` |
|      3 |  190 | `			ph7_class *pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|      3 |  191 | `			ph7_class_instance *pErrInst = 0;` |
|      3 |  192 | `			if( pErrorClass ){` |
|      3 |  193 | `				pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|      1 |  194 | `			}` |
|      3 |  195 | `			if( pErrInst ){` |
|      - |  196 | `				ph7_class_method *pCons;` |
|      3 |  197 | `				pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|      3 |  198 | `				if( pCons ){` |
|      - |  199 | `					ph7_value sArg;` |
|      - |  200 | `					ph7_value *apArg[1];` |
|      - |  201 | `					SyString sMsgStr;` |
|      - |  202 | `					static const char zErrMsg[] =` |
|      - |  203 | `						"Cannot throw objects that do not implement Throwable";` |
|      3 |  204 | `					SyStringInitFromBuf(&sMsgStr,zErrMsg,sizeof(zErrMsg)-1);` |
|      3 |  205 | `					PH7_MemObjInit(pVm,&sArg);` |
|      3 |  206 | `					PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|      3 |  207 | `					apArg[0] = &sArg;` |
|      3 |  208 | `					PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|      3 |  209 | `					PH7_MemObjRelease(&sArg);` |
|      1 |  210 | `				}` |
|      3 |  211 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|      3 |  212 | `				PH7_ClassInstanceUnref(pErrInst);` |
|      3 |  213 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  214 | `					VM_EXIT_ABORT;` |
|      - |  215 | `				}` |
|      2 |  216 | `			}else{` |
|      - |  217 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|    ! 0 |  218 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|    ! 0 |  219 | `				if( rc == SXERR_ABORT ){` |
|    ! 0 |  220 | `					VM_EXIT_ABORT;` |
|      - |  221 | `				}` |
|      - |  222 | `			}` |
|      2 |  223 | `		}else{` |
|      - |  224 | `			/* Throw the exception */` |
|    735 |  225 | `			rc = VmThrowException(&(*pVm),pThis);` |
|    735 |  226 | `			if( rc == SXERR_ABORT ){` |
|      - |  227 | `				/* Abort processing immediately */` |
|     28 |  228 | `				VM_EXIT_ABORT;` |
|      - |  229 | `			}` |
|      - |  230 | `		}` |
|    359 |  231 | `	}else{` |
|      - |  232 | `		/* Expecting a class instance */` |
|    ! 0 |  233 | `		VmUncaughtException(&(*pVm),0);` |
|      - |  234 | `		/* Pre-split latent bug preserved verbatim: the original arm discarded` |
|      - |  235 | `		 * VmUncaughtException's status and tested the dispatch loop's STALE rc` |
|      - |  236 | `		 * (whatever the previous instruction left), which in practice was never` |
|      - |  237 | `		 * SXERR_ABORT — so this branch effectively never aborted. Keep that` |
|      - |  238 | `		 * de-facto behavior deterministic here; the real fix (testing the` |
|      - |  239 | `		 * call's own status) is a recorded correctness follow-up. */` |
|    ! 0 |  240 | `		rc = SXRET_OK;` |
|    ! 0 |  241 | `		if( rc == SXERR_ABORT ){` |
|      - |  242 | `			/* Abort processing immediately */` |
|    ! 0 |  243 | `			VM_EXIT_ABORT;` |
|      - |  244 | `		}` |
|      - |  245 | `	}` |
|      - |  246 | `	/* Pop the top entry */` |
|    713 |  247 | `	VmPopOperand(&pTos,1);` |
|      - |  248 | `	/* ROOT C: throw caught by an inline try in THIS exec -> jump to its catch/finally` |
|      - |  249 | `	 * (draining any mid-expression operands back to the try's base). */` |
|    713 |  250 | `	PH7_INLINE_RESUME_BREAK()` |
|      - |  251 | `	/* pInlineInstr still set here means an inline try in an OUTER exec caught it (e.g. a` |
|      - |  252 | ``	 * throwing sub-generator delegated via `yield from`): propagate so the owning exec lands. */`` |
|    699 |  253 | `	if( rc == PH7_EXCEPTION \|\| pVm->pResumeFrame \|\| pVm->pInlineInstr ){` |
|      - |  254 | ``		/* The throw was handled by a `catch` that ran IN PLACE — either this try's own`` |
|      - |  255 | `		 * finally threw past itself superseding it (rc == PH7_EXCEPTION), or the catch` |
|      - |  256 | `		 * sits several frames above this one (pVm->pResumeFrame recorded). Resume at the` |
|      - |  257 | `		 * catching body's landing pad if that body is THIS exec (VmRecordedResume), else` |
|      - |  258 | `		 * unwind so the owning exec lands. Without this a throw caught at an enclosing` |
|      - |  259 | `		 * frame would blindly jump to nJump (this throw's lexically-nearest try) and run` |
|      - |  260 | `		 * dead code after it (ROOT B, face a) or continue a callee as if it never threw` |
|      - |  261 | `		 * (face c). */` |
|      - |  262 | `		sxi32 iResumePc;` |
|    623 |  263 | `		if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){` |
|    213 |  264 | `			pc = iResumePc;` |
|    213 |  265 | `			VM_EXIT_BREAK;` |
|      - |  266 | `		}` |
|    415 |  267 | `		VM_EXIT_EXCEPTION;` |
|      - |  268 | `	}` |
|      - |  269 | `	/* No in-place catch recorded: this throw's own enclosing try caught it (the` |
|      - |  270 | `	 * common case; its landing pad is exactly nJump). Perform an unconditional jump` |
|      - |  271 | `	 * to the try's OP_POP_EXCEPTION landing pad, which tears down the try frame, runs` |
|      - |  272 | ``	 * finally, and (when a catch/finally issued a `return`) materializes the body`` |
|      - |  273 | `	 * frame's pending return. Routing the return through OP_POP_EXCEPTION keeps the` |
|      - |  274 | `	 * frame stack balanced. */` |
|     79 |  275 | `	pc = nJump - 1;` |
|     79 |  276 | `	VM_EXIT_BREAK;` |
|    ! 0 |  277 | `	VM_EXIT_BREAK;` |
|    371 |  278 | `}` |
|      - |  279 |  |
|      - |  280 | `/*` |
|      - |  281 | ` * OP_SWITCH: body moved verbatim from the OP_SWITCH arm of` |
|      - |  282 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  283 | ` */` |
|    324 |  284 | `PH7_PRIVATE VmOpRc VmExecOpSwitch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  285 | `{` |
|    329 |  286 | `	ph7_value *pTos = pState->pTos;` |
|    329 |  287 | `	ph7_value *pStack = pState->pStack;` |
|    329 |  288 | `	VmInstr *aInstr = pState->aInstr;` |
|    329 |  289 | `	sxi32 pc = pState->pc;` |
|      - |  290 | `	sxi32 rc;` |
|    162 |  291 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|    329 |  292 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|      - |  293 | `	ph7_case_expr *aCase,*pCase;` |
|      - |  294 | `	ph7_value sValue,sCaseValue;` |
|      - |  295 | `	sxu32 n,nEntry;` |
|      - |  296 | `#ifdef UNTRUST` |
|      - |  297 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|      - |  298 | `		VM_EXIT_ABORT;` |
|      - |  299 | `	}` |
|      - |  300 | `#endif` |
|      - |  301 | `	/* Point to the case table  */` |
|    329 |  302 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|    329 |  303 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|      - |  304 | `	/* Select the appropriate case block to execute */` |
|    329 |  305 | `	PH7_MemObjInit(pVm,&sValue);` |
|    329 |  306 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|   1211 |  307 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|   1209 |  308 | `		pCase = &aCase[n];` |
|   1209 |  309 | `		PH7_MemObjLoad(pTos,&sValue);` |
|      - |  310 | `		/* Execute the case expression first */` |
|   1209 |  311 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue,FALSE);` |
|      - |  312 | `		/* Compare the two expression */` |
|   1209 |  313 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|   1209 |  314 | `		PH7_MemObjRelease(&sValue);` |
|   1209 |  315 | `		PH7_MemObjRelease(&sCaseValue);` |
|   1209 |  316 | `		if( rc == 0 ){` |
|      - |  317 | `			/* Value match,jump to this block */` |
|    327 |  318 | `			pc = pCase->nStart - 1;` |
|    327 |  319 | `			break;` |
|      - |  320 | `		}` |
|    446 |  321 | `	}` |
|    329 |  322 | `	VmPopOperand(&pTos,1);` |
|    329 |  323 | `	if( n >= nEntry ){` |
|      - |  324 | `		/* No approprite case to execute,jump to the default case */` |
|      3 |  325 | `		if( pSwitch->nDefault > 0 ){` |
|      3 |  326 | `			pc = pSwitch->nDefault - 1;` |
|      2 |  327 | `		}else{` |
|      - |  328 | `			/* No default case,jump out of this switch */` |
|    ! 0 |  329 | `			pc = pSwitch->nOut - 1;` |
|      - |  330 | `		}` |
|      1 |  331 | `	}` |
|    329 |  332 | `	VM_EXIT_BREAK;` |
|    ! 0 |  333 | `	VM_EXIT_BREAK;` |
|      5 |  334 | `}` |
|      - |  335 |  |
|      - |  336 | `/*` |
|      - |  337 | ` * OP_CATCH: body moved verbatim from the OP_CATCH arm of` |
|      - |  338 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  339 | ` */` |
|     66 |  340 | `PH7_PRIVATE VmOpRc VmExecOpCatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  341 | `{` |
|     71 |  342 | `	ph7_value *pTos = pState->pTos;` |
|     71 |  343 | `	ph7_value *pStack = pState->pStack;` |
|     71 |  344 | `	VmInstr *aInstr = pState->aInstr;` |
|     71 |  345 | `	sxi32 pc = pState->pc;` |
|      - |  346 | `	sxi32 rc;` |
|     33 |  347 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - |  348 | `	/* BYTECODE stage 2b: mutable state (pInflight) lives on the LIVE activation` |
|      - |  349 | `	 * of this try, not the compiled p3 (which VmThrowInline kept on aException` |
|      - |  350 | `	 * marked iInCatch). Compiled fields (sEntry) are shared either way. */` |
|     71 |  351 | `	ph7_exception *pExcC = (ph7_exception *)pInstr->p3;` |
|     71 |  352 | `	ph7_exception *pExc = VmExcLive(&(*pVm),pExcC);` |
|     71 |  353 | `	ph7_exception_block *pCatch = (ph7_exception_block *)SySetAt(&pExcC->sEntry,(sxu32)pInstr->iP1);` |
|     71 |  354 | `	ph7_class_instance *pBind = pExc ? pExc->pInflight : 0;` |
|     71 |  355 | `	VmFrame *pBody = VmSkipExceptionFrames(pVm->pFrame);` |
|     71 |  356 | `	pBody->iFlags &= ~VM_FRAME_THROW;` |
|     71 |  357 | `	if( pCatch && pBind && pCatch->sThis.nByte > 0 ){` |
|      - |  358 | `		/* sThis empty => PHP 8.0 non-capturing catch (catch (Type) {}): the` |
|      - |  359 | `		 * exception is caught but not bound to any variable. */` |
|     69 |  360 | `		ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|     69 |  361 | `		if( pObj ){` |
|      - |  362 | `			/* Overwrite-then-release (mirrors PH7_MemObjStore): pin the new instance,` |
|      - |  363 | `			 * free the slot's prior contents, then rebind. */` |
|     69 |  364 | `			pBind->iRef++;` |
|     69 |  365 | `			PH7_MemObjRelease(pObj);` |
|     69 |  366 | `			pObj->x.pOther = pBind;` |
|     69 |  367 | `			MemObjSetType(pObj,MEMOBJ_OBJ);` |
|     32 |  368 | `		}` |
|     32 |  369 | `	}` |
|     71 |  370 | `	if( pBind ){` |
|      - |  371 | `		/* Drop the hold VmThrowInline took across the redirect. */` |
|     71 |  372 | `		PH7_ClassInstanceUnref(pBind);` |
|     33 |  373 | `	}` |
|     71 |  374 | `	if( pExc ){` |
|     71 |  375 | `		pExc->pInflight = 0;` |
|     33 |  376 | `	}` |
|     71 |  377 | `	VM_EXIT_BREAK;` |
|    ! 0 |  378 | `	VM_EXIT_BREAK;` |
|      5 |  379 | `}` |
|      - |  380 |  |
|      - |  381 | `/*` |
|      - |  382 | ` * OP_LOAD_EXCEPTION: body moved verbatim from the OP_LOAD_EXCEPTION arm of` |
|      - |  383 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|      - |  384 | ` */` |
|   2674 |  385 | `PH7_PRIVATE VmOpRc VmExecOpLoadException(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|      5 |  386 | `{` |
|   2679 |  387 | `	ph7_value *pTos = pState->pTos;` |
|   2679 |  388 | `	ph7_value *pStack = pState->pStack;` |
|   2679 |  389 | `	VmInstr *aInstr = pState->aInstr;` |
|   2679 |  390 | `	sxi32 pc = pState->pc;` |
|      - |  391 | `	sxi32 rc;` |
|   1337 |  392 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      - |  393 | `	/* BYTECODE stage 2b: push a fresh ACTIVATION of this lexical try (own` |
|      - |  394 | `	 * mutable state per entry — see VmExcActivate), never the shared` |
|      - |  395 | `	 * compiled object. */` |
|   2679 |  396 | `	ph7_exception *pException = VmExcActivate(&(*pVm),(ph7_exception *)pInstr->p3);` |
|      - |  397 | `	VmFrame *pFrameLocal;` |
|   2679 |  398 | `	if( pException == 0 ){` |
|    ! 0 |  399 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|    ! 0 |  400 | `		VM_EXIT_ABORT;` |
|      - |  401 | `	}` |
|      - |  402 | `	/* Create the exception frame BEFORE publishing the activation, so an OOM` |
|      - |  403 | `	 * abort cannot orphan a pushed entry with no frame behind it. */` |
|   2679 |  404 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
|   2679 |  405 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  406 | `		VmExcRelease(&(*pVm),pException);` |
|    ! 0 |  407 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|    ! 0 |  408 | `		VM_EXIT_ABORT;` |
|      - |  409 | `	}` |
|   2679 |  410 | `	if( SXRET_OK != SySetPut(&pVm->aException,(const void *)&pException) ){` |
|    ! 0 |  411 | `		VmExcRelease(&(*pVm),pException);` |
|    ! 0 |  412 | `		VmLeaveFrame(&(*pVm));` |
|    ! 0 |  413 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|    ! 0 |  414 | `		VM_EXIT_ABORT;` |
|      - |  415 | `	}` |
|      - |  416 | `	/* Mark the special frame */` |
|   2679 |  417 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
|   2679 |  418 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|      - |  419 | `	/* Record the landing pad on the exception too, so an in-place catch can resume` |
|      - |  420 | `	 * the throwing site at THIS try (survives the exception frame's teardown), plus` |
|      - |  421 | `	 * the bytecode array it indexes — the resume only fires in the exec running that` |
|      - |  422 | `	 * array, so a mini-program (inline try in a catch/finally) and the body that` |
|      - |  423 | `	 * shares its frame don't mis-apply each other's landing pad. iLandingPc mirrors the` |
|      - |  424 | `	 * frame's iExceptionJump just set above — reuse it so the two can't drift. */` |
|   2679 |  425 | `	pException->iLandingPc = pFrameLocal->iExceptionJump;` |
|   2679 |  426 | `	pException->pOwnerInstr = (void *)aInstr;` |
|      - |  427 | `	/* Operand-stack base at try entry (0-based TOS index; -1 when empty). The post-try` |
|      - |  428 | `	 * landing pad is reached with the stack back at this depth; Generator::throw()` |
|      - |  429 | `	 * inject-at-yield drains to it before landing (a mid-expression yield leaves the` |
|      - |  430 | `	 * abandoned expression's operands above this base). Normal throws are already here. */` |
|   2679 |  431 | `	pException->iStackDepth = (sxi32)(pTos - pStack);` |
|      - |  432 | `	/* '@' depth at try entry — see ph7_exception.iErrSuppress */` |
|   2679 |  433 | `	pException->iErrSuppress = pVm->nErrSuppress;` |
|      - |  434 | `	/* Point to the frame that trigger the exception */` |
|   2679 |  435 | `	pFrameLocal = pFrameLocal->pParent;` |
|   2679 |  436 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|   2679 |  437 | `	pException->pFrame = pFrameLocal;` |
|   2679 |  438 | `	VM_EXIT_BREAK;` |
|    ! 0 |  439 | `	VM_EXIT_BREAK;` |
|   1342 |  440 | `}` |
|      - |  441 |  |
