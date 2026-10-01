# src/ph7/vm_ops_oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1742/1953 lines (89.20%)

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
| 2133960 |   28 | `PH7_PRIVATE VmOpRc VmExecOpNew(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   29 | `{` |
| 2133965 |   30 | `	ph7_value *pTos = pState->pTos;` |
| 2133965 |   31 | `	ph7_value *pStack = pState->pStack;` |
| 2133965 |   32 | `	VmInstr *aInstr = pState->aInstr;` |
| 2133965 |   33 | `	sxi32 pc = pState->pc;` |
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
| 2133965 |   49 | `	int bScreenOnly = pInstr->iP1 < 0;` |
| 2694576 |   50 | `	sxi32 nCtorArgs = bScreenOnly ? 0` |
| 1627612 |   51 | `		: pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|       - |   52 | `	ph7_value *pArg;` |
| 2133965 |   53 | `	ph7_class *pClass = 0;` |
|       - |   54 | `	ph7_class_instance *pNew;` |
|       - |   55 | `	/* Resolving the class below can run an AUTOLOADER that throws. Snapshot the boundary` |
|       - |   56 | `	 * rail so the "not found" report can tell a missing class from an autoloader that` |
|       - |   57 | `	 * raised — php propagates the autoloader's exception and reports nothing else, while` |
|       - |   58 | `	 * this arm used to throw its own Error on top of it (uncaught, killing the script` |
|       - |   59 | `	 * right after the real exception had been handled). */` |
| 2133965 |   60 | `	sxi32 nNewBrc = pVm->nBoundaryRc;` |
| 2133965 |   61 | `	const void *pNewRes = (const void *)pVm->pResumeFrame;` |
| 2133965 |   62 | `	pArg = &pTos[-nCtorArgs]; /* Constructor arguments (if available) */` |
|       - |   63 | `	/* Same PHP 8.1 spread-key realignment as OP_CALL: build a per-actual-slot name` |
|       - |   64 | `	 * map when the ctor arg list unpacked string-keyed elements. NEW keeps the` |
|       - |   65 | `	 * class-name slot at pTos (above the args), so pArg is already the correct base;` |
|       - |   66 | `	 * the build also truncates this call's captured runs. */` |
|       - |   67 | `	VmCallArgMap sEffNewMap;` |
|       - |   68 | `	/* The screen pass has no arguments and no runs of its own: building (and` |
|       - |   69 | `	 * truncating) an effective map there would speak for the enclosing call. */` |
| 2694576 |   70 | `	VmCallArgMap *pEffNewMap = bScreenOnly ? 0 : VmEffCallArgMap(pVm,pInstr,pArg,` |
| 1121245 |   71 | `		nCtorArgs > 0 ? (sxu32)nCtorArgs : 0,&sEffNewMap);` |
| 3200956 |   72 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
| 2133945 |   73 | `		const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
| 2133945 |   74 | `		sxu32 nCls = SyBlobLength(&pTos->sBlob);` |
| 2133940 |   75 | `		if( (nCls == sizeof("self")-1 && SyMemcmp(zCls,"self",sizeof("self")-1) == 0)` |
| 2133924 |   76 | `		 \|\| (nCls == sizeof("static")-1 && SyMemcmp(zCls,"static",sizeof("static")-1) == 0)` |
| 2133876 |   77 | `		 \|\| (nCls == sizeof("parent")-1 && SyMemcmp(zCls,"parent",sizeof("parent")-1) == 0) ){` |
|       - |   78 | `			/* new self() / new static() / new parent(): resolve against the live` |
|       - |   79 | `			 * class context (LSB for static), sharing the FCC resolver. */` |
|      76 |   80 | `			pClass = VmFccResolveScope(&(*pVm),pTos);` |
|      76 |   81 | `			if( pClass && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT)) ){` |
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
|      40 |   93 | `		}else{` |
|       - |   94 | `			/* Try to extract the desired class */` |
| 2133873 |   95 | `			pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,` |
|       - |   96 | `				TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|       5 |   97 | `		}` |
| 1066970 |   98 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - |   99 | `		/* Take the base class from the loaded instance */` |
|       7 |  100 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       3 |  101 | `	}` |
| 2133965 |  102 | `	if( pClass == 0 ){` |
|       - |  103 | ``		/* php: `new NoSuch()` is a CATCHABLE Error ('Class "NoSuch" not found'), not a`` |
|       - |  104 | `		 * hard stop. Aborting here killed the script outright -- and with exit status 0,` |
|       - |  105 | `		 * so a caller could not even tell it had failed. */` |
|       - |  106 | `		SyBlob sErrM;` |
|       - |  107 | `		sxi32 rcErr;` |
|      60 |  108 | `		ph7_class *pNotNew = 0;` |
|      60 |  109 | `		if( PH7_VmClassLookupRaised(&(*pVm),nNewBrc,pNewRes) ){` |
|       - |  110 | `			/* The autoloader threw: land THAT (an in-place catch leaves 0 behind plus a` |
|       - |  111 | `			 * recorded resume frame, which the router picks up). */` |
|       7 |  112 | `			sxi32 rcAuto = pVm->nBoundaryRc;` |
|       7 |  113 | `			pVm->nBoundaryRc = 0;` |
|       7 |  114 | `			if( nCtorArgs > 0 ){` |
|     ! 0 |  115 | `				VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  116 | `			}` |
|       7 |  117 | `			PH7_MemObjRelease(pTos);` |
|       7 |  118 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       7 |  119 | `			pTos->nIdx = SXU32_HIGH;` |
|       7 |  120 | `			if( rcAuto == PH7_ABORT ){` |
|     ! 0 |  121 | `				VM_EXIT_ABORT;` |
|       - |  122 | `			}` |
|       7 |  123 | `			rc = PH7_EXCEPTION;` |
|       7 |  124 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  125 | `		}` |
|      54 |  126 | `		SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      54 |  127 | `		if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
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
|      62 |  141 | `		if( pNotNew && (pNotNew->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) ){` |
|       - |  142 | `			/* php names WHAT it will not instantiate; a trait is one of the three, and` |
|       - |  143 | `			 * PH7's loadable-only extract rejected it as "not found" — the one shape of` |
|       - |  144 | ``			 * `new` whose refusal did not say why. */`` |
|      25 |  145 | `			const char *zKind = (pNotNew->iFlags & PH7_CLASS_INTERFACE) ? "interface"` |
|      14 |  146 | `				: (pNotNew->iFlags & PH7_CLASS_TRAIT) ? "trait" : "abstract class";` |
|      17 |  147 | `			SyBlobFormat(&sErrM,"Cannot instantiate %s %z",zKind,&pNotNew->sName);` |
|      46 |  148 | `		}else if( (pTos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|       - |  149 | `` 			/* php refuses the OPERAND before it ever has a name to look up: a `new` `` |
|       - |  150 | `			 * takes an object or a string and nothing else, and every other value --` |
|       - |  151 | `			 * int, float, bool, null, array, resource -- is` |
|       - |  152 | ``			 * `Class name must be a valid object or a string`. PHL string-cast the`` |
|       - |  153 | ``			 * slot's raw blob instead, so all six answered `Class "" not found`, a`` |
|       - |  154 | `			 * sentence that names a class the program never wrote. (An EMPTY string` |
|       - |  155 | ``			 * really is `Class "" not found` in php, so the test is the TYPE.) The`` |
|       - |  156 | ``			 * `::` twin of this refusal is already at the bottom of VmExecOpMember. */`` |
|      13 |  157 | `			SyBlobAppend(&sErrM,"Class name must be a valid object or a string",` |
|       - |  158 | `				sizeof("Class name must be a valid object or a string")-1);` |
|       7 |  159 | `		}else{` |
|      26 |  160 | `			SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|      22 |  161 | `				SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob));` |
|       - |  162 | `		}` |
|       - |  163 | `		/* Settle the stack BEFORE throwing: the class-name slot becomes the (NULL)` |
|       - |  164 | `		 * expression result and the ctor arguments go. */` |
|      54 |  165 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  166 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  167 | `		}` |
|      54 |  168 | `		PH7_MemObjRelease(pTos);` |
|      54 |  169 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|      54 |  170 | `		pTos->nIdx = SXU32_HIGH;` |
|      79 |  171 | `		rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      25 |  172 | `			SyBlobLength(&sErrM));` |
|      54 |  173 | `		SyBlobRelease(&sErrM);` |
|      54 |  174 | `		if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      54 |  175 | `		rc = rcErr;` |
|      56 |  176 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
| 2133909 |  177 | `	}else if( pClass->iFlags & PH7_CLASS_NOINSTANTIATE ){` |
|       - |  178 | `		/* php refuses this one in the create_object handler, so the refusal names the` |
|       - |  179 | `		 * INSTANTIATION and never reaches the constructor — which matters because the` |
|       - |  180 | `		 * class may still declare a private __construct that Reflection prints. Same` |
|       - |  181 | `		 * shape as the enum reject below: no instance, no __destruct. */` |
|       - |  182 | `		SyBlob sErrMsg;` |
|      42 |  183 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      42 |  184 | `		if( pClass->zNewRefusal ){` |
|       - |  185 | `			/* php words a few of these per class — Directory's names dir() as the` |
|       - |  186 | `			 * way to get one — so the spec's own sentence wins when it has one. */` |
|      38 |  187 | `			SyBlobAppend(&sErrMsg,pClass->zNewRefusal,SyStrlen(pClass->zNewRefusal));` |
|      21 |  188 | `		}else{` |
|       5 |  189 | `			SyBlobFormat(&sErrMsg,"Instantiation of class %z is not allowed",&pClass->sName);` |
|       - |  190 | `		}` |
|       - |  191 | `		{` |
|      42 |  192 | `			const char *zRefCls = pClass->zNewRefusalClass ? pClass->zNewRefusalClass : "Error";` |
|      61 |  193 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),zRefCls,` |
|      38 |  194 | `				(sxu32)SyStrlen(zRefCls),&sErrMsg));` |
|       - |  195 | `		}` |
|      42 |  196 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  197 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  198 | `		}` |
|      42 |  199 | `		PH7_MemObjRelease(pTos);` |
|      42 |  200 | `		pTos->nIdx = SXU32_HIGH;` |
|      42 |  201 | `		VM_EXIT_BREAK;` |
| 2133871 |  202 | `	}else if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|       - |  203 | `		/* php 8.1: enums cannot be instantiated — a catchable Error, raised` |
|       - |  204 | `		 * BEFORE any construction (no instance, no __destruct). */` |
|       - |  205 | `		SyBlob sErrMsg;` |
|       9 |  206 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       9 |  207 | `		SyBlobFormat(&sErrMsg,"Cannot instantiate enum %z",&pClass->sName);` |
|       9 |  208 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       9 |  209 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  210 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  211 | `		}` |
|       9 |  212 | `		PH7_MemObjRelease(pTos);` |
|       9 |  213 | `		pTos->nIdx = SXU32_HIGH;` |
|       9 |  214 | `		VM_EXIT_BREAK;` |
| 2133858 |  215 | `	}else if( VmClassStaticDeferPending(pClass)` |
| 1066915 |  216 | `	       && (rc = PH7_VmMaterializeClassStatics(&(*pVm),pClass)) != SXRET_OK ){` |
|       - |  217 | `		/* php materializes the static table at instantiation too, so a default` |
|       - |  218 | `		 * that THREW at the declaration (or failed its type check) raises BEFORE` |
|       - |  219 | `		 * any construction (no instance, no __destruct) — same shape as the enum` |
|       - |  220 | `		 * reject above: park the status, settle the stack, and let the` |
|       - |  221 | `		 * fetch-point router land it. */` |
|       6 |  222 | `		VmBoundaryPark(&(*pVm),rc);` |
|       6 |  223 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  224 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  225 | `		}` |
|       6 |  226 | `		PH7_MemObjRelease(pTos);` |
|       6 |  227 | `		pTos->nIdx = SXU32_HIGH;` |
|       6 |  228 | `		VM_EXIT_BREAK;` |
|     ! 0 |  229 | `	}else{` |
|       - |  230 | `		ph7_class_method *pCons;` |
|       - |  231 | `		/* Check if a constructor is available — BEFORE instantiation: a` |
|       - |  232 | ``		 * visibility-denied `new` must not construct (nor later destruct)`` |
|       - |  233 | `		 * the object (band A #4). */` |
|       - |  234 | `		/* Only an explicit __construct is the constructor. PHP-4-style class-name` |
|       - |  235 | `		 * constructors were removed in PHP 8.0 — a method named like the class is a` |
|       - |  236 | `		 * plain method, so no same-name fallback here. */` |
| 2133859 |  237 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       - |  238 | `		/* Constructor visibility (band A #4): __construct now KEEPS its` |
|       - |  239 | `		 * declared protection (PH7_NewClassMethod no longer forces it` |
|       - |  240 | ``		 * public), so `new C()` on a private/protected constructor from`` |
|       - |  241 | `		 * the wrong scope is php's catchable Error. Reflection's` |
|       - |  242 | `		 * newInstance path sets bReflectBypass like method invoke. */` |
| 2133859 |  243 | `		if( pCons && pCons->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - |  244 | `			/* php binds non-public access by the member's DECLARING class, not the` |
|       - |  245 | `			 * instantiated class: a private constructor declared in a base is` |
|       - |  246 | `			 * reachable from that base's own methods even when instantiating a` |
|       - |  247 | ``			 * subclass (`new Child()` inside Base::factory()). __construct is a`` |
|       - |  248 | `			 * method, so pass its declaring class (sFunc.pUserData) — mirroring the` |
|       - |  249 | `			 * method-call visibility check — rather than pClass, whose hAttr lookup` |
|       - |  250 | `			 * for a method name always misses and falls to a wrong exact-class test. */` |
|      45 |  251 | `			ph7_class *pCtorDecl = pCons->sFunc.pUserData ? (ph7_class *)pCons->sFunc.pUserData : pClass;` |
|      45 |  252 | `			if( pVm->bReflectBypass ){` |
|     ! 0 |  253 | `				pVm->bReflectBypass = 0;` |
|      45 |  254 | `			}else if( !PH7_VmClassMemberAccess(&(*pVm),pCtorDecl,&pCons->sFunc.sName,pCons->iProtection,FALSE) ){` |
|       - |  255 | `				SyBlob sErrMsg;` |
|      29 |  256 | `				const char *zVis = pCons->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       - |  257 | `				/* php NAMES the calling scope when there is one (see the method twin in` |
|       - |  258 | `				 * OP_CALL); "global scope" is only for code outside every class. */` |
|      29 |  259 | `				ph7_class *pCtorScope = PH7_VmCallerScope(&(*pVm));` |
|      29 |  260 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      29 |  261 | `				if( pCtorScope ){` |
|       9 |  262 | `					SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from scope %z",` |
|       4 |  263 | `						zVis,&pClass->sName,&pCtorScope->sName);` |
|       5 |  264 | `				}else{` |
|      21 |  265 | `					SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from global scope",` |
|      10 |  266 | `						zVis,&pClass->sName);` |
|       - |  267 | `				}` |
|      29 |  268 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       - |  269 | `				/* Pop ctor args + release the class-name operand, leave NULL` |
|       - |  270 | `				 * as the expression value; the fetch-point router lands the` |
|       - |  271 | `				 * throw. No instance was created. */` |
|      29 |  272 | `				if( nCtorArgs > 0 ){` |
|     ! 0 |  273 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  274 | `				}` |
|      29 |  275 | `				PH7_MemObjRelease(pTos);` |
|      29 |  276 | `				pTos->nIdx = SXU32_HIGH;` |
|      29 |  277 | `				VM_EXIT_BREAK;` |
|       - |  278 | `			}` |
|       8 |  279 | `		}` |
| 2133831 |  280 | `		if( bScreenOnly ){` |
|       - |  281 | `			/* Every refusal above has been asked. Leave the class name standing for` |
|       - |  282 | `			 * the real pass and let the arguments run. */` |
| 1012694 |  283 | `			VM_EXIT_BREAK;` |
|       - |  284 | `		}` |
| 1121142 |  285 | `		if( nCtorArgs > 0 ){` |
|       - |  286 | ``			/* D1: a `new`'s arguments are ARGUMENTS. An element/property one whose`` |
|       - |  287 | `			 * target was absent at load time rides a deferred carrier that only OP_CALL` |
|       - |  288 | `			 * resolved, and the ctor call reaches its callee by pointer rather than` |
|       - |  289 | ``			 * through that opcode — so `new C($a['missing'])` passed a silent NULL where`` |
|       - |  290 | ``			 * php warns, and a by-REFERENCE ctor parameter (`new C($a['fresh'])` with`` |
|       - |  291 | ``			 * `__construct(&$x)`) never vivified the target at all: php writes the`` |
|       - |  292 | `			 * element, PHL left it uncreated. Resolve here, where the constructor is` |
|       - |  293 | `			 * known and pVm->pFrame is still the caller, exactly as every OP_CALL` |
|       - |  294 | `			 * dispatch branch does. A class with NO constructor still resolves — the` |
|       - |  295 | `			 * arguments were evaluated and php reports what reading them found. */` |
| 1012740 |  296 | `			ph7_class_method *pCtorArgs = pCons;` |
|       - |  297 | `			sxi32 rcDA;` |
| 1012740 |  298 | `			if( pCtorArgs == 0 ){` |
|       3 |  299 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pEffNewMap);` |
| 1012739 |  300 | `			}else if( pCtorArgs->sFunc.iFlags & VM_FUNC_NATIVE ){` |
|       - |  301 | `				/* A native constructor has no compiled formals; its by-ref positions` |
|       - |  302 | `				 * come from the signature-derived mask, the builtin way. */` |
| 1011927 |  303 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
| 1011941 |  304 | `					pCtorArgs->sFunc.pNative ? pCtorArgs->sFunc.pNative->nByRefMask : 0,0,0,pEffNewMap);` |
|  505966 |  305 | `			}else{` |
|    1193 |  306 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|     792 |  307 | `					(ph7_vm_func_arg *)SySetBasePtr(&pCtorArgs->sFunc.aArgs),` |
|     396 |  308 | `					SySetUsed(&pCtorArgs->sFunc.aArgs),0,0,0,pEffNewMap);` |
|       - |  309 | `			}` |
| 1012740 |  310 | `			if( rcDA == PH7_ABORT \|\| rcDA == PH7_EXCEPTION ){` |
|       - |  311 | `				/* Reading an argument raised (a string-offset Error, a magic accessor's` |
|       - |  312 | `				 * throw): no object is created, and the stack is tidied exactly as the` |
|       - |  313 | `				 * constructor-throw path below tidies it. */` |
|       - |  314 | `				sxi32 iResumeDA;` |
|       3 |  315 | `				if( rcDA == PH7_ABORT ){` |
|     ! 0 |  316 | `					VM_EXIT_ABORT;` |
|       - |  317 | `				}` |
|       3 |  318 | `				if( VmRecordedResume(pVm,&iResumeDA,pState->pEntryFrame,aInstr) ){` |
|     ! 0 |  319 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  320 | `					PH7_MemObjRelease(pTos);` |
|     ! 0 |  321 | `					PH7_RESUME_DRAIN()` |
|     ! 0 |  322 | `					pc = iResumeDA;` |
|     ! 0 |  323 | `					VM_EXIT_BREAK;` |
|       - |  324 | `				}` |
|       3 |  325 | `				VM_EXIT_EXCEPTION;` |
|       - |  326 | `			}` |
|  506357 |  327 | `		}` |
|       - |  328 | `		/* Create a new class instance */` |
| 1121140 |  329 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
| 1121140 |  330 | `		if( pNew == 0 ){` |
|     ! 0 |  331 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  332 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|     ! 0 |  333 | `				&pClass->sName` |
|       - |  334 | `			);` |
|     ! 0 |  335 | `			PH7_MemObjRelease(pTos);` |
|     ! 0 |  336 | `			if( nCtorArgs > 0 ){` |
|       - |  337 | `				/* Pop given arguments */` |
|     ! 0 |  338 | `				VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  339 | `			}` |
|     ! 0 |  340 | `			VM_EXIT_BREAK;` |
|       - |  341 | `		}` |
| 1121140 |  342 | `		if( pVm->nBoundaryRc != 0 ){` |
|       - |  343 | `			/* A typed property DEFAULT failed its type check during instance-` |
|       - |  344 | `			 * frame creation (parked catchable TypeError): php aborts the` |
|       - |  345 | `			 * construction — no constructor call, no object. Consume the` |
|       - |  346 | `			 * parked status here and route exactly like a constructor throw` |
|       - |  347 | `			 * (the exception object is already registered). */` |
|      38 |  348 | `			sxi32 rcDef = pVm->nBoundaryRc;` |
|       - |  349 | `			sxi32 iDefResumePc;` |
|      38 |  350 | `			pVm->nBoundaryRc = 0;` |
|       - |  351 | `			/* php never reaches a __destruct here either: object_init_ex evaluates the` |
|       - |  352 | `			 * defaults BEFORE the object exists, so a failing one leaves nothing to` |
|       - |  353 | `			 * destruct. PHL builds the instance first, so it marks it instead. */` |
|      38 |  354 | `			PH7_ClassInstanceCtorFailed(pNew);` |
|      38 |  355 | `			PH7_ClassInstanceUnref(pNew);` |
|      38 |  356 | `			if( rcDef == PH7_ABORT ){` |
|     ! 0 |  357 | `				VM_EXIT_ABORT;` |
|       - |  358 | `			}` |
|      38 |  359 | `			if( VmRecordedResume(pVm,&iDefResumePc,pState->pEntryFrame,aInstr) ){` |
|       - |  360 | `				/* This frame's own try caught it in-place: tidy the stack` |
|       - |  361 | `				 * (pop ctor args + release the class-name slot, then drain any` |
|       - |  362 | `				 * abandoned outer-expression operands to the try's base — the` |
|       - |  363 | `				 * class-name slot itself sits above it) and resume. */` |
|      38 |  364 | `				if( nCtorArgs > 0 ){` |
|       3 |  365 | `					VmPopOperand(&pTos,nCtorArgs);` |
|       1 |  366 | `				}` |
|      38 |  367 | `				PH7_MemObjRelease(pTos);` |
|      76 |  368 | `				PH7_RESUME_DRAIN()` |
|      38 |  369 | `				pc = iDefResumePc;` |
|      38 |  370 | `				VM_EXIT_BREAK;` |
|       - |  371 | `			}` |
|     ! 0 |  372 | `			VM_EXIT_EXCEPTION;` |
|       - |  373 | `		}` |
| 1121104 |  374 | `		if( pCons ){` |
|       - |  375 | `			/* Call the class constructor.  Collect args in stack order and` |
|       - |  376 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|       - |  377 | `			 * receiving OP_CALL path runs its named-argument matching` |
|       - |  378 | `			 * (including variadic string-key packing). */` |
| 1115022 |  379 | `			VmCallArgMap *pNewMap = pEffNewMap;` |
|       - |  380 | `			sxi32 rcCons;` |
|       - |  381 | `			SySet aArg; /* ctor argument scratch (the loop's shared set stays with OP_CALL) */` |
| 1115022 |  382 | `			SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
| 2130341 |  383 | `			while( pArg < pTos ){` |
| 1015324 |  384 | `				SySetPut(&aArg,(const void *)&pArg);` |
| 1015324 |  385 | `				pArg++;` |
|       5 |  386 | `			}` |
|       - |  387 | `			/* Too-few-arguments is php's catchable ArgumentCountError, raised` |
|       - |  388 | `			 * by the shared OP_CALL install path this ctor call routes through` |
|       - |  389 | `			 * (was a PHL-only notice here). */` |
| 1115022 |  390 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
| 1115022 |  391 | `			SySetRelease(&aArg); /* scratch dies here, on every exit path */` |
|       - |  392 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
| 1115022 |  393 | `			if( pNew->iRef < 1 ){` |
|     ! 0 |  394 | `				pNew->iRef = 1;` |
|     ! 0 |  395 | `			}` |
| 1115022 |  396 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|       - |  397 | `				/* The constructor raised: the half-constructed object must not` |
|       - |  398 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|       - |  399 | `				 * The class-name operand (and any leftover args) are released by` |
|       - |  400 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|       - |  401 | `				sxi32 iResumePc;` |
|       - |  402 | `				/* php's ctor-failed mark: no __destruct for an object whose` |
|       - |  403 | `				 * constructor threw, here or ever (see PH7_ClassInstanceCtorFailed). */` |
|  100858 |  404 | `				PH7_ClassInstanceCtorFailed(pNew);` |
|  100858 |  405 | `				PH7_ClassInstanceUnref(pNew);` |
|  100858 |  406 | `				if( rcCons == PH7_ABORT ){` |
|      10 |  407 | `					VM_EXIT_ABORT;` |
|       - |  408 | `				}` |
|  100850 |  409 | `				if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){` |
|       - |  410 | `					/* This frame's own try caught it in-place: tidy the stack` |
|       - |  411 | `					 * (pop ctor args + release the class-name slot, then drain any` |
|       - |  412 | `					 * abandoned outer-expression operands to the try's base — the` |
|       - |  413 | `					 * class-name slot itself sits above it) and resume. */` |
|  100574 |  414 | `					if( nCtorArgs > 0 ){` |
|     562 |  415 | `						VmPopOperand(&pTos,nCtorArgs);` |
|     279 |  416 | `					}` |
|  100574 |  417 | `					PH7_MemObjRelease(pTos);` |
|  501032 |  418 | `					PH7_RESUME_DRAIN()` |
|  100574 |  419 | `					pc = iResumePc;` |
|  100574 |  420 | `					VM_EXIT_BREAK;` |
|       - |  421 | `				}` |
|     279 |  422 | `				VM_EXIT_EXCEPTION;` |
|       - |  423 | `			}` |
|  507073 |  424 | `		}` |
| 1020251 |  425 | `		if( nCtorArgs > 0 ){` |
|       - |  426 | `			/* Pop given arguments */` |
| 1011907 |  427 | `			VmPopOperand(&pTos,nCtorArgs);` |
|  505942 |  428 | `		}` |
| 1020251 |  429 | `		PH7_MemObjRelease(pTos);` |
| 1020251 |  430 | `		pTos->x.pOther = pNew;` |
| 1020251 |  431 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - |  432 | `	}` |
| 1020251 |  433 | `	VM_EXIT_BREAK;` |
|     ! 0 |  434 | `	VM_EXIT_BREAK;` |
| 1066964 |  435 | `}` |
|       - |  436 |  |
|       - |  437 | `/*` |
|       - |  438 | `` * Is this OP_MEMBER the SOURCE of a reference bind — `$r =& $o->p`, `$a[] =& $o->p`,`` |
|       - |  439 | `` * `[&$o->p]`, a by-reference `foreach ($o->p as &$v)`, a by-reference destructuring`` |
|       - |  440 | `` * source? php compiles all of them with `zend_compile_var(source, BP_VAR_W)`, so the`` |
|       - |  441 | ` * fetch is a WRITE context: a missing property is CREATED, not warned about. PHL` |
|       - |  442 | ` * leaves the instruction a PH7_MEMBER_READ on purpose -- a handler-backed property` |
|       - |  443 | ` * still hands back a copy there, and an overloaded one still dispatches __get -- and` |
|       - |  444 | ` * carries the context in a flag the compiler stamps beside it.` |
|       - |  445 | ` */` |
|  323759 |  446 | `static int VmMemberFetchIsRefSource(const VmInstr *pInstr)` |
|       5 |  447 | `{` |
|  323764 |  448 | `	return pInstr->iP2 == PH7_MEMBER_READ && pInstr->bRefSrc != 0;` |
|       5 |  449 | `}` |
|       - |  450 | `/*` |
|       - |  451 | ` * Is this OP_MEMBER fetching the property for WRITING — php's BP_VAR_W / BP_VAR_RW` |
|       - |  452 | ` * fetch, the one that asks the object for something to MODIFY? The compiler tags` |
|       - |  453 | `` * the base of a subscript-write / `??=` PH7_MEMBER_WRITE, so that answers most of`` |
|       - |  454 | ``  * it once the shapes that share the tag are excluded: a plain store and a `??=` `` |
|       - |  455 | ` * write through their own paths, and a direct read-modify-write is the accessor` |
|       - |  456 | ` * pair (VmMagicRmwArm), not a write fetch. The shapes php compiles as plain READS —` |
|       - |  457 | ` * a reference SOURCE, in all its spellings — carry the compiler's own marker` |
|       - |  458 | `` * (VmMemberFetchIsRefSource). `$o->p =& $x` is NOT one of them: the member is`` |
|       - |  459 | ` * the reference TARGET there and carries its own iP2.` |
|       - |  460 | ` */` |
|  100766 |  461 | `static int VmMemberFetchForWrite(const VmInstr *pInstr)` |
|       5 |  462 | `{` |
|  100771 |  463 | `	const VmInstr *pNext = pInstr + 1;` |
|  100771 |  464 | `	if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|     533 |  465 | `		int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);` |
|     533 |  466 | `		int bCoalesceW = (pNext->iOp == PH7_OP_NULLC_JMP);` |
|     533 |  467 | `		return !bPlainStore && !bCoalesceW && !VmMemberNextIsRmw(pNext);` |
|       - |  468 | `	}` |
|  100243 |  469 | `	return VmMemberFetchIsRefSource(pInstr);` |
|   50388 |  470 | `}` |
|       - |  471 | `/*` |
|       - |  472 | ` * A native class's property is a field of php's own C struct, and the only writes` |
|       - |  473 | ` * that reach one are the writes the store filter converts (ph7_class::xSet). So` |
|       - |  474 | ` * the fetched value keeps its slot index for exactly the shapes that end in such` |
|       - |  475 | `` * a store -- a plain assignment, `??=`, a destructuring target and the`` |
|       - |  476 | ` * read-modify-write forms -- and is a TEMPORARY for every other use. That is` |
|       - |  477 | ` * php's own answer: it has no ptr_ptr handler for such a property, so a reference` |
|       - |  478 | `` * bind (`$r = &$i->f`), a by-reference argument (`preg_match($p,$s,$i->s)`) and a`` |
|       - |  479 | ` * by-reference foreach all get a copy whose writes are SILENTLY lost -- silently,` |
|       - |  480 | ` * unlike the overloaded case, which php has a notice for.` |
|       - |  481 | ` */` |
|   10037 |  482 | `static int VmMemberNativeSetKeepsSlot(const VmInstr *pInstr)` |
|       5 |  483 | `{` |
|   10042 |  484 | `	if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|       - |  485 | ``		/* Every fetch the compiler tags for writing: a plain store, `??=`, a`` |
|       - |  486 | `		 * compound assign, and the base of a subscript write — that last one has` |
|       - |  487 | ``		 * to reach the real value so `$i->y[0] = 5` is php's "Cannot use a scalar`` |
|       - |  488 | `		 * value as an array" rather than a write nobody notices. */` |
|    1400 |  489 | `		return 1;` |
|       - |  490 | `	}` |
|    8646 |  491 | `	if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       7 |  492 | `		return 1;   /* list()/foreach destructuring target */` |
|       - |  493 | `	}` |
|    8640 |  494 | `	return VmMemberNextIsWrite(pInstr + 1);   /* ++/-- and the compound assigns */` |
|    5024 |  495 | `}` |
|       - |  496 | `/*` |
|       - |  497 | `` * `$doc->encoding ??= 'UTF-8'` on a name a native class's own property-handler`` |
|       - |  498 | ` * table carries.` |
|       - |  499 | ` *` |
|       - |  500 | ` * The member opcode's handler gate answers a READ and can only REFUSE a write,` |
|       - |  501 | ` * because the value does not exist yet -- and a coalesce that finds null must` |
|       - |  502 | ` * make one. Only the overloaded-coalesce rail carries a pending store to the` |
|       - |  503 | ` * point the value arrives, so this shape is left to it: the read it makes goes` |
|       - |  504 | ` * through the same handler anyway.` |
|       - |  505 | ` */` |
|    7173 |  506 | `static int VmMemberCoalOwned(ph7_class_instance *pThis,const VmInstr *pInstr,const SyString *pName)` |
|       5 |  507 | `{` |
|    7570 |  508 | `	return pInstr->iP2 == PH7_MEMBER_WRITE` |
|    3979 |  509 | `	    && pInstr[1].iOp == PH7_OP_NULLC_JMP` |
|    7565 |  510 | `	    && PH7_ClassNativePropOwns(pThis,pName);` |
|       5 |  511 | `}` |
|       - |  512 | `/*` |
|       - |  513 | `` * php's `Indirect modification of overloaded property C::$p has no effect`: the`` |
|       - |  514 | ` * write-context fetch above landed on a property only __get answers for, so what` |
|       - |  515 | ` * comes back is a VALUE and whatever the rest of the expression writes into it is` |
|       - |  516 | ` * thrown away. php says so and carries on.` |
|       - |  517 | ` *` |
|       - |  518 | ` * PHL had the notice at one site only (a by-reference ARGUMENT, VmBindPropByRef)` |
|       - |  519 | ` * and, worse, the value was not a copy: __get's return still shared the object's` |
|       - |  520 | `` * own nested hashmap by COW, so `$o->a['b'] = 9`, `$o->a[] = 5` and a by-reference`` |
|       - |  521 | ` * foreach modified the object php leaves untouched. Separating it here is what` |
|       - |  522 | ` * makes the write land nowhere.` |
|       - |  523 | ` *` |
|       - |  524 | ` * An ENGINE __get is not this — php routes those through the class's own property` |
|       - |  525 | ` * handler, which hands back the real element (ArrayObject's ARRAY_AS_PROPS reads` |
|       - |  526 | ` * and writes through, in both engines). php stays silent for an OBJECT value too:` |
|       - |  527 | ` * a handle is shared, so nothing about the write is indirect.` |
|       - |  528 | ` */` |
|      32 |  529 | `PH7_PRIVATE void PH7_VmOverloadedPropNotice(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,ph7_value *pVal)` |
|       1 |  530 | `{` |
|      33 |  531 | `	ph7_class_method *pGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|      33 |  532 | `	if( pGet == 0 \|\| (pGet->sFunc.iFlags & VM_FUNC_NATIVE) ){` |
|     ! 0 |  533 | `		return;` |
|       - |  534 | `	}` |
|      33 |  535 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       3 |  536 | `		return;` |
|       - |  537 | `	}` |
|      46 |  538 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - |  539 | `		"Indirect modification of overloaded property %z::$%z has no effect",` |
|      15 |  540 | `		&pClass->sName,pName);` |
|      31 |  541 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      25 |  542 | `		PH7_HashmapCowSeparate(&(*pVm),pVal);` |
|      12 |  543 | `	}` |
|      17 |  544 | `}` |
|       - |  545 | `/*` |
|       - |  546 | ` * Does an OVERLOADED read-modify-write apply to this property access? php runs` |
|       - |  547 | ` * one only when the class answers BOTH sides; the guard means we are already` |
|       - |  548 | ` * inside this property's own __get, where php behaves as if the accessor were` |
|       - |  549 | ` * absent.` |
|       - |  550 | ` */` |
|      44 |  551 | `static int VmMagicRmwEligible(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,const SyString *pName)` |
|       2 |  552 | `{` |
|      46 |  553 | `	if( PH7_ClassNativePropOwns(pThis,pName) ){` |
|       - |  554 | `		/* A native class's own property handler answers BOTH halves -- php reads` |
|       - |  555 | ``		 * `$doc->version .= '.1'` through read_property and writes the result back`` |
|       - |  556 | `		 * through write_property, with no magic accessor involved either way. */` |
|       5 |  557 | `		return 1;` |
|       - |  558 | `	}` |
|      56 |  559 | `	return PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) != 0` |
|      34 |  560 | `	    && PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1) != 0` |
|      54 |  561 | `	    && !VmMagicGuardHeld(pVm,(void *)pThis,pName,'g');` |
|      24 |  562 | `}` |
|       - |  563 | `/*` |
|       - |  564 | `` * php's read-modify-write of an OVERLOADED property — `$o->n++`, `--$o->n`,`` |
|       - |  565 | `` * `$o->n += 2`, `$o->n .= 'x'` on a name the instance does not expose. php reads`` |
|       - |  566 | ` * the current value through __get, lets the modify op compute on it, and writes` |
|       - |  567 | ` * the result back through __set; PHL fell through to dynamic-property creation` |
|       - |  568 | ` * instead, so the ordinary shape (a class declaring BOTH accessors) died on` |
|       - |  569 | `` * `Cannot create dynamic property C::$n` — or, when the name IS declared but`` |
|       - |  570 | `` * inaccessible from this scope, on `Cannot access private property C::$n` —`` |
|       - |  571 | ` * for a statement php runs.` |
|       - |  572 | ` *` |
|       - |  573 | ` * The write-back rides the same pending-entry rail property HOOKS use: the` |
|       - |  574 | ` * current value goes into a fresh SCRATCH memobj the modify op mutates in` |
|       - |  575 | ` * place, and the entry makes that op's tail (PH7_HOOK_RMW_WRITEBACK) dispatch` |
|       - |  576 | ` * __set with the computed value.` |
|       - |  577 | ` *` |
|       - |  578 | ` * BOTH accessors are required, which is php's own split: with only __get php` |
|       - |  579 | ` * goes on to CREATE the property (PHL's §10 policy refuses a dynamic property),` |
|       - |  580 | `` * and with only __set it warns `Undefined property` and reads null — neither is`` |
|       - |  581 | ` * this path.` |
|       - |  582 | ` *` |
|       - |  583 | ` * *pOut takes __get's value and *pnScratch the slot the modify op must address;` |
|       - |  584 | ` * the caller does its own stack surgery afterwards, because pName still aliases` |
|       - |  585 | `` * the NAME operand when the name is dynamic (`$o->$k++`) and releasing that`` |
|       - |  586 | ` * operand first would leave it dangling. *pnScratch stays SXU32_HIGH when __get` |
|       - |  587 | ` * threw (nothing is armed — the fetch-point router lands the parked throw and` |
|       - |  588 | ` * php's __set never runs) or when the scratch reservation failed.` |
|       - |  589 | ` */` |
|      32 |  590 | `static void VmMagicRmwArm(` |
|       - |  591 | `	ph7_vm *pVm,` |
|       - |  592 | `	ph7_class_instance *pThis,` |
|       - |  593 | `	ph7_class *pClass,` |
|       - |  594 | `	const SyString *pName,` |
|       - |  595 | `	ph7_value *pOut,` |
|       - |  596 | `	sxu32 *pnScratch,` |
|       - |  597 | `	void *pOwnerStack,` |
|       - |  598 | `	void *pInstrs,` |
|       - |  599 | `	sxu32 nPc` |
|       - |  600 | `	)` |
|       1 |  601 | `{` |
|       - |  602 | `	ph7_value *pScr;` |
|       - |  603 | `	VmHookRmw sRmw;` |
|      33 |  604 | `	*pnScratch = SXU32_HIGH;` |
|      33 |  605 | `	VmMagicGuardPush(pVm,(void *)pThis,pName,'g');` |
|      33 |  606 | `	PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,pName,pOut);` |
|      33 |  607 | `	VmMagicGuardPop(pVm);` |
|      33 |  608 | `	if( pVm->nBoundaryRc != 0 ){` |
|       3 |  609 | `		PH7_MemObjRelease(pOut);` |
|       3 |  610 | `		return;` |
|       - |  611 | `	}` |
|      31 |  612 | `	pScr = PH7_ReserveMemObj(&(*pVm));` |
|      31 |  613 | `	if( pScr == 0 ){` |
|       - |  614 | `		/* OOM: loud allocator diagnostics already fired; the value still stands. */` |
|     ! 0 |  615 | `		return;` |
|       - |  616 | `	}` |
|      31 |  617 | `	PH7_MemObjStore(pOut,pScr);` |
|      31 |  618 | `	sRmw.iKind = VM_HOOK_PEND_RMW_MAGIC;` |
|      31 |  619 | `	sRmw.pThis = pThis;` |
|      31 |  620 | `	sRmw.pAttr = 0;` |
|      31 |  621 | `	sRmw.nBackIdx = SXU32_HIGH;` |
|      31 |  622 | `	sRmw.nScratchIdx = pScr->nIdx;` |
|      31 |  623 | `	SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      31 |  624 | `	SyBlobAppend(&sRmw.sName,(const void *)pName->zString,pName->nByte);` |
|      31 |  625 | `	sRmw.pOwnerStack = pOwnerStack;` |
|      31 |  626 | `	sRmw.pInstrs = pInstrs;` |
|      31 |  627 | `	sRmw.nJmpPc = nPc;  /* the modify op ... */` |
|      31 |  628 | `	sRmw.nPc = nPc;     /* ... is the whole window */` |
|      31 |  629 | `	pThis->iRef++;` |
|      31 |  630 | `	SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      31 |  631 | `	*pnScratch = pScr->nIdx;` |
|      17 |  632 | `}` |
|       - |  633 | `/*` |
|       - |  634 | `` * The property php's `C::$name` form finds. That form reads the class's whole`` |
|       - |  635 | ` * property table -- instance properties and constants included, answering on` |
|       - |  636 | ` * visibility before static-ness (see the arm that uses it) -- and php's table` |
|       - |  637 | `` * carries a base's PRIVATE instance property as a SHADOW entry, so `B::$q` on`` |
|       - |  638 | `` * `class A { private $q; }` is "Cannot access private property B::$q" and not the`` |
|       - |  639 | ` * undeclared-static sentence. Here that member lives under its mangled STORAGE` |
|       - |  640 | ` * name, which the plain probe cannot see, so the ancestry answers for it: the` |
|       - |  641 | ` * nearest base that declares one under the plain name.` |
|       - |  642 | ` */` |
|  101768 |  643 | `static ph7_class_attr * VmClassAttrWithShadow(ph7_class *pClass,const char *zName,sxu32 nName)` |
|       5 |  644 | `{` |
|       - |  645 | `	ph7_class *pWalk;` |
|  101773 |  646 | `	ph7_class_attr *pAttr = PH7_ClassExtractAttribute(pClass,zName,nName);` |
|  101773 |  647 | `	if( pAttr ){` |
|  101765 |  648 | `		return pAttr;` |
|       - |  649 | `	}` |
|      10 |  650 | `	for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){` |
|     ! 0 |  651 | `		pAttr = PH7_ClassExtractAttribute(pWalk,zName,nName);` |
|     ! 0 |  652 | `		if( pAttr ){` |
|     ! 0 |  653 | `			return (pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|     ! 0 |  654 | `			     && (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0)` |
|     ! 0 |  655 | `				? pAttr : 0;` |
|       - |  656 | `		}` |
|     ! 0 |  657 | `	}` |
|      10 |  658 | `	return 0;` |
|   50889 |  659 | `}` |
|       - |  660 | `/*` |
|       - |  661 | ` * May the executing scope touch this class-level member reached through an` |
|       - |  662 | `` * INSTANCE (`$o->s` on a static, or on a class constant)?`` |
|       - |  663 | ` *` |
|       - |  664 | ` * The ordinary attribute rule, plus php's shadow re-lookup: when the member the` |
|       - |  665 | ` * object's class holds under this name is a private one the scope may not touch,` |
|       - |  666 | ` * php asks the SCOPE's own table for the name and uses what it finds there. With` |
|       - |  667 | `` * `class A { private static $q; } class B extends A { private static $q; }` that`` |
|       - |  668 | `` * is how `$b->q` from inside A reaches A's own -- the as-non-static notice and`` |
|       - |  669 | ` * then the ordinary undefined-property answer, rather than a visibility refusal` |
|       - |  670 | ` * about B's.` |
|       - |  671 | ` */` |
|      24 |  672 | `static int VmStaticThroughInstanceVisible(ph7_vm *pVm,ph7_class *pClass,` |
|       - |  673 | `	ph7_class_attr *pAttr,const SyString *pName)` |
|       3 |  674 | `{` |
|       - |  675 | `	ph7_class *pScope;` |
|       - |  676 | `	ph7_class_attr *pShadow;` |
|      27 |  677 | `	if( PH7_VmClassAttrAccess(&(*pVm),pClass,pAttr,FALSE) ){` |
|      20 |  678 | `		return 1;` |
|       - |  679 | `	}` |
|       8 |  680 | `	if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE \|\| pName->nByte < 1 ){` |
|     ! 0 |  681 | `		return 0;` |
|       - |  682 | `	}` |
|       8 |  683 | `	pScope = PH7_VmCallerScope(&(*pVm));` |
|       8 |  684 | `	if( pScope == 0 \|\| !PH7_VmInstanceOf(pClass,pScope) ){` |
|       8 |  685 | `		return 0;` |
|       - |  686 | `	}` |
|     ! 0 |  687 | `	pShadow = PH7_ClassExtractAttribute(pScope,pName->zString,pName->nByte);` |
|     ! 0 |  688 | `	return pShadow != 0` |
|     ! 0 |  689 | `		&& pShadow != pAttr` |
|     ! 0 |  690 | `		&& pShadow->iProtection == PH7_CLASS_PROT_PRIVATE` |
|     ! 0 |  691 | `		&& PH7_VmMemberOwnerClass(pShadow->pDeclClass,pScope) == pScope;` |
|      15 |  692 | `}` |
|       - |  693 | `/*` |
|       - |  694 | ` * OP_MEMBER: body moved verbatim from the OP_MEMBER arm of` |
|       - |  695 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  696 | ` */` |
|  385554 |  697 | `PH7_PRIVATE VmOpRc VmExecOpMember(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  698 | `{` |
|  385559 |  699 | `	ph7_value *pTos = pState->pTos;` |
|  385559 |  700 | `	ph7_value *pStack = pState->pStack;` |
|  385559 |  701 | `	VmInstr *aInstr = pState->aInstr;` |
|  385559 |  702 | `	sxi32 pc = pState->pc;` |
|       - |  703 | `	sxi32 rc;` |
|       - |  704 | `	ph7_class_instance *pThis;` |
|       - |  705 | `	ph7_value *pNos;` |
|       - |  706 | `	SyString sName;` |
|  385559 |  707 | ``	int bStaticHidden = 0; /* a `::$name` already refused for VISIBILITY */`` |
|  385559 |  708 | `	if( !pInstr->iP1 ){` |
|  277880 |  709 | `		pNos = &pTos[-1];` |
|       - |  710 | `#ifdef UNTRUST` |
|       - |  711 | `		if( pNos < pStack ){` |
|       - |  712 | `			VM_EXIT_ABORT;` |
|       - |  713 | `		}` |
|       - |  714 | `#endif` |
|       - |  715 | `		/* What a dynamic NAME may BE, decided before the receiver is looked at --` |
|       - |  716 | `		 * php settles this in the opcode handler's first lines, and the two name` |
|       - |  717 | `		 * positions settle it differently.` |
|       - |  718 | `		 *` |
|       - |  719 | `		 * A METHOD name must ALREADY be a string: zend's INIT_METHOD_CALL refuses` |
|       - |  720 | ``		 * anything else outright with `Method name must be a string`, before the`` |
|       - |  721 | `		 * receiver's type, before __call, and before the name would be looked up.` |
|       - |  722 | `		 * A PROPERTY name is COERCED instead, with the user-visible rules -- an` |
|       - |  723 | ``		 * array warns `Array to string conversion` and renders "Array", a float,`` |
|       - |  724 | `		 * bool or null spells itself out, an object hands over its __toString()` |
|       - |  725 | ``		 * and one without it is php's catchable `Object of class X could not be`` |
|       - |  726 | ``		 * converted to string`.`` |
|       - |  727 | `		 *` |
|       - |  728 | `		 * PHL read the name slot's RAW BLOB, which is empty for every value that` |
|       - |  729 | ``		 * is not already a string, so `$o->{5}`, `$o->{1.5}`, `$o->{true}` and`` |
|       - |  730 | ``		 * `$o->{$stringable}` all named the property "" -- one shared property per`` |
|       - |  731 | `		 * object, silently, on every access shape (read, write, isset, unset,` |
|       - |  732 | ``		 * increment, by-ref) -- and `$o->{[1]}` on an object with no such property`` |
|       - |  733 | `		 * SEGFAULTED, because an empty blob hands out a NULL pointer that the` |
|       - |  734 | `		 * dynamic-property path dereferences.` |
|       - |  735 | `		 *` |
|       - |  736 | `		 * php's own exception is the one shape that answers before it ever asks` |
|       - |  737 | ``		 * for the name: a lookup (isset/empty/`??`) or an unset() whose receiver`` |
|       - |  738 | `		 * is not an object short-circuits, so no coercion and no diagnostic. A` |
|       - |  739 | ``		 * `?->` on null never reaches here at all -- OP_NULLSAFE_JMP has already`` |
|       - |  740 | `		 * jumped past both the name expression and this op.` |
|       - |  741 | `		 */` |
|  277880 |  742 | `		if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|  155841 |  743 | `			if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  744 | `				sxi32 rcMn;` |
|      11 |  745 | `				VmPopOperand(&pTos,1);` |
|      11 |  746 | `				PH7_MemObjRelease(pTos);` |
|      11 |  747 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      11 |  748 | `				pTos->nIdx = SXU32_HIGH;` |
|      11 |  749 | `				rcMn = VmThrowFromVm(&(*pVm),"Error","Method name must be a string",` |
|       - |  750 | `					sizeof("Method name must be a string")-1);` |
|      11 |  751 | `				if( rcMn == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 |  752 | `				rc = rcMn;` |
|      11 |  753 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  754 | `			}` |
|  199912 |  755 | `		}else if( (pNos->iFlags & MEMOBJ_OBJ)` |
|   61105 |  756 | `		       \|\| (pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2)) ){` |
|  122010 |  757 | `			sxi32 rcNm = PH7_MemObjToStringUV(pTos);` |
|  122010 |  758 | `			if( rcNm != SXRET_OK ){` |
|       3 |  759 | `				VmPopOperand(&pTos,1);` |
|       3 |  760 | `				PH7_MemObjRelease(pTos);` |
|       3 |  761 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       3 |  762 | `				pTos->nIdx = SXU32_HIGH;` |
|       3 |  763 | `				if( rcNm == PH7_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  764 | `				rc = rcNm;` |
|       3 |  765 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  766 | `			}` |
|   61002 |  767 | `		}` |
|  277863 |  768 | `		if( pInstr->iP2 == PH7_MEMBER_DEFPATH` |
|  141577 |  769 | `		 && (pNos->iFlags & (MEMOBJ_AUX_DEFPATH\|MEMOBJ_AUX_DEFERRED)) ){` |
|       - |  770 | `			/* D1 commit 2: deferred property arg ($o->p) whose base is itself a deferred lvalue` |
|       - |  771 | ``			 * (a nested chain `$a["k"]->p`, or an undefined base object). Append a property step`` |
|       - |  772 | `			 * to the base's captured path; OP_CALL re-walks it. */` |
|       - |  773 | `			SyString sProp;` |
|      21 |  774 | `			VmDeferredPath *pPath = 0;` |
|      21 |  775 | `			SyStringInitFromBuf(&sProp,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      21 |  776 | `			if( pNos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|      19 |  777 | `				pPath = (VmDeferredPath *)pNos->x.pOther;` |
|      19 |  778 | `				VmDeferPathPushProp(pPath,&sProp);` |
|      10 |  779 | `			}else{` |
|       - |  780 | `				SyString sRootName;` |
|       3 |  781 | `				SyStringInitFromBuf(&sRootName,(const char *)pNos->x.pOther,` |
|       - |  782 | `					pNos->x.pOther ? SyStrlen((const char *)pNos->x.pOther) : 0);` |
|       3 |  783 | `				pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|       3 |  784 | `				if( pPath ){` |
|       3 |  785 | `					pNos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name */` |
|       3 |  786 | `					pNos->x.pOther = pPath;` |
|       3 |  787 | `					pNos->iFlags \|= MEMOBJ_AUX_DEFPATH;` |
|       3 |  788 | `					pNos->nIdx = SXU32_HIGH;` |
|       3 |  789 | `					VmDeferPathPushProp(pPath,&sProp);` |
|       1 |  790 | `				}` |
|       - |  791 | `			}` |
|      21 |  792 | `			VmPopOperand(&pTos,1); /* drop the property name; the carrier stays as pTos */` |
|      21 |  793 | `			VM_EXIT_BREAK;` |
|       - |  794 | `		}` |
|  277848 |  795 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - |  796 | `			ph7_class *pClass;` |
|       - |  797 | `			/* Class already instantiated */` |
|  277684 |  798 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       - |  799 | `			/* Point to the instantiated class */` |
|  277684 |  800 | `			pClass = pThis->pClass;` |
|       - |  801 | `			/* Extract attribute name first */` |
|  277684 |  802 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|  277684 |  803 | `			if( PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
|       - |  804 | `				/* __PHP_Incomplete_Class: every property access and method call is` |
|       - |  805 | `				 * php's incomplete-object diagnostic — the carrier's OWN entries are` |
|       - |  806 | `				 * for the engine's surfaces only. A READ (isset/empty/?? included)` |
|       - |  807 | `				 * is an E_WARNING answering NULL; every WRITE shape and unset() is` |
|       - |  808 | `				 * a catchable Error; a method call is its own Error, raised where` |
|       - |  809 | `				 * the undefined-method twin raises (before any argument runs). The` |
|       - |  810 | `				 * DEFPATH pre-pass defers like a MISSING property would — the slot` |
|       - |  811 | `				 * fast path must not hand out the carrier's storage — so the by-ref` |
|       - |  812 | `				 * resolve (VmBindPropByRef) raises the Error and the by-value` |
|       - |  813 | `				 * re-drive lands back here in READ context for the warning.` |
|       - |  814 | ``				 * `??=` reads before it refuses, so it takes BOTH, php's order. */`` |
|      37 |  815 | `				VmInstr *pIncNext = pInstr + 1;` |
|      37 |  816 | `				if( pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|       - |  817 | `					SyString sIncProp;` |
|       5 |  818 | `					VmDeferredPath *pIncPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|       5 |  819 | `					SyStringInitFromBuf(&sIncProp,sName.zString,sName.nByte);` |
|       5 |  820 | `					if( pIncPath && VmDeferPathPushProp(pIncPath,&sIncProp) == SXRET_OK ){` |
|       5 |  821 | `						VmPopOperand(&pTos,1);       /* drop the property name */` |
|       5 |  822 | `						pThis->iRef++;` |
|       5 |  823 | `						PH7_MemObjRelease(pTos);     /* collapse the object slot into the carrier */` |
|       5 |  824 | `						pTos->x.pOther = pIncPath;` |
|       5 |  825 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|       5 |  826 | `						pTos->nIdx = SXU32_HIGH;` |
|       5 |  827 | `						PH7_ClassInstanceUnref(pThis);` |
|       5 |  828 | `						VM_EXIT_BREAK;` |
|       - |  829 | `					}` |
|     ! 0 |  830 | `					if( pIncPath ){` |
|     ! 0 |  831 | `						VmFreeDeferredPath(pIncPath);` |
|     ! 0 |  832 | `					}` |
|       - |  833 | `					/* allocation failure (or a temporary base below): the read` |
|       - |  834 | `					 * verdict is all that is left — fall through to warn + NULL. */` |
|     ! 0 |  835 | `				}` |
|      33 |  836 | `				int bIncCall = (pInstr->iP2 == PH7_MEMBER_METHOD);` |
|      33 |  837 | `				int bIncCoalW = (pInstr->iP2 == PH7_MEMBER_WRITE && pIncNext->iOp == PH7_OP_NULLC_JMP);` |
|      65 |  838 | `				int bIncModify = pInstr->iP2 != PH7_MEMBER_DEFPATH` |
|      63 |  839 | `					&& (pInstr->iP2 == PH7_MEMBER_UNSET` |
|      31 |  840 | `					 \|\| pInstr->iP2 == PH7_MEMBER_WRITE` |
|      25 |  841 | `					 \|\| pInstr->iP2 == PH7_MEMBER_REF_TARGET` |
|      19 |  842 | `					 \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|      18 |  843 | `					 \|\| VmMemberNextIsWrite(pIncNext)` |
|      18 |  844 | `					 \|\| VmMemberFetchForWrite(pInstr));` |
|      33 |  845 | `				if( bIncCall ){` |
|       - |  846 | `					SyBlob sIncErr;` |
|       - |  847 | `					sxi32 rcInc;` |
|       3 |  848 | `					SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|       3 |  849 | `					PH7_VmIncompleteMsg(&(*pVm),pThis,"call a method",&sIncErr);` |
|       3 |  850 | `					VmPopOperand(&pTos,1);` |
|       3 |  851 | `					PH7_MemObjRelease(pTos);` |
|       3 |  852 | `					pTos->nIdx = SXU32_HIGH;` |
|       4 |  853 | `					rcInc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sIncErr),` |
|       1 |  854 | `						SyBlobLength(&sIncErr));` |
|       3 |  855 | `					SyBlobRelease(&sIncErr);` |
|       3 |  856 | `					if( rcInc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  857 | `					rc = rcInc;` |
|       3 |  858 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  859 | `				}` |
|      31 |  860 | `				if( bIncCoalW ){` |
|       3 |  861 | `					PH7_VmIncompleteAccessWarn(&(*pVm),pThis,0);` |
|       1 |  862 | `				}` |
|      31 |  863 | `				if( bIncModify ){` |
|       - |  864 | `					SyBlob sIncErr;` |
|      17 |  865 | `					SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|      17 |  866 | `					PH7_VmIncompleteMsg(&(*pVm),pThis,"modify a property",&sIncErr);` |
|      17 |  867 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sIncErr));` |
|       9 |  868 | `				}else{` |
|      15 |  869 | `					PH7_VmIncompleteAccessWarn(&(*pVm),pThis,0);` |
|       - |  870 | `				}` |
|      31 |  871 | `				VmPopOperand(&pTos,1);   /* pop the attribute name */` |
|      31 |  872 | `				PH7_MemObjRelease(pTos); /* the object slot becomes the NULL answer */` |
|      31 |  873 | `				pTos->nIdx = SXU32_HIGH;` |
|      31 |  874 | `				VM_EXIT_BREAK;` |
|       - |  875 | `			}` |
|  277648 |  876 | `			if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - |  877 | `				/* Method call */` |
|  155815 |  878 | `				ph7_class_method *pMeth = 0;` |
|  155815 |  879 | `				int bFwdCall = 0; /* re-targeted to a dual iterator's inner: no scope check */` |
|  155815 |  880 | `				if( sName.nByte > 0 ){` |
|       - |  881 | `					/* Extract the target method */` |
|  155815 |  882 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|   77860 |  883 | `				}` |
|  155815 |  884 | `				if( pMeth == 0 ){` |
|       - |  885 | `					/* php's dual iterators forward an unknown method to the one they` |
|       - |  886 | `					 * WRAP (spl_dual_it_call_method), ahead of __call and of the` |
|       - |  887 | `					 * undefined-method Error. Re-target the receiver slot the call` |
|       - |  888 | ``					 * below binds `$this` from and carry on down the ordinary path, so`` |
|       - |  889 | `					 * the visibility rules and the screened-name marking are the same` |
|       - |  890 | `					 * ones every other method call takes. */` |
|     119 |  891 | `					ph7_class_instance *pFwdInner = 0;` |
|     119 |  892 | `					ph7_class_method *pFwdMeth = 0;` |
|     119 |  893 | `					if( PH7_SplOuterForward(&(*pVm),pThis,&sName,&pFwdInner,&pFwdMeth) ){` |
|      25 |  894 | `						ph7_class_instance *pOuter = pThis;` |
|      25 |  895 | `						pFwdInner->iRef++;      /* the stack slot's new reference */` |
|      25 |  896 | `						pNos->x.pOther = pFwdInner;` |
|      25 |  897 | `						PH7_ClassInstanceUnref(pOuter); /* ...and drop its old one */` |
|      25 |  898 | `						pThis = pFwdInner;` |
|      25 |  899 | `						pClass = pFwdInner->pClass;` |
|      25 |  900 | `						pMeth = pFwdMeth;` |
|       - |  901 | `						/* php's forward is a direct zend_call_method with no calling` |
|       - |  902 | `						 * scope, so the inner's own visibility does not apply: a` |
|       - |  903 | `						 * PROTECTED method on the wrapped iterator answers. */` |
|      25 |  904 | `						bFwdCall = 1;` |
|      12 |  905 | `					}` |
|      58 |  906 | `				}` |
|  155815 |  907 | `				if( pMeth == 0 ){` |
|      95 |  908 | `					ph7_class_method *pCallMagic = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|      95 |  909 | `					if( pCallMagic ){` |
|       - |  910 | `						/* php: a missing method dispatches __call($name, $args) — the` |
|       - |  911 | `						 * args are only collected by the following OP_CALL, so stash the` |
|       - |  912 | `						 * receiver + class + original name and MARK the callee slot` |
|       - |  913 | `						 * (band A #3b; pre-fix the name-only call discarded everything` |
|       - |  914 | `						 * and the call site failed with "Invalid function name"). Stack:` |
|       - |  915 | `						 * pop the method name, then the receiver slot becomes the marked` |
|       - |  916 | `						 * carrier OP_CALL routes through the packing body. */` |
|      57 |  917 | `						VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pThis,pClass,&sName);` |
|      57 |  918 | `						if( pPend == 0 ){` |
|     ! 0 |  919 | `							VM_EXIT_ABORT;` |
|       - |  920 | `						}` |
|      57 |  921 | `						VmPopOperand(&pTos,1);` |
|      57 |  922 | `						PH7_MemObjRelease(pTos);` |
|      57 |  923 | `						pTos->x.pOther = pPend;` |
|      57 |  924 | `						pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|      30 |  925 | `					}else{` |
|       - |  926 | `						{` |
|       - |  927 | `							/* php raises a catchable Error here; PH7 printed a notice and CARRIED ON` |
|       - |  928 | `							 * with NULL, so the call silently produced nothing. */` |
|       - |  929 | `							SyBlob sErrM;` |
|       - |  930 | `							sxi32 rcErr;` |
|      39 |  931 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      39 |  932 | `							SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",&pClass->sName,&sName);` |
|      39 |  933 | `							VmPopOperand(&pTos,1);` |
|      39 |  934 | `							PH7_MemObjRelease(pTos);` |
|      39 |  935 | `							pTos->nIdx = SXU32_HIGH;` |
|      58 |  936 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      19 |  937 | `								SyBlobLength(&sErrM));` |
|      39 |  938 | `							SyBlobRelease(&sErrM);` |
|      39 |  939 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      39 |  940 | `							rc = rcErr;` |
|      39 |  941 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  942 | `						}` |
|       - |  943 | `					}` |
|      30 |  944 | `				}else{` |
|  155723 |  945 | `					ph7_class_method *pDeniedCall = 0;` |
|  155723 |  946 | `					int bDenied = 0;` |
|       - |  947 | `					/* php decides a method's visibility against the class that OWNS it,` |
|       - |  948 | `					 * which for a trait method is the class that composed it — a trait is` |
|       - |  949 | `					 * a compile-time construct php has flattened away by now. Deciding` |
|       - |  950 | `					 * against the trait instead made every rule a question of who USES it:` |
|       - |  951 | `					 * a protected trait method was denied to a SUBCLASS of the composing` |
|       - |  952 | `					 * class (not a trait user itself) and granted to an unrelated class` |
|       - |  953 | `					 * that happened to use the same trait. */` |
|  155723 |  954 | `					ph7_class *pOwner = 0;` |
|  155718 |  955 | `					if( !bFwdCall && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC` |
|   77872 |  956 | `					 && !PH7_VmClassMemberAccess(&(*pVm),` |
|     130 |  957 | `						(pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth)),` |
|      65 |  958 | `						&sName,pMeth->iProtection,FALSE) ){` |
|      57 |  959 | `						int bRebound = 0;` |
|      57 |  960 | `						if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - |  961 | `							/* php: when the instance's class SHADOWS the caller's` |
|       - |  962 | `							 * private method (child redeclares private m), code in` |
|       - |  963 | `							 * the caller's class dispatches its OWN private m, not` |
|       - |  964 | `							 * the shadow. Rebind to the calling scope's method when` |
|       - |  965 | `							 * that scope declares a private one of this name. */` |
|      49 |  966 | `							VmFrame *pFrameLocal = VmSkipExceptionFrames(pVm->pFrame);` |
|      49 |  967 | `							ph7_vm_func *pCallerFunc = (ph7_vm_func *)pFrameLocal->pUserData;` |
|      49 |  968 | `							if( pCallerFunc && (pCallerFunc->iFlags & VM_FUNC_CLASS_METHOD) && pCallerFunc->pUserData ){` |
|       6 |  969 | `								ph7_class *pScope = (ph7_class *)pCallerFunc->pUserData;` |
|       6 |  970 | `								ph7_class_method *pOwn = PH7_ClassExtractMethod(pScope,sName.zString,sName.nByte);` |
|       4 |  971 | `								if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       6 |  972 | `								 && (ph7_class *)pOwn->sFunc.pUserData == pScope ){` |
|       6 |  973 | `									pMeth = pOwn;` |
|       6 |  974 | `									bRebound = 1;` |
|       2 |  975 | `								}` |
|       2 |  976 | `							}` |
|      23 |  977 | `						}` |
|      57 |  978 | `						if( !bRebound ){` |
|       - |  979 | `							/* Inaccessible from this scope: php routes through __call when` |
|       - |  980 | `							 * declared (band A #3b); without it the refusal is raised right` |
|       - |  981 | `							 * here, beside the undefined-method and abstract-method twins. */` |
|      53 |  982 | `							pDeniedCall = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|      53 |  983 | `							bDenied = pDeniedCall == 0;` |
|      25 |  984 | `						}` |
|      27 |  985 | `					}` |
|  155723 |  986 | `					if( bDenied ){` |
|       - |  987 | ``						/* php's visibility Error for `$o->m()`. It is raised HERE, at the`` |
|       - |  988 | `						 * member resolution, and not left to OP_CALL: the method OP_CALL` |
|       - |  989 | `						 * would re-derive is looked up by the FUNCTION's own name against` |
|       - |  990 | `						 * its declaring class, which a trait adaptation splits away from` |
|       - |  991 | ``						 * the entry this lookup actually chose — `hi as private pHi` gave`` |
|       - |  992 | ``						 * OP_CALL the trait's public `hi`, so the private alias ran from`` |
|       - |  993 | `						 * global scope in silence. The entry that answered is right here,` |
|       - |  994 | `						 * with its composed protection.` |
|       - |  995 | `						 *` |
|       - |  996 | ``						 * php names the method as the CALL SPELLS it (`$o->PQ()` on a`` |
|       - |  997 | ``						 * private `pQ()` reports `PQ`) and the class that OWNS the method,`` |
|       - |  998 | `						 * which for a trait method is the composing class. */` |
|       - |  999 | `						SyBlob sErrM;` |
|       - | 1000 | `						sxi32 rcErr;` |
|      43 | 1001 | `						ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|      63 | 1002 | `						const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      20 | 1003 | `							? "private" : "protected";` |
|      43 | 1004 | `						SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      43 | 1005 | `						if( pScope ){` |
|       7 | 1006 | `							SyBlobFormat(&sErrM,"Call to %s method %z::%z() from scope %z",` |
|       3 | 1007 | `								zVis,&pOwner->sName,&sName,&pScope->sName);` |
|       4 | 1008 | `						}else{` |
|      37 | 1009 | `							SyBlobFormat(&sErrM,"Call to %s method %z::%z() from global scope",` |
|      17 | 1010 | `								zVis,&pOwner->sName,&sName);` |
|       - | 1011 | `						}` |
|      43 | 1012 | `						VmPopOperand(&pTos,1);` |
|      43 | 1013 | `						PH7_MemObjRelease(pTos);` |
|      43 | 1014 | `						pTos->nIdx = SXU32_HIGH;` |
|      63 | 1015 | `						rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      20 | 1016 | `							SyBlobLength(&sErrM));` |
|      43 | 1017 | `						SyBlobRelease(&sErrM);` |
|      43 | 1018 | `						if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      43 | 1019 | `						rc = rcErr;` |
|      47 | 1020 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1021 | `					}` |
|  155683 | 1022 | `					if( pDeniedCall ){` |
|      11 | 1023 | `						VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pThis,pClass,&sName);` |
|      11 | 1024 | `						if( pPend == 0 ){` |
|     ! 0 | 1025 | `							VM_EXIT_ABORT;` |
|       - | 1026 | `						}` |
|      11 | 1027 | `						VmPopOperand(&pTos,1);` |
|      11 | 1028 | `						PH7_MemObjRelease(pTos);` |
|      11 | 1029 | `						pTos->x.pOther = pPend;` |
|      11 | 1030 | `						pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|       6 | 1031 | `					}else{` |
|       - | 1032 | `						/* Push method name on the stack, MARKED as already screened: the` |
|       - | 1033 | `						 * decision above was made against the entry this lookup chose,` |
|       - | 1034 | `						 * which is the only place a trait adaptation's composed` |
|       - | 1035 | `						 * protection is visible. */` |
|  155673 | 1036 | `						PH7_MemObjRelease(pTos);` |
|  155673 | 1037 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|  155673 | 1038 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|  155673 | 1039 | `						pTos->iFlags \|= MEMOBJ_AUX_MEMBERCALL;` |
|       - | 1040 | `					}` |
|       - | 1041 | `				}` |
|  155737 | 1042 | `				pTos->nIdx = SXU32_HIGH;` |
|   77826 | 1043 | `			}else{` |
|       - | 1044 | `				/* Attribute access. iP2: 0 = read, 2 = unset, 3 = isset, 4 = empty. */` |
|  121838 | 1045 | `				VmClassAttr *pObjAttr = 0;` |
|  121838 | 1046 | `				SyHashEntry *pEntry = 0;` |
|       - | 1047 | `				/* A LAZY native property read before anything installed it, whose` |
|       - | 1048 | `				 * class answers such a read from its zeroed struct rather than` |
|       - | 1049 | `				 * calling the name undefined (PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT).` |
|       - | 1050 | `				 * Set by the miss handling below and answered after the pop. */` |
|  121838 | 1051 | `				ph7_class_attr *pLazyDefault = 0;` |
|       - | 1052 | `				/* An UNINITIALIZED typed slot handed to the deferred call-argument rail` |
|       - | 1053 | `				 * below, which records it whether or not the class declares __get: php` |
|       - | 1054 | `				 * consults no magic accessor for a typed property that was never written` |
|       - | 1055 | `				 * (only an unset() one reaches __get there), and the two diagnostics it` |
|       - | 1056 | `				 * DOES have -- one for a by-value binding, one for a by-reference one --` |
|       - | 1057 | `				 * are exactly what the rail exists to tell apart. */` |
|  121838 | 1058 | `				int bDeferUninit = 0;` |
|  121833 | 1059 | `				if( sName.nByte > 0 && sName.zString[0] == 0` |
|   60930 | 1060 | `				 && !PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
|       - | 1061 | `					/* php refuses a property name beginning with a NUL outright:` |
|       - | 1062 | ``					 * `Cannot access property starting with "\0"`. Those bytes are`` |
|       - | 1063 | `					 * the ENGINE's private mangling — "\0Cls\0p" is C1::$p and` |
|       - | 1064 | `					 * "\0*\0p" is a protected slot — so a script that could write one` |
|       - | 1065 | `					 * would forge a private member of any class it names, and every` |
|       - | 1066 | `					 * display surface would then render the forgery as the real thing.` |
|       - | 1067 | `					 * Two rules ride with it, both probe-verified: a MAGIC accessor` |
|       - | 1068 | `					 * wins (php dispatches __get/__set/__isset/__unset with the raw` |
|       - | 1069 | `					 * name and says nothing), and a LOOKUP context is silent — isset()` |
|       - | 1070 | ``					 * is false, empty() true, `??` takes the default — which is what`` |
|       - | 1071 | `					 * the existing miss handling below already answers. The refusal` |
|       - | 1072 | `					 * does not depend on whether such a property exists: php raises it` |
|       - | 1073 | `					 * on the NAME, before any lookup, and the one place a NUL-keyed` |
|       - | 1074 | `					 * property legitimately lives is the __PHP_Incomplete_Class` |
|       - | 1075 | `					 * carrier, whose own gate above has already answered for it.` |
|       - | 1076 | ``					 * A METHOD name is not covered — php lets `$o->{"\0m"}()` reach`` |
|       - | 1077 | `					 * the ordinary undefined-method Error — so this sits in the` |
|       - | 1078 | `					 * attribute branch only. */` |
|       - | 1079 | `					const char *zNulMagic;` |
|      51 | 1080 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       5 | 1081 | `						zNulMagic = "__unset";` |
|      49 | 1082 | `					}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|      46 | 1083 | `					       \|\| VmMemberNextIsWrite(pInstr + 1) ){` |
|      19 | 1084 | `						zNulMagic = "__set";` |
|      10 | 1085 | `					}else{` |
|      29 | 1086 | `						zNulMagic = "__get";` |
|       - | 1087 | `					}` |
|      50 | 1088 | `					if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      47 | 1089 | `					 && PH7_ClassExtractMethod(pClass,zNulMagic,(sxu32)SyStrlen(zNulMagic)) == 0 ){` |
|       - | 1090 | `						SyBlob sNulErr;` |
|       - | 1091 | `						sxi32 rcNul;` |
|      33 | 1092 | `						SyBlobInit(&sNulErr,&pVm->sAllocator);` |
|      33 | 1093 | `						SyBlobAppend(&sNulErr,"Cannot access property starting with \"\\0\"",` |
|       - | 1094 | `							sizeof("Cannot access property starting with \"\\0\"")-1);` |
|      33 | 1095 | `						VmPopOperand(&pTos,1);   /* pop the attribute name */` |
|      33 | 1096 | `						PH7_MemObjRelease(pTos); /* the object slot becomes the NULL answer */` |
|      33 | 1097 | `						pTos->nIdx = SXU32_HIGH;` |
|      49 | 1098 | `						rcNul = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sNulErr),` |
|      16 | 1099 | `							SyBlobLength(&sNulErr));` |
|      33 | 1100 | `						SyBlobRelease(&sNulErr);` |
|      33 | 1101 | `						if( rcNul == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      33 | 1102 | `						rc = rcNul;` |
|      33 | 1103 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1104 | `					}` |
|       9 | 1105 | `				}` |
|       - | 1106 | `				/* Extract the target attribute, as the class whose code is RUNNING` |
|       - | 1107 | `				 * sees it: a scope that declares a private of this name owns a slot` |
|       - | 1108 | `				 * of its own on every instance below it (php's mangled storage name)` |
|       - | 1109 | `				 * and means THAT one, whatever the object's class holds under the` |
|       - | 1110 | `				 * plain name. The EMPTY name is a real property name in php —` |
|       - | 1111 | ``				 * `$o->{''} = 1` creates one and `$o->{''}` reads it back — and it is`` |
|       - | 1112 | `				 * the one name SyHashGet cannot answer for, so it takes a` |
|       - | 1113 | `				 * list-walking lookup inside.` |
|       - | 1114 | `				 *` |
|       - | 1115 | `				 * The name is hashed ONCE, here, and reused by both questions` |
|       - | 1116 | `				 * underneath -- the scope's private-name screen and the slot probe` |
|       - | 1117 | `				 * on this object -- because every property table shares one hash` |
|       - | 1118 | `				 * function and hashing is what a lookup spends (PERF.md §5). */` |
|  182702 | 1119 | `				pEntry = PH7_ClassInstanceScopedAttrEntry(&(*pVm),pThis,sName.zString,sName.nByte,` |
|  121801 | 1120 | `					sName.nByte > 0 ? SyHashKey(&pThis->hAttr,(const void *)sName.zString,sName.nByte) : 0);` |
|  121806 | 1121 | `				if( pEntry ){` |
|       - | 1122 | `					/* Point to the attribute value */` |
|  113947 | 1123 | `					pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|   56971 | 1124 | `				}` |
|  121801 | 1125 | `				if( pObjAttr` |
|  117867 | 1126 | `				 && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT))` |
|   56988 | 1127 | `				 && !VmStaticThroughInstanceVisible(&(*pVm),pClass,pObjAttr->pAttr,&sName) ){` |
|       - | 1128 | `					/* A private static this scope may not touch. When it is an INHERITED` |
|       - | 1129 | ``					 * one php's instance path does not see it at all: `$b->s` is an`` |
|       - | 1130 | `					 * ordinary MISSING property (warn and null, a dynamic write, a silent` |
|       - | 1131 | `					 * unset), with none of the visibility refusal the DECLARING class's` |
|       - | 1132 | `					 * own instance gets. The declaring class's own keeps the refusal. */` |
|       9 | 1133 | `					if( PH7_VmMemberOwnerClass(pObjAttr->pAttr->pDeclClass,pClass) != pClass ){` |
|       3 | 1134 | `						pEntry = 0;` |
|       3 | 1135 | `						pObjAttr = 0;` |
|       1 | 1136 | `					}` |
|  121800 | 1137 | `				}else if( pObjAttr` |
|  117866 | 1138 | `				 && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) ){` |
|       - | 1139 | `					/* A static property (or a constant) belongs to the CLASS: php does` |
|       - | 1140 | `					 * not find it through an instance at all. It notices the attempt —` |
|       - | 1141 | ``					 * `Accessing static property C::$s as non static` — and then treats`` |
|       - | 1142 | `					 * the name as an ordinary MISSING property: a read warns and answers` |
|       - | 1143 | `					 * null, isset() is false, a write goes to a dynamic property (which` |
|       - | 1144 | `					 * PHL rejects by the §10 policy, like any other undeclared write).` |
|       - | 1145 | `					 * PHL's instance table carries an entry for every declared member` |
|       - | 1146 | ``					 * (statics share the class slot), so `$o->s` used to READ and — far`` |
|       - | 1147 | `					 * worse — WRITE the class's own static in silence. isset()/empty()` |
|       - | 1148 | `					 * pass silently, as php's do. */` |
|      18 | 1149 | `					if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      18 | 1150 | `					 && pInstr->iP2 != PH7_MEMBER_DEFPATH ){` |
|       - | 1151 | `						/* Silent where php is silent: isset()/empty(), and any class` |
|       - | 1152 | `						 * that declares the MAGIC accessor this context would dispatch` |
|       - | 1153 | ``						 * (php passes `silent = ce->__get/__set/__unset != NULL` to its`` |
|       - | 1154 | `						 * property-offset lookup). Once per access, too: the deferred` |
|       - | 1155 | `						 * call-argument PRE-PASS (PH7_MEMBER_DEFPATH) re-runs this op for` |
|       - | 1156 | ``						 * the same source `$o->s` and the value pass carries the notice`` |
|       - | 1157 | `						 * — the BY-REF form has no value pass and notices at its own` |
|       - | 1158 | `						 * site (VmBindPropByRef). */` |
|       - | 1159 | `						const char *zMagic;` |
|      10 | 1160 | `						if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       3 | 1161 | `							zMagic = "__unset";` |
|       8 | 1162 | `						}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|       8 | 1163 | `						       \|\| VmMemberNextIsWrite(pInstr + 1) ){` |
|       3 | 1164 | `							zMagic = "__set";` |
|       2 | 1165 | `						}else{` |
|       6 | 1166 | `							zMagic = "__get";` |
|       - | 1167 | `						}` |
|      10 | 1168 | `						if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) == 0 ){` |
|      11 | 1169 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - | 1170 | `								"Accessing static property %z::$%z as non static",` |
|       6 | 1171 | `								&pClass->sName,&pObjAttr->pAttr->sName);` |
|       3 | 1172 | `						}` |
|       4 | 1173 | `					}` |
|      20 | 1174 | `					pEntry = 0;` |
|      20 | 1175 | `					pObjAttr = 0;` |
|       9 | 1176 | `				}` |
|  121801 | 1177 | `				if( pObjAttr == 0 && PH7_ClassHasNativeProp(pClass)` |
|    7536 | 1178 | `				 && !VmMemberCoalOwned(pThis,pInstr,&sName) ){` |
|       - | 1179 | `					/* php's read_property / has_property / write_property /` |
|       - | 1180 | `					 * unset_property handlers, for a class whose properties are not` |
|       - | 1181 | `					 * storage at all: PDORow answers every read from the statement's` |
|       - | 1182 | `					 * current ROW, holds no slot for any of them, and refuses every` |
|       - | 1183 | `					 * write. It comes first among the miss paths, and before the` |
|       - | 1184 | `					 * declared-but-absent one: a name this class owns is neither a` |
|       - | 1185 | `					 * dynamic property to create, nor an "Undefined property" to warn` |
|       - | 1186 | `					 * about, nor a __get to dispatch.` |
|       - | 1187 | `					 *` |
|       - | 1188 | `					 * Which handler php asks is what the CONTEXT says: a plain store,` |
|       - | 1189 | `					 * a destructuring target and a read-modify-write all end in a` |
|       - | 1190 | ``					 * write; `??=` reads first and writes only when the read answered`` |
|       - | 1191 | ``					 * null; a subscript-write base (`$o->p[0] = 1`) is a READ whose`` |
|       - | 1192 | `					 * value the subscript then refuses; and isset() stops at the` |
|       - | 1193 | ``					 * truth while empty() and `??` take the value. */`` |
|    7176 | 1194 | `					VmInstr *pPropNext = pInstr + 1;` |
|    7176 | 1195 | `					int bPropStore = (pPropNext->iOp == PH7_OP_STORE && pPropNext->iP2 != 0);` |
|   11152 | 1196 | `					int bPropCoal = (pInstr->iP2 == PH7_MEMBER_WRITE` |
|    7171 | 1197 | `						&& pPropNext->iOp == PH7_OP_NULLC_JMP);` |
|       - | 1198 | `					PH7_NativePropCtx sProp;` |
|       - | 1199 | `					ph7_value sPropVal;` |
|    7176 | 1200 | `					PH7_MemObjInit(&(*pVm),&sPropVal);` |
|    7176 | 1201 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|      22 | 1202 | `						sProp.iMode = PH7_NATIVE_PROP_UNSET;` |
|    7163 | 1203 | `					}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET \|\| bPropStore` |
|    6778 | 1204 | `					       \|\| VmMemberNextIsRmw(pPropNext) ){` |
|     764 | 1205 | `						sProp.iMode = PH7_NATIVE_PROP_WRITE;` |
|    6775 | 1206 | `					}else if( pInstr->iP2 == PH7_MEMBER_ISSET ){` |
|     170 | 1207 | `						sProp.iMode = PH7_NATIVE_PROP_ISSET;` |
|    6310 | 1208 | `					}else if( pInstr->iP2 == PH7_MEMBER_EMPTY ){` |
|       - | 1209 | `						/* php's empty() asks has_property with a non-zero` |
|       - | 1210 | `						 * check_empty and takes THAT for the answer -- it never` |
|       - | 1211 | `						 * reads the value. The two are not the same question:` |
|       - | 1212 | ``						 * `empty($row->queryString)` is TRUE on a name the`` |
|       - | 1213 | `						 * handler does not know, while reading it works. */` |
|      29 | 1214 | `						sProp.iMode = PH7_NATIVE_PROP_NOTEMPTY;` |
|      15 | 1215 | `					}else{` |
|    6198 | 1216 | `						sProp.iMode = PH7_NATIVE_PROP_READ;` |
|       - | 1217 | `					}` |
|    7176 | 1218 | `					sProp.pName = &sName;` |
|    7176 | 1219 | `					sProp.pResult = &sPropVal;` |
|    7176 | 1220 | `					sProp.bAnswered = 0;` |
|    7176 | 1221 | `					sProp.zThrowClass = 0;` |
|    7176 | 1222 | `					sProp.zThrowMsg[0] = 0;` |
|    7176 | 1223 | `					sProp.iThrowCode = 0;` |
|       - | 1224 | ``					/* php's `??` is its THIRD accessor level: it takes the property's`` |
|       - | 1225 | `					 * VALUE and says nothing about a name that is not there. */` |
|    7176 | 1226 | `					sProp.bQuiet = (pInstr->iP2 == PH7_MEMBER_COALESCE);` |
|       - | 1227 | `					/* ...and a fetch in WRITE context is php's get_property_ptr_ptr,` |
|       - | 1228 | `					 * which a handler whose property is a real element answers with` |
|       - | 1229 | ``					 * the element itself -- so `$ao->list[] = 1` lands in the store`` |
|       - | 1230 | `					 * rather than in a temporary, and creates the key it is missing.` |
|       - | 1231 | ``					 * A reference SOURCE asks the same handler: `$r =& $ao->z` creates`` |
|       - | 1232 | `					 * the key and binds to it, where a VIRTUAL property (no slot to` |
|       - | 1233 | `					 * give) ignores the flag and hands back its value copy. */` |
|   10370 | 1234 | `					sProp.bWriteCtx = VmMemberNativeSetKeepsSlot(pInstr)` |
|    7171 | 1235 | `						\|\| VmMemberFetchIsRefSource(pInstr);` |
|    7176 | 1236 | `					sProp.nSlot = SXU32_HIGH;` |
|    7176 | 1237 | `					if( PH7_ClassNativeProp(pThis,&sProp) ){` |
|    6299 | 1238 | `						if( sProp.zThrowClass == 0 && bPropCoal` |
|    3127 | 1239 | `						 && sProp.iMode == PH7_NATIVE_PROP_READ` |
|       9 | 1240 | `						 && (sPropVal.iFlags & MEMOBJ_NULL) ){` |
|       - | 1241 | ``							/* `$o->p ??= v` on a name that reads null: the store php`` |
|       - | 1242 | `							 * skips for a non-null one is the one it now makes, so the` |
|       - | 1243 | `							 * refusal is the WRITE handler's. */` |
|       3 | 1244 | `							sProp.iMode = PH7_NATIVE_PROP_WRITE;` |
|       3 | 1245 | `							sProp.bAnswered = 0;` |
|       3 | 1246 | `							PH7_ClassNativeProp(pThis,&sProp);` |
|       1 | 1247 | `						}` |
|    6304 | 1248 | `						if( sProp.zThrowClass ){` |
|       - | 1249 | `							/* Parked, like every other refusal this op makes: the` |
|       - | 1250 | `							 * fetch-point router lands it and abandons this slot. */` |
|      79 | 1251 | `							VmBoundaryPark(&(*pVm),` |
|      52 | 1252 | `								VmThrowFixedErrorCode(&(*pVm),sProp.zThrowClass,` |
|      26 | 1253 | `									sProp.iThrowCode,sProp.zThrowMsg));` |
|      53 | 1254 | `							PH7_MemObjRelease(&sPropVal);` |
|      53 | 1255 | `							VmPopOperand(&pTos,1);      /* the property name */` |
|      53 | 1256 | `							PH7_MemObjRelease(pTos);    /* the object slot is the answer */` |
|      53 | 1257 | `							pTos->nIdx = SXU32_HIGH;` |
|    3176 | 1258 | `							VM_EXIT_BREAK;` |
|       - | 1259 | `						}` |
|    6252 | 1260 | `						VmPopOperand(&pTos,1);          /* the property name */` |
|    6252 | 1261 | `						pThis->iRef++;` |
|    6252 | 1262 | `						PH7_MemObjRelease(pTos);` |
|    6252 | 1263 | `						if( sProp.iMode == PH7_NATIVE_PROP_ISSET ){` |
|       - | 1264 | `							/* isset() only tests null-ness: a non-null marker for` |
|       - | 1265 | `							 * true, NULL for false — what the __isset arm pushes. */` |
|     118 | 1266 | `							if( ph7_value_to_bool(&sPropVal) ){` |
|      78 | 1267 | `								pTos->x.iVal = 1;` |
|      78 | 1268 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      40 | 1269 | `							}` |
|    6194 | 1270 | `						}else if( sProp.iMode != PH7_NATIVE_PROP_UNSET ){` |
|       - | 1271 | `							/* EMPTY pushes the handler's BOOL, which the op then` |
|       - | 1272 | `							 * judges for emptiness -- the same answer php takes. */` |
|    6126 | 1273 | `							PH7_MemObjStore(&sPropVal,pTos);` |
|    3061 | 1274 | `						}` |
|       - | 1275 | `						/* SXU32_HIGH unless the handler handed back a real element's` |
|       - | 1276 | `						 * slot: a virtual property is a VALUE and never an lvalue,` |
|       - | 1277 | `						 * while ArrayObject's storage element is the thing itself. */` |
|    6252 | 1278 | `						pTos->nIdx = sProp.nSlot;` |
|    6252 | 1279 | `						PH7_MemObjRelease(&sPropVal);` |
|    6252 | 1280 | `						PH7_ClassInstanceUnref(pThis);` |
|    6252 | 1281 | `						VM_EXIT_BREAK;` |
|       - | 1282 | `					}` |
|     875 | 1283 | `					PH7_MemObjRelease(&sPropVal);` |
|     436 | 1284 | `				}` |
|  115507 | 1285 | `				if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - | 1286 | `					/* unset($o->prop): remove the property entirely so it disappears from` |
|       - | 1287 | `					 * foreach / json_encode / get_object_vars / (array) — matching PHP (a value-only` |
|       - | 1288 | `					 * release would leave a zombie null entry). Leave a NULL constant on the stack so` |
|       - | 1289 | `					 * the trailing generic unset() builtin is a no-op (mirrors LOAD_IDX iP2=5).` |
|       - | 1290 | `					 * php dispatches __unset($name) for a MISSING or INACCESSIBLE property` |
|       - | 1291 | `					 * (band A #3b — pre-fix an inaccessible private was silently DELETED from` |
|       - | 1292 | `					 * outside the class); without __unset, an inaccessible unset is php's` |
|       - | 1293 | `					 * catchable "Cannot access ..." Error and a missing one stays a no-op. */` |
|     164 | 1294 | `					int bUnsAccessible = pEntry ? PH7_VmClassAttrAccess(&(*pVm),pClass,pObjAttr->pAttr,FALSE) : 0;` |
|     233 | 1295 | `					ph7_class_attr *pUnsNoWrite = pEntry ? pObjAttr->pAttr` |
|      91 | 1296 | `						: PH7_ClassScopedAttribute(&(*pVm),pClass,sName.zString,sName.nByte);` |
|     164 | 1297 | `					sxi32 rcUnsRo = SXRET_OK;` |
|     164 | 1298 | `					if( pEntry && (pObjAttr->iState & VM_CLASS_ATTR_RDONLY) != 0 ){` |
|       - | 1299 | `						/* php's read-only handler answers an unset with the same sentence` |
|       - | 1300 | ``						 * it answers a store: `Property p is read only`. */`` |
|       3 | 1301 | `						VmBoundaryPark(&(*pVm),VmThrowNativeReadOnly(&(*pVm),pObjAttr->pAttr));` |
|     163 | 1302 | `					}else if( pUnsNoWrite && (pUnsNoWrite->iFlags & (PH7_CLASS_ATTR_NATIVE_NOWRITE` |
|      73 | 1303 | `					                                                \|PH7_CLASS_ATTR_NATIVE_NOSLOT)) != 0 ){` |
|       - | 1304 | `						/* php's own unset handler for this class refuses, and words it` |
|       - | 1305 | ``						 * without either "readonly" or "property": `Cannot unset C::$p`.`` |
|       - | 1306 | `						 * It runs whether the object has a struct or not, so an` |
|       - | 1307 | `						 * unconstructed one -- which holds no slot at all -- refuses too` |
|       - | 1308 | `						 * rather than falling through to the missing-property no-op.` |
|       - | 1309 | `						 * A VIRTUAL property (NOSLOT) is the same answer for the same` |
|       - | 1310 | `						 * reason: php's unset_property handler for one has nothing to` |
|       - | 1311 | ``						 * remove, so `unset($doc->preserveWhiteSpace)` is this Error and`` |
|       - | 1312 | `						 * not the silent no-op a name the object lacks would take. */` |
|      37 | 1313 | `						VmBoundaryPark(&(*pVm),` |
|      12 | 1314 | `							VmThrowNativeNoUnset(&(*pVm),pThis->pClass,pUnsNoWrite));` |
|     150 | 1315 | `					}else if( pEntry && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) != 0 ){` |
|       - | 1316 | `						/* A native class's property is php's own C struct field, and` |
|       - | 1317 | `						 * unset() is a std handler that looks for a REAL property and` |
|       - | 1318 | `						 * finds none: nothing happens, nothing is said, and the next` |
|       - | 1319 | `						 * read still answers the struct. Removing the slot here left` |
|       - | 1320 | ``						 * `unset($i->y); $i->y` an Undefined property warning and NULL`` |
|       - | 1321 | `						 * for a statement php ignores. */` |
|     138 | 1322 | `					}else if( pEntry && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) != 0 ){` |
|       - | 1323 | `						/* php 8.4: a hooked property (virtual or backed) can never be` |
|       - | 1324 | `						 * unset — catchable Error, even from inside its own hook body` |
|       - | 1325 | `						 * (probe-verified). */` |
|       - | 1326 | `						SyBlob sErrMsg;` |
|       5 | 1327 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       5 | 1328 | `						SyBlobFormat(&sErrMsg,"Cannot unset hooked property %z::$%z",` |
|       4 | 1329 | `							&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       5 | 1330 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     129 | 1331 | `					}else if( pEntry && bUnsAccessible` |
|     110 | 1332 | `					       && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0` |
|      70 | 1333 | `					       && (rcUnsRo = VmCheckReadonlyUnset(&(*pVm),pThis->pClass,pObjAttr)) != SXRET_OK ){` |
|       - | 1334 | `						/* php refuses to destroy a readonly property: an initialized` |
|       - | 1335 | `						 * one from every scope, and an uninitialized one from a scope` |
|       - | 1336 | `						 * that may not write it. Deleting it here re-armed the` |
|       - | 1337 | ``						 * write-once latch, so a `readonly` value could be replaced by`` |
|       - | 1338 | `						 * anything in two statements. The refusal is thrown inside the` |
|       - | 1339 | `						 * check; parking it is what routes it like every other one` |
|       - | 1340 | `						 * raised from this opcode. */` |
|      17 | 1341 | `						VmBoundaryPark(&(*pVm),rcUnsRo);` |
|     122 | 1342 | `					}else if( pEntry && bUnsAccessible ){` |
|      92 | 1343 | `						if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|      59 | 1344 | `						 && (pObjAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){` |
|       - | 1345 | `							/* A TYPED property keeps its DECLARATION: php's unset makes` |
|       - | 1346 | `							 * it uninitialized, so var_dump still names it` |
|       - | 1347 | ``							 * `uninitialized(T)`, a read is "must not be accessed before`` |
|       - | 1348 | `							 * initialization" rather than an Undefined property, and a` |
|       - | 1349 | `							 * later write lands back in its declared position instead of` |
|       - | 1350 | `							 * appending a dynamic one at the end. The seven other` |
|       - | 1351 | `							 * presentation surfaces leave an uninitialized property out,` |
|       - | 1352 | `							 * which is what made the delete look right. */` |
|      24 | 1353 | `							ph7_value *pUnsSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|      24 | 1354 | `							if( pUnsSlot ){` |
|      24 | 1355 | `								PH7_MemObjRelease(pUnsSlot);` |
|      24 | 1356 | `								MemObjSetType(pUnsSlot,MEMOBJ_NULL);` |
|      11 | 1357 | `							}` |
|      24 | 1358 | `							pObjAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|      13 | 1359 | `						}else{` |
|      72 | 1360 | `							PH7_VmReleaseInstanceAttr(&(*pVm),pObjAttr);` |
|       - | 1361 | `` 							/* Through the instance's own door: a `foreach`/`array_walk` `` |
|       - | 1362 | `							 * standing one property short of this one is holding the` |
|       - | 1363 | `							 * entry about to be freed. */` |
|      72 | 1364 | `							PH7_ClassInstanceDeleteAttrEntry(pThis,pEntry);` |
|       - | 1365 | `						}` |
|      48 | 1366 | `					}else{` |
|      21 | 1367 | `						ph7_class_method *pUnsetMagic = PH7_ClassExtractMethod(pClass,"__unset",sizeof("__unset")-1);` |
|      21 | 1368 | `						if( pUnsetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'u') ){` |
|      13 | 1369 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'u');` |
|      13 | 1370 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__unset",sizeof("__unset")-1,&sName,0);` |
|      13 | 1371 | `							VmMagicGuardPop(pVm);` |
|      14 | 1372 | `						}else if( pEntry ){` |
|       - | 1373 | `							/* Inaccessible and no __unset: php's catchable Error. Parked on` |
|       - | 1374 | `							 * the boundary rail; the op completes benignly and the` |
|       - | 1375 | `							 * fetch-point router lands it. */` |
|       - | 1376 | `							SyBlob sErrMsg;` |
|       3 | 1377 | `							const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       3 | 1378 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 1379 | `							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",zVis,&pClass->sName,&sName);` |
|       3 | 1380 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       1 | 1381 | `						}` |
|       - | 1382 | `						/* Missing property without __unset: silent no-op (php). */` |
|       - | 1383 | `					}` |
|     164 | 1384 | `					VmPopOperand(&pTos,1);    /* pop the attribute name */` |
|     164 | 1385 | `					PH7_MemObjRelease(pTos);  /* release the object on the stack ($o's stack ref) */` |
|     164 | 1386 | `					pTos->nIdx = SXU32_HIGH;  /* NULL constant */` |
|     164 | 1387 | `					VM_EXIT_BREAK;` |
|       - | 1388 | `				}` |
|  115347 | 1389 | `				if( pObjAttr == 0 ){` |
|       - | 1390 | `					/* Member not present on the instance and the next instruction writes/modifies it` |
|       - | 1391 | ``					 * (store, array-append/keyed-write, `??=`, ++/--, or a compound-assign — see`` |
|       - | 1392 | `					 * VmMemberNextIsWrite; the compiler always emits a terminating PH7_OP_DONE so` |
|       - | 1393 | `					 * pInstr+1 is in-bounds). Create the property so the operation lands on a real slot` |
|       - | 1394 | ``					 * (PHP auto-vivifies a fresh property for all these forms, not just `=`):`` |
|       - | 1395 | `					 *   - a DECLARED prop that was unset() and is re-assigned → recreate it` |
|       - | 1396 | `					 *     (PHP re-appends it at the end), OR` |
|       - | 1397 | `					 *   - a dynamic prop on a dynamic-allowing class (stdClass).` |
|       - | 1398 | `					 * The created slot is NULL with a real nIdx, which the modify-op then vivifies` |
|       - | 1399 | `					 * (NULL→array for [], NULL→0/"" for ++/.=) and writes back in place.` |
|       - | 1400 | `					 * Two signals: the compiler tags the member itself iP2=PH7_MEMBER_WRITE when it is` |
|       - | 1401 | ``					 * the base of a write-subscript / `??=` (the modify-op is not the immediately-next`` |
|       - | 1402 | `					 * instruction there), and for ++/--/compound-assign/store the next opcode is the` |
|       - | 1403 | `					 * modify-op directly (VmMemberNextIsWrite). */` |
|    1563 | 1404 | `					VmInstr *pNext = pInstr + 1;` |
|       - | 1405 | ``					/* A reference SOURCE is php's write-context fetch too (`$r =& $o->p`,`` |
|       - | 1406 | ``					 * `[&$o->p]`, `foreach ($o->p as &$v)`): the name it does not find is`` |
|       - | 1407 | `					 * CREATED, silently, and the bind reaches the created slot. PHL read` |
|       - | 1408 | `					 * it, so every such source warned about what php was on the point of` |
|       - | 1409 | `					 * creating and then bound a fresh variable of its own -- a write` |
|       - | 1410 | `					 * through the reference never reached the object. */` |
|    1563 | 1411 | `					int bRefSrcMiss = VmMemberFetchIsRefSource(pInstr);` |
|    1558 | 1412 | `					if( pInstr->iP2 == PH7_MEMBER_WRITE \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|     540 | 1413 | `					 \|\| VmMemberNextIsWrite(pNext) \|\| bRefSrcMiss ){` |
|    1079 | 1414 | `						ph7_class_attr *pDecl = PH7_ClassScopedAttribute(&(*pVm),pThis->pClass,sName.zString,sName.nByte);` |
|    1079 | 1415 | `						if( pDecl && PH7_ATTR_LAZY_ABSENT(pDecl,pThis) ){` |
|       - | 1416 | `							/* The object has never held this name. A class whose handler` |
|       - | 1417 | `							 * REFUSES every write answers the same sentence with or` |
|       - | 1418 | `							 * without a struct, and nothing is created; otherwise php's` |
|       - | 1419 | `							 * own write goes to the standard handler and CREATES a` |
|       - | 1420 | `							 * dynamic property beside the struct, which PHL refuses` |
|       - | 1421 | `							 * (§10) -- so fall through to the dynamic branch and let it.` |
|       - | 1422 | `` 							 * Once the constructor has installed the set, an `unset()` `` |
|       - | 1423 | `							 * and a re-write are the ordinary declared path again. */` |
|      16 | 1424 | `							if( pDecl->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|       4 | 1425 | `								VmBoundaryPark(&(*pVm),` |
|       1 | 1426 | `									VmThrowNativeNoWrite(&(*pVm),pThis->pClass,pDecl));` |
|       3 | 1427 | `								VmPopOperand(&pTos,1);    /* pop the attribute name */` |
|       3 | 1428 | `								PH7_MemObjRelease(pTos);  /* the object slot becomes the answer */` |
|       3 | 1429 | `								pTos->nIdx = SXU32_HIGH;` |
|       3 | 1430 | `								VM_EXIT_BREAK;` |
|       - | 1431 | `							}` |
|      13 | 1432 | `							pDecl = 0;` |
|       6 | 1433 | `						}` |
|    1077 | 1434 | `						if( pDecl && (pDecl->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){` |
|       - | 1435 | `							/* A VIRTUAL property: php keeps no slot to re-create, and its` |
|       - | 1436 | `							 * write goes to the class's own write_property handler, which` |
|       - | 1437 | `							 * the rails below reach through PH7_ClassNativePropOwns.` |
|       - | 1438 | `							 * Creating one would give the object a real property php has` |
|       - | 1439 | `							 * none of, and would take the read with it` |
|       - | 1440 | ``							 * (`$doc->formatOutput = false` then answered out of the slot`` |
|       - | 1441 | `							 * rather than out of the extension's state). */` |
|     708 | 1442 | `							pDecl = 0;` |
|     353 | 1443 | `						}` |
|    1077 | 1444 | `						if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      15 | 1445 | `							VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pObjAttr);` |
|       8 | 1446 | `						}else{` |
|       - | 1447 | `							/* php 8 semantics (band A #3b): a PLAIN store to a missing` |
|       - | 1448 | `							 * property dispatches __set($name,$value) when declared —` |
|       - | 1449 | `							 * the value only exists at the following OP_STORE, so park` |
|       - | 1450 | `							 * the receiver+name for it (one-instruction lifetime; the` |
|       - | 1451 | `							 * guard makes a same-name write inside __set fall through` |
|       - | 1452 | `							 * to dynamic creation, like php). Without __set — or for a` |
|       - | 1453 | `							 * subscript-write base / read-modify-write, which php does` |
|       - | 1454 | `							 * NOT route through __set — create a dynamic property on` |
|       - | 1455 | `							 * ANY class with the 8.2 deprecation (suppressed for` |
|       - | 1456 | `							 * stdClass and #[AllowDynamicProperties]); a readonly` |
|       - | 1457 | `							 * class raises php's catchable Error instead. */` |
|    1063 | 1458 | `							ph7_class_method *pSetMagic = 0;` |
|    1063 | 1459 | `							ph7_class_method *pCoalIsset = 0, *pCoalGet = 0, *pCoalSet = 0;` |
|    1063 | 1460 | `							int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);` |
|       - | 1461 | `							/* A name the class's OWN property-handler table carries is` |
|       - | 1462 | ``							 * overloaded exactly the way a `__set` class's is -- php's`` |
|       - | 1463 | ``							 * write_property stands where `__set` would -- so every rail`` |
|       - | 1464 | `							 * below takes it, and the one door they all end at` |
|       - | 1465 | `							 * (VmMagicSetDispatch) asks the handler first. */` |
|    1063 | 1466 | `							int bOwnedSet = PH7_ClassNativePropOwns(pThis,&sName);` |
|    1063 | 1467 | `							if( bPlainStore \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|     947 | 1468 | `								pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|     471 | 1469 | `							}` |
|    1063 | 1470 | `							if( (pSetMagic \|\| bOwnedSet) && pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 1471 | `								/* php dispatches __set($name, element) for a missing` |
|       - | 1472 | `								 * destructuring target, but the element value only exists` |
|       - | 1473 | `								 * at the following OP_LOAD_LIST — the dispatch is` |
|       - | 1474 | `								 * unsupported (recorded residual). Leave the miss: the` |
|       - | 1475 | `								 * property stays uncreated, matching php's observable` |
|       - | 1476 | `								 * state (its __set did not store either). */` |
|    1050 | 1477 | `							}else if( (bOwnedSet && bPlainStore)` |
|     682 | 1478 | `							 \|\| (pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s')) ){` |
|     773 | 1479 | `								pThis->iRef++;` |
|     773 | 1480 | `								pVm->pMagicSetThis = pThis;` |
|     773 | 1481 | `								SyBlobReset(&pVm->sMagicSetName);` |
|     773 | 1482 | `								SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       - | 1483 | `								/* pObjAttr stays NULL; the miss path below stays silent. */` |
|     686 | 1484 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|     103 | 1485 | `							 && pNext->iOp == PH7_OP_NULLC_JMP` |
|      52 | 1486 | `							 && (bOwnedSet` |
|      11 | 1487 | `							  \|\| (pCoalIsset = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1)) != 0` |
|       8 | 1488 | `							  \|\| (pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)) != 0` |
|       5 | 1489 | `							  \|\| (pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1)) != 0) ){` |
|       - | 1490 | ``								/* `$o->p ??= v` on a missing property with magic accessors:`` |
|       - | 1491 | `								 * php consults __isset first when declared — false means` |
|       - | 1492 | `								 * assign directly through __set with NO __get call (the` |
|       - | 1493 | `								 * test value stays null); true (or no __isset) means __get` |
|       - | 1494 | `								 * provides the test value. A COAL_MAGIC entry is pushed` |
|       - | 1495 | `								 * for the OP_NULLC_STORE at the jump target; the` |
|       - | 1496 | `								 * fetch-point sweep drops it when the short-circuit jump` |
|       - | 1497 | `								 * skips the assign or a throw abandons the RHS. Without` |
|       - | 1498 | `								 * __set the assign side keeps the pre-existing loud` |
|       - | 1499 | `								 * "Cannot perform assignment" path (php would create a` |
|       - | 1500 | `								 * dynamic property there — recorded residual). Note the` |
|       - | 1501 | `								 * condition's short-circuit assignment chain: a hit on an` |
|       - | 1502 | `								 * earlier method leaves the later pointers unresolved, so` |
|       - | 1503 | `								 * re-resolve the leftovers here (each is one hash probe,` |
|       - | 1504 | `								 * only on this ??=-miss path). */` |
|       - | 1505 | `								ph7_value sTest;` |
|       9 | 1506 | `								int bMiss = 0; /* __isset said false: skip __get, test value stays null */` |
|       9 | 1507 | `								if( pCoalIsset != 0 ){` |
|       5 | 1508 | `									pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       2 | 1509 | `								}` |
|       9 | 1510 | `								if( pCoalIsset != 0 \|\| pCoalGet != 0 ){` |
|       7 | 1511 | `									pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|       3 | 1512 | `								}` |
|       9 | 1513 | `								PH7_MemObjInit(pVm,&sTest);` |
|       9 | 1514 | `								if( pCoalIsset && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 1515 | `									ph7_value sIssetRet;` |
|       5 | 1516 | `									PH7_MemObjInit(pVm,&sIssetRet);` |
|       5 | 1517 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|       5 | 1518 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|       5 | 1519 | `									VmMagicGuardPop(pVm);` |
|       5 | 1520 | `									PH7_MemObjToBool(&sIssetRet);` |
|       5 | 1521 | `									bMiss = sIssetRet.x.iVal == 0;` |
|       5 | 1522 | `									PH7_MemObjRelease(&sIssetRet);` |
|       2 | 1523 | `								}` |
|       9 | 1524 | `								if( bOwnedSet && !bMiss ){` |
|       - | 1525 | `									/* php's ASSIGN_COALESCE fetches BP_VAR_IS, which` |
|       - | 1526 | `									 * asks has_property before read_property -- so a` |
|       - | 1527 | `									 * name the handler answers nothing for is never` |
|       - | 1528 | `									 * READ, and the assign it makes says nothing about` |
|       - | 1529 | `									 * a key that was not there. */` |
|       - | 1530 | `									ph7_value sOwnIs;` |
|       3 | 1531 | `									PH7_MemObjInit(pVm,&sOwnIs);` |
|       3 | 1532 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,` |
|       - | 1533 | `										"__isset",sizeof("__isset")-1,&sName,&sOwnIs);` |
|       3 | 1534 | `									PH7_MemObjToBool(&sOwnIs);` |
|       3 | 1535 | `									bMiss = sOwnIs.x.iVal == 0;` |
|       3 | 1536 | `									PH7_MemObjRelease(&sOwnIs);` |
|       1 | 1537 | `								}` |
|       8 | 1538 | `								if( !bMiss && (pCoalGet \|\| bOwnedSet)` |
|       5 | 1539 | `								 && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       5 | 1540 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       5 | 1541 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sTest);` |
|       5 | 1542 | `									VmMagicGuardPop(pVm);` |
|       2 | 1543 | `								}` |
|       8 | 1544 | `								if( (pCoalSet \|\| bOwnedSet)` |
|       8 | 1545 | `								 && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s')` |
|       9 | 1546 | `								 && pVm->nBoundaryRc == 0 ){` |
|       - | 1547 | `									VmHookRmw sPend;` |
|       9 | 1548 | `									sPend.iKind = VM_HOOK_PEND_COAL_MAGIC;` |
|       9 | 1549 | `									sPend.pThis = pThis;` |
|       9 | 1550 | `									sPend.pAttr = 0;` |
|       9 | 1551 | `									sPend.nBackIdx = SXU32_HIGH;` |
|       9 | 1552 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|       9 | 1553 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|       9 | 1554 | `									SyBlobAppend(&sPend.sName,(const void *)sName.zString,sName.nByte);` |
|       9 | 1555 | `									sPend.pOwnerStack = (void *)pStack;` |
|       9 | 1556 | `									sPend.pInstrs = (void *)aInstr;` |
|       9 | 1557 | `									sPend.nJmpPc = (sxu32)(pc + 1);        /* the OP_NULLC_JMP */` |
|       9 | 1558 | `									sPend.nPc = (sxu32)(pNext->iP2 - 1);   /* the OP_NULLC_STORE */` |
|       9 | 1559 | `									pThis->iRef++;` |
|       9 | 1560 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|       4 | 1561 | `								}` |
|       - | 1562 | `								/* Pop the attribute name; the test value becomes the` |
|       - | 1563 | `								 * expression slot (a temp, not an lvalue). */` |
|       9 | 1564 | `								VmPopOperand(&pTos,1);` |
|       9 | 1565 | `								pThis->iRef++;` |
|       9 | 1566 | `								PH7_MemObjRelease(pTos);` |
|       9 | 1567 | `								PH7_MemObjStore(&sTest,pTos);` |
|       9 | 1568 | `								pTos->nIdx = SXU32_HIGH;` |
|       9 | 1569 | `								PH7_MemObjRelease(&sTest);` |
|       9 | 1570 | `								PH7_ClassInstanceUnref(pThis);` |
|       9 | 1571 | `								VM_EXIT_BREAK;` |
|     290 | 1572 | `							}else if( VmMemberNextIsRmw(pNext)` |
|     171 | 1573 | `							 && VmMagicRmwEligible(pVm,pClass,pThis,&sName) ){` |
|       - | 1574 | `								/* ++/--/compound-assign on an overloaded property: php` |
|       - | 1575 | `								 * reads through __get and writes the computed value back` |
|       - | 1576 | `								 * through __set. Arm the scratch slot the modify op will` |
|       - | 1577 | `								 * mutate and finish the op here — the pre-existing path` |
|       - | 1578 | `								 * vivified a dynamic property instead, which on any` |
|       - | 1579 | `								 * ordinary accessor class is PHL's` |
|       - | 1580 | `								 * "Cannot create dynamic property" Error. */` |
|       - | 1581 | `								ph7_value sRmwVal;` |
|       - | 1582 | `								sxu32 nRmwScratch;` |
|      31 | 1583 | `								PH7_MemObjInit(pVm,&sRmwVal);` |
|      46 | 1584 | `								VmMagicRmwArm(&(*pVm),pThis,pClass,&sName,&sRmwVal,&nRmwScratch,` |
|      30 | 1585 | `									(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|      31 | 1586 | `								VmPopOperand(&pTos,1);   /* drop the property name */` |
|      31 | 1587 | `								pThis->iRef++;` |
|      31 | 1588 | `								PH7_MemObjRelease(pTos); /* collapse the object slot */` |
|      31 | 1589 | `								PH7_MemObjStore(&sRmwVal,pTos);` |
|      31 | 1590 | `								pTos->nIdx = nRmwScratch;` |
|      31 | 1591 | `								PH7_MemObjRelease(&sRmwVal);` |
|      31 | 1592 | `								PH7_ClassInstanceUnref(pThis);` |
|      31 | 1593 | `								VM_EXIT_BREAK;` |
|     260 | 1594 | `							}else if( bRefSrcMiss` |
|     156 | 1595 | `							 && (bOwnedSet` |
|      42 | 1596 | `							  \|\| PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)) ){` |
|       - | 1597 | `								/* An OVERLOADED name is php's exception to the rule above:` |
|       - | 1598 | `								 * get_property_ptr_ptr has nothing to hand out for one, so` |
|       - | 1599 | `								 * php falls back to read_property in write mode -- __get` |
|       - | 1600 | `								 * (or the class's own read handler) answers, and the` |
|       - | 1601 | ``								 * `Indirect modification of overloaded property` notice`` |
|       - | 1602 | `								 * comes with it. Leave the miss: the read gate below is` |
|       - | 1603 | `								 * where both of those live. */` |
|     258 | 1604 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|      62 | 1605 | `							 && !VmMemberNextIsWrite(pNext)` |
|      39 | 1606 | `							 && PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) ){` |
|       - | 1607 | `								/* Subscript-write base on a class with __get: php reads` |
|       - | 1608 | `								 * through the magic layer (the write lands on the temp and` |
|       - | 1609 | `								 * is lost, like php's indirect-modification case). A PLAIN` |
|       - | 1610 | `								 * store (bPlainStore — the compiler tags those` |
|       - | 1611 | `								 * PH7_MEMBER_WRITE too) is NOT this case: it falls through` |
|       - | 1612 | `								 * to dynamic creation. Leave the miss path — the read gate` |
|       - | 1613 | `								 * below dispatches __get. */` |
|     256 | 1614 | `							}else if( pThis->pClass->iFlags & PH7_CLASS_READONLY ){` |
|       - | 1615 | `								SyBlob sErrMsg;` |
|       3 | 1616 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 1617 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|       2 | 1618 | `									&pThis->pClass->sName,&sName);` |
|       3 | 1619 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     252 | 1620 | `							}else if( !VmClassAllowsDynamicProps(pVm,pThis->pClass) ){` |
|       - | 1621 | `								/* php 8.2 only DEPRECATES creating a dynamic property on a` |
|       - | 1622 | `								 * class without #[AllowDynamicProperties]; PHL rejects it.` |
|       - | 1623 | `								 * stdClass / __set / declared props are unaffected. */` |
|       - | 1624 | `								SyBlob sErrMsg;` |
|      43 | 1625 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      43 | 1626 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|      38 | 1627 | `									&pThis->pClass->sName,&sName);` |
|      43 | 1628 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      24 | 1629 | `							}else{` |
|     212 | 1630 | `								PH7_VmCreateDynamicAttr(&(*pVm),pThis,sName.zString,sName.nByte,&pObjAttr);` |
|       - | 1631 | `							}` |
|       - | 1632 | `						}` |
|    1049 | 1633 | `						if( pObjAttr && VmMemberNextIsRmw(pNext) ){` |
|       - | 1634 | `							/* A read-modify-write READS the property before it writes it,` |
|       - | 1635 | `							 * so php warns that the name it just created was undefined and` |
|       - | 1636 | ``							 * then goes on with null — `$o->hits++` on a fresh object is`` |
|       - | 1637 | ``							 * `Undefined property: C::$hits` and then int(1). PHL created`` |
|       - | 1638 | `							 * the slot in silence. Only the read-modify-write forms:` |
|       - | 1639 | ``							 * a subscript-write base (`$o->arr[] = 1`) and a `??=` read`` |
|       - | 1640 | `							 * nothing, and php says nothing for either. */` |
|      19 | 1641 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|       6 | 1642 | `								&pClass->sName,&sName);` |
|       6 | 1643 | `						}` |
|     522 | 1644 | `					}` |
|     764 | 1645 | `				}` |
|       - | 1646 | `				/* An UNINITIALIZED typed property is deferred for the same reason a missing` |
|       - | 1647 | `				 * one is: which diagnostic php raises depends on the parameter. A by-VALUE` |
|       - | 1648 | ``				 * binding is the ordinary read Error (`Typed property ... must not be`` |
|       - | 1649 | ``				 * accessed before initialization`), a by-REFERENCE one is php's other`` |
|       - | 1650 | ``				 * sentence (`Cannot access uninitialized non-nullable property ... by`` |
|       - | 1651 | ``				 * reference`) -- or a NULL bind when the type admits null. A HOOKED`` |
|       - | 1652 | `				 * property is not one of these: its backing store is never what answers,` |
|       - | 1653 | `				 * and the hook rail below has php's own refusal for it -- and neither is an` |
|       - | 1654 | `` 				 * INACCESSIBLE one: php screens visibility first, so `f($o->privateSlot)` `` |
|       - | 1655 | ``				 * is `Cannot access private property` whether the slot was written or not. */`` |
|  115312 | 1656 | `				if( pObjAttr && (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|   57358 | 1657 | `				 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|     710 | 1658 | `				 && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET` |
|     355 | 1659 | `				                                \|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0` |
|     656 | 1660 | `				 && PH7_VmClassAttrAccess(&(*pVm),pClass,pObjAttr->pAttr,FALSE)` |
|     603 | 1661 | `				 && pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|      21 | 1662 | `					pObjAttr = 0;` |
|      21 | 1663 | `					bDeferUninit = 1;` |
|      10 | 1664 | `				}` |
|  115312 | 1665 | `				if( pObjAttr == 0 && pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH` |
|     162 | 1666 | `				 && (bDeferUninit` |
|     139 | 1667 | `				  \|\| !(PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)` |
|      72 | 1668 | `				    && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g'))) ){` |
|       - | 1669 | `					/* D1 commit 2: deferred property arg, property absent on a REACHABLE object` |
|       - | 1670 | `					 * base ($o is a real slot). Record a property step rooted at $o so OP_CALL can` |
|       - | 1671 | `					 * vivify+bind (by-ref) or read via __get / warn (by-value). A magic __get/__set` |
|       - | 1672 | `					 * class is recorded the same way; the resolve step emits php's Notice for the` |
|       - | 1673 | `					 * by-ref case and dispatches __get for by-value. */` |
|       - | 1674 | `					SyString sProp;` |
|     107 | 1675 | `					VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|     107 | 1676 | `					SyStringInitFromBuf(&sProp,sName.zString,sName.nByte);` |
|     107 | 1677 | `					if( pPath && VmDeferPathPushProp(pPath,&sProp) == SXRET_OK ){` |
|     107 | 1678 | `						VmPopOperand(&pTos,1);       /* drop the property name */` |
|     107 | 1679 | `						pThis->iRef++;` |
|     107 | 1680 | `						PH7_MemObjRelease(pTos);     /* collapse the object slot into the carrier */` |
|     107 | 1681 | `						pTos->x.pOther = pPath;` |
|     107 | 1682 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|     107 | 1683 | `						pTos->nIdx = SXU32_HIGH;` |
|     107 | 1684 | `						PH7_ClassInstanceUnref(pThis);` |
|     107 | 1685 | `						VM_EXIT_BREAK;` |
|       - | 1686 | `					}` |
|     ! 0 | 1687 | `					if( pPath ){` |
|     ! 0 | 1688 | `						VmFreeDeferredPath(pPath);` |
|     ! 0 | 1689 | `					}` |
|       - | 1690 | `					/* fall through to the normal miss handling on allocation failure */` |
|     ! 0 | 1691 | `				}` |
|  115208 | 1692 | `				if( pObjAttr == 0` |
|   58220 | 1693 | `				 && (pInstr->iP2 == PH7_MEMBER_READ \|\| pInstr->iP2 == PH7_MEMBER_COALESCE) ){` |
|       - | 1694 | `					/* A LAZY native property whose class answers a read from its ZEROED` |
|       - | 1695 | `					 * struct (DatePeriod), asked before anything installed the set. php` |
|       - | 1696 | ``					 * consults that read handler in the plain read AND in `??` -- its third`` |
|       - | 1697 | `` 					 * accessor level takes the property's VALUE, so `$p->recurrences ?? 'd'` `` |
|       - | 1698 | `					 * is 0 there -- while isset()/empty() go to the has_property handler,` |
|       - | 1699 | `					 * which answers false for an object that has no struct at all. */` |
|     353 | 1700 | `					ph7_class_attr *pLz = PH7_ClassExtractAttribute(pClass,` |
|     116 | 1701 | `						SyStringData(&sName),SyStringLength(&sName));` |
|     232 | 1702 | `					if( pLz && (pLz->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT)` |
|      36 | 1703 | `					 && PH7_ATTR_LAZY_ABSENT(pLz,pThis) ){` |
|      25 | 1704 | `						pLazyDefault = pLz;` |
|      12 | 1705 | `					}` |
|     116 | 1706 | `				}` |
|  115213 | 1707 | `				if( pObjAttr == 0 ){` |
|       - | 1708 | `					/* Missing property. On a plain READ, php dispatches __get($name) and the` |
|       - | 1709 | `					 * expression takes its RETURN VALUE (band A #3a — pre-fix the result was` |
|       - | 1710 | `					 * discarded and a PHL-native warn fired even when __get existed). isset/` |
|       - | 1711 | `					 * empty context consults __isset first (then __get for empty()'s value` |
|       - | 1712 | `					 * test); a plain-store write context parked a pending __set above. A` |
|       - | 1713 | `					 * self-recursive read of the same property falls back to the` |
|       - | 1714 | `					 * undefined-property path via the guard, like php's property guard. */` |
|    1227 | 1715 | `					ph7_class_method *pGetMagic = 0;` |
|    1227 | 1716 | `					if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|     197 | 1717 | `						ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|     197 | 1718 | `						if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 1719 | `							ph7_value sIssetRet;` |
|       - | 1720 | `							int bSet;` |
|      61 | 1721 | `							PH7_MemObjInit(pVm,&sIssetRet);` |
|      61 | 1722 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|      61 | 1723 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|      61 | 1724 | `							VmMagicGuardPop(pVm);` |
|      61 | 1725 | `							PH7_MemObjToBool(&sIssetRet);` |
|      61 | 1726 | `							bSet = sIssetRet.x.iVal != 0;` |
|      61 | 1727 | `							PH7_MemObjRelease(&sIssetRet);` |
|      61 | 1728 | `							if( bSet && VmMemberCtxWantsValue(pInstr->iP2) ){` |
|       - | 1729 | ``								/* empty() and `??`: __isset said set, so php goes on to`` |
|       - | 1730 | `								 * __get for the VALUE — emptiness is judged on it, and the` |
|       - | 1731 | `								 * coalesce simply IS it (a null answer then takes the` |
|       - | 1732 | `								 * default, which OP_NULLC does for free). */` |
|      30 | 1733 | `								ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       - | 1734 | `								ph7_value sEmptyVal;` |
|      30 | 1735 | `								PH7_MemObjInit(pVm,&sEmptyVal);` |
|      30 | 1736 | `								if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|      30 | 1737 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|      30 | 1738 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|      30 | 1739 | `									VmMagicGuardPop(pVm);` |
|      14 | 1740 | `								}` |
|      30 | 1741 | `								VmPopOperand(&pTos,1);` |
|      30 | 1742 | `								pThis->iRef++;` |
|      30 | 1743 | `								PH7_MemObjRelease(pTos);` |
|      30 | 1744 | `								PH7_MemObjStore(&sEmptyVal,pTos);` |
|      30 | 1745 | `								pTos->nIdx = SXU32_HIGH;` |
|      30 | 1746 | `								PH7_MemObjRelease(&sEmptyVal);` |
|      30 | 1747 | `								PH7_ClassInstanceUnref(pThis);` |
|      30 | 1748 | `								VM_EXIT_BREAK;` |
|       - | 1749 | `							}` |
|       - | 1750 | `							/* isset(): the truth of __isset IS the answer — push a non-null` |
|       - | 1751 | `							 * marker for true, NULL for false (isset only tests null-ness). */` |
|      33 | 1752 | `							VmPopOperand(&pTos,1);` |
|      33 | 1753 | `							pThis->iRef++;` |
|      33 | 1754 | `							PH7_MemObjRelease(pTos);` |
|      33 | 1755 | `							if( bSet ){` |
|      17 | 1756 | `								pTos->x.iVal = 1;` |
|      17 | 1757 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       7 | 1758 | `							}` |
|      33 | 1759 | `							pTos->nIdx = SXU32_HIGH;` |
|      33 | 1760 | `							PH7_ClassInstanceUnref(pThis);` |
|      33 | 1761 | `							VM_EXIT_BREAK;` |
|       - | 1762 | `						}` |
|      68 | 1763 | `					}` |
|    1164 | 1764 | `					if( (!VmMemberCtxIsLookup(pInstr->iP2) \|\| pInstr->iP2 == PH7_MEMBER_COALESCE)` |
|    1112 | 1765 | `					 && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|    1117 | 1766 | `					 && !VmMemberNextIsWrite(pInstr + 1) ){` |
|       - | 1767 | `						/* Plain reads AND the ??=/subscript write-base (PH7_MEMBER_WRITE,` |
|       - | 1768 | `						 * which php reads through __get); read-modify-write forms are` |
|       - | 1769 | `						 * excluded (they vivified above — approximate, recorded), and so is` |
|       - | 1770 | `						 * a destructuring target (a pure write — php never reads it).` |
|       - | 1771 | ``						 * `??` reaches here only when the class declares NO __isset — the`` |
|       - | 1772 | `						 * branch above has returned otherwise — so php's gate is absent and` |
|       - | 1773 | `						 * __get answers on its own. */` |
|     263 | 1774 | `						pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|     129 | 1775 | `					}` |
|    1169 | 1776 | `					if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 1777 | `						ph7_value sMagicRet;` |
|     114 | 1778 | `						PH7_MemObjInit(pVm,&sMagicRet);` |
|     114 | 1779 | `						VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     114 | 1780 | `						PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|     114 | 1781 | `						VmMagicGuardPop(pVm);` |
|     114 | 1782 | `						if( VmMemberFetchForWrite(pInstr) ){` |
|       - | 1783 | `							/* php's indirect-modification notice, and the separation` |
|       - | 1784 | `							 * that makes the write it describes land nowhere. Raised` |
|       - | 1785 | `							 * BEFORE the pop below: sName still aliases the NAME` |
|       - | 1786 | ``							 * operand when the name is dynamic (`$o->$k[0] = v`). */`` |
|      13 | 1787 | `							PH7_VmOverloadedPropNotice(&(*pVm),pClass,&sName,&sMagicRet);` |
|     108 | 1788 | `						}else if( pInstr->iP2 == PH7_MEMBER_DEFPATH ){` |
|       - | 1789 | `							/* A deferred call ARGUMENT: __get has answered — php calls it` |
|       - | 1790 | `							 * where the property is WRITTEN, whatever the parameter turns` |
|       - | 1791 | `							 * out to be — and only the by-ref verdict is still pending.` |
|       - | 1792 | `							 * Carry the value with the class and name that produced it, so` |
|       - | 1793 | `							 * the notice lands at the call for a by-REFERENCE parameter and` |
|       - | 1794 | `							 * nothing is said for a by-value one, without __get running` |
|       - | 1795 | `							 * twice or a second read arriving after a later argument's side` |
|       - | 1796 | `							 * effects. */` |
|      54 | 1797 | `							VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_PROP,` |
|      17 | 1798 | `								pClass,&sName,&sMagicRet);` |
|      37 | 1799 | `							if( pPre ){` |
|      37 | 1800 | `								VmPopOperand(&pTos,1);   /* drop the property name */` |
|      37 | 1801 | `								pThis->iRef++;` |
|      37 | 1802 | `								PH7_MemObjRelease(pTos); /* collapse the object slot into the carrier */` |
|      37 | 1803 | `								pTos->x.pOther = pPre;` |
|      37 | 1804 | `								pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      37 | 1805 | `								pTos->nIdx = SXU32_HIGH;` |
|      37 | 1806 | `								PH7_MemObjRelease(&sMagicRet);` |
|      37 | 1807 | `								PH7_ClassInstanceUnref(pThis);` |
|      37 | 1808 | `								VM_EXIT_BREAK;` |
|       - | 1809 | `							}` |
|     ! 0 | 1810 | `						}` |
|       - | 1811 | `						/* Pop the attribute name, replace the object slot with the magic` |
|       - | 1812 | `						 * result (a temp, not an lvalue — nIdx stays constant). A throw` |
|       - | 1813 | `						 * from __get parked at the boundary; the fetch-point router lands` |
|       - | 1814 | `						 * it and abandons this slot. */` |
|      80 | 1815 | `						VmPopOperand(&pTos,1);` |
|      80 | 1816 | `						pThis->iRef++;` |
|      80 | 1817 | `						PH7_MemObjRelease(pTos);` |
|      80 | 1818 | `						PH7_MemObjStore(&sMagicRet,pTos);` |
|      80 | 1819 | `						pTos->nIdx = SXU32_HIGH;` |
|      80 | 1820 | `						PH7_MemObjRelease(&sMagicRet);` |
|      80 | 1821 | `						PH7_ClassInstanceUnref(pThis);` |
|      80 | 1822 | `						VM_EXIT_BREAK;` |
|       - | 1823 | `					}` |
|       - | 1824 | `					/* No __get (or guard held): load null. In isset()/empty() context (iP2 3/4)` |
|       - | 1825 | `					 * PHP returns false/true SILENTLY, so suppress the read-miss warning there` |
|       - | 1826 | `					 * (mirrors the array LOAD_IDX iP2=4/6 suppression); likewise when a` |
|       - | 1827 | `					 * pending __set was parked above (the following OP_STORE dispatches it —` |
|       - | 1828 | `					 * nothing is "undefined" about that write) or a throw is parked on the` |
|       - | 1829 | `					 * boundary rail (e.g. the readonly-class dynamic-property Error — the` |
|       - | 1830 | `					 * fetch-point router lands it right after this op). */` |
|    1054 | 1831 | `					if( !VmMemberCtxIsLookup(pInstr->iP2) && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|     924 | 1832 | `					 && pVm->pMagicSetThis == 0` |
|     544 | 1833 | `					 && pVm->nBoundaryRc == 0 ){` |
|       - | 1834 | `						/* A property missing from the INSTANCE may still be DECLARED by the` |
|       - | 1835 | ``						 * class — that is what `unset($o->p)` leaves behind, and php keeps`` |
|       - | 1836 | `						 * answering for the declaration rather than calling the name` |
|       - | 1837 | `						 * undefined: the visibility screen still applies, and a TYPED slot` |
|       - | 1838 | `						 * is back to uninitialized (the same Error a never-written one` |
|       - | 1839 | `						 * raises). Only a name the class does not declare at all is the` |
|       - | 1840 | `						 * "Undefined property" warning. A destructuring target is silent` |
|       - | 1841 | `						 * either way: php created the property above or dispatched __set. */` |
|     174 | 1842 | `						ph7_class_attr *pDeclAttr = PH7_ClassScopedAttribute(&(*pVm),pClass,` |
|      57 | 1843 | `							SyStringData(&sName),SyStringLength(&sName));` |
|       - | 1844 | `						/* A static property, a class constant and a native engine slot are` |
|       - | 1845 | `						 * not instance properties; a dynamic one is gone for good once it` |
|       - | 1846 | `						 * is unset, since nothing declares it. */` |
|     114 | 1847 | `						if( pDeclAttr` |
|      76 | 1848 | `						 && (pDeclAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|       - | 1849 | `						                          \|PH7_CLASS_ATTR_HIDDEN\|PH7_CLASS_ATTR_DYNAMIC)) ){` |
|       9 | 1850 | `							pDeclAttr = 0;` |
|       3 | 1851 | `						}` |
|     117 | 1852 | `						if( pDeclAttr && PH7_ATTR_LAZY_ABSENT(pDeclAttr,pThis) ){` |
|       - | 1853 | `							/* The object has never held this name. php has two answers and` |
|       - | 1854 | `							 * the class says which: a read handler over the ZEROED struct` |
|       - | 1855 | `							 * (DatePeriod -- null/0/false, in silence), or nothing at all` |
|       - | 1856 | `							 * (DateInterval), which is the ordinary "Undefined property"` |
|       - | 1857 | `							 * warning. Either way the DECLARATION is not what answers, so` |
|       - | 1858 | `							 * the typed-slot Error below must not fire on a name php keeps` |
|       - | 1859 | `							 * no slot for. */` |
|      25 | 1860 | `							if( pDeclAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT ){` |
|      21 | 1861 | `								pLazyDefault = pDeclAttr;` |
|      10 | 1862 | `							}` |
|      25 | 1863 | `							pDeclAttr = 0;` |
|      12 | 1864 | `						}` |
|     117 | 1865 | `						if( pDeclAttr && (pDeclAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){` |
|       - | 1866 | `							/* ...and a VIRTUAL property is answered by the class's handler` |
|       - | 1867 | `							 * too, so the DECLARATION must not raise the typed-slot Error` |
|       - | 1868 | `							 * for a name php keeps no slot for either. */` |
|     ! 0 | 1869 | `							pDeclAttr = 0;` |
|     ! 0 | 1870 | `						}` |
|     117 | 1871 | `						if( pLazyDefault ){` |
|       - | 1872 | `							/* php's read handler answered: say nothing. */` |
|     105 | 1873 | `						}else if( pDeclAttr` |
|      51 | 1874 | `						 && !PH7_VmClassAttrAccess(&(*pVm),pClass,pDeclAttr,FALSE) ){` |
|       - | 1875 | `							SyBlob sErrMsg;` |
|     ! 0 | 1876 | `							const char *zVis = pDeclAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|     ! 0 | 1877 | `								? "private" : "protected";` |
|     ! 0 | 1878 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     ! 0 | 1879 | `							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",` |
|     ! 0 | 1880 | `								zVis,&pClass->sName,&sName);` |
|     ! 0 | 1881 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",` |
|       - | 1882 | `								sizeof("Error")-1,&sErrMsg));` |
|      97 | 1883 | `						}else if( pDeclAttr && (pDeclAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|     ! 0 | 1884 | `							VmBoundaryPark(&(*pVm),` |
|     ! 0 | 1885 | `								VmThrowUninitializedPropertyError(&(*pVm),pClass,pDeclAttr));` |
|     ! 0 | 1886 | `						}else{` |
|     144 | 1887 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|      47 | 1888 | `								&pClass->sName,&sName);` |
|       - | 1889 | `						}` |
|      57 | 1890 | `					}` |
|     527 | 1891 | `				}` |
|  115040 | 1892 | `				if( pObjAttr && (pObjAttr->iState & VM_CLASS_ATTR_RDONLY)` |
|   57018 | 1893 | `				 && (pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|      35 | 1894 | `				  \|\| ((pInstr + 1)->iOp == PH7_OP_STORE && (pInstr + 1)->iP2 != 0)) ){` |
|       - | 1895 | `					/* php's write_property handler refuses the plain store and the` |
|       - | 1896 | `					 * destructuring one that goes through it; everything that takes a` |
|       - | 1897 | ``					 * POINTER to the property instead -- a compound assign, `++`, `??=`,`` |
|       - | 1898 | `					 * a reference bind -- bypasses the handler in php and is left alone` |
|       - | 1899 | `					 * here too. */` |
|       7 | 1900 | `					VmBoundaryPark(&(*pVm),VmThrowNativeReadOnly(&(*pVm),pObjAttr->pAttr));` |
|       7 | 1901 | `					VmPopOperand(&pTos,1);` |
|       7 | 1902 | `					PH7_MemObjRelease(pTos);` |
|       7 | 1903 | `					pTos->nIdx = SXU32_HIGH;` |
|       7 | 1904 | `					VM_EXIT_BREAK;` |
|       - | 1905 | `				}` |
|  115039 | 1906 | `				VmPopOperand(&pTos,1);` |
|       - | 1907 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|       - | 1908 | `				 * This is due to the following case:` |
|       - | 1909 | `				 *     (new TestClass())->foo;` |
|       - | 1910 | `				 */` |
|  115039 | 1911 | `				pThis->iRef++;` |
|  115039 | 1912 | `				PH7_MemObjRelease(pTos);` |
|  115039 | 1913 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|  115039 | 1914 | `				if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - | 1915 | ``					/* `$o->p =& $x`: stash the resolved instance property slot for the`` |
|       - | 1916 | `					 * following member-marked OP_STORE_REF, which rebinds it to alias the` |
|       - | 1917 | `					 * source variable. Do NOT run the read/hook/magic/uninit machinery` |
|       - | 1918 | `					 * below — a reference bind neither reads the value nor triggers` |
|       - | 1919 | `					 * get/set hooks or an uninitialized-typed Error. pThis stays retained` |
|       - | 1920 | `					 * (the iRef++ above); OP_STORE_REF releases it. */` |
|      58 | 1921 | `					if( pObjAttr && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE) ){` |
|       - | 1922 | `						/* A reference bind is a WRITE, and php's refusing handler answers` |
|       - | 1923 | `						 * it with the same sentence a plain store gets. */` |
|     ! 0 | 1924 | `						VmBoundaryPark(&(*pVm),` |
|     ! 0 | 1925 | `							VmThrowNativeNoWrite(&(*pVm),pObjAttr->pOwner,pObjAttr->pAttr));` |
|     ! 0 | 1926 | `						pVm->pRefTargetAttr = 0;` |
|     ! 0 | 1927 | `						pVm->pRefTargetThis = 0;` |
|     ! 0 | 1928 | `						pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 1929 | `						PH7_ClassInstanceUnref(pThis);` |
|      59 | 1930 | `					}else if( pObjAttr && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) ){` |
|       - | 1931 | ``						/* `$i->f =& $x`: php REFUSES to make a handler-backed property`` |
|       - | 1932 | `						 * the target of a reference — there is no slot to rebind, and` |
|       - | 1933 | `						 * every write through the alias would skip the conversion the` |
|       - | 1934 | `						 * handler is there to do. PHL rebound the slot instead, so` |
|       - | 1935 | ``						 * `$x = 2.5` afterwards wrote 2.5 microseconds-free into the`` |
|       - | 1936 | `						 * interval. */` |
|       - | 1937 | `						SyBlob sErrMsg;` |
|       3 | 1938 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 1939 | `						SyBlobAppend(&sErrMsg,"Cannot assign by reference to overloaded object",` |
|       - | 1940 | `							sizeof("Cannot assign by reference to overloaded object")-1);` |
|       3 | 1941 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       3 | 1942 | `						pVm->pRefTargetAttr = 0;` |
|       3 | 1943 | `						pVm->pRefTargetThis = 0;` |
|       3 | 1944 | `						pVm->pRefTargetStaticAttr = 0;` |
|       3 | 1945 | `						PH7_ClassInstanceUnref(pThis);` |
|      57 | 1946 | `					}else if( pObjAttr ){` |
|      56 | 1947 | `						pVm->pRefTargetAttr = pObjAttr;` |
|      56 | 1948 | `						pVm->pRefTargetThis = pThis;` |
|      56 | 1949 | `						pVm->pRefTargetStaticAttr = 0;` |
|      56 | 1950 | `						pTos->nIdx = pObjAttr->nIdx;` |
|      29 | 1951 | `					}else{` |
|       - | 1952 | `						/* Missing/inaccessible target: leave nothing stashed; OP_STORE_REF` |
|       - | 1953 | `						 * no-ops. Balance the retain. */` |
|     ! 0 | 1954 | `						pVm->pRefTargetAttr = 0;` |
|     ! 0 | 1955 | `						pVm->pRefTargetThis = 0;` |
|     ! 0 | 1956 | `						pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 1957 | `						PH7_ClassInstanceUnref(pThis);` |
|       - | 1958 | `					}` |
|      58 | 1959 | `					VM_EXIT_BREAK;` |
|       - | 1960 | `				}` |
|  114983 | 1961 | `				if( pLazyDefault && pLazyDefault->pNativeValue ){` |
|       - | 1962 | `					/* A LAZY native property read before its class installed the set,` |
|       - | 1963 | `					 * on a class that answers such a read from its ZEROED struct. The` |
|       - | 1964 | `					 * declared literal IS that struct's field (the 76th session moved` |
|       - | 1965 | `					 * DatePeriod's seven defaults onto it), and the answer is a` |
|       - | 1966 | `					 * TEMPORARY: nothing was installed, so there is no slot to address` |
|       - | 1967 | `					 * and nIdx stays the constant sentinel the pop left. */` |
|      25 | 1968 | `					PH7_NativeLiteralValue(&(*pVm),pLazyDefault->pNativeValue,pTos);` |
|      12 | 1969 | `				}` |
|  114983 | 1970 | `				if( pObjAttr ){` |
|  113929 | 1971 | `					ph7_value *pValue = 0; /* cc warning */` |
|       - | 1972 | `					/* Check attribute access */` |
|  113929 | 1973 | `					if( PH7_VmClassAttrAccess(&(*pVm),pClass,pObjAttr->pAttr,FALSE) ){` |
|  113878 | 1974 | `						if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET))` |
|   57129 | 1975 | `						 && !VmHookGuardHeld(pVm,(void *)pThis,&sName) ){` |
|       - | 1976 | `							/* PHP 8.4 property hooks: route reads, writes, and the` |
|       - | 1977 | `							 * read-modify-write forms through the synthesized hook` |
|       - | 1978 | `							 * methods. A held guard (either kind) means we are INSIDE` |
|       - | 1979 | `							 * one of this property's own hook bodies — fall through to` |
|       - | 1980 | `` 							 * the raw backing slot for BOTH directions (php: `$this->x` `` |
|       - | 1981 | `							 * within any of x's hooks addresses the backing store). */` |
|     232 | 1982 | `							VmInstr *pNextH = pInstr + 1;` |
|     232 | 1983 | `							int bPlainStore = (pNextH->iOp == PH7_OP_STORE && pNextH->iP2 != 0);` |
|     394 | 1984 | `							int bCoalesceW = pInstr->iP2 == PH7_MEMBER_WRITE` |
|     228 | 1985 | `								&& pNextH->iOp == PH7_OP_NULLC_JMP;` |
|       - | 1986 | `							/* ++/--/compound-assign: the modify op FOLLOWS the member` |
|       - | 1987 | `							 * directly (the compiler tags compound-assign members` |
|       - | 1988 | `							 * PH7_MEMBER_WRITE too, so classify by the next op, not iP2;` |
|       - | 1989 | `							 * the !bPlainStore term is load-bearing — VmMemberNextIsWrite` |
|       - | 1990 | `							 * returns 1 for a member OP_STORE too) */` |
|     232 | 1991 | `							int bRmwNext = !bPlainStore && VmMemberNextIsWrite(pNextH);` |
|     275 | 1992 | `							int bSubscriptW = !bPlainStore && !bCoalesceW && !bRmwNext` |
|     316 | 1993 | `								&& pInstr->iP2 == PH7_MEMBER_WRITE;` |
|     232 | 1994 | `							if( bPlainStore ){` |
|       - | 1995 | `								/* Arm the pending hook-set for the following OP_STORE — it` |
|       - | 1996 | `								 * dispatches the set hook, or throws the read-only Error` |
|       - | 1997 | `								 * when the property only has a get hook. (The scalar` |
|       - | 1998 | `								 * transient is safe here: its window is exactly one` |
|       - | 1999 | `								 * instruction, MEMBER -> STORE, nothing runs in between.) */` |
|      55 | 2000 | `								pThis->iRef++;` |
|      55 | 2001 | `								pVm->pHookSetThis = pThis;` |
|      55 | 2002 | `								pVm->pHookSetAttr = pObjAttr->pAttr;` |
|      55 | 2003 | `								pVm->nHookSetIdx = pObjAttr->nIdx;` |
|      55 | 2004 | `								pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|      55 | 2005 | `								PH7_ClassInstanceUnref(pThis);` |
|      55 | 2006 | `								VM_EXIT_BREAK;` |
|       - | 2007 | `							}` |
|     180 | 2008 | `							if( bSubscriptW ){` |
|       - | 2009 | `								/* Subscript write base ($o->m[] = v, $o->m[$k] = v):` |
|       - | 2010 | `								 * php's catchable Error, with or without a set hook` |
|       - | 2011 | ``								 * (only a by-ref `&get` hook would allow it — a loud`` |
|       - | 2012 | `								 * compile error in PHL). The op completes benignly with` |
|       - | 2013 | `								 * a null temp; the fetch-point router lands the throw. */` |
|       - | 2014 | `								SyBlob sErrMsg;` |
|       7 | 2015 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       7 | 2016 | `								SyBlobFormat(&sErrMsg,"Indirect modification of %z::$%z is not allowed",` |
|       6 | 2017 | `									&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       7 | 2018 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       7 | 2019 | `								PH7_ClassInstanceUnref(pThis);` |
|       7 | 2020 | `								VM_EXIT_BREAK;` |
|       - | 2021 | `							}` |
|     170 | 2022 | `							if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|      89 | 2023 | `							 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 2024 | `								/* VIRTUAL set-only property: there is no backing store to` |
|       - | 2025 | `								 * fall back to, so EVERY read context — plain read,` |
|       - | 2026 | `								 * isset()/empty() (php throws there too, probe-verified),` |
|       - | 2027 | `								 * the ??= test, an RMW read — is php's catchable` |
|       - | 2028 | `								 * write-only Error. */` |
|       - | 2029 | `								SyBlob sErrMsg;` |
|       9 | 2030 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       9 | 2031 | `								SyBlobFormat(&sErrMsg,"Property %z::$%z is write-only",` |
|       8 | 2032 | `									&pThis->pClass->sName,&pObjAttr->pAttr->sName);` |
|       9 | 2033 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       9 | 2034 | `								PH7_ClassInstanceUnref(pThis);` |
|       9 | 2035 | `								VM_EXIT_BREAK;` |
|       - | 2036 | `							}` |
|     166 | 2037 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2038 | ``								/* isset()/empty()/`??` on a hooked property all call the get`` |
|       - | 2039 | `								 * hook (php): isset is "get() !== null", empty tests the` |
|       - | 2040 | ``								 * value, and `??` IS the value. */`` |
|       - | 2041 | `								ph7_value sHookRet;` |
|      16 | 2042 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|      16 | 2043 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|      16 | 2044 | `									if( !VmMemberCtxWantsValue(pInstr->iP2) ){` |
|       - | 2045 | `										/* isset(): the trailing builtin tests NULL-ness of what` |
|       - | 2046 | `										 * it is handed, so "not set" has to BE null. Storing` |
|       - | 2047 | ``										 * bool(FALSE) here made `isset($o->p)` answer TRUE for`` |
|       - | 2048 | `										 * a get hook returning null — the same non-null marker` |
|       - | 2049 | `										 * convention the __isset path uses. */` |
|       8 | 2050 | `										if( (sHookRet.iFlags & MEMOBJ_NULL) == 0 ){` |
|       6 | 2051 | `											pTos->x.iVal = 1;` |
|       6 | 2052 | `											MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       4 | 2053 | `										}else{` |
|       3 | 2054 | `											MemObjSetType(pTos,MEMOBJ_NULL);` |
|       - | 2055 | `										}` |
|       5 | 2056 | `									}else{` |
|       - | 2057 | ``										/* empty() judges the value; `??` IS the value. */`` |
|      10 | 2058 | `										PH7_MemObjStore(&sHookRet,pTos);` |
|       - | 2059 | `									}` |
|      16 | 2060 | `									pTos->nIdx = SXU32_HIGH;` |
|      16 | 2061 | `									PH7_MemObjRelease(&sHookRet);` |
|      16 | 2062 | `									PH7_ClassInstanceUnref(pThis);` |
|      16 | 2063 | `									VM_EXIT_BREAK;` |
|       - | 2064 | `								}` |
|     ! 0 | 2065 | `								PH7_MemObjRelease(&sHookRet);` |
|     ! 0 | 2066 | `							}` |
|     152 | 2067 | `							if( !bCoalesceW && !bRmwNext && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2068 | `								/* Read: dispatch __phl_hook_get_NAME (inheritance-aware) */` |
|       - | 2069 | `								ph7_value sHookRet;` |
|     118 | 2070 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|     118 | 2071 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|     104 | 2072 | `									if( pInstr->iP2 == PH7_MEMBER_DEFPATH ){` |
|       - | 2073 | `										/* A deferred call ARGUMENT of a HOOKED property. The` |
|       - | 2074 | `										 * get hook has answered, as php's does; what a` |
|       - | 2075 | `										 * by-REFERENCE parameter then gets is not a notice` |
|       - | 2076 | `										 * but php's refusal — a hook has no slot to alias` |
|       - | 2077 | ``										 * and `&get` does not exist here — while a by-VALUE`` |
|       - | 2078 | `										 * one simply takes the hook's value. */` |
|      60 | 2079 | `										VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),` |
|      38 | 2080 | `											VM_OVER_HOOK,pThis->pClass,&pObjAttr->pAttr->sName,&sHookRet);` |
|      41 | 2081 | `										if( pPre ){` |
|      41 | 2082 | `											PH7_MemObjRelease(pTos);` |
|      41 | 2083 | `											pTos->x.pOther = pPre;` |
|      41 | 2084 | `											pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      41 | 2085 | `											pTos->nIdx = SXU32_HIGH;` |
|      41 | 2086 | `											PH7_MemObjRelease(&sHookRet);` |
|      41 | 2087 | `											PH7_ClassInstanceUnref(pThis);` |
|      72 | 2088 | `											VM_EXIT_BREAK;` |
|       - | 2089 | `										}` |
|     ! 0 | 2090 | `									}` |
|      65 | 2091 | `									PH7_MemObjStore(&sHookRet,pTos);` |
|      65 | 2092 | `									pTos->nIdx = SXU32_HIGH;` |
|      65 | 2093 | `									PH7_MemObjRelease(&sHookRet);` |
|      65 | 2094 | `									PH7_ClassInstanceUnref(pThis);` |
|      65 | 2095 | `									VM_EXIT_BREAK;` |
|       - | 2096 | `								}` |
|      15 | 2097 | `								PH7_MemObjRelease(&sHookRet);` |
|       7 | 2098 | `							}` |
|      49 | 2099 | `							if( bCoalesceW \|\| bRmwNext ){` |
|       - | 2100 | ``								/* `$o->x ??= v` (bCoalesceW) and the read-modify-write`` |
|       - | 2101 | ``								 * forms `$o->x++`, `$o->x .= v`, … (bRmwNext): php reads`` |
|       - | 2102 | `								 * through the get hook (raw backing when the property is` |
|       - | 2103 | `								 * set-only) and writes through the set hook.` |
|       - | 2104 | `								 *   - ??=: the test value goes on the stack and a` |
|       - | 2105 | `								 *     COAL_HOOK entry is pushed for the OP_NULLC_STORE` |
|       - | 2106 | `								 *     at the jump target; the fetch-point sweep drops it` |
|       - | 2107 | `								 *     when the short-circuit jump skips the assign or a` |
|       - | 2108 | `								 *     throw abandons the RHS (the entry is NOT a scalar` |
|       - | 2109 | `								 *     transient — nested stores/coalesces in the RHS` |
|       - | 2110 | `								 *     stack their own entries above it).` |
|       - | 2111 | `								 *   - RMW: the value goes into a fresh SCRATCH slot the` |
|       - | 2112 | `								 *     modify op mutates in place; the entry makes the` |
|       - | 2113 | `								 *     op's tail dispatch the set side with the computed` |
|       - | 2114 | `								 *     value (PH7_HOOK_RMW_WRITEBACK). */` |
|       - | 2115 | `								ph7_value sCur;` |
|       - | 2116 | `								sxi32 rcCur;` |
|      35 | 2117 | `								PH7_MemObjInit(pVm,&sCur);` |
|      35 | 2118 | `								rcCur = PH7_VmHookGetAttrValue(pThis,pObjAttr,&sCur);` |
|      35 | 2119 | `								if( rcCur == SXERR_NOTFOUND ){` |
|       - | 2120 | `									/* set-only hook (or guard edge): the read side is the` |
|       - | 2121 | `									 * raw backing store (php) */` |
|       3 | 2122 | `									ph7_value *pBack = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|       3 | 2123 | `									if( pBack ){` |
|       3 | 2124 | `										PH7_MemObjStore(pBack,&sCur);` |
|       2 | 2125 | `									}` |
|      34 | 2126 | `								}else if( pVm->nBoundaryRc != 0 ){` |
|       - | 2127 | `									/* the get hook threw: leave the null temp; the` |
|       - | 2128 | `									 * fetch-point router lands the parked throw —` |
|       - | 2129 | `									 * nothing is armed. */` |
|     ! 0 | 2130 | `									PH7_MemObjRelease(&sCur);` |
|     ! 0 | 2131 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2132 | `									VM_EXIT_BREAK;` |
|       - | 2133 | `								}` |
|      35 | 2134 | `								if( bCoalesceW ){` |
|       - | 2135 | `									VmHookRmw sPend;` |
|      15 | 2136 | `									PH7_MemObjStore(&sCur,pTos);` |
|      15 | 2137 | `									pTos->nIdx = SXU32_HIGH;` |
|      15 | 2138 | `									PH7_MemObjRelease(&sCur);` |
|      15 | 2139 | `									sPend.iKind = VM_HOOK_PEND_COAL_HOOK;` |
|      15 | 2140 | `									sPend.pThis = pThis;` |
|      15 | 2141 | `									sPend.pAttr = pObjAttr->pAttr;` |
|      15 | 2142 | `									sPend.nBackIdx = pObjAttr->nIdx;` |
|      15 | 2143 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|      15 | 2144 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|      15 | 2145 | `									sPend.pOwnerStack = (void *)pStack;` |
|      15 | 2146 | `									sPend.pInstrs = (void *)aInstr;` |
|      15 | 2147 | `									sPend.nJmpPc = (sxu32)(pc + 1);            /* the OP_NULLC_JMP */` |
|      15 | 2148 | `									sPend.nPc = (sxu32)(pNextH->iP2 - 1);      /* the OP_NULLC_STORE */` |
|      15 | 2149 | `									pThis->iRef++;` |
|      15 | 2150 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|      15 | 2151 | `									PH7_ClassInstanceUnref(pThis);` |
|      15 | 2152 | `									VM_EXIT_BREAK;` |
|       - | 2153 | `								}` |
|       - | 2154 | `								{` |
|      21 | 2155 | `									ph7_value *pScr = PH7_ReserveMemObj(&(*pVm));` |
|      21 | 2156 | `									if( pScr ){` |
|       - | 2157 | `										VmHookRmw sRmw;` |
|      21 | 2158 | `										PH7_MemObjStore(&sCur,pScr);` |
|      21 | 2159 | `										PH7_MemObjStore(&sCur,pTos);` |
|      21 | 2160 | `										pTos->nIdx = pScr->nIdx;` |
|      21 | 2161 | `										sRmw.iKind = VM_HOOK_PEND_RMW;` |
|      21 | 2162 | `										sRmw.pThis = pThis;` |
|      21 | 2163 | `										sRmw.pAttr = pObjAttr->pAttr;` |
|      21 | 2164 | `										sRmw.nBackIdx = pObjAttr->nIdx;` |
|      21 | 2165 | `										sRmw.nScratchIdx = pScr->nIdx;` |
|      21 | 2166 | `										SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      21 | 2167 | `										sRmw.pOwnerStack = (void *)pStack;` |
|      21 | 2168 | `										sRmw.pInstrs = (void *)aInstr;` |
|      21 | 2169 | `										sRmw.nJmpPc = (sxu32)(pc + 1);  /* the modify op ... */` |
|      21 | 2170 | `										sRmw.nPc = (sxu32)(pc + 1);     /* ... is the whole window */` |
|      21 | 2171 | `										pThis->iRef++;` |
|      21 | 2172 | `										SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      10 | 2173 | `									}` |
|       - | 2174 | `									/* OOM: pScr == 0 — leave the null temp (loud allocator` |
|       - | 2175 | `									 * diagnostics already fired) */` |
|      21 | 2176 | `									PH7_MemObjRelease(&sCur);` |
|      21 | 2177 | `									PH7_ClassInstanceUnref(pThis);` |
|      21 | 2178 | `									VM_EXIT_BREAK;` |
|       - | 2179 | `								}` |
|       - | 2180 | `							}` |
|       7 | 2181 | `						}` |
|       - | 2182 | ``						/* `$o->p[$k] = v`, `$o->p[] = v` and `unset($o->p[$k])`: the`` |
|       - | 2183 | `						 * property is the BASE of a subscript write, so the write lands` |
|       - | 2184 | `						 * inside whatever it holds. php screens that where it screens a` |
|       - | 2185 | ``						 * store -- `Cannot indirectly modify readonly property C::$p` --`` |
|       - | 2186 | `						 * and it screens it BEFORE the uninitialized-typed read below,` |
|       - | 2187 | `						 * which is why this sits in front. PHL wrote through to the` |
|       - | 2188 | `						 * array a readonly property held. */` |
|       - | 2189 | `						{` |
|  113669 | 2190 | `							VmInstr *pNextI = pInstr + 1;` |
|  113669 | 2191 | `							int bStoreI = (pNextI->iOp == PH7_OP_STORE && pNextI->iP2 != 0);` |
|  113669 | 2192 | `							int bCoalI = pNextI->iOp == PH7_OP_NULLC_JMP;` |
|  172980 | 2193 | `							int bUnsetBase = pInstr->iP2 == PH7_MEMBER_READ` |
|  113680 | 2194 | `								&& pNextI->iOp == PH7_OP_LOAD_IDX && VM_IDX_IS_UNSET(pNextI->iP2);` |
|  170501 | 2195 | `							int bBaseW = bUnsetBase` |
|  219506 | 2196 | `								\|\| (pInstr->iP2 == PH7_MEMBER_WRITE` |
|  109753 | 2197 | `								    && !bStoreI && !bCoalI && !VmMemberNextIsWrite(pNextI));` |
|  113669 | 2198 | `							if( bBaseW ){` |
|     267 | 2199 | `								sxi32 rcInd = PH7_VmCheckIndirectModify(&(*pVm),pObjAttr->nIdx);` |
|     267 | 2200 | `								if( rcInd != SXRET_OK ){` |
|      10 | 2201 | `									VmBoundaryPark(&(*pVm),rcInd);` |
|      10 | 2202 | `									PH7_MemObjRelease(pTos);` |
|      10 | 2203 | `									pTos->nIdx = SXU32_HIGH;` |
|      10 | 2204 | `									PH7_ClassInstanceUnref(pThis);` |
|      10 | 2205 | `									VM_EXIT_BREAK;` |
|       - | 2206 | `								}` |
|     127 | 2207 | `							}` |
|       - | 2208 | `						}` |
|       - | 2209 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|       - | 2210 | `						 * We can only raise it on a real read, not when the slot is the` |
|       - | 2211 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|       - | 2212 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|       - | 2213 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|  113656 | 2214 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|   57122 | 2215 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|     583 | 2216 | `							VmInstr *pNext = pInstr + 1;` |
|     583 | 2217 | `							int bIsLhs = 0;` |
|     583 | 2218 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|     497 | 2219 | `								bIsLhs = 1;` |
|     246 | 2220 | `							}` |
|     583 | 2221 | `							if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 2222 | `								/* Destructuring target: the OP_LOAD_LIST store (typed-` |
|       - | 2223 | `								 * enforced) initializes it — never a read (php). */` |
|       7 | 2224 | `								bIsLhs = 1;` |
|       3 | 2225 | `							}` |
|       - | 2226 | ``							/* isset()/empty()/`??` read an uninitialized typed property`` |
|       - | 2227 | `							 * as "not set" — a silent miss, NOT the Error a plain read` |
|       - | 2228 | `							 * raises (php). The compiler tags such an access iP2 =` |
|       - | 2229 | `							 * ISSET/EMPTY; treat it like the assignment-LHS case and fall` |
|       - | 2230 | `							 * through to load the slot's NULL. */` |
|     583 | 2231 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|      28 | 2232 | `								bIsLhs = 1;` |
|      13 | 2233 | `							}` |
|       - | 2234 | ``							/* A WRITE base is not a read either. `$o->p ??= v` takes the`` |
|       - | 2235 | `							 * slot's NULL and assigns over it, and a DIMENSION write` |
|       - | 2236 | ``							 * (`$o->t['n'] = v`) AUTO-INITIALIZES an array when the`` |
|       - | 2237 | `							 * declared type has room for one -- php's` |
|       - | 2238 | `							 * zend_handle_fetch_obj_flags -- or refuses with its own` |
|       - | 2239 | `							 * TypeError when it does not. Raising the read Error here` |
|       - | 2240 | `							 * instead is what stopped Doctrine's ClassMetadata, whose` |
|       - | 2241 | ``							 * `public array $table;` is filled exactly that way. */`` |
|     583 | 2242 | `							if( (pInstr + 1)->iOp == PH7_OP_NULLC_JMP ){` |
|       3 | 2243 | ``								bIsLhs = 1;   /* `$o->p ??= v` assigns over the unset slot */`` |
|     582 | 2244 | `							}else if( VmMemberFetchIsRefSource(pInstr) ){` |
|       - | 2245 | `								/* A REFERENCE fetch has a rule of its own: php seeds NULL` |
|       - | 2246 | `								 * and binds when the declared type admits one, and refuses` |
|       - | 2247 | ``								 * with `Cannot access uninitialized non-nullable property`` |
|       - | 2248 | ``								 * ... by reference` when it does not. The auto-initialize`` |
|       - | 2249 | `								 * rule below belongs to a DIMENSION write, and running it` |
|       - | 2250 | ``								 * here turned `?int $t` into an ARRAY. */`` |
|      25 | 2251 | `								sxi32 rcRs = VmRefUninitTypedProperty(&(*pVm),pClass,pObjAttr,` |
|      16 | 2252 | `									(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pObjAttr->nIdx));` |
|      17 | 2253 | `								if( rcRs != SXRET_OK ){` |
|       9 | 2254 | `									VmBoundaryPark(&(*pVm),rcRs);` |
|       9 | 2255 | `									PH7_ClassInstanceUnref(pThis);` |
|       9 | 2256 | `									VM_EXIT_BREAK;` |
|       - | 2257 | `								}` |
|       9 | 2258 | `								bIsLhs = 1;` |
|     569 | 2259 | `							}else if( VmMemberFetchForWrite(pInstr) ){` |
|      17 | 2260 | `								sxi32 rcAI = VmAutoInitArrayProperty(&(*pVm),pObjAttr,` |
|      10 | 2261 | `									(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pObjAttr->nIdx));` |
|      12 | 2262 | `								if( rcAI != SXRET_OK ){` |
|       3 | 2263 | `									VmBoundaryPark(&(*pVm),rcAI);` |
|       3 | 2264 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 2265 | `									VM_EXIT_BREAK;` |
|       - | 2266 | `								}` |
|       9 | 2267 | `								bIsLhs = 1;` |
|       4 | 2268 | `							}` |
|     573 | 2269 | `							if( !bIsLhs ){` |
|      30 | 2270 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|      30 | 2271 | `								PH7_ClassInstanceUnref(pThis);` |
|      30 | 2272 | `								if( rcU == PH7_ABORT ){` |
|       3 | 2273 | `									VM_EXIT_ABORT;` |
|       - | 2274 | `								}` |
|       - | 2275 | `								{` |
|       - | 2276 | `									sxi32 iRp;` |
|      28 | 2277 | `									if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|       9 | 2278 | `										PH7_RESUME_DRAIN()` |
|       9 | 2279 | `										pc = iRp;` |
|       9 | 2280 | `										VM_EXIT_BREAK;` |
|       - | 2281 | `									}` |
|       - | 2282 | `								}` |
|      20 | 2283 | `								VM_EXIT_EXCEPTION;` |
|       - | 2284 | `							}` |
|     271 | 2285 | `						}` |
|       - | 2286 | `						/* Load attribute */` |
|  113625 | 2287 | `						pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|  113625 | 2288 | `						if( pValue ){` |
|  113625 | 2289 | `							if( pThis->iRef < 2 ){` |
|       - | 2290 | `								/* Perform a store operation,rather than a load operation since` |
|       - | 2291 | `								 * the class instance '$this' will be deleted shortly.` |
|       - | 2292 | `								 */` |
|     390 | 2293 | `								PH7_MemObjStore(pValue,pTos);` |
|     197 | 2294 | `							}else{` |
|       - | 2295 | `								/* Simple load */` |
|  113239 | 2296 | `								PH7_MemObjLoad(pValue,pTos);` |
|       - | 2297 | `							}` |
|  113625 | 2298 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|  113625 | 2299 | `								if( pThis->iRef > 1 ){` |
|       - | 2300 | `									/* Load attribute index */` |
|  113239 | 2301 | `									pTos->nIdx = pObjAttr->nIdx;` |
|  113234 | 2302 | `									if( VmMemberFetchIsRefSource(pInstr)` |
|   56693 | 2303 | `									 && (pObjAttr->iState & (VM_CLASS_ATTR_REFBOUND\|VM_CLASS_ATTR_REFSRCPIN)) == 0 ){` |
|       - | 2304 | `										/* This fetch is about to become a reference, so php makes` |
|       - | 2305 | `										 * THIS property a reference too. The table cannot name a` |
|       - | 2306 | `										 * property as a holder, so without a pin of its own the` |
|       - | 2307 | `										 * slot's only recorded holder is the other end's -- and` |
|       - | 2308 | ``										 * `$q->p =& $o->p` followed by $q's death freed the value`` |
|       - | 2309 | `										 * and left $o->p reading NULL. One counted pin, given back` |
|       - | 2310 | `										 * when the property is released; the bit makes it` |
|       - | 2311 | `										 * idempotent, so a source fetched in a loop pins once. */` |
|     132 | 2312 | `										pObjAttr->iState \|= VM_CLASS_ATTR_REFSRCPIN;` |
|     132 | 2313 | `										VmPinMemObjSlotCounted(&(*pVm),pObjAttr->nIdx);` |
|      65 | 2314 | `									}` |
|   56617 | 2315 | `								}` |
|   56810 | 2316 | `							}` |
|  113620 | 2317 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET)` |
|   58248 | 2318 | `							 && !VmMemberNativeSetKeepsSlot(pInstr) ){` |
|    2253 | 2319 | `								pTos->nIdx = SXU32_HIGH;` |
|    2253 | 2320 | `								pTos->iFlags \|= MEMOBJ_AUX_NATIVEPROP;` |
|    1124 | 2321 | `							}` |
|   56810 | 2322 | `						}` |
|  113625 | 2323 | `						if( pInstr->iP2 == PH7_MEMBER_ISSET ){` |
|       - | 2324 | `							/* isset() tests null-ness and nothing else, so reduce the loaded` |
|       - | 2325 | `							 * value to the same non-null marker the __isset and get-hook` |
|       - | 2326 | `							 * paths push. A property read off a TEMPORARY receiver` |
|       - | 2327 | ``							 * (`isset(f()->p)`, `isset((new C)->p)`, and now every`` |
|       - | 2328 | `							 * intermediate link of an accessor chain) leaves no variable` |
|       - | 2329 | `							 * index behind, and the trailing builtin read that as a` |
|       - | 2330 | `							 * CONSTANT and warned -- a diagnostic php has no equivalent of,` |
|       - | 2331 | `							 * its isset() being a language construct rather than a call. */` |
|      87 | 2332 | `							int bSet = (pTos->iFlags & MEMOBJ_NULL) == 0;` |
|      87 | 2333 | `							PH7_MemObjRelease(pTos);` |
|      87 | 2334 | `							if( bSet ){` |
|      62 | 2335 | `								pTos->x.iVal = 1;` |
|      62 | 2336 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      30 | 2337 | `							}` |
|      87 | 2338 | `							pTos->nIdx = SXU32_HIGH;` |
|      42 | 2339 | `						}` |
|   56815 | 2340 | `					}else{` |
|       - | 2341 | `						/* Inaccessible (private/protected) from this scope. php consults` |
|       - | 2342 | `						 * __get here exactly like a missing property (band A #3a) before` |
|       - | 2343 | `						 * erroring; the guard keeps a self-recursive read from looping. */` |
|      49 | 2344 | `						ph7_class_method *pGetMagic = 0;` |
|       - | 2345 | ``						/* A WRITE-context fetch (the base of `$o->p[k] = v`) reads through`` |
|       - | 2346 | `						 * __get in php too — the write then lands on the value it answered` |
|       - | 2347 | `						 * and is lost, with php's indirect-modification notice. PHL refused` |
|       - | 2348 | ``						 * the whole statement with `Cannot access private property` instead,`` |
|       - | 2349 | ``						 * a fatal on a program php runs. A plain store and a `??=` keep`` |
|       - | 2350 | `						 * their own paths below. */` |
|      46 | 2351 | `						if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      64 | 2352 | `						 && (VmMemberFetchForWrite(pInstr)` |
|      40 | 2353 | `						  \|\| (pInstr->iP2 != PH7_MEMBER_WRITE && !VmMemberNextIsWrite(pInstr + 1))) ){` |
|      43 | 2354 | `							pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|      20 | 2355 | `						}` |
|       - | 2356 | `						/* ++/--/compound-assign on a DECLARED but inaccessible property:` |
|       - | 2357 | `						 * php's accessors answer for it exactly as they do for a missing` |
|       - | 2358 | `						 * one (__get, modify, __set), where PHL raised` |
|       - | 2359 | ``						 * `Cannot access private property C::$n`. A plain store keeps its`` |
|       - | 2360 | `						 * own __set park below — it is a write, but not a read-modify-write. */` |
|      46 | 2361 | `						if( VmMemberNextIsRmw(pInstr + 1)` |
|      27 | 2362 | `						 && VmMagicRmwEligible(pVm,pClass,pThis,&sName) ){` |
|       - | 2363 | `							/* The name was already popped and pTos released above, so` |
|       - | 2364 | `							 * sName is the instruction's own literal here. */` |
|       - | 2365 | `							ph7_value sRmwVal;` |
|       - | 2366 | `							sxu32 nRmwScratch;` |
|       3 | 2367 | `							PH7_MemObjInit(pVm,&sRmwVal);` |
|       4 | 2368 | `							VmMagicRmwArm(&(*pVm),pThis,pClass,&sName,&sRmwVal,&nRmwScratch,` |
|       2 | 2369 | `								(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|       3 | 2370 | `							PH7_MemObjStore(&sRmwVal,pTos);` |
|       3 | 2371 | `							pTos->nIdx = nRmwScratch;` |
|       3 | 2372 | `							PH7_MemObjRelease(&sRmwVal);` |
|       3 | 2373 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 2374 | `							VM_EXIT_BREAK;` |
|       - | 2375 | `						}` |
|      47 | 2376 | `						if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 2377 | `							ph7_value sMagicRet;` |
|       9 | 2378 | `							PH7_MemObjInit(pVm,&sMagicRet);` |
|       9 | 2379 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       9 | 2380 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|       9 | 2381 | `							VmMagicGuardPop(pVm);` |
|       9 | 2382 | `							if( VmMemberFetchForWrite(pInstr) ){` |
|       5 | 2383 | `								PH7_VmOverloadedPropNotice(&(*pVm),pClass,&sName,&sMagicRet);` |
|       2 | 2384 | `							}` |
|       - | 2385 | `							/* The name was already popped and pTos released above; just` |
|       - | 2386 | `							 * take the magic result as the expression value. */` |
|       9 | 2387 | `							PH7_MemObjStore(&sMagicRet,pTos);` |
|       9 | 2388 | `							pTos->nIdx = SXU32_HIGH;` |
|       9 | 2389 | `							PH7_MemObjRelease(&sMagicRet);` |
|       9 | 2390 | `							PH7_ClassInstanceUnref(pThis);` |
|       9 | 2391 | `							VM_EXIT_BREAK;` |
|       - | 2392 | `						}` |
|      39 | 2393 | `						if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2394 | `							/* isset/empty on an inaccessible property: php consults __isset` |
|       - | 2395 | `							 * (band A #3b), and is silently false without it — never an` |
|       - | 2396 | `							 * Error (pre-fix PHL fataled here). */` |
|       3 | 2397 | `							ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|       3 | 2398 | `							int bSet = 0;` |
|       3 | 2399 | `							if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 2400 | `								ph7_value sIssetRet;` |
|     ! 0 | 2401 | `								PH7_MemObjInit(pVm,&sIssetRet);` |
|     ! 0 | 2402 | `								VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|     ! 0 | 2403 | `								PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|     ! 0 | 2404 | `								VmMagicGuardPop(pVm);` |
|     ! 0 | 2405 | `								PH7_MemObjToBool(&sIssetRet);` |
|     ! 0 | 2406 | `								bSet = sIssetRet.x.iVal != 0;` |
|     ! 0 | 2407 | `								PH7_MemObjRelease(&sIssetRet);` |
|     ! 0 | 2408 | `							}` |
|       3 | 2409 | `							if( pInstr->iP2 == PH7_MEMBER_COALESCE && pIssetMagic == 0 ){` |
|       - | 2410 | ``								/* `??` with no __isset to gate it: php reads straight through`` |
|       - | 2411 | `								 * __get, exactly as it does for a MISSING property. The` |
|       - | 2412 | `								 * __get dispatch just above this block is gated on a` |
|       - | 2413 | `								 * non-lookup context, so answer here. */` |
|     ! 0 | 2414 | `								ph7_class_method *pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|     ! 0 | 2415 | `								if( pCoalGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 2416 | `									ph7_value sCoalRet;` |
|     ! 0 | 2417 | `									PH7_MemObjInit(pVm,&sCoalRet);` |
|     ! 0 | 2418 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     ! 0 | 2419 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sCoalRet);` |
|     ! 0 | 2420 | `									VmMagicGuardPop(pVm);` |
|     ! 0 | 2421 | `									PH7_MemObjStore(&sCoalRet,pTos);` |
|     ! 0 | 2422 | `									pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 2423 | `									PH7_MemObjRelease(&sCoalRet);` |
|     ! 0 | 2424 | `								}` |
|     ! 0 | 2425 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2426 | `								VM_EXIT_BREAK;` |
|       - | 2427 | `							}` |
|       3 | 2428 | `							if( bSet ){` |
|     ! 0 | 2429 | `								if( VmMemberCtxWantsValue(pInstr->iP2) ){` |
|     ! 0 | 2430 | `									ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       - | 2431 | `									ph7_value sEmptyVal;` |
|     ! 0 | 2432 | `									PH7_MemObjInit(pVm,&sEmptyVal);` |
|     ! 0 | 2433 | `									if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|     ! 0 | 2434 | `										VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     ! 0 | 2435 | `										PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|     ! 0 | 2436 | `										VmMagicGuardPop(pVm);` |
|     ! 0 | 2437 | `									}` |
|     ! 0 | 2438 | `									PH7_MemObjStore(&sEmptyVal,pTos);` |
|     ! 0 | 2439 | `									pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 2440 | `									PH7_MemObjRelease(&sEmptyVal);` |
|     ! 0 | 2441 | `								}else{` |
|     ! 0 | 2442 | `									pTos->x.iVal = 1;` |
|     ! 0 | 2443 | `									MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     ! 0 | 2444 | `									pTos->nIdx = SXU32_HIGH;` |
|       - | 2445 | `								}` |
|     ! 0 | 2446 | `							}` |
|       3 | 2447 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 2448 | `							VM_EXIT_BREAK;` |
|       - | 2449 | `						}` |
|       - | 2450 | `						{` |
|       - | 2451 | `							/* A plain store to an inaccessible property dispatches __set` |
|       - | 2452 | `							 * (band A #3b): park the receiver+name for the following` |
|       - | 2453 | `							 * OP_STORE, exactly like the missing-property case. */` |
|      37 | 2454 | `							VmInstr *pNextW = pInstr + 1;` |
|      37 | 2455 | `							if( pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0 ){` |
|       3 | 2456 | `								ph7_class_method *pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|       3 | 2457 | `								if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){` |
|       3 | 2458 | `									pThis->iRef++;` |
|       3 | 2459 | `									pVm->pMagicSetThis = pThis;` |
|       3 | 2460 | `									SyBlobReset(&pVm->sMagicSetName);` |
|       3 | 2461 | `									SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       3 | 2462 | `									pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|       3 | 2463 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 2464 | `									VM_EXIT_BREAK;` |
|       - | 2465 | `								}` |
|     ! 0 | 2466 | `							}` |
|       - | 2467 | `						}` |
|      32 | 2468 | `						if( ((pInstr + 1)->iOp == PH7_OP_NULLC \|\| (pInstr + 1)->iOp == PH7_OP_NULLC_JMP)` |
|      19 | 2469 | `						 && pInstr->iP2 != PH7_MEMBER_WRITE ){` |
|       - | 2470 | `` 							/* `$o->priv ?? default` (OP_NULLC; NULLC_JMP is the `??=` `` |
|       - | 2471 | ``							 * pre-test): php treats `??` as an isset-style lookup — an`` |
|       - | 2472 | `							 * inaccessible property yields null SILENTLY (the` |
|       - | 2473 | `							 * __isset/__get consults already ran above), letting the` |
|       - | 2474 | `							 * coalesce pick the default. */` |
|     ! 0 | 2475 | `							PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2476 | `							VM_EXIT_BREAK; /* pTos is already the null temp */` |
|       - | 2477 | `						}` |
|       - | 2478 | `						/* A subclass reading a PARENT's PRIVATE property used to be` |
|       - | 2479 | `						 * special-cased into an "Undefined property" warning here. It is` |
|       - | 2480 | `						 * php's answer, but not because of the SCOPE: the base's private` |
|       - | 2481 | `						 * lives under its mangled storage name, so the subclass's plain` |
|       - | 2482 | `						 * lookup never reaches this point at all -- and reading the SAME` |
|       - | 2483 | `						 * property on an instance of the declaring class, which does, is` |
|       - | 2484 | `						 * php's ordinary visibility refusal. Deciding it from the scope` |
|       - | 2485 | `						 * turned that one into a warning too. */` |
|       - | 2486 | `						/* Genuinely inaccessible: php's CATCHABLE Error (parked on the` |
|       - | 2487 | `						 * boundary rail; the fetch-point router lands it — it was an` |
|       - | 2488 | `						 * uncatchable VmReportUncaughtException+Abort before). */` |
|       - | 2489 | `						{` |
|       - | 2490 | `						SyBlob sErrMsg;` |
|      35 | 2491 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      35 | 2492 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      35 | 2493 | `						SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",` |
|      16 | 2494 | `							zVis,&pClass->sName,&sName);` |
|      35 | 2495 | `						PH7_ClassInstanceUnref(pThis);` |
|      35 | 2496 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      35 | 2497 | `						SyBlobRelease(&sErrMsg);` |
|      35 | 2498 | `						VM_EXIT_BREAK;` |
|       - | 2499 | `						}` |
|       - | 2500 | `					}` |
|   56810 | 2501 | `				}` |
|       - | 2502 | `				/* Safely unreference the object */` |
|  114679 | 2503 | `				PH7_ClassInstanceUnref(pThis);` |
|       - | 2504 | `			}` |
|  135163 | 2505 | `		}else{` |
|     164 | 2506 | `			if( (pNos->iFlags & MEMOBJ_AUX_STROFFSET)` |
|      90 | 2507 | `			 && (pInstr->iP2 == PH7_MEMBER_WRITE \|\| pInstr->iP2 == PH7_MEMBER_UNSET` |
|       5 | 2508 | `			  \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET \|\| pInstr->iP2 == PH7_MEMBER_REF_TARGET` |
|       - | 2509 | ``			  /* A reference SOURCE (`$r =& $s[0]->p`) is compiled as a READ here on`` |
|       - | 2510 | `			   * purpose — php hands back a copy for a handler-backed property — so the` |
|       - | 2511 | `			   * bind that follows is what makes it a reach-inside, exactly as it does` |
|       - | 2512 | `			   * for a subscript. */` |
|       4 | 2513 | `			  \|\| (pInstr->iP2 == PH7_MEMBER_READ` |
|       3 | 2514 | `			      && ((pInstr + 1)->iOp == PH7_OP_STORE_REF` |
|       1 | 2515 | `			       \|\| (pInstr + 1)->iOp == PH7_OP_LOAD_REF` |
|     ! 0 | 2516 | `			       \|\| (pInstr + 1)->iOp == PH7_OP_STORE_IDX_REF))) ){` |
|       - | 2517 | `				/* The base is a string OFFSET and this reaches INSIDE it. php refuses every` |
|       - | 2518 | `				 * such reach and words the refusal from what is doing the reaching, so a` |
|       - | 2519 | ``				 * PROPERTY is `Cannot use string offset as an object` where a subscript is`` |
|       - | 2520 | ``				 * `... as an array` — `$s[0]->p = 1`, `$s[0]->p += 1`, `$s[0]->p ??= 1` and`` |
|       - | 2521 | ``				 * `unset($s[0]->p)` alike. PHL reported the generic non-object write`` |
|       - | 2522 | ``				 * (`Attempt to assign property "p" on string`) and said nothing at all for`` |
|       - | 2523 | ``				 * the unset. A METHOD CALL is not one of these: php keeps `Call to a member`` |
|       - | 2524 | ``				 * function p() on string` there, and so does the path below. */`` |
|       - | 2525 | `				sxi32 rcSo;` |
|      11 | 2526 | `				VmPopOperand(&pTos,1);` |
|      11 | 2527 | `				PH7_MemObjRelease(pTos);` |
|      11 | 2528 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      11 | 2529 | `				pTos->nIdx = SXU32_HIGH;` |
|      11 | 2530 | `				rcSo = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an object",` |
|       - | 2531 | `					sizeof("Cannot use string offset as an object")-1);` |
|      11 | 2532 | `				if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 | 2533 | `				rc = rcSo;` |
|      11 | 2534 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2535 | `			}` |
|       - | 2536 | ``			/* `->` on a non-object (e.g. a null intermediate). Silent in isset()/empty()/unset()`` |
|       - | 2537 | ``			 * context (iP2 2/3/4) so `isset($o->missing->x)` / `unset($o->missing->x)` match PHP. */`` |
|     156 | 2538 | `			if( pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2539 | `				/* php draws a sharp line here, and PH7 drew none: CALLING a method on a` |
|       - | 2540 | `				 * non-object is a catchable Error, while READING a property off one is a` |
|       - | 2541 | `				 * Warning that yields NULL. PH7 raised the same notice for both and` |
|       - | 2542 | ``				 * carried on with NULL, so `$null->m()` silently did nothing. */`` |
|       - | 2543 | `				SyString sMemb;` |
|     114 | 2544 | `				const char *zVerb = 0;` |
|     114 | 2545 | `				SyStringInitFromBuf(&sMemb,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     114 | 2546 | `				if( pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|       - | 2547 | ``					/* A deferred call ARGUMENT (`f($u->p)`): which diagnostic php raises`` |
|       - | 2548 | `					 * depends on the parameter, and this op does not know it — a by-VALUE` |
|       - | 2549 | `					 * binding is the read warning below, a by-REFERENCE one is php's` |
|       - | 2550 | ``					 * `Attempt to modify property` Error. Record the step rooted at the`` |
|       - | 2551 | `					 * base and let OP_CALL re-drive it in the mode the callee decides,` |
|       - | 2552 | `					 * exactly as a property MISSING on a real object is recorded. */` |
|      27 | 2553 | `					VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|      27 | 2554 | `					if( pPath && VmDeferPathPushProp(pPath,&sMemb) == SXRET_OK ){` |
|      27 | 2555 | `						VmPopOperand(&pTos,1);   /* drop the property name */` |
|      27 | 2556 | `						PH7_MemObjRelease(pTos); /* collapse the base slot into the carrier */` |
|      27 | 2557 | `						pTos->x.pOther = pPath;` |
|      27 | 2558 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      27 | 2559 | `						pTos->nIdx = SXU32_HIGH;` |
|      53 | 2560 | `						VM_EXIT_BREAK;` |
|       - | 2561 | `					}` |
|     ! 0 | 2562 | `					if( pPath ){` |
|     ! 0 | 2563 | `						VmFreeDeferredPath(pPath);` |
|     ! 0 | 2564 | `					}` |
|       - | 2565 | `					/* fall through to the immediate warning on allocation failure */` |
|     ! 0 | 2566 | `				}` |
|       - | 2567 | `				/* WRITING one is an Error too, and php picks its verb from what the` |
|       - | 2568 | `				 * write actually is. PH7 warned about a READ it never performed and` |
|       - | 2569 | `				 * then let the store fail into its own` |
|       - | 2570 | `				 * "Cannot perform assignment on a constant class attribute", so the` |
|       - | 2571 | `				 * script carried on past a statement php stops it for. The kind is` |
|       - | 2572 | `				 * read off the following instruction, the way every other write` |
|       - | 2573 | `				 * classification in this handler is (VmMemberFetchForWrite). */` |
|      88 | 2574 | `				if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|      33 | 2575 | `					const VmInstr *pNextW = pInstr + 1;` |
|      32 | 2576 | `					if( (pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0)` |
|      22 | 2577 | ``					 \|\| pNextW->iOp == PH7_OP_NULLC_JMP /* `$u->p ??= v` */`` |
|      12 | 2578 | `					 \|\| VmNextIsCompoundAssign(pNextW) ){` |
|      25 | 2579 | `						zVerb = "assign";` |
|      21 | 2580 | `					}else if( pNextW->iOp == PH7_OP_INCR \|\| pNextW->iOp == PH7_OP_DECR ){` |
|       5 | 2581 | `						zVerb = "increment/decrement";` |
|       3 | 2582 | `					}else{` |
|       - | 2583 | ``						/* The base of a subscript write (`$u->p[] = v`): php asks the`` |
|       - | 2584 | `						 * property for something to modify, and there is no property. */` |
|       5 | 2585 | `						zVerb = "modify";` |
|       1 | 2586 | `					}` |
|      72 | 2587 | `				}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       3 | 2588 | `					zVerb = "assign";` |
|      55 | 2589 | `				}else if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       3 | 2590 | `					zVerb = "modify";` |
|      53 | 2591 | `				}else if( VmMemberFetchIsRefSource(pInstr) ){` |
|       - | 2592 | ``					/* `$r =& $o->missing->p`: the intermediate is null and php asks it`` |
|       - | 2593 | ``					 * for something to bind to, which is its `modify` sentence -- not`` |
|       - | 2594 | `					 * the read warning this used to fall through to. */` |
|       3 | 2595 | `					zVerb = "modify";` |
|       1 | 2596 | `				}` |
|      88 | 2597 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD \|\| zVerb ){` |
|       - | 2598 | `					SyBlob sErrM;` |
|       - | 2599 | `					sxi32 rcErr;` |
|      53 | 2600 | `					SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      53 | 2601 | `					if( zVerb ){` |
|      39 | 2602 | `						SyBlobFormat(&sErrM,"Attempt to %s property \"%z\" on %s",` |
|      19 | 2603 | `							zVerb,&sMemb,VmArithValueName(pNos));` |
|      20 | 2604 | `					}else{` |
|      15 | 2605 | `						SyBlobFormat(&sErrM,"Call to a member function %z() on %s",` |
|       7 | 2606 | `							&sMemb,VmArithValueName(pNos));` |
|       - | 2607 | `					}` |
|      53 | 2608 | `					VmPopOperand(&pTos,1);` |
|      53 | 2609 | `					PH7_MemObjRelease(pTos);` |
|      53 | 2610 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      53 | 2611 | `					pTos->nIdx = SXU32_HIGH;` |
|      79 | 2612 | `					rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      26 | 2613 | `						SyBlobLength(&sErrM));` |
|      53 | 2614 | `					SyBlobRelease(&sErrM);` |
|      53 | 2615 | `					if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      53 | 2616 | `					rc = rcErr;` |
|      53 | 2617 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2618 | `				}` |
|      53 | 2619 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Attempt to read property \"%z\" on %s",` |
|      17 | 2620 | `					&sMemb,VmArithValueName(pNos));` |
|      17 | 2621 | `			}` |
|      78 | 2622 | `			VmPopOperand(&pTos,1);` |
|      78 | 2623 | `			PH7_MemObjRelease(pTos);` |
|      78 | 2624 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|       - | 2625 | `		}` |
|  135201 | 2626 | `	}else{` |
|       - | 2627 | `		/* Static member access using class name */` |
|  107684 | 2628 | `		pNos = pTos;` |
|  107684 | 2629 | `		pThis = 0;` |
|  107684 | 2630 | `		if( !pInstr->p3 ){` |
|    5910 | 2631 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    5910 | 2632 | `			pNos--;` |
|       - | 2633 | `#ifdef UNTRUST` |
|       - | 2634 | `			if( pNos < pStack ){` |
|       - | 2635 | `				VM_EXIT_ABORT;` |
|       - | 2636 | `			}` |
|       - | 2637 | `#endif` |
|    2950 | 2638 | `		}else{` |
|       - | 2639 | `			/* Attribute name already computed */` |
|  101779 | 2640 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|       - | 2641 | `		}` |
|  107684 | 2642 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|  107644 | 2643 | `			ph7_class *pClass = 0;` |
|       - | 2644 | `			/* A FORWARDING static call — self::/parent::/static:: — preserves the` |
|       - | 2645 | `			 * caller's late-static-binding class (php); a non-forwarding C::m() resets` |
|       - | 2646 | `			 * it to C. The receiver slot below keeps the literal keyword, which OP_CALL` |
|       - | 2647 | `			 * can't resolve, so it would fall back to the callee's DECLARING class and` |
|       - | 2648 | `			 * lose LSB. Remember the live LSB class here and stamp it onto the receiver` |
|       - | 2649 | `			 * after the method name is pushed. */` |
|  107644 | 2650 | `			int bForwardingCall = 0;` |
|  107644 | 2651 | `			ph7_class *pForwardLsb = 0;` |
|       - | 2652 | `			/* Same rail snapshot as OP_NEW: the lookup below can run an autoloader that` |
|       - | 2653 | `			 * throws, and php then reports that exception and nothing else. */` |
|  107644 | 2654 | `			sxi32 nMbBrc = pVm->nBoundaryRc;` |
|  107644 | 2655 | `			const void *pMbRes = (const void *)pVm->pResumeFrame;` |
|  107644 | 2656 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - | 2657 | `				/* Class already instantiated */` |
|      24 | 2658 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      24 | 2659 | `				pClass = pThis->pClass;` |
|      24 | 2660 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      13 | 2661 | `			}else{` |
|       - | 2662 | `				/* Try to extract the target class */` |
|  107622 | 2663 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|  107622 | 2664 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|  107622 | 2665 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|       - | 2666 | `					/* Handle self/static/parent keywords */` |
|  107622 | 2667 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       - | 2668 | `						/* In a trait method, self:: resolves to the USING class */` |
|    1497 | 2669 | `						pClass = PH7_VmPeekSelfClass(&(*pVm));` |
|    1497 | 2670 | `						bForwardingCall = 1;` |
|    1497 | 2671 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|  106876 | 2672 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|     134 | 2673 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|     134 | 2674 | `						bForwardingCall = 1;` |
|     134 | 2675 | `						pForwardLsb = pClass;` |
|  106065 | 2676 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|     186 | 2677 | `						pClass = PH7_VmResolveParentClass(&(*pVm));` |
|     186 | 2678 | `						bForwardingCall = 1;` |
|     186 | 2679 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|      95 | 2680 | `					}else{` |
|  105818 | 2681 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|       - | 2682 | `					}` |
|   53801 | 2683 | `				}` |
|       - | 2684 | `			}` |
|  107644 | 2685 | `			if( pClass == 0 ){` |
|       - | 2686 | `				/* Undefined class: php throws a catchable Error */` |
|       - | 2687 | `				SyBlob sErrM;` |
|       - | 2688 | `				sxi32 rcErr;` |
|      30 | 2689 | `				if( PH7_VmClassLookupRaised(&(*pVm),nMbBrc,pMbRes) ){` |
|       - | 2690 | `					/* ...unless an autoloader raised: land THAT instead of reporting the` |
|       - | 2691 | `					 * class missing on top of it. */` |
|      10 | 2692 | `					sxi32 rcAutoM = pVm->nBoundaryRc;` |
|      10 | 2693 | `					pVm->nBoundaryRc = 0;` |
|      10 | 2694 | `					if( !pInstr->p3 ){` |
|       8 | 2695 | `						VmPopOperand(&pTos,1);` |
|       3 | 2696 | `					}` |
|      10 | 2697 | `					PH7_MemObjRelease(pTos);` |
|      10 | 2698 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      10 | 2699 | `					pTos->nIdx = SXU32_HIGH;` |
|      10 | 2700 | `					if( rcAutoM == PH7_ABORT ){` |
|     ! 0 | 2701 | `						VM_EXIT_ABORT;` |
|       - | 2702 | `					}` |
|      10 | 2703 | `					rc = PH7_EXCEPTION;` |
|      10 | 2704 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2705 | `				}` |
|      21 | 2706 | `				SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      21 | 2707 | `				SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|      18 | 2708 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob));` |
|      21 | 2709 | `				if( !pInstr->p3 ){` |
|      21 | 2710 | `					VmPopOperand(&pTos,1);` |
|       9 | 2711 | `				}` |
|      21 | 2712 | `				PH7_MemObjRelease(pTos);` |
|      21 | 2713 | `				pTos->nIdx = SXU32_HIGH;` |
|      30 | 2714 | `				rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       9 | 2715 | `					SyBlobLength(&sErrM));` |
|      21 | 2716 | `				SyBlobRelease(&sErrM);` |
|      21 | 2717 | `				if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      21 | 2718 | `				rc = rcErr;` |
|      29 | 2719 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 2720 | `			}else{` |
|       - | 2721 | `				/* The static twin of the name rule at the top of this handler, and it` |
|       - | 2722 | `				 * runs HERE rather than up there because php resolves the CLASS first:` |
|       - | 2723 | ``				 * `NoSuchClass::${$arr}` is the class refusal alone, with no coercion`` |
|       - | 2724 | ``				 * and no `Array to string conversion` behind it, while the same name on`` |
|       - | 2725 | ``				 * a class that exists warns and then reports `Access to undeclared`` |
|       - | 2726 | ``				 * static property C::$Array`. A `::` METHOD name is refused the same way`` |
|       - | 2727 | `				 * an instance one is -- once the class is known. */` |
|  107618 | 2728 | `				if( !pInstr->p3 ){` |
|    5860 | 2729 | `					if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|    3052 | 2730 | `						if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 2731 | `							sxi32 rcMn;` |
|       3 | 2732 | `							VmPopOperand(&pTos,1);` |
|       3 | 2733 | `							PH7_MemObjRelease(pTos);` |
|       3 | 2734 | `							MemObjSetType(pTos,MEMOBJ_NULL);` |
|       3 | 2735 | `							pTos->nIdx = SXU32_HIGH;` |
|       3 | 2736 | `							if( pThis ){` |
|     ! 0 | 2737 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2738 | `								pThis = 0;` |
|     ! 0 | 2739 | `							}` |
|       3 | 2740 | `							rcMn = VmThrowFromVm(&(*pVm),"Error","Method name must be a string",` |
|       - | 2741 | `								sizeof("Method name must be a string")-1);` |
|       3 | 2742 | `							if( rcMn == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 | 2743 | `							rc = rcMn;` |
|       3 | 2744 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2745 | `						}` |
|    1520 | 2746 | `					}else{` |
|    2813 | 2747 | `						sxi32 rcNm = PH7_MemObjToStringUV(pTos);` |
|    2813 | 2748 | `						if( rcNm != SXRET_OK ){` |
|     ! 0 | 2749 | `							VmPopOperand(&pTos,1);` |
|     ! 0 | 2750 | `							PH7_MemObjRelease(pTos);` |
|     ! 0 | 2751 | `							MemObjSetType(pTos,MEMOBJ_NULL);` |
|     ! 0 | 2752 | `							pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 2753 | `							if( pThis ){` |
|     ! 0 | 2754 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2755 | `								pThis = 0;` |
|     ! 0 | 2756 | `							}` |
|     ! 0 | 2757 | `							if( rcNm == PH7_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 2758 | `							rc = rcNm;` |
|     ! 0 | 2759 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2760 | `						}` |
|       - | 2761 | `					}` |
|       - | 2762 | `					/* The coercion rewrote the slot the name was read from. */` |
|    5858 | 2763 | `					SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),` |
|       - | 2764 | `						SyBlobLength(&pTos->sBlob));` |
|    2919 | 2765 | `				}` |
|  107616 | 2766 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - | 2767 | `					/* Method call */` |
|    3050 | 2768 | `					ph7_class_method *pMeth = 0;` |
|    3050 | 2769 | `					if( sName.nByte > 0 ){` |
|       - | 2770 | `						/* An INTERFACE's methods are looked up too: they are all abstract,` |
|       - | 2771 | `						 * so the arm below reports php's "Cannot call abstract method` |
|       - | 2772 | `						 * I::m()" rather than claiming the name does not exist. */` |
|    3050 | 2773 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|    1515 | 2774 | `					}` |
|    3050 | 2775 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      71 | 2776 | `						if( pMeth ){` |
|       - | 2777 | `							SyBlob sErrM;` |
|       - | 2778 | `							sxi32 rcErr;` |
|       5 | 2779 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       5 | 2780 | `							SyBlobFormat(&sErrM,"Cannot call abstract method %z::%z()",` |
|       2 | 2781 | `								&pClass->sName,&sName);` |
|       5 | 2782 | `							if( !pInstr->p3 ){` |
|       5 | 2783 | `								VmPopOperand(&pTos,1);` |
|       2 | 2784 | `							}` |
|       5 | 2785 | `							PH7_MemObjRelease(pTos);` |
|       5 | 2786 | `							pTos->nIdx = SXU32_HIGH;` |
|       - | 2787 | ``							/* The `$obj::m()` form took a reference on the receiver at the`` |
|       - | 2788 | `							 * top of this branch; every exit has to give it back, and only` |
|       - | 2789 | `							 * the fall-through at the end of the arm used to. */` |
|       5 | 2790 | `							if( pThis ){` |
|     ! 0 | 2791 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2792 | `								pThis = 0;` |
|     ! 0 | 2793 | `							}` |
|       7 | 2794 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       2 | 2795 | `								SyBlobLength(&sErrM));` |
|       5 | 2796 | `							SyBlobRelease(&sErrM);` |
|       5 | 2797 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 2798 | `							rc = rcErr;` |
|       5 | 2799 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 2800 | `						}else{` |
|      67 | 2801 | `							ph7_class_instance *pMagicThis = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|      67 | 2802 | `							ph7_class_method *pCallStaticMagic = pMagicThis` |
|      16 | 2803 | `								? PH7_ClassExtractMethod(pMagicThis->pClass,"__call",sizeof("__call")-1)` |
|      56 | 2804 | `								: PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1);` |
|      67 | 2805 | `							if( pCallStaticMagic ){` |
|       - | 2806 | `								/* php: C::missing(...) dispatches __callStatic($name,$args)` |
|       - | 2807 | `								 * through the packing body (see the instance twin) — or` |
|       - | 2808 | `								 * __call($name,$args) on the calling frame's own $this when` |
|       - | 2809 | `								 * that receiver fits C (VmStaticCallMagicThis). */` |
|      84 | 2810 | `								VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pMagicThis,` |
|      27 | 2811 | `									pMagicThis ? pMagicThis->pClass : pClass,&sName);` |
|      57 | 2812 | `								if( pPend == 0 ){` |
|     ! 0 | 2813 | `									VM_EXIT_ABORT;` |
|       - | 2814 | `								}` |
|      57 | 2815 | `								if( !pInstr->p3 ){` |
|      57 | 2816 | `									VmPopOperand(&pTos,1);` |
|      27 | 2817 | `								}` |
|      57 | 2818 | `								PH7_MemObjRelease(pTos);` |
|      57 | 2819 | `								if( pThis ){` |
|     ! 0 | 2820 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2821 | `									pThis = 0;` |
|     ! 0 | 2822 | `								}` |
|      57 | 2823 | `								pTos->x.pOther = pPend;` |
|      57 | 2824 | `								pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|      57 | 2825 | `								pTos->nIdx = SXU32_HIGH;` |
|      57 | 2826 | `								VM_EXIT_BREAK;` |
|       - | 2827 | `							}` |
|       - | 2828 | `							{` |
|       - | 2829 | `								/* php: the STATIC form reports the same "Call to undefined` |
|       - | 2830 | `								 * method C::m()" as the instance one. */` |
|       - | 2831 | `								SyBlob sErrM;` |
|       - | 2832 | `								sxi32 rcErr;` |
|      11 | 2833 | `								SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      11 | 2834 | `								SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",` |
|       5 | 2835 | `									&pClass->sName,&sName);` |
|      11 | 2836 | `								if( !pInstr->p3 ){` |
|      11 | 2837 | `									VmPopOperand(&pTos,1);` |
|       5 | 2838 | `								}` |
|      11 | 2839 | `								PH7_MemObjRelease(pTos);` |
|      11 | 2840 | `								pTos->nIdx = SXU32_HIGH;` |
|      11 | 2841 | `								if( pThis ){` |
|     ! 0 | 2842 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2843 | `									pThis = 0;` |
|     ! 0 | 2844 | `								}` |
|      16 | 2845 | `								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       5 | 2846 | `									SyBlobLength(&sErrM));` |
|      11 | 2847 | `								SyBlobRelease(&sErrM);` |
|      11 | 2848 | `								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 | 2849 | `								rc = rcErr;` |
|      11 | 2850 | `								PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2851 | `							}` |
|       - | 2852 | `						}` |
|       - | 2853 | `						/* Pop the method name from the stack */` |
|     ! 0 | 2854 | `						if( !pInstr->p3 ){` |
|     ! 0 | 2855 | `							VmPopOperand(&pTos,1);` |
|       - | 2856 | `						}` |
|     ! 0 | 2857 | `						PH7_MemObjRelease(pTos);` |
|     ! 0 | 2858 | `					}else{` |
|       - | 2859 | `						/* Inaccessible from this scope: php routes through the same fallback a` |
|       - | 2860 | ``						 * MISSING name takes — __call on a compatible `$this`, else`` |
|       - | 2861 | ``						 * __callStatic — which is why `C::privateStatic()` runs a catch-all`` |
|       - | 2862 | `						 * instead of the "Call to private method" Error OP_CALL would raise` |
|       - | 2863 | `						 * below. */` |
|    2982 | 2864 | `						ph7_class_method *pDeniedStatic = 0;` |
|    2982 | 2865 | `						ph7_class_instance *pDeniedThis = 0;` |
|    2982 | 2866 | `						int bDeniedStatic = 0;` |
|    2982 | 2867 | `						if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - | 2868 | `							/* The OWNING class decides, as in the instance twin above: a` |
|       - | 2869 | `							 * trait method's rules belong to the class that composed it. */` |
|      33 | 2870 | `							ph7_class *pOwnerCls = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|      33 | 2871 | `							if( !PH7_VmClassMemberAccess(&(*pVm),pOwnerCls,&sName,pMeth->iProtection,FALSE) ){` |
|      15 | 2872 | `								pDeniedThis = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|      15 | 2873 | `								pDeniedStatic = pDeniedThis` |
|       4 | 2874 | `									? PH7_ClassExtractMethod(pDeniedThis->pClass,"__call",` |
|       - | 2875 | `										sizeof("__call")-1)` |
|      12 | 2876 | `									: PH7_ClassExtractMethod(pClass,"__callStatic",` |
|       - | 2877 | `										sizeof("__callStatic")-1);` |
|      15 | 2878 | `								bDeniedStatic = pDeniedStatic == 0;` |
|       7 | 2879 | `							}` |
|      16 | 2880 | `						}` |
|    2982 | 2881 | `						if( bDeniedStatic ){` |
|       - | 2882 | `							/* No catch-all answers for it: php's visibility Error, raised at` |
|       - | 2883 | `							 * the resolution like the instance twin above. OP_CALL's own` |
|       - | 2884 | `							 * screen re-derives the method from the FUNCTION's name against` |
|       - | 2885 | `							 * its declaring class, which cannot see a trait adaptation's` |
|       - | 2886 | ``							 * composed protection — `sHi as private sPriv` ran from global`` |
|       - | 2887 | `							 * scope in silence. */` |
|       - | 2888 | `							SyBlob sErrM;` |
|       - | 2889 | `							sxi32 rcErr;` |
|       7 | 2890 | `							ph7_class *pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|       7 | 2891 | `							ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|      10 | 2892 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       3 | 2893 | `								? "private" : "protected";` |
|       7 | 2894 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       7 | 2895 | `							if( pScope ){` |
|       3 | 2896 | `								SyBlobFormat(&sErrM,"Call to %s method %z::%z() from scope %z",` |
|       1 | 2897 | `									zVis,&pOwner->sName,&sName,&pScope->sName);` |
|       2 | 2898 | `							}else{` |
|       5 | 2899 | `								SyBlobFormat(&sErrM,"Call to %s method %z::%z() from global scope",` |
|       2 | 2900 | `									zVis,&pOwner->sName,&sName);` |
|       - | 2901 | `							}` |
|       7 | 2902 | `							if( !pInstr->p3 ){` |
|       7 | 2903 | `								VmPopOperand(&pTos,1);` |
|       3 | 2904 | `							}` |
|       7 | 2905 | `							PH7_MemObjRelease(pTos);` |
|       7 | 2906 | `							pTos->nIdx = SXU32_HIGH;` |
|       7 | 2907 | `							if( pThis ){` |
|     ! 0 | 2908 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2909 | `								pThis = 0;` |
|     ! 0 | 2910 | `							}` |
|      10 | 2911 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       3 | 2912 | `								SyBlobLength(&sErrM));` |
|       7 | 2913 | `							SyBlobRelease(&sErrM);` |
|       7 | 2914 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       7 | 2915 | `							rc = rcErr;` |
|       7 | 2916 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2917 | `						}` |
|    2976 | 2918 | `						if( pDeniedStatic ){` |
|       - | 2919 | `							/* The packing body takes no receiver slot: drop the method-name` |
|       - | 2920 | `							 * slot so the MARK lands on the receiver slot, exactly as the` |
|       - | 2921 | `							 * missing-method twin above does (leaving the class name in place` |
|       - | 2922 | `							 * would pack it as the first $args entry). */` |
|      13 | 2923 | `							VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pDeniedThis,` |
|       4 | 2924 | `								pDeniedThis ? pDeniedThis->pClass : pClass,&sName);` |
|       9 | 2925 | `							if( pPend == 0 ){` |
|     ! 0 | 2926 | `								VM_EXIT_ABORT;` |
|       - | 2927 | `							}` |
|       9 | 2928 | `							if( !pInstr->p3 ){` |
|       9 | 2929 | `								VmPopOperand(&pTos,1);` |
|       4 | 2930 | `							}` |
|       9 | 2931 | `							PH7_MemObjRelease(pTos);` |
|       9 | 2932 | `							if( pThis ){` |
|     ! 0 | 2933 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2934 | `								pThis = 0;` |
|     ! 0 | 2935 | `							}` |
|       9 | 2936 | `							pTos->x.pOther = pPend;` |
|       9 | 2937 | `							pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|       9 | 2938 | `							pTos->nIdx = SXU32_HIGH;` |
|       9 | 2939 | `							VM_EXIT_BREAK;` |
|       - | 2940 | `						}` |
|       - | 2941 | ``						/* php refuses a NON-STATIC method named through `::` unless the`` |
|       - | 2942 | ``						 * CALLING frame holds a `$this` the class accepts: that is what`` |
|       - | 2943 | ``						 * makes `self::`/`parent::`/`static::` and `C::m()` from inside a`` |
|       - | 2944 | `						 * C method work, and every other spelling — from global scope,` |
|       - | 2945 | `` 						 * from a static method, from an unrelated class, and `$obj::m()` `` |
|       - | 2946 | ``						 * — an Error. PHL ran the body instead, with `$this` unset or`` |
|       - | 2947 | ``						 * (for the object form) bound to the object the `::` was written`` |
|       - | 2948 | `						 * on, which php never uses: it takes the CALLER's. The message` |
|       - | 2949 | ``						 * names the DECLARING class, so `D::m()` reports B::m(). */`` |
|    2968 | 2950 | `						if( (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|     156 | 2951 | `							ph7_class_instance *pCallerThis = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|     156 | 2952 | `							if( pCallerThis == 0 ){` |
|       - | 2953 | `								SyBlob sErrM;` |
|       - | 2954 | `								sxi32 rcErr;` |
|      13 | 2955 | `								ph7_class *pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|      13 | 2956 | `								SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      13 | 2957 | `								SyBlobFormat(&sErrM,"Non-static method %z::%z() cannot be called statically",` |
|       6 | 2958 | `									&pOwner->sName,&pMeth->sFunc.sName);` |
|      13 | 2959 | `								if( !pInstr->p3 ){` |
|      13 | 2960 | `									VmPopOperand(&pTos,1);` |
|       6 | 2961 | `								}` |
|      13 | 2962 | `								PH7_MemObjRelease(pTos);` |
|      13 | 2963 | `								pTos->nIdx = SXU32_HIGH;` |
|      13 | 2964 | `								if( pThis ){` |
|       3 | 2965 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 2966 | `									pThis = 0;` |
|       1 | 2967 | `								}` |
|      19 | 2968 | `								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       6 | 2969 | `									SyBlobLength(&sErrM));` |
|      13 | 2970 | `								SyBlobRelease(&sErrM);` |
|      13 | 2971 | `								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      13 | 2972 | `								rc = rcErr;` |
|      13 | 2973 | `								PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2974 | `							}` |
|       - | 2975 | `` 							/* The receiver is that `$this` — never the object the `::` `` |
|       - | 2976 | `							 * was written on, and never nothing. Naming it here is also` |
|       - | 2977 | `							 * what makes the call work inside a CLOSURE declared in a` |
|       - | 2978 | ``							 * method: php gives such a closure a `$this`, PHL carries it`` |
|       - | 2979 | `							 * as a frame variable rather than on the frame itself, and` |
|       - | 2980 | ``							 * OP_CALL reads only the frame, so `self::m()` in a closure`` |
|       - | 2981 | `							 * used to run with no receiver at all. */` |
|     144 | 2982 | `							PH7_MemObjRelease(pNos);` |
|     144 | 2983 | `							pNos->x.pOther = pCallerThis;` |
|     144 | 2984 | `							MemObjSetType(pNos,MEMOBJ_OBJ);` |
|     144 | 2985 | `							pCallerThis->iRef++;` |
|     144 | 2986 | `							pNos->nIdx = SXU32_HIGH;` |
|      70 | 2987 | `						}` |
|       - | 2988 | `						/* Push method name on the stack, MARKED as already screened (see the` |
|       - | 2989 | `						 * instance twin: OP_CALL cannot re-derive a composed protection). */` |
|    2956 | 2990 | `						PH7_MemObjRelease(pTos);` |
|    2956 | 2991 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|    2956 | 2992 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|    2956 | 2993 | `						pTos->iFlags \|= MEMOBJ_AUX_MEMBERCALL;` |
|       - | 2994 | `					}` |
|    2956 | 2995 | `					pTos->nIdx = SXU32_HIGH;` |
|       - | 2996 | `					/* Forwarding call (self::/parent::/static::): overwrite the receiver` |
|       - | 2997 | `					 * slot (pNos, one below the method name) with the live LSB class name` |
|       - | 2998 | ``					 * so OP_CALL pushes THAT onto aSelf — preserving `static::` inside the`` |
|       - | 2999 | `					 * callee. Without this the literal keyword falls through to the` |
|       - | 3000 | `					 * callee's declaring class. Only when an LSB class is actually in` |
|       - | 3001 | `					 * scope (a static call from global scope has none). */` |
|    2956 | 3002 | `					if( bForwardingCall && pForwardLsb && (pNos->iFlags & MEMOBJ_STRING) ){` |
|      86 | 3003 | `						SyBlobReset(&pNos->sBlob);` |
|      86 | 3004 | `						SyBlobAppend(&pNos->sBlob,pForwardLsb->sName.zString,pForwardLsb->sName.nByte);` |
|      42 | 3005 | `					}` |
|    1473 | 3006 | `				}else{` |
|       - | 3007 | `					/* Attribute access */` |
|  104571 | 3008 | `					ph7_class_attr *pAttr = 0;` |
|  104571 | 3009 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - | 3010 | `						/* unset(C::$x): PHP rejects unsetting a static property with a fatal Error.` |
|       - | 3011 | `						 * Without this the iP2=unset tag falls through to a normal static read and the` |
|       - | 3012 | `						 * trailing generic unset() would silently NULL (and de-type) the shared slot. */` |
|       - | 3013 | `						char zMsg[256];` |
|     ! 0 | 3014 | `						SyBufferFormat(zMsg,sizeof(zMsg),"Attempt to unset static property %.*s::$%.*s",` |
|     ! 0 | 3015 | `							(int)pClass->sName.nByte,pClass->sName.zString,(int)sName.nByte,sName.zString);` |
|     ! 0 | 3016 | `						VmReportUncaughtException(&(*pVm),"Error",5,zMsg,(sxu32)SyStrlen(zMsg),0,0);` |
|     ! 0 | 3017 | `						VM_EXIT_ABORT;` |
|       - | 3018 | `					}` |
|       - | 3019 | `					/* Check for special ::class pseudo-constant */` |
|  105092 | 3020 | `					if( sName.nByte == sizeof("class")-1 &&` |
|    1042 | 3021 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|       - | 3022 | `						/* ::class returns the fully qualified class name */` |
|       - | 3023 | `						/* Pop the attribute name from the stack */` |
|     186 | 3024 | `						if( !pInstr->p3 ){` |
|     186 | 3025 | `							VmPopOperand(&pTos,1);` |
|      91 | 3026 | `						}` |
|     186 | 3027 | `						PH7_MemObjRelease(pTos);` |
|       - | 3028 | `						/* Load the class name */` |
|     186 | 3029 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|     186 | 3030 | `						pTos->nIdx = SXU32_HIGH;` |
|      95 | 3031 | `					}else{` |
|       - | 3032 | `						/* Extract the target attribute. php keeps constants and` |
|       - | 3033 | `						 * (static) properties in separate namespaces; the source` |
|       - | 3034 | ``						 * form disambiguates (set by the `::` codegen, compile.c):`` |
|       - | 3035 | ``						 * a `$`-form is a STATIC PROPERTY (hAttr) — either a literal`` |
|       - | 3036 | ``						 * name folded into pInstr->p3 (`C::$s`) or a dynamic name on`` |
|       - | 3037 | ``						 * the stack marked iP1==2 (`C::$$x`, `C::${$e}`); a bareword`` |
|       - | 3038 | `						 * (p3==0, iP1==1) is a CONSTANT or enum case (hConst). This` |
|       - | 3039 | ``						 * is what lets `const C` and `public $C` coexist and resolve`` |
|       - | 3040 | `						 * to the right member. */` |
|       - | 3041 | `						/* php gives a class CONSTANT no silent-lookup mode at all: there is` |
|       - | 3042 | ``						 * no BP_VAR_IS fetch for one, so `empty(C::K)` and `C::K ?? $d` raise`` |
|       - | 3043 | `						 * the same Error a plain read does -- undefined OR inaccessible --` |
|       - | 3044 | `						 * where the same two shapes over a static PROPERTY answer quietly.` |
|       - | 3045 | `						 * PHL silenced both, so a typo'd or private class constant read` |
|       - | 3046 | ``						 * through `??` handed back the default. (isset() over one is a php`` |
|       - | 3047 | `						 * COMPILE error, so it never reaches this.) */` |
|  104389 | 3048 | `						int bConstForm = (pInstr->p3 == 0 && pInstr->iP1 != 2);` |
|  104389 | 3049 | `						if( sName.nByte > 0 ){` |
|  104389 | 3050 | `							pAttr = bConstForm` |
|    2616 | 3051 | `								? PH7_ClassExtractConstant(pClass,sName.zString,sName.nByte)` |
|  103076 | 3052 | `								: VmClassAttrWithShadow(pClass,sName.zString,sName.nByte);` |
|   52192 | 3053 | `						}` |
|  104384 | 3054 | `						if( pAttr && bConstForm && (pClass->iFlags & PH7_CLASS_TRAIT) != 0` |
|    1307 | 3055 | `						 && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 3056 | `							/* A trait constant belongs to the classes that COMPOSE the trait` |
|       - | 3057 | ``							 * and to no one else: php refuses `T::K` outright, from inside a`` |
|       - | 3058 | `							 * trait method as readily as from outside. (PHP 8.2 introduced` |
|       - | 3059 | `							 * trait constants; this refusal came with them.) */` |
|       - | 3060 | `							SyBlob sErrTr;` |
|       3 | 3061 | `							SyBlobInit(&sErrTr,&pVm->sAllocator);` |
|       3 | 3062 | `							SyBlobFormat(&sErrTr,"Cannot access trait constant %z::%z directly",` |
|       1 | 3063 | `								&pClass->sName,&pAttr->sName);` |
|       3 | 3064 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",` |
|       - | 3065 | `								sizeof("Error")-1,&sErrTr));` |
|       3 | 3066 | `							pAttr = 0;` |
|       3 | 3067 | `							bStaticHidden = 1;` |
|       1 | 3068 | `						}` |
|  104384 | 3069 | `						if( pAttr && !bConstForm` |
|  103065 | 3070 | `						 && (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT))` |
|   50880 | 3071 | `						     != PH7_CLASS_ATTR_STATIC ){` |
|       - | 3072 | ``							/* php's `::$name` reads the class's PROPERTY table -- which holds`` |
|       - | 3073 | `							 * the INSTANCE properties and the constants too -- and answers in` |
|       - | 3074 | `							 * a fixed order: visibility first, then static-ness. So a private` |
|       - | 3075 | ``							 * instance property is `Cannot access private property H::$pi`,`` |
|       - | 3076 | ``							 * a public one and a CONSTANT named with the `$` form are`` |
|       - | 3077 | ``							 * `Access to undeclared static property H::$inst`, and neither`` |
|       - | 3078 | `							 * ever yields a value: the static table simply has no such row.` |
|       - | 3079 | `							 * PHL matched only the constant case; an instance property` |
|       - | 3080 | ``							 * reached through `::` printed PH7's own uncatchable "Access to a`` |
|       - | 3081 | `							 * non-static class attribute ... PH7 is loading NULL" and CARRIED` |
|       - | 3082 | ``							 * ON -- reading null, and letting `H::$inst = 'w'` report success`` |
|       - | 3083 | `							 * for a write php refuses. Fold it into the not-found arm below,` |
|       - | 3084 | `							 * raising the visibility refusal first when that is what php` |
|       - | 3085 | `							 * answers. */` |
|       8 | 3086 | `							if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|       9 | 3087 | `							 && !PH7_VmClassAttrAccess(&(*pVm),pClass,pAttr,FALSE) ){` |
|       - | 3088 | `								SyBlob sErrVis;` |
|       3 | 3089 | `								SyBlobInit(&sErrVis,&pVm->sAllocator);` |
|       2 | 3090 | `								SyBlobFormat(&sErrVis,"Cannot access %s property %z::$%z",` |
|       2 | 3091 | `									pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected",` |
|       1 | 3092 | `									&pClass->sName,&pAttr->sName);` |
|       3 | 3093 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",` |
|       - | 3094 | `									sizeof("Error")-1,&sErrVis));` |
|       3 | 3095 | `								bStaticHidden = 1;` |
|       1 | 3096 | `							}` |
|       9 | 3097 | `							pAttr = 0;` |
|       4 | 3098 | `						}` |
|  104389 | 3099 | `						if( pAttr == 0 && !bStaticHidden ){` |
|       - | 3100 | `							/* No such member. php raises a catchable Error whose wording` |
|       - | 3101 | `							 * depends on the ACCESS form (the same p3/iP1 signal used above):` |
|       - | 3102 | ``							 * a bareword `C::MISSING` (constant form) is "Undefined constant`` |
|       - | 3103 | ``							 * C::MISSING", a `$`-form `C::$missing` is "Access to undeclared`` |
|       - | 3104 | `							 * static property C::$missing". Instance magic (__get) is never` |
|       - | 3105 | `							 * consulted for statics (band A #3b). isset()/empty() context` |
|       - | 3106 | `							 * stays silently false. Parked on the boundary rail; the op` |
|       - | 3107 | `							 * completes benignly with NULL and the fetch-point router lands` |
|       - | 3108 | `							 * the throw. */` |
|      30 | 3109 | `							if( bConstForm \|\| !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 3110 | `								SyBlob sErrMsg;` |
|      30 | 3111 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      30 | 3112 | `								if( bConstForm ){` |
|      16 | 3113 | `									SyBlobFormat(&sErrMsg,"Undefined constant %z::%z",` |
|       7 | 3114 | `										&pClass->sName,&sName);` |
|       9 | 3115 | `								}else{` |
|      16 | 3116 | `									SyBlobFormat(&sErrMsg,"Access to undeclared static property %z::$%z",` |
|       7 | 3117 | `										&pClass->sName,&sName);` |
|       - | 3118 | `								}` |
|      30 | 3119 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      14 | 3120 | `							}` |
|      14 | 3121 | `						}` |
|       - | 3122 | `						/* Pop the attribute name from the stack */` |
|  104389 | 3123 | `						if( !pInstr->p3 ){` |
|    2631 | 3124 | `							VmPopOperand(&pTos,1);` |
|    1313 | 3125 | `						}` |
|  104389 | 3126 | `						PH7_MemObjRelease(pTos);` |
|  104389 | 3127 | `						pTos->nIdx = SXU32_HIGH;` |
|  104389 | 3128 | `						if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - | 3129 | ``							/* `self::$s =& $x` / `C::$s =& $x`: stash the static property slot`` |
|       - | 3130 | `							 * for the following member-marked OP_STORE_REF (class-level, shared` |
|       - | 3131 | `							 * across instances — matches php). Skip the read machinery below. */` |
|      16 | 3132 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|      16 | 3133 | `							 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|      19 | 3134 | `							 && VmClassStaticDeferPending(pClass) ){` |
|       - | 3135 | `								/* Binding a reference TO a static touches the table, so it` |
|       - | 3136 | `								 * materializes first, like every other static access. */` |
|       3 | 3137 | `								sxi32 rcRt = PH7_VmMaterializeClassStatics(&(*pVm),pClass);` |
|       3 | 3138 | `								if( rcRt != SXRET_OK ){` |
|       3 | 3139 | `									pVm->pRefTargetStaticAttr = 0;` |
|       3 | 3140 | `									pVm->pRefTargetAttr = 0;` |
|       3 | 3141 | `									pVm->pRefTargetThis = 0;` |
|       3 | 3142 | `									if( pThis ){` |
|     ! 0 | 3143 | `										PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3144 | `									}` |
|       3 | 3145 | `									if( rcRt == PH7_ABORT ){` |
|     ! 0 | 3146 | `										VM_EXIT_ABORT;` |
|       - | 3147 | `									}` |
|       - | 3148 | `									{` |
|       - | 3149 | `										sxi32 iRpR;` |
|       3 | 3150 | `										if( VmRecordedResume(pVm,&iRpR,pState->pEntryFrame,aInstr) ){` |
|       7 | 3151 | `											PH7_RESUME_DRAIN()` |
|       3 | 3152 | `											pc = iRpR;` |
|       3 | 3153 | `											VM_EXIT_BREAK;` |
|       - | 3154 | `										}` |
|       - | 3155 | `									}` |
|     ! 0 | 3156 | `									VM_EXIT_EXCEPTION;` |
|       - | 3157 | `								}` |
|     ! 0 | 3158 | `							}` |
|      14 | 3159 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|      16 | 3160 | `							 && PH7_VmClassAttrAccess(&(*pVm),pClass,pAttr,FALSE) ){` |
|      16 | 3161 | `								pVm->pRefTargetStaticAttr = pAttr;` |
|      16 | 3162 | `								pVm->pRefTargetAttr = 0;` |
|      16 | 3163 | `								pVm->pRefTargetThis = 0;` |
|      16 | 3164 | `								pTos->nIdx = pAttr->nIdx;` |
|       9 | 3165 | `							}else{` |
|     ! 0 | 3166 | `								pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 3167 | `								pVm->pRefTargetAttr = 0;` |
|     ! 0 | 3168 | `								pVm->pRefTargetThis = 0;` |
|       - | 3169 | `							}` |
|      16 | 3170 | `							if( pThis ){` |
|     ! 0 | 3171 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3172 | `							}` |
|      16 | 3173 | `							VM_EXIT_BREAK;` |
|       - | 3174 | `						}` |
|  104373 | 3175 | `						if( pAttr ){` |
|       - | 3176 | `							{` |
|       - | 3177 | `								ph7_value *pValue;` |
|       - | 3178 | `								/* php materializes the class's static table at the FIRST` |
|       - | 3179 | `								 * static-property access (any property, any context — read,` |
|       - | 3180 | `								 * write, even isset): a default whose evaluation was DEFERRED` |
|       - | 3181 | `								 * (it threw at the declaration) runs here, and a typed one that` |
|       - | 3182 | `								 * failed its check throws its catchable TypeError here. Class` |
|       - | 3183 | `								 * constants and static method calls do not trigger the` |
|       - | 3184 | `								 * materialization (php-exact). */` |
|  104336 | 3185 | `								if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|  103036 | 3186 | `								 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|  101741 | 3187 | `								 && VmClassStaticDeferPending(pClass) ){` |
|      48 | 3188 | `									sxi32 rcD = PH7_VmMaterializeClassStatics(&(*pVm),pClass);` |
|      48 | 3189 | `									if( rcD != SXRET_OK ){` |
|      44 | 3190 | `										if( pThis ){` |
|     ! 0 | 3191 | `											PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3192 | `										}` |
|      44 | 3193 | `										if( rcD == PH7_ABORT ){` |
|     ! 0 | 3194 | `											VM_EXIT_ABORT;` |
|       - | 3195 | `										}` |
|       - | 3196 | `										{` |
|       - | 3197 | `											sxi32 iRpD;` |
|      44 | 3198 | `											if( VmRecordedResume(pVm,&iRpD,pState->pEntryFrame,aInstr) ){` |
|      84 | 3199 | `												PH7_RESUME_DRAIN()` |
|      40 | 3200 | `												pc = iRpD;` |
|      40 | 3201 | `												VM_EXIT_BREAK;` |
|       - | 3202 | `											}` |
|       - | 3203 | `										}` |
|       5 | 3204 | `										VM_EXIT_EXCEPTION;` |
|       - | 3205 | `									}` |
|       2 | 3206 | `								}` |
|       - | 3207 | `								/* Check if the access to the attribute is allowed */` |
|  104299 | 3208 | `								if( PH7_VmClassAttrAccess(&(*pVm),pClass,pAttr,FALSE) ){` |
|       - | 3209 | `									/* PHP 7.4+: uninitialized typed static read.` |
|       - | 3210 | `									 * Same LHS-of-store peek as the instance path. */` |
|  104270 | 3211 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|  102241 | 3212 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|  150224 | 3213 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|  100146 | 3214 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|  100151 | 3215 | `										if( pS ){` |
|  100151 | 3216 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|  100151 | 3217 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|  100034 | 3218 | `												VmInstr *pNext = pInstr + 1;` |
|  100034 | 3219 | `												int bIsLhs = 0;` |
|  100034 | 3220 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|      11 | 3221 | `													bIsLhs = 1;` |
|       4 | 3222 | `												}` |
|  100034 | 3223 | `												if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 3224 | `													/* Destructuring target ([S::$s] = [...]):` |
|       - | 3225 | `													 * initialized by the OP_LOAD_LIST store. */` |
|       3 | 3226 | `													bIsLhs = 1;` |
|       1 | 3227 | `												}` |
|       - | 3228 | ``												/* isset()/empty()/`??` ask whether the property`` |
|       - | 3229 | `												 * HAS a value and read an uninitialized one as` |
|       - | 3230 | `												 * "not set" -- a silent miss, not the Error a` |
|       - | 3231 | `												 * plain read raises. The instance path has had` |
|       - | 3232 | `												 * this since typed properties landed; the STATIC` |
|       - | 3233 | ``												 * one never did, so `isset(C::$n)` threw where php`` |
|       - | 3234 | ``												 * answers false. (`??=` is the NULLC_JMP branch`` |
|       - | 3235 | `												 * below -- it is a write base, not a lookup.) */` |
|  100034 | 3236 | `												if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|      13 | 3237 | `													bIsLhs = 1;` |
|       6 | 3238 | `												}` |
|       - | 3239 | `												/* And a WRITE base is not a read: same` |
|       - | 3240 | `												 * auto-initialize rule as the instance path. A` |
|       - | 3241 | ``												 * read-MODIFY-write (`C::$n++`, `.=`) is not one`` |
|       - | 3242 | `												 * of these -- it reads the property first, so` |
|       - | 3243 | `												 * php's Error stands. */` |
|  100034 | 3244 | `												if( (pInstr + 1)->iOp == PH7_OP_NULLC_JMP ){` |
|       3 | 3245 | `													bIsLhs = 1;` |
|  100033 | 3246 | `												}else if( VmMemberFetchIsRefSource(pInstr) ){` |
|       - | 3247 | `													/* A reference fetch has the instance path's own` |
|       - | 3248 | `													 * rule: NULL and bind when the type admits it,` |
|       - | 3249 | `													 * php's by-reference refusal when it does not. */` |
|       4 | 3250 | `													sxi32 rcRs = VmRefUninitTypedProperty(&(*pVm),pClass,pV,` |
|       1 | 3251 | `														(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx));` |
|       3 | 3252 | `													if( rcRs != SXRET_OK ){` |
|       3 | 3253 | `														VmBoundaryPark(&(*pVm),rcRs);` |
|       3 | 3254 | `														if( pThis ){` |
|     ! 0 | 3255 | `															PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3256 | `														}` |
|       3 | 3257 | `														VM_EXIT_BREAK;` |
|       - | 3258 | `													}` |
|     ! 0 | 3259 | `													bIsLhs = 1;` |
|  100030 | 3260 | `												}else if( VmMemberFetchForWrite(pInstr) ){` |
|       4 | 3261 | `													sxi32 rcAI = VmAutoInitArrayProperty(&(*pVm),pV,` |
|       1 | 3262 | `														(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx));` |
|       3 | 3263 | `													if( rcAI != SXRET_OK ){` |
|     ! 0 | 3264 | `														VmBoundaryPark(&(*pVm),rcAI);` |
|     ! 0 | 3265 | `														if( pThis ){` |
|     ! 0 | 3266 | `															PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3267 | `														}` |
|     ! 0 | 3268 | `														VM_EXIT_BREAK;` |
|       - | 3269 | `													}` |
|       3 | 3270 | `													bIsLhs = 1;` |
|       1 | 3271 | `												}` |
|  100032 | 3272 | `												if( !bIsLhs ){` |
|  100004 | 3273 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|  100004 | 3274 | `													if( pThis ){` |
|     ! 0 | 3275 | `														PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3276 | `													}` |
|  100004 | 3277 | `													if( rcU == PH7_ABORT ){` |
|     ! 0 | 3278 | `														VM_EXIT_ABORT;` |
|       - | 3279 | `													}` |
|       - | 3280 | `													{` |
|       - | 3281 | `														sxi32 iRp;` |
|  100004 | 3282 | `														if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|  200004 | 3283 | `															PH7_RESUME_DRAIN()` |
|  100004 | 3284 | `															pc = iRp;` |
|  100004 | 3285 | `															VM_EXIT_BREAK;` |
|       - | 3286 | `														}` |
|       - | 3287 | `													}` |
|     ! 0 | 3288 | `													VM_EXIT_EXCEPTION;` |
|       - | 3289 | `												}` |
|      13 | 3290 | `											}` |
|      71 | 3291 | `										}` |
|      71 | 3292 | `									}` |
|    4266 | 3293 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|    3432 | 3294 | `									 && SySetUsed(&pAttr->aAttrs) > 0 ){` |
|       - | 3295 | `										/* php 8.4 #[\Deprecated] on a class constant:` |
|       - | 3296 | `										 * every access re-warns. */` |
|      14 | 3297 | `										VmDeprecatedConstNotice(&(*pVm),pClass,pAttr);` |
|       6 | 3298 | `									}` |
|    4271 | 3299 | `									if( pAttr->nIdx == SXU32_HIGH ){` |
|       - | 3300 | `										/* Unmaterialized slot. Enum case: first access` |
|       - | 3301 | `										 * materializes ALL the singletons. Plain constant: its` |
|       - | 3302 | `										 * initializer hasn't run yet (mount-order-dependent` |
|       - | 3303 | `										 * cross-constant reference) — evaluate on demand. A` |
|       - | 3304 | `										 * raised TypeError/Error (backing mismatch, duplicate` |
|       - | 3305 | `										 * value, self-reference) parks on the boundary rail;` |
|       - | 3306 | `										 * the op completes benignly with NULL and the` |
|       - | 3307 | `										 * fetch-point router lands the throw. */` |
|       - | 3308 | `										sxi32 rcEnum;` |
|     803 | 3309 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|       - | 3310 | `											/* php: a DIRECT static access evaluates every case` |
|       - | 3311 | `											 * of the enum (whole-class constant update) — a` |
|       - | 3312 | `											 * broken sibling case throws here too. A reference` |
|       - | 3313 | `											 * from inside another constant's initializer` |
|       - | 3314 | `											 * (nConstEvalDepth > 0) evaluates only the` |
|       - | 3315 | `											 * requested case. */` |
|      75 | 3316 | `											if( pVm->nConstEvalDepth > 0 ){` |
|       3 | 3317 | `												rcEnum = VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|       2 | 3318 | `											}else{` |
|      73 | 3319 | `												rcEnum = VmEnumMaterialize(&(*pVm),pClass);` |
|       - | 3320 | `											}` |
|      40 | 3321 | `										}else{` |
|     733 | 3322 | `											rcEnum = VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|       - | 3323 | `										}` |
|     803 | 3324 | `										if( rcEnum != SXRET_OK ){` |
|      40 | 3325 | `											VmBoundaryPark(&(*pVm),rcEnum);` |
|      19 | 3326 | `										}` |
|     399 | 3327 | `									}` |
|       - | 3328 | `									/* Load the desired attribute */` |
|    4271 | 3329 | `									pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx);` |
|    4271 | 3330 | `									if( pValue ){` |
|    4231 | 3331 | `										PH7_MemObjLoad(pValue,pTos);` |
|    4231 | 3332 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       - | 3333 | `											/* Load index number */` |
|    1683 | 3334 | `											pTos->nIdx = pAttr->nIdx;` |
|    1678 | 3335 | `											if( VmMemberFetchIsRefSource(pInstr)` |
|     845 | 3336 | `											 && (pAttr->iFlags & (PH7_CLASS_ATTR_REFBOUND\|PH7_CLASS_ATTR_REFSRCPIN)) == 0 ){` |
|       - | 3337 | `												/* The instance path's rule, for a class static:` |
|       - | 3338 | `												 * one counted pin so the other end's unpin cannot` |
|       - | 3339 | ``												 * free the static's value. `$o->p =& C::$s;` then`` |
|       - | 3340 | `												 * dropping $o read C::$s as NULL. */` |
|       3 | 3341 | `												pAttr->iFlags \|= PH7_CLASS_ATTR_REFSRCPIN;` |
|       3 | 3342 | `												VmPinMemObjSlotCounted(&(*pVm),pAttr->nIdx);` |
|       1 | 3343 | `											}` |
|     839 | 3344 | `										}` |
|    2118 | 3345 | `									}` |
|    2160 | 3346 | `								}else if( bConstForm \|\| !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 3347 | `									/* Denied by visibility. php's Error is CATCHABLE here` |
|       - | 3348 | `									 * exactly as it is for an instance property, and PHL` |
|       - | 3349 | `									 * reported it uncaught and ABORTED the script -- so a` |
|       - | 3350 | ``									 * `try { C::$protectedStatic; } catch` never ran its`` |
|       - | 3351 | `									 * catch, and everything after the try was dropped with` |
|       - | 3352 | `									 * exit status 0. Parked on the boundary rail like the` |
|       - | 3353 | `									 * instance twin; the op completes with the NULL already` |
|       - | 3354 | `									 * in the slot and the fetch-point router lands the throw.` |
|       - | 3355 | ``									 * A lookup (isset/empty/`??`) stays silent and false, as`` |
|       - | 3356 | `									 * php's is. The name is built with the ATTRIBUTE's own` |
|       - | 3357 | `									 * spelling, which is the declaration's. */` |
|       - | 3358 | `									SyBlob sErrVis;` |
|      30 | 3359 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       9 | 3360 | `										? "private" : "protected";` |
|      21 | 3361 | `									SyBlobInit(&sErrVis,&pVm->sAllocator);` |
|      21 | 3362 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|      15 | 3363 | `										SyBlobFormat(&sErrVis,"Cannot access %s constant %z::%z",` |
|       6 | 3364 | `											zVis,&pClass->sName,&pAttr->sName);` |
|       9 | 3365 | `									}else{` |
|       7 | 3366 | `										SyBlobFormat(&sErrVis,"Cannot access %s property %z::$%z",` |
|       3 | 3367 | `											zVis,&pClass->sName,&pAttr->sName);` |
|       - | 3368 | `									}` |
|      21 | 3369 | `									VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",` |
|       - | 3370 | `										sizeof("Error")-1,&sErrVis));` |
|       9 | 3371 | `								}` |
|       - | 3372 | `							}` |
|    2145 | 3373 | `						}` |
|       - | 3374 | `					}` |
|       - | 3375 | `				}` |
|    7460 | 3376 | `				if( pThis ){` |
|       - | 3377 | `					/* Safely unreference the object */` |
|      22 | 3378 | `					PH7_ClassInstanceUnref(pThis);` |
|      10 | 3379 | `				}` |
|       - | 3380 | `			}` |
|    3725 | 3381 | `		}else{` |
|       - | 3382 | ``			/* `$v::X` where $v holds neither an object nor a class NAME. php's`` |
|       - | 3383 | `			 * catchable Error, which STOPS the statement; PH7 raised its own` |
|       - | 3384 | `` 			 * uncatchable wording and carried on with NULL, so a `$obj::$s = 1` `` |
|       - | 3385 | `			 * through a null base went on to fail again in OP_STORE. */` |
|       - | 3386 | `			sxi32 rcCn;` |
|      41 | 3387 | `			if( !pInstr->p3 ){` |
|      27 | 3388 | `				VmPopOperand(&pTos,1);` |
|      13 | 3389 | `			}` |
|      41 | 3390 | `			PH7_MemObjRelease(pTos);` |
|      41 | 3391 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      41 | 3392 | `			pTos->nIdx = SXU32_HIGH;` |
|      41 | 3393 | `			rcCn = VmThrowFromVm(&(*pVm),"Error","Class name must be a valid object or a string",` |
|       - | 3394 | `				sizeof("Class name must be a valid object or a string")-1);` |
|      41 | 3395 | `			if( rcCn == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      41 | 3396 | `			rc = rcCn;` |
|      41 | 3397 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 3398 | `		}` |
|       - | 3399 | `	}` |
|  277942 | 3400 | `	VM_EXIT_BREAK;` |
|     ! 0 | 3401 | `	VM_EXIT_BREAK;` |
|  192735 | 3402 | `}` |
|       - | 3403 |  |
|       - | 3404 | `/*` |
|       - | 3405 | ` * OP_CLONE: body moved verbatim from the OP_CLONE arm of` |
|       - | 3406 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 3407 | ` */` |
|     442 | 3408 | `PH7_PRIVATE VmOpRc VmExecOpClone(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 3409 | `{` |
|     447 | 3410 | `	ph7_value *pTos = pState->pTos;` |
|     447 | 3411 | `	ph7_value *pStack = pState->pStack;` |
|     447 | 3412 | `	VmInstr *aInstr = pState->aInstr;` |
|     447 | 3413 | `	sxi32 pc = pState->pc;` |
|       - | 3414 | `	sxi32 rc;` |
|     221 | 3415 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 3416 | `	ph7_class_instance *pSrc,*pClone;` |
|       - | 3417 | `#ifdef UNTRUST` |
|       - | 3418 | `	if( pTos < pStack ){` |
|       - | 3419 | `		VM_EXIT_ABORT;` |
|       - | 3420 | `	}` |
|       - | 3421 | `#endif` |
|       - | 3422 | `	/* Make sure we are dealing with a class instance. PHP 8 throws a catchable` |
|       - | 3423 | ``	 * TypeError for a non-object operand — for both `clone $x` and clone($x). */`` |
|     447 | 3424 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - | 3425 | `		SyBlob sMsg;` |
|       - | 3426 | `		char zGiven[64];` |
|      24 | 3427 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 3428 | `		/* php names the VALUE for a bool ("true given", not "bool given"), which is` |
|       - | 3429 | `		 * what every other argument diagnostic here already says — the shared helper. */` |
|      24 | 3430 | `		SyBlobFormat(&sMsg,"clone(): Argument #1 ($object) must be of type object, %s given",` |
|      11 | 3431 | `			VmValueGivenName(pTos,zGiven,sizeof(zGiven)));` |
|      24 | 3432 | `		rc = VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|      24 | 3433 | `		PH7_MemObjRelease(pTos);` |
|      24 | 3434 | `		pTos->nIdx = SXU32_HIGH;` |
|      24 | 3435 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 3436 | `			VM_EXIT_ABORT;` |
|       - | 3437 | `		}` |
|       - | 3438 | `		{` |
|       - | 3439 | `			sxi32 iRp;` |
|      24 | 3440 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      24 | 3441 | `				pc = iRp;` |
|      24 | 3442 | `				VM_EXIT_BREAK;` |
|       - | 3443 | `			}` |
|       - | 3444 | `		}` |
|     ! 0 | 3445 | `		VM_EXIT_EXCEPTION;` |
|       - | 3446 | `	}` |
|       - | 3447 | `	/* Point to the source */` |
|     425 | 3448 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|       - | 3449 | `	/* Enum cases are not cloneable — php's catchable Error (the singleton` |
|       - | 3450 | `	 * identity would break) — and neither is a class whose instances own a` |
|       - | 3451 | `	 * C-side resource a slot-by-slot copy would double-free (PH7_CLASS_NOCLONE),` |
|       - | 3452 | `	 * nor a Generator or a Fiber (their suspended C state is not copyable). php` |
|       - | 3453 | `	 * words all of them the same way and throws the same catchable Error; the` |
|       - | 3454 | `	 * Generator/Fiber pair used to take an older warn-only path here, so` |
|       - | 3455 | ``	 * `clone $gen` PRINTED an uncatchable diagnostic and then carried on with the`` |
|       - | 3456 | ``	 * ORIGINAL object standing in for the copy — the one shape of `clone` that`` |
|       - | 3457 | `	 * answered a value php never lets the program reach. */` |
|     420 | 3458 | `	if( (pSrc->pClass->iFlags & PH7_CLASS_ENUM) \|\| PH7_ClassIsUncloneable(pSrc->pClass)` |
|     348 | 3459 | `		\|\| pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|       - | 3460 | `		SyBlob sMsg;` |
|     153 | 3461 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     153 | 3462 | `		SyBlobFormat(&sMsg,"Trying to clone an uncloneable object of class %z",` |
|     150 | 3463 | `			&pSrc->pClass->sName);` |
|     153 | 3464 | `		rc = VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|     153 | 3465 | `		PH7_MemObjRelease(pTos);` |
|     153 | 3466 | `		pTos->nIdx = SXU32_HIGH;` |
|     153 | 3467 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 3468 | `			VM_EXIT_ABORT;` |
|       - | 3469 | `		}` |
|       - | 3470 | `		{` |
|       - | 3471 | `			sxi32 iRp;` |
|     153 | 3472 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|     105 | 3473 | `				pc = iRp;` |
|     105 | 3474 | `				VM_EXIT_BREAK;` |
|       - | 3475 | `			}` |
|       - | 3476 | `		}` |
|      50 | 3477 | `		VM_EXIT_EXCEPTION;` |
|       - | 3478 | `	}` |
|       - | 3479 | `	/* Perform the clone operation */` |
|     275 | 3480 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|     275 | 3481 | `	PH7_MemObjRelease(pTos);` |
|     275 | 3482 | `	if( pClone == 0 ){` |
|     ! 0 | 3483 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 3484 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|     ! 0 | 3485 | `	}else{` |
|       - | 3486 | `		/* Load the cloned object */` |
|     275 | 3487 | `		pTos->x.pOther = pClone;` |
|     275 | 3488 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - | 3489 | `	}` |
|     275 | 3490 | `	VM_EXIT_BREAK;` |
|     ! 0 | 3491 | `	VM_EXIT_BREAK;` |
|     226 | 3492 | `}` |
|       - | 3493 |  |
