# src/ph7/vm_ops_oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1376/1550 lines (88.77%)

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
| 2113668 |   28 | `PH7_PRIVATE VmOpRc VmExecOpNew(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   29 | `{` |
| 2113673 |   30 | `	ph7_value *pTos = pState->pTos;` |
| 2113673 |   31 | `	ph7_value *pStack = pState->pStack;` |
| 2113673 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
| 2113673 |   33 | `	sxi32 pc = pState->pc;` |
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
| 2113673 |   49 | `	int bScreenOnly = pInstr->iP1 < 0;` |
| 2668316 |   50 | `	sxi32 nCtorArgs = bScreenOnly ? 0` |
| 1611477 |   51 | `		: pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|       - |   52 | `	ph7_value *pArg;` |
| 2113673 |   53 | `	ph7_class *pClass = 0;` |
|       - |   54 | `	ph7_class_instance *pNew;` |
|       - |   55 | `	/* Resolving the class below can run an AUTOLOADER that throws. Snapshot the boundary` |
|       - |   56 | `	 * rail so the "not found" report can tell a missing class from an autoloader that` |
|       - |   57 | `	 * raised — php propagates the autoloader's exception and reports nothing else, while` |
|       - |   58 | `	 * this arm used to throw its own Error on top of it (uncaught, killing the script` |
|       - |   59 | `	 * right after the real exception had been handled). */` |
| 2113673 |   60 | `	sxi32 nNewBrc = pVm->nBoundaryRc;` |
| 2113673 |   61 | `	const void *pNewRes = (const void *)pVm->pResumeFrame;` |
| 2113673 |   62 | `	pArg = &pTos[-nCtorArgs]; /* Constructor arguments (if available) */` |
|       - |   63 | `	/* Same PHP 8.1 spread-key realignment as OP_CALL: build a per-actual-slot name` |
|       - |   64 | `	 * map when the ctor arg list unpacked string-keyed elements. NEW keeps the` |
|       - |   65 | `	 * class-name slot at pTos (above the args), so pArg is already the correct base;` |
|       - |   66 | `	 * the build also truncates this call's captured runs. */` |
|       - |   67 | `	VmCallArgMap sEffNewMap;` |
|       - |   68 | `	/* The screen pass has no arguments and no runs of its own: building (and` |
|       - |   69 | `	 * truncating) an effective map there would speak for the enclosing call. */` |
| 2668316 |   70 | `	VmCallArgMap *pEffNewMap = bScreenOnly ? 0 : VmEffCallArgMap(pVm,pInstr,pArg,` |
| 1109286 |   71 | `		nCtorArgs > 0 ? (sxu32)nCtorArgs : 0,&sEffNewMap);` |
| 3170505 |   72 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
| 2113669 |   73 | `		const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
| 2113669 |   74 | `		sxu32 nCls = SyBlobLength(&pTos->sBlob);` |
| 2113664 |   75 | `		if( (nCls == sizeof("self")-1 && SyMemcmp(zCls,"self",sizeof("self")-1) == 0)` |
| 2113652 |   76 | `		 \|\| (nCls == sizeof("static")-1 && SyMemcmp(zCls,"static",sizeof("static")-1) == 0)` |
| 2113608 |   77 | `		 \|\| (nCls == sizeof("parent")-1 && SyMemcmp(zCls,"parent",sizeof("parent")-1) == 0) ){` |
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
| 2113605 |   95 | `			pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,` |
|       - |   96 | `				TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|       5 |   97 | `		}` |
| 1056837 |   98 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - |   99 | `		/* Take the base class from the loaded instance */` |
|       5 |  100 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       2 |  101 | `	}` |
| 2113673 |  102 | `	if( pClass == 0 ){` |
|       - |  103 | ``		/* php: `new NoSuch()` is a CATCHABLE Error ('Class "NoSuch" not found'), not a`` |
|       - |  104 | `		 * hard stop. Aborting here killed the script outright -- and with exit status 0,` |
|       - |  105 | `		 * so a caller could not even tell it had failed. */` |
|       - |  106 | `		SyBlob sErrM;` |
|       - |  107 | `		sxi32 rcErr;` |
|      44 |  108 | `		ph7_class *pNotNew = 0;` |
|      44 |  109 | `		if( PH7_VmClassLookupRaised(&(*pVm),nNewBrc,pNewRes) ){` |
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
|      40 |  126 | `		SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      40 |  127 | `		if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       - |  128 | `			/* The extract above only accepts NEW-able classes, so an interface, an` |
|       - |  129 | `			 * abstract class or a trait comes back as 0 and used to be reported as` |
|       - |  130 | `			 * "not found". Look again without that filter so php's real message can be` |
|       - |  131 | `			 * given — but through the table DIRECTLY, never PH7_VmExtractClass: that` |
|       - |  132 | `			 * one fires the autoloader when the name is absent, so a genuinely missing` |
|       - |  133 | `			 * class ran every registered autoloader TWICE where php runs them once. */` |
|      40 |  134 | `			const char *zNotNew = (const char *)SyBlobData(&pTos->sBlob);` |
|      40 |  135 | `			sxu32 nNotNew = SyBlobLength(&pTos->sBlob);` |
|       - |  136 | `			SyHashEntry *pNotNewEntry;` |
|      40 |  137 | `			PH7_VmClassNameAnchor(&zNotNew,&nNotNew);` |
|      40 |  138 | `			pNotNewEntry = nNotNew > 0 ? SyHashGet(&pVm->hClass,(const void *)zNotNew,nNotNew) : 0;` |
|      40 |  139 | `			pNotNew = pNotNewEntry ? (ph7_class *)pNotNewEntry->pUserData : 0;` |
|      18 |  140 | `		}` |
|      48 |  141 | `		if( pNotNew && (pNotNew->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) ){` |
|       - |  142 | `			/* php names WHAT it will not instantiate; a trait is one of the three, and` |
|       - |  143 | `			 * PH7's loadable-only extract rejected it as "not found" — the one shape of` |
|       - |  144 | ``			 * `new` whose refusal did not say why. */`` |
|      25 |  145 | `			const char *zKind = (pNotNew->iFlags & PH7_CLASS_INTERFACE) ? "interface"` |
|      14 |  146 | `				: (pNotNew->iFlags & PH7_CLASS_TRAIT) ? "trait" : "abstract class";` |
|      17 |  147 | `			SyBlobFormat(&sErrM,"Cannot instantiate %s %z",zKind,&pNotNew->sName);` |
|       9 |  148 | `		}else{` |
|      24 |  149 | `			SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|      20 |  150 | `				SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob));` |
|       - |  151 | `		}` |
|       - |  152 | `		/* Settle the stack BEFORE throwing: the class-name slot becomes the (NULL)` |
|       - |  153 | `		 * expression result and the ctor arguments go. */` |
|      40 |  154 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  155 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  156 | `		}` |
|      40 |  157 | `		PH7_MemObjRelease(pTos);` |
|      40 |  158 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|      40 |  159 | `		pTos->nIdx = SXU32_HIGH;` |
|      58 |  160 | `		rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      18 |  161 | `			SyBlobLength(&sErrM));` |
|      40 |  162 | `		SyBlobRelease(&sErrM);` |
|      40 |  163 | `		if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      40 |  164 | `		rc = rcErr;` |
|      48 |  165 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
| 2113633 |  166 | `	}else if( pClass->iFlags & PH7_CLASS_NOINSTANTIATE ){` |
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
| 2113627 |  187 | `	}else if( pClass->iFlags & PH7_CLASS_ENUM ){` |
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
| 2113616 |  200 | `	}else if( VmClassStaticDeferPending(pClass)` |
| 1056815 |  201 | `	       && (rc = PH7_VmMaterializeClassStatics(&(*pVm),pClass)) != SXRET_OK ){` |
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
| 2113617 |  222 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       - |  223 | `		/* Constructor visibility (band A #4): __construct now KEEPS its` |
|       - |  224 | `		 * declared protection (PH7_NewClassMethod no longer forces it` |
|       - |  225 | ``		 * public), so `new C()` on a private/protected constructor from`` |
|       - |  226 | `		 * the wrong scope is php's catchable Error. Reflection's` |
|       - |  227 | `		 * newInstance path sets bReflectBypass like method invoke. */` |
| 2113617 |  228 | `		if( pCons && pCons->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
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
| 2113593 |  265 | `		if( bScreenOnly ){` |
|       - |  266 | `			/* Every refusal above has been asked. Leave the class name standing for` |
|       - |  267 | `			 * the real pass and let the arguments run. */` |
| 1004363 |  268 | `			VM_EXIT_BREAK;` |
|       - |  269 | `		}` |
| 1109235 |  270 | `		if( nCtorArgs > 0 ){` |
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
| 1004377 |  281 | `			ph7_class_method *pCtorArgs = pCons;` |
|       - |  282 | `			sxi32 rcDA;` |
| 1004377 |  283 | `			if( pCtorArgs == 0 ){` |
|      19 |  284 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pEffNewMap);` |
| 1004368 |  285 | `			}else if( pCtorArgs->sFunc.iFlags & VM_FUNC_NATIVE ){` |
|       - |  286 | `				/* A native constructor has no compiled formals; its by-ref positions` |
|       - |  287 | `				 * come from the signature-derived mask, the builtin way. */` |
| 1003895 |  288 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
| 1003890 |  289 | `					pCtorArgs->sFunc.pNative ? pCtorArgs->sFunc.pNative->nByRefMask : 0,0,0,pEffNewMap);` |
|  501950 |  290 | `			}else{` |
|     701 |  291 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|     464 |  292 | `					(ph7_vm_func_arg *)SySetBasePtr(&pCtorArgs->sFunc.aArgs),` |
|     232 |  293 | `					SySetUsed(&pCtorArgs->sFunc.aArgs),0,0,0,pEffNewMap);` |
|       - |  294 | `			}` |
| 1004377 |  295 | `			if( rcDA == PH7_ABORT \|\| rcDA == PH7_EXCEPTION ){` |
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
|  502185 |  312 | `		}` |
|       - |  313 | `		/* Create a new class instance */` |
| 1109233 |  314 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
| 1109233 |  315 | `		if( pNew == 0 ){` |
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
| 1109233 |  327 | `		if( pVm->nBoundaryRc != 0 ){` |
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
| 1109199 |  355 | `		if( pCons ){` |
|       - |  356 | `			/* Call the class constructor.  Collect args in stack order and` |
|       - |  357 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|       - |  358 | `			 * receiving OP_CALL path runs its named-argument matching` |
|       - |  359 | `			 * (including variadic string-key packing). */` |
| 1104755 |  360 | `			VmCallArgMap *pNewMap = pEffNewMap;` |
|       - |  361 | `			sxi32 rcCons;` |
|       - |  362 | `			SySet aArg; /* ctor argument scratch (the loop's shared set stays with OP_CALL) */` |
| 1104755 |  363 | `			SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
| 2110141 |  364 | `			while( pArg < pTos ){` |
| 1005391 |  365 | `				SySetPut(&aArg,(const void *)&pArg);` |
| 1005391 |  366 | `				pArg++;` |
|       5 |  367 | `			}` |
|       - |  368 | `			/* Too-few-arguments is php's catchable ArgumentCountError, raised` |
|       - |  369 | `			 * by the shared OP_CALL install path this ctor call routes through` |
|       - |  370 | `			 * (was a PHL-only notice here). */` |
| 1104755 |  371 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
| 1104755 |  372 | `			SySetRelease(&aArg); /* scratch dies here, on every exit path */` |
|       - |  373 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
| 1104755 |  374 | `			if( pNew->iRef < 1 ){` |
|     ! 0 |  375 | `				pNew->iRef = 1;` |
|     ! 0 |  376 | `			}` |
| 1104755 |  377 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|       - |  378 | `				/* The constructor raised: the half-constructed object must not` |
|       - |  379 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|       - |  380 | `				 * The class-name operand (and any leftover args) are released by` |
|       - |  381 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|       - |  382 | `				sxi32 iResumePc;` |
|  100243 |  383 | `				PH7_ClassInstanceUnref(pNew);` |
|  100243 |  384 | `				if( rcCons == PH7_ABORT ){` |
|       5 |  385 | `					VM_EXIT_ABORT;` |
|       - |  386 | `				}` |
|  100239 |  387 | `				if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){` |
|       - |  388 | `					/* This frame's own try caught it in-place: tidy the stack` |
|       - |  389 | `					 * (pop ctor args + release the class-name slot, then drain any` |
|       - |  390 | `					 * abandoned outer-expression operands to the try's base — the` |
|       - |  391 | `					 * class-name slot itself sits above it) and resume. */` |
|  100135 |  392 | `					if( nCtorArgs > 0 ){` |
|     125 |  393 | `						VmPopOperand(&pTos,nCtorArgs);` |
|      61 |  394 | `					}` |
|  100135 |  395 | `					PH7_MemObjRelease(pTos);` |
|  500265 |  396 | `					PH7_RESUME_DRAIN()` |
|  100135 |  397 | `					pc = iResumePc;` |
|  100135 |  398 | `					VM_EXIT_BREAK;` |
|       - |  399 | `				}` |
|     106 |  400 | `				VM_EXIT_EXCEPTION;` |
|       - |  401 | `			}` |
|  502256 |  402 | `		}` |
| 1008961 |  403 | `		if( nCtorArgs > 0 ){` |
|       - |  404 | `			/* Pop given arguments */` |
| 1004151 |  405 | `			VmPopOperand(&pTos,nCtorArgs);` |
|  502073 |  406 | `		}` |
| 1008961 |  407 | `		PH7_MemObjRelease(pTos);` |
| 1008961 |  408 | `		pTos->x.pOther = pNew;` |
| 1008961 |  409 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - |  410 | `	}` |
| 1008961 |  411 | `	VM_EXIT_BREAK;` |
|     ! 0 |  412 | `	VM_EXIT_BREAK;` |
| 1056839 |  413 | `}` |
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
|     239 |  435 | `		if( pNext->iOp == PH7_OP_STORE_REF ){` |
|       5 |  436 | `			return 1;` |
|       - |  437 | `		}` |
|     235 |  438 | `		if( pNext->iOp == PH7_OP_FOREACH_INIT && pNext->p3 ){` |
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
|  325949 |  563 | `PH7_PRIVATE VmOpRc VmExecOpMember(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  564 | `{` |
|  325954 |  565 | `	ph7_value *pTos = pState->pTos;` |
|  325954 |  566 | `	ph7_value *pStack = pState->pStack;` |
|  325954 |  567 | `	VmInstr *aInstr = pState->aInstr;` |
|  325954 |  568 | `	sxi32 pc = pState->pc;` |
|       - |  569 | `	sxi32 rc;` |
|       - |  570 | `	ph7_class_instance *pThis;` |
|       - |  571 | `	ph7_value *pNos;` |
|       - |  572 | `	SyString sName;` |
|  325954 |  573 | `	if( !pInstr->iP1 ){` |
|  222960 |  574 | `		pNos = &pTos[-1];` |
|       - |  575 | `#ifdef UNTRUST` |
|       - |  576 | `		if( pNos < pStack ){` |
|       - |  577 | `			VM_EXIT_ABORT;` |
|       - |  578 | `		}` |
|       - |  579 | `#endif` |
|  222955 |  580 | `		if( pInstr->iP2 == PH7_MEMBER_DEFPATH` |
|  111922 |  581 | `		 && (pNos->iFlags & (MEMOBJ_AUX_DEFPATH\|MEMOBJ_AUX_DEFERRED)) ){` |
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
|  222934 |  607 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - |  608 | `			ph7_class *pClass;` |
|       - |  609 | `			/* Class already instantiated */` |
|  222832 |  610 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       - |  611 | `			/* Point to the instantiated class */` |
|  222832 |  612 | `			pClass = pThis->pClass;` |
|       - |  613 | `			/* Extract attribute name first */` |
|  222832 |  614 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|  222832 |  615 | `			if( PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
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
|  222796 |  688 | `			if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - |  689 | `				/* Method call */` |
|  116320 |  690 | `				ph7_class_method *pMeth = 0;` |
|  116320 |  691 | `				if( sName.nByte > 0 ){` |
|       - |  692 | `					/* Extract the target method */` |
|  116320 |  693 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|   58159 |  694 | `				}` |
|  116320 |  695 | `				if( pMeth == 0 ){` |
|      64 |  696 | `					ph7_class_method *pCallMagic = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|      64 |  697 | `					if( pCallMagic ){` |
|       - |  698 | `						/* php: a missing method dispatches __call($name, $args) — the` |
|       - |  699 | `						 * args are only collected by the following OP_CALL, so stash the` |
|       - |  700 | `						 * receiver + class + original name and MARK the callee slot` |
|       - |  701 | `						 * (band A #3b; pre-fix the name-only call discarded everything` |
|       - |  702 | `						 * and the call site failed with "Invalid function name"). Stack:` |
|       - |  703 | `						 * pop the method name, then the receiver slot becomes the marked` |
|       - |  704 | `						 * carrier OP_CALL routes through the packing body. */` |
|      58 |  705 | `						VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pThis,pClass,&sName);` |
|      58 |  706 | `						if( pPend == 0 ){` |
|     ! 0 |  707 | `							VM_EXIT_ABORT;` |
|       - |  708 | `						}` |
|      58 |  709 | `						VmPopOperand(&pTos,1);` |
|      58 |  710 | `						PH7_MemObjRelease(pTos);` |
|      58 |  711 | `						pTos->x.pOther = pPend;` |
|      58 |  712 | `						pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|      31 |  713 | `					}else{` |
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
|      31 |  732 | `				}else{` |
|  116260 |  733 | `					ph7_class_method *pDeniedCall = 0;` |
|  116260 |  734 | `					int bDenied = 0;` |
|       - |  735 | `					/* php decides a method's visibility against the class that OWNS it,` |
|       - |  736 | `					 * which for a trait method is the class that composed it — a trait is` |
|       - |  737 | `					 * a compile-time construct php has flattened away by now. Deciding` |
|       - |  738 | `					 * against the trait instead made every rule a question of who USES it:` |
|       - |  739 | `					 * a protected trait method was denied to a SUBCLASS of the composing` |
|       - |  740 | `					 * class (not a trait user itself) and granted to an unrelated class` |
|       - |  741 | `					 * that happened to use the same trait. */` |
|  116260 |  742 | `					ph7_class *pOwner = 0;` |
|  116255 |  743 | `					if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC` |
|   58191 |  744 | `					 && !PH7_VmClassMemberAccess(&(*pVm),` |
|     114 |  745 | `						(pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth)),` |
|      57 |  746 | `						&sName,pMeth->iProtection,FALSE) ){` |
|      56 |  747 | `						int bRebound = 0;` |
|      56 |  748 | `						if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - |  749 | `							/* php: when the instance's class SHADOWS the caller's` |
|       - |  750 | `							 * private method (child redeclares private m), code in` |
|       - |  751 | `							 * the caller's class dispatches its OWN private m, not` |
|       - |  752 | `							 * the shadow. Rebind to the calling scope's method when` |
|       - |  753 | `							 * that scope declares a private one of this name. */` |
|      48 |  754 | `							VmFrame *pFrameLocal = VmSkipExceptionFrames(pVm->pFrame);` |
|      48 |  755 | `							ph7_vm_func *pCallerFunc = (ph7_vm_func *)pFrameLocal->pUserData;` |
|      48 |  756 | `							if( pCallerFunc && (pCallerFunc->iFlags & VM_FUNC_CLASS_METHOD) && pCallerFunc->pUserData ){` |
|       6 |  757 | `								ph7_class *pScope = (ph7_class *)pCallerFunc->pUserData;` |
|       6 |  758 | `								ph7_class_method *pOwn = PH7_ClassExtractMethod(pScope,sName.zString,sName.nByte);` |
|       4 |  759 | `								if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       6 |  760 | `								 && (ph7_class *)pOwn->sFunc.pUserData == pScope ){` |
|       6 |  761 | `									pMeth = pOwn;` |
|       6 |  762 | `									bRebound = 1;` |
|       2 |  763 | `								}` |
|       2 |  764 | `							}` |
|      22 |  765 | `						}` |
|      56 |  766 | `						if( !bRebound ){` |
|       - |  767 | `							/* Inaccessible from this scope: php routes through __call when` |
|       - |  768 | `							 * declared (band A #3b); without it the refusal is raised right` |
|       - |  769 | `							 * here, beside the undefined-method and abstract-method twins. */` |
|      52 |  770 | `							pDeniedCall = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|      52 |  771 | `							bDenied = pDeniedCall == 0;` |
|      24 |  772 | `						}` |
|      26 |  773 | `					}` |
|  116260 |  774 | `					if( bDenied ){` |
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
|      42 |  789 | `						ph7_class *pScope = PH7_VmCallerScopeName(&(*pVm));` |
|      61 |  790 | `						const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      19 |  791 | `							? "private" : "protected";` |
|      42 |  792 | `						SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      42 |  793 | `						if( pScope ){` |
|       7 |  794 | `							SyBlobFormat(&sErrM,"Call to %s method %z::%z() from scope %z",` |
|       3 |  795 | `								zVis,&pOwner->sName,&sName,&pScope->sName);` |
|       4 |  796 | `						}else{` |
|      36 |  797 | `							SyBlobFormat(&sErrM,"Call to %s method %z::%z() from global scope",` |
|      16 |  798 | `								zVis,&pOwner->sName,&sName);` |
|       - |  799 | `						}` |
|      42 |  800 | `						VmPopOperand(&pTos,1);` |
|      42 |  801 | `						PH7_MemObjRelease(pTos);` |
|      42 |  802 | `						pTos->nIdx = SXU32_HIGH;` |
|      61 |  803 | `						rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      19 |  804 | `							SyBlobLength(&sErrM));` |
|      42 |  805 | `						SyBlobRelease(&sErrM);` |
|      42 |  806 | `						if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      42 |  807 | `						rc = rcErr;` |
|      52 |  808 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  809 | `					}` |
|  116222 |  810 | `					if( pDeniedCall ){` |
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
|  116212 |  824 | `						PH7_MemObjRelease(pTos);` |
|  116212 |  825 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|  116212 |  826 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|  116212 |  827 | `						pTos->iFlags \|= MEMOBJ_AUX_MEMBERCALL;` |
|       - |  828 | `					}` |
|       - |  829 | `				}` |
|  116276 |  830 | `				pTos->nIdx = SXU32_HIGH;` |
|   58142 |  831 | `			}else{` |
|       - |  832 | `				/* Attribute access. iP2: 0 = read, 2 = unset, 3 = isset, 4 = empty. */` |
|  106481 |  833 | `				VmClassAttr *pObjAttr = 0;` |
|  106481 |  834 | `				SyHashEntry *pEntry = 0;` |
|  106476 |  835 | `				if( sName.nByte > 0 && sName.zString[0] == 0` |
|   53264 |  836 | `				 && !PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
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
|  106449 |  887 | `				if( sName.nByte > 0 ){` |
|  106441 |  888 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|   53223 |  889 | `				}else{` |
|       9 |  890 | `					pEntry = PH7_ClassInstanceAttrEntry(pThis,sName.zString,0);` |
|       - |  891 | `				}` |
|  106449 |  892 | `				if( pEntry ){` |
|       - |  893 | `					/* Point to the attribute value */` |
|  105775 |  894 | `					pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|   52885 |  895 | `				}` |
|  106444 |  896 | `				if( pObjAttr` |
|  106107 |  897 | `				 && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT))` |
|   52900 |  898 | `				 && PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,` |
|      20 |  899 | `					pObjAttr->pAttr->iProtection,FALSE) ){` |
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
|      18 |  910 | `					if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      18 |  911 | `					 && pInstr->iP2 != PH7_MEMBER_DEFPATH ){` |
|       - |  912 | `						/* Silent where php is silent: isset()/empty(), and any class` |
|       - |  913 | `						 * that declares the MAGIC accessor this context would dispatch` |
|       - |  914 | ``						 * (php passes `silent = ce->__get/__set/__unset != NULL` to its`` |
|       - |  915 | `						 * property-offset lookup). Once per access, too: the deferred` |
|       - |  916 | `						 * call-argument PRE-PASS (PH7_MEMBER_DEFPATH) re-runs this op for` |
|       - |  917 | ``						 * the same source `$o->s` and the value pass carries the notice`` |
|       - |  918 | `						 * — the BY-REF form has no value pass and notices at its own` |
|       - |  919 | `						 * site (VmBindPropByRef). */` |
|       - |  920 | `						const char *zMagic;` |
|      10 |  921 | `						if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       3 |  922 | `							zMagic = "__unset";` |
|       8 |  923 | `						}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|       8 |  924 | `						       \|\| VmMemberNextIsWrite(pInstr + 1) ){` |
|       3 |  925 | `							zMagic = "__set";` |
|       2 |  926 | `						}else{` |
|       6 |  927 | `							zMagic = "__get";` |
|       - |  928 | `						}` |
|      10 |  929 | `						if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) == 0 ){` |
|      11 |  930 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - |  931 | `								"Accessing static property %z::$%z as non static",` |
|       6 |  932 | `								&pClass->sName,&pObjAttr->pAttr->sName);` |
|       3 |  933 | `						}` |
|       4 |  934 | `					}` |
|      20 |  935 | `					pEntry = 0;` |
|      20 |  936 | `					pObjAttr = 0;` |
|       9 |  937 | `				}` |
|  106449 |  938 | `				if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - |  939 | `					/* unset($o->prop): remove the property entirely so it disappears from` |
|       - |  940 | `					 * foreach / json_encode / get_object_vars / (array) — matching PHP (a value-only` |
|       - |  941 | `					 * release would leave a zombie null entry). Leave a NULL constant on the stack so` |
|       - |  942 | `					 * the trailing generic unset() builtin is a no-op (mirrors LOAD_IDX iP2=5).` |
|       - |  943 | `					 * php dispatches __unset($name) for a MISSING or INACCESSIBLE property` |
|       - |  944 | `					 * (band A #3b — pre-fix an inaccessible private was silently DELETED from` |
|       - |  945 | `					 * outside the class); without __unset, an inaccessible unset is php's` |
|       - |  946 | `					 * catchable "Cannot access ..." Error and a missing one stays a no-op. */` |
|      59 |  947 | `					int bUnsAccessible = pEntry ? PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) : 0;` |
|      61 |  948 | `					if( pEntry && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) != 0 ){` |
|       - |  949 | `						/* php 8.4: a hooked property (virtual or backed) can never be` |
|       - |  950 | `						 * unset — catchable Error, even from inside its own hook body` |
|       - |  951 | `						 * (probe-verified). */` |
|       - |  952 | `						SyBlob sErrMsg;` |
|       5 |  953 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       5 |  954 | `						SyBlobFormat(&sErrMsg,"Cannot unset hooked property %z::$%z",` |
|       4 |  955 | `							&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       5 |  956 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      57 |  957 | `					}else if( pEntry && bUnsAccessible ){` |
|      40 |  958 | `						PH7_VmReleaseInstanceAttr(&(*pVm),pObjAttr);` |
|      40 |  959 | `						SyHashDeleteEntry2(pEntry);` |
|      21 |  960 | `					}else{` |
|      16 |  961 | `						ph7_class_method *pUnsetMagic = PH7_ClassExtractMethod(pClass,"__unset",sizeof("__unset")-1);` |
|      16 |  962 | `						if( pUnsetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'u') ){` |
|      14 |  963 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'u');` |
|      14 |  964 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__unset",sizeof("__unset")-1,&sName,0);` |
|      14 |  965 | `							VmMagicGuardPop(pVm);` |
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
|      59 |  978 | `					VmPopOperand(&pTos,1);    /* pop the attribute name */` |
|      59 |  979 | `					PH7_MemObjRelease(pTos);  /* release the object on the stack ($o's stack ref) */` |
|      59 |  980 | `					pTos->nIdx = SXU32_HIGH;  /* NULL constant */` |
|      59 |  981 | `					VM_EXIT_BREAK;` |
|       - |  982 | `				}` |
|  106393 |  983 | `				if( pObjAttr == 0 ){` |
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
|     687 |  998 | `					VmInstr *pNext = pInstr + 1;` |
|     682 |  999 | `					if( pInstr->iP2 == PH7_MEMBER_WRITE \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|     494 | 1000 | `					 \|\| VmMemberNextIsWrite(pNext) ){` |
|     199 | 1001 | `						ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pThis->pClass,sName.zString,sName.nByte);` |
|     199 | 1002 | `						if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      12 | 1003 | `							VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pObjAttr);` |
|       7 | 1004 | `						}else{` |
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
|     189 | 1016 | `							ph7_class_method *pSetMagic = 0;` |
|     189 | 1017 | `							ph7_class_method *pCoalIsset = 0, *pCoalGet = 0, *pCoalSet = 0;` |
|     189 | 1018 | `							int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);` |
|     189 | 1019 | `							if( bPlainStore \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|     125 | 1020 | `								pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|      60 | 1021 | `							}` |
|     189 | 1022 | `							if( pSetMagic && pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 1023 | `								/* php dispatches __set($name, element) for a missing` |
|       - | 1024 | `								 * destructuring target, but the element value only exists` |
|       - | 1025 | `								 * at the following OP_LOAD_LIST — the dispatch is` |
|       - | 1026 | `								 * unsupported (recorded residual). Leave the miss: the` |
|       - | 1027 | `								 * property stays uncreated, matching php's observable` |
|       - | 1028 | `								 * state (its __set did not store either). */` |
|     189 | 1029 | `							}else if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){` |
|      29 | 1030 | `								pThis->iRef++;` |
|      29 | 1031 | `								pVm->pMagicSetThis = pThis;` |
|      29 | 1032 | `								SyBlobReset(&pVm->sMagicSetName);` |
|      29 | 1033 | `								SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       - | 1034 | `								/* pObjAttr stays NULL; the miss path below stays silent. */` |
|     174 | 1035 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|      65 | 1036 | `							 && pNext->iOp == PH7_OP_NULLC_JMP` |
|      44 | 1037 | `							 && ((pCoalIsset = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1)) != 0` |
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
|     152 | 1106 | `							}else if( VmMemberNextIsRmw(pNext)` |
|      99 | 1107 | `							 && VmMagicRmwEligible(pVm,pClass,pThis,&sName) ){` |
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
|     126 | 1128 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|      33 | 1129 | `							 && !VmMemberNextIsWrite(pNext)` |
|      32 | 1130 | `							 && PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) ){` |
|       - | 1131 | `								/* Subscript-write base on a class with __get: php reads` |
|       - | 1132 | `								 * through the magic layer (the write lands on the temp and` |
|       - | 1133 | `								 * is lost, like php's indirect-modification case). A PLAIN` |
|       - | 1134 | `								 * store (bPlainStore — the compiler tags those` |
|       - | 1135 | `								 * PH7_MEMBER_WRITE too) is NOT this case: it falls through` |
|       - | 1136 | `								 * to dynamic creation. Leave the miss path — the read gate` |
|       - | 1137 | `								 * below dispatches __get. */` |
|     128 | 1138 | `							}else if( pThis->pClass->iFlags & PH7_CLASS_READONLY ){` |
|       - | 1139 | `								SyBlob sErrMsg;` |
|     ! 0 | 1140 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     ! 0 | 1141 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|     ! 0 | 1142 | `									&pThis->pClass->sName,&sName);` |
|     ! 0 | 1143 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     120 | 1144 | `							}else if( !VmClassAllowsDynamicProps(pVm,pThis->pClass)` |
|      76 | 1145 | `							       && !VmClassHasAttributeNamed(pThis->pClass,"AllowDynamicProperties",sizeof("AllowDynamicProperties")-1) ){` |
|       - | 1146 | `								/* php 8.2 only DEPRECATES creating a dynamic property on a` |
|       - | 1147 | `								 * class without #[AllowDynamicProperties]; PHL rejects it.` |
|       - | 1148 | `								 * stdClass / __set / declared props are unaffected. */` |
|       - | 1149 | `								SyBlob sErrMsg;` |
|       9 | 1150 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       9 | 1151 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|       6 | 1152 | `									&pThis->pClass->sName,&sName);` |
|       9 | 1153 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       6 | 1154 | `							}else{` |
|     118 | 1155 | `								PH7_VmCreateDynamicAttr(&(*pVm),pThis,sName.zString,sName.nByte,&pObjAttr);` |
|       - | 1156 | `							}` |
|       - | 1157 | `						}` |
|     167 | 1158 | `						if( pObjAttr && VmMemberNextIsRmw(pNext) ){` |
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
|      81 | 1169 | `					}` |
|     325 | 1170 | `				}` |
|  106356 | 1171 | `				if( pObjAttr == 0 && pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH` |
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
|  106351 | 1197 | `				if( pObjAttr == 0 ){` |
|       - | 1198 | `					/* Missing property. On a plain READ, php dispatches __get($name) and the` |
|       - | 1199 | `					 * expression takes its RETURN VALUE (band A #3a — pre-fix the result was` |
|       - | 1200 | `					 * discarded and a PHL-native warn fired even when __get existed). isset/` |
|       - | 1201 | `					 * empty context consults __isset first (then __get for empty()'s value` |
|       - | 1202 | `					 * test); a plain-store write context parked a pending __set above. A` |
|       - | 1203 | `					 * self-recursive read of the same property falls back to the` |
|       - | 1204 | `					 * undefined-property path via the guard, like php's property guard. */` |
|     521 | 1205 | `					ph7_class_method *pGetMagic = 0;` |
|     521 | 1206 | `					if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|     102 | 1207 | `						ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|     102 | 1208 | `						if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
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
|      25 | 1253 | `					}` |
|     468 | 1254 | `					if( (!VmMemberCtxIsLookup(pInstr->iP2) \|\| pInstr->iP2 == PH7_MEMBER_COALESCE)` |
|     446 | 1255 | `					 && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|     451 | 1256 | `					 && !VmMemberNextIsWrite(pInstr + 1) ){` |
|       - | 1257 | `						/* Plain reads AND the ??=/subscript write-base (PH7_MEMBER_WRITE,` |
|       - | 1258 | `						 * which php reads through __get); read-modify-write forms are` |
|       - | 1259 | `						 * excluded (they vivified above — approximate, recorded), and so is` |
|       - | 1260 | `						 * a destructuring target (a pure write — php never reads it).` |
|       - | 1261 | ``						 * `??` reaches here only when the class declares NO __isset — the`` |
|       - | 1262 | `						 * branch above has returned otherwise — so php's gate is absent and` |
|       - | 1263 | `						 * __get answers on its own. */` |
|     397 | 1264 | `						pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|     196 | 1265 | `					}` |
|     473 | 1266 | `					if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
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
|     104 | 1321 | `					if( !VmMemberCtxIsLookup(pInstr->iP2) && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|      56 | 1322 | `					 && pVm->pMagicSetThis == 0` |
|      47 | 1323 | `					 && pVm->nBoundaryRc == 0 ){` |
|       - | 1324 | `						/* A property missing from the INSTANCE may still be DECLARED by the` |
|       - | 1325 | ``						 * class — that is what `unset($o->p)` leaves behind, and php keeps`` |
|       - | 1326 | `						 * answering for the declaration rather than calling the name` |
|       - | 1327 | `						 * undefined: the visibility screen still applies, and a TYPED slot` |
|       - | 1328 | `						 * is back to uninitialized (the same Error a never-written one` |
|       - | 1329 | `						 * raises). Only a name the class does not declare at all is the` |
|       - | 1330 | `						 * "Undefined property" warning. A destructuring target is silent` |
|       - | 1331 | `						 * either way: php created the property above or dispatched __set. */` |
|      39 | 1332 | `						ph7_class_attr *pDeclAttr = PH7_ClassExtractAttribute(pClass,` |
|      12 | 1333 | `							SyStringData(&sName),SyStringLength(&sName));` |
|       - | 1334 | `						/* A static property, a class constant and a native engine slot are` |
|       - | 1335 | `						 * not instance properties; a dynamic one is gone for good once it` |
|       - | 1336 | `						 * is unset, since nothing declares it. */` |
|      24 | 1337 | `						if( pDeclAttr` |
|      22 | 1338 | `						 && (pDeclAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|       - | 1339 | `						                          \|PH7_CLASS_ATTR_HIDDEN\|PH7_CLASS_ATTR_DYNAMIC)) ){` |
|       6 | 1340 | `							pDeclAttr = 0;` |
|       2 | 1341 | `						}` |
|      24 | 1342 | `						if( pDeclAttr` |
|      20 | 1343 | `						 && !PH7_VmClassMemberAccess(&(*pVm),pClass,&pDeclAttr->sName,` |
|       7 | 1344 | `							pDeclAttr->iProtection,FALSE) ){` |
|       - | 1345 | `							SyBlob sErrMsg;` |
|       7 | 1346 | `							const char *zVis = pDeclAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       2 | 1347 | `								? "private" : "protected";` |
|       5 | 1348 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       5 | 1349 | `							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",` |
|       2 | 1350 | `								zVis,&pClass->sName,&sName);` |
|       5 | 1351 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",` |
|       - | 1352 | `								sizeof("Error")-1,&sErrMsg));` |
|      25 | 1353 | `						}else if( pDeclAttr && (pDeclAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|       7 | 1354 | `							VmBoundaryPark(&(*pVm),` |
|       2 | 1355 | `								VmThrowUninitializedPropertyError(&(*pVm),pClass,pDeclAttr));` |
|       3 | 1356 | `						}else{` |
|      27 | 1357 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|       8 | 1358 | `								&pClass->sName,&sName);` |
|       - | 1359 | `						}` |
|      12 | 1360 | `					}` |
|      52 | 1361 | `				}` |
|  105939 | 1362 | `				VmPopOperand(&pTos,1);` |
|       - | 1363 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|       - | 1364 | `				 * This is due to the following case:` |
|       - | 1365 | `				 *     (new TestClass())->foo;` |
|       - | 1366 | `				 */` |
|  105939 | 1367 | `				pThis->iRef++;` |
|  105939 | 1368 | `				PH7_MemObjRelease(pTos);` |
|  105939 | 1369 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|  105939 | 1370 | `				if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - | 1371 | ``					/* `$o->p =& $x`: stash the resolved instance property slot for the`` |
|       - | 1372 | `					 * following member-marked OP_STORE_REF, which rebinds it to alias the` |
|       - | 1373 | `					 * source variable. Do NOT run the read/hook/magic/uninit machinery` |
|       - | 1374 | `					 * below — a reference bind neither reads the value nor triggers` |
|       - | 1375 | `					 * get/set hooks or an uninitialized-typed Error. pThis stays retained` |
|       - | 1376 | `					 * (the iRef++ above); OP_STORE_REF releases it. */` |
|      32 | 1377 | `					if( pObjAttr ){` |
|      32 | 1378 | `						pVm->pRefTargetAttr = pObjAttr;` |
|      32 | 1379 | `						pVm->pRefTargetThis = pThis;` |
|      32 | 1380 | `						pVm->pRefTargetStaticAttr = 0;` |
|      32 | 1381 | `						pTos->nIdx = pObjAttr->nIdx;` |
|      17 | 1382 | `					}else{` |
|       - | 1383 | `						/* Missing/inaccessible target: leave nothing stashed; OP_STORE_REF` |
|       - | 1384 | `						 * no-ops. Balance the retain. */` |
|     ! 0 | 1385 | `						pVm->pRefTargetAttr = 0;` |
|     ! 0 | 1386 | `						pVm->pRefTargetThis = 0;` |
|     ! 0 | 1387 | `						pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 1388 | `						PH7_ClassInstanceUnref(pThis);` |
|       - | 1389 | `					}` |
|      32 | 1390 | `					VM_EXIT_BREAK;` |
|       - | 1391 | `				}` |
|  105909 | 1392 | `				if( pObjAttr ){` |
|  105805 | 1393 | `					ph7_value *pValue = 0; /* cc warning */` |
|       - | 1394 | `					/* Check attribute access */` |
|  105805 | 1395 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|  105770 | 1396 | `						if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET))` |
|   53067 | 1397 | `						 && !VmHookGuardHeld(pVm,(void *)pThis,&sName) ){` |
|       - | 1398 | `							/* PHP 8.4 property hooks: route reads, writes, and the` |
|       - | 1399 | `							 * read-modify-write forms through the synthesized hook` |
|       - | 1400 | `							 * methods. A held guard (either kind) means we are INSIDE` |
|       - | 1401 | `							 * one of this property's own hook bodies — fall through to` |
|       - | 1402 | `` 							 * the raw backing slot for BOTH directions (php: `$this->x` `` |
|       - | 1403 | `							 * within any of x's hooks addresses the backing store). */` |
|     221 | 1404 | `							VmInstr *pNextH = pInstr + 1;` |
|     221 | 1405 | `							int bPlainStore = (pNextH->iOp == PH7_OP_STORE && pNextH->iP2 != 0);` |
|     377 | 1406 | `							int bCoalesceW = pInstr->iP2 == PH7_MEMBER_WRITE` |
|     216 | 1407 | `								&& pNextH->iOp == PH7_OP_NULLC_JMP;` |
|       - | 1408 | `							/* ++/--/compound-assign: the modify op FOLLOWS the member` |
|       - | 1409 | `							 * directly (the compiler tags compound-assign members` |
|       - | 1410 | `							 * PH7_MEMBER_WRITE too, so classify by the next op, not iP2;` |
|       - | 1411 | `							 * the !bPlainStore term is load-bearing — VmMemberNextIsWrite` |
|       - | 1412 | `							 * returns 1 for a member OP_STORE too) */` |
|     221 | 1413 | `							int bRmwNext = !bPlainStore && VmMemberNextIsWrite(pNextH);` |
|     258 | 1414 | `							int bSubscriptW = !bPlainStore && !bCoalesceW && !bRmwNext` |
|     298 | 1415 | `								&& pInstr->iP2 == PH7_MEMBER_WRITE;` |
|     221 | 1416 | `							if( bPlainStore ){` |
|       - | 1417 | `								/* Arm the pending hook-set for the following OP_STORE — it` |
|       - | 1418 | `								 * dispatches the set hook, or throws the read-only Error` |
|       - | 1419 | `								 * when the property only has a get hook. (The scalar` |
|       - | 1420 | `								 * transient is safe here: its window is exactly one` |
|       - | 1421 | `								 * instruction, MEMBER -> STORE, nothing runs in between.) */` |
|      56 | 1422 | `								pThis->iRef++;` |
|      56 | 1423 | `								pVm->pHookSetThis = pThis;` |
|      56 | 1424 | `								pVm->pHookSetAttr = pObjAttr->pAttr;` |
|      56 | 1425 | `								pVm->nHookSetIdx = pObjAttr->nIdx;` |
|      56 | 1426 | `								pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|      56 | 1427 | `								PH7_ClassInstanceUnref(pThis);` |
|      56 | 1428 | `								VM_EXIT_BREAK;` |
|       - | 1429 | `							}` |
|     168 | 1430 | `							if( bSubscriptW ){` |
|       - | 1431 | `								/* Subscript write base ($o->m[] = v, $o->m[$k] = v):` |
|       - | 1432 | `								 * php's catchable Error, with or without a set hook` |
|       - | 1433 | ``								 * (only a by-ref `&get` hook would allow it — a loud`` |
|       - | 1434 | `								 * compile error in PHL). The op completes benignly with` |
|       - | 1435 | `								 * a null temp; the fetch-point router lands the throw. */` |
|       - | 1436 | `								SyBlob sErrMsg;` |
|       7 | 1437 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       7 | 1438 | `								SyBlobFormat(&sErrMsg,"Indirect modification of %z::$%z is not allowed",` |
|       6 | 1439 | `									&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       7 | 1440 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       7 | 1441 | `								PH7_ClassInstanceUnref(pThis);` |
|       7 | 1442 | `								VM_EXIT_BREAK;` |
|       - | 1443 | `							}` |
|     158 | 1444 | `							if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|      83 | 1445 | `							 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1446 | `								/* VIRTUAL set-only property: there is no backing store to` |
|       - | 1447 | `								 * fall back to, so EVERY read context — plain read,` |
|       - | 1448 | `								 * isset()/empty() (php throws there too, probe-verified),` |
|       - | 1449 | `								 * the ??= test, an RMW read — is php's catchable` |
|       - | 1450 | `								 * write-only Error. */` |
|       - | 1451 | `								SyBlob sErrMsg;` |
|       9 | 1452 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       9 | 1453 | `								SyBlobFormat(&sErrMsg,"Property %z::$%z is write-only",` |
|       8 | 1454 | `									&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       9 | 1455 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       9 | 1456 | `								PH7_ClassInstanceUnref(pThis);` |
|       9 | 1457 | `								VM_EXIT_BREAK;` |
|       - | 1458 | `							}` |
|     154 | 1459 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1460 | ``								/* isset()/empty()/`??` on a hooked property all call the get`` |
|       - | 1461 | `								 * hook (php): isset is "get() !== null", empty tests the` |
|       - | 1462 | ``								 * value, and `??` IS the value. */`` |
|       - | 1463 | `								ph7_value sHookRet;` |
|      16 | 1464 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|      16 | 1465 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|      16 | 1466 | `									if( !VmMemberCtxWantsValue(pInstr->iP2) ){` |
|       - | 1467 | `										/* isset(): the trailing builtin tests NULL-ness of what` |
|       - | 1468 | `										 * it is handed, so "not set" has to BE null. Storing` |
|       - | 1469 | ``										 * bool(FALSE) here made `isset($o->p)` answer TRUE for`` |
|       - | 1470 | `										 * a get hook returning null — the same non-null marker` |
|       - | 1471 | `										 * convention the __isset path uses. */` |
|       8 | 1472 | `										if( (sHookRet.iFlags & MEMOBJ_NULL) == 0 ){` |
|       6 | 1473 | `											pTos->x.iVal = 1;` |
|       6 | 1474 | `											MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       4 | 1475 | `										}else{` |
|       3 | 1476 | `											MemObjSetType(pTos,MEMOBJ_NULL);` |
|       - | 1477 | `										}` |
|       5 | 1478 | `									}else{` |
|       - | 1479 | ``										/* empty() judges the value; `??` IS the value. */`` |
|      10 | 1480 | `										PH7_MemObjStore(&sHookRet,pTos);` |
|       - | 1481 | `									}` |
|      16 | 1482 | `									pTos->nIdx = SXU32_HIGH;` |
|      16 | 1483 | `									PH7_MemObjRelease(&sHookRet);` |
|      16 | 1484 | `									PH7_ClassInstanceUnref(pThis);` |
|      16 | 1485 | `									VM_EXIT_BREAK;` |
|       - | 1486 | `								}` |
|     ! 0 | 1487 | `								PH7_MemObjRelease(&sHookRet);` |
|     ! 0 | 1488 | `							}` |
|     140 | 1489 | `							if( !bCoalesceW && !bRmwNext && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1490 | `								/* Read: dispatch __phl_hook_get_NAME (inheritance-aware) */` |
|       - | 1491 | `								ph7_value sHookRet;` |
|     106 | 1492 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|     106 | 1493 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|      92 | 1494 | `									if( pInstr->iP2 == PH7_MEMBER_DEFPATH ){` |
|       - | 1495 | `										/* A deferred call ARGUMENT of a HOOKED property. The` |
|       - | 1496 | `										 * get hook has answered, as php's does; what a` |
|       - | 1497 | `										 * by-REFERENCE parameter then gets is not a notice` |
|       - | 1498 | `										 * but php's refusal — a hook has no slot to alias` |
|       - | 1499 | ``										 * and `&get` does not exist here — while a by-VALUE`` |
|       - | 1500 | `										 * one simply takes the hook's value. */` |
|      42 | 1501 | `										VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),` |
|      26 | 1502 | `											VM_OVER_HOOK,pThis->pClass,&pObjAttr->pAttr->sName,&sHookRet);` |
|      29 | 1503 | `										if( pPre ){` |
|      29 | 1504 | `											PH7_MemObjRelease(pTos);` |
|      29 | 1505 | `											pTos->x.pOther = pPre;` |
|      29 | 1506 | `											pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      29 | 1507 | `											pTos->nIdx = SXU32_HIGH;` |
|      29 | 1508 | `											PH7_MemObjRelease(&sHookRet);` |
|      29 | 1509 | `											PH7_ClassInstanceUnref(pThis);` |
|      60 | 1510 | `											VM_EXIT_BREAK;` |
|       - | 1511 | `										}` |
|     ! 0 | 1512 | `									}` |
|      65 | 1513 | `									PH7_MemObjStore(&sHookRet,pTos);` |
|      65 | 1514 | `									pTos->nIdx = SXU32_HIGH;` |
|      65 | 1515 | `									PH7_MemObjRelease(&sHookRet);` |
|      65 | 1516 | `									PH7_ClassInstanceUnref(pThis);` |
|      65 | 1517 | `									VM_EXIT_BREAK;` |
|       - | 1518 | `								}` |
|      15 | 1519 | `								PH7_MemObjRelease(&sHookRet);` |
|       7 | 1520 | `							}` |
|      49 | 1521 | `							if( bCoalesceW \|\| bRmwNext ){` |
|       - | 1522 | ``								/* `$o->x ??= v` (bCoalesceW) and the read-modify-write`` |
|       - | 1523 | ``								 * forms `$o->x++`, `$o->x .= v`, … (bRmwNext): php reads`` |
|       - | 1524 | `								 * through the get hook (raw backing when the property is` |
|       - | 1525 | `								 * set-only) and writes through the set hook.` |
|       - | 1526 | `								 *   - ??=: the test value goes on the stack and a` |
|       - | 1527 | `								 *     COAL_HOOK entry is pushed for the OP_NULLC_STORE` |
|       - | 1528 | `								 *     at the jump target; the fetch-point sweep drops it` |
|       - | 1529 | `								 *     when the short-circuit jump skips the assign or a` |
|       - | 1530 | `								 *     throw abandons the RHS (the entry is NOT a scalar` |
|       - | 1531 | `								 *     transient — nested stores/coalesces in the RHS` |
|       - | 1532 | `								 *     stack their own entries above it).` |
|       - | 1533 | `								 *   - RMW: the value goes into a fresh SCRATCH slot the` |
|       - | 1534 | `								 *     modify op mutates in place; the entry makes the` |
|       - | 1535 | `								 *     op's tail dispatch the set side with the computed` |
|       - | 1536 | `								 *     value (PH7_HOOK_RMW_WRITEBACK). */` |
|       - | 1537 | `								ph7_value sCur;` |
|       - | 1538 | `								sxi32 rcCur;` |
|      35 | 1539 | `								PH7_MemObjInit(pVm,&sCur);` |
|      35 | 1540 | `								rcCur = PH7_VmHookGetAttrValue(pThis,pObjAttr,&sCur);` |
|      35 | 1541 | `								if( rcCur == SXERR_NOTFOUND ){` |
|       - | 1542 | `									/* set-only hook (or guard edge): the read side is the` |
|       - | 1543 | `									 * raw backing store (php) */` |
|       3 | 1544 | `									ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|       3 | 1545 | `									if( pBack ){` |
|       3 | 1546 | `										PH7_MemObjStore(pBack,&sCur);` |
|       2 | 1547 | `									}` |
|      34 | 1548 | `								}else if( pVm->nBoundaryRc != 0 ){` |
|       - | 1549 | `									/* the get hook threw: leave the null temp; the` |
|       - | 1550 | `									 * fetch-point router lands the parked throw —` |
|       - | 1551 | `									 * nothing is armed. */` |
|     ! 0 | 1552 | `									PH7_MemObjRelease(&sCur);` |
|     ! 0 | 1553 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1554 | `									VM_EXIT_BREAK;` |
|       - | 1555 | `								}` |
|      35 | 1556 | `								if( bCoalesceW ){` |
|       - | 1557 | `									VmHookRmw sPend;` |
|      15 | 1558 | `									PH7_MemObjStore(&sCur,pTos);` |
|      15 | 1559 | `									pTos->nIdx = SXU32_HIGH;` |
|      15 | 1560 | `									PH7_MemObjRelease(&sCur);` |
|      15 | 1561 | `									sPend.iKind = VM_HOOK_PEND_COAL_HOOK;` |
|      15 | 1562 | `									sPend.pThis = pThis;` |
|      15 | 1563 | `									sPend.pAttr = pObjAttr->pAttr;` |
|      15 | 1564 | `									sPend.nBackIdx = pObjAttr->nIdx;` |
|      15 | 1565 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|      15 | 1566 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|      15 | 1567 | `									sPend.pOwnerStack = (void *)pStack;` |
|      15 | 1568 | `									sPend.pInstrs = (void *)aInstr;` |
|      15 | 1569 | `									sPend.nJmpPc = (sxu32)(pc + 1);            /* the OP_NULLC_JMP */` |
|      15 | 1570 | `									sPend.nPc = (sxu32)(pNextH->iP2 - 1);      /* the OP_NULLC_STORE */` |
|      15 | 1571 | `									pThis->iRef++;` |
|      15 | 1572 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|      15 | 1573 | `									PH7_ClassInstanceUnref(pThis);` |
|      15 | 1574 | `									VM_EXIT_BREAK;` |
|       - | 1575 | `								}` |
|       - | 1576 | `								{` |
|      21 | 1577 | `									ph7_value *pScr = PH7_ReserveMemObj(&(*pVm));` |
|      21 | 1578 | `									if( pScr ){` |
|       - | 1579 | `										VmHookRmw sRmw;` |
|      21 | 1580 | `										PH7_MemObjStore(&sCur,pScr);` |
|      21 | 1581 | `										PH7_MemObjStore(&sCur,pTos);` |
|      21 | 1582 | `										pTos->nIdx = pScr->nIdx;` |
|      21 | 1583 | `										sRmw.iKind = VM_HOOK_PEND_RMW;` |
|      21 | 1584 | `										sRmw.pThis = pThis;` |
|      21 | 1585 | `										sRmw.pAttr = pObjAttr->pAttr;` |
|      21 | 1586 | `										sRmw.nBackIdx = pObjAttr->nIdx;` |
|      21 | 1587 | `										sRmw.nScratchIdx = pScr->nIdx;` |
|      21 | 1588 | `										SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      21 | 1589 | `										sRmw.pOwnerStack = (void *)pStack;` |
|      21 | 1590 | `										sRmw.pInstrs = (void *)aInstr;` |
|      21 | 1591 | `										sRmw.nJmpPc = (sxu32)(pc + 1);  /* the modify op ... */` |
|      21 | 1592 | `										sRmw.nPc = (sxu32)(pc + 1);     /* ... is the whole window */` |
|      21 | 1593 | `										pThis->iRef++;` |
|      21 | 1594 | `										SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      10 | 1595 | `									}` |
|       - | 1596 | `									/* OOM: pScr == 0 — leave the null temp (loud allocator` |
|       - | 1597 | `									 * diagnostics already fired) */` |
|      21 | 1598 | `									PH7_MemObjRelease(&sCur);` |
|      21 | 1599 | `									PH7_ClassInstanceUnref(pThis);` |
|      21 | 1600 | `									VM_EXIT_BREAK;` |
|       - | 1601 | `								}` |
|       - | 1602 | `							}` |
|       7 | 1603 | `						}` |
|       - | 1604 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|       - | 1605 | `						 * We can only raise it on a real read, not when the slot is the` |
|       - | 1606 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|       - | 1607 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|       - | 1608 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|  105568 | 1609 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|   52946 | 1610 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|     319 | 1611 | `							VmInstr *pNext = pInstr + 1;` |
|     319 | 1612 | `							int bIsLhs = 0;` |
|     319 | 1613 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|     295 | 1614 | `								bIsLhs = 1;` |
|     145 | 1615 | `							}` |
|     319 | 1616 | `							if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 1617 | `								/* Destructuring target: the OP_LOAD_LIST store (typed-` |
|       - | 1618 | `								 * enforced) initializes it — never a read (php). */` |
|       7 | 1619 | `								bIsLhs = 1;` |
|       3 | 1620 | `							}` |
|       - | 1621 | ``							/* isset()/empty()/`??` read an uninitialized typed property`` |
|       - | 1622 | `							 * as "not set" — a silent miss, NOT the Error a plain read` |
|       - | 1623 | `							 * raises (php). The compiler tags such an access iP2 =` |
|       - | 1624 | `							 * ISSET/EMPTY; treat it like the assignment-LHS case and fall` |
|       - | 1625 | `							 * through to load the slot's NULL. */` |
|     319 | 1626 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       9 | 1627 | `								bIsLhs = 1;` |
|       4 | 1628 | `							}` |
|     319 | 1629 | `							if( !bIsLhs ){` |
|      14 | 1630 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|      14 | 1631 | `								PH7_ClassInstanceUnref(pThis);` |
|      14 | 1632 | `								if( rcU == PH7_ABORT ){` |
|       3 | 1633 | `									VM_EXIT_ABORT;` |
|       - | 1634 | `								}` |
|       - | 1635 | `								{` |
|       - | 1636 | `									sxi32 iRp;` |
|      11 | 1637 | `									if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      10 | 1638 | `										PH7_RESUME_DRAIN()` |
|       6 | 1639 | `										pc = iRp;` |
|       6 | 1640 | `										VM_EXIT_BREAK;` |
|       - | 1641 | `									}` |
|       - | 1642 | `								}` |
|       5 | 1643 | `								VM_EXIT_EXCEPTION;` |
|       - | 1644 | `							}` |
|     152 | 1645 | `						}` |
|       - | 1646 | `						/* Load attribute */` |
|  105563 | 1647 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|  105563 | 1648 | `						if( pValue ){` |
|  105563 | 1649 | `							if( pThis->iRef < 2 ){` |
|       - | 1650 | `								/* Perform a store operation,rather than a load operation since` |
|       - | 1651 | `								 * the class instance '$this' will be deleted shortly.` |
|       - | 1652 | `								 */` |
|     166 | 1653 | `								PH7_MemObjStore(pValue,pTos);` |
|      84 | 1654 | `							}else{` |
|       - | 1655 | `								/* Simple load */` |
|  105399 | 1656 | `								PH7_MemObjLoad(pValue,pTos);` |
|       - | 1657 | `							}` |
|  105563 | 1658 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|  105563 | 1659 | `								if( pThis->iRef > 1 ){` |
|       - | 1660 | `									/* Load attribute index */` |
|  105399 | 1661 | `									pTos->nIdx = pObjAttr->nIdx;` |
|   52697 | 1662 | `								}` |
|   52779 | 1663 | `							}` |
|   52779 | 1664 | `						}` |
|   52784 | 1665 | `					}else{` |
|       - | 1666 | `						/* Inaccessible (private/protected) from this scope. php consults` |
|       - | 1667 | `						 * __get here exactly like a missing property (band A #3a) before` |
|       - | 1668 | `						 * erroring; the guard keeps a self-recursive read from looping. */` |
|      33 | 1669 | `						ph7_class_method *pGetMagic = 0;` |
|       - | 1670 | ``						/* A WRITE-context fetch (the base of `$o->p[k] = v`) reads through`` |
|       - | 1671 | `						 * __get in php too — the write then lands on the value it answered` |
|       - | 1672 | `						 * and is lost, with php's indirect-modification notice. PHL refused` |
|       - | 1673 | ``						 * the whole statement with `Cannot access private property` instead,`` |
|       - | 1674 | ``						 * a fatal on a program php runs. A plain store and a `??=` keep`` |
|       - | 1675 | `						 * their own paths below. */` |
|      30 | 1676 | `						if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      36 | 1677 | `						 && (VmMemberFetchForWrite(pInstr)` |
|      20 | 1678 | `						  \|\| (pInstr->iP2 != PH7_MEMBER_WRITE && !VmMemberNextIsWrite(pInstr + 1))) ){` |
|      21 | 1679 | `							pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       9 | 1680 | `						}` |
|       - | 1681 | `						/* ++/--/compound-assign on a DECLARED but inaccessible property:` |
|       - | 1682 | `						 * php's accessors answer for it exactly as they do for a missing` |
|       - | 1683 | `						 * one (__get, modify, __set), where PHL raised` |
|       - | 1684 | ``						 * `Cannot access private property C::$n`. A plain store keeps its`` |
|       - | 1685 | `						 * own __set park below — it is a write, but not a read-modify-write. */` |
|      30 | 1686 | `						if( VmMemberNextIsRmw(pInstr + 1)` |
|      19 | 1687 | `						 && VmMagicRmwEligible(pVm,pClass,pThis,&sName) ){` |
|       - | 1688 | `							/* The name was already popped and pTos released above, so` |
|       - | 1689 | `							 * sName is the instruction's own literal here. */` |
|       - | 1690 | `							ph7_value sRmwVal;` |
|       - | 1691 | `							sxu32 nRmwScratch;` |
|       3 | 1692 | `							PH7_MemObjInit(pVm,&sRmwVal);` |
|       4 | 1693 | `							VmMagicRmwArm(&(*pVm),pThis,pClass,&sName,&sRmwVal,&nRmwScratch,` |
|       2 | 1694 | `								(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|       3 | 1695 | `							PH7_MemObjStore(&sRmwVal,pTos);` |
|       3 | 1696 | `							pTos->nIdx = nRmwScratch;` |
|       3 | 1697 | `							PH7_MemObjRelease(&sRmwVal);` |
|       3 | 1698 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 1699 | `							VM_EXIT_BREAK;` |
|       - | 1700 | `						}` |
|      31 | 1701 | `						if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 1702 | `							ph7_value sMagicRet;` |
|       9 | 1703 | `							PH7_MemObjInit(pVm,&sMagicRet);` |
|       9 | 1704 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       9 | 1705 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|       9 | 1706 | `							VmMagicGuardPop(pVm);` |
|       9 | 1707 | `							if( VmMemberFetchForWrite(pInstr) ){` |
|       5 | 1708 | `								PH7_VmOverloadedPropNotice(&(*pVm),pClass,&sName,&sMagicRet);` |
|       2 | 1709 | `							}` |
|       - | 1710 | `							/* The name was already popped and pTos released above; just` |
|       - | 1711 | `							 * take the magic result as the expression value. */` |
|       9 | 1712 | `							PH7_MemObjStore(&sMagicRet,pTos);` |
|       9 | 1713 | `							pTos->nIdx = SXU32_HIGH;` |
|       9 | 1714 | `							PH7_MemObjRelease(&sMagicRet);` |
|       9 | 1715 | `							PH7_ClassInstanceUnref(pThis);` |
|       9 | 1716 | `							VM_EXIT_BREAK;` |
|       - | 1717 | `						}` |
|      23 | 1718 | `						if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1719 | `							/* isset/empty on an inaccessible property: php consults __isset` |
|       - | 1720 | `							 * (band A #3b), and is silently false without it — never an` |
|       - | 1721 | `							 * Error (pre-fix PHL fataled here). */` |
|       9 | 1722 | `							ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|       9 | 1723 | `							int bSet = 0;` |
|       9 | 1724 | `							if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 1725 | `								ph7_value sIssetRet;` |
|     ! 0 | 1726 | `								PH7_MemObjInit(pVm,&sIssetRet);` |
|     ! 0 | 1727 | `								VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|     ! 0 | 1728 | `								PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|     ! 0 | 1729 | `								VmMagicGuardPop(pVm);` |
|     ! 0 | 1730 | `								PH7_MemObjToBool(&sIssetRet);` |
|     ! 0 | 1731 | `								bSet = sIssetRet.x.iVal != 0;` |
|     ! 0 | 1732 | `								PH7_MemObjRelease(&sIssetRet);` |
|     ! 0 | 1733 | `							}` |
|       9 | 1734 | `							if( pInstr->iP2 == PH7_MEMBER_COALESCE && pIssetMagic == 0 ){` |
|       - | 1735 | ``								/* `??` with no __isset to gate it: php reads straight through`` |
|       - | 1736 | `								 * __get, exactly as it does for a MISSING property. The` |
|       - | 1737 | `								 * __get dispatch just above this block is gated on a` |
|       - | 1738 | `								 * non-lookup context, so answer here. */` |
|       5 | 1739 | `								ph7_class_method *pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       5 | 1740 | `								if( pCoalGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 1741 | `									ph7_value sCoalRet;` |
|     ! 0 | 1742 | `									PH7_MemObjInit(pVm,&sCoalRet);` |
|     ! 0 | 1743 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     ! 0 | 1744 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sCoalRet);` |
|     ! 0 | 1745 | `									VmMagicGuardPop(pVm);` |
|     ! 0 | 1746 | `									PH7_MemObjStore(&sCoalRet,pTos);` |
|     ! 0 | 1747 | `									pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1748 | `									PH7_MemObjRelease(&sCoalRet);` |
|     ! 0 | 1749 | `								}` |
|       5 | 1750 | `								PH7_ClassInstanceUnref(pThis);` |
|       5 | 1751 | `								VM_EXIT_BREAK;` |
|       - | 1752 | `							}` |
|       5 | 1753 | `							if( bSet ){` |
|     ! 0 | 1754 | `								if( VmMemberCtxWantsValue(pInstr->iP2) ){` |
|     ! 0 | 1755 | `									ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       - | 1756 | `									ph7_value sEmptyVal;` |
|     ! 0 | 1757 | `									PH7_MemObjInit(pVm,&sEmptyVal);` |
|     ! 0 | 1758 | `									if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|     ! 0 | 1759 | `										VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     ! 0 | 1760 | `										PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|     ! 0 | 1761 | `										VmMagicGuardPop(pVm);` |
|     ! 0 | 1762 | `									}` |
|     ! 0 | 1763 | `									PH7_MemObjStore(&sEmptyVal,pTos);` |
|     ! 0 | 1764 | `									pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 1765 | `									PH7_MemObjRelease(&sEmptyVal);` |
|     ! 0 | 1766 | `								}else{` |
|     ! 0 | 1767 | `									pTos->x.iVal = 1;` |
|     ! 0 | 1768 | `									MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     ! 0 | 1769 | `									pTos->nIdx = SXU32_HIGH;` |
|       - | 1770 | `								}` |
|     ! 0 | 1771 | `							}` |
|       5 | 1772 | `							PH7_ClassInstanceUnref(pThis);` |
|       5 | 1773 | `							VM_EXIT_BREAK;` |
|       - | 1774 | `						}` |
|       - | 1775 | `						{` |
|       - | 1776 | `							/* A plain store to an inaccessible property dispatches __set` |
|       - | 1777 | `							 * (band A #3b): park the receiver+name for the following` |
|       - | 1778 | `							 * OP_STORE, exactly like the missing-property case. */` |
|      15 | 1779 | `							VmInstr *pNextW = pInstr + 1;` |
|      15 | 1780 | `							if( pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0 ){` |
|       3 | 1781 | `								ph7_class_method *pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|       3 | 1782 | `								if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){` |
|       3 | 1783 | `									pThis->iRef++;` |
|       3 | 1784 | `									pVm->pMagicSetThis = pThis;` |
|       3 | 1785 | `									SyBlobReset(&pVm->sMagicSetName);` |
|       3 | 1786 | `									SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       3 | 1787 | `									pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|       3 | 1788 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 1789 | `									VM_EXIT_BREAK;` |
|       - | 1790 | `								}` |
|     ! 0 | 1791 | `							}` |
|       - | 1792 | `						}` |
|      10 | 1793 | `						if( ((pInstr + 1)->iOp == PH7_OP_NULLC \|\| (pInstr + 1)->iOp == PH7_OP_NULLC_JMP)` |
|       7 | 1794 | `						 && pInstr->iP2 != PH7_MEMBER_WRITE ){` |
|       - | 1795 | `` 							/* `$o->priv ?? default` (OP_NULLC; NULLC_JMP is the `??=` `` |
|       - | 1796 | ``							 * pre-test): php treats `??` as an isset-style lookup — an`` |
|       - | 1797 | `							 * inaccessible property yields null SILENTLY (the` |
|       - | 1798 | `							 * __isset/__get consults already ran above), letting the` |
|       - | 1799 | `							 * coalesce pick the default. */` |
|     ! 0 | 1800 | `							PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1801 | `							VM_EXIT_BREAK; /* pTos is already the null temp */` |
|       - | 1802 | `						}` |
|       - | 1803 | `						/* A subclass reading a PARENT's PRIVATE property: php treats it as` |
|       - | 1804 | `						 * an UNDEFINED property (the base-private is invisible to the` |
|       - | 1805 | `						 * subclass scope) — a Warning + null, NOT an access Error. Only` |
|       - | 1806 | `						 * this exact shape warns; every other denied read is the catchable` |
|       - | 1807 | `						 * "Cannot access" Error below. */` |
|       - | 1808 | `						{` |
|      12 | 1809 | `						ph7_class *pSelf = VmCurrentSelf(&(*pVm));` |
|      12 | 1810 | `						ph7_class *pDecl = pObjAttr->pAttr->pDeclClass;` |
|      10 | 1811 | `						if( pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      11 | 1812 | `						 && pSelf && pDecl && pSelf != pDecl && PH7_VmInstanceOf(pSelf,pDecl) ){` |
|       3 | 1813 | `							if( !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       4 | 1814 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|       1 | 1815 | `									&pClass->sName,&sName);` |
|       1 | 1816 | `							}` |
|       3 | 1817 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 1818 | `							VM_EXIT_BREAK; /* pTos is already the null temp */` |
|       - | 1819 | `						}` |
|       - | 1820 | `						}` |
|       - | 1821 | `						/* Genuinely inaccessible: php's CATCHABLE Error (parked on the` |
|       - | 1822 | `						 * boundary rail; the fetch-point router lands it — it was an` |
|       - | 1823 | `						 * uncatchable VmReportUncaughtException+Abort before). */` |
|       - | 1824 | `						{` |
|       - | 1825 | `						SyBlob sErrMsg;` |
|      10 | 1826 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      10 | 1827 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      10 | 1828 | `						SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",` |
|       4 | 1829 | `							zVis,&pClass->sName,&sName);` |
|      10 | 1830 | `						PH7_ClassInstanceUnref(pThis);` |
|      10 | 1831 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      10 | 1832 | `						SyBlobRelease(&sErrMsg);` |
|      10 | 1833 | `						VM_EXIT_BREAK;` |
|       - | 1834 | `						}` |
|       - | 1835 | `					}` |
|   52779 | 1836 | `				}` |
|       - | 1837 | `				/* Safely unreference the object */` |
|  105667 | 1838 | `				PH7_ClassInstanceUnref(pThis);` |
|       - | 1839 | `			}` |
|  110973 | 1840 | `		}else{` |
|       - | 1841 | ``			/* `->` on a non-object (e.g. a null intermediate). Silent in isset()/empty()/unset()`` |
|       - | 1842 | ``			 * context (iP2 2/3/4) so `isset($o->missing->x)` / `unset($o->missing->x)` match PHP. */`` |
|     104 | 1843 | `			if( pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1844 | `				/* php draws a sharp line here, and PH7 drew none: CALLING a method on a` |
|       - | 1845 | `				 * non-object is a catchable Error, while READING a property off one is a` |
|       - | 1846 | `				 * Warning that yields NULL. PH7 raised the same notice for both and` |
|       - | 1847 | ``				 * carried on with NULL, so `$null->m()` silently did nothing. */`` |
|       - | 1848 | `				SyString sMemb;` |
|     100 | 1849 | `				const char *zVerb = 0;` |
|     100 | 1850 | `				SyStringInitFromBuf(&sMemb,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     100 | 1851 | `				if( pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|       - | 1852 | ``					/* A deferred call ARGUMENT (`f($u->p)`): which diagnostic php raises`` |
|       - | 1853 | `					 * depends on the parameter, and this op does not know it — a by-VALUE` |
|       - | 1854 | `					 * binding is the read warning below, a by-REFERENCE one is php's` |
|       - | 1855 | ``					 * `Attempt to modify property` Error. Record the step rooted at the`` |
|       - | 1856 | `					 * base and let OP_CALL re-drive it in the mode the callee decides,` |
|       - | 1857 | `					 * exactly as a property MISSING on a real object is recorded. */` |
|      21 | 1858 | `					VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|      21 | 1859 | `					if( pPath && VmDeferPathPushProp(pPath,&sMemb) == SXRET_OK ){` |
|      21 | 1860 | `						VmPopOperand(&pTos,1);   /* drop the property name */` |
|      21 | 1861 | `						PH7_MemObjRelease(pTos); /* collapse the base slot into the carrier */` |
|      21 | 1862 | `						pTos->x.pOther = pPath;` |
|      21 | 1863 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      21 | 1864 | `						pTos->nIdx = SXU32_HIGH;` |
|      44 | 1865 | `						VM_EXIT_BREAK;` |
|       - | 1866 | `					}` |
|     ! 0 | 1867 | `					if( pPath ){` |
|     ! 0 | 1868 | `						VmFreeDeferredPath(pPath);` |
|     ! 0 | 1869 | `					}` |
|       - | 1870 | `					/* fall through to the immediate warning on allocation failure */` |
|     ! 0 | 1871 | `				}` |
|       - | 1872 | `				/* WRITING one is an Error too, and php picks its verb from what the` |
|       - | 1873 | `				 * write actually is. PH7 warned about a READ it never performed and` |
|       - | 1874 | `				 * then let the store fail into its own` |
|       - | 1875 | `				 * "Cannot perform assignment on a constant class attribute", so the` |
|       - | 1876 | `				 * script carried on past a statement php stops it for. The kind is` |
|       - | 1877 | `				 * read off the following instruction, the way every other write` |
|       - | 1878 | `				 * classification in this handler is (VmMemberFetchForWrite). */` |
|      80 | 1879 | `				if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|      31 | 1880 | `					const VmInstr *pNextW = pInstr + 1;` |
|      30 | 1881 | `					if( (pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0)` |
|      21 | 1882 | ``					 \|\| pNextW->iOp == PH7_OP_NULLC_JMP /* `$u->p ??= v` */`` |
|      12 | 1883 | `					 \|\| VmNextIsCompoundAssign(pNextW) ){` |
|      23 | 1884 | `						zVerb = "assign";` |
|      20 | 1885 | `					}else if( pNextW->iOp == PH7_OP_INCR \|\| pNextW->iOp == PH7_OP_DECR ){` |
|       5 | 1886 | `						zVerb = "increment/decrement";` |
|       3 | 1887 | `					}else{` |
|       - | 1888 | ``						/* The base of a subscript write (`$u->p[] = v`): php asks the`` |
|       - | 1889 | `						 * property for something to modify, and there is no property. */` |
|       5 | 1890 | `						zVerb = "modify";` |
|       1 | 1891 | `					}` |
|      65 | 1892 | `				}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       3 | 1893 | `					zVerb = "assign";` |
|      49 | 1894 | `				}else if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       3 | 1895 | `					zVerb = "modify";` |
|       1 | 1896 | `				}` |
|      80 | 1897 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD \|\| zVerb ){` |
|       - | 1898 | `					SyBlob sErrM;` |
|       - | 1899 | `					sxi32 rcErr;` |
|      47 | 1900 | `					SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      47 | 1901 | `					if( zVerb ){` |
|      35 | 1902 | `						SyBlobFormat(&sErrM,"Attempt to %s property \"%z\" on %s",` |
|      17 | 1903 | `							zVerb,&sMemb,VmArithValueName(pNos));` |
|      18 | 1904 | `					}else{` |
|      13 | 1905 | `						SyBlobFormat(&sErrM,"Call to a member function %z() on %s",` |
|       6 | 1906 | `							&sMemb,VmArithValueName(pNos));` |
|       - | 1907 | `					}` |
|      47 | 1908 | `					VmPopOperand(&pTos,1);` |
|      47 | 1909 | `					PH7_MemObjRelease(pTos);` |
|      47 | 1910 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      47 | 1911 | `					pTos->nIdx = SXU32_HIGH;` |
|      70 | 1912 | `					rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      23 | 1913 | `						SyBlobLength(&sErrM));` |
|      47 | 1914 | `					SyBlobRelease(&sErrM);` |
|      47 | 1915 | `					if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      47 | 1916 | `					rc = rcErr;` |
|      49 | 1917 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1918 | `				}` |
|      50 | 1919 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Attempt to read property \"%z\" on %s",` |
|      16 | 1920 | `					&sMemb,VmArithValueName(pNos));` |
|      16 | 1921 | `			}` |
|      38 | 1922 | `			VmPopOperand(&pTos,1);` |
|      38 | 1923 | `			PH7_MemObjRelease(pTos);` |
|      38 | 1924 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|       - | 1925 | `		}` |
|  110991 | 1926 | `	}else{` |
|       - | 1927 | `		/* Static member access using class name */` |
|  102999 | 1928 | `		pNos = pTos;` |
|  102999 | 1929 | `		pThis = 0;` |
|  102999 | 1930 | `		if( !pInstr->p3 ){` |
|    2563 | 1931 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    2563 | 1932 | `			pNos--;` |
|       - | 1933 | `#ifdef UNTRUST` |
|       - | 1934 | `			if( pNos < pStack ){` |
|       - | 1935 | `				VM_EXIT_ABORT;` |
|       - | 1936 | `			}` |
|       - | 1937 | `#endif` |
|    1284 | 1938 | `		}else{` |
|       - | 1939 | `			/* Attribute name already computed */` |
|  100441 | 1940 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|       - | 1941 | `		}` |
|  102999 | 1942 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|  102995 | 1943 | `			ph7_class *pClass = 0;` |
|       - | 1944 | `			/* A FORWARDING static call — self::/parent::/static:: — preserves the` |
|       - | 1945 | `			 * caller's late-static-binding class (php); a non-forwarding C::m() resets` |
|       - | 1946 | `			 * it to C. The receiver slot below keeps the literal keyword, which OP_CALL` |
|       - | 1947 | `			 * can't resolve, so it would fall back to the callee's DECLARING class and` |
|       - | 1948 | `			 * lose LSB. Remember the live LSB class here and stamp it onto the receiver` |
|       - | 1949 | `			 * after the method name is pushed. */` |
|  102995 | 1950 | `			int bForwardingCall = 0;` |
|  102995 | 1951 | `			ph7_class *pForwardLsb = 0;` |
|       - | 1952 | `			/* Same rail snapshot as OP_NEW: the lookup below can run an autoloader that` |
|       - | 1953 | `			 * throws, and php then reports that exception and nothing else. */` |
|  102995 | 1954 | `			sxi32 nMbBrc = pVm->nBoundaryRc;` |
|  102995 | 1955 | `			const void *pMbRes = (const void *)pVm->pResumeFrame;` |
|  102995 | 1956 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - | 1957 | `				/* Class already instantiated */` |
|      24 | 1958 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      24 | 1959 | `				pClass = pThis->pClass;` |
|      24 | 1960 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      13 | 1961 | `			}else{` |
|       - | 1962 | `				/* Try to extract the target class */` |
|  102973 | 1963 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|  102973 | 1964 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|  102973 | 1965 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|       - | 1966 | `					/* Handle self/static/parent keywords */` |
|  102973 | 1967 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       - | 1968 | `						/* In a trait method, self:: resolves to the USING class */` |
|     415 | 1969 | `						pClass = PH7_VmPeekSelfClass(&(*pVm));` |
|     415 | 1970 | `						bForwardingCall = 1;` |
|     415 | 1971 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|  102768 | 1972 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|     110 | 1973 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|     110 | 1974 | `						bForwardingCall = 1;` |
|     110 | 1975 | `						pForwardLsb = pClass;` |
|  102510 | 1976 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|     126 | 1977 | `						pClass = PH7_VmResolveParentClass(&(*pVm));` |
|     126 | 1978 | `						bForwardingCall = 1;` |
|     126 | 1979 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|      65 | 1980 | `					}else{` |
|  102335 | 1981 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|       - | 1982 | `					}` |
|   51484 | 1983 | `				}` |
|       - | 1984 | `			}` |
|  102995 | 1985 | `			if( pClass == 0 ){` |
|       - | 1986 | `				/* Undefined class: php throws a catchable Error */` |
|       - | 1987 | `				SyBlob sErrM;` |
|       - | 1988 | `				sxi32 rcErr;` |
|      15 | 1989 | `				if( PH7_VmClassLookupRaised(&(*pVm),nMbBrc,pMbRes) ){` |
|       - | 1990 | `					/* ...unless an autoloader raised: land THAT instead of reporting the` |
|       - | 1991 | `					 * class missing on top of it. */` |
|      10 | 1992 | `					sxi32 rcAutoM = pVm->nBoundaryRc;` |
|      10 | 1993 | `					pVm->nBoundaryRc = 0;` |
|      10 | 1994 | `					if( !pInstr->p3 ){` |
|       8 | 1995 | `						VmPopOperand(&pTos,1);` |
|       3 | 1996 | `					}` |
|      10 | 1997 | `					PH7_MemObjRelease(pTos);` |
|      10 | 1998 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      10 | 1999 | `					pTos->nIdx = SXU32_HIGH;` |
|      10 | 2000 | `					if( rcAutoM == PH7_ABORT ){` |
|     ! 0 | 2001 | `						VM_EXIT_ABORT;` |
|       - | 2002 | `					}` |
|      10 | 2003 | `					rc = PH7_EXCEPTION;` |
|      10 | 2004 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2005 | `				}` |
|       6 | 2006 | `				SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       6 | 2007 | `				SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|       4 | 2008 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob));` |
|       6 | 2009 | `				if( !pInstr->p3 ){` |
|       6 | 2010 | `					VmPopOperand(&pTos,1);` |
|       2 | 2011 | `				}` |
|       6 | 2012 | `				PH7_MemObjRelease(pTos);` |
|       6 | 2013 | `				pTos->nIdx = SXU32_HIGH;` |
|       8 | 2014 | `				rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       2 | 2015 | `					SyBlobLength(&sErrM));` |
|       6 | 2016 | `				SyBlobRelease(&sErrM);` |
|       6 | 2017 | `				if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       6 | 2018 | `				rc = rcErr;` |
|       6 | 2019 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 2020 | `			}else{` |
|  102983 | 2021 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - | 2022 | `					/* Method call */` |
|    1469 | 2023 | `					ph7_class_method *pMeth = 0;` |
|    1469 | 2024 | `					if( sName.nByte > 0 ){` |
|       - | 2025 | `						/* An INTERFACE's methods are looked up too: they are all abstract,` |
|       - | 2026 | `						 * so the arm below reports php's "Cannot call abstract method` |
|       - | 2027 | `						 * I::m()" rather than claiming the name does not exist. */` |
|    1469 | 2028 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|     732 | 2029 | `					}` |
|    1469 | 2030 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      70 | 2031 | `						if( pMeth ){` |
|       - | 2032 | `							SyBlob sErrM;` |
|       - | 2033 | `							sxi32 rcErr;` |
|       5 | 2034 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       5 | 2035 | `							SyBlobFormat(&sErrM,"Cannot call abstract method %z::%z()",` |
|       2 | 2036 | `								&pClass->sName,&sName);` |
|       5 | 2037 | `							if( !pInstr->p3 ){` |
|       5 | 2038 | `								VmPopOperand(&pTos,1);` |
|       2 | 2039 | `							}` |
|       5 | 2040 | `							PH7_MemObjRelease(pTos);` |
|       5 | 2041 | `							pTos->nIdx = SXU32_HIGH;` |
|       - | 2042 | ``							/* The `$obj::m()` form took a reference on the receiver at the`` |
|       - | 2043 | `							 * top of this branch; every exit has to give it back, and only` |
|       - | 2044 | `							 * the fall-through at the end of the arm used to. */` |
|       5 | 2045 | `							if( pThis ){` |
|     ! 0 | 2046 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2047 | `								pThis = 0;` |
|     ! 0 | 2048 | `							}` |
|       7 | 2049 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       2 | 2050 | `								SyBlobLength(&sErrM));` |
|       5 | 2051 | `							SyBlobRelease(&sErrM);` |
|       5 | 2052 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 2053 | `							rc = rcErr;` |
|       7 | 2054 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 2055 | `						}else{` |
|      66 | 2056 | `							ph7_class_instance *pMagicThis = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|      66 | 2057 | `							ph7_class_method *pCallStaticMagic = pMagicThis` |
|      16 | 2058 | `								? PH7_ClassExtractMethod(pMagicThis->pClass,"__call",sizeof("__call")-1)` |
|      54 | 2059 | `								: PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1);` |
|      66 | 2060 | `							if( pCallStaticMagic ){` |
|       - | 2061 | `								/* php: C::missing(...) dispatches __callStatic($name,$args)` |
|       - | 2062 | `								 * through the packing body (see the instance twin) — or` |
|       - | 2063 | `								 * __call($name,$args) on the calling frame's own $this when` |
|       - | 2064 | `								 * that receiver fits C (VmStaticCallMagicThis). */` |
|      85 | 2065 | `								VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pMagicThis,` |
|      27 | 2066 | `									pMagicThis ? pMagicThis->pClass : pClass,&sName);` |
|      58 | 2067 | `								if( pPend == 0 ){` |
|     ! 0 | 2068 | `									VM_EXIT_ABORT;` |
|       - | 2069 | `								}` |
|      58 | 2070 | `								if( !pInstr->p3 ){` |
|      58 | 2071 | `									VmPopOperand(&pTos,1);` |
|      27 | 2072 | `								}` |
|      58 | 2073 | `								PH7_MemObjRelease(pTos);` |
|      58 | 2074 | `								if( pThis ){` |
|     ! 0 | 2075 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2076 | `									pThis = 0;` |
|     ! 0 | 2077 | `								}` |
|      58 | 2078 | `								pTos->x.pOther = pPend;` |
|      58 | 2079 | `								pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|      58 | 2080 | `								pTos->nIdx = SXU32_HIGH;` |
|      58 | 2081 | `								VM_EXIT_BREAK;` |
|       - | 2082 | `							}` |
|       - | 2083 | `							{` |
|       - | 2084 | `								/* php: the STATIC form reports the same "Call to undefined` |
|       - | 2085 | `								 * method C::m()" as the instance one. */` |
|       - | 2086 | `								SyBlob sErrM;` |
|       - | 2087 | `								sxi32 rcErr;` |
|       9 | 2088 | `								SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       9 | 2089 | `								SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",` |
|       4 | 2090 | `									&pClass->sName,&sName);` |
|       9 | 2091 | `								if( !pInstr->p3 ){` |
|       9 | 2092 | `									VmPopOperand(&pTos,1);` |
|       4 | 2093 | `								}` |
|       9 | 2094 | `								PH7_MemObjRelease(pTos);` |
|       9 | 2095 | `								pTos->nIdx = SXU32_HIGH;` |
|       9 | 2096 | `								if( pThis ){` |
|     ! 0 | 2097 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2098 | `									pThis = 0;` |
|     ! 0 | 2099 | `								}` |
|      13 | 2100 | `								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       4 | 2101 | `									SyBlobLength(&sErrM));` |
|       9 | 2102 | `								SyBlobRelease(&sErrM);` |
|       9 | 2103 | `								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 | 2104 | `								rc = rcErr;` |
|      11 | 2105 | `								PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2106 | `							}` |
|       - | 2107 | `						}` |
|       - | 2108 | `						/* Pop the method name from the stack */` |
|     ! 0 | 2109 | `						if( !pInstr->p3 ){` |
|     ! 0 | 2110 | `							VmPopOperand(&pTos,1);` |
|       - | 2111 | `						}` |
|     ! 0 | 2112 | `						PH7_MemObjRelease(pTos);` |
|     ! 0 | 2113 | `					}else{` |
|       - | 2114 | `						/* Inaccessible from this scope: php routes through the same fallback a` |
|       - | 2115 | ``						 * MISSING name takes — __call on a compatible `$this`, else`` |
|       - | 2116 | ``						 * __callStatic — which is why `C::privateStatic()` runs a catch-all`` |
|       - | 2117 | `						 * instead of the "Call to private method" Error OP_CALL would raise` |
|       - | 2118 | `						 * below. */` |
|    1403 | 2119 | `						ph7_class_method *pDeniedStatic = 0;` |
|    1403 | 2120 | `						ph7_class_instance *pDeniedThis = 0;` |
|    1403 | 2121 | `						int bDeniedStatic = 0;` |
|    1403 | 2122 | `						if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - | 2123 | `							/* The OWNING class decides, as in the instance twin above: a` |
|       - | 2124 | `							 * trait method's rules belong to the class that composed it. */` |
|      29 | 2125 | `							ph7_class *pOwnerCls = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|      29 | 2126 | `							if( !PH7_VmClassMemberAccess(&(*pVm),pOwnerCls,&sName,pMeth->iProtection,FALSE) ){` |
|      15 | 2127 | `								pDeniedThis = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|      15 | 2128 | `								pDeniedStatic = pDeniedThis` |
|       4 | 2129 | `									? PH7_ClassExtractMethod(pDeniedThis->pClass,"__call",` |
|       - | 2130 | `										sizeof("__call")-1)` |
|      12 | 2131 | `									: PH7_ClassExtractMethod(pClass,"__callStatic",` |
|       - | 2132 | `										sizeof("__callStatic")-1);` |
|      15 | 2133 | `								bDeniedStatic = pDeniedStatic == 0;` |
|       7 | 2134 | `							}` |
|      14 | 2135 | `						}` |
|    1403 | 2136 | `						if( bDeniedStatic ){` |
|       - | 2137 | `							/* No catch-all answers for it: php's visibility Error, raised at` |
|       - | 2138 | `							 * the resolution like the instance twin above. OP_CALL's own` |
|       - | 2139 | `							 * screen re-derives the method from the FUNCTION's name against` |
|       - | 2140 | `							 * its declaring class, which cannot see a trait adaptation's` |
|       - | 2141 | ``							 * composed protection — `sHi as private sPriv` ran from global`` |
|       - | 2142 | `							 * scope in silence. */` |
|       - | 2143 | `							SyBlob sErrM;` |
|       - | 2144 | `							sxi32 rcErr;` |
|       7 | 2145 | `							ph7_class *pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|       7 | 2146 | `							ph7_class *pScope = PH7_VmCallerScopeName(&(*pVm));` |
|      10 | 2147 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       3 | 2148 | `								? "private" : "protected";` |
|       7 | 2149 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       7 | 2150 | `							if( pScope ){` |
|       3 | 2151 | `								SyBlobFormat(&sErrM,"Call to %s method %z::%z() from scope %z",` |
|       1 | 2152 | `									zVis,&pOwner->sName,&sName,&pScope->sName);` |
|       2 | 2153 | `							}else{` |
|       5 | 2154 | `								SyBlobFormat(&sErrM,"Call to %s method %z::%z() from global scope",` |
|       2 | 2155 | `									zVis,&pOwner->sName,&sName);` |
|       - | 2156 | `							}` |
|       7 | 2157 | `							if( !pInstr->p3 ){` |
|       7 | 2158 | `								VmPopOperand(&pTos,1);` |
|       3 | 2159 | `							}` |
|       7 | 2160 | `							PH7_MemObjRelease(pTos);` |
|       7 | 2161 | `							pTos->nIdx = SXU32_HIGH;` |
|       7 | 2162 | `							if( pThis ){` |
|     ! 0 | 2163 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2164 | `								pThis = 0;` |
|     ! 0 | 2165 | `							}` |
|      10 | 2166 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       3 | 2167 | `								SyBlobLength(&sErrM));` |
|       7 | 2168 | `							SyBlobRelease(&sErrM);` |
|       7 | 2169 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       7 | 2170 | `							rc = rcErr;` |
|       7 | 2171 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2172 | `						}` |
|    1397 | 2173 | `						if( pDeniedStatic ){` |
|       - | 2174 | `							/* The packing body takes no receiver slot: drop the method-name` |
|       - | 2175 | `							 * slot so the MARK lands on the receiver slot, exactly as the` |
|       - | 2176 | `							 * missing-method twin above does (leaving the class name in place` |
|       - | 2177 | `							 * would pack it as the first $args entry). */` |
|      13 | 2178 | `							VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pDeniedThis,` |
|       4 | 2179 | `								pDeniedThis ? pDeniedThis->pClass : pClass,&sName);` |
|       9 | 2180 | `							if( pPend == 0 ){` |
|     ! 0 | 2181 | `								VM_EXIT_ABORT;` |
|       - | 2182 | `							}` |
|       9 | 2183 | `							if( !pInstr->p3 ){` |
|       9 | 2184 | `								VmPopOperand(&pTos,1);` |
|       4 | 2185 | `							}` |
|       9 | 2186 | `							PH7_MemObjRelease(pTos);` |
|       9 | 2187 | `							if( pThis ){` |
|     ! 0 | 2188 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2189 | `								pThis = 0;` |
|     ! 0 | 2190 | `							}` |
|       9 | 2191 | `							pTos->x.pOther = pPend;` |
|       9 | 2192 | `							pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|       9 | 2193 | `							pTos->nIdx = SXU32_HIGH;` |
|       9 | 2194 | `							VM_EXIT_BREAK;` |
|       - | 2195 | `						}` |
|       - | 2196 | ``						/* php refuses a NON-STATIC method named through `::` unless the`` |
|       - | 2197 | ``						 * CALLING frame holds a `$this` the class accepts: that is what`` |
|       - | 2198 | ``						 * makes `self::`/`parent::`/`static::` and `C::m()` from inside a`` |
|       - | 2199 | `						 * C method work, and every other spelling — from global scope,` |
|       - | 2200 | `` 						 * from a static method, from an unrelated class, and `$obj::m()` `` |
|       - | 2201 | ``						 * — an Error. PHL ran the body instead, with `$this` unset or`` |
|       - | 2202 | ``						 * (for the object form) bound to the object the `::` was written`` |
|       - | 2203 | `						 * on, which php never uses: it takes the CALLER's. The message` |
|       - | 2204 | ``						 * names the DECLARING class, so `D::m()` reports B::m(). */`` |
|    1389 | 2205 | `						if( (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|     130 | 2206 | `							ph7_class_instance *pCallerThis = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|     130 | 2207 | `							if( pCallerThis == 0 ){` |
|       - | 2208 | `								SyBlob sErrM;` |
|       - | 2209 | `								sxi32 rcErr;` |
|      13 | 2210 | `								ph7_class *pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|      13 | 2211 | `								SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      13 | 2212 | `								SyBlobFormat(&sErrM,"Non-static method %z::%z() cannot be called statically",` |
|       6 | 2213 | `									&pOwner->sName,&pMeth->sFunc.sName);` |
|      13 | 2214 | `								if( !pInstr->p3 ){` |
|      13 | 2215 | `									VmPopOperand(&pTos,1);` |
|       6 | 2216 | `								}` |
|      13 | 2217 | `								PH7_MemObjRelease(pTos);` |
|      13 | 2218 | `								pTos->nIdx = SXU32_HIGH;` |
|      13 | 2219 | `								if( pThis ){` |
|       3 | 2220 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 2221 | `									pThis = 0;` |
|       1 | 2222 | `								}` |
|      19 | 2223 | `								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       6 | 2224 | `									SyBlobLength(&sErrM));` |
|      13 | 2225 | `								SyBlobRelease(&sErrM);` |
|      13 | 2226 | `								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      13 | 2227 | `								rc = rcErr;` |
|      13 | 2228 | `								PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2229 | `							}` |
|       - | 2230 | `` 							/* The receiver is that `$this` — never the object the `::` `` |
|       - | 2231 | `							 * was written on, and never nothing. Naming it here is also` |
|       - | 2232 | `							 * what makes the call work inside a CLOSURE declared in a` |
|       - | 2233 | ``							 * method: php gives such a closure a `$this`, PHL carries it`` |
|       - | 2234 | `							 * as a frame variable rather than on the frame itself, and` |
|       - | 2235 | ``							 * OP_CALL reads only the frame, so `self::m()` in a closure`` |
|       - | 2236 | `							 * used to run with no receiver at all. */` |
|     118 | 2237 | `							PH7_MemObjRelease(pNos);` |
|     118 | 2238 | `							pNos->x.pOther = pCallerThis;` |
|     118 | 2239 | `							MemObjSetType(pNos,MEMOBJ_OBJ);` |
|     118 | 2240 | `							pCallerThis->iRef++;` |
|     118 | 2241 | `							pNos->nIdx = SXU32_HIGH;` |
|      58 | 2242 | `						}` |
|       - | 2243 | `						/* Push method name on the stack, MARKED as already screened (see the` |
|       - | 2244 | `						 * instance twin: OP_CALL cannot re-derive a composed protection). */` |
|    1377 | 2245 | `						PH7_MemObjRelease(pTos);` |
|    1377 | 2246 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|    1377 | 2247 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|    1377 | 2248 | `						pTos->iFlags \|= MEMOBJ_AUX_MEMBERCALL;` |
|       - | 2249 | `					}` |
|    1377 | 2250 | `					pTos->nIdx = SXU32_HIGH;` |
|       - | 2251 | `					/* Forwarding call (self::/parent::/static::): overwrite the receiver` |
|       - | 2252 | `					 * slot (pNos, one below the method name) with the live LSB class name` |
|       - | 2253 | ``					 * so OP_CALL pushes THAT onto aSelf — preserving `static::` inside the`` |
|       - | 2254 | `					 * callee. Without this the literal keyword falls through to the` |
|       - | 2255 | `					 * callee's declaring class. Only when an LSB class is actually in` |
|       - | 2256 | `					 * scope (a static call from global scope has none). */` |
|    1377 | 2257 | `					if( bForwardingCall && pForwardLsb && (pNos->iFlags & MEMOBJ_STRING) ){` |
|      80 | 2258 | `						SyBlobReset(&pNos->sBlob);` |
|      80 | 2259 | `						SyBlobAppend(&pNos->sBlob,pForwardLsb->sName.zString,pForwardLsb->sName.nByte);` |
|      39 | 2260 | `					}` |
|     691 | 2261 | `				}else{` |
|       - | 2262 | `					/* Attribute access */` |
|  101519 | 2263 | `					ph7_class_attr *pAttr = 0;` |
|  101519 | 2264 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - | 2265 | `						/* unset(C::$x): PHP rejects unsetting a static property with a fatal Error.` |
|       - | 2266 | `						 * Without this the iP2=unset tag falls through to a normal static read and the` |
|       - | 2267 | `						 * trailing generic unset() would silently NULL (and de-type) the shared slot. */` |
|       - | 2268 | `						char zMsg[256];` |
|     ! 0 | 2269 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Attempt to unset static property %.*s::$%.*s",` |
|     ! 0 | 2270 | `							(int)pClass->sName.nByte,pClass->sName.zString,(int)sName.nByte,sName.zString);` |
|     ! 0 | 2271 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|     ! 0 | 2272 | `						VM_EXIT_ABORT;` |
|       - | 2273 | `					}` |
|       - | 2274 | `					/* Check for special ::class pseudo-constant */` |
|  101679 | 2275 | `					if( sName.nByte == sizeof("class")-1 &&` |
|     320 | 2276 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|       - | 2277 | `						/* ::class returns the fully qualified class name */` |
|       - | 2278 | `						/* Pop the attribute name from the stack */` |
|     152 | 2279 | `						if( !pInstr->p3 ){` |
|     152 | 2280 | `							VmPopOperand(&pTos,1);` |
|      74 | 2281 | `						}` |
|     152 | 2282 | `						PH7_MemObjRelease(pTos);` |
|       - | 2283 | `						/* Load the class name */` |
|     152 | 2284 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|     152 | 2285 | `						pTos->nIdx = SXU32_HIGH;` |
|      78 | 2286 | `					}else{` |
|       - | 2287 | `						/* Extract the target attribute. php keeps constants and` |
|       - | 2288 | `						 * (static) properties in separate namespaces; the source` |
|       - | 2289 | ``						 * form disambiguates (set by the `::` codegen, compile.c):`` |
|       - | 2290 | ``						 * a `$`-form is a STATIC PROPERTY (hAttr) — either a literal`` |
|       - | 2291 | ``						 * name folded into pInstr->p3 (`C::$s`) or a dynamic name on`` |
|       - | 2292 | ``						 * the stack marked iP1==2 (`C::$$x`, `C::${$e}`); a bareword`` |
|       - | 2293 | `						 * (p3==0, iP1==1) is a CONSTANT or enum case (hConst). This` |
|       - | 2294 | ``						 * is what lets `const C` and `public $C` coexist and resolve`` |
|       - | 2295 | `						 * to the right member. */` |
|  101371 | 2296 | `						if( sName.nByte > 0 ){` |
|  101838 | 2297 | `							pAttr = (pInstr->p3 \|\| pInstr->iP1 == 2)` |
|  100438 | 2298 | `								? PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte)` |
|   51611 | 2299 | `								: PH7_ClassExtractConstant(pClass,sName.zString,sName.nByte);` |
|   50683 | 2300 | `						}` |
|  101371 | 2301 | `						if( pAttr == 0 ){` |
|       - | 2302 | `							/* No such member. php raises a catchable Error whose wording` |
|       - | 2303 | `							 * depends on the ACCESS form (the same p3/iP1 signal used above):` |
|       - | 2304 | ``							 * a bareword `C::MISSING` (constant form) is "Undefined constant`` |
|       - | 2305 | ``							 * C::MISSING", a `$`-form `C::$missing` is "Access to undeclared`` |
|       - | 2306 | `							 * static property C::$missing". Instance magic (__get) is never` |
|       - | 2307 | `							 * consulted for statics (band A #3b). isset()/empty() context` |
|       - | 2308 | `							 * stays silently false. Parked on the boundary rail; the op` |
|       - | 2309 | `							 * completes benignly with NULL and the fetch-point router lands` |
|       - | 2310 | `							 * the throw. */` |
|      14 | 2311 | `							if( !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2312 | `								SyBlob sErrMsg;` |
|      14 | 2313 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      14 | 2314 | `								if( pInstr->p3 == 0 && pInstr->iP1 != 2 ){` |
|       7 | 2315 | `									SyBlobFormat(&sErrMsg,"Undefined constant %z::%z",` |
|       3 | 2316 | `										&pClass->sName,&sName);` |
|       4 | 2317 | `								}else{` |
|       8 | 2318 | `									SyBlobFormat(&sErrMsg,"Access to undeclared static property %z::$%z",` |
|       3 | 2319 | `										&pClass->sName,&sName);` |
|       - | 2320 | `								}` |
|      14 | 2321 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       6 | 2322 | `							}` |
|       6 | 2323 | `						}` |
|       - | 2324 | `						/* Pop the attribute name from the stack */` |
|  101371 | 2325 | `						if( !pInstr->p3 ){` |
|     939 | 2326 | `							VmPopOperand(&pTos,1);` |
|     467 | 2327 | `						}` |
|  101371 | 2328 | `						PH7_MemObjRelease(pTos);` |
|  101371 | 2329 | `						pTos->nIdx = SXU32_HIGH;` |
|  101371 | 2330 | `						if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - | 2331 | ``							/* `self::$s =& $x` / `C::$s =& $x`: stash the static property slot`` |
|       - | 2332 | `							 * for the following member-marked OP_STORE_REF (class-level, shared` |
|       - | 2333 | `							 * across instances — matches php). Skip the read machinery below. */` |
|      14 | 2334 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|      14 | 2335 | `							 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|      16 | 2336 | `							 && VmClassStaticDeferPending(pClass) ){` |
|       - | 2337 | `								/* Binding a reference TO a static touches the table, so it` |
|       - | 2338 | `								 * materializes first, like every other static access. */` |
|       3 | 2339 | `								sxi32 rcRt = PH7_VmMaterializeClassStatics(&(*pVm),pClass);` |
|       3 | 2340 | `								if( rcRt != SXRET_OK ){` |
|       3 | 2341 | `									pVm->pRefTargetStaticAttr = 0;` |
|       3 | 2342 | `									pVm->pRefTargetAttr = 0;` |
|       3 | 2343 | `									pVm->pRefTargetThis = 0;` |
|       3 | 2344 | `									if( pThis ){` |
|     ! 0 | 2345 | `										PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2346 | `									}` |
|       3 | 2347 | `									if( rcRt == PH7_ABORT ){` |
|     ! 0 | 2348 | `										VM_EXIT_ABORT;` |
|       - | 2349 | `									}` |
|       - | 2350 | `									{` |
|       - | 2351 | `										sxi32 iRpR;` |
|       3 | 2352 | `										if( VmRecordedResume(pVm,&iRpR,pState->pEntryFrame,aInstr) ){` |
|       7 | 2353 | `											PH7_RESUME_DRAIN()` |
|       3 | 2354 | `											pc = iRpR;` |
|       3 | 2355 | `											VM_EXIT_BREAK;` |
|       - | 2356 | `										}` |
|       - | 2357 | `									}` |
|     ! 0 | 2358 | `									VM_EXIT_EXCEPTION;` |
|       - | 2359 | `								}` |
|     ! 0 | 2360 | `							}` |
|      12 | 2361 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|      14 | 2362 | `							 && PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|      14 | 2363 | `								pVm->pRefTargetStaticAttr = pAttr;` |
|      14 | 2364 | `								pVm->pRefTargetAttr = 0;` |
|      14 | 2365 | `								pVm->pRefTargetThis = 0;` |
|      14 | 2366 | `								pTos->nIdx = pAttr->nIdx;` |
|       8 | 2367 | `							}else{` |
|     ! 0 | 2368 | `								pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 2369 | `								pVm->pRefTargetAttr = 0;` |
|     ! 0 | 2370 | `								pVm->pRefTargetThis = 0;` |
|       - | 2371 | `							}` |
|      14 | 2372 | `							if( pThis ){` |
|     ! 0 | 2373 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2374 | `							}` |
|      14 | 2375 | `							VM_EXIT_BREAK;` |
|       - | 2376 | `						}` |
|  101357 | 2377 | `						if( pAttr ){` |
|  101345 | 2378 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       - | 2379 | `								/* Access to a non static attribute */` |
|     ! 0 | 2380 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|     ! 0 | 2381 | `									&pClass->sName,&pAttr->sName` |
|       - | 2382 | `									);` |
|     ! 0 | 2383 | `							}else{` |
|       - | 2384 | `								ph7_value *pValue;` |
|       - | 2385 | `								/* php materializes the class's static table at the FIRST` |
|       - | 2386 | `								 * static-property access (any property, any context — read,` |
|       - | 2387 | `								 * write, even isset): a default whose evaluation was DEFERRED` |
|       - | 2388 | `								 * (it threw at the declaration) runs here, and a typed one that` |
|       - | 2389 | `								 * failed its check throws its catchable TypeError here. Class` |
|       - | 2390 | `								 * constants and static method calls do not trigger the` |
|       - | 2391 | `								 * materialization (php-exact). */` |
|  101340 | 2392 | `								if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|  100879 | 2393 | `								 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|  100423 | 2394 | `								 && VmClassStaticDeferPending(pClass) ){` |
|      49 | 2395 | `									sxi32 rcD = PH7_VmMaterializeClassStatics(&(*pVm),pClass);` |
|      49 | 2396 | `									if( rcD != SXRET_OK ){` |
|      45 | 2397 | `										if( pThis ){` |
|     ! 0 | 2398 | `											PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2399 | `										}` |
|      45 | 2400 | `										if( rcD == PH7_ABORT ){` |
|     ! 0 | 2401 | `											VM_EXIT_ABORT;` |
|       - | 2402 | `										}` |
|       - | 2403 | `										{` |
|       - | 2404 | `											sxi32 iRpD;` |
|      45 | 2405 | `											if( VmRecordedResume(pVm,&iRpD,pState->pEntryFrame,aInstr) ){` |
|      89 | 2406 | `												PH7_RESUME_DRAIN()` |
|      41 | 2407 | `												pc = iRpD;` |
|      41 | 2408 | `												VM_EXIT_BREAK;` |
|       - | 2409 | `											}` |
|       - | 2410 | `										}` |
|       5 | 2411 | `										VM_EXIT_EXCEPTION;` |
|       - | 2412 | `									}` |
|       2 | 2413 | `								}` |
|       - | 2414 | `								/* Check if the access to the attribute is allowed */` |
|  101303 | 2415 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|       - | 2416 | `									/* PHP 7.4+: uninitialized typed static read.` |
|       - | 2417 | `									 * Same LHS-of-store peek as the instance path. */` |
|  101294 | 2418 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|  100721 | 2419 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|  150143 | 2420 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|  100092 | 2421 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|  100097 | 2422 | `										if( pS ){` |
|  100097 | 2423 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|  100097 | 2424 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|  100014 | 2425 | `												VmInstr *pNext = pInstr + 1;` |
|  100014 | 2426 | `												int bIsLhs = 0;` |
|  100014 | 2427 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       9 | 2428 | `													bIsLhs = 1;` |
|       3 | 2429 | `												}` |
|  100014 | 2430 | `												if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 2431 | `													/* Destructuring target ([S::$s] = [...]):` |
|       - | 2432 | `													 * initialized by the OP_LOAD_LIST store. */` |
|       3 | 2433 | `													bIsLhs = 1;` |
|       1 | 2434 | `												}` |
|  100014 | 2435 | `												if( !bIsLhs ){` |
|  100003 | 2436 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|  100003 | 2437 | `													if( pThis ){` |
|     ! 0 | 2438 | `														PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2439 | `													}` |
|  100003 | 2440 | `													if( rcU == PH7_ABORT ){` |
|     ! 0 | 2441 | `														VM_EXIT_ABORT;` |
|       - | 2442 | `													}` |
|       - | 2443 | `													{` |
|       - | 2444 | `														sxi32 iRp;` |
|  100003 | 2445 | `														if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|  200005 | 2446 | `															PH7_RESUME_DRAIN()` |
|  100003 | 2447 | `															pc = iRp;` |
|  100003 | 2448 | `															VM_EXIT_BREAK;` |
|       - | 2449 | `														}` |
|       - | 2450 | `													}` |
|     ! 0 | 2451 | `													VM_EXIT_EXCEPTION;` |
|       - | 2452 | `												}` |
|       4 | 2453 | `											}` |
|      45 | 2454 | `										}` |
|      45 | 2455 | `									}` |
|    1292 | 2456 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|    1110 | 2457 | `									 && SySetUsed(&pAttr->aAttrs) > 0 ){` |
|       - | 2458 | `										/* php 8.4 #[\Deprecated] on a class constant:` |
|       - | 2459 | `										 * every access re-warns. */` |
|      11 | 2460 | `										VmDeprecatedConstNotice(&(*pVm),pClass,pAttr);` |
|       5 | 2461 | `									}` |
|    1297 | 2462 | `									if( pAttr->nIdx == SXU32_HIGH ){` |
|       - | 2463 | `										/* Unmaterialized slot. Enum case: first access` |
|       - | 2464 | `										 * materializes ALL the singletons. Plain constant: its` |
|       - | 2465 | `										 * initializer hasn't run yet (mount-order-dependent` |
|       - | 2466 | `										 * cross-constant reference) — evaluate on demand. A` |
|       - | 2467 | `										 * raised TypeError/Error (backing mismatch, duplicate` |
|       - | 2468 | `										 * value, self-reference) parks on the boundary rail;` |
|       - | 2469 | `										 * the op completes benignly with NULL and the` |
|       - | 2470 | `										 * fetch-point router lands the throw. */` |
|       - | 2471 | `										sxi32 rcEnum;` |
|     473 | 2472 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|       - | 2473 | `											/* php: a DIRECT static access evaluates every case` |
|       - | 2474 | `											 * of the enum (whole-class constant update) — a` |
|       - | 2475 | `											 * broken sibling case throws here too. A reference` |
|       - | 2476 | `											 * from inside another constant's initializer` |
|       - | 2477 | `											 * (nConstEvalDepth > 0) evaluates only the` |
|       - | 2478 | `											 * requested case. */` |
|      57 | 2479 | `											if( pVm->nConstEvalDepth > 0 ){` |
|       3 | 2480 | `												rcEnum = VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|       2 | 2481 | `											}else{` |
|      55 | 2482 | `												rcEnum = VmEnumMaterialize(&(*pVm),pClass);` |
|       - | 2483 | `											}` |
|      31 | 2484 | `										}else{` |
|     421 | 2485 | `											rcEnum = VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|       - | 2486 | `										}` |
|     473 | 2487 | `										if( rcEnum != SXRET_OK ){` |
|      41 | 2488 | `											VmBoundaryPark(&(*pVm),rcEnum);` |
|      19 | 2489 | `										}` |
|     234 | 2490 | `									}` |
|       - | 2491 | `									/* Load the desired attribute */` |
|    1297 | 2492 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|    1297 | 2493 | `									if( pValue ){` |
|    1257 | 2494 | `										PH7_MemObjLoad(pValue,pTos);` |
|    1257 | 2495 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       - | 2496 | `											/* Load index number */` |
|     379 | 2497 | `											pTos->nIdx = pAttr->nIdx;` |
|     187 | 2498 | `										}` |
|     626 | 2499 | `									}` |
|     651 | 2500 | `								}else{` |
|       - | 2501 | `									/* Throw Error exception (PHP-compatible) */` |
|       - | 2502 | `									char zMsg[256];` |
|       5 | 2503 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       5 | 2504 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|       7 | 2505 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|       4 | 2506 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|       4 | 2507 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|       3 | 2508 | `									}else{` |
|     ! 0 | 2509 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|     ! 0 | 2510 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|     ! 0 | 2511 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|       - | 2512 | `									}` |
|       5 | 2513 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|       5 | 2514 | `									VM_EXIT_ABORT;` |
|       - | 2515 | `								}` |
|       - | 2516 | `							}` |
|     646 | 2517 | `						}` |
|       - | 2518 | `					}` |
|       - | 2519 | `				}` |
|    2829 | 2520 | `				if( pThis ){` |
|       - | 2521 | `					/* Safely unreference the object */` |
|      22 | 2522 | `					PH7_ClassInstanceUnref(pThis);` |
|      10 | 2523 | `				}` |
|       - | 2524 | `			}` |
|    1417 | 2525 | `		}else{` |
|       - | 2526 | ``			/* `$v::X` where $v holds neither an object nor a class NAME. php's`` |
|       - | 2527 | `			 * catchable Error, which STOPS the statement; PH7 raised its own` |
|       - | 2528 | `` 			 * uncatchable wording and carried on with NULL, so a `$obj::$s = 1` `` |
|       - | 2529 | `			 * through a null base went on to fail again in OP_STORE. */` |
|       - | 2530 | `			sxi32 rcCn;` |
|       5 | 2531 | `			if( !pInstr->p3 ){` |
|       3 | 2532 | `				VmPopOperand(&pTos,1);` |
|       1 | 2533 | `			}` |
|       5 | 2534 | `			PH7_MemObjRelease(pTos);` |
|       5 | 2535 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 2536 | `			pTos->nIdx = SXU32_HIGH;` |
|       5 | 2537 | `			rcCn = VmThrowFromVm(&(*pVm),"Error","Class name must be a valid object or a string",` |
|       - | 2538 | `				sizeof("Class name must be a valid object or a string")-1);` |
|       5 | 2539 | `			if( rcCn == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 2540 | `			rc = rcCn;` |
|       5 | 2541 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2542 | `		}` |
|       - | 2543 | `	}` |
|  224798 | 2544 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2545 | `	VM_EXIT_BREAK;` |
|  162981 | 2546 | `}` |
|       - | 2547 |  |
|       - | 2548 | `/*` |
|       - | 2549 | ` * OP_CLONE: body moved verbatim from the OP_CLONE arm of` |
|       - | 2550 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2551 | ` */` |
|     244 | 2552 | `PH7_PRIVATE VmOpRc VmExecOpClone(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2553 | `{` |
|     249 | 2554 | `	ph7_value *pTos = pState->pTos;` |
|     249 | 2555 | `	ph7_value *pStack = pState->pStack;` |
|     249 | 2556 | `	VmInstr *aInstr = pState->aInstr;` |
|     249 | 2557 | `	sxi32 pc = pState->pc;` |
|       - | 2558 | `	sxi32 rc;` |
|     122 | 2559 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2560 | `	ph7_class_instance *pSrc,*pClone;` |
|       - | 2561 | `#ifdef UNTRUST` |
|       - | 2562 | `	if( pTos < pStack ){` |
|       - | 2563 | `		VM_EXIT_ABORT;` |
|       - | 2564 | `	}` |
|       - | 2565 | `#endif` |
|       - | 2566 | `	/* Make sure we are dealing with a class instance. PHP 8 throws a catchable` |
|       - | 2567 | ``	 * TypeError for a non-object operand — for both `clone $x` and clone($x). */`` |
|     249 | 2568 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - | 2569 | `		SyBlob sMsg;` |
|       - | 2570 | `		char zGiven[64];` |
|      24 | 2571 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 2572 | `		/* php names the VALUE for a bool ("true given", not "bool given"), which is` |
|       - | 2573 | `		 * what every other argument diagnostic here already says — the shared helper. */` |
|      24 | 2574 | `		SyBlobFormat(&sMsg,"clone(): Argument #1 ($object) must be of type object, %s given",` |
|      11 | 2575 | `			VmValueGivenName(pTos,zGiven,sizeof(zGiven)));` |
|      24 | 2576 | `		rc = VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|      24 | 2577 | `		PH7_MemObjRelease(pTos);` |
|      24 | 2578 | `		pTos->nIdx = SXU32_HIGH;` |
|      24 | 2579 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 2580 | `			VM_EXIT_ABORT;` |
|       - | 2581 | `		}` |
|       - | 2582 | `		{` |
|       - | 2583 | `			sxi32 iRp;` |
|      24 | 2584 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      24 | 2585 | `				pc = iRp;` |
|      24 | 2586 | `				VM_EXIT_BREAK;` |
|       - | 2587 | `			}` |
|       - | 2588 | `		}` |
|     ! 0 | 2589 | `		VM_EXIT_EXCEPTION;` |
|       - | 2590 | `	}` |
|       - | 2591 | `	/* Point to the source */` |
|     227 | 2592 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|       - | 2593 | `	/* Enum cases are not cloneable — php's catchable Error (the singleton` |
|       - | 2594 | `	 * identity would break) — and neither is a class whose instances own a` |
|       - | 2595 | `	 * C-side resource a slot-by-slot copy would double-free (PH7_CLASS_NOCLONE),` |
|       - | 2596 | `	 * nor a Generator or a Fiber (their suspended C state is not copyable). php` |
|       - | 2597 | `	 * words all of them the same way and throws the same catchable Error; the` |
|       - | 2598 | `	 * Generator/Fiber pair used to take an older warn-only path here, so` |
|       - | 2599 | ``	 * `clone $gen` PRINTED an uncatchable diagnostic and then carried on with the`` |
|       - | 2600 | ``	 * ORIGINAL object standing in for the copy — the one shape of `clone` that`` |
|       - | 2601 | `	 * answered a value php never lets the program reach. */` |
|     222 | 2602 | `	if( (pSrc->pClass->iFlags & (PH7_CLASS_ENUM\|PH7_CLASS_NOCLONE))` |
|     190 | 2603 | `		\|\| pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|       - | 2604 | `		SyBlob sMsg;` |
|      80 | 2605 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|      80 | 2606 | `		SyBlobFormat(&sMsg,"Trying to clone an uncloneable object of class %z",` |
|      78 | 2607 | `			&pSrc->pClass->sName);` |
|      80 | 2608 | `		rc = VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|      80 | 2609 | `		PH7_MemObjRelease(pTos);` |
|      80 | 2610 | `		pTos->nIdx = SXU32_HIGH;` |
|      80 | 2611 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 2612 | `			VM_EXIT_ABORT;` |
|       - | 2613 | `		}` |
|       - | 2614 | `		{` |
|       - | 2615 | `			sxi32 iRp;` |
|      80 | 2616 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      62 | 2617 | `				pc = iRp;` |
|      62 | 2618 | `				VM_EXIT_BREAK;` |
|       - | 2619 | `			}` |
|       - | 2620 | `		}` |
|      19 | 2621 | `		VM_EXIT_EXCEPTION;` |
|       - | 2622 | `	}` |
|       - | 2623 | `	/* Perform the clone operation */` |
|     149 | 2624 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|     149 | 2625 | `	PH7_MemObjRelease(pTos);` |
|     149 | 2626 | `	if( pClone == 0 ){` |
|     ! 0 | 2627 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 2628 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|     ! 0 | 2629 | `	}else{` |
|       - | 2630 | `		/* Load the cloned object */` |
|     149 | 2631 | `		pTos->x.pOther = pClone;` |
|     149 | 2632 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - | 2633 | `	}` |
|     149 | 2634 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2635 | `	VM_EXIT_BREAK;` |
|     127 | 2636 | `}` |
|       - | 2637 |  |
