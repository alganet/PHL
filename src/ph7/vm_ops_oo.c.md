# src/ph7/vm_ops_oo.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1881/2093 lines (89.87%)

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
|     508 |   33 | `static int VmAnonAbstractGapFatal(ph7_vm *pVm,ph7_class *pClass)` |
|       5 |   34 | `{` |
|       - |   35 | `	SyBlob sMsg;` |
|       - |   36 | `	int bGap;` |
|     513 |   37 | `	SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     513 |   38 | `	bGap = PH7_ClassAbstractGap(&(*pVm),pClass,&sMsg) != 0;` |
|     513 |   39 | `	if( bGap ){` |
|       8 |   40 | `		PH7_VmFatalError(&(*pVm),"%.*s",` |
|       4 |   41 | `			(int)SyBlobLength(&sMsg),(const char *)SyBlobData(&sMsg));` |
|       6 |   42 | `		pVm->iExitStatus = 255;` |
|       6 |   43 | `		pVm->bHaltRequested = 1;` |
|       2 |   44 | `	}` |
|     513 |   45 | `	SyBlobRelease(&sMsg);` |
|     513 |   46 | `	return bGap;` |
|       5 |   47 | `}` |
| 2144044 |   48 | `PH7_PRIVATE VmOpRc VmExecOpNew(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |   49 | `{` |
| 2144049 |   50 | `	ph7_value *pTos = pState->pTos;` |
| 2144049 |   51 | `	ph7_value *pStack = pState->pStack;` |
| 2144049 |   52 | `	VmInstr *aInstr = pState->aInstr;` |
| 2144049 |   53 | `	sxi32 pc = pState->pc;` |
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
| 2144049 |   69 | `	int bScreenOnly = pInstr->iP1 < 0;` |
| 2144049 |   70 | `	if( pInstr->iP1 == -2 ){` |
|       - |   71 | `		/* An anonymous class's DECLARATION screen: the class is in p3, the stack is` |
|       - |   72 | `		 * untouched and the constructor arguments have not run yet (see` |
|       - |   73 | `		 * PH7_CompileAnnonClass). */` |
|     267 |   74 | `		if( pInstr->p3 && VmAnonAbstractGapFatal(&(*pVm),(ph7_class *)pInstr->p3) ){` |
|       6 |   75 | `			VM_EXIT_ABORT;` |
|       - |   76 | `		}` |
|     263 |   77 | `		VM_EXIT_BREAK;` |
|       - |   78 | `	}` |
| 2707407 |   79 | `	sxi32 nCtorArgs = bScreenOnly ? 0` |
| 1635509 |   80 | `		: pInstr->iP1 + (pInstr->iP2 ? VmSpreadOwnExtra(pVm,pInstr->iP1,pTos) : 0);` |
|       - |   81 | `	ph7_value *pArg;` |
| 2143787 |   82 | `	ph7_class *pClass = 0;` |
|       - |   83 | `	ph7_class_instance *pNew;` |
|       - |   84 | `	/* Resolving the class below can run an AUTOLOADER that throws. Snapshot the boundary` |
|       - |   85 | `	 * rail so the "not found" report can tell a missing class from an autoloader that` |
|       - |   86 | `	 * raised — php propagates the autoloader's exception and reports nothing else, while` |
|       - |   87 | `	 * this arm used to throw its own Error on top of it (uncaught, killing the script` |
|       - |   88 | `	 * right after the real exception had been handled). */` |
| 2143787 |   89 | `	sxi32 nNewBrc = pVm->nBoundaryRc;` |
| 2143787 |   90 | `	const void *pNewRes = (const void *)pVm->pResumeFrame;` |
| 2143787 |   91 | `	pArg = &pTos[-nCtorArgs]; /* Constructor arguments (if available) */` |
|       - |   92 | `	/* Same PHP 8.1 spread-key realignment as OP_CALL: build a per-actual-slot name` |
|       - |   93 | `	 * map when the ctor arg list unpacked string-keyed elements. NEW keeps the` |
|       - |   94 | `	 * class-name slot at pTos (above the args), so pArg is already the correct base;` |
|       - |   95 | `	 * the build also truncates this call's captured runs. */` |
|       - |   96 | `	VmCallArgMap sEffNewMap;` |
|       - |   97 | `	/* The screen pass has no arguments and no runs of its own: building (and` |
|       - |   98 | `	 * truncating) an effective map there would speak for the enclosing call. */` |
| 2707407 |   99 | `	VmCallArgMap *pEffNewMap = bScreenOnly ? 0 : VmEffCallArgMap(pVm,pInstr,pArg,` |
| 1127263 |  100 | `		nCtorArgs > 0 ? (sxu32)nCtorArgs : 0,&sEffNewMap);` |
| 3215661 |  101 | `	if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
| 2143653 |  102 | `		const char *zCls = (const char *)SyBlobData(&pTos->sBlob);` |
| 2143653 |  103 | `		sxu32 nCls = SyBlobLength(&pTos->sBlob);` |
| 2143648 |  104 | ``		if( pInstr->bDiscard /* the keyword was WRITTEN: `new $c` never resolves one */`` |
| 1071838 |  105 | `		 && ((nCls == sizeof("self")-1 && SyMemcmp(zCls,"self",sizeof("self")-1) == 0)` |
|      88 |  106 | `		 \|\| (nCls == sizeof("static")-1 && SyMemcmp(zCls,"static",sizeof("static")-1) == 0)` |
|      74 |  107 | `		 \|\| (nCls == sizeof("parent")-1 && SyMemcmp(zCls,"parent",sizeof("parent")-1) == 0)) ){` |
|       - |  108 | `			/* new self() / new static() / new parent(): resolve against the live` |
|       - |  109 | `			 * class context (LSB for static), sharing the FCC resolver. */` |
|     114 |  110 | `			pClass = VmFccResolveScope(&(*pVm),pTos);` |
|     114 |  111 | `			if( pClass && (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) ){` |
|       - |  112 | `				/* Not new-able (the named path's iLoadable extract excludes these` |
|       - |  113 | `				 * before it ever gets here): php's wording, with the RESOLVED name.` |
|       - |  114 | `				 *` |
|       - |  115 | `				 * It is the SAME refusal the named path makes a few lines below and it` |
|       - |  116 | ``				 * is thrown the same way -- a catchable `Error`. This arm used to print`` |
|       - |  117 | `				 * an uncatchable engine diagnostic and abort with exit status 0, so` |
|       - |  118 | ``				 * `new parent()` on an abstract base could neither be caught nor even`` |
|       - |  119 | `				 * be told apart from a clean run by the shell. */` |
|       - |  120 | `				SyBlob sErrAbs;` |
|       - |  121 | `				sxi32 rcAbs;` |
|      13 |  122 | `				const char *zKindAbs = (pClass->iFlags & PH7_CLASS_INTERFACE) ? "interface"` |
|       8 |  123 | `					: (pClass->iFlags & PH7_CLASS_TRAIT) ? "trait" : "abstract class";` |
|       9 |  124 | `				SyBlobInit(&sErrAbs,&pVm->sAllocator);` |
|       9 |  125 | `				SyBlobFormat(&sErrAbs,"Cannot instantiate %s %z",zKindAbs,&pClass->sDisp);` |
|       9 |  126 | `				if( nCtorArgs > 0 ){` |
|     ! 0 |  127 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  128 | `				}` |
|       9 |  129 | `				PH7_MemObjRelease(pTos);` |
|       9 |  130 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       9 |  131 | `				pTos->nIdx = SXU32_HIGH;` |
|      13 |  132 | `				rcAbs = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrAbs),` |
|       4 |  133 | `					SyBlobLength(&sErrAbs));` |
|       9 |  134 | `				SyBlobRelease(&sErrAbs);` |
|       9 |  135 | `				if( rcAbs == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 |  136 | `				rc = rcAbs;` |
|       9 |  137 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  138 | `			}` |
|      55 |  139 | `		}else{` |
|       - |  140 | `			/* Try to extract the desired class */` |
| 2143609 |  141 | `			pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,` |
|       - |  142 | `				TRUE /* Only loadable class but not 'interface' or 'abstract' class*/,0);` |
|       5 |  143 | `		}` |
| 1071968 |  144 | `	}else if( pTos->iFlags & MEMOBJ_OBJ ){` |
|       - |  145 | `		/* Take the base class from the loaded instance */` |
|      10 |  146 | `		pClass = ((ph7_class_instance *)pTos->x.pOther)->pClass;` |
|       4 |  147 | `	}` |
| 2143733 |  148 | `	if( pClass == 0 ){` |
|       - |  149 | ``		/* php: `new NoSuch()` is a CATCHABLE Error ('Class "NoSuch" not found'), not a`` |
|       - |  150 | `		 * hard stop. Aborting here killed the script outright -- and with exit status 0,` |
|       - |  151 | `		 * so a caller could not even tell it had failed. */` |
|       - |  152 | `		SyBlob sErrM;` |
|       - |  153 | `		sxi32 rcErr;` |
|      99 |  154 | `		ph7_class *pNotNew = 0;` |
|      99 |  155 | `		if( PH7_VmClassLookupRaised(&(*pVm),nNewBrc,pNewRes) ){` |
|       - |  156 | `			/* The autoloader threw: land THAT (an in-place catch leaves 0 behind plus a` |
|       - |  157 | `			 * recorded resume frame, which the router picks up). */` |
|      19 |  158 | `			sxi32 rcAuto = pVm->nBoundaryRc;` |
|      19 |  159 | `			pVm->nBoundaryRc = 0;` |
|      19 |  160 | `			if( nCtorArgs > 0 ){` |
|     ! 0 |  161 | `				VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  162 | `			}` |
|      19 |  163 | `			PH7_MemObjRelease(pTos);` |
|      19 |  164 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      19 |  165 | `			pTos->nIdx = SXU32_HIGH;` |
|      19 |  166 | `			if( rcAuto == PH7_ABORT ){` |
|       3 |  167 | `				VM_EXIT_ABORT;` |
|       - |  168 | `			}` |
|      17 |  169 | `			rc = PH7_EXCEPTION;` |
|      21 |  170 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  171 | `		}` |
|      83 |  172 | `		SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      83 |  173 | `		if( (pTos->iFlags & MEMOBJ_STRING) && SyBlobLength(&pTos->sBlob) > 0 ){` |
|       - |  174 | `			/* The extract above only accepts NEW-able classes, so an interface, an` |
|       - |  175 | `			 * abstract class or a trait comes back as 0 and used to be reported as` |
|       - |  176 | `			 * "not found". Look again without that filter so php's real message can be` |
|       - |  177 | `			 * given — but through the table DIRECTLY, never PH7_VmExtractClass: that` |
|       - |  178 | `			 * one fires the autoloader when the name is absent, so a genuinely missing` |
|       - |  179 | `			 * class ran every registered autoloader TWICE where php runs them once. */` |
|      69 |  180 | `			const char *zNotNew = (const char *)SyBlobData(&pTos->sBlob);` |
|      69 |  181 | `			sxu32 nNotNew = SyBlobLength(&pTos->sBlob);` |
|       - |  182 | `			SyHashEntry *pNotNewEntry;` |
|      69 |  183 | `			PH7_VmClassNameAnchor(&zNotNew,&nNotNew);` |
|      69 |  184 | `			pNotNewEntry = nNotNew > 0 ? PH7_VmClassEntry(pVm,zNotNew,nNotNew) : 0;` |
|      69 |  185 | `			pNotNew = pNotNewEntry ? (ph7_class *)pNotNewEntry->pUserData : 0;` |
|      32 |  186 | `		}` |
|      91 |  187 | `		if( pNotNew && (pNotNew->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_ABSTRACT\|PH7_CLASS_TRAIT)) ){` |
|       - |  188 | `			/* php names WHAT it will not instantiate; a trait is one of the three, and` |
|       - |  189 | `			 * PH7's loadable-only extract rejected it as "not found" — the one shape of` |
|       - |  190 | ``			 * `new` whose refusal did not say why. */`` |
|      25 |  191 | `			const char *zKind = (pNotNew->iFlags & PH7_CLASS_INTERFACE) ? "interface"` |
|      14 |  192 | `				: (pNotNew->iFlags & PH7_CLASS_TRAIT) ? "trait" : "abstract class";` |
|      17 |  193 | `			SyBlobFormat(&sErrM,"Cannot instantiate %s %z",zKind,&pNotNew->sDisp);` |
|      75 |  194 | `		}else if( (pTos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ)) == 0 ){` |
|       - |  195 | `` 			/* php refuses the OPERAND before it ever has a name to look up: a `new` `` |
|       - |  196 | `			 * takes an object or a string and nothing else, and every other value --` |
|       - |  197 | `			 * int, float, bool, null, array, resource -- is` |
|       - |  198 | ``			 * `Class name must be a valid object or a string`. PHL string-cast the`` |
|       - |  199 | ``			 * slot's raw blob instead, so all six answered `Class "" not found`, a`` |
|       - |  200 | `			 * sentence that names a class the program never wrote. (An EMPTY string` |
|       - |  201 | ``			 * really is `Class "" not found` in php, so the test is the TYPE.) The`` |
|       - |  202 | ``			 * `::` twin of this refusal is already at the bottom of VmExecOpMember. */`` |
|      13 |  203 | `			SyBlobAppend(&sErrM,"Class name must be a valid object or a string",` |
|       - |  204 | `				sizeof("Class name must be a valid object or a string")-1);` |
|       7 |  205 | `		}else{` |
|       - |  206 | ``			/* `new self` / `new static` / `new parent` with no class behind the keyword. */`` |
|       - |  207 | `			char zKwBuf[96];` |
|      85 |  208 | `			const char *zKwWhy = pInstr->bDiscard ? PH7_VmScopeKeywordRefusal(&(*pVm),` |
|      10 |  209 | `				(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob),` |
|      30 |  210 | `				FALSE,zKwBuf,(int)sizeof(zKwBuf)) : 0;` |
|      55 |  211 | `			if( zKwWhy ){` |
|      11 |  212 | `				SyBlobAppend(&sErrM,zKwWhy,(sxu32)SyStrlen(zKwWhy));` |
|       6 |  213 | `			}else{` |
|      45 |  214 | `				SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|      40 |  215 | `					SyBlobLength(&pTos->sBlob),(const char *)SyBlobData(&pTos->sBlob));` |
|       - |  216 | `			}` |
|       - |  217 | `		}` |
|       - |  218 | `		/* Settle the stack BEFORE throwing: the class-name slot becomes the (NULL)` |
|       - |  219 | `		 * expression result and the ctor arguments go. */` |
|      83 |  220 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  221 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  222 | `		}` |
|      83 |  223 | `		PH7_MemObjRelease(pTos);` |
|      83 |  224 | `		MemObjSetType(pTos,MEMOBJ_NULL);` |
|      83 |  225 | `		pTos->nIdx = SXU32_HIGH;` |
|     122 |  226 | `		rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      39 |  227 | `			SyBlobLength(&sErrM));` |
|      83 |  228 | `		SyBlobRelease(&sErrM);` |
|      83 |  229 | `		if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      83 |  230 | `		rc = rcErr;` |
|      85 |  231 | `		PH7_THROW_ROUTE_MIDEXPR(rc)` |
| 2143639 |  232 | `	}else if( pClass->iFlags & PH7_CLASS_NOINSTANTIATE ){` |
|       - |  233 | `		/* php refuses this one in the create_object handler, so the refusal names the` |
|       - |  234 | `		 * INSTANTIATION and never reaches the constructor — which matters because the` |
|       - |  235 | `		 * class may still declare a private __construct that Reflection prints. Same` |
|       - |  236 | `		 * shape as the enum reject below: no instance, no __destruct. */` |
|       - |  237 | `		SyBlob sErrMsg;` |
|      43 |  238 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      43 |  239 | `		if( pClass->zNewRefusal ){` |
|       - |  240 | `			/* php words a few of these per class — Directory's names dir() as the` |
|       - |  241 | `			 * way to get one — so the spec's own sentence wins when it has one. */` |
|      39 |  242 | `			SyBlobAppend(&sErrMsg,pClass->zNewRefusal,SyStrlen(pClass->zNewRefusal));` |
|      22 |  243 | `		}else{` |
|       5 |  244 | `			SyBlobFormat(&sErrMsg,"Instantiation of class %z is not allowed",&pClass->sDisp);` |
|       - |  245 | `		}` |
|       - |  246 | `		{` |
|      43 |  247 | `			const char *zRefCls = pClass->zNewRefusalClass ? pClass->zNewRefusalClass : "Error";` |
|      62 |  248 | `			VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),zRefCls,` |
|      38 |  249 | `				(sxu32)SyStrlen(zRefCls),&sErrMsg));` |
|       - |  250 | `		}` |
|      43 |  251 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  252 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  253 | `		}` |
|      43 |  254 | `		PH7_MemObjRelease(pTos);` |
|      43 |  255 | `		pTos->nIdx = SXU32_HIGH;` |
|      43 |  256 | `		VM_EXIT_BREAK;` |
| 2143601 |  257 | `	}else if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|       - |  258 | `		/* php 8.1: enums cannot be instantiated — a catchable Error, raised` |
|       - |  259 | `		 * BEFORE any construction (no instance, no __destruct). */` |
|       - |  260 | `		SyBlob sErrMsg;` |
|       9 |  261 | `		SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       9 |  262 | `		SyBlobFormat(&sErrMsg,"Cannot instantiate enum %z",&pClass->sDisp);` |
|       9 |  263 | `		VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       9 |  264 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  265 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  266 | `		}` |
|       9 |  267 | `		PH7_MemObjRelease(pTos);` |
|       9 |  268 | `		pTos->nIdx = SXU32_HIGH;` |
|       9 |  269 | `		VM_EXIT_BREAK;` |
| 2143588 |  270 | `	}else if( (pClass->iFlags & PH7_CLASS_ANON)` |
| 1071901 |  271 | `	       && VmAnonAbstractGapFatal(&(*pVm),pClass) ){` |
|       - |  272 | `		/* An anonymous class's DECLARATION is an expression, so php asks whether it` |
|       - |  273 | ``		 * leaves an abstract method unimplemented when the `new` RUNS -- not when the`` |
|       - |  274 | `		 * unit compiles. The refusal therefore carries the frame chain that reached` |
|       - |  275 | ``		 * it, and a `new` on a branch nothing takes is never asked at all. PHL mounts`` |
|       - |  276 | `		 * the class at compile time and used to ask there, which failed the whole FILE` |
|       - |  277 | ``		 * on a `new` php never performs. Uncatchable in php too, so nothing user`` |
|       - |  278 | `		 * written depends on either timing. */` |
|     ! 0 |  279 | `		VM_EXIT_ABORT;` |
| 2143588 |  280 | `	}else if( VmClassStaticDeferPending(pClass)` |
| 1071949 |  281 | `	       && (rc = PH7_VmMaterializeClassStatics(&(*pVm),pClass)) != SXRET_OK ){` |
|       - |  282 | `		/* php materializes the static table at instantiation too, so a default` |
|       - |  283 | `		 * that THREW at the declaration (or failed its type check) raises BEFORE` |
|       - |  284 | `		 * any construction (no instance, no __destruct) — same shape as the enum` |
|       - |  285 | `		 * reject above: park the status, settle the stack, and let the` |
|       - |  286 | `		 * fetch-point router land it. */` |
|      28 |  287 | `		VmBoundaryPark(&(*pVm),rc);` |
|      28 |  288 | `		if( nCtorArgs > 0 ){` |
|     ! 0 |  289 | `			VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  290 | `		}` |
|      28 |  291 | `		PH7_MemObjRelease(pTos);` |
|      28 |  292 | `		pTos->nIdx = SXU32_HIGH;` |
|      28 |  293 | `		VM_EXIT_BREAK;` |
|     ! 0 |  294 | `	}else{` |
|       - |  295 | `		ph7_class_method *pCons;` |
|       - |  296 | `		/* Check if a constructor is available — BEFORE instantiation: a` |
|       - |  297 | ``		 * visibility-denied `new` must not construct (nor later destruct)`` |
|       - |  298 | `		 * the object (band A #4). */` |
|       - |  299 | `		/* Only an explicit __construct is the constructor. PHP-4-style class-name` |
|       - |  300 | `		 * constructors were removed in PHP 8.0 — a method named like the class is a` |
|       - |  301 | `		 * plain method, so no same-name fallback here. */` |
| 2143569 |  302 | `		pCons = PH7_ClassExtractMethod(pClass,"__construct",sizeof("__construct")-1);` |
|       - |  303 | `		/* Constructor visibility (band A #4): __construct now KEEPS its` |
|       - |  304 | `		 * declared protection (PH7_NewClassMethod no longer forces it` |
|       - |  305 | ``		 * public), so `new C()` on a private/protected constructor from`` |
|       - |  306 | `		 * the wrong scope is php's catchable Error. Reflection's` |
|       - |  307 | `		 * newInstance path sets bReflectBypass like method invoke. */` |
| 2143569 |  308 | `		if( pCons && pCons->iProtection != PH7_CLASS_PROT_PUBLIC ){` |
|       - |  309 | `			/* php binds non-public access by the member's DECLARING class, not the` |
|       - |  310 | `			 * instantiated class: a private constructor declared in a base is` |
|       - |  311 | `			 * reachable from that base's own methods even when instantiating a` |
|       - |  312 | ``			 * subclass (`new Child()` inside Base::factory()). __construct is a`` |
|       - |  313 | `			 * method, so pass its declaring class (sFunc.pUserData) — mirroring the` |
|       - |  314 | `			 * method-call visibility check — rather than pClass, whose hAttr lookup` |
|       - |  315 | `			 * for a method name always misses and falls to a wrong exact-class test. */` |
|      81 |  316 | `			ph7_class *pCtorDecl = pCons->sFunc.pUserData ? (ph7_class *)pCons->sFunc.pUserData : pClass;` |
|      81 |  317 | `			if( pVm->bReflectBypass ){` |
|     ! 0 |  318 | `				pVm->bReflectBypass = 0;` |
|      81 |  319 | `			}else if( !PH7_VmClassMemberAccess(&(*pVm),pCtorDecl,&pCons->sFunc.sName,pCons->iProtection,FALSE) ){` |
|       - |  320 | `				SyBlob sErrMsg;` |
|      57 |  321 | `				const char *zVis = pCons->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       - |  322 | `				/* php NAMES the calling scope when there is one (see the method twin in` |
|       - |  323 | `				 * OP_CALL); "global scope" is only for code outside every class. */` |
|      57 |  324 | `				ph7_class *pCtorScope = PH7_VmCallerScope(&(*pVm));` |
|       - |  325 | `				/* And it names the constructor's DECLARING class, not the class the` |
|       - |  326 | ``				 * `new` asked for: `new Child()` on a private constructor inherited`` |
|       - |  327 | `				 * from Base reports Base::__construct(). The method twin says this` |
|       - |  328 | `				 * through PH7_VmMethodScopeName, which is also the door that turns a` |
|       - |  329 | `				 * trait declarer into the class that COMPOSED it — php names the using` |
|       - |  330 | `				 * class, never the trait. The access test above still asks pCtorDecl,` |
|       - |  331 | `				 * the raw declarer, because the trait grants are decided there. */` |
|      57 |  332 | `				ph7_class *pCtorName = PH7_VmMethodScopeName(&(*pVm),pClass,pCons);` |
|      57 |  333 | `				SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      57 |  334 | `				if( pCtorScope ){` |
|      12 |  335 | `					SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from scope %z",` |
|       5 |  336 | `						zVis,&pCtorName->sDisp,&pCtorScope->sDisp);` |
|       7 |  337 | `				}else{` |
|      47 |  338 | `					SyBlobFormat(&sErrMsg,"Call to %s %z::__construct() from global scope",` |
|      22 |  339 | `						zVis,&pCtorName->sDisp);` |
|       - |  340 | `				}` |
|      57 |  341 | `				VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       - |  342 | `				/* Pop ctor args + release the class-name operand, leave NULL` |
|       - |  343 | `				 * as the expression value; the fetch-point router lands the` |
|       - |  344 | `				 * throw. No instance was created. */` |
|      57 |  345 | `				if( nCtorArgs > 0 ){` |
|     ! 0 |  346 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  347 | `				}` |
|      57 |  348 | `				PH7_MemObjRelease(pTos);` |
|      57 |  349 | `				pTos->nIdx = SXU32_HIGH;` |
|      57 |  350 | `				VM_EXIT_BREAK;` |
|       - |  351 | `			}` |
|      12 |  352 | `		}` |
| 2143515 |  353 | `		if( bScreenOnly ){` |
|       - |  354 | `			/* Every refusal above has been asked. Leave the class name standing for` |
|       - |  355 | `			 * the real pass and let the arguments run. */` |
| 1016438 |  356 | `			VM_EXIT_BREAK;` |
|       - |  357 | `		}` |
| 1127082 |  358 | `		if( nCtorArgs > 0 ){` |
|       - |  359 | ``			/* D1: a `new`'s arguments are ARGUMENTS. An element/property one whose`` |
|       - |  360 | `			 * target was absent at load time rides a deferred carrier that only OP_CALL` |
|       - |  361 | `			 * resolved, and the ctor call reaches its callee by pointer rather than` |
|       - |  362 | ``			 * through that opcode — so `new C($a['missing'])` passed a silent NULL where`` |
|       - |  363 | ``			 * php warns, and a by-REFERENCE ctor parameter (`new C($a['fresh'])` with`` |
|       - |  364 | ``			 * `__construct(&$x)`) never vivified the target at all: php writes the`` |
|       - |  365 | `			 * element, PHL left it uncreated. Resolve here, where the constructor is` |
|       - |  366 | `			 * known and pVm->pFrame is still the caller, exactly as every OP_CALL` |
|       - |  367 | `			 * dispatch branch does. A class with NO constructor still resolves — the` |
|       - |  368 | `			 * arguments were evaluated and php reports what reading them found. */` |
| 1016456 |  369 | `			ph7_class_method *pCtorArgs = pCons;` |
|       - |  370 | `			sxi32 rcDA;` |
| 1016456 |  371 | `			if( pCtorArgs == 0 ){` |
|      14 |  372 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,0,0,0,pEffNewMap);` |
| 1016450 |  373 | `			}else if( pCtorArgs->sFunc.iFlags & VM_FUNC_NATIVE ){` |
|       - |  374 | `				/* A native constructor has no compiled formals; its by-ref positions` |
|       - |  375 | `				 * come from the signature-derived mask, the builtin way. */` |
| 1015545 |  376 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,0,0,` |
| 1015559 |  377 | `					pCtorArgs->sFunc.pNative ? pCtorArgs->sFunc.pNative->nByRefMask : 0,0,0,pEffNewMap);` |
|  507775 |  378 | `			}else{` |
|    1325 |  379 | `				rcDA = PH7_VmResolveDeferredArgs(&(*pVm),pArg,pTos,` |
|     880 |  380 | `					(ph7_vm_func_arg *)SySetBasePtr(&pCtorArgs->sFunc.aArgs),` |
|     440 |  381 | `					SySetUsed(&pCtorArgs->sFunc.aArgs),0,0,0,pEffNewMap);` |
|       - |  382 | `			}` |
| 1016456 |  383 | `			if( rcDA == PH7_ABORT \|\| rcDA == PH7_EXCEPTION ){` |
|       - |  384 | `				/* Reading an argument raised (a string-offset Error, a magic accessor's` |
|       - |  385 | `				 * throw): no object is created, and the stack is tidied exactly as the` |
|       - |  386 | `				 * constructor-throw path below tidies it. */` |
|       - |  387 | `				sxi32 iResumeDA;` |
|       3 |  388 | `				if( rcDA == PH7_ABORT ){` |
|     ! 0 |  389 | `					VM_EXIT_ABORT;` |
|       - |  390 | `				}` |
|       3 |  391 | `				if( VmRecordedResume(pVm,&iResumeDA,pState->pEntryFrame,aInstr) ){` |
|     ! 0 |  392 | `					VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  393 | `					PH7_MemObjRelease(pTos);` |
|     ! 0 |  394 | `					PH7_RESUME_DRAIN()` |
|     ! 0 |  395 | `					pc = iResumeDA;` |
|     ! 0 |  396 | `					VM_EXIT_BREAK;` |
|       - |  397 | `				}` |
|       3 |  398 | `				VM_EXIT_EXCEPTION;` |
|       - |  399 | `			}` |
|  508215 |  400 | `		}` |
| 1127080 |  401 | `		if( pCons == 0 && nCtorArgs > 0 && pEffNewMap && pEffNewMap->bHasNamed ){` |
|       - |  402 | `			/* A class with no constructor still takes a call: php sends the arguments to` |
|       - |  403 | `			 * a pass-through function that declares no parameter, so the first NAME --` |
|       - |  404 | ``			 * written, or a string key an unpack produced -- is `Unknown named`` |
|       - |  405 | ``			 * parameter` at the call site, and the object php allocated for the `new` is`` |
|       - |  406 | `			 * dropped unconstructed (no __destruct). PHL built the object and ignored the` |
|       - |  407 | `			 * name. Refuse before the instance exists; the stack is tidied exactly as the` |
|       - |  408 | `			 * argument-read throw above tidies it. */` |
|       - |  409 | `			sxu32 iNamed;` |
|       9 |  410 | `			for( iNamed = 0; iNamed < (sxu32)nCtorArgs && iNamed < pEffNewMap->nTotal; iNamed++ ){` |
|       9 |  411 | `				if( pEffNewMap->aNames[iNamed].nByte > 0 ){` |
|       - |  412 | `					char zNamedErr[160];` |
|       - |  413 | `					sxi32 rcNamed,iResumeNamed;` |
|      10 |  414 | `					SyBufferFormat(zNamedErr,sizeof(zNamedErr),"Unknown named parameter $%.*s",` |
|       6 |  415 | `						(int)pEffNewMap->aNames[iNamed].nByte,pEffNewMap->aNames[iNamed].zString);` |
|       7 |  416 | `					rcNamed = VmThrowNamedArgError(&(*pVm),zNamedErr,(sxu32)SyStrlen(zNamedErr));` |
|       7 |  417 | `					if( rcNamed == PH7_ABORT ){` |
|     ! 0 |  418 | `						VM_EXIT_ABORT;` |
|       - |  419 | `					}` |
|       7 |  420 | `					if( VmRecordedResume(pVm,&iResumeNamed,pState->pEntryFrame,aInstr) ){` |
|     ! 0 |  421 | `						VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  422 | `						PH7_MemObjRelease(pTos);` |
|     ! 0 |  423 | `						PH7_RESUME_DRAIN()` |
|     ! 0 |  424 | `						pc = iResumeNamed;` |
|     ! 0 |  425 | `						VM_EXIT_BREAK;` |
|       - |  426 | `					}` |
|       7 |  427 | `					VM_EXIT_EXCEPTION;` |
|       - |  428 | `				}` |
|       2 |  429 | `			}` |
|     ! 0 |  430 | `		}` |
|       - |  431 | `		/* Create a new class instance */` |
| 1127074 |  432 | `		pNew = PH7_NewClassInstance(&(*pVm),pClass);` |
| 1127074 |  433 | `		if( pNew == 0 ){` |
|     ! 0 |  434 | `			VmErrorFormat(&(*pVm),PH7_CTX_ERR,` |
|       - |  435 | `				"Cannot create new class '%z' instance due to a memory failure,PH7 is loading NULL",` |
|     ! 0 |  436 | `				&pClass->sDisp` |
|       - |  437 | `			);` |
|     ! 0 |  438 | `			PH7_MemObjRelease(pTos);` |
|     ! 0 |  439 | `			if( nCtorArgs > 0 ){` |
|       - |  440 | `				/* Pop given arguments */` |
|     ! 0 |  441 | `				VmPopOperand(&pTos,nCtorArgs);` |
|     ! 0 |  442 | `			}` |
|     ! 0 |  443 | `			VM_EXIT_BREAK;` |
|       - |  444 | `		}` |
| 1127074 |  445 | `		if( pVm->nBoundaryRc != 0 ){` |
|       - |  446 | `			/* A typed property DEFAULT failed its type check during instance-` |
|       - |  447 | `			 * frame creation (parked catchable TypeError): php aborts the` |
|       - |  448 | `			 * construction — no constructor call, no object. Consume the` |
|       - |  449 | `			 * parked status here and route exactly like a constructor throw` |
|       - |  450 | `			 * (the exception object is already registered). */` |
|      21 |  451 | `			sxi32 rcDef = pVm->nBoundaryRc;` |
|       - |  452 | `			sxi32 iDefResumePc;` |
|      21 |  453 | `			pVm->nBoundaryRc = 0;` |
|       - |  454 | `			/* php never reaches a __destruct here either: object_init_ex evaluates the` |
|       - |  455 | `			 * defaults BEFORE the object exists, so a failing one leaves nothing to` |
|       - |  456 | `			 * destruct. PHL builds the instance first, so it marks it instead. */` |
|      21 |  457 | `			PH7_ClassInstanceCtorFailed(pNew);` |
|      21 |  458 | `			PH7_ClassInstanceUnref(pNew);` |
|      21 |  459 | `			if( rcDef == PH7_ABORT ){` |
|     ! 0 |  460 | `				VM_EXIT_ABORT;` |
|       - |  461 | `			}` |
|      21 |  462 | `			if( VmRecordedResume(pVm,&iDefResumePc,pState->pEntryFrame,aInstr) ){` |
|       - |  463 | `				/* This frame's own try caught it in-place: tidy the stack` |
|       - |  464 | `				 * (pop ctor args + release the class-name slot, then drain any` |
|       - |  465 | `				 * abandoned outer-expression operands to the try's base — the` |
|       - |  466 | `				 * class-name slot itself sits above it) and resume. */` |
|      21 |  467 | `				if( nCtorArgs > 0 ){` |
|       3 |  468 | `					VmPopOperand(&pTos,nCtorArgs);` |
|       1 |  469 | `				}` |
|      21 |  470 | `				PH7_MemObjRelease(pTos);` |
|      45 |  471 | `				PH7_RESUME_DRAIN()` |
|      21 |  472 | `				pc = iDefResumePc;` |
|      21 |  473 | `				VM_EXIT_BREAK;` |
|       - |  474 | `			}` |
|     ! 0 |  475 | `			VM_EXIT_EXCEPTION;` |
|       - |  476 | `		}` |
| 1127054 |  477 | `		if( pCons ){` |
|       - |  478 | `			/* Call the class constructor.  Collect args in stack order and` |
|       - |  479 | `			 * forward any VmCallArgMap from the NEW instruction so the` |
|       - |  480 | `			 * receiving OP_CALL path runs its named-argument matching` |
|       - |  481 | `			 * (including variadic string-key packing). */` |
| 1119214 |  482 | `			VmCallArgMap *pNewMap = pEffNewMap;` |
|       - |  483 | `			sxi32 rcCons;` |
|       - |  484 | `			SySet aArg; /* ctor argument scratch (the loop's shared set stays with OP_CALL) */` |
| 1119214 |  485 | `			SySetInit(&aArg,&pVm->sAllocator,sizeof(ph7_value *));` |
| 2138649 |  486 | `			while( pArg < pTos ){` |
| 1019440 |  487 | `				SySetPut(&aArg,(const void *)&pArg);` |
| 1019440 |  488 | `				pArg++;` |
|       5 |  489 | `			}` |
|       - |  490 | `			/* Too-few-arguments is php's catchable ArgumentCountError, raised` |
|       - |  491 | `			 * by the shared OP_CALL install path this ctor call routes through` |
|       - |  492 | `			 * (was a PHL-only notice here). */` |
| 1119214 |  493 | `			rcCons = VmCallClassMethodWithMap(&(*pVm),pNew,pCons,0,(int)SySetUsed(&aArg),(ph7_value **)SySetBasePtr(&aArg),pNewMap);` |
| 1119214 |  494 | `			SySetRelease(&aArg); /* scratch dies here, on every exit path */` |
|       - |  495 | `			/* TICKET 1433-52: Unsetting $this in the constructor body */` |
| 1119214 |  496 | `			if( pNew->iRef < 1 ){` |
|     ! 0 |  497 | `				pNew->iRef = 1;` |
|     ! 0 |  498 | `			}` |
| 1119214 |  499 | `			if( rcCons == PH7_ABORT \|\| rcCons == PH7_EXCEPTION ){` |
|       - |  500 | `				/* The constructor raised: the half-constructed object must not` |
|       - |  501 | `				 * become the NEW result. Drop our reference so it is destroyed.` |
|       - |  502 | `				 * The class-name operand (and any leftover args) are released by` |
|       - |  503 | `				 * the Abort/Exception unwind, or explicitly on the resume path. */` |
|       - |  504 | `				sxi32 iResumePc;` |
|       - |  505 | `				/* php's ctor-failed mark: no __destruct for an object whose` |
|       - |  506 | `				 * constructor threw, here or ever (see PH7_ClassInstanceCtorFailed). */` |
|  100924 |  507 | `				PH7_ClassInstanceCtorFailed(pNew);` |
|  100924 |  508 | `				PH7_ClassInstanceUnref(pNew);` |
|  100924 |  509 | `				if( rcCons == PH7_ABORT ){` |
|      10 |  510 | `					VM_EXIT_ABORT;` |
|       - |  511 | `				}` |
|  100916 |  512 | `				if( VmRecordedResume(pVm,&iResumePc,pState->pEntryFrame,aInstr) ){` |
|       - |  513 | `					/* This frame's own try caught it in-place: tidy the stack` |
|       - |  514 | `					 * (pop ctor args + release the class-name slot, then drain any` |
|       - |  515 | `					 * abandoned outer-expression operands to the try's base — the` |
|       - |  516 | `					 * class-name slot itself sits above it) and resume. */` |
|  100605 |  517 | `					if( nCtorArgs > 0 ){` |
|     593 |  518 | `						VmPopOperand(&pTos,nCtorArgs);` |
|     294 |  519 | `					}` |
|  100605 |  520 | `					PH7_MemObjRelease(pTos);` |
|  501077 |  521 | `					PH7_RESUME_DRAIN()` |
|  100605 |  522 | `					pc = iResumePc;` |
|  100605 |  523 | `					VM_EXIT_BREAK;` |
|       - |  524 | `				}` |
|     315 |  525 | `				VM_EXIT_EXCEPTION;` |
|       - |  526 | `			}` |
|  509136 |  527 | `		}` |
| 1026135 |  528 | `		if( nCtorArgs > 0 ){` |
|       - |  529 | `			/* Pop given arguments */` |
| 1015553 |  530 | `			VmPopOperand(&pTos,nCtorArgs);` |
|  507765 |  531 | `		}` |
| 1026135 |  532 | `		PH7_MemObjRelease(pTos);` |
| 1026135 |  533 | `		pTos->x.pOther = pNew;` |
| 1026135 |  534 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - |  535 | `	}` |
| 1026135 |  536 | `	VM_EXIT_BREAK;` |
|     ! 0 |  537 | `	VM_EXIT_BREAK;` |
| 1071983 |  538 | `}` |
|       - |  539 |  |
|       - |  540 | `/*` |
|       - |  541 | `` * Is this OP_MEMBER the SOURCE of a reference bind — `$r =& $o->p`, `$a[] =& $o->p`,`` |
|       - |  542 | `` * `[&$o->p]`, a by-reference `foreach ($o->p as &$v)`, a by-reference destructuring`` |
|       - |  543 | `` * source? php compiles all of them with `zend_compile_var(source, BP_VAR_W)`, so the`` |
|       - |  544 | ` * fetch is a WRITE context: a missing property is CREATED, not warned about. PHL` |
|       - |  545 | ` * leaves the instruction a PH7_MEMBER_READ on purpose -- a handler-backed property` |
|       - |  546 | ` * still hands back a copy there, and an overloaded one still dispatches __get -- and` |
|       - |  547 | ` * carries the context in a flag the compiler stamps beside it.` |
|       - |  548 | ` */` |
|  356007 |  549 | `static int VmMemberFetchIsRefSource(const VmInstr *pInstr)` |
|       5 |  550 | `{` |
|  356012 |  551 | `	return pInstr->iP2 == PH7_MEMBER_READ && pInstr->bRefSrc != 0;` |
|       5 |  552 | `}` |
|       - |  553 | `/*` |
|       - |  554 | ` * Is this OP_MEMBER fetching the property for WRITING — php's BP_VAR_W / BP_VAR_RW` |
|       - |  555 | ` * fetch, the one that asks the object for something to MODIFY? The compiler tags` |
|       - |  556 | `` * the base of a subscript-write / `??=` PH7_MEMBER_WRITE, so that answers most of`` |
|       - |  557 | ``  * it once the shapes that share the tag are excluded: a plain store and a `??=` `` |
|       - |  558 | ` * write through their own paths, and a direct read-modify-write is the accessor` |
|       - |  559 | ` * pair (VmMagicRmwArm), not a write fetch. The shapes php compiles as plain READS —` |
|       - |  560 | ` * a reference SOURCE, in all its spellings — carry the compiler's own marker` |
|       - |  561 | `` * (VmMemberFetchIsRefSource). `$o->p =& $x` is NOT one of them: the member is`` |
|       - |  562 | ` * the reference TARGET there and carries its own iP2.` |
|       - |  563 | ` */` |
|  100830 |  564 | `static int VmMemberFetchForWrite(const VmInstr *pInstr)` |
|       5 |  565 | `{` |
|  100835 |  566 | `	const VmInstr *pNext = pInstr + 1;` |
|  100835 |  567 | `	if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|     597 |  568 | `		int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);` |
|     597 |  569 | `		int bCoalesceW = (pNext->iOp == PH7_OP_NULLC_JMP);` |
|     597 |  570 | `		return !bPlainStore && !bCoalesceW && !VmMemberNextIsRmw(pNext);` |
|       - |  571 | `	}` |
|  100243 |  572 | `	return VmMemberFetchIsRefSource(pInstr);` |
|   50420 |  573 | `}` |
|       - |  574 | `/*` |
|       - |  575 | ` * A native class's property is a field of php's own C struct, and the only writes` |
|       - |  576 | ` * that reach one are the writes the store filter converts (ph7_class::xSet). So` |
|       - |  577 | ` * the fetched value keeps its slot index for exactly the shapes that end in such` |
|       - |  578 | `` * a store -- a plain assignment, `??=`, a destructuring target and the`` |
|       - |  579 | ` * read-modify-write forms -- and is a TEMPORARY for every other use. That is` |
|       - |  580 | ` * php's own answer: it has no ptr_ptr handler for such a property, so a reference` |
|       - |  581 | `` * bind (`$r = &$i->f`), a by-reference argument (`preg_match($p,$s,$i->s)`) and a`` |
|       - |  582 | ` * by-reference foreach all get a copy whose writes are SILENTLY lost -- silently,` |
|       - |  583 | ` * unlike the overloaded case, which php has a notice for.` |
|       - |  584 | ` */` |
|   40243 |  585 | `static int VmMemberNativeSetKeepsSlot(const VmInstr *pInstr)` |
|       5 |  586 | `{` |
|   40248 |  587 | `	if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|       - |  588 | ``		/* Every fetch the compiler tags for writing: a plain store, `??=`, a`` |
|       - |  589 | `		 * compound assign, and the base of a subscript write — that last one has` |
|       - |  590 | ``		 * to reach the real value so `$i->y[0] = 5` is php's "Cannot use a scalar`` |
|       - |  591 | `		 * value as an array" rather than a write nobody notices. */` |
|    1753 |  592 | `		return 1;` |
|       - |  593 | `	}` |
|   38500 |  594 | `	if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       7 |  595 | `		return 1;   /* list()/foreach destructuring target */` |
|       - |  596 | `	}` |
|   38494 |  597 | `	return VmMemberNextIsWrite(pInstr + 1);   /* ++/-- and the compound assigns */` |
|   20127 |  598 | `}` |
|       - |  599 | `/*` |
|       - |  600 | `` * `$doc->encoding ??= 'UTF-8'` on a name a native class's own property-handler`` |
|       - |  601 | ` * table carries.` |
|       - |  602 | ` *` |
|       - |  603 | ` * The member opcode's handler gate answers a READ and can only REFUSE a write,` |
|       - |  604 | ` * because the value does not exist yet -- and a coalesce that finds null must` |
|       - |  605 | ` * make one. Only the overloaded-coalesce rail carries a pending store to the` |
|       - |  606 | ` * point the value arrives, so this shape is left to it: the read it makes goes` |
|       - |  607 | ` * through the same handler anyway.` |
|       - |  608 | ` */` |
|   37303 |  609 | `static int VmMemberCoalOwned(ph7_class_instance *pThis,const VmInstr *pInstr,const SyString *pName)` |
|       5 |  610 | `{` |
|   37876 |  611 | `	return pInstr->iP2 == PH7_MEMBER_WRITE` |
|   19220 |  612 | `	    && pInstr[1].iOp == PH7_OP_NULLC_JMP` |
|   37871 |  613 | `	    && PH7_ClassNativePropOwns(pThis,pName);` |
|       5 |  614 | `}` |
|       - |  615 | `/*` |
|       - |  616 | `` * php's `Indirect modification of overloaded property C::$p has no effect`: the`` |
|       - |  617 | ` * write-context fetch above landed on a property only __get answers for, so what` |
|       - |  618 | ` * comes back is a VALUE and whatever the rest of the expression writes into it is` |
|       - |  619 | ` * thrown away. php says so and carries on.` |
|       - |  620 | ` *` |
|       - |  621 | ` * PHL had the notice at one site only (a by-reference ARGUMENT, VmBindPropByRef)` |
|       - |  622 | ` * and, worse, the value was not a copy: __get's return still shared the object's` |
|       - |  623 | `` * own nested hashmap by COW, so `$o->a['b'] = 9`, `$o->a[] = 5` and a by-reference`` |
|       - |  624 | ` * foreach modified the object php leaves untouched. Separating it here is what` |
|       - |  625 | ` * makes the write land nowhere.` |
|       - |  626 | ` *` |
|       - |  627 | ` * An ENGINE __get is not this — php routes those through the class's own property` |
|       - |  628 | ` * handler, which hands back the real element (ArrayObject's ARRAY_AS_PROPS reads` |
|       - |  629 | ` * and writes through, in both engines). php stays silent for an OBJECT value too:` |
|       - |  630 | ` * a handle is shared, so nothing about the write is indirect.` |
|       - |  631 | ` */` |
|      32 |  632 | `PH7_PRIVATE void PH7_VmOverloadedPropNotice(ph7_vm *pVm,ph7_class *pClass,const SyString *pName,ph7_value *pVal)` |
|       1 |  633 | `{` |
|      33 |  634 | `	ph7_class_method *pGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|      33 |  635 | `	if( pGet == 0 \|\| (pGet->sFunc.iFlags & VM_FUNC_NATIVE) ){` |
|     ! 0 |  636 | `		return;` |
|       - |  637 | `	}` |
|      33 |  638 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|       3 |  639 | `		return;` |
|       - |  640 | `	}` |
|      46 |  641 | `	VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - |  642 | `		"Indirect modification of overloaded property %z::$%z has no effect",` |
|      15 |  643 | `		&pClass->sDisp,pName);` |
|      31 |  644 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      25 |  645 | `		PH7_HashmapCowSeparate(&(*pVm),pVal);` |
|      12 |  646 | `	}` |
|      17 |  647 | `}` |
|       - |  648 | `/*` |
|       - |  649 | ` * Does an OVERLOADED read-modify-write apply to this property access? php runs` |
|       - |  650 | ` * one only when the class answers BOTH sides; the guard means we are already` |
|       - |  651 | ` * inside this property's own __get, where php behaves as if the accessor were` |
|       - |  652 | ` * absent.` |
|       - |  653 | ` */` |
|      44 |  654 | `static int VmMagicRmwEligible(ph7_vm *pVm,ph7_class *pClass,ph7_class_instance *pThis,const SyString *pName)` |
|       2 |  655 | `{` |
|      46 |  656 | `	if( PH7_ClassNativePropOwns(pThis,pName) ){` |
|       - |  657 | `		/* A native class's own property handler answers BOTH halves -- php reads` |
|       - |  658 | ``		 * `$doc->version .= '.1'` through read_property and writes the result back`` |
|       - |  659 | `		 * through write_property, with no magic accessor involved either way. */` |
|       5 |  660 | `		return 1;` |
|       - |  661 | `	}` |
|      56 |  662 | `	return PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) != 0` |
|      34 |  663 | `	    && PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1) != 0` |
|      54 |  664 | `	    && !VmMagicGuardHeld(pVm,(void *)pThis,pName,'g');` |
|      24 |  665 | `}` |
|       - |  666 | `/*` |
|       - |  667 | `` * php's read-modify-write of an OVERLOADED property — `$o->n++`, `--$o->n`,`` |
|       - |  668 | `` * `$o->n += 2`, `$o->n .= 'x'` on a name the instance does not expose. php reads`` |
|       - |  669 | ` * the current value through __get, lets the modify op compute on it, and writes` |
|       - |  670 | ` * the result back through __set; PHL fell through to dynamic-property creation` |
|       - |  671 | ` * instead, so the ordinary shape (a class declaring BOTH accessors) died on` |
|       - |  672 | `` * `Cannot create dynamic property C::$n` — or, when the name IS declared but`` |
|       - |  673 | `` * inaccessible from this scope, on `Cannot access private property C::$n` —`` |
|       - |  674 | ` * for a statement php runs.` |
|       - |  675 | ` *` |
|       - |  676 | ` * The write-back rides the same pending-entry rail property HOOKS use: the` |
|       - |  677 | ` * current value goes into a fresh SCRATCH memobj the modify op mutates in` |
|       - |  678 | ` * place, and the entry makes that op's tail (PH7_HOOK_RMW_WRITEBACK) dispatch` |
|       - |  679 | ` * __set with the computed value.` |
|       - |  680 | ` *` |
|       - |  681 | ` * BOTH accessors are required, which is php's own split: with only __get php` |
|       - |  682 | ` * goes on to CREATE the property (PHL's scope policy refuses a dynamic property),` |
|       - |  683 | `` * and with only __set it warns `Undefined property` and reads null — neither is`` |
|       - |  684 | ` * this path.` |
|       - |  685 | ` *` |
|       - |  686 | ` * *pOut takes __get's value and *pnScratch the slot the modify op must address;` |
|       - |  687 | ` * the caller does its own stack surgery afterwards, because pName still aliases` |
|       - |  688 | `` * the NAME operand when the name is dynamic (`$o->$k++`) and releasing that`` |
|       - |  689 | ` * operand first would leave it dangling. *pnScratch stays SXU32_HIGH when __get` |
|       - |  690 | ` * threw (nothing is armed — the fetch-point router lands the parked throw and` |
|       - |  691 | ` * php's __set never runs) or when the scratch reservation failed.` |
|       - |  692 | ` */` |
|      32 |  693 | `static void VmMagicRmwArm(` |
|       - |  694 | `	ph7_vm *pVm,` |
|       - |  695 | `	ph7_class_instance *pThis,` |
|       - |  696 | `	ph7_class *pClass,` |
|       - |  697 | `	const SyString *pName,` |
|       - |  698 | `	ph7_value *pOut,` |
|       - |  699 | `	sxu32 *pnScratch,` |
|       - |  700 | `	void *pOwnerStack,` |
|       - |  701 | `	void *pInstrs,` |
|       - |  702 | `	sxu32 nPc` |
|       - |  703 | `	)` |
|       1 |  704 | `{` |
|       - |  705 | `	ph7_value *pScr;` |
|       - |  706 | `	VmHookRmw sRmw;` |
|      33 |  707 | `	*pnScratch = SXU32_HIGH;` |
|      33 |  708 | `	VmMagicGuardPush(pVm,(void *)pThis,pName,'g');` |
|      33 |  709 | `	PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,pName,pOut);` |
|      33 |  710 | `	VmMagicGuardPop(pVm);` |
|      33 |  711 | `	if( pVm->nBoundaryRc != 0 ){` |
|       3 |  712 | `		PH7_MemObjRelease(pOut);` |
|       3 |  713 | `		return;` |
|       - |  714 | `	}` |
|      31 |  715 | `	pScr = PH7_ReserveMemObj(&(*pVm));` |
|      31 |  716 | `	if( pScr == 0 ){` |
|       - |  717 | `		/* OOM: loud allocator diagnostics already fired; the value still stands. */` |
|     ! 0 |  718 | `		return;` |
|       - |  719 | `	}` |
|      31 |  720 | `	PH7_MemObjStore(pOut,pScr);` |
|      31 |  721 | `	sRmw.iKind = VM_HOOK_PEND_RMW_MAGIC;` |
|      31 |  722 | `	sRmw.pThis = pThis;` |
|      31 |  723 | `	sRmw.pAttr = 0;` |
|      31 |  724 | `	sRmw.nBackIdx = SXU32_HIGH;` |
|      31 |  725 | `	sRmw.nScratchIdx = pScr->nIdx;` |
|      31 |  726 | `	SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      31 |  727 | `	SyBlobAppend(&sRmw.sName,(const void *)pName->zString,pName->nByte);` |
|      31 |  728 | `	sRmw.pOwnerStack = pOwnerStack;` |
|      31 |  729 | `	sRmw.pInstrs = pInstrs;` |
|      31 |  730 | `	sRmw.nJmpPc = nPc;  /* the modify op ... */` |
|      31 |  731 | `	sRmw.nPc = nPc;     /* ... is the whole window */` |
|      31 |  732 | `	pThis->iRef++;` |
|      31 |  733 | `	SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      31 |  734 | `	*pnScratch = pScr->nIdx;` |
|      17 |  735 | `}` |
|       - |  736 | `/*` |
|       - |  737 | `` * The property php's `C::$name` form finds. That form reads the class's whole`` |
|       - |  738 | ` * property table -- instance properties and constants included, answering on` |
|       - |  739 | ` * visibility before static-ness (see the arm that uses it) -- and php's table` |
|       - |  740 | `` * carries a base's PRIVATE instance property as a SHADOW entry, so `B::$q` on`` |
|       - |  741 | `` * `class A { private $q; }` is "Cannot access private property B::$q" and not the`` |
|       - |  742 | ` * undeclared-static sentence. Here that member lives under its mangled STORAGE` |
|       - |  743 | ` * name, which the plain probe cannot see, so the ancestry answers for it: the` |
|       - |  744 | ` * nearest base that declares one under the plain name.` |
|       - |  745 | ` */` |
|  101986 |  746 | `static ph7_class_attr * VmClassAttrWithShadow(ph7_class *pClass,const char *zName,sxu32 nName)` |
|       5 |  747 | `{` |
|       - |  748 | `	ph7_class *pWalk;` |
|  101991 |  749 | `	ph7_class_attr *pAttr = PH7_ClassExtractAttribute(pClass,zName,nName);` |
|  101991 |  750 | `	if( pAttr ){` |
|  101977 |  751 | `		return pAttr;` |
|       - |  752 | `	}` |
|      17 |  753 | `	for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){` |
|     ! 0 |  754 | `		pAttr = PH7_ClassExtractAttribute(pWalk,zName,nName);` |
|     ! 0 |  755 | `		if( pAttr ){` |
|     ! 0 |  756 | `			return (pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|     ! 0 |  757 | `			     && (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0)` |
|     ! 0 |  758 | `				? pAttr : 0;` |
|       - |  759 | `		}` |
|     ! 0 |  760 | `	}` |
|      17 |  761 | `	return 0;` |
|   50998 |  762 | `}` |
|       - |  763 | `/*` |
|       - |  764 | ` * May the executing scope touch this class-level member reached through an` |
|       - |  765 | `` * INSTANCE (`$o->s` on a static, or on a class constant)?`` |
|       - |  766 | ` *` |
|       - |  767 | ` * The ordinary attribute rule, plus php's shadow re-lookup: when the member the` |
|       - |  768 | ` * object's class holds under this name is a private one the scope may not touch,` |
|       - |  769 | ` * php asks the SCOPE's own table for the name and uses what it finds there. With` |
|       - |  770 | `` * `class A { private static $q; } class B extends A { private static $q; }` that`` |
|       - |  771 | `` * is how `$b->q` from inside A reaches A's own -- the as-non-static notice and`` |
|       - |  772 | ` * then the ordinary undefined-property answer, rather than a visibility refusal` |
|       - |  773 | ` * about B's.` |
|       - |  774 | ` */` |
|      24 |  775 | `static int VmStaticThroughInstanceVisible(ph7_vm *pVm,ph7_class *pClass,` |
|       - |  776 | `	ph7_class_attr *pAttr,const SyString *pName)` |
|       3 |  777 | `{` |
|       - |  778 | `	ph7_class *pScope;` |
|       - |  779 | `	ph7_class_attr *pShadow;` |
|      27 |  780 | `	if( PH7_VmClassAttrAccess(&(*pVm),pClass,pAttr,FALSE) ){` |
|      20 |  781 | `		return 1;` |
|       - |  782 | `	}` |
|       8 |  783 | `	if( pAttr->iProtection != PH7_CLASS_PROT_PRIVATE \|\| pName->nByte < 1 ){` |
|     ! 0 |  784 | `		return 0;` |
|       - |  785 | `	}` |
|       8 |  786 | `	pScope = PH7_VmCallerScope(&(*pVm));` |
|       8 |  787 | `	if( pScope == 0 \|\| !PH7_VmInstanceOf(pClass,pScope) ){` |
|       8 |  788 | `		return 0;` |
|       - |  789 | `	}` |
|     ! 0 |  790 | `	pShadow = PH7_ClassExtractAttribute(pScope,pName->zString,pName->nByte);` |
|     ! 0 |  791 | `	return pShadow != 0` |
|     ! 0 |  792 | `		&& pShadow != pAttr` |
|     ! 0 |  793 | `		&& pShadow->iProtection == PH7_CLASS_PROT_PRIVATE` |
|     ! 0 |  794 | `		&& PH7_VmMemberOwnerClass(pShadow->pDeclClass,pScope) == pScope;` |
|      15 |  795 | `}` |
|       - |  796 | `/*` |
|       - |  797 | ` * OP_MEMBER: body moved verbatim from the OP_MEMBER arm of` |
|       - |  798 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - |  799 | ` */` |
|  465952 |  800 | `PH7_PRIVATE VmOpRc VmExecOpMember(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 |  801 | `{` |
|  465957 |  802 | `	ph7_value *pTos = pState->pTos;` |
|  465957 |  803 | `	ph7_value *pStack = pState->pStack;` |
|  465957 |  804 | `	VmInstr *aInstr = pState->aInstr;` |
|  465957 |  805 | `	sxi32 pc = pState->pc;` |
|       - |  806 | `	sxi32 rc;` |
|       - |  807 | `	ph7_class_instance *pThis;` |
|       - |  808 | `	ph7_value *pNos;` |
|       - |  809 | `	SyString sName;` |
|  465957 |  810 | ``	int bStaticHidden = 0; /* a `::$name` already refused for VISIBILITY */`` |
|  465957 |  811 | `	if( !pInstr->iP1 ){` |
|  341390 |  812 | `		pNos = &pTos[-1];` |
|       - |  813 | `#ifdef UNTRUST` |
|       - |  814 | `		if( pNos < pStack ){` |
|       - |  815 | `			VM_EXIT_ABORT;` |
|       - |  816 | `		}` |
|       - |  817 | `#endif` |
|       - |  818 | `		/* What a dynamic NAME may BE, decided before the receiver is looked at --` |
|       - |  819 | `		 * php settles this in the opcode handler's first lines, and the two name` |
|       - |  820 | `		 * positions settle it differently.` |
|       - |  821 | `		 *` |
|       - |  822 | `		 * A METHOD name must ALREADY be a string: zend's INIT_METHOD_CALL refuses` |
|       - |  823 | ``		 * anything else outright with `Method name must be a string`, before the`` |
|       - |  824 | `		 * receiver's type, before __call, and before the name would be looked up.` |
|       - |  825 | `		 * A PROPERTY name is COERCED instead, with the user-visible rules -- an` |
|       - |  826 | ``		 * array warns `Array to string conversion` and renders "Array", a float,`` |
|       - |  827 | `		 * bool or null spells itself out, an object hands over its __toString()` |
|       - |  828 | ``		 * and one without it is php's catchable `Object of class X could not be`` |
|       - |  829 | ``		 * converted to string`.`` |
|       - |  830 | `		 *` |
|       - |  831 | `		 * PHL read the name slot's RAW BLOB, which is empty for every value that` |
|       - |  832 | ``		 * is not already a string, so `$o->{5}`, `$o->{1.5}`, `$o->{true}` and`` |
|       - |  833 | ``		 * `$o->{$stringable}` all named the property "" -- one shared property per`` |
|       - |  834 | `		 * object, silently, on every access shape (read, write, isset, unset,` |
|       - |  835 | ``		 * increment, by-ref) -- and `$o->{[1]}` on an object with no such property`` |
|       - |  836 | `		 * SEGFAULTED, because an empty blob hands out a NULL pointer that the` |
|       - |  837 | `		 * dynamic-property path dereferences.` |
|       - |  838 | `		 *` |
|       - |  839 | `		 * php's own exception is the one shape that answers before it ever asks` |
|       - |  840 | ``		 * for the name: a lookup (isset/empty/`??`) or an unset() whose receiver`` |
|       - |  841 | `		 * is not an object short-circuits, so no coercion and no diagnostic. A` |
|       - |  842 | ``		 * `?->` on null never reaches here at all -- OP_NULLSAFE_JMP has already`` |
|       - |  843 | `		 * jumped past both the name expression and this op.` |
|       - |  844 | `		 */` |
|  341390 |  845 | `		if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|  183491 |  846 | `			if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - |  847 | `				sxi32 rcMn;` |
|      11 |  848 | `				VmPopOperand(&pTos,1);` |
|      11 |  849 | `				PH7_MemObjRelease(pTos);` |
|      11 |  850 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      11 |  851 | `				pTos->nIdx = SXU32_HIGH;` |
|      11 |  852 | `				rcMn = VmThrowFromVm(&(*pVm),"Error","Method name must be a string",` |
|       - |  853 | `					sizeof("Method name must be a string")-1);` |
|      11 |  854 | `				if( rcMn == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 |  855 | `				rc = rcMn;` |
|      11 |  856 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  857 | `			}` |
|  249597 |  858 | `		}else if( (pNos->iFlags & MEMOBJ_OBJ)` |
|   80743 |  859 | `		       \|\| (pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2)) ){` |
|  157844 |  860 | `			sxi32 rcNm = PH7_MemObjToStringUV(pTos);` |
|  157844 |  861 | `			if( rcNm != SXRET_OK ){` |
|       3 |  862 | `				VmPopOperand(&pTos,1);` |
|       3 |  863 | `				PH7_MemObjRelease(pTos);` |
|       3 |  864 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|       3 |  865 | `				pTos->nIdx = SXU32_HIGH;` |
|       3 |  866 | `				if( rcNm == PH7_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  867 | `				rc = rcNm;` |
|       3 |  868 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  869 | `			}` |
|   78919 |  870 | `		}` |
|  341373 |  871 | `		if( pInstr->iP2 == PH7_MEMBER_DEFPATH` |
|  177230 |  872 | `		 && (pNos->iFlags & (MEMOBJ_AUX_DEFPATH\|MEMOBJ_AUX_DEFERRED)) ){` |
|       - |  873 | `			/* D1 commit 2: deferred property arg ($o->p) whose base is itself a deferred lvalue` |
|       - |  874 | ``			 * (a nested chain `$a["k"]->p`, or an undefined base object). Append a property step`` |
|       - |  875 | `			 * to the base's captured path; OP_CALL re-walks it. */` |
|       - |  876 | `			SyString sProp;` |
|    3413 |  877 | `			VmDeferredPath *pPath = 0;` |
|    3413 |  878 | `			SyStringInitFromBuf(&sProp,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|    3413 |  879 | `			if( pNos->iFlags & MEMOBJ_AUX_DEFPATH ){` |
|    3408 |  880 | `				pPath = (VmDeferredPath *)pNos->x.pOther;` |
|    3408 |  881 | `				VmDeferPathPushProp(pPath,&sProp);` |
|    1705 |  882 | `			}else{` |
|       - |  883 | `				SyString sRootName;` |
|       6 |  884 | `				SyStringInitFromBuf(&sRootName,(const char *)pNos->x.pOther,` |
|       - |  885 | `					pNos->x.pOther ? SyStrlen((const char *)pNos->x.pOther) : 0);` |
|       6 |  886 | `				pPath = VmDeferPathNew(&(*pVm),1,SXU32_HIGH,&sRootName);` |
|       6 |  887 | `				if( pPath ){` |
|       6 |  888 | `					pNos->iFlags &= ~MEMOBJ_AUX_DEFERRED; /* borrowed name */` |
|       6 |  889 | `					pNos->x.pOther = pPath;` |
|       6 |  890 | `					pNos->iFlags \|= MEMOBJ_AUX_DEFPATH;` |
|       6 |  891 | `					pNos->nIdx = SXU32_HIGH;` |
|       6 |  892 | `					VmDeferPathPushProp(pPath,&sProp);` |
|       2 |  893 | `				}` |
|       - |  894 | `			}` |
|    3413 |  895 | `			VmPopOperand(&pTos,1); /* drop the property name; the carrier stays as pTos */` |
|    3413 |  896 | `			VM_EXIT_BREAK;` |
|       - |  897 | `		}` |
|  337968 |  898 | `		if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - |  899 | `			ph7_class *pClass;` |
|       - |  900 | `			/* Class already instantiated */` |
|  337778 |  901 | `			pThis = (ph7_class_instance *)pNos->x.pOther;` |
|       - |  902 | `			/* Point to the instantiated class */` |
|  337778 |  903 | `			pClass = pThis->pClass;` |
|       - |  904 | `			/* Extract attribute name first */` |
|  337778 |  905 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|  337778 |  906 | `			if( PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
|       - |  907 | `				/* __PHP_Incomplete_Class: every property access and method call is` |
|       - |  908 | `				 * php's incomplete-object diagnostic — the carrier's OWN entries are` |
|       - |  909 | `				 * for the engine's surfaces only. A READ (isset/empty/?? included)` |
|       - |  910 | `				 * is an E_WARNING answering NULL; every WRITE shape and unset() is` |
|       - |  911 | `				 * a catchable Error; a method call is its own Error, raised where` |
|       - |  912 | `				 * the undefined-method twin raises (before any argument runs). The` |
|       - |  913 | `				 * DEFPATH pre-pass defers like a MISSING property would — the slot` |
|       - |  914 | `				 * fast path must not hand out the carrier's storage — so the by-ref` |
|       - |  915 | `				 * resolve (VmBindPropByRef) raises the Error and the by-value` |
|       - |  916 | `				 * re-drive lands back here in READ context for the warning.` |
|       - |  917 | ``				 * `??=` reads before it refuses, so it takes BOTH, php's order. */`` |
|      37 |  918 | `				VmInstr *pIncNext = pInstr + 1;` |
|      37 |  919 | `				if( pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|       - |  920 | `					SyString sIncProp;` |
|       5 |  921 | `					VmDeferredPath *pIncPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|       5 |  922 | `					SyStringInitFromBuf(&sIncProp,sName.zString,sName.nByte);` |
|       5 |  923 | `					if( pIncPath && VmDeferPathPushProp(pIncPath,&sIncProp) == SXRET_OK ){` |
|       5 |  924 | `						VmPopOperand(&pTos,1);       /* drop the property name */` |
|       5 |  925 | `						pThis->iRef++;` |
|       5 |  926 | `						PH7_MemObjRelease(pTos);     /* collapse the object slot into the carrier */` |
|       5 |  927 | `						pTos->x.pOther = pIncPath;` |
|       5 |  928 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|       5 |  929 | `						pTos->nIdx = SXU32_HIGH;` |
|       5 |  930 | `						PH7_ClassInstanceUnref(pThis);` |
|       5 |  931 | `						VM_EXIT_BREAK;` |
|       - |  932 | `					}` |
|     ! 0 |  933 | `					if( pIncPath ){` |
|     ! 0 |  934 | `						VmFreeDeferredPath(pIncPath);` |
|     ! 0 |  935 | `					}` |
|       - |  936 | `					/* allocation failure (or a temporary base below): the read` |
|       - |  937 | `					 * verdict is all that is left — fall through to warn + NULL. */` |
|     ! 0 |  938 | `				}` |
|      33 |  939 | `				int bIncCall = (pInstr->iP2 == PH7_MEMBER_METHOD);` |
|      33 |  940 | `				int bIncCoalW = (pInstr->iP2 == PH7_MEMBER_WRITE && pIncNext->iOp == PH7_OP_NULLC_JMP);` |
|      65 |  941 | `				int bIncModify = pInstr->iP2 != PH7_MEMBER_DEFPATH` |
|      63 |  942 | `					&& (pInstr->iP2 == PH7_MEMBER_UNSET` |
|      31 |  943 | `					 \|\| pInstr->iP2 == PH7_MEMBER_WRITE` |
|      25 |  944 | `					 \|\| pInstr->iP2 == PH7_MEMBER_REF_TARGET` |
|      19 |  945 | `					 \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|      18 |  946 | `					 \|\| VmMemberNextIsWrite(pIncNext)` |
|      18 |  947 | `					 \|\| VmMemberFetchForWrite(pInstr));` |
|      33 |  948 | `				if( bIncCall ){` |
|       - |  949 | `					SyBlob sIncErr;` |
|       - |  950 | `					sxi32 rcInc;` |
|       3 |  951 | `					SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|       3 |  952 | `					PH7_VmIncompleteMsg(&(*pVm),pThis,"call a method",&sIncErr);` |
|       3 |  953 | `					VmPopOperand(&pTos,1);` |
|       3 |  954 | `					PH7_MemObjRelease(pTos);` |
|       3 |  955 | `					pTos->nIdx = SXU32_HIGH;` |
|       4 |  956 | `					rcInc = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sIncErr),` |
|       1 |  957 | `						SyBlobLength(&sIncErr));` |
|       3 |  958 | `					SyBlobRelease(&sIncErr);` |
|       3 |  959 | `					if( rcInc == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 |  960 | `					rc = rcInc;` |
|       3 |  961 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - |  962 | `				}` |
|      31 |  963 | `				if( bIncCoalW ){` |
|       3 |  964 | `					PH7_VmIncompleteAccessWarn(&(*pVm),pThis,0);` |
|       1 |  965 | `				}` |
|      31 |  966 | `				if( bIncModify ){` |
|       - |  967 | `					SyBlob sIncErr;` |
|      17 |  968 | `					SyBlobInit(&sIncErr,&pVm->sAllocator);` |
|      17 |  969 | `					PH7_VmIncompleteMsg(&(*pVm),pThis,"modify a property",&sIncErr);` |
|      17 |  970 | `					VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sIncErr));` |
|       9 |  971 | `				}else{` |
|      15 |  972 | `					PH7_VmIncompleteAccessWarn(&(*pVm),pThis,0);` |
|       - |  973 | `				}` |
|      31 |  974 | `				VmPopOperand(&pTos,1);   /* pop the attribute name */` |
|      31 |  975 | `				PH7_MemObjRelease(pTos); /* the object slot becomes the NULL answer */` |
|      31 |  976 | `				pTos->nIdx = SXU32_HIGH;` |
|      31 |  977 | `				VM_EXIT_BREAK;` |
|       - |  978 | `			}` |
|  337742 |  979 | `			if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - |  980 | `				/* Method call */` |
|  183465 |  981 | `				ph7_class_method *pMeth = 0;` |
|  183465 |  982 | `				int bFwdCall = 0; /* re-targeted to a dual iterator's inner: no scope check */` |
|  183465 |  983 | `				if( sName.nByte > 0 ){` |
|       - |  984 | `					/* Extract the target method */` |
|  183465 |  985 | `					pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|   91685 |  986 | `				}` |
|  183465 |  987 | `				if( pMeth == 0 ){` |
|       - |  988 | `					/* php's dual iterators forward an unknown method to the one they` |
|       - |  989 | `					 * WRAP (spl_dual_it_call_method), ahead of __call and of the` |
|       - |  990 | `					 * undefined-method Error. Re-target the receiver slot the call` |
|       - |  991 | ``					 * below binds `$this` from and carry on down the ordinary path, so`` |
|       - |  992 | `					 * the visibility rules and the screened-name marking are the same` |
|       - |  993 | `					 * ones every other method call takes. */` |
|     126 |  994 | `					ph7_class_instance *pFwdInner = 0;` |
|     126 |  995 | `					ph7_class_method *pFwdMeth = 0;` |
|     126 |  996 | `					if( PH7_SplOuterForward(&(*pVm),pThis,&sName,&pFwdInner,&pFwdMeth) ){` |
|      25 |  997 | `						ph7_class_instance *pOuter = pThis;` |
|      25 |  998 | `						pFwdInner->iRef++;      /* the stack slot's new reference */` |
|      25 |  999 | `						pNos->x.pOther = pFwdInner;` |
|      25 | 1000 | `						PH7_ClassInstanceUnref(pOuter); /* ...and drop its old one */` |
|      25 | 1001 | `						pThis = pFwdInner;` |
|      25 | 1002 | `						pClass = pFwdInner->pClass;` |
|      25 | 1003 | `						pMeth = pFwdMeth;` |
|       - | 1004 | `						/* php's forward is a direct zend_call_method with no calling` |
|       - | 1005 | `						 * scope, so the inner's own visibility does not apply: a` |
|       - | 1006 | `						 * PROTECTED method on the wrapped iterator answers. */` |
|      25 | 1007 | `						bFwdCall = 1;` |
|      12 | 1008 | `					}` |
|      61 | 1009 | `				}` |
|  183465 | 1010 | `				if( pMeth == 0 ){` |
|     102 | 1011 | `					ph7_class_method *pCallMagic = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|     102 | 1012 | `					if( pCallMagic ){` |
|       - | 1013 | `						/* php: a missing method dispatches __call($name, $args) — the` |
|       - | 1014 | `						 * args are only collected by the following OP_CALL, so stash the` |
|       - | 1015 | `						 * receiver + class + original name and MARK the callee slot` |
|       - | 1016 | `						 * (band A #3b; pre-fix the name-only call discarded everything` |
|       - | 1017 | `						 * and the call site failed with "Invalid function name"). Stack:` |
|       - | 1018 | `						 * pop the method name, then the receiver slot becomes the marked` |
|       - | 1019 | `						 * carrier OP_CALL routes through the packing body. */` |
|      60 | 1020 | `						VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pThis,pClass,&sName);` |
|      60 | 1021 | `						if( pPend == 0 ){` |
|     ! 0 | 1022 | `							VM_EXIT_ABORT;` |
|       - | 1023 | `						}` |
|      60 | 1024 | `						VmPopOperand(&pTos,1);` |
|      60 | 1025 | `						PH7_MemObjRelease(pTos);` |
|      60 | 1026 | `						pTos->x.pOther = pPend;` |
|      60 | 1027 | `						pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|      32 | 1028 | `					}else{` |
|       - | 1029 | `						{` |
|       - | 1030 | `							/* php raises a catchable Error here; PH7 printed a notice and CARRIED ON` |
|       - | 1031 | `							 * with NULL, so the call silently produced nothing. */` |
|       - | 1032 | `							SyBlob sErrM;` |
|       - | 1033 | `							sxi32 rcErr;` |
|      43 | 1034 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      43 | 1035 | `							SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",&pClass->sDisp,&sName);` |
|      43 | 1036 | `							VmPopOperand(&pTos,1);` |
|      43 | 1037 | `							PH7_MemObjRelease(pTos);` |
|      43 | 1038 | `							pTos->nIdx = SXU32_HIGH;` |
|      64 | 1039 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      21 | 1040 | `								SyBlobLength(&sErrM));` |
|      43 | 1041 | `							SyBlobRelease(&sErrM);` |
|      43 | 1042 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      43 | 1043 | `							rc = rcErr;` |
|      43 | 1044 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1045 | `						}` |
|       - | 1046 | `					}` |
|      32 | 1047 | `				}else{` |
|  183367 | 1048 | `					ph7_class_method *pDeniedCall = 0;` |
|  183367 | 1049 | `					int bDenied = 0;` |
|       - | 1050 | `					/* php decides a method's visibility against the class that OWNS it,` |
|       - | 1051 | `					 * which for a trait method is the class that composed it — a trait is` |
|       - | 1052 | `					 * a compile-time construct php has flattened away by now. Deciding` |
|       - | 1053 | `					 * against the trait instead made every rule a question of who USES it:` |
|       - | 1054 | `					 * a protected trait method was denied to a SUBCLASS of the composing` |
|       - | 1055 | `					 * class (not a trait user itself) and granted to an unrelated class` |
|       - | 1056 | `					 * that happened to use the same trait. */` |
|  183367 | 1057 | `					ph7_class *pOwner = 0;` |
|  183362 | 1058 | `					if( !bFwdCall && pMeth->iProtection != PH7_CLASS_PROT_PUBLIC` |
|   91694 | 1059 | `					 && !PH7_VmClassMemberAccess(&(*pVm),` |
|     130 | 1060 | `						(pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth)),` |
|      65 | 1061 | `						&sName,pMeth->iProtection,FALSE) ){` |
|      58 | 1062 | `						int bRebound = 0;` |
|      58 | 1063 | `						if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|       - | 1064 | `							/* php: when the instance's class SHADOWS the caller's` |
|       - | 1065 | `							 * private method (child redeclares private m), code in` |
|       - | 1066 | `							 * the caller's class dispatches its OWN private m, not` |
|       - | 1067 | `							 * the shadow. Rebind to the calling scope's method when` |
|       - | 1068 | `							 * that scope declares a private one of this name. */` |
|      50 | 1069 | `							VmFrame *pFrameLocal = VmSkipExceptionFrames(pVm->pFrame);` |
|      50 | 1070 | `							ph7_vm_func *pCallerFunc = (ph7_vm_func *)pFrameLocal->pUserData;` |
|      50 | 1071 | `							if( pCallerFunc && (pCallerFunc->iFlags & VM_FUNC_CLASS_METHOD) && pCallerFunc->pUserData ){` |
|       6 | 1072 | `								ph7_class *pScope = (ph7_class *)pCallerFunc->pUserData;` |
|       6 | 1073 | `								ph7_class_method *pOwn = PH7_ClassExtractMethod(pScope,sName.zString,sName.nByte);` |
|       4 | 1074 | `								if( pOwn && pOwn->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       6 | 1075 | `								 && (ph7_class *)pOwn->sFunc.pUserData == pScope ){` |
|       6 | 1076 | `									pMeth = pOwn;` |
|       6 | 1077 | `									bRebound = 1;` |
|       2 | 1078 | `								}` |
|       2 | 1079 | `							}` |
|      23 | 1080 | `						}` |
|      58 | 1081 | `						if( !bRebound ){` |
|       - | 1082 | `							/* Inaccessible from this scope: php routes through __call when` |
|       - | 1083 | `							 * declared (band A #3b); without it the refusal is raised right` |
|       - | 1084 | `							 * here, beside the undefined-method and abstract-method twins. */` |
|      54 | 1085 | `							pDeniedCall = PH7_ClassExtractMethod(pClass,"__call",sizeof("__call")-1);` |
|      54 | 1086 | `							bDenied = pDeniedCall == 0;` |
|      25 | 1087 | `						}` |
|      27 | 1088 | `					}` |
|  183367 | 1089 | `					if( bDenied ){` |
|       - | 1090 | ``						/* php's visibility Error for `$o->m()`. It is raised HERE, at the`` |
|       - | 1091 | `						 * member resolution, and not left to OP_CALL: the method OP_CALL` |
|       - | 1092 | `						 * would re-derive is looked up by the FUNCTION's own name against` |
|       - | 1093 | `						 * its declaring class, which a trait adaptation splits away from` |
|       - | 1094 | ``						 * the entry this lookup actually chose — `hi as private pHi` gave`` |
|       - | 1095 | ``						 * OP_CALL the trait's public `hi`, so the private alias ran from`` |
|       - | 1096 | `						 * global scope in silence. The entry that answered is right here,` |
|       - | 1097 | `						 * with its composed protection.` |
|       - | 1098 | `						 *` |
|       - | 1099 | ``						 * php names the method as the CALL SPELLS it (`$o->PQ()` on a`` |
|       - | 1100 | ``						 * private `pQ()` reports `PQ`) and the class that OWNS the method,`` |
|       - | 1101 | `						 * which for a trait method is the composing class. */` |
|       - | 1102 | `						SyBlob sErrM;` |
|       - | 1103 | `						sxi32 rcErr;` |
|      44 | 1104 | `						ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|      64 | 1105 | `						const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      20 | 1106 | `							? "private" : "protected";` |
|      44 | 1107 | `						SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      44 | 1108 | `						if( pScope ){` |
|       7 | 1109 | `							SyBlobFormat(&sErrM,"Call to %s method %z::%z() from scope %z",` |
|       3 | 1110 | `								zVis,&pOwner->sDisp,&sName,&pScope->sDisp);` |
|       4 | 1111 | `						}else{` |
|      38 | 1112 | `							SyBlobFormat(&sErrM,"Call to %s method %z::%z() from global scope",` |
|      17 | 1113 | `								zVis,&pOwner->sDisp,&sName);` |
|       - | 1114 | `						}` |
|      44 | 1115 | `						VmPopOperand(&pTos,1);` |
|      44 | 1116 | `						PH7_MemObjRelease(pTos);` |
|      44 | 1117 | `						pTos->nIdx = SXU32_HIGH;` |
|      64 | 1118 | `						rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      20 | 1119 | `							SyBlobLength(&sErrM));` |
|      44 | 1120 | `						SyBlobRelease(&sErrM);` |
|      44 | 1121 | `						if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      44 | 1122 | `						rc = rcErr;` |
|      48 | 1123 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1124 | `					}` |
|  183327 | 1125 | `					if( pDeniedCall ){` |
|      11 | 1126 | `						VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pThis,pClass,&sName);` |
|      11 | 1127 | `						if( pPend == 0 ){` |
|     ! 0 | 1128 | `							VM_EXIT_ABORT;` |
|       - | 1129 | `						}` |
|      11 | 1130 | `						VmPopOperand(&pTos,1);` |
|      11 | 1131 | `						PH7_MemObjRelease(pTos);` |
|      11 | 1132 | `						pTos->x.pOther = pPend;` |
|      11 | 1133 | `						pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|       6 | 1134 | `					}else{` |
|       - | 1135 | `						/* Push method name on the stack, MARKED as already screened: the` |
|       - | 1136 | `						 * decision above was made against the entry this lookup chose,` |
|       - | 1137 | `						 * which is the only place a trait adaptation's composed` |
|       - | 1138 | `						 * protection is visible. */` |
|  183317 | 1139 | `						PH7_MemObjRelease(pTos);` |
|  183317 | 1140 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|  183317 | 1141 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|  183317 | 1142 | `						pTos->iFlags \|= MEMOBJ_AUX_MEMBERCALL;` |
|       - | 1143 | `					}` |
|       - | 1144 | `				}` |
|  183383 | 1145 | `				pTos->nIdx = SXU32_HIGH;` |
|   91649 | 1146 | `			}else{` |
|       - | 1147 | `				/* Attribute access. iP2: 0 = read, 2 = unset, 3 = isset, 4 = empty. */` |
|  154282 | 1148 | `				VmClassAttr *pObjAttr = 0;` |
|  154282 | 1149 | `				SyHashEntry *pEntry = 0;` |
|       - | 1150 | `				/* A LAZY native property read before anything installed it, whose` |
|       - | 1151 | `				 * class answers such a read from its zeroed struct rather than` |
|       - | 1152 | `				 * calling the name undefined (PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT).` |
|       - | 1153 | `				 * Set by the miss handling below and answered after the pop. */` |
|  154282 | 1154 | `				ph7_class_attr *pLazyDefault = 0;` |
|       - | 1155 | `				/* An UNINITIALIZED typed slot handed to the deferred call-argument rail` |
|       - | 1156 | `				 * below, which records it whether or not the class declares __get: php` |
|       - | 1157 | `				 * consults no magic accessor for a typed property that was never written` |
|       - | 1158 | `				 * (only an unset() one reaches __get there), and the two diagnostics it` |
|       - | 1159 | `				 * DOES have -- one for a by-value binding, one for a by-reference one --` |
|       - | 1160 | `				 * are exactly what the rail exists to tell apart. */` |
|  154282 | 1161 | `				int bDeferUninit = 0;` |
|  154277 | 1162 | `				if( sName.nByte > 0 && sName.zString[0] == 0` |
|   77152 | 1163 | `				 && !PH7_VmIsIncompleteClass(&(*pVm),pClass) ){` |
|       - | 1164 | `					/* php refuses a property name beginning with a NUL outright:` |
|       - | 1165 | ``					 * `Cannot access property starting with "\0"`. Those bytes are`` |
|       - | 1166 | `					 * the ENGINE's private mangling — "\0Cls\0p" is C1::$p and` |
|       - | 1167 | `					 * "\0*\0p" is a protected slot — so a script that could write one` |
|       - | 1168 | `					 * would forge a private member of any class it names, and every` |
|       - | 1169 | `					 * display surface would then render the forgery as the real thing.` |
|       - | 1170 | `					 * Two rules ride with it, both probe-verified: a MAGIC accessor` |
|       - | 1171 | `					 * wins (php dispatches __get/__set/__isset/__unset with the raw` |
|       - | 1172 | `					 * name and says nothing), and a LOOKUP context is silent — isset()` |
|       - | 1173 | ``					 * is false, empty() true, `??` takes the default — which is what`` |
|       - | 1174 | `					 * the existing miss handling below already answers. The refusal` |
|       - | 1175 | `					 * does not depend on whether such a property exists: php raises it` |
|       - | 1176 | `					 * on the NAME, before any lookup, and the one place a NUL-keyed` |
|       - | 1177 | `					 * property legitimately lives is the __PHP_Incomplete_Class` |
|       - | 1178 | `					 * carrier, whose own gate above has already answered for it.` |
|       - | 1179 | ``					 * A METHOD name is not covered — php lets `$o->{"\0m"}()` reach`` |
|       - | 1180 | `					 * the ordinary undefined-method Error — so this sits in the` |
|       - | 1181 | `					 * attribute branch only. */` |
|       - | 1182 | `					const char *zNulMagic;` |
|      51 | 1183 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       5 | 1184 | `						zNulMagic = "__unset";` |
|      49 | 1185 | `					}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|      46 | 1186 | `					       \|\| VmMemberNextIsWrite(pInstr + 1) ){` |
|      19 | 1187 | `						zNulMagic = "__set";` |
|      10 | 1188 | `					}else{` |
|      29 | 1189 | `						zNulMagic = "__get";` |
|       - | 1190 | `					}` |
|      50 | 1191 | `					if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      47 | 1192 | `					 && PH7_ClassExtractMethod(pClass,zNulMagic,(sxu32)SyStrlen(zNulMagic)) == 0 ){` |
|       - | 1193 | `						SyBlob sNulErr;` |
|       - | 1194 | `						sxi32 rcNul;` |
|      33 | 1195 | `						SyBlobInit(&sNulErr,&pVm->sAllocator);` |
|      33 | 1196 | `						SyBlobAppend(&sNulErr,"Cannot access property starting with \"\\0\"",` |
|       - | 1197 | `							sizeof("Cannot access property starting with \"\\0\"")-1);` |
|      33 | 1198 | `						VmPopOperand(&pTos,1);   /* pop the attribute name */` |
|      33 | 1199 | `						PH7_MemObjRelease(pTos); /* the object slot becomes the NULL answer */` |
|      33 | 1200 | `						pTos->nIdx = SXU32_HIGH;` |
|      49 | 1201 | `						rcNul = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sNulErr),` |
|      16 | 1202 | `							SyBlobLength(&sNulErr));` |
|      33 | 1203 | `						SyBlobRelease(&sNulErr);` |
|      33 | 1204 | `						if( rcNul == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      33 | 1205 | `						rc = rcNul;` |
|      33 | 1206 | `						PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 1207 | `					}` |
|       9 | 1208 | `				}` |
|       - | 1209 | `				/* Extract the target attribute, as the class whose code is RUNNING` |
|       - | 1210 | `				 * sees it: a scope that declares a private of this name owns a slot` |
|       - | 1211 | `				 * of its own on every instance below it (php's mangled storage name)` |
|       - | 1212 | `				 * and means THAT one, whatever the object's class holds under the` |
|       - | 1213 | `				 * plain name. The EMPTY name is a real property name in php —` |
|       - | 1214 | ``				 * `$o->{''} = 1` creates one and `$o->{''}` reads it back — and it is`` |
|       - | 1215 | `				 * the one name SyHashGet cannot answer for, so it takes a` |
|       - | 1216 | `				 * list-walking lookup inside.` |
|       - | 1217 | `				 *` |
|       - | 1218 | `				 * The name is hashed ONCE, here, and reused by both questions` |
|       - | 1219 | `				 * underneath -- the scope's private-name screen and the slot probe` |
|       - | 1220 | `				 * on this object -- because every property table shares one hash` |
|       - | 1221 | `				 * function and hashing is what a lookup spends. */` |
|  231368 | 1222 | `				pEntry = PH7_ClassInstanceScopedAttrEntry(&(*pVm),pThis,sName.zString,sName.nByte,` |
|  154245 | 1223 | `					sName.nByte > 0 ? SyHashKey(&pThis->hAttr,(const void *)sName.zString,sName.nByte) : 0);` |
|  154250 | 1224 | `				if( pEntry ){` |
|       - | 1225 | `					/* Point to the attribute value */` |
|  116281 | 1226 | `					pObjAttr = (VmClassAttr *)pEntry->pUserData;` |
|   58138 | 1227 | `				}` |
|  154245 | 1228 | `				if( pObjAttr` |
|  135256 | 1229 | `				 && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT))` |
|   58155 | 1230 | `				 && !VmStaticThroughInstanceVisible(&(*pVm),pClass,pObjAttr->pAttr,&sName) ){` |
|       - | 1231 | `					/* A private static this scope may not touch. When it is an INHERITED` |
|       - | 1232 | ``					 * one php's instance path does not see it at all: `$b->s` is an`` |
|       - | 1233 | `					 * ordinary MISSING property (warn and null, a dynamic write, a silent` |
|       - | 1234 | `					 * unset), with none of the visibility refusal the DECLARING class's` |
|       - | 1235 | `					 * own instance gets. The declaring class's own keeps the refusal. */` |
|       9 | 1236 | `					if( PH7_VmMemberOwnerClass(pObjAttr->pAttr->pDeclClass,pClass) != pClass ){` |
|       3 | 1237 | `						pEntry = 0;` |
|       3 | 1238 | `						pObjAttr = 0;` |
|       1 | 1239 | `					}` |
|  154244 | 1240 | `				}else if( pObjAttr` |
|  135255 | 1241 | `				 && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) ){` |
|       - | 1242 | `					/* A static property (or a constant) belongs to the CLASS: php does` |
|       - | 1243 | `					 * not find it through an instance at all. It notices the attempt —` |
|       - | 1244 | ``					 * `Accessing static property C::$s as non static` — and then treats`` |
|       - | 1245 | `					 * the name as an ordinary MISSING property: a read warns and answers` |
|       - | 1246 | `					 * null, isset() is false, a write goes to a dynamic property (which` |
|       - | 1247 | `					 * PHL rejects by the scope policy, like any other undeclared write).` |
|       - | 1248 | `					 * PHL's instance table carries an entry for every declared member` |
|       - | 1249 | ``					 * (statics share the class slot), so `$o->s` used to READ and — far`` |
|       - | 1250 | `					 * worse — WRITE the class's own static in silence. isset()/empty()` |
|       - | 1251 | `					 * pass silently, as php's do. */` |
|      18 | 1252 | `					if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      18 | 1253 | `					 && pInstr->iP2 != PH7_MEMBER_DEFPATH ){` |
|       - | 1254 | `						/* Silent where php is silent: isset()/empty(), and any class` |
|       - | 1255 | `						 * that declares the MAGIC accessor this context would dispatch` |
|       - | 1256 | ``						 * (php passes `silent = ce->__get/__set/__unset != NULL` to its`` |
|       - | 1257 | `						 * property-offset lookup). Once per access, too: the deferred` |
|       - | 1258 | `						 * call-argument PRE-PASS (PH7_MEMBER_DEFPATH) re-runs this op for` |
|       - | 1259 | ``						 * the same source `$o->s` and the value pass carries the notice`` |
|       - | 1260 | `						 * — the BY-REF form has no value pass and notices at its own` |
|       - | 1261 | `						 * site (VmBindPropByRef). */` |
|       - | 1262 | `						const char *zMagic;` |
|      10 | 1263 | `						if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       3 | 1264 | `							zMagic = "__unset";` |
|       8 | 1265 | `						}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|       8 | 1266 | `						       \|\| VmMemberNextIsWrite(pInstr + 1) ){` |
|       3 | 1267 | `							zMagic = "__set";` |
|       2 | 1268 | `						}else{` |
|       6 | 1269 | `							zMagic = "__get";` |
|       - | 1270 | `						}` |
|      10 | 1271 | `						if( PH7_ClassExtractMethod(pClass,zMagic,(sxu32)SyStrlen(zMagic)) == 0 ){` |
|      11 | 1272 | `							VmErrorFormat(&(*pVm),PH7_CTX_NOTICE,` |
|       - | 1273 | `								"Accessing static property %z::$%z as non static",` |
|       6 | 1274 | `								&pClass->sDisp,&pObjAttr->pAttr->sName);` |
|       3 | 1275 | `						}` |
|       4 | 1276 | `					}` |
|      20 | 1277 | `					pEntry = 0;` |
|      20 | 1278 | `					pObjAttr = 0;` |
|       9 | 1279 | `				}` |
|       - | 1280 | `				/* ...and a LAZY SLOT is the second door into the same handler: the` |
|       - | 1281 | ``				 * name IS on the object from `new`, in its declared position and`` |
|       - | 1282 | `				 * uninitialized, so the miss test above would never fire for it and` |
|       - | 1283 | `				 * the read would raise the uninitialized-typed Error instead of` |
|       - | 1284 | `				 * asking the class. php fills such a slot from its own read_property` |
|       - | 1285 | ``				 * (`Dom\Element::$children`), so the first read goes to the handler`` |
|       - | 1286 | `				 * and every read after it is answered by the filled slot. */` |
|  154245 | 1287 | `				if( pObjAttr != 0` |
|  135246 | 1288 | `				 && (pObjAttr->iState & VM_CLASS_ATTR_UNINIT) != 0` |
|   58551 | 1289 | `				 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZYSLOT) != 0` |
|     449 | 1290 | `				 && pInstr->iP2 != PH7_MEMBER_UNSET ){` |
|      44 | 1291 | `					pEntry = 0;` |
|      44 | 1292 | `					pObjAttr = 0;` |
|      20 | 1293 | `				}` |
|  154245 | 1294 | `				if( pObjAttr == 0 && PH7_ClassHasNativeProp(pClass)` |
|   37676 | 1295 | `				 && !VmMemberCoalOwned(pThis,pInstr,&sName) ){` |
|       - | 1296 | `					/* php's read_property / has_property / write_property /` |
|       - | 1297 | `					 * unset_property handlers, for a class whose properties are not` |
|       - | 1298 | `					 * storage at all: PDORow answers every read from the statement's` |
|       - | 1299 | `					 * current ROW, holds no slot for any of them, and refuses every` |
|       - | 1300 | `					 * write. It comes first among the miss paths, and before the` |
|       - | 1301 | `					 * declared-but-absent one: a name this class owns is neither a` |
|       - | 1302 | `					 * dynamic property to create, nor an "Undefined property" to warn` |
|       - | 1303 | `					 * about, nor a __get to dispatch.` |
|       - | 1304 | `					 *` |
|       - | 1305 | `					 * Which handler php asks is what the CONTEXT says: a plain store,` |
|       - | 1306 | `					 * a destructuring target and a read-modify-write all end in a` |
|       - | 1307 | ``					 * write; `??=` reads first and writes only when the read answered`` |
|       - | 1308 | ``					 * null; a subscript-write base (`$o->p[0] = 1`) is a READ whose`` |
|       - | 1309 | `					 * value the subscript then refuses; and isset() stops at the` |
|       - | 1310 | ``					 * truth while empty() and `??` take the value. */`` |
|   37306 | 1311 | `					VmInstr *pPropNext = pInstr + 1;` |
|   37306 | 1312 | `					int bPropStore = (pPropNext->iOp == PH7_OP_STORE && pPropNext->iP2 != 0);` |
|   56523 | 1313 | `					int bPropCoal = (pInstr->iP2 == PH7_MEMBER_WRITE` |
|   37301 | 1314 | `						&& pPropNext->iOp == PH7_OP_NULLC_JMP);` |
|       - | 1315 | `					PH7_NativePropCtx sProp;` |
|       - | 1316 | `					ph7_value sPropVal;` |
|   37306 | 1317 | `					PH7_MemObjInit(&(*pVm),&sPropVal);` |
|   37306 | 1318 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|      25 | 1319 | `						sProp.iMode = PH7_NATIVE_PROP_UNSET;` |
|   37293 | 1320 | `					}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET \|\| bPropStore` |
|   36730 | 1321 | `					       \|\| VmMemberNextIsRmw(pPropNext) ){` |
|    1119 | 1322 | `						sProp.iMode = PH7_NATIVE_PROP_WRITE;` |
|   36727 | 1323 | `					}else if( pInstr->iP2 == PH7_MEMBER_ISSET ){` |
|     273 | 1324 | `						sProp.iMode = PH7_NATIVE_PROP_ISSET;` |
|   36036 | 1325 | `					}else if( pInstr->iP2 == PH7_MEMBER_EMPTY ){` |
|       - | 1326 | `						/* php's empty() asks has_property with a non-zero` |
|       - | 1327 | `						 * check_empty and takes THAT for the answer -- it never` |
|       - | 1328 | `						 * reads the value. The two are not the same question:` |
|       - | 1329 | ``						 * `empty($row->queryString)` is TRUE on a name the`` |
|       - | 1330 | `						 * handler does not know, while reading it works. */` |
|      29 | 1331 | `						sProp.iMode = PH7_NATIVE_PROP_NOTEMPTY;` |
|      15 | 1332 | `					}else{` |
|   35874 | 1333 | `						sProp.iMode = PH7_NATIVE_PROP_READ;` |
|       - | 1334 | `					}` |
|   37306 | 1335 | `					sProp.pName = &sName;` |
|   37306 | 1336 | `					sProp.pResult = &sPropVal;` |
|   37306 | 1337 | `					sProp.bAnswered = 0;` |
|   37306 | 1338 | `					sProp.zThrowClass = 0;` |
|   37306 | 1339 | `					sProp.zThrowMsg[0] = 0;` |
|   37306 | 1340 | `					sProp.iThrowCode = 0;` |
|       - | 1341 | ``					/* php's `??` is its THIRD accessor level: it takes the property's`` |
|       - | 1342 | `					 * VALUE and says nothing about a name that is not there. */` |
|   37306 | 1343 | `					sProp.bQuiet = (pInstr->iP2 == PH7_MEMBER_COALESCE);` |
|       - | 1344 | `					/* ...and a fetch in WRITE context is php's get_property_ptr_ptr,` |
|       - | 1345 | `					 * which a handler whose property is a real element answers with` |
|       - | 1346 | ``					 * the element itself -- so `$ao->list[] = 1` lands in the store`` |
|       - | 1347 | `					 * rather than in a temporary, and creates the key it is missing.` |
|       - | 1348 | ``					 * A reference SOURCE asks the same handler: `$r =& $ao->z` creates`` |
|       - | 1349 | `					 * the key and binds to it, where a VIRTUAL property (no slot to` |
|       - | 1350 | `					 * give) ignores the flag and hands back its value copy. */` |
|   55389 | 1351 | `					sProp.bWriteCtx = VmMemberNativeSetKeepsSlot(pInstr)` |
|   37301 | 1352 | `						\|\| VmMemberFetchIsRefSource(pInstr);` |
|   37306 | 1353 | `					sProp.nSlot = SXU32_HIGH;` |
|   37306 | 1354 | `					if( PH7_ClassNativeProp(pThis,&sProp) ){` |
|   36067 | 1355 | `						if( sProp.zThrowClass == 0 && bPropCoal` |
|   18007 | 1356 | `						 && sProp.iMode == PH7_NATIVE_PROP_READ` |
|       9 | 1357 | `						 && (sPropVal.iFlags & MEMOBJ_NULL) ){` |
|       - | 1358 | ``							/* `$o->p ??= v` on a name that reads null: the store php`` |
|       - | 1359 | `							 * skips for a non-null one is the one it now makes, so the` |
|       - | 1360 | `							 * refusal is the WRITE handler's. */` |
|       3 | 1361 | `							sProp.iMode = PH7_NATIVE_PROP_WRITE;` |
|       3 | 1362 | `							sProp.bAnswered = 0;` |
|       3 | 1363 | `							PH7_ClassNativeProp(pThis,&sProp);` |
|       1 | 1364 | `						}` |
|   36072 | 1365 | `						if( sProp.zThrowClass ){` |
|       - | 1366 | `							/* Parked, like every other refusal this op makes: the` |
|       - | 1367 | `							 * fetch-point router lands it and abandons this slot. */` |
|      91 | 1368 | `							VmBoundaryPark(&(*pVm),` |
|      60 | 1369 | `								VmThrowFixedErrorCode(&(*pVm),sProp.zThrowClass,` |
|      30 | 1370 | `									sProp.iThrowCode,sProp.zThrowMsg));` |
|      61 | 1371 | `							PH7_MemObjRelease(&sPropVal);` |
|      61 | 1372 | `							VmPopOperand(&pTos,1);      /* the property name */` |
|      61 | 1373 | `							PH7_MemObjRelease(pTos);    /* the object slot is the answer */` |
|      61 | 1374 | `							pTos->nIdx = SXU32_HIGH;` |
|   18064 | 1375 | `							VM_EXIT_BREAK;` |
|       - | 1376 | `						}` |
|   36012 | 1377 | `						VmPopOperand(&pTos,1);          /* the property name */` |
|   36012 | 1378 | `						pThis->iRef++;` |
|   36012 | 1379 | `						PH7_MemObjRelease(pTos);` |
|   36012 | 1380 | `						if( sProp.iMode == PH7_NATIVE_PROP_ISSET ){` |
|       - | 1381 | `							/* isset() only tests null-ness: a non-null marker for` |
|       - | 1382 | `							 * true, NULL for false — what the __isset arm pushes. */` |
|     213 | 1383 | `							if( ph7_value_to_bool(&sPropVal) ){` |
|     137 | 1384 | `								pTos->x.iVal = 1;` |
|     137 | 1385 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      71 | 1386 | `							}` |
|   35908 | 1387 | `						}else if( sProp.iMode != PH7_NATIVE_PROP_UNSET ){` |
|       - | 1388 | `							/* EMPTY pushes the handler's BOOL, which the op then` |
|       - | 1389 | `							 * judges for emptiness -- the same answer php takes. */` |
|   35794 | 1390 | `							PH7_MemObjStore(&sPropVal,pTos);` |
|   17895 | 1391 | `						}` |
|       - | 1392 | `						/* SXU32_HIGH unless the handler handed back a real element's` |
|       - | 1393 | `						 * slot: a virtual property is a VALUE and never an lvalue,` |
|       - | 1394 | `						 * while ArrayObject's storage element is the thing itself. */` |
|   36012 | 1395 | `						pTos->nIdx = sProp.nSlot;` |
|   36012 | 1396 | `						PH7_MemObjRelease(&sPropVal);` |
|   36012 | 1397 | `						PH7_ClassInstanceUnref(pThis);` |
|   36012 | 1398 | `						VM_EXIT_BREAK;` |
|       - | 1399 | `					}` |
|    1239 | 1400 | `					PH7_MemObjRelease(&sPropVal);` |
|     617 | 1401 | `				}` |
|  118183 | 1402 | `				if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - | 1403 | `					/* unset($o->prop): remove the property entirely so it disappears from` |
|       - | 1404 | `					 * foreach / json_encode / get_object_vars / (array) — matching PHP (a value-only` |
|       - | 1405 | `					 * release would leave a zombie null entry). Leave a NULL constant on the stack so` |
|       - | 1406 | `					 * the trailing generic unset() builtin is a no-op (mirrors LOAD_IDX iP2=5).` |
|       - | 1407 | `					 * php dispatches __unset($name) for a MISSING or INACCESSIBLE property` |
|       - | 1408 | `					 * (band A #3b — pre-fix an inaccessible private was silently DELETED from` |
|       - | 1409 | `					 * outside the class); without __unset, an inaccessible unset is php's` |
|       - | 1410 | `					 * catchable "Cannot access ..." Error and a missing one stays a no-op. */` |
|     174 | 1411 | `					int bUnsAccessible = pEntry ? PH7_VmClassAttrAccess(&(*pVm),pClass,pObjAttr->pAttr,FALSE) : 0;` |
|     247 | 1412 | `					ph7_class_attr *pUnsNoWrite = pEntry ? pObjAttr->pAttr` |
|      97 | 1413 | `						: PH7_ClassScopedAttribute(&(*pVm),pClass,sName.zString,sName.nByte);` |
|     174 | 1414 | `					sxi32 rcUnsRo = SXRET_OK;` |
|     174 | 1415 | `					if( pEntry && (pObjAttr->iState & VM_CLASS_ATTR_RDONLY) != 0 ){` |
|       - | 1416 | `						/* php's read-only handler answers an unset with the same sentence` |
|       - | 1417 | ``						 * it answers a store: `Property p is read only`. */`` |
|       3 | 1418 | `						VmBoundaryPark(&(*pVm),VmThrowNativeReadOnly(&(*pVm),pObjAttr->pAttr));` |
|     173 | 1419 | `					}else if( pUnsNoWrite && (pUnsNoWrite->iFlags & (PH7_CLASS_ATTR_NATIVE_NOWRITE` |
|      78 | 1420 | `					                                                \|PH7_CLASS_ATTR_NATIVE_NOSLOT)) != 0 ){` |
|       - | 1421 | `						/* php's own unset handler for this class refuses, and words it` |
|       - | 1422 | ``						 * without either "readonly" or "property": `Cannot unset C::$p`.`` |
|       - | 1423 | `						 * It runs whether the object has a struct or not, so an` |
|       - | 1424 | `						 * unconstructed one -- which holds no slot at all -- refuses too` |
|       - | 1425 | `						 * rather than falling through to the missing-property no-op.` |
|       - | 1426 | `						 * A VIRTUAL property (NOSLOT) is the same answer for the same` |
|       - | 1427 | `						 * reason: php's unset_property handler for one has nothing to` |
|       - | 1428 | ``						 * remove, so `unset($doc->preserveWhiteSpace)` is this Error and`` |
|       - | 1429 | `						 * not the silent no-op a name the object lacks would take. */` |
|      48 | 1430 | `						VmBoundaryPark(&(*pVm),` |
|      15 | 1431 | `							VmThrowNativeNoUnset(&(*pVm),pThis->pClass,pUnsNoWrite));` |
|     157 | 1432 | `					}else if( pEntry && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) != 0 ){` |
|       - | 1433 | `						/* A native class's property is php's own C struct field, and` |
|       - | 1434 | `						 * unset() is a std handler that looks for a REAL property and` |
|       - | 1435 | `						 * finds none: nothing happens, nothing is said, and the next` |
|       - | 1436 | `						 * read still answers the struct. Removing the slot here left` |
|       - | 1437 | ``						 * `unset($i->y); $i->y` an Undefined property warning and NULL`` |
|       - | 1438 | `						 * for a statement php ignores. */` |
|     142 | 1439 | `					}else if( pEntry && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) != 0 ){` |
|       - | 1440 | `						/* php 8.4: a hooked property (virtual or backed) can never be` |
|       - | 1441 | `						 * unset — catchable Error, even from inside its own hook body` |
|       - | 1442 | `						 * (probe-verified). */` |
|       - | 1443 | `						SyBlob sErrMsg;` |
|       5 | 1444 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       5 | 1445 | `						SyBlobFormat(&sErrMsg,"Cannot unset hooked property %z::$%z",` |
|       4 | 1446 | `							&pThis->pClass->sDisp,&pObjAttr->pAttr->sName);` |
|       5 | 1447 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     133 | 1448 | `					}else if( pEntry && bUnsAccessible` |
|     114 | 1449 | `					       && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0` |
|      72 | 1450 | `					       && (rcUnsRo = VmCheckReadonlyUnset(&(*pVm),pThis->pClass,pObjAttr)) != SXRET_OK ){` |
|       - | 1451 | `						/* php refuses to destroy a readonly property: an initialized` |
|       - | 1452 | `						 * one from every scope, and an uninitialized one from a scope` |
|       - | 1453 | `						 * that may not write it. Deleting it here re-armed the` |
|       - | 1454 | ``						 * write-once latch, so a `readonly` value could be replaced by`` |
|       - | 1455 | `						 * anything in two statements. The refusal is thrown inside the` |
|       - | 1456 | `						 * check; parking it is what routes it like every other one` |
|       - | 1457 | `						 * raised from this opcode. */` |
|      17 | 1458 | `						VmBoundaryPark(&(*pVm),rcUnsRo);` |
|     126 | 1459 | `					}else if( pEntry && bUnsAccessible ){` |
|      96 | 1460 | `						if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|      62 | 1461 | `						 && (pObjAttr->iState & VM_CLASS_ATTR_REFBOUND) == 0 ){` |
|       - | 1462 | `							/* A TYPED property keeps its DECLARATION: php's unset makes` |
|       - | 1463 | `							 * it uninitialized, so var_dump still names it` |
|       - | 1464 | ``							 * `uninitialized(T)`, a read is "must not be accessed before`` |
|       - | 1465 | `							 * initialization" rather than an Undefined property, and a` |
|       - | 1466 | `							 * later write lands back in its declared position instead of` |
|       - | 1467 | `							 * appending a dynamic one at the end. The seven other` |
|       - | 1468 | `							 * presentation surfaces leave an uninitialized property out,` |
|       - | 1469 | `							 * which is what made the delete look right. */` |
|      24 | 1470 | `							ph7_value *pUnsSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|      24 | 1471 | `							if( pUnsSlot ){` |
|      24 | 1472 | `								PH7_MemObjRelease(pUnsSlot);` |
|      24 | 1473 | `								MemObjSetType(pUnsSlot,MEMOBJ_NULL);` |
|      11 | 1474 | `							}` |
|      24 | 1475 | `							pObjAttr->iState \|= VM_CLASS_ATTR_UNINIT;` |
|      13 | 1476 | `						}else{` |
|      77 | 1477 | `							PH7_VmReleaseInstanceAttr(&(*pVm),pObjAttr);` |
|       - | 1478 | `` 							/* Through the instance's own door: a `foreach`/`array_walk` `` |
|       - | 1479 | `							 * standing one property short of this one is holding the` |
|       - | 1480 | `							 * entry about to be freed. */` |
|      77 | 1481 | `							PH7_ClassInstanceDeleteAttrEntry(pThis,pEntry);` |
|       - | 1482 | `						}` |
|      51 | 1483 | `					}else{` |
|      21 | 1484 | `						ph7_class_method *pUnsetMagic = PH7_ClassExtractMethod(pClass,"__unset",sizeof("__unset")-1);` |
|      21 | 1485 | `						if( pUnsetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'u') ){` |
|      13 | 1486 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'u');` |
|      13 | 1487 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__unset",sizeof("__unset")-1,&sName,0);` |
|      13 | 1488 | `							VmMagicGuardPop(pVm);` |
|      14 | 1489 | `						}else if( pEntry ){` |
|       - | 1490 | `							/* Inaccessible and no __unset: php's catchable Error. Parked on` |
|       - | 1491 | `							 * the boundary rail; the op completes benignly and the` |
|       - | 1492 | `							 * fetch-point router lands it. */` |
|       - | 1493 | `							SyBlob sErrMsg;` |
|       3 | 1494 | `							const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|       3 | 1495 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 1496 | `							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",zVis,&pClass->sDisp,&sName);` |
|       3 | 1497 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       1 | 1498 | `						}` |
|       - | 1499 | `						/* Missing property without __unset: silent no-op (php). */` |
|       - | 1500 | `					}` |
|     174 | 1501 | `					VmPopOperand(&pTos,1);    /* pop the attribute name */` |
|     174 | 1502 | `					PH7_MemObjRelease(pTos);  /* release the object on the stack ($o's stack ref) */` |
|     174 | 1503 | `					pTos->nIdx = SXU32_HIGH;  /* NULL constant */` |
|     174 | 1504 | `					VM_EXIT_BREAK;` |
|       - | 1505 | `				}` |
|  118013 | 1506 | `				if( pObjAttr == 0 ){` |
|       - | 1507 | `					/* Member not present on the instance and the next instruction writes/modifies it` |
|       - | 1508 | ``					 * (store, array-append/keyed-write, `??=`, ++/--, or a compound-assign — see`` |
|       - | 1509 | `					 * VmMemberNextIsWrite; the compiler always emits a terminating PH7_OP_DONE so` |
|       - | 1510 | `					 * pInstr+1 is in-bounds). Create the property so the operation lands on a real slot` |
|       - | 1511 | ``					 * (PHP auto-vivifies a fresh property for all these forms, not just `=`):`` |
|       - | 1512 | `					 *   - a DECLARED prop that was unset() and is re-assigned → recreate it` |
|       - | 1513 | `					 *     (PHP re-appends it at the end), OR` |
|       - | 1514 | `					 *   - a dynamic prop on a dynamic-allowing class (stdClass).` |
|       - | 1515 | `					 * The created slot is NULL with a real nIdx, which the modify-op then vivifies` |
|       - | 1516 | `					 * (NULL→array for [], NULL→0/"" for ++/.=) and writes back in place.` |
|       - | 1517 | `					 * Two signals: the compiler tags the member itself iP2=PH7_MEMBER_WRITE when it is` |
|       - | 1518 | ``					 * the base of a write-subscript / `??=` (the modify-op is not the immediately-next`` |
|       - | 1519 | `					 * instruction there), and for ++/--/compound-assign/store the next opcode is the` |
|       - | 1520 | `					 * modify-op directly (VmMemberNextIsWrite). */` |
|    1943 | 1521 | `					VmInstr *pNext = pInstr + 1;` |
|       - | 1522 | ``					/* A reference SOURCE is php's write-context fetch too (`$r =& $o->p`,`` |
|       - | 1523 | ``					 * `[&$o->p]`, `foreach ($o->p as &$v)`): the name it does not find is`` |
|       - | 1524 | `					 * CREATED, silently, and the bind reaches the created slot. PHL read` |
|       - | 1525 | `					 * it, so every such source warned about what php was on the point of` |
|       - | 1526 | `					 * creating and then bound a fresh variable of its own -- a write` |
|       - | 1527 | `					 * through the reference never reached the object. */` |
|    1943 | 1528 | `					int bRefSrcMiss = VmMemberFetchIsRefSource(pInstr);` |
|    1938 | 1529 | `					if( pInstr->iP2 == PH7_MEMBER_WRITE \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|     558 | 1530 | `					 \|\| VmMemberNextIsWrite(pNext) \|\| bRefSrcMiss ){` |
|    1441 | 1531 | `						ph7_class_attr *pDecl = PH7_ClassScopedAttribute(&(*pVm),pThis->pClass,sName.zString,sName.nByte);` |
|    1441 | 1532 | `						if( pDecl && PH7_ATTR_LAZY_ABSENT(pDecl,pThis) ){` |
|       - | 1533 | `							/* The object has never held this name. A class whose handler` |
|       - | 1534 | `							 * REFUSES every write answers the same sentence with or` |
|       - | 1535 | `							 * without a struct, and nothing is created; otherwise php's` |
|       - | 1536 | `							 * own write goes to the standard handler and CREATES a` |
|       - | 1537 | `							 * dynamic property beside the struct, which PHL refuses` |
|       - | 1538 | `							 * (the scope policy) -- so fall through to the dynamic branch and let it.` |
|       - | 1539 | `` 							 * Once the constructor has installed the set, an `unset()` `` |
|       - | 1540 | `							 * and a re-write are the ordinary declared path again. */` |
|      16 | 1541 | `							if( pDecl->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE ){` |
|       4 | 1542 | `								VmBoundaryPark(&(*pVm),` |
|       1 | 1543 | `									VmThrowNativeNoWrite(&(*pVm),pThis->pClass,pDecl));` |
|       3 | 1544 | `								VmPopOperand(&pTos,1);    /* pop the attribute name */` |
|       3 | 1545 | `								PH7_MemObjRelease(pTos);  /* the object slot becomes the answer */` |
|       3 | 1546 | `								pTos->nIdx = SXU32_HIGH;` |
|       3 | 1547 | `								VM_EXIT_BREAK;` |
|       - | 1548 | `							}` |
|      13 | 1549 | `							pDecl = 0;` |
|       6 | 1550 | `						}` |
|    1439 | 1551 | `						if( pDecl && (pDecl->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){` |
|       - | 1552 | `							/* A VIRTUAL property: php keeps no slot to re-create, and its` |
|       - | 1553 | `							 * write goes to the class's own write_property handler, which` |
|       - | 1554 | `							 * the rails below reach through PH7_ClassNativePropOwns.` |
|       - | 1555 | `							 * Creating one would give the object a real property php has` |
|       - | 1556 | `							 * none of, and would take the read with it` |
|       - | 1557 | ``							 * (`$doc->formatOutput = false` then answered out of the slot`` |
|       - | 1558 | `							 * rather than out of the extension's state). */` |
|    1051 | 1559 | `							pDecl = 0;` |
|     523 | 1560 | `						}` |
|    1439 | 1561 | `						if( pDecl && (pDecl->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT)) == 0 ){` |
|      18 | 1562 | `							VmRecreateDeclaredAttr(&(*pVm),pThis,pDecl,&pObjAttr);` |
|      10 | 1563 | `						}else{` |
|       - | 1564 | `							/* php 8 semantics (band A #3b): a PLAIN store to a missing` |
|       - | 1565 | `							 * property dispatches __set($name,$value) when declared —` |
|       - | 1566 | `							 * the value only exists at the following OP_STORE, so park` |
|       - | 1567 | `							 * the receiver+name for it (one-instruction lifetime; the` |
|       - | 1568 | `							 * guard makes a same-name write inside __set fall through` |
|       - | 1569 | `							 * to dynamic creation, like php). Without __set — or for a` |
|       - | 1570 | `							 * subscript-write base / read-modify-write, which php does` |
|       - | 1571 | `							 * NOT route through __set — create a dynamic property on` |
|       - | 1572 | `							 * ANY class with the 8.2 deprecation (suppressed for` |
|       - | 1573 | `							 * stdClass and #[AllowDynamicProperties]); a readonly` |
|       - | 1574 | `							 * class raises php's catchable Error instead. */` |
|    1423 | 1575 | `							ph7_class_method *pSetMagic = 0;` |
|    1423 | 1576 | `							ph7_class_method *pCoalIsset = 0, *pCoalGet = 0, *pCoalSet = 0;` |
|    1423 | 1577 | `							int bPlainStore = (pNext->iOp == PH7_OP_STORE && pNext->iP2 != 0);` |
|       - | 1578 | `							/* A name the class's OWN property-handler table carries is` |
|       - | 1579 | ``							 * overloaded exactly the way a `__set` class's is -- php's`` |
|       - | 1580 | ``							 * write_property stands where `__set` would -- so every rail`` |
|       - | 1581 | `							 * below takes it, and the one door they all end at` |
|       - | 1582 | `							 * (VmMagicSetDispatch) asks the handler first. */` |
|    1423 | 1583 | `							int bOwnedSet = PH7_ClassNativePropOwns(pThis,&sName);` |
|    1423 | 1584 | `							if( bPlainStore \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|    1305 | 1585 | `								pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|     650 | 1586 | `							}` |
|    1423 | 1587 | `							if( (pSetMagic \|\| bOwnedSet) && pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 1588 | `								/* php dispatches __set($name, element) for a missing` |
|       - | 1589 | `								 * destructuring target, but the element value only exists` |
|       - | 1590 | `								 * at the following OP_LOAD_LIST — the dispatch is` |
|       - | 1591 | `								 * unsupported (recorded residual). Leave the miss: the` |
|       - | 1592 | `								 * property stays uncreated, matching php's observable` |
|       - | 1593 | `								 * state (its __set did not store either). */` |
|    1410 | 1594 | `							}else if( (bOwnedSet && bPlainStore)` |
|     867 | 1595 | `							 \|\| (pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s')) ){` |
|    1125 | 1596 | `								pThis->iRef++;` |
|    1125 | 1597 | `								pVm->pMagicSetThis = pThis;` |
|    1125 | 1598 | `								SyBlobReset(&pVm->sMagicSetName);` |
|    1125 | 1599 | `								SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       - | 1600 | `								/* pObjAttr stays NULL; the miss path below stays silent. */` |
|     873 | 1601 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|     105 | 1602 | `							 && pNext->iOp == PH7_OP_NULLC_JMP` |
|      53 | 1603 | `							 && (bOwnedSet` |
|      11 | 1604 | `							  \|\| (pCoalIsset = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1)) != 0` |
|       8 | 1605 | `							  \|\| (pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)) != 0` |
|       5 | 1606 | `							  \|\| (pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1)) != 0) ){` |
|       - | 1607 | ``								/* `$o->p ??= v` on a missing property with magic accessors:`` |
|       - | 1608 | `								 * php consults __isset first when declared — false means` |
|       - | 1609 | `								 * assign directly through __set with NO __get call (the` |
|       - | 1610 | `								 * test value stays null); true (or no __isset) means __get` |
|       - | 1611 | `								 * provides the test value. A COAL_MAGIC entry is pushed` |
|       - | 1612 | `								 * for the OP_NULLC_STORE at the jump target; the` |
|       - | 1613 | `								 * fetch-point sweep drops it when the short-circuit jump` |
|       - | 1614 | `								 * skips the assign or a throw abandons the RHS. Without` |
|       - | 1615 | `								 * __set the assign side keeps the pre-existing loud` |
|       - | 1616 | `								 * "Cannot perform assignment" path (php would create a` |
|       - | 1617 | `								 * dynamic property there — recorded residual). Note the` |
|       - | 1618 | `								 * condition's short-circuit assignment chain: a hit on an` |
|       - | 1619 | `								 * earlier method leaves the later pointers unresolved, so` |
|       - | 1620 | `								 * re-resolve the leftovers here (each is one hash probe,` |
|       - | 1621 | `								 * only on this ??=-miss path). */` |
|       - | 1622 | `								ph7_value sTest;` |
|       9 | 1623 | `								int bMiss = 0; /* __isset said false: skip __get, test value stays null */` |
|       9 | 1624 | `								if( pCoalIsset != 0 ){` |
|       5 | 1625 | `									pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       2 | 1626 | `								}` |
|       9 | 1627 | `								if( pCoalIsset != 0 \|\| pCoalGet != 0 ){` |
|       7 | 1628 | `									pCoalSet = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|       3 | 1629 | `								}` |
|       9 | 1630 | `								PH7_MemObjInit(pVm,&sTest);` |
|       9 | 1631 | `								if( pCoalIsset && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 1632 | `									ph7_value sIssetRet;` |
|       5 | 1633 | `									PH7_MemObjInit(pVm,&sIssetRet);` |
|       5 | 1634 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|       5 | 1635 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|       5 | 1636 | `									VmMagicGuardPop(pVm);` |
|       5 | 1637 | `									PH7_MemObjToBool(&sIssetRet);` |
|       5 | 1638 | `									bMiss = sIssetRet.x.iVal == 0;` |
|       5 | 1639 | `									PH7_MemObjRelease(&sIssetRet);` |
|       2 | 1640 | `								}` |
|       9 | 1641 | `								if( bOwnedSet && !bMiss ){` |
|       - | 1642 | `									/* php's ASSIGN_COALESCE fetches BP_VAR_IS, which` |
|       - | 1643 | `									 * asks has_property before read_property -- so a` |
|       - | 1644 | `									 * name the handler answers nothing for is never` |
|       - | 1645 | `									 * READ, and the assign it makes says nothing about` |
|       - | 1646 | `									 * a key that was not there. */` |
|       - | 1647 | `									ph7_value sOwnIs;` |
|       3 | 1648 | `									PH7_MemObjInit(pVm,&sOwnIs);` |
|       3 | 1649 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,` |
|       - | 1650 | `										"__isset",sizeof("__isset")-1,&sName,&sOwnIs);` |
|       3 | 1651 | `									PH7_MemObjToBool(&sOwnIs);` |
|       3 | 1652 | `									bMiss = sOwnIs.x.iVal == 0;` |
|       3 | 1653 | `									PH7_MemObjRelease(&sOwnIs);` |
|       1 | 1654 | `								}` |
|       8 | 1655 | `								if( !bMiss && (pCoalGet \|\| bOwnedSet)` |
|       5 | 1656 | `								 && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       5 | 1657 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       5 | 1658 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sTest);` |
|       5 | 1659 | `									VmMagicGuardPop(pVm);` |
|       2 | 1660 | `								}` |
|       8 | 1661 | `								if( (pCoalSet \|\| bOwnedSet)` |
|       8 | 1662 | `								 && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s')` |
|       9 | 1663 | `								 && pVm->nBoundaryRc == 0 ){` |
|       - | 1664 | `									VmHookRmw sPend;` |
|       9 | 1665 | `									sPend.iKind = VM_HOOK_PEND_COAL_MAGIC;` |
|       9 | 1666 | `									sPend.pThis = pThis;` |
|       9 | 1667 | `									sPend.pAttr = 0;` |
|       9 | 1668 | `									sPend.nBackIdx = SXU32_HIGH;` |
|       9 | 1669 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|       9 | 1670 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|       9 | 1671 | `									SyBlobAppend(&sPend.sName,(const void *)sName.zString,sName.nByte);` |
|       9 | 1672 | `									sPend.pOwnerStack = (void *)pStack;` |
|       9 | 1673 | `									sPend.pInstrs = (void *)aInstr;` |
|       9 | 1674 | `									sPend.nJmpPc = (sxu32)(pc + 1);        /* the OP_NULLC_JMP */` |
|       9 | 1675 | `									sPend.nPc = (sxu32)(pNext->iP2 - 1);   /* the OP_NULLC_STORE */` |
|       9 | 1676 | `									pThis->iRef++;` |
|       9 | 1677 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|       4 | 1678 | `								}` |
|       - | 1679 | `								/* Pop the attribute name; the test value becomes the` |
|       - | 1680 | `								 * expression slot (a temp, not an lvalue). */` |
|       9 | 1681 | `								VmPopOperand(&pTos,1);` |
|       9 | 1682 | `								pThis->iRef++;` |
|       9 | 1683 | `								PH7_MemObjRelease(pTos);` |
|       9 | 1684 | `								PH7_MemObjStore(&sTest,pTos);` |
|       9 | 1685 | `								pTos->nIdx = SXU32_HIGH;` |
|       9 | 1686 | `								PH7_MemObjRelease(&sTest);` |
|       9 | 1687 | `								PH7_ClassInstanceUnref(pThis);` |
|       9 | 1688 | `								VM_EXIT_BREAK;` |
|     300 | 1689 | `							}else if( VmMemberNextIsRmw(pNext)` |
|     176 | 1690 | `							 && VmMagicRmwEligible(pVm,pClass,pThis,&sName) ){` |
|       - | 1691 | `								/* ++/--/compound-assign on an overloaded property: php` |
|       - | 1692 | `								 * reads through __get and writes the computed value back` |
|       - | 1693 | `								 * through __set. Arm the scratch slot the modify op will` |
|       - | 1694 | `								 * mutate and finish the op here — the pre-existing path` |
|       - | 1695 | `								 * vivified a dynamic property instead, which on any` |
|       - | 1696 | `								 * ordinary accessor class is PHL's` |
|       - | 1697 | `								 * "Cannot create dynamic property" Error. */` |
|       - | 1698 | `								ph7_value sRmwVal;` |
|       - | 1699 | `								sxu32 nRmwScratch;` |
|      31 | 1700 | `								PH7_MemObjInit(pVm,&sRmwVal);` |
|      46 | 1701 | `								VmMagicRmwArm(&(*pVm),pThis,pClass,&sName,&sRmwVal,&nRmwScratch,` |
|      30 | 1702 | `									(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|      31 | 1703 | `								VmPopOperand(&pTos,1);   /* drop the property name */` |
|      31 | 1704 | `								pThis->iRef++;` |
|      31 | 1705 | `								PH7_MemObjRelease(pTos); /* collapse the object slot */` |
|      31 | 1706 | `								PH7_MemObjStore(&sRmwVal,pTos);` |
|      31 | 1707 | `								pTos->nIdx = nRmwScratch;` |
|      31 | 1708 | `								PH7_MemObjRelease(&sRmwVal);` |
|      31 | 1709 | `								PH7_ClassInstanceUnref(pThis);` |
|      31 | 1710 | `								VM_EXIT_BREAK;` |
|     270 | 1711 | `							}else if( bRefSrcMiss` |
|     161 | 1712 | `							 && (bOwnedSet` |
|      42 | 1713 | `							  \|\| PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)) ){` |
|       - | 1714 | `								/* An OVERLOADED name is php's exception to the rule above:` |
|       - | 1715 | `								 * get_property_ptr_ptr has nothing to hand out for one, so` |
|       - | 1716 | `								 * php falls back to read_property in write mode -- __get` |
|       - | 1717 | `								 * (or the class's own read handler) answers, and the` |
|       - | 1718 | ``								 * `Indirect modification of overloaded property` notice`` |
|       - | 1719 | `								 * comes with it. Leave the miss: the read gate below is` |
|       - | 1720 | `								 * where both of those live. */` |
|     268 | 1721 | `							}else if( !bPlainStore && pInstr->iP2 == PH7_MEMBER_WRITE` |
|      64 | 1722 | `							 && !VmMemberNextIsWrite(pNext)` |
|      41 | 1723 | `							 && PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1) ){` |
|       - | 1724 | `								/* Subscript-write base on a class with __get: php reads` |
|       - | 1725 | `								 * through the magic layer (the write lands on the temp and` |
|       - | 1726 | `								 * is lost, like php's indirect-modification case). A PLAIN` |
|       - | 1727 | `								 * store (bPlainStore — the compiler tags those` |
|       - | 1728 | `								 * PH7_MEMBER_WRITE too) is NOT this case: it falls through` |
|       - | 1729 | `								 * to dynamic creation. Leave the miss path — the read gate` |
|       - | 1730 | `								 * below dispatches __get. */` |
|     266 | 1731 | `							}else if( pThis->pClass->iFlags & PH7_CLASS_READONLY ){` |
|       - | 1732 | `								SyBlob sErrMsg;` |
|       3 | 1733 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 1734 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|       2 | 1735 | `									&pThis->pClass->sDisp,&sName);` |
|       3 | 1736 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|     262 | 1737 | `							}else if( !VmClassAllowsDynamicProps(pVm,pThis->pClass) ){` |
|       - | 1738 | `								/* php 8.2 only DEPRECATES creating a dynamic property on a` |
|       - | 1739 | `								 * class without #[AllowDynamicProperties]; PHL rejects it.` |
|       - | 1740 | `								 * stdClass / __set / declared props are unaffected. */` |
|       - | 1741 | `								SyBlob sErrMsg;` |
|      43 | 1742 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      43 | 1743 | `								SyBlobFormat(&sErrMsg,"Cannot create dynamic property %z::$%z",` |
|      38 | 1744 | `									&pThis->pClass->sDisp,&sName);` |
|      43 | 1745 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      24 | 1746 | `							}else{` |
|     223 | 1747 | `								PH7_VmCreateDynamicAttr(&(*pVm),pThis,sName.zString,sName.nByte,&pObjAttr);` |
|       - | 1748 | `							}` |
|       - | 1749 | `						}` |
|    1411 | 1750 | `						if( pObjAttr && VmMemberNextIsRmw(pNext) ){` |
|       - | 1751 | `							/* A read-modify-write READS the property before it writes it,` |
|       - | 1752 | `							 * so php warns that the name it just created was undefined and` |
|       - | 1753 | ``							 * then goes on with null — `$o->hits++` on a fresh object is`` |
|       - | 1754 | ``							 * `Undefined property: C::$hits` and then int(1). PHL created`` |
|       - | 1755 | `							 * the slot in silence. Only the read-modify-write forms:` |
|       - | 1756 | ``							 * a subscript-write base (`$o->arr[] = 1`) and a `??=` read`` |
|       - | 1757 | `							 * nothing, and php says nothing for either. */` |
|      19 | 1758 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|       6 | 1759 | `								&pClass->sDisp,&sName);` |
|       6 | 1760 | `						}` |
|     703 | 1761 | `					}` |
|     954 | 1762 | `				}` |
|       - | 1763 | `				/* An UNINITIALIZED typed property is deferred for the same reason a missing` |
|       - | 1764 | `				 * one is: which diagnostic php raises depends on the parameter. A by-VALUE` |
|       - | 1765 | ``				 * binding is the ordinary read Error (`Typed property ... must not be`` |
|       - | 1766 | ``				 * accessed before initialization`), a by-REFERENCE one is php's other`` |
|       - | 1767 | ``				 * sentence (`Cannot access uninitialized non-nullable property ... by`` |
|       - | 1768 | ``				 * reference`) -- or a NULL bind when the type admits null. A HOOKED`` |
|       - | 1769 | `				 * property is not one of these: its backing store is never what answers,` |
|       - | 1770 | `				 * and the hook rail below has php's own refusal for it -- and neither is an` |
|       - | 1771 | `` 				 * INACCESSIBLE one: php screens visibility first, so `f($o->privateSlot)` `` |
|       - | 1772 | ``				 * is `Cannot access private property` whether the slot was written or not. */`` |
|  117978 | 1773 | `				if( pObjAttr && (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|   58546 | 1774 | `				 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|     788 | 1775 | `				 && (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET` |
|     394 | 1776 | `				                                \|PH7_CLASS_ATTR_HOOK_VIRTUAL)) == 0` |
|     727 | 1777 | `				 && PH7_VmClassAttrAccess(&(*pVm),pClass,pObjAttr->pAttr,FALSE)` |
|     667 | 1778 | `				 && pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|      21 | 1779 | `					pObjAttr = 0;` |
|      21 | 1780 | `					bDeferUninit = 1;` |
|      10 | 1781 | `				}` |
|  117978 | 1782 | `				if( pObjAttr == 0 && pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH` |
|     166 | 1783 | `				 && (bDeferUninit` |
|     143 | 1784 | `				  \|\| !(PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1)` |
|      74 | 1785 | `				    && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g'))) ){` |
|       - | 1786 | `					/* D1 commit 2: deferred property arg, property absent on a REACHABLE object` |
|       - | 1787 | `					 * base ($o is a real slot). Record a property step rooted at $o so OP_CALL can` |
|       - | 1788 | `					 * vivify+bind (by-ref) or read via __get / warn (by-value). A magic __get/__set` |
|       - | 1789 | `					 * class is recorded the same way; the resolve step emits php's Notice for the` |
|       - | 1790 | `					 * by-ref case and dispatches __get for by-value. */` |
|       - | 1791 | `					SyString sProp;` |
|     113 | 1792 | `					VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|     113 | 1793 | `					SyStringInitFromBuf(&sProp,sName.zString,sName.nByte);` |
|     113 | 1794 | `					if( pPath && VmDeferPathPushProp(pPath,&sProp) == SXRET_OK ){` |
|     113 | 1795 | `						VmPopOperand(&pTos,1);       /* drop the property name */` |
|     113 | 1796 | `						pThis->iRef++;` |
|     113 | 1797 | `						PH7_MemObjRelease(pTos);     /* collapse the object slot into the carrier */` |
|     113 | 1798 | `						pTos->x.pOther = pPath;` |
|     113 | 1799 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|     113 | 1800 | `						pTos->nIdx = SXU32_HIGH;` |
|     113 | 1801 | `						PH7_ClassInstanceUnref(pThis);` |
|     113 | 1802 | `						VM_EXIT_BREAK;` |
|       - | 1803 | `					}` |
|     ! 0 | 1804 | `					if( pPath ){` |
|     ! 0 | 1805 | `						VmFreeDeferredPath(pPath);` |
|     ! 0 | 1806 | `					}` |
|       - | 1807 | `					/* fall through to the normal miss handling on allocation failure */` |
|     ! 0 | 1808 | `				}` |
|  117870 | 1809 | `				if( pObjAttr == 0` |
|   59733 | 1810 | `				 && (pInstr->iP2 == PH7_MEMBER_READ \|\| pInstr->iP2 == PH7_MEMBER_COALESCE) ){` |
|       - | 1811 | `					/* A LAZY native property whose class answers a read from its ZEROED` |
|       - | 1812 | `					 * struct (DatePeriod), asked before anything installed the set. php` |
|       - | 1813 | ``					 * consults that read handler in the plain read AND in `??` -- its third`` |
|       - | 1814 | `` 					 * accessor level takes the property's VALUE, so `$p->recurrences ?? 'd'` `` |
|       - | 1815 | `					 * is 0 there -- while isset()/empty() go to the has_property handler,` |
|       - | 1816 | `					 * which answers false for an object that has no struct at all. */` |
|     359 | 1817 | `					ph7_class_attr *pLz = PH7_ClassExtractAttribute(pClass,` |
|     118 | 1818 | `						SyStringData(&sName),SyStringLength(&sName));` |
|     236 | 1819 | `					if( pLz && (pLz->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT)` |
|      36 | 1820 | `					 && PH7_ATTR_LAZY_ABSENT(pLz,pThis) ){` |
|      25 | 1821 | `						pLazyDefault = pLz;` |
|      12 | 1822 | `					}` |
|     118 | 1823 | `				}` |
|  117875 | 1824 | `				if( pObjAttr == 0 ){` |
|       - | 1825 | `					/* Missing property. On a plain READ, php dispatches __get($name) and the` |
|       - | 1826 | `					 * expression takes its RETURN VALUE (band A #3a — pre-fix the result was` |
|       - | 1827 | `					 * discarded and a PHL-native warn fired even when __get existed). isset/` |
|       - | 1828 | `					 * empty context consults __isset first (then __get for empty()'s value` |
|       - | 1829 | `					 * test); a plain-store write context parked a pending __set above. A` |
|       - | 1830 | `					 * self-recursive read of the same property falls back to the` |
|       - | 1831 | `					 * undefined-property path via the guard, like php's property guard. */` |
|    1591 | 1832 | `					ph7_class_method *pGetMagic = 0;` |
|    1591 | 1833 | `					if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|     207 | 1834 | `						ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|     207 | 1835 | `						if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 1836 | `							ph7_value sIssetRet;` |
|       - | 1837 | `							int bSet;` |
|      61 | 1838 | `							PH7_MemObjInit(pVm,&sIssetRet);` |
|      61 | 1839 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|      61 | 1840 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|      61 | 1841 | `							VmMagicGuardPop(pVm);` |
|      61 | 1842 | `							PH7_MemObjToBool(&sIssetRet);` |
|      61 | 1843 | `							bSet = sIssetRet.x.iVal != 0;` |
|      61 | 1844 | `							PH7_MemObjRelease(&sIssetRet);` |
|      61 | 1845 | `							if( bSet && VmMemberCtxWantsValue(pInstr->iP2) ){` |
|       - | 1846 | ``								/* empty() and `??`: __isset said set, so php goes on to`` |
|       - | 1847 | `								 * __get for the VALUE — emptiness is judged on it, and the` |
|       - | 1848 | `								 * coalesce simply IS it (a null answer then takes the` |
|       - | 1849 | `								 * default, which OP_NULLC does for free). */` |
|      30 | 1850 | `								ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       - | 1851 | `								ph7_value sEmptyVal;` |
|      30 | 1852 | `								PH7_MemObjInit(pVm,&sEmptyVal);` |
|      30 | 1853 | `								if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|      30 | 1854 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|      30 | 1855 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|      30 | 1856 | `									VmMagicGuardPop(pVm);` |
|      14 | 1857 | `								}` |
|      30 | 1858 | `								VmPopOperand(&pTos,1);` |
|      30 | 1859 | `								pThis->iRef++;` |
|      30 | 1860 | `								PH7_MemObjRelease(pTos);` |
|      30 | 1861 | `								PH7_MemObjStore(&sEmptyVal,pTos);` |
|      30 | 1862 | `								pTos->nIdx = SXU32_HIGH;` |
|      30 | 1863 | `								PH7_MemObjRelease(&sEmptyVal);` |
|      30 | 1864 | `								PH7_ClassInstanceUnref(pThis);` |
|      30 | 1865 | `								VM_EXIT_BREAK;` |
|       - | 1866 | `							}` |
|       - | 1867 | `							/* isset(): the truth of __isset IS the answer — push a non-null` |
|       - | 1868 | `							 * marker for true, NULL for false (isset only tests null-ness). */` |
|      33 | 1869 | `							VmPopOperand(&pTos,1);` |
|      33 | 1870 | `							pThis->iRef++;` |
|      33 | 1871 | `							PH7_MemObjRelease(pTos);` |
|      33 | 1872 | `							if( bSet ){` |
|      17 | 1873 | `								pTos->x.iVal = 1;` |
|      17 | 1874 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       7 | 1875 | `							}` |
|      33 | 1876 | `							pTos->nIdx = SXU32_HIGH;` |
|      33 | 1877 | `							PH7_ClassInstanceUnref(pThis);` |
|      33 | 1878 | `							VM_EXIT_BREAK;` |
|       - | 1879 | `						}` |
|      73 | 1880 | `					}` |
|    1528 | 1881 | `					if( (!VmMemberCtxIsLookup(pInstr->iP2) \|\| pInstr->iP2 == PH7_MEMBER_COALESCE)` |
|    1471 | 1882 | `					 && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|    1476 | 1883 | `					 && !VmMemberNextIsWrite(pInstr + 1) ){` |
|       - | 1884 | `						/* Plain reads AND the ??=/subscript write-base (PH7_MEMBER_WRITE,` |
|       - | 1885 | `						 * which php reads through __get); read-modify-write forms are` |
|       - | 1886 | `						 * excluded (they vivified above — approximate, recorded), and so is` |
|       - | 1887 | `						 * a destructuring target (a pure write — php never reads it).` |
|       - | 1888 | ``						 * `??` reaches here only when the class declares NO __isset — the`` |
|       - | 1889 | `						 * branch above has returned otherwise — so php's gate is absent and` |
|       - | 1890 | `						 * __get answers on its own. */` |
|     267 | 1891 | `						pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|     131 | 1892 | `					}` |
|    1533 | 1893 | `					if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 1894 | `						ph7_value sMagicRet;` |
|     114 | 1895 | `						PH7_MemObjInit(pVm,&sMagicRet);` |
|     114 | 1896 | `						VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     114 | 1897 | `						PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|     114 | 1898 | `						VmMagicGuardPop(pVm);` |
|     114 | 1899 | `						if( VmMemberFetchForWrite(pInstr) ){` |
|       - | 1900 | `							/* php's indirect-modification notice, and the separation` |
|       - | 1901 | `							 * that makes the write it describes land nowhere. Raised` |
|       - | 1902 | `							 * BEFORE the pop below: sName still aliases the NAME` |
|       - | 1903 | ``							 * operand when the name is dynamic (`$o->$k[0] = v`). */`` |
|      13 | 1904 | `							PH7_VmOverloadedPropNotice(&(*pVm),pClass,&sName,&sMagicRet);` |
|     108 | 1905 | `						}else if( pInstr->iP2 == PH7_MEMBER_DEFPATH ){` |
|       - | 1906 | `							/* A deferred call ARGUMENT: __get has answered — php calls it` |
|       - | 1907 | `							 * where the property is WRITTEN, whatever the parameter turns` |
|       - | 1908 | `							 * out to be — and only the by-ref verdict is still pending.` |
|       - | 1909 | `							 * Carry the value with the class and name that produced it, so` |
|       - | 1910 | `							 * the notice lands at the call for a by-REFERENCE parameter and` |
|       - | 1911 | `							 * nothing is said for a by-value one, without __get running` |
|       - | 1912 | `							 * twice or a second read arriving after a later argument's side` |
|       - | 1913 | `							 * effects. */` |
|      54 | 1914 | `							VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),VM_OVER_PROP,` |
|      17 | 1915 | `								pClass,&sName,&sMagicRet);` |
|      37 | 1916 | `							if( pPre ){` |
|      37 | 1917 | `								VmPopOperand(&pTos,1);   /* drop the property name */` |
|      37 | 1918 | `								pThis->iRef++;` |
|      37 | 1919 | `								PH7_MemObjRelease(pTos); /* collapse the object slot into the carrier */` |
|      37 | 1920 | `								pTos->x.pOther = pPre;` |
|      37 | 1921 | `								pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      37 | 1922 | `								pTos->nIdx = SXU32_HIGH;` |
|      37 | 1923 | `								PH7_MemObjRelease(&sMagicRet);` |
|      37 | 1924 | `								PH7_ClassInstanceUnref(pThis);` |
|      37 | 1925 | `								VM_EXIT_BREAK;` |
|       - | 1926 | `							}` |
|     ! 0 | 1927 | `						}` |
|       - | 1928 | `						/* Pop the attribute name, replace the object slot with the magic` |
|       - | 1929 | `						 * result (a temp, not an lvalue — nIdx stays constant). A throw` |
|       - | 1930 | `						 * from __get parked at the boundary; the fetch-point router lands` |
|       - | 1931 | `						 * it and abandons this slot. */` |
|      80 | 1932 | `						VmPopOperand(&pTos,1);` |
|      80 | 1933 | `						pThis->iRef++;` |
|      80 | 1934 | `						PH7_MemObjRelease(pTos);` |
|      80 | 1935 | `						PH7_MemObjStore(&sMagicRet,pTos);` |
|      80 | 1936 | `						pTos->nIdx = SXU32_HIGH;` |
|      80 | 1937 | `						PH7_MemObjRelease(&sMagicRet);` |
|      80 | 1938 | `						PH7_ClassInstanceUnref(pThis);` |
|      80 | 1939 | `						VM_EXIT_BREAK;` |
|       - | 1940 | `					}` |
|       - | 1941 | `					/* No __get (or guard held): load null. In isset()/empty() context (iP2 3/4)` |
|       - | 1942 | `					 * PHP returns false/true SILENTLY, so suppress the read-miss warning there` |
|       - | 1943 | `					 * (mirrors the array LOAD_IDX iP2=4/6 suppression); likewise when a` |
|       - | 1944 | `					 * pending __set was parked above (the following OP_STORE dispatches it —` |
|       - | 1945 | `					 * nothing is "undefined" about that write) or a throw is parked on the` |
|       - | 1946 | `					 * boundary rail (e.g. the readonly-class dynamic-property Error — the` |
|       - | 1947 | `					 * fetch-point router lands it right after this op). */` |
|    1418 | 1948 | `					if( !VmMemberCtxIsLookup(pInstr->iP2) && pInstr->iP2 != PH7_MEMBER_LIST_TARGET` |
|    1278 | 1949 | `					 && pVm->pMagicSetThis == 0` |
|     723 | 1950 | `					 && pVm->nBoundaryRc == 0 ){` |
|       - | 1951 | `						/* A property missing from the INSTANCE may still be DECLARED by the` |
|       - | 1952 | ``						 * class — that is what `unset($o->p)` leaves behind, and php keeps`` |
|       - | 1953 | `						 * answering for the declaration rather than calling the name` |
|       - | 1954 | `						 * undefined: the visibility screen still applies, and a TYPED slot` |
|       - | 1955 | `						 * is back to uninitialized (the same Error a never-written one` |
|       - | 1956 | `						 * raises). Only a name the class does not declare at all is the` |
|       - | 1957 | `						 * "Undefined property" warning. A destructuring target is silent` |
|       - | 1958 | `						 * either way: php created the property above or dispatched __set. */` |
|     182 | 1959 | `						ph7_class_attr *pDeclAttr = PH7_ClassScopedAttribute(&(*pVm),pClass,` |
|      59 | 1960 | `							SyStringData(&sName),SyStringLength(&sName));` |
|       - | 1961 | `						/* A static property, a class constant and a native engine slot are` |
|       - | 1962 | `						 * not instance properties; a dynamic one is gone for good once it` |
|       - | 1963 | `						 * is unset, since nothing declares it. */` |
|     118 | 1964 | `						if( pDeclAttr` |
|      80 | 1965 | `						 && (pDeclAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|       - | 1966 | `						                          \|PH7_CLASS_ATTR_HIDDEN\|PH7_CLASS_ATTR_DYNAMIC)) ){` |
|       9 | 1967 | `							pDeclAttr = 0;` |
|       3 | 1968 | `						}` |
|     123 | 1969 | `						if( pDeclAttr && PH7_ATTR_LAZY_ABSENT(pDeclAttr,pThis) ){` |
|       - | 1970 | `							/* The object has never held this name. php has two answers and` |
|       - | 1971 | `							 * the class says which: a read handler over the ZEROED struct` |
|       - | 1972 | `							 * (DatePeriod -- null/0/false, in silence), or nothing at all` |
|       - | 1973 | `							 * (DateInterval), which is the ordinary "Undefined property"` |
|       - | 1974 | `							 * warning. Either way the DECLARATION is not what answers, so` |
|       - | 1975 | `							 * the typed-slot Error below must not fire on a name php keeps` |
|       - | 1976 | `							 * no slot for. */` |
|      25 | 1977 | `							if( pDeclAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT ){` |
|      21 | 1978 | `								pLazyDefault = pDeclAttr;` |
|      10 | 1979 | `							}` |
|      25 | 1980 | `							pDeclAttr = 0;` |
|      12 | 1981 | `						}` |
|     123 | 1982 | `						if( pDeclAttr && (pDeclAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){` |
|       - | 1983 | `							/* ...and a VIRTUAL property is answered by the class's handler` |
|       - | 1984 | `							 * too, so the DECLARATION must not raise the typed-slot Error` |
|       - | 1985 | `							 * for a name php keeps no slot for either. */` |
|     ! 0 | 1986 | `							pDeclAttr = 0;` |
|     ! 0 | 1987 | `						}` |
|     123 | 1988 | `						if( pLazyDefault ){` |
|       - | 1989 | `							/* php's read handler answered: say nothing. */` |
|     109 | 1990 | `						}else if( pDeclAttr` |
|      55 | 1991 | `						 && !PH7_VmClassAttrAccess(&(*pVm),pClass,pDeclAttr,FALSE) ){` |
|       - | 1992 | `							SyBlob sErrMsg;` |
|     ! 0 | 1993 | `							const char *zVis = pDeclAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|     ! 0 | 1994 | `								? "private" : "protected";` |
|     ! 0 | 1995 | `							SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|     ! 0 | 1996 | `							SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",` |
|     ! 0 | 1997 | `								zVis,&pClass->sDisp,&sName);` |
|     ! 0 | 1998 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",` |
|       - | 1999 | `								sizeof("Error")-1,&sErrMsg));` |
|     103 | 2000 | `						}else if( pDeclAttr && (pDeclAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|     ! 0 | 2001 | `							VmBoundaryPark(&(*pVm),` |
|     ! 0 | 2002 | `								VmThrowUninitializedPropertyError(&(*pVm),pClass,pDeclAttr));` |
|     ! 0 | 2003 | `						}else{` |
|     152 | 2004 | `							VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Undefined property: %z::$%z",` |
|      49 | 2005 | `								&pClass->sDisp,&sName);` |
|       - | 2006 | `						}` |
|      59 | 2007 | `					}` |
|     709 | 2008 | `				}` |
|  117702 | 2009 | `				if( pObjAttr && (pObjAttr->iState & VM_CLASS_ATTR_RDONLY)` |
|   58167 | 2010 | `				 && (pInstr->iP2 == PH7_MEMBER_LIST_TARGET` |
|      35 | 2011 | `				  \|\| ((pInstr + 1)->iOp == PH7_OP_STORE && (pInstr + 1)->iP2 != 0)) ){` |
|       - | 2012 | `					/* php's write_property handler refuses the plain store and the` |
|       - | 2013 | `					 * destructuring one that goes through it; everything that takes a` |
|       - | 2014 | ``					 * POINTER to the property instead -- a compound assign, `++`, `??=`,`` |
|       - | 2015 | `					 * a reference bind -- bypasses the handler in php and is left alone` |
|       - | 2016 | `					 * here too. */` |
|       7 | 2017 | `					VmBoundaryPark(&(*pVm),VmThrowNativeReadOnly(&(*pVm),pObjAttr->pAttr));` |
|       7 | 2018 | `					VmPopOperand(&pTos,1);` |
|       7 | 2019 | `					PH7_MemObjRelease(pTos);` |
|       7 | 2020 | `					pTos->nIdx = SXU32_HIGH;` |
|       7 | 2021 | `					VM_EXIT_BREAK;` |
|       - | 2022 | `				}` |
|  117701 | 2023 | `				VmPopOperand(&pTos,1);` |
|       - | 2024 | `				/* TICKET 1433-49: Deffer garbage collection until attribute loading.` |
|       - | 2025 | `				 * This is due to the following case:` |
|       - | 2026 | `				 *     (new TestClass())->foo;` |
|       - | 2027 | `				 */` |
|  117701 | 2028 | `				pThis->iRef++;` |
|  117701 | 2029 | `				PH7_MemObjRelease(pTos);` |
|  117701 | 2030 | `				pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|  117701 | 2031 | `				if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - | 2032 | ``					/* `$o->p =& $x`: stash the resolved instance property slot for the`` |
|       - | 2033 | `					 * following member-marked OP_STORE_REF, which rebinds it to alias the` |
|       - | 2034 | `					 * source variable. Do NOT run the read/hook/magic/uninit machinery` |
|       - | 2035 | `					 * below — a reference bind neither reads the value nor triggers` |
|       - | 2036 | `					 * get/set hooks or an uninitialized-typed Error. pThis stays retained` |
|       - | 2037 | `					 * (the iRef++ above); OP_STORE_REF releases it. */` |
|      62 | 2038 | `					if( pObjAttr && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOWRITE) ){` |
|       - | 2039 | `						/* A reference bind is a WRITE, and php's refusing handler answers` |
|       - | 2040 | `						 * it with the same sentence a plain store gets. */` |
|     ! 0 | 2041 | `						VmBoundaryPark(&(*pVm),` |
|     ! 0 | 2042 | `							VmThrowNativeNoWrite(&(*pVm),PH7_VmAttrOwner(pObjAttr),pObjAttr->pAttr));` |
|     ! 0 | 2043 | `						pVm->pRefTargetAttr = 0;` |
|     ! 0 | 2044 | `						pVm->pRefTargetThis = 0;` |
|     ! 0 | 2045 | `						pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 2046 | `						PH7_ClassInstanceUnref(pThis);` |
|      63 | 2047 | `					}else if( pObjAttr && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET) ){` |
|       - | 2048 | ``						/* `$i->f =& $x`: php REFUSES to make a handler-backed property`` |
|       - | 2049 | `						 * the target of a reference — there is no slot to rebind, and` |
|       - | 2050 | `						 * every write through the alias would skip the conversion the` |
|       - | 2051 | `						 * handler is there to do. PHL rebound the slot instead, so` |
|       - | 2052 | ``						 * `$x = 2.5` afterwards wrote 2.5 microseconds-free into the`` |
|       - | 2053 | `						 * interval. */` |
|       - | 2054 | `						SyBlob sErrMsg;` |
|       3 | 2055 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       3 | 2056 | `						SyBlobAppend(&sErrMsg,"Cannot assign by reference to overloaded object",` |
|       - | 2057 | `							sizeof("Cannot assign by reference to overloaded object")-1);` |
|       3 | 2058 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       3 | 2059 | `						pVm->pRefTargetAttr = 0;` |
|       3 | 2060 | `						pVm->pRefTargetThis = 0;` |
|       3 | 2061 | `						pVm->pRefTargetStaticAttr = 0;` |
|       3 | 2062 | `						PH7_ClassInstanceUnref(pThis);` |
|      61 | 2063 | `					}else if( pObjAttr ){` |
|      60 | 2064 | `						pVm->pRefTargetAttr = pObjAttr;` |
|      60 | 2065 | `						pVm->pRefTargetThis = pThis;` |
|      60 | 2066 | `						pVm->pRefTargetStaticAttr = 0;` |
|      60 | 2067 | `						pTos->nIdx = pObjAttr->nIdx;` |
|      31 | 2068 | `					}else{` |
|       - | 2069 | `						/* Missing/inaccessible target: leave nothing stashed; OP_STORE_REF` |
|       - | 2070 | `						 * no-ops. Balance the retain. */` |
|     ! 0 | 2071 | `						pVm->pRefTargetAttr = 0;` |
|     ! 0 | 2072 | `						pVm->pRefTargetThis = 0;` |
|     ! 0 | 2073 | `						pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 2074 | `						PH7_ClassInstanceUnref(pThis);` |
|       - | 2075 | `					}` |
|      62 | 2076 | `					VM_EXIT_BREAK;` |
|       - | 2077 | `				}` |
|  117641 | 2078 | `				if( pLazyDefault && pLazyDefault->pNativeValue ){` |
|       - | 2079 | `					/* A LAZY native property read before its class installed the set,` |
|       - | 2080 | `					 * on a class that answers such a read from its ZEROED struct. The` |
|       - | 2081 | `					 * declared literal IS that struct's field (the 76th session moved` |
|       - | 2082 | `					 * DatePeriod's seven defaults onto it), and the answer is a` |
|       - | 2083 | `					 * TEMPORARY: nothing was installed, so there is no slot to address` |
|       - | 2084 | `					 * and nIdx stays the constant sentinel the pop left. */` |
|      25 | 2085 | `					PH7_NativeLiteralValue(&(*pVm),pLazyDefault->pNativeValue,pTos);` |
|      12 | 2086 | `				}` |
|  117641 | 2087 | `				if( pObjAttr ){` |
|  116223 | 2088 | `					ph7_value *pValue = 0; /* cc warning */` |
|       - | 2089 | `					/* Check attribute access */` |
|  116223 | 2090 | `					if( PH7_VmClassAttrAccess(&(*pVm),pClass,pObjAttr->pAttr,FALSE) ){` |
|  116172 | 2091 | `						if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET))` |
|   58304 | 2092 | `						 && !VmHookGuardHeld(pVm,(void *)pThis,&sName) ){` |
|       - | 2093 | `							/* PHP 8.4 property hooks: route reads, writes, and the` |
|       - | 2094 | `							 * read-modify-write forms through the synthesized hook` |
|       - | 2095 | `							 * methods. A held guard (either kind) means we are INSIDE` |
|       - | 2096 | `							 * one of this property's own hook bodies — fall through to` |
|       - | 2097 | `` 							 * the raw backing slot for BOTH directions (php: `$this->x` `` |
|       - | 2098 | `							 * within any of x's hooks addresses the backing store). */` |
|     279 | 2099 | `							VmInstr *pNextH = pInstr + 1;` |
|     279 | 2100 | `							int bPlainStore = (pNextH->iOp == PH7_OP_STORE && pNextH->iP2 != 0);` |
|     470 | 2101 | `							int bCoalesceW = pInstr->iP2 == PH7_MEMBER_WRITE` |
|     274 | 2102 | `								&& pNextH->iOp == PH7_OP_NULLC_JMP;` |
|       - | 2103 | `							/* ++/--/compound-assign: the modify op FOLLOWS the member` |
|       - | 2104 | `							 * directly (the compiler tags compound-assign members` |
|       - | 2105 | `							 * PH7_MEMBER_WRITE too, so classify by the next op, not iP2;` |
|       - | 2106 | `							 * the !bPlainStore term is load-bearing — VmMemberNextIsWrite` |
|       - | 2107 | `							 * returns 1 for a member OP_STORE too) */` |
|     279 | 2108 | `							int bRmwNext = !bPlainStore && VmMemberNextIsWrite(pNextH);` |
|     333 | 2109 | `							int bSubscriptW = !bPlainStore && !bCoalesceW && !bRmwNext` |
|     379 | 2110 | `								&& pInstr->iP2 == PH7_MEMBER_WRITE;` |
|     279 | 2111 | `							if( bPlainStore ){` |
|       - | 2112 | `								/* Arm the pending hook-set for the following OP_STORE — it` |
|       - | 2113 | `								 * dispatches the set hook, or throws the read-only Error` |
|       - | 2114 | `								 * when the property only has a get hook. (The scalar` |
|       - | 2115 | `								 * transient is safe here: its window is exactly one` |
|       - | 2116 | `								 * instruction, MEMBER -> STORE, nothing runs in between.) */` |
|      67 | 2117 | `								pThis->iRef++;` |
|      67 | 2118 | `								pVm->pHookSetThis = pThis;` |
|      67 | 2119 | `								pVm->pHookSetAttr = pObjAttr->pAttr;` |
|      67 | 2120 | `								pVm->nHookSetIdx = pObjAttr->nIdx;` |
|      67 | 2121 | `								pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|      67 | 2122 | `								PH7_ClassInstanceUnref(pThis);` |
|      67 | 2123 | `								VM_EXIT_BREAK;` |
|       - | 2124 | `							}` |
|     215 | 2125 | `							if( bSubscriptW ){` |
|       - | 2126 | `								/* Subscript write base ($o->m[] = v, $o->m[$k] = v):` |
|       - | 2127 | `								 * php's catchable Error, with or without a set hook` |
|       - | 2128 | ``								 * (only a by-ref `&get` hook would allow it — a loud`` |
|       - | 2129 | `								 * compile error in PHL). The op completes benignly with` |
|       - | 2130 | `								 * a null temp; the fetch-point router lands the throw. */` |
|       - | 2131 | `								SyBlob sErrMsg;` |
|       7 | 2132 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       7 | 2133 | `								SyBlobFormat(&sErrMsg,"Indirect modification of %z::$%z is not allowed",` |
|       6 | 2134 | `									&pThis->pClass->sDisp,&pObjAttr->pAttr->sName);` |
|       7 | 2135 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       7 | 2136 | `								PH7_ClassInstanceUnref(pThis);` |
|       7 | 2137 | `								VM_EXIT_BREAK;` |
|       - | 2138 | `							}` |
|     204 | 2139 | `							if( (pObjAttr->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_VIRTUAL))` |
|     107 | 2140 | `							 == PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|       - | 2141 | `								/* VIRTUAL set-only property: there is no backing store to` |
|       - | 2142 | `								 * fall back to, so EVERY read context — plain read,` |
|       - | 2143 | `								 * isset()/empty() (php throws there too, probe-verified),` |
|       - | 2144 | `								 * the ??= test, an RMW read — is php's catchable` |
|       - | 2145 | `								 * write-only Error. */` |
|       - | 2146 | `								SyBlob sErrMsg;` |
|       9 | 2147 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|       9 | 2148 | `								SyBlobFormat(&sErrMsg,"Property %z::$%z is write-only",` |
|       8 | 2149 | `									&pThis->pClass->sDisp,&pObjAttr->pAttr->sName);` |
|       9 | 2150 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|       9 | 2151 | `								PH7_ClassInstanceUnref(pThis);` |
|       9 | 2152 | `								VM_EXIT_BREAK;` |
|       - | 2153 | `							}` |
|     201 | 2154 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2155 | ``								/* isset()/empty()/`??` on a hooked property all call the get`` |
|       - | 2156 | `								 * hook (php): isset is "get() !== null", empty tests the` |
|       - | 2157 | ``								 * value, and `??` IS the value. */`` |
|       - | 2158 | `								ph7_value sHookRet;` |
|      16 | 2159 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|      16 | 2160 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|      16 | 2161 | `									if( !VmMemberCtxWantsValue(pInstr->iP2) ){` |
|       - | 2162 | `										/* isset(): the trailing builtin tests NULL-ness of what` |
|       - | 2163 | `										 * it is handed, so "not set" has to BE null. Storing` |
|       - | 2164 | ``										 * bool(FALSE) here made `isset($o->p)` answer TRUE for`` |
|       - | 2165 | `										 * a get hook returning null — the same non-null marker` |
|       - | 2166 | `										 * convention the __isset path uses. */` |
|       8 | 2167 | `										if( (sHookRet.iFlags & MEMOBJ_NULL) == 0 ){` |
|       6 | 2168 | `											pTos->x.iVal = 1;` |
|       6 | 2169 | `											MemObjSetType(pTos,MEMOBJ_BOOL);` |
|       4 | 2170 | `										}else{` |
|       3 | 2171 | `											MemObjSetType(pTos,MEMOBJ_NULL);` |
|       - | 2172 | `										}` |
|       5 | 2173 | `									}else{` |
|       - | 2174 | ``										/* empty() judges the value; `??` IS the value. */`` |
|      10 | 2175 | `										PH7_MemObjStore(&sHookRet,pTos);` |
|       - | 2176 | `									}` |
|      16 | 2177 | `									pTos->nIdx = SXU32_HIGH;` |
|      16 | 2178 | `									PH7_MemObjRelease(&sHookRet);` |
|      16 | 2179 | `									PH7_ClassInstanceUnref(pThis);` |
|      16 | 2180 | `									VM_EXIT_BREAK;` |
|       - | 2181 | `								}` |
|     ! 0 | 2182 | `								PH7_MemObjRelease(&sHookRet);` |
|     ! 0 | 2183 | `							}` |
|     187 | 2184 | `							if( !bCoalesceW && !bRmwNext && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2185 | `								/* Read: dispatch __phl_hook_get_NAME (inheritance-aware) */` |
|       - | 2186 | `								ph7_value sHookRet;` |
|     153 | 2187 | `								PH7_MemObjInit(pVm,&sHookRet);` |
|     153 | 2188 | `								if( PH7_VmHookGetAttrValue(pThis,pObjAttr,&sHookRet) != SXERR_NOTFOUND ){` |
|     135 | 2189 | `									if( pInstr->iP2 == PH7_MEMBER_DEFPATH ){` |
|       - | 2190 | `										/* A deferred call ARGUMENT of a HOOKED property. The` |
|       - | 2191 | `										 * get hook has answered, as php's does; what a` |
|       - | 2192 | `										 * by-REFERENCE parameter then gets is not a notice` |
|       - | 2193 | `										 * but php's refusal — a hook has no slot to alias` |
|       - | 2194 | ``										 * and `&get` does not exist here — while a by-VALUE`` |
|       - | 2195 | `										 * one simply takes the hook's value. */` |
|     107 | 2196 | `										VmDeferredPath *pPre = VmDeferPathNewPrefetch(&(*pVm),` |
|      68 | 2197 | `											VM_OVER_HOOK,pThis->pClass,&pObjAttr->pAttr->sName,&sHookRet);` |
|      73 | 2198 | `										if( pPre ){` |
|      73 | 2199 | `											PH7_MemObjRelease(pTos);` |
|      73 | 2200 | `											pTos->x.pOther = pPre;` |
|      73 | 2201 | `											pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      73 | 2202 | `											pTos->nIdx = SXU32_HIGH;` |
|      73 | 2203 | `											PH7_MemObjRelease(&sHookRet);` |
|      73 | 2204 | `											PH7_ClassInstanceUnref(pThis);` |
|     104 | 2205 | `											VM_EXIT_BREAK;` |
|       - | 2206 | `										}` |
|     ! 0 | 2207 | `									}` |
|      65 | 2208 | `									PH7_MemObjStore(&sHookRet,pTos);` |
|      65 | 2209 | `									pTos->nIdx = SXU32_HIGH;` |
|      65 | 2210 | `									PH7_MemObjRelease(&sHookRet);` |
|      65 | 2211 | `									PH7_ClassInstanceUnref(pThis);` |
|      65 | 2212 | `									VM_EXIT_BREAK;` |
|       - | 2213 | `								}` |
|      19 | 2214 | `								PH7_MemObjRelease(&sHookRet);` |
|       9 | 2215 | `							}` |
|      53 | 2216 | `							if( bCoalesceW \|\| bRmwNext ){` |
|       - | 2217 | ``								/* `$o->x ??= v` (bCoalesceW) and the read-modify-write`` |
|       - | 2218 | ``								 * forms `$o->x++`, `$o->x .= v`, … (bRmwNext): php reads`` |
|       - | 2219 | `								 * through the get hook (raw backing when the property is` |
|       - | 2220 | `								 * set-only) and writes through the set hook.` |
|       - | 2221 | `								 *   - ??=: the test value goes on the stack and a` |
|       - | 2222 | `								 *     COAL_HOOK entry is pushed for the OP_NULLC_STORE` |
|       - | 2223 | `								 *     at the jump target; the fetch-point sweep drops it` |
|       - | 2224 | `								 *     when the short-circuit jump skips the assign or a` |
|       - | 2225 | `								 *     throw abandons the RHS (the entry is NOT a scalar` |
|       - | 2226 | `								 *     transient — nested stores/coalesces in the RHS` |
|       - | 2227 | `								 *     stack their own entries above it).` |
|       - | 2228 | `								 *   - RMW: the value goes into a fresh SCRATCH slot the` |
|       - | 2229 | `								 *     modify op mutates in place; the entry makes the` |
|       - | 2230 | `								 *     op's tail dispatch the set side with the computed` |
|       - | 2231 | `								 *     value (PH7_HOOK_RMW_WRITEBACK). */` |
|       - | 2232 | `								ph7_value sCur;` |
|       - | 2233 | `								sxi32 rcCur;` |
|      35 | 2234 | `								PH7_MemObjInit(pVm,&sCur);` |
|      35 | 2235 | `								rcCur = PH7_VmHookGetAttrValue(pThis,pObjAttr,&sCur);` |
|      35 | 2236 | `								if( rcCur == SXERR_NOTFOUND ){` |
|       - | 2237 | `									/* set-only hook (or guard edge): the read side is the` |
|       - | 2238 | `									 * raw backing store (php) */` |
|       3 | 2239 | `									ph7_value *pBack = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|       3 | 2240 | `									if( pBack ){` |
|       3 | 2241 | `										PH7_MemObjStore(pBack,&sCur);` |
|       2 | 2242 | `									}` |
|      34 | 2243 | `								}else if( pVm->nBoundaryRc != 0 ){` |
|       - | 2244 | `									/* the get hook threw: leave the null temp; the` |
|       - | 2245 | `									 * fetch-point router lands the parked throw —` |
|       - | 2246 | `									 * nothing is armed. */` |
|     ! 0 | 2247 | `									PH7_MemObjRelease(&sCur);` |
|     ! 0 | 2248 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2249 | `									VM_EXIT_BREAK;` |
|       - | 2250 | `								}` |
|      35 | 2251 | `								if( bCoalesceW ){` |
|       - | 2252 | `									VmHookRmw sPend;` |
|      15 | 2253 | `									PH7_MemObjStore(&sCur,pTos);` |
|      15 | 2254 | `									pTos->nIdx = SXU32_HIGH;` |
|      15 | 2255 | `									PH7_MemObjRelease(&sCur);` |
|      15 | 2256 | `									sPend.iKind = VM_HOOK_PEND_COAL_HOOK;` |
|      15 | 2257 | `									sPend.pThis = pThis;` |
|      15 | 2258 | `									sPend.pAttr = pObjAttr->pAttr;` |
|      15 | 2259 | `									sPend.nBackIdx = pObjAttr->nIdx;` |
|      15 | 2260 | `									sPend.nScratchIdx = SXU32_HIGH;` |
|      15 | 2261 | `									SyBlobInit(&sPend.sName,&pVm->sAllocator);` |
|      15 | 2262 | `									sPend.pOwnerStack = (void *)pStack;` |
|      15 | 2263 | `									sPend.pInstrs = (void *)aInstr;` |
|      15 | 2264 | `									sPend.nJmpPc = (sxu32)(pc + 1);            /* the OP_NULLC_JMP */` |
|      15 | 2265 | `									sPend.nPc = (sxu32)(pNextH->iP2 - 1);      /* the OP_NULLC_STORE */` |
|      15 | 2266 | `									pThis->iRef++;` |
|      15 | 2267 | `									SySetPut(&pVm->aHookRmw,(const void *)&sPend);` |
|      15 | 2268 | `									PH7_ClassInstanceUnref(pThis);` |
|      15 | 2269 | `									VM_EXIT_BREAK;` |
|       - | 2270 | `								}` |
|       - | 2271 | `								{` |
|      21 | 2272 | `									ph7_value *pScr = PH7_ReserveMemObj(&(*pVm));` |
|      21 | 2273 | `									if( pScr ){` |
|       - | 2274 | `										VmHookRmw sRmw;` |
|      21 | 2275 | `										PH7_MemObjStore(&sCur,pScr);` |
|      21 | 2276 | `										PH7_MemObjStore(&sCur,pTos);` |
|      21 | 2277 | `										pTos->nIdx = pScr->nIdx;` |
|      21 | 2278 | `										sRmw.iKind = VM_HOOK_PEND_RMW;` |
|      21 | 2279 | `										sRmw.pThis = pThis;` |
|      21 | 2280 | `										sRmw.pAttr = pObjAttr->pAttr;` |
|      21 | 2281 | `										sRmw.nBackIdx = pObjAttr->nIdx;` |
|      21 | 2282 | `										sRmw.nScratchIdx = pScr->nIdx;` |
|      21 | 2283 | `										SyBlobInit(&sRmw.sName,&pVm->sAllocator);` |
|      21 | 2284 | `										sRmw.pOwnerStack = (void *)pStack;` |
|      21 | 2285 | `										sRmw.pInstrs = (void *)aInstr;` |
|      21 | 2286 | `										sRmw.nJmpPc = (sxu32)(pc + 1);  /* the modify op ... */` |
|      21 | 2287 | `										sRmw.nPc = (sxu32)(pc + 1);     /* ... is the whole window */` |
|      21 | 2288 | `										pThis->iRef++;` |
|      21 | 2289 | `										SySetPut(&pVm->aHookRmw,(const void *)&sRmw);` |
|      10 | 2290 | `									}` |
|       - | 2291 | `									/* OOM: pScr == 0 — leave the null temp (loud allocator` |
|       - | 2292 | `									 * diagnostics already fired) */` |
|      21 | 2293 | `									PH7_MemObjRelease(&sCur);` |
|      21 | 2294 | `									PH7_ClassInstanceUnref(pThis);` |
|      21 | 2295 | `									VM_EXIT_BREAK;` |
|       - | 2296 | `								}` |
|       - | 2297 | `							}` |
|       9 | 2298 | `						}` |
|       - | 2299 | ``						/* `$o->p[$k] = v`, `$o->p[] = v` and `unset($o->p[$k])`: the`` |
|       - | 2300 | `						 * property is the BASE of a subscript write, so the write lands` |
|       - | 2301 | `						 * inside whatever it holds. php screens that where it screens a` |
|       - | 2302 | ``						 * store -- `Cannot indirectly modify readonly property C::$p` --`` |
|       - | 2303 | `						 * and it screens it BEFORE the uninitialized-typed read below,` |
|       - | 2304 | `						 * which is why this sits in front. PHL wrote through to the` |
|       - | 2305 | `						 * array a readonly property held. */` |
|       - | 2306 | `						{` |
|  115921 | 2307 | `							VmInstr *pNextI = pInstr + 1;` |
|  115921 | 2308 | `							int bStoreI = (pNextI->iOp == PH7_OP_STORE && pNextI->iP2 != 0);` |
|  115921 | 2309 | `							int bCoalI = pNextI->iOp == PH7_OP_NULLC_JMP;` |
|  176635 | 2310 | `							int bUnsetBase = pInstr->iP2 == PH7_MEMBER_READ` |
|  115930 | 2311 | `								&& pNextI->iOp == PH7_OP_LOAD_IDX && VM_IDX_IS_UNSET(pNextI->iP2);` |
|  173879 | 2312 | `							int bBaseW = bUnsetBase` |
|  222868 | 2313 | `								\|\| (pInstr->iP2 == PH7_MEMBER_WRITE` |
|  111434 | 2314 | `								    && !bStoreI && !bCoalI && !VmMemberNextIsWrite(pNextI));` |
|  115921 | 2315 | `							if( bBaseW ){` |
|     269 | 2316 | `								sxi32 rcInd = PH7_VmCheckIndirectModify(&(*pVm),pObjAttr->nIdx);` |
|     269 | 2317 | `								if( rcInd != SXRET_OK ){` |
|      10 | 2318 | `									VmBoundaryPark(&(*pVm),rcInd);` |
|      10 | 2319 | `									PH7_MemObjRelease(pTos);` |
|      10 | 2320 | `									pTos->nIdx = SXU32_HIGH;` |
|      10 | 2321 | `									PH7_ClassInstanceUnref(pThis);` |
|      10 | 2322 | `									VM_EXIT_BREAK;` |
|       - | 2323 | `								}` |
|     128 | 2324 | `							}` |
|       - | 2325 | `						}` |
|       - | 2326 | `						/* PHP 7.4+: reading an uninitialized typed property is an Error.` |
|       - | 2327 | `						 * We can only raise it on a real read, not when the slot is the` |
|       - | 2328 | `						 * LHS of an assignment — peek at the next instruction to decide.` |
|       - | 2329 | `						 * Safe: the compiler always emits a terminating PH7_OP_DONE, so` |
|       - | 2330 | `						 * pInstr+1 is in-bounds while we are inside a non-DONE opcode. */` |
|  115908 | 2331 | `						if( (pObjAttr->iState & VM_CLASS_ATTR_UNINIT)` |
|   58280 | 2332 | `						 && (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|     647 | 2333 | `							VmInstr *pNext = pInstr + 1;` |
|     647 | 2334 | `							int bIsLhs = 0;` |
|     647 | 2335 | `							if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|     561 | 2336 | `								bIsLhs = 1;` |
|     278 | 2337 | `							}` |
|     647 | 2338 | `							if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 2339 | `								/* Destructuring target: the OP_LOAD_LIST store (typed-` |
|       - | 2340 | `								 * enforced) initializes it — never a read (php). */` |
|       7 | 2341 | `								bIsLhs = 1;` |
|       3 | 2342 | `							}` |
|       - | 2343 | ``							/* isset()/empty()/`??` read an uninitialized typed property`` |
|       - | 2344 | `							 * as "not set" — a silent miss, NOT the Error a plain read` |
|       - | 2345 | `							 * raises (php). The compiler tags such an access iP2 =` |
|       - | 2346 | `							 * ISSET/EMPTY; treat it like the assignment-LHS case and fall` |
|       - | 2347 | `							 * through to load the slot's NULL. */` |
|     647 | 2348 | `							if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|      28 | 2349 | `								bIsLhs = 1;` |
|      13 | 2350 | `							}` |
|       - | 2351 | ``							/* A WRITE base is not a read either. `$o->p ??= v` takes the`` |
|       - | 2352 | `							 * slot's NULL and assigns over it, and a DIMENSION write` |
|       - | 2353 | ``							 * (`$o->t['n'] = v`) AUTO-INITIALIZES an array when the`` |
|       - | 2354 | `							 * declared type has room for one -- php's` |
|       - | 2355 | `							 * zend_handle_fetch_obj_flags -- or refuses with its own` |
|       - | 2356 | `							 * TypeError when it does not. Raising the read Error here` |
|       - | 2357 | `							 * instead is what stopped Doctrine's ClassMetadata, whose` |
|       - | 2358 | ``							 * `public array $table;` is filled exactly that way. */`` |
|     647 | 2359 | `							if( (pInstr + 1)->iOp == PH7_OP_NULLC_JMP ){` |
|       3 | 2360 | ``								bIsLhs = 1;   /* `$o->p ??= v` assigns over the unset slot */`` |
|     646 | 2361 | `							}else if( VmMemberFetchIsRefSource(pInstr) ){` |
|       - | 2362 | `								/* A REFERENCE fetch has a rule of its own: php seeds NULL` |
|       - | 2363 | `								 * and binds when the declared type admits one, and refuses` |
|       - | 2364 | ``								 * with `Cannot access uninitialized non-nullable property`` |
|       - | 2365 | ``								 * ... by reference` when it does not. The auto-initialize`` |
|       - | 2366 | `								 * rule below belongs to a DIMENSION write, and running it` |
|       - | 2367 | ``								 * here turned `?int $t` into an ARRAY. */`` |
|      25 | 2368 | `								sxi32 rcRs = VmRefUninitTypedProperty(&(*pVm),pClass,pObjAttr,` |
|      16 | 2369 | `									(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pObjAttr->nIdx));` |
|      17 | 2370 | `								if( rcRs != SXRET_OK ){` |
|       9 | 2371 | `									VmBoundaryPark(&(*pVm),rcRs);` |
|       9 | 2372 | `									PH7_ClassInstanceUnref(pThis);` |
|       9 | 2373 | `									VM_EXIT_BREAK;` |
|       - | 2374 | `								}` |
|       9 | 2375 | `								bIsLhs = 1;` |
|     633 | 2376 | `							}else if( VmMemberFetchForWrite(pInstr) ){` |
|      17 | 2377 | `								sxi32 rcAI = VmAutoInitArrayProperty(&(*pVm),pObjAttr,` |
|      10 | 2378 | `									(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pObjAttr->nIdx));` |
|      12 | 2379 | `								if( rcAI != SXRET_OK ){` |
|       3 | 2380 | `									VmBoundaryPark(&(*pVm),rcAI);` |
|       3 | 2381 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 2382 | `									VM_EXIT_BREAK;` |
|       - | 2383 | `								}` |
|       9 | 2384 | `								bIsLhs = 1;` |
|       4 | 2385 | `							}` |
|     637 | 2386 | `							if( !bIsLhs ){` |
|      30 | 2387 | `								sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pObjAttr->pAttr);` |
|      30 | 2388 | `								PH7_ClassInstanceUnref(pThis);` |
|      30 | 2389 | `								if( rcU == PH7_ABORT ){` |
|       3 | 2390 | `									VM_EXIT_ABORT;` |
|       - | 2391 | `								}` |
|       - | 2392 | `								{` |
|       - | 2393 | `									sxi32 iRp;` |
|      28 | 2394 | `									if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|       9 | 2395 | `										PH7_RESUME_DRAIN()` |
|       9 | 2396 | `										pc = iRp;` |
|       9 | 2397 | `										VM_EXIT_BREAK;` |
|       - | 2398 | `									}` |
|       - | 2399 | `								}` |
|      20 | 2400 | `								VM_EXIT_EXCEPTION;` |
|       - | 2401 | `							}` |
|     303 | 2402 | `						}` |
|       - | 2403 | `						/* Load attribute */` |
|  115877 | 2404 | `						pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pObjAttr->nIdx);` |
|  115877 | 2405 | `						if( pValue ){` |
|  115877 | 2406 | `							if( pThis->iRef < 2 ){` |
|       - | 2407 | `								/* Perform a store operation,rather than a load operation since` |
|       - | 2408 | `								 * the class instance '$this' will be deleted shortly.` |
|       - | 2409 | `								 */` |
|     823 | 2410 | `								PH7_MemObjStore(pValue,pTos);` |
|     414 | 2411 | `							}else{` |
|       - | 2412 | `								/* Simple load */` |
|  115059 | 2413 | `								PH7_MemObjLoad(pValue,pTos);` |
|       - | 2414 | `							}` |
|  115877 | 2415 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|  115877 | 2416 | `								if( pThis->iRef > 1 ){` |
|       - | 2417 | `									/* Load attribute index */` |
|  115059 | 2418 | `									pTos->nIdx = pObjAttr->nIdx;` |
|  115054 | 2419 | `									if( VmMemberFetchIsRefSource(pInstr)` |
|   57603 | 2420 | `									 && (pObjAttr->iState & (VM_CLASS_ATTR_REFBOUND\|VM_CLASS_ATTR_REFSRCPIN)) == 0 ){` |
|       - | 2421 | `										/* This fetch is about to become a reference, so php makes` |
|       - | 2422 | `										 * THIS property a reference too. The table cannot name a` |
|       - | 2423 | `										 * property as a holder, so without a pin of its own the` |
|       - | 2424 | `										 * slot's only recorded holder is the other end's -- and` |
|       - | 2425 | ``										 * `$q->p =& $o->p` followed by $q's death freed the value`` |
|       - | 2426 | `										 * and left $o->p reading NULL. One counted pin, given back` |
|       - | 2427 | `										 * when the property is released; the bit makes it` |
|       - | 2428 | `										 * idempotent, so a source fetched in a loop pins once. */` |
|     132 | 2429 | `										pObjAttr->iState \|= VM_CLASS_ATTR_REFSRCPIN;` |
|     132 | 2430 | `										VmPinMemObjSlotCounted(&(*pVm),pObjAttr->nIdx);` |
|      65 | 2431 | `									}` |
|   57527 | 2432 | `								}` |
|   57936 | 2433 | `							}` |
|  115872 | 2434 | `							if( (pObjAttr->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_SET)` |
|   59412 | 2435 | `							 && !VmMemberNativeSetKeepsSlot(pInstr) ){` |
|    2328 | 2436 | `								pTos->nIdx = SXU32_HIGH;` |
|    2328 | 2437 | `								pTos->iFlags \|= MEMOBJ_AUX_NATIVEPROP;` |
|    1162 | 2438 | `							}` |
|   57936 | 2439 | `						}` |
|  115877 | 2440 | `						if( pInstr->iP2 == PH7_MEMBER_ISSET ){` |
|       - | 2441 | `							/* isset() tests null-ness and nothing else, so reduce the loaded` |
|       - | 2442 | `							 * value to the same non-null marker the __isset and get-hook` |
|       - | 2443 | `							 * paths push. A property read off a TEMPORARY receiver` |
|       - | 2444 | ``							 * (`isset(f()->p)`, `isset((new C)->p)`, and now every`` |
|       - | 2445 | `							 * intermediate link of an accessor chain) leaves no variable` |
|       - | 2446 | `							 * index behind, and the trailing builtin read that as a` |
|       - | 2447 | `							 * CONSTANT and warned -- a diagnostic php has no equivalent of,` |
|       - | 2448 | `							 * its isset() being a language construct rather than a call. */` |
|      91 | 2449 | `							int bSet = (pTos->iFlags & MEMOBJ_NULL) == 0;` |
|      91 | 2450 | `							PH7_MemObjRelease(pTos);` |
|      91 | 2451 | `							if( bSet ){` |
|      67 | 2452 | `								pTos->x.iVal = 1;` |
|      67 | 2453 | `								MemObjSetType(pTos,MEMOBJ_BOOL);` |
|      32 | 2454 | `							}` |
|      91 | 2455 | `							pTos->nIdx = SXU32_HIGH;` |
|      44 | 2456 | `						}` |
|   57941 | 2457 | `					}else{` |
|       - | 2458 | `						/* Inaccessible (private/protected) from this scope. php consults` |
|       - | 2459 | `						 * __get here exactly like a missing property (band A #3a) before` |
|       - | 2460 | `						 * erroring; the guard keeps a self-recursive read from looping. */` |
|      50 | 2461 | `						ph7_class_method *pGetMagic = 0;` |
|       - | 2462 | ``						/* A WRITE-context fetch (the base of `$o->p[k] = v`) reads through`` |
|       - | 2463 | `						 * __get in php too — the write then lands on the value it answered` |
|       - | 2464 | `						 * and is lost, with php's indirect-modification notice. PHL refused` |
|       - | 2465 | ``						 * the whole statement with `Cannot access private property` instead,`` |
|       - | 2466 | ``						 * a fatal on a program php runs. A plain store and a `??=` keep`` |
|       - | 2467 | `						 * their own paths below. */` |
|      46 | 2468 | `						if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|      65 | 2469 | `						 && (VmMemberFetchForWrite(pInstr)` |
|      40 | 2470 | `						  \|\| (pInstr->iP2 != PH7_MEMBER_WRITE && !VmMemberNextIsWrite(pInstr + 1))) ){` |
|      44 | 2471 | `							pGetMagic = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|      20 | 2472 | `						}` |
|       - | 2473 | `						/* ++/--/compound-assign on a DECLARED but inaccessible property:` |
|       - | 2474 | `						 * php's accessors answer for it exactly as they do for a missing` |
|       - | 2475 | `						 * one (__get, modify, __set), where PHL raised` |
|       - | 2476 | ``						 * `Cannot access private property C::$n`. A plain store keeps its`` |
|       - | 2477 | `						 * own __set park below — it is a write, but not a read-modify-write. */` |
|      46 | 2478 | `						if( VmMemberNextIsRmw(pInstr + 1)` |
|      28 | 2479 | `						 && VmMagicRmwEligible(pVm,pClass,pThis,&sName) ){` |
|       - | 2480 | `							/* The name was already popped and pTos released above, so` |
|       - | 2481 | `							 * sName is the instruction's own literal here. */` |
|       - | 2482 | `							ph7_value sRmwVal;` |
|       - | 2483 | `							sxu32 nRmwScratch;` |
|       3 | 2484 | `							PH7_MemObjInit(pVm,&sRmwVal);` |
|       4 | 2485 | `							VmMagicRmwArm(&(*pVm),pThis,pClass,&sName,&sRmwVal,&nRmwScratch,` |
|       2 | 2486 | `								(void *)pStack,(void *)aInstr,(sxu32)(pc + 1));` |
|       3 | 2487 | `							PH7_MemObjStore(&sRmwVal,pTos);` |
|       3 | 2488 | `							pTos->nIdx = nRmwScratch;` |
|       3 | 2489 | `							PH7_MemObjRelease(&sRmwVal);` |
|       3 | 2490 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 2491 | `							VM_EXIT_BREAK;` |
|       - | 2492 | `						}` |
|      48 | 2493 | `						if( pGetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 2494 | `							ph7_value sMagicRet;` |
|       9 | 2495 | `							PH7_MemObjInit(pVm,&sMagicRet);` |
|       9 | 2496 | `							VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|       9 | 2497 | `							PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sMagicRet);` |
|       9 | 2498 | `							VmMagicGuardPop(pVm);` |
|       9 | 2499 | `							if( VmMemberFetchForWrite(pInstr) ){` |
|       5 | 2500 | `								PH7_VmOverloadedPropNotice(&(*pVm),pClass,&sName,&sMagicRet);` |
|       2 | 2501 | `							}` |
|       - | 2502 | `							/* The name was already popped and pTos released above; just` |
|       - | 2503 | `							 * take the magic result as the expression value. */` |
|       9 | 2504 | `							PH7_MemObjStore(&sMagicRet,pTos);` |
|       9 | 2505 | `							pTos->nIdx = SXU32_HIGH;` |
|       9 | 2506 | `							PH7_MemObjRelease(&sMagicRet);` |
|       9 | 2507 | `							PH7_ClassInstanceUnref(pThis);` |
|       9 | 2508 | `							VM_EXIT_BREAK;` |
|       - | 2509 | `						}` |
|      40 | 2510 | `						if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2511 | `							/* isset/empty on an inaccessible property: php consults __isset` |
|       - | 2512 | `							 * (band A #3b), and is silently false without it — never an` |
|       - | 2513 | `							 * Error (pre-fix PHL fataled here). */` |
|       3 | 2514 | `							ph7_class_method *pIssetMagic = PH7_ClassExtractMethod(pClass,"__isset",sizeof("__isset")-1);` |
|       3 | 2515 | `							int bSet = 0;` |
|       3 | 2516 | `							if( pIssetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'i') ){` |
|       - | 2517 | `								ph7_value sIssetRet;` |
|     ! 0 | 2518 | `								PH7_MemObjInit(pVm,&sIssetRet);` |
|     ! 0 | 2519 | `								VmMagicGuardPush(pVm,(void *)pThis,&sName,'i');` |
|     ! 0 | 2520 | `								PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__isset",sizeof("__isset")-1,&sName,&sIssetRet);` |
|     ! 0 | 2521 | `								VmMagicGuardPop(pVm);` |
|     ! 0 | 2522 | `								PH7_MemObjToBool(&sIssetRet);` |
|     ! 0 | 2523 | `								bSet = sIssetRet.x.iVal != 0;` |
|     ! 0 | 2524 | `								PH7_MemObjRelease(&sIssetRet);` |
|     ! 0 | 2525 | `							}` |
|       3 | 2526 | `							if( pInstr->iP2 == PH7_MEMBER_COALESCE && pIssetMagic == 0 ){` |
|       - | 2527 | ``								/* `??` with no __isset to gate it: php reads straight through`` |
|       - | 2528 | `								 * __get, exactly as it does for a MISSING property. The` |
|       - | 2529 | `								 * __get dispatch just above this block is gated on a` |
|       - | 2530 | `								 * non-lookup context, so answer here. */` |
|     ! 0 | 2531 | `								ph7_class_method *pCoalGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|     ! 0 | 2532 | `								if( pCoalGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|       - | 2533 | `									ph7_value sCoalRet;` |
|     ! 0 | 2534 | `									PH7_MemObjInit(pVm,&sCoalRet);` |
|     ! 0 | 2535 | `									VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     ! 0 | 2536 | `									PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sCoalRet);` |
|     ! 0 | 2537 | `									VmMagicGuardPop(pVm);` |
|     ! 0 | 2538 | `									PH7_MemObjStore(&sCoalRet,pTos);` |
|     ! 0 | 2539 | `									pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 2540 | `									PH7_MemObjRelease(&sCoalRet);` |
|     ! 0 | 2541 | `								}` |
|     ! 0 | 2542 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2543 | `								VM_EXIT_BREAK;` |
|       - | 2544 | `							}` |
|       3 | 2545 | `							if( bSet ){` |
|     ! 0 | 2546 | `								if( VmMemberCtxWantsValue(pInstr->iP2) ){` |
|     ! 0 | 2547 | `									ph7_class_method *pEmptyGet = PH7_ClassExtractMethod(pClass,"__get",sizeof("__get")-1);` |
|       - | 2548 | `									ph7_value sEmptyVal;` |
|     ! 0 | 2549 | `									PH7_MemObjInit(pVm,&sEmptyVal);` |
|     ! 0 | 2550 | `									if( pEmptyGet && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'g') ){` |
|     ! 0 | 2551 | `										VmMagicGuardPush(pVm,(void *)pThis,&sName,'g');` |
|     ! 0 | 2552 | `										PH7_ClassInstanceCallMagicMethod(&(*pVm),pClass,pThis,"__get",sizeof("__get")-1,&sName,&sEmptyVal);` |
|     ! 0 | 2553 | `										VmMagicGuardPop(pVm);` |
|     ! 0 | 2554 | `									}` |
|     ! 0 | 2555 | `									PH7_MemObjStore(&sEmptyVal,pTos);` |
|     ! 0 | 2556 | `									pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 2557 | `									PH7_MemObjRelease(&sEmptyVal);` |
|     ! 0 | 2558 | `								}else{` |
|     ! 0 | 2559 | `									pTos->x.iVal = 1;` |
|     ! 0 | 2560 | `									MemObjSetType(pTos,MEMOBJ_BOOL);` |
|     ! 0 | 2561 | `									pTos->nIdx = SXU32_HIGH;` |
|       - | 2562 | `								}` |
|     ! 0 | 2563 | `							}` |
|       3 | 2564 | `							PH7_ClassInstanceUnref(pThis);` |
|       3 | 2565 | `							VM_EXIT_BREAK;` |
|       - | 2566 | `						}` |
|       - | 2567 | `						{` |
|       - | 2568 | `							/* A plain store to an inaccessible property dispatches __set` |
|       - | 2569 | `							 * (band A #3b): park the receiver+name for the following` |
|       - | 2570 | `							 * OP_STORE, exactly like the missing-property case. */` |
|      38 | 2571 | `							VmInstr *pNextW = pInstr + 1;` |
|      38 | 2572 | `							if( pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0 ){` |
|       3 | 2573 | `								ph7_class_method *pSetMagic = PH7_ClassExtractMethod(pClass,"__set",sizeof("__set")-1);` |
|       3 | 2574 | `								if( pSetMagic && !VmMagicGuardHeld(pVm,(void *)pThis,&sName,'s') ){` |
|       3 | 2575 | `									pThis->iRef++;` |
|       3 | 2576 | `									pVm->pMagicSetThis = pThis;` |
|       3 | 2577 | `									SyBlobReset(&pVm->sMagicSetName);` |
|       3 | 2578 | `									SyBlobAppend(&pVm->sMagicSetName,(const void *)sName.zString,sName.nByte);` |
|       3 | 2579 | `									pTos->nIdx = SXU32_HIGH; /* sentinel slot for OP_STORE */` |
|       3 | 2580 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 2581 | `									VM_EXIT_BREAK;` |
|       - | 2582 | `								}` |
|     ! 0 | 2583 | `							}` |
|       - | 2584 | `						}` |
|      32 | 2585 | `						if( ((pInstr + 1)->iOp == PH7_OP_NULLC \|\| (pInstr + 1)->iOp == PH7_OP_NULLC_JMP)` |
|      20 | 2586 | `						 && pInstr->iP2 != PH7_MEMBER_WRITE ){` |
|       - | 2587 | `` 							/* `$o->priv ?? default` (OP_NULLC; NULLC_JMP is the `??=` `` |
|       - | 2588 | ``							 * pre-test): php treats `??` as an isset-style lookup — an`` |
|       - | 2589 | `							 * inaccessible property yields null SILENTLY (the` |
|       - | 2590 | `							 * __isset/__get consults already ran above), letting the` |
|       - | 2591 | `							 * coalesce pick the default. */` |
|     ! 0 | 2592 | `							PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2593 | `							VM_EXIT_BREAK; /* pTos is already the null temp */` |
|       - | 2594 | `						}` |
|       - | 2595 | `						/* A subclass reading a PARENT's PRIVATE property used to be` |
|       - | 2596 | `						 * special-cased into an "Undefined property" warning here. It is` |
|       - | 2597 | `						 * php's answer, but not because of the SCOPE: the base's private` |
|       - | 2598 | `						 * lives under its mangled storage name, so the subclass's plain` |
|       - | 2599 | `						 * lookup never reaches this point at all -- and reading the SAME` |
|       - | 2600 | `						 * property on an instance of the declaring class, which does, is` |
|       - | 2601 | `						 * php's ordinary visibility refusal. Deciding it from the scope` |
|       - | 2602 | `						 * turned that one into a warning too. */` |
|       - | 2603 | `						/* Genuinely inaccessible: php's CATCHABLE Error (parked on the` |
|       - | 2604 | `						 * boundary rail; the fetch-point router lands it — it was an` |
|       - | 2605 | `						 * uncatchable VmReportUncaughtException+Abort before). */` |
|       - | 2606 | `						{` |
|       - | 2607 | `						SyBlob sErrMsg;` |
|      36 | 2608 | `						const char *zVis = pObjAttr->pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected";` |
|      36 | 2609 | `						SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      36 | 2610 | `						SyBlobFormat(&sErrMsg,"Cannot access %s property %z::$%z",` |
|      16 | 2611 | `							zVis,&pClass->sDisp,&sName);` |
|      36 | 2612 | `						PH7_ClassInstanceUnref(pThis);` |
|      36 | 2613 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      36 | 2614 | `						SyBlobRelease(&sErrMsg);` |
|      36 | 2615 | `						VM_EXIT_BREAK;` |
|       - | 2616 | `						}` |
|       - | 2617 | `					}` |
|   57936 | 2618 | `				}` |
|       - | 2619 | `				/* Safely unreference the object */` |
|  117295 | 2620 | `				PH7_ClassInstanceUnref(pThis);` |
|       - | 2621 | `			}` |
|  150294 | 2622 | `		}else{` |
|     190 | 2623 | `			if( (pNos->iFlags & MEMOBJ_AUX_STROFFSET)` |
|     103 | 2624 | `			 && (pInstr->iP2 == PH7_MEMBER_WRITE \|\| pInstr->iP2 == PH7_MEMBER_UNSET` |
|       5 | 2625 | `			  \|\| pInstr->iP2 == PH7_MEMBER_LIST_TARGET \|\| pInstr->iP2 == PH7_MEMBER_REF_TARGET` |
|       - | 2626 | ``			  /* A reference SOURCE (`$r =& $s[0]->p`) is compiled as a READ here on`` |
|       - | 2627 | `			   * purpose — php hands back a copy for a handler-backed property — so the` |
|       - | 2628 | `			   * bind that follows is what makes it a reach-inside, exactly as it does` |
|       - | 2629 | `			   * for a subscript. */` |
|       4 | 2630 | `			  \|\| (pInstr->iP2 == PH7_MEMBER_READ` |
|       3 | 2631 | `			      && ((pInstr + 1)->iOp == PH7_OP_STORE_REF` |
|       1 | 2632 | `			       \|\| (pInstr + 1)->iOp == PH7_OP_LOAD_REF` |
|     ! 0 | 2633 | `			       \|\| (pInstr + 1)->iOp == PH7_OP_STORE_IDX_REF))) ){` |
|       - | 2634 | `				/* The base is a string OFFSET and this reaches INSIDE it. php refuses every` |
|       - | 2635 | `				 * such reach and words the refusal from what is doing the reaching, so a` |
|       - | 2636 | ``				 * PROPERTY is `Cannot use string offset as an object` where a subscript is`` |
|       - | 2637 | ``				 * `... as an array` — `$s[0]->p = 1`, `$s[0]->p += 1`, `$s[0]->p ??= 1` and`` |
|       - | 2638 | ``				 * `unset($s[0]->p)` alike. PHL reported the generic non-object write`` |
|       - | 2639 | ``				 * (`Attempt to assign property "p" on string`) and said nothing at all for`` |
|       - | 2640 | ``				 * the unset. A METHOD CALL is not one of these: php keeps `Call to a member`` |
|       - | 2641 | ``				 * function p() on string` there, and so does the path below. */`` |
|       - | 2642 | `				sxi32 rcSo;` |
|      11 | 2643 | `				VmPopOperand(&pTos,1);` |
|      11 | 2644 | `				PH7_MemObjRelease(pTos);` |
|      11 | 2645 | `				MemObjSetType(pTos,MEMOBJ_NULL);` |
|      11 | 2646 | `				pTos->nIdx = SXU32_HIGH;` |
|      11 | 2647 | `				rcSo = VmThrowFromVm(&(*pVm),"Error","Cannot use string offset as an object",` |
|       - | 2648 | `					sizeof("Cannot use string offset as an object")-1);` |
|      11 | 2649 | `				if( rcSo == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 | 2650 | `				rc = rcSo;` |
|      11 | 2651 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2652 | `			}` |
|       - | 2653 | ``			/* `->` on a non-object (e.g. a null intermediate). Silent in isset()/empty()/unset()`` |
|       - | 2654 | ``			 * context (iP2 2/3/4) so `isset($o->missing->x)` / `unset($o->missing->x)` match PHP. */`` |
|     182 | 2655 | `			if( pInstr->iP2 != PH7_MEMBER_UNSET && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 2656 | `				/* php draws a sharp line here, and PH7 drew none: CALLING a method on a` |
|       - | 2657 | `				 * non-object is a catchable Error, while READING a property off one is a` |
|       - | 2658 | `				 * Warning that yields NULL. PH7 raised the same notice for both and` |
|       - | 2659 | ``				 * carried on with NULL, so `$null->m()` silently did nothing. */`` |
|       - | 2660 | `				SyString sMemb;` |
|     114 | 2661 | `				const char *zVerb = 0;` |
|     114 | 2662 | `				SyStringInitFromBuf(&sMemb,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|     114 | 2663 | `				if( pInstr->iP2 == PH7_MEMBER_DEFPATH && pNos->nIdx != SXU32_HIGH ){` |
|       - | 2664 | ``					/* A deferred call ARGUMENT (`f($u->p)`): which diagnostic php raises`` |
|       - | 2665 | `					 * depends on the parameter, and this op does not know it — a by-VALUE` |
|       - | 2666 | `					 * binding is the read warning below, a by-REFERENCE one is php's` |
|       - | 2667 | ``					 * `Attempt to modify property` Error. Record the step rooted at the`` |
|       - | 2668 | `					 * base and let OP_CALL re-drive it in the mode the callee decides,` |
|       - | 2669 | `					 * exactly as a property MISSING on a real object is recorded. */` |
|      27 | 2670 | `					VmDeferredPath *pPath = VmDeferPathNew(&(*pVm),0,pNos->nIdx,0);` |
|      27 | 2671 | `					if( pPath && VmDeferPathPushProp(pPath,&sMemb) == SXRET_OK ){` |
|      27 | 2672 | `						VmPopOperand(&pTos,1);   /* drop the property name */` |
|      27 | 2673 | `						PH7_MemObjRelease(pTos); /* collapse the base slot into the carrier */` |
|      27 | 2674 | `						pTos->x.pOther = pPath;` |
|      27 | 2675 | `						pTos->iFlags = MEMOBJ_NULL \| MEMOBJ_AUX_DEFPATH;` |
|      27 | 2676 | `						pTos->nIdx = SXU32_HIGH;` |
|      53 | 2677 | `						VM_EXIT_BREAK;` |
|       - | 2678 | `					}` |
|     ! 0 | 2679 | `					if( pPath ){` |
|     ! 0 | 2680 | `						VmFreeDeferredPath(pPath);` |
|     ! 0 | 2681 | `					}` |
|       - | 2682 | `					/* fall through to the immediate warning on allocation failure */` |
|     ! 0 | 2683 | `				}` |
|       - | 2684 | `				/* WRITING one is an Error too, and php picks its verb from what the` |
|       - | 2685 | `				 * write actually is. PH7 warned about a READ it never performed and` |
|       - | 2686 | `				 * then let the store fail into its own` |
|       - | 2687 | `				 * "Cannot perform assignment on a constant class attribute", so the` |
|       - | 2688 | `				 * script carried on past a statement php stops it for. The kind is` |
|       - | 2689 | `				 * read off the following instruction, the way every other write` |
|       - | 2690 | `				 * classification in this handler is (VmMemberFetchForWrite). */` |
|      88 | 2691 | `				if( pInstr->iP2 == PH7_MEMBER_WRITE ){` |
|      33 | 2692 | `					const VmInstr *pNextW = pInstr + 1;` |
|      32 | 2693 | `					if( (pNextW->iOp == PH7_OP_STORE && pNextW->iP2 != 0)` |
|      22 | 2694 | ``					 \|\| pNextW->iOp == PH7_OP_NULLC_JMP /* `$u->p ??= v` */`` |
|      12 | 2695 | `					 \|\| VmNextIsCompoundAssign(pNextW) ){` |
|      25 | 2696 | `						zVerb = "assign";` |
|      21 | 2697 | `					}else if( pNextW->iOp == PH7_OP_INCR \|\| pNextW->iOp == PH7_OP_DECR ){` |
|       5 | 2698 | `						zVerb = "increment/decrement";` |
|       3 | 2699 | `					}else{` |
|       - | 2700 | ``						/* The base of a subscript write (`$u->p[] = v`): php asks the`` |
|       - | 2701 | `						 * property for something to modify, and there is no property. */` |
|       5 | 2702 | `						zVerb = "modify";` |
|       1 | 2703 | `					}` |
|      72 | 2704 | `				}else if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       3 | 2705 | `					zVerb = "assign";` |
|      55 | 2706 | `				}else if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       3 | 2707 | `					zVerb = "modify";` |
|      53 | 2708 | `				}else if( VmMemberFetchIsRefSource(pInstr) ){` |
|       - | 2709 | ``					/* `$r =& $o->missing->p`: the intermediate is null and php asks it`` |
|       - | 2710 | ``					 * for something to bind to, which is its `modify` sentence -- not`` |
|       - | 2711 | `					 * the read warning this used to fall through to. */` |
|       3 | 2712 | `					zVerb = "modify";` |
|       1 | 2713 | `				}` |
|      88 | 2714 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD \|\| zVerb ){` |
|       - | 2715 | `					SyBlob sErrM;` |
|       - | 2716 | `					sxi32 rcErr;` |
|      53 | 2717 | `					SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      53 | 2718 | `					if( zVerb ){` |
|      39 | 2719 | `						SyBlobFormat(&sErrM,"Attempt to %s property \"%z\" on %s",` |
|      19 | 2720 | `							zVerb,&sMemb,VmArithValueName(pNos));` |
|      20 | 2721 | `					}else{` |
|      15 | 2722 | `						SyBlobFormat(&sErrM,"Call to a member function %z() on %s",` |
|       7 | 2723 | `							&sMemb,VmArithValueName(pNos));` |
|       - | 2724 | `					}` |
|      53 | 2725 | `					VmPopOperand(&pTos,1);` |
|      53 | 2726 | `					PH7_MemObjRelease(pTos);` |
|      53 | 2727 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      53 | 2728 | `					pTos->nIdx = SXU32_HIGH;` |
|      79 | 2729 | `					rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      26 | 2730 | `						SyBlobLength(&sErrM));` |
|      53 | 2731 | `					SyBlobRelease(&sErrM);` |
|      53 | 2732 | `					if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      53 | 2733 | `					rc = rcErr;` |
|      53 | 2734 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2735 | `				}` |
|      53 | 2736 | `				VmErrorFormat(&(*pVm),PH7_CTX_WARNING,"Attempt to read property \"%z\" on %s",` |
|      17 | 2737 | `					&sMemb,VmArithValueName(pNos));` |
|      17 | 2738 | `			}` |
|     104 | 2739 | `			VmPopOperand(&pTos,1);` |
|     104 | 2740 | `			PH7_MemObjRelease(pTos);` |
|     104 | 2741 | `			pTos->nIdx = SXU32_HIGH; /* Assume we are loading a constant */` |
|       - | 2742 | `		}` |
|  150345 | 2743 | `	}else{` |
|       - | 2744 | `		/* Static member access using class name */` |
|  124572 | 2745 | `		pNos = pTos;` |
|  124572 | 2746 | `		pThis = 0;` |
|  124572 | 2747 | `		if( !pInstr->p3 ){` |
|   22536 | 2748 | `			SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),SyBlobLength(&pTos->sBlob));` |
|   22536 | 2749 | `			pNos--;` |
|       - | 2750 | `#ifdef UNTRUST` |
|       - | 2751 | `			if( pNos < pStack ){` |
|       - | 2752 | `				VM_EXIT_ABORT;` |
|       - | 2753 | `			}` |
|       - | 2754 | `#endif` |
|   11263 | 2755 | `		}else{` |
|       - | 2756 | `			/* Attribute name already computed */` |
|  102041 | 2757 | `			SyStringInitFromBuf(&sName,pInstr->p3,SyStrlen((const char *)pInstr->p3));` |
|       - | 2758 | `		}` |
|  124567 | 2759 | `		if( (pNos->iFlags & MEMOBJ_OBJ) == 0 && !pInstr->bDiscard` |
|  123086 | 2760 | `		 && pInstr->p3 == 0 && pInstr->iP1 != 2` |
|   20824 | 2761 | `		 && sName.nByte == sizeof("class")-1` |
|   10518 | 2762 | `		 && SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|       - | 2763 | ``			/* `$expr::class` takes an OBJECT and nothing else: php compiles it to`` |
|       - | 2764 | `			 * ZEND_FETCH_CLASS_NAME, which refuses every other operand -- a class NAME` |
|       - | 2765 | ``			 * held in a string included -- with `Cannot use "::class" on <value>`. A`` |
|       - | 2766 | `			 * written keyword (self::class) or a literal name (A::class, ('A')::class)` |
|       - | 2767 | `			 * is folded before it gets here. PHL looked the string up as a class, so` |
|       - | 2768 | ``			 * `$c::class` answered the name back, or `Class "x" not found`. */`` |
|       - | 2769 | `			SyBlob sErrCn;` |
|       - | 2770 | `			sxi32 rcCn;` |
|      39 | 2771 | `			SyBlobInit(&sErrCn,&pVm->sAllocator);` |
|      39 | 2772 | `			SyBlobFormat(&sErrCn,"Cannot use \"::class\" on %s",VmArithValueName(pNos));` |
|      39 | 2773 | `			if( !pInstr->p3 ){` |
|      39 | 2774 | `				VmPopOperand(&pTos,1);` |
|      19 | 2775 | `			}` |
|      39 | 2776 | `			PH7_MemObjRelease(pTos);` |
|      39 | 2777 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      39 | 2778 | `			pTos->nIdx = SXU32_HIGH;` |
|      58 | 2779 | `			rcCn = VmThrowFromVm(&(*pVm),"TypeError",(const char *)SyBlobData(&sErrCn),` |
|      19 | 2780 | `				SyBlobLength(&sErrCn));` |
|      39 | 2781 | `			SyBlobRelease(&sErrCn);` |
|      39 | 2782 | `			if( rcCn == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      39 | 2783 | `			rc = rcCn;` |
|     111 | 2784 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2785 | `		}` |
|  124534 | 2786 | `		if( pNos->iFlags & (MEMOBJ_STRING\|MEMOBJ_OBJ) ){` |
|  124494 | 2787 | `			ph7_class *pClass = 0;` |
|       - | 2788 | `			/* A FORWARDING static call — self::/parent::/static:: — preserves the` |
|       - | 2789 | `			 * caller's late-static-binding class (php); a non-forwarding C::m() resets` |
|       - | 2790 | `			 * it to C. The receiver slot below keeps the literal keyword, which OP_CALL` |
|       - | 2791 | `			 * can't resolve, so it would fall back to the callee's DECLARING class and` |
|       - | 2792 | `			 * lose LSB. Remember the live LSB class here and stamp it onto the receiver` |
|       - | 2793 | `			 * after the method name is pushed. */` |
|  124494 | 2794 | `			int bForwardingCall = 0;` |
|  124494 | 2795 | `			ph7_class *pForwardLsb = 0;` |
|       - | 2796 | `			/* Same rail snapshot as OP_NEW: the lookup below can run an autoloader that` |
|       - | 2797 | `			 * throws, and php then reports that exception and nothing else. */` |
|  124494 | 2798 | `			sxi32 nMbBrc = pVm->nBoundaryRc;` |
|  124494 | 2799 | `			const void *pMbRes = (const void *)pVm->pResumeFrame;` |
|  124494 | 2800 | `			if( pNos->iFlags & MEMOBJ_OBJ ){` |
|       - | 2801 | `				/* Class already instantiated */` |
|      79 | 2802 | `				pThis = (ph7_class_instance *)pNos->x.pOther;` |
|      79 | 2803 | `				pClass = pThis->pClass;` |
|      79 | 2804 | `				pThis->iRef++; /* Deffer garbage collection */` |
|      41 | 2805 | `			}else{` |
|       - | 2806 | `				/* Try to extract the target class */` |
|  124418 | 2807 | `				if( SyBlobLength(&pNos->sBlob) > 0 ){` |
|  124418 | 2808 | `					const char *zCls = (const char *)SyBlobData(&pNos->sBlob);` |
|  124418 | 2809 | `					sxu32 nCls = (sxu32)SyBlobLength(&pNos->sBlob);` |
|       - | 2810 | `					/* Handle self/static/parent keywords -- WRITTEN ones only` |
|       - | 2811 | ``					 * (VmInstr::bDiscard): `$c::m()` over `$c = 'self'` looks the`` |
|       - | 2812 | `					 * name up as a class, and php finds none. */` |
|  124418 | 2813 | `					if( !pInstr->bDiscard ){` |
|  121608 | 2814 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|   63609 | 2815 | `					}else if( nCls == 4 && SyMemcmp(zCls,"self",4) == 0 ){` |
|       - | 2816 | `						/* In a trait method, self:: resolves to the USING class */` |
|    1743 | 2817 | `						pClass = PH7_VmPeekSelfClass(&(*pVm));` |
|    1743 | 2818 | `						bForwardingCall = 1;` |
|    1743 | 2819 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|    1946 | 2820 | `					}else if( nCls == 6 && SyMemcmp(zCls,"static",6) == 0 ){` |
|     825 | 2821 | `						pClass = PH7_VmPeekTopClass(&(*pVm));` |
|     825 | 2822 | `						bForwardingCall = 1;` |
|     825 | 2823 | `						pForwardLsb = pClass;` |
|     667 | 2824 | `					}else if( nCls == 6 && SyMemcmp(zCls,"parent",6) == 0 ){` |
|     257 | 2825 | `						pClass = PH7_VmResolveParentClass(&(*pVm));` |
|     257 | 2826 | `						bForwardingCall = 1;` |
|     257 | 2827 | `						pForwardLsb = PH7_VmPeekTopClass(&(*pVm));` |
|     131 | 2828 | `					}else{` |
|     ! 0 | 2829 | `						pClass = PH7_VmExtractClass(&(*pVm),zCls,nCls,FALSE,0);` |
|       - | 2830 | `					}` |
|   62199 | 2831 | `				}` |
|       - | 2832 | `			}` |
|  124494 | 2833 | `			if( pClass == 0 ){` |
|       - | 2834 | `				/* Undefined class: php throws a catchable Error */` |
|       - | 2835 | `				SyBlob sErrM;` |
|       - | 2836 | `				sxi32 rcErr;` |
|      84 | 2837 | `				if( PH7_VmClassLookupRaised(&(*pVm),nMbBrc,pMbRes) ){` |
|       - | 2838 | `					/* ...unless an autoloader raised: land THAT instead of reporting the` |
|       - | 2839 | `					 * class missing on top of it. */` |
|      12 | 2840 | `					sxi32 rcAutoM = pVm->nBoundaryRc;` |
|      12 | 2841 | `					pVm->nBoundaryRc = 0;` |
|      12 | 2842 | `					if( !pInstr->p3 ){` |
|      10 | 2843 | `						VmPopOperand(&pTos,1);` |
|       4 | 2844 | `					}` |
|      12 | 2845 | `					PH7_MemObjRelease(pTos);` |
|      12 | 2846 | `					MemObjSetType(pTos,MEMOBJ_NULL);` |
|      12 | 2847 | `					pTos->nIdx = SXU32_HIGH;` |
|      12 | 2848 | `					if( rcAutoM == PH7_ABORT ){` |
|     ! 0 | 2849 | `						VM_EXIT_ABORT;` |
|       - | 2850 | `					}` |
|      12 | 2851 | `					rc = PH7_EXCEPTION;` |
|      12 | 2852 | `					PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2853 | `				}` |
|      74 | 2854 | `				SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       - | 2855 | `				{` |
|       - | 2856 | `					/* A WRITTEN keyword with no class behind it is php's scope refusal,` |
|       - | 2857 | ``					 * and `self::class` (the bareword `class`) is its own sentence. */`` |
|      74 | 2858 | `					const char *zKwWhy = 0;` |
|       - | 2859 | `					char zKwBuf[96];` |
|      74 | 2860 | `					if( pInstr->bDiscard ){` |
|      37 | 2861 | `						int bClassName = pInstr->p3 == 0 && pInstr->iP1 != 2` |
|      22 | 2862 | `							&& pInstr->iP2 != PH7_MEMBER_METHOD` |
|      19 | 2863 | `							&& (pTos->iFlags & MEMOBJ_STRING)` |
|      16 | 2864 | `							&& SyBlobLength(&pTos->sBlob) == sizeof("class")-1` |
|      39 | 2865 | `							&& SyStrnicmp((const char *)SyBlobData(&pTos->sBlob),"class",` |
|       5 | 2866 | `								sizeof("class")-1) == 0;` |
|      43 | 2867 | `						zKwWhy = PH7_VmScopeKeywordRefusal(&(*pVm),` |
|      28 | 2868 | `							(const char *)SyBlobData(&pNos->sBlob),SyBlobLength(&pNos->sBlob),` |
|      14 | 2869 | `							bClassName,zKwBuf,(int)sizeof(zKwBuf));` |
|      14 | 2870 | `					}` |
|      74 | 2871 | `					if( zKwWhy ){` |
|      29 | 2872 | `						SyBlobAppend(&sErrM,zKwWhy,(sxu32)SyStrlen(zKwWhy));` |
|      15 | 2873 | `					}else{` |
|      46 | 2874 | `						SyBlobFormat(&sErrM,"Class \"%.*s\" not found",` |
|      42 | 2875 | `							SyBlobLength(&pNos->sBlob),(const char *)SyBlobData(&pNos->sBlob));` |
|       - | 2876 | `					}` |
|       - | 2877 | `				}` |
|      74 | 2878 | `				if( !pInstr->p3 ){` |
|      56 | 2879 | `					VmPopOperand(&pTos,1);` |
|      27 | 2880 | `				}` |
|      74 | 2881 | `				PH7_MemObjRelease(pTos);` |
|      74 | 2882 | `				pTos->nIdx = SXU32_HIGH;` |
|     109 | 2883 | `				rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      35 | 2884 | `					SyBlobLength(&sErrM));` |
|      74 | 2885 | `				SyBlobRelease(&sErrM);` |
|      74 | 2886 | `				if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      74 | 2887 | `				rc = rcErr;` |
|      86 | 2888 | `				PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 2889 | `			}else{` |
|       - | 2890 | `				/* The static twin of the name rule at the top of this handler, and it` |
|       - | 2891 | `				 * runs HERE rather than up there because php resolves the CLASS first:` |
|       - | 2892 | ``				 * `NoSuchClass::${$arr}` is the class refusal alone, with no coercion`` |
|       - | 2893 | ``				 * and no `Array to string conversion` behind it, while the same name on`` |
|       - | 2894 | ``				 * a class that exists warns and then reports `Access to undeclared`` |
|       - | 2895 | ``				 * static property C::$Array`. A `::` METHOD name is refused the same way`` |
|       - | 2896 | `				 * an instance one is -- once the class is known. */` |
|  124414 | 2897 | `				if( !pInstr->p3 ){` |
|   22410 | 2898 | `					if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|   18698 | 2899 | `						if( (pTos->iFlags & MEMOBJ_STRING) == 0 ){` |
|       - | 2900 | `							sxi32 rcMn;` |
|       3 | 2901 | `							VmPopOperand(&pTos,1);` |
|       3 | 2902 | `							PH7_MemObjRelease(pTos);` |
|       3 | 2903 | `							MemObjSetType(pTos,MEMOBJ_NULL);` |
|       3 | 2904 | `							pTos->nIdx = SXU32_HIGH;` |
|       3 | 2905 | `							if( pThis ){` |
|     ! 0 | 2906 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2907 | `								pThis = 0;` |
|     ! 0 | 2908 | `							}` |
|       3 | 2909 | `							rcMn = VmThrowFromVm(&(*pVm),"Error","Method name must be a string",` |
|       - | 2910 | `								sizeof("Method name must be a string")-1);` |
|       3 | 2911 | `							if( rcMn == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       3 | 2912 | `							rc = rcMn;` |
|       3 | 2913 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2914 | `						}` |
|    9343 | 2915 | `					}else{` |
|    3717 | 2916 | `						sxi32 rcNm = PH7_MemObjToStringUV(pTos);` |
|    3717 | 2917 | `						if( rcNm != SXRET_OK ){` |
|     ! 0 | 2918 | `							VmPopOperand(&pTos,1);` |
|     ! 0 | 2919 | `							PH7_MemObjRelease(pTos);` |
|     ! 0 | 2920 | `							MemObjSetType(pTos,MEMOBJ_NULL);` |
|     ! 0 | 2921 | `							pTos->nIdx = SXU32_HIGH;` |
|     ! 0 | 2922 | `							if( pThis ){` |
|     ! 0 | 2923 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2924 | `								pThis = 0;` |
|     ! 0 | 2925 | `							}` |
|     ! 0 | 2926 | `							if( rcNm == PH7_ABORT ){ VM_EXIT_ABORT; }` |
|     ! 0 | 2927 | `							rc = rcNm;` |
|     ! 0 | 2928 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2929 | `						}` |
|       - | 2930 | `					}` |
|       - | 2931 | `					/* The coercion rewrote the slot the name was read from. */` |
|   22408 | 2932 | `					SyStringInitFromBuf(&sName,(const char *)SyBlobData(&pTos->sBlob),` |
|       - | 2933 | `						SyBlobLength(&pTos->sBlob));` |
|   11194 | 2934 | `				}` |
|  124412 | 2935 | `				if( pInstr->iP2 == PH7_MEMBER_METHOD ){` |
|       - | 2936 | `					/* Method call */` |
|   18696 | 2937 | `					ph7_class_method *pMeth = 0;` |
|       - | 2938 | ``					/* A LITERAL `X::__construct()` (any case) is not a method lookup in php:`` |
|       - | 2939 | `					 * the compiler drops the name and the call asks for the class's` |
|       - | 2940 | `					 * constructor itself. So no visibility screen and no __callStatic` |
|       - | 2941 | `					 * fallback -- a class without one is "Cannot call constructor", and the` |
|       - | 2942 | ``					 * only refusal is a PRIVATE constructor called with a `$this` whose class`` |
|       - | 2943 | `					 * is not the constructor's own. That compares the OBJECT's class, not the` |
|       - | 2944 | ``					 * calling scope: `self::__construct()` in A's own method refuses on a`` |
|       - | 2945 | `					 * subclass instance, and a protected one is never refused here. Anything` |
|       - | 2946 | `					 * else falls to the non-static rule below. A dynamic name is an ordinary` |
|       - | 2947 | `					 * lookup and keeps the ordinary messages; the compiler marks the literal` |
|       - | 2948 | `					 * (VmInstr::bRefSrc). */` |
|   18696 | 2949 | `					int bCtorLiteral = pInstr->bRefSrc != 0;` |
|   18696 | 2950 | `					if( sName.nByte > 0 ){` |
|       - | 2951 | `						/* An INTERFACE's methods are looked up too: they are all abstract,` |
|       - | 2952 | `						 * so the arm below reports php's "Cannot call abstract method` |
|       - | 2953 | `						 * I::m()" rather than claiming the name does not exist. */` |
|   18696 | 2954 | `						pMeth = PH7_ClassExtractMethod(pClass,sName.zString,sName.nByte);` |
|    9338 | 2955 | `					}` |
|   18696 | 2956 | `					if( bCtorLiteral ){` |
|      71 | 2957 | `						ph7_class_instance *pRawThis = PH7_VmCallerThis(&(*pVm));` |
|     125 | 2958 | `						int bCtorRefused = pMeth == 0 \|\| ( pRawThis` |
|      58 | 2959 | `							&& pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|      37 | 2960 | `							&& pRawThis->pClass != PH7_VmMethodScopeName(&(*pVm),pClass,pMeth) );` |
|      71 | 2961 | `						if( bCtorRefused ){` |
|       - | 2962 | `							SyBlob sErrM;` |
|       - | 2963 | `							sxi32 rcErr;` |
|      21 | 2964 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      21 | 2965 | `							if( pMeth == 0 ){` |
|       7 | 2966 | `								SyBlobAppend(&sErrM,"Cannot call constructor",sizeof("Cannot call constructor")-1);` |
|       4 | 2967 | `							}else{` |
|      15 | 2968 | `								SyBlobFormat(&sErrM,"Cannot call private %z::__construct()",&pClass->sDisp);` |
|       - | 2969 | `							}` |
|      21 | 2970 | `							if( !pInstr->p3 ){` |
|      21 | 2971 | `								VmPopOperand(&pTos,1);` |
|      10 | 2972 | `							}` |
|      21 | 2973 | `							PH7_MemObjRelease(pTos);` |
|      21 | 2974 | `							pTos->nIdx = SXU32_HIGH;` |
|      21 | 2975 | `							if( pThis ){` |
|     ! 0 | 2976 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 2977 | `								pThis = 0;` |
|     ! 0 | 2978 | `							}` |
|      31 | 2979 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      10 | 2980 | `								SyBlobLength(&sErrM));` |
|      21 | 2981 | `							SyBlobRelease(&sErrM);` |
|      21 | 2982 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      21 | 2983 | `							rc = rcErr;` |
|      21 | 2984 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 2985 | `						}` |
|      24 | 2986 | `					}` |
|       - | 2987 | `					/* The constructor request never asks whether the constructor is` |
|       - | 2988 | `					 * abstract: php runs an abstract (or interface) one as its empty body. */` |
|   18676 | 2989 | `					if( pMeth == 0 \|\| ((pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) && !bCtorLiteral) ){` |
|     104 | 2990 | `						if( pMeth ){` |
|       - | 2991 | `							SyBlob sErrM;` |
|       - | 2992 | `							sxi32 rcErr;` |
|       9 | 2993 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|       9 | 2994 | `							SyBlobFormat(&sErrM,"Cannot call abstract method %z::%z()",` |
|       4 | 2995 | `								&pClass->sDisp,&sName);` |
|       9 | 2996 | `							if( !pInstr->p3 ){` |
|       9 | 2997 | `								VmPopOperand(&pTos,1);` |
|       4 | 2998 | `							}` |
|       9 | 2999 | `							PH7_MemObjRelease(pTos);` |
|       9 | 3000 | `							pTos->nIdx = SXU32_HIGH;` |
|       - | 3001 | ``							/* The `$obj::m()` form took a reference on the receiver at the`` |
|       - | 3002 | `							 * top of this branch; every exit has to give it back, and only` |
|       - | 3003 | `							 * the fall-through at the end of the arm used to. */` |
|       9 | 3004 | `							if( pThis ){` |
|     ! 0 | 3005 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3006 | `								pThis = 0;` |
|     ! 0 | 3007 | `							}` |
|      13 | 3008 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       4 | 3009 | `								SyBlobLength(&sErrM));` |
|       9 | 3010 | `							SyBlobRelease(&sErrM);` |
|       9 | 3011 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|       9 | 3012 | `							rc = rcErr;` |
|       9 | 3013 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|     ! 0 | 3014 | `						}else{` |
|      96 | 3015 | `							ph7_class_instance *pMagicThis = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|      96 | 3016 | `							ph7_class_method *pCallStaticMagic = pMagicThis` |
|      16 | 3017 | `								? PH7_ClassExtractMethod(pMagicThis->pClass,"__call",sizeof("__call")-1)` |
|      84 | 3018 | `								: PH7_ClassExtractMethod(pClass,"__callStatic",sizeof("__callStatic")-1);` |
|      96 | 3019 | `							if( pCallStaticMagic ){` |
|       - | 3020 | `								/* php: C::missing(...) dispatches __callStatic($name,$args)` |
|       - | 3021 | `								 * through the packing body (see the instance twin) — or` |
|       - | 3022 | `								 * __call($name,$args) on the calling frame's own $this when` |
|       - | 3023 | `								 * that receiver fits C (VmStaticCallMagicThis). */` |
|     127 | 3024 | `								VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pMagicThis,` |
|      41 | 3025 | `									pMagicThis ? pMagicThis->pClass : pClass,&sName);` |
|      86 | 3026 | `								if( pPend == 0 ){` |
|     ! 0 | 3027 | `									VM_EXIT_ABORT;` |
|       - | 3028 | `								}` |
|       - | 3029 | ``								/* `parent::missing()` forwards the caller's called class`` |
|       - | 3030 | `								 * into __callStatic, as the named-method twin below does. */` |
|      86 | 3031 | `								if( pMagicThis == 0 && bForwardingCall ){` |
|      31 | 3032 | `									pPend->pLsb = pForwardLsb;` |
|      15 | 3033 | `								}` |
|      86 | 3034 | `								if( !pInstr->p3 ){` |
|      86 | 3035 | `									VmPopOperand(&pTos,1);` |
|      41 | 3036 | `								}` |
|      86 | 3037 | `								PH7_MemObjRelease(pTos);` |
|      86 | 3038 | `								if( pThis ){` |
|     ! 0 | 3039 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3040 | `									pThis = 0;` |
|     ! 0 | 3041 | `								}` |
|      86 | 3042 | `								pTos->x.pOther = pPend;` |
|      86 | 3043 | `								pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|      86 | 3044 | `								pTos->nIdx = SXU32_HIGH;` |
|      86 | 3045 | `								VM_EXIT_BREAK;` |
|       - | 3046 | `							}` |
|       - | 3047 | `							{` |
|       - | 3048 | `								/* php: the STATIC form reports the same "Call to undefined` |
|       - | 3049 | `								 * method C::m()" as the instance one. */` |
|       - | 3050 | `								SyBlob sErrM;` |
|       - | 3051 | `								sxi32 rcErr;` |
|      11 | 3052 | `								SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      11 | 3053 | `								SyBlobFormat(&sErrM,"Call to undefined method %z::%z()",` |
|       5 | 3054 | `									&pClass->sDisp,&sName);` |
|      11 | 3055 | `								if( !pInstr->p3 ){` |
|      11 | 3056 | `									VmPopOperand(&pTos,1);` |
|       5 | 3057 | `								}` |
|      11 | 3058 | `								PH7_MemObjRelease(pTos);` |
|      11 | 3059 | `								pTos->nIdx = SXU32_HIGH;` |
|      11 | 3060 | `								if( pThis ){` |
|     ! 0 | 3061 | `									PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3062 | `									pThis = 0;` |
|     ! 0 | 3063 | `								}` |
|      16 | 3064 | `								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       5 | 3065 | `									SyBlobLength(&sErrM));` |
|      11 | 3066 | `								SyBlobRelease(&sErrM);` |
|      11 | 3067 | `								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      11 | 3068 | `								rc = rcErr;` |
|      11 | 3069 | `								PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 3070 | `							}` |
|       - | 3071 | `						}` |
|       - | 3072 | `						/* Pop the method name from the stack */` |
|     ! 0 | 3073 | `						if( !pInstr->p3 ){` |
|     ! 0 | 3074 | `							VmPopOperand(&pTos,1);` |
|       - | 3075 | `						}` |
|     ! 0 | 3076 | `						PH7_MemObjRelease(pTos);` |
|     ! 0 | 3077 | `					}else{` |
|       - | 3078 | `						/* Inaccessible from this scope: php routes through the same fallback a` |
|       - | 3079 | ``						 * MISSING name takes — __call on a compatible `$this`, else`` |
|       - | 3080 | ``						 * __callStatic — which is why `C::privateStatic()` runs a catch-all`` |
|       - | 3081 | `						 * instead of the "Call to private method" Error OP_CALL would raise` |
|       - | 3082 | `						 * below. */` |
|   18576 | 3083 | `						ph7_class_method *pDeniedStatic = 0;` |
|   18576 | 3084 | `						ph7_class_instance *pDeniedThis = 0;` |
|   18576 | 3085 | `						int bDeniedStatic = 0;` |
|   18576 | 3086 | `						if( pMeth->iProtection != PH7_CLASS_PROT_PUBLIC && !bCtorLiteral ){` |
|       - | 3087 | `							/* The OWNING class decides, as in the instance twin above: a` |
|       - | 3088 | `							 * trait method's rules belong to the class that composed it. */` |
|      51 | 3089 | `							ph7_class *pOwnerCls = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|      51 | 3090 | `							if( !PH7_VmClassMemberAccess(&(*pVm),pOwnerCls,&sName,pMeth->iProtection,FALSE) ){` |
|      27 | 3091 | `								pDeniedThis = PH7_VmStaticFallbackThis(&(*pVm),pClass);` |
|      27 | 3092 | `								pDeniedStatic = pDeniedThis` |
|       4 | 3093 | `									? PH7_ClassExtractMethod(pDeniedThis->pClass,"__call",` |
|       - | 3094 | `										sizeof("__call")-1)` |
|      24 | 3095 | `									: PH7_ClassExtractMethod(pClass,"__callStatic",` |
|       - | 3096 | `										sizeof("__callStatic")-1);` |
|      27 | 3097 | `								bDeniedStatic = pDeniedStatic == 0;` |
|      13 | 3098 | `							}` |
|      25 | 3099 | `						}` |
|   18576 | 3100 | `						if( bDeniedStatic ){` |
|       - | 3101 | `							/* No catch-all answers for it: php's visibility Error, raised at` |
|       - | 3102 | `							 * the resolution like the instance twin above. OP_CALL's own` |
|       - | 3103 | `							 * screen re-derives the method from the FUNCTION's name against` |
|       - | 3104 | `							 * its declaring class, which cannot see a trait adaptation's` |
|       - | 3105 | ``							 * composed protection — `sHi as private sPriv` ran from global`` |
|       - | 3106 | `							 * scope in silence. */` |
|       - | 3107 | `							SyBlob sErrM;` |
|       - | 3108 | `							sxi32 rcErr;` |
|      13 | 3109 | `							ph7_class *pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|      13 | 3110 | `							ph7_class *pScope = PH7_VmCallerScope(&(*pVm));` |
|      19 | 3111 | `							const char *zVis = pMeth->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       6 | 3112 | `								? "private" : "protected";` |
|      13 | 3113 | `							SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      13 | 3114 | `							if( pScope ){` |
|       5 | 3115 | `								SyBlobFormat(&sErrM,"Call to %s method %z::%z() from scope %z",` |
|       2 | 3116 | `									zVis,&pOwner->sDisp,&sName,&pScope->sDisp);` |
|       3 | 3117 | `							}else{` |
|       9 | 3118 | `								SyBlobFormat(&sErrM,"Call to %s method %z::%z() from global scope",` |
|       4 | 3119 | `									zVis,&pOwner->sDisp,&sName);` |
|       - | 3120 | `							}` |
|      13 | 3121 | `							if( !pInstr->p3 ){` |
|      13 | 3122 | `								VmPopOperand(&pTos,1);` |
|       6 | 3123 | `							}` |
|      13 | 3124 | `							PH7_MemObjRelease(pTos);` |
|      13 | 3125 | `							pTos->nIdx = SXU32_HIGH;` |
|      13 | 3126 | `							if( pThis ){` |
|     ! 0 | 3127 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3128 | `								pThis = 0;` |
|     ! 0 | 3129 | `							}` |
|      19 | 3130 | `							rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|       6 | 3131 | `								SyBlobLength(&sErrM));` |
|      13 | 3132 | `							SyBlobRelease(&sErrM);` |
|      13 | 3133 | `							if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      13 | 3134 | `							rc = rcErr;` |
|      13 | 3135 | `							PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 3136 | `						}` |
|   18564 | 3137 | `						if( pDeniedStatic ){` |
|       - | 3138 | `							/* The packing body takes no receiver slot: drop the method-name` |
|       - | 3139 | `							 * slot so the MARK lands on the receiver slot, exactly as the` |
|       - | 3140 | `							 * missing-method twin above does (leaving the class name in place` |
|       - | 3141 | `							 * would pack it as the first $args entry). */` |
|      22 | 3142 | `							VmMagicCall *pPend = VmMagicCallNew(&(*pVm),pDeniedThis,` |
|       7 | 3143 | `								pDeniedThis ? pDeniedThis->pClass : pClass,&sName);` |
|      15 | 3144 | `							if( pPend == 0 ){` |
|     ! 0 | 3145 | `								VM_EXIT_ABORT;` |
|       - | 3146 | `							}` |
|      15 | 3147 | `							if( pDeniedThis == 0 && bForwardingCall ){` |
|       7 | 3148 | `								pPend->pLsb = pForwardLsb;` |
|       3 | 3149 | `							}` |
|      15 | 3150 | `							if( !pInstr->p3 ){` |
|      15 | 3151 | `								VmPopOperand(&pTos,1);` |
|       7 | 3152 | `							}` |
|      15 | 3153 | `							PH7_MemObjRelease(pTos);` |
|      15 | 3154 | `							if( pThis ){` |
|     ! 0 | 3155 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3156 | `								pThis = 0;` |
|     ! 0 | 3157 | `							}` |
|      15 | 3158 | `							pTos->x.pOther = pPend;` |
|      15 | 3159 | `							pTos->iFlags = MEMOBJ_NULL\|MEMOBJ_AUX_MAGICCALL;` |
|      15 | 3160 | `							pTos->nIdx = SXU32_HIGH;` |
|      15 | 3161 | `							VM_EXIT_BREAK;` |
|       - | 3162 | `						}` |
|       - | 3163 | ``						/* php refuses a NON-STATIC method named through `::` unless the`` |
|       - | 3164 | ``						 * CALLING frame holds a `$this` the class accepts: that is what`` |
|       - | 3165 | ``						 * makes `self::`/`parent::`/`static::` and `C::m()` from inside a`` |
|       - | 3166 | `						 * C method work, and every other spelling — from global scope,` |
|       - | 3167 | `` 						 * from a static method, from an unrelated class, and `$obj::m()` `` |
|       - | 3168 | ``						 * — an Error. PHL ran the body instead, with `$this` unset or`` |
|       - | 3169 | ``						 * (for the object form) bound to the object the `::` was written`` |
|       - | 3170 | `						 * on, which php never uses: it takes the CALLER's. The message` |
|       - | 3171 | ``						 * names the DECLARING class, so `D::m()` reports B::m(). */`` |
|   18550 | 3172 | `						if( (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|     202 | 3173 | `							ph7_class_instance *pCallerThis = PH7_VmCallerThisFor(&(*pVm),pClass);` |
|     202 | 3174 | `							if( pCallerThis == 0 ){` |
|       - | 3175 | `								SyBlob sErrM;` |
|       - | 3176 | `								sxi32 rcErr;` |
|      25 | 3177 | `								ph7_class *pOwner = PH7_VmMethodScopeName(&(*pVm),pClass,pMeth);` |
|      25 | 3178 | `								SyBlobInit(&sErrM,&pVm->sAllocator);` |
|      25 | 3179 | `								SyBlobFormat(&sErrM,"Non-static method %z::%z() cannot be called statically",` |
|      12 | 3180 | `									&pOwner->sDisp,&pMeth->sFunc.sName);` |
|      25 | 3181 | `								if( !pInstr->p3 ){` |
|      25 | 3182 | `									VmPopOperand(&pTos,1);` |
|      12 | 3183 | `								}` |
|      25 | 3184 | `								PH7_MemObjRelease(pTos);` |
|      25 | 3185 | `								pTos->nIdx = SXU32_HIGH;` |
|      25 | 3186 | `								if( pThis ){` |
|       3 | 3187 | `									PH7_ClassInstanceUnref(pThis);` |
|       3 | 3188 | `									pThis = 0;` |
|       1 | 3189 | `								}` |
|      37 | 3190 | `								rcErr = VmThrowFromVm(&(*pVm),"Error",(const char *)SyBlobData(&sErrM),` |
|      12 | 3191 | `									SyBlobLength(&sErrM));` |
|      25 | 3192 | `								SyBlobRelease(&sErrM);` |
|      25 | 3193 | `								if( rcErr == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      25 | 3194 | `								rc = rcErr;` |
|      25 | 3195 | `								PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 3196 | `							}` |
|       - | 3197 | `` 							/* The receiver is that `$this` — never the object the `::` `` |
|       - | 3198 | `							 * was written on, and never nothing. Naming it here is also` |
|       - | 3199 | `							 * what makes the call work inside a CLOSURE declared in a` |
|       - | 3200 | ``							 * method: php gives such a closure a `$this`, PHL carries it`` |
|       - | 3201 | `							 * as a frame variable rather than on the frame itself, and` |
|       - | 3202 | ``							 * OP_CALL reads only the frame, so `self::m()` in a closure`` |
|       - | 3203 | `							 * used to run with no receiver at all. */` |
|     178 | 3204 | `							PH7_MemObjRelease(pNos);` |
|     178 | 3205 | `							pNos->x.pOther = pCallerThis;` |
|     178 | 3206 | `							MemObjSetType(pNos,MEMOBJ_OBJ);` |
|     178 | 3207 | `							pCallerThis->iRef++;` |
|     178 | 3208 | `							pNos->nIdx = SXU32_HIGH;` |
|      87 | 3209 | `						}` |
|       - | 3210 | `						/* Push method name on the stack, MARKED as already screened (see the` |
|       - | 3211 | `						 * instance twin: OP_CALL cannot re-derive a composed protection). */` |
|   18526 | 3212 | `						PH7_MemObjRelease(pTos);` |
|   18526 | 3213 | `						SyBlobAppend(&pTos->sBlob,SyStringData(&pMeth->sVmName),SyStringLength(&pMeth->sVmName));` |
|   18526 | 3214 | `						MemObjSetType(pTos,MEMOBJ_STRING);` |
|   18526 | 3215 | `						pTos->iFlags \|= MEMOBJ_AUX_MEMBERCALL;` |
|       - | 3216 | `					}` |
|   18526 | 3217 | `					pTos->nIdx = SXU32_HIGH;` |
|       - | 3218 | `					/* Forwarding call (self::/parent::/static::): overwrite the receiver` |
|       - | 3219 | `					 * slot (pNos, one below the method name) with the live LSB class name` |
|       - | 3220 | ``					 * so OP_CALL pushes THAT onto aSelf — preserving `static::` inside the`` |
|       - | 3221 | `					 * callee. Without this the literal keyword falls through to the` |
|       - | 3222 | `					 * callee's declaring class. Only when an LSB class is actually in` |
|       - | 3223 | `					 * scope (a static call from global scope has none). */` |
|   18526 | 3224 | `					if( bForwardingCall && pForwardLsb && (pNos->iFlags & MEMOBJ_STRING) ){` |
|     180 | 3225 | `						SyBlobReset(&pNos->sBlob);` |
|     180 | 3226 | `						SyBlobAppend(&pNos->sBlob,pForwardLsb->sName.zString,pForwardLsb->sName.nByte);` |
|      89 | 3227 | `					}` |
|    9258 | 3228 | `				}else{` |
|       - | 3229 | `					/* Attribute access */` |
|  105721 | 3230 | `					ph7_class_attr *pAttr = 0;` |
|  105721 | 3231 | `					if( pInstr->iP2 == PH7_MEMBER_UNSET ){` |
|       - | 3232 | `						/* unset(C::$x): php refuses it, unconditionally and before any` |
|       - | 3233 | `						 * lookup -- an undeclared name, a private one and one reached` |
|       - | 3234 | `						 * through a subclass all read the same, and the class named is` |
|       - | 3235 | ``						 * whatever the `::` resolved to (so `parent::$b` says Base and`` |
|       - | 3236 | ``						 * `Child::$b` says Child even when Base declares it). Only the`` |
|       - | 3237 | `						 * class NAME is resolved first, so an unknown one still answers` |
|       - | 3238 | ``						 * `Class "X" not found`.`` |
|       - | 3239 | `						 *` |
|       - | 3240 | `						 * The refusal is php's CATCHABLE Error; PHL reported it uncaught` |
|       - | 3241 | ``						 * and ABORTED, so `try { unset(C::$s); } catch (Throwable)` never`` |
|       - | 3242 | `						 * ran its catch and everything after the try was dropped with exit` |
|       - | 3243 | `						 * status 0. Parked on the boundary rail like the other refusals in` |
|       - | 3244 | `						 * this arm, with the op completing as a NULL that carries no slot` |
|       - | 3245 | `						 * index -- the trailing generic unset() then has nothing to clear,` |
|       - | 3246 | `						 * which is what stops it silently NULLing (and de-typing) the` |
|       - | 3247 | `						 * shared static slot on the way out.` |
|       - | 3248 | `						 *` |
|       - | 3249 | `						 * The name is the DISPLAY name: an anonymous class's identity` |
|       - | 3250 | ``						 * carries php's NUL-separated `class@anonymous\0file:line$hash`,`` |
|       - | 3251 | `						 * and formatting that through a C string truncated the message at` |
|       - | 3252 | ``						 * the NUL -- it lost `::$x` entirely. */`` |
|       - | 3253 | `						SyBlob sErrUn;` |
|      40 | 3254 | `						SyBlobInit(&sErrUn,&pVm->sAllocator);` |
|      40 | 3255 | `						SyBlobFormat(&sErrUn,"Attempt to unset static property %z::$%z",` |
|      19 | 3256 | `							&pClass->sDisp,&sName);` |
|      40 | 3257 | `						VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",` |
|       - | 3258 | `							sizeof("Error")-1,&sErrUn));` |
|      40 | 3259 | `						if( !pInstr->p3 ){` |
|       7 | 3260 | `							VmPopOperand(&pTos,1);` |
|       3 | 3261 | `						}` |
|      40 | 3262 | `						PH7_MemObjRelease(pTos);` |
|      40 | 3263 | `						MemObjSetType(pTos,MEMOBJ_NULL);` |
|      40 | 3264 | `						pTos->nIdx = SXU32_HIGH;` |
|      40 | 3265 | `						if( pThis ){` |
|       5 | 3266 | `							PH7_ClassInstanceUnref(pThis);` |
|       2 | 3267 | `						}` |
|      40 | 3268 | `						VM_EXIT_BREAK;` |
|       - | 3269 | `					}` |
|       - | 3270 | `					/* Check for special ::class pseudo-constant -- the BAREWORD form` |
|       - | 3271 | ``					 * only (p3==0, iP1!=2, as bConstForm below). `C::$class` is an`` |
|       - | 3272 | `					 * ordinary static property that happens to be named "class";` |
|       - | 3273 | `					 * matching it here answered the class name for every read. */` |
|  105678 | 3274 | `					if( pInstr->p3 == 0 && pInstr->iP1 != 2 &&` |
|    4217 | 3275 | `					    sName.nByte == sizeof("class")-1 &&` |
|    1040 | 3276 | `					    SyStrnicmp(sName.zString,"class",sizeof("class")-1) == 0 ){` |
|       - | 3277 | `						/* ::class returns the fully qualified class name */` |
|       - | 3278 | `						/* Pop the attribute name from the stack */` |
|     953 | 3279 | `						if( !pInstr->p3 ){` |
|     953 | 3280 | `							VmPopOperand(&pTos,1);` |
|     474 | 3281 | `						}` |
|     953 | 3282 | `						PH7_MemObjRelease(pTos);` |
|       - | 3283 | `						/* Load the class name */` |
|     953 | 3284 | `						ph7_value_string(pTos,pClass->sName.zString,(int)pClass->sName.nByte);` |
|     953 | 3285 | `						pTos->nIdx = SXU32_HIGH;` |
|     479 | 3286 | `					}else{` |
|       - | 3287 | `						/* Extract the target attribute. php keeps constants and` |
|       - | 3288 | `						 * (static) properties in separate namespaces; the source` |
|       - | 3289 | ``						 * form disambiguates (set by the `::` codegen, compile.c):`` |
|       - | 3290 | ``						 * a `$`-form is a STATIC PROPERTY (hAttr) — either a literal`` |
|       - | 3291 | ``						 * name folded into pInstr->p3 (`C::$s`) or a dynamic name on`` |
|       - | 3292 | ``						 * the stack marked iP1==2 (`C::$$x`, `C::${$e}`); a bareword`` |
|       - | 3293 | `						 * (p3==0, iP1==1) is a CONSTANT or enum case (hConst). This` |
|       - | 3294 | ``						 * is what lets `const C` and `public $C` coexist and resolve`` |
|       - | 3295 | `						 * to the right member. */` |
|       - | 3296 | `						/* php gives a class CONSTANT no silent-lookup mode at all: there is` |
|       - | 3297 | ``						 * no BP_VAR_IS fetch for one, so `empty(C::K)` and `C::K ?? $d` raise`` |
|       - | 3298 | `						 * the same Error a plain read does -- undefined OR inaccessible --` |
|       - | 3299 | `						 * where the same two shapes over a static PROPERTY answer quietly.` |
|       - | 3300 | `						 * PHL silenced both, so a typo'd or private class constant read` |
|       - | 3301 | ``						 * through `??` handed back the default. (isset() over one is a php`` |
|       - | 3302 | `						 * COMPILE error, so it never reaches this.) */` |
|  104735 | 3303 | `						int bConstForm = (pInstr->p3 == 0 && pInstr->iP1 != 2);` |
|  104735 | 3304 | `						if( sName.nByte > 0 ){` |
|  104735 | 3305 | `							pAttr = bConstForm` |
|    2744 | 3306 | `								? PH7_ClassExtractConstant(pClass,sName.zString,sName.nByte)` |
|  103358 | 3307 | `								: VmClassAttrWithShadow(pClass,sName.zString,sName.nByte);` |
|   52365 | 3308 | `						}` |
|  104730 | 3309 | `						if( pAttr && bConstForm && (pClass->iFlags & PH7_CLASS_TRAIT) != 0` |
|    1371 | 3310 | `						 && !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 3311 | `							/* A trait constant belongs to the classes that COMPOSE the trait` |
|       - | 3312 | ``							 * and to no one else: php refuses `T::K` outright, from inside a`` |
|       - | 3313 | `							 * trait method as readily as from outside. (PHP 8.2 introduced` |
|       - | 3314 | `							 * trait constants; this refusal came with them.) */` |
|       - | 3315 | `							SyBlob sErrTr;` |
|       3 | 3316 | `							SyBlobInit(&sErrTr,&pVm->sAllocator);` |
|       3 | 3317 | `							SyBlobFormat(&sErrTr,"Cannot access trait constant %z::%z directly",` |
|       1 | 3318 | `								&pClass->sDisp,&pAttr->sName);` |
|       3 | 3319 | `							VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",` |
|       - | 3320 | `								sizeof("Error")-1,&sErrTr));` |
|       3 | 3321 | `							pAttr = 0;` |
|       3 | 3322 | `							bStaticHidden = 1;` |
|       1 | 3323 | `						}` |
|  104730 | 3324 | `						if( pAttr && !bConstForm` |
|  103341 | 3325 | `						 && (pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT))` |
|   50986 | 3326 | `						     != PH7_CLASS_ATTR_STATIC ){` |
|       - | 3327 | ``							/* php's `::$name` reads the class's PROPERTY table -- which holds`` |
|       - | 3328 | `							 * the INSTANCE properties and the constants too -- and answers in` |
|       - | 3329 | `							 * a fixed order: visibility first, then static-ness. So a private` |
|       - | 3330 | ``							 * instance property is `Cannot access private property H::$pi`,`` |
|       - | 3331 | ``							 * a public one and a CONSTANT named with the `$` form are`` |
|       - | 3332 | ``							 * `Access to undeclared static property H::$inst`, and neither`` |
|       - | 3333 | `							 * ever yields a value: the static table simply has no such row.` |
|       - | 3334 | `							 * PHL matched only the constant case; an instance property` |
|       - | 3335 | ``							 * reached through `::` printed PH7's own uncatchable "Access to a`` |
|       - | 3336 | `							 * non-static class attribute ... PH7 is loading NULL" and CARRIED` |
|       - | 3337 | ``							 * ON -- reading null, and letting `H::$inst = 'w'` report success`` |
|       - | 3338 | `							 * for a write php refuses. Fold it into the not-found arm below,` |
|       - | 3339 | `							 * raising the visibility refusal first when that is what php` |
|       - | 3340 | `							 * answers. */` |
|       8 | 3341 | `							if( !VmMemberCtxIsLookup(pInstr->iP2)` |
|       9 | 3342 | `							 && !PH7_VmClassAttrAccess(&(*pVm),pClass,pAttr,FALSE) ){` |
|       - | 3343 | `								SyBlob sErrVis;` |
|       3 | 3344 | `								SyBlobInit(&sErrVis,&pVm->sAllocator);` |
|       2 | 3345 | `								SyBlobFormat(&sErrVis,"Cannot access %s property %z::$%z",` |
|       2 | 3346 | `									pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ? "private" : "protected",` |
|       1 | 3347 | `									&pClass->sDisp,&pAttr->sName);` |
|       3 | 3348 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",` |
|       - | 3349 | `									sizeof("Error")-1,&sErrVis));` |
|       3 | 3350 | `								bStaticHidden = 1;` |
|       1 | 3351 | `							}` |
|       9 | 3352 | `							pAttr = 0;` |
|       4 | 3353 | `						}` |
|  104735 | 3354 | `						if( pAttr == 0 && !bStaticHidden ){` |
|       - | 3355 | `							/* No such member. php raises a catchable Error whose wording` |
|       - | 3356 | `							 * depends on the ACCESS form (the same p3/iP1 signal used above):` |
|       - | 3357 | ``							 * a bareword `C::MISSING` (constant form) is "Undefined constant`` |
|       - | 3358 | ``							 * C::MISSING", a `$`-form `C::$missing` is "Access to undeclared`` |
|       - | 3359 | `							 * static property C::$missing". Instance magic (__get) is never` |
|       - | 3360 | `							 * consulted for statics (band A #3b). isset()/empty() context` |
|       - | 3361 | `							 * stays silently false. Parked on the boundary rail; the op` |
|       - | 3362 | `							 * completes benignly with NULL and the fetch-point router lands` |
|       - | 3363 | `							 * the throw. */` |
|      37 | 3364 | `							if( bConstForm \|\| !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 3365 | `								SyBlob sErrMsg;` |
|      33 | 3366 | `								SyBlobInit(&sErrMsg,&pVm->sAllocator);` |
|      33 | 3367 | `								if( bConstForm ){` |
|      16 | 3368 | `									SyBlobFormat(&sErrMsg,"Undefined constant %z::%z",` |
|       7 | 3369 | `										&pClass->sDisp,&sName);` |
|       9 | 3370 | `								}else{` |
|      19 | 3371 | `									SyBlobFormat(&sErrMsg,"Access to undeclared static property %z::$%z",` |
|       8 | 3372 | `										&pClass->sDisp,&sName);` |
|       - | 3373 | `								}` |
|      33 | 3374 | `								VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",sizeof("Error")-1,&sErrMsg));` |
|      15 | 3375 | `							}` |
|      17 | 3376 | `						}` |
|       - | 3377 | `						/* Pop the attribute name from the stack */` |
|  104735 | 3378 | `						if( !pInstr->p3 ){` |
|    2763 | 3379 | `							VmPopOperand(&pTos,1);` |
|    1379 | 3380 | `						}` |
|  104735 | 3381 | `						PH7_MemObjRelease(pTos);` |
|  104735 | 3382 | `						pTos->nIdx = SXU32_HIGH;` |
|  104735 | 3383 | `						if( pInstr->iP2 == PH7_MEMBER_REF_TARGET ){` |
|       - | 3384 | ``							/* `self::$s =& $x` / `C::$s =& $x`: stash the static property slot`` |
|       - | 3385 | `							 * for the following member-marked OP_STORE_REF (class-level, shared` |
|       - | 3386 | `							 * across instances — matches php). Skip the read machinery below. */` |
|      16 | 3387 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|      16 | 3388 | `							 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|      18 | 3389 | `							 && VmClassStaticDeferPending(pClass) ){` |
|       - | 3390 | `								/* Binding a reference TO a static touches the table, so it` |
|       - | 3391 | `								 * materializes first, like every other static access. */` |
|       3 | 3392 | `								sxi32 rcRt = PH7_VmMaterializeClassStatics(&(*pVm),pClass);` |
|       3 | 3393 | `								if( rcRt != SXRET_OK ){` |
|       3 | 3394 | `									pVm->pRefTargetStaticAttr = 0;` |
|       3 | 3395 | `									pVm->pRefTargetAttr = 0;` |
|       3 | 3396 | `									pVm->pRefTargetThis = 0;` |
|       3 | 3397 | `									if( pThis ){` |
|     ! 0 | 3398 | `										PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3399 | `									}` |
|       3 | 3400 | `									if( rcRt == PH7_ABORT ){` |
|     ! 0 | 3401 | `										VM_EXIT_ABORT;` |
|       - | 3402 | `									}` |
|       - | 3403 | `									{` |
|       - | 3404 | `										sxi32 iRpR;` |
|       3 | 3405 | `										if( VmRecordedResume(pVm,&iRpR,pState->pEntryFrame,aInstr) ){` |
|       7 | 3406 | `											PH7_RESUME_DRAIN()` |
|       3 | 3407 | `											pc = iRpR;` |
|       3 | 3408 | `											VM_EXIT_BREAK;` |
|       - | 3409 | `										}` |
|       - | 3410 | `									}` |
|     ! 0 | 3411 | `									VM_EXIT_EXCEPTION;` |
|       - | 3412 | `								}` |
|     ! 0 | 3413 | `							}` |
|      14 | 3414 | `							if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|      16 | 3415 | `							 && PH7_VmClassAttrAccess(&(*pVm),pClass,pAttr,FALSE) ){` |
|      16 | 3416 | `								pVm->pRefTargetStaticAttr = pAttr;` |
|      16 | 3417 | `								pVm->pRefTargetAttr = 0;` |
|      16 | 3418 | `								pVm->pRefTargetThis = 0;` |
|      16 | 3419 | `								pTos->nIdx = pAttr->nIdx;` |
|       9 | 3420 | `							}else{` |
|     ! 0 | 3421 | `								pVm->pRefTargetStaticAttr = 0;` |
|     ! 0 | 3422 | `								pVm->pRefTargetAttr = 0;` |
|     ! 0 | 3423 | `								pVm->pRefTargetThis = 0;` |
|       - | 3424 | `							}` |
|      16 | 3425 | `							if( pThis ){` |
|     ! 0 | 3426 | `								PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3427 | `							}` |
|      16 | 3428 | `							VM_EXIT_BREAK;` |
|       - | 3429 | `						}` |
|  104719 | 3430 | `						if( pAttr ){` |
|       - | 3431 | `							{` |
|       - | 3432 | `								ph7_value *pValue;` |
|       - | 3433 | `								/* php materializes the class's static table at the FIRST` |
|       - | 3434 | `								 * static-property access (any property, any context — read,` |
|       - | 3435 | `								 * write, even isset): a default whose evaluation was DEFERRED` |
|       - | 3436 | `								 * (it threw at the declaration) runs here, and a typed one that` |
|       - | 3437 | `								 * failed its check throws its catchable TypeError here. Class` |
|       - | 3438 | `								 * constants and static method calls do not trigger the` |
|       - | 3439 | `								 * materialization (php-exact). */` |
|  104676 | 3440 | `								if( (pAttr->iFlags & PH7_CLASS_ATTR_STATIC)` |
|  103312 | 3441 | `								 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0` |
|  101953 | 3442 | `								 && VmClassStaticDeferPending(pClass) ){` |
|      61 | 3443 | `									sxi32 rcD = PH7_VmMaterializeClassStatics(&(*pVm),pClass);` |
|      61 | 3444 | `									if( rcD != SXRET_OK ){` |
|      51 | 3445 | `										if( pThis ){` |
|     ! 0 | 3446 | `											PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3447 | `										}` |
|      51 | 3448 | `										if( rcD == PH7_ABORT ){` |
|     ! 0 | 3449 | `											VM_EXIT_ABORT;` |
|       - | 3450 | `										}` |
|       - | 3451 | `										{` |
|       - | 3452 | `											sxi32 iRpD;` |
|      51 | 3453 | `											if( VmRecordedResume(pVm,&iRpD,pState->pEntryFrame,aInstr) ){` |
|      84 | 3454 | `												PH7_RESUME_DRAIN()` |
|      40 | 3455 | `												pc = iRpD;` |
|      40 | 3456 | `												VM_EXIT_BREAK;` |
|       - | 3457 | `											}` |
|       - | 3458 | `										}` |
|      12 | 3459 | `										VM_EXIT_EXCEPTION;` |
|       - | 3460 | `									}` |
|       5 | 3461 | `								}` |
|       - | 3462 | `								/* Check if the access to the attribute is allowed */` |
|  104633 | 3463 | `								if( PH7_VmClassAttrAccess(&(*pVm),pClass,pAttr,FALSE) ){` |
|       - | 3464 | `									/* PHP 7.4+: uninitialized typed static read.` |
|       - | 3465 | `									 * Same LHS-of-store peek as the instance path. */` |
|  104604 | 3466 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0` |
|  102416 | 3467 | `									 && (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|  150233 | 3468 | `										SyHashEntry *pS = SyHashGet(&pVm->hTypedSlot,` |
|  100152 | 3469 | `											(const void *)&pAttr->nIdx,sizeof(sxu32));` |
|  100157 | 3470 | `										if( pS ){` |
|  100157 | 3471 | `											VmClassAttr *pV = (VmClassAttr *)pS->pUserData;` |
|  100157 | 3472 | `											if( pV && (pV->iState & VM_CLASS_ATTR_UNINIT) ){` |
|  100034 | 3473 | `												VmInstr *pNext = pInstr + 1;` |
|  100034 | 3474 | `												int bIsLhs = 0;` |
|  100034 | 3475 | `												if( pNext->iOp == PH7_OP_STORE && pNext->iP2 ){` |
|      11 | 3476 | `													bIsLhs = 1;` |
|       4 | 3477 | `												}` |
|  100034 | 3478 | `												if( pInstr->iP2 == PH7_MEMBER_LIST_TARGET ){` |
|       - | 3479 | `													/* Destructuring target ([S::$s] = [...]):` |
|       - | 3480 | `													 * initialized by the OP_LOAD_LIST store. */` |
|       3 | 3481 | `													bIsLhs = 1;` |
|       1 | 3482 | `												}` |
|       - | 3483 | ``												/* isset()/empty()/`??` ask whether the property`` |
|       - | 3484 | `												 * HAS a value and read an uninitialized one as` |
|       - | 3485 | `												 * "not set" -- a silent miss, not the Error a` |
|       - | 3486 | `												 * plain read raises. The instance path has had` |
|       - | 3487 | `												 * this since typed properties landed; the STATIC` |
|       - | 3488 | ``												 * one never did, so `isset(C::$n)` threw where php`` |
|       - | 3489 | ``												 * answers false. (`??=` is the NULLC_JMP branch`` |
|       - | 3490 | `												 * below -- it is a write base, not a lookup.) */` |
|  100034 | 3491 | `												if( VmMemberCtxIsLookup(pInstr->iP2) ){` |
|      13 | 3492 | `													bIsLhs = 1;` |
|       6 | 3493 | `												}` |
|       - | 3494 | `												/* And a WRITE base is not a read: same` |
|       - | 3495 | `												 * auto-initialize rule as the instance path. A` |
|       - | 3496 | ``												 * read-MODIFY-write (`C::$n++`, `.=`) is not one`` |
|       - | 3497 | `												 * of these -- it reads the property first, so` |
|       - | 3498 | `												 * php's Error stands. */` |
|  100034 | 3499 | `												if( (pInstr + 1)->iOp == PH7_OP_NULLC_JMP ){` |
|       3 | 3500 | `													bIsLhs = 1;` |
|  100033 | 3501 | `												}else if( VmMemberFetchIsRefSource(pInstr) ){` |
|       - | 3502 | `													/* A reference fetch has the instance path's own` |
|       - | 3503 | `													 * rule: NULL and bind when the type admits it,` |
|       - | 3504 | `													 * php's by-reference refusal when it does not. */` |
|       4 | 3505 | `													sxi32 rcRs = VmRefUninitTypedProperty(&(*pVm),pClass,pV,` |
|       1 | 3506 | `														(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx));` |
|       3 | 3507 | `													if( rcRs != SXRET_OK ){` |
|       3 | 3508 | `														VmBoundaryPark(&(*pVm),rcRs);` |
|       3 | 3509 | `														if( pThis ){` |
|     ! 0 | 3510 | `															PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3511 | `														}` |
|       3 | 3512 | `														VM_EXIT_BREAK;` |
|       - | 3513 | `													}` |
|     ! 0 | 3514 | `													bIsLhs = 1;` |
|  100030 | 3515 | `												}else if( VmMemberFetchForWrite(pInstr) ){` |
|       4 | 3516 | `													sxi32 rcAI = VmAutoInitArrayProperty(&(*pVm),pV,` |
|       1 | 3517 | `														(ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx));` |
|       3 | 3518 | `													if( rcAI != SXRET_OK ){` |
|     ! 0 | 3519 | `														VmBoundaryPark(&(*pVm),rcAI);` |
|     ! 0 | 3520 | `														if( pThis ){` |
|     ! 0 | 3521 | `															PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3522 | `														}` |
|     ! 0 | 3523 | `														VM_EXIT_BREAK;` |
|       - | 3524 | `													}` |
|       3 | 3525 | `													bIsLhs = 1;` |
|       1 | 3526 | `												}` |
|  100032 | 3527 | `												if( !bIsLhs ){` |
|  100004 | 3528 | `													sxi32 rcU = VmThrowUninitializedPropertyError(&(*pVm),pClass,pAttr);` |
|  100004 | 3529 | `													if( pThis ){` |
|     ! 0 | 3530 | `														PH7_ClassInstanceUnref(pThis);` |
|     ! 0 | 3531 | `													}` |
|  100004 | 3532 | `													if( rcU == PH7_ABORT ){` |
|     ! 0 | 3533 | `														VM_EXIT_ABORT;` |
|       - | 3534 | `													}` |
|       - | 3535 | `													{` |
|       - | 3536 | `														sxi32 iRp;` |
|  100004 | 3537 | `														if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|  200004 | 3538 | `															PH7_RESUME_DRAIN()` |
|  100004 | 3539 | `															pc = iRp;` |
|  100004 | 3540 | `															VM_EXIT_BREAK;` |
|       - | 3541 | `														}` |
|       - | 3542 | `													}` |
|     ! 0 | 3543 | `													VM_EXIT_EXCEPTION;` |
|       - | 3544 | `												}` |
|      13 | 3545 | `											}` |
|      74 | 3546 | `										}` |
|      74 | 3547 | `									}` |
|    4600 | 3548 | `									if( (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|    3663 | 3549 | `									 && SySetUsed(&pAttr->aAttrs) > 0 ){` |
|       - | 3550 | `										/* php 8.4 #[\Deprecated] on a class constant:` |
|       - | 3551 | `										 * every access re-warns. */` |
|      14 | 3552 | `										VmDeprecatedConstNotice(&(*pVm),pClass,pAttr);` |
|       6 | 3553 | `									}` |
|    4605 | 3554 | `									if( pAttr->nIdx == SXU32_HIGH ){` |
|       - | 3555 | `										/* Unmaterialized slot. Enum case: first access` |
|       - | 3556 | `										 * materializes ALL the singletons. Plain constant: its` |
|       - | 3557 | `										 * initializer hasn't run yet (mount-order-dependent` |
|       - | 3558 | `										 * cross-constant reference) — evaluate on demand. A` |
|       - | 3559 | `										 * raised TypeError/Error (backing mismatch, duplicate` |
|       - | 3560 | `										 * value, self-reference) parks on the boundary rail;` |
|       - | 3561 | `										 * the op completes benignly with NULL and the` |
|       - | 3562 | `										 * fetch-point router lands the throw. */` |
|       - | 3563 | `										sxi32 rcEnum;` |
|     829 | 3564 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE ){` |
|       - | 3565 | `											/* php: a DIRECT static access evaluates every case` |
|       - | 3566 | `											 * of the enum (whole-class constant update) — a` |
|       - | 3567 | `											 * broken sibling case throws here too. A reference` |
|       - | 3568 | `											 * from inside another constant's initializer` |
|       - | 3569 | `											 * (nConstEvalDepth > 0) evaluates only the` |
|       - | 3570 | `											 * requested case. */` |
|      87 | 3571 | `											if( pVm->nConstEvalDepth > 0 ){` |
|       3 | 3572 | `												rcEnum = VmEnumMaterializeCase(&(*pVm),pClass,pAttr);` |
|       2 | 3573 | `											}else{` |
|      85 | 3574 | `												rcEnum = VmEnumMaterialize(&(*pVm),pClass);` |
|       - | 3575 | `											}` |
|      46 | 3576 | `										}else{` |
|     747 | 3577 | `											rcEnum = VmClassConstEvalOnDemand(&(*pVm),pClass,pAttr);` |
|       - | 3578 | `										}` |
|     829 | 3579 | `										if( rcEnum != SXRET_OK ){` |
|      41 | 3580 | `											VmBoundaryPark(&(*pVm),rcEnum);` |
|      19 | 3581 | `										}` |
|     412 | 3582 | `									}` |
|       - | 3583 | `									/* Load the desired attribute */` |
|    4605 | 3584 | `									pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj,pAttr->nIdx);` |
|    4605 | 3585 | `									if( pValue ){` |
|    4565 | 3586 | `										PH7_MemObjLoad(pValue,pTos);` |
|    4565 | 3587 | `										if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|       - | 3588 | `											/* Load index number */` |
|    1889 | 3589 | `											pTos->nIdx = pAttr->nIdx;` |
|    1884 | 3590 | `											if( VmMemberFetchIsRefSource(pInstr)` |
|     949 | 3591 | `											 && (pAttr->iFlags & (PH7_CLASS_ATTR_REFBOUND\|PH7_CLASS_ATTR_REFSRCPIN)) == 0 ){` |
|       - | 3592 | `												/* The instance path's rule, for a class static:` |
|       - | 3593 | `												 * one counted pin so the other end's unpin cannot` |
|       - | 3594 | ``												 * free the static's value. `$o->p =& C::$s;` then`` |
|       - | 3595 | `												 * dropping $o read C::$s as NULL. */` |
|       6 | 3596 | `												pAttr->iFlags \|= PH7_CLASS_ATTR_REFSRCPIN;` |
|       6 | 3597 | `												VmPinMemObjSlotCounted(&(*pVm),pAttr->nIdx);` |
|       2 | 3598 | `											}` |
|     942 | 3599 | `										}` |
|    2285 | 3600 | `									}` |
|    2327 | 3601 | `								}else if( bConstForm \|\| !VmMemberCtxIsLookup(pInstr->iP2) ){` |
|       - | 3602 | `									/* Denied by visibility. php's Error is CATCHABLE here` |
|       - | 3603 | `									 * exactly as it is for an instance property, and PHL` |
|       - | 3604 | `									 * reported it uncaught and ABORTED the script -- so a` |
|       - | 3605 | ``									 * `try { C::$protectedStatic; } catch` never ran its`` |
|       - | 3606 | `									 * catch, and everything after the try was dropped with` |
|       - | 3607 | `									 * exit status 0. Parked on the boundary rail like the` |
|       - | 3608 | `									 * instance twin; the op completes with the NULL already` |
|       - | 3609 | `									 * in the slot and the fetch-point router lands the throw.` |
|       - | 3610 | ``									 * A lookup (isset/empty/`??`) stays silent and false, as`` |
|       - | 3611 | `									 * php's is. The name is built with the ATTRIBUTE's own` |
|       - | 3612 | `									 * spelling, which is the declaration's. */` |
|       - | 3613 | `									SyBlob sErrVis;` |
|      30 | 3614 | `									const char *zVis = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE` |
|       9 | 3615 | `										? "private" : "protected";` |
|      21 | 3616 | `									SyBlobInit(&sErrVis,&pVm->sAllocator);` |
|      21 | 3617 | `									if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|      15 | 3618 | `										SyBlobFormat(&sErrVis,"Cannot access %s constant %z::%z",` |
|       6 | 3619 | `											zVis,&pClass->sDisp,&pAttr->sName);` |
|       9 | 3620 | `									}else{` |
|       7 | 3621 | `										SyBlobFormat(&sErrVis,"Cannot access %s property %z::$%z",` |
|       3 | 3622 | `											zVis,&pClass->sDisp,&pAttr->sName);` |
|       - | 3623 | `									}` |
|      21 | 3624 | `									VmBoundaryPark(&(*pVm),VmThrowBuiltinError(&(*pVm),"Error",` |
|       - | 3625 | `										sizeof("Error")-1,&sErrVis));` |
|       9 | 3626 | `								}` |
|       - | 3627 | `							}` |
|    2312 | 3628 | `						}` |
|       - | 3629 | `					}` |
|       - | 3630 | `				}` |
|   24136 | 3631 | `				if( pThis ){` |
|       - | 3632 | `					/* Safely unreference the object */` |
|      73 | 3633 | `					PH7_ClassInstanceUnref(pThis);` |
|      35 | 3634 | `				}` |
|       - | 3635 | `			}` |
|   12063 | 3636 | `		}else{` |
|       - | 3637 | ``			/* `$v::X` where $v holds neither an object nor a class NAME. php's`` |
|       - | 3638 | `			 * catchable Error, which STOPS the statement; PH7 raised its own` |
|       - | 3639 | `` 			 * uncatchable wording and carried on with NULL, so a `$obj::$s = 1` `` |
|       - | 3640 | `			 * through a null base went on to fail again in OP_STORE. */` |
|       - | 3641 | `			sxi32 rcCn;` |
|      41 | 3642 | `			if( !pInstr->p3 ){` |
|      27 | 3643 | `				VmPopOperand(&pTos,1);` |
|      13 | 3644 | `			}` |
|      41 | 3645 | `			PH7_MemObjRelease(pTos);` |
|      41 | 3646 | `			MemObjSetType(pTos,MEMOBJ_NULL);` |
|      41 | 3647 | `			pTos->nIdx = SXU32_HIGH;` |
|      41 | 3648 | `			rcCn = VmThrowFromVm(&(*pVm),"Error","Class name must be a valid object or a string",` |
|       - | 3649 | `				sizeof("Class name must be a valid object or a string")-1);` |
|      41 | 3650 | `			if( rcCn == SXERR_ABORT ){ VM_EXIT_ABORT; }` |
|      41 | 3651 | `			rc = rcCn;` |
|      41 | 3652 | `			PH7_THROW_ROUTE_MIDEXPR(rc)` |
|       - | 3653 | `		}` |
|       - | 3654 | `	}` |
|  324906 | 3655 | `	VM_EXIT_BREAK;` |
|     ! 0 | 3656 | `	VM_EXIT_BREAK;` |
|  232934 | 3657 | `}` |
|       - | 3658 |  |
|       - | 3659 | `/*` |
|       - | 3660 | ` * OP_CLONE: body moved verbatim from the OP_CLONE arm of` |
|       - | 3661 | ` * VmByteCodeExecBody; arm-terminal breaks became VM_EXIT_BREAK.` |
|       - | 3662 | ` */` |
|     618 | 3663 | `PH7_PRIVATE VmOpRc VmExecOpClone(ph7_vm *pVm,VmExecState *pState,VmInstr *pInstr)` |
|       5 | 3664 | `{` |
|     623 | 3665 | `	ph7_value *pTos = pState->pTos;` |
|     623 | 3666 | `	ph7_value *pStack = pState->pStack;` |
|     623 | 3667 | `	VmInstr *aInstr = pState->aInstr;` |
|     623 | 3668 | `	sxi32 pc = pState->pc;` |
|       - | 3669 | `	sxi32 rc;` |
|     309 | 3670 | `	SXUNUSED(pInstr); SXUNUSED(pStack); SXUNUSED(aInstr); SXUNUSED(rc);` |
|       - | 3671 | `	ph7_class_instance *pSrc,*pClone;` |
|       - | 3672 | `#ifdef UNTRUST` |
|       - | 3673 | `	if( pTos < pStack ){` |
|       - | 3674 | `		VM_EXIT_ABORT;` |
|       - | 3675 | `	}` |
|       - | 3676 | `#endif` |
|       - | 3677 | `	/* Make sure we are dealing with a class instance. PHP 8 throws a catchable` |
|       - | 3678 | ``	 * TypeError for a non-object operand — for both `clone $x` and clone($x). */`` |
|     623 | 3679 | `	if( (pTos->iFlags & MEMOBJ_OBJ) == 0 ){` |
|       - | 3680 | `		SyBlob sMsg;` |
|       - | 3681 | `		char zGiven[64];` |
|      24 | 3682 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|       - | 3683 | `		/* php names the VALUE for a bool ("true given", not "bool given"), which is` |
|       - | 3684 | `		 * what every other argument diagnostic here already says — the shared helper. */` |
|      24 | 3685 | `		SyBlobFormat(&sMsg,"clone(): Argument #1 ($object) must be of type object, %s given",` |
|      11 | 3686 | `			VmValueGivenName(pTos,zGiven,sizeof(zGiven)));` |
|      24 | 3687 | `		rc = VmThrowBuiltinError(pVm,"TypeError",sizeof("TypeError")-1,&sMsg);` |
|      24 | 3688 | `		PH7_MemObjRelease(pTos);` |
|      24 | 3689 | `		pTos->nIdx = SXU32_HIGH;` |
|      24 | 3690 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 3691 | `			VM_EXIT_ABORT;` |
|       - | 3692 | `		}` |
|       - | 3693 | `		{` |
|       - | 3694 | `			sxi32 iRp;` |
|      24 | 3695 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|      24 | 3696 | `				pc = iRp;` |
|      24 | 3697 | `				VM_EXIT_BREAK;` |
|       - | 3698 | `			}` |
|       - | 3699 | `		}` |
|     ! 0 | 3700 | `		VM_EXIT_EXCEPTION;` |
|       - | 3701 | `	}` |
|       - | 3702 | `	/* Point to the source */` |
|     601 | 3703 | `	pSrc = (ph7_class_instance *)pTos->x.pOther;` |
|       - | 3704 | `	/* Enum cases are not cloneable — php's catchable Error (the singleton` |
|       - | 3705 | `	 * identity would break) — and neither is a class whose instances own a` |
|       - | 3706 | `	 * C-side resource a slot-by-slot copy would double-free (PH7_CLASS_NOCLONE),` |
|       - | 3707 | `	 * nor a Generator or a Fiber (their suspended C state is not copyable). php` |
|       - | 3708 | `	 * words all of them the same way and throws the same catchable Error; the` |
|       - | 3709 | `	 * Generator/Fiber pair used to take an older warn-only path here, so` |
|       - | 3710 | ``	 * `clone $gen` PRINTED an uncatchable diagnostic and then carried on with the`` |
|       - | 3711 | ``	 * ORIGINAL object standing in for the copy — the one shape of `clone` that`` |
|       - | 3712 | `	 * answered a value php never lets the program reach. */` |
|     596 | 3713 | `	if( (pSrc->pClass->iFlags & PH7_CLASS_ENUM) \|\| PH7_ClassIsUncloneable(pSrc->pClass)` |
|     519 | 3714 | `		\|\| pSrc->pClass == pVm->pGeneratorClass \|\| pSrc->pClass == pVm->pFiberClass ){` |
|       - | 3715 | `		SyBlob sMsg;` |
|     164 | 3716 | `		SyBlobInit(&sMsg,&pVm->sAllocator);` |
|     164 | 3717 | `		SyBlobFormat(&sMsg,"Trying to clone an uncloneable object of class %z",` |
|     160 | 3718 | `			&pSrc->pClass->sDisp);` |
|     164 | 3719 | `		rc = VmThrowBuiltinError(pVm,"Error",sizeof("Error")-1,&sMsg);` |
|     164 | 3720 | `		PH7_MemObjRelease(pTos);` |
|     164 | 3721 | `		pTos->nIdx = SXU32_HIGH;` |
|     164 | 3722 | `		if( rc == PH7_ABORT ){` |
|     ! 0 | 3723 | `			VM_EXIT_ABORT;` |
|       - | 3724 | `		}` |
|       - | 3725 | `		{` |
|       - | 3726 | `			sxi32 iRp;` |
|     164 | 3727 | `			if( VmRecordedResume(pVm,&iRp,pState->pEntryFrame,aInstr) ){` |
|     116 | 3728 | `				pc = iRp;` |
|     116 | 3729 | `				VM_EXIT_BREAK;` |
|       - | 3730 | `			}` |
|       - | 3731 | `		}` |
|      50 | 3732 | `		VM_EXIT_EXCEPTION;` |
|       - | 3733 | `	}` |
|       - | 3734 | `	/* Perform the clone operation */` |
|     441 | 3735 | `	pClone = PH7_CloneClassInstance(pSrc);` |
|     441 | 3736 | `	PH7_MemObjRelease(pTos);` |
|     441 | 3737 | `	if( pClone == 0 ){` |
|     ! 0 | 3738 | `		PH7_VmThrowError(&(*pVm),0,PH7_CTX_ERR,` |
|       - | 3739 | `			"Clone: cannot make an object clone due to a memory failure,PH7 is loading NULL");` |
|     ! 0 | 3740 | `	}else{` |
|       - | 3741 | `		/* Load the cloned object */` |
|     441 | 3742 | `		pTos->x.pOther = pClone;` |
|     441 | 3743 | `		MemObjSetType(pTos,MEMOBJ_OBJ);` |
|       - | 3744 | `	}` |
|     441 | 3745 | `	VM_EXIT_BREAK;` |
|     ! 0 | 3746 | `	VM_EXIT_BREAK;` |
|     314 | 3747 | `}` |
|       - | 3748 |  |
