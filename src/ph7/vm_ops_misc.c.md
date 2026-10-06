# src/ph7/vm_ops_misc.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 271/317 lines (85.49%)

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
|  235989 |   28 | `PH7_PRIVATE VmOpRc VmExecOpConsume(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   29 | `{` |
|  235994 |   30 | `	ph7_value *pTos = pState->pTos;` |
|  235994 |   31 | `	ph7_value *pStack = pState->pStack;` |
|  235994 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|  235994 |   33 | `	sxi32 pc = pState->pc;` |
|       - |   34 | `	sxi32 rc;` |
|  117914 |   35 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|  235994 |   36 | `	ph7_output_consumer *pCons = &pVm->sVmConsumer;` |
|  235994 |   37 | `	ph7_value *pCur,*pOut = pTos;` |
|       - |   38 |  |
|  235994 |   39 | `	pOut = &pTos[-pInstr->iP1 + 1];` |
|  235994 |   40 | `	pCur = pOut;` |
|       - |   41 | `	/* Start the consume process  */` |
|  471991 |   42 | `	while( pOut <= pTos ){` |
|       - |   43 | `		/* Force a string cast (echo/print: user-visible array->string warning).` |
|       - |   44 | `` 		 * A not-stringable object throws HERE, mid-list: php compiles `echo a,b,c` `` |
|       - |   45 | `		 * to one ECHO per operand, so everything left of the object is already` |
|       - |   46 | `		 * out. Release what is left so the abandoned operands don't outlive the` |
|       - |   47 | `		 * op, then route. */` |
|  236020 |   48 | `		sxi32 rcSv = PH7_MemObjToStringUV(pOut);` |
|  236020 |   49 | `		if( rcSv != SXRET_OK ){` |
|      39 |   50 | `			while( pOut <= pTos ){` |
|      21 |   51 | `				PH7_MemObjRelease(pOut);` |
|      21 |   52 | `				pOut++;` |
|       3 |   53 | `			}` |
|      21 |   54 | `			pTos = &pCur[-1];` |
|      21 |   55 | `			PH7_DISPATCH_TOSTRING_RC(rcSv)` |
|     ! 0 |   56 | `		}` |
|  236002 |   57 | `		if( SyBlobLength(&pOut->sBlob) > 0 ){` |
|       - |   58 | `			/*SyBlobNullAppend(&pOut->sBlob);*/` |
|       - |   59 | `			/* Invoke the output consumer callback */` |
|  207011 |   60 | `			rc = pCons->xConsumer(SyBlobData(&pOut->sBlob),SyBlobLength(&pOut->sBlob),pCons->pUserData);` |
|  207011 |   61 | `			VmTrackOutput(pVm, SyBlobLength(&pOut->sBlob));` |
|  207011 |   62 | `			SyBlobRelease(&pOut->sBlob);` |
|  207011 |   63 | `			if( rc == SXERR_ABORT ){` |
|       - |   64 | `				/* Output consumer callback request an operation abort. */` |
|     ! 0 |   65 | `				VM_EXIT_ABORT;` |
|       - |   66 | `			}` |
|  103428 |   67 | `		}` |
|  236002 |   68 | `		pOut++;` |
|       5 |   69 | `	}` |
|  235976 |   70 | `	pTos = &pCur[-1];` |
|  235976 |   71 | `	VM_EXIT_BREAK;` |
|     ! 0 |   72 | `	VM_EXIT_BREAK;` |
|  117919 |   73 | `}` |
|       - |   74 |  |
|       - |   75 | `/*` |
|       - |   76 | ` * OP_MATCH: body moved verbatim from the OP_MATCH arm of` |
|       - |   77 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |   78 | ` */` |
|     272 |   79 | `PH7_PRIVATE VmOpRc VmExecOpMatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   80 | `{` |
|     277 |   81 | `	ph7_value *pTos = pState->pTos;` |
|     277 |   82 | `	ph7_value *pStack = pState->pStack;` |
|     277 |   83 | `	VmInstr *aInstr = pState->aInstr;` |
|     277 |   84 | `	sxi32 pc = pState->pc;` |
|       - |   85 | `	sxi32 rc;` |
|     136 |   86 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     277 |   87 | `	ph7_match *pMatch = (ph7_match *)pInstr->p3;` |
|     277 |   88 | `	ph7_match_arm *aArm,*pArm,*pDefault = 0;` |
|       - |   89 | `	ph7_value sSubject,sCond,sResult;` |
|       - |   90 | `	sxu32 i,j,nArm,nCond;` |
|     277 |   91 | `	sxi32 rcArm = SXRET_OK;` |
|     277 |   92 | `	const void *pResumeBefore = (const void *)pVm->pResumeFrame;` |
|     277 |   93 | `	const void *pInlineBefore = (const void *)pVm->pInlineInstr;` |
|     277 |   94 | `	int matched = 0;` |
|       - |   95 | `#ifdef UNTRUST` |
|       - |   96 | `	if( pMatch == 0 \|\| pTos < pStack ){` |
|       - |   97 | `		VM_EXIT_ABORT;` |
|       - |   98 | `	}` |
|       - |   99 | `#endif` |
|     277 |  100 | `	aArm = (ph7_match_arm *)SySetBasePtr(&pMatch->aArms);` |
|     277 |  101 | `	nArm = SySetUsed(&pMatch->aArms);` |
|     277 |  102 | `	PH7_MemObjInit(pVm,&sSubject);` |
|     277 |  103 | `	PH7_MemObjInit(pVm,&sCond);` |
|     277 |  104 | `	PH7_MemObjInit(pVm,&sResult);` |
|     277 |  105 | `	PH7_MemObjLoad(pTos,&sSubject);` |
|       - |  106 | `	/* Each condition and each arm BODY is its own bytecode container, run by` |
|       - |  107 | `	 * VmLocalExec. A throw inside one abandons the whole match expression in php,` |
|       - |  108 | `	 * so its status is routed below (VmLocalExecThrew): ignoring it let a caught` |
|       - |  109 | `	 * throw fall through to the NEXT condition — and then to the default arm,` |
|       - |  110 | `	 * which php never reaches — and handed the abandoned statement a value. */` |
|     863 |  111 | `	for( i = 0; i < nArm && !matched && rcArm == SXRET_OK; ++i ){` |
|     590 |  112 | `		pArm = &aArm[i];` |
|     590 |  113 | `		if( pArm->bDefault ){` |
|      18 |  114 | `			pDefault = pArm;` |
|      18 |  115 | `			continue;` |
|       - |  116 | `		}` |
|     574 |  117 | `		nCond = SySetUsed(&pArm->aConds);` |
|     956 |  118 | `		for( j = 0; j < nCond; ++j ){` |
|     612 |  119 | `			SySet *pCondBc = (SySet *)SySetAt(&pArm->aConds,j);` |
|     612 |  120 | `			if( pCondBc == 0 ){` |
|     ! 0 |  121 | `				continue;` |
|       - |  122 | `			}` |
|     612 |  123 | `			rcArm = VmLocalExec(pVm,pCondBc,&sCond,FALSE);` |
|     612 |  124 | `			if( rcArm == PH7_ABORT \|\| VmLocalExecThrew(pVm,rcArm,pResumeBefore,pInlineBefore) ){` |
|       2 |  125 | `				break;` |
|       - |  126 | `			}` |
|     610 |  127 | `			rcArm = SXRET_OK;` |
|     610 |  128 | `			rc = PH7_MemObjCmp(&sSubject,&sCond,TRUE /* strict */,0);` |
|     610 |  129 | `			PH7_MemObjRelease(&sCond);` |
|     610 |  130 | `			if( rc == 0 ){` |
|     228 |  131 | `				rcArm = VmLocalExec(pVm,&pArm->aResult,&sResult,FALSE);` |
|     228 |  132 | `				matched = 1;` |
|     228 |  133 | `				break;` |
|       - |  134 | `			}` |
|     195 |  135 | `		}` |
|     289 |  136 | `	}` |
|     277 |  137 | `	if( !matched && pDefault && rcArm != PH7_ABORT && !VmLocalExecThrew(pVm,rcArm,pResumeBefore,pInlineBefore) ){` |
|      18 |  138 | `		rcArm = VmLocalExec(pVm,&pDefault->aResult,&sResult,FALSE);` |
|      18 |  139 | `		matched = 1;` |
|       8 |  140 | `	}` |
|     277 |  141 | `	if( rcArm == PH7_ABORT ){` |
|     ! 0 |  142 | `		PH7_MemObjRelease(&sCond);` |
|     ! 0 |  143 | `		PH7_MemObjRelease(&sSubject);` |
|     ! 0 |  144 | `		PH7_MemObjRelease(&sResult);` |
|     ! 0 |  145 | `		VM_EXIT_ABORT;` |
|       - |  146 | `	}` |
|     277 |  147 | `	if( VmLocalExecThrew(pVm,rcArm,pResumeBefore,pInlineBefore) ){` |
|      39 |  148 | `		PH7_MemObjRelease(&sCond);` |
|      39 |  149 | `		PH7_MemObjRelease(&sSubject);` |
|      39 |  150 | `		PH7_MemObjRelease(&sResult);` |
|      39 |  151 | `		VmPopOperand(&pTos,1); /* the subject this OP_MATCH would have replaced */` |
|      39 |  152 | `		PH7_THROW_ROUTE_MIDEXPR(rcArm)` |
|       - |  153 | `	}` |
|     241 |  154 | `	if( !matched ){` |
|       - |  155 | `		SyBlob sErrMsg;` |
|      34 |  156 | `		sxi64 nMax = PH7_VmIniGetInt(&(*pVm),"zend.exception_string_param_max_len",0);` |
|      34 |  157 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      34 |  158 | `		SyBlobAppend(&sErrMsg,"Unhandled match case ",sizeof("Unhandled match case ")-1);` |
|       - |  159 | `		/* php names the VALUE the way a trace argument prints it -- unless traces` |
|       - |  160 | `		 * keep no arguments (zend.exception_ignore_args), or it is a string they` |
|       - |  161 | `		 * would print as '...' -- and falls back to the TYPE, which for an object is` |
|       - |  162 | `		 * its class. */` |
|      30 |  163 | `		if( PH7_VmIniGetBool(&(*pVm),"zend.exception_ignore_args",1)` |
|      24 |  164 | `		 \|\| ((sSubject.iFlags & MEMOBJ_STRING) && nMax == 0)` |
|      22 |  165 | `		 \|\| !PH7_VmAppendTraceScalar(&(*pVm),&sErrMsg,&sSubject,nMax) ){` |
|      20 |  166 | `			const char *zType = "unknown";` |
|      20 |  167 | `			switch(sSubject.iFlags & MEMOBJ_ALL){` |
|     ! 0 |  168 | `			case MEMOBJ_NULL:   zType = "null";   break;` |
|     ! 0 |  169 | `			case MEMOBJ_BOOL:   zType = "bool";   break;` |
|      14 |  170 | `			case MEMOBJ_INT:    zType = "int";    break;` |
|     ! 0 |  171 | `			case MEMOBJ_REAL:   zType = "float";  break;` |
|     ! 0 |  172 | `			case MEMOBJ_STRING: zType = "string"; break;` |
|       3 |  173 | `			case MEMOBJ_HASHMAP:zType = "array";  break;` |
|       5 |  174 | `			case MEMOBJ_OBJ:    zType = 0;        break;` |
|     ! 0 |  175 | `			case MEMOBJ_RES:    zType = "resource"; break;` |
|     ! 0 |  176 | `			default: break;` |
|       - |  177 | `			}` |
|      20 |  178 | `			SyBlobAppend(&sErrMsg,"of type ",sizeof("of type ")-1);` |
|      20 |  179 | `			if( zType ){` |
|      16 |  180 | `				SyBlobAppend(&sErrMsg,zType,(sxu32)SyStrlen(zType));` |
|      10 |  181 | `			}else{` |
|       5 |  182 | `				ph7_class_instance *pObj = (ph7_class_instance *)sSubject.x.pOther;` |
|       5 |  183 | `				if( pObj && pObj->pClass ){` |
|       5 |  184 | `					SyBlobFormat(&sErrMsg,"%z",&pObj->pClass->sDisp);` |
|       2 |  185 | `				}` |
|       - |  186 | `			}` |
|       8 |  187 | `		}` |
|      34 |  188 | `		PH7_MemObjRelease(&sSubject);` |
|      34 |  189 | `		PH7_MemObjRelease(&sResult);` |
|       - |  190 | `		/* php raises a CATCHABLE \UnhandledMatchError here, so build a real` |
|       - |  191 | `		 * exception object and route it like any mid-expression throw (an` |
|       - |  192 | `		 * enclosing try in this body resumes at its landing pad; otherwise it` |
|       - |  193 | `		 * propagates and renders as uncaught, as before). Reporting it straight` |
|       - |  194 | `		 * to the uncaught renderer — what this site used to do — made the error` |
|       - |  195 | `		 * unconditionally fatal even inside try/catch. */` |
|      34 |  196 | `		rc = VmThrowBuiltinError(&(*pVm),"UnhandledMatchError",` |
|       - |  197 | `			sizeof("UnhandledMatchError")-1,&sErrMsg);` |
|      58 |  198 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  199 | `	}` |
|     210 |  200 | `	PH7_MemObjRelease(&sSubject);` |
|       - |  201 | `	/* Replace subject on TOS with the arm result */` |
|     210 |  202 | `	PH7_MemObjStore(&sResult,pTos);` |
|     210 |  203 | `	PH7_MemObjRelease(&sResult);` |
|     210 |  204 | `	VM_EXIT_BREAK;` |
|     ! 0 |  205 | `	VM_EXIT_BREAK;` |
|     141 |  206 | `}` |
|       - |  207 |  |
|       - |  208 | `/*` |
|       - |  209 | ` * Build an \Error instance carrying zMsg (or NULL when the class is` |
|       - |  210 | ` * unavailable). Shared by OP_THROW's two "operand cannot be thrown" cases:` |
|       - |  211 | ` * php reports a non-object as "Can only throw objects" and a non-Throwable` |
|       - |  212 | ` * object as "Cannot throw objects that do not implement Throwable", both as` |
|       - |  213 | ` * ordinary catchable throws. The caller hands the instance to` |
|       - |  214 | ` * VmThrowException so the routing below sees exactly the status a normal` |
|       - |  215 | ` * throw produces. Error::__construct comes from the built-in library and` |
|       - |  216 | ` * cannot realistically fail, so its return is not checked.` |
|       - |  217 | ` */` |
|      18 |  218 | `static ph7_class_instance * VmNewThrowError(ph7_vm *pVm,const char *zMsg,sxu32 nMsg)` |
|       2 |  219 | `{` |
|       - |  220 | `	ph7_class *pErrorClass;` |
|       - |  221 | `	ph7_class_instance *pErrInst;` |
|       - |  222 | `	ph7_class_method *pCons;` |
|      20 |  223 | `	pErrorClass = PH7_VmExtractClass(&(*pVm),"Error",sizeof("Error")-1,TRUE,0);` |
|      20 |  224 | `	if( pErrorClass == 0 ){` |
|     ! 0 |  225 | `		return 0;` |
|       - |  226 | `	}` |
|      20 |  227 | `	pErrInst = PH7_NewClassInstance(&(*pVm),pErrorClass);` |
|      20 |  228 | `	if( pErrInst == 0 ){` |
|     ! 0 |  229 | `		return 0;` |
|       - |  230 | `	}` |
|      20 |  231 | `	pCons = PH7_ClassExtractMethod(pErrorClass,"__construct",sizeof("__construct")-1);` |
|      20 |  232 | `	if( pCons ){` |
|       - |  233 | `		ph7_value sArg;` |
|       - |  234 | `		ph7_value *apArg[1];` |
|       - |  235 | `		SyString sMsgStr;` |
|      20 |  236 | `		SyStringInitFromBuf(&sMsgStr,zMsg,nMsg);` |
|      20 |  237 | `		PH7_MemObjInitFromString(pVm,&sArg,&sMsgStr);` |
|      20 |  238 | `		apArg[0] = &sArg;` |
|      20 |  239 | `		PH7_VmCallClassMethod(&(*pVm),pErrInst,pCons,0,1,apArg);` |
|      20 |  240 | `		PH7_MemObjRelease(&sArg);` |
|       9 |  241 | `	}` |
|      20 |  242 | `	return pErrInst;` |
|      11 |  243 | `}` |
|       - |  244 | `/*` |
|       - |  245 | ` * OP_THROW: body moved verbatim from the OP_THROW arm of` |
|       - |  246 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  247 | ` */` |
| 1001303 |  248 | `PH7_PRIVATE VmOpRc VmExecOpThrow(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  249 | `{` |
| 1001308 |  250 | `	ph7_value *pTos = pState->pTos;` |
| 1001308 |  251 | `	ph7_value *pStack = pState->pStack;` |
| 1001308 |  252 | `	VmInstr *aInstr = pState->aInstr;` |
| 1001308 |  253 | `	sxi32 pc = pState->pc;` |
|       - |  254 | `	sxi32 rc;` |
|  500650 |  255 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
| 1001308 |  256 | `	VmFrame *pFrameLocal = pVm->pFrame;` |
| 1001308 |  257 | `	sxu32 nJump = pInstr->iP2;` |
|       - |  258 | `#ifdef UNTRUST` |
|       - |  259 | `	if( pTos < pStack ){` |
|       - |  260 | `		VM_EXIT_ABORT;` |
|       - |  261 | `	}` |
|       - |  262 | `#endif` |
| 1001308 |  263 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
|       - |  264 | `	/* Tell the upper layer that an exception was thrown */` |
| 1001308 |  265 | `	pFrameLocal->iFlags \|= VM_FRAME_THROW;` |
| 1001308 |  266 | `	if( pTos->iFlags & MEMOBJ_OBJ ){` |
| 1001294 |  267 | `		ph7_class_instance *pThis = (ph7_class_instance *)pTos->x.pOther;` |
|       - |  268 | `		ph7_class *pThrowable;` |
|       - |  269 | `		/* Thrown object must implement the Throwable interface (PHP 7+). */` |
| 1001294 |  270 | `		pThrowable = PH7_VmExtractClass(&(*pVm),"Throwable",sizeof("Throwable")-1,FALSE,0);` |
| 1001296 |  271 | `		if( pThrowable == 0 \|\| !PH7_VmInstanceOf(pThis->pClass,pThrowable) ){` |
|       - |  272 | `			/* Not a Throwable: replace with Error(msg) matching PHP behavior. */` |
|       - |  273 | `			static const char zErrMsg[] =` |
|       - |  274 | `				"Cannot throw objects that do not implement Throwable";` |
|       6 |  275 | `			ph7_class_instance *pErrInst = VmNewThrowError(&(*pVm),zErrMsg,sizeof(zErrMsg)-1);` |
|       6 |  276 | `			if( pErrInst ){` |
|       6 |  277 | `				rc = VmThrowException(&(*pVm),pErrInst);` |
|       6 |  278 | `				PH7_ClassInstanceUnref(pErrInst);` |
|       6 |  279 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  280 | `					VM_EXIT_ABORT;` |
|       - |  281 | `				}` |
|       4 |  282 | `			}else{` |
|       - |  283 | `				/* Bootstrap failure — fall back to uncaught reporting */` |
|     ! 0 |  284 | `				rc = VmUncaughtException(&(*pVm),pThis);` |
|     ! 0 |  285 | `				if( rc == SXERR_ABORT ){` |
|     ! 0 |  286 | `					VM_EXIT_ABORT;` |
|       - |  287 | `				}` |
|       - |  288 | `			}` |
|       4 |  289 | `		}else{` |
|       - |  290 | `			/* Throw the exception */` |
| 1001290 |  291 | `			rc = VmThrowException(&(*pVm),pThis);` |
| 1001290 |  292 | `			if( rc == SXERR_ABORT ){` |
|       - |  293 | `				/* Abort processing immediately */` |
|      51 |  294 | `				VM_EXIT_ABORT;` |
|       - |  295 | `			}` |
|       - |  296 | `		}` |
|  500625 |  297 | `	}else{` |
|       - |  298 | `		/* php raises a CATCHABLE Error for a non-object operand. This used to` |
|       - |  299 | `		 * report a bogus uncaught "Exception" and then fall into the jump below,` |
|       - |  300 | `		 * so no catch ran yet execution carried on past the try — and the branch` |
|       - |  301 | `		 * tested a STALE rc left by the previous instruction, which is the` |
|       - |  302 | `		 * latent bug the interpreter split surfaced. Build the same Error shape` |
|       - |  303 | `		 * the not-Throwable case uses and let the routing below land it. */` |
|       - |  304 | `		static const char zErrMsg[] = "Can only throw objects";` |
|      15 |  305 | `		ph7_class_instance *pErrInst = VmNewThrowError(&(*pVm),zErrMsg,sizeof(zErrMsg)-1);` |
|      15 |  306 | `		if( pErrInst ){` |
|      15 |  307 | `			rc = VmThrowException(&(*pVm),pErrInst);` |
|      15 |  308 | `			PH7_ClassInstanceUnref(pErrInst);` |
|       8 |  309 | `		}else{` |
|       - |  310 | `			/* Bootstrap failure — fall back to uncaught reporting */` |
|     ! 0 |  311 | `			rc = VmUncaughtException(&(*pVm),0);` |
|       - |  312 | `		}` |
|      15 |  313 | `		if( rc == SXERR_ABORT ){` |
|       - |  314 | `			/* Abort processing immediately */` |
|     ! 0 |  315 | `			VM_EXIT_ABORT;` |
|       - |  316 | `		}` |
|       - |  317 | `	}` |
|       - |  318 | `	/* Pop the top entry */` |
| 1001262 |  319 | `	VmPopOperand(&pTos,1);` |
|       - |  320 | `	/* ROOT C: throw caught by an inline try in THIS exec -> jump to its catch/finally` |
|       - |  321 | `	 * (draining any mid-expression operands back to the try's base). */` |
| 1001262 |  322 | `	PH7_INLINE_RESUME_BREAK()` |
|       - |  323 | `	/* pInlineInstr still set here means an inline try in an OUTER exec caught it (e.g. a` |
|       - |  324 | ``	 * throwing sub-generator delegated via `yield from`): propagate so the owning exec lands. */`` |
| 1001240 |  325 | `	if( rc == PH7_EXCEPTION \|\| pVm->pResumeFrame \|\| pVm->pInlineInstr ){` |
|       - |  326 | ``		/* The throw was handled by a `catch` that ran IN PLACE — either this try's own`` |
|       - |  327 | `		 * finally threw past itself superseding it (rc == PH7_EXCEPTION), or the catch` |
|       - |  328 | `		 * sits several frames above this one (pVm->pResumeFrame recorded). Resume at the` |
|       - |  329 | `		 * catching body's landing pad if that body is THIS exec (VmRecordedResume), else` |
|       - |  330 | `		 * unwind so the owning exec lands. Without this a throw caught at an enclosing` |
|       - |  331 | `		 * frame would blindly jump to nJump (this throw's lexically-nearest try) and run` |
|       - |  332 | `		 * dead code after it (ROOT B, face a) or continue a callee as if it never threw` |
|       - |  333 | `		 * (face c). */` |
|       - |  334 | `		sxi32 iResumePc;` |
|  901156 |  335 | `		if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){` |
|  400405 |  336 | `			PH7_RESUME_DRAIN()` |
|  300405 |  337 | `			pc = iResumePc;` |
|  300405 |  338 | `			VM_EXIT_BREAK;` |
|       - |  339 | `		}` |
|  600756 |  340 | `		VM_EXIT_EXCEPTION;` |
|       - |  341 | `	}` |
|       - |  342 | `	/* No in-place catch recorded: this throw's own enclosing try caught it (the` |
|       - |  343 | `	 * common case; its landing pad is exactly nJump). A throw-EXPRESSION` |
|       - |  344 | ``	 * (`$q = 1 + throw new E`) abandons its outer expression's pending operands`` |
|       - |  345 | `	 * above the try's base — drain to the catching activation's recorded depth` |
|       - |  346 | `	 * (matched by landing pad + bytecode array so an unrelated activation can` |
|       - |  347 | `	 * never be consulted) before jumping, or each caught throw leaks a slot.` |
|       - |  348 | `	 * Then jump to the try's OP_POP_EXCEPTION landing pad, which tears down the` |
|       - |  349 | ``	 * try frame, runs finally, and (when a catch/finally issued a `return`)`` |
|       - |  350 | `	 * materializes the body frame's pending return. Routing the return through` |
|       - |  351 | `	 * OP_POP_EXCEPTION keeps the frame stack balanced. */` |
|  100089 |  352 | `	if( SySetUsed(&pVm->aException) > 0 ){` |
|     ! 0 |  353 | `		ph7_exception *pTopExc = ((ph7_exception **)SySetBasePtr(&pVm->aException))[SySetUsed(&pVm->aException)-1];` |
|     ! 0 |  354 | `		if( pTopExc->iLandingPc == (sxu32)nJump && pTopExc->pOwnerInstr == (void *)aInstr ){` |
|     ! 0 |  355 | `			while( (sxi32)(pTos - pStack) > pTopExc->iStackDepth ){` |
|     ! 0 |  356 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 |  357 | `				pTos--;` |
|     ! 0 |  358 | `			}` |
|     ! 0 |  359 | `		}` |
|     ! 0 |  360 | `	}` |
|  100089 |  361 | `	pc = nJump - 1;` |
|  100089 |  362 | `	VM_EXIT_BREAK;` |
|     ! 0 |  363 | `	VM_EXIT_BREAK;` |
|  500655 |  364 | `}` |
|       - |  365 |  |
|       - |  366 | `/*` |
|       - |  367 | ` * OP_SWITCH: body moved verbatim from the OP_SWITCH arm of` |
|       - |  368 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  369 | ` */` |
|     300 |  370 | `PH7_PRIVATE VmOpRc VmExecOpSwitch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  371 | `{` |
|     305 |  372 | `	ph7_value *pTos = pState->pTos;` |
|     305 |  373 | `	ph7_value *pStack = pState->pStack;` |
|     305 |  374 | `	VmInstr *aInstr = pState->aInstr;` |
|     305 |  375 | `	sxi32 pc = pState->pc;` |
|       - |  376 | `	sxi32 rc;` |
|     150 |  377 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     305 |  378 | `	ph7_switch *pSwitch = (ph7_switch *)pInstr->p3;` |
|       - |  379 | `	ph7_case_expr *aCase,*pCase;` |
|       - |  380 | `	ph7_value sValue,sCaseValue;` |
|       - |  381 | `	sxu32 n,nEntry;` |
|     305 |  382 | `	const void *pResumeBefore = (const void *)pVm->pResumeFrame;` |
|     305 |  383 | `	const void *pInlineBefore = (const void *)pVm->pInlineInstr;` |
|       - |  384 | `#ifdef UNTRUST` |
|       - |  385 | `	if( pSwitch == 0 \|\| pTos < pStack ){` |
|       - |  386 | `		VM_EXIT_ABORT;` |
|       - |  387 | `	}` |
|       - |  388 | `#endif` |
|       - |  389 | `	/* Point to the case table  */` |
|     305 |  390 | `	aCase = (ph7_case_expr *)SySetBasePtr(&pSwitch->aCaseExpr);` |
|     305 |  391 | `	nEntry = SySetUsed(&pSwitch->aCaseExpr);` |
|       - |  392 | `	/* Select the appropriate case block to execute */` |
|     305 |  393 | `	PH7_MemObjInit(pVm,&sValue);` |
|     305 |  394 | `	PH7_MemObjInit(pVm,&sCaseValue);` |
|     769 |  395 | `	for( n = 0 ; n < nEntry ; ++n ){` |
|       - |  396 | `		sxi32 rcCase;` |
|     703 |  397 | `		pCase = &aCase[n];` |
|     703 |  398 | `		PH7_MemObjLoad(pTos,&sValue);` |
|       - |  399 | `		/* Execute the case expression first. It is its own bytecode container` |
|       - |  400 | ``		 * (VmLocalExec), so a throw inside it — `case boom():` — used to be caught`` |
|       - |  401 | `		 * in place and then IGNORED here: the scan carried on into the remaining` |
|       - |  402 | ``		 * cases and finally jumped to `default:`, running a branch php never`` |
|       - |  403 | `		 * reaches. Route the status the way any mid-expression throw is routed. */` |
|     703 |  404 | `		rcCase = VmLocalExec(pVm,&pCase->aByteCode,&sCaseValue,FALSE);` |
|     703 |  405 | `		if( rcCase == PH7_ABORT \|\| VmLocalExecThrew(pVm,rcCase,pResumeBefore,pInlineBefore) ){` |
|       5 |  406 | `			PH7_MemObjRelease(&sValue);` |
|       5 |  407 | `			PH7_MemObjRelease(&sCaseValue);` |
|       5 |  408 | `			if( rcCase == PH7_ABORT ){` |
|     ! 0 |  409 | `				VM_EXIT_ABORT;` |
|       - |  410 | `			}` |
|       5 |  411 | `			VmPopOperand(&pTos,1); /* the switch subject */` |
|       5 |  412 | `			PH7_THROW_ROUTE_MIDEXPR(rcCase)` |
|       - |  413 | `		}` |
|       - |  414 | `		/* Compare the two expression */` |
|     699 |  415 | `		rc = PH7_MemObjCmp(&sValue,&sCaseValue,FALSE,0);` |
|     699 |  416 | `		PH7_MemObjRelease(&sValue);` |
|     699 |  417 | `		PH7_MemObjRelease(&sCaseValue);` |
|     699 |  418 | `		if( PH7_CmpRefusalPending(pVm) ){` |
|       - |  419 | `			/* A native compare handler refused this pair (two different kinds of` |
|       - |  420 | `			 * DateTimeZone). php raises it out of the comparison, so the switch` |
|       - |  421 | ``			 * never reaches a branch -- not even `default:`. */`` |
|       - |  422 | `			sxi32 rcRef;` |
|       5 |  423 | `			VmPopOperand(&pTos,1); /* the switch subject */` |
|       5 |  424 | `			rcRef = PH7_CmpRefusalRaise(pVm);` |
|       5 |  425 | `			if( rcRef == SXERR_ABORT ){` |
|     ! 0 |  426 | `				VM_EXIT_ABORT;` |
|       - |  427 | `			}` |
|       5 |  428 | `			PH7_THROW_ROUTE_MIDEXPR(rcRef)` |
|       - |  429 | `		}` |
|     695 |  430 | `		if( rc == 0 ){` |
|       - |  431 | `			/* Value match,jump to this block */` |
|     231 |  432 | `			pc = pCase->nStart - 1;` |
|     231 |  433 | `			break;` |
|       - |  434 | `		}` |
|     237 |  435 | `	}` |
|     297 |  436 | `	VmPopOperand(&pTos,1);` |
|     297 |  437 | `	if( n >= nEntry ){` |
|       - |  438 | `		/* No approprite case to execute,jump to the default case */` |
|      68 |  439 | `		if( pSwitch->nDefault > 0 ){` |
|      62 |  440 | `			pc = pSwitch->nDefault - 1;` |
|      32 |  441 | `		}else{` |
|       - |  442 | `			/* No default case,jump out of this switch */` |
|       7 |  443 | `			pc = pSwitch->nOut - 1;` |
|       - |  444 | `		}` |
|      33 |  445 | `	}` |
|     297 |  446 | `	VM_EXIT_BREAK;` |
|     ! 0 |  447 | `	VM_EXIT_BREAK;` |
|     155 |  448 | `}` |
|       - |  449 |  |
|       - |  450 | `/*` |
|       - |  451 | ` * OP_CATCH: body moved verbatim from the OP_CATCH arm of` |
|       - |  452 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  453 | ` */` |
|      88 |  454 | `PH7_PRIVATE VmOpRc VmExecOpCatch(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  455 | `{` |
|      93 |  456 | `	ph7_value *pTos = pState->pTos;` |
|      93 |  457 | `	ph7_value *pStack = pState->pStack;` |
|      93 |  458 | `	VmInstr *aInstr = pState->aInstr;` |
|      93 |  459 | `	sxi32 pc = pState->pc;` |
|       - |  460 | `	sxi32 rc;` |
|      44 |  461 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - |  462 | `	/* BYTECODE stage 2b: mutable state (pInflight) lives on the LIVE activation` |
|       - |  463 | `	 * of this try, not the compiled p3 (which VmThrowInline kept on aException` |
|       - |  464 | `	 * marked iInCatch). Compiled fields (sEntry) are shared either way. */` |
|      93 |  465 | `	ph7_exception *pExcC = (ph7_exception *)pInstr->p3;` |
|      93 |  466 | `	ph7_exception *pExc = VmExcLive(&(*pVm),pExcC);` |
|      93 |  467 | `	ph7_exception_block *pCatch = (ph7_exception_block *)SySetAt(&pExcC->sEntry,(sxu32)pInstr->iP1);` |
|      93 |  468 | `	ph7_class_instance *pBind = pExc ? pExc->pInflight : 0;` |
|      93 |  469 | `	VmFrame *pBody = VmSkipExceptionFrames(pVm->pFrame);` |
|      93 |  470 | `	pBody->iFlags &= ~VM_FRAME_THROW;` |
|      93 |  471 | `	if( pCatch && pBind && pCatch->sThis.nByte > 0 ){` |
|       - |  472 | `		/* sThis empty => PHP 8.0 non-capturing catch (catch (Type) {}): the` |
|       - |  473 | `		 * exception is caught but not bound to any variable. */` |
|      91 |  474 | `		ph7_value *pObj = VmExtractMemObj(&(*pVm),&pCatch->sThis,FALSE,TRUE);` |
|      91 |  475 | `		if( pObj ){` |
|       - |  476 | `			/* Overwrite-then-release (mirrors PH7_MemObjStore): pin the new instance,` |
|       - |  477 | `			 * free the slot's prior contents, then rebind. */` |
|      91 |  478 | `			pBind->iRef++;` |
|      91 |  479 | `			PH7_MemObjRelease(pObj);` |
|      91 |  480 | `			pObj->x.pOther = pBind;` |
|      91 |  481 | `			MemObjSetType(pObj,MEMOBJ_OBJ);` |
|      43 |  482 | `		}` |
|      43 |  483 | `	}` |
|      93 |  484 | `	if( pBind ){` |
|       - |  485 | `		/* Drop the hold VmThrowInline took across the redirect. */` |
|      93 |  486 | `		PH7_ClassInstanceUnref(pBind);` |
|      44 |  487 | `	}` |
|      93 |  488 | `	if( pExc ){` |
|      93 |  489 | `		pExc->pInflight = 0;` |
|      44 |  490 | `	}` |
|      93 |  491 | `	VM_EXIT_BREAK;` |
|     ! 0 |  492 | `	VM_EXIT_BREAK;` |
|       5 |  493 | `}` |
|       - |  494 |  |
|       - |  495 | `/*` |
|       - |  496 | ` * OP_LOAD_EXCEPTION: body moved verbatim from the OP_LOAD_EXCEPTION arm of` |
|       - |  497 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  498 | ` */` |
| 1488350 |  499 | `PH7_PRIVATE VmOpRc VmExecOpLoadException(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  500 | `{` |
| 1488355 |  501 | `	ph7_value *pTos = pState->pTos;` |
| 1488355 |  502 | `	ph7_value *pStack = pState->pStack;` |
| 1488355 |  503 | `	VmInstr *aInstr = pState->aInstr;` |
| 1488355 |  504 | `	sxi32 pc = pState->pc;` |
|       - |  505 | `	sxi32 rc;` |
|  744066 |  506 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - |  507 | `	/* BYTECODE stage 2b: push a fresh ACTIVATION of this lexical try (own` |
|       - |  508 | `	 * mutable state per entry — see VmExcActivate), never the shared` |
|       - |  509 | `	 * compiled object. */` |
| 1488355 |  510 | `	ph7_exception *pException = VmExcActivate(&(*pVm),(ph7_exception *)pInstr->p3);` |
|       - |  511 | `	VmFrame *pFrameLocal;` |
| 1488355 |  512 | `	if( pException == 0 ){` |
|     ! 0 |  513 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|     ! 0 |  514 | `		VM_EXIT_ABORT;` |
|       - |  515 | `	}` |
|       - |  516 | `	/* Create the exception frame BEFORE publishing the activation, so an OOM` |
|       - |  517 | `	 * abort cannot orphan a pushed entry with no frame behind it. */` |
| 1488355 |  518 | `	rc = VmEnterFrame(&(*pVm),0,0,&pFrameLocal);` |
| 1488355 |  519 | `	if( rc != SXRET_OK ){` |
|     ! 0 |  520 | `		VmExcRelease(&(*pVm),pException);` |
|     ! 0 |  521 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|     ! 0 |  522 | `		VM_EXIT_ABORT;` |
|       - |  523 | `	}` |
| 1488355 |  524 | `	if( SXRET_OK != SySetPut(&pVm->aException,(const void *)&pException) ){` |
|     ! 0 |  525 | `		VmExcRelease(&(*pVm),pException);` |
|     ! 0 |  526 | `		VmLeaveFrame(&(*pVm));` |
|     ! 0 |  527 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Fatal PH7 engine is runnig out of memory");` |
|     ! 0 |  528 | `		VM_EXIT_ABORT;` |
|       - |  529 | `	}` |
|       - |  530 | `	/* Mark the special frame */` |
| 1488355 |  531 | `	pFrameLocal->iFlags \|= VM_FRAME_EXCEPTION;` |
| 1488355 |  532 | `	pFrameLocal->iExceptionJump = pInstr->iP2;` |
|       - |  533 | `	/* Record the landing pad on the exception too, so an in-place catch can resume` |
|       - |  534 | `	 * the throwing site at THIS try (survives the exception frame's teardown), plus` |
|       - |  535 | `	 * the bytecode array it indexes — the resume only fires in the exec running that` |
|       - |  536 | `	 * array, so a mini-program (inline try in a catch/finally) and the body that` |
|       - |  537 | `	 * shares its frame don't mis-apply each other's landing pad. iLandingPc mirrors the` |
|       - |  538 | `	 * frame's iExceptionJump just set above — reuse it so the two can't drift. */` |
| 1488355 |  539 | `	pException->iLandingPc = pFrameLocal->iExceptionJump;` |
| 1488355 |  540 | `	pException->pOwnerInstr = (void *)aInstr;` |
|       - |  541 | `	/* Operand-stack base at try entry (0-based TOS index; -1 when empty). The post-try` |
|       - |  542 | `	 * landing pad is reached with the stack back at this depth; Generator::throw()` |
|       - |  543 | `	 * inject-at-yield drains to it before landing (a mid-expression yield leaves the` |
|       - |  544 | `	 * abandoned expression's operands above this base). Normal throws are already here. */` |
| 1488355 |  545 | `	pException->iStackDepth = (sxi32)(pTos - pStack);` |
|       - |  546 | `	/* '@' depth at try entry — see ph7_exception.iErrSuppress */` |
| 1488355 |  547 | `	pException->iErrSuppress = pVm->nErrSuppress;` |
|       - |  548 | `	/* Late-static-binding depth at try entry — see ph7_exception.nSelfDepth. */` |
| 1488355 |  549 | `	pException->nSelfDepth = SySetUsed(&pVm->aSelf);` |
|       - |  550 | `	/* Point to the frame that trigger the exception */` |
| 1488355 |  551 | `	pFrameLocal = pFrameLocal->pParent;` |
| 1488355 |  552 | `	pFrameLocal = VmSkipExceptionFrames(pFrameLocal);` |
| 1488355 |  553 | `	pException->pFrame = pFrameLocal;` |
| 1488355 |  554 | `	VM_EXIT_BREAK;` |
|     ! 0 |  555 | `	VM_EXIT_BREAK;` |
|  744071 |  556 | `}` |
|       - |  557 |  |
