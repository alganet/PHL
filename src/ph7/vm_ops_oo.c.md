# src/ph7/vm_ops_oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 868/1015 lines (85.52%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2011, 2012, 2013, 2014 Symisc Systems <licensing@symisc.net>` |
|     - |    3 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    4 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    5 | ` */` |
|     - |    6 | `#include "ph7int.h"` |
|     - |    7 | `/*` |
|     - |    8 | ` * Section:` |
|     - |    9 | ` *    Opcode handlers extracted from vm.c's dispatch loop. Each handler runs` |
|     - |   10 | ` *    one opcode arm against the caller's VmExecState: the loop syncs pTos/pc` |
|     - |   11 | ` *    in, calls the handler, reloads them and routes the returned VmOpRc onto` |
|     - |   12 | ` *    its labels (same idiom as VmCallFinish).` |
|     - |   13 | ` * Status:` |
|     - |   14 | ` *    Stable.` |
|     - |   15 | ` */` |
|     - |   16 | `/* Bind the dispatch-routing exits to handler semantics (see vm_dispatch.h),` |
|     - |   17 | ` * and map the macros' sState references onto our state parameter. */` |
|     - |   18 | `#define VM_EXIT_BREAK      { pState->pTos = pTos; pState->pc = pc; return VM_OP_NEXT; }` |
|     - |   19 | `#define VM_EXIT_ABORT      { pState->pTos = pTos; pState->pc = pc; return VM_OP_ABORT; }` |
|     - |   20 | `#define VM_EXIT_EXCEPTION  { pState->pTos = pTos; pState->pc = pc; return VM_OP_EXCEPTION; }` |
|     - |   21 | `#include "vm_dispatch.h"` |
|     - |   22 | `#define sState (*pState)` |
|     - |   23 |  |
|     - |   24 | `/*` |
|     - |   25 | ` * OP_CLONE_APPLY: body moved verbatim from the OP_CLONE_APPLY arm of` |
|     - |   26 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|     - |   27 | ` */` |
|    16 |   28 | `PH7_PRIVATE VmOpRc VmExecOpCloneApply(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|     1 |   29 | `{` |
|    17 |   30 | `	ph7_value *pTos = pState->pTos;` |
|    17 |   31 | `	ph7_value *pStack = pState->pStack;` |
|    17 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
|    17 |   33 | `	sxi32 pc = pState->pc;` |
|     - |   34 | `	sxi32 rc;` |
|     8 |   35 | `	SXUNUSED(pInstr);` |
|     8 |   36 | `	SXUNUSED(pStack);` |
|     - |   37 | `	ph7_value *pUpdates,*pObj;` |
|     - |   38 | `	ph7_class_instance *pClone;` |
|     - |   39 | `	ph7_hashmap *pMap;` |
|     - |   40 | `	ph7_hashmap_node *pNode;` |
|    17 |   41 | `	sxi32 rcApply = SXRET_OK;` |
|     - |   42 | `	sxu32 n;` |
|     - |   43 | `#ifdef UNTRUST` |
|     - |   44 | `	if( pTos < &pStack[1] ){` |
|     - |   45 | `		VM_EXIT_ABORT;` |
|     - |   46 | `	}` |
|     - |   47 | `#endif` |
|    17 |   48 | `	pUpdates = pTos;` |
|    17 |   49 | `	pObj = &pTos[-1];` |
|     - |   50 | `	/* $withProperties must be an array (PHP: TypeError otherwise). */` |
|    17 |   51 | `	if( (pUpdates->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|     - |   52 | `		SyBlob sMsg;` |
|   ! 0 |   53 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|   ! 0 |   54 | `		SyBlobFormat(&sMsg,"clone(): Argument #2 ($withProperties) must be of type array, %s given",` |
|   ! 0 |   55 | `			ph7_type_name(pUpdates));` |
|   ! 0 |   56 | `		rc = VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|   ! 0 |   57 | `		if( rc == PH7_ABORT ){` |
|   ! 0 |   58 | `			VM_EXIT_ABORT;` |
|     - |   59 | `		}` |
|     - |   60 | `		/* Pop the (bad) updates argument and dispatch to the nearest catch. */` |
|   ! 0 |   61 | `		VmPopOperand(&pTos,1);` |
|     - |   62 | `		{` |
|     - |   63 | `			sxi32 iRp;` |
|   ! 0 |   64 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|   ! 0 |   65 | `				pc = iRp;` |
|   ! 0 |   66 | `				VM_EXIT_BREAK;` |
|     - |   67 | `			}` |
|     - |   68 | `		}` |
|   ! 0 |   69 | `		VM_EXIT_EXCEPTION;` |
|     - |   70 | `	}` |
|     - |   71 | `	/* The clone must be an object; OP_CLONE leaves NULL only on a prior failure` |
|     - |   72 | `	 * (already reported) — in that case just drop the updates and carry the NULL. */` |
|    17 |   73 | `	if( (pObj->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |   74 | `		VmPopOperand(&pTos,1);` |
|   ! 0 |   75 | `		VM_EXIT_BREAK;` |
|     - |   76 | `	}` |
|    17 |   77 | `	pClone = (ph7_class_instance *)pObj->x.pOther;` |
|    17 |   78 | `	pMap = (ph7_hashmap *)pUpdates->x.pOther;` |
|     - |   79 | `	/* Apply each update in insertion order (pFirst -> pPrev is the forward link). */` |
|    17 |   80 | `	pNode = pMap->pFirst;` |
|    35 |   81 | `	for( n = pMap->nEntry ; n > 0 && rcApply == SXRET_OK ; --n ){` |
|     - |   82 | `		ph7_value *pVal;` |
|     - |   83 | `		ph7_value sVal;` |
|     - |   84 | `		const char *zName;` |
|     - |   85 | `		sxu32 nName;` |
|     - |   86 | `		char zKeyBuf[64];` |
|    19 |   87 | `		if( pNode == 0 ){` |
|   ! 0 |   88 | `			break;` |
|     - |   89 | `		}` |
|    19 |   90 | `		pVal = (ph7_value *)SySetAt(&pVm->aMemObj,pNode->nValIdx);` |
|    19 |   91 | `		if( pNode->iType == HASHMAP_INT_NODE ){` |
|     - |   92 | ``			/* An int key becomes the property name (PHP: `$5`). */`` |
|   ! 0 |   93 | `			nName = SyBufferFormat(zKeyBuf,sizeof(zKeyBuf),"%qd",pNode->xKey.iKey);` |
|   ! 0 |   94 | `			zName = zKeyBuf;` |
|   ! 0 |   95 | `		}else{` |
|    19 |   96 | `			zName = (const char *)SyBlobData(&pNode->xKey.sKey);` |
|    19 |   97 | `			nName = SyBlobLength(&pNode->xKey.sKey);` |
|     - |   98 | `		}` |
|    19 |   99 | `		if( pVal ){` |
|     - |  100 | `			/* Snapshot the update value into a stack local FIRST: applying it may` |
|     - |  101 | `			 * create a dynamic property, whose slot reservation can reallocate` |
|     - |  102 | `			 * pVm->aMemObj and dangle pVal (a pointer into it). The name is safe` |
|     - |  103 | `			 * (it lives in the node's key blob / zKeyBuf, not in aMemObj). */` |
|    19 |  104 | `			PH7_MemObjInit(pVm,&sVal);` |
|    19 |  105 | `			PH7_MemObjLoad(pVal,&sVal);` |
|    19 |  106 | `			rcApply = VmCloneApplyUpdate(pVm,pClone,zName,nName,&sVal);` |
|    19 |  107 | `			PH7_MemObjRelease(&sVal);` |
|     9 |  108 | `		}` |
|    19 |  109 | `		pNode = pNode->pPrev;` |
|    10 |  110 | `	}` |
|    17 |  111 | `	if( rcApply == PH7_ABORT ){` |
|   ! 0 |  112 | `		VM_EXIT_ABORT;` |
|     - |  113 | `	}` |
|    17 |  114 | `	if( rcApply == PH7_EXCEPTION ){` |
|     - |  115 | `		/* An update threw (visibility / readonly / type). Pop the updates array` |
|     - |  116 | `		 * and hand control to the nearest catch, else propagate out of the loop. */` |
|     5 |  117 | `		VmPopOperand(&pTos,1);` |
|     - |  118 | `		{` |
|     - |  119 | `			sxi32 iRp;` |
|     5 |  120 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|     5 |  121 | `				pc = iRp;` |
|     5 |  122 | `				VM_EXIT_BREAK;` |
|     - |  123 | `			}` |
|     - |  124 | `		}` |
|   ! 0 |  125 | `		VM_EXIT_EXCEPTION;` |
|     - |  126 | `	}` |
|     - |  127 | `	/* Success: drop the updates array, leaving the clone on the stack. */` |
|    13 |  128 | `	VmPopOperand(&pTos,1);` |
|    13 |  129 | `	VM_EXIT_BREAK;` |
|   ! 0 |  130 | `	VM_EXIT_BREAK;` |
|     9 |  131 | `}` |
|     - |  132 |  |
|     - |  133 | `/*` |
|     - |  134 | ` * OP_NEW: body moved verbatim from the OP_NEW arm of` |
|     - |  135 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|     - |  136 | ` */` |
|  4794 |  137 | `PH7_PRIVATE VmOpRc VmExecOpNew(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|     5 |  138 | `{` |
|  4799 |  139 | `	ph7_value *pTos = pState->pTos;` |
|  4799 |  140 | `	ph7_value *pStack = pState->pStack;` |
|  4799 |  141 | `	VmInstr *aInstr = pState->aInstr;` |
|  4799 |  142 | `	sxi32 pc = pState->pc;` |
|     - |  143 | `	sxi32 rc;` |
|     - |  144 | `	/* Constructor arg count: compile-time args plus THIS new's own unpack` |
|     - |  145 | `	 * expansion (iP2 = hasSpread, transferred from the popped OP_CALL —` |
|     - |  146 | ``	 * `new C(...$args)` used to ignore the extras, leaving the expanded`` |
|     - |  147 | `	 * elements ABOVE the class-name slot and fataling "Class ' ' is not` |
|     - |  148 | `	 * defined"). VmSpreadOwnExtra counts only this new's own runs, so a nested` |
|     - |  149 | `	 * spread call in the ctor arg list stays scoped to itself. */` |
|  4799 |  150 | `	sxi32 nCtorArgs = pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|     - |  151 | `	ph7_value *pArg;` |
|  4799 |  152 | `	ph7_class *pClass = 0;` |
|     - |  153 | `	ph7_class_instance *pNew;` |
|  4799 |  154 | `	pArg = &pTos[-nCtorArgs]; /* Constructor arguments (if available) */` |
|     - |  155 | `	/* Same PHP 8.1 spread-key realignment as OP_CALL: build a per-actual-slot name` |
|     - |  156 | `	 * map when the ctor arg list unpacked string-keyed elements. NEW keeps the` |
|     - |  157 | `	 * class-name slot at pTos (above the args), so pArg is already the correct base;` |
|     - |  158 | `	 * the build also truncates this call's captured runs. */` |
|     - |  159 | `	VmCallArgMap sEffNewMap;` |
|  7196 |  160 | `	VmCallArgMap *pEffNewMap = VmEffCallArgMap(pVm,pInstr,pArg,` |
|  4794 |  161 | `		nCtorArgs > 0 ? (sxu32)nCtorArgs : 0,&sEffNewMap);` |
|  7196 |  162 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|  4799 |  163 | `		const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
|  4799 |  164 | `		sxu32 nCls = SyBlobLength(&pTos->sBlob);` |
|  4794 |  165 | `		if( (nCls == sizeof("self")-1 && SyMemcmp(zCls,"self",sizeof("self")-1) == 0)` |
|  4787 |  166 | `		 \|\| (nCls == sizeof("static")-1 && SyMemcmp(zCls,"static",sizeof("static")-1) == 0)` |
|  4757 |  167 | `		 \|\| (nCls == sizeof("parent")-1 && SyMemcmp(zCls,"parent",sizeof("parent")-1) == 0) ){` |
|     - |  168 | `			/* new self() / new static() / new parent(): resolve against the live` |
|     - |  169 | `			 * class context (LSB for static), sharing the FCC resolver. */` |
|    45 |  170 | `			pClass = VmFccResolveScope(&(*pVm),pTos);` |
|    45 |  171 | `			if( pClass && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) ){` |
|     - |  172 | `				/* Not new-able (the named path's iLoadable extract excludes these` |
|     - |  173 | `				 * before it ever gets here): php's wording, with the RESOLVED name. */` |
|   ! 0 |  174 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot instantiate %s %z",` |
|   ! 0 |  175 | `					(pClass->iFlags & PH7_CLASS_INTERFACE) ? "interface" : "abstract class",` |
|   ! 0 |  176 | `					&pClass->sName);` |
|   ! 0 |  177 | `				PH7_MemObjRelease(pTos);` |
|   ! 0 |  178 | `				if( nCtorArgs > 0 ){` |
|   ! 0 |  179 | `					VmPopOperand(&pTos,nCtorArgs);` |
|   ! 0 |  180 | `				}` |
|   ! 0 |  181 | `				VM_EXIT_ABORT;` |
|     - |  182 | `			}` |
|    23 |  183 | `		}else{` |
|     - |  184 | `			/* Try to extract the desired class */` |
|  4755 |  185 | `			pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,` |
|     - |  186 | `				TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|     5 |  187 | `		}` |
|  2397 |  188 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|     - |  189 | `		/* Take the base class from the loaded instance */` |
|   ! 0 |  190 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|   ! 0 |  191 | `	}` |
|  4799 |  192 | `	if( pClass == 0 ){` |
|     - |  193 | ``		/* php: `new NoSuch()` is a CATCHABLE Error ('Class "NoSuch" not found'), not a`` |
|     - |  194 | `		 * hard stop. Aborting here killed the script outright -- and with exit status 0,` |
|     - |  195 | `		 * so a caller could not even tell it had failed. */` |
|     - |  196 | `		SyBlob sErrM;` |
|     - |  197 | `		sxi32 rcErr;` |
|    10 |  198 | `		ph7_class *pNotNew = 0;` |
|    10 |  199 | `		SyBlobInit(&sErrM,&pVm->sAllocator);` |
|    10 |  200 | `		if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|     - |  201 | `			/* The extract above only accepts NEW-able classes, so an interface or an` |
|     - |  202 | `			 * abstract class comes back as 0 and used to be reported as "not found".` |
|     - |  203 | `			 * Look again without that filter so php's real message can be given. */` |
|    14 |  204 | `			pNotNew = PH7_VmExtractClass(&(*pVm),(const char *)SyBlobData(&pTos->sBlob),` |
|     8 |  205 | `				SyBlobLength(&pTos->sBlob),FALSE,0);` |
|     4 |  206 | `		}` |
|    10 |  207 | `		if( pNotNew && (pNotNew->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) ){` |
|     5 |  208 | `			SyBlobFormat(&sErrM,"Cannot instantiate %s %z",` |
|     4 |  209 | `				(pNotNew->iFlags & PH7_CLASS_INTERFACE) ? "interface" : "abstract class",` |
|     2 |  210 | `				&pNotNew->sName);` |
|     3 |  211 | `		}else{` |
|     6 |  212 | `			SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|     4 |  213 | `				SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob));` |
|     - |  214 | `		}` |
|     - |  215 | `		/* Settle the stack BEFORE throwing: the class-name slot becomes the (NULL)` |
|     - |  216 | `		 * expression result and the ctor arguments go. */` |
|    10 |  217 | `		if( nCtorArgs > 0 ){` |
|   ! 0 |  218 | `			VmPopOperand(&pTos,nCtorArgs);` |
|   ! 0 |  219 | `		}` |
|    10 |  220 | `		PH7_MemObjRelease(pTos);` |
|    10 |  221 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|    10 |  222 | `		pTos->nIdx = SXU32_HIGH;` |
|    14 |  223 | `		rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|     4 |  224 | `			SyBlobLength(&sErrM));` |
|    10 |  225 | `		SyBlobRelease(&sErrM);` |
|    10 |  226 | `		if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|    10 |  227 | `		rc = rcErr;` |
|    10 |  228 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
|  4791 |  229 | `	}else if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|     - |  230 | `		/* php 8.1: enums cannot be instantiated — a catchable Error, raised` |
|     - |  231 | `		 * BEFORE any construction (no instance, no __destruct). */` |
|     - |  232 | `		SyBlob sErrMsg;` |
|     3 |  233 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     3 |  234 | `		SyBlobFormat(&sErrMsg,"Cannot instantiate enum %z",&pClass->sName);` |
|     3 |  235 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     3 |  236 | `		if( nCtorArgs > 0 ){` |
|   ! 0 |  237 | `			VmPopOperand(&pTos,nCtorArgs);` |
|   ! 0 |  238 | `		}` |
|     3 |  239 | `		PH7_MemObjRelease(pTos);` |
|     3 |  240 | `		pTos->nIdx = SXU32_HIGH;` |
|     3 |  241 | `		VM_EXIT_BREAK;` |
|   ! 0 |  242 | `	}else{` |
|     - |  243 | `		ph7_class_method *pCons;` |
|     - |  244 | `		/* Check if a constructor is available — BEFORE instantiation: a` |
|     - |  245 | ``		 * visibility-denied `new` must not construct (nor later destruct)`` |
|     - |  246 | `		 * the object (band A #4). */` |
|  4789 |  247 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|  4789 |  248 | `		if( pCons == 0 ){` |
|  1857 |  249 | `			SyString *pName = &pClass->sName;` |
|     - |  250 | `			/* Check for a constructor with the same base class name */` |
|  1857 |  251 | `			pCons = PH7_ClassExtractMethod(pClass,pName->zString,pName->nByte);` |
|   926 |  252 | `		}` |
|     - |  253 | `		/* Constructor visibility (band A #4): __construct now KEEPS its` |
|     - |  254 | `		 * declared protection (PH7_NewClassMethod no longer forces it` |
|     - |  255 | ``		 * public), so `new C()` on a private/protected constructor from`` |
|     - |  256 | `		 * the wrong scope is php's catchable Error. Reflection's` |
|     - |  257 | `		 * newInstance path sets bReflectBypass like method invoke. */` |
|  4789 |  258 | `		if( pCons && pCons->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|     - |  259 | `			/* php binds non-public access by the member's DECLARING class, not the` |
|     - |  260 | `			 * instantiated class: a private constructor declared in a base is` |
|     - |  261 | `			 * reachable from that base's own methods even when instantiating a` |
|     - |  262 | ``			 * subclass (`new Child()` inside Base::factory()). __construct is a`` |
|     - |  263 | `			 * method, so pass its declaring class (sFunc.pUserData) — mirroring the` |
|     - |  264 | `			 * method-call visibility check — rather than pClass, whose hAttr lookup` |
|     - |  265 | `			 * for a method name always misses and falls to a wrong exact-class test. */` |
|    23 |  266 | `			ph7_class *pCtorDecl = pCons->sFunc.pUserData ? (ph7_class *)pCons->sFunc.pUserData : pClass;` |
|    23 |  267 | `			if( pVm->bReflectBypass ){` |
|   ! 0 |  268 | `				pVm->bReflectBypass = 0;` |
|    23 |  269 | `			}else if( !PH7_VmClassMemberAccess(&(*pVm),pCtorDecl,&pCons->sFunc.sName,pCons->iProtection,FALSE) ){` |
|     - |  270 | `				SyBlob sErrMsg;` |
|     7 |  271 | `				const char *zVis = pCons->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|     7 |  272 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     7 |  273 | `				SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from global scope",` |
|     3 |  274 | `					zVis,&pClass->sName);` |
|     7 |  275 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     - |  276 | `				/* Pop ctor args + release the class-name operand, leave NULL` |
|     - |  277 | `				 * as the expression value; the fetch-point router lands the` |
|     - |  278 | `				 * throw. No instance was created. */` |
|     7 |  279 | `				if( nCtorArgs > 0 ){` |
|   ! 0 |  280 | `					VmPopOperand(&pTos,nCtorArgs);` |
|   ! 0 |  281 | `				}` |
|     7 |  282 | `				PH7_MemObjRelease(pTos);` |
|     7 |  283 | `				pTos->nIdx = SXU32_HIGH;` |
|     7 |  284 | `				VM_EXIT_BREAK;` |
|     - |  285 | `			}` |
|     8 |  286 | `		}` |
|     - |  287 | `		/* Create a new class instance */` |
|  4783 |  288 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
|  4783 |  289 | `		if( pNew == 0 ){` |
|   ! 0 |  290 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     - |  291 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|   ! 0 |  292 | `				&pClass->sName` |
|     - |  293 | `			);` |
|   ! 0 |  294 | `			PH7_MemObjRelease(pTos);` |
|   ! 0 |  295 | `			if( nCtorArgs > 0 ){` |
|     - |  296 | `				/* Pop given arguments */` |
|   ! 0 |  297 | `				VmPopOperand(&pTos,nCtorArgs);` |
|   ! 0 |  298 | `			}` |
|   ! 0 |  299 | `			VM_EXIT_BREAK;` |
|     - |  300 | `		}` |
|  4783 |  301 | `		if( pCons ){` |
|     - |  302 | `			/* Call the class constructor.  Collect args in stack order and` |
|     - |  303 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|     - |  304 | `			 * receiving OP_CALL path runs its named-argument matching` |
|     - |  305 | `			 * (including variadic string-key packing). */` |
|  2931 |  306 | `			VmCallArgMap *pNewMap = pEffNewMap;` |
|     - |  307 | `			sxi32 rcCons;` |
|     - |  308 | `			SySet aArg; /* ctor argument scratch (the loop's shared set stays with OP_CALL) */` |
|  2931 |  309 | `			SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
|  6745 |  310 | `			while( pArg < pTos ){` |
|  3819 |  311 | `				SySetPut(&aArg,(const void *)&pArg);` |
|  3819 |  312 | `				pArg++;` |
|     5 |  313 | `			}` |
|     - |  314 | `			/* Too-few-arguments is php's catchable ArgumentCountError, raised` |
|     - |  315 | `			 * by the shared OP_CALL install path this ctor call routes through` |
|     - |  316 | `			 * (was a PHL-only notice here). */` |
|  2931 |  317 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
|  2931 |  318 | `			SySetRelease(&aArg); /* scratch dies here, on every exit path */` |
|     - |  319 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
|  2931 |  320 | `			if( pNew->iRef < 1 ){` |
|   ! 0 |  321 | `				pNew->iRef = 1;` |
|   ! 0 |  322 | `			}` |
|  2931 |  323 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|     - |  324 | `				/* The constructor raised: the half-constructed object must not` |
|     - |  325 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|     - |  326 | `				 * The class-name operand (and any leftover args) are released by` |
|     - |  327 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|     - |  328 | `				sxi32 iResumePc;` |
|   106 |  329 | `				PH7_ClassInstanceUnref(pNew);` |
|   106 |  330 | `				if( rcCons == PH7_ABORT ){` |
|     5 |  331 | `					VM_EXIT_ABORT;` |
|     - |  332 | `				}` |
|   101 |  333 | `				if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){` |
|     - |  334 | `					/* This frame's own try caught it in-place: tidy the stack` |
|     - |  335 | `					 * (pop ctor args + release the class-name slot) and resume. */` |
|    99 |  336 | `					if( nCtorArgs > 0 ){` |
|    91 |  337 | `						VmPopOperand(&pTos,nCtorArgs);` |
|    45 |  338 | `					}` |
|    99 |  339 | `					PH7_MemObjRelease(pTos);` |
|    99 |  340 | `					pc = iResumePc;` |
|    99 |  341 | `					VM_EXIT_BREAK;` |
|     - |  342 | `				}` |
|     3 |  343 | `				VM_EXIT_EXCEPTION;` |
|     - |  344 | `			}` |
|  1411 |  345 | `		}` |
|  4679 |  346 | `		if( nCtorArgs > 0 ){` |
|     - |  347 | `			/* Pop given arguments */` |
|  2617 |  348 | `			VmPopOperand(&pTos,nCtorArgs);` |
|  1306 |  349 | `		}` |
|  4679 |  350 | `		PH7_MemObjRelease(pTos);` |
|  4679 |  351 | `		pTos->x.pOther = pNew;` |
|  4679 |  352 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|     - |  353 | `	}` |
|  4679 |  354 | `	VM_EXIT_BREAK;` |
|   ! 0 |  355 | `	VM_EXIT_BREAK;` |
|  2402 |  356 | `}` |
|     - |  357 |  |
|     - |  358 | `/*` |
|     - |  359 | ` * OP_MEMBER: body moved verbatim from the OP_MEMBER arm of` |
|     - |  360 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|     - |  361 | ` */` |
| 45130 |  362 | `PH7_PRIVATE VmOpRc VmExecOpMember(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|     5 |  363 | `{` |
| 45135 |  364 | `	ph7_value *pTos = pState->pTos;` |
| 45135 |  365 | `	ph7_value *pStack = pState->pStack;` |
| 45135 |  366 | `	VmInstr *aInstr = pState->aInstr;` |
| 45135 |  367 | `	sxi32 pc = pState->pc;` |
|     - |  368 | `	sxi32 rc;` |
|     - |  369 | `	ph7_class_instance *pThis;` |
|     - |  370 | `	ph7_value *pNos;` |
|     - |  371 | `	SyString sName;` |
| 45135 |  372 | `	if( !pInstr->iP1 ){` |
| 42617 |  373 | `		pNos = &pTos[-1];` |
|     - |  374 | `#ifdef UNTRUST` |
|     - |  375 | `		if( pNos < pStack ){` |
|     - |  376 | `			VM_EXIT_ABORT;` |
|     - |  377 | `		}` |
|     - |  378 | `#endif` |
| 42617 |  379 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|     - |  380 | `			ph7_class *pClass;` |
|     - |  381 | `			/* Class already instantiated */` |
| 42607 |  382 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|     - |  383 | `			/* Point to the instantiated class */` |
| 42607 |  384 | `			pClass = pThis->pClass;` |
|     - |  385 | `			/* Extract attribute name first */` |
| 42607 |  386 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
| 42607 |  387 | `			if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|     - |  388 | `				/* Method call */` |
| 11237 |  389 | `				ph7_class_method *pMeth = 0;` |
| 11237 |  390 | `				if( sName.nByte > 0 ){` |
|     - |  391 | `					/* Extract the target method */` |
| 11237 |  392 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|  5616 |  393 | `				}` |
| 11237 |  394 | `				if( pMeth == 0 ){` |
|     9 |  395 | `					ph7_class_method *pCallMagic = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|     9 |  396 | `					if( pCallMagic ){` |
|     - |  397 | `						/* php: a missing method dispatches __call($name, $args) — the` |
|     - |  398 | `						 * args are only collected by the following OP_CALL, so stash the` |
|     - |  399 | `						 * receiver + class + original name and redirect the callee to` |
|     - |  400 | `						 * the hidden packing trampoline (band A #3b; pre-fix the name-` |
|     - |  401 | `						 * only call discarded everything and the call site failed with` |
|     - |  402 | `						 * "Invalid function name"). Stack: pop the method name, then` |
|     - |  403 | `						 * the receiver slot becomes the trampoline's callee name. */` |
|     7 |  404 | `						SyBlobReset(&pVm->sMagicCallName);` |
|     7 |  405 | `						SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);` |
|     7 |  406 | `						pThis->iRef++;` |
|     7 |  407 | `						pVm->pMagicCallThis = pThis;` |
|     7 |  408 | `						pVm->pMagicCallClass = pClass;` |
|     7 |  409 | `						VmPopOperand(&pTos,1);` |
|     7 |  410 | `						PH7_MemObjRelease(pTos);` |
|     7 |  411 | `						SyBlobAppend(&pTos->sBlob,"__phl_magic_call",sizeof("__phl_magic_call")-1);` |
|     7 |  412 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|     4 |  413 | `					}else{` |
|     - |  414 | `						{` |
|     - |  415 | `							/* php raises a catchable Error here; PH7 printed a notice and CARRIED ON` |
|     - |  416 | `							 * with NULL, so the call silently produced nothing. */` |
|     - |  417 | `							SyBlob sErrM;` |
|     - |  418 | `							sxi32 rcErr;` |
|     3 |  419 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|     3 |  420 | `							SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",&pClass->sName,&sName);` |
|     3 |  421 | `							VmPopOperand(&pTos,1);` |
|     3 |  422 | `							PH7_MemObjRelease(pTos);` |
|     3 |  423 | `							pTos->nIdx = SXU32_HIGH;` |
|     4 |  424 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|     1 |  425 | `								SyBlobLength(&sErrM));` |
|     3 |  426 | `							SyBlobRelease(&sErrM);` |
|     3 |  427 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     3 |  428 | `							rc = rcErr;` |
|     3 |  429 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     - |  430 | `						}` |
|     - |  431 | `					}` |
|     4 |  432 | `				}else{` |
| 11229 |  433 | `					ph7_class_method *pDeniedCall = 0;` |
| 11224 |  434 | `					if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC` |
|  8127 |  435 | `					 && !PH7_VmClassMemberAccess(&(*pVm),` |
|  2510 |  436 | `						pMeth->sFunc.pUserData ? (ph7_class *)pMeth->sFunc.pUserData : pClass,` |
|  1255 |  437 | `						&sName,pMeth->iProtection,FALSE) ){` |
|    14 |  438 | `						int bRebound = 0;` |
|    14 |  439 | `						if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     - |  440 | `							/* php: when the instance's class SHADOWS the caller's` |
|     - |  441 | `							 * private method (child redeclares private m), code in` |
|     - |  442 | `							 * the caller's class dispatches its OWN private m, not` |
|     - |  443 | `							 * the shadow. Rebind to the calling scope's method when` |
|     - |  444 | `							 * that scope declares a private one of this name. */` |
|     9 |  445 | `							VmFrame *pFrameLocal = VmSkipExceptionFrames(pVm->pFrame);` |
|     9 |  446 | `							ph7_vm_func *pCallerFunc = (ph7_vm_func *)pFrameLocal->pUserData;` |
|     9 |  447 | `							if( pCallerFunc && (pCallerFunc->iFlags & VM_FUNC_CLASS_METHOD) && pCallerFunc->pUserData ){` |
|     3 |  448 | `								ph7_class *pScope = (ph7_class *)pCallerFunc->pUserData;` |
|     3 |  449 | `								ph7_class_method *pOwn = PH7_ClassExtractMethod(pScope,sName.zString,sName.nByte);` |
|     2 |  450 | `								if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE` |
|     3 |  451 | `								 && (ph7_class *)pOwn->sFunc.pUserData == pScope ){` |
|     3 |  452 | `									pMeth = pOwn;` |
|     3 |  453 | `									bRebound = 1;` |
|     1 |  454 | `								}` |
|     1 |  455 | `							}` |
|     4 |  456 | `						}` |
|    14 |  457 | `						if( !bRebound ){` |
|     - |  458 | `							/* Inaccessible from this scope: php routes through __call when` |
|     - |  459 | `							 * declared (band A #3b); without it OP_CALL raises its` |
|     - |  460 | `							 * "Call to private/protected method" Error as before. */` |
|    12 |  461 | `							pDeniedCall = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|     5 |  462 | `						}` |
|     6 |  463 | `					}` |
| 11229 |  464 | `					if( pDeniedCall ){` |
|     3 |  465 | `						SyBlobReset(&pVm->sMagicCallName);` |
|     3 |  466 | `						SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);` |
|     3 |  467 | `						pThis->iRef++;` |
|     3 |  468 | `						pVm->pMagicCallThis = pThis;` |
|     3 |  469 | `						pVm->pMagicCallClass = pClass;` |
|     3 |  470 | `						VmPopOperand(&pTos,1);` |
|     3 |  471 | `						PH7_MemObjRelease(pTos);` |
|     3 |  472 | `						SyBlobAppend(&pTos->sBlob,"__phl_magic_call",sizeof("__phl_magic_call")-1);` |
|     3 |  473 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|     2 |  474 | `					}else{` |
|     - |  475 | `						/* Push method name on the stack */` |
| 11227 |  476 | `						PH7_MemObjRelease(pTos);` |
| 11227 |  477 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
| 11227 |  478 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|     - |  479 | `					}` |
|     - |  480 | `				}` |
| 11235 |  481 | `				pTos->nIdx = SXU32_HIGH;` |
|  5620 |  482 | `			}else{` |
|     - |  483 | `				/* Attribute access. iP2: 0 = read, 2 = unset, 3 = isset, 4 = empty. */` |
| 31375 |  484 | `				VmClassAttr *pObjAttr = 0;` |
| 31375 |  485 | `				SyHashEntry *pEntry = 0;` |
|     - |  486 | `				/* Extract the target attribute */` |
| 31375 |  487 | `				if( sName.nByte > 0 ){` |
| 31375 |  488 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
| 31375 |  489 | `					if( pEntry ){` |
|     - |  490 | `						/* Point to the attribute value */` |
| 30989 |  491 | `						pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
| 15492 |  492 | `					}` |
| 15685 |  493 | `				}` |
| 31375 |  494 | `				if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|     - |  495 | `					/* unset($o->prop): remove the property entirely so it disappears from` |
|     - |  496 | `					 * foreach / json_encode / get_object_vars / (array) — matching PHP (a value-only` |
|     - |  497 | `					 * release would leave a zombie null entry). Leave a NULL constant on the stack so` |
|     - |  498 | `					 * the trailing generic unset() builtin is a no-op (mirrors LOAD_IDX iP2=5).` |
|     - |  499 | `					 * php dispatches __unset($name) for a MISSING or INACCESSIBLE property` |
|     - |  500 | `					 * (band A #3b — pre-fix an inaccessible private was silently DELETED from` |
|     - |  501 | `					 * outside the class); without __unset, an inaccessible unset is php's` |
|     - |  502 | `					 * catchable "Cannot access ..." Error and a missing one stays a no-op. */` |
|    39 |  503 | `					int bUnsAccessible = pEntry ? PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) : 0;` |
|    41 |  504 | `					if( pEntry && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) != 0 ){` |
|     - |  505 | `						/* php 8.4: a hooked property (virtual or backed) can never be` |
|     - |  506 | `						 * unset — catchable Error, even from inside its own hook body` |
|     - |  507 | `						 * (probe-verified). */` |
|     - |  508 | `						SyBlob sErrMsg;` |
|     5 |  509 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     5 |  510 | `						SyBlobFormat(&sErrMsg,"Cannot unset hooked property %z::$%z",` |
|     4 |  511 | `							&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|     5 |  512 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|    37 |  513 | `					}else if( pEntry && bUnsAccessible ){` |
|    27 |  514 | `						PH7_VmReleaseInstanceAttr(&(*pVm),pObjAttr);` |
|    27 |  515 | `						SyHashDeleteEntry2(pEntry);` |
|    14 |  516 | `					}else{` |
|     9 |  517 | `						ph7_class_method *pUnsetMagic = PH7_ClassExtractMethod(pClass,"__unset",sizeof("__unset")-1);` |
|     9 |  518 | `						if( pUnsetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'u') ){` |
|     7 |  519 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'u');` |
|     7 |  520 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__unset",sizeof("__unset")-1,&sName,0);` |
|     7 |  521 | `							VmMagicGuardPop(pVm);` |
|     6 |  522 | `						}else if( pEntry ){` |
|     - |  523 | `							/* Inaccessible and no __unset: php's catchable Error. Parked on` |
|     - |  524 | `							 * the boundary rail; the op completes benignly and the` |
|     - |  525 | `							 * fetch-point router lands it. */` |
|     - |  526 | `							SyBlob sErrMsg;` |
|     3 |  527 | `							const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|     3 |  528 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     3 |  529 | `							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",zVis,&pClass->sName,&sName);` |
|     3 |  530 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     1 |  531 | `						}` |
|     - |  532 | `						/* Missing property without __unset: silent no-op (php). */` |
|     - |  533 | `					}` |
|    39 |  534 | `					VmPopOperand(&pTos,1);    /* pop the attribute name */` |
|    39 |  535 | `					PH7_MemObjRelease(pTos);  /* release the object on the stack ($o's stack ref) */` |
|    39 |  536 | `					pTos->nIdx = SXU32_HIGH;  /* NULL constant */` |
|   259 |  537 | `					VM_EXIT_BREAK;` |
|     - |  538 | `				}` |
| 31337 |  539 | `				if( pObjAttr == 0 && sName.nByte > 0 ){` |
|     - |  540 | `					/* Member not present on the instance and the next instruction writes/modifies it` |
|     - |  541 | ``					 * (store, array-append/keyed-write, `??=`, ++/--, or a compound-assign — see`` |
|     - |  542 | `					 * VmMemberNextIsWrite; the compiler always emits a terminating PH7_OP_DONE so` |
|     - |  543 | `					 * pInstr+1 is in-bounds). Create the property so the operation lands on a real slot` |
|     - |  544 | ``					 * (PHP auto-vivifies a fresh property for all these forms, not just `=`):`` |
|     - |  545 | `					 *   - a DECLARED prop that was unset() and is re-assigned → recreate it` |
|     - |  546 | `					 *     (PHP re-appends it at the end), OR` |
|     - |  547 | `					 *   - a dynamic prop on a dynamic-allowing class (stdClass).` |
|     - |  548 | `					 * The created slot is NULL with a real nIdx, which the modify-op then vivifies` |
|     - |  549 | `					 * (NULL→array for [], NULL→0/"" for ++/.=) and writes back in place.` |
|     - |  550 | `					 * Two signals: the compiler tags the member itself iP2=PH7_MEMBER_WRITE when it is` |
|     - |  551 | ``					 * the base of a write-subscript / `??=` (the modify-op is not the immediately-next`` |
|     - |  552 | `					 * instruction there), and for ++/--/compound-assign/store the next opcode is the` |
|     - |  553 | `					 * modify-op directly (VmMemberNextIsWrite). */` |
|   385 |  554 | `					VmInstr *pNext = pInstr + 1;` |
|   385 |  555 | `					if( pInstr->iP2 == PH7_MEMBER_WRITE \|\| VmMemberNextIsWrite(pNext) ){` |
|    85 |  556 | `						ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pThis->pClass,sName.zString,sName.nByte);` |
|    85 |  557 | `						if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|     7 |  558 | `							VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pObjAttr);` |
|     4 |  559 | `						}else{` |
|     - |  560 | `							/* php 8 semantics (band A #3b): a PLAIN store to a missing` |
|     - |  561 | `							 * property dispatches __set($name,$value) when declared —` |
|     - |  562 | `							 * the value only exists at the following OP_STORE, so park` |
|     - |  563 | `							 * the receiver+name for it (one-instruction lifetime; the` |
|     - |  564 | `							 * guard makes a same-name write inside __set fall through` |
|     - |  565 | `							 * to dynamic creation, like php). Without __set — or for a` |
|     - |  566 | `							 * subscript-write base / read-modify-write, which php does` |
|     - |  567 | `							 * NOT route through __set — create a dynamic property on` |
|     - |  568 | `							 * ANY class with the 8.2 deprecation (suppressed for` |
|     - |  569 | `							 * stdClass and #[AllowDynamicProperties]); a readonly` |
|     - |  570 | `							 * class raises php's catchable Error instead. */` |
|    79 |  571 | `							ph7_class_method *pSetMagic = 0;` |
|    79 |  572 | `							ph7_class_method *pCoalIsset = 0, *pCoalGet = 0, *pCoalSet = 0;` |
|    79 |  573 | `							int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);` |
|    79 |  574 | `							if( bPlainStore ){` |
|    61 |  575 | `								pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|    29 |  576 | `							}` |
|    79 |  577 | `							if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){` |
|     9 |  578 | `								pThis->iRef++;` |
|     9 |  579 | `								pVm->pMagicSetThis = pThis;` |
|     9 |  580 | `								SyBlobReset(&pVm->sMagicSetName);` |
|     9 |  581 | `								SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|     - |  582 | `								/* pObjAttr stays NULL; the miss path below stays silent. */` |
|    73 |  583 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|    18 |  584 | `							 && pNext->iOp == PH7_OP_NULLC_JMP` |
|    17 |  585 | `							 && ((pCoalIsset = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1)) != 0` |
|     6 |  586 | `							  \|\| (pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)) != 0` |
|     3 |  587 | `							  \|\| (pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1)) != 0) ){` |
|     - |  588 | ``								/* `$o->p ??= v` on a missing property with magic accessors:`` |
|     - |  589 | `								 * php consults __isset first when declared — false means` |
|     - |  590 | `								 * assign directly through __set with NO __get call (the` |
|     - |  591 | `								 * test value stays null); true (or no __isset) means __get` |
|     - |  592 | `								 * provides the test value. A COAL_MAGIC entry is pushed` |
|     - |  593 | `								 * for the OP_NULLC_STORE at the jump target; the` |
|     - |  594 | `								 * fetch-point sweep drops it when the short-circuit jump` |
|     - |  595 | `								 * skips the assign or a throw abandons the RHS. Without` |
|     - |  596 | `								 * __set the assign side keeps the pre-existing loud` |
|     - |  597 | `								 * "Cannot perform assignment" path (php would create a` |
|     - |  598 | `								 * dynamic property there — recorded residual). Note the` |
|     - |  599 | `								 * condition's short-circuit assignment chain: a hit on an` |
|     - |  600 | `								 * earlier method leaves the later pointers unresolved, so` |
|     - |  601 | `								 * re-resolve the leftovers here (each is one hash probe,` |
|     - |  602 | `								 * only on this ??=-miss path). */` |
|     - |  603 | `								ph7_value sTest;` |
|     7 |  604 | `								int bMiss = 0; /* __isset said false: skip __get, test value stays null */` |
|     7 |  605 | `								if( pCoalIsset != 0 ){` |
|     5 |  606 | `									pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|     2 |  607 | `								}` |
|     7 |  608 | `								if( pCoalIsset != 0 \|\| pCoalGet != 0 ){` |
|     7 |  609 | `									pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|     3 |  610 | `								}` |
|     7 |  611 | `								PH7_MemObjInit(pVm,&sTest);` |
|     7 |  612 | `								if( pCoalIsset && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|     - |  613 | `									ph7_value sIssetRet;` |
|     5 |  614 | `									PH7_MemObjInit(pVm,&sIssetRet);` |
|     5 |  615 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|     5 |  616 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|     5 |  617 | `									VmMagicGuardPop(pVm);` |
|     5 |  618 | `									PH7_MemObjToBool(&sIssetRet);` |
|     5 |  619 | `									bMiss = sIssetRet.x.iVal == 0;` |
|     5 |  620 | `									PH7_MemObjRelease(&sIssetRet);` |
|     2 |  621 | `								}` |
|     7 |  622 | `								if( !bMiss && pCoalGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|     5 |  623 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     5 |  624 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sTest);` |
|     5 |  625 | `									VmMagicGuardPop(pVm);` |
|     2 |  626 | `								}` |
|     6 |  627 | `								if( pCoalSet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s')` |
|     7 |  628 | `								 && pVm->nBoundaryRc == 0 ){` |
|     - |  629 | `									VmHookRmw sPend;` |
|     7 |  630 | `									sPend.iKind = VM_HOOK_PEND_COAL_MAGIC;` |
|     7 |  631 | `									sPend.pThis = pThis;` |
|     7 |  632 | `									sPend.pAttr = 0;` |
|     7 |  633 | `									sPend.nBackIdx = SXU32_HIGH;` |
|     7 |  634 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|     7 |  635 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|     7 |  636 | `									SyBlobAppend(&sPend.sName,(const void *)sName.zString,sName.nByte);` |
|     7 |  637 | `									sPend.pOwnerStack = (void *)pStack;` |
|     7 |  638 | `									sPend.pInstrs = (void *)aInstr;` |
|     7 |  639 | `									sPend.nJmpPc = (sxu32)(pc + 1);        /* the OP_NULLC_JMP */` |
|     7 |  640 | `									sPend.nPc = (sxu32)(pNext->iP2 - 1);   /* the OP_NULLC_STORE */` |
|     7 |  641 | `									pThis->iRef++;` |
|     7 |  642 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|     3 |  643 | `								}` |
|     - |  644 | `								/* Pop the attribute name; the test value becomes the` |
|     - |  645 | `								 * expression slot (a temp, not an lvalue). */` |
|     7 |  646 | `								VmPopOperand(&pTos,1);` |
|     7 |  647 | `								pThis->iRef++;` |
|     7 |  648 | `								PH7_MemObjRelease(pTos);` |
|     7 |  649 | `								PH7_MemObjStore(&sTest,pTos);` |
|     7 |  650 | `								pTos->nIdx = SXU32_HIGH;` |
|     7 |  651 | `								PH7_MemObjRelease(&sTest);` |
|     7 |  652 | `								PH7_ClassInstanceUnref(pThis);` |
|     7 |  653 | `								VM_EXIT_BREAK;` |
|    62 |  654 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|    12 |  655 | `							 && !VmMemberNextIsWrite(pNext)` |
|    15 |  656 | `							 && PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) ){` |
|     - |  657 | `								/* Subscript-write base on a class with __get: php reads` |
|     - |  658 | `								 * through the magic layer (the write lands on the temp and` |
|     - |  659 | `								 * is lost, like php's indirect-modification case). A PLAIN` |
|     - |  660 | `								 * store (bPlainStore — the compiler tags those` |
|     - |  661 | `								 * PH7_MEMBER_WRITE too) is NOT this case: it falls through` |
|     - |  662 | `								 * to dynamic creation. A direct ++/--/compound-assign` |
|     - |  663 | `								 * (VmMemberNextIsWrite — the compiler now tags those` |
|     - |  664 | `								 * PH7_MEMBER_WRITE as well) is NOT this case either: it` |
|     - |  665 | `								 * falls through to dynamic creation like before (the` |
|     - |  666 | `								 * recorded RMW-vivifies-instead-of-__get residual, §7).` |
|     - |  667 | `								 * Leave the miss path — the read gate below dispatches` |
|     - |  668 | `								 * __get. */` |
|    65 |  669 | `							}else if( pThis->pClass->iFlags & PH7_CLASS_READONLY ){` |
|     - |  670 | `								SyBlob sErrMsg;` |
|   ! 0 |  671 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|   ! 0 |  672 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|   ! 0 |  673 | `									&pThis->pClass->sName,&sName);` |
|   ! 0 |  674 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|    62 |  675 | `							}else if( !VmClassAllowsDynamicProps(pVm,pThis->pClass)` |
|    36 |  676 | `							       && !VmClassHasAttributeNamed(pThis->pClass,"AllowDynamicProperties",sizeof("AllowDynamicProperties")-1) ){` |
|     - |  677 | `								/* php 8.2 only DEPRECATES creating a dynamic property on a` |
|     - |  678 | `								 * class without #[AllowDynamicProperties]; PHL rejects it.` |
|     - |  679 | `								 * stdClass / __set / declared props are unaffected. */` |
|     - |  680 | `								SyBlob sErrMsg;` |
|     3 |  681 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     3 |  682 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|     2 |  683 | `									&pThis->pClass->sName,&sName);` |
|     3 |  684 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     2 |  685 | `							}else{` |
|    63 |  686 | `								PH7_VmCreateDynamicAttr(&(*pVm),pThis,sName.zString,sName.nByte,&pObjAttr);` |
|     - |  687 | `							}` |
|     - |  688 | `						}` |
|    38 |  689 | `					}` |
|   188 |  690 | `				}` |
| 31331 |  691 | `				if( pObjAttr == 0 ){` |
|     - |  692 | `					/* Missing property. On a plain READ, php dispatches __get($name) and the` |
|     - |  693 | `					 * expression takes its RETURN VALUE (band A #3a — pre-fix the result was` |
|     - |  694 | `					 * discarded and a PHL-native warn fired even when __get existed). isset/` |
|     - |  695 | `					 * empty context consults __isset first (then __get for empty()'s value` |
|     - |  696 | `					 * test); a plain-store write context parked a pending __set above. A` |
|     - |  697 | `					 * self-recursive read of the same property falls back to the` |
|     - |  698 | `					 * undefined-property path via the guard, like php's property guard. */` |
|   312 |  699 | `					ph7_class_method *pGetMagic = 0;` |
|   312 |  700 | `					if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|    43 |  701 | `						ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|    43 |  702 | `						if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|     - |  703 | `							ph7_value sIssetRet;` |
|     - |  704 | `							int bSet;` |
|    13 |  705 | `							PH7_MemObjInit(pVm,&sIssetRet);` |
|    13 |  706 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|    13 |  707 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|    13 |  708 | `							VmMagicGuardPop(pVm);` |
|    13 |  709 | `							PH7_MemObjToBool(&sIssetRet);` |
|    13 |  710 | `							bSet = sIssetRet.x.iVal != 0;` |
|    13 |  711 | `							PH7_MemObjRelease(&sIssetRet);` |
|    13 |  712 | `							if( bSet && pInstr->iP2 == PH7_MEMBER_EMPTY ){` |
|     - |  713 | `								/* empty(): __isset said set — fetch the value via __get` |
|     - |  714 | `								 * (php) so emptiness is judged on the real value. */` |
|     3 |  715 | `								ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|     - |  716 | `								ph7_value sEmptyVal;` |
|     3 |  717 | `								PH7_MemObjInit(pVm,&sEmptyVal);` |
|     3 |  718 | `								if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|     3 |  719 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     3 |  720 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|     3 |  721 | `									VmMagicGuardPop(pVm);` |
|     1 |  722 | `								}` |
|     3 |  723 | `								VmPopOperand(&pTos,1);` |
|     3 |  724 | `								pThis->iRef++;` |
|     3 |  725 | `								PH7_MemObjRelease(pTos);` |
|     3 |  726 | `								PH7_MemObjStore(&sEmptyVal,pTos);` |
|     3 |  727 | `								pTos->nIdx = SXU32_HIGH;` |
|     3 |  728 | `								PH7_MemObjRelease(&sEmptyVal);` |
|     3 |  729 | `								PH7_ClassInstanceUnref(pThis);` |
|     3 |  730 | `								VM_EXIT_BREAK;` |
|     - |  731 | `							}` |
|     - |  732 | `							/* isset(): the truth of __isset IS the answer — push a non-null` |
|     - |  733 | `							 * marker for true, NULL for false (isset only tests null-ness). */` |
|    11 |  734 | `							VmPopOperand(&pTos,1);` |
|    11 |  735 | `							pThis->iRef++;` |
|    11 |  736 | `							PH7_MemObjRelease(pTos);` |
|    11 |  737 | `							if( bSet ){` |
|     5 |  738 | `								pTos->x.iVal = 1;` |
|     5 |  739 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     2 |  740 | `							}` |
|    11 |  741 | `							pTos->nIdx = SXU32_HIGH;` |
|    11 |  742 | `							PH7_ClassInstanceUnref(pThis);` |
|    11 |  743 | `							VM_EXIT_BREAK;` |
|     - |  744 | `						}` |
|    15 |  745 | `					}` |
|   300 |  746 | `					if( !VmMemberCtxIsLookup(pInstr->iP2) && !VmMemberNextIsWrite(pInstr + 1) ){` |
|     - |  747 | `						/* Plain reads AND the ??=/subscript write-base (PH7_MEMBER_WRITE,` |
|     - |  748 | `						 * which php reads through __get); read-modify-write forms are` |
|     - |  749 | `						 * excluded (they vivified above — approximate, recorded). */` |
|   259 |  750 | `						pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|   129 |  751 | `					}` |
|   300 |  752 | `					if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|     - |  753 | `						ph7_value sMagicRet;` |
|   257 |  754 | `						PH7_MemObjInit(pVm,&sMagicRet);` |
|   257 |  755 | `						VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|   257 |  756 | `						PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|   257 |  757 | `						VmMagicGuardPop(pVm);` |
|     - |  758 | `						/* Pop the attribute name, replace the object slot with the magic` |
|     - |  759 | `						 * result (a temp, not an lvalue — nIdx stays constant). A throw` |
|     - |  760 | `						 * from __get parked at the boundary; the fetch-point router lands` |
|     - |  761 | `						 * it and abandons this slot. */` |
|   257 |  762 | `						VmPopOperand(&pTos,1);` |
|   257 |  763 | `						pThis->iRef++;` |
|   257 |  764 | `						PH7_MemObjRelease(pTos);` |
|   257 |  765 | `						PH7_MemObjStore(&sMagicRet,pTos);` |
|   257 |  766 | `						pTos->nIdx = SXU32_HIGH;` |
|   257 |  767 | `						PH7_MemObjRelease(&sMagicRet);` |
|   257 |  768 | `						PH7_ClassInstanceUnref(pThis);` |
|   257 |  769 | `						VM_EXIT_BREAK;` |
|     - |  770 | `					}` |
|     - |  771 | `					/* No __get (or guard held): load null. In isset()/empty() context (iP2 3/4)` |
|     - |  772 | `					 * PHP returns false/true SILENTLY, so suppress the read-miss warning there` |
|     - |  773 | `					 * (mirrors the array LOAD_IDX iP2=4/6 suppression); likewise when a` |
|     - |  774 | `					 * pending __set was parked above (the following OP_STORE dispatches it —` |
|     - |  775 | `					 * nothing is "undefined" about that write) or a throw is parked on the` |
|     - |  776 | `					 * boundary rail (e.g. the readonly-class dynamic-property Error — the` |
|     - |  777 | `					 * fetch-point router lands it right after this op). */` |
|    42 |  778 | `					if( !VmMemberCtxIsLookup(pInstr->iP2) && pVm->pMagicSetThis == 0` |
|    10 |  779 | `					 && pVm->nBoundaryRc == 0 ){` |
|     4 |  780 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|     1 |  781 | `							&pClass->sName,&sName);` |
|     1 |  782 | `					}` |
|    21 |  783 | `				}` |
| 31063 |  784 | `				VmPopOperand(&pTos,1);` |
|     - |  785 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|     - |  786 | `				 * This is due to the following case:` |
|     - |  787 | `				 *     (new TestClass())->foo;` |
|     - |  788 | `				 */` |
| 31063 |  789 | `				pThis->iRef++;` |
| 31063 |  790 | `				PH7_MemObjRelease(pTos);` |
| 31063 |  791 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
| 31063 |  792 | `				if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|     - |  793 | ``					/* `$o->p =& $x`: stash the resolved instance property slot for the`` |
|     - |  794 | `					 * following member-marked OP_STORE_REF, which rebinds it to alias the` |
|     - |  795 | `					 * source variable. Do NOT run the read/hook/magic/uninit machinery` |
|     - |  796 | `					 * below — a reference bind neither reads the value nor triggers` |
|     - |  797 | `					 * get/set hooks or an uninitialized-typed Error. pThis stays retained` |
|     - |  798 | `					 * (the iRef++ above); OP_STORE_REF releases it. */` |
|     9 |  799 | `					if( pObjAttr ){` |
|     9 |  800 | `						pVm->pRefTargetAttr = pObjAttr;` |
|     9 |  801 | `						pVm->pRefTargetThis = pThis;` |
|     9 |  802 | `						pVm->pRefTargetStaticAttr = 0;` |
|     9 |  803 | `						pTos->nIdx = pObjAttr->nIdx;` |
|     5 |  804 | `					}else{` |
|     - |  805 | `						/* Missing/inaccessible target: leave nothing stashed; OP_STORE_REF` |
|     - |  806 | `						 * no-ops. Balance the retain. */` |
|   ! 0 |  807 | `						pVm->pRefTargetAttr = 0;` |
|   ! 0 |  808 | `						pVm->pRefTargetThis = 0;` |
|   ! 0 |  809 | `						pVm->pRefTargetStaticAttr = 0;` |
|   ! 0 |  810 | `						PH7_ClassInstanceUnref(pThis);` |
|     - |  811 | `					}` |
|     9 |  812 | `					VM_EXIT_BREAK;` |
|     - |  813 | `				}` |
| 31055 |  814 | `				if( pObjAttr ){` |
| 31013 |  815 | `					ph7_value *pValue = 0; /* cc warning */` |
|     - |  816 | `					/* Check attribute access */` |
| 31013 |  817 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
| 30990 |  818 | `						if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET))` |
| 15642 |  819 | `						 && !VmHookGuardHeld(pVm,(void *)pThis,&sName) ){` |
|     - |  820 | `							/* PHP 8.4 property hooks: route reads, writes, and the` |
|     - |  821 | `							 * read-modify-write forms through the synthesized hook` |
|     - |  822 | `							 * methods. A held guard (either kind) means we are INSIDE` |
|     - |  823 | `							 * one of this property's own hook bodies — fall through to` |
|     - |  824 | `` 							 * the raw backing slot for BOTH directions (php: `$this->x` `` |
|     - |  825 | `							 * within any of x's hooks addresses the backing store). */` |
|   151 |  826 | `							VmInstr *pNextH = pInstr + 1;` |
|   151 |  827 | `							int bPlainStore = (pNextH->iOp == PH7_OP_STORE && pNextH->iP2 != 0);` |
|   262 |  828 | `							int bCoalesceW = pInstr->iP2 == PH7_MEMBER_WRITE` |
|   150 |  829 | `								&& pNextH->iOp == PH7_OP_NULLC_JMP;` |
|     - |  830 | `							/* ++/--/compound-assign: the modify op FOLLOWS the member` |
|     - |  831 | `							 * directly (the compiler tags compound-assign members` |
|     - |  832 | `							 * PH7_MEMBER_WRITE too, so classify by the next op, not iP2;` |
|     - |  833 | `							 * the !bPlainStore term is load-bearing — VmMemberNextIsWrite` |
|     - |  834 | `							 * returns 1 for a member OP_STORE too) */` |
|   151 |  835 | `							int bRmwNext = !bPlainStore && VmMemberNextIsWrite(pNextH);` |
|   177 |  836 | `							int bSubscriptW = !bPlainStore && !bCoalesceW && !bRmwNext` |
|   210 |  837 | `								&& pInstr->iP2 == PH7_MEMBER_WRITE;` |
|   151 |  838 | `							if( bPlainStore ){` |
|     - |  839 | `								/* Arm the pending hook-set for the following OP_STORE — it` |
|     - |  840 | `								 * dispatches the set hook, or throws the read-only Error` |
|     - |  841 | `								 * when the property only has a get hook. (The scalar` |
|     - |  842 | `								 * transient is safe here: its window is exactly one` |
|     - |  843 | `								 * instruction, MEMBER -> STORE, nothing runs in between.) */` |
|    31 |  844 | `								pThis->iRef++;` |
|    31 |  845 | `								pVm->pHookSetThis = pThis;` |
|    31 |  846 | `								pVm->pHookSetAttr = pObjAttr->pAttr;` |
|    31 |  847 | `								pVm->nHookSetIdx = pObjAttr->nIdx;` |
|    31 |  848 | `								pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|    31 |  849 | `								PH7_ClassInstanceUnref(pThis);` |
|    31 |  850 | `								VM_EXIT_BREAK;` |
|     - |  851 | `							}` |
|   121 |  852 | `							if( bSubscriptW ){` |
|     - |  853 | `								/* Subscript write base ($o->m[] = v, $o->m[$k] = v):` |
|     - |  854 | `								 * php's catchable Error, with or without a set hook` |
|     - |  855 | ``								 * (only a by-ref `&get` hook would allow it — a loud`` |
|     - |  856 | `								 * compile error in PHL). The op completes benignly with` |
|     - |  857 | `								 * a null temp; the fetch-point router lands the throw. */` |
|     - |  858 | `								SyBlob sErrMsg;` |
|     5 |  859 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     5 |  860 | `								SyBlobFormat(&sErrMsg,"Indirect modification of %z::$%z is not allowed",` |
|     4 |  861 | `									&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|     5 |  862 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     5 |  863 | `								PH7_ClassInstanceUnref(pThis);` |
|     5 |  864 | `								VM_EXIT_BREAK;` |
|     - |  865 | `							}` |
|   116 |  866 | `							if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|    59 |  867 | `							 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|     - |  868 | `								/* VIRTUAL set-only property: there is no backing store to` |
|     - |  869 | `								 * fall back to, so EVERY read context — plain read,` |
|     - |  870 | `								 * isset()/empty() (php throws there too, probe-verified),` |
|     - |  871 | `								 * the ??= test, an RMW read — is php's catchable` |
|     - |  872 | `								 * write-only Error. */` |
|     - |  873 | `								SyBlob sErrMsg;` |
|     9 |  874 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     9 |  875 | `								SyBlobFormat(&sErrMsg,"Property %z::$%z is write-only",` |
|     8 |  876 | `									&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|     9 |  877 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     9 |  878 | `								PH7_ClassInstanceUnref(pThis);` |
|     9 |  879 | `								VM_EXIT_BREAK;` |
|     - |  880 | `							}` |
|   109 |  881 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|     - |  882 | `								/* isset()/empty() on a hooked property calls the get hook` |
|     - |  883 | `								 * (php): isset is "get() !== null", empty tests the value. */` |
|     - |  884 | `								ph7_value sHookRet;` |
|     5 |  885 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|     5 |  886 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|     5 |  887 | `									if( pInstr->iP2 == PH7_MEMBER_ISSET ){` |
|     3 |  888 | `										pTos->x.iVal = (sHookRet.iFlags & MEMOBJ_NULL) == 0;` |
|     3 |  889 | `										MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     2 |  890 | `									}else{` |
|     - |  891 | `										/* empty(): hand the value to the truthiness test */` |
|     3 |  892 | `										PH7_MemObjStore(&sHookRet,pTos);` |
|     - |  893 | `									}` |
|     5 |  894 | `									pTos->nIdx = SXU32_HIGH;` |
|     5 |  895 | `									PH7_MemObjRelease(&sHookRet);` |
|     5 |  896 | `									PH7_ClassInstanceUnref(pThis);` |
|     5 |  897 | `									VM_EXIT_BREAK;` |
|     - |  898 | `								}` |
|   ! 0 |  899 | `								PH7_MemObjRelease(&sHookRet);` |
|   ! 0 |  900 | `							}` |
|   105 |  901 | `							if( !bCoalesceW && !bRmwNext && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|     - |  902 | `								/* Read: dispatch __phl_hook_get_NAME (inheritance-aware) */` |
|     - |  903 | `								ph7_value sHookRet;` |
|    71 |  904 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|    71 |  905 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|    57 |  906 | `									PH7_MemObjStore(&sHookRet,pTos);` |
|    57 |  907 | `									pTos->nIdx = SXU32_HIGH;` |
|    57 |  908 | `									PH7_MemObjRelease(&sHookRet);` |
|    57 |  909 | `									PH7_ClassInstanceUnref(pThis);` |
|    57 |  910 | `									VM_EXIT_BREAK;` |
|     - |  911 | `								}` |
|    15 |  912 | `								PH7_MemObjRelease(&sHookRet);` |
|     7 |  913 | `							}` |
|    49 |  914 | `							if( bCoalesceW \|\| bRmwNext ){` |
|     - |  915 | ``								/* `$o->x ??= v` (bCoalesceW) and the read-modify-write`` |
|     - |  916 | ``								 * forms `$o->x++`, `$o->x .= v`, … (bRmwNext): php reads`` |
|     - |  917 | `								 * through the get hook (raw backing when the property is` |
|     - |  918 | `								 * set-only) and writes through the set hook.` |
|     - |  919 | `								 *   - ??=: the test value goes on the stack and a` |
|     - |  920 | `								 *     COAL_HOOK entry is pushed for the OP_NULLC_STORE` |
|     - |  921 | `								 *     at the jump target; the fetch-point sweep drops it` |
|     - |  922 | `								 *     when the short-circuit jump skips the assign or a` |
|     - |  923 | `								 *     throw abandons the RHS (the entry is NOT a scalar` |
|     - |  924 | `								 *     transient — nested stores/coalesces in the RHS` |
|     - |  925 | `								 *     stack their own entries above it).` |
|     - |  926 | `								 *   - RMW: the value goes into a fresh SCRATCH slot the` |
|     - |  927 | `								 *     modify op mutates in place; the entry makes the` |
|     - |  928 | `								 *     op's tail dispatch the set side with the computed` |
|     - |  929 | `								 *     value (PH7_HOOK_RMW_WRITEBACK). */` |
|     - |  930 | `								ph7_value sCur;` |
|     - |  931 | `								sxi32 rcCur;` |
|    35 |  932 | `								PH7_MemObjInit(pVm,&sCur);` |
|    35 |  933 | `								rcCur = PH7_VmHookGetAttrValue(pThis,pObjAttr,&sCur);` |
|    35 |  934 | `								if( rcCur == SXERR_NOTFOUND ){` |
|     - |  935 | `									/* set-only hook (or guard edge): the read side is the` |
|     - |  936 | `									 * raw backing store (php) */` |
|     3 |  937 | `									ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|     3 |  938 | `									if( pBack ){` |
|     3 |  939 | `										PH7_MemObjStore(pBack,&sCur);` |
|     2 |  940 | `									}` |
|    34 |  941 | `								}else if( pVm->nBoundaryRc != 0 ){` |
|     - |  942 | `									/* the get hook threw: leave the null temp; the` |
|     - |  943 | `									 * fetch-point router lands the parked throw —` |
|     - |  944 | `									 * nothing is armed. */` |
|   ! 0 |  945 | `									PH7_MemObjRelease(&sCur);` |
|   ! 0 |  946 | `									PH7_ClassInstanceUnref(pThis);` |
|   ! 0 |  947 | `									VM_EXIT_BREAK;` |
|     - |  948 | `								}` |
|    35 |  949 | `								if( bCoalesceW ){` |
|     - |  950 | `									VmHookRmw sPend;` |
|    15 |  951 | `									PH7_MemObjStore(&sCur,pTos);` |
|    15 |  952 | `									pTos->nIdx = SXU32_HIGH;` |
|    15 |  953 | `									PH7_MemObjRelease(&sCur);` |
|    15 |  954 | `									sPend.iKind = VM_HOOK_PEND_COAL_HOOK;` |
|    15 |  955 | `									sPend.pThis = pThis;` |
|    15 |  956 | `									sPend.pAttr = pObjAttr->pAttr;` |
|    15 |  957 | `									sPend.nBackIdx = pObjAttr->nIdx;` |
|    15 |  958 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|    15 |  959 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|    15 |  960 | `									sPend.pOwnerStack = (void *)pStack;` |
|    15 |  961 | `									sPend.pInstrs = (void *)aInstr;` |
|    15 |  962 | `									sPend.nJmpPc = (sxu32)(pc + 1);            /* the OP_NULLC_JMP */` |
|    15 |  963 | `									sPend.nPc = (sxu32)(pNextH->iP2 - 1);      /* the OP_NULLC_STORE */` |
|    15 |  964 | `									pThis->iRef++;` |
|    15 |  965 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|    15 |  966 | `									PH7_ClassInstanceUnref(pThis);` |
|    15 |  967 | `									VM_EXIT_BREAK;` |
|     - |  968 | `								}` |
|     - |  969 | `								{` |
|    21 |  970 | `									ph7_value *pScr = PH7_ReserveMemObj(&(*pVm));` |
|    21 |  971 | `									if( pScr ){` |
|     - |  972 | `										VmHookRmw sRmw;` |
|    21 |  973 | `										PH7_MemObjStore(&sCur,pScr);` |
|    21 |  974 | `										PH7_MemObjStore(&sCur,pTos);` |
|    21 |  975 | `										pTos->nIdx = pScr->nIdx;` |
|    21 |  976 | `										sRmw.iKind = VM_HOOK_PEND_RMW;` |
|    21 |  977 | `										sRmw.pThis = pThis;` |
|    21 |  978 | `										sRmw.pAttr = pObjAttr->pAttr;` |
|    21 |  979 | `										sRmw.nBackIdx = pObjAttr->nIdx;` |
|    21 |  980 | `										sRmw.nScratchIdx = pScr->nIdx;` |
|    21 |  981 | `										SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|    21 |  982 | `										sRmw.pOwnerStack = (void *)pStack;` |
|    21 |  983 | `										sRmw.pInstrs = (void *)aInstr;` |
|    21 |  984 | `										sRmw.nJmpPc = (sxu32)(pc + 1);  /* the modify op ... */` |
|    21 |  985 | `										sRmw.nPc = (sxu32)(pc + 1);     /* ... is the whole window */` |
|    21 |  986 | `										pThis->iRef++;` |
|    21 |  987 | `										SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|    10 |  988 | `									}` |
|     - |  989 | `									/* OOM: pScr == 0 — leave the null temp (loud allocator` |
|     - |  990 | `									 * diagnostics already fired) */` |
|    21 |  991 | `									PH7_MemObjRelease(&sCur);` |
|    21 |  992 | `									PH7_ClassInstanceUnref(pThis);` |
|    21 |  993 | `									VM_EXIT_BREAK;` |
|     - |  994 | `								}` |
|     - |  995 | `							}` |
|     7 |  996 | `						}` |
|     - |  997 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|     - |  998 | `						 * We can only raise it on a real read, not when the slot is the` |
|     - |  999 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|     - | 1000 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|     - | 1001 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
| 30854 | 1002 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
| 15546 | 1003 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|   233 | 1004 | `							VmInstr *pNext = pInstr + 1;` |
|   233 | 1005 | `							int bIsLhs = 0;` |
|   233 | 1006 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|   223 | 1007 | `								bIsLhs = 1;` |
|   109 | 1008 | `							}` |
|     - | 1009 | ``							/* isset()/empty()/`??` read an uninitialized typed property`` |
|     - | 1010 | `							 * as "not set" — a silent miss, NOT the Error a plain read` |
|     - | 1011 | `							 * raises (php). The compiler tags such an access iP2 =` |
|     - | 1012 | `							 * ISSET/EMPTY; treat it like the assignment-LHS case and fall` |
|     - | 1013 | `							 * through to load the slot's NULL. */` |
|   233 | 1014 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|     7 | 1015 | `								bIsLhs = 1;` |
|     3 | 1016 | `							}` |
|   233 | 1017 | `							if( !bIsLhs ){` |
|     6 | 1018 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|     6 | 1019 | `								PH7_ClassInstanceUnref(pThis);` |
|     6 | 1020 | `								if( rcU == PH7_ABORT ){` |
|     3 | 1021 | `									VM_EXIT_ABORT;` |
|     - | 1022 | `								}` |
|     - | 1023 | `								{` |
|     - | 1024 | `									sxi32 iRp;` |
|     3 | 1025 | `									if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|     3 | 1026 | `										pc = iRp;` |
|     3 | 1027 | `										VM_EXIT_BREAK;` |
|     - | 1028 | `									}` |
|     - | 1029 | `								}` |
|   ! 0 | 1030 | `								VM_EXIT_EXCEPTION;` |
|     - | 1031 | `							}` |
|   112 | 1032 | `						}` |
|     - | 1033 | `						/* Load attribute */` |
| 30855 | 1034 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
| 30855 | 1035 | `						if( pValue ){` |
| 30853 | 1036 | `							if( pThis->iRef < 2 ){` |
|     - | 1037 | `								/* Perform a store operation,rather than a load operation since` |
|     - | 1038 | `								 * the class instance '$this' will be deleted shortly.` |
|     - | 1039 | `								 */` |
|   103 | 1040 | `								PH7_MemObjStore(pValue,pTos);` |
|    52 | 1041 | `							}else{` |
|     - | 1042 | `								/* Simple load */` |
| 30751 | 1043 | `								PH7_MemObjLoad(pValue,pTos);` |
|     - | 1044 | `							}` |
| 30853 | 1045 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
| 30853 | 1046 | `								if( pThis->iRef > 1 ){` |
|     - | 1047 | `									/* Load attribute index */` |
| 30751 | 1048 | `									pTos->nIdx = pObjAttr->nIdx;` |
| 15373 | 1049 | `								}` |
| 15424 | 1050 | `							}` |
| 15424 | 1051 | `						}` |
| 15430 | 1052 | `					}else{` |
|     - | 1053 | `						/* Inaccessible (private/protected) from this scope. php consults` |
|     - | 1054 | `						 * __get here exactly like a missing property (band A #3a) before` |
|     - | 1055 | `						 * erroring; the guard keeps a self-recursive read from looping. */` |
|    20 | 1056 | `						ph7_class_method *pGetMagic = 0;` |
|    18 | 1057 | `						if( pInstr->iP2 != PH7_MEMBER_WRITE && !VmMemberCtxIsLookup(pInstr->iP2)` |
|    14 | 1058 | `						 && !VmMemberNextIsWrite(pInstr + 1) ){` |
|    10 | 1059 | `							pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|     4 | 1060 | `						}` |
|    20 | 1061 | `						if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|     - | 1062 | `							ph7_value sMagicRet;` |
|     3 | 1063 | `							PH7_MemObjInit(pVm,&sMagicRet);` |
|     3 | 1064 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     3 | 1065 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|     3 | 1066 | `							VmMagicGuardPop(pVm);` |
|     - | 1067 | `							/* The name was already popped and pTos released above; just` |
|     - | 1068 | `							 * take the magic result as the expression value. */` |
|     3 | 1069 | `							PH7_MemObjStore(&sMagicRet,pTos);` |
|     3 | 1070 | `							pTos->nIdx = SXU32_HIGH;` |
|     3 | 1071 | `							PH7_MemObjRelease(&sMagicRet);` |
|     3 | 1072 | `							PH7_ClassInstanceUnref(pThis);` |
|     3 | 1073 | `							VM_EXIT_BREAK;` |
|     - | 1074 | `						}` |
|    18 | 1075 | `						if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|     - | 1076 | `							/* isset/empty on an inaccessible property: php consults __isset` |
|     - | 1077 | `							 * (band A #3b), and is silently false without it — never an` |
|     - | 1078 | `							 * Error (pre-fix PHL fataled here). */` |
|     9 | 1079 | `							ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|     9 | 1080 | `							int bSet = 0;` |
|     9 | 1081 | `							if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|     - | 1082 | `								ph7_value sIssetRet;` |
|   ! 0 | 1083 | `								PH7_MemObjInit(pVm,&sIssetRet);` |
|   ! 0 | 1084 | `								VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|   ! 0 | 1085 | `								PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|   ! 0 | 1086 | `								VmMagicGuardPop(pVm);` |
|   ! 0 | 1087 | `								PH7_MemObjToBool(&sIssetRet);` |
|   ! 0 | 1088 | `								bSet = sIssetRet.x.iVal != 0;` |
|   ! 0 | 1089 | `								PH7_MemObjRelease(&sIssetRet);` |
|   ! 0 | 1090 | `							}` |
|     9 | 1091 | `							if( bSet ){` |
|   ! 0 | 1092 | `								if( pInstr->iP2 == PH7_MEMBER_EMPTY ){` |
|   ! 0 | 1093 | `									ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|     - | 1094 | `									ph7_value sEmptyVal;` |
|   ! 0 | 1095 | `									PH7_MemObjInit(pVm,&sEmptyVal);` |
|   ! 0 | 1096 | `									if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|   ! 0 | 1097 | `										VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|   ! 0 | 1098 | `										PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|   ! 0 | 1099 | `										VmMagicGuardPop(pVm);` |
|   ! 0 | 1100 | `									}` |
|   ! 0 | 1101 | `									PH7_MemObjStore(&sEmptyVal,pTos);` |
|   ! 0 | 1102 | `									pTos->nIdx = SXU32_HIGH;` |
|   ! 0 | 1103 | `									PH7_MemObjRelease(&sEmptyVal);` |
|   ! 0 | 1104 | `								}else{` |
|   ! 0 | 1105 | `									pTos->x.iVal = 1;` |
|   ! 0 | 1106 | `									MemObjSetType(pTos,MEMOBJ_BOOL);` |
|   ! 0 | 1107 | `									pTos->nIdx = SXU32_HIGH;` |
|     - | 1108 | `								}` |
|   ! 0 | 1109 | `							}` |
|     9 | 1110 | `							PH7_ClassInstanceUnref(pThis);` |
|     9 | 1111 | `							VM_EXIT_BREAK;` |
|     - | 1112 | `						}` |
|     - | 1113 | `						{` |
|     - | 1114 | `							/* A plain store to an inaccessible property dispatches __set` |
|     - | 1115 | `							 * (band A #3b): park the receiver+name for the following` |
|     - | 1116 | `							 * OP_STORE, exactly like the missing-property case. */` |
|    10 | 1117 | `							VmInstr *pNextW = pInstr + 1;` |
|    10 | 1118 | `							if( pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0 ){` |
|     3 | 1119 | `								ph7_class_method *pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|     3 | 1120 | `								if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){` |
|     3 | 1121 | `									pThis->iRef++;` |
|     3 | 1122 | `									pVm->pMagicSetThis = pThis;` |
|     3 | 1123 | `									SyBlobReset(&pVm->sMagicSetName);` |
|     3 | 1124 | `									SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|     3 | 1125 | `									pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|     3 | 1126 | `									PH7_ClassInstanceUnref(pThis);` |
|     3 | 1127 | `									VM_EXIT_BREAK;` |
|     - | 1128 | `								}` |
|   ! 0 | 1129 | `							}` |
|     - | 1130 | `						}` |
|     6 | 1131 | `						if( ((pInstr + 1)->iOp == PH7_OP_NULLC \|\| (pInstr + 1)->iOp == PH7_OP_NULLC_JMP)` |
|     4 | 1132 | `						 && pInstr->iP2 != PH7_MEMBER_WRITE ){` |
|     - | 1133 | `` 							/* `$o->priv ?? default` (OP_NULLC; NULLC_JMP is the `??=` `` |
|     - | 1134 | ``							 * pre-test): php treats `??` as an isset-style lookup — an`` |
|     - | 1135 | `							 * inaccessible property yields null SILENTLY (the` |
|     - | 1136 | `							 * __isset/__get consults already ran above), letting the` |
|     - | 1137 | `							 * coalesce pick the default. */` |
|   ! 0 | 1138 | `							PH7_ClassInstanceUnref(pThis);` |
|   ! 0 | 1139 | `							VM_EXIT_BREAK; /* pTos is already the null temp */` |
|     - | 1140 | `						}` |
|     - | 1141 | `						/* A subclass reading a PARENT's PRIVATE property: php treats it as` |
|     - | 1142 | `						 * an UNDEFINED property (the base-private is invisible to the` |
|     - | 1143 | `						 * subclass scope) — a Warning + null, NOT an access Error. Only` |
|     - | 1144 | `						 * this exact shape warns; every other denied read is the catchable` |
|     - | 1145 | `						 * "Cannot access" Error below. */` |
|     - | 1146 | `						{` |
|     7 | 1147 | `						ph7_class *pSelf = VmCurrentSelf(&(*pVm));` |
|     7 | 1148 | `						ph7_class *pDecl = pObjAttr->pAttr->pDeclClass;` |
|     6 | 1149 | `						if( pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|     6 | 1150 | `						 && pSelf && pDecl && pSelf != pDecl && PH7_VmInstanceOf(pSelf,pDecl) ){` |
|     3 | 1151 | `							if( !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|     4 | 1152 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|     1 | 1153 | `									&pClass->sName,&sName);` |
|     1 | 1154 | `							}` |
|     3 | 1155 | `							PH7_ClassInstanceUnref(pThis);` |
|     3 | 1156 | `							VM_EXIT_BREAK; /* pTos is already the null temp */` |
|     - | 1157 | `						}` |
|     - | 1158 | `						}` |
|     - | 1159 | `						/* Genuinely inaccessible: php's CATCHABLE Error (parked on the` |
|     - | 1160 | `						 * boundary rail; the fetch-point router lands it — it was an` |
|     - | 1161 | `						 * uncatchable VmReportUncaughtException+Abort before). */` |
|     - | 1162 | `						{` |
|     - | 1163 | `						SyBlob sErrMsg;` |
|     5 | 1164 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|     5 | 1165 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     5 | 1166 | `						SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",` |
|     2 | 1167 | `							zVis,&pClass->sName,&sName);` |
|     5 | 1168 | `						PH7_ClassInstanceUnref(pThis);` |
|     5 | 1169 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     5 | 1170 | `						SyBlobRelease(&sErrMsg);` |
|     5 | 1171 | `						VM_EXIT_BREAK;` |
|     - | 1172 | `						}` |
|     - | 1173 | `					}` |
| 15425 | 1174 | `				}` |
|     - | 1175 | `				/* Safely unreference the object */` |
| 30897 | 1176 | `				PH7_ClassInstanceUnref(pThis);` |
|     - | 1177 | `			}` |
| 21066 | 1178 | `		}else{` |
|     - | 1179 | ``			/* `->` on a non-object (e.g. a null intermediate). Silent in isset()/empty()/unset()`` |
|     - | 1180 | ``			 * context (iP2 2/3/4) so `isset($o->missing->x)` / `unset($o->missing->x)` match PHP. */`` |
|    12 | 1181 | `			if( pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|     - | 1182 | `				/* php draws a sharp line here, and PH7 drew none: CALLING a method on a` |
|     - | 1183 | `				 * non-object is a catchable Error, while READING a property off one is a` |
|     - | 1184 | `				 * Warning that yields NULL. PH7 raised the same notice for both and` |
|     - | 1185 | ``				 * carried on with NULL, so `$null->m()` silently did nothing. */`` |
|     - | 1186 | `				SyString sMemb;` |
|     8 | 1187 | `				SyStringInitFromBuf(&sMemb,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     8 | 1188 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|     - | 1189 | `					SyBlob sErrM;` |
|     - | 1190 | `					sxi32 rcErr;` |
|     5 | 1191 | `					SyBlobInit(&sErrM,&pVm->sAllocator);` |
|     5 | 1192 | `					SyBlobFormat(&sErrM,"Call to a member function %z() on %s",` |
|     2 | 1193 | `						&sMemb,VmArithTypeName(pNos));` |
|     5 | 1194 | `					VmPopOperand(&pTos,1);` |
|     5 | 1195 | `					PH7_MemObjRelease(pTos);` |
|     5 | 1196 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|     5 | 1197 | `					pTos->nIdx = SXU32_HIGH;` |
|     7 | 1198 | `					rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|     2 | 1199 | `						SyBlobLength(&sErrM));` |
|     5 | 1200 | `					SyBlobRelease(&sErrM);` |
|     5 | 1201 | `					if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     5 | 1202 | `					rc = rcErr;` |
|     5 | 1203 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     - | 1204 | `				}` |
|     4 | 1205 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Attempt to read property \"%z\" on %s",` |
|     1 | 1206 | `					&sMemb,VmArithTypeName(pNos));` |
|     1 | 1207 | `			}` |
|     8 | 1208 | `			VmPopOperand(&pTos,1);` |
|     8 | 1209 | `			PH7_MemObjRelease(pTos);` |
|     8 | 1210 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|     - | 1211 | `		}` |
| 21069 | 1212 | `	}else{` |
|     - | 1213 | `		/* Static member access using class name */` |
|  2523 | 1214 | `		pNos = pTos;` |
|  2523 | 1215 | `		pThis = 0;` |
|  2523 | 1216 | `		if( !pInstr->p3 ){` |
|  1965 | 1217 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|  1965 | 1218 | `			pNos--;` |
|     - | 1219 | `#ifdef UNTRUST` |
|     - | 1220 | `			if( pNos < pStack ){` |
|     - | 1221 | `				VM_EXIT_ABORT;` |
|     - | 1222 | `			}` |
|     - | 1223 | `#endif` |
|   985 | 1224 | `		}else{` |
|     - | 1225 | `			/* Attribute name already computed */` |
|   563 | 1226 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|     - | 1227 | `		}` |
|  2523 | 1228 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|  2523 | 1229 | `			ph7_class *pClass = 0;` |
|     - | 1230 | `			/* A FORWARDING static call — self::/parent::/static:: — preserves the` |
|     - | 1231 | `			 * caller's late-static-binding class (php); a non-forwarding C::m() resets` |
|     - | 1232 | `			 * it to C. The receiver slot below keeps the literal keyword, which OP_CALL` |
|     - | 1233 | `			 * can't resolve, so it would fall back to the callee's DECLARING class and` |
|     - | 1234 | `			 * lose LSB. Remember the live LSB class here and stamp it onto the receiver` |
|     - | 1235 | `			 * after the method name is pushed. */` |
|  2523 | 1236 | `			int bForwardingCall = 0;` |
|  2523 | 1237 | `			ph7_class *pForwardLsb = 0;` |
|  2523 | 1238 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|     - | 1239 | `				/* Class already instantiated */` |
|    11 | 1240 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|    11 | 1241 | `				pClass = pThis->pClass;` |
|    11 | 1242 | `				pThis->iRef++; /* Deffer garbage collection */` |
|     6 | 1243 | `			}else{` |
|     - | 1244 | `				/* Try to extract the target class */` |
|  2513 | 1245 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|  2513 | 1246 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|  2513 | 1247 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|     - | 1248 | `					/* Handle self/static/parent keywords */` |
|  2513 | 1249 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|   715 | 1250 | `						pClass = PH7_VmPeekDeclaringClass(&(*pVm));` |
|   715 | 1251 | `						if( pClass && (pClass->iFlags & PH7_CLASS_TRAIT) ){` |
|     - | 1252 | `							/* In a trait method, self:: resolves to the using class */` |
|    14 | 1253 | `							pClass = PH7_VmPeekTopClass(&(*pVm));` |
|     6 | 1254 | `						}` |
|   715 | 1255 | `						bForwardingCall = 1;` |
|   715 | 1256 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|  2158 | 1257 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|    59 | 1258 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|    59 | 1259 | `						bForwardingCall = 1;` |
|    59 | 1260 | `						pForwardLsb = pClass;` |
|  1775 | 1261 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|   181 | 1262 | `						pClass = PH7_VmResolveParentClass(&(*pVm));` |
|   181 | 1263 | `						bForwardingCall = 1;` |
|   181 | 1264 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|    92 | 1265 | `					}else{` |
|  1569 | 1266 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|     - | 1267 | `					}` |
|  1254 | 1268 | `				}` |
|     - | 1269 | `			}` |
|  2523 | 1270 | `			if( pClass == 0 ){` |
|     - | 1271 | `				/* Undefined class: php throws a catchable Error */` |
|     - | 1272 | `				SyBlob sErrM;` |
|     - | 1273 | `				sxi32 rcErr;` |
|   ! 0 | 1274 | `				SyBlobInit(&sErrM,&pVm->sAllocator);` |
|   ! 0 | 1275 | `				SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|   ! 0 | 1276 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob));` |
|   ! 0 | 1277 | `				if( !pInstr->p3 ){` |
|   ! 0 | 1278 | `					VmPopOperand(&pTos,1);` |
|   ! 0 | 1279 | `				}` |
|   ! 0 | 1280 | `				PH7_MemObjRelease(pTos);` |
|   ! 0 | 1281 | `				pTos->nIdx = SXU32_HIGH;` |
|   ! 0 | 1282 | `				rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|   ! 0 | 1283 | `					SyBlobLength(&sErrM));` |
|   ! 0 | 1284 | `				SyBlobRelease(&sErrM);` |
|   ! 0 | 1285 | `				if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|   ! 0 | 1286 | `				rc = rcErr;` |
|   ! 0 | 1287 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|   ! 0 | 1288 | `			}else{` |
|  2523 | 1289 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|     - | 1290 | `					/* Method call */` |
|   993 | 1291 | `					ph7_class_method *pMeth = 0;` |
|   993 | 1292 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|     - | 1293 | `						/* Extract the target method */` |
|   993 | 1294 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|   494 | 1295 | `					}` |
|   993 | 1296 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|     7 | 1297 | `						if( pMeth ){` |
|     - | 1298 | `							SyBlob sErrM;` |
|     - | 1299 | `							sxi32 rcErr;` |
|     3 | 1300 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|     3 | 1301 | `							SyBlobFormat(&sErrM,"Cannot call abstract method %z::%z()",` |
|     1 | 1302 | `								&pClass->sName,&sName);` |
|     3 | 1303 | `							if( !pInstr->p3 ){` |
|     3 | 1304 | `								VmPopOperand(&pTos,1);` |
|     1 | 1305 | `							}` |
|     3 | 1306 | `							PH7_MemObjRelease(pTos);` |
|     3 | 1307 | `							pTos->nIdx = SXU32_HIGH;` |
|     4 | 1308 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|     1 | 1309 | `								SyBlobLength(&sErrM));` |
|     3 | 1310 | `							SyBlobRelease(&sErrM);` |
|     3 | 1311 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     3 | 1312 | `							rc = rcErr;` |
|     3 | 1313 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|   ! 0 | 1314 | `						}else{` |
|     5 | 1315 | `							ph7_class_method *pCallStaticMagic = PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1);` |
|     5 | 1316 | `							if( pCallStaticMagic ){` |
|     - | 1317 | `								/* php: C::missing(...) dispatches __callStatic($name,$args)` |
|     - | 1318 | `								 * via the packing trampoline (see the instance twin). */` |
|     3 | 1319 | `								SyBlobReset(&pVm->sMagicCallName);` |
|     3 | 1320 | `								SyBlobAppend(&pVm->sMagicCallName,(const void *)sName.zString,sName.nByte);` |
|     3 | 1321 | `								pVm->pMagicCallThis = 0;` |
|     3 | 1322 | `								pVm->pMagicCallClass = pClass;` |
|     3 | 1323 | `								if( !pInstr->p3 ){` |
|     3 | 1324 | `									VmPopOperand(&pTos,1);` |
|     1 | 1325 | `								}` |
|     3 | 1326 | `								PH7_MemObjRelease(pTos);` |
|     3 | 1327 | `								SyBlobAppend(&pTos->sBlob,"__phl_magic_call",sizeof("__phl_magic_call")-1);` |
|     3 | 1328 | `								MemObjSetType(pTos,MEMOBJ_STRING);` |
|     3 | 1329 | `								pTos->nIdx = SXU32_HIGH;` |
|     3 | 1330 | `								VM_EXIT_BREAK;` |
|     - | 1331 | `							}` |
|     - | 1332 | `							{` |
|     - | 1333 | `								/* php: the STATIC form reports the same "Call to undefined` |
|     - | 1334 | `								 * method C::m()" as the instance one. */` |
|     - | 1335 | `								SyBlob sErrM;` |
|     - | 1336 | `								sxi32 rcErr;` |
|     3 | 1337 | `								SyBlobInit(&sErrM,&pVm->sAllocator);` |
|     3 | 1338 | `								SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",` |
|     1 | 1339 | `									&pClass->sName,&sName);` |
|     3 | 1340 | `								if( !pInstr->p3 ){` |
|     3 | 1341 | `									VmPopOperand(&pTos,1);` |
|     1 | 1342 | `								}` |
|     3 | 1343 | `								PH7_MemObjRelease(pTos);` |
|     3 | 1344 | `								pTos->nIdx = SXU32_HIGH;` |
|     4 | 1345 | `								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|     1 | 1346 | `									SyBlobLength(&sErrM));` |
|     3 | 1347 | `								SyBlobRelease(&sErrM);` |
|     3 | 1348 | `								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|     3 | 1349 | `								rc = rcErr;` |
|     3 | 1350 | `								PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     - | 1351 | `							}` |
|     - | 1352 | `						}` |
|     - | 1353 | `						/* Pop the method name from the stack */` |
|   ! 0 | 1354 | `						if( !pInstr->p3 ){` |
|   ! 0 | 1355 | `							VmPopOperand(&pTos,1);` |
|     - | 1356 | `						}` |
|   ! 0 | 1357 | `						PH7_MemObjRelease(pTos);` |
|   ! 0 | 1358 | `					}else{` |
|     - | 1359 | `						/* Push method name on the stack */` |
|   987 | 1360 | `						PH7_MemObjRelease(pTos);` |
|   987 | 1361 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|   987 | 1362 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|     - | 1363 | `					}` |
|   987 | 1364 | `					pTos->nIdx = SXU32_HIGH;` |
|     - | 1365 | `					/* Forwarding call (self::/parent::/static::): overwrite the receiver` |
|     - | 1366 | `					 * slot (pNos, one below the method name) with the live LSB class name` |
|     - | 1367 | ``					 * so OP_CALL pushes THAT onto aSelf — preserving `static::` inside the`` |
|     - | 1368 | `					 * callee. Without this the literal keyword falls through to the` |
|     - | 1369 | `					 * callee's declaring class. Only when an LSB class is actually in` |
|     - | 1370 | `					 * scope (a static call from global scope has none). */` |
|   987 | 1371 | `					if( bForwardingCall && pForwardLsb && (pNos->iFlags & MEMOBJ_STRING) ){` |
|   298 | 1372 | `						SyBlobReset(&pNos->sBlob);` |
|   298 | 1373 | `						SyBlobAppend(&pNos->sBlob,pForwardLsb->sName.zString,pForwardLsb->sName.nByte);` |
|   148 | 1374 | `					}` |
|   496 | 1375 | `				}else{` |
|     - | 1376 | `					/* Attribute access */` |
|  1535 | 1377 | `					ph7_class_attr *pAttr = 0;` |
|  1535 | 1378 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|     - | 1379 | `						/* unset(C::$x): PHP rejects unsetting a static property with a fatal Error.` |
|     - | 1380 | `						 * Without this the iP2=unset tag falls through to a normal static read and the` |
|     - | 1381 | `						 * trailing generic unset() would silently NULL (and de-type) the shared slot. */` |
|     - | 1382 | `						char zMsg[256];` |
|   ! 0 | 1383 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Attempt to unset static property %.*s::$%.*s",` |
|   ! 0 | 1384 | `							(int)pClass->sName.nByte,pClass->sName.zString,(int)sName.nByte,sName.zString);` |
|   ! 0 | 1385 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|   ! 0 | 1386 | `						VM_EXIT_ABORT;` |
|     - | 1387 | `					}` |
|     - | 1388 | `					/* Check for special ::class pseudo-constant */` |
|  1643 | 1389 | `					if( sName.nByte == sizeof("class")-1 &&` |
|   216 | 1390 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|     - | 1391 | `						/* ::class returns the fully qualified class name */` |
|     - | 1392 | `						/* Pop the attribute name from the stack */` |
|    88 | 1393 | `						if( !pInstr->p3 ){` |
|    88 | 1394 | `							VmPopOperand(&pTos,1);` |
|    42 | 1395 | `						}` |
|    88 | 1396 | `						PH7_MemObjRelease(pTos);` |
|     - | 1397 | `						/* Load the class name */` |
|    88 | 1398 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|    88 | 1399 | `						pTos->nIdx = SXU32_HIGH;` |
|    46 | 1400 | `					}else{` |
|     - | 1401 | `						/* Extract the target attribute */` |
|  1451 | 1402 | `						if( sName.nByte > 0 ){` |
|  1451 | 1403 | `							pAttr = PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|   723 | 1404 | `						}` |
|  1451 | 1405 | `						if( pAttr == 0 ){` |
|     - | 1406 | `							/* No such STATIC attribute. php raises a catchable Error` |
|     - | 1407 | `							 * ("Access to undeclared static property") — instance magic` |
|     - | 1408 | `							 * (__get) is never consulted for statics (band A #3b; the old` |
|     - | 1409 | `							 * path warned + called __get with a null $this and discarded` |
|     - | 1410 | `							 * it). isset()/empty() context stays silently false. Parked on` |
|     - | 1411 | `							 * the boundary rail; the op completes benignly with NULL and` |
|     - | 1412 | `							 * the fetch-point router lands the throw. */` |
|     3 | 1413 | `							if( !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|     - | 1414 | `								SyBlob sErrMsg;` |
|     3 | 1415 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     3 | 1416 | `								SyBlobFormat(&sErrMsg,"Access to undeclared static property %z::$%z",` |
|     1 | 1417 | `									&pClass->sName,&sName);` |
|     3 | 1418 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     1 | 1419 | `							}` |
|     1 | 1420 | `						}` |
|     - | 1421 | `						/* Pop the attribute name from the stack */` |
|  1451 | 1422 | `						if( !pInstr->p3 ){` |
|   892 | 1423 | `							VmPopOperand(&pTos,1);` |
|   444 | 1424 | `						}` |
|  1451 | 1425 | `						PH7_MemObjRelease(pTos);` |
|  1451 | 1426 | `						pTos->nIdx = SXU32_HIGH;` |
|  1451 | 1427 | `						if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|     - | 1428 | ``							/* `self::$s =& $x` / `C::$s =& $x`: stash the static property slot`` |
|     - | 1429 | `							 * for the following member-marked OP_STORE_REF (class-level, shared` |
|     - | 1430 | `							 * across instances — matches php). Skip the read machinery below. */` |
|     2 | 1431 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|     3 | 1432 | `							 && PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|     3 | 1433 | `								pVm->pRefTargetStaticAttr = pAttr;` |
|     3 | 1434 | `								pVm->pRefTargetAttr = 0;` |
|     3 | 1435 | `								pVm->pRefTargetThis = 0;` |
|     3 | 1436 | `								pTos->nIdx = pAttr->nIdx;` |
|     2 | 1437 | `							}else{` |
|   ! 0 | 1438 | `								pVm->pRefTargetStaticAttr = 0;` |
|   ! 0 | 1439 | `								pVm->pRefTargetAttr = 0;` |
|   ! 0 | 1440 | `								pVm->pRefTargetThis = 0;` |
|     - | 1441 | `							}` |
|     3 | 1442 | `							if( pThis ){` |
|   ! 0 | 1443 | `								PH7_ClassInstanceUnref(pThis);` |
|   ! 0 | 1444 | `							}` |
|     3 | 1445 | `							VM_EXIT_BREAK;` |
|     - | 1446 | `						}` |
|  1449 | 1447 | `						if( pAttr ){` |
|  1447 | 1448 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|     - | 1449 | `								/* Access to a non static attribute */` |
|   ! 0 | 1450 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|   ! 0 | 1451 | `									&pClass->sName,&pAttr->sName` |
|     - | 1452 | `									);` |
|   ! 0 | 1453 | `							}else{` |
|     - | 1454 | `								ph7_value *pValue;` |
|     - | 1455 | `								/* Check if the access to the attribute is allowed */` |
|  1447 | 1456 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|     - | 1457 | `									/* PHP 7.4+: uninitialized typed static read.` |
|     - | 1458 | `									 * Same LHS-of-store peek as the instance path. */` |
|  1438 | 1459 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|   766 | 1460 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|    78 | 1461 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|    50 | 1462 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|    53 | 1463 | `										if( pS ){` |
|    53 | 1464 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|    53 | 1465 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|     8 | 1466 | `												VmInstr *pNext = pInstr + 1;` |
|     8 | 1467 | `												int bIsLhs = 0;` |
|     8 | 1468 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|     6 | 1469 | `													bIsLhs = 1;` |
|     2 | 1470 | `												}` |
|     8 | 1471 | `												if( !bIsLhs ){` |
|     3 | 1472 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|     3 | 1473 | `													if( pThis ){` |
|   ! 0 | 1474 | `														PH7_ClassInstanceUnref(pThis);` |
|   ! 0 | 1475 | `													}` |
|     3 | 1476 | `													if( rcU == PH7_ABORT ){` |
|   ! 0 | 1477 | `														VM_EXIT_ABORT;` |
|     - | 1478 | `													}` |
|     - | 1479 | `													{` |
|     - | 1480 | `														sxi32 iRp;` |
|     3 | 1481 | `														if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|     3 | 1482 | `															pc = iRp;` |
|     3 | 1483 | `															VM_EXIT_BREAK;` |
|     - | 1484 | `														}` |
|     - | 1485 | `													}` |
|   ! 0 | 1486 | `													VM_EXIT_EXCEPTION;` |
|     - | 1487 | `												}` |
|     2 | 1488 | `											}` |
|    24 | 1489 | `										}` |
|    24 | 1490 | `									}` |
|  1436 | 1491 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|  1165 | 1492 | `									 && SySetUsed(&pAttr->aAttrs) > 0 ){` |
|     - | 1493 | `										/* php 8.4 #[\Deprecated] on a class constant:` |
|     - | 1494 | `										 * every access re-warns. */` |
|    11 | 1495 | `										VmDeprecatedConstNotice(&(*pVm),pClass,pAttr);` |
|     5 | 1496 | `									}` |
|  1441 | 1497 | `									if( pAttr->nIdx == SXU32_HIGH ){` |
|     - | 1498 | `										/* Unmaterialized slot. Enum case: first access` |
|     - | 1499 | `										 * materializes ALL the singletons. Plain constant: its` |
|     - | 1500 | `										 * initializer hasn't run yet (mount-order-dependent` |
|     - | 1501 | `										 * cross-constant reference) — evaluate on demand. A` |
|     - | 1502 | `										 * raised TypeError/Error (backing mismatch, duplicate` |
|     - | 1503 | `										 * value, self-reference) parks on the boundary rail;` |
|     - | 1504 | `										 * the op completes benignly with NULL and the` |
|     - | 1505 | `										 * fetch-point router lands the throw. */` |
|     - | 1506 | `										sxi32 rcEnum;` |
|   228 | 1507 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|     - | 1508 | `											/* php: a DIRECT static access evaluates every case` |
|     - | 1509 | `											 * of the enum (whole-class constant update) — a` |
|     - | 1510 | `											 * broken sibling case throws here too. A reference` |
|     - | 1511 | `											 * from inside another constant's initializer` |
|     - | 1512 | `											 * (nConstEvalDepth > 0) evaluates only the` |
|     - | 1513 | `											 * requested case. */` |
|    21 | 1514 | `											if( pVm->nConstEvalDepth > 0 ){` |
|     3 | 1515 | `												rcEnum = VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|     2 | 1516 | `											}else{` |
|    19 | 1517 | `												rcEnum = VmEnumMaterialize(&(*pVm),pClass);` |
|     - | 1518 | `											}` |
|    11 | 1519 | `										}else{` |
|   208 | 1520 | `											rcEnum = VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|     - | 1521 | `										}` |
|   228 | 1522 | `										if( rcEnum != SXRET_OK ){` |
|     7 | 1523 | `											VmBoundaryPark(&(*pVm),rcEnum);` |
|     3 | 1524 | `										}` |
|   112 | 1525 | `									}` |
|     - | 1526 | `									/* Load the desired attribute */` |
|  1441 | 1527 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|  1441 | 1528 | `									if( pValue ){` |
|  1435 | 1529 | `										PH7_MemObjLoad(pValue,pTos);` |
|  1435 | 1530 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|     - | 1531 | `											/* Load index number */` |
|   557 | 1532 | `											pTos->nIdx = pAttr->nIdx;` |
|   276 | 1533 | `										}` |
|   715 | 1534 | `									}` |
|   723 | 1535 | `								}else{` |
|     - | 1536 | `									/* Throw Error exception (PHP-compatible) */` |
|     - | 1537 | `									char zMsg[256];` |
|     5 | 1538 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|     5 | 1539 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|     7 | 1540 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|     4 | 1541 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|     4 | 1542 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|     3 | 1543 | `									}else{` |
|   ! 0 | 1544 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|   ! 0 | 1545 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|   ! 0 | 1546 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|     - | 1547 | `									}` |
|     5 | 1548 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|     5 | 1549 | `									VM_EXIT_ABORT;` |
|     - | 1550 | `								}` |
|     - | 1551 | `							}` |
|   718 | 1552 | `						}` |
|     - | 1553 | `					}` |
|     - | 1554 | `				}` |
|  2509 | 1555 | `				if( pThis ){` |
|     - | 1556 | `					/* Safely unreference the object */` |
|    11 | 1557 | `					PH7_ClassInstanceUnref(pThis);` |
|     5 | 1558 | `				}` |
|     - | 1559 | `			}` |
|  1257 | 1560 | `		}else{` |
|     - | 1561 | `			/* Pop operands */` |
|   ! 0 | 1562 | `			PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,"Invalid class name,PH7 is loading NULL");` |
|   ! 0 | 1563 | `			if( !pInstr->p3 ){` |
|   ! 0 | 1564 | `				VmPopOperand(&pTos,1);` |
|   ! 0 | 1565 | `			}` |
|   ! 0 | 1566 | `			PH7_MemObjRelease(pTos);` |
|   ! 0 | 1567 | `			pTos->nIdx = SXU32_HIGH;` |
|     - | 1568 | `		}` |
|     - | 1569 | `	}` |
| 44637 | 1570 | `	VM_EXIT_BREAK;` |
|   ! 0 | 1571 | `	VM_EXIT_BREAK;` |
| 22570 | 1572 | `}` |
|     - | 1573 |  |
|     - | 1574 | `/*` |
|     - | 1575 | ` * OP_CLONE: body moved verbatim from the OP_CLONE arm of` |
|     - | 1576 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|     - | 1577 | ` */` |
|   214 | 1578 | `PH7_PRIVATE VmOpRc VmExecOpClone(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|     5 | 1579 | `{` |
|   219 | 1580 | `	ph7_value *pTos = pState->pTos;` |
|   219 | 1581 | `	ph7_value *pStack = pState->pStack;` |
|   219 | 1582 | `	VmInstr *aInstr = pState->aInstr;` |
|   219 | 1583 | `	sxi32 pc = pState->pc;` |
|     - | 1584 | `	sxi32 rc;` |
|   107 | 1585 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|     - | 1586 | `	ph7_class_instance *pSrc,*pClone;` |
|     - | 1587 | `#ifdef UNTRUST` |
|     - | 1588 | `	if( pTos < pStack ){` |
|     - | 1589 | `		VM_EXIT_ABORT;` |
|     - | 1590 | `	}` |
|     - | 1591 | `#endif` |
|     - | 1592 | `	/* Make sure we are dealing with a class instance. PHP 8 throws a catchable` |
|     - | 1593 | ``	 * TypeError for a non-object operand — for both `clone $x` and clone($x). */`` |
|   219 | 1594 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     - | 1595 | `		SyBlob sMsg;` |
|     7 | 1596 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     7 | 1597 | `		SyBlobFormat(&sMsg,"clone(): Argument #1 ($object) must be of type object, %s given",` |
|     3 | 1598 | `			ph7_type_name(pTos));` |
|     7 | 1599 | `		rc = VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|     7 | 1600 | `		PH7_MemObjRelease(pTos);` |
|     7 | 1601 | `		pTos->nIdx = SXU32_HIGH;` |
|     7 | 1602 | `		if( rc == PH7_ABORT ){` |
|   ! 0 | 1603 | `			VM_EXIT_ABORT;` |
|     - | 1604 | `		}` |
|     - | 1605 | `		{` |
|     - | 1606 | `			sxi32 iRp;` |
|     7 | 1607 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|     7 | 1608 | `				pc = iRp;` |
|     7 | 1609 | `				VM_EXIT_BREAK;` |
|     - | 1610 | `			}` |
|     - | 1611 | `		}` |
|   ! 0 | 1612 | `		VM_EXIT_EXCEPTION;` |
|     - | 1613 | `	}` |
|     - | 1614 | `	/* Point to the source */` |
|   212 | 1615 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|     - | 1616 | `	/* Enum cases are not cloneable — php's catchable Error (the singleton` |
|     - | 1617 | `	 * identity would break). */` |
|   212 | 1618 | `	if( pSrc->pClass->iFlags & PH7_CLASS_ENUM ){` |
|     - | 1619 | `		SyBlob sMsg;` |
|     3 | 1620 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     3 | 1621 | `		SyBlobFormat(&sMsg,"Trying to clone an uncloneable object of class %z",` |
|     2 | 1622 | `			&pSrc->pClass->sName);` |
|     3 | 1623 | `		rc = VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|     3 | 1624 | `		PH7_MemObjRelease(pTos);` |
|     3 | 1625 | `		pTos->nIdx = SXU32_HIGH;` |
|     3 | 1626 | `		if( rc == PH7_ABORT ){` |
|   ! 0 | 1627 | `			VM_EXIT_ABORT;` |
|     - | 1628 | `		}` |
|     - | 1629 | `		{` |
|     - | 1630 | `			sxi32 iRp;` |
|     3 | 1631 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|   ! 0 | 1632 | `				pc = iRp;` |
|   ! 0 | 1633 | `				VM_EXIT_BREAK;` |
|     - | 1634 | `			}` |
|     - | 1635 | `		}` |
|     3 | 1636 | `		VM_EXIT_EXCEPTION;` |
|     - | 1637 | `	}` |
|     - | 1638 | `	/* Generator and Fiber objects are not cloneable (matches PHP) */` |
|   210 | 1639 | `	if( pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|   ! 0 | 1640 | `		VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|     - | 1641 | `			"Trying to clone an uncloneable object of class '%z'",` |
|   ! 0 | 1642 | `			&pSrc->pClass->sName);` |
|   ! 0 | 1643 | `		PH7_MemObjRelease(pTos);` |
|   ! 0 | 1644 | `		VM_EXIT_BREAK;` |
|     - | 1645 | `	}` |
|     - | 1646 | `	/* Perform the clone operation */` |
|   210 | 1647 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|   210 | 1648 | `	PH7_MemObjRelease(pTos);` |
|   210 | 1649 | `	if( pClone == 0 ){` |
|   ! 0 | 1650 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|     - | 1651 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|   ! 0 | 1652 | `	}else{` |
|     - | 1653 | `		/* Load the cloned object */` |
|   210 | 1654 | `		pTos->x.pOther = pClone;` |
|   210 | 1655 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|     - | 1656 | `	}` |
|   210 | 1657 | `	VM_EXIT_BREAK;` |
|   ! 0 | 1658 | `	VM_EXIT_BREAK;` |
|   112 | 1659 | `}` |
|     - | 1660 |  |
