# src/ph7/vm_ops_oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1407/1581 lines (88.99%)

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
|       - |   25 | ` * OP_NEW: body moved verbatim from the OP_NEW arm of` |
|       - |   26 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |   27 | ` */` |
| 2117214 |   28 | `PH7_PRIVATE VmOpRc VmExecOpNew(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   29 | `{` |
| 2117219 |   30 | `	ph7_value *pTos = pState->pTos;` |
| 2117219 |   31 | `	ph7_value *pStack = pState->pStack;` |
| 2117219 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
| 2117219 |   33 | `	sxi32 pc = pState->pc;` |
|       - |   34 | `	sxi32 rc;` |
|       - |   35 | `	/* Constructor arg count: compile-time args plus THIS new's own unpack` |
|       - |   36 | `	 * expansion (iP2 = hasSpread, transferred from the popped OP_CALL —` |
|       - |   37 | ``	 * `new C(...$args)` used to ignore the extras, leaving the expanded`` |
|       - |   38 | `	 * elements ABOVE the class-name slot and fataling "Class ' ' is not` |
|       - |   39 | `	 * defined"). VmSpreadOwnExtra counts only this new's own runs, so a nested` |
|       - |   40 | `	 * spread call in the ctor arg list stays scoped to itself. */` |
|       - |   41 | `	/* iP1 < 0 is the SCREEN pass: php's NEW resolves the class, refuses everything a` |
|       - |   42 | ``	 * `new` can be refused for and allocates the object BEFORE the constructor arguments`` |
|       - |   43 | `	 * are evaluated — only the constructor body runs after them. PHL evaluated the whole` |
|       - |   44 | ``	 * argument list first, so `new NoSuchClass(s(1))`, `new AbstractC(s(1))` and`` |
|       - |   45 | ``	 * `new PrivateCtorC(s(1))` all ran `s(1)` on a `new` php never performs. The screen`` |
|       - |   46 | `	 * is this same handler with no arguments on the stack, returning just before the` |
|       - |   47 | `	 * allocation and LEAVING the class name for the real pass that follows it — one code` |
|       - |   48 | `	 * path, so the two can never disagree about what a refusal is. */` |
| 2117219 |   49 | `	int bScreenOnly = pInstr->iP1 < 0;` |
| 2673206 |   50 | `	sxi32 nCtorArgs = bScreenOnly ? 0` |
| 1614594 |   51 | `		: pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|       - |   52 | `	ph7_value *pArg;` |
| 2117219 |   53 | `	ph7_class *pClass = 0;` |
|       - |   54 | `	ph7_class_instance *pNew;` |
|       - |   55 | `	/* Resolving the class below can run an AUTOLOADER that throws. Snapshot the boundary` |
|       - |   56 | `	 * rail so the "not found" report can tell a missing class from an autoloader that` |
|       - |   57 | `	 * raised — php propagates the autoloader's exception and reports nothing else, while` |
|       - |   58 | `	 * this arm used to throw its own Error on top of it (uncaught, killing the script` |
|       - |   59 | `	 * right after the real exception had been handled). */` |
| 2117219 |   60 | `	sxi32 nNewBrc = pVm->nBoundaryRc;` |
| 2117219 |   61 | `	const void *pNewRes = (const void *)pVm->pResumeFrame;` |
| 2117219 |   62 | `	pArg = &pTos[-nCtorArgs]; /* Constructor arguments (if available) */` |
|       - |   63 | `	/* Same PHP 8.1 spread-key realignment as OP_CALL: build a per-actual-slot name` |
|       - |   64 | `	 * map when the ctor arg list unpacked string-keyed elements. NEW keeps the` |
|       - |   65 | `	 * class-name slot at pTos (above the args), so pArg is already the correct base;` |
|       - |   66 | `	 * the build also truncates this call's captured runs. */` |
|       - |   67 | `	VmCallArgMap sEffNewMap;` |
|       - |   68 | `	/* The screen pass has no arguments and no runs of its own: building (and` |
|       - |   69 | `	 * truncating) an effective map there would speak for the enclosing call. */` |
| 2673206 |   70 | `	VmCallArgMap *pEffNewMap = bScreenOnly ? 0 : VmEffCallArgMap(pVm,pInstr,pArg,` |
| 1111974 |   71 | `		nCtorArgs > 0 ? (sxu32)nCtorArgs : 0,&sEffNewMap);` |
| 3175824 |   72 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
| 2117215 |   73 | `		const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
| 2117215 |   74 | `		sxu32 nCls = SyBlobLength(&pTos->sBlob);` |
| 2117210 |   75 | `		if( (nCls == sizeof("self")-1 && SyMemcmp(zCls,"self",sizeof("self")-1) == 0)` |
| 2117198 |   76 | `		 \|\| (nCls == sizeof("static")-1 && SyMemcmp(zCls,"static",sizeof("static")-1) == 0)` |
| 2117154 |   77 | `		 \|\| (nCls == sizeof("parent")-1 && SyMemcmp(zCls,"parent",sizeof("parent")-1) == 0) ){` |
|       - |   78 | `			/* new self() / new static() / new parent(): resolve against the live` |
|       - |   79 | `			 * class context (LSB for static), sharing the FCC resolver. */` |
|      66 |   80 | `			pClass = VmFccResolveScope(&(*pVm),pTos);` |
|      66 |   81 | `			if( pClass && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) ){` |
|       - |   82 | `				/* Not new-able (the named path's iLoadable extract excludes these` |
|       - |   83 | `				 * before it ever gets here): php's wording, with the RESOLVED name. */` |
|     ! 0 |   84 | `				VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Cannot instantiate %s %z",` |
|     ! 0 |   85 | `					(pClass->iFlags & PH7_CLASS_INTERFACE) ? "interface" : "abstract class",` |
|     ! 0 |   86 | `					&pClass->sName);` |
|     ! 0 |   87 | `				PH7_MemObjRelease(pTos);` |
|     ! 0 |   88 | `				if( nCtorArgs > 0 ){` |
|     ! 0 |   89 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |   90 | `				}` |
|     ! 0 |   91 | `				VM_EXIT_ABORT;` |
|       - |   92 | `			}` |
|      34 |   93 | `		}else{` |
|       - |   94 | `			/* Try to extract the desired class */` |
| 2117151 |   95 | `			pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,` |
|       - |   96 | `				TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|       5 |   97 | `		}` |
| 1058610 |   98 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - |   99 | `		/* Take the base class from the loaded instance */` |
|       5 |  100 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       2 |  101 | `	}` |
| 2117219 |  102 | `	if( pClass == 0 ){` |
|       - |  103 | ``		/* php: `new NoSuch()` is a CATCHABLE Error ('Class "NoSuch" not found'), not a`` |
|       - |  104 | `		 * hard stop. Aborting here killed the script outright -- and with exit status 0,` |
|       - |  105 | `		 * so a caller could not even tell it had failed. */` |
|       - |  106 | `		SyBlob sErrM;` |
|       - |  107 | `		sxi32 rcErr;` |
|      43 |  108 | `		ph7_class *pNotNew = 0;` |
|      43 |  109 | `		if( PH7_VmClassLookupRaised(&(*pVm),nNewBrc,pNewRes) ){` |
|       - |  110 | `			/* The autoloader threw: land THAT (an in-place catch leaves 0 behind plus a` |
|       - |  111 | `			 * recorded resume frame, which the router picks up). */` |
|       5 |  112 | `			sxi32 rcAuto = pVm->nBoundaryRc;` |
|       5 |  113 | `			pVm->nBoundaryRc = 0;` |
|       5 |  114 | `			if( nCtorArgs > 0 ){` |
|     ! 0 |  115 | `				VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  116 | `			}` |
|       5 |  117 | `			PH7_MemObjRelease(pTos);` |
|       5 |  118 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 |  119 | `			pTos->nIdx = SXU32_HIGH;` |
|       5 |  120 | `			if( rcAuto == PH7_ABORT ){` |
|     ! 0 |  121 | `				VM_EXIT_ABORT;` |
|       - |  122 | `			}` |
|       5 |  123 | `			rc = PH7_EXCEPTION;` |
|       5 |  124 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  125 | `		}` |
|      39 |  126 | `		SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      39 |  127 | `		if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       - |  128 | `			/* The extract above only accepts NEW-able classes, so an interface, an` |
|       - |  129 | `			 * abstract class or a trait comes back as 0 and used to be reported as` |
|       - |  130 | `			 * "not found". Look again without that filter so php's real message can be` |
|       - |  131 | `			 * given — but through the table DIRECTLY, never PH7_VmExtractClass: that` |
|       - |  132 | `			 * one fires the autoloader when the name is absent, so a genuinely missing` |
|       - |  133 | `			 * class ran every registered autoloader TWICE where php runs them once. */` |
|      39 |  134 | `			const char *zNotNew = (const char *)SyBlobData(&pTos->sBlob);` |
|      39 |  135 | `			sxu32 nNotNew = SyBlobLength(&pTos->sBlob);` |
|       - |  136 | `			SyHashEntry *pNotNewEntry;` |
|      39 |  137 | `			PH7_VmClassNameAnchor(&zNotNew,&nNotNew);` |
|      39 |  138 | `			pNotNewEntry = nNotNew > 0 ? SyHashGet(&pVm->hClass,(const void *)zNotNew,nNotNew) : 0;` |
|      39 |  139 | `			pNotNew = pNotNewEntry ? (ph7_class *)pNotNewEntry->pUserData : 0;` |
|      18 |  140 | `		}` |
|      47 |  141 | `		if( pNotNew && (pNotNew->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) ){` |
|       - |  142 | `			/* php names WHAT it will not instantiate; a trait is one of the three, and` |
|       - |  143 | `			 * PH7's loadable-only extract rejected it as "not found" — the one shape of` |
|       - |  144 | ``			 * `new` whose refusal did not say why. */`` |
|      25 |  145 | `			const char *zKind = (pNotNew->iFlags & PH7_CLASS_INTERFACE) ? "interface"` |
|      14 |  146 | `				: (pNotNew->iFlags & PH7_CLASS_TRAIT) ? "trait" : "abstract class";` |
|      17 |  147 | `			SyBlobFormat(&sErrM,"Cannot instantiate %s %z",zKind,&pNotNew->sName);` |
|       9 |  148 | `		}else{` |
|      23 |  149 | `			SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|      20 |  150 | `				SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob));` |
|       - |  151 | `		}` |
|       - |  152 | `		/* Settle the stack BEFORE throwing: the class-name slot becomes the (NULL)` |
|       - |  153 | `		 * expression result and the ctor arguments go. */` |
|      39 |  154 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  155 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  156 | `		}` |
|      39 |  157 | `		PH7_MemObjRelease(pTos);` |
|      39 |  158 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|      39 |  159 | `		pTos->nIdx = SXU32_HIGH;` |
|      57 |  160 | `		rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      18 |  161 | `			SyBlobLength(&sErrM));` |
|      39 |  162 | `		SyBlobRelease(&sErrM);` |
|      39 |  163 | `		if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      39 |  164 | `		rc = rcErr;` |
|      47 |  165 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
| 2117179 |  166 | `	}else if( pClass->iFlags & PH7_CLASS_NOINSTANTIATE ){` |
|       - |  167 | `		/* php refuses this one in the create_object handler, so the refusal names the` |
|       - |  168 | `		 * INSTANTIATION and never reaches the constructor — which matters because the` |
|       - |  169 | `		 * class may still declare a private __construct that Reflection prints. Same` |
|       - |  170 | `		 * shape as the enum reject below: no instance, no __destruct. */` |
|       - |  171 | `		SyBlob sErrMsg;` |
|      12 |  172 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      12 |  173 | `		if( pClass->zNewRefusal ){` |
|       - |  174 | `			/* php words a few of these per class — Directory's names dir() as the` |
|       - |  175 | `			 * way to get one — so the spec's own sentence wins when it has one. */` |
|       8 |  176 | `			SyBlobAppend(&sErrMsg,pClass->zNewRefusal,SyStrlen(pClass->zNewRefusal));` |
|       5 |  177 | `		}else{` |
|       5 |  178 | `			SyBlobFormat(&sErrMsg,"Instantiation of class %z is not allowed",&pClass->sName);` |
|       - |  179 | `		}` |
|      12 |  180 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      12 |  181 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  182 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  183 | `		}` |
|      12 |  184 | `		PH7_MemObjRelease(pTos);` |
|      12 |  185 | `		pTos->nIdx = SXU32_HIGH;` |
|      12 |  186 | `		VM_EXIT_BREAK;` |
| 2117169 |  187 | `	}else if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|       - |  188 | `		/* php 8.1: enums cannot be instantiated — a catchable Error, raised` |
|       - |  189 | `		 * BEFORE any construction (no instance, no __destruct). */` |
|       - |  190 | `		SyBlob sErrMsg;` |
|       7 |  191 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       7 |  192 | `		SyBlobFormat(&sErrMsg,"Cannot instantiate enum %z",&pClass->sName);` |
|       7 |  193 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       7 |  194 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  195 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  196 | `		}` |
|       7 |  197 | `		PH7_MemObjRelease(pTos);` |
|       7 |  198 | `		pTos->nIdx = SXU32_HIGH;` |
|       7 |  199 | `		VM_EXIT_BREAK;` |
| 2117158 |  200 | `	}else if( VmClassStaticDeferPending(pClass)` |
| 1058586 |  201 | `	       && (rc = PH7_VmMaterializeClassStatics(&(*pVm),pClass)) != SXRET_OK ){` |
|       - |  202 | `		/* php materializes the static table at instantiation too, so a default` |
|       - |  203 | `		 * that THREW at the declaration (or failed its type check) raises BEFORE` |
|       - |  204 | `		 * any construction (no instance, no __destruct) — same shape as the enum` |
|       - |  205 | `		 * reject above: park the status, settle the stack, and let the` |
|       - |  206 | `		 * fetch-point router land it. */` |
|       6 |  207 | `		VmBoundaryPark(&(*pVm),rc);` |
|       6 |  208 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  209 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  210 | `		}` |
|       6 |  211 | `		PH7_MemObjRelease(pTos);` |
|       6 |  212 | `		pTos->nIdx = SXU32_HIGH;` |
|       6 |  213 | `		VM_EXIT_BREAK;` |
|     ! 0 |  214 | `	}else{` |
|       - |  215 | `		ph7_class_method *pCons;` |
|       - |  216 | `		/* Check if a constructor is available — BEFORE instantiation: a` |
|       - |  217 | ``		 * visibility-denied `new` must not construct (nor later destruct)`` |
|       - |  218 | `		 * the object (band A #4). */` |
|       - |  219 | `		/* Only an explicit __construct is the constructor. PHP-4-style class-name` |
|       - |  220 | `		 * constructors were removed in PHP 8.0 — a method named like the class is a` |
|       - |  221 | `		 * plain method, so no same-name fallback here. */` |
| 2117159 |  222 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       - |  223 | `		/* Constructor visibility (band A #4): __construct now KEEPS its` |
|       - |  224 | `		 * declared protection (PH7_NewClassMethod no longer forces it` |
|       - |  225 | ``		 * public), so `new C()` on a private/protected constructor from`` |
|       - |  226 | `		 * the wrong scope is php's catchable Error. Reflection's` |
|       - |  227 | `		 * newInstance path sets bReflectBypass like method invoke. */` |
| 2117159 |  228 | `		if( pCons && pCons->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - |  229 | `			/* php binds non-public access by the member's DECLARING class, not the` |
|       - |  230 | `			 * instantiated class: a private constructor declared in a base is` |
|       - |  231 | `			 * reachable from that base's own methods even when instantiating a` |
|       - |  232 | ``			 * subclass (`new Child()` inside Base::factory()). __construct is a`` |
|       - |  233 | `			 * method, so pass its declaring class (sFunc.pUserData) — mirroring the` |
|       - |  234 | `			 * method-call visibility check — rather than pClass, whose hAttr lookup` |
|       - |  235 | `			 * for a method name always misses and falls to a wrong exact-class test. */` |
|      39 |  236 | `			ph7_class *pCtorDecl = pCons->sFunc.pUserData ? (ph7_class *)pCons->sFunc.pUserData : pClass;` |
|      39 |  237 | `			if( pVm->bReflectBypass ){` |
|     ! 0 |  238 | `				pVm->bReflectBypass = 0;` |
|      39 |  239 | `			}else if( !PH7_VmClassMemberAccess(&(*pVm),pCtorDecl,&pCons->sFunc.sName,pCons->iProtection,FALSE) ){` |
|       - |  240 | `				SyBlob sErrMsg;` |
|      25 |  241 | `				const char *zVis = pCons->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       - |  242 | `				/* php NAMES the calling scope when there is one (see the method twin in` |
|       - |  243 | `				 * OP_CALL); "global scope" is only for code outside every class. */` |
|      25 |  244 | `				ph7_class *pCtorScope = PH7_VmCallerScopeName(&(*pVm));` |
|      25 |  245 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      25 |  246 | `				if( pCtorScope ){` |
|       9 |  247 | `					SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from scope %z",` |
|       4 |  248 | `						zVis,&pClass->sName,&pCtorScope->sName);` |
|       5 |  249 | `				}else{` |
|      17 |  250 | `					SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from global scope",` |
|       8 |  251 | `						zVis,&pClass->sName);` |
|       - |  252 | `				}` |
|      25 |  253 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       - |  254 | `				/* Pop ctor args + release the class-name operand, leave NULL` |
|       - |  255 | `				 * as the expression value; the fetch-point router lands the` |
|       - |  256 | `				 * throw. No instance was created. */` |
|      25 |  257 | `				if( nCtorArgs > 0 ){` |
|     ! 0 |  258 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  259 | `				}` |
|      25 |  260 | `				PH7_MemObjRelease(pTos);` |
|      25 |  261 | `				pTos->nIdx = SXU32_HIGH;` |
|      25 |  262 | `				VM_EXIT_BREAK;` |
|       - |  263 | `			}` |
|       7 |  264 | `		}` |
| 2117135 |  265 | `		if( bScreenOnly ){` |
|       - |  266 | `			/* Every refusal above has been asked. Leave the class name standing for` |
|       - |  267 | `			 * the real pass and let the arguments run. */` |
| 1005221 |  268 | `			VM_EXIT_BREAK;` |
|       - |  269 | `		}` |
| 1111919 |  270 | `		if( nCtorArgs > 0 ){` |
|       - |  271 | ``			/* D1: a `new`'s arguments are ARGUMENTS. An element/property one whose`` |
|       - |  272 | `			 * target was absent at load time rides a deferred carrier that only OP_CALL` |
|       - |  273 | `			 * resolved, and the ctor call reaches its callee by pointer rather than` |
|       - |  274 | ``			 * through that opcode — so `new C($a['missing'])` passed a silent NULL where`` |
|       - |  275 | ``			 * php warns, and a by-REFERENCE ctor parameter (`new C($a['fresh'])` with`` |
|       - |  276 | ``			 * `__construct(&$x)`) never vivified the target at all: php writes the`` |
|       - |  277 | `			 * element, PHL left it uncreated. Resolve here, where the constructor is` |
|       - |  278 | `			 * known and pVm->pFrame is still the caller, exactly as every OP_CALL` |
|       - |  279 | `			 * dispatch branch does. A class with NO constructor still resolves — the` |
|       - |  280 | `			 * arguments were evaluated and php reports what reading them found. */` |
| 1005235 |  281 | `			ph7_class_method *pCtorArgs = pCons;` |
|       - |  282 | `			sxi32 rcDA;` |
| 1005235 |  283 | `			if( pCtorArgs == 0 ){` |
|       3 |  284 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pEffNewMap);` |
| 1005234 |  285 | `			}else if( pCtorArgs->sFunc.iFlags & VM_FUNC_NATIVE ){` |
|       - |  286 | `				/* A native constructor has no compiled formals; its by-ref positions` |
|       - |  287 | `				 * come from the signature-derived mask, the builtin way. */` |
| 1004757 |  288 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
| 1004752 |  289 | `					pCtorArgs->sFunc.pNative ? pCtorArgs->sFunc.pNative->nByRefMask : 0,0,0,pEffNewMap);` |
|  502381 |  290 | `			}else{` |
|     719 |  291 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|     476 |  292 | `					(ph7_vm_func_arg *)SySetBasePtr(&pCtorArgs->sFunc.aArgs),` |
|     238 |  293 | `					SySetUsed(&pCtorArgs->sFunc.aArgs),0,0,0,pEffNewMap);` |
|       - |  294 | `			}` |
| 1005235 |  295 | `			if( rcDA == PH7_ABORT \|\| rcDA == PH7_EXCEPTION ){` |
|       - |  296 | `				/* Reading an argument raised (a string-offset Error, a magic accessor's` |
|       - |  297 | `				 * throw): no object is created, and the stack is tidied exactly as the` |
|       - |  298 | `				 * constructor-throw path below tidies it. */` |
|       - |  299 | `				sxi32 iResumeDA;` |
|       3 |  300 | `				if( rcDA == PH7_ABORT ){` |
|     ! 0 |  301 | `					VM_EXIT_ABORT;` |
|       - |  302 | `				}` |
|       3 |  303 | `				if( VmRecordedResume(pVm,&iResumeDA,pState->pEntryFrame,aInstr) ){` |
|     ! 0 |  304 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  305 | `					PH7_MemObjRelease(pTos);` |
|     ! 0 |  306 | `					PH7_RESUME_DRAIN()` |
|     ! 0 |  307 | `					pc = iResumeDA;` |
|     ! 0 |  308 | `					VM_EXIT_BREAK;` |
|       - |  309 | `				}` |
|       3 |  310 | `				VM_EXIT_EXCEPTION;` |
|       - |  311 | `			}` |
|  502614 |  312 | `		}` |
|       - |  313 | `		/* Create a new class instance */` |
| 1111917 |  314 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
| 1111917 |  315 | `		if( pNew == 0 ){` |
|     ! 0 |  316 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  317 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|     ! 0 |  318 | `				&pClass->sName` |
|       - |  319 | `			);` |
|     ! 0 |  320 | `			PH7_MemObjRelease(pTos);` |
|     ! 0 |  321 | `			if( nCtorArgs > 0 ){` |
|       - |  322 | `				/* Pop given arguments */` |
|     ! 0 |  323 | `				VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  324 | `			}` |
|     ! 0 |  325 | `			VM_EXIT_BREAK;` |
|       - |  326 | `		}` |
| 1111917 |  327 | `		if( pVm->nBoundaryRc != 0 ){` |
|       - |  328 | `			/* A typed property DEFAULT failed its type check during instance-` |
|       - |  329 | `			 * frame creation (parked catchable TypeError): php aborts the` |
|       - |  330 | `			 * construction — no constructor call, no object. Consume the` |
|       - |  331 | `			 * parked status here and route exactly like a constructor throw` |
|       - |  332 | `			 * (the exception object is already registered). */` |
|      35 |  333 | `			sxi32 rcDef = pVm->nBoundaryRc;` |
|       - |  334 | `			sxi32 iDefResumePc;` |
|      35 |  335 | `			pVm->nBoundaryRc = 0;` |
|      35 |  336 | `			PH7_ClassInstanceUnref(pNew);` |
|      35 |  337 | `			if( rcDef == PH7_ABORT ){` |
|     ! 0 |  338 | `				VM_EXIT_ABORT;` |
|       - |  339 | `			}` |
|      35 |  340 | `			if( VmRecordedResume(pVm,&iDefResumePc,pState->pEntryFrame,aInstr) ){` |
|       - |  341 | `				/* This frame's own try caught it in-place: tidy the stack` |
|       - |  342 | `				 * (pop ctor args + release the class-name slot, then drain any` |
|       - |  343 | `				 * abandoned outer-expression operands to the try's base — the` |
|       - |  344 | `				 * class-name slot itself sits above it) and resume. */` |
|      35 |  345 | `				if( nCtorArgs > 0 ){` |
|       3 |  346 | `					VmPopOperand(&pTos,nCtorArgs);` |
|       1 |  347 | `				}` |
|      35 |  348 | `				PH7_MemObjRelease(pTos);` |
|      75 |  349 | `				PH7_RESUME_DRAIN()` |
|      35 |  350 | `				pc = iDefResumePc;` |
|      35 |  351 | `				VM_EXIT_BREAK;` |
|       - |  352 | `			}` |
|     ! 0 |  353 | `			VM_EXIT_EXCEPTION;` |
|       - |  354 | `		}` |
| 1111883 |  355 | `		if( pCons ){` |
|       - |  356 | `			/* Call the class constructor.  Collect args in stack order and` |
|       - |  357 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|       - |  358 | `			 * receiving OP_CALL path runs its named-argument matching` |
|       - |  359 | `			 * (including variadic string-key packing). */` |
| 1107233 |  360 | `			VmCallArgMap *pNewMap = pEffNewMap;` |
|       - |  361 | `			sxi32 rcCons;` |
|       - |  362 | `			SySet aArg; /* ctor argument scratch (the loop's shared set stays with OP_CALL) */` |
| 1107233 |  363 | `			SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
| 2113773 |  364 | `			while( pArg < pTos ){` |
| 1006545 |  365 | `				SySetPut(&aArg,(const void *)&pArg);` |
| 1006545 |  366 | `				pArg++;` |
|       5 |  367 | `			}` |
|       - |  368 | `			/* Too-few-arguments is php's catchable ArgumentCountError, raised` |
|       - |  369 | `			 * by the shared OP_CALL install path this ctor call routes through` |
|       - |  370 | `			 * (was a PHL-only notice here). */` |
| 1107233 |  371 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
| 1107233 |  372 | `			SySetRelease(&aArg); /* scratch dies here, on every exit path */` |
|       - |  373 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
| 1107233 |  374 | `			if( pNew->iRef < 1 ){` |
|     ! 0 |  375 | `				pNew->iRef = 1;` |
|     ! 0 |  376 | `			}` |
| 1107233 |  377 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|       - |  378 | `				/* The constructor raised: the half-constructed object must not` |
|       - |  379 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|       - |  380 | `				 * The class-name operand (and any leftover args) are released by` |
|       - |  381 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|       - |  382 | `				sxi32 iResumePc;` |
|  100322 |  383 | `				PH7_ClassInstanceUnref(pNew);` |
|  100322 |  384 | `				if( rcCons == PH7_ABORT ){` |
|       5 |  385 | `					VM_EXIT_ABORT;` |
|       - |  386 | `				}` |
|  100318 |  387 | `				if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){` |
|       - |  388 | `					/* This frame's own try caught it in-place: tidy the stack` |
|       - |  389 | `					 * (pop ctor args + release the class-name slot, then drain any` |
|       - |  390 | `					 * abandoned outer-expression operands to the try's base — the` |
|       - |  391 | `					 * class-name slot itself sits above it) and resume. */` |
|  100140 |  392 | `					if( nCtorArgs > 0 ){` |
|     132 |  393 | `						VmPopOperand(&pTos,nCtorArgs);` |
|      64 |  394 | `					}` |
|  100140 |  395 | `					PH7_MemObjRelease(pTos);` |
|  500276 |  396 | `					PH7_RESUME_DRAIN()` |
|  100140 |  397 | `					pc = iResumePc;` |
|  100140 |  398 | `					VM_EXIT_BREAK;` |
|       - |  399 | `				}` |
|     181 |  400 | `				VM_EXIT_EXCEPTION;` |
|       - |  401 | `			}` |
|  503455 |  402 | `		}` |
| 1011565 |  403 | `		if( nCtorArgs > 0 ){` |
|       - |  404 | `			/* Pop given arguments */` |
| 1004931 |  405 | `			VmPopOperand(&pTos,nCtorArgs);` |
|  502463 |  406 | `		}` |
| 1011565 |  407 | `		PH7_MemObjRelease(pTos);` |
| 1011565 |  408 | `		pTos->x.pOther = pNew;` |
| 1011565 |  409 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - |  410 | `	}` |
| 1011565 |  411 | `	VM_EXIT_BREAK;` |
|     ! 0 |  412 | `	VM_EXIT_BREAK;` |
| 1058612 |  413 | `}` |
|       - |  414 |  |
|       - |  415 | `/*` |
|       - |  416 | ` * Is this OP_MEMBER fetching the property for WRITING — php's BP_VAR_W / BP_VAR_RW` |
|       - |  417 | ` * fetch, the one that asks the object for something to MODIFY? The compiler tags` |
|       - |  418 | `` * the base of a subscript-write / `??=` PH7_MEMBER_WRITE, so that answers most of`` |
|       - |  419 | ``  * it once the shapes that share the tag are excluded: a plain store and a `??=` `` |
|       - |  420 | ` * write through their own paths, and a direct read-modify-write is the accessor` |
|       - |  421 | ` * pair (VmMagicRmwArm), not a write fetch. The two php compiles as plain READS —` |
|       - |  422 | `` * binding a reference (`$r = &$o->p`) and iterating by reference — are told apart`` |
|       - |  423 | `` * by the instruction that follows. `$o->p =& $x` is NOT one of them: the member is`` |
|       - |  424 | ` * the reference TARGET there and carries its own iP2.` |
|       - |  425 | ` */` |
|    5833 |  426 | `static int VmMemberFetchForWrite(const VmInstr *pInstr)` |
|       5 |  427 | `{` |
|    5838 |  428 | `	const VmInstr *pNext = pInstr + 1;` |
|    5838 |  429 | `	if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|      15 |  430 | `		int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);` |
|      15 |  431 | `		int bCoalesceW = (pNext->iOp == PH7_OP_NULLC_JMP);` |
|      15 |  432 | `		return !bPlainStore && !bCoalesceW && !VmMemberNextIsRmw(pNext);` |
|       - |  433 | `	}` |
|    5824 |  434 | `	if( pInstr->iP2 == PH7_MEMBER_READ ){` |
|    3588 |  435 | `		if( pNext->iOp == PH7_OP_STORE_REF ){` |
|       5 |  436 | `			return 1;` |
|       - |  437 | `		}` |
|    3584 |  438 | `		if( pNext->iOp == PH7_OP_FOREACH_INIT && pNext->p3 ){` |
|      39 |  439 | `			return (((ph7_foreach_info *)pNext->p3)->iFlags & PH7_4EACH_STEP_REF) != 0;` |
|       - |  440 | `		}` |
|    1771 |  441 | `	}` |
|    5782 |  442 | `	return 0;` |
|    2922 |  443 | `}` |
|       - |  444 | `/*` |
|       - |  445 | ` * A native class's property is a field of php's own C struct, and the only writes` |
|       - |  446 | ` * that reach one are the writes the store filter converts (ph7_class::xSet). So` |
|       - |  447 | ` * the fetched value keeps its slot index for exactly the shapes that end in such` |
|       - |  448 | `` * a store -- a plain assignment, `??=`, a destructuring target and the`` |
|       - |  449 | ` * read-modify-write forms -- and is a TEMPORARY for every other use. That is` |
|       - |  450 | ` * php's own answer: it has no ptr_ptr handler for such a property, so a reference` |
|       - |  451 | `` * bind (`$r = &$i->f`), a by-reference argument (`preg_match($p,$s,$i->s)`) and a`` |
|       - |  452 | ` * by-reference foreach all get a copy whose writes are SILENTLY lost -- silently,` |
|       - |  453 | ` * unlike the overloaded case, which php has a notice for.` |
|       - |  454 | ` */` |
|    1118 |  455 | `static int VmMemberNativeSetKeepsSlot(const VmInstr *pInstr)` |
|       2 |  456 | `{` |
|    1120 |  457 | `	if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|       - |  458 | ``		/* Every fetch the compiler tags for writing: a plain store, `??=`, a`` |
|       - |  459 | `		 * compound assign, and the base of a subscript write — that last one has` |
|       - |  460 | ``		 * to reach the real value so `$i->y[0] = 5` is php's "Cannot use a scalar`` |
|       - |  461 | `		 * value as an array" rather than a write nobody notices. */` |
|     610 |  462 | `		return 1;` |
|       - |  463 | `	}` |
|     512 |  464 | `	if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       5 |  465 | `		return 1;   /* list()/foreach destructuring target */` |
|       - |  466 | `	}` |
|     508 |  467 | `	return VmMemberNextIsWrite(pInstr + 1);   /* ++/-- and the compound assigns */` |
|     561 |  468 | `}` |
|       - |  469 | `/*` |
|       - |  470 | `` * php's `Indirect modification of overloaded property C::$p has no effect`: the`` |
|       - |  471 | ` * write-context fetch above landed on a property only __get answers for, so what` |
|       - |  472 | ` * comes back is a VALUE and whatever the rest of the expression writes into it is` |
|       - |  473 | ` * thrown away. php says so and carries on.` |
|       - |  474 | ` *` |
|       - |  475 | ` * PHL had the notice at one site only (a by-reference ARGUMENT, VmBindPropByRef)` |
|       - |  476 | ` * and, worse, the value was not a copy: __get's return still shared the object's` |
|       - |  477 | `` * own nested hashmap by COW, so `$o->a['b'] = 9`, `$o->a[] = 5` and a by-reference`` |
|       - |  478 | ` * foreach modified the object php leaves untouched. Separating it here is what` |
|       - |  479 | ` * makes the write land nowhere.` |
|       - |  480 | ` *` |
|       - |  481 | ` * An ENGINE __get is not this — php routes those through the class's own property` |
|       - |  482 | ` * handler, which hands back the real element (ArrayObject's ARRAY_AS_PROPS reads` |
|       - |  483 | ` * and writes through, in both engines). php stays silent for an OBJECT value too:` |
|       - |  484 | ` * a handle is shared, so nothing about the write is indirect.` |
|       - |  485 | ` */` |
|      28 |  486 | `PH7_PRIVATE void PH7_VmOverloadedPropNotice(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,ph7_value *pVal)` |
|       1 |  487 | `{` |
|      29 |  488 | `	ph7_class_method *pGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|      29 |  489 | `	if( pGet == 0 \|\| (pGet->sFunc.iFlags & VM_FUNC_NATIVE) ){` |
|     ! 0 |  490 | `		return;` |
|       - |  491 | `	}` |
|      29 |  492 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       3 |  493 | `		return;` |
|       - |  494 | `	}` |
|      40 |  495 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - |  496 | `		"Indirect modification of overloaded property %z::$%z has no effect",` |
|      13 |  497 | `		&pClass->sName,pName);` |
|      27 |  498 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      25 |  499 | `		PH7_HashmapCowSeparate(&(*pVm),pVal);` |
|      12 |  500 | `	}` |
|      15 |  501 | `}` |
|       - |  502 | `/*` |
|       - |  503 | ` * Does an OVERLOADED read-modify-write apply to this property access? php runs` |
|       - |  504 | ` * one only when the class answers BOTH sides; the guard means we are already` |
|       - |  505 | ` * inside this property's own __get, where php behaves as if the accessor were` |
|       - |  506 | ` * absent.` |
|       - |  507 | ` */` |
|      38 |  508 | `static int VmMagicRmwEligible(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,const SyString *pName)` |
|       1 |  509 | `{` |
|      53 |  510 | `	return PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) != 0` |
|      33 |  511 | `	    && PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1) != 0` |
|      52 |  512 | `	    && !VmMagicGuardHeld(pVm,(void *)pThis,pName,'g');` |
|       1 |  513 | `}` |
|       - |  514 | `/*` |
|       - |  515 | `` * php's read-modify-write of an OVERLOADED property — `$o->n++`, `--$o->n`,`` |
|       - |  516 | `` * `$o->n += 2`, `$o->n .= 'x'` on a name the instance does not expose. php reads`` |
|       - |  517 | ` * the current value through __get, lets the modify op compute on it, and writes` |
|       - |  518 | ` * the result back through __set; PHL fell through to dynamic-property creation` |
|       - |  519 | ` * instead, so the ordinary shape (a class declaring BOTH accessors) died on` |
|       - |  520 | `` * `Cannot create dynamic property C::$n` — or, when the name IS declared but`` |
|       - |  521 | `` * inaccessible from this scope, on `Cannot access private property C::$n` —`` |
|       - |  522 | ` * for a statement php runs.` |
|       - |  523 | ` *` |
|       - |  524 | ` * The write-back rides the same pending-entry rail property HOOKS use: the` |
|       - |  525 | ` * current value goes into a fresh SCRATCH memobj the modify op mutates in` |
|       - |  526 | ` * place, and the entry makes that op's tail (PH7_HOOK_RMW_WRITEBACK) dispatch` |
|       - |  527 | ` * __set with the computed value.` |
|       - |  528 | ` *` |
|       - |  529 | ` * BOTH accessors are required, which is php's own split: with only __get php` |
|       - |  530 | ` * goes on to CREATE the property (PHL's §10 policy refuses a dynamic property),` |
|       - |  531 | `` * and with only __set it warns `Undefined property` and reads null — neither is`` |
|       - |  532 | ` * this path.` |
|       - |  533 | ` *` |
|       - |  534 | ` * *pOut takes __get's value and *pnScratch the slot the modify op must address;` |
|       - |  535 | ` * the caller does its own stack surgery afterwards, because pName still aliases` |
|       - |  536 | `` * the NAME operand when the name is dynamic (`$o->$k++`) and releasing that`` |
|       - |  537 | ` * operand first would leave it dangling. *pnScratch stays SXU32_HIGH when __get` |
|       - |  538 | ` * threw (nothing is armed — the fetch-point router lands the parked throw and` |
|       - |  539 | ` * php's __set never runs) or when the scratch reservation failed.` |
|       - |  540 | ` */` |
|      28 |  541 | `static void VmMagicRmwArm(` |
|       - |  542 | `	ph7_vm *pVm,` |
|       - |  543 | `	ph7_class_instance *pThis,` |
|       - |  544 | `	ph7_class *pClass,` |
|       - |  545 | `	const SyString *pName,` |
|       - |  546 | `	ph7_value *pOut,` |
|       - |  547 | `	sxu32 *pnScratch,` |
|       - |  548 | `	void *pOwnerStack,` |
|       - |  549 | `	void *pInstrs,` |
|       - |  550 | `	sxu32 nPc` |
|       - |  551 | `	)` |
|       1 |  552 | `{` |
|       - |  553 | `	ph7_value *pScr;` |
|       - |  554 | `	VmHookRmw sRmw;` |
|      29 |  555 | `	*pnScratch = SXU32_HIGH;` |
|      29 |  556 | `	VmMagicGuardPush(pVm,(void *)pThis,pName,'g');` |
|      29 |  557 | `	PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,pName,pOut);` |
|      29 |  558 | `	VmMagicGuardPop(pVm);` |
|      29 |  559 | `	if( pVm->nBoundaryRc != 0 ){` |
|       3 |  560 | `		PH7_MemObjRelease(pOut);` |
|       3 |  561 | `		return;` |
|       - |  562 | `	}` |
|      27 |  563 | `	pScr = PH7_ReserveMemObj(&(*pVm));` |
|      27 |  564 | `	if( pScr == 0 ){` |
|       - |  565 | `		/* OOM: loud allocator diagnostics already fired; the value still stands. */` |
|     ! 0 |  566 | `		return;` |
|       - |  567 | `	}` |
|      27 |  568 | `	PH7_MemObjStore(pOut,pScr);` |
|      27 |  569 | `	sRmw.iKind = VM_HOOK_PEND_RMW_MAGIC;` |
|      27 |  570 | `	sRmw.pThis = pThis;` |
|      27 |  571 | `	sRmw.pAttr = 0;` |
|      27 |  572 | `	sRmw.nBackIdx = SXU32_HIGH;` |
|      27 |  573 | `	sRmw.nScratchIdx = pScr->nIdx;` |
|      27 |  574 | `	SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      27 |  575 | `	SyBlobAppend(&sRmw.sName,(const void *)pName->zString,pName->nByte);` |
|      27 |  576 | `	sRmw.pOwnerStack = pOwnerStack;` |
|      27 |  577 | `	sRmw.pInstrs = pInstrs;` |
|      27 |  578 | `	sRmw.nJmpPc = nPc;  /* the modify op ... */` |
|      27 |  579 | `	sRmw.nPc = nPc;     /* ... is the whole window */` |
|      27 |  580 | `	pThis->iRef++;` |
|      27 |  581 | `	SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      27 |  582 | `	*pnScratch = pScr->nIdx;` |
|      15 |  583 | `}` |
|       - |  584 | `/*` |
|       - |  585 | ` * OP_MEMBER: body moved verbatim from the OP_MEMBER arm of` |
|       - |  586 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  587 | ` */` |
|  346285 |  588 | `PH7_PRIVATE VmOpRc VmExecOpMember(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  589 | `{` |
|  346290 |  590 | `	ph7_value *pTos = pState->pTos;` |
|  346290 |  591 | `	ph7_value *pStack = pState->pStack;` |
|  346290 |  592 | `	VmInstr *aInstr = pState->aInstr;` |
|  346290 |  593 | `	sxi32 pc = pState->pc;` |
|       - |  594 | `	sxi32 rc;` |
|       - |  595 | `	ph7_class_instance *pThis;` |
|       - |  596 | `	ph7_value *pNos;` |
|       - |  597 | `	SyString sName;` |
|  346290 |  598 | `	if( !pInstr->iP1 ){` |
|  242718 |  599 | `		pNos = &pTos[-1];` |
|       - |  600 | `#ifdef UNTRUST` |
|       - |  601 | `		if( pNos < pStack ){` |
|       - |  602 | `			VM_EXIT_ABORT;` |
|       - |  603 | `		}` |
|       - |  604 | `#endif` |
|  242713 |  605 | `		if( pInstr->iP2 == PH7_MEMBER_DEFPATH` |
|  123201 |  606 | `		 && (pNos->iFlags & (MEMOBJ_AUX_DEFPATH\|MEMOBJ_AUX_DEFERRED)) ){` |
|       - |  607 | `			/* D1 commit 2: deferred property arg ($o->p) whose base is itself a deferred lvalue` |
|       - |  608 | ``			 * (a nested chain `$a["k"]->p`, or an undefined base object). Append a property step`` |
|       - |  609 | `			 * to the base's captured path; OP_CALL re-walks it. */` |
|       - |  610 | `			SyString sProp;` |
|     275 |  611 | `			VmDeferredPath *pPath = 0;` |
|     275 |  612 | `			SyStringInitFromBuf(&sProp,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     275 |  613 | `			if( pNos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|     275 |  614 | `				pPath = (VmDeferredPath *)pNos->x.pOther;` |
|     275 |  615 | `				VmDeferPathPushProp(pPath,&sProp);` |
|     139 |  616 | `			}else{` |
|       - |  617 | `				SyString sRootName;` |
|     ! 0 |  618 | `				SyStringInitFromBuf(&sRootName,(const char *)pNos->x.pOther,` |
|       - |  619 | `					pNos->x.pOther ? SyStrlen((const char *)pNos->x.pOther) : 0);` |
|     ! 0 |  620 | `				pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|     ! 0 |  621 | `				if( pPath ){` |
|     ! 0 |  622 | `					pNos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name */` |
|     ! 0 |  623 | `					pNos->x.pOther = pPath;` |
|     ! 0 |  624 | `					pNos->iFlags \|= MEMOBJ_AUX_DEFPATH;` |
|     ! 0 |  625 | `					pNos->nIdx = SXU32_HIGH;` |
|     ! 0 |  626 | `					VmDeferPathPushProp(pPath,&sProp);` |
|     ! 0 |  627 | `				}` |
|       - |  628 | `			}` |
|     275 |  629 | `			VmPopOperand(&pTos,1); /* drop the property name; the carrier stays as pTos */` |
|     275 |  630 | `			VM_EXIT_BREAK;` |
|       - |  631 | `		}` |
|  242446 |  632 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - |  633 | `			ph7_class *pClass;` |
|       - |  634 | `			/* Class already instantiated */` |
|  242316 |  635 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       - |  636 | `			/* Point to the instantiated class */` |
|  242316 |  637 | `			pClass = pThis->pClass;` |
|       - |  638 | `			/* Extract attribute name first */` |
|  242316 |  639 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|  242316 |  640 | `			if( PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
|       - |  641 | `				/* __PHP_Incomplete_Class: every property access and method call is` |
|       - |  642 | `				 * php's incomplete-object diagnostic — the carrier's OWN entries are` |
|       - |  643 | `				 * for the engine's surfaces only. A READ (isset/empty/?? included)` |
|       - |  644 | `				 * is an E_WARNING answering NULL; every WRITE shape and unset() is` |
|       - |  645 | `				 * a catchable Error; a method call is its own Error, raised where` |
|       - |  646 | `				 * the undefined-method twin raises (before any argument runs). The` |
|       - |  647 | `				 * DEFPATH pre-pass defers like a MISSING property would — the slot` |
|       - |  648 | `				 * fast path must not hand out the carrier's storage — so the by-ref` |
|       - |  649 | `				 * resolve (VmBindPropByRef) raises the Error and the by-value` |
|       - |  650 | `				 * re-drive lands back here in READ context for the warning.` |
|       - |  651 | ``				 * `??=` reads before it refuses, so it takes BOTH, php's order. */`` |
|      37 |  652 | `				VmInstr *pIncNext = pInstr + 1;` |
|      37 |  653 | `				if( pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|       - |  654 | `					SyString sIncProp;` |
|       5 |  655 | `					VmDeferredPath *pIncPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|       5 |  656 | `					SyStringInitFromBuf(&sIncProp,sName.zString,sName.nByte);` |
|       5 |  657 | `					if( pIncPath && VmDeferPathPushProp(pIncPath,&sIncProp) == SXRET_OK ){` |
|       5 |  658 | `						VmPopOperand(&pTos,1);       /* drop the property name */` |
|       5 |  659 | `						pThis->iRef++;` |
|       5 |  660 | `						PH7_MemObjRelease(pTos);     /* collapse the object slot into the carrier */` |
|       5 |  661 | `						pTos->x.pOther = pIncPath;` |
|       5 |  662 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|       5 |  663 | `						pTos->nIdx = SXU32_HIGH;` |
|       5 |  664 | `						PH7_ClassInstanceUnref(pThis);` |
|       5 |  665 | `						VM_EXIT_BREAK;` |
|       - |  666 | `					}` |
|     ! 0 |  667 | `					if( pIncPath ){` |
|     ! 0 |  668 | `						VmFreeDeferredPath(pIncPath);` |
|     ! 0 |  669 | `					}` |
|       - |  670 | `					/* allocation failure (or a temporary base below): the read` |
|       - |  671 | `					 * verdict is all that is left — fall through to warn + NULL. */` |
|     ! 0 |  672 | `				}` |
|      33 |  673 | `				int bIncCall = (pInstr->iP2 == PH7_MEMBER_METHOD);` |
|      33 |  674 | `				int bIncCoalW = (pInstr->iP2 == PH7_MEMBER_WRITE && pIncNext->iOp == PH7_OP_NULLC_JMP);` |
|      65 |  675 | `				int bIncModify = pInstr->iP2 != PH7_MEMBER_DEFPATH` |
|      63 |  676 | `					&& (pInstr->iP2 == PH7_MEMBER_UNSET` |
|      31 |  677 | `					 \|\| pInstr->iP2 == PH7_MEMBER_WRITE` |
|      25 |  678 | `					 \|\| pInstr->iP2 == PH7_MEMBER_REF_TARGET` |
|      19 |  679 | `					 \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|      18 |  680 | `					 \|\| VmMemberNextIsWrite(pIncNext)` |
|      18 |  681 | `					 \|\| VmMemberFetchForWrite(pInstr));` |
|      33 |  682 | `				if( bIncCall ){` |
|       - |  683 | `					SyBlob sIncErr;` |
|       - |  684 | `					sxi32 rcInc;` |
|       3 |  685 | `					SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|       3 |  686 | `					PH7_VmIncompleteMsg(&(*pVm),pThis,"call a method",&sIncErr);` |
|       3 |  687 | `					VmPopOperand(&pTos,1);` |
|       3 |  688 | `					PH7_MemObjRelease(pTos);` |
|       3 |  689 | `					pTos->nIdx = SXU32_HIGH;` |
|       4 |  690 | `					rcInc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sIncErr),` |
|       1 |  691 | `						SyBlobLength(&sIncErr));` |
|       3 |  692 | `					SyBlobRelease(&sIncErr);` |
|       3 |  693 | `					if( rcInc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  694 | `					rc = rcInc;` |
|       3 |  695 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  696 | `				}` |
|      31 |  697 | `				if( bIncCoalW ){` |
|       3 |  698 | `					PH7_VmIncompleteAccessWarn(&(*pVm),pThis,0);` |
|       1 |  699 | `				}` |
|      31 |  700 | `				if( bIncModify ){` |
|       - |  701 | `					SyBlob sIncErr;` |
|      17 |  702 | `					SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|      17 |  703 | `					PH7_VmIncompleteMsg(&(*pVm),pThis,"modify a property",&sIncErr);` |
|      17 |  704 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sIncErr));` |
|       9 |  705 | `				}else{` |
|      15 |  706 | `					PH7_VmIncompleteAccessWarn(&(*pVm),pThis,0);` |
|       - |  707 | `				}` |
|      31 |  708 | `				VmPopOperand(&pTos,1);   /* pop the attribute name */` |
|      31 |  709 | `				PH7_MemObjRelease(pTos); /* the object slot becomes the NULL answer */` |
|      31 |  710 | `				pTos->nIdx = SXU32_HIGH;` |
|      31 |  711 | `				VM_EXIT_BREAK;` |
|       - |  712 | `			}` |
|  242280 |  713 | `			if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - |  714 | `				/* Method call */` |
|  128229 |  715 | `				ph7_class_method *pMeth = 0;` |
|  128229 |  716 | `				if( sName.nByte > 0 ){` |
|       - |  717 | `					/* Extract the target method */` |
|  128229 |  718 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|   64113 |  719 | `				}` |
|  128229 |  720 | `				if( pMeth == 0 ){` |
|      67 |  721 | `					ph7_class_method *pCallMagic = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|      67 |  722 | `					if( pCallMagic ){` |
|       - |  723 | `						/* php: a missing method dispatches __call($name, $args) — the` |
|       - |  724 | `						 * args are only collected by the following OP_CALL, so stash the` |
|       - |  725 | `						 * receiver + class + original name and MARK the callee slot` |
|       - |  726 | `						 * (band A #3b; pre-fix the name-only call discarded everything` |
|       - |  727 | `						 * and the call site failed with "Invalid function name"). Stack:` |
|       - |  728 | `						 * pop the method name, then the receiver slot becomes the marked` |
|       - |  729 | `						 * carrier OP_CALL routes through the packing body. */` |
|      57 |  730 | `						VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pThis,pClass,&sName);` |
|      57 |  731 | `						if( pPend == 0 ){` |
|     ! 0 |  732 | `							VM_EXIT_ABORT;` |
|       - |  733 | `						}` |
|      57 |  734 | `						VmPopOperand(&pTos,1);` |
|      57 |  735 | `						PH7_MemObjRelease(pTos);` |
|      57 |  736 | `						pTos->x.pOther = pPend;` |
|      57 |  737 | `						pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|      30 |  738 | `					}else{` |
|       - |  739 | `						{` |
|       - |  740 | `							/* php raises a catchable Error here; PH7 printed a notice and CARRIED ON` |
|       - |  741 | `							 * with NULL, so the call silently produced nothing. */` |
|       - |  742 | `							SyBlob sErrM;` |
|       - |  743 | `							sxi32 rcErr;` |
|      11 |  744 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      11 |  745 | `							SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",&pClass->sName,&sName);` |
|      11 |  746 | `							VmPopOperand(&pTos,1);` |
|      11 |  747 | `							PH7_MemObjRelease(pTos);` |
|      11 |  748 | `							pTos->nIdx = SXU32_HIGH;` |
|      16 |  749 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       5 |  750 | `								SyBlobLength(&sErrM));` |
|      11 |  751 | `							SyBlobRelease(&sErrM);` |
|      11 |  752 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 |  753 | `							rc = rcErr;` |
|      11 |  754 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  755 | `						}` |
|       - |  756 | `					}` |
|      30 |  757 | `				}else{` |
|  128165 |  758 | `					ph7_class_method *pDeniedCall = 0;` |
|  128165 |  759 | `					int bDenied = 0;` |
|       - |  760 | `					/* php decides a method's visibility against the class that OWNS it,` |
|       - |  761 | `					 * which for a trait method is the class that composed it — a trait is` |
|       - |  762 | `					 * a compile-time construct php has flattened away by now. Deciding` |
|       - |  763 | `					 * against the trait instead made every rule a question of who USES it:` |
|       - |  764 | `					 * a protected trait method was denied to a SUBCLASS of the composing` |
|       - |  765 | `					 * class (not a trait user itself) and granted to an unrelated class` |
|       - |  766 | `					 * that happened to use the same trait. */` |
|  128165 |  767 | `					ph7_class *pOwner = 0;` |
|  128160 |  768 | `					if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC` |
|   64143 |  769 | `					 && !PH7_VmClassMemberAccess(&(*pVm),` |
|     114 |  770 | `						(pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth)),` |
|      57 |  771 | `						&sName,pMeth->iProtection,FALSE) ){` |
|      56 |  772 | `						int bRebound = 0;` |
|      56 |  773 | `						if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - |  774 | `							/* php: when the instance's class SHADOWS the caller's` |
|       - |  775 | `							 * private method (child redeclares private m), code in` |
|       - |  776 | `							 * the caller's class dispatches its OWN private m, not` |
|       - |  777 | `							 * the shadow. Rebind to the calling scope's method when` |
|       - |  778 | `							 * that scope declares a private one of this name. */` |
|      48 |  779 | `							VmFrame *pFrameLocal = VmSkipExceptionFrames(pVm->pFrame);` |
|      48 |  780 | `							ph7_vm_func *pCallerFunc = (ph7_vm_func *)pFrameLocal->pUserData;` |
|      48 |  781 | `							if( pCallerFunc && (pCallerFunc->iFlags & VM_FUNC_CLASS_METHOD) && pCallerFunc->pUserData ){` |
|       6 |  782 | `								ph7_class *pScope = (ph7_class *)pCallerFunc->pUserData;` |
|       6 |  783 | `								ph7_class_method *pOwn = PH7_ClassExtractMethod(pScope,sName.zString,sName.nByte);` |
|       4 |  784 | `								if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       6 |  785 | `								 && (ph7_class *)pOwn->sFunc.pUserData == pScope ){` |
|       6 |  786 | `									pMeth = pOwn;` |
|       6 |  787 | `									bRebound = 1;` |
|       2 |  788 | `								}` |
|       2 |  789 | `							}` |
|      22 |  790 | `						}` |
|      56 |  791 | `						if( !bRebound ){` |
|       - |  792 | `							/* Inaccessible from this scope: php routes through __call when` |
|       - |  793 | `							 * declared (band A #3b); without it the refusal is raised right` |
|       - |  794 | `							 * here, beside the undefined-method and abstract-method twins. */` |
|      52 |  795 | `							pDeniedCall = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|      52 |  796 | `							bDenied = pDeniedCall == 0;` |
|      24 |  797 | `						}` |
|      26 |  798 | `					}` |
|  128165 |  799 | `					if( bDenied ){` |
|       - |  800 | ``						/* php's visibility Error for `$o->m()`. It is raised HERE, at the`` |
|       - |  801 | `						 * member resolution, and not left to OP_CALL: the method OP_CALL` |
|       - |  802 | `						 * would re-derive is looked up by the FUNCTION's own name against` |
|       - |  803 | `						 * its declaring class, which a trait adaptation splits away from` |
|       - |  804 | ``						 * the entry this lookup actually chose — `hi as private pHi` gave`` |
|       - |  805 | ``						 * OP_CALL the trait's public `hi`, so the private alias ran from`` |
|       - |  806 | `						 * global scope in silence. The entry that answered is right here,` |
|       - |  807 | `						 * with its composed protection.` |
|       - |  808 | `						 *` |
|       - |  809 | ``						 * php names the method as the CALL SPELLS it (`$o->PQ()` on a`` |
|       - |  810 | ``						 * private `pQ()` reports `PQ`) and the class that OWNS the method,`` |
|       - |  811 | `						 * which for a trait method is the composing class. */` |
|       - |  812 | `						SyBlob sErrM;` |
|       - |  813 | `						sxi32 rcErr;` |
|      42 |  814 | `						ph7_class *pScope = PH7_VmCallerScopeName(&(*pVm));` |
|      61 |  815 | `						const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      19 |  816 | `							? "private" : "protected";` |
|      42 |  817 | `						SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      42 |  818 | `						if( pScope ){` |
|       7 |  819 | `							SyBlobFormat(&sErrM,"Call to %s method %z::%z() from scope %z",` |
|       3 |  820 | `								zVis,&pOwner->sName,&sName,&pScope->sName);` |
|       4 |  821 | `						}else{` |
|      36 |  822 | `							SyBlobFormat(&sErrM,"Call to %s method %z::%z() from global scope",` |
|      16 |  823 | `								zVis,&pOwner->sName,&sName);` |
|       - |  824 | `						}` |
|      42 |  825 | `						VmPopOperand(&pTos,1);` |
|      42 |  826 | `						PH7_MemObjRelease(pTos);` |
|      42 |  827 | `						pTos->nIdx = SXU32_HIGH;` |
|      61 |  828 | `						rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      19 |  829 | `							SyBlobLength(&sErrM));` |
|      42 |  830 | `						SyBlobRelease(&sErrM);` |
|      42 |  831 | `						if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      42 |  832 | `						rc = rcErr;` |
|      52 |  833 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  834 | `					}` |
|  128127 |  835 | `					if( pDeniedCall ){` |
|      11 |  836 | `						VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pThis,pClass,&sName);` |
|      11 |  837 | `						if( pPend == 0 ){` |
|     ! 0 |  838 | `							VM_EXIT_ABORT;` |
|       - |  839 | `						}` |
|      11 |  840 | `						VmPopOperand(&pTos,1);` |
|      11 |  841 | `						PH7_MemObjRelease(pTos);` |
|      11 |  842 | `						pTos->x.pOther = pPend;` |
|      11 |  843 | `						pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|       6 |  844 | `					}else{` |
|       - |  845 | `						/* Push method name on the stack, MARKED as already screened: the` |
|       - |  846 | `						 * decision above was made against the entry this lookup chose,` |
|       - |  847 | `						 * which is the only place a trait adaptation's composed` |
|       - |  848 | `						 * protection is visible. */` |
|  128117 |  849 | `						PH7_MemObjRelease(pTos);` |
|  128117 |  850 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|  128117 |  851 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|  128117 |  852 | `						pTos->iFlags \|= MEMOBJ_AUX_MEMBERCALL;` |
|       - |  853 | `					}` |
|       - |  854 | `				}` |
|  128181 |  855 | `				pTos->nIdx = SXU32_HIGH;` |
|   64094 |  856 | `			}else{` |
|       - |  857 | `				/* Attribute access. iP2: 0 = read, 2 = unset, 3 = isset, 4 = empty. */` |
|  114056 |  858 | `				VmClassAttr *pObjAttr = 0;` |
|  114056 |  859 | `				SyHashEntry *pEntry = 0;` |
|  114051 |  860 | `				if( sName.nByte > 0 && sName.zString[0] == 0` |
|   57052 |  861 | `				 && !PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
|       - |  862 | `					/* php refuses a property name beginning with a NUL outright:` |
|       - |  863 | ``					 * `Cannot access property starting with "\0"`. Those bytes are`` |
|       - |  864 | `					 * the ENGINE's private mangling — "\0Cls\0p" is C1::$p and` |
|       - |  865 | `					 * "\0*\0p" is a protected slot — so a script that could write one` |
|       - |  866 | `					 * would forge a private member of any class it names, and every` |
|       - |  867 | `					 * display surface would then render the forgery as the real thing.` |
|       - |  868 | `					 * Two rules ride with it, both probe-verified: a MAGIC accessor` |
|       - |  869 | `					 * wins (php dispatches __get/__set/__isset/__unset with the raw` |
|       - |  870 | `					 * name and says nothing), and a LOOKUP context is silent — isset()` |
|       - |  871 | ``					 * is false, empty() true, `??` takes the default — which is what`` |
|       - |  872 | `					 * the existing miss handling below already answers. The refusal` |
|       - |  873 | `					 * does not depend on whether such a property exists: php raises it` |
|       - |  874 | `					 * on the NAME, before any lookup, and the one place a NUL-keyed` |
|       - |  875 | `					 * property legitimately lives is the __PHP_Incomplete_Class` |
|       - |  876 | `					 * carrier, whose own gate above has already answered for it.` |
|       - |  877 | ``					 * A METHOD name is not covered — php lets `$o->{"\0m"}()` reach`` |
|       - |  878 | `					 * the ordinary undefined-method Error — so this sits in the` |
|       - |  879 | `					 * attribute branch only. */` |
|       - |  880 | `					const char *zNulMagic;` |
|      51 |  881 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       5 |  882 | `						zNulMagic = "__unset";` |
|      49 |  883 | `					}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|      46 |  884 | `					       \|\| VmMemberNextIsWrite(pInstr + 1) ){` |
|      19 |  885 | `						zNulMagic = "__set";` |
|      10 |  886 | `					}else{` |
|      29 |  887 | `						zNulMagic = "__get";` |
|       - |  888 | `					}` |
|      50 |  889 | `					if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      47 |  890 | `					 && PH7_ClassExtractMethod(pClass,zNulMagic,(sxu32)SyStrlen(zNulMagic)) == 0 ){` |
|       - |  891 | `						SyBlob sNulErr;` |
|       - |  892 | `						sxi32 rcNul;` |
|      33 |  893 | `						SyBlobInit(&sNulErr,&pVm->sAllocator);` |
|      33 |  894 | `						SyBlobAppend(&sNulErr,"Cannot access property starting with \"\\0\"",` |
|       - |  895 | `							sizeof("Cannot access property starting with \"\\0\"")-1);` |
|      33 |  896 | `						VmPopOperand(&pTos,1);   /* pop the attribute name */` |
|      33 |  897 | `						PH7_MemObjRelease(pTos); /* the object slot becomes the NULL answer */` |
|      33 |  898 | `						pTos->nIdx = SXU32_HIGH;` |
|      49 |  899 | `						rcNul = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sNulErr),` |
|      16 |  900 | `							SyBlobLength(&sNulErr));` |
|      33 |  901 | `						SyBlobRelease(&sNulErr);` |
|      33 |  902 | `						if( rcNul == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      33 |  903 | `						rc = rcNul;` |
|      33 |  904 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  905 | `					}` |
|       9 |  906 | `				}` |
|       - |  907 | `				/* Extract the target attribute. The EMPTY name is a real property` |
|       - |  908 | ``				 * name in php — `$o->{''} = 1` creates one and `$o->{''}` reads it`` |
|       - |  909 | `				 * back — and it is the one name SyHashGet cannot answer for, so it` |
|       - |  910 | `				 * takes the list-walking lookup; every other name keeps the direct` |
|       - |  911 | `				 * hash probe this path has always made. */` |
|  114024 |  912 | `				if( sName.nByte > 0 ){` |
|  114016 |  913 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|   57011 |  914 | `				}else{` |
|       9 |  915 | `					pEntry = PH7_ClassInstanceAttrEntry(pThis,sName.zString,0);` |
|       - |  916 | `				}` |
|  114024 |  917 | `				if( pEntry ){` |
|       - |  918 | `					/* Point to the attribute value */` |
|  107401 |  919 | `					pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|   53698 |  920 | `				}` |
|  114019 |  921 | `				if( pObjAttr` |
|  110708 |  922 | `				 && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT))` |
|   53713 |  923 | `				 && PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,` |
|      20 |  924 | `					pObjAttr->pAttr->iProtection,FALSE) ){` |
|       - |  925 | `					/* A static property (or a constant) belongs to the CLASS: php does` |
|       - |  926 | `					 * not find it through an instance at all. It notices the attempt —` |
|       - |  927 | ``					 * `Accessing static property C::$s as non static` — and then treats`` |
|       - |  928 | `					 * the name as an ordinary MISSING property: a read warns and answers` |
|       - |  929 | `					 * null, isset() is false, a write goes to a dynamic property (which` |
|       - |  930 | `					 * PHL rejects by the §10 policy, like any other undeclared write).` |
|       - |  931 | `					 * PHL's instance table carries an entry for every declared member` |
|       - |  932 | ``					 * (statics share the class slot), so `$o->s` used to READ and — far`` |
|       - |  933 | `					 * worse — WRITE the class's own static in silence. isset()/empty()` |
|       - |  934 | `					 * pass silently, as php's do. */` |
|      18 |  935 | `					if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      18 |  936 | `					 && pInstr->iP2 != PH7_MEMBER_DEFPATH ){` |
|       - |  937 | `						/* Silent where php is silent: isset()/empty(), and any class` |
|       - |  938 | `						 * that declares the MAGIC accessor this context would dispatch` |
|       - |  939 | ``						 * (php passes `silent = ce->__get/__set/__unset != NULL` to its`` |
|       - |  940 | `						 * property-offset lookup). Once per access, too: the deferred` |
|       - |  941 | `						 * call-argument PRE-PASS (PH7_MEMBER_DEFPATH) re-runs this op for` |
|       - |  942 | ``						 * the same source `$o->s` and the value pass carries the notice`` |
|       - |  943 | `						 * — the BY-REF form has no value pass and notices at its own` |
|       - |  944 | `						 * site (VmBindPropByRef). */` |
|       - |  945 | `						const char *zMagic;` |
|      10 |  946 | `						if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       3 |  947 | `							zMagic = "__unset";` |
|       8 |  948 | `						}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|       8 |  949 | `						       \|\| VmMemberNextIsWrite(pInstr + 1) ){` |
|       3 |  950 | `							zMagic = "__set";` |
|       2 |  951 | `						}else{` |
|       6 |  952 | `							zMagic = "__get";` |
|       - |  953 | `						}` |
|      10 |  954 | `						if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) == 0 ){` |
|      11 |  955 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - |  956 | `								"Accessing static property %z::$%z as non static",` |
|       6 |  957 | `								&pClass->sName,&pObjAttr->pAttr->sName);` |
|       3 |  958 | `						}` |
|       4 |  959 | `					}` |
|      20 |  960 | `					pEntry = 0;` |
|      20 |  961 | `					pObjAttr = 0;` |
|       9 |  962 | `				}` |
|  114024 |  963 | `				if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - |  964 | `					/* unset($o->prop): remove the property entirely so it disappears from` |
|       - |  965 | `					 * foreach / json_encode / get_object_vars / (array) — matching PHP (a value-only` |
|       - |  966 | `					 * release would leave a zombie null entry). Leave a NULL constant on the stack so` |
|       - |  967 | `					 * the trailing generic unset() builtin is a no-op (mirrors LOAD_IDX iP2=5).` |
|       - |  968 | `					 * php dispatches __unset($name) for a MISSING or INACCESSIBLE property` |
|       - |  969 | `					 * (band A #3b — pre-fix an inaccessible private was silently DELETED from` |
|       - |  970 | `					 * outside the class); without __unset, an inaccessible unset is php's` |
|       - |  971 | `					 * catchable "Cannot access ..." Error and a missing one stays a no-op. */` |
|      61 |  972 | `					int bUnsAccessible = pEntry ? PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) : 0;` |
|      61 |  973 | `					if( pEntry && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) != 0 ){` |
|       - |  974 | `						/* A native class's property is php's own C struct field, and` |
|       - |  975 | `						 * unset() is a std handler that looks for a REAL property and` |
|       - |  976 | `						 * finds none: nothing happens, nothing is said, and the next` |
|       - |  977 | `						 * read still answers the struct. Removing the slot here left` |
|       - |  978 | ``						 * `unset($i->y); $i->y` an Undefined property warning and NULL`` |
|       - |  979 | `						 * for a statement php ignores. */` |
|      62 |  980 | `					}else if( pEntry && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) != 0 ){` |
|       - |  981 | `						/* php 8.4: a hooked property (virtual or backed) can never be` |
|       - |  982 | `						 * unset — catchable Error, even from inside its own hook body` |
|       - |  983 | `						 * (probe-verified). */` |
|       - |  984 | `						SyBlob sErrMsg;` |
|       5 |  985 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       5 |  986 | `						SyBlobFormat(&sErrMsg,"Cannot unset hooked property %z::$%z",` |
|       4 |  987 | `							&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       5 |  988 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      57 |  989 | `					}else if( pEntry && bUnsAccessible ){` |
|      40 |  990 | `						PH7_VmReleaseInstanceAttr(&(*pVm),pObjAttr);` |
|      40 |  991 | `						SyHashDeleteEntry2(pEntry);` |
|      21 |  992 | `					}else{` |
|      16 |  993 | `						ph7_class_method *pUnsetMagic = PH7_ClassExtractMethod(pClass,"__unset",sizeof("__unset")-1);` |
|      16 |  994 | `						if( pUnsetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'u') ){` |
|      14 |  995 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'u');` |
|      14 |  996 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__unset",sizeof("__unset")-1,&sName,0);` |
|      14 |  997 | `							VmMagicGuardPop(pVm);` |
|       9 |  998 | `						}else if( pEntry ){` |
|       - |  999 | `							/* Inaccessible and no __unset: php's catchable Error. Parked on` |
|       - | 1000 | `							 * the boundary rail; the op completes benignly and the` |
|       - | 1001 | `							 * fetch-point router lands it. */` |
|       - | 1002 | `							SyBlob sErrMsg;` |
|       3 | 1003 | `							const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       3 | 1004 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 1005 | `							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",zVis,&pClass->sName,&sName);` |
|       3 | 1006 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       1 | 1007 | `						}` |
|       - | 1008 | `						/* Missing property without __unset: silent no-op (php). */` |
|       - | 1009 | `					}` |
|      61 | 1010 | `					VmPopOperand(&pTos,1);    /* pop the attribute name */` |
|      61 | 1011 | `					PH7_MemObjRelease(pTos);  /* release the object on the stack ($o's stack ref) */` |
|      61 | 1012 | `					pTos->nIdx = SXU32_HIGH;  /* NULL constant */` |
|      61 | 1013 | `					VM_EXIT_BREAK;` |
|       - | 1014 | `				}` |
|  113966 | 1015 | `				if( pObjAttr == 0 ){` |
|       - | 1016 | `					/* Member not present on the instance and the next instruction writes/modifies it` |
|       - | 1017 | ``					 * (store, array-append/keyed-write, `??=`, ++/--, or a compound-assign — see`` |
|       - | 1018 | `					 * VmMemberNextIsWrite; the compiler always emits a terminating PH7_OP_DONE so` |
|       - | 1019 | `					 * pInstr+1 is in-bounds). Create the property so the operation lands on a real slot` |
|       - | 1020 | ``					 * (PHP auto-vivifies a fresh property for all these forms, not just `=`):`` |
|       - | 1021 | `					 *   - a DECLARED prop that was unset() and is re-assigned → recreate it` |
|       - | 1022 | `					 *     (PHP re-appends it at the end), OR` |
|       - | 1023 | `					 *   - a dynamic prop on a dynamic-allowing class (stdClass).` |
|       - | 1024 | `					 * The created slot is NULL with a real nIdx, which the modify-op then vivifies` |
|       - | 1025 | `					 * (NULL→array for [], NULL→0/"" for ++/.=) and writes back in place.` |
|       - | 1026 | `					 * Two signals: the compiler tags the member itself iP2=PH7_MEMBER_WRITE when it is` |
|       - | 1027 | ``					 * the base of a write-subscript / `??=` (the modify-op is not the immediately-next`` |
|       - | 1028 | `					 * instruction there), and for ++/--/compound-assign/store the next opcode is the` |
|       - | 1029 | `					 * modify-op directly (VmMemberNextIsWrite). */` |
|    6636 | 1030 | `					VmInstr *pNext = pInstr + 1;` |
|    6631 | 1031 | `					if( pInstr->iP2 == PH7_MEMBER_WRITE \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|    6093 | 1032 | `					 \|\| VmMemberNextIsWrite(pNext) ){` |
|     549 | 1033 | `						ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pThis->pClass,sName.zString,sName.nByte);` |
|     549 | 1034 | `						if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      12 | 1035 | `							VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pObjAttr);` |
|       7 | 1036 | `						}else{` |
|       - | 1037 | `							/* php 8 semantics (band A #3b): a PLAIN store to a missing` |
|       - | 1038 | `							 * property dispatches __set($name,$value) when declared —` |
|       - | 1039 | `							 * the value only exists at the following OP_STORE, so park` |
|       - | 1040 | `							 * the receiver+name for it (one-instruction lifetime; the` |
|       - | 1041 | `							 * guard makes a same-name write inside __set fall through` |
|       - | 1042 | `							 * to dynamic creation, like php). Without __set — or for a` |
|       - | 1043 | `							 * subscript-write base / read-modify-write, which php does` |
|       - | 1044 | `							 * NOT route through __set — create a dynamic property on` |
|       - | 1045 | `							 * ANY class with the 8.2 deprecation (suppressed for` |
|       - | 1046 | `							 * stdClass and #[AllowDynamicProperties]); a readonly` |
|       - | 1047 | `							 * class raises php's catchable Error instead. */` |
|     539 | 1048 | `							ph7_class_method *pSetMagic = 0;` |
|     539 | 1049 | `							ph7_class_method *pCoalIsset = 0, *pCoalGet = 0, *pCoalSet = 0;` |
|     539 | 1050 | `							int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);` |
|     539 | 1051 | `							if( bPlainStore \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|     475 | 1052 | `								pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|     235 | 1053 | `							}` |
|     539 | 1054 | `							if( pSetMagic && pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 1055 | `								/* php dispatches __set($name, element) for a missing` |
|       - | 1056 | `								 * destructuring target, but the element value only exists` |
|       - | 1057 | `								 * at the following OP_LOAD_LIST — the dispatch is` |
|       - | 1058 | `								 * unsupported (recorded residual). Leave the miss: the` |
|       - | 1059 | `								 * property stays uncreated, matching php's observable` |
|       - | 1060 | `								 * state (its __set did not store either). */` |
|     539 | 1061 | `							}else if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){` |
|     375 | 1062 | `								pThis->iRef++;` |
|     375 | 1063 | `								pVm->pMagicSetThis = pThis;` |
|     375 | 1064 | `								SyBlobReset(&pVm->sMagicSetName);` |
|     375 | 1065 | `								SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       - | 1066 | `								/* pObjAttr stays NULL; the miss path below stays silent. */` |
|     351 | 1067 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|      65 | 1068 | `							 && pNext->iOp == PH7_OP_NULLC_JMP` |
|      44 | 1069 | `							 && ((pCoalIsset = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1)) != 0` |
|       8 | 1070 | `							  \|\| (pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)) != 0` |
|       5 | 1071 | `							  \|\| (pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1)) != 0) ){` |
|       - | 1072 | ``								/* `$o->p ??= v` on a missing property with magic accessors:`` |
|       - | 1073 | `								 * php consults __isset first when declared — false means` |
|       - | 1074 | `								 * assign directly through __set with NO __get call (the` |
|       - | 1075 | `								 * test value stays null); true (or no __isset) means __get` |
|       - | 1076 | `								 * provides the test value. A COAL_MAGIC entry is pushed` |
|       - | 1077 | `								 * for the OP_NULLC_STORE at the jump target; the` |
|       - | 1078 | `								 * fetch-point sweep drops it when the short-circuit jump` |
|       - | 1079 | `								 * skips the assign or a throw abandons the RHS. Without` |
|       - | 1080 | `								 * __set the assign side keeps the pre-existing loud` |
|       - | 1081 | `								 * "Cannot perform assignment" path (php would create a` |
|       - | 1082 | `								 * dynamic property there — recorded residual). Note the` |
|       - | 1083 | `								 * condition's short-circuit assignment chain: a hit on an` |
|       - | 1084 | `								 * earlier method leaves the later pointers unresolved, so` |
|       - | 1085 | `								 * re-resolve the leftovers here (each is one hash probe,` |
|       - | 1086 | `								 * only on this ??=-miss path). */` |
|       - | 1087 | `								ph7_value sTest;` |
|       7 | 1088 | `								int bMiss = 0; /* __isset said false: skip __get, test value stays null */` |
|       7 | 1089 | `								if( pCoalIsset != 0 ){` |
|       5 | 1090 | `									pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       2 | 1091 | `								}` |
|       7 | 1092 | `								if( pCoalIsset != 0 \|\| pCoalGet != 0 ){` |
|       7 | 1093 | `									pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|       3 | 1094 | `								}` |
|       7 | 1095 | `								PH7_MemObjInit(pVm,&sTest);` |
|       7 | 1096 | `								if( pCoalIsset && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 1097 | `									ph7_value sIssetRet;` |
|       5 | 1098 | `									PH7_MemObjInit(pVm,&sIssetRet);` |
|       5 | 1099 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|       5 | 1100 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|       5 | 1101 | `									VmMagicGuardPop(pVm);` |
|       5 | 1102 | `									PH7_MemObjToBool(&sIssetRet);` |
|       5 | 1103 | `									bMiss = sIssetRet.x.iVal == 0;` |
|       5 | 1104 | `									PH7_MemObjRelease(&sIssetRet);` |
|       2 | 1105 | `								}` |
|       7 | 1106 | `								if( !bMiss && pCoalGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       5 | 1107 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       5 | 1108 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sTest);` |
|       5 | 1109 | `									VmMagicGuardPop(pVm);` |
|       2 | 1110 | `								}` |
|       6 | 1111 | `								if( pCoalSet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s')` |
|       7 | 1112 | `								 && pVm->nBoundaryRc == 0 ){` |
|       - | 1113 | `									VmHookRmw sPend;` |
|       7 | 1114 | `									sPend.iKind = VM_HOOK_PEND_COAL_MAGIC;` |
|       7 | 1115 | `									sPend.pThis = pThis;` |
|       7 | 1116 | `									sPend.pAttr = 0;` |
|       7 | 1117 | `									sPend.nBackIdx = SXU32_HIGH;` |
|       7 | 1118 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|       7 | 1119 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|       7 | 1120 | `									SyBlobAppend(&sPend.sName,(const void *)sName.zString,sName.nByte);` |
|       7 | 1121 | `									sPend.pOwnerStack = (void *)pStack;` |
|       7 | 1122 | `									sPend.pInstrs = (void *)aInstr;` |
|       7 | 1123 | `									sPend.nJmpPc = (sxu32)(pc + 1);        /* the OP_NULLC_JMP */` |
|       7 | 1124 | `									sPend.nPc = (sxu32)(pNext->iP2 - 1);   /* the OP_NULLC_STORE */` |
|       7 | 1125 | `									pThis->iRef++;` |
|       7 | 1126 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|       3 | 1127 | `								}` |
|       - | 1128 | `								/* Pop the attribute name; the test value becomes the` |
|       - | 1129 | `								 * expression slot (a temp, not an lvalue). */` |
|       7 | 1130 | `								VmPopOperand(&pTos,1);` |
|       7 | 1131 | `								pThis->iRef++;` |
|       7 | 1132 | `								PH7_MemObjRelease(pTos);` |
|       7 | 1133 | `								PH7_MemObjStore(&sTest,pTos);` |
|       7 | 1134 | `								pTos->nIdx = SXU32_HIGH;` |
|       7 | 1135 | `								PH7_MemObjRelease(&sTest);` |
|       7 | 1136 | `								PH7_ClassInstanceUnref(pThis);` |
|       7 | 1137 | `								VM_EXIT_BREAK;` |
|     156 | 1138 | `							}else if( VmMemberNextIsRmw(pNext)` |
|     101 | 1139 | `							 && VmMagicRmwEligible(pVm,pClass,pThis,&sName) ){` |
|       - | 1140 | `								/* ++/--/compound-assign on an overloaded property: php` |
|       - | 1141 | `								 * reads through __get and writes the computed value back` |
|       - | 1142 | `								 * through __set. Arm the scratch slot the modify op will` |
|       - | 1143 | `								 * mutate and finish the op here — the pre-existing path` |
|       - | 1144 | `								 * vivified a dynamic property instead, which on any` |
|       - | 1145 | `								 * ordinary accessor class is PHL's` |
|       - | 1146 | `								 * "Cannot create dynamic property" Error. */` |
|       - | 1147 | `								ph7_value sRmwVal;` |
|       - | 1148 | `								sxu32 nRmwScratch;` |
|      27 | 1149 | `								PH7_MemObjInit(pVm,&sRmwVal);` |
|      40 | 1150 | `								VmMagicRmwArm(&(*pVm),pThis,pClass,&sName,&sRmwVal,&nRmwScratch,` |
|      26 | 1151 | `									(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|      27 | 1152 | `								VmPopOperand(&pTos,1);   /* drop the property name */` |
|      27 | 1153 | `								pThis->iRef++;` |
|      27 | 1154 | `								PH7_MemObjRelease(pTos); /* collapse the object slot */` |
|      27 | 1155 | `								PH7_MemObjStore(&sRmwVal,pTos);` |
|      27 | 1156 | `								pTos->nIdx = nRmwScratch;` |
|      27 | 1157 | `								PH7_MemObjRelease(&sRmwVal);` |
|      27 | 1158 | `								PH7_ClassInstanceUnref(pThis);` |
|      27 | 1159 | `								VM_EXIT_BREAK;` |
|     130 | 1160 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|      33 | 1161 | `							 && !VmMemberNextIsWrite(pNext)` |
|      32 | 1162 | `							 && PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) ){` |
|       - | 1163 | `								/* Subscript-write base on a class with __get: php reads` |
|       - | 1164 | `								 * through the magic layer (the write lands on the temp and` |
|       - | 1165 | `								 * is lost, like php's indirect-modification case). A PLAIN` |
|       - | 1166 | `								 * store (bPlainStore — the compiler tags those` |
|       - | 1167 | `								 * PH7_MEMBER_WRITE too) is NOT this case: it falls through` |
|       - | 1168 | `								 * to dynamic creation. Leave the miss path — the read gate` |
|       - | 1169 | `								 * below dispatches __get. */` |
|     132 | 1170 | `							}else if( pThis->pClass->iFlags & PH7_CLASS_READONLY ){` |
|       - | 1171 | `								SyBlob sErrMsg;` |
|     ! 0 | 1172 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     ! 0 | 1173 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|     ! 0 | 1174 | `									&pThis->pClass->sName,&sName);` |
|     ! 0 | 1175 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     124 | 1176 | `							}else if( !VmClassAllowsDynamicProps(pVm,pThis->pClass)` |
|      82 | 1177 | `							       && !VmClassHasAttributeNamed(pThis->pClass,"AllowDynamicProperties",sizeof("AllowDynamicProperties")-1) ){` |
|       - | 1178 | `								/* php 8.2 only DEPRECATES creating a dynamic property on a` |
|       - | 1179 | `								 * class without #[AllowDynamicProperties]; PHL rejects it.` |
|       - | 1180 | `								 * stdClass / __set / declared props are unaffected. */` |
|       - | 1181 | `								SyBlob sErrMsg;` |
|      14 | 1182 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      14 | 1183 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|      10 | 1184 | `									&pThis->pClass->sName,&sName);` |
|      14 | 1185 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       9 | 1186 | `							}else{` |
|     119 | 1187 | `								PH7_VmCreateDynamicAttr(&(*pVm),pThis,sName.zString,sName.nByte,&pObjAttr);` |
|       - | 1188 | `							}` |
|       - | 1189 | `						}` |
|     517 | 1190 | `						if( pObjAttr && VmMemberNextIsRmw(pNext) ){` |
|       - | 1191 | `							/* A read-modify-write READS the property before it writes it,` |
|       - | 1192 | `							 * so php warns that the name it just created was undefined and` |
|       - | 1193 | ``							 * then goes on with null — `$o->hits++` on a fresh object is`` |
|       - | 1194 | ``							 * `Undefined property: C::$hits` and then int(1). PHL created`` |
|       - | 1195 | `							 * the slot in silence. Only the read-modify-write forms:` |
|       - | 1196 | ``							 * a subscript-write base (`$o->arr[] = 1`) and a `??=` read`` |
|       - | 1197 | `							 * nothing, and php says nothing for either. */` |
|      19 | 1198 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|       6 | 1199 | `								&pClass->sName,&sName);` |
|       6 | 1200 | `						}` |
|     256 | 1201 | `					}` |
|    3300 | 1202 | `				}` |
|  113929 | 1203 | `				if( pObjAttr == 0 && pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH` |
|    3233 | 1204 | `				 && !(PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)` |
|    2117 | 1205 | `				   && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g')) ){` |
|       - | 1206 | `					/* D1 commit 2: deferred property arg, property absent on a REACHABLE object` |
|       - | 1207 | `					 * base ($o is a real slot). Record a property step rooted at $o so OP_CALL can` |
|       - | 1208 | `					 * vivify+bind (by-ref) or read via __get / warn (by-value). A magic __get/__set` |
|       - | 1209 | `					 * class is recorded the same way; the resolve step emits php's Notice for the` |
|       - | 1210 | `					 * by-ref case and dispatches __get for by-value. */` |
|       - | 1211 | `					SyString sProp;` |
|      13 | 1212 | `					VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|      13 | 1213 | `					SyStringInitFromBuf(&sProp,sName.zString,sName.nByte);` |
|      13 | 1214 | `					if( pPath && VmDeferPathPushProp(pPath,&sProp) == SXRET_OK ){` |
|      13 | 1215 | `						VmPopOperand(&pTos,1);       /* drop the property name */` |
|      13 | 1216 | `						pThis->iRef++;` |
|      13 | 1217 | `						PH7_MemObjRelease(pTos);     /* collapse the object slot into the carrier */` |
|      13 | 1218 | `						pTos->x.pOther = pPath;` |
|      13 | 1219 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      13 | 1220 | `						pTos->nIdx = SXU32_HIGH;` |
|      13 | 1221 | `						PH7_ClassInstanceUnref(pThis);` |
|      13 | 1222 | `						VM_EXIT_BREAK;` |
|       - | 1223 | `					}` |
|     ! 0 | 1224 | `					if( pPath ){` |
|     ! 0 | 1225 | `						VmFreeDeferredPath(pPath);` |
|     ! 0 | 1226 | `					}` |
|       - | 1227 | `					/* fall through to the normal miss handling on allocation failure */` |
|     ! 0 | 1228 | `				}` |
|  113924 | 1229 | `				if( pObjAttr == 0 ){` |
|       - | 1230 | `					/* Missing property. On a plain READ, php dispatches __get($name) and the` |
|       - | 1231 | `					 * expression takes its RETURN VALUE (band A #3a — pre-fix the result was` |
|       - | 1232 | `					 * discarded and a PHL-native warn fired even when __get existed). isset/` |
|       - | 1233 | `					 * empty context consults __isset first (then __get for empty()'s value` |
|       - | 1234 | `					 * test); a plain-store write context parked a pending __set above. A` |
|       - | 1235 | `					 * self-recursive read of the same property falls back to the` |
|       - | 1236 | `					 * undefined-property path via the guard, like php's property guard. */` |
|    6470 | 1237 | `					ph7_class_method *pGetMagic = 0;` |
|    6470 | 1238 | `					if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|     284 | 1239 | `						ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|     284 | 1240 | `						if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 1241 | `							ph7_value sIssetRet;` |
|       - | 1242 | `							int bSet;` |
|     219 | 1243 | `							PH7_MemObjInit(pVm,&sIssetRet);` |
|     219 | 1244 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|     219 | 1245 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|     219 | 1246 | `							VmMagicGuardPop(pVm);` |
|     219 | 1247 | `							PH7_MemObjToBool(&sIssetRet);` |
|     219 | 1248 | `							bSet = sIssetRet.x.iVal != 0;` |
|     219 | 1249 | `							PH7_MemObjRelease(&sIssetRet);` |
|     219 | 1250 | `							if( bSet && VmMemberCtxWantsValue(pInstr->iP2) ){` |
|       - | 1251 | ``								/* empty() and `??`: __isset said set, so php goes on to`` |
|       - | 1252 | `								 * __get for the VALUE — emptiness is judged on it, and the` |
|       - | 1253 | `								 * coalesce simply IS it (a null answer then takes the` |
|       - | 1254 | `								 * default, which OP_NULLC does for free). */` |
|      90 | 1255 | `								ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       - | 1256 | `								ph7_value sEmptyVal;` |
|      90 | 1257 | `								PH7_MemObjInit(pVm,&sEmptyVal);` |
|      90 | 1258 | `								if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|      90 | 1259 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|      90 | 1260 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|      90 | 1261 | `									VmMagicGuardPop(pVm);` |
|      44 | 1262 | `								}` |
|      90 | 1263 | `								VmPopOperand(&pTos,1);` |
|      90 | 1264 | `								pThis->iRef++;` |
|      90 | 1265 | `								PH7_MemObjRelease(pTos);` |
|      90 | 1266 | `								PH7_MemObjStore(&sEmptyVal,pTos);` |
|      90 | 1267 | `								pTos->nIdx = SXU32_HIGH;` |
|      90 | 1268 | `								PH7_MemObjRelease(&sEmptyVal);` |
|      90 | 1269 | `								PH7_ClassInstanceUnref(pThis);` |
|      90 | 1270 | `								VM_EXIT_BREAK;` |
|       - | 1271 | `							}` |
|       - | 1272 | `							/* isset(): the truth of __isset IS the answer — push a non-null` |
|       - | 1273 | `							 * marker for true, NULL for false (isset only tests null-ness). */` |
|     131 | 1274 | `							VmPopOperand(&pTos,1);` |
|     131 | 1275 | `							pThis->iRef++;` |
|     131 | 1276 | `							PH7_MemObjRelease(pTos);` |
|     131 | 1277 | `							if( bSet ){` |
|      61 | 1278 | `								pTos->x.iVal = 1;` |
|      61 | 1279 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      29 | 1280 | `							}` |
|     131 | 1281 | `							pTos->nIdx = SXU32_HIGH;` |
|     131 | 1282 | `							PH7_ClassInstanceUnref(pThis);` |
|     131 | 1283 | `							VM_EXIT_BREAK;` |
|       - | 1284 | `						}` |
|      32 | 1285 | `					}` |
|    6249 | 1286 | `					if( (!VmMemberCtxIsLookup(pInstr->iP2) \|\| pInstr->iP2 == PH7_MEMBER_COALESCE)` |
|    6224 | 1287 | `					 && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|    6229 | 1288 | `					 && !VmMemberNextIsWrite(pInstr + 1) ){` |
|       - | 1289 | `						/* Plain reads AND the ??=/subscript write-base (PH7_MEMBER_WRITE,` |
|       - | 1290 | `						 * which php reads through __get); read-modify-write forms are` |
|       - | 1291 | `						 * excluded (they vivified above — approximate, recorded), and so is` |
|       - | 1292 | `						 * a destructuring target (a pure write — php never reads it).` |
|       - | 1293 | ``						 * `??` reaches here only when the class declares NO __isset — the`` |
|       - | 1294 | `						 * branch above has returned otherwise — so php's gate is absent and` |
|       - | 1295 | `						 * __get answers on its own. */` |
|    5822 | 1296 | `						pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|    2909 | 1297 | `					}` |
|    6254 | 1298 | `					if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 1299 | `						ph7_value sMagicRet;` |
|    5790 | 1300 | `						PH7_MemObjInit(pVm,&sMagicRet);` |
|    5790 | 1301 | `						VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|    5790 | 1302 | `						PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|    5790 | 1303 | `						VmMagicGuardPop(pVm);` |
|    5790 | 1304 | `						if( VmMemberFetchForWrite(pInstr) ){` |
|       - | 1305 | `							/* php's indirect-modification notice, and the separation` |
|       - | 1306 | `							 * that makes the write it describes land nowhere. Raised` |
|       - | 1307 | `							 * BEFORE the pop below: sName still aliases the NAME` |
|       - | 1308 | ``							 * operand when the name is dynamic (`$o->$k[0] = v`). */`` |
|      11 | 1309 | `							PH7_VmOverloadedPropNotice(&(*pVm),pClass,&sName,&sMagicRet);` |
|    5785 | 1310 | `						}else if( pInstr->iP2 == PH7_MEMBER_DEFPATH ){` |
|       - | 1311 | `							/* A deferred call ARGUMENT: __get has answered — php calls it` |
|       - | 1312 | `							 * where the property is WRITTEN, whatever the parameter turns` |
|       - | 1313 | `							 * out to be — and only the by-ref verdict is still pending.` |
|       - | 1314 | `							 * Carry the value with the class and name that produced it, so` |
|       - | 1315 | `							 * the notice lands at the call for a by-REFERENCE parameter and` |
|       - | 1316 | `							 * nothing is said for a by-value one, without __get running` |
|       - | 1317 | `							 * twice or a second read arriving after a later argument's side` |
|       - | 1318 | `							 * effects. */` |
|    3322 | 1319 | `							VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_PROP,` |
|    1106 | 1320 | `								pClass,&sName,&sMagicRet);` |
|    2216 | 1321 | `							if( pPre ){` |
|    2216 | 1322 | `								VmPopOperand(&pTos,1);   /* drop the property name */` |
|    2216 | 1323 | `								pThis->iRef++;` |
|    2216 | 1324 | `								PH7_MemObjRelease(pTos); /* collapse the object slot into the carrier */` |
|    2216 | 1325 | `								pTos->x.pOther = pPre;` |
|    2216 | 1326 | `								pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|    2216 | 1327 | `								pTos->nIdx = SXU32_HIGH;` |
|    2216 | 1328 | `								PH7_MemObjRelease(&sMagicRet);` |
|    2216 | 1329 | `								PH7_ClassInstanceUnref(pThis);` |
|    2216 | 1330 | `								VM_EXIT_BREAK;` |
|       - | 1331 | `							}` |
|     ! 0 | 1332 | `						}` |
|       - | 1333 | `						/* Pop the attribute name, replace the object slot with the magic` |
|       - | 1334 | `						 * result (a temp, not an lvalue — nIdx stays constant). A throw` |
|       - | 1335 | `						 * from __get parked at the boundary; the fetch-point router lands` |
|       - | 1336 | `						 * it and abandons this slot. */` |
|    3578 | 1337 | `						VmPopOperand(&pTos,1);` |
|    3578 | 1338 | `						pThis->iRef++;` |
|    3578 | 1339 | `						PH7_MemObjRelease(pTos);` |
|    3578 | 1340 | `						PH7_MemObjStore(&sMagicRet,pTos);` |
|    3578 | 1341 | `						pTos->nIdx = SXU32_HIGH;` |
|    3578 | 1342 | `						PH7_MemObjRelease(&sMagicRet);` |
|    3578 | 1343 | `						PH7_ClassInstanceUnref(pThis);` |
|    3578 | 1344 | `						VM_EXIT_BREAK;` |
|       - | 1345 | `					}` |
|       - | 1346 | `					/* No __get (or guard held): load null. In isset()/empty() context (iP2 3/4)` |
|       - | 1347 | `					 * PHP returns false/true SILENTLY, so suppress the read-miss warning there` |
|       - | 1348 | `					 * (mirrors the array LOAD_IDX iP2=4/6 suppression); likewise when a` |
|       - | 1349 | `					 * pending __set was parked above (the following OP_STORE dispatches it —` |
|       - | 1350 | `					 * nothing is "undefined" about that write) or a throw is parked on the` |
|       - | 1351 | `					 * boundary rail (e.g. the readonly-class dynamic-property Error — the` |
|       - | 1352 | `					 * fetch-point router lands it right after this op). */` |
|     464 | 1353 | `					if( !VmMemberCtxIsLookup(pInstr->iP2) && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|     406 | 1354 | `					 && pVm->pMagicSetThis == 0` |
|     224 | 1355 | `					 && pVm->nBoundaryRc == 0 ){` |
|       - | 1356 | `						/* A property missing from the INSTANCE may still be DECLARED by the` |
|       - | 1357 | ``						 * class — that is what `unset($o->p)` leaves behind, and php keeps`` |
|       - | 1358 | `						 * answering for the declaration rather than calling the name` |
|       - | 1359 | `						 * undefined: the visibility screen still applies, and a TYPED slot` |
|       - | 1360 | `						 * is back to uninitialized (the same Error a never-written one` |
|       - | 1361 | `						 * raises). Only a name the class does not declare at all is the` |
|       - | 1362 | `						 * "Undefined property" warning. A destructuring target is silent` |
|       - | 1363 | `						 * either way: php created the property above or dispatched __set. */` |
|      40 | 1364 | `						ph7_class_attr *pDeclAttr = PH7_ClassExtractAttribute(pClass,` |
|      12 | 1365 | `							SyStringData(&sName),SyStringLength(&sName));` |
|       - | 1366 | `						/* A static property, a class constant and a native engine slot are` |
|       - | 1367 | `						 * not instance properties; a dynamic one is gone for good once it` |
|       - | 1368 | `						 * is unset, since nothing declares it. */` |
|      24 | 1369 | `						if( pDeclAttr` |
|      23 | 1370 | `						 && (pDeclAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|       - | 1371 | `						                          \|PH7_CLASS_ATTR_HIDDEN\|PH7_CLASS_ATTR_DYNAMIC)) ){` |
|       6 | 1372 | `							pDeclAttr = 0;` |
|       2 | 1373 | `						}` |
|      24 | 1374 | `						if( pDeclAttr` |
|      21 | 1375 | `						 && !PH7_VmClassMemberAccess(&(*pVm),pClass,&pDeclAttr->sName,` |
|       7 | 1376 | `							pDeclAttr->iProtection,FALSE) ){` |
|       - | 1377 | `							SyBlob sErrMsg;` |
|       7 | 1378 | `							const char *zVis = pDeclAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       2 | 1379 | `								? "private" : "protected";` |
|       5 | 1380 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       5 | 1381 | `							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",` |
|       2 | 1382 | `								zVis,&pClass->sName,&sName);` |
|       5 | 1383 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",` |
|       - | 1384 | `								sizeof("Error")-1,&sErrMsg));` |
|      26 | 1385 | `						}else if( pDeclAttr && (pDeclAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       7 | 1386 | `							VmBoundaryPark(&(*pVm),` |
|       2 | 1387 | `								VmThrowUninitializedPropertyError(&(*pVm),pClass,pDeclAttr));` |
|       3 | 1388 | `						}else{` |
|      28 | 1389 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|       8 | 1390 | `								&pClass->sName,&sName);` |
|       - | 1391 | `						}` |
|      12 | 1392 | `					}` |
|     232 | 1393 | `				}` |
|  107923 | 1394 | `				VmPopOperand(&pTos,1);` |
|       - | 1395 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|       - | 1396 | `				 * This is due to the following case:` |
|       - | 1397 | `				 *     (new TestClass())->foo;` |
|       - | 1398 | `				 */` |
|  107923 | 1399 | `				pThis->iRef++;` |
|  107923 | 1400 | `				PH7_MemObjRelease(pTos);` |
|  107923 | 1401 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|  107923 | 1402 | `				if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - | 1403 | ``					/* `$o->p =& $x`: stash the resolved instance property slot for the`` |
|       - | 1404 | `					 * following member-marked OP_STORE_REF, which rebinds it to alias the` |
|       - | 1405 | `					 * source variable. Do NOT run the read/hook/magic/uninit machinery` |
|       - | 1406 | `					 * below — a reference bind neither reads the value nor triggers` |
|       - | 1407 | `					 * get/set hooks or an uninitialized-typed Error. pThis stays retained` |
|       - | 1408 | `					 * (the iRef++ above); OP_STORE_REF releases it. */` |
|      35 | 1409 | `					if( pObjAttr && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) ){` |
|       - | 1410 | ``						/* `$i->f =& $x`: php REFUSES to make a handler-backed property`` |
|       - | 1411 | `						 * the target of a reference — there is no slot to rebind, and` |
|       - | 1412 | `						 * every write through the alias would skip the conversion the` |
|       - | 1413 | `						 * handler is there to do. PHL rebound the slot instead, so` |
|       - | 1414 | ``						 * `$x = 2.5` afterwards wrote 2.5 microseconds-free into the`` |
|       - | 1415 | `						 * interval. */` |
|       - | 1416 | `						SyBlob sErrMsg;` |
|       3 | 1417 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 1418 | `						SyBlobAppend(&sErrMsg,"Cannot assign by reference to overloaded object",` |
|       - | 1419 | `							sizeof("Cannot assign by reference to overloaded object")-1);` |
|       3 | 1420 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       3 | 1421 | `						pVm->pRefTargetAttr = 0;` |
|       3 | 1422 | `						pVm->pRefTargetThis = 0;` |
|       3 | 1423 | `						pVm->pRefTargetStaticAttr = 0;` |
|       3 | 1424 | `						PH7_ClassInstanceUnref(pThis);` |
|      33 | 1425 | `					}else if( pObjAttr ){` |
|      32 | 1426 | `						pVm->pRefTargetAttr = pObjAttr;` |
|      32 | 1427 | `						pVm->pRefTargetThis = pThis;` |
|      32 | 1428 | `						pVm->pRefTargetStaticAttr = 0;` |
|      32 | 1429 | `						pTos->nIdx = pObjAttr->nIdx;` |
|      17 | 1430 | `					}else{` |
|       - | 1431 | `						/* Missing/inaccessible target: leave nothing stashed; OP_STORE_REF` |
|       - | 1432 | `						 * no-ops. Balance the retain. */` |
|     ! 0 | 1433 | `						pVm->pRefTargetAttr = 0;` |
|     ! 0 | 1434 | `						pVm->pRefTargetThis = 0;` |
|     ! 0 | 1435 | `						pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 1436 | `						PH7_ClassInstanceUnref(pThis);` |
|       - | 1437 | `					}` |
|      34 | 1438 | `					VM_EXIT_BREAK;` |
|       - | 1439 | `				}` |
|  107891 | 1440 | `				if( pObjAttr ){` |
|  107427 | 1441 | `					ph7_value *pValue = 0; /* cc warning */` |
|       - | 1442 | `					/* Check attribute access */` |
|  107427 | 1443 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|  107392 | 1444 | `						if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET))` |
|   53878 | 1445 | `						 && !VmHookGuardHeld(pVm,(void *)pThis,&sName) ){` |
|       - | 1446 | `							/* PHP 8.4 property hooks: route reads, writes, and the` |
|       - | 1447 | `							 * read-modify-write forms through the synthesized hook` |
|       - | 1448 | `							 * methods. A held guard (either kind) means we are INSIDE` |
|       - | 1449 | `							 * one of this property's own hook bodies — fall through to` |
|       - | 1450 | `` 							 * the raw backing slot for BOTH directions (php: `$this->x` `` |
|       - | 1451 | `							 * within any of x's hooks addresses the backing store). */` |
|     221 | 1452 | `							VmInstr *pNextH = pInstr + 1;` |
|     221 | 1453 | `							int bPlainStore = (pNextH->iOp == PH7_OP_STORE && pNextH->iP2 != 0);` |
|     377 | 1454 | `							int bCoalesceW = pInstr->iP2 == PH7_MEMBER_WRITE` |
|     216 | 1455 | `								&& pNextH->iOp == PH7_OP_NULLC_JMP;` |
|       - | 1456 | `							/* ++/--/compound-assign: the modify op FOLLOWS the member` |
|       - | 1457 | `							 * directly (the compiler tags compound-assign members` |
|       - | 1458 | `							 * PH7_MEMBER_WRITE too, so classify by the next op, not iP2;` |
|       - | 1459 | `							 * the !bPlainStore term is load-bearing — VmMemberNextIsWrite` |
|       - | 1460 | `							 * returns 1 for a member OP_STORE too) */` |
|     221 | 1461 | `							int bRmwNext = !bPlainStore && VmMemberNextIsWrite(pNextH);` |
|     258 | 1462 | `							int bSubscriptW = !bPlainStore && !bCoalesceW && !bRmwNext` |
|     298 | 1463 | `								&& pInstr->iP2 == PH7_MEMBER_WRITE;` |
|     221 | 1464 | `							if( bPlainStore ){` |
|       - | 1465 | `								/* Arm the pending hook-set for the following OP_STORE — it` |
|       - | 1466 | `								 * dispatches the set hook, or throws the read-only Error` |
|       - | 1467 | `								 * when the property only has a get hook. (The scalar` |
|       - | 1468 | `								 * transient is safe here: its window is exactly one` |
|       - | 1469 | `								 * instruction, MEMBER -> STORE, nothing runs in between.) */` |
|      56 | 1470 | `								pThis->iRef++;` |
|      56 | 1471 | `								pVm->pHookSetThis = pThis;` |
|      56 | 1472 | `								pVm->pHookSetAttr = pObjAttr->pAttr;` |
|      56 | 1473 | `								pVm->nHookSetIdx = pObjAttr->nIdx;` |
|      56 | 1474 | `								pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|      56 | 1475 | `								PH7_ClassInstanceUnref(pThis);` |
|      56 | 1476 | `								VM_EXIT_BREAK;` |
|       - | 1477 | `							}` |
|     168 | 1478 | `							if( bSubscriptW ){` |
|       - | 1479 | `								/* Subscript write base ($o->m[] = v, $o->m[$k] = v):` |
|       - | 1480 | `								 * php's catchable Error, with or without a set hook` |
|       - | 1481 | ``								 * (only a by-ref `&get` hook would allow it — a loud`` |
|       - | 1482 | `								 * compile error in PHL). The op completes benignly with` |
|       - | 1483 | `								 * a null temp; the fetch-point router lands the throw. */` |
|       - | 1484 | `								SyBlob sErrMsg;` |
|       7 | 1485 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       7 | 1486 | `								SyBlobFormat(&sErrMsg,"Indirect modification of %z::$%z is not allowed",` |
|       6 | 1487 | `									&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       7 | 1488 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       7 | 1489 | `								PH7_ClassInstanceUnref(pThis);` |
|       7 | 1490 | `								VM_EXIT_BREAK;` |
|       - | 1491 | `							}` |
|     158 | 1492 | `							if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|      83 | 1493 | `							 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1494 | `								/* VIRTUAL set-only property: there is no backing store to` |
|       - | 1495 | `								 * fall back to, so EVERY read context — plain read,` |
|       - | 1496 | `								 * isset()/empty() (php throws there too, probe-verified),` |
|       - | 1497 | `								 * the ??= test, an RMW read — is php's catchable` |
|       - | 1498 | `								 * write-only Error. */` |
|       - | 1499 | `								SyBlob sErrMsg;` |
|       9 | 1500 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       9 | 1501 | `								SyBlobFormat(&sErrMsg,"Property %z::$%z is write-only",` |
|       8 | 1502 | `									&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       9 | 1503 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       9 | 1504 | `								PH7_ClassInstanceUnref(pThis);` |
|       9 | 1505 | `								VM_EXIT_BREAK;` |
|       - | 1506 | `							}` |
|     154 | 1507 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1508 | ``								/* isset()/empty()/`??` on a hooked property all call the get`` |
|       - | 1509 | `								 * hook (php): isset is "get() !== null", empty tests the` |
|       - | 1510 | ``								 * value, and `??` IS the value. */`` |
|       - | 1511 | `								ph7_value sHookRet;` |
|      16 | 1512 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|      16 | 1513 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|      16 | 1514 | `									if( !VmMemberCtxWantsValue(pInstr->iP2) ){` |
|       - | 1515 | `										/* isset(): the trailing builtin tests NULL-ness of what` |
|       - | 1516 | `										 * it is handed, so "not set" has to BE null. Storing` |
|       - | 1517 | ``										 * bool(FALSE) here made `isset($o->p)` answer TRUE for`` |
|       - | 1518 | `										 * a get hook returning null — the same non-null marker` |
|       - | 1519 | `										 * convention the __isset path uses. */` |
|       8 | 1520 | `										if( (sHookRet.iFlags & MEMOBJ_NULL) == 0 ){` |
|       6 | 1521 | `											pTos->x.iVal = 1;` |
|       6 | 1522 | `											MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       4 | 1523 | `										}else{` |
|       3 | 1524 | `											MemObjSetType(pTos,MEMOBJ_NULL);` |
|       - | 1525 | `										}` |
|       5 | 1526 | `									}else{` |
|       - | 1527 | ``										/* empty() judges the value; `??` IS the value. */`` |
|      10 | 1528 | `										PH7_MemObjStore(&sHookRet,pTos);` |
|       - | 1529 | `									}` |
|      16 | 1530 | `									pTos->nIdx = SXU32_HIGH;` |
|      16 | 1531 | `									PH7_MemObjRelease(&sHookRet);` |
|      16 | 1532 | `									PH7_ClassInstanceUnref(pThis);` |
|      16 | 1533 | `									VM_EXIT_BREAK;` |
|       - | 1534 | `								}` |
|     ! 0 | 1535 | `								PH7_MemObjRelease(&sHookRet);` |
|     ! 0 | 1536 | `							}` |
|     140 | 1537 | `							if( !bCoalesceW && !bRmwNext && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1538 | `								/* Read: dispatch __phl_hook_get_NAME (inheritance-aware) */` |
|       - | 1539 | `								ph7_value sHookRet;` |
|     106 | 1540 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|     106 | 1541 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|      92 | 1542 | `									if( pInstr->iP2 == PH7_MEMBER_DEFPATH ){` |
|       - | 1543 | `										/* A deferred call ARGUMENT of a HOOKED property. The` |
|       - | 1544 | `										 * get hook has answered, as php's does; what a` |
|       - | 1545 | `										 * by-REFERENCE parameter then gets is not a notice` |
|       - | 1546 | `										 * but php's refusal — a hook has no slot to alias` |
|       - | 1547 | ``										 * and `&get` does not exist here — while a by-VALUE`` |
|       - | 1548 | `										 * one simply takes the hook's value. */` |
|      42 | 1549 | `										VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),` |
|      26 | 1550 | `											VM_OVER_HOOK,pThis->pClass,&pObjAttr->pAttr->sName,&sHookRet);` |
|      29 | 1551 | `										if( pPre ){` |
|      29 | 1552 | `											PH7_MemObjRelease(pTos);` |
|      29 | 1553 | `											pTos->x.pOther = pPre;` |
|      29 | 1554 | `											pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      29 | 1555 | `											pTos->nIdx = SXU32_HIGH;` |
|      29 | 1556 | `											PH7_MemObjRelease(&sHookRet);` |
|      29 | 1557 | `											PH7_ClassInstanceUnref(pThis);` |
|      60 | 1558 | `											VM_EXIT_BREAK;` |
|       - | 1559 | `										}` |
|     ! 0 | 1560 | `									}` |
|      65 | 1561 | `									PH7_MemObjStore(&sHookRet,pTos);` |
|      65 | 1562 | `									pTos->nIdx = SXU32_HIGH;` |
|      65 | 1563 | `									PH7_MemObjRelease(&sHookRet);` |
|      65 | 1564 | `									PH7_ClassInstanceUnref(pThis);` |
|      65 | 1565 | `									VM_EXIT_BREAK;` |
|       - | 1566 | `								}` |
|      15 | 1567 | `								PH7_MemObjRelease(&sHookRet);` |
|       7 | 1568 | `							}` |
|      49 | 1569 | `							if( bCoalesceW \|\| bRmwNext ){` |
|       - | 1570 | ``								/* `$o->x ??= v` (bCoalesceW) and the read-modify-write`` |
|       - | 1571 | ``								 * forms `$o->x++`, `$o->x .= v`, … (bRmwNext): php reads`` |
|       - | 1572 | `								 * through the get hook (raw backing when the property is` |
|       - | 1573 | `								 * set-only) and writes through the set hook.` |
|       - | 1574 | `								 *   - ??=: the test value goes on the stack and a` |
|       - | 1575 | `								 *     COAL_HOOK entry is pushed for the OP_NULLC_STORE` |
|       - | 1576 | `								 *     at the jump target; the fetch-point sweep drops it` |
|       - | 1577 | `								 *     when the short-circuit jump skips the assign or a` |
|       - | 1578 | `								 *     throw abandons the RHS (the entry is NOT a scalar` |
|       - | 1579 | `								 *     transient — nested stores/coalesces in the RHS` |
|       - | 1580 | `								 *     stack their own entries above it).` |
|       - | 1581 | `								 *   - RMW: the value goes into a fresh SCRATCH slot the` |
|       - | 1582 | `								 *     modify op mutates in place; the entry makes the` |
|       - | 1583 | `								 *     op's tail dispatch the set side with the computed` |
|       - | 1584 | `								 *     value (PH7_HOOK_RMW_WRITEBACK). */` |
|       - | 1585 | `								ph7_value sCur;` |
|       - | 1586 | `								sxi32 rcCur;` |
|      35 | 1587 | `								PH7_MemObjInit(pVm,&sCur);` |
|      35 | 1588 | `								rcCur = PH7_VmHookGetAttrValue(pThis,pObjAttr,&sCur);` |
|      35 | 1589 | `								if( rcCur == SXERR_NOTFOUND ){` |
|       - | 1590 | `									/* set-only hook (or guard edge): the read side is the` |
|       - | 1591 | `									 * raw backing store (php) */` |
|       3 | 1592 | `									ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|       3 | 1593 | `									if( pBack ){` |
|       3 | 1594 | `										PH7_MemObjStore(pBack,&sCur);` |
|       2 | 1595 | `									}` |
|      34 | 1596 | `								}else if( pVm->nBoundaryRc != 0 ){` |
|       - | 1597 | `									/* the get hook threw: leave the null temp; the` |
|       - | 1598 | `									 * fetch-point router lands the parked throw —` |
|       - | 1599 | `									 * nothing is armed. */` |
|     ! 0 | 1600 | `									PH7_MemObjRelease(&sCur);` |
|     ! 0 | 1601 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1602 | `									VM_EXIT_BREAK;` |
|       - | 1603 | `								}` |
|      35 | 1604 | `								if( bCoalesceW ){` |
|       - | 1605 | `									VmHookRmw sPend;` |
|      15 | 1606 | `									PH7_MemObjStore(&sCur,pTos);` |
|      15 | 1607 | `									pTos->nIdx = SXU32_HIGH;` |
|      15 | 1608 | `									PH7_MemObjRelease(&sCur);` |
|      15 | 1609 | `									sPend.iKind = VM_HOOK_PEND_COAL_HOOK;` |
|      15 | 1610 | `									sPend.pThis = pThis;` |
|      15 | 1611 | `									sPend.pAttr = pObjAttr->pAttr;` |
|      15 | 1612 | `									sPend.nBackIdx = pObjAttr->nIdx;` |
|      15 | 1613 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|      15 | 1614 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|      15 | 1615 | `									sPend.pOwnerStack = (void *)pStack;` |
|      15 | 1616 | `									sPend.pInstrs = (void *)aInstr;` |
|      15 | 1617 | `									sPend.nJmpPc = (sxu32)(pc + 1);            /* the OP_NULLC_JMP */` |
|      15 | 1618 | `									sPend.nPc = (sxu32)(pNextH->iP2 - 1);      /* the OP_NULLC_STORE */` |
|      15 | 1619 | `									pThis->iRef++;` |
|      15 | 1620 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|      15 | 1621 | `									PH7_ClassInstanceUnref(pThis);` |
|      15 | 1622 | `									VM_EXIT_BREAK;` |
|       - | 1623 | `								}` |
|       - | 1624 | `								{` |
|      21 | 1625 | `									ph7_value *pScr = PH7_ReserveMemObj(&(*pVm));` |
|      21 | 1626 | `									if( pScr ){` |
|       - | 1627 | `										VmHookRmw sRmw;` |
|      21 | 1628 | `										PH7_MemObjStore(&sCur,pScr);` |
|      21 | 1629 | `										PH7_MemObjStore(&sCur,pTos);` |
|      21 | 1630 | `										pTos->nIdx = pScr->nIdx;` |
|      21 | 1631 | `										sRmw.iKind = VM_HOOK_PEND_RMW;` |
|      21 | 1632 | `										sRmw.pThis = pThis;` |
|      21 | 1633 | `										sRmw.pAttr = pObjAttr->pAttr;` |
|      21 | 1634 | `										sRmw.nBackIdx = pObjAttr->nIdx;` |
|      21 | 1635 | `										sRmw.nScratchIdx = pScr->nIdx;` |
|      21 | 1636 | `										SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      21 | 1637 | `										sRmw.pOwnerStack = (void *)pStack;` |
|      21 | 1638 | `										sRmw.pInstrs = (void *)aInstr;` |
|      21 | 1639 | `										sRmw.nJmpPc = (sxu32)(pc + 1);  /* the modify op ... */` |
|      21 | 1640 | `										sRmw.nPc = (sxu32)(pc + 1);     /* ... is the whole window */` |
|      21 | 1641 | `										pThis->iRef++;` |
|      21 | 1642 | `										SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      10 | 1643 | `									}` |
|       - | 1644 | `									/* OOM: pScr == 0 — leave the null temp (loud allocator` |
|       - | 1645 | `									 * diagnostics already fired) */` |
|      21 | 1646 | `									PH7_MemObjRelease(&sCur);` |
|      21 | 1647 | `									PH7_ClassInstanceUnref(pThis);` |
|      21 | 1648 | `									VM_EXIT_BREAK;` |
|       - | 1649 | `								}` |
|       - | 1650 | `							}` |
|       7 | 1651 | `						}` |
|       - | 1652 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|       - | 1653 | `						 * We can only raise it on a real read, not when the slot is the` |
|       - | 1654 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|       - | 1655 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|       - | 1656 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|  107190 | 1657 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|   53759 | 1658 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|     323 | 1659 | `							VmInstr *pNext = pInstr + 1;` |
|     323 | 1660 | `							int bIsLhs = 0;` |
|     323 | 1661 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|     299 | 1662 | `								bIsLhs = 1;` |
|     147 | 1663 | `							}` |
|     323 | 1664 | `							if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 1665 | `								/* Destructuring target: the OP_LOAD_LIST store (typed-` |
|       - | 1666 | `								 * enforced) initializes it — never a read (php). */` |
|       7 | 1667 | `								bIsLhs = 1;` |
|       3 | 1668 | `							}` |
|       - | 1669 | ``							/* isset()/empty()/`??` read an uninitialized typed property`` |
|       - | 1670 | `							 * as "not set" — a silent miss, NOT the Error a plain read` |
|       - | 1671 | `							 * raises (php). The compiler tags such an access iP2 =` |
|       - | 1672 | `							 * ISSET/EMPTY; treat it like the assignment-LHS case and fall` |
|       - | 1673 | `							 * through to load the slot's NULL. */` |
|     323 | 1674 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       9 | 1675 | `								bIsLhs = 1;` |
|       4 | 1676 | `							}` |
|     323 | 1677 | `							if( !bIsLhs ){` |
|      14 | 1678 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|      14 | 1679 | `								PH7_ClassInstanceUnref(pThis);` |
|      14 | 1680 | `								if( rcU == PH7_ABORT ){` |
|       3 | 1681 | `									VM_EXIT_ABORT;` |
|       - | 1682 | `								}` |
|       - | 1683 | `								{` |
|       - | 1684 | `									sxi32 iRp;` |
|      11 | 1685 | `									if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      10 | 1686 | `										PH7_RESUME_DRAIN()` |
|       6 | 1687 | `										pc = iRp;` |
|       6 | 1688 | `										VM_EXIT_BREAK;` |
|       - | 1689 | `									}` |
|       - | 1690 | `								}` |
|       5 | 1691 | `								VM_EXIT_EXCEPTION;` |
|       - | 1692 | `							}` |
|     154 | 1693 | `						}` |
|       - | 1694 | `						/* Load attribute */` |
|  107185 | 1695 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|  107185 | 1696 | `						if( pValue ){` |
|  107185 | 1697 | `							if( pThis->iRef < 2 ){` |
|       - | 1698 | `								/* Perform a store operation,rather than a load operation since` |
|       - | 1699 | `								 * the class instance '$this' will be deleted shortly.` |
|       - | 1700 | `								 */` |
|     202 | 1701 | `								PH7_MemObjStore(pValue,pTos);` |
|     102 | 1702 | `							}else{` |
|       - | 1703 | `								/* Simple load */` |
|  106985 | 1704 | `								PH7_MemObjLoad(pValue,pTos);` |
|       - | 1705 | `							}` |
|  107185 | 1706 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|  107185 | 1707 | `								if( pThis->iRef > 1 ){` |
|       - | 1708 | `									/* Load attribute index */` |
|  106985 | 1709 | `									pTos->nIdx = pObjAttr->nIdx;` |
|   53490 | 1710 | `								}` |
|   53590 | 1711 | `							}` |
|  107180 | 1712 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET)` |
|   54154 | 1713 | `							 && !VmMemberNativeSetKeepsSlot(pInstr) ){` |
|     508 | 1714 | `								pTos->nIdx = SXU32_HIGH;` |
|     508 | 1715 | `								pTos->iFlags \|= MEMOBJ_AUX_NATIVEPROP;` |
|     253 | 1716 | `							}` |
|   53590 | 1717 | `						}` |
|  107185 | 1718 | `						if( pInstr->iP2 == PH7_MEMBER_ISSET ){` |
|       - | 1719 | `							/* isset() tests null-ness and nothing else, so reduce the loaded` |
|       - | 1720 | `							 * value to the same non-null marker the __isset and get-hook` |
|       - | 1721 | `							 * paths push. A property read off a TEMPORARY receiver` |
|       - | 1722 | ``							 * (`isset(f()->p)`, `isset((new C)->p)`, and now every`` |
|       - | 1723 | `							 * intermediate link of an accessor chain) leaves no variable` |
|       - | 1724 | `							 * index behind, and the trailing builtin read that as a` |
|       - | 1725 | `							 * CONSTANT and warned -- a diagnostic php has no equivalent of,` |
|       - | 1726 | `							 * its isset() being a language construct rather than a call. */` |
|      59 | 1727 | `							int bSet = (pTos->iFlags & MEMOBJ_NULL) == 0;` |
|      59 | 1728 | `							PH7_MemObjRelease(pTos);` |
|      59 | 1729 | `							if( bSet ){` |
|      45 | 1730 | `								pTos->x.iVal = 1;` |
|      45 | 1731 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      22 | 1732 | `							}` |
|      59 | 1733 | `							pTos->nIdx = SXU32_HIGH;` |
|      29 | 1734 | `						}` |
|   53595 | 1735 | `					}else{` |
|       - | 1736 | `						/* Inaccessible (private/protected) from this scope. php consults` |
|       - | 1737 | `						 * __get here exactly like a missing property (band A #3a) before` |
|       - | 1738 | `						 * erroring; the guard keeps a self-recursive read from looping. */` |
|      33 | 1739 | `						ph7_class_method *pGetMagic = 0;` |
|       - | 1740 | ``						/* A WRITE-context fetch (the base of `$o->p[k] = v`) reads through`` |
|       - | 1741 | `						 * __get in php too — the write then lands on the value it answered` |
|       - | 1742 | `						 * and is lost, with php's indirect-modification notice. PHL refused` |
|       - | 1743 | ``						 * the whole statement with `Cannot access private property` instead,`` |
|       - | 1744 | ``						 * a fatal on a program php runs. A plain store and a `??=` keep`` |
|       - | 1745 | `						 * their own paths below. */` |
|      30 | 1746 | `						if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      36 | 1747 | `						 && (VmMemberFetchForWrite(pInstr)` |
|      20 | 1748 | `						  \|\| (pInstr->iP2 != PH7_MEMBER_WRITE && !VmMemberNextIsWrite(pInstr + 1))) ){` |
|      21 | 1749 | `							pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       9 | 1750 | `						}` |
|       - | 1751 | `						/* ++/--/compound-assign on a DECLARED but inaccessible property:` |
|       - | 1752 | `						 * php's accessors answer for it exactly as they do for a missing` |
|       - | 1753 | `						 * one (__get, modify, __set), where PHL raised` |
|       - | 1754 | ``						 * `Cannot access private property C::$n`. A plain store keeps its`` |
|       - | 1755 | `						 * own __set park below — it is a write, but not a read-modify-write. */` |
|      30 | 1756 | `						if( VmMemberNextIsRmw(pInstr + 1)` |
|      19 | 1757 | `						 && VmMagicRmwEligible(pVm,pClass,pThis,&sName) ){` |
|       - | 1758 | `							/* The name was already popped and pTos released above, so` |
|       - | 1759 | `							 * sName is the instruction's own literal here. */` |
|       - | 1760 | `							ph7_value sRmwVal;` |
|       - | 1761 | `							sxu32 nRmwScratch;` |
|       3 | 1762 | `							PH7_MemObjInit(pVm,&sRmwVal);` |
|       4 | 1763 | `							VmMagicRmwArm(&(*pVm),pThis,pClass,&sName,&sRmwVal,&nRmwScratch,` |
|       2 | 1764 | `								(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|       3 | 1765 | `							PH7_MemObjStore(&sRmwVal,pTos);` |
|       3 | 1766 | `							pTos->nIdx = nRmwScratch;` |
|       3 | 1767 | `							PH7_MemObjRelease(&sRmwVal);` |
|       3 | 1768 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 1769 | `							VM_EXIT_BREAK;` |
|       - | 1770 | `						}` |
|      31 | 1771 | `						if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 1772 | `							ph7_value sMagicRet;` |
|       9 | 1773 | `							PH7_MemObjInit(pVm,&sMagicRet);` |
|       9 | 1774 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       9 | 1775 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|       9 | 1776 | `							VmMagicGuardPop(pVm);` |
|       9 | 1777 | `							if( VmMemberFetchForWrite(pInstr) ){` |
|       5 | 1778 | `								PH7_VmOverloadedPropNotice(&(*pVm),pClass,&sName,&sMagicRet);` |
|       2 | 1779 | `							}` |
|       - | 1780 | `							/* The name was already popped and pTos released above; just` |
|       - | 1781 | `							 * take the magic result as the expression value. */` |
|       9 | 1782 | `							PH7_MemObjStore(&sMagicRet,pTos);` |
|       9 | 1783 | `							pTos->nIdx = SXU32_HIGH;` |
|       9 | 1784 | `							PH7_MemObjRelease(&sMagicRet);` |
|       9 | 1785 | `							PH7_ClassInstanceUnref(pThis);` |
|       9 | 1786 | `							VM_EXIT_BREAK;` |
|       - | 1787 | `						}` |
|      23 | 1788 | `						if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1789 | `							/* isset/empty on an inaccessible property: php consults __isset` |
|       - | 1790 | `							 * (band A #3b), and is silently false without it — never an` |
|       - | 1791 | `							 * Error (pre-fix PHL fataled here). */` |
|       9 | 1792 | `							ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|       9 | 1793 | `							int bSet = 0;` |
|       9 | 1794 | `							if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 1795 | `								ph7_value sIssetRet;` |
|     ! 0 | 1796 | `								PH7_MemObjInit(pVm,&sIssetRet);` |
|     ! 0 | 1797 | `								VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|     ! 0 | 1798 | `								PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|     ! 0 | 1799 | `								VmMagicGuardPop(pVm);` |
|     ! 0 | 1800 | `								PH7_MemObjToBool(&sIssetRet);` |
|     ! 0 | 1801 | `								bSet = sIssetRet.x.iVal != 0;` |
|     ! 0 | 1802 | `								PH7_MemObjRelease(&sIssetRet);` |
|     ! 0 | 1803 | `							}` |
|       9 | 1804 | `							if( pInstr->iP2 == PH7_MEMBER_COALESCE && pIssetMagic == 0 ){` |
|       - | 1805 | ``								/* `??` with no __isset to gate it: php reads straight through`` |
|       - | 1806 | `								 * __get, exactly as it does for a MISSING property. The` |
|       - | 1807 | `								 * __get dispatch just above this block is gated on a` |
|       - | 1808 | `								 * non-lookup context, so answer here. */` |
|       5 | 1809 | `								ph7_class_method *pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       5 | 1810 | `								if( pCoalGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 1811 | `									ph7_value sCoalRet;` |
|     ! 0 | 1812 | `									PH7_MemObjInit(pVm,&sCoalRet);` |
|     ! 0 | 1813 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     ! 0 | 1814 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sCoalRet);` |
|     ! 0 | 1815 | `									VmMagicGuardPop(pVm);` |
|     ! 0 | 1816 | `									PH7_MemObjStore(&sCoalRet,pTos);` |
|     ! 0 | 1817 | `									pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1818 | `									PH7_MemObjRelease(&sCoalRet);` |
|     ! 0 | 1819 | `								}` |
|       5 | 1820 | `								PH7_ClassInstanceUnref(pThis);` |
|       5 | 1821 | `								VM_EXIT_BREAK;` |
|       - | 1822 | `							}` |
|       5 | 1823 | `							if( bSet ){` |
|     ! 0 | 1824 | `								if( VmMemberCtxWantsValue(pInstr->iP2) ){` |
|     ! 0 | 1825 | `									ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       - | 1826 | `									ph7_value sEmptyVal;` |
|     ! 0 | 1827 | `									PH7_MemObjInit(pVm,&sEmptyVal);` |
|     ! 0 | 1828 | `									if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|     ! 0 | 1829 | `										VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     ! 0 | 1830 | `										PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|     ! 0 | 1831 | `										VmMagicGuardPop(pVm);` |
|     ! 0 | 1832 | `									}` |
|     ! 0 | 1833 | `									PH7_MemObjStore(&sEmptyVal,pTos);` |
|     ! 0 | 1834 | `									pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1835 | `									PH7_MemObjRelease(&sEmptyVal);` |
|     ! 0 | 1836 | `								}else{` |
|     ! 0 | 1837 | `									pTos->x.iVal = 1;` |
|     ! 0 | 1838 | `									MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     ! 0 | 1839 | `									pTos->nIdx = SXU32_HIGH;` |
|       - | 1840 | `								}` |
|     ! 0 | 1841 | `							}` |
|       5 | 1842 | `							PH7_ClassInstanceUnref(pThis);` |
|       5 | 1843 | `							VM_EXIT_BREAK;` |
|       - | 1844 | `						}` |
|       - | 1845 | `						{` |
|       - | 1846 | `							/* A plain store to an inaccessible property dispatches __set` |
|       - | 1847 | `							 * (band A #3b): park the receiver+name for the following` |
|       - | 1848 | `							 * OP_STORE, exactly like the missing-property case. */` |
|      15 | 1849 | `							VmInstr *pNextW = pInstr + 1;` |
|      15 | 1850 | `							if( pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0 ){` |
|       3 | 1851 | `								ph7_class_method *pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|       3 | 1852 | `								if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){` |
|       3 | 1853 | `									pThis->iRef++;` |
|       3 | 1854 | `									pVm->pMagicSetThis = pThis;` |
|       3 | 1855 | `									SyBlobReset(&pVm->sMagicSetName);` |
|       3 | 1856 | `									SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       3 | 1857 | `									pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|       3 | 1858 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 1859 | `									VM_EXIT_BREAK;` |
|       - | 1860 | `								}` |
|     ! 0 | 1861 | `							}` |
|       - | 1862 | `						}` |
|      10 | 1863 | `						if( ((pInstr + 1)->iOp == PH7_OP_NULLC \|\| (pInstr + 1)->iOp == PH7_OP_NULLC_JMP)` |
|       7 | 1864 | `						 && pInstr->iP2 != PH7_MEMBER_WRITE ){` |
|       - | 1865 | `` 							/* `$o->priv ?? default` (OP_NULLC; NULLC_JMP is the `??=` `` |
|       - | 1866 | ``							 * pre-test): php treats `??` as an isset-style lookup — an`` |
|       - | 1867 | `							 * inaccessible property yields null SILENTLY (the` |
|       - | 1868 | `							 * __isset/__get consults already ran above), letting the` |
|       - | 1869 | `							 * coalesce pick the default. */` |
|     ! 0 | 1870 | `							PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1871 | `							VM_EXIT_BREAK; /* pTos is already the null temp */` |
|       - | 1872 | `						}` |
|       - | 1873 | `						/* A subclass reading a PARENT's PRIVATE property: php treats it as` |
|       - | 1874 | `						 * an UNDEFINED property (the base-private is invisible to the` |
|       - | 1875 | `						 * subclass scope) — a Warning + null, NOT an access Error. Only` |
|       - | 1876 | `						 * this exact shape warns; every other denied read is the catchable` |
|       - | 1877 | `						 * "Cannot access" Error below. */` |
|       - | 1878 | `						{` |
|      12 | 1879 | `						ph7_class *pSelf = VmCurrentSelf(&(*pVm));` |
|      12 | 1880 | `						ph7_class *pDecl = pObjAttr->pAttr->pDeclClass;` |
|      10 | 1881 | `						if( pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      11 | 1882 | `						 && pSelf && pDecl && pSelf != pDecl && PH7_VmInstanceOf(pSelf,pDecl) ){` |
|       3 | 1883 | `							if( !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       4 | 1884 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|       1 | 1885 | `									&pClass->sName,&sName);` |
|       1 | 1886 | `							}` |
|       3 | 1887 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 1888 | `							VM_EXIT_BREAK; /* pTos is already the null temp */` |
|       - | 1889 | `						}` |
|       - | 1890 | `						}` |
|       - | 1891 | `						/* Genuinely inaccessible: php's CATCHABLE Error (parked on the` |
|       - | 1892 | `						 * boundary rail; the fetch-point router lands it — it was an` |
|       - | 1893 | `						 * uncatchable VmReportUncaughtException+Abort before). */` |
|       - | 1894 | `						{` |
|       - | 1895 | `						SyBlob sErrMsg;` |
|      10 | 1896 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      10 | 1897 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      10 | 1898 | `						SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",` |
|       4 | 1899 | `							zVis,&pClass->sName,&sName);` |
|      10 | 1900 | `						PH7_ClassInstanceUnref(pThis);` |
|      10 | 1901 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      10 | 1902 | `						SyBlobRelease(&sErrMsg);` |
|      10 | 1903 | `						VM_EXIT_BREAK;` |
|       - | 1904 | `						}` |
|       - | 1905 | `					}` |
|   53590 | 1906 | `				}` |
|       - | 1907 | `				/* Safely unreference the object */` |
|  107649 | 1908 | `				PH7_ClassInstanceUnref(pThis);` |
|       - | 1909 | `			}` |
|  117916 | 1910 | `		}else{` |
|       - | 1911 | ``			/* `->` on a non-object (e.g. a null intermediate). Silent in isset()/empty()/unset()`` |
|       - | 1912 | ``			 * context (iP2 2/3/4) so `isset($o->missing->x)` / `unset($o->missing->x)` match PHP. */`` |
|     132 | 1913 | `			if( pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1914 | `				/* php draws a sharp line here, and PH7 drew none: CALLING a method on a` |
|       - | 1915 | `				 * non-object is a catchable Error, while READING a property off one is a` |
|       - | 1916 | `				 * Warning that yields NULL. PH7 raised the same notice for both and` |
|       - | 1917 | ``				 * carried on with NULL, so `$null->m()` silently did nothing. */`` |
|       - | 1918 | `				SyString sMemb;` |
|     100 | 1919 | `				const char *zVerb = 0;` |
|     100 | 1920 | `				SyStringInitFromBuf(&sMemb,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     100 | 1921 | `				if( pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|       - | 1922 | ``					/* A deferred call ARGUMENT (`f($u->p)`): which diagnostic php raises`` |
|       - | 1923 | `					 * depends on the parameter, and this op does not know it — a by-VALUE` |
|       - | 1924 | `					 * binding is the read warning below, a by-REFERENCE one is php's` |
|       - | 1925 | ``					 * `Attempt to modify property` Error. Record the step rooted at the`` |
|       - | 1926 | `					 * base and let OP_CALL re-drive it in the mode the callee decides,` |
|       - | 1927 | `					 * exactly as a property MISSING on a real object is recorded. */` |
|      21 | 1928 | `					VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|      21 | 1929 | `					if( pPath && VmDeferPathPushProp(pPath,&sMemb) == SXRET_OK ){` |
|      21 | 1930 | `						VmPopOperand(&pTos,1);   /* drop the property name */` |
|      21 | 1931 | `						PH7_MemObjRelease(pTos); /* collapse the base slot into the carrier */` |
|      21 | 1932 | `						pTos->x.pOther = pPath;` |
|      21 | 1933 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      21 | 1934 | `						pTos->nIdx = SXU32_HIGH;` |
|      44 | 1935 | `						VM_EXIT_BREAK;` |
|       - | 1936 | `					}` |
|     ! 0 | 1937 | `					if( pPath ){` |
|     ! 0 | 1938 | `						VmFreeDeferredPath(pPath);` |
|     ! 0 | 1939 | `					}` |
|       - | 1940 | `					/* fall through to the immediate warning on allocation failure */` |
|     ! 0 | 1941 | `				}` |
|       - | 1942 | `				/* WRITING one is an Error too, and php picks its verb from what the` |
|       - | 1943 | `				 * write actually is. PH7 warned about a READ it never performed and` |
|       - | 1944 | `				 * then let the store fail into its own` |
|       - | 1945 | `				 * "Cannot perform assignment on a constant class attribute", so the` |
|       - | 1946 | `				 * script carried on past a statement php stops it for. The kind is` |
|       - | 1947 | `				 * read off the following instruction, the way every other write` |
|       - | 1948 | `				 * classification in this handler is (VmMemberFetchForWrite). */` |
|      80 | 1949 | `				if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|      31 | 1950 | `					const VmInstr *pNextW = pInstr + 1;` |
|      30 | 1951 | `					if( (pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0)` |
|      21 | 1952 | ``					 \|\| pNextW->iOp == PH7_OP_NULLC_JMP /* `$u->p ??= v` */`` |
|      12 | 1953 | `					 \|\| VmNextIsCompoundAssign(pNextW) ){` |
|      23 | 1954 | `						zVerb = "assign";` |
|      20 | 1955 | `					}else if( pNextW->iOp == PH7_OP_INCR \|\| pNextW->iOp == PH7_OP_DECR ){` |
|       5 | 1956 | `						zVerb = "increment/decrement";` |
|       3 | 1957 | `					}else{` |
|       - | 1958 | ``						/* The base of a subscript write (`$u->p[] = v`): php asks the`` |
|       - | 1959 | `						 * property for something to modify, and there is no property. */` |
|       5 | 1960 | `						zVerb = "modify";` |
|       1 | 1961 | `					}` |
|      65 | 1962 | `				}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       3 | 1963 | `					zVerb = "assign";` |
|      49 | 1964 | `				}else if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       3 | 1965 | `					zVerb = "modify";` |
|       1 | 1966 | `				}` |
|      80 | 1967 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD \|\| zVerb ){` |
|       - | 1968 | `					SyBlob sErrM;` |
|       - | 1969 | `					sxi32 rcErr;` |
|      47 | 1970 | `					SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      47 | 1971 | `					if( zVerb ){` |
|      35 | 1972 | `						SyBlobFormat(&sErrM,"Attempt to %s property \"%z\" on %s",` |
|      17 | 1973 | `							zVerb,&sMemb,VmArithValueName(pNos));` |
|      18 | 1974 | `					}else{` |
|      13 | 1975 | `						SyBlobFormat(&sErrM,"Call to a member function %z() on %s",` |
|       6 | 1976 | `							&sMemb,VmArithValueName(pNos));` |
|       - | 1977 | `					}` |
|      47 | 1978 | `					VmPopOperand(&pTos,1);` |
|      47 | 1979 | `					PH7_MemObjRelease(pTos);` |
|      47 | 1980 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      47 | 1981 | `					pTos->nIdx = SXU32_HIGH;` |
|      70 | 1982 | `					rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      23 | 1983 | `						SyBlobLength(&sErrM));` |
|      47 | 1984 | `					SyBlobRelease(&sErrM);` |
|      47 | 1985 | `					if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      47 | 1986 | `					rc = rcErr;` |
|      49 | 1987 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1988 | `				}` |
|      50 | 1989 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Attempt to read property \"%z\" on %s",` |
|      16 | 1990 | `					&sMemb,VmArithValueName(pNos));` |
|      16 | 1991 | `			}` |
|      66 | 1992 | `			VmPopOperand(&pTos,1);` |
|      66 | 1993 | `			PH7_MemObjRelease(pTos);` |
|      66 | 1994 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|       - | 1995 | `		}` |
|  117948 | 1996 | `	}else{` |
|       - | 1997 | `		/* Static member access using class name */` |
|  103577 | 1998 | `		pNos = pTos;` |
|  103577 | 1999 | `		pThis = 0;` |
|  103577 | 2000 | `		if( !pInstr->p3 ){` |
|    3141 | 2001 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    3141 | 2002 | `			pNos--;` |
|       - | 2003 | `#ifdef UNTRUST` |
|       - | 2004 | `			if( pNos < pStack ){` |
|       - | 2005 | `				VM_EXIT_ABORT;` |
|       - | 2006 | `			}` |
|       - | 2007 | `#endif` |
|    1573 | 2008 | `		}else{` |
|       - | 2009 | `			/* Attribute name already computed */` |
|  100441 | 2010 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|       - | 2011 | `		}` |
|  103577 | 2012 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|  103573 | 2013 | `			ph7_class *pClass = 0;` |
|       - | 2014 | `			/* A FORWARDING static call — self::/parent::/static:: — preserves the` |
|       - | 2015 | `			 * caller's late-static-binding class (php); a non-forwarding C::m() resets` |
|       - | 2016 | `			 * it to C. The receiver slot below keeps the literal keyword, which OP_CALL` |
|       - | 2017 | `			 * can't resolve, so it would fall back to the callee's DECLARING class and` |
|       - | 2018 | `			 * lose LSB. Remember the live LSB class here and stamp it onto the receiver` |
|       - | 2019 | `			 * after the method name is pushed. */` |
|  103573 | 2020 | `			int bForwardingCall = 0;` |
|  103573 | 2021 | `			ph7_class *pForwardLsb = 0;` |
|       - | 2022 | `			/* Same rail snapshot as OP_NEW: the lookup below can run an autoloader that` |
|       - | 2023 | `			 * throws, and php then reports that exception and nothing else. */` |
|  103573 | 2024 | `			sxi32 nMbBrc = pVm->nBoundaryRc;` |
|  103573 | 2025 | `			const void *pMbRes = (const void *)pVm->pResumeFrame;` |
|  103573 | 2026 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - | 2027 | `				/* Class already instantiated */` |
|      24 | 2028 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      24 | 2029 | `				pClass = pThis->pClass;` |
|      24 | 2030 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      13 | 2031 | `			}else{` |
|       - | 2032 | `				/* Try to extract the target class */` |
|  103551 | 2033 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|  103551 | 2034 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|  103551 | 2035 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|       - | 2036 | `					/* Handle self/static/parent keywords */` |
|  103551 | 2037 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       - | 2038 | `						/* In a trait method, self:: resolves to the USING class */` |
|     415 | 2039 | `						pClass = PH7_VmPeekSelfClass(&(*pVm));` |
|     415 | 2040 | `						bForwardingCall = 1;` |
|     415 | 2041 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|  103346 | 2042 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|     110 | 2043 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|     110 | 2044 | `						bForwardingCall = 1;` |
|     110 | 2045 | `						pForwardLsb = pClass;` |
|  103088 | 2046 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|     126 | 2047 | `						pClass = PH7_VmResolveParentClass(&(*pVm));` |
|     126 | 2048 | `						bForwardingCall = 1;` |
|     126 | 2049 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|      65 | 2050 | `					}else{` |
|  102913 | 2051 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|       - | 2052 | `					}` |
|   51773 | 2053 | `				}` |
|       - | 2054 | `			}` |
|  103573 | 2055 | `			if( pClass == 0 ){` |
|       - | 2056 | `				/* Undefined class: php throws a catchable Error */` |
|       - | 2057 | `				SyBlob sErrM;` |
|       - | 2058 | `				sxi32 rcErr;` |
|      15 | 2059 | `				if( PH7_VmClassLookupRaised(&(*pVm),nMbBrc,pMbRes) ){` |
|       - | 2060 | `					/* ...unless an autoloader raised: land THAT instead of reporting the` |
|       - | 2061 | `					 * class missing on top of it. */` |
|      10 | 2062 | `					sxi32 rcAutoM = pVm->nBoundaryRc;` |
|      10 | 2063 | `					pVm->nBoundaryRc = 0;` |
|      10 | 2064 | `					if( !pInstr->p3 ){` |
|       8 | 2065 | `						VmPopOperand(&pTos,1);` |
|       3 | 2066 | `					}` |
|      10 | 2067 | `					PH7_MemObjRelease(pTos);` |
|      10 | 2068 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      10 | 2069 | `					pTos->nIdx = SXU32_HIGH;` |
|      10 | 2070 | `					if( rcAutoM == PH7_ABORT ){` |
|     ! 0 | 2071 | `						VM_EXIT_ABORT;` |
|       - | 2072 | `					}` |
|      10 | 2073 | `					rc = PH7_EXCEPTION;` |
|      10 | 2074 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2075 | `				}` |
|       6 | 2076 | `				SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       6 | 2077 | `				SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|       4 | 2078 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob));` |
|       6 | 2079 | `				if( !pInstr->p3 ){` |
|       6 | 2080 | `					VmPopOperand(&pTos,1);` |
|       2 | 2081 | `				}` |
|       6 | 2082 | `				PH7_MemObjRelease(pTos);` |
|       6 | 2083 | `				pTos->nIdx = SXU32_HIGH;` |
|       8 | 2084 | `				rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       2 | 2085 | `					SyBlobLength(&sErrM));` |
|       6 | 2086 | `				SyBlobRelease(&sErrM);` |
|       6 | 2087 | `				if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       6 | 2088 | `				rc = rcErr;` |
|       6 | 2089 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 2090 | `			}else{` |
|  103561 | 2091 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - | 2092 | `					/* Method call */` |
|    1559 | 2093 | `					ph7_class_method *pMeth = 0;` |
|    1559 | 2094 | `					if( sName.nByte > 0 ){` |
|       - | 2095 | `						/* An INTERFACE's methods are looked up too: they are all abstract,` |
|       - | 2096 | `						 * so the arm below reports php's "Cannot call abstract method` |
|       - | 2097 | `						 * I::m()" rather than claiming the name does not exist. */` |
|    1559 | 2098 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|     777 | 2099 | `					}` |
|    1559 | 2100 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      70 | 2101 | `						if( pMeth ){` |
|       - | 2102 | `							SyBlob sErrM;` |
|       - | 2103 | `							sxi32 rcErr;` |
|       5 | 2104 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       5 | 2105 | `							SyBlobFormat(&sErrM,"Cannot call abstract method %z::%z()",` |
|       2 | 2106 | `								&pClass->sName,&sName);` |
|       5 | 2107 | `							if( !pInstr->p3 ){` |
|       5 | 2108 | `								VmPopOperand(&pTos,1);` |
|       2 | 2109 | `							}` |
|       5 | 2110 | `							PH7_MemObjRelease(pTos);` |
|       5 | 2111 | `							pTos->nIdx = SXU32_HIGH;` |
|       - | 2112 | ``							/* The `$obj::m()` form took a reference on the receiver at the`` |
|       - | 2113 | `							 * top of this branch; every exit has to give it back, and only` |
|       - | 2114 | `							 * the fall-through at the end of the arm used to. */` |
|       5 | 2115 | `							if( pThis ){` |
|     ! 0 | 2116 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2117 | `								pThis = 0;` |
|     ! 0 | 2118 | `							}` |
|       7 | 2119 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       2 | 2120 | `								SyBlobLength(&sErrM));` |
|       5 | 2121 | `							SyBlobRelease(&sErrM);` |
|       5 | 2122 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 2123 | `							rc = rcErr;` |
|       7 | 2124 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 2125 | `						}else{` |
|      66 | 2126 | `							ph7_class_instance *pMagicThis = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|      66 | 2127 | `							ph7_class_method *pCallStaticMagic = pMagicThis` |
|      16 | 2128 | `								? PH7_ClassExtractMethod(pMagicThis->pClass,"__call",sizeof("__call")-1)` |
|      54 | 2129 | `								: PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1);` |
|      66 | 2130 | `							if( pCallStaticMagic ){` |
|       - | 2131 | `								/* php: C::missing(...) dispatches __callStatic($name,$args)` |
|       - | 2132 | `								 * through the packing body (see the instance twin) — or` |
|       - | 2133 | `								 * __call($name,$args) on the calling frame's own $this when` |
|       - | 2134 | `								 * that receiver fits C (VmStaticCallMagicThis). */` |
|      85 | 2135 | `								VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pMagicThis,` |
|      27 | 2136 | `									pMagicThis ? pMagicThis->pClass : pClass,&sName);` |
|      58 | 2137 | `								if( pPend == 0 ){` |
|     ! 0 | 2138 | `									VM_EXIT_ABORT;` |
|       - | 2139 | `								}` |
|      58 | 2140 | `								if( !pInstr->p3 ){` |
|      58 | 2141 | `									VmPopOperand(&pTos,1);` |
|      27 | 2142 | `								}` |
|      58 | 2143 | `								PH7_MemObjRelease(pTos);` |
|      58 | 2144 | `								if( pThis ){` |
|     ! 0 | 2145 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2146 | `									pThis = 0;` |
|     ! 0 | 2147 | `								}` |
|      58 | 2148 | `								pTos->x.pOther = pPend;` |
|      58 | 2149 | `								pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|      58 | 2150 | `								pTos->nIdx = SXU32_HIGH;` |
|      58 | 2151 | `								VM_EXIT_BREAK;` |
|       - | 2152 | `							}` |
|       - | 2153 | `							{` |
|       - | 2154 | `								/* php: the STATIC form reports the same "Call to undefined` |
|       - | 2155 | `								 * method C::m()" as the instance one. */` |
|       - | 2156 | `								SyBlob sErrM;` |
|       - | 2157 | `								sxi32 rcErr;` |
|       9 | 2158 | `								SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       9 | 2159 | `								SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",` |
|       4 | 2160 | `									&pClass->sName,&sName);` |
|       9 | 2161 | `								if( !pInstr->p3 ){` |
|       9 | 2162 | `									VmPopOperand(&pTos,1);` |
|       4 | 2163 | `								}` |
|       9 | 2164 | `								PH7_MemObjRelease(pTos);` |
|       9 | 2165 | `								pTos->nIdx = SXU32_HIGH;` |
|       9 | 2166 | `								if( pThis ){` |
|     ! 0 | 2167 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2168 | `									pThis = 0;` |
|     ! 0 | 2169 | `								}` |
|      13 | 2170 | `								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       4 | 2171 | `									SyBlobLength(&sErrM));` |
|       9 | 2172 | `								SyBlobRelease(&sErrM);` |
|       9 | 2173 | `								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 | 2174 | `								rc = rcErr;` |
|      11 | 2175 | `								PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2176 | `							}` |
|       - | 2177 | `						}` |
|       - | 2178 | `						/* Pop the method name from the stack */` |
|     ! 0 | 2179 | `						if( !pInstr->p3 ){` |
|     ! 0 | 2180 | `							VmPopOperand(&pTos,1);` |
|       - | 2181 | `						}` |
|     ! 0 | 2182 | `						PH7_MemObjRelease(pTos);` |
|     ! 0 | 2183 | `					}else{` |
|       - | 2184 | `						/* Inaccessible from this scope: php routes through the same fallback a` |
|       - | 2185 | ``						 * MISSING name takes — __call on a compatible `$this`, else`` |
|       - | 2186 | ``						 * __callStatic — which is why `C::privateStatic()` runs a catch-all`` |
|       - | 2187 | `						 * instead of the "Call to private method" Error OP_CALL would raise` |
|       - | 2188 | `						 * below. */` |
|    1493 | 2189 | `						ph7_class_method *pDeniedStatic = 0;` |
|    1493 | 2190 | `						ph7_class_instance *pDeniedThis = 0;` |
|    1493 | 2191 | `						int bDeniedStatic = 0;` |
|    1493 | 2192 | `						if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - | 2193 | `							/* The OWNING class decides, as in the instance twin above: a` |
|       - | 2194 | `							 * trait method's rules belong to the class that composed it. */` |
|      29 | 2195 | `							ph7_class *pOwnerCls = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|      29 | 2196 | `							if( !PH7_VmClassMemberAccess(&(*pVm),pOwnerCls,&sName,pMeth->iProtection,FALSE) ){` |
|      15 | 2197 | `								pDeniedThis = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|      15 | 2198 | `								pDeniedStatic = pDeniedThis` |
|       4 | 2199 | `									? PH7_ClassExtractMethod(pDeniedThis->pClass,"__call",` |
|       - | 2200 | `										sizeof("__call")-1)` |
|      12 | 2201 | `									: PH7_ClassExtractMethod(pClass,"__callStatic",` |
|       - | 2202 | `										sizeof("__callStatic")-1);` |
|      15 | 2203 | `								bDeniedStatic = pDeniedStatic == 0;` |
|       7 | 2204 | `							}` |
|      14 | 2205 | `						}` |
|    1493 | 2206 | `						if( bDeniedStatic ){` |
|       - | 2207 | `							/* No catch-all answers for it: php's visibility Error, raised at` |
|       - | 2208 | `							 * the resolution like the instance twin above. OP_CALL's own` |
|       - | 2209 | `							 * screen re-derives the method from the FUNCTION's name against` |
|       - | 2210 | `							 * its declaring class, which cannot see a trait adaptation's` |
|       - | 2211 | ``							 * composed protection — `sHi as private sPriv` ran from global`` |
|       - | 2212 | `							 * scope in silence. */` |
|       - | 2213 | `							SyBlob sErrM;` |
|       - | 2214 | `							sxi32 rcErr;` |
|       7 | 2215 | `							ph7_class *pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|       7 | 2216 | `							ph7_class *pScope = PH7_VmCallerScopeName(&(*pVm));` |
|      10 | 2217 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       3 | 2218 | `								? "private" : "protected";` |
|       7 | 2219 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       7 | 2220 | `							if( pScope ){` |
|       3 | 2221 | `								SyBlobFormat(&sErrM,"Call to %s method %z::%z() from scope %z",` |
|       1 | 2222 | `									zVis,&pOwner->sName,&sName,&pScope->sName);` |
|       2 | 2223 | `							}else{` |
|       5 | 2224 | `								SyBlobFormat(&sErrM,"Call to %s method %z::%z() from global scope",` |
|       2 | 2225 | `									zVis,&pOwner->sName,&sName);` |
|       - | 2226 | `							}` |
|       7 | 2227 | `							if( !pInstr->p3 ){` |
|       7 | 2228 | `								VmPopOperand(&pTos,1);` |
|       3 | 2229 | `							}` |
|       7 | 2230 | `							PH7_MemObjRelease(pTos);` |
|       7 | 2231 | `							pTos->nIdx = SXU32_HIGH;` |
|       7 | 2232 | `							if( pThis ){` |
|     ! 0 | 2233 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2234 | `								pThis = 0;` |
|     ! 0 | 2235 | `							}` |
|      10 | 2236 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       3 | 2237 | `								SyBlobLength(&sErrM));` |
|       7 | 2238 | `							SyBlobRelease(&sErrM);` |
|       7 | 2239 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       7 | 2240 | `							rc = rcErr;` |
|       7 | 2241 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2242 | `						}` |
|    1487 | 2243 | `						if( pDeniedStatic ){` |
|       - | 2244 | `							/* The packing body takes no receiver slot: drop the method-name` |
|       - | 2245 | `							 * slot so the MARK lands on the receiver slot, exactly as the` |
|       - | 2246 | `							 * missing-method twin above does (leaving the class name in place` |
|       - | 2247 | `							 * would pack it as the first $args entry). */` |
|      13 | 2248 | `							VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pDeniedThis,` |
|       4 | 2249 | `								pDeniedThis ? pDeniedThis->pClass : pClass,&sName);` |
|       9 | 2250 | `							if( pPend == 0 ){` |
|     ! 0 | 2251 | `								VM_EXIT_ABORT;` |
|       - | 2252 | `							}` |
|       9 | 2253 | `							if( !pInstr->p3 ){` |
|       9 | 2254 | `								VmPopOperand(&pTos,1);` |
|       4 | 2255 | `							}` |
|       9 | 2256 | `							PH7_MemObjRelease(pTos);` |
|       9 | 2257 | `							if( pThis ){` |
|     ! 0 | 2258 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2259 | `								pThis = 0;` |
|     ! 0 | 2260 | `							}` |
|       9 | 2261 | `							pTos->x.pOther = pPend;` |
|       9 | 2262 | `							pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|       9 | 2263 | `							pTos->nIdx = SXU32_HIGH;` |
|       9 | 2264 | `							VM_EXIT_BREAK;` |
|       - | 2265 | `						}` |
|       - | 2266 | ``						/* php refuses a NON-STATIC method named through `::` unless the`` |
|       - | 2267 | ``						 * CALLING frame holds a `$this` the class accepts: that is what`` |
|       - | 2268 | ``						 * makes `self::`/`parent::`/`static::` and `C::m()` from inside a`` |
|       - | 2269 | `						 * C method work, and every other spelling — from global scope,` |
|       - | 2270 | `` 						 * from a static method, from an unrelated class, and `$obj::m()` `` |
|       - | 2271 | ``						 * — an Error. PHL ran the body instead, with `$this` unset or`` |
|       - | 2272 | ``						 * (for the object form) bound to the object the `::` was written`` |
|       - | 2273 | `						 * on, which php never uses: it takes the CALLER's. The message` |
|       - | 2274 | ``						 * names the DECLARING class, so `D::m()` reports B::m(). */`` |
|    1479 | 2275 | `						if( (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|     131 | 2276 | `							ph7_class_instance *pCallerThis = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|     131 | 2277 | `							if( pCallerThis == 0 ){` |
|       - | 2278 | `								SyBlob sErrM;` |
|       - | 2279 | `								sxi32 rcErr;` |
|      13 | 2280 | `								ph7_class *pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|      13 | 2281 | `								SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      13 | 2282 | `								SyBlobFormat(&sErrM,"Non-static method %z::%z() cannot be called statically",` |
|       6 | 2283 | `									&pOwner->sName,&pMeth->sFunc.sName);` |
|      13 | 2284 | `								if( !pInstr->p3 ){` |
|      13 | 2285 | `									VmPopOperand(&pTos,1);` |
|       6 | 2286 | `								}` |
|      13 | 2287 | `								PH7_MemObjRelease(pTos);` |
|      13 | 2288 | `								pTos->nIdx = SXU32_HIGH;` |
|      13 | 2289 | `								if( pThis ){` |
|       3 | 2290 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 2291 | `									pThis = 0;` |
|       1 | 2292 | `								}` |
|      19 | 2293 | `								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       6 | 2294 | `									SyBlobLength(&sErrM));` |
|      13 | 2295 | `								SyBlobRelease(&sErrM);` |
|      13 | 2296 | `								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      13 | 2297 | `								rc = rcErr;` |
|      13 | 2298 | `								PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2299 | `							}` |
|       - | 2300 | `` 							/* The receiver is that `$this` — never the object the `::` `` |
|       - | 2301 | `							 * was written on, and never nothing. Naming it here is also` |
|       - | 2302 | `							 * what makes the call work inside a CLOSURE declared in a` |
|       - | 2303 | ``							 * method: php gives such a closure a `$this`, PHL carries it`` |
|       - | 2304 | `							 * as a frame variable rather than on the frame itself, and` |
|       - | 2305 | ``							 * OP_CALL reads only the frame, so `self::m()` in a closure`` |
|       - | 2306 | `							 * used to run with no receiver at all. */` |
|     119 | 2307 | `							PH7_MemObjRelease(pNos);` |
|     119 | 2308 | `							pNos->x.pOther = pCallerThis;` |
|     119 | 2309 | `							MemObjSetType(pNos,MEMOBJ_OBJ);` |
|     119 | 2310 | `							pCallerThis->iRef++;` |
|     119 | 2311 | `							pNos->nIdx = SXU32_HIGH;` |
|      58 | 2312 | `						}` |
|       - | 2313 | `						/* Push method name on the stack, MARKED as already screened (see the` |
|       - | 2314 | `						 * instance twin: OP_CALL cannot re-derive a composed protection). */` |
|    1467 | 2315 | `						PH7_MemObjRelease(pTos);` |
|    1467 | 2316 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|    1467 | 2317 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|    1467 | 2318 | `						pTos->iFlags \|= MEMOBJ_AUX_MEMBERCALL;` |
|       - | 2319 | `					}` |
|    1467 | 2320 | `					pTos->nIdx = SXU32_HIGH;` |
|       - | 2321 | `					/* Forwarding call (self::/parent::/static::): overwrite the receiver` |
|       - | 2322 | `					 * slot (pNos, one below the method name) with the live LSB class name` |
|       - | 2323 | ``					 * so OP_CALL pushes THAT onto aSelf — preserving `static::` inside the`` |
|       - | 2324 | `					 * callee. Without this the literal keyword falls through to the` |
|       - | 2325 | `					 * callee's declaring class. Only when an LSB class is actually in` |
|       - | 2326 | `					 * scope (a static call from global scope has none). */` |
|    1467 | 2327 | `					if( bForwardingCall && pForwardLsb && (pNos->iFlags & MEMOBJ_STRING) ){` |
|      80 | 2328 | `						SyBlobReset(&pNos->sBlob);` |
|      80 | 2329 | `						SyBlobAppend(&pNos->sBlob,pForwardLsb->sName.zString,pForwardLsb->sName.nByte);` |
|      39 | 2330 | `					}` |
|     736 | 2331 | `				}else{` |
|       - | 2332 | `					/* Attribute access */` |
|  102007 | 2333 | `					ph7_class_attr *pAttr = 0;` |
|  102007 | 2334 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - | 2335 | `						/* unset(C::$x): PHP rejects unsetting a static property with a fatal Error.` |
|       - | 2336 | `						 * Without this the iP2=unset tag falls through to a normal static read and the` |
|       - | 2337 | `						 * trailing generic unset() would silently NULL (and de-type) the shared slot. */` |
|       - | 2338 | `						char zMsg[256];` |
|     ! 0 | 2339 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Attempt to unset static property %.*s::$%.*s",` |
|     ! 0 | 2340 | `							(int)pClass->sName.nByte,pClass->sName.zString,(int)sName.nByte,sName.zString);` |
|     ! 0 | 2341 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|     ! 0 | 2342 | `						VM_EXIT_ABORT;` |
|       - | 2343 | `					}` |
|       - | 2344 | `					/* Check for special ::class pseudo-constant */` |
|  102167 | 2345 | `					if( sName.nByte == sizeof("class")-1 &&` |
|     320 | 2346 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|       - | 2347 | `						/* ::class returns the fully qualified class name */` |
|       - | 2348 | `						/* Pop the attribute name from the stack */` |
|     152 | 2349 | `						if( !pInstr->p3 ){` |
|     152 | 2350 | `							VmPopOperand(&pTos,1);` |
|      74 | 2351 | `						}` |
|     152 | 2352 | `						PH7_MemObjRelease(pTos);` |
|       - | 2353 | `						/* Load the class name */` |
|     152 | 2354 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|     152 | 2355 | `						pTos->nIdx = SXU32_HIGH;` |
|      78 | 2356 | `					}else{` |
|       - | 2357 | `						/* Extract the target attribute. php keeps constants and` |
|       - | 2358 | `						 * (static) properties in separate namespaces; the source` |
|       - | 2359 | ``						 * form disambiguates (set by the `::` codegen, compile.c):`` |
|       - | 2360 | ``						 * a `$`-form is a STATIC PROPERTY (hAttr) — either a literal`` |
|       - | 2361 | ``						 * name folded into pInstr->p3 (`C::$s`) or a dynamic name on`` |
|       - | 2362 | ``						 * the stack marked iP1==2 (`C::$$x`, `C::${$e}`); a bareword`` |
|       - | 2363 | `						 * (p3==0, iP1==1) is a CONSTANT or enum case (hConst). This` |
|       - | 2364 | ``						 * is what lets `const C` and `public $C` coexist and resolve`` |
|       - | 2365 | `						 * to the right member. */` |
|  101859 | 2366 | `						if( sName.nByte > 0 ){` |
|  102570 | 2367 | `							pAttr = (pInstr->p3 \|\| pInstr->iP1 == 2)` |
|  100438 | 2368 | `								? PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte)` |
|   52343 | 2369 | `								: PH7_ClassExtractConstant(pClass,sName.zString,sName.nByte);` |
|   50927 | 2370 | `						}` |
|  101859 | 2371 | `						if( pAttr == 0 ){` |
|       - | 2372 | `							/* No such member. php raises a catchable Error whose wording` |
|       - | 2373 | `							 * depends on the ACCESS form (the same p3/iP1 signal used above):` |
|       - | 2374 | ``							 * a bareword `C::MISSING` (constant form) is "Undefined constant`` |
|       - | 2375 | ``							 * C::MISSING", a `$`-form `C::$missing` is "Access to undeclared`` |
|       - | 2376 | `							 * static property C::$missing". Instance magic (__get) is never` |
|       - | 2377 | `							 * consulted for statics (band A #3b). isset()/empty() context` |
|       - | 2378 | `							 * stays silently false. Parked on the boundary rail; the op` |
|       - | 2379 | `							 * completes benignly with NULL and the fetch-point router lands` |
|       - | 2380 | `							 * the throw. */` |
|      17 | 2381 | `							if( !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2382 | `								SyBlob sErrMsg;` |
|      17 | 2383 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      17 | 2384 | `								if( pInstr->p3 == 0 && pInstr->iP1 != 2 ){` |
|      10 | 2385 | `									SyBlobFormat(&sErrMsg,"Undefined constant %z::%z",` |
|       4 | 2386 | `										&pClass->sName,&sName);` |
|       6 | 2387 | `								}else{` |
|       8 | 2388 | `									SyBlobFormat(&sErrMsg,"Access to undeclared static property %z::$%z",` |
|       3 | 2389 | `										&pClass->sName,&sName);` |
|       - | 2390 | `								}` |
|      17 | 2391 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       7 | 2392 | `							}` |
|       7 | 2393 | `						}` |
|       - | 2394 | `						/* Pop the attribute name from the stack */` |
|  101859 | 2395 | `						if( !pInstr->p3 ){` |
|    1427 | 2396 | `							VmPopOperand(&pTos,1);` |
|     711 | 2397 | `						}` |
|  101859 | 2398 | `						PH7_MemObjRelease(pTos);` |
|  101859 | 2399 | `						pTos->nIdx = SXU32_HIGH;` |
|  101859 | 2400 | `						if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - | 2401 | ``							/* `self::$s =& $x` / `C::$s =& $x`: stash the static property slot`` |
|       - | 2402 | `							 * for the following member-marked OP_STORE_REF (class-level, shared` |
|       - | 2403 | `							 * across instances — matches php). Skip the read machinery below. */` |
|      14 | 2404 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|      14 | 2405 | `							 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|      16 | 2406 | `							 && VmClassStaticDeferPending(pClass) ){` |
|       - | 2407 | `								/* Binding a reference TO a static touches the table, so it` |
|       - | 2408 | `								 * materializes first, like every other static access. */` |
|       3 | 2409 | `								sxi32 rcRt = PH7_VmMaterializeClassStatics(&(*pVm),pClass);` |
|       3 | 2410 | `								if( rcRt != SXRET_OK ){` |
|       3 | 2411 | `									pVm->pRefTargetStaticAttr = 0;` |
|       3 | 2412 | `									pVm->pRefTargetAttr = 0;` |
|       3 | 2413 | `									pVm->pRefTargetThis = 0;` |
|       3 | 2414 | `									if( pThis ){` |
|     ! 0 | 2415 | `										PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2416 | `									}` |
|       3 | 2417 | `									if( rcRt == PH7_ABORT ){` |
|     ! 0 | 2418 | `										VM_EXIT_ABORT;` |
|       - | 2419 | `									}` |
|       - | 2420 | `									{` |
|       - | 2421 | `										sxi32 iRpR;` |
|       3 | 2422 | `										if( VmRecordedResume(pVm,&iRpR,pState->pEntryFrame,aInstr) ){` |
|       7 | 2423 | `											PH7_RESUME_DRAIN()` |
|       3 | 2424 | `											pc = iRpR;` |
|       3 | 2425 | `											VM_EXIT_BREAK;` |
|       - | 2426 | `										}` |
|       - | 2427 | `									}` |
|     ! 0 | 2428 | `									VM_EXIT_EXCEPTION;` |
|       - | 2429 | `								}` |
|     ! 0 | 2430 | `							}` |
|      12 | 2431 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|      14 | 2432 | `							 && PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|      14 | 2433 | `								pVm->pRefTargetStaticAttr = pAttr;` |
|      14 | 2434 | `								pVm->pRefTargetAttr = 0;` |
|      14 | 2435 | `								pVm->pRefTargetThis = 0;` |
|      14 | 2436 | `								pTos->nIdx = pAttr->nIdx;` |
|       8 | 2437 | `							}else{` |
|     ! 0 | 2438 | `								pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 2439 | `								pVm->pRefTargetAttr = 0;` |
|     ! 0 | 2440 | `								pVm->pRefTargetThis = 0;` |
|       - | 2441 | `							}` |
|      14 | 2442 | `							if( pThis ){` |
|     ! 0 | 2443 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2444 | `							}` |
|      14 | 2445 | `							VM_EXIT_BREAK;` |
|       - | 2446 | `						}` |
|  101845 | 2447 | `						if( pAttr ){` |
|  101831 | 2448 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       - | 2449 | `								/* Access to a non static attribute */` |
|     ! 0 | 2450 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|     ! 0 | 2451 | `									&pClass->sName,&pAttr->sName` |
|       - | 2452 | `									);` |
|     ! 0 | 2453 | `							}else{` |
|       - | 2454 | `								ph7_value *pValue;` |
|       - | 2455 | `								/* php materializes the class's static table at the FIRST` |
|       - | 2456 | `								 * static-property access (any property, any context — read,` |
|       - | 2457 | `								 * write, even isset): a default whose evaluation was DEFERRED` |
|       - | 2458 | `								 * (it threw at the declaration) runs here, and a typed one that` |
|       - | 2459 | `								 * failed its check throws its catchable TypeError here. Class` |
|       - | 2460 | `								 * constants and static method calls do not trigger the` |
|       - | 2461 | `								 * materialization (php-exact). */` |
|  101826 | 2462 | `								if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|  101122 | 2463 | `								 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|  100423 | 2464 | `								 && VmClassStaticDeferPending(pClass) ){` |
|      49 | 2465 | `									sxi32 rcD = PH7_VmMaterializeClassStatics(&(*pVm),pClass);` |
|      49 | 2466 | `									if( rcD != SXRET_OK ){` |
|      45 | 2467 | `										if( pThis ){` |
|     ! 0 | 2468 | `											PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2469 | `										}` |
|      45 | 2470 | `										if( rcD == PH7_ABORT ){` |
|     ! 0 | 2471 | `											VM_EXIT_ABORT;` |
|       - | 2472 | `										}` |
|       - | 2473 | `										{` |
|       - | 2474 | `											sxi32 iRpD;` |
|      45 | 2475 | `											if( VmRecordedResume(pVm,&iRpD,pState->pEntryFrame,aInstr) ){` |
|      89 | 2476 | `												PH7_RESUME_DRAIN()` |
|      41 | 2477 | `												pc = iRpD;` |
|      41 | 2478 | `												VM_EXIT_BREAK;` |
|       - | 2479 | `											}` |
|       - | 2480 | `										}` |
|       5 | 2481 | `										VM_EXIT_EXCEPTION;` |
|       - | 2482 | `									}` |
|       2 | 2483 | `								}` |
|       - | 2484 | `								/* Check if the access to the attribute is allowed */` |
|  101789 | 2485 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|       - | 2486 | `									/* PHP 7.4+: uninitialized typed static read.` |
|       - | 2487 | `									 * Same LHS-of-store peek as the instance path. */` |
|  101780 | 2488 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|  100964 | 2489 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|  150143 | 2490 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|  100092 | 2491 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|  100097 | 2492 | `										if( pS ){` |
|  100097 | 2493 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|  100097 | 2494 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|  100015 | 2495 | `												VmInstr *pNext = pInstr + 1;` |
|  100015 | 2496 | `												int bIsLhs = 0;` |
|  100015 | 2497 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       9 | 2498 | `													bIsLhs = 1;` |
|       3 | 2499 | `												}` |
|  100015 | 2500 | `												if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 2501 | `													/* Destructuring target ([S::$s] = [...]):` |
|       - | 2502 | `													 * initialized by the OP_LOAD_LIST store. */` |
|       3 | 2503 | `													bIsLhs = 1;` |
|       1 | 2504 | `												}` |
|  100015 | 2505 | `												if( !bIsLhs ){` |
|  100004 | 2506 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|  100004 | 2507 | `													if( pThis ){` |
|     ! 0 | 2508 | `														PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2509 | `													}` |
|  100004 | 2510 | `													if( rcU == PH7_ABORT ){` |
|     ! 0 | 2511 | `														VM_EXIT_ABORT;` |
|       - | 2512 | `													}` |
|       - | 2513 | `													{` |
|       - | 2514 | `														sxi32 iRp;` |
|  100004 | 2515 | `														if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|  200006 | 2516 | `															PH7_RESUME_DRAIN()` |
|  100004 | 2517 | `															pc = iRp;` |
|  100004 | 2518 | `															VM_EXIT_BREAK;` |
|       - | 2519 | `														}` |
|       - | 2520 | `													}` |
|     ! 0 | 2521 | `													VM_EXIT_EXCEPTION;` |
|       - | 2522 | `												}` |
|       4 | 2523 | `											}` |
|      45 | 2524 | `										}` |
|      45 | 2525 | `									}` |
|    1778 | 2526 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|    1596 | 2527 | `									 && SySetUsed(&pAttr->aAttrs) > 0 ){` |
|       - | 2528 | `										/* php 8.4 #[\Deprecated] on a class constant:` |
|       - | 2529 | `										 * every access re-warns. */` |
|      11 | 2530 | `										VmDeprecatedConstNotice(&(*pVm),pClass,pAttr);` |
|       5 | 2531 | `									}` |
|    1783 | 2532 | `									if( pAttr->nIdx == SXU32_HIGH ){` |
|       - | 2533 | `										/* Unmaterialized slot. Enum case: first access` |
|       - | 2534 | `										 * materializes ALL the singletons. Plain constant: its` |
|       - | 2535 | `										 * initializer hasn't run yet (mount-order-dependent` |
|       - | 2536 | `										 * cross-constant reference) — evaluate on demand. A` |
|       - | 2537 | `										 * raised TypeError/Error (backing mismatch, duplicate` |
|       - | 2538 | `										 * value, self-reference) parks on the boundary rail;` |
|       - | 2539 | `										 * the op completes benignly with NULL and the` |
|       - | 2540 | `										 * fetch-point router lands the throw. */` |
|       - | 2541 | `										sxi32 rcEnum;` |
|     607 | 2542 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|       - | 2543 | `											/* php: a DIRECT static access evaluates every case` |
|       - | 2544 | `											 * of the enum (whole-class constant update) — a` |
|       - | 2545 | `											 * broken sibling case throws here too. A reference` |
|       - | 2546 | `											 * from inside another constant's initializer` |
|       - | 2547 | `											 * (nConstEvalDepth > 0) evaluates only the` |
|       - | 2548 | `											 * requested case. */` |
|      57 | 2549 | `											if( pVm->nConstEvalDepth > 0 ){` |
|       3 | 2550 | `												rcEnum = VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|       2 | 2551 | `											}else{` |
|      55 | 2552 | `												rcEnum = VmEnumMaterialize(&(*pVm),pClass);` |
|       - | 2553 | `											}` |
|      31 | 2554 | `										}else{` |
|     555 | 2555 | `											rcEnum = VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|       - | 2556 | `										}` |
|     607 | 2557 | `										if( rcEnum != SXRET_OK ){` |
|      41 | 2558 | `											VmBoundaryPark(&(*pVm),rcEnum);` |
|      19 | 2559 | `										}` |
|     301 | 2560 | `									}` |
|       - | 2561 | `									/* Load the desired attribute */` |
|    1783 | 2562 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|    1783 | 2563 | `									if( pValue ){` |
|    1743 | 2564 | `										PH7_MemObjLoad(pValue,pTos);` |
|    1743 | 2565 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       - | 2566 | `											/* Load index number */` |
|     379 | 2567 | `											pTos->nIdx = pAttr->nIdx;` |
|     187 | 2568 | `										}` |
|     869 | 2569 | `									}` |
|     894 | 2570 | `								}else{` |
|       - | 2571 | `									/* Throw Error exception (PHP-compatible) */` |
|       - | 2572 | `									char zMsg[256];` |
|       5 | 2573 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       5 | 2574 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|       7 | 2575 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|       4 | 2576 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|       4 | 2577 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|       3 | 2578 | `									}else{` |
|     ! 0 | 2579 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|     ! 0 | 2580 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|     ! 0 | 2581 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|       - | 2582 | `									}` |
|       5 | 2583 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|       5 | 2584 | `									VM_EXIT_ABORT;` |
|       - | 2585 | `								}` |
|       - | 2586 | `							}` |
|     889 | 2587 | `						}` |
|       - | 2588 | `					}` |
|       - | 2589 | `				}` |
|    3407 | 2590 | `				if( pThis ){` |
|       - | 2591 | `					/* Safely unreference the object */` |
|      22 | 2592 | `					PH7_ClassInstanceUnref(pThis);` |
|      10 | 2593 | `				}` |
|       - | 2594 | `			}` |
|    1706 | 2595 | `		}else{` |
|       - | 2596 | ``			/* `$v::X` where $v holds neither an object nor a class NAME. php's`` |
|       - | 2597 | `			 * catchable Error, which STOPS the statement; PH7 raised its own` |
|       - | 2598 | `` 			 * uncatchable wording and carried on with NULL, so a `$obj::$s = 1` `` |
|       - | 2599 | `			 * through a null base went on to fail again in OP_STORE. */` |
|       - | 2600 | `			sxi32 rcCn;` |
|       5 | 2601 | `			if( !pInstr->p3 ){` |
|       3 | 2602 | `				VmPopOperand(&pTos,1);` |
|       1 | 2603 | `			}` |
|       5 | 2604 | `			PH7_MemObjRelease(pTos);` |
|       5 | 2605 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 2606 | `			pTos->nIdx = SXU32_HIGH;` |
|       5 | 2607 | `			rcCn = VmThrowFromVm(&(*pVm),"Error","Class name must be a valid object or a string",` |
|       - | 2608 | `				sizeof("Class name must be a valid object or a string")-1);` |
|       5 | 2609 | `			if( rcCn == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 2610 | `			rc = rcCn;` |
|       5 | 2611 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2612 | `		}` |
|       - | 2613 | `	}` |
|  239291 | 2614 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2615 | `	VM_EXIT_BREAK;` |
|  173149 | 2616 | `}` |
|       - | 2617 |  |
|       - | 2618 | `/*` |
|       - | 2619 | ` * OP_CLONE: body moved verbatim from the OP_CLONE arm of` |
|       - | 2620 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2621 | ` */` |
|     304 | 2622 | `PH7_PRIVATE VmOpRc VmExecOpClone(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2623 | `{` |
|     309 | 2624 | `	ph7_value *pTos = pState->pTos;` |
|     309 | 2625 | `	ph7_value *pStack = pState->pStack;` |
|     309 | 2626 | `	VmInstr *aInstr = pState->aInstr;` |
|     309 | 2627 | `	sxi32 pc = pState->pc;` |
|       - | 2628 | `	sxi32 rc;` |
|     152 | 2629 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2630 | `	ph7_class_instance *pSrc,*pClone;` |
|       - | 2631 | `#ifdef UNTRUST` |
|       - | 2632 | `	if( pTos < pStack ){` |
|       - | 2633 | `		VM_EXIT_ABORT;` |
|       - | 2634 | `	}` |
|       - | 2635 | `#endif` |
|       - | 2636 | `	/* Make sure we are dealing with a class instance. PHP 8 throws a catchable` |
|       - | 2637 | ``	 * TypeError for a non-object operand — for both `clone $x` and clone($x). */`` |
|     309 | 2638 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - | 2639 | `		SyBlob sMsg;` |
|       - | 2640 | `		char zGiven[64];` |
|      24 | 2641 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 2642 | `		/* php names the VALUE for a bool ("true given", not "bool given"), which is` |
|       - | 2643 | `		 * what every other argument diagnostic here already says — the shared helper. */` |
|      24 | 2644 | `		SyBlobFormat(&sMsg,"clone(): Argument #1 ($object) must be of type object, %s given",` |
|      11 | 2645 | `			VmValueGivenName(pTos,zGiven,sizeof(zGiven)));` |
|      24 | 2646 | `		rc = VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|      24 | 2647 | `		PH7_MemObjRelease(pTos);` |
|      24 | 2648 | `		pTos->nIdx = SXU32_HIGH;` |
|      24 | 2649 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 2650 | `			VM_EXIT_ABORT;` |
|       - | 2651 | `		}` |
|       - | 2652 | `		{` |
|       - | 2653 | `			sxi32 iRp;` |
|      24 | 2654 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      24 | 2655 | `				pc = iRp;` |
|      24 | 2656 | `				VM_EXIT_BREAK;` |
|       - | 2657 | `			}` |
|       - | 2658 | `		}` |
|     ! 0 | 2659 | `		VM_EXIT_EXCEPTION;` |
|       - | 2660 | `	}` |
|       - | 2661 | `	/* Point to the source */` |
|     287 | 2662 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|       - | 2663 | `	/* Enum cases are not cloneable — php's catchable Error (the singleton` |
|       - | 2664 | `	 * identity would break) — and neither is a class whose instances own a` |
|       - | 2665 | `	 * C-side resource a slot-by-slot copy would double-free (PH7_CLASS_NOCLONE),` |
|       - | 2666 | `	 * nor a Generator or a Fiber (their suspended C state is not copyable). php` |
|       - | 2667 | `	 * words all of them the same way and throws the same catchable Error; the` |
|       - | 2668 | `	 * Generator/Fiber pair used to take an older warn-only path here, so` |
|       - | 2669 | ``	 * `clone $gen` PRINTED an uncatchable diagnostic and then carried on with the`` |
|       - | 2670 | ``	 * ORIGINAL object standing in for the copy — the one shape of `clone` that`` |
|       - | 2671 | `	 * answered a value php never lets the program reach. */` |
|     282 | 2672 | `	if( (pSrc->pClass->iFlags & PH7_CLASS_ENUM) \|\| PH7_ClassIsUncloneable(pSrc->pClass)` |
|     235 | 2673 | `		\|\| pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|       - | 2674 | `		SyBlob sMsg;` |
|     104 | 2675 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     104 | 2676 | `		SyBlobFormat(&sMsg,"Trying to clone an uncloneable object of class %z",` |
|     102 | 2677 | `			&pSrc->pClass->sName);` |
|     104 | 2678 | `		rc = VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|     104 | 2679 | `		PH7_MemObjRelease(pTos);` |
|     104 | 2680 | `		pTos->nIdx = SXU32_HIGH;` |
|     104 | 2681 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 2682 | `			VM_EXIT_ABORT;` |
|       - | 2683 | `		}` |
|       - | 2684 | `		{` |
|       - | 2685 | `			sxi32 iRp;` |
|     104 | 2686 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      84 | 2687 | `				pc = iRp;` |
|      84 | 2688 | `				VM_EXIT_BREAK;` |
|       - | 2689 | `			}` |
|       - | 2690 | `		}` |
|      21 | 2691 | `		VM_EXIT_EXCEPTION;` |
|       - | 2692 | `	}` |
|       - | 2693 | `	/* Perform the clone operation */` |
|     185 | 2694 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|     185 | 2695 | `	PH7_MemObjRelease(pTos);` |
|     185 | 2696 | `	if( pClone == 0 ){` |
|     ! 0 | 2697 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 2698 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|     ! 0 | 2699 | `	}else{` |
|       - | 2700 | `		/* Load the cloned object */` |
|     185 | 2701 | `		pTos->x.pOther = pClone;` |
|     185 | 2702 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - | 2703 | `	}` |
|     185 | 2704 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2705 | `	VM_EXIT_BREAK;` |
|     157 | 2706 | `}` |
|       - | 2707 |  |
