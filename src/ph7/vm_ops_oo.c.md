# src/ph7/vm_ops_oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1789/1991 lines (89.85%)

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
|       - |   28 | `/*` |
|       - |   29 | ` * php's abstract-method refusal for an ANONYMOUS class, raised where php raises` |
|       - |   30 | `` * it: at the `new`. Answers 1 (and has reported the fatal and requested the halt)`` |
|       - |   31 | ` * when the class leaves one unimplemented, 0 when it does not.` |
|       - |   32 | ` */` |
|     342 |   33 | `static int VmAnonAbstractGapFatal(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |   34 | `{` |
|       - |   35 | `	SyBlob sMsg;` |
|       - |   36 | `	int bGap;` |
|     347 |   37 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     347 |   38 | `	bGap = PH7_ClassAbstractGap(&(*pVm),pClass,&sMsg) != 0;` |
|     347 |   39 | `	if( bGap ){` |
|       8 |   40 | `		PH7_VmFatalError(&(*pVm),"%.*s",` |
|       4 |   41 | `			(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       6 |   42 | `		pVm->iExitStatus = 255;` |
|       6 |   43 | `		pVm->bHaltRequested = 1;` |
|       2 |   44 | `	}` |
|     347 |   45 | `	SyBlobRelease(&sMsg);` |
|     347 |   46 | `	return bGap;` |
|       5 |   47 | `}` |
| 2140116 |   48 | `PH7_PRIVATE VmOpRc VmExecOpNew(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   49 | `{` |
| 2140121 |   50 | `	ph7_value *pTos = pState->pTos;` |
| 2140121 |   51 | `	ph7_value *pStack = pState->pStack;` |
| 2140121 |   52 | `	VmInstr *aInstr = pState->aInstr;` |
| 2140121 |   53 | `	sxi32 pc = pState->pc;` |
|       - |   54 | `	sxi32 rc;` |
|       - |   55 | `	/* Constructor arg count: compile-time args plus THIS new's own unpack` |
|       - |   56 | `	 * expansion (iP2 = hasSpread, transferred from the popped OP_CALL —` |
|       - |   57 | ``	 * `new C(...$args)` used to ignore the extras, leaving the expanded`` |
|       - |   58 | `	 * elements ABOVE the class-name slot and fataling "Class ' ' is not` |
|       - |   59 | `	 * defined"). VmSpreadOwnExtra counts only this new's own runs, so a nested` |
|       - |   60 | `	 * spread call in the ctor arg list stays scoped to itself. */` |
|       - |   61 | `	/* iP1 < 0 is the SCREEN pass: php's NEW resolves the class, refuses everything a` |
|       - |   62 | ``	 * `new` can be refused for and allocates the object BEFORE the constructor arguments`` |
|       - |   63 | `	 * are evaluated — only the constructor body runs after them. PHL evaluated the whole` |
|       - |   64 | ``	 * argument list first, so `new NoSuchClass(s(1))`, `new AbstractC(s(1))` and`` |
|       - |   65 | ``	 * `new PrivateCtorC(s(1))` all ran `s(1)` on a `new` php never performs. The screen`` |
|       - |   66 | `	 * is this same handler with no arguments on the stack, returning just before the` |
|       - |   67 | `	 * allocation and LEAVING the class name for the real pass that follows it — one code` |
|       - |   68 | `	 * path, so the two can never disagree about what a refusal is. */` |
| 2140121 |   69 | `	int bScreenOnly = pInstr->iP1 < 0;` |
| 2140121 |   70 | `	if( pInstr->iP1 == -2 ){` |
|       - |   71 | `		/* An anonymous class's DECLARATION screen: the class is in p3, the stack is` |
|       - |   72 | `		 * untouched and the constructor arguments have not run yet (see` |
|       - |   73 | `		 * PH7_CompileAnnonClass). */` |
|     175 |   74 | `		if( pInstr->p3 && VmAnonAbstractGapFatal(&(*pVm),(ph7_class *)pInstr->p3) ){` |
|       6 |   75 | `			VM_EXIT_ABORT;` |
|       - |   76 | `		}` |
|     171 |   77 | `		VM_EXIT_BREAK;` |
|       - |   78 | `	}` |
| 2702342 |   79 | `	sxi32 nCtorArgs = bScreenOnly ? 0` |
| 1632385 |   80 | `		: pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|       - |   81 | `	ph7_value *pArg;` |
| 2139951 |   82 | `	ph7_class *pClass = 0;` |
|       - |   83 | `	ph7_class_instance *pNew;` |
|       - |   84 | `	/* Resolving the class below can run an AUTOLOADER that throws. Snapshot the boundary` |
|       - |   85 | `	 * rail so the "not found" report can tell a missing class from an autoloader that` |
|       - |   86 | `	 * raised — php propagates the autoloader's exception and reports nothing else, while` |
|       - |   87 | `	 * this arm used to throw its own Error on top of it (uncaught, killing the script` |
|       - |   88 | `	 * right after the real exception had been handled). */` |
| 2139951 |   89 | `	sxi32 nNewBrc = pVm->nBoundaryRc;` |
| 2139951 |   90 | `	const void *pNewRes = (const void *)pVm->pResumeFrame;` |
| 2139951 |   91 | `	pArg = &pTos[-nCtorArgs]; /* Constructor arguments (if available) */` |
|       - |   92 | `	/* Same PHP 8.1 spread-key realignment as OP_CALL: build a per-actual-slot name` |
|       - |   93 | `	 * map when the ctor arg list unpacked string-keyed elements. NEW keeps the` |
|       - |   94 | `	 * class-name slot at pTos (above the args), so pArg is already the correct base;` |
|       - |   95 | `	 * the build also truncates this call's captured runs. */` |
|       - |   96 | `	VmCallArgMap sEffNewMap;` |
|       - |   97 | `	/* The screen pass has no arguments and no runs of its own: building (and` |
|       - |   98 | `	 * truncating) an effective map there would speak for the enclosing call. */` |
| 2702342 |   99 | `	VmCallArgMap *pEffNewMap = bScreenOnly ? 0 : VmEffCallArgMap(pVm,pInstr,pArg,` |
| 1124805 |  100 | `		nCtorArgs > 0 ? (sxu32)nCtorArgs : 0,&sEffNewMap);` |
| 3209931 |  101 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
| 2139931 |  102 | `		const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
| 2139931 |  103 | `		sxu32 nCls = SyBlobLength(&pTos->sBlob);` |
| 2139926 |  104 | `		if( (nCls == sizeof("self")-1 && SyMemcmp(zCls,"self",sizeof("self")-1) == 0)` |
| 2139908 |  105 | `		 \|\| (nCls == sizeof("static")-1 && SyMemcmp(zCls,"static",sizeof("static")-1) == 0)` |
| 2139853 |  106 | `		 \|\| (nCls == sizeof("parent")-1 && SyMemcmp(zCls,"parent",sizeof("parent")-1) == 0) ){` |
|       - |  107 | `			/* new self() / new static() / new parent(): resolve against the live` |
|       - |  108 | `			 * class context (LSB for static), sharing the FCC resolver. */` |
|      85 |  109 | `			pClass = VmFccResolveScope(&(*pVm),pTos);` |
|      85 |  110 | `			if( pClass && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) ){` |
|       - |  111 | `				/* Not new-able (the named path's iLoadable extract excludes these` |
|       - |  112 | `				 * before it ever gets here): php's wording, with the RESOLVED name.` |
|       - |  113 | `				 *` |
|       - |  114 | `				 * It is the SAME refusal the named path makes a few lines below and it` |
|       - |  115 | ``				 * is thrown the same way -- a catchable `Error`. This arm used to print`` |
|       - |  116 | `				 * an uncatchable engine diagnostic and abort with exit status 0, so` |
|       - |  117 | ``				 * `new parent()` on an abstract base could neither be caught nor even`` |
|       - |  118 | `				 * be told apart from a clean run by the shell. */` |
|       - |  119 | `				SyBlob sErrAbs;` |
|       - |  120 | `				sxi32 rcAbs;` |
|      13 |  121 | `				const char *zKindAbs = (pClass->iFlags & PH7_CLASS_INTERFACE) ? "interface"` |
|       8 |  122 | `					: (pClass->iFlags & PH7_CLASS_TRAIT) ? "trait" : "abstract class";` |
|       9 |  123 | `				SyBlobInit(&sErrAbs,&pVm->sAllocator);` |
|       9 |  124 | `				SyBlobFormat(&sErrAbs,"Cannot instantiate %s %z",zKindAbs,&pClass->sDisp);` |
|       9 |  125 | `				if( nCtorArgs > 0 ){` |
|     ! 0 |  126 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  127 | `				}` |
|       9 |  128 | `				PH7_MemObjRelease(pTos);` |
|       9 |  129 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       9 |  130 | `				pTos->nIdx = SXU32_HIGH;` |
|      13 |  131 | `				rcAbs = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrAbs),` |
|       4 |  132 | `					SyBlobLength(&sErrAbs));` |
|       9 |  133 | `				SyBlobRelease(&sErrAbs);` |
|       9 |  134 | `				if( rcAbs == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 |  135 | `				rc = rcAbs;` |
|       9 |  136 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  137 | `			}` |
|      40 |  138 | `		}else{` |
|       - |  139 | `			/* Try to extract the desired class */` |
| 2139849 |  140 | `			pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,` |
|       - |  141 | `				TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|       5 |  142 | `		}` |
| 1069959 |  143 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - |  144 | `		/* Take the base class from the loaded instance */` |
|       7 |  145 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       3 |  146 | `	}` |
| 2139943 |  147 | `	if( pClass == 0 ){` |
|       - |  148 | ``		/* php: `new NoSuch()` is a CATCHABLE Error ('Class "NoSuch" not found'), not a`` |
|       - |  149 | `		 * hard stop. Aborting here killed the script outright -- and with exit status 0,` |
|       - |  150 | `		 * so a caller could not even tell it had failed. */` |
|       - |  151 | `		SyBlob sErrM;` |
|       - |  152 | `		sxi32 rcErr;` |
|      60 |  153 | `		ph7_class *pNotNew = 0;` |
|      60 |  154 | `		if( PH7_VmClassLookupRaised(&(*pVm),nNewBrc,pNewRes) ){` |
|       - |  155 | `			/* The autoloader threw: land THAT (an in-place catch leaves 0 behind plus a` |
|       - |  156 | `			 * recorded resume frame, which the router picks up). */` |
|       8 |  157 | `			sxi32 rcAuto = pVm->nBoundaryRc;` |
|       8 |  158 | `			pVm->nBoundaryRc = 0;` |
|       8 |  159 | `			if( nCtorArgs > 0 ){` |
|     ! 0 |  160 | `				VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  161 | `			}` |
|       8 |  162 | `			PH7_MemObjRelease(pTos);` |
|       8 |  163 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|       8 |  164 | `			pTos->nIdx = SXU32_HIGH;` |
|       8 |  165 | `			if( rcAuto == PH7_ABORT ){` |
|     ! 0 |  166 | `				VM_EXIT_ABORT;` |
|       - |  167 | `			}` |
|       8 |  168 | `			rc = PH7_EXCEPTION;` |
|       8 |  169 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  170 | `		}` |
|      54 |  171 | `		SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      54 |  172 | `		if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       - |  173 | `			/* The extract above only accepts NEW-able classes, so an interface, an` |
|       - |  174 | `			 * abstract class or a trait comes back as 0 and used to be reported as` |
|       - |  175 | `			 * "not found". Look again without that filter so php's real message can be` |
|       - |  176 | `			 * given — but through the table DIRECTLY, never PH7_VmExtractClass: that` |
|       - |  177 | `			 * one fires the autoloader when the name is absent, so a genuinely missing` |
|       - |  178 | `			 * class ran every registered autoloader TWICE where php runs them once. */` |
|      40 |  179 | `			const char *zNotNew = (const char *)SyBlobData(&pTos->sBlob);` |
|      40 |  180 | `			sxu32 nNotNew = SyBlobLength(&pTos->sBlob);` |
|       - |  181 | `			SyHashEntry *pNotNewEntry;` |
|      40 |  182 | `			PH7_VmClassNameAnchor(&zNotNew,&nNotNew);` |
|      40 |  183 | `			pNotNewEntry = nNotNew > 0 ? SyHashGet(&pVm->hClass,(const void *)zNotNew,nNotNew) : 0;` |
|      40 |  184 | `			pNotNew = pNotNewEntry ? (ph7_class *)pNotNewEntry->pUserData : 0;` |
|      18 |  185 | `		}` |
|      62 |  186 | `		if( pNotNew && (pNotNew->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) ){` |
|       - |  187 | `			/* php names WHAT it will not instantiate; a trait is one of the three, and` |
|       - |  188 | `			 * PH7's loadable-only extract rejected it as "not found" — the one shape of` |
|       - |  189 | ``			 * `new` whose refusal did not say why. */`` |
|      25 |  190 | `			const char *zKind = (pNotNew->iFlags & PH7_CLASS_INTERFACE) ? "interface"` |
|      14 |  191 | `				: (pNotNew->iFlags & PH7_CLASS_TRAIT) ? "trait" : "abstract class";` |
|      17 |  192 | `			SyBlobFormat(&sErrM,"Cannot instantiate %s %z",zKind,&pNotNew->sDisp);` |
|      46 |  193 | `		}else if( (pTos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|       - |  194 | `` 			/* php refuses the OPERAND before it ever has a name to look up: a `new` `` |
|       - |  195 | `			 * takes an object or a string and nothing else, and every other value --` |
|       - |  196 | `			 * int, float, bool, null, array, resource -- is` |
|       - |  197 | ``			 * `Class name must be a valid object or a string`. PHL string-cast the`` |
|       - |  198 | ``			 * slot's raw blob instead, so all six answered `Class "" not found`, a`` |
|       - |  199 | `			 * sentence that names a class the program never wrote. (An EMPTY string` |
|       - |  200 | ``			 * really is `Class "" not found` in php, so the test is the TYPE.) The`` |
|       - |  201 | ``			 * `::` twin of this refusal is already at the bottom of VmExecOpMember. */`` |
|      13 |  202 | `			SyBlobAppend(&sErrM,"Class name must be a valid object or a string",` |
|       - |  203 | `				sizeof("Class name must be a valid object or a string")-1);` |
|       7 |  204 | `		}else{` |
|      26 |  205 | `			SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|      22 |  206 | `				SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob));` |
|       - |  207 | `		}` |
|       - |  208 | `		/* Settle the stack BEFORE throwing: the class-name slot becomes the (NULL)` |
|       - |  209 | `		 * expression result and the ctor arguments go. */` |
|      54 |  210 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  211 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  212 | `		}` |
|      54 |  213 | `		PH7_MemObjRelease(pTos);` |
|      54 |  214 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|      54 |  215 | `		pTos->nIdx = SXU32_HIGH;` |
|      79 |  216 | `		rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      25 |  217 | `			SyBlobLength(&sErrM));` |
|      54 |  218 | `		SyBlobRelease(&sErrM);` |
|      54 |  219 | `		if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      54 |  220 | `		rc = rcErr;` |
|      56 |  221 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
| 2139887 |  222 | `	}else if( pClass->iFlags & PH7_CLASS_NOINSTANTIATE ){` |
|       - |  223 | `		/* php refuses this one in the create_object handler, so the refusal names the` |
|       - |  224 | `		 * INSTANTIATION and never reaches the constructor — which matters because the` |
|       - |  225 | `		 * class may still declare a private __construct that Reflection prints. Same` |
|       - |  226 | `		 * shape as the enum reject below: no instance, no __destruct. */` |
|       - |  227 | `		SyBlob sErrMsg;` |
|      42 |  228 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      42 |  229 | `		if( pClass->zNewRefusal ){` |
|       - |  230 | `			/* php words a few of these per class — Directory's names dir() as the` |
|       - |  231 | `			 * way to get one — so the spec's own sentence wins when it has one. */` |
|      38 |  232 | `			SyBlobAppend(&sErrMsg,pClass->zNewRefusal,SyStrlen(pClass->zNewRefusal));` |
|      21 |  233 | `		}else{` |
|       5 |  234 | `			SyBlobFormat(&sErrMsg,"Instantiation of class %z is not allowed",&pClass->sDisp);` |
|       - |  235 | `		}` |
|       - |  236 | `		{` |
|      42 |  237 | `			const char *zRefCls = pClass->zNewRefusalClass ? pClass->zNewRefusalClass : "Error";` |
|      61 |  238 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),zRefCls,` |
|      38 |  239 | `				(sxu32)SyStrlen(zRefCls),&sErrMsg));` |
|       - |  240 | `		}` |
|      42 |  241 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  242 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  243 | `		}` |
|      42 |  244 | `		PH7_MemObjRelease(pTos);` |
|      42 |  245 | `		pTos->nIdx = SXU32_HIGH;` |
|      42 |  246 | `		VM_EXIT_BREAK;` |
| 2139849 |  247 | `	}else if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|       - |  248 | `		/* php 8.1: enums cannot be instantiated — a catchable Error, raised` |
|       - |  249 | `		 * BEFORE any construction (no instance, no __destruct). */` |
|       - |  250 | `		SyBlob sErrMsg;` |
|       9 |  251 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       9 |  252 | `		SyBlobFormat(&sErrMsg,"Cannot instantiate enum %z",&pClass->sDisp);` |
|       9 |  253 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       9 |  254 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  255 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  256 | `		}` |
|       9 |  257 | `		PH7_MemObjRelease(pTos);` |
|       9 |  258 | `		pTos->nIdx = SXU32_HIGH;` |
|       9 |  259 | `		VM_EXIT_BREAK;` |
| 2139836 |  260 | `	}else if( (pClass->iFlags & PH7_CLASS_ANON)` |
| 1069988 |  261 | `	       && VmAnonAbstractGapFatal(&(*pVm),pClass) ){` |
|       - |  262 | `		/* An anonymous class's DECLARATION is an expression, so php asks whether it` |
|       - |  263 | ``		 * leaves an abstract method unimplemented when the `new` RUNS -- not when the`` |
|       - |  264 | `		 * unit compiles. The refusal therefore carries the frame chain that reached` |
|       - |  265 | ``		 * it, and a `new` on a branch nothing takes is never asked at all. PHL mounts`` |
|       - |  266 | `		 * the class at compile time and used to ask there, which failed the whole FILE` |
|       - |  267 | ``		 * on a `new` php never performs. Uncatchable in php too, so nothing user`` |
|       - |  268 | `		 * written depends on either timing. */` |
|     ! 0 |  269 | `		VM_EXIT_ABORT;` |
| 2139836 |  270 | `	}else if( VmClassStaticDeferPending(pClass)` |
| 1069904 |  271 | `	       && (rc = PH7_VmMaterializeClassStatics(&(*pVm),pClass)) != SXRET_OK ){` |
|       - |  272 | `		/* php materializes the static table at instantiation too, so a default` |
|       - |  273 | `		 * that THREW at the declaration (or failed its type check) raises BEFORE` |
|       - |  274 | `		 * any construction (no instance, no __destruct) — same shape as the enum` |
|       - |  275 | `		 * reject above: park the status, settle the stack, and let the` |
|       - |  276 | `		 * fetch-point router land it. */` |
|       6 |  277 | `		VmBoundaryPark(&(*pVm),rc);` |
|       6 |  278 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  279 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  280 | `		}` |
|       6 |  281 | `		PH7_MemObjRelease(pTos);` |
|       6 |  282 | `		pTos->nIdx = SXU32_HIGH;` |
|       6 |  283 | `		VM_EXIT_BREAK;` |
|     ! 0 |  284 | `	}else{` |
|       - |  285 | `		ph7_class_method *pCons;` |
|       - |  286 | `		/* Check if a constructor is available — BEFORE instantiation: a` |
|       - |  287 | ``		 * visibility-denied `new` must not construct (nor later destruct)`` |
|       - |  288 | `		 * the object (band A #4). */` |
|       - |  289 | `		/* Only an explicit __construct is the constructor. PHP-4-style class-name` |
|       - |  290 | `		 * constructors were removed in PHP 8.0 — a method named like the class is a` |
|       - |  291 | `		 * plain method, so no same-name fallback here. */` |
| 2139837 |  292 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       - |  293 | `		/* Constructor visibility (band A #4): __construct now KEEPS its` |
|       - |  294 | `		 * declared protection (PH7_NewClassMethod no longer forces it` |
|       - |  295 | ``		 * public), so `new C()` on a private/protected constructor from`` |
|       - |  296 | `		 * the wrong scope is php's catchable Error. Reflection's` |
|       - |  297 | `		 * newInstance path sets bReflectBypass like method invoke. */` |
| 2139837 |  298 | `		if( pCons && pCons->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - |  299 | `			/* php binds non-public access by the member's DECLARING class, not the` |
|       - |  300 | `			 * instantiated class: a private constructor declared in a base is` |
|       - |  301 | `			 * reachable from that base's own methods even when instantiating a` |
|       - |  302 | ``			 * subclass (`new Child()` inside Base::factory()). __construct is a`` |
|       - |  303 | `			 * method, so pass its declaring class (sFunc.pUserData) — mirroring the` |
|       - |  304 | `			 * method-call visibility check — rather than pClass, whose hAttr lookup` |
|       - |  305 | `			 * for a method name always misses and falls to a wrong exact-class test. */` |
|      45 |  306 | `			ph7_class *pCtorDecl = pCons->sFunc.pUserData ? (ph7_class *)pCons->sFunc.pUserData : pClass;` |
|      45 |  307 | `			if( pVm->bReflectBypass ){` |
|     ! 0 |  308 | `				pVm->bReflectBypass = 0;` |
|      45 |  309 | `			}else if( !PH7_VmClassMemberAccess(&(*pVm),pCtorDecl,&pCons->sFunc.sName,pCons->iProtection,FALSE) ){` |
|       - |  310 | `				SyBlob sErrMsg;` |
|      29 |  311 | `				const char *zVis = pCons->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       - |  312 | `				/* php NAMES the calling scope when there is one (see the method twin in` |
|       - |  313 | `				 * OP_CALL); "global scope" is only for code outside every class. */` |
|      29 |  314 | `				ph7_class *pCtorScope = PH7_VmCallerScope(&(*pVm));` |
|      29 |  315 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      29 |  316 | `				if( pCtorScope ){` |
|       9 |  317 | `					SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from scope %z",` |
|       4 |  318 | `						zVis,&pClass->sDisp,&pCtorScope->sDisp);` |
|       5 |  319 | `				}else{` |
|      21 |  320 | `					SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from global scope",` |
|      10 |  321 | `						zVis,&pClass->sDisp);` |
|       - |  322 | `				}` |
|      29 |  323 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       - |  324 | `				/* Pop ctor args + release the class-name operand, leave NULL` |
|       - |  325 | `				 * as the expression value; the fetch-point router lands the` |
|       - |  326 | `				 * throw. No instance was created. */` |
|      29 |  327 | `				if( nCtorArgs > 0 ){` |
|     ! 0 |  328 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  329 | `				}` |
|      29 |  330 | `				PH7_MemObjRelease(pTos);` |
|      29 |  331 | `				pTos->nIdx = SXU32_HIGH;` |
|      29 |  332 | `				VM_EXIT_BREAK;` |
|       - |  333 | `			}` |
|       8 |  334 | `		}` |
| 2139809 |  335 | `		if( bScreenOnly ){` |
|       - |  336 | `			/* Every refusal above has been asked. Leave the class name standing for` |
|       - |  337 | `			 * the real pass and let the arguments run. */` |
| 1015114 |  338 | `			VM_EXIT_BREAK;` |
|       - |  339 | `		}` |
| 1124700 |  340 | `		if( nCtorArgs > 0 ){` |
|       - |  341 | ``			/* D1: a `new`'s arguments are ARGUMENTS. An element/property one whose`` |
|       - |  342 | `			 * target was absent at load time rides a deferred carrier that only OP_CALL` |
|       - |  343 | `			 * resolved, and the ctor call reaches its callee by pointer rather than` |
|       - |  344 | ``			 * through that opcode — so `new C($a['missing'])` passed a silent NULL where`` |
|       - |  345 | ``			 * php warns, and a by-REFERENCE ctor parameter (`new C($a['fresh'])` with`` |
|       - |  346 | ``			 * `__construct(&$x)`) never vivified the target at all: php writes the`` |
|       - |  347 | `			 * element, PHL left it uncreated. Resolve here, where the constructor is` |
|       - |  348 | `			 * known and pVm->pFrame is still the caller, exactly as every OP_CALL` |
|       - |  349 | `			 * dispatch branch does. A class with NO constructor still resolves — the` |
|       - |  350 | `			 * arguments were evaluated and php reports what reading them found. */` |
| 1015160 |  351 | `			ph7_class_method *pCtorArgs = pCons;` |
|       - |  352 | `			sxi32 rcDA;` |
| 1015160 |  353 | `			if( pCtorArgs == 0 ){` |
|       3 |  354 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pEffNewMap);` |
| 1015159 |  355 | `			}else if( pCtorArgs->sFunc.iFlags & VM_FUNC_NATIVE ){` |
|       - |  356 | `				/* A native constructor has no compiled formals; its by-ref positions` |
|       - |  357 | `				 * come from the signature-derived mask, the builtin way. */` |
| 1014297 |  358 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
| 1014311 |  359 | `					pCtorArgs->sFunc.pNative ? pCtorArgs->sFunc.pNative->nByRefMask : 0,0,0,pEffNewMap);` |
|  507151 |  360 | `			}else{` |
|    1268 |  361 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|     842 |  362 | `					(ph7_vm_func_arg *)SySetBasePtr(&pCtorArgs->sFunc.aArgs),` |
|     421 |  363 | `					SySetUsed(&pCtorArgs->sFunc.aArgs),0,0,0,pEffNewMap);` |
|       - |  364 | `			}` |
| 1015160 |  365 | `			if( rcDA == PH7_ABORT \|\| rcDA == PH7_EXCEPTION ){` |
|       - |  366 | `				/* Reading an argument raised (a string-offset Error, a magic accessor's` |
|       - |  367 | `				 * throw): no object is created, and the stack is tidied exactly as the` |
|       - |  368 | `				 * constructor-throw path below tidies it. */` |
|       - |  369 | `				sxi32 iResumeDA;` |
|       3 |  370 | `				if( rcDA == PH7_ABORT ){` |
|     ! 0 |  371 | `					VM_EXIT_ABORT;` |
|       - |  372 | `				}` |
|       3 |  373 | `				if( VmRecordedResume(pVm,&iResumeDA,pState->pEntryFrame,aInstr) ){` |
|     ! 0 |  374 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  375 | `					PH7_MemObjRelease(pTos);` |
|     ! 0 |  376 | `					PH7_RESUME_DRAIN()` |
|     ! 0 |  377 | `					pc = iResumeDA;` |
|     ! 0 |  378 | `					VM_EXIT_BREAK;` |
|       - |  379 | `				}` |
|       3 |  380 | `				VM_EXIT_EXCEPTION;` |
|       - |  381 | `			}` |
|  507567 |  382 | `		}` |
|       - |  383 | `		/* Create a new class instance */` |
| 1124698 |  384 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
| 1124698 |  385 | `		if( pNew == 0 ){` |
|     ! 0 |  386 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  387 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|     ! 0 |  388 | `				&pClass->sDisp` |
|       - |  389 | `			);` |
|     ! 0 |  390 | `			PH7_MemObjRelease(pTos);` |
|     ! 0 |  391 | `			if( nCtorArgs > 0 ){` |
|       - |  392 | `				/* Pop given arguments */` |
|     ! 0 |  393 | `				VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  394 | `			}` |
|     ! 0 |  395 | `			VM_EXIT_BREAK;` |
|       - |  396 | `		}` |
| 1124698 |  397 | `		if( pVm->nBoundaryRc != 0 ){` |
|       - |  398 | `			/* A typed property DEFAULT failed its type check during instance-` |
|       - |  399 | `			 * frame creation (parked catchable TypeError): php aborts the` |
|       - |  400 | `			 * construction — no constructor call, no object. Consume the` |
|       - |  401 | `			 * parked status here and route exactly like a constructor throw` |
|       - |  402 | `			 * (the exception object is already registered). */` |
|      37 |  403 | `			sxi32 rcDef = pVm->nBoundaryRc;` |
|       - |  404 | `			sxi32 iDefResumePc;` |
|      37 |  405 | `			pVm->nBoundaryRc = 0;` |
|       - |  406 | `			/* php never reaches a __destruct here either: object_init_ex evaluates the` |
|       - |  407 | `			 * defaults BEFORE the object exists, so a failing one leaves nothing to` |
|       - |  408 | `			 * destruct. PHL builds the instance first, so it marks it instead. */` |
|      37 |  409 | `			PH7_ClassInstanceCtorFailed(pNew);` |
|      37 |  410 | `			PH7_ClassInstanceUnref(pNew);` |
|      37 |  411 | `			if( rcDef == PH7_ABORT ){` |
|     ! 0 |  412 | `				VM_EXIT_ABORT;` |
|       - |  413 | `			}` |
|      37 |  414 | `			if( VmRecordedResume(pVm,&iDefResumePc,pState->pEntryFrame,aInstr) ){` |
|       - |  415 | `				/* This frame's own try caught it in-place: tidy the stack` |
|       - |  416 | `				 * (pop ctor args + release the class-name slot, then drain any` |
|       - |  417 | `				 * abandoned outer-expression operands to the try's base — the` |
|       - |  418 | `				 * class-name slot itself sits above it) and resume. */` |
|      37 |  419 | `				if( nCtorArgs > 0 ){` |
|       3 |  420 | `					VmPopOperand(&pTos,nCtorArgs);` |
|       1 |  421 | `				}` |
|      37 |  422 | `				PH7_MemObjRelease(pTos);` |
|      75 |  423 | `				PH7_RESUME_DRAIN()` |
|      37 |  424 | `				pc = iDefResumePc;` |
|      37 |  425 | `				VM_EXIT_BREAK;` |
|       - |  426 | `			}` |
|     ! 0 |  427 | `			VM_EXIT_EXCEPTION;` |
|       - |  428 | `		}` |
| 1124662 |  429 | `		if( pCons ){` |
|       - |  430 | `			/* Call the class constructor.  Collect args in stack order and` |
|       - |  431 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|       - |  432 | `			 * receiving OP_CALL path runs its named-argument matching` |
|       - |  433 | `			 * (including variadic string-key packing). */` |
| 1117454 |  434 | `			VmCallArgMap *pNewMap = pEffNewMap;` |
|       - |  435 | `			sxi32 rcCons;` |
|       - |  436 | `			SySet aArg; /* ctor argument scratch (the loop's shared set stays with OP_CALL) */` |
| 1117454 |  437 | `			SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
| 2135379 |  438 | `			while( pArg < pTos ){` |
| 1017930 |  439 | `				SySetPut(&aArg,(const void *)&pArg);` |
| 1017930 |  440 | `				pArg++;` |
|       5 |  441 | `			}` |
|       - |  442 | `			/* Too-few-arguments is php's catchable ArgumentCountError, raised` |
|       - |  443 | `			 * by the shared OP_CALL install path this ctor call routes through` |
|       - |  444 | `			 * (was a PHL-only notice here). */` |
| 1117454 |  445 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
| 1117454 |  446 | `			SySetRelease(&aArg); /* scratch dies here, on every exit path */` |
|       - |  447 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
| 1117454 |  448 | `			if( pNew->iRef < 1 ){` |
|     ! 0 |  449 | `				pNew->iRef = 1;` |
|     ! 0 |  450 | `			}` |
| 1117454 |  451 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|       - |  452 | `				/* The constructor raised: the half-constructed object must not` |
|       - |  453 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|       - |  454 | `				 * The class-name operand (and any leftover args) are released by` |
|       - |  455 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|       - |  456 | `				sxi32 iResumePc;` |
|       - |  457 | `				/* php's ctor-failed mark: no __destruct for an object whose` |
|       - |  458 | `				 * constructor threw, here or ever (see PH7_ClassInstanceCtorFailed). */` |
|  100886 |  459 | `				PH7_ClassInstanceCtorFailed(pNew);` |
|  100886 |  460 | `				PH7_ClassInstanceUnref(pNew);` |
|  100886 |  461 | `				if( rcCons == PH7_ABORT ){` |
|      10 |  462 | `					VM_EXIT_ABORT;` |
|       - |  463 | `				}` |
|  100878 |  464 | `				if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){` |
|       - |  465 | `					/* This frame's own try caught it in-place: tidy the stack` |
|       - |  466 | `					 * (pop ctor args + release the class-name slot, then drain any` |
|       - |  467 | `					 * abandoned outer-expression operands to the try's base — the` |
|       - |  468 | `					 * class-name slot itself sits above it) and resume. */` |
|  100601 |  469 | `					if( nCtorArgs > 0 ){` |
|     589 |  470 | `						VmPopOperand(&pTos,nCtorArgs);` |
|     292 |  471 | `					}` |
|  100601 |  472 | `					PH7_MemObjRelease(pTos);` |
|  501073 |  473 | `					PH7_RESUME_DRAIN()` |
|  100601 |  474 | `					pc = iResumePc;` |
|  100601 |  475 | `					VM_EXIT_BREAK;` |
|       - |  476 | `				}` |
|     282 |  477 | `				VM_EXIT_EXCEPTION;` |
|       - |  478 | `			}` |
|  508275 |  479 | `		}` |
| 1023781 |  480 | `		if( nCtorArgs > 0 ){` |
|       - |  481 | `			/* Pop given arguments */` |
| 1014299 |  482 | `			VmPopOperand(&pTos,nCtorArgs);` |
|  507138 |  483 | `		}` |
| 1023781 |  484 | `		PH7_MemObjRelease(pTos);` |
| 1023781 |  485 | `		pTos->x.pOther = pNew;` |
| 1023781 |  486 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - |  487 | `	}` |
| 1023781 |  488 | `	VM_EXIT_BREAK;` |
|     ! 0 |  489 | `	VM_EXIT_BREAK;` |
| 1070042 |  490 | `}` |
|       - |  491 |  |
|       - |  492 | `/*` |
|       - |  493 | `` * Is this OP_MEMBER the SOURCE of a reference bind — `$r =& $o->p`, `$a[] =& $o->p`,`` |
|       - |  494 | `` * `[&$o->p]`, a by-reference `foreach ($o->p as &$v)`, a by-reference destructuring`` |
|       - |  495 | `` * source? php compiles all of them with `zend_compile_var(source, BP_VAR_W)`, so the`` |
|       - |  496 | ` * fetch is a WRITE context: a missing property is CREATED, not warned about. PHL` |
|       - |  497 | ` * leaves the instruction a PH7_MEMBER_READ on purpose -- a handler-backed property` |
|       - |  498 | ` * still hands back a copy there, and an overloaded one still dispatches __get -- and` |
|       - |  499 | ` * carries the context in a flag the compiler stamps beside it.` |
|       - |  500 | ` */` |
|  325183 |  501 | `static int VmMemberFetchIsRefSource(const VmInstr *pInstr)` |
|       5 |  502 | `{` |
|  325188 |  503 | `	return pInstr->iP2 == PH7_MEMBER_READ && pInstr->bRefSrc != 0;` |
|       5 |  504 | `}` |
|       - |  505 | `/*` |
|       - |  506 | ` * Is this OP_MEMBER fetching the property for WRITING — php's BP_VAR_W / BP_VAR_RW` |
|       - |  507 | ` * fetch, the one that asks the object for something to MODIFY? The compiler tags` |
|       - |  508 | `` * the base of a subscript-write / `??=` PH7_MEMBER_WRITE, so that answers most of`` |
|       - |  509 | ``  * it once the shapes that share the tag are excluded: a plain store and a `??=` `` |
|       - |  510 | ` * write through their own paths, and a direct read-modify-write is the accessor` |
|       - |  511 | ` * pair (VmMagicRmwArm), not a write fetch. The shapes php compiles as plain READS —` |
|       - |  512 | ` * a reference SOURCE, in all its spellings — carry the compiler's own marker` |
|       - |  513 | `` * (VmMemberFetchIsRefSource). `$o->p =& $x` is NOT one of them: the member is`` |
|       - |  514 | ` * the reference TARGET there and carries its own iP2.` |
|       - |  515 | ` */` |
|  100814 |  516 | `static int VmMemberFetchForWrite(const VmInstr *pInstr)` |
|       5 |  517 | `{` |
|  100819 |  518 | `	const VmInstr *pNext = pInstr + 1;` |
|  100819 |  519 | `	if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|     581 |  520 | `		int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);` |
|     581 |  521 | `		int bCoalesceW = (pNext->iOp == PH7_OP_NULLC_JMP);` |
|     581 |  522 | `		return !bPlainStore && !bCoalesceW && !VmMemberNextIsRmw(pNext);` |
|       - |  523 | `	}` |
|  100243 |  524 | `	return VmMemberFetchIsRefSource(pInstr);` |
|   50412 |  525 | `}` |
|       - |  526 | `/*` |
|       - |  527 | ` * A native class's property is a field of php's own C struct, and the only writes` |
|       - |  528 | ` * that reach one are the writes the store filter converts (ph7_class::xSet). So` |
|       - |  529 | ` * the fetched value keeps its slot index for exactly the shapes that end in such` |
|       - |  530 | `` * a store -- a plain assignment, `??=`, a destructuring target and the`` |
|       - |  531 | ` * read-modify-write forms -- and is a TEMPORARY for every other use. That is` |
|       - |  532 | ` * php's own answer: it has no ptr_ptr handler for such a property, so a reference` |
|       - |  533 | `` * bind (`$r = &$i->f`), a by-reference argument (`preg_match($p,$s,$i->s)`) and a`` |
|       - |  534 | ` * by-reference foreach all get a copy whose writes are SILENTLY lost -- silently,` |
|       - |  535 | ` * unlike the overloaded case, which php has a notice for.` |
|       - |  536 | ` */` |
|   10149 |  537 | `static int VmMemberNativeSetKeepsSlot(const VmInstr *pInstr)` |
|       5 |  538 | `{` |
|   10154 |  539 | `	if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|       - |  540 | ``		/* Every fetch the compiler tags for writing: a plain store, `??=`, a`` |
|       - |  541 | `		 * compound assign, and the base of a subscript write — that last one has` |
|       - |  542 | ``		 * to reach the real value so `$i->y[0] = 5` is php's "Cannot use a scalar`` |
|       - |  543 | `		 * value as an array" rather than a write nobody notices. */` |
|    1410 |  544 | `		return 1;` |
|       - |  545 | `	}` |
|    8748 |  546 | `	if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       7 |  547 | `		return 1;   /* list()/foreach destructuring target */` |
|       - |  548 | `	}` |
|    8742 |  549 | `	return VmMemberNextIsWrite(pInstr + 1);   /* ++/-- and the compound assigns */` |
|    5080 |  550 | `}` |
|       - |  551 | `/*` |
|       - |  552 | `` * `$doc->encoding ??= 'UTF-8'` on a name a native class's own property-handler`` |
|       - |  553 | ` * table carries.` |
|       - |  554 | ` *` |
|       - |  555 | ` * The member opcode's handler gate answers a READ and can only REFUSE a write,` |
|       - |  556 | ` * because the value does not exist yet -- and a coalesce that finds null must` |
|       - |  557 | ` * make one. Only the overloaded-coalesce rail carries a pending store to the` |
|       - |  558 | ` * point the value arrives, so this shape is left to it: the read it makes goes` |
|       - |  559 | ` * through the same handler anyway.` |
|       - |  560 | ` */` |
|    7209 |  561 | `static int VmMemberCoalOwned(ph7_class_instance *pThis,const VmInstr *pInstr,const SyString *pName)` |
|       5 |  562 | `{` |
|    7611 |  563 | `	return pInstr->iP2 == PH7_MEMBER_WRITE` |
|    4002 |  564 | `	    && pInstr[1].iOp == PH7_OP_NULLC_JMP` |
|    7606 |  565 | `	    && PH7_ClassNativePropOwns(pThis,pName);` |
|       5 |  566 | `}` |
|       - |  567 | `/*` |
|       - |  568 | `` * php's `Indirect modification of overloaded property C::$p has no effect`: the`` |
|       - |  569 | ` * write-context fetch above landed on a property only __get answers for, so what` |
|       - |  570 | ` * comes back is a VALUE and whatever the rest of the expression writes into it is` |
|       - |  571 | ` * thrown away. php says so and carries on.` |
|       - |  572 | ` *` |
|       - |  573 | ` * PHL had the notice at one site only (a by-reference ARGUMENT, VmBindPropByRef)` |
|       - |  574 | ` * and, worse, the value was not a copy: __get's return still shared the object's` |
|       - |  575 | `` * own nested hashmap by COW, so `$o->a['b'] = 9`, `$o->a[] = 5` and a by-reference`` |
|       - |  576 | ` * foreach modified the object php leaves untouched. Separating it here is what` |
|       - |  577 | ` * makes the write land nowhere.` |
|       - |  578 | ` *` |
|       - |  579 | ` * An ENGINE __get is not this — php routes those through the class's own property` |
|       - |  580 | ` * handler, which hands back the real element (ArrayObject's ARRAY_AS_PROPS reads` |
|       - |  581 | ` * and writes through, in both engines). php stays silent for an OBJECT value too:` |
|       - |  582 | ` * a handle is shared, so nothing about the write is indirect.` |
|       - |  583 | ` */` |
|      32 |  584 | `PH7_PRIVATE void PH7_VmOverloadedPropNotice(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,ph7_value *pVal)` |
|       1 |  585 | `{` |
|      33 |  586 | `	ph7_class_method *pGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|      33 |  587 | `	if( pGet == 0 \|\| (pGet->sFunc.iFlags & VM_FUNC_NATIVE) ){` |
|     ! 0 |  588 | `		return;` |
|       - |  589 | `	}` |
|      33 |  590 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       3 |  591 | `		return;` |
|       - |  592 | `	}` |
|      46 |  593 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - |  594 | `		"Indirect modification of overloaded property %z::$%z has no effect",` |
|      15 |  595 | `		&pClass->sDisp,pName);` |
|      31 |  596 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      25 |  597 | `		PH7_HashmapCowSeparate(&(*pVm),pVal);` |
|      12 |  598 | `	}` |
|      17 |  599 | `}` |
|       - |  600 | `/*` |
|       - |  601 | ` * Does an OVERLOADED read-modify-write apply to this property access? php runs` |
|       - |  602 | ` * one only when the class answers BOTH sides; the guard means we are already` |
|       - |  603 | ` * inside this property's own __get, where php behaves as if the accessor were` |
|       - |  604 | ` * absent.` |
|       - |  605 | ` */` |
|      44 |  606 | `static int VmMagicRmwEligible(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,const SyString *pName)` |
|       2 |  607 | `{` |
|      46 |  608 | `	if( PH7_ClassNativePropOwns(pThis,pName) ){` |
|       - |  609 | `		/* A native class's own property handler answers BOTH halves -- php reads` |
|       - |  610 | ``		 * `$doc->version .= '.1'` through read_property and writes the result back`` |
|       - |  611 | `		 * through write_property, with no magic accessor involved either way. */` |
|       5 |  612 | `		return 1;` |
|       - |  613 | `	}` |
|      56 |  614 | `	return PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) != 0` |
|      34 |  615 | `	    && PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1) != 0` |
|      54 |  616 | `	    && !VmMagicGuardHeld(pVm,(void *)pThis,pName,'g');` |
|      24 |  617 | `}` |
|       - |  618 | `/*` |
|       - |  619 | `` * php's read-modify-write of an OVERLOADED property — `$o->n++`, `--$o->n`,`` |
|       - |  620 | `` * `$o->n += 2`, `$o->n .= 'x'` on a name the instance does not expose. php reads`` |
|       - |  621 | ` * the current value through __get, lets the modify op compute on it, and writes` |
|       - |  622 | ` * the result back through __set; PHL fell through to dynamic-property creation` |
|       - |  623 | ` * instead, so the ordinary shape (a class declaring BOTH accessors) died on` |
|       - |  624 | `` * `Cannot create dynamic property C::$n` — or, when the name IS declared but`` |
|       - |  625 | `` * inaccessible from this scope, on `Cannot access private property C::$n` —`` |
|       - |  626 | ` * for a statement php runs.` |
|       - |  627 | ` *` |
|       - |  628 | ` * The write-back rides the same pending-entry rail property HOOKS use: the` |
|       - |  629 | ` * current value goes into a fresh SCRATCH memobj the modify op mutates in` |
|       - |  630 | ` * place, and the entry makes that op's tail (PH7_HOOK_RMW_WRITEBACK) dispatch` |
|       - |  631 | ` * __set with the computed value.` |
|       - |  632 | ` *` |
|       - |  633 | ` * BOTH accessors are required, which is php's own split: with only __get php` |
|       - |  634 | ` * goes on to CREATE the property (PHL's scope policy refuses a dynamic property),` |
|       - |  635 | `` * and with only __set it warns `Undefined property` and reads null — neither is`` |
|       - |  636 | ` * this path.` |
|       - |  637 | ` *` |
|       - |  638 | ` * *pOut takes __get's value and *pnScratch the slot the modify op must address;` |
|       - |  639 | ` * the caller does its own stack surgery afterwards, because pName still aliases` |
|       - |  640 | `` * the NAME operand when the name is dynamic (`$o->$k++`) and releasing that`` |
|       - |  641 | ` * operand first would leave it dangling. *pnScratch stays SXU32_HIGH when __get` |
|       - |  642 | ` * threw (nothing is armed — the fetch-point router lands the parked throw and` |
|       - |  643 | ` * php's __set never runs) or when the scratch reservation failed.` |
|       - |  644 | ` */` |
|      32 |  645 | `static void VmMagicRmwArm(` |
|       - |  646 | `	ph7_vm *pVm,` |
|       - |  647 | `	ph7_class_instance *pThis,` |
|       - |  648 | `	ph7_class *pClass,` |
|       - |  649 | `	const SyString *pName,` |
|       - |  650 | `	ph7_value *pOut,` |
|       - |  651 | `	sxu32 *pnScratch,` |
|       - |  652 | `	void *pOwnerStack,` |
|       - |  653 | `	void *pInstrs,` |
|       - |  654 | `	sxu32 nPc` |
|       - |  655 | `	)` |
|       1 |  656 | `{` |
|       - |  657 | `	ph7_value *pScr;` |
|       - |  658 | `	VmHookRmw sRmw;` |
|      33 |  659 | `	*pnScratch = SXU32_HIGH;` |
|      33 |  660 | `	VmMagicGuardPush(pVm,(void *)pThis,pName,'g');` |
|      33 |  661 | `	PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,pName,pOut);` |
|      33 |  662 | `	VmMagicGuardPop(pVm);` |
|      33 |  663 | `	if( pVm->nBoundaryRc != 0 ){` |
|       3 |  664 | `		PH7_MemObjRelease(pOut);` |
|       3 |  665 | `		return;` |
|       - |  666 | `	}` |
|      31 |  667 | `	pScr = PH7_ReserveMemObj(&(*pVm));` |
|      31 |  668 | `	if( pScr == 0 ){` |
|       - |  669 | `		/* OOM: loud allocator diagnostics already fired; the value still stands. */` |
|     ! 0 |  670 | `		return;` |
|       - |  671 | `	}` |
|      31 |  672 | `	PH7_MemObjStore(pOut,pScr);` |
|      31 |  673 | `	sRmw.iKind = VM_HOOK_PEND_RMW_MAGIC;` |
|      31 |  674 | `	sRmw.pThis = pThis;` |
|      31 |  675 | `	sRmw.pAttr = 0;` |
|      31 |  676 | `	sRmw.nBackIdx = SXU32_HIGH;` |
|      31 |  677 | `	sRmw.nScratchIdx = pScr->nIdx;` |
|      31 |  678 | `	SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      31 |  679 | `	SyBlobAppend(&sRmw.sName,(const void *)pName->zString,pName->nByte);` |
|      31 |  680 | `	sRmw.pOwnerStack = pOwnerStack;` |
|      31 |  681 | `	sRmw.pInstrs = pInstrs;` |
|      31 |  682 | `	sRmw.nJmpPc = nPc;  /* the modify op ... */` |
|      31 |  683 | `	sRmw.nPc = nPc;     /* ... is the whole window */` |
|      31 |  684 | `	pThis->iRef++;` |
|      31 |  685 | `	SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      31 |  686 | `	*pnScratch = pScr->nIdx;` |
|      17 |  687 | `}` |
|       - |  688 | `/*` |
|       - |  689 | `` * The property php's `C::$name` form finds. That form reads the class's whole`` |
|       - |  690 | ` * property table -- instance properties and constants included, answering on` |
|       - |  691 | ` * visibility before static-ness (see the arm that uses it) -- and php's table` |
|       - |  692 | `` * carries a base's PRIVATE instance property as a SHADOW entry, so `B::$q` on`` |
|       - |  693 | `` * `class A { private $q; }` is "Cannot access private property B::$q" and not the`` |
|       - |  694 | ` * undeclared-static sentence. Here that member lives under its mangled STORAGE` |
|       - |  695 | ` * name, which the plain probe cannot see, so the ancestry answers for it: the` |
|       - |  696 | ` * nearest base that declares one under the plain name.` |
|       - |  697 | ` */` |
|  101804 |  698 | `static ph7_class_attr * VmClassAttrWithShadow(ph7_class *pClass,const char *zName,sxu32 nName)` |
|       5 |  699 | `{` |
|       - |  700 | `	ph7_class *pWalk;` |
|  101809 |  701 | `	ph7_class_attr *pAttr = PH7_ClassExtractAttribute(pClass,zName,nName);` |
|  101809 |  702 | `	if( pAttr ){` |
|  101801 |  703 | `		return pAttr;` |
|       - |  704 | `	}` |
|      10 |  705 | `	for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){` |
|     ! 0 |  706 | `		pAttr = PH7_ClassExtractAttribute(pWalk,zName,nName);` |
|     ! 0 |  707 | `		if( pAttr ){` |
|     ! 0 |  708 | `			return (pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|     ! 0 |  709 | `			     && (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0)` |
|     ! 0 |  710 | `				? pAttr : 0;` |
|       - |  711 | `		}` |
|     ! 0 |  712 | `	}` |
|      10 |  713 | `	return 0;` |
|   50907 |  714 | `}` |
|       - |  715 | `/*` |
|       - |  716 | ` * May the executing scope touch this class-level member reached through an` |
|       - |  717 | `` * INSTANCE (`$o->s` on a static, or on a class constant)?`` |
|       - |  718 | ` *` |
|       - |  719 | ` * The ordinary attribute rule, plus php's shadow re-lookup: when the member the` |
|       - |  720 | ` * object's class holds under this name is a private one the scope may not touch,` |
|       - |  721 | ` * php asks the SCOPE's own table for the name and uses what it finds there. With` |
|       - |  722 | `` * `class A { private static $q; } class B extends A { private static $q; }` that`` |
|       - |  723 | `` * is how `$b->q` from inside A reaches A's own -- the as-non-static notice and`` |
|       - |  724 | ` * then the ordinary undefined-property answer, rather than a visibility refusal` |
|       - |  725 | ` * about B's.` |
|       - |  726 | ` */` |
|      24 |  727 | `static int VmStaticThroughInstanceVisible(ph7_vm *pVm,ph7_class *pClass,` |
|       - |  728 | `	ph7_class_attr *pAttr,const SyString *pName)` |
|       3 |  729 | `{` |
|       - |  730 | `	ph7_class *pScope;` |
|       - |  731 | `	ph7_class_attr *pShadow;` |
|      27 |  732 | `	if( PH7_VmClassAttrAccess(&(*pVm),pClass,pAttr,FALSE) ){` |
|      20 |  733 | `		return 1;` |
|       - |  734 | `	}` |
|       8 |  735 | `	if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE \|\| pName->nByte < 1 ){` |
|     ! 0 |  736 | `		return 0;` |
|       - |  737 | `	}` |
|       8 |  738 | `	pScope = PH7_VmCallerScope(&(*pVm));` |
|       8 |  739 | `	if( pScope == 0 \|\| !PH7_VmInstanceOf(pClass,pScope) ){` |
|       8 |  740 | `		return 0;` |
|       - |  741 | `	}` |
|     ! 0 |  742 | `	pShadow = PH7_ClassExtractAttribute(pScope,pName->zString,pName->nByte);` |
|     ! 0 |  743 | `	return pShadow != 0` |
|     ! 0 |  744 | `		&& pShadow != pAttr` |
|     ! 0 |  745 | `		&& pShadow->iProtection == PH7_CLASS_PROT_PRIVATE` |
|     ! 0 |  746 | `		&& PH7_VmMemberOwnerClass(pShadow->pDeclClass,pScope) == pScope;` |
|      15 |  747 | `}` |
|       - |  748 | `/*` |
|       - |  749 | ` * OP_MEMBER: body moved verbatim from the OP_MEMBER arm of` |
|       - |  750 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  751 | ` */` |
|  390400 |  752 | `PH7_PRIVATE VmOpRc VmExecOpMember(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  753 | `{` |
|  390405 |  754 | `	ph7_value *pTos = pState->pTos;` |
|  390405 |  755 | `	ph7_value *pStack = pState->pStack;` |
|  390405 |  756 | `	VmInstr *aInstr = pState->aInstr;` |
|  390405 |  757 | `	sxi32 pc = pState->pc;` |
|       - |  758 | `	sxi32 rc;` |
|       - |  759 | `	ph7_class_instance *pThis;` |
|       - |  760 | `	ph7_value *pNos;` |
|       - |  761 | `	SyString sName;` |
|  390405 |  762 | ``	int bStaticHidden = 0; /* a `::$name` already refused for VISIBILITY */`` |
|  390405 |  763 | `	if( !pInstr->iP1 ){` |
|  282452 |  764 | `		pNos = &pTos[-1];` |
|       - |  765 | `#ifdef UNTRUST` |
|       - |  766 | `		if( pNos < pStack ){` |
|       - |  767 | `			VM_EXIT_ABORT;` |
|       - |  768 | `		}` |
|       - |  769 | `#endif` |
|       - |  770 | `		/* What a dynamic NAME may BE, decided before the receiver is looked at --` |
|       - |  771 | `		 * php settles this in the opcode handler's first lines, and the two name` |
|       - |  772 | `		 * positions settle it differently.` |
|       - |  773 | `		 *` |
|       - |  774 | `		 * A METHOD name must ALREADY be a string: zend's INIT_METHOD_CALL refuses` |
|       - |  775 | ``		 * anything else outright with `Method name must be a string`, before the`` |
|       - |  776 | `		 * receiver's type, before __call, and before the name would be looked up.` |
|       - |  777 | `		 * A PROPERTY name is COERCED instead, with the user-visible rules -- an` |
|       - |  778 | ``		 * array warns `Array to string conversion` and renders "Array", a float,`` |
|       - |  779 | `		 * bool or null spells itself out, an object hands over its __toString()` |
|       - |  780 | ``		 * and one without it is php's catchable `Object of class X could not be`` |
|       - |  781 | ``		 * converted to string`.`` |
|       - |  782 | `		 *` |
|       - |  783 | `		 * PHL read the name slot's RAW BLOB, which is empty for every value that` |
|       - |  784 | ``		 * is not already a string, so `$o->{5}`, `$o->{1.5}`, `$o->{true}` and`` |
|       - |  785 | ``		 * `$o->{$stringable}` all named the property "" -- one shared property per`` |
|       - |  786 | `		 * object, silently, on every access shape (read, write, isset, unset,` |
|       - |  787 | ``		 * increment, by-ref) -- and `$o->{[1]}` on an object with no such property`` |
|       - |  788 | `		 * SEGFAULTED, because an empty blob hands out a NULL pointer that the` |
|       - |  789 | `		 * dynamic-property path dereferences.` |
|       - |  790 | `		 *` |
|       - |  791 | `		 * php's own exception is the one shape that answers before it ever asks` |
|       - |  792 | ``		 * for the name: a lookup (isset/empty/`??`) or an unset() whose receiver`` |
|       - |  793 | `		 * is not an object short-circuits, so no coercion and no diagnostic. A` |
|       - |  794 | ``		 * `?->` on null never reaches here at all -- OP_NULLSAFE_JMP has already`` |
|       - |  795 | `		 * jumped past both the name expression and this op.` |
|       - |  796 | `		 */` |
|  282452 |  797 | `		if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|  159005 |  798 | `			if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  799 | `				sxi32 rcMn;` |
|      11 |  800 | `				VmPopOperand(&pTos,1);` |
|      11 |  801 | `				PH7_MemObjRelease(pTos);` |
|      11 |  802 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      11 |  803 | `				pTos->nIdx = SXU32_HIGH;` |
|      11 |  804 | `				rcMn = VmThrowFromVm(&(*pVm),"Error","Method name must be a string",` |
|       - |  805 | `					sizeof("Method name must be a string")-1);` |
|      11 |  806 | `				if( rcMn == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 |  807 | `				rc = rcMn;` |
|      11 |  808 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  809 | `			}` |
|  202902 |  810 | `		}else if( (pNos->iFlags & MEMOBJ_OBJ)` |
|   61809 |  811 | `		       \|\| (pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2)) ){` |
|  123418 |  812 | `			sxi32 rcNm = PH7_MemObjToStringUV(pTos);` |
|  123418 |  813 | `			if( rcNm != SXRET_OK ){` |
|       3 |  814 | `				VmPopOperand(&pTos,1);` |
|       3 |  815 | `				PH7_MemObjRelease(pTos);` |
|       3 |  816 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       3 |  817 | `				pTos->nIdx = SXU32_HIGH;` |
|       3 |  818 | `				if( rcNm == PH7_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  819 | `				rc = rcNm;` |
|       3 |  820 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  821 | `			}` |
|   61706 |  822 | `		}` |
|  282435 |  823 | `		if( pInstr->iP2 == PH7_MEMBER_DEFPATH` |
|  143918 |  824 | `		 && (pNos->iFlags & (MEMOBJ_AUX_DEFPATH\|MEMOBJ_AUX_DEFERRED)) ){` |
|       - |  825 | `			/* D1 commit 2: deferred property arg ($o->p) whose base is itself a deferred lvalue` |
|       - |  826 | ``			 * (a nested chain `$a["k"]->p`, or an undefined base object). Append a property step`` |
|       - |  827 | `			 * to the base's captured path; OP_CALL re-walks it. */` |
|       - |  828 | `			SyString sProp;` |
|      21 |  829 | `			VmDeferredPath *pPath = 0;` |
|      21 |  830 | `			SyStringInitFromBuf(&sProp,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|      21 |  831 | `			if( pNos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|      19 |  832 | `				pPath = (VmDeferredPath *)pNos->x.pOther;` |
|      19 |  833 | `				VmDeferPathPushProp(pPath,&sProp);` |
|      10 |  834 | `			}else{` |
|       - |  835 | `				SyString sRootName;` |
|       3 |  836 | `				SyStringInitFromBuf(&sRootName,(const char *)pNos->x.pOther,` |
|       - |  837 | `					pNos->x.pOther ? SyStrlen((const char *)pNos->x.pOther) : 0);` |
|       3 |  838 | `				pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|       3 |  839 | `				if( pPath ){` |
|       3 |  840 | `					pNos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name */` |
|       3 |  841 | `					pNos->x.pOther = pPath;` |
|       3 |  842 | `					pNos->iFlags \|= MEMOBJ_AUX_DEFPATH;` |
|       3 |  843 | `					pNos->nIdx = SXU32_HIGH;` |
|       3 |  844 | `					VmDeferPathPushProp(pPath,&sProp);` |
|       1 |  845 | `				}` |
|       - |  846 | `			}` |
|      21 |  847 | `			VmPopOperand(&pTos,1); /* drop the property name; the carrier stays as pTos */` |
|      21 |  848 | `			VM_EXIT_BREAK;` |
|       - |  849 | `		}` |
|  282420 |  850 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - |  851 | `			ph7_class *pClass;` |
|       - |  852 | `			/* Class already instantiated */` |
|  282256 |  853 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       - |  854 | `			/* Point to the instantiated class */` |
|  282256 |  855 | `			pClass = pThis->pClass;` |
|       - |  856 | `			/* Extract attribute name first */` |
|  282256 |  857 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|  282256 |  858 | `			if( PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
|       - |  859 | `				/* __PHP_Incomplete_Class: every property access and method call is` |
|       - |  860 | `				 * php's incomplete-object diagnostic — the carrier's OWN entries are` |
|       - |  861 | `				 * for the engine's surfaces only. A READ (isset/empty/?? included)` |
|       - |  862 | `				 * is an E_WARNING answering NULL; every WRITE shape and unset() is` |
|       - |  863 | `				 * a catchable Error; a method call is its own Error, raised where` |
|       - |  864 | `				 * the undefined-method twin raises (before any argument runs). The` |
|       - |  865 | `				 * DEFPATH pre-pass defers like a MISSING property would — the slot` |
|       - |  866 | `				 * fast path must not hand out the carrier's storage — so the by-ref` |
|       - |  867 | `				 * resolve (VmBindPropByRef) raises the Error and the by-value` |
|       - |  868 | `				 * re-drive lands back here in READ context for the warning.` |
|       - |  869 | ``				 * `??=` reads before it refuses, so it takes BOTH, php's order. */`` |
|      37 |  870 | `				VmInstr *pIncNext = pInstr + 1;` |
|      37 |  871 | `				if( pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|       - |  872 | `					SyString sIncProp;` |
|       5 |  873 | `					VmDeferredPath *pIncPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|       5 |  874 | `					SyStringInitFromBuf(&sIncProp,sName.zString,sName.nByte);` |
|       5 |  875 | `					if( pIncPath && VmDeferPathPushProp(pIncPath,&sIncProp) == SXRET_OK ){` |
|       5 |  876 | `						VmPopOperand(&pTos,1);       /* drop the property name */` |
|       5 |  877 | `						pThis->iRef++;` |
|       5 |  878 | `						PH7_MemObjRelease(pTos);     /* collapse the object slot into the carrier */` |
|       5 |  879 | `						pTos->x.pOther = pIncPath;` |
|       5 |  880 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|       5 |  881 | `						pTos->nIdx = SXU32_HIGH;` |
|       5 |  882 | `						PH7_ClassInstanceUnref(pThis);` |
|       5 |  883 | `						VM_EXIT_BREAK;` |
|       - |  884 | `					}` |
|     ! 0 |  885 | `					if( pIncPath ){` |
|     ! 0 |  886 | `						VmFreeDeferredPath(pIncPath);` |
|     ! 0 |  887 | `					}` |
|       - |  888 | `					/* allocation failure (or a temporary base below): the read` |
|       - |  889 | `					 * verdict is all that is left — fall through to warn + NULL. */` |
|     ! 0 |  890 | `				}` |
|      33 |  891 | `				int bIncCall = (pInstr->iP2 == PH7_MEMBER_METHOD);` |
|      33 |  892 | `				int bIncCoalW = (pInstr->iP2 == PH7_MEMBER_WRITE && pIncNext->iOp == PH7_OP_NULLC_JMP);` |
|      65 |  893 | `				int bIncModify = pInstr->iP2 != PH7_MEMBER_DEFPATH` |
|      63 |  894 | `					&& (pInstr->iP2 == PH7_MEMBER_UNSET` |
|      31 |  895 | `					 \|\| pInstr->iP2 == PH7_MEMBER_WRITE` |
|      25 |  896 | `					 \|\| pInstr->iP2 == PH7_MEMBER_REF_TARGET` |
|      19 |  897 | `					 \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|      18 |  898 | `					 \|\| VmMemberNextIsWrite(pIncNext)` |
|      18 |  899 | `					 \|\| VmMemberFetchForWrite(pInstr));` |
|      33 |  900 | `				if( bIncCall ){` |
|       - |  901 | `					SyBlob sIncErr;` |
|       - |  902 | `					sxi32 rcInc;` |
|       3 |  903 | `					SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|       3 |  904 | `					PH7_VmIncompleteMsg(&(*pVm),pThis,"call a method",&sIncErr);` |
|       3 |  905 | `					VmPopOperand(&pTos,1);` |
|       3 |  906 | `					PH7_MemObjRelease(pTos);` |
|       3 |  907 | `					pTos->nIdx = SXU32_HIGH;` |
|       4 |  908 | `					rcInc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sIncErr),` |
|       1 |  909 | `						SyBlobLength(&sIncErr));` |
|       3 |  910 | `					SyBlobRelease(&sIncErr);` |
|       3 |  911 | `					if( rcInc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  912 | `					rc = rcInc;` |
|       3 |  913 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  914 | `				}` |
|      31 |  915 | `				if( bIncCoalW ){` |
|       3 |  916 | `					PH7_VmIncompleteAccessWarn(&(*pVm),pThis,0);` |
|       1 |  917 | `				}` |
|      31 |  918 | `				if( bIncModify ){` |
|       - |  919 | `					SyBlob sIncErr;` |
|      17 |  920 | `					SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|      17 |  921 | `					PH7_VmIncompleteMsg(&(*pVm),pThis,"modify a property",&sIncErr);` |
|      17 |  922 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sIncErr));` |
|       9 |  923 | `				}else{` |
|      15 |  924 | `					PH7_VmIncompleteAccessWarn(&(*pVm),pThis,0);` |
|       - |  925 | `				}` |
|      31 |  926 | `				VmPopOperand(&pTos,1);   /* pop the attribute name */` |
|      31 |  927 | `				PH7_MemObjRelease(pTos); /* the object slot becomes the NULL answer */` |
|      31 |  928 | `				pTos->nIdx = SXU32_HIGH;` |
|      31 |  929 | `				VM_EXIT_BREAK;` |
|       - |  930 | `			}` |
|  282220 |  931 | `			if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - |  932 | `				/* Method call */` |
|  158979 |  933 | `				ph7_class_method *pMeth = 0;` |
|  158979 |  934 | `				int bFwdCall = 0; /* re-targeted to a dual iterator's inner: no scope check */` |
|  158979 |  935 | `				if( sName.nByte > 0 ){` |
|       - |  936 | `					/* Extract the target method */` |
|  158979 |  937 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|   79442 |  938 | `				}` |
|  158979 |  939 | `				if( pMeth == 0 ){` |
|       - |  940 | `					/* php's dual iterators forward an unknown method to the one they` |
|       - |  941 | `					 * WRAP (spl_dual_it_call_method), ahead of __call and of the` |
|       - |  942 | `					 * undefined-method Error. Re-target the receiver slot the call` |
|       - |  943 | ``					 * below binds `$this` from and carry on down the ordinary path, so`` |
|       - |  944 | `					 * the visibility rules and the screened-name marking are the same` |
|       - |  945 | `					 * ones every other method call takes. */` |
|     124 |  946 | `					ph7_class_instance *pFwdInner = 0;` |
|     124 |  947 | `					ph7_class_method *pFwdMeth = 0;` |
|     124 |  948 | `					if( PH7_SplOuterForward(&(*pVm),pThis,&sName,&pFwdInner,&pFwdMeth) ){` |
|      25 |  949 | `						ph7_class_instance *pOuter = pThis;` |
|      25 |  950 | `						pFwdInner->iRef++;      /* the stack slot's new reference */` |
|      25 |  951 | `						pNos->x.pOther = pFwdInner;` |
|      25 |  952 | `						PH7_ClassInstanceUnref(pOuter); /* ...and drop its old one */` |
|      25 |  953 | `						pThis = pFwdInner;` |
|      25 |  954 | `						pClass = pFwdInner->pClass;` |
|      25 |  955 | `						pMeth = pFwdMeth;` |
|       - |  956 | `						/* php's forward is a direct zend_call_method with no calling` |
|       - |  957 | `						 * scope, so the inner's own visibility does not apply: a` |
|       - |  958 | `						 * PROTECTED method on the wrapped iterator answers. */` |
|      25 |  959 | `						bFwdCall = 1;` |
|      12 |  960 | `					}` |
|      60 |  961 | `				}` |
|  158979 |  962 | `				if( pMeth == 0 ){` |
|     100 |  963 | `					ph7_class_method *pCallMagic = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|     100 |  964 | `					if( pCallMagic ){` |
|       - |  965 | `						/* php: a missing method dispatches __call($name, $args) — the` |
|       - |  966 | `						 * args are only collected by the following OP_CALL, so stash the` |
|       - |  967 | `						 * receiver + class + original name and MARK the callee slot` |
|       - |  968 | `						 * (band A #3b; pre-fix the name-only call discarded everything` |
|       - |  969 | `						 * and the call site failed with "Invalid function name"). Stack:` |
|       - |  970 | `						 * pop the method name, then the receiver slot becomes the marked` |
|       - |  971 | `						 * carrier OP_CALL routes through the packing body. */` |
|      58 |  972 | `						VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pThis,pClass,&sName);` |
|      58 |  973 | `						if( pPend == 0 ){` |
|     ! 0 |  974 | `							VM_EXIT_ABORT;` |
|       - |  975 | `						}` |
|      58 |  976 | `						VmPopOperand(&pTos,1);` |
|      58 |  977 | `						PH7_MemObjRelease(pTos);` |
|      58 |  978 | `						pTos->x.pOther = pPend;` |
|      58 |  979 | `						pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|      31 |  980 | `					}else{` |
|       - |  981 | `						{` |
|       - |  982 | `							/* php raises a catchable Error here; PH7 printed a notice and CARRIED ON` |
|       - |  983 | `							 * with NULL, so the call silently produced nothing. */` |
|       - |  984 | `							SyBlob sErrM;` |
|       - |  985 | `							sxi32 rcErr;` |
|      43 |  986 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      43 |  987 | `							SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",&pClass->sDisp,&sName);` |
|      43 |  988 | `							VmPopOperand(&pTos,1);` |
|      43 |  989 | `							PH7_MemObjRelease(pTos);` |
|      43 |  990 | `							pTos->nIdx = SXU32_HIGH;` |
|      64 |  991 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      21 |  992 | `								SyBlobLength(&sErrM));` |
|      43 |  993 | `							SyBlobRelease(&sErrM);` |
|      43 |  994 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      43 |  995 | `							rc = rcErr;` |
|      43 |  996 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  997 | `						}` |
|       - |  998 | `					}` |
|      31 |  999 | `				}else{` |
|  158883 | 1000 | `					ph7_class_method *pDeniedCall = 0;` |
|  158883 | 1001 | `					int bDenied = 0;` |
|       - | 1002 | `					/* php decides a method's visibility against the class that OWNS it,` |
|       - | 1003 | `					 * which for a trait method is the class that composed it — a trait is` |
|       - | 1004 | `					 * a compile-time construct php has flattened away by now. Deciding` |
|       - | 1005 | `					 * against the trait instead made every rule a question of who USES it:` |
|       - | 1006 | `					 * a protected trait method was denied to a SUBCLASS of the composing` |
|       - | 1007 | `					 * class (not a trait user itself) and granted to an unrelated class` |
|       - | 1008 | `					 * that happened to use the same trait. */` |
|  158883 | 1009 | `					ph7_class *pOwner = 0;` |
|  158878 | 1010 | `					if( !bFwdCall && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC` |
|   79452 | 1011 | `					 && !PH7_VmClassMemberAccess(&(*pVm),` |
|     130 | 1012 | `						(pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth)),` |
|      65 | 1013 | `						&sName,pMeth->iProtection,FALSE) ){` |
|      57 | 1014 | `						int bRebound = 0;` |
|      57 | 1015 | `						if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - | 1016 | `							/* php: when the instance's class SHADOWS the caller's` |
|       - | 1017 | `							 * private method (child redeclares private m), code in` |
|       - | 1018 | `							 * the caller's class dispatches its OWN private m, not` |
|       - | 1019 | `							 * the shadow. Rebind to the calling scope's method when` |
|       - | 1020 | `							 * that scope declares a private one of this name. */` |
|      49 | 1021 | `							VmFrame *pFrameLocal = VmSkipExceptionFrames(pVm->pFrame);` |
|      49 | 1022 | `							ph7_vm_func *pCallerFunc = (ph7_vm_func *)pFrameLocal->pUserData;` |
|      49 | 1023 | `							if( pCallerFunc && (pCallerFunc->iFlags & VM_FUNC_CLASS_METHOD) && pCallerFunc->pUserData ){` |
|       6 | 1024 | `								ph7_class *pScope = (ph7_class *)pCallerFunc->pUserData;` |
|       6 | 1025 | `								ph7_class_method *pOwn = PH7_ClassExtractMethod(pScope,sName.zString,sName.nByte);` |
|       4 | 1026 | `								if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       6 | 1027 | `								 && (ph7_class *)pOwn->sFunc.pUserData == pScope ){` |
|       6 | 1028 | `									pMeth = pOwn;` |
|       6 | 1029 | `									bRebound = 1;` |
|       2 | 1030 | `								}` |
|       2 | 1031 | `							}` |
|      23 | 1032 | `						}` |
|      57 | 1033 | `						if( !bRebound ){` |
|       - | 1034 | `							/* Inaccessible from this scope: php routes through __call when` |
|       - | 1035 | `							 * declared (band A #3b); without it the refusal is raised right` |
|       - | 1036 | `							 * here, beside the undefined-method and abstract-method twins. */` |
|      53 | 1037 | `							pDeniedCall = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|      53 | 1038 | `							bDenied = pDeniedCall == 0;` |
|      25 | 1039 | `						}` |
|      27 | 1040 | `					}` |
|  158883 | 1041 | `					if( bDenied ){` |
|       - | 1042 | ``						/* php's visibility Error for `$o->m()`. It is raised HERE, at the`` |
|       - | 1043 | `						 * member resolution, and not left to OP_CALL: the method OP_CALL` |
|       - | 1044 | `						 * would re-derive is looked up by the FUNCTION's own name against` |
|       - | 1045 | `						 * its declaring class, which a trait adaptation splits away from` |
|       - | 1046 | ``						 * the entry this lookup actually chose — `hi as private pHi` gave`` |
|       - | 1047 | ``						 * OP_CALL the trait's public `hi`, so the private alias ran from`` |
|       - | 1048 | `						 * global scope in silence. The entry that answered is right here,` |
|       - | 1049 | `						 * with its composed protection.` |
|       - | 1050 | `						 *` |
|       - | 1051 | ``						 * php names the method as the CALL SPELLS it (`$o->PQ()` on a`` |
|       - | 1052 | ``						 * private `pQ()` reports `PQ`) and the class that OWNS the method,`` |
|       - | 1053 | `						 * which for a trait method is the composing class. */` |
|       - | 1054 | `						SyBlob sErrM;` |
|       - | 1055 | `						sxi32 rcErr;` |
|      43 | 1056 | `						ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|      63 | 1057 | `						const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      20 | 1058 | `							? "private" : "protected";` |
|      43 | 1059 | `						SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      43 | 1060 | `						if( pScope ){` |
|       7 | 1061 | `							SyBlobFormat(&sErrM,"Call to %s method %z::%z() from scope %z",` |
|       3 | 1062 | `								zVis,&pOwner->sDisp,&sName,&pScope->sDisp);` |
|       4 | 1063 | `						}else{` |
|      37 | 1064 | `							SyBlobFormat(&sErrM,"Call to %s method %z::%z() from global scope",` |
|      17 | 1065 | `								zVis,&pOwner->sDisp,&sName);` |
|       - | 1066 | `						}` |
|      43 | 1067 | `						VmPopOperand(&pTos,1);` |
|      43 | 1068 | `						PH7_MemObjRelease(pTos);` |
|      43 | 1069 | `						pTos->nIdx = SXU32_HIGH;` |
|      63 | 1070 | `						rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      20 | 1071 | `							SyBlobLength(&sErrM));` |
|      43 | 1072 | `						SyBlobRelease(&sErrM);` |
|      43 | 1073 | `						if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      43 | 1074 | `						rc = rcErr;` |
|      47 | 1075 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1076 | `					}` |
|  158843 | 1077 | `					if( pDeniedCall ){` |
|      11 | 1078 | `						VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pThis,pClass,&sName);` |
|      11 | 1079 | `						if( pPend == 0 ){` |
|     ! 0 | 1080 | `							VM_EXIT_ABORT;` |
|       - | 1081 | `						}` |
|      11 | 1082 | `						VmPopOperand(&pTos,1);` |
|      11 | 1083 | `						PH7_MemObjRelease(pTos);` |
|      11 | 1084 | `						pTos->x.pOther = pPend;` |
|      11 | 1085 | `						pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|       6 | 1086 | `					}else{` |
|       - | 1087 | `						/* Push method name on the stack, MARKED as already screened: the` |
|       - | 1088 | `						 * decision above was made against the entry this lookup chose,` |
|       - | 1089 | `						 * which is the only place a trait adaptation's composed` |
|       - | 1090 | `						 * protection is visible. */` |
|  158833 | 1091 | `						PH7_MemObjRelease(pTos);` |
|  158833 | 1092 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|  158833 | 1093 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|  158833 | 1094 | `						pTos->iFlags \|= MEMOBJ_AUX_MEMBERCALL;` |
|       - | 1095 | `					}` |
|       - | 1096 | `				}` |
|  158897 | 1097 | `				pTos->nIdx = SXU32_HIGH;` |
|   79406 | 1098 | `			}else{` |
|       - | 1099 | `				/* Attribute access. iP2: 0 = read, 2 = unset, 3 = isset, 4 = empty. */` |
|  123246 | 1100 | `				VmClassAttr *pObjAttr = 0;` |
|  123246 | 1101 | `				SyHashEntry *pEntry = 0;` |
|       - | 1102 | `				/* A LAZY native property read before anything installed it, whose` |
|       - | 1103 | `				 * class answers such a read from its zeroed struct rather than` |
|       - | 1104 | `				 * calling the name undefined (PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT).` |
|       - | 1105 | `				 * Set by the miss handling below and answered after the pop. */` |
|  123246 | 1106 | `				ph7_class_attr *pLazyDefault = 0;` |
|       - | 1107 | `				/* An UNINITIALIZED typed slot handed to the deferred call-argument rail` |
|       - | 1108 | `				 * below, which records it whether or not the class declares __get: php` |
|       - | 1109 | `				 * consults no magic accessor for a typed property that was never written` |
|       - | 1110 | `				 * (only an unset() one reaches __get there), and the two diagnostics it` |
|       - | 1111 | `				 * DOES have -- one for a by-value binding, one for a by-reference one --` |
|       - | 1112 | `				 * are exactly what the rail exists to tell apart. */` |
|  123246 | 1113 | `				int bDeferUninit = 0;` |
|  123241 | 1114 | `				if( sName.nByte > 0 && sName.zString[0] == 0` |
|   61634 | 1115 | `				 && !PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
|       - | 1116 | `					/* php refuses a property name beginning with a NUL outright:` |
|       - | 1117 | ``					 * `Cannot access property starting with "\0"`. Those bytes are`` |
|       - | 1118 | `					 * the ENGINE's private mangling — "\0Cls\0p" is C1::$p and` |
|       - | 1119 | `					 * "\0*\0p" is a protected slot — so a script that could write one` |
|       - | 1120 | `					 * would forge a private member of any class it names, and every` |
|       - | 1121 | `					 * display surface would then render the forgery as the real thing.` |
|       - | 1122 | `					 * Two rules ride with it, both probe-verified: a MAGIC accessor` |
|       - | 1123 | `					 * wins (php dispatches __get/__set/__isset/__unset with the raw` |
|       - | 1124 | `					 * name and says nothing), and a LOOKUP context is silent — isset()` |
|       - | 1125 | ``					 * is false, empty() true, `??` takes the default — which is what`` |
|       - | 1126 | `					 * the existing miss handling below already answers. The refusal` |
|       - | 1127 | `					 * does not depend on whether such a property exists: php raises it` |
|       - | 1128 | `					 * on the NAME, before any lookup, and the one place a NUL-keyed` |
|       - | 1129 | `					 * property legitimately lives is the __PHP_Incomplete_Class` |
|       - | 1130 | `					 * carrier, whose own gate above has already answered for it.` |
|       - | 1131 | ``					 * A METHOD name is not covered — php lets `$o->{"\0m"}()` reach`` |
|       - | 1132 | `					 * the ordinary undefined-method Error — so this sits in the` |
|       - | 1133 | `					 * attribute branch only. */` |
|       - | 1134 | `					const char *zNulMagic;` |
|      51 | 1135 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       5 | 1136 | `						zNulMagic = "__unset";` |
|      49 | 1137 | `					}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|      46 | 1138 | `					       \|\| VmMemberNextIsWrite(pInstr + 1) ){` |
|      19 | 1139 | `						zNulMagic = "__set";` |
|      10 | 1140 | `					}else{` |
|      29 | 1141 | `						zNulMagic = "__get";` |
|       - | 1142 | `					}` |
|      50 | 1143 | `					if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      47 | 1144 | `					 && PH7_ClassExtractMethod(pClass,zNulMagic,(sxu32)SyStrlen(zNulMagic)) == 0 ){` |
|       - | 1145 | `						SyBlob sNulErr;` |
|       - | 1146 | `						sxi32 rcNul;` |
|      33 | 1147 | `						SyBlobInit(&sNulErr,&pVm->sAllocator);` |
|      33 | 1148 | `						SyBlobAppend(&sNulErr,"Cannot access property starting with \"\\0\"",` |
|       - | 1149 | `							sizeof("Cannot access property starting with \"\\0\"")-1);` |
|      33 | 1150 | `						VmPopOperand(&pTos,1);   /* pop the attribute name */` |
|      33 | 1151 | `						PH7_MemObjRelease(pTos); /* the object slot becomes the NULL answer */` |
|      33 | 1152 | `						pTos->nIdx = SXU32_HIGH;` |
|      49 | 1153 | `						rcNul = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sNulErr),` |
|      16 | 1154 | `							SyBlobLength(&sNulErr));` |
|      33 | 1155 | `						SyBlobRelease(&sNulErr);` |
|      33 | 1156 | `						if( rcNul == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      33 | 1157 | `						rc = rcNul;` |
|      33 | 1158 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1159 | `					}` |
|       9 | 1160 | `				}` |
|       - | 1161 | `				/* Extract the target attribute, as the class whose code is RUNNING` |
|       - | 1162 | `				 * sees it: a scope that declares a private of this name owns a slot` |
|       - | 1163 | `				 * of its own on every instance below it (php's mangled storage name)` |
|       - | 1164 | `				 * and means THAT one, whatever the object's class holds under the` |
|       - | 1165 | `				 * plain name. The EMPTY name is a real property name in php —` |
|       - | 1166 | ``				 * `$o->{''} = 1` creates one and `$o->{''}` reads it back — and it is`` |
|       - | 1167 | `				 * the one name SyHashGet cannot answer for, so it takes a` |
|       - | 1168 | `				 * list-walking lookup inside.` |
|       - | 1169 | `				 *` |
|       - | 1170 | `				 * The name is hashed ONCE, here, and reused by both questions` |
|       - | 1171 | `				 * underneath -- the scope's private-name screen and the slot probe` |
|       - | 1172 | `				 * on this object -- because every property table shares one hash` |
|       - | 1173 | `				 * function and hashing is what a lookup spends. */` |
|  184814 | 1174 | `				pEntry = PH7_ClassInstanceScopedAttrEntry(&(*pVm),pThis,sName.zString,sName.nByte,` |
|  123209 | 1175 | `					sName.nByte > 0 ? SyHashKey(&pThis->hAttr,(const void *)sName.zString,sName.nByte) : 0);` |
|  123214 | 1176 | `				if( pEntry ){` |
|       - | 1177 | `					/* Point to the attribute value */` |
|  115307 | 1178 | `					pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|   57651 | 1179 | `				}` |
|  123209 | 1180 | `				if( pObjAttr` |
|  119251 | 1181 | `				 && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT))` |
|   57668 | 1182 | `				 && !VmStaticThroughInstanceVisible(&(*pVm),pClass,pObjAttr->pAttr,&sName) ){` |
|       - | 1183 | `					/* A private static this scope may not touch. When it is an INHERITED` |
|       - | 1184 | ``					 * one php's instance path does not see it at all: `$b->s` is an`` |
|       - | 1185 | `					 * ordinary MISSING property (warn and null, a dynamic write, a silent` |
|       - | 1186 | `					 * unset), with none of the visibility refusal the DECLARING class's` |
|       - | 1187 | `					 * own instance gets. The declaring class's own keeps the refusal. */` |
|       9 | 1188 | `					if( PH7_VmMemberOwnerClass(pObjAttr->pAttr->pDeclClass,pClass) != pClass ){` |
|       3 | 1189 | `						pEntry = 0;` |
|       3 | 1190 | `						pObjAttr = 0;` |
|       1 | 1191 | `					}` |
|  123208 | 1192 | `				}else if( pObjAttr` |
|  119250 | 1193 | `				 && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) ){` |
|       - | 1194 | `					/* A static property (or a constant) belongs to the CLASS: php does` |
|       - | 1195 | `					 * not find it through an instance at all. It notices the attempt —` |
|       - | 1196 | ``					 * `Accessing static property C::$s as non static` — and then treats`` |
|       - | 1197 | `					 * the name as an ordinary MISSING property: a read warns and answers` |
|       - | 1198 | `					 * null, isset() is false, a write goes to a dynamic property (which` |
|       - | 1199 | `					 * PHL rejects by the scope policy, like any other undeclared write).` |
|       - | 1200 | `					 * PHL's instance table carries an entry for every declared member` |
|       - | 1201 | ``					 * (statics share the class slot), so `$o->s` used to READ and — far`` |
|       - | 1202 | `					 * worse — WRITE the class's own static in silence. isset()/empty()` |
|       - | 1203 | `					 * pass silently, as php's do. */` |
|      18 | 1204 | `					if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      18 | 1205 | `					 && pInstr->iP2 != PH7_MEMBER_DEFPATH ){` |
|       - | 1206 | `						/* Silent where php is silent: isset()/empty(), and any class` |
|       - | 1207 | `						 * that declares the MAGIC accessor this context would dispatch` |
|       - | 1208 | ``						 * (php passes `silent = ce->__get/__set/__unset != NULL` to its`` |
|       - | 1209 | `						 * property-offset lookup). Once per access, too: the deferred` |
|       - | 1210 | `						 * call-argument PRE-PASS (PH7_MEMBER_DEFPATH) re-runs this op for` |
|       - | 1211 | ``						 * the same source `$o->s` and the value pass carries the notice`` |
|       - | 1212 | `						 * — the BY-REF form has no value pass and notices at its own` |
|       - | 1213 | `						 * site (VmBindPropByRef). */` |
|       - | 1214 | `						const char *zMagic;` |
|      10 | 1215 | `						if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       3 | 1216 | `							zMagic = "__unset";` |
|       8 | 1217 | `						}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|       8 | 1218 | `						       \|\| VmMemberNextIsWrite(pInstr + 1) ){` |
|       3 | 1219 | `							zMagic = "__set";` |
|       2 | 1220 | `						}else{` |
|       6 | 1221 | `							zMagic = "__get";` |
|       - | 1222 | `						}` |
|      10 | 1223 | `						if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) == 0 ){` |
|      11 | 1224 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - | 1225 | `								"Accessing static property %z::$%z as non static",` |
|       6 | 1226 | `								&pClass->sDisp,&pObjAttr->pAttr->sName);` |
|       3 | 1227 | `						}` |
|       4 | 1228 | `					}` |
|      20 | 1229 | `					pEntry = 0;` |
|      20 | 1230 | `					pObjAttr = 0;` |
|       9 | 1231 | `				}` |
|  123209 | 1232 | `				if( pObjAttr == 0 && PH7_ClassHasNativeProp(pClass)` |
|    7578 | 1233 | `				 && !VmMemberCoalOwned(pThis,pInstr,&sName) ){` |
|       - | 1234 | `					/* php's read_property / has_property / write_property /` |
|       - | 1235 | `					 * unset_property handlers, for a class whose properties are not` |
|       - | 1236 | `					 * storage at all: PDORow answers every read from the statement's` |
|       - | 1237 | `					 * current ROW, holds no slot for any of them, and refuses every` |
|       - | 1238 | `					 * write. It comes first among the miss paths, and before the` |
|       - | 1239 | `					 * declared-but-absent one: a name this class owns is neither a` |
|       - | 1240 | `					 * dynamic property to create, nor an "Undefined property" to warn` |
|       - | 1241 | `					 * about, nor a __get to dispatch.` |
|       - | 1242 | `					 *` |
|       - | 1243 | `					 * Which handler php asks is what the CONTEXT says: a plain store,` |
|       - | 1244 | `					 * a destructuring target and a read-modify-write all end in a` |
|       - | 1245 | ``					 * write; `??=` reads first and writes only when the read answered`` |
|       - | 1246 | ``					 * null; a subscript-write base (`$o->p[0] = 1`) is a READ whose`` |
|       - | 1247 | `					 * value the subscript then refuses; and isset() stops at the` |
|       - | 1248 | ``					 * truth while empty() and `??` take the value. */`` |
|    7212 | 1249 | `					VmInstr *pPropNext = pInstr + 1;` |
|    7212 | 1250 | `					int bPropStore = (pPropNext->iOp == PH7_OP_STORE && pPropNext->iP2 != 0);` |
|   11211 | 1251 | `					int bPropCoal = (pInstr->iP2 == PH7_MEMBER_WRITE` |
|    7207 | 1252 | `						&& pPropNext->iOp == PH7_OP_NULLC_JMP);` |
|       - | 1253 | `					PH7_NativePropCtx sProp;` |
|       - | 1254 | `					ph7_value sPropVal;` |
|    7212 | 1255 | `					PH7_MemObjInit(&(*pVm),&sPropVal);` |
|    7212 | 1256 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|      22 | 1257 | `						sProp.iMode = PH7_NATIVE_PROP_UNSET;` |
|    7199 | 1258 | `					}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET \|\| bPropStore` |
|    6809 | 1259 | `					       \|\| VmMemberNextIsRmw(pPropNext) ){` |
|     775 | 1260 | `						sProp.iMode = PH7_NATIVE_PROP_WRITE;` |
|    6806 | 1261 | `					}else if( pInstr->iP2 == PH7_MEMBER_ISSET ){` |
|     170 | 1262 | `						sProp.iMode = PH7_NATIVE_PROP_ISSET;` |
|    6336 | 1263 | `					}else if( pInstr->iP2 == PH7_MEMBER_EMPTY ){` |
|       - | 1264 | `						/* php's empty() asks has_property with a non-zero` |
|       - | 1265 | `						 * check_empty and takes THAT for the answer -- it never` |
|       - | 1266 | `						 * reads the value. The two are not the same question:` |
|       - | 1267 | ``						 * `empty($row->queryString)` is TRUE on a name the`` |
|       - | 1268 | `						 * handler does not know, while reading it works. */` |
|      29 | 1269 | `						sProp.iMode = PH7_NATIVE_PROP_NOTEMPTY;` |
|      15 | 1270 | `					}else{` |
|    6224 | 1271 | `						sProp.iMode = PH7_NATIVE_PROP_READ;` |
|       - | 1272 | `					}` |
|    7212 | 1273 | `					sProp.pName = &sName;` |
|    7212 | 1274 | `					sProp.pResult = &sPropVal;` |
|    7212 | 1275 | `					sProp.bAnswered = 0;` |
|    7212 | 1276 | `					sProp.zThrowClass = 0;` |
|    7212 | 1277 | `					sProp.zThrowMsg[0] = 0;` |
|    7212 | 1278 | `					sProp.iThrowCode = 0;` |
|       - | 1279 | ``					/* php's `??` is its THIRD accessor level: it takes the property's`` |
|       - | 1280 | `					 * VALUE and says nothing about a name that is not there. */` |
|    7212 | 1281 | `					sProp.bQuiet = (pInstr->iP2 == PH7_MEMBER_COALESCE);` |
|       - | 1282 | `					/* ...and a fetch in WRITE context is php's get_property_ptr_ptr,` |
|       - | 1283 | `					 * which a handler whose property is a real element answers with` |
|       - | 1284 | ``					 * the element itself -- so `$ao->list[] = 1` lands in the store`` |
|       - | 1285 | `					 * rather than in a temporary, and creates the key it is missing.` |
|       - | 1286 | ``					 * A reference SOURCE asks the same handler: `$r =& $ao->z` creates`` |
|       - | 1287 | `					 * the key and binds to it, where a VIRTUAL property (no slot to` |
|       - | 1288 | `					 * give) ignores the flag and hands back its value copy. */` |
|   10419 | 1289 | `					sProp.bWriteCtx = VmMemberNativeSetKeepsSlot(pInstr)` |
|    7207 | 1290 | `						\|\| VmMemberFetchIsRefSource(pInstr);` |
|    7212 | 1291 | `					sProp.nSlot = SXU32_HIGH;` |
|    7212 | 1292 | `					if( PH7_ClassNativeProp(pThis,&sProp) ){` |
|    6325 | 1293 | `						if( sProp.zThrowClass == 0 && bPropCoal` |
|    3140 | 1294 | `						 && sProp.iMode == PH7_NATIVE_PROP_READ` |
|       9 | 1295 | `						 && (sPropVal.iFlags & MEMOBJ_NULL) ){` |
|       - | 1296 | ``							/* `$o->p ??= v` on a name that reads null: the store php`` |
|       - | 1297 | `							 * skips for a non-null one is the one it now makes, so the` |
|       - | 1298 | `							 * refusal is the WRITE handler's. */` |
|       3 | 1299 | `							sProp.iMode = PH7_NATIVE_PROP_WRITE;` |
|       3 | 1300 | `							sProp.bAnswered = 0;` |
|       3 | 1301 | `							PH7_ClassNativeProp(pThis,&sProp);` |
|       1 | 1302 | `						}` |
|    6330 | 1303 | `						if( sProp.zThrowClass ){` |
|       - | 1304 | `							/* Parked, like every other refusal this op makes: the` |
|       - | 1305 | `							 * fetch-point router lands it and abandons this slot. */` |
|      79 | 1306 | `							VmBoundaryPark(&(*pVm),` |
|      52 | 1307 | `								VmThrowFixedErrorCode(&(*pVm),sProp.zThrowClass,` |
|      26 | 1308 | `									sProp.iThrowCode,sProp.zThrowMsg));` |
|      53 | 1309 | `							PH7_MemObjRelease(&sPropVal);` |
|      53 | 1310 | `							VmPopOperand(&pTos,1);      /* the property name */` |
|      53 | 1311 | `							PH7_MemObjRelease(pTos);    /* the object slot is the answer */` |
|      53 | 1312 | `							pTos->nIdx = SXU32_HIGH;` |
|    3189 | 1313 | `							VM_EXIT_BREAK;` |
|       - | 1314 | `						}` |
|    6278 | 1315 | `						VmPopOperand(&pTos,1);          /* the property name */` |
|    6278 | 1316 | `						pThis->iRef++;` |
|    6278 | 1317 | `						PH7_MemObjRelease(pTos);` |
|    6278 | 1318 | `						if( sProp.iMode == PH7_NATIVE_PROP_ISSET ){` |
|       - | 1319 | `							/* isset() only tests null-ness: a non-null marker for` |
|       - | 1320 | `							 * true, NULL for false — what the __isset arm pushes. */` |
|     118 | 1321 | `							if( ph7_value_to_bool(&sPropVal) ){` |
|      78 | 1322 | `								pTos->x.iVal = 1;` |
|      78 | 1323 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      40 | 1324 | `							}` |
|    6220 | 1325 | `						}else if( sProp.iMode != PH7_NATIVE_PROP_UNSET ){` |
|       - | 1326 | `							/* EMPTY pushes the handler's BOOL, which the op then` |
|       - | 1327 | `							 * judges for emptiness -- the same answer php takes. */` |
|    6152 | 1328 | `							PH7_MemObjStore(&sPropVal,pTos);` |
|    3074 | 1329 | `						}` |
|       - | 1330 | `						/* SXU32_HIGH unless the handler handed back a real element's` |
|       - | 1331 | `						 * slot: a virtual property is a VALUE and never an lvalue,` |
|       - | 1332 | `						 * while ArrayObject's storage element is the thing itself. */` |
|    6278 | 1333 | `						pTos->nIdx = sProp.nSlot;` |
|    6278 | 1334 | `						PH7_MemObjRelease(&sPropVal);` |
|    6278 | 1335 | `						PH7_ClassInstanceUnref(pThis);` |
|    6278 | 1336 | `						VM_EXIT_BREAK;` |
|       - | 1337 | `					}` |
|     885 | 1338 | `					PH7_MemObjRelease(&sPropVal);` |
|     441 | 1339 | `				}` |
|  116889 | 1340 | `				if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - | 1341 | `					/* unset($o->prop): remove the property entirely so it disappears from` |
|       - | 1342 | `					 * foreach / json_encode / get_object_vars / (array) — matching PHP (a value-only` |
|       - | 1343 | `					 * release would leave a zombie null entry). Leave a NULL constant on the stack so` |
|       - | 1344 | `					 * the trailing generic unset() builtin is a no-op (mirrors LOAD_IDX iP2=5).` |
|       - | 1345 | `					 * php dispatches __unset($name) for a MISSING or INACCESSIBLE property` |
|       - | 1346 | `					 * (band A #3b — pre-fix an inaccessible private was silently DELETED from` |
|       - | 1347 | `					 * outside the class); without __unset, an inaccessible unset is php's` |
|       - | 1348 | `					 * catchable "Cannot access ..." Error and a missing one stays a no-op. */` |
|     165 | 1349 | `					int bUnsAccessible = pEntry ? PH7_VmClassAttrAccess(&(*pVm),pClass,pObjAttr->pAttr,FALSE) : 0;` |
|     235 | 1350 | `					ph7_class_attr *pUnsNoWrite = pEntry ? pObjAttr->pAttr` |
|      92 | 1351 | `						: PH7_ClassScopedAttribute(&(*pVm),pClass,sName.zString,sName.nByte);` |
|     165 | 1352 | `					sxi32 rcUnsRo = SXRET_OK;` |
|     165 | 1353 | `					if( pEntry && (pObjAttr->iState & VM_CLASS_ATTR_RDONLY) != 0 ){` |
|       - | 1354 | `						/* php's read-only handler answers an unset with the same sentence` |
|       - | 1355 | ``						 * it answers a store: `Property p is read only`. */`` |
|       3 | 1356 | `						VmBoundaryPark(&(*pVm),VmThrowNativeReadOnly(&(*pVm),pObjAttr->pAttr));` |
|     164 | 1357 | `					}else if( pUnsNoWrite && (pUnsNoWrite->iFlags & (PH7_CLASS_ATTR_NATIVE_NOWRITE` |
|      74 | 1358 | `					                                                \|PH7_CLASS_ATTR_NATIVE_NOSLOT)) != 0 ){` |
|       - | 1359 | `						/* php's own unset handler for this class refuses, and words it` |
|       - | 1360 | ``						 * without either "readonly" or "property": `Cannot unset C::$p`.`` |
|       - | 1361 | `						 * It runs whether the object has a struct or not, so an` |
|       - | 1362 | `						 * unconstructed one -- which holds no slot at all -- refuses too` |
|       - | 1363 | `						 * rather than falling through to the missing-property no-op.` |
|       - | 1364 | `						 * A VIRTUAL property (NOSLOT) is the same answer for the same` |
|       - | 1365 | `						 * reason: php's unset_property handler for one has nothing to` |
|       - | 1366 | ``						 * remove, so `unset($doc->preserveWhiteSpace)` is this Error and`` |
|       - | 1367 | `						 * not the silent no-op a name the object lacks would take. */` |
|      37 | 1368 | `						VmBoundaryPark(&(*pVm),` |
|      12 | 1369 | `							VmThrowNativeNoUnset(&(*pVm),pThis->pClass,pUnsNoWrite));` |
|     151 | 1370 | `					}else if( pEntry && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) != 0 ){` |
|       - | 1371 | `						/* A native class's property is php's own C struct field, and` |
|       - | 1372 | `						 * unset() is a std handler that looks for a REAL property and` |
|       - | 1373 | `						 * finds none: nothing happens, nothing is said, and the next` |
|       - | 1374 | `						 * read still answers the struct. Removing the slot here left` |
|       - | 1375 | ``						 * `unset($i->y); $i->y` an Undefined property warning and NULL`` |
|       - | 1376 | `						 * for a statement php ignores. */` |
|     139 | 1377 | `					}else if( pEntry && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) != 0 ){` |
|       - | 1378 | `						/* php 8.4: a hooked property (virtual or backed) can never be` |
|       - | 1379 | `						 * unset — catchable Error, even from inside its own hook body` |
|       - | 1380 | `						 * (probe-verified). */` |
|       - | 1381 | `						SyBlob sErrMsg;` |
|       5 | 1382 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       5 | 1383 | `						SyBlobFormat(&sErrMsg,"Cannot unset hooked property %z::$%z",` |
|       4 | 1384 | `							&pThis->pClass->sDisp,&pObjAttr->pAttr->sName);` |
|       5 | 1385 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     131 | 1386 | `					}else if( pEntry && bUnsAccessible` |
|     112 | 1387 | `					       && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0` |
|      70 | 1388 | `					       && (rcUnsRo = VmCheckReadonlyUnset(&(*pVm),pThis->pClass,pObjAttr)) != SXRET_OK ){` |
|       - | 1389 | `						/* php refuses to destroy a readonly property: an initialized` |
|       - | 1390 | `						 * one from every scope, and an uninitialized one from a scope` |
|       - | 1391 | `						 * that may not write it. Deleting it here re-armed the` |
|       - | 1392 | ``						 * write-once latch, so a `readonly` value could be replaced by`` |
|       - | 1393 | `						 * anything in two statements. The refusal is thrown inside the` |
|       - | 1394 | `						 * check; parking it is what routes it like every other one` |
|       - | 1395 | `						 * raised from this opcode. */` |
|      17 | 1396 | `						VmBoundaryPark(&(*pVm),rcUnsRo);` |
|     123 | 1397 | `					}else if( pEntry && bUnsAccessible ){` |
|      94 | 1398 | `						if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|      60 | 1399 | `						 && (pObjAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){` |
|       - | 1400 | `							/* A TYPED property keeps its DECLARATION: php's unset makes` |
|       - | 1401 | `							 * it uninitialized, so var_dump still names it` |
|       - | 1402 | ``							 * `uninitialized(T)`, a read is "must not be accessed before`` |
|       - | 1403 | `							 * initialization" rather than an Undefined property, and a` |
|       - | 1404 | `							 * later write lands back in its declared position instead of` |
|       - | 1405 | `							 * appending a dynamic one at the end. The seven other` |
|       - | 1406 | `							 * presentation surfaces leave an uninitialized property out,` |
|       - | 1407 | `							 * which is what made the delete look right. */` |
|      24 | 1408 | `							ph7_value *pUnsSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|      24 | 1409 | `							if( pUnsSlot ){` |
|      24 | 1410 | `								PH7_MemObjRelease(pUnsSlot);` |
|      24 | 1411 | `								MemObjSetType(pUnsSlot,MEMOBJ_NULL);` |
|      11 | 1412 | `							}` |
|      24 | 1413 | `							pObjAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|      13 | 1414 | `						}else{` |
|      74 | 1415 | `							PH7_VmReleaseInstanceAttr(&(*pVm),pObjAttr);` |
|       - | 1416 | `` 							/* Through the instance's own door: a `foreach`/`array_walk` `` |
|       - | 1417 | `							 * standing one property short of this one is holding the` |
|       - | 1418 | `							 * entry about to be freed. */` |
|      74 | 1419 | `							PH7_ClassInstanceDeleteAttrEntry(pThis,pEntry);` |
|       - | 1420 | `						}` |
|      49 | 1421 | `					}else{` |
|      21 | 1422 | `						ph7_class_method *pUnsetMagic = PH7_ClassExtractMethod(pClass,"__unset",sizeof("__unset")-1);` |
|      21 | 1423 | `						if( pUnsetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'u') ){` |
|      13 | 1424 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'u');` |
|      13 | 1425 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__unset",sizeof("__unset")-1,&sName,0);` |
|      13 | 1426 | `							VmMagicGuardPop(pVm);` |
|      14 | 1427 | `						}else if( pEntry ){` |
|       - | 1428 | `							/* Inaccessible and no __unset: php's catchable Error. Parked on` |
|       - | 1429 | `							 * the boundary rail; the op completes benignly and the` |
|       - | 1430 | `							 * fetch-point router lands it. */` |
|       - | 1431 | `							SyBlob sErrMsg;` |
|       3 | 1432 | `							const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       3 | 1433 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 1434 | `							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",zVis,&pClass->sDisp,&sName);` |
|       3 | 1435 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       1 | 1436 | `						}` |
|       - | 1437 | `						/* Missing property without __unset: silent no-op (php). */` |
|       - | 1438 | `					}` |
|     165 | 1439 | `					VmPopOperand(&pTos,1);    /* pop the attribute name */` |
|     165 | 1440 | `					PH7_MemObjRelease(pTos);  /* release the object on the stack ($o's stack ref) */` |
|     165 | 1441 | `					pTos->nIdx = SXU32_HIGH;  /* NULL constant */` |
|     165 | 1442 | `					VM_EXIT_BREAK;` |
|       - | 1443 | `				}` |
|  116727 | 1444 | `				if( pObjAttr == 0 ){` |
|       - | 1445 | `					/* Member not present on the instance and the next instruction writes/modifies it` |
|       - | 1446 | ``					 * (store, array-append/keyed-write, `??=`, ++/--, or a compound-assign — see`` |
|       - | 1447 | `					 * VmMemberNextIsWrite; the compiler always emits a terminating PH7_OP_DONE so` |
|       - | 1448 | `					 * pInstr+1 is in-bounds). Create the property so the operation lands on a real slot` |
|       - | 1449 | ``					 * (PHP auto-vivifies a fresh property for all these forms, not just `=`):`` |
|       - | 1450 | `					 *   - a DECLARED prop that was unset() and is re-assigned → recreate it` |
|       - | 1451 | `					 *     (PHP re-appends it at the end), OR` |
|       - | 1452 | `					 *   - a dynamic prop on a dynamic-allowing class (stdClass).` |
|       - | 1453 | `					 * The created slot is NULL with a real nIdx, which the modify-op then vivifies` |
|       - | 1454 | `					 * (NULL→array for [], NULL→0/"" for ++/.=) and writes back in place.` |
|       - | 1455 | `					 * Two signals: the compiler tags the member itself iP2=PH7_MEMBER_WRITE when it is` |
|       - | 1456 | ``					 * the base of a write-subscript / `??=` (the modify-op is not the immediately-next`` |
|       - | 1457 | `					 * instruction there), and for ++/--/compound-assign/store the next opcode is the` |
|       - | 1458 | `					 * modify-op directly (VmMemberNextIsWrite). */` |
|    1585 | 1459 | `					VmInstr *pNext = pInstr + 1;` |
|       - | 1460 | ``					/* A reference SOURCE is php's write-context fetch too (`$r =& $o->p`,`` |
|       - | 1461 | ``					 * `[&$o->p]`, `foreach ($o->p as &$v)`): the name it does not find is`` |
|       - | 1462 | `					 * CREATED, silently, and the bind reaches the created slot. PHL read` |
|       - | 1463 | `					 * it, so every such source warned about what php was on the point of` |
|       - | 1464 | `					 * creating and then bound a fresh variable of its own -- a write` |
|       - | 1465 | `					 * through the reference never reached the object. */` |
|    1585 | 1466 | `					int bRefSrcMiss = VmMemberFetchIsRefSource(pInstr);` |
|    1580 | 1467 | `					if( pInstr->iP2 == PH7_MEMBER_WRITE \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|     542 | 1468 | `					 \|\| VmMemberNextIsWrite(pNext) \|\| bRefSrcMiss ){` |
|    1099 | 1469 | `						ph7_class_attr *pDecl = PH7_ClassScopedAttribute(&(*pVm),pThis->pClass,sName.zString,sName.nByte);` |
|    1099 | 1470 | `						if( pDecl && PH7_ATTR_LAZY_ABSENT(pDecl,pThis) ){` |
|       - | 1471 | `							/* The object has never held this name. A class whose handler` |
|       - | 1472 | `							 * REFUSES every write answers the same sentence with or` |
|       - | 1473 | `							 * without a struct, and nothing is created; otherwise php's` |
|       - | 1474 | `							 * own write goes to the standard handler and CREATES a` |
|       - | 1475 | `							 * dynamic property beside the struct, which PHL refuses` |
|       - | 1476 | `							 * (the scope policy) -- so fall through to the dynamic branch and let it.` |
|       - | 1477 | `` 							 * Once the constructor has installed the set, an `unset()` `` |
|       - | 1478 | `							 * and a re-write are the ordinary declared path again. */` |
|      16 | 1479 | `							if( pDecl->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|       4 | 1480 | `								VmBoundaryPark(&(*pVm),` |
|       1 | 1481 | `									VmThrowNativeNoWrite(&(*pVm),pThis->pClass,pDecl));` |
|       3 | 1482 | `								VmPopOperand(&pTos,1);    /* pop the attribute name */` |
|       3 | 1483 | `								PH7_MemObjRelease(pTos);  /* the object slot becomes the answer */` |
|       3 | 1484 | `								pTos->nIdx = SXU32_HIGH;` |
|       3 | 1485 | `								VM_EXIT_BREAK;` |
|       - | 1486 | `							}` |
|      13 | 1487 | `							pDecl = 0;` |
|       6 | 1488 | `						}` |
|    1097 | 1489 | `						if( pDecl && (pDecl->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){` |
|       - | 1490 | `							/* A VIRTUAL property: php keeps no slot to re-create, and its` |
|       - | 1491 | `							 * write goes to the class's own write_property handler, which` |
|       - | 1492 | `							 * the rails below reach through PH7_ClassNativePropOwns.` |
|       - | 1493 | `							 * Creating one would give the object a real property php has` |
|       - | 1494 | `							 * none of, and would take the read with it` |
|       - | 1495 | ``							 * (`$doc->formatOutput = false` then answered out of the slot`` |
|       - | 1496 | `							 * rather than out of the extension's state). */` |
|     708 | 1497 | `							pDecl = 0;` |
|     353 | 1498 | `						}` |
|    1097 | 1499 | `						if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      15 | 1500 | `							VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pObjAttr);` |
|       8 | 1501 | `						}else{` |
|       - | 1502 | `							/* php 8 semantics (band A #3b): a PLAIN store to a missing` |
|       - | 1503 | `							 * property dispatches __set($name,$value) when declared —` |
|       - | 1504 | `							 * the value only exists at the following OP_STORE, so park` |
|       - | 1505 | `							 * the receiver+name for it (one-instruction lifetime; the` |
|       - | 1506 | `							 * guard makes a same-name write inside __set fall through` |
|       - | 1507 | `							 * to dynamic creation, like php). Without __set — or for a` |
|       - | 1508 | `							 * subscript-write base / read-modify-write, which php does` |
|       - | 1509 | `							 * NOT route through __set — create a dynamic property on` |
|       - | 1510 | `							 * ANY class with the 8.2 deprecation (suppressed for` |
|       - | 1511 | `							 * stdClass and #[AllowDynamicProperties]); a readonly` |
|       - | 1512 | `							 * class raises php's catchable Error instead. */` |
|    1083 | 1513 | `							ph7_class_method *pSetMagic = 0;` |
|    1083 | 1514 | `							ph7_class_method *pCoalIsset = 0, *pCoalGet = 0, *pCoalSet = 0;` |
|    1083 | 1515 | `							int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);` |
|       - | 1516 | `							/* A name the class's OWN property-handler table carries is` |
|       - | 1517 | ``							 * overloaded exactly the way a `__set` class's is -- php's`` |
|       - | 1518 | ``							 * write_property stands where `__set` would -- so every rail`` |
|       - | 1519 | `							 * below takes it, and the one door they all end at` |
|       - | 1520 | `							 * (VmMagicSetDispatch) asks the handler first. */` |
|    1083 | 1521 | `							int bOwnedSet = PH7_ClassNativePropOwns(pThis,&sName);` |
|    1083 | 1522 | `							if( bPlainStore \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|     965 | 1523 | `								pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|     480 | 1524 | `							}` |
|    1083 | 1525 | `							if( (pSetMagic \|\| bOwnedSet) && pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 1526 | `								/* php dispatches __set($name, element) for a missing` |
|       - | 1527 | `								 * destructuring target, but the element value only exists` |
|       - | 1528 | `								 * at the following OP_LOAD_LIST — the dispatch is` |
|       - | 1529 | `								 * unsupported (recorded residual). Leave the miss: the` |
|       - | 1530 | `								 * property stays uncreated, matching php's observable` |
|       - | 1531 | `								 * state (its __set did not store either). */` |
|    1070 | 1532 | `							}else if( (bOwnedSet && bPlainStore)` |
|     697 | 1533 | `							 \|\| (pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s')) ){` |
|     784 | 1534 | `								pThis->iRef++;` |
|     784 | 1535 | `								pVm->pMagicSetThis = pThis;` |
|     784 | 1536 | `								SyBlobReset(&pVm->sMagicSetName);` |
|     784 | 1537 | `								SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       - | 1538 | `								/* pObjAttr stays NULL; the miss path below stays silent. */` |
|     702 | 1539 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|     105 | 1540 | `							 && pNext->iOp == PH7_OP_NULLC_JMP` |
|      53 | 1541 | `							 && (bOwnedSet` |
|      11 | 1542 | `							  \|\| (pCoalIsset = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1)) != 0` |
|       8 | 1543 | `							  \|\| (pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)) != 0` |
|       5 | 1544 | `							  \|\| (pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1)) != 0) ){` |
|       - | 1545 | ``								/* `$o->p ??= v` on a missing property with magic accessors:`` |
|       - | 1546 | `								 * php consults __isset first when declared — false means` |
|       - | 1547 | `								 * assign directly through __set with NO __get call (the` |
|       - | 1548 | `								 * test value stays null); true (or no __isset) means __get` |
|       - | 1549 | `								 * provides the test value. A COAL_MAGIC entry is pushed` |
|       - | 1550 | `								 * for the OP_NULLC_STORE at the jump target; the` |
|       - | 1551 | `								 * fetch-point sweep drops it when the short-circuit jump` |
|       - | 1552 | `								 * skips the assign or a throw abandons the RHS. Without` |
|       - | 1553 | `								 * __set the assign side keeps the pre-existing loud` |
|       - | 1554 | `								 * "Cannot perform assignment" path (php would create a` |
|       - | 1555 | `								 * dynamic property there — recorded residual). Note the` |
|       - | 1556 | `								 * condition's short-circuit assignment chain: a hit on an` |
|       - | 1557 | `								 * earlier method leaves the later pointers unresolved, so` |
|       - | 1558 | `								 * re-resolve the leftovers here (each is one hash probe,` |
|       - | 1559 | `								 * only on this ??=-miss path). */` |
|       - | 1560 | `								ph7_value sTest;` |
|       9 | 1561 | `								int bMiss = 0; /* __isset said false: skip __get, test value stays null */` |
|       9 | 1562 | `								if( pCoalIsset != 0 ){` |
|       5 | 1563 | `									pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       2 | 1564 | `								}` |
|       9 | 1565 | `								if( pCoalIsset != 0 \|\| pCoalGet != 0 ){` |
|       7 | 1566 | `									pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|       3 | 1567 | `								}` |
|       9 | 1568 | `								PH7_MemObjInit(pVm,&sTest);` |
|       9 | 1569 | `								if( pCoalIsset && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 1570 | `									ph7_value sIssetRet;` |
|       5 | 1571 | `									PH7_MemObjInit(pVm,&sIssetRet);` |
|       5 | 1572 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|       5 | 1573 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|       5 | 1574 | `									VmMagicGuardPop(pVm);` |
|       5 | 1575 | `									PH7_MemObjToBool(&sIssetRet);` |
|       5 | 1576 | `									bMiss = sIssetRet.x.iVal == 0;` |
|       5 | 1577 | `									PH7_MemObjRelease(&sIssetRet);` |
|       2 | 1578 | `								}` |
|       9 | 1579 | `								if( bOwnedSet && !bMiss ){` |
|       - | 1580 | `									/* php's ASSIGN_COALESCE fetches BP_VAR_IS, which` |
|       - | 1581 | `									 * asks has_property before read_property -- so a` |
|       - | 1582 | `									 * name the handler answers nothing for is never` |
|       - | 1583 | `									 * READ, and the assign it makes says nothing about` |
|       - | 1584 | `									 * a key that was not there. */` |
|       - | 1585 | `									ph7_value sOwnIs;` |
|       3 | 1586 | `									PH7_MemObjInit(pVm,&sOwnIs);` |
|       3 | 1587 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,` |
|       - | 1588 | `										"__isset",sizeof("__isset")-1,&sName,&sOwnIs);` |
|       3 | 1589 | `									PH7_MemObjToBool(&sOwnIs);` |
|       3 | 1590 | `									bMiss = sOwnIs.x.iVal == 0;` |
|       3 | 1591 | `									PH7_MemObjRelease(&sOwnIs);` |
|       1 | 1592 | `								}` |
|       8 | 1593 | `								if( !bMiss && (pCoalGet \|\| bOwnedSet)` |
|       5 | 1594 | `								 && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       5 | 1595 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       5 | 1596 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sTest);` |
|       5 | 1597 | `									VmMagicGuardPop(pVm);` |
|       2 | 1598 | `								}` |
|       8 | 1599 | `								if( (pCoalSet \|\| bOwnedSet)` |
|       8 | 1600 | `								 && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s')` |
|       9 | 1601 | `								 && pVm->nBoundaryRc == 0 ){` |
|       - | 1602 | `									VmHookRmw sPend;` |
|       9 | 1603 | `									sPend.iKind = VM_HOOK_PEND_COAL_MAGIC;` |
|       9 | 1604 | `									sPend.pThis = pThis;` |
|       9 | 1605 | `									sPend.pAttr = 0;` |
|       9 | 1606 | `									sPend.nBackIdx = SXU32_HIGH;` |
|       9 | 1607 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|       9 | 1608 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|       9 | 1609 | `									SyBlobAppend(&sPend.sName,(const void *)sName.zString,sName.nByte);` |
|       9 | 1610 | `									sPend.pOwnerStack = (void *)pStack;` |
|       9 | 1611 | `									sPend.pInstrs = (void *)aInstr;` |
|       9 | 1612 | `									sPend.nJmpPc = (sxu32)(pc + 1);        /* the OP_NULLC_JMP */` |
|       9 | 1613 | `									sPend.nPc = (sxu32)(pNext->iP2 - 1);   /* the OP_NULLC_STORE */` |
|       9 | 1614 | `									pThis->iRef++;` |
|       9 | 1615 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|       4 | 1616 | `								}` |
|       - | 1617 | `								/* Pop the attribute name; the test value becomes the` |
|       - | 1618 | `								 * expression slot (a temp, not an lvalue). */` |
|       9 | 1619 | `								VmPopOperand(&pTos,1);` |
|       9 | 1620 | `								pThis->iRef++;` |
|       9 | 1621 | `								PH7_MemObjRelease(pTos);` |
|       9 | 1622 | `								PH7_MemObjStore(&sTest,pTos);` |
|       9 | 1623 | `								pTos->nIdx = SXU32_HIGH;` |
|       9 | 1624 | `								PH7_MemObjRelease(&sTest);` |
|       9 | 1625 | `								PH7_ClassInstanceUnref(pThis);` |
|       9 | 1626 | `								VM_EXIT_BREAK;` |
|     300 | 1627 | `							}else if( VmMemberNextIsRmw(pNext)` |
|     176 | 1628 | `							 && VmMagicRmwEligible(pVm,pClass,pThis,&sName) ){` |
|       - | 1629 | `								/* ++/--/compound-assign on an overloaded property: php` |
|       - | 1630 | `								 * reads through __get and writes the computed value back` |
|       - | 1631 | `								 * through __set. Arm the scratch slot the modify op will` |
|       - | 1632 | `								 * mutate and finish the op here — the pre-existing path` |
|       - | 1633 | `								 * vivified a dynamic property instead, which on any` |
|       - | 1634 | `								 * ordinary accessor class is PHL's` |
|       - | 1635 | `								 * "Cannot create dynamic property" Error. */` |
|       - | 1636 | `								ph7_value sRmwVal;` |
|       - | 1637 | `								sxu32 nRmwScratch;` |
|      31 | 1638 | `								PH7_MemObjInit(pVm,&sRmwVal);` |
|      46 | 1639 | `								VmMagicRmwArm(&(*pVm),pThis,pClass,&sName,&sRmwVal,&nRmwScratch,` |
|      30 | 1640 | `									(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|      31 | 1641 | `								VmPopOperand(&pTos,1);   /* drop the property name */` |
|      31 | 1642 | `								pThis->iRef++;` |
|      31 | 1643 | `								PH7_MemObjRelease(pTos); /* collapse the object slot */` |
|      31 | 1644 | `								PH7_MemObjStore(&sRmwVal,pTos);` |
|      31 | 1645 | `								pTos->nIdx = nRmwScratch;` |
|      31 | 1646 | `								PH7_MemObjRelease(&sRmwVal);` |
|      31 | 1647 | `								PH7_ClassInstanceUnref(pThis);` |
|      31 | 1648 | `								VM_EXIT_BREAK;` |
|     270 | 1649 | `							}else if( bRefSrcMiss` |
|     161 | 1650 | `							 && (bOwnedSet` |
|      42 | 1651 | `							  \|\| PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)) ){` |
|       - | 1652 | `								/* An OVERLOADED name is php's exception to the rule above:` |
|       - | 1653 | `								 * get_property_ptr_ptr has nothing to hand out for one, so` |
|       - | 1654 | `								 * php falls back to read_property in write mode -- __get` |
|       - | 1655 | `								 * (or the class's own read handler) answers, and the` |
|       - | 1656 | ``								 * `Indirect modification of overloaded property` notice`` |
|       - | 1657 | `								 * comes with it. Leave the miss: the read gate below is` |
|       - | 1658 | `								 * where both of those live. */` |
|     268 | 1659 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|      64 | 1660 | `							 && !VmMemberNextIsWrite(pNext)` |
|      41 | 1661 | `							 && PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) ){` |
|       - | 1662 | `								/* Subscript-write base on a class with __get: php reads` |
|       - | 1663 | `								 * through the magic layer (the write lands on the temp and` |
|       - | 1664 | `								 * is lost, like php's indirect-modification case). A PLAIN` |
|       - | 1665 | `								 * store (bPlainStore — the compiler tags those` |
|       - | 1666 | `								 * PH7_MEMBER_WRITE too) is NOT this case: it falls through` |
|       - | 1667 | `								 * to dynamic creation. Leave the miss path — the read gate` |
|       - | 1668 | `								 * below dispatches __get. */` |
|     266 | 1669 | `							}else if( pThis->pClass->iFlags & PH7_CLASS_READONLY ){` |
|       - | 1670 | `								SyBlob sErrMsg;` |
|       3 | 1671 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 1672 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|       2 | 1673 | `									&pThis->pClass->sDisp,&sName);` |
|       3 | 1674 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     262 | 1675 | `							}else if( !VmClassAllowsDynamicProps(pVm,pThis->pClass) ){` |
|       - | 1676 | `								/* php 8.2 only DEPRECATES creating a dynamic property on a` |
|       - | 1677 | `								 * class without #[AllowDynamicProperties]; PHL rejects it.` |
|       - | 1678 | `								 * stdClass / __set / declared props are unaffected. */` |
|       - | 1679 | `								SyBlob sErrMsg;` |
|      43 | 1680 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      43 | 1681 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|      38 | 1682 | `									&pThis->pClass->sDisp,&sName);` |
|      43 | 1683 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      24 | 1684 | `							}else{` |
|     223 | 1685 | `								PH7_VmCreateDynamicAttr(&(*pVm),pThis,sName.zString,sName.nByte,&pObjAttr);` |
|       - | 1686 | `							}` |
|       - | 1687 | `						}` |
|    1069 | 1688 | `						if( pObjAttr && VmMemberNextIsRmw(pNext) ){` |
|       - | 1689 | `							/* A read-modify-write READS the property before it writes it,` |
|       - | 1690 | `							 * so php warns that the name it just created was undefined and` |
|       - | 1691 | ``							 * then goes on with null — `$o->hits++` on a fresh object is`` |
|       - | 1692 | ``							 * `Undefined property: C::$hits` and then int(1). PHL created`` |
|       - | 1693 | `							 * the slot in silence. Only the read-modify-write forms:` |
|       - | 1694 | ``							 * a subscript-write base (`$o->arr[] = 1`) and a `??=` read`` |
|       - | 1695 | `							 * nothing, and php says nothing for either. */` |
|      19 | 1696 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|       6 | 1697 | `								&pClass->sDisp,&sName);` |
|       6 | 1698 | `						}` |
|     532 | 1699 | `					}` |
|     775 | 1700 | `				}` |
|       - | 1701 | `				/* An UNINITIALIZED typed property is deferred for the same reason a missing` |
|       - | 1702 | `				 * one is: which diagnostic php raises depends on the parameter. A by-VALUE` |
|       - | 1703 | ``				 * binding is the ordinary read Error (`Typed property ... must not be`` |
|       - | 1704 | ``				 * accessed before initialization`), a by-REFERENCE one is php's other`` |
|       - | 1705 | ``				 * sentence (`Cannot access uninitialized non-nullable property ... by`` |
|       - | 1706 | ``				 * reference`) -- or a NULL bind when the type admits null. A HOOKED`` |
|       - | 1707 | `				 * property is not one of these: its backing store is never what answers,` |
|       - | 1708 | `				 * and the hook rail below has php's own refusal for it -- and neither is an` |
|       - | 1709 | `` 				 * INACCESSIBLE one: php screens visibility first, so `f($o->privateSlot)` `` |
|       - | 1710 | ``				 * is `Cannot access private property` whether the slot was written or not. */`` |
|  116692 | 1711 | `				if( pObjAttr && (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|   58066 | 1712 | `				 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|     758 | 1713 | `				 && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET` |
|     379 | 1714 | `				                                \|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0` |
|     704 | 1715 | `				 && PH7_VmClassAttrAccess(&(*pVm),pClass,pObjAttr->pAttr,FALSE)` |
|     651 | 1716 | `				 && pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|      21 | 1717 | `					pObjAttr = 0;` |
|      21 | 1718 | `					bDeferUninit = 1;` |
|      10 | 1719 | `				}` |
|  116692 | 1720 | `				if( pObjAttr == 0 && pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH` |
|     162 | 1721 | `				 && (bDeferUninit` |
|     139 | 1722 | `				  \|\| !(PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)` |
|      72 | 1723 | `				    && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g'))) ){` |
|       - | 1724 | `					/* D1 commit 2: deferred property arg, property absent on a REACHABLE object` |
|       - | 1725 | `					 * base ($o is a real slot). Record a property step rooted at $o so OP_CALL can` |
|       - | 1726 | `					 * vivify+bind (by-ref) or read via __get / warn (by-value). A magic __get/__set` |
|       - | 1727 | `					 * class is recorded the same way; the resolve step emits php's Notice for the` |
|       - | 1728 | `					 * by-ref case and dispatches __get for by-value. */` |
|       - | 1729 | `					SyString sProp;` |
|     107 | 1730 | `					VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|     107 | 1731 | `					SyStringInitFromBuf(&sProp,sName.zString,sName.nByte);` |
|     107 | 1732 | `					if( pPath && VmDeferPathPushProp(pPath,&sProp) == SXRET_OK ){` |
|     107 | 1733 | `						VmPopOperand(&pTos,1);       /* drop the property name */` |
|     107 | 1734 | `						pThis->iRef++;` |
|     107 | 1735 | `						PH7_MemObjRelease(pTos);     /* collapse the object slot into the carrier */` |
|     107 | 1736 | `						pTos->x.pOther = pPath;` |
|     107 | 1737 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|     107 | 1738 | `						pTos->nIdx = SXU32_HIGH;` |
|     107 | 1739 | `						PH7_ClassInstanceUnref(pThis);` |
|     107 | 1740 | `						VM_EXIT_BREAK;` |
|       - | 1741 | `					}` |
|     ! 0 | 1742 | `					if( pPath ){` |
|     ! 0 | 1743 | `						VmFreeDeferredPath(pPath);` |
|     ! 0 | 1744 | `					}` |
|       - | 1745 | `					/* fall through to the normal miss handling on allocation failure */` |
|     ! 0 | 1746 | `				}` |
|  116588 | 1747 | `				if( pObjAttr == 0` |
|   58916 | 1748 | `				 && (pInstr->iP2 == PH7_MEMBER_READ \|\| pInstr->iP2 == PH7_MEMBER_COALESCE) ){` |
|       - | 1749 | `					/* A LAZY native property whose class answers a read from its ZEROED` |
|       - | 1750 | `					 * struct (DatePeriod), asked before anything installed the set. php` |
|       - | 1751 | ``					 * consults that read handler in the plain read AND in `??` -- its third`` |
|       - | 1752 | `` 					 * accessor level takes the property's VALUE, so `$p->recurrences ?? 'd'` `` |
|       - | 1753 | `					 * is 0 there -- while isset()/empty() go to the has_property handler,` |
|       - | 1754 | `					 * which answers false for an object that has no struct at all. */` |
|     353 | 1755 | `					ph7_class_attr *pLz = PH7_ClassExtractAttribute(pClass,` |
|     116 | 1756 | `						SyStringData(&sName),SyStringLength(&sName));` |
|     232 | 1757 | `					if( pLz && (pLz->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT)` |
|      36 | 1758 | `					 && PH7_ATTR_LAZY_ABSENT(pLz,pThis) ){` |
|      25 | 1759 | `						pLazyDefault = pLz;` |
|      12 | 1760 | `					}` |
|     116 | 1761 | `				}` |
|  116593 | 1762 | `				if( pObjAttr == 0 ){` |
|       - | 1763 | `					/* Missing property. On a plain READ, php dispatches __get($name) and the` |
|       - | 1764 | `					 * expression takes its RETURN VALUE (band A #3a — pre-fix the result was` |
|       - | 1765 | `					 * discarded and a PHL-native warn fired even when __get existed). isset/` |
|       - | 1766 | `					 * empty context consults __isset first (then __get for empty()'s value` |
|       - | 1767 | `					 * test); a plain-store write context parked a pending __set above. A` |
|       - | 1768 | `					 * self-recursive read of the same property falls back to the` |
|       - | 1769 | `					 * undefined-property path via the guard, like php's property guard. */` |
|    1239 | 1770 | `					ph7_class_method *pGetMagic = 0;` |
|    1239 | 1771 | `					if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|     199 | 1772 | `						ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|     199 | 1773 | `						if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 1774 | `							ph7_value sIssetRet;` |
|       - | 1775 | `							int bSet;` |
|      61 | 1776 | `							PH7_MemObjInit(pVm,&sIssetRet);` |
|      61 | 1777 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|      61 | 1778 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|      61 | 1779 | `							VmMagicGuardPop(pVm);` |
|      61 | 1780 | `							PH7_MemObjToBool(&sIssetRet);` |
|      61 | 1781 | `							bSet = sIssetRet.x.iVal != 0;` |
|      61 | 1782 | `							PH7_MemObjRelease(&sIssetRet);` |
|      61 | 1783 | `							if( bSet && VmMemberCtxWantsValue(pInstr->iP2) ){` |
|       - | 1784 | ``								/* empty() and `??`: __isset said set, so php goes on to`` |
|       - | 1785 | `								 * __get for the VALUE — emptiness is judged on it, and the` |
|       - | 1786 | `								 * coalesce simply IS it (a null answer then takes the` |
|       - | 1787 | `								 * default, which OP_NULLC does for free). */` |
|      30 | 1788 | `								ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       - | 1789 | `								ph7_value sEmptyVal;` |
|      30 | 1790 | `								PH7_MemObjInit(pVm,&sEmptyVal);` |
|      30 | 1791 | `								if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|      30 | 1792 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|      30 | 1793 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|      30 | 1794 | `									VmMagicGuardPop(pVm);` |
|      14 | 1795 | `								}` |
|      30 | 1796 | `								VmPopOperand(&pTos,1);` |
|      30 | 1797 | `								pThis->iRef++;` |
|      30 | 1798 | `								PH7_MemObjRelease(pTos);` |
|      30 | 1799 | `								PH7_MemObjStore(&sEmptyVal,pTos);` |
|      30 | 1800 | `								pTos->nIdx = SXU32_HIGH;` |
|      30 | 1801 | `								PH7_MemObjRelease(&sEmptyVal);` |
|      30 | 1802 | `								PH7_ClassInstanceUnref(pThis);` |
|      30 | 1803 | `								VM_EXIT_BREAK;` |
|       - | 1804 | `							}` |
|       - | 1805 | `							/* isset(): the truth of __isset IS the answer — push a non-null` |
|       - | 1806 | `							 * marker for true, NULL for false (isset only tests null-ness). */` |
|      33 | 1807 | `							VmPopOperand(&pTos,1);` |
|      33 | 1808 | `							pThis->iRef++;` |
|      33 | 1809 | `							PH7_MemObjRelease(pTos);` |
|      33 | 1810 | `							if( bSet ){` |
|      17 | 1811 | `								pTos->x.iVal = 1;` |
|      17 | 1812 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       7 | 1813 | `							}` |
|      33 | 1814 | `							pTos->nIdx = SXU32_HIGH;` |
|      33 | 1815 | `							PH7_ClassInstanceUnref(pThis);` |
|      33 | 1816 | `							VM_EXIT_BREAK;` |
|       - | 1817 | `						}` |
|      69 | 1818 | `					}` |
|    1176 | 1819 | `					if( (!VmMemberCtxIsLookup(pInstr->iP2) \|\| pInstr->iP2 == PH7_MEMBER_COALESCE)` |
|    1123 | 1820 | `					 && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|    1128 | 1821 | `					 && !VmMemberNextIsWrite(pInstr + 1) ){` |
|       - | 1822 | `						/* Plain reads AND the ??=/subscript write-base (PH7_MEMBER_WRITE,` |
|       - | 1823 | `						 * which php reads through __get); read-modify-write forms are` |
|       - | 1824 | `						 * excluded (they vivified above — approximate, recorded), and so is` |
|       - | 1825 | `						 * a destructuring target (a pure write — php never reads it).` |
|       - | 1826 | ``						 * `??` reaches here only when the class declares NO __isset — the`` |
|       - | 1827 | `						 * branch above has returned otherwise — so php's gate is absent and` |
|       - | 1828 | `						 * __get answers on its own. */` |
|     263 | 1829 | `						pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|     129 | 1830 | `					}` |
|    1181 | 1831 | `					if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 1832 | `						ph7_value sMagicRet;` |
|     114 | 1833 | `						PH7_MemObjInit(pVm,&sMagicRet);` |
|     114 | 1834 | `						VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     114 | 1835 | `						PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|     114 | 1836 | `						VmMagicGuardPop(pVm);` |
|     114 | 1837 | `						if( VmMemberFetchForWrite(pInstr) ){` |
|       - | 1838 | `							/* php's indirect-modification notice, and the separation` |
|       - | 1839 | `							 * that makes the write it describes land nowhere. Raised` |
|       - | 1840 | `							 * BEFORE the pop below: sName still aliases the NAME` |
|       - | 1841 | ``							 * operand when the name is dynamic (`$o->$k[0] = v`). */`` |
|      13 | 1842 | `							PH7_VmOverloadedPropNotice(&(*pVm),pClass,&sName,&sMagicRet);` |
|     108 | 1843 | `						}else if( pInstr->iP2 == PH7_MEMBER_DEFPATH ){` |
|       - | 1844 | `							/* A deferred call ARGUMENT: __get has answered — php calls it` |
|       - | 1845 | `							 * where the property is WRITTEN, whatever the parameter turns` |
|       - | 1846 | `							 * out to be — and only the by-ref verdict is still pending.` |
|       - | 1847 | `							 * Carry the value with the class and name that produced it, so` |
|       - | 1848 | `							 * the notice lands at the call for a by-REFERENCE parameter and` |
|       - | 1849 | `							 * nothing is said for a by-value one, without __get running` |
|       - | 1850 | `							 * twice or a second read arriving after a later argument's side` |
|       - | 1851 | `							 * effects. */` |
|      54 | 1852 | `							VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_PROP,` |
|      17 | 1853 | `								pClass,&sName,&sMagicRet);` |
|      37 | 1854 | `							if( pPre ){` |
|      37 | 1855 | `								VmPopOperand(&pTos,1);   /* drop the property name */` |
|      37 | 1856 | `								pThis->iRef++;` |
|      37 | 1857 | `								PH7_MemObjRelease(pTos); /* collapse the object slot into the carrier */` |
|      37 | 1858 | `								pTos->x.pOther = pPre;` |
|      37 | 1859 | `								pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      37 | 1860 | `								pTos->nIdx = SXU32_HIGH;` |
|      37 | 1861 | `								PH7_MemObjRelease(&sMagicRet);` |
|      37 | 1862 | `								PH7_ClassInstanceUnref(pThis);` |
|      37 | 1863 | `								VM_EXIT_BREAK;` |
|       - | 1864 | `							}` |
|     ! 0 | 1865 | `						}` |
|       - | 1866 | `						/* Pop the attribute name, replace the object slot with the magic` |
|       - | 1867 | `						 * result (a temp, not an lvalue — nIdx stays constant). A throw` |
|       - | 1868 | `						 * from __get parked at the boundary; the fetch-point router lands` |
|       - | 1869 | `						 * it and abandons this slot. */` |
|      80 | 1870 | `						VmPopOperand(&pTos,1);` |
|      80 | 1871 | `						pThis->iRef++;` |
|      80 | 1872 | `						PH7_MemObjRelease(pTos);` |
|      80 | 1873 | `						PH7_MemObjStore(&sMagicRet,pTos);` |
|      80 | 1874 | `						pTos->nIdx = SXU32_HIGH;` |
|      80 | 1875 | `						PH7_MemObjRelease(&sMagicRet);` |
|      80 | 1876 | `						PH7_ClassInstanceUnref(pThis);` |
|      80 | 1877 | `						VM_EXIT_BREAK;` |
|       - | 1878 | `					}` |
|       - | 1879 | `					/* No __get (or guard held): load null. In isset()/empty() context (iP2 3/4)` |
|       - | 1880 | `					 * PHP returns false/true SILENTLY, so suppress the read-miss warning there` |
|       - | 1881 | `					 * (mirrors the array LOAD_IDX iP2=4/6 suppression); likewise when a` |
|       - | 1882 | `					 * pending __set was parked above (the following OP_STORE dispatches it —` |
|       - | 1883 | `					 * nothing is "undefined" about that write) or a throw is parked on the` |
|       - | 1884 | `					 * boundary rail (e.g. the readonly-class dynamic-property Error — the` |
|       - | 1885 | `					 * fetch-point router lands it right after this op). */` |
|    1066 | 1886 | `					if( !VmMemberCtxIsLookup(pInstr->iP2) && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|     934 | 1887 | `					 && pVm->pMagicSetThis == 0` |
|     549 | 1888 | `					 && pVm->nBoundaryRc == 0 ){` |
|       - | 1889 | `						/* A property missing from the INSTANCE may still be DECLARED by the` |
|       - | 1890 | ``						 * class — that is what `unset($o->p)` leaves behind, and php keeps`` |
|       - | 1891 | `						 * answering for the declaration rather than calling the name` |
|       - | 1892 | `						 * undefined: the visibility screen still applies, and a TYPED slot` |
|       - | 1893 | `						 * is back to uninitialized (the same Error a never-written one` |
|       - | 1894 | `						 * raises). Only a name the class does not declare at all is the` |
|       - | 1895 | `						 * "Undefined property" warning. A destructuring target is silent` |
|       - | 1896 | `						 * either way: php created the property above or dispatched __set. */` |
|     175 | 1897 | `						ph7_class_attr *pDeclAttr = PH7_ClassScopedAttribute(&(*pVm),pClass,` |
|      57 | 1898 | `							SyStringData(&sName),SyStringLength(&sName));` |
|       - | 1899 | `						/* A static property, a class constant and a native engine slot are` |
|       - | 1900 | `						 * not instance properties; a dynamic one is gone for good once it` |
|       - | 1901 | `						 * is unset, since nothing declares it. */` |
|     114 | 1902 | `						if( pDeclAttr` |
|      77 | 1903 | `						 && (pDeclAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|       - | 1904 | `						                          \|PH7_CLASS_ATTR_HIDDEN\|PH7_CLASS_ATTR_DYNAMIC)) ){` |
|       9 | 1905 | `							pDeclAttr = 0;` |
|       3 | 1906 | `						}` |
|     118 | 1907 | `						if( pDeclAttr && PH7_ATTR_LAZY_ABSENT(pDeclAttr,pThis) ){` |
|       - | 1908 | `							/* The object has never held this name. php has two answers and` |
|       - | 1909 | `							 * the class says which: a read handler over the ZEROED struct` |
|       - | 1910 | `							 * (DatePeriod -- null/0/false, in silence), or nothing at all` |
|       - | 1911 | `							 * (DateInterval), which is the ordinary "Undefined property"` |
|       - | 1912 | `							 * warning. Either way the DECLARATION is not what answers, so` |
|       - | 1913 | `							 * the typed-slot Error below must not fire on a name php keeps` |
|       - | 1914 | `							 * no slot for. */` |
|      25 | 1915 | `							if( pDeclAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT ){` |
|      21 | 1916 | `								pLazyDefault = pDeclAttr;` |
|      10 | 1917 | `							}` |
|      25 | 1918 | `							pDeclAttr = 0;` |
|      12 | 1919 | `						}` |
|     118 | 1920 | `						if( pDeclAttr && (pDeclAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){` |
|       - | 1921 | `							/* ...and a VIRTUAL property is answered by the class's handler` |
|       - | 1922 | `							 * too, so the DECLARATION must not raise the typed-slot Error` |
|       - | 1923 | `							 * for a name php keeps no slot for either. */` |
|     ! 0 | 1924 | `							pDeclAttr = 0;` |
|     ! 0 | 1925 | `						}` |
|     118 | 1926 | `						if( pLazyDefault ){` |
|       - | 1927 | `							/* php's read handler answered: say nothing. */` |
|     105 | 1928 | `						}else if( pDeclAttr` |
|      52 | 1929 | `						 && !PH7_VmClassAttrAccess(&(*pVm),pClass,pDeclAttr,FALSE) ){` |
|       - | 1930 | `							SyBlob sErrMsg;` |
|     ! 0 | 1931 | `							const char *zVis = pDeclAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|     ! 0 | 1932 | `								? "private" : "protected";` |
|     ! 0 | 1933 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     ! 0 | 1934 | `							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",` |
|     ! 0 | 1935 | `								zVis,&pClass->sDisp,&sName);` |
|     ! 0 | 1936 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",` |
|       - | 1937 | `								sizeof("Error")-1,&sErrMsg));` |
|      98 | 1938 | `						}else if( pDeclAttr && (pDeclAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|     ! 0 | 1939 | `							VmBoundaryPark(&(*pVm),` |
|     ! 0 | 1940 | `								VmThrowUninitializedPropertyError(&(*pVm),pClass,pDeclAttr));` |
|     ! 0 | 1941 | `						}else{` |
|     145 | 1942 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|      47 | 1943 | `								&pClass->sDisp,&sName);` |
|       - | 1944 | `						}` |
|      57 | 1945 | `					}` |
|     533 | 1946 | `				}` |
|  116420 | 1947 | `				if( pObjAttr && (pObjAttr->iState & VM_CLASS_ATTR_RDONLY)` |
|   57702 | 1948 | `				 && (pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|      35 | 1949 | `				  \|\| ((pInstr + 1)->iOp == PH7_OP_STORE && (pInstr + 1)->iP2 != 0)) ){` |
|       - | 1950 | `					/* php's write_property handler refuses the plain store and the` |
|       - | 1951 | `					 * destructuring one that goes through it; everything that takes a` |
|       - | 1952 | ``					 * POINTER to the property instead -- a compound assign, `++`, `??=`,`` |
|       - | 1953 | `					 * a reference bind -- bypasses the handler in php and is left alone` |
|       - | 1954 | `					 * here too. */` |
|       7 | 1955 | `					VmBoundaryPark(&(*pVm),VmThrowNativeReadOnly(&(*pVm),pObjAttr->pAttr));` |
|       7 | 1956 | `					VmPopOperand(&pTos,1);` |
|       7 | 1957 | `					PH7_MemObjRelease(pTos);` |
|       7 | 1958 | `					pTos->nIdx = SXU32_HIGH;` |
|       7 | 1959 | `					VM_EXIT_BREAK;` |
|       - | 1960 | `				}` |
|  116419 | 1961 | `				VmPopOperand(&pTos,1);` |
|       - | 1962 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|       - | 1963 | `				 * This is due to the following case:` |
|       - | 1964 | `				 *     (new TestClass())->foo;` |
|       - | 1965 | `				 */` |
|  116419 | 1966 | `				pThis->iRef++;` |
|  116419 | 1967 | `				PH7_MemObjRelease(pTos);` |
|  116419 | 1968 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|  116419 | 1969 | `				if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - | 1970 | ``					/* `$o->p =& $x`: stash the resolved instance property slot for the`` |
|       - | 1971 | `					 * following member-marked OP_STORE_REF, which rebinds it to alias the` |
|       - | 1972 | `					 * source variable. Do NOT run the read/hook/magic/uninit machinery` |
|       - | 1973 | `					 * below — a reference bind neither reads the value nor triggers` |
|       - | 1974 | `					 * get/set hooks or an uninitialized-typed Error. pThis stays retained` |
|       - | 1975 | `					 * (the iRef++ above); OP_STORE_REF releases it. */` |
|      62 | 1976 | `					if( pObjAttr && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE) ){` |
|       - | 1977 | `						/* A reference bind is a WRITE, and php's refusing handler answers` |
|       - | 1978 | `						 * it with the same sentence a plain store gets. */` |
|     ! 0 | 1979 | `						VmBoundaryPark(&(*pVm),` |
|     ! 0 | 1980 | `							VmThrowNativeNoWrite(&(*pVm),PH7_VmAttrOwner(pObjAttr),pObjAttr->pAttr));` |
|     ! 0 | 1981 | `						pVm->pRefTargetAttr = 0;` |
|     ! 0 | 1982 | `						pVm->pRefTargetThis = 0;` |
|     ! 0 | 1983 | `						pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 1984 | `						PH7_ClassInstanceUnref(pThis);` |
|      63 | 1985 | `					}else if( pObjAttr && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) ){` |
|       - | 1986 | ``						/* `$i->f =& $x`: php REFUSES to make a handler-backed property`` |
|       - | 1987 | `						 * the target of a reference — there is no slot to rebind, and` |
|       - | 1988 | `						 * every write through the alias would skip the conversion the` |
|       - | 1989 | `						 * handler is there to do. PHL rebound the slot instead, so` |
|       - | 1990 | ``						 * `$x = 2.5` afterwards wrote 2.5 microseconds-free into the`` |
|       - | 1991 | `						 * interval. */` |
|       - | 1992 | `						SyBlob sErrMsg;` |
|       3 | 1993 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 1994 | `						SyBlobAppend(&sErrMsg,"Cannot assign by reference to overloaded object",` |
|       - | 1995 | `							sizeof("Cannot assign by reference to overloaded object")-1);` |
|       3 | 1996 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       3 | 1997 | `						pVm->pRefTargetAttr = 0;` |
|       3 | 1998 | `						pVm->pRefTargetThis = 0;` |
|       3 | 1999 | `						pVm->pRefTargetStaticAttr = 0;` |
|       3 | 2000 | `						PH7_ClassInstanceUnref(pThis);` |
|      61 | 2001 | `					}else if( pObjAttr ){` |
|      60 | 2002 | `						pVm->pRefTargetAttr = pObjAttr;` |
|      60 | 2003 | `						pVm->pRefTargetThis = pThis;` |
|      60 | 2004 | `						pVm->pRefTargetStaticAttr = 0;` |
|      60 | 2005 | `						pTos->nIdx = pObjAttr->nIdx;` |
|      31 | 2006 | `					}else{` |
|       - | 2007 | `						/* Missing/inaccessible target: leave nothing stashed; OP_STORE_REF` |
|       - | 2008 | `						 * no-ops. Balance the retain. */` |
|     ! 0 | 2009 | `						pVm->pRefTargetAttr = 0;` |
|     ! 0 | 2010 | `						pVm->pRefTargetThis = 0;` |
|     ! 0 | 2011 | `						pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 2012 | `						PH7_ClassInstanceUnref(pThis);` |
|       - | 2013 | `					}` |
|      62 | 2014 | `					VM_EXIT_BREAK;` |
|       - | 2015 | `				}` |
|  116359 | 2016 | `				if( pLazyDefault && pLazyDefault->pNativeValue ){` |
|       - | 2017 | `					/* A LAZY native property read before its class installed the set,` |
|       - | 2018 | `					 * on a class that answers such a read from its ZEROED struct. The` |
|       - | 2019 | `					 * declared literal IS that struct's field (the 76th session moved` |
|       - | 2020 | `					 * DatePeriod's seven defaults onto it), and the answer is a` |
|       - | 2021 | `					 * TEMPORARY: nothing was installed, so there is no slot to address` |
|       - | 2022 | `					 * and nIdx stays the constant sentinel the pop left. */` |
|      25 | 2023 | `					PH7_NativeLiteralValue(&(*pVm),pLazyDefault->pNativeValue,pTos);` |
|      12 | 2024 | `				}` |
|  116359 | 2025 | `				if( pObjAttr ){` |
|  115293 | 2026 | `					ph7_value *pValue = 0; /* cc warning */` |
|       - | 2027 | `					/* Check attribute access */` |
|  115293 | 2028 | `					if( PH7_VmClassAttrAccess(&(*pVm),pClass,pObjAttr->pAttr,FALSE) ){` |
|  115242 | 2029 | `						if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET))` |
|   57811 | 2030 | `						 && !VmHookGuardHeld(pVm,(void *)pThis,&sName) ){` |
|       - | 2031 | `							/* PHP 8.4 property hooks: route reads, writes, and the` |
|       - | 2032 | `							 * read-modify-write forms through the synthesized hook` |
|       - | 2033 | `							 * methods. A held guard (either kind) means we are INSIDE` |
|       - | 2034 | `							 * one of this property's own hook bodies — fall through to` |
|       - | 2035 | `` 							 * the raw backing slot for BOTH directions (php: `$this->x` `` |
|       - | 2036 | `							 * within any of x's hooks addresses the backing store). */` |
|     232 | 2037 | `							VmInstr *pNextH = pInstr + 1;` |
|     232 | 2038 | `							int bPlainStore = (pNextH->iOp == PH7_OP_STORE && pNextH->iP2 != 0);` |
|     394 | 2039 | `							int bCoalesceW = pInstr->iP2 == PH7_MEMBER_WRITE` |
|     228 | 2040 | `								&& pNextH->iOp == PH7_OP_NULLC_JMP;` |
|       - | 2041 | `							/* ++/--/compound-assign: the modify op FOLLOWS the member` |
|       - | 2042 | `							 * directly (the compiler tags compound-assign members` |
|       - | 2043 | `							 * PH7_MEMBER_WRITE too, so classify by the next op, not iP2;` |
|       - | 2044 | `							 * the !bPlainStore term is load-bearing — VmMemberNextIsWrite` |
|       - | 2045 | `							 * returns 1 for a member OP_STORE too) */` |
|     232 | 2046 | `							int bRmwNext = !bPlainStore && VmMemberNextIsWrite(pNextH);` |
|     275 | 2047 | `							int bSubscriptW = !bPlainStore && !bCoalesceW && !bRmwNext` |
|     316 | 2048 | `								&& pInstr->iP2 == PH7_MEMBER_WRITE;` |
|     232 | 2049 | `							if( bPlainStore ){` |
|       - | 2050 | `								/* Arm the pending hook-set for the following OP_STORE — it` |
|       - | 2051 | `								 * dispatches the set hook, or throws the read-only Error` |
|       - | 2052 | `								 * when the property only has a get hook. (The scalar` |
|       - | 2053 | `								 * transient is safe here: its window is exactly one` |
|       - | 2054 | `								 * instruction, MEMBER -> STORE, nothing runs in between.) */` |
|      55 | 2055 | `								pThis->iRef++;` |
|      55 | 2056 | `								pVm->pHookSetThis = pThis;` |
|      55 | 2057 | `								pVm->pHookSetAttr = pObjAttr->pAttr;` |
|      55 | 2058 | `								pVm->nHookSetIdx = pObjAttr->nIdx;` |
|      55 | 2059 | `								pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|      55 | 2060 | `								PH7_ClassInstanceUnref(pThis);` |
|      55 | 2061 | `								VM_EXIT_BREAK;` |
|       - | 2062 | `							}` |
|     180 | 2063 | `							if( bSubscriptW ){` |
|       - | 2064 | `								/* Subscript write base ($o->m[] = v, $o->m[$k] = v):` |
|       - | 2065 | `								 * php's catchable Error, with or without a set hook` |
|       - | 2066 | ``								 * (only a by-ref `&get` hook would allow it — a loud`` |
|       - | 2067 | `								 * compile error in PHL). The op completes benignly with` |
|       - | 2068 | `								 * a null temp; the fetch-point router lands the throw. */` |
|       - | 2069 | `								SyBlob sErrMsg;` |
|       7 | 2070 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       7 | 2071 | `								SyBlobFormat(&sErrMsg,"Indirect modification of %z::$%z is not allowed",` |
|       6 | 2072 | `									&pThis->pClass->sDisp,&pObjAttr->pAttr->sName);` |
|       7 | 2073 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       7 | 2074 | `								PH7_ClassInstanceUnref(pThis);` |
|       7 | 2075 | `								VM_EXIT_BREAK;` |
|       - | 2076 | `							}` |
|     170 | 2077 | `							if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|      89 | 2078 | `							 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 2079 | `								/* VIRTUAL set-only property: there is no backing store to` |
|       - | 2080 | `								 * fall back to, so EVERY read context — plain read,` |
|       - | 2081 | `								 * isset()/empty() (php throws there too, probe-verified),` |
|       - | 2082 | `								 * the ??= test, an RMW read — is php's catchable` |
|       - | 2083 | `								 * write-only Error. */` |
|       - | 2084 | `								SyBlob sErrMsg;` |
|       9 | 2085 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       9 | 2086 | `								SyBlobFormat(&sErrMsg,"Property %z::$%z is write-only",` |
|       8 | 2087 | `									&pThis->pClass->sDisp,&pObjAttr->pAttr->sName);` |
|       9 | 2088 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       9 | 2089 | `								PH7_ClassInstanceUnref(pThis);` |
|       9 | 2090 | `								VM_EXIT_BREAK;` |
|       - | 2091 | `							}` |
|     166 | 2092 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2093 | ``								/* isset()/empty()/`??` on a hooked property all call the get`` |
|       - | 2094 | `								 * hook (php): isset is "get() !== null", empty tests the` |
|       - | 2095 | ``								 * value, and `??` IS the value. */`` |
|       - | 2096 | `								ph7_value sHookRet;` |
|      16 | 2097 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|      16 | 2098 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|      16 | 2099 | `									if( !VmMemberCtxWantsValue(pInstr->iP2) ){` |
|       - | 2100 | `										/* isset(): the trailing builtin tests NULL-ness of what` |
|       - | 2101 | `										 * it is handed, so "not set" has to BE null. Storing` |
|       - | 2102 | ``										 * bool(FALSE) here made `isset($o->p)` answer TRUE for`` |
|       - | 2103 | `										 * a get hook returning null — the same non-null marker` |
|       - | 2104 | `										 * convention the __isset path uses. */` |
|       8 | 2105 | `										if( (sHookRet.iFlags & MEMOBJ_NULL) == 0 ){` |
|       6 | 2106 | `											pTos->x.iVal = 1;` |
|       6 | 2107 | `											MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       4 | 2108 | `										}else{` |
|       3 | 2109 | `											MemObjSetType(pTos,MEMOBJ_NULL);` |
|       - | 2110 | `										}` |
|       5 | 2111 | `									}else{` |
|       - | 2112 | ``										/* empty() judges the value; `??` IS the value. */`` |
|      10 | 2113 | `										PH7_MemObjStore(&sHookRet,pTos);` |
|       - | 2114 | `									}` |
|      16 | 2115 | `									pTos->nIdx = SXU32_HIGH;` |
|      16 | 2116 | `									PH7_MemObjRelease(&sHookRet);` |
|      16 | 2117 | `									PH7_ClassInstanceUnref(pThis);` |
|      16 | 2118 | `									VM_EXIT_BREAK;` |
|       - | 2119 | `								}` |
|     ! 0 | 2120 | `								PH7_MemObjRelease(&sHookRet);` |
|     ! 0 | 2121 | `							}` |
|     152 | 2122 | `							if( !bCoalesceW && !bRmwNext && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2123 | `								/* Read: dispatch __phl_hook_get_NAME (inheritance-aware) */` |
|       - | 2124 | `								ph7_value sHookRet;` |
|     118 | 2125 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|     118 | 2126 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|     104 | 2127 | `									if( pInstr->iP2 == PH7_MEMBER_DEFPATH ){` |
|       - | 2128 | `										/* A deferred call ARGUMENT of a HOOKED property. The` |
|       - | 2129 | `										 * get hook has answered, as php's does; what a` |
|       - | 2130 | `										 * by-REFERENCE parameter then gets is not a notice` |
|       - | 2131 | `										 * but php's refusal — a hook has no slot to alias` |
|       - | 2132 | ``										 * and `&get` does not exist here — while a by-VALUE`` |
|       - | 2133 | `										 * one simply takes the hook's value. */` |
|      60 | 2134 | `										VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),` |
|      38 | 2135 | `											VM_OVER_HOOK,pThis->pClass,&pObjAttr->pAttr->sName,&sHookRet);` |
|      41 | 2136 | `										if( pPre ){` |
|      41 | 2137 | `											PH7_MemObjRelease(pTos);` |
|      41 | 2138 | `											pTos->x.pOther = pPre;` |
|      41 | 2139 | `											pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      41 | 2140 | `											pTos->nIdx = SXU32_HIGH;` |
|      41 | 2141 | `											PH7_MemObjRelease(&sHookRet);` |
|      41 | 2142 | `											PH7_ClassInstanceUnref(pThis);` |
|      72 | 2143 | `											VM_EXIT_BREAK;` |
|       - | 2144 | `										}` |
|     ! 0 | 2145 | `									}` |
|      65 | 2146 | `									PH7_MemObjStore(&sHookRet,pTos);` |
|      65 | 2147 | `									pTos->nIdx = SXU32_HIGH;` |
|      65 | 2148 | `									PH7_MemObjRelease(&sHookRet);` |
|      65 | 2149 | `									PH7_ClassInstanceUnref(pThis);` |
|      65 | 2150 | `									VM_EXIT_BREAK;` |
|       - | 2151 | `								}` |
|      15 | 2152 | `								PH7_MemObjRelease(&sHookRet);` |
|       7 | 2153 | `							}` |
|      49 | 2154 | `							if( bCoalesceW \|\| bRmwNext ){` |
|       - | 2155 | ``								/* `$o->x ??= v` (bCoalesceW) and the read-modify-write`` |
|       - | 2156 | ``								 * forms `$o->x++`, `$o->x .= v`, … (bRmwNext): php reads`` |
|       - | 2157 | `								 * through the get hook (raw backing when the property is` |
|       - | 2158 | `								 * set-only) and writes through the set hook.` |
|       - | 2159 | `								 *   - ??=: the test value goes on the stack and a` |
|       - | 2160 | `								 *     COAL_HOOK entry is pushed for the OP_NULLC_STORE` |
|       - | 2161 | `								 *     at the jump target; the fetch-point sweep drops it` |
|       - | 2162 | `								 *     when the short-circuit jump skips the assign or a` |
|       - | 2163 | `								 *     throw abandons the RHS (the entry is NOT a scalar` |
|       - | 2164 | `								 *     transient — nested stores/coalesces in the RHS` |
|       - | 2165 | `								 *     stack their own entries above it).` |
|       - | 2166 | `								 *   - RMW: the value goes into a fresh SCRATCH slot the` |
|       - | 2167 | `								 *     modify op mutates in place; the entry makes the` |
|       - | 2168 | `								 *     op's tail dispatch the set side with the computed` |
|       - | 2169 | `								 *     value (PH7_HOOK_RMW_WRITEBACK). */` |
|       - | 2170 | `								ph7_value sCur;` |
|       - | 2171 | `								sxi32 rcCur;` |
|      35 | 2172 | `								PH7_MemObjInit(pVm,&sCur);` |
|      35 | 2173 | `								rcCur = PH7_VmHookGetAttrValue(pThis,pObjAttr,&sCur);` |
|      35 | 2174 | `								if( rcCur == SXERR_NOTFOUND ){` |
|       - | 2175 | `									/* set-only hook (or guard edge): the read side is the` |
|       - | 2176 | `									 * raw backing store (php) */` |
|       3 | 2177 | `									ph7_value *pBack = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|       3 | 2178 | `									if( pBack ){` |
|       3 | 2179 | `										PH7_MemObjStore(pBack,&sCur);` |
|       2 | 2180 | `									}` |
|      34 | 2181 | `								}else if( pVm->nBoundaryRc != 0 ){` |
|       - | 2182 | `									/* the get hook threw: leave the null temp; the` |
|       - | 2183 | `									 * fetch-point router lands the parked throw —` |
|       - | 2184 | `									 * nothing is armed. */` |
|     ! 0 | 2185 | `									PH7_MemObjRelease(&sCur);` |
|     ! 0 | 2186 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2187 | `									VM_EXIT_BREAK;` |
|       - | 2188 | `								}` |
|      35 | 2189 | `								if( bCoalesceW ){` |
|       - | 2190 | `									VmHookRmw sPend;` |
|      15 | 2191 | `									PH7_MemObjStore(&sCur,pTos);` |
|      15 | 2192 | `									pTos->nIdx = SXU32_HIGH;` |
|      15 | 2193 | `									PH7_MemObjRelease(&sCur);` |
|      15 | 2194 | `									sPend.iKind = VM_HOOK_PEND_COAL_HOOK;` |
|      15 | 2195 | `									sPend.pThis = pThis;` |
|      15 | 2196 | `									sPend.pAttr = pObjAttr->pAttr;` |
|      15 | 2197 | `									sPend.nBackIdx = pObjAttr->nIdx;` |
|      15 | 2198 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|      15 | 2199 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|      15 | 2200 | `									sPend.pOwnerStack = (void *)pStack;` |
|      15 | 2201 | `									sPend.pInstrs = (void *)aInstr;` |
|      15 | 2202 | `									sPend.nJmpPc = (sxu32)(pc + 1);            /* the OP_NULLC_JMP */` |
|      15 | 2203 | `									sPend.nPc = (sxu32)(pNextH->iP2 - 1);      /* the OP_NULLC_STORE */` |
|      15 | 2204 | `									pThis->iRef++;` |
|      15 | 2205 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|      15 | 2206 | `									PH7_ClassInstanceUnref(pThis);` |
|      15 | 2207 | `									VM_EXIT_BREAK;` |
|       - | 2208 | `								}` |
|       - | 2209 | `								{` |
|      21 | 2210 | `									ph7_value *pScr = PH7_ReserveMemObj(&(*pVm));` |
|      21 | 2211 | `									if( pScr ){` |
|       - | 2212 | `										VmHookRmw sRmw;` |
|      21 | 2213 | `										PH7_MemObjStore(&sCur,pScr);` |
|      21 | 2214 | `										PH7_MemObjStore(&sCur,pTos);` |
|      21 | 2215 | `										pTos->nIdx = pScr->nIdx;` |
|      21 | 2216 | `										sRmw.iKind = VM_HOOK_PEND_RMW;` |
|      21 | 2217 | `										sRmw.pThis = pThis;` |
|      21 | 2218 | `										sRmw.pAttr = pObjAttr->pAttr;` |
|      21 | 2219 | `										sRmw.nBackIdx = pObjAttr->nIdx;` |
|      21 | 2220 | `										sRmw.nScratchIdx = pScr->nIdx;` |
|      21 | 2221 | `										SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      21 | 2222 | `										sRmw.pOwnerStack = (void *)pStack;` |
|      21 | 2223 | `										sRmw.pInstrs = (void *)aInstr;` |
|      21 | 2224 | `										sRmw.nJmpPc = (sxu32)(pc + 1);  /* the modify op ... */` |
|      21 | 2225 | `										sRmw.nPc = (sxu32)(pc + 1);     /* ... is the whole window */` |
|      21 | 2226 | `										pThis->iRef++;` |
|      21 | 2227 | `										SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      10 | 2228 | `									}` |
|       - | 2229 | `									/* OOM: pScr == 0 — leave the null temp (loud allocator` |
|       - | 2230 | `									 * diagnostics already fired) */` |
|      21 | 2231 | `									PH7_MemObjRelease(&sCur);` |
|      21 | 2232 | `									PH7_ClassInstanceUnref(pThis);` |
|      21 | 2233 | `									VM_EXIT_BREAK;` |
|       - | 2234 | `								}` |
|       - | 2235 | `							}` |
|       7 | 2236 | `						}` |
|       - | 2237 | ``						/* `$o->p[$k] = v`, `$o->p[] = v` and `unset($o->p[$k])`: the`` |
|       - | 2238 | `						 * property is the BASE of a subscript write, so the write lands` |
|       - | 2239 | `						 * inside whatever it holds. php screens that where it screens a` |
|       - | 2240 | ``						 * store -- `Cannot indirectly modify readonly property C::$p` --`` |
|       - | 2241 | `						 * and it screens it BEFORE the uninitialized-typed read below,` |
|       - | 2242 | `						 * which is why this sits in front. PHL wrote through to the` |
|       - | 2243 | `						 * array a readonly property held. */` |
|       - | 2244 | `						{` |
|  115033 | 2245 | `							VmInstr *pNextI = pInstr + 1;` |
|  115033 | 2246 | `							int bStoreI = (pNextI->iOp == PH7_OP_STORE && pNextI->iP2 != 0);` |
|  115033 | 2247 | `							int bCoalI = pNextI->iOp == PH7_OP_NULLC_JMP;` |
|  175134 | 2248 | `							int bUnsetBase = pInstr->iP2 == PH7_MEMBER_READ` |
|  115042 | 2249 | `								&& pNextI->iOp == PH7_OP_LOAD_IDX && VM_IDX_IS_UNSET(pNextI->iP2);` |
|  172547 | 2250 | `							int bBaseW = bUnsetBase` |
|  221930 | 2251 | `								\|\| (pInstr->iP2 == PH7_MEMBER_WRITE` |
|  110965 | 2252 | `								    && !bStoreI && !bCoalI && !VmMemberNextIsWrite(pNextI));` |
|  115033 | 2253 | `							if( bBaseW ){` |
|     268 | 2254 | `								sxi32 rcInd = PH7_VmCheckIndirectModify(&(*pVm),pObjAttr->nIdx);` |
|     268 | 2255 | `								if( rcInd != SXRET_OK ){` |
|      10 | 2256 | `									VmBoundaryPark(&(*pVm),rcInd);` |
|      10 | 2257 | `									PH7_MemObjRelease(pTos);` |
|      10 | 2258 | `									pTos->nIdx = SXU32_HIGH;` |
|      10 | 2259 | `									PH7_ClassInstanceUnref(pThis);` |
|      10 | 2260 | `									VM_EXIT_BREAK;` |
|       - | 2261 | `								}` |
|     128 | 2262 | `							}` |
|       - | 2263 | `						}` |
|       - | 2264 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|       - | 2265 | `						 * We can only raise it on a real read, not when the slot is the` |
|       - | 2266 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|       - | 2267 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|       - | 2268 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|  115020 | 2269 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|   57828 | 2270 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|     631 | 2271 | `							VmInstr *pNext = pInstr + 1;` |
|     631 | 2272 | `							int bIsLhs = 0;` |
|     631 | 2273 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|     545 | 2274 | `								bIsLhs = 1;` |
|     270 | 2275 | `							}` |
|     631 | 2276 | `							if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 2277 | `								/* Destructuring target: the OP_LOAD_LIST store (typed-` |
|       - | 2278 | `								 * enforced) initializes it — never a read (php). */` |
|       7 | 2279 | `								bIsLhs = 1;` |
|       3 | 2280 | `							}` |
|       - | 2281 | ``							/* isset()/empty()/`??` read an uninitialized typed property`` |
|       - | 2282 | `							 * as "not set" — a silent miss, NOT the Error a plain read` |
|       - | 2283 | `							 * raises (php). The compiler tags such an access iP2 =` |
|       - | 2284 | `							 * ISSET/EMPTY; treat it like the assignment-LHS case and fall` |
|       - | 2285 | `							 * through to load the slot's NULL. */` |
|     631 | 2286 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|      28 | 2287 | `								bIsLhs = 1;` |
|      13 | 2288 | `							}` |
|       - | 2289 | ``							/* A WRITE base is not a read either. `$o->p ??= v` takes the`` |
|       - | 2290 | `							 * slot's NULL and assigns over it, and a DIMENSION write` |
|       - | 2291 | ``							 * (`$o->t['n'] = v`) AUTO-INITIALIZES an array when the`` |
|       - | 2292 | `							 * declared type has room for one -- php's` |
|       - | 2293 | `							 * zend_handle_fetch_obj_flags -- or refuses with its own` |
|       - | 2294 | `							 * TypeError when it does not. Raising the read Error here` |
|       - | 2295 | `							 * instead is what stopped Doctrine's ClassMetadata, whose` |
|       - | 2296 | ``							 * `public array $table;` is filled exactly that way. */`` |
|     631 | 2297 | `							if( (pInstr + 1)->iOp == PH7_OP_NULLC_JMP ){` |
|       3 | 2298 | ``								bIsLhs = 1;   /* `$o->p ??= v` assigns over the unset slot */`` |
|     630 | 2299 | `							}else if( VmMemberFetchIsRefSource(pInstr) ){` |
|       - | 2300 | `								/* A REFERENCE fetch has a rule of its own: php seeds NULL` |
|       - | 2301 | `								 * and binds when the declared type admits one, and refuses` |
|       - | 2302 | ``								 * with `Cannot access uninitialized non-nullable property`` |
|       - | 2303 | ``								 * ... by reference` when it does not. The auto-initialize`` |
|       - | 2304 | `								 * rule below belongs to a DIMENSION write, and running it` |
|       - | 2305 | ``								 * here turned `?int $t` into an ARRAY. */`` |
|      25 | 2306 | `								sxi32 rcRs = VmRefUninitTypedProperty(&(*pVm),pClass,pObjAttr,` |
|      16 | 2307 | `									(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pObjAttr->nIdx));` |
|      17 | 2308 | `								if( rcRs != SXRET_OK ){` |
|       9 | 2309 | `									VmBoundaryPark(&(*pVm),rcRs);` |
|       9 | 2310 | `									PH7_ClassInstanceUnref(pThis);` |
|       9 | 2311 | `									VM_EXIT_BREAK;` |
|       - | 2312 | `								}` |
|       9 | 2313 | `								bIsLhs = 1;` |
|     617 | 2314 | `							}else if( VmMemberFetchForWrite(pInstr) ){` |
|      17 | 2315 | `								sxi32 rcAI = VmAutoInitArrayProperty(&(*pVm),pObjAttr,` |
|      10 | 2316 | `									(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pObjAttr->nIdx));` |
|      12 | 2317 | `								if( rcAI != SXRET_OK ){` |
|       3 | 2318 | `									VmBoundaryPark(&(*pVm),rcAI);` |
|       3 | 2319 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 2320 | `									VM_EXIT_BREAK;` |
|       - | 2321 | `								}` |
|       9 | 2322 | `								bIsLhs = 1;` |
|       4 | 2323 | `							}` |
|     621 | 2324 | `							if( !bIsLhs ){` |
|      30 | 2325 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|      30 | 2326 | `								PH7_ClassInstanceUnref(pThis);` |
|      30 | 2327 | `								if( rcU == PH7_ABORT ){` |
|       3 | 2328 | `									VM_EXIT_ABORT;` |
|       - | 2329 | `								}` |
|       - | 2330 | `								{` |
|       - | 2331 | `									sxi32 iRp;` |
|      28 | 2332 | `									if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|       9 | 2333 | `										PH7_RESUME_DRAIN()` |
|       9 | 2334 | `										pc = iRp;` |
|       9 | 2335 | `										VM_EXIT_BREAK;` |
|       - | 2336 | `									}` |
|       - | 2337 | `								}` |
|      20 | 2338 | `								VM_EXIT_EXCEPTION;` |
|       - | 2339 | `							}` |
|     295 | 2340 | `						}` |
|       - | 2341 | `						/* Load attribute */` |
|  114989 | 2342 | `						pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|  114989 | 2343 | `						if( pValue ){` |
|  114989 | 2344 | `							if( pThis->iRef < 2 ){` |
|       - | 2345 | `								/* Perform a store operation,rather than a load operation since` |
|       - | 2346 | `								 * the class instance '$this' will be deleted shortly.` |
|       - | 2347 | `								 */` |
|     463 | 2348 | `								PH7_MemObjStore(pValue,pTos);` |
|     234 | 2349 | `							}else{` |
|       - | 2350 | `								/* Simple load */` |
|  114531 | 2351 | `								PH7_MemObjLoad(pValue,pTos);` |
|       - | 2352 | `							}` |
|  114989 | 2353 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|  114989 | 2354 | `								if( pThis->iRef > 1 ){` |
|       - | 2355 | `									/* Load attribute index */` |
|  114531 | 2356 | `									pTos->nIdx = pObjAttr->nIdx;` |
|  114526 | 2357 | `									if( VmMemberFetchIsRefSource(pInstr)` |
|   57339 | 2358 | `									 && (pObjAttr->iState & (VM_CLASS_ATTR_REFBOUND\|VM_CLASS_ATTR_REFSRCPIN)) == 0 ){` |
|       - | 2359 | `										/* This fetch is about to become a reference, so php makes` |
|       - | 2360 | `										 * THIS property a reference too. The table cannot name a` |
|       - | 2361 | `										 * property as a holder, so without a pin of its own the` |
|       - | 2362 | `										 * slot's only recorded holder is the other end's -- and` |
|       - | 2363 | ``										 * `$q->p =& $o->p` followed by $q's death freed the value`` |
|       - | 2364 | `										 * and left $o->p reading NULL. One counted pin, given back` |
|       - | 2365 | `										 * when the property is released; the bit makes it` |
|       - | 2366 | `										 * idempotent, so a source fetched in a loop pins once. */` |
|     132 | 2367 | `										pObjAttr->iState \|= VM_CLASS_ATTR_REFSRCPIN;` |
|     132 | 2368 | `										VmPinMemObjSlotCounted(&(*pVm),pObjAttr->nIdx);` |
|      65 | 2369 | `									}` |
|   57263 | 2370 | `								}` |
|   57492 | 2371 | `							}` |
|  114984 | 2372 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET)` |
|   58968 | 2373 | `							 && !VmMemberNativeSetKeepsSlot(pInstr) ){` |
|    2329 | 2374 | `								pTos->nIdx = SXU32_HIGH;` |
|    2329 | 2375 | `								pTos->iFlags \|= MEMOBJ_AUX_NATIVEPROP;` |
|    1162 | 2376 | `							}` |
|   57492 | 2377 | `						}` |
|  114989 | 2378 | `						if( pInstr->iP2 == PH7_MEMBER_ISSET ){` |
|       - | 2379 | `							/* isset() tests null-ness and nothing else, so reduce the loaded` |
|       - | 2380 | `							 * value to the same non-null marker the __isset and get-hook` |
|       - | 2381 | `							 * paths push. A property read off a TEMPORARY receiver` |
|       - | 2382 | ``							 * (`isset(f()->p)`, `isset((new C)->p)`, and now every`` |
|       - | 2383 | `							 * intermediate link of an accessor chain) leaves no variable` |
|       - | 2384 | `							 * index behind, and the trailing builtin read that as a` |
|       - | 2385 | `							 * CONSTANT and warned -- a diagnostic php has no equivalent of,` |
|       - | 2386 | `							 * its isset() being a language construct rather than a call. */` |
|      89 | 2387 | `							int bSet = (pTos->iFlags & MEMOBJ_NULL) == 0;` |
|      89 | 2388 | `							PH7_MemObjRelease(pTos);` |
|      89 | 2389 | `							if( bSet ){` |
|      64 | 2390 | `								pTos->x.iVal = 1;` |
|      64 | 2391 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      31 | 2392 | `							}` |
|      89 | 2393 | `							pTos->nIdx = SXU32_HIGH;` |
|      43 | 2394 | `						}` |
|   57497 | 2395 | `					}else{` |
|       - | 2396 | `						/* Inaccessible (private/protected) from this scope. php consults` |
|       - | 2397 | `						 * __get here exactly like a missing property (band A #3a) before` |
|       - | 2398 | `						 * erroring; the guard keeps a self-recursive read from looping. */` |
|      49 | 2399 | `						ph7_class_method *pGetMagic = 0;` |
|       - | 2400 | ``						/* A WRITE-context fetch (the base of `$o->p[k] = v`) reads through`` |
|       - | 2401 | `						 * __get in php too — the write then lands on the value it answered` |
|       - | 2402 | `						 * and is lost, with php's indirect-modification notice. PHL refused` |
|       - | 2403 | ``						 * the whole statement with `Cannot access private property` instead,`` |
|       - | 2404 | ``						 * a fatal on a program php runs. A plain store and a `??=` keep`` |
|       - | 2405 | `						 * their own paths below. */` |
|      46 | 2406 | `						if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      64 | 2407 | `						 && (VmMemberFetchForWrite(pInstr)` |
|      40 | 2408 | `						  \|\| (pInstr->iP2 != PH7_MEMBER_WRITE && !VmMemberNextIsWrite(pInstr + 1))) ){` |
|      43 | 2409 | `							pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|      20 | 2410 | `						}` |
|       - | 2411 | `						/* ++/--/compound-assign on a DECLARED but inaccessible property:` |
|       - | 2412 | `						 * php's accessors answer for it exactly as they do for a missing` |
|       - | 2413 | `						 * one (__get, modify, __set), where PHL raised` |
|       - | 2414 | ``						 * `Cannot access private property C::$n`. A plain store keeps its`` |
|       - | 2415 | `						 * own __set park below — it is a write, but not a read-modify-write. */` |
|      46 | 2416 | `						if( VmMemberNextIsRmw(pInstr + 1)` |
|      27 | 2417 | `						 && VmMagicRmwEligible(pVm,pClass,pThis,&sName) ){` |
|       - | 2418 | `							/* The name was already popped and pTos released above, so` |
|       - | 2419 | `							 * sName is the instruction's own literal here. */` |
|       - | 2420 | `							ph7_value sRmwVal;` |
|       - | 2421 | `							sxu32 nRmwScratch;` |
|       3 | 2422 | `							PH7_MemObjInit(pVm,&sRmwVal);` |
|       4 | 2423 | `							VmMagicRmwArm(&(*pVm),pThis,pClass,&sName,&sRmwVal,&nRmwScratch,` |
|       2 | 2424 | `								(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|       3 | 2425 | `							PH7_MemObjStore(&sRmwVal,pTos);` |
|       3 | 2426 | `							pTos->nIdx = nRmwScratch;` |
|       3 | 2427 | `							PH7_MemObjRelease(&sRmwVal);` |
|       3 | 2428 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 2429 | `							VM_EXIT_BREAK;` |
|       - | 2430 | `						}` |
|      47 | 2431 | `						if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 2432 | `							ph7_value sMagicRet;` |
|       9 | 2433 | `							PH7_MemObjInit(pVm,&sMagicRet);` |
|       9 | 2434 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       9 | 2435 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|       9 | 2436 | `							VmMagicGuardPop(pVm);` |
|       9 | 2437 | `							if( VmMemberFetchForWrite(pInstr) ){` |
|       5 | 2438 | `								PH7_VmOverloadedPropNotice(&(*pVm),pClass,&sName,&sMagicRet);` |
|       2 | 2439 | `							}` |
|       - | 2440 | `							/* The name was already popped and pTos released above; just` |
|       - | 2441 | `							 * take the magic result as the expression value. */` |
|       9 | 2442 | `							PH7_MemObjStore(&sMagicRet,pTos);` |
|       9 | 2443 | `							pTos->nIdx = SXU32_HIGH;` |
|       9 | 2444 | `							PH7_MemObjRelease(&sMagicRet);` |
|       9 | 2445 | `							PH7_ClassInstanceUnref(pThis);` |
|       9 | 2446 | `							VM_EXIT_BREAK;` |
|       - | 2447 | `						}` |
|      39 | 2448 | `						if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2449 | `							/* isset/empty on an inaccessible property: php consults __isset` |
|       - | 2450 | `							 * (band A #3b), and is silently false without it — never an` |
|       - | 2451 | `							 * Error (pre-fix PHL fataled here). */` |
|       3 | 2452 | `							ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|       3 | 2453 | `							int bSet = 0;` |
|       3 | 2454 | `							if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 2455 | `								ph7_value sIssetRet;` |
|     ! 0 | 2456 | `								PH7_MemObjInit(pVm,&sIssetRet);` |
|     ! 0 | 2457 | `								VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|     ! 0 | 2458 | `								PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|     ! 0 | 2459 | `								VmMagicGuardPop(pVm);` |
|     ! 0 | 2460 | `								PH7_MemObjToBool(&sIssetRet);` |
|     ! 0 | 2461 | `								bSet = sIssetRet.x.iVal != 0;` |
|     ! 0 | 2462 | `								PH7_MemObjRelease(&sIssetRet);` |
|     ! 0 | 2463 | `							}` |
|       3 | 2464 | `							if( pInstr->iP2 == PH7_MEMBER_COALESCE && pIssetMagic == 0 ){` |
|       - | 2465 | ``								/* `??` with no __isset to gate it: php reads straight through`` |
|       - | 2466 | `								 * __get, exactly as it does for a MISSING property. The` |
|       - | 2467 | `								 * __get dispatch just above this block is gated on a` |
|       - | 2468 | `								 * non-lookup context, so answer here. */` |
|     ! 0 | 2469 | `								ph7_class_method *pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|     ! 0 | 2470 | `								if( pCoalGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 2471 | `									ph7_value sCoalRet;` |
|     ! 0 | 2472 | `									PH7_MemObjInit(pVm,&sCoalRet);` |
|     ! 0 | 2473 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     ! 0 | 2474 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sCoalRet);` |
|     ! 0 | 2475 | `									VmMagicGuardPop(pVm);` |
|     ! 0 | 2476 | `									PH7_MemObjStore(&sCoalRet,pTos);` |
|     ! 0 | 2477 | `									pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 2478 | `									PH7_MemObjRelease(&sCoalRet);` |
|     ! 0 | 2479 | `								}` |
|     ! 0 | 2480 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2481 | `								VM_EXIT_BREAK;` |
|       - | 2482 | `							}` |
|       3 | 2483 | `							if( bSet ){` |
|     ! 0 | 2484 | `								if( VmMemberCtxWantsValue(pInstr->iP2) ){` |
|     ! 0 | 2485 | `									ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       - | 2486 | `									ph7_value sEmptyVal;` |
|     ! 0 | 2487 | `									PH7_MemObjInit(pVm,&sEmptyVal);` |
|     ! 0 | 2488 | `									if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|     ! 0 | 2489 | `										VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     ! 0 | 2490 | `										PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|     ! 0 | 2491 | `										VmMagicGuardPop(pVm);` |
|     ! 0 | 2492 | `									}` |
|     ! 0 | 2493 | `									PH7_MemObjStore(&sEmptyVal,pTos);` |
|     ! 0 | 2494 | `									pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 2495 | `									PH7_MemObjRelease(&sEmptyVal);` |
|     ! 0 | 2496 | `								}else{` |
|     ! 0 | 2497 | `									pTos->x.iVal = 1;` |
|     ! 0 | 2498 | `									MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     ! 0 | 2499 | `									pTos->nIdx = SXU32_HIGH;` |
|       - | 2500 | `								}` |
|     ! 0 | 2501 | `							}` |
|       3 | 2502 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 2503 | `							VM_EXIT_BREAK;` |
|       - | 2504 | `						}` |
|       - | 2505 | `						{` |
|       - | 2506 | `							/* A plain store to an inaccessible property dispatches __set` |
|       - | 2507 | `							 * (band A #3b): park the receiver+name for the following` |
|       - | 2508 | `							 * OP_STORE, exactly like the missing-property case. */` |
|      37 | 2509 | `							VmInstr *pNextW = pInstr + 1;` |
|      37 | 2510 | `							if( pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0 ){` |
|       3 | 2511 | `								ph7_class_method *pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|       3 | 2512 | `								if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){` |
|       3 | 2513 | `									pThis->iRef++;` |
|       3 | 2514 | `									pVm->pMagicSetThis = pThis;` |
|       3 | 2515 | `									SyBlobReset(&pVm->sMagicSetName);` |
|       3 | 2516 | `									SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       3 | 2517 | `									pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|       3 | 2518 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 2519 | `									VM_EXIT_BREAK;` |
|       - | 2520 | `								}` |
|     ! 0 | 2521 | `							}` |
|       - | 2522 | `						}` |
|      32 | 2523 | `						if( ((pInstr + 1)->iOp == PH7_OP_NULLC \|\| (pInstr + 1)->iOp == PH7_OP_NULLC_JMP)` |
|      19 | 2524 | `						 && pInstr->iP2 != PH7_MEMBER_WRITE ){` |
|       - | 2525 | `` 							/* `$o->priv ?? default` (OP_NULLC; NULLC_JMP is the `??=` `` |
|       - | 2526 | ``							 * pre-test): php treats `??` as an isset-style lookup — an`` |
|       - | 2527 | `							 * inaccessible property yields null SILENTLY (the` |
|       - | 2528 | `							 * __isset/__get consults already ran above), letting the` |
|       - | 2529 | `							 * coalesce pick the default. */` |
|     ! 0 | 2530 | `							PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2531 | `							VM_EXIT_BREAK; /* pTos is already the null temp */` |
|       - | 2532 | `						}` |
|       - | 2533 | `						/* A subclass reading a PARENT's PRIVATE property used to be` |
|       - | 2534 | `						 * special-cased into an "Undefined property" warning here. It is` |
|       - | 2535 | `						 * php's answer, but not because of the SCOPE: the base's private` |
|       - | 2536 | `						 * lives under its mangled storage name, so the subclass's plain` |
|       - | 2537 | `						 * lookup never reaches this point at all -- and reading the SAME` |
|       - | 2538 | `						 * property on an instance of the declaring class, which does, is` |
|       - | 2539 | `						 * php's ordinary visibility refusal. Deciding it from the scope` |
|       - | 2540 | `						 * turned that one into a warning too. */` |
|       - | 2541 | `						/* Genuinely inaccessible: php's CATCHABLE Error (parked on the` |
|       - | 2542 | `						 * boundary rail; the fetch-point router lands it — it was an` |
|       - | 2543 | `						 * uncatchable VmReportUncaughtException+Abort before). */` |
|       - | 2544 | `						{` |
|       - | 2545 | `						SyBlob sErrMsg;` |
|      35 | 2546 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      35 | 2547 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      35 | 2548 | `						SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",` |
|      16 | 2549 | `							zVis,&pClass->sDisp,&sName);` |
|      35 | 2550 | `						PH7_ClassInstanceUnref(pThis);` |
|      35 | 2551 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      35 | 2552 | `						SyBlobRelease(&sErrMsg);` |
|      35 | 2553 | `						VM_EXIT_BREAK;` |
|       - | 2554 | `						}` |
|       - | 2555 | `					}` |
|   57492 | 2556 | `				}` |
|       - | 2557 | `				/* Safely unreference the object */` |
|  116055 | 2558 | `				PH7_ClassInstanceUnref(pThis);` |
|       - | 2559 | `			}` |
|  137431 | 2560 | `		}else{` |
|     164 | 2561 | `			if( (pNos->iFlags & MEMOBJ_AUX_STROFFSET)` |
|      90 | 2562 | `			 && (pInstr->iP2 == PH7_MEMBER_WRITE \|\| pInstr->iP2 == PH7_MEMBER_UNSET` |
|       5 | 2563 | `			  \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET \|\| pInstr->iP2 == PH7_MEMBER_REF_TARGET` |
|       - | 2564 | ``			  /* A reference SOURCE (`$r =& $s[0]->p`) is compiled as a READ here on`` |
|       - | 2565 | `			   * purpose — php hands back a copy for a handler-backed property — so the` |
|       - | 2566 | `			   * bind that follows is what makes it a reach-inside, exactly as it does` |
|       - | 2567 | `			   * for a subscript. */` |
|       4 | 2568 | `			  \|\| (pInstr->iP2 == PH7_MEMBER_READ` |
|       3 | 2569 | `			      && ((pInstr + 1)->iOp == PH7_OP_STORE_REF` |
|       1 | 2570 | `			       \|\| (pInstr + 1)->iOp == PH7_OP_LOAD_REF` |
|     ! 0 | 2571 | `			       \|\| (pInstr + 1)->iOp == PH7_OP_STORE_IDX_REF))) ){` |
|       - | 2572 | `				/* The base is a string OFFSET and this reaches INSIDE it. php refuses every` |
|       - | 2573 | `				 * such reach and words the refusal from what is doing the reaching, so a` |
|       - | 2574 | ``				 * PROPERTY is `Cannot use string offset as an object` where a subscript is`` |
|       - | 2575 | ``				 * `... as an array` — `$s[0]->p = 1`, `$s[0]->p += 1`, `$s[0]->p ??= 1` and`` |
|       - | 2576 | ``				 * `unset($s[0]->p)` alike. PHL reported the generic non-object write`` |
|       - | 2577 | ``				 * (`Attempt to assign property "p" on string`) and said nothing at all for`` |
|       - | 2578 | ``				 * the unset. A METHOD CALL is not one of these: php keeps `Call to a member`` |
|       - | 2579 | ``				 * function p() on string` there, and so does the path below. */`` |
|       - | 2580 | `				sxi32 rcSo;` |
|      11 | 2581 | `				VmPopOperand(&pTos,1);` |
|      11 | 2582 | `				PH7_MemObjRelease(pTos);` |
|      11 | 2583 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      11 | 2584 | `				pTos->nIdx = SXU32_HIGH;` |
|      11 | 2585 | `				rcSo = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an object",` |
|       - | 2586 | `					sizeof("Cannot use string offset as an object")-1);` |
|      11 | 2587 | `				if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 | 2588 | `				rc = rcSo;` |
|      11 | 2589 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2590 | `			}` |
|       - | 2591 | ``			/* `->` on a non-object (e.g. a null intermediate). Silent in isset()/empty()/unset()`` |
|       - | 2592 | ``			 * context (iP2 2/3/4) so `isset($o->missing->x)` / `unset($o->missing->x)` match PHP. */`` |
|     156 | 2593 | `			if( pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2594 | `				/* php draws a sharp line here, and PH7 drew none: CALLING a method on a` |
|       - | 2595 | `				 * non-object is a catchable Error, while READING a property off one is a` |
|       - | 2596 | `				 * Warning that yields NULL. PH7 raised the same notice for both and` |
|       - | 2597 | ``				 * carried on with NULL, so `$null->m()` silently did nothing. */`` |
|       - | 2598 | `				SyString sMemb;` |
|     114 | 2599 | `				const char *zVerb = 0;` |
|     114 | 2600 | `				SyStringInitFromBuf(&sMemb,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     114 | 2601 | `				if( pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|       - | 2602 | ``					/* A deferred call ARGUMENT (`f($u->p)`): which diagnostic php raises`` |
|       - | 2603 | `					 * depends on the parameter, and this op does not know it — a by-VALUE` |
|       - | 2604 | `					 * binding is the read warning below, a by-REFERENCE one is php's` |
|       - | 2605 | ``					 * `Attempt to modify property` Error. Record the step rooted at the`` |
|       - | 2606 | `					 * base and let OP_CALL re-drive it in the mode the callee decides,` |
|       - | 2607 | `					 * exactly as a property MISSING on a real object is recorded. */` |
|      27 | 2608 | `					VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|      27 | 2609 | `					if( pPath && VmDeferPathPushProp(pPath,&sMemb) == SXRET_OK ){` |
|      27 | 2610 | `						VmPopOperand(&pTos,1);   /* drop the property name */` |
|      27 | 2611 | `						PH7_MemObjRelease(pTos); /* collapse the base slot into the carrier */` |
|      27 | 2612 | `						pTos->x.pOther = pPath;` |
|      27 | 2613 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      27 | 2614 | `						pTos->nIdx = SXU32_HIGH;` |
|      53 | 2615 | `						VM_EXIT_BREAK;` |
|       - | 2616 | `					}` |
|     ! 0 | 2617 | `					if( pPath ){` |
|     ! 0 | 2618 | `						VmFreeDeferredPath(pPath);` |
|     ! 0 | 2619 | `					}` |
|       - | 2620 | `					/* fall through to the immediate warning on allocation failure */` |
|     ! 0 | 2621 | `				}` |
|       - | 2622 | `				/* WRITING one is an Error too, and php picks its verb from what the` |
|       - | 2623 | `				 * write actually is. PH7 warned about a READ it never performed and` |
|       - | 2624 | `				 * then let the store fail into its own` |
|       - | 2625 | `				 * "Cannot perform assignment on a constant class attribute", so the` |
|       - | 2626 | `				 * script carried on past a statement php stops it for. The kind is` |
|       - | 2627 | `				 * read off the following instruction, the way every other write` |
|       - | 2628 | `				 * classification in this handler is (VmMemberFetchForWrite). */` |
|      88 | 2629 | `				if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|      33 | 2630 | `					const VmInstr *pNextW = pInstr + 1;` |
|      32 | 2631 | `					if( (pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0)` |
|      22 | 2632 | ``					 \|\| pNextW->iOp == PH7_OP_NULLC_JMP /* `$u->p ??= v` */`` |
|      12 | 2633 | `					 \|\| VmNextIsCompoundAssign(pNextW) ){` |
|      25 | 2634 | `						zVerb = "assign";` |
|      21 | 2635 | `					}else if( pNextW->iOp == PH7_OP_INCR \|\| pNextW->iOp == PH7_OP_DECR ){` |
|       5 | 2636 | `						zVerb = "increment/decrement";` |
|       3 | 2637 | `					}else{` |
|       - | 2638 | ``						/* The base of a subscript write (`$u->p[] = v`): php asks the`` |
|       - | 2639 | `						 * property for something to modify, and there is no property. */` |
|       5 | 2640 | `						zVerb = "modify";` |
|       1 | 2641 | `					}` |
|      72 | 2642 | `				}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       3 | 2643 | `					zVerb = "assign";` |
|      55 | 2644 | `				}else if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       3 | 2645 | `					zVerb = "modify";` |
|      53 | 2646 | `				}else if( VmMemberFetchIsRefSource(pInstr) ){` |
|       - | 2647 | ``					/* `$r =& $o->missing->p`: the intermediate is null and php asks it`` |
|       - | 2648 | ``					 * for something to bind to, which is its `modify` sentence -- not`` |
|       - | 2649 | `					 * the read warning this used to fall through to. */` |
|       3 | 2650 | `					zVerb = "modify";` |
|       1 | 2651 | `				}` |
|      88 | 2652 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD \|\| zVerb ){` |
|       - | 2653 | `					SyBlob sErrM;` |
|       - | 2654 | `					sxi32 rcErr;` |
|      53 | 2655 | `					SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      53 | 2656 | `					if( zVerb ){` |
|      39 | 2657 | `						SyBlobFormat(&sErrM,"Attempt to %s property \"%z\" on %s",` |
|      19 | 2658 | `							zVerb,&sMemb,VmArithValueName(pNos));` |
|      20 | 2659 | `					}else{` |
|      15 | 2660 | `						SyBlobFormat(&sErrM,"Call to a member function %z() on %s",` |
|       7 | 2661 | `							&sMemb,VmArithValueName(pNos));` |
|       - | 2662 | `					}` |
|      53 | 2663 | `					VmPopOperand(&pTos,1);` |
|      53 | 2664 | `					PH7_MemObjRelease(pTos);` |
|      53 | 2665 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      53 | 2666 | `					pTos->nIdx = SXU32_HIGH;` |
|      79 | 2667 | `					rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      26 | 2668 | `						SyBlobLength(&sErrM));` |
|      53 | 2669 | `					SyBlobRelease(&sErrM);` |
|      53 | 2670 | `					if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      53 | 2671 | `					rc = rcErr;` |
|      53 | 2672 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2673 | `				}` |
|      53 | 2674 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Attempt to read property \"%z\" on %s",` |
|      17 | 2675 | `					&sMemb,VmArithValueName(pNos));` |
|      17 | 2676 | `			}` |
|      78 | 2677 | `			VmPopOperand(&pTos,1);` |
|      78 | 2678 | `			PH7_MemObjRelease(pTos);` |
|      78 | 2679 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|       - | 2680 | `		}` |
|  137469 | 2681 | `	}else{` |
|       - | 2682 | `		/* Static member access using class name */` |
|  107958 | 2683 | `		pNos = pTos;` |
|  107958 | 2684 | `		pThis = 0;` |
|  107958 | 2685 | `		if( !pInstr->p3 ){` |
|    6116 | 2686 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    6116 | 2687 | `			pNos--;` |
|       - | 2688 | `#ifdef UNTRUST` |
|       - | 2689 | `			if( pNos < pStack ){` |
|       - | 2690 | `				VM_EXIT_ABORT;` |
|       - | 2691 | `			}` |
|       - | 2692 | `#endif` |
|    3053 | 2693 | `		}else{` |
|       - | 2694 | `			/* Attribute name already computed */` |
|  101847 | 2695 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|       - | 2696 | `		}` |
|  107958 | 2697 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|  107918 | 2698 | `			ph7_class *pClass = 0;` |
|       - | 2699 | `			/* A FORWARDING static call — self::/parent::/static:: — preserves the` |
|       - | 2700 | `			 * caller's late-static-binding class (php); a non-forwarding C::m() resets` |
|       - | 2701 | `			 * it to C. The receiver slot below keeps the literal keyword, which OP_CALL` |
|       - | 2702 | `			 * can't resolve, so it would fall back to the callee's DECLARING class and` |
|       - | 2703 | `			 * lose LSB. Remember the live LSB class here and stamp it onto the receiver` |
|       - | 2704 | `			 * after the method name is pushed. */` |
|  107918 | 2705 | `			int bForwardingCall = 0;` |
|  107918 | 2706 | `			ph7_class *pForwardLsb = 0;` |
|       - | 2707 | `			/* Same rail snapshot as OP_NEW: the lookup below can run an autoloader that` |
|       - | 2708 | `			 * throws, and php then reports that exception and nothing else. */` |
|  107918 | 2709 | `			sxi32 nMbBrc = pVm->nBoundaryRc;` |
|  107918 | 2710 | `			const void *pMbRes = (const void *)pVm->pResumeFrame;` |
|  107918 | 2711 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - | 2712 | `				/* Class already instantiated */` |
|      33 | 2713 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      33 | 2714 | `				pClass = pThis->pClass;` |
|      33 | 2715 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      18 | 2716 | `			}else{` |
|       - | 2717 | `				/* Try to extract the target class */` |
|  107888 | 2718 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|  107888 | 2719 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|  107888 | 2720 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|       - | 2721 | `					/* Handle self/static/parent keywords */` |
|  107888 | 2722 | `					if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       - | 2723 | `						/* In a trait method, self:: resolves to the USING class */` |
|    1505 | 2724 | `						pClass = PH7_VmPeekSelfClass(&(*pVm));` |
|    1505 | 2725 | `						bForwardingCall = 1;` |
|    1505 | 2726 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|  107138 | 2727 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|     164 | 2728 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|     164 | 2729 | `						bForwardingCall = 1;` |
|     164 | 2730 | `						pForwardLsb = pClass;` |
|  106308 | 2731 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|     188 | 2732 | `						pClass = PH7_VmResolveParentClass(&(*pVm));` |
|     188 | 2733 | `						bForwardingCall = 1;` |
|     188 | 2734 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|      96 | 2735 | `					}else{` |
|  106044 | 2736 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|       - | 2737 | `					}` |
|   53934 | 2738 | `				}` |
|       - | 2739 | `			}` |
|  107918 | 2740 | `			if( pClass == 0 ){` |
|       - | 2741 | `				/* Undefined class: php throws a catchable Error */` |
|       - | 2742 | `				SyBlob sErrM;` |
|       - | 2743 | `				sxi32 rcErr;` |
|      33 | 2744 | `				if( PH7_VmClassLookupRaised(&(*pVm),nMbBrc,pMbRes) ){` |
|       - | 2745 | `					/* ...unless an autoloader raised: land THAT instead of reporting the` |
|       - | 2746 | `					 * class missing on top of it. */` |
|      10 | 2747 | `					sxi32 rcAutoM = pVm->nBoundaryRc;` |
|      10 | 2748 | `					pVm->nBoundaryRc = 0;` |
|      10 | 2749 | `					if( !pInstr->p3 ){` |
|       8 | 2750 | `						VmPopOperand(&pTos,1);` |
|       3 | 2751 | `					}` |
|      10 | 2752 | `					PH7_MemObjRelease(pTos);` |
|      10 | 2753 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      10 | 2754 | `					pTos->nIdx = SXU32_HIGH;` |
|      10 | 2755 | `					if( rcAutoM == PH7_ABORT ){` |
|     ! 0 | 2756 | `						VM_EXIT_ABORT;` |
|       - | 2757 | `					}` |
|      10 | 2758 | `					rc = PH7_EXCEPTION;` |
|      10 | 2759 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2760 | `				}` |
|      24 | 2761 | `				SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      24 | 2762 | `				SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|      20 | 2763 | `					SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob));` |
|      24 | 2764 | `				if( !pInstr->p3 ){` |
|      21 | 2765 | `					VmPopOperand(&pTos,1);` |
|       9 | 2766 | `				}` |
|      24 | 2767 | `				PH7_MemObjRelease(pTos);` |
|      24 | 2768 | `				pTos->nIdx = SXU32_HIGH;` |
|      34 | 2769 | `				rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      10 | 2770 | `					SyBlobLength(&sErrM));` |
|      24 | 2771 | `				SyBlobRelease(&sErrM);` |
|      24 | 2772 | `				if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      24 | 2773 | `				rc = rcErr;` |
|      32 | 2774 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 2775 | `			}else{` |
|       - | 2776 | `				/* The static twin of the name rule at the top of this handler, and it` |
|       - | 2777 | `				 * runs HERE rather than up there because php resolves the CLASS first:` |
|       - | 2778 | ``				 * `NoSuchClass::${$arr}` is the class refusal alone, with no coercion`` |
|       - | 2779 | ``				 * and no `Array to string conversion` behind it, while the same name on`` |
|       - | 2780 | ``				 * a class that exists warns and then reports `Access to undeclared`` |
|       - | 2781 | ``				 * static property C::$Array`. A `::` METHOD name is refused the same way`` |
|       - | 2782 | `				 * an instance one is -- once the class is known. */` |
|  107890 | 2783 | `				if( !pInstr->p3 ){` |
|    6066 | 2784 | `					if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|    3224 | 2785 | `						if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 2786 | `							sxi32 rcMn;` |
|       3 | 2787 | `							VmPopOperand(&pTos,1);` |
|       3 | 2788 | `							PH7_MemObjRelease(pTos);` |
|       3 | 2789 | `							MemObjSetType(pTos,MEMOBJ_NULL);` |
|       3 | 2790 | `							pTos->nIdx = SXU32_HIGH;` |
|       3 | 2791 | `							if( pThis ){` |
|     ! 0 | 2792 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2793 | `								pThis = 0;` |
|     ! 0 | 2794 | `							}` |
|       3 | 2795 | `							rcMn = VmThrowFromVm(&(*pVm),"Error","Method name must be a string",` |
|       - | 2796 | `								sizeof("Method name must be a string")-1);` |
|       3 | 2797 | `							if( rcMn == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 | 2798 | `							rc = rcMn;` |
|       3 | 2799 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2800 | `						}` |
|    1606 | 2801 | `					}else{` |
|    2847 | 2802 | `						sxi32 rcNm = PH7_MemObjToStringUV(pTos);` |
|    2847 | 2803 | `						if( rcNm != SXRET_OK ){` |
|     ! 0 | 2804 | `							VmPopOperand(&pTos,1);` |
|     ! 0 | 2805 | `							PH7_MemObjRelease(pTos);` |
|     ! 0 | 2806 | `							MemObjSetType(pTos,MEMOBJ_NULL);` |
|     ! 0 | 2807 | `							pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 2808 | `							if( pThis ){` |
|     ! 0 | 2809 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2810 | `								pThis = 0;` |
|     ! 0 | 2811 | `							}` |
|     ! 0 | 2812 | `							if( rcNm == PH7_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 2813 | `							rc = rcNm;` |
|     ! 0 | 2814 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2815 | `						}` |
|       - | 2816 | `					}` |
|       - | 2817 | `					/* The coercion rewrote the slot the name was read from. */` |
|    6064 | 2818 | `					SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),` |
|       - | 2819 | `						SyBlobLength(&pTos->sBlob));` |
|    3022 | 2820 | `				}` |
|  107888 | 2821 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - | 2822 | `					/* Method call */` |
|    3222 | 2823 | `					ph7_class_method *pMeth = 0;` |
|    3222 | 2824 | `					if( sName.nByte > 0 ){` |
|       - | 2825 | `						/* An INTERFACE's methods are looked up too: they are all abstract,` |
|       - | 2826 | `						 * so the arm below reports php's "Cannot call abstract method` |
|       - | 2827 | `						 * I::m()" rather than claiming the name does not exist. */` |
|    3222 | 2828 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|    1601 | 2829 | `					}` |
|    3222 | 2830 | `					if( pMeth == 0 \|\| (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) ){` |
|      72 | 2831 | `						if( pMeth ){` |
|       - | 2832 | `							SyBlob sErrM;` |
|       - | 2833 | `							sxi32 rcErr;` |
|       5 | 2834 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       5 | 2835 | `							SyBlobFormat(&sErrM,"Cannot call abstract method %z::%z()",` |
|       2 | 2836 | `								&pClass->sDisp,&sName);` |
|       5 | 2837 | `							if( !pInstr->p3 ){` |
|       5 | 2838 | `								VmPopOperand(&pTos,1);` |
|       2 | 2839 | `							}` |
|       5 | 2840 | `							PH7_MemObjRelease(pTos);` |
|       5 | 2841 | `							pTos->nIdx = SXU32_HIGH;` |
|       - | 2842 | ``							/* The `$obj::m()` form took a reference on the receiver at the`` |
|       - | 2843 | `							 * top of this branch; every exit has to give it back, and only` |
|       - | 2844 | `							 * the fall-through at the end of the arm used to. */` |
|       5 | 2845 | `							if( pThis ){` |
|     ! 0 | 2846 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2847 | `								pThis = 0;` |
|     ! 0 | 2848 | `							}` |
|       7 | 2849 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       2 | 2850 | `								SyBlobLength(&sErrM));` |
|       5 | 2851 | `							SyBlobRelease(&sErrM);` |
|       5 | 2852 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       5 | 2853 | `							rc = rcErr;` |
|       5 | 2854 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 2855 | `						}else{` |
|      68 | 2856 | `							ph7_class_instance *pMagicThis = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|      68 | 2857 | `							ph7_class_method *pCallStaticMagic = pMagicThis` |
|      16 | 2858 | `								? PH7_ClassExtractMethod(pMagicThis->pClass,"__call",sizeof("__call")-1)` |
|      56 | 2859 | `								: PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1);` |
|      68 | 2860 | `							if( pCallStaticMagic ){` |
|       - | 2861 | `								/* php: C::missing(...) dispatches __callStatic($name,$args)` |
|       - | 2862 | `								 * through the packing body (see the instance twin) — or` |
|       - | 2863 | `								 * __call($name,$args) on the calling frame's own $this when` |
|       - | 2864 | `								 * that receiver fits C (VmStaticCallMagicThis). */` |
|      85 | 2865 | `								VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pMagicThis,` |
|      27 | 2866 | `									pMagicThis ? pMagicThis->pClass : pClass,&sName);` |
|      58 | 2867 | `								if( pPend == 0 ){` |
|     ! 0 | 2868 | `									VM_EXIT_ABORT;` |
|       - | 2869 | `								}` |
|      58 | 2870 | `								if( !pInstr->p3 ){` |
|      58 | 2871 | `									VmPopOperand(&pTos,1);` |
|      27 | 2872 | `								}` |
|      58 | 2873 | `								PH7_MemObjRelease(pTos);` |
|      58 | 2874 | `								if( pThis ){` |
|     ! 0 | 2875 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2876 | `									pThis = 0;` |
|     ! 0 | 2877 | `								}` |
|      58 | 2878 | `								pTos->x.pOther = pPend;` |
|      58 | 2879 | `								pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|      58 | 2880 | `								pTos->nIdx = SXU32_HIGH;` |
|      58 | 2881 | `								VM_EXIT_BREAK;` |
|       - | 2882 | `							}` |
|       - | 2883 | `							{` |
|       - | 2884 | `								/* php: the STATIC form reports the same "Call to undefined` |
|       - | 2885 | `								 * method C::m()" as the instance one. */` |
|       - | 2886 | `								SyBlob sErrM;` |
|       - | 2887 | `								sxi32 rcErr;` |
|      11 | 2888 | `								SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      11 | 2889 | `								SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",` |
|       5 | 2890 | `									&pClass->sDisp,&sName);` |
|      11 | 2891 | `								if( !pInstr->p3 ){` |
|      11 | 2892 | `									VmPopOperand(&pTos,1);` |
|       5 | 2893 | `								}` |
|      11 | 2894 | `								PH7_MemObjRelease(pTos);` |
|      11 | 2895 | `								pTos->nIdx = SXU32_HIGH;` |
|      11 | 2896 | `								if( pThis ){` |
|     ! 0 | 2897 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2898 | `									pThis = 0;` |
|     ! 0 | 2899 | `								}` |
|      16 | 2900 | `								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       5 | 2901 | `									SyBlobLength(&sErrM));` |
|      11 | 2902 | `								SyBlobRelease(&sErrM);` |
|      11 | 2903 | `								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 | 2904 | `								rc = rcErr;` |
|      11 | 2905 | `								PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2906 | `							}` |
|       - | 2907 | `						}` |
|       - | 2908 | `						/* Pop the method name from the stack */` |
|     ! 0 | 2909 | `						if( !pInstr->p3 ){` |
|     ! 0 | 2910 | `							VmPopOperand(&pTos,1);` |
|       - | 2911 | `						}` |
|     ! 0 | 2912 | `						PH7_MemObjRelease(pTos);` |
|     ! 0 | 2913 | `					}else{` |
|       - | 2914 | `						/* Inaccessible from this scope: php routes through the same fallback a` |
|       - | 2915 | ``						 * MISSING name takes — __call on a compatible `$this`, else`` |
|       - | 2916 | ``						 * __callStatic — which is why `C::privateStatic()` runs a catch-all`` |
|       - | 2917 | `						 * instead of the "Call to private method" Error OP_CALL would raise` |
|       - | 2918 | `						 * below. */` |
|    3154 | 2919 | `						ph7_class_method *pDeniedStatic = 0;` |
|    3154 | 2920 | `						ph7_class_instance *pDeniedThis = 0;` |
|    3154 | 2921 | `						int bDeniedStatic = 0;` |
|    3154 | 2922 | `						if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - | 2923 | `							/* The OWNING class decides, as in the instance twin above: a` |
|       - | 2924 | `							 * trait method's rules belong to the class that composed it. */` |
|      33 | 2925 | `							ph7_class *pOwnerCls = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|      33 | 2926 | `							if( !PH7_VmClassMemberAccess(&(*pVm),pOwnerCls,&sName,pMeth->iProtection,FALSE) ){` |
|      15 | 2927 | `								pDeniedThis = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|      15 | 2928 | `								pDeniedStatic = pDeniedThis` |
|       4 | 2929 | `									? PH7_ClassExtractMethod(pDeniedThis->pClass,"__call",` |
|       - | 2930 | `										sizeof("__call")-1)` |
|      12 | 2931 | `									: PH7_ClassExtractMethod(pClass,"__callStatic",` |
|       - | 2932 | `										sizeof("__callStatic")-1);` |
|      15 | 2933 | `								bDeniedStatic = pDeniedStatic == 0;` |
|       7 | 2934 | `							}` |
|      16 | 2935 | `						}` |
|    3154 | 2936 | `						if( bDeniedStatic ){` |
|       - | 2937 | `							/* No catch-all answers for it: php's visibility Error, raised at` |
|       - | 2938 | `							 * the resolution like the instance twin above. OP_CALL's own` |
|       - | 2939 | `							 * screen re-derives the method from the FUNCTION's name against` |
|       - | 2940 | `							 * its declaring class, which cannot see a trait adaptation's` |
|       - | 2941 | ``							 * composed protection — `sHi as private sPriv` ran from global`` |
|       - | 2942 | `							 * scope in silence. */` |
|       - | 2943 | `							SyBlob sErrM;` |
|       - | 2944 | `							sxi32 rcErr;` |
|       7 | 2945 | `							ph7_class *pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|       7 | 2946 | `							ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|      10 | 2947 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       3 | 2948 | `								? "private" : "protected";` |
|       7 | 2949 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       7 | 2950 | `							if( pScope ){` |
|       3 | 2951 | `								SyBlobFormat(&sErrM,"Call to %s method %z::%z() from scope %z",` |
|       1 | 2952 | `									zVis,&pOwner->sDisp,&sName,&pScope->sDisp);` |
|       2 | 2953 | `							}else{` |
|       5 | 2954 | `								SyBlobFormat(&sErrM,"Call to %s method %z::%z() from global scope",` |
|       2 | 2955 | `									zVis,&pOwner->sDisp,&sName);` |
|       - | 2956 | `							}` |
|       7 | 2957 | `							if( !pInstr->p3 ){` |
|       7 | 2958 | `								VmPopOperand(&pTos,1);` |
|       3 | 2959 | `							}` |
|       7 | 2960 | `							PH7_MemObjRelease(pTos);` |
|       7 | 2961 | `							pTos->nIdx = SXU32_HIGH;` |
|       7 | 2962 | `							if( pThis ){` |
|     ! 0 | 2963 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2964 | `								pThis = 0;` |
|     ! 0 | 2965 | `							}` |
|      10 | 2966 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       3 | 2967 | `								SyBlobLength(&sErrM));` |
|       7 | 2968 | `							SyBlobRelease(&sErrM);` |
|       7 | 2969 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       7 | 2970 | `							rc = rcErr;` |
|       7 | 2971 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2972 | `						}` |
|    3148 | 2973 | `						if( pDeniedStatic ){` |
|       - | 2974 | `							/* The packing body takes no receiver slot: drop the method-name` |
|       - | 2975 | `							 * slot so the MARK lands on the receiver slot, exactly as the` |
|       - | 2976 | `							 * missing-method twin above does (leaving the class name in place` |
|       - | 2977 | `							 * would pack it as the first $args entry). */` |
|      13 | 2978 | `							VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pDeniedThis,` |
|       4 | 2979 | `								pDeniedThis ? pDeniedThis->pClass : pClass,&sName);` |
|       9 | 2980 | `							if( pPend == 0 ){` |
|     ! 0 | 2981 | `								VM_EXIT_ABORT;` |
|       - | 2982 | `							}` |
|       9 | 2983 | `							if( !pInstr->p3 ){` |
|       9 | 2984 | `								VmPopOperand(&pTos,1);` |
|       4 | 2985 | `							}` |
|       9 | 2986 | `							PH7_MemObjRelease(pTos);` |
|       9 | 2987 | `							if( pThis ){` |
|     ! 0 | 2988 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2989 | `								pThis = 0;` |
|     ! 0 | 2990 | `							}` |
|       9 | 2991 | `							pTos->x.pOther = pPend;` |
|       9 | 2992 | `							pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|       9 | 2993 | `							pTos->nIdx = SXU32_HIGH;` |
|       9 | 2994 | `							VM_EXIT_BREAK;` |
|       - | 2995 | `						}` |
|       - | 2996 | ``						/* php refuses a NON-STATIC method named through `::` unless the`` |
|       - | 2997 | ``						 * CALLING frame holds a `$this` the class accepts: that is what`` |
|       - | 2998 | ``						 * makes `self::`/`parent::`/`static::` and `C::m()` from inside a`` |
|       - | 2999 | `						 * C method work, and every other spelling — from global scope,` |
|       - | 3000 | `` 						 * from a static method, from an unrelated class, and `$obj::m()` `` |
|       - | 3001 | ``						 * — an Error. PHL ran the body instead, with `$this` unset or`` |
|       - | 3002 | ``						 * (for the object form) bound to the object the `::` was written`` |
|       - | 3003 | `						 * on, which php never uses: it takes the CALLER's. The message` |
|       - | 3004 | ``						 * names the DECLARING class, so `D::m()` reports B::m(). */`` |
|    3140 | 3005 | `						if( (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|     156 | 3006 | `							ph7_class_instance *pCallerThis = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|     156 | 3007 | `							if( pCallerThis == 0 ){` |
|       - | 3008 | `								SyBlob sErrM;` |
|       - | 3009 | `								sxi32 rcErr;` |
|      13 | 3010 | `								ph7_class *pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|      13 | 3011 | `								SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      13 | 3012 | `								SyBlobFormat(&sErrM,"Non-static method %z::%z() cannot be called statically",` |
|       6 | 3013 | `									&pOwner->sDisp,&pMeth->sFunc.sName);` |
|      13 | 3014 | `								if( !pInstr->p3 ){` |
|      13 | 3015 | `									VmPopOperand(&pTos,1);` |
|       6 | 3016 | `								}` |
|      13 | 3017 | `								PH7_MemObjRelease(pTos);` |
|      13 | 3018 | `								pTos->nIdx = SXU32_HIGH;` |
|      13 | 3019 | `								if( pThis ){` |
|       3 | 3020 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 3021 | `									pThis = 0;` |
|       1 | 3022 | `								}` |
|      19 | 3023 | `								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       6 | 3024 | `									SyBlobLength(&sErrM));` |
|      13 | 3025 | `								SyBlobRelease(&sErrM);` |
|      13 | 3026 | `								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      13 | 3027 | `								rc = rcErr;` |
|      13 | 3028 | `								PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 3029 | `							}` |
|       - | 3030 | `` 							/* The receiver is that `$this` — never the object the `::` `` |
|       - | 3031 | `							 * was written on, and never nothing. Naming it here is also` |
|       - | 3032 | `							 * what makes the call work inside a CLOSURE declared in a` |
|       - | 3033 | ``							 * method: php gives such a closure a `$this`, PHL carries it`` |
|       - | 3034 | `							 * as a frame variable rather than on the frame itself, and` |
|       - | 3035 | ``							 * OP_CALL reads only the frame, so `self::m()` in a closure`` |
|       - | 3036 | `							 * used to run with no receiver at all. */` |
|     144 | 3037 | `							PH7_MemObjRelease(pNos);` |
|     144 | 3038 | `							pNos->x.pOther = pCallerThis;` |
|     144 | 3039 | `							MemObjSetType(pNos,MEMOBJ_OBJ);` |
|     144 | 3040 | `							pCallerThis->iRef++;` |
|     144 | 3041 | `							pNos->nIdx = SXU32_HIGH;` |
|      70 | 3042 | `						}` |
|       - | 3043 | `						/* Push method name on the stack, MARKED as already screened (see the` |
|       - | 3044 | `						 * instance twin: OP_CALL cannot re-derive a composed protection). */` |
|    3128 | 3045 | `						PH7_MemObjRelease(pTos);` |
|    3128 | 3046 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|    3128 | 3047 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|    3128 | 3048 | `						pTos->iFlags \|= MEMOBJ_AUX_MEMBERCALL;` |
|       - | 3049 | `					}` |
|    3128 | 3050 | `					pTos->nIdx = SXU32_HIGH;` |
|       - | 3051 | `					/* Forwarding call (self::/parent::/static::): overwrite the receiver` |
|       - | 3052 | `					 * slot (pNos, one below the method name) with the live LSB class name` |
|       - | 3053 | ``					 * so OP_CALL pushes THAT onto aSelf — preserving `static::` inside the`` |
|       - | 3054 | `					 * callee. Without this the literal keyword falls through to the` |
|       - | 3055 | `					 * callee's declaring class. Only when an LSB class is actually in` |
|       - | 3056 | `					 * scope (a static call from global scope has none). */` |
|    3128 | 3057 | `					if( bForwardingCall && pForwardLsb && (pNos->iFlags & MEMOBJ_STRING) ){` |
|      90 | 3058 | `						SyBlobReset(&pNos->sBlob);` |
|      90 | 3059 | `						SyBlobAppend(&pNos->sBlob,pForwardLsb->sName.zString,pForwardLsb->sName.nByte);` |
|      44 | 3060 | `					}` |
|    1559 | 3061 | `				}else{` |
|       - | 3062 | `					/* Attribute access */` |
|  104671 | 3063 | `					ph7_class_attr *pAttr = 0;` |
|  104671 | 3064 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - | 3065 | `						/* unset(C::$x): php refuses it, unconditionally and before any` |
|       - | 3066 | `						 * lookup -- an undeclared name, a private one and one reached` |
|       - | 3067 | `						 * through a subclass all read the same, and the class named is` |
|       - | 3068 | ``						 * whatever the `::` resolved to (so `parent::$b` says Base and`` |
|       - | 3069 | ``						 * `Child::$b` says Child even when Base declares it). Only the`` |
|       - | 3070 | `						 * class NAME is resolved first, so an unknown one still answers` |
|       - | 3071 | ``						 * `Class "X" not found`.`` |
|       - | 3072 | `						 *` |
|       - | 3073 | `						 * The refusal is php's CATCHABLE Error; PHL reported it uncaught` |
|       - | 3074 | ``						 * and ABORTED, so `try { unset(C::$s); } catch (Throwable)` never`` |
|       - | 3075 | `						 * ran its catch and everything after the try was dropped with exit` |
|       - | 3076 | `						 * status 0. Parked on the boundary rail like the other refusals in` |
|       - | 3077 | `						 * this arm, with the op completing as a NULL that carries no slot` |
|       - | 3078 | `						 * index -- the trailing generic unset() then has nothing to clear,` |
|       - | 3079 | `						 * which is what stops it silently NULLing (and de-typing) the` |
|       - | 3080 | `						 * shared static slot on the way out.` |
|       - | 3081 | `						 *` |
|       - | 3082 | `						 * The name is the DISPLAY name: an anonymous class's identity` |
|       - | 3083 | ``						 * carries php's NUL-separated `class@anonymous\0file:line$hash`,`` |
|       - | 3084 | `						 * and formatting that through a C string truncated the message at` |
|       - | 3085 | ``						 * the NUL -- it lost `::$x` entirely. */`` |
|       - | 3086 | `						SyBlob sErrUn;` |
|      37 | 3087 | `						SyBlobInit(&sErrUn,&pVm->sAllocator);` |
|      37 | 3088 | `						SyBlobFormat(&sErrUn,"Attempt to unset static property %z::$%z",` |
|      18 | 3089 | `							&pClass->sDisp,&sName);` |
|      37 | 3090 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",` |
|       - | 3091 | `							sizeof("Error")-1,&sErrUn));` |
|      37 | 3092 | `						if( !pInstr->p3 ){` |
|       7 | 3093 | `							VmPopOperand(&pTos,1);` |
|       3 | 3094 | `						}` |
|      37 | 3095 | `						PH7_MemObjRelease(pTos);` |
|      37 | 3096 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      37 | 3097 | `						pTos->nIdx = SXU32_HIGH;` |
|      37 | 3098 | `						if( pThis ){` |
|       5 | 3099 | `							PH7_ClassInstanceUnref(pThis);` |
|       2 | 3100 | `						}` |
|      37 | 3101 | `						VM_EXIT_BREAK;` |
|       - | 3102 | `					}` |
|       - | 3103 | `					/* Check for special ::class pseudo-constant */` |
|  105169 | 3104 | `					if( sName.nByte == sizeof("class")-1 &&` |
|    1068 | 3105 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|       - | 3106 | `						/* ::class returns the fully qualified class name */` |
|       - | 3107 | `						/* Pop the attribute name from the stack */` |
|     212 | 3108 | `						if( !pInstr->p3 ){` |
|     212 | 3109 | `							VmPopOperand(&pTos,1);` |
|     104 | 3110 | `						}` |
|     212 | 3111 | `						PH7_MemObjRelease(pTos);` |
|       - | 3112 | `						/* Load the class name */` |
|     212 | 3113 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|     212 | 3114 | `						pTos->nIdx = SXU32_HIGH;` |
|     108 | 3115 | `					}else{` |
|       - | 3116 | `						/* Extract the target attribute. php keeps constants and` |
|       - | 3117 | `						 * (static) properties in separate namespaces; the source` |
|       - | 3118 | ``						 * form disambiguates (set by the `::` codegen, compile.c):`` |
|       - | 3119 | ``						 * a `$`-form is a STATIC PROPERTY (hAttr) — either a literal`` |
|       - | 3120 | ``						 * name folded into pInstr->p3 (`C::$s`) or a dynamic name on`` |
|       - | 3121 | ``						 * the stack marked iP1==2 (`C::$$x`, `C::${$e}`); a bareword`` |
|       - | 3122 | `						 * (p3==0, iP1==1) is a CONSTANT or enum case (hConst). This` |
|       - | 3123 | ``						 * is what lets `const C` and `public $C` coexist and resolve`` |
|       - | 3124 | `						 * to the right member. */` |
|       - | 3125 | `						/* php gives a class CONSTANT no silent-lookup mode at all: there is` |
|       - | 3126 | ``						 * no BP_VAR_IS fetch for one, so `empty(C::K)` and `C::K ?? $d` raise`` |
|       - | 3127 | `						 * the same Error a plain read does -- undefined OR inaccessible --` |
|       - | 3128 | `						 * where the same two shapes over a static PROPERTY answer quietly.` |
|       - | 3129 | `						 * PHL silenced both, so a typo'd or private class constant read` |
|       - | 3130 | ``						 * through `??` handed back the default. (isset() over one is a php`` |
|       - | 3131 | `						 * COMPILE error, so it never reaches this.) */` |
|  104427 | 3132 | `						int bConstForm = (pInstr->p3 == 0 && pInstr->iP1 != 2);` |
|  104427 | 3133 | `						if( sName.nByte > 0 ){` |
|  104427 | 3134 | `							pAttr = bConstForm` |
|    2618 | 3135 | `								? PH7_ClassExtractConstant(pClass,sName.zString,sName.nByte)` |
|  103113 | 3136 | `								: VmClassAttrWithShadow(pClass,sName.zString,sName.nByte);` |
|   52211 | 3137 | `						}` |
|  104422 | 3138 | `						if( pAttr && bConstForm && (pClass->iFlags & PH7_CLASS_TRAIT) != 0` |
|    1308 | 3139 | `						 && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 3140 | `							/* A trait constant belongs to the classes that COMPOSE the trait` |
|       - | 3141 | ``							 * and to no one else: php refuses `T::K` outright, from inside a`` |
|       - | 3142 | `							 * trait method as readily as from outside. (PHP 8.2 introduced` |
|       - | 3143 | `							 * trait constants; this refusal came with them.) */` |
|       - | 3144 | `							SyBlob sErrTr;` |
|       3 | 3145 | `							SyBlobInit(&sErrTr,&pVm->sAllocator);` |
|       3 | 3146 | `							SyBlobFormat(&sErrTr,"Cannot access trait constant %z::%z directly",` |
|       1 | 3147 | `								&pClass->sDisp,&pAttr->sName);` |
|       3 | 3148 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",` |
|       - | 3149 | `								sizeof("Error")-1,&sErrTr));` |
|       3 | 3150 | `							pAttr = 0;` |
|       3 | 3151 | `							bStaticHidden = 1;` |
|       1 | 3152 | `						}` |
|  104422 | 3153 | `						if( pAttr && !bConstForm` |
|  103102 | 3154 | `						 && (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT))` |
|   50898 | 3155 | `						     != PH7_CLASS_ATTR_STATIC ){` |
|       - | 3156 | ``							/* php's `::$name` reads the class's PROPERTY table -- which holds`` |
|       - | 3157 | `							 * the INSTANCE properties and the constants too -- and answers in` |
|       - | 3158 | `							 * a fixed order: visibility first, then static-ness. So a private` |
|       - | 3159 | ``							 * instance property is `Cannot access private property H::$pi`,`` |
|       - | 3160 | ``							 * a public one and a CONSTANT named with the `$` form are`` |
|       - | 3161 | ``							 * `Access to undeclared static property H::$inst`, and neither`` |
|       - | 3162 | `							 * ever yields a value: the static table simply has no such row.` |
|       - | 3163 | `							 * PHL matched only the constant case; an instance property` |
|       - | 3164 | ``							 * reached through `::` printed PH7's own uncatchable "Access to a`` |
|       - | 3165 | `							 * non-static class attribute ... PH7 is loading NULL" and CARRIED` |
|       - | 3166 | ``							 * ON -- reading null, and letting `H::$inst = 'w'` report success`` |
|       - | 3167 | `							 * for a write php refuses. Fold it into the not-found arm below,` |
|       - | 3168 | `							 * raising the visibility refusal first when that is what php` |
|       - | 3169 | `							 * answers. */` |
|       8 | 3170 | `							if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|       9 | 3171 | `							 && !PH7_VmClassAttrAccess(&(*pVm),pClass,pAttr,FALSE) ){` |
|       - | 3172 | `								SyBlob sErrVis;` |
|       3 | 3173 | `								SyBlobInit(&sErrVis,&pVm->sAllocator);` |
|       2 | 3174 | `								SyBlobFormat(&sErrVis,"Cannot access %s property %z::$%z",` |
|       2 | 3175 | `									pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected",` |
|       1 | 3176 | `									&pClass->sDisp,&pAttr->sName);` |
|       3 | 3177 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",` |
|       - | 3178 | `									sizeof("Error")-1,&sErrVis));` |
|       3 | 3179 | `								bStaticHidden = 1;` |
|       1 | 3180 | `							}` |
|       9 | 3181 | `							pAttr = 0;` |
|       4 | 3182 | `						}` |
|  104427 | 3183 | `						if( pAttr == 0 && !bStaticHidden ){` |
|       - | 3184 | `							/* No such member. php raises a catchable Error whose wording` |
|       - | 3185 | `							 * depends on the ACCESS form (the same p3/iP1 signal used above):` |
|       - | 3186 | ``							 * a bareword `C::MISSING` (constant form) is "Undefined constant`` |
|       - | 3187 | ``							 * C::MISSING", a `$`-form `C::$missing` is "Access to undeclared`` |
|       - | 3188 | `							 * static property C::$missing". Instance magic (__get) is never` |
|       - | 3189 | `							 * consulted for statics (band A #3b). isset()/empty() context` |
|       - | 3190 | `							 * stays silently false. Parked on the boundary rail; the op` |
|       - | 3191 | `							 * completes benignly with NULL and the fetch-point router lands` |
|       - | 3192 | `							 * the throw. */` |
|      31 | 3193 | `							if( bConstForm \|\| !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 3194 | `								SyBlob sErrMsg;` |
|      31 | 3195 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      31 | 3196 | `								if( bConstForm ){` |
|      17 | 3197 | `									SyBlobFormat(&sErrMsg,"Undefined constant %z::%z",` |
|       7 | 3198 | `										&pClass->sDisp,&sName);` |
|      10 | 3199 | `								}else{` |
|      16 | 3200 | `									SyBlobFormat(&sErrMsg,"Access to undeclared static property %z::$%z",` |
|       7 | 3201 | `										&pClass->sDisp,&sName);` |
|       - | 3202 | `								}` |
|      31 | 3203 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      14 | 3204 | `							}` |
|      14 | 3205 | `						}` |
|       - | 3206 | `						/* Pop the attribute name from the stack */` |
|  104427 | 3207 | `						if( !pInstr->p3 ){` |
|    2633 | 3208 | `							VmPopOperand(&pTos,1);` |
|    1314 | 3209 | `						}` |
|  104427 | 3210 | `						PH7_MemObjRelease(pTos);` |
|  104427 | 3211 | `						pTos->nIdx = SXU32_HIGH;` |
|  104427 | 3212 | `						if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - | 3213 | ``							/* `self::$s =& $x` / `C::$s =& $x`: stash the static property slot`` |
|       - | 3214 | `							 * for the following member-marked OP_STORE_REF (class-level, shared` |
|       - | 3215 | `							 * across instances — matches php). Skip the read machinery below. */` |
|      16 | 3216 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|      16 | 3217 | `							 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|      18 | 3218 | `							 && VmClassStaticDeferPending(pClass) ){` |
|       - | 3219 | `								/* Binding a reference TO a static touches the table, so it` |
|       - | 3220 | `								 * materializes first, like every other static access. */` |
|       3 | 3221 | `								sxi32 rcRt = PH7_VmMaterializeClassStatics(&(*pVm),pClass);` |
|       3 | 3222 | `								if( rcRt != SXRET_OK ){` |
|       3 | 3223 | `									pVm->pRefTargetStaticAttr = 0;` |
|       3 | 3224 | `									pVm->pRefTargetAttr = 0;` |
|       3 | 3225 | `									pVm->pRefTargetThis = 0;` |
|       3 | 3226 | `									if( pThis ){` |
|     ! 0 | 3227 | `										PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3228 | `									}` |
|       3 | 3229 | `									if( rcRt == PH7_ABORT ){` |
|     ! 0 | 3230 | `										VM_EXIT_ABORT;` |
|       - | 3231 | `									}` |
|       - | 3232 | `									{` |
|       - | 3233 | `										sxi32 iRpR;` |
|       3 | 3234 | `										if( VmRecordedResume(pVm,&iRpR,pState->pEntryFrame,aInstr) ){` |
|       7 | 3235 | `											PH7_RESUME_DRAIN()` |
|       3 | 3236 | `											pc = iRpR;` |
|       3 | 3237 | `											VM_EXIT_BREAK;` |
|       - | 3238 | `										}` |
|       - | 3239 | `									}` |
|     ! 0 | 3240 | `									VM_EXIT_EXCEPTION;` |
|       - | 3241 | `								}` |
|     ! 0 | 3242 | `							}` |
|      14 | 3243 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|      16 | 3244 | `							 && PH7_VmClassAttrAccess(&(*pVm),pClass,pAttr,FALSE) ){` |
|      16 | 3245 | `								pVm->pRefTargetStaticAttr = pAttr;` |
|      16 | 3246 | `								pVm->pRefTargetAttr = 0;` |
|      16 | 3247 | `								pVm->pRefTargetThis = 0;` |
|      16 | 3248 | `								pTos->nIdx = pAttr->nIdx;` |
|       9 | 3249 | `							}else{` |
|     ! 0 | 3250 | `								pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 3251 | `								pVm->pRefTargetAttr = 0;` |
|     ! 0 | 3252 | `								pVm->pRefTargetThis = 0;` |
|       - | 3253 | `							}` |
|      16 | 3254 | `							if( pThis ){` |
|     ! 0 | 3255 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3256 | `							}` |
|      16 | 3257 | `							VM_EXIT_BREAK;` |
|       - | 3258 | `						}` |
|  104411 | 3259 | `						if( pAttr ){` |
|       - | 3260 | `							{` |
|       - | 3261 | `								ph7_value *pValue;` |
|       - | 3262 | `								/* php materializes the class's static table at the FIRST` |
|       - | 3263 | `								 * static-property access (any property, any context — read,` |
|       - | 3264 | `								 * write, even isset): a default whose evaluation was DEFERRED` |
|       - | 3265 | `								 * (it threw at the declaration) runs here, and a typed one that` |
|       - | 3266 | `								 * failed its check throws its catchable TypeError here. Class` |
|       - | 3267 | `								 * constants and static method calls do not trigger the` |
|       - | 3268 | `								 * materialization (php-exact). */` |
|  104374 | 3269 | `								if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|  103073 | 3270 | `								 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|  101777 | 3271 | `								 && VmClassStaticDeferPending(pClass) ){` |
|      48 | 3272 | `									sxi32 rcD = PH7_VmMaterializeClassStatics(&(*pVm),pClass);` |
|      48 | 3273 | `									if( rcD != SXRET_OK ){` |
|      44 | 3274 | `										if( pThis ){` |
|     ! 0 | 3275 | `											PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3276 | `										}` |
|      44 | 3277 | `										if( rcD == PH7_ABORT ){` |
|     ! 0 | 3278 | `											VM_EXIT_ABORT;` |
|       - | 3279 | `										}` |
|       - | 3280 | `										{` |
|       - | 3281 | `											sxi32 iRpD;` |
|      44 | 3282 | `											if( VmRecordedResume(pVm,&iRpD,pState->pEntryFrame,aInstr) ){` |
|      84 | 3283 | `												PH7_RESUME_DRAIN()` |
|      40 | 3284 | `												pc = iRpD;` |
|      40 | 3285 | `												VM_EXIT_BREAK;` |
|       - | 3286 | `											}` |
|       - | 3287 | `										}` |
|       5 | 3288 | `										VM_EXIT_EXCEPTION;` |
|       - | 3289 | `									}` |
|       2 | 3290 | `								}` |
|       - | 3291 | `								/* Check if the access to the attribute is allowed */` |
|  104337 | 3292 | `								if( PH7_VmClassAttrAccess(&(*pVm),pClass,pAttr,FALSE) ){` |
|       - | 3293 | `									/* PHP 7.4+: uninitialized typed static read.` |
|       - | 3294 | `									 * Same LHS-of-store peek as the instance path. */` |
|  104308 | 3295 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|  102260 | 3296 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|  150224 | 3297 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|  100146 | 3298 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|  100151 | 3299 | `										if( pS ){` |
|  100151 | 3300 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|  100151 | 3301 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|  100033 | 3302 | `												VmInstr *pNext = pInstr + 1;` |
|  100033 | 3303 | `												int bIsLhs = 0;` |
|  100033 | 3304 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|      10 | 3305 | `													bIsLhs = 1;` |
|       4 | 3306 | `												}` |
|  100033 | 3307 | `												if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 3308 | `													/* Destructuring target ([S::$s] = [...]):` |
|       - | 3309 | `													 * initialized by the OP_LOAD_LIST store. */` |
|       3 | 3310 | `													bIsLhs = 1;` |
|       1 | 3311 | `												}` |
|       - | 3312 | ``												/* isset()/empty()/`??` ask whether the property`` |
|       - | 3313 | `												 * HAS a value and read an uninitialized one as` |
|       - | 3314 | `												 * "not set" -- a silent miss, not the Error a` |
|       - | 3315 | `												 * plain read raises. The instance path has had` |
|       - | 3316 | `												 * this since typed properties landed; the STATIC` |
|       - | 3317 | ``												 * one never did, so `isset(C::$n)` threw where php`` |
|       - | 3318 | ``												 * answers false. (`??=` is the NULLC_JMP branch`` |
|       - | 3319 | `												 * below -- it is a write base, not a lookup.) */` |
|  100033 | 3320 | `												if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|      13 | 3321 | `													bIsLhs = 1;` |
|       6 | 3322 | `												}` |
|       - | 3323 | `												/* And a WRITE base is not a read: same` |
|       - | 3324 | `												 * auto-initialize rule as the instance path. A` |
|       - | 3325 | ``												 * read-MODIFY-write (`C::$n++`, `.=`) is not one`` |
|       - | 3326 | `												 * of these -- it reads the property first, so` |
|       - | 3327 | `												 * php's Error stands. */` |
|  100033 | 3328 | `												if( (pInstr + 1)->iOp == PH7_OP_NULLC_JMP ){` |
|       3 | 3329 | `													bIsLhs = 1;` |
|  100032 | 3330 | `												}else if( VmMemberFetchIsRefSource(pInstr) ){` |
|       - | 3331 | `													/* A reference fetch has the instance path's own` |
|       - | 3332 | `													 * rule: NULL and bind when the type admits it,` |
|       - | 3333 | `													 * php's by-reference refusal when it does not. */` |
|       4 | 3334 | `													sxi32 rcRs = VmRefUninitTypedProperty(&(*pVm),pClass,pV,` |
|       1 | 3335 | `														(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx));` |
|       3 | 3336 | `													if( rcRs != SXRET_OK ){` |
|       3 | 3337 | `														VmBoundaryPark(&(*pVm),rcRs);` |
|       3 | 3338 | `														if( pThis ){` |
|     ! 0 | 3339 | `															PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3340 | `														}` |
|       3 | 3341 | `														VM_EXIT_BREAK;` |
|       - | 3342 | `													}` |
|     ! 0 | 3343 | `													bIsLhs = 1;` |
|  100029 | 3344 | `												}else if( VmMemberFetchForWrite(pInstr) ){` |
|       4 | 3345 | `													sxi32 rcAI = VmAutoInitArrayProperty(&(*pVm),pV,` |
|       1 | 3346 | `														(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx));` |
|       3 | 3347 | `													if( rcAI != SXRET_OK ){` |
|     ! 0 | 3348 | `														VmBoundaryPark(&(*pVm),rcAI);` |
|     ! 0 | 3349 | `														if( pThis ){` |
|     ! 0 | 3350 | `															PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3351 | `														}` |
|     ! 0 | 3352 | `														VM_EXIT_BREAK;` |
|       - | 3353 | `													}` |
|       3 | 3354 | `													bIsLhs = 1;` |
|       1 | 3355 | `												}` |
|  100031 | 3356 | `												if( !bIsLhs ){` |
|  100004 | 3357 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|  100004 | 3358 | `													if( pThis ){` |
|     ! 0 | 3359 | `														PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3360 | `													}` |
|  100004 | 3361 | `													if( rcU == PH7_ABORT ){` |
|     ! 0 | 3362 | `														VM_EXIT_ABORT;` |
|       - | 3363 | `													}` |
|       - | 3364 | `													{` |
|       - | 3365 | `														sxi32 iRp;` |
|  100004 | 3366 | `														if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|  200004 | 3367 | `															PH7_RESUME_DRAIN()` |
|  100004 | 3368 | `															pc = iRp;` |
|  100004 | 3369 | `															VM_EXIT_BREAK;` |
|       - | 3370 | `														}` |
|       - | 3371 | `													}` |
|     ! 0 | 3372 | `													VM_EXIT_EXCEPTION;` |
|       - | 3373 | `												}` |
|      13 | 3374 | `											}` |
|      71 | 3375 | `										}` |
|      71 | 3376 | `									}` |
|    4304 | 3377 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|    3452 | 3378 | `									 && SySetUsed(&pAttr->aAttrs) > 0 ){` |
|       - | 3379 | `										/* php 8.4 #[\Deprecated] on a class constant:` |
|       - | 3380 | `										 * every access re-warns. */` |
|      14 | 3381 | `										VmDeprecatedConstNotice(&(*pVm),pClass,pAttr);` |
|       6 | 3382 | `									}` |
|    4309 | 3383 | `									if( pAttr->nIdx == SXU32_HIGH ){` |
|       - | 3384 | `										/* Unmaterialized slot. Enum case: first access` |
|       - | 3385 | `										 * materializes ALL the singletons. Plain constant: its` |
|       - | 3386 | `										 * initializer hasn't run yet (mount-order-dependent` |
|       - | 3387 | `										 * cross-constant reference) — evaluate on demand. A` |
|       - | 3388 | `										 * raised TypeError/Error (backing mismatch, duplicate` |
|       - | 3389 | `										 * value, self-reference) parks on the boundary rail;` |
|       - | 3390 | `										 * the op completes benignly with NULL and the` |
|       - | 3391 | `										 * fetch-point router lands the throw. */` |
|       - | 3392 | `										sxi32 rcEnum;` |
|     805 | 3393 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|       - | 3394 | `											/* php: a DIRECT static access evaluates every case` |
|       - | 3395 | `											 * of the enum (whole-class constant update) — a` |
|       - | 3396 | `											 * broken sibling case throws here too. A reference` |
|       - | 3397 | `											 * from inside another constant's initializer` |
|       - | 3398 | `											 * (nConstEvalDepth > 0) evaluates only the` |
|       - | 3399 | `											 * requested case. */` |
|      75 | 3400 | `											if( pVm->nConstEvalDepth > 0 ){` |
|       3 | 3401 | `												rcEnum = VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|       2 | 3402 | `											}else{` |
|      73 | 3403 | `												rcEnum = VmEnumMaterialize(&(*pVm),pClass);` |
|       - | 3404 | `											}` |
|      40 | 3405 | `										}else{` |
|     735 | 3406 | `											rcEnum = VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|       - | 3407 | `										}` |
|     805 | 3408 | `										if( rcEnum != SXRET_OK ){` |
|      41 | 3409 | `											VmBoundaryPark(&(*pVm),rcEnum);` |
|      19 | 3410 | `										}` |
|     400 | 3411 | `									}` |
|       - | 3412 | `									/* Load the desired attribute */` |
|    4309 | 3413 | `									pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx);` |
|    4309 | 3414 | `									if( pValue ){` |
|    4269 | 3415 | `										PH7_MemObjLoad(pValue,pTos);` |
|    4269 | 3416 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       - | 3417 | `											/* Load index number */` |
|    1719 | 3418 | `											pTos->nIdx = pAttr->nIdx;` |
|    1714 | 3419 | `											if( VmMemberFetchIsRefSource(pInstr)` |
|     863 | 3420 | `											 && (pAttr->iFlags & (PH7_CLASS_ATTR_REFBOUND\|PH7_CLASS_ATTR_REFSRCPIN)) == 0 ){` |
|       - | 3421 | `												/* The instance path's rule, for a class static:` |
|       - | 3422 | `												 * one counted pin so the other end's unpin cannot` |
|       - | 3423 | ``												 * free the static's value. `$o->p =& C::$s;` then`` |
|       - | 3424 | `												 * dropping $o read C::$s as NULL. */` |
|       3 | 3425 | `												pAttr->iFlags \|= PH7_CLASS_ATTR_REFSRCPIN;` |
|       3 | 3426 | `												VmPinMemObjSlotCounted(&(*pVm),pAttr->nIdx);` |
|       1 | 3427 | `											}` |
|     857 | 3428 | `										}` |
|    2137 | 3429 | `									}` |
|    2179 | 3430 | `								}else if( bConstForm \|\| !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 3431 | `									/* Denied by visibility. php's Error is CATCHABLE here` |
|       - | 3432 | `									 * exactly as it is for an instance property, and PHL` |
|       - | 3433 | `									 * reported it uncaught and ABORTED the script -- so a` |
|       - | 3434 | ``									 * `try { C::$protectedStatic; } catch` never ran its`` |
|       - | 3435 | `									 * catch, and everything after the try was dropped with` |
|       - | 3436 | `									 * exit status 0. Parked on the boundary rail like the` |
|       - | 3437 | `									 * instance twin; the op completes with the NULL already` |
|       - | 3438 | `									 * in the slot and the fetch-point router lands the throw.` |
|       - | 3439 | ``									 * A lookup (isset/empty/`??`) stays silent and false, as`` |
|       - | 3440 | `									 * php's is. The name is built with the ATTRIBUTE's own` |
|       - | 3441 | `									 * spelling, which is the declaration's. */` |
|       - | 3442 | `									SyBlob sErrVis;` |
|      30 | 3443 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       9 | 3444 | `										? "private" : "protected";` |
|      21 | 3445 | `									SyBlobInit(&sErrVis,&pVm->sAllocator);` |
|      21 | 3446 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|      15 | 3447 | `										SyBlobFormat(&sErrVis,"Cannot access %s constant %z::%z",` |
|       6 | 3448 | `											zVis,&pClass->sDisp,&pAttr->sName);` |
|       9 | 3449 | `									}else{` |
|       7 | 3450 | `										SyBlobFormat(&sErrVis,"Cannot access %s property %z::$%z",` |
|       3 | 3451 | `											zVis,&pClass->sDisp,&pAttr->sName);` |
|       - | 3452 | `									}` |
|      21 | 3453 | `									VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",` |
|       - | 3454 | `										sizeof("Error")-1,&sErrVis));` |
|       9 | 3455 | `								}` |
|       - | 3456 | `							}` |
|    2164 | 3457 | `						}` |
|       - | 3458 | `					}` |
|       - | 3459 | `				}` |
|    7696 | 3460 | `				if( pThis ){` |
|       - | 3461 | `					/* Safely unreference the object */` |
|      27 | 3462 | `					PH7_ClassInstanceUnref(pThis);` |
|      12 | 3463 | `				}` |
|       - | 3464 | `			}` |
|    3843 | 3465 | `		}else{` |
|       - | 3466 | ``			/* `$v::X` where $v holds neither an object nor a class NAME. php's`` |
|       - | 3467 | `			 * catchable Error, which STOPS the statement; PH7 raised its own` |
|       - | 3468 | `` 			 * uncatchable wording and carried on with NULL, so a `$obj::$s = 1` `` |
|       - | 3469 | `			 * through a null base went on to fail again in OP_STORE. */` |
|       - | 3470 | `			sxi32 rcCn;` |
|      41 | 3471 | `			if( !pInstr->p3 ){` |
|      27 | 3472 | `				VmPopOperand(&pTos,1);` |
|      13 | 3473 | `			}` |
|      41 | 3474 | `			PH7_MemObjRelease(pTos);` |
|      41 | 3475 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      41 | 3476 | `			pTos->nIdx = SXU32_HIGH;` |
|      41 | 3477 | `			rcCn = VmThrowFromVm(&(*pVm),"Error","Class name must be a valid object or a string",` |
|       - | 3478 | `				sizeof("Class name must be a valid object or a string")-1);` |
|      41 | 3479 | `			if( rcCn == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      41 | 3480 | `			rc = rcCn;` |
|      41 | 3481 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 3482 | `		}` |
|       - | 3483 | `	}` |
|  282714 | 3484 | `	VM_EXIT_BREAK;` |
|     ! 0 | 3485 | `	VM_EXIT_BREAK;` |
|  195158 | 3486 | `}` |
|       - | 3487 |  |
|       - | 3488 | `/*` |
|       - | 3489 | ` * OP_CLONE: body moved verbatim from the OP_CLONE arm of` |
|       - | 3490 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 3491 | ` */` |
|     604 | 3492 | `PH7_PRIVATE VmOpRc VmExecOpClone(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 3493 | `{` |
|     609 | 3494 | `	ph7_value *pTos = pState->pTos;` |
|     609 | 3495 | `	ph7_value *pStack = pState->pStack;` |
|     609 | 3496 | `	VmInstr *aInstr = pState->aInstr;` |
|     609 | 3497 | `	sxi32 pc = pState->pc;` |
|       - | 3498 | `	sxi32 rc;` |
|     302 | 3499 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 3500 | `	ph7_class_instance *pSrc,*pClone;` |
|       - | 3501 | `#ifdef UNTRUST` |
|       - | 3502 | `	if( pTos < pStack ){` |
|       - | 3503 | `		VM_EXIT_ABORT;` |
|       - | 3504 | `	}` |
|       - | 3505 | `#endif` |
|       - | 3506 | `	/* Make sure we are dealing with a class instance. PHP 8 throws a catchable` |
|       - | 3507 | ``	 * TypeError for a non-object operand — for both `clone $x` and clone($x). */`` |
|     609 | 3508 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - | 3509 | `		SyBlob sMsg;` |
|       - | 3510 | `		char zGiven[64];` |
|      24 | 3511 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 3512 | `		/* php names the VALUE for a bool ("true given", not "bool given"), which is` |
|       - | 3513 | `		 * what every other argument diagnostic here already says — the shared helper. */` |
|      24 | 3514 | `		SyBlobFormat(&sMsg,"clone(): Argument #1 ($object) must be of type object, %s given",` |
|      11 | 3515 | `			VmValueGivenName(pTos,zGiven,sizeof(zGiven)));` |
|      24 | 3516 | `		rc = VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|      24 | 3517 | `		PH7_MemObjRelease(pTos);` |
|      24 | 3518 | `		pTos->nIdx = SXU32_HIGH;` |
|      24 | 3519 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 3520 | `			VM_EXIT_ABORT;` |
|       - | 3521 | `		}` |
|       - | 3522 | `		{` |
|       - | 3523 | `			sxi32 iRp;` |
|      24 | 3524 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      24 | 3525 | `				pc = iRp;` |
|      24 | 3526 | `				VM_EXIT_BREAK;` |
|       - | 3527 | `			}` |
|       - | 3528 | `		}` |
|     ! 0 | 3529 | `		VM_EXIT_EXCEPTION;` |
|       - | 3530 | `	}` |
|       - | 3531 | `	/* Point to the source */` |
|     587 | 3532 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|       - | 3533 | `	/* Enum cases are not cloneable — php's catchable Error (the singleton` |
|       - | 3534 | `	 * identity would break) — and neither is a class whose instances own a` |
|       - | 3535 | `	 * C-side resource a slot-by-slot copy would double-free (PH7_CLASS_NOCLONE),` |
|       - | 3536 | `	 * nor a Generator or a Fiber (their suspended C state is not copyable). php` |
|       - | 3537 | `	 * words all of them the same way and throws the same catchable Error; the` |
|       - | 3538 | `	 * Generator/Fiber pair used to take an older warn-only path here, so` |
|       - | 3539 | ``	 * `clone $gen` PRINTED an uncatchable diagnostic and then carried on with the`` |
|       - | 3540 | ``	 * ORIGINAL object standing in for the copy — the one shape of `clone` that`` |
|       - | 3541 | `	 * answered a value php never lets the program reach. */` |
|     582 | 3542 | `	if( (pSrc->pClass->iFlags & PH7_CLASS_ENUM) \|\| PH7_ClassIsUncloneable(pSrc->pClass)` |
|     510 | 3543 | `		\|\| pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|       - | 3544 | `		SyBlob sMsg;` |
|     155 | 3545 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     155 | 3546 | `		SyBlobFormat(&sMsg,"Trying to clone an uncloneable object of class %z",` |
|     150 | 3547 | `			&pSrc->pClass->sDisp);` |
|     155 | 3548 | `		rc = VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|     155 | 3549 | `		PH7_MemObjRelease(pTos);` |
|     155 | 3550 | `		pTos->nIdx = SXU32_HIGH;` |
|     155 | 3551 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 3552 | `			VM_EXIT_ABORT;` |
|       - | 3553 | `		}` |
|       - | 3554 | `		{` |
|       - | 3555 | `			sxi32 iRp;` |
|     155 | 3556 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|     106 | 3557 | `				pc = iRp;` |
|     106 | 3558 | `				VM_EXIT_BREAK;` |
|       - | 3559 | `			}` |
|       - | 3560 | `		}` |
|      50 | 3561 | `		VM_EXIT_EXCEPTION;` |
|       - | 3562 | `	}` |
|       - | 3563 | `	/* Perform the clone operation */` |
|     437 | 3564 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|     437 | 3565 | `	PH7_MemObjRelease(pTos);` |
|     437 | 3566 | `	if( pClone == 0 ){` |
|     ! 0 | 3567 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 3568 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|     ! 0 | 3569 | `	}else{` |
|       - | 3570 | `		/* Load the cloned object */` |
|     437 | 3571 | `		pTos->x.pOther = pClone;` |
|     437 | 3572 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - | 3573 | `	}` |
|     437 | 3574 | `	VM_EXIT_BREAK;` |
|     ! 0 | 3575 | `	VM_EXIT_BREAK;` |
|     307 | 3576 | `}` |
|       - | 3577 |  |
