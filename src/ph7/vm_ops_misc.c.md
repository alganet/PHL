# src/ph7/vm_ops_misc.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 251/299 lines (83.95%)

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
|   96014 |   28 | `PH7_PRIVATE VmOpRc VmExecOpConsume(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   29 | `{` |
|   96019 |   30 | `	ph7_value *pTos = pState->pTos;` |
|   96019 |   31 | `	ph7_value *pStack = pState->pStack;` |
|   96019 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|   96019 |   33 | `	sxi32 pc = pState->pc;` |
|       - |   34 | `	sxi32 rc;` |
|   48007 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|   96019 |   36 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|   96019 |   37 | `	ph7_value *pCur,*pOut = pTos;` |
|       - |   38 |  |
|   96019 |   39 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|   96019 |   40 | `	pCur = pOut;` |
|       - |   41 | `	/* Start the consume process  */` |
|  192015 |   42 | `	while( pOut <= pTos ){` |
|       - |   43 | `		/* Force a string cast (echo/print: user-visible array->string warning, §2).` |
|       - |   44 | `` 		 * A not-stringable object throws HERE, mid-list: php compiles `echo a,b,c` `` |
|       - |   45 | `		 * to one ECHO per operand, so everything left of the object is already` |
|       - |   46 | `		 * out. Release what is left so the abandoned operands don't outlive the` |
|       - |   47 | `		 * op, then route. */` |
|   96019 |   48 | `		sxi32 rcSv = PH7_MemObjToStringUV(pOut);` |
|   96019 |   49 | `		if( rcSv != SXRET_OK ){` |
|      39 |   50 | `			while( pOut <= pTos ){` |
|      21 |   51 | `				PH7_MemObjRelease(pOut);` |
|      21 |   52 | `				pOut++;` |
|       3 |   53 | `			}` |
|      21 |   54 | `			pTos = &pCur[-1];` |
|      21 |   55 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|     ! 0 |   56 | `		}` |
|   96001 |   57 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|       - |   58 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|       - |   59 | `			/* Invoke the output consumer callback */` |
|   81843 |   60 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|   81843 |   61 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|   81843 |   62 | `			SyBlobRelease(&pOut->sBlob);` |
|   81843 |   63 | `			if( rc == SXERR_ABORT ){` |
|       - |   64 | `				/* Output consumer callback request an operation abort. */` |
|     ! 0 |   65 | `				VM_EXIT_ABORT;` |
|       - |   66 | `			}` |
|   40919 |   67 | `		}` |
|   96001 |   68 | `		pOut++;` |
|       5 |   69 | `	}` |
|   96001 |   70 | `	pTos = &pCur[-1];` |
|   96001 |   71 | `	VM_EXIT_BREAK;` |
|     ! 0 |   72 | `	VM_EXIT_BREAK;` |
|   48012 |   73 | `}` |
|       - |   74 |  |
|       - |   75 | `/*` |
|       - |   76 | ` * OP_MATCH: body moved verbatim from the OP_MATCH arm of` |
|       - |   77 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |   78 | ` */` |
|     154 |   79 | `PH7_PRIVATE VmOpRc VmExecOpMatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       4 |   80 | `{` |
|     158 |   81 | `	ph7_value *pTos = pState->pTos;` |
|     158 |   82 | `	ph7_value *pStack = pState->pStack;` |
|     158 |   83 | `	VmInstr *aInstr = pState->aInstr;` |
|     158 |   84 | `	sxi32 pc = pState->pc;` |
|       - |   85 | `	sxi32 rc;` |
|      77 |   86 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     158 |   87 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|     158 |   88 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|       - |   89 | `	ph7_value sSubject,sCond,sResult;` |
|       - |   90 | `	sxu32 i,j,nArm,nCond;` |
|     158 |   91 | `	sxi32 rcArm = SXRET_OK;` |
|     158 |   92 | `	const void *pResumeBefore = (const void *)pVm->pResumeFrame;` |
|     158 |   93 | `	const void *pInlineBefore = (const void *)pVm->pInlineInstr;` |
|     158 |   94 | `	int matched = 0;` |
|       - |   95 | `#ifdef UNTRUST` |
|       - |   96 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|       - |   97 | `		VM_EXIT_ABORT;` |
|       - |   98 | `	}` |
|       - |   99 | `#endif` |
|     158 |  100 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|     158 |  101 | `	nArm = SySetUsed(&pMatch->aArms);` |
|     158 |  102 | `	PH7_MemObjInit(pVm,&sSubject);` |
|     158 |  103 | `	PH7_MemObjInit(pVm,&sCond);` |
|     158 |  104 | `	PH7_MemObjInit(pVm,&sResult);` |
|     158 |  105 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|       - |  106 | `	/* Each condition and each arm BODY is its own bytecode container, run by` |
|       - |  107 | `	 * VmLocalExec. A throw inside one abandons the whole match expression in php,` |
|       - |  108 | `	 * so its status is routed below (VmLocalExecThrew): ignoring it let a caught` |
|       - |  109 | `	 * throw fall through to the NEXT condition — and then to the default arm,` |
|       - |  110 | `	 * which php never reaches — and handed the abandoned statement a value. */` |
|     454 |  111 | `	for( i = 0; i < nArm && !matched && rcArm == SXRET_OK; ++i ){` |
|     300 |  112 | `		pArm = &aArm[i];` |
|     300 |  113 | `		if( pArm->bDefault ){` |
|      13 |  114 | `			pDefault = pArm;` |
|      13 |  115 | `			continue;` |
|       - |  116 | `		}` |
|     288 |  117 | `		nCond = SySetUsed(&pArm->aConds);` |
|     468 |  118 | `		for( j = 0; j < nCond; ++j ){` |
|     320 |  119 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|     320 |  120 | `			if( pCondBc == 0 ){` |
|     ! 0 |  121 | `				continue;` |
|       - |  122 | `			}` |
|     320 |  123 | `			rcArm = VmLocalExec(pVm,pCondBc,&sCond,FALSE);` |
|     320 |  124 | `			if( rcArm == PH7_ABORT \|\| VmLocalExecThrew(pVm,rcArm,pResumeBefore,pInlineBefore) ){` |
|       2 |  125 | `				break;` |
|       - |  126 | `			}` |
|     318 |  127 | `			rcArm = SXRET_OK;` |
|     318 |  128 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|     318 |  129 | `			PH7_MemObjRelease(&sCond);` |
|     318 |  130 | `			if( rc == 0 ){` |
|     137 |  131 | `				rcArm = VmLocalExec(pVm,&pArm->aResult,&sResult,FALSE);` |
|     137 |  132 | `				matched = 1;` |
|     137 |  133 | `				break;` |
|       - |  134 | `			}` |
|      93 |  135 | `		}` |
|     146 |  136 | `	}` |
|     158 |  137 | `	if( !matched && pDefault && rcArm != PH7_ABORT && !VmLocalExecThrew(pVm,rcArm,pResumeBefore,pInlineBefore) ){` |
|      13 |  138 | `		rcArm = VmLocalExec(pVm,&pDefault->aResult,&sResult,FALSE);` |
|      13 |  139 | `		matched = 1;` |
|       6 |  140 | `	}` |
|     158 |  141 | `	if( rcArm == PH7_ABORT ){` |
|     ! 0 |  142 | `		PH7_MemObjRelease(&sCond);` |
|     ! 0 |  143 | `		PH7_MemObjRelease(&sSubject);` |
|     ! 0 |  144 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 |  145 | `		VM_EXIT_ABORT;` |
|       - |  146 | `	}` |
|     158 |  147 | `	if( VmLocalExecThrew(pVm,rcArm,pResumeBefore,pInlineBefore) ){` |
|      20 |  148 | `		PH7_MemObjRelease(&sCond);` |
|      20 |  149 | `		PH7_MemObjRelease(&sSubject);` |
|      20 |  150 | `		PH7_MemObjRelease(&sResult);` |
|      20 |  151 | `		VmPopOperand(&pTos,1); /* the subject this OP_MATCH would have replaced */` |
|      22 |  152 | `		PH7_THROW_ROUTE_MIDEXPR(rcArm)` |
|       - |  153 | `	}` |
|     140 |  154 | `	if( !matched ){` |
|       9 |  155 | `		const char *zType = "unknown";` |
|       - |  156 | `		char zMsg[128];` |
|       - |  157 | `		sxu32 nMsg;` |
|       9 |  158 | `		switch(sSubject.iFlags & MEMOBJ_ALL){` |
|     ! 0 |  159 | `		case MEMOBJ_NULL:   zType = "null";   break;` |
|     ! 0 |  160 | `		case MEMOBJ_BOOL:   zType = "bool";   break;` |
|       9 |  161 | `		case MEMOBJ_INT:    zType = "int";    break;` |
|     ! 0 |  162 | `		case MEMOBJ_REAL:   zType = "float";  break;` |
|     ! 0 |  163 | `		case MEMOBJ_STRING: zType = "string"; break;` |
|     ! 0 |  164 | `		case MEMOBJ_HASHMAP:zType = "array";  break;` |
|     ! 0 |  165 | `		case MEMOBJ_OBJ:    zType = "object"; break;` |
|     ! 0 |  166 | `		case MEMOBJ_RES:    zType = "resource"; break;` |
|     ! 0 |  167 | `		default: break;` |
|       - |  168 | `		}` |
|       - |  169 | `		SyBlob sErrMsg;` |
|      12 |  170 | `		nMsg = SyBufferFormat(zMsg,sizeof(zMsg),` |
|       3 |  171 | `			"Unhandled match case of type %s",zType);` |
|       9 |  172 | `		PH7_MemObjRelease(&sSubject);` |
|       9 |  173 | `		PH7_MemObjRelease(&sResult);` |
|       - |  174 | `		/* php raises a CATCHABLE \UnhandledMatchError here, so build a real` |
|       - |  175 | `		 * exception object and route it like any mid-expression throw (an` |
|       - |  176 | `		 * enclosing try in this body resumes at its landing pad; otherwise it` |
|       - |  177 | `		 * propagates and renders as uncaught, as before). Reporting it straight` |
|       - |  178 | `		 * to the uncaught renderer — what this site used to do — made the error` |
|       - |  179 | `		 * unconditionally fatal even inside try/catch. */` |
|       9 |  180 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       9 |  181 | `		SyBlobAppend(&sErrMsg,zMsg,nMsg);` |
|       9 |  182 | `		rc = VmThrowBuiltinError(&(*pVm),"UnhandledMatchError",` |
|       - |  183 | `			sizeof("UnhandledMatchError")-1,&sErrMsg);` |
|      13 |  184 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  185 | `	}` |
|     133 |  186 | `	PH7_MemObjRelease(&sSubject);` |
|       - |  187 | `	/* Replace subject on TOS with the arm result */` |
|     133 |  188 | `	PH7_MemObjStore(&sResult,pTos);` |
|     133 |  189 | `	PH7_MemObjRelease(&sResult);` |
|     133 |  190 | `	VM_EXIT_BREAK;` |
|     ! 0 |  191 | `	VM_EXIT_BREAK;` |
|      81 |  192 | `}` |
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
| 1000788 |  234 | `PH7_PRIVATE VmOpRc VmExecOpThrow(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  235 | `{` |
| 1000793 |  236 | `	ph7_value *pTos = pState->pTos;` |
| 1000793 |  237 | `	ph7_value *pStack = pState->pStack;` |
| 1000793 |  238 | `	VmInstr *aInstr = pState->aInstr;` |
| 1000793 |  239 | `	sxi32 pc = pState->pc;` |
|       - |  240 | `	sxi32 rc;` |
|  500394 |  241 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 1000793 |  242 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
| 1000793 |  243 | `	sxu32 nJump = pInstr->iP2;` |
|       - |  244 | `#ifdef UNTRUST` |
|       - |  245 | `	if( pTos < pStack ){` |
|       - |  246 | `		VM_EXIT_ABORT;` |
|       - |  247 | `	}` |
|       - |  248 | `#endif` |
| 1000793 |  249 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       - |  250 | `	/* Tell the upper layer that an exception was thrown */` |
| 1000793 |  251 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
| 1000793 |  252 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
| 1000779 |  253 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|       - |  254 | `		ph7_class *pThrowable;` |
|       - |  255 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
| 1000779 |  256 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
| 1000781 |  257 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
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
| 1000775 |  277 | `			rc = VmThrowException(&(*pVm),pThis);` |
| 1000775 |  278 | `			if( rc == SXERR_ABORT ){` |
|       - |  279 | `				/* Abort processing immediately */` |
|      27 |  280 | `				VM_EXIT_ABORT;` |
|       - |  281 | `			}` |
|       - |  282 | `		}` |
|  500381 |  283 | `	}else{` |
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
| 1000771 |  305 | `	VmPopOperand(&pTos,1);` |
|       - |  306 | `	/* ROOT C: throw caught by an inline try in THIS exec -> jump to its catch/finally` |
|       - |  307 | `	 * (draining any mid-expression operands back to the try's base). */` |
| 1000771 |  308 | `	PH7_INLINE_RESUME_BREAK()` |
|       - |  309 | `	/* pInlineInstr still set here means an inline try in an OUTER exec caught it (e.g. a` |
|       - |  310 | ``	 * throwing sub-generator delegated via `yield from`): propagate so the owning exec lands. */`` |
| 1000751 |  311 | `	if( rc == PH7_EXCEPTION \|\| pVm->pResumeFrame \|\| pVm->pInlineInstr ){` |
|       - |  312 | ``		/* The throw was handled by a `catch` that ran IN PLACE — either this try's own`` |
|       - |  313 | `		 * finally threw past itself superseding it (rc == PH7_EXCEPTION), or the catch` |
|       - |  314 | `		 * sits several frames above this one (pVm->pResumeFrame recorded). Resume at the` |
|       - |  315 | `		 * catching body's landing pad if that body is THIS exec (VmRecordedResume), else` |
|       - |  316 | `		 * unwind so the owning exec lands. Without this a throw caught at an enclosing` |
|       - |  317 | `		 * frame would blindly jump to nJump (this throw's lexically-nearest try) and run` |
|       - |  318 | `		 * dead code after it (ROOT B, face a) or continue a callee as if it never threw` |
|       - |  319 | `		 * (face c). */` |
|       - |  320 | `		sxi32 iResumePc;` |
|  900675 |  321 | `		if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){` |
|  400353 |  322 | `			PH7_RESUME_DRAIN()` |
|  300351 |  323 | `			pc = iResumePc;` |
|  300351 |  324 | `			VM_EXIT_BREAK;` |
|       - |  325 | `		}` |
|  600329 |  326 | `		VM_EXIT_EXCEPTION;` |
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
|  100081 |  338 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|     ! 0 |  339 | `		ph7_exception *pTopExc = ((ph7_exception **)SySetBasePtr(&pVm->aException))[SySetUsed(&pVm->aException)-1];` |
|     ! 0 |  340 | `		if( pTopExc->iLandingPc == (sxu32)nJump && pTopExc->pOwnerInstr == (void *)aInstr ){` |
|     ! 0 |  341 | `			while( (sxi32)(pTos - pStack) > pTopExc->iStackDepth ){` |
|     ! 0 |  342 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 |  343 | `				pTos--;` |
|     ! 0 |  344 | `			}` |
|     ! 0 |  345 | `		}` |
|     ! 0 |  346 | `	}` |
|  100081 |  347 | `	pc = nJump - 1;` |
|  100081 |  348 | `	VM_EXIT_BREAK;` |
|     ! 0 |  349 | `	VM_EXIT_BREAK;` |
|  500399 |  350 | `}` |
|       - |  351 |  |
|       - |  352 | `/*` |
|       - |  353 | ` * OP_SWITCH: body moved verbatim from the OP_SWITCH arm of` |
|       - |  354 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  355 | ` */` |
|      72 |  356 | `PH7_PRIVATE VmOpRc VmExecOpSwitch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  357 | `{` |
|      77 |  358 | `	ph7_value *pTos = pState->pTos;` |
|      77 |  359 | `	ph7_value *pStack = pState->pStack;` |
|      77 |  360 | `	VmInstr *aInstr = pState->aInstr;` |
|      77 |  361 | `	sxi32 pc = pState->pc;` |
|       - |  362 | `	sxi32 rc;` |
|      36 |  363 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|      77 |  364 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|       - |  365 | `	ph7_case_expr *aCase,*pCase;` |
|       - |  366 | `	ph7_value sValue,sCaseValue;` |
|       - |  367 | `	sxu32 n,nEntry;` |
|      77 |  368 | `	const void *pResumeBefore = (const void *)pVm->pResumeFrame;` |
|      77 |  369 | `	const void *pInlineBefore = (const void *)pVm->pInlineInstr;` |
|       - |  370 | `#ifdef UNTRUST` |
|       - |  371 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|       - |  372 | `		VM_EXIT_ABORT;` |
|       - |  373 | `	}` |
|       - |  374 | `#endif` |
|       - |  375 | `	/* Point to the case table  */` |
|      77 |  376 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|      77 |  377 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|       - |  378 | `	/* Select the appropriate case block to execute */` |
|      77 |  379 | `	PH7_MemObjInit(pVm,&sValue);` |
|      77 |  380 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|     161 |  381 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       - |  382 | `		sxi32 rcCase;` |
|     153 |  383 | `		pCase = &aCase[n];` |
|     153 |  384 | `		PH7_MemObjLoad(pTos,&sValue);` |
|       - |  385 | `		/* Execute the case expression first. It is its own bytecode container` |
|       - |  386 | ``		 * (VmLocalExec), so a throw inside it — `case boom():` — used to be caught`` |
|       - |  387 | `		 * in place and then IGNORED here: the scan carried on into the remaining` |
|       - |  388 | ``		 * cases and finally jumped to `default:`, running a branch php never`` |
|       - |  389 | `		 * reaches. Route the status the way any mid-expression throw is routed. */` |
|     153 |  390 | `		rcCase = VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue,FALSE);` |
|     153 |  391 | `		if( rcCase == PH7_ABORT \|\| VmLocalExecThrew(pVm,rcCase,pResumeBefore,pInlineBefore) ){` |
|       5 |  392 | `			PH7_MemObjRelease(&sValue);` |
|       5 |  393 | `			PH7_MemObjRelease(&sCaseValue);` |
|       5 |  394 | `			if( rcCase == PH7_ABORT ){` |
|     ! 0 |  395 | `				VM_EXIT_ABORT;` |
|       - |  396 | `			}` |
|       5 |  397 | `			VmPopOperand(&pTos,1); /* the switch subject */` |
|       5 |  398 | `			PH7_THROW_ROUTE_MIDEXPR(rcCase)` |
|       - |  399 | `		}` |
|       - |  400 | `		/* Compare the two expression */` |
|     149 |  401 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|     149 |  402 | `		PH7_MemObjRelease(&sValue);` |
|     149 |  403 | `		PH7_MemObjRelease(&sCaseValue);` |
|     149 |  404 | `		if( rc == 0 ){` |
|       - |  405 | `			/* Value match,jump to this block */` |
|      65 |  406 | `			pc = pCase->nStart - 1;` |
|      65 |  407 | `			break;` |
|       - |  408 | `		}` |
|      47 |  409 | `	}` |
|      73 |  410 | `	VmPopOperand(&pTos,1);` |
|      73 |  411 | `	if( n >= nEntry ){` |
|       - |  412 | `		/* No approprite case to execute,jump to the default case */` |
|      10 |  413 | `		if( pSwitch->nDefault > 0 ){` |
|      10 |  414 | `			pc = pSwitch->nDefault - 1;` |
|       6 |  415 | `		}else{` |
|       - |  416 | `			/* No default case,jump out of this switch */` |
|     ! 0 |  417 | `			pc = pSwitch->nOut - 1;` |
|       - |  418 | `		}` |
|       4 |  419 | `	}` |
|      73 |  420 | `	VM_EXIT_BREAK;` |
|     ! 0 |  421 | `	VM_EXIT_BREAK;` |
|      41 |  422 | `}` |
|       - |  423 |  |
|       - |  424 | `/*` |
|       - |  425 | ` * OP_CATCH: body moved verbatim from the OP_CATCH arm of` |
|       - |  426 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  427 | ` */` |
|      76 |  428 | `PH7_PRIVATE VmOpRc VmExecOpCatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  429 | `{` |
|      81 |  430 | `	ph7_value *pTos = pState->pTos;` |
|      81 |  431 | `	ph7_value *pStack = pState->pStack;` |
|      81 |  432 | `	VmInstr *aInstr = pState->aInstr;` |
|      81 |  433 | `	sxi32 pc = pState->pc;` |
|       - |  434 | `	sxi32 rc;` |
|      38 |  435 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - |  436 | `	/* BYTECODE stage 2b: mutable state (pInflight) lives on the LIVE activation` |
|       - |  437 | `	 * of this try, not the compiled p3 (which VmThrowInline kept on aException` |
|       - |  438 | `	 * marked iInCatch). Compiled fields (sEntry) are shared either way. */` |
|      81 |  439 | `	ph7_exception *pExcC = (ph7_exception *)pInstr->p3;` |
|      81 |  440 | `	ph7_exception *pExc = VmExcLive(&(*pVm),pExcC);` |
|      81 |  441 | `	ph7_exception_block *pCatch = (ph7_exception_block *)SySetAt(&pExcC->sEntry,(sxu32)pInstr->iP1);` |
|      81 |  442 | `	ph7_class_instance *pBind = pExc ? pExc->pInflight : 0;` |
|      81 |  443 | `	VmFrame *pBody = VmSkipExceptionFrames(pVm->pFrame);` |
|      81 |  444 | `	pBody->iFlags &= ~VM_FRAME_THROW;` |
|      81 |  445 | `	if( pCatch && pBind && pCatch->sThis.nByte > 0 ){` |
|       - |  446 | `		/* sThis empty => PHP 8.0 non-capturing catch (catch (Type) {}): the` |
|       - |  447 | `		 * exception is caught but not bound to any variable. */` |
|      79 |  448 | `		ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      79 |  449 | `		if( pObj ){` |
|       - |  450 | `			/* Overwrite-then-release (mirrors PH7_MemObjStore): pin the new instance,` |
|       - |  451 | `			 * free the slot's prior contents, then rebind. */` |
|      79 |  452 | `			pBind->iRef++;` |
|      79 |  453 | `			PH7_MemObjRelease(pObj);` |
|      79 |  454 | `			pObj->x.pOther = pBind;` |
|      79 |  455 | `			MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      37 |  456 | `		}` |
|      37 |  457 | `	}` |
|      81 |  458 | `	if( pBind ){` |
|       - |  459 | `		/* Drop the hold VmThrowInline took across the redirect. */` |
|      81 |  460 | `		PH7_ClassInstanceUnref(pBind);` |
|      38 |  461 | `	}` |
|      81 |  462 | `	if( pExc ){` |
|      81 |  463 | `		pExc->pInflight = 0;` |
|      38 |  464 | `	}` |
|      81 |  465 | `	VM_EXIT_BREAK;` |
|     ! 0 |  466 | `	VM_EXIT_BREAK;` |
|       5 |  467 | `}` |
|       - |  468 |  |
|       - |  469 | `/*` |
|       - |  470 | ` * OP_LOAD_EXCEPTION: body moved verbatim from the OP_LOAD_EXCEPTION arm of` |
|       - |  471 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  472 | ` */` |
| 1457980 |  473 | `PH7_PRIVATE VmOpRc VmExecOpLoadException(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  474 | `{` |
| 1457985 |  475 | `	ph7_value *pTos = pState->pTos;` |
| 1457985 |  476 | `	ph7_value *pStack = pState->pStack;` |
| 1457985 |  477 | `	VmInstr *aInstr = pState->aInstr;` |
| 1457985 |  478 | `	sxi32 pc = pState->pc;` |
|       - |  479 | `	sxi32 rc;` |
|  728990 |  480 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - |  481 | `	/* BYTECODE stage 2b: push a fresh ACTIVATION of this lexical try (own` |
|       - |  482 | `	 * mutable state per entry — see VmExcActivate), never the shared` |
|       - |  483 | `	 * compiled object. */` |
| 1457985 |  484 | `	ph7_exception *pException = VmExcActivate(&(*pVm),(ph7_exception *)pInstr->p3);` |
|       - |  485 | `	VmFrame *pFrameLocal;` |
| 1457985 |  486 | `	if( pException == 0 ){` |
|     ! 0 |  487 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|     ! 0 |  488 | `		VM_EXIT_ABORT;` |
|       - |  489 | `	}` |
|       - |  490 | `	/* Create the exception frame BEFORE publishing the activation, so an OOM` |
|       - |  491 | `	 * abort cannot orphan a pushed entry with no frame behind it. */` |
| 1457985 |  492 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
| 1457985 |  493 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  494 | `		VmExcRelease(&(*pVm),pException);` |
|     ! 0 |  495 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|     ! 0 |  496 | `		VM_EXIT_ABORT;` |
|       - |  497 | `	}` |
| 1457985 |  498 | `	if( SXRET_OK != SySetPut(&pVm->aException,(const void *)&pException) ){` |
|     ! 0 |  499 | `		VmExcRelease(&(*pVm),pException);` |
|     ! 0 |  500 | `		VmLeaveFrame(&(*pVm));` |
|     ! 0 |  501 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|     ! 0 |  502 | `		VM_EXIT_ABORT;` |
|       - |  503 | `	}` |
|       - |  504 | `	/* Mark the special frame */` |
| 1457985 |  505 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
| 1457985 |  506 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|       - |  507 | `	/* Record the landing pad on the exception too, so an in-place catch can resume` |
|       - |  508 | `	 * the throwing site at THIS try (survives the exception frame's teardown), plus` |
|       - |  509 | `	 * the bytecode array it indexes — the resume only fires in the exec running that` |
|       - |  510 | `	 * array, so a mini-program (inline try in a catch/finally) and the body that` |
|       - |  511 | `	 * shares its frame don't mis-apply each other's landing pad. iLandingPc mirrors the` |
|       - |  512 | `	 * frame's iExceptionJump just set above — reuse it so the two can't drift. */` |
| 1457985 |  513 | `	pException->iLandingPc = pFrameLocal->iExceptionJump;` |
| 1457985 |  514 | `	pException->pOwnerInstr = (void *)aInstr;` |
|       - |  515 | `	/* Operand-stack base at try entry (0-based TOS index; -1 when empty). The post-try` |
|       - |  516 | `	 * landing pad is reached with the stack back at this depth; Generator::throw()` |
|       - |  517 | `	 * inject-at-yield drains to it before landing (a mid-expression yield leaves the` |
|       - |  518 | `	 * abandoned expression's operands above this base). Normal throws are already here. */` |
| 1457985 |  519 | `	pException->iStackDepth = (sxi32)(pTos - pStack);` |
|       - |  520 | `	/* '@' depth at try entry — see ph7_exception.iErrSuppress */` |
| 1457985 |  521 | `	pException->iErrSuppress = pVm->nErrSuppress;` |
|       - |  522 | `	/* Point to the frame that trigger the exception */` |
| 1457985 |  523 | `	pFrameLocal = pFrameLocal->pParent;` |
| 1457985 |  524 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
| 1457985 |  525 | `	pException->pFrame = pFrameLocal;` |
| 1457985 |  526 | `	VM_EXIT_BREAK;` |
|     ! 0 |  527 | `	VM_EXIT_BREAK;` |
|  728995 |  528 | `}` |
|       - |  529 |  |
