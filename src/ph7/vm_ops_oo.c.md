# src/ph7/vm_ops_oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1556/1735 lines (89.68%)

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
| 2128196 |   28 | `PH7_PRIVATE VmOpRc VmExecOpNew(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   29 | `{` |
| 2128201 |   30 | `	ph7_value *pTos = pState->pTos;` |
| 2128201 |   31 | `	ph7_value *pStack = pState->pStack;` |
| 2128201 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
| 2128201 |   33 | `	sxi32 pc = pState->pc;` |
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
| 2128201 |   49 | `	int bScreenOnly = pInstr->iP1 < 0;` |
| 2687020 |   50 | `	sxi32 nCtorArgs = bScreenOnly ? 0` |
| 1622917 |   51 | `		: pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|       - |   52 | `	ph7_value *pArg;` |
| 2128201 |   53 | `	ph7_class *pClass = 0;` |
|       - |   54 | `	ph7_class_instance *pNew;` |
|       - |   55 | `	/* Resolving the class below can run an AUTOLOADER that throws. Snapshot the boundary` |
|       - |   56 | `	 * rail so the "not found" report can tell a missing class from an autoloader that` |
|       - |   57 | `	 * raised — php propagates the autoloader's exception and reports nothing else, while` |
|       - |   58 | `	 * this arm used to throw its own Error on top of it (uncaught, killing the script` |
|       - |   59 | `	 * right after the real exception had been handled). */` |
| 2128201 |   60 | `	sxi32 nNewBrc = pVm->nBoundaryRc;` |
| 2128201 |   61 | `	const void *pNewRes = (const void *)pVm->pResumeFrame;` |
| 2128201 |   62 | `	pArg = &pTos[-nCtorArgs]; /* Constructor arguments (if available) */` |
|       - |   63 | `	/* Same PHP 8.1 spread-key realignment as OP_CALL: build a per-actual-slot name` |
|       - |   64 | `	 * map when the ctor arg list unpacked string-keyed elements. NEW keeps the` |
|       - |   65 | `	 * class-name slot at pTos (above the args), so pArg is already the correct base;` |
|       - |   66 | `	 * the build also truncates this call's captured runs. */` |
|       - |   67 | `	VmCallArgMap sEffNewMap;` |
|       - |   68 | `	/* The screen pass has no arguments and no runs of its own: building (and` |
|       - |   69 | `	 * truncating) an effective map there would speak for the enclosing call. */` |
| 2687020 |   70 | `	VmCallArgMap *pEffNewMap = bScreenOnly ? 0 : VmEffCallArgMap(pVm,pInstr,pArg,` |
| 1117638 |   71 | `		nCtorArgs > 0 ? (sxu32)nCtorArgs : 0,&sEffNewMap);` |
| 3192297 |   72 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
| 2128197 |   73 | `		const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
| 2128197 |   74 | `		sxu32 nCls = SyBlobLength(&pTos->sBlob);` |
| 2128192 |   75 | `		if( (nCls == sizeof("self")-1 && SyMemcmp(zCls,"self",sizeof("self")-1) == 0)` |
| 2128180 |   76 | `		 \|\| (nCls == sizeof("static")-1 && SyMemcmp(zCls,"static",sizeof("static")-1) == 0)` |
| 2128136 |   77 | `		 \|\| (nCls == sizeof("parent")-1 && SyMemcmp(zCls,"parent",sizeof("parent")-1) == 0) ){` |
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
| 2128133 |   95 | `			pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,` |
|       - |   96 | `				TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|       5 |   97 | `		}` |
| 1064101 |   98 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - |   99 | `		/* Take the base class from the loaded instance */` |
|       5 |  100 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       2 |  101 | `	}` |
| 2128201 |  102 | `	if( pClass == 0 ){` |
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
| 2128161 |  166 | `	}else if( pClass->iFlags & PH7_CLASS_NOINSTANTIATE ){` |
|       - |  167 | `		/* php refuses this one in the create_object handler, so the refusal names the` |
|       - |  168 | `		 * INSTANTIATION and never reaches the constructor — which matters because the` |
|       - |  169 | `		 * class may still declare a private __construct that Reflection prints. Same` |
|       - |  170 | `		 * shape as the enum reject below: no instance, no __destruct. */` |
|       - |  171 | `		SyBlob sErrMsg;` |
|      20 |  172 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      20 |  173 | `		if( pClass->zNewRefusal ){` |
|       - |  174 | `			/* php words a few of these per class — Directory's names dir() as the` |
|       - |  175 | `			 * way to get one — so the spec's own sentence wins when it has one. */` |
|      16 |  176 | `			SyBlobAppend(&sErrMsg,pClass->zNewRefusal,SyStrlen(pClass->zNewRefusal));` |
|       9 |  177 | `		}else{` |
|       5 |  178 | `			SyBlobFormat(&sErrMsg,"Instantiation of class %z is not allowed",&pClass->sName);` |
|       - |  179 | `		}` |
|       - |  180 | `		{` |
|      20 |  181 | `			const char *zRefCls = pClass->zNewRefusalClass ? pClass->zNewRefusalClass : "Error";` |
|      29 |  182 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),zRefCls,` |
|      18 |  183 | `				(sxu32)SyStrlen(zRefCls),&sErrMsg));` |
|       - |  184 | `		}` |
|      20 |  185 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  186 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  187 | `		}` |
|      20 |  188 | `		PH7_MemObjRelease(pTos);` |
|      20 |  189 | `		pTos->nIdx = SXU32_HIGH;` |
|      20 |  190 | `		VM_EXIT_BREAK;` |
| 2128143 |  191 | `	}else if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|       - |  192 | `		/* php 8.1: enums cannot be instantiated — a catchable Error, raised` |
|       - |  193 | `		 * BEFORE any construction (no instance, no __destruct). */` |
|       - |  194 | `		SyBlob sErrMsg;` |
|       9 |  195 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       9 |  196 | `		SyBlobFormat(&sErrMsg,"Cannot instantiate enum %z",&pClass->sName);` |
|       9 |  197 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       9 |  198 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  199 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  200 | `		}` |
|       9 |  201 | `		PH7_MemObjRelease(pTos);` |
|       9 |  202 | `		pTos->nIdx = SXU32_HIGH;` |
|       9 |  203 | `		VM_EXIT_BREAK;` |
| 2128130 |  204 | `	}else if( VmClassStaticDeferPending(pClass)` |
| 1064072 |  205 | `	       && (rc = PH7_VmMaterializeClassStatics(&(*pVm),pClass)) != SXRET_OK ){` |
|       - |  206 | `		/* php materializes the static table at instantiation too, so a default` |
|       - |  207 | `		 * that THREW at the declaration (or failed its type check) raises BEFORE` |
|       - |  208 | `		 * any construction (no instance, no __destruct) — same shape as the enum` |
|       - |  209 | `		 * reject above: park the status, settle the stack, and let the` |
|       - |  210 | `		 * fetch-point router land it. */` |
|       6 |  211 | `		VmBoundaryPark(&(*pVm),rc);` |
|       6 |  212 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  213 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  214 | `		}` |
|       6 |  215 | `		PH7_MemObjRelease(pTos);` |
|       6 |  216 | `		pTos->nIdx = SXU32_HIGH;` |
|       6 |  217 | `		VM_EXIT_BREAK;` |
|     ! 0 |  218 | `	}else{` |
|       - |  219 | `		ph7_class_method *pCons;` |
|       - |  220 | `		/* Check if a constructor is available — BEFORE instantiation: a` |
|       - |  221 | ``		 * visibility-denied `new` must not construct (nor later destruct)`` |
|       - |  222 | `		 * the object (band A #4). */` |
|       - |  223 | `		/* Only an explicit __construct is the constructor. PHP-4-style class-name` |
|       - |  224 | `		 * constructors were removed in PHP 8.0 — a method named like the class is a` |
|       - |  225 | `		 * plain method, so no same-name fallback here. */` |
| 2128131 |  226 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       - |  227 | `		/* Constructor visibility (band A #4): __construct now KEEPS its` |
|       - |  228 | `		 * declared protection (PH7_NewClassMethod no longer forces it` |
|       - |  229 | ``		 * public), so `new C()` on a private/protected constructor from`` |
|       - |  230 | `		 * the wrong scope is php's catchable Error. Reflection's` |
|       - |  231 | `		 * newInstance path sets bReflectBypass like method invoke. */` |
| 2128131 |  232 | `		if( pCons && pCons->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - |  233 | `			/* php binds non-public access by the member's DECLARING class, not the` |
|       - |  234 | `			 * instantiated class: a private constructor declared in a base is` |
|       - |  235 | `			 * reachable from that base's own methods even when instantiating a` |
|       - |  236 | ``			 * subclass (`new Child()` inside Base::factory()). __construct is a`` |
|       - |  237 | `			 * method, so pass its declaring class (sFunc.pUserData) — mirroring the` |
|       - |  238 | `			 * method-call visibility check — rather than pClass, whose hAttr lookup` |
|       - |  239 | `			 * for a method name always misses and falls to a wrong exact-class test. */` |
|      39 |  240 | `			ph7_class *pCtorDecl = pCons->sFunc.pUserData ? (ph7_class *)pCons->sFunc.pUserData : pClass;` |
|      39 |  241 | `			if( pVm->bReflectBypass ){` |
|     ! 0 |  242 | `				pVm->bReflectBypass = 0;` |
|      39 |  243 | `			}else if( !PH7_VmClassMemberAccess(&(*pVm),pCtorDecl,&pCons->sFunc.sName,pCons->iProtection,FALSE) ){` |
|       - |  244 | `				SyBlob sErrMsg;` |
|      25 |  245 | `				const char *zVis = pCons->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       - |  246 | `				/* php NAMES the calling scope when there is one (see the method twin in` |
|       - |  247 | `				 * OP_CALL); "global scope" is only for code outside every class. */` |
|      25 |  248 | `				ph7_class *pCtorScope = PH7_VmCallerScopeName(&(*pVm));` |
|      25 |  249 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      25 |  250 | `				if( pCtorScope ){` |
|       9 |  251 | `					SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from scope %z",` |
|       4 |  252 | `						zVis,&pClass->sName,&pCtorScope->sName);` |
|       5 |  253 | `				}else{` |
|      17 |  254 | `					SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from global scope",` |
|       8 |  255 | `						zVis,&pClass->sName);` |
|       - |  256 | `				}` |
|      25 |  257 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       - |  258 | `				/* Pop ctor args + release the class-name operand, leave NULL` |
|       - |  259 | `				 * as the expression value; the fetch-point router lands the` |
|       - |  260 | `				 * throw. No instance was created. */` |
|      25 |  261 | `				if( nCtorArgs > 0 ){` |
|     ! 0 |  262 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  263 | `				}` |
|      25 |  264 | `				PH7_MemObjRelease(pTos);` |
|      25 |  265 | `				pTos->nIdx = SXU32_HIGH;` |
|      25 |  266 | `				VM_EXIT_BREAK;` |
|       - |  267 | `			}` |
|       7 |  268 | `		}` |
| 2128107 |  269 | `		if( bScreenOnly ){` |
|       - |  270 | `			/* Every refusal above has been asked. Leave the class name standing for` |
|       - |  271 | `			 * the real pass and let the arguments run. */` |
| 1010539 |  272 | `			VM_EXIT_BREAK;` |
|       - |  273 | `		}` |
| 1117573 |  274 | `		if( nCtorArgs > 0 ){` |
|       - |  275 | ``			/* D1: a `new`'s arguments are ARGUMENTS. An element/property one whose`` |
|       - |  276 | `			 * target was absent at load time rides a deferred carrier that only OP_CALL` |
|       - |  277 | `			 * resolved, and the ctor call reaches its callee by pointer rather than` |
|       - |  278 | ``			 * through that opcode — so `new C($a['missing'])` passed a silent NULL where`` |
|       - |  279 | ``			 * php warns, and a by-REFERENCE ctor parameter (`new C($a['fresh'])` with`` |
|       - |  280 | ``			 * `__construct(&$x)`) never vivified the target at all: php writes the`` |
|       - |  281 | `			 * element, PHL left it uncreated. Resolve here, where the constructor is` |
|       - |  282 | `			 * known and pVm->pFrame is still the caller, exactly as every OP_CALL` |
|       - |  283 | `			 * dispatch branch does. A class with NO constructor still resolves — the` |
|       - |  284 | `			 * arguments were evaluated and php reports what reading them found. */` |
| 1010555 |  285 | `			ph7_class_method *pCtorArgs = pCons;` |
|       - |  286 | `			sxi32 rcDA;` |
| 1010555 |  287 | `			if( pCtorArgs == 0 ){` |
|       3 |  288 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pEffNewMap);` |
| 1010554 |  289 | `			}else if( pCtorArgs->sFunc.iFlags & VM_FUNC_NATIVE ){` |
|       - |  290 | `				/* A native constructor has no compiled formals; its by-ref positions` |
|       - |  291 | `				 * come from the signature-derived mask, the builtin way. */` |
| 1009959 |  292 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
| 1009954 |  293 | `					pCtorArgs->sFunc.pNative ? pCtorArgs->sFunc.pNative->nByRefMask : 0,0,0,pEffNewMap);` |
|  504982 |  294 | `			}else{` |
|     896 |  295 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|     594 |  296 | `					(ph7_vm_func_arg *)SySetBasePtr(&pCtorArgs->sFunc.aArgs),` |
|     297 |  297 | `					SySetUsed(&pCtorArgs->sFunc.aArgs),0,0,0,pEffNewMap);` |
|       - |  298 | `			}` |
| 1010555 |  299 | `			if( rcDA == PH7_ABORT \|\| rcDA == PH7_EXCEPTION ){` |
|       - |  300 | `				/* Reading an argument raised (a string-offset Error, a magic accessor's` |
|       - |  301 | `				 * throw): no object is created, and the stack is tidied exactly as the` |
|       - |  302 | `				 * constructor-throw path below tidies it. */` |
|       - |  303 | `				sxi32 iResumeDA;` |
|       3 |  304 | `				if( rcDA == PH7_ABORT ){` |
|     ! 0 |  305 | `					VM_EXIT_ABORT;` |
|       - |  306 | `				}` |
|       3 |  307 | `				if( VmRecordedResume(pVm,&iResumeDA,pState->pEntryFrame,aInstr) ){` |
|     ! 0 |  308 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  309 | `					PH7_MemObjRelease(pTos);` |
|     ! 0 |  310 | `					PH7_RESUME_DRAIN()` |
|     ! 0 |  311 | `					pc = iResumeDA;` |
|     ! 0 |  312 | `					VM_EXIT_BREAK;` |
|       - |  313 | `				}` |
|       3 |  314 | `				VM_EXIT_EXCEPTION;` |
|       - |  315 | `			}` |
|  505274 |  316 | `		}` |
|       - |  317 | `		/* Create a new class instance */` |
| 1117571 |  318 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
| 1117571 |  319 | `		if( pNew == 0 ){` |
|     ! 0 |  320 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  321 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|     ! 0 |  322 | `				&pClass->sName` |
|       - |  323 | `			);` |
|     ! 0 |  324 | `			PH7_MemObjRelease(pTos);` |
|     ! 0 |  325 | `			if( nCtorArgs > 0 ){` |
|       - |  326 | `				/* Pop given arguments */` |
|     ! 0 |  327 | `				VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  328 | `			}` |
|     ! 0 |  329 | `			VM_EXIT_BREAK;` |
|       - |  330 | `		}` |
| 1117571 |  331 | `		if( pVm->nBoundaryRc != 0 ){` |
|       - |  332 | `			/* A typed property DEFAULT failed its type check during instance-` |
|       - |  333 | `			 * frame creation (parked catchable TypeError): php aborts the` |
|       - |  334 | `			 * construction — no constructor call, no object. Consume the` |
|       - |  335 | `			 * parked status here and route exactly like a constructor throw` |
|       - |  336 | `			 * (the exception object is already registered). */` |
|      36 |  337 | `			sxi32 rcDef = pVm->nBoundaryRc;` |
|       - |  338 | `			sxi32 iDefResumePc;` |
|      36 |  339 | `			pVm->nBoundaryRc = 0;` |
|      36 |  340 | `			PH7_ClassInstanceUnref(pNew);` |
|      36 |  341 | `			if( rcDef == PH7_ABORT ){` |
|     ! 0 |  342 | `				VM_EXIT_ABORT;` |
|       - |  343 | `			}` |
|      36 |  344 | `			if( VmRecordedResume(pVm,&iDefResumePc,pState->pEntryFrame,aInstr) ){` |
|       - |  345 | `				/* This frame's own try caught it in-place: tidy the stack` |
|       - |  346 | `				 * (pop ctor args + release the class-name slot, then drain any` |
|       - |  347 | `				 * abandoned outer-expression operands to the try's base — the` |
|       - |  348 | `				 * class-name slot itself sits above it) and resume. */` |
|      36 |  349 | `				if( nCtorArgs > 0 ){` |
|       3 |  350 | `					VmPopOperand(&pTos,nCtorArgs);` |
|       1 |  351 | `				}` |
|      36 |  352 | `				PH7_MemObjRelease(pTos);` |
|      76 |  353 | `				PH7_RESUME_DRAIN()` |
|      36 |  354 | `				pc = iDefResumePc;` |
|      36 |  355 | `				VM_EXIT_BREAK;` |
|       - |  356 | `			}` |
|     ! 0 |  357 | `			VM_EXIT_EXCEPTION;` |
|       - |  358 | `		}` |
| 1117537 |  359 | `		if( pCons ){` |
|       - |  360 | `			/* Call the class constructor.  Collect args in stack order and` |
|       - |  361 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|       - |  362 | `			 * receiving OP_CALL path runs its named-argument matching` |
|       - |  363 | `			 * (including variadic string-key packing). */` |
| 1112661 |  364 | `			VmCallArgMap *pNewMap = pEffNewMap;` |
|       - |  365 | `			sxi32 rcCons;` |
|       - |  366 | `			SySet aArg; /* ctor argument scratch (the loop's shared set stays with OP_CALL) */` |
| 1112661 |  367 | `			SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
| 2125395 |  368 | `			while( pArg < pTos ){` |
| 1012739 |  369 | `				SySetPut(&aArg,(const void *)&pArg);` |
| 1012739 |  370 | `				pArg++;` |
|       5 |  371 | `			}` |
|       - |  372 | `			/* Too-few-arguments is php's catchable ArgumentCountError, raised` |
|       - |  373 | `			 * by the shared OP_CALL install path this ctor call routes through` |
|       - |  374 | `			 * (was a PHL-only notice here). */` |
| 1112661 |  375 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
| 1112661 |  376 | `			SySetRelease(&aArg); /* scratch dies here, on every exit path */` |
|       - |  377 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
| 1112661 |  378 | `			if( pNew->iRef < 1 ){` |
|     ! 0 |  379 | `				pNew->iRef = 1;` |
|     ! 0 |  380 | `			}` |
| 1112661 |  381 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|       - |  382 | `				/* The constructor raised: the half-constructed object must not` |
|       - |  383 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|       - |  384 | `				 * The class-name operand (and any leftover args) are released by` |
|       - |  385 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|       - |  386 | `				sxi32 iResumePc;` |
|  100793 |  387 | `				PH7_ClassInstanceUnref(pNew);` |
|  100793 |  388 | `				if( rcCons == PH7_ABORT ){` |
|       5 |  389 | `					VM_EXIT_ABORT;` |
|       - |  390 | `				}` |
|  100788 |  391 | `				if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){` |
|       - |  392 | `					/* This frame's own try caught it in-place: tidy the stack` |
|       - |  393 | `					 * (pop ctor args + release the class-name slot, then drain any` |
|       - |  394 | `					 * abandoned outer-expression operands to the try's base — the` |
|       - |  395 | `					 * class-name slot itself sits above it) and resume. */` |
|  100552 |  396 | `					if( nCtorArgs > 0 ){` |
|     544 |  397 | `						VmPopOperand(&pTos,nCtorArgs);` |
|     270 |  398 | `					}` |
|  100552 |  399 | `					PH7_MemObjRelease(pTos);` |
|  501142 |  400 | `					PH7_RESUME_DRAIN()` |
|  100552 |  401 | `					pc = iResumePc;` |
|  100552 |  402 | `					VM_EXIT_BREAK;` |
|       - |  403 | `				}` |
|     238 |  404 | `				VM_EXIT_EXCEPTION;` |
|       - |  405 | `			}` |
|  505934 |  406 | `		}` |
| 1016749 |  407 | `		if( nCtorArgs > 0 ){` |
|       - |  408 | `			/* Pop given arguments */` |
| 1009783 |  409 | `			VmPopOperand(&pTos,nCtorArgs);` |
|  504889 |  410 | `		}` |
| 1016749 |  411 | `		PH7_MemObjRelease(pTos);` |
| 1016749 |  412 | `		pTos->x.pOther = pNew;` |
| 1016749 |  413 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - |  414 | `	}` |
| 1016749 |  415 | `	VM_EXIT_BREAK;` |
|     ! 0 |  416 | `	VM_EXIT_BREAK;` |
| 1064103 |  417 | `}` |
|       - |  418 |  |
|       - |  419 | `/*` |
|       - |  420 | ` * Is this OP_MEMBER fetching the property for WRITING — php's BP_VAR_W / BP_VAR_RW` |
|       - |  421 | ` * fetch, the one that asks the object for something to MODIFY? The compiler tags` |
|       - |  422 | `` * the base of a subscript-write / `??=` PH7_MEMBER_WRITE, so that answers most of`` |
|       - |  423 | ``  * it once the shapes that share the tag are excluded: a plain store and a `??=` `` |
|       - |  424 | ` * write through their own paths, and a direct read-modify-write is the accessor` |
|       - |  425 | ` * pair (VmMagicRmwArm), not a write fetch. The two php compiles as plain READS —` |
|       - |  426 | `` * binding a reference (`$r = &$o->p`) and iterating by reference — are told apart`` |
|       - |  427 | `` * by the instruction that follows. `$o->p =& $x` is NOT one of them: the member is`` |
|       - |  428 | ` * the reference TARGET there and carries its own iP2.` |
|       - |  429 | ` */` |
|    5845 |  430 | `static int VmMemberFetchForWrite(const VmInstr *pInstr)` |
|       5 |  431 | `{` |
|    5850 |  432 | `	const VmInstr *pNext = pInstr + 1;` |
|    5850 |  433 | `	if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|      15 |  434 | `		int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);` |
|      15 |  435 | `		int bCoalesceW = (pNext->iOp == PH7_OP_NULLC_JMP);` |
|      15 |  436 | `		return !bPlainStore && !bCoalesceW && !VmMemberNextIsRmw(pNext);` |
|       - |  437 | `	}` |
|    5836 |  438 | `	if( pInstr->iP2 == PH7_MEMBER_READ ){` |
|    3594 |  439 | `		if( pNext->iOp == PH7_OP_STORE_REF ){` |
|       5 |  440 | `			return 1;` |
|       - |  441 | `		}` |
|    3590 |  442 | `		if( pNext->iOp == PH7_OP_FOREACH_INIT && pNext->p3 ){` |
|      39 |  443 | `			return (((ph7_foreach_info *)pNext->p3)->iFlags & PH7_4EACH_STEP_REF) != 0;` |
|       - |  444 | `		}` |
|    1774 |  445 | `	}` |
|    5794 |  446 | `	return 0;` |
|    2928 |  447 | `}` |
|       - |  448 | `/*` |
|       - |  449 | ` * A native class's property is a field of php's own C struct, and the only writes` |
|       - |  450 | ` * that reach one are the writes the store filter converts (ph7_class::xSet). So` |
|       - |  451 | ` * the fetched value keeps its slot index for exactly the shapes that end in such` |
|       - |  452 | `` * a store -- a plain assignment, `??=`, a destructuring target and the`` |
|       - |  453 | ` * read-modify-write forms -- and is a TEMPORARY for every other use. That is` |
|       - |  454 | ` * php's own answer: it has no ptr_ptr handler for such a property, so a reference` |
|       - |  455 | `` * bind (`$r = &$i->f`), a by-reference argument (`preg_match($p,$s,$i->s)`) and a`` |
|       - |  456 | ` * by-reference foreach all get a copy whose writes are SILENTLY lost -- silently,` |
|       - |  457 | ` * unlike the overloaded case, which php has a notice for.` |
|       - |  458 | ` */` |
|    1812 |  459 | `static int VmMemberNativeSetKeepsSlot(const VmInstr *pInstr)` |
|       3 |  460 | `{` |
|    1815 |  461 | `	if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|       - |  462 | ``		/* Every fetch the compiler tags for writing: a plain store, `??=`, a`` |
|       - |  463 | `		 * compound assign, and the base of a subscript write — that last one has` |
|       - |  464 | ``		 * to reach the real value so `$i->y[0] = 5` is php's "Cannot use a scalar`` |
|       - |  465 | `		 * value as an array" rather than a write nobody notices. */` |
|     615 |  466 | `		return 1;` |
|       - |  467 | `	}` |
|    1203 |  468 | `	if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       5 |  469 | `		return 1;   /* list()/foreach destructuring target */` |
|       - |  470 | `	}` |
|    1199 |  471 | `	return VmMemberNextIsWrite(pInstr + 1);   /* ++/-- and the compound assigns */` |
|     909 |  472 | `}` |
|       - |  473 | `/*` |
|       - |  474 | `` * php's `Indirect modification of overloaded property C::$p has no effect`: the`` |
|       - |  475 | ` * write-context fetch above landed on a property only __get answers for, so what` |
|       - |  476 | ` * comes back is a VALUE and whatever the rest of the expression writes into it is` |
|       - |  477 | ` * thrown away. php says so and carries on.` |
|       - |  478 | ` *` |
|       - |  479 | ` * PHL had the notice at one site only (a by-reference ARGUMENT, VmBindPropByRef)` |
|       - |  480 | ` * and, worse, the value was not a copy: __get's return still shared the object's` |
|       - |  481 | `` * own nested hashmap by COW, so `$o->a['b'] = 9`, `$o->a[] = 5` and a by-reference`` |
|       - |  482 | ` * foreach modified the object php leaves untouched. Separating it here is what` |
|       - |  483 | ` * makes the write land nowhere.` |
|       - |  484 | ` *` |
|       - |  485 | ` * An ENGINE __get is not this — php routes those through the class's own property` |
|       - |  486 | ` * handler, which hands back the real element (ArrayObject's ARRAY_AS_PROPS reads` |
|       - |  487 | ` * and writes through, in both engines). php stays silent for an OBJECT value too:` |
|       - |  488 | ` * a handle is shared, so nothing about the write is indirect.` |
|       - |  489 | ` */` |
|      30 |  490 | `PH7_PRIVATE void PH7_VmOverloadedPropNotice(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,ph7_value *pVal)` |
|       1 |  491 | `{` |
|      31 |  492 | `	ph7_class_method *pGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|      31 |  493 | `	if( pGet == 0 \|\| (pGet->sFunc.iFlags & VM_FUNC_NATIVE) ){` |
|     ! 0 |  494 | `		return;` |
|       - |  495 | `	}` |
|      31 |  496 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       3 |  497 | `		return;` |
|       - |  498 | `	}` |
|      43 |  499 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - |  500 | `		"Indirect modification of overloaded property %z::$%z has no effect",` |
|      14 |  501 | `		&pClass->sName,pName);` |
|      29 |  502 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      25 |  503 | `		PH7_HashmapCowSeparate(&(*pVm),pVal);` |
|      12 |  504 | `	}` |
|      16 |  505 | `}` |
|       - |  506 | `/*` |
|       - |  507 | ` * Does an OVERLOADED read-modify-write apply to this property access? php runs` |
|       - |  508 | ` * one only when the class answers BOTH sides; the guard means we are already` |
|       - |  509 | ` * inside this property's own __get, where php behaves as if the accessor were` |
|       - |  510 | ` * absent.` |
|       - |  511 | ` */` |
|      40 |  512 | `static int VmMagicRmwEligible(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,const SyString *pName)` |
|       2 |  513 | `{` |
|      56 |  514 | `	return PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) != 0` |
|      34 |  515 | `	    && PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1) != 0` |
|      54 |  516 | `	    && !VmMagicGuardHeld(pVm,(void *)pThis,pName,'g');` |
|       2 |  517 | `}` |
|       - |  518 | `/*` |
|       - |  519 | `` * php's read-modify-write of an OVERLOADED property — `$o->n++`, `--$o->n`,`` |
|       - |  520 | `` * `$o->n += 2`, `$o->n .= 'x'` on a name the instance does not expose. php reads`` |
|       - |  521 | ` * the current value through __get, lets the modify op compute on it, and writes` |
|       - |  522 | ` * the result back through __set; PHL fell through to dynamic-property creation` |
|       - |  523 | ` * instead, so the ordinary shape (a class declaring BOTH accessors) died on` |
|       - |  524 | `` * `Cannot create dynamic property C::$n` — or, when the name IS declared but`` |
|       - |  525 | `` * inaccessible from this scope, on `Cannot access private property C::$n` —`` |
|       - |  526 | ` * for a statement php runs.` |
|       - |  527 | ` *` |
|       - |  528 | ` * The write-back rides the same pending-entry rail property HOOKS use: the` |
|       - |  529 | ` * current value goes into a fresh SCRATCH memobj the modify op mutates in` |
|       - |  530 | ` * place, and the entry makes that op's tail (PH7_HOOK_RMW_WRITEBACK) dispatch` |
|       - |  531 | ` * __set with the computed value.` |
|       - |  532 | ` *` |
|       - |  533 | ` * BOTH accessors are required, which is php's own split: with only __get php` |
|       - |  534 | ` * goes on to CREATE the property (PHL's §10 policy refuses a dynamic property),` |
|       - |  535 | `` * and with only __set it warns `Undefined property` and reads null — neither is`` |
|       - |  536 | ` * this path.` |
|       - |  537 | ` *` |
|       - |  538 | ` * *pOut takes __get's value and *pnScratch the slot the modify op must address;` |
|       - |  539 | ` * the caller does its own stack surgery afterwards, because pName still aliases` |
|       - |  540 | `` * the NAME operand when the name is dynamic (`$o->$k++`) and releasing that`` |
|       - |  541 | ` * operand first would leave it dangling. *pnScratch stays SXU32_HIGH when __get` |
|       - |  542 | ` * threw (nothing is armed — the fetch-point router lands the parked throw and` |
|       - |  543 | ` * php's __set never runs) or when the scratch reservation failed.` |
|       - |  544 | ` */` |
|      28 |  545 | `static void VmMagicRmwArm(` |
|       - |  546 | `	ph7_vm *pVm,` |
|       - |  547 | `	ph7_class_instance *pThis,` |
|       - |  548 | `	ph7_class *pClass,` |
|       - |  549 | `	const SyString *pName,` |
|       - |  550 | `	ph7_value *pOut,` |
|       - |  551 | `	sxu32 *pnScratch,` |
|       - |  552 | `	void *pOwnerStack,` |
|       - |  553 | `	void *pInstrs,` |
|       - |  554 | `	sxu32 nPc` |
|       - |  555 | `	)` |
|       1 |  556 | `{` |
|       - |  557 | `	ph7_value *pScr;` |
|       - |  558 | `	VmHookRmw sRmw;` |
|      29 |  559 | `	*pnScratch = SXU32_HIGH;` |
|      29 |  560 | `	VmMagicGuardPush(pVm,(void *)pThis,pName,'g');` |
|      29 |  561 | `	PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,pName,pOut);` |
|      29 |  562 | `	VmMagicGuardPop(pVm);` |
|      29 |  563 | `	if( pVm->nBoundaryRc != 0 ){` |
|       3 |  564 | `		PH7_MemObjRelease(pOut);` |
|       3 |  565 | `		return;` |
|       - |  566 | `	}` |
|      27 |  567 | `	pScr = PH7_ReserveMemObj(&(*pVm));` |
|      27 |  568 | `	if( pScr == 0 ){` |
|       - |  569 | `		/* OOM: loud allocator diagnostics already fired; the value still stands. */` |
|     ! 0 |  570 | `		return;` |
|       - |  571 | `	}` |
|      27 |  572 | `	PH7_MemObjStore(pOut,pScr);` |
|      27 |  573 | `	sRmw.iKind = VM_HOOK_PEND_RMW_MAGIC;` |
|      27 |  574 | `	sRmw.pThis = pThis;` |
|      27 |  575 | `	sRmw.pAttr = 0;` |
|      27 |  576 | `	sRmw.nBackIdx = SXU32_HIGH;` |
|      27 |  577 | `	sRmw.nScratchIdx = pScr->nIdx;` |
|      27 |  578 | `	SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      27 |  579 | `	SyBlobAppend(&sRmw.sName,(const void *)pName->zString,pName->nByte);` |
|      27 |  580 | `	sRmw.pOwnerStack = pOwnerStack;` |
|      27 |  581 | `	sRmw.pInstrs = pInstrs;` |
|      27 |  582 | `	sRmw.nJmpPc = nPc;  /* the modify op ... */` |
|      27 |  583 | `	sRmw.nPc = nPc;     /* ... is the whole window */` |
|      27 |  584 | `	pThis->iRef++;` |
|      27 |  585 | `	SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      27 |  586 | `	*pnScratch = pScr->nIdx;` |
|      15 |  587 | `}` |
|       - |  588 | `/*` |
|       - |  589 | ` * OP_MEMBER: body moved verbatim from the OP_MEMBER arm of` |
|       - |  590 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  591 | ` */` |
|  367520 |  592 | `PH7_PRIVATE VmOpRc VmExecOpMember(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  593 | `{` |
|  367525 |  594 | `	ph7_value *pTos = pState->pTos;` |
|  367525 |  595 | `	ph7_value *pStack = pState->pStack;` |
|  367525 |  596 | `	VmInstr *aInstr = pState->aInstr;` |
|  367525 |  597 | `	sxi32 pc = pState->pc;` |
|       - |  598 | `	sxi32 rc;` |
|       - |  599 | `	ph7_class_instance *pThis;` |
|       - |  600 | `	ph7_value *pNos;` |
|       - |  601 | `	SyString sName;` |
|  367525 |  602 | `	if( !pInstr->iP1 ){` |
|  262021 |  603 | `		pNos = &pTos[-1];` |
|       - |  604 | `#ifdef UNTRUST` |
|       - |  605 | `		if( pNos < pStack ){` |
|       - |  606 | `			VM_EXIT_ABORT;` |
|       - |  607 | `		}` |
|       - |  608 | `#endif` |
|  262016 |  609 | `		if( pInstr->iP2 == PH7_MEMBER_DEFPATH` |
|  133489 |  610 | `		 && (pNos->iFlags & (MEMOBJ_AUX_DEFPATH\|MEMOBJ_AUX_DEFERRED)) ){` |
|       - |  611 | `			/* D1 commit 2: deferred property arg ($o->p) whose base is itself a deferred lvalue` |
|       - |  612 | ``			 * (a nested chain `$a["k"]->p`, or an undefined base object). Append a property step`` |
|       - |  613 | `			 * to the base's captured path; OP_CALL re-walks it. */` |
|       - |  614 | `			SyString sProp;` |
|     279 |  615 | `			VmDeferredPath *pPath = 0;` |
|     279 |  616 | `			SyStringInitFromBuf(&sProp,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     279 |  617 | `			if( pNos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|     277 |  618 | `				pPath = (VmDeferredPath *)pNos->x.pOther;` |
|     277 |  619 | `				VmDeferPathPushProp(pPath,&sProp);` |
|     140 |  620 | `			}else{` |
|       - |  621 | `				SyString sRootName;` |
|       3 |  622 | `				SyStringInitFromBuf(&sRootName,(const char *)pNos->x.pOther,` |
|       - |  623 | `					pNos->x.pOther ? SyStrlen((const char *)pNos->x.pOther) : 0);` |
|       3 |  624 | `				pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|       3 |  625 | `				if( pPath ){` |
|       3 |  626 | `					pNos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name */` |
|       3 |  627 | `					pNos->x.pOther = pPath;` |
|       3 |  628 | `					pNos->iFlags \|= MEMOBJ_AUX_DEFPATH;` |
|       3 |  629 | `					pNos->nIdx = SXU32_HIGH;` |
|       3 |  630 | `					VmDeferPathPushProp(pPath,&sProp);` |
|       1 |  631 | `				}` |
|       - |  632 | `			}` |
|     279 |  633 | `			VmPopOperand(&pTos,1); /* drop the property name; the carrier stays as pTos */` |
|     279 |  634 | `			VM_EXIT_BREAK;` |
|       - |  635 | `		}` |
|  261745 |  636 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - |  637 | `			ph7_class *pClass;` |
|       - |  638 | `			/* Class already instantiated */` |
|  261597 |  639 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       - |  640 | `			/* Point to the instantiated class */` |
|  261597 |  641 | `			pClass = pThis->pClass;` |
|       - |  642 | `			/* Extract attribute name first */` |
|  261597 |  643 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|  261597 |  644 | `			if( PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
|       - |  645 | `				/* __PHP_Incomplete_Class: every property access and method call is` |
|       - |  646 | `				 * php's incomplete-object diagnostic — the carrier's OWN entries are` |
|       - |  647 | `				 * for the engine's surfaces only. A READ (isset/empty/?? included)` |
|       - |  648 | `				 * is an E_WARNING answering NULL; every WRITE shape and unset() is` |
|       - |  649 | `				 * a catchable Error; a method call is its own Error, raised where` |
|       - |  650 | `				 * the undefined-method twin raises (before any argument runs). The` |
|       - |  651 | `				 * DEFPATH pre-pass defers like a MISSING property would — the slot` |
|       - |  652 | `				 * fast path must not hand out the carrier's storage — so the by-ref` |
|       - |  653 | `				 * resolve (VmBindPropByRef) raises the Error and the by-value` |
|       - |  654 | `				 * re-drive lands back here in READ context for the warning.` |
|       - |  655 | ``				 * `??=` reads before it refuses, so it takes BOTH, php's order. */`` |
|      37 |  656 | `				VmInstr *pIncNext = pInstr + 1;` |
|      37 |  657 | `				if( pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|       - |  658 | `					SyString sIncProp;` |
|       5 |  659 | `					VmDeferredPath *pIncPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|       5 |  660 | `					SyStringInitFromBuf(&sIncProp,sName.zString,sName.nByte);` |
|       5 |  661 | `					if( pIncPath && VmDeferPathPushProp(pIncPath,&sIncProp) == SXRET_OK ){` |
|       5 |  662 | `						VmPopOperand(&pTos,1);       /* drop the property name */` |
|       5 |  663 | `						pThis->iRef++;` |
|       5 |  664 | `						PH7_MemObjRelease(pTos);     /* collapse the object slot into the carrier */` |
|       5 |  665 | `						pTos->x.pOther = pIncPath;` |
|       5 |  666 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|       5 |  667 | `						pTos->nIdx = SXU32_HIGH;` |
|       5 |  668 | `						PH7_ClassInstanceUnref(pThis);` |
|       5 |  669 | `						VM_EXIT_BREAK;` |
|       - |  670 | `					}` |
|     ! 0 |  671 | `					if( pIncPath ){` |
|     ! 0 |  672 | `						VmFreeDeferredPath(pIncPath);` |
|     ! 0 |  673 | `					}` |
|       - |  674 | `					/* allocation failure (or a temporary base below): the read` |
|       - |  675 | `					 * verdict is all that is left — fall through to warn + NULL. */` |
|     ! 0 |  676 | `				}` |
|      33 |  677 | `				int bIncCall = (pInstr->iP2 == PH7_MEMBER_METHOD);` |
|      33 |  678 | `				int bIncCoalW = (pInstr->iP2 == PH7_MEMBER_WRITE && pIncNext->iOp == PH7_OP_NULLC_JMP);` |
|      65 |  679 | `				int bIncModify = pInstr->iP2 != PH7_MEMBER_DEFPATH` |
|      63 |  680 | `					&& (pInstr->iP2 == PH7_MEMBER_UNSET` |
|      31 |  681 | `					 \|\| pInstr->iP2 == PH7_MEMBER_WRITE` |
|      25 |  682 | `					 \|\| pInstr->iP2 == PH7_MEMBER_REF_TARGET` |
|      19 |  683 | `					 \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|      18 |  684 | `					 \|\| VmMemberNextIsWrite(pIncNext)` |
|      18 |  685 | `					 \|\| VmMemberFetchForWrite(pInstr));` |
|      33 |  686 | `				if( bIncCall ){` |
|       - |  687 | `					SyBlob sIncErr;` |
|       - |  688 | `					sxi32 rcInc;` |
|       3 |  689 | `					SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|       3 |  690 | `					PH7_VmIncompleteMsg(&(*pVm),pThis,"call a method",&sIncErr);` |
|       3 |  691 | `					VmPopOperand(&pTos,1);` |
|       3 |  692 | `					PH7_MemObjRelease(pTos);` |
|       3 |  693 | `					pTos->nIdx = SXU32_HIGH;` |
|       4 |  694 | `					rcInc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sIncErr),` |
|       1 |  695 | `						SyBlobLength(&sIncErr));` |
|       3 |  696 | `					SyBlobRelease(&sIncErr);` |
|       3 |  697 | `					if( rcInc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  698 | `					rc = rcInc;` |
|       3 |  699 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  700 | `				}` |
|      31 |  701 | `				if( bIncCoalW ){` |
|       3 |  702 | `					PH7_VmIncompleteAccessWarn(&(*pVm),pThis,0);` |
|       1 |  703 | `				}` |
|      31 |  704 | `				if( bIncModify ){` |
|       - |  705 | `					SyBlob sIncErr;` |
|      17 |  706 | `					SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|      17 |  707 | `					PH7_VmIncompleteMsg(&(*pVm),pThis,"modify a property",&sIncErr);` |
|      17 |  708 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sIncErr));` |
|       9 |  709 | `				}else{` |
|      15 |  710 | `					PH7_VmIncompleteAccessWarn(&(*pVm),pThis,0);` |
|       - |  711 | `				}` |
|      31 |  712 | `				VmPopOperand(&pTos,1);   /* pop the attribute name */` |
|      31 |  713 | `				PH7_MemObjRelease(pTos); /* the object slot becomes the NULL answer */` |
|      31 |  714 | `				pTos->nIdx = SXU32_HIGH;` |
|      31 |  715 | `				VM_EXIT_BREAK;` |
|       - |  716 | `			}` |
|  261561 |  717 | `			if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - |  718 | `				/* Method call */` |
|  144774 |  719 | `				ph7_class_method *pMeth = 0;` |
|  144774 |  720 | `				if( sName.nByte > 0 ){` |
|       - |  721 | `					/* Extract the target method */` |
|  144774 |  722 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|   72386 |  723 | `				}` |
|  144774 |  724 | `				if( pMeth == 0 ){` |
|      66 |  725 | `					ph7_class_method *pCallMagic = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|      66 |  726 | `					if( pCallMagic ){` |
|       - |  727 | `						/* php: a missing method dispatches __call($name, $args) — the` |
|       - |  728 | `						 * args are only collected by the following OP_CALL, so stash the` |
|       - |  729 | `						 * receiver + class + original name and MARK the callee slot` |
|       - |  730 | `						 * (band A #3b; pre-fix the name-only call discarded everything` |
|       - |  731 | `						 * and the call site failed with "Invalid function name"). Stack:` |
|       - |  732 | `						 * pop the method name, then the receiver slot becomes the marked` |
|       - |  733 | `						 * carrier OP_CALL routes through the packing body. */` |
|      56 |  734 | `						VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pThis,pClass,&sName);` |
|      56 |  735 | `						if( pPend == 0 ){` |
|     ! 0 |  736 | `							VM_EXIT_ABORT;` |
|       - |  737 | `						}` |
|      56 |  738 | `						VmPopOperand(&pTos,1);` |
|      56 |  739 | `						PH7_MemObjRelease(pTos);` |
|      56 |  740 | `						pTos->x.pOther = pPend;` |
|      56 |  741 | `						pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|      29 |  742 | `					}else{` |
|       - |  743 | `						{` |
|       - |  744 | `							/* php raises a catchable Error here; PH7 printed a notice and CARRIED ON` |
|       - |  745 | `							 * with NULL, so the call silently produced nothing. */` |
|       - |  746 | `							SyBlob sErrM;` |
|       - |  747 | `							sxi32 rcErr;` |
|      11 |  748 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      11 |  749 | `							SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",&pClass->sName,&sName);` |
|      11 |  750 | `							VmPopOperand(&pTos,1);` |
|      11 |  751 | `							PH7_MemObjRelease(pTos);` |
|      11 |  752 | `							pTos->nIdx = SXU32_HIGH;` |
|      16 |  753 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       5 |  754 | `								SyBlobLength(&sErrM));` |
|      11 |  755 | `							SyBlobRelease(&sErrM);` |
|      11 |  756 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 |  757 | `							rc = rcErr;` |
|      11 |  758 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  759 | `						}` |
|       - |  760 | `					}` |
|      29 |  761 | `				}else{` |
|  144710 |  762 | `					ph7_class_method *pDeniedCall = 0;` |
|  144710 |  763 | `					int bDenied = 0;` |
|       - |  764 | `					/* php decides a method's visibility against the class that OWNS it,` |
|       - |  765 | `					 * which for a trait method is the class that composed it — a trait is` |
|       - |  766 | `					 * a compile-time construct php has flattened away by now. Deciding` |
|       - |  767 | `					 * against the trait instead made every rule a question of who USES it:` |
|       - |  768 | `					 * a protected trait method was denied to a SUBCLASS of the composing` |
|       - |  769 | `					 * class (not a trait user itself) and granted to an unrelated class` |
|       - |  770 | `					 * that happened to use the same trait. */` |
|  144710 |  771 | `					ph7_class *pOwner = 0;` |
|  144705 |  772 | `					if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC` |
|   72416 |  773 | `					 && !PH7_VmClassMemberAccess(&(*pVm),` |
|     114 |  774 | `						(pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth)),` |
|      57 |  775 | `						&sName,pMeth->iProtection,FALSE) ){` |
|      56 |  776 | `						int bRebound = 0;` |
|      56 |  777 | `						if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - |  778 | `							/* php: when the instance's class SHADOWS the caller's` |
|       - |  779 | `							 * private method (child redeclares private m), code in` |
|       - |  780 | `							 * the caller's class dispatches its OWN private m, not` |
|       - |  781 | `							 * the shadow. Rebind to the calling scope's method when` |
|       - |  782 | `							 * that scope declares a private one of this name. */` |
|      48 |  783 | `							VmFrame *pFrameLocal = VmSkipExceptionFrames(pVm->pFrame);` |
|      48 |  784 | `							ph7_vm_func *pCallerFunc = (ph7_vm_func *)pFrameLocal->pUserData;` |
|      48 |  785 | `							if( pCallerFunc && (pCallerFunc->iFlags & VM_FUNC_CLASS_METHOD) && pCallerFunc->pUserData ){` |
|       6 |  786 | `								ph7_class *pScope = (ph7_class *)pCallerFunc->pUserData;` |
|       6 |  787 | `								ph7_class_method *pOwn = PH7_ClassExtractMethod(pScope,sName.zString,sName.nByte);` |
|       4 |  788 | `								if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       6 |  789 | `								 && (ph7_class *)pOwn->sFunc.pUserData == pScope ){` |
|       6 |  790 | `									pMeth = pOwn;` |
|       6 |  791 | `									bRebound = 1;` |
|       2 |  792 | `								}` |
|       2 |  793 | `							}` |
|      22 |  794 | `						}` |
|      56 |  795 | `						if( !bRebound ){` |
|       - |  796 | `							/* Inaccessible from this scope: php routes through __call when` |
|       - |  797 | `							 * declared (band A #3b); without it the refusal is raised right` |
|       - |  798 | `							 * here, beside the undefined-method and abstract-method twins. */` |
|      52 |  799 | `							pDeniedCall = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|      52 |  800 | `							bDenied = pDeniedCall == 0;` |
|      24 |  801 | `						}` |
|      26 |  802 | `					}` |
|  144710 |  803 | `					if( bDenied ){` |
|       - |  804 | ``						/* php's visibility Error for `$o->m()`. It is raised HERE, at the`` |
|       - |  805 | `						 * member resolution, and not left to OP_CALL: the method OP_CALL` |
|       - |  806 | `						 * would re-derive is looked up by the FUNCTION's own name against` |
|       - |  807 | `						 * its declaring class, which a trait adaptation splits away from` |
|       - |  808 | ``						 * the entry this lookup actually chose — `hi as private pHi` gave`` |
|       - |  809 | ``						 * OP_CALL the trait's public `hi`, so the private alias ran from`` |
|       - |  810 | `						 * global scope in silence. The entry that answered is right here,` |
|       - |  811 | `						 * with its composed protection.` |
|       - |  812 | `						 *` |
|       - |  813 | ``						 * php names the method as the CALL SPELLS it (`$o->PQ()` on a`` |
|       - |  814 | ``						 * private `pQ()` reports `PQ`) and the class that OWNS the method,`` |
|       - |  815 | `						 * which for a trait method is the composing class. */` |
|       - |  816 | `						SyBlob sErrM;` |
|       - |  817 | `						sxi32 rcErr;` |
|      42 |  818 | `						ph7_class *pScope = PH7_VmCallerScopeName(&(*pVm));` |
|      61 |  819 | `						const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      19 |  820 | `							? "private" : "protected";` |
|      42 |  821 | `						SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      42 |  822 | `						if( pScope ){` |
|       7 |  823 | `							SyBlobFormat(&sErrM,"Call to %s method %z::%z() from scope %z",` |
|       3 |  824 | `								zVis,&pOwner->sName,&sName,&pScope->sName);` |
|       4 |  825 | `						}else{` |
|      36 |  826 | `							SyBlobFormat(&sErrM,"Call to %s method %z::%z() from global scope",` |
|      16 |  827 | `								zVis,&pOwner->sName,&sName);` |
|       - |  828 | `						}` |
|      42 |  829 | `						VmPopOperand(&pTos,1);` |
|      42 |  830 | `						PH7_MemObjRelease(pTos);` |
|      42 |  831 | `						pTos->nIdx = SXU32_HIGH;` |
|      61 |  832 | `						rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      19 |  833 | `							SyBlobLength(&sErrM));` |
|      42 |  834 | `						SyBlobRelease(&sErrM);` |
|      42 |  835 | `						if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      42 |  836 | `						rc = rcErr;` |
|      52 |  837 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  838 | `					}` |
|  144672 |  839 | `					if( pDeniedCall ){` |
|      11 |  840 | `						VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pThis,pClass,&sName);` |
|      11 |  841 | `						if( pPend == 0 ){` |
|     ! 0 |  842 | `							VM_EXIT_ABORT;` |
|       - |  843 | `						}` |
|      11 |  844 | `						VmPopOperand(&pTos,1);` |
|      11 |  845 | `						PH7_MemObjRelease(pTos);` |
|      11 |  846 | `						pTos->x.pOther = pPend;` |
|      11 |  847 | `						pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|       6 |  848 | `					}else{` |
|       - |  849 | `						/* Push method name on the stack, MARKED as already screened: the` |
|       - |  850 | `						 * decision above was made against the entry this lookup chose,` |
|       - |  851 | `						 * which is the only place a trait adaptation's composed` |
|       - |  852 | `						 * protection is visible. */` |
|  144662 |  853 | `						PH7_MemObjRelease(pTos);` |
|  144662 |  854 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|  144662 |  855 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|  144662 |  856 | `						pTos->iFlags \|= MEMOBJ_AUX_MEMBERCALL;` |
|       - |  857 | `					}` |
|       - |  858 | `				}` |
|  144726 |  859 | `				pTos->nIdx = SXU32_HIGH;` |
|   72367 |  860 | `			}else{` |
|       - |  861 | `				/* Attribute access. iP2: 0 = read, 2 = unset, 3 = isset, 4 = empty. */` |
|  116792 |  862 | `				VmClassAttr *pObjAttr = 0;` |
|  116792 |  863 | `				SyHashEntry *pEntry = 0;` |
|       - |  864 | `				/* A LAZY native property read before anything installed it, whose` |
|       - |  865 | `				 * class answers such a read from its zeroed struct rather than` |
|       - |  866 | `				 * calling the name undefined (PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT).` |
|       - |  867 | `				 * Set by the miss handling below and answered after the pop. */` |
|  116792 |  868 | `				ph7_class_attr *pLazyDefault = 0;` |
|  116787 |  869 | `				if( sName.nByte > 0 && sName.zString[0] == 0` |
|   58420 |  870 | `				 && !PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
|       - |  871 | `					/* php refuses a property name beginning with a NUL outright:` |
|       - |  872 | ``					 * `Cannot access property starting with "\0"`. Those bytes are`` |
|       - |  873 | `					 * the ENGINE's private mangling — "\0Cls\0p" is C1::$p and` |
|       - |  874 | `					 * "\0*\0p" is a protected slot — so a script that could write one` |
|       - |  875 | `					 * would forge a private member of any class it names, and every` |
|       - |  876 | `					 * display surface would then render the forgery as the real thing.` |
|       - |  877 | `					 * Two rules ride with it, both probe-verified: a MAGIC accessor` |
|       - |  878 | `					 * wins (php dispatches __get/__set/__isset/__unset with the raw` |
|       - |  879 | `					 * name and says nothing), and a LOOKUP context is silent — isset()` |
|       - |  880 | ``					 * is false, empty() true, `??` takes the default — which is what`` |
|       - |  881 | `					 * the existing miss handling below already answers. The refusal` |
|       - |  882 | `					 * does not depend on whether such a property exists: php raises it` |
|       - |  883 | `					 * on the NAME, before any lookup, and the one place a NUL-keyed` |
|       - |  884 | `					 * property legitimately lives is the __PHP_Incomplete_Class` |
|       - |  885 | `					 * carrier, whose own gate above has already answered for it.` |
|       - |  886 | ``					 * A METHOD name is not covered — php lets `$o->{"\0m"}()` reach`` |
|       - |  887 | `					 * the ordinary undefined-method Error — so this sits in the` |
|       - |  888 | `					 * attribute branch only. */` |
|       - |  889 | `					const char *zNulMagic;` |
|      51 |  890 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       5 |  891 | `						zNulMagic = "__unset";` |
|      49 |  892 | `					}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|      46 |  893 | `					       \|\| VmMemberNextIsWrite(pInstr + 1) ){` |
|      19 |  894 | `						zNulMagic = "__set";` |
|      10 |  895 | `					}else{` |
|      29 |  896 | `						zNulMagic = "__get";` |
|       - |  897 | `					}` |
|      50 |  898 | `					if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      47 |  899 | `					 && PH7_ClassExtractMethod(pClass,zNulMagic,(sxu32)SyStrlen(zNulMagic)) == 0 ){` |
|       - |  900 | `						SyBlob sNulErr;` |
|       - |  901 | `						sxi32 rcNul;` |
|      33 |  902 | `						SyBlobInit(&sNulErr,&pVm->sAllocator);` |
|      33 |  903 | `						SyBlobAppend(&sNulErr,"Cannot access property starting with \"\\0\"",` |
|       - |  904 | `							sizeof("Cannot access property starting with \"\\0\"")-1);` |
|      33 |  905 | `						VmPopOperand(&pTos,1);   /* pop the attribute name */` |
|      33 |  906 | `						PH7_MemObjRelease(pTos); /* the object slot becomes the NULL answer */` |
|      33 |  907 | `						pTos->nIdx = SXU32_HIGH;` |
|      49 |  908 | `						rcNul = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sNulErr),` |
|      16 |  909 | `							SyBlobLength(&sNulErr));` |
|      33 |  910 | `						SyBlobRelease(&sNulErr);` |
|      33 |  911 | `						if( rcNul == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      33 |  912 | `						rc = rcNul;` |
|      33 |  913 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  914 | `					}` |
|       9 |  915 | `				}` |
|       - |  916 | `				/* Extract the target attribute. The EMPTY name is a real property` |
|       - |  917 | ``				 * name in php — `$o->{''} = 1` creates one and `$o->{''}` reads it`` |
|       - |  918 | `				 * back — and it is the one name SyHashGet cannot answer for, so it` |
|       - |  919 | `				 * takes the list-walking lookup; every other name keeps the direct` |
|       - |  920 | `				 * hash probe this path has always made. */` |
|  116760 |  921 | `				if( sName.nByte > 0 ){` |
|  116752 |  922 | `					pEntry = SyHashGet(&pThis->hAttr,(const void *)sName.zString,sName.nByte);` |
|   58379 |  923 | `				}else{` |
|       9 |  924 | `					pEntry = PH7_ClassInstanceAttrEntry(pThis,sName.zString,0);` |
|       - |  925 | `				}` |
|  116760 |  926 | `				if( pEntry ){` |
|       - |  927 | `					/* Point to the attribute value */` |
|  109887 |  928 | `					pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|   54941 |  929 | `				}` |
|  116755 |  930 | `				if( pObjAttr` |
|  113319 |  931 | `				 && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT))` |
|   54956 |  932 | `				 && PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,` |
|      20 |  933 | `					pObjAttr->pAttr->iProtection,FALSE) ){` |
|       - |  934 | `					/* A static property (or a constant) belongs to the CLASS: php does` |
|       - |  935 | `					 * not find it through an instance at all. It notices the attempt —` |
|       - |  936 | ``					 * `Accessing static property C::$s as non static` — and then treats`` |
|       - |  937 | `					 * the name as an ordinary MISSING property: a read warns and answers` |
|       - |  938 | `					 * null, isset() is false, a write goes to a dynamic property (which` |
|       - |  939 | `					 * PHL rejects by the §10 policy, like any other undeclared write).` |
|       - |  940 | `					 * PHL's instance table carries an entry for every declared member` |
|       - |  941 | ``					 * (statics share the class slot), so `$o->s` used to READ and — far`` |
|       - |  942 | `					 * worse — WRITE the class's own static in silence. isset()/empty()` |
|       - |  943 | `					 * pass silently, as php's do. */` |
|      18 |  944 | `					if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      18 |  945 | `					 && pInstr->iP2 != PH7_MEMBER_DEFPATH ){` |
|       - |  946 | `						/* Silent where php is silent: isset()/empty(), and any class` |
|       - |  947 | `						 * that declares the MAGIC accessor this context would dispatch` |
|       - |  948 | ``						 * (php passes `silent = ce->__get/__set/__unset != NULL` to its`` |
|       - |  949 | `						 * property-offset lookup). Once per access, too: the deferred` |
|       - |  950 | `						 * call-argument PRE-PASS (PH7_MEMBER_DEFPATH) re-runs this op for` |
|       - |  951 | ``						 * the same source `$o->s` and the value pass carries the notice`` |
|       - |  952 | `						 * — the BY-REF form has no value pass and notices at its own` |
|       - |  953 | `						 * site (VmBindPropByRef). */` |
|       - |  954 | `						const char *zMagic;` |
|      10 |  955 | `						if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       3 |  956 | `							zMagic = "__unset";` |
|       8 |  957 | `						}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|       8 |  958 | `						       \|\| VmMemberNextIsWrite(pInstr + 1) ){` |
|       3 |  959 | `							zMagic = "__set";` |
|       2 |  960 | `						}else{` |
|       6 |  961 | `							zMagic = "__get";` |
|       - |  962 | `						}` |
|      10 |  963 | `						if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) == 0 ){` |
|      11 |  964 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - |  965 | `								"Accessing static property %z::$%z as non static",` |
|       6 |  966 | `								&pClass->sName,&pObjAttr->pAttr->sName);` |
|       3 |  967 | `						}` |
|       4 |  968 | `					}` |
|      20 |  969 | `					pEntry = 0;` |
|      20 |  970 | `					pObjAttr = 0;` |
|       9 |  971 | `				}` |
|  116760 |  972 | `				if( pObjAttr == 0 && PH7_ClassHasNativeProp(pClass) ){` |
|       - |  973 | `					/* php's read_property / has_property / write_property /` |
|       - |  974 | `					 * unset_property handlers, for a class whose properties are not` |
|       - |  975 | `					 * storage at all: PDORow answers every read from the statement's` |
|       - |  976 | `					 * current ROW, holds no slot for any of them, and refuses every` |
|       - |  977 | `					 * write. It comes first among the miss paths, and before the` |
|       - |  978 | `					 * declared-but-absent one: a name this class owns is neither a` |
|       - |  979 | `					 * dynamic property to create, nor an "Undefined property" to warn` |
|       - |  980 | `					 * about, nor a __get to dispatch.` |
|       - |  981 | `					 *` |
|       - |  982 | `					 * Which handler php asks is what the CONTEXT says: a plain store,` |
|       - |  983 | `					 * a destructuring target and a read-modify-write all end in a` |
|       - |  984 | ``					 * write; `??=` reads first and writes only when the read answered`` |
|       - |  985 | ``					 * null; a subscript-write base (`$o->p[0] = 1`) is a READ whose`` |
|       - |  986 | `					 * value the subscript then refuses; and isset() stops at the` |
|       - |  987 | ``					 * truth while empty() and `??` take the value. */`` |
|     147 |  988 | `					VmInstr *pPropNext = pInstr + 1;` |
|     147 |  989 | `					int bPropStore = (pPropNext->iOp == PH7_OP_STORE && pPropNext->iP2 != 0);` |
|     227 |  990 | `					int bPropCoal = (pInstr->iP2 == PH7_MEMBER_WRITE` |
|     146 |  991 | `						&& pPropNext->iOp == PH7_OP_NULLC_JMP);` |
|       - |  992 | `					PH7_NativePropCtx sProp;` |
|       - |  993 | `					ph7_value sPropVal;` |
|     147 |  994 | `					PH7_MemObjInit(&(*pVm),&sPropVal);` |
|     147 |  995 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       5 |  996 | `						sProp.iMode = PH7_NATIVE_PROP_UNSET;` |
|     145 |  997 | `					}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET \|\| bPropStore` |
|     139 |  998 | `					       \|\| VmMemberNextIsRmw(pPropNext) ){` |
|      11 |  999 | `						sProp.iMode = PH7_NATIVE_PROP_WRITE;` |
|     138 | 1000 | `					}else if( pInstr->iP2 == PH7_MEMBER_ISSET ){` |
|      25 | 1001 | `						sProp.iMode = PH7_NATIVE_PROP_ISSET;` |
|     121 | 1002 | `					}else if( pInstr->iP2 == PH7_MEMBER_EMPTY ){` |
|       - | 1003 | `						/* php's empty() asks has_property with a non-zero` |
|       - | 1004 | `						 * check_empty and takes THAT for the answer -- it never` |
|       - | 1005 | `						 * reads the value. The two are not the same question:` |
|       - | 1006 | ``						 * `empty($row->queryString)` is TRUE on a name the`` |
|       - | 1007 | `						 * handler does not know, while reading it works. */` |
|      17 | 1008 | `						sProp.iMode = PH7_NATIVE_PROP_EXISTS;` |
|       9 | 1009 | `					}else{` |
|      93 | 1010 | `						sProp.iMode = PH7_NATIVE_PROP_READ;` |
|       - | 1011 | `					}` |
|     147 | 1012 | `					sProp.pName = &sName;` |
|     147 | 1013 | `					sProp.pResult = &sPropVal;` |
|     147 | 1014 | `					sProp.bAnswered = 0;` |
|     147 | 1015 | `					sProp.zThrowClass = 0;` |
|     147 | 1016 | `					sProp.zThrowMsg[0] = 0;` |
|     147 | 1017 | `					if( PH7_ClassNativeProp(pThis,&sProp) ){` |
|     146 | 1018 | `						if( sProp.zThrowClass == 0 && bPropCoal` |
|      68 | 1019 | `						 && sProp.iMode == PH7_NATIVE_PROP_READ` |
|       5 | 1020 | `						 && (sPropVal.iFlags & MEMOBJ_NULL) ){` |
|       - | 1021 | ``							/* `$o->p ??= v` on a name that reads null: the store php`` |
|       - | 1022 | `							 * skips for a non-null one is the one it now makes, so the` |
|       - | 1023 | `							 * refusal is the WRITE handler's. */` |
|       3 | 1024 | `							sProp.iMode = PH7_NATIVE_PROP_WRITE;` |
|       3 | 1025 | `							sProp.bAnswered = 0;` |
|       3 | 1026 | `							PH7_ClassNativeProp(pThis,&sProp);` |
|       1 | 1027 | `						}` |
|     147 | 1028 | `						if( sProp.zThrowClass ){` |
|       - | 1029 | `							/* Parked, like every other refusal this op makes: the` |
|       - | 1030 | `							 * fetch-point router lands it and abandons this slot. */` |
|      25 | 1031 | `							VmBoundaryPark(&(*pVm),` |
|       8 | 1032 | `								VmThrowFixedError(&(*pVm),sProp.zThrowClass,sProp.zThrowMsg));` |
|      17 | 1033 | `							PH7_MemObjRelease(&sPropVal);` |
|      17 | 1034 | `							VmPopOperand(&pTos,1);      /* the property name */` |
|      17 | 1035 | `							PH7_MemObjRelease(pTos);    /* the object slot is the answer */` |
|      17 | 1036 | `							pTos->nIdx = SXU32_HIGH;` |
|      82 | 1037 | `							VM_EXIT_BREAK;` |
|       - | 1038 | `						}` |
|     131 | 1039 | `						VmPopOperand(&pTos,1);          /* the property name */` |
|     131 | 1040 | `						pThis->iRef++;` |
|     131 | 1041 | `						PH7_MemObjRelease(pTos);` |
|     131 | 1042 | `						if( sProp.iMode == PH7_NATIVE_PROP_ISSET ){` |
|       - | 1043 | `							/* isset() only tests null-ness: a non-null marker for` |
|       - | 1044 | `							 * true, NULL for false — what the __isset arm pushes. */` |
|      25 | 1045 | `							if( ph7_value_to_bool(&sPropVal) ){` |
|      15 | 1046 | `								pTos->x.iVal = 1;` |
|      15 | 1047 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       8 | 1048 | `							}` |
|     119 | 1049 | `						}else if( sProp.iMode != PH7_NATIVE_PROP_UNSET ){` |
|       - | 1050 | `							/* EMPTY pushes the handler's BOOL, which the op then` |
|       - | 1051 | `							 * judges for emptiness -- the same answer php takes. */` |
|     107 | 1052 | `							PH7_MemObjStore(&sPropVal,pTos);` |
|      53 | 1053 | `						}` |
|     131 | 1054 | `						pTos->nIdx = SXU32_HIGH;   /* a value, never an lvalue */` |
|     131 | 1055 | `						PH7_MemObjRelease(&sPropVal);` |
|     131 | 1056 | `						PH7_ClassInstanceUnref(pThis);` |
|     131 | 1057 | `						VM_EXIT_BREAK;` |
|       - | 1058 | `					}` |
|     ! 0 | 1059 | `					PH7_MemObjRelease(&sPropVal);` |
|     ! 0 | 1060 | `				}` |
|  116614 | 1061 | `				if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - | 1062 | `					/* unset($o->prop): remove the property entirely so it disappears from` |
|       - | 1063 | `					 * foreach / json_encode / get_object_vars / (array) — matching PHP (a value-only` |
|       - | 1064 | `					 * release would leave a zombie null entry). Leave a NULL constant on the stack so` |
|       - | 1065 | `					 * the trailing generic unset() builtin is a no-op (mirrors LOAD_IDX iP2=5).` |
|       - | 1066 | `					 * php dispatches __unset($name) for a MISSING or INACCESSIBLE property` |
|       - | 1067 | `					 * (band A #3b — pre-fix an inaccessible private was silently DELETED from` |
|       - | 1068 | `					 * outside the class); without __unset, an inaccessible unset is php's` |
|       - | 1069 | `					 * catchable "Cannot access ..." Error and a missing one stays a no-op. */` |
|     120 | 1070 | `					int bUnsAccessible = pEntry ? PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) : 0;` |
|     172 | 1071 | `					ph7_class_attr *pUnsNoWrite = pEntry ? pObjAttr->pAttr` |
|      64 | 1072 | `						: PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte);` |
|     120 | 1073 | `					sxi32 rcUnsRo = SXRET_OK;` |
|     120 | 1074 | `					if( pEntry && (pObjAttr->iState & VM_CLASS_ATTR_RDONLY) != 0 ){` |
|       - | 1075 | `						/* php's read-only handler answers an unset with the same sentence` |
|       - | 1076 | ``						 * it answers a store: `Property p is read only`. */`` |
|       3 | 1077 | `						VmBoundaryPark(&(*pVm),VmThrowNativeReadOnly(&(*pVm),pObjAttr->pAttr));` |
|     119 | 1078 | `					}else if( pUnsNoWrite && (pUnsNoWrite->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE) != 0 ){` |
|       - | 1079 | `						/* php's own unset handler for this class refuses, and words it` |
|       - | 1080 | ``						 * without either "readonly" or "property": `Cannot unset C::$p`.`` |
|       - | 1081 | `						 * It runs whether the object has a struct or not, so an` |
|       - | 1082 | `						 * unconstructed one -- which holds no slot at all -- refuses too` |
|       - | 1083 | `						 * rather than falling through to the missing-property no-op. */` |
|      28 | 1084 | `						VmBoundaryPark(&(*pVm),` |
|       9 | 1085 | `							VmThrowNativeNoUnset(&(*pVm),pThis->pClass,pUnsNoWrite));` |
|     109 | 1086 | `					}else if( pEntry && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) != 0 ){` |
|       - | 1087 | `						/* A native class's property is php's own C struct field, and` |
|       - | 1088 | `						 * unset() is a std handler that looks for a REAL property and` |
|       - | 1089 | `						 * finds none: nothing happens, nothing is said, and the next` |
|       - | 1090 | `						 * read still answers the struct. Removing the slot here left` |
|       - | 1091 | ``						 * `unset($i->y); $i->y` an Undefined property warning and NULL`` |
|       - | 1092 | `						 * for a statement php ignores. */` |
|     100 | 1093 | `					}else if( pEntry && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) != 0 ){` |
|       - | 1094 | `						/* php 8.4: a hooked property (virtual or backed) can never be` |
|       - | 1095 | `						 * unset — catchable Error, even from inside its own hook body` |
|       - | 1096 | `						 * (probe-verified). */` |
|       - | 1097 | `						SyBlob sErrMsg;` |
|       5 | 1098 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       5 | 1099 | `						SyBlobFormat(&sErrMsg,"Cannot unset hooked property %z::$%z",` |
|       4 | 1100 | `							&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       5 | 1101 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      91 | 1102 | `					}else if( pEntry && bUnsAccessible` |
|      76 | 1103 | `					       && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0` |
|      53 | 1104 | `					       && (rcUnsRo = VmCheckReadonlyUnset(&(*pVm),pThis->pClass,pObjAttr)) != SXRET_OK ){` |
|       - | 1105 | `						/* php refuses to destroy a readonly property: an initialized` |
|       - | 1106 | `						 * one from every scope, and an uninitialized one from a scope` |
|       - | 1107 | `						 * that may not write it. Deleting it here re-armed the` |
|       - | 1108 | ``						 * write-once latch, so a `readonly` value could be replaced by`` |
|       - | 1109 | `						 * anything in two statements. The refusal is thrown inside the` |
|       - | 1110 | `						 * check; parking it is what routes it like every other one` |
|       - | 1111 | `						 * raised from this opcode. */` |
|      17 | 1112 | `						VmBoundaryPark(&(*pVm),rcUnsRo);` |
|      84 | 1113 | `					}else if( pEntry && bUnsAccessible ){` |
|      58 | 1114 | `						if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|      40 | 1115 | `						 && (pObjAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){` |
|       - | 1116 | `							/* A TYPED property keeps its DECLARATION: php's unset makes` |
|       - | 1117 | `							 * it uninitialized, so var_dump still names it` |
|       - | 1118 | ``							 * `uninitialized(T)`, a read is "must not be accessed before`` |
|       - | 1119 | `							 * initialization" rather than an Undefined property, and a` |
|       - | 1120 | `							 * later write lands back in its declared position instead of` |
|       - | 1121 | `							 * appending a dynamic one at the end. The seven other` |
|       - | 1122 | `							 * presentation surfaces leave an uninitialized property out,` |
|       - | 1123 | `							 * which is what made the delete look right. */` |
|      20 | 1124 | `							ph7_value *pUnsSlot = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|      20 | 1125 | `							if( pUnsSlot ){` |
|      20 | 1126 | `								PH7_MemObjRelease(pUnsSlot);` |
|      20 | 1127 | `								MemObjSetType(pUnsSlot,MEMOBJ_NULL);` |
|       9 | 1128 | `							}` |
|      20 | 1129 | `							pObjAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|      11 | 1130 | `						}else{` |
|      42 | 1131 | `							PH7_VmReleaseInstanceAttr(&(*pVm),pObjAttr);` |
|      42 | 1132 | `							SyHashDeleteEntry2(pEntry);` |
|       - | 1133 | `						}` |
|      31 | 1134 | `					}else{` |
|      17 | 1135 | `						ph7_class_method *pUnsetMagic = PH7_ClassExtractMethod(pClass,"__unset",sizeof("__unset")-1);` |
|      17 | 1136 | `						if( pUnsetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'u') ){` |
|      15 | 1137 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'u');` |
|      15 | 1138 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__unset",sizeof("__unset")-1,&sName,0);` |
|      15 | 1139 | `							VmMagicGuardPop(pVm);` |
|       9 | 1140 | `						}else if( pEntry ){` |
|       - | 1141 | `							/* Inaccessible and no __unset: php's catchable Error. Parked on` |
|       - | 1142 | `							 * the boundary rail; the op completes benignly and the` |
|       - | 1143 | `							 * fetch-point router lands it. */` |
|       - | 1144 | `							SyBlob sErrMsg;` |
|       3 | 1145 | `							const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       3 | 1146 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 1147 | `							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",zVis,&pClass->sName,&sName);` |
|       3 | 1148 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       1 | 1149 | `						}` |
|       - | 1150 | `						/* Missing property without __unset: silent no-op (php). */` |
|       - | 1151 | `					}` |
|     120 | 1152 | `					VmPopOperand(&pTos,1);    /* pop the attribute name */` |
|     120 | 1153 | `					PH7_MemObjRelease(pTos);  /* release the object on the stack ($o's stack ref) */` |
|     120 | 1154 | `					pTos->nIdx = SXU32_HIGH;  /* NULL constant */` |
|     120 | 1155 | `					VM_EXIT_BREAK;` |
|       - | 1156 | `				}` |
|  116498 | 1157 | `				if( pObjAttr == 0 ){` |
|       - | 1158 | `					/* Member not present on the instance and the next instruction writes/modifies it` |
|       - | 1159 | ``					 * (store, array-append/keyed-write, `??=`, ++/--, or a compound-assign — see`` |
|       - | 1160 | `					 * VmMemberNextIsWrite; the compiler always emits a terminating PH7_OP_DONE so` |
|       - | 1161 | `					 * pInstr+1 is in-bounds). Create the property so the operation lands on a real slot` |
|       - | 1162 | ``					 * (PHP auto-vivifies a fresh property for all these forms, not just `=`):`` |
|       - | 1163 | `					 *   - a DECLARED prop that was unset() and is re-assigned → recreate it` |
|       - | 1164 | `					 *     (PHP re-appends it at the end), OR` |
|       - | 1165 | `					 *   - a dynamic prop on a dynamic-allowing class (stdClass).` |
|       - | 1166 | `					 * The created slot is NULL with a real nIdx, which the modify-op then vivifies` |
|       - | 1167 | `					 * (NULL→array for [], NULL→0/"" for ++/.=) and writes back in place.` |
|       - | 1168 | `					 * Two signals: the compiler tags the member itself iP2=PH7_MEMBER_WRITE when it is` |
|       - | 1169 | ``					 * the base of a write-subscript / `??=` (the modify-op is not the immediately-next`` |
|       - | 1170 | `					 * instruction there), and for ++/--/compound-assign/store the next opcode is the` |
|       - | 1171 | `					 * modify-op directly (VmMemberNextIsWrite). */` |
|    6738 | 1172 | `					VmInstr *pNext = pInstr + 1;` |
|    6733 | 1173 | `					if( pInstr->iP2 == PH7_MEMBER_WRITE \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|    6167 | 1174 | `					 \|\| VmMemberNextIsWrite(pNext) ){` |
|     577 | 1175 | `						ph7_class_attr *pDecl = PH7_ClassExtractAttribute(pThis->pClass,sName.zString,sName.nByte);` |
|     577 | 1176 | `						if( pDecl && PH7_ATTR_LAZY_ABSENT(pDecl,pThis) ){` |
|       - | 1177 | `							/* The object has never held this name. A class whose handler` |
|       - | 1178 | `							 * REFUSES every write answers the same sentence with or` |
|       - | 1179 | `							 * without a struct, and nothing is created; otherwise php's` |
|       - | 1180 | `							 * own write goes to the standard handler and CREATES a` |
|       - | 1181 | `							 * dynamic property beside the struct, which PHL refuses` |
|       - | 1182 | `							 * (§10) -- so fall through to the dynamic branch and let it.` |
|       - | 1183 | `` 							 * Once the constructor has installed the set, an `unset()` `` |
|       - | 1184 | `							 * and a re-write are the ordinary declared path again. */` |
|      16 | 1185 | `							if( pDecl->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|       4 | 1186 | `								VmBoundaryPark(&(*pVm),` |
|       1 | 1187 | `									VmThrowNativeNoWrite(&(*pVm),pThis->pClass,pDecl));` |
|       3 | 1188 | `								VmPopOperand(&pTos,1);    /* pop the attribute name */` |
|       3 | 1189 | `								PH7_MemObjRelease(pTos);  /* the object slot becomes the answer */` |
|       3 | 1190 | `								pTos->nIdx = SXU32_HIGH;` |
|       3 | 1191 | `								VM_EXIT_BREAK;` |
|       - | 1192 | `							}` |
|      13 | 1193 | `							pDecl = 0;` |
|       6 | 1194 | `						}` |
|     575 | 1195 | `						if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       9 | 1196 | `							VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pObjAttr);` |
|       5 | 1197 | `						}else{` |
|       - | 1198 | `							/* php 8 semantics (band A #3b): a PLAIN store to a missing` |
|       - | 1199 | `							 * property dispatches __set($name,$value) when declared —` |
|       - | 1200 | `							 * the value only exists at the following OP_STORE, so park` |
|       - | 1201 | `							 * the receiver+name for it (one-instruction lifetime; the` |
|       - | 1202 | `							 * guard makes a same-name write inside __set fall through` |
|       - | 1203 | `							 * to dynamic creation, like php). Without __set — or for a` |
|       - | 1204 | `							 * subscript-write base / read-modify-write, which php does` |
|       - | 1205 | `							 * NOT route through __set — create a dynamic property on` |
|       - | 1206 | `							 * ANY class with the 8.2 deprecation (suppressed for` |
|       - | 1207 | `							 * stdClass and #[AllowDynamicProperties]); a readonly` |
|       - | 1208 | `							 * class raises php's catchable Error instead. */` |
|     567 | 1209 | `							ph7_class_method *pSetMagic = 0;` |
|     567 | 1210 | `							ph7_class_method *pCoalIsset = 0, *pCoalGet = 0, *pCoalSet = 0;` |
|     567 | 1211 | `							int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);` |
|     567 | 1212 | `							if( bPlainStore \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|     501 | 1213 | `								pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|     248 | 1214 | `							}` |
|     567 | 1215 | `							if( pSetMagic && pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 1216 | `								/* php dispatches __set($name, element) for a missing` |
|       - | 1217 | `								 * destructuring target, but the element value only exists` |
|       - | 1218 | `								 * at the following OP_LOAD_LIST — the dispatch is` |
|       - | 1219 | `								 * unsupported (recorded residual). Leave the miss: the` |
|       - | 1220 | `								 * property stays uncreated, matching php's observable` |
|       - | 1221 | `								 * state (its __set did not store either). */` |
|     567 | 1222 | `							}else if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){` |
|     375 | 1223 | `								pThis->iRef++;` |
|     375 | 1224 | `								pVm->pMagicSetThis = pThis;` |
|     375 | 1225 | `								SyBlobReset(&pVm->sMagicSetName);` |
|     375 | 1226 | `								SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       - | 1227 | `								/* pObjAttr stays NULL; the miss path below stays silent. */` |
|     379 | 1228 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|      67 | 1229 | `							 && pNext->iOp == PH7_OP_NULLC_JMP` |
|      45 | 1230 | `							 && ((pCoalIsset = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1)) != 0` |
|       8 | 1231 | `							  \|\| (pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)) != 0` |
|       5 | 1232 | `							  \|\| (pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1)) != 0) ){` |
|       - | 1233 | ``								/* `$o->p ??= v` on a missing property with magic accessors:`` |
|       - | 1234 | `								 * php consults __isset first when declared — false means` |
|       - | 1235 | `								 * assign directly through __set with NO __get call (the` |
|       - | 1236 | `								 * test value stays null); true (or no __isset) means __get` |
|       - | 1237 | `								 * provides the test value. A COAL_MAGIC entry is pushed` |
|       - | 1238 | `								 * for the OP_NULLC_STORE at the jump target; the` |
|       - | 1239 | `								 * fetch-point sweep drops it when the short-circuit jump` |
|       - | 1240 | `								 * skips the assign or a throw abandons the RHS. Without` |
|       - | 1241 | `								 * __set the assign side keeps the pre-existing loud` |
|       - | 1242 | `								 * "Cannot perform assignment" path (php would create a` |
|       - | 1243 | `								 * dynamic property there — recorded residual). Note the` |
|       - | 1244 | `								 * condition's short-circuit assignment chain: a hit on an` |
|       - | 1245 | `								 * earlier method leaves the later pointers unresolved, so` |
|       - | 1246 | `								 * re-resolve the leftovers here (each is one hash probe,` |
|       - | 1247 | `								 * only on this ??=-miss path). */` |
|       - | 1248 | `								ph7_value sTest;` |
|       7 | 1249 | `								int bMiss = 0; /* __isset said false: skip __get, test value stays null */` |
|       7 | 1250 | `								if( pCoalIsset != 0 ){` |
|       5 | 1251 | `									pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       2 | 1252 | `								}` |
|       7 | 1253 | `								if( pCoalIsset != 0 \|\| pCoalGet != 0 ){` |
|       7 | 1254 | `									pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|       3 | 1255 | `								}` |
|       7 | 1256 | `								PH7_MemObjInit(pVm,&sTest);` |
|       7 | 1257 | `								if( pCoalIsset && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 1258 | `									ph7_value sIssetRet;` |
|       5 | 1259 | `									PH7_MemObjInit(pVm,&sIssetRet);` |
|       5 | 1260 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|       5 | 1261 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|       5 | 1262 | `									VmMagicGuardPop(pVm);` |
|       5 | 1263 | `									PH7_MemObjToBool(&sIssetRet);` |
|       5 | 1264 | `									bMiss = sIssetRet.x.iVal == 0;` |
|       5 | 1265 | `									PH7_MemObjRelease(&sIssetRet);` |
|       2 | 1266 | `								}` |
|       7 | 1267 | `								if( !bMiss && pCoalGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       5 | 1268 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       5 | 1269 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sTest);` |
|       5 | 1270 | `									VmMagicGuardPop(pVm);` |
|       2 | 1271 | `								}` |
|       6 | 1272 | `								if( pCoalSet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s')` |
|       7 | 1273 | `								 && pVm->nBoundaryRc == 0 ){` |
|       - | 1274 | `									VmHookRmw sPend;` |
|       7 | 1275 | `									sPend.iKind = VM_HOOK_PEND_COAL_MAGIC;` |
|       7 | 1276 | `									sPend.pThis = pThis;` |
|       7 | 1277 | `									sPend.pAttr = 0;` |
|       7 | 1278 | `									sPend.nBackIdx = SXU32_HIGH;` |
|       7 | 1279 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|       7 | 1280 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|       7 | 1281 | `									SyBlobAppend(&sPend.sName,(const void *)sName.zString,sName.nByte);` |
|       7 | 1282 | `									sPend.pOwnerStack = (void *)pStack;` |
|       7 | 1283 | `									sPend.pInstrs = (void *)aInstr;` |
|       7 | 1284 | `									sPend.nJmpPc = (sxu32)(pc + 1);        /* the OP_NULLC_JMP */` |
|       7 | 1285 | `									sPend.nPc = (sxu32)(pNext->iP2 - 1);   /* the OP_NULLC_STORE */` |
|       7 | 1286 | `									pThis->iRef++;` |
|       7 | 1287 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|       3 | 1288 | `								}` |
|       - | 1289 | `								/* Pop the attribute name; the test value becomes the` |
|       - | 1290 | `								 * expression slot (a temp, not an lvalue). */` |
|       7 | 1291 | `								VmPopOperand(&pTos,1);` |
|       7 | 1292 | `								pThis->iRef++;` |
|       7 | 1293 | `								PH7_MemObjRelease(pTos);` |
|       7 | 1294 | `								PH7_MemObjStore(&sTest,pTos);` |
|       7 | 1295 | `								pTos->nIdx = SXU32_HIGH;` |
|       7 | 1296 | `								PH7_MemObjRelease(&sTest);` |
|       7 | 1297 | `								PH7_ClassInstanceUnref(pThis);` |
|       7 | 1298 | `								VM_EXIT_BREAK;` |
|     184 | 1299 | `							}else if( VmMemberNextIsRmw(pNext)` |
|     116 | 1300 | `							 && VmMagicRmwEligible(pVm,pClass,pThis,&sName) ){` |
|       - | 1301 | `								/* ++/--/compound-assign on an overloaded property: php` |
|       - | 1302 | `								 * reads through __get and writes the computed value back` |
|       - | 1303 | `								 * through __set. Arm the scratch slot the modify op will` |
|       - | 1304 | `								 * mutate and finish the op here — the pre-existing path` |
|       - | 1305 | `								 * vivified a dynamic property instead, which on any` |
|       - | 1306 | `								 * ordinary accessor class is PHL's` |
|       - | 1307 | `								 * "Cannot create dynamic property" Error. */` |
|       - | 1308 | `								ph7_value sRmwVal;` |
|       - | 1309 | `								sxu32 nRmwScratch;` |
|      27 | 1310 | `								PH7_MemObjInit(pVm,&sRmwVal);` |
|      40 | 1311 | `								VmMagicRmwArm(&(*pVm),pThis,pClass,&sName,&sRmwVal,&nRmwScratch,` |
|      26 | 1312 | `									(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|      27 | 1313 | `								VmPopOperand(&pTos,1);   /* drop the property name */` |
|      27 | 1314 | `								pThis->iRef++;` |
|      27 | 1315 | `								PH7_MemObjRelease(pTos); /* collapse the object slot */` |
|      27 | 1316 | `								PH7_MemObjStore(&sRmwVal,pTos);` |
|      27 | 1317 | `								pTos->nIdx = nRmwScratch;` |
|      27 | 1318 | `								PH7_MemObjRelease(&sRmwVal);` |
|      27 | 1319 | `								PH7_ClassInstanceUnref(pThis);` |
|      27 | 1320 | `								VM_EXIT_BREAK;` |
|     158 | 1321 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|      35 | 1322 | `							 && !VmMemberNextIsWrite(pNext)` |
|      33 | 1323 | `							 && PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) ){` |
|       - | 1324 | `								/* Subscript-write base on a class with __get: php reads` |
|       - | 1325 | `								 * through the magic layer (the write lands on the temp and` |
|       - | 1326 | `								 * is lost, like php's indirect-modification case). A PLAIN` |
|       - | 1327 | `								 * store (bPlainStore — the compiler tags those` |
|       - | 1328 | `								 * PH7_MEMBER_WRITE too) is NOT this case: it falls through` |
|       - | 1329 | `								 * to dynamic creation. Leave the miss path — the read gate` |
|       - | 1330 | `								 * below dispatches __get. */` |
|     160 | 1331 | `							}else if( pThis->pClass->iFlags & PH7_CLASS_READONLY ){` |
|       - | 1332 | `								SyBlob sErrMsg;` |
|       3 | 1333 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 1334 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|       2 | 1335 | `									&pThis->pClass->sName,&sName);` |
|       3 | 1336 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     156 | 1337 | `							}else if( !VmClassAllowsDynamicProps(pVm,pThis->pClass) ){` |
|       - | 1338 | `								/* php 8.2 only DEPRECATES creating a dynamic property on a` |
|       - | 1339 | `								 * class without #[AllowDynamicProperties]; PHL rejects it.` |
|       - | 1340 | `								 * stdClass / __set / declared props are unaffected. */` |
|       - | 1341 | `								SyBlob sErrMsg;` |
|      30 | 1342 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      30 | 1343 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|      26 | 1344 | `									&pThis->pClass->sName,&sName);` |
|      30 | 1345 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      17 | 1346 | `							}else{` |
|     129 | 1347 | `								PH7_VmCreateDynamicAttr(&(*pVm),pThis,sName.zString,sName.nByte,&pObjAttr);` |
|       - | 1348 | `							}` |
|       - | 1349 | `						}` |
|     543 | 1350 | `						if( pObjAttr && VmMemberNextIsRmw(pNext) ){` |
|       - | 1351 | `							/* A read-modify-write READS the property before it writes it,` |
|       - | 1352 | `							 * so php warns that the name it just created was undefined and` |
|       - | 1353 | ``							 * then goes on with null — `$o->hits++` on a fresh object is`` |
|       - | 1354 | ``							 * `Undefined property: C::$hits` and then int(1). PHL created`` |
|       - | 1355 | `							 * the slot in silence. Only the read-modify-write forms:` |
|       - | 1356 | ``							 * a subscript-write base (`$o->arr[] = 1`) and a `??=` read`` |
|       - | 1357 | `							 * nothing, and php says nothing for either. */` |
|      19 | 1358 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|       6 | 1359 | `								&pClass->sName,&sName);` |
|       6 | 1360 | `						}` |
|     269 | 1361 | `					}` |
|    3350 | 1362 | `				}` |
|  116459 | 1363 | `				if( pObjAttr == 0 && pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH` |
|    3274 | 1364 | `				 && !(PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)` |
|    2138 | 1365 | `				   && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g')) ){` |
|       - | 1366 | `					/* D1 commit 2: deferred property arg, property absent on a REACHABLE object` |
|       - | 1367 | `					 * base ($o is a real slot). Record a property step rooted at $o so OP_CALL can` |
|       - | 1368 | `					 * vivify+bind (by-ref) or read via __get / warn (by-value). A magic __get/__set` |
|       - | 1369 | `					 * class is recorded the same way; the resolve step emits php's Notice for the` |
|       - | 1370 | `					 * by-ref case and dispatches __get for by-value. */` |
|       - | 1371 | `					SyString sProp;` |
|      51 | 1372 | `					VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|      51 | 1373 | `					SyStringInitFromBuf(&sProp,sName.zString,sName.nByte);` |
|      51 | 1374 | `					if( pPath && VmDeferPathPushProp(pPath,&sProp) == SXRET_OK ){` |
|      51 | 1375 | `						VmPopOperand(&pTos,1);       /* drop the property name */` |
|      51 | 1376 | `						pThis->iRef++;` |
|      51 | 1377 | `						PH7_MemObjRelease(pTos);     /* collapse the object slot into the carrier */` |
|      51 | 1378 | `						pTos->x.pOther = pPath;` |
|      51 | 1379 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      51 | 1380 | `						pTos->nIdx = SXU32_HIGH;` |
|      51 | 1381 | `						PH7_ClassInstanceUnref(pThis);` |
|      51 | 1382 | `						VM_EXIT_BREAK;` |
|       - | 1383 | `					}` |
|     ! 0 | 1384 | `					if( pPath ){` |
|     ! 0 | 1385 | `						VmFreeDeferredPath(pPath);` |
|     ! 0 | 1386 | `					}` |
|       - | 1387 | `					/* fall through to the normal miss handling on allocation failure */` |
|     ! 0 | 1388 | `				}` |
|  116411 | 1389 | `				if( pObjAttr == 0` |
|   61470 | 1390 | `				 && (pInstr->iP2 == PH7_MEMBER_READ \|\| pInstr->iP2 == PH7_MEMBER_COALESCE) ){` |
|       - | 1391 | `					/* A LAZY native property whose class answers a read from its ZEROED` |
|       - | 1392 | `					 * struct (DatePeriod), asked before anything installed the set. php` |
|       - | 1393 | ``					 * consults that read handler in the plain read AND in `??` -- its third`` |
|       - | 1394 | `` 					 * accessor level takes the property's VALUE, so `$p->recurrences ?? 'd'` `` |
|       - | 1395 | `					 * is 0 there -- while isset()/empty() go to the has_property handler,` |
|       - | 1396 | `					 * which answers false for an object that has no struct at all. */` |
|    5602 | 1397 | `					ph7_class_attr *pLz = PH7_ClassExtractAttribute(pClass,` |
|    1866 | 1398 | `						SyStringData(&sName),SyStringLength(&sName));` |
|    3731 | 1399 | `					if( pLz && (pLz->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT)` |
|      35 | 1400 | `					 && PH7_ATTR_LAZY_ABSENT(pLz,pThis) ){` |
|      25 | 1401 | `						pLazyDefault = pLz;` |
|      12 | 1402 | `					}` |
|    1866 | 1403 | `				}` |
|  116416 | 1404 | `				if( pObjAttr == 0 ){` |
|       - | 1405 | `					/* Missing property. On a plain READ, php dispatches __get($name) and the` |
|       - | 1406 | `					 * expression takes its RETURN VALUE (band A #3a — pre-fix the result was` |
|       - | 1407 | `					 * discarded and a PHL-native warn fired even when __get existed). isset/` |
|       - | 1408 | `					 * empty context consults __isset first (then __get for empty()'s value` |
|       - | 1409 | `					 * test); a plain-store write context parked a pending __set above. A` |
|       - | 1410 | `					 * self-recursive read of the same property falls back to the` |
|       - | 1411 | `					 * undefined-property path via the guard, like php's property guard. */` |
|    6524 | 1412 | `					ph7_class_method *pGetMagic = 0;` |
|    6524 | 1413 | `					if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|     297 | 1414 | `						ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|     297 | 1415 | `						if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 1416 | `							ph7_value sIssetRet;` |
|       - | 1417 | `							int bSet;` |
|     219 | 1418 | `							PH7_MemObjInit(pVm,&sIssetRet);` |
|     219 | 1419 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|     219 | 1420 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|     219 | 1421 | `							VmMagicGuardPop(pVm);` |
|     219 | 1422 | `							PH7_MemObjToBool(&sIssetRet);` |
|     219 | 1423 | `							bSet = sIssetRet.x.iVal != 0;` |
|     219 | 1424 | `							PH7_MemObjRelease(&sIssetRet);` |
|     219 | 1425 | `							if( bSet && VmMemberCtxWantsValue(pInstr->iP2) ){` |
|       - | 1426 | ``								/* empty() and `??`: __isset said set, so php goes on to`` |
|       - | 1427 | `								 * __get for the VALUE — emptiness is judged on it, and the` |
|       - | 1428 | `								 * coalesce simply IS it (a null answer then takes the` |
|       - | 1429 | `								 * default, which OP_NULLC does for free). */` |
|      90 | 1430 | `								ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       - | 1431 | `								ph7_value sEmptyVal;` |
|      90 | 1432 | `								PH7_MemObjInit(pVm,&sEmptyVal);` |
|      90 | 1433 | `								if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|      90 | 1434 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|      90 | 1435 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|      90 | 1436 | `									VmMagicGuardPop(pVm);` |
|      44 | 1437 | `								}` |
|      90 | 1438 | `								VmPopOperand(&pTos,1);` |
|      90 | 1439 | `								pThis->iRef++;` |
|      90 | 1440 | `								PH7_MemObjRelease(pTos);` |
|      90 | 1441 | `								PH7_MemObjStore(&sEmptyVal,pTos);` |
|      90 | 1442 | `								pTos->nIdx = SXU32_HIGH;` |
|      90 | 1443 | `								PH7_MemObjRelease(&sEmptyVal);` |
|      90 | 1444 | `								PH7_ClassInstanceUnref(pThis);` |
|      90 | 1445 | `								VM_EXIT_BREAK;` |
|       - | 1446 | `							}` |
|       - | 1447 | `							/* isset(): the truth of __isset IS the answer — push a non-null` |
|       - | 1448 | `							 * marker for true, NULL for false (isset only tests null-ness). */` |
|     131 | 1449 | `							VmPopOperand(&pTos,1);` |
|     131 | 1450 | `							pThis->iRef++;` |
|     131 | 1451 | `							PH7_MemObjRelease(pTos);` |
|     131 | 1452 | `							if( bSet ){` |
|      61 | 1453 | `								pTos->x.iVal = 1;` |
|      61 | 1454 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      29 | 1455 | `							}` |
|     131 | 1456 | `							pTos->nIdx = SXU32_HIGH;` |
|     131 | 1457 | `							PH7_ClassInstanceUnref(pThis);` |
|     131 | 1458 | `							VM_EXIT_BREAK;` |
|       - | 1459 | `						}` |
|      39 | 1460 | `					}` |
|    6303 | 1461 | `					if( (!VmMemberCtxIsLookup(pInstr->iP2) \|\| pInstr->iP2 == PH7_MEMBER_COALESCE)` |
|    6274 | 1462 | `					 && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|    6279 | 1463 | `					 && !VmMemberNextIsWrite(pInstr + 1) ){` |
|       - | 1464 | `						/* Plain reads AND the ??=/subscript write-base (PH7_MEMBER_WRITE,` |
|       - | 1465 | `						 * which php reads through __get); read-modify-write forms are` |
|       - | 1466 | `						 * excluded (they vivified above — approximate, recorded), and so is` |
|       - | 1467 | `						 * a destructuring target (a pure write — php never reads it).` |
|       - | 1468 | ``						 * `??` reaches here only when the class declares NO __isset — the`` |
|       - | 1469 | `						 * branch above has returned otherwise — so php's gate is absent and` |
|       - | 1470 | `						 * __get answers on its own. */` |
|    5850 | 1471 | `						pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|    2923 | 1472 | `					}` |
|    6308 | 1473 | `					if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 1474 | `						ph7_value sMagicRet;` |
|    5792 | 1475 | `						PH7_MemObjInit(pVm,&sMagicRet);` |
|    5792 | 1476 | `						VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|    5792 | 1477 | `						PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|    5792 | 1478 | `						VmMagicGuardPop(pVm);` |
|    5792 | 1479 | `						if( VmMemberFetchForWrite(pInstr) ){` |
|       - | 1480 | `							/* php's indirect-modification notice, and the separation` |
|       - | 1481 | `							 * that makes the write it describes land nowhere. Raised` |
|       - | 1482 | `							 * BEFORE the pop below: sName still aliases the NAME` |
|       - | 1483 | ``							 * operand when the name is dynamic (`$o->$k[0] = v`). */`` |
|      11 | 1484 | `							PH7_VmOverloadedPropNotice(&(*pVm),pClass,&sName,&sMagicRet);` |
|    5787 | 1485 | `						}else if( pInstr->iP2 == PH7_MEMBER_DEFPATH ){` |
|       - | 1486 | `							/* A deferred call ARGUMENT: __get has answered — php calls it` |
|       - | 1487 | `							 * where the property is WRITTEN, whatever the parameter turns` |
|       - | 1488 | `							 * out to be — and only the by-ref verdict is still pending.` |
|       - | 1489 | `							 * Carry the value with the class and name that produced it, so` |
|       - | 1490 | `							 * the notice lands at the call for a by-REFERENCE parameter and` |
|       - | 1491 | `							 * nothing is said for a by-value one, without __get running` |
|       - | 1492 | `							 * twice or a second read arriving after a later argument's side` |
|       - | 1493 | `							 * effects. */` |
|    3326 | 1494 | `							VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_PROP,` |
|    1107 | 1495 | `								pClass,&sName,&sMagicRet);` |
|    2219 | 1496 | `							if( pPre ){` |
|    2219 | 1497 | `								VmPopOperand(&pTos,1);   /* drop the property name */` |
|    2219 | 1498 | `								pThis->iRef++;` |
|    2219 | 1499 | `								PH7_MemObjRelease(pTos); /* collapse the object slot into the carrier */` |
|    2219 | 1500 | `								pTos->x.pOther = pPre;` |
|    2219 | 1501 | `								pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|    2219 | 1502 | `								pTos->nIdx = SXU32_HIGH;` |
|    2219 | 1503 | `								PH7_MemObjRelease(&sMagicRet);` |
|    2219 | 1504 | `								PH7_ClassInstanceUnref(pThis);` |
|    2219 | 1505 | `								VM_EXIT_BREAK;` |
|       - | 1506 | `							}` |
|     ! 0 | 1507 | `						}` |
|       - | 1508 | `						/* Pop the attribute name, replace the object slot with the magic` |
|       - | 1509 | `						 * result (a temp, not an lvalue — nIdx stays constant). A throw` |
|       - | 1510 | `						 * from __get parked at the boundary; the fetch-point router lands` |
|       - | 1511 | `						 * it and abandons this slot. */` |
|    3578 | 1512 | `						VmPopOperand(&pTos,1);` |
|    3578 | 1513 | `						pThis->iRef++;` |
|    3578 | 1514 | `						PH7_MemObjRelease(pTos);` |
|    3578 | 1515 | `						PH7_MemObjStore(&sMagicRet,pTos);` |
|    3578 | 1516 | `						pTos->nIdx = SXU32_HIGH;` |
|    3578 | 1517 | `						PH7_MemObjRelease(&sMagicRet);` |
|    3578 | 1518 | `						PH7_ClassInstanceUnref(pThis);` |
|    3578 | 1519 | `						VM_EXIT_BREAK;` |
|       - | 1520 | `					}` |
|       - | 1521 | `					/* No __get (or guard held): load null. In isset()/empty() context (iP2 3/4)` |
|       - | 1522 | `					 * PHP returns false/true SILENTLY, so suppress the read-miss warning there` |
|       - | 1523 | `					 * (mirrors the array LOAD_IDX iP2=4/6 suppression); likewise when a` |
|       - | 1524 | `					 * pending __set was parked above (the following OP_STORE dispatches it —` |
|       - | 1525 | `					 * nothing is "undefined" about that write) or a throw is parked on the` |
|       - | 1526 | `					 * boundary rail (e.g. the readonly-class dynamic-property Error — the` |
|       - | 1527 | `					 * fetch-point router lands it right after this op). */` |
|     516 | 1528 | `					if( !VmMemberCtxIsLookup(pInstr->iP2) && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|     444 | 1529 | `					 && pVm->pMagicSetThis == 0` |
|     263 | 1530 | `					 && pVm->nBoundaryRc == 0 ){` |
|       - | 1531 | `						/* A property missing from the INSTANCE may still be DECLARED by the` |
|       - | 1532 | ``						 * class — that is what `unset($o->p)` leaves behind, and php keeps`` |
|       - | 1533 | `						 * answering for the declaration rather than calling the name` |
|       - | 1534 | `						 * undefined: the visibility screen still applies, and a TYPED slot` |
|       - | 1535 | `						 * is back to uninitialized (the same Error a never-written one` |
|       - | 1536 | `						 * raises). Only a name the class does not declare at all is the` |
|       - | 1537 | `						 * "Undefined property" warning. A destructuring target is silent` |
|       - | 1538 | `						 * either way: php created the property above or dispatched __set. */` |
|      70 | 1539 | `						ph7_class_attr *pDeclAttr = PH7_ClassExtractAttribute(pClass,` |
|      22 | 1540 | `							SyStringData(&sName),SyStringLength(&sName));` |
|       - | 1541 | `						/* A static property, a class constant and a native engine slot are` |
|       - | 1542 | `						 * not instance properties; a dynamic one is gone for good once it` |
|       - | 1543 | `						 * is unset, since nothing declares it. */` |
|      44 | 1544 | `						if( pDeclAttr` |
|      41 | 1545 | `						 && (pDeclAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|       - | 1546 | `						                          \|PH7_CLASS_ATTR_HIDDEN\|PH7_CLASS_ATTR_DYNAMIC)) ){` |
|       6 | 1547 | `							pDeclAttr = 0;` |
|       2 | 1548 | `						}` |
|      48 | 1549 | `						if( pDeclAttr && PH7_ATTR_LAZY_ABSENT(pDeclAttr,pThis) ){` |
|       - | 1550 | `							/* The object has never held this name. php has two answers and` |
|       - | 1551 | `							 * the class says which: a read handler over the ZEROED struct` |
|       - | 1552 | `							 * (DatePeriod -- null/0/false, in silence), or nothing at all` |
|       - | 1553 | `							 * (DateInterval), which is the ordinary "Undefined property"` |
|       - | 1554 | `							 * warning. Either way the DECLARATION is not what answers, so` |
|       - | 1555 | `							 * the typed-slot Error below must not fire on a name php keeps` |
|       - | 1556 | `							 * no slot for. */` |
|      25 | 1557 | `							if( pDeclAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT ){` |
|      21 | 1558 | `								pLazyDefault = pDeclAttr;` |
|      10 | 1559 | `							}` |
|      25 | 1560 | `							pDeclAttr = 0;` |
|      12 | 1561 | `						}` |
|      48 | 1562 | `						if( pLazyDefault ){` |
|       - | 1563 | `							/* php's read handler answered: say nothing. */` |
|      35 | 1564 | `						}else if( pDeclAttr` |
|      17 | 1565 | `						 && !PH7_VmClassMemberAccess(&(*pVm),pClass,&pDeclAttr->sName,` |
|       1 | 1566 | `							pDeclAttr->iProtection,FALSE) ){` |
|       - | 1567 | `							SyBlob sErrMsg;` |
|     ! 0 | 1568 | `							const char *zVis = pDeclAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|     ! 0 | 1569 | `								? "private" : "protected";` |
|     ! 0 | 1570 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     ! 0 | 1571 | `							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",` |
|     ! 0 | 1572 | `								zVis,&pClass->sName,&sName);` |
|     ! 0 | 1573 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",` |
|       - | 1574 | `								sizeof("Error")-1,&sErrMsg));` |
|      28 | 1575 | `						}else if( pDeclAttr && (pDeclAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|     ! 0 | 1576 | `							VmBoundaryPark(&(*pVm),` |
|     ! 0 | 1577 | `								VmThrowUninitializedPropertyError(&(*pVm),pClass,pDeclAttr));` |
|     ! 0 | 1578 | `						}else{` |
|      40 | 1579 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|      12 | 1580 | `								&pClass->sName,&sName);` |
|       - | 1581 | `						}` |
|      22 | 1582 | `					}` |
|     258 | 1583 | `				}` |
|  110408 | 1584 | `				if( pObjAttr && (pObjAttr->iState & VM_CLASS_ATTR_RDONLY)` |
|   54971 | 1585 | `				 && (pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|      35 | 1586 | `				  \|\| ((pInstr + 1)->iOp == PH7_OP_STORE && (pInstr + 1)->iP2 != 0)) ){` |
|       - | 1587 | `					/* php's write_property handler refuses the plain store and the` |
|       - | 1588 | `					 * destructuring one that goes through it; everything that takes a` |
|       - | 1589 | ``					 * POINTER to the property instead -- a compound assign, `++`, `??=`,`` |
|       - | 1590 | `					 * a reference bind -- bypasses the handler in php and is left alone` |
|       - | 1591 | `					 * here too. */` |
|       7 | 1592 | `					VmBoundaryPark(&(*pVm),VmThrowNativeReadOnly(&(*pVm),pObjAttr->pAttr));` |
|       7 | 1593 | `					VmPopOperand(&pTos,1);` |
|       7 | 1594 | `					PH7_MemObjRelease(pTos);` |
|       7 | 1595 | `					pTos->nIdx = SXU32_HIGH;` |
|       7 | 1596 | `					VM_EXIT_BREAK;` |
|       - | 1597 | `				}` |
|  110407 | 1598 | `				VmPopOperand(&pTos,1);` |
|       - | 1599 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|       - | 1600 | `				 * This is due to the following case:` |
|       - | 1601 | `				 *     (new TestClass())->foo;` |
|       - | 1602 | `				 */` |
|  110407 | 1603 | `				pThis->iRef++;` |
|  110407 | 1604 | `				PH7_MemObjRelease(pTos);` |
|  110407 | 1605 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|  110407 | 1606 | `				if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - | 1607 | ``					/* `$o->p =& $x`: stash the resolved instance property slot for the`` |
|       - | 1608 | `					 * following member-marked OP_STORE_REF, which rebinds it to alias the` |
|       - | 1609 | `					 * source variable. Do NOT run the read/hook/magic/uninit machinery` |
|       - | 1610 | `					 * below — a reference bind neither reads the value nor triggers` |
|       - | 1611 | `					 * get/set hooks or an uninitialized-typed Error. pThis stays retained` |
|       - | 1612 | `					 * (the iRef++ above); OP_STORE_REF releases it. */` |
|      36 | 1613 | `					if( pObjAttr && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE) ){` |
|       - | 1614 | `						/* A reference bind is a WRITE, and php's refusing handler answers` |
|       - | 1615 | `						 * it with the same sentence a plain store gets. */` |
|     ! 0 | 1616 | `						VmBoundaryPark(&(*pVm),` |
|     ! 0 | 1617 | `							VmThrowNativeNoWrite(&(*pVm),pObjAttr->pOwner,pObjAttr->pAttr));` |
|     ! 0 | 1618 | `						pVm->pRefTargetAttr = 0;` |
|     ! 0 | 1619 | `						pVm->pRefTargetThis = 0;` |
|     ! 0 | 1620 | `						pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 1621 | `						PH7_ClassInstanceUnref(pThis);` |
|      37 | 1622 | `					}else if( pObjAttr && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) ){` |
|       - | 1623 | ``						/* `$i->f =& $x`: php REFUSES to make a handler-backed property`` |
|       - | 1624 | `						 * the target of a reference — there is no slot to rebind, and` |
|       - | 1625 | `						 * every write through the alias would skip the conversion the` |
|       - | 1626 | `						 * handler is there to do. PHL rebound the slot instead, so` |
|       - | 1627 | ``						 * `$x = 2.5` afterwards wrote 2.5 microseconds-free into the`` |
|       - | 1628 | `						 * interval. */` |
|       - | 1629 | `						SyBlob sErrMsg;` |
|       3 | 1630 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 1631 | `						SyBlobAppend(&sErrMsg,"Cannot assign by reference to overloaded object",` |
|       - | 1632 | `							sizeof("Cannot assign by reference to overloaded object")-1);` |
|       3 | 1633 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       3 | 1634 | `						pVm->pRefTargetAttr = 0;` |
|       3 | 1635 | `						pVm->pRefTargetThis = 0;` |
|       3 | 1636 | `						pVm->pRefTargetStaticAttr = 0;` |
|       3 | 1637 | `						PH7_ClassInstanceUnref(pThis);` |
|      35 | 1638 | `					}else if( pObjAttr ){` |
|      34 | 1639 | `						pVm->pRefTargetAttr = pObjAttr;` |
|      34 | 1640 | `						pVm->pRefTargetThis = pThis;` |
|      34 | 1641 | `						pVm->pRefTargetStaticAttr = 0;` |
|      34 | 1642 | `						pTos->nIdx = pObjAttr->nIdx;` |
|      18 | 1643 | `					}else{` |
|       - | 1644 | `						/* Missing/inaccessible target: leave nothing stashed; OP_STORE_REF` |
|       - | 1645 | `						 * no-ops. Balance the retain. */` |
|     ! 0 | 1646 | `						pVm->pRefTargetAttr = 0;` |
|     ! 0 | 1647 | `						pVm->pRefTargetThis = 0;` |
|     ! 0 | 1648 | `						pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 1649 | `						PH7_ClassInstanceUnref(pThis);` |
|       - | 1650 | `					}` |
|      36 | 1651 | `					VM_EXIT_BREAK;` |
|       - | 1652 | `				}` |
|  110373 | 1653 | `				if( pLazyDefault && pLazyDefault->pNativeValue ){` |
|       - | 1654 | `					/* A LAZY native property read before its class installed the set,` |
|       - | 1655 | `					 * on a class that answers such a read from its ZEROED struct. The` |
|       - | 1656 | `					 * declared literal IS that struct's field (the 76th session moved` |
|       - | 1657 | `					 * DatePeriod's seven defaults onto it), and the answer is a` |
|       - | 1658 | `					 * TEMPORARY: nothing was installed, so there is no slot to address` |
|       - | 1659 | `					 * and nIdx stays the constant sentinel the pop left. */` |
|      25 | 1660 | `					PH7_NativeLiteralValue(&(*pVm),pLazyDefault->pNativeValue,pTos);` |
|      12 | 1661 | `				}` |
|  110373 | 1662 | `				if( pObjAttr ){` |
|  109857 | 1663 | `					ph7_value *pValue = 0; /* cc warning */` |
|       - | 1664 | `					/* Check attribute access */` |
|  109857 | 1665 | `					if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pObjAttr->pAttr->sName,pObjAttr->pAttr->iProtection,FALSE) ){` |
|  109812 | 1666 | `						if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET))` |
|   55093 | 1667 | `						 && !VmHookGuardHeld(pVm,(void *)pThis,&sName) ){` |
|       - | 1668 | `							/* PHP 8.4 property hooks: route reads, writes, and the` |
|       - | 1669 | `							 * read-modify-write forms through the synthesized hook` |
|       - | 1670 | `							 * methods. A held guard (either kind) means we are INSIDE` |
|       - | 1671 | `							 * one of this property's own hook bodies — fall through to` |
|       - | 1672 | `` 							 * the raw backing slot for BOTH directions (php: `$this->x` `` |
|       - | 1673 | `							 * within any of x's hooks addresses the backing store). */` |
|     227 | 1674 | `							VmInstr *pNextH = pInstr + 1;` |
|     227 | 1675 | `							int bPlainStore = (pNextH->iOp == PH7_OP_STORE && pNextH->iP2 != 0);` |
|     386 | 1676 | `							int bCoalesceW = pInstr->iP2 == PH7_MEMBER_WRITE` |
|     222 | 1677 | `								&& pNextH->iOp == PH7_OP_NULLC_JMP;` |
|       - | 1678 | `							/* ++/--/compound-assign: the modify op FOLLOWS the member` |
|       - | 1679 | `							 * directly (the compiler tags compound-assign members` |
|       - | 1680 | `							 * PH7_MEMBER_WRITE too, so classify by the next op, not iP2;` |
|       - | 1681 | `							 * the !bPlainStore term is load-bearing — VmMemberNextIsWrite` |
|       - | 1682 | `							 * returns 1 for a member OP_STORE too) */` |
|     227 | 1683 | `							int bRmwNext = !bPlainStore && VmMemberNextIsWrite(pNextH);` |
|     267 | 1684 | `							int bSubscriptW = !bPlainStore && !bCoalesceW && !bRmwNext` |
|     307 | 1685 | `								&& pInstr->iP2 == PH7_MEMBER_WRITE;` |
|     227 | 1686 | `							if( bPlainStore ){` |
|       - | 1687 | `								/* Arm the pending hook-set for the following OP_STORE — it` |
|       - | 1688 | `								 * dispatches the set hook, or throws the read-only Error` |
|       - | 1689 | `								 * when the property only has a get hook. (The scalar` |
|       - | 1690 | `								 * transient is safe here: its window is exactly one` |
|       - | 1691 | `								 * instruction, MEMBER -> STORE, nothing runs in between.) */` |
|      56 | 1692 | `								pThis->iRef++;` |
|      56 | 1693 | `								pVm->pHookSetThis = pThis;` |
|      56 | 1694 | `								pVm->pHookSetAttr = pObjAttr->pAttr;` |
|      56 | 1695 | `								pVm->nHookSetIdx = pObjAttr->nIdx;` |
|      56 | 1696 | `								pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|      56 | 1697 | `								PH7_ClassInstanceUnref(pThis);` |
|      56 | 1698 | `								VM_EXIT_BREAK;` |
|       - | 1699 | `							}` |
|     174 | 1700 | `							if( bSubscriptW ){` |
|       - | 1701 | `								/* Subscript write base ($o->m[] = v, $o->m[$k] = v):` |
|       - | 1702 | `								 * php's catchable Error, with or without a set hook` |
|       - | 1703 | ``								 * (only a by-ref `&get` hook would allow it — a loud`` |
|       - | 1704 | `								 * compile error in PHL). The op completes benignly with` |
|       - | 1705 | `								 * a null temp; the fetch-point router lands the throw. */` |
|       - | 1706 | `								SyBlob sErrMsg;` |
|       7 | 1707 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       7 | 1708 | `								SyBlobFormat(&sErrMsg,"Indirect modification of %z::$%z is not allowed",` |
|       6 | 1709 | `									&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       7 | 1710 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       7 | 1711 | `								PH7_ClassInstanceUnref(pThis);` |
|       7 | 1712 | `								VM_EXIT_BREAK;` |
|       - | 1713 | `							}` |
|     164 | 1714 | `							if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|      86 | 1715 | `							 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 1716 | `								/* VIRTUAL set-only property: there is no backing store to` |
|       - | 1717 | `								 * fall back to, so EVERY read context — plain read,` |
|       - | 1718 | `								 * isset()/empty() (php throws there too, probe-verified),` |
|       - | 1719 | `								 * the ??= test, an RMW read — is php's catchable` |
|       - | 1720 | `								 * write-only Error. */` |
|       - | 1721 | `								SyBlob sErrMsg;` |
|       9 | 1722 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       9 | 1723 | `								SyBlobFormat(&sErrMsg,"Property %z::$%z is write-only",` |
|       8 | 1724 | `									&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       9 | 1725 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       9 | 1726 | `								PH7_ClassInstanceUnref(pThis);` |
|       9 | 1727 | `								VM_EXIT_BREAK;` |
|       - | 1728 | `							}` |
|     160 | 1729 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1730 | ``								/* isset()/empty()/`??` on a hooked property all call the get`` |
|       - | 1731 | `								 * hook (php): isset is "get() !== null", empty tests the` |
|       - | 1732 | ``								 * value, and `??` IS the value. */`` |
|       - | 1733 | `								ph7_value sHookRet;` |
|      16 | 1734 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|      16 | 1735 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|      16 | 1736 | `									if( !VmMemberCtxWantsValue(pInstr->iP2) ){` |
|       - | 1737 | `										/* isset(): the trailing builtin tests NULL-ness of what` |
|       - | 1738 | `										 * it is handed, so "not set" has to BE null. Storing` |
|       - | 1739 | ``										 * bool(FALSE) here made `isset($o->p)` answer TRUE for`` |
|       - | 1740 | `										 * a get hook returning null — the same non-null marker` |
|       - | 1741 | `										 * convention the __isset path uses. */` |
|       8 | 1742 | `										if( (sHookRet.iFlags & MEMOBJ_NULL) == 0 ){` |
|       6 | 1743 | `											pTos->x.iVal = 1;` |
|       6 | 1744 | `											MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       4 | 1745 | `										}else{` |
|       3 | 1746 | `											MemObjSetType(pTos,MEMOBJ_NULL);` |
|       - | 1747 | `										}` |
|       5 | 1748 | `									}else{` |
|       - | 1749 | ``										/* empty() judges the value; `??` IS the value. */`` |
|      10 | 1750 | `										PH7_MemObjStore(&sHookRet,pTos);` |
|       - | 1751 | `									}` |
|      16 | 1752 | `									pTos->nIdx = SXU32_HIGH;` |
|      16 | 1753 | `									PH7_MemObjRelease(&sHookRet);` |
|      16 | 1754 | `									PH7_ClassInstanceUnref(pThis);` |
|      16 | 1755 | `									VM_EXIT_BREAK;` |
|       - | 1756 | `								}` |
|     ! 0 | 1757 | `								PH7_MemObjRelease(&sHookRet);` |
|     ! 0 | 1758 | `							}` |
|     146 | 1759 | `							if( !bCoalesceW && !bRmwNext && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 1760 | `								/* Read: dispatch __phl_hook_get_NAME (inheritance-aware) */` |
|       - | 1761 | `								ph7_value sHookRet;` |
|     112 | 1762 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|     112 | 1763 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|      98 | 1764 | `									if( pInstr->iP2 == PH7_MEMBER_DEFPATH ){` |
|       - | 1765 | `										/* A deferred call ARGUMENT of a HOOKED property. The` |
|       - | 1766 | `										 * get hook has answered, as php's does; what a` |
|       - | 1767 | `										 * by-REFERENCE parameter then gets is not a notice` |
|       - | 1768 | `										 * but php's refusal — a hook has no slot to alias` |
|       - | 1769 | ``										 * and `&get` does not exist here — while a by-VALUE`` |
|       - | 1770 | `										 * one simply takes the hook's value. */` |
|      51 | 1771 | `										VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),` |
|      32 | 1772 | `											VM_OVER_HOOK,pThis->pClass,&pObjAttr->pAttr->sName,&sHookRet);` |
|      35 | 1773 | `										if( pPre ){` |
|      35 | 1774 | `											PH7_MemObjRelease(pTos);` |
|      35 | 1775 | `											pTos->x.pOther = pPre;` |
|      35 | 1776 | `											pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      35 | 1777 | `											pTos->nIdx = SXU32_HIGH;` |
|      35 | 1778 | `											PH7_MemObjRelease(&sHookRet);` |
|      35 | 1779 | `											PH7_ClassInstanceUnref(pThis);` |
|      66 | 1780 | `											VM_EXIT_BREAK;` |
|       - | 1781 | `										}` |
|     ! 0 | 1782 | `									}` |
|      65 | 1783 | `									PH7_MemObjStore(&sHookRet,pTos);` |
|      65 | 1784 | `									pTos->nIdx = SXU32_HIGH;` |
|      65 | 1785 | `									PH7_MemObjRelease(&sHookRet);` |
|      65 | 1786 | `									PH7_ClassInstanceUnref(pThis);` |
|      65 | 1787 | `									VM_EXIT_BREAK;` |
|       - | 1788 | `								}` |
|      15 | 1789 | `								PH7_MemObjRelease(&sHookRet);` |
|       7 | 1790 | `							}` |
|      49 | 1791 | `							if( bCoalesceW \|\| bRmwNext ){` |
|       - | 1792 | ``								/* `$o->x ??= v` (bCoalesceW) and the read-modify-write`` |
|       - | 1793 | ``								 * forms `$o->x++`, `$o->x .= v`, … (bRmwNext): php reads`` |
|       - | 1794 | `								 * through the get hook (raw backing when the property is` |
|       - | 1795 | `								 * set-only) and writes through the set hook.` |
|       - | 1796 | `								 *   - ??=: the test value goes on the stack and a` |
|       - | 1797 | `								 *     COAL_HOOK entry is pushed for the OP_NULLC_STORE` |
|       - | 1798 | `								 *     at the jump target; the fetch-point sweep drops it` |
|       - | 1799 | `								 *     when the short-circuit jump skips the assign or a` |
|       - | 1800 | `								 *     throw abandons the RHS (the entry is NOT a scalar` |
|       - | 1801 | `								 *     transient — nested stores/coalesces in the RHS` |
|       - | 1802 | `								 *     stack their own entries above it).` |
|       - | 1803 | `								 *   - RMW: the value goes into a fresh SCRATCH slot the` |
|       - | 1804 | `								 *     modify op mutates in place; the entry makes the` |
|       - | 1805 | `								 *     op's tail dispatch the set side with the computed` |
|       - | 1806 | `								 *     value (PH7_HOOK_RMW_WRITEBACK). */` |
|       - | 1807 | `								ph7_value sCur;` |
|       - | 1808 | `								sxi32 rcCur;` |
|      35 | 1809 | `								PH7_MemObjInit(pVm,&sCur);` |
|      35 | 1810 | `								rcCur = PH7_VmHookGetAttrValue(pThis,pObjAttr,&sCur);` |
|      35 | 1811 | `								if( rcCur == SXERR_NOTFOUND ){` |
|       - | 1812 | `									/* set-only hook (or guard edge): the read side is the` |
|       - | 1813 | `									 * raw backing store (php) */` |
|       3 | 1814 | `									ph7_value *pBack = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|       3 | 1815 | `									if( pBack ){` |
|       3 | 1816 | `										PH7_MemObjStore(pBack,&sCur);` |
|       2 | 1817 | `									}` |
|      34 | 1818 | `								}else if( pVm->nBoundaryRc != 0 ){` |
|       - | 1819 | `									/* the get hook threw: leave the null temp; the` |
|       - | 1820 | `									 * fetch-point router lands the parked throw —` |
|       - | 1821 | `									 * nothing is armed. */` |
|     ! 0 | 1822 | `									PH7_MemObjRelease(&sCur);` |
|     ! 0 | 1823 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 1824 | `									VM_EXIT_BREAK;` |
|       - | 1825 | `								}` |
|      35 | 1826 | `								if( bCoalesceW ){` |
|       - | 1827 | `									VmHookRmw sPend;` |
|      15 | 1828 | `									PH7_MemObjStore(&sCur,pTos);` |
|      15 | 1829 | `									pTos->nIdx = SXU32_HIGH;` |
|      15 | 1830 | `									PH7_MemObjRelease(&sCur);` |
|      15 | 1831 | `									sPend.iKind = VM_HOOK_PEND_COAL_HOOK;` |
|      15 | 1832 | `									sPend.pThis = pThis;` |
|      15 | 1833 | `									sPend.pAttr = pObjAttr->pAttr;` |
|      15 | 1834 | `									sPend.nBackIdx = pObjAttr->nIdx;` |
|      15 | 1835 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|      15 | 1836 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|      15 | 1837 | `									sPend.pOwnerStack = (void *)pStack;` |
|      15 | 1838 | `									sPend.pInstrs = (void *)aInstr;` |
|      15 | 1839 | `									sPend.nJmpPc = (sxu32)(pc + 1);            /* the OP_NULLC_JMP */` |
|      15 | 1840 | `									sPend.nPc = (sxu32)(pNextH->iP2 - 1);      /* the OP_NULLC_STORE */` |
|      15 | 1841 | `									pThis->iRef++;` |
|      15 | 1842 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|      15 | 1843 | `									PH7_ClassInstanceUnref(pThis);` |
|      15 | 1844 | `									VM_EXIT_BREAK;` |
|       - | 1845 | `								}` |
|       - | 1846 | `								{` |
|      21 | 1847 | `									ph7_value *pScr = PH7_ReserveMemObj(&(*pVm));` |
|      21 | 1848 | `									if( pScr ){` |
|       - | 1849 | `										VmHookRmw sRmw;` |
|      21 | 1850 | `										PH7_MemObjStore(&sCur,pScr);` |
|      21 | 1851 | `										PH7_MemObjStore(&sCur,pTos);` |
|      21 | 1852 | `										pTos->nIdx = pScr->nIdx;` |
|      21 | 1853 | `										sRmw.iKind = VM_HOOK_PEND_RMW;` |
|      21 | 1854 | `										sRmw.pThis = pThis;` |
|      21 | 1855 | `										sRmw.pAttr = pObjAttr->pAttr;` |
|      21 | 1856 | `										sRmw.nBackIdx = pObjAttr->nIdx;` |
|      21 | 1857 | `										sRmw.nScratchIdx = pScr->nIdx;` |
|      21 | 1858 | `										SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      21 | 1859 | `										sRmw.pOwnerStack = (void *)pStack;` |
|      21 | 1860 | `										sRmw.pInstrs = (void *)aInstr;` |
|      21 | 1861 | `										sRmw.nJmpPc = (sxu32)(pc + 1);  /* the modify op ... */` |
|      21 | 1862 | `										sRmw.nPc = (sxu32)(pc + 1);     /* ... is the whole window */` |
|      21 | 1863 | `										pThis->iRef++;` |
|      21 | 1864 | `										SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      10 | 1865 | `									}` |
|       - | 1866 | `									/* OOM: pScr == 0 — leave the null temp (loud allocator` |
|       - | 1867 | `									 * diagnostics already fired) */` |
|      21 | 1868 | `									PH7_MemObjRelease(&sCur);` |
|      21 | 1869 | `									PH7_ClassInstanceUnref(pThis);` |
|      21 | 1870 | `									VM_EXIT_BREAK;` |
|       - | 1871 | `								}` |
|       - | 1872 | `							}` |
|       7 | 1873 | `						}` |
|       - | 1874 | ``						/* `$o->p[$k] = v`, `$o->p[] = v` and `unset($o->p[$k])`: the`` |
|       - | 1875 | `						 * property is the BASE of a subscript write, so the write lands` |
|       - | 1876 | `						 * inside whatever it holds. php screens that where it screens a` |
|       - | 1877 | ``						 * store -- `Cannot indirectly modify readonly property C::$p` --`` |
|       - | 1878 | `						 * and it screens it BEFORE the uninitialized-typed read below,` |
|       - | 1879 | `						 * which is why this sits in front. PHL wrote through to the` |
|       - | 1880 | `						 * array a readonly property held. */` |
|       - | 1881 | `						{` |
|  109609 | 1882 | `							VmInstr *pNextI = pInstr + 1;` |
|  109609 | 1883 | `							int bStoreI = (pNextI->iOp == PH7_OP_STORE && pNextI->iP2 != 0);` |
|  109609 | 1884 | `							int bCoalI = pNextI->iOp == PH7_OP_NULLC_JMP;` |
|  166089 | 1885 | `							int bUnsetBase = pInstr->iP2 == PH7_MEMBER_READ` |
|  109620 | 1886 | `								&& pNextI->iOp == PH7_OP_LOAD_IDX && VM_IDX_IS_UNSET(pNextI->iP2);` |
|  164411 | 1887 | `							int bBaseW = bUnsetBase` |
|  213282 | 1888 | `								\|\| (pInstr->iP2 == PH7_MEMBER_WRITE` |
|  106641 | 1889 | `								    && !bStoreI && !bCoalI && !VmMemberNextIsWrite(pNextI));` |
|  109609 | 1890 | `							if( bBaseW ){` |
|     234 | 1891 | `								sxi32 rcInd = PH7_VmCheckIndirectModify(&(*pVm),pObjAttr->nIdx);` |
|     234 | 1892 | `								if( rcInd != SXRET_OK ){` |
|       7 | 1893 | `									VmBoundaryPark(&(*pVm),rcInd);` |
|       7 | 1894 | `									PH7_MemObjRelease(pTos);` |
|       7 | 1895 | `									pTos->nIdx = SXU32_HIGH;` |
|       7 | 1896 | `									PH7_ClassInstanceUnref(pThis);` |
|       7 | 1897 | `									VM_EXIT_BREAK;` |
|       - | 1898 | `								}` |
|     112 | 1899 | `							}` |
|       - | 1900 | `						}` |
|       - | 1901 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|       - | 1902 | `						 * We can only raise it on a real read, not when the slot is the` |
|       - | 1903 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|       - | 1904 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|       - | 1905 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|  109598 | 1906 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|   55040 | 1907 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|     477 | 1908 | `							VmInstr *pNext = pInstr + 1;` |
|     477 | 1909 | `							int bIsLhs = 0;` |
|     477 | 1910 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|     433 | 1911 | `								bIsLhs = 1;` |
|     214 | 1912 | `							}` |
|     477 | 1913 | `							if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 1914 | `								/* Destructuring target: the OP_LOAD_LIST store (typed-` |
|       - | 1915 | `								 * enforced) initializes it — never a read (php). */` |
|       7 | 1916 | `								bIsLhs = 1;` |
|       3 | 1917 | `							}` |
|       - | 1918 | ``							/* isset()/empty()/`??` read an uninitialized typed property`` |
|       - | 1919 | `							 * as "not set" — a silent miss, NOT the Error a plain read` |
|       - | 1920 | `							 * raises (php). The compiler tags such an access iP2 =` |
|       - | 1921 | `							 * ISSET/EMPTY; treat it like the assignment-LHS case and fall` |
|       - | 1922 | `							 * through to load the slot's NULL. */` |
|     477 | 1923 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|      22 | 1924 | `								bIsLhs = 1;` |
|      10 | 1925 | `							}` |
|     477 | 1926 | `							if( !bIsLhs ){` |
|      22 | 1927 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|      22 | 1928 | `								PH7_ClassInstanceUnref(pThis);` |
|      22 | 1929 | `								if( rcU == PH7_ABORT ){` |
|       3 | 1930 | `									VM_EXIT_ABORT;` |
|       - | 1931 | `								}` |
|       - | 1932 | `								{` |
|       - | 1933 | `									sxi32 iRp;` |
|      20 | 1934 | `									if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      23 | 1935 | `										PH7_RESUME_DRAIN()` |
|      11 | 1936 | `										pc = iRp;` |
|      11 | 1937 | `										VM_EXIT_BREAK;` |
|       - | 1938 | `									}` |
|       - | 1939 | `								}` |
|      10 | 1940 | `								VM_EXIT_EXCEPTION;` |
|       - | 1941 | `							}` |
|     227 | 1942 | `						}` |
|       - | 1943 | `						/* Load attribute */` |
|  109585 | 1944 | `						pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|  109585 | 1945 | `						if( pValue ){` |
|  109585 | 1946 | `							if( pThis->iRef < 2 ){` |
|       - | 1947 | `								/* Perform a store operation,rather than a load operation since` |
|       - | 1948 | `								 * the class instance '$this' will be deleted shortly.` |
|       - | 1949 | `								 */` |
|     258 | 1950 | `								PH7_MemObjStore(pValue,pTos);` |
|     131 | 1951 | `							}else{` |
|       - | 1952 | `								/* Simple load */` |
|  109331 | 1953 | `								PH7_MemObjLoad(pValue,pTos);` |
|       - | 1954 | `							}` |
|  109585 | 1955 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|  109585 | 1956 | `								if( pThis->iRef > 1 ){` |
|       - | 1957 | `									/* Load attribute index */` |
|  109331 | 1958 | `									pTos->nIdx = pObjAttr->nIdx;` |
|   54663 | 1959 | `								}` |
|   54790 | 1960 | `							}` |
|  109580 | 1961 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET)` |
|   55701 | 1962 | `							 && !VmMemberNativeSetKeepsSlot(pInstr) ){` |
|    1199 | 1963 | `								pTos->nIdx = SXU32_HIGH;` |
|    1199 | 1964 | `								pTos->iFlags \|= MEMOBJ_AUX_NATIVEPROP;` |
|     598 | 1965 | `							}` |
|   54790 | 1966 | `						}` |
|  109585 | 1967 | `						if( pInstr->iP2 == PH7_MEMBER_ISSET ){` |
|       - | 1968 | `							/* isset() tests null-ness and nothing else, so reduce the loaded` |
|       - | 1969 | `							 * value to the same non-null marker the __isset and get-hook` |
|       - | 1970 | `							 * paths push. A property read off a TEMPORARY receiver` |
|       - | 1971 | ``							 * (`isset(f()->p)`, `isset((new C)->p)`, and now every`` |
|       - | 1972 | `							 * intermediate link of an accessor chain) leaves no variable` |
|       - | 1973 | `							 * index behind, and the trailing builtin read that as a` |
|       - | 1974 | `							 * CONSTANT and warned -- a diagnostic php has no equivalent of,` |
|       - | 1975 | `							 * its isset() being a language construct rather than a call. */` |
|      82 | 1976 | `							int bSet = (pTos->iFlags & MEMOBJ_NULL) == 0;` |
|      82 | 1977 | `							PH7_MemObjRelease(pTos);` |
|      82 | 1978 | `							if( bSet ){` |
|      59 | 1979 | `								pTos->x.iVal = 1;` |
|      59 | 1980 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      29 | 1981 | `							}` |
|      82 | 1982 | `							pTos->nIdx = SXU32_HIGH;` |
|      40 | 1983 | `						}` |
|   54795 | 1984 | `					}else{` |
|       - | 1985 | `						/* Inaccessible (private/protected) from this scope. php consults` |
|       - | 1986 | `						 * __get here exactly like a missing property (band A #3a) before` |
|       - | 1987 | `						 * erroring; the guard keeps a self-recursive read from looping. */` |
|      44 | 1988 | `						ph7_class_method *pGetMagic = 0;` |
|       - | 1989 | ``						/* A WRITE-context fetch (the base of `$o->p[k] = v`) reads through`` |
|       - | 1990 | `						 * __get in php too — the write then lands on the value it answered` |
|       - | 1991 | `						 * and is lost, with php's indirect-modification notice. PHL refused` |
|       - | 1992 | ``						 * the whole statement with `Cannot access private property` instead,`` |
|       - | 1993 | ``						 * a fatal on a program php runs. A plain store and a `??=` keep`` |
|       - | 1994 | `						 * their own paths below. */` |
|      40 | 1995 | `						if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      52 | 1996 | `						 && (VmMemberFetchForWrite(pInstr)` |
|      30 | 1997 | `						  \|\| (pInstr->iP2 != PH7_MEMBER_WRITE && !VmMemberNextIsWrite(pInstr + 1))) ){` |
|      32 | 1998 | `							pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|      14 | 1999 | `						}` |
|       - | 2000 | `						/* ++/--/compound-assign on a DECLARED but inaccessible property:` |
|       - | 2001 | `						 * php's accessors answer for it exactly as they do for a missing` |
|       - | 2002 | `						 * one (__get, modify, __set), where PHL raised` |
|       - | 2003 | ``						 * `Cannot access private property C::$n`. A plain store keeps its`` |
|       - | 2004 | `						 * own __set park below — it is a write, but not a read-modify-write. */` |
|      40 | 2005 | `						if( VmMemberNextIsRmw(pInstr + 1)` |
|      25 | 2006 | `						 && VmMagicRmwEligible(pVm,pClass,pThis,&sName) ){` |
|       - | 2007 | `							/* The name was already popped and pTos released above, so` |
|       - | 2008 | `							 * sName is the instruction's own literal here. */` |
|       - | 2009 | `							ph7_value sRmwVal;` |
|       - | 2010 | `							sxu32 nRmwScratch;` |
|       3 | 2011 | `							PH7_MemObjInit(pVm,&sRmwVal);` |
|       4 | 2012 | `							VmMagicRmwArm(&(*pVm),pThis,pClass,&sName,&sRmwVal,&nRmwScratch,` |
|       2 | 2013 | `								(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|       3 | 2014 | `							PH7_MemObjStore(&sRmwVal,pTos);` |
|       3 | 2015 | `							pTos->nIdx = nRmwScratch;` |
|       3 | 2016 | `							PH7_MemObjRelease(&sRmwVal);` |
|       3 | 2017 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 2018 | `							VM_EXIT_BREAK;` |
|       - | 2019 | `						}` |
|      42 | 2020 | `						if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 2021 | `							ph7_value sMagicRet;` |
|       9 | 2022 | `							PH7_MemObjInit(pVm,&sMagicRet);` |
|       9 | 2023 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       9 | 2024 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|       9 | 2025 | `							VmMagicGuardPop(pVm);` |
|       9 | 2026 | `							if( VmMemberFetchForWrite(pInstr) ){` |
|       5 | 2027 | `								PH7_VmOverloadedPropNotice(&(*pVm),pClass,&sName,&sMagicRet);` |
|       2 | 2028 | `							}` |
|       - | 2029 | `							/* The name was already popped and pTos released above; just` |
|       - | 2030 | `							 * take the magic result as the expression value. */` |
|       9 | 2031 | `							PH7_MemObjStore(&sMagicRet,pTos);` |
|       9 | 2032 | `							pTos->nIdx = SXU32_HIGH;` |
|       9 | 2033 | `							PH7_MemObjRelease(&sMagicRet);` |
|       9 | 2034 | `							PH7_ClassInstanceUnref(pThis);` |
|       9 | 2035 | `							VM_EXIT_BREAK;` |
|       - | 2036 | `						}` |
|      34 | 2037 | `						if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2038 | `							/* isset/empty on an inaccessible property: php consults __isset` |
|       - | 2039 | `							 * (band A #3b), and is silently false without it — never an` |
|       - | 2040 | `							 * Error (pre-fix PHL fataled here). */` |
|       9 | 2041 | `							ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|       9 | 2042 | `							int bSet = 0;` |
|       9 | 2043 | `							if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 2044 | `								ph7_value sIssetRet;` |
|     ! 0 | 2045 | `								PH7_MemObjInit(pVm,&sIssetRet);` |
|     ! 0 | 2046 | `								VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|     ! 0 | 2047 | `								PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|     ! 0 | 2048 | `								VmMagicGuardPop(pVm);` |
|     ! 0 | 2049 | `								PH7_MemObjToBool(&sIssetRet);` |
|     ! 0 | 2050 | `								bSet = sIssetRet.x.iVal != 0;` |
|     ! 0 | 2051 | `								PH7_MemObjRelease(&sIssetRet);` |
|     ! 0 | 2052 | `							}` |
|       9 | 2053 | `							if( pInstr->iP2 == PH7_MEMBER_COALESCE && pIssetMagic == 0 ){` |
|       - | 2054 | ``								/* `??` with no __isset to gate it: php reads straight through`` |
|       - | 2055 | `								 * __get, exactly as it does for a MISSING property. The` |
|       - | 2056 | `								 * __get dispatch just above this block is gated on a` |
|       - | 2057 | `								 * non-lookup context, so answer here. */` |
|       5 | 2058 | `								ph7_class_method *pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       5 | 2059 | `								if( pCoalGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 2060 | `									ph7_value sCoalRet;` |
|     ! 0 | 2061 | `									PH7_MemObjInit(pVm,&sCoalRet);` |
|     ! 0 | 2062 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     ! 0 | 2063 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sCoalRet);` |
|     ! 0 | 2064 | `									VmMagicGuardPop(pVm);` |
|     ! 0 | 2065 | `									PH7_MemObjStore(&sCoalRet,pTos);` |
|     ! 0 | 2066 | `									pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 2067 | `									PH7_MemObjRelease(&sCoalRet);` |
|     ! 0 | 2068 | `								}` |
|       5 | 2069 | `								PH7_ClassInstanceUnref(pThis);` |
|       5 | 2070 | `								VM_EXIT_BREAK;` |
|       - | 2071 | `							}` |
|       5 | 2072 | `							if( bSet ){` |
|     ! 0 | 2073 | `								if( VmMemberCtxWantsValue(pInstr->iP2) ){` |
|     ! 0 | 2074 | `									ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       - | 2075 | `									ph7_value sEmptyVal;` |
|     ! 0 | 2076 | `									PH7_MemObjInit(pVm,&sEmptyVal);` |
|     ! 0 | 2077 | `									if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|     ! 0 | 2078 | `										VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     ! 0 | 2079 | `										PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|     ! 0 | 2080 | `										VmMagicGuardPop(pVm);` |
|     ! 0 | 2081 | `									}` |
|     ! 0 | 2082 | `									PH7_MemObjStore(&sEmptyVal,pTos);` |
|     ! 0 | 2083 | `									pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 2084 | `									PH7_MemObjRelease(&sEmptyVal);` |
|     ! 0 | 2085 | `								}else{` |
|     ! 0 | 2086 | `									pTos->x.iVal = 1;` |
|     ! 0 | 2087 | `									MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     ! 0 | 2088 | `									pTos->nIdx = SXU32_HIGH;` |
|       - | 2089 | `								}` |
|     ! 0 | 2090 | `							}` |
|       5 | 2091 | `							PH7_ClassInstanceUnref(pThis);` |
|       5 | 2092 | `							VM_EXIT_BREAK;` |
|       - | 2093 | `						}` |
|       - | 2094 | `						{` |
|       - | 2095 | `							/* A plain store to an inaccessible property dispatches __set` |
|       - | 2096 | `							 * (band A #3b): park the receiver+name for the following` |
|       - | 2097 | `							 * OP_STORE, exactly like the missing-property case. */` |
|      26 | 2098 | `							VmInstr *pNextW = pInstr + 1;` |
|      26 | 2099 | `							if( pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0 ){` |
|       3 | 2100 | `								ph7_class_method *pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|       3 | 2101 | `								if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){` |
|       3 | 2102 | `									pThis->iRef++;` |
|       3 | 2103 | `									pVm->pMagicSetThis = pThis;` |
|       3 | 2104 | `									SyBlobReset(&pVm->sMagicSetName);` |
|       3 | 2105 | `									SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       3 | 2106 | `									pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|       3 | 2107 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 2108 | `									VM_EXIT_BREAK;` |
|       - | 2109 | `								}` |
|     ! 0 | 2110 | `							}` |
|       - | 2111 | `						}` |
|      20 | 2112 | `						if( ((pInstr + 1)->iOp == PH7_OP_NULLC \|\| (pInstr + 1)->iOp == PH7_OP_NULLC_JMP)` |
|      14 | 2113 | `						 && pInstr->iP2 != PH7_MEMBER_WRITE ){` |
|       - | 2114 | `` 							/* `$o->priv ?? default` (OP_NULLC; NULLC_JMP is the `??=` `` |
|       - | 2115 | ``							 * pre-test): php treats `??` as an isset-style lookup — an`` |
|       - | 2116 | `							 * inaccessible property yields null SILENTLY (the` |
|       - | 2117 | `							 * __isset/__get consults already ran above), letting the` |
|       - | 2118 | `							 * coalesce pick the default. */` |
|     ! 0 | 2119 | `							PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2120 | `							VM_EXIT_BREAK; /* pTos is already the null temp */` |
|       - | 2121 | `						}` |
|       - | 2122 | `						/* A subclass reading a PARENT's PRIVATE property: php treats it as` |
|       - | 2123 | `						 * an UNDEFINED property (the base-private is invisible to the` |
|       - | 2124 | `						 * subclass scope) — a Warning + null, NOT an access Error. Only` |
|       - | 2125 | `						 * this exact shape warns; every other denied read is the catchable` |
|       - | 2126 | `						 * "Cannot access" Error below. */` |
|       - | 2127 | `						{` |
|      24 | 2128 | `						ph7_class *pSelf = VmCurrentSelf(&(*pVm));` |
|      24 | 2129 | `						ph7_class *pDecl = pObjAttr->pAttr->pDeclClass;` |
|      20 | 2130 | `						if( pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      21 | 2131 | `						 && pSelf && pDecl && pSelf != pDecl && PH7_VmInstanceOf(pSelf,pDecl) ){` |
|       3 | 2132 | `							if( !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       4 | 2133 | `								VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|       1 | 2134 | `									&pClass->sName,&sName);` |
|       1 | 2135 | `							}` |
|       3 | 2136 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 2137 | `							VM_EXIT_BREAK; /* pTos is already the null temp */` |
|       - | 2138 | `						}` |
|       - | 2139 | `						}` |
|       - | 2140 | `						/* Genuinely inaccessible: php's CATCHABLE Error (parked on the` |
|       - | 2141 | `						 * boundary rail; the fetch-point router lands it — it was an` |
|       - | 2142 | `						 * uncatchable VmReportUncaughtException+Abort before). */` |
|       - | 2143 | `						{` |
|       - | 2144 | `						SyBlob sErrMsg;` |
|      22 | 2145 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      22 | 2146 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      22 | 2147 | `						SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",` |
|       9 | 2148 | `							zVis,&pClass->sName,&sName);` |
|      22 | 2149 | `						PH7_ClassInstanceUnref(pThis);` |
|      22 | 2150 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      22 | 2151 | `						SyBlobRelease(&sErrMsg);` |
|      22 | 2152 | `						VM_EXIT_BREAK;` |
|       - | 2153 | `						}` |
|       - | 2154 | `					}` |
|   54790 | 2155 | `				}` |
|       - | 2156 | `				/* Safely unreference the object */` |
|  110101 | 2157 | `				PH7_ClassInstanceUnref(pThis);` |
|       - | 2158 | `			}` |
|  127415 | 2159 | `		}else{` |
|     148 | 2160 | `			if( (pNos->iFlags & MEMOBJ_AUX_STROFFSET)` |
|      82 | 2161 | `			 && (pInstr->iP2 == PH7_MEMBER_WRITE \|\| pInstr->iP2 == PH7_MEMBER_UNSET` |
|       5 | 2162 | `			  \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET \|\| pInstr->iP2 == PH7_MEMBER_REF_TARGET` |
|       - | 2163 | ``			  /* A reference SOURCE (`$r =& $s[0]->p`) is compiled as a READ here on`` |
|       - | 2164 | `			   * purpose — php hands back a copy for a handler-backed property — so the` |
|       - | 2165 | `			   * bind that follows is what makes it a reach-inside, exactly as it does` |
|       - | 2166 | `			   * for a subscript. */` |
|       4 | 2167 | `			  \|\| (pInstr->iP2 == PH7_MEMBER_READ` |
|       3 | 2168 | `			      && ((pInstr + 1)->iOp == PH7_OP_STORE_REF` |
|       1 | 2169 | `			       \|\| (pInstr + 1)->iOp == PH7_OP_LOAD_REF` |
|     ! 0 | 2170 | `			       \|\| (pInstr + 1)->iOp == PH7_OP_STORE_IDX_REF))) ){` |
|       - | 2171 | `				/* The base is a string OFFSET and this reaches INSIDE it. php refuses every` |
|       - | 2172 | `				 * such reach and words the refusal from what is doing the reaching, so a` |
|       - | 2173 | ``				 * PROPERTY is `Cannot use string offset as an object` where a subscript is`` |
|       - | 2174 | ``				 * `... as an array` — `$s[0]->p = 1`, `$s[0]->p += 1`, `$s[0]->p ??= 1` and`` |
|       - | 2175 | ``				 * `unset($s[0]->p)` alike. PHL reported the generic non-object write`` |
|       - | 2176 | ``				 * (`Attempt to assign property "p" on string`) and said nothing at all for`` |
|       - | 2177 | ``				 * the unset. A METHOD CALL is not one of these: php keeps `Call to a member`` |
|       - | 2178 | ``				 * function p() on string` there, and so does the path below. */`` |
|       - | 2179 | `				sxi32 rcSo;` |
|      11 | 2180 | `				VmPopOperand(&pTos,1);` |
|      11 | 2181 | `				PH7_MemObjRelease(pTos);` |
|      11 | 2182 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      11 | 2183 | `				pTos->nIdx = SXU32_HIGH;` |
|      11 | 2184 | `				rcSo = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an object",` |
|       - | 2185 | `					sizeof("Cannot use string offset as an object")-1);` |
|      11 | 2186 | `				if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 | 2187 | `				rc = rcSo;` |
|      11 | 2188 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2189 | `			}` |
|       - | 2190 | ``			/* `->` on a non-object (e.g. a null intermediate). Silent in isset()/empty()/unset()`` |
|       - | 2191 | ``			 * context (iP2 2/3/4) so `isset($o->missing->x)` / `unset($o->missing->x)` match PHP. */`` |
|     140 | 2192 | `			if( pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2193 | `				/* php draws a sharp line here, and PH7 drew none: CALLING a method on a` |
|       - | 2194 | `				 * non-object is a catchable Error, while READING a property off one is a` |
|       - | 2195 | `				 * Warning that yields NULL. PH7 raised the same notice for both and` |
|       - | 2196 | ``				 * carried on with NULL, so `$null->m()` silently did nothing. */`` |
|       - | 2197 | `				SyString sMemb;` |
|     108 | 2198 | `				const char *zVerb = 0;` |
|     108 | 2199 | `				SyStringInitFromBuf(&sMemb,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     108 | 2200 | `				if( pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|       - | 2201 | ``					/* A deferred call ARGUMENT (`f($u->p)`): which diagnostic php raises`` |
|       - | 2202 | `					 * depends on the parameter, and this op does not know it — a by-VALUE` |
|       - | 2203 | `					 * binding is the read warning below, a by-REFERENCE one is php's` |
|       - | 2204 | ``					 * `Attempt to modify property` Error. Record the step rooted at the`` |
|       - | 2205 | `					 * base and let OP_CALL re-drive it in the mode the callee decides,` |
|       - | 2206 | `					 * exactly as a property MISSING on a real object is recorded. */` |
|      27 | 2207 | `					VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|      27 | 2208 | `					if( pPath && VmDeferPathPushProp(pPath,&sMemb) == SXRET_OK ){` |
|      27 | 2209 | `						VmPopOperand(&pTos,1);   /* drop the property name */` |
|      27 | 2210 | `						PH7_MemObjRelease(pTos); /* collapse the base slot into the carrier */` |
|      27 | 2211 | `						pTos->x.pOther = pPath;` |
|      27 | 2212 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      27 | 2213 | `						pTos->nIdx = SXU32_HIGH;` |
|      51 | 2214 | `						VM_EXIT_BREAK;` |
|       - | 2215 | `					}` |
|     ! 0 | 2216 | `					if( pPath ){` |
|     ! 0 | 2217 | `						VmFreeDeferredPath(pPath);` |
|     ! 0 | 2218 | `					}` |
|       - | 2219 | `					/* fall through to the immediate warning on allocation failure */` |
|     ! 0 | 2220 | `				}` |
|       - | 2221 | `				/* WRITING one is an Error too, and php picks its verb from what the` |
|       - | 2222 | `				 * write actually is. PH7 warned about a READ it never performed and` |
|       - | 2223 | `				 * then let the store fail into its own` |
|       - | 2224 | `				 * "Cannot perform assignment on a constant class attribute", so the` |
|       - | 2225 | `				 * script carried on past a statement php stops it for. The kind is` |
|       - | 2226 | `				 * read off the following instruction, the way every other write` |
|       - | 2227 | `				 * classification in this handler is (VmMemberFetchForWrite). */` |
|      82 | 2228 | `				if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|      31 | 2229 | `					const VmInstr *pNextW = pInstr + 1;` |
|      30 | 2230 | `					if( (pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0)` |
|      21 | 2231 | ``					 \|\| pNextW->iOp == PH7_OP_NULLC_JMP /* `$u->p ??= v` */`` |
|      12 | 2232 | `					 \|\| VmNextIsCompoundAssign(pNextW) ){` |
|      23 | 2233 | `						zVerb = "assign";` |
|      20 | 2234 | `					}else if( pNextW->iOp == PH7_OP_INCR \|\| pNextW->iOp == PH7_OP_DECR ){` |
|       5 | 2235 | `						zVerb = "increment/decrement";` |
|       3 | 2236 | `					}else{` |
|       - | 2237 | ``						/* The base of a subscript write (`$u->p[] = v`): php asks the`` |
|       - | 2238 | `						 * property for something to modify, and there is no property. */` |
|       5 | 2239 | `						zVerb = "modify";` |
|       1 | 2240 | `					}` |
|      67 | 2241 | `				}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       3 | 2242 | `					zVerb = "assign";` |
|      51 | 2243 | `				}else if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       3 | 2244 | `					zVerb = "modify";` |
|       1 | 2245 | `				}` |
|      82 | 2246 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD \|\| zVerb ){` |
|       - | 2247 | `					SyBlob sErrM;` |
|       - | 2248 | `					sxi32 rcErr;` |
|      49 | 2249 | `					SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      49 | 2250 | `					if( zVerb ){` |
|      35 | 2251 | `						SyBlobFormat(&sErrM,"Attempt to %s property \"%z\" on %s",` |
|      17 | 2252 | `							zVerb,&sMemb,VmArithValueName(pNos));` |
|      18 | 2253 | `					}else{` |
|      15 | 2254 | `						SyBlobFormat(&sErrM,"Call to a member function %z() on %s",` |
|       7 | 2255 | `							&sMemb,VmArithValueName(pNos));` |
|       - | 2256 | `					}` |
|      49 | 2257 | `					VmPopOperand(&pTos,1);` |
|      49 | 2258 | `					PH7_MemObjRelease(pTos);` |
|      49 | 2259 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      49 | 2260 | `					pTos->nIdx = SXU32_HIGH;` |
|      73 | 2261 | `					rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      24 | 2262 | `						SyBlobLength(&sErrM));` |
|      49 | 2263 | `					SyBlobRelease(&sErrM);` |
|      49 | 2264 | `					if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      49 | 2265 | `					rc = rcErr;` |
|      51 | 2266 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2267 | `				}` |
|      50 | 2268 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Attempt to read property \"%z\" on %s",` |
|      16 | 2269 | `					&sMemb,VmArithValueName(pNos));` |
|      16 | 2270 | `			}` |
|      66 | 2271 | `			VmPopOperand(&pTos,1);` |
|      66 | 2272 | `			PH7_MemObjRelease(pTos);` |
|      66 | 2273 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|       - | 2274 | `		}` |
|  127447 | 2275 | `	}else{` |
|       - | 2276 | `		/* Static member access using class name */` |
|  105509 | 2277 | `		pNos = pTos;` |
|  105509 | 2278 | `		pThis = 0;` |
|  105509 | 2279 | `		if( !pInstr->p3 ){` |
|    5061 | 2280 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    5061 | 2281 | `			pNos--;` |
|       - | 2282 | `#ifdef UNTRUST` |
|       - | 2283 | `			if( pNos < pStack ){` |
|       - | 2284 | `				VM_EXIT_ABORT;` |
|       - | 2285 | `			}` |
|       - | 2286 | `#endif` |
|    2533 | 2287 | `		}else{` |
|       - | 2288 | `			/* Attribute name already computed */` |
|  100453 | 2289 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|       - | 2290 | `		}` |
|  105509 | 2291 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|  105505 | 2292 | `			ph7_class *pClass = 0;` |
|       - | 2293 | `			/* A FORWARDING static call — self::/parent::/static:: — preserves the` |
|       - | 2294 | `			 * caller's late-static-binding class (php); a non-forwarding C::m() resets` |
|       - | 2295 | `			 * it to C. The receiver slot below keeps the literal keyword, which OP_CALL` |
|       - | 2296 | `			 * can't resolve, so it would fall back to the callee's DECLARING class and` |
|       - | 2297 | `			 * lose LSB. Remember the live LSB class here and stamp it onto the receiver` |
|       - | 2298 | `			 * after the method name is pushed. */` |
|  105505 | 2299 | `			int bForwardingCall = 0;` |
|  105505 | 2300 | `			ph7_class *pForwardLsb = 0;` |
|       - | 2301 | `			/* Same rail snapshot as OP_NEW: the lookup below can run an autoloader that` |
|       - | 2302 | `			 * throws, and php then reports that exception and nothing else. */` |
|  105505 | 2303 | `			sxi32 nMbBrc = pVm->nBoundaryRc;` |
|  105505 | 2304 | `			const void *pMbRes = (const void *)pVm->pResumeFrame;` |
|  105505 | 2305 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - | 2306 | `				/* Class already instantiated */` |
|      24 | 2307 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      24 | 2308 | `				pClass = pThis->pClass;` |
|      24 | 2309 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      13 | 2310 | `			}else{` |
|       - | 2311 | `				/* Try to extract the target class */` |
|  105483 | 2312 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|  105483 | 2313 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|  105483 | 2314 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|       - | 2315 | `					/* Handle self/static/parent keywords */` |
|  105483 | 2316 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       - | 2317 | `						/* In a trait method, self:: resolves to the USING class */` |
|     415 | 2318 | `						pClass = PH7_VmPeekSelfClass(&(*pVm));` |
|     415 | 2319 | `						bForwardingCall = 1;` |
|     415 | 2320 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|  105278 | 2321 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|     110 | 2322 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|     110 | 2323 | `						bForwardingCall = 1;` |
|     110 | 2324 | `						pForwardLsb = pClass;` |
|  105020 | 2325 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|     146 | 2326 | `						pClass = PH7_VmResolveParentClass(&(*pVm));` |
|     146 | 2327 | `						bForwardingCall = 1;` |
|     146 | 2328 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|      75 | 2329 | `					}else{` |
|  104825 | 2330 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|       - | 2331 | `					}` |
|   52739 | 2332 | `				}` |
|       - | 2333 | `			}` |
|  105505 | 2334 | `			if( pClass == 0 ){` |
|       - | 2335 | `				/* Undefined class: php throws a catchable Error */` |
|       - | 2336 | `				SyBlob sErrM;` |
|       - | 2337 | `				sxi32 rcErr;` |
|      14 | 2338 | `				if( PH7_VmClassLookupRaised(&(*pVm),nMbBrc,pMbRes) ){` |
|       - | 2339 | `					/* ...unless an autoloader raised: land THAT instead of reporting the` |
|       - | 2340 | `					 * class missing on top of it. */` |
|       9 | 2341 | `					sxi32 rcAutoM = pVm->nBoundaryRc;` |
|       9 | 2342 | `					pVm->nBoundaryRc = 0;` |
|       9 | 2343 | `					if( !pInstr->p3 ){` |
|       7 | 2344 | `						VmPopOperand(&pTos,1);` |
|       3 | 2345 | `					}` |
|       9 | 2346 | `					PH7_MemObjRelease(pTos);` |
|       9 | 2347 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|       9 | 2348 | `					pTos->nIdx = SXU32_HIGH;` |
|       9 | 2349 | `					if( rcAutoM == PH7_ABORT ){` |
|     ! 0 | 2350 | `						VM_EXIT_ABORT;` |
|       - | 2351 | `					}` |
|       9 | 2352 | `					rc = PH7_EXCEPTION;` |
|       9 | 2353 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2354 | `				}` |
|       6 | 2355 | `				SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       6 | 2356 | `				SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|       4 | 2357 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob));` |
|       6 | 2358 | `				if( !pInstr->p3 ){` |
|       6 | 2359 | `					VmPopOperand(&pTos,1);` |
|       2 | 2360 | `				}` |
|       6 | 2361 | `				PH7_MemObjRelease(pTos);` |
|       6 | 2362 | `				pTos->nIdx = SXU32_HIGH;` |
|       8 | 2363 | `				rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       2 | 2364 | `					SyBlobLength(&sErrM));` |
|       6 | 2365 | `				SyBlobRelease(&sErrM);` |
|       6 | 2366 | `				if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       6 | 2367 | `				rc = rcErr;` |
|       6 | 2368 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 2369 | `			}else{` |
|  105493 | 2370 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - | 2371 | `					/* Method call */` |
|    2823 | 2372 | `					ph7_class_method *pMeth = 0;` |
|    2823 | 2373 | `					if( sName.nByte > 0 ){` |
|       - | 2374 | `						/* An INTERFACE's methods are looked up too: they are all abstract,` |
|       - | 2375 | `						 * so the arm below reports php's "Cannot call abstract method` |
|       - | 2376 | `						 * I::m()" rather than claiming the name does not exist. */` |
|    2823 | 2377 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|    1409 | 2378 | `					}` |
|    2823 | 2379 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      71 | 2380 | `						if( pMeth ){` |
|       - | 2381 | `							SyBlob sErrM;` |
|       - | 2382 | `							sxi32 rcErr;` |
|       5 | 2383 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       5 | 2384 | `							SyBlobFormat(&sErrM,"Cannot call abstract method %z::%z()",` |
|       2 | 2385 | `								&pClass->sName,&sName);` |
|       5 | 2386 | `							if( !pInstr->p3 ){` |
|       5 | 2387 | `								VmPopOperand(&pTos,1);` |
|       2 | 2388 | `							}` |
|       5 | 2389 | `							PH7_MemObjRelease(pTos);` |
|       5 | 2390 | `							pTos->nIdx = SXU32_HIGH;` |
|       - | 2391 | ``							/* The `$obj::m()` form took a reference on the receiver at the`` |
|       - | 2392 | `							 * top of this branch; every exit has to give it back, and only` |
|       - | 2393 | `							 * the fall-through at the end of the arm used to. */` |
|       5 | 2394 | `							if( pThis ){` |
|     ! 0 | 2395 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2396 | `								pThis = 0;` |
|     ! 0 | 2397 | `							}` |
|       7 | 2398 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       2 | 2399 | `								SyBlobLength(&sErrM));` |
|       5 | 2400 | `							SyBlobRelease(&sErrM);` |
|       5 | 2401 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 2402 | `							rc = rcErr;` |
|       7 | 2403 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 2404 | `						}else{` |
|      67 | 2405 | `							ph7_class_instance *pMagicThis = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|      67 | 2406 | `							ph7_class_method *pCallStaticMagic = pMagicThis` |
|      16 | 2407 | `								? PH7_ClassExtractMethod(pMagicThis->pClass,"__call",sizeof("__call")-1)` |
|      56 | 2408 | `								: PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1);` |
|      67 | 2409 | `							if( pCallStaticMagic ){` |
|       - | 2410 | `								/* php: C::missing(...) dispatches __callStatic($name,$args)` |
|       - | 2411 | `								 * through the packing body (see the instance twin) — or` |
|       - | 2412 | `								 * __call($name,$args) on the calling frame's own $this when` |
|       - | 2413 | `								 * that receiver fits C (VmStaticCallMagicThis). */` |
|      84 | 2414 | `								VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pMagicThis,` |
|      27 | 2415 | `									pMagicThis ? pMagicThis->pClass : pClass,&sName);` |
|      57 | 2416 | `								if( pPend == 0 ){` |
|     ! 0 | 2417 | `									VM_EXIT_ABORT;` |
|       - | 2418 | `								}` |
|      57 | 2419 | `								if( !pInstr->p3 ){` |
|      57 | 2420 | `									VmPopOperand(&pTos,1);` |
|      27 | 2421 | `								}` |
|      57 | 2422 | `								PH7_MemObjRelease(pTos);` |
|      57 | 2423 | `								if( pThis ){` |
|     ! 0 | 2424 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2425 | `									pThis = 0;` |
|     ! 0 | 2426 | `								}` |
|      57 | 2427 | `								pTos->x.pOther = pPend;` |
|      57 | 2428 | `								pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|      57 | 2429 | `								pTos->nIdx = SXU32_HIGH;` |
|      57 | 2430 | `								VM_EXIT_BREAK;` |
|       - | 2431 | `							}` |
|       - | 2432 | `							{` |
|       - | 2433 | `								/* php: the STATIC form reports the same "Call to undefined` |
|       - | 2434 | `								 * method C::m()" as the instance one. */` |
|       - | 2435 | `								SyBlob sErrM;` |
|       - | 2436 | `								sxi32 rcErr;` |
|      11 | 2437 | `								SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      11 | 2438 | `								SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",` |
|       5 | 2439 | `									&pClass->sName,&sName);` |
|      11 | 2440 | `								if( !pInstr->p3 ){` |
|      11 | 2441 | `									VmPopOperand(&pTos,1);` |
|       5 | 2442 | `								}` |
|      11 | 2443 | `								PH7_MemObjRelease(pTos);` |
|      11 | 2444 | `								pTos->nIdx = SXU32_HIGH;` |
|      11 | 2445 | `								if( pThis ){` |
|     ! 0 | 2446 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2447 | `									pThis = 0;` |
|     ! 0 | 2448 | `								}` |
|      16 | 2449 | `								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       5 | 2450 | `									SyBlobLength(&sErrM));` |
|      11 | 2451 | `								SyBlobRelease(&sErrM);` |
|      11 | 2452 | `								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 | 2453 | `								rc = rcErr;` |
|      15 | 2454 | `								PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2455 | `							}` |
|       - | 2456 | `						}` |
|       - | 2457 | `						/* Pop the method name from the stack */` |
|     ! 0 | 2458 | `						if( !pInstr->p3 ){` |
|     ! 0 | 2459 | `							VmPopOperand(&pTos,1);` |
|       - | 2460 | `						}` |
|     ! 0 | 2461 | `						PH7_MemObjRelease(pTos);` |
|     ! 0 | 2462 | `					}else{` |
|       - | 2463 | `						/* Inaccessible from this scope: php routes through the same fallback a` |
|       - | 2464 | ``						 * MISSING name takes — __call on a compatible `$this`, else`` |
|       - | 2465 | ``						 * __callStatic — which is why `C::privateStatic()` runs a catch-all`` |
|       - | 2466 | `						 * instead of the "Call to private method" Error OP_CALL would raise` |
|       - | 2467 | `						 * below. */` |
|    2755 | 2468 | `						ph7_class_method *pDeniedStatic = 0;` |
|    2755 | 2469 | `						ph7_class_instance *pDeniedThis = 0;` |
|    2755 | 2470 | `						int bDeniedStatic = 0;` |
|    2755 | 2471 | `						if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - | 2472 | `							/* The OWNING class decides, as in the instance twin above: a` |
|       - | 2473 | `							 * trait method's rules belong to the class that composed it. */` |
|      29 | 2474 | `							ph7_class *pOwnerCls = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|      29 | 2475 | `							if( !PH7_VmClassMemberAccess(&(*pVm),pOwnerCls,&sName,pMeth->iProtection,FALSE) ){` |
|      15 | 2476 | `								pDeniedThis = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|      15 | 2477 | `								pDeniedStatic = pDeniedThis` |
|       4 | 2478 | `									? PH7_ClassExtractMethod(pDeniedThis->pClass,"__call",` |
|       - | 2479 | `										sizeof("__call")-1)` |
|      12 | 2480 | `									: PH7_ClassExtractMethod(pClass,"__callStatic",` |
|       - | 2481 | `										sizeof("__callStatic")-1);` |
|      15 | 2482 | `								bDeniedStatic = pDeniedStatic == 0;` |
|       7 | 2483 | `							}` |
|      14 | 2484 | `						}` |
|    2755 | 2485 | `						if( bDeniedStatic ){` |
|       - | 2486 | `							/* No catch-all answers for it: php's visibility Error, raised at` |
|       - | 2487 | `							 * the resolution like the instance twin above. OP_CALL's own` |
|       - | 2488 | `							 * screen re-derives the method from the FUNCTION's name against` |
|       - | 2489 | `							 * its declaring class, which cannot see a trait adaptation's` |
|       - | 2490 | ``							 * composed protection — `sHi as private sPriv` ran from global`` |
|       - | 2491 | `							 * scope in silence. */` |
|       - | 2492 | `							SyBlob sErrM;` |
|       - | 2493 | `							sxi32 rcErr;` |
|       7 | 2494 | `							ph7_class *pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|       7 | 2495 | `							ph7_class *pScope = PH7_VmCallerScopeName(&(*pVm));` |
|      10 | 2496 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       3 | 2497 | `								? "private" : "protected";` |
|       7 | 2498 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       7 | 2499 | `							if( pScope ){` |
|       3 | 2500 | `								SyBlobFormat(&sErrM,"Call to %s method %z::%z() from scope %z",` |
|       1 | 2501 | `									zVis,&pOwner->sName,&sName,&pScope->sName);` |
|       2 | 2502 | `							}else{` |
|       5 | 2503 | `								SyBlobFormat(&sErrM,"Call to %s method %z::%z() from global scope",` |
|       2 | 2504 | `									zVis,&pOwner->sName,&sName);` |
|       - | 2505 | `							}` |
|       7 | 2506 | `							if( !pInstr->p3 ){` |
|       7 | 2507 | `								VmPopOperand(&pTos,1);` |
|       3 | 2508 | `							}` |
|       7 | 2509 | `							PH7_MemObjRelease(pTos);` |
|       7 | 2510 | `							pTos->nIdx = SXU32_HIGH;` |
|       7 | 2511 | `							if( pThis ){` |
|     ! 0 | 2512 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2513 | `								pThis = 0;` |
|     ! 0 | 2514 | `							}` |
|      10 | 2515 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       3 | 2516 | `								SyBlobLength(&sErrM));` |
|       7 | 2517 | `							SyBlobRelease(&sErrM);` |
|       7 | 2518 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       7 | 2519 | `							rc = rcErr;` |
|       7 | 2520 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2521 | `						}` |
|    2749 | 2522 | `						if( pDeniedStatic ){` |
|       - | 2523 | `							/* The packing body takes no receiver slot: drop the method-name` |
|       - | 2524 | `							 * slot so the MARK lands on the receiver slot, exactly as the` |
|       - | 2525 | `							 * missing-method twin above does (leaving the class name in place` |
|       - | 2526 | `							 * would pack it as the first $args entry). */` |
|      13 | 2527 | `							VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pDeniedThis,` |
|       4 | 2528 | `								pDeniedThis ? pDeniedThis->pClass : pClass,&sName);` |
|       9 | 2529 | `							if( pPend == 0 ){` |
|     ! 0 | 2530 | `								VM_EXIT_ABORT;` |
|       - | 2531 | `							}` |
|       9 | 2532 | `							if( !pInstr->p3 ){` |
|       9 | 2533 | `								VmPopOperand(&pTos,1);` |
|       4 | 2534 | `							}` |
|       9 | 2535 | `							PH7_MemObjRelease(pTos);` |
|       9 | 2536 | `							if( pThis ){` |
|     ! 0 | 2537 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2538 | `								pThis = 0;` |
|     ! 0 | 2539 | `							}` |
|       9 | 2540 | `							pTos->x.pOther = pPend;` |
|       9 | 2541 | `							pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|       9 | 2542 | `							pTos->nIdx = SXU32_HIGH;` |
|       9 | 2543 | `							VM_EXIT_BREAK;` |
|       - | 2544 | `						}` |
|       - | 2545 | ``						/* php refuses a NON-STATIC method named through `::` unless the`` |
|       - | 2546 | ``						 * CALLING frame holds a `$this` the class accepts: that is what`` |
|       - | 2547 | ``						 * makes `self::`/`parent::`/`static::` and `C::m()` from inside a`` |
|       - | 2548 | `						 * C method work, and every other spelling — from global scope,` |
|       - | 2549 | `` 						 * from a static method, from an unrelated class, and `$obj::m()` `` |
|       - | 2550 | ``						 * — an Error. PHL ran the body instead, with `$this` unset or`` |
|       - | 2551 | ``						 * (for the object form) bound to the object the `::` was written`` |
|       - | 2552 | `						 * on, which php never uses: it takes the CALLER's. The message` |
|       - | 2553 | ``						 * names the DECLARING class, so `D::m()` reports B::m(). */`` |
|    2741 | 2554 | `						if( (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|     151 | 2555 | `							ph7_class_instance *pCallerThis = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|     151 | 2556 | `							if( pCallerThis == 0 ){` |
|       - | 2557 | `								SyBlob sErrM;` |
|       - | 2558 | `								sxi32 rcErr;` |
|      13 | 2559 | `								ph7_class *pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|      13 | 2560 | `								SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      13 | 2561 | `								SyBlobFormat(&sErrM,"Non-static method %z::%z() cannot be called statically",` |
|       6 | 2562 | `									&pOwner->sName,&pMeth->sFunc.sName);` |
|      13 | 2563 | `								if( !pInstr->p3 ){` |
|      13 | 2564 | `									VmPopOperand(&pTos,1);` |
|       6 | 2565 | `								}` |
|      13 | 2566 | `								PH7_MemObjRelease(pTos);` |
|      13 | 2567 | `								pTos->nIdx = SXU32_HIGH;` |
|      13 | 2568 | `								if( pThis ){` |
|       3 | 2569 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 2570 | `									pThis = 0;` |
|       1 | 2571 | `								}` |
|      19 | 2572 | `								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       6 | 2573 | `									SyBlobLength(&sErrM));` |
|      13 | 2574 | `								SyBlobRelease(&sErrM);` |
|      13 | 2575 | `								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      13 | 2576 | `								rc = rcErr;` |
|      13 | 2577 | `								PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2578 | `							}` |
|       - | 2579 | `` 							/* The receiver is that `$this` — never the object the `::` `` |
|       - | 2580 | `							 * was written on, and never nothing. Naming it here is also` |
|       - | 2581 | `							 * what makes the call work inside a CLOSURE declared in a` |
|       - | 2582 | ``							 * method: php gives such a closure a `$this`, PHL carries it`` |
|       - | 2583 | `							 * as a frame variable rather than on the frame itself, and` |
|       - | 2584 | ``							 * OP_CALL reads only the frame, so `self::m()` in a closure`` |
|       - | 2585 | `							 * used to run with no receiver at all. */` |
|     139 | 2586 | `							PH7_MemObjRelease(pNos);` |
|     139 | 2587 | `							pNos->x.pOther = pCallerThis;` |
|     139 | 2588 | `							MemObjSetType(pNos,MEMOBJ_OBJ);` |
|     139 | 2589 | `							pCallerThis->iRef++;` |
|     139 | 2590 | `							pNos->nIdx = SXU32_HIGH;` |
|      68 | 2591 | `						}` |
|       - | 2592 | `						/* Push method name on the stack, MARKED as already screened (see the` |
|       - | 2593 | `						 * instance twin: OP_CALL cannot re-derive a composed protection). */` |
|    2729 | 2594 | `						PH7_MemObjRelease(pTos);` |
|    2729 | 2595 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|    2729 | 2596 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|    2729 | 2597 | `						pTos->iFlags \|= MEMOBJ_AUX_MEMBERCALL;` |
|       - | 2598 | `					}` |
|    2729 | 2599 | `					pTos->nIdx = SXU32_HIGH;` |
|       - | 2600 | `					/* Forwarding call (self::/parent::/static::): overwrite the receiver` |
|       - | 2601 | `					 * slot (pNos, one below the method name) with the live LSB class name` |
|       - | 2602 | ``					 * so OP_CALL pushes THAT onto aSelf — preserving `static::` inside the`` |
|       - | 2603 | `					 * callee. Without this the literal keyword falls through to the` |
|       - | 2604 | `					 * callee's declaring class. Only when an LSB class is actually in` |
|       - | 2605 | `					 * scope (a static call from global scope has none). */` |
|    2729 | 2606 | `					if( bForwardingCall && pForwardLsb && (pNos->iFlags & MEMOBJ_STRING) ){` |
|      80 | 2607 | `						SyBlobReset(&pNos->sBlob);` |
|      80 | 2608 | `						SyBlobAppend(&pNos->sBlob,pForwardLsb->sName.zString,pForwardLsb->sName.nByte);` |
|      39 | 2609 | `					}` |
|    1367 | 2610 | `				}else{` |
|       - | 2611 | `					/* Attribute access */` |
|  102675 | 2612 | `					ph7_class_attr *pAttr = 0;` |
|  102675 | 2613 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - | 2614 | `						/* unset(C::$x): PHP rejects unsetting a static property with a fatal Error.` |
|       - | 2615 | `						 * Without this the iP2=unset tag falls through to a normal static read and the` |
|       - | 2616 | `						 * trailing generic unset() would silently NULL (and de-type) the shared slot. */` |
|       - | 2617 | `						char zMsg[256];` |
|     ! 0 | 2618 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Attempt to unset static property %.*s::$%.*s",` |
|     ! 0 | 2619 | `							(int)pClass->sName.nByte,pClass->sName.zString,(int)sName.nByte,sName.zString);` |
|     ! 0 | 2620 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|     ! 0 | 2621 | `						VM_EXIT_ABORT;` |
|       - | 2622 | `					}` |
|       - | 2623 | `					/* Check for special ::class pseudo-constant */` |
|  102837 | 2624 | `					if( sName.nByte == sizeof("class")-1 &&` |
|     324 | 2625 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|       - | 2626 | `						/* ::class returns the fully qualified class name */` |
|       - | 2627 | `						/* Pop the attribute name from the stack */` |
|     153 | 2628 | `						if( !pInstr->p3 ){` |
|     153 | 2629 | `							VmPopOperand(&pTos,1);` |
|      74 | 2630 | `						}` |
|     153 | 2631 | `						PH7_MemObjRelease(pTos);` |
|       - | 2632 | `						/* Load the class name */` |
|     153 | 2633 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|     153 | 2634 | `						pTos->nIdx = SXU32_HIGH;` |
|      79 | 2635 | `					}else{` |
|       - | 2636 | `						/* Extract the target attribute. php keeps constants and` |
|       - | 2637 | `						 * (static) properties in separate namespaces; the source` |
|       - | 2638 | ``						 * form disambiguates (set by the `::` codegen, compile.c):`` |
|       - | 2639 | ``						 * a `$`-form is a STATIC PROPERTY (hAttr) — either a literal`` |
|       - | 2640 | ``						 * name folded into pInstr->p3 (`C::$s`) or a dynamic name on`` |
|       - | 2641 | ``						 * the stack marked iP1==2 (`C::$$x`, `C::${$e}`); a bareword`` |
|       - | 2642 | `						 * (p3==0, iP1==1) is a CONSTANT or enum case (hConst). This` |
|       - | 2643 | ``						 * is what lets `const C` and `public $C` coexist and resolve`` |
|       - | 2644 | `						 * to the right member. */` |
|  102527 | 2645 | `						if( sName.nByte > 0 ){` |
|  103566 | 2646 | `							pAttr = (pInstr->p3 \|\| pInstr->iP1 == 2)` |
|  100450 | 2647 | `								? PH7_ClassExtractAttribute(pClass,sName.zString,sName.nByte)` |
|   53333 | 2648 | `								: PH7_ClassExtractConstant(pClass,sName.zString,sName.nByte);` |
|   51261 | 2649 | `						}` |
|  102527 | 2650 | `						if( pAttr == 0 ){` |
|       - | 2651 | `							/* No such member. php raises a catchable Error whose wording` |
|       - | 2652 | `							 * depends on the ACCESS form (the same p3/iP1 signal used above):` |
|       - | 2653 | ``							 * a bareword `C::MISSING` (constant form) is "Undefined constant`` |
|       - | 2654 | ``							 * C::MISSING", a `$`-form `C::$missing` is "Access to undeclared`` |
|       - | 2655 | `							 * static property C::$missing". Instance magic (__get) is never` |
|       - | 2656 | `							 * consulted for statics (band A #3b). isset()/empty() context` |
|       - | 2657 | `							 * stays silently false. Parked on the boundary rail; the op` |
|       - | 2658 | `							 * completes benignly with NULL and the fetch-point router lands` |
|       - | 2659 | `							 * the throw. */` |
|      16 | 2660 | `							if( !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2661 | `								SyBlob sErrMsg;` |
|      16 | 2662 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      16 | 2663 | `								if( pInstr->p3 == 0 && pInstr->iP1 != 2 ){` |
|       9 | 2664 | `									SyBlobFormat(&sErrMsg,"Undefined constant %z::%z",` |
|       4 | 2665 | `										&pClass->sName,&sName);` |
|       5 | 2666 | `								}else{` |
|       8 | 2667 | `									SyBlobFormat(&sErrMsg,"Access to undeclared static property %z::$%z",` |
|       3 | 2668 | `										&pClass->sName,&sName);` |
|       - | 2669 | `								}` |
|      16 | 2670 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       7 | 2671 | `							}` |
|       7 | 2672 | `						}` |
|       - | 2673 | `						/* Pop the attribute name from the stack */` |
|  102527 | 2674 | `						if( !pInstr->p3 ){` |
|    2083 | 2675 | `							VmPopOperand(&pTos,1);` |
|    1039 | 2676 | `						}` |
|  102527 | 2677 | `						PH7_MemObjRelease(pTos);` |
|  102527 | 2678 | `						pTos->nIdx = SXU32_HIGH;` |
|  102527 | 2679 | `						if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - | 2680 | ``							/* `self::$s =& $x` / `C::$s =& $x`: stash the static property slot`` |
|       - | 2681 | `							 * for the following member-marked OP_STORE_REF (class-level, shared` |
|       - | 2682 | `							 * across instances — matches php). Skip the read machinery below. */` |
|      16 | 2683 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|      16 | 2684 | `							 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|      18 | 2685 | `							 && VmClassStaticDeferPending(pClass) ){` |
|       - | 2686 | `								/* Binding a reference TO a static touches the table, so it` |
|       - | 2687 | `								 * materializes first, like every other static access. */` |
|       3 | 2688 | `								sxi32 rcRt = PH7_VmMaterializeClassStatics(&(*pVm),pClass);` |
|       3 | 2689 | `								if( rcRt != SXRET_OK ){` |
|       3 | 2690 | `									pVm->pRefTargetStaticAttr = 0;` |
|       3 | 2691 | `									pVm->pRefTargetAttr = 0;` |
|       3 | 2692 | `									pVm->pRefTargetThis = 0;` |
|       3 | 2693 | `									if( pThis ){` |
|     ! 0 | 2694 | `										PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2695 | `									}` |
|       3 | 2696 | `									if( rcRt == PH7_ABORT ){` |
|     ! 0 | 2697 | `										VM_EXIT_ABORT;` |
|       - | 2698 | `									}` |
|       - | 2699 | `									{` |
|       - | 2700 | `										sxi32 iRpR;` |
|       3 | 2701 | `										if( VmRecordedResume(pVm,&iRpR,pState->pEntryFrame,aInstr) ){` |
|       7 | 2702 | `											PH7_RESUME_DRAIN()` |
|       3 | 2703 | `											pc = iRpR;` |
|       3 | 2704 | `											VM_EXIT_BREAK;` |
|       - | 2705 | `										}` |
|       - | 2706 | `									}` |
|     ! 0 | 2707 | `									VM_EXIT_EXCEPTION;` |
|       - | 2708 | `								}` |
|     ! 0 | 2709 | `							}` |
|      14 | 2710 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|      16 | 2711 | `							 && PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|      16 | 2712 | `								pVm->pRefTargetStaticAttr = pAttr;` |
|      16 | 2713 | `								pVm->pRefTargetAttr = 0;` |
|      16 | 2714 | `								pVm->pRefTargetThis = 0;` |
|      16 | 2715 | `								pTos->nIdx = pAttr->nIdx;` |
|       9 | 2716 | `							}else{` |
|     ! 0 | 2717 | `								pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 2718 | `								pVm->pRefTargetAttr = 0;` |
|     ! 0 | 2719 | `								pVm->pRefTargetThis = 0;` |
|       - | 2720 | `							}` |
|      16 | 2721 | `							if( pThis ){` |
|     ! 0 | 2722 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2723 | `							}` |
|      16 | 2724 | `							VM_EXIT_BREAK;` |
|       - | 2725 | `						}` |
|  102511 | 2726 | `						if( pAttr ){` |
|  102497 | 2727 | `							if( (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|       - | 2728 | `								/* Access to a non static attribute */` |
|     ! 0 | 2729 | `								VmErrorFormat(&(*pVm),PH7_CTX_ERR,"Access to a non-static class attribute '%z::%z',PH7 is loading NULL",` |
|     ! 0 | 2730 | `									&pClass->sName,&pAttr->sName` |
|       - | 2731 | `									);` |
|     ! 0 | 2732 | `							}else{` |
|       - | 2733 | `								ph7_value *pValue;` |
|       - | 2734 | `								/* php materializes the class's static table at the FIRST` |
|       - | 2735 | `								 * static-property access (any property, any context — read,` |
|       - | 2736 | `								 * write, even isset): a default whose evaluation was DEFERRED` |
|       - | 2737 | `								 * (it threw at the declaration) runs here, and a typed one that` |
|       - | 2738 | `								 * failed its check throws its catchable TypeError here. Class` |
|       - | 2739 | `								 * constants and static method calls do not trigger the` |
|       - | 2740 | `								 * materialization (php-exact). */` |
|  102492 | 2741 | `								if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|  101460 | 2742 | `								 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|  100433 | 2743 | `								 && VmClassStaticDeferPending(pClass) ){` |
|      49 | 2744 | `									sxi32 rcD = PH7_VmMaterializeClassStatics(&(*pVm),pClass);` |
|      49 | 2745 | `									if( rcD != SXRET_OK ){` |
|      45 | 2746 | `										if( pThis ){` |
|     ! 0 | 2747 | `											PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2748 | `										}` |
|      45 | 2749 | `										if( rcD == PH7_ABORT ){` |
|     ! 0 | 2750 | `											VM_EXIT_ABORT;` |
|       - | 2751 | `										}` |
|       - | 2752 | `										{` |
|       - | 2753 | `											sxi32 iRpD;` |
|      45 | 2754 | `											if( VmRecordedResume(pVm,&iRpD,pState->pEntryFrame,aInstr) ){` |
|      89 | 2755 | `												PH7_RESUME_DRAIN()` |
|      41 | 2756 | `												pc = iRpD;` |
|      41 | 2757 | `												VM_EXIT_BREAK;` |
|       - | 2758 | `											}` |
|       - | 2759 | `										}` |
|       5 | 2760 | `										VM_EXIT_EXCEPTION;` |
|       - | 2761 | `									}` |
|       2 | 2762 | `								}` |
|       - | 2763 | `								/* Check if the access to the attribute is allowed */` |
|  102455 | 2764 | `								if( PH7_VmClassMemberAccess(&(*pVm),pClass,&pAttr->sName,pAttr->iProtection,FALSE) ){` |
|       - | 2765 | `									/* PHP 7.4+: uninitialized typed static read.` |
|       - | 2766 | `									 * Same LHS-of-store peek as the instance path. */` |
|  102446 | 2767 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|  101297 | 2768 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|  150143 | 2769 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|  100092 | 2770 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|  100097 | 2771 | `										if( pS ){` |
|  100097 | 2772 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|  100097 | 2773 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|  100014 | 2774 | `												VmInstr *pNext = pInstr + 1;` |
|  100014 | 2775 | `												int bIsLhs = 0;` |
|  100014 | 2776 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|       8 | 2777 | `													bIsLhs = 1;` |
|       3 | 2778 | `												}` |
|  100014 | 2779 | `												if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 2780 | `													/* Destructuring target ([S::$s] = [...]):` |
|       - | 2781 | `													 * initialized by the OP_LOAD_LIST store. */` |
|       3 | 2782 | `													bIsLhs = 1;` |
|       1 | 2783 | `												}` |
|  100014 | 2784 | `												if( !bIsLhs ){` |
|  100004 | 2785 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|  100004 | 2786 | `													if( pThis ){` |
|     ! 0 | 2787 | `														PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2788 | `													}` |
|  100004 | 2789 | `													if( rcU == PH7_ABORT ){` |
|     ! 0 | 2790 | `														VM_EXIT_ABORT;` |
|       - | 2791 | `													}` |
|       - | 2792 | `													{` |
|       - | 2793 | `														sxi32 iRp;` |
|  100004 | 2794 | `														if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|  200006 | 2795 | `															PH7_RESUME_DRAIN()` |
|  100004 | 2796 | `															pc = iRp;` |
|  100004 | 2797 | `															VM_EXIT_BREAK;` |
|       - | 2798 | `														}` |
|       - | 2799 | `													}` |
|     ! 0 | 2800 | `													VM_EXIT_EXCEPTION;` |
|       - | 2801 | `												}` |
|       4 | 2802 | `											}` |
|      45 | 2803 | `										}` |
|      45 | 2804 | `									}` |
|    2444 | 2805 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|    2257 | 2806 | `									 && SySetUsed(&pAttr->aAttrs) > 0 ){` |
|       - | 2807 | `										/* php 8.4 #[\Deprecated] on a class constant:` |
|       - | 2808 | `										 * every access re-warns. */` |
|      14 | 2809 | `										VmDeprecatedConstNotice(&(*pVm),pClass,pAttr);` |
|       6 | 2810 | `									}` |
|    2449 | 2811 | `									if( pAttr->nIdx == SXU32_HIGH ){` |
|       - | 2812 | `										/* Unmaterialized slot. Enum case: first access` |
|       - | 2813 | `										 * materializes ALL the singletons. Plain constant: its` |
|       - | 2814 | `										 * initializer hasn't run yet (mount-order-dependent` |
|       - | 2815 | `										 * cross-constant reference) — evaluate on demand. A` |
|       - | 2816 | `										 * raised TypeError/Error (backing mismatch, duplicate` |
|       - | 2817 | `										 * value, self-reference) parks on the boundary rail;` |
|       - | 2818 | `										 * the op completes benignly with NULL and the` |
|       - | 2819 | `										 * fetch-point router lands the throw. */` |
|       - | 2820 | `										sxi32 rcEnum;` |
|     677 | 2821 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|       - | 2822 | `											/* php: a DIRECT static access evaluates every case` |
|       - | 2823 | `											 * of the enum (whole-class constant update) — a` |
|       - | 2824 | `											 * broken sibling case throws here too. A reference` |
|       - | 2825 | `											 * from inside another constant's initializer` |
|       - | 2826 | `											 * (nConstEvalDepth > 0) evaluates only the` |
|       - | 2827 | `											 * requested case. */` |
|      63 | 2828 | `											if( pVm->nConstEvalDepth > 0 ){` |
|       3 | 2829 | `												rcEnum = VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|       2 | 2830 | `											}else{` |
|      61 | 2831 | `												rcEnum = VmEnumMaterialize(&(*pVm),pClass);` |
|       - | 2832 | `											}` |
|      34 | 2833 | `										}else{` |
|     619 | 2834 | `											rcEnum = VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|       - | 2835 | `										}` |
|     677 | 2836 | `										if( rcEnum != SXRET_OK ){` |
|      41 | 2837 | `											VmBoundaryPark(&(*pVm),rcEnum);` |
|      19 | 2838 | `										}` |
|     336 | 2839 | `									}` |
|       - | 2840 | `									/* Load the desired attribute */` |
|    2449 | 2841 | `									pValue = (ph7_value *)SySetAt(&pVm->aMemObj,pAttr->nIdx);` |
|    2449 | 2842 | `									if( pValue ){` |
|    2409 | 2843 | `										PH7_MemObjLoad(pValue,pTos);` |
|    2409 | 2844 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       - | 2845 | `											/* Load index number */` |
|     389 | 2846 | `											pTos->nIdx = pAttr->nIdx;` |
|     192 | 2847 | `										}` |
|    1202 | 2848 | `									}` |
|    1227 | 2849 | `								}else{` |
|       - | 2850 | `									/* Throw Error exception (PHP-compatible) */` |
|       - | 2851 | `									char zMsg[256];` |
|       5 | 2852 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       5 | 2853 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|       7 | 2854 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s constant %.*s::%.*s",` |
|       4 | 2855 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|       4 | 2856 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|       3 | 2857 | `									}else{` |
|     ! 0 | 2858 | `										SyBufferFormat(zMsg,sizeof(zMsg),"Cannot access %s property %.*s::$%.*s",` |
|     ! 0 | 2859 | `											zVis,(int)pClass->sName.nByte,pClass->sName.zString,` |
|     ! 0 | 2860 | `											(int)pAttr->sName.nByte,pAttr->sName.zString);` |
|       - | 2861 | `									}` |
|       5 | 2862 | `									VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|       5 | 2863 | `									VM_EXIT_ABORT;` |
|       - | 2864 | `								}` |
|       - | 2865 | `							}` |
|    1222 | 2866 | `						}` |
|       - | 2867 | `					}` |
|       - | 2868 | `				}` |
|    5335 | 2869 | `				if( pThis ){` |
|       - | 2870 | `					/* Safely unreference the object */` |
|      22 | 2871 | `					PH7_ClassInstanceUnref(pThis);` |
|      10 | 2872 | `				}` |
|       - | 2873 | `			}` |
|    2670 | 2874 | `		}else{` |
|       - | 2875 | ``			/* `$v::X` where $v holds neither an object nor a class NAME. php's`` |
|       - | 2876 | `			 * catchable Error, which STOPS the statement; PH7 raised its own` |
|       - | 2877 | `` 			 * uncatchable wording and carried on with NULL, so a `$obj::$s = 1` `` |
|       - | 2878 | `			 * through a null base went on to fail again in OP_STORE. */` |
|       - | 2879 | `			sxi32 rcCn;` |
|       5 | 2880 | `			if( !pInstr->p3 ){` |
|       3 | 2881 | `				VmPopOperand(&pTos,1);` |
|       1 | 2882 | `			}` |
|       5 | 2883 | `			PH7_MemObjRelease(pTos);` |
|       5 | 2884 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       5 | 2885 | `			pTos->nIdx = SXU32_HIGH;` |
|       5 | 2886 | `			rcCn = VmThrowFromVm(&(*pVm),"Error","Class name must be a valid object or a string",` |
|       - | 2887 | `				sizeof("Class name must be a valid object or a string")-1);` |
|       5 | 2888 | `			if( rcCn == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 2889 | `			rc = rcCn;` |
|       5 | 2890 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2891 | `		}` |
|       - | 2892 | `	}` |
|  260216 | 2893 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2894 | `	VM_EXIT_BREAK;` |
|  183767 | 2895 | `}` |
|       - | 2896 |  |
|       - | 2897 | `/*` |
|       - | 2898 | ` * OP_CLONE: body moved verbatim from the OP_CLONE arm of` |
|       - | 2899 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 2900 | ` */` |
|     416 | 2901 | `PH7_PRIVATE VmOpRc VmExecOpClone(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 2902 | `{` |
|     421 | 2903 | `	ph7_value *pTos = pState->pTos;` |
|     421 | 2904 | `	ph7_value *pStack = pState->pStack;` |
|     421 | 2905 | `	VmInstr *aInstr = pState->aInstr;` |
|     421 | 2906 | `	sxi32 pc = pState->pc;` |
|       - | 2907 | `	sxi32 rc;` |
|     208 | 2908 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 2909 | `	ph7_class_instance *pSrc,*pClone;` |
|       - | 2910 | `#ifdef UNTRUST` |
|       - | 2911 | `	if( pTos < pStack ){` |
|       - | 2912 | `		VM_EXIT_ABORT;` |
|       - | 2913 | `	}` |
|       - | 2914 | `#endif` |
|       - | 2915 | `	/* Make sure we are dealing with a class instance. PHP 8 throws a catchable` |
|       - | 2916 | ``	 * TypeError for a non-object operand — for both `clone $x` and clone($x). */`` |
|     421 | 2917 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - | 2918 | `		SyBlob sMsg;` |
|       - | 2919 | `		char zGiven[64];` |
|      24 | 2920 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 2921 | `		/* php names the VALUE for a bool ("true given", not "bool given"), which is` |
|       - | 2922 | `		 * what every other argument diagnostic here already says — the shared helper. */` |
|      24 | 2923 | `		SyBlobFormat(&sMsg,"clone(): Argument #1 ($object) must be of type object, %s given",` |
|      11 | 2924 | `			VmValueGivenName(pTos,zGiven,sizeof(zGiven)));` |
|      24 | 2925 | `		rc = VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|      24 | 2926 | `		PH7_MemObjRelease(pTos);` |
|      24 | 2927 | `		pTos->nIdx = SXU32_HIGH;` |
|      24 | 2928 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 2929 | `			VM_EXIT_ABORT;` |
|       - | 2930 | `		}` |
|       - | 2931 | `		{` |
|       - | 2932 | `			sxi32 iRp;` |
|      24 | 2933 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      24 | 2934 | `				pc = iRp;` |
|      24 | 2935 | `				VM_EXIT_BREAK;` |
|       - | 2936 | `			}` |
|       - | 2937 | `		}` |
|     ! 0 | 2938 | `		VM_EXIT_EXCEPTION;` |
|       - | 2939 | `	}` |
|       - | 2940 | `	/* Point to the source */` |
|     399 | 2941 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|       - | 2942 | `	/* Enum cases are not cloneable — php's catchable Error (the singleton` |
|       - | 2943 | `	 * identity would break) — and neither is a class whose instances own a` |
|       - | 2944 | `	 * C-side resource a slot-by-slot copy would double-free (PH7_CLASS_NOCLONE),` |
|       - | 2945 | `	 * nor a Generator or a Fiber (their suspended C state is not copyable). php` |
|       - | 2946 | `	 * words all of them the same way and throws the same catchable Error; the` |
|       - | 2947 | `	 * Generator/Fiber pair used to take an older warn-only path here, so` |
|       - | 2948 | ``	 * `clone $gen` PRINTED an uncatchable diagnostic and then carried on with the`` |
|       - | 2949 | ``	 * ORIGINAL object standing in for the copy — the one shape of `clone` that`` |
|       - | 2950 | `	 * answered a value php never lets the program reach. */` |
|     394 | 2951 | `	if( (pSrc->pClass->iFlags & PH7_CLASS_ENUM) \|\| PH7_ClassIsUncloneable(pSrc->pClass)` |
|     332 | 2952 | `		\|\| pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|       - | 2953 | `		SyBlob sMsg;` |
|     132 | 2954 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     132 | 2955 | `		SyBlobFormat(&sMsg,"Trying to clone an uncloneable object of class %z",` |
|     130 | 2956 | `			&pSrc->pClass->sName);` |
|     132 | 2957 | `		rc = VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|     132 | 2958 | `		PH7_MemObjRelease(pTos);` |
|     132 | 2959 | `		pTos->nIdx = SXU32_HIGH;` |
|     132 | 2960 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 2961 | `			VM_EXIT_ABORT;` |
|       - | 2962 | `		}` |
|       - | 2963 | `		{` |
|       - | 2964 | `			sxi32 iRp;` |
|     132 | 2965 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      92 | 2966 | `				pc = iRp;` |
|      92 | 2967 | `				VM_EXIT_BREAK;` |
|       - | 2968 | `			}` |
|       - | 2969 | `		}` |
|      41 | 2970 | `		VM_EXIT_EXCEPTION;` |
|       - | 2971 | `	}` |
|       - | 2972 | `	/* Perform the clone operation */` |
|     269 | 2973 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|     269 | 2974 | `	PH7_MemObjRelease(pTos);` |
|     269 | 2975 | `	if( pClone == 0 ){` |
|     ! 0 | 2976 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 2977 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|     ! 0 | 2978 | `	}else{` |
|       - | 2979 | `		/* Load the cloned object */` |
|     269 | 2980 | `		pTos->x.pOther = pClone;` |
|     269 | 2981 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - | 2982 | `	}` |
|     269 | 2983 | `	VM_EXIT_BREAK;` |
|     ! 0 | 2984 | `	VM_EXIT_BREAK;` |
|     213 | 2985 | `}` |
|       - | 2986 |  |
