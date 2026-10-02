# src/ph7/vm_ops_misc.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 258/306 lines (84.31%)

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
|  195727 |   28 | `PH7_PRIVATE VmOpRc VmExecOpConsume(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   29 | `{` |
|  195732 |   30 | `	ph7_value *pTos = pState->pTos;` |
|  195732 |   31 | `	ph7_value *pStack = pState->pStack;` |
|  195732 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|  195732 |   33 | `	sxi32 pc = pState->pc;` |
|       - |   34 | `	sxi32 rc;` |
|   97783 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  195732 |   36 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|  195732 |   37 | `	ph7_value *pCur,*pOut = pTos;` |
|       - |   38 |  |
|  195732 |   39 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|  195732 |   40 | `	pCur = pOut;` |
|       - |   41 | `	/* Start the consume process  */` |
|  391457 |   42 | `	while( pOut <= pTos ){` |
|       - |   43 | `		/* Force a string cast (echo/print: user-visible array->string warning).` |
|       - |   44 | `` 		 * A not-stringable object throws HERE, mid-list: php compiles `echo a,b,c` `` |
|       - |   45 | `		 * to one ECHO per operand, so everything left of the object is already` |
|       - |   46 | `		 * out. Release what is left so the abandoned operands don't outlive the` |
|       - |   47 | `		 * op, then route. */` |
|  195748 |   48 | `		sxi32 rcSv = PH7_MemObjToStringUV(pOut);` |
|  195748 |   49 | `		if( rcSv != SXRET_OK ){` |
|      39 |   50 | `			while( pOut <= pTos ){` |
|      21 |   51 | `				PH7_MemObjRelease(pOut);` |
|      21 |   52 | `				pOut++;` |
|       3 |   53 | `			}` |
|      21 |   54 | `			pTos = &pCur[-1];` |
|      21 |   55 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|     ! 0 |   56 | `		}` |
|  195730 |   57 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|       - |   58 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|       - |   59 | `			/* Invoke the output consumer callback */` |
|  167865 |   60 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|  167865 |   61 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|  167865 |   62 | `			SyBlobRelease(&pOut->sBlob);` |
|  167865 |   63 | `			if( rc == SXERR_ABORT ){` |
|       - |   64 | `				/* Output consumer callback request an operation abort. */` |
|     ! 0 |   65 | `				VM_EXIT_ABORT;` |
|       - |   66 | `			}` |
|   83855 |   67 | `		}` |
|  195730 |   68 | `		pOut++;` |
|       5 |   69 | `	}` |
|  195714 |   70 | `	pTos = &pCur[-1];` |
|  195714 |   71 | `	VM_EXIT_BREAK;` |
|     ! 0 |   72 | `	VM_EXIT_BREAK;` |
|   97788 |   73 | `}` |
|       - |   74 |  |
|       - |   75 | `/*` |
|       - |   76 | ` * OP_MATCH: body moved verbatim from the OP_MATCH arm of` |
|       - |   77 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |   78 | ` */` |
|     236 |   79 | `PH7_PRIVATE VmOpRc VmExecOpMatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       4 |   80 | `{` |
|     240 |   81 | `	ph7_value *pTos = pState->pTos;` |
|     240 |   82 | `	ph7_value *pStack = pState->pStack;` |
|     240 |   83 | `	VmInstr *aInstr = pState->aInstr;` |
|     240 |   84 | `	sxi32 pc = pState->pc;` |
|       - |   85 | `	sxi32 rc;` |
|     118 |   86 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     240 |   87 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|     240 |   88 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|       - |   89 | `	ph7_value sSubject,sCond,sResult;` |
|       - |   90 | `	sxu32 i,j,nArm,nCond;` |
|     240 |   91 | `	sxi32 rcArm = SXRET_OK;` |
|     240 |   92 | `	const void *pResumeBefore = (const void *)pVm->pResumeFrame;` |
|     240 |   93 | `	const void *pInlineBefore = (const void *)pVm->pInlineInstr;` |
|     240 |   94 | `	int matched = 0;` |
|       - |   95 | `#ifdef UNTRUST` |
|       - |   96 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|       - |   97 | `		VM_EXIT_ABORT;` |
|       - |   98 | `	}` |
|       - |   99 | `#endif` |
|     240 |  100 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|     240 |  101 | `	nArm = SySetUsed(&pMatch->aArms);` |
|     240 |  102 | `	PH7_MemObjInit(pVm,&sSubject);` |
|     240 |  103 | `	PH7_MemObjInit(pVm,&sCond);` |
|     240 |  104 | `	PH7_MemObjInit(pVm,&sResult);` |
|     240 |  105 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|       - |  106 | `	/* Each condition and each arm BODY is its own bytecode container, run by` |
|       - |  107 | `	 * VmLocalExec. A throw inside one abandons the whole match expression in php,` |
|       - |  108 | `	 * so its status is routed below (VmLocalExecThrew): ignoring it let a caught` |
|       - |  109 | `	 * throw fall through to the NEXT condition — and then to the default arm,` |
|       - |  110 | `	 * which php never reaches — and handed the abandoned statement a value. */` |
|     760 |  111 | `	for( i = 0; i < nArm && !matched && rcArm == SXRET_OK; ++i ){` |
|     524 |  112 | `		pArm = &aArm[i];` |
|     524 |  113 | `		if( pArm->bDefault ){` |
|      18 |  114 | `			pDefault = pArm;` |
|      18 |  115 | `			continue;` |
|       - |  116 | `		}` |
|     508 |  117 | `		nCond = SySetUsed(&pArm->aConds);` |
|     838 |  118 | `		for( j = 0; j < nCond; ++j ){` |
|     546 |  119 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|     546 |  120 | `			if( pCondBc == 0 ){` |
|     ! 0 |  121 | `				continue;` |
|       - |  122 | `			}` |
|     546 |  123 | `			rcArm = VmLocalExec(pVm,pCondBc,&sCond,FALSE);` |
|     546 |  124 | `			if( rcArm == PH7_ABORT \|\| VmLocalExecThrew(pVm,rcArm,pResumeBefore,pInlineBefore) ){` |
|       2 |  125 | `				break;` |
|       - |  126 | `			}` |
|     544 |  127 | `			rcArm = SXRET_OK;` |
|     544 |  128 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|     544 |  129 | `			PH7_MemObjRelease(&sCond);` |
|     544 |  130 | `			if( rc == 0 ){` |
|     214 |  131 | `				rcArm = VmLocalExec(pVm,&pArm->aResult,&sResult,FALSE);` |
|     214 |  132 | `				matched = 1;` |
|     214 |  133 | `				break;` |
|       - |  134 | `			}` |
|     168 |  135 | `		}` |
|     256 |  136 | `	}` |
|     240 |  137 | `	if( !matched && pDefault && rcArm != PH7_ABORT && !VmLocalExecThrew(pVm,rcArm,pResumeBefore,pInlineBefore) ){` |
|      18 |  138 | `		rcArm = VmLocalExec(pVm,&pDefault->aResult,&sResult,FALSE);` |
|      18 |  139 | `		matched = 1;` |
|       8 |  140 | `	}` |
|     240 |  141 | `	if( rcArm == PH7_ABORT ){` |
|     ! 0 |  142 | `		PH7_MemObjRelease(&sCond);` |
|     ! 0 |  143 | `		PH7_MemObjRelease(&sSubject);` |
|     ! 0 |  144 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 |  145 | `		VM_EXIT_ABORT;` |
|       - |  146 | `	}` |
|     240 |  147 | `	if( VmLocalExecThrew(pVm,rcArm,pResumeBefore,pInlineBefore) ){` |
|      34 |  148 | `		PH7_MemObjRelease(&sCond);` |
|      34 |  149 | `		PH7_MemObjRelease(&sSubject);` |
|      34 |  150 | `		PH7_MemObjRelease(&sResult);` |
|      34 |  151 | `		VmPopOperand(&pTos,1); /* the subject this OP_MATCH would have replaced */` |
|      34 |  152 | `		PH7_THROW_ROUTE_MIDEXPR(rcArm)` |
|       - |  153 | `	}` |
|     208 |  154 | `	if( !matched ){` |
|      12 |  155 | `		const char *zType = "unknown";` |
|       - |  156 | `		char zMsg[128];` |
|       - |  157 | `		sxu32 nMsg;` |
|      12 |  158 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|     ! 0 |  159 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|     ! 0 |  160 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|      12 |  161 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|     ! 0 |  162 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|     ! 0 |  163 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|     ! 0 |  164 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|     ! 0 |  165 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|     ! 0 |  166 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|     ! 0 |  167 | `		default: break;` |
|       - |  168 | `		}` |
|       - |  169 | `		SyBlob sErrMsg;` |
|      16 |  170 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       4 |  171 | `			"Unhandled match case of type %s",zType);` |
|      12 |  172 | `		PH7_MemObjRelease(&sSubject);` |
|      12 |  173 | `		PH7_MemObjRelease(&sResult);` |
|       - |  174 | `		/* php raises a CATCHABLE \UnhandledMatchError here, so build a real` |
|       - |  175 | `		 * exception object and route it like any mid-expression throw (an` |
|       - |  176 | `		 * enclosing try in this body resumes at its landing pad; otherwise it` |
|       - |  177 | `		 * propagates and renders as uncaught, as before). Reporting it straight` |
|       - |  178 | `		 * to the uncaught renderer — what this site used to do — made the error` |
|       - |  179 | `		 * unconditionally fatal even inside try/catch. */` |
|      12 |  180 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      12 |  181 | `		SyBlobAppend(&sErrMsg,zMsg,nMsg);` |
|      12 |  182 | `		rc = VmThrowBuiltinError(&(*pVm),"UnhandledMatchError",` |
|       - |  183 | `			sizeof("UnhandledMatchError")-1,&sErrMsg);` |
|      16 |  184 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  185 | `	}` |
|     200 |  186 | `	PH7_MemObjRelease(&sSubject);` |
|       - |  187 | `	/* Replace subject on TOS with the arm result */` |
|     200 |  188 | `	PH7_MemObjStore(&sResult,pTos);` |
|     200 |  189 | `	PH7_MemObjRelease(&sResult);` |
|     200 |  190 | `	VM_EXIT_BREAK;` |
|     ! 0 |  191 | `	VM_EXIT_BREAK;` |
|     122 |  192 | `}` |
|       - |  193 |  |
|       - |  194 | `/*` |
|       - |  195 | ` * Build an \Error instance carrying zMsg (or NULL when the class is` |
|       - |  196 | ` * unavailable). Shared by OP_THROW's two "operand cannot be thrown" cases:` |
|       - |  197 | ` * php reports a non-object as "Can only throw objects" and a non-Throwable` |
|       - |  198 | ` * object as "Cannot throw objects that do not implement Throwable", both as` |
|       - |  199 | ` * ordinary catchable throws. The caller hands the instance to` |
|       - |  200 | ` * VmThrowException so the routing below sees exactly the status a normal` |
|       - |  201 | ` * throw produces. Error::__construct comes from the built-in library and` |
|       - |  202 | ` * cannot realistically fail, so its return is not checked.` |
|       - |  203 | ` */` |
|      18 |  204 | `static ph7_class_instance * VmNewThrowError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|       2 |  205 | `{` |
|       - |  206 | `	ph7_class *pErrorClass;` |
|       - |  207 | `	ph7_class_instance *pErrInst;` |
|       - |  208 | `	ph7_class_method *pCons;` |
|      20 |  209 | `	pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|      20 |  210 | `	if( pErrorClass == 0 ){` |
|     ! 0 |  211 | `		return 0;` |
|       - |  212 | `	}` |
|      20 |  213 | `	pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|      20 |  214 | `	if( pErrInst == 0 ){` |
|     ! 0 |  215 | `		return 0;` |
|       - |  216 | `	}` |
|      20 |  217 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|      20 |  218 | `	if( pCons ){` |
|       - |  219 | `		ph7_value sArg;` |
|       - |  220 | `		ph7_value *apArg[1];` |
|       - |  221 | `		SyString sMsgStr;` |
|      20 |  222 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|      20 |  223 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|      20 |  224 | `		apArg[0] = &sArg;` |
|      20 |  225 | `		PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|      20 |  226 | `		PH7_MemObjRelease(&sArg);` |
|       9 |  227 | `	}` |
|      20 |  228 | `	return pErrInst;` |
|      11 |  229 | `}` |
|       - |  230 | `/*` |
|       - |  231 | ` * OP_THROW: body moved verbatim from the OP_THROW arm of` |
|       - |  232 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  233 | ` */` |
| 1001061 |  234 | `PH7_PRIVATE VmOpRc VmExecOpThrow(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  235 | `{` |
| 1001066 |  236 | `	ph7_value *pTos = pState->pTos;` |
| 1001066 |  237 | `	ph7_value *pStack = pState->pStack;` |
| 1001066 |  238 | `	VmInstr *aInstr = pState->aInstr;` |
| 1001066 |  239 | `	sxi32 pc = pState->pc;` |
|       - |  240 | `	sxi32 rc;` |
|  500529 |  241 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 1001066 |  242 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
| 1001066 |  243 | `	sxu32 nJump = pInstr->iP2;` |
|       - |  244 | `#ifdef UNTRUST` |
|       - |  245 | `	if( pTos < pStack ){` |
|       - |  246 | `		VM_EXIT_ABORT;` |
|       - |  247 | `	}` |
|       - |  248 | `#endif` |
| 1001066 |  249 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       - |  250 | `	/* Tell the upper layer that an exception was thrown */` |
| 1001066 |  251 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
| 1001066 |  252 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
| 1001052 |  253 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|       - |  254 | `		ph7_class *pThrowable;` |
|       - |  255 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
| 1001052 |  256 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
| 1001054 |  257 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|       - |  258 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior. */` |
|       - |  259 | `			static const char zErrMsg[] =` |
|       - |  260 | `				"Cannot throw objects that do not implement Throwable";` |
|       6 |  261 | `			ph7_class_instance *pErrInst = VmNewThrowError(&(*pVm),zErrMsg,sizeof(zErrMsg)-1);` |
|       6 |  262 | `			if( pErrInst ){` |
|       6 |  263 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|       6 |  264 | `				PH7_ClassInstanceUnref(pErrInst);` |
|       6 |  265 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  266 | `					VM_EXIT_ABORT;` |
|       - |  267 | `				}` |
|       4 |  268 | `			}else{` |
|       - |  269 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|     ! 0 |  270 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|     ! 0 |  271 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  272 | `					VM_EXIT_ABORT;` |
|       - |  273 | `				}` |
|       - |  274 | `			}` |
|       4 |  275 | `		}else{` |
|       - |  276 | `			/* Throw the exception */` |
| 1001048 |  277 | `			rc = VmThrowException(&(*pVm),pThis);` |
| 1001048 |  278 | `			if( rc == SXERR_ABORT ){` |
|       - |  279 | `				/* Abort processing immediately */` |
|      51 |  280 | `				VM_EXIT_ABORT;` |
|       - |  281 | `			}` |
|       - |  282 | `		}` |
|  500504 |  283 | `	}else{` |
|       - |  284 | `		/* php raises a CATCHABLE Error for a non-object operand. This used to` |
|       - |  285 | `		 * report a bogus uncaught "Exception" and then fall into the jump below,` |
|       - |  286 | `		 * so no catch ran yet execution carried on past the try — and the branch` |
|       - |  287 | `		 * tested a STALE rc left by the previous instruction, which is the` |
|       - |  288 | `		 * latent bug the interpreter split surfaced. Build the same Error shape` |
|       - |  289 | `		 * the not-Throwable case uses and let the routing below land it. */` |
|       - |  290 | `		static const char zErrMsg[] = "Can only throw objects";` |
|      15 |  291 | `		ph7_class_instance *pErrInst = VmNewThrowError(&(*pVm),zErrMsg,sizeof(zErrMsg)-1);` |
|      15 |  292 | `		if( pErrInst ){` |
|      15 |  293 | `			rc = VmThrowException(&(*pVm),pErrInst);` |
|      15 |  294 | `			PH7_ClassInstanceUnref(pErrInst);` |
|       8 |  295 | `		}else{` |
|       - |  296 | `			/* Bootstrap failure — fall back to uncaught reporting */` |
|     ! 0 |  297 | `			rc = VmUncaughtException(&(*pVm),0);` |
|       - |  298 | `		}` |
|      15 |  299 | `		if( rc == SXERR_ABORT ){` |
|       - |  300 | `			/* Abort processing immediately */` |
|     ! 0 |  301 | `			VM_EXIT_ABORT;` |
|       - |  302 | `		}` |
|       - |  303 | `	}` |
|       - |  304 | `	/* Pop the top entry */` |
| 1001020 |  305 | `	VmPopOperand(&pTos,1);` |
|       - |  306 | `	/* ROOT C: throw caught by an inline try in THIS exec -> jump to its catch/finally` |
|       - |  307 | `	 * (draining any mid-expression operands back to the try's base). */` |
| 1001020 |  308 | `	PH7_INLINE_RESUME_BREAK()` |
|       - |  309 | `	/* pInlineInstr still set here means an inline try in an OUTER exec caught it (e.g. a` |
|       - |  310 | ``	 * throwing sub-generator delegated via `yield from`): propagate so the owning exec lands. */`` |
| 1000998 |  311 | `	if( rc == PH7_EXCEPTION \|\| pVm->pResumeFrame \|\| pVm->pInlineInstr ){` |
|       - |  312 | ``		/* The throw was handled by a `catch` that ran IN PLACE — either this try's own`` |
|       - |  313 | `		 * finally threw past itself superseding it (rc == PH7_EXCEPTION), or the catch` |
|       - |  314 | `		 * sits several frames above this one (pVm->pResumeFrame recorded). Resume at the` |
|       - |  315 | `		 * catching body's landing pad if that body is THIS exec (VmRecordedResume), else` |
|       - |  316 | `		 * unwind so the owning exec lands. Without this a throw caught at an enclosing` |
|       - |  317 | `		 * frame would blindly jump to nJump (this throw's lexically-nearest try) and run` |
|       - |  318 | `		 * dead code after it (ROOT B, face a) or continue a callee as if it never threw` |
|       - |  319 | `		 * (face c). */` |
|       - |  320 | `		sxi32 iResumePc;` |
|  900914 |  321 | `		if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){` |
|  400373 |  322 | `			PH7_RESUME_DRAIN()` |
|  300373 |  323 | `			pc = iResumePc;` |
|  300373 |  324 | `			VM_EXIT_BREAK;` |
|       - |  325 | `		}` |
|  600546 |  326 | `		VM_EXIT_EXCEPTION;` |
|       - |  327 | `	}` |
|       - |  328 | `	/* No in-place catch recorded: this throw's own enclosing try caught it (the` |
|       - |  329 | `	 * common case; its landing pad is exactly nJump). A throw-EXPRESSION` |
|       - |  330 | ``	 * (`$q = 1 + throw new E`) abandons its outer expression's pending operands`` |
|       - |  331 | `	 * above the try's base — drain to the catching activation's recorded depth` |
|       - |  332 | `	 * (matched by landing pad + bytecode array so an unrelated activation can` |
|       - |  333 | `	 * never be consulted) before jumping, or each caught throw leaks a slot.` |
|       - |  334 | `	 * Then jump to the try's OP_POP_EXCEPTION landing pad, which tears down the` |
|       - |  335 | ``	 * try frame, runs finally, and (when a catch/finally issued a `return`)`` |
|       - |  336 | `	 * materializes the body frame's pending return. Routing the return through` |
|       - |  337 | `	 * OP_POP_EXCEPTION keeps the frame stack balanced. */` |
|  100088 |  338 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|     ! 0 |  339 | `		ph7_exception *pTopExc = ((ph7_exception **)SySetBasePtr(&pVm->aException))[SySetUsed(&pVm->aException)-1];` |
|     ! 0 |  340 | `		if( pTopExc->iLandingPc == (sxu32)nJump && pTopExc->pOwnerInstr == (void *)aInstr ){` |
|     ! 0 |  341 | `			while( (sxi32)(pTos - pStack) > pTopExc->iStackDepth ){` |
|     ! 0 |  342 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 |  343 | `				pTos--;` |
|     ! 0 |  344 | `			}` |
|     ! 0 |  345 | `		}` |
|     ! 0 |  346 | `	}` |
|  100088 |  347 | `	pc = nJump - 1;` |
|  100088 |  348 | `	VM_EXIT_BREAK;` |
|     ! 0 |  349 | `	VM_EXIT_BREAK;` |
|  500534 |  350 | `}` |
|       - |  351 |  |
|       - |  352 | `/*` |
|       - |  353 | ` * OP_SWITCH: body moved verbatim from the OP_SWITCH arm of` |
|       - |  354 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  355 | ` */` |
|     250 |  356 | `PH7_PRIVATE VmOpRc VmExecOpSwitch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  357 | `{` |
|     255 |  358 | `	ph7_value *pTos = pState->pTos;` |
|     255 |  359 | `	ph7_value *pStack = pState->pStack;` |
|     255 |  360 | `	VmInstr *aInstr = pState->aInstr;` |
|     255 |  361 | `	sxi32 pc = pState->pc;` |
|       - |  362 | `	sxi32 rc;` |
|     125 |  363 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     255 |  364 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|       - |  365 | `	ph7_case_expr *aCase,*pCase;` |
|       - |  366 | `	ph7_value sValue,sCaseValue;` |
|       - |  367 | `	sxu32 n,nEntry;` |
|     255 |  368 | `	const void *pResumeBefore = (const void *)pVm->pResumeFrame;` |
|     255 |  369 | `	const void *pInlineBefore = (const void *)pVm->pInlineInstr;` |
|       - |  370 | `#ifdef UNTRUST` |
|       - |  371 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|       - |  372 | `		VM_EXIT_ABORT;` |
|       - |  373 | `	}` |
|       - |  374 | `#endif` |
|       - |  375 | `	/* Point to the case table  */` |
|     255 |  376 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|     255 |  377 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|       - |  378 | `	/* Select the appropriate case block to execute */` |
|     255 |  379 | `	PH7_MemObjInit(pVm,&sValue);` |
|     255 |  380 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|     665 |  381 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       - |  382 | `		sxi32 rcCase;` |
|     613 |  383 | `		pCase = &aCase[n];` |
|     613 |  384 | `		PH7_MemObjLoad(pTos,&sValue);` |
|       - |  385 | `		/* Execute the case expression first. It is its own bytecode container` |
|       - |  386 | ``		 * (VmLocalExec), so a throw inside it — `case boom():` — used to be caught`` |
|       - |  387 | `		 * in place and then IGNORED here: the scan carried on into the remaining` |
|       - |  388 | ``		 * cases and finally jumped to `default:`, running a branch php never`` |
|       - |  389 | `		 * reaches. Route the status the way any mid-expression throw is routed. */` |
|     613 |  390 | `		rcCase = VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue,FALSE);` |
|     613 |  391 | `		if( rcCase == PH7_ABORT \|\| VmLocalExecThrew(pVm,rcCase,pResumeBefore,pInlineBefore) ){` |
|       5 |  392 | `			PH7_MemObjRelease(&sValue);` |
|       5 |  393 | `			PH7_MemObjRelease(&sCaseValue);` |
|       5 |  394 | `			if( rcCase == PH7_ABORT ){` |
|     ! 0 |  395 | `				VM_EXIT_ABORT;` |
|       - |  396 | `			}` |
|       5 |  397 | `			VmPopOperand(&pTos,1); /* the switch subject */` |
|       5 |  398 | `			PH7_THROW_ROUTE_MIDEXPR(rcCase)` |
|       - |  399 | `		}` |
|       - |  400 | `		/* Compare the two expression */` |
|     609 |  401 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|     609 |  402 | `		PH7_MemObjRelease(&sValue);` |
|     609 |  403 | `		PH7_MemObjRelease(&sCaseValue);` |
|     609 |  404 | `		if( PH7_CmpRefusalPending(pVm) ){` |
|       - |  405 | `			/* A native compare handler refused this pair (two different kinds of` |
|       - |  406 | `			 * DateTimeZone). php raises it out of the comparison, so the switch` |
|       - |  407 | ``			 * never reaches a branch -- not even `default:`. */`` |
|       - |  408 | `			sxi32 rcRef;` |
|       5 |  409 | `			VmPopOperand(&pTos,1); /* the switch subject */` |
|       5 |  410 | `			rcRef = PH7_CmpRefusalRaise(pVm);` |
|       5 |  411 | `			if( rcRef == SXERR_ABORT ){` |
|     ! 0 |  412 | `				VM_EXIT_ABORT;` |
|       - |  413 | `			}` |
|       5 |  414 | `			PH7_THROW_ROUTE_MIDEXPR(rcRef)` |
|       - |  415 | `		}` |
|     605 |  416 | `		if( rc == 0 ){` |
|       - |  417 | `			/* Value match,jump to this block */` |
|     195 |  418 | `			pc = pCase->nStart - 1;` |
|     195 |  419 | `			break;` |
|       - |  420 | `		}` |
|     210 |  421 | `	}` |
|     247 |  422 | `	VmPopOperand(&pTos,1);` |
|     247 |  423 | `	if( n >= nEntry ){` |
|       - |  424 | `		/* No approprite case to execute,jump to the default case */` |
|      55 |  425 | `		if( pSwitch->nDefault > 0 ){` |
|      53 |  426 | `			pc = pSwitch->nDefault - 1;` |
|      28 |  427 | `		}else{` |
|       - |  428 | `			/* No default case,jump out of this switch */` |
|       3 |  429 | `			pc = pSwitch->nOut - 1;` |
|       - |  430 | `		}` |
|      26 |  431 | `	}` |
|     247 |  432 | `	VM_EXIT_BREAK;` |
|     ! 0 |  433 | `	VM_EXIT_BREAK;` |
|     130 |  434 | `}` |
|       - |  435 |  |
|       - |  436 | `/*` |
|       - |  437 | ` * OP_CATCH: body moved verbatim from the OP_CATCH arm of` |
|       - |  438 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  439 | ` */` |
|      84 |  440 | `PH7_PRIVATE VmOpRc VmExecOpCatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  441 | `{` |
|      89 |  442 | `	ph7_value *pTos = pState->pTos;` |
|      89 |  443 | `	ph7_value *pStack = pState->pStack;` |
|      89 |  444 | `	VmInstr *aInstr = pState->aInstr;` |
|      89 |  445 | `	sxi32 pc = pState->pc;` |
|       - |  446 | `	sxi32 rc;` |
|      42 |  447 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - |  448 | `	/* BYTECODE stage 2b: mutable state (pInflight) lives on the LIVE activation` |
|       - |  449 | `	 * of this try, not the compiled p3 (which VmThrowInline kept on aException` |
|       - |  450 | `	 * marked iInCatch). Compiled fields (sEntry) are shared either way. */` |
|      89 |  451 | `	ph7_exception *pExcC = (ph7_exception *)pInstr->p3;` |
|      89 |  452 | `	ph7_exception *pExc = VmExcLive(&(*pVm),pExcC);` |
|      89 |  453 | `	ph7_exception_block *pCatch = (ph7_exception_block *)SySetAt(&pExcC->sEntry,(sxu32)pInstr->iP1);` |
|      89 |  454 | `	ph7_class_instance *pBind = pExc ? pExc->pInflight : 0;` |
|      89 |  455 | `	VmFrame *pBody = VmSkipExceptionFrames(pVm->pFrame);` |
|      89 |  456 | `	pBody->iFlags &= ~VM_FRAME_THROW;` |
|      89 |  457 | `	if( pCatch && pBind && pCatch->sThis.nByte > 0 ){` |
|       - |  458 | `		/* sThis empty => PHP 8.0 non-capturing catch (catch (Type) {}): the` |
|       - |  459 | `		 * exception is caught but not bound to any variable. */` |
|      87 |  460 | `		ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      87 |  461 | `		if( pObj ){` |
|       - |  462 | `			/* Overwrite-then-release (mirrors PH7_MemObjStore): pin the new instance,` |
|       - |  463 | `			 * free the slot's prior contents, then rebind. */` |
|      87 |  464 | `			pBind->iRef++;` |
|      87 |  465 | `			PH7_MemObjRelease(pObj);` |
|      87 |  466 | `			pObj->x.pOther = pBind;` |
|      87 |  467 | `			MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      41 |  468 | `		}` |
|      41 |  469 | `	}` |
|      89 |  470 | `	if( pBind ){` |
|       - |  471 | `		/* Drop the hold VmThrowInline took across the redirect. */` |
|      89 |  472 | `		PH7_ClassInstanceUnref(pBind);` |
|      42 |  473 | `	}` |
|      89 |  474 | `	if( pExc ){` |
|      89 |  475 | `		pExc->pInflight = 0;` |
|      42 |  476 | `	}` |
|      89 |  477 | `	VM_EXIT_BREAK;` |
|     ! 0 |  478 | `	VM_EXIT_BREAK;` |
|       5 |  479 | `}` |
|       - |  480 |  |
|       - |  481 | `/*` |
|       - |  482 | ` * OP_LOAD_EXCEPTION: body moved verbatim from the OP_LOAD_EXCEPTION arm of` |
|       - |  483 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  484 | ` */` |
| 1482706 |  485 | `PH7_PRIVATE VmOpRc VmExecOpLoadException(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  486 | `{` |
| 1482711 |  487 | `	ph7_value *pTos = pState->pTos;` |
| 1482711 |  488 | `	ph7_value *pStack = pState->pStack;` |
| 1482711 |  489 | `	VmInstr *aInstr = pState->aInstr;` |
| 1482711 |  490 | `	sxi32 pc = pState->pc;` |
|       - |  491 | `	sxi32 rc;` |
|  741244 |  492 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - |  493 | `	/* BYTECODE stage 2b: push a fresh ACTIVATION of this lexical try (own` |
|       - |  494 | `	 * mutable state per entry — see VmExcActivate), never the shared` |
|       - |  495 | `	 * compiled object. */` |
| 1482711 |  496 | `	ph7_exception *pException = VmExcActivate(&(*pVm),(ph7_exception *)pInstr->p3);` |
|       - |  497 | `	VmFrame *pFrameLocal;` |
| 1482711 |  498 | `	if( pException == 0 ){` |
|     ! 0 |  499 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|     ! 0 |  500 | `		VM_EXIT_ABORT;` |
|       - |  501 | `	}` |
|       - |  502 | `	/* Create the exception frame BEFORE publishing the activation, so an OOM` |
|       - |  503 | `	 * abort cannot orphan a pushed entry with no frame behind it. */` |
| 1482711 |  504 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
| 1482711 |  505 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  506 | `		VmExcRelease(&(*pVm),pException);` |
|     ! 0 |  507 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|     ! 0 |  508 | `		VM_EXIT_ABORT;` |
|       - |  509 | `	}` |
| 1482711 |  510 | `	if( SXRET_OK != SySetPut(&pVm->aException,(const void *)&pException) ){` |
|     ! 0 |  511 | `		VmExcRelease(&(*pVm),pException);` |
|     ! 0 |  512 | `		VmLeaveFrame(&(*pVm));` |
|     ! 0 |  513 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|     ! 0 |  514 | `		VM_EXIT_ABORT;` |
|       - |  515 | `	}` |
|       - |  516 | `	/* Mark the special frame */` |
| 1482711 |  517 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
| 1482711 |  518 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|       - |  519 | `	/* Record the landing pad on the exception too, so an in-place catch can resume` |
|       - |  520 | `	 * the throwing site at THIS try (survives the exception frame's teardown), plus` |
|       - |  521 | `	 * the bytecode array it indexes — the resume only fires in the exec running that` |
|       - |  522 | `	 * array, so a mini-program (inline try in a catch/finally) and the body that` |
|       - |  523 | `	 * shares its frame don't mis-apply each other's landing pad. iLandingPc mirrors the` |
|       - |  524 | `	 * frame's iExceptionJump just set above — reuse it so the two can't drift. */` |
| 1482711 |  525 | `	pException->iLandingPc = pFrameLocal->iExceptionJump;` |
| 1482711 |  526 | `	pException->pOwnerInstr = (void *)aInstr;` |
|       - |  527 | `	/* Operand-stack base at try entry (0-based TOS index; -1 when empty). The post-try` |
|       - |  528 | `	 * landing pad is reached with the stack back at this depth; Generator::throw()` |
|       - |  529 | `	 * inject-at-yield drains to it before landing (a mid-expression yield leaves the` |
|       - |  530 | `	 * abandoned expression's operands above this base). Normal throws are already here. */` |
| 1482711 |  531 | `	pException->iStackDepth = (sxi32)(pTos - pStack);` |
|       - |  532 | `	/* '@' depth at try entry — see ph7_exception.iErrSuppress */` |
| 1482711 |  533 | `	pException->iErrSuppress = pVm->nErrSuppress;` |
|       - |  534 | `	/* Late-static-binding depth at try entry — see ph7_exception.nSelfDepth. */` |
| 1482711 |  535 | `	pException->nSelfDepth = SySetUsed(&pVm->aSelf);` |
|       - |  536 | `	/* Point to the frame that trigger the exception */` |
| 1482711 |  537 | `	pFrameLocal = pFrameLocal->pParent;` |
| 1482711 |  538 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
| 1482711 |  539 | `	pException->pFrame = pFrameLocal;` |
| 1482711 |  540 | `	VM_EXIT_BREAK;` |
|     ! 0 |  541 | `	VM_EXIT_BREAK;` |
|  741249 |  542 | `}` |
|       - |  543 |  |
