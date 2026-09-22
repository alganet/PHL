# src/ph7/vm_ops_oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1045/1213 lines (86.15%)

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
| 1107276 |  137 | `PH7_PRIVATE VmOpRc VmExecOpNew(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  138 | `{` |
| 1107281 |  139 | `	ph7_value *pTos = pState->pTos;` |
| 1107281 |  140 | `	ph7_value *pStack = pState->pStack;` |
| 1107281 |  141 | `	VmInstr *aInstr = pState->aInstr;` |
| 1107281 |  142 | `	sxi32 pc = pState->pc;` |
|       - |  143 | `	sxi32 rc;` |
|       - |  144 | `	/* Constructor arg count: compile-time args plus THIS new's own unpack` |
|       - |  145 | `	 * expansion (iP2 = hasSpread, transferred from the popped OP_CALL —` |
|       - |  146 | ``	 * `new C(...$args)` used to ignore the extras, leaving the expanded`` |
|       - |  147 | `	 * elements ABOVE the class-name slot and fataling "Class ' ' is not` |
|       - |  148 | `	 * defined"). VmSpreadOwnExtra counts only this new's own runs, so a nested` |
|       - |  149 | `	 * spread call in the ctor arg list stays scoped to itself. */` |
| 1107281 |  150 | `	sxi32 nCtorArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|       - |  151 | `	ph7_value *pArg;` |
| 1107281 |  152 | `	ph7_class *pClass = 0;` |
|       - |  153 | `	ph7_class_instance *pNew;` |
|       - |  154 | `	/* Resolving the class below can run an AUTOLOADER that throws. Snapshot the boundary` |
|       - |  155 | `	 * rail so the "not found" report can tell a missing class from an autoloader that` |
|       - |  156 | `	 * raised — php propagates the autoloader's exception and reports nothing else, while` |
|       - |  157 | `	 * this arm used to throw its own Error on top of it (uncaught, killing the script` |
|       - |  158 | `	 * right after the real exception had been handled). */` |
| 1107281 |  159 | `	sxi32 nNewBrc = pVm->nBoundaryRc;` |
| 1107281 |  160 | `	const void *pNewRes = (const void *)pVm->pResumeFrame;` |
| 1107281 |  161 | `	pArg = &pTos[-nCtorArgs]; /* Constructor arguments (if available) */` |
|       - |  162 | `	/* Same PHP 8.1 spread-key realignment as OP_CALL: build a per-actual-slot name` |
|       - |  163 | `	 * map when the ctor arg list unpacked string-keyed elements. NEW keeps the` |
|       - |  164 | `	 * class-name slot at pTos (above the args), so pArg is already the correct base;` |
|       - |  165 | `	 * the build also truncates this call's captured runs. */` |
|       - |  166 | `	VmCallArgMap sEffNewMap;` |
| 1660919 |  167 | `	VmCallArgMap *pEffNewMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
| 1107276 |  168 | `		nCtorArgs > 0 ? (sxu32)nCtorArgs : 0,&sEffNewMap);` |
| 1660919 |  169 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
| 1107281 |  170 | `		const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
| 1107281 |  171 | `		sxu32 nCls = SyBlobLength(&pTos->sBlob);` |
| 1107276 |  172 | `		if( (nCls == sizeof("self")-1 && SyMemcmp(zCls,"self",sizeof("self")-1) == 0)` |
| 1107268 |  173 | `		 \|\| (nCls == sizeof("static")-1 && SyMemcmp(zCls,"static",sizeof("static")-1) == 0)` |
| 1107235 |  174 | `		 \|\| (nCls == sizeof("parent")-1 && SyMemcmp(zCls,"parent",sizeof("parent")-1) == 0) ){` |
|       - |  175 | `			/* new self() / new static() / new parent(): resolve against the live` |
|       - |  176 | `			 * class context (LSB for static), sharing the FCC resolver. */` |
|      50 |  177 | `			pClass = VmFccResolveScope(&(*pVm),pTos);` |
|      50 |  178 | `			if( pClass && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) ){` |
|       - |  179 | `				/* Not new-able (the named path's iLoadable extract excludes these` |
|       - |  180 | `				 * before it ever gets here): php's wording, with the RESOLVED name. */` |
|     ! 0 |  181 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot instantiate %s %z",` |
|     ! 0 |  182 | `					(pClass->iFlags & PH7_CLASS_INTERFACE) ? "interface" : "abstract class",` |
|     ! 0 |  183 | `					&pClass->sName);` |
|     ! 0 |  184 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 |  185 | `				if( nCtorArgs > 0 ){` |
|     ! 0 |  186 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  187 | `				}` |
|     ! 0 |  188 | `				VM_EXIT_ABORT;` |
|       - |  189 | `			}` |
|      26 |  190 | `		}else{` |
|       - |  191 | `			/* Try to extract the desired class */` |
| 1107233 |  192 | `			pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,` |
|       - |  193 | `				TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|       5 |  194 | `		}` |
|  553638 |  195 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - |  196 | `		/* Take the base class from the loaded instance */` |
|     ! 0 |  197 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|     ! 0 |  198 | `	}` |
| 1107281 |  199 | `	if( pClass == 0 ){` |
|       - |  200 | ``		/* php: `new NoSuch()` is a CATCHABLE Error ('Class "NoSuch" not found'), not a`` |
|       - |  201 | `		 * hard stop. Aborting here killed the script outright -- and with exit status 0,` |
|       - |  202 | `		 * so a caller could not even tell it had failed. */` |
|       - |  203 | `		SyBlob sErrM;` |
|       - |  204 | `		sxi32 rcErr;` |
|      25 |  205 | `		ph7_class *pNotNew = 0;` |
|      25 |  206 | `		if( PH7_VmClassLookupRaised(&(*pVm),nNewBrc,pNewRes) ){` |
|       - |  207 | `			/* The autoloader threw: land THAT (an in-place catch leaves 0 behind plus a` |
|       - |  208 | `			 * recorded resume frame, which the router picks up). */` |
|       5 |  209 | `			sxi32 rcAuto = pVm->nBoundaryRc;` |
|       5 |  210 | `			pVm->nBoundaryRc = 0;` |
|       5 |  211 | `			if( nCtorArgs > 0 ){` |
|       3 |  212 | `				VmPopOperand(&pTos,nCtorArgs);` |
|       1 |  213 | `			}` |
|       5 |  214 | `			PH7_MemObjRelease(pTos);` |
|       5 |  215 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 |  216 | `			pTos->nIdx = SXU32_HIGH;` |
|       5 |  217 | `			if( rcAuto == PH7_ABORT ){` |
|     ! 0 |  218 | `				VM_EXIT_ABORT;` |
|       - |  219 | `			}` |
|       5 |  220 | `			rc = PH7_EXCEPTION;` |
|       5 |  221 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  222 | `		}` |
|      21 |  223 | `		SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      21 |  224 | `		if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       - |  225 | `			/* The extract above only accepts NEW-able classes, so an interface or an` |
|       - |  226 | `			 * abstract class comes back as 0 and used to be reported as "not found".` |
|       - |  227 | `			 * Look again without that filter so php's real message can be given. */` |
|      29 |  228 | `			pNotNew = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|      16 |  229 | `				SyBlobLength(&pTos->sBlob),FALSE,0);` |
|       8 |  230 | `		}` |
|      21 |  231 | `		if( pNotNew && (pNotNew->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) ){` |
|       5 |  232 | `			SyBlobFormat(&sErrM,"Cannot instantiate %s %z",` |
|       4 |  233 | `				(pNotNew->iFlags & PH7_CLASS_INTERFACE) ? "interface" : "abstract class",` |
|       2 |  234 | `				&pNotNew->sName);` |
|       3 |  235 | `		}else{` |
|      17 |  236 | `			SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|      12 |  237 | `				SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob));` |
|       - |  238 | `		}` |
|       - |  239 | `		/* Settle the stack BEFORE throwing: the class-name slot becomes the (NULL)` |
|       - |  240 | `		 * expression result and the ctor arguments go. */` |
|      21 |  241 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  242 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  243 | `		}` |
|      21 |  244 | `		PH7_MemObjRelease(pTos);` |
|      21 |  245 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|      21 |  246 | `		pTos->nIdx = SXU32_HIGH;` |
|      29 |  247 | `		rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       8 |  248 | `			SyBlobLength(&sErrM));` |
|      21 |  249 | `		SyBlobRelease(&sErrM);` |
|      21 |  250 | `		if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      21 |  251 | `		rc = rcErr;` |
|      29 |  252 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
| 1107261 |  253 | `	}else if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|       - |  254 | `		/* php 8.1: enums cannot be instantiated — a catchable Error, raised` |
|       - |  255 | `		 * BEFORE any construction (no instance, no __destruct). */` |
|       - |  256 | `		SyBlob sErrMsg;` |
|       3 |  257 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 |  258 | `		SyBlobFormat(&sErrMsg,"Cannot instantiate enum %z",&pClass->sName);` |
|       3 |  259 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       3 |  260 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  261 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  262 | `		}` |
|       3 |  263 | `		PH7_MemObjRelease(pTos);` |
|       3 |  264 | `		pTos->nIdx = SXU32_HIGH;` |
|       3 |  265 | `		VM_EXIT_BREAK;` |
| 1107254 |  266 | `	}else if( VmClassStaticDeferPending(pClass)` |
|  553634 |  267 | `	       && (rc = PH7_VmMaterializeClassStatics(&(*pVm),pClass)) != SXRET_OK ){` |
|       - |  268 | `		/* php materializes the static table at instantiation too, so a default` |
|       - |  269 | `		 * that THREW at the declaration (or failed its type check) raises BEFORE` |
|       - |  270 | `		 * any construction (no instance, no __destruct) — same shape as the enum` |
|       - |  271 | `		 * reject above: park the status, settle the stack, and let the` |
|       - |  272 | `		 * fetch-point router land it. */` |
|       6 |  273 | `		VmBoundaryPark(&(*pVm),rc);` |
|       6 |  274 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  275 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  276 | `		}` |
|       6 |  277 | `		PH7_MemObjRelease(pTos);` |
|       6 |  278 | `		pTos->nIdx = SXU32_HIGH;` |
|       6 |  279 | `		VM_EXIT_BREAK;` |
|     ! 0 |  280 | `	}else{` |
|       - |  281 | `		ph7_class_method *pCons;` |
|       - |  282 | `		/* Check if a constructor is available — BEFORE instantiation: a` |
|       - |  283 | ``		 * visibility-denied `new` must not construct (nor later destruct)`` |
|       - |  284 | `		 * the object (band A #4). */` |
|       - |  285 | `		/* Only an explicit __construct is the constructor. PHP-4-style class-name` |
|       - |  286 | `		 * constructors were removed in PHP 8.0 — a method named like the class is a` |
|       - |  287 | `		 * plain method, so no same-name fallback here. */` |
| 1107255 |  288 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       - |  289 | `		/* Constructor visibility (band A #4): __construct now KEEPS its` |
|       - |  290 | `		 * declared protection (PH7_NewClassMethod no longer forces it` |
|       - |  291 | ``		 * public), so `new C()` on a private/protected constructor from`` |
|       - |  292 | `		 * the wrong scope is php's catchable Error. Reflection's` |
|       - |  293 | `		 * newInstance path sets bReflectBypass like method invoke. */` |
| 1107255 |  294 | `		if( pCons && pCons->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - |  295 | `			/* php binds non-public access by the member's DECLARING class, not the` |
|       - |  296 | `			 * instantiated class: a private constructor declared in a base is` |
|       - |  297 | `			 * reachable from that base's own methods even when instantiating a` |
|       - |  298 | ``			 * subclass (`new Child()` inside Base::factory()). __construct is a`` |
|       - |  299 | `			 * method, so pass its declaring class (sFunc.pUserData) — mirroring the` |
|       - |  300 | `			 * method-call visibility check — rather than pClass, whose hAttr lookup` |
|       - |  301 | `			 * for a method name always misses and falls to a wrong exact-class test. */` |
|      23 |  302 | `			ph7_class *pCtorDecl = pCons->sFunc.pUserData ? (ph7_class *)pCons->sFunc.pUserData : pClass;` |
|      23 |  303 | `			if( pVm->bReflectBypass ){` |
|     ! 0 |  304 | `				pVm->bReflectBypass = 0;` |
|      23 |  305 | `			}else if( !PH7_VmClassMemberAccess(&(*pVm),pCtorDecl,&pCons->sFunc.sName,pCons->iProtection,FALSE) ){` |
|       - |  306 | `				SyBlob sErrMsg;` |
|       7 |  307 | `				const char *zVis = pCons->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       7 |  308 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       7 |  309 | `				SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from global scope",` |
|       3 |  310 | `					zVis,&pClass->sName);` |
|       7 |  311 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       - |  312 | `				/* Pop ctor args + release the class-name operand, leave NULL` |
|       - |  313 | `				 * as the expression value; the fetch-point router lands the` |
|       - |  314 | `				 * throw. No instance was created. */` |
|       7 |  315 | `				if( nCtorArgs > 0 ){` |
|     ! 0 |  316 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  317 | `				}` |
|       7 |  318 | `				PH7_MemObjRelease(pTos);` |
|       7 |  319 | `				pTos->nIdx = SXU32_HIGH;` |
|       7 |  320 | `				VM_EXIT_BREAK;` |
|       - |  321 | `			}` |
|       8 |  322 | `		}` |
|       - |  323 | `		/* Create a new class instance */` |
| 1107249 |  324 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
| 1107249 |  325 | `		if( pNew == 0 ){` |
|     ! 0 |  326 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  327 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|     ! 0 |  328 | `				&pClass->sName` |
|       - |  329 | `			);` |
|     ! 0 |  330 | `			PH7_MemObjRelease(pTos);` |
|     ! 0 |  331 | `			if( nCtorArgs > 0 ){` |
|       - |  332 | `				/* Pop given arguments */` |
|     ! 0 |  333 | `				VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  334 | `			}` |
|     ! 0 |  335 | `			VM_EXIT_BREAK;` |
|       - |  336 | `		}` |
| 1107249 |  337 | `		if( pVm->nBoundaryRc != 0 ){` |
|       - |  338 | `			/* A typed property DEFAULT failed its type check during instance-` |
|       - |  339 | `			 * frame creation (parked catchable TypeError): php aborts the` |
|       - |  340 | `			 * construction — no constructor call, no object. Consume the` |
|       - |  341 | `			 * parked status here and route exactly like a constructor throw` |
|       - |  342 | `			 * (the exception object is already registered). */` |
|      35 |  343 | `			sxi32 rcDef = pVm->nBoundaryRc;` |
|       - |  344 | `			sxi32 iDefResumePc;` |
|      35 |  345 | `			pVm->nBoundaryRc = 0;` |
|      35 |  346 | `			PH7_ClassInstanceUnref(pNew);` |
|      35 |  347 | `			if( rcDef == PH7_ABORT ){` |
|     ! 0 |  348 | `				VM_EXIT_ABORT;` |
|       - |  349 | `			}` |
|      35 |  350 | `			if( VmRecordedResume(pVm,&iDefResumePc,pState->pEntryFrame,aInstr) ){` |
|       - |  351 | `				/* This frame's own try caught it in-place: tidy the stack` |
|       - |  352 | `				 * (pop ctor args + release the class-name slot, then drain any` |
|       - |  353 | `				 * abandoned outer-expression operands to the try's base — the` |
|       - |  354 | `				 * class-name slot itself sits above it) and resume. */` |
|      35 |  355 | `				if( nCtorArgs > 0 ){` |
|       3 |  356 | `					VmPopOperand(&pTos,nCtorArgs);` |
|       1 |  357 | `				}` |
|      35 |  358 | `				PH7_MemObjRelease(pTos);` |
|      71 |  359 | `				PH7_RESUME_DRAIN()` |
|      35 |  360 | `				pc = iDefResumePc;` |
|      35 |  361 | `				VM_EXIT_BREAK;` |
|       - |  362 | `			}` |
|     ! 0 |  363 | `			VM_EXIT_EXCEPTION;` |
|       - |  364 | `		}` |
| 1107215 |  365 | `		if( pCons ){` |
|       - |  366 | `			/* Call the class constructor.  Collect args in stack order and` |
|       - |  367 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|       - |  368 | `			 * receiving OP_CALL path runs its named-argument matching` |
|       - |  369 | `			 * (including variadic string-key packing). */` |
| 1103807 |  370 | `			VmCallArgMap *pNewMap = pEffNewMap;` |
|       - |  371 | `			sxi32 rcCons;` |
|       - |  372 | `			SySet aArg; /* ctor argument scratch (the loop's shared set stays with OP_CALL) */` |
| 1103807 |  373 | `			SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
| 2109071 |  374 | `			while( pArg < pTos ){` |
| 1005269 |  375 | `				SySetPut(&aArg,(const void *)&pArg);` |
| 1005269 |  376 | `				pArg++;` |
|       5 |  377 | `			}` |
|       - |  378 | `			/* Too-few-arguments is php's catchable ArgumentCountError, raised` |
|       - |  379 | `			 * by the shared OP_CALL install path this ctor call routes through` |
|       - |  380 | `			 * (was a PHL-only notice here). */` |
| 1103807 |  381 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
| 1103807 |  382 | `			SySetRelease(&aArg); /* scratch dies here, on every exit path */` |
|       - |  383 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
| 1103807 |  384 | `			if( pNew->iRef < 1 ){` |
|     ! 0 |  385 | `				pNew->iRef = 1;` |
|     ! 0 |  386 | `			}` |
| 1103807 |  387 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|       - |  388 | `				/* The constructor raised: the half-constructed object must not` |
|       - |  389 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|       - |  390 | `				 * The class-name operand (and any leftover args) are released by` |
|       - |  391 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|       - |  392 | `				sxi32 iResumePc;` |
|  100109 |  393 | `				PH7_ClassInstanceUnref(pNew);` |
|  100109 |  394 | `				if( rcCons == PH7_ABORT ){` |
|       5 |  395 | `					VM_EXIT_ABORT;` |
|       - |  396 | `				}` |
|  100104 |  397 | `				if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){` |
|       - |  398 | `					/* This frame's own try caught it in-place: tidy the stack` |
|       - |  399 | `					 * (pop ctor args + release the class-name slot, then drain any` |
|       - |  400 | `					 * abandoned outer-expression operands to the try's base — the` |
|       - |  401 | `					 * class-name slot itself sits above it) and resume. */` |
|  100100 |  402 | `					if( nCtorArgs > 0 ){` |
|      91 |  403 | `						VmPopOperand(&pTos,nCtorArgs);` |
|      45 |  404 | `					}` |
|  100100 |  405 | `					PH7_MemObjRelease(pTos);` |
|  500198 |  406 | `					PH7_RESUME_DRAIN()` |
|  100100 |  407 | `					pc = iResumePc;` |
|  100100 |  408 | `					VM_EXIT_BREAK;` |
|       - |  409 | `				}` |
|       5 |  410 | `				VM_EXIT_EXCEPTION;` |
|       - |  411 | `			}` |
|  501848 |  412 | `		}` |
| 1007109 |  413 | `		if( nCtorArgs > 0 ){` |
|       - |  414 | `			/* Pop given arguments */` |
| 1003483 |  415 | `			VmPopOperand(&pTos,nCtorArgs);` |
|  501739 |  416 | `		}` |
| 1007109 |  417 | `		PH7_MemObjRelease(pTos);` |
| 1007109 |  418 | `		pTos->x.pOther = pNew;` |
| 1007109 |  419 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - |  420 | `	}` |
| 1007109 |  421 | `	VM_EXIT_BREAK;` |
|     ! 0 |  422 | `	VM_EXIT_BREAK;` |
|  553643 |  423 | `}` |
|       - |  424 |  |
|       - |  425 | `/*` |
|       - |  426 | ` * OP_MEMBER: body moved verbatim from the OP_MEMBER arm of` |
|       - |  427 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  428 | ` */` |
| 3247054 |  429 | `PH7_PRIVATE VmOpRc VmExecOpMember(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  430 | `{` |
| 3247059 |  431 | `	ph7_value *pTos = pState->pTos;` |
| 3247059 |  432 | `	ph7_value *pStack = pState->pStack;` |
| 3247059 |  433 | `	VmInstr *aInstr = pState->aInstr;` |
| 3247059 |  434 | `	sxi32 pc = pState->pc;` |
|       - |  435 | `	sxi32 rc;` |
|       - |  436 | `	ph7_class_instance *pThis;` |
|       - |  437 | `	ph7_value *pNos;` |
|       - |  438 | `	SyString sName;` |
| 3247059 |  439 | `	if( !pInstr->iP1 ){` |
| 3144083 |  440 | `		pNos = &pTos[-1];` |
|       - |  441 | `#ifdef UNTRUST` |
|       - |  442 | `		if( pNos < pStack ){` |
|       - |  443 | `			VM_EXIT_ABORT;` |
|       - |  444 | `		}` |
|       - |  445 | `#endif` |
| 3144078 |  446 | `		if( pInstr->iP2 == PH7_MEMBER_DEFPATH` |
| 1576435 |  447 | `		 && (pNos->iFlags & (MEMOBJ_AUX_DEFPATH\|MEMOBJ_AUX_DEFERRED)) ){` |
|       - |  448 | `			/* D1 commit 2: deferred property arg ($o->p) whose base is itself a deferred lvalue` |
|       - |  449 | ``			 * (a nested chain `$a["k"]->p`, or an undefined base object). Append a property step`` |
|       - |  450 | `			 * to the base's captured path; OP_CALL re-walks it. */` |
|       - |  451 | `			SyString sProp;` |
|      17 |  452 | `			VmDeferredPath *pPath = 0;` |
|      17 |  453 | `			SyStringInitFromBuf(&sProp,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      17 |  454 | `			if( pNos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|      17 |  455 | `				pPath = (VmDeferredPath *)pNos->x.pOther;` |
|      17 |  456 | `				VmDeferPathPushProp(pPath,&sProp);` |
|       9 |  457 | `			}else{` |
|       - |  458 | `				SyString sRootName;` |
|     ! 0 |  459 | `				SyStringInitFromBuf(&sRootName,(const char *)pNos->x.pOther,` |
|       - |  460 | `					pNos->x.pOther ? SyStrlen((const char *)pNos->x.pOther) : 0);` |
|     ! 0 |  461 | `				pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|     ! 0 |  462 | `				if( pPath ){` |
|     ! 0 |  463 | `					pNos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name */` |
|     ! 0 |  464 | `					pNos->x.pOther = pPath;` |
|     ! 0 |  465 | `					pNos->iFlags \|= MEMOBJ_AUX_DEFPATH;` |
|     ! 0 |  466 | `					pNos->nIdx = SXU32_HIGH;` |
|     ! 0 |  467 | `					VmDeferPathPushProp(pPath,&sProp);` |
|     ! 0 |  468 | `				}` |
|       - |  469 | `			}` |
|      17 |  470 | `			VmPopOperand(&pTos,1); /* drop the property name; the carrier stays as pTos */` |
|      17 |  471 | `			VM_EXIT_BREAK;` |
|       - |  472 | `		}` |
| 3144067 |  473 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - |  474 | `			ph7_class *pClass;` |
|       - |  475 | `			/* Class already instantiated */` |
| 3144055 |  476 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       - |  477 | `			/* Point to the instantiated class */` |
| 3144055 |  478 | `			pClass = pThis->pClass;` |
|       - |  479 | `			/* Extract attribute name first */` |
| 3144055 |  480 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
| 3144055 |  481 | `			if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - |  482 | `				/* Method call */` |
|  114443 |  483 | `				ph7_class_method *pMeth = 0;` |
|  114443 |  484 | `				if( sName.nByte > 0 ){` |
|       - |  485 | `					/* Extract the target method */` |
|  114443 |  486 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|   57219 |  487 | `				}` |
|  114443 |  488 | `				if( pMeth == 0 ){` |
|      17 |  489 | `					ph7_class_method *pCallMagic = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|      17 |  490 | `					if( pCallMagic ){` |
|       - |  491 | `						/* php: a missing method dispatches __call($name, $args) — the` |
|       - |  492 | `						 * args are only collected by the following OP_CALL, so stash the` |
|       - |  493 | `						 * receiver + class + original name and redirect the callee to` |
|       - |  494 | `						 * the hidden packing trampoline (band A #3b; pre-fix the name-` |
|       - |  495 | `						 * only call discarded everything and the call site failed with` |
|       - |  496 | `						 * "Invalid function name"). Stack: pop the method name, then` |
|       - |  497 | `						 * the receiver slot becomes the trampoline's callee name. */` |
|      15 |  498 | `						SyBlobReset(&pVm->sMagicCallName);` |
|      15 |  499 | `						SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);` |
|      15 |  500 | `						pThis->iRef++;` |
|      15 |  501 | `						pVm->pMagicCallThis = pThis;` |
|      15 |  502 | `						pVm->pMagicCallClass = pClass;` |
|      15 |  503 | `						VmPopOperand(&pTos,1);` |
|      15 |  504 | `						PH7_MemObjRelease(pTos);` |
|      15 |  505 | `						SyBlobAppend(&pTos->sBlob,"__phl_magic_call",sizeof("__phl_magic_call")-1);` |
|      15 |  506 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|       9 |  507 | `					}else{` |
|       - |  508 | `						{` |
|       - |  509 | `							/* php raises a catchable Error here; PH7 printed a notice and CARRIED ON` |
|       - |  510 | `							 * with NULL, so the call silently produced nothing. */` |
|       - |  511 | `							SyBlob sErrM;` |
|       - |  512 | `							sxi32 rcErr;` |
|       3 |  513 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       3 |  514 | `							SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",&pClass->sName,&sName);` |
|       3 |  515 | `							VmPopOperand(&pTos,1);` |
|       3 |  516 | `							PH7_MemObjRelease(pTos);` |
|       3 |  517 | `							pTos->nIdx = SXU32_HIGH;` |
|       4 |  518 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       1 |  519 | `								SyBlobLength(&sErrM));` |
|       3 |  520 | `							SyBlobRelease(&sErrM);` |
|       3 |  521 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  522 | `							rc = rcErr;` |
|       3 |  523 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  524 | `						}` |
|       - |  525 | `					}` |
|       9 |  526 | `				}else{` |
|  114429 |  527 | `					ph7_class_method *pDeniedCall = 0;` |
|  114424 |  528 | `					if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC` |
|   60307 |  529 | `					 && !PH7_VmClassMemberAccess(&(*pVm),` |
|    3090 |  530 | `						pMeth->sFunc.pUserData ? (ph7_class *)pMeth->sFunc.pUserData : pClass,` |
|    1545 |  531 | `						&sName,pMeth->iProtection,FALSE) ){` |
|      23 |  532 | `						int bRebound = 0;` |
|      23 |  533 | `						if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - |  534 | `							/* php: when the instance's class SHADOWS the caller's` |
|       - |  535 | `							 * private method (child redeclares private m), code in` |
|       - |  536 | `							 * the caller's class dispatches its OWN private m, not` |
|       - |  537 | `							 * the shadow. Rebind to the calling scope's method when` |
|       - |  538 | `							 * that scope declares a private one of this name. */` |
|      18 |  539 | `							VmFrame *pFrameLocal = VmSkipExceptionFrames(pVm->pFrame);` |
|      18 |  540 | `							ph7_vm_func *pCallerFunc = (ph7_vm_func *)pFrameLocal->pUserData;` |
|      18 |  541 | `							if( pCallerFunc && (pCallerFunc->iFlags & VM_FUNC_CLASS_METHOD) && pCallerFunc->pUserData ){` |
|       3 |  542 | `								ph7_class *pScope = (ph7_class *)pCallerFunc->pUserData;` |
|       3 |  543 | `								ph7_class_method *pOwn = PH7_ClassExtractMethod(pScope,sName.zString,sName.nByte);` |
|       2 |  544 | `								if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       3 |  545 | `								 && (ph7_class *)pOwn->sFunc.pUserData == pScope ){` |
|       3 |  546 | `									pMeth = pOwn;` |
|       3 |  547 | `									bRebound = 1;` |
|       1 |  548 | `								}` |
|       1 |  549 | `							}` |
|       8 |  550 | `						}` |
|      23 |  551 | `						if( !bRebound ){` |
|       - |  552 | `							/* Inaccessible from this scope: php routes through __call when` |
|       - |  553 | `							 * declared (band A #3b); without it OP_CALL raises its` |
|       - |  554 | `							 * "Call to private/protected method" Error as before. */` |
|      21 |  555 | `							pDeniedCall = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|       9 |  556 | `						}` |
|      10 |  557 | `					}` |
|  114429 |  558 | `					if( pDeniedCall ){` |
|       5 |  559 | `						SyBlobReset(&pVm->sMagicCallName);` |
|       5 |  560 | `						SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);` |
|       5 |  561 | `						pThis->iRef++;` |
|       5 |  562 | `						pVm->pMagicCallThis = pThis;` |
|       5 |  563 | `						pVm->pMagicCallClass = pClass;` |
|       5 |  564 | `						VmPopOperand(&pTos,1);` |
|       5 |  565 | `						PH7_MemObjRelease(pTos);` |
|       5 |  566 | `						SyBlobAppend(&pTos->sBlob,"__phl_magic_call",sizeof("__phl_magic_call")-1);` |
|       5 |  567 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|       3 |  568 | `					}else{` |
|       - |  569 | `						/* Push method name on the stack */` |
|  114425 |  570 | `						PH7_MemObjRelease(pTos);` |
|  114425 |  571 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|  114425 |  572 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|       - |  573 | `					}` |
|       - |  574 | `				}` |
|  114441 |  575 | `				pTos->nIdx = SXU32_HIGH;` |
|   57223 |  576 | `			}else{` |
|       - |  577 | `				/* Attribute access. iP2: 0 = read, 2 = unset, 3 = isset, 4 = empty. */` |
| 3029617 |  578 | `				VmClassAttr *pObjAttr = 0;` |
| 3029617 |  579 | `				SyHashEntry *pEntry = 0;` |
|       - |  580 | `				/* Extract the target attribute */` |
| 3029617 |  581 | `				if( sName.nByte > 0 ){` |
| 3029617 |  582 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
| 3029617 |  583 | `					if( pEntry ){` |
|       - |  584 | `						/* Point to the attribute value */` |
| 3029083 |  585 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
| 1514539 |  586 | `					}` |
| 1514806 |  587 | `				}` |
| 3029612 |  588 | `				if( pObjAttr` |
| 3029345 |  589 | `				 && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT))` |
| 1514554 |  590 | `				 && PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,` |
|      20 |  591 | `					pObjAttr->pAttr->iProtection,FALSE) ){` |
|       - |  592 | `					/* A static property (or a constant) belongs to the CLASS: php does` |
|       - |  593 | `					 * not find it through an instance at all. It notices the attempt —` |
|       - |  594 | ``					 * `Accessing static property C::$s as non static` — and then treats`` |
|       - |  595 | `					 * the name as an ordinary MISSING property: a read warns and answers` |
|       - |  596 | `					 * null, isset() is false, a write goes to a dynamic property (which` |
|       - |  597 | `					 * PHL rejects by the §10 policy, like any other undeclared write).` |
|       - |  598 | `					 * PHL's instance table carries an entry for every declared member` |
|       - |  599 | ``					 * (statics share the class slot), so `$o->s` used to READ and — far`` |
|       - |  600 | `					 * worse — WRITE the class's own static in silence. isset()/empty()` |
|       - |  601 | `					 * pass silently, as php's do. */` |
|      18 |  602 | `					if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      17 |  603 | `					 && pInstr->iP2 != PH7_MEMBER_DEFPATH ){` |
|       - |  604 | `						/* Silent where php is silent: isset()/empty(), and any class` |
|       - |  605 | `						 * that declares the MAGIC accessor this context would dispatch` |
|       - |  606 | ``						 * (php passes `silent = ce->__get/__set/__unset != NULL` to its`` |
|       - |  607 | `						 * property-offset lookup). Once per access, too: the deferred` |
|       - |  608 | `						 * call-argument PRE-PASS (PH7_MEMBER_DEFPATH) re-runs this op for` |
|       - |  609 | ``						 * the same source `$o->s` and the value pass carries the notice`` |
|       - |  610 | `						 * — the BY-REF form has no value pass and notices at its own` |
|       - |  611 | `						 * site (VmBindPropByRef). */` |
|       - |  612 | `						const char *zMagic;` |
|       9 |  613 | `						if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       3 |  614 | `							zMagic = "__unset";` |
|       8 |  615 | `						}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|       7 |  616 | `						       \|\| VmMemberNextIsWrite(pInstr + 1) ){` |
|       3 |  617 | `							zMagic = "__set";` |
|       2 |  618 | `						}else{` |
|       5 |  619 | `							zMagic = "__get";` |
|       - |  620 | `						}` |
|       9 |  621 | `						if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) == 0 ){` |
|       7 |  622 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - |  623 | `								"Accessing static property %z::$%z as non static",` |
|       4 |  624 | `								&pClass->sName,&pObjAttr->pAttr->sName);` |
|       2 |  625 | `						}` |
|       4 |  626 | `					}` |
|      19 |  627 | `					pEntry = 0;` |
|      19 |  628 | `					pObjAttr = 0;` |
|       9 |  629 | `				}` |
| 3029617 |  630 | `				if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - |  631 | `					/* unset($o->prop): remove the property entirely so it disappears from` |
|       - |  632 | `					 * foreach / json_encode / get_object_vars / (array) — matching PHP (a value-only` |
|       - |  633 | `					 * release would leave a zombie null entry). Leave a NULL constant on the stack so` |
|       - |  634 | `					 * the trailing generic unset() builtin is a no-op (mirrors LOAD_IDX iP2=5).` |
|       - |  635 | `					 * php dispatches __unset($name) for a MISSING or INACCESSIBLE property` |
|       - |  636 | `					 * (band A #3b — pre-fix an inaccessible private was silently DELETED from` |
|       - |  637 | `					 * outside the class); without __unset, an inaccessible unset is php's` |
|       - |  638 | `					 * catchable "Cannot access ..." Error and a missing one stays a no-op. */` |
|      45 |  639 | `					int bUnsAccessible = pEntry ? PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) : 0;` |
|      47 |  640 | `					if( pEntry && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) != 0 ){` |
|       - |  641 | `						/* php 8.4: a hooked property (virtual or backed) can never be` |
|       - |  642 | `						 * unset — catchable Error, even from inside its own hook body` |
|       - |  643 | `						 * (probe-verified). */` |
|       - |  644 | `						SyBlob sErrMsg;` |
|       5 |  645 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       5 |  646 | `						SyBlobFormat(&sErrMsg,"Cannot unset hooked property %z::$%z",` |
|       4 |  647 | `							&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       5 |  648 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      43 |  649 | `					}else if( pEntry && bUnsAccessible ){` |
|      27 |  650 | `						PH7_VmReleaseInstanceAttr(&(*pVm),pObjAttr);` |
|      27 |  651 | `						SyHashDeleteEntry2(pEntry);` |
|      14 |  652 | `					}else{` |
|      15 |  653 | `						ph7_class_method *pUnsetMagic = PH7_ClassExtractMethod(pClass,"__unset",sizeof("__unset")-1);` |
|      15 |  654 | `						if( pUnsetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'u') ){` |
|      13 |  655 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'u');` |
|      13 |  656 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__unset",sizeof("__unset")-1,&sName,0);` |
|      13 |  657 | `							VmMagicGuardPop(pVm);` |
|       8 |  658 | `						}else if( pEntry ){` |
|       - |  659 | `							/* Inaccessible and no __unset: php's catchable Error. Parked on` |
|       - |  660 | `							 * the boundary rail; the op completes benignly and the` |
|       - |  661 | `							 * fetch-point router lands it. */` |
|       - |  662 | `							SyBlob sErrMsg;` |
|       3 |  663 | `							const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       3 |  664 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 |  665 | `							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",zVis,&pClass->sName,&sName);` |
|       3 |  666 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       1 |  667 | `						}` |
|       - |  668 | `						/* Missing property without __unset: silent no-op (php). */` |
|       - |  669 | `					}` |
|      45 |  670 | `					VmPopOperand(&pTos,1);    /* pop the attribute name */` |
|      45 |  671 | `					PH7_MemObjRelease(pTos);  /* release the object on the stack ($o's stack ref) */` |
|      45 |  672 | `					pTos->nIdx = SXU32_HIGH;  /* NULL constant */` |
|     359 |  673 | `					VM_EXIT_BREAK;` |
|       - |  674 | `				}` |
| 3029575 |  675 | `				if( pObjAttr == 0 && sName.nByte > 0 ){` |
|       - |  676 | `					/* Member not present on the instance and the next instruction writes/modifies it` |
|       - |  677 | ``					 * (store, array-append/keyed-write, `??=`, ++/--, or a compound-assign — see`` |
|       - |  678 | `					 * VmMemberNextIsWrite; the compiler always emits a terminating PH7_OP_DONE so` |
|       - |  679 | `					 * pInstr+1 is in-bounds). Create the property so the operation lands on a real slot` |
|       - |  680 | ``					 * (PHP auto-vivifies a fresh property for all these forms, not just `=`):`` |
|       - |  681 | `					 *   - a DECLARED prop that was unset() and is re-assigned → recreate it` |
|       - |  682 | `					 *     (PHP re-appends it at the end), OR` |
|       - |  683 | `					 *   - a dynamic prop on a dynamic-allowing class (stdClass).` |
|       - |  684 | `					 * The created slot is NULL with a real nIdx, which the modify-op then vivifies` |
|       - |  685 | `					 * (NULL→array for [], NULL→0/"" for ++/.=) and writes back in place.` |
|       - |  686 | `					 * Two signals: the compiler tags the member itself iP2=PH7_MEMBER_WRITE when it is` |
|       - |  687 | ``					 * the base of a write-subscript / `??=` (the modify-op is not the immediately-next`` |
|       - |  688 | `					 * instruction there), and for ++/--/compound-assign/store the next opcode is the` |
|       - |  689 | `					 * modify-op directly (VmMemberNextIsWrite). */` |
|     549 |  690 | `					VmInstr *pNext = pInstr + 1;` |
|     544 |  691 | `					if( pInstr->iP2 == PH7_MEMBER_WRITE \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|     440 |  692 | `					 \|\| VmMemberNextIsWrite(pNext) ){` |
|     115 |  693 | `						ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pThis->pClass,sName.zString,sName.nByte);` |
|     115 |  694 | `						if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       7 |  695 | `							VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pObjAttr);` |
|       4 |  696 | `						}else{` |
|       - |  697 | `							/* php 8 semantics (band A #3b): a PLAIN store to a missing` |
|       - |  698 | `							 * property dispatches __set($name,$value) when declared —` |
|       - |  699 | `							 * the value only exists at the following OP_STORE, so park` |
|       - |  700 | `							 * the receiver+name for it (one-instruction lifetime; the` |
|       - |  701 | `							 * guard makes a same-name write inside __set fall through` |
|       - |  702 | `							 * to dynamic creation, like php). Without __set — or for a` |
|       - |  703 | `							 * subscript-write base / read-modify-write, which php does` |
|       - |  704 | `							 * NOT route through __set — create a dynamic property on` |
|       - |  705 | `							 * ANY class with the 8.2 deprecation (suppressed for` |
|       - |  706 | `							 * stdClass and #[AllowDynamicProperties]); a readonly` |
|       - |  707 | `							 * class raises php's catchable Error instead. */` |
|     109 |  708 | `							ph7_class_method *pSetMagic = 0;` |
|     109 |  709 | `							ph7_class_method *pCoalIsset = 0, *pCoalGet = 0, *pCoalSet = 0;` |
|     109 |  710 | `							int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);` |
|     109 |  711 | `							if( bPlainStore \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|      91 |  712 | `								pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|      43 |  713 | `							}` |
|     109 |  714 | `							if( pSetMagic && pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - |  715 | `								/* php dispatches __set($name, element) for a missing` |
|       - |  716 | `								 * destructuring target, but the element value only exists` |
|       - |  717 | `								 * at the following OP_LOAD_LIST — the dispatch is` |
|       - |  718 | `								 * unsupported (recorded residual). Leave the miss: the` |
|       - |  719 | `								 * property stays uncreated, matching php's observable` |
|       - |  720 | `								 * state (its __set did not store either). */` |
|     109 |  721 | `							}else if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){` |
|      15 |  722 | `								pThis->iRef++;` |
|      15 |  723 | `								pVm->pMagicSetThis = pThis;` |
|      15 |  724 | `								SyBlobReset(&pVm->sMagicSetName);` |
|      15 |  725 | `								SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       - |  726 | `								/* pObjAttr stays NULL; the miss path below stays silent. */` |
|     101 |  727 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|      19 |  728 | `							 && pNext->iOp == PH7_OP_NULLC_JMP` |
|      19 |  729 | `							 && ((pCoalIsset = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1)) != 0` |
|       6 |  730 | `							  \|\| (pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)) != 0` |
|       3 |  731 | `							  \|\| (pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1)) != 0) ){` |
|       - |  732 | ``								/* `$o->p ??= v` on a missing property with magic accessors:`` |
|       - |  733 | `								 * php consults __isset first when declared — false means` |
|       - |  734 | `								 * assign directly through __set with NO __get call (the` |
|       - |  735 | `								 * test value stays null); true (or no __isset) means __get` |
|       - |  736 | `								 * provides the test value. A COAL_MAGIC entry is pushed` |
|       - |  737 | `								 * for the OP_NULLC_STORE at the jump target; the` |
|       - |  738 | `								 * fetch-point sweep drops it when the short-circuit jump` |
|       - |  739 | `								 * skips the assign or a throw abandons the RHS. Without` |
|       - |  740 | `								 * __set the assign side keeps the pre-existing loud` |
|       - |  741 | `								 * "Cannot perform assignment" path (php would create a` |
|       - |  742 | `								 * dynamic property there — recorded residual). Note the` |
|       - |  743 | `								 * condition's short-circuit assignment chain: a hit on an` |
|       - |  744 | `								 * earlier method leaves the later pointers unresolved, so` |
|       - |  745 | `								 * re-resolve the leftovers here (each is one hash probe,` |
|       - |  746 | `								 * only on this ??=-miss path). */` |
|       - |  747 | `								ph7_value sTest;` |
|       7 |  748 | `								int bMiss = 0; /* __isset said false: skip __get, test value stays null */` |
|       7 |  749 | `								if( pCoalIsset != 0 ){` |
|       5 |  750 | `									pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       2 |  751 | `								}` |
|       7 |  752 | `								if( pCoalIsset != 0 \|\| pCoalGet != 0 ){` |
|       7 |  753 | `									pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|       3 |  754 | `								}` |
|       7 |  755 | `								PH7_MemObjInit(pVm,&sTest);` |
|       7 |  756 | `								if( pCoalIsset && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - |  757 | `									ph7_value sIssetRet;` |
|       5 |  758 | `									PH7_MemObjInit(pVm,&sIssetRet);` |
|       5 |  759 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|       5 |  760 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|       5 |  761 | `									VmMagicGuardPop(pVm);` |
|       5 |  762 | `									PH7_MemObjToBool(&sIssetRet);` |
|       5 |  763 | `									bMiss = sIssetRet.x.iVal == 0;` |
|       5 |  764 | `									PH7_MemObjRelease(&sIssetRet);` |
|       2 |  765 | `								}` |
|       7 |  766 | `								if( !bMiss && pCoalGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       5 |  767 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       5 |  768 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sTest);` |
|       5 |  769 | `									VmMagicGuardPop(pVm);` |
|       2 |  770 | `								}` |
|       6 |  771 | `								if( pCoalSet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s')` |
|       7 |  772 | `								 && pVm->nBoundaryRc == 0 ){` |
|       - |  773 | `									VmHookRmw sPend;` |
|       7 |  774 | `									sPend.iKind = VM_HOOK_PEND_COAL_MAGIC;` |
|       7 |  775 | `									sPend.pThis = pThis;` |
|       7 |  776 | `									sPend.pAttr = 0;` |
|       7 |  777 | `									sPend.nBackIdx = SXU32_HIGH;` |
|       7 |  778 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|       7 |  779 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|       7 |  780 | `									SyBlobAppend(&sPend.sName,(const void *)sName.zString,sName.nByte);` |
|       7 |  781 | `									sPend.pOwnerStack = (void *)pStack;` |
|       7 |  782 | `									sPend.pInstrs = (void *)aInstr;` |
|       7 |  783 | `									sPend.nJmpPc = (sxu32)(pc + 1);        /* the OP_NULLC_JMP */` |
|       7 |  784 | `									sPend.nPc = (sxu32)(pNext->iP2 - 1);   /* the OP_NULLC_STORE */` |
|       7 |  785 | `									pThis->iRef++;` |
|       7 |  786 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|       3 |  787 | `								}` |
|       - |  788 | `								/* Pop the attribute name; the test value becomes the` |
|       - |  789 | `								 * expression slot (a temp, not an lvalue). */` |
|       7 |  790 | `								VmPopOperand(&pTos,1);` |
|       7 |  791 | `								pThis->iRef++;` |
|       7 |  792 | `								PH7_MemObjRelease(pTos);` |
|       7 |  793 | `								PH7_MemObjStore(&sTest,pTos);` |
|       7 |  794 | `								pTos->nIdx = SXU32_HIGH;` |
|       7 |  795 | `								PH7_MemObjRelease(&sTest);` |
|       7 |  796 | `								PH7_ClassInstanceUnref(pThis);` |
|       7 |  797 | `								VM_EXIT_BREAK;` |
|      86 |  798 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|      13 |  799 | `							 && !VmMemberNextIsWrite(pNext)` |
|      17 |  800 | `							 && PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) ){` |
|       - |  801 | `								/* Subscript-write base on a class with __get: php reads` |
|       - |  802 | `								 * through the magic layer (the write lands on the temp and` |
|       - |  803 | `								 * is lost, like php's indirect-modification case). A PLAIN` |
|       - |  804 | `								 * store (bPlainStore — the compiler tags those` |
|       - |  805 | `								 * PH7_MEMBER_WRITE too) is NOT this case: it falls through` |
|       - |  806 | `								 * to dynamic creation. A direct ++/--/compound-assign` |
|       - |  807 | `								 * (VmMemberNextIsWrite — the compiler now tags those` |
|       - |  808 | `								 * PH7_MEMBER_WRITE as well) is NOT this case either: it` |
|       - |  809 | `								 * falls through to dynamic creation like before (the` |
|       - |  810 | `								 * recorded RMW-vivifies-instead-of-__get residual, §7).` |
|       - |  811 | `								 * Leave the miss path — the read gate below dispatches` |
|       - |  812 | `								 * __get. */` |
|      91 |  813 | `							}else if( pThis->pClass->iFlags & PH7_CLASS_READONLY ){` |
|       - |  814 | `								SyBlob sErrMsg;` |
|     ! 0 |  815 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     ! 0 |  816 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|     ! 0 |  817 | `									&pThis->pClass->sName,&sName);` |
|     ! 0 |  818 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      86 |  819 | `							}else if( !VmClassAllowsDynamicProps(pVm,pThis->pClass)` |
|      54 |  820 | `							       && !VmClassHasAttributeNamed(pThis->pClass,"AllowDynamicProperties",sizeof("AllowDynamicProperties")-1) ){` |
|       - |  821 | `								/* php 8.2 only DEPRECATES creating a dynamic property on a` |
|       - |  822 | `								 * class without #[AllowDynamicProperties]; PHL rejects it.` |
|       - |  823 | `								 * stdClass / __set / declared props are unaffected. */` |
|       - |  824 | `								SyBlob sErrMsg;` |
|       8 |  825 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       8 |  826 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|       6 |  827 | `									&pThis->pClass->sName,&sName);` |
|       8 |  828 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       5 |  829 | `							}else{` |
|      85 |  830 | `								PH7_VmCreateDynamicAttr(&(*pVm),pThis,sName.zString,sName.nByte,&pObjAttr);` |
|       - |  831 | `							}` |
|       - |  832 | `						}` |
|      52 |  833 | `					}` |
|     269 |  834 | `				}` |
| 3029569 |  835 | `				if( pObjAttr == 0 && pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|       - |  836 | `					/* D1 commit 2: deferred property arg, property absent on a REACHABLE object` |
|       - |  837 | `					 * base ($o is a real slot). Record a property step rooted at $o so OP_CALL can` |
|       - |  838 | `					 * vivify+bind (by-ref) or read via __get / warn (by-value). A magic __get/__set` |
|       - |  839 | `					 * class is recorded the same way; the resolve step emits php's Notice for the` |
|       - |  840 | `					 * by-ref case and dispatches __get for by-value. */` |
|       - |  841 | `					SyString sProp;` |
|      87 |  842 | `					VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|      87 |  843 | `					SyStringInitFromBuf(&sProp,sName.zString,sName.nByte);` |
|      87 |  844 | `					if( pPath && VmDeferPathPushProp(pPath,&sProp) == SXRET_OK ){` |
|      87 |  845 | `						VmPopOperand(&pTos,1);       /* drop the property name */` |
|      87 |  846 | `						pThis->iRef++;` |
|      87 |  847 | `						PH7_MemObjRelease(pTos);     /* collapse the object slot into the carrier */` |
|      87 |  848 | `						pTos->x.pOther = pPath;` |
|      87 |  849 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      87 |  850 | `						pTos->nIdx = SXU32_HIGH;` |
|      87 |  851 | `						PH7_ClassInstanceUnref(pThis);` |
|      87 |  852 | `						VM_EXIT_BREAK;` |
|       - |  853 | `					}` |
|     ! 0 |  854 | `					if( pPath ){` |
|     ! 0 |  855 | `						VmFreeDeferredPath(pPath);` |
|     ! 0 |  856 | `					}` |
|       - |  857 | `					/* fall through to the normal miss handling on allocation failure */` |
|     ! 0 |  858 | `				}` |
| 3029485 |  859 | `				if( pObjAttr == 0 ){` |
|       - |  860 | `					/* Missing property. On a plain READ, php dispatches __get($name) and the` |
|       - |  861 | `					 * expression takes its RETURN VALUE (band A #3a — pre-fix the result was` |
|       - |  862 | `					 * discarded and a PHL-native warn fired even when __get existed). isset/` |
|       - |  863 | `					 * empty context consults __isset first (then __get for empty()'s value` |
|       - |  864 | `					 * test); a plain-store write context parked a pending __set above. A` |
|       - |  865 | `					 * self-recursive read of the same property falls back to the` |
|       - |  866 | `					 * undefined-property path via the guard, like php's property guard. */` |
|     373 |  867 | `					ph7_class_method *pGetMagic = 0;` |
|     373 |  868 | `					if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|      76 |  869 | `						ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|      76 |  870 | `						if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - |  871 | `							ph7_value sIssetRet;` |
|       - |  872 | `							int bSet;` |
|      39 |  873 | `							PH7_MemObjInit(pVm,&sIssetRet);` |
|      39 |  874 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|      39 |  875 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|      39 |  876 | `							VmMagicGuardPop(pVm);` |
|      39 |  877 | `							PH7_MemObjToBool(&sIssetRet);` |
|      39 |  878 | `							bSet = sIssetRet.x.iVal != 0;` |
|      39 |  879 | `							PH7_MemObjRelease(&sIssetRet);` |
|      39 |  880 | `							if( bSet && VmMemberCtxWantsValue(pInstr->iP2) ){` |
|       - |  881 | ``								/* empty() and `??`: __isset said set, so php goes on to`` |
|       - |  882 | `								 * __get for the VALUE — emptiness is judged on it, and the` |
|       - |  883 | `								 * coalesce simply IS it (a null answer then takes the` |
|       - |  884 | `								 * default, which OP_NULLC does for free). */` |
|      18 |  885 | `								ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       - |  886 | `								ph7_value sEmptyVal;` |
|      18 |  887 | `								PH7_MemObjInit(pVm,&sEmptyVal);` |
|      18 |  888 | `								if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|      18 |  889 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|      18 |  890 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|      18 |  891 | `									VmMagicGuardPop(pVm);` |
|       8 |  892 | `								}` |
|      18 |  893 | `								VmPopOperand(&pTos,1);` |
|      18 |  894 | `								pThis->iRef++;` |
|      18 |  895 | `								PH7_MemObjRelease(pTos);` |
|      18 |  896 | `								PH7_MemObjStore(&sEmptyVal,pTos);` |
|      18 |  897 | `								pTos->nIdx = SXU32_HIGH;` |
|      18 |  898 | `								PH7_MemObjRelease(&sEmptyVal);` |
|      18 |  899 | `								PH7_ClassInstanceUnref(pThis);` |
|      18 |  900 | `								VM_EXIT_BREAK;` |
|       - |  901 | `							}` |
|       - |  902 | `							/* isset(): the truth of __isset IS the answer — push a non-null` |
|       - |  903 | `							 * marker for true, NULL for false (isset only tests null-ness). */` |
|      23 |  904 | `							VmPopOperand(&pTos,1);` |
|      23 |  905 | `							pThis->iRef++;` |
|      23 |  906 | `							PH7_MemObjRelease(pTos);` |
|      23 |  907 | `							if( bSet ){` |
|      11 |  908 | `								pTos->x.iVal = 1;` |
|      11 |  909 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       4 |  910 | `							}` |
|      23 |  911 | `							pTos->nIdx = SXU32_HIGH;` |
|      23 |  912 | `							PH7_ClassInstanceUnref(pThis);` |
|      23 |  913 | `							VM_EXIT_BREAK;` |
|       - |  914 | `						}` |
|      18 |  915 | `					}` |
|     332 |  916 | `					if( (!VmMemberCtxIsLookup(pInstr->iP2) \|\| pInstr->iP2 == PH7_MEMBER_COALESCE)` |
|     315 |  917 | `					 && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|     320 |  918 | `					 && !VmMemberNextIsWrite(pInstr + 1) ){` |
|       - |  919 | `						/* Plain reads AND the ??=/subscript write-base (PH7_MEMBER_WRITE,` |
|       - |  920 | `						 * which php reads through __get); read-modify-write forms are` |
|       - |  921 | `						 * excluded (they vivified above — approximate, recorded), and so is` |
|       - |  922 | `						 * a destructuring target (a pure write — php never reads it).` |
|       - |  923 | ``						 * `??` reaches here only when the class declares NO __isset — the`` |
|       - |  924 | `						 * branch above has returned otherwise — so php's gate is absent and` |
|       - |  925 | `						 * __get answers on its own. */` |
|     285 |  926 | `						pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|     140 |  927 | `					}` |
|     337 |  928 | `					if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - |  929 | `						ph7_value sMagicRet;` |
|     279 |  930 | `						PH7_MemObjInit(pVm,&sMagicRet);` |
|     279 |  931 | `						VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     279 |  932 | `						PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|     279 |  933 | `						VmMagicGuardPop(pVm);` |
|       - |  934 | `						/* Pop the attribute name, replace the object slot with the magic` |
|       - |  935 | `						 * result (a temp, not an lvalue — nIdx stays constant). A throw` |
|       - |  936 | `						 * from __get parked at the boundary; the fetch-point router lands` |
|       - |  937 | `						 * it and abandons this slot. */` |
|     279 |  938 | `						VmPopOperand(&pTos,1);` |
|     279 |  939 | `						pThis->iRef++;` |
|     279 |  940 | `						PH7_MemObjRelease(pTos);` |
|     279 |  941 | `						PH7_MemObjStore(&sMagicRet,pTos);` |
|     279 |  942 | `						pTos->nIdx = SXU32_HIGH;` |
|     279 |  943 | `						PH7_MemObjRelease(&sMagicRet);` |
|     279 |  944 | `						PH7_ClassInstanceUnref(pThis);` |
|     279 |  945 | `						VM_EXIT_BREAK;` |
|       - |  946 | `					}` |
|       - |  947 | `					/* No __get (or guard held): load null. In isset()/empty() context (iP2 3/4)` |
|       - |  948 | `					 * PHP returns false/true SILENTLY, so suppress the read-miss warning there` |
|       - |  949 | `					 * (mirrors the array LOAD_IDX iP2=4/6 suppression); likewise when a` |
|       - |  950 | `					 * pending __set was parked above (the following OP_STORE dispatches it —` |
|       - |  951 | `					 * nothing is "undefined" about that write) or a throw is parked on the` |
|       - |  952 | `					 * boundary rail (e.g. the readonly-class dynamic-property Error — the` |
|       - |  953 | `					 * fetch-point router lands it right after this op). */` |
|      58 |  954 | `					if( !VmMemberCtxIsLookup(pInstr->iP2) && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|      24 |  955 | `					 && pVm->pMagicSetThis == 0` |
|      23 |  956 | `					 && pVm->nBoundaryRc == 0 ){` |
|       - |  957 | `						/* A destructuring target is also silent: php either created the` |
|       - |  958 | `						 * property above or dispatched __set — neither warns. */` |
|      12 |  959 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|       3 |  960 | `							&pClass->sName,&sName);` |
|       3 |  961 | `					}` |
|      29 |  962 | `				}` |
| 3029175 |  963 | `				VmPopOperand(&pTos,1);` |
|       - |  964 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|       - |  965 | `				 * This is due to the following case:` |
|       - |  966 | `				 *     (new TestClass())->foo;` |
|       - |  967 | `				 */` |
| 3029175 |  968 | `				pThis->iRef++;` |
| 3029175 |  969 | `				PH7_MemObjRelease(pTos);` |
| 3029175 |  970 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
| 3029175 |  971 | `				if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - |  972 | ``					/* `$o->p =& $x`: stash the resolved instance property slot for the`` |
|       - |  973 | `					 * following member-marked OP_STORE_REF, which rebinds it to alias the` |
|       - |  974 | `					 * source variable. Do NOT run the read/hook/magic/uninit machinery` |
|       - |  975 | `					 * below — a reference bind neither reads the value nor triggers` |
|       - |  976 | `					 * get/set hooks or an uninitialized-typed Error. pThis stays retained` |
|       - |  977 | `					 * (the iRef++ above); OP_STORE_REF releases it. */` |
|      12 |  978 | `					if( pObjAttr ){` |
|      12 |  979 | `						pVm->pRefTargetAttr = pObjAttr;` |
|      12 |  980 | `						pVm->pRefTargetThis = pThis;` |
|      12 |  981 | `						pVm->pRefTargetStaticAttr = 0;` |
|      12 |  982 | `						pTos->nIdx = pObjAttr->nIdx;` |
|       7 |  983 | `					}else{` |
|       - |  984 | `						/* Missing/inaccessible target: leave nothing stashed; OP_STORE_REF` |
|       - |  985 | `						 * no-ops. Balance the retain. */` |
|     ! 0 |  986 | `						pVm->pRefTargetAttr = 0;` |
|     ! 0 |  987 | `						pVm->pRefTargetThis = 0;` |
|     ! 0 |  988 | `						pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 |  989 | `						PH7_ClassInstanceUnref(pThis);` |
|       - |  990 | `					}` |
|      12 |  991 | `					VM_EXIT_BREAK;` |
|       - |  992 | `				}` |
| 3029165 |  993 | `				if( pObjAttr ){` |
| 3029107 |  994 | `					ph7_value *pValue = 0; /* cc warning */` |
|       - |  995 | `					/* Check attribute access */` |
| 3029107 |  996 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
| 3029080 |  997 | `						if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET))` |
| 1514716 |  998 | `						 && !VmHookGuardHeld(pVm,(void *)pThis,&sName) ){` |
|       - |  999 | `							/* PHP 8.4 property hooks: route reads, writes, and the` |
|       - | 1000 | `							 * read-modify-write forms through the synthesized hook` |
|       - | 1001 | `							 * methods. A held guard (either kind) means we are INSIDE` |
|       - | 1002 | `							 * one of this property's own hook bodies — fall through to` |
|       - | 1003 | `` 							 * the raw backing slot for BOTH directions (php: `$this->x` `` |
|       - | 1004 | `							 * within any of x's hooks addresses the backing store). */` |
|     211 | 1005 | `							VmInstr *pNextH = pInstr + 1;` |
|     211 | 1006 | `							int bPlainStore = (pNextH->iOp == PH7_OP_STORE && pNextH->iP2 != 0);` |
|     361 | 1007 | `							int bCoalesceW = pInstr->iP2 == PH7_MEMBER_WRITE` |
|     206 | 1008 | `								&& pNextH->iOp == PH7_OP_NULLC_JMP;` |
|       - | 1009 | `							/* ++/--/compound-assign: the modify op FOLLOWS the member` |
|       - | 1010 | `							 * directly (the compiler tags compound-assign members` |
|       - | 1011 | `							 * PH7_MEMBER_WRITE too, so classify by the next op, not iP2;` |
|       - | 1012 | `							 * the !bPlainStore term is load-bearing — VmMemberNextIsWrite` |
|       - | 1013 | `							 * returns 1 for a member OP_STORE too) */` |
|     211 | 1014 | `							int bRmwNext = !bPlainStore && VmMemberNextIsWrite(pNextH);` |
|     243 | 1015 | `							int bSubscriptW = !bPlainStore && !bCoalesceW && !bRmwNext` |
|     283 | 1016 | `								&& pInstr->iP2 == PH7_MEMBER_WRITE;` |
|     211 | 1017 | `							if( bPlainStore ){` |
|       - | 1018 | `								/* Arm the pending hook-set for the following OP_STORE — it` |
|       - | 1019 | `								 * dispatches the set hook, or throws the read-only Error` |
|       - | 1020 | `								 * when the property only has a get hook. (The scalar` |
|       - | 1021 | `								 * transient is safe here: its window is exactly one` |
|       - | 1022 | `								 * instruction, MEMBER -> STORE, nothing runs in between.) */` |
|      55 | 1023 | `								pThis->iRef++;` |
|      55 | 1024 | `								pVm->pHookSetThis = pThis;` |
|      55 | 1025 | `								pVm->pHookSetAttr = pObjAttr->pAttr;` |
|      55 | 1026 | `								pVm->nHookSetIdx = pObjAttr->nIdx;` |
|      55 | 1027 | `								pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|      55 | 1028 | `								PH7_ClassInstanceUnref(pThis);` |
|      55 | 1029 | `								VM_EXIT_BREAK;` |
|       - | 1030 | `							}` |
|     159 | 1031 | `							if( bSubscriptW ){` |
|       - | 1032 | `								/* Subscript write base ($o->m[] = v, $o->m[$k] = v):` |
|       - | 1033 | `								 * php's catchable Error, with or without a set hook` |
|       - | 1034 | ``								 * (only a by-ref `&get` hook would allow it — a loud`` |
|       - | 1035 | `								 * compile error in PHL). The op completes benignly with` |
|       - | 1036 | `								 * a null temp; the fetch-point router lands the throw. */` |
|       - | 1037 | `								SyBlob sErrMsg;` |
|       5 | 1038 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       5 | 1039 | `								SyBlobFormat(&sErrMsg,"Indirect modification of %z::$%z is not allowed",` |
|       4 | 1040 | `									&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       5 | 1041 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       5 | 1042 | `								PH7_ClassInstanceUnref(pThis);` |
|       5 | 1043 | `								VM_EXIT_BREAK;` |
|       - | 1044 | `							}` |
|     150 | 1045 | `							if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|      80 | 1046 | `							 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1047 | `								/* VIRTUAL set-only property: there is no backing store to` |
|       - | 1048 | `								 * fall back to, so EVERY read context — plain read,` |
|       - | 1049 | `								 * isset()/empty() (php throws there too, probe-verified),` |
|       - | 1050 | `								 * the ??= test, an RMW read — is php's catchable` |
|       - | 1051 | `								 * write-only Error. */` |
|       - | 1052 | `								SyBlob sErrMsg;` |
|       9 | 1053 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       9 | 1054 | `								SyBlobFormat(&sErrMsg,"Property %z::$%z is write-only",` |
|       8 | 1055 | `									&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       9 | 1056 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       9 | 1057 | `								PH7_ClassInstanceUnref(pThis);` |
|       9 | 1058 | `								VM_EXIT_BREAK;` |
|       - | 1059 | `							}` |
|     147 | 1060 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1061 | ``								/* isset()/empty()/`??` on a hooked property all call the get`` |
|       - | 1062 | `								 * hook (php): isset is "get() !== null", empty tests the` |
|       - | 1063 | ``								 * value, and `??` IS the value. */`` |
|       - | 1064 | `								ph7_value sHookRet;` |
|      16 | 1065 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|      16 | 1066 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|      16 | 1067 | `									if( !VmMemberCtxWantsValue(pInstr->iP2) ){` |
|       - | 1068 | `										/* isset(): the trailing builtin tests NULL-ness of what` |
|       - | 1069 | `										 * it is handed, so "not set" has to BE null. Storing` |
|       - | 1070 | ``										 * bool(FALSE) here made `isset($o->p)` answer TRUE for`` |
|       - | 1071 | `										 * a get hook returning null — the same non-null marker` |
|       - | 1072 | `										 * convention the __isset path uses. */` |
|       8 | 1073 | `										if( (sHookRet.iFlags & MEMOBJ_NULL) == 0 ){` |
|       6 | 1074 | `											pTos->x.iVal = 1;` |
|       6 | 1075 | `											MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       4 | 1076 | `										}else{` |
|       3 | 1077 | `											MemObjSetType(pTos,MEMOBJ_NULL);` |
|       - | 1078 | `										}` |
|       5 | 1079 | `									}else{` |
|       - | 1080 | ``										/* empty() judges the value; `??` IS the value. */`` |
|      10 | 1081 | `										PH7_MemObjStore(&sHookRet,pTos);` |
|       - | 1082 | `									}` |
|      16 | 1083 | `									pTos->nIdx = SXU32_HIGH;` |
|      16 | 1084 | `									PH7_MemObjRelease(&sHookRet);` |
|      16 | 1085 | `									PH7_ClassInstanceUnref(pThis);` |
|      16 | 1086 | `									VM_EXIT_BREAK;` |
|       - | 1087 | `								}` |
|     ! 0 | 1088 | `								PH7_MemObjRelease(&sHookRet);` |
|     ! 0 | 1089 | `							}` |
|     132 | 1090 | `							if( !bCoalesceW && !bRmwNext && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1091 | `								/* Read: dispatch __phl_hook_get_NAME (inheritance-aware) */` |
|       - | 1092 | `								ph7_value sHookRet;` |
|      98 | 1093 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|      98 | 1094 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|      84 | 1095 | `									PH7_MemObjStore(&sHookRet,pTos);` |
|      84 | 1096 | `									pTos->nIdx = SXU32_HIGH;` |
|      84 | 1097 | `									PH7_MemObjRelease(&sHookRet);` |
|      84 | 1098 | `									PH7_ClassInstanceUnref(pThis);` |
|      84 | 1099 | `									VM_EXIT_BREAK;` |
|       - | 1100 | `								}` |
|      15 | 1101 | `								PH7_MemObjRelease(&sHookRet);` |
|       7 | 1102 | `							}` |
|      49 | 1103 | `							if( bCoalesceW \|\| bRmwNext ){` |
|       - | 1104 | ``								/* `$o->x ??= v` (bCoalesceW) and the read-modify-write`` |
|       - | 1105 | ``								 * forms `$o->x++`, `$o->x .= v`, … (bRmwNext): php reads`` |
|       - | 1106 | `								 * through the get hook (raw backing when the property is` |
|       - | 1107 | `								 * set-only) and writes through the set hook.` |
|       - | 1108 | `								 *   - ??=: the test value goes on the stack and a` |
|       - | 1109 | `								 *     COAL_HOOK entry is pushed for the OP_NULLC_STORE` |
|       - | 1110 | `								 *     at the jump target; the fetch-point sweep drops it` |
|       - | 1111 | `								 *     when the short-circuit jump skips the assign or a` |
|       - | 1112 | `								 *     throw abandons the RHS (the entry is NOT a scalar` |
|       - | 1113 | `								 *     transient — nested stores/coalesces in the RHS` |
|       - | 1114 | `								 *     stack their own entries above it).` |
|       - | 1115 | `								 *   - RMW: the value goes into a fresh SCRATCH slot the` |
|       - | 1116 | `								 *     modify op mutates in place; the entry makes the` |
|       - | 1117 | `								 *     op's tail dispatch the set side with the computed` |
|       - | 1118 | `								 *     value (PH7_HOOK_RMW_WRITEBACK). */` |
|       - | 1119 | `								ph7_value sCur;` |
|       - | 1120 | `								sxi32 rcCur;` |
|      35 | 1121 | `								PH7_MemObjInit(pVm,&sCur);` |
|      35 | 1122 | `								rcCur = PH7_VmHookGetAttrValue(pThis,pObjAttr,&sCur);` |
|      35 | 1123 | `								if( rcCur == SXERR_NOTFOUND ){` |
|       - | 1124 | `									/* set-only hook (or guard edge): the read side is the` |
|       - | 1125 | `									 * raw backing store (php) */` |
|       3 | 1126 | `									ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|       3 | 1127 | `									if( pBack ){` |
|       3 | 1128 | `										PH7_MemObjStore(pBack,&sCur);` |
|       2 | 1129 | `									}` |
|      34 | 1130 | `								}else if( pVm->nBoundaryRc != 0 ){` |
|       - | 1131 | `									/* the get hook threw: leave the null temp; the` |
|       - | 1132 | `									 * fetch-point router lands the parked throw —` |
|       - | 1133 | `									 * nothing is armed. */` |
|     ! 0 | 1134 | `									PH7_MemObjRelease(&sCur);` |
|     ! 0 | 1135 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1136 | `									VM_EXIT_BREAK;` |
|       - | 1137 | `								}` |
|      35 | 1138 | `								if( bCoalesceW ){` |
|       - | 1139 | `									VmHookRmw sPend;` |
|      15 | 1140 | `									PH7_MemObjStore(&sCur,pTos);` |
|      15 | 1141 | `									pTos->nIdx = SXU32_HIGH;` |
|      15 | 1142 | `									PH7_MemObjRelease(&sCur);` |
|      15 | 1143 | `									sPend.iKind = VM_HOOK_PEND_COAL_HOOK;` |
|      15 | 1144 | `									sPend.pThis = pThis;` |
|      15 | 1145 | `									sPend.pAttr = pObjAttr->pAttr;` |
|      15 | 1146 | `									sPend.nBackIdx = pObjAttr->nIdx;` |
|      15 | 1147 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|      15 | 1148 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|      15 | 1149 | `									sPend.pOwnerStack = (void *)pStack;` |
|      15 | 1150 | `									sPend.pInstrs = (void *)aInstr;` |
|      15 | 1151 | `									sPend.nJmpPc = (sxu32)(pc + 1);            /* the OP_NULLC_JMP */` |
|      15 | 1152 | `									sPend.nPc = (sxu32)(pNextH->iP2 - 1);      /* the OP_NULLC_STORE */` |
|      15 | 1153 | `									pThis->iRef++;` |
|      15 | 1154 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|      15 | 1155 | `									PH7_ClassInstanceUnref(pThis);` |
|      15 | 1156 | `									VM_EXIT_BREAK;` |
|       - | 1157 | `								}` |
|       - | 1158 | `								{` |
|      21 | 1159 | `									ph7_value *pScr = PH7_ReserveMemObj(&(*pVm));` |
|      21 | 1160 | `									if( pScr ){` |
|       - | 1161 | `										VmHookRmw sRmw;` |
|      21 | 1162 | `										PH7_MemObjStore(&sCur,pScr);` |
|      21 | 1163 | `										PH7_MemObjStore(&sCur,pTos);` |
|      21 | 1164 | `										pTos->nIdx = pScr->nIdx;` |
|      21 | 1165 | `										sRmw.iKind = VM_HOOK_PEND_RMW;` |
|      21 | 1166 | `										sRmw.pThis = pThis;` |
|      21 | 1167 | `										sRmw.pAttr = pObjAttr->pAttr;` |
|      21 | 1168 | `										sRmw.nBackIdx = pObjAttr->nIdx;` |
|      21 | 1169 | `										sRmw.nScratchIdx = pScr->nIdx;` |
|      21 | 1170 | `										SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      21 | 1171 | `										sRmw.pOwnerStack = (void *)pStack;` |
|      21 | 1172 | `										sRmw.pInstrs = (void *)aInstr;` |
|      21 | 1173 | `										sRmw.nJmpPc = (sxu32)(pc + 1);  /* the modify op ... */` |
|      21 | 1174 | `										sRmw.nPc = (sxu32)(pc + 1);     /* ... is the whole window */` |
|      21 | 1175 | `										pThis->iRef++;` |
|      21 | 1176 | `										SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      10 | 1177 | `									}` |
|       - | 1178 | `									/* OOM: pScr == 0 — leave the null temp (loud allocator` |
|       - | 1179 | `									 * diagnostics already fired) */` |
|      21 | 1180 | `									PH7_MemObjRelease(&sCur);` |
|      21 | 1181 | `									PH7_ClassInstanceUnref(pThis);` |
|      21 | 1182 | `									VM_EXIT_BREAK;` |
|       - | 1183 | `								}` |
|       - | 1184 | `							}` |
|       7 | 1185 | `						}` |
|       - | 1186 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|       - | 1187 | `						 * We can only raise it on a real read, not when the slot is the` |
|       - | 1188 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|       - | 1189 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|       - | 1190 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
| 3028888 | 1191 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
| 1514584 | 1192 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|     275 | 1193 | `							VmInstr *pNext = pInstr + 1;` |
|     275 | 1194 | `							int bIsLhs = 0;` |
|     275 | 1195 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|     259 | 1196 | `								bIsLhs = 1;` |
|     127 | 1197 | `							}` |
|     275 | 1198 | `							if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 1199 | `								/* Destructuring target: the OP_LOAD_LIST store (typed-` |
|       - | 1200 | `								 * enforced) initializes it — never a read (php). */` |
|       7 | 1201 | `								bIsLhs = 1;` |
|       3 | 1202 | `							}` |
|       - | 1203 | ``							/* isset()/empty()/`??` read an uninitialized typed property`` |
|       - | 1204 | `							 * as "not set" — a silent miss, NOT the Error a plain read` |
|       - | 1205 | `							 * raises (php). The compiler tags such an access iP2 =` |
|       - | 1206 | `							 * ISSET/EMPTY; treat it like the assignment-LHS case and fall` |
|       - | 1207 | `							 * through to load the slot's NULL. */` |
|     275 | 1208 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       7 | 1209 | `								bIsLhs = 1;` |
|       3 | 1210 | `							}` |
|     275 | 1211 | `							if( !bIsLhs ){` |
|       6 | 1212 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|       6 | 1213 | `								PH7_ClassInstanceUnref(pThis);` |
|       6 | 1214 | `								if( rcU == PH7_ABORT ){` |
|       3 | 1215 | `									VM_EXIT_ABORT;` |
|       - | 1216 | `								}` |
|       - | 1217 | `								{` |
|       - | 1218 | `									sxi32 iRp;` |
|       3 | 1219 | `									if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|       5 | 1220 | `										PH7_RESUME_DRAIN()` |
|       3 | 1221 | `										pc = iRp;` |
|       3 | 1222 | `										VM_EXIT_BREAK;` |
|       - | 1223 | `									}` |
|       - | 1224 | `								}` |
|     ! 0 | 1225 | `								VM_EXIT_EXCEPTION;` |
|       - | 1226 | `							}` |
|     133 | 1227 | `						}` |
|       - | 1228 | `						/* Load attribute */` |
| 3028889 | 1229 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
| 3028889 | 1230 | `						if( pValue ){` |
| 3028889 | 1231 | `							if( pThis->iRef < 2 ){` |
|       - | 1232 | `								/* Perform a store operation,rather than a load operation since` |
|       - | 1233 | `								 * the class instance '$this' will be deleted shortly.` |
|       - | 1234 | `								 */` |
|     113 | 1235 | `								PH7_MemObjStore(pValue,pTos);` |
|      58 | 1236 | `							}else{` |
|       - | 1237 | `								/* Simple load */` |
| 3028779 | 1238 | `								PH7_MemObjLoad(pValue,pTos);` |
|       - | 1239 | `							}` |
| 3028889 | 1240 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
| 3028889 | 1241 | `								if( pThis->iRef > 1 ){` |
|       - | 1242 | `									/* Load attribute index */` |
| 3028779 | 1243 | `									pTos->nIdx = pObjAttr->nIdx;` |
| 1514387 | 1244 | `								}` |
| 1514442 | 1245 | `							}` |
| 1514442 | 1246 | `						}` |
| 1514447 | 1247 | `					}else{` |
|       - | 1248 | `						/* Inaccessible (private/protected) from this scope. php consults` |
|       - | 1249 | `						 * __get here exactly like a missing property (band A #3a) before` |
|       - | 1250 | `						 * erroring; the guard keeps a self-recursive read from looping. */` |
|      25 | 1251 | `						ph7_class_method *pGetMagic = 0;` |
|      22 | 1252 | `						if( pInstr->iP2 != PH7_MEMBER_WRITE && !VmMemberCtxIsLookup(pInstr->iP2)` |
|      19 | 1253 | `						 && !VmMemberNextIsWrite(pInstr + 1) ){` |
|      15 | 1254 | `							pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       6 | 1255 | `						}` |
|      25 | 1256 | `						if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 1257 | `							ph7_value sMagicRet;` |
|       3 | 1258 | `							PH7_MemObjInit(pVm,&sMagicRet);` |
|       3 | 1259 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       3 | 1260 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|       3 | 1261 | `							VmMagicGuardPop(pVm);` |
|       - | 1262 | `							/* The name was already popped and pTos released above; just` |
|       - | 1263 | `							 * take the magic result as the expression value. */` |
|       3 | 1264 | `							PH7_MemObjStore(&sMagicRet,pTos);` |
|       3 | 1265 | `							pTos->nIdx = SXU32_HIGH;` |
|       3 | 1266 | `							PH7_MemObjRelease(&sMagicRet);` |
|       3 | 1267 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 1268 | `							VM_EXIT_BREAK;` |
|       - | 1269 | `						}` |
|      23 | 1270 | `						if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1271 | `							/* isset/empty on an inaccessible property: php consults __isset` |
|       - | 1272 | `							 * (band A #3b), and is silently false without it — never an` |
|       - | 1273 | `							 * Error (pre-fix PHL fataled here). */` |
|       9 | 1274 | `							ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|       9 | 1275 | `							int bSet = 0;` |
|       9 | 1276 | `							if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 1277 | `								ph7_value sIssetRet;` |
|     ! 0 | 1278 | `								PH7_MemObjInit(pVm,&sIssetRet);` |
|     ! 0 | 1279 | `								VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|     ! 0 | 1280 | `								PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|     ! 0 | 1281 | `								VmMagicGuardPop(pVm);` |
|     ! 0 | 1282 | `								PH7_MemObjToBool(&sIssetRet);` |
|     ! 0 | 1283 | `								bSet = sIssetRet.x.iVal != 0;` |
|     ! 0 | 1284 | `								PH7_MemObjRelease(&sIssetRet);` |
|     ! 0 | 1285 | `							}` |
|       9 | 1286 | `							if( pInstr->iP2 == PH7_MEMBER_COALESCE && pIssetMagic == 0 ){` |
|       - | 1287 | ``								/* `??` with no __isset to gate it: php reads straight through`` |
|       - | 1288 | `								 * __get, exactly as it does for a MISSING property. The` |
|       - | 1289 | `								 * __get dispatch just above this block is gated on a` |
|       - | 1290 | `								 * non-lookup context, so answer here. */` |
|       5 | 1291 | `								ph7_class_method *pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       5 | 1292 | `								if( pCoalGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 1293 | `									ph7_value sCoalRet;` |
|     ! 0 | 1294 | `									PH7_MemObjInit(pVm,&sCoalRet);` |
|     ! 0 | 1295 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     ! 0 | 1296 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sCoalRet);` |
|     ! 0 | 1297 | `									VmMagicGuardPop(pVm);` |
|     ! 0 | 1298 | `									PH7_MemObjStore(&sCoalRet,pTos);` |
|     ! 0 | 1299 | `									pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1300 | `									PH7_MemObjRelease(&sCoalRet);` |
|     ! 0 | 1301 | `								}` |
|       5 | 1302 | `								PH7_ClassInstanceUnref(pThis);` |
|       5 | 1303 | `								VM_EXIT_BREAK;` |
|       - | 1304 | `							}` |
|       5 | 1305 | `							if( bSet ){` |
|     ! 0 | 1306 | `								if( VmMemberCtxWantsValue(pInstr->iP2) ){` |
|     ! 0 | 1307 | `									ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       - | 1308 | `									ph7_value sEmptyVal;` |
|     ! 0 | 1309 | `									PH7_MemObjInit(pVm,&sEmptyVal);` |
|     ! 0 | 1310 | `									if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|     ! 0 | 1311 | `										VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     ! 0 | 1312 | `										PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|     ! 0 | 1313 | `										VmMagicGuardPop(pVm);` |
|     ! 0 | 1314 | `									}` |
|     ! 0 | 1315 | `									PH7_MemObjStore(&sEmptyVal,pTos);` |
|     ! 0 | 1316 | `									pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1317 | `									PH7_MemObjRelease(&sEmptyVal);` |
|     ! 0 | 1318 | `								}else{` |
|     ! 0 | 1319 | `									pTos->x.iVal = 1;` |
|     ! 0 | 1320 | `									MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     ! 0 | 1321 | `									pTos->nIdx = SXU32_HIGH;` |
|       - | 1322 | `								}` |
|     ! 0 | 1323 | `							}` |
|       5 | 1324 | `							PH7_ClassInstanceUnref(pThis);` |
|       5 | 1325 | `							VM_EXIT_BREAK;` |
|       - | 1326 | `						}` |
|       - | 1327 | `						{` |
|       - | 1328 | `							/* A plain store to an inaccessible property dispatches __set` |
|       - | 1329 | `							 * (band A #3b): park the receiver+name for the following` |
|       - | 1330 | `							 * OP_STORE, exactly like the missing-property case. */` |
|      15 | 1331 | `							VmInstr *pNextW = pInstr + 1;` |
|      15 | 1332 | `							if( pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0 ){` |
|       3 | 1333 | `								ph7_class_method *pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|       3 | 1334 | `								if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){` |
|       3 | 1335 | `									pThis->iRef++;` |
|       3 | 1336 | `									pVm->pMagicSetThis = pThis;` |
|       3 | 1337 | `									SyBlobReset(&pVm->sMagicSetName);` |
|       3 | 1338 | `									SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       3 | 1339 | `									pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|       3 | 1340 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 1341 | `									VM_EXIT_BREAK;` |
|       - | 1342 | `								}` |
|     ! 0 | 1343 | `							}` |
|       - | 1344 | `						}` |
|      10 | 1345 | `						if( ((pInstr + 1)->iOp == PH7_OP_NULLC \|\| (pInstr + 1)->iOp == PH7_OP_NULLC_JMP)` |
|       7 | 1346 | `						 && pInstr->iP2 != PH7_MEMBER_WRITE ){` |
|       - | 1347 | `` 							/* `$o->priv ?? default` (OP_NULLC; NULLC_JMP is the `??=` `` |
|       - | 1348 | ``							 * pre-test): php treats `??` as an isset-style lookup — an`` |
|       - | 1349 | `							 * inaccessible property yields null SILENTLY (the` |
|       - | 1350 | `							 * __isset/__get consults already ran above), letting the` |
|       - | 1351 | `							 * coalesce pick the default. */` |
|     ! 0 | 1352 | `							PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1353 | `							VM_EXIT_BREAK; /* pTos is already the null temp */` |
|       - | 1354 | `						}` |
|       - | 1355 | `						/* A subclass reading a PARENT's PRIVATE property: php treats it as` |
|       - | 1356 | `						 * an UNDEFINED property (the base-private is invisible to the` |
|       - | 1357 | `						 * subclass scope) — a Warning + null, NOT an access Error. Only` |
|       - | 1358 | `						 * this exact shape warns; every other denied read is the catchable` |
|       - | 1359 | `						 * "Cannot access" Error below. */` |
|       - | 1360 | `						{` |
|      12 | 1361 | `						ph7_class *pSelf = VmCurrentSelf(&(*pVm));` |
|      12 | 1362 | `						ph7_class *pDecl = pObjAttr->pAttr->pDeclClass;` |
|      10 | 1363 | `						if( pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      11 | 1364 | `						 && pSelf && pDecl && pSelf != pDecl && PH7_VmInstanceOf(pSelf,pDecl) ){` |
|       3 | 1365 | `							if( !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       4 | 1366 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|       1 | 1367 | `									&pClass->sName,&sName);` |
|       1 | 1368 | `							}` |
|       3 | 1369 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 1370 | `							VM_EXIT_BREAK; /* pTos is already the null temp */` |
|       - | 1371 | `						}` |
|       - | 1372 | `						}` |
|       - | 1373 | `						/* Genuinely inaccessible: php's CATCHABLE Error (parked on the` |
|       - | 1374 | `						 * boundary rail; the fetch-point router lands it — it was an` |
|       - | 1375 | `						 * uncatchable VmReportUncaughtException+Abort before). */` |
|       - | 1376 | `						{` |
|       - | 1377 | `						SyBlob sErrMsg;` |
|      10 | 1378 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      10 | 1379 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      10 | 1380 | `						SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",` |
|       4 | 1381 | `							zVis,&pClass->sName,&sName);` |
|      10 | 1382 | `						PH7_ClassInstanceUnref(pThis);` |
|      10 | 1383 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      10 | 1384 | `						SyBlobRelease(&sErrMsg);` |
|      10 | 1385 | `						VM_EXIT_BREAK;` |
|       - | 1386 | `						}` |
|       - | 1387 | `					}` |
| 1514442 | 1388 | `				}` |
|       - | 1389 | `				/* Safely unreference the object */` |
| 3028947 | 1390 | `				PH7_ClassInstanceUnref(pThis);` |
|       - | 1391 | `			}` |
| 1571694 | 1392 | `		}else{` |
|       - | 1393 | ``			/* `->` on a non-object (e.g. a null intermediate). Silent in isset()/empty()/unset()`` |
|       - | 1394 | ``			 * context (iP2 2/3/4) so `isset($o->missing->x)` / `unset($o->missing->x)` match PHP. */`` |
|      14 | 1395 | `			if( pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1396 | `				/* php draws a sharp line here, and PH7 drew none: CALLING a method on a` |
|       - | 1397 | `				 * non-object is a catchable Error, while READING a property off one is a` |
|       - | 1398 | `				 * Warning that yields NULL. PH7 raised the same notice for both and` |
|       - | 1399 | ``				 * carried on with NULL, so `$null->m()` silently did nothing. */`` |
|       - | 1400 | `				SyString sMemb;` |
|      10 | 1401 | `				SyStringInitFromBuf(&sMemb,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      10 | 1402 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - | 1403 | `					SyBlob sErrM;` |
|       - | 1404 | `					sxi32 rcErr;` |
|       5 | 1405 | `					SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       5 | 1406 | `					SyBlobFormat(&sErrM,"Call to a member function %z() on %s",` |
|       2 | 1407 | `						&sMemb,VmArithTypeName(pNos));` |
|       5 | 1408 | `					VmPopOperand(&pTos,1);` |
|       5 | 1409 | `					PH7_MemObjRelease(pTos);` |
|       5 | 1410 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 1411 | `					pTos->nIdx = SXU32_HIGH;` |
|       7 | 1412 | `					rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       2 | 1413 | `						SyBlobLength(&sErrM));` |
|       5 | 1414 | `					SyBlobRelease(&sErrM);` |
|       5 | 1415 | `					if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 1416 | `					rc = rcErr;` |
|       5 | 1417 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1418 | `				}` |
|       8 | 1419 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Attempt to read property \"%z\" on %s",` |
|       2 | 1420 | `					&sMemb,VmArithTypeName(pNos));` |
|       2 | 1421 | `			}` |
|      10 | 1422 | `			VmPopOperand(&pTos,1);` |
|      10 | 1423 | `			PH7_MemObjRelease(pTos);` |
|      10 | 1424 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|       - | 1425 | `		}` |
| 1571698 | 1426 | `	}else{` |
|       - | 1427 | `		/* Static member access using class name */` |
|  102981 | 1428 | `		pNos = pTos;` |
|  102981 | 1429 | `		pThis = 0;` |
|  102981 | 1430 | `		if( !pInstr->p3 ){` |
|    2297 | 1431 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    2297 | 1432 | `			pNos--;` |
|       - | 1433 | `#ifdef UNTRUST` |
|       - | 1434 | `			if( pNos < pStack ){` |
|       - | 1435 | `				VM_EXIT_ABORT;` |
|       - | 1436 | `			}` |
|       - | 1437 | `#endif` |
|    1151 | 1438 | `		}else{` |
|       - | 1439 | `			/* Attribute name already computed */` |
|  100689 | 1440 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|       - | 1441 | `		}` |
|  102981 | 1442 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|  102981 | 1443 | `			ph7_class *pClass = 0;` |
|       - | 1444 | `			/* A FORWARDING static call — self::/parent::/static:: — preserves the` |
|       - | 1445 | `			 * caller's late-static-binding class (php); a non-forwarding C::m() resets` |
|       - | 1446 | `			 * it to C. The receiver slot below keeps the literal keyword, which OP_CALL` |
|       - | 1447 | `			 * can't resolve, so it would fall back to the callee's DECLARING class and` |
|       - | 1448 | `			 * lose LSB. Remember the live LSB class here and stamp it onto the receiver` |
|       - | 1449 | `			 * after the method name is pushed. */` |
|  102981 | 1450 | `			int bForwardingCall = 0;` |
|  102981 | 1451 | `			ph7_class *pForwardLsb = 0;` |
|       - | 1452 | `			/* Same rail snapshot as OP_NEW: the lookup below can run an autoloader that` |
|       - | 1453 | `			 * throws, and php then reports that exception and nothing else. */` |
|  102981 | 1454 | `			sxi32 nMbBrc = pVm->nBoundaryRc;` |
|  102981 | 1455 | `			const void *pMbRes = (const void *)pVm->pResumeFrame;` |
|  102981 | 1456 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - | 1457 | `				/* Class already instantiated */` |
|      14 | 1458 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      14 | 1459 | `				pClass = pThis->pClass;` |
|      14 | 1460 | `				pThis->iRef++; /* Deffer garbage collection */` |
|       8 | 1461 | `			}else{` |
|       - | 1462 | `				/* Try to extract the target class */` |
|  102969 | 1463 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|  102969 | 1464 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|  102969 | 1465 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|       - | 1466 | `					/* Handle self/static/parent keywords */` |
|  102969 | 1467 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|     733 | 1468 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|     733 | 1469 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|       - | 1470 | `							/* In a trait method, self:: resolves to the using class */` |
|      14 | 1471 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|       6 | 1472 | `						}` |
|     733 | 1473 | `						bForwardingCall = 1;` |
|     733 | 1474 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|  102605 | 1475 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|      64 | 1476 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|      64 | 1477 | `						bForwardingCall = 1;` |
|      64 | 1478 | `						pForwardLsb = pClass;` |
|  102211 | 1479 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|     185 | 1480 | `						pClass = PH7_VmResolveParentClass(&(*pVm));` |
|     185 | 1481 | `						bForwardingCall = 1;` |
|     185 | 1482 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|      94 | 1483 | `					}else{` |
|  101999 | 1484 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|       - | 1485 | `					}` |
|   51482 | 1486 | `				}` |
|       - | 1487 | `			}` |
|  102981 | 1488 | `			if( pClass == 0 ){` |
|       - | 1489 | `				/* Undefined class: php throws a catchable Error */` |
|       - | 1490 | `				SyBlob sErrM;` |
|       - | 1491 | `				sxi32 rcErr;` |
|      12 | 1492 | `				if( PH7_VmClassLookupRaised(&(*pVm),nMbBrc,pMbRes) ){` |
|       - | 1493 | `					/* ...unless an autoloader raised: land THAT instead of reporting the` |
|       - | 1494 | `					 * class missing on top of it. */` |
|      10 | 1495 | `					sxi32 rcAutoM = pVm->nBoundaryRc;` |
|      10 | 1496 | `					pVm->nBoundaryRc = 0;` |
|      10 | 1497 | `					if( !pInstr->p3 ){` |
|       8 | 1498 | `						VmPopOperand(&pTos,1);` |
|       3 | 1499 | `					}` |
|      10 | 1500 | `					PH7_MemObjRelease(pTos);` |
|      10 | 1501 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      10 | 1502 | `					pTos->nIdx = SXU32_HIGH;` |
|      10 | 1503 | `					if( rcAutoM == PH7_ABORT ){` |
|     ! 0 | 1504 | `						VM_EXIT_ABORT;` |
|       - | 1505 | `					}` |
|      10 | 1506 | `					rc = PH7_EXCEPTION;` |
|      10 | 1507 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1508 | `				}` |
|       3 | 1509 | `				SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       3 | 1510 | `				SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|       2 | 1511 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob));` |
|       3 | 1512 | `				if( !pInstr->p3 ){` |
|       3 | 1513 | `					VmPopOperand(&pTos,1);` |
|       1 | 1514 | `				}` |
|       3 | 1515 | `				PH7_MemObjRelease(pTos);` |
|       3 | 1516 | `				pTos->nIdx = SXU32_HIGH;` |
|       4 | 1517 | `				rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       1 | 1518 | `					SyBlobLength(&sErrM));` |
|       3 | 1519 | `				SyBlobRelease(&sErrM);` |
|       3 | 1520 | `				if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 | 1521 | `				rc = rcErr;` |
|       3 | 1522 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 1523 | `			}else{` |
|  102971 | 1524 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - | 1525 | `					/* Method call */` |
|    1065 | 1526 | `					ph7_class_method *pMeth = 0;` |
|    1065 | 1527 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|       - | 1528 | `						/* Extract the target method */` |
|    1065 | 1529 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|     530 | 1530 | `					}` |
|    1065 | 1531 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      15 | 1532 | `						if( pMeth ){` |
|       - | 1533 | `							SyBlob sErrM;` |
|       - | 1534 | `							sxi32 rcErr;` |
|       3 | 1535 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       3 | 1536 | `							SyBlobFormat(&sErrM,"Cannot call abstract method %z::%z()",` |
|       1 | 1537 | `								&pClass->sName,&sName);` |
|       3 | 1538 | `							if( !pInstr->p3 ){` |
|       3 | 1539 | `								VmPopOperand(&pTos,1);` |
|       1 | 1540 | `							}` |
|       3 | 1541 | `							PH7_MemObjRelease(pTos);` |
|       3 | 1542 | `							pTos->nIdx = SXU32_HIGH;` |
|       4 | 1543 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       1 | 1544 | `								SyBlobLength(&sErrM));` |
|       3 | 1545 | `							SyBlobRelease(&sErrM);` |
|       3 | 1546 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 | 1547 | `							rc = rcErr;` |
|       3 | 1548 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 1549 | `						}else{` |
|      13 | 1550 | `							ph7_class_method *pCallStaticMagic = PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1);` |
|      13 | 1551 | `							if( pCallStaticMagic ){` |
|       - | 1552 | `								/* php: C::missing(...) dispatches __callStatic($name,$args)` |
|       - | 1553 | `								 * via the packing trampoline (see the instance twin). */` |
|      11 | 1554 | `								SyBlobReset(&pVm->sMagicCallName);` |
|      11 | 1555 | `								SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);` |
|      11 | 1556 | `								pVm->pMagicCallThis = 0;` |
|      11 | 1557 | `								pVm->pMagicCallClass = pClass;` |
|      11 | 1558 | `								if( !pInstr->p3 ){` |
|      11 | 1559 | `									VmPopOperand(&pTos,1);` |
|       4 | 1560 | `								}` |
|      11 | 1561 | `								PH7_MemObjRelease(pTos);` |
|      11 | 1562 | `								SyBlobAppend(&pTos->sBlob,"__phl_magic_call",sizeof("__phl_magic_call")-1);` |
|      11 | 1563 | `								MemObjSetType(pTos,MEMOBJ_STRING);` |
|      11 | 1564 | `								pTos->nIdx = SXU32_HIGH;` |
|      11 | 1565 | `								VM_EXIT_BREAK;` |
|       - | 1566 | `							}` |
|       - | 1567 | `							{` |
|       - | 1568 | `								/* php: the STATIC form reports the same "Call to undefined` |
|       - | 1569 | `								 * method C::m()" as the instance one. */` |
|       - | 1570 | `								SyBlob sErrM;` |
|       - | 1571 | `								sxi32 rcErr;` |
|       3 | 1572 | `								SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       3 | 1573 | `								SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",` |
|       1 | 1574 | `									&pClass->sName,&sName);` |
|       3 | 1575 | `								if( !pInstr->p3 ){` |
|       3 | 1576 | `									VmPopOperand(&pTos,1);` |
|       1 | 1577 | `								}` |
|       3 | 1578 | `								PH7_MemObjRelease(pTos);` |
|       3 | 1579 | `								pTos->nIdx = SXU32_HIGH;` |
|       4 | 1580 | `								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       1 | 1581 | `									SyBlobLength(&sErrM));` |
|       3 | 1582 | `								SyBlobRelease(&sErrM);` |
|       3 | 1583 | `								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 | 1584 | `								rc = rcErr;` |
|       3 | 1585 | `								PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1586 | `							}` |
|       - | 1587 | `						}` |
|       - | 1588 | `						/* Pop the method name from the stack */` |
|     ! 0 | 1589 | `						if( !pInstr->p3 ){` |
|     ! 0 | 1590 | `							VmPopOperand(&pTos,1);` |
|       - | 1591 | `						}` |
|     ! 0 | 1592 | `						PH7_MemObjRelease(pTos);` |
|     ! 0 | 1593 | `					}else{` |
|       - | 1594 | `						/* Inaccessible from this scope: php routes through __callStatic when` |
|       - | 1595 | `						 * declared — the same rule the instance twin applies with __call, and` |
|       - | 1596 | ``						 * the reason `C::privateStatic()` runs the catch-all instead of the`` |
|       - | 1597 | `						 * "Call to private method" Error OP_CALL would raise below. */` |
|    1053 | 1598 | `						ph7_class_method *pDeniedStatic = 0;` |
|    1053 | 1599 | `						if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     124 | 1600 | `							ph7_class *pDeclCls = pMeth->sFunc.pUserData` |
|      82 | 1601 | `								? (ph7_class *)pMeth->sFunc.pUserData : pClass;` |
|      83 | 1602 | `							if( !PH7_VmClassMemberAccess(&(*pVm),pDeclCls,&sName,pMeth->iProtection,FALSE) ){` |
|       3 | 1603 | `								pDeniedStatic = PH7_ClassExtractMethod(pClass,"__callStatic",` |
|       - | 1604 | `									sizeof("__callStatic")-1);` |
|       1 | 1605 | `							}` |
|      41 | 1606 | `						}` |
|    1053 | 1607 | `						if( pDeniedStatic ){` |
|       - | 1608 | `							/* The trampoline is a plain function: drop the method-name slot so` |
|       - | 1609 | `							 * its name lands on the RECEIVER slot, exactly as the` |
|       - | 1610 | `							 * missing-method twin above does (leaving the class name in place` |
|       - | 1611 | `							 * would pack it as the first $args entry). */` |
|       3 | 1612 | `							SyBlobReset(&pVm->sMagicCallName);` |
|       3 | 1613 | `							SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);` |
|       3 | 1614 | `							pVm->pMagicCallThis = 0;` |
|       3 | 1615 | `							pVm->pMagicCallClass = pClass;` |
|       3 | 1616 | `							if( !pInstr->p3 ){` |
|       3 | 1617 | `								VmPopOperand(&pTos,1);` |
|       1 | 1618 | `							}` |
|       3 | 1619 | `							PH7_MemObjRelease(pTos);` |
|       3 | 1620 | `							SyBlobAppend(&pTos->sBlob,"__phl_magic_call",sizeof("__phl_magic_call")-1);` |
|       3 | 1621 | `							MemObjSetType(pTos,MEMOBJ_STRING);` |
|       3 | 1622 | `							pTos->nIdx = SXU32_HIGH;` |
|       3 | 1623 | `							VM_EXIT_BREAK;` |
|       - | 1624 | `						}` |
|       - | 1625 | `						/* Push method name on the stack */` |
|    1051 | 1626 | `						PH7_MemObjRelease(pTos);` |
|    1051 | 1627 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|    1051 | 1628 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|       - | 1629 | `					}` |
|    1051 | 1630 | `					pTos->nIdx = SXU32_HIGH;` |
|       - | 1631 | `					/* Forwarding call (self::/parent::/static::): overwrite the receiver` |
|       - | 1632 | `					 * slot (pNos, one below the method name) with the live LSB class name` |
|       - | 1633 | ``					 * so OP_CALL pushes THAT onto aSelf — preserving `static::` inside the`` |
|       - | 1634 | `					 * callee. Without this the literal keyword falls through to the` |
|       - | 1635 | `					 * callee's declaring class. Only when an LSB class is actually in` |
|       - | 1636 | `					 * scope (a static call from global scope has none). */` |
|    1051 | 1637 | `					if( bForwardingCall && pForwardLsb && (pNos->iFlags & MEMOBJ_STRING) ){` |
|     301 | 1638 | `						SyBlobReset(&pNos->sBlob);` |
|     301 | 1639 | `						SyBlobAppend(&pNos->sBlob,pForwardLsb->sName.zString,pForwardLsb->sName.nByte);` |
|     149 | 1640 | `					}` |
|     528 | 1641 | `				}else{` |
|       - | 1642 | `					/* Attribute access */` |
|  101911 | 1643 | `					ph7_class_attr *pAttr = 0;` |
|  101911 | 1644 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - | 1645 | `						/* unset(C::$x): PHP rejects unsetting a static property with a fatal Error.` |
|       - | 1646 | `						 * Without this the iP2=unset tag falls through to a normal static read and the` |
|       - | 1647 | `						 * trailing generic unset() would silently NULL (and de-type) the shared slot. */` |
|       - | 1648 | `						char zMsg[256];` |
|     ! 0 | 1649 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Attempt to unset static property %.*s::$%.*s",` |
|     ! 0 | 1650 | `							(int)pClass->sName.nByte,pClass->sName.zString,(int)sName.nByte,sName.zString);` |
|     ! 0 | 1651 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|     ! 0 | 1652 | `						VM_EXIT_ABORT;` |
|       - | 1653 | `					}` |
|       - | 1654 | `					/* Check for special ::class pseudo-constant */` |
|  102036 | 1655 | `					if( sName.nByte == sizeof("class")-1 &&` |
|     250 | 1656 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|       - | 1657 | `						/* ::class returns the fully qualified class name */` |
|       - | 1658 | `						/* Pop the attribute name from the stack */` |
|      93 | 1659 | `						if( !pInstr->p3 ){` |
|      93 | 1660 | `							VmPopOperand(&pTos,1);` |
|      44 | 1661 | `						}` |
|      93 | 1662 | `						PH7_MemObjRelease(pTos);` |
|       - | 1663 | `						/* Load the class name */` |
|      93 | 1664 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|      93 | 1665 | `						pTos->nIdx = SXU32_HIGH;` |
|      49 | 1666 | `					}else{` |
|       - | 1667 | `						/* Extract the target attribute. php keeps constants and` |
|       - | 1668 | `						 * (static) properties in separate namespaces; the source` |
|       - | 1669 | ``						 * form disambiguates (set by the `::` codegen, compile.c):`` |
|       - | 1670 | ``						 * a `$`-form is a STATIC PROPERTY (hAttr) — either a literal`` |
|       - | 1671 | ``						 * name folded into pInstr->p3 (`C::$s`) or a dynamic name on`` |
|       - | 1672 | ``						 * the stack marked iP1==2 (`C::$$x`, `C::${$e}`); a bareword`` |
|       - | 1673 | `						 * (p3==0, iP1==1) is a CONSTANT or enum case (hConst). This` |
|       - | 1674 | ``						 * is what lets `const C` and `public $C` coexist and resolve`` |
|       - | 1675 | `						 * to the right member. */` |
|  101823 | 1676 | `						if( sName.nByte > 0 ){` |
|  102391 | 1677 | `							pAttr = (pInstr->p3 \|\| pInstr->iP1 == 2)` |
|  100688 | 1678 | `								? PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte)` |
|   52039 | 1679 | `								: PH7_ClassExtractConstant(pClass,sName.zString,sName.nByte);` |
|   50909 | 1680 | `						}` |
|  101823 | 1681 | `						if( pAttr == 0 ){` |
|       - | 1682 | `							/* No such member. php raises a catchable Error whose wording` |
|       - | 1683 | `							 * depends on the ACCESS form (the same p3/iP1 signal used above):` |
|       - | 1684 | ``							 * a bareword `C::MISSING` (constant form) is "Undefined constant`` |
|       - | 1685 | ``							 * C::MISSING", a `$`-form `C::$missing` is "Access to undeclared`` |
|       - | 1686 | `							 * static property C::$missing". Instance magic (__get) is never` |
|       - | 1687 | `							 * consulted for statics (band A #3b). isset()/empty() context` |
|       - | 1688 | `							 * stays silently false. Parked on the boundary rail; the op` |
|       - | 1689 | `							 * completes benignly with NULL and the fetch-point router lands` |
|       - | 1690 | `							 * the throw. */` |
|      14 | 1691 | `							if( !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1692 | `								SyBlob sErrMsg;` |
|      14 | 1693 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      14 | 1694 | `								if( pInstr->p3 == 0 && pInstr->iP1 != 2 ){` |
|       7 | 1695 | `									SyBlobFormat(&sErrMsg,"Undefined constant %z::%z",` |
|       3 | 1696 | `										&pClass->sName,&sName);` |
|       4 | 1697 | `								}else{` |
|       8 | 1698 | `									SyBlobFormat(&sErrMsg,"Access to undeclared static property %z::$%z",` |
|       3 | 1699 | `										&pClass->sName,&sName);` |
|       - | 1700 | `								}` |
|      14 | 1701 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       6 | 1702 | `							}` |
|       6 | 1703 | `						}` |
|       - | 1704 | `						/* Pop the attribute name from the stack */` |
|  101823 | 1705 | `						if( !pInstr->p3 ){` |
|    1141 | 1706 | `							VmPopOperand(&pTos,1);` |
|     568 | 1707 | `						}` |
|  101823 | 1708 | `						PH7_MemObjRelease(pTos);` |
|  101823 | 1709 | `						pTos->nIdx = SXU32_HIGH;` |
|  101823 | 1710 | `						if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - | 1711 | ``							/* `self::$s =& $x` / `C::$s =& $x`: stash the static property slot`` |
|       - | 1712 | `							 * for the following member-marked OP_STORE_REF (class-level, shared` |
|       - | 1713 | `							 * across instances — matches php). Skip the read machinery below. */` |
|       6 | 1714 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|       6 | 1715 | `							 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|       9 | 1716 | `							 && VmClassStaticDeferPending(pClass) ){` |
|       - | 1717 | `								/* Binding a reference TO a static touches the table, so it` |
|       - | 1718 | `								 * materializes first, like every other static access. */` |
|       3 | 1719 | `								sxi32 rcRt = PH7_VmMaterializeClassStatics(&(*pVm),pClass);` |
|       3 | 1720 | `								if( rcRt != SXRET_OK ){` |
|       3 | 1721 | `									pVm->pRefTargetStaticAttr = 0;` |
|       3 | 1722 | `									pVm->pRefTargetAttr = 0;` |
|       3 | 1723 | `									pVm->pRefTargetThis = 0;` |
|       3 | 1724 | `									if( pThis ){` |
|     ! 0 | 1725 | `										PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1726 | `									}` |
|       3 | 1727 | `									if( rcRt == PH7_ABORT ){` |
|     ! 0 | 1728 | `										VM_EXIT_ABORT;` |
|       - | 1729 | `									}` |
|       - | 1730 | `									{` |
|       - | 1731 | `										sxi32 iRpR;` |
|       3 | 1732 | `										if( VmRecordedResume(pVm,&iRpR,pState->pEntryFrame,aInstr) ){` |
|       7 | 1733 | `											PH7_RESUME_DRAIN()` |
|       3 | 1734 | `											pc = iRpR;` |
|       3 | 1735 | `											VM_EXIT_BREAK;` |
|       - | 1736 | `										}` |
|       - | 1737 | `									}` |
|     ! 0 | 1738 | `									VM_EXIT_EXCEPTION;` |
|       - | 1739 | `								}` |
|     ! 0 | 1740 | `							}` |
|       4 | 1741 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|       6 | 1742 | `							 && PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|       6 | 1743 | `								pVm->pRefTargetStaticAttr = pAttr;` |
|       6 | 1744 | `								pVm->pRefTargetAttr = 0;` |
|       6 | 1745 | `								pVm->pRefTargetThis = 0;` |
|       6 | 1746 | `								pTos->nIdx = pAttr->nIdx;` |
|       4 | 1747 | `							}else{` |
|     ! 0 | 1748 | `								pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 1749 | `								pVm->pRefTargetAttr = 0;` |
|     ! 0 | 1750 | `								pVm->pRefTargetThis = 0;` |
|       - | 1751 | `							}` |
|       6 | 1752 | `							if( pThis ){` |
|     ! 0 | 1753 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1754 | `							}` |
|       6 | 1755 | `							VM_EXIT_BREAK;` |
|       - | 1756 | `						}` |
|  101817 | 1757 | `						if( pAttr ){` |
|  101805 | 1758 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       - | 1759 | `								/* Access to a non static attribute */` |
|     ! 0 | 1760 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|     ! 0 | 1761 | `									&pClass->sName,&pAttr->sName` |
|       - | 1762 | `									);` |
|     ! 0 | 1763 | `							}else{` |
|       - | 1764 | `								ph7_value *pValue;` |
|       - | 1765 | `								/* php materializes the class's static table at the FIRST` |
|       - | 1766 | `								 * static-property access (any property, any context — read,` |
|       - | 1767 | `								 * write, even isset): a default whose evaluation was DEFERRED` |
|       - | 1768 | `								 * (it threw at the declaration) runs here, and a typed one that` |
|       - | 1769 | `								 * failed its check throws its catchable TypeError here. Class` |
|       - | 1770 | `								 * constants and static method calls do not trigger the` |
|       - | 1771 | `								 * materialization (php-exact). */` |
|  101800 | 1772 | `								if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|  101238 | 1773 | `								 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|  100681 | 1774 | `								 && VmClassStaticDeferPending(pClass) ){` |
|      48 | 1775 | `									sxi32 rcD = PH7_VmMaterializeClassStatics(&(*pVm),pClass);` |
|      48 | 1776 | `									if( rcD != SXRET_OK ){` |
|      44 | 1777 | `										if( pThis ){` |
|     ! 0 | 1778 | `											PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1779 | `										}` |
|      44 | 1780 | `										if( rcD == PH7_ABORT ){` |
|     ! 0 | 1781 | `											VM_EXIT_ABORT;` |
|       - | 1782 | `										}` |
|       - | 1783 | `										{` |
|       - | 1784 | `											sxi32 iRpD;` |
|      44 | 1785 | `											if( VmRecordedResume(pVm,&iRpD,pState->pEntryFrame,aInstr) ){` |
|      82 | 1786 | `												PH7_RESUME_DRAIN()` |
|      40 | 1787 | `												pc = iRpD;` |
|      40 | 1788 | `												VM_EXIT_BREAK;` |
|       - | 1789 | `											}` |
|       - | 1790 | `										}` |
|       5 | 1791 | `										VM_EXIT_EXCEPTION;` |
|       - | 1792 | `									}` |
|       2 | 1793 | `								}` |
|       - | 1794 | `								/* Check if the access to the attribute is allowed */` |
|  101763 | 1795 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|       - | 1796 | `									/* PHP 7.4+: uninitialized typed static read.` |
|       - | 1797 | `									 * Same LHS-of-store peek as the instance path. */` |
|  101754 | 1798 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|  100938 | 1799 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|  150104 | 1800 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|  100066 | 1801 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|  100071 | 1802 | `										if( pS ){` |
|  100071 | 1803 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|  100071 | 1804 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|  100014 | 1805 | `												VmInstr *pNext = pInstr + 1;` |
|  100014 | 1806 | `												int bIsLhs = 0;` |
|  100014 | 1807 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       8 | 1808 | `													bIsLhs = 1;` |
|       3 | 1809 | `												}` |
|  100014 | 1810 | `												if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 1811 | `													/* Destructuring target ([S::$s] = [...]):` |
|       - | 1812 | `													 * initialized by the OP_LOAD_LIST store. */` |
|       3 | 1813 | `													bIsLhs = 1;` |
|       1 | 1814 | `												}` |
|  100014 | 1815 | `												if( !bIsLhs ){` |
|  100004 | 1816 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|  100004 | 1817 | `													if( pThis ){` |
|     ! 0 | 1818 | `														PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1819 | `													}` |
|  100004 | 1820 | `													if( rcU == PH7_ABORT ){` |
|     ! 0 | 1821 | `														VM_EXIT_ABORT;` |
|       - | 1822 | `													}` |
|       - | 1823 | `													{` |
|       - | 1824 | `														sxi32 iRp;` |
|  100004 | 1825 | `														if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|  200006 | 1826 | `															PH7_RESUME_DRAIN()` |
|  100004 | 1827 | `															pc = iRp;` |
|  100004 | 1828 | `															VM_EXIT_BREAK;` |
|       - | 1829 | `														}` |
|       - | 1830 | `													}` |
|     ! 0 | 1831 | `													VM_EXIT_EXCEPTION;` |
|       - | 1832 | `												}` |
|       4 | 1833 | `											}` |
|      32 | 1834 | `										}` |
|      32 | 1835 | `									}` |
|    1752 | 1836 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|    1441 | 1837 | `									 && SySetUsed(&pAttr->aAttrs) > 0 ){` |
|       - | 1838 | `										/* php 8.4 #[\Deprecated] on a class constant:` |
|       - | 1839 | `										 * every access re-warns. */` |
|      11 | 1840 | `										VmDeprecatedConstNotice(&(*pVm),pClass,pAttr);` |
|       5 | 1841 | `									}` |
|    1757 | 1842 | `									if( pAttr->nIdx == SXU32_HIGH ){` |
|       - | 1843 | `										/* Unmaterialized slot. Enum case: first access` |
|       - | 1844 | `										 * materializes ALL the singletons. Plain constant: its` |
|       - | 1845 | `										 * initializer hasn't run yet (mount-order-dependent` |
|       - | 1846 | `										 * cross-constant reference) — evaluate on demand. A` |
|       - | 1847 | `										 * raised TypeError/Error (backing mismatch, duplicate` |
|       - | 1848 | `										 * value, self-reference) parks on the boundary rail;` |
|       - | 1849 | `										 * the op completes benignly with NULL and the` |
|       - | 1850 | `										 * fetch-point router lands the throw. */` |
|       - | 1851 | `										sxi32 rcEnum;` |
|     433 | 1852 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|       - | 1853 | `											/* php: a DIRECT static access evaluates every case` |
|       - | 1854 | `											 * of the enum (whole-class constant update) — a` |
|       - | 1855 | `											 * broken sibling case throws here too. A reference` |
|       - | 1856 | `											 * from inside another constant's initializer` |
|       - | 1857 | `											 * (nConstEvalDepth > 0) evaluates only the` |
|       - | 1858 | `											 * requested case. */` |
|      41 | 1859 | `											if( pVm->nConstEvalDepth > 0 ){` |
|       3 | 1860 | `												rcEnum = VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|       2 | 1861 | `											}else{` |
|      39 | 1862 | `												rcEnum = VmEnumMaterialize(&(*pVm),pClass);` |
|       - | 1863 | `											}` |
|      23 | 1864 | `										}else{` |
|     397 | 1865 | `											rcEnum = VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|       - | 1866 | `										}` |
|     433 | 1867 | `										if( rcEnum != SXRET_OK ){` |
|      41 | 1868 | `											VmBoundaryPark(&(*pVm),rcEnum);` |
|      19 | 1869 | `										}` |
|     214 | 1870 | `									}` |
|       - | 1871 | `									/* Load the desired attribute */` |
|    1757 | 1872 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|    1757 | 1873 | `									if( pValue ){` |
|    1717 | 1874 | `										PH7_MemObjLoad(pValue,pTos);` |
|    1717 | 1875 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       - | 1876 | `											/* Load index number */` |
|     637 | 1877 | `											pTos->nIdx = pAttr->nIdx;` |
|     316 | 1878 | `										}` |
|     856 | 1879 | `									}` |
|     881 | 1880 | `								}else{` |
|       - | 1881 | `									/* Throw Error exception (PHP-compatible) */` |
|       - | 1882 | `									char zMsg[256];` |
|       5 | 1883 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       5 | 1884 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|       7 | 1885 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|       4 | 1886 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|       4 | 1887 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|       3 | 1888 | `									}else{` |
|     ! 0 | 1889 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|     ! 0 | 1890 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|     ! 0 | 1891 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|       - | 1892 | `									}` |
|       5 | 1893 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|       5 | 1894 | `									VM_EXIT_ABORT;` |
|       - | 1895 | `								}` |
|       - | 1896 | `							}` |
|     876 | 1897 | `						}` |
|       - | 1898 | `					}` |
|       - | 1899 | `				}` |
|    2903 | 1900 | `				if( pThis ){` |
|       - | 1901 | `					/* Safely unreference the object */` |
|      14 | 1902 | `					PH7_ClassInstanceUnref(pThis);` |
|       6 | 1903 | `				}` |
|       - | 1904 | `			}` |
|    1454 | 1905 | `		}else{` |
|       - | 1906 | `			/* Pop operands */` |
|     ! 0 | 1907 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|     ! 0 | 1908 | `			if( !pInstr->p3 ){` |
|     ! 0 | 1909 | `				VmPopOperand(&pTos,1);` |
|     ! 0 | 1910 | `			}` |
|     ! 0 | 1911 | `			PH7_MemObjRelease(pTos);` |
|     ! 0 | 1912 | `			pTos->nIdx = SXU32_HIGH;` |
|       - | 1913 | `		}` |
|       - | 1914 | `	}` |
| 3146289 | 1915 | `	VM_EXIT_BREAK;` |
|     ! 0 | 1916 | `	VM_EXIT_BREAK;` |
| 1623532 | 1917 | `}` |
|       - | 1918 |  |
|       - | 1919 | `/*` |
|       - | 1920 | ` * OP_CLONE: body moved verbatim from the OP_CLONE arm of` |
|       - | 1921 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 1922 | ` */` |
|     220 | 1923 | `PH7_PRIVATE VmOpRc VmExecOpClone(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 1924 | `{` |
|     225 | 1925 | `	ph7_value *pTos = pState->pTos;` |
|     225 | 1926 | `	ph7_value *pStack = pState->pStack;` |
|     225 | 1927 | `	VmInstr *aInstr = pState->aInstr;` |
|     225 | 1928 | `	sxi32 pc = pState->pc;` |
|       - | 1929 | `	sxi32 rc;` |
|     110 | 1930 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 1931 | `	ph7_class_instance *pSrc,*pClone;` |
|       - | 1932 | `#ifdef UNTRUST` |
|       - | 1933 | `	if( pTos < pStack ){` |
|       - | 1934 | `		VM_EXIT_ABORT;` |
|       - | 1935 | `	}` |
|       - | 1936 | `#endif` |
|       - | 1937 | `	/* Make sure we are dealing with a class instance. PHP 8 throws a catchable` |
|       - | 1938 | ``	 * TypeError for a non-object operand — for both `clone $x` and clone($x). */`` |
|     225 | 1939 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - | 1940 | `		SyBlob sMsg;` |
|       7 | 1941 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       7 | 1942 | `		SyBlobFormat(&sMsg,"clone(): Argument #1 ($object) must be of type object, %s given",` |
|       3 | 1943 | `			ph7_type_name(pTos));` |
|       7 | 1944 | `		rc = VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|       7 | 1945 | `		PH7_MemObjRelease(pTos);` |
|       7 | 1946 | `		pTos->nIdx = SXU32_HIGH;` |
|       7 | 1947 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 1948 | `			VM_EXIT_ABORT;` |
|       - | 1949 | `		}` |
|       - | 1950 | `		{` |
|       - | 1951 | `			sxi32 iRp;` |
|       7 | 1952 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|       7 | 1953 | `				pc = iRp;` |
|       7 | 1954 | `				VM_EXIT_BREAK;` |
|       - | 1955 | `			}` |
|       - | 1956 | `		}` |
|     ! 0 | 1957 | `		VM_EXIT_EXCEPTION;` |
|       - | 1958 | `	}` |
|       - | 1959 | `	/* Point to the source */` |
|     219 | 1960 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|       - | 1961 | `	/* Enum cases are not cloneable — php's catchable Error (the singleton` |
|       - | 1962 | `	 * identity would break). */` |
|     219 | 1963 | `	if( pSrc->pClass->iFlags & PH7_CLASS_ENUM ){` |
|       - | 1964 | `		SyBlob sMsg;` |
|       3 | 1965 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       3 | 1966 | `		SyBlobFormat(&sMsg,"Trying to clone an uncloneable object of class %z",` |
|       2 | 1967 | `			&pSrc->pClass->sName);` |
|       3 | 1968 | `		rc = VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|       3 | 1969 | `		PH7_MemObjRelease(pTos);` |
|       3 | 1970 | `		pTos->nIdx = SXU32_HIGH;` |
|       3 | 1971 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 1972 | `			VM_EXIT_ABORT;` |
|       - | 1973 | `		}` |
|       - | 1974 | `		{` |
|       - | 1975 | `			sxi32 iRp;` |
|       3 | 1976 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|     ! 0 | 1977 | `				pc = iRp;` |
|     ! 0 | 1978 | `				VM_EXIT_BREAK;` |
|       - | 1979 | `			}` |
|       - | 1980 | `		}` |
|       3 | 1981 | `		VM_EXIT_EXCEPTION;` |
|       - | 1982 | `	}` |
|       - | 1983 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|     217 | 1984 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|     ! 0 | 1985 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - | 1986 | `			"Trying to clone an uncloneable object of class '%z'",` |
|     ! 0 | 1987 | `			&pSrc->pClass->sName);` |
|     ! 0 | 1988 | `		PH7_MemObjRelease(pTos);` |
|     ! 0 | 1989 | `		VM_EXIT_BREAK;` |
|       - | 1990 | `	}` |
|       - | 1991 | `	/* Perform the clone operation */` |
|     217 | 1992 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|     217 | 1993 | `	PH7_MemObjRelease(pTos);` |
|     217 | 1994 | `	if( pClone == 0 ){` |
|     ! 0 | 1995 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 1996 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|     ! 0 | 1997 | `	}else{` |
|       - | 1998 | `		/* Load the cloned object */` |
|     217 | 1999 | `		pTos->x.pOther = pClone;` |
|     217 | 2000 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - | 2001 | `	}` |
|     217 | 2002 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2003 | `	VM_EXIT_BREAK;` |
|     115 | 2004 | `}` |
|       - | 2005 |  |
