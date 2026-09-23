# src/ph7/vm_ops_oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1324/1483 lines (89.28%)

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
| 2113200 |   28 | `PH7_PRIVATE VmOpRc VmExecOpNew(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   29 | `{` |
| 2113205 |   30 | `	ph7_value *pTos = pState->pTos;` |
| 2113205 |   31 | `	ph7_value *pStack = pState->pStack;` |
| 2113205 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
| 2113205 |   33 | `	sxi32 pc = pState->pc;` |
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
| 2113205 |   49 | `	int bScreenOnly = pInstr->iP1 < 0;` |
| 2667686 |   50 | `	sxi32 nCtorArgs = bScreenOnly ? 0` |
| 1611081 |   51 | `		: pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|       - |   52 | `	ph7_value *pArg;` |
| 2113205 |   53 | `	ph7_class *pClass = 0;` |
|       - |   54 | `	ph7_class_instance *pNew;` |
|       - |   55 | `	/* Resolving the class below can run an AUTOLOADER that throws. Snapshot the boundary` |
|       - |   56 | `	 * rail so the "not found" report can tell a missing class from an autoloader that` |
|       - |   57 | `	 * raised — php propagates the autoloader's exception and reports nothing else, while` |
|       - |   58 | `	 * this arm used to throw its own Error on top of it (uncaught, killing the script` |
|       - |   59 | `	 * right after the real exception had been handled). */` |
| 2113205 |   60 | `	sxi32 nNewBrc = pVm->nBoundaryRc;` |
| 2113205 |   61 | `	const void *pNewRes = (const void *)pVm->pResumeFrame;` |
| 2113205 |   62 | `	pArg = &pTos[-nCtorArgs]; /* Constructor arguments (if available) */` |
|       - |   63 | `	/* Same PHP 8.1 spread-key realignment as OP_CALL: build a per-actual-slot name` |
|       - |   64 | `	 * map when the ctor arg list unpacked string-keyed elements. NEW keeps the` |
|       - |   65 | `	 * class-name slot at pTos (above the args), so pArg is already the correct base;` |
|       - |   66 | `	 * the build also truncates this call's captured runs. */` |
|       - |   67 | `	VmCallArgMap sEffNewMap;` |
|       - |   68 | `	/* The screen pass has no arguments and no runs of its own: building (and` |
|       - |   69 | `	 * truncating) an effective map there would speak for the enclosing call. */` |
| 2667686 |   70 | `	VmCallArgMap *pEffNewMap = bScreenOnly ? 0 : VmEffCallArgMap(pVm,pInstr,pArg,` |
| 1108962 |   71 | `		nCtorArgs > 0 ? (sxu32)nCtorArgs : 0,&sEffNewMap);` |
| 3169803 |   72 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
| 2113201 |   73 | `		const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
| 2113201 |   74 | `		sxu32 nCls = SyBlobLength(&pTos->sBlob);` |
| 2113196 |   75 | `		if( (nCls == sizeof("self")-1 && SyMemcmp(zCls,"self",sizeof("self")-1) == 0)` |
| 2113184 |   76 | `		 \|\| (nCls == sizeof("static")-1 && SyMemcmp(zCls,"static",sizeof("static")-1) == 0)` |
| 2113140 |   77 | `		 \|\| (nCls == sizeof("parent")-1 && SyMemcmp(zCls,"parent",sizeof("parent")-1) == 0) ){` |
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
| 2113137 |   95 | `			pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,` |
|       - |   96 | `				TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|       5 |   97 | `		}` |
| 1056603 |   98 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - |   99 | `		/* Take the base class from the loaded instance */` |
|       5 |  100 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       2 |  101 | `	}` |
| 2113205 |  102 | `	if( pClass == 0 ){` |
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
| 2113165 |  166 | `	}else if( pClass->iFlags & PH7_CLASS_NOINSTANTIATE ){` |
|       - |  167 | `		/* php refuses this one in the create_object handler, so the refusal names the` |
|       - |  168 | `		 * INSTANTIATION and never reaches the constructor — which matters because the` |
|       - |  169 | `		 * class may still declare a private __construct that Reflection prints. Same` |
|       - |  170 | `		 * shape as the enum reject below: no instance, no __destruct. */` |
|       - |  171 | `		SyBlob sErrMsg;` |
|       8 |  172 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       8 |  173 | `		if( pClass->zNewRefusal ){` |
|       - |  174 | `			/* php words a few of these per class — Directory's names dir() as the` |
|       - |  175 | `			 * way to get one — so the spec's own sentence wins when it has one. */` |
|       3 |  176 | `			SyBlobAppend(&sErrMsg,pClass->zNewRefusal,SyStrlen(pClass->zNewRefusal));` |
|       2 |  177 | `		}else{` |
|       5 |  178 | `			SyBlobFormat(&sErrMsg,"Instantiation of class %z is not allowed",&pClass->sName);` |
|       - |  179 | `		}` |
|       8 |  180 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       8 |  181 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  182 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  183 | `		}` |
|       8 |  184 | `		PH7_MemObjRelease(pTos);` |
|       8 |  185 | `		pTos->nIdx = SXU32_HIGH;` |
|       8 |  186 | `		VM_EXIT_BREAK;` |
| 2113159 |  187 | `	}else if( pClass->iFlags & PH7_CLASS_ENUM ){` |
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
| 2113148 |  200 | `	}else if( VmClassStaticDeferPending(pClass)` |
| 1056581 |  201 | `	       && (rc = PH7_VmMaterializeClassStatics(&(*pVm),pClass)) != SXRET_OK ){` |
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
| 2113149 |  222 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       - |  223 | `		/* Constructor visibility (band A #4): __construct now KEEPS its` |
|       - |  224 | `		 * declared protection (PH7_NewClassMethod no longer forces it` |
|       - |  225 | ``		 * public), so `new C()` on a private/protected constructor from`` |
|       - |  226 | `		 * the wrong scope is php's catchable Error. Reflection's` |
|       - |  227 | `		 * newInstance path sets bReflectBypass like method invoke. */` |
| 2113149 |  228 | `		if( pCons && pCons->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - |  229 | `			/* php binds non-public access by the member's DECLARING class, not the` |
|       - |  230 | `			 * instantiated class: a private constructor declared in a base is` |
|       - |  231 | `			 * reachable from that base's own methods even when instantiating a` |
|       - |  232 | ``			 * subclass (`new Child()` inside Base::factory()). __construct is a`` |
|       - |  233 | `			 * method, so pass its declaring class (sFunc.pUserData) — mirroring the` |
|       - |  234 | `			 * method-call visibility check — rather than pClass, whose hAttr lookup` |
|       - |  235 | `			 * for a method name always misses and falls to a wrong exact-class test. */` |
|      37 |  236 | `			ph7_class *pCtorDecl = pCons->sFunc.pUserData ? (ph7_class *)pCons->sFunc.pUserData : pClass;` |
|      37 |  237 | `			if( pVm->bReflectBypass ){` |
|     ! 0 |  238 | `				pVm->bReflectBypass = 0;` |
|      37 |  239 | `			}else if( !PH7_VmClassMemberAccess(&(*pVm),pCtorDecl,&pCons->sFunc.sName,pCons->iProtection,FALSE) ){` |
|       - |  240 | `				SyBlob sErrMsg;` |
|      23 |  241 | `				const char *zVis = pCons->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       - |  242 | `				/* php NAMES the calling scope when there is one (see the method twin in` |
|       - |  243 | `				 * OP_CALL); "global scope" is only for code outside every class. */` |
|      23 |  244 | `				ph7_class *pCtorScope = PH7_VmCallerScopeName(&(*pVm));` |
|      23 |  245 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      23 |  246 | `				if( pCtorScope ){` |
|       9 |  247 | `					SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from scope %z",` |
|       4 |  248 | `						zVis,&pClass->sName,&pCtorScope->sName);` |
|       5 |  249 | `				}else{` |
|      15 |  250 | `					SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from global scope",` |
|       7 |  251 | `						zVis,&pClass->sName);` |
|       - |  252 | `				}` |
|      23 |  253 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       - |  254 | `				/* Pop ctor args + release the class-name operand, leave NULL` |
|       - |  255 | `				 * as the expression value; the fetch-point router lands the` |
|       - |  256 | `				 * throw. No instance was created. */` |
|      23 |  257 | `				if( nCtorArgs > 0 ){` |
|     ! 0 |  258 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  259 | `				}` |
|      23 |  260 | `				PH7_MemObjRelease(pTos);` |
|      23 |  261 | `				pTos->nIdx = SXU32_HIGH;` |
|      23 |  262 | `				VM_EXIT_BREAK;` |
|       - |  263 | `			}` |
|       7 |  264 | `		}` |
| 2113127 |  265 | `		if( bScreenOnly ){` |
|       - |  266 | `			/* Every refusal above has been asked. Leave the class name standing for` |
|       - |  267 | `			 * the real pass and let the arguments run. */` |
| 1004219 |  268 | `			VM_EXIT_BREAK;` |
|       - |  269 | `		}` |
| 1108913 |  270 | `		if( nCtorArgs > 0 ){` |
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
| 1004233 |  281 | `			ph7_class_method *pCtorArgs = pCons;` |
|       - |  282 | `			sxi32 rcDA;` |
| 1004233 |  283 | `			if( pCtorArgs == 0 ){` |
|      19 |  284 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pEffNewMap);` |
| 1004224 |  285 | `			}else if( pCtorArgs->sFunc.iFlags & VM_FUNC_NATIVE ){` |
|       - |  286 | `				/* A native constructor has no compiled formals; its by-ref positions` |
|       - |  287 | `				 * come from the signature-derived mask, the builtin way. */` |
| 1003761 |  288 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
| 1003756 |  289 | `					pCtorArgs->sFunc.pNative ? pCtorArgs->sFunc.pNative->nByRefMask : 0,0,0,pEffNewMap);` |
|  501883 |  290 | `			}else{` |
|     686 |  291 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|     454 |  292 | `					(ph7_vm_func_arg *)SySetBasePtr(&pCtorArgs->sFunc.aArgs),` |
|     227 |  293 | `					SySetUsed(&pCtorArgs->sFunc.aArgs),0,0,0,pEffNewMap);` |
|       - |  294 | `			}` |
| 1004233 |  295 | `			if( rcDA == PH7_ABORT \|\| rcDA == PH7_EXCEPTION ){` |
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
|  502113 |  312 | `		}` |
|       - |  313 | `		/* Create a new class instance */` |
| 1108911 |  314 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
| 1108911 |  315 | `		if( pNew == 0 ){` |
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
| 1108911 |  327 | `		if( pVm->nBoundaryRc != 0 ){` |
|       - |  328 | `			/* A typed property DEFAULT failed its type check during instance-` |
|       - |  329 | `			 * frame creation (parked catchable TypeError): php aborts the` |
|       - |  330 | `			 * construction — no constructor call, no object. Consume the` |
|       - |  331 | `			 * parked status here and route exactly like a constructor throw` |
|       - |  332 | `			 * (the exception object is already registered). */` |
|      36 |  333 | `			sxi32 rcDef = pVm->nBoundaryRc;` |
|       - |  334 | `			sxi32 iDefResumePc;` |
|      36 |  335 | `			pVm->nBoundaryRc = 0;` |
|      36 |  336 | `			PH7_ClassInstanceUnref(pNew);` |
|      36 |  337 | `			if( rcDef == PH7_ABORT ){` |
|     ! 0 |  338 | `				VM_EXIT_ABORT;` |
|       - |  339 | `			}` |
|      36 |  340 | `			if( VmRecordedResume(pVm,&iDefResumePc,pState->pEntryFrame,aInstr) ){` |
|       - |  341 | `				/* This frame's own try caught it in-place: tidy the stack` |
|       - |  342 | `				 * (pop ctor args + release the class-name slot, then drain any` |
|       - |  343 | `				 * abandoned outer-expression operands to the try's base — the` |
|       - |  344 | `				 * class-name slot itself sits above it) and resume. */` |
|      36 |  345 | `				if( nCtorArgs > 0 ){` |
|       3 |  346 | `					VmPopOperand(&pTos,nCtorArgs);` |
|       1 |  347 | `				}` |
|      36 |  348 | `				PH7_MemObjRelease(pTos);` |
|      76 |  349 | `				PH7_RESUME_DRAIN()` |
|      36 |  350 | `				pc = iDefResumePc;` |
|      36 |  351 | `				VM_EXIT_BREAK;` |
|       - |  352 | `			}` |
|     ! 0 |  353 | `			VM_EXIT_EXCEPTION;` |
|       - |  354 | `		}` |
| 1108877 |  355 | `		if( pCons ){` |
|       - |  356 | `			/* Call the class constructor.  Collect args in stack order and` |
|       - |  357 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|       - |  358 | `			 * receiving OP_CALL path runs its named-argument matching` |
|       - |  359 | `			 * (including variadic string-key packing). */` |
| 1104597 |  360 | `			VmCallArgMap *pNewMap = pEffNewMap;` |
|       - |  361 | `			sxi32 rcCons;` |
|       - |  362 | `			SySet aArg; /* ctor argument scratch (the loop's shared set stays with OP_CALL) */` |
| 1104597 |  363 | `			SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
| 2109833 |  364 | `			while( pArg < pTos ){` |
| 1005241 |  365 | `				SySetPut(&aArg,(const void *)&pArg);` |
| 1005241 |  366 | `				pArg++;` |
|       5 |  367 | `			}` |
|       - |  368 | `			/* Too-few-arguments is php's catchable ArgumentCountError, raised` |
|       - |  369 | `			 * by the shared OP_CALL install path this ctor call routes through` |
|       - |  370 | `			 * (was a PHL-only notice here). */` |
| 1104597 |  371 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
| 1104597 |  372 | `			SySetRelease(&aArg); /* scratch dies here, on every exit path */` |
|       - |  373 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
| 1104597 |  374 | `			if( pNew->iRef < 1 ){` |
|     ! 0 |  375 | `				pNew->iRef = 1;` |
|     ! 0 |  376 | `			}` |
| 1104597 |  377 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|       - |  378 | `				/* The constructor raised: the half-constructed object must not` |
|       - |  379 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|       - |  380 | `				 * The class-name operand (and any leftover args) are released by` |
|       - |  381 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|       - |  382 | `				sxi32 iResumePc;` |
|  100216 |  383 | `				PH7_ClassInstanceUnref(pNew);` |
|  100216 |  384 | `				if( rcCons == PH7_ABORT ){` |
|       5 |  385 | `					VM_EXIT_ABORT;` |
|       - |  386 | `				}` |
|  100212 |  387 | `				if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){` |
|       - |  388 | `					/* This frame's own try caught it in-place: tidy the stack` |
|       - |  389 | `					 * (pop ctor args + release the class-name slot, then drain any` |
|       - |  390 | `					 * abandoned outer-expression operands to the try's base — the` |
|       - |  391 | `					 * class-name slot itself sits above it) and resume. */` |
|  100112 |  392 | `					if( nCtorArgs > 0 ){` |
|     103 |  393 | `						VmPopOperand(&pTos,nCtorArgs);` |
|      51 |  394 | `					}` |
|  100112 |  395 | `					PH7_MemObjRelease(pTos);` |
|  500222 |  396 | `					PH7_RESUME_DRAIN()` |
|  100112 |  397 | `					pc = iResumePc;` |
|  100112 |  398 | `					VM_EXIT_BREAK;` |
|       - |  399 | `				}` |
|     101 |  400 | `				VM_EXIT_EXCEPTION;` |
|       - |  401 | `			}` |
|  502189 |  402 | `		}` |
| 1008663 |  403 | `		if( nCtorArgs > 0 ){` |
|       - |  404 | `			/* Pop given arguments */` |
| 1004027 |  405 | `			VmPopOperand(&pTos,nCtorArgs);` |
|  502011 |  406 | `		}` |
| 1008663 |  407 | `		PH7_MemObjRelease(pTos);` |
| 1008663 |  408 | `		pTos->x.pOther = pNew;` |
| 1008663 |  409 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - |  410 | `	}` |
| 1008663 |  411 | `	VM_EXIT_BREAK;` |
|     ! 0 |  412 | `	VM_EXIT_BREAK;` |
| 1056605 |  413 | `}` |
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
|     412 |  426 | `static int VmMemberFetchForWrite(const VmInstr *pInstr)` |
|       5 |  427 | `{` |
|     417 |  428 | `	const VmInstr *pNext = pInstr + 1;` |
|     417 |  429 | `	if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|      15 |  430 | `		int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);` |
|      15 |  431 | `		int bCoalesceW = (pNext->iOp == PH7_OP_NULLC_JMP);` |
|      15 |  432 | `		return !bPlainStore && !bCoalesceW && !VmMemberNextIsRmw(pNext);` |
|       - |  433 | `	}` |
|     403 |  434 | `	if( pInstr->iP2 == PH7_MEMBER_READ ){` |
|     238 |  435 | `		if( pNext->iOp == PH7_OP_STORE_REF ){` |
|       5 |  436 | `			return 1;` |
|       - |  437 | `		}` |
|     234 |  438 | `		if( pNext->iOp == PH7_OP_FOREACH_INIT && pNext->p3 ){` |
|       9 |  439 | `			return (((ph7_foreach_info *)pNext->p3)->iFlags & PH7_4EACH_STEP_REF) != 0;` |
|       - |  440 | `		}` |
|     111 |  441 | `	}` |
|     391 |  442 | `	return 0;` |
|     211 |  443 | `}` |
|       - |  444 | `/*` |
|       - |  445 | `` * php's `Indirect modification of overloaded property C::$p has no effect`: the`` |
|       - |  446 | ` * write-context fetch above landed on a property only __get answers for, so what` |
|       - |  447 | ` * comes back is a VALUE and whatever the rest of the expression writes into it is` |
|       - |  448 | ` * thrown away. php says so and carries on.` |
|       - |  449 | ` *` |
|       - |  450 | ` * PHL had the notice at one site only (a by-reference ARGUMENT, VmBindPropByRef)` |
|       - |  451 | ` * and, worse, the value was not a copy: __get's return still shared the object's` |
|       - |  452 | `` * own nested hashmap by COW, so `$o->a['b'] = 9`, `$o->a[] = 5` and a by-reference`` |
|       - |  453 | ` * foreach modified the object php leaves untouched. Separating it here is what` |
|       - |  454 | ` * makes the write land nowhere.` |
|       - |  455 | ` *` |
|       - |  456 | ` * An ENGINE __get is not this — php routes those through the class's own property` |
|       - |  457 | ` * handler, which hands back the real element (ArrayObject's ARRAY_AS_PROPS reads` |
|       - |  458 | ` * and writes through, in both engines). php stays silent for an OBJECT value too:` |
|       - |  459 | ` * a handle is shared, so nothing about the write is indirect.` |
|       - |  460 | ` */` |
|      28 |  461 | `PH7_PRIVATE void PH7_VmOverloadedPropNotice(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,ph7_value *pVal)` |
|       1 |  462 | `{` |
|      29 |  463 | `	ph7_class_method *pGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|      29 |  464 | `	if( pGet == 0 \|\| (pGet->sFunc.iFlags & VM_FUNC_NATIVE) ){` |
|     ! 0 |  465 | `		return;` |
|       - |  466 | `	}` |
|      29 |  467 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       3 |  468 | `		return;` |
|       - |  469 | `	}` |
|      40 |  470 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - |  471 | `		"Indirect modification of overloaded property %z::$%z has no effect",` |
|      13 |  472 | `		&pClass->sName,pName);` |
|      27 |  473 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      25 |  474 | `		PH7_HashmapCowSeparate(&(*pVm),pVal);` |
|      12 |  475 | `	}` |
|      15 |  476 | `}` |
|       - |  477 | `/*` |
|       - |  478 | ` * Does an OVERLOADED read-modify-write apply to this property access? php runs` |
|       - |  479 | ` * one only when the class answers BOTH sides; the guard means we are already` |
|       - |  480 | ` * inside this property's own __get, where php behaves as if the accessor were` |
|       - |  481 | ` * absent.` |
|       - |  482 | ` */` |
|      38 |  483 | `static int VmMagicRmwEligible(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,const SyString *pName)` |
|       1 |  484 | `{` |
|      53 |  485 | `	return PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) != 0` |
|      33 |  486 | `	    && PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1) != 0` |
|      52 |  487 | `	    && !VmMagicGuardHeld(pVm,(void *)pThis,pName,'g');` |
|       1 |  488 | `}` |
|       - |  489 | `/*` |
|       - |  490 | `` * php's read-modify-write of an OVERLOADED property — `$o->n++`, `--$o->n`,`` |
|       - |  491 | `` * `$o->n += 2`, `$o->n .= 'x'` on a name the instance does not expose. php reads`` |
|       - |  492 | ` * the current value through __get, lets the modify op compute on it, and writes` |
|       - |  493 | ` * the result back through __set; PHL fell through to dynamic-property creation` |
|       - |  494 | ` * instead, so the ordinary shape (a class declaring BOTH accessors) died on` |
|       - |  495 | `` * `Cannot create dynamic property C::$n` — or, when the name IS declared but`` |
|       - |  496 | `` * inaccessible from this scope, on `Cannot access private property C::$n` —`` |
|       - |  497 | ` * for a statement php runs.` |
|       - |  498 | ` *` |
|       - |  499 | ` * The write-back rides the same pending-entry rail property HOOKS use: the` |
|       - |  500 | ` * current value goes into a fresh SCRATCH memobj the modify op mutates in` |
|       - |  501 | ` * place, and the entry makes that op's tail (PH7_HOOK_RMW_WRITEBACK) dispatch` |
|       - |  502 | ` * __set with the computed value.` |
|       - |  503 | ` *` |
|       - |  504 | ` * BOTH accessors are required, which is php's own split: with only __get php` |
|       - |  505 | ` * goes on to CREATE the property (PHL's §10 policy refuses a dynamic property),` |
|       - |  506 | `` * and with only __set it warns `Undefined property` and reads null — neither is`` |
|       - |  507 | ` * this path.` |
|       - |  508 | ` *` |
|       - |  509 | ` * *pOut takes __get's value and *pnScratch the slot the modify op must address;` |
|       - |  510 | ` * the caller does its own stack surgery afterwards, because pName still aliases` |
|       - |  511 | `` * the NAME operand when the name is dynamic (`$o->$k++`) and releasing that`` |
|       - |  512 | ` * operand first would leave it dangling. *pnScratch stays SXU32_HIGH when __get` |
|       - |  513 | ` * threw (nothing is armed — the fetch-point router lands the parked throw and` |
|       - |  514 | ` * php's __set never runs) or when the scratch reservation failed.` |
|       - |  515 | ` */` |
|      28 |  516 | `static void VmMagicRmwArm(` |
|       - |  517 | `	ph7_vm *pVm,` |
|       - |  518 | `	ph7_class_instance *pThis,` |
|       - |  519 | `	ph7_class *pClass,` |
|       - |  520 | `	const SyString *pName,` |
|       - |  521 | `	ph7_value *pOut,` |
|       - |  522 | `	sxu32 *pnScratch,` |
|       - |  523 | `	void *pOwnerStack,` |
|       - |  524 | `	void *pInstrs,` |
|       - |  525 | `	sxu32 nPc` |
|       - |  526 | `	)` |
|       1 |  527 | `{` |
|       - |  528 | `	ph7_value *pScr;` |
|       - |  529 | `	VmHookRmw sRmw;` |
|      29 |  530 | `	*pnScratch = SXU32_HIGH;` |
|      29 |  531 | `	VmMagicGuardPush(pVm,(void *)pThis,pName,'g');` |
|      29 |  532 | `	PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,pName,pOut);` |
|      29 |  533 | `	VmMagicGuardPop(pVm);` |
|      29 |  534 | `	if( pVm->nBoundaryRc != 0 ){` |
|       3 |  535 | `		PH7_MemObjRelease(pOut);` |
|       3 |  536 | `		return;` |
|       - |  537 | `	}` |
|      27 |  538 | `	pScr = PH7_ReserveMemObj(&(*pVm));` |
|      27 |  539 | `	if( pScr == 0 ){` |
|       - |  540 | `		/* OOM: loud allocator diagnostics already fired; the value still stands. */` |
|     ! 0 |  541 | `		return;` |
|       - |  542 | `	}` |
|      27 |  543 | `	PH7_MemObjStore(pOut,pScr);` |
|      27 |  544 | `	sRmw.iKind = VM_HOOK_PEND_RMW_MAGIC;` |
|      27 |  545 | `	sRmw.pThis = pThis;` |
|      27 |  546 | `	sRmw.pAttr = 0;` |
|      27 |  547 | `	sRmw.nBackIdx = SXU32_HIGH;` |
|      27 |  548 | `	sRmw.nScratchIdx = pScr->nIdx;` |
|      27 |  549 | `	SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      27 |  550 | `	SyBlobAppend(&sRmw.sName,(const void *)pName->zString,pName->nByte);` |
|      27 |  551 | `	sRmw.pOwnerStack = pOwnerStack;` |
|      27 |  552 | `	sRmw.pInstrs = pInstrs;` |
|      27 |  553 | `	sRmw.nJmpPc = nPc;  /* the modify op ... */` |
|      27 |  554 | `	sRmw.nPc = nPc;     /* ... is the whole window */` |
|      27 |  555 | `	pThis->iRef++;` |
|      27 |  556 | `	SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      27 |  557 | `	*pnScratch = pScr->nIdx;` |
|      15 |  558 | `}` |
|       - |  559 | `/*` |
|       - |  560 | ` * OP_MEMBER: body moved verbatim from the OP_MEMBER arm of` |
|       - |  561 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  562 | ` */` |
|  324637 |  563 | `PH7_PRIVATE VmOpRc VmExecOpMember(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  564 | `{` |
|  324642 |  565 | `	ph7_value *pTos = pState->pTos;` |
|  324642 |  566 | `	ph7_value *pStack = pState->pStack;` |
|  324642 |  567 | `	VmInstr *aInstr = pState->aInstr;` |
|  324642 |  568 | `	sxi32 pc = pState->pc;` |
|       - |  569 | `	sxi32 rc;` |
|       - |  570 | `	ph7_class_instance *pThis;` |
|       - |  571 | `	ph7_value *pNos;` |
|       - |  572 | `	SyString sName;` |
|  324642 |  573 | `	if( !pInstr->iP1 ){` |
|  221902 |  574 | `		pNos = &pTos[-1];` |
|       - |  575 | `#ifdef UNTRUST` |
|       - |  576 | `		if( pNos < pStack ){` |
|       - |  577 | `			VM_EXIT_ABORT;` |
|       - |  578 | `		}` |
|       - |  579 | `#endif` |
|  221897 |  580 | `		if( pInstr->iP2 == PH7_MEMBER_DEFPATH` |
|  111379 |  581 | `		 && (pNos->iFlags & (MEMOBJ_AUX_DEFPATH\|MEMOBJ_AUX_DEFERRED)) ){` |
|       - |  582 | `			/* D1 commit 2: deferred property arg ($o->p) whose base is itself a deferred lvalue` |
|       - |  583 | ``			 * (a nested chain `$a["k"]->p`, or an undefined base object). Append a property step`` |
|       - |  584 | `			 * to the base's captured path; OP_CALL re-walks it. */` |
|       - |  585 | `			SyString sProp;` |
|      27 |  586 | `			VmDeferredPath *pPath = 0;` |
|      27 |  587 | `			SyStringInitFromBuf(&sProp,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      27 |  588 | `			if( pNos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|      27 |  589 | `				pPath = (VmDeferredPath *)pNos->x.pOther;` |
|      27 |  590 | `				VmDeferPathPushProp(pPath,&sProp);` |
|      14 |  591 | `			}else{` |
|       - |  592 | `				SyString sRootName;` |
|     ! 0 |  593 | `				SyStringInitFromBuf(&sRootName,(const char *)pNos->x.pOther,` |
|       - |  594 | `					pNos->x.pOther ? SyStrlen((const char *)pNos->x.pOther) : 0);` |
|     ! 0 |  595 | `				pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|     ! 0 |  596 | `				if( pPath ){` |
|     ! 0 |  597 | `					pNos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name */` |
|     ! 0 |  598 | `					pNos->x.pOther = pPath;` |
|     ! 0 |  599 | `					pNos->iFlags \|= MEMOBJ_AUX_DEFPATH;` |
|     ! 0 |  600 | `					pNos->nIdx = SXU32_HIGH;` |
|     ! 0 |  601 | `					VmDeferPathPushProp(pPath,&sProp);` |
|     ! 0 |  602 | `				}` |
|       - |  603 | `			}` |
|      27 |  604 | `			VmPopOperand(&pTos,1); /* drop the property name; the carrier stays as pTos */` |
|      27 |  605 | `			VM_EXIT_BREAK;` |
|       - |  606 | `		}` |
|  221876 |  607 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - |  608 | `			ph7_class *pClass;` |
|       - |  609 | `			/* Class already instantiated */` |
|  221774 |  610 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       - |  611 | `			/* Point to the instantiated class */` |
|  221774 |  612 | `			pClass = pThis->pClass;` |
|       - |  613 | `			/* Extract attribute name first */` |
|  221774 |  614 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|  221774 |  615 | `			if( PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
|       - |  616 | `				/* __PHP_Incomplete_Class: every property access and method call is` |
|       - |  617 | `				 * php's incomplete-object diagnostic — the carrier's OWN entries are` |
|       - |  618 | `				 * for the engine's surfaces only. A READ (isset/empty/?? included)` |
|       - |  619 | `				 * is an E_WARNING answering NULL; every WRITE shape and unset() is` |
|       - |  620 | `				 * a catchable Error; a method call is its own Error, raised where` |
|       - |  621 | `				 * the undefined-method twin raises (before any argument runs). The` |
|       - |  622 | `				 * DEFPATH pre-pass defers like a MISSING property would — the slot` |
|       - |  623 | `				 * fast path must not hand out the carrier's storage — so the by-ref` |
|       - |  624 | `				 * resolve (VmBindPropByRef) raises the Error and the by-value` |
|       - |  625 | `				 * re-drive lands back here in READ context for the warning.` |
|       - |  626 | ``				 * `??=` reads before it refuses, so it takes BOTH, php's order. */`` |
|      37 |  627 | `				VmInstr *pIncNext = pInstr + 1;` |
|      37 |  628 | `				if( pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|       - |  629 | `					SyString sIncProp;` |
|       5 |  630 | `					VmDeferredPath *pIncPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|       5 |  631 | `					SyStringInitFromBuf(&sIncProp,sName.zString,sName.nByte);` |
|       5 |  632 | `					if( pIncPath && VmDeferPathPushProp(pIncPath,&sIncProp) == SXRET_OK ){` |
|       5 |  633 | `						VmPopOperand(&pTos,1);       /* drop the property name */` |
|       5 |  634 | `						pThis->iRef++;` |
|       5 |  635 | `						PH7_MemObjRelease(pTos);     /* collapse the object slot into the carrier */` |
|       5 |  636 | `						pTos->x.pOther = pIncPath;` |
|       5 |  637 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|       5 |  638 | `						pTos->nIdx = SXU32_HIGH;` |
|       5 |  639 | `						PH7_ClassInstanceUnref(pThis);` |
|       5 |  640 | `						VM_EXIT_BREAK;` |
|       - |  641 | `					}` |
|     ! 0 |  642 | `					if( pIncPath ){` |
|     ! 0 |  643 | `						VmFreeDeferredPath(pIncPath);` |
|     ! 0 |  644 | `					}` |
|       - |  645 | `					/* allocation failure (or a temporary base below): the read` |
|       - |  646 | `					 * verdict is all that is left — fall through to warn + NULL. */` |
|     ! 0 |  647 | `				}` |
|      33 |  648 | `				int bIncCall = (pInstr->iP2 == PH7_MEMBER_METHOD);` |
|      33 |  649 | `				int bIncCoalW = (pInstr->iP2 == PH7_MEMBER_WRITE && pIncNext->iOp == PH7_OP_NULLC_JMP);` |
|      65 |  650 | `				int bIncModify = pInstr->iP2 != PH7_MEMBER_DEFPATH` |
|      63 |  651 | `					&& (pInstr->iP2 == PH7_MEMBER_UNSET` |
|      31 |  652 | `					 \|\| pInstr->iP2 == PH7_MEMBER_WRITE` |
|      25 |  653 | `					 \|\| pInstr->iP2 == PH7_MEMBER_REF_TARGET` |
|      19 |  654 | `					 \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|      18 |  655 | `					 \|\| VmMemberNextIsWrite(pIncNext)` |
|      18 |  656 | `					 \|\| VmMemberFetchForWrite(pInstr));` |
|      33 |  657 | `				if( bIncCall ){` |
|       - |  658 | `					SyBlob sIncErr;` |
|       - |  659 | `					sxi32 rcInc;` |
|       3 |  660 | `					SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|       3 |  661 | `					PH7_VmIncompleteMsg(&(*pVm),pThis,"call a method",&sIncErr);` |
|       3 |  662 | `					VmPopOperand(&pTos,1);` |
|       3 |  663 | `					PH7_MemObjRelease(pTos);` |
|       3 |  664 | `					pTos->nIdx = SXU32_HIGH;` |
|       4 |  665 | `					rcInc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sIncErr),` |
|       1 |  666 | `						SyBlobLength(&sIncErr));` |
|       3 |  667 | `					SyBlobRelease(&sIncErr);` |
|       3 |  668 | `					if( rcInc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  669 | `					rc = rcInc;` |
|       3 |  670 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  671 | `				}` |
|      31 |  672 | `				if( bIncCoalW ){` |
|       3 |  673 | `					PH7_VmIncompleteAccessWarn(&(*pVm),pThis,0);` |
|       1 |  674 | `				}` |
|      31 |  675 | `				if( bIncModify ){` |
|       - |  676 | `					SyBlob sIncErr;` |
|      17 |  677 | `					SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|      17 |  678 | `					PH7_VmIncompleteMsg(&(*pVm),pThis,"modify a property",&sIncErr);` |
|      17 |  679 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sIncErr));` |
|       9 |  680 | `				}else{` |
|      15 |  681 | `					PH7_VmIncompleteAccessWarn(&(*pVm),pThis,0);` |
|       - |  682 | `				}` |
|      31 |  683 | `				VmPopOperand(&pTos,1);   /* pop the attribute name */` |
|      31 |  684 | `				PH7_MemObjRelease(pTos); /* the object slot becomes the NULL answer */` |
|      31 |  685 | `				pTos->nIdx = SXU32_HIGH;` |
|      31 |  686 | `				VM_EXIT_BREAK;` |
|       - |  687 | `			}` |
|  221738 |  688 | `			if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - |  689 | `				/* Method call */` |
|  115600 |  690 | `				ph7_class_method *pMeth = 0;` |
|  115600 |  691 | `				if( sName.nByte > 0 ){` |
|       - |  692 | `					/* Extract the target method */` |
|  115600 |  693 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|   57799 |  694 | `				}` |
|  115600 |  695 | `				if( pMeth == 0 ){` |
|      60 |  696 | `					ph7_class_method *pCallMagic = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|      60 |  697 | `					if( pCallMagic ){` |
|       - |  698 | `						/* php: a missing method dispatches __call($name, $args) — the` |
|       - |  699 | `						 * args are only collected by the following OP_CALL, so stash the` |
|       - |  700 | `						 * receiver + class + original name and MARK the callee slot` |
|       - |  701 | `						 * (band A #3b; pre-fix the name-only call discarded everything` |
|       - |  702 | `						 * and the call site failed with "Invalid function name"). Stack:` |
|       - |  703 | `						 * pop the method name, then the receiver slot becomes the marked` |
|       - |  704 | `						 * carrier OP_CALL routes through the packing body. */` |
|      54 |  705 | `						VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pThis,pClass,&sName);` |
|      54 |  706 | `						if( pPend == 0 ){` |
|     ! 0 |  707 | `							VM_EXIT_ABORT;` |
|       - |  708 | `						}` |
|      54 |  709 | `						VmPopOperand(&pTos,1);` |
|      54 |  710 | `						PH7_MemObjRelease(pTos);` |
|      54 |  711 | `						pTos->x.pOther = pPend;` |
|      54 |  712 | `						pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|      28 |  713 | `					}else{` |
|       - |  714 | `						{` |
|       - |  715 | `							/* php raises a catchable Error here; PH7 printed a notice and CARRIED ON` |
|       - |  716 | `							 * with NULL, so the call silently produced nothing. */` |
|       - |  717 | `							SyBlob sErrM;` |
|       - |  718 | `							sxi32 rcErr;` |
|       7 |  719 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       7 |  720 | `							SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",&pClass->sName,&sName);` |
|       7 |  721 | `							VmPopOperand(&pTos,1);` |
|       7 |  722 | `							PH7_MemObjRelease(pTos);` |
|       7 |  723 | `							pTos->nIdx = SXU32_HIGH;` |
|      10 |  724 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       3 |  725 | `								SyBlobLength(&sErrM));` |
|       7 |  726 | `							SyBlobRelease(&sErrM);` |
|       7 |  727 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       7 |  728 | `							rc = rcErr;` |
|       7 |  729 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  730 | `						}` |
|       - |  731 | `					}` |
|      28 |  732 | `				}else{` |
|  115542 |  733 | `					ph7_class_method *pDeniedCall = 0;` |
|  115542 |  734 | `					int bDenied = 0;` |
|       - |  735 | `					/* php decides a method's visibility against the class that OWNS it,` |
|       - |  736 | `					 * which for a trait method is the class that composed it — a trait is` |
|       - |  737 | `					 * a compile-time construct php has flattened away by now. Deciding` |
|       - |  738 | `					 * against the trait instead made every rule a question of who USES it:` |
|       - |  739 | `					 * a protected trait method was denied to a SUBCLASS of the composing` |
|       - |  740 | `					 * class (not a trait user itself) and granted to an unrelated class` |
|       - |  741 | `					 * that happened to use the same trait. */` |
|  115542 |  742 | `					ph7_class *pOwner = 0;` |
|  115537 |  743 | `					if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC` |
|   57831 |  744 | `					 && !PH7_VmClassMemberAccess(&(*pVm),` |
|     112 |  745 | `						(pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth)),` |
|      56 |  746 | `						&sName,pMeth->iProtection,FALSE) ){` |
|      55 |  747 | `						int bRebound = 0;` |
|      55 |  748 | `						if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - |  749 | `							/* php: when the instance's class SHADOWS the caller's` |
|       - |  750 | `							 * private method (child redeclares private m), code in` |
|       - |  751 | `							 * the caller's class dispatches its OWN private m, not` |
|       - |  752 | `							 * the shadow. Rebind to the calling scope's method when` |
|       - |  753 | `							 * that scope declares a private one of this name. */` |
|      47 |  754 | `							VmFrame *pFrameLocal = VmSkipExceptionFrames(pVm->pFrame);` |
|      47 |  755 | `							ph7_vm_func *pCallerFunc = (ph7_vm_func *)pFrameLocal->pUserData;` |
|      47 |  756 | `							if( pCallerFunc && (pCallerFunc->iFlags & VM_FUNC_CLASS_METHOD) && pCallerFunc->pUserData ){` |
|       6 |  757 | `								ph7_class *pScope = (ph7_class *)pCallerFunc->pUserData;` |
|       6 |  758 | `								ph7_class_method *pOwn = PH7_ClassExtractMethod(pScope,sName.zString,sName.nByte);` |
|       4 |  759 | `								if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       6 |  760 | `								 && (ph7_class *)pOwn->sFunc.pUserData == pScope ){` |
|       6 |  761 | `									pMeth = pOwn;` |
|       6 |  762 | `									bRebound = 1;` |
|       2 |  763 | `								}` |
|       2 |  764 | `							}` |
|      22 |  765 | `						}` |
|      55 |  766 | `						if( !bRebound ){` |
|       - |  767 | `							/* Inaccessible from this scope: php routes through __call when` |
|       - |  768 | `							 * declared (band A #3b); without it the refusal is raised right` |
|       - |  769 | `							 * here, beside the undefined-method and abstract-method twins. */` |
|      51 |  770 | `							pDeniedCall = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|      51 |  771 | `							bDenied = pDeniedCall == 0;` |
|      24 |  772 | `						}` |
|      26 |  773 | `					}` |
|  115542 |  774 | `					if( bDenied ){` |
|       - |  775 | ``						/* php's visibility Error for `$o->m()`. It is raised HERE, at the`` |
|       - |  776 | `						 * member resolution, and not left to OP_CALL: the method OP_CALL` |
|       - |  777 | `						 * would re-derive is looked up by the FUNCTION's own name against` |
|       - |  778 | `						 * its declaring class, which a trait adaptation splits away from` |
|       - |  779 | ``						 * the entry this lookup actually chose — `hi as private pHi` gave`` |
|       - |  780 | ``						 * OP_CALL the trait's public `hi`, so the private alias ran from`` |
|       - |  781 | `						 * global scope in silence. The entry that answered is right here,` |
|       - |  782 | `						 * with its composed protection.` |
|       - |  783 | `						 *` |
|       - |  784 | ``						 * php names the method as the CALL SPELLS it (`$o->PQ()` on a`` |
|       - |  785 | ``						 * private `pQ()` reports `PQ`) and the class that OWNS the method,`` |
|       - |  786 | `						 * which for a trait method is the composing class. */` |
|       - |  787 | `						SyBlob sErrM;` |
|       - |  788 | `						sxi32 rcErr;` |
|      41 |  789 | `						ph7_class *pScope = PH7_VmCallerScopeName(&(*pVm));` |
|      60 |  790 | `						const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      19 |  791 | `							? "private" : "protected";` |
|      41 |  792 | `						SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      41 |  793 | `						if( pScope ){` |
|       7 |  794 | `							SyBlobFormat(&sErrM,"Call to %s method %z::%z() from scope %z",` |
|       3 |  795 | `								zVis,&pOwner->sName,&sName,&pScope->sName);` |
|       4 |  796 | `						}else{` |
|      35 |  797 | `							SyBlobFormat(&sErrM,"Call to %s method %z::%z() from global scope",` |
|      16 |  798 | `								zVis,&pOwner->sName,&sName);` |
|       - |  799 | `						}` |
|      41 |  800 | `						VmPopOperand(&pTos,1);` |
|      41 |  801 | `						PH7_MemObjRelease(pTos);` |
|      41 |  802 | `						pTos->nIdx = SXU32_HIGH;` |
|      60 |  803 | `						rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      19 |  804 | `							SyBlobLength(&sErrM));` |
|      41 |  805 | `						SyBlobRelease(&sErrM);` |
|      41 |  806 | `						if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      41 |  807 | `						rc = rcErr;` |
|      51 |  808 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  809 | `					}` |
|  115504 |  810 | `					if( pDeniedCall ){` |
|      11 |  811 | `						VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pThis,pClass,&sName);` |
|      11 |  812 | `						if( pPend == 0 ){` |
|     ! 0 |  813 | `							VM_EXIT_ABORT;` |
|       - |  814 | `						}` |
|      11 |  815 | `						VmPopOperand(&pTos,1);` |
|      11 |  816 | `						PH7_MemObjRelease(pTos);` |
|      11 |  817 | `						pTos->x.pOther = pPend;` |
|      11 |  818 | `						pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|       6 |  819 | `					}else{` |
|       - |  820 | `						/* Push method name on the stack, MARKED as already screened: the` |
|       - |  821 | `						 * decision above was made against the entry this lookup chose,` |
|       - |  822 | `						 * which is the only place a trait adaptation's composed` |
|       - |  823 | `						 * protection is visible. */` |
|  115494 |  824 | `						PH7_MemObjRelease(pTos);` |
|  115494 |  825 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|  115494 |  826 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|  115494 |  827 | `						pTos->iFlags \|= MEMOBJ_AUX_MEMBERCALL;` |
|       - |  828 | `					}` |
|       - |  829 | `				}` |
|  115556 |  830 | `				pTos->nIdx = SXU32_HIGH;` |
|   57782 |  831 | `			}else{` |
|       - |  832 | `				/* Attribute access. iP2: 0 = read, 2 = unset, 3 = isset, 4 = empty. */` |
|  106143 |  833 | `				VmClassAttr *pObjAttr = 0;` |
|  106143 |  834 | `				SyHashEntry *pEntry = 0;` |
|  106138 |  835 | `				if( sName.nByte > 0 && sName.zString[0] == 0` |
|   53095 |  836 | `				 && !PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
|       - |  837 | `					/* php refuses a property name beginning with a NUL outright:` |
|       - |  838 | ``					 * `Cannot access property starting with "\0"`. Those bytes are`` |
|       - |  839 | `					 * the ENGINE's private mangling — "\0Cls\0p" is C1::$p and` |
|       - |  840 | `					 * "\0*\0p" is a protected slot — so a script that could write one` |
|       - |  841 | `					 * would forge a private member of any class it names, and every` |
|       - |  842 | `					 * display surface would then render the forgery as the real thing.` |
|       - |  843 | `					 * Two rules ride with it, both probe-verified: a MAGIC accessor` |
|       - |  844 | `					 * wins (php dispatches __get/__set/__isset/__unset with the raw` |
|       - |  845 | `					 * name and says nothing), and a LOOKUP context is silent — isset()` |
|       - |  846 | ``					 * is false, empty() true, `??` takes the default — which is what`` |
|       - |  847 | `					 * the existing miss handling below already answers. The refusal` |
|       - |  848 | `					 * does not depend on whether such a property exists: php raises it` |
|       - |  849 | `					 * on the NAME, before any lookup, and the one place a NUL-keyed` |
|       - |  850 | `					 * property legitimately lives is the __PHP_Incomplete_Class` |
|       - |  851 | `					 * carrier, whose own gate above has already answered for it.` |
|       - |  852 | ``					 * A METHOD name is not covered — php lets `$o->{"\0m"}()` reach`` |
|       - |  853 | `					 * the ordinary undefined-method Error — so this sits in the` |
|       - |  854 | `					 * attribute branch only. */` |
|       - |  855 | `					const char *zNulMagic;` |
|      51 |  856 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       5 |  857 | `						zNulMagic = "__unset";` |
|      49 |  858 | `					}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|      46 |  859 | `					       \|\| VmMemberNextIsWrite(pInstr + 1) ){` |
|      19 |  860 | `						zNulMagic = "__set";` |
|      10 |  861 | `					}else{` |
|      29 |  862 | `						zNulMagic = "__get";` |
|       - |  863 | `					}` |
|      50 |  864 | `					if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      47 |  865 | `					 && PH7_ClassExtractMethod(pClass,zNulMagic,(sxu32)SyStrlen(zNulMagic)) == 0 ){` |
|       - |  866 | `						SyBlob sNulErr;` |
|       - |  867 | `						sxi32 rcNul;` |
|      33 |  868 | `						SyBlobInit(&sNulErr,&pVm->sAllocator);` |
|      33 |  869 | `						SyBlobAppend(&sNulErr,"Cannot access property starting with \"\\0\"",` |
|       - |  870 | `							sizeof("Cannot access property starting with \"\\0\"")-1);` |
|      33 |  871 | `						VmPopOperand(&pTos,1);   /* pop the attribute name */` |
|      33 |  872 | `						PH7_MemObjRelease(pTos); /* the object slot becomes the NULL answer */` |
|      33 |  873 | `						pTos->nIdx = SXU32_HIGH;` |
|      49 |  874 | `						rcNul = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sNulErr),` |
|      16 |  875 | `							SyBlobLength(&sNulErr));` |
|      33 |  876 | `						SyBlobRelease(&sNulErr);` |
|      33 |  877 | `						if( rcNul == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      33 |  878 | `						rc = rcNul;` |
|      33 |  879 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  880 | `					}` |
|       9 |  881 | `				}` |
|       - |  882 | `				/* Extract the target attribute. The EMPTY name is a real property` |
|       - |  883 | ``				 * name in php — `$o->{''} = 1` creates one and `$o->{''}` reads it`` |
|       - |  884 | `				 * back — and it is the one name SyHashGet cannot answer for, so it` |
|       - |  885 | `				 * takes the list-walking lookup; every other name keeps the direct` |
|       - |  886 | `				 * hash probe this path has always made. */` |
|  106111 |  887 | `				if( sName.nByte > 0 ){` |
|  106103 |  888 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|   53054 |  889 | `				}else{` |
|       9 |  890 | `					pEntry = PH7_ClassInstanceAttrEntry(pThis,sName.zString,0);` |
|       - |  891 | `				}` |
|  106111 |  892 | `				if( pEntry ){` |
|       - |  893 | `					/* Point to the attribute value */` |
|  105463 |  894 | `					pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|   52729 |  895 | `				}` |
|  106106 |  896 | `				if( pObjAttr` |
|  105782 |  897 | `				 && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT))` |
|   52743 |  898 | `				 && PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,` |
|      18 |  899 | `					pObjAttr->pAttr->iProtection,FALSE) ){` |
|       - |  900 | `					/* A static property (or a constant) belongs to the CLASS: php does` |
|       - |  901 | `					 * not find it through an instance at all. It notices the attempt —` |
|       - |  902 | ``					 * `Accessing static property C::$s as non static` — and then treats`` |
|       - |  903 | `					 * the name as an ordinary MISSING property: a read warns and answers` |
|       - |  904 | `					 * null, isset() is false, a write goes to a dynamic property (which` |
|       - |  905 | `					 * PHL rejects by the §10 policy, like any other undeclared write).` |
|       - |  906 | `					 * PHL's instance table carries an entry for every declared member` |
|       - |  907 | ``					 * (statics share the class slot), so `$o->s` used to READ and — far`` |
|       - |  908 | `					 * worse — WRITE the class's own static in silence. isset()/empty()` |
|       - |  909 | `					 * pass silently, as php's do. */` |
|      16 |  910 | `					if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      15 |  911 | `					 && pInstr->iP2 != PH7_MEMBER_DEFPATH ){` |
|       - |  912 | `						/* Silent where php is silent: isset()/empty(), and any class` |
|       - |  913 | `						 * that declares the MAGIC accessor this context would dispatch` |
|       - |  914 | ``						 * (php passes `silent = ce->__get/__set/__unset != NULL` to its`` |
|       - |  915 | `						 * property-offset lookup). Once per access, too: the deferred` |
|       - |  916 | `						 * call-argument PRE-PASS (PH7_MEMBER_DEFPATH) re-runs this op for` |
|       - |  917 | ``						 * the same source `$o->s` and the value pass carries the notice`` |
|       - |  918 | `						 * — the BY-REF form has no value pass and notices at its own` |
|       - |  919 | `						 * site (VmBindPropByRef). */` |
|       - |  920 | `						const char *zMagic;` |
|       7 |  921 | `						if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       3 |  922 | `							zMagic = "__unset";` |
|       6 |  923 | `						}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|       5 |  924 | `						       \|\| VmMemberNextIsWrite(pInstr + 1) ){` |
|       3 |  925 | `							zMagic = "__set";` |
|       2 |  926 | `						}else{` |
|       3 |  927 | `							zMagic = "__get";` |
|       - |  928 | `						}` |
|       7 |  929 | `						if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) == 0 ){` |
|       7 |  930 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - |  931 | `								"Accessing static property %z::$%z as non static",` |
|       4 |  932 | `								&pClass->sName,&pObjAttr->pAttr->sName);` |
|       2 |  933 | `						}` |
|       3 |  934 | `					}` |
|      17 |  935 | `					pEntry = 0;` |
|      17 |  936 | `					pObjAttr = 0;` |
|       8 |  937 | `				}` |
|  106111 |  938 | `				if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - |  939 | `					/* unset($o->prop): remove the property entirely so it disappears from` |
|       - |  940 | `					 * foreach / json_encode / get_object_vars / (array) — matching PHP (a value-only` |
|       - |  941 | `					 * release would leave a zombie null entry). Leave a NULL constant on the stack so` |
|       - |  942 | `					 * the trailing generic unset() builtin is a no-op (mirrors LOAD_IDX iP2=5).` |
|       - |  943 | `					 * php dispatches __unset($name) for a MISSING or INACCESSIBLE property` |
|       - |  944 | `					 * (band A #3b — pre-fix an inaccessible private was silently DELETED from` |
|       - |  945 | `					 * outside the class); without __unset, an inaccessible unset is php's` |
|       - |  946 | `					 * catchable "Cannot access ..." Error and a missing one stays a no-op. */` |
|      51 |  947 | `					int bUnsAccessible = pEntry ? PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) : 0;` |
|      53 |  948 | `					if( pEntry && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) != 0 ){` |
|       - |  949 | `						/* php 8.4: a hooked property (virtual or backed) can never be` |
|       - |  950 | `						 * unset — catchable Error, even from inside its own hook body` |
|       - |  951 | `						 * (probe-verified). */` |
|       - |  952 | `						SyBlob sErrMsg;` |
|       5 |  953 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       5 |  954 | `						SyBlobFormat(&sErrMsg,"Cannot unset hooked property %z::$%z",` |
|       4 |  955 | `							&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       5 |  956 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      49 |  957 | `					}else if( pEntry && bUnsAccessible ){` |
|      31 |  958 | `						PH7_VmReleaseInstanceAttr(&(*pVm),pObjAttr);` |
|      31 |  959 | `						SyHashDeleteEntry2(pEntry);` |
|      16 |  960 | `					}else{` |
|      17 |  961 | `						ph7_class_method *pUnsetMagic = PH7_ClassExtractMethod(pClass,"__unset",sizeof("__unset")-1);` |
|      17 |  962 | `						if( pUnsetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'u') ){` |
|      15 |  963 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'u');` |
|      15 |  964 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__unset",sizeof("__unset")-1,&sName,0);` |
|      15 |  965 | `							VmMagicGuardPop(pVm);` |
|       9 |  966 | `						}else if( pEntry ){` |
|       - |  967 | `							/* Inaccessible and no __unset: php's catchable Error. Parked on` |
|       - |  968 | `							 * the boundary rail; the op completes benignly and the` |
|       - |  969 | `							 * fetch-point router lands it. */` |
|       - |  970 | `							SyBlob sErrMsg;` |
|       3 |  971 | `							const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       3 |  972 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 |  973 | `							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",zVis,&pClass->sName,&sName);` |
|       3 |  974 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       1 |  975 | `						}` |
|       - |  976 | `						/* Missing property without __unset: silent no-op (php). */` |
|       - |  977 | `					}` |
|      51 |  978 | `					VmPopOperand(&pTos,1);    /* pop the attribute name */` |
|      51 |  979 | `					PH7_MemObjRelease(pTos);  /* release the object on the stack ($o's stack ref) */` |
|      51 |  980 | `					pTos->nIdx = SXU32_HIGH;  /* NULL constant */` |
|      51 |  981 | `					VM_EXIT_BREAK;` |
|       - |  982 | `				}` |
|  106063 |  983 | `				if( pObjAttr == 0 ){` |
|       - |  984 | `					/* Member not present on the instance and the next instruction writes/modifies it` |
|       - |  985 | ``					 * (store, array-append/keyed-write, `??=`, ++/--, or a compound-assign — see`` |
|       - |  986 | `					 * VmMemberNextIsWrite; the compiler always emits a terminating PH7_OP_DONE so` |
|       - |  987 | `					 * pInstr+1 is in-bounds). Create the property so the operation lands on a real slot` |
|       - |  988 | ``					 * (PHP auto-vivifies a fresh property for all these forms, not just `=`):`` |
|       - |  989 | `					 *   - a DECLARED prop that was unset() and is re-assigned → recreate it` |
|       - |  990 | `					 *     (PHP re-appends it at the end), OR` |
|       - |  991 | `					 *   - a dynamic prop on a dynamic-allowing class (stdClass).` |
|       - |  992 | `					 * The created slot is NULL with a real nIdx, which the modify-op then vivifies` |
|       - |  993 | `					 * (NULL→array for [], NULL→0/"" for ++/.=) and writes back in place.` |
|       - |  994 | `					 * Two signals: the compiler tags the member itself iP2=PH7_MEMBER_WRITE when it is` |
|       - |  995 | ``					 * the base of a write-subscript / `??=` (the modify-op is not the immediately-next`` |
|       - |  996 | `					 * instruction there), and for ++/--/compound-assign/store the next opcode is the` |
|       - |  997 | `					 * modify-op directly (VmMemberNextIsWrite). */` |
|     659 |  998 | `					VmInstr *pNext = pInstr + 1;` |
|     654 |  999 | `					if( pInstr->iP2 == PH7_MEMBER_WRITE \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|     470 | 1000 | `					 \|\| VmMemberNextIsWrite(pNext) ){` |
|     195 | 1001 | `						ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pThis->pClass,sName.zString,sName.nByte);` |
|     195 | 1002 | `						if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       9 | 1003 | `							VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pObjAttr);` |
|       5 | 1004 | `						}else{` |
|       - | 1005 | `							/* php 8 semantics (band A #3b): a PLAIN store to a missing` |
|       - | 1006 | `							 * property dispatches __set($name,$value) when declared —` |
|       - | 1007 | `							 * the value only exists at the following OP_STORE, so park` |
|       - | 1008 | `							 * the receiver+name for it (one-instruction lifetime; the` |
|       - | 1009 | `							 * guard makes a same-name write inside __set fall through` |
|       - | 1010 | `							 * to dynamic creation, like php). Without __set — or for a` |
|       - | 1011 | `							 * subscript-write base / read-modify-write, which php does` |
|       - | 1012 | `							 * NOT route through __set — create a dynamic property on` |
|       - | 1013 | `							 * ANY class with the 8.2 deprecation (suppressed for` |
|       - | 1014 | `							 * stdClass and #[AllowDynamicProperties]); a readonly` |
|       - | 1015 | `							 * class raises php's catchable Error instead. */` |
|     187 | 1016 | `							ph7_class_method *pSetMagic = 0;` |
|     187 | 1017 | `							ph7_class_method *pCoalIsset = 0, *pCoalGet = 0, *pCoalSet = 0;` |
|     187 | 1018 | `							int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);` |
|     187 | 1019 | `							if( bPlainStore \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|     123 | 1020 | `								pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|      59 | 1021 | `							}` |
|     187 | 1022 | `							if( pSetMagic && pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 1023 | `								/* php dispatches __set($name, element) for a missing` |
|       - | 1024 | `								 * destructuring target, but the element value only exists` |
|       - | 1025 | `								 * at the following OP_LOAD_LIST — the dispatch is` |
|       - | 1026 | `								 * unsupported (recorded residual). Leave the miss: the` |
|       - | 1027 | `								 * property stays uncreated, matching php's observable` |
|       - | 1028 | `								 * state (its __set did not store either). */` |
|     187 | 1029 | `							}else if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){` |
|      29 | 1030 | `								pThis->iRef++;` |
|      29 | 1031 | `								pVm->pMagicSetThis = pThis;` |
|      29 | 1032 | `								SyBlobReset(&pVm->sMagicSetName);` |
|      29 | 1033 | `								SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       - | 1034 | `								/* pObjAttr stays NULL; the miss path below stays silent. */` |
|     172 | 1035 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|      65 | 1036 | `							 && pNext->iOp == PH7_OP_NULLC_JMP` |
|      43 | 1037 | `							 && ((pCoalIsset = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1)) != 0` |
|       8 | 1038 | `							  \|\| (pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)) != 0` |
|       5 | 1039 | `							  \|\| (pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1)) != 0) ){` |
|       - | 1040 | ``								/* `$o->p ??= v` on a missing property with magic accessors:`` |
|       - | 1041 | `								 * php consults __isset first when declared — false means` |
|       - | 1042 | `								 * assign directly through __set with NO __get call (the` |
|       - | 1043 | `								 * test value stays null); true (or no __isset) means __get` |
|       - | 1044 | `								 * provides the test value. A COAL_MAGIC entry is pushed` |
|       - | 1045 | `								 * for the OP_NULLC_STORE at the jump target; the` |
|       - | 1046 | `								 * fetch-point sweep drops it when the short-circuit jump` |
|       - | 1047 | `								 * skips the assign or a throw abandons the RHS. Without` |
|       - | 1048 | `								 * __set the assign side keeps the pre-existing loud` |
|       - | 1049 | `								 * "Cannot perform assignment" path (php would create a` |
|       - | 1050 | `								 * dynamic property there — recorded residual). Note the` |
|       - | 1051 | `								 * condition's short-circuit assignment chain: a hit on an` |
|       - | 1052 | `								 * earlier method leaves the later pointers unresolved, so` |
|       - | 1053 | `								 * re-resolve the leftovers here (each is one hash probe,` |
|       - | 1054 | `								 * only on this ??=-miss path). */` |
|       - | 1055 | `								ph7_value sTest;` |
|       7 | 1056 | `								int bMiss = 0; /* __isset said false: skip __get, test value stays null */` |
|       7 | 1057 | `								if( pCoalIsset != 0 ){` |
|       5 | 1058 | `									pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       2 | 1059 | `								}` |
|       7 | 1060 | `								if( pCoalIsset != 0 \|\| pCoalGet != 0 ){` |
|       7 | 1061 | `									pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|       3 | 1062 | `								}` |
|       7 | 1063 | `								PH7_MemObjInit(pVm,&sTest);` |
|       7 | 1064 | `								if( pCoalIsset && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 1065 | `									ph7_value sIssetRet;` |
|       5 | 1066 | `									PH7_MemObjInit(pVm,&sIssetRet);` |
|       5 | 1067 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|       5 | 1068 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|       5 | 1069 | `									VmMagicGuardPop(pVm);` |
|       5 | 1070 | `									PH7_MemObjToBool(&sIssetRet);` |
|       5 | 1071 | `									bMiss = sIssetRet.x.iVal == 0;` |
|       5 | 1072 | `									PH7_MemObjRelease(&sIssetRet);` |
|       2 | 1073 | `								}` |
|       7 | 1074 | `								if( !bMiss && pCoalGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       5 | 1075 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       5 | 1076 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sTest);` |
|       5 | 1077 | `									VmMagicGuardPop(pVm);` |
|       2 | 1078 | `								}` |
|       6 | 1079 | `								if( pCoalSet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s')` |
|       7 | 1080 | `								 && pVm->nBoundaryRc == 0 ){` |
|       - | 1081 | `									VmHookRmw sPend;` |
|       7 | 1082 | `									sPend.iKind = VM_HOOK_PEND_COAL_MAGIC;` |
|       7 | 1083 | `									sPend.pThis = pThis;` |
|       7 | 1084 | `									sPend.pAttr = 0;` |
|       7 | 1085 | `									sPend.nBackIdx = SXU32_HIGH;` |
|       7 | 1086 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|       7 | 1087 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|       7 | 1088 | `									SyBlobAppend(&sPend.sName,(const void *)sName.zString,sName.nByte);` |
|       7 | 1089 | `									sPend.pOwnerStack = (void *)pStack;` |
|       7 | 1090 | `									sPend.pInstrs = (void *)aInstr;` |
|       7 | 1091 | `									sPend.nJmpPc = (sxu32)(pc + 1);        /* the OP_NULLC_JMP */` |
|       7 | 1092 | `									sPend.nPc = (sxu32)(pNext->iP2 - 1);   /* the OP_NULLC_STORE */` |
|       7 | 1093 | `									pThis->iRef++;` |
|       7 | 1094 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|       3 | 1095 | `								}` |
|       - | 1096 | `								/* Pop the attribute name; the test value becomes the` |
|       - | 1097 | `								 * expression slot (a temp, not an lvalue). */` |
|       7 | 1098 | `								VmPopOperand(&pTos,1);` |
|       7 | 1099 | `								pThis->iRef++;` |
|       7 | 1100 | `								PH7_MemObjRelease(pTos);` |
|       7 | 1101 | `								PH7_MemObjStore(&sTest,pTos);` |
|       7 | 1102 | `								pTos->nIdx = SXU32_HIGH;` |
|       7 | 1103 | `								PH7_MemObjRelease(&sTest);` |
|       7 | 1104 | `								PH7_ClassInstanceUnref(pThis);` |
|       7 | 1105 | `								VM_EXIT_BREAK;` |
|     150 | 1106 | `							}else if( VmMemberNextIsRmw(pNext)` |
|      97 | 1107 | `							 && VmMagicRmwEligible(pVm,pClass,pThis,&sName) ){` |
|       - | 1108 | `								/* ++/--/compound-assign on an overloaded property: php` |
|       - | 1109 | `								 * reads through __get and writes the computed value back` |
|       - | 1110 | `								 * through __set. Arm the scratch slot the modify op will` |
|       - | 1111 | `								 * mutate and finish the op here — the pre-existing path` |
|       - | 1112 | `								 * vivified a dynamic property instead, which on any` |
|       - | 1113 | `								 * ordinary accessor class is PHL's` |
|       - | 1114 | `								 * "Cannot create dynamic property" Error. */` |
|       - | 1115 | `								ph7_value sRmwVal;` |
|       - | 1116 | `								sxu32 nRmwScratch;` |
|      27 | 1117 | `								PH7_MemObjInit(pVm,&sRmwVal);` |
|      40 | 1118 | `								VmMagicRmwArm(&(*pVm),pThis,pClass,&sName,&sRmwVal,&nRmwScratch,` |
|      26 | 1119 | `									(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|      27 | 1120 | `								VmPopOperand(&pTos,1);   /* drop the property name */` |
|      27 | 1121 | `								pThis->iRef++;` |
|      27 | 1122 | `								PH7_MemObjRelease(pTos); /* collapse the object slot */` |
|      27 | 1123 | `								PH7_MemObjStore(&sRmwVal,pTos);` |
|      27 | 1124 | `								pTos->nIdx = nRmwScratch;` |
|      27 | 1125 | `								PH7_MemObjRelease(&sRmwVal);` |
|      27 | 1126 | `								PH7_ClassInstanceUnref(pThis);` |
|      27 | 1127 | `								VM_EXIT_BREAK;` |
|     124 | 1128 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|      33 | 1129 | `							 && !VmMemberNextIsWrite(pNext)` |
|      31 | 1130 | `							 && PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) ){` |
|       - | 1131 | `								/* Subscript-write base on a class with __get: php reads` |
|       - | 1132 | `								 * through the magic layer (the write lands on the temp and` |
|       - | 1133 | `								 * is lost, like php's indirect-modification case). A PLAIN` |
|       - | 1134 | `								 * store (bPlainStore — the compiler tags those` |
|       - | 1135 | `								 * PH7_MEMBER_WRITE too) is NOT this case: it falls through` |
|       - | 1136 | `								 * to dynamic creation. Leave the miss path — the read gate` |
|       - | 1137 | `								 * below dispatches __get. */` |
|     125 | 1138 | `							}else if( pThis->pClass->iFlags & PH7_CLASS_READONLY ){` |
|       - | 1139 | `								SyBlob sErrMsg;` |
|     ! 0 | 1140 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     ! 0 | 1141 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|     ! 0 | 1142 | `									&pThis->pClass->sName,&sName);` |
|     ! 0 | 1143 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     118 | 1144 | `							}else if( !VmClassAllowsDynamicProps(pVm,pThis->pClass)` |
|      74 | 1145 | `							       && !VmClassHasAttributeNamed(pThis->pClass,"AllowDynamicProperties",sizeof("AllowDynamicProperties")-1) ){` |
|       - | 1146 | `								/* php 8.2 only DEPRECATES creating a dynamic property on a` |
|       - | 1147 | `								 * class without #[AllowDynamicProperties]; PHL rejects it.` |
|       - | 1148 | `								 * stdClass / __set / declared props are unaffected. */` |
|       - | 1149 | `								SyBlob sErrMsg;` |
|       8 | 1150 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       8 | 1151 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|       6 | 1152 | `									&pThis->pClass->sName,&sName);` |
|       8 | 1153 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       5 | 1154 | `							}else{` |
|     116 | 1155 | `								PH7_VmCreateDynamicAttr(&(*pVm),pThis,sName.zString,sName.nByte,&pObjAttr);` |
|       - | 1156 | `							}` |
|       - | 1157 | `						}` |
|     163 | 1158 | `						if( pObjAttr && VmMemberNextIsRmw(pNext) ){` |
|       - | 1159 | `							/* A read-modify-write READS the property before it writes it,` |
|       - | 1160 | `							 * so php warns that the name it just created was undefined and` |
|       - | 1161 | ``							 * then goes on with null — `$o->hits++` on a fresh object is`` |
|       - | 1162 | ``							 * `Undefined property: C::$hits` and then int(1). PHL created`` |
|       - | 1163 | `							 * the slot in silence. Only the read-modify-write forms:` |
|       - | 1164 | ``							 * a subscript-write base (`$o->arr[] = 1`) and a `??=` read`` |
|       - | 1165 | `							 * nothing, and php says nothing for either. */` |
|      19 | 1166 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|       6 | 1167 | `								&pClass->sName,&sName);` |
|       6 | 1168 | `						}` |
|      79 | 1169 | `					}` |
|     311 | 1170 | `				}` |
|  106026 | 1171 | `				if( pObjAttr == 0 && pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH` |
|     197 | 1172 | `				 && !(PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)` |
|     115 | 1173 | `				   && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g')) ){` |
|       - | 1174 | `					/* D1 commit 2: deferred property arg, property absent on a REACHABLE object` |
|       - | 1175 | `					 * base ($o is a real slot). Record a property step rooted at $o so OP_CALL can` |
|       - | 1176 | `					 * vivify+bind (by-ref) or read via __get / warn (by-value). A magic __get/__set` |
|       - | 1177 | `					 * class is recorded the same way; the resolve step emits php's Notice for the` |
|       - | 1178 | `					 * by-ref case and dispatches __get for by-value. */` |
|       - | 1179 | `					SyString sProp;` |
|      13 | 1180 | `					VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|      13 | 1181 | `					SyStringInitFromBuf(&sProp,sName.zString,sName.nByte);` |
|      13 | 1182 | `					if( pPath && VmDeferPathPushProp(pPath,&sProp) == SXRET_OK ){` |
|      13 | 1183 | `						VmPopOperand(&pTos,1);       /* drop the property name */` |
|      13 | 1184 | `						pThis->iRef++;` |
|      13 | 1185 | `						PH7_MemObjRelease(pTos);     /* collapse the object slot into the carrier */` |
|      13 | 1186 | `						pTos->x.pOther = pPath;` |
|      13 | 1187 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      13 | 1188 | `						pTos->nIdx = SXU32_HIGH;` |
|      13 | 1189 | `						PH7_ClassInstanceUnref(pThis);` |
|      13 | 1190 | `						VM_EXIT_BREAK;` |
|       - | 1191 | `					}` |
|     ! 0 | 1192 | `					if( pPath ){` |
|     ! 0 | 1193 | `						VmFreeDeferredPath(pPath);` |
|     ! 0 | 1194 | `					}` |
|       - | 1195 | `					/* fall through to the normal miss handling on allocation failure */` |
|     ! 0 | 1196 | `				}` |
|  106021 | 1197 | `				if( pObjAttr == 0 ){` |
|       - | 1198 | `					/* Missing property. On a plain READ, php dispatches __get($name) and the` |
|       - | 1199 | `					 * expression takes its RETURN VALUE (band A #3a — pre-fix the result was` |
|       - | 1200 | `					 * discarded and a PHL-native warn fired even when __get existed). isset/` |
|       - | 1201 | `					 * empty context consults __isset first (then __get for empty()'s value` |
|       - | 1202 | `					 * test); a plain-store write context parked a pending __set above. A` |
|       - | 1203 | `					 * self-recursive read of the same property falls back to the` |
|       - | 1204 | `					 * undefined-property path via the guard, like php's property guard. */` |
|     497 | 1205 | `					ph7_class_method *pGetMagic = 0;` |
|     497 | 1206 | `					if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|      94 | 1207 | `						ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|      94 | 1208 | `						if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 1209 | `							ph7_value sIssetRet;` |
|       - | 1210 | `							int bSet;` |
|      51 | 1211 | `							PH7_MemObjInit(pVm,&sIssetRet);` |
|      51 | 1212 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|      51 | 1213 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|      51 | 1214 | `							VmMagicGuardPop(pVm);` |
|      51 | 1215 | `							PH7_MemObjToBool(&sIssetRet);` |
|      51 | 1216 | `							bSet = sIssetRet.x.iVal != 0;` |
|      51 | 1217 | `							PH7_MemObjRelease(&sIssetRet);` |
|      51 | 1218 | `							if( bSet && VmMemberCtxWantsValue(pInstr->iP2) ){` |
|       - | 1219 | ``								/* empty() and `??`: __isset said set, so php goes on to`` |
|       - | 1220 | `								 * __get for the VALUE — emptiness is judged on it, and the` |
|       - | 1221 | `								 * coalesce simply IS it (a null answer then takes the` |
|       - | 1222 | `								 * default, which OP_NULLC does for free). */` |
|      18 | 1223 | `								ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       - | 1224 | `								ph7_value sEmptyVal;` |
|      18 | 1225 | `								PH7_MemObjInit(pVm,&sEmptyVal);` |
|      18 | 1226 | `								if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|      18 | 1227 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|      18 | 1228 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|      18 | 1229 | `									VmMagicGuardPop(pVm);` |
|       8 | 1230 | `								}` |
|      18 | 1231 | `								VmPopOperand(&pTos,1);` |
|      18 | 1232 | `								pThis->iRef++;` |
|      18 | 1233 | `								PH7_MemObjRelease(pTos);` |
|      18 | 1234 | `								PH7_MemObjStore(&sEmptyVal,pTos);` |
|      18 | 1235 | `								pTos->nIdx = SXU32_HIGH;` |
|      18 | 1236 | `								PH7_MemObjRelease(&sEmptyVal);` |
|      18 | 1237 | `								PH7_ClassInstanceUnref(pThis);` |
|      18 | 1238 | `								VM_EXIT_BREAK;` |
|       - | 1239 | `							}` |
|       - | 1240 | `							/* isset(): the truth of __isset IS the answer — push a non-null` |
|       - | 1241 | `							 * marker for true, NULL for false (isset only tests null-ness). */` |
|      35 | 1242 | `							VmPopOperand(&pTos,1);` |
|      35 | 1243 | `							pThis->iRef++;` |
|      35 | 1244 | `							PH7_MemObjRelease(pTos);` |
|      35 | 1245 | `							if( bSet ){` |
|      17 | 1246 | `								pTos->x.iVal = 1;` |
|      17 | 1247 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       7 | 1248 | `							}` |
|      35 | 1249 | `							pTos->nIdx = SXU32_HIGH;` |
|      35 | 1250 | `							PH7_ClassInstanceUnref(pThis);` |
|      35 | 1251 | `							VM_EXIT_BREAK;` |
|       - | 1252 | `						}` |
|      21 | 1253 | `					}` |
|     444 | 1254 | `					if( (!VmMemberCtxIsLookup(pInstr->iP2) \|\| pInstr->iP2 == PH7_MEMBER_COALESCE)` |
|     425 | 1255 | `					 && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|     430 | 1256 | `					 && !VmMemberNextIsWrite(pInstr + 1) ){` |
|       - | 1257 | `						/* Plain reads AND the ??=/subscript write-base (PH7_MEMBER_WRITE,` |
|       - | 1258 | `						 * which php reads through __get); read-modify-write forms are` |
|       - | 1259 | `						 * excluded (they vivified above — approximate, recorded), and so is` |
|       - | 1260 | `						 * a destructuring target (a pure write — php never reads it).` |
|       - | 1261 | ``						 * `??` reaches here only when the class declares NO __isset — the`` |
|       - | 1262 | `						 * branch above has returned otherwise — so php's gate is absent and` |
|       - | 1263 | `						 * __get answers on its own. */` |
|     379 | 1264 | `						pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|     187 | 1265 | `					}` |
|     449 | 1266 | `					if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 1267 | `						ph7_value sMagicRet;` |
|     369 | 1268 | `						PH7_MemObjInit(pVm,&sMagicRet);` |
|     369 | 1269 | `						VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     369 | 1270 | `						PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|     369 | 1271 | `						VmMagicGuardPop(pVm);` |
|     369 | 1272 | `						if( VmMemberFetchForWrite(pInstr) ){` |
|       - | 1273 | `							/* php's indirect-modification notice, and the separation` |
|       - | 1274 | `							 * that makes the write it describes land nowhere. Raised` |
|       - | 1275 | `							 * BEFORE the pop below: sName still aliases the NAME` |
|       - | 1276 | ``							 * operand when the name is dynamic (`$o->$k[0] = v`). */`` |
|      11 | 1277 | `							PH7_VmOverloadedPropNotice(&(*pVm),pClass,&sName,&sMagicRet);` |
|     364 | 1278 | `						}else if( pInstr->iP2 == PH7_MEMBER_DEFPATH ){` |
|       - | 1279 | `							/* A deferred call ARGUMENT: __get has answered — php calls it` |
|       - | 1280 | `							 * where the property is WRITTEN, whatever the parameter turns` |
|       - | 1281 | `							 * out to be — and only the by-ref verdict is still pending.` |
|       - | 1282 | `							 * Carry the value with the class and name that produced it, so` |
|       - | 1283 | `							 * the notice lands at the call for a by-REFERENCE parameter and` |
|       - | 1284 | `							 * nothing is said for a by-value one, without __get running` |
|       - | 1285 | `							 * twice or a second read arriving after a later argument's side` |
|       - | 1286 | `							 * effects. */` |
|     219 | 1287 | `							VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_PROP,` |
|      72 | 1288 | `								pClass,&sName,&sMagicRet);` |
|     147 | 1289 | `							if( pPre ){` |
|     147 | 1290 | `								VmPopOperand(&pTos,1);   /* drop the property name */` |
|     147 | 1291 | `								pThis->iRef++;` |
|     147 | 1292 | `								PH7_MemObjRelease(pTos); /* collapse the object slot into the carrier */` |
|     147 | 1293 | `								pTos->x.pOther = pPre;` |
|     147 | 1294 | `								pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|     147 | 1295 | `								pTos->nIdx = SXU32_HIGH;` |
|     147 | 1296 | `								PH7_MemObjRelease(&sMagicRet);` |
|     147 | 1297 | `								PH7_ClassInstanceUnref(pThis);` |
|     147 | 1298 | `								VM_EXIT_BREAK;` |
|       - | 1299 | `							}` |
|     ! 0 | 1300 | `						}` |
|       - | 1301 | `						/* Pop the attribute name, replace the object slot with the magic` |
|       - | 1302 | `						 * result (a temp, not an lvalue — nIdx stays constant). A throw` |
|       - | 1303 | `						 * from __get parked at the boundary; the fetch-point router lands` |
|       - | 1304 | `						 * it and abandons this slot. */` |
|     225 | 1305 | `						VmPopOperand(&pTos,1);` |
|     225 | 1306 | `						pThis->iRef++;` |
|     225 | 1307 | `						PH7_MemObjRelease(pTos);` |
|     225 | 1308 | `						PH7_MemObjStore(&sMagicRet,pTos);` |
|     225 | 1309 | `						pTos->nIdx = SXU32_HIGH;` |
|     225 | 1310 | `						PH7_MemObjRelease(&sMagicRet);` |
|     225 | 1311 | `						PH7_ClassInstanceUnref(pThis);` |
|     225 | 1312 | `						VM_EXIT_BREAK;` |
|       - | 1313 | `					}` |
|       - | 1314 | `					/* No __get (or guard held): load null. In isset()/empty() context (iP2 3/4)` |
|       - | 1315 | `					 * PHP returns false/true SILENTLY, so suppress the read-miss warning there` |
|       - | 1316 | `					 * (mirrors the array LOAD_IDX iP2=4/6 suppression); likewise when a` |
|       - | 1317 | `					 * pending __set was parked above (the following OP_STORE dispatches it —` |
|       - | 1318 | `					 * nothing is "undefined" about that write) or a throw is parked on the` |
|       - | 1319 | `					 * boundary rail (e.g. the readonly-class dynamic-property Error — the` |
|       - | 1320 | `					 * fetch-point router lands it right after this op). */` |
|      80 | 1321 | `					if( !VmMemberCtxIsLookup(pInstr->iP2) && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|      40 | 1322 | `					 && pVm->pMagicSetThis == 0` |
|      31 | 1323 | `					 && pVm->nBoundaryRc == 0 ){` |
|       - | 1324 | `						/* A destructuring target is also silent: php either created the` |
|       - | 1325 | `						 * property above or dispatched __set — neither warns. */` |
|      15 | 1326 | `						VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|       4 | 1327 | `							&pClass->sName,&sName);` |
|       4 | 1328 | `					}` |
|      40 | 1329 | `				}` |
|  105609 | 1330 | `				VmPopOperand(&pTos,1);` |
|       - | 1331 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|       - | 1332 | `				 * This is due to the following case:` |
|       - | 1333 | `				 *     (new TestClass())->foo;` |
|       - | 1334 | `				 */` |
|  105609 | 1335 | `				pThis->iRef++;` |
|  105609 | 1336 | `				PH7_MemObjRelease(pTos);` |
|  105609 | 1337 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|  105609 | 1338 | `				if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - | 1339 | ``					/* `$o->p =& $x`: stash the resolved instance property slot for the`` |
|       - | 1340 | `					 * following member-marked OP_STORE_REF, which rebinds it to alias the` |
|       - | 1341 | `					 * source variable. Do NOT run the read/hook/magic/uninit machinery` |
|       - | 1342 | `					 * below — a reference bind neither reads the value nor triggers` |
|       - | 1343 | `					 * get/set hooks or an uninitialized-typed Error. pThis stays retained` |
|       - | 1344 | `					 * (the iRef++ above); OP_STORE_REF releases it. */` |
|      32 | 1345 | `					if( pObjAttr ){` |
|      32 | 1346 | `						pVm->pRefTargetAttr = pObjAttr;` |
|      32 | 1347 | `						pVm->pRefTargetThis = pThis;` |
|      32 | 1348 | `						pVm->pRefTargetStaticAttr = 0;` |
|      32 | 1349 | `						pTos->nIdx = pObjAttr->nIdx;` |
|      17 | 1350 | `					}else{` |
|       - | 1351 | `						/* Missing/inaccessible target: leave nothing stashed; OP_STORE_REF` |
|       - | 1352 | `						 * no-ops. Balance the retain. */` |
|     ! 0 | 1353 | `						pVm->pRefTargetAttr = 0;` |
|     ! 0 | 1354 | `						pVm->pRefTargetThis = 0;` |
|     ! 0 | 1355 | `						pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 1356 | `						PH7_ClassInstanceUnref(pThis);` |
|       - | 1357 | `					}` |
|      32 | 1358 | `					VM_EXIT_BREAK;` |
|       - | 1359 | `				}` |
|  105579 | 1360 | `				if( pObjAttr ){` |
|  105499 | 1361 | `					ph7_value *pValue = 0; /* cc warning */` |
|       - | 1362 | `					/* Check attribute access */` |
|  105499 | 1363 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|  105464 | 1364 | `						if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET))` |
|   52913 | 1365 | `						 && !VmHookGuardHeld(pVm,(void *)pThis,&sName) ){` |
|       - | 1366 | `							/* PHP 8.4 property hooks: route reads, writes, and the` |
|       - | 1367 | `							 * read-modify-write forms through the synthesized hook` |
|       - | 1368 | `							 * methods. A held guard (either kind) means we are INSIDE` |
|       - | 1369 | `							 * one of this property's own hook bodies — fall through to` |
|       - | 1370 | `` 							 * the raw backing slot for BOTH directions (php: `$this->x` `` |
|       - | 1371 | `							 * within any of x's hooks addresses the backing store). */` |
|     221 | 1372 | `							VmInstr *pNextH = pInstr + 1;` |
|     221 | 1373 | `							int bPlainStore = (pNextH->iOp == PH7_OP_STORE && pNextH->iP2 != 0);` |
|     377 | 1374 | `							int bCoalesceW = pInstr->iP2 == PH7_MEMBER_WRITE` |
|     216 | 1375 | `								&& pNextH->iOp == PH7_OP_NULLC_JMP;` |
|       - | 1376 | `							/* ++/--/compound-assign: the modify op FOLLOWS the member` |
|       - | 1377 | `							 * directly (the compiler tags compound-assign members` |
|       - | 1378 | `							 * PH7_MEMBER_WRITE too, so classify by the next op, not iP2;` |
|       - | 1379 | `							 * the !bPlainStore term is load-bearing — VmMemberNextIsWrite` |
|       - | 1380 | `							 * returns 1 for a member OP_STORE too) */` |
|     221 | 1381 | `							int bRmwNext = !bPlainStore && VmMemberNextIsWrite(pNextH);` |
|     258 | 1382 | `							int bSubscriptW = !bPlainStore && !bCoalesceW && !bRmwNext` |
|     298 | 1383 | `								&& pInstr->iP2 == PH7_MEMBER_WRITE;` |
|     221 | 1384 | `							if( bPlainStore ){` |
|       - | 1385 | `								/* Arm the pending hook-set for the following OP_STORE — it` |
|       - | 1386 | `								 * dispatches the set hook, or throws the read-only Error` |
|       - | 1387 | `								 * when the property only has a get hook. (The scalar` |
|       - | 1388 | `								 * transient is safe here: its window is exactly one` |
|       - | 1389 | `								 * instruction, MEMBER -> STORE, nothing runs in between.) */` |
|      55 | 1390 | `								pThis->iRef++;` |
|      55 | 1391 | `								pVm->pHookSetThis = pThis;` |
|      55 | 1392 | `								pVm->pHookSetAttr = pObjAttr->pAttr;` |
|      55 | 1393 | `								pVm->nHookSetIdx = pObjAttr->nIdx;` |
|      55 | 1394 | `								pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|      55 | 1395 | `								PH7_ClassInstanceUnref(pThis);` |
|      55 | 1396 | `								VM_EXIT_BREAK;` |
|       - | 1397 | `							}` |
|     169 | 1398 | `							if( bSubscriptW ){` |
|       - | 1399 | `								/* Subscript write base ($o->m[] = v, $o->m[$k] = v):` |
|       - | 1400 | `								 * php's catchable Error, with or without a set hook` |
|       - | 1401 | ``								 * (only a by-ref `&get` hook would allow it — a loud`` |
|       - | 1402 | `								 * compile error in PHL). The op completes benignly with` |
|       - | 1403 | `								 * a null temp; the fetch-point router lands the throw. */` |
|       - | 1404 | `								SyBlob sErrMsg;` |
|       7 | 1405 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       7 | 1406 | `								SyBlobFormat(&sErrMsg,"Indirect modification of %z::$%z is not allowed",` |
|       6 | 1407 | `									&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       7 | 1408 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       7 | 1409 | `								PH7_ClassInstanceUnref(pThis);` |
|       7 | 1410 | `								VM_EXIT_BREAK;` |
|       - | 1411 | `							}` |
|     158 | 1412 | `							if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|      84 | 1413 | `							 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1414 | `								/* VIRTUAL set-only property: there is no backing store to` |
|       - | 1415 | `								 * fall back to, so EVERY read context — plain read,` |
|       - | 1416 | `								 * isset()/empty() (php throws there too, probe-verified),` |
|       - | 1417 | `								 * the ??= test, an RMW read — is php's catchable` |
|       - | 1418 | `								 * write-only Error. */` |
|       - | 1419 | `								SyBlob sErrMsg;` |
|       9 | 1420 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       9 | 1421 | `								SyBlobFormat(&sErrMsg,"Property %z::$%z is write-only",` |
|       8 | 1422 | `									&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       9 | 1423 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       9 | 1424 | `								PH7_ClassInstanceUnref(pThis);` |
|       9 | 1425 | `								VM_EXIT_BREAK;` |
|       - | 1426 | `							}` |
|     155 | 1427 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1428 | ``								/* isset()/empty()/`??` on a hooked property all call the get`` |
|       - | 1429 | `								 * hook (php): isset is "get() !== null", empty tests the` |
|       - | 1430 | ``								 * value, and `??` IS the value. */`` |
|       - | 1431 | `								ph7_value sHookRet;` |
|      16 | 1432 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|      16 | 1433 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|      16 | 1434 | `									if( !VmMemberCtxWantsValue(pInstr->iP2) ){` |
|       - | 1435 | `										/* isset(): the trailing builtin tests NULL-ness of what` |
|       - | 1436 | `										 * it is handed, so "not set" has to BE null. Storing` |
|       - | 1437 | ``										 * bool(FALSE) here made `isset($o->p)` answer TRUE for`` |
|       - | 1438 | `										 * a get hook returning null — the same non-null marker` |
|       - | 1439 | `										 * convention the __isset path uses. */` |
|       8 | 1440 | `										if( (sHookRet.iFlags & MEMOBJ_NULL) == 0 ){` |
|       6 | 1441 | `											pTos->x.iVal = 1;` |
|       6 | 1442 | `											MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       4 | 1443 | `										}else{` |
|       3 | 1444 | `											MemObjSetType(pTos,MEMOBJ_NULL);` |
|       - | 1445 | `										}` |
|       5 | 1446 | `									}else{` |
|       - | 1447 | ``										/* empty() judges the value; `??` IS the value. */`` |
|      10 | 1448 | `										PH7_MemObjStore(&sHookRet,pTos);` |
|       - | 1449 | `									}` |
|      16 | 1450 | `									pTos->nIdx = SXU32_HIGH;` |
|      16 | 1451 | `									PH7_MemObjRelease(&sHookRet);` |
|      16 | 1452 | `									PH7_ClassInstanceUnref(pThis);` |
|      16 | 1453 | `									VM_EXIT_BREAK;` |
|       - | 1454 | `								}` |
|     ! 0 | 1455 | `								PH7_MemObjRelease(&sHookRet);` |
|     ! 0 | 1456 | `							}` |
|     140 | 1457 | `							if( !bCoalesceW && !bRmwNext && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1458 | `								/* Read: dispatch __phl_hook_get_NAME (inheritance-aware) */` |
|       - | 1459 | `								ph7_value sHookRet;` |
|     106 | 1460 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|     106 | 1461 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|      92 | 1462 | `									if( pInstr->iP2 == PH7_MEMBER_DEFPATH ){` |
|       - | 1463 | `										/* A deferred call ARGUMENT of a HOOKED property. The` |
|       - | 1464 | `										 * get hook has answered, as php's does; what a` |
|       - | 1465 | `										 * by-REFERENCE parameter then gets is not a notice` |
|       - | 1466 | `										 * but php's refusal — a hook has no slot to alias` |
|       - | 1467 | ``										 * and `&get` does not exist here — while a by-VALUE`` |
|       - | 1468 | `										 * one simply takes the hook's value. */` |
|      42 | 1469 | `										VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),` |
|      26 | 1470 | `											VM_OVER_HOOK,pThis->pClass,&pObjAttr->pAttr->sName,&sHookRet);` |
|      29 | 1471 | `										if( pPre ){` |
|      29 | 1472 | `											PH7_MemObjRelease(pTos);` |
|      29 | 1473 | `											pTos->x.pOther = pPre;` |
|      29 | 1474 | `											pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      29 | 1475 | `											pTos->nIdx = SXU32_HIGH;` |
|      29 | 1476 | `											PH7_MemObjRelease(&sHookRet);` |
|      29 | 1477 | `											PH7_ClassInstanceUnref(pThis);` |
|      60 | 1478 | `											VM_EXIT_BREAK;` |
|       - | 1479 | `										}` |
|     ! 0 | 1480 | `									}` |
|      65 | 1481 | `									PH7_MemObjStore(&sHookRet,pTos);` |
|      65 | 1482 | `									pTos->nIdx = SXU32_HIGH;` |
|      65 | 1483 | `									PH7_MemObjRelease(&sHookRet);` |
|      65 | 1484 | `									PH7_ClassInstanceUnref(pThis);` |
|      65 | 1485 | `									VM_EXIT_BREAK;` |
|       - | 1486 | `								}` |
|      15 | 1487 | `								PH7_MemObjRelease(&sHookRet);` |
|       7 | 1488 | `							}` |
|      49 | 1489 | `							if( bCoalesceW \|\| bRmwNext ){` |
|       - | 1490 | ``								/* `$o->x ??= v` (bCoalesceW) and the read-modify-write`` |
|       - | 1491 | ``								 * forms `$o->x++`, `$o->x .= v`, … (bRmwNext): php reads`` |
|       - | 1492 | `								 * through the get hook (raw backing when the property is` |
|       - | 1493 | `								 * set-only) and writes through the set hook.` |
|       - | 1494 | `								 *   - ??=: the test value goes on the stack and a` |
|       - | 1495 | `								 *     COAL_HOOK entry is pushed for the OP_NULLC_STORE` |
|       - | 1496 | `								 *     at the jump target; the fetch-point sweep drops it` |
|       - | 1497 | `								 *     when the short-circuit jump skips the assign or a` |
|       - | 1498 | `								 *     throw abandons the RHS (the entry is NOT a scalar` |
|       - | 1499 | `								 *     transient — nested stores/coalesces in the RHS` |
|       - | 1500 | `								 *     stack their own entries above it).` |
|       - | 1501 | `								 *   - RMW: the value goes into a fresh SCRATCH slot the` |
|       - | 1502 | `								 *     modify op mutates in place; the entry makes the` |
|       - | 1503 | `								 *     op's tail dispatch the set side with the computed` |
|       - | 1504 | `								 *     value (PH7_HOOK_RMW_WRITEBACK). */` |
|       - | 1505 | `								ph7_value sCur;` |
|       - | 1506 | `								sxi32 rcCur;` |
|      35 | 1507 | `								PH7_MemObjInit(pVm,&sCur);` |
|      35 | 1508 | `								rcCur = PH7_VmHookGetAttrValue(pThis,pObjAttr,&sCur);` |
|      35 | 1509 | `								if( rcCur == SXERR_NOTFOUND ){` |
|       - | 1510 | `									/* set-only hook (or guard edge): the read side is the` |
|       - | 1511 | `									 * raw backing store (php) */` |
|       3 | 1512 | `									ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|       3 | 1513 | `									if( pBack ){` |
|       3 | 1514 | `										PH7_MemObjStore(pBack,&sCur);` |
|       2 | 1515 | `									}` |
|      34 | 1516 | `								}else if( pVm->nBoundaryRc != 0 ){` |
|       - | 1517 | `									/* the get hook threw: leave the null temp; the` |
|       - | 1518 | `									 * fetch-point router lands the parked throw —` |
|       - | 1519 | `									 * nothing is armed. */` |
|     ! 0 | 1520 | `									PH7_MemObjRelease(&sCur);` |
|     ! 0 | 1521 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1522 | `									VM_EXIT_BREAK;` |
|       - | 1523 | `								}` |
|      35 | 1524 | `								if( bCoalesceW ){` |
|       - | 1525 | `									VmHookRmw sPend;` |
|      15 | 1526 | `									PH7_MemObjStore(&sCur,pTos);` |
|      15 | 1527 | `									pTos->nIdx = SXU32_HIGH;` |
|      15 | 1528 | `									PH7_MemObjRelease(&sCur);` |
|      15 | 1529 | `									sPend.iKind = VM_HOOK_PEND_COAL_HOOK;` |
|      15 | 1530 | `									sPend.pThis = pThis;` |
|      15 | 1531 | `									sPend.pAttr = pObjAttr->pAttr;` |
|      15 | 1532 | `									sPend.nBackIdx = pObjAttr->nIdx;` |
|      15 | 1533 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|      15 | 1534 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|      15 | 1535 | `									sPend.pOwnerStack = (void *)pStack;` |
|      15 | 1536 | `									sPend.pInstrs = (void *)aInstr;` |
|      15 | 1537 | `									sPend.nJmpPc = (sxu32)(pc + 1);            /* the OP_NULLC_JMP */` |
|      15 | 1538 | `									sPend.nPc = (sxu32)(pNextH->iP2 - 1);      /* the OP_NULLC_STORE */` |
|      15 | 1539 | `									pThis->iRef++;` |
|      15 | 1540 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|      15 | 1541 | `									PH7_ClassInstanceUnref(pThis);` |
|      15 | 1542 | `									VM_EXIT_BREAK;` |
|       - | 1543 | `								}` |
|       - | 1544 | `								{` |
|      21 | 1545 | `									ph7_value *pScr = PH7_ReserveMemObj(&(*pVm));` |
|      21 | 1546 | `									if( pScr ){` |
|       - | 1547 | `										VmHookRmw sRmw;` |
|      21 | 1548 | `										PH7_MemObjStore(&sCur,pScr);` |
|      21 | 1549 | `										PH7_MemObjStore(&sCur,pTos);` |
|      21 | 1550 | `										pTos->nIdx = pScr->nIdx;` |
|      21 | 1551 | `										sRmw.iKind = VM_HOOK_PEND_RMW;` |
|      21 | 1552 | `										sRmw.pThis = pThis;` |
|      21 | 1553 | `										sRmw.pAttr = pObjAttr->pAttr;` |
|      21 | 1554 | `										sRmw.nBackIdx = pObjAttr->nIdx;` |
|      21 | 1555 | `										sRmw.nScratchIdx = pScr->nIdx;` |
|      21 | 1556 | `										SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      21 | 1557 | `										sRmw.pOwnerStack = (void *)pStack;` |
|      21 | 1558 | `										sRmw.pInstrs = (void *)aInstr;` |
|      21 | 1559 | `										sRmw.nJmpPc = (sxu32)(pc + 1);  /* the modify op ... */` |
|      21 | 1560 | `										sRmw.nPc = (sxu32)(pc + 1);     /* ... is the whole window */` |
|      21 | 1561 | `										pThis->iRef++;` |
|      21 | 1562 | `										SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      10 | 1563 | `									}` |
|       - | 1564 | `									/* OOM: pScr == 0 — leave the null temp (loud allocator` |
|       - | 1565 | `									 * diagnostics already fired) */` |
|      21 | 1566 | `									PH7_MemObjRelease(&sCur);` |
|      21 | 1567 | `									PH7_ClassInstanceUnref(pThis);` |
|      21 | 1568 | `									VM_EXIT_BREAK;` |
|       - | 1569 | `								}` |
|       - | 1570 | `							}` |
|       7 | 1571 | `						}` |
|       - | 1572 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|       - | 1573 | `						 * We can only raise it on a real read, not when the slot is the` |
|       - | 1574 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|       - | 1575 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|       - | 1576 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|  105262 | 1577 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|   52785 | 1578 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|     303 | 1579 | `							VmInstr *pNext = pInstr + 1;` |
|     303 | 1580 | `							int bIsLhs = 0;` |
|     303 | 1581 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|     283 | 1582 | `								bIsLhs = 1;` |
|     139 | 1583 | `							}` |
|     303 | 1584 | `							if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 1585 | `								/* Destructuring target: the OP_LOAD_LIST store (typed-` |
|       - | 1586 | `								 * enforced) initializes it — never a read (php). */` |
|       7 | 1587 | `								bIsLhs = 1;` |
|       3 | 1588 | `							}` |
|       - | 1589 | ``							/* isset()/empty()/`??` read an uninitialized typed property`` |
|       - | 1590 | `							 * as "not set" — a silent miss, NOT the Error a plain read` |
|       - | 1591 | `							 * raises (php). The compiler tags such an access iP2 =` |
|       - | 1592 | `							 * ISSET/EMPTY; treat it like the assignment-LHS case and fall` |
|       - | 1593 | `							 * through to load the slot's NULL. */` |
|     303 | 1594 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       7 | 1595 | `								bIsLhs = 1;` |
|       3 | 1596 | `							}` |
|     303 | 1597 | `							if( !bIsLhs ){` |
|      11 | 1598 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|      11 | 1599 | `								PH7_ClassInstanceUnref(pThis);` |
|      11 | 1600 | `								if( rcU == PH7_ABORT ){` |
|       3 | 1601 | `									VM_EXIT_ABORT;` |
|       - | 1602 | `								}` |
|       - | 1603 | `								{` |
|       - | 1604 | `									sxi32 iRp;` |
|       8 | 1605 | `									if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|       5 | 1606 | `										PH7_RESUME_DRAIN()` |
|       3 | 1607 | `										pc = iRp;` |
|       3 | 1608 | `										VM_EXIT_BREAK;` |
|       - | 1609 | `									}` |
|       - | 1610 | `								}` |
|       5 | 1611 | `								VM_EXIT_EXCEPTION;` |
|       - | 1612 | `							}` |
|     145 | 1613 | `						}` |
|       - | 1614 | `						/* Load attribute */` |
|  105259 | 1615 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|  105259 | 1616 | `						if( pValue ){` |
|  105259 | 1617 | `							if( pThis->iRef < 2 ){` |
|       - | 1618 | `								/* Perform a store operation,rather than a load operation since` |
|       - | 1619 | `								 * the class instance '$this' will be deleted shortly.` |
|       - | 1620 | `								 */` |
|     167 | 1621 | `								PH7_MemObjStore(pValue,pTos);` |
|      85 | 1622 | `							}else{` |
|       - | 1623 | `								/* Simple load */` |
|  105095 | 1624 | `								PH7_MemObjLoad(pValue,pTos);` |
|       - | 1625 | `							}` |
|  105259 | 1626 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|  105259 | 1627 | `								if( pThis->iRef > 1 ){` |
|       - | 1628 | `									/* Load attribute index */` |
|  105095 | 1629 | `									pTos->nIdx = pObjAttr->nIdx;` |
|   52545 | 1630 | `								}` |
|   52627 | 1631 | `							}` |
|   52627 | 1632 | `						}` |
|   52632 | 1633 | `					}else{` |
|       - | 1634 | `						/* Inaccessible (private/protected) from this scope. php consults` |
|       - | 1635 | `						 * __get here exactly like a missing property (band A #3a) before` |
|       - | 1636 | `						 * erroring; the guard keeps a self-recursive read from looping. */` |
|      33 | 1637 | `						ph7_class_method *pGetMagic = 0;` |
|       - | 1638 | ``						/* A WRITE-context fetch (the base of `$o->p[k] = v`) reads through`` |
|       - | 1639 | `						 * __get in php too — the write then lands on the value it answered` |
|       - | 1640 | `						 * and is lost, with php's indirect-modification notice. PHL refused` |
|       - | 1641 | ``						 * the whole statement with `Cannot access private property` instead,`` |
|       - | 1642 | ``						 * a fatal on a program php runs. A plain store and a `??=` keep`` |
|       - | 1643 | `						 * their own paths below. */` |
|      30 | 1644 | `						if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      36 | 1645 | `						 && (VmMemberFetchForWrite(pInstr)` |
|      20 | 1646 | `						  \|\| (pInstr->iP2 != PH7_MEMBER_WRITE && !VmMemberNextIsWrite(pInstr + 1))) ){` |
|      21 | 1647 | `							pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       9 | 1648 | `						}` |
|       - | 1649 | `						/* ++/--/compound-assign on a DECLARED but inaccessible property:` |
|       - | 1650 | `						 * php's accessors answer for it exactly as they do for a missing` |
|       - | 1651 | `						 * one (__get, modify, __set), where PHL raised` |
|       - | 1652 | ``						 * `Cannot access private property C::$n`. A plain store keeps its`` |
|       - | 1653 | `						 * own __set park below — it is a write, but not a read-modify-write. */` |
|      30 | 1654 | `						if( VmMemberNextIsRmw(pInstr + 1)` |
|      19 | 1655 | `						 && VmMagicRmwEligible(pVm,pClass,pThis,&sName) ){` |
|       - | 1656 | `							/* The name was already popped and pTos released above, so` |
|       - | 1657 | `							 * sName is the instruction's own literal here. */` |
|       - | 1658 | `							ph7_value sRmwVal;` |
|       - | 1659 | `							sxu32 nRmwScratch;` |
|       3 | 1660 | `							PH7_MemObjInit(pVm,&sRmwVal);` |
|       4 | 1661 | `							VmMagicRmwArm(&(*pVm),pThis,pClass,&sName,&sRmwVal,&nRmwScratch,` |
|       2 | 1662 | `								(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|       3 | 1663 | `							PH7_MemObjStore(&sRmwVal,pTos);` |
|       3 | 1664 | `							pTos->nIdx = nRmwScratch;` |
|       3 | 1665 | `							PH7_MemObjRelease(&sRmwVal);` |
|       3 | 1666 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 1667 | `							VM_EXIT_BREAK;` |
|       - | 1668 | `						}` |
|      31 | 1669 | `						if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 1670 | `							ph7_value sMagicRet;` |
|       9 | 1671 | `							PH7_MemObjInit(pVm,&sMagicRet);` |
|       9 | 1672 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       9 | 1673 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|       9 | 1674 | `							VmMagicGuardPop(pVm);` |
|       9 | 1675 | `							if( VmMemberFetchForWrite(pInstr) ){` |
|       5 | 1676 | `								PH7_VmOverloadedPropNotice(&(*pVm),pClass,&sName,&sMagicRet);` |
|       2 | 1677 | `							}` |
|       - | 1678 | `							/* The name was already popped and pTos released above; just` |
|       - | 1679 | `							 * take the magic result as the expression value. */` |
|       9 | 1680 | `							PH7_MemObjStore(&sMagicRet,pTos);` |
|       9 | 1681 | `							pTos->nIdx = SXU32_HIGH;` |
|       9 | 1682 | `							PH7_MemObjRelease(&sMagicRet);` |
|       9 | 1683 | `							PH7_ClassInstanceUnref(pThis);` |
|       9 | 1684 | `							VM_EXIT_BREAK;` |
|       - | 1685 | `						}` |
|      23 | 1686 | `						if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1687 | `							/* isset/empty on an inaccessible property: php consults __isset` |
|       - | 1688 | `							 * (band A #3b), and is silently false without it — never an` |
|       - | 1689 | `							 * Error (pre-fix PHL fataled here). */` |
|       9 | 1690 | `							ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|       9 | 1691 | `							int bSet = 0;` |
|       9 | 1692 | `							if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 1693 | `								ph7_value sIssetRet;` |
|     ! 0 | 1694 | `								PH7_MemObjInit(pVm,&sIssetRet);` |
|     ! 0 | 1695 | `								VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|     ! 0 | 1696 | `								PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|     ! 0 | 1697 | `								VmMagicGuardPop(pVm);` |
|     ! 0 | 1698 | `								PH7_MemObjToBool(&sIssetRet);` |
|     ! 0 | 1699 | `								bSet = sIssetRet.x.iVal != 0;` |
|     ! 0 | 1700 | `								PH7_MemObjRelease(&sIssetRet);` |
|     ! 0 | 1701 | `							}` |
|       9 | 1702 | `							if( pInstr->iP2 == PH7_MEMBER_COALESCE && pIssetMagic == 0 ){` |
|       - | 1703 | ``								/* `??` with no __isset to gate it: php reads straight through`` |
|       - | 1704 | `								 * __get, exactly as it does for a MISSING property. The` |
|       - | 1705 | `								 * __get dispatch just above this block is gated on a` |
|       - | 1706 | `								 * non-lookup context, so answer here. */` |
|       5 | 1707 | `								ph7_class_method *pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       5 | 1708 | `								if( pCoalGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 1709 | `									ph7_value sCoalRet;` |
|     ! 0 | 1710 | `									PH7_MemObjInit(pVm,&sCoalRet);` |
|     ! 0 | 1711 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     ! 0 | 1712 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sCoalRet);` |
|     ! 0 | 1713 | `									VmMagicGuardPop(pVm);` |
|     ! 0 | 1714 | `									PH7_MemObjStore(&sCoalRet,pTos);` |
|     ! 0 | 1715 | `									pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1716 | `									PH7_MemObjRelease(&sCoalRet);` |
|     ! 0 | 1717 | `								}` |
|       5 | 1718 | `								PH7_ClassInstanceUnref(pThis);` |
|       5 | 1719 | `								VM_EXIT_BREAK;` |
|       - | 1720 | `							}` |
|       5 | 1721 | `							if( bSet ){` |
|     ! 0 | 1722 | `								if( VmMemberCtxWantsValue(pInstr->iP2) ){` |
|     ! 0 | 1723 | `									ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       - | 1724 | `									ph7_value sEmptyVal;` |
|     ! 0 | 1725 | `									PH7_MemObjInit(pVm,&sEmptyVal);` |
|     ! 0 | 1726 | `									if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|     ! 0 | 1727 | `										VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     ! 0 | 1728 | `										PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|     ! 0 | 1729 | `										VmMagicGuardPop(pVm);` |
|     ! 0 | 1730 | `									}` |
|     ! 0 | 1731 | `									PH7_MemObjStore(&sEmptyVal,pTos);` |
|     ! 0 | 1732 | `									pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1733 | `									PH7_MemObjRelease(&sEmptyVal);` |
|     ! 0 | 1734 | `								}else{` |
|     ! 0 | 1735 | `									pTos->x.iVal = 1;` |
|     ! 0 | 1736 | `									MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     ! 0 | 1737 | `									pTos->nIdx = SXU32_HIGH;` |
|       - | 1738 | `								}` |
|     ! 0 | 1739 | `							}` |
|       5 | 1740 | `							PH7_ClassInstanceUnref(pThis);` |
|       5 | 1741 | `							VM_EXIT_BREAK;` |
|       - | 1742 | `						}` |
|       - | 1743 | `						{` |
|       - | 1744 | `							/* A plain store to an inaccessible property dispatches __set` |
|       - | 1745 | `							 * (band A #3b): park the receiver+name for the following` |
|       - | 1746 | `							 * OP_STORE, exactly like the missing-property case. */` |
|      15 | 1747 | `							VmInstr *pNextW = pInstr + 1;` |
|      15 | 1748 | `							if( pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0 ){` |
|       3 | 1749 | `								ph7_class_method *pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|       3 | 1750 | `								if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){` |
|       3 | 1751 | `									pThis->iRef++;` |
|       3 | 1752 | `									pVm->pMagicSetThis = pThis;` |
|       3 | 1753 | `									SyBlobReset(&pVm->sMagicSetName);` |
|       3 | 1754 | `									SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       3 | 1755 | `									pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|       3 | 1756 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 1757 | `									VM_EXIT_BREAK;` |
|       - | 1758 | `								}` |
|     ! 0 | 1759 | `							}` |
|       - | 1760 | `						}` |
|      10 | 1761 | `						if( ((pInstr + 1)->iOp == PH7_OP_NULLC \|\| (pInstr + 1)->iOp == PH7_OP_NULLC_JMP)` |
|       7 | 1762 | `						 && pInstr->iP2 != PH7_MEMBER_WRITE ){` |
|       - | 1763 | `` 							/* `$o->priv ?? default` (OP_NULLC; NULLC_JMP is the `??=` `` |
|       - | 1764 | ``							 * pre-test): php treats `??` as an isset-style lookup — an`` |
|       - | 1765 | `							 * inaccessible property yields null SILENTLY (the` |
|       - | 1766 | `							 * __isset/__get consults already ran above), letting the` |
|       - | 1767 | `							 * coalesce pick the default. */` |
|     ! 0 | 1768 | `							PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1769 | `							VM_EXIT_BREAK; /* pTos is already the null temp */` |
|       - | 1770 | `						}` |
|       - | 1771 | `						/* A subclass reading a PARENT's PRIVATE property: php treats it as` |
|       - | 1772 | `						 * an UNDEFINED property (the base-private is invisible to the` |
|       - | 1773 | `						 * subclass scope) — a Warning + null, NOT an access Error. Only` |
|       - | 1774 | `						 * this exact shape warns; every other denied read is the catchable` |
|       - | 1775 | `						 * "Cannot access" Error below. */` |
|       - | 1776 | `						{` |
|      12 | 1777 | `						ph7_class *pSelf = VmCurrentSelf(&(*pVm));` |
|      12 | 1778 | `						ph7_class *pDecl = pObjAttr->pAttr->pDeclClass;` |
|      10 | 1779 | `						if( pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      11 | 1780 | `						 && pSelf && pDecl && pSelf != pDecl && PH7_VmInstanceOf(pSelf,pDecl) ){` |
|       3 | 1781 | `							if( !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       4 | 1782 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|       1 | 1783 | `									&pClass->sName,&sName);` |
|       1 | 1784 | `							}` |
|       3 | 1785 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 1786 | `							VM_EXIT_BREAK; /* pTos is already the null temp */` |
|       - | 1787 | `						}` |
|       - | 1788 | `						}` |
|       - | 1789 | `						/* Genuinely inaccessible: php's CATCHABLE Error (parked on the` |
|       - | 1790 | `						 * boundary rail; the fetch-point router lands it — it was an` |
|       - | 1791 | `						 * uncatchable VmReportUncaughtException+Abort before). */` |
|       - | 1792 | `						{` |
|       - | 1793 | `						SyBlob sErrMsg;` |
|      10 | 1794 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      10 | 1795 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      10 | 1796 | `						SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",` |
|       4 | 1797 | `							zVis,&pClass->sName,&sName);` |
|      10 | 1798 | `						PH7_ClassInstanceUnref(pThis);` |
|      10 | 1799 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      10 | 1800 | `						SyBlobRelease(&sErrMsg);` |
|      10 | 1801 | `						VM_EXIT_BREAK;` |
|       - | 1802 | `						}` |
|       - | 1803 | `					}` |
|   52627 | 1804 | `				}` |
|       - | 1805 | `				/* Safely unreference the object */` |
|  105339 | 1806 | `				PH7_ClassInstanceUnref(pThis);` |
|       - | 1807 | `			}` |
|  110449 | 1808 | `		}else{` |
|       - | 1809 | ``			/* `->` on a non-object (e.g. a null intermediate). Silent in isset()/empty()/unset()`` |
|       - | 1810 | ``			 * context (iP2 2/3/4) so `isset($o->missing->x)` / `unset($o->missing->x)` match PHP. */`` |
|     104 | 1811 | `			if( pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1812 | `				/* php draws a sharp line here, and PH7 drew none: CALLING a method on a` |
|       - | 1813 | `				 * non-object is a catchable Error, while READING a property off one is a` |
|       - | 1814 | `				 * Warning that yields NULL. PH7 raised the same notice for both and` |
|       - | 1815 | ``				 * carried on with NULL, so `$null->m()` silently did nothing. */`` |
|       - | 1816 | `				SyString sMemb;` |
|     100 | 1817 | `				const char *zVerb = 0;` |
|     100 | 1818 | `				SyStringInitFromBuf(&sMemb,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     100 | 1819 | `				if( pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|       - | 1820 | ``					/* A deferred call ARGUMENT (`f($u->p)`): which diagnostic php raises`` |
|       - | 1821 | `					 * depends on the parameter, and this op does not know it — a by-VALUE` |
|       - | 1822 | `					 * binding is the read warning below, a by-REFERENCE one is php's` |
|       - | 1823 | ``					 * `Attempt to modify property` Error. Record the step rooted at the`` |
|       - | 1824 | `					 * base and let OP_CALL re-drive it in the mode the callee decides,` |
|       - | 1825 | `					 * exactly as a property MISSING on a real object is recorded. */` |
|      21 | 1826 | `					VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|      21 | 1827 | `					if( pPath && VmDeferPathPushProp(pPath,&sMemb) == SXRET_OK ){` |
|      21 | 1828 | `						VmPopOperand(&pTos,1);   /* drop the property name */` |
|      21 | 1829 | `						PH7_MemObjRelease(pTos); /* collapse the base slot into the carrier */` |
|      21 | 1830 | `						pTos->x.pOther = pPath;` |
|      21 | 1831 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      21 | 1832 | `						pTos->nIdx = SXU32_HIGH;` |
|      44 | 1833 | `						VM_EXIT_BREAK;` |
|       - | 1834 | `					}` |
|     ! 0 | 1835 | `					if( pPath ){` |
|     ! 0 | 1836 | `						VmFreeDeferredPath(pPath);` |
|     ! 0 | 1837 | `					}` |
|       - | 1838 | `					/* fall through to the immediate warning on allocation failure */` |
|     ! 0 | 1839 | `				}` |
|       - | 1840 | `				/* WRITING one is an Error too, and php picks its verb from what the` |
|       - | 1841 | `				 * write actually is. PH7 warned about a READ it never performed and` |
|       - | 1842 | `				 * then let the store fail into its own` |
|       - | 1843 | `				 * "Cannot perform assignment on a constant class attribute", so the` |
|       - | 1844 | `				 * script carried on past a statement php stops it for. The kind is` |
|       - | 1845 | `				 * read off the following instruction, the way every other write` |
|       - | 1846 | `				 * classification in this handler is (VmMemberFetchForWrite). */` |
|      80 | 1847 | `				if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|      31 | 1848 | `					const VmInstr *pNextW = pInstr + 1;` |
|      30 | 1849 | `					if( (pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0)` |
|      21 | 1850 | ``					 \|\| pNextW->iOp == PH7_OP_NULLC_JMP /* `$u->p ??= v` */`` |
|      12 | 1851 | `					 \|\| VmNextIsCompoundAssign(pNextW) ){` |
|      23 | 1852 | `						zVerb = "assign";` |
|      20 | 1853 | `					}else if( pNextW->iOp == PH7_OP_INCR \|\| pNextW->iOp == PH7_OP_DECR ){` |
|       5 | 1854 | `						zVerb = "increment/decrement";` |
|       3 | 1855 | `					}else{` |
|       - | 1856 | ``						/* The base of a subscript write (`$u->p[] = v`): php asks the`` |
|       - | 1857 | `						 * property for something to modify, and there is no property. */` |
|       5 | 1858 | `						zVerb = "modify";` |
|       1 | 1859 | `					}` |
|      65 | 1860 | `				}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       3 | 1861 | `					zVerb = "assign";` |
|      49 | 1862 | `				}else if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       3 | 1863 | `					zVerb = "modify";` |
|       1 | 1864 | `				}` |
|      80 | 1865 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD \|\| zVerb ){` |
|       - | 1866 | `					SyBlob sErrM;` |
|       - | 1867 | `					sxi32 rcErr;` |
|      47 | 1868 | `					SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      47 | 1869 | `					if( zVerb ){` |
|      35 | 1870 | `						SyBlobFormat(&sErrM,"Attempt to %s property \"%z\" on %s",` |
|      17 | 1871 | `							zVerb,&sMemb,VmArithValueName(pNos));` |
|      18 | 1872 | `					}else{` |
|      13 | 1873 | `						SyBlobFormat(&sErrM,"Call to a member function %z() on %s",` |
|       6 | 1874 | `							&sMemb,VmArithValueName(pNos));` |
|       - | 1875 | `					}` |
|      47 | 1876 | `					VmPopOperand(&pTos,1);` |
|      47 | 1877 | `					PH7_MemObjRelease(pTos);` |
|      47 | 1878 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      47 | 1879 | `					pTos->nIdx = SXU32_HIGH;` |
|      70 | 1880 | `					rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      23 | 1881 | `						SyBlobLength(&sErrM));` |
|      47 | 1882 | `					SyBlobRelease(&sErrM);` |
|      47 | 1883 | `					if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      47 | 1884 | `					rc = rcErr;` |
|      49 | 1885 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1886 | `				}` |
|      50 | 1887 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Attempt to read property \"%z\" on %s",` |
|      16 | 1888 | `					&sMemb,VmArithValueName(pNos));` |
|      16 | 1889 | `			}` |
|      38 | 1890 | `			VmPopOperand(&pTos,1);` |
|      38 | 1891 | `			PH7_MemObjRelease(pTos);` |
|      38 | 1892 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|       - | 1893 | `		}` |
|  110467 | 1894 | `	}else{` |
|       - | 1895 | `		/* Static member access using class name */` |
|  102745 | 1896 | `		pNos = pTos;` |
|  102745 | 1897 | `		pThis = 0;` |
|  102745 | 1898 | `		if( !pInstr->p3 ){` |
|    2497 | 1899 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    2497 | 1900 | `			pNos--;` |
|       - | 1901 | `#ifdef UNTRUST` |
|       - | 1902 | `			if( pNos < pStack ){` |
|       - | 1903 | `				VM_EXIT_ABORT;` |
|       - | 1904 | `			}` |
|       - | 1905 | `#endif` |
|    1251 | 1906 | `		}else{` |
|       - | 1907 | `			/* Attribute name already computed */` |
|  100253 | 1908 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|       - | 1909 | `		}` |
|  102745 | 1910 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|  102741 | 1911 | `			ph7_class *pClass = 0;` |
|       - | 1912 | `			/* A FORWARDING static call — self::/parent::/static:: — preserves the` |
|       - | 1913 | `			 * caller's late-static-binding class (php); a non-forwarding C::m() resets` |
|       - | 1914 | `			 * it to C. The receiver slot below keeps the literal keyword, which OP_CALL` |
|       - | 1915 | `			 * can't resolve, so it would fall back to the callee's DECLARING class and` |
|       - | 1916 | `			 * lose LSB. Remember the live LSB class here and stamp it onto the receiver` |
|       - | 1917 | `			 * after the method name is pushed. */` |
|  102741 | 1918 | `			int bForwardingCall = 0;` |
|  102741 | 1919 | `			ph7_class *pForwardLsb = 0;` |
|       - | 1920 | `			/* Same rail snapshot as OP_NEW: the lookup below can run an autoloader that` |
|       - | 1921 | `			 * throws, and php then reports that exception and nothing else. */` |
|  102741 | 1922 | `			sxi32 nMbBrc = pVm->nBoundaryRc;` |
|  102741 | 1923 | `			const void *pMbRes = (const void *)pVm->pResumeFrame;` |
|  102741 | 1924 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - | 1925 | `				/* Class already instantiated */` |
|      18 | 1926 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      18 | 1927 | `				pClass = pThis->pClass;` |
|      18 | 1928 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      10 | 1929 | `			}else{` |
|       - | 1930 | `				/* Try to extract the target class */` |
|  102725 | 1931 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|  102725 | 1932 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|  102725 | 1933 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|       - | 1934 | `					/* Handle self/static/parent keywords */` |
|  102725 | 1935 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       - | 1936 | `						/* In a trait method, self:: resolves to the USING class */` |
|     267 | 1937 | `						pClass = PH7_VmPeekSelfClass(&(*pVm));` |
|     267 | 1938 | `						bForwardingCall = 1;` |
|     267 | 1939 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|  102594 | 1940 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|     106 | 1941 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|     106 | 1942 | `						bForwardingCall = 1;` |
|     106 | 1943 | `						pForwardLsb = pClass;` |
|  102412 | 1944 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|     113 | 1945 | `						pClass = PH7_VmResolveParentClass(&(*pVm));` |
|     113 | 1946 | `						bForwardingCall = 1;` |
|     113 | 1947 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|      58 | 1948 | `					}else{` |
|  102251 | 1949 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|       - | 1950 | `					}` |
|   51360 | 1951 | `				}` |
|       - | 1952 | `			}` |
|  102741 | 1953 | `			if( pClass == 0 ){` |
|       - | 1954 | `				/* Undefined class: php throws a catchable Error */` |
|       - | 1955 | `				SyBlob sErrM;` |
|       - | 1956 | `				sxi32 rcErr;` |
|      15 | 1957 | `				if( PH7_VmClassLookupRaised(&(*pVm),nMbBrc,pMbRes) ){` |
|       - | 1958 | `					/* ...unless an autoloader raised: land THAT instead of reporting the` |
|       - | 1959 | `					 * class missing on top of it. */` |
|      10 | 1960 | `					sxi32 rcAutoM = pVm->nBoundaryRc;` |
|      10 | 1961 | `					pVm->nBoundaryRc = 0;` |
|      10 | 1962 | `					if( !pInstr->p3 ){` |
|       8 | 1963 | `						VmPopOperand(&pTos,1);` |
|       3 | 1964 | `					}` |
|      10 | 1965 | `					PH7_MemObjRelease(pTos);` |
|      10 | 1966 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      10 | 1967 | `					pTos->nIdx = SXU32_HIGH;` |
|      10 | 1968 | `					if( rcAutoM == PH7_ABORT ){` |
|     ! 0 | 1969 | `						VM_EXIT_ABORT;` |
|       - | 1970 | `					}` |
|      10 | 1971 | `					rc = PH7_EXCEPTION;` |
|      10 | 1972 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1973 | `				}` |
|       6 | 1974 | `				SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       6 | 1975 | `				SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|       4 | 1976 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob));` |
|       6 | 1977 | `				if( !pInstr->p3 ){` |
|       6 | 1978 | `					VmPopOperand(&pTos,1);` |
|       2 | 1979 | `				}` |
|       6 | 1980 | `				PH7_MemObjRelease(pTos);` |
|       6 | 1981 | `				pTos->nIdx = SXU32_HIGH;` |
|       8 | 1982 | `				rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       2 | 1983 | `					SyBlobLength(&sErrM));` |
|       6 | 1984 | `				SyBlobRelease(&sErrM);` |
|       6 | 1985 | `				if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       6 | 1986 | `				rc = rcErr;` |
|       6 | 1987 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 1988 | `			}else{` |
|  102729 | 1989 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - | 1990 | `					/* Method call */` |
|    1407 | 1991 | `					ph7_class_method *pMeth = 0;` |
|    1407 | 1992 | `					if( sName.nByte > 0 && (pClass->iFlags & PH7_CLASS_INTERFACE) == 0){` |
|       - | 1993 | `						/* Extract the target method */` |
|    1407 | 1994 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|     701 | 1995 | `					}` |
|    1407 | 1996 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      65 | 1997 | `						if( pMeth ){` |
|       - | 1998 | `							SyBlob sErrM;` |
|       - | 1999 | `							sxi32 rcErr;` |
|       3 | 2000 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       3 | 2001 | `							SyBlobFormat(&sErrM,"Cannot call abstract method %z::%z()",` |
|       1 | 2002 | `								&pClass->sName,&sName);` |
|       3 | 2003 | `							if( !pInstr->p3 ){` |
|       3 | 2004 | `								VmPopOperand(&pTos,1);` |
|       1 | 2005 | `							}` |
|       3 | 2006 | `							PH7_MemObjRelease(pTos);` |
|       3 | 2007 | `							pTos->nIdx = SXU32_HIGH;` |
|       4 | 2008 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       1 | 2009 | `								SyBlobLength(&sErrM));` |
|       3 | 2010 | `							SyBlobRelease(&sErrM);` |
|       3 | 2011 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 | 2012 | `							rc = rcErr;` |
|       3 | 2013 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 2014 | `						}else{` |
|      63 | 2015 | `							ph7_class_instance *pMagicThis = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|      63 | 2016 | `							ph7_class_method *pCallStaticMagic = pMagicThis` |
|      16 | 2017 | `								? PH7_ClassExtractMethod(pMagicThis->pClass,"__call",sizeof("__call")-1)` |
|      52 | 2018 | `								: PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1);` |
|      63 | 2019 | `							if( pCallStaticMagic ){` |
|       - | 2020 | `								/* php: C::missing(...) dispatches __callStatic($name,$args)` |
|       - | 2021 | `								 * through the packing body (see the instance twin) — or` |
|       - | 2022 | `								 * __call($name,$args) on the calling frame's own $this when` |
|       - | 2023 | `								 * that receiver fits C (VmStaticCallMagicThis). */` |
|      81 | 2024 | `								VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pMagicThis,` |
|      26 | 2025 | `									pMagicThis ? pMagicThis->pClass : pClass,&sName);` |
|      55 | 2026 | `								if( pPend == 0 ){` |
|     ! 0 | 2027 | `									VM_EXIT_ABORT;` |
|       - | 2028 | `								}` |
|      55 | 2029 | `								if( !pInstr->p3 ){` |
|      55 | 2030 | `									VmPopOperand(&pTos,1);` |
|      26 | 2031 | `								}` |
|      55 | 2032 | `								PH7_MemObjRelease(pTos);` |
|      55 | 2033 | `								pTos->x.pOther = pPend;` |
|      55 | 2034 | `								pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|      55 | 2035 | `								pTos->nIdx = SXU32_HIGH;` |
|      55 | 2036 | `								VM_EXIT_BREAK;` |
|       - | 2037 | `							}` |
|       - | 2038 | `							{` |
|       - | 2039 | `								/* php: the STATIC form reports the same "Call to undefined` |
|       - | 2040 | `								 * method C::m()" as the instance one. */` |
|       - | 2041 | `								SyBlob sErrM;` |
|       - | 2042 | `								sxi32 rcErr;` |
|       9 | 2043 | `								SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       9 | 2044 | `								SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",` |
|       4 | 2045 | `									&pClass->sName,&sName);` |
|       9 | 2046 | `								if( !pInstr->p3 ){` |
|       9 | 2047 | `									VmPopOperand(&pTos,1);` |
|       4 | 2048 | `								}` |
|       9 | 2049 | `								PH7_MemObjRelease(pTos);` |
|       9 | 2050 | `								pTos->nIdx = SXU32_HIGH;` |
|      13 | 2051 | `								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       4 | 2052 | `									SyBlobLength(&sErrM));` |
|       9 | 2053 | `								SyBlobRelease(&sErrM);` |
|       9 | 2054 | `								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 | 2055 | `								rc = rcErr;` |
|      11 | 2056 | `								PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2057 | `							}` |
|       - | 2058 | `						}` |
|       - | 2059 | `						/* Pop the method name from the stack */` |
|     ! 0 | 2060 | `						if( !pInstr->p3 ){` |
|     ! 0 | 2061 | `							VmPopOperand(&pTos,1);` |
|       - | 2062 | `						}` |
|     ! 0 | 2063 | `						PH7_MemObjRelease(pTos);` |
|     ! 0 | 2064 | `					}else{` |
|       - | 2065 | `						/* Inaccessible from this scope: php routes through the same fallback a` |
|       - | 2066 | ``						 * MISSING name takes — __call on a compatible `$this`, else`` |
|       - | 2067 | ``						 * __callStatic — which is why `C::privateStatic()` runs a catch-all`` |
|       - | 2068 | `						 * instead of the "Call to private method" Error OP_CALL would raise` |
|       - | 2069 | `						 * below. */` |
|    1345 | 2070 | `						ph7_class_method *pDeniedStatic = 0;` |
|    1345 | 2071 | `						ph7_class_instance *pDeniedThis = 0;` |
|    1345 | 2072 | `						int bDeniedStatic = 0;` |
|    1345 | 2073 | `						if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - | 2074 | `							/* The OWNING class decides, as in the instance twin above: a` |
|       - | 2075 | `							 * trait method's rules belong to the class that composed it. */` |
|      29 | 2076 | `							ph7_class *pOwnerCls = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|      29 | 2077 | `							if( !PH7_VmClassMemberAccess(&(*pVm),pOwnerCls,&sName,pMeth->iProtection,FALSE) ){` |
|      15 | 2078 | `								pDeniedThis = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|      15 | 2079 | `								pDeniedStatic = pDeniedThis` |
|       4 | 2080 | `									? PH7_ClassExtractMethod(pDeniedThis->pClass,"__call",` |
|       - | 2081 | `										sizeof("__call")-1)` |
|      12 | 2082 | `									: PH7_ClassExtractMethod(pClass,"__callStatic",` |
|       - | 2083 | `										sizeof("__callStatic")-1);` |
|      15 | 2084 | `								bDeniedStatic = pDeniedStatic == 0;` |
|       7 | 2085 | `							}` |
|      14 | 2086 | `						}` |
|    1345 | 2087 | `						if( bDeniedStatic ){` |
|       - | 2088 | `							/* No catch-all answers for it: php's visibility Error, raised at` |
|       - | 2089 | `							 * the resolution like the instance twin above. OP_CALL's own` |
|       - | 2090 | `							 * screen re-derives the method from the FUNCTION's name against` |
|       - | 2091 | `							 * its declaring class, which cannot see a trait adaptation's` |
|       - | 2092 | ``							 * composed protection — `sHi as private sPriv` ran from global`` |
|       - | 2093 | `							 * scope in silence. */` |
|       - | 2094 | `							SyBlob sErrM;` |
|       - | 2095 | `							sxi32 rcErr;` |
|       7 | 2096 | `							ph7_class *pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|       7 | 2097 | `							ph7_class *pScope = PH7_VmCallerScopeName(&(*pVm));` |
|      10 | 2098 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       3 | 2099 | `								? "private" : "protected";` |
|       7 | 2100 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       7 | 2101 | `							if( pScope ){` |
|       3 | 2102 | `								SyBlobFormat(&sErrM,"Call to %s method %z::%z() from scope %z",` |
|       1 | 2103 | `									zVis,&pOwner->sName,&sName,&pScope->sName);` |
|       2 | 2104 | `							}else{` |
|       5 | 2105 | `								SyBlobFormat(&sErrM,"Call to %s method %z::%z() from global scope",` |
|       2 | 2106 | `									zVis,&pOwner->sName,&sName);` |
|       - | 2107 | `							}` |
|       7 | 2108 | `							if( !pInstr->p3 ){` |
|       7 | 2109 | `								VmPopOperand(&pTos,1);` |
|       3 | 2110 | `							}` |
|       7 | 2111 | `							PH7_MemObjRelease(pTos);` |
|       7 | 2112 | `							pTos->nIdx = SXU32_HIGH;` |
|      10 | 2113 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       3 | 2114 | `								SyBlobLength(&sErrM));` |
|       7 | 2115 | `							SyBlobRelease(&sErrM);` |
|       7 | 2116 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       7 | 2117 | `							rc = rcErr;` |
|       7 | 2118 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2119 | `						}` |
|    1339 | 2120 | `						if( pDeniedStatic ){` |
|       - | 2121 | `							/* The packing body takes no receiver slot: drop the method-name` |
|       - | 2122 | `							 * slot so the MARK lands on the receiver slot, exactly as the` |
|       - | 2123 | `							 * missing-method twin above does (leaving the class name in place` |
|       - | 2124 | `							 * would pack it as the first $args entry). */` |
|      13 | 2125 | `							VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pDeniedThis,` |
|       4 | 2126 | `								pDeniedThis ? pDeniedThis->pClass : pClass,&sName);` |
|       9 | 2127 | `							if( pPend == 0 ){` |
|     ! 0 | 2128 | `								VM_EXIT_ABORT;` |
|       - | 2129 | `							}` |
|       9 | 2130 | `							if( !pInstr->p3 ){` |
|       9 | 2131 | `								VmPopOperand(&pTos,1);` |
|       4 | 2132 | `							}` |
|       9 | 2133 | `							PH7_MemObjRelease(pTos);` |
|       9 | 2134 | `							pTos->x.pOther = pPend;` |
|       9 | 2135 | `							pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|       9 | 2136 | `							pTos->nIdx = SXU32_HIGH;` |
|       9 | 2137 | `							VM_EXIT_BREAK;` |
|       - | 2138 | `						}` |
|       - | 2139 | `						/* Push method name on the stack, MARKED as already screened (see the` |
|       - | 2140 | `						 * instance twin: OP_CALL cannot re-derive a composed protection). */` |
|    1331 | 2141 | `						PH7_MemObjRelease(pTos);` |
|    1331 | 2142 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|    1331 | 2143 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|    1331 | 2144 | `						pTos->iFlags \|= MEMOBJ_AUX_MEMBERCALL;` |
|       - | 2145 | `					}` |
|    1331 | 2146 | `					pTos->nIdx = SXU32_HIGH;` |
|       - | 2147 | `					/* Forwarding call (self::/parent::/static::): overwrite the receiver` |
|       - | 2148 | `					 * slot (pNos, one below the method name) with the live LSB class name` |
|       - | 2149 | ``					 * so OP_CALL pushes THAT onto aSelf — preserving `static::` inside the`` |
|       - | 2150 | `					 * callee. Without this the literal keyword falls through to the` |
|       - | 2151 | `					 * callee's declaring class. Only when an LSB class is actually in` |
|       - | 2152 | `					 * scope (a static call from global scope has none). */` |
|    1331 | 2153 | `					if( bForwardingCall && pForwardLsb && (pNos->iFlags & MEMOBJ_STRING) ){` |
|     161 | 2154 | `						SyBlobReset(&pNos->sBlob);` |
|     161 | 2155 | `						SyBlobAppend(&pNos->sBlob,pForwardLsb->sName.zString,pForwardLsb->sName.nByte);` |
|      79 | 2156 | `					}` |
|     668 | 2157 | `				}else{` |
|       - | 2158 | `					/* Attribute access */` |
|  101327 | 2159 | `					ph7_class_attr *pAttr = 0;` |
|  101327 | 2160 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - | 2161 | `						/* unset(C::$x): PHP rejects unsetting a static property with a fatal Error.` |
|       - | 2162 | `						 * Without this the iP2=unset tag falls through to a normal static read and the` |
|       - | 2163 | `						 * trailing generic unset() would silently NULL (and de-type) the shared slot. */` |
|       - | 2164 | `						char zMsg[256];` |
|     ! 0 | 2165 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Attempt to unset static property %.*s::$%.*s",` |
|     ! 0 | 2166 | `							(int)pClass->sName.nByte,pClass->sName.zString,(int)sName.nByte,sName.zString);` |
|     ! 0 | 2167 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|     ! 0 | 2168 | `						VM_EXIT_ABORT;` |
|       - | 2169 | `					}` |
|       - | 2170 | `					/* Check for special ::class pseudo-constant */` |
|  101463 | 2171 | `					if( sName.nByte == sizeof("class")-1 &&` |
|     272 | 2172 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|       - | 2173 | `						/* ::class returns the fully qualified class name */` |
|       - | 2174 | `						/* Pop the attribute name from the stack */` |
|     153 | 2175 | `						if( !pInstr->p3 ){` |
|     153 | 2176 | `							VmPopOperand(&pTos,1);` |
|      74 | 2177 | `						}` |
|     153 | 2178 | `						PH7_MemObjRelease(pTos);` |
|       - | 2179 | `						/* Load the class name */` |
|     153 | 2180 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|     153 | 2181 | `						pTos->nIdx = SXU32_HIGH;` |
|      79 | 2182 | `					}else{` |
|       - | 2183 | `						/* Extract the target attribute. php keeps constants and` |
|       - | 2184 | `						 * (static) properties in separate namespaces; the source` |
|       - | 2185 | ``						 * form disambiguates (set by the `::` codegen, compile.c):`` |
|       - | 2186 | ``						 * a `$`-form is a STATIC PROPERTY (hAttr) — either a literal`` |
|       - | 2187 | ``						 * name folded into pInstr->p3 (`C::$s`) or a dynamic name on`` |
|       - | 2188 | ``						 * the stack marked iP1==2 (`C::$$x`, `C::${$e}`); a bareword`` |
|       - | 2189 | `						 * (p3==0, iP1==1) is a CONSTANT or enum case (hConst). This` |
|       - | 2190 | ``						 * is what lets `const C` and `public $C` coexist and resolve`` |
|       - | 2191 | `						 * to the right member. */` |
|  101179 | 2192 | `						if( sName.nByte > 0 ){` |
|  101644 | 2193 | `							pAttr = (pInstr->p3 \|\| pInstr->iP1 == 2)` |
|  100250 | 2194 | `								? PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte)` |
|   51511 | 2195 | `								: PH7_ClassExtractConstant(pClass,sName.zString,sName.nByte);` |
|   50587 | 2196 | `						}` |
|  101179 | 2197 | `						if( pAttr == 0 ){` |
|       - | 2198 | `							/* No such member. php raises a catchable Error whose wording` |
|       - | 2199 | `							 * depends on the ACCESS form (the same p3/iP1 signal used above):` |
|       - | 2200 | ``							 * a bareword `C::MISSING` (constant form) is "Undefined constant`` |
|       - | 2201 | ``							 * C::MISSING", a `$`-form `C::$missing` is "Access to undeclared`` |
|       - | 2202 | `							 * static property C::$missing". Instance magic (__get) is never` |
|       - | 2203 | `							 * consulted for statics (band A #3b). isset()/empty() context` |
|       - | 2204 | `							 * stays silently false. Parked on the boundary rail; the op` |
|       - | 2205 | `							 * completes benignly with NULL and the fetch-point router lands` |
|       - | 2206 | `							 * the throw. */` |
|      14 | 2207 | `							if( !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2208 | `								SyBlob sErrMsg;` |
|      14 | 2209 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      14 | 2210 | `								if( pInstr->p3 == 0 && pInstr->iP1 != 2 ){` |
|       7 | 2211 | `									SyBlobFormat(&sErrMsg,"Undefined constant %z::%z",` |
|       3 | 2212 | `										&pClass->sName,&sName);` |
|       4 | 2213 | `								}else{` |
|       8 | 2214 | `									SyBlobFormat(&sErrMsg,"Access to undeclared static property %z::$%z",` |
|       3 | 2215 | `										&pClass->sName,&sName);` |
|       - | 2216 | `								}` |
|      14 | 2217 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       6 | 2218 | `							}` |
|       6 | 2219 | `						}` |
|       - | 2220 | `						/* Pop the attribute name from the stack */` |
|  101179 | 2221 | `						if( !pInstr->p3 ){` |
|     935 | 2222 | `							VmPopOperand(&pTos,1);` |
|     465 | 2223 | `						}` |
|  101179 | 2224 | `						PH7_MemObjRelease(pTos);` |
|  101179 | 2225 | `						pTos->nIdx = SXU32_HIGH;` |
|  101179 | 2226 | `						if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - | 2227 | ``							/* `self::$s =& $x` / `C::$s =& $x`: stash the static property slot`` |
|       - | 2228 | `							 * for the following member-marked OP_STORE_REF (class-level, shared` |
|       - | 2229 | `							 * across instances — matches php). Skip the read machinery below. */` |
|      14 | 2230 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|      14 | 2231 | `							 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|      17 | 2232 | `							 && VmClassStaticDeferPending(pClass) ){` |
|       - | 2233 | `								/* Binding a reference TO a static touches the table, so it` |
|       - | 2234 | `								 * materializes first, like every other static access. */` |
|       3 | 2235 | `								sxi32 rcRt = PH7_VmMaterializeClassStatics(&(*pVm),pClass);` |
|       3 | 2236 | `								if( rcRt != SXRET_OK ){` |
|       3 | 2237 | `									pVm->pRefTargetStaticAttr = 0;` |
|       3 | 2238 | `									pVm->pRefTargetAttr = 0;` |
|       3 | 2239 | `									pVm->pRefTargetThis = 0;` |
|       3 | 2240 | `									if( pThis ){` |
|     ! 0 | 2241 | `										PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2242 | `									}` |
|       3 | 2243 | `									if( rcRt == PH7_ABORT ){` |
|     ! 0 | 2244 | `										VM_EXIT_ABORT;` |
|       - | 2245 | `									}` |
|       - | 2246 | `									{` |
|       - | 2247 | `										sxi32 iRpR;` |
|       3 | 2248 | `										if( VmRecordedResume(pVm,&iRpR,pState->pEntryFrame,aInstr) ){` |
|       7 | 2249 | `											PH7_RESUME_DRAIN()` |
|       3 | 2250 | `											pc = iRpR;` |
|       3 | 2251 | `											VM_EXIT_BREAK;` |
|       - | 2252 | `										}` |
|       - | 2253 | `									}` |
|     ! 0 | 2254 | `									VM_EXIT_EXCEPTION;` |
|       - | 2255 | `								}` |
|     ! 0 | 2256 | `							}` |
|      12 | 2257 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|      14 | 2258 | `							 && PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|      14 | 2259 | `								pVm->pRefTargetStaticAttr = pAttr;` |
|      14 | 2260 | `								pVm->pRefTargetAttr = 0;` |
|      14 | 2261 | `								pVm->pRefTargetThis = 0;` |
|      14 | 2262 | `								pTos->nIdx = pAttr->nIdx;` |
|       8 | 2263 | `							}else{` |
|     ! 0 | 2264 | `								pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 2265 | `								pVm->pRefTargetAttr = 0;` |
|     ! 0 | 2266 | `								pVm->pRefTargetThis = 0;` |
|       - | 2267 | `							}` |
|      14 | 2268 | `							if( pThis ){` |
|     ! 0 | 2269 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2270 | `							}` |
|      14 | 2271 | `							VM_EXIT_BREAK;` |
|       - | 2272 | `						}` |
|  101165 | 2273 | `						if( pAttr ){` |
|  101153 | 2274 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       - | 2275 | `								/* Access to a non static attribute */` |
|     ! 0 | 2276 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|     ! 0 | 2277 | `									&pClass->sName,&pAttr->sName` |
|       - | 2278 | `									);` |
|     ! 0 | 2279 | `							}else{` |
|       - | 2280 | `								ph7_value *pValue;` |
|       - | 2281 | `								/* php materializes the class's static table at the FIRST` |
|       - | 2282 | `								 * static-property access (any property, any context — read,` |
|       - | 2283 | `								 * write, even isset): a default whose evaluation was DEFERRED` |
|       - | 2284 | `								 * (it threw at the declaration) runs here, and a typed one that` |
|       - | 2285 | `								 * failed its check throws its catchable TypeError here. Class` |
|       - | 2286 | `								 * constants and static method calls do not trigger the` |
|       - | 2287 | `								 * materialization (php-exact). */` |
|  101148 | 2288 | `								if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|  100689 | 2289 | `								 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|  100235 | 2290 | `								 && VmClassStaticDeferPending(pClass) ){` |
|      48 | 2291 | `									sxi32 rcD = PH7_VmMaterializeClassStatics(&(*pVm),pClass);` |
|      48 | 2292 | `									if( rcD != SXRET_OK ){` |
|      44 | 2293 | `										if( pThis ){` |
|     ! 0 | 2294 | `											PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2295 | `										}` |
|      44 | 2296 | `										if( rcD == PH7_ABORT ){` |
|     ! 0 | 2297 | `											VM_EXIT_ABORT;` |
|       - | 2298 | `										}` |
|       - | 2299 | `										{` |
|       - | 2300 | `											sxi32 iRpD;` |
|      44 | 2301 | `											if( VmRecordedResume(pVm,&iRpD,pState->pEntryFrame,aInstr) ){` |
|      88 | 2302 | `												PH7_RESUME_DRAIN()` |
|      40 | 2303 | `												pc = iRpD;` |
|      40 | 2304 | `												VM_EXIT_BREAK;` |
|       - | 2305 | `											}` |
|       - | 2306 | `										}` |
|       5 | 2307 | `										VM_EXIT_EXCEPTION;` |
|       - | 2308 | `									}` |
|       2 | 2309 | `								}` |
|       - | 2310 | `								/* Check if the access to the attribute is allowed */` |
|  101111 | 2311 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|       - | 2312 | `									/* PHP 7.4+: uninitialized typed static read.` |
|       - | 2313 | `									 * Same LHS-of-store peek as the instance path. */` |
|  101102 | 2314 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|  100613 | 2315 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|  150107 | 2316 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|  100068 | 2317 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|  100073 | 2318 | `										if( pS ){` |
|  100073 | 2319 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|  100073 | 2320 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|  100014 | 2321 | `												VmInstr *pNext = pInstr + 1;` |
|  100014 | 2322 | `												int bIsLhs = 0;` |
|  100014 | 2323 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       9 | 2324 | `													bIsLhs = 1;` |
|       3 | 2325 | `												}` |
|  100014 | 2326 | `												if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 2327 | `													/* Destructuring target ([S::$s] = [...]):` |
|       - | 2328 | `													 * initialized by the OP_LOAD_LIST store. */` |
|       3 | 2329 | `													bIsLhs = 1;` |
|       1 | 2330 | `												}` |
|  100014 | 2331 | `												if( !bIsLhs ){` |
|  100004 | 2332 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|  100004 | 2333 | `													if( pThis ){` |
|     ! 0 | 2334 | `														PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2335 | `													}` |
|  100004 | 2336 | `													if( rcU == PH7_ABORT ){` |
|     ! 0 | 2337 | `														VM_EXIT_ABORT;` |
|       - | 2338 | `													}` |
|       - | 2339 | `													{` |
|       - | 2340 | `														sxi32 iRp;` |
|  100004 | 2341 | `														if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|  200006 | 2342 | `															PH7_RESUME_DRAIN()` |
|  100004 | 2343 | `															pc = iRp;` |
|  100004 | 2344 | `															VM_EXIT_BREAK;` |
|       - | 2345 | `														}` |
|       - | 2346 | `													}` |
|     ! 0 | 2347 | `													VM_EXIT_EXCEPTION;` |
|       - | 2348 | `												}` |
|       4 | 2349 | `											}` |
|      33 | 2350 | `										}` |
|      33 | 2351 | `									}` |
|    1100 | 2352 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|    1012 | 2353 | `									 && SySetUsed(&pAttr->aAttrs) > 0 ){` |
|       - | 2354 | `										/* php 8.4 #[\Deprecated] on a class constant:` |
|       - | 2355 | `										 * every access re-warns. */` |
|      11 | 2356 | `										VmDeprecatedConstNotice(&(*pVm),pClass,pAttr);` |
|       5 | 2357 | `									}` |
|    1105 | 2358 | `									if( pAttr->nIdx == SXU32_HIGH ){` |
|       - | 2359 | `										/* Unmaterialized slot. Enum case: first access` |
|       - | 2360 | `										 * materializes ALL the singletons. Plain constant: its` |
|       - | 2361 | `										 * initializer hasn't run yet (mount-order-dependent` |
|       - | 2362 | `										 * cross-constant reference) — evaluate on demand. A` |
|       - | 2363 | `										 * raised TypeError/Error (backing mismatch, duplicate` |
|       - | 2364 | `										 * value, self-reference) parks on the boundary rail;` |
|       - | 2365 | `										 * the op completes benignly with NULL and the` |
|       - | 2366 | `										 * fetch-point router lands the throw. */` |
|       - | 2367 | `										sxi32 rcEnum;` |
|     469 | 2368 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|       - | 2369 | `											/* php: a DIRECT static access evaluates every case` |
|       - | 2370 | `											 * of the enum (whole-class constant update) — a` |
|       - | 2371 | `											 * broken sibling case throws here too. A reference` |
|       - | 2372 | `											 * from inside another constant's initializer` |
|       - | 2373 | `											 * (nConstEvalDepth > 0) evaluates only the` |
|       - | 2374 | `											 * requested case. */` |
|      53 | 2375 | `											if( pVm->nConstEvalDepth > 0 ){` |
|       3 | 2376 | `												rcEnum = VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|       2 | 2377 | `											}else{` |
|      51 | 2378 | `												rcEnum = VmEnumMaterialize(&(*pVm),pClass);` |
|       - | 2379 | `											}` |
|      28 | 2380 | `										}else{` |
|     419 | 2381 | `											rcEnum = VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|       - | 2382 | `										}` |
|     469 | 2383 | `										if( rcEnum != SXRET_OK ){` |
|      41 | 2384 | `											VmBoundaryPark(&(*pVm),rcEnum);` |
|      19 | 2385 | `										}` |
|     232 | 2386 | `									}` |
|       - | 2387 | `									/* Load the desired attribute */` |
|    1105 | 2388 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|    1105 | 2389 | `									if( pValue ){` |
|    1065 | 2390 | `										PH7_MemObjLoad(pValue,pTos);` |
|    1065 | 2391 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       - | 2392 | `											/* Load index number */` |
|     191 | 2393 | `											pTos->nIdx = pAttr->nIdx;` |
|      93 | 2394 | `										}` |
|     530 | 2395 | `									}` |
|     555 | 2396 | `								}else{` |
|       - | 2397 | `									/* Throw Error exception (PHP-compatible) */` |
|       - | 2398 | `									char zMsg[256];` |
|       5 | 2399 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       5 | 2400 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|       7 | 2401 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|       4 | 2402 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|       4 | 2403 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|       3 | 2404 | `									}else{` |
|     ! 0 | 2405 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|     ! 0 | 2406 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|     ! 0 | 2407 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|       - | 2408 | `									}` |
|       5 | 2409 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|       5 | 2410 | `									VM_EXIT_ABORT;` |
|       - | 2411 | `								}` |
|       - | 2412 | `							}` |
|     550 | 2413 | `						}` |
|       - | 2414 | `					}` |
|       - | 2415 | `				}` |
|    2591 | 2416 | `				if( pThis ){` |
|       - | 2417 | `					/* Safely unreference the object */` |
|      18 | 2418 | `					PH7_ClassInstanceUnref(pThis);` |
|       8 | 2419 | `				}` |
|       - | 2420 | `			}` |
|    1298 | 2421 | `		}else{` |
|       - | 2422 | ``			/* `$v::X` where $v holds neither an object nor a class NAME. php's`` |
|       - | 2423 | `			 * catchable Error, which STOPS the statement; PH7 raised its own` |
|       - | 2424 | `` 			 * uncatchable wording and carried on with NULL, so a `$obj::$s = 1` `` |
|       - | 2425 | `			 * through a null base went on to fail again in OP_STORE. */` |
|       - | 2426 | `			sxi32 rcCn;` |
|       5 | 2427 | `			if( !pInstr->p3 ){` |
|       3 | 2428 | `				VmPopOperand(&pTos,1);` |
|       1 | 2429 | `			}` |
|       5 | 2430 | `			PH7_MemObjRelease(pTos);` |
|       5 | 2431 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 2432 | `			pTos->nIdx = SXU32_HIGH;` |
|       5 | 2433 | `			rcCn = VmThrowFromVm(&(*pVm),"Error","Class name must be a valid object or a string",` |
|       - | 2434 | `				sizeof("Class name must be a valid object or a string")-1);` |
|       5 | 2435 | `			if( rcCn == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 2436 | `			rc = rcCn;` |
|       5 | 2437 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2438 | `		}` |
|       - | 2439 | `	}` |
|  223512 | 2440 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2441 | `	VM_EXIT_BREAK;` |
|  162325 | 2442 | `}` |
|       - | 2443 |  |
|       - | 2444 | `/*` |
|       - | 2445 | ` * OP_CLONE: body moved verbatim from the OP_CLONE arm of` |
|       - | 2446 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2447 | ` */` |
|     240 | 2448 | `PH7_PRIVATE VmOpRc VmExecOpClone(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2449 | `{` |
|     245 | 2450 | `	ph7_value *pTos = pState->pTos;` |
|     245 | 2451 | `	ph7_value *pStack = pState->pStack;` |
|     245 | 2452 | `	VmInstr *aInstr = pState->aInstr;` |
|     245 | 2453 | `	sxi32 pc = pState->pc;` |
|       - | 2454 | `	sxi32 rc;` |
|     120 | 2455 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2456 | `	ph7_class_instance *pSrc,*pClone;` |
|       - | 2457 | `#ifdef UNTRUST` |
|       - | 2458 | `	if( pTos < pStack ){` |
|       - | 2459 | `		VM_EXIT_ABORT;` |
|       - | 2460 | `	}` |
|       - | 2461 | `#endif` |
|       - | 2462 | `	/* Make sure we are dealing with a class instance. PHP 8 throws a catchable` |
|       - | 2463 | ``	 * TypeError for a non-object operand — for both `clone $x` and clone($x). */`` |
|     245 | 2464 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - | 2465 | `		SyBlob sMsg;` |
|       - | 2466 | `		char zGiven[64];` |
|      24 | 2467 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 2468 | `		/* php names the VALUE for a bool ("true given", not "bool given"), which is` |
|       - | 2469 | `		 * what every other argument diagnostic here already says — the shared helper. */` |
|      24 | 2470 | `		SyBlobFormat(&sMsg,"clone(): Argument #1 ($object) must be of type object, %s given",` |
|      11 | 2471 | `			VmValueGivenName(pTos,zGiven,sizeof(zGiven)));` |
|      24 | 2472 | `		rc = VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|      24 | 2473 | `		PH7_MemObjRelease(pTos);` |
|      24 | 2474 | `		pTos->nIdx = SXU32_HIGH;` |
|      24 | 2475 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 2476 | `			VM_EXIT_ABORT;` |
|       - | 2477 | `		}` |
|       - | 2478 | `		{` |
|       - | 2479 | `			sxi32 iRp;` |
|      24 | 2480 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      24 | 2481 | `				pc = iRp;` |
|      24 | 2482 | `				VM_EXIT_BREAK;` |
|       - | 2483 | `			}` |
|       - | 2484 | `		}` |
|     ! 0 | 2485 | `		VM_EXIT_EXCEPTION;` |
|       - | 2486 | `	}` |
|       - | 2487 | `	/* Point to the source */` |
|     223 | 2488 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|       - | 2489 | `	/* Enum cases are not cloneable — php's catchable Error (the singleton` |
|       - | 2490 | `	 * identity would break) — and neither is a class whose instances own a` |
|       - | 2491 | `	 * C-side resource a slot-by-slot copy would double-free (PH7_CLASS_NOCLONE),` |
|       - | 2492 | `	 * nor a Generator or a Fiber (their suspended C state is not copyable). php` |
|       - | 2493 | `	 * words all of them the same way and throws the same catchable Error; the` |
|       - | 2494 | `	 * Generator/Fiber pair used to take an older warn-only path here, so` |
|       - | 2495 | ``	 * `clone $gen` PRINTED an uncatchable diagnostic and then carried on with the`` |
|       - | 2496 | ``	 * ORIGINAL object standing in for the copy — the one shape of `clone` that`` |
|       - | 2497 | `	 * answered a value php never lets the program reach. */` |
|     218 | 2498 | `	if( (pSrc->pClass->iFlags & (PH7_CLASS_ENUM\|PH7_CLASS_NOCLONE))` |
|     186 | 2499 | `		\|\| pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|       - | 2500 | `		SyBlob sMsg;` |
|      80 | 2501 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      80 | 2502 | `		SyBlobFormat(&sMsg,"Trying to clone an uncloneable object of class %z",` |
|      78 | 2503 | `			&pSrc->pClass->sName);` |
|      80 | 2504 | `		rc = VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|      80 | 2505 | `		PH7_MemObjRelease(pTos);` |
|      80 | 2506 | `		pTos->nIdx = SXU32_HIGH;` |
|      80 | 2507 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 2508 | `			VM_EXIT_ABORT;` |
|       - | 2509 | `		}` |
|       - | 2510 | `		{` |
|       - | 2511 | `			sxi32 iRp;` |
|      80 | 2512 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      62 | 2513 | `				pc = iRp;` |
|      62 | 2514 | `				VM_EXIT_BREAK;` |
|       - | 2515 | `			}` |
|       - | 2516 | `		}` |
|      19 | 2517 | `		VM_EXIT_EXCEPTION;` |
|       - | 2518 | `	}` |
|       - | 2519 | `	/* Perform the clone operation */` |
|     145 | 2520 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|     145 | 2521 | `	PH7_MemObjRelease(pTos);` |
|     145 | 2522 | `	if( pClone == 0 ){` |
|     ! 0 | 2523 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 2524 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|     ! 0 | 2525 | `	}else{` |
|       - | 2526 | `		/* Load the cloned object */` |
|     145 | 2527 | `		pTos->x.pOther = pClone;` |
|     145 | 2528 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - | 2529 | `	}` |
|     145 | 2530 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2531 | `	VM_EXIT_BREAK;` |
|     125 | 2532 | `}` |
|       - | 2533 |  |
