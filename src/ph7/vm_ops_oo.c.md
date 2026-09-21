# src/ph7/vm_ops_oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 934/1105 lines (84.52%)

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
|       - |   25 | ` * OP_CLONE_APPLY: body moved verbatim from the OP_CLONE_APPLY arm of` |
|       - |   26 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |   27 | ` */` |
|      16 |   28 | `PH7_PRIVATE VmOpRc VmExecOpCloneApply(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       1 |   29 | `{` |
|      17 |   30 | `	ph7_value *pTos = pState->pTos;` |
|      17 |   31 | `	ph7_value *pStack = pState->pStack;` |
|      17 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|      17 |   33 | `	sxi32 pc = pState->pc;` |
|       - |   34 | `	sxi32 rc;` |
|       8 |   35 | `	SXUNUSED(pInstr);` |
|       8 |   36 | `	SXUNUSED(pStack);` |
|       - |   37 | `	ph7_value *pUpdates,*pObj;` |
|       - |   38 | `	ph7_class_instance *pClone;` |
|       - |   39 | `	ph7_hashmap *pMap;` |
|       - |   40 | `	ph7_hashmap_node *pNode;` |
|      17 |   41 | `	sxi32 rcApply = SXRET_OK;` |
|       - |   42 | `	sxu32 n;` |
|       - |   43 | `#ifdef UNTRUST` |
|       - |   44 | `	if( pTos < &pStack[1] ){` |
|       - |   45 | `		VM_EXIT_ABORT;` |
|       - |   46 | `	}` |
|       - |   47 | `#endif` |
|      17 |   48 | `	pUpdates = pTos;` |
|      17 |   49 | `	pObj = &pTos[-1];` |
|       - |   50 | `	/* $withProperties must be an array (PHP: TypeError otherwise). */` |
|      17 |   51 | `	if( (pUpdates->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|       - |   52 | `		SyBlob sMsg;` |
|     ! 0 |   53 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     ! 0 |   54 | `		SyBlobFormat(&sMsg,"clone(): Argument #2 ($withProperties) must be of type array, %s given",` |
|     ! 0 |   55 | `			ph7_type_name(pUpdates));` |
|     ! 0 |   56 | `		rc = VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|     ! 0 |   57 | `		if( rc == PH7_ABORT ){` |
|     ! 0 |   58 | `			VM_EXIT_ABORT;` |
|       - |   59 | `		}` |
|       - |   60 | `		/* Pop the (bad) updates argument and dispatch to the nearest catch. */` |
|     ! 0 |   61 | `		VmPopOperand(&pTos,1);` |
|       - |   62 | `		{` |
|       - |   63 | `			sxi32 iRp;` |
|     ! 0 |   64 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|     ! 0 |   65 | `				pc = iRp;` |
|     ! 0 |   66 | `				VM_EXIT_BREAK;` |
|       - |   67 | `			}` |
|       - |   68 | `		}` |
|     ! 0 |   69 | `		VM_EXIT_EXCEPTION;` |
|       - |   70 | `	}` |
|       - |   71 | `	/* The clone must be an object; OP_CLONE leaves NULL only on a prior failure` |
|       - |   72 | `	 * (already reported) — in that case just drop the updates and carry the NULL. */` |
|      17 |   73 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     ! 0 |   74 | `		VmPopOperand(&pTos,1);` |
|     ! 0 |   75 | `		VM_EXIT_BREAK;` |
|       - |   76 | `	}` |
|      17 |   77 | `	pClone = (ph7_class_instance *)pObj->x.pOther;` |
|      17 |   78 | `	pMap = (ph7_hashmap *)pUpdates->x.pOther;` |
|       - |   79 | `	/* Apply each update in insertion order (pFirst -> pPrev is the forward link). */` |
|      17 |   80 | `	pNode = pMap->pFirst;` |
|      35 |   81 | `	for( n = pMap->nEntry ; n > 0 && rcApply == SXRET_OK ; --n ){` |
|       - |   82 | `		ph7_value *pVal;` |
|       - |   83 | `		ph7_value sVal;` |
|       - |   84 | `		const char *zName;` |
|       - |   85 | `		sxu32 nName;` |
|       - |   86 | `		char zKeyBuf[64];` |
|      19 |   87 | `		if( pNode == 0 ){` |
|     ! 0 |   88 | `			break;` |
|       - |   89 | `		}` |
|      19 |   90 | `		pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|      19 |   91 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|       - |   92 | ``			/* An int key becomes the property name (PHP: `$5`). */`` |
|     ! 0 |   93 | `			nName = SyBufferFormat(zKeyBuf,sizeof(zKeyBuf),"%qd",pNode->xKey.iKey);` |
|     ! 0 |   94 | `			zName = zKeyBuf;` |
|     ! 0 |   95 | `		}else{` |
|      19 |   96 | `			zName = (const char *)SyBlobData(&pNode->xKey.sKey);` |
|      19 |   97 | `			nName = SyBlobLength(&pNode->xKey.sKey);` |
|       - |   98 | `		}` |
|      19 |   99 | `		if( pVal ){` |
|       - |  100 | `			/* Snapshot the update value into a stack local FIRST: applying it may` |
|       - |  101 | `			 * create a dynamic property, whose slot reservation can reallocate` |
|       - |  102 | `			 * pVm->aMemObj and dangle pVal (a pointer into it). The name is safe` |
|       - |  103 | `			 * (it lives in the node's key blob / zKeyBuf, not in aMemObj). */` |
|      19 |  104 | `			PH7_MemObjInit(pVm,&sVal);` |
|      19 |  105 | `			PH7_MemObjLoad(pVal,&sVal);` |
|      19 |  106 | `			rcApply = VmCloneApplyUpdate(pVm,pClone,zName,nName,&sVal);` |
|      19 |  107 | `			PH7_MemObjRelease(&sVal);` |
|       9 |  108 | `		}` |
|      19 |  109 | `		pNode = pNode->pPrev;` |
|      10 |  110 | `	}` |
|      17 |  111 | `	if( rcApply == PH7_ABORT ){` |
|     ! 0 |  112 | `		VM_EXIT_ABORT;` |
|       - |  113 | `	}` |
|      17 |  114 | `	if( rcApply == PH7_EXCEPTION ){` |
|       - |  115 | `		/* An update threw (visibility / readonly / type). Pop the updates array` |
|       - |  116 | `		 * and hand control to the nearest catch, else propagate out of the loop. */` |
|       5 |  117 | `		VmPopOperand(&pTos,1);` |
|       - |  118 | `		{` |
|       - |  119 | `			sxi32 iRp;` |
|       5 |  120 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|       5 |  121 | `				pc = iRp;` |
|       5 |  122 | `				VM_EXIT_BREAK;` |
|       - |  123 | `			}` |
|       - |  124 | `		}` |
|     ! 0 |  125 | `		VM_EXIT_EXCEPTION;` |
|       - |  126 | `	}` |
|       - |  127 | `	/* Success: drop the updates array, leaving the clone on the stack. */` |
|      13 |  128 | `	VmPopOperand(&pTos,1);` |
|      13 |  129 | `	VM_EXIT_BREAK;` |
|     ! 0 |  130 | `	VM_EXIT_BREAK;` |
|       9 |  131 | `}` |
|       - |  132 |  |
|       - |  133 | `/*` |
|       - |  134 | ` * OP_NEW: body moved verbatim from the OP_NEW arm of` |
|       - |  135 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  136 | ` */` |
| 1105192 |  137 | `PH7_PRIVATE VmOpRc VmExecOpNew(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  138 | `{` |
| 1105197 |  139 | `	ph7_value *pTos = pState->pTos;` |
| 1105197 |  140 | `	ph7_value *pStack = pState->pStack;` |
| 1105197 |  141 | `	VmInstr *aInstr = pState->aInstr;` |
| 1105197 |  142 | `	sxi32 pc = pState->pc;` |
|       - |  143 | `	sxi32 rc;` |
|       - |  144 | `	/* Constructor arg count: compile-time args plus THIS new's own unpack` |
|       - |  145 | `	 * expansion (iP2 = hasSpread, transferred from the popped OP_CALL —` |
|       - |  146 | ``	 * `new C(...$args)` used to ignore the extras, leaving the expanded`` |
|       - |  147 | `	 * elements ABOVE the class-name slot and fataling "Class ' ' is not` |
|       - |  148 | `	 * defined"). VmSpreadOwnExtra counts only this new's own runs, so a nested` |
|       - |  149 | `	 * spread call in the ctor arg list stays scoped to itself. */` |
| 1105197 |  150 | `	sxi32 nCtorArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|       - |  151 | `	ph7_value *pArg;` |
| 1105197 |  152 | `	ph7_class *pClass = 0;` |
|       - |  153 | `	ph7_class_instance *pNew;` |
| 1105197 |  154 | `	pArg = &pTos[-nCtorArgs]; /* Constructor arguments (if available) */` |
|       - |  155 | `	/* Same PHP 8.1 spread-key realignment as OP_CALL: build a per-actual-slot name` |
|       - |  156 | `	 * map when the ctor arg list unpacked string-keyed elements. NEW keeps the` |
|       - |  157 | `	 * class-name slot at pTos (above the args), so pArg is already the correct base;` |
|       - |  158 | `	 * the build also truncates this call's captured runs. */` |
|       - |  159 | `	VmCallArgMap sEffNewMap;` |
| 1657793 |  160 | `	VmCallArgMap *pEffNewMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
| 1105192 |  161 | `		nCtorArgs > 0 ? (sxu32)nCtorArgs : 0,&sEffNewMap);` |
| 1657793 |  162 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
| 1105197 |  163 | `		const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
| 1105197 |  164 | `		sxu32 nCls = SyBlobLength(&pTos->sBlob);` |
| 1105192 |  165 | `		if( (nCls == sizeof("self")-1 && SyMemcmp(zCls,"self",sizeof("self")-1) == 0)` |
| 1105185 |  166 | `		 \|\| (nCls == sizeof("static")-1 && SyMemcmp(zCls,"static",sizeof("static")-1) == 0)` |
| 1105153 |  167 | `		 \|\| (nCls == sizeof("parent")-1 && SyMemcmp(zCls,"parent",sizeof("parent")-1) == 0) ){` |
|       - |  168 | `			/* new self() / new static() / new parent(): resolve against the live` |
|       - |  169 | `			 * class context (LSB for static), sharing the FCC resolver. */` |
|      47 |  170 | `			pClass = VmFccResolveScope(&(*pVm),pTos);` |
|      47 |  171 | `			if( pClass && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) ){` |
|       - |  172 | `				/* Not new-able (the named path's iLoadable extract excludes these` |
|       - |  173 | `				 * before it ever gets here): php's wording, with the RESOLVED name. */` |
|     ! 0 |  174 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot instantiate %s %z",` |
|     ! 0 |  175 | `					(pClass->iFlags & PH7_CLASS_INTERFACE) ? "interface" : "abstract class",` |
|     ! 0 |  176 | `					&pClass->sName);` |
|     ! 0 |  177 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 |  178 | `				if( nCtorArgs > 0 ){` |
|     ! 0 |  179 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  180 | `				}` |
|     ! 0 |  181 | `				VM_EXIT_ABORT;` |
|       - |  182 | `			}` |
|      24 |  183 | `		}else{` |
|       - |  184 | `			/* Try to extract the desired class */` |
| 1105151 |  185 | `			pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,` |
|       - |  186 | `				TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|       5 |  187 | `		}` |
|  552596 |  188 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - |  189 | `		/* Take the base class from the loaded instance */` |
|     ! 0 |  190 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|     ! 0 |  191 | `	}` |
| 1105197 |  192 | `	if( pClass == 0 ){` |
|       - |  193 | ``		/* php: `new NoSuch()` is a CATCHABLE Error ('Class "NoSuch" not found'), not a`` |
|       - |  194 | `		 * hard stop. Aborting here killed the script outright -- and with exit status 0,` |
|       - |  195 | `		 * so a caller could not even tell it had failed. */` |
|       - |  196 | `		SyBlob sErrM;` |
|       - |  197 | `		sxi32 rcErr;` |
|      13 |  198 | `		ph7_class *pNotNew = 0;` |
|      13 |  199 | `		SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      13 |  200 | `		if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       - |  201 | `			/* The extract above only accepts NEW-able classes, so an interface or an` |
|       - |  202 | `			 * abstract class comes back as 0 and used to be reported as "not found".` |
|       - |  203 | `			 * Look again without that filter so php's real message can be given. */` |
|      18 |  204 | `			pNotNew = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      10 |  205 | `				SyBlobLength(&pTos->sBlob),FALSE,0);` |
|       5 |  206 | `		}` |
|      13 |  207 | `		if( pNotNew && (pNotNew->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) ){` |
|       5 |  208 | `			SyBlobFormat(&sErrM,"Cannot instantiate %s %z",` |
|       4 |  209 | `				(pNotNew->iFlags & PH7_CLASS_INTERFACE) ? "interface" : "abstract class",` |
|       2 |  210 | `				&pNotNew->sName);` |
|       3 |  211 | `		}else{` |
|       9 |  212 | `			SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|       6 |  213 | `				SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob));` |
|       - |  214 | `		}` |
|       - |  215 | `		/* Settle the stack BEFORE throwing: the class-name slot becomes the (NULL)` |
|       - |  216 | `		 * expression result and the ctor arguments go. */` |
|      13 |  217 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  218 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  219 | `		}` |
|      13 |  220 | `		PH7_MemObjRelease(pTos);` |
|      13 |  221 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|      13 |  222 | `		pTos->nIdx = SXU32_HIGH;` |
|      18 |  223 | `		rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       5 |  224 | `			SyBlobLength(&sErrM));` |
|      13 |  225 | `		SyBlobRelease(&sErrM);` |
|      13 |  226 | `		if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      13 |  227 | `		rc = rcErr;` |
|      17 |  228 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
| 1105187 |  229 | `	}else if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|       - |  230 | `		/* php 8.1: enums cannot be instantiated — a catchable Error, raised` |
|       - |  231 | `		 * BEFORE any construction (no instance, no __destruct). */` |
|       - |  232 | `		SyBlob sErrMsg;` |
|       3 |  233 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 |  234 | `		SyBlobFormat(&sErrMsg,"Cannot instantiate enum %z",&pClass->sName);` |
|       3 |  235 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       3 |  236 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  237 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  238 | `		}` |
|       3 |  239 | `		PH7_MemObjRelease(pTos);` |
|       3 |  240 | `		pTos->nIdx = SXU32_HIGH;` |
|       3 |  241 | `		VM_EXIT_BREAK;` |
| 1105180 |  242 | `	}else if( (pClass->iFlags & PH7_CLASS_STATIC_TYPE_DEFER)` |
|  552596 |  243 | `	       && (rc = VmThrowDeferredStaticType(&(*pVm),pClass)) != SXRET_OK ){` |
|       - |  244 | `		/* Deferred typed-static-default failure: php also materializes the` |
|       - |  245 | ``		 * static table at instantiation, so `new C` throws the catchable`` |
|       - |  246 | `		 * TypeError BEFORE any construction (no instance, no __destruct) —` |
|       - |  247 | `		 * same shape as the enum reject above: park the status, settle the` |
|       - |  248 | `		 * stack, and let the fetch-point router land it. */` |
|       3 |  249 | `		VmBoundaryPark(&(*pVm),rc);` |
|       3 |  250 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  251 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  252 | `		}` |
|       3 |  253 | `		PH7_MemObjRelease(pTos);` |
|       3 |  254 | `		pTos->nIdx = SXU32_HIGH;` |
|       3 |  255 | `		VM_EXIT_BREAK;` |
|     ! 0 |  256 | `	}else{` |
|       - |  257 | `		ph7_class_method *pCons;` |
|       - |  258 | `		/* Check if a constructor is available — BEFORE instantiation: a` |
|       - |  259 | ``		 * visibility-denied `new` must not construct (nor later destruct)`` |
|       - |  260 | `		 * the object (band A #4). */` |
|       - |  261 | `		/* Only an explicit __construct is the constructor. PHP-4-style class-name` |
|       - |  262 | `		 * constructors were removed in PHP 8.0 — a method named like the class is a` |
|       - |  263 | `		 * plain method, so no same-name fallback here. */` |
| 1105183 |  264 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       - |  265 | `		/* Constructor visibility (band A #4): __construct now KEEPS its` |
|       - |  266 | `		 * declared protection (PH7_NewClassMethod no longer forces it` |
|       - |  267 | ``		 * public), so `new C()` on a private/protected constructor from`` |
|       - |  268 | `		 * the wrong scope is php's catchable Error. Reflection's` |
|       - |  269 | `		 * newInstance path sets bReflectBypass like method invoke. */` |
| 1105183 |  270 | `		if( pCons && pCons->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - |  271 | `			/* php binds non-public access by the member's DECLARING class, not the` |
|       - |  272 | `			 * instantiated class: a private constructor declared in a base is` |
|       - |  273 | `			 * reachable from that base's own methods even when instantiating a` |
|       - |  274 | ``			 * subclass (`new Child()` inside Base::factory()). __construct is a`` |
|       - |  275 | `			 * method, so pass its declaring class (sFunc.pUserData) — mirroring the` |
|       - |  276 | `			 * method-call visibility check — rather than pClass, whose hAttr lookup` |
|       - |  277 | `			 * for a method name always misses and falls to a wrong exact-class test. */` |
|      23 |  278 | `			ph7_class *pCtorDecl = pCons->sFunc.pUserData ? (ph7_class *)pCons->sFunc.pUserData : pClass;` |
|      23 |  279 | `			if( pVm->bReflectBypass ){` |
|     ! 0 |  280 | `				pVm->bReflectBypass = 0;` |
|      23 |  281 | `			}else if( !PH7_VmClassMemberAccess(&(*pVm),pCtorDecl,&pCons->sFunc.sName,pCons->iProtection,FALSE) ){` |
|       - |  282 | `				SyBlob sErrMsg;` |
|       7 |  283 | `				const char *zVis = pCons->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       7 |  284 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       7 |  285 | `				SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from global scope",` |
|       3 |  286 | `					zVis,&pClass->sName);` |
|       7 |  287 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       - |  288 | `				/* Pop ctor args + release the class-name operand, leave NULL` |
|       - |  289 | `				 * as the expression value; the fetch-point router lands the` |
|       - |  290 | `				 * throw. No instance was created. */` |
|       7 |  291 | `				if( nCtorArgs > 0 ){` |
|     ! 0 |  292 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  293 | `				}` |
|       7 |  294 | `				PH7_MemObjRelease(pTos);` |
|       7 |  295 | `				pTos->nIdx = SXU32_HIGH;` |
|       7 |  296 | `				VM_EXIT_BREAK;` |
|       - |  297 | `			}` |
|       8 |  298 | `		}` |
|       - |  299 | `		/* Create a new class instance */` |
| 1105177 |  300 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
| 1105177 |  301 | `		if( pNew == 0 ){` |
|     ! 0 |  302 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  303 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|     ! 0 |  304 | `				&pClass->sName` |
|       - |  305 | `			);` |
|     ! 0 |  306 | `			PH7_MemObjRelease(pTos);` |
|     ! 0 |  307 | `			if( nCtorArgs > 0 ){` |
|       - |  308 | `				/* Pop given arguments */` |
|     ! 0 |  309 | `				VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  310 | `			}` |
|     ! 0 |  311 | `			VM_EXIT_BREAK;` |
|       - |  312 | `		}` |
| 1105177 |  313 | `		if( pVm->nBoundaryRc != 0 ){` |
|       - |  314 | `			/* A typed property DEFAULT failed its type check during instance-` |
|       - |  315 | `			 * frame creation (parked catchable TypeError): php aborts the` |
|       - |  316 | `			 * construction — no constructor call, no object. Consume the` |
|       - |  317 | `			 * parked status here and route exactly like a constructor throw` |
|       - |  318 | `			 * (the exception object is already registered). */` |
|      13 |  319 | `			sxi32 rcDef = pVm->nBoundaryRc;` |
|       - |  320 | `			sxi32 iDefResumePc;` |
|      13 |  321 | `			pVm->nBoundaryRc = 0;` |
|      13 |  322 | `			PH7_ClassInstanceUnref(pNew);` |
|      13 |  323 | `			if( rcDef == PH7_ABORT ){` |
|     ! 0 |  324 | `				VM_EXIT_ABORT;` |
|       - |  325 | `			}` |
|      13 |  326 | `			if( VmRecordedResume(pVm,&iDefResumePc,pState->pEntryFrame,aInstr) ){` |
|       - |  327 | `				/* This frame's own try caught it in-place: tidy the stack` |
|       - |  328 | `				 * (pop ctor args + release the class-name slot, then drain any` |
|       - |  329 | `				 * abandoned outer-expression operands to the try's base — the` |
|       - |  330 | `				 * class-name slot itself sits above it) and resume. */` |
|      13 |  331 | `				if( nCtorArgs > 0 ){` |
|     ! 0 |  332 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  333 | `				}` |
|      13 |  334 | `				PH7_MemObjRelease(pTos);` |
|      25 |  335 | `				PH7_RESUME_DRAIN()` |
|      13 |  336 | `				pc = iDefResumePc;` |
|      13 |  337 | `				VM_EXIT_BREAK;` |
|       - |  338 | `			}` |
|     ! 0 |  339 | `			VM_EXIT_EXCEPTION;` |
|       - |  340 | `		}` |
| 1105165 |  341 | `		if( pCons ){` |
|       - |  342 | `			/* Call the class constructor.  Collect args in stack order and` |
|       - |  343 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|       - |  344 | `			 * receiving OP_CALL path runs its named-argument matching` |
|       - |  345 | `			 * (including variadic string-key packing). */` |
| 1103067 |  346 | `			VmCallArgMap *pNewMap = pEffNewMap;` |
|       - |  347 | `			sxi32 rcCons;` |
|       - |  348 | `			SySet aArg; /* ctor argument scratch (the loop's shared set stays with OP_CALL) */` |
| 1103067 |  349 | `			SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
| 2107029 |  350 | `			while( pArg < pTos ){` |
| 1003967 |  351 | `				SySetPut(&aArg,(const void *)&pArg);` |
| 1003967 |  352 | `				pArg++;` |
|       5 |  353 | `			}` |
|       - |  354 | `			/* Too-few-arguments is php's catchable ArgumentCountError, raised` |
|       - |  355 | `			 * by the shared OP_CALL install path this ctor call routes through` |
|       - |  356 | `			 * (was a PHL-only notice here). */` |
| 1103067 |  357 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
| 1103067 |  358 | `			SySetRelease(&aArg); /* scratch dies here, on every exit path */` |
|       - |  359 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
| 1103067 |  360 | `			if( pNew->iRef < 1 ){` |
|     ! 0 |  361 | `				pNew->iRef = 1;` |
|     ! 0 |  362 | `			}` |
| 1103067 |  363 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|       - |  364 | `				/* The constructor raised: the half-constructed object must not` |
|       - |  365 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|       - |  366 | `				 * The class-name operand (and any leftover args) are released by` |
|       - |  367 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|       - |  368 | `				sxi32 iResumePc;` |
|  100107 |  369 | `				PH7_ClassInstanceUnref(pNew);` |
|  100107 |  370 | `				if( rcCons == PH7_ABORT ){` |
|       5 |  371 | `					VM_EXIT_ABORT;` |
|       - |  372 | `				}` |
|  100102 |  373 | `				if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){` |
|       - |  374 | `					/* This frame's own try caught it in-place: tidy the stack` |
|       - |  375 | `					 * (pop ctor args + release the class-name slot, then drain any` |
|       - |  376 | `					 * abandoned outer-expression operands to the try's base — the` |
|       - |  377 | `					 * class-name slot itself sits above it) and resume. */` |
|  100100 |  378 | `					if( nCtorArgs > 0 ){` |
|      91 |  379 | `						VmPopOperand(&pTos,nCtorArgs);` |
|      45 |  380 | `					}` |
|  100100 |  381 | `					PH7_MemObjRelease(pTos);` |
|  500198 |  382 | `					PH7_RESUME_DRAIN()` |
|  100100 |  383 | `					pc = iResumePc;` |
|  100100 |  384 | `					VM_EXIT_BREAK;` |
|       - |  385 | `				}` |
|       3 |  386 | `				VM_EXIT_EXCEPTION;` |
|       - |  387 | `			}` |
|  501479 |  388 | `		}` |
| 1005061 |  389 | `		if( nCtorArgs > 0 ){` |
|       - |  390 | `			/* Pop given arguments */` |
| 1002749 |  391 | `			VmPopOperand(&pTos,nCtorArgs);` |
|  501372 |  392 | `		}` |
| 1005061 |  393 | `		PH7_MemObjRelease(pTos);` |
| 1005061 |  394 | `		pTos->x.pOther = pNew;` |
| 1005061 |  395 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - |  396 | `	}` |
| 1005061 |  397 | `	VM_EXIT_BREAK;` |
|     ! 0 |  398 | `	VM_EXIT_BREAK;` |
|  552601 |  399 | `}` |
|       - |  400 |  |
|       - |  401 | `/*` |
|       - |  402 | ` * OP_MEMBER: body moved verbatim from the OP_MEMBER arm of` |
|       - |  403 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  404 | ` */` |
| 3236444 |  405 | `PH7_PRIVATE VmOpRc VmExecOpMember(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  406 | `{` |
| 3236449 |  407 | `	ph7_value *pTos = pState->pTos;` |
| 3236449 |  408 | `	ph7_value *pStack = pState->pStack;` |
| 3236449 |  409 | `	VmInstr *aInstr = pState->aInstr;` |
| 3236449 |  410 | `	sxi32 pc = pState->pc;` |
|       - |  411 | `	sxi32 rc;` |
|       - |  412 | `	ph7_class_instance *pThis;` |
|       - |  413 | `	ph7_value *pNos;` |
|       - |  414 | `	SyString sName;` |
| 3236449 |  415 | `	if( !pInstr->iP1 ){` |
| 3133707 |  416 | `		pNos = &pTos[-1];` |
|       - |  417 | `#ifdef UNTRUST` |
|       - |  418 | `		if( pNos < pStack ){` |
|       - |  419 | `			VM_EXIT_ABORT;` |
|       - |  420 | `		}` |
|       - |  421 | `#endif` |
| 3133702 |  422 | `		if( pInstr->iP2 == PH7_MEMBER_DEFPATH` |
| 1571054 |  423 | `		 && (pNos->iFlags & (MEMOBJ_AUX_DEFPATH\|MEMOBJ_AUX_DEFERRED)) ){` |
|       - |  424 | `			/* D1 commit 2: deferred property arg ($o->p) whose base is itself a deferred lvalue` |
|       - |  425 | ``			 * (a nested chain `$a["k"]->p`, or an undefined base object). Append a property step`` |
|       - |  426 | `			 * to the base's captured path; OP_CALL re-walks it. */` |
|       - |  427 | `			SyString sProp;` |
|      17 |  428 | `			VmDeferredPath *pPath = 0;` |
|      17 |  429 | `			SyStringInitFromBuf(&sProp,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      17 |  430 | `			if( pNos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|      17 |  431 | `				pPath = (VmDeferredPath *)pNos->x.pOther;` |
|      17 |  432 | `				VmDeferPathPushProp(pPath,&sProp);` |
|       9 |  433 | `			}else{` |
|       - |  434 | `				SyString sRootName;` |
|     ! 0 |  435 | `				SyStringInitFromBuf(&sRootName,(const char *)pNos->x.pOther,` |
|       - |  436 | `					pNos->x.pOther ? SyStrlen((const char *)pNos->x.pOther) : 0);` |
|     ! 0 |  437 | `				pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|     ! 0 |  438 | `				if( pPath ){` |
|     ! 0 |  439 | `					pNos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name */` |
|     ! 0 |  440 | `					pNos->x.pOther = pPath;` |
|     ! 0 |  441 | `					pNos->iFlags \|= MEMOBJ_AUX_DEFPATH;` |
|     ! 0 |  442 | `					pNos->nIdx = SXU32_HIGH;` |
|     ! 0 |  443 | `					VmDeferPathPushProp(pPath,&sProp);` |
|     ! 0 |  444 | `				}` |
|       - |  445 | `			}` |
|      17 |  446 | `			VmPopOperand(&pTos,1); /* drop the property name; the carrier stays as pTos */` |
|      17 |  447 | `			VM_EXIT_BREAK;` |
|       - |  448 | `		}` |
| 3133691 |  449 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - |  450 | `			ph7_class *pClass;` |
|       - |  451 | `			/* Class already instantiated */` |
| 3133681 |  452 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       - |  453 | `			/* Point to the instantiated class */` |
| 3133681 |  454 | `			pClass = pThis->pClass;` |
|       - |  455 | `			/* Extract attribute name first */` |
| 3133681 |  456 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
| 3133681 |  457 | `			if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - |  458 | `				/* Method call */` |
|  112019 |  459 | `				ph7_class_method *pMeth = 0;` |
|  112019 |  460 | `				if( sName.nByte > 0 ){` |
|       - |  461 | `					/* Extract the target method */` |
|  112019 |  462 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|   56007 |  463 | `				}` |
|  112019 |  464 | `				if( pMeth == 0 ){` |
|       9 |  465 | `					ph7_class_method *pCallMagic = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|       9 |  466 | `					if( pCallMagic ){` |
|       - |  467 | `						/* php: a missing method dispatches __call($name, $args) — the` |
|       - |  468 | `						 * args are only collected by the following OP_CALL, so stash the` |
|       - |  469 | `						 * receiver + class + original name and redirect the callee to` |
|       - |  470 | `						 * the hidden packing trampoline (band A #3b; pre-fix the name-` |
|       - |  471 | `						 * only call discarded everything and the call site failed with` |
|       - |  472 | `						 * "Invalid function name"). Stack: pop the method name, then` |
|       - |  473 | `						 * the receiver slot becomes the trampoline's callee name. */` |
|       7 |  474 | `						SyBlobReset(&pVm->sMagicCallName);` |
|       7 |  475 | `						SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);` |
|       7 |  476 | `						pThis->iRef++;` |
|       7 |  477 | `						pVm->pMagicCallThis = pThis;` |
|       7 |  478 | `						pVm->pMagicCallClass = pClass;` |
|       7 |  479 | `						VmPopOperand(&pTos,1);` |
|       7 |  480 | `						PH7_MemObjRelease(pTos);` |
|       7 |  481 | `						SyBlobAppend(&pTos->sBlob,"__phl_magic_call",sizeof("__phl_magic_call")-1);` |
|       7 |  482 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|       4 |  483 | `					}else{` |
|       - |  484 | `						{` |
|       - |  485 | `							/* php raises a catchable Error here; PH7 printed a notice and CARRIED ON` |
|       - |  486 | `							 * with NULL, so the call silently produced nothing. */` |
|       - |  487 | `							SyBlob sErrM;` |
|       - |  488 | `							sxi32 rcErr;` |
|       3 |  489 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       3 |  490 | `							SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",&pClass->sName,&sName);` |
|       3 |  491 | `							VmPopOperand(&pTos,1);` |
|       3 |  492 | `							PH7_MemObjRelease(pTos);` |
|       3 |  493 | `							pTos->nIdx = SXU32_HIGH;` |
|       4 |  494 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       1 |  495 | `								SyBlobLength(&sErrM));` |
|       3 |  496 | `							SyBlobRelease(&sErrM);` |
|       3 |  497 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  498 | `							rc = rcErr;` |
|       3 |  499 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  500 | `						}` |
|       - |  501 | `					}` |
|       4 |  502 | `				}else{` |
|  112011 |  503 | `					ph7_class_method *pDeniedCall = 0;` |
|  112006 |  504 | `					if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC` |
|   58562 |  505 | `					 && !PH7_VmClassMemberAccess(&(*pVm),` |
|    2554 |  506 | `						pMeth->sFunc.pUserData ? (ph7_class *)pMeth->sFunc.pUserData : pClass,` |
|    1277 |  507 | `						&sName,pMeth->iProtection,FALSE) ){` |
|      18 |  508 | `						int bRebound = 0;` |
|      18 |  509 | `						if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - |  510 | `							/* php: when the instance's class SHADOWS the caller's` |
|       - |  511 | `							 * private method (child redeclares private m), code in` |
|       - |  512 | `							 * the caller's class dispatches its OWN private m, not` |
|       - |  513 | `							 * the shadow. Rebind to the calling scope's method when` |
|       - |  514 | `							 * that scope declares a private one of this name. */` |
|      14 |  515 | `							VmFrame *pFrameLocal = VmSkipExceptionFrames(pVm->pFrame);` |
|      14 |  516 | `							ph7_vm_func *pCallerFunc = (ph7_vm_func *)pFrameLocal->pUserData;` |
|      14 |  517 | `							if( pCallerFunc && (pCallerFunc->iFlags & VM_FUNC_CLASS_METHOD) && pCallerFunc->pUserData ){` |
|       3 |  518 | `								ph7_class *pScope = (ph7_class *)pCallerFunc->pUserData;` |
|       3 |  519 | `								ph7_class_method *pOwn = PH7_ClassExtractMethod(pScope,sName.zString,sName.nByte);` |
|       2 |  520 | `								if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       3 |  521 | `								 && (ph7_class *)pOwn->sFunc.pUserData == pScope ){` |
|       3 |  522 | `									pMeth = pOwn;` |
|       3 |  523 | `									bRebound = 1;` |
|       1 |  524 | `								}` |
|       1 |  525 | `							}` |
|       6 |  526 | `						}` |
|      18 |  527 | `						if( !bRebound ){` |
|       - |  528 | `							/* Inaccessible from this scope: php routes through __call when` |
|       - |  529 | `							 * declared (band A #3b); without it OP_CALL raises its` |
|       - |  530 | `							 * "Call to private/protected method" Error as before. */` |
|      16 |  531 | `							pDeniedCall = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|       7 |  532 | `						}` |
|       8 |  533 | `					}` |
|  112011 |  534 | `					if( pDeniedCall ){` |
|       3 |  535 | `						SyBlobReset(&pVm->sMagicCallName);` |
|       3 |  536 | `						SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);` |
|       3 |  537 | `						pThis->iRef++;` |
|       3 |  538 | `						pVm->pMagicCallThis = pThis;` |
|       3 |  539 | `						pVm->pMagicCallClass = pClass;` |
|       3 |  540 | `						VmPopOperand(&pTos,1);` |
|       3 |  541 | `						PH7_MemObjRelease(pTos);` |
|       3 |  542 | `						SyBlobAppend(&pTos->sBlob,"__phl_magic_call",sizeof("__phl_magic_call")-1);` |
|       3 |  543 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|       2 |  544 | `					}else{` |
|       - |  545 | `						/* Push method name on the stack */` |
|  112009 |  546 | `						PH7_MemObjRelease(pTos);` |
|  112009 |  547 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|  112009 |  548 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|       - |  549 | `					}` |
|       - |  550 | `				}` |
|  112017 |  551 | `				pTos->nIdx = SXU32_HIGH;` |
|   56011 |  552 | `			}else{` |
|       - |  553 | `				/* Attribute access. iP2: 0 = read, 2 = unset, 3 = isset, 4 = empty. */` |
| 3021667 |  554 | `				VmClassAttr *pObjAttr = 0;` |
| 3021667 |  555 | `				SyHashEntry *pEntry = 0;` |
|       - |  556 | `				/* Extract the target attribute */` |
| 3021667 |  557 | `				if( sName.nByte > 0 ){` |
| 3021667 |  558 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
| 3021667 |  559 | `					if( pEntry ){` |
|       - |  560 | `						/* Point to the attribute value */` |
| 3021197 |  561 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
| 1510596 |  562 | `					}` |
| 1510831 |  563 | `				}` |
| 3021667 |  564 | `				if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - |  565 | `					/* unset($o->prop): remove the property entirely so it disappears from` |
|       - |  566 | `					 * foreach / json_encode / get_object_vars / (array) — matching PHP (a value-only` |
|       - |  567 | `					 * release would leave a zombie null entry). Leave a NULL constant on the stack so` |
|       - |  568 | `					 * the trailing generic unset() builtin is a no-op (mirrors LOAD_IDX iP2=5).` |
|       - |  569 | `					 * php dispatches __unset($name) for a MISSING or INACCESSIBLE property` |
|       - |  570 | `					 * (band A #3b — pre-fix an inaccessible private was silently DELETED from` |
|       - |  571 | `					 * outside the class); without __unset, an inaccessible unset is php's` |
|       - |  572 | `					 * catchable "Cannot access ..." Error and a missing one stays a no-op. */` |
|      39 |  573 | `					int bUnsAccessible = pEntry ? PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) : 0;` |
|      41 |  574 | `					if( pEntry && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) != 0 ){` |
|       - |  575 | `						/* php 8.4: a hooked property (virtual or backed) can never be` |
|       - |  576 | `						 * unset — catchable Error, even from inside its own hook body` |
|       - |  577 | `						 * (probe-verified). */` |
|       - |  578 | `						SyBlob sErrMsg;` |
|       5 |  579 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       5 |  580 | `						SyBlobFormat(&sErrMsg,"Cannot unset hooked property %z::$%z",` |
|       4 |  581 | `							&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       5 |  582 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      37 |  583 | `					}else if( pEntry && bUnsAccessible ){` |
|      27 |  584 | `						PH7_VmReleaseInstanceAttr(&(*pVm),pObjAttr);` |
|      27 |  585 | `						SyHashDeleteEntry2(pEntry);` |
|      14 |  586 | `					}else{` |
|       9 |  587 | `						ph7_class_method *pUnsetMagic = PH7_ClassExtractMethod(pClass,"__unset",sizeof("__unset")-1);` |
|       9 |  588 | `						if( pUnsetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'u') ){` |
|       7 |  589 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'u');` |
|       7 |  590 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__unset",sizeof("__unset")-1,&sName,0);` |
|       7 |  591 | `							VmMagicGuardPop(pVm);` |
|       6 |  592 | `						}else if( pEntry ){` |
|       - |  593 | `							/* Inaccessible and no __unset: php's catchable Error. Parked on` |
|       - |  594 | `							 * the boundary rail; the op completes benignly and the` |
|       - |  595 | `							 * fetch-point router lands it. */` |
|       - |  596 | `							SyBlob sErrMsg;` |
|       3 |  597 | `							const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       3 |  598 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 |  599 | `							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",zVis,&pClass->sName,&sName);` |
|       3 |  600 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       1 |  601 | `						}` |
|       - |  602 | `						/* Missing property without __unset: silent no-op (php). */` |
|       - |  603 | `					}` |
|      39 |  604 | `					VmPopOperand(&pTos,1);    /* pop the attribute name */` |
|      39 |  605 | `					PH7_MemObjRelease(pTos);  /* release the object on the stack ($o's stack ref) */` |
|      39 |  606 | `					pTos->nIdx = SXU32_HIGH;  /* NULL constant */` |
|     297 |  607 | `					VM_EXIT_BREAK;` |
|       - |  608 | `				}` |
| 3021629 |  609 | `				if( pObjAttr == 0 && sName.nByte > 0 ){` |
|       - |  610 | `					/* Member not present on the instance and the next instruction writes/modifies it` |
|       - |  611 | ``					 * (store, array-append/keyed-write, `??=`, ++/--, or a compound-assign — see`` |
|       - |  612 | `					 * VmMemberNextIsWrite; the compiler always emits a terminating PH7_OP_DONE so` |
|       - |  613 | `					 * pInstr+1 is in-bounds). Create the property so the operation lands on a real slot` |
|       - |  614 | ``					 * (PHP auto-vivifies a fresh property for all these forms, not just `=`):`` |
|       - |  615 | `					 *   - a DECLARED prop that was unset() and is re-assigned → recreate it` |
|       - |  616 | `					 *     (PHP re-appends it at the end), OR` |
|       - |  617 | `					 *   - a dynamic prop on a dynamic-allowing class (stdClass).` |
|       - |  618 | `					 * The created slot is NULL with a real nIdx, which the modify-op then vivifies` |
|       - |  619 | `					 * (NULL→array for [], NULL→0/"" for ++/.=) and writes back in place.` |
|       - |  620 | `					 * Two signals: the compiler tags the member itself iP2=PH7_MEMBER_WRITE when it is` |
|       - |  621 | ``					 * the base of a write-subscript / `??=` (the modify-op is not the immediately-next`` |
|       - |  622 | `					 * instruction there), and for ++/--/compound-assign/store the next opcode is the` |
|       - |  623 | `					 * modify-op directly (VmMemberNextIsWrite). */` |
|     470 |  624 | `					VmInstr *pNext = pInstr + 1;` |
|     466 |  625 | `					if( pInstr->iP2 == PH7_MEMBER_WRITE \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|     383 |  626 | `					 \|\| VmMemberNextIsWrite(pNext) ){` |
|      92 |  627 | `						ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pThis->pClass,sName.zString,sName.nByte);` |
|      92 |  628 | `						if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       7 |  629 | `							VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pObjAttr);` |
|       4 |  630 | `						}else{` |
|       - |  631 | `							/* php 8 semantics (band A #3b): a PLAIN store to a missing` |
|       - |  632 | `							 * property dispatches __set($name,$value) when declared —` |
|       - |  633 | `							 * the value only exists at the following OP_STORE, so park` |
|       - |  634 | `							 * the receiver+name for it (one-instruction lifetime; the` |
|       - |  635 | `							 * guard makes a same-name write inside __set fall through` |
|       - |  636 | `							 * to dynamic creation, like php). Without __set — or for a` |
|       - |  637 | `							 * subscript-write base / read-modify-write, which php does` |
|       - |  638 | `							 * NOT route through __set — create a dynamic property on` |
|       - |  639 | `							 * ANY class with the 8.2 deprecation (suppressed for` |
|       - |  640 | `							 * stdClass and #[AllowDynamicProperties]); a readonly` |
|       - |  641 | `							 * class raises php's catchable Error instead. */` |
|      86 |  642 | `							ph7_class_method *pSetMagic = 0;` |
|      86 |  643 | `							ph7_class_method *pCoalIsset = 0, *pCoalGet = 0, *pCoalSet = 0;` |
|      86 |  644 | `							int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);` |
|      86 |  645 | `							if( bPlainStore \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|      68 |  646 | `								pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|      32 |  647 | `							}` |
|      86 |  648 | `							if( pSetMagic && pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - |  649 | `								/* php dispatches __set($name, element) for a missing` |
|       - |  650 | `								 * destructuring target, but the element value only exists` |
|       - |  651 | `								 * at the following OP_LOAD_LIST — the dispatch is` |
|       - |  652 | `								 * unsupported (recorded residual). Leave the miss: the` |
|       - |  653 | `								 * property stays uncreated, matching php's observable` |
|       - |  654 | `								 * state (its __set did not store either). */` |
|      86 |  655 | `							}else if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){` |
|       9 |  656 | `								pThis->iRef++;` |
|       9 |  657 | `								pVm->pMagicSetThis = pThis;` |
|       9 |  658 | `								SyBlobReset(&pVm->sMagicSetName);` |
|       9 |  659 | `								SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       - |  660 | `								/* pObjAttr stays NULL; the miss path below stays silent. */` |
|      79 |  661 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|      19 |  662 | `							 && pNext->iOp == PH7_OP_NULLC_JMP` |
|      18 |  663 | `							 && ((pCoalIsset = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1)) != 0` |
|       6 |  664 | `							  \|\| (pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)) != 0` |
|       3 |  665 | `							  \|\| (pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1)) != 0) ){` |
|       - |  666 | ``								/* `$o->p ??= v` on a missing property with magic accessors:`` |
|       - |  667 | `								 * php consults __isset first when declared — false means` |
|       - |  668 | `								 * assign directly through __set with NO __get call (the` |
|       - |  669 | `								 * test value stays null); true (or no __isset) means __get` |
|       - |  670 | `								 * provides the test value. A COAL_MAGIC entry is pushed` |
|       - |  671 | `								 * for the OP_NULLC_STORE at the jump target; the` |
|       - |  672 | `								 * fetch-point sweep drops it when the short-circuit jump` |
|       - |  673 | `								 * skips the assign or a throw abandons the RHS. Without` |
|       - |  674 | `								 * __set the assign side keeps the pre-existing loud` |
|       - |  675 | `								 * "Cannot perform assignment" path (php would create a` |
|       - |  676 | `								 * dynamic property there — recorded residual). Note the` |
|       - |  677 | `								 * condition's short-circuit assignment chain: a hit on an` |
|       - |  678 | `								 * earlier method leaves the later pointers unresolved, so` |
|       - |  679 | `								 * re-resolve the leftovers here (each is one hash probe,` |
|       - |  680 | `								 * only on this ??=-miss path). */` |
|       - |  681 | `								ph7_value sTest;` |
|       7 |  682 | `								int bMiss = 0; /* __isset said false: skip __get, test value stays null */` |
|       7 |  683 | `								if( pCoalIsset != 0 ){` |
|       5 |  684 | `									pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       2 |  685 | `								}` |
|       7 |  686 | `								if( pCoalIsset != 0 \|\| pCoalGet != 0 ){` |
|       7 |  687 | `									pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|       3 |  688 | `								}` |
|       7 |  689 | `								PH7_MemObjInit(pVm,&sTest);` |
|       7 |  690 | `								if( pCoalIsset && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - |  691 | `									ph7_value sIssetRet;` |
|       5 |  692 | `									PH7_MemObjInit(pVm,&sIssetRet);` |
|       5 |  693 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|       5 |  694 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|       5 |  695 | `									VmMagicGuardPop(pVm);` |
|       5 |  696 | `									PH7_MemObjToBool(&sIssetRet);` |
|       5 |  697 | `									bMiss = sIssetRet.x.iVal == 0;` |
|       5 |  698 | `									PH7_MemObjRelease(&sIssetRet);` |
|       2 |  699 | `								}` |
|       7 |  700 | `								if( !bMiss && pCoalGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       5 |  701 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       5 |  702 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sTest);` |
|       5 |  703 | `									VmMagicGuardPop(pVm);` |
|       2 |  704 | `								}` |
|       6 |  705 | `								if( pCoalSet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s')` |
|       7 |  706 | `								 && pVm->nBoundaryRc == 0 ){` |
|       - |  707 | `									VmHookRmw sPend;` |
|       7 |  708 | `									sPend.iKind = VM_HOOK_PEND_COAL_MAGIC;` |
|       7 |  709 | `									sPend.pThis = pThis;` |
|       7 |  710 | `									sPend.pAttr = 0;` |
|       7 |  711 | `									sPend.nBackIdx = SXU32_HIGH;` |
|       7 |  712 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|       7 |  713 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|       7 |  714 | `									SyBlobAppend(&sPend.sName,(const void *)sName.zString,sName.nByte);` |
|       7 |  715 | `									sPend.pOwnerStack = (void *)pStack;` |
|       7 |  716 | `									sPend.pInstrs = (void *)aInstr;` |
|       7 |  717 | `									sPend.nJmpPc = (sxu32)(pc + 1);        /* the OP_NULLC_JMP */` |
|       7 |  718 | `									sPend.nPc = (sxu32)(pNext->iP2 - 1);   /* the OP_NULLC_STORE */` |
|       7 |  719 | `									pThis->iRef++;` |
|       7 |  720 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|       3 |  721 | `								}` |
|       - |  722 | `								/* Pop the attribute name; the test value becomes the` |
|       - |  723 | `								 * expression slot (a temp, not an lvalue). */` |
|       7 |  724 | `								VmPopOperand(&pTos,1);` |
|       7 |  725 | `								pThis->iRef++;` |
|       7 |  726 | `								PH7_MemObjRelease(pTos);` |
|       7 |  727 | `								PH7_MemObjStore(&sTest,pTos);` |
|       7 |  728 | `								pTos->nIdx = SXU32_HIGH;` |
|       7 |  729 | `								PH7_MemObjRelease(&sTest);` |
|       7 |  730 | `								PH7_ClassInstanceUnref(pThis);` |
|       7 |  731 | `								VM_EXIT_BREAK;` |
|      68 |  732 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|      13 |  733 | `							 && !VmMemberNextIsWrite(pNext)` |
|      16 |  734 | `							 && PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) ){` |
|       - |  735 | `								/* Subscript-write base on a class with __get: php reads` |
|       - |  736 | `								 * through the magic layer (the write lands on the temp and` |
|       - |  737 | `								 * is lost, like php's indirect-modification case). A PLAIN` |
|       - |  738 | `								 * store (bPlainStore — the compiler tags those` |
|       - |  739 | `								 * PH7_MEMBER_WRITE too) is NOT this case: it falls through` |
|       - |  740 | `								 * to dynamic creation. A direct ++/--/compound-assign` |
|       - |  741 | `								 * (VmMemberNextIsWrite — the compiler now tags those` |
|       - |  742 | `								 * PH7_MEMBER_WRITE as well) is NOT this case either: it` |
|       - |  743 | `								 * falls through to dynamic creation like before (the` |
|       - |  744 | `								 * recorded RMW-vivifies-instead-of-__get residual, §7).` |
|       - |  745 | `								 * Leave the miss path — the read gate below dispatches` |
|       - |  746 | `								 * __get. */` |
|      72 |  747 | `							}else if( pThis->pClass->iFlags & PH7_CLASS_READONLY ){` |
|       - |  748 | `								SyBlob sErrMsg;` |
|     ! 0 |  749 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     ! 0 |  750 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|     ! 0 |  751 | `									&pThis->pClass->sName,&sName);` |
|     ! 0 |  752 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      68 |  753 | `							}else if( !VmClassAllowsDynamicProps(pVm,pThis->pClass)` |
|      42 |  754 | `							       && !VmClassHasAttributeNamed(pThis->pClass,"AllowDynamicProperties",sizeof("AllowDynamicProperties")-1) ){` |
|       - |  755 | `								/* php 8.2 only DEPRECATES creating a dynamic property on a` |
|       - |  756 | `								 * class without #[AllowDynamicProperties]; PHL rejects it.` |
|       - |  757 | `								 * stdClass / __set / declared props are unaffected. */` |
|       - |  758 | `								SyBlob sErrMsg;` |
|       6 |  759 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       6 |  760 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|       4 |  761 | `									&pThis->pClass->sName,&sName);` |
|       6 |  762 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       4 |  763 | `							}else{` |
|      67 |  764 | `								PH7_VmCreateDynamicAttr(&(*pVm),pThis,sName.zString,sName.nByte,&pObjAttr);` |
|       - |  765 | `							}` |
|       - |  766 | `						}` |
|      41 |  767 | `					}` |
|     230 |  768 | `				}` |
| 3021623 |  769 | `				if( pObjAttr == 0 && pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|       - |  770 | `					/* D1 commit 2: deferred property arg, property absent on a REACHABLE object` |
|       - |  771 | `					 * base ($o is a real slot). Record a property step rooted at $o so OP_CALL can` |
|       - |  772 | `					 * vivify+bind (by-ref) or read via __get / warn (by-value). A magic __get/__set` |
|       - |  773 | `					 * class is recorded the same way; the resolve step emits php's Notice for the` |
|       - |  774 | `					 * by-ref case and dispatches __get for by-value. */` |
|       - |  775 | `					SyString sProp;` |
|      78 |  776 | `					VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|      78 |  777 | `					SyStringInitFromBuf(&sProp,sName.zString,sName.nByte);` |
|      78 |  778 | `					if( pPath && VmDeferPathPushProp(pPath,&sProp) == SXRET_OK ){` |
|      78 |  779 | `						VmPopOperand(&pTos,1);       /* drop the property name */` |
|      78 |  780 | `						pThis->iRef++;` |
|      78 |  781 | `						PH7_MemObjRelease(pTos);     /* collapse the object slot into the carrier */` |
|      78 |  782 | `						pTos->x.pOther = pPath;` |
|      78 |  783 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      78 |  784 | `						pTos->nIdx = SXU32_HIGH;` |
|      78 |  785 | `						PH7_ClassInstanceUnref(pThis);` |
|      78 |  786 | `						VM_EXIT_BREAK;` |
|       - |  787 | `					}` |
|     ! 0 |  788 | `					if( pPath ){` |
|     ! 0 |  789 | `						VmFreeDeferredPath(pPath);` |
|     ! 0 |  790 | `					}` |
|       - |  791 | `					/* fall through to the normal miss handling on allocation failure */` |
|     ! 0 |  792 | `				}` |
| 3021547 |  793 | `				if( pObjAttr == 0 ){` |
|       - |  794 | `					/* Missing property. On a plain READ, php dispatches __get($name) and the` |
|       - |  795 | `					 * expression takes its RETURN VALUE (band A #3a — pre-fix the result was` |
|       - |  796 | `					 * discarded and a PHL-native warn fired even when __get existed). isset/` |
|       - |  797 | `					 * empty context consults __isset first (then __get for empty()'s value` |
|       - |  798 | `					 * test); a plain-store write context parked a pending __set above. A` |
|       - |  799 | `					 * self-recursive read of the same property falls back to the` |
|       - |  800 | `					 * undefined-property path via the guard, like php's property guard. */` |
|     317 |  801 | `					ph7_class_method *pGetMagic = 0;` |
|     317 |  802 | `					if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|      43 |  803 | `						ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|      43 |  804 | `						if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - |  805 | `							ph7_value sIssetRet;` |
|       - |  806 | `							int bSet;` |
|      13 |  807 | `							PH7_MemObjInit(pVm,&sIssetRet);` |
|      13 |  808 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|      13 |  809 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|      13 |  810 | `							VmMagicGuardPop(pVm);` |
|      13 |  811 | `							PH7_MemObjToBool(&sIssetRet);` |
|      13 |  812 | `							bSet = sIssetRet.x.iVal != 0;` |
|      13 |  813 | `							PH7_MemObjRelease(&sIssetRet);` |
|      13 |  814 | `							if( bSet && pInstr->iP2 == PH7_MEMBER_EMPTY ){` |
|       - |  815 | `								/* empty(): __isset said set — fetch the value via __get` |
|       - |  816 | `								 * (php) so emptiness is judged on the real value. */` |
|       3 |  817 | `								ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       - |  818 | `								ph7_value sEmptyVal;` |
|       3 |  819 | `								PH7_MemObjInit(pVm,&sEmptyVal);` |
|       3 |  820 | `								if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       3 |  821 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       3 |  822 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|       3 |  823 | `									VmMagicGuardPop(pVm);` |
|       1 |  824 | `								}` |
|       3 |  825 | `								VmPopOperand(&pTos,1);` |
|       3 |  826 | `								pThis->iRef++;` |
|       3 |  827 | `								PH7_MemObjRelease(pTos);` |
|       3 |  828 | `								PH7_MemObjStore(&sEmptyVal,pTos);` |
|       3 |  829 | `								pTos->nIdx = SXU32_HIGH;` |
|       3 |  830 | `								PH7_MemObjRelease(&sEmptyVal);` |
|       3 |  831 | `								PH7_ClassInstanceUnref(pThis);` |
|       3 |  832 | `								VM_EXIT_BREAK;` |
|       - |  833 | `							}` |
|       - |  834 | `							/* isset(): the truth of __isset IS the answer — push a non-null` |
|       - |  835 | `							 * marker for true, NULL for false (isset only tests null-ness). */` |
|      11 |  836 | `							VmPopOperand(&pTos,1);` |
|      11 |  837 | `							pThis->iRef++;` |
|      11 |  838 | `							PH7_MemObjRelease(pTos);` |
|      11 |  839 | `							if( bSet ){` |
|       5 |  840 | `								pTos->x.iVal = 1;` |
|       5 |  841 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       2 |  842 | `							}` |
|      11 |  843 | `							pTos->nIdx = SXU32_HIGH;` |
|      11 |  844 | `							PH7_ClassInstanceUnref(pThis);` |
|      11 |  845 | `							VM_EXIT_BREAK;` |
|       - |  846 | `						}` |
|      15 |  847 | `					}` |
|     302 |  848 | `					if( !VmMemberCtxIsLookup(pInstr->iP2) && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|     275 |  849 | `					 && !VmMemberNextIsWrite(pInstr + 1) ){` |
|       - |  850 | `						/* Plain reads AND the ??=/subscript write-base (PH7_MEMBER_WRITE,` |
|       - |  851 | `						 * which php reads through __get); read-modify-write forms are` |
|       - |  852 | `						 * excluded (they vivified above — approximate, recorded), and so is` |
|       - |  853 | `						 * a destructuring target (a pure write — php never reads it). */` |
|     262 |  854 | `						pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|     130 |  855 | `					}` |
|     305 |  856 | `					if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - |  857 | `						ph7_value sMagicRet;` |
|     257 |  858 | `						PH7_MemObjInit(pVm,&sMagicRet);` |
|     257 |  859 | `						VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     257 |  860 | `						PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|     257 |  861 | `						VmMagicGuardPop(pVm);` |
|       - |  862 | `						/* Pop the attribute name, replace the object slot with the magic` |
|       - |  863 | `						 * result (a temp, not an lvalue — nIdx stays constant). A throw` |
|       - |  864 | `						 * from __get parked at the boundary; the fetch-point router lands` |
|       - |  865 | `						 * it and abandons this slot. */` |
|     257 |  866 | `						VmPopOperand(&pTos,1);` |
|     257 |  867 | `						pThis->iRef++;` |
|     257 |  868 | `						PH7_MemObjRelease(pTos);` |
|     257 |  869 | `						PH7_MemObjStore(&sMagicRet,pTos);` |
|     257 |  870 | `						pTos->nIdx = SXU32_HIGH;` |
|     257 |  871 | `						PH7_MemObjRelease(&sMagicRet);` |
|     257 |  872 | `						PH7_ClassInstanceUnref(pThis);` |
|     257 |  873 | `						VM_EXIT_BREAK;` |
|       - |  874 | `					}` |
|       - |  875 | `					/* No __get (or guard held): load null. In isset()/empty() context (iP2 3/4)` |
|       - |  876 | `					 * PHP returns false/true SILENTLY, so suppress the read-miss warning there` |
|       - |  877 | `					 * (mirrors the array LOAD_IDX iP2=4/6 suppression); likewise when a` |
|       - |  878 | `					 * pending __set was parked above (the following OP_STORE dispatches it —` |
|       - |  879 | `					 * nothing is "undefined" about that write) or a throw is parked on the` |
|       - |  880 | `					 * boundary rail (e.g. the readonly-class dynamic-property Error — the` |
|       - |  881 | `					 * fetch-point router lands it right after this op). */` |
|      46 |  882 | `					if( !VmMemberCtxIsLookup(pInstr->iP2) && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|      16 |  883 | `					 && pVm->pMagicSetThis == 0` |
|      15 |  884 | `					 && pVm->nBoundaryRc == 0 ){` |
|       - |  885 | `						/* A destructuring target is also silent: php either created the` |
|       - |  886 | `						 * property above or dispatched __set — neither warns. */` |
|       8 |  887 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|       2 |  888 | `							&pClass->sName,&sName);` |
|       2 |  889 | `					}` |
|      23 |  890 | `				}` |
| 3021279 |  891 | `				VmPopOperand(&pTos,1);` |
|       - |  892 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|       - |  893 | `				 * This is due to the following case:` |
|       - |  894 | `				 *     (new TestClass())->foo;` |
|       - |  895 | `				 */` |
| 3021279 |  896 | `				pThis->iRef++;` |
| 3021279 |  897 | `				PH7_MemObjRelease(pTos);` |
| 3021279 |  898 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
| 3021279 |  899 | `				if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - |  900 | ``					/* `$o->p =& $x`: stash the resolved instance property slot for the`` |
|       - |  901 | `					 * following member-marked OP_STORE_REF, which rebinds it to alias the` |
|       - |  902 | `					 * source variable. Do NOT run the read/hook/magic/uninit machinery` |
|       - |  903 | `					 * below — a reference bind neither reads the value nor triggers` |
|       - |  904 | `					 * get/set hooks or an uninitialized-typed Error. pThis stays retained` |
|       - |  905 | `					 * (the iRef++ above); OP_STORE_REF releases it. */` |
|       9 |  906 | `					if( pObjAttr ){` |
|       9 |  907 | `						pVm->pRefTargetAttr = pObjAttr;` |
|       9 |  908 | `						pVm->pRefTargetThis = pThis;` |
|       9 |  909 | `						pVm->pRefTargetStaticAttr = 0;` |
|       9 |  910 | `						pTos->nIdx = pObjAttr->nIdx;` |
|       5 |  911 | `					}else{` |
|       - |  912 | `						/* Missing/inaccessible target: leave nothing stashed; OP_STORE_REF` |
|       - |  913 | `						 * no-ops. Balance the retain. */` |
|     ! 0 |  914 | `						pVm->pRefTargetAttr = 0;` |
|     ! 0 |  915 | `						pVm->pRefTargetThis = 0;` |
|     ! 0 |  916 | `						pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 |  917 | `						PH7_ClassInstanceUnref(pThis);` |
|       - |  918 | `					}` |
|       9 |  919 | `					VM_EXIT_BREAK;` |
|       - |  920 | `				}` |
| 3021271 |  921 | `				if( pObjAttr ){` |
| 3021225 |  922 | `					ph7_value *pValue = 0; /* cc warning */` |
|       - |  923 | `					/* Check attribute access */` |
| 3021225 |  924 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
| 3021202 |  925 | `						if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET))` |
| 1510748 |  926 | `						 && !VmHookGuardHeld(pVm,(void *)pThis,&sName) ){` |
|       - |  927 | `							/* PHP 8.4 property hooks: route reads, writes, and the` |
|       - |  928 | `							 * read-modify-write forms through the synthesized hook` |
|       - |  929 | `							 * methods. A held guard (either kind) means we are INSIDE` |
|       - |  930 | `							 * one of this property's own hook bodies — fall through to` |
|       - |  931 | `` 							 * the raw backing slot for BOTH directions (php: `$this->x` `` |
|       - |  932 | `							 * within any of x's hooks addresses the backing store). */` |
|     151 |  933 | `							VmInstr *pNextH = pInstr + 1;` |
|     151 |  934 | `							int bPlainStore = (pNextH->iOp == PH7_OP_STORE && pNextH->iP2 != 0);` |
|     262 |  935 | `							int bCoalesceW = pInstr->iP2 == PH7_MEMBER_WRITE` |
|     150 |  936 | `								&& pNextH->iOp == PH7_OP_NULLC_JMP;` |
|       - |  937 | `							/* ++/--/compound-assign: the modify op FOLLOWS the member` |
|       - |  938 | `							 * directly (the compiler tags compound-assign members` |
|       - |  939 | `							 * PH7_MEMBER_WRITE too, so classify by the next op, not iP2;` |
|       - |  940 | `							 * the !bPlainStore term is load-bearing — VmMemberNextIsWrite` |
|       - |  941 | `							 * returns 1 for a member OP_STORE too) */` |
|     151 |  942 | `							int bRmwNext = !bPlainStore && VmMemberNextIsWrite(pNextH);` |
|     177 |  943 | `							int bSubscriptW = !bPlainStore && !bCoalesceW && !bRmwNext` |
|     210 |  944 | `								&& pInstr->iP2 == PH7_MEMBER_WRITE;` |
|     151 |  945 | `							if( bPlainStore ){` |
|       - |  946 | `								/* Arm the pending hook-set for the following OP_STORE — it` |
|       - |  947 | `								 * dispatches the set hook, or throws the read-only Error` |
|       - |  948 | `								 * when the property only has a get hook. (The scalar` |
|       - |  949 | `								 * transient is safe here: its window is exactly one` |
|       - |  950 | `								 * instruction, MEMBER -> STORE, nothing runs in between.) */` |
|      31 |  951 | `								pThis->iRef++;` |
|      31 |  952 | `								pVm->pHookSetThis = pThis;` |
|      31 |  953 | `								pVm->pHookSetAttr = pObjAttr->pAttr;` |
|      31 |  954 | `								pVm->nHookSetIdx = pObjAttr->nIdx;` |
|      31 |  955 | `								pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|      31 |  956 | `								PH7_ClassInstanceUnref(pThis);` |
|      31 |  957 | `								VM_EXIT_BREAK;` |
|       - |  958 | `							}` |
|     121 |  959 | `							if( bSubscriptW ){` |
|       - |  960 | `								/* Subscript write base ($o->m[] = v, $o->m[$k] = v):` |
|       - |  961 | `								 * php's catchable Error, with or without a set hook` |
|       - |  962 | ``								 * (only a by-ref `&get` hook would allow it — a loud`` |
|       - |  963 | `								 * compile error in PHL). The op completes benignly with` |
|       - |  964 | `								 * a null temp; the fetch-point router lands the throw. */` |
|       - |  965 | `								SyBlob sErrMsg;` |
|       5 |  966 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       5 |  967 | `								SyBlobFormat(&sErrMsg,"Indirect modification of %z::$%z is not allowed",` |
|       4 |  968 | `									&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       5 |  969 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       5 |  970 | `								PH7_ClassInstanceUnref(pThis);` |
|       5 |  971 | `								VM_EXIT_BREAK;` |
|       - |  972 | `							}` |
|     116 |  973 | `							if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|      59 |  974 | `							 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - |  975 | `								/* VIRTUAL set-only property: there is no backing store to` |
|       - |  976 | `								 * fall back to, so EVERY read context — plain read,` |
|       - |  977 | `								 * isset()/empty() (php throws there too, probe-verified),` |
|       - |  978 | `								 * the ??= test, an RMW read — is php's catchable` |
|       - |  979 | `								 * write-only Error. */` |
|       - |  980 | `								SyBlob sErrMsg;` |
|       9 |  981 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       9 |  982 | `								SyBlobFormat(&sErrMsg,"Property %z::$%z is write-only",` |
|       8 |  983 | `									&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       9 |  984 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       9 |  985 | `								PH7_ClassInstanceUnref(pThis);` |
|       9 |  986 | `								VM_EXIT_BREAK;` |
|       - |  987 | `							}` |
|     109 |  988 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - |  989 | `								/* isset()/empty() on a hooked property calls the get hook` |
|       - |  990 | `								 * (php): isset is "get() !== null", empty tests the value. */` |
|       - |  991 | `								ph7_value sHookRet;` |
|       5 |  992 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|       5 |  993 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|       5 |  994 | `									if( pInstr->iP2 == PH7_MEMBER_ISSET ){` |
|       3 |  995 | `										pTos->x.iVal = (sHookRet.iFlags & MEMOBJ_NULL) == 0;` |
|       3 |  996 | `										MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       2 |  997 | `									}else{` |
|       - |  998 | `										/* empty(): hand the value to the truthiness test */` |
|       3 |  999 | `										PH7_MemObjStore(&sHookRet,pTos);` |
|       - | 1000 | `									}` |
|       5 | 1001 | `									pTos->nIdx = SXU32_HIGH;` |
|       5 | 1002 | `									PH7_MemObjRelease(&sHookRet);` |
|       5 | 1003 | `									PH7_ClassInstanceUnref(pThis);` |
|       5 | 1004 | `									VM_EXIT_BREAK;` |
|       - | 1005 | `								}` |
|     ! 0 | 1006 | `								PH7_MemObjRelease(&sHookRet);` |
|     ! 0 | 1007 | `							}` |
|     105 | 1008 | `							if( !bCoalesceW && !bRmwNext && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1009 | `								/* Read: dispatch __phl_hook_get_NAME (inheritance-aware) */` |
|       - | 1010 | `								ph7_value sHookRet;` |
|      71 | 1011 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|      71 | 1012 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|      57 | 1013 | `									PH7_MemObjStore(&sHookRet,pTos);` |
|      57 | 1014 | `									pTos->nIdx = SXU32_HIGH;` |
|      57 | 1015 | `									PH7_MemObjRelease(&sHookRet);` |
|      57 | 1016 | `									PH7_ClassInstanceUnref(pThis);` |
|      57 | 1017 | `									VM_EXIT_BREAK;` |
|       - | 1018 | `								}` |
|      15 | 1019 | `								PH7_MemObjRelease(&sHookRet);` |
|       7 | 1020 | `							}` |
|      49 | 1021 | `							if( bCoalesceW \|\| bRmwNext ){` |
|       - | 1022 | ``								/* `$o->x ??= v` (bCoalesceW) and the read-modify-write`` |
|       - | 1023 | ``								 * forms `$o->x++`, `$o->x .= v`, … (bRmwNext): php reads`` |
|       - | 1024 | `								 * through the get hook (raw backing when the property is` |
|       - | 1025 | `								 * set-only) and writes through the set hook.` |
|       - | 1026 | `								 *   - ??=: the test value goes on the stack and a` |
|       - | 1027 | `								 *     COAL_HOOK entry is pushed for the OP_NULLC_STORE` |
|       - | 1028 | `								 *     at the jump target; the fetch-point sweep drops it` |
|       - | 1029 | `								 *     when the short-circuit jump skips the assign or a` |
|       - | 1030 | `								 *     throw abandons the RHS (the entry is NOT a scalar` |
|       - | 1031 | `								 *     transient — nested stores/coalesces in the RHS` |
|       - | 1032 | `								 *     stack their own entries above it).` |
|       - | 1033 | `								 *   - RMW: the value goes into a fresh SCRATCH slot the` |
|       - | 1034 | `								 *     modify op mutates in place; the entry makes the` |
|       - | 1035 | `								 *     op's tail dispatch the set side with the computed` |
|       - | 1036 | `								 *     value (PH7_HOOK_RMW_WRITEBACK). */` |
|       - | 1037 | `								ph7_value sCur;` |
|       - | 1038 | `								sxi32 rcCur;` |
|      35 | 1039 | `								PH7_MemObjInit(pVm,&sCur);` |
|      35 | 1040 | `								rcCur = PH7_VmHookGetAttrValue(pThis,pObjAttr,&sCur);` |
|      35 | 1041 | `								if( rcCur == SXERR_NOTFOUND ){` |
|       - | 1042 | `									/* set-only hook (or guard edge): the read side is the` |
|       - | 1043 | `									 * raw backing store (php) */` |
|       3 | 1044 | `									ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|       3 | 1045 | `									if( pBack ){` |
|       3 | 1046 | `										PH7_MemObjStore(pBack,&sCur);` |
|       2 | 1047 | `									}` |
|      34 | 1048 | `								}else if( pVm->nBoundaryRc != 0 ){` |
|       - | 1049 | `									/* the get hook threw: leave the null temp; the` |
|       - | 1050 | `									 * fetch-point router lands the parked throw —` |
|       - | 1051 | `									 * nothing is armed. */` |
|     ! 0 | 1052 | `									PH7_MemObjRelease(&sCur);` |
|     ! 0 | 1053 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1054 | `									VM_EXIT_BREAK;` |
|       - | 1055 | `								}` |
|      35 | 1056 | `								if( bCoalesceW ){` |
|       - | 1057 | `									VmHookRmw sPend;` |
|      15 | 1058 | `									PH7_MemObjStore(&sCur,pTos);` |
|      15 | 1059 | `									pTos->nIdx = SXU32_HIGH;` |
|      15 | 1060 | `									PH7_MemObjRelease(&sCur);` |
|      15 | 1061 | `									sPend.iKind = VM_HOOK_PEND_COAL_HOOK;` |
|      15 | 1062 | `									sPend.pThis = pThis;` |
|      15 | 1063 | `									sPend.pAttr = pObjAttr->pAttr;` |
|      15 | 1064 | `									sPend.nBackIdx = pObjAttr->nIdx;` |
|      15 | 1065 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|      15 | 1066 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|      15 | 1067 | `									sPend.pOwnerStack = (void *)pStack;` |
|      15 | 1068 | `									sPend.pInstrs = (void *)aInstr;` |
|      15 | 1069 | `									sPend.nJmpPc = (sxu32)(pc + 1);            /* the OP_NULLC_JMP */` |
|      15 | 1070 | `									sPend.nPc = (sxu32)(pNextH->iP2 - 1);      /* the OP_NULLC_STORE */` |
|      15 | 1071 | `									pThis->iRef++;` |
|      15 | 1072 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|      15 | 1073 | `									PH7_ClassInstanceUnref(pThis);` |
|      15 | 1074 | `									VM_EXIT_BREAK;` |
|       - | 1075 | `								}` |
|       - | 1076 | `								{` |
|      21 | 1077 | `									ph7_value *pScr = PH7_ReserveMemObj(&(*pVm));` |
|      21 | 1078 | `									if( pScr ){` |
|       - | 1079 | `										VmHookRmw sRmw;` |
|      21 | 1080 | `										PH7_MemObjStore(&sCur,pScr);` |
|      21 | 1081 | `										PH7_MemObjStore(&sCur,pTos);` |
|      21 | 1082 | `										pTos->nIdx = pScr->nIdx;` |
|      21 | 1083 | `										sRmw.iKind = VM_HOOK_PEND_RMW;` |
|      21 | 1084 | `										sRmw.pThis = pThis;` |
|      21 | 1085 | `										sRmw.pAttr = pObjAttr->pAttr;` |
|      21 | 1086 | `										sRmw.nBackIdx = pObjAttr->nIdx;` |
|      21 | 1087 | `										sRmw.nScratchIdx = pScr->nIdx;` |
|      21 | 1088 | `										SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      21 | 1089 | `										sRmw.pOwnerStack = (void *)pStack;` |
|      21 | 1090 | `										sRmw.pInstrs = (void *)aInstr;` |
|      21 | 1091 | `										sRmw.nJmpPc = (sxu32)(pc + 1);  /* the modify op ... */` |
|      21 | 1092 | `										sRmw.nPc = (sxu32)(pc + 1);     /* ... is the whole window */` |
|      21 | 1093 | `										pThis->iRef++;` |
|      21 | 1094 | `										SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      10 | 1095 | `									}` |
|       - | 1096 | `									/* OOM: pScr == 0 — leave the null temp (loud allocator` |
|       - | 1097 | `									 * diagnostics already fired) */` |
|      21 | 1098 | `									PH7_MemObjRelease(&sCur);` |
|      21 | 1099 | `									PH7_ClassInstanceUnref(pThis);` |
|      21 | 1100 | `									VM_EXIT_BREAK;` |
|       - | 1101 | `								}` |
|       - | 1102 | `							}` |
|       7 | 1103 | `						}` |
|       - | 1104 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|       - | 1105 | `						 * We can only raise it on a real read, not when the slot is the` |
|       - | 1106 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|       - | 1107 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|       - | 1108 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
| 3021066 | 1109 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
| 1510664 | 1110 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|     257 | 1111 | `							VmInstr *pNext = pInstr + 1;` |
|     257 | 1112 | `							int bIsLhs = 0;` |
|     257 | 1113 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|     241 | 1114 | `								bIsLhs = 1;` |
|     118 | 1115 | `							}` |
|     257 | 1116 | `							if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 1117 | `								/* Destructuring target: the OP_LOAD_LIST store (typed-` |
|       - | 1118 | `								 * enforced) initializes it — never a read (php). */` |
|       7 | 1119 | `								bIsLhs = 1;` |
|       3 | 1120 | `							}` |
|       - | 1121 | ``							/* isset()/empty()/`??` read an uninitialized typed property`` |
|       - | 1122 | `							 * as "not set" — a silent miss, NOT the Error a plain read` |
|       - | 1123 | `							 * raises (php). The compiler tags such an access iP2 =` |
|       - | 1124 | `							 * ISSET/EMPTY; treat it like the assignment-LHS case and fall` |
|       - | 1125 | `							 * through to load the slot's NULL. */` |
|     257 | 1126 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       7 | 1127 | `								bIsLhs = 1;` |
|       3 | 1128 | `							}` |
|     257 | 1129 | `							if( !bIsLhs ){` |
|       6 | 1130 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|       6 | 1131 | `								PH7_ClassInstanceUnref(pThis);` |
|       6 | 1132 | `								if( rcU == PH7_ABORT ){` |
|       3 | 1133 | `									VM_EXIT_ABORT;` |
|       - | 1134 | `								}` |
|       - | 1135 | `								{` |
|       - | 1136 | `									sxi32 iRp;` |
|       3 | 1137 | `									if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|       5 | 1138 | `										PH7_RESUME_DRAIN()` |
|       3 | 1139 | `										pc = iRp;` |
|       3 | 1140 | `										VM_EXIT_BREAK;` |
|       - | 1141 | `									}` |
|       - | 1142 | `								}` |
|     ! 0 | 1143 | `								VM_EXIT_EXCEPTION;` |
|       - | 1144 | `							}` |
|     124 | 1145 | `						}` |
|       - | 1146 | `						/* Load attribute */` |
| 3021067 | 1147 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
| 3021067 | 1148 | `						if( pValue ){` |
| 3021067 | 1149 | `							if( pThis->iRef < 2 ){` |
|       - | 1150 | `								/* Perform a store operation,rather than a load operation since` |
|       - | 1151 | `								 * the class instance '$this' will be deleted shortly.` |
|       - | 1152 | `								 */` |
|     110 | 1153 | `								PH7_MemObjStore(pValue,pTos);` |
|      56 | 1154 | `							}else{` |
|       - | 1155 | `								/* Simple load */` |
| 3020959 | 1156 | `								PH7_MemObjLoad(pValue,pTos);` |
|       - | 1157 | `							}` |
| 3021067 | 1158 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
| 3021067 | 1159 | `								if( pThis->iRef > 1 ){` |
|       - | 1160 | `									/* Load attribute index */` |
| 3020959 | 1161 | `									pTos->nIdx = pObjAttr->nIdx;` |
| 1510477 | 1162 | `								}` |
| 1510531 | 1163 | `							}` |
| 1510531 | 1164 | `						}` |
| 1510536 | 1165 | `					}else{` |
|       - | 1166 | `						/* Inaccessible (private/protected) from this scope. php consults` |
|       - | 1167 | `						 * __get here exactly like a missing property (band A #3a) before` |
|       - | 1168 | `						 * erroring; the guard keeps a self-recursive read from looping. */` |
|      20 | 1169 | `						ph7_class_method *pGetMagic = 0;` |
|      18 | 1170 | `						if( pInstr->iP2 != PH7_MEMBER_WRITE && !VmMemberCtxIsLookup(pInstr->iP2)` |
|      14 | 1171 | `						 && !VmMemberNextIsWrite(pInstr + 1) ){` |
|      10 | 1172 | `							pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       4 | 1173 | `						}` |
|      20 | 1174 | `						if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 1175 | `							ph7_value sMagicRet;` |
|       3 | 1176 | `							PH7_MemObjInit(pVm,&sMagicRet);` |
|       3 | 1177 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       3 | 1178 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|       3 | 1179 | `							VmMagicGuardPop(pVm);` |
|       - | 1180 | `							/* The name was already popped and pTos released above; just` |
|       - | 1181 | `							 * take the magic result as the expression value. */` |
|       3 | 1182 | `							PH7_MemObjStore(&sMagicRet,pTos);` |
|       3 | 1183 | `							pTos->nIdx = SXU32_HIGH;` |
|       3 | 1184 | `							PH7_MemObjRelease(&sMagicRet);` |
|       3 | 1185 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 1186 | `							VM_EXIT_BREAK;` |
|       - | 1187 | `						}` |
|      18 | 1188 | `						if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1189 | `							/* isset/empty on an inaccessible property: php consults __isset` |
|       - | 1190 | `							 * (band A #3b), and is silently false without it — never an` |
|       - | 1191 | `							 * Error (pre-fix PHL fataled here). */` |
|       9 | 1192 | `							ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|       9 | 1193 | `							int bSet = 0;` |
|       9 | 1194 | `							if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 1195 | `								ph7_value sIssetRet;` |
|     ! 0 | 1196 | `								PH7_MemObjInit(pVm,&sIssetRet);` |
|     ! 0 | 1197 | `								VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|     ! 0 | 1198 | `								PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|     ! 0 | 1199 | `								VmMagicGuardPop(pVm);` |
|     ! 0 | 1200 | `								PH7_MemObjToBool(&sIssetRet);` |
|     ! 0 | 1201 | `								bSet = sIssetRet.x.iVal != 0;` |
|     ! 0 | 1202 | `								PH7_MemObjRelease(&sIssetRet);` |
|     ! 0 | 1203 | `							}` |
|       9 | 1204 | `							if( bSet ){` |
|     ! 0 | 1205 | `								if( pInstr->iP2 == PH7_MEMBER_EMPTY ){` |
|     ! 0 | 1206 | `									ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       - | 1207 | `									ph7_value sEmptyVal;` |
|     ! 0 | 1208 | `									PH7_MemObjInit(pVm,&sEmptyVal);` |
|     ! 0 | 1209 | `									if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|     ! 0 | 1210 | `										VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     ! 0 | 1211 | `										PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|     ! 0 | 1212 | `										VmMagicGuardPop(pVm);` |
|     ! 0 | 1213 | `									}` |
|     ! 0 | 1214 | `									PH7_MemObjStore(&sEmptyVal,pTos);` |
|     ! 0 | 1215 | `									pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1216 | `									PH7_MemObjRelease(&sEmptyVal);` |
|     ! 0 | 1217 | `								}else{` |
|     ! 0 | 1218 | `									pTos->x.iVal = 1;` |
|     ! 0 | 1219 | `									MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     ! 0 | 1220 | `									pTos->nIdx = SXU32_HIGH;` |
|       - | 1221 | `								}` |
|     ! 0 | 1222 | `							}` |
|       9 | 1223 | `							PH7_ClassInstanceUnref(pThis);` |
|       9 | 1224 | `							VM_EXIT_BREAK;` |
|       - | 1225 | `						}` |
|       - | 1226 | `						{` |
|       - | 1227 | `							/* A plain store to an inaccessible property dispatches __set` |
|       - | 1228 | `							 * (band A #3b): park the receiver+name for the following` |
|       - | 1229 | `							 * OP_STORE, exactly like the missing-property case. */` |
|      10 | 1230 | `							VmInstr *pNextW = pInstr + 1;` |
|      10 | 1231 | `							if( pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0 ){` |
|       3 | 1232 | `								ph7_class_method *pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|       3 | 1233 | `								if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){` |
|       3 | 1234 | `									pThis->iRef++;` |
|       3 | 1235 | `									pVm->pMagicSetThis = pThis;` |
|       3 | 1236 | `									SyBlobReset(&pVm->sMagicSetName);` |
|       3 | 1237 | `									SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       3 | 1238 | `									pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|       3 | 1239 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 1240 | `									VM_EXIT_BREAK;` |
|       - | 1241 | `								}` |
|     ! 0 | 1242 | `							}` |
|       - | 1243 | `						}` |
|       6 | 1244 | `						if( ((pInstr + 1)->iOp == PH7_OP_NULLC \|\| (pInstr + 1)->iOp == PH7_OP_NULLC_JMP)` |
|       4 | 1245 | `						 && pInstr->iP2 != PH7_MEMBER_WRITE ){` |
|       - | 1246 | `` 							/* `$o->priv ?? default` (OP_NULLC; NULLC_JMP is the `??=` `` |
|       - | 1247 | ``							 * pre-test): php treats `??` as an isset-style lookup — an`` |
|       - | 1248 | `							 * inaccessible property yields null SILENTLY (the` |
|       - | 1249 | `							 * __isset/__get consults already ran above), letting the` |
|       - | 1250 | `							 * coalesce pick the default. */` |
|     ! 0 | 1251 | `							PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1252 | `							VM_EXIT_BREAK; /* pTos is already the null temp */` |
|       - | 1253 | `						}` |
|       - | 1254 | `						/* A subclass reading a PARENT's PRIVATE property: php treats it as` |
|       - | 1255 | `						 * an UNDEFINED property (the base-private is invisible to the` |
|       - | 1256 | `						 * subclass scope) — a Warning + null, NOT an access Error. Only` |
|       - | 1257 | `						 * this exact shape warns; every other denied read is the catchable` |
|       - | 1258 | `						 * "Cannot access" Error below. */` |
|       - | 1259 | `						{` |
|       7 | 1260 | `						ph7_class *pSelf = VmCurrentSelf(&(*pVm));` |
|       7 | 1261 | `						ph7_class *pDecl = pObjAttr->pAttr->pDeclClass;` |
|       6 | 1262 | `						if( pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       6 | 1263 | `						 && pSelf && pDecl && pSelf != pDecl && PH7_VmInstanceOf(pSelf,pDecl) ){` |
|       3 | 1264 | `							if( !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       4 | 1265 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|       1 | 1266 | `									&pClass->sName,&sName);` |
|       1 | 1267 | `							}` |
|       3 | 1268 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 1269 | `							VM_EXIT_BREAK; /* pTos is already the null temp */` |
|       - | 1270 | `						}` |
|       - | 1271 | `						}` |
|       - | 1272 | `						/* Genuinely inaccessible: php's CATCHABLE Error (parked on the` |
|       - | 1273 | `						 * boundary rail; the fetch-point router lands it — it was an` |
|       - | 1274 | `						 * uncatchable VmReportUncaughtException+Abort before). */` |
|       - | 1275 | `						{` |
|       - | 1276 | `						SyBlob sErrMsg;` |
|       5 | 1277 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       5 | 1278 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       5 | 1279 | `						SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",` |
|       2 | 1280 | `							zVis,&pClass->sName,&sName);` |
|       5 | 1281 | `						PH7_ClassInstanceUnref(pThis);` |
|       5 | 1282 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       5 | 1283 | `						SyBlobRelease(&sErrMsg);` |
|       5 | 1284 | `						VM_EXIT_BREAK;` |
|       - | 1285 | `						}` |
|       - | 1286 | `					}` |
| 1510531 | 1287 | `				}` |
|       - | 1288 | `				/* Safely unreference the object */` |
| 3021113 | 1289 | `				PH7_ClassInstanceUnref(pThis);` |
|       - | 1290 | `			}` |
| 1566565 | 1291 | `		}else{` |
|       - | 1292 | ``			/* `->` on a non-object (e.g. a null intermediate). Silent in isset()/empty()/unset()`` |
|       - | 1293 | ``			 * context (iP2 2/3/4) so `isset($o->missing->x)` / `unset($o->missing->x)` match PHP. */`` |
|      12 | 1294 | `			if( pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1295 | `				/* php draws a sharp line here, and PH7 drew none: CALLING a method on a` |
|       - | 1296 | `				 * non-object is a catchable Error, while READING a property off one is a` |
|       - | 1297 | `				 * Warning that yields NULL. PH7 raised the same notice for both and` |
|       - | 1298 | ``				 * carried on with NULL, so `$null->m()` silently did nothing. */`` |
|       - | 1299 | `				SyString sMemb;` |
|       8 | 1300 | `				SyStringInitFromBuf(&sMemb,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|       8 | 1301 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - | 1302 | `					SyBlob sErrM;` |
|       - | 1303 | `					sxi32 rcErr;` |
|       5 | 1304 | `					SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       5 | 1305 | `					SyBlobFormat(&sErrM,"Call to a member function %z() on %s",` |
|       2 | 1306 | `						&sMemb,VmArithTypeName(pNos));` |
|       5 | 1307 | `					VmPopOperand(&pTos,1);` |
|       5 | 1308 | `					PH7_MemObjRelease(pTos);` |
|       5 | 1309 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1310 | `					pTos->nIdx = SXU32_HIGH;` |
|       7 | 1311 | `					rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       2 | 1312 | `						SyBlobLength(&sErrM));` |
|       5 | 1313 | `					SyBlobRelease(&sErrM);` |
|       5 | 1314 | `					if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 1315 | `					rc = rcErr;` |
|       5 | 1316 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1317 | `				}` |
|       4 | 1318 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Attempt to read property \"%z\" on %s",` |
|       1 | 1319 | `					&sMemb,VmArithTypeName(pNos));` |
|       1 | 1320 | `			}` |
|       8 | 1321 | `			VmPopOperand(&pTos,1);` |
|       8 | 1322 | `			PH7_MemObjRelease(pTos);` |
|       8 | 1323 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|       - | 1324 | `		}` |
| 1566568 | 1325 | `	}else{` |
|       - | 1326 | `		/* Static member access using class name */` |
|  102747 | 1327 | `		pNos = pTos;` |
|  102747 | 1328 | `		pThis = 0;` |
|  102747 | 1329 | `		if( !pInstr->p3 ){` |
|    2119 | 1330 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    2119 | 1331 | `			pNos--;` |
|       - | 1332 | `#ifdef UNTRUST` |
|       - | 1333 | `			if( pNos < pStack ){` |
|       - | 1334 | `				VM_EXIT_ABORT;` |
|       - | 1335 | `			}` |
|       - | 1336 | `#endif` |
|    1062 | 1337 | `		}else{` |
|       - | 1338 | `			/* Attribute name already computed */` |
|  100633 | 1339 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|       - | 1340 | `		}` |
|  102747 | 1341 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|  102747 | 1342 | `			ph7_class *pClass = 0;` |
|       - | 1343 | `			/* A FORWARDING static call — self::/parent::/static:: — preserves the` |
|       - | 1344 | `			 * caller's late-static-binding class (php); a non-forwarding C::m() resets` |
|       - | 1345 | `			 * it to C. The receiver slot below keeps the literal keyword, which OP_CALL` |
|       - | 1346 | `			 * can't resolve, so it would fall back to the callee's DECLARING class and` |
|       - | 1347 | `			 * lose LSB. Remember the live LSB class here and stamp it onto the receiver` |
|       - | 1348 | `			 * after the method name is pushed. */` |
|  102747 | 1349 | `			int bForwardingCall = 0;` |
|  102747 | 1350 | `			ph7_class *pForwardLsb = 0;` |
|  102747 | 1351 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - | 1352 | `				/* Class already instantiated */` |
|      11 | 1353 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      11 | 1354 | `				pClass = pThis->pClass;` |
|      11 | 1355 | `				pThis->iRef++; /* Deffer garbage collection */` |
|       6 | 1356 | `			}else{` |
|       - | 1357 | `				/* Try to extract the target class */` |
|  102737 | 1358 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|  102737 | 1359 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|  102737 | 1360 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|       - | 1361 | `					/* Handle self/static/parent keywords */` |
|  102737 | 1362 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|     721 | 1363 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|     721 | 1364 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|       - | 1365 | `							/* In a trait method, self:: resolves to the using class */` |
|      14 | 1366 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       6 | 1367 | `						}` |
|     721 | 1368 | `						bForwardingCall = 1;` |
|     721 | 1369 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|  102379 | 1370 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|      61 | 1371 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      61 | 1372 | `						bForwardingCall = 1;` |
|      61 | 1373 | `						pForwardLsb = pClass;` |
|  101992 | 1374 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|     184 | 1375 | `						pClass = PH7_VmResolveParentClass(&(*pVm));` |
|     184 | 1376 | `						bForwardingCall = 1;` |
|     184 | 1377 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|      94 | 1378 | `					}else{` |
|  101783 | 1379 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|       - | 1380 | `					}` |
|   51366 | 1381 | `				}` |
|       - | 1382 | `			}` |
|  102747 | 1383 | `			if( pClass == 0 ){` |
|       - | 1384 | `				/* Undefined class: php throws a catchable Error */` |
|       - | 1385 | `				SyBlob sErrM;` |
|       - | 1386 | `				sxi32 rcErr;` |
|     ! 0 | 1387 | `				SyBlobInit(&sErrM,&pVm->sAllocator);` |
|     ! 0 | 1388 | `				SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|     ! 0 | 1389 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob));` |
|     ! 0 | 1390 | `				if( !pInstr->p3 ){` |
|     ! 0 | 1391 | `					VmPopOperand(&pTos,1);` |
|     ! 0 | 1392 | `				}` |
|     ! 0 | 1393 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 | 1394 | `				pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1395 | `				rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|     ! 0 | 1396 | `					SyBlobLength(&sErrM));` |
|     ! 0 | 1397 | `				SyBlobRelease(&sErrM);` |
|     ! 0 | 1398 | `				if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 1399 | `				rc = rcErr;` |
|     ! 0 | 1400 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 1401 | `			}else{` |
|  102747 | 1402 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - | 1403 | `					/* Method call */` |
|    1007 | 1404 | `					ph7_class_method *pMeth = 0;` |
|    1007 | 1405 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|       - | 1406 | `						/* Extract the target method */` |
|    1007 | 1407 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|     501 | 1408 | `					}` |
|    1007 | 1409 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|       7 | 1410 | `						if( pMeth ){` |
|       - | 1411 | `							SyBlob sErrM;` |
|       - | 1412 | `							sxi32 rcErr;` |
|       3 | 1413 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       3 | 1414 | `							SyBlobFormat(&sErrM,"Cannot call abstract method %z::%z()",` |
|       1 | 1415 | `								&pClass->sName,&sName);` |
|       3 | 1416 | `							if( !pInstr->p3 ){` |
|       3 | 1417 | `								VmPopOperand(&pTos,1);` |
|       1 | 1418 | `							}` |
|       3 | 1419 | `							PH7_MemObjRelease(pTos);` |
|       3 | 1420 | `							pTos->nIdx = SXU32_HIGH;` |
|       4 | 1421 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       1 | 1422 | `								SyBlobLength(&sErrM));` |
|       3 | 1423 | `							SyBlobRelease(&sErrM);` |
|       3 | 1424 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 | 1425 | `							rc = rcErr;` |
|       3 | 1426 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 1427 | `						}else{` |
|       5 | 1428 | `							ph7_class_method *pCallStaticMagic = PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1);` |
|       5 | 1429 | `							if( pCallStaticMagic ){` |
|       - | 1430 | `								/* php: C::missing(...) dispatches __callStatic($name,$args)` |
|       - | 1431 | `								 * via the packing trampoline (see the instance twin). */` |
|       3 | 1432 | `								SyBlobReset(&pVm->sMagicCallName);` |
|       3 | 1433 | `								SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);` |
|       3 | 1434 | `								pVm->pMagicCallThis = 0;` |
|       3 | 1435 | `								pVm->pMagicCallClass = pClass;` |
|       3 | 1436 | `								if( !pInstr->p3 ){` |
|       3 | 1437 | `									VmPopOperand(&pTos,1);` |
|       1 | 1438 | `								}` |
|       3 | 1439 | `								PH7_MemObjRelease(pTos);` |
|       3 | 1440 | `								SyBlobAppend(&pTos->sBlob,"__phl_magic_call",sizeof("__phl_magic_call")-1);` |
|       3 | 1441 | `								MemObjSetType(pTos,MEMOBJ_STRING);` |
|       3 | 1442 | `								pTos->nIdx = SXU32_HIGH;` |
|       3 | 1443 | `								VM_EXIT_BREAK;` |
|       - | 1444 | `							}` |
|       - | 1445 | `							{` |
|       - | 1446 | `								/* php: the STATIC form reports the same "Call to undefined` |
|       - | 1447 | `								 * method C::m()" as the instance one. */` |
|       - | 1448 | `								SyBlob sErrM;` |
|       - | 1449 | `								sxi32 rcErr;` |
|       3 | 1450 | `								SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       3 | 1451 | `								SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",` |
|       1 | 1452 | `									&pClass->sName,&sName);` |
|       3 | 1453 | `								if( !pInstr->p3 ){` |
|       3 | 1454 | `									VmPopOperand(&pTos,1);` |
|       1 | 1455 | `								}` |
|       3 | 1456 | `								PH7_MemObjRelease(pTos);` |
|       3 | 1457 | `								pTos->nIdx = SXU32_HIGH;` |
|       4 | 1458 | `								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       1 | 1459 | `									SyBlobLength(&sErrM));` |
|       3 | 1460 | `								SyBlobRelease(&sErrM);` |
|       3 | 1461 | `								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 | 1462 | `								rc = rcErr;` |
|       3 | 1463 | `								PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1464 | `							}` |
|       - | 1465 | `						}` |
|       - | 1466 | `						/* Pop the method name from the stack */` |
|     ! 0 | 1467 | `						if( !pInstr->p3 ){` |
|     ! 0 | 1468 | `							VmPopOperand(&pTos,1);` |
|       - | 1469 | `						}` |
|     ! 0 | 1470 | `						PH7_MemObjRelease(pTos);` |
|     ! 0 | 1471 | `					}else{` |
|       - | 1472 | `						/* Push method name on the stack */` |
|    1001 | 1473 | `						PH7_MemObjRelease(pTos);` |
|    1001 | 1474 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|    1001 | 1475 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|       - | 1476 | `					}` |
|    1001 | 1477 | `					pTos->nIdx = SXU32_HIGH;` |
|       - | 1478 | `					/* Forwarding call (self::/parent::/static::): overwrite the receiver` |
|       - | 1479 | `					 * slot (pNos, one below the method name) with the live LSB class name` |
|       - | 1480 | ``					 * so OP_CALL pushes THAT onto aSelf — preserving `static::` inside the`` |
|       - | 1481 | `					 * callee. Without this the literal keyword falls through to the` |
|       - | 1482 | `					 * callee's declaring class. Only when an LSB class is actually in` |
|       - | 1483 | `					 * scope (a static call from global scope has none). */` |
|    1001 | 1484 | `					if( bForwardingCall && pForwardLsb && (pNos->iFlags & MEMOBJ_STRING) ){` |
|     298 | 1485 | `						SyBlobReset(&pNos->sBlob);` |
|     298 | 1486 | `						SyBlobAppend(&pNos->sBlob,pForwardLsb->sName.zString,pForwardLsb->sName.nByte);` |
|     148 | 1487 | `					}` |
|     503 | 1488 | `				}else{` |
|       - | 1489 | `					/* Attribute access */` |
|  101745 | 1490 | `					ph7_class_attr *pAttr = 0;` |
|  101745 | 1491 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - | 1492 | `						/* unset(C::$x): PHP rejects unsetting a static property with a fatal Error.` |
|       - | 1493 | `						 * Without this the iP2=unset tag falls through to a normal static read and the` |
|       - | 1494 | `						 * trailing generic unset() would silently NULL (and de-type) the shared slot. */` |
|       - | 1495 | `						char zMsg[256];` |
|     ! 0 | 1496 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Attempt to unset static property %.*s::$%.*s",` |
|     ! 0 | 1497 | `							(int)pClass->sName.nByte,pClass->sName.zString,(int)sName.nByte,sName.zString);` |
|     ! 0 | 1498 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|     ! 0 | 1499 | `						VM_EXIT_ABORT;` |
|       - | 1500 | `					}` |
|       - | 1501 | `					/* Check for special ::class pseudo-constant */` |
|  101868 | 1502 | `					if( sName.nByte == sizeof("class")-1 &&` |
|     246 | 1503 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|       - | 1504 | `						/* ::class returns the fully qualified class name */` |
|       - | 1505 | `						/* Pop the attribute name from the stack */` |
|      88 | 1506 | `						if( !pInstr->p3 ){` |
|      88 | 1507 | `							VmPopOperand(&pTos,1);` |
|      42 | 1508 | `						}` |
|      88 | 1509 | `						PH7_MemObjRelease(pTos);` |
|       - | 1510 | `						/* Load the class name */` |
|      88 | 1511 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      88 | 1512 | `						pTos->nIdx = SXU32_HIGH;` |
|      46 | 1513 | `					}else{` |
|       - | 1514 | `						/* Extract the target attribute. php keeps constants and` |
|       - | 1515 | `						 * (static) properties in separate namespaces; the source` |
|       - | 1516 | ``						 * form disambiguates (set by the `::` codegen, compile.c):`` |
|       - | 1517 | ``						 * a `$`-form is a STATIC PROPERTY (hAttr) — either a literal`` |
|       - | 1518 | ``						 * name folded into pInstr->p3 (`C::$s`) or a dynamic name on`` |
|       - | 1519 | ``						 * the stack marked iP1==2 (`C::$$x`, `C::${$e}`); a bareword`` |
|       - | 1520 | `						 * (p3==0, iP1==1) is a CONSTANT or enum case (hConst). This` |
|       - | 1521 | ``						 * is what lets `const C` and `public $C` coexist and resolve`` |
|       - | 1522 | `						 * to the right member. */` |
|  101661 | 1523 | `						if( sName.nByte > 0 ){` |
|  102175 | 1524 | `							pAttr = (pInstr->p3 \|\| pInstr->iP1 == 2)` |
|  100634 | 1525 | `								? PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte)` |
|   51850 | 1526 | `								: PH7_ClassExtractConstant(pClass,sName.zString,sName.nByte);` |
|   50828 | 1527 | `						}` |
|  101661 | 1528 | `						if( pAttr == 0 ){` |
|       - | 1529 | `							/* No such member. php raises a catchable Error whose wording` |
|       - | 1530 | `							 * depends on the ACCESS form (the same p3/iP1 signal used above):` |
|       - | 1531 | ``							 * a bareword `C::MISSING` (constant form) is "Undefined constant`` |
|       - | 1532 | ``							 * C::MISSING", a `$`-form `C::$missing` is "Access to undeclared`` |
|       - | 1533 | `							 * static property C::$missing". Instance magic (__get) is never` |
|       - | 1534 | `							 * consulted for statics (band A #3b). isset()/empty() context` |
|       - | 1535 | `							 * stays silently false. Parked on the boundary rail; the op` |
|       - | 1536 | `							 * completes benignly with NULL and the fetch-point router lands` |
|       - | 1537 | `							 * the throw. */` |
|      14 | 1538 | `							if( !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1539 | `								SyBlob sErrMsg;` |
|      14 | 1540 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      14 | 1541 | `								if( pInstr->p3 == 0 && pInstr->iP1 != 2 ){` |
|       7 | 1542 | `									SyBlobFormat(&sErrMsg,"Undefined constant %z::%z",` |
|       3 | 1543 | `										&pClass->sName,&sName);` |
|       4 | 1544 | `								}else{` |
|       8 | 1545 | `									SyBlobFormat(&sErrMsg,"Access to undeclared static property %z::$%z",` |
|       3 | 1546 | `										&pClass->sName,&sName);` |
|       - | 1547 | `								}` |
|      14 | 1548 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       6 | 1549 | `							}` |
|       6 | 1550 | `						}` |
|       - | 1551 | `						/* Pop the attribute name from the stack */` |
|  101661 | 1552 | `						if( !pInstr->p3 ){` |
|    1033 | 1553 | `							VmPopOperand(&pTos,1);` |
|     514 | 1554 | `						}` |
|  101661 | 1555 | `						PH7_MemObjRelease(pTos);` |
|  101661 | 1556 | `						pTos->nIdx = SXU32_HIGH;` |
|  101661 | 1557 | `						if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - | 1558 | ``							/* `self::$s =& $x` / `C::$s =& $x`: stash the static property slot`` |
|       - | 1559 | `							 * for the following member-marked OP_STORE_REF (class-level, shared` |
|       - | 1560 | `							 * across instances — matches php). Skip the read machinery below. */` |
|       2 | 1561 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|       3 | 1562 | `							 && PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|       3 | 1563 | `								pVm->pRefTargetStaticAttr = pAttr;` |
|       3 | 1564 | `								pVm->pRefTargetAttr = 0;` |
|       3 | 1565 | `								pVm->pRefTargetThis = 0;` |
|       3 | 1566 | `								pTos->nIdx = pAttr->nIdx;` |
|       2 | 1567 | `							}else{` |
|     ! 0 | 1568 | `								pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 1569 | `								pVm->pRefTargetAttr = 0;` |
|     ! 0 | 1570 | `								pVm->pRefTargetThis = 0;` |
|       - | 1571 | `							}` |
|       3 | 1572 | `							if( pThis ){` |
|     ! 0 | 1573 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1574 | `							}` |
|       3 | 1575 | `							VM_EXIT_BREAK;` |
|       - | 1576 | `						}` |
|  101659 | 1577 | `						if( pAttr ){` |
|  101647 | 1578 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       - | 1579 | `								/* Access to a non static attribute */` |
|     ! 0 | 1580 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|     ! 0 | 1581 | `									&pClass->sName,&pAttr->sName` |
|       - | 1582 | `									);` |
|     ! 0 | 1583 | `							}else{` |
|       - | 1584 | `								ph7_value *pValue;` |
|       - | 1585 | `								/* Deferred typed-static-default failure: php materializes the` |
|       - | 1586 | `								 * class's static table at the FIRST static-property access` |
|       - | 1587 | `								 * (any property, any context — read, write, even isset), so a` |
|       - | 1588 | `								 * bad default throws its catchable TypeError here. Constants` |
|       - | 1589 | `								 * and method calls do not trigger it (php-exact). */` |
|  101642 | 1590 | `								if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|  101134 | 1591 | `								 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|  100631 | 1592 | `								 && (pClass->iFlags & PH7_CLASS_STATIC_TYPE_DEFER) ){` |
|      15 | 1593 | `									sxi32 rcD = VmThrowDeferredStaticType(&(*pVm),pClass);` |
|      15 | 1594 | `									if( rcD != SXRET_OK ){` |
|      15 | 1595 | `										if( pThis ){` |
|     ! 0 | 1596 | `											PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1597 | `										}` |
|      15 | 1598 | `										if( rcD == PH7_ABORT ){` |
|     ! 0 | 1599 | `											VM_EXIT_ABORT;` |
|       - | 1600 | `										}` |
|       - | 1601 | `										{` |
|       - | 1602 | `											sxi32 iRpD;` |
|      15 | 1603 | `											if( VmRecordedResume(pVm,&iRpD,pState->pEntryFrame,aInstr) ){` |
|      31 | 1604 | `												PH7_RESUME_DRAIN()` |
|      15 | 1605 | `												pc = iRpD;` |
|      15 | 1606 | `												VM_EXIT_BREAK;` |
|       - | 1607 | `											}` |
|       - | 1608 | `										}` |
|     ! 0 | 1609 | `										VM_EXIT_EXCEPTION;` |
|       - | 1610 | `									}` |
|     ! 0 | 1611 | `								}` |
|       - | 1612 | `								/* Check if the access to the attribute is allowed */` |
|  101633 | 1613 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|       - | 1614 | `									/* PHP 7.4+: uninitialized typed static read.` |
|       - | 1615 | `									 * Same LHS-of-store peek as the instance path. */` |
|  101624 | 1616 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|  100867 | 1617 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|  150101 | 1618 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|  100064 | 1619 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|  100069 | 1620 | `										if( pS ){` |
|  100069 | 1621 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|  100069 | 1622 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|  100011 | 1623 | `												VmInstr *pNext = pInstr + 1;` |
|  100011 | 1624 | `												int bIsLhs = 0;` |
|  100011 | 1625 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       6 | 1626 | `													bIsLhs = 1;` |
|       2 | 1627 | `												}` |
|  100011 | 1628 | `												if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 1629 | `													/* Destructuring target ([S::$s] = [...]):` |
|       - | 1630 | `													 * initialized by the OP_LOAD_LIST store. */` |
|       3 | 1631 | `													bIsLhs = 1;` |
|       1 | 1632 | `												}` |
|  100011 | 1633 | `												if( !bIsLhs ){` |
|  100004 | 1634 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|  100004 | 1635 | `													if( pThis ){` |
|     ! 0 | 1636 | `														PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1637 | `													}` |
|  100004 | 1638 | `													if( rcU == PH7_ABORT ){` |
|     ! 0 | 1639 | `														VM_EXIT_ABORT;` |
|       - | 1640 | `													}` |
|       - | 1641 | `													{` |
|       - | 1642 | `														sxi32 iRp;` |
|  100004 | 1643 | `														if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|  200006 | 1644 | `															PH7_RESUME_DRAIN()` |
|  100004 | 1645 | `															pc = iRp;` |
|  100004 | 1646 | `															VM_EXIT_BREAK;` |
|       - | 1647 | `														}` |
|       - | 1648 | `													}` |
|     ! 0 | 1649 | `													VM_EXIT_EXCEPTION;` |
|       - | 1650 | `												}` |
|       3 | 1651 | `											}` |
|      31 | 1652 | `										}` |
|      31 | 1653 | `									}` |
|    1622 | 1654 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|    1322 | 1655 | `									 && SySetUsed(&pAttr->aAttrs) > 0 ){` |
|       - | 1656 | `										/* php 8.4 #[\Deprecated] on a class constant:` |
|       - | 1657 | `										 * every access re-warns. */` |
|      11 | 1658 | `										VmDeprecatedConstNotice(&(*pVm),pClass,pAttr);` |
|       5 | 1659 | `									}` |
|    1627 | 1660 | `									if( pAttr->nIdx == SXU32_HIGH ){` |
|       - | 1661 | `										/* Unmaterialized slot. Enum case: first access` |
|       - | 1662 | `										 * materializes ALL the singletons. Plain constant: its` |
|       - | 1663 | `										 * initializer hasn't run yet (mount-order-dependent` |
|       - | 1664 | `										 * cross-constant reference) — evaluate on demand. A` |
|       - | 1665 | `										 * raised TypeError/Error (backing mismatch, duplicate` |
|       - | 1666 | `										 * value, self-reference) parks on the boundary rail;` |
|       - | 1667 | `										 * the op completes benignly with NULL and the` |
|       - | 1668 | `										 * fetch-point router lands the throw. */` |
|       - | 1669 | `										sxi32 rcEnum;` |
|     347 | 1670 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|       - | 1671 | `											/* php: a DIRECT static access evaluates every case` |
|       - | 1672 | `											 * of the enum (whole-class constant update) — a` |
|       - | 1673 | `											 * broken sibling case throws here too. A reference` |
|       - | 1674 | `											 * from inside another constant's initializer` |
|       - | 1675 | `											 * (nConstEvalDepth > 0) evaluates only the` |
|       - | 1676 | `											 * requested case. */` |
|      31 | 1677 | `											if( pVm->nConstEvalDepth > 0 ){` |
|       3 | 1678 | `												rcEnum = VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|       2 | 1679 | `											}else{` |
|      29 | 1680 | `												rcEnum = VmEnumMaterialize(&(*pVm),pClass);` |
|       - | 1681 | `											}` |
|      17 | 1682 | `										}else{` |
|     319 | 1683 | `											rcEnum = VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|       - | 1684 | `										}` |
|     347 | 1685 | `										if( rcEnum != SXRET_OK ){` |
|       7 | 1686 | `											VmBoundaryPark(&(*pVm),rcEnum);` |
|       3 | 1687 | `										}` |
|     171 | 1688 | `									}` |
|       - | 1689 | `									/* Load the desired attribute */` |
|    1627 | 1690 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|    1627 | 1691 | `									if( pValue ){` |
|    1621 | 1692 | `										PH7_MemObjLoad(pValue,pTos);` |
|    1621 | 1693 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       - | 1694 | `											/* Load index number */` |
|     615 | 1695 | `											pTos->nIdx = pAttr->nIdx;` |
|     305 | 1696 | `										}` |
|     808 | 1697 | `									}` |
|     816 | 1698 | `								}else{` |
|       - | 1699 | `									/* Throw Error exception (PHP-compatible) */` |
|       - | 1700 | `									char zMsg[256];` |
|       5 | 1701 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       5 | 1702 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|       7 | 1703 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|       4 | 1704 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|       4 | 1705 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|       3 | 1706 | `									}else{` |
|     ! 0 | 1707 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|     ! 0 | 1708 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|     ! 0 | 1709 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|       - | 1710 | `									}` |
|       5 | 1711 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|       5 | 1712 | `									VM_EXIT_ABORT;` |
|       - | 1713 | `								}` |
|       - | 1714 | `							}` |
|     811 | 1715 | `						}` |
|       - | 1716 | `					}` |
|       - | 1717 | `				}` |
|    2719 | 1718 | `				if( pThis ){` |
|       - | 1719 | `					/* Safely unreference the object */` |
|      11 | 1720 | `					PH7_ClassInstanceUnref(pThis);` |
|       5 | 1721 | `				}` |
|       - | 1722 | `			}` |
|    1362 | 1723 | `		}else{` |
|       - | 1724 | `			/* Pop operands */` |
|     ! 0 | 1725 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|     ! 0 | 1726 | `			if( !pInstr->p3 ){` |
|     ! 0 | 1727 | `				VmPopOperand(&pTos,1);` |
|     ! 0 | 1728 | `			}` |
|     ! 0 | 1729 | `			PH7_MemObjRelease(pTos);` |
|     ! 0 | 1730 | `			pTos->nIdx = SXU32_HIGH;` |
|       - | 1731 | `		}` |
|       - | 1732 | `	}` |
| 3135845 | 1733 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1734 | `	VM_EXIT_BREAK;` |
| 1618227 | 1735 | `}` |
|       - | 1736 |  |
|       - | 1737 | `/*` |
|       - | 1738 | ` * OP_CLONE: body moved verbatim from the OP_CLONE arm of` |
|       - | 1739 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1740 | ` */` |
|     216 | 1741 | `PH7_PRIVATE VmOpRc VmExecOpClone(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       4 | 1742 | `{` |
|     220 | 1743 | `	ph7_value *pTos = pState->pTos;` |
|     220 | 1744 | `	ph7_value *pStack = pState->pStack;` |
|     220 | 1745 | `	VmInstr *aInstr = pState->aInstr;` |
|     220 | 1746 | `	sxi32 pc = pState->pc;` |
|       - | 1747 | `	sxi32 rc;` |
|     108 | 1748 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 1749 | `	ph7_class_instance *pSrc,*pClone;` |
|       - | 1750 | `#ifdef UNTRUST` |
|       - | 1751 | `	if( pTos < pStack ){` |
|       - | 1752 | `		VM_EXIT_ABORT;` |
|       - | 1753 | `	}` |
|       - | 1754 | `#endif` |
|       - | 1755 | `	/* Make sure we are dealing with a class instance. PHP 8 throws a catchable` |
|       - | 1756 | ``	 * TypeError for a non-object operand — for both `clone $x` and clone($x). */`` |
|     220 | 1757 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - | 1758 | `		SyBlob sMsg;` |
|       7 | 1759 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       7 | 1760 | `		SyBlobFormat(&sMsg,"clone(): Argument #1 ($object) must be of type object, %s given",` |
|       3 | 1761 | `			ph7_type_name(pTos));` |
|       7 | 1762 | `		rc = VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|       7 | 1763 | `		PH7_MemObjRelease(pTos);` |
|       7 | 1764 | `		pTos->nIdx = SXU32_HIGH;` |
|       7 | 1765 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 1766 | `			VM_EXIT_ABORT;` |
|       - | 1767 | `		}` |
|       - | 1768 | `		{` |
|       - | 1769 | `			sxi32 iRp;` |
|       7 | 1770 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|       7 | 1771 | `				pc = iRp;` |
|       7 | 1772 | `				VM_EXIT_BREAK;` |
|       - | 1773 | `			}` |
|       - | 1774 | `		}` |
|     ! 0 | 1775 | `		VM_EXIT_EXCEPTION;` |
|       - | 1776 | `	}` |
|       - | 1777 | `	/* Point to the source */` |
|     214 | 1778 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|       - | 1779 | `	/* Enum cases are not cloneable — php's catchable Error (the singleton` |
|       - | 1780 | `	 * identity would break). */` |
|     214 | 1781 | `	if( pSrc->pClass->iFlags & PH7_CLASS_ENUM ){` |
|       - | 1782 | `		SyBlob sMsg;` |
|       3 | 1783 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 | 1784 | `		SyBlobFormat(&sMsg,"Trying to clone an uncloneable object of class %z",` |
|       2 | 1785 | `			&pSrc->pClass->sName);` |
|       3 | 1786 | `		rc = VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       3 | 1787 | `		PH7_MemObjRelease(pTos);` |
|       3 | 1788 | `		pTos->nIdx = SXU32_HIGH;` |
|       3 | 1789 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 1790 | `			VM_EXIT_ABORT;` |
|       - | 1791 | `		}` |
|       - | 1792 | `		{` |
|       - | 1793 | `			sxi32 iRp;` |
|       3 | 1794 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|     ! 0 | 1795 | `				pc = iRp;` |
|     ! 0 | 1796 | `				VM_EXIT_BREAK;` |
|       - | 1797 | `			}` |
|       - | 1798 | `		}` |
|       3 | 1799 | `		VM_EXIT_EXCEPTION;` |
|       - | 1800 | `	}` |
|       - | 1801 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|     212 | 1802 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|     ! 0 | 1803 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - | 1804 | `			"Trying to clone an uncloneable object of class '%z'",` |
|     ! 0 | 1805 | `			&pSrc->pClass->sName);` |
|     ! 0 | 1806 | `		PH7_MemObjRelease(pTos);` |
|     ! 0 | 1807 | `		VM_EXIT_BREAK;` |
|       - | 1808 | `	}` |
|       - | 1809 | `	/* Perform the clone operation */` |
|     212 | 1810 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|     212 | 1811 | `	PH7_MemObjRelease(pTos);` |
|     212 | 1812 | `	if( pClone == 0 ){` |
|     ! 0 | 1813 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 1814 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|     ! 0 | 1815 | `	}else{` |
|       - | 1816 | `		/* Load the cloned object */` |
|     212 | 1817 | `		pTos->x.pOther = pClone;` |
|     212 | 1818 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - | 1819 | `	}` |
|     212 | 1820 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1821 | `	VM_EXIT_BREAK;` |
|     112 | 1822 | `}` |
|       - | 1823 |  |
