# src/ph7/vm_ops_misc.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 223/265 lines (84.15%)

[Root index](../../index.md) | [Directory index](index.md)

|    Hits | Line | Source |
| ------: | ---: | :--- |
|       - |    1 | `/**` |
|       - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|       - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|       - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|       - |    5 | ` */` |
|       - |    6 | `#include "ph7int.h"` |
|       - |    7 | `/*` |
|       - |    8 | ` * Section:` |
|       - |    9 | ` *    Opcode handlers extracted from vm.c's dispatch loop. Each handler runs` |
|       - |   10 | ` *    one opcode arm against the caller's VmExecState: the loop syncs pTos/pc` |
|       - |   11 | ` *    in, calls the handler, reloads them and routes the returned VmOpRc onto` |
|       - |   12 | ` *    its labels (same idiom as VmCallFinish).` |
|       - |   13 | ` * Status:` |
|       - |   14 | ` *    Stable.` |
|       - |   15 | ` */` |
|       - |   16 | `/* Bind the dispatch-routing exits to handler semantics (see vm_dispatch.h),` |
|       - |   17 | ` * and map the macros' sState references onto our state parameter. */` |
|       - |   18 | `#define VM_EXIT_BREAK      { pState->pTos = pTos; pState->pc = pc; return VM_OP_NEXT; }` |
|       - |   19 | `#define VM_EXIT_ABORT      { pState->pTos = pTos; pState->pc = pc; return VM_OP_ABORT; }` |
|       - |   20 | `#define VM_EXIT_EXCEPTION  { pState->pTos = pTos; pState->pc = pc; return VM_OP_EXCEPTION; }` |
|       - |   21 | `#include "vm_dispatch.h"` |
|       - |   22 | `#define sState (*pState)` |
|       - |   23 |  |
|       - |   24 | `/*` |
|       - |   25 | ` * OP_CONSUME: body moved verbatim from the OP_CONSUME arm of` |
|       - |   26 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |   27 | ` */` |
|   57242 |   28 | `PH7_PRIVATE VmOpRc VmExecOpConsume(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   29 | `{` |
|   57247 |   30 | `	ph7_value *pTos = pState->pTos;` |
|   57247 |   31 | `	ph7_value *pStack = pState->pStack;` |
|   57247 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|   57247 |   33 | `	sxi32 pc = pState->pc;` |
|       - |   34 | `	sxi32 rc;` |
|   28621 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   57247 |   36 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|   57247 |   37 | `	ph7_value *pCur,*pOut = pTos;` |
|       - |   38 |  |
|   57247 |   39 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|   57247 |   40 | `	pCur = pOut;` |
|       - |   41 | `	/* Start the consume process  */` |
|  114489 |   42 | `	while( pOut <= pTos ){` |
|       - |   43 | `		/* Force a string cast (echo/print: user-visible array->string warning, §2) */` |
|   57247 |   44 | `		PH7_MemObjToStringUV(pOut);` |
|   57247 |   45 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|       - |   46 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|       - |   47 | `			/* Invoke the output consumer callback */` |
|   44395 |   48 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|   44395 |   49 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|   44395 |   50 | `			SyBlobRelease(&pOut->sBlob);` |
|   44395 |   51 | `			if( rc == SXERR_ABORT ){` |
|       - |   52 | `				/* Output consumer callback request an operation abort. */` |
|     ! 0 |   53 | `				VM_EXIT_ABORT;` |
|       - |   54 | `			}` |
|   22195 |   55 | `		}` |
|   57247 |   56 | `		pOut++;` |
|       5 |   57 | `	}` |
|   57247 |   58 | `	pTos = &pCur[-1];` |
|   57247 |   59 | `	VM_EXIT_BREAK;` |
|     ! 0 |   60 | `	VM_EXIT_BREAK;` |
|   28626 |   61 | `}` |
|       - |   62 |  |
|       - |   63 | `/*` |
|       - |   64 | ` * OP_MATCH: body moved verbatim from the OP_MATCH arm of` |
|       - |   65 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |   66 | ` */` |
|     124 |   67 | `PH7_PRIVATE VmOpRc VmExecOpMatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       3 |   68 | `{` |
|     127 |   69 | `	ph7_value *pTos = pState->pTos;` |
|     127 |   70 | `	ph7_value *pStack = pState->pStack;` |
|     127 |   71 | `	VmInstr *aInstr = pState->aInstr;` |
|     127 |   72 | `	sxi32 pc = pState->pc;` |
|       - |   73 | `	sxi32 rc;` |
|      62 |   74 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     127 |   75 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|     127 |   76 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|       - |   77 | `	ph7_value sSubject,sCond,sResult;` |
|       - |   78 | `	sxu32 i,j,nArm,nCond;` |
|     127 |   79 | `	int matched = 0;` |
|       - |   80 | `#ifdef UNTRUST` |
|       - |   81 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|       - |   82 | `		VM_EXIT_ABORT;` |
|       - |   83 | `	}` |
|       - |   84 | `#endif` |
|     127 |   85 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|     127 |   86 | `	nArm = SySetUsed(&pMatch->aArms);` |
|     127 |   87 | `	PH7_MemObjInit(pVm,&sSubject);` |
|     127 |   88 | `	PH7_MemObjInit(pVm,&sCond);` |
|     127 |   89 | `	PH7_MemObjInit(pVm,&sResult);` |
|     127 |   90 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|     387 |   91 | `	for( i = 0; i < nArm && !matched; ++i ){` |
|     263 |   92 | `		pArm = &aArm[i];` |
|     263 |   93 | `		if( pArm->bDefault ){` |
|      13 |   94 | `			pDefault = pArm;` |
|      13 |   95 | `			continue;` |
|       - |   96 | `		}` |
|     251 |   97 | `		nCond = SySetUsed(&pArm->aConds);` |
|     423 |   98 | `		for( j = 0; j < nCond; ++j ){` |
|     283 |   99 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|     283 |  100 | `			if( pCondBc == 0 ){` |
|     ! 0 |  101 | `				continue;` |
|       - |  102 | `			}` |
|     283 |  103 | `			VmLocalExec(pVm,pCondBc,&sCond,FALSE);` |
|     283 |  104 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|     283 |  105 | `			PH7_MemObjRelease(&sCond);` |
|     283 |  106 | `			if( rc == 0 ){` |
|     111 |  107 | `				VmLocalExec(pVm,&pArm->aResult,&sResult,FALSE);` |
|     111 |  108 | `				matched = 1;` |
|     111 |  109 | `				break;` |
|       - |  110 | `			}` |
|      88 |  111 | `		}` |
|     127 |  112 | `	}` |
|     127 |  113 | `	if( !matched && pDefault ){` |
|      13 |  114 | `		VmLocalExec(pVm,&pDefault->aResult,&sResult,FALSE);` |
|      13 |  115 | `		matched = 1;` |
|       6 |  116 | `	}` |
|     127 |  117 | `	if( !matched ){` |
|       6 |  118 | `		const char *zType = "unknown";` |
|       - |  119 | `		char zMsg[128];` |
|       - |  120 | `		sxu32 nMsg;` |
|       6 |  121 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|     ! 0 |  122 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|     ! 0 |  123 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|       6 |  124 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|     ! 0 |  125 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|     ! 0 |  126 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|     ! 0 |  127 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|     ! 0 |  128 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|     ! 0 |  129 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|     ! 0 |  130 | `		default: break;` |
|       - |  131 | `		}` |
|       - |  132 | `		SyBlob sErrMsg;` |
|       8 |  133 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       2 |  134 | `			"Unhandled match case of type %s",zType);` |
|       6 |  135 | `		PH7_MemObjRelease(&sSubject);` |
|       6 |  136 | `		PH7_MemObjRelease(&sResult);` |
|       - |  137 | `		/* php raises a CATCHABLE \UnhandledMatchError here, so build a real` |
|       - |  138 | `		 * exception object and route it like any mid-expression throw (an` |
|       - |  139 | `		 * enclosing try in this body resumes at its landing pad; otherwise it` |
|       - |  140 | `		 * propagates and renders as uncaught, as before). Reporting it straight` |
|       - |  141 | `		 * to the uncaught renderer — what this site used to do — made the error` |
|       - |  142 | `		 * unconditionally fatal even inside try/catch. */` |
|       6 |  143 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       6 |  144 | `		SyBlobAppend(&sErrMsg,zMsg,nMsg);` |
|       6 |  145 | `		rc = VmThrowBuiltinError(&(*pVm),"UnhandledMatchError",` |
|       - |  146 | `			sizeof("UnhandledMatchError")-1,&sErrMsg);` |
|       6 |  147 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  148 | `	}` |
|     123 |  149 | `	PH7_MemObjRelease(&sSubject);` |
|       - |  150 | `	/* Replace subject on TOS with the arm result */` |
|     123 |  151 | `	PH7_MemObjStore(&sResult,pTos);` |
|     123 |  152 | `	PH7_MemObjRelease(&sResult);` |
|     123 |  153 | `	VM_EXIT_BREAK;` |
|     ! 0 |  154 | `	VM_EXIT_BREAK;` |
|      65 |  155 | `}` |
|       - |  156 |  |
|       - |  157 | `/*` |
|       - |  158 | ` * Build an \Error instance carrying zMsg (or NULL when the class is` |
|       - |  159 | ` * unavailable). Shared by OP_THROW's two "operand cannot be thrown" cases:` |
|       - |  160 | ` * php reports a non-object as "Can only throw objects" and a non-Throwable` |
|       - |  161 | ` * object as "Cannot throw objects that do not implement Throwable", both as` |
|       - |  162 | ` * ordinary catchable throws. The caller hands the instance to` |
|       - |  163 | ` * VmThrowException so the routing below sees exactly the status a normal` |
|       - |  164 | ` * throw produces. Error::__construct comes from the built-in library and` |
|       - |  165 | ` * cannot realistically fail, so its return is not checked.` |
|       - |  166 | ` */` |
|      18 |  167 | `static ph7_class_instance * VmNewThrowError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|       2 |  168 | `{` |
|       - |  169 | `	ph7_class *pErrorClass;` |
|       - |  170 | `	ph7_class_instance *pErrInst;` |
|       - |  171 | `	ph7_class_method *pCons;` |
|      20 |  172 | `	pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|      20 |  173 | `	if( pErrorClass == 0 ){` |
|     ! 0 |  174 | `		return 0;` |
|       - |  175 | `	}` |
|      20 |  176 | `	pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|      20 |  177 | `	if( pErrInst == 0 ){` |
|     ! 0 |  178 | `		return 0;` |
|       - |  179 | `	}` |
|      20 |  180 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|      20 |  181 | `	if( pCons ){` |
|       - |  182 | `		ph7_value sArg;` |
|       - |  183 | `		ph7_value *apArg[1];` |
|       - |  184 | `		SyString sMsgStr;` |
|      20 |  185 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|      20 |  186 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|      20 |  187 | `		apArg[0] = &sArg;` |
|      20 |  188 | `		PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|      20 |  189 | `		PH7_MemObjRelease(&sArg);` |
|       9 |  190 | `	}` |
|      20 |  191 | `	return pErrInst;` |
|      11 |  192 | `}` |
|       - |  193 | `/*` |
|       - |  194 | ` * OP_THROW: body moved verbatim from the OP_THROW arm of` |
|       - |  195 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  196 | ` */` |
| 1000820 |  197 | `PH7_PRIVATE VmOpRc VmExecOpThrow(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  198 | `{` |
| 1000825 |  199 | `	ph7_value *pTos = pState->pTos;` |
| 1000825 |  200 | `	ph7_value *pStack = pState->pStack;` |
| 1000825 |  201 | `	VmInstr *aInstr = pState->aInstr;` |
| 1000825 |  202 | `	sxi32 pc = pState->pc;` |
|       - |  203 | `	sxi32 rc;` |
|  500410 |  204 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 1000825 |  205 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
| 1000825 |  206 | `	sxu32 nJump = pInstr->iP2;` |
|       - |  207 | `#ifdef UNTRUST` |
|       - |  208 | `	if( pTos < pStack ){` |
|       - |  209 | `		VM_EXIT_ABORT;` |
|       - |  210 | `	}` |
|       - |  211 | `#endif` |
| 1000825 |  212 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       - |  213 | `	/* Tell the upper layer that an exception was thrown */` |
| 1000825 |  214 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
| 1000825 |  215 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
| 1000811 |  216 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|       - |  217 | `		ph7_class *pThrowable;` |
|       - |  218 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
| 1000811 |  219 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
| 1000813 |  220 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|       - |  221 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior. */` |
|       - |  222 | `			static const char zErrMsg[] =` |
|       - |  223 | `				"Cannot throw objects that do not implement Throwable";` |
|       6 |  224 | `			ph7_class_instance *pErrInst = VmNewThrowError(&(*pVm),zErrMsg,sizeof(zErrMsg)-1);` |
|       6 |  225 | `			if( pErrInst ){` |
|       6 |  226 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|       6 |  227 | `				PH7_ClassInstanceUnref(pErrInst);` |
|       6 |  228 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  229 | `					VM_EXIT_ABORT;` |
|       - |  230 | `				}` |
|       4 |  231 | `			}else{` |
|       - |  232 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|     ! 0 |  233 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|     ! 0 |  234 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  235 | `					VM_EXIT_ABORT;` |
|       - |  236 | `				}` |
|       - |  237 | `			}` |
|       4 |  238 | `		}else{` |
|       - |  239 | `			/* Throw the exception */` |
| 1000807 |  240 | `			rc = VmThrowException(&(*pVm),pThis);` |
| 1000807 |  241 | `			if( rc == SXERR_ABORT ){` |
|       - |  242 | `				/* Abort processing immediately */` |
|      34 |  243 | `				VM_EXIT_ABORT;` |
|       - |  244 | `			}` |
|       - |  245 | `		}` |
|  500393 |  246 | `	}else{` |
|       - |  247 | `		/* php raises a CATCHABLE Error for a non-object operand. This used to` |
|       - |  248 | `		 * report a bogus uncaught "Exception" and then fall into the jump below,` |
|       - |  249 | `		 * so no catch ran yet execution carried on past the try — and the branch` |
|       - |  250 | `		 * tested a STALE rc left by the previous instruction, which is the` |
|       - |  251 | `		 * latent bug the interpreter split surfaced. Build the same Error shape` |
|       - |  252 | `		 * the not-Throwable case uses and let the routing below land it. */` |
|       - |  253 | `		static const char zErrMsg[] = "Can only throw objects";` |
|      15 |  254 | `		ph7_class_instance *pErrInst = VmNewThrowError(&(*pVm),zErrMsg,sizeof(zErrMsg)-1);` |
|      15 |  255 | `		if( pErrInst ){` |
|      15 |  256 | `			rc = VmThrowException(&(*pVm),pErrInst);` |
|      15 |  257 | `			PH7_ClassInstanceUnref(pErrInst);` |
|       8 |  258 | `		}else{` |
|       - |  259 | `			/* Bootstrap failure — fall back to uncaught reporting */` |
|     ! 0 |  260 | `			rc = VmUncaughtException(&(*pVm),0);` |
|       - |  261 | `		}` |
|      15 |  262 | `		if( rc == SXERR_ABORT ){` |
|       - |  263 | `			/* Abort processing immediately */` |
|     ! 0 |  264 | `			VM_EXIT_ABORT;` |
|       - |  265 | `		}` |
|       - |  266 | `	}` |
|       - |  267 | `	/* Pop the top entry */` |
| 1000795 |  268 | `	VmPopOperand(&pTos,1);` |
|       - |  269 | `	/* ROOT C: throw caught by an inline try in THIS exec -> jump to its catch/finally` |
|       - |  270 | `	 * (draining any mid-expression operands back to the try's base). */` |
| 1000795 |  271 | `	PH7_INLINE_RESUME_BREAK()` |
|       - |  272 | `	/* pInlineInstr still set here means an inline try in an OUTER exec caught it (e.g. a` |
|       - |  273 | ``	 * throwing sub-generator delegated via `yield from`): propagate so the owning exec lands. */`` |
| 1000779 |  274 | `	if( rc == PH7_EXCEPTION \|\| pVm->pResumeFrame \|\| pVm->pInlineInstr ){` |
|       - |  275 | ``		/* The throw was handled by a `catch` that ran IN PLACE — either this try's own`` |
|       - |  276 | `		 * finally threw past itself superseding it (rc == PH7_EXCEPTION), or the catch` |
|       - |  277 | `		 * sits several frames above this one (pVm->pResumeFrame recorded). Resume at the` |
|       - |  278 | `		 * catching body's landing pad if that body is THIS exec (VmRecordedResume), else` |
|       - |  279 | `		 * unwind so the owning exec lands. Without this a throw caught at an enclosing` |
|       - |  280 | `		 * frame would blindly jump to nJump (this throw's lexically-nearest try) and run` |
|       - |  281 | `		 * dead code after it (ROOT B, face a) or continue a callee as if it never threw` |
|       - |  282 | `		 * (face c). */` |
|       - |  283 | `		sxi32 iResumePc;` |
|  900705 |  284 | `		if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){` |
|  400249 |  285 | `			PH7_RESUME_DRAIN()` |
|  300247 |  286 | `			pc = iResumePc;` |
|  300247 |  287 | `			VM_EXIT_BREAK;` |
|       - |  288 | `		}` |
|  600463 |  289 | `		VM_EXIT_EXCEPTION;` |
|       - |  290 | `	}` |
|       - |  291 | `	/* No in-place catch recorded: this throw's own enclosing try caught it (the` |
|       - |  292 | `	 * common case; its landing pad is exactly nJump). A throw-EXPRESSION` |
|       - |  293 | ``	 * (`$q = 1 + throw new E`) abandons its outer expression's pending operands`` |
|       - |  294 | `	 * above the try's base — drain to the catching activation's recorded depth` |
|       - |  295 | `	 * (matched by landing pad + bytecode array so an unrelated activation can` |
|       - |  296 | `	 * never be consulted) before jumping, or each caught throw leaks a slot.` |
|       - |  297 | `	 * Then jump to the try's OP_POP_EXCEPTION landing pad, which tears down the` |
|       - |  298 | ``	 * try frame, runs finally, and (when a catch/finally issued a `return`)`` |
|       - |  299 | `	 * materializes the body frame's pending return. Routing the return through` |
|       - |  300 | `	 * OP_POP_EXCEPTION keeps the frame stack balanced. */` |
|  100078 |  301 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|     ! 0 |  302 | `		ph7_exception *pTopExc = ((ph7_exception **)SySetBasePtr(&pVm->aException))[SySetUsed(&pVm->aException)-1];` |
|     ! 0 |  303 | `		if( pTopExc->iLandingPc == (sxu32)nJump && pTopExc->pOwnerInstr == (void *)aInstr ){` |
|     ! 0 |  304 | `			while( (sxi32)(pTos - pStack) > pTopExc->iStackDepth ){` |
|     ! 0 |  305 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 |  306 | `				pTos--;` |
|     ! 0 |  307 | `			}` |
|     ! 0 |  308 | `		}` |
|     ! 0 |  309 | `	}` |
|  100078 |  310 | `	pc = nJump - 1;` |
|  100078 |  311 | `	VM_EXIT_BREAK;` |
|     ! 0 |  312 | `	VM_EXIT_BREAK;` |
|  500415 |  313 | `}` |
|       - |  314 |  |
|       - |  315 | `/*` |
|       - |  316 | ` * OP_SWITCH: body moved verbatim from the OP_SWITCH arm of` |
|       - |  317 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  318 | ` */` |
|     328 |  319 | `PH7_PRIVATE VmOpRc VmExecOpSwitch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  320 | `{` |
|     333 |  321 | `	ph7_value *pTos = pState->pTos;` |
|     333 |  322 | `	ph7_value *pStack = pState->pStack;` |
|     333 |  323 | `	VmInstr *aInstr = pState->aInstr;` |
|     333 |  324 | `	sxi32 pc = pState->pc;` |
|       - |  325 | `	sxi32 rc;` |
|     164 |  326 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     333 |  327 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|       - |  328 | `	ph7_case_expr *aCase,*pCase;` |
|       - |  329 | `	ph7_value sValue,sCaseValue;` |
|       - |  330 | `	sxu32 n,nEntry;` |
|       - |  331 | `#ifdef UNTRUST` |
|       - |  332 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|       - |  333 | `		VM_EXIT_ABORT;` |
|       - |  334 | `	}` |
|       - |  335 | `#endif` |
|       - |  336 | `	/* Point to the case table  */` |
|     333 |  337 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|     333 |  338 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|       - |  339 | `	/* Select the appropriate case block to execute */` |
|     333 |  340 | `	PH7_MemObjInit(pVm,&sValue);` |
|     333 |  341 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|    1215 |  342 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|    1213 |  343 | `		pCase = &aCase[n];` |
|    1213 |  344 | `		PH7_MemObjLoad(pTos,&sValue);` |
|       - |  345 | `		/* Execute the case expression first */` |
|    1213 |  346 | `		VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue,FALSE);` |
|       - |  347 | `		/* Compare the two expression */` |
|    1213 |  348 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|    1213 |  349 | `		PH7_MemObjRelease(&sValue);` |
|    1213 |  350 | `		PH7_MemObjRelease(&sCaseValue);` |
|    1213 |  351 | `		if( rc == 0 ){` |
|       - |  352 | `			/* Value match,jump to this block */` |
|     331 |  353 | `			pc = pCase->nStart - 1;` |
|     331 |  354 | `			break;` |
|       - |  355 | `		}` |
|     446 |  356 | `	}` |
|     333 |  357 | `	VmPopOperand(&pTos,1);` |
|     333 |  358 | `	if( n >= nEntry ){` |
|       - |  359 | `		/* No approprite case to execute,jump to the default case */` |
|       3 |  360 | `		if( pSwitch->nDefault > 0 ){` |
|       3 |  361 | `			pc = pSwitch->nDefault - 1;` |
|       2 |  362 | `		}else{` |
|       - |  363 | `			/* No default case,jump out of this switch */` |
|     ! 0 |  364 | `			pc = pSwitch->nOut - 1;` |
|       - |  365 | `		}` |
|       1 |  366 | `	}` |
|     333 |  367 | `	VM_EXIT_BREAK;` |
|     ! 0 |  368 | `	VM_EXIT_BREAK;` |
|       5 |  369 | `}` |
|       - |  370 |  |
|       - |  371 | `/*` |
|       - |  372 | ` * OP_CATCH: body moved verbatim from the OP_CATCH arm of` |
|       - |  373 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  374 | ` */` |
|      70 |  375 | `PH7_PRIVATE VmOpRc VmExecOpCatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  376 | `{` |
|      75 |  377 | `	ph7_value *pTos = pState->pTos;` |
|      75 |  378 | `	ph7_value *pStack = pState->pStack;` |
|      75 |  379 | `	VmInstr *aInstr = pState->aInstr;` |
|      75 |  380 | `	sxi32 pc = pState->pc;` |
|       - |  381 | `	sxi32 rc;` |
|      35 |  382 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - |  383 | `	/* BYTECODE stage 2b: mutable state (pInflight) lives on the LIVE activation` |
|       - |  384 | `	 * of this try, not the compiled p3 (which VmThrowInline kept on aException` |
|       - |  385 | `	 * marked iInCatch). Compiled fields (sEntry) are shared either way. */` |
|      75 |  386 | `	ph7_exception *pExcC = (ph7_exception *)pInstr->p3;` |
|      75 |  387 | `	ph7_exception *pExc = VmExcLive(&(*pVm),pExcC);` |
|      75 |  388 | `	ph7_exception_block *pCatch = (ph7_exception_block *)SySetAt(&pExcC->sEntry,(sxu32)pInstr->iP1);` |
|      75 |  389 | `	ph7_class_instance *pBind = pExc ? pExc->pInflight : 0;` |
|      75 |  390 | `	VmFrame *pBody = VmSkipExceptionFrames(pVm->pFrame);` |
|      75 |  391 | `	pBody->iFlags &= ~VM_FRAME_THROW;` |
|      75 |  392 | `	if( pCatch && pBind && pCatch->sThis.nByte > 0 ){` |
|       - |  393 | `		/* sThis empty => PHP 8.0 non-capturing catch (catch (Type) {}): the` |
|       - |  394 | `		 * exception is caught but not bound to any variable. */` |
|      73 |  395 | `		ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      73 |  396 | `		if( pObj ){` |
|       - |  397 | `			/* Overwrite-then-release (mirrors PH7_MemObjStore): pin the new instance,` |
|       - |  398 | `			 * free the slot's prior contents, then rebind. */` |
|      73 |  399 | `			pBind->iRef++;` |
|      73 |  400 | `			PH7_MemObjRelease(pObj);` |
|      73 |  401 | `			pObj->x.pOther = pBind;` |
|      73 |  402 | `			MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      34 |  403 | `		}` |
|      34 |  404 | `	}` |
|      75 |  405 | `	if( pBind ){` |
|       - |  406 | `		/* Drop the hold VmThrowInline took across the redirect. */` |
|      75 |  407 | `		PH7_ClassInstanceUnref(pBind);` |
|      35 |  408 | `	}` |
|      75 |  409 | `	if( pExc ){` |
|      75 |  410 | `		pExc->pInflight = 0;` |
|      35 |  411 | `	}` |
|      75 |  412 | `	VM_EXIT_BREAK;` |
|     ! 0 |  413 | `	VM_EXIT_BREAK;` |
|       5 |  414 | `}` |
|       - |  415 |  |
|       - |  416 | `/*` |
|       - |  417 | ` * OP_LOAD_EXCEPTION: body moved verbatim from the OP_LOAD_EXCEPTION arm of` |
|       - |  418 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  419 | ` */` |
| 1447504 |  420 | `PH7_PRIVATE VmOpRc VmExecOpLoadException(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  421 | `{` |
| 1447509 |  422 | `	ph7_value *pTos = pState->pTos;` |
| 1447509 |  423 | `	ph7_value *pStack = pState->pStack;` |
| 1447509 |  424 | `	VmInstr *aInstr = pState->aInstr;` |
| 1447509 |  425 | `	sxi32 pc = pState->pc;` |
|       - |  426 | `	sxi32 rc;` |
|  723752 |  427 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - |  428 | `	/* BYTECODE stage 2b: push a fresh ACTIVATION of this lexical try (own` |
|       - |  429 | `	 * mutable state per entry — see VmExcActivate), never the shared` |
|       - |  430 | `	 * compiled object. */` |
| 1447509 |  431 | `	ph7_exception *pException = VmExcActivate(&(*pVm),(ph7_exception *)pInstr->p3);` |
|       - |  432 | `	VmFrame *pFrameLocal;` |
| 1447509 |  433 | `	if( pException == 0 ){` |
|     ! 0 |  434 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|     ! 0 |  435 | `		VM_EXIT_ABORT;` |
|       - |  436 | `	}` |
|       - |  437 | `	/* Create the exception frame BEFORE publishing the activation, so an OOM` |
|       - |  438 | `	 * abort cannot orphan a pushed entry with no frame behind it. */` |
| 1447509 |  439 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
| 1447509 |  440 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  441 | `		VmExcRelease(&(*pVm),pException);` |
|     ! 0 |  442 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|     ! 0 |  443 | `		VM_EXIT_ABORT;` |
|       - |  444 | `	}` |
| 1447509 |  445 | `	if( SXRET_OK != SySetPut(&pVm->aException,(const void *)&pException) ){` |
|     ! 0 |  446 | `		VmExcRelease(&(*pVm),pException);` |
|     ! 0 |  447 | `		VmLeaveFrame(&(*pVm));` |
|     ! 0 |  448 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|     ! 0 |  449 | `		VM_EXIT_ABORT;` |
|       - |  450 | `	}` |
|       - |  451 | `	/* Mark the special frame */` |
| 1447509 |  452 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
| 1447509 |  453 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|       - |  454 | `	/* Record the landing pad on the exception too, so an in-place catch can resume` |
|       - |  455 | `	 * the throwing site at THIS try (survives the exception frame's teardown), plus` |
|       - |  456 | `	 * the bytecode array it indexes — the resume only fires in the exec running that` |
|       - |  457 | `	 * array, so a mini-program (inline try in a catch/finally) and the body that` |
|       - |  458 | `	 * shares its frame don't mis-apply each other's landing pad. iLandingPc mirrors the` |
|       - |  459 | `	 * frame's iExceptionJump just set above — reuse it so the two can't drift. */` |
| 1447509 |  460 | `	pException->iLandingPc = pFrameLocal->iExceptionJump;` |
| 1447509 |  461 | `	pException->pOwnerInstr = (void *)aInstr;` |
|       - |  462 | `	/* Operand-stack base at try entry (0-based TOS index; -1 when empty). The post-try` |
|       - |  463 | `	 * landing pad is reached with the stack back at this depth; Generator::throw()` |
|       - |  464 | `	 * inject-at-yield drains to it before landing (a mid-expression yield leaves the` |
|       - |  465 | `	 * abandoned expression's operands above this base). Normal throws are already here. */` |
| 1447509 |  466 | `	pException->iStackDepth = (sxi32)(pTos - pStack);` |
|       - |  467 | `	/* '@' depth at try entry — see ph7_exception.iErrSuppress */` |
| 1447509 |  468 | `	pException->iErrSuppress = pVm->nErrSuppress;` |
|       - |  469 | `	/* Point to the frame that trigger the exception */` |
| 1447509 |  470 | `	pFrameLocal = pFrameLocal->pParent;` |
| 1447509 |  471 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
| 1447509 |  472 | `	pException->pFrame = pFrameLocal;` |
| 1447509 |  473 | `	VM_EXIT_BREAK;` |
|     ! 0 |  474 | `	VM_EXIT_BREAK;` |
|  723757 |  475 | `}` |
|       - |  476 |  |
