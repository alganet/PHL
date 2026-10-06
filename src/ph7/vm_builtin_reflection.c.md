# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6346/7107 lines (89.29%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits |  Line | Source |
| -----: | ----: | :--- |
|      - |     1 | `/**` |
|      - |     2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |     3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |     4 | ` */` |
|      - |     5 | `#include "ph7int.h"` |
|      - |     6 | `/*` |
|      - |     7 | ` * This file implements the PHP 8.5 Reflection API.` |
|      - |     8 | ` *` |
|      - |     9 | ` * Following the engine's builtin-class pattern (Generator/Fiber/Closure),` |
|      - |    10 | ` * the Reflection classes themselves are written in PHP, embedded below as` |
|      - |    11 | ` * C string chunks and compiled at VM init by PH7_VmInstallReflection().` |
|      - |    12 | ` * Native behavior is provided by a small set of global __reflect_* thunk` |
|      - |    13 | ` * functions implemented here: the PHP methods forward to them, passing` |
|      - |    14 | ` * their target (class name, object, ...) explicitly.` |
|      - |    15 | ` *` |
|      - |    16 | ` * The chunks are kept below 30 KB each: MSVC caps a concatenated string` |
|      - |    17 | ` * literal at 65,535 bytes and the Windows build is real (build-aux/nmake.mk).` |
|      - |    18 | ` */` |
|      - |    19 |  |
|      - |    20 | `/* Bound on hierarchy walks; matches PH7_INTERFACE_WALK_MAX_DEPTH in` |
|      - |    21 | ` * vm_builtin_class.c. */` |
|      - |    22 | `#define REFLECT_WALK_MAX_DEPTH 64` |
|      - |    23 |  |
|      - |    24 | `static sxi32 ReflectEnforceStore(ph7_context *pCtx, sxu32 nIdx, ph7_value *pValue);` |
|      - |    25 |  |
|      - |    26 | `/*` |
|      - |    27 | ` * Resolve a class-name string or object into a ph7_class pointer. An unknown` |
|      - |    28 | ` * string name is autoloaded ONCE, by the lookup itself: a second` |
|      - |    29 | ` * PH7_VmTriggerAutoload here asked every loader again for a class the first` |
|      - |    30 | ` * round had already failed to find. Returns NULL when the class does not exist` |
|      - |    31 | ` * (the PHP layer turns that into ReflectionException).` |
|      - |    32 | ` */` |
|  10647 |    33 | `static ph7_class * ReflectResolveClass(ph7_vm *pVm, ph7_value *pArg)` |
|      5 |    34 | `{` |
|  10652 |    35 | `	return PH7_VmExtractClassFromValue(pVm, pArg);` |
|      5 |    36 | `}` |
|      - |    37 | `/*` |
|      - |    38 | ` * The same, for a door where php raises NOTHING of its own when an autoloader` |
|      - |    39 | `` * threw (ReflectionClass, ReflectionMethod, ReflectionEnum: `if`` |
|      - |    40 | `` * (!EG(exception))`). Answers 1 when the lookup raised, with *pRc the status`` |
|      - |    41 | ` * the door stops on -- PH7_OK after an in-place catch, whose resume frame the` |
|      - |    42 | ` * router already holds. Throwing "does not exist" there piled a second` |
|      - |    43 | ` * exception on the loader's, and it came back uncaught: the catch had already` |
|      - |    44 | ` * run in place for the first.` |
|      - |    45 | ` */` |
|   3207 |    46 | `static int ReflectResolveClassRaised(ph7_vm *pVm, ph7_value *pArg, ph7_class **ppClass, sxi32 *pRc)` |
|      5 |    47 | `{` |
|   3212 |    48 | `	sxi32 nBrc = pVm->nBoundaryRc;` |
|   3212 |    49 | `	const void *pRes = (const void *)pVm->pResumeFrame;` |
|   3212 |    50 | `	*ppClass = ReflectResolveClass(pVm, pArg);` |
|   3212 |    51 | `	*pRc = PH7_OK;` |
|   3212 |    52 | `	if( *ppClass \|\| !PH7_VmClassLookupRaised(pVm, nBrc, pRes) ){` |
|   3204 |    53 | `		return 0;` |
|      - |    54 | `	}` |
|      9 |    55 | `	if( pVm->nBoundaryRc != nBrc && PH7_CALLBACK_UNWOUND(pVm->nBoundaryRc) ){` |
|      9 |    56 | `		*pRc = pVm->nBoundaryRc;` |
|      4 |    57 | `	}` |
|      9 |    58 | `	return 1;` |
|   1606 |    59 | `}` |
|      - |    60 | `/*` |
|      - |    61 | ` * The other door shape: php throws its "does not exist" ReflectionException` |
|      - |    62 | ` * whatever the autoloader did, and zend chains a pending loader exception as` |
|      - |    63 | ` * its $previous (ReflectionProperty, ReflectionClassConstant and the enum` |
|      - |    64 | ` * cases, ReflectionParameter, isSubclassOf(), implementsInterface()). The` |
|      - |    65 | ` * lookup runs behind a throw fence, so the caller's catch does not run in place` |
|      - |    66 | ` * for the loader's exception first; *ppPrev carries it out (one reference, for` |
|      - |    67 | ` * the door to wrap and release). *pRc is PH7_ABORT when a loader exited.` |
|      - |    68 | ` */` |
|   1530 |    69 | `static ph7_class * ReflectResolveClassFenced(ph7_context *pCtx, ph7_value *pArg,` |
|      - |    70 | `	ph7_class * (*xResolve)(ph7_context *, ph7_value *), ph7_class_instance **ppPrev, sxi32 *pRc)` |
|      5 |    71 | `{` |
|   1535 |    72 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1535 |    73 | `	sxi32 nBrc = pVm->nBoundaryRc;` |
|   1535 |    74 | `	sxu32 nFenceIn = pVm->nThrowFence;` |
|   1535 |    75 | `	ph7_class_instance *pExcIn = pVm->pFencedExc;` |
|      - |    76 | `	ph7_class *pClass;` |
|   1535 |    77 | `	pVm->pFencedExc = 0;` |
|   1535 |    78 | `	pVm->nThrowFence = SySetUsed(&pVm->aException) + 1;` |
|   1535 |    79 | `	pClass = xResolve ? xResolve(pCtx, pArg) : ReflectResolveClass(pVm, pArg);` |
|   1535 |    80 | `	pVm->nThrowFence = nFenceIn;` |
|   1535 |    81 | `	*ppPrev = pVm->pFencedExc;` |
|   1535 |    82 | `	pVm->pFencedExc = pExcIn;` |
|   1535 |    83 | `	*pRc = PH7_OK;` |
|   1535 |    84 | `	if( pVm->nBoundaryRc == PH7_ABORT && nBrc != PH7_ABORT ){` |
|    ! 0 |    85 | `		*pRc = PH7_ABORT;` |
|    ! 0 |    86 | `		if( *ppPrev ){` |
|    ! 0 |    87 | `			PH7_ClassInstanceUnref(*ppPrev);` |
|    ! 0 |    88 | `			*ppPrev = 0;` |
|    ! 0 |    89 | `		}` |
|   1535 |    90 | `	}else if( *ppPrev ){` |
|     15 |    91 | `		pVm->nBoundaryRc = nBrc; /* nothing was caught in place: nothing to route */` |
|      7 |    92 | `	}` |
|   1535 |    93 | `	return pClass;` |
|      5 |    94 | `}` |
|      - |    95 | `/*` |
|      - |    96 | ` * Hand a freshly created class instance to the caller. The return slot` |
|      - |    97 | ` * takes over the initial reference from PH7_NewClassInstance (iRef=1):` |
|      - |    98 | ` * no extra iRef++ here (see the synthesized-object invariant — a stray` |
|      - |    99 | ` * bump leaks the object and disables its __destruct).` |
|      - |   100 | ` */` |
|   2487 |   101 | `static int ReflectResultObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|      5 |   102 | `{` |
|   2492 |   103 | `	if( pObj == 0 ){` |
|    ! 0 |   104 | `		ph7_result_null(pCtx);` |
|    ! 0 |   105 | `		return PH7_OK;` |
|      - |   106 | `	}` |
|   2492 |   107 | `	PH7_MemObjRelease(pCtx->pRet);` |
|   2492 |   108 | `	pCtx->pRet->x.pOther = pObj;` |
|   2492 |   109 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|   2492 |   110 | `	return PH7_OK;` |
|   1247 |   111 | `}` |
|      - |   112 | `/* The last of the descriptor marshalling: ReflectMapAddDyn survives because` |
|      - |   113 | ` * ReflectAttrArgs() answers a php ARRAY whose named arguments are string keys.` |
|      - |   114 | ` * Its Bool/Int/Str/Null/Attrs/Doc siblings went with __phl_rcinfo. */` |
|      - |   115 | `/* Add an entry under a dynamic (SyString) key. */` |
|    174 |   116 | `static void ReflectMapAddDyn(ph7_context *pCtx, ph7_value *pMap,` |
|      - |   117 | `	const SyString *pKey, ph7_value *pVal)` |
|      3 |   118 | `{` |
|    177 |   119 | `	ph7_value *pK = ph7_context_new_scalar(pCtx);` |
|    177 |   120 | `	if( pK == 0 ){ return; }` |
|    177 |   121 | `	ph7_value_string(pK, pKey->zString, (int)pKey->nByte);` |
|    177 |   122 | `	ph7_array_add_elem(pMap, pK, pVal);` |
|     76 |   123 | `}` |
|      - |   124 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth);` |
|      - |   125 | `/* Append pIface to the set unless it is already there (dedup by pointer). */` |
|   2693 |   126 | `static void ReflectIfaceAppend(SySet *pOut, ph7_class *pIface)` |
|      5 |   127 | `{` |
|      - |   128 | `	ph7_class **apKnown;` |
|      - |   129 | `	sxu32 n;` |
|   2698 |   130 | `	if( pIface == 0 ){` |
|    ! 0 |   131 | `		return;` |
|      - |   132 | `	}` |
|   2698 |   133 | `	apKnown = (ph7_class **)SySetBasePtr(pOut);` |
|   5104 |   134 | `	for( n = 0 ; n < SySetUsed(pOut) ; n++ ){` |
|   2686 |   135 | `		if( apKnown[n] == pIface ){` |
|    279 |   136 | `			return;` |
|      - |   137 | `		}` |
|   1054 |   138 | `	}` |
|   2422 |   139 | `	SySetPut(pOut, (const void *)&pIface);` |
|   1245 |   140 | `}` |
|      - |   141 | `static void ReflectFlattenIfaces(ph7_vm *pVm, ph7_class *pClass, SySet *pOut, int iDepth);` |
|      - |   142 | `/* Append pIface's OWN flattened list, BACKWARDS — see ReflectFlattenIfaces. */` |
|   1179 |   143 | `static void ReflectIfaceAppendOwn(ph7_vm *pVm, SySet *pOut, ph7_class *pIface, int iDepth)` |
|      5 |   144 | `{` |
|      - |   145 | `	SySet aSub;` |
|      - |   146 | `	ph7_class **ap;` |
|      - |   147 | `	sxu32 n;` |
|   1184 |   148 | `	SySetInit(&aSub, pOut->pAllocator, sizeof(ph7_class *));` |
|   1184 |   149 | `	ReflectFlattenIfaces(pVm, pIface, &aSub, iDepth + 1);` |
|   1184 |   150 | `	ap = (ph7_class **)SySetBasePtr(&aSub);` |
|   1759 |   151 | `	for( n = SySetUsed(&aSub) ; n > 0 ; n-- ){` |
|    579 |   152 | `		ReflectIfaceAppend(pOut, ap[n-1]);` |
|    272 |   153 | `	}` |
|   1184 |   154 | `	SySetRelease(&aSub);` |
|   1184 |   155 | `}` |
|      - |   156 | `/*` |
|      - |   157 | ` * The interface list php reports for pClass, IN PHP'S ORDER — the one` |
|      - |   158 | `` * `getInterfaceNames()`, `getInterfaces()`, `class_implements()` and the`` |
|      - |   159 | `` * export's `implements` / `extends` clause all publish. Caller owns the set`` |
|      - |   160 | ` * (SySetInit with sizeof(ph7_class *)).` |
|      - |   161 | ` *` |
|      - |   162 | ` * zend builds it once at link time out of two primitives, and every ordering` |
|      - |   163 | ` * rule below is one of them (all measured against php 8.5.9, 271 internal` |
|      - |   164 | ` * classes and interfaces agreeing row for row):` |
|      - |   165 | ` *` |
|      - |   166 | `` *   - `zend_do_inherit_interfaces` copies a list in BACKWARDS. That is what`` |
|      - |   167 | ``  *     puts `IteratorIterator` before its own `Traversable` and `Iterator` `` |
|      - |   168 | `` *     (`RecursiveIteratorIterator` reads `OuterIterator, Traversable,`` |
|      - |   169 | ``  *     Iterator`), and what makes `ErrorException` list `Throwable, Stringable` `` |
|      - |   170 | `` *     where its parent `Exception` lists them the other way round.`` |
|      - |   171 | ` *   - An INTERNAL class hands its interfaces over ONE AT A TIME` |
|      - |   172 | `` *     (`zend_class_implements`), so each is followed immediately by its own`` |
|      - |   173 | ` *     list; a compiled one declares them as a BLOCK, so all the declared ones` |
|      - |   174 | `` *     come first and the inherited ones after. `ArrayObject` is the first`` |
|      - |   175 | `` *     shape, `class Z implements P1, P2` the second.`` |
|      - |   176 | ` *   - The parent's list opens the answer, backwards for an internal class and` |
|      - |   177 | ` *     for a compiled one that declares NO interface of its own (which is the` |
|      - |   178 | `` *     `zend_do_inherit_interfaces` path again), forwards otherwise.`` |
|      - |   179 | ` *` |
|      - |   180 | ` * This engine walked a flattened set of its own and matched php on none of` |
|      - |   181 | ` * those: 71 of the 197 classes both engines share answered a different order.` |
|      - |   182 | ` */` |
|   2115 |   183 | `static void ReflectFlattenIfaces(ph7_vm *pVm, ph7_class *pClass, SySet *pOut, int iDepth)` |
|      5 |   184 | `{` |
|      - |   185 | `	SySet aDecl;` |
|      - |   186 | `	ph7_class **ap;` |
|      - |   187 | `	sxu32 n, nDecl;` |
|      - |   188 | `	int bInternal, bIface;` |
|   2120 |   189 | `	if( pClass == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|    ! 0 |   190 | `		return;` |
|      - |   191 | `	}` |
|   2120 |   192 | `	bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;` |
|   2120 |   193 | `	bIface = (pClass->iFlags & PH7_CLASS_INTERFACE) != 0;` |
|      - |   194 | `	/*` |
|      - |   195 | `	 * php auto-implements Stringable for an INTERNAL class that declares its` |
|      - |   196 | `	 * own __toString, and does it while REGISTERING the class -- before the` |
|      - |   197 | `	 * parent's interfaces are inherited and before its own are named. That is` |
|      - |   198 | ``	 * the whole reason `CachingIterator` opens with Stringable and its`` |
|      - |   199 | `	 * subclass, which only inherits the method, does not. A compiled class` |
|      - |   200 | `	 * gets the same auto-implement at the END of its declared list instead,` |
|      - |   201 | `	 * which compile_class.c already spells.` |
|      - |   202 | `	 */` |
|   2120 |   203 | `	if( bInternal && !bIface ){` |
|    732 |   204 | `		SyHashEntry *pTs = SyHashGet(&pClass->hMethod,` |
|      - |   205 | `			(const void *)"__toString", sizeof("__toString")-1);` |
|    927 |   206 | `		if( pTs && ReflectMethodDeclClass(pClass,` |
|    501 |   207 | `				(ph7_class_method *)pTs->pUserData) == pClass ){` |
|    191 |   208 | `			ReflectIfaceAppend(pOut, PH7_VmExtractClass(pVm,` |
|      - |   209 | `				"Stringable", sizeof("Stringable")-1, FALSE, 0));` |
|     85 |   210 | `		}` |
|    343 |   211 | `	}` |
|      - |   212 | `	/* What the class DECLARES, in declaration order. PHL keeps the first` |
|      - |   213 | ``	 * parent of an `interface B extends A, C` on the base chain and the rest`` |
|      - |   214 | `	 * in aInterface; php keeps no base chain for an interface at all. */` |
|   2120 |   215 | `	SySetInit(&aDecl, pOut->pAllocator, sizeof(ph7_class *));` |
|   2120 |   216 | `	if( bIface && pClass->pBase ){` |
|    505 |   217 | `		SySetPut(&aDecl, (const void *)&pClass->pBase);` |
|    237 |   218 | `	}` |
|   2120 |   219 | `	ap = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   2798 |   220 | `	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){` |
|    683 |   221 | `		SySetPut(&aDecl, (const void *)&ap[n]);` |
|    326 |   222 | `	}` |
|   2120 |   223 | `	nDecl = SySetUsed(&aDecl);` |
|      - |   224 | `	/* The parent class's own list opens the answer. */` |
|   2120 |   225 | `	if( !bIface && pClass->pBase ){` |
|      - |   226 | `		SySet aParent;` |
|    382 |   227 | `		SySetInit(&aParent, pOut->pAllocator, sizeof(ph7_class *));` |
|    382 |   228 | `		ReflectFlattenIfaces(pVm, pClass->pBase, &aParent, iDepth + 1);` |
|    382 |   229 | `		ap = (ph7_class **)SySetBasePtr(&aParent);` |
|    382 |   230 | `		if( bInternal \|\| nDecl == 0 ){` |
|   1121 |   231 | `			for( n = SySetUsed(&aParent) ; n > 0 ; n-- ){` |
|    751 |   232 | `				ReflectIfaceAppend(pOut, ap[n-1]);` |
|    331 |   233 | `			}` |
|    174 |   234 | `		}else{` |
|     14 |   235 | `			for( n = 0 ; n < SySetUsed(&aParent) ; n++ ){` |
|      5 |   236 | `				ReflectIfaceAppend(pOut, ap[n]);` |
|      3 |   237 | `			}` |
|      - |   238 | `		}` |
|    382 |   239 | `		SySetRelease(&aParent);` |
|    174 |   240 | `	}` |
|   2120 |   241 | `	ap = (ph7_class **)SySetBasePtr(&aDecl);` |
|   2120 |   242 | `	if( bInternal ){` |
|   2926 |   243 | `		for( n = 0 ; n < nDecl ; n++ ){` |
|   1045 |   244 | `			ReflectIfaceAppend(pOut, ap[n]);` |
|   1045 |   245 | `			ReflectIfaceAppendOwn(pVm, pOut, ap[n], iDepth);` |
|    493 |   246 | `		}` |
|    892 |   247 | `	}else{` |
|    375 |   248 | `		for( n = 0 ; n < nDecl ; n++ ){` |
|    141 |   249 | `			ReflectIfaceAppend(pOut, ap[n]);` |
|     72 |   250 | `		}` |
|    375 |   251 | `		for( n = 0 ; n < nDecl ; n++ ){` |
|    141 |   252 | `			ReflectIfaceAppendOwn(pVm, pOut, ap[n], iDepth);` |
|     72 |   253 | `		}` |
|      - |   254 | `	}` |
|   2120 |   255 | `	SySetRelease(&aDecl);` |
|   1010 |   256 | `}` |
|      - |   257 | ``/* The list every Reflection door asks for -- and `class_implements()`, which is`` |
|      - |   258 | ` * the same answer under another name (vm_builtin_class.c). */` |
|    558 |   259 | `PH7_PRIVATE void PH7_ReflectInterfacesOf(ph7_vm *pVm, ph7_class *pClass, SySet *pOut)` |
|      5 |   260 | `{` |
|    563 |   261 | `	ReflectFlattenIfaces(pVm, pClass, pOut, 0);` |
|    563 |   262 | `}` |
|      - |   263 | `/*` |
|      - |   264 | ` * Deepest base class whose method table maps the same name to the very` |
|      - |   265 | ` * same ph7_class_method pointer: inheritance shares member pointers` |
|      - |   266 | ` * (PH7_ClassInherit), so this identifies the declaring class. Methods` |
|      - |   267 | ` * copied in from traits are not on the pBase chain and thus report the` |
|      - |   268 | ` * using class, which is what PHP reports too.` |
|      - |   269 | ` */` |
| 125727 |   270 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth)` |
|      5 |   271 | `{` |
| 125732 |   272 | `	ph7_class *pDecl = pClass;` |
| 125732 |   273 | `	ph7_class *pBase = pClass->pBase;` |
| 125732 |   274 | `	int iDepth = 0;` |
| 190001 |   275 | `	while( pBase && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|      - |   276 | `		SyHashEntry *pEntry;` |
| 142270 |   277 | `		pEntry = SyHashGet(&pBase->hMethod, (const void *)SyStringData(&pMeth->sFunc.sName),` |
|  47093 |   278 | `			SyStringLength(&pMeth->sFunc.sName));` |
|  95177 |   279 | `		if( pEntry == 0 \|\| (ph7_class_method *)pEntry->pUserData != pMeth ){` |
|  15353 |   280 | `			break;` |
|      - |   281 | `		}` |
|  64274 |   282 | `		pDecl = pBase;` |
|  64274 |   283 | `		pBase = pBase->pBase;` |
|  64274 |   284 | `		iDepth++;` |
|      5 |   285 | `	}` |
|      - |   286 | `	/* A record the class only got from an INTERFACE belongs to the interface:` |
|      - |   287 | ``	 * php's scope for `ReflectionMethod('AbstractImpl','ifaceMethod')` is the`` |
|      - |   288 | `	 * interface that declared it, and the base walk above cannot see one` |
|      - |   289 | `	 * (an interface is not on the pBase chain of the class implementing it). */` |
| 125732 |   290 | `	if( pDecl->iFlags & PH7_CLASS_INTERFACE ){` |
|   5301 |   291 | `		return pDecl;` |
|      - |   292 | `	}` |
|      - |   293 | `	{` |
| 120434 |   294 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&pDecl->aInterface);` |
|      - |   295 | `		sxu32 n;` |
| 210197 |   296 | `		for( n = 0 ; n < SySetUsed(&pDecl->aInterface) ; n++ ){` |
| 134465 |   297 | `			SyHashEntry *pEntry = SyHashGet(&apIface[n]->hMethod,` |
|  89801 |   298 | `				(const void *)SyStringData(&pMeth->sFunc.sName),` |
|  44659 |   299 | `				SyStringLength(&pMeth->sFunc.sName));` |
|  89806 |   300 | `			if( pEntry && (ph7_class_method *)pEntry->pUserData == pMeth ){` |
|     39 |   301 | `				return ReflectMethodDeclClass(apIface[n], pMeth);` |
|      - |   302 | `			}` |
|  44645 |   303 | `		}` |
|      - |   304 | `	}` |
| 120396 |   305 | `	return pDecl;` |
|  62682 |   306 | `}` |
|      - |   307 | `/* Fetch a class attribute (property or constant) by plain name. */` |
|      4 |   308 | `static ph7_class_attr * ReflectFetchAttr(ph7_class *pClass, ph7_value *pName)` |
|      1 |   309 | `{` |
|      - |   310 | `	SyHashEntry *pEntry;` |
|      - |   311 | `	const char *zName;` |
|      - |   312 | `	int nLen;` |
|      5 |   313 | `	zName = ph7_value_to_string(pName, &nLen);` |
|      5 |   314 | `	if( nLen < 1 ){` |
|    ! 0 |   315 | `		return 0;` |
|      - |   316 | `	}` |
|      5 |   317 | `	pEntry = SyHashGet(&pClass->hAttr, (const void *)zName, (sxu32)nLen);` |
|      5 |   318 | `	if( pEntry == 0 ){` |
|      3 |   319 | `		return 0;` |
|      - |   320 | `	}` |
|      3 |   321 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|      3 |   322 | `}` |
|      - |   323 | `/* Fetch a class CONSTANT (or enum case) by name from the hConst namespace. */` |
|      2 |   324 | `static ph7_class_attr * ReflectFetchConst(ph7_class *pClass, ph7_value *pName)` |
|      1 |   325 | `{` |
|      - |   326 | `	const char *zName;` |
|      - |   327 | `	int nLen;` |
|      3 |   328 | `	zName = ph7_value_to_string(pName, &nLen);` |
|      3 |   329 | `	if( nLen < 1 ){` |
|    ! 0 |   330 | `		return 0;` |
|      - |   331 | `	}` |
|      3 |   332 | `	return PH7_ClassExtractConstant(pClass, zName, (sxu32)nLen);` |
|      2 |   333 | `}` |
|      - |   334 | `/* Fetch a member by name from EITHER namespace (property then constant) — used` |
|      - |   335 | ` * where the caller reflects over a member that may be either (e.g. attributes` |
|      - |   336 | ` * attached to a property or a constant). */` |
|      4 |   337 | `static ph7_class_attr * ReflectFetchMember(ph7_class *pClass, ph7_value *pName)` |
|      1 |   338 | `{` |
|      5 |   339 | `	ph7_class_attr *pAttr = ReflectFetchAttr(pClass, pName);` |
|      5 |   340 | `	if( pAttr == 0 ){` |
|      3 |   341 | `		pAttr = ReflectFetchConst(pClass, pName);` |
|      1 |   342 | `	}` |
|      5 |   343 | `	return pAttr;` |
|      1 |   344 | `}` |
|      - |   345 | `/*` |
|      - |   346 | ` * ---------------------------------------------------------------------------` |
|      - |   347 | ` * The member walk.` |
|      - |   348 | ` *` |
|      - |   349 | ` * php reports a class's members in ONE order — the class's own first (in` |
|      - |   350 | ` * declaration order), then each inheritance level's, outward — and every` |
|      - |   351 | ` * accessor that lists or looks one up has to agree with it. It used to live` |
|      - |   352 | ` * inside the descriptor builder alone; the native ReflectionClass needs the` |
|      - |   353 | ` * same order for getMethods()/getProperties()/getReflectionConstants() and the` |
|      - |   354 | ` * same visibility filtering for hasMethod()/getMethod()/getConstructor(), so` |
|      - |   355 | ` * the walk is factored out here and every one of them drives it.` |
|      - |   356 | ` *` |
|      - |   357 | ` * Per level the DECLARING class's own hash is iterated — a subclass hash` |
|      - |   358 | ` * interleaves inherited pointers unpredictably — and a pointer-identity lookup` |
|      - |   359 | ` * in the reflected class's hash drops what is not visible there (overridden` |
|      - |   360 | ` * entries). Methods come out reversed because hMethod is still a head-insert` |
|      - |   361 | ` * table, while hAttr/hConst insert at the tail.` |
|      - |   362 | ` * ---------------------------------------------------------------------------` |
|      - |   363 | ` */` |
|      - |   364 | `#define REFLECT_MEMBER_PROP   0` |
|      - |   365 | `#define REFLECT_MEMBER_CONST  1` |
|      - |   366 | `#define REFLECT_MEMBER_METHOD 2` |
|      - |   367 |  |
|      - |   368 | `typedef struct ReflectMember ReflectMember;` |
|      - |   369 | `struct ReflectMember` |
|      - |   370 | `{` |
|      - |   371 | `	int iKind;               /* REFLECT_MEMBER_* */` |
|      - |   372 | `	SyString sKey;           /* the name php reports it under: a trait` |
|      - |   373 | ``	                          * `use T { m as n; }` alias differs from the`` |
|      - |   374 | `	                          * method's own sFunc.sName, and php reports n */` |
|      - |   375 | `	ph7_class *pDecl;        /* declaring class */` |
|      - |   376 | `	ph7_class_attr *pAttr;   /* property or constant (NULL for a method) */` |
|      - |   377 | `	ph7_class_method *pMeth; /* method (NULL for a property or constant) */` |
|      - |   378 | `};` |
|      - |   379 | `/*` |
|      - |   380 | ` * Collect the members php would report for pClass, in php's own order, into a` |
|      - |   381 | ` * SySet of ReflectMember. The caller owns the set (SySetInit with` |
|      - |   382 | ` * sizeof(ReflectMember) / SySetRelease).` |
|      - |   383 | ` *` |
|      - |   384 | ` * bLookup selects which of php's TWO answers is wanted. The LISTING` |
|      - |   385 | ` * (getMethods()/getProperties()/getReflectionConstants(), bLookup = 0) hides a` |
|      - |   386 | ` * base class's private members; the LOOKUP (bLookup = 1) does not, because php` |
|      - |   387 | ` * keeps a parent's private in the child's tables and ReflectionMethod resolves` |
|      - |   388 | ` * against those. The two really do disagree: hasMethod('basePriv') is true on` |
|      - |   389 | ` * the subclass while getMethods() never mentions it.` |
|      - |   390 | ` */` |
|   3193 |   391 | `static void ReflectMembers(ph7_vm *pVm, ph7_class *pClass, SySet *pOut, int bLookup)` |
|      5 |   392 | `{` |
|      - |   393 | `	ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];` |
|   3198 |   394 | `	ph7_class *pWalk = pClass;` |
|      - |   395 | `	SyHashEntry *pEntry;` |
|      - |   396 | `	SySet aTmp;` |
|   3198 |   397 | `	int bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;` |
|      - |   398 | `	int iPart;` |
|   3198 |   399 | `	sxu32 nChain = 0, iLevel, nLev, nT;` |
|   7357 |   400 | `	while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){` |
|   4164 |   401 | `		aChain[nChain++] = pWalk;` |
|   4164 |   402 | `		pWalk = pWalk->pBase;` |
|      5 |   403 | `	}` |
|   3198 |   404 | `	SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));` |
|      - |   405 | `	/* PROPERTIES of an INTERNAL class come out base-first, and that is php's` |
|      - |   406 | `	 * registration order rather than a rule of its own: a user class declares its` |
|      - |   407 | ``	 * own properties and THEN inherits (`class B extends A` reports B's before`` |
|      - |   408 | `	 * A's), while an internal one is registered against its parent and declares` |
|      - |   409 | `	 * afterwards — so ErrorException reports Exception's five and then its own` |
|      - |   410 | `	 * $severity. Only the property table flips: php lists an internal class's` |
|      - |   411 | `	 * METHODS and CONSTANTS own-first like everything else, so the walk below` |
|      - |   412 | `	 * runs the two tables in opposite level orders. */` |
|  12777 |   413 | `	for( iPart = 0 ; iPart < 3 ; iPart++ ){` |
|  22061 |   414 | `	  for( nLev = 0 ; nLev < nChain ; nLev++ ){` |
|      - |   415 | `		ph7_class *pLevel;` |
|  12482 |   416 | `		int iTab = iPart;` |
|  12482 |   417 | `		iLevel = (iPart == 0 && bInternal) ? nChain - 1 - nLev : nLev;` |
|  12482 |   418 | `		pLevel = aChain[iLevel];` |
|      - |   419 | `		/* --- Properties (hAttr, iPart 0) and constants/enum cases (hConst,` |
|      - |   420 | `		 * iPart 1) — php's two separate member namespaces. Each table is` |
|      - |   421 | `		 * collected and emitted independently; the CONSTANT flag still decides` |
|      - |   422 | `		 * which kind comes out. --- */` |
|  12482 |   423 | `		if( iPart < 2 ){` |
|   8323 |   424 | `			SyHash *pSrcHash = iTab ? &pLevel->hConst : &pLevel->hAttr;` |
|   8323 |   425 | `			SyHash *pRefHash = iTab ? &pClass->hConst : &pClass->hAttr;` |
|   8323 |   426 | `			SySetReset(&aTmp);` |
|   8323 |   427 | `			SyHashResetLoopCursor(pSrcHash);` |
|  71252 |   428 | `			while( (pEntry = SyHashGetNextEntry(pSrcHash)) != 0 ){` |
|  62934 |   429 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  62934 |   430 | `				ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pLevel);` |
|  62934 |   431 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_HIDDEN ){` |
|      - |   432 | `					/* A native class's engine slot: php holds that state in its own C` |
|      - |   433 | `					 * struct and reports no property for it at all. */` |
|   7446 |   434 | `					continue;` |
|      - |   435 | `				}` |
|  55493 |   436 | `				if( iLevel == 0 ){` |
|      - |   437 | `					sxu32 j;` |
|      - |   438 | `					/* Own = declared here or by an off-chain provider (trait) */` |
|  50173 |   439 | `					for( j = 1 ; j < nChain ; j++ ){` |
|  17645 |   440 | `						if( aChain[j] == pDecl ){ break; }` |
|   3517 |   441 | `					}` |
|  43069 |   442 | `					if( j < nChain ){ continue; }` |
|  16260 |   443 | `				}else{` |
|      - |   444 | `					SyHashEntry *pSub;` |
|  12427 |   445 | `					if( pDecl != pLevel ){ continue; }` |
|      - |   446 | `					/* A base's PRIVATE member is not part of the subclass's` |
|      - |   447 | `					 * surface — php reports neither a private property nor a` |
|      - |   448 | `					 * private constant of a parent on the child. PHL's` |
|      - |   449 | `					 * inheritance copies them down all the same, so the filter` |
|      - |   450 | `					 * has to be here. */` |
|  10631 |   451 | `					if( !bLookup && pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){ continue; }` |
|      - |   452 | `					/* Must still be the visible member in the reflected class */` |
|  10307 |   453 | `					pSub = SyHashGet(pRefHash, pEntry->pKey, pEntry->nKeyLen);` |
|  10307 |   454 | `					if( pSub == 0 \|\| pSub->pUserData != (void *)pAttr ){ continue; }` |
|      - |   455 | `				}` |
|  42771 |   456 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|      5 |   457 | `			}` |
|      - |   458 | `			/* Forward: hAttr/hConst iterate in DECLARATION order (tail inserts),` |
|      - |   459 | `			 * so members come out in the order php reports them. */` |
|  51089 |   460 | `			for( nT = 0 ; nT < SySetUsed(&aTmp) ; nT++ ){` |
|  42771 |   461 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT);` |
|  42771 |   462 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;` |
|      - |   463 | `				ReflectMember sMember;` |
|  42771 |   464 | `				sMember.iKind = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|  21397 |   465 | `					? REFLECT_MEMBER_CONST : REFLECT_MEMBER_PROP;` |
|  42771 |   466 | `				sMember.sKey = pAttr->sName;` |
|  42771 |   467 | `				sMember.pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pLevel);` |
|  42771 |   468 | `				sMember.pAttr = pAttr;` |
|  42771 |   469 | `				sMember.pMeth = 0;` |
|  42771 |   470 | `				SySetPut(pOut, (const void *)&sMember);` |
|  21374 |   471 | `			}` |
|   8323 |   472 | `			continue;` |
|      - |   473 | `		}` |
|      - |   474 | `		/* --- Methods. The reported name is the hash-entry KEY, not the` |
|      - |   475 | `		 * function's own name (see ReflectMember::sKey). --- */` |
|   4164 |   476 | `		SySetReset(&aTmp);` |
|   4164 |   477 | `		SyHashResetLoopCursor(&pLevel->hMethod);` |
|  64380 |   478 | `		while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){` |
|  60221 |   479 | `			ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  60221 |   480 | `			ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|      - |   481 | `			/* A property HOOK is compiled into a method (__phl_hook_get_NAME /` |
|      - |   482 | ``			 * __phl_hook_set_NAME) so that inheritance and `parent::` work on`` |
|      - |   483 | `			 * it, but php has no method there at all: it reports the hook on` |
|      - |   484 | `			 * the PROPERTY. Listing it would put a PHL-only name on a` |
|      - |   485 | `			 * php-visible surface. */` |
|  60216 |   486 | `			if( pEntry->nKeyLen > sizeof("__phl_hook_")-1` |
|  41608 |   487 | `			 && SyMemcmp(pEntry->pKey, "__phl_hook_", sizeof("__phl_hook_")-1) == 0 ){` |
|    862 |   488 | `				continue;` |
|      - |   489 | `			}` |
|  59363 |   490 | `			if( iLevel == 0 ){` |
|      - |   491 | `				sxu32 j;` |
|  56707 |   492 | `				for( j = 1 ; j < nChain ; j++ ){` |
|  26771 |   493 | `					if( aChain[j] == pDecl ){ break; }` |
|   6652 |   494 | `				}` |
|  43100 |   495 | `				if( j < nChain ){ continue; }` |
|  14944 |   496 | `			}else{` |
|      - |   497 | `				SyHashEntry *pSub;` |
|  16267 |   498 | `				if( pDecl != pLevel ){ continue; }` |
|      - |   499 | `				/* Same rule as the members above: a base's PRIVATE method is` |
|      - |   500 | ``				 * not on the subclass's surface. `class B extends A` lists`` |
|      - |   501 | `				 * only A::q when A::p is private — php's inheritance never` |
|      - |   502 | `				 * hands the child a private, and PH7_ClassInherit's copy-down` |
|      - |   503 | `				 * does, so it is filtered here. */` |
|  13384 |   504 | `				if( !bLookup && pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){ continue; }` |
|  12948 |   505 | `				pSub = SyHashGet(&pClass->hMethod, pEntry->pKey, pEntry->nKeyLen);` |
|  12948 |   506 | `				if( pSub == 0 \|\| pSub->pUserData != (void *)pMeth ){` |
|      - |   507 | `					/* Overridden below this level: already reported */` |
|    221 |   508 | `					continue;` |
|      - |   509 | `				}` |
|      - |   510 | `			}` |
|  42664 |   511 | `			SySetPut(&aTmp, (const void *)&pEntry);` |
|      5 |   512 | `		}` |
|  46823 |   513 | `		for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|  42664 |   514 | `			SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|      - |   515 | `			ReflectMember sMember;` |
|  42664 |   516 | `			sMember.iKind = REFLECT_MEMBER_METHOD;` |
|  42664 |   517 | `			SyStringInitFromBuf(&sMember.sKey, (const char *)pE->pKey, pE->nKeyLen);` |
|  42664 |   518 | `			sMember.pMeth = (ph7_class_method *)pE->pUserData;` |
|  42664 |   519 | `			sMember.pDecl = ReflectMethodDeclClass(pClass, sMember.pMeth);` |
|  42664 |   520 | `			sMember.pAttr = 0;` |
|  42664 |   521 | `			SySetPut(pOut, (const void *)&sMember);` |
|  21290 |   522 | `		}` |
|   2082 |   523 | `	  }` |
|   4793 |   524 | `	}` |
|   3198 |   525 | `	SySetRelease(&aTmp);` |
|   3198 |   526 | `}` |
|      - |   527 | `/* Does a collected member name match zName exactly? */` |
|  16200 |   528 | `static int ReflectKeyIs(const ReflectMember *pM, const char *zName, int nName)` |
|      5 |   529 | `{` |
|  18342 |   530 | `	return SyStringLength(&pM->sKey) == (sxu32)nName` |
|  16200 |   531 | `		&& SyMemcmp(SyStringData(&pM->sKey), zName, (sxu32)nName) == 0;` |
|      5 |   532 | `}` |
|      - |   533 | `/*` |
|      - |   534 | ` * php's METHOD lookup, which is NOT the listing.` |
|      - |   535 | ` *` |
|      - |   536 | ` * getMethods() hides a base's private method, but hasMethod()/getMethod() find` |
|      - |   537 | ` * one: Zend keeps the parent's private in the child's function table and reads` |
|      - |   538 | ` * that table directly. PHL's inheritance copies methods down the same way, so` |
|      - |   539 | ` * the lookup is the class's own hMethod — case-insensitively, like php.` |
|      - |   540 | ` */` |
|  21376 |   541 | `static SyHashEntry * ReflectFindMethodEntry(ph7_class *pClass, const char *zName, int nName)` |
|      5 |   542 | `{` |
|      - |   543 | `	SyHashEntry *pEntry;` |
|  21381 |   544 | `	if( nName < 1 ){` |
|    ! 0 |   545 | `		return 0;` |
|      - |   546 | `	}` |
|  21381 |   547 | `	pEntry = SyHashGet(&pClass->hMethod, (const void *)zName, (sxu32)nName);` |
|  21381 |   548 | `	if( pEntry ){` |
|  13593 |   549 | `		return pEntry;` |
|      - |   550 | `	}` |
|   7791 |   551 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  57515 |   552 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  45830 |   553 | `		if( (int)pEntry->nKeyLen == nName` |
|  24638 |   554 | `		 && SyStrnicmp((const char *)pEntry->pKey, zName, (sxu32)nName) == 0 ){` |
|    ! 0 |   555 | `			return pEntry;` |
|      - |   556 | `		}` |
|      3 |   557 | `	}` |
|   7791 |   558 | `	return 0;` |
|  10693 |   559 | `}` |
|      - |   560 | `/*` |
|      - |   561 | `` * The visibility of the __construct / __clone `new` and `clone` would reach, or`` |
|      - |   562 | ` * 0 when the class has none — what isInstantiable() and isCloneable() screen on.` |
|      - |   563 | ` * The LOOKUP, not the listing: a class that inherits a private constructor is` |
|      - |   564 | ` * still not instantiable even though getMethods() does not report one.` |
|      - |   565 | ` */` |
|    168 |   566 | `static void ReflectCtorCloneVis(ph7_vm *pVm, ph7_class *pClass, sxi32 *piCtor, sxi32 *piClone)` |
|      5 |   567 | `{` |
|      - |   568 | `	ph7_class_method *pMeth;` |
|     84 |   569 | `	SXUNUSED(pVm);` |
|    173 |   570 | `	*piCtor = *piClone = 0;` |
|    173 |   571 | `	pMeth = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|    173 |   572 | `	if( pMeth ){` |
|     94 |   573 | `		*piCtor = pMeth->iProtection;` |
|     45 |   574 | `	}` |
|    173 |   575 | `	pMeth = PH7_ClassExtractMethod(pClass, "__clone", sizeof("__clone")-1);` |
|    173 |   576 | `	if( pMeth ){` |
|      7 |   577 | `		*piClone = pMeth->iProtection;` |
|      3 |   578 | `	}` |
|    173 |   579 | `}` |
|      - |   580 | `/* Where __phl_rcinfo() was: a full class DESCRIPTOR array — every constant,` |
|      - |   581 | ` * property and method of the whole inheritance chain, marshalled once and` |
|      - |   582 | ` * memoized on pVm->hClassInfo because the prelude classes rebuilt it once per` |
|      - |   583 | ` * member they constructed. Every one of its readers is a native class now and` |
|      - |   584 | ` * reads ph7_class directly, so the builder and the VM memo are both gone. */` |
|      - |   585 | `/*` |
|      - |   586 | ` * Collect a PHP array's values into a ph7_value* set (call arguments).` |
|      - |   587 | ` * When ppNames is non-NULL, string keys become named arguments: a name` |
|      - |   588 | ` * map is lazily allocated (like call_user_func_array's) with one entry` |
|      - |   589 | ` * per collected slot, empty entries meaning positional. When ppNodes is` |
|      - |   590 | ` * non-NULL it receives each slot's array node, which is what says whether` |
|      - |   591 | ` * an element is a REFERENCE (freed by the caller, like the names).` |
|      - |   592 | ` */` |
|    122 |   593 | `static sxi32 ReflectCollectArgs(ph7_context *pCtx, ph7_value *pArray, SySet *pOut, SyString **ppNames,` |
|      - |   594 | `	ph7_hashmap_node ***ppNodes)` |
|      4 |   595 | `{` |
|      - |   596 | `	ph7_hashmap *pMap;` |
|      - |   597 | `	ph7_hashmap_node *pEntry;` |
|    126 |   598 | `	SyString *aNames = 0;` |
|    126 |   599 | `	sxu32 nSlot = 0;` |
|      - |   600 | `	sxu32 n;` |
|    126 |   601 | `	if( ppNames ){` |
|    126 |   602 | `		*ppNames = 0;` |
|     61 |   603 | `	}` |
|    126 |   604 | `	if( ppNodes ){` |
|     58 |   605 | `		*ppNodes = 0;` |
|     27 |   606 | `	}` |
|    126 |   607 | `	if( !ph7_value_is_array(pArray) ){` |
|    ! 0 |   608 | `		return SXRET_OK;` |
|      - |   609 | `	}` |
|    126 |   610 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|    126 |   611 | `	pEntry = pMap->pFirst;` |
|    126 |   612 | `	if( ppNodes && pMap->nEntry > 0 ){` |
|    104 |   613 | `		*ppNodes = (ph7_hashmap_node **)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     50 |   614 | `			pMap->nEntry * sizeof(ph7_hashmap_node *));` |
|     54 |   615 | `		if( *ppNodes ){` |
|     54 |   616 | `			SyZero(*ppNodes, pMap->nEntry * sizeof(ph7_hashmap_node *));` |
|     25 |   617 | `		}` |
|     25 |   618 | `	}` |
|    268 |   619 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    146 |   620 | `		ph7_value *pValue = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|    146 |   621 | `		if( pValue ){` |
|    146 |   622 | `			if( ppNodes && *ppNodes ){` |
|     80 |   623 | `				(*ppNodes)[nSlot] = pEntry;` |
|     38 |   624 | `			}` |
|    146 |   625 | `			if( ppNames && pEntry->iType == HASHMAP_BLOB_NODE ){` |
|     43 |   626 | `				if( aNames == 0 ){` |
|     49 |   627 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     32 |   628 | `						pMap->nEntry * sizeof(SyString));` |
|     33 |   629 | `					if( aNames ){` |
|     33 |   630 | `						SyZero(aNames, pMap->nEntry * sizeof(SyString));` |
|     16 |   631 | `					}` |
|     16 |   632 | `				}` |
|     43 |   633 | `				if( aNames ){` |
|     43 |   634 | `					SyStringInitFromBuf(&aNames[nSlot],` |
|      - |   635 | `						SyBlobData(&pEntry->xKey.sKey), SyBlobLength(&pEntry->xKey.sKey));` |
|     21 |   636 | `				}` |
|     21 |   637 | `			}` |
|    146 |   638 | `			SySetPut(pOut, (const void *)&pValue);` |
|    146 |   639 | `			nSlot++;` |
|     71 |   640 | `		}` |
|    146 |   641 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     75 |   642 | `	}` |
|    126 |   643 | `	if( ppNames ){` |
|    126 |   644 | `		*ppNames = aNames;` |
|     61 |   645 | `	}` |
|    126 |   646 | `	return SXRET_OK;` |
|     65 |   647 | `}` |
|      - |   648 | `/*` |
|      - |   649 | ` * The names a forwarding reflection door (invoke(), invokeArgs(), newInstance(),` |
|      - |   650 | ` * newInstanceArgs()) hands its target, built into pMap. With aNames, they are the` |
|      - |   651 | ` * string keys ReflectCollectArgs found in an argument array; without, they are the` |
|      - |   652 | ` * door's own call-site names from argument nSkip on -- the signature binder has` |
|      - |   653 | `` * already collected every `name:` the door's declared parameters do not take as a`` |
|      - |   654 | ` * named extra. Answers 0 when there is none: the call is positional. php reaches the` |
|      - |   655 | ` * target from an internal function either way, so the map binds WEAKLY; the caller` |
|      - |   656 | ` * sets bCallbackWeak around the dispatch.` |
|      - |   657 | ` */` |
|    190 |   658 | `static VmCallArgMap *ReflectDoorArgMap(ph7_context *pCtx, SyString *aNames, sxu32 nSlot,` |
|      - |   659 | `	sxu32 nSkip, VmCallArgMap *pMap)` |
|      5 |   660 | `{` |
|    195 |   661 | `	VmCallArgMap *pOuter = pCtx->pArgMap;` |
|    195 |   662 | `	SyZero(pMap, sizeof(*pMap));` |
|    195 |   663 | `	if( aNames == 0 ){` |
|    167 |   664 | `		if( pOuter == 0 \|\| !pOuter->bHasNamed \|\| pOuter->nTotal <= nSkip ){` |
|    127 |   665 | `			return 0;` |
|      - |   666 | `		}` |
|     42 |   667 | `		nSlot = pOuter->nTotal - nSkip;` |
|     42 |   668 | `		aNames = &pOuter->aNames[nSkip];` |
|     42 |   669 | `		pMap->bFromUnpack = pOuter->bFromUnpack;` |
|     42 |   670 | `		pMap->aRun = pOuter->aRun ? &pOuter->aRun[nSkip] : 0;` |
|     20 |   671 | `	}` |
|     70 |   672 | `	pMap->bHasNamed = 1;` |
|     70 |   673 | `	pMap->nTotal = nSlot;` |
|     70 |   674 | `	pMap->aNames = aNames;` |
|     70 |   675 | `	return pMap;` |
|    100 |   676 | `}` |
|      - |   677 | `/*` |
|      - |   678 | ` * Instantiate pClassName and run its constructor over pArgs (a PHP array;` |
|      - |   679 | `` * string keys become NAMED arguments, which is how `#[Attr(x: 1)]` arrives).`` |
|      - |   680 | ` * The object lands in the call's result slot.` |
|      - |   681 | ` *` |
|      - |   682 | ` * ReflectionAttribute::newInstance()'s back end. It is deliberately NOT the` |
|      - |   683 | ` * ReflectionClass one (ReflectNewInstance, below): php runs no instantiability` |
|      - |   684 | ` * or constructor-visibility screen here — the attribute's own #[Attribute]` |
|      - |   685 | ` * declaration is what was checked — so an abstract or private-ctor attribute` |
|      - |   686 | ` * class reaches the engine's own Error, not a ReflectionException.` |
|      - |   687 | ` */` |
|     58 |   688 | `static sxi32 ReflectAttrInstantiate(ph7_context *pCtx, ph7_value *pClassName, ph7_value *pArgs)` |
|      4 |   689 | `{` |
|     62 |   690 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |   691 | `	ph7_class *pClass;` |
|      - |   692 | `	ph7_class_instance *pThis;` |
|      - |   693 | `	ph7_class_method *pCons;` |
|     62 |   694 | `	if( (pClass = ReflectResolveClass(pVm, pClassName)) == 0 ){` |
|    ! 0 |   695 | `		ph7_result_null(pCtx);` |
|    ! 0 |   696 | `		return PH7_OK;` |
|      - |   697 | `	}` |
|     62 |   698 | `	if( VmClassStaticDeferPending(pClass) ){` |
|      - |   699 | `		/* Instantiation materializes the static table (OP_NEW does it too), so a` |
|      - |   700 | `		 * broken default raises BEFORE any object exists. */` |
|    ! 0 |   701 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pVm, pClass);` |
|    ! 0 |   702 | `		if( rcMat != SXRET_OK ){` |
|    ! 0 |   703 | `			return rcMat;` |
|      - |   704 | `		}` |
|    ! 0 |   705 | `	}` |
|     62 |   706 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|     62 |   707 | `	if( pThis == 0 ){` |
|    ! 0 |   708 | `		ph7_result_null(pCtx);` |
|    ! 0 |   709 | `		return PH7_OK;` |
|      - |   710 | `	}` |
|     62 |   711 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|     62 |   712 | `	if( pCons ){` |
|      - |   713 | `		SySet aArg;` |
|      - |   714 | `		sxi32 rc;` |
|     55 |   715 | `		SyString *aNames = 0;` |
|     55 |   716 | `		SySetInit(&aArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|     55 |   717 | `		if( pArgs ){` |
|     55 |   718 | `			ReflectCollectArgs(pCtx, pArgs, &aArg, &aNames, 0);` |
|     26 |   719 | `		}` |
|     55 |   720 | `		if( aNames ){` |
|      - |   721 | `			VmCallArgMap sMap;` |
|      5 |   722 | `			SyZero(&sMap,sizeof(sMap)); /* new map fields must read unset, not stack garbage */` |
|      5 |   723 | `			sMap.bHasNamed = 1;` |
|      5 |   724 | `			sMap.bIsNamespaced = 0;` |
|      5 |   725 | `			sMap.bStrict = 0;` |
|      5 |   726 | `			sMap.nTotal = SySetUsed(&aArg);` |
|      5 |   727 | `			sMap.aNames = aNames;` |
|      7 |   728 | `			rc = PH7_VmCallClassMethodMap(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|      4 |   729 | `				(ph7_value **)SySetBasePtr(&aArg), &sMap);` |
|      5 |   730 | `			SyMemBackendFree(&pVm->sAllocator, aNames);` |
|      3 |   731 | `		}else{` |
|     75 |   732 | `			rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|     48 |   733 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|      - |   734 | `		}` |
|     55 |   735 | `		SySetRelease(&aArg);` |
|     55 |   736 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|      3 |   737 | `			PH7_ClassInstanceCtorFailed(pThis);` |
|      3 |   738 | `			PH7_ClassInstanceUnref(pThis);` |
|      3 |   739 | `			return rc;` |
|      - |   740 | `		}` |
|     25 |   741 | `	}` |
|     59 |   742 | `	return ReflectResultObject(pCtx, pThis);` |
|     33 |   743 | `}` |
|      - |   744 | `/* Where __reflect_new_no_ctor() was: the one caller was chunk 7's` |
|      - |   745 | ` * __reflect_build_attrs, which built a ReflectionAttribute and then filled it` |
|      - |   746 | ` * through a public __init() php does not have. C builds the instance itself` |
|      - |   747 | ` * (ReflectAttrNew), so both are gone. */` |
|      - |   748 | `/*` |
|      - |   749 | ` * Typed/readonly store enforcement for reflection writes. Like the VM's` |
|      - |   750 | ` * store path, except an UNINITIALIZED readonly property may be written from` |
|      - |   751 | ` * any scope (PHP lets ReflectionProperty::setValue initialize readonly): the` |
|      - |   752 | ` * READONLY bit is masked off for the enforcement call so the set-scope check` |
|      - |   753 | ` * is skipped, while an already-initialized readonly still gets PHP's` |
|      - |   754 | ` * "Cannot modify readonly property" Error. Returns SXRET_OK/PH7_EXCEPTION/` |
|      - |   755 | ` * PH7_ABORT; the value may be coerced in place.` |
|      - |   756 | ` */` |
|     22 |   757 | `static sxi32 ReflectEnforceStore(ph7_context *pCtx, sxu32 nIdx, ph7_value *pValue)` |
|      1 |   758 | `{` |
|     23 |   759 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |   760 | `	SyHashEntry *pSlot;` |
|      - |   761 | `	VmClassAttr *pVmAttr;` |
|      - |   762 | `	ph7_class_attr *pAttr;` |
|      - |   763 | `	sxi32 iSaved, rc;` |
|     23 |   764 | `	pSlot = SyHashGet(&pVm->hTypedSlot, (const void *)&nIdx, sizeof(sxu32));` |
|     23 |   765 | `	if( pSlot == 0 ){` |
|     13 |   766 | `		return SXRET_OK; /* Untyped slot: plain store */` |
|      - |   767 | `	}` |
|     11 |   768 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     11 |   769 | `	pAttr = pVmAttr->pAttr;` |
|     11 |   770 | `	if( pAttr == 0 ){` |
|    ! 0 |   771 | `		return SXRET_OK;` |
|      - |   772 | `	}` |
|     11 |   773 | `	iSaved = pAttr->iFlags;` |
|     11 |   774 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) ){` |
|    ! 0 |   775 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_READONLY;` |
|    ! 0 |   776 | `	}` |
|     11 |   777 | `	rc = PH7_VmEnforcePropStore(pVm, nIdx, pValue);` |
|     11 |   778 | `	pAttr->iFlags = iSaved;` |
|     11 |   779 | `	return rc;` |
|     12 |   780 | `}` |
|      - |   781 | `/* Hand an EXISTING instance to the caller: takes an extra reference` |
|      - |   782 | ` * (unlike ReflectResultObject, which transfers a fresh instance's one). */` |
|      2 |   783 | `static int ReflectResultExistingObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|      1 |   784 | `{` |
|      3 |   785 | `	if( pObj == 0 ){` |
|    ! 0 |   786 | `		ph7_result_null(pCtx);` |
|    ! 0 |   787 | `		return PH7_OK;` |
|      - |   788 | `	}` |
|      3 |   789 | `	PH7_MemObjRelease(pCtx->pRet);` |
|      3 |   790 | `	pObj->iRef++;` |
|      3 |   791 | `	pCtx->pRet->x.pOther = pObj;` |
|      3 |   792 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|      3 |   793 | `	return PH7_OK;` |
|      2 |   794 | `}` |
|      - |   795 | `/* pVal is a Closure instance? Return it, else NULL. */` |
|   7488 |   796 | `static ph7_class_instance * ReflectValueClosure(ph7_vm *pVm, ph7_value *pVal)` |
|      5 |   797 | `{` |
|      - |   798 | `	ph7_class_instance *pThis;` |
|   7493 |   799 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
|   5317 |   800 | `		return 0;` |
|      - |   801 | `	}` |
|   2181 |   802 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|   2181 |   803 | `	return (pThis->pClass == pVm->pClosureClass) ? pThis : 0;` |
|   3743 |   804 | `}` |
|      - |   805 | `/*` |
|      - |   806 | ` * Resolve a reflection callable target into its compiled function.` |
|      - |   807 | ` *   - pMethodArg a non-empty string  -> method mode: pTarget is a class name` |
|      - |   808 | ` *     or object; outputs *ppClass and *ppMeth.` |
|      - |   809 | ` *   - pTarget a Closure              -> unwrap $__fn into hFunction; *ppClosure.` |
|      - |   810 | ` *   - pTarget a string               -> hFunction (user) or hHostFunction` |
|      - |   811 | ` *     (*ppHost set, returns NULL).` |
|      - |   812 | ` * Returns the ph7_vm_func, or NULL (host function or unresolvable).` |
|      - |   813 | ` */` |
|  11266 |   814 | `static ph7_vm_func * ReflectResolveCallable(ph7_vm *pVm, ph7_value *pTarget,` |
|      - |   815 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|      - |   816 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|      5 |   817 | `{` |
|      - |   818 | `	SyHashEntry *pEntry;` |
|  11271 |   819 | `	if( ppClass ){ *ppClass = 0; }` |
|  11271 |   820 | `	if( ppMeth ){ *ppMeth = 0; }` |
|  11271 |   821 | `	if( ppHost ){ *ppHost = 0; }` |
|  11271 |   822 | `	if( ppClosure ){ *ppClosure = 0; }` |
|  11271 |   823 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
|   5625 |   824 | `		ph7_class *pClass = ReflectResolveClass(pVm, pTarget);` |
|      - |   825 | `		ph7_class_method *pMeth;` |
|   5625 |   826 | `		if( pClass == 0 ){` |
|    ! 0 |   827 | `			return 0;` |
|      - |   828 | `		}` |
|   8435 |   829 | `		pMeth = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|   2810 |   830 | `			SyBlobLength(&pMethodArg->sBlob));` |
|   5625 |   831 | `		if( pMeth && (pMeth->iFlags & PH7_CLASS_ATTR_FABRICATED) ){` |
|      - |   832 | ``			/* `Closure::__invoke` named over an OBJECT is the one method whose IDENTITY`` |
|      - |   833 | `			 * and whose BODY come from different places: php builds an internal method` |
|      - |   834 | `			 * record on the Closure class and copies the closure's parameter list into` |
|      - |   835 | `			 * it. So report the method (name, class, modifiers) and the closure's own` |
|      - |   836 | `			 * function (parameters, return type, what invoke() runs) together. Named` |
|      - |   837 | `			 * over a class instead, there is no closure and the declaration -- which is` |
|      - |   838 | `` 			 * empty -- is all there is, which is why php's class-level `__invoke` `` |
|      - |   839 | `			 * answers zero parameters. */` |
|    105 |   840 | `			ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|    105 |   841 | `			if( pClo ){` |
|     99 |   842 | `				ph7_vm_func *pBody = ReflectResolveCallable(pVm, pTarget, 0, 0, 0, 0, 0);` |
|     99 |   843 | `				if( pBody ){` |
|     99 |   844 | `					if( ppClass ){ *ppClass = pClass; }` |
|     99 |   845 | `					if( ppMeth ){ *ppMeth = pMeth; }` |
|     99 |   846 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|     99 |   847 | `					return pBody;` |
|      - |   848 | `				}` |
|    ! 0 |   849 | `			}` |
|      3 |   850 | `		}` |
|   5527 |   851 | `		if( pMeth == 0 ){` |
|      - |   852 | `			/* getMethods()/ReflectionClass reports a base class's PRIVATE methods on` |
|      - |   853 | `			 * the subclass (php copies them into the child's table), but private` |
|      - |   854 | `			 * methods are not inherited into the child's method table, so the plain` |
|      - |   855 | `			 * extract misses them when a ReflectionMethod obtained from the subclass` |
|      - |   856 | `			 * is re-resolved by (subclass, name). Walk the base chain to find the` |
|      - |   857 | `			 * declaring class's own copy. */` |
|    ! 0 |   858 | `			ph7_class *pWalk = pClass->pBase;` |
|    ! 0 |   859 | `			while( pWalk && pMeth == 0 ){` |
|    ! 0 |   860 | `				pMeth = PH7_ClassExtractMethod(pWalk, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|    ! 0 |   861 | `					SyBlobLength(&pMethodArg->sBlob));` |
|    ! 0 |   862 | `				pWalk = pWalk->pBase;` |
|    ! 0 |   863 | `			}` |
|    ! 0 |   864 | `		}` |
|   5527 |   865 | `		if( pMeth == 0 ){` |
|    ! 0 |   866 | `			return 0;` |
|      - |   867 | `		}` |
|   5527 |   868 | `		if( ppClass ){ *ppClass = pClass; }` |
|   5527 |   869 | `		if( ppMeth ){ *ppMeth = pMeth; }` |
|   5527 |   870 | `		return &pMeth->sFunc;` |
|      - |   871 | `	}` |
|      - |   872 | `	{` |
|   5651 |   873 | `		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|   5651 |   874 | `		if( pClo ){` |
|      - |   875 | `			SyString sAttr;` |
|      - |   876 | `			ph7_value *pFn;` |
|   1291 |   877 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|   1291 |   878 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|   1291 |   879 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) < 1 ){` |
|    ! 0 |   880 | `				return 0;` |
|      - |   881 | `			}` |
|      - |   882 | `			/* A closure over an object method or __invoke object` |
|      - |   883 | `			 * (Closure::fromCallable([$obj,'m']) / fromCallable($invokeObj)) stores the` |
|      - |   884 | `			 * bare method name in $__fn and the class in $__scope. Resolve it as a METHOD` |
|      - |   885 | `			 * of $__scope FIRST — before the global function table — so a same-named` |
|      - |   886 | `			 * global function does not shadow the method (the bug: $__fn "add" hitting a` |
|      - |   887 | `			 * global add()). A plain anonymous closure's $__fn is its unique lambda name,` |
|      - |   888 | `			 * which is not a method, so this falls through cleanly. */` |
|      - |   889 | `			{` |
|      - |   890 | `				SyString sScope;` |
|      - |   891 | `				ph7_value *pScope;` |
|   1291 |   892 | `				SyStringInitFromBuf(&sScope, "__scope", 7);` |
|   1291 |   893 | `				pScope = PH7_ClassInstanceFetchAttr(pClo, &sScope);` |
|   1291 |   894 | `				if( pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0 ){` |
|   1331 |   895 | `					ph7_class *pScopeCls = PH7_VmExtractClass(pVm,` |
|    884 |   896 | `						(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|    889 |   897 | `					if( pScopeCls ){` |
|   1331 |   898 | `						ph7_class_method *pScopeMeth = PH7_ClassExtractMethod(pScopeCls,` |
|    884 |   899 | `							(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    889 |   900 | `						if( pScopeMeth ){` |
|    336 |   901 | `							if( ppClass ){ *ppClass = pScopeCls; }` |
|    336 |   902 | `							if( ppMeth ){ *ppMeth = pScopeMeth; }` |
|    336 |   903 | `							if( ppClosure ){ *ppClosure = pClo; }` |
|    336 |   904 | `							return &pScopeMeth->sFunc;` |
|      - |   905 | `						}` |
|    276 |   906 | `					}` |
|    276 |   907 | `				}` |
|      - |   908 | `			}` |
|    959 |   909 | `			pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    959 |   910 | `			if( pEntry == 0 ){` |
|      - |   911 | `				/* A Closure over a host function (Closure::fromCallable('strlen')) */` |
|    410 |   912 | `				pEntry = PH7_VmGetHostFunction(pVm, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob), FALSE);` |
|    410 |   913 | `				if( pEntry && ppHost ){` |
|     12 |   914 | `					*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|     12 |   915 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|      5 |   916 | `				}` |
|    410 |   917 | `				return 0;` |
|      - |   918 | `			}` |
|    553 |   919 | `			if( ppClosure ){ *ppClosure = pClo; }` |
|    553 |   920 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|      - |   921 | `		}` |
|      - |   922 | `	}` |
|   4365 |   923 | `	if( pTarget->iFlags & MEMOBJ_STRING ){` |
|   4365 |   924 | `		if( SyBlobLength(&pTarget->sBlob) < 1 ){` |
|    ! 0 |   925 | `			return 0;` |
|      - |   926 | `		}` |
|   4365 |   927 | `		pEntry = PH7_VmGetUserFunction(pVm, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob), FALSE);` |
|   4365 |   928 | `		if( pEntry ){` |
|   1073 |   929 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|      - |   930 | `		}` |
|   3297 |   931 | `		pEntry = PH7_VmGetHostFunction(pVm, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob), FALSE);` |
|   3297 |   932 | `		if( pEntry && ppHost ){` |
|   3279 |   933 | `			*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|   1633 |   934 | `		}` |
|   1642 |   935 | `	}` |
|   3297 |   936 | `	return 0;` |
|   5634 |   937 | `}` |
|      - |   938 | `/*` |
|      - |   939 | ` * ---------------------------------------------------------------------------` |
|      - |   940 | ` * The signature parser.` |
|      - |   941 | ` *` |
|      - |   942 | ` * A C builtin and a native method declare their parameters as ONE php-style` |
|      - |   943 | `` * string (`"string $name, ?int $len = null"`) — the aBuiltinSig[] row or the`` |
|      - |   944 | ` * PH7_NativeClassSpec's zSig — because neither has a compiled parameter list to` |
|      - |   945 | ` * read. Reflection needs that string as the same param-meta shape a compiled` |
|      - |   946 | ` * function produces, so it is parsed here and the descriptor comes out of` |
|      - |   947 | ` * __reflect_func_info() already uniform.` |
|      - |   948 | ` *` |
|      - |   949 | ` * This was chunk 8 of the reflection prelude (__reflect_sig_split /` |
|      - |   950 | ` * __reflect_sig_scalar / __reflect_parse_sig / __reflect_sig_fixup), which every` |
|      - |   951 | ` * caller had to remember to wrap around __reflect_func_info() — six call sites,` |
|      - |   952 | ` * and the one that FORGOT is why every native method reported zero parameters` |
|      - |   953 | ` * until 31 Jul. Producing the parsed form at the source removes the wrapper and` |
|      - |   954 | ` * the possibility of forgetting it.` |
|      - |   955 | ` * ---------------------------------------------------------------------------` |
|      - |   956 | ` */` |
|      - |   957 | `/* Trim ASCII spaces off both ends of [z, z+n). */` |
|  37640 |   958 | `static void ReflectSigTrim(const char **pz, int *pn)` |
|      5 |   959 | `{` |
|  37645 |   960 | `	const char *z = *pz;` |
|  37645 |   961 | `	int n = *pn;` |
|  73429 |   962 | `	while( n > 0 && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|  16969 |   963 | `		z++;` |
|  16969 |   964 | `		n--;` |
|      5 |   965 | `	}` |
|  59383 |   966 | `	while( n > 0 && (z[n-1] == ' ' \|\| z[n-1] == '\t') ){` |
|   2923 |   967 | `		n--;` |
|      5 |   968 | `	}` |
|  37645 |   969 | `	*pz = z;` |
|  37645 |   970 | `	*pn = n;` |
|  37645 |   971 | `}` |
|      - |   972 | ``/* Byte search that respects single-quoted runs (a default may be `'a,b'` or`` |
|      - |   973 | `` * `'it\'s'`), which is the whole reason the split is not SyByteFind. Answers`` |
|      - |   974 | ` * the offset of the first unquoted zWhat, or -1. */` |
|  50538 |   975 | `static int ReflectSigFindUnquoted(const char *z, int n, char cWhat)` |
|      5 |   976 | `{` |
|      - |   977 | `	int k;` |
|  50543 |   978 | `	char cQuote = 0;` |
| 770991 |   979 | `	for( k = 0 ; k < n ; k++ ){` |
| 750525 |   980 | `		if( cQuote ){` |
|   5385 |   981 | `			if( z[k] == '\\' && k + 1 < n ){` |
|    960 |   982 | `				k++;` |
|   4906 |   983 | `			}else if( z[k] == cQuote ){` |
|   2051 |   984 | `				cQuote = 0;` |
|   1028 |   985 | `			}` |
| 747835 |   986 | `		}else if( z[k] == '\'' \|\| z[k] == '"' ){` |
|      - |   987 | `			/* BOTH spellings open a run. A native method's zSig lives in C, so` |
|      - |   988 | ``			 * `string $separator = ","` is the natural way to write it -- and`` |
|      - |   989 | `			 * tracking only the single quote let the comma INSIDE that default` |
|      - |   990 | `			 * split the parameter in two, which is how setCsvControl() came to` |
|      - |   991 | ``			 * report four parameters, one of them named `$"`. */`` |
|   2051 |   992 | `			cQuote = z[k];` |
| 744122 |   993 | `		}else if( z[k] == cWhat ){` |
|  30077 |   994 | `			return k;` |
|      - |   995 | `		}` |
| 360217 |   996 | `	}` |
|  20471 |   997 | `	return -1;` |
|  25271 |   998 | `}` |
|      - |   999 | `/* Does [z,n) contain zNeedle? (case-sensitive; nNeedle > 0) */` |
|  18656 |  1000 | `static int ReflectSigHas(const char *z, int n, const char *zNeedle, int nNeedle)` |
|      5 |  1001 | `{` |
|      - |  1002 | `	int k;` |
| 227857 |  1003 | `	for( k = 0 ; k + nNeedle <= n ; k++ ){` |
| 209797 |  1004 | `		if( SyMemcmp((const void *)&z[k], (const void *)zNeedle, (sxu32)nNeedle) == 0 ){` |
|    601 |  1005 | `			return 1;` |
|      - |  1006 | `		}` |
| 104603 |  1007 | `	}` |
|  18065 |  1008 | `	return 0;` |
|   9333 |  1009 | `}` |
|      - |  1010 | ``/* Case-insensitive twin, for the `null` arm of a union type text. */`` |
|   6427 |  1011 | `static int ReflectSigHasNoCase(const char *z, int n, const char *zNeedle, int nNeedle)` |
|      5 |  1012 | `{` |
|      - |  1013 | `	int k, j;` |
|  27293 |  1014 | `	for( k = 0 ; k + nNeedle <= n ; k++ ){` |
|  22104 |  1015 | `		for( j = 0 ; j < nNeedle ; j++ ){` |
|  22060 |  1016 | `			if( SyToLower(z[k+j]) != SyToLower(zNeedle[j]) ){` |
|  20866 |  1017 | `				break;` |
|      - |  1018 | `			}` |
|    602 |  1019 | `		}` |
|  20910 |  1020 | `		if( j == nNeedle ){` |
|     48 |  1021 | `			return 1;` |
|      - |  1022 | `		}` |
|  10434 |  1023 | `	}` |
|   6388 |  1024 | `	return 0;` |
|   3217 |  1025 | `}` |
|      - |  1026 | `/*` |
|      - |  1027 | `` * One `C::K` (or `C::class`) term of a declared default, evaluated.`` |
|      - |  1028 | ` *` |
|      - |  1029 | `` * php's stub writes these as SOURCE — `string $class = SplFileInfo::class`,`` |
|      - |  1030 | `` * `int $flags = FilesystemIterator::KEY_AS_PATHNAME\|…` — and the reflector then`` |
|      - |  1031 | ` * answers both the text (the export line) and the VALUE (getDefaultValue()). A` |
|      - |  1032 | ` * native method's zSig is that same source, so the value has to be read out of` |
|      - |  1033 | ` * the class here; there are no compiled parameter records to hold it.` |
|      - |  1034 | ` */` |
|   1360 |  1035 | `static int ReflectSigClassConst(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      5 |  1036 | `{` |
|   1365 |  1037 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  1038 | `	ph7_class_attr *pAttr;` |
|      - |  1039 | `	ph7_class *pClass;` |
|      - |  1040 | `	ph7_value *pValue;` |
|      - |  1041 | `	int iSep;` |
|   1365 |  1042 | `	ReflectSigTrim(&z, &n);` |
|   6473 |  1043 | `	for( iSep = 0 ; iSep + 1 < n ; ++iSep ){` |
|   5219 |  1044 | `		if( z[iSep] == ':' && z[iSep+1] == ':' ){` |
|    109 |  1045 | `			break;` |
|      - |  1046 | `		}` |
|   2559 |  1047 | `	}` |
|   1365 |  1048 | `	if( iSep + 1 >= n \|\| iSep < 1 ){` |
|   1259 |  1049 | `		return 0;` |
|      - |  1050 | `	}` |
|    109 |  1051 | `	pClass = PH7_VmExtractClass(pVm, z, (sxu32)iSep, TRUE, 0);` |
|    109 |  1052 | `	if( pClass == 0 ){` |
|    ! 0 |  1053 | `		return 0;` |
|      - |  1054 | `	}` |
|    109 |  1055 | `	z += iSep + 2;` |
|    109 |  1056 | `	n -= iSep + 2;` |
|    109 |  1057 | `	if( n == (int)sizeof("class")-1 && SyMemcmp(z, "class", sizeof("class")-1) == 0 ){` |
|      - |  1058 | ``		/* `C::class` is the class NAME, and php prints the name it was DECLARED`` |
|      - |  1059 | `		 * with rather than the spelling in the signature. */` |
|     10 |  1060 | `		SyString *pName = &pClass->sName;` |
|     10 |  1061 | `		ph7_value_string(pOut, SyStringData(pName), (int)SyStringLength(pName));` |
|     10 |  1062 | `		return 1;` |
|      - |  1063 | `	}` |
|    101 |  1064 | `	pAttr = PH7_ClassExtractConstant(pClass, z, (sxu32)n);` |
|    101 |  1065 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|    ! 0 |  1066 | `		return 0;` |
|      - |  1067 | `	}` |
|    101 |  1068 | `	if( pAttr->nIdx == SXU32_HIGH ){` |
|      - |  1069 | `` 		/* Not materialized yet: bring it into being exactly as a direct `C::K` `` |
|      - |  1070 | `		 * read would. An enum CASE is a singleton with its own materializer --` |
|      - |  1071 | `		 * running the constant initializer over one answers nothing, which is` |
|      - |  1072 | ``		 * why `RoundingMode::HalfAwayFromZero` could not be reduced at all. */`` |
|     35 |  1073 | `		sxi32 rcConst = (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)` |
|    ! 0 |  1074 | `			? VmEnumMaterializeCase(pVm, pClass, pAttr)` |
|     22 |  1075 | `			: VmClassConstEvalOnDemand(pVm, pClass, pAttr);` |
|     24 |  1076 | `		if( rcConst != SXRET_OK ){` |
|    ! 0 |  1077 | `			return 0;` |
|      - |  1078 | `		}` |
|     11 |  1079 | `	}` |
|    101 |  1080 | `	pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, pAttr->nIdx);` |
|    101 |  1081 | `	if( pValue == 0 ){` |
|    ! 0 |  1082 | `		return 0;` |
|      - |  1083 | `	}` |
|    101 |  1084 | `	PH7_MemObjStore(pValue, pOut);` |
|    101 |  1085 | `	return 1;` |
|    685 |  1086 | `}` |
|      - |  1087 | `/*` |
|      - |  1088 | ` * A GLOBAL constant named by a declared default — php's stub writes` |
|      - |  1089 | `` * `int $severity = E_ERROR` and both surfaces read it from here: the export`` |
|      - |  1090 | ` * prints the NAME (rule 28: the argument was never folded, so php still has the` |
|      - |  1091 | ` * source) and getDefaultValue() answers what it expands to. Answers 0 for` |
|      - |  1092 | ` * anything that is not a plain identifier naming a defined constant, which is` |
|      - |  1093 | `` * what keeps `null`/`true`/a bare number out of this branch.`` |
|      - |  1094 | ` */` |
|   3002 |  1095 | `static int ReflectSigIsIdent(const char *z, int n)` |
|      5 |  1096 | `{` |
|      - |  1097 | `	int k;` |
|   3007 |  1098 | `	if( n < 1 \|\| (z[0] != '_' && !SyisAlpha(z[0])) ){` |
|   1605 |  1099 | `		return 0;` |
|      - |  1100 | `	}` |
|   8718 |  1101 | `	for( k = 1 ; k < n ; ++k ){` |
|   7396 |  1102 | `		if( z[k] != '_' && z[k] != '\\' && !SyisAlphaNum(z[k]) ){` |
|     83 |  1103 | `			return 0;` |
|      - |  1104 | `		}` |
|   3660 |  1105 | `	}` |
|   1326 |  1106 | `	return 1;` |
|   1506 |  1107 | `}` |
|   3002 |  1108 | `static int ReflectSigGlobalConst(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      5 |  1109 | `{` |
|      - |  1110 | `	SyHashEntry *pEntry;` |
|      - |  1111 | `	ph7_constant *pCons;` |
|   3007 |  1112 | `	ReflectSigTrim(&z, &n);` |
|   3007 |  1113 | `	if( !ReflectSigIsIdent(z, n) ){` |
|   1685 |  1114 | `		return 0;` |
|      - |  1115 | `	}` |
|   1326 |  1116 | `	pEntry = PH7_VmConstantFetch(pCtx->pVm, z, (sxu32)n, 1);` |
|   1326 |  1117 | `	if( pEntry == 0 ){` |
|   1113 |  1118 | `		return 0;` |
|      - |  1119 | `	}` |
|    216 |  1120 | `	pCons = (ph7_constant *)pEntry->pUserData;` |
|    216 |  1121 | `	if( pCons == 0 \|\| pCons->xExpand == 0 ){` |
|    ! 0 |  1122 | `		return 0;` |
|      - |  1123 | `	}` |
|    216 |  1124 | `	pCons->xExpand(pOut, pCons->pUserData);` |
|    216 |  1125 | `	return 1;` |
|   1506 |  1126 | `}` |
|      - |  1127 | `/*` |
|      - |  1128 | `` * A constant EXPRESSION: one term, or the `\|` fold php's own stubs write for a`` |
|      - |  1129 | `` * flags default (`KEY_AS_PATHNAME \| CURRENT_AS_FILEINFO \| SKIP_DOTS`, and`` |
|      - |  1130 | `` * `SQLITE3_OPEN_READWRITE \| SQLITE3_OPEN_CREATE`). Either kind of name may`` |
|      - |  1131 | ` * stand in it -- a class constant or a global one -- because php's stubs write` |
|      - |  1132 | ` * both, and a term is looked up as whichever it turns out to be.` |
|      - |  1133 | ` */` |
|    498 |  1134 | `static int ReflectSigConstExpr(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      5 |  1135 | `{` |
|    503 |  1136 | `	sxi64 iAcc = 0;` |
|    503 |  1137 | `	int iStart = 0;` |
|    503 |  1138 | `	int k, nTerm = 0;` |
|    503 |  1139 | `	if( !ReflectSigHas(z, n, "::", 2) && !ReflectSigHas(z, n, "\|", 1) ){` |
|    437 |  1140 | `		return 0;` |
|      - |  1141 | `	}` |
|   2536 |  1142 | `	for( k = 0 ; k <= n ; ++k ){` |
|   2470 |  1143 | `		if( k < n && z[k] != '\|' ){` |
|   2342 |  1144 | `			continue;` |
|      - |  1145 | `		}` |
|    128 |  1146 | `		if( !ReflectSigClassConst(pCtx, &z[iStart], k - iStart, pOut)` |
|    110 |  1147 | `		 && !ReflectSigGlobalConst(pCtx, &z[iStart], k - iStart, pOut) ){` |
|    ! 0 |  1148 | `			return 0;` |
|      - |  1149 | `		}` |
|    130 |  1150 | `		nTerm++;` |
|    130 |  1151 | `		if( k < n \|\| nTerm > 1 ){` |
|      - |  1152 | `			/* A fold is arithmetic, so every arm has to be an integer; a lone` |
|      - |  1153 | ``			 * term keeps whatever type it had (`C::class` is a string). */`` |
|    100 |  1154 | `			if( (pOut->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) != 0 ){` |
|    ! 0 |  1155 | `				return 0;` |
|      - |  1156 | `			}` |
|    100 |  1157 | `			PH7_MemObjToInteger(pOut);` |
|    100 |  1158 | `			iAcc \|= pOut->x.iVal;` |
|     49 |  1159 | `		}` |
|    130 |  1160 | `		iStart = k + 1;` |
|     66 |  1161 | `	}` |
|     68 |  1162 | `	if( nTerm > 1 ){` |
|     38 |  1163 | `		ph7_value_int64(pOut, iAcc);` |
|     18 |  1164 | `	}` |
|     68 |  1165 | `	return nTerm > 0;` |
|    254 |  1166 | `}` |
|      - |  1167 | `/* One hex digit's value, or -1. */` |
|     54 |  1168 | `static int ReflectHexVal(char c)` |
|      2 |  1169 | `{` |
|     56 |  1170 | `	if( c >= '0' && c <= '9' ) return c - '0';` |
|    ! 0 |  1171 | `	if( c >= 'a' && c <= 'f' ) return c - 'a' + 10;` |
|    ! 0 |  1172 | `	if( c >= 'A' && c <= 'F' ) return c - 'A' + 10;` |
|    ! 0 |  1173 | `	return -1;` |
|     29 |  1174 | `}` |
|      - |  1175 | `/*` |
|      - |  1176 | ` * php keeps the SOURCE of a constant EXPRESSION in a stub's default and prints` |
|      - |  1177 | ` * it back verbatim, and two more shapes of one live in the signature table` |
|      - |  1178 | `` * beside the `C::K` and `A\|B` forms already handled: an integer written in a`` |
|      - |  1179 | `` * RADIX (`0777` for mkdir's permissions) and a product (`2 * 1024 * 1024` for`` |
|      - |  1180 | ` * SplTempFileObject's memory bound). Both are text php never reduces on the` |
|      - |  1181 | ` * page; both have to be reduced for getDefaultValue().` |
|      - |  1182 | ` */` |
|   1544 |  1183 | `static int ReflectSigRadixInt(const char *z, int n, sxi64 *pOut)` |
|      5 |  1184 | `{` |
|   1549 |  1185 | `	int iRadix = 8, k = 1;` |
|   1549 |  1186 | `	sxi64 iVal = 0;` |
|   1549 |  1187 | `	ReflectSigTrim(&z, &n);` |
|   1549 |  1188 | `	if( n < 2 \|\| z[0] != '0' ){` |
|   1543 |  1189 | `		return 0;` |
|      - |  1190 | `	}` |
|      8 |  1191 | `	if( z[1] == 'x' \|\| z[1] == 'X' ){       iRadix = 16; k = 2; }` |
|      8 |  1192 | `	else if( z[1] == 'o' \|\| z[1] == 'O' ){  iRadix = 8;  k = 2; }` |
|      8 |  1193 | `	else if( z[1] == 'b' \|\| z[1] == 'B' ){  iRadix = 2;  k = 2; }` |
|      8 |  1194 | `	if( k >= n ){` |
|    ! 0 |  1195 | `		return 0;` |
|      - |  1196 | `	}` |
|     26 |  1197 | `	for( ; k < n ; ++k ){` |
|     20 |  1198 | `		int iDigit = ReflectHexVal(z[k]);` |
|     20 |  1199 | `		if( iDigit < 0 \|\| iDigit >= iRadix ){` |
|    ! 0 |  1200 | `			return 0;` |
|      - |  1201 | `		}` |
|     20 |  1202 | `		iVal = iVal * iRadix + iDigit;` |
|     11 |  1203 | `	}` |
|      8 |  1204 | `	*pOut = iVal;` |
|      8 |  1205 | `	return 1;` |
|    777 |  1206 | `}` |
|      - |  1207 | ``/* `a * b * c` over integer literals and integer constants. */`` |
|    416 |  1208 | `static int ReflectSigProduct(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      5 |  1209 | `{` |
|    421 |  1210 | `	sxi64 iAcc = 1;` |
|    421 |  1211 | `	int iStart = 0, k, nTerm = 0;` |
|    421 |  1212 | `	if( ReflectSigFindUnquoted(z, n, '*') < 0 ){` |
|    419 |  1213 | `		return 0;` |
|      - |  1214 | `	}` |
|     35 |  1215 | `	for( k = 0 ; k <= n ; ++k ){` |
|      - |  1216 | `		const char *zTerm;` |
|      - |  1217 | `		int nTermLen;` |
|     33 |  1218 | `		sxi64 iVal = 0;` |
|     33 |  1219 | `		sxu8 bReal = 0;` |
|     33 |  1220 | `		if( k < n && z[k] != '*' ){` |
|     27 |  1221 | `			continue;` |
|      - |  1222 | `		}` |
|      7 |  1223 | `		zTerm = &z[iStart];` |
|      7 |  1224 | `		nTermLen = k - iStart;` |
|      7 |  1225 | `		ReflectSigTrim(&zTerm, &nTermLen);` |
|      7 |  1226 | `		if( nTermLen < 1 ){` |
|    ! 0 |  1227 | `			return 0;` |
|      - |  1228 | `		}` |
|      7 |  1229 | `		if( !ReflectSigRadixInt(zTerm, nTermLen, &iVal) ){` |
|      - |  1230 | `			ph7_value *pTerm;` |
|      7 |  1231 | `			if( SyStrIsNumeric(zTerm, (sxu32)nTermLen, &bReal, 0) == SXRET_OK && !bReal ){` |
|      7 |  1232 | `				SyStrToInt64(zTerm, (sxu32)nTermLen, (void *)&iVal, 0);` |
|      4 |  1233 | `			}else if( (pTerm = ph7_context_new_scalar(pCtx)) != 0` |
|    ! 0 |  1234 | `			       && (ReflectSigClassConst(pCtx, zTerm, nTermLen, pTerm)` |
|    ! 0 |  1235 | `			        \|\| ReflectSigGlobalConst(pCtx, zTerm, nTermLen, pTerm))` |
|    ! 0 |  1236 | `			       && (pTerm->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|    ! 0 |  1237 | `				PH7_MemObjToInteger(pTerm);` |
|    ! 0 |  1238 | `				iVal = pTerm->x.iVal;` |
|    ! 0 |  1239 | `			}else{` |
|    ! 0 |  1240 | `				return 0;` |
|      - |  1241 | `			}` |
|      3 |  1242 | `		}` |
|      7 |  1243 | `		iAcc *= iVal;` |
|      7 |  1244 | `		nTerm++;` |
|      7 |  1245 | `		iStart = k + 1;` |
|      4 |  1246 | `	}` |
|      3 |  1247 | `	if( nTerm < 2 ){` |
|    ! 0 |  1248 | `		return 0;` |
|      - |  1249 | `	}` |
|      3 |  1250 | `	ph7_value_int64(pOut, iAcc);` |
|      3 |  1251 | `	return 1;` |
|    213 |  1252 | `}` |
|      - |  1253 | `/*` |
|      - |  1254 | ` * Is this default TEXT one php prints back as SOURCE rather than as a value?` |
|      - |  1255 | `` * The `C::K` and `A\|B` forms are answered by their own readers above; these`` |
|      - |  1256 | ` * two are the ones a plain literal reader would silently reduce to a number.` |
|      - |  1257 | ` */` |
|   1128 |  1258 | `static int ReflectSigIsSourceExpr(const char *z, int n)` |
|      5 |  1259 | `{` |
|   1133 |  1260 | `	sxi64 iIgnored = 0;` |
|   1695 |  1261 | `	return ReflectSigFindUnquoted(z, n, '*') >= 0` |
|   1128 |  1262 | `	    \|\| ReflectSigRadixInt(z, n, &iIgnored);` |
|      5 |  1263 | `}` |
|      - |  1264 | `/*` |
|      - |  1265 | ` * A default-value TEXT to a value, when the text denotes a scalar php can` |
|      - |  1266 | `` * reproduce. Answers 1 and fills pOut, or 0 for anything else (`[]`,`` |
|      - |  1267 | `` * `array (`, a constant name) which the caller reports its own way.`` |
|      - |  1268 | ` */` |
|   1396 |  1269 | `static int ReflectSigScalar(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      5 |  1270 | `{` |
|   1401 |  1271 | `	sxu8 bReal = 0;` |
|   1401 |  1272 | `	if( n == 1 && z[0] == '?' ){` |
|    ! 0 |  1273 | `		return 0;` |
|      - |  1274 | `	}` |
|   1401 |  1275 | `	if( (n == 4 && (SyMemcmp(z,"NULL",4) == 0 \|\| SyMemcmp(z,"null",4) == 0)) ){` |
|    494 |  1276 | `		ph7_value_null(pOut);` |
|    494 |  1277 | `		return 1;` |
|      - |  1278 | `	}` |
|    911 |  1279 | `	if( n == 4 && SyMemcmp(z,"true",4) == 0 ){` |
|     59 |  1280 | `		ph7_value_bool(pOut,1);` |
|     59 |  1281 | `		return 1;` |
|      - |  1282 | `	}` |
|    855 |  1283 | `	if( n == 5 && SyMemcmp(z,"false",5) == 0 ){` |
|    119 |  1284 | `		ph7_value_bool(pOut,0);` |
|    119 |  1285 | `		return 1;` |
|      - |  1286 | `	}` |
|    739 |  1287 | `	if( n >= 2 && (z[0] == '\'' \|\| z[0] == '"') && z[n-1] == z[0] ){` |
|      - |  1288 | `		/* Undo the escapes the signature writer emits, which are php's own set --` |
|      - |  1289 | `		 * the same one ReflectExportStrQ puts back when it prints the line. A row` |
|      - |  1290 | `		 * cannot hold these bytes any other way: a signature is a C string, so a` |
|      - |  1291 | ``		 * NUL would END it, which is why trim()'s ` \n\r\t\v\x00` had no default`` |
|      - |  1292 | `		 * row at all and its parameter answered isDefaultValueAvailable() false.` |
|      - |  1293 | `		 * BOTH quote spellings are accepted because both scanners in vm_arg_check.c` |
|      - |  1294 | `` 		 * step over either one: a native method's zSig lives in C, so `= \"static\"` `` |
|      - |  1295 | `		 * is the natural way to write Closure::bindTo's default. */` |
|      - |  1296 | `		SyBlob sOut;` |
|    255 |  1297 | `		char cQuote = z[0];` |
|      - |  1298 | `		int k;` |
|    255 |  1299 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    627 |  1300 | `		for( k = 1 ; k < n - 1 ; k++ ){` |
|    377 |  1301 | `			char c = z[k];` |
|    377 |  1302 | `			if( c == '\\' && k + 1 < n - 1 ){` |
|    162 |  1303 | `				char e = z[k+1];` |
|    162 |  1304 | `				int bTook = 1;` |
|    162 |  1305 | `				switch( e ){` |
|     31 |  1306 | `					case 'n': c = 0x0A; break;` |
|     27 |  1307 | `					case 'r': c = 0x0D; break;` |
|     23 |  1308 | `					case 't': c = 0x09; break;` |
|     23 |  1309 | `					case 'v': c = 0x0B; break;` |
|      5 |  1310 | `					case 'f': c = 0x0C; break;` |
|    ! 0 |  1311 | `					case 'e': c = 0x1B; break;` |
|      9 |  1312 | `					case 'x': {` |
|     19 |  1313 | `						int h1 = ReflectHexVal(k + 3 < n - 1 ? z[k+2] : 0);` |
|     19 |  1314 | `						int h2 = ReflectHexVal(k + 3 < n - 1 ? z[k+3] : 0);` |
|     19 |  1315 | `						if( h1 < 0 \|\| h2 < 0 ){` |
|    ! 0 |  1316 | `							bTook = 0;` |
|    ! 0 |  1317 | `							break;` |
|      - |  1318 | `						}` |
|     19 |  1319 | `						c = (char)((h1 << 4) \| h2);` |
|     19 |  1320 | `						k += 2;   /* the two hex digits; the 'x' below */` |
|     19 |  1321 | `						break;` |
|      - |  1322 | `					}` |
|     19 |  1323 | `					default:` |
|     40 |  1324 | `						bTook = (e == cQuote \|\| e == '\\');` |
|     40 |  1325 | `						c = e;` |
|     38 |  1326 | `						break;` |
|      - |  1327 | `				}` |
|    162 |  1328 | `				if( bTook ){` |
|    162 |  1329 | `					k++;` |
|     82 |  1330 | `				}else{` |
|    ! 0 |  1331 | `					c = '\\';` |
|      - |  1332 | `				}` |
|     80 |  1333 | `			}` |
|    377 |  1334 | `			SyBlobAppend(&sOut,(const void *)&c,sizeof(char));` |
|    191 |  1335 | `		}` |
|    255 |  1336 | `		ph7_value_string(pOut,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    255 |  1337 | `		SyBlobRelease(&sOut);` |
|    255 |  1338 | `		return 1;` |
|      - |  1339 | `	}` |
|    489 |  1340 | `	if( ReflectSigConstExpr(pCtx,z,n,pOut) ){` |
|     54 |  1341 | `		return 1;` |
|      - |  1342 | `	}` |
|    437 |  1343 | `	if( ReflectSigGlobalConst(pCtx,z,n,pOut) ){` |
|     18 |  1344 | `		return 1;` |
|      - |  1345 | `	}` |
|    421 |  1346 | `	if( ReflectSigProduct(pCtx,z,n,pOut) ){` |
|      3 |  1347 | `		return 1;` |
|      - |  1348 | `	}` |
|      - |  1349 | `	{` |
|    419 |  1350 | `		sxi64 iRadix = 0;` |
|    419 |  1351 | `		if( ReflectSigRadixInt(z,n,&iRadix) ){` |
|      6 |  1352 | `			ph7_value_int64(pOut,iRadix);` |
|      6 |  1353 | `			return 1;` |
|      - |  1354 | `		}` |
|      - |  1355 | `	}` |
|    415 |  1356 | `	if( n > 0 && SyStrIsNumeric(z,(sxu32)n,&bReal,0) == SXRET_OK ){` |
|      - |  1357 | `		/* php's own rule for the text form: a '.', an exponent or a hex marker` |
|      - |  1358 | `		 * makes it a float, everything else an int. */` |
|    348 |  1359 | `		if( bReal \|\| ReflectSigHas(z,n,".",1)` |
|    352 |  1360 | `		 \|\| ReflectSigHasNoCase(z,n,"e",1) \|\| ReflectSigHasNoCase(z,n,"x",1) ){` |
|      - |  1361 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - |  1362 | `			/* the VALUE is an out-parameter here; the return is a status, and` |
|      - |  1363 | `			 * reading it as the number made every float default 0.0 */` |
|      3 |  1364 | `			sxreal rVal = 0.0;` |
|      3 |  1365 | `			SyStrToReal(z,(sxu32)n,(void *)&rVal,0);` |
|      3 |  1366 | `			ph7_value_double(pOut,rVal);` |
|      - |  1367 | `#else` |
|      - |  1368 | `			sxi64 iRaw = 0;` |
|      - |  1369 | `			SyStrToInt64(z,(sxu32)n,(void *)&iRaw,0);` |
|      - |  1370 | `			ph7_value_int64(pOut,iRaw);` |
|      - |  1371 | `#endif` |
|      2 |  1372 | `		}else{` |
|    350 |  1373 | `			sxi64 iVal = 0;` |
|    350 |  1374 | `			SyStrToInt64(z,(sxu32)n,(void *)&iVal,0);` |
|    350 |  1375 | `			ph7_value_int64(pOut,iVal);` |
|      - |  1376 | `		}` |
|    353 |  1377 | `		return 1;` |
|      - |  1378 | `	}` |
|     65 |  1379 | `	return 0;` |
|    703 |  1380 | `}` |
|      - |  1381 | `/*` |
|      - |  1382 | ` * The same reduction, for the door OUTSIDE this file that needs it.` |
|      - |  1383 | ` *` |
|      - |  1384 | ` * A declared signature's default is TEXT, and two places have to turn it into a` |
|      - |  1385 | ` * value: getDefaultValue() above, and the named-argument binder in` |
|      - |  1386 | ` * vm_arg_check.c, which materializes the parameters a named call SKIPPED` |
|      - |  1387 | `` * (`mkdir($d, recursive: true)` has to supply `$permissions`). That binder`` |
|      - |  1388 | `` * carried a reader of its own -- null/true/false, `[]`, a quoted string and a`` |
|      - |  1389 | ` * DECIMAL number -- so the two doors onto one value disagreed on every default` |
|      - |  1390 | `` * the small reader could not spell: `0777` bound as 777 (a directory created`` |
|      - |  1391 | ` * mode 01411, which the next chdir() could not enter), and any default written` |
|      - |  1392 | `` * as a CONSTANT (`ENT_QUOTES \| …`, `M_E`, `SORT_REGULAR`, `SplFileObject::class`,`` |
|      - |  1393 | `` * `2 * 1024 * 1024` -- ~180 parameters) bound as "not passed", so php's own`` |
|      - |  1394 | `` * `htmlspecialchars("<a>", encoding: 'UTF-8')` was an ArgumentCountError here.`` |
|      - |  1395 | `` * There is one reader now and this is its door; `[]` stays with the caller`` |
|      - |  1396 | ` * because the value it builds is a hashmap rather than a scalar.` |
|      - |  1397 | ` */` |
|     30 |  1398 | `PH7_PRIVATE int PH7_VmSigDefaultToValue(ph7_context *pCtx,const char *z,int n,ph7_value *pOut)` |
|      3 |  1399 | `{` |
|     33 |  1400 | `	if( pCtx == 0 \|\| z == 0 \|\| n < 1 \|\| pOut == 0 ){` |
|    ! 0 |  1401 | `		return 0;` |
|      - |  1402 | `	}` |
|     33 |  1403 | `	return ReflectSigScalar(pCtx,z,n,pOut);` |
|     18 |  1404 | `}` |
|      - |  1405 | `/*` |
|      - |  1406 | ` * One parameter, described uniformly.` |
|      - |  1407 | ` *` |
|      - |  1408 | ` * A reflected function's parameters come from one of TWO places — a compiled` |
|      - |  1409 | `` * function's `ph7_vm_func_arg` list, or the php-style SIGNATURE STRING a C`` |
|      - |  1410 | ` * builtin / native method declares — and both the descriptor array` |
|      - |  1411 | ` * (__reflect_func_info, for the chunks still in PHP) and the native` |
|      - |  1412 | ` * ReflectionParameter have to read them the same way. This struct is what they` |
|      - |  1413 | `` * agree on; `pArg` is set only on the compiled path, where a default is`` |
|      - |  1414 | ` * BYTE-CODE rather than text.` |
|      - |  1415 | ` */` |
|      - |  1416 | `typedef struct ReflectParamDesc ReflectParamDesc;` |
|      - |  1417 | `struct ReflectParamDesc` |
|      - |  1418 | `{` |
|      - |  1419 | `	SyString sName;` |
|      - |  1420 | `	int iPos;` |
|      - |  1421 | `	int bByRef, bVariadic, bHasDef, bOptional, bNullable, bPromoted;` |
|      - |  1422 | `	SyString sType;            /* nByte == 0 -> untyped. For a COMPILED parameter this` |
|      - |  1423 | `	                            * points into zTypeBuf below, php's stored text with` |
|      - |  1424 | ``	                            * `self`/`parent` resolved (see ReflectDeclScope). */`` |
|      - |  1425 | `	char zTypeBuf[192];` |
|      - |  1426 | `	SyString sDefText;         /* signature-declared default TEXT (nByte == 0 -> none) */` |
|      - |  1427 | `	ph7_vm_func_arg *pArg;     /* compiled parameter, or NULL for a declared one */` |
|      - |  1428 | `	int bInternal;             /* owner is INTERNAL to php -- which for a COMPILED` |
|      - |  1429 | `	                            * parameter is the prelude's builtins, and decides` |
|      - |  1430 | `	                            * how php spells its default (see ReflectExportDefault) */` |
|      - |  1431 | `};` |
|      - |  1432 | `/*` |
|      - |  1433 | ` * Split a signature string on its top-level commas (a quoted default may hold` |
|      - |  1434 | ` * its own). Answers the parameter COUNT; when iWant is in range, hands back` |
|      - |  1435 | ` * that part's bytes.` |
|      - |  1436 | ` */` |
|  13696 |  1437 | `static int ReflectSigPart(const char *zSig, int nSig, int iWant,` |
|      - |  1438 | `	const char **pzPart, int *pnPart)` |
|      5 |  1439 | `{` |
|  13701 |  1440 | `	int iPos = 0;` |
|  27747 |  1441 | `	while( nSig > 0 ){` |
|  24687 |  1442 | `		int iComma = ReflectSigFindUnquoted(zSig,nSig,',');` |
|  24687 |  1443 | `		const char *zPart = zSig;` |
|  24687 |  1444 | `		int nPart = iComma < 0 ? nSig : iComma;` |
|  24687 |  1445 | `		ReflectSigTrim(&zPart,&nPart);` |
|  24687 |  1446 | `		if( nPart > 0 ){` |
|  24687 |  1447 | `			if( iPos == iWant && pzPart ){` |
|   6783 |  1448 | `				*pzPart = zPart;` |
|   6783 |  1449 | `				*pnPart = nPart;` |
|   3389 |  1450 | `			}` |
|  24687 |  1451 | `			iPos++;` |
|  12341 |  1452 | `		}` |
|  24687 |  1453 | `		if( iComma < 0 ){` |
|  10641 |  1454 | `			break;` |
|      - |  1455 | `		}` |
|  14051 |  1456 | `		zSig += iComma + 1;` |
|  14051 |  1457 | `		nSig -= iComma + 1;` |
|      5 |  1458 | `	}` |
|  13701 |  1459 | `	return iPos;` |
|      5 |  1460 | `}` |
|      - |  1461 | ``/* One `type &$name = default` part into the uniform description. */`` |
|   6778 |  1462 | `static void ReflectSigDescribe(const char *z, int n, int iPos, ReflectParamDesc *pOut)` |
|      5 |  1463 | `{` |
|   6783 |  1464 | `	const char *zDef = 0;` |
|   6783 |  1465 | `	int nDef = 0;` |
|      - |  1466 | `	int iEq, iDollar, iSpace;` |
|   6783 |  1467 | `	SyZero(pOut,sizeof(*pOut));` |
|   6783 |  1468 | `	pOut->iPos = iPos;` |
|   6783 |  1469 | `	if( n > 0 && z[0] == '~' ){` |
|      - |  1470 | `		/* The signature table's "declared here, screened by the builtin" marker` |
|      - |  1471 | `		 * (see vm_arg_check.c): php DECLARES this type and its C body asks for a` |
|      - |  1472 | `		 * tighter one, so Reflection reports what follows the marker. */` |
|    244 |  1473 | `		z++;` |
|    244 |  1474 | `		n--;` |
|    121 |  1475 | `	}` |
|      - |  1476 | ``	/* `= default` splits off first: everything after the first unquoted '='. */`` |
|   6783 |  1477 | `	iEq = ReflectSigFindUnquoted(z,n,'=');` |
|   6783 |  1478 | `	if( iEq >= 0 ){` |
|   2801 |  1479 | `		zDef = &z[iEq+1];` |
|   2801 |  1480 | `		nDef = n - iEq - 1;` |
|   2801 |  1481 | `		ReflectSigTrim(&zDef,&nDef);` |
|   2801 |  1482 | `		n = iEq;` |
|   2801 |  1483 | `		ReflectSigTrim(&z,&n);` |
|   1398 |  1484 | `	}` |
|   6783 |  1485 | `	if( zDef && nDef == 1 && zDef[0] == '!' ){` |
|      - |  1486 | ``		/* `= !` is the mirror image of `= ?` below, and it is php's own`` |
|      - |  1487 | `		 * stub-versus-body mismatch in the argument COUNT rather than the type:` |
|      - |  1488 | `		 * the parameter is DECLARED with no default -- Reflection reports it` |
|      - |  1489 | `		 * required, and getNumberOfRequiredParameters() counts it -- while the` |
|      - |  1490 | `		 * C body's own argument screen lets the call omit it.` |
|      - |  1491 | ``		 * `Dom\Node::insertBefore()`'s $child is the first: Reflection says two`` |
|      - |  1492 | ``		 * required parameters and `$p->insertBefore($n)` runs. So the marker`` |
|      - |  1493 | `		 * vanishes HERE (no default, not optional) and is left standing for` |
|      - |  1494 | ``		 * VmDeriveArityFromSig, which reads the `=` and lowers the minimum. */`` |
|     16 |  1495 | `		zDef = 0;` |
|     16 |  1496 | `		nDef = 0;` |
|   6776 |  1497 | `	}else if( zDef && nDef == 1 && zDef[0] == '?' ){` |
|      - |  1498 | ``		/* `= ?` is the table's OPTIONAL-but-no-default marker, php's own shape`` |
|      - |  1499 | `		 * for a parameter like ReflectionClass::getStaticPropertyValue()'s` |
|      - |  1500 | `		 * $default: isOptional() true, isDefaultValueAvailable() FALSE, so` |
|      - |  1501 | `		 * getDefaultValue() raises. Reporting it as a default (which is what` |
|      - |  1502 | ``		 * a bare `hasdef` did) made that call answer NULL instead. */`` |
|    102 |  1503 | `		pOut->bOptional = 1;` |
|    102 |  1504 | `		zDef = 0;` |
|    102 |  1505 | `		nDef = 0;` |
|     50 |  1506 | `	}` |
|   6783 |  1507 | `	pOut->bVariadic = ReflectSigHas(z,n,"...",3);` |
|   6783 |  1508 | `	if( pOut->bVariadic ){` |
|      - |  1509 | `		/* php: a variadic parameter never HAS a default -- it defaults to "no` |
|      - |  1510 | `		 * further arguments", which is not a value. Several signature rows still` |
|      - |  1511 | ``		 * write `mixed ...$values = ?` (the table's "optional, unspecified"`` |
|      - |  1512 | `		 * marker), and honouring it made isDefaultValueAvailable() true where php` |
|      - |  1513 | `		 * says false, so getDefaultValue() then threw on printf/array_merge. */` |
|    303 |  1514 | `		zDef = 0;` |
|    303 |  1515 | `		nDef = 0;` |
|    150 |  1516 | `	}` |
|   6783 |  1517 | `	iDollar = ReflectSigFindUnquoted(z,n,'$');` |
|   6783 |  1518 | `	iSpace = ReflectSigFindUnquoted(z,n,' ');` |
|      - |  1519 | `	{` |
|   6783 |  1520 | `		const char *zName = iDollar < 0 ? z : &z[iDollar+1];` |
|   6783 |  1521 | `		int nName = iDollar < 0 ? n : n - iDollar - 1;` |
|   6783 |  1522 | `		SyStringInitFromBuf(&pOut->sName,zName,nName);` |
|      - |  1523 | `	}` |
|   6783 |  1524 | `	pOut->bByRef = ReflectSigHas(z,n,"&",1);` |
|   6783 |  1525 | `	pOut->bHasDef = zDef != 0;` |
|   6783 |  1526 | `	pOut->bOptional = pOut->bOptional \|\| pOut->bVariadic \|\| zDef != 0;` |
|   6783 |  1527 | `	if( iSpace >= 0 && iDollar >= 0 && iSpace < iDollar ){` |
|      - |  1528 | `` 		/* The type is whatever precedes the first space, so `?DOMNode $child` `` |
|      - |  1529 | ``		 * types as `?DOMNode` and an untyped `$x` types as nothing. */`` |
|   6423 |  1530 | `		pOut->bNullable = (z[0] == '?' \|\| ReflectSigHasNoCase(z,iSpace,"null",4));` |
|   6423 |  1531 | `		SyStringInitFromBuf(&pOut->sType,z,iSpace);` |
|   3209 |  1532 | `	}` |
|   6783 |  1533 | `	if( zDef ){` |
|   2687 |  1534 | `		SyStringInitFromBuf(&pOut->sDefText,zDef,nDef);` |
|   1341 |  1535 | `	}` |
|   6783 |  1536 | `}` |
|      - |  1537 | `/*` |
|      - |  1538 | ` * Resolve a Generator object into its wrapper. Mirrors the static` |
|      - |  1539 | ` * VmGeneratorExtractCtx in vm.c: the $__ctx attribute carries the` |
|      - |  1540 | ` * ph7_generator pointer as a resource value.` |
|      - |  1541 | ` */` |
|     44 |  1542 | `static ph7_generator * ReflectGeneratorCtx(ph7_vm *pVm, ph7_value *pVal)` |
|      1 |  1543 | `{` |
|      - |  1544 | `	ph7_class_instance *pThis;` |
|      - |  1545 | `	ph7_value *pAttr;` |
|      - |  1546 | `	SyString sAttr;` |
|     45 |  1547 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVm->pGeneratorClass == 0 ){` |
|    ! 0 |  1548 | `		return 0;` |
|      - |  1549 | `	}` |
|     45 |  1550 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     45 |  1551 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|    ! 0 |  1552 | `		return 0;` |
|      - |  1553 | `	}` |
|     45 |  1554 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|     45 |  1555 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     45 |  1556 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|    ! 0 |  1557 | `		return 0;` |
|      - |  1558 | `	}` |
|     45 |  1559 | `	return (ph7_generator *)pAttr->x.pOther;` |
|     23 |  1560 | `}` |
|      - |  1561 | `/*` |
|      - |  1562 | ` * Evaluate the recorded argument expressions of one declared attribute and` |
|      - |  1563 | ` * answer them as a PHP array, or NULL when the target no longer resolves.` |
|      - |  1564 | ` *` |
|      - |  1565 | ` * apArg is the four-part SPEC a ReflectionAttribute carries — kind 'class'` |
|      - |  1566 | ` * (target = class), 'attr' (class + property/constant name), 'method' (class +` |
|      - |  1567 | ` * method), 'fn' (function name or Closure), 'param' (function spec + parameter` |
|      - |  1568 | ` * index), 'const' (global constant name) — plus which attribute of that target.` |
|      - |  1569 | ` * Named arguments become string keys.` |
|      - |  1570 | ` *` |
|      - |  1571 | ` * The values are evaluated HERE rather than when the reflector was built:` |
|      - |  1572 | `` * `#[Attr(self::SOME)]` runs php code, and php runs it at getArguments() time.`` |
|      - |  1573 | ` */` |
|    182 |  1574 | `static ph7_value * ReflectAttrArgs(ph7_context *pCtx, ph7_value **apArg, sxu32 nAttrIdx)` |
|      5 |  1575 | `{` |
|    187 |  1576 | `	ph7_vm *pVm = pCtx->pVm;` |
|    187 |  1577 | `	SySet *pAttrs = 0;` |
|    187 |  1578 | `	ph7_class *pDeclCls = 0; /* class an attribute is declared on: scope for self:: in its args */` |
|      - |  1579 | `	ph7_attribute *pAttrRec;` |
|      - |  1580 | `	ph7_value *pOut;` |
|      - |  1581 | `	const char *zKind;` |
|      - |  1582 | `	int nKind;` |
|      - |  1583 | `	sxu32 n;` |
|    187 |  1584 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|    259 |  1585 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|    149 |  1586 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|    149 |  1587 | `		if( pClass ){ pAttrs = &pClass->aAttrs; pDeclCls = pClass; }` |
|    115 |  1588 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|      5 |  1589 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|      5 |  1590 | `		ph7_class_attr *pMember = pClass ? ReflectFetchMember(pClass, apArg[2]) : 0;` |
|      5 |  1591 | `		if( pMember ){ pAttrs = &pMember->aAttrs; pDeclCls = pClass; }` |
|     46 |  1592 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|     16 |  1593 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     16 |  1594 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|     36 |  1595 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|     15 |  1596 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], 0, 0, 0, 0, 0);` |
|     15 |  1597 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|     18 |  1598 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|      7 |  1599 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], apArg[2], 0, 0, 0, 0);` |
|      7 |  1600 | `		ph7_vm_func_arg *pParam = pFunc` |
|      6 |  1601 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|      7 |  1602 | `		if( pParam ){ pAttrs = &pParam->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|      6 |  1603 | `	}else if( nKind == 5 && SyMemcmp(zKind, "const", 5) == 0 ){` |
|      - |  1604 | ``		/* Global constant (php 8.5 attributes on `const` statements) */`` |
|      - |  1605 | `		const char *zCName;` |
|      - |  1606 | `		int nCName;` |
|      - |  1607 | `		SyHashEntry *pCEntry;` |
|      3 |  1608 | `		zCName = ph7_value_to_string(apArg[1], &nCName);` |
|      3 |  1609 | `		pCEntry = nCName > 0 ? PH7_VmConstantFetch(pVm, zCName, (sxu32)nCName, 1) : 0;` |
|      3 |  1610 | `		if( pCEntry ){ pAttrs = &((ph7_constant *)pCEntry->pUserData)->aAttrs; }` |
|      1 |  1611 | `	}` |
|    182 |  1612 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|    187 |  1613 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|    ! 0 |  1614 | `		return 0;` |
|      - |  1615 | `	}` |
|    403 |  1616 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|    221 |  1617 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|      - |  1618 | `		ph7_value sValue;` |
|    221 |  1619 | `		PH7_MemObjInit(pVm, &sValue);` |
|    221 |  1620 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|      - |  1621 | `` 			/* Evaluate under the attribute's declaring-class scope so `self::class` `` |
|      - |  1622 | ``			 * / `self::CONST` / `parent::` resolve like php (else self:: would bind`` |
|      - |  1623 | `			 * to the reflection machinery's own class). */` |
|    186 |  1624 | `			PH7_VmExecAttrArg(pVm, &pArgRec->aByteCode, pDeclCls, &sValue);` |
|    128 |  1625 | `		}else if( pArgRec->pNativeValue ){` |
|      - |  1626 | ``			/* A NATIVE attribute (php's own `#[Attribute(...)]` on Attribute and`` |
|      - |  1627 | `			 * Deprecated): the argument is a literal, not byte-code. */` |
|     37 |  1628 | `			PH7_NativeLiteralValue(pVm, pArgRec->pNativeValue, &sValue);` |
|     17 |  1629 | `		}` |
|    221 |  1630 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|     13 |  1631 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|      7 |  1632 | `		}else{` |
|    209 |  1633 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|      - |  1634 | `		}` |
|    221 |  1635 | `		PH7_MemObjRelease(&sValue);` |
|    113 |  1636 | `	}` |
|    187 |  1637 | `	return pOut;` |
|     96 |  1638 | `}` |
|      - |  1639 | `/*` |
|      - |  1640 | ` * ---------------------------------------------------------------------------` |
|      - |  1641 | ` * The ReflectionType family.` |
|      - |  1642 | ` *` |
|      - |  1643 | ` * php's four type objects are pure VALUES: a text, a nullability flag, and for` |
|      - |  1644 | ` * the composites a list of members. Nothing in userland can build one — php` |
|      - |  1645 | `` * declares no constructor on any of them and refuses `clone` — so the prelude's`` |
|      - |  1646 | `` * public `__construct($name, $nullable, $text)` existed only because the factory`` |
|      - |  1647 | ` * that fills them was itself PHP. The factory is C now (ReflectMakeType), so the` |
|      - |  1648 | ` * constructors are gone and the classes say what php's say.` |
|      - |  1649 | ` * ---------------------------------------------------------------------------` |
|      - |  1650 | ` */` |
|      - |  1651 | `#define RT_TEXT     "__text"` |
|      - |  1652 | `#define RT_NULLABLE "__nullable"` |
|      - |  1653 | `#define RT_TNAME    "__tname"` |
|      - |  1654 | `#define RT_TYPES    "__types"` |
|      - |  1655 |  |
|     26 |  1656 | `static int vm_builtin_ReflectionType_allowsNull(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  1657 | `{` |
|     28 |  1658 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     13 |  1659 | `	SXUNUSED(nArg);` |
|     13 |  1660 | `	SXUNUSED(apArg);` |
|     28 |  1661 | `	ph7_result_bool(pCtx, pThis != 0 && PH7_NativeAttrTruthy(pThis, RT_NULLABLE));` |
|     28 |  1662 | `	return PH7_OK;` |
|      2 |  1663 | `}` |
|   1737 |  1664 | `static int vm_builtin_ReflectionType_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  1665 | `{` |
|   1742 |  1666 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   1742 |  1667 | `	const char *zText = "";` |
|   1742 |  1668 | `	int nText = 0;` |
|    867 |  1669 | `	SXUNUSED(nArg);` |
|    867 |  1670 | `	SXUNUSED(apArg);` |
|   1742 |  1671 | `	if( pThis ){` |
|   1742 |  1672 | `		PH7_NativeAttrStr(pThis, RT_TEXT, &zText, &nText);` |
|    867 |  1673 | `	}` |
|   1742 |  1674 | `	ph7_result_string(pCtx, zText, nText);` |
|   1742 |  1675 | `	return PH7_OK;` |
|      5 |  1676 | `}` |
|     22 |  1677 | `static int vm_builtin_ReflectionNamedType_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  1678 | `{` |
|     25 |  1679 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     25 |  1680 | `	const char *zName = "";` |
|     25 |  1681 | `	int nName = 0;` |
|     11 |  1682 | `	SXUNUSED(nArg);` |
|     11 |  1683 | `	SXUNUSED(apArg);` |
|     25 |  1684 | `	if( pThis ){` |
|     25 |  1685 | `		PH7_NativeAttrStr(pThis, RT_TNAME, &zName, &nName);` |
|     11 |  1686 | `	}` |
|     25 |  1687 | `	ph7_result_string(pCtx, zName, nName);` |
|     25 |  1688 | `	return PH7_OK;` |
|      3 |  1689 | `}` |
|      - |  1690 | `/* php's builtin-type set, case-insensitively. Anything else is a class name. */` |
|     46 |  1691 | `static int ReflectTypeIsBuiltin(const char *zName, int nName)` |
|      1 |  1692 | `{` |
|      - |  1693 | `	static const char *azBuiltin[] = {` |
|      - |  1694 | `		"int","float","string","bool","array","object","mixed",` |
|      - |  1695 | `		"void","never","null","callable","iterable","true","false"` |
|      - |  1696 | `	};` |
|      - |  1697 | `	sxu32 n;` |
|    369 |  1698 | `	for( n = 0 ; n < SX_ARRAYSIZE(azBuiltin) ; n++ ){` |
|    359 |  1699 | `		int nWant = (int)SyStrlen(azBuiltin[n]);` |
|      - |  1700 | `		int k;` |
|    359 |  1701 | `		if( nWant != nName ){` |
|    283 |  1702 | `			continue;` |
|      - |  1703 | `		}` |
|    243 |  1704 | `		for( k = 0 ; k < nWant ; k++ ){` |
|    207 |  1705 | `			if( SyToLower(zName[k]) != azBuiltin[n][k] ){` |
|     41 |  1706 | `				break;` |
|      - |  1707 | `			}` |
|     84 |  1708 | `		}` |
|     77 |  1709 | `		if( k == nWant ){` |
|     37 |  1710 | `			return 1;` |
|      - |  1711 | `		}` |
|     21 |  1712 | `	}` |
|     11 |  1713 | `	return 0;` |
|     24 |  1714 | `}` |
|     38 |  1715 | `static int vm_builtin_ReflectionNamedType_isBuiltin(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  1716 | `{` |
|     39 |  1717 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     39 |  1718 | `	const char *zName = "";` |
|     39 |  1719 | `	int nName = 0;` |
|     19 |  1720 | `	SXUNUSED(nArg);` |
|     19 |  1721 | `	SXUNUSED(apArg);` |
|     39 |  1722 | `	if( pThis ){` |
|     39 |  1723 | `		PH7_NativeAttrStr(pThis, RT_TNAME, &zName, &nName);` |
|     19 |  1724 | `	}` |
|     39 |  1725 | `	ph7_result_bool(pCtx, ReflectTypeIsBuiltin(zName, nName));` |
|     39 |  1726 | `	return PH7_OK;` |
|      1 |  1727 | `}` |
|      6 |  1728 | `static int vm_builtin_ReflectionType_getTypes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  1729 | `{` |
|      7 |  1730 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      7 |  1731 | `	ph7_value *pTypes = pThis ? PH7_NativeAttr(pThis, RT_TYPES) : 0;` |
|      3 |  1732 | `	SXUNUSED(nArg);` |
|      3 |  1733 | `	SXUNUSED(apArg);` |
|      7 |  1734 | `	if( pTypes && (pTypes->iFlags & MEMOBJ_HASHMAP) ){` |
|      7 |  1735 | `		ph7_result_value(pCtx, pTypes);` |
|      4 |  1736 | `	}else{` |
|      - |  1737 | `		/* The declared default is a native NULL slot (a spec cannot carry an` |
|      - |  1738 | `		 * array literal), but php's getTypes() always answers a list. */` |
|    ! 0 |  1739 | `		ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|    ! 0 |  1740 | `		if( pEmpty ){` |
|    ! 0 |  1741 | `			ph7_result_value(pCtx, pEmpty);` |
|    ! 0 |  1742 | `		}` |
|      - |  1743 | `	}` |
|      7 |  1744 | `	return PH7_OK;` |
|      1 |  1745 | `}` |
|      - |  1746 | `/* Build one of the three concrete types with its slots filled. The caller owns` |
|      - |  1747 | ` * the reference (PH7_NativeResultObject / a list insert drops it). */` |
|   2593 |  1748 | `static ph7_class_instance * ReflectNewType(ph7_context *pCtx, const char *zClass,` |
|      - |  1749 | `	const char *zText, int nText, int bNullable)` |
|      5 |  1750 | `{` |
|   2598 |  1751 | `	ph7_vm *pVm = pCtx->pVm;` |
|   2598 |  1752 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|   2598 |  1753 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|   2598 |  1754 | `	if( pObj == 0 ){` |
|    ! 0 |  1755 | `		return 0;` |
|      - |  1756 | `	}` |
|   2598 |  1757 | `	PH7_NativeSetAttrStr(pVm, pObj, RT_TEXT, zText, nText);` |
|   2598 |  1758 | `	PH7_NativeSetAttrBool(pVm, pObj, RT_NULLABLE, bNullable);` |
|   2598 |  1759 | `	return pObj;` |
|   1300 |  1760 | `}` |
|      - |  1761 | `/*` |
|      - |  1762 | ` * Append pType to a composite's member list, handing over the caller's reference.` |
|      - |  1763 | ` *` |
|      - |  1764 | ` * The carrier is a STACK value, deliberately: a ph7_context_new_scalar() is` |
|      - |  1765 | ` * released with the call context, and releasing a MEMOBJ_OBJ carrier unrefs the` |
|      - |  1766 | ` * instance a second time — which freed every member of a union or intersection` |
|      - |  1767 | ` * the moment its factory returned, so the list came back holding dead objects.` |
|      - |  1768 | ` * PH7_NativeResultObject hands an instance over the same way.` |
|      - |  1769 | ` */` |
|    884 |  1770 | `static void ReflectTypeListAdd(ph7_context *pCtx, ph7_value *pList, ph7_class_instance *pType)` |
|      5 |  1771 | `{` |
|      - |  1772 | `	ph7_value sVal;` |
|    889 |  1773 | `	if( pType == 0 ){` |
|    ! 0 |  1774 | `		return;` |
|      - |  1775 | `	}` |
|    889 |  1776 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    889 |  1777 | `	sVal.x.pOther = pType;` |
|    889 |  1778 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|    889 |  1779 | `	ph7_array_add_elem(pList, 0, &sVal);   /* takes its own reference */` |
|    889 |  1780 | `	PH7_ClassInstanceUnref(pType);` |
|    447 |  1781 | `}` |
|      - |  1782 | `/* Exact, case-insensitive name test (php lower-cases before comparing). */` |
|   4000 |  1783 | `static int ReflectTypeNameIs(const char *z, int n, const char *zWant)` |
|      5 |  1784 | `{` |
|   4005 |  1785 | `	int nWant = (int)SyStrlen(zWant), k;` |
|   4005 |  1786 | `	if( n != nWant ){` |
|   3352 |  1787 | `		return 0;` |
|      - |  1788 | `	}` |
|   1016 |  1789 | `	for( k = 0 ; k < n ; k++ ){` |
|    942 |  1790 | `		if( SyToLower(z[k]) != zWant[k] ){` |
|    584 |  1791 | `			return 0;` |
|      - |  1792 | `		}` |
|    183 |  1793 | `	}` |
|     78 |  1794 | `	return 1;` |
|   2002 |  1795 | `}` |
|      - |  1796 | `/*` |
|      - |  1797 | ` * A ReflectionNamedType for one name.` |
|      - |  1798 | ` *` |
|      - |  1799 | ` * bQMark says the text carried a leading '?', which is the ONLY thing that puts` |
|      - |  1800 | `` * one back on the rendered text — while `null` and `mixed` are nullable by their`` |
|      - |  1801 | ` * own meaning without ever rendering a '?'. Those two rules are independent, and` |
|      - |  1802 | `` * conflating them is how `?array` came out as `array`.`` |
|      - |  1803 | ` */` |
|   2307 |  1804 | `static ph7_class_instance * ReflectNewNamed(ph7_context *pCtx, const char *z, int n, int bQMark)` |
|      5 |  1805 | `{` |
|      - |  1806 | `	ph7_class_instance *pObj;` |
|      - |  1807 | `	char zBuf[256];` |
|   2312 |  1808 | `	const char *zText = z;` |
|   2312 |  1809 | `	int nText = n;` |
|   2312 |  1810 | `	int bNullable = bQMark \|\| ReflectTypeNameIs(z, n, "null") \|\| ReflectTypeNameIs(z, n, "mixed");` |
|   2312 |  1811 | `	if( bQMark && n + 1 < (int)sizeof(zBuf) ){` |
|    319 |  1812 | `		zBuf[0] = '?';` |
|    319 |  1813 | `		SyMemcpy(z, &zBuf[1], (sxu32)n);` |
|    319 |  1814 | `		zText = zBuf;` |
|    319 |  1815 | `		nText = n + 1;` |
|    157 |  1816 | `	}` |
|   2312 |  1817 | `	pObj = ReflectNewType(pCtx, "ReflectionNamedType", zText, nText, bNullable);` |
|   2312 |  1818 | `	if( pObj ){` |
|   2312 |  1819 | `		PH7_NativeSetAttrStr(pCtx->pVm, pObj, RT_TNAME, z, n);` |
|   1152 |  1820 | `	}` |
|   2312 |  1821 | `	return pObj;` |
|      5 |  1822 | `}` |
|      - |  1823 | `/*` |
|      - |  1824 | `` * One ATOM of a type text: `?X`, `(A&B)` or a plain name. An intersection is`` |
|      - |  1825 | ` * the only composite an atom can be, because php only nests that way.` |
|      - |  1826 | ` */` |
|   2295 |  1827 | `static ph7_class_instance * ReflectMakeAtom(ph7_context *pCtx, const char *z, int n)` |
|      5 |  1828 | `{` |
|   2300 |  1829 | `	int bQMark = 0;` |
|   2300 |  1830 | `	if( n > 0 && z[0] == '?' ){` |
|    319 |  1831 | `		bQMark = 1;` |
|    319 |  1832 | `		z++;` |
|    319 |  1833 | `		n--;` |
|    157 |  1834 | `	}` |
|   2300 |  1835 | `	if( n > 1 && z[0] == '(' && z[n-1] == ')' ){` |
|     11 |  1836 | `		z++;` |
|     11 |  1837 | `		n -= 2;` |
|      4 |  1838 | `	}` |
|   2300 |  1839 | `	if( ReflectSigFindUnquoted(z, n, '&') >= 0 ){` |
|     15 |  1840 | `		ph7_class_instance *pObj = ReflectNewType(pCtx, "ReflectionIntersectionType", z, n, 0);` |
|     15 |  1841 | `		ph7_value *pList = ph7_context_new_array(pCtx);` |
|     15 |  1842 | `		const char *zCur = z;` |
|     15 |  1843 | `		int nCur = n;` |
|     15 |  1844 | `		if( pObj == 0 \|\| pList == 0 ){` |
|    ! 0 |  1845 | `			return pObj;` |
|      - |  1846 | `		}` |
|     27 |  1847 | `		while( nCur > 0 ){` |
|     27 |  1848 | `			int iCut = ReflectSigFindUnquoted(zCur, nCur, '&');` |
|     39 |  1849 | `			ReflectTypeListAdd(pCtx, pList,` |
|     12 |  1850 | `				ReflectNewNamed(pCtx, zCur, iCut < 0 ? nCur : iCut, 0));` |
|     27 |  1851 | `			if( iCut < 0 ){` |
|     15 |  1852 | `				break;` |
|      - |  1853 | `			}` |
|     15 |  1854 | `			zCur += iCut + 1;` |
|     15 |  1855 | `			nCur -= iCut + 1;` |
|      3 |  1856 | `		}` |
|     15 |  1857 | `		PH7_NativeSetProp(pCtx->pVm, pObj, RT_TYPES, sizeof(RT_TYPES)-1, pList);` |
|     15 |  1858 | `		return pObj;` |
|      - |  1859 | `	}` |
|   2288 |  1860 | `	return ReflectNewNamed(pCtx, z, n, bQMark);` |
|   1151 |  1861 | `}` |
|      - |  1862 | `/*` |
|      - |  1863 | ` * A declared type TEXT to the object php answers for it: a named type, a union,` |
|      - |  1864 | `` * or an intersection. NULL for an absent type. `X\|null` collapses back to a`` |
|      - |  1865 | `` * NULLABLE named type, which is what php reports (`?X`), but only when exactly`` |
|      - |  1866 | ` * one non-null arm is left and it is not itself an intersection.` |
|      - |  1867 | ` */` |
|   1933 |  1868 | `static ph7_class_instance * ReflectMakeType(ph7_context *pCtx, const char *zText, int nText)` |
|      5 |  1869 | `{` |
|   1938 |  1870 | `	const char *zBody = zText;` |
|   1938 |  1871 | `	int nBody = nText;` |
|   1938 |  1872 | `	int bNullable = 0, bHasNull = 0, nParts = 0, nNonNull = 0;` |
|   1938 |  1873 | `	const char *zLastNonNull = 0;` |
|   1938 |  1874 | `	int nLastNonNull = 0;` |
|      - |  1875 | `	const char *zCur;` |
|      - |  1876 | `	int nCur, iDepth, k, iStart;` |
|   1938 |  1877 | `	if( nText < 1 ){` |
|    ! 0 |  1878 | `		return 0;` |
|      - |  1879 | `	}` |
|   1938 |  1880 | `	if( zBody[0] == '?' ){` |
|    319 |  1881 | `		bNullable = 1;` |
|    319 |  1882 | `		zBody++;` |
|    319 |  1883 | `		nBody--;` |
|    157 |  1884 | `	}` |
|      - |  1885 | ``	/* Split on the TOP-LEVEL '\|' only: `A\|(B&C)` has one at depth 0 and none`` |
|      - |  1886 | `	 * inside the parentheses. */` |
|   1938 |  1887 | `	iDepth = 0;` |
|   1938 |  1888 | `	iStart = 0;` |
|  18173 |  1889 | `	for( k = 0 ; k <= nBody ; k++ ){` |
|  16240 |  1890 | `		if( k < nBody && zBody[k] == '(' ){` |
|     11 |  1891 | `			iDepth++;` |
|     11 |  1892 | `			continue;` |
|      - |  1893 | `		}` |
|  16232 |  1894 | `		if( k < nBody && zBody[k] == ')' ){` |
|     11 |  1895 | `			iDepth--;` |
|     11 |  1896 | `			continue;` |
|      - |  1897 | `		}` |
|  16224 |  1898 | `		if( k == nBody \|\| (zBody[k] == '\|' && iDepth == 0) ){` |
|   2300 |  1899 | `			zCur = &zBody[iStart];` |
|   2300 |  1900 | `			nCur = k - iStart;` |
|   2300 |  1901 | `			nParts++;` |
|   2300 |  1902 | `			if( nCur == 4 && ReflectSigHasNoCase(zCur, 4, "null", 4) ){` |
|     20 |  1903 | `				bHasNull = 1;` |
|     11 |  1904 | `			}else{` |
|   2282 |  1905 | `				nNonNull++;` |
|   2282 |  1906 | `				zLastNonNull = zCur;` |
|   2282 |  1907 | `				nLastNonNull = nCur;` |
|      - |  1908 | `			}` |
|   2300 |  1909 | `			iStart = k + 1;` |
|   1146 |  1910 | `		}` |
|   8107 |  1911 | `	}` |
|   1938 |  1912 | `	if( nParts > 1 ){` |
|      - |  1913 | `		ph7_class_instance *pObj;` |
|      - |  1914 | `		ph7_value *pList;` |
|    274 |  1915 | `		if( bHasNull && nNonNull == 1` |
|     13 |  1916 | `		 && ReflectSigFindUnquoted(zLastNonNull, nLastNonNull, '&') < 0 ){` |
|      - |  1917 | ``			/* `X\|null` IS `?X` to php. */`` |
|      - |  1918 | `			char zBuf[256];` |
|      - |  1919 | `			ph7_class_instance *pNamed;` |
|    ! 0 |  1920 | `			const char *zRender = zLastNonNull;` |
|    ! 0 |  1921 | `			int nRender = nLastNonNull;` |
|    ! 0 |  1922 | `			if( nLastNonNull + 1 < (int)sizeof(zBuf) ){` |
|    ! 0 |  1923 | `				zBuf[0] = '?';` |
|    ! 0 |  1924 | `				SyMemcpy(zLastNonNull, &zBuf[1], (sxu32)nLastNonNull);` |
|    ! 0 |  1925 | `				zRender = zBuf;` |
|    ! 0 |  1926 | `				nRender = nLastNonNull + 1;` |
|    ! 0 |  1927 | `			}` |
|    ! 0 |  1928 | `			pNamed = ReflectNewType(pCtx, "ReflectionNamedType", zRender, nRender, 1);` |
|    ! 0 |  1929 | `			if( pNamed ){` |
|    ! 0 |  1930 | `				PH7_NativeSetAttrStr(pCtx->pVm, pNamed, RT_TNAME, zLastNonNull, nLastNonNull);` |
|    ! 0 |  1931 | `			}` |
|    ! 0 |  1932 | `			return pNamed;` |
|      - |  1933 | `		}` |
|    279 |  1934 | `		pObj = ReflectNewType(pCtx, "ReflectionUnionType", zBody, nBody, bNullable \|\| bHasNull);` |
|    279 |  1935 | `		pList = ph7_context_new_array(pCtx);` |
|    279 |  1936 | `		if( pObj == 0 \|\| pList == 0 ){` |
|    ! 0 |  1937 | `			return pObj;` |
|      - |  1938 | `		}` |
|    279 |  1939 | `		iDepth = 0;` |
|    279 |  1940 | `		iStart = 0;` |
|   4819 |  1941 | `		for( k = 0 ; k <= nBody ; k++ ){` |
|   4545 |  1942 | `			if( k < nBody && zBody[k] == '(' ){` |
|     11 |  1943 | `				iDepth++;` |
|     11 |  1944 | `				continue;` |
|      - |  1945 | `			}` |
|   4537 |  1946 | `			if( k < nBody && zBody[k] == ')' ){` |
|     11 |  1947 | `				iDepth--;` |
|     11 |  1948 | `				continue;` |
|      - |  1949 | `			}` |
|   4529 |  1950 | `			if( k == nBody \|\| (zBody[k] == '\|' && iDepth == 0) ){` |
|    959 |  1951 | `				ReflectTypeListAdd(pCtx, pList,` |
|    318 |  1952 | `					ReflectMakeAtom(pCtx, &zBody[iStart], k - iStart));` |
|    641 |  1953 | `				iStart = k + 1;` |
|    318 |  1954 | `			}` |
|   2267 |  1955 | `		}` |
|    279 |  1956 | `		PH7_NativeSetProp(pCtx->pVm, pObj, RT_TYPES, sizeof(RT_TYPES)-1, pList);` |
|    279 |  1957 | `		return pObj;` |
|      - |  1958 | `	}` |
|   1664 |  1959 | `	if( ReflectSigFindUnquoted(zBody, nBody, '&') >= 0 ){` |
|      5 |  1960 | `		return ReflectMakeAtom(pCtx, zBody, nBody);` |
|      - |  1961 | `	}` |
|   1660 |  1962 | `	if( bNullable ){` |
|      - |  1963 | `		char zBuf[256];` |
|    319 |  1964 | `		if( nBody + 1 < (int)sizeof(zBuf) ){` |
|    319 |  1965 | `			zBuf[0] = '?';` |
|    319 |  1966 | `			SyMemcpy(zBody, &zBuf[1], (sxu32)nBody);` |
|    319 |  1967 | `			return ReflectMakeAtom(pCtx, zBuf, nBody + 1);` |
|      - |  1968 | `		}` |
|    ! 0 |  1969 | `	}` |
|   1346 |  1970 | `	return ReflectMakeAtom(pCtx, zBody, nBody);` |
|    970 |  1971 | `}` |
|      - |  1972 | `/*` |
|      - |  1973 | ` * Declare the four type classes. Called from PH7_VmInstallReflection() where` |
|      - |  1974 | `` * chunk 4 used to be compiled, so `Stringable` (a core interface) already`` |
|      - |  1975 | ` * exists. PH7_CLASS_NOCLONE is php's own rule for these: they are values the` |
|      - |  1976 | `` * engine hands out, and `clone $type` is an Error, not a copy.`` |
|      - |  1977 | ` */` |
|   8445 |  1978 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionTypes(ph7_vm *pVm)` |
|      5 |  1979 | `{` |
|      - |  1980 | `	static const PH7_NativePropDef aBaseProp[] = {` |
|      - |  1981 | `		{ RT_TEXT,     PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - |  1982 | `		{ RT_NULLABLE, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 }, 0 },` |
|      - |  1983 | `	};` |
|      - |  1984 | `	static const PH7_NativeMethodDef aBaseMethod[] = {` |
|      - |  1985 | `		{ "allowsNull", PH7_MOD_PUBLIC, "", "@bool",       vm_builtin_ReflectionType_allowsNull },` |
|      - |  1986 | `		{ "__toString", PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionType_toString },` |
|      - |  1987 | `	};` |
|      - |  1988 | `	static const PH7_NativePropDef aNamedProp[] = {` |
|      - |  1989 | `		{ RT_TNAME, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - |  1990 | `	};` |
|      - |  1991 | `	static const PH7_NativeMethodDef aNamedMethod[] = {` |
|      - |  1992 | `		{ "getName",   PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionNamedType_getName },` |
|      - |  1993 | `		{ "isBuiltin", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionNamedType_isBuiltin },` |
|      - |  1994 | `	};` |
|      - |  1995 | `	static const PH7_NativePropDef aCompProp[] = {` |
|      - |  1996 | `		{ RT_TYPES, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  1997 | `	};` |
|      - |  1998 | `	static const PH7_NativeMethodDef aCompMethod[] = {` |
|      - |  1999 | `		{ "getTypes", PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionType_getTypes },` |
|      - |  2000 | `	};` |
|      - |  2001 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  2002 | `		{ "ReflectionType", 0, "Stringable", PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  2003 | `		  aBaseMethod, SX_ARRAYSIZE(aBaseMethod), 0, 0, aBaseProp, SX_ARRAYSIZE(aBaseProp), 0, 0, 0 },` |
|      - |  2004 | `		{ "ReflectionNamedType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  2005 | `		  aNamedMethod, SX_ARRAYSIZE(aNamedMethod), 0, 0, aNamedProp, SX_ARRAYSIZE(aNamedProp), 0, 0, 0 },` |
|      - |  2006 | `		{ "ReflectionUnionType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  2007 | `		  aCompMethod, SX_ARRAYSIZE(aCompMethod), 0, 0, aCompProp, SX_ARRAYSIZE(aCompProp), 0, 0, 0 },` |
|      - |  2008 | `		{ "ReflectionIntersectionType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  2009 | `		  aCompMethod, SX_ARRAYSIZE(aCompMethod), 0, 0, aCompProp, SX_ARRAYSIZE(aCompProp), 0, 0, 0 },` |
|      - |  2010 | `	};` |
|   8450 |  2011 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  2012 | `}` |
|      - |  2013 | `/*` |
|      - |  2014 | ` * ---------------------------------------------------------------------------` |
|      - |  2015 | ` * The six standalone reflection classes.` |
|      - |  2016 | ` *` |
|      - |  2017 | ` * ReflectionGenerator, ReflectionFiber, ReflectionConstant, ReflectionExtension,` |
|      - |  2018 | ` * ReflectionZendExtension and ReflectionReference reflect ENGINE state rather` |
|      - |  2019 | ` * than a class hierarchy, so each was a prelude class over one or two thunks` |
|      - |  2020 | ` * whose only job was to hand that state to PHP. Their bodies are the C now, and` |
|      - |  2021 | ` * the four thunks that existed for them (__reflect_gen_info, __reflect_gen_exec,` |
|      - |  2022 | ` * __reflect_const_info, __reflect_ref_id) are gone with them.` |
|      - |  2023 | ` *` |
|      - |  2024 | ` * The ReflectionEnum family stays in the prelude: it extends ReflectionClass and` |
|      - |  2025 | ` * ReflectionClassConstant, which are still PHP.` |
|      - |  2026 | ` * ---------------------------------------------------------------------------` |
|      - |  2027 | ` */` |
|      - |  2028 | `#define RG_GEN   "__gen"` |
|      - |  2029 | `#define RF_FIBER "__fiber"` |
|      - |  2030 | `#define RR_ID    "__id"` |
|      - |  2031 |  |
|      - |  2032 | `/* Build an instance of a class that may still be PRELUDE PHP, running its` |
|      - |  2033 | ` * constructor. Answers 0 when the constructor threw (rc says which). */` |
|   3520 |  2034 | `static ph7_class_instance * ReflectConstruct(ph7_context *pCtx, const char *zClass,` |
|      - |  2035 | `	int nArg, ph7_value **apArg, sxi32 *pRc)` |
|      5 |  2036 | `{` |
|   3525 |  2037 | `	ph7_vm *pVm = pCtx->pVm;` |
|   3525 |  2038 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|      - |  2039 | `	ph7_class_instance *pThis;` |
|      - |  2040 | `	ph7_class_method *pCons;` |
|   3525 |  2041 | `	*pRc = PH7_OK;` |
|   3525 |  2042 | `	if( pClass == 0 ){` |
|    ! 0 |  2043 | `		return 0;` |
|      - |  2044 | `	}` |
|   3525 |  2045 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|   3525 |  2046 | `	if( pThis == 0 ){` |
|    ! 0 |  2047 | `		return 0;` |
|      - |  2048 | `	}` |
|   3525 |  2049 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|   3525 |  2050 | `	if( pCons ){` |
|   3525 |  2051 | `		sxi32 rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, nArg, apArg);` |
|   3525 |  2052 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|      5 |  2053 | `			PH7_ClassInstanceCtorFailed(pThis);` |
|      5 |  2054 | `			PH7_ClassInstanceUnref(pThis);` |
|      5 |  2055 | `			*pRc = rc;` |
|      5 |  2056 | `			return 0;` |
|      - |  2057 | `		}` |
|   1758 |  2058 | `	}` |
|   3521 |  2059 | `	return pThis;` |
|   1765 |  2060 | `}` |
|      - |  2061 | `/* The instance a native reflector wraps ($this->__gen / $this->__fiber). */` |
|     50 |  2062 | `static ph7_class_instance * ReflectWrapped(ph7_context *pCtx, const char *zSlot)` |
|      1 |  2063 | `{` |
|     51 |  2064 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     51 |  2065 | `	return pThis ? PH7_NativeAttrObj(pThis, zSlot) : 0;` |
|      1 |  2066 | `}` |
|      - |  2067 | `/* Hand back a wrapped instance the receiver already owns (a borrowed reference). */` |
|     10 |  2068 | `static int ReflectResultBorrowed(ph7_context *pCtx, ph7_class_instance *pObj)` |
|      1 |  2069 | `{` |
|      - |  2070 | `	ph7_value sVal;` |
|     11 |  2071 | `	if( pObj == 0 ){` |
|    ! 0 |  2072 | `		ph7_result_null(pCtx);` |
|    ! 0 |  2073 | `		return PH7_OK;` |
|      - |  2074 | `	}` |
|     11 |  2075 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     11 |  2076 | `	sVal.x.pOther = pObj;` |
|     11 |  2077 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|     11 |  2078 | `	ph7_result_value(pCtx, &sVal);   /* takes its own reference */` |
|     11 |  2079 | `	return PH7_OK;` |
|      6 |  2080 | `}` |
|      - |  2081 | `/*` |
|      - |  2082 | ` * ---------------------------------------------------------------------------` |
|      - |  2083 | ` * ReflectionAttribute — chunk 7.` |
|      - |  2084 | ` *` |
|      - |  2085 | ` * php's own class, plus the shared getAttributes() body that produces it. The` |
|      - |  2086 | ` * chunk it replaces also carried __reflect_target_names (folded into the` |
|      - |  2087 | ` * "cannot target" diagnostic below) and __reflect_has_deprecated, which had` |
|      - |  2088 | ` * already lost its last caller when isDeprecated() became C.` |
|      - |  2089 | ` *` |
|      - |  2090 | ` * An instance holds a SPEC, not the arguments: [kind, target, member,` |
|      - |  2091 | ` * paramIdx] names something the engine can reopen, and getArguments()` |
|      - |  2092 | ` * evaluates the recorded expressions on every call, because php does too --` |
|      - |  2093 | `` * `#[A(self::X)]` is php code and it runs when it is asked for. That is also`` |
|      - |  2094 | ` * why the prelude needed a public __init(): PHP could not fill a fresh object` |
|      - |  2095 | ` * any other way. C fills it directly, so __init() (and the` |
|      - |  2096 | ` * __reflect_new_no_ctor that built the empty shell) are gone with it.` |
|      - |  2097 | ` * ---------------------------------------------------------------------------` |
|      - |  2098 | ` */` |
|      - |  2099 | ``#define RA_NAME   "name"      /* php declares this one PUBLIC: `$attr->name` */`` |
|      - |  2100 | `#define RA_SPEC   "__spec"    /* [kind, target, member, paramIdx] */` |
|      - |  2101 | `#define RA_IDX    "__idx"     /* which of that target's attributes this is */` |
|      - |  2102 | `#define RA_TARGET "__target"  /* the Attribute::TARGET_* bit it was found on */` |
|      - |  2103 | `#define RA_REP    "__rep"     /* the target carries more than one of this name */` |
|      - |  2104 |  |
|      - |  2105 | `/* Bound on the value exporter's own recursion. An attribute argument cannot be` |
|      - |  2106 | ` * cyclic, but an OBJECT argument's property graph can be. */` |
|      - |  2107 | `#define REFLECT_EXPORT_MAX_DEPTH 31` |
|      - |  2108 |  |
|      - |  2109 | `/* php's Attribute::TARGET_* names, in bit order (TARGET_CLASS is bit 0). */` |
|      - |  2110 | `static const char *const azReflectTarget[] = {` |
|      - |  2111 | `	"class", "function", "method", "property", "class constant", "parameter", "constant"` |
|      - |  2112 | `};` |
|      - |  2113 |  |
|      - |  2114 | `static void ReflectExportValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth);` |
|      - |  2115 |  |
|      - |  2116 | `/*` |
|      - |  2117 | ` * A string the way php's reflection prints one, in either of php's TWO quotings.` |
|      - |  2118 | ` *` |
|      - |  2119 | ` * A USERLAND default prints single-quoted, with the NON-PRINTABLE bytes escaped` |
|      - |  2120 | ` * (php's smart_str_append_escaped) and the quote itself NOT escaped — php's own` |
|      - |  2121 | ` * output for "q'q" is 'q'q'. An INTERNAL one prints DOUBLE-quoted and escapes the` |
|      - |  2122 | `` * quote: php answers `string $enclosure = "\""` for str_getcsv where PHL used to`` |
|      - |  2123 | `` * answer `'"'`. Every internal function with a string default was affected; the`` |
|      - |  2124 | `` * pair was found on Closure::bindTo's `= "static"` while making that class native.`` |
|      - |  2125 | ` */` |
|    398 |  2126 | `static void ReflectExportStrQ(SyBlob *pOut, const char *zIn, sxu32 nIn, char cQuote)` |
|      5 |  2127 | `{` |
|      - |  2128 | `	static const char zHexDigit[] = "0123456789ABCDEF";` |
|      - |  2129 | `	sxu32 i;` |
|    403 |  2130 | `	SyBlobAppend(pOut, &cQuote, sizeof(char));` |
|    761 |  2131 | `	for( i = 0 ; i < nIn ; i++ ){` |
|    363 |  2132 | `		unsigned char c = (unsigned char)zIn[i];` |
|    363 |  2133 | `		if( c == (unsigned char)cQuote && cQuote == '"' ){` |
|     22 |  2134 | `			SyBlobAppend(pOut, "\\\"", sizeof("\\\"")-1);` |
|     22 |  2135 | `			continue;` |
|      - |  2136 | `		}` |
|      - |  2137 | `		{` |
|    343 |  2138 | `		const char *zEsc = 0;` |
|    343 |  2139 | `		switch( c ){` |
|     13 |  2140 | `			case 0x09: zEsc = "\\t"; break;` |
|     30 |  2141 | `			case 0x0A: zEsc = "\\n"; break;` |
|     13 |  2142 | `			case 0x0B: zEsc = "\\v"; break;` |
|      5 |  2143 | `			case 0x0C: zEsc = "\\f"; break;` |
|     15 |  2144 | `			case 0x0D: zEsc = "\\r"; break;` |
|      3 |  2145 | `			case 0x1B: zEsc = "\\e"; break;` |
|     24 |  2146 | `			case '\\': zEsc = "\\\\"; break;` |
|    244 |  2147 | `			default:   break;` |
|      - |  2148 | `		}` |
|    343 |  2149 | `		if( zEsc ){` |
|     96 |  2150 | `			SyBlobAppend(pOut, zEsc, sizeof("\\t")-1);` |
|    303 |  2151 | `		}else if( c < 0x20 \|\| c > 0x7E ){` |
|      - |  2152 | `			char zHex[4];` |
|     15 |  2153 | `			zHex[0] = '\\';` |
|     15 |  2154 | `			zHex[1] = 'x';` |
|     15 |  2155 | `			zHex[2] = zHexDigit[(c >> 4) & 0x0F];` |
|     15 |  2156 | `			zHex[3] = zHexDigit[c & 0x0F];` |
|     15 |  2157 | `			SyBlobAppend(pOut, zHex, sizeof(zHex));` |
|      8 |  2158 | `		}else{` |
|    235 |  2159 | `			SyBlobAppend(pOut, (const char *)&c, sizeof(char));` |
|      - |  2160 | `		}` |
|      - |  2161 | `		}` |
|    174 |  2162 | `	}` |
|    403 |  2163 | `	SyBlobAppend(pOut, &cQuote, sizeof(char));` |
|    403 |  2164 | `}` |
|      - |  2165 | `/* php's userland quoting, which is what every existing caller means. */` |
|    186 |  2166 | `static void ReflectExportStr(SyBlob *pOut, const char *zIn, sxu32 nIn)` |
|      1 |  2167 | `{` |
|    187 |  2168 | `	ReflectExportStrQ(pOut, zIn, nIn, '\'');` |
|    187 |  2169 | `}` |
|      - |  2170 | `/*` |
|      - |  2171 | ` * A float the way php's reflection prints one: the plain string cast (which` |
|      - |  2172 | `` * PHL already renders php's way, 1.0E+15 and all), then php's `zero_frac` —`` |
|      - |  2173 | ` * a float that came out without a fraction gets ".0" so 1.0 does not print as` |
|      - |  2174 | ` * 1. INF and NAN render as words and are left alone.` |
|      - |  2175 | ` */` |
|     52 |  2176 | `static void ReflectExportReal(ph7_vm *pVm, SyBlob *pOut, ph7_real rVal)` |
|      4 |  2177 | `{` |
|      - |  2178 | `	ph7_value sTmp;` |
|      - |  2179 | `	const char *zText;` |
|     56 |  2180 | `	int nText, i, bPlain = 1;` |
|     56 |  2181 | `	PH7_MemObjInitFromReal(pVm, &sTmp, rVal);` |
|     56 |  2182 | `	zText = ph7_value_to_string(&sTmp, &nText);` |
|     56 |  2183 | `	if( nText > 0 ){` |
|     56 |  2184 | `		SyBlobAppend(pOut, zText, (sxu32)nText);` |
|     26 |  2185 | `	}` |
|    122 |  2186 | `	for( i = 0 ; i < nText ; i++ ){` |
|     80 |  2187 | `		if( (zText[i] < '0' \|\| zText[i] > '9') && !(i == 0 && zText[i] == '-') ){` |
|     11 |  2188 | `			bPlain = 0;` |
|     11 |  2189 | `			break;` |
|      - |  2190 | `		}` |
|     37 |  2191 | `	}` |
|     56 |  2192 | `	if( bPlain && nText > 0 ){` |
|     46 |  2193 | `		SyBlobAppend(pOut, ".0", sizeof(".0")-1);` |
|     21 |  2194 | `	}` |
|     56 |  2195 | `	PH7_MemObjRelease(&sTmp);` |
|     56 |  2196 | `}` |
|      - |  2197 | `/*` |
|      - |  2198 | ` * An array the way php's reflection prints one: a LIST — every key an integer,` |
|      - |  2199 | ` * in sequence from zero — prints no keys at all, and anything else prints one` |
|      - |  2200 | `` * for EVERY entry. That is why `[1, 'k' => 2]` comes out as`` |
|      - |  2201 | `` * `[0 => 1, 'k' => 2]` while `[[1], [2 => 3]]` keeps its outer keys silent.`` |
|      - |  2202 | ` */` |
|     36 |  2203 | `static void ReflectExportArray(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth)` |
|      1 |  2204 | `{` |
|     37 |  2205 | `	ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|      - |  2206 | `	ph7_hashmap_node *pEntry;` |
|     37 |  2207 | `	sxi64 iExpect = 0;` |
|      - |  2208 | `	sxu32 n;` |
|     37 |  2209 | `	int bList = 1;` |
|     69 |  2210 | `	for( pEntry = pMap->pFirst, n = 0 ; n < pMap->nEntry && pEntry ; n++ ){` |
|     45 |  2211 | `		if( pEntry->iType == HASHMAP_BLOB_NODE \|\| pEntry->xKey.iKey != iExpect ){` |
|     13 |  2212 | `			bList = 0;` |
|     13 |  2213 | `			break;` |
|      - |  2214 | `		}` |
|     33 |  2215 | `		iExpect++;` |
|     33 |  2216 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     17 |  2217 | `	}` |
|     37 |  2218 | `	SyBlobAppend(pOut, "[", sizeof(char));` |
|     83 |  2219 | `	for( pEntry = pMap->pFirst, n = 0 ; n < pMap->nEntry && pEntry ; n++ ){` |
|     47 |  2220 | `		ph7_value *pMember = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|     47 |  2221 | `		if( n > 0 ){` |
|     17 |  2222 | `			SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|      8 |  2223 | `		}` |
|     47 |  2224 | `		if( !bList ){` |
|     21 |  2225 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|     13 |  2226 | `				ReflectExportStr(pOut, (const char *)SyBlobData(&pEntry->xKey.sKey),` |
|      4 |  2227 | `					SyBlobLength(&pEntry->xKey.sKey));` |
|      5 |  2228 | `			}else{` |
|     13 |  2229 | `				SyBlobFormat(pOut, "%qd", pEntry->xKey.iKey);` |
|      - |  2230 | `			}` |
|     21 |  2231 | `			SyBlobAppend(pOut, " => ", sizeof(" => ")-1);` |
|     10 |  2232 | `		}` |
|     47 |  2233 | `		ReflectExportValue(pCtx, pOut, pMember, iDepth + 1);` |
|     47 |  2234 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     24 |  2235 | `	}` |
|     37 |  2236 | `	SyBlobAppend(pOut, "]", sizeof(char));` |
|     37 |  2237 | `}` |
|      - |  2238 | `/*` |
|      - |  2239 | `` * php's TYPE word in a `Constant [ ... ]` head -- for a CLASS constant, a`` |
|      - |  2240 | ` * global one and an extension's listing alike. 0 means "an object", whose word` |
|      - |  2241 | ` * is its own class name and which only the caller can spell.` |
|      - |  2242 | ` */` |
|   1606 |  2243 | `static const char * ReflectExportTypeWord(ph7_value *pVal)` |
|      2 |  2244 | `{` |
|   1608 |  2245 | `	if( pVal == 0 ){` |
|    ! 0 |  2246 | `		return "null";` |
|      - |  2247 | `	}` |
|   1608 |  2248 | `	if( (pVal->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      3 |  2249 | `		return 0;` |
|      - |  2250 | `	}` |
|   1606 |  2251 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|      3 |  2252 | `		return "null";` |
|      - |  2253 | `	}` |
|   1604 |  2254 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      5 |  2255 | `		return "array";` |
|      - |  2256 | `	}` |
|   1600 |  2257 | `	if( pVal->iFlags & MEMOBJ_RES ){` |
|     13 |  2258 | `		return "resource";` |
|      - |  2259 | `	}` |
|   1588 |  2260 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      5 |  2261 | `		return "bool";` |
|      - |  2262 | `	}` |
|   1584 |  2263 | `	if( pVal->iFlags & MEMOBJ_REAL ){` |
|     11 |  2264 | `		return "float";` |
|      - |  2265 | `	}` |
|   1574 |  2266 | `	if( pVal->iFlags & MEMOBJ_INT ){` |
|   1466 |  2267 | `		return "int";` |
|      - |  2268 | `	}` |
|    110 |  2269 | `	if( pVal->iFlags & MEMOBJ_STRING ){` |
|    110 |  2270 | `		return "string";` |
|      - |  2271 | `	}` |
|    ! 0 |  2272 | `	return "null";` |
|    805 |  2273 | `}` |
|      - |  2274 | ``/* The text php prints between a constant's `{ ` and ` }`. NULL and FALSE are`` |
|      - |  2275 | `` * both nothing at all, an array is `Array` and an object `Object`. */`` |
|   1606 |  2276 | `static void ReflectExportConstValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal)` |
|      2 |  2277 | `{` |
|   1608 |  2278 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|      3 |  2279 | `		return;` |
|      - |  2280 | `	}` |
|   1606 |  2281 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      5 |  2282 | `		SyBlobAppend(pOut, "Array", sizeof("Array")-1);` |
|      5 |  2283 | `		return;` |
|      - |  2284 | `	}` |
|   1602 |  2285 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      3 |  2286 | `		SyBlobAppend(pOut, "Object", sizeof("Object")-1);` |
|      3 |  2287 | `		return;` |
|      - |  2288 | `	}` |
|   1600 |  2289 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      5 |  2290 | `		if( pVal->x.iVal ){` |
|      3 |  2291 | `			SyBlobAppend(pOut, "1", sizeof(char));` |
|      1 |  2292 | `		}` |
|      5 |  2293 | `		return;` |
|      - |  2294 | `	}` |
|      - |  2295 | `	{` |
|      - |  2296 | `		ph7_value sTmp;` |
|      - |  2297 | `		const char *zText;` |
|      - |  2298 | `		int nText;` |
|   1596 |  2299 | `		PH7_MemObjInit(pCtx->pVm, &sTmp);` |
|   1596 |  2300 | `		PH7_MemObjStore(pVal, &sTmp);` |
|   1596 |  2301 | `		zText = ph7_value_to_string(&sTmp, &nText);` |
|   1596 |  2302 | `		if( nText > 0 ){` |
|   1592 |  2303 | `			SyBlobAppend(pOut, zText, (sxu32)nText);` |
|    795 |  2304 | `		}` |
|   1596 |  2305 | `		PH7_MemObjRelease(&sTmp);` |
|      - |  2306 | `	}` |
|    805 |  2307 | `}` |
|      - |  2308 | `/*` |
|      - |  2309 | `` * One value in php's reflection export syntax — the text after `= ` in a`` |
|      - |  2310 | `` * parameter default and inside `Argument #0 [ … ]` in an attribute dump.`` |
|      - |  2311 | ` *` |
|      - |  2312 | ` * The type dispatch is PH7_MemObjDump's, including its REAL-before-INT order:` |
|      - |  2313 | ` * an integer-valued float carries a cached int view too, and php prints 1.0.` |
|      - |  2314 | ` *` |
|      - |  2315 | ` * php renders an OBJECT argument by echoing the source expression it never` |
|      - |  2316 | `` * folded (`new \A(x: 1)`), which needs the AST. PHL evaluates attribute`` |
|      - |  2317 | ` * arguments from byte-code and has only the resulting VALUE, so a non-enum` |
|      - |  2318 | ` * object is rebuilt from its own properties: same shape, not always the same` |
|      - |  2319 | ` * bytes. An enum case — the one object php's own value formatter handles —` |
|      - |  2320 | ` * matches exactly.` |
|      - |  2321 | ` */` |
|    936 |  2322 | `static void ReflectExportValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth)` |
|      5 |  2323 | `{` |
|    941 |  2324 | `	if( pVal == 0 \|\| iDepth > REFLECT_EXPORT_MAX_DEPTH ){` |
|    ! 0 |  2325 | `		SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|    ! 0 |  2326 | `		return;` |
|      - |  2327 | `	}` |
|    941 |  2328 | `	if( (pVal->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      3 |  2329 | `		ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|      3 |  2330 | `		if( pObj->pClass->iFlags & PH7_CLASS_ENUM ){` |
|      3 |  2331 | `			ph7_value *pName = PH7_EnumCaseNameValue(pObj);` |
|      - |  2332 | `			/* php writes the case as the source did, so a namespaced enum comes` |
|      - |  2333 | `			 * out fully qualified (\N\E::One) and a global one bare (E::One).` |
|      - |  2334 | `			 * The separator is the only thing left of that distinction here. */` |
|      3 |  2335 | `			if( SyByteFind(SyStringData(&pObj->pClass->sName),` |
|      4 |  2336 | `				SyStringLength(&pObj->pClass->sName), '\\', 0) == SXRET_OK ){` |
|    ! 0 |  2337 | `				SyBlobAppend(pOut, "\\", sizeof(char));` |
|    ! 0 |  2338 | `			}` |
|      3 |  2339 | `			SyBlobFormat(pOut, "%z::", &pObj->pClass->sName);` |
|      3 |  2340 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|      3 |  2341 | `				SyBlobAppend(pOut, SyBlobData(&pName->sBlob), SyBlobLength(&pName->sBlob));` |
|      1 |  2342 | `			}` |
|      3 |  2343 | `			return;` |
|      - |  2344 | `		}` |
|    ! 0 |  2345 | `		SyBlobFormat(pOut, "new \\%z(", &pObj->pClass->sName);` |
|      - |  2346 | `		{` |
|      - |  2347 | `			SyHashEntry *pEntry;` |
|    ! 0 |  2348 | `			int nWritten = 0;` |
|    ! 0 |  2349 | `			SyHashResetLoopCursor(&pObj->hAttr);` |
|    ! 0 |  2350 | `			while((pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){` |
|    ! 0 |  2351 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - |  2352 | `				ph7_value *pSlot;` |
|    ! 0 |  2353 | `				if( PH7_ATTR_UNPRESENTED(pVmAttr)` |
|    ! 0 |  2354 | `				 \|\| (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) ){` |
|    ! 0 |  2355 | `					continue; /* class-level members are not part of the object */` |
|      - |  2356 | `				}` |
|    ! 0 |  2357 | `				pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|    ! 0 |  2358 | `				if( nWritten++ ){` |
|    ! 0 |  2359 | `					SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|    ! 0 |  2360 | `				}` |
|    ! 0 |  2361 | `				ReflectExportValue(pCtx, pOut, pSlot, iDepth + 1);` |
|    ! 0 |  2362 | `			}` |
|      - |  2363 | `		}` |
|    ! 0 |  2364 | `		SyBlobAppend(pOut, ")", sizeof(char));` |
|    ! 0 |  2365 | `		return;` |
|      - |  2366 | `	}` |
|    939 |  2367 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|     31 |  2368 | `		SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|     31 |  2369 | `		return;` |
|      - |  2370 | `	}` |
|    909 |  2371 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|     37 |  2372 | `		ReflectExportArray(pCtx, pOut, pVal, iDepth);` |
|     37 |  2373 | `		return;` |
|      - |  2374 | `	}` |
|    873 |  2375 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|    153 |  2376 | `		if( pVal->x.iVal != 0 ){` |
|     53 |  2377 | `			SyBlobAppend(pOut, "true", sizeof("true")-1);` |
|     28 |  2378 | `		}else{` |
|    103 |  2379 | `			SyBlobAppend(pOut, "false", sizeof("false")-1);` |
|      - |  2380 | `		}` |
|    153 |  2381 | `		return;` |
|      - |  2382 | `	}` |
|    723 |  2383 | `	if( pVal->iFlags & MEMOBJ_REAL ){` |
|     56 |  2384 | `		ReflectExportReal(pCtx->pVm, pOut, pVal->rVal);` |
|     56 |  2385 | `		return;` |
|      - |  2386 | `	}` |
|    670 |  2387 | `	if( pVal->iFlags & MEMOBJ_INT ){` |
|    492 |  2388 | `		SyBlobFormat(pOut, "%qd", pVal->x.iVal);` |
|    492 |  2389 | `		return;` |
|      - |  2390 | `	}` |
|    179 |  2391 | `	if( pVal->iFlags & MEMOBJ_STRING ){` |
|    179 |  2392 | `		ReflectExportStr(pOut, (const char *)SyBlobData(&pVal->sBlob), SyBlobLength(&pVal->sBlob));` |
|    179 |  2393 | `		return;` |
|      - |  2394 | `	}` |
|      - |  2395 | `	/* A resource, and anything else php has no export syntax for. */` |
|    ! 0 |  2396 | `	SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|    473 |  2397 | `}` |
|      - |  2398 | `/*` |
|      - |  2399 | ` * Build one ReflectionAttribute. pSpec is shared by every attribute of the` |
|      - |  2400 | ` * same target — it says how to reopen it, not which one this is.` |
|      - |  2401 | ` */` |
|    180 |  2402 | `static ph7_class_instance * ReflectAttrNew(ph7_context *pCtx, SyString *pName,` |
|      - |  2403 | `	ph7_value *pSpec, sxu32 nIdx, int iTargetBit, int bRepeated)` |
|      5 |  2404 | `{` |
|    185 |  2405 | `	ph7_vm *pVm = pCtx->pVm;` |
|    185 |  2406 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, "ReflectionAttribute",` |
|      - |  2407 | `		sizeof("ReflectionAttribute")-1, FALSE, 0);` |
|    185 |  2408 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|    185 |  2409 | `	if( pObj == 0 ){` |
|    ! 0 |  2410 | `		return 0;` |
|      - |  2411 | `	}` |
|    185 |  2412 | `	PH7_NativeSetAttrStr(pVm, pObj, RA_NAME, SyStringData(pName), (int)SyStringLength(pName));` |
|    185 |  2413 | `	PH7_NativeSetProp(pVm, pObj, RA_SPEC, sizeof(RA_SPEC)-1, pSpec);` |
|    185 |  2414 | `	PH7_NativeSetAttrInt(pVm, pObj, RA_IDX, (sxi64)nIdx);` |
|    185 |  2415 | `	PH7_NativeSetAttrInt(pVm, pObj, RA_TARGET, iTargetBit);` |
|    185 |  2416 | `	PH7_NativeSetAttrBool(pVm, pObj, RA_REP, bRepeated);` |
|    185 |  2417 | `	return pObj;` |
|     95 |  2418 | `}` |
|      - |  2419 | `/*` |
|      - |  2420 | ` * Unpack the receiver's spec into the four-value vector ReflectAttrArgs takes.` |
|      - |  2421 | ` * Returns 0 when the object is not one this file built.` |
|      - |  2422 | ` */` |
|    110 |  2423 | `static int ReflectAttrSpec(ph7_context *pCtx, ph7_class_instance *pThis, ph7_value **apOut)` |
|      5 |  2424 | `{` |
|    115 |  2425 | `	ph7_value *pSpec = pThis ? PH7_NativeAttr(pThis, RA_SPEC) : 0;` |
|      - |  2426 | `	ph7_hashmap *pMap;` |
|      - |  2427 | `	int i;` |
|    115 |  2428 | `	if( pSpec == 0 \|\| (pSpec->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 |  2429 | `		return 0;` |
|      - |  2430 | `	}` |
|    115 |  2431 | `	pMap = (ph7_hashmap *)pSpec->x.pOther;` |
|    555 |  2432 | `	for( i = 0 ; i < 4 ; i++ ){` |
|      - |  2433 | `		ph7_value sKey;` |
|    445 |  2434 | `		ph7_hashmap_node *pNode = 0;` |
|      - |  2435 | `		sxi32 rc;` |
|    445 |  2436 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sKey, i);` |
|    445 |  2437 | `		rc = PH7_HashmapLookup(pMap, &sKey, &pNode);` |
|    445 |  2438 | `		PH7_MemObjRelease(&sKey);` |
|    445 |  2439 | `		if( rc != SXRET_OK \|\| pNode == 0 ){` |
|    ! 0 |  2440 | `			return 0;` |
|      - |  2441 | `		}` |
|    445 |  2442 | `		apOut[i] = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pNode->nValIdx);` |
|    445 |  2443 | `		if( apOut[i] == 0 ){` |
|    ! 0 |  2444 | `			return 0;` |
|      - |  2445 | `		}` |
|    225 |  2446 | `	}` |
|    115 |  2447 | `	return 1;` |
|     60 |  2448 | `}` |
|      - |  2449 | `/* The receiver's evaluated arguments, or an empty array when the target no` |
|      - |  2450 | `` * longer resolves (what the chunk's `$a === null ? array() : $a` said). */`` |
|    110 |  2451 | `static ph7_value * ReflectAttrOwnArgs(ph7_context *pCtx, ph7_class_instance *pThis)` |
|      5 |  2452 | `{` |
|      - |  2453 | `	ph7_value *apSpec[4];` |
|    115 |  2454 | `	ph7_value *pArgs = 0;` |
|    115 |  2455 | `	if( pThis && ReflectAttrSpec(pCtx, pThis, apSpec) ){` |
|    115 |  2456 | `		pArgs = ReflectAttrArgs(pCtx, apSpec, (sxu32)PH7_NativeAttrInt(pThis, RA_IDX));` |
|     55 |  2457 | `	}` |
|    115 |  2458 | `	return pArgs ? pArgs : ph7_context_new_array(pCtx);` |
|      5 |  2459 | `}` |
|     64 |  2460 | `static int vm_builtin_ReflectionAttribute_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  2461 | `{` |
|     68 |  2462 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     68 |  2463 | `	const char *zName = "";` |
|     68 |  2464 | `	int nName = 0;` |
|     32 |  2465 | `	SXUNUSED(nArg);` |
|     32 |  2466 | `	SXUNUSED(apArg);` |
|     68 |  2467 | `	if( pThis ){` |
|     68 |  2468 | `		PH7_NativeAttrStr(pThis, RA_NAME, &zName, &nName);` |
|     32 |  2469 | `	}` |
|     68 |  2470 | `	ph7_result_string(pCtx, zName, nName);` |
|     68 |  2471 | `	return PH7_OK;` |
|      4 |  2472 | `}` |
|     24 |  2473 | `static int vm_builtin_ReflectionAttribute_getTarget(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2474 | `{` |
|     25 |  2475 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     12 |  2476 | `	SXUNUSED(nArg);` |
|     12 |  2477 | `	SXUNUSED(apArg);` |
|     25 |  2478 | `	ph7_result_int64(pCtx, pThis ? PH7_NativeAttrInt(pThis, RA_TARGET) : 0);` |
|     25 |  2479 | `	return PH7_OK;` |
|      1 |  2480 | `}` |
|     18 |  2481 | `static int vm_builtin_ReflectionAttribute_isRepeated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2482 | `{` |
|     19 |  2483 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      9 |  2484 | `	SXUNUSED(nArg);` |
|      9 |  2485 | `	SXUNUSED(apArg);` |
|     19 |  2486 | `	ph7_result_bool(pCtx, pThis != 0 && PH7_NativeAttrTruthy(pThis, RA_REP));` |
|     19 |  2487 | `	return PH7_OK;` |
|      1 |  2488 | `}` |
|     40 |  2489 | `static int vm_builtin_ReflectionAttribute_getArguments(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  2490 | `{` |
|     43 |  2491 | `	ph7_value *pArgs = ReflectAttrOwnArgs(pCtx, PH7_ContextThis(pCtx));` |
|     20 |  2492 | `	SXUNUSED(nArg);` |
|     20 |  2493 | `	SXUNUSED(apArg);` |
|     43 |  2494 | `	if( pArgs == 0 ){` |
|    ! 0 |  2495 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  2496 | `	}` |
|     43 |  2497 | `	ph7_result_value(pCtx, pArgs);` |
|     43 |  2498 | `	return PH7_OK;` |
|     23 |  2499 | `}` |
|      - |  2500 | `/*` |
|      - |  2501 | ` * newInstance(): the four screens php runs before it constructs anything —` |
|      - |  2502 | ` * the attribute class exists, it is declared #[Attribute], that declaration` |
|      - |  2503 | ` * allows the target this was found on, and it allows repetition if it was` |
|      - |  2504 | ` * repeated. The messages are php's, byte for byte.` |
|      - |  2505 | ` */` |
|     80 |  2506 | `static int vm_builtin_ReflectionAttribute_newInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  2507 | `{` |
|     84 |  2508 | `	ph7_vm *pVm = pCtx->pVm;` |
|     84 |  2509 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     84 |  2510 | `	ph7_value *pNameVal = pThis ? PH7_NativeAttr(pThis, RA_NAME) : 0;` |
|      - |  2511 | `	ph7_class *pClass;` |
|      - |  2512 | `	ph7_attribute *aA;` |
|      - |  2513 | `	ph7_value *pDeclArgs;` |
|     84 |  2514 | `	sxu32 n, nDecl = 0;` |
|     84 |  2515 | `	int bDecl = 0, iTarget, iBit;` |
|     84 |  2516 | `	sxi64 iFlags = 127; /* php's TARGET_ALL: #[Attribute] with no argument */` |
|     40 |  2517 | `	SXUNUSED(nArg);` |
|     40 |  2518 | `	SXUNUSED(apArg);` |
|     84 |  2519 | `	if( pNameVal == 0 ){` |
|    ! 0 |  2520 | `		ph7_result_null(pCtx);` |
|    ! 0 |  2521 | `		return PH7_OK;` |
|      - |  2522 | `	}` |
|     84 |  2523 | `	pClass = ReflectResolveClass(pVm, pNameVal);` |
|     84 |  2524 | `	if( pClass == 0 ){` |
|      - |  2525 | `		SyString sName;` |
|      5 |  2526 | `		SyStringInitFromBuf(&sName, SyBlobData(&pNameVal->sBlob), SyBlobLength(&pNameVal->sBlob));` |
|      5 |  2527 | `		return PH7_VmThrowException(pCtx, "Error", "Attribute class \"%z\" not found", &sName);` |
|      - |  2528 | `	}` |
|      - |  2529 | `	/* Which #[...] on the attribute class is its own #[Attribute] declaration */` |
|     80 |  2530 | `	aA = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);` |
|     80 |  2531 | `	for( n = 0 ; n < SySetUsed(&pClass->aAttrs) ; n++ ){` |
|     72 |  2532 | `		if( SyStringLength(&aA[n].sName) == sizeof("Attribute")-1` |
|     76 |  2533 | `		 && SyStrnicmp(SyStringData(&aA[n].sName), "Attribute", sizeof("Attribute")-1) == 0 ){` |
|     76 |  2534 | `			nDecl = n;` |
|     76 |  2535 | `			bDecl = 1;` |
|     76 |  2536 | `			break;` |
|      - |  2537 | `		}` |
|    ! 0 |  2538 | `	}` |
|     80 |  2539 | `	if( !bDecl ){` |
|      7 |  2540 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      2 |  2541 | `			"Attempting to use non-attribute class \"%z\" as attribute", &pClass->sDisp);` |
|      - |  2542 | `	}` |
|      - |  2543 | ``	/* Its argument is the target mask: positional, or named `flags:`. */`` |
|      - |  2544 | `	{` |
|      - |  2545 | `		ph7_value *apSpec[4];` |
|     76 |  2546 | `		ph7_value *pKind = ph7_context_new_scalar(pCtx);` |
|     76 |  2547 | `		ph7_value *pMem  = ph7_context_new_scalar(pCtx);` |
|     76 |  2548 | `		ph7_value *pIdx  = ph7_context_new_scalar(pCtx);` |
|     76 |  2549 | `		if( pKind == 0 \|\| pMem == 0 \|\| pIdx == 0 ){` |
|    ! 0 |  2550 | `			return PH7_ContextMemoryError(pCtx);` |
|      - |  2551 | `		}` |
|     76 |  2552 | `		ph7_value_string(pKind, "class", sizeof("class")-1);` |
|     76 |  2553 | `		ph7_value_null(pMem);` |
|     76 |  2554 | `		ph7_value_int(pIdx, 0);` |
|     76 |  2555 | `		apSpec[0] = pKind;` |
|     76 |  2556 | `		apSpec[1] = pNameVal;` |
|     76 |  2557 | `		apSpec[2] = pMem;` |
|     76 |  2558 | `		apSpec[3] = pIdx;` |
|     76 |  2559 | `		pDeclArgs = ReflectAttrArgs(pCtx, apSpec, nDecl);` |
|      - |  2560 | `	}` |
|     76 |  2561 | `	if( pDeclArgs ){` |
|     76 |  2562 | `		ph7_value *pFlags = ph7_array_fetch(pDeclArgs, "0", -1);` |
|     76 |  2563 | `		if( pFlags == 0 ){` |
|     20 |  2564 | `			pFlags = ph7_array_fetch(pDeclArgs, "flags", -1);` |
|      9 |  2565 | `		}` |
|     76 |  2566 | `		if( pFlags ){` |
|     57 |  2567 | `			iFlags = ph7_value_to_int64(pFlags);` |
|     27 |  2568 | `		}` |
|     36 |  2569 | `	}` |
|     76 |  2570 | `	iTarget = (int)PH7_NativeAttrInt(pThis, RA_TARGET);` |
|      - |  2571 | `	/* php's INTERNAL attributes carry a validator beside their mask, and the` |
|      - |  2572 | `	 * engine runs BOTH where the declaration compiles (GenStateCheckAttrPlacement)` |
|      - |  2573 | ``	 * -- so a misplaced `#[\Deprecated]` or `#[\Override]` never reaches this`` |
|      - |  2574 | `	 * far. What is left here is the USERLAND rule, which php checks only when` |
|      - |  2575 | `	 * someone asks: the mask below and the repetition test after it. */` |
|     76 |  2576 | `	if( (iFlags & iTarget) == 0 ){` |
|      - |  2577 | `		SyBlob sAllowed;` |
|      - |  2578 | `		sxi32 rc;` |
|     10 |  2579 | `		SyBlobInit(&sAllowed, &pVm->sAllocator);` |
|     66 |  2580 | `		for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azReflectTarget) ; iBit++ ){` |
|     58 |  2581 | `			if( (iFlags & ((sxi64)1 << iBit)) == 0 ){` |
|     50 |  2582 | `				continue;` |
|      - |  2583 | `			}` |
|     10 |  2584 | `			if( SyBlobLength(&sAllowed) > 0 ){` |
|    ! 0 |  2585 | `				SyBlobAppend(&sAllowed, ", ", sizeof(", ")-1);` |
|    ! 0 |  2586 | `			}` |
|     14 |  2587 | `			SyBlobAppend(&sAllowed, azReflectTarget[iBit],` |
|      8 |  2588 | `				(sxu32)SyStrlen(azReflectTarget[iBit]));` |
|      6 |  2589 | `		}` |
|     10 |  2590 | `		SyBlobAppend(&sAllowed, "", sizeof(char)); /* NUL for the %s below */` |
|     22 |  2591 | `		for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azReflectTarget) ; iBit++ ){` |
|     22 |  2592 | `			if( iTarget == (1 << iBit) ){` |
|     10 |  2593 | `				break;` |
|      - |  2594 | `			}` |
|      8 |  2595 | `		}` |
|     14 |  2596 | `		rc = PH7_VmThrowException(pCtx, "Error",` |
|      4 |  2597 | `			"Attribute \"%z\" cannot target %s (allowed targets: %s)", &pClass->sDisp,` |
|      4 |  2598 | `			iBit < (int)SX_ARRAYSIZE(azReflectTarget) ? azReflectTarget[iBit] : "",` |
|      4 |  2599 | `			SyBlobData(&sAllowed));` |
|     10 |  2600 | `		SyBlobRelease(&sAllowed);` |
|     10 |  2601 | `		return rc;` |
|      - |  2602 | `	}` |
|     68 |  2603 | `	if( PH7_NativeAttrTruthy(pThis, RA_REP) && (iFlags & 128) == 0 ){` |
|     11 |  2604 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      3 |  2605 | `			"Attribute \"%z\" must not be repeated", &pClass->sDisp);` |
|      - |  2606 | `	}` |
|     62 |  2607 | `	return ReflectAttrInstantiate(pCtx, pNameVal, ReflectAttrOwnArgs(pCtx, pThis));` |
|     44 |  2608 | `}` |
|      - |  2609 | `/*` |
|      - |  2610 | `` * php's export text. A bare `Attribute [ Name ]` when there are no arguments;`` |
|      - |  2611 | ` * otherwise the same head followed by the argument block, each argument` |
|      - |  2612 | `` * rendered in php's value-export syntax and named ones as `name = value`.`` |
|      - |  2613 | ` */` |
|     12 |  2614 | `static int vm_builtin_ReflectionAttribute_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2615 | `{` |
|     13 |  2616 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 |  2617 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     13 |  2618 | `	ph7_value *pArgs = ReflectAttrOwnArgs(pCtx, pThis);` |
|     13 |  2619 | `	const char *zName = "";` |
|     13 |  2620 | `	int nName = 0;` |
|      - |  2621 | `	SyBlob sOut;` |
|     13 |  2622 | `	sxu32 nCount = 0;` |
|      6 |  2623 | `	SXUNUSED(nArg);` |
|      6 |  2624 | `	SXUNUSED(apArg);` |
|     13 |  2625 | `	if( pThis ){` |
|     13 |  2626 | `		PH7_NativeAttrStr(pThis, RA_NAME, &zName, &nName);` |
|      6 |  2627 | `	}` |
|     13 |  2628 | `	if( pArgs && (pArgs->iFlags & MEMOBJ_HASHMAP) ){` |
|     13 |  2629 | `		nCount = ((ph7_hashmap *)pArgs->x.pOther)->nEntry;` |
|      6 |  2630 | `	}` |
|     13 |  2631 | `	SyBlobInit(&sOut, &pVm->sAllocator);` |
|     13 |  2632 | `	SyBlobAppend(&sOut, "Attribute [ ", sizeof("Attribute [ ")-1);` |
|     13 |  2633 | `	if( nName > 0 ){` |
|     13 |  2634 | `		SyBlobAppend(&sOut, zName, (sxu32)nName);` |
|      6 |  2635 | `	}` |
|     13 |  2636 | `	SyBlobAppend(&sOut, " ]", sizeof(" ]")-1);` |
|     13 |  2637 | `	if( nCount < 1 ){` |
|      3 |  2638 | `		SyBlobAppend(&sOut, "\n", sizeof(char));` |
|      2 |  2639 | `	}else{` |
|     11 |  2640 | `		ph7_hashmap *pMap = (ph7_hashmap *)pArgs->x.pOther;` |
|     11 |  2641 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|      - |  2642 | `		sxu32 n;` |
|     11 |  2643 | `		SyBlobFormat(&sOut, " {\n  - Arguments [%u] {\n", nCount);` |
|     81 |  2644 | `		for( n = 0 ; n < nCount && pEntry ; n++ ){` |
|     71 |  2645 | `			ph7_value *pMember = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, pEntry->nValIdx);` |
|     71 |  2646 | `			SyBlobFormat(&sOut, "    Argument #%u [ ", n);` |
|     71 |  2647 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      7 |  2648 | `				SyBlobAppend(&sOut, SyBlobData(&pEntry->xKey.sKey),` |
|      2 |  2649 | `					SyBlobLength(&pEntry->xKey.sKey));` |
|      5 |  2650 | `				SyBlobAppend(&sOut, " = ", sizeof(" = ")-1);` |
|      2 |  2651 | `			}` |
|     71 |  2652 | `			ReflectExportValue(pCtx, &sOut, pMember, 0);` |
|     71 |  2653 | `			SyBlobAppend(&sOut, " ]\n", sizeof(" ]\n")-1);` |
|     71 |  2654 | `			pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     36 |  2655 | `		}` |
|     11 |  2656 | `		SyBlobAppend(&sOut, "  }\n}\n", sizeof("  }\n}\n")-1);` |
|      - |  2657 | `	}` |
|     13 |  2658 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     13 |  2659 | `	SyBlobRelease(&sOut);` |
|     13 |  2660 | `	return PH7_OK;` |
|      1 |  2661 | `}` |
|      - |  2662 | `/* The body of the two methods php declares PRIVATE and never calls. Neither is` |
|      - |  2663 | `` * reachable from php code: `new ReflectionAttribute` is the engine's own "Call`` |
|      - |  2664 | `` * to private … from global scope" Error, and `clone` is refused by`` |
|      - |  2665 | ` * PH7_CLASS_NOCLONE before any body runs. They exist so the class REPORTS them,` |
|      - |  2666 | ` * which is the only way php's own dump shows them too. */` |
|    ! 0 |  2667 | `static int vm_builtin_ReflectionAttribute_private(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  2668 | `{` |
|    ! 0 |  2669 | `	SXUNUSED(nArg);` |
|    ! 0 |  2670 | `	SXUNUSED(apArg);` |
|    ! 0 |  2671 | `	SXUNUSED(pCtx);` |
|    ! 0 |  2672 | `	return PH7_OK;` |
|    ! 0 |  2673 | `}` |
|      - |  2674 | `/*` |
|      - |  2675 | ` * Declare ReflectionAttribute. Called from PH7_VmInstallReflection() where` |
|      - |  2676 | ` * chunk 7 used to be compiled, so Reflector (chunk 1) already exists.` |
|      - |  2677 | ` *` |
|      - |  2678 | ` * PH7_CLASS_NOCLONE is php's rule for it (php declares __clone private AND` |
|      - |  2679 | ` * refuses the copy), and the class is NOT final — php 8.5 unsealed it.` |
|      - |  2680 | ` */` |
|   8445 |  2681 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionAttribute(ph7_vm *pVm)` |
|      5 |  2682 | `{` |
|      - |  2683 | `	static const PH7_NativeConstDef aConst[] = {` |
|      - |  2684 | `		{ "IS_INSTANCEOF", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|      - |  2685 | `	};` |
|      - |  2686 | `	static const PH7_NativePropDef aProp[] = {` |
|      - |  2687 | `		{ RA_NAME,   PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  2688 | `		/* PHL-only, and PROTECTED so php code cannot reach them: the spec that` |
|      - |  2689 | `		 * reopens the target. php holds the same state on the C struct behind the` |
|      - |  2690 | `		 * object, invisible; PHL has no hidden-slot bit yet (recorded), so these` |
|      - |  2691 | `		 * four still show up in a var_dump where php shows only $name. */` |
|      - |  2692 | `		{ RA_SPEC,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0,  0.0 }, 0 },` |
|      - |  2693 | `		{ RA_IDX,    PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0,  0.0 }, 0 },` |
|      - |  2694 | `		{ RA_TARGET, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0,  0.0 }, 0 },` |
|      - |  2695 | `		{ RA_REP,    PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0,  0.0 }, 0 },` |
|      - |  2696 | `	};` |
|      - |  2697 | `	/* php's own listing order, which is what __toString() prints. */` |
|      - |  2698 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|      - |  2699 | `		{ "getName",      PH7_MOD_PUBLIC,  "", "string", vm_builtin_ReflectionAttribute_getName },` |
|      - |  2700 | `		{ "getTarget",    PH7_MOD_PUBLIC,  "", "int",    vm_builtin_ReflectionAttribute_getTarget },` |
|      - |  2701 | `		{ "isRepeated",   PH7_MOD_PUBLIC,  "", "bool",   vm_builtin_ReflectionAttribute_isRepeated },` |
|      - |  2702 | `		{ "getArguments", PH7_MOD_PUBLIC,  "", "array",  vm_builtin_ReflectionAttribute_getArguments },` |
|      - |  2703 | `		{ "newInstance",  PH7_MOD_PUBLIC,  "", "object", vm_builtin_ReflectionAttribute_newInstance },` |
|      - |  2704 | `		{ "__toString",   PH7_MOD_PUBLIC,  "", "string", vm_builtin_ReflectionAttribute_toString },` |
|      - |  2705 | `		{ "__clone",      PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionAttribute_private },` |
|      - |  2706 | `		{ "__construct",  PH7_MOD_PRIVATE, "", 0,      vm_builtin_ReflectionAttribute_private },` |
|      - |  2707 | `	};` |
|      - |  2708 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  2709 | `		{ "ReflectionAttribute", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  2710 | `		  aMethod, SX_ARRAYSIZE(aMethod), aConst, SX_ARRAYSIZE(aConst),` |
|      - |  2711 | `		  aProp, SX_ARRAYSIZE(aProp), 0, 0, 0 },` |
|      - |  2712 | `	};` |
|   8450 |  2713 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  2714 | `}` |
|      - |  2715 | `/*` |
|      - |  2716 | ` * The shared getAttributes() body.` |
|      - |  2717 | ` *` |
|      - |  2718 | ` * Turns the target's #[...] records into ReflectionAttribute objects, applying` |
|      - |  2719 | ` * php's name / IS_INSTANCEOF filter. Argument VALUES stay lazy — what each` |
|      - |  2720 | ` * object carries is the [kind, target, member, paramIdx] spec that reopens` |
|      - |  2721 | ` * them — so a reflector declaring getAttributes() only has to say WHICH target` |
|      - |  2722 | ` * it is.` |
|      - |  2723 | ` */` |
|    184 |  2724 | `static int ReflectBuildAttrs(ph7_context *pCtx, SySet *pAttrs, const char *zKind,` |
|      - |  2725 | `	ph7_value *pTarget, const char *zMember, int nMember,` |
|      - |  2726 | `	int iParamIdx, int iTargetBit, int nArg, ph7_value **apArg)` |
|      5 |  2727 | `{` |
|    189 |  2728 | `	ph7_vm *pVm = pCtx->pVm;` |
|    189 |  2729 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|    189 |  2730 | `	ph7_class *pFilter = 0;` |
|    189 |  2731 | `	const char *zFilter = 0;` |
|    189 |  2732 | `	int nFilter = 0;` |
|      - |  2733 | `	ph7_value *pSpec, *pOut;` |
|      - |  2734 | `	sxu32 n, i;` |
|    189 |  2735 | `	pOut = ph7_context_new_array(pCtx);` |
|    189 |  2736 | `	pSpec = ph7_context_new_array(pCtx);` |
|    189 |  2737 | `	if( pOut == 0 \|\| pSpec == 0 ){` |
|    ! 0 |  2738 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  2739 | `	}` |
|    189 |  2740 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     15 |  2741 | `		zFilter = ph7_value_to_string(apArg[0], &nFilter);` |
|     15 |  2742 | `		if( nFilter < 1 ){` |
|    ! 0 |  2743 | `			zFilter = 0;` |
|    ! 0 |  2744 | `		}` |
|      - |  2745 | `		/* IS_INSTANCEOF: keep a SUBCLASS of the named attribute too. The exact` |
|      - |  2746 | `		 * name still matches on its own, so this only has to answer for the rest. */` |
|     15 |  2747 | `		if( zFilter && nArg > 1 && (ph7_value_to_int(apArg[1]) & 2) ){` |
|      5 |  2748 | `			pFilter = ReflectResolveClass(pVm, apArg[0]);` |
|      2 |  2749 | `		}` |
|      7 |  2750 | `	}` |
|      - |  2751 | `	{` |
|    189 |  2752 | `		ph7_value *pKind = ph7_context_new_scalar(pCtx);` |
|    189 |  2753 | `		ph7_value *pMem  = ph7_context_new_scalar(pCtx);` |
|    189 |  2754 | `		ph7_value *pIdx  = ph7_context_new_scalar(pCtx);` |
|    189 |  2755 | `		if( pKind == 0 \|\| pMem == 0 \|\| pIdx == 0 ){` |
|    ! 0 |  2756 | `			return PH7_ContextMemoryError(pCtx);` |
|      - |  2757 | `		}` |
|    189 |  2758 | `		ph7_value_string(pKind, zKind, -1);` |
|    189 |  2759 | `		if( zMember ){` |
|     85 |  2760 | `			ph7_value_string(pMem, zMember, nMember);` |
|     44 |  2761 | `		}else{` |
|    107 |  2762 | `			ph7_value_null(pMem);` |
|      - |  2763 | `		}` |
|    189 |  2764 | `		ph7_value_int(pIdx, iParamIdx);` |
|    189 |  2765 | `		ph7_array_add_elem(pSpec, 0, pKind);` |
|      - |  2766 | `		/* The target rides as a VALUE, not a name: a closure's attributes are` |
|      - |  2767 | `		 * reopened through the Closure object itself. */` |
|    189 |  2768 | `		ph7_array_add_elem(pSpec, 0, pTarget);` |
|    189 |  2769 | `		ph7_array_add_elem(pSpec, 0, pMem);` |
|    189 |  2770 | `		ph7_array_add_elem(pSpec, 0, pIdx);` |
|      - |  2771 | `	}` |
|    395 |  2772 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|    211 |  2773 | `		int bRepeated = 0;` |
|    211 |  2774 | `		if( zFilter ){` |
|     89 |  2775 | `			int bKeep = ((int)SyStringLength(&aA[n].sName) == nFilter` |
|     50 |  2776 | `				&& SyStrnicmp(SyStringData(&aA[n].sName), zFilter, (sxu32)nFilter) == 0);` |
|     51 |  2777 | `			if( !bKeep && pFilter ){` |
|     16 |  2778 | `				ph7_class *pCand = PH7_VmExtractClass(pVm, SyStringData(&aA[n].sName),` |
|     10 |  2779 | `					SyStringLength(&aA[n].sName), FALSE, 0);` |
|     11 |  2780 | `				if( pCand == 0 ){` |
|    ! 0 |  2781 | `					pCand = PH7_VmTriggerAutoload(pVm, SyStringData(&aA[n].sName),` |
|    ! 0 |  2782 | `						SyStringLength(&aA[n].sName), FALSE);` |
|    ! 0 |  2783 | `				}` |
|     11 |  2784 | `				bKeep = pCand != 0 && pCand != pFilter && PH7_VmInstanceOf(pCand, pFilter);` |
|      5 |  2785 | `			}` |
|     51 |  2786 | `			if( !bKeep ){` |
|     27 |  2787 | `				continue;` |
|      - |  2788 | `			}` |
|     12 |  2789 | `		}` |
|      - |  2790 | `		/* isRepeated() asks about the TARGET, not about the filtered result:` |
|      - |  2791 | `		 * two #[A] make both of them repeated even when only one is asked for. */` |
|    399 |  2792 | `		for( i = 0 ; i < SySetUsed(pAttrs) ; i++ ){` |
|    254 |  2793 | `			if( i != n && SyStringLength(&aA[i].sName) == SyStringLength(&aA[n].sName)` |
|     75 |  2794 | `			 && SyStrnicmp(SyStringData(&aA[i].sName), SyStringData(&aA[n].sName),` |
|     66 |  2795 | `				SyStringLength(&aA[n].sName)) == 0 ){` |
|     42 |  2796 | `				bRepeated = 1;` |
|     42 |  2797 | `				break;` |
|      - |  2798 | `			}` |
|    112 |  2799 | `		}` |
|    275 |  2800 | `		ReflectTypeListAdd(pCtx, pOut,` |
|    180 |  2801 | `			ReflectAttrNew(pCtx, &aA[n].sName, pSpec, n, iTargetBit, bRepeated));` |
|     95 |  2802 | `	}` |
|    189 |  2803 | `	ph7_result_value(pCtx, pOut);` |
|    189 |  2804 | `	return PH7_OK;` |
|     97 |  2805 | `}` |
|      - |  2806 | `/*` |
|      - |  2807 | ` * The three "no runtime line tracking" methods. php answers a file/line/trace` |
|      - |  2808 | ` * from the executing frame; PHL has no per-instruction line record (the same` |
|      - |  2809 | ` * gap debug_backtrace() has), so it says so loudly rather than inventing one.` |
|      - |  2810 | ` */` |
|    ! 0 |  2811 | `static int ReflectUnsupported(ph7_context *pCtx, const char *zWho)` |
|    ! 0 |  2812 | `{` |
|    ! 0 |  2813 | `	return PH7_VmThrowException(pCtx, "Error",` |
|    ! 0 |  2814 | `		"%s is not supported by PHL (no runtime line tracking)", zWho);` |
|    ! 0 |  2815 | `}` |
|      - |  2816 | `#define REFLECT_UNSUPPORTED(NAME,TEXT) \` |
|      - |  2817 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  2818 | `	{ \` |
|      - |  2819 | `		SXUNUSED(nArg); \` |
|      - |  2820 | `		SXUNUSED(apArg); \` |
|      - |  2821 | `		return ReflectUnsupported(pCtx, TEXT); \` |
|      - |  2822 | `	}` |
|    ! 0 |  2823 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_execLine,"ReflectionGenerator::getExecutingLine()")` |
|    ! 0 |  2824 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_execFile,"ReflectionGenerator::getExecutingFile()")` |
|    ! 0 |  2825 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_trace,"ReflectionGenerator::getTrace()")` |
|    ! 0 |  2826 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_execLine,"ReflectionFiber::getExecutingLine()")` |
|    ! 0 |  2827 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_execFile,"ReflectionFiber::getExecutingFile()")` |
|    ! 0 |  2828 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_trace,"ReflectionFiber::getTrace()")` |
|      - |  2829 |  |
|      - |  2830 | `/* ReflectionGenerator::__construct(Generator $generator) */` |
|     18 |  2831 | `static int vm_builtin_ReflectionGenerator_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2832 | `{` |
|     19 |  2833 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     19 |  2834 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 |  2835 | `		return PH7_OK;` |
|      - |  2836 | `	}` |
|     19 |  2837 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RG_GEN, (ph7_class_instance *)apArg[0]->x.pOther);` |
|     19 |  2838 | `	return PH7_OK;` |
|     10 |  2839 | `}` |
|      - |  2840 | `/* The coroutine context of the wrapped generator, or NULL. */` |
|     36 |  2841 | `static ph7_exec_ctx * ReflectGenExec(ph7_context *pCtx)` |
|      1 |  2842 | `{` |
|     37 |  2843 | `	ph7_class_instance *pGenObj = ReflectWrapped(pCtx, RG_GEN);` |
|      - |  2844 | `	ph7_value sVal;` |
|      - |  2845 | `	ph7_generator *pGen;` |
|     37 |  2846 | `	if( pGenObj == 0 ){` |
|    ! 0 |  2847 | `		return 0;` |
|      - |  2848 | `	}` |
|     37 |  2849 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     37 |  2850 | `	sVal.x.pOther = pGenObj;` |
|     37 |  2851 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|     37 |  2852 | `	pGen = ReflectGeneratorCtx(pCtx->pVm, &sVal);` |
|     37 |  2853 | `	return pGen ? pGen->pCtx : 0;` |
|     19 |  2854 | `}` |
|      8 |  2855 | `static int vm_builtin_ReflectionGenerator_isClosed(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2856 | `{` |
|      9 |  2857 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|      4 |  2858 | `	SXUNUSED(nArg);` |
|      4 |  2859 | `	SXUNUSED(apArg);` |
|     17 |  2860 | `	ph7_result_bool(pCtx, pExec != 0` |
|     12 |  2861 | `		&& (pExec->iState == PH7_CTX_STATE_COMPLETED \|\| pExec->iState == PH7_CTX_STATE_CLOSED));` |
|      9 |  2862 | `	return PH7_OK;` |
|      1 |  2863 | `}` |
|      - |  2864 | `/* ReflectionGenerator::getThis(): ?object — the receiver the generator's body` |
|      - |  2865 | ` * runs against. A coroutine frame installs it as a frame VARIABLE (see` |
|      - |  2866 | ` * VmFiberSetupFrame), not as pFrame->pThis, so both are checked. */` |
|     10 |  2867 | `static int vm_builtin_ReflectionGenerator_getThis(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2868 | `{` |
|     11 |  2869 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|      5 |  2870 | `	SXUNUSED(nArg);` |
|      5 |  2871 | `	SXUNUSED(apArg);` |
|     11 |  2872 | `	if( pExec && pExec->pFrame ){` |
|     11 |  2873 | `		SyHashEntry *pVar = SyHashGet(&pExec->pFrame->hVar, "this", sizeof("this")-1);` |
|     11 |  2874 | `		if( pVar ){` |
|     10 |  2875 | `			ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,` |
|      6 |  2876 | `				(sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|      7 |  2877 | `			if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|      7 |  2878 | `				ph7_result_value(pCtx, pSlot);` |
|      7 |  2879 | `				return PH7_OK;` |
|      - |  2880 | `			}` |
|    ! 0 |  2881 | `		}` |
|      5 |  2882 | `		if( pExec->pFrame->pThis ){` |
|    ! 0 |  2883 | `			return ReflectResultBorrowed(pCtx, pExec->pFrame->pThis);` |
|      - |  2884 | `		}` |
|      2 |  2885 | `	}` |
|      5 |  2886 | `	ph7_result_null(pCtx);` |
|      5 |  2887 | `	return PH7_OK;` |
|      6 |  2888 | `}` |
|      - |  2889 | `/* ReflectionGenerator::getFunction(): ReflectionFunctionAbstract — still a` |
|      - |  2890 | ` * prelude class, so it is built through its own constructor. */` |
|     18 |  2891 | `static int vm_builtin_ReflectionGenerator_getFunction(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2892 | `{` |
|     19 |  2893 | `	ph7_vm *pVm = pCtx->pVm;` |
|     19 |  2894 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|     19 |  2895 | `	ph7_vm_func *pFunc = pExec ? pExec->pFunc : 0;` |
|      - |  2896 | `	ph7_class_instance *pOut;` |
|      - |  2897 | `	ph7_value aArg[2];` |
|     19 |  2898 | `	int nCtorArg = 1;` |
|      - |  2899 | `	sxi32 rc;` |
|      9 |  2900 | `	SXUNUSED(nArg);` |
|      9 |  2901 | `	SXUNUSED(apArg);` |
|     19 |  2902 | `	if( pFunc == 0 ){` |
|    ! 0 |  2903 | `		ph7_result_null(pCtx);` |
|    ! 0 |  2904 | `		return PH7_OK;` |
|      - |  2905 | `	}` |
|     19 |  2906 | `	PH7_MemObjInit(pVm, &aArg[0]);` |
|     19 |  2907 | `	PH7_MemObjInit(pVm, &aArg[1]);` |
|     19 |  2908 | `	if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      9 |  2909 | `		ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|      9 |  2910 | `		ph7_value_string(&aArg[0], SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|      9 |  2911 | `		ph7_value_string(&aArg[1], SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|      9 |  2912 | `		nCtorArg = 2;` |
|      5 |  2913 | `	}else{` |
|     11 |  2914 | `		ph7_value_string(&aArg[0], SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|      - |  2915 | `	}` |
|      - |  2916 | `	{` |
|      - |  2917 | `		ph7_value *apCtor[2];` |
|     19 |  2918 | `		apCtor[0] = &aArg[0];` |
|     19 |  2919 | `		apCtor[1] = &aArg[1];` |
|     28 |  2920 | `		pOut = ReflectConstruct(pCtx, nCtorArg == 2 ? "ReflectionMethod" : "ReflectionFunction",` |
|      9 |  2921 | `			nCtorArg, apCtor, &rc);` |
|      - |  2922 | `	}` |
|     19 |  2923 | `	PH7_MemObjRelease(&aArg[0]);` |
|     19 |  2924 | `	PH7_MemObjRelease(&aArg[1]);` |
|     19 |  2925 | `	if( pOut == 0 ){` |
|    ! 0 |  2926 | `		if( rc != PH7_OK ){` |
|    ! 0 |  2927 | `			return rc;` |
|      - |  2928 | `		}` |
|    ! 0 |  2929 | `		ph7_result_null(pCtx);` |
|    ! 0 |  2930 | `		return PH7_OK;` |
|      - |  2931 | `	}` |
|     19 |  2932 | `	return ReflectResultObject(pCtx, pOut);` |
|     10 |  2933 | `}` |
|      - |  2934 | `` /* ReflectionGenerator::getExecutingGenerator(): Generator — follow `yield from` `` |
|      - |  2935 | ` * delegation to the innermost one that is actually running. */` |
|      6 |  2936 | `static int vm_builtin_ReflectionGenerator_getExecuting(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2937 | `{` |
|      7 |  2938 | `	ph7_vm *pVm = pCtx->pVm;` |
|      7 |  2939 | `	ph7_class_instance *pGenObj = ReflectWrapped(pCtx, RG_GEN);` |
|      - |  2940 | `	ph7_value sVal, *pCur;` |
|      - |  2941 | `	ph7_generator *pGen;` |
|      7 |  2942 | `	int iDepth = 0;` |
|      3 |  2943 | `	SXUNUSED(nArg);` |
|      3 |  2944 | `	SXUNUSED(apArg);` |
|      7 |  2945 | `	if( pGenObj == 0 ){` |
|    ! 0 |  2946 | `		ph7_result_null(pCtx);` |
|    ! 0 |  2947 | `		return PH7_OK;` |
|      - |  2948 | `	}` |
|      7 |  2949 | `	PH7_MemObjInit(pVm, &sVal);` |
|      7 |  2950 | `	sVal.x.pOther = pGenObj;` |
|      7 |  2951 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|      7 |  2952 | `	pCur = &sVal;` |
|      7 |  2953 | `	pGen = ReflectGeneratorCtx(pVm, &sVal);` |
|     12 |  2954 | `	while( pGen && pGen->pCtx && pGen->pCtx->iDelegateState == 3` |
|     10 |  2955 | `	 && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|      3 |  2956 | `		ph7_generator *pInner = ReflectGeneratorCtx(pVm, &pGen->pCtx->sDelegate);` |
|      3 |  2957 | `		if( pInner == 0 ){` |
|    ! 0 |  2958 | `			break;` |
|      - |  2959 | `		}` |
|      3 |  2960 | `		pCur = &pGen->pCtx->sDelegate;` |
|      3 |  2961 | `		pGen = pInner;` |
|      3 |  2962 | `		iDepth++;` |
|      1 |  2963 | `	}` |
|      7 |  2964 | `	return ReflectResultBorrowed(pCtx, (ph7_class_instance *)pCur->x.pOther);` |
|      4 |  2965 | `}` |
|      - |  2966 | `/* ReflectionFiber::__construct(Fiber $fiber) / getFiber() / getCallable() */` |
|      4 |  2967 | `static int vm_builtin_ReflectionFiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2968 | `{` |
|      5 |  2969 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 |  2970 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 |  2971 | `		return PH7_OK;` |
|      - |  2972 | `	}` |
|      5 |  2973 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RF_FIBER, (ph7_class_instance *)apArg[0]->x.pOther);` |
|      5 |  2974 | `	return PH7_OK;` |
|      3 |  2975 | `}` |
|      4 |  2976 | `static int vm_builtin_ReflectionFiber_getFiber(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2977 | `{` |
|      2 |  2978 | `	SXUNUSED(nArg);` |
|      2 |  2979 | `	SXUNUSED(apArg);` |
|      5 |  2980 | `	return ReflectResultBorrowed(pCtx, ReflectWrapped(pCtx, RF_FIBER));` |
|      1 |  2981 | `}` |
|      4 |  2982 | `static int vm_builtin_ReflectionFiber_getCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2983 | `{` |
|      5 |  2984 | `	ph7_class_instance *pFiber = ReflectWrapped(pCtx, RF_FIBER);` |
|      5 |  2985 | `	ph7_value *pVal = pFiber ? PH7_NativeAttr(pFiber, "__callable") : 0;` |
|      2 |  2986 | `	SXUNUSED(nArg);` |
|      2 |  2987 | `	SXUNUSED(apArg);` |
|      5 |  2988 | `	if( pVal ){` |
|      5 |  2989 | `		ph7_result_value(pCtx, pVal);` |
|      2 |  2990 | `	}` |
|      5 |  2991 | `	return PH7_OK;` |
|      1 |  2992 | `}` |
|      - |  2993 | `/* Does this target carry a #[\Deprecated] attribute? */` |
|   4410 |  2994 | `static int ReflectHasDeprecated(SySet *pAttrs)` |
|      3 |  2995 | `{` |
|   4413 |  2996 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|      - |  2997 | `	sxu32 n;` |
|   4423 |  2998 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|     28 |  2999 | `		if( SyStringLength(&aA[n].sName) == sizeof("deprecated")-1` |
|     24 |  3000 | `		 && SyStrnicmp(SyStringData(&aA[n].sName), "deprecated", sizeof("deprecated")-1) == 0 ){` |
|     19 |  3001 | `			return 1;` |
|      - |  3002 | `		}` |
|      6 |  3003 | `	}` |
|   4395 |  3004 | `	return 0;` |
|   2208 |  3005 | `}` |
|      - |  3006 | `/* ---- ReflectionConstant ---- */` |
|      - |  3007 | `/* The engine's record for a global constant, or NULL when undefined. */` |
|   1784 |  3008 | `static ph7_constant * ReflectConstEntry(ph7_vm *pVm, const char *zName, int nName)` |
|      3 |  3009 | `{` |
|   1787 |  3010 | `	SyHashEntry *pEntry = nName > 0 ? PH7_VmConstantFetch(pVm, zName, (sxu32)nName, 1) : 0;` |
|   1787 |  3011 | `	return pEntry ? (ph7_constant *)pEntry->pUserData : 0;` |
|      3 |  3012 | `}` |
|      - |  3013 | `/* The receiver's constant record, resolved from its public $name. */` |
|    180 |  3014 | `static ph7_constant * ReflectConstOf(ph7_context *pCtx)` |
|      1 |  3015 | `{` |
|    181 |  3016 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    181 |  3017 | `	const char *zName = "";` |
|    181 |  3018 | `	int nName = 0;` |
|    181 |  3019 | `	if( pThis ){` |
|    181 |  3020 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     90 |  3021 | `	}` |
|    181 |  3022 | `	return ReflectConstEntry(pCtx->pVm, zName, nName);` |
|      1 |  3023 | `}` |
|    110 |  3024 | `static int vm_builtin_ReflectionConstant_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3025 | `{` |
|    111 |  3026 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    111 |  3027 | `	int nName = 0;` |
|    111 |  3028 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|    111 |  3029 | `	if( pThis == 0 ){` |
|    ! 0 |  3030 | `		return PH7_OK;` |
|      - |  3031 | `	}` |
|    111 |  3032 | `	if( ReflectConstEntry(pCtx->pVm, zName, nName) == 0 ){` |
|     10 |  3033 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 |  3034 | `			"Constant \"%.*s\" does not exist", nName, zName);` |
|      - |  3035 | `	}` |
|    105 |  3036 | `	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", zName, nName);` |
|    105 |  3037 | `	return PH7_OK;` |
|     56 |  3038 | `}` |
|      4 |  3039 | `static int vm_builtin_ReflectionConstant_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3040 | `{` |
|      5 |  3041 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 |  3042 | `	const char *zName = "";` |
|      5 |  3043 | `	int nName = 0;` |
|      2 |  3044 | `	SXUNUSED(nArg);` |
|      2 |  3045 | `	SXUNUSED(apArg);` |
|      5 |  3046 | `	if( pThis ){` |
|      5 |  3047 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 |  3048 | `	}` |
|      5 |  3049 | `	ph7_result_string(pCtx, zName, nName);` |
|      5 |  3050 | `	return PH7_OK;` |
|      1 |  3051 | `}` |
|      - |  3052 | `/* The namespace split php reports: everything before the last '\', and the rest. */` |
|      8 |  3053 | `static int ReflectConstNsCut(const char *zName, int nName)` |
|      1 |  3054 | `{` |
|      - |  3055 | `	int k;` |
|    101 |  3056 | `	for( k = nName - 1 ; k >= 0 ; k-- ){` |
|     93 |  3057 | `		if( zName[k] == '\\' ){` |
|    ! 0 |  3058 | `			return k;` |
|      - |  3059 | `		}` |
|     47 |  3060 | `	}` |
|      9 |  3061 | `	return -1;` |
|      5 |  3062 | `}` |
|      4 |  3063 | `static int vm_builtin_ReflectionConstant_getNamespaceName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3064 | `{` |
|      5 |  3065 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 |  3066 | `	const char *zName = "";` |
|      5 |  3067 | `	int nName = 0, iCut;` |
|      2 |  3068 | `	SXUNUSED(nArg);` |
|      2 |  3069 | `	SXUNUSED(apArg);` |
|      5 |  3070 | `	if( pThis ){` |
|      5 |  3071 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 |  3072 | `	}` |
|      5 |  3073 | `	iCut = ReflectConstNsCut(zName, nName);` |
|      5 |  3074 | `	ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|      5 |  3075 | `	return PH7_OK;` |
|      1 |  3076 | `}` |
|      4 |  3077 | `static int vm_builtin_ReflectionConstant_getShortName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3078 | `{` |
|      5 |  3079 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 |  3080 | `	const char *zName = "";` |
|      5 |  3081 | `	int nName = 0, iCut;` |
|      2 |  3082 | `	SXUNUSED(nArg);` |
|      2 |  3083 | `	SXUNUSED(apArg);` |
|      5 |  3084 | `	if( pThis ){` |
|      5 |  3085 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 |  3086 | `	}` |
|      5 |  3087 | `	iCut = ReflectConstNsCut(zName, nName);` |
|      5 |  3088 | `	if( iCut < 0 ){` |
|      5 |  3089 | `		ph7_result_string(pCtx, zName, nName);` |
|      3 |  3090 | `	}else{` |
|    ! 0 |  3091 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|      - |  3092 | `	}` |
|      5 |  3093 | `	return PH7_OK;` |
|      1 |  3094 | `}` |
|      8 |  3095 | `static int vm_builtin_ReflectionConstant_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3096 | `{` |
|      9 |  3097 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      - |  3098 | `	ph7_value sValue;` |
|      4 |  3099 | `	SXUNUSED(nArg);` |
|      4 |  3100 | `	SXUNUSED(apArg);` |
|      9 |  3101 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|      9 |  3102 | `	if( pCons && pCons->xExpand ){` |
|      9 |  3103 | `		pCons->xExpand(&sValue, pCons->pUserData);` |
|      4 |  3104 | `	}` |
|      9 |  3105 | `	ph7_result_value(pCtx, &sValue);` |
|      9 |  3106 | `	PH7_MemObjRelease(&sValue);` |
|      9 |  3107 | `	return PH7_OK;` |
|      1 |  3108 | `}` |
|     64 |  3109 | `static int vm_builtin_ReflectionConstant_isDeprecated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3110 | `{` |
|     65 |  3111 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|     32 |  3112 | `	SXUNUSED(nArg);` |
|     32 |  3113 | `	SXUNUSED(apArg);` |
|      - |  3114 | `	/* The same fact the E_DEPRECATED at a NAMING reads, and the same one the` |
|      - |  3115 | `	 * export tags -- reading it here does not raise the notice, which is why` |
|      - |  3116 | `	 * this asks the record rather than expanding the constant. */` |
|    129 |  3117 | `	ph7_result_bool(pCtx, pCons != 0` |
|     96 |  3118 | `		&& (pCons->zDeprecated != 0 \|\| ReflectHasDeprecated(&pCons->aAttrs)));` |
|     65 |  3119 | `	return PH7_OK;` |
|      1 |  3120 | `}` |
|      8 |  3121 | `static int vm_builtin_ReflectionConstant_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3122 | `{` |
|      9 |  3123 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      4 |  3124 | `	SXUNUSED(nArg);` |
|      4 |  3125 | `	SXUNUSED(apArg);` |
|      9 |  3126 | `	if( pCons && SyStringLength(&pCons->sFile) > 0 ){` |
|      5 |  3127 | `		ph7_result_string(pCtx, SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));` |
|      3 |  3128 | `	}else{` |
|      5 |  3129 | `		ph7_result_bool(pCtx, 0);` |
|      - |  3130 | `	}` |
|      9 |  3131 | `	return PH7_OK;` |
|      1 |  3132 | `}` |
|      - |  3133 | `/* The ReflectionExtension builder every reflector's getExtension() answers with` |
|      - |  3134 | ` * -- defined further down, beside the class partition it reads. */` |
|      - |  3135 | `static int ReflectExtensionOf(ph7_context *pCtx, int iExt);` |
|      - |  3136 | `/* An engine constant belongs to the extension the partition places its name in;` |
|      - |  3137 | ` * a userland define() belongs to none, which php reports as null / false. */` |
|     38 |  3138 | `static int ReflectConstExtId(ph7_context *pCtx)` |
|      1 |  3139 | `{` |
|     39 |  3140 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|     39 |  3141 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     39 |  3142 | `	const char *zName = "";` |
|     39 |  3143 | `	int nName = 0;` |
|     39 |  3144 | `	if( pCons == 0 \|\| pCons->bUserDefined ){` |
|      9 |  3145 | `		return -1;` |
|      - |  3146 | `	}` |
|     31 |  3147 | `	if( pThis ){` |
|     31 |  3148 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     15 |  3149 | `	}` |
|     31 |  3150 | `	return PH7_VmExtOfConstant(zName, nName);` |
|     20 |  3151 | `}` |
|      8 |  3152 | `static int vm_builtin_ReflectionConstant_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3153 | `{` |
|      4 |  3154 | `	SXUNUSED(nArg);` |
|      4 |  3155 | `	SXUNUSED(apArg);` |
|      9 |  3156 | `	return ReflectExtensionOf(pCtx, ReflectConstExtId(pCtx));` |
|      1 |  3157 | `}` |
|     30 |  3158 | `static int vm_builtin_ReflectionConstant_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3159 | `{` |
|     31 |  3160 | `	int iExt = ReflectConstExtId(pCtx);` |
|     15 |  3161 | `	SXUNUSED(nArg);` |
|     15 |  3162 | `	SXUNUSED(apArg);` |
|     31 |  3163 | `	if( iExt < 0 ){` |
|      7 |  3164 | `		ph7_result_bool(pCtx, 0);` |
|      4 |  3165 | `	}else{` |
|     25 |  3166 | `		ph7_result_string(pCtx, PH7_VmExtensionName(iExt), -1);` |
|      - |  3167 | `	}` |
|     31 |  3168 | `	return PH7_OK;` |
|      1 |  3169 | `}` |
|      - |  3170 | `/* getAttributes() still routes through the prelude builder: chunk 7 owns the` |
|      - |  3171 | ` * ReflectionAttribute shape and the lazy argument evaluation. */` |
|      2 |  3172 | `static int vm_builtin_ReflectionConstant_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3173 | `{` |
|      3 |  3174 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      3 |  3175 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      3 |  3176 | `	const char *zName = "";` |
|      3 |  3177 | `	int nName = 0;` |
|      3 |  3178 | `	if( pThis == 0 \|\| pCons == 0 ){` |
|    ! 0 |  3179 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  3180 | `		return PH7_OK;` |
|      - |  3181 | `	}` |
|      3 |  3182 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      - |  3183 | `	{` |
|      - |  3184 | `		ph7_value sTarget;` |
|      - |  3185 | `		int rc;` |
|      3 |  3186 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      3 |  3187 | `		ph7_value_string(&sTarget, zName, nName);` |
|      - |  3188 | `		/* 64 = Attribute::TARGET_CONSTANT */` |
|      4 |  3189 | `		rc = ReflectBuildAttrs(pCtx, &pCons->aAttrs, "const", &sTarget, 0, 0, 0, 64,` |
|      1 |  3190 | `			nArg, apArg);` |
|      3 |  3191 | `		PH7_MemObjRelease(&sTarget);` |
|      3 |  3192 | `		return rc;` |
|      - |  3193 | `	}` |
|      2 |  3194 | `}` |
|      - |  3195 | `/* php's ZEND_ACC_NO_FILE_CACHE: the two constants whose value belongs to the` |
|      - |  3196 | ` * RUNNING process rather than to the build, so opcache must not bake them in. */` |
|    872 |  3197 | `static int ReflectConstNoFileCache(const SyString *pName)` |
|      1 |  3198 | `{` |
|      - |  3199 | `	static const char * const azNoCache[] = { "PHP_BINARY", "PHP_SAPI" };` |
|      - |  3200 | `	sxu32 n;` |
|   2609 |  3201 | `	for( n = 0 ; n < SX_ARRAYSIZE(azNoCache) ; ++n ){` |
|   1742 |  3202 | `		if( SyStringLength(pName) == SyStrlen(azNoCache[n])` |
|    932 |  3203 | `		 && SyMemcmp(SyStringData(pName), azNoCache[n], SyStringLength(pName)) == 0 ){` |
|      7 |  3204 | `			return 1;` |
|      - |  3205 | `		}` |
|    869 |  3206 | `	}` |
|    867 |  3207 | `	return 0;` |
|    437 |  3208 | `}` |
|      - |  3209 | `/*` |
|      - |  3210 | `` * php's `Constant [ <persistent> int JSON_HEX_TAG ] { 1 }` -- one line, and the`` |
|      - |  3211 | `` * same one an extension's Constants block lists. `<persistent>` is the ENGINE's`` |
|      - |  3212 | ` * own: a define()d constant carries no tag at all, and one php deprecated the` |
|      - |  3213 | `` * SYMBOL of reads `<persistent, deprecated>`. The value is taken from the`` |
|      - |  3214 | ` * constant's expander DIRECTLY, so asking for the export does not raise the` |
|      - |  3215 | ` * E_DEPRECATED that naming it would.` |
|      - |  3216 | ` */` |
|    932 |  3217 | `static void ReflectExportGlobalConstLine(ph7_context *pCtx, SyBlob *pOut, ph7_constant *pCons)` |
|      1 |  3218 | `{` |
|      - |  3219 | `	ph7_value sVal;` |
|      - |  3220 | `	const char *zType;` |
|    933 |  3221 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    933 |  3222 | `	if( pCons->xExpand ){` |
|    933 |  3223 | `		pCons->xExpand(&sVal, pCons->pUserData);` |
|    466 |  3224 | `	}` |
|    933 |  3225 | `	SyBlobAppend(pOut, "Constant [ ", sizeof("Constant [ ")-1);` |
|    933 |  3226 | `	zType = ReflectExportTypeWord(&sVal);` |
|      - |  3227 | ``	/* php's flag words, in its order. `persistent` is a MODULE's registration`` |
|      - |  3228 | `	 * and a define()d constant has none; the three standard streams are the` |
|      - |  3229 | `	 * CLI SAPI's own per-REQUEST constants and have none either, which is what` |
|      - |  3230 | ``	 * the resource test below says. `no_file_cache` is php's opcache flag, and`` |
|      - |  3231 | `	 * it marks exactly the two constants whose value is this process's. */` |
|    933 |  3232 | `	if( !pCons->bUserDefined && (sVal.iFlags & MEMOBJ_RES) == 0 ){` |
|    893 |  3233 | `		if( pCons->zDeprecated ){` |
|     21 |  3234 | `			SyBlobAppend(pOut, "<persistent, deprecated> ", sizeof("<persistent, deprecated> ")-1);` |
|    883 |  3235 | `		}else if( ReflectConstNoFileCache(&pCons->sName) ){` |
|      7 |  3236 | `			SyBlobAppend(pOut, "<persistent, no_file_cache> ",` |
|      - |  3237 | `				sizeof("<persistent, no_file_cache> ")-1);` |
|      4 |  3238 | `		}else{` |
|    867 |  3239 | `			SyBlobAppend(pOut, "<persistent> ", sizeof("<persistent> ")-1);` |
|      1 |  3240 | `		}` |
|    487 |  3241 | `	}else if( ReflectHasDeprecated(&pCons->aAttrs) ){` |
|      7 |  3242 | `		SyBlobAppend(pOut, "<deprecated> ", sizeof("<deprecated> ")-1);` |
|      3 |  3243 | `	}` |
|    933 |  3244 | `	if( zType ){` |
|    933 |  3245 | `		SyBlobFormat(pOut, "%s ", zType);` |
|    467 |  3246 | `	}else{` |
|    ! 0 |  3247 | `		SyBlobFormat(pOut, "%z ", &((ph7_class_instance *)sVal.x.pOther)->pClass->sName);` |
|      - |  3248 | `	}` |
|    933 |  3249 | `	SyBlobFormat(pOut, "%z ] { ", &pCons->sName);` |
|    933 |  3250 | `	ReflectExportConstValue(pCtx, pOut, &sVal);` |
|    933 |  3251 | `	SyBlobAppend(pOut, " }\n", sizeof(" }\n")-1);` |
|    933 |  3252 | `	PH7_MemObjRelease(&sVal);` |
|    933 |  3253 | `}` |
|     60 |  3254 | `static int vm_builtin_ReflectionConstant_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3255 | `{` |
|     61 |  3256 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      - |  3257 | `	SyBlob sOut;` |
|     30 |  3258 | `	SXUNUSED(nArg);` |
|     30 |  3259 | `	SXUNUSED(apArg);` |
|     61 |  3260 | `	if( pCons == 0 ){` |
|    ! 0 |  3261 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 |  3262 | `		return PH7_OK;` |
|      - |  3263 | `	}` |
|     61 |  3264 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     61 |  3265 | `	ReflectExportGlobalConstLine(pCtx, &sOut, pCons);` |
|     61 |  3266 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     61 |  3267 | `	SyBlobRelease(&sOut);` |
|     61 |  3268 | `	return PH7_OK;` |
|     31 |  3269 | `}` |
|      - |  3270 | `/* ---- ReflectionExtension: one per name this build reports as loaded ---- */` |
|      - |  3271 | `/*` |
|      - |  3272 | ` * php matches the name case-insensitively and then KEEPS its own spelling, so` |
|      - |  3273 | `` * `new ReflectionExtension('DATE')` reports `date` and `spl` reports `SPL`.`` |
|      - |  3274 | `` * A `phl.stub_extensions` name is loaded too and has no canonical spelling of`` |
|      - |  3275 | ` * its own, so it keeps the caller's.` |
|      - |  3276 | ` */` |
|    105 |  3277 | `static int vm_builtin_ReflectionExtension_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  3278 | `{` |
|    109 |  3279 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    109 |  3280 | `	int nName = 0, iExt;` |
|    109 |  3281 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|    109 |  3282 | `	if( pThis == 0 ){` |
|    ! 0 |  3283 | `		return PH7_OK;` |
|      - |  3284 | `	}` |
|    109 |  3285 | `	iExt = PH7_VmExtensionLookup(zName, nName);` |
|    109 |  3286 | `	if( iExt >= 0 ){` |
|     99 |  3287 | `		const char *zCanon = PH7_VmExtensionName(iExt);` |
|     99 |  3288 | `		PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", zCanon, (int)SyStrlen(zCanon));` |
|     99 |  3289 | `		return PH7_OK;` |
|      - |  3290 | `	}` |
|     12 |  3291 | `	if( PH7_VmExtensionIsLoaded(pCtx->pVm, zName, nName) ){` |
|    ! 0 |  3292 | `		PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", zName, nName);` |
|    ! 0 |  3293 | `		return PH7_OK;` |
|      - |  3294 | `	}` |
|     17 |  3295 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      5 |  3296 | `		"Extension \"%.*s\" does not exist", nName, zName);` |
|     55 |  3297 | `}` |
|     24 |  3298 | `static int vm_builtin_ReflectionExtension_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  3299 | `{` |
|     26 |  3300 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     26 |  3301 | `	const char *zName = "";` |
|     26 |  3302 | `	int nName = 0;` |
|     12 |  3303 | `	SXUNUSED(nArg);` |
|     12 |  3304 | `	SXUNUSED(apArg);` |
|     26 |  3305 | `	if( pThis ){` |
|     26 |  3306 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     12 |  3307 | `	}` |
|     26 |  3308 | `	ph7_result_string(pCtx, zName, nName);` |
|     26 |  3309 | `	return PH7_OK;` |
|      2 |  3310 | `}` |
|    ! 0 |  3311 | `static int vm_builtin_ReflectionExtension_getVersion(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3312 | `{` |
|      - |  3313 | `	ph7_value sFn, sRes;` |
|      - |  3314 | `	SyString sStr;` |
|    ! 0 |  3315 | `	SXUNUSED(nArg);` |
|    ! 0 |  3316 | `	SXUNUSED(apArg);` |
|    ! 0 |  3317 | `	PH7_MemObjInit(pCtx->pVm, &sFn);` |
|    ! 0 |  3318 | `	PH7_MemObjInit(pCtx->pVm, &sRes);` |
|    ! 0 |  3319 | `	SyStringInitFromBuf(&sStr, "phpversion", sizeof("phpversion")-1);` |
|    ! 0 |  3320 | `	PH7_MemObjInitFromString(pCtx->pVm, &sFn, &sStr);` |
|    ! 0 |  3321 | `	if( PH7_VmCallUserFunction(pCtx->pVm, &sFn, 0, 0, &sRes) == SXRET_OK ){` |
|    ! 0 |  3322 | `		ph7_result_value(pCtx, &sRes);` |
|    ! 0 |  3323 | `	}` |
|    ! 0 |  3324 | `	PH7_MemObjRelease(&sFn);` |
|    ! 0 |  3325 | `	PH7_MemObjRelease(&sRes);` |
|    ! 0 |  3326 | `	return PH7_OK;` |
|    ! 0 |  3327 | `}` |
|      - |  3328 | `/*` |
|      - |  3329 | ` * The four listings an extension answers about ITSELF -- getFunctions(),` |
|      - |  3330 | ` * getClasses()/getClassNames(), getConstants() and getINIEntries(). All of them` |
|      - |  3331 | ` * are the partition walked in php's own registration order (which is not` |
|      - |  3332 | ` * alphabetical) and filtered against the live VM, so a build without one of the` |
|      - |  3333 | ` * compile-time extensions simply lists nothing for it.` |
|      - |  3334 | ` */` |
|      - |  3335 | `#define REFLECT_EXT_MAP        0   /* name => a fresh reflector over it */` |
|      - |  3336 | `#define REFLECT_EXT_NAMES      1   /* a plain LIST of the names */` |
|      - |  3337 | `#define REFLECT_EXT_VALUES     2   /* name => the value the engine holds */` |
|      - |  3338 | `typedef struct ReflectExtList ReflectExtList;` |
|      - |  3339 | `struct ReflectExtList {` |
|      - |  3340 | `	ph7_context *pCtx;` |
|      - |  3341 | `	ph7_value *pList;   /* the array being built */` |
|      - |  3342 | `	ph7_value *pVal;    /* one scratch value, reused for every entry */` |
|      - |  3343 | `	int iKind;          /* PH7_EXT_KIND_* */` |
|      - |  3344 | `	int iShape;         /* REFLECT_EXT_* */` |
|      - |  3345 | `};` |
|      - |  3346 | ``/* A reflector over one internal name, built with its `name` slot already filled:`` |
|      - |  3347 | ` * the constructor would only re-resolve what this walk already has. */` |
|    286 |  3348 | `static int ReflectExtMakeReflector(ph7_context *pCtx, const char *zRefl,` |
|      - |  3349 | `	const char *zName, int nName, ph7_value *pOut)` |
|      2 |  3350 | `{` |
|    288 |  3351 | `	ph7_vm *pVm = pCtx->pVm;` |
|    288 |  3352 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, zRefl, (sxu32)SyStrlen(zRefl), FALSE, 0);` |
|      - |  3353 | `	ph7_class_instance *pObj;` |
|    288 |  3354 | `	if( pClass == 0 \|\| (pObj = PH7_NewClassInstance(pVm, pClass)) == 0 ){` |
|    ! 0 |  3355 | `		return 0;` |
|      - |  3356 | `	}` |
|    288 |  3357 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", zName, nName);` |
|    288 |  3358 | `	PH7_MemObjRelease(pOut);` |
|    288 |  3359 | `	pOut->x.pOther = pObj;` |
|    288 |  3360 | `	pOut->iFlags = MEMOBJ_OBJ;` |
|    288 |  3361 | `	return 1;` |
|    145 |  3362 | `}` |
|   1268 |  3363 | `static int ReflectExtListStep(const char *zName, int nName, void *pData)` |
|      4 |  3364 | `{` |
|   1272 |  3365 | `	ReflectExtList *p = (ReflectExtList *)pData;` |
|   1272 |  3366 | `	ph7_context *pCtx = p->pCtx;` |
|      - |  3367 | `	SyBlob sKey;` |
|      - |  3368 | `	char *zKey;` |
|   1272 |  3369 | `	int i, rc = 0;` |
|   1272 |  3370 | `	if( !PH7_VmInternalNameExists(pCtx->pVm, p->iKind, zName, nName) ){` |
|    ! 0 |  3371 | `		return 0;` |
|      - |  3372 | `	}` |
|      - |  3373 | `	/* ph7_array_add_strkey_elem() takes a NUL-terminated key, and the walk has` |
|      - |  3374 | `	 * only a name and a length -- so the key is built rather than borrowed,` |
|      - |  3375 | `	 * which is also where the fold happens. */` |
|   1272 |  3376 | `	SyBlobInit(&sKey, &pCtx->pVm->sAllocator);` |
|   1272 |  3377 | `	SyBlobAppend(&sKey, (const void *)zName, (sxu32)nName);` |
|   1272 |  3378 | `	SyBlobAppend(&sKey, (const void *)"", 1);` |
|   1272 |  3379 | `	zKey = (char *)SyBlobData(&sKey);` |
|   1272 |  3380 | `	if( p->iKind == PH7_EXT_KIND_FUNC ){` |
|      - |  3381 | `		/* php keys the map with the name the engine STORES, which is folded. */` |
|   5686 |  3382 | `		for( i = 0 ; i < nName ; ++i ){` |
|   5412 |  3383 | `			zKey[i] = (char)SyToLower(zKey[i]);` |
|   2707 |  3384 | `		}` |
|    137 |  3385 | `	}` |
|   1272 |  3386 | `	switch( p->iShape ){` |
|    108 |  3387 | `		case REFLECT_EXT_NAMES:` |
|    215 |  3388 | `			ph7_value_reset_string_cursor(p->pVal);` |
|    215 |  3389 | `			ph7_value_string(p->pVal, zName, nName);` |
|    215 |  3390 | `			ph7_array_add_elem(p->pList, 0, p->pVal);` |
|    215 |  3391 | `			SyBlobRelease(&sKey);` |
|    215 |  3392 | `			return 0;` |
|    513 |  3393 | `		case REFLECT_EXT_VALUES:` |
|    773 |  3394 | `			if( p->iKind == PH7_EXT_KIND_INI ){` |
|      - |  3395 | `				SyBlob sVal;` |
|    151 |  3396 | `				SyBlobInit(&sVal, &pCtx->pVm->sAllocator);` |
|    151 |  3397 | `				PH7_VmIniGetStr(pCtx->pVm, zKey, &sVal);` |
|    151 |  3398 | `				ph7_value_reset_string_cursor(p->pVal);` |
|    151 |  3399 | `				if( PH7_VmIniIsUnset(pCtx->pVm, zKey) ){` |
|      - |  3400 | `					/* php shows the RAW value here, so a directive declared with` |
|      - |  3401 | `					 * no value is NULL rather than the empty string ini_get()` |
|      - |  3402 | `					 * makes of it. */` |
|      9 |  3403 | `					PH7_MemObjRelease(p->pVal);` |
|      5 |  3404 | `				}else{` |
|    197 |  3405 | `					ph7_value_string(p->pVal, (const char *)SyBlobData(&sVal),` |
|    141 |  3406 | `						(int)SyBlobLength(&sVal));` |
|      - |  3407 | `				}` |
|    151 |  3408 | `				SyBlobRelease(&sVal);` |
|     60 |  3409 | `			}else{` |
|    625 |  3410 | `				ph7_constant *pCons = ReflectConstEntry(pCtx->pVm, zName, nName);` |
|    625 |  3411 | `				PH7_MemObjRelease(p->pVal);` |
|    625 |  3412 | `				if( pCons && pCons->xExpand ){` |
|      - |  3413 | `					/* Describing the table is not READING an entry: php's` |
|      - |  3414 | `					 * deprecated constants report when a program names one,` |
|      - |  3415 | `					 * and a listing is silent (get_defined_constants()'s` |
|      - |  3416 | `					 * own rule, and the same switch). */` |
|    625 |  3417 | `					pCtx->pVm->bConstEnum++;` |
|    625 |  3418 | `					pCons->xExpand(p->pVal, pCons->pUserData);` |
|    625 |  3419 | `					pCtx->pVm->bConstEnum--;` |
|    200 |  3420 | `				}` |
|      - |  3421 | `			}` |
|    773 |  3422 | `			break;` |
|    143 |  3423 | `		default:` |
|    431 |  3424 | `			if( !ReflectExtMakeReflector(pCtx,` |
|    286 |  3425 | `					p->iKind == PH7_EXT_KIND_CLASS ? "ReflectionClass" : "ReflectionFunction",` |
|    143 |  3426 | `					zName, nName, p->pVal) ){` |
|    ! 0 |  3427 | `				SyBlobRelease(&sKey);` |
|    ! 0 |  3428 | `				return 0;` |
|      - |  3429 | `			}` |
|    286 |  3430 | `			break;` |
|      - |  3431 | `	}` |
|   1060 |  3432 | `	ph7_array_add_strkey_elem(p->pList, zKey, p->pVal);` |
|   1060 |  3433 | `	SyBlobRelease(&sKey);` |
|   1060 |  3434 | `	return rc;` |
|    508 |  3435 | `}` |
|     68 |  3436 | `static int ReflectExtListing(ph7_context *pCtx, int iKind, int iShape)` |
|      4 |  3437 | `{` |
|     72 |  3438 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  3439 | `	ReflectExtList sWalk;` |
|     72 |  3440 | `	const char *zName = "";` |
|     72 |  3441 | `	int nName = 0, iExt;` |
|     72 |  3442 | `	sWalk.pCtx = pCtx;` |
|     72 |  3443 | `	sWalk.iKind = iKind;` |
|     72 |  3444 | `	sWalk.iShape = iShape;` |
|     72 |  3445 | `	sWalk.pList = ph7_context_new_array(pCtx);` |
|     72 |  3446 | `	sWalk.pVal = ph7_context_new_scalar(pCtx);` |
|     72 |  3447 | `	if( sWalk.pList == 0 \|\| sWalk.pVal == 0 ){` |
|    ! 0 |  3448 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  3449 | `	}` |
|     72 |  3450 | `	if( pThis ){` |
|     72 |  3451 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     32 |  3452 | `	}` |
|      - |  3453 | ``	/* A `phl.stub_extensions` name has no id and synthesizes nothing, so its`` |
|      - |  3454 | `	 * every listing is empty -- which is also what php answers for a module` |
|      - |  3455 | `	 * that registers none of that kind. */` |
|     72 |  3456 | `	iExt = PH7_VmExtensionLookup(zName, nName);` |
|     72 |  3457 | `	if( iExt >= 0 ){` |
|     72 |  3458 | `		PH7_VmExtWalk(iExt, iKind, ReflectExtListStep, &sWalk);` |
|     32 |  3459 | `	}` |
|     72 |  3460 | `	ph7_result_value(pCtx, sWalk.pList);` |
|     72 |  3461 | `	return PH7_OK;` |
|     36 |  3462 | `}` |
|      - |  3463 | `#define REFLECT_EXT_LISTING(NAME,KIND,SHAPE) \` |
|      - |  3464 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  3465 | `	{ \` |
|      - |  3466 | `		SXUNUSED(nArg); \` |
|      - |  3467 | `		SXUNUSED(apArg); \` |
|      - |  3468 | `		return ReflectExtListing(pCtx, KIND, SHAPE); \` |
|      - |  3469 | `	}` |
|     14 |  3470 | `REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getFunctions,` |
|      2 |  3471 | `	PH7_EXT_KIND_FUNC,  REFLECT_EXT_MAP)` |
|      7 |  3472 | `REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getClasses,` |
|      1 |  3473 | `	PH7_EXT_KIND_CLASS, REFLECT_EXT_MAP)` |
|     24 |  3474 | `REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getClassNames,` |
|      3 |  3475 | `	PH7_EXT_KIND_CLASS, REFLECT_EXT_NAMES)` |
|     20 |  3476 | `REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getConstants,` |
|      3 |  3477 | `	PH7_EXT_KIND_CONST, REFLECT_EXT_VALUES)` |
|     15 |  3478 | `REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getINIEntries,` |
|      3 |  3479 | `	PH7_EXT_KIND_INI,   REFLECT_EXT_VALUES)` |
|      6 |  3480 | `static int ReflectExtDepStep(const char *zOn, const char *zKind, void *pData)` |
|      1 |  3481 | `{` |
|      7 |  3482 | `	ReflectExtList *p = (ReflectExtList *)pData;` |
|      7 |  3483 | `	ph7_value_reset_string_cursor(p->pVal);` |
|      7 |  3484 | `	ph7_value_string(p->pVal, zKind, -1);` |
|      7 |  3485 | `	ph7_array_add_strkey_elem(p->pList, zOn, p->pVal);` |
|      7 |  3486 | `	return 0;` |
|      1 |  3487 | `}` |
|      8 |  3488 | `static int vm_builtin_ReflectionExtension_getDependencies(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3489 | `{` |
|      9 |  3490 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  3491 | `	ReflectExtList sWalk;` |
|      9 |  3492 | `	const char *zName = "";` |
|      9 |  3493 | `	int nName = 0, iExt;` |
|      4 |  3494 | `	SXUNUSED(nArg);` |
|      4 |  3495 | `	SXUNUSED(apArg);` |
|      9 |  3496 | `	sWalk.pList = ph7_context_new_array(pCtx);` |
|      9 |  3497 | `	sWalk.pVal = ph7_context_new_scalar(pCtx);` |
|      9 |  3498 | `	if( sWalk.pList == 0 \|\| sWalk.pVal == 0 ){` |
|    ! 0 |  3499 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  3500 | `	}` |
|      9 |  3501 | `	if( pThis ){` |
|      9 |  3502 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      4 |  3503 | `	}` |
|      9 |  3504 | `	iExt = PH7_VmExtensionLookup(zName, nName);` |
|      9 |  3505 | `	if( iExt >= 0 ){` |
|      9 |  3506 | `		sWalk.pCtx = pCtx;` |
|      9 |  3507 | `		PH7_VmExtWalkDep(iExt, ReflectExtDepStep, &sWalk);` |
|      4 |  3508 | `	}` |
|      9 |  3509 | `	ph7_result_value(pCtx, sWalk.pList);` |
|      9 |  3510 | `	return PH7_OK;` |
|      5 |  3511 | `}` |
|    ! 0 |  3512 | `static int vm_builtin_ReflectionExtension_isPersistent(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3513 | `{` |
|    ! 0 |  3514 | `	SXUNUSED(nArg);` |
|    ! 0 |  3515 | `	SXUNUSED(apArg);` |
|    ! 0 |  3516 | `	ph7_result_bool(pCtx, 1);` |
|    ! 0 |  3517 | `	return PH7_OK;` |
|    ! 0 |  3518 | `}` |
|    ! 0 |  3519 | `static int vm_builtin_ReflectionExtension_isTemporary(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3520 | `{` |
|    ! 0 |  3521 | `	SXUNUSED(nArg);` |
|    ! 0 |  3522 | `	SXUNUSED(apArg);` |
|    ! 0 |  3523 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 |  3524 | `	return PH7_OK;` |
|    ! 0 |  3525 | `}` |
|    ! 0 |  3526 | `static int vm_builtin_ReflectionExtension_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3527 | `{` |
|    ! 0 |  3528 | `	SXUNUSED(nArg);` |
|    ! 0 |  3529 | `	SXUNUSED(apArg);` |
|    ! 0 |  3530 | `	SXUNUSED(pCtx);` |
|    ! 0 |  3531 | `	return PH7_OK;` |
|    ! 0 |  3532 | `}` |
|      - |  3533 | `/*` |
|      - |  3534 | ` * ---------------------------------------------------------------------------` |
|      - |  3535 | ` * The EXTENSION block -- php's whole module, and the last export this engine` |
|      - |  3536 | `` * did not have (it printed one placeholder line, `Extension [ extension #1`` |
|      - |  3537 | `` * name ]`). It NESTS the function and class blocks, which is why it waited for`` |
|      - |  3538 | ` * them: its bytes cannot match php's until theirs do.` |
|      - |  3539 | ` *` |
|      - |  3540 | ` * php's shape, measured against 8.5.9:` |
|      - |  3541 | ` *` |
|      - |  3542 | ` *   Extension [ <persistent> extension #N name version V ] {` |
|      - |  3543 | ` *   <blank>` |
|      - |  3544 | ` *     - Dependencies { … }        each section only when it has rows,` |
|      - |  3545 | ` *   <blank>                       in this order, and each preceded by a` |
|      - |  3546 | ` *     - INI { … }                 blank line -- an extension with no rows` |
|      - |  3547 | `` *   <blank>                       at all prints `{` and `}` on consecutive`` |
|      - |  3548 | ` *     - Constants [C] { … }       lines instead.` |
|      - |  3549 | ` *   <blank>` |
|      - |  3550 | ` *     - Functions { … }` |
|      - |  3551 | ` *   <blank>` |
|      - |  3552 | ` *     - Classes [K] { … }` |
|      - |  3553 | ` *   }` |
|      - |  3554 | ` *` |
|      - |  3555 | ` * A nested block is the STANDALONE export with four spaces on every non-empty` |
|      - |  3556 | ` * line -- verified byte for byte against php, which is what lets this reuse` |
|      - |  3557 | ` * the two block builders rather than threading an indent through them.` |
|      - |  3558 | ` * ---------------------------------------------------------------------------` |
|      - |  3559 | ` */` |
|      - |  3560 | `static void ReflectExportFuncByName(ph7_context *pCtx, SyBlob *pOut,` |
|      - |  3561 | `	const char *zName, int nName);` |
|      - |  3562 | `static sxi32 ReflectExportClassBlock(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass);` |
|      - |  3563 | `/* Copy pIn into pOut with nPad spaces in front of every non-empty line. */` |
|   1374 |  3564 | `static void ReflectExportPad(SyBlob *pOut, SyBlob *pIn, int nPad)` |
|      2 |  3565 | `{` |
|   1376 |  3566 | `	const char *zIn = (const char *)SyBlobData(pIn);` |
|   1376 |  3567 | `	sxu32 nIn = SyBlobLength(pIn), i = 0;` |
|  33598 |  3568 | `	while( i < nIn ){` |
|  32224 |  3569 | `		sxu32 j = i;` |
| 772979 |  3570 | `		while( j < nIn && zIn[j] != '\n' ){` |
| 740757 |  3571 | `			j++;` |
|      2 |  3572 | `		}` |
|  32224 |  3573 | `		if( j > i ){` |
|      - |  3574 | `			int k;` |
| 120822 |  3575 | `			for( k = 0 ; k < nPad ; k++ ){` |
|  96658 |  3576 | `				SyBlobAppend(pOut, " ", sizeof(char));` |
|  48330 |  3577 | `			}` |
|  24166 |  3578 | `			SyBlobAppend(pOut, &zIn[i], j - i);` |
|  12082 |  3579 | `		}` |
|  32224 |  3580 | `		if( j < nIn ){` |
|  32224 |  3581 | `			SyBlobAppend(pOut, "\n", sizeof(char));` |
|  16111 |  3582 | `		}` |
|  32224 |  3583 | `		i = j + 1;` |
|      2 |  3584 | `	}` |
|   1376 |  3585 | `}` |
|      - |  3586 | `typedef struct ReflectExtDump ReflectExtDump;` |
|      - |  3587 | `struct ReflectExtDump {` |
|      - |  3588 | `	ph7_context *pCtx;` |
|      - |  3589 | `	SyBlob *pOut;     /* the section body, already indented */` |
|      - |  3590 | `	int iKind;` |
|      - |  3591 | `	sxu32 nRow;` |
|      - |  3592 | `};` |
|      - |  3593 | ``/* php's access word for an ini directive: the whole mask is `ALL`, and`` |
|      - |  3594 | ` * anything else is the set bits joined with a comma in php's own order. */` |
|     62 |  3595 | `static void ReflectExtIniAccess(SyBlob *pOut, sxi32 iAccess)` |
|      1 |  3596 | `{` |
|      - |  3597 | `	static const struct { sxi32 iBit; const char *zWord; } aBit[] = {` |
|      - |  3598 | `		{ 1, "USER" }, { 2, "PERDIR" }, { 4, "SYSTEM" }` |
|      - |  3599 | `	};` |
|      - |  3600 | `	sxu32 n;` |
|     63 |  3601 | `	int bFirst = 1;` |
|     63 |  3602 | `	if( (iAccess & 7) == 7 ){` |
|     43 |  3603 | `		SyBlobAppend(pOut, "ALL", sizeof("ALL")-1);` |
|     43 |  3604 | `		return;` |
|      - |  3605 | `	}` |
|     81 |  3606 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBit) ; ++n ){` |
|     61 |  3607 | `		if( iAccess & aBit[n].iBit ){` |
|     33 |  3608 | `			if( !bFirst ){` |
|     13 |  3609 | `				SyBlobAppend(pOut, ",", sizeof(char));` |
|      6 |  3610 | `			}` |
|     33 |  3611 | `			SyBlobAppend(pOut, aBit[n].zWord, SyStrlen(aBit[n].zWord));` |
|     33 |  3612 | `			bFirst = 0;` |
|     16 |  3613 | `		}` |
|     31 |  3614 | `	}` |
|     32 |  3615 | `}` |
|   1436 |  3616 | `static int ReflectExtDumpStep(const char *zName, int nName, void *pData)` |
|      2 |  3617 | `{` |
|   1438 |  3618 | `	ReflectExtDump *p = (ReflectExtDump *)pData;` |
|   1438 |  3619 | `	ph7_context *pCtx = p->pCtx;` |
|   1438 |  3620 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  3621 | `	SyBlob sBlock;` |
|   1438 |  3622 | `	if( !PH7_VmInternalNameExists(pVm, p->iKind, zName, nName) ){` |
|    ! 0 |  3623 | `		return 0;` |
|      - |  3624 | `	}` |
|   1438 |  3625 | `	p->nRow++;` |
|   1438 |  3626 | `	if( p->iKind == PH7_EXT_KIND_INI ){` |
|      - |  3627 | `		SyBlob sVal, sDef;` |
|     63 |  3628 | `		sxi32 iAccess = 0;` |
|     63 |  3629 | `		SyBlobInit(&sVal, &pVm->sAllocator);` |
|     63 |  3630 | `		SyBlobInit(&sDef, &pVm->sAllocator);` |
|     63 |  3631 | `		if( PH7_VmIniDescribe(pVm, zName, (sxu32)nName, &iAccess, &sVal, &sDef) ){` |
|     63 |  3632 | `			SyBlobAppend(p->pOut, "    Entry [ ", sizeof("    Entry [ ")-1);` |
|     63 |  3633 | `			SyBlobAppend(p->pOut, zName, (sxu32)nName);` |
|     63 |  3634 | `			SyBlobAppend(p->pOut, " <", sizeof(" <")-1);` |
|     63 |  3635 | `			ReflectExtIniAccess(p->pOut, iAccess);` |
|     63 |  3636 | `			SyBlobAppend(p->pOut, "> ]\n      Current = '", sizeof("> ]\n      Current = '")-1);` |
|     63 |  3637 | `			SyBlobAppend(p->pOut, SyBlobData(&sVal), SyBlobLength(&sVal));` |
|     63 |  3638 | `			SyBlobAppend(p->pOut, "'\n", sizeof("'\n")-1);` |
|      - |  3639 | `			/* php prints the DEFAULT only for a directive a script moved. */` |
|     62 |  3640 | `			if( SyBlobLength(&sVal) != SyBlobLength(&sDef)` |
|     63 |  3641 | `			 \|\| SyMemcmp(SyBlobData(&sVal), SyBlobData(&sDef), SyBlobLength(&sVal)) != 0 ){` |
|      3 |  3642 | `				SyBlobAppend(p->pOut, "      Default = '", sizeof("      Default = '")-1);` |
|      3 |  3643 | `				SyBlobAppend(p->pOut, SyBlobData(&sDef), SyBlobLength(&sDef));` |
|      3 |  3644 | `				SyBlobAppend(p->pOut, "'\n", sizeof("'\n")-1);` |
|      1 |  3645 | `			}` |
|     63 |  3646 | `			SyBlobAppend(p->pOut, "    }\n", sizeof("    }\n")-1);` |
|     31 |  3647 | `		}` |
|     63 |  3648 | `		SyBlobRelease(&sVal);` |
|     63 |  3649 | `		SyBlobRelease(&sDef);` |
|     63 |  3650 | `		return 0;` |
|      - |  3651 | `	}` |
|   1376 |  3652 | `	SyBlobInit(&sBlock, &pVm->sAllocator);` |
|   1376 |  3653 | `	if( p->iKind == PH7_EXT_KIND_CONST ){` |
|    873 |  3654 | `		ph7_constant *pCons = ReflectConstEntry(pVm, zName, nName);` |
|    873 |  3655 | `		if( pCons ){` |
|    873 |  3656 | `			pVm->bConstEnum++;   /* describing the table is not READING an entry */` |
|    873 |  3657 | `			ReflectExportGlobalConstLine(pCtx, &sBlock, pCons);` |
|    873 |  3658 | `			pVm->bConstEnum--;` |
|    437 |  3659 | `		}` |
|    940 |  3660 | `	}else if( p->iKind == PH7_EXT_KIND_CLASS ){` |
|    253 |  3661 | `		ph7_class *pClass = PH7_VmExtractClass(pVm, zName, (sxu32)nName, FALSE, 0);` |
|      - |  3662 | `		/* php separates one CLASS block from the next with a blank line, and` |
|      - |  3663 | `		 * does NOT do the same for functions or constants. */` |
|    253 |  3664 | `		if( p->nRow > 1 ){` |
|    239 |  3665 | `			SyBlobAppend(p->pOut, "\n", sizeof(char));` |
|    119 |  3666 | `		}` |
|    253 |  3667 | `		if( pClass ){` |
|    253 |  3668 | `			ReflectExportClassBlock(pCtx, &sBlock, pClass);` |
|    126 |  3669 | `		}` |
|    127 |  3670 | `	}else{` |
|    252 |  3671 | `		ReflectExportFuncByName(pCtx, &sBlock, zName, nName);` |
|      - |  3672 | `	}` |
|   1376 |  3673 | `	ReflectExportPad(p->pOut, &sBlock, 4);` |
|   1376 |  3674 | `	SyBlobRelease(&sBlock);` |
|   1376 |  3675 | `	return 0;` |
|    720 |  3676 | `}` |
|      - |  3677 | ``/* One `  - <title>[ [N]] { … }` section, written only when it has rows. */`` |
|    100 |  3678 | `static void ReflectExtSection(SyBlob *pOut, const char *zTitle, SyBlob *pBody,` |
|      - |  3679 | `	sxu32 nRow, int bCount)` |
|      2 |  3680 | `{` |
|    102 |  3681 | `	if( SyBlobLength(pBody) < 1 ){` |
|     56 |  3682 | `		return;` |
|      - |  3683 | `	}` |
|     48 |  3684 | `	SyBlobFormat(pOut, "\n  - %s ", zTitle);` |
|     48 |  3685 | `	if( bCount ){` |
|     25 |  3686 | `		SyBlobFormat(pOut, "[%u] ", nRow);` |
|     12 |  3687 | `	}` |
|     48 |  3688 | `	SyBlobAppend(pOut, "{\n", sizeof("{\n")-1);` |
|     48 |  3689 | `	SyBlobAppend(pOut, SyBlobData(pBody), SyBlobLength(pBody));` |
|     48 |  3690 | `	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|     52 |  3691 | `}` |
|      2 |  3692 | `static int ReflectExtDepLine(const char *zOn, const char *zKind, void *pData)` |
|      1 |  3693 | `{` |
|      3 |  3694 | `	ReflectExtDump *p = (ReflectExtDump *)pData;` |
|      3 |  3695 | `	p->nRow++;` |
|      3 |  3696 | `	SyBlobFormat(p->pOut, "    Dependency [ %s (%s) ]\n", zOn, zKind);` |
|      3 |  3697 | `	return 0;` |
|      1 |  3698 | `}` |
|     80 |  3699 | `static void ReflectExtOneSection(ph7_context *pCtx, SyBlob *pOut, int iExt,` |
|      - |  3700 | `	int iKind, const char *zTitle, int bCount)` |
|      2 |  3701 | `{` |
|      - |  3702 | `	ReflectExtDump sDump;` |
|      - |  3703 | `	SyBlob sBody;` |
|     82 |  3704 | `	SyBlobInit(&sBody, &pCtx->pVm->sAllocator);` |
|     82 |  3705 | `	sDump.pCtx = pCtx;` |
|     82 |  3706 | `	sDump.pOut = &sBody;` |
|     82 |  3707 | `	sDump.iKind = iKind;` |
|     82 |  3708 | `	sDump.nRow = 0;` |
|     82 |  3709 | `	PH7_VmExtWalk(iExt, iKind, ReflectExtDumpStep, &sDump);` |
|     82 |  3710 | `	ReflectExtSection(pOut, zTitle, &sBody, sDump.nRow, bCount);` |
|     82 |  3711 | `	SyBlobRelease(&sBody);` |
|     82 |  3712 | `}` |
|     20 |  3713 | `static int vm_builtin_ReflectionExtension_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  3714 | `{` |
|     22 |  3715 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     22 |  3716 | `	ph7_vm *pVm = pCtx->pVm;` |
|     22 |  3717 | `	const char *zName = "";` |
|     22 |  3718 | `	int nName = 0, iExt;` |
|      - |  3719 | `	SyBlob sOut;` |
|     10 |  3720 | `	SXUNUSED(nArg);` |
|     10 |  3721 | `	SXUNUSED(apArg);` |
|     22 |  3722 | `	if( pThis ){` |
|     22 |  3723 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     10 |  3724 | `	}` |
|     22 |  3725 | `	iExt = PH7_VmExtensionLookup(zName, nName);` |
|     22 |  3726 | `	SyBlobInit(&sOut, &pVm->sAllocator);` |
|      - |  3727 | `` 	/* php's `#N` is the module's REGISTRATION index; a `phl.stub_extensions` `` |
|      - |  3728 | `	 * name has no partition of its own and rides at the end of the table. */` |
|      - |  3729 | `	/* The version is the one getVersion() reports, which is php's answer for a` |
|      - |  3730 | `	 * BUNDLED module: the interpreter's own, not the extension's. */` |
|     22 |  3731 | `	SyBlobFormat(&sOut, "Extension [ <persistent> extension #%d %.*s version %s ] {\n",` |
|     10 |  3732 | `		iExt >= 0 ? iExt : PH7_VmExtensionCount(), nName, zName, PHP_COMPAT_VERSION);` |
|     22 |  3733 | `	if( iExt >= 0 ){` |
|      - |  3734 | `		ReflectExtDump sDep;` |
|      - |  3735 | `		SyBlob sBody;` |
|     22 |  3736 | `		SyBlobInit(&sBody, &pVm->sAllocator);` |
|     22 |  3737 | `		sDep.pCtx = pCtx;` |
|     22 |  3738 | `		sDep.pOut = &sBody;` |
|     22 |  3739 | `		sDep.iKind = -1;` |
|     22 |  3740 | `		sDep.nRow = 0;` |
|     22 |  3741 | `		PH7_VmExtWalkDep(iExt, ReflectExtDepLine, &sDep);` |
|     22 |  3742 | `		ReflectExtSection(&sOut, "Dependencies", &sBody, sDep.nRow, 0);` |
|     22 |  3743 | `		SyBlobRelease(&sBody);` |
|     22 |  3744 | `		ReflectExtOneSection(pCtx, &sOut, iExt, PH7_EXT_KIND_INI,   "INI",       0);` |
|     22 |  3745 | `		ReflectExtOneSection(pCtx, &sOut, iExt, PH7_EXT_KIND_CONST, "Constants", 1);` |
|     22 |  3746 | `		ReflectExtOneSection(pCtx, &sOut, iExt, PH7_EXT_KIND_FUNC,  "Functions", 0);` |
|     22 |  3747 | `		ReflectExtOneSection(pCtx, &sOut, iExt, PH7_EXT_KIND_CLASS, "Classes",   1);` |
|     10 |  3748 | `	}` |
|     22 |  3749 | `	SyBlobAppend(&sOut, "}\n", sizeof("}\n")-1);` |
|     22 |  3750 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     22 |  3751 | `	SyBlobRelease(&sOut);` |
|     22 |  3752 | `	return PH7_OK;` |
|      2 |  3753 | `}` |
|      - |  3754 | `/* ---- ReflectionZendExtension: none exist, so the constructor always refuses ---- */` |
|      6 |  3755 | `static int vm_builtin_ReflectionZendExtension_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  3756 | `{` |
|      8 |  3757 | `	int nName = 0;` |
|      8 |  3758 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|     11 |  3759 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 |  3760 | `		"Zend Extension \"%.*s\" does not exist", nName, zName);` |
|      2 |  3761 | `}` |
|    ! 0 |  3762 | `static int vm_builtin_ReflectionZendExtension_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3763 | `{` |
|    ! 0 |  3764 | `	SXUNUSED(nArg);` |
|    ! 0 |  3765 | `	SXUNUSED(apArg);` |
|    ! 0 |  3766 | `	ph7_result_string(pCtx, "", 0);` |
|    ! 0 |  3767 | `	return PH7_OK;` |
|    ! 0 |  3768 | `}` |
|      - |  3769 | `/* ---- ReflectionReference ---- */` |
|      - |  3770 | `/*` |
|      - |  3771 | ` * ReflectionReference::fromArrayElement(array $array, string\|int $key)` |
|      - |  3772 | ` *` |
|      - |  3773 | ` * Answers a reflector only when the element IS a reference — its slot carries a` |
|      - |  3774 | ` * reference-table record with at least two links. The instance is built without` |
|      - |  3775 | ` * running the (private) constructor, exactly as php's factory does.` |
|      - |  3776 | ` */` |
|     18 |  3777 | `static int vm_builtin_ReflectionReference_fromArrayElement(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3778 | `{` |
|      - |  3779 | `	char zGiven[64];` |
|     19 |  3780 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  3781 | `	ph7_hashmap *pMap;` |
|     19 |  3782 | `	ph7_hashmap_node *pNode = 0;` |
|      - |  3783 | `	ph7_class *pClass;` |
|      - |  3784 | `	ph7_class_instance *pObj;` |
|      - |  3785 | `	char zId[64];` |
|     19 |  3786 | `	if( nArg < 1 ){` |
|    ! 0 |  3787 | `		ph7_result_null(pCtx);` |
|    ! 0 |  3788 | `		return PH7_OK;` |
|      - |  3789 | `	}` |
|     19 |  3790 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - |  3791 | ``		/* The shared ZPP screen leaves a SCALAR against a bare `array` parameter`` |
|      - |  3792 | `		 * unscreened (it only judges array/object/null/resource pairings and` |
|      - |  3793 | `		 * scalars against class-typed ones), so the refusal php words from the` |
|      - |  3794 | `		 * declared type is written here -- as the prelude's own is_array() check` |
|      - |  3795 | `		 * did. Recorded with the rest of that gap. */` |
|    ! 0 |  3796 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|      - |  3797 | `			"ReflectionReference::fromArrayElement(): Argument #1 ($array) "` |
|    ! 0 |  3798 | `			"must be of type array, %s given", VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|      - |  3799 | `	}` |
|     19 |  3800 | `	if( nArg < 2 ){` |
|    ! 0 |  3801 | `		ph7_result_null(pCtx);` |
|    ! 0 |  3802 | `		return PH7_OK;` |
|      - |  3803 | `	}` |
|     19 |  3804 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     19 |  3805 | `	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK \|\| pNode == 0 ){` |
|    ! 0 |  3806 | `		ph7_result_null(pCtx);` |
|    ! 0 |  3807 | `		return PH7_OK;` |
|      - |  3808 | `	}` |
|     19 |  3809 | `	if( PH7_VmSlotRefCount(pVm, pNode->nValIdx) < 2 ){` |
|      7 |  3810 | `		ph7_result_null(pCtx);` |
|      7 |  3811 | `		return PH7_OK;` |
|      - |  3812 | `	}` |
|     13 |  3813 | `	pClass = PH7_VmExtractClass(pVm, "ReflectionReference", sizeof("ReflectionReference")-1, FALSE, 0);` |
|     13 |  3814 | `	pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|     13 |  3815 | `	if( pObj == 0 ){` |
|    ! 0 |  3816 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  3817 | `	}` |
|      - |  3818 | `	/* php's ids are opaque strings; the slot index is the identity PHL has. */` |
|     13 |  3819 | `	SyBufferFormat(zId, sizeof(zId), "phlref%u", (unsigned)pNode->nValIdx);` |
|     13 |  3820 | `	PH7_NativeSetAttrStr(pVm, pObj, RR_ID, zId, (int)SyStrlen(zId));` |
|     13 |  3821 | `	return ReflectResultObject(pCtx, pObj);` |
|     10 |  3822 | `}` |
|     12 |  3823 | `static int vm_builtin_ReflectionReference_getId(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3824 | `{` |
|     13 |  3825 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     13 |  3826 | `	const char *zId = "";` |
|     13 |  3827 | `	int nId = 0;` |
|      6 |  3828 | `	SXUNUSED(nArg);` |
|      6 |  3829 | `	SXUNUSED(apArg);` |
|     13 |  3830 | `	if( pThis ){` |
|     13 |  3831 | `		PH7_NativeAttrStr(pThis, RR_ID, &zId, &nId);` |
|      6 |  3832 | `	}` |
|     13 |  3833 | `	ph7_result_string(pCtx, zId, nId);` |
|     13 |  3834 | `	return PH7_OK;` |
|      1 |  3835 | `}` |
|      - |  3836 | `/* php declares this private and never calls it; reaching it is the diagnostic. */` |
|    ! 0 |  3837 | `static int vm_builtin_ReflectionReference_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3838 | `{` |
|    ! 0 |  3839 | `	SXUNUSED(nArg);` |
|    ! 0 |  3840 | `	SXUNUSED(apArg);` |
|    ! 0 |  3841 | `	SXUNUSED(pCtx);` |
|    ! 0 |  3842 | `	return PH7_OK;` |
|    ! 0 |  3843 | `}` |
|      - |  3844 | `/*` |
|      - |  3845 | ` * Declare the six. Called from PH7_VmInstallReflection() where chunk 5 used to` |
|      - |  3846 | ` * be compiled, so Reflector (chunk 1) already exists.` |
|      - |  3847 | ` */` |
|   8445 |  3848 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionSmall(ph7_vm *pVm)` |
|      5 |  3849 | `{` |
|      - |  3850 | `	static const PH7_NativePropDef aGenProp[] = {` |
|      - |  3851 | `		{ RG_GEN, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  3852 | `	};` |
|      - |  3853 | `	static const PH7_NativeMethodDef aGenMethod[] = {` |
|      - |  3854 | `		{ "__construct",           PH7_MOD_PUBLIC, "Generator $generator", "",` |
|      - |  3855 | `		  vm_builtin_ReflectionGenerator_construct },` |
|      - |  3856 | `		{ "getFunction",           PH7_MOD_PUBLIC, "", "ReflectionFunctionAbstract",` |
|      - |  3857 | `		  vm_builtin_ReflectionGenerator_getFunction },` |
|      - |  3858 | `		{ "getThis",               PH7_MOD_PUBLIC, "", "?object", vm_builtin_ReflectionGenerator_getThis },` |
|      - |  3859 | `		{ "getExecutingGenerator", PH7_MOD_PUBLIC, "", "Generator",` |
|      - |  3860 | `		  vm_builtin_ReflectionGenerator_getExecuting },` |
|      - |  3861 | `		{ "isClosed",              PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionGenerator_isClosed },` |
|      - |  3862 | `		{ "getExecutingLine",      PH7_MOD_PUBLIC, "", "int", vm_builtin_ReflectionGenerator_execLine },` |
|      - |  3863 | `		{ "getExecutingFile",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionGenerator_execFile },` |
|      - |  3864 | `		{ "getTrace",              PH7_MOD_PUBLIC,` |
|      - |  3865 | `		  "int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT", "array",` |
|      - |  3866 | `		  vm_builtin_ReflectionGenerator_trace },` |
|      - |  3867 | `	};` |
|      - |  3868 | `	static const PH7_NativePropDef aFiberProp[] = {` |
|      - |  3869 | `		{ RF_FIBER, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  3870 | `	};` |
|      - |  3871 | `	static const PH7_NativeMethodDef aFiberMethod[] = {` |
|      - |  3872 | `		{ "__construct",      PH7_MOD_PUBLIC, "Fiber $fiber", "", vm_builtin_ReflectionFiber_construct },` |
|      - |  3873 | `		{ "getFiber",         PH7_MOD_PUBLIC, "", "Fiber",    vm_builtin_ReflectionFiber_getFiber },` |
|      - |  3874 | `		{ "getCallable",      PH7_MOD_PUBLIC, "", "callable", vm_builtin_ReflectionFiber_getCallable },` |
|      - |  3875 | `		{ "getExecutingLine", PH7_MOD_PUBLIC, "", "?int",     vm_builtin_ReflectionFiber_execLine },` |
|      - |  3876 | `		{ "getExecutingFile", PH7_MOD_PUBLIC, "", "?string",  vm_builtin_ReflectionFiber_execFile },` |
|      - |  3877 | `		{ "getTrace",         PH7_MOD_PUBLIC, "int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT",` |
|      - |  3878 | `		  "array", vm_builtin_ReflectionFiber_trace },` |
|      - |  3879 | `	};` |
|      - |  3880 | `	static const PH7_NativePropDef aNameProp[] = {` |
|      - |  3881 | `		{ "name", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  3882 | `	};` |
|      - |  3883 | `	static const PH7_NativeMethodDef aConstMethod[] = {` |
|      - |  3884 | `		{ "__construct",       PH7_MOD_PUBLIC, "string $name", "",` |
|      - |  3885 | `		  vm_builtin_ReflectionConstant_construct },` |
|      - |  3886 | `		{ "getName",           PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_getName },` |
|      - |  3887 | `		{ "getNamespaceName",  PH7_MOD_PUBLIC, "", "string",` |
|      - |  3888 | `		  vm_builtin_ReflectionConstant_getNamespaceName },` |
|      - |  3889 | `		{ "getShortName",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_getShortName },` |
|      - |  3890 | `		{ "getValue",          PH7_MOD_PUBLIC, "", "mixed",  vm_builtin_ReflectionConstant_getValue },` |
|      - |  3891 | `		{ "isDeprecated",      PH7_MOD_PUBLIC, "", "bool",   vm_builtin_ReflectionConstant_isDeprecated },` |
|      - |  3892 | `		{ "getFileName",       PH7_MOD_PUBLIC, "", "string\|false",` |
|      - |  3893 | `		  vm_builtin_ReflectionConstant_getFileName },` |
|      - |  3894 | `		{ "getExtension",      PH7_MOD_PUBLIC, "", "?ReflectionExtension",` |
|      - |  3895 | `		  vm_builtin_ReflectionConstant_getExtension },` |
|      - |  3896 | `		{ "getExtensionName",  PH7_MOD_PUBLIC, "", "string\|false",` |
|      - |  3897 | `		  vm_builtin_ReflectionConstant_getExtensionName },` |
|      - |  3898 | `		{ "getAttributes",     PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - |  3899 | `		  vm_builtin_ReflectionConstant_getAttributes },` |
|      - |  3900 | `		{ "__toString",        PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_toString },` |
|      - |  3901 | `	};` |
|      - |  3902 | `	static const PH7_NativeMethodDef aExtMethod[] = {` |
|      - |  3903 | `		{ "__construct",     PH7_MOD_PUBLIC, "string $name", "", vm_builtin_ReflectionExtension_construct },` |
|      - |  3904 | `		{ "getName",         PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionExtension_getName },` |
|      - |  3905 | `		{ "getVersion",      PH7_MOD_PUBLIC, "", "@?string", vm_builtin_ReflectionExtension_getVersion },` |
|      - |  3906 | `		{ "getFunctions",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getFunctions },` |
|      - |  3907 | `		{ "getConstants",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getConstants },` |
|      - |  3908 | `		{ "getINIEntries",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getINIEntries },` |
|      - |  3909 | `		{ "getClasses",      PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getClasses },` |
|      - |  3910 | `		{ "getClassNames",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getClassNames },` |
|      - |  3911 | `		{ "getDependencies", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getDependencies },` |
|      - |  3912 | `		{ "info",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_ReflectionExtension_info },` |
|      - |  3913 | `		{ "isPersistent",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionExtension_isPersistent },` |
|      - |  3914 | `		{ "isTemporary",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionExtension_isTemporary },` |
|      - |  3915 | `		{ "__toString",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionExtension_toString },` |
|      - |  3916 | `	};` |
|      - |  3917 | `	static const PH7_NativeMethodDef aZendMethod[] = {` |
|      - |  3918 | `		{ "__construct", PH7_MOD_PUBLIC, "string $name", "",` |
|      - |  3919 | `		  vm_builtin_ReflectionZendExtension_construct },` |
|      - |  3920 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionExtension_getName },` |
|      - |  3921 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionZendExtension_toString },` |
|      - |  3922 | `	};` |
|      - |  3923 | `	static const PH7_NativePropDef aRefProp[] = {` |
|      - |  3924 | `		{ RR_ID, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - |  3925 | `	};` |
|      - |  3926 | `	static const PH7_NativeMethodDef aRefMethod[] = {` |
|      - |  3927 | ``		/* php declares the constructor PRIVATE, so `new ReflectionReference` is`` |
|      - |  3928 | `		 * the engine's own "Call to private ... from global scope" Error rather` |
|      - |  3929 | `		 * than a throw the prelude had to write by hand. */` |
|      - |  3930 | `		{ "__construct",      PH7_MOD_PRIVATE, "", "", vm_builtin_ReflectionReference_construct },` |
|      - |  3931 | `		{ "fromArrayElement", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array, string\|int $key",` |
|      - |  3932 | `		  "?ReflectionReference", vm_builtin_ReflectionReference_fromArrayElement },` |
|      - |  3933 | `		{ "getId",            PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionReference_getId },` |
|      - |  3934 | `	};` |
|      - |  3935 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  3936 | `		{ "ReflectionGenerator", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3937 | `		  aGenMethod, SX_ARRAYSIZE(aGenMethod), 0, 0, aGenProp, SX_ARRAYSIZE(aGenProp), 0, 0, 0 },` |
|      - |  3938 | `		{ "ReflectionFiber", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3939 | `		  aFiberMethod, SX_ARRAYSIZE(aFiberMethod), 0, 0, aFiberProp, SX_ARRAYSIZE(aFiberProp), 0, 0, 0 },` |
|      - |  3940 | `		{ "ReflectionConstant", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3941 | `		  aConstMethod, SX_ARRAYSIZE(aConstMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|      - |  3942 | `		{ "ReflectionExtension", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3943 | `		  aExtMethod, SX_ARRAYSIZE(aExtMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|      - |  3944 | `		{ "ReflectionZendExtension", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3945 | `		  aZendMethod, SX_ARRAYSIZE(aZendMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|      - |  3946 | `		{ "ReflectionReference", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3947 | `		  aRefMethod, SX_ARRAYSIZE(aRefMethod), 0, 0, aRefProp, SX_ARRAYSIZE(aRefProp), 0, 0, 0 },` |
|      - |  3948 | `	};` |
|   8450 |  3949 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  3950 | `}` |
|      - |  3951 | `/*` |
|      - |  3952 | ` * ---------------------------------------------------------------------------` |
|      - |  3953 | ` * Reflector, Reflection, ReflectionException, ReflectionClass, ReflectionObject.` |
|      - |  3954 | ` *` |
|      - |  3955 | ` * Chunk 1 — the core of the whole API. Its ~350 lines of PHP funnelled every` |
|      - |  3956 | ` * accessor through __phl_rcinfo(), a memoized descriptor ARRAY built by a C` |
|      - |  3957 | ` * thunk: sixty methods that each rebuilt or re-read a marshalled copy of state` |
|      - |  3958 | ` * the engine was already holding. A native method reads ph7_class directly, so` |
|      - |  3959 | ` * the descriptor is gone from this path entirely and the memo it needed with it.` |
|      - |  3960 | ` *` |
|      - |  3961 | ` * What stays behind is the prelude that has not moved yet, and these classes` |
|      - |  3962 | ` * still reach it by name where php's own object graph does: getMethod() builds` |
|      - |  3963 | ` * a ReflectionMethod, getProperty() a ReflectionProperty, getAttributes() calls` |
|      - |  3964 | ` * __reflect_build_attrs, __toString() calls __reflect_export_class. Those become` |
|      - |  3965 | ` * direct C the moment chunks 2, 3, 7 and 9 land.` |
|      - |  3966 | ` * ---------------------------------------------------------------------------` |
|      - |  3967 | ` */` |
|      - |  3968 | `#define RC_OBJ "__obj"` |
|      - |  3969 |  |
|      - |  3970 | ``/* The class a ReflectionClass reflects: its `name` slot, resolved. The`` |
|      - |  3971 | ` * constructor already stored the canonical name, so no autoload can be needed` |
|      - |  3972 | ` * here. */` |
|   2109 |  3973 | `static ph7_class * ReflectClassOf(ph7_context *pCtx)` |
|      5 |  3974 | `{` |
|   2114 |  3975 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  3976 | `	const char *zName;` |
|      - |  3977 | `	int nName;` |
|   2114 |  3978 | `	if( pThis == 0 ){` |
|    ! 0 |  3979 | `		return 0;` |
|      - |  3980 | `	}` |
|   2114 |  3981 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|   2114 |  3982 | `	if( nName < 1 ){` |
|    ! 0 |  3983 | `		return 0;` |
|      - |  3984 | `	}` |
|      - |  3985 | `	/* PH7_NativeAttrStr borrows bytes that are NOT NUL-terminated: the length` |
|      - |  3986 | `	 * has to travel with them. */` |
|   2114 |  3987 | `	return PH7_VmExtractClass(pCtx->pVm, zName, (sxu32)nName, FALSE, 0);` |
|   1049 |  3988 | `}` |
|      - |  3989 | `/* The instance a ReflectionObject was built over, or NULL for a plain` |
|      - |  3990 | ` * ReflectionClass — what makes DYNAMIC properties visible. */` |
|    784 |  3991 | `static ph7_class_instance * ReflectClassObj(ph7_context *pCtx)` |
|      5 |  3992 | `{` |
|    789 |  3993 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    789 |  3994 | `	return pThis ? PH7_NativeAttrObj(pThis, RC_OBJ) : 0;` |
|      5 |  3995 | `}` |
|      - |  3996 | `/* $this->name as bytes. */` |
|   1878 |  3997 | `static void ReflectClassName(ph7_context *pCtx, const char **pzOut, int *pnOut)` |
|      5 |  3998 | `{` |
|   1883 |  3999 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   1883 |  4000 | `	*pzOut = "";` |
|   1883 |  4001 | `	*pnOut = 0;` |
|   1883 |  4002 | `	if( pThis ){` |
|   1883 |  4003 | `		PH7_NativeAttrStr(pThis, "name", pzOut, pnOut);` |
|    937 |  4004 | `	}` |
|   1883 |  4005 | `}` |
|      - |  4006 | `/* Answer the truth of one of the reflected class's iFlags bits. */` |
|    232 |  4007 | `static int ReflectClassFlag(ph7_context *pCtx, sxi32 iFlag)` |
|      5 |  4008 | `{` |
|    237 |  4009 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|    237 |  4010 | `	return pClass != 0 && (pClass->iFlags & iFlag) != 0;` |
|      5 |  4011 | `}` |
|      - |  4012 | `#define REFLECT_CLASS_FLAG(NAME,FLAG,NEGATE) \` |
|      - |  4013 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  4014 | `	{ \` |
|      - |  4015 | `		SXUNUSED(nArg); \` |
|      - |  4016 | `		SXUNUSED(apArg); \` |
|      - |  4017 | `		ph7_result_bool(pCtx, NEGATE ? !ReflectClassFlag(pCtx,FLAG) : ReflectClassFlag(pCtx,FLAG)); \` |
|      - |  4018 | `		return PH7_OK; \` |
|      - |  4019 | `	}` |
|     35 |  4020 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isInternal,    PH7_CLASS_INTERNAL,  0)` |
|      5 |  4021 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isUserDefined, PH7_CLASS_INTERNAL,  1)` |
|     15 |  4022 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isInterface,   PH7_CLASS_INTERFACE, 0)` |
|      5 |  4023 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isTrait,       PH7_CLASS_TRAIT,     0)` |
|      - |  4024 | `/*` |
|      - |  4025 | ` * zend's IMPLICIT abstract bit, which this engine did not carry: an INTERFACE` |
|      - |  4026 | `` * that declares a method is abstract, so `(new ReflectionClass('Countable'))`` |
|      - |  4027 | `` * ->isAbstract()` is true where this said false for 24 of php's 25 interfaces`` |
|      - |  4028 | ` * and for every userland one besides. The bit follows the METHODS rather than` |
|      - |  4029 | `` * the keyword, which is why `Traversable` -- an interface with nothing in it --`` |
|      - |  4030 | ` * is php's one negative answer. Only this predicate reads it: getModifiers()` |
|      - |  4031 | `` * is 0 for an interface in php too, and the export writes `interface X`, never`` |
|      - |  4032 | `` * `abstract interface X`.`` |
|      - |  4033 | ` */` |
|     94 |  4034 | `static int vm_builtin_ReflectionClass_isAbstract(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4035 | `{` |
|     96 |  4036 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     96 |  4037 | `	int bAbstract = 0;` |
|     47 |  4038 | `	SXUNUSED(nArg);` |
|     47 |  4039 | `	SXUNUSED(apArg);` |
|     96 |  4040 | `	if( pClass ){` |
|    138 |  4041 | `		bAbstract = (pClass->iFlags & PH7_CLASS_ABSTRACT) != 0` |
|    122 |  4042 | `			\|\| ((pClass->iFlags & PH7_CLASS_INTERFACE) != 0` |
|     56 |  4043 | `			 && SyHashTotalEntry(&pClass->hMethod) > 0);` |
|     47 |  4044 | `	}` |
|     96 |  4045 | `	ph7_result_bool(pCtx, bAbstract);` |
|     96 |  4046 | `	return PH7_OK;` |
|      2 |  4047 | `}` |
|    161 |  4048 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isFinal,       PH7_CLASS_FINAL,     0)` |
|     11 |  4049 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isReadOnly,    PH7_CLASS_READONLY,  0)` |
|     13 |  4050 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isEnum,        PH7_CLASS_ENUM,      0)` |
|      - |  4051 |  |
|      - |  4052 | ``/* Build a ReflectionClass over pTarget with its `name` already filled in: the`` |
|      - |  4053 | ` * constructor would only re-resolve a class this code is holding. */` |
|   1584 |  4054 | `static int ReflectResultClassOf(ph7_context *pCtx, ph7_class *pTarget)` |
|      5 |  4055 | `{` |
|   1589 |  4056 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  4057 | `	ph7_class *pRC;` |
|      - |  4058 | `	ph7_class_instance *pObj;` |
|   1589 |  4059 | `	if( pTarget == 0 ){` |
|    ! 0 |  4060 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4061 | `		return PH7_OK;` |
|      - |  4062 | `	}` |
|   1589 |  4063 | `	pRC = PH7_VmExtractClass(pVm, "ReflectionClass", sizeof("ReflectionClass")-1, FALSE, 0);` |
|   1589 |  4064 | `	if( pRC == 0 \|\| (pObj = PH7_NewClassInstance(pVm, pRC)) == 0 ){` |
|    ! 0 |  4065 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4066 | `		return PH7_OK;` |
|      - |  4067 | `	}` |
|   2377 |  4068 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(&pTarget->sName),` |
|   1584 |  4069 | `		(int)SyStringLength(&pTarget->sName));` |
|   1589 |  4070 | `	PH7_NativeResultObject(pCtx, pObj);` |
|   1589 |  4071 | `	return PH7_OK;` |
|    793 |  4072 | `}` |
|      - |  4073 | ``/* The class an argument declared `ReflectionClass\|string` denotes: a reflector's`` |
|      - |  4074 | `` * `name` slot, or the string itself. NULL when it names nothing (after`` |
|      - |  4075 | ` * autoload). */` |
|     42 |  4076 | `static ph7_class * ReflectClassArg(ph7_context *pCtx, ph7_value *pArg)` |
|      2 |  4077 | `{` |
|     44 |  4078 | `	ph7_vm *pVm = pCtx->pVm;` |
|     44 |  4079 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|    ! 0 |  4080 | `		ph7_class_instance *pObj = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 |  4081 | `		ph7_class *pRC = PH7_VmExtractClass(pVm, "ReflectionClass", sizeof("ReflectionClass")-1, FALSE, 0);` |
|    ! 0 |  4082 | `		if( pRC && PH7_VmInstanceOf(pObj->pClass, pRC) ){` |
|      - |  4083 | `			const char *zName;` |
|      - |  4084 | `			int nName;` |
|    ! 0 |  4085 | `			PH7_NativeAttrStr(pObj, "name", &zName, &nName);` |
|    ! 0 |  4086 | `			return nName > 0 ? PH7_VmExtractClass(pVm, zName, (sxu32)nName, FALSE, 0) : 0;` |
|      - |  4087 | `		}` |
|    ! 0 |  4088 | `	}` |
|     44 |  4089 | `	return ReflectResolveClass(pVm, pArg);` |
|     17 |  4090 | `}` |
|      - |  4091 | `/* ---- constructors ---- */` |
|      - |  4092 | `/* ReflectionClass::__construct(object\|string $objectOrClass) */` |
|   1225 |  4093 | `static int vm_builtin_ReflectionClass_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  4094 | `{` |
|   1230 |  4095 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  4096 | `	ph7_class *pClass;` |
|      - |  4097 | `	sxi32 rc;` |
|   1230 |  4098 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 |  4099 | `		return PH7_OK;` |
|      - |  4100 | `	}` |
|   1230 |  4101 | `	if( ReflectResolveClassRaised(pCtx->pVm, apArg[0], &pClass, &rc) ){` |
|      5 |  4102 | `		return rc;` |
|      - |  4103 | `	}` |
|   1226 |  4104 | `	if( pClass == 0 ){` |
|      - |  4105 | `		/* php reports the NAME it was handed, after the declared object\|string` |
|      - |  4106 | ``		 * has coerced a scalar — `new ReflectionClass(1.5)` says Class "1.5". */`` |
|      - |  4107 | `		const char *zName;` |
|      - |  4108 | `		int nName;` |
|     17 |  4109 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|     24 |  4110 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      7 |  4111 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      - |  4112 | `	}` |
|   1813 |  4113 | `	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", SyStringData(&pClass->sName),` |
|   1207 |  4114 | `		(int)SyStringLength(&pClass->sName));` |
|   1212 |  4115 | `	return PH7_OK;` |
|    615 |  4116 | `}` |
|      - |  4117 | `/* ReflectionObject::__construct(object $object) — the same, plus the receiver` |
|      - |  4118 | ` * whose DYNAMIC properties the inherited accessors then report. */` |
|     24 |  4119 | `static int vm_builtin_ReflectionObject_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4120 | `{` |
|     26 |  4121 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  4122 | `	sxi32 rc;` |
|     26 |  4123 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 |  4124 | `		return PH7_OK;` |
|      - |  4125 | `	}` |
|     26 |  4126 | `	rc = vm_builtin_ReflectionClass_construct(pCtx, nArg, apArg);` |
|     26 |  4127 | `	if( rc != PH7_OK ){` |
|    ! 0 |  4128 | `		return rc;` |
|      - |  4129 | `	}` |
|     26 |  4130 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RC_OBJ, (ph7_class_instance *)apArg[0]->x.pOther);` |
|     26 |  4131 | `	return PH7_OK;` |
|     14 |  4132 | `}` |
|      - |  4133 | `/* ReflectionClass::__clone(): void — php declares it private, and the class is` |
|      - |  4134 | ` * uncloneable besides (PH7_CLASS_NOCLONE answers before any body runs). */` |
|    ! 0 |  4135 | `static int vm_builtin_ReflectionClass_clone(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  4136 | `{` |
|    ! 0 |  4137 | `	SXUNUSED(pCtx);` |
|    ! 0 |  4138 | `	SXUNUSED(nArg);` |
|    ! 0 |  4139 | `	SXUNUSED(apArg);` |
|    ! 0 |  4140 | `	return PH7_OK;` |
|    ! 0 |  4141 | `}` |
|      - |  4142 | `/* ---- name ---- */` |
|   1374 |  4143 | `static int vm_builtin_ReflectionClass_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  4144 | `{` |
|      - |  4145 | `	const char *zName;` |
|      - |  4146 | `	int nName;` |
|    685 |  4147 | `	SXUNUSED(nArg);` |
|    685 |  4148 | `	SXUNUSED(apArg);` |
|   1378 |  4149 | `	ReflectClassName(pCtx, &zName, &nName);` |
|   1378 |  4150 | `	ph7_result_string(pCtx, zName, nName);` |
|   1378 |  4151 | `	return PH7_OK;` |
|      4 |  4152 | `}` |
|      - |  4153 | `/* Offset just past the last namespace separator, or -1 when there is none. */` |
|      6 |  4154 | `static int ReflectNsCut(const char *zName, int nName)` |
|      1 |  4155 | `{` |
|      - |  4156 | `	int i;` |
|     91 |  4157 | `	for( i = nName - 1 ; i >= 0 ; i-- ){` |
|     85 |  4158 | `		if( zName[i] == '\\' ){` |
|    ! 0 |  4159 | `			return i;` |
|      - |  4160 | `		}` |
|     43 |  4161 | `	}` |
|      7 |  4162 | `	return -1;` |
|      4 |  4163 | `}` |
|      2 |  4164 | `static int vm_builtin_ReflectionClass_getShortName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4165 | `{` |
|      - |  4166 | `	const char *zName;` |
|      - |  4167 | `	int nName, iCut;` |
|      1 |  4168 | `	SXUNUSED(nArg);` |
|      1 |  4169 | `	SXUNUSED(apArg);` |
|      3 |  4170 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 |  4171 | `	iCut = ReflectNsCut(zName, nName);` |
|      3 |  4172 | `	if( iCut < 0 ){` |
|      3 |  4173 | `		ph7_result_string(pCtx, zName, nName);` |
|      2 |  4174 | `	}else{` |
|    ! 0 |  4175 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|      - |  4176 | `	}` |
|      3 |  4177 | `	return PH7_OK;` |
|      1 |  4178 | `}` |
|      2 |  4179 | `static int vm_builtin_ReflectionClass_getNamespaceName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4180 | `{` |
|      - |  4181 | `	const char *zName;` |
|      - |  4182 | `	int nName, iCut;` |
|      1 |  4183 | `	SXUNUSED(nArg);` |
|      1 |  4184 | `	SXUNUSED(apArg);` |
|      3 |  4185 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 |  4186 | `	iCut = ReflectNsCut(zName, nName);` |
|      3 |  4187 | `	ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|      3 |  4188 | `	return PH7_OK;` |
|      1 |  4189 | `}` |
|      2 |  4190 | `static int vm_builtin_ReflectionClass_inNamespace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4191 | `{` |
|      - |  4192 | `	const char *zName;` |
|      - |  4193 | `	int nName;` |
|      1 |  4194 | `	SXUNUSED(nArg);` |
|      1 |  4195 | `	SXUNUSED(apArg);` |
|      3 |  4196 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 |  4197 | `	ph7_result_bool(pCtx, ReflectNsCut(zName, nName) >= 0);` |
|      3 |  4198 | `	return PH7_OK;` |
|      1 |  4199 | `}` |
|     10 |  4200 | `static int vm_builtin_ReflectionClass_isAnonymous(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  4201 | `{` |
|      - |  4202 | `	/* An anonymous class is the only one whose name has two halves: php synthesizes` |
|      - |  4203 | ``	 * `<prefix>@anonymous` + NUL + `file:line$hex`, and PH7_NewRawClass cuts sDisp at`` |
|      - |  4204 | `	 * the NUL, so a shorter display name IS the marker. The old test read the name` |
|      - |  4205 | ``	 * for a literal `class@anonymous` prefix, which php only ever writes when the`` |
|      - |  4206 | `	 * class has no parent and no interface. */` |
|     13 |  4207 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      5 |  4208 | `	SXUNUSED(nArg);` |
|      5 |  4209 | `	SXUNUSED(apArg);` |
|     13 |  4210 | `	ph7_result_bool(pCtx, pClass != 0 && pClass->sDisp.nByte != pClass->sName.nByte);` |
|     13 |  4211 | `	return PH7_OK;` |
|      3 |  4212 | `}` |
|      - |  4213 | `/* ---- shape ---- */` |
|     50 |  4214 | `static int vm_builtin_ReflectionClass_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4215 | `{` |
|     52 |  4216 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     52 |  4217 | `	sxi64 iMods = 0;` |
|     25 |  4218 | `	SXUNUSED(nArg);` |
|     25 |  4219 | `	SXUNUSED(apArg);` |
|     52 |  4220 | `	if( pClass ){` |
|     52 |  4221 | `		if( pClass->iFlags & PH7_CLASS_ABSTRACT ){ iMods \|= 64; }` |
|     52 |  4222 | `		if( pClass->iFlags & PH7_CLASS_FINAL ){ iMods \|= 32; }` |
|     52 |  4223 | `		if( pClass->iFlags & PH7_CLASS_READONLY ){ iMods \|= 65536; }` |
|     25 |  4224 | `	}` |
|     52 |  4225 | `	ph7_result_int64(pCtx, iMods);` |
|     52 |  4226 | `	return PH7_OK;` |
|      2 |  4227 | `}` |
|     80 |  4228 | `static int vm_builtin_ReflectionClass_getParentClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  4229 | `{` |
|     83 |  4230 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     36 |  4231 | `	SXUNUSED(nArg);` |
|     36 |  4232 | `	SXUNUSED(apArg);` |
|     83 |  4233 | `	if( pClass == 0 \|\| pClass->pBase == 0 ){` |
|     22 |  4234 | `		ph7_result_bool(pCtx, 0);` |
|     22 |  4235 | `		return PH7_OK;` |
|      - |  4236 | `	}` |
|     63 |  4237 | `	return ReflectResultClassOf(pCtx, pClass->pBase);` |
|     39 |  4238 | `}` |
|      - |  4239 | `/* getInterfaceNames()/getInterfaces()/getTraitNames()/getTraits() — one walk,` |
|      - |  4240 | ` * four shapes. bReflector picks name-list vs {name: ReflectionClass}. */` |
|    164 |  4241 | `static int ReflectClassNameList(ph7_context *pCtx, int bTraits, int bReflector)` |
|      4 |  4242 | `{` |
|    168 |  4243 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|    168 |  4244 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|      - |  4245 | `	SySet aSet;` |
|      - |  4246 | `	ph7_class **apOut;` |
|      - |  4247 | `	sxu32 n, nOut;` |
|    168 |  4248 | `	if( pList == 0 ){` |
|    ! 0 |  4249 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  4250 | `	}` |
|    168 |  4251 | `	if( pClass == 0 ){` |
|    ! 0 |  4252 | `		ph7_result_value(pCtx, pList);` |
|    ! 0 |  4253 | `		return PH7_OK;` |
|      - |  4254 | `	}` |
|    168 |  4255 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|    168 |  4256 | `	if( bTraits ){` |
|      3 |  4257 | `		apOut = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      3 |  4258 | `		nOut = SySetUsed(&pClass->aTrait);` |
|      2 |  4259 | `	}else{` |
|    166 |  4260 | `		PH7_ReflectInterfacesOf(pCtx->pVm, pClass, &aSet);` |
|    166 |  4261 | `		apOut = (ph7_class **)SySetBasePtr(&aSet);` |
|    166 |  4262 | `		nOut = SySetUsed(&aSet);` |
|      - |  4263 | `	}` |
|    466 |  4264 | `	for( n = 0 ; n < nOut ; n++ ){` |
|    302 |  4265 | `		SyString *pName = &apOut[n]->sName;` |
|    302 |  4266 | `		if( bReflector ){` |
|      - |  4267 | `			/* {name: ReflectionClass} — the reflector is built here rather than` |
|      - |  4268 | `			 * through ReflectResultClassOf, which writes the RESULT slot. */` |
|      5 |  4269 | `			ph7_class *pRC = PH7_VmExtractClass(pCtx->pVm, "ReflectionClass",` |
|      - |  4270 | `				sizeof("ReflectionClass")-1, FALSE, 0);` |
|      5 |  4271 | `			ph7_class_instance *pObj = pRC ? PH7_NewClassInstance(pCtx->pVm, pRC) : 0;` |
|      5 |  4272 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|      - |  4273 | `			ph7_value sVal;` |
|      5 |  4274 | `			if( pObj == 0 \|\| pKey == 0 ){ break; }` |
|      7 |  4275 | `			PH7_NativeSetAttrStr(pCtx->pVm, pObj, "name", SyStringData(pName),` |
|      4 |  4276 | `				(int)SyStringLength(pName));` |
|      - |  4277 | `			/* A STACK carrier, never a context scalar: releasing a MEMOBJ_OBJ` |
|      - |  4278 | `			 * context value would unref the instance a second time. */` |
|      5 |  4279 | `			PH7_MemObjInit(pCtx->pVm, &sVal);` |
|      5 |  4280 | `			sVal.x.pOther = pObj;` |
|      5 |  4281 | `			sVal.iFlags = MEMOBJ_OBJ;` |
|      5 |  4282 | `			ph7_value_string(pKey, SyStringData(pName), (int)SyStringLength(pName));` |
|      5 |  4283 | `			ph7_array_add_elem(pList, pKey, &sVal);   /* takes its own reference */` |
|      5 |  4284 | `			PH7_ClassInstanceUnref(pObj);` |
|      3 |  4285 | `		}else{` |
|    298 |  4286 | `			ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    298 |  4287 | `			if( pVal == 0 ){ break; }` |
|    298 |  4288 | `			ph7_value_string(pVal, SyStringData(pName), (int)SyStringLength(pName));` |
|    298 |  4289 | `			ph7_array_add_elem(pList, 0, pVal);` |
|      - |  4290 | `		}` |
|    153 |  4291 | `	}` |
|    168 |  4292 | `	SySetRelease(&aSet);` |
|    168 |  4293 | `	ph7_result_value(pCtx, pList);` |
|    168 |  4294 | `	return PH7_OK;` |
|     86 |  4295 | `}` |
|      - |  4296 | `#define REFLECT_CLASS_LIST(NAME,TRAITS,REFLECTOR) \` |
|      - |  4297 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  4298 | `	{ \` |
|      - |  4299 | `		SXUNUSED(nArg); \` |
|      - |  4300 | `		SXUNUSED(apArg); \` |
|      - |  4301 | `		return ReflectClassNameList(pCtx,TRAITS,REFLECTOR); \` |
|      - |  4302 | `	}` |
|    164 |  4303 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getInterfaceNames, 0, 0)` |
|      3 |  4304 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getInterfaces,     0, 1)` |
|      3 |  4305 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getTraitNames,     1, 0)` |
|    ! 0 |  4306 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getTraits,         1, 1)` |
|      - |  4307 |  |
|      - |  4308 | ``/* getTraitAliases(): php reports `use T { m as n; }` renames; PHL's compiler`` |
|      - |  4309 | ` * installs the alias as a method and keeps no rename record, so the map is` |
|      - |  4310 | ` * empty (recorded). */` |
|    ! 0 |  4311 | `static int vm_builtin_ReflectionClass_getTraitAliases(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  4312 | `{` |
|    ! 0 |  4313 | `	SXUNUSED(nArg);` |
|    ! 0 |  4314 | `	SXUNUSED(apArg);` |
|    ! 0 |  4315 | `	ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  4316 | `	return PH7_OK;` |
|    ! 0 |  4317 | `}` |
|      4 |  4318 | `static int vm_builtin_ReflectionClass_isIterable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4319 | `{` |
|      5 |  4320 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4321 | `	SySet aSet;` |
|      - |  4322 | `	ph7_class **apIface;` |
|      - |  4323 | `	sxu32 n;` |
|      5 |  4324 | `	int bIterable = 0;` |
|      2 |  4325 | `	SXUNUSED(nArg);` |
|      2 |  4326 | `	SXUNUSED(apArg);` |
|      4 |  4327 | `	if( pClass == 0` |
|      5 |  4328 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT)) ){` |
|    ! 0 |  4329 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4330 | `		return PH7_OK;` |
|      - |  4331 | `	}` |
|      5 |  4332 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|      5 |  4333 | `	PH7_ReflectInterfacesOf(pCtx->pVm, pClass, &aSet);` |
|      5 |  4334 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|      9 |  4335 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|      9 |  4336 | `		if( pCtx->pVm->pTraversableClass && apIface[n] == pCtx->pVm->pTraversableClass ){` |
|      5 |  4337 | `			bIterable = 1;` |
|      5 |  4338 | `			break;` |
|      - |  4339 | `		}` |
|      3 |  4340 | `	}` |
|      5 |  4341 | `	SySetRelease(&aSet);` |
|      5 |  4342 | `	ph7_result_bool(pCtx, bIterable);` |
|      5 |  4343 | `	return PH7_OK;` |
|      3 |  4344 | `}` |
|     32 |  4345 | `static int vm_builtin_ReflectionClass_implementsInterface(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4346 | `{` |
|      - |  4347 | `	ph7_class_instance *pPrev;` |
|      - |  4348 | `	sxi32 rcLook;` |
|     34 |  4349 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4350 | `	ph7_class *pTarget;` |
|      - |  4351 | `	SySet aSet;` |
|      - |  4352 | `	ph7_class **apIface;` |
|      - |  4353 | `	sxu32 n;` |
|     34 |  4354 | `	int bYes = 0;` |
|     34 |  4355 | `	if( nArg < 1 ){` |
|    ! 0 |  4356 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4357 | `		return PH7_OK;` |
|      - |  4358 | `	}` |
|     34 |  4359 | `	pTarget = ReflectResolveClassFenced(pCtx, apArg[0], ReflectClassArg, &pPrev, &rcLook);` |
|     34 |  4360 | `	if( pTarget == 0 ){` |
|      - |  4361 | `		const char *zName;` |
|      - |  4362 | `		int nName;` |
|      8 |  4363 | `		if( rcLook != PH7_OK ){` |
|    ! 0 |  4364 | `			return rcLook;` |
|      - |  4365 | `		}` |
|      8 |  4366 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|     11 |  4367 | `		rcLook = PH7_VmThrowExceptionPrev(pCtx, pPrev, "ReflectionException",` |
|      3 |  4368 | `			"Interface \"%.*s\" does not exist", nName, zName);` |
|      8 |  4369 | `		if( pPrev ){` |
|      3 |  4370 | `			PH7_ClassInstanceUnref(pPrev);` |
|      1 |  4371 | `		}` |
|      8 |  4372 | `		return rcLook;` |
|      - |  4373 | `	}` |
|     27 |  4374 | `	if( (pTarget->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      4 |  4375 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  4376 | `			"%z is not an interface", &pTarget->sDisp);` |
|      - |  4377 | `	}` |
|     25 |  4378 | `	if( pClass == pTarget ){` |
|      3 |  4379 | `		ph7_result_bool(pCtx, 1);` |
|      3 |  4380 | `		return PH7_OK;` |
|      - |  4381 | `	}` |
|     23 |  4382 | `	if( pClass == 0 ){` |
|    ! 0 |  4383 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4384 | `		return PH7_OK;` |
|      - |  4385 | `	}` |
|     23 |  4386 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|     23 |  4387 | `	PH7_ReflectInterfacesOf(pCtx->pVm, pClass, &aSet);` |
|     23 |  4388 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|     58 |  4389 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|     52 |  4390 | `		if( apIface[n] == pTarget ){` |
|     17 |  4391 | `			bYes = 1;` |
|     17 |  4392 | `			break;` |
|      - |  4393 | `		}` |
|      2 |  4394 | `	}` |
|     23 |  4395 | `	SySetRelease(&aSet);` |
|     23 |  4396 | `	ph7_result_bool(pCtx, bYes);` |
|     23 |  4397 | `	return PH7_OK;` |
|     12 |  4398 | `}` |
|     10 |  4399 | `static int vm_builtin_ReflectionClass_isSubclassOf(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4400 | `{` |
|      - |  4401 | `	ph7_class_instance *pPrev;` |
|      - |  4402 | `	sxi32 rcLook;` |
|     12 |  4403 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4404 | `	ph7_class *pTarget, *pWalk;` |
|      - |  4405 | `	SySet aSet;` |
|      - |  4406 | `	ph7_class **apIface;` |
|      - |  4407 | `	sxu32 n;` |
|     12 |  4408 | `	int iDepth = 0, bYes = 0;` |
|     12 |  4409 | `	if( nArg < 1 ){` |
|    ! 0 |  4410 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4411 | `		return PH7_OK;` |
|      - |  4412 | `	}` |
|     12 |  4413 | `	pTarget = ReflectResolveClassFenced(pCtx, apArg[0], ReflectClassArg, &pPrev, &rcLook);` |
|     12 |  4414 | `	if( pTarget == 0 ){` |
|      - |  4415 | `		const char *zName;` |
|      - |  4416 | `		int nName;` |
|      5 |  4417 | `		if( rcLook != PH7_OK ){` |
|    ! 0 |  4418 | `			return rcLook;` |
|      - |  4419 | `		}` |
|      5 |  4420 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|      7 |  4421 | `		rcLook = PH7_VmThrowExceptionPrev(pCtx, pPrev, "ReflectionException",` |
|      2 |  4422 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      5 |  4423 | `		if( pPrev ){` |
|      3 |  4424 | `			PH7_ClassInstanceUnref(pPrev);` |
|      1 |  4425 | `		}` |
|      5 |  4426 | `		return rcLook;` |
|      - |  4427 | `	}` |
|      - |  4428 | `	/* php: a class is never a subclass of ITSELF */` |
|      7 |  4429 | `	if( pClass == 0 \|\| pClass == pTarget ){` |
|      3 |  4430 | `		ph7_result_bool(pCtx, 0);` |
|      3 |  4431 | `		return PH7_OK;` |
|      - |  4432 | `	}` |
|      7 |  4433 | `	for( pWalk = pClass->pBase ; pWalk && iDepth <= REFLECT_WALK_MAX_DEPTH ; pWalk = pWalk->pBase ){` |
|      5 |  4434 | `		if( pWalk == pTarget ){` |
|      3 |  4435 | `			ph7_result_bool(pCtx, 1);` |
|      3 |  4436 | `			return PH7_OK;` |
|      - |  4437 | `		}` |
|      3 |  4438 | `		iDepth++;` |
|      2 |  4439 | `	}` |
|      3 |  4440 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|      3 |  4441 | `	PH7_ReflectInterfacesOf(pCtx->pVm, pClass, &aSet);` |
|      3 |  4442 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|      3 |  4443 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|      3 |  4444 | `		if( apIface[n] == pTarget ){` |
|      3 |  4445 | `			bYes = 1;` |
|      3 |  4446 | `			break;` |
|      - |  4447 | `		}` |
|    ! 0 |  4448 | `	}` |
|      3 |  4449 | `	SySetRelease(&aSet);` |
|      3 |  4450 | `	ph7_result_bool(pCtx, bYes);` |
|      3 |  4451 | `	return PH7_OK;` |
|      7 |  4452 | `}` |
|      8 |  4453 | `static int vm_builtin_ReflectionClass_isInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4454 | `{` |
|      9 |  4455 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4456 | `	ph7_class_instance *pObj;` |
|      9 |  4457 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 \|\| pClass == 0 ){` |
|    ! 0 |  4458 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4459 | `		return PH7_OK;` |
|      - |  4460 | `	}` |
|      9 |  4461 | `	pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      9 |  4462 | `	ph7_result_bool(pCtx, PH7_VmInstanceOf(pObj->pClass, pClass) != 0);` |
|      9 |  4463 | `	return PH7_OK;` |
|      5 |  4464 | `}` |
|      - |  4465 | `/* ---- source position ---- */` |
|     10 |  4466 | `static int vm_builtin_ReflectionClass_getStartLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4467 | `{` |
|     11 |  4468 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      5 |  4469 | `	SXUNUSED(nArg);` |
|      5 |  4470 | `	SXUNUSED(apArg);` |
|     11 |  4471 | `	if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_INTERNAL) ){` |
|      3 |  4472 | `		ph7_result_bool(pCtx, 0);` |
|      2 |  4473 | `	}else{` |
|      9 |  4474 | `		ph7_result_int64(pCtx, (sxi64)pClass->nLine);` |
|      - |  4475 | `	}` |
|     11 |  4476 | `	return PH7_OK;` |
|      1 |  4477 | `}` |
|      6 |  4478 | `static int vm_builtin_ReflectionClass_getEndLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4479 | `{` |
|      7 |  4480 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      3 |  4481 | `	SXUNUSED(nArg);` |
|      3 |  4482 | `	SXUNUSED(apArg);` |
|      7 |  4483 | `	if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_INTERNAL) ){` |
|      3 |  4484 | `		ph7_result_bool(pCtx, 0);` |
|      2 |  4485 | `	}else{` |
|      5 |  4486 | `		ph7_result_int64(pCtx, (sxi64)pClass->nEndLine);` |
|      - |  4487 | `	}` |
|      7 |  4488 | `	return PH7_OK;` |
|      1 |  4489 | `}` |
|     18 |  4490 | `static int vm_builtin_ReflectionClass_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4491 | `{` |
|     20 |  4492 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      9 |  4493 | `	SXUNUSED(nArg);` |
|      9 |  4494 | `	SXUNUSED(apArg);` |
|     20 |  4495 | `	if( pClass && SyStringLength(&pClass->sFile) > 0 ){` |
|     10 |  4496 | `		ph7_result_string(pCtx, SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));` |
|      6 |  4497 | `	}else{` |
|     11 |  4498 | `		ph7_result_bool(pCtx, 0);` |
|      - |  4499 | `	}` |
|     20 |  4500 | `	return PH7_OK;` |
|      2 |  4501 | `}` |
|      8 |  4502 | `static int vm_builtin_ReflectionClass_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4503 | `{` |
|      9 |  4504 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      4 |  4505 | `	SXUNUSED(nArg);` |
|      4 |  4506 | `	SXUNUSED(apArg);` |
|      9 |  4507 | `	if( pClass && SyStringLength(&pClass->sDoc) > 0 ){` |
|      3 |  4508 | `		ph7_result_string(pCtx, SyStringData(&pClass->sDoc), (int)SyStringLength(&pClass->sDoc));` |
|      2 |  4509 | `	}else{` |
|      7 |  4510 | `		ph7_result_bool(pCtx, 0);` |
|      - |  4511 | `	}` |
|      9 |  4512 | `	return PH7_OK;` |
|      1 |  4513 | `}` |
|      - |  4514 | `/* ---- instantiation ---- */` |
|     88 |  4515 | `static int vm_builtin_ReflectionClass_isInstantiable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  4516 | `{` |
|     92 |  4517 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4518 | `	sxi32 iCtor, iClone;` |
|     44 |  4519 | `	SXUNUSED(nArg);` |
|     44 |  4520 | `	SXUNUSED(apArg);` |
|     88 |  4521 | `	if( pClass == 0` |
|     92 |  4522 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT\|PH7_CLASS_ENUM)) ){` |
|     13 |  4523 | `		ph7_result_bool(pCtx, 0);` |
|     13 |  4524 | `		return PH7_OK;` |
|      - |  4525 | `	}` |
|     80 |  4526 | `	ReflectCtorCloneVis(pCtx->pVm, pClass, &iCtor, &iClone);` |
|     80 |  4527 | `	ph7_result_bool(pCtx, iCtor == 0 \|\| iCtor == PH7_CLASS_PROT_PUBLIC);` |
|     80 |  4528 | `	return PH7_OK;` |
|     48 |  4529 | `}` |
|     36 |  4530 | `static int vm_builtin_ReflectionClass_isCloneable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4531 | `{` |
|     38 |  4532 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4533 | `	sxi32 iCtor, iClone;` |
|     18 |  4534 | `	SXUNUSED(nArg);` |
|     18 |  4535 | `	SXUNUSED(apArg);` |
|     36 |  4536 | `	if( pClass == 0` |
|     38 |  4537 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT)) ){` |
|      3 |  4538 | `		ph7_result_bool(pCtx, 0);` |
|      3 |  4539 | `		return PH7_OK;` |
|      - |  4540 | `	}` |
|     36 |  4541 | `	ReflectCtorCloneVis(pCtx->pVm, pClass, &iCtor, &iClone);` |
|     36 |  4542 | `	if( iClone != 0 ){` |
|      - |  4543 | `		/* php consults a declared __clone FIRST and answers its visibility --` |
|      - |  4544 | ``		 * even when the clone_obj refusal would still answer `clone` itself, so`` |
|      - |  4545 | `		 * a subclass of Exception that declares a public __clone reports TRUE` |
|      - |  4546 | `		 * and refuses anyway. php's own inconsistency, kept. */` |
|      7 |  4547 | `		ph7_result_bool(pCtx, iClone == PH7_CLASS_PROT_PUBLIC);` |
|      7 |  4548 | `		return PH7_OK;` |
|      - |  4549 | `	}` |
|      - |  4550 | `	/* No __clone anywhere: php's answer is whether the clone_obj handler` |
|      - |  4551 | `	 * exists. The refusal flag walks the base chain like the handler it` |
|      - |  4552 | ``	 * models, so `class M extends IteratorIterator {}` reports false too. */`` |
|     30 |  4553 | `	ph7_result_bool(pCtx, !PH7_ClassIsUncloneable(pClass));` |
|     30 |  4554 | `	return PH7_OK;` |
|     20 |  4555 | `}` |
|      - |  4556 | `/*` |
|      - |  4557 | ` * php's own gate, raised before any object exists -- the one every C-side` |
|      - |  4558 | ` * instantiation asks, Reflection's newInstance() and PDO's FETCH_CLASS alike.` |
|      - |  4559 | ` */` |
|    320 |  4560 | `PH7_PRIVATE sxi32 PH7_VmCheckInstantiable(ph7_context *pCtx, ph7_class *pClass)` |
|      4 |  4561 | `{` |
|    324 |  4562 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|      3 |  4563 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate interface %z", &pClass->sDisp);` |
|      - |  4564 | `	}` |
|    322 |  4565 | `	if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|      3 |  4566 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate trait %z", &pClass->sDisp);` |
|      - |  4567 | `	}` |
|    320 |  4568 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|      - |  4569 | `		/* php 8.1 names the enum rather than the FINAL class it also is. */` |
|      3 |  4570 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate enum %z", &pClass->sDisp);` |
|      - |  4571 | `	}` |
|    318 |  4572 | `	if( pClass->iFlags & PH7_CLASS_ABSTRACT ){` |
|      7 |  4573 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate abstract class %z", &pClass->sDisp);` |
|      - |  4574 | `	}` |
|    312 |  4575 | `	if( pClass->iFlags & PH7_CLASS_NOINSTANTIATE ){` |
|      - |  4576 | ``		/* The `new` path's create_object refusal, which php raises here too —`` |
|      - |  4577 | `		 * ReflectionClass::newInstance() on a Closure is the same Error, not a` |
|      - |  4578 | `		 * visibility one about its private constructor. */` |
|     12 |  4579 | `		if( pClass->zNewRefusal ){` |
|     10 |  4580 | `			return PH7_VmThrowException(pCtx,` |
|      8 |  4581 | `				pClass->zNewRefusalClass ? pClass->zNewRefusalClass : "Error",` |
|      4 |  4582 | `				"%s", pClass->zNewRefusal);` |
|      - |  4583 | `		}` |
|      4 |  4584 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      1 |  4585 | `			"Instantiation of class %z is not allowed", &pClass->sDisp);` |
|      - |  4586 | `	}` |
|    302 |  4587 | `	return PH7_OK;` |
|    164 |  4588 | `}` |
|      - |  4589 | `/*` |
|      - |  4590 | ` * newInstance()/newInstanceArgs(): the checks php runs, then the constructor.` |
|      - |  4591 | ` * apCtor/nCtor are already-collected arguments; pMap names them when the` |
|      - |  4592 | `` * caller wrote `name:` or handed an array with string keys (ReflectDoorArgMap).`` |
|      - |  4593 | ` * nCounted is how many of them the no-constructor refusal counts: php's` |
|      - |  4594 | `` * newInstance() asks ZEND_NUM_ARGS(), which a `name:` never adds to, while`` |
|      - |  4595 | ` * newInstanceArgs() counts its array's every element.` |
|      - |  4596 | ` */` |
|     70 |  4597 | `static int ReflectNewInstance(ph7_context *pCtx, int nCtor, ph7_value **apCtor,` |
|      - |  4598 | `	VmCallArgMap *pMap, int nCounted)` |
|      4 |  4599 | `{` |
|     74 |  4600 | `	ph7_vm *pVm = pCtx->pVm;` |
|     74 |  4601 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4602 | `	ph7_class_instance *pObj;` |
|      - |  4603 | `	ph7_class_method *pCons;` |
|      - |  4604 | `	sxi32 iCtorVis, iCloneVis, rc;` |
|     74 |  4605 | `	if( pClass == 0 ){` |
|    ! 0 |  4606 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4607 | `		return PH7_OK;` |
|      - |  4608 | `	}` |
|     74 |  4609 | `	rc = PH7_VmCheckInstantiable(pCtx, pClass);` |
|     74 |  4610 | `	if( rc != PH7_OK ){` |
|     14 |  4611 | `		return rc;` |
|      - |  4612 | `	}` |
|     62 |  4613 | `	ReflectCtorCloneVis(pVm, pClass, &iCtorVis, &iCloneVis);` |
|     62 |  4614 | `	if( iCtorVis != 0 && iCtorVis != PH7_CLASS_PROT_PUBLIC ){` |
|      4 |  4615 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  4616 | `			"Access to non-public constructor of class %z", &pClass->sDisp);` |
|      - |  4617 | `	}` |
|     60 |  4618 | `	if( iCtorVis == 0 && nCounted > 0 ){` |
|     13 |  4619 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  4620 | `			"Class %z does not have a constructor, so you cannot pass any constructor arguments",` |
|      4 |  4621 | `			&pClass->sDisp);` |
|      - |  4622 | `	}` |
|     52 |  4623 | `	if( VmClassStaticDeferPending(pClass) ){` |
|      - |  4624 | `		/* Instantiation materializes the static table (OP_NEW does it too), so a` |
|      - |  4625 | `		 * broken default raises BEFORE any object exists. */` |
|      3 |  4626 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pVm, pClass);` |
|      3 |  4627 | `		if( rcMat != SXRET_OK ){` |
|      3 |  4628 | `			return rcMat;` |
|      - |  4629 | `		}` |
|    ! 0 |  4630 | `	}` |
|     50 |  4631 | `	pObj = PH7_NewClassInstance(pVm, pClass);` |
|     50 |  4632 | `	if( pObj == 0 ){` |
|    ! 0 |  4633 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4634 | `		return PH7_OK;` |
|      - |  4635 | `	}` |
|     50 |  4636 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|     50 |  4637 | `	if( pCons ){` |
|      - |  4638 | `		/* Weak binding, like ReflectMethodInvoke: the frame that makes this call is` |
|      - |  4639 | `		 * ReflectionClass::newInstance(), an internal function. */` |
|     36 |  4640 | `		pVm->bCallbackWeak = 1;` |
|     36 |  4641 | `		rc = PH7_VmCallClassMethodMap(pVm, pObj, pCons, 0, nCtor, apCtor, pMap);` |
|     36 |  4642 | `		pVm->bCallbackWeak = 0;` |
|     36 |  4643 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|     15 |  4644 | `			PH7_ClassInstanceCtorFailed(pObj);` |
|     15 |  4645 | `			PH7_ClassInstanceUnref(pObj);` |
|     15 |  4646 | `			return rc;` |
|      - |  4647 | `		}` |
|     10 |  4648 | `	}` |
|     37 |  4649 | `	return ReflectResultObject(pCtx, pObj);` |
|     39 |  4650 | `}` |
|     54 |  4651 | `static int vm_builtin_ReflectionClass_newInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  4652 | `{` |
|      - |  4653 | `	VmCallArgMap sMap;` |
|     58 |  4654 | `	VmCallArgMap *pMap = ReflectDoorArgMap(pCtx, 0, 0, 0, &sMap);` |
|     58 |  4655 | `	int nPositional = 0;` |
|      - |  4656 | `	/* Named arguments follow every positional one. */` |
|    109 |  4657 | `	while( nPositional < nArg && (pMap == 0 \|\| (sxu32)nPositional >= pMap->nTotal` |
|     34 |  4658 | `			\|\| pMap->aNames[nPositional].nByte == 0) ){` |
|     30 |  4659 | `		nPositional++;` |
|      4 |  4660 | `	}` |
|     58 |  4661 | `	return ReflectNewInstance(pCtx, nArg, apArg, pMap, nPositional);` |
|      4 |  4662 | `}` |
|     16 |  4663 | `static int vm_builtin_ReflectionClass_newInstanceArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  4664 | `{` |
|      - |  4665 | `	SySet aArg;` |
|     19 |  4666 | `	SyString *aNames = 0;` |
|      - |  4667 | `	VmCallArgMap sMap;` |
|      - |  4668 | `	int rc;` |
|     19 |  4669 | `	SySetInit(&aArg, &pCtx->pVm->sAllocator, sizeof(ph7_value *));` |
|     19 |  4670 | `	if( nArg > 0 ){` |
|     19 |  4671 | `		ReflectCollectArgs(pCtx, apArg[0], &aArg, &aNames, 0);` |
|      8 |  4672 | `	}` |
|     27 |  4673 | `	rc = ReflectNewInstance(pCtx, (int)SySetUsed(&aArg), (ph7_value **)SySetBasePtr(&aArg),` |
|     10 |  4674 | `		aNames ? ReflectDoorArgMap(pCtx, aNames, SySetUsed(&aArg), 0, &sMap) : 0,` |
|     16 |  4675 | `		(int)SySetUsed(&aArg));` |
|     19 |  4676 | `	if( aNames ){` |
|      5 |  4677 | `		SyMemBackendFree(&pCtx->pVm->sAllocator, aNames);` |
|      2 |  4678 | `	}` |
|     19 |  4679 | `	SySetRelease(&aArg);` |
|     19 |  4680 | `	return rc;` |
|      3 |  4681 | `}` |
|    186 |  4682 | `static int vm_builtin_ReflectionClass_newInstanceWithoutConstructor(ph7_context *pCtx,` |
|      - |  4683 | `	int nArg, ph7_value **apArg)` |
|      4 |  4684 | `{` |
|    190 |  4685 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4686 | `	sxi32 rc;` |
|     93 |  4687 | `	SXUNUSED(nArg);` |
|     93 |  4688 | `	SXUNUSED(apArg);` |
|    190 |  4689 | `	if( pClass == 0 ){` |
|    ! 0 |  4690 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4691 | `		return PH7_OK;` |
|      - |  4692 | `	}` |
|    190 |  4693 | `	rc = PH7_VmCheckInstantiable(pCtx, pClass);` |
|    190 |  4694 | `	if( rc != PH7_OK ){` |
|      3 |  4695 | `		return rc;` |
|      - |  4696 | `	}` |
|    188 |  4697 | `	if( VmClassStaticDeferPending(pClass) ){` |
|      3 |  4698 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);` |
|      3 |  4699 | `		if( rcMat != SXRET_OK ){` |
|      3 |  4700 | `			return rcMat;` |
|      - |  4701 | `		}` |
|    ! 0 |  4702 | `	}` |
|    185 |  4703 | `	return ReflectResultObject(pCtx, PH7_NewClassInstance(pCtx->pVm, pClass));` |
|     97 |  4704 | `}` |
|      - |  4705 | `/* ---- members ---- */` |
|      - |  4706 | `/* The modifier mask php filters a member on. */` |
|    364 |  4707 | `static sxi64 ReflectVisMask(sxi32 iProt)` |
|      3 |  4708 | `{` |
|    367 |  4709 | `	if( iProt == PH7_CLASS_PROT_PUBLIC ){` |
|    287 |  4710 | `		return 1;` |
|      - |  4711 | `	}` |
|     82 |  4712 | `	return iProt == PH7_CLASS_PROT_PROTECTED ? 2 : 4;` |
|    185 |  4713 | `}` |
|      - |  4714 | `/*` |
|      - |  4715 | ` * Is this property protected(set) as far as php's Reflection is concerned?` |
|      - |  4716 | ` *` |
|      - |  4717 | `` * The bit is either DECLARED or implied by `readonly` — but readonly implies it`` |
|      - |  4718 | `` * only for a property whose read side is PUBLIC. A `protected readonly` or`` |
|      - |  4719 | `` * `private readonly` one already writes no wider than it reads, so php adds`` |
|      - |  4720 | `` * nothing (`private readonly int $v` is modifiers 132, not 2180), and an explicit`` |
|      - |  4721 | `` * `private(set)` beside readonly is the set visibility, so readonly adds nothing`` |
|      - |  4722 | `` * there either (`public private(set) readonly` is 4257).`` |
|      - |  4723 | ` */` |
|    248 |  4724 | `static int ReflectPropProtectedSet(ph7_class_attr *pAttr)` |
|      2 |  4725 | `{` |
|    250 |  4726 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET ){` |
|     28 |  4727 | `		return 1;` |
|      - |  4728 | `	}` |
|    244 |  4729 | `	return (pAttr->iFlags & PH7_CLASS_ATTR_READONLY)` |
|    131 |  4730 | `	    && (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) == 0` |
|    242 |  4731 | `	    && pAttr->iProtection == PH7_CLASS_PROT_PUBLIC;` |
|    126 |  4732 | `}` |
|    222 |  4733 | `static sxi64 ReflectPropModifiers(ph7_class_attr *pAttr)` |
|      2 |  4734 | `{` |
|    224 |  4735 | `	sxi64 iMods = ReflectVisMask(pAttr->iProtection);` |
|    224 |  4736 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){ iMods \|= 16; }` |
|      - |  4737 | `	/* php's IS_VIRTUAL is "there is no slot behind this name", and it has two` |
|      - |  4738 | `	 * sources: a HOOKED property with no backing store, and a NATIVE class's` |
|      - |  4739 | `	 * property that php fabricates from its own C struct (DatePeriod's, and` |
|      - |  4740 | `	 * BcMath\Number's value/scale). Both report virtual there. */` |
|    224 |  4741 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_VIRTUAL\|PH7_CLASS_ATTR_NATIVE_VIRTUAL) ){` |
|     69 |  4742 | `		iMods \|= 512;` |
|     34 |  4743 | `	}` |
|    224 |  4744 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){ iMods \|= 128; }` |
|    224 |  4745 | `	if( ReflectPropProtectedSet(pAttr) ){ iMods \|= 2048; }` |
|      - |  4746 | ``	/* PHP 8.4's `final` PROPERTY -- IS_FINAL, the same bit a method and a class`` |
|      - |  4747 | `	 * constant carry. */` |
|    224 |  4748 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods \|= 32; }` |
|    224 |  4749 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|      - |  4750 | `		/* private(set) cannot be widened by a subclass, so php reports it FINAL. */` |
|     19 |  4751 | `		iMods \|= 4096\|32;` |
|      9 |  4752 | `	}` |
|    224 |  4753 | `	return iMods;` |
|      2 |  4754 | `}` |
|    116 |  4755 | `static sxi64 ReflectMethodModifiers(ph7_class_method *pMeth)` |
|      2 |  4756 | `{` |
|    118 |  4757 | `	sxi64 iMods = ReflectVisMask(pMeth->iProtection);` |
|    118 |  4758 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){ iMods \|= 16; }` |
|    118 |  4759 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){ iMods \|= 64; }` |
|    118 |  4760 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods \|= 32; }` |
|    118 |  4761 | `	return iMods;` |
|      2 |  4762 | `}` |
|     26 |  4763 | `static sxi64 ReflectConstModifiers(ph7_class_attr *pAttr)` |
|      1 |  4764 | `{` |
|     27 |  4765 | `	sxi64 iMods = ReflectVisMask(pAttr->iProtection);` |
|     27 |  4766 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods \|= 32; }` |
|     27 |  4767 | `	return iMods;` |
|      1 |  4768 | `}` |
|      - |  4769 | `/* Does the reflected object own a property under this name? (1 = exists) */` |
|     18 |  4770 | `static int ReflectObjHasProp(ph7_class_instance *pObj, const char *zName, int nName)` |
|      1 |  4771 | `{` |
|     14 |  4772 | `	return pObj != 0 && nName > 0` |
|     20 |  4773 | `		&& SyHashGet(&pObj->hAttr, (const void *)zName, (sxu32)nName) != 0;` |
|      1 |  4774 | `}` |
|     18 |  4775 | `static int vm_builtin_ReflectionClass_hasMethod(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4776 | `{` |
|     20 |  4777 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4778 | `	const char *zName;` |
|      - |  4779 | `	int nName;` |
|     20 |  4780 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  4781 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4782 | `		return PH7_OK;` |
|      - |  4783 | `	}` |
|     20 |  4784 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     20 |  4785 | `	ph7_result_bool(pCtx, ReflectFindMethodEntry(pClass, zName, nName) != 0);` |
|     20 |  4786 | `	return PH7_OK;` |
|     11 |  4787 | `}` |
|     18 |  4788 | `static int vm_builtin_ReflectionClass_hasProperty(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4789 | `{` |
|     20 |  4790 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4791 | `	const char *zName;` |
|     20 |  4792 | `	int nName, bFound = 0;` |
|      - |  4793 | `	SySet aMembers;` |
|      - |  4794 | `	sxu32 n;` |
|     20 |  4795 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  4796 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4797 | `		return PH7_OK;` |
|      - |  4798 | `	}` |
|     20 |  4799 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     20 |  4800 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     20 |  4801 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     56 |  4802 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     44 |  4803 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     44 |  4804 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zName, nName) ){` |
|      8 |  4805 | `			bFound = 1;` |
|      8 |  4806 | `			break;` |
|      - |  4807 | `		}` |
|     19 |  4808 | `	}` |
|     20 |  4809 | `	SySetRelease(&aMembers);` |
|      - |  4810 | `	/* A ReflectionObject also sees the instance's own dynamic properties */` |
|     20 |  4811 | `	if( !bFound ){` |
|     13 |  4812 | `		bFound = ReflectObjHasProp(ReflectClassObj(pCtx), zName, nName);` |
|      6 |  4813 | `	}` |
|     20 |  4814 | `	ph7_result_bool(pCtx, bFound);` |
|     20 |  4815 | `	return PH7_OK;` |
|     11 |  4816 | `}` |
|     24 |  4817 | `static int vm_builtin_ReflectionClass_hasConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4818 | `{` |
|     26 |  4819 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4820 | `	const char *zName;` |
|     26 |  4821 | `	int nName, bFound = 0;` |
|      - |  4822 | `	SySet aMembers;` |
|      - |  4823 | `	sxu32 n;` |
|     26 |  4824 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  4825 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4826 | `		return PH7_OK;` |
|      - |  4827 | `	}` |
|     26 |  4828 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     26 |  4829 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     26 |  4830 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|   1320 |  4831 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   1300 |  4832 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   1300 |  4833 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|      5 |  4834 | `			bFound = 1;` |
|      5 |  4835 | `			break;` |
|      - |  4836 | `		}` |
|    649 |  4837 | `	}` |
|     26 |  4838 | `	SySetRelease(&aMembers);` |
|     26 |  4839 | `	ph7_result_bool(pCtx, bFound);` |
|     26 |  4840 | `	return PH7_OK;` |
|     14 |  4841 | `}` |
|      - |  4842 | `/*` |
|      - |  4843 | ` * The value of a class constant, materializing its lazily-evaluated slot.` |
|      - |  4844 | ` *` |
|      - |  4845 | ` * php re-evaluates a failed initializer on EVERY read, so this can raise; the` |
|      - |  4846 | ` * status is returned rather than swallowed, because a native body that answers` |
|      - |  4847 | ` * PH7_OK with a throw in flight lets the caller carry on and print the NULL it` |
|      - |  4848 | ` * never should have seen.` |
|      - |  4849 | ` */` |
|    860 |  4850 | `static sxi32 ReflectConstSlot(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr,` |
|      - |  4851 | `	ph7_value **ppOut)` |
|      5 |  4852 | `{` |
|    865 |  4853 | `	sxi32 rc = PH7_VmMaterializeClassConst(pCtx->pVm, pClass, pAttr);` |
|    865 |  4854 | `	*ppOut = 0;` |
|    865 |  4855 | `	if( rc != SXRET_OK ){` |
|      3 |  4856 | `		return rc;` |
|      - |  4857 | `	}` |
|    862 |  4858 | `	*ppOut = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|    862 |  4859 | `	return SXRET_OK;` |
|    421 |  4860 | `}` |
|     10 |  4861 | `static int vm_builtin_ReflectionClass_getConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4862 | `{` |
|     12 |  4863 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4864 | `	const char *zName;` |
|      - |  4865 | `	int nName;` |
|      - |  4866 | `	SySet aMembers;` |
|      - |  4867 | `	sxu32 n;` |
|     12 |  4868 | `	ph7_class_attr *pFound = 0;` |
|     12 |  4869 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  4870 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4871 | `		return PH7_OK;` |
|      - |  4872 | `	}` |
|     12 |  4873 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     12 |  4874 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     12 |  4875 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     28 |  4876 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     28 |  4877 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     28 |  4878 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|     12 |  4879 | `			pFound = pM->pAttr;` |
|     12 |  4880 | `			break;` |
|      - |  4881 | `		}` |
|      9 |  4882 | `	}` |
|     12 |  4883 | `	SySetRelease(&aMembers);` |
|     12 |  4884 | `	if( pFound == 0 ){` |
|    ! 0 |  4885 | `		PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED,` |
|      - |  4886 | `			"ReflectionClass::getConstant() for a non-existent constant is deprecated, "` |
|      - |  4887 | `			"use ReflectionClass::hasConstant() to check if the constant exists");` |
|    ! 0 |  4888 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4889 | `		return PH7_OK;` |
|      - |  4890 | `	}` |
|      - |  4891 | `	{` |
|      - |  4892 | `		ph7_value *pVal;` |
|     12 |  4893 | `		sxi32 rc = ReflectConstSlot(pCtx, pClass, pFound, &pVal);` |
|     12 |  4894 | `		if( rc != SXRET_OK ){` |
|      3 |  4895 | `			return rc;` |
|      - |  4896 | `		}` |
|      9 |  4897 | `		if( pVal ){` |
|      9 |  4898 | `			ph7_result_value(pCtx, pVal);` |
|      5 |  4899 | `		}else{` |
|    ! 0 |  4900 | `			ph7_result_null(pCtx);` |
|      - |  4901 | `		}` |
|      - |  4902 | `	}` |
|      9 |  4903 | `	return PH7_OK;` |
|      7 |  4904 | `}` |
|     55 |  4905 | `static int vm_builtin_ReflectionClass_getConstants(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  4906 | `{` |
|     60 |  4907 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     60 |  4908 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  4909 | `	SySet aMembers;` |
|      - |  4910 | `	sxu32 n;` |
|     60 |  4911 | `	int bFilter = (nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0);` |
|     60 |  4912 | `	sxi64 iFilter = bFilter ? ph7_value_to_int64(apArg[0]) : 0;` |
|     60 |  4913 | `	if( pOut == 0 ){` |
|    ! 0 |  4914 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  4915 | `	}` |
|     60 |  4916 | `	if( pClass == 0 ){` |
|    ! 0 |  4917 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  4918 | `		return PH7_OK;` |
|      - |  4919 | `	}` |
|     60 |  4920 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     60 |  4921 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|    485 |  4922 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    428 |  4923 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - |  4924 | `		ph7_value *pVal;` |
|    428 |  4925 | `		if( pM->iKind != REFLECT_MEMBER_CONST ){` |
|    304 |  4926 | `			continue;` |
|      - |  4927 | `		}` |
|    128 |  4928 | `		if( bFilter && (ReflectConstModifiers(pM->pAttr) & iFilter) == 0 ){` |
|      5 |  4929 | `			continue;` |
|      - |  4930 | `		}` |
|      - |  4931 | `		{` |
|    124 |  4932 | `			sxi32 rc = ReflectConstSlot(pCtx, pClass, pM->pAttr, &pVal);` |
|    124 |  4933 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  4934 | `				SySetRelease(&aMembers);` |
|    ! 0 |  4935 | `				return rc;` |
|      - |  4936 | `			}` |
|      - |  4937 | `		}` |
|    124 |  4938 | `		if( pVal ){` |
|    124 |  4939 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, pVal);` |
|     47 |  4940 | `		}` |
|     49 |  4941 | `	}` |
|     60 |  4942 | `	SySetRelease(&aMembers);` |
|     60 |  4943 | `	ph7_result_value(pCtx, pOut);` |
|     60 |  4944 | `	return PH7_OK;` |
|     32 |  4945 | `}` |
|      - |  4946 | `/*` |
|      - |  4947 | ` * getMethod()/getProperty()/getReflectionConstant() build a class that is still` |
|      - |  4948 | ` * PRELUDE PHP, so they go through its constructor (ReflectConstruct). They` |
|      - |  4949 | ` * become direct C when chunks 2 and 3 land.` |
|      - |  4950 | ` */` |
|    120 |  4951 | `static int ReflectResultMember(ph7_context *pCtx, const char *zClass,` |
|      - |  4952 | `	ph7_value *pTarget, const SyString *pName)` |
|      4 |  4953 | `{` |
|      - |  4954 | `	ph7_value sName;` |
|      - |  4955 | `	ph7_value *apCtor[2];` |
|      - |  4956 | `	ph7_class_instance *pOut;` |
|      - |  4957 | `	sxi32 rc;` |
|    124 |  4958 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|    124 |  4959 | `	ph7_value_string(&sName, SyStringData(pName), (int)SyStringLength(pName));` |
|    124 |  4960 | `	apCtor[0] = pTarget;` |
|    124 |  4961 | `	apCtor[1] = &sName;` |
|      - |  4962 | `	/* A FACTORY build, not a user constructor call — see ph7_vm::nReflectFactory. */` |
|    124 |  4963 | `	pCtx->pVm->nReflectFactory++;` |
|    124 |  4964 | `	pOut = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|    124 |  4965 | `	pCtx->pVm->nReflectFactory--;` |
|    124 |  4966 | `	PH7_MemObjRelease(&sName);` |
|    124 |  4967 | `	if( pOut == 0 ){` |
|    ! 0 |  4968 | `		if( rc != PH7_OK ){` |
|    ! 0 |  4969 | `			return rc;` |
|      - |  4970 | `		}` |
|    ! 0 |  4971 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4972 | `		return PH7_OK;` |
|      - |  4973 | `	}` |
|    124 |  4974 | `	return ReflectResultObject(pCtx, pOut);` |
|     64 |  4975 | `}` |
|      - |  4976 | `/*` |
|      - |  4977 | `` * A `ReflectionMethod($this->..., ...)`-shaped first argument.`` |
|      - |  4978 | ` *` |
|      - |  4979 | ` * The OBJECT when this reflector was built over one (a ReflectionObject), its` |
|      - |  4980 | ` * class NAME otherwise. For every ordinary member the two are interchangeable --` |
|      - |  4981 | ` * the constructor resolves an object to its class either way -- but not for a` |
|      - |  4982 | `` * FABRICATED method: `(new ReflectionObject($c))->getMethod('__invoke')` describes`` |
|      - |  4983 | `` * THE CLOSURE's parameters where `(new ReflectionClass('Closure'))`` |
|      - |  4984 | `` * ->getMethod('__invoke')` has nothing to describe, and php draws that line at`` |
|      - |  4985 | ` * exactly this question. Takes a reference, so the caller's release balances.` |
|      - |  4986 | ` */` |
|    438 |  4987 | `static void ReflectSelfName(ph7_context *pCtx, ph7_value *pOut)` |
|      5 |  4988 | `{` |
|    443 |  4989 | `	ph7_class_instance *pObj = ReflectClassObj(pCtx);` |
|      - |  4990 | `	const char *zName;` |
|      - |  4991 | `	int nName;` |
|    443 |  4992 | `	PH7_MemObjInit(pCtx->pVm, pOut);` |
|    443 |  4993 | `	if( pObj ){` |
|     24 |  4994 | `		pObj->iRef++;` |
|     24 |  4995 | `		pOut->x.pOther = pObj;` |
|     24 |  4996 | `		pOut->iFlags = MEMOBJ_OBJ;` |
|     24 |  4997 | `		return;` |
|      - |  4998 | `	}` |
|    421 |  4999 | `	ReflectClassName(pCtx, &zName, &nName);` |
|    421 |  5000 | `	ph7_value_string(pOut, zName, nName);` |
|    224 |  5001 | `}` |
|     84 |  5002 | `static int vm_builtin_ReflectionClass_getMethod(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  5003 | `{` |
|     86 |  5004 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  5005 | `	SyHashEntry *pEntry;` |
|      - |  5006 | `	const char *zName;` |
|      - |  5007 | `	int nName;` |
|      - |  5008 | `	SyString sFound;` |
|     86 |  5009 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  5010 | `		ph7_result_null(pCtx);` |
|    ! 0 |  5011 | `		return PH7_OK;` |
|      - |  5012 | `	}` |
|     86 |  5013 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     86 |  5014 | `	pEntry = ReflectFindMethodEntry(pClass, zName, nName);` |
|     86 |  5015 | `	if( pEntry == 0 ){` |
|      4 |  5016 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  5017 | `			"Method %z::%.*s() does not exist", &pClass->sDisp, nName, zName);` |
|      - |  5018 | `	}` |
|      - |  5019 | `	/* The reported name is the DECLARED spelling, whatever case was asked for. */` |
|     84 |  5020 | `	SyStringInitFromBuf(&sFound, (const char *)pEntry->pKey, pEntry->nKeyLen);` |
|      - |  5021 | `	{` |
|      - |  5022 | `		ph7_value sSelf;` |
|      - |  5023 | `		int rc;` |
|     84 |  5024 | `		ReflectSelfName(pCtx, &sSelf);` |
|     84 |  5025 | `		rc = ReflectResultMember(pCtx, "ReflectionMethod", &sSelf, &sFound);` |
|     84 |  5026 | `		PH7_MemObjRelease(&sSelf);` |
|     84 |  5027 | `		return rc;` |
|      - |  5028 | `	}` |
|     44 |  5029 | `}` |
|     28 |  5030 | `static int vm_builtin_ReflectionClass_getConstructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  5031 | `{` |
|     31 |  5032 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  5033 | `	SyHashEntry *pEntry;` |
|      - |  5034 | `	SyString sFound;` |
|     14 |  5035 | `	SXUNUSED(nArg);` |
|     14 |  5036 | `	SXUNUSED(apArg);` |
|     31 |  5037 | `	if( pClass == 0 ){` |
|    ! 0 |  5038 | `		ph7_result_null(pCtx);` |
|    ! 0 |  5039 | `		return PH7_OK;` |
|      - |  5040 | `	}` |
|      - |  5041 | `	/* No PHP-4 class-name constructor: a method named like the class is a plain` |
|      - |  5042 | `	 * method (removed in 8.0), so this stays null without an explicit one. */` |
|     31 |  5043 | `	pEntry = ReflectFindMethodEntry(pClass, "__construct", sizeof("__construct")-1);` |
|     31 |  5044 | `	if( pEntry == 0 ){` |
|     11 |  5045 | `		ph7_result_null(pCtx);` |
|     11 |  5046 | `		return PH7_OK;` |
|      - |  5047 | `	}` |
|     23 |  5048 | `	SyStringInitFromBuf(&sFound, (const char *)pEntry->pKey, pEntry->nKeyLen);` |
|      - |  5049 | `	{` |
|      - |  5050 | `		ph7_value sSelf;` |
|      - |  5051 | `		int rc;` |
|     23 |  5052 | `		ReflectSelfName(pCtx, &sSelf);` |
|     23 |  5053 | `		rc = ReflectResultMember(pCtx, "ReflectionMethod", &sSelf, &sFound);` |
|     23 |  5054 | `		PH7_MemObjRelease(&sSelf);` |
|     23 |  5055 | `		return rc;` |
|      - |  5056 | `	}` |
|     17 |  5057 | `}` |
|      - |  5058 | `/*` |
|      - |  5059 | ` * getMethods() / getProperties() / getReflectionConstants(): one member walk,` |
|      - |  5060 | ` * one reflector per surviving member. Each reflector is a prelude class built` |
|      - |  5061 | ` * through its constructor, and is appended through a STACK carrier so the list` |
|      - |  5062 | ` * takes its own reference rather than the creation one.` |
|      - |  5063 | ` */` |
|    320 |  5064 | `static int ReflectMemberList(ph7_context *pCtx, int iKind, const char *zClass,` |
|      - |  5065 | `	int nArg, ph7_value **apArg)` |
|      5 |  5066 | `{` |
|    325 |  5067 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|    325 |  5068 | `	ph7_class_instance *pObj = ReflectClassObj(pCtx);` |
|    325 |  5069 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  5070 | `	ph7_value sSelf;` |
|      - |  5071 | `	SySet aMembers;` |
|      - |  5072 | `	sxu32 n;` |
|    325 |  5073 | `	int bFilter = (nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0);` |
|    325 |  5074 | `	sxi64 iFilter = bFilter ? ph7_value_to_int64(apArg[0]) : 0;` |
|    325 |  5075 | `	if( pOut == 0 ){` |
|    ! 0 |  5076 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  5077 | `	}` |
|    325 |  5078 | `	if( pClass == 0 ){` |
|    ! 0 |  5079 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  5080 | `		return PH7_OK;` |
|      - |  5081 | `	}` |
|    325 |  5082 | `	ReflectSelfName(pCtx, &sSelf);` |
|    325 |  5083 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|    325 |  5084 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|   5013 |  5085 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   4693 |  5086 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - |  5087 | `		ph7_value sName, sVal;` |
|      - |  5088 | `		ph7_value *apCtor[2];` |
|      - |  5089 | `		ph7_class_instance *pRef;` |
|      - |  5090 | `		sxi32 rc;` |
|   4693 |  5091 | `		if( pM->iKind != iKind ){` |
|   2908 |  5092 | `			continue;` |
|      - |  5093 | `		}` |
|   1828 |  5094 | `		if( iKind == REFLECT_MEMBER_PROP && pObj != 0 && pM->pAttr != 0` |
|    142 |  5095 | `		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT) == 0` |
|    133 |  5096 | `		 && PH7_ATTR_LAZY_ABSENT(pM->pAttr,pObj) ){` |
|      - |  5097 | `			/* A ReflectionObject reflects the OBJECT, and a LAZY property php does` |
|      - |  5098 | `			 * not DECLARE (DateInterval's ten) is not on one that was never` |
|      - |  5099 | `			 * constructed -- php reports none there either. The declared-and-virtual` |
|      - |  5100 | `			 * kind (DatePeriod's seven, PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT) is` |
|      - |  5101 | `			 * reported whatever the object holds, because php declares it. */` |
|     45 |  5102 | `			continue;` |
|      - |  5103 | `		}` |
|   1784 |  5104 | `		if( iKind == REFLECT_MEMBER_PROP && pObj == 0 && pM->pAttr != 0` |
|    441 |  5105 | `		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND) != 0 ){` |
|      - |  5106 | `			/* An ON-DEMAND property belongs to the objects that took it, so the` |
|      - |  5107 | `			 * CLASS's list -- which php builds from declarations and DateInterval` |
|      - |  5108 | `			 * has none of -- does not gain a name for it. */` |
|    ! 0 |  5109 | `			continue;` |
|      - |  5110 | `		}` |
|   1784 |  5111 | `		if( iKind == REFLECT_MEMBER_PROP && pObj != 0 && pM->pAttr != 0` |
|     98 |  5112 | `		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY) != 0` |
|    101 |  5113 | `		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT) == 0 ){` |
|      - |  5114 | `			/* ...and for the same reason a property the object HIDES or never took` |
|      - |  5115 | `			 * is not on it either: php's from-string DateInterval reflects the two` |
|      - |  5116 | ``			 * names it presents, and an ordinary one has no `date_string` at all. */`` |
|    100 |  5117 | `			SyHashEntry *pOwn = SyHashGet(&pObj->hAttr,` |
|     66 |  5118 | `				SyStringData(&pM->pAttr->sName),SyStringLength(&pM->pAttr->sName));` |
|     66 |  5119 | `			if( pOwn == 0` |
|     65 |  5120 | `			 \|\| (((VmClassAttr *)pOwn->pUserData)->iState & VM_CLASS_ATTR_UNSEEN) ){` |
|     23 |  5121 | `				continue;` |
|      - |  5122 | `			}` |
|     22 |  5123 | `		}` |
|   1767 |  5124 | `		if( bFilter ){` |
|     33 |  5125 | `			sxi64 iMods = iKind == REFLECT_MEMBER_METHOD ? ReflectMethodModifiers(pM->pMeth)` |
|     40 |  5126 | `				: (iKind == REFLECT_MEMBER_CONST ? ReflectConstModifiers(pM->pAttr)` |
|     20 |  5127 | `				                                 : ReflectPropModifiers(pM->pAttr));` |
|     33 |  5128 | `			if( (iMods & iFilter) == 0 ){` |
|     21 |  5129 | `				continue;` |
|      - |  5130 | `			}` |
|      6 |  5131 | `		}` |
|   1747 |  5132 | `		PH7_MemObjInit(pCtx->pVm, &sName);` |
|   1747 |  5133 | `		ph7_value_string(&sName, SyStringData(&pM->sKey), (int)SyStringLength(&pM->sKey));` |
|   1747 |  5134 | `		apCtor[0] = &sSelf;` |
|   1747 |  5135 | `		apCtor[1] = &sName;` |
|   1747 |  5136 | `		pCtx->pVm->nReflectFactory++;   /* see ph7_vm::nReflectFactory */` |
|   1747 |  5137 | `		pRef = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|   1747 |  5138 | `		pCtx->pVm->nReflectFactory--;` |
|   1747 |  5139 | `		PH7_MemObjRelease(&sName);` |
|   1747 |  5140 | `		if( pRef == 0 ){` |
|    ! 0 |  5141 | `			SySetRelease(&aMembers);` |
|    ! 0 |  5142 | `			PH7_MemObjRelease(&sSelf);` |
|    ! 0 |  5143 | `			if( rc != PH7_OK ){` |
|    ! 0 |  5144 | `				return rc;` |
|      - |  5145 | `			}` |
|    ! 0 |  5146 | `			ph7_result_value(pCtx, pOut);` |
|    ! 0 |  5147 | `			return PH7_OK;` |
|      - |  5148 | `		}` |
|   1747 |  5149 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|   1747 |  5150 | `		sVal.x.pOther = pRef;` |
|   1747 |  5151 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|   1747 |  5152 | `		ph7_array_add_elem(pOut, 0, &sVal);   /* takes its own reference */` |
|   1747 |  5153 | `		PH7_ClassInstanceUnref(pRef);` |
|    876 |  5154 | `	}` |
|    325 |  5155 | `	SySetRelease(&aMembers);` |
|      - |  5156 | `	/* A ReflectionObject also reports the instance's own DYNAMIC properties,` |
|      - |  5157 | `	 * which no class declaration knows about. */` |
|    325 |  5158 | `	if( iKind == REFLECT_MEMBER_PROP && pObj != 0 && (!bFilter \|\| (iFilter & 1)) ){` |
|      - |  5159 | `		SyHashEntry *pEntry;` |
|      - |  5160 | `		ph7_value sTarget;` |
|     17 |  5161 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     17 |  5162 | `		sTarget.x.pOther = pObj;` |
|     17 |  5163 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|     17 |  5164 | `		SyHashResetLoopCursor(&pObj->hAttr);` |
|    111 |  5165 | `		while( (pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){` |
|     95 |  5166 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - |  5167 | `			ph7_value sName, sVal;` |
|      - |  5168 | `			ph7_value *apCtor[2];` |
|      - |  5169 | `			ph7_class_instance *pRef;` |
|      - |  5170 | `			sxi32 rc;` |
|     95 |  5171 | `			if( pVmAttr->pAttr == 0 \|\| (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) == 0 ){` |
|     91 |  5172 | `				continue;` |
|      - |  5173 | `			}` |
|      5 |  5174 | `			PH7_MemObjInit(pCtx->pVm, &sName);` |
|      7 |  5175 | `			ph7_value_string(&sName, SyStringData(&pVmAttr->pAttr->sName),` |
|      4 |  5176 | `				(int)SyStringLength(&pVmAttr->pAttr->sName));` |
|      5 |  5177 | `			apCtor[0] = &sTarget;` |
|      5 |  5178 | `			apCtor[1] = &sName;` |
|      5 |  5179 | `			pRef = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|      5 |  5180 | `			PH7_MemObjRelease(&sName);` |
|      5 |  5181 | `			if( pRef == 0 ){` |
|    ! 0 |  5182 | `				break;` |
|      - |  5183 | `			}` |
|      5 |  5184 | `			PH7_MemObjInit(pCtx->pVm, &sVal);` |
|      5 |  5185 | `			sVal.x.pOther = pRef;` |
|      5 |  5186 | `			sVal.iFlags = MEMOBJ_OBJ;` |
|      5 |  5187 | `			ph7_array_add_elem(pOut, 0, &sVal);` |
|      5 |  5188 | `			PH7_ClassInstanceUnref(pRef);` |
|      1 |  5189 | `		}` |
|      - |  5190 | `		/* sTarget borrows pObj and never took a reference: nothing to release. */` |
|      8 |  5191 | `	}` |
|    325 |  5192 | `	PH7_MemObjRelease(&sSelf);` |
|    325 |  5193 | `	ph7_result_value(pCtx, pOut);` |
|    325 |  5194 | `	return PH7_OK;` |
|    165 |  5195 | `}` |
|    146 |  5196 | `static int vm_builtin_ReflectionClass_getMethods(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  5197 | `{` |
|    150 |  5198 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_METHOD, "ReflectionMethod", nArg, apArg);` |
|      4 |  5199 | `}` |
|    168 |  5200 | `static int vm_builtin_ReflectionClass_getProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  5201 | `{` |
|    173 |  5202 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_PROP, "ReflectionProperty", nArg, apArg);` |
|      5 |  5203 | `}` |
|      6 |  5204 | `static int vm_builtin_ReflectionClass_getReflectionConstants(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  5205 | `{` |
|      7 |  5206 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_CONST, "ReflectionClassConstant", nArg, apArg);` |
|      1 |  5207 | `}` |
|     14 |  5208 | `static int vm_builtin_ReflectionClass_getProperty(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  5209 | `{` |
|     16 |  5210 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     16 |  5211 | `	ph7_class_instance *pObj = ReflectClassObj(pCtx);` |
|      - |  5212 | `	const char *zName;` |
|     16 |  5213 | `	int nName, bFound = 0;` |
|      - |  5214 | `	SySet aMembers;` |
|      - |  5215 | `	sxu32 n;` |
|      - |  5216 | `	SyString sFound;` |
|     16 |  5217 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  5218 | `		ph7_result_null(pCtx);` |
|    ! 0 |  5219 | `		return PH7_OK;` |
|      - |  5220 | `	}` |
|     16 |  5221 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     16 |  5222 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     16 |  5223 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     90 |  5224 | `	for( n = 0 ; n < SySetUsed(&aMembers) && !bFound ; n++ ){` |
|     76 |  5225 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     76 |  5226 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zName, nName) ){` |
|     10 |  5227 | `			sFound = pM->sKey;` |
|     10 |  5228 | `			bFound = 1;` |
|      4 |  5229 | `		}` |
|     39 |  5230 | `	}` |
|     16 |  5231 | `	SySetRelease(&aMembers);` |
|     16 |  5232 | `	if( bFound ){` |
|      - |  5233 | `		ph7_value sSelf;` |
|      - |  5234 | `		int rc;` |
|     10 |  5235 | `		ReflectSelfName(pCtx, &sSelf);` |
|     10 |  5236 | `		rc = ReflectResultMember(pCtx, "ReflectionProperty", &sSelf, &sFound);` |
|     10 |  5237 | `		PH7_MemObjRelease(&sSelf);` |
|     10 |  5238 | `		return rc;` |
|      - |  5239 | `	}` |
|      7 |  5240 | `	if( ReflectObjHasProp(pObj, zName, nName) ){` |
|      - |  5241 | `		/* A dynamic property: the reflector is built over the OBJECT, since the` |
|      - |  5242 | `		 * class declaration has no record of it. */` |
|      - |  5243 | `		ph7_value sTarget;` |
|      - |  5244 | `		SyString sName;` |
|      - |  5245 | `		int rc;` |
|      3 |  5246 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      3 |  5247 | `		sTarget.x.pOther = pObj;` |
|      3 |  5248 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|      3 |  5249 | `		SyStringInitFromBuf(&sName, zName, nName);` |
|      3 |  5250 | `		rc = ReflectResultMember(pCtx, "ReflectionProperty", &sTarget, &sName);` |
|      3 |  5251 | `		return rc;` |
|      - |  5252 | `	}` |
|      7 |  5253 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 |  5254 | `		"Property %z::$%.*s does not exist", &pClass->sDisp, nName, zName);` |
|      9 |  5255 | `}` |
|     10 |  5256 | `static int vm_builtin_ReflectionClass_getReflectionConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  5257 | `{` |
|     11 |  5258 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  5259 | `	const char *zName;` |
|     11 |  5260 | `	int nName, bFound = 0;` |
|      - |  5261 | `	SySet aMembers;` |
|      - |  5262 | `	sxu32 n;` |
|      - |  5263 | `	SyString sFound;` |
|     11 |  5264 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  5265 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  5266 | `		return PH7_OK;` |
|      - |  5267 | `	}` |
|     11 |  5268 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     11 |  5269 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     11 |  5270 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     33 |  5271 | `	for( n = 0 ; n < SySetUsed(&aMembers) && !bFound ; n++ ){` |
|     23 |  5272 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     23 |  5273 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|      9 |  5274 | `			sFound = pM->sKey;` |
|      9 |  5275 | `			bFound = 1;` |
|      4 |  5276 | `		}` |
|     12 |  5277 | `	}` |
|     11 |  5278 | `	SySetRelease(&aMembers);` |
|     11 |  5279 | `	if( !bFound ){` |
|      3 |  5280 | `		ph7_result_bool(pCtx, 0);` |
|      3 |  5281 | `		return PH7_OK;` |
|      - |  5282 | `	}` |
|      - |  5283 | `	{` |
|      - |  5284 | `		ph7_value sSelf;` |
|      - |  5285 | `		int rc;` |
|      9 |  5286 | `		ReflectSelfName(pCtx, &sSelf);` |
|      9 |  5287 | `		rc = ReflectResultMember(pCtx, "ReflectionClassConstant", &sSelf, &sFound);` |
|      9 |  5288 | `		PH7_MemObjRelease(&sSelf);` |
|      9 |  5289 | `		return rc;` |
|      - |  5290 | `	}` |
|      6 |  5291 | `}` |
|      - |  5292 | `/* ---- statics and defaults ---- */` |
|      - |  5293 | `/* Reading or writing a static through reflection materializes the class's` |
|      - |  5294 | `` * static table exactly as `C::$s` does, so a default that threw at the`` |
|      - |  5295 | ` * declaration raises HERE. */` |
|     62 |  5296 | `static sxi32 ReflectMaterializeStatics(ph7_context *pCtx, ph7_class *pClass)` |
|      3 |  5297 | `{` |
|     65 |  5298 | `	if( VmClassStaticDeferPending(pClass) ){` |
|     29 |  5299 | `		return PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);` |
|      - |  5300 | `	}` |
|     39 |  5301 | `	return SXRET_OK;` |
|     34 |  5302 | `}` |
|      8 |  5303 | `static int vm_builtin_ReflectionClass_getStaticProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  5304 | `{` |
|     10 |  5305 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     10 |  5306 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  5307 | `	SySet aMembers;` |
|      - |  5308 | `	sxu32 n;` |
|      4 |  5309 | `	SXUNUSED(nArg);` |
|      4 |  5310 | `	SXUNUSED(apArg);` |
|     10 |  5311 | `	if( pOut == 0 ){` |
|    ! 0 |  5312 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  5313 | `	}` |
|     10 |  5314 | `	if( pClass == 0 ){` |
|    ! 0 |  5315 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  5316 | `		return PH7_OK;` |
|      - |  5317 | `	}` |
|      - |  5318 | `	{` |
|     10 |  5319 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|     10 |  5320 | `		if( rc != SXRET_OK ){` |
|      3 |  5321 | `			return rc;` |
|      - |  5322 | `		}` |
|      - |  5323 | `	}` |
|      7 |  5324 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|      7 |  5325 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     39 |  5326 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     33 |  5327 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - |  5328 | `		ph7_value *pVal;` |
|      - |  5329 | `		SyHashEntry *pSlot;` |
|     32 |  5330 | `		if( pM->iKind != REFLECT_MEMBER_PROP` |
|     29 |  5331 | `		 \|\| (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|     21 |  5332 | `			continue;` |
|      - |  5333 | `		}` |
|      - |  5334 | `		/* An UNINITIALIZED typed static has no value to report, and php simply` |
|      - |  5335 | `		 * leaves it out rather than raising the read Error here. */` |
|     13 |  5336 | `		pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pM->pAttr->nIdx, sizeof(sxu32));` |
|     13 |  5337 | `		if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|      3 |  5338 | `			continue;` |
|      - |  5339 | `		}` |
|     11 |  5340 | `		pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pM->pAttr->nIdx);` |
|     11 |  5341 | `		if( pVal ){` |
|     11 |  5342 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, pVal);` |
|      5 |  5343 | `		}` |
|      6 |  5344 | `	}` |
|      7 |  5345 | `	SySetRelease(&aMembers);` |
|      7 |  5346 | `	ph7_result_value(pCtx, pOut);` |
|      7 |  5347 | `	return PH7_OK;` |
|      6 |  5348 | `}` |
|      - |  5349 | `/* The declared STATIC property of this name, or NULL. */` |
|     20 |  5350 | `static ph7_class_attr * ReflectStaticAttr(ph7_context *pCtx, ph7_class *pClass,` |
|      - |  5351 | `	const char *zName, int nName)` |
|      3 |  5352 | `{` |
|      - |  5353 | `	SyHashEntry *pEntry;` |
|      - |  5354 | `	ph7_class_attr *pAttr;` |
|     23 |  5355 | `	if( nName < 1 ){` |
|    ! 0 |  5356 | `		return 0;` |
|      - |  5357 | `	}` |
|     23 |  5358 | `	pEntry = SyHashGet(&pClass->hAttr, (const void *)zName, (sxu32)nName);` |
|     23 |  5359 | `	if( pEntry == 0 ){` |
|      9 |  5360 | `		return 0;` |
|      - |  5361 | `	}` |
|     15 |  5362 | `	pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      6 |  5363 | `	SXUNUSED(pCtx);` |
|     15 |  5364 | `	return (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? pAttr : 0;` |
|     13 |  5365 | `}` |
|     22 |  5366 | `static int vm_builtin_ReflectionClass_getStaticPropertyValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  5367 | `{` |
|     25 |  5368 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  5369 | `	ph7_class_attr *pAttr;` |
|      - |  5370 | `	const char *zName;` |
|      - |  5371 | `	int nName;` |
|     25 |  5372 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  5373 | `		ph7_result_null(pCtx);` |
|    ! 0 |  5374 | `		return PH7_OK;` |
|      - |  5375 | `	}` |
|      - |  5376 | `	{` |
|     25 |  5377 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|     25 |  5378 | `		if( rc != SXRET_OK ){` |
|      7 |  5379 | `			return rc;` |
|      - |  5380 | `		}` |
|      - |  5381 | `	}` |
|     19 |  5382 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     19 |  5383 | `	pAttr = ReflectStaticAttr(pCtx, pClass, zName, nName);` |
|     19 |  5384 | `	if( pAttr == 0 ){` |
|      7 |  5385 | `		if( nArg > 1 ){` |
|      5 |  5386 | `			ph7_result_value(pCtx, apArg[1]);` |
|      5 |  5387 | `			return PH7_OK;` |
|      - |  5388 | `		}` |
|      4 |  5389 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  5390 | `			"Property %z::$%.*s does not exist", &pClass->sDisp, nName, zName);` |
|      - |  5391 | `	}` |
|      - |  5392 | `	{` |
|      - |  5393 | `		/* Uninitialized typed static: the same Error the VM raises on read */` |
|     13 |  5394 | `		SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|      - |  5395 | `		ph7_value *pVal;` |
|     13 |  5396 | `		if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|      - |  5397 | `			/* php says "Typed property" HERE and "Typed static property" from` |
|      - |  5398 | ``			 * ReflectionProperty::getValue() and from `A::$s` itself — the`` |
|      - |  5399 | `			 * three wordings were checked against the oracle, they really do` |
|      - |  5400 | `			 * differ by call site. */` |
|      3 |  5401 | `			ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|      4 |  5402 | `			return PH7_VmThrowException(pCtx, "Error",` |
|      - |  5403 | `				"Typed property %z::$%z must not be accessed before initialization",` |
|      1 |  5404 | `				&pDecl->sDisp, &pAttr->sName);` |
|      - |  5405 | `		}` |
|     11 |  5406 | `		pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|     11 |  5407 | `		if( pVal ){` |
|     11 |  5408 | `			ph7_result_value(pCtx, pVal);` |
|      7 |  5409 | `		}else{` |
|    ! 0 |  5410 | `			ph7_result_null(pCtx);` |
|      - |  5411 | `		}` |
|      - |  5412 | `	}` |
|     11 |  5413 | `	return PH7_OK;` |
|     14 |  5414 | `}` |
|      8 |  5415 | `static int vm_builtin_ReflectionClass_setStaticPropertyValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  5416 | `{` |
|     10 |  5417 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  5418 | `	ph7_class_attr *pAttr;` |
|      - |  5419 | `	ph7_value *pSlot;` |
|      - |  5420 | `	const char *zName;` |
|      - |  5421 | `	int nName;` |
|     10 |  5422 | `	if( pClass == 0 \|\| nArg < 2 ){` |
|    ! 0 |  5423 | `		return PH7_OK;` |
|      - |  5424 | `	}` |
|      - |  5425 | `	{` |
|     10 |  5426 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|     10 |  5427 | `		if( rc != SXRET_OK ){` |
|      5 |  5428 | `			return rc;` |
|      - |  5429 | `		}` |
|      - |  5430 | `	}` |
|      5 |  5431 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|      5 |  5432 | `	pAttr = ReflectStaticAttr(pCtx, pClass, zName, nName);` |
|      5 |  5433 | `	if( pAttr == 0 ){` |
|      4 |  5434 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  5435 | `			"Class %z does not have a property named %.*s", &pClass->sDisp, nName, zName);` |
|      - |  5436 | `	}` |
|      3 |  5437 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      3 |  5438 | `	if( pSlot == 0 ){` |
|    ! 0 |  5439 | `		return PH7_OK;` |
|      - |  5440 | `	}` |
|      - |  5441 | `	{` |
|      3 |  5442 | `		sxi32 rc = ReflectEnforceStore(pCtx, pAttr->nIdx, apArg[1]);` |
|      3 |  5443 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  5444 | `			return rc;` |
|      - |  5445 | `		}` |
|      - |  5446 | `	}` |
|      3 |  5447 | `	PH7_MemObjStore(apArg[1], pSlot);` |
|      3 |  5448 | `	return PH7_OK;` |
|      6 |  5449 | `}` |
|      8 |  5450 | `static int vm_builtin_ReflectionClass_getDefaultProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  5451 | `{` |
|     10 |  5452 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     10 |  5453 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  5454 | `	SySet aMembers;` |
|      - |  5455 | `	sxu32 n;` |
|      - |  5456 | `	int iPass;` |
|      4 |  5457 | `	SXUNUSED(nArg);` |
|      4 |  5458 | `	SXUNUSED(apArg);` |
|     10 |  5459 | `	if( pOut == 0 ){` |
|    ! 0 |  5460 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  5461 | `	}` |
|     10 |  5462 | `	if( pClass == 0 ){` |
|    ! 0 |  5463 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  5464 | `		return PH7_OK;` |
|      - |  5465 | `	}` |
|      - |  5466 | `	{` |
|      - |  5467 | `		/* Listing the defaults resolves the class, as get_class_vars() does. */` |
|     10 |  5468 | `		sxi32 rcMat = ReflectMaterializeStatics(pCtx, pClass);` |
|     10 |  5469 | `		if( rcMat != SXRET_OK ){` |
|      3 |  5470 | `			return rcMat;` |
|      - |  5471 | `		}` |
|      - |  5472 | `	}` |
|      8 |  5473 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|      8 |  5474 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|      - |  5475 | `	/* php reports the STATIC properties first, then the instance ones. */` |
|     20 |  5476 | `	for( iPass = 0 ; iPass < 2 ; iPass++ ){` |
|     66 |  5477 | `		for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     54 |  5478 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - |  5479 | `			int bStatic;` |
|      - |  5480 | `			ph7_value sValue;` |
|     54 |  5481 | `			if( pM->iKind != REFLECT_MEMBER_PROP ){` |
|     19 |  5482 | `				continue;` |
|      - |  5483 | `			}` |
|     50 |  5484 | `			bStatic = (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|     50 |  5485 | `			if( bStatic != (iPass == 0) ){` |
|     26 |  5486 | `				continue;` |
|      - |  5487 | `			}` |
|     24 |  5488 | `			if( (pM->pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|     19 |  5489 | `			 && SySetUsed(&pM->pAttr->aByteCode) < 1 ){` |
|      - |  5490 | `				/* A TYPED property with no initializer has no default at all —` |
|      - |  5491 | `				 * it is uninitialized, and php leaves it out. An UNTYPED one` |
|      - |  5492 | `				 * without an initializer defaults to null and is listed. */` |
|      5 |  5493 | `				continue;` |
|      - |  5494 | `			}` |
|     22 |  5495 | `			PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     22 |  5496 | `			if( SySetUsed(&pM->pAttr->aByteCode) > 0 ){` |
|      - |  5497 | `				/* Same evaluation path the VM uses for omitted call arguments */` |
|     18 |  5498 | `				VmLocalExec(pCtx->pVm, &pM->pAttr->aByteCode, &sValue, FALSE);` |
|     18 |  5499 | `				PH7_VmResolvedDefault(pCtx->pVm, pClass, pM->pAttr, &sValue);` |
|      8 |  5500 | `			}` |
|     22 |  5501 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, &sValue);` |
|     22 |  5502 | `			PH7_MemObjRelease(&sValue);` |
|     12 |  5503 | `		}` |
|      8 |  5504 | `	}` |
|      8 |  5505 | `	SySetRelease(&aMembers);` |
|      8 |  5506 | `	ph7_result_value(pCtx, pOut);` |
|      8 |  5507 | `	return PH7_OK;` |
|      6 |  5508 | `}` |
|      - |  5509 | `/* ---- attributes, extension, export ---- */` |
|     82 |  5510 | `static int vm_builtin_ReflectionClass_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  5511 | `{` |
|     86 |  5512 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  5513 | `	const char *zName;` |
|      - |  5514 | `	int nName;` |
|     86 |  5515 | `	if( pClass == 0 ){` |
|    ! 0 |  5516 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  5517 | `		return PH7_OK;` |
|      - |  5518 | `	}` |
|     86 |  5519 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      - |  5520 | `	{` |
|      - |  5521 | `		ph7_value sTarget;` |
|      - |  5522 | `		int rc;` |
|     86 |  5523 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     86 |  5524 | `		ph7_value_string(&sTarget, zName, nName);` |
|      - |  5525 | `		/* 1 = Attribute::TARGET_CLASS */` |
|    127 |  5526 | `		rc = ReflectBuildAttrs(pCtx, &pClass->aAttrs, "class", &sTarget, 0, 0, 0, 1,` |
|     41 |  5527 | `			nArg, apArg);` |
|     86 |  5528 | `		PH7_MemObjRelease(&sTarget);` |
|     86 |  5529 | `		return rc;` |
|      - |  5530 | `	}` |
|     45 |  5531 | `}` |
|      - |  5532 | `/*` |
|      - |  5533 | ` * The ReflectionExtension for one extension id, which is what every reflector's` |
|      - |  5534 | ` * getExtension() answers for an INTERNAL target. A target the partition has no` |
|      - |  5535 | ` * row for (iExt < 0) belongs to no extension, which is php's null.` |
|      - |  5536 | ` */` |
|     24 |  5537 | `static int ReflectExtensionOf(ph7_context *pCtx, int iExt)` |
|      2 |  5538 | `{` |
|      - |  5539 | `	ph7_value sName;` |
|      - |  5540 | `	ph7_value *apCtor[1];` |
|      - |  5541 | `	ph7_class_instance *pExt;` |
|      - |  5542 | `	const char *zName;` |
|      - |  5543 | `	sxi32 rc;` |
|     26 |  5544 | `	if( iExt < 0 ){` |
|     10 |  5545 | `		ph7_result_null(pCtx);` |
|     10 |  5546 | `		return PH7_OK;` |
|      - |  5547 | `	}` |
|     17 |  5548 | `	zName = PH7_VmExtensionName(iExt);` |
|     17 |  5549 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|     17 |  5550 | `	ph7_value_string(&sName, zName, -1);` |
|     17 |  5551 | `	apCtor[0] = &sName;` |
|     17 |  5552 | `	pExt = ReflectConstruct(pCtx, "ReflectionExtension", 1, apCtor, &rc);` |
|     17 |  5553 | `	PH7_MemObjRelease(&sName);` |
|     17 |  5554 | `	if( pExt == 0 ){` |
|    ! 0 |  5555 | `		if( rc != PH7_OK ){` |
|    ! 0 |  5556 | `			return rc;` |
|      - |  5557 | `		}` |
|    ! 0 |  5558 | `		ph7_result_null(pCtx);` |
|    ! 0 |  5559 | `		return PH7_OK;` |
|      - |  5560 | `	}` |
|     17 |  5561 | `	return ReflectResultObject(pCtx, pExt);` |
|     14 |  5562 | `}` |
|      - |  5563 | `/*` |
|      - |  5564 | ` * The extension a reflected CLASS belongs to, or -1 for a userland one. A class` |
|      - |  5565 | ` * php has no row for keeps php's answer for an internal name it cannot place --` |
|      - |  5566 | ` * Core, the engine's own.` |
|      - |  5567 | ` */` |
|   4724 |  5568 | `static int ReflectClassExtId(ph7_class *pClass)` |
|      3 |  5569 | `{` |
|   4727 |  5570 | `	if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_INTERNAL) == 0 ){` |
|     35 |  5571 | `		return -1;` |
|      - |  5572 | `	}` |
|   7038 |  5573 | `	return PH7_VmExtOfClass(SyStringData(&pClass->sName),` |
|   4690 |  5574 | `		(int)SyStringLength(&pClass->sName));` |
|   2365 |  5575 | `}` |
|    126 |  5576 | `static int vm_builtin_ReflectionClass_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  5577 | `{` |
|    129 |  5578 | `	int iExt = ReflectClassExtId(ReflectClassOf(pCtx));` |
|     63 |  5579 | `	SXUNUSED(nArg);` |
|     63 |  5580 | `	SXUNUSED(apArg);` |
|    129 |  5581 | `	if( iExt < 0 ){` |
|      3 |  5582 | `		ph7_result_bool(pCtx, 0);` |
|      2 |  5583 | `	}else{` |
|    127 |  5584 | `		ph7_result_string(pCtx, PH7_VmExtensionName(iExt), -1);` |
|      - |  5585 | `	}` |
|    129 |  5586 | `	return PH7_OK;` |
|      3 |  5587 | `}` |
|      - |  5588 | `/* php's export format (chunk 9) — defined at the END of this file, where the` |
|      - |  5589 | ` * member walk, the function reference and the parameter description it reads` |
|      - |  5590 | ` * are all in scope. */` |
|      - |  5591 | `static int ReflectExportClassSelf(ph7_context *pCtx);` |
|      - |  5592 | `static int ReflectExportFuncSelf(ph7_context *pCtx);` |
|      - |  5593 | `static int ReflectExportParamSelf(ph7_context *pCtx);` |
|      - |  5594 | `static int ReflectExportPropSelf(ph7_context *pCtx);` |
|      - |  5595 | `static int ReflectExportConstSelf(ph7_context *pCtx);` |
|      - |  5596 | `/*` |
|      - |  5597 | ` * __toString(): php's export format, still chunk 9 — a PHP function written` |
|      - |  5598 | `` * against the PUBLIC reflection API of its target, so it is called with `$this`.`` |
|      - |  5599 | ` * bIndentArg adds the export family's second "" indent argument.` |
|      - |  5600 | ` */` |
|      6 |  5601 | `static int vm_builtin_ReflectionClass_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  5602 | `{` |
|      3 |  5603 | `	SXUNUSED(nArg);` |
|      3 |  5604 | `	SXUNUSED(apArg);` |
|      7 |  5605 | `	return ReflectExtensionOf(pCtx, ReflectClassExtId(ReflectClassOf(pCtx)));` |
|      1 |  5606 | `}` |
|     60 |  5607 | `static int vm_builtin_ReflectionClass_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  5608 | `{` |
|     30 |  5609 | `	SXUNUSED(nArg);` |
|     30 |  5610 | `	SXUNUSED(apArg);` |
|     62 |  5611 | `	return ReflectExportClassSelf(pCtx);` |
|      2 |  5612 | `}` |
|      - |  5613 | `/* ---- lazy objects: PHL has none (recorded) ---- */` |
|    ! 0 |  5614 | `static int ReflectNoLazy(ph7_context *pCtx, const char *zWho)` |
|    ! 0 |  5615 | `{` |
|    ! 0 |  5616 | `	return PH7_VmThrowException(pCtx, "Error",` |
|    ! 0 |  5617 | `		"%s is not supported by PHL (no lazy objects)", zWho);` |
|    ! 0 |  5618 | `}` |
|      - |  5619 | `#define REFLECT_NO_LAZY(NAME,TEXT) \` |
|      - |  5620 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  5621 | `	{ \` |
|      - |  5622 | `		SXUNUSED(nArg); \` |
|      - |  5623 | `		SXUNUSED(apArg); \` |
|      - |  5624 | `		return ReflectNoLazy(pCtx, TEXT); \` |
|      - |  5625 | `	}` |
|    ! 0 |  5626 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_newLazyGhost,"ReflectionClass::newLazyGhost()")` |
|    ! 0 |  5627 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_newLazyProxy,"ReflectionClass::newLazyProxy()")` |
|    ! 0 |  5628 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_resetAsLazyGhost,"ReflectionClass::resetAsLazyGhost()")` |
|    ! 0 |  5629 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_resetAsLazyProxy,"ReflectionClass::resetAsLazyProxy()")` |
|      - |  5630 | `/* The four QUERIES answer what is true of a VM with no lazy objects, rather` |
|      - |  5631 | ` * than refusing: an object here is always initialized and never has one. */` |
|    ! 0 |  5632 | `static int vm_builtin_ReflectionClass_getLazyInitializer(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  5633 | `{` |
|    ! 0 |  5634 | `	SXUNUSED(nArg);` |
|    ! 0 |  5635 | `	SXUNUSED(apArg);` |
|    ! 0 |  5636 | `	ph7_result_null(pCtx);` |
|    ! 0 |  5637 | `	return PH7_OK;` |
|    ! 0 |  5638 | `}` |
|    ! 0 |  5639 | `static int vm_builtin_ReflectionClass_passThroughObject(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  5640 | `{` |
|    ! 0 |  5641 | `	if( nArg > 0 ){` |
|    ! 0 |  5642 | `		ph7_result_value(pCtx, apArg[0]);` |
|    ! 0 |  5643 | `	}else{` |
|    ! 0 |  5644 | `		ph7_result_null(pCtx);` |
|      - |  5645 | `	}` |
|    ! 0 |  5646 | `	return PH7_OK;` |
|    ! 0 |  5647 | `}` |
|    ! 0 |  5648 | `static int vm_builtin_ReflectionClass_isUninitializedLazyObject(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  5649 | `{` |
|    ! 0 |  5650 | `	SXUNUSED(nArg);` |
|    ! 0 |  5651 | `	SXUNUSED(apArg);` |
|    ! 0 |  5652 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 |  5653 | `	return PH7_OK;` |
|    ! 0 |  5654 | `}` |
|      - |  5655 | `/*` |
|      - |  5656 | ` * Reflection::getModifierNames(int $modifiers)` |
|      - |  5657 | ` *` |
|      - |  5658 | ` * php's own order and its own SWITCH: the visibility names come out of one` |
|      - |  5659 | ` * three-way choice and the set-visibility names out of another, so a mask with` |
|      - |  5660 | ` * two visibility bits set names NEITHER (which is what php answers).` |
|      - |  5661 | ` */` |
|    192 |  5662 | `static int vm_builtin_Reflection_getModifierNames(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  5663 | `{` |
|    195 |  5664 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|    195 |  5665 | `	sxi64 iMods = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|      - |  5666 | `	const char *azName[8];` |
|    195 |  5667 | `	int nName = 0, n;` |
|    195 |  5668 | `	if( pOut == 0 ){` |
|    ! 0 |  5669 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  5670 | `	}` |
|    195 |  5671 | `	if( iMods & 64 ){ azName[nName++] = "abstract"; }` |
|    195 |  5672 | `	if( iMods & 32 ){ azName[nName++] = "final"; }` |
|    195 |  5673 | `	if( iMods & 512 ){ azName[nName++] = "virtual"; }` |
|    195 |  5674 | `	switch( iMods & (1\|2\|4) ){` |
|    151 |  5675 | `	case 1: azName[nName++] = "public"; break;` |
|     21 |  5676 | `	case 2: azName[nName++] = "protected"; break;` |
|     18 |  5677 | `	case 4: azName[nName++] = "private"; break;` |
|      8 |  5678 | `	default: break;` |
|      - |  5679 | `	}` |
|    195 |  5680 | `	switch( iMods & (2048\|4096) ){` |
|     18 |  5681 | `	case 2048: azName[nName++] = "protected(set)"; break;` |
|      9 |  5682 | `	case 4096: azName[nName++] = "private(set)"; break;` |
|    168 |  5683 | `	default: break;` |
|      - |  5684 | `	}` |
|    195 |  5685 | `	if( iMods & 16 ){ azName[nName++] = "static"; }` |
|    195 |  5686 | `	if( iMods & 128 ){ azName[nName++] = "readonly"; }` |
|    479 |  5687 | `	for( n = 0 ; n < nName ; n++ ){` |
|    287 |  5688 | `		ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|    287 |  5689 | `		if( pName == 0 ){ break; }` |
|    287 |  5690 | `		ph7_value_string(pName, azName[n], -1);` |
|    287 |  5691 | `		ph7_array_add_elem(pOut, 0, pName);` |
|    145 |  5692 | `	}` |
|    195 |  5693 | `	ph7_result_value(pCtx, pOut);` |
|    195 |  5694 | `	return PH7_OK;` |
|     99 |  5695 | `}` |
|      - |  5696 | `/*` |
|      - |  5697 | ` * Declare chunk 1. Called from PH7_VmInstallReflection() where it used to be` |
|      - |  5698 | `` * compiled — before chunks 2 and 3, which name `Reflector` in their own`` |
|      - |  5699 | `` * `implements` clauses.`` |
|      - |  5700 | ` *` |
|      - |  5701 | ` * The method table is in php's own DECLARATION order, which is the order` |
|      - |  5702 | ` * ReflectionClass::getMethods() reports for these classes themselves.` |
|      - |  5703 | ` */` |
|   8445 |  5704 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionClass(ph7_vm *pVm)` |
|      5 |  5705 | `{` |
|      - |  5706 | `	static const PH7_NativePropDef aClassProp[] = {` |
|      - |  5707 | `		{ "name",  PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  5708 | `		/* PHL-only: the instance a ReflectionObject was built over. php keeps it` |
|      - |  5709 | `		 * out of sight; PHL has no hidden-slot bit yet (recorded). */` |
|      - |  5710 | `		{ RC_OBJ,  PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  5711 | `	};` |
|      - |  5712 | `	static const PH7_NativeMethodDef aClassMethod[] = {` |
|      - |  5713 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionClass_clone },` |
|      - |  5714 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $objectOrClass", "",` |
|      - |  5715 | `		  vm_builtin_ReflectionClass_construct },` |
|      - |  5716 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionClass_toString },` |
|      - |  5717 | `		{ "getName",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getName },` |
|      - |  5718 | `		{ "isInternal",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInternal },` |
|      - |  5719 | `		{ "isUserDefined", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isUserDefined },` |
|      - |  5720 | `		{ "isAnonymous",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isAnonymous },` |
|      - |  5721 | `		{ "isInstantiable",PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInstantiable },` |
|      - |  5722 | `		{ "isCloneable",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isCloneable },` |
|      - |  5723 | `		{ "getFileName",   PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getFileName },` |
|      - |  5724 | `		{ "getStartLine",  PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionClass_getStartLine },` |
|      - |  5725 | `		{ "getEndLine",    PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionClass_getEndLine },` |
|      - |  5726 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getDocComment },` |
|      - |  5727 | `		{ "getConstructor",PH7_MOD_PUBLIC, "", "@?ReflectionMethod", vm_builtin_ReflectionClass_getConstructor },` |
|      - |  5728 | `		{ "hasMethod",     PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasMethod },` |
|      - |  5729 | `		{ "getMethod",     PH7_MOD_PUBLIC, "string $name", "@ReflectionMethod", vm_builtin_ReflectionClass_getMethod },` |
|      - |  5730 | `		{ "getMethods",    PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - |  5731 | `		  vm_builtin_ReflectionClass_getMethods },` |
|      - |  5732 | `		{ "hasProperty",   PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasProperty },` |
|      - |  5733 | `		{ "getProperty",   PH7_MOD_PUBLIC, "string $name", "@ReflectionProperty", vm_builtin_ReflectionClass_getProperty },` |
|      - |  5734 | `		{ "getProperties", PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - |  5735 | `		  vm_builtin_ReflectionClass_getProperties },` |
|      - |  5736 | `		{ "hasConstant",   PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasConstant },` |
|      - |  5737 | `		{ "getConstants",  PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - |  5738 | `		  vm_builtin_ReflectionClass_getConstants },` |
|      - |  5739 | `		{ "getReflectionConstants", PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - |  5740 | `		  vm_builtin_ReflectionClass_getReflectionConstants },` |
|      - |  5741 | `		{ "getConstant",   PH7_MOD_PUBLIC, "string $name", "@mixed", vm_builtin_ReflectionClass_getConstant },` |
|      - |  5742 | `		{ "getReflectionConstant", PH7_MOD_PUBLIC, "string $name", "@ReflectionClassConstant\|false",` |
|      - |  5743 | `		  vm_builtin_ReflectionClass_getReflectionConstant },` |
|      - |  5744 | `		{ "getInterfaces",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getInterfaces },` |
|      - |  5745 | `		{ "getInterfaceNames", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getInterfaceNames },` |
|      - |  5746 | `		{ "isInterface",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInterface },` |
|      - |  5747 | `		{ "getTraits",         PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraits },` |
|      - |  5748 | `		{ "getTraitNames",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraitNames },` |
|      - |  5749 | `		{ "getTraitAliases",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraitAliases },` |
|      - |  5750 | `		{ "isTrait",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isTrait },` |
|      - |  5751 | `		{ "isEnum",            PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClass_isEnum },` |
|      - |  5752 | `		{ "isAbstract",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isAbstract },` |
|      - |  5753 | `		{ "isFinal",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isFinal },` |
|      - |  5754 | `		{ "isReadOnly",        PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClass_isReadOnly },` |
|      - |  5755 | `		{ "getModifiers",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionClass_getModifiers },` |
|      - |  5756 | `		{ "isInstance",        PH7_MOD_PUBLIC, "object $object", "@bool",` |
|      - |  5757 | `		  vm_builtin_ReflectionClass_isInstance },` |
|      - |  5758 | `		{ "newInstance",       PH7_MOD_PUBLIC, "mixed ...$args", "@object",` |
|      - |  5759 | `		  vm_builtin_ReflectionClass_newInstance },` |
|      - |  5760 | `		{ "newInstanceWithoutConstructor", PH7_MOD_PUBLIC, "", "@object",` |
|      - |  5761 | `		  vm_builtin_ReflectionClass_newInstanceWithoutConstructor },` |
|      - |  5762 | `		{ "newInstanceArgs",   PH7_MOD_PUBLIC, "array $args = []", "@?object",` |
|      - |  5763 | `		  vm_builtin_ReflectionClass_newInstanceArgs },` |
|      - |  5764 | `		{ "newLazyGhost",      PH7_MOD_PUBLIC, "callable $initializer, int $options = 0", "object",` |
|      - |  5765 | `		  vm_builtin_ReflectionClass_newLazyGhost },` |
|      - |  5766 | `		{ "newLazyProxy",      PH7_MOD_PUBLIC, "callable $factory, int $options = 0", "object",` |
|      - |  5767 | `		  vm_builtin_ReflectionClass_newLazyProxy },` |
|      - |  5768 | `		{ "resetAsLazyGhost",  PH7_MOD_PUBLIC,` |
|      - |  5769 | `		  "object $object, callable $initializer, int $options = 0", "void",` |
|      - |  5770 | `		  vm_builtin_ReflectionClass_resetAsLazyGhost },` |
|      - |  5771 | `		{ "resetAsLazyProxy",  PH7_MOD_PUBLIC,` |
|      - |  5772 | `		  "object $object, callable $factory, int $options = 0", "void",` |
|      - |  5773 | `		  vm_builtin_ReflectionClass_resetAsLazyProxy },` |
|      - |  5774 | `		{ "initializeLazyObject", PH7_MOD_PUBLIC, "object $object", "object",` |
|      - |  5775 | `		  vm_builtin_ReflectionClass_passThroughObject },` |
|      - |  5776 | `		{ "isUninitializedLazyObject", PH7_MOD_PUBLIC, "object $object", "bool",` |
|      - |  5777 | `		  vm_builtin_ReflectionClass_isUninitializedLazyObject },` |
|      - |  5778 | `		{ "markLazyObjectAsInitialized", PH7_MOD_PUBLIC, "object $object", "object",` |
|      - |  5779 | `		  vm_builtin_ReflectionClass_passThroughObject },` |
|      - |  5780 | `		{ "getLazyInitializer", PH7_MOD_PUBLIC, "object $object", "?callable",` |
|      - |  5781 | `		  vm_builtin_ReflectionClass_getLazyInitializer },` |
|      - |  5782 | `		{ "getParentClass",    PH7_MOD_PUBLIC, "", "@ReflectionClass\|false", vm_builtin_ReflectionClass_getParentClass },` |
|      - |  5783 | `		{ "isSubclassOf",      PH7_MOD_PUBLIC, "ReflectionClass\|string $class", "@bool",` |
|      - |  5784 | `		  vm_builtin_ReflectionClass_isSubclassOf },` |
|      - |  5785 | `		{ "getStaticProperties", PH7_MOD_PUBLIC, "", "@array",` |
|      - |  5786 | `		  vm_builtin_ReflectionClass_getStaticProperties },` |
|      - |  5787 | ``		/* `mixed $default = ?` is the table's "optional, no default VALUE"`` |
|      - |  5788 | `		 * marker — php's own shape here: isOptional() true,` |
|      - |  5789 | `		 * isDefaultValueAvailable() false. */` |
|      - |  5790 | `		{ "getStaticPropertyValue", PH7_MOD_PUBLIC, "string $name, mixed $default = ?", "@mixed",` |
|      - |  5791 | `		  vm_builtin_ReflectionClass_getStaticPropertyValue },` |
|      - |  5792 | `		{ "setStaticPropertyValue", PH7_MOD_PUBLIC, "string $name, mixed $value", "@void",` |
|      - |  5793 | `		  vm_builtin_ReflectionClass_setStaticPropertyValue },` |
|      - |  5794 | `		{ "getDefaultProperties", PH7_MOD_PUBLIC, "", "@array",` |
|      - |  5795 | `		  vm_builtin_ReflectionClass_getDefaultProperties },` |
|      - |  5796 | `		{ "isIterable",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isIterable },` |
|      - |  5797 | `		{ "isIterateable",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isIterable },` |
|      - |  5798 | `		{ "implementsInterface", PH7_MOD_PUBLIC, "ReflectionClass\|string $interface", "@bool",` |
|      - |  5799 | `		  vm_builtin_ReflectionClass_implementsInterface },` |
|      - |  5800 | `		{ "getExtension",      PH7_MOD_PUBLIC, "", "@?ReflectionExtension", vm_builtin_ReflectionClass_getExtension },` |
|      - |  5801 | `		{ "getExtensionName",  PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getExtensionName },` |
|      - |  5802 | `		{ "inNamespace",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_inNamespace },` |
|      - |  5803 | `		{ "getNamespaceName",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getNamespaceName },` |
|      - |  5804 | `		{ "getShortName",      PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getShortName },` |
|      - |  5805 | `		{ "getAttributes",     PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - |  5806 | `		  vm_builtin_ReflectionClass_getAttributes },` |
|      - |  5807 | `	};` |
|      - |  5808 | `	static const PH7_NativeConstDef aClassConst[] = {` |
|      - |  5809 | `		{ "IS_IMPLICIT_ABSTRACT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,    0, 0.0 },` |
|      - |  5810 | `		{ "IS_EXPLICIT_ABSTRACT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64,    0, 0.0 },` |
|      - |  5811 | `		{ "IS_FINAL",             PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32,    0, 0.0 },` |
|      - |  5812 | `		{ "IS_READONLY",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 65536, 0, 0.0 },` |
|      - |  5813 | `		{ "SKIP_INITIALIZATION_ON_SERIALIZE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 8, 0, 0.0 },` |
|      - |  5814 | `		{ "SKIP_DESTRUCTOR",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,    0, 0.0 },` |
|      - |  5815 | `	};` |
|      - |  5816 | `	static const PH7_NativeMethodDef aObjectMethod[] = {` |
|      - |  5817 | `		{ "__construct", PH7_MOD_PUBLIC, "object $object", "",` |
|      - |  5818 | `		  vm_builtin_ReflectionObject_construct },` |
|      - |  5819 | `	};` |
|      - |  5820 | `	static const PH7_NativeMethodDef aReflectionMethod[] = {` |
|      - |  5821 | `		{ "getModifierNames", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "int $modifiers", "@array",` |
|      - |  5822 | `		  vm_builtin_Reflection_getModifierNames },` |
|      - |  5823 | `	};` |
|      - |  5824 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  5825 | `		{ "Reflector", "Stringable", 0, PH7_CLASS_INTERFACE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  5826 | `		{ "ReflectionException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  5827 | `		{ "Reflection", 0, 0, 0,` |
|      - |  5828 | `		  aReflectionMethod, SX_ARRAYSIZE(aReflectionMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  5829 | ``		/* Uncloneable and unserializable in php too: `clone` is an Error and`` |
|      - |  5830 | `		 * serialize() a catchable Exception naming the class. */` |
|      - |  5831 | `		{ "ReflectionClass", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  5832 | `		  aClassMethod, SX_ARRAYSIZE(aClassMethod),` |
|      - |  5833 | `		  aClassConst, SX_ARRAYSIZE(aClassConst),` |
|      - |  5834 | `		  aClassProp, SX_ARRAYSIZE(aClassProp), 0, 0, 0 },` |
|      - |  5835 | `		{ "ReflectionObject", "ReflectionClass", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  5836 | `		  aObjectMethod, SX_ARRAYSIZE(aObjectMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  5837 | `	};` |
|   8450 |  5838 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  5839 | `}` |
|      - |  5840 | `/*` |
|      - |  5841 | ` * ---------------------------------------------------------------------------` |
|      - |  5842 | ` * ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,` |
|      - |  5843 | ` * ReflectionParameter.` |
|      - |  5844 | ` *` |
|      - |  5845 | ` * Chunk 2. Its accessors funnelled through __reflect_func_info(), a descriptor` |
|      - |  5846 | ` * ARRAY built per call from either a compiled ph7_vm_func or a declared` |
|      - |  5847 | ` * SIGNATURE STRING; a native method reads whichever of the two the target` |
|      - |  5848 | ` * actually has, through the one uniform description both already agreed on` |
|      - |  5849 | ` * (ReflectParamDesc / ReflectSigDescribe).` |
|      - |  5850 | ` *` |
|      - |  5851 | ` * Five thunks retire with the chunk: __reflect_func_info, __reflect_invoke,` |
|      - |  5852 | ` * __reflect_closure, __reflect_param_default and __reflect_param_defconst.` |
|      - |  5853 | ` * ---------------------------------------------------------------------------` |
|      - |  5854 | ` */` |
|      - |  5855 | `#define RF_CL "__cl"     /* ReflectionFunctionAbstract: the reflected Closure  */` |
|      - |  5856 | `/*` |
|      - |  5857 | ` * ReflectionMethod: the class the reflector was BUILT FOR, which is not the` |
|      - |  5858 | `` * one `$class` reports. php keeps both — `intern->ce` is the class the`` |
|      - |  5859 | ` * constructor (or the ReflectionClass that handed the method out) named, while` |
|      - |  5860 | `` * the public `$class` is the method's DECLARING class — and its export tags are`` |
|      - |  5861 | ` * relative to the first: a method whose declaring class is not the reflector's` |
|      - |  5862 | `` * own class prints `inherits <declaring>`. PHL had only the declaring one, so`` |
|      - |  5863 | ` * every directly-built reflector lost the tag.` |
|      - |  5864 | ` */` |
|      - |  5865 | `#define RM_CE "__ce"` |
|      - |  5866 | `#define RP_T  "__t"      /* ReflectionParameter: the function/class it belongs to */` |
|      - |  5867 | `#define RP_M  "__m"      /* ReflectionParameter: the method name, or null      */` |
|      - |  5868 | `#define RP_P  "__p"      /* ReflectionParameter: the position                  */` |
|      - |  5869 |  |
|      - |  5870 | `/* Everything a reflected function IS, resolved once per call. */` |
|      - |  5871 | `typedef struct ReflectFuncRef ReflectFuncRef;` |
|      - |  5872 | `struct ReflectFuncRef` |
|      - |  5873 | `{` |
|      - |  5874 | `	ph7_vm *pVm;                  /* the VM, for the declared-type text rewrite */` |
|      - |  5875 | `	ph7_vm_func *pFunc;           /* compiled body (NULL for a pure C builtin) */` |
|      - |  5876 | `	ph7_user_func *pHost;         /* C builtin (NULL otherwise) */` |
|      - |  5877 | `	ph7_class *pClass;            /* class a METHOD was reached through */` |
|      - |  5878 | `	ph7_class_method *pMeth;      /* the method */` |
|      - |  5879 | `	ph7_class_instance *pClosure; /* the Closure being reflected, if any */` |
|      - |  5880 | `	const char *zSig;             /* declared parameter signature, or NULL */` |
|      - |  5881 | `	const char *zRet;             /* declared return type, or NULL */` |
|      - |  5882 | ``	int bFabricated;              /* `Closure::__invoke`: pFunc is the CLOSURE's, borrowed for its`` |
|      - |  5883 | `	                               * parameter list alone. Everything that describes where the` |
|      - |  5884 | `	                               * function came FROM -- internal or user, file, lines, module --` |
|      - |  5885 | `	                               * belongs to the fabricated method instead, which php builds as` |
|      - |  5886 | `	                               * an internal one with no module at all. */` |
|      - |  5887 | `	int bTrampoline;              /* a Closure over a name its class answers only through __call` |
|      - |  5888 | `	                               * or __callStatic: no body at all, pClosure alone (see` |
|      - |  5889 | `	                               * ReflectFuncFill). */` |
|      - |  5890 | `};` |
|      - |  5891 | `/*` |
|      - |  5892 | ` * Resolve a callable into the reference, and work out WHICH of the two` |
|      - |  5893 | ` * parameter sources describes it: a declared signature string (a C builtin, a` |
|      - |  5894 | ` * native method, or an embedded-PHP builtin declared argless over` |
|      - |  5895 | ` * func_get_args()) wins over the compiled argument list, exactly as` |
|      - |  5896 | ` * ReflectSigFixup made it win in the descriptor.` |
|      - |  5897 | ` */` |
|  11136 |  5898 | `static int ReflectFuncFill(ph7_vm *pVm, ph7_value *pTarget, ph7_value *pMethodArg,` |
|      - |  5899 | `	ReflectFuncRef *pOut)` |
|      5 |  5900 | `{` |
|  11141 |  5901 | `	SyZero(pOut, sizeof(*pOut));` |
|  11141 |  5902 | `	pOut->pVm = pVm;` |
|  16705 |  5903 | `	pOut->pFunc = ReflectResolveCallable(pVm, pTarget, pMethodArg,` |
|   5564 |  5904 | `		&pOut->pClass, &pOut->pMeth, &pOut->pHost, &pOut->pClosure);` |
|      - |  5905 | `	/* A name the class DOES declare, but the creating scope could not reach, is a trampoline` |
|      - |  5906 | `	 * as much as a missing one: the closure was never screened, so the method the lookup` |
|      - |  5907 | `	 * finds here is not the function it runs. */` |
|  11136 |  5908 | `	if( pOut->pClosure && pMethodArg == 0` |
|    842 |  5909 | `	 && (pOut->pClosure->iFlags & (VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SCREENED))` |
|    394 |  5910 | `			== VM_INSTANCE_FCC_METHOD ){` |
|     74 |  5911 | `		pOut->pFunc = 0;` |
|     74 |  5912 | `		pOut->pHost = 0;` |
|     74 |  5913 | `		pOut->pMeth = 0;` |
|     36 |  5914 | `	}` |
|  11141 |  5915 | `	if( pOut->pFunc == 0 && pOut->pHost == 0 ){` |
|    491 |  5916 | `		ph7_class_instance *pClo = pMethodArg ? 0 : ReflectValueClosure(pVm, pTarget);` |
|    491 |  5917 | `		if( pClo == 0 \|\| (pClo->iFlags & (VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SCREENED))` |
|    234 |  5918 | `				!= VM_INSTANCE_FCC_METHOD ){` |
|     21 |  5919 | `			return 0;` |
|      - |  5920 | `		}` |
|      - |  5921 | `		/* A call TRAMPOLINE: php reflects the internal function it builds for the` |
|      - |  5922 | `		 * catch-all, whose signature depends on the door that minted it -- one` |
|      - |  5923 | ``		 * variadic `mixed ...$arguments` through the `(...)` syntax, and none at all`` |
|      - |  5924 | `		 * through Closure::fromCallable() (no parameter information, which the export` |
|      - |  5925 | `		 * prints as a block with no parameter section). */` |
|    473 |  5926 | `		pOut->pClosure = pClo;` |
|    473 |  5927 | `		pOut->bTrampoline = 1;` |
|    473 |  5928 | `		pOut->zSig = (pClo->iFlags & VM_INSTANCE_FCC_SYNTAX) ? "mixed ...$arguments" : "";` |
|    473 |  5929 | `		return 1;` |
|      - |  5930 | `	}` |
|  10655 |  5931 | `	if( pOut->pFunc == 0 ){` |
|   3289 |  5932 | `		pOut->zSig = pOut->pHost->zSig;` |
|   3289 |  5933 | `		pOut->zRet = pOut->pHost->zRet;` |
|   3289 |  5934 | `		return 1;` |
|      - |  5935 | `	}` |
|   7371 |  5936 | `	if( pOut->pMeth && (pOut->pMeth->iFlags & PH7_CLASS_ATTR_FABRICATED) ){` |
|      - |  5937 | `		/* The split above: with a closure, pFunc is the CLOSURE's, so nothing of the` |
|      - |  5938 | `		 * method's own (empty) declaration may describe it -- and where it CAME from` |
|      - |  5939 | `		 * is still the method's, which bFabricated is what says. WITHOUT one (the` |
|      - |  5940 | `		 * class-level form) there is nothing to describe at all, which php prints as` |
|      - |  5941 | `		 * a block with no parameter section rather than an empty one. */` |
|    105 |  5942 | `		pOut->bFabricated = 1;` |
|    105 |  5943 | `		return 1;` |
|      - |  5944 | `	}` |
|   7267 |  5945 | `	if( (pOut->pFunc->iFlags & VM_FUNC_NATIVE) && pOut->pFunc->pNative ){` |
|   4879 |  5946 | `		pOut->zSig = pOut->pFunc->pNative->zSig;` |
|   4879 |  5947 | `		if( SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|   4879 |  5948 | `			pOut->zRet = pOut->pFunc->pNative->zRet;` |
|   2437 |  5949 | `		}` |
|   4830 |  5950 | `	}else if( (pOut->pFunc->iFlags & VM_FUNC_INTERNAL)` |
|   1318 |  5951 | `	 && SySetUsed(&pOut->pFunc->aArgs) == 0 && pOut->pMeth == 0 ){` |
|    ! 0 |  5952 | `		const char *zRet = 0;` |
|    ! 0 |  5953 | `		pOut->zSig = PH7_VmBuiltinSigLookup(SyStringData(&pOut->pFunc->sName),` |
|    ! 0 |  5954 | `			SyStringLength(&pOut->pFunc->sName), &zRet);` |
|    ! 0 |  5955 | `		if( zRet && SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|    ! 0 |  5956 | `			pOut->zRet = zRet;` |
|    ! 0 |  5957 | `		}` |
|    ! 0 |  5958 | `	}` |
|   7267 |  5959 | `	if( pOut->zSig && pOut->zSig[0] == '\0' ){` |
|      - |  5960 | `		/* A declared-EMPTY signature really is "no parameters", not "undescribed". */` |
|   1151 |  5961 | `		return 1;` |
|      - |  5962 | `	}` |
|   6121 |  5963 | `	return 1;` |
|   5569 |  5964 | `}` |
|      - |  5965 | `/* Is this receiver a ReflectionMethod (rather than a ReflectionFunction)? */` |
|   6314 |  5966 | `static int ReflectIsMethodReflector(ph7_context *pCtx, ph7_class_instance *pThis)` |
|      5 |  5967 | `{` |
|      - |  5968 | `	ph7_class *pRM;` |
|   6319 |  5969 | `	if( pThis == 0 ){` |
|    ! 0 |  5970 | `		return 0;` |
|      - |  5971 | `	}` |
|   6319 |  5972 | `	pRM = PH7_VmExtractClass(pCtx->pVm, "ReflectionMethod", sizeof("ReflectionMethod")-1, FALSE, 0);` |
|   6319 |  5973 | `	return pRM != 0 && PH7_VmInstanceOf(pThis->pClass, pRM);` |
|   3160 |  5974 | `}` |
|      - |  5975 | `/*` |
|      - |  5976 | `` * The function `$this` reflects. A ReflectionFunction built over a Closure keeps`` |
|      - |  5977 | `` * the object in `__cl` and resolves through it (its `$__fn` is a lambda name no`` |
|      - |  5978 | ` * function table holds); a ReflectionMethod resolves ($this->class, $this->name).` |
|      - |  5979 | ` */` |
|   5364 |  5980 | `static int ReflectFuncOfThis(ph7_context *pCtx, ReflectFuncRef *pOut)` |
|      5 |  5981 | `{` |
|   5369 |  5982 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  5983 | `	ph7_class_instance *pClo;` |
|      - |  5984 | `	ph7_value sTarget, sMethod;` |
|      - |  5985 | `	const char *zName, *zClass;` |
|      - |  5986 | `	int nName, nClass, rc, bMethod;` |
|   5369 |  5987 | `	SyZero(pOut, sizeof(*pOut));` |
|   5369 |  5988 | `	if( pThis == 0 ){` |
|    ! 0 |  5989 | `		return 0;` |
|      - |  5990 | `	}` |
|   5369 |  5991 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|   5369 |  5992 | `	PH7_MemObjInit(pCtx->pVm, &sMethod);` |
|   5369 |  5993 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|   5369 |  5994 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|   5369 |  5995 | `	bMethod = ReflectIsMethodReflector(pCtx, pThis);` |
|   5369 |  5996 | `	if( pClo && bMethod ){` |
|      - |  5997 | ``		/* The fabricated `Closure::__invoke`: name it over the OBJECT, which is what`` |
|      - |  5998 | `		 * makes the resolver hand back the method's identity and the closure's body` |
|      - |  5999 | `		 * together (see ReflectResolveCallable). */` |
|     39 |  6000 | `		sTarget.x.pOther = pClo;` |
|     39 |  6001 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|     39 |  6002 | `		ph7_value_string(&sMethod, zName, nName);` |
|   5350 |  6003 | `	}else if( pClo ){` |
|    763 |  6004 | `		sTarget.x.pOther = pClo;` |
|    763 |  6005 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|   4952 |  6006 | `	}else if( bMethod ){` |
|   3431 |  6007 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|   3431 |  6008 | `		ph7_value_string(&sTarget, zClass, nClass);` |
|   3431 |  6009 | `		ph7_value_string(&sMethod, zName, nName);` |
|   1718 |  6010 | `	}else{` |
|   1147 |  6011 | `		ph7_value_string(&sTarget, zName, nName);` |
|      - |  6012 | `	}` |
|   5369 |  6013 | `	rc = ReflectFuncFill(pCtx->pVm, &sTarget, (sMethod.iFlags & MEMOBJ_STRING) ? &sMethod : 0, pOut);` |
|      - |  6014 | `	/* sTarget only ever BORROWS the closure; releasing it would unref twice. */` |
|   5369 |  6015 | `	if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|   4573 |  6016 | `		PH7_MemObjRelease(&sTarget);` |
|   2282 |  6017 | `	}` |
|   5369 |  6018 | `	PH7_MemObjRelease(&sMethod);` |
|   5369 |  6019 | `	return rc;` |
|   2685 |  6020 | `}` |
|      - |  6021 | `/*` |
|      - |  6022 | `` * The class `$this` was BUILT FOR (php's `intern->ce`), or NULL — a`` |
|      - |  6023 | ` * ReflectionFunction has none, and so does a reflector whose class went away.` |
|      - |  6024 | ` */` |
|    672 |  6025 | `static ph7_class * ReflectOwnerOfThis(ph7_context *pCtx)` |
|      4 |  6026 | `{` |
|    676 |  6027 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    676 |  6028 | `	const char *zName = 0;` |
|    676 |  6029 | `	int nName = 0;` |
|    676 |  6030 | `	if( pThis == 0 ){` |
|    ! 0 |  6031 | `		return 0;` |
|      - |  6032 | `	}` |
|    676 |  6033 | `	PH7_NativeAttrStr(pThis, RM_CE, &zName, &nName);` |
|    676 |  6034 | `	if( zName == 0 \|\| nName < 1 ){` |
|    282 |  6035 | `		return 0;` |
|      - |  6036 | `	}` |
|    396 |  6037 | `	return PH7_VmExtractClass(pCtx->pVm, zName, (sxu32)nName, FALSE, 0);` |
|    340 |  6038 | `}` |
|      - |  6039 | `/* How many parameters the target declares. */` |
|   7642 |  6040 | `static int ReflectParamCount(const ReflectFuncRef *pRef)` |
|      5 |  6041 | `{` |
|   7647 |  6042 | `	if( pRef->zSig ){` |
|   6921 |  6043 | `		return ReflectSigPart(pRef->zSig, (int)SyStrlen(pRef->zSig), -1, 0, 0);` |
|      - |  6044 | `	}` |
|    731 |  6045 | `	return pRef->pFunc ? (int)SySetUsed(&pRef->pFunc->aArgs) : 0;` |
|   3826 |  6046 | `}` |
|      - |  6047 | `/*` |
|      - |  6048 | `` * The class a `self`/`parent` in this function's declared types resolves to for`` |
|      - |  6049 | ` * DISPLAY: php substitutes the declaring class into the stored text at compile` |
|      - |  6050 | ` * time, and has nothing to substitute for a TRAIT method (whose text keeps the` |
|      - |  6051 | ` * keyword however many classes composed it) or for a plain function.` |
|      - |  6052 | `` * `static` and `iterable` are printed as written -- PH7_HINT_TEXT_* off.`` |
|      - |  6053 | ` */` |
|   1142 |  6054 | `static ph7_class * ReflectDeclScope(const ReflectFuncRef *pRef)` |
|      5 |  6055 | `{` |
|   1147 |  6056 | `	if( pRef->pFunc == 0 \|\| (pRef->pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|    796 |  6057 | `		return 0; /* pUserData is a class only for a METHOD */` |
|      - |  6058 | `	}` |
|    354 |  6059 | `	return VmHintScopeDeclared((ph7_class *)pRef->pFunc->pUserData);` |
|    576 |  6060 | `}` |
|      - |  6061 | `/*` |
|      - |  6062 | ` * A MEMBER's declared type as php prints it: the same rewrite ReflectDeclScope` |
|      - |  6063 | ` * describes, keyed on the class that declared the member rather than the one it` |
|      - |  6064 | `` * was reached through -- so a trait's `?self` stays `?self`.`` |
|      - |  6065 | ` */` |
|    680 |  6066 | `static const char * ReflectMemberTypeText(ph7_vm *pVm,ph7_class_attr *pAttr,char *zBuf,sxu32 nBuf)` |
|      4 |  6067 | `{` |
|   1024 |  6068 | `	return VmHintTextResolvedEx(pVm,&pAttr->sTypeName,` |
|    340 |  6069 | `		VmHintScopeDeclared(pAttr->pDeclClass),0,zBuf,nBuf);` |
|      4 |  6070 | `}` |
|      - |  6071 | `/* Describe the iPos-th parameter. Answers 0 when there is none. */` |
|   7834 |  6072 | `static int ReflectParamAt(const ReflectFuncRef *pRef, int iPos, ReflectParamDesc *pOut)` |
|      5 |  6073 | `{` |
|   7839 |  6074 | `	SyZero(pOut, sizeof(*pOut));` |
|   7839 |  6075 | `	if( iPos < 0 ){` |
|    ! 0 |  6076 | `		return 0;` |
|      - |  6077 | `	}` |
|   7839 |  6078 | `	if( pRef->zSig ){` |
|   6785 |  6079 | `		const char *zPart = 0;` |
|   6785 |  6080 | `		int nPart = 0, nTotal;` |
|   6785 |  6081 | `		nTotal = ReflectSigPart(pRef->zSig, (int)SyStrlen(pRef->zSig), iPos, &zPart, &nPart);` |
|   6785 |  6082 | `		if( iPos >= nTotal \|\| zPart == 0 ){` |
|      3 |  6083 | `			return 0;` |
|      - |  6084 | `		}` |
|   6783 |  6085 | `		ReflectSigDescribe(zPart, nPart, iPos, pOut);` |
|   6783 |  6086 | `		return 1;` |
|      - |  6087 | `	}` |
|   1059 |  6088 | `	if( pRef->pFunc == 0 ){` |
|    ! 0 |  6089 | `		return 0;` |
|      - |  6090 | `	}` |
|      - |  6091 | `	{` |
|   1059 |  6092 | `		ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pRef->pFunc->aArgs, (sxu32)iPos);` |
|   1059 |  6093 | `		if( pArg == 0 ){` |
|     15 |  6094 | `			return 0;` |
|      - |  6095 | `		}` |
|   1045 |  6096 | `		pOut->iPos = iPos;` |
|   1045 |  6097 | `		pOut->sName = pArg->sName;` |
|   1045 |  6098 | `		pOut->bByRef = (pArg->iFlags & VM_FUNC_ARG_BY_REF) != 0;` |
|   1045 |  6099 | `		pOut->bVariadic = (pArg->iFlags & VM_FUNC_ARG_VARIADIC) != 0;` |
|      - |  6100 | `		/* The compiler never sets ARG_HAS_DEF; a default IS compiled byte-code` |
|      - |  6101 | `		 * (the same test the OP_CALL default-value path uses). */` |
|   1045 |  6102 | `		pOut->bHasDef = SySetUsed(&pArg->aByteCode) > 0;` |
|   1045 |  6103 | `		pOut->bNullable = (pArg->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|   1045 |  6104 | `		pOut->bPromoted = (pArg->iFlags & VM_FUNC_ARG_PROMOTED) != 0;` |
|   1045 |  6105 | `		pOut->bOptional = pOut->bVariadic \|\| pOut->bHasDef;` |
|   1045 |  6106 | `		if( pRef->bFabricated ){` |
|      - |  6107 | `			/* php copies the closure's parameter list into an INTERNAL record, and a` |
|      - |  6108 | `			 * default VALUE does not survive that copy: every optional parameter of a` |
|      - |  6109 | ``			 * fabricated `Closure::__invoke` answers isDefaultValueAvailable() false`` |
|      - |  6110 | ``			 * and exports as `= <default>`, while staying optional. */`` |
|     69 |  6111 | `			pOut->bHasDef = 0;` |
|     34 |  6112 | `		}` |
|      - |  6113 | `		{` |
|   1565 |  6114 | `			const char *zT = VmHintTextResolvedEx(pRef->pVm,&pArg->sTypeName,` |
|   1040 |  6115 | `				ReflectDeclScope(pRef),0,pOut->zTypeBuf,sizeof(pOut->zTypeBuf));` |
|   1045 |  6116 | `			SyStringInitFromBuf(&pOut->sType,zT,(sxu32)SyStrlen(zT));` |
|      - |  6117 | `		}` |
|   1045 |  6118 | `		pOut->pArg = pArg;` |
|   2051 |  6119 | `		pOut->bInternal = pRef->bFabricated` |
|   1040 |  6120 | `			\|\| (pRef->pFunc->iFlags & VM_FUNC_INTERNAL) != 0;` |
|   1045 |  6121 | `		return 1;` |
|      - |  6122 | `	}` |
|   3922 |  6123 | `}` |
|      - |  6124 | `/* The declaring class php reports for a reflected METHOD. */` |
|   9760 |  6125 | `static ph7_class * ReflectFuncDeclClass(const ReflectFuncRef *pRef)` |
|      4 |  6126 | `{` |
|   9764 |  6127 | `	if( pRef->pMeth == 0 \|\| pRef->pClass == 0 ){` |
|      3 |  6128 | `		return 0;` |
|      - |  6129 | `	}` |
|   9762 |  6130 | `	return ReflectMethodDeclClass(pRef->pClass, pRef->pMeth);` |
|   4884 |  6131 | `}` |
|      - |  6132 | `/*` |
|      - |  6133 | ` * The declared return type TEXT, or 0 — and whether php calls it TENTATIVE.` |
|      - |  6134 | ` *` |
|      - |  6135 | ` * php's stubs carry two kinds of internal return type and Reflection answers` |
|      - |  6136 | ` * differently for each: a real one is reported by getReturnType()/hasReturnType()` |
|      - |  6137 | `` * and printed `Return [ T ]`, while an `@tentative-return-type` answers NULL and`` |
|      - |  6138 | ` * false from those two, is reported by getTentativeReturnType()/` |
|      - |  6139 | `` * hasTentativeReturnType(), and prints `Tentative return [ T ]`. Nearly every`` |
|      - |  6140 | ` * internal SPL, date and Reflection method is the tentative kind.` |
|      - |  6141 | ` *` |
|      - |  6142 | `` * PHL has one field for both, so a leading `@` on a `zRet` — the same character`` |
|      - |  6143 | ` * php's stub annotation uses — marks the tentative kind. It is stripped here, so` |
|      - |  6144 | ` * nothing downstream of this function ever sees it.` |
|      - |  6145 | ` */` |
|   6169 |  6146 | `static int ReflectFuncRetTextEx(const ReflectFuncRef *pRef, const char **pz, int *pn,` |
|      - |  6147 | `	int *pbTentative, char *zBuf, sxu32 nBuf)` |
|      5 |  6148 | `{` |
|   6174 |  6149 | `	if( pbTentative ){` |
|   6174 |  6150 | `		*pbTentative = 0;` |
|   3083 |  6151 | `	}` |
|   6174 |  6152 | `	if( pRef->zRet && pRef->zRet[0] ){` |
|   5628 |  6153 | `		const char *z = pRef->zRet;` |
|   5628 |  6154 | `		if( z[0] == '@' ){` |
|   3173 |  6155 | `			if( pbTentative ){` |
|   3173 |  6156 | `				*pbTentative = 1;` |
|   1585 |  6157 | `			}` |
|   3173 |  6158 | `			z++;` |
|   1585 |  6159 | `		}` |
|   5628 |  6160 | `		*pz = z;` |
|   5628 |  6161 | `		*pn = (int)SyStrlen(z);` |
|   5628 |  6162 | `		return 1;` |
|      - |  6163 | `	}` |
|    550 |  6164 | `	if( pRef->pFunc == 0 ){` |
|     29 |  6165 | `		return 0;` |
|      - |  6166 | `	}` |
|    524 |  6167 | `	if( SyStringLength(&pRef->pFunc->sReturnTypeName) > 0 ){` |
|    157 |  6168 | `		*pz = VmHintTextResolvedEx(pRef->pVm,&pRef->pFunc->sReturnTypeName,` |
|     51 |  6169 | `			ReflectDeclScope(pRef),0,zBuf,nBuf);` |
|    106 |  6170 | `		*pn = (int)SyStrlen(*pz);` |
|    106 |  6171 | `		return 1;` |
|      - |  6172 | `	}` |
|    421 |  6173 | `	return 0;` |
|   3088 |  6174 | `}` |
|      - |  6175 | `/* The declared return type php would report from getReturnType(): a TENTATIVE one` |
|      - |  6176 | ` * is not reported there at all, which is the whole distinction. */` |
|    863 |  6177 | `static int ReflectFuncRetText(const ReflectFuncRef *pRef, const char **pz, int *pn,` |
|      - |  6178 | `	char *zBuf, sxu32 nBuf)` |
|      5 |  6179 | `{` |
|    868 |  6180 | `	int bTentative = 0;` |
|    868 |  6181 | `	if( !ReflectFuncRetTextEx(pRef, pz, pn, &bTentative, zBuf, nBuf) ){` |
|     28 |  6182 | `		return 0;` |
|      - |  6183 | `	}` |
|    842 |  6184 | `	return bTentative ? 0 : 1;` |
|    435 |  6185 | `}` |
|      - |  6186 | `/* Is the reflected function internal (a C builtin or an embedded-chunk one)? */` |
|   9971 |  6187 | `static int ReflectFuncIsInternal(const ReflectFuncRef *pRef)` |
|      4 |  6188 | `{` |
|   9975 |  6189 | `	if( pRef->pHost \|\| pRef->bFabricated \|\| pRef->bTrampoline ){` |
|   1161 |  6190 | `		return 1;` |
|      - |  6191 | `	}` |
|   8818 |  6192 | `	return pRef->pFunc != 0 && (pRef->pFunc->iFlags & VM_FUNC_INTERNAL) != 0;` |
|   4989 |  6193 | `}` |
|      - |  6194 | ``/* Resolve `$this`, or answer a default when the target has gone missing. */`` |
|      - |  6195 | `#define REFLECT_FUNC_OR(REF,STMT) \` |
|      - |  6196 | `	if( !ReflectFuncOfThis(pCtx, &(REF)) ){ STMT; return PH7_OK; }` |
|      - |  6197 | `/*` |
|      - |  6198 | ` * The same, for the three getClosure* accessors: what they read is captured on` |
|      - |  6199 | ` * the Closure OBJECT, not in a function record, so a reflector over a closure` |
|      - |  6200 | ` * nothing resolves (php's magic-method trampoline) must still answer from the` |
|      - |  6201 | `` * Closure `$this` is holding. Without this they fell out at the resolve and`` |
|      - |  6202 | ` * reported no scope and no bound object for a callable php describes fully.` |
|      - |  6203 | ` */` |
|      - |  6204 | `#define REFLECT_CLOSURE_OR(REF,STMT) \` |
|      - |  6205 | `	if( !ReflectFuncOfThis(pCtx, &(REF)) ){ \` |
|      - |  6206 | `		ph7_class_instance *_pRcThis = PH7_ContextThis(pCtx); \` |
|      - |  6207 | `		SyZero(&(REF), sizeof(REF)); \` |
|      - |  6208 | `		(REF).pVm = pCtx->pVm; \` |
|      - |  6209 | `		(REF).pClosure = _pRcThis ? PH7_NativeAttrObj(_pRcThis, RF_CL) : 0; \` |
|      - |  6210 | `		if( (REF).pClosure == 0 ){ STMT; return PH7_OK; } \` |
|      - |  6211 | `	}` |
|      - |  6212 |  |
|      - |  6213 | `/* ---- ReflectionFunctionAbstract ---- */` |
|    ! 0 |  6214 | `static int vm_builtin_ReflectionFunc_clone(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  6215 | `{` |
|    ! 0 |  6216 | `	SXUNUSED(pCtx);` |
|    ! 0 |  6217 | `	SXUNUSED(nArg);` |
|    ! 0 |  6218 | `	SXUNUSED(apArg);` |
|    ! 0 |  6219 | `	return PH7_OK;` |
|    ! 0 |  6220 | `}` |
|    638 |  6221 | `static int vm_builtin_ReflectionFunc_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  6222 | `{` |
|    642 |  6223 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    642 |  6224 | `	const char *zName = "";` |
|    642 |  6225 | `	int nName = 0;` |
|    319 |  6226 | `	SXUNUSED(nArg);` |
|    319 |  6227 | `	SXUNUSED(apArg);` |
|    642 |  6228 | `	if( pThis ){` |
|    642 |  6229 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    319 |  6230 | `	}` |
|    642 |  6231 | `	ph7_result_string(pCtx, zName, nName);` |
|    642 |  6232 | `	return PH7_OK;` |
|      4 |  6233 | `}` |
|      - |  6234 | ``/* The `name` slot's namespace split — the same three answers ReflectionClass gives. */`` |
|    ! 0 |  6235 | `static int ReflectFuncNamePart(ph7_context *pCtx, int iWhat)` |
|    ! 0 |  6236 | `{` |
|    ! 0 |  6237 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    ! 0 |  6238 | `	const char *zName = "";` |
|    ! 0 |  6239 | `	int nName = 0, iCut;` |
|    ! 0 |  6240 | `	if( pThis ){` |
|    ! 0 |  6241 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    ! 0 |  6242 | `	}` |
|    ! 0 |  6243 | `	iCut = ReflectNsCut(zName, nName);` |
|    ! 0 |  6244 | `	if( iWhat == 0 ){` |
|    ! 0 |  6245 | `		ph7_result_bool(pCtx, iCut >= 0);` |
|    ! 0 |  6246 | `	}else if( iWhat == 1 ){` |
|    ! 0 |  6247 | `		ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|    ! 0 |  6248 | `	}else if( iCut < 0 ){` |
|    ! 0 |  6249 | `		ph7_result_string(pCtx, zName, nName);` |
|    ! 0 |  6250 | `	}else{` |
|    ! 0 |  6251 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|      - |  6252 | `	}` |
|    ! 0 |  6253 | `	return PH7_OK;` |
|    ! 0 |  6254 | `}` |
|      - |  6255 | `#define REFLECT_FUNC_NAMEPART(NAME,WHAT) \` |
|      - |  6256 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  6257 | `	{ \` |
|      - |  6258 | `		SXUNUSED(nArg); \` |
|      - |  6259 | `		SXUNUSED(apArg); \` |
|      - |  6260 | `		return ReflectFuncNamePart(pCtx, WHAT); \` |
|      - |  6261 | `	}` |
|    ! 0 |  6262 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_inNamespace, 0)` |
|    ! 0 |  6263 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_getNamespaceName, 1)` |
|    ! 0 |  6264 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_getShortName, 2)` |
|      - |  6265 |  |
|      - |  6266 | `/*` |
|      - |  6267 | ` * isClosure() is php's ZEND_ACC_CLOSURE: EVERY function a Closure object holds` |
|      - |  6268 | `` * carries it -- `strlen(...)`, `fromCallable([$o,'m'])` and a __call`` |
|      - |  6269 | ` * trampoline included -- and nothing reached by name does. A ReflectionMethod` |
|      - |  6270 | `` * never does either, not even `Closure::__invoke`, which php fabricates as a`` |
|      - |  6271 | `` * plain internal method. isAnonymous() is the narrower `{closure}` question.`` |
|      - |  6272 | ` */` |
|     48 |  6273 | `static int vm_builtin_ReflectionFunc_isClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6274 | `{` |
|     50 |  6275 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     24 |  6276 | `	SXUNUSED(nArg);` |
|     24 |  6277 | `	SXUNUSED(apArg);` |
|     88 |  6278 | `	ph7_result_bool(pCtx, pThis != 0 && PH7_NativeAttrObj(pThis, RF_CL) != 0` |
|     43 |  6279 | `		&& !ReflectIsMethodReflector(pCtx, pThis));` |
|     50 |  6280 | `	return PH7_OK;` |
|      2 |  6281 | `}` |
|     38 |  6282 | `static int vm_builtin_ReflectionFunction_isAnonymous(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6283 | `{` |
|      - |  6284 | `	ReflectFuncRef sRef;` |
|     40 |  6285 | `	int bAnon = 0;` |
|     19 |  6286 | `	SXUNUSED(nArg);` |
|     19 |  6287 | `	SXUNUSED(apArg);` |
|     40 |  6288 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     40 |  6289 | `	if( sRef.pFunc ){` |
|      - |  6290 | ``		/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|      - |  6291 | `		 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|     30 |  6292 | `		bAnon = (sRef.pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|     28 |  6293 | `		if( !bAnon && SyStringLength(&sRef.pFunc->sName) > 9` |
|     10 |  6294 | `		 && (SyMemcmp(SyStringData(&sRef.pFunc->sName), "[lambda_", 8) == 0` |
|      1 |  6295 | `		  \|\| SyMemcmp(SyStringData(&sRef.pFunc->sName), "[closure_", 9) == 0) ){` |
|      3 |  6296 | `			bAnon = 1;` |
|      1 |  6297 | `		}` |
|     14 |  6298 | `	}` |
|     40 |  6299 | `	ph7_result_bool(pCtx, bAnon);` |
|     40 |  6300 | `	return PH7_OK;` |
|     21 |  6301 | `}` |
|      - |  6302 | `/*` |
|      - |  6303 | ` * php's deprecation for an INTERNAL name, or NULL. Userland's is the` |
|      - |  6304 | ` * #[\Deprecated] attribute; this is the engine's own table, stamped on the C` |
|      - |  6305 | ` * body a builtin and a native method share (aDeprecatedFunc[]).` |
|      - |  6306 | ` */` |
|   4938 |  6307 | `static const ph7_deprecated_name * ReflectFuncDeprecated(const ReflectFuncRef *pRef)` |
|      4 |  6308 | `{` |
|   4942 |  6309 | `	if( pRef->pHost ){` |
|    514 |  6310 | `		return pRef->pHost->pDeprecated;` |
|      - |  6311 | `	}` |
|   4432 |  6312 | `	if( pRef->pFunc && (pRef->pFunc->iFlags & VM_FUNC_NATIVE) && pRef->pFunc->pNative ){` |
|   4311 |  6313 | `		return pRef->pFunc->pNative->pDeprecated;` |
|      - |  6314 | `	}` |
|    124 |  6315 | `	return 0;` |
|   2473 |  6316 | `}` |
|     38 |  6317 | `static int vm_builtin_ReflectionFunc_isDeprecated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6318 | `{` |
|      - |  6319 | `	ReflectFuncRef sRef;` |
|     19 |  6320 | `	SXUNUSED(nArg);` |
|     19 |  6321 | `	SXUNUSED(apArg);` |
|     39 |  6322 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     49 |  6323 | `	ph7_result_bool(pCtx, ReflectFuncDeprecated(&sRef) != 0` |
|     24 |  6324 | `		\|\| (sRef.pFunc != 0 && ReflectHasDeprecated(&sRef.pFunc->aAttrs)));` |
|     39 |  6325 | `	return PH7_OK;` |
|     20 |  6326 | `}` |
|     66 |  6327 | `static int vm_builtin_ReflectionFunc_isInternal(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  6328 | `{` |
|      - |  6329 | `	ReflectFuncRef sRef;` |
|     33 |  6330 | `	SXUNUSED(nArg);` |
|     33 |  6331 | `	SXUNUSED(apArg);` |
|     70 |  6332 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     70 |  6333 | `	ph7_result_bool(pCtx, ReflectFuncIsInternal(&sRef));` |
|     70 |  6334 | `	return PH7_OK;` |
|     37 |  6335 | `}` |
|     10 |  6336 | `static int vm_builtin_ReflectionFunc_isUserDefined(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6337 | `{` |
|      - |  6338 | `	ReflectFuncRef sRef;` |
|      5 |  6339 | `	SXUNUSED(nArg);` |
|      5 |  6340 | `	SXUNUSED(apArg);` |
|     12 |  6341 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     12 |  6342 | `	ph7_result_bool(pCtx, !ReflectFuncIsInternal(&sRef));` |
|     12 |  6343 | `	return PH7_OK;` |
|      7 |  6344 | `}` |
|      6 |  6345 | `static int vm_builtin_ReflectionFunc_isGenerator(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6346 | `{` |
|      - |  6347 | `	ReflectFuncRef sRef;` |
|      3 |  6348 | `	SXUNUSED(nArg);` |
|      3 |  6349 | `	SXUNUSED(apArg);` |
|      7 |  6350 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|      7 |  6351 | `	ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|      7 |  6352 | `	return PH7_OK;` |
|      4 |  6353 | `}` |
|     20 |  6354 | `static int vm_builtin_ReflectionFunc_isVariadic(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  6355 | `{` |
|      - |  6356 | `	ReflectFuncRef sRef;` |
|      - |  6357 | `	ReflectParamDesc sDesc;` |
|     23 |  6358 | `	int n, nTotal, bVariadic = 0;` |
|     10 |  6359 | `	SXUNUSED(nArg);` |
|     10 |  6360 | `	SXUNUSED(apArg);` |
|     23 |  6361 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     23 |  6362 | `	nTotal = ReflectParamCount(&sRef);` |
|     43 |  6363 | `	for( n = 0 ; n < nTotal ; n++ ){` |
|     37 |  6364 | `		if( ReflectParamAt(&sRef, n, &sDesc) && sDesc.bVariadic ){` |
|     17 |  6365 | `			bVariadic = 1;` |
|     17 |  6366 | `			break;` |
|      - |  6367 | `		}` |
|     11 |  6368 | `	}` |
|     23 |  6369 | `	ph7_result_bool(pCtx, bVariadic);` |
|     23 |  6370 | `	return PH7_OK;` |
|     13 |  6371 | `}` |
|      - |  6372 | ``/* isStatic(): a METHOD's own staticness, a closure's `static function () {}`, a trampoline's catch-all. */`` |
|    140 |  6373 | `static int vm_builtin_ReflectionFunc_isStatic(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  6374 | `{` |
|      - |  6375 | `	ReflectFuncRef sRef;` |
|     70 |  6376 | `	SXUNUSED(nArg);` |
|     70 |  6377 | `	SXUNUSED(apArg);` |
|    144 |  6378 | `	REFLECT_CLOSURE_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    144 |  6379 | `	if( sRef.pClosure && (sRef.pClosure->iFlags` |
|     50 |  6380 | `			& (VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SCREENED)) == VM_INSTANCE_FCC_METHOD ){` |
|      - |  6381 | `		/* A catch-all trampoline: static exactly when it is __callStatic's */` |
|     41 |  6382 | `		ph7_result_bool(pCtx, PH7_VmClosureIsStatic(pCtx->pVm, sRef.pClosure));` |
|    124 |  6383 | `	}else if( sRef.pMeth ){` |
|     95 |  6384 | `		ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|     49 |  6385 | `	}else{` |
|     11 |  6386 | `		ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);` |
|      - |  6387 | `	}` |
|    144 |  6388 | `	return PH7_OK;` |
|     74 |  6389 | `}` |
|      8 |  6390 | `static int vm_builtin_ReflectionFunc_returnsReference(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6391 | `{` |
|      - |  6392 | `	ReflectFuncRef sRef;` |
|      4 |  6393 | `	SXUNUSED(nArg);` |
|      4 |  6394 | `	SXUNUSED(apArg);` |
|      9 |  6395 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|      9 |  6396 | `	ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|      9 |  6397 | `	return PH7_OK;` |
|      5 |  6398 | `}` |
|      - |  6399 | `/* The Closure instance whose captured state the three getClosure* read. */` |
|    728 |  6400 | `static ph7_value * ReflectClosureAttr(ReflectFuncRef *pRef, const char *zName)` |
|      5 |  6401 | `{` |
|      - |  6402 | `	SyString sAttr;` |
|    733 |  6403 | `	if( pRef->pClosure == 0 ){` |
|    ! 0 |  6404 | `		return 0;` |
|      - |  6405 | `	}` |
|    733 |  6406 | `	SyStringInitFromBuf(&sAttr, zName, SyStrlen(zName));` |
|    733 |  6407 | `	return PH7_ClassInstanceFetchAttr(pRef->pClosure, &sAttr);` |
|    369 |  6408 | `}` |
|      - |  6409 | `/*` |
|      - |  6410 | ` * A closure EXPRESSION keeps what it is bound to where OP_LOAD_CLOSURE put it, on its` |
|      - |  6411 | ` * per-instantiation function: the declaring class in pUserData, the called class in` |
|      - |  6412 | `` * pLsbClass, the receiver as the auto-captured `$this`. It carries no $__this/$__scope until`` |
|      - |  6413 | ` * a bind/bindTo clone writes them, and from then on those are the whole answer (an empty pair` |
|      - |  6414 | ` * is an unbind). Returns that function for an untouched closure expression, else 0.` |
|      - |  6415 | ` */` |
|    182 |  6416 | `static ph7_vm_func * ReflectClosureExpr(const ReflectFuncRef *pRef)` |
|      2 |  6417 | `{` |
|    182 |  6418 | `	if( pRef->pClosure == 0 \|\| pRef->pFunc == 0 \|\| pRef->pMeth \|\| pRef->bFabricated` |
|    130 |  6419 | `	 \|\| (pRef->pClosure->iFlags & (VM_INSTANCE_FCC_BOUND\|VM_INSTANCE_FCC_REBOUND` |
|      - |  6420 | `			\|VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_INVOKE_OBJ))` |
|    114 |  6421 | `	 \|\| (pRef->pFunc->iFlags & VM_FUNC_CLOSURE) == 0 ){` |
|     94 |  6422 | `		return 0;` |
|      - |  6423 | `	}` |
|     91 |  6424 | `	return pRef->pFunc;` |
|     93 |  6425 | `}` |
|      - |  6426 | `/* The receiver a closure expression captured from the method that made it, or 0. */` |
|    128 |  6427 | `static ph7_class_instance * ReflectClosureExprThis(const ReflectFuncRef *pRef)` |
|      2 |  6428 | `{` |
|    130 |  6429 | `	ph7_vm_func *pFunc = ReflectClosureExpr(pRef);` |
|      - |  6430 | `	ph7_vm_func_closure_env *aEnv;` |
|      - |  6431 | `	sxu32 n;` |
|    130 |  6432 | `	if( pFunc == 0 ){` |
|     84 |  6433 | `		return 0;` |
|      - |  6434 | `	}` |
|     47 |  6435 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     49 |  6436 | `	for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; n++ ){` |
|     40 |  6437 | `		if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|     40 |  6438 | `			&& SyMemcmp(SyStringData(&aEnv[n].sName),"this",sizeof("this")-1) == 0 ){` |
|     39 |  6439 | `			return (aEnv[n].sValue.iFlags & MEMOBJ_OBJ)` |
|     31 |  6440 | `				? (ph7_class_instance *)aEnv[n].sValue.x.pOther : 0;` |
|      - |  6441 | `		}` |
|      2 |  6442 | `	}` |
|      9 |  6443 | `	return 0;` |
|     66 |  6444 | `}` |
|    184 |  6445 | `static int vm_builtin_ReflectionFunc_getClosureThis(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6446 | `{` |
|      - |  6447 | `	ReflectFuncRef sRef;` |
|      - |  6448 | `	ph7_value *pAttr;` |
|     92 |  6449 | `	SXUNUSED(nArg);` |
|     92 |  6450 | `	SXUNUSED(apArg);` |
|    186 |  6451 | `	REFLECT_CLOSURE_OR(sRef, ph7_result_null(pCtx))` |
|    186 |  6452 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|    186 |  6453 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|     77 |  6454 | `		ph7_result_value(pCtx, pAttr);` |
|     39 |  6455 | `	}else{` |
|    110 |  6456 | `		ph7_class_instance *pRecv = ReflectClosureExprThis(&sRef);` |
|    110 |  6457 | `		if( pRecv ){` |
|      - |  6458 | `			ph7_value sRecv;` |
|     15 |  6459 | `			PH7_MemObjInit(pCtx->pVm, &sRecv);` |
|     15 |  6460 | `			sRecv.x.pOther = pRecv;` |
|     15 |  6461 | `			sRecv.iFlags = MEMOBJ_OBJ;` |
|      - |  6462 | `			/* ph7_result_value takes its own reference; sRecv only borrows. */` |
|     15 |  6463 | `			ph7_result_value(pCtx, &sRecv);` |
|      8 |  6464 | `		}else{` |
|     96 |  6465 | `			ph7_result_null(pCtx);` |
|      - |  6466 | `		}` |
|      - |  6467 | `	}` |
|    186 |  6468 | `	return PH7_OK;` |
|     94 |  6469 | `}` |
|    190 |  6470 | `static int vm_builtin_ReflectionFunc_getClosureScopeClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  6471 | `{` |
|      - |  6472 | `	ReflectFuncRef sRef;` |
|      - |  6473 | `	ph7_value *pAttr;` |
|     95 |  6474 | `	SXUNUSED(nArg);` |
|     95 |  6475 | `	SXUNUSED(apArg);` |
|    193 |  6476 | `	REFLECT_CLOSURE_OR(sRef, ph7_result_null(pCtx))` |
|    193 |  6477 | `	pAttr = ReflectClosureAttr(&sRef, "__scope");` |
|    193 |  6478 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|      - |  6479 | `		/* php reports the class that DECLARED the callee, not the one the callable NAMED:` |
|      - |  6480 | ``		 * `$__scope` is the CALLED scope (what `static::` answers, reported by`` |
|      - |  6481 | `		 * getClosureCalledClass below), and for an inherited or trait-composed method the` |
|      - |  6482 | `		 * two differ. PH7_VmClosureScopeClass is the one rule. */` |
|    231 |  6483 | `		return ReflectResultClassOf(pCtx,` |
|     76 |  6484 | `			PH7_VmClosureScopeClass(pCtx->pVm, sRef.pClosure));` |
|      - |  6485 | `	}` |
|     39 |  6486 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|     39 |  6487 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|      - |  6488 | `		/* A rebind wrote down any scope it kept, so a receiver with none was handed php's` |
|      - |  6489 | ``		 * dummy scope -- `Closure` -- not the receiver's class. */`` |
|     13 |  6490 | `		return ReflectResultClassOf(pCtx, (sRef.pClosure->iFlags & VM_INSTANCE_FCC_METHOD)` |
|      4 |  6491 | `			? ((ph7_class_instance *)pAttr->x.pOther)->pClass : pCtx->pVm->pClosureClass);` |
|      - |  6492 | `	}` |
|     31 |  6493 | `	if( ReflectClosureExpr(&sRef) && sRef.pFunc->pUserData ){` |
|      - |  6494 | `		/* The class whose method made it -- the USING class when that method is a trait's,` |
|      - |  6495 | ``		 * which php composes into the class, exactly as `self::` resolves in the body. */`` |
|     19 |  6496 | `		ph7_class *pDecl = (ph7_class *)sRef.pFunc->pUserData;` |
|     19 |  6497 | `		if( (pDecl->iFlags & PH7_CLASS_TRAIT) && sRef.pFunc->pLsbClass ){` |
|      5 |  6498 | `			pDecl = PH7_VmTraitUsingClass(pCtx->pVm, pDecl, (ph7_class *)sRef.pFunc->pLsbClass);` |
|      2 |  6499 | `		}` |
|     19 |  6500 | `		return ReflectResultClassOf(pCtx, pDecl);` |
|      - |  6501 | `	}` |
|     13 |  6502 | `	ph7_result_null(pCtx);` |
|     13 |  6503 | `	return PH7_OK;` |
|     98 |  6504 | `}` |
|      - |  6505 | `/*` |
|      - |  6506 | ` * ReflectionFunction::getClosureCalledClass() — php's CALLED scope, the other half of the` |
|      - |  6507 | `` * pair above: the class the call goes THROUGH, which is what `static::` answers. A bound`` |
|      - |  6508 | `` * closure's is its `$this`'s class; a static callable's is the class the callable NAMED. It`` |
|      - |  6509 | `` * shared getClosureScopeClass()'s body while `$__scope` answered both questions; it cannot`` |
|      - |  6510 | ` * now, because that one narrows a method closure to its DECLARING class.` |
|      - |  6511 | ` */` |
|    152 |  6512 | `static int vm_builtin_ReflectionFunc_getClosureCalledClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6513 | `{` |
|      - |  6514 | `	ReflectFuncRef sRef;` |
|      - |  6515 | `	ph7_value *pAttr;` |
|     76 |  6516 | `	SXUNUSED(nArg);` |
|     76 |  6517 | `	SXUNUSED(apArg);` |
|    154 |  6518 | `	REFLECT_CLOSURE_OR(sRef, ph7_result_null(pCtx))` |
|    154 |  6519 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|    154 |  6520 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|     60 |  6521 | `		return ReflectResultClassOf(pCtx, ((ph7_class_instance *)pAttr->x.pOther)->pClass);` |
|      - |  6522 | `	}` |
|      - |  6523 | ``	/* A forwarding `parent::sf(...)` goes through the caller's class, not $__scope's. */`` |
|     96 |  6524 | `	pAttr = ReflectClosureAttr(&sRef, "__called");` |
|     96 |  6525 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pAttr->sBlob) == 0 ){` |
|     62 |  6526 | `		pAttr = ReflectClosureAttr(&sRef, "__scope");` |
|     30 |  6527 | `	}` |
|     96 |  6528 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|    107 |  6529 | `		return ReflectResultClassOf(pCtx, PH7_VmExtractClass(pCtx->pVm,` |
|     70 |  6530 | `			(const char *)SyBlobData(&pAttr->sBlob), SyBlobLength(&pAttr->sBlob), FALSE, 0));` |
|      - |  6531 | `	}` |
|     25 |  6532 | `	if( ReflectClosureExpr(&sRef) ){` |
|      - |  6533 | ``		/* What `static::` answers in the body: the receiver's class, else the class the`` |
|      - |  6534 | `		 * making call went through. */` |
|     21 |  6535 | `		ph7_class_instance *pRecv = ReflectClosureExprThis(&sRef);` |
|     21 |  6536 | `		if( pRecv ){` |
|     11 |  6537 | `			return ReflectResultClassOf(pCtx, pRecv->pClass);` |
|      - |  6538 | `		}` |
|     11 |  6539 | `		if( sRef.pFunc->pLsbClass ){` |
|      9 |  6540 | `			return ReflectResultClassOf(pCtx, (ph7_class *)sRef.pFunc->pLsbClass);` |
|      - |  6541 | `		}` |
|      1 |  6542 | `	}` |
|      7 |  6543 | `	ph7_result_null(pCtx);` |
|      7 |  6544 | `	return PH7_OK;` |
|     78 |  6545 | `}` |
|     10 |  6546 | `static int vm_builtin_ReflectionFunc_getClosureUsedVariables(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6547 | `{` |
|      - |  6548 | `	ReflectFuncRef sRef;` |
|     12 |  6549 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  6550 | `	ph7_vm_func_closure_env *aEnv;` |
|      - |  6551 | `	sxu32 n;` |
|      5 |  6552 | `	SXUNUSED(nArg);` |
|      5 |  6553 | `	SXUNUSED(apArg);` |
|     12 |  6554 | `	if( pOut == 0 ){` |
|    ! 0 |  6555 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  6556 | `	}` |
|     12 |  6557 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pClosure == 0 \|\| sRef.pFunc == 0 ){` |
|    ! 0 |  6558 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  6559 | `		return PH7_OK;` |
|      - |  6560 | `	}` |
|      - |  6561 | `	/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */` |
|     12 |  6562 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|     28 |  6563 | `	for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; n++ ){` |
|     18 |  6564 | `		if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){` |
|     12 |  6565 | `			continue;` |
|      - |  6566 | `		}` |
|      6 |  6567 | `		if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|      4 |  6568 | `		 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|    ! 0 |  6569 | `			continue;` |
|      - |  6570 | `		}` |
|      7 |  6571 | `		if( (aEnv[n].iFlags & VM_FUNC_ARG_BY_REF) && aEnv[n].nIdx != SXU32_HIGH ){` |
|      - |  6572 | `			/* Captured by reference: report the slot's live value */` |
|      3 |  6573 | `			ph7_value *pLive = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, aEnv[n].nIdx);` |
|      3 |  6574 | `			ReflectMapAddDyn(pCtx, pOut, &aEnv[n].sName, pLive ? pLive : &aEnv[n].sValue);` |
|      3 |  6575 | `			continue;` |
|      - |  6576 | `		}` |
|      5 |  6577 | `		ReflectMapAddDyn(pCtx, pOut, &aEnv[n].sName, &aEnv[n].sValue);` |
|      3 |  6578 | `	}` |
|     12 |  6579 | `	ph7_result_value(pCtx, pOut);` |
|     12 |  6580 | `	return PH7_OK;` |
|      7 |  6581 | `}` |
|     14 |  6582 | `static int vm_builtin_ReflectionFunc_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6583 | `{` |
|      - |  6584 | `	ReflectFuncRef sRef;` |
|      7 |  6585 | `	SXUNUSED(nArg);` |
|      7 |  6586 | `	SXUNUSED(apArg);` |
|     15 |  6587 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     15 |  6588 | `	if( sRef.pFunc && SyStringLength(&sRef.pFunc->sDoc) > 0 ){` |
|     11 |  6589 | `		ph7_result_string(pCtx, SyStringData(&sRef.pFunc->sDoc), (int)SyStringLength(&sRef.pFunc->sDoc));` |
|      6 |  6590 | `	}else{` |
|      5 |  6591 | `		ph7_result_bool(pCtx, 0);` |
|      - |  6592 | `	}` |
|     15 |  6593 | `	return PH7_OK;` |
|      8 |  6594 | `}` |
|     38 |  6595 | `static int vm_builtin_ReflectionFunc_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  6596 | `{` |
|      - |  6597 | `	ReflectFuncRef sRef;` |
|     19 |  6598 | `	SXUNUSED(nArg);` |
|     19 |  6599 | `	SXUNUSED(apArg);` |
|     42 |  6600 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     42 |  6601 | `	if( sRef.pFunc && !sRef.bFabricated && SyStringLength(&sRef.pFunc->sFile) > 0 ){` |
|      7 |  6602 | `		ph7_result_string(pCtx, SyStringData(&sRef.pFunc->sFile), (int)SyStringLength(&sRef.pFunc->sFile));` |
|      4 |  6603 | `	}else{` |
|     36 |  6604 | `		ph7_result_bool(pCtx, 0);` |
|      - |  6605 | `	}` |
|     42 |  6606 | `	return PH7_OK;` |
|     23 |  6607 | `}` |
|     40 |  6608 | `static int ReflectFuncLine(ph7_context *pCtx, int bEnd)` |
|      3 |  6609 | `{` |
|      - |  6610 | `	ReflectFuncRef sRef;` |
|     43 |  6611 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| ReflectFuncIsInternal(&sRef) \|\| sRef.pFunc == 0 ){` |
|     29 |  6612 | `		ph7_result_bool(pCtx, 0);` |
|     29 |  6613 | `		return PH7_OK;` |
|      - |  6614 | `	}` |
|     16 |  6615 | `	ph7_result_int64(pCtx, (sxi64)(bEnd ? sRef.pFunc->nEndLine : sRef.pFunc->nLine));` |
|     16 |  6616 | `	return PH7_OK;` |
|     23 |  6617 | `}` |
|     38 |  6618 | `static int vm_builtin_ReflectionFunc_getStartLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  6619 | `{` |
|     19 |  6620 | `	SXUNUSED(nArg);` |
|     19 |  6621 | `	SXUNUSED(apArg);` |
|     41 |  6622 | `	return ReflectFuncLine(pCtx, 0);` |
|      3 |  6623 | `}` |
|      2 |  6624 | `static int vm_builtin_ReflectionFunc_getEndLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6625 | `{` |
|      1 |  6626 | `	SXUNUSED(nArg);` |
|      1 |  6627 | `	SXUNUSED(apArg);` |
|      3 |  6628 | `	return ReflectFuncLine(pCtx, 1);` |
|      1 |  6629 | `}` |
|    258 |  6630 | `static int vm_builtin_ReflectionFunc_hasReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6631 | `{` |
|      - |  6632 | `	ReflectFuncRef sRef;` |
|      - |  6633 | `	const char *z;` |
|      - |  6634 | `	char zRetBuf[192];` |
|      - |  6635 | `	int n;` |
|    129 |  6636 | `	SXUNUSED(nArg);` |
|    129 |  6637 | `	SXUNUSED(apArg);` |
|    260 |  6638 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    260 |  6639 | `	ph7_result_bool(pCtx, ReflectFuncRetText(&sRef, &z, &n, zRetBuf, sizeof(zRetBuf)));` |
|    260 |  6640 | `	return PH7_OK;` |
|    131 |  6641 | `}` |
|    605 |  6642 | `static int vm_builtin_ReflectionFunc_getReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  6643 | `{` |
|      - |  6644 | `	ReflectFuncRef sRef;` |
|      - |  6645 | `	const char *z;` |
|      - |  6646 | `	char zRetBuf[192];` |
|      - |  6647 | `	int n;` |
|    301 |  6648 | `	SXUNUSED(nArg);` |
|    301 |  6649 | `	SXUNUSED(apArg);` |
|    610 |  6650 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|    610 |  6651 | `	if( !ReflectFuncRetText(&sRef, &z, &n, zRetBuf, sizeof(zRetBuf)) ){` |
|     31 |  6652 | `		ph7_result_null(pCtx);` |
|     31 |  6653 | `		return PH7_OK;` |
|      - |  6654 | `	}` |
|    582 |  6655 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx, z, n));` |
|    306 |  6656 | `}` |
|      - |  6657 | ``/* A native method's `@`-marked zRet is php's `@tentative-return-type`: reported`` |
|      - |  6658 | ` * HERE and by nothing else, which is what separates it from a real one. */` |
|    214 |  6659 | `static int vm_builtin_ReflectionFunc_hasTentativeReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6660 | `{` |
|      - |  6661 | `	ReflectFuncRef sRef;` |
|      - |  6662 | `	const char *z;` |
|      - |  6663 | `	char zRetBuf[192];` |
|    215 |  6664 | `	int n, bTentative = 0;` |
|    107 |  6665 | `	SXUNUSED(nArg);` |
|    107 |  6666 | `	SXUNUSED(apArg);` |
|    215 |  6667 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    215 |  6668 | `	ph7_result_bool(pCtx, ReflectFuncRetTextEx(&sRef, &z, &n, &bTentative, zRetBuf, sizeof(zRetBuf)) && bTentative);` |
|    215 |  6669 | `	return PH7_OK;` |
|    108 |  6670 | `}` |
|    192 |  6671 | `static int vm_builtin_ReflectionFunc_getTentativeReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6672 | `{` |
|      - |  6673 | `	ReflectFuncRef sRef;` |
|      - |  6674 | `	const char *z;` |
|      - |  6675 | `	char zRetBuf[192];` |
|    193 |  6676 | `	int n, bTentative = 0;` |
|     96 |  6677 | `	SXUNUSED(nArg);` |
|     96 |  6678 | `	SXUNUSED(apArg);` |
|    193 |  6679 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|    193 |  6680 | `	if( !ReflectFuncRetTextEx(&sRef, &z, &n, &bTentative, zRetBuf, sizeof(zRetBuf)) \|\| !bTentative ){` |
|     19 |  6681 | `		ph7_result_null(pCtx);` |
|     19 |  6682 | `		return PH7_OK;` |
|      - |  6683 | `	}` |
|    175 |  6684 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx, z, n));` |
|     97 |  6685 | `}` |
|    116 |  6686 | `static int vm_builtin_ReflectionFunc_getNumberOfParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  6687 | `{` |
|      - |  6688 | `	ReflectFuncRef sRef;` |
|     58 |  6689 | `	SXUNUSED(nArg);` |
|     58 |  6690 | `	SXUNUSED(apArg);` |
|    121 |  6691 | `	REFLECT_FUNC_OR(sRef, ph7_result_int(pCtx, 0))` |
|    121 |  6692 | `	if( sRef.pHost && sRef.zSig == 0 ){` |
|      - |  6693 | `		/* An UNDESCRIBED builtin: the arity table is all there is. */` |
|    ! 0 |  6694 | `		ph7_result_int64(pCtx, (sxi64)sRef.pHost->nMinArg);` |
|    ! 0 |  6695 | `		return PH7_OK;` |
|      - |  6696 | `	}` |
|    121 |  6697 | `	ph7_result_int(pCtx, ReflectParamCount(&sRef));` |
|    121 |  6698 | `	return PH7_OK;` |
|     63 |  6699 | `}` |
|     98 |  6700 | `static int vm_builtin_ReflectionFunc_getNumberOfRequiredParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  6701 | `{` |
|      - |  6702 | `	ReflectFuncRef sRef;` |
|      - |  6703 | `	ReflectParamDesc sDesc;` |
|    102 |  6704 | `	int n, nTotal, nReq = 0;` |
|     49 |  6705 | `	SXUNUSED(nArg);` |
|     49 |  6706 | `	SXUNUSED(apArg);` |
|    102 |  6707 | `	REFLECT_FUNC_OR(sRef, ph7_result_int(pCtx, 0))` |
|    102 |  6708 | `	if( sRef.pHost && sRef.zSig == 0 ){` |
|    ! 0 |  6709 | `		ph7_result_int64(pCtx, (sxi64)sRef.pHost->nMinArg);` |
|    ! 0 |  6710 | `		return PH7_OK;` |
|      - |  6711 | `	}` |
|    102 |  6712 | `	nTotal = ReflectParamCount(&sRef);` |
|    192 |  6713 | `	for( n = nTotal ; n > 0 ; n-- ){` |
|    154 |  6714 | `		if( ReflectParamAt(&sRef, n - 1, &sDesc) && !sDesc.bVariadic && !sDesc.bOptional ){` |
|     64 |  6715 | `			nReq = n;` |
|     64 |  6716 | `			break;` |
|      - |  6717 | `		}` |
|     49 |  6718 | `	}` |
|    102 |  6719 | `	ph7_result_int(pCtx, nReq);` |
|    102 |  6720 | `	return PH7_OK;` |
|     53 |  6721 | `}` |
|      - |  6722 | `/* The (target, method) pair a ReflectionParameter is built over. */` |
|    908 |  6723 | `static void ReflectFuncParamSpec(ph7_context *pCtx, ph7_value *pSpec)` |
|      5 |  6724 | `{` |
|    913 |  6725 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  6726 | `	ph7_class_instance *pClo;` |
|      - |  6727 | `	const char *zName, *zClass;` |
|      - |  6728 | `	int nName, nClass;` |
|    913 |  6729 | `	PH7_MemObjInit(pCtx->pVm, pSpec);` |
|    913 |  6730 | `	if( pThis == 0 ){` |
|    ! 0 |  6731 | `		return;` |
|      - |  6732 | `	}` |
|    913 |  6733 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|    913 |  6734 | `	if( pClo && !ReflectIsMethodReflector(pCtx, pThis) ){` |
|     25 |  6735 | `		pSpec->x.pOther = pClo;` |
|     25 |  6736 | `		pSpec->iFlags = MEMOBJ_OBJ;` |
|     25 |  6737 | `		return;` |
|      - |  6738 | `	}` |
|    891 |  6739 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    891 |  6740 | `	if( ReflectIsMethodReflector(pCtx, pThis) ){` |
|      - |  6741 | ``		/* `[class, method]`, which ReflectionParameter's constructor accepts -- and`` |
|      - |  6742 | ``		 * for the fabricated `Closure::__invoke` the OBJECT stands in for the class,`` |
|      - |  6743 | `		 * because that pair is what resolves to the closure's parameter list while` |
|      - |  6744 | ``		 * still naming a method (php's parameter reports `Closure` as its declaring`` |
|      - |  6745 | ``		 * class and `__invoke` as its declaring function). */`` |
|    479 |  6746 | `		ph7_value *pList = ph7_context_new_array(pCtx);` |
|    479 |  6747 | `		ph7_value *pA = ph7_context_new_scalar(pCtx);` |
|    479 |  6748 | `		ph7_value *pB = ph7_context_new_scalar(pCtx);` |
|    479 |  6749 | `		if( pList == 0 \|\| pA == 0 \|\| pB == 0 ){` |
|    ! 0 |  6750 | `			return;` |
|      - |  6751 | `		}` |
|    479 |  6752 | `		if( pClo ){` |
|      - |  6753 | `			/* pA is a CONTEXT value: the call's teardown releases it, so stamping an` |
|      - |  6754 | `			 * object into it has to take a reference the way any other holder does.` |
|      - |  6755 | `			 * (Without it the closure was unref'd once per getParameters() call and` |
|      - |  6756 | `			 * freed under its own Closure object -- a use-after-free that surfaced` |
|      - |  6757 | `			 * three hundred files into a phpstan run.) */` |
|      5 |  6758 | `			pClo->iRef++;` |
|      5 |  6759 | `			pA->x.pOther = pClo;` |
|      5 |  6760 | `			pA->iFlags = MEMOBJ_OBJ;` |
|      3 |  6761 | `		}else{` |
|    475 |  6762 | `			PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|    475 |  6763 | `			ph7_value_string(pA, zClass, nClass);` |
|      - |  6764 | `		}` |
|    479 |  6765 | `		ph7_value_string(pB, zName, nName);` |
|    479 |  6766 | `		ph7_array_add_elem(pList, 0, pA);` |
|    479 |  6767 | `		ph7_array_add_elem(pList, 0, pB);` |
|    479 |  6768 | `		PH7_MemObjStore(pList, pSpec);` |
|    479 |  6769 | `		return;` |
|      - |  6770 | `	}` |
|    416 |  6771 | `	ph7_value_string(pSpec, zName, nName);` |
|    459 |  6772 | `}` |
|    892 |  6773 | `static int vm_builtin_ReflectionFunc_getParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  6774 | `{` |
|      - |  6775 | `	ReflectFuncRef sRef;` |
|    897 |  6776 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  6777 | `	ph7_value sSpec;` |
|      - |  6778 | `	int n, nTotal;` |
|    446 |  6779 | `	SXUNUSED(nArg);` |
|    446 |  6780 | `	SXUNUSED(apArg);` |
|    897 |  6781 | `	if( pOut == 0 ){` |
|    ! 0 |  6782 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  6783 | `	}` |
|    897 |  6784 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 |  6785 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  6786 | `		return PH7_OK;` |
|      - |  6787 | `	}` |
|    897 |  6788 | `	nTotal = ReflectParamCount(&sRef);` |
|    897 |  6789 | `	ReflectFuncParamSpec(pCtx, &sSpec);` |
|   2407 |  6790 | `	for( n = 0 ; n < nTotal ; n++ ){` |
|      - |  6791 | `		ph7_value sPos, sVal;` |
|      - |  6792 | `		ph7_value *apCtor[2];` |
|      - |  6793 | `		ph7_class_instance *pParam;` |
|      - |  6794 | `		sxi32 rc;` |
|   1515 |  6795 | `		PH7_MemObjInit(pCtx->pVm, &sPos);` |
|   1515 |  6796 | `		ph7_value_int(&sPos, n);` |
|   1515 |  6797 | `		apCtor[0] = &sSpec;` |
|   1515 |  6798 | `		apCtor[1] = &sPos;` |
|   1515 |  6799 | `		pParam = ReflectConstruct(pCtx, "ReflectionParameter", 2, apCtor, &rc);` |
|   1515 |  6800 | `		PH7_MemObjRelease(&sPos);` |
|   1515 |  6801 | `		if( pParam == 0 ){` |
|    ! 0 |  6802 | `			break;` |
|      - |  6803 | `		}` |
|   1515 |  6804 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|   1515 |  6805 | `		sVal.x.pOther = pParam;` |
|   1515 |  6806 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|   1515 |  6807 | `		ph7_array_add_elem(pOut, 0, &sVal);   /* takes its own reference */` |
|   1515 |  6808 | `		PH7_ClassInstanceUnref(pParam);` |
|    760 |  6809 | `	}` |
|    897 |  6810 | `	if( (sSpec.iFlags & MEMOBJ_OBJ) == 0 ){` |
|    879 |  6811 | `		PH7_MemObjRelease(&sSpec);` |
|    437 |  6812 | `	}` |
|    897 |  6813 | `	ph7_result_value(pCtx, pOut);` |
|    897 |  6814 | `	return PH7_OK;` |
|    451 |  6815 | `}` |
|      4 |  6816 | `static int vm_builtin_ReflectionFunc_getStaticVariables(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6817 | `{` |
|      - |  6818 | `	ReflectFuncRef sRef;` |
|      5 |  6819 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  6820 | `	ph7_vm_func_static_var *aStatic;` |
|      - |  6821 | `	sxu32 n;` |
|      2 |  6822 | `	SXUNUSED(nArg);` |
|      2 |  6823 | `	SXUNUSED(apArg);` |
|      5 |  6824 | `	if( pOut == 0 ){` |
|    ! 0 |  6825 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  6826 | `	}` |
|      5 |  6827 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pFunc == 0 ){` |
|    ! 0 |  6828 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  6829 | `		return PH7_OK;` |
|      - |  6830 | `	}` |
|      - |  6831 | ``	/* php reports a closure's captured `use` variables here TOO, ahead of the body's`` |
|      - |  6832 | `	 * own statics: it compiles both into one static-variables table, and` |
|      - |  6833 | `	 * ReflectionFunction::getStaticVariables() hands back the whole thing. PHL listed` |
|      - |  6834 | ``	 * only the `static $x` ones, so a closure's captures were invisible to reflection.`` |
|      - |  6835 | `	 *` |
|      - |  6836 | `	 * Pest reads exactly this to find the closure a test case wraps --` |
|      - |  6837 | ``	 * `Reflection::getFunctionVariable($this->__test, 'closure')` is`` |
|      - |  6838 | ``	 * `(new ReflectionFunction($f))->getStaticVariables()['closure']` -- and with the`` |
|      - |  6839 | `	 * captures missing it got null and EVERY test in a Pest suite failed on the` |
|      - |  6840 | `	 * TypeError that followed.` |
|      - |  6841 | `	 *` |
|      - |  6842 | ``	 * `$this` is not one of them: php keeps the receiver in its own slot and never`` |
|      - |  6843 | `	 * lists it as a static, while PHL carries it as an ordinary env entry. A by-REFERENCE` |
|      - |  6844 | `	 * capture reports what its slot holds NOW, not the value at closure creation. */` |
|      - |  6845 | `	{` |
|      5 |  6846 | `		ph7_vm_func_closure_env *aEnv =` |
|      4 |  6847 | `			(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|      - |  6848 | `		sxu32 k;` |
|      5 |  6849 | `		for( k = 0 ; k < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++k ){` |
|    ! 0 |  6850 | `			ph7_value *pVal = &aEnv[k].sValue;` |
|    ! 0 |  6851 | `			if( SyStringLength(&aEnv[k].sName) == sizeof("this")-1` |
|    ! 0 |  6852 | `			 && SyMemcmp(SyStringData(&aEnv[k].sName), "this", sizeof("this")-1) == 0 ){` |
|    ! 0 |  6853 | `				continue;` |
|      - |  6854 | `			}` |
|    ! 0 |  6855 | `			if( aEnv[k].nIdx != SXU32_HIGH ){` |
|    ! 0 |  6856 | `				ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, aEnv[k].nIdx);` |
|    ! 0 |  6857 | `				if( pSlot ){` |
|    ! 0 |  6858 | `					pVal = pSlot;` |
|    ! 0 |  6859 | `				}` |
|    ! 0 |  6860 | `			}` |
|    ! 0 |  6861 | `			ReflectMapAddDyn(pCtx, pOut, &aEnv[k].sName, pVal);` |
|    ! 0 |  6862 | `		}` |
|      - |  6863 | `	}` |
|      - |  6864 | `	/* Current value when the slot was initialized (first call), otherwise the` |
|      - |  6865 | `	 * evaluated default — php's getStaticVariables initializes on demand and` |
|      - |  6866 | `	 * reports the same values. */` |
|      5 |  6867 | `	aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&sRef.pFunc->aStatic);` |
|      9 |  6868 | `	for( n = 0 ; n < SySetUsed(&sRef.pFunc->aStatic) ; n++ ){` |
|      5 |  6869 | `		ph7_value *pVal = 0;` |
|      - |  6870 | `		ph7_value sScratch;` |
|      5 |  6871 | `		int bScratch = 0;` |
|      5 |  6872 | `		if( aStatic[n].nIdx != SXU32_HIGH ){` |
|      3 |  6873 | `			pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);` |
|      1 |  6874 | `		}` |
|      5 |  6875 | `		if( pVal == 0 ){` |
|      3 |  6876 | `			PH7_MemObjInit(pCtx->pVm, &sScratch);` |
|      3 |  6877 | `			if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      3 |  6878 | `				VmLocalExec(pCtx->pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|      1 |  6879 | `			}` |
|      3 |  6880 | `			pVal = &sScratch;` |
|      3 |  6881 | `			bScratch = 1;` |
|      1 |  6882 | `		}` |
|      5 |  6883 | `		ReflectMapAddDyn(pCtx, pOut, &aStatic[n].sName, pVal);` |
|      5 |  6884 | `		if( bScratch ){` |
|      3 |  6885 | `			PH7_MemObjRelease(&sScratch);` |
|      1 |  6886 | `		}` |
|      3 |  6887 | `	}` |
|      5 |  6888 | `	ph7_result_value(pCtx, pOut);` |
|      5 |  6889 | `	return PH7_OK;` |
|      3 |  6890 | `}` |
|      - |  6891 | ``/* One `key => value` entry of a presented shape, by name. */`` |
|     96 |  6892 | `static void ClosurePresentAdd(ph7_vm *pVm, ph7_value *pOut, const char *zKey, ph7_value *pVal)` |
|      2 |  6893 | `{` |
|      - |  6894 | `	ph7_value sKey;` |
|     98 |  6895 | `	PH7_MemObjInitFromString(pVm, &sKey, 0);` |
|     98 |  6896 | `	PH7_MemObjStringAppend(&sKey, zKey, (sxu32)SyStrlen(zKey));` |
|     98 |  6897 | `	ph7_array_add_elem(pOut, &sKey, pVal);   /* takes its OWN reference */` |
|     98 |  6898 | `	PH7_MemObjRelease(&sKey);` |
|     98 |  6899 | `}` |
|      - |  6900 | `/*` |
|      - |  6901 | ` * php's DEBUG presentation for a Closure (ph7_class::xPresent) —` |
|      - |  6902 | `` * `zend_closure_get_debug_info` (Zend/zend_closures.c), key for key and in php's`` |
|      - |  6903 | `` * order. Every key is CONDITIONAL, which is why a plain `function(){}` shows three`` |
|      - |  6904 | ` * and a bound one with captures shows five:` |
|      - |  6905 | ` *` |
|      - |  6906 | ` *   - a FAKE closure (php's ZEND_ACC_FAKE_CLOSURE — a first-class callable or` |
|      - |  6907 | `` *     `Closure::fromCallable` over a NAMED function) shows one `function` key,`` |
|      - |  6908 | `` *     `Class::method` when it carries a scope and the bare name otherwise, and NO`` |
|      - |  6909 | `` *     name/file/line. A real closure shows `name` (php's `{closure:SCOPE:LINE}`,`` |
|      - |  6910 | `` *     which `PH7_VmFuncDisplayName` answers), `file` and an INT `line`;`` |
|      - |  6911 | `` *   - `static` is the closure's own variable state — the `use` captures AND the`` |
|      - |  6912 | `` *     body's `static $x` — keyed WITHOUT the `$`, and omitted when empty. php`` |
|      - |  6913 | ` *     shows it before the first call too, since the initializers are compiled;` |
|      - |  6914 | `` *   - `this` is the bound receiver, present only when there is one (`bindTo`,`` |
|      - |  6915 | ` *     an instance first-class callable, or the implicit capture in a method);` |
|      - |  6916 | `` *   - `parameter` is `"$name" => "<required>"\|"<optional>"`, `&$name` for a by-ref`` |
|      - |  6917 | ` *     parameter, omitted when the callee takes none. php's cut is` |
|      - |  6918 | `` *     `required_num_args`, which counts through the LAST required parameter — so`` |
|      - |  6919 | `` *     `function($a, $b = 1, $c)` reports all THREE as `<required>`, not two.`` |
|      - |  6920 | ` *` |
|      - |  6921 | `` * The non-debug half answers nothing: php's `get_properties` for a Closure is an`` |
|      - |  6922 | `` * empty table, and `(array)$closure` never reaches it at all (php special-cases a`` |
|      - |  6923 | `` * Closure in `convert_to_array` and wraps it as a SCALAR, `[0 => $closure]`; PHL`` |
|      - |  6924 | `` * answers `[]` there — recorded).`` |
|      - |  6925 | ` */` |
|      - |  6926 | `/*` |
|      - |  6927 | ` * A call TRAMPOLINE -- a callable over a name the class answers only through __call or` |
|      - |  6928 | ` * __callStatic -- has no body for the reflector to resolve, and php still describes it: its` |
|      - |  6929 | `` * function IS the catch-all, carrying the NAME that was asked for, so `function` is`` |
|      - |  6930 | `` * `DECLARING_CLASS::name`; `this` is the receiver the __call one binds; and `parameter` is the`` |
|      - |  6931 | ``  * signature php gives the trampoline, which is one variadic `$arguments` when the `(...)` `` |
|      - |  6932 | ` * syntax minted it and none at all through Closure::fromCallable().` |
|      - |  6933 | ` */` |
|     20 |  6934 | `static void ClosurePresentTrampoline(ph7_vm *pVm, ph7_class_instance *pThis, ph7_value *pOut)` |
|      2 |  6935 | `{` |
|     22 |  6936 | `	ph7_class *pScope = PH7_VmClosureScopeClass(pVm, pThis);` |
|      - |  6937 | `	ph7_value *pFn, *pBound, sVal;` |
|      - |  6938 | `	SyString sAttr;` |
|     22 |  6939 | `	SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|     22 |  6940 | `	pFn = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     22 |  6941 | `	if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  6942 | `		return;` |
|      - |  6943 | `	}` |
|     22 |  6944 | `	PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|     22 |  6945 | `	if( pScope ){` |
|     22 |  6946 | `		PH7_MemObjStringAppend(&sVal, SyStringData(&pScope->sName), SyStringLength(&pScope->sName));` |
|     22 |  6947 | `		PH7_MemObjStringAppend(&sVal, "::", 2);` |
|     10 |  6948 | `	}` |
|     22 |  6949 | `	PH7_MemObjStringAppend(&sVal, (const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|     22 |  6950 | `	ClosurePresentAdd(pVm, pOut, "function", &sVal);` |
|     22 |  6951 | `	PH7_MemObjRelease(&sVal);` |
|     22 |  6952 | `	SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     22 |  6953 | `	pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     22 |  6954 | `	if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|     13 |  6955 | `		ClosurePresentAdd(pVm, pOut, "this", pBound);` |
|      6 |  6956 | `	}` |
|     22 |  6957 | `	if( pThis->iFlags & VM_INSTANCE_FCC_SYNTAX ){` |
|     16 |  6958 | `		ph7_hashmap *pHm = PH7_NewHashmap(pVm, 0, 0);` |
|     16 |  6959 | `		if( pHm ){` |
|      - |  6960 | `			ph7_value sMap, sKey, sWhat;` |
|     16 |  6961 | `			PH7_MemObjInitFromArray(pVm, &sMap, pHm);` |
|     16 |  6962 | `			PH7_MemObjInitFromString(pVm, &sKey, 0);` |
|     16 |  6963 | `			PH7_MemObjStringAppend(&sKey, "$arguments", sizeof("$arguments")-1);` |
|     16 |  6964 | `			PH7_MemObjInitFromString(pVm, &sWhat, 0);` |
|     16 |  6965 | `			PH7_MemObjStringAppend(&sWhat, "<optional>", sizeof("<optional>")-1);` |
|     16 |  6966 | `			ph7_array_add_elem(&sMap, &sKey, &sWhat);` |
|     16 |  6967 | `			PH7_MemObjRelease(&sKey);` |
|     16 |  6968 | `			PH7_MemObjRelease(&sWhat);` |
|     16 |  6969 | `			ClosurePresentAdd(pVm, pOut, "parameter", &sMap);` |
|     16 |  6970 | `			PH7_MemObjRelease(&sMap);` |
|      7 |  6971 | `		}` |
|      7 |  6972 | `	}` |
|     12 |  6973 | `}` |
|     50 |  6974 | `PH7_PRIVATE sxi32 PH7_ClosurePresent(ph7_vm *pVm, ph7_class_instance *pThis,` |
|      - |  6975 | `	ph7_value *pOut, int bDebug)` |
|      4 |  6976 | `{` |
|      - |  6977 | `	ReflectFuncRef sRef;` |
|      - |  6978 | `	ph7_value sCarrier, sVal;` |
|     54 |  6979 | `	int bFake = 1;` |
|     54 |  6980 | `	if( !bDebug \|\| pThis == 0 ){` |
|     17 |  6981 | `		return SXRET_OK;` |
|      - |  6982 | `	}` |
|      - |  6983 | `	/* Rule 16's other half: this carrier never took a reference, so it must be` |
|      - |  6984 | `	 * blanked before it is released or the Closure is unref'd a second time. */` |
|     38 |  6985 | `	PH7_MemObjInit(pVm, &sCarrier);` |
|     38 |  6986 | `	sCarrier.x.pOther = pThis;` |
|     38 |  6987 | `	MemObjSetType(&sCarrier, MEMOBJ_OBJ);` |
|     38 |  6988 | `	if( !ReflectFuncFill(pVm, &sCarrier, 0, &sRef) \|\| sRef.bTrampoline ){` |
|     22 |  6989 | `		sCarrier.x.pOther = 0;` |
|     22 |  6990 | `		sCarrier.iFlags = MEMOBJ_NULL;` |
|     22 |  6991 | `		PH7_MemObjRelease(&sCarrier);` |
|     20 |  6992 | `		if( (pThis->iFlags & (VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SCREENED))` |
|     12 |  6993 | `				== VM_INSTANCE_FCC_METHOD ){` |
|     22 |  6994 | `			ClosurePresentTrampoline(pVm, pThis, pOut);` |
|     10 |  6995 | `		}` |
|     22 |  6996 | `		return SXRET_OK;` |
|      - |  6997 | `	}` |
|     17 |  6998 | `	sCarrier.x.pOther = 0;` |
|     17 |  6999 | `	sCarrier.iFlags = MEMOBJ_NULL;` |
|     17 |  7000 | `	PH7_MemObjRelease(&sCarrier);` |
|      - |  7001 | `	/* php's FAKE-closure bit, read off what the callable actually resolved TO: a` |
|      - |  7002 | `	 * real closure's body is the anonymous function itself (the compiler's` |
|      - |  7003 | ``	 * `{closure:...}` name, or the synthesized key that stands in for it). */`` |
|     17 |  7004 | `	if( sRef.pFunc && sRef.pMeth == 0 ){` |
|      9 |  7005 | `		const SyString *pN = &sRef.pFunc->sName;` |
|      8 |  7006 | `		if( SyStringLength(&sRef.pFunc->sClosureName) > 0` |
|      4 |  7007 | `		 \|\| (pN->nByte > 8 && SyMemcmp(pN->zString, "[lambda_", 8) == 0)` |
|      1 |  7008 | `		 \|\| (pN->nByte > 9 && SyMemcmp(pN->zString, "[closure_", 9) == 0) ){` |
|      9 |  7009 | `			bFake = 0;` |
|      4 |  7010 | `		}` |
|      4 |  7011 | `	}` |
|     17 |  7012 | `	PH7_MemObjInit(pVm, &sVal);` |
|     17 |  7013 | `	if( bFake ){` |
|      9 |  7014 | `		ph7_class *pScope = ReflectFuncDeclClass(&sRef);` |
|      9 |  7015 | `		const SyString *pName = sRef.pFunc ? &sRef.pFunc->sName : &sRef.pHost->sName;` |
|      9 |  7016 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|      9 |  7017 | `		if( pScope == 0 && sRef.pClass ){` |
|    ! 0 |  7018 | `			pScope = sRef.pClass;` |
|    ! 0 |  7019 | `		}` |
|      9 |  7020 | `		if( pScope ){` |
|     10 |  7021 | `			PH7_MemObjStringAppend(&sVal, SyStringData(&pScope->sName),` |
|      3 |  7022 | `				SyStringLength(&pScope->sName));` |
|      7 |  7023 | `			PH7_MemObjStringAppend(&sVal, "::", 2);` |
|      3 |  7024 | `		}` |
|      9 |  7025 | `		PH7_MemObjStringAppend(&sVal, SyStringData(pName), SyStringLength(pName));` |
|      9 |  7026 | `		ClosurePresentAdd(pVm, pOut, "function", &sVal);` |
|      9 |  7027 | `		PH7_MemObjRelease(&sVal);` |
|      5 |  7028 | `	}else{` |
|      - |  7029 | `		const char *zShow;` |
|      9 |  7030 | `		int nShow = PH7_VmFuncDisplayName(pVm, sRef.pFunc, &zShow);` |
|      9 |  7031 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|      9 |  7032 | `		PH7_MemObjStringAppend(&sVal, zShow, (sxu32)(nShow > 0 ? nShow : 0));` |
|      9 |  7033 | `		ClosurePresentAdd(pVm, pOut, "name", &sVal);` |
|      9 |  7034 | `		PH7_MemObjRelease(&sVal);` |
|      9 |  7035 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|     13 |  7036 | `		PH7_MemObjStringAppend(&sVal, SyStringData(&sRef.pFunc->sFile),` |
|      8 |  7037 | `			SyStringLength(&sRef.pFunc->sFile));` |
|      9 |  7038 | `		ClosurePresentAdd(pVm, pOut, "file", &sVal);` |
|      9 |  7039 | `		PH7_MemObjRelease(&sVal);` |
|      9 |  7040 | `		PH7_MemObjInitFromInt(pVm, &sVal, (sxi64)sRef.pFunc->nLine);` |
|      9 |  7041 | `		ClosurePresentAdd(pVm, pOut, "line", &sVal);` |
|      9 |  7042 | `		PH7_MemObjRelease(&sVal);` |
|      - |  7043 | `	}` |
|      - |  7044 | ``	/* `static`: the captured `use` variables first (php compiles them into the`` |
|      - |  7045 | ``	 * same table, ahead of the body's own statics), then the body's `static $x`,`` |
|      - |  7046 | `	 * whose value is the live slot once it exists and the compiled initializer` |
|      - |  7047 | `	 * before that — the rule getStaticVariables() already follows. */` |
|     17 |  7048 | `	if( sRef.pFunc ){` |
|     15 |  7049 | `		ph7_hashmap *pHm = PH7_NewHashmap(pVm, 0, 0);` |
|      - |  7050 | `		sxu32 n;` |
|     15 |  7051 | `		if( pHm ){` |
|      - |  7052 | `			ph7_value sMap;` |
|     15 |  7053 | `			ph7_value *pMap = &sMap;` |
|     15 |  7054 | `			ph7_vm_func_closure_env *aEnv =` |
|     14 |  7055 | `				(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|     15 |  7056 | `			ph7_vm_func_static_var *aStatic =` |
|     14 |  7057 | `				(ph7_vm_func_static_var *)SySetBasePtr(&sRef.pFunc->aStatic);` |
|     15 |  7058 | `			PH7_MemObjInitFromArray(pVm, pMap, pHm);` |
|     27 |  7059 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++n ){` |
|     13 |  7060 | `				ph7_value *pVal = &aEnv[n].sValue;` |
|      - |  7061 | `				ph7_value sKey;` |
|     12 |  7062 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|     11 |  7063 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|      - |  7064 | ``					/* PHL carries the auto-captured `$this` as an env entry; php keeps`` |
|      - |  7065 | ``					 * it in its own `this_ptr` slot and never lists it as a static. It`` |
|      - |  7066 | ``					 * is reported below, under `this`. */`` |
|      9 |  7067 | `					continue;` |
|      - |  7068 | `				}` |
|      5 |  7069 | `				if( aEnv[n].nIdx != SXU32_HIGH ){` |
|      - |  7070 | ``					/* `use (&$x)`: the capture is an alias onto a pinned slot, and`` |
|      - |  7071 | `					 * php reports what the slot holds NOW, not the birth value. */` |
|    ! 0 |  7072 | `					ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, aEnv[n].nIdx);` |
|    ! 0 |  7073 | `					if( pSlot ){` |
|    ! 0 |  7074 | `						pVal = pSlot;` |
|    ! 0 |  7075 | `					}` |
|    ! 0 |  7076 | `				}` |
|      5 |  7077 | `				PH7_MemObjInitFromString(pVm, &sKey, &aEnv[n].sName);` |
|      5 |  7078 | `				ph7_array_add_elem(pMap, &sKey, pVal);` |
|      5 |  7079 | `				PH7_MemObjRelease(&sKey);` |
|      3 |  7080 | `			}` |
|     17 |  7081 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aStatic) ; ++n ){` |
|      3 |  7082 | `				ph7_value *pVal = 0;` |
|      - |  7083 | `				ph7_value sScratch, sKey;` |
|      3 |  7084 | `				int bScratch = 0;` |
|      3 |  7085 | `				if( aStatic[n].nIdx != SXU32_HIGH ){` |
|    ! 0 |  7086 | `					pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, aStatic[n].nIdx);` |
|    ! 0 |  7087 | `				}` |
|      3 |  7088 | `				if( pVal == 0 ){` |
|      3 |  7089 | `					PH7_MemObjInit(pVm, &sScratch);` |
|      3 |  7090 | `					if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      3 |  7091 | `						VmLocalExec(pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|      1 |  7092 | `					}` |
|      3 |  7093 | `					pVal = &sScratch;` |
|      3 |  7094 | `					bScratch = 1;` |
|      1 |  7095 | `				}` |
|      3 |  7096 | `				PH7_MemObjInitFromString(pVm, &sKey, &aStatic[n].sName);` |
|      3 |  7097 | `				ph7_array_add_elem(pMap, &sKey, pVal);` |
|      3 |  7098 | `				PH7_MemObjRelease(&sKey);` |
|      3 |  7099 | `				if( bScratch ){` |
|      3 |  7100 | `					PH7_MemObjRelease(&sScratch);` |
|      1 |  7101 | `				}` |
|      2 |  7102 | `			}` |
|     15 |  7103 | `			if( ph7_array_count(pMap) > 0 ){` |
|      5 |  7104 | `				ClosurePresentAdd(pVm, pOut, "static", pMap);` |
|      2 |  7105 | `			}` |
|     15 |  7106 | `			PH7_MemObjRelease(pMap);` |
|      7 |  7107 | `		}` |
|      7 |  7108 | `	}` |
|      - |  7109 | ``	/* `this`: the bound receiver. php keeps ONE `this_ptr` however the binding`` |
|      - |  7110 | ``	 * happened; PHL keeps two — an explicit bind (`bindTo`, an instance`` |
|      - |  7111 | ``	 * first-class callable) writes the `$__this` slot, while the IMPLICIT capture a`` |
|      - |  7112 | `	 * closure written inside a method gets rides in the closure ENVIRONMENT under` |
|      - |  7113 | ``	 * the name `this`. Either one is php's answer here. */`` |
|      - |  7114 | `	{` |
|      - |  7115 | `		SyString sAttr;` |
|      - |  7116 | `		ph7_value *pBound;` |
|     17 |  7117 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     17 |  7118 | `		pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     17 |  7119 | `		if( (pBound == 0 \|\| (pBound->iFlags & MEMOBJ_OBJ) == 0) && sRef.pFunc ){` |
|     11 |  7120 | `			ph7_vm_func_closure_env *aEnv =` |
|     10 |  7121 | `				(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|      - |  7122 | `			sxu32 n;` |
|     11 |  7123 | `			pBound = 0;` |
|     15 |  7124 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++n ){` |
|     12 |  7125 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|     11 |  7126 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|      9 |  7127 | `					pBound = &aEnv[n].sValue;` |
|      9 |  7128 | `					break;` |
|      - |  7129 | `				}` |
|      3 |  7130 | `			}` |
|      5 |  7131 | `		}` |
|     17 |  7132 | `		if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|      7 |  7133 | `			ClosurePresentAdd(pVm, pOut, "this", pBound);` |
|      3 |  7134 | `		}` |
|      - |  7135 | `	}` |
|      - |  7136 | ``	/* `parameter`: php's cut is required_num_args, which runs through the LAST`` |
|      - |  7137 | `	 * required parameter rather than stopping at the first optional one. */` |
|      - |  7138 | `	{` |
|      - |  7139 | `		ReflectParamDesc sDesc;` |
|     17 |  7140 | `		int nArgTotal = 0, nRequired = 0, i;` |
|     33 |  7141 | `		while( ReflectParamAt(&sRef, nArgTotal, &sDesc) ){` |
|     17 |  7142 | `			if( !sDesc.bOptional ){` |
|     11 |  7143 | `				nRequired = nArgTotal + 1;` |
|      5 |  7144 | `			}` |
|     17 |  7145 | `			nArgTotal++;` |
|      1 |  7146 | `		}` |
|     17 |  7147 | `		if( nArgTotal > 0 ){` |
|      9 |  7148 | `			ph7_hashmap *pHm = PH7_NewHashmap(pVm, 0, 0);` |
|      9 |  7149 | `			if( pHm ){` |
|      - |  7150 | `				ph7_value sMap;` |
|      9 |  7151 | `				ph7_value *pMap = &sMap;` |
|      9 |  7152 | `				PH7_MemObjInitFromArray(pVm, pMap, pHm);` |
|     25 |  7153 | `				for( i = 0 ; i < nArgTotal ; ++i ){` |
|      - |  7154 | `					ph7_value sKey, sWhat;` |
|     17 |  7155 | `					if( !ReflectParamAt(&sRef, i, &sDesc) ){` |
|    ! 0 |  7156 | `						break;` |
|      - |  7157 | `					}` |
|     17 |  7158 | `					PH7_MemObjInitFromString(pVm, &sKey, 0);` |
|     17 |  7159 | `					if( sDesc.bByRef ){` |
|      3 |  7160 | `						PH7_MemObjStringAppend(&sKey, "&", 1);` |
|      1 |  7161 | `					}` |
|     17 |  7162 | `					PH7_MemObjStringAppend(&sKey, "$", 1);` |
|     25 |  7163 | `					PH7_MemObjStringAppend(&sKey, SyStringData(&sDesc.sName),` |
|      8 |  7164 | `						SyStringLength(&sDesc.sName));` |
|     17 |  7165 | `					PH7_MemObjInitFromString(pVm, &sWhat, 0);` |
|     17 |  7166 | `					PH7_MemObjStringAppend(&sWhat,` |
|      8 |  7167 | `						i >= nRequired ? "<optional>" : "<required>",` |
|      8 |  7168 | `						i >= nRequired ? sizeof("<optional>")-1 : sizeof("<required>")-1);` |
|     17 |  7169 | `					ph7_array_add_elem(pMap, &sKey, &sWhat);` |
|     17 |  7170 | `					PH7_MemObjRelease(&sKey);` |
|     17 |  7171 | `					PH7_MemObjRelease(&sWhat);` |
|      9 |  7172 | `				}` |
|      9 |  7173 | `				ClosurePresentAdd(pVm, pOut, "parameter", pMap);` |
|      9 |  7174 | `				PH7_MemObjRelease(pMap);` |
|      4 |  7175 | `			}` |
|      4 |  7176 | `		}` |
|      - |  7177 | `	}` |
|     17 |  7178 | `	return SXRET_OK;` |
|     29 |  7179 | `}` |
|      - |  7180 | `/*` |
|      - |  7181 | ` * A METHOD belongs to the extension its DECLARING class does -- php reports SPL` |
|      - |  7182 | ` * for a method a userland subclass inherited from ArrayObject -- and a plain` |
|      - |  7183 | ` * function to the one the partition places its own name in.` |
|      - |  7184 | ` */` |
|   4955 |  7185 | `static int ReflectFuncExtId(const ReflectFuncRef *pRef)` |
|      4 |  7186 | `{` |
|      - |  7187 | `	const SyString *pName;` |
|   4959 |  7188 | `	if( !ReflectFuncIsInternal(pRef) \|\| pRef->bFabricated \|\| pRef->bTrampoline ){` |
|      - |  7189 | `		/* A fabricated method belongs to no module: php answers false for` |
|      - |  7190 | `		 * getExtensionName() and NULL for getExtension(), and prints a bare` |
|      - |  7191 | ``		 * `<internal>` in the export. */`` |
|    125 |  7192 | `		return -1;` |
|      - |  7193 | `	}` |
|   4837 |  7194 | `	if( pRef->pMeth ){` |
|   4283 |  7195 | `		return ReflectClassExtId(ReflectFuncDeclClass(pRef));` |
|      - |  7196 | `	}` |
|    557 |  7197 | `	pName = pRef->pHost ? &pRef->pHost->sName : &pRef->pFunc->sName;` |
|    557 |  7198 | `	return PH7_VmExtOfFunc(SyStringData(pName), (int)SyStringLength(pName));` |
|   2481 |  7199 | `}` |
|     55 |  7200 | `static int vm_builtin_ReflectionFunc_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  7201 | `{` |
|      - |  7202 | `	ReflectFuncRef sRef;` |
|      - |  7203 | `	int iExt;` |
|     27 |  7204 | `	SXUNUSED(nArg);` |
|     27 |  7205 | `	SXUNUSED(apArg);` |
|     59 |  7206 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     59 |  7207 | `	iExt = ReflectFuncExtId(&sRef);` |
|     59 |  7208 | `	if( iExt < 0 ){` |
|     15 |  7209 | `		ph7_result_bool(pCtx, 0);` |
|      9 |  7210 | `	}else{` |
|     46 |  7211 | `		ph7_result_string(pCtx, PH7_VmExtensionName(iExt), -1);` |
|      - |  7212 | `	}` |
|     59 |  7213 | `	return PH7_OK;` |
|     31 |  7214 | `}` |
|     10 |  7215 | `static int vm_builtin_ReflectionFunc_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7216 | `{` |
|      - |  7217 | `	ReflectFuncRef sRef;` |
|      5 |  7218 | `	SXUNUSED(nArg);` |
|      5 |  7219 | `	SXUNUSED(apArg);` |
|     12 |  7220 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|     12 |  7221 | `	return ReflectExtensionOf(pCtx, ReflectFuncExtId(&sRef));` |
|      7 |  7222 | `}` |
|     84 |  7223 | `static int vm_builtin_ReflectionFunc_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  7224 | `{` |
|     87 |  7225 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7226 | `	ReflectFuncRef sRef;` |
|      - |  7227 | `	int rc;` |
|     87 |  7228 | `	if( pThis == 0 \|\| !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pFunc == 0 ){` |
|    ! 0 |  7229 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  7230 | `		return PH7_OK;` |
|      - |  7231 | `	}` |
|     87 |  7232 | `	if( sRef.pMeth ){` |
|      - |  7233 | `		/* 4 = Attribute::TARGET_METHOD */` |
|      - |  7234 | `		ph7_value sTarget;` |
|      - |  7235 | `		const char *zClass, *zName;` |
|      - |  7236 | `		int nClass, nName;` |
|     71 |  7237 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     71 |  7238 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     71 |  7239 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     71 |  7240 | `		ph7_value_string(&sTarget, zClass, nClass);` |
|    105 |  7241 | `		rc = ReflectBuildAttrs(pCtx, &sRef.pFunc->aAttrs, "method", &sTarget,` |
|     34 |  7242 | `			zName, nName, 0, 4, nArg, apArg);` |
|     71 |  7243 | `		PH7_MemObjRelease(&sTarget);` |
|     71 |  7244 | `		return rc;` |
|      - |  7245 | `	}` |
|      - |  7246 | `	{` |
|      - |  7247 | `		/* 2 = Attribute::TARGET_FUNCTION */` |
|      - |  7248 | `		ph7_value sTarget;` |
|     19 |  7249 | `		ReflectFuncParamSpec(pCtx, &sTarget);` |
|     27 |  7250 | `		rc = ReflectBuildAttrs(pCtx, &sRef.pFunc->aAttrs, "fn", &sTarget, 0, 0, 0, 2,` |
|      8 |  7251 | `			nArg, apArg);` |
|     19 |  7252 | `		if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|     15 |  7253 | `			PH7_MemObjRelease(&sTarget);` |
|      6 |  7254 | `		}` |
|     19 |  7255 | `		return rc;` |
|      - |  7256 | `	}` |
|     45 |  7257 | `}` |
|      - |  7258 | `/* __toString(): php's export format, still chunk 9. */` |
|    526 |  7259 | `static int vm_builtin_ReflectionFunc_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  7260 | `{` |
|    263 |  7261 | `	SXUNUSED(nArg);` |
|    263 |  7262 | `	SXUNUSED(apArg);` |
|    530 |  7263 | `	return ReflectExportFuncSelf(pCtx);` |
|      4 |  7264 | `}` |
|      - |  7265 | `/* ---- ReflectionFunction ---- */` |
|      - |  7266 | `/* ReflectionFunction::__construct(Closure\|string $function) */` |
|   1252 |  7267 | `static int vm_builtin_ReflectionFunction_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  7268 | `{` |
|   1257 |  7269 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1257 |  7270 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7271 | `	ReflectFuncRef sRef;` |
|      - |  7272 | `	ph7_class_instance *pClo;` |
|   1257 |  7273 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 |  7274 | `		return PH7_OK;` |
|      - |  7275 | `	}` |
|   1257 |  7276 | `	pClo = ReflectValueClosure(pVm, apArg[0]);` |
|   1257 |  7277 | `	if( !ReflectFuncFill(pCtx->pVm, apArg[0], 0, &sRef) \|\| sRef.bTrampoline ){` |
|      - |  7278 | `		const char *zName;` |
|      - |  7279 | `		int nName;` |
|    118 |  7280 | `		if( pClo ){` |
|      - |  7281 | `			/* A Closure whose body no table holds -- php's magic-method TRAMPOLINE` |
|      - |  7282 | `` 			 * (`Closure::fromCallable([$o, 'zz'])` where the class reaches `zz` `` |
|      - |  7283 | `			 * only through __call) is the shape real libraries hit. php still` |
|      - |  7284 | `			 * NAMES it: the name is the one that was asked for, which the Closure` |
|      - |  7285 | `` 			 * carries in $__fn, and the scope is $__scope. Leaving `name` `` |
|      - |  7286 | `			 * uninitialized made every read of it a typed-property Error, and` |
|      - |  7287 | ``			 * `str_contains($r->name, '{closure')` is one line of twig's filter`` |
|      - |  7288 | `			 * compiler. The accessors describe the internal record php builds for` |
|      - |  7289 | `			 * it (ReflectFuncFill); there is no file and no line. */` |
|      - |  7290 | `			SyString sAttr;` |
|      - |  7291 | `			ph7_value *pFn;` |
|    102 |  7292 | `			PH7_NativeSetAttrObj(pVm, pThis, RF_CL, pClo);` |
|    102 |  7293 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    102 |  7294 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|    102 |  7295 | `			if( pFn && (pFn->iFlags & MEMOBJ_STRING) && SyBlobLength(&pFn->sBlob) > 0 ){` |
|    151 |  7296 | `				PH7_NativeSetAttrStr(pVm, pThis, "name",` |
|     98 |  7297 | `					(const char *)SyBlobData(&pFn->sBlob), (int)SyBlobLength(&pFn->sBlob));` |
|     49 |  7298 | `			}` |
|    102 |  7299 | `			return PH7_OK;` |
|      - |  7300 | `		}` |
|     19 |  7301 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|     27 |  7302 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      8 |  7303 | `			"Function %.*s() does not exist", nName, zName);` |
|      - |  7304 | `	}` |
|   1143 |  7305 | `	if( pClo ){` |
|    231 |  7306 | `		PH7_NativeSetAttrObj(pVm, pThis, RF_CL, pClo);` |
|    113 |  7307 | `	}` |
|   1143 |  7308 | `	if( sRef.pFunc ){` |
|    479 |  7309 | `		int bAnon = (sRef.pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|    474 |  7310 | `		if( !bAnon && SyStringLength(&sRef.pFunc->sName) > 9` |
|    212 |  7311 | `		 && (SyMemcmp(SyStringData(&sRef.pFunc->sName), "[lambda_", 8) == 0` |
|     67 |  7312 | `		  \|\| SyMemcmp(SyStringData(&sRef.pFunc->sName), "[closure_", 9) == 0) ){` |
|      9 |  7313 | `			bAnon = 1;` |
|      3 |  7314 | `		}` |
|    479 |  7315 | `		if( bAnon ){` |
|      - |  7316 | ``			/* php 8.4 names an anonymous function `{closure:SCOPE:LINE}`, where SCOPE is`` |
|      - |  7317 | `			 * the enclosing function and only the FILE at top level — the compiler` |
|      - |  7318 | `			 * recorded it (sClosureName); the file form is the fallback for a closure` |
|      - |  7319 | `			 * built without one. */` |
|      - |  7320 | `			char zBuf[512];` |
|    141 |  7321 | `			const char *zShow = SyStringData(&sRef.pFunc->sClosureName);` |
|    141 |  7322 | `			int nShow = (int)SyStringLength(&sRef.pFunc->sClosureName);` |
|    141 |  7323 | `			if( nShow < 1 ){` |
|    ! 0 |  7324 | `				const char *zFile = SyStringLength(&sRef.pFunc->sFile) > 0` |
|    ! 0 |  7325 | `					? SyStringData(&sRef.pFunc->sFile) : "";` |
|    ! 0 |  7326 | `				int nFile = (int)SyStringLength(&sRef.pFunc->sFile);` |
|    ! 0 |  7327 | `				nShow = SyBufferFormat(zBuf, sizeof(zBuf), "{closure:%.*s:%u}",` |
|    ! 0 |  7328 | `					nFile, zFile, sRef.pFunc->nLine);` |
|    ! 0 |  7329 | `				zShow = zBuf;` |
|    ! 0 |  7330 | `			}` |
|    141 |  7331 | `			PH7_NativeSetAttrStr(pVm, pThis, "name", zShow, nShow);` |
|    141 |  7332 | `			if( pClo == 0 ){` |
|      - |  7333 | `				/* Named by its lambda name: keep a Closure so the accessors can` |
|      - |  7334 | `				 * still reach the captured scope. */` |
|    ! 0 |  7335 | `				PH7_NativeSetAttrObj(pVm, pThis, RF_CL,` |
|    ! 0 |  7336 | `					PH7_VmNewClosure(pVm, &sRef.pFunc->sName, 0, 0));` |
|    ! 0 |  7337 | `			}` |
|    141 |  7338 | `			return PH7_OK;` |
|      - |  7339 | `		}` |
|    512 |  7340 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sRef.pFunc->sName),` |
|    338 |  7341 | `			(int)SyStringLength(&sRef.pFunc->sName));` |
|    343 |  7342 | `		return PH7_OK;` |
|      - |  7343 | `	}` |
|    999 |  7344 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sRef.pHost->sName),` |
|    664 |  7345 | `		(int)SyStringLength(&sRef.pHost->sName));` |
|    669 |  7346 | `	return PH7_OK;` |
|    629 |  7347 | `}` |
|    ! 0 |  7348 | `static int vm_builtin_ReflectionFunction_isDisabled(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  7349 | `{` |
|    ! 0 |  7350 | `	SXUNUSED(nArg);` |
|    ! 0 |  7351 | `	SXUNUSED(apArg);` |
|    ! 0 |  7352 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 |  7353 | `	return PH7_OK;` |
|    ! 0 |  7354 | `}` |
|      - |  7355 | `/* The callable a ReflectionFunction invokes: the Closure it holds, or its name. */` |
|     92 |  7356 | `static void ReflectFunctionCallable(ph7_context *pCtx, ph7_value *pOut)` |
|      5 |  7357 | `{` |
|     97 |  7358 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7359 | `	ph7_class_instance *pClo;` |
|     97 |  7360 | `	const char *zName = "";` |
|     97 |  7361 | `	int nName = 0;` |
|     97 |  7362 | `	PH7_MemObjInit(pCtx->pVm, pOut);` |
|     97 |  7363 | `	if( pThis == 0 ){` |
|    ! 0 |  7364 | `		return;` |
|      - |  7365 | `	}` |
|     97 |  7366 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|     97 |  7367 | `	if( pClo ){` |
|     13 |  7368 | `		pOut->x.pOther = pClo;` |
|     13 |  7369 | `		pOut->iFlags = MEMOBJ_OBJ;` |
|     13 |  7370 | `		return;` |
|      - |  7371 | `	}` |
|     85 |  7372 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     85 |  7373 | `	ph7_value_string(pOut, zName, nName);` |
|     51 |  7374 | `}` |
|      - |  7375 | `/*` |
|      - |  7376 | ` * php reaches the target through zend_call_function, which binds a by-REFERENCE` |
|      - |  7377 | `` * parameter only to a reference: invoke()'s own `...$args` are by value, so every`` |
|      - |  7378 | ` * by-ref formal warns and gets a copy (bCopy re-marks apCall so it does), and an` |
|      - |  7379 | ` * invokeArgs() element warns unless it is itself a reference (apNode), the rule` |
|      - |  7380 | ` * call_user_func_array() already follows.` |
|      - |  7381 | ` */` |
|     92 |  7382 | `static int ReflectFunctionInvoke(ph7_context *pCtx, int nCall, ph7_value **apCall,` |
|      - |  7383 | `	VmCallArgMap *pMap, ph7_hashmap_node **apNode, int bCopy)` |
|      5 |  7384 | `{` |
|     97 |  7385 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  7386 | `	ph7_value sTarget, sResult;` |
|      - |  7387 | `	sxi32 rc;` |
|     97 |  7388 | `	ReflectFunctionCallable(pCtx, &sTarget);` |
|    143 |  7389 | `	rc = PH7_VmByRefArgsGivenValue(pVm, 0, 0, &sTarget, nCall, bCopy ? apCall : 0, apNode,` |
|     46 |  7390 | `		pMap ? pMap->aNames : 0);` |
|     97 |  7391 | `	if( rc != SXRET_OK ){` |
|      5 |  7392 | `		if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|      5 |  7393 | `			PH7_MemObjRelease(&sTarget);` |
|      2 |  7394 | `		}` |
|      5 |  7395 | `		return rc;` |
|      - |  7396 | `	}` |
|     93 |  7397 | `	PH7_MemObjInit(pVm, &sResult);` |
|     93 |  7398 | `	sResult.nIdx = SXU32_HIGH;` |
|     93 |  7399 | `	pVm->bCallbackWeak = 1;` |
|     93 |  7400 | `	rc = PH7_VmCallUserFunctionWithMap(pVm, &sTarget, nCall, apCall, &sResult, pMap);` |
|     93 |  7401 | `	pVm->bCallbackWeak = 0; /* clear if the dispatch never reached an OP_CALL */` |
|     93 |  7402 | `	if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|     81 |  7403 | `		PH7_MemObjRelease(&sTarget);` |
|     38 |  7404 | `	}` |
|     93 |  7405 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|     34 |  7406 | `		PH7_MemObjRelease(&sResult);` |
|     34 |  7407 | `		return rc;` |
|      - |  7408 | `	}` |
|     62 |  7409 | `	ph7_result_value(pCtx, &sResult);` |
|     62 |  7410 | `	PH7_MemObjRelease(&sResult);` |
|     62 |  7411 | `	return PH7_OK;` |
|     51 |  7412 | `}` |
|     58 |  7413 | `static int vm_builtin_ReflectionFunction_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  7414 | `{` |
|      - |  7415 | `	VmCallArgMap sMap;` |
|     63 |  7416 | `	return ReflectFunctionInvoke(pCtx, nArg, apArg, ReflectDoorArgMap(pCtx, 0, 0, 0, &sMap), 0, 1);` |
|      5 |  7417 | `}` |
|     34 |  7418 | `static int vm_builtin_ReflectionFunction_invokeArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  7419 | `{` |
|      - |  7420 | `	SySet aCall;` |
|     38 |  7421 | `	SyString *aNames = 0;` |
|     38 |  7422 | `	ph7_hashmap_node **apNode = 0;` |
|      - |  7423 | `	VmCallArgMap sMap;` |
|      - |  7424 | `	int rc;` |
|     38 |  7425 | `	SySetInit(&aCall, &pCtx->pVm->sAllocator, sizeof(ph7_value *));` |
|     38 |  7426 | `	if( nArg > 0 ){` |
|     38 |  7427 | `		ReflectCollectArgs(pCtx, apArg[0], &aCall, &aNames, &apNode);` |
|     17 |  7428 | `	}` |
|     55 |  7429 | `	rc = ReflectFunctionInvoke(pCtx, (int)SySetUsed(&aCall), (ph7_value **)SySetBasePtr(&aCall),` |
|     34 |  7430 | `		aNames ? ReflectDoorArgMap(pCtx, aNames, SySetUsed(&aCall), 0, &sMap) : 0, apNode, 0);` |
|     38 |  7431 | `	if( aNames ){` |
|     19 |  7432 | `		SyMemBackendFree(&pCtx->pVm->sAllocator, aNames);` |
|      9 |  7433 | `	}` |
|     38 |  7434 | `	if( apNode ){` |
|     38 |  7435 | `		SyMemBackendFree(&pCtx->pVm->sAllocator, apNode);` |
|     17 |  7436 | `	}` |
|     38 |  7437 | `	SySetRelease(&aCall);` |
|     38 |  7438 | `	return rc;` |
|      4 |  7439 | `}` |
|      8 |  7440 | `static int vm_builtin_ReflectionFunction_getClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7441 | `{` |
|     10 |  7442 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7443 | `	ph7_class_instance *pClo;` |
|     10 |  7444 | `	const char *zName = "";` |
|     10 |  7445 | `	int nName = 0;` |
|      - |  7446 | `	SyString sName;` |
|      4 |  7447 | `	SXUNUSED(nArg);` |
|      4 |  7448 | `	SXUNUSED(apArg);` |
|     10 |  7449 | `	if( pThis == 0 ){` |
|    ! 0 |  7450 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7451 | `		return PH7_OK;` |
|      - |  7452 | `	}` |
|     10 |  7453 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|     10 |  7454 | `	if( pClo ){` |
|      - |  7455 | `		/* Already a Closure: hand the same instance back */` |
|      3 |  7456 | `		return ReflectResultExistingObject(pCtx, pClo);` |
|      - |  7457 | `	}` |
|      8 |  7458 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      8 |  7459 | `	SyStringInitFromBuf(&sName, zName, nName);` |
|      8 |  7460 | `	return ReflectResultObject(pCtx, PH7_VmNewClosure(pCtx->pVm, &sName, 0, 0));` |
|      6 |  7461 | `}` |
|      - |  7462 | `/* ---- ReflectionMethod ---- */` |
|      - |  7463 | `/* ReflectionMethod::__construct(object\|string $objectOrMethod, ?string $method = null) */` |
|   1982 |  7464 | `static int vm_builtin_ReflectionMethod_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  7465 | `{` |
|   1987 |  7466 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1987 |  7467 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7468 | `	ph7_class *pClass;` |
|      - |  7469 | `	SyHashEntry *pEntry;` |
|      - |  7470 | `	ph7_value sClass, sMethod;` |
|      - |  7471 | `	const char *zMethod;` |
|   1987 |  7472 | `	int nMethod, rc = PH7_OK;` |
|   1987 |  7473 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 |  7474 | `		return PH7_OK;` |
|      - |  7475 | `	}` |
|   1987 |  7476 | `	PH7_MemObjInit(pVm, &sClass);` |
|   1987 |  7477 | `	PH7_MemObjInit(pVm, &sMethod);` |
|   1988 |  7478 | `	if( nArg < 2 \|\| ph7_value_is_null(apArg[1]) ){` |
|      - |  7479 | `		/* One-argument form: "Class::method" */` |
|      - |  7480 | `		const char *zSpec;` |
|      3 |  7481 | `		int nSpec, iSep = -1, k;` |
|      3 |  7482 | `		if( (apArg[0]->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  7483 | `			PH7_MemObjRelease(&sClass);` |
|    ! 0 |  7484 | `			PH7_MemObjRelease(&sMethod);` |
|    ! 0 |  7485 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  7486 | `				"The parameter class is expected to be either a string or an object");` |
|      - |  7487 | `		}` |
|      3 |  7488 | `		zSpec = ph7_value_to_string(apArg[0], &nSpec);` |
|     19 |  7489 | `		for( k = 0 ; k + 1 < nSpec ; k++ ){` |
|     19 |  7490 | `			if( zSpec[k] == ':' && zSpec[k+1] == ':' ){` |
|      3 |  7491 | `				iSep = k;` |
|      3 |  7492 | `				break;` |
|      - |  7493 | `			}` |
|      9 |  7494 | `		}` |
|      3 |  7495 | `		if( iSep < 0 ){` |
|    ! 0 |  7496 | `			PH7_MemObjRelease(&sClass);` |
|    ! 0 |  7497 | `			PH7_MemObjRelease(&sMethod);` |
|    ! 0 |  7498 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 |  7499 | `				"Invalid method name %.*s", nSpec, zSpec);` |
|      - |  7500 | `		}` |
|      - |  7501 | `		/* php 8.3 deprecated the one-argument spelling in favour of the static` |
|      - |  7502 | `		 * factory. createFromMethodName() splits the name ITSELF and constructs` |
|      - |  7503 | `		 * with two, so it does not trip this. */` |
|      3 |  7504 | `		PH7_VmThrowError(pVm, 0, E_DEPRECATED,` |
|      - |  7505 | `			"Calling ReflectionMethod::__construct() with 1 argument is deprecated, "` |
|      - |  7506 | `			"use ReflectionMethod::createFromMethodName() instead");` |
|      3 |  7507 | `		ph7_value_string(&sClass, zSpec, iSep);` |
|      3 |  7508 | `		ph7_value_string(&sMethod, &zSpec[iSep+2], nSpec - iSep - 2);` |
|      2 |  7509 | `	}else{` |
|   1985 |  7510 | `		PH7_MemObjStore(apArg[0], &sClass);` |
|   1985 |  7511 | `		PH7_MemObjStore(apArg[1], &sMethod);` |
|      - |  7512 | `	}` |
|   1987 |  7513 | `	if( ReflectResolveClassRaised(pVm, &sClass, &pClass, &rc) ){` |
|      5 |  7514 | `		goto Done;` |
|      - |  7515 | `	}` |
|   1983 |  7516 | `	if( pClass == 0 ){` |
|      - |  7517 | `		const char *zName;` |
|      - |  7518 | `		int nName;` |
|      8 |  7519 | `		zName = ph7_value_to_string(&sClass, &nName);` |
|     11 |  7520 | `		rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 |  7521 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      8 |  7522 | `		goto Done;` |
|      - |  7523 | `	}` |
|   1977 |  7524 | `	zMethod = ph7_value_to_string(&sMethod, &nMethod);` |
|   1977 |  7525 | `	pEntry = ReflectFindMethodEntry(pClass, zMethod, nMethod);` |
|   1977 |  7526 | `	if( pEntry == 0 ){` |
|     13 |  7527 | `		rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      6 |  7528 | `			"Method %z::%.*s() does not exist", &pClass->sDisp, nMethod, zMethod);` |
|      7 |  7529 | `		goto Done;` |
|      - |  7530 | `	}` |
|   1971 |  7531 | `	if( ((ph7_class_method *)pEntry->pUserData)->iFlags & PH7_CLASS_ATTR_FABRICATED ){` |
|      - |  7532 | `		/* php has no such row in the class's function table, so a CONSTRUCTOR asked` |
|      - |  7533 | ``		 * for it by class name finds nothing: `new ReflectionMethod('Closure',`` |
|      - |  7534 | ``		 * '__invoke')` is a ReflectionException even though the method answers`` |
|      - |  7535 | `		 * everywhere else. Given the OBJECT, php builds it -- and so does a` |
|      - |  7536 | `		 * ReflectionClass door, which is the nReflectFactory window. */` |
|     26 |  7537 | `		if( (sClass.iFlags & MEMOBJ_OBJ) == 0 && pVm->nReflectFactory < 1 ){` |
|      5 |  7538 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 |  7539 | `				"Method %z::%.*s() does not exist", &pClass->sDisp, nMethod, zMethod);` |
|      3 |  7540 | `			goto Done;` |
|      - |  7541 | `		}` |
|     24 |  7542 | `		if( sClass.iFlags & MEMOBJ_OBJ ){` |
|      - |  7543 | `			/* ...and what it builds describes THAT closure: park the instance where` |
|      - |  7544 | `			 * ReflectFuncOfThis already looks for a reflected Closure, so the` |
|      - |  7545 | `			 * parameter list, the return type and invoke() are the closure's own. */` |
|     20 |  7546 | `			PH7_NativeSetAttrObj(pVm, pThis, RF_CL, (ph7_class_instance *)sClass.x.pOther);` |
|      9 |  7547 | `		}` |
|     11 |  7548 | `	}` |
|      - |  7549 | `	{` |
|      - |  7550 | `		/* php's $class is the DECLARING class, not the one the lookup went` |
|      - |  7551 | ``		 * through: `new ReflectionMethod('Kid','bm')` on an inherited method`` |
|      - |  7552 | `		 * reports Base. */` |
|   1969 |  7553 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, (ph7_class_method *)pEntry->pUserData);` |
|   2951 |  7554 | `		PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pDecl->sName),` |
|   1964 |  7555 | `			(int)SyStringLength(&pDecl->sName));` |
|      - |  7556 | `	}` |
|      - |  7557 | `	/* The class the reflector was built FOR, which the export tags read. Every` |
|      - |  7558 | `	 * ReflectionClass door that hands a method out passes its OWN name as the` |
|      - |  7559 | `	 * first constructor argument, so they all land here too. */` |
|   2951 |  7560 | `	PH7_NativeSetAttrStr(pVm, pThis, RM_CE, SyStringData(&pClass->sName),` |
|   1964 |  7561 | `		(int)SyStringLength(&pClass->sName));` |
|      - |  7562 | `	/* The DECLARED spelling, whatever case was asked for. */` |
|   1969 |  7563 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", (const char *)pEntry->pKey, (int)pEntry->nKeyLen);` |
|    991 |  7564 | `Done:` |
|   1987 |  7565 | `	PH7_MemObjRelease(&sClass);` |
|   1987 |  7566 | `	PH7_MemObjRelease(&sMethod);` |
|   1987 |  7567 | `	return rc;` |
|    996 |  7568 | `}` |
|      - |  7569 | `/* ReflectionMethod::createFromMethodName(string $method): static */` |
|      8 |  7570 | `static int vm_builtin_ReflectionMethod_createFromMethodName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7571 | `{` |
|      - |  7572 | `	ph7_class_instance *pOut;` |
|      - |  7573 | `	ph7_value sClass, sMethod;` |
|      - |  7574 | `	ph7_value *apCtor[2];` |
|      - |  7575 | `	const char *zSpec;` |
|     10 |  7576 | `	int nSpec, iSep = -1, k;` |
|      - |  7577 | `	sxi32 rc;` |
|     10 |  7578 | `	if( nArg < 1 ){` |
|    ! 0 |  7579 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7580 | `		return PH7_OK;` |
|      - |  7581 | `	}` |
|      - |  7582 | `	/* Split here rather than letting the constructor do it: the one-argument` |
|      - |  7583 | `	 * constructor is the DEPRECATED spelling this factory exists to replace. */` |
|     10 |  7584 | `	zSpec = ph7_value_to_string(apArg[0], &nSpec);` |
|     68 |  7585 | `	for( k = 0 ; k + 1 < nSpec ; k++ ){` |
|     68 |  7586 | `		if( zSpec[k] == ':' && zSpec[k+1] == ':' ){` |
|     10 |  7587 | `			iSep = k;` |
|     10 |  7588 | `			break;` |
|      - |  7589 | `		}` |
|     31 |  7590 | `	}` |
|     10 |  7591 | `	if( iSep < 0 ){` |
|    ! 0 |  7592 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 |  7593 | `			"Invalid method name %.*s", nSpec, zSpec);` |
|      - |  7594 | `	}` |
|     10 |  7595 | `	PH7_MemObjInit(pCtx->pVm, &sClass);` |
|     10 |  7596 | `	PH7_MemObjInit(pCtx->pVm, &sMethod);` |
|     10 |  7597 | `	ph7_value_string(&sClass, zSpec, iSep);` |
|     10 |  7598 | `	ph7_value_string(&sMethod, &zSpec[iSep+2], nSpec - iSep - 2);` |
|     10 |  7599 | `	apCtor[0] = &sClass;` |
|     10 |  7600 | `	apCtor[1] = &sMethod;` |
|     10 |  7601 | `	pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|     10 |  7602 | `	PH7_MemObjRelease(&sClass);` |
|     10 |  7603 | `	PH7_MemObjRelease(&sMethod);` |
|     10 |  7604 | `	if( pOut == 0 ){` |
|      5 |  7605 | `		if( rc != PH7_OK ){` |
|      5 |  7606 | `			return rc;` |
|      - |  7607 | `		}` |
|    ! 0 |  7608 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7609 | `		return PH7_OK;` |
|      - |  7610 | `	}` |
|      5 |  7611 | `	return ReflectResultObject(pCtx, pOut);` |
|      6 |  7612 | `}` |
|      - |  7613 | `/* The visibility/modifier predicates, all off the method record. */` |
|     46 |  7614 | `static int ReflectMethodFlag(ph7_context *pCtx, int iWhat)` |
|      4 |  7615 | `{` |
|      - |  7616 | `	ReflectFuncRef sRef;` |
|     50 |  7617 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|    ! 0 |  7618 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  7619 | `		return PH7_OK;` |
|      - |  7620 | `	}` |
|     50 |  7621 | `	switch( iWhat ){` |
|     15 |  7622 | `	case 0: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PUBLIC); break;` |
|     11 |  7623 | `	case 1: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PRIVATE); break;` |
|      3 |  7624 | `	case 2: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PROTECTED); break;` |
|      7 |  7625 | `	case 3: ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0); break;` |
|     18 |  7626 | `	default: ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0); break;` |
|      - |  7627 | `	}` |
|     50 |  7628 | `	return PH7_OK;` |
|     27 |  7629 | `}` |
|      - |  7630 | `#define REFLECT_METHOD_FLAG(NAME,WHAT) \` |
|      - |  7631 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  7632 | `	{ \` |
|      - |  7633 | `		SXUNUSED(nArg); \` |
|      - |  7634 | `		SXUNUSED(apArg); \` |
|      - |  7635 | `		return ReflectMethodFlag(pCtx, WHAT); \` |
|      - |  7636 | `	}` |
|     15 |  7637 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isPublic, 0)` |
|     11 |  7638 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isPrivate, 1)` |
|      3 |  7639 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isProtected, 2)` |
|      7 |  7640 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isAbstract, 3)` |
|     18 |  7641 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isFinal, 4)` |
|      - |  7642 |  |
|      - |  7643 | `/* isConstructor()/isDestructor() read the NAME, not the record: a trait` |
|      - |  7644 | `` * `use T { m as __construct; }` alias is the constructor under that key. */`` |
|      4 |  7645 | `static int ReflectMethodNameIs(ph7_context *pCtx, const char *zWant, int nWant)` |
|      2 |  7646 | `{` |
|      6 |  7647 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      6 |  7648 | `	const char *zName = "";` |
|      6 |  7649 | `	int nName = 0;` |
|      6 |  7650 | `	if( pThis ){` |
|      6 |  7651 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 |  7652 | `	}` |
|      6 |  7653 | `	ph7_result_bool(pCtx, nName == nWant && SyStrnicmp(zName, zWant, (sxu32)nWant) == 0);` |
|      6 |  7654 | `	return PH7_OK;` |
|      2 |  7655 | `}` |
|      4 |  7656 | `static int vm_builtin_ReflectionMethod_isConstructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7657 | `{` |
|      2 |  7658 | `	SXUNUSED(nArg);` |
|      2 |  7659 | `	SXUNUSED(apArg);` |
|      6 |  7660 | `	return ReflectMethodNameIs(pCtx, "__construct", sizeof("__construct")-1);` |
|      2 |  7661 | `}` |
|    ! 0 |  7662 | `static int vm_builtin_ReflectionMethod_isDestructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  7663 | `{` |
|    ! 0 |  7664 | `	SXUNUSED(nArg);` |
|    ! 0 |  7665 | `	SXUNUSED(apArg);` |
|    ! 0 |  7666 | `	return ReflectMethodNameIs(pCtx, "__destruct", sizeof("__destruct")-1);` |
|    ! 0 |  7667 | `}` |
|    108 |  7668 | `static int vm_builtin_ReflectionMethod_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7669 | `{` |
|      - |  7670 | `	ReflectFuncRef sRef;` |
|     54 |  7671 | `	SXUNUSED(nArg);` |
|     54 |  7672 | `	SXUNUSED(apArg);` |
|    110 |  7673 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|    ! 0 |  7674 | `		ph7_result_int(pCtx, 0);` |
|    ! 0 |  7675 | `		return PH7_OK;` |
|      - |  7676 | `	}` |
|    110 |  7677 | `	ph7_result_int64(pCtx, ReflectMethodModifiers(sRef.pMeth));` |
|    110 |  7678 | `	return PH7_OK;` |
|     56 |  7679 | `}` |
|    956 |  7680 | `static int vm_builtin_ReflectionMethod_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  7681 | `{` |
|      - |  7682 | `	ReflectFuncRef sRef;` |
|    478 |  7683 | `	SXUNUSED(nArg);` |
|    478 |  7684 | `	SXUNUSED(apArg);` |
|    960 |  7685 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 |  7686 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7687 | `		return PH7_OK;` |
|      - |  7688 | `	}` |
|    960 |  7689 | `	return ReflectResultClassOf(pCtx, ReflectFuncDeclClass(&sRef));` |
|    482 |  7690 | `}` |
|      - |  7691 | `/*` |
|      - |  7692 | ` * The receiver check php runs before invoking or binding: a non-static method` |
|      - |  7693 | ` * needs an instance OF ITS DECLARING CLASS; a static one ignores whatever was` |
|      - |  7694 | ` * handed over. *ppThis is cleared for a static method.` |
|      - |  7695 | ` */` |
|     86 |  7696 | `static sxi32 ReflectMethodReceiver(ph7_context *pCtx, ReflectFuncRef *pRef,` |
|      - |  7697 | `	ph7_value *pObject, ph7_class_instance **ppThis, int bClosure)` |
|      2 |  7698 | `{` |
|     88 |  7699 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     88 |  7700 | `	const char *zClass = "", *zName = "";` |
|     88 |  7701 | `	int nClass = 0, nName = 0;` |
|     88 |  7702 | `	ph7_class *pDecl = ReflectFuncDeclClass(pRef);` |
|     88 |  7703 | `	*ppThis = 0;` |
|     88 |  7704 | `	if( pThis ){` |
|     88 |  7705 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     88 |  7706 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     43 |  7707 | `	}` |
|     88 |  7708 | `	if( pRef->pMeth && (pRef->pMeth->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|     30 |  7709 | `		return PH7_OK;` |
|      - |  7710 | `	}` |
|     60 |  7711 | `	if( pObject == 0 \|\| (pObject->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      9 |  7712 | `		if( bClosure ){` |
|      5 |  7713 | `			return PH7_VmThrowException(pCtx, "ValueError",` |
|      - |  7714 | `				"ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods");` |
|      - |  7715 | `		}` |
|      7 |  7716 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  7717 | `			"Trying to invoke non static method %.*s::%.*s() without an object",` |
|      2 |  7718 | `			nClass, zClass, nName, zName);` |
|      - |  7719 | `	}` |
|     52 |  7720 | `	*ppThis = (ph7_class_instance *)pObject->x.pOther;` |
|     52 |  7721 | `	if( pDecl && !PH7_VmInstanceOf((*ppThis)->pClass, pDecl) ){` |
|      3 |  7722 | `		*ppThis = 0;` |
|      3 |  7723 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  7724 | `			"Given object is not an instance of the class this method was declared in");` |
|      - |  7725 | `	}` |
|     50 |  7726 | `	return PH7_OK;` |
|     45 |  7727 | `}` |
|      - |  7728 | `/* The by-reference rule is ReflectFunctionInvoke's (apNode, bCopy), named with the` |
|      - |  7729 | ` * method's DECLARING class, as php names it. */` |
|     70 |  7730 | `static int ReflectMethodInvoke(ph7_context *pCtx, ph7_value *pObject, int nCall, ph7_value **apCall,` |
|      - |  7731 | `	VmCallArgMap *pMap, ph7_hashmap_node **apNode, int bCopy)` |
|      2 |  7732 | `{` |
|     72 |  7733 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  7734 | `	ReflectFuncRef sRef;` |
|     72 |  7735 | `	ph7_class_instance *pRecv = 0;` |
|      - |  7736 | `	ph7_value sResult;` |
|      - |  7737 | `	sxi32 rc;` |
|     72 |  7738 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|    ! 0 |  7739 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7740 | `		return PH7_OK;` |
|      - |  7741 | `	}` |
|     72 |  7742 | `	if( sRef.pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|      - |  7743 | `		/* php refuses an abstract method before it looks at the receiver, and names the` |
|      - |  7744 | `		 * class and method the reflector was made for. getClosure() does not: its closure` |
|      - |  7745 | `		 * runs php's empty abstract body. */` |
|     15 |  7746 | `		const char *zClass = "", *zName = "";` |
|     15 |  7747 | `		int nClass = 0, nName = 0;` |
|     15 |  7748 | `		ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     15 |  7749 | `		if( pThis ){` |
|     15 |  7750 | `			PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     15 |  7751 | `			PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      7 |  7752 | `		}` |
|     22 |  7753 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      7 |  7754 | `			"Trying to invoke abstract method %.*s::%.*s()", nClass, zClass, nName, zName);` |
|      - |  7755 | `	}` |
|     58 |  7756 | `	rc = ReflectMethodReceiver(pCtx, &sRef, pObject, &pRecv, 0);` |
|     58 |  7757 | `	if( rc != PH7_OK ){` |
|      7 |  7758 | `		return rc;` |
|      - |  7759 | `	}` |
|     77 |  7760 | `	rc = PH7_VmByRefArgsGivenValue(pVm, ReflectFuncDeclClass(&sRef), &sRef.pMeth->sFunc, 0,` |
|     25 |  7761 | `		nCall, bCopy ? apCall : 0, apNode, pMap ? pMap->aNames : 0);` |
|     52 |  7762 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  7763 | `		return rc;` |
|      - |  7764 | `	}` |
|     52 |  7765 | `	PH7_MemObjInit(pVm, &sResult);` |
|     52 |  7766 | `	sResult.nIdx = SXU32_HIGH;` |
|      - |  7767 | `	/* Reflection ignores method visibility (PHP 8.1+); the flag is consumed by` |
|      - |  7768 | `	 * the first OP_CALL, i.e. this synthetic one. */` |
|     52 |  7769 | `	pVm->bReflectBypass = 1;` |
|      - |  7770 | `	/* And it binds the arguments WEAKLY however strict the file that called` |
|      - |  7771 | `	 * invoke() is: php reads strict mode off the frame that MADE the call, and` |
|      - |  7772 | `	 * that frame is ReflectionMethod::invoke itself, an internal function with no` |
|      - |  7773 | `	 * strict_types of its own. PHPUnit's mock builder reaches every original` |
|      - |  7774 | `	 * constructor this way (Generator::instantiate -> getConstructor()->invokeArgs),` |
|      - |  7775 | `	 * from a file that declares strict_types=1. */` |
|     52 |  7776 | `	pVm->bCallbackWeak = 1;` |
|     52 |  7777 | `	rc = PH7_VmCallClassMethodMap(pVm, pRecv, sRef.pMeth, &sResult, nCall, apCall, pMap);` |
|     52 |  7778 | `	pVm->bCallbackWeak = 0; /* clear if the dispatch never reached an OP_CALL */` |
|     52 |  7779 | `	pVm->bReflectBypass = 0;` |
|     52 |  7780 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|      5 |  7781 | `		PH7_MemObjRelease(&sResult);` |
|      5 |  7782 | `		return rc;` |
|      - |  7783 | `	}` |
|     48 |  7784 | `	ph7_result_value(pCtx, &sResult);` |
|     48 |  7785 | `	PH7_MemObjRelease(&sResult);` |
|     48 |  7786 | `	return PH7_OK;` |
|     37 |  7787 | `}` |
|     50 |  7788 | `static int vm_builtin_ReflectionMethod_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7789 | `{` |
|      - |  7790 | `	VmCallArgMap sMap;` |
|    102 |  7791 | `	return ReflectMethodInvoke(pCtx, nArg > 0 ? apArg[0] : 0,` |
|     50 |  7792 | `		nArg > 1 ? nArg - 1 : 0, nArg > 1 ? &apArg[1] : 0,` |
|     25 |  7793 | `		ReflectDoorArgMap(pCtx, 0, 0, 1, &sMap), 0, 1);` |
|      2 |  7794 | `}` |
|     20 |  7795 | `static int vm_builtin_ReflectionMethod_invokeArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7796 | `{` |
|      - |  7797 | `	SySet aCall;` |
|     22 |  7798 | `	SyString *aNames = 0;` |
|     22 |  7799 | `	ph7_hashmap_node **apNode = 0;` |
|      - |  7800 | `	VmCallArgMap sMap;` |
|      - |  7801 | `	int rc;` |
|     22 |  7802 | `	SySetInit(&aCall, &pCtx->pVm->sAllocator, sizeof(ph7_value *));` |
|     22 |  7803 | `	if( nArg > 1 ){` |
|     22 |  7804 | `		ReflectCollectArgs(pCtx, apArg[1], &aCall, &aNames, &apNode);` |
|     10 |  7805 | `	}` |
|     22 |  7806 | `	rc = ReflectMethodInvoke(pCtx, nArg > 0 ? apArg[0] : 0,` |
|     20 |  7807 | `		(int)SySetUsed(&aCall), (ph7_value **)SySetBasePtr(&aCall),` |
|     20 |  7808 | `		aNames ? ReflectDoorArgMap(pCtx, aNames, SySetUsed(&aCall), 0, &sMap) : 0, apNode, 0);` |
|     22 |  7809 | `	if( aNames ){` |
|      7 |  7810 | `		SyMemBackendFree(&pCtx->pVm->sAllocator, aNames);` |
|      3 |  7811 | `	}` |
|     22 |  7812 | `	if( apNode ){` |
|     18 |  7813 | `		SyMemBackendFree(&pCtx->pVm->sAllocator, apNode);` |
|      8 |  7814 | `	}` |
|     22 |  7815 | `	SySetRelease(&aCall);` |
|     22 |  7816 | `	return rc;` |
|      2 |  7817 | `}` |
|     30 |  7818 | `static int vm_builtin_ReflectionMethod_getClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7819 | `{` |
|      - |  7820 | `	ReflectFuncRef sRef;` |
|     32 |  7821 | `	ph7_class_instance *pRecv = 0;` |
|      - |  7822 | `	sxi32 rc;` |
|     32 |  7823 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 \|\| sRef.pClass == 0 ){` |
|    ! 0 |  7824 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7825 | `		return PH7_OK;` |
|      - |  7826 | `	}` |
|     32 |  7827 | `	rc = ReflectMethodReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pRecv, 1);` |
|     32 |  7828 | `	if( rc != PH7_OK ){` |
|      5 |  7829 | `		return rc;` |
|      - |  7830 | `	}` |
|      - |  7831 | `	{` |
|      - |  7832 | `		/* php hands back a closure over the RESOLVED method and reflection is allowed past` |
|      - |  7833 | `		 * protection (8.1+), so this one must dispatch without a second visibility decision —` |
|      - |  7834 | `		 * the FCC_METHOD mark is what tells the unwrap its $__fn is a screened METHOD name. */` |
|     41 |  7835 | `		ph7_class_instance *pClo = PH7_VmNewClosure(pCtx->pVm, &sRef.pMeth->sFunc.sName,` |
|     26 |  7836 | `			pRecv, &sRef.pClass->sName);` |
|     28 |  7837 | `		if( pClo ){` |
|     28 |  7838 | `			pClo->iFlags \|= VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SCREENED;` |
|     13 |  7839 | `		}` |
|     28 |  7840 | `		return ReflectResultObject(pCtx, pClo);` |
|      - |  7841 | `	}` |
|     17 |  7842 | `}` |
|      - |  7843 | `/* php 8.1 made every reflected member accessible; the setter is a no-op it kept. */` |
|    ! 0 |  7844 | `static int vm_builtin_ReflectionMethod_setAccessible(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  7845 | `{` |
|    ! 0 |  7846 | `	SXUNUSED(pCtx);` |
|    ! 0 |  7847 | `	SXUNUSED(nArg);` |
|    ! 0 |  7848 | `	SXUNUSED(apArg);` |
|    ! 0 |  7849 | `	return PH7_OK;` |
|    ! 0 |  7850 | `}` |
|      - |  7851 | `/*` |
|      - |  7852 | `` * php's `overwrites`: the class the entry found in the PARENT's method table`` |
|      - |  7853 | ` * belongs to — which is an interface when the parent only inherited it from` |
|      - |  7854 | `` * one, so `ReflectionFunction::__toString` overwrites Stringable. A method`` |
|      - |  7855 | ` * that first appears in an interface the class itself implements is a` |
|      - |  7856 | `` * `prototype` instead, never an overwrite.`` |
|      - |  7857 | ` */` |
|   2256 |  7858 | `static ph7_class * ReflectOverwritesIn(ph7_class *pClass, const char *zName, int nName)` |
|      3 |  7859 | `{` |
|      - |  7860 | `	ph7_class *pWalk;` |
|   2259 |  7861 | `	int iDepth = 0;` |
|   2259 |  7862 | `	if( pClass == 0 \|\| nName < 1 ){` |
|    ! 0 |  7863 | `		return 0;` |
|      - |  7864 | `	}` |
|   2851 |  7865 | `	for( pWalk = pClass->pBase ; pWalk && iDepth <= REFLECT_WALK_MAX_DEPTH ; pWalk = pWalk->pBase ){` |
|    757 |  7866 | `		SyHashEntry *pEntry = ReflectFindMethodEntry(pWalk, zName, nName);` |
|    757 |  7867 | `		if( pEntry ){` |
|    165 |  7868 | `			ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    165 |  7869 | `			if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|    165 |  7870 | `				return ReflectMethodDeclClass(pWalk, pMeth);` |
|      - |  7871 | `			}` |
|    ! 0 |  7872 | `		}` |
|    593 |  7873 | `		iDepth++;` |
|    297 |  7874 | `	}` |
|   2095 |  7875 | `	return 0;` |
|   1131 |  7876 | `}` |
|      - |  7877 | `/*` |
|      - |  7878 | `` * php's `prototype`: the ROOT-most declaration the entry in pClass's method`` |
|      - |  7879 | ` * table answers to, or NULL.` |
|      - |  7880 | ` *` |
|      - |  7881 | `` * zend assigns it once, at link time, as `child->prototype = parent->prototype`` |
|      - |  7882 | `` * ? parent->prototype : parent` — so it CHAINS past every intermediate`` |
|      - |  7883 | ` * override and names the class where the contract began, not the nearest one` |
|      - |  7884 | `` * (`AppendIterator::current` is `prototype Iterator`, three classes up, where`` |
|      - |  7885 | ` * this engine answered its immediate parent). Two rules ride on it, both` |
|      - |  7886 | ` * measured against php 8.5.9:` |
|      - |  7887 | ` *` |
|      - |  7888 | ` *   - An INTERFACE wins over the parent chain. zend inherits from the parent` |
|      - |  7889 | ` *     class first and implements the interfaces after, and each implementation` |
|      - |  7890 | `` *     re-assigns the prototype — so `class C extends B implements I`, with both`` |
|      - |  7891 | ` *     declaring f(), reports I and not B.` |
|      - |  7892 | ` *   - A CONSTRUCTOR takes one only where the contract is really a contract:` |
|      - |  7893 | ` *     the chain link that would have STARTED it is dropped unless it is` |
|      - |  7894 | ` *     abstract (an interface's ctor is abstract too). An inherited prototype` |
|      - |  7895 | ` *     still rides through, so a ctor three deep from an abstract one keeps it.` |
|      - |  7896 | ` */` |
|   8974 |  7897 | `static ph7_class * ReflectPrototypeOf(ph7_context *pCtx, ph7_class *pClass,` |
|      - |  7898 | `	const char *zName, int nName, int iDepth)` |
|      3 |  7899 | `{` |
|   8977 |  7900 | `	ph7_class *pWalk, *pDecl, *pRes = 0;` |
|      - |  7901 | `	SyHashEntry *pOwn;` |
|      - |  7902 | `	int bCtor;` |
|   8977 |  7903 | `	if( pClass == 0 \|\| nName < 1 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|    ! 0 |  7904 | `		return 0;` |
|      - |  7905 | `	}` |
|   8977 |  7906 | `	pOwn = ReflectFindMethodEntry(pClass, zName, nName);` |
|   8977 |  7907 | `	if( pOwn == 0 ){` |
|    ! 0 |  7908 | `		return 0;` |
|      - |  7909 | `	}` |
|   9679 |  7910 | `	bCtor = nName == sizeof("__construct")-1` |
|   8974 |  7911 | `		&& SyStrnicmp(zName, "__construct", sizeof("__construct")-1) == 0;` |
|   8977 |  7912 | `	pDecl = ReflectMethodDeclClass(pClass, (ph7_class_method *)pOwn->pUserData);` |
|   8977 |  7913 | `	if( pDecl != pClass ){` |
|      - |  7914 | `		/* The class did not declare this one: zend copies the record in and` |
|      - |  7915 | `		 * assigns NOTHING, so whatever prototype the record already carries is` |
|      - |  7916 | ``		 * what a reflector reads here. `DOMAttr::C14N` therefore has none at`` |
|      - |  7917 | `		 * all, where this engine named the nearest declaring base. */` |
|   2612 |  7918 | `		pRes = ReflectPrototypeOf(pCtx, pDecl, zName, nName, iDepth + 1);` |
|   7672 |  7919 | `	}else if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      - |  7920 | `		/* Its own declaration, checked against the parent's at link time. An` |
|      - |  7921 | `		 * interface has no such link — its parents are declared parents, and` |
|      - |  7922 | `		 * are walked with the interface list below. */` |
|   5451 |  7923 | `		for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){` |
|   1135 |  7924 | `			SyHashEntry *pEntry = ReflectFindMethodEntry(pWalk, zName, nName);` |
|      - |  7925 | `			ph7_class_method *pMeth;` |
|   1135 |  7926 | `			if( pEntry == 0 ){` |
|    773 |  7927 | `				continue;` |
|      - |  7928 | `			}` |
|    363 |  7929 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    363 |  7930 | `			if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|    ! 0 |  7931 | `				break;` |
|      - |  7932 | `			}` |
|    363 |  7933 | `			pRes = ReflectPrototypeOf(pCtx, pWalk, zName, nName, iDepth + 1);` |
|    363 |  7934 | `			if( pRes == 0 && !(bCtor && (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0) ){` |
|    123 |  7935 | `				pRes = ReflectMethodDeclClass(pWalk, pMeth);` |
|     61 |  7936 | `			}` |
|    363 |  7937 | `			break;` |
|    ! 0 |  7938 | `		}` |
|   2338 |  7939 | `	}` |
|      - |  7940 | `	/* Each interface this class DECLARES re-assigns it, the last one winning —` |
|      - |  7941 | `	 * which is why an interface beats the parent chain. PHL keeps the first` |
|      - |  7942 | `	 * parent of an INTERFACE on the base chain, so it joins the list here. */` |
|      - |  7943 | `	{` |
|   8977 |  7944 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   8977 |  7945 | `		sxu32 n, nIface = SySetUsed(&pClass->aInterface);` |
|  24719 |  7946 | `		for( n = 0 ; n <= nIface ; n++ ){` |
|      - |  7947 | `			ph7_class *pIface;` |
|      - |  7948 | `			SyHashEntry *pEntry;` |
|  15745 |  7949 | `			if( n == nIface ){` |
|   8977 |  7950 | `				pIface = (pClass->iFlags & PH7_CLASS_INTERFACE) ? pClass->pBase : 0;` |
|   4490 |  7951 | `			}else{` |
|   6771 |  7952 | `				pIface = apIface[n];` |
|      - |  7953 | `			}` |
|  15745 |  7954 | `			if( pIface == 0 ){` |
|   7335 |  7955 | `				continue;` |
|      - |  7956 | `			}` |
|   8413 |  7957 | `			pEntry = ReflectFindMethodEntry(pIface, zName, nName);` |
|      - |  7958 | `			/* Nothing to check against when the class holds the interface's` |
|      - |  7959 | `			 * very own record: an interface that merely EXTENDS another copies` |
|      - |  7960 | ``			 * the method in and stops, so `interface B extends A` prints`` |
|      - |  7961 | ``			 * `inherits A` and no prototype at all. */`` |
|   8413 |  7962 | `			if( pEntry == 0 \|\| pOwn->pUserData == pEntry->pUserData ){` |
|   6929 |  7963 | `				continue;` |
|      - |  7964 | `			}` |
|   1487 |  7965 | `			pRes = ReflectPrototypeOf(pCtx, pIface, zName, nName, iDepth + 1);` |
|   1487 |  7966 | `			if( pRes == 0 ){` |
|   2229 |  7967 | `				pRes = ReflectMethodDeclClass(pIface,` |
|   1484 |  7968 | `					(ph7_class_method *)pEntry->pUserData);` |
|    742 |  7969 | `			}` |
|    745 |  7970 | `		}` |
|      - |  7971 | `	}` |
|   8977 |  7972 | `	return pRes;` |
|   4490 |  7973 | `}` |
|      - |  7974 | `/* The same question asked by a ReflectionMethod receiver, which carries the` |
|      - |  7975 | `` * method name on `$this`. The prototype belongs to the entry in the class the`` |
|      - |  7976 | ` * reflector was BUILT FOR, which is php's anchor for it too. */` |
|    146 |  7977 | `static ph7_class * ReflectMethodPrototype(ph7_context *pCtx, ReflectFuncRef *pRef)` |
|      1 |  7978 | `{` |
|    147 |  7979 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    147 |  7980 | `	ph7_class *pOwner = ReflectOwnerOfThis(pCtx);` |
|    147 |  7981 | `	const char *zName = "";` |
|    147 |  7982 | `	int nName = 0;` |
|    147 |  7983 | `	if( pThis == 0 ){` |
|    ! 0 |  7984 | `		return 0;` |
|      - |  7985 | `	}` |
|    147 |  7986 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    147 |  7987 | `	return ReflectPrototypeOf(pCtx, pOwner ? pOwner : pRef->pClass, zName, nName, 0);` |
|     74 |  7988 | `}` |
|     94 |  7989 | `static int vm_builtin_ReflectionMethod_hasPrototype(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7990 | `{` |
|      - |  7991 | `	ReflectFuncRef sRef;` |
|     47 |  7992 | `	SXUNUSED(nArg);` |
|     47 |  7993 | `	SXUNUSED(apArg);` |
|     95 |  7994 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 |  7995 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  7996 | `		return PH7_OK;` |
|      - |  7997 | `	}` |
|     95 |  7998 | `	ph7_result_bool(pCtx, ReflectMethodPrototype(pCtx, &sRef) != 0);` |
|     95 |  7999 | `	return PH7_OK;` |
|     48 |  8000 | `}` |
|     52 |  8001 | `static int vm_builtin_ReflectionMethod_getPrototype(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8002 | `{` |
|     53 |  8003 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  8004 | `	ReflectFuncRef sRef;` |
|      - |  8005 | `	ph7_class *pProto;` |
|     53 |  8006 | `	const char *zClass = "", *zName = "";` |
|     53 |  8007 | `	int nClass = 0, nName = 0;` |
|     26 |  8008 | `	SXUNUSED(nArg);` |
|     26 |  8009 | `	SXUNUSED(apArg);` |
|     53 |  8010 | `	if( pThis == 0 \|\| !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 |  8011 | `		ph7_result_null(pCtx);` |
|    ! 0 |  8012 | `		return PH7_OK;` |
|      - |  8013 | `	}` |
|     53 |  8014 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     53 |  8015 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     53 |  8016 | `	pProto = ReflectMethodPrototype(pCtx, &sRef);` |
|     53 |  8017 | `	if( pProto == 0 ){` |
|      7 |  8018 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 |  8019 | `			"Method %.*s::%.*s does not have a prototype", nClass, zClass, nName, zName);` |
|      - |  8020 | `	}` |
|      - |  8021 | `	{` |
|      - |  8022 | `		ph7_value sClass, sName;` |
|      - |  8023 | `		ph7_value *apCtor[2];` |
|      - |  8024 | `		ph7_class_instance *pOut;` |
|      - |  8025 | `		sxi32 rc;` |
|     49 |  8026 | `		PH7_MemObjInit(pCtx->pVm, &sClass);` |
|     49 |  8027 | `		PH7_MemObjInit(pCtx->pVm, &sName);` |
|     49 |  8028 | `		ph7_value_string(&sClass, SyStringData(&pProto->sName), (int)SyStringLength(&pProto->sName));` |
|     49 |  8029 | `		ph7_value_string(&sName, zName, nName);` |
|     49 |  8030 | `		apCtor[0] = &sClass;` |
|     49 |  8031 | `		apCtor[1] = &sName;` |
|     49 |  8032 | `		pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|     49 |  8033 | `		PH7_MemObjRelease(&sClass);` |
|     49 |  8034 | `		PH7_MemObjRelease(&sName);` |
|     49 |  8035 | `		if( pOut == 0 ){` |
|    ! 0 |  8036 | `			if( rc != PH7_OK ){` |
|    ! 0 |  8037 | `				return rc;` |
|      - |  8038 | `			}` |
|    ! 0 |  8039 | `			ph7_result_null(pCtx);` |
|    ! 0 |  8040 | `			return PH7_OK;` |
|      - |  8041 | `		}` |
|     49 |  8042 | `		return ReflectResultObject(pCtx, pOut);` |
|      - |  8043 | `	}` |
|     27 |  8044 | `}` |
|      - |  8045 | `/* ---- ReflectionParameter ---- */` |
|      - |  8046 | `/*` |
|      - |  8047 | ``  * The function a ReflectionParameter belongs to, from its own `__t`/`__m` `` |
|      - |  8048 | `` * slots. `__t` holds a Closure, a class name or a function name; `__m` the`` |
|      - |  8049 | ` * method name, or null.` |
|      - |  8050 | ` */` |
|   2696 |  8051 | `static int ReflectParamOwner(ph7_context *pCtx, ReflectFuncRef *pRef, ReflectParamDesc *pDesc)` |
|      5 |  8052 | `{` |
|   2701 |  8053 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  8054 | `	ph7_value *pT, *pM;` |
|      - |  8055 | `	int rc;` |
|   2701 |  8056 | `	SyZero(pRef, sizeof(*pRef));` |
|   2701 |  8057 | `	if( pDesc ){` |
|   2605 |  8058 | `		SyZero(pDesc, sizeof(*pDesc));` |
|   1300 |  8059 | `	}` |
|   2701 |  8060 | `	if( pThis == 0 ){` |
|    ! 0 |  8061 | `		return 0;` |
|      - |  8062 | `	}` |
|   2701 |  8063 | `	pT = PH7_NativeAttr(pThis, RP_T);` |
|   2701 |  8064 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|   2701 |  8065 | `	if( pT == 0 ){` |
|    ! 0 |  8066 | `		return 0;` |
|      - |  8067 | `	}` |
|   2701 |  8068 | `	rc = ReflectFuncFill(pCtx->pVm, pT, (pM && (pM->iFlags & MEMOBJ_STRING)) ? pM : 0, pRef);` |
|   2701 |  8069 | `	if( rc == 0 \|\| pDesc == 0 ){` |
|    100 |  8070 | `		return rc;` |
|      - |  8071 | `	}` |
|   2605 |  8072 | `	return ReflectParamAt(pRef, (int)PH7_NativeAttrInt(pThis, RP_P), pDesc);` |
|   1353 |  8073 | `}` |
|      - |  8074 | `/* ReflectionParameter::__construct($function, string\|int $param) */` |
|   1542 |  8075 | `static int vm_builtin_ReflectionParameter_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  8076 | `{` |
|   1547 |  8077 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1547 |  8078 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  8079 | `	ReflectFuncRef sRef;` |
|      - |  8080 | `	ReflectParamDesc sDesc;` |
|      - |  8081 | `	ph7_value sTarget, sMethod;` |
|   1547 |  8082 | `	int nTotal, iFound = -1, rc = PH7_OK;` |
|   1547 |  8083 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 |  8084 | `		return PH7_OK;` |
|      - |  8085 | `	}` |
|   1547 |  8086 | `	SyZero(&sDesc, sizeof(sDesc));` |
|   1547 |  8087 | `	PH7_MemObjInit(pVm, &sTarget);` |
|   1547 |  8088 | `	PH7_MemObjInit(pVm, &sMethod);` |
|      - |  8089 | ``	/* `[$objOrClass, $method]`, `"C::m"` and a plain function/Closure are the`` |
|      - |  8090 | `	 * three spellings php accepts. */` |
|   1547 |  8091 | `	if( ph7_value_is_array(apArg[0]) ){` |
|    633 |  8092 | `		ph7_value *pA = ph7_array_fetch(apArg[0], "0", 1);` |
|    633 |  8093 | `		ph7_value *pB = ph7_array_fetch(apArg[0], "1", 1);` |
|    638 |  8094 | `		if( pA && (pA->iFlags & MEMOBJ_OBJ) ){` |
|     11 |  8095 | `			ph7_class_instance *pObj = (ph7_class_instance *)pA->x.pOther;` |
|     11 |  8096 | `			ph7_class_method *pM0 = pB && (pB->iFlags & MEMOBJ_STRING)` |
|     15 |  8097 | `				? PH7_ClassExtractMethod(pObj->pClass,` |
|     10 |  8098 | `					(const char *)SyBlobData(&pB->sBlob), SyBlobLength(&pB->sBlob))` |
|      5 |  8099 | `				: 0;` |
|     11 |  8100 | `			if( pM0 && (pM0->iFlags & PH7_CLASS_ATTR_FABRICATED) ){` |
|      - |  8101 | ``				/* `[$closure, '__invoke']` has to keep the OBJECT: the class name alone`` |
|      - |  8102 | `				 * resolves to the empty class-level declaration, and it is THIS` |
|      - |  8103 | `				 * closure's parameter list the pair is naming. Every other pair` |
|      - |  8104 | `				 * reduces to its class, which is what php reports as the parameter's` |
|      - |  8105 | `				 * declaring class either way. */` |
|     11 |  8106 | `				PH7_MemObjStore(pA, &sTarget);` |
|      6 |  8107 | `			}else{` |
|    ! 0 |  8108 | `				ph7_value_string(&sTarget, SyStringData(&pObj->pClass->sName),` |
|    ! 0 |  8109 | `					(int)SyStringLength(&pObj->pClass->sName));` |
|      1 |  8110 | `			}` |
|    628 |  8111 | `		}else if( pA ){` |
|    623 |  8112 | `			PH7_MemObjStore(pA, &sTarget);` |
|    309 |  8113 | `		}` |
|    633 |  8114 | `		if( pB ){` |
|    633 |  8115 | `			PH7_MemObjStore(pB, &sMethod);` |
|    314 |  8116 | `		}` |
|    319 |  8117 | `	}else{` |
|      - |  8118 | ``		/* A STRING is a plain function name, `"C::m"` included: php does not`` |
|      - |  8119 | ``		 * split one here (it reports `Function C::m() does not exist`), unlike`` |
|      - |  8120 | `		 * ReflectionMethod's own one-argument form. The prelude split it and` |
|      - |  8121 | `		 * silently reflected the method. */` |
|    918 |  8122 | `		PH7_MemObjStore(apArg[0], &sTarget);` |
|      - |  8123 | `	}` |
|   1547 |  8124 | `	if( (sMethod.iFlags & MEMOBJ_STRING) && (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|      - |  8125 | `		/* A pair's class is looked up first, and on its own refusal: php says` |
|      - |  8126 | ``		 * `Class "X" does not exist` (a throwing autoloader's exception its`` |
|      - |  8127 | `		 * $previous), never that the METHOD is missing. */` |
|      - |  8128 | `		ph7_class_instance *pPrev;` |
|      - |  8129 | `		sxi32 rcLook;` |
|    623 |  8130 | `		if( ReflectResolveClassFenced(pCtx, &sTarget, 0, &pPrev, &rcLook) == 0 ){` |
|      - |  8131 | `			const char *zName;` |
|      - |  8132 | `			int nName;` |
|      5 |  8133 | `			rc = rcLook;` |
|      5 |  8134 | `			if( rc == PH7_OK ){` |
|      5 |  8135 | `				zName = ph7_value_to_string(&sTarget, &nName);` |
|      7 |  8136 | `				rc = PH7_VmThrowExceptionPrev(pCtx, pPrev, "ReflectionException",` |
|      2 |  8137 | `					"Class \"%.*s\" does not exist", nName, zName);` |
|      2 |  8138 | `			}` |
|      5 |  8139 | `			if( pPrev ){` |
|      3 |  8140 | `				PH7_ClassInstanceUnref(pPrev);` |
|      1 |  8141 | `			}` |
|      5 |  8142 | `			goto Done;` |
|      - |  8143 | `		}` |
|    307 |  8144 | `	}` |
|   1543 |  8145 | `	if( !ReflectFuncFill(pCtx->pVm, &sTarget, (sMethod.iFlags & MEMOBJ_STRING) ? &sMethod : 0, &sRef) ){` |
|      - |  8146 | `		const char *zName;` |
|      - |  8147 | `		int nName;` |
|      3 |  8148 | `		if( sMethod.iFlags & MEMOBJ_STRING ){` |
|      - |  8149 | `			const char *zM;` |
|      - |  8150 | `			int nM;` |
|    ! 0 |  8151 | `			zName = ph7_value_to_string(&sTarget, &nName);` |
|    ! 0 |  8152 | `			zM = ph7_value_to_string(&sMethod, &nM);` |
|    ! 0 |  8153 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 |  8154 | `				"Method %.*s::%.*s() does not exist", nName, zName, nM, zM);` |
|    ! 0 |  8155 | `		}else{` |
|      3 |  8156 | `			zName = ph7_value_to_string(&sTarget, &nName);` |
|      4 |  8157 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  8158 | `				"Function %.*s() does not exist", nName, zName);` |
|      - |  8159 | `		}` |
|      3 |  8160 | `		goto Done;` |
|      - |  8161 | `	}` |
|   1541 |  8162 | `	nTotal = ReflectParamCount(&sRef);` |
|   1541 |  8163 | `	if( apArg[1]->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|   1529 |  8164 | `		int iWant = (int)ph7_value_to_int(apArg[1]);` |
|   1529 |  8165 | `		if( iWant >= 0 && iWant < nTotal && ReflectParamAt(&sRef, iWant, &sDesc) ){` |
|   1525 |  8166 | `			iFound = iWant;` |
|    760 |  8167 | `		}` |
|   1529 |  8168 | `		if( iFound < 0 ){` |
|      6 |  8169 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  8170 | `				"The parameter specified by its offset could not be found");` |
|      6 |  8171 | `			goto Done;` |
|      - |  8172 | `		}` |
|    765 |  8173 | `	}else{` |
|      - |  8174 | `		const char *zWant;` |
|      - |  8175 | `		int nWant, n;` |
|     14 |  8176 | `		zWant = ph7_value_to_string(apArg[1], &nWant);` |
|     36 |  8177 | `		for( n = 0 ; n < nTotal ; n++ ){` |
|     32 |  8178 | `			if( ReflectParamAt(&sRef, n, &sDesc)` |
|     32 |  8179 | `			 && SyStringLength(&sDesc.sName) == (sxu32)nWant` |
|     26 |  8180 | `			 && SyMemcmp(SyStringData(&sDesc.sName), zWant, (sxu32)nWant) == 0 ){` |
|     12 |  8181 | `				iFound = n;` |
|     12 |  8182 | `				break;` |
|      - |  8183 | `			}` |
|     12 |  8184 | `		}` |
|     14 |  8185 | `		if( iFound < 0 ){` |
|      3 |  8186 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  8187 | `				"The parameter specified by its name could not be found");` |
|      3 |  8188 | `			goto Done;` |
|      - |  8189 | `		}` |
|      - |  8190 | `	}` |
|   2300 |  8191 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sDesc.sName),` |
|   1530 |  8192 | `		(int)SyStringLength(&sDesc.sName));` |
|      - |  8193 | `	/* Record the CANONICAL target: the class the method really came through and` |
|      - |  8194 | `	 * its declared spelling, so a re-resolve cannot land somewhere else. */` |
|   1535 |  8195 | `	if( sRef.bFabricated && sRef.pClosure && sRef.pMeth ){` |
|      - |  8196 | `		/* Its target is the OBJECT plus the method name: re-resolving by class name` |
|      - |  8197 | `		 * would find the empty class-level declaration instead of this closure. */` |
|     11 |  8198 | `		PH7_NativeSetAttrObj(pVm, pThis, RP_T, sRef.pClosure);` |
|     16 |  8199 | `		PH7_NativeSetAttrStr(pVm, pThis, RP_M, SyStringData(&sRef.pMeth->sFunc.sName),` |
|     10 |  8200 | `			(int)SyStringLength(&sRef.pMeth->sFunc.sName));` |
|   1530 |  8201 | `	}else if( sRef.pMeth && sRef.pClass ){` |
|    935 |  8202 | `		PH7_NativeSetAttrStr(pVm, pThis, RP_T, SyStringData(&sRef.pClass->sName),` |
|    620 |  8203 | `			(int)SyStringLength(&sRef.pClass->sName));` |
|    935 |  8204 | `		PH7_NativeSetAttrStr(pVm, pThis, RP_M, SyStringData(&sRef.pMeth->sFunc.sName),` |
|    620 |  8205 | `			(int)SyStringLength(&sRef.pMeth->sFunc.sName));` |
|    315 |  8206 | `	}else{` |
|    904 |  8207 | `		ph7_value *pSlot = PH7_NativeAttr(pThis, RP_T);` |
|    904 |  8208 | `		if( pSlot ){` |
|    904 |  8209 | `			PH7_MemObjStore(&sTarget, pSlot);` |
|    450 |  8210 | `		}` |
|      - |  8211 | `	}` |
|   1535 |  8212 | `	PH7_NativeSetAttrInt(pVm, pThis, RP_P, (sxi64)iFound);` |
|    771 |  8213 | `Done:` |
|   1547 |  8214 | `	PH7_MemObjRelease(&sTarget);` |
|   1547 |  8215 | `	PH7_MemObjRelease(&sMethod);` |
|   1547 |  8216 | `	return rc;` |
|    776 |  8217 | `}` |
|    906 |  8218 | `static int vm_builtin_ReflectionParameter_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  8219 | `{` |
|    910 |  8220 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    910 |  8221 | `	const char *zName = "";` |
|    910 |  8222 | `	int nName = 0;` |
|    453 |  8223 | `	SXUNUSED(nArg);` |
|    453 |  8224 | `	SXUNUSED(apArg);` |
|    910 |  8225 | `	if( pThis ){` |
|    910 |  8226 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    453 |  8227 | `	}` |
|    910 |  8228 | `	ph7_result_string(pCtx, zName, nName);` |
|    910 |  8229 | `	return PH7_OK;` |
|      4 |  8230 | `}` |
|     64 |  8231 | `static int vm_builtin_ReflectionParameter_getPosition(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  8232 | `{` |
|     67 |  8233 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     32 |  8234 | `	SXUNUSED(nArg);` |
|     32 |  8235 | `	SXUNUSED(apArg);` |
|     67 |  8236 | `	ph7_result_int64(pCtx, pThis ? PH7_NativeAttrInt(pThis, RP_P) : 0);` |
|     67 |  8237 | `	return PH7_OK;` |
|      3 |  8238 | `}` |
|      - |  8239 | `/* The boolean predicates that read one flag of the description. */` |
|    914 |  8240 | `static int ReflectParamFlag(ph7_context *pCtx, int iWhat)` |
|      5 |  8241 | `{` |
|      - |  8242 | `	ReflectFuncRef sRef;` |
|      - |  8243 | `	ReflectParamDesc sDesc;` |
|    919 |  8244 | `	int bYes = 0;` |
|    919 |  8245 | `	if( ReflectParamOwner(pCtx, &sRef, &sDesc) ){` |
|    919 |  8246 | `		switch( iWhat ){` |
|     58 |  8247 | `		case 0: bYes = sDesc.bByRef; break;` |
|      3 |  8248 | `		case 1: bYes = !sDesc.bByRef; break;` |
|    163 |  8249 | `		case 2: bYes = sDesc.bVariadic; break;` |
|    ! 0 |  8250 | `		case 3: bYes = sDesc.bPromoted; break;` |
|    567 |  8251 | `		case 4: bYes = sDesc.bHasDef; break;` |
|    124 |  8252 | `		case 5: bYes = SyStringLength(&sDesc.sType) > 0; break;` |
|      7 |  8253 | `		default:` |
|      - |  8254 | `			/* allowsNull(): untyped, nullable, or a type that INCLUDES null */` |
|     20 |  8255 | `			bYes = SyStringLength(&sDesc.sType) < 1 \|\| sDesc.bNullable` |
|     11 |  8256 | `				\|\| ReflectTypeNameIs(SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType), "mixed")` |
|     20 |  8257 | `				\|\| ReflectTypeNameIs(SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType), "null");` |
|     14 |  8258 | `			break;` |
|      - |  8259 | `		}` |
|    462 |  8260 | `	}else if( iWhat == 1 \|\| iWhat == 6 ){` |
|    ! 0 |  8261 | `		bYes = 1;` |
|    ! 0 |  8262 | `	}` |
|    919 |  8263 | `	ph7_result_bool(pCtx, bYes);` |
|    919 |  8264 | `	return PH7_OK;` |
|      5 |  8265 | `}` |
|      - |  8266 | `#define REFLECT_PARAM_FLAG(NAME,WHAT) \` |
|      - |  8267 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  8268 | `	{ \` |
|      - |  8269 | `		SXUNUSED(nArg); \` |
|      - |  8270 | `		SXUNUSED(apArg); \` |
|      - |  8271 | `		return ReflectParamFlag(pCtx, WHAT); \` |
|      - |  8272 | `	}` |
|     58 |  8273 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isPassedByReference, 0)` |
|      3 |  8274 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_canBePassedByValue, 1)` |
|    163 |  8275 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isVariadic, 2)` |
|    ! 0 |  8276 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isPromoted, 3)` |
|    567 |  8277 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isDefaultValueAvailable, 4)` |
|    124 |  8278 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_hasType, 5)` |
|     16 |  8279 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_allowsNull, 6)` |
|      - |  8280 |  |
|      - |  8281 | `/* isOptional(): every parameter from here on has to be omissible too. */` |
|     80 |  8282 | `static int vm_builtin_ReflectionParameter_isOptional(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  8283 | `{` |
|     84 |  8284 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  8285 | `	ReflectFuncRef sRef;` |
|      - |  8286 | `	ReflectParamDesc sDesc;` |
|      - |  8287 | `	int n, nTotal, iPos;` |
|     40 |  8288 | `	SXUNUSED(nArg);` |
|     40 |  8289 | `	SXUNUSED(apArg);` |
|     84 |  8290 | `	if( pThis == 0 \|\| !ReflectParamOwner(pCtx, &sRef, 0) ){` |
|    ! 0 |  8291 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  8292 | `		return PH7_OK;` |
|      - |  8293 | `	}` |
|     84 |  8294 | `	iPos = (int)PH7_NativeAttrInt(pThis, RP_P);` |
|     84 |  8295 | `	nTotal = ReflectParamCount(&sRef);` |
|    160 |  8296 | `	for( n = iPos ; n < nTotal ; n++ ){` |
|    110 |  8297 | `		if( ReflectParamAt(&sRef, n, &sDesc) && !sDesc.bVariadic && !sDesc.bOptional ){` |
|     33 |  8298 | `			ph7_result_bool(pCtx, 0);` |
|     33 |  8299 | `			return PH7_OK;` |
|      - |  8300 | `		}` |
|     42 |  8301 | `	}` |
|     54 |  8302 | `	ph7_result_bool(pCtx, 1);` |
|     54 |  8303 | `	return PH7_OK;` |
|     44 |  8304 | `}` |
|    982 |  8305 | `static int vm_builtin_ReflectionParameter_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  8306 | `{` |
|      - |  8307 | `	ReflectFuncRef sRef;` |
|      - |  8308 | `	ReflectParamDesc sDesc;` |
|    491 |  8309 | `	SXUNUSED(nArg);` |
|    491 |  8310 | `	SXUNUSED(apArg);` |
|    987 |  8311 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| SyStringLength(&sDesc.sType) < 1 ){` |
|     11 |  8312 | `		ph7_result_null(pCtx);` |
|     11 |  8313 | `		return PH7_OK;` |
|      - |  8314 | `	}` |
|   1466 |  8315 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx,` |
|    974 |  8316 | `		SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType)));` |
|    496 |  8317 | `}` |
|      - |  8318 | `/*` |
|      - |  8319 | ` * getClass()/isArray()/isCallable() — php 8.0 deprecated these in favour of` |
|      - |  8320 | ` * getType() but still declares them and still answers. The E_DEPRECATED that` |
|      - |  8321 | ` * comes with a call is raised at the CALL now, from the one table every` |
|      - |  8322 | ` * deprecated internal name is stamped from (aDeprecatedFunc[]), which is where` |
|      - |  8323 | ` * php raises it too — before the callee's own screens rather than inside it.` |
|      - |  8324 | ` */` |
|      8 |  8325 | `static int vm_builtin_ReflectionParameter_getClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8326 | `{` |
|      - |  8327 | `	ReflectFuncRef sRef;` |
|      - |  8328 | `	ReflectParamDesc sDesc;` |
|      - |  8329 | `	const char *zType;` |
|      - |  8330 | `	int nType;` |
|      4 |  8331 | `	SXUNUSED(nArg);` |
|      4 |  8332 | `	SXUNUSED(apArg);` |
|      9 |  8333 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| SyStringLength(&sDesc.sType) < 1 ){` |
|    ! 0 |  8334 | `		ph7_result_null(pCtx);` |
|    ! 0 |  8335 | `		return PH7_OK;` |
|      - |  8336 | `	}` |
|      9 |  8337 | `	zType = SyStringData(&sDesc.sType);` |
|      9 |  8338 | `	nType = (int)SyStringLength(&sDesc.sType);` |
|      9 |  8339 | `	if( nType > 0 && zType[0] == '?' ){` |
|      3 |  8340 | `		zType++;` |
|      3 |  8341 | `		nType--;` |
|      1 |  8342 | `	}` |
|      9 |  8343 | `	if( ReflectTypeIsBuiltin(zType, nType) ){` |
|      7 |  8344 | `		ph7_result_null(pCtx);` |
|      7 |  8345 | `		return PH7_OK;` |
|      - |  8346 | `	}` |
|      3 |  8347 | `	return ReflectResultClassOf(pCtx, PH7_VmExtractClass(pCtx->pVm, zType, (sxu32)nType, FALSE, 0));` |
|      5 |  8348 | `}` |
|     16 |  8349 | `static int ReflectParamTypeIs(ph7_context *pCtx, const char *zWant)` |
|      1 |  8350 | `{` |
|      - |  8351 | `	ReflectFuncRef sRef;` |
|      - |  8352 | `	ReflectParamDesc sDesc;` |
|     17 |  8353 | `	int bYes = 0;` |
|     17 |  8354 | `	if( ReflectParamOwner(pCtx, &sRef, &sDesc) && SyStringLength(&sDesc.sType) > 0 ){` |
|     17 |  8355 | `		const char *zType = SyStringData(&sDesc.sType);` |
|     17 |  8356 | `		int nType = (int)SyStringLength(&sDesc.sType);` |
|      - |  8357 | ``		/* php answers true for `?array` too: the deprecated pair asks about the`` |
|      - |  8358 | `		 * type ATOM, and nullability is a separate question. */` |
|     17 |  8359 | `		if( zType[0] == '?' ){` |
|      5 |  8360 | `			zType++;` |
|      5 |  8361 | `			nType--;` |
|      2 |  8362 | `		}` |
|     17 |  8363 | `		bYes = ReflectTypeNameIs(zType, nType, zWant);` |
|      8 |  8364 | `	}` |
|     17 |  8365 | `	ph7_result_bool(pCtx, bYes);` |
|     17 |  8366 | `	return PH7_OK;` |
|      1 |  8367 | `}` |
|      8 |  8368 | `static int vm_builtin_ReflectionParameter_isArray(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8369 | `{` |
|      4 |  8370 | `	SXUNUSED(nArg);` |
|      4 |  8371 | `	SXUNUSED(apArg);` |
|      9 |  8372 | `	return ReflectParamTypeIs(pCtx, "array");` |
|      1 |  8373 | `}` |
|      8 |  8374 | `static int vm_builtin_ReflectionParameter_isCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8375 | `{` |
|      4 |  8376 | `	SXUNUSED(nArg);` |
|      4 |  8377 | `	SXUNUSED(apArg);` |
|      9 |  8378 | `	return ReflectParamTypeIs(pCtx, "callable");` |
|      1 |  8379 | `}` |
|      - |  8380 | `/*` |
|      - |  8381 | `` * The class `self`/`parent` resolve against inside a parameter's default: the one`` |
|      - |  8382 | ` * that DECLARED the method (a trait's members belong to the composing class). 0 for` |
|      - |  8383 | ` * a plain function, whose defaults can name neither.` |
|      - |  8384 | ` */` |
|     62 |  8385 | `static ph7_class * ReflectParamSelfClass(const ReflectFuncRef *pRef)` |
|      2 |  8386 | `{` |
|     64 |  8387 | `	if( pRef->pMeth == 0 ){` |
|     32 |  8388 | `		return 0;` |
|      - |  8389 | `	}` |
|     34 |  8390 | `	return PH7_VmMemberOwnerClass((ph7_class *)pRef->pMeth->sFunc.pUserData,` |
|     32 |  8391 | `		pRef->pClass ? pRef->pClass : (ph7_class *)pRef->pMeth->sFunc.pUserData);` |
|     33 |  8392 | `}` |
|    310 |  8393 | `static int vm_builtin_ReflectionParameter_getDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  8394 | `{` |
|      - |  8395 | `	ReflectFuncRef sRef;` |
|      - |  8396 | `	ReflectParamDesc sDesc;` |
|    155 |  8397 | `	SXUNUSED(nArg);` |
|    155 |  8398 | `	SXUNUSED(apArg);` |
|    314 |  8399 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|      5 |  8400 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  8401 | `			"Internal error: Failed to retrieve the default value");` |
|      - |  8402 | `	}` |
|    310 |  8403 | `	if( sDesc.pArg ){` |
|      - |  8404 | `		/* Compiled: the same evaluation path the VM uses for an omitted argument --` |
|      - |  8405 | `		 * except that one runs INSIDE the call, where the frame already names the` |
|      - |  8406 | ``		 * class `self::K` resolves against. Reflection has no such frame, so a`` |
|      - |  8407 | ``		 * default written `self::K` answered `Class "self" not found` where php`` |
|      - |  8408 | `		 * answers its value. Mark the declaring class the way a member` |
|      - |  8409 | `		 * initializer's evaluation does (PH7_VmPeekDeclaringClass reads the pair). */` |
|      - |  8410 | `		ph7_value sValue;` |
|     64 |  8411 | `		ph7_class *pSaveCls = pCtx->pVm->pConstEvalClass;` |
|     64 |  8412 | `		void *pSaveFrame = pCtx->pVm->pConstEvalFrame;` |
|     64 |  8413 | `		ph7_class *pDecl = ReflectParamSelfClass(&sRef);` |
|     64 |  8414 | `		if( pDecl ){` |
|     34 |  8415 | `			pCtx->pVm->pConstEvalClass = pDecl;` |
|     34 |  8416 | `			pCtx->pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pCtx->pVm->pFrame);` |
|     16 |  8417 | `		}` |
|     64 |  8418 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     64 |  8419 | `		VmLocalExec(pCtx->pVm, &sDesc.pArg->aByteCode, &sValue, FALSE);` |
|     64 |  8420 | `		pCtx->pVm->pConstEvalClass = pSaveCls;` |
|     64 |  8421 | `		pCtx->pVm->pConstEvalFrame = pSaveFrame;` |
|     64 |  8422 | `		ph7_result_value(pCtx, &sValue);` |
|     64 |  8423 | `		PH7_MemObjRelease(&sValue);` |
|     64 |  8424 | `		return PH7_OK;` |
|      - |  8425 | `	}` |
|      - |  8426 | `	{` |
|      - |  8427 | `		/* Declared: the signature's default TEXT, reduced. */` |
|    247 |  8428 | `		const char *zDef = SyStringData(&sDesc.sDefText);` |
|    247 |  8429 | `		int nDef = (int)SyStringLength(&sDesc.sDefText);` |
|    247 |  8430 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    247 |  8431 | `		if( pVal && ReflectSigScalar(pCtx, zDef, nDef, pVal) ){` |
|    233 |  8432 | `			ph7_result_value(pCtx, pVal);` |
|    233 |  8433 | `			return PH7_OK;` |
|      - |  8434 | `		}` |
|     14 |  8435 | `		if( (nDef >= 1 && zDef[0] == '[')` |
|      9 |  8436 | `		 \|\| (nDef >= (int)sizeof("array (")-1` |
|    ! 0 |  8437 | `		  && SyMemcmp(zDef, "array (", sizeof("array (")-1) == 0) ){` |
|     16 |  8438 | `			ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|     16 |  8439 | `			return PH7_OK;` |
|      - |  8440 | `		}` |
|      - |  8441 | `	}` |
|    ! 0 |  8442 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  8443 | `		"Internal error: Failed to retrieve the default value");` |
|    159 |  8444 | `}` |
|      - |  8445 | `/*` |
|      - |  8446 | ` * A default that is a plain global-constant reference compiles to exactly` |
|      - |  8447 | ` * [ OP_LOADC (EXPAND), OP_DONE ] with the name in the literal table.` |
|      - |  8448 | ` *` |
|      - |  8449 | ` * A DECLARED default is TEXT, and php answers the same two questions about one:` |
|      - |  8450 | `` * `int $type = PDO::PARAM_STR` is a constant default and its name is what the`` |
|      - |  8451 | `` * stub wrote. Only a SINGLE reference counts -- the `\|` fold beside it`` |
|      - |  8452 | ` * evaluates to a number that no constant carries, and php answers false and` |
|      - |  8453 | ` * null for it while still printing the source in the export.` |
|      - |  8454 | ` */` |
|   1606 |  8455 | `static int ReflectParamDefConst(ph7_context *pCtx, ReflectParamDesc *pDesc,` |
|      - |  8456 | `	const char **pz, int *pn)` |
|      5 |  8457 | `{` |
|      - |  8458 | `	VmInstr *aInstr;` |
|      - |  8459 | `	ph7_value *pLit;` |
|   1611 |  8460 | `	if( pDesc->pArg == 0 && pDesc->bHasDef ){` |
|   1459 |  8461 | `		const char *zDef = SyStringData(&pDesc->sDefText);` |
|   1459 |  8462 | `		int nDef = (int)SyStringLength(&pDesc->sDefText);` |
|      - |  8463 | `		ph7_value *pVal;` |
|   1459 |  8464 | `		ReflectSigTrim(&zDef, &nDef);` |
|   1459 |  8465 | `		if( nDef < 1 \|\| ReflectSigHas(zDef, nDef, "\|", 1) ){` |
|     62 |  8466 | `			return 0;` |
|      - |  8467 | `		}` |
|   1399 |  8468 | `		pVal = ph7_context_new_scalar(pCtx);` |
|   1399 |  8469 | `		if( pVal == 0 ){` |
|    ! 0 |  8470 | `			return 0;` |
|      - |  8471 | `		}` |
|   1394 |  8472 | `		if( nDef > (int)sizeof("::class")-1` |
|    823 |  8473 | `		 && SyMemcmp(&zDef[nDef - (sizeof("::class")-1)], "::class",` |
|    121 |  8474 | `		             sizeof("::class")-1) == 0 ){` |
|      - |  8475 | ``			/* `C::class` reads a class NAME rather than a constant, and php`` |
|      - |  8476 | `			 * answers false for it while still printing it in the export. */` |
|     57 |  8477 | `			return 0;` |
|      - |  8478 | `		}` |
|   1340 |  8479 | `		if( !ReflectSigGlobalConst(pCtx, zDef, nDef, pVal)` |
|   1291 |  8480 | `		 && !ReflectSigClassConst(pCtx, zDef, nDef, pVal) ){` |
|   1171 |  8481 | `			return 0;` |
|      - |  8482 | `		}` |
|    178 |  8483 | `		*pz = zDef;` |
|    178 |  8484 | `		*pn = nDef;` |
|    178 |  8485 | `		return 1;` |
|      - |  8486 | `	}` |
|    154 |  8487 | `	if( pDesc->pArg == 0 ){` |
|    ! 0 |  8488 | `		return 0;` |
|      - |  8489 | `	}` |
|    154 |  8490 | `	if( SySetUsed(&pDesc->pArg->aByteCode) == 4 ){` |
|      - |  8491 | `` 		/* A CLASS constant compiles to the class name, the member name and the `::` `` |
|      - |  8492 | `		 * fetch: [ LOADC <class>, LOADC <member>, MEMBER(static, bareword), DONE ].` |
|      - |  8493 | `		 * php answers true for one and names it the way the source spelled it --` |
|      - |  8494 | ``		 * `self::K` stays `self::K` -- minus a leading `\`, which it drops. Only the`` |
|      - |  8495 | `		 * plain global-constant shape below was recognised, so every class-constant` |
|      - |  8496 | `		 * default reported itself as no constant at all. */` |
|     49 |  8497 | `		VmInstr *aCC = (VmInstr *)SySetBasePtr(&pDesc->pArg->aByteCode);` |
|     48 |  8498 | `		if( aCC[0].iOp == PH7_OP_LOADC && aCC[1].iOp == PH7_OP_LOADC` |
|     48 |  8499 | `		 && aCC[2].iOp == PH7_OP_MEMBER && aCC[2].iP1 == 1 && aCC[2].p3 == 0` |
|     47 |  8500 | `		 && aCC[3].iOp == PH7_OP_DONE ){` |
|     47 |  8501 | `			ph7_value *pCls = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj,aCC[0].iP2);` |
|     47 |  8502 | `			ph7_value *pMem = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj,aCC[1].iP2);` |
|     46 |  8503 | `			if( pCls && pMem && SyBlobLength(&pCls->sBlob) > 0` |
|     46 |  8504 | `			 && SyBlobLength(&pMem->sBlob) > 0` |
|     49 |  8505 | `			 && !(SyBlobLength(&pMem->sBlob) == sizeof("class")-1` |
|     25 |  8506 | `			   && SyStrnicmp((const char *)SyBlobData(&pMem->sBlob),"class",` |
|      2 |  8507 | `			                 sizeof("class")-1) == 0) ){` |
|      - |  8508 | ``				/* `C::class` reads a class NAME rather than a constant, and php`` |
|      - |  8509 | `				 * answers false for it -- the same rule the declared-signature path` |
|      - |  8510 | `				 * above makes. */` |
|     43 |  8511 | `				const char *zCls = (const char *)SyBlobData(&pCls->sBlob);` |
|     43 |  8512 | `				sxu32 nCls = SyBlobLength(&pCls->sBlob);` |
|     43 |  8513 | `				SyBlob *pOut = &pCtx->pVm->sReflectConstName;` |
|     43 |  8514 | `				while( nCls > 0 && zCls[0] == '\\' ){` |
|    ! 0 |  8515 | `					zCls++;` |
|    ! 0 |  8516 | `					nCls--;` |
|    ! 0 |  8517 | `				}` |
|     43 |  8518 | `				SyBlobReset(pOut);` |
|     43 |  8519 | `				SyBlobAppend(pOut,zCls,nCls);` |
|     43 |  8520 | `				SyBlobAppend(pOut,"::",sizeof("::")-1);` |
|     43 |  8521 | `				SyBlobAppend(pOut,SyBlobData(&pMem->sBlob),SyBlobLength(&pMem->sBlob));` |
|     43 |  8522 | `				*pz = (const char *)SyBlobData(pOut);` |
|     43 |  8523 | `				*pn = (int)SyBlobLength(pOut);` |
|     43 |  8524 | `				return 1;` |
|      - |  8525 | `			}` |
|      2 |  8526 | `		}` |
|      7 |  8527 | `		return 0;` |
|      - |  8528 | `	}` |
|    106 |  8529 | `	if( SySetUsed(&pDesc->pArg->aByteCode) != 2 ){` |
|     20 |  8530 | `		return 0;` |
|      - |  8531 | `	}` |
|     88 |  8532 | `	aInstr = (VmInstr *)SySetBasePtr(&pDesc->pArg->aByteCode);` |
|     86 |  8533 | `	if( aInstr[0].iOp != PH7_OP_LOADC \|\| (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0` |
|     54 |  8534 | `	 \|\| aInstr[1].iOp != PH7_OP_DONE ){` |
|     70 |  8535 | `		return 0;` |
|      - |  8536 | `	}` |
|     20 |  8537 | `	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);` |
|     20 |  8538 | `	if( pLit == 0 \|\| SyBlobLength(&pLit->sBlob) < 1 ){` |
|    ! 0 |  8539 | `		return 0;` |
|      - |  8540 | `	}` |
|     20 |  8541 | `	*pz = (const char *)SyBlobData(&pLit->sBlob);` |
|     20 |  8542 | `	*pn = (int)SyBlobLength(&pLit->sBlob);` |
|     20 |  8543 | `	return 1;` |
|    808 |  8544 | `}` |
|    128 |  8545 | `static int vm_builtin_ReflectionParameter_isDefaultValueConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  8546 | `{` |
|      - |  8547 | `	ReflectFuncRef sRef;` |
|      - |  8548 | `	ReflectParamDesc sDesc;` |
|      - |  8549 | `	const char *z;` |
|      - |  8550 | `	int n;` |
|     64 |  8551 | `	SXUNUSED(nArg);` |
|     64 |  8552 | `	SXUNUSED(apArg);` |
|    130 |  8553 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|      - |  8554 | `		/* php raises here rather than answering false: asking whether a default` |
|      - |  8555 | `		 * is a constant presupposes there IS one. The prelude answered false. */` |
|      3 |  8556 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  8557 | `			"Internal error: Failed to retrieve the default value");` |
|      - |  8558 | `	}` |
|    128 |  8559 | `	ph7_result_bool(pCtx, ReflectParamDefConst(pCtx, &sDesc, &z, &n) != 0);` |
|    128 |  8560 | `	return PH7_OK;` |
|     66 |  8561 | `}` |
|     64 |  8562 | `static int vm_builtin_ReflectionParameter_getDefaultValueConstantName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  8563 | `{` |
|      - |  8564 | `	ReflectFuncRef sRef;` |
|      - |  8565 | `	ReflectParamDesc sDesc;` |
|      - |  8566 | `	const char *z;` |
|      - |  8567 | `	int n;` |
|     32 |  8568 | `	SXUNUSED(nArg);` |
|     32 |  8569 | `	SXUNUSED(apArg);` |
|     66 |  8570 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|    ! 0 |  8571 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  8572 | `			"Internal error: Failed to retrieve the default value");` |
|      - |  8573 | `	}` |
|     66 |  8574 | `	if( ReflectParamDefConst(pCtx, &sDesc, &z, &n) ){` |
|     38 |  8575 | `		ph7_result_string(pCtx, z, n);` |
|     20 |  8576 | `	}else{` |
|     30 |  8577 | `		ph7_result_null(pCtx);` |
|      - |  8578 | `	}` |
|     66 |  8579 | `	return PH7_OK;` |
|     34 |  8580 | `}` |
|     14 |  8581 | `static int vm_builtin_ReflectionParameter_getDeclaringFunction(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  8582 | `{` |
|     17 |  8583 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  8584 | `	ph7_value *pT, *pM;` |
|      - |  8585 | `	ph7_value *apCtor[2];` |
|      - |  8586 | `	ph7_class_instance *pOut;` |
|      - |  8587 | `	sxi32 rc;` |
|      7 |  8588 | `	SXUNUSED(nArg);` |
|      7 |  8589 | `	SXUNUSED(apArg);` |
|     17 |  8590 | `	if( pThis == 0 \|\| (pT = PH7_NativeAttr(pThis, RP_T)) == 0 ){` |
|    ! 0 |  8591 | `		ph7_result_null(pCtx);` |
|    ! 0 |  8592 | `		return PH7_OK;` |
|      - |  8593 | `	}` |
|     17 |  8594 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|     17 |  8595 | `	apCtor[0] = pT;` |
|     17 |  8596 | `	apCtor[1] = pM;` |
|     17 |  8597 | `	if( pM && (pM->iFlags & MEMOBJ_STRING) ){` |
|     10 |  8598 | `		pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|      6 |  8599 | `	}else{` |
|      8 |  8600 | `		pOut = ReflectConstruct(pCtx, "ReflectionFunction", 1, apCtor, &rc);` |
|      - |  8601 | `	}` |
|     17 |  8602 | `	if( pOut == 0 ){` |
|    ! 0 |  8603 | `		if( rc != PH7_OK ){` |
|    ! 0 |  8604 | `			return rc;` |
|      - |  8605 | `		}` |
|    ! 0 |  8606 | `		ph7_result_null(pCtx);` |
|    ! 0 |  8607 | `		return PH7_OK;` |
|      - |  8608 | `	}` |
|     17 |  8609 | `	return ReflectResultObject(pCtx, pOut);` |
|     10 |  8610 | `}` |
|     16 |  8611 | `static int vm_builtin_ReflectionParameter_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  8612 | `{` |
|      - |  8613 | `	ReflectFuncRef sRef;` |
|      8 |  8614 | `	SXUNUSED(nArg);` |
|      8 |  8615 | `	SXUNUSED(apArg);` |
|     19 |  8616 | `	if( ReflectParamOwner(pCtx, &sRef, 0) && sRef.bTrampoline ){` |
|      - |  8617 | ``		/* The trampoline's `$arguments` belongs to the catch-all's class */`` |
|      7 |  8618 | `		return ReflectResultClassOf(pCtx, PH7_VmClosureScopeClass(pCtx->pVm, sRef.pClosure));` |
|      - |  8619 | `	}` |
|     12 |  8620 | `	if( sRef.pMeth == 0 ){` |
|      3 |  8621 | `		ph7_result_null(pCtx);` |
|      3 |  8622 | `		return PH7_OK;` |
|      - |  8623 | `	}` |
|     10 |  8624 | `	return ReflectResultClassOf(pCtx, ReflectFuncDeclClass(&sRef));` |
|     11 |  8625 | `}` |
|      8 |  8626 | `static int vm_builtin_ReflectionParameter_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8627 | `{` |
|      9 |  8628 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  8629 | `	ReflectFuncRef sRef;` |
|      - |  8630 | `	ReflectParamDesc sDesc;` |
|      - |  8631 | `	ph7_value *pT, *pM;` |
|      9 |  8632 | `	const char *zMember = 0;` |
|      9 |  8633 | `	int nMember = 0;` |
|      9 |  8634 | `	if( pThis == 0 \|\| !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| sDesc.pArg == 0 ){` |
|    ! 0 |  8635 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  8636 | `		return PH7_OK;` |
|      - |  8637 | `	}` |
|      9 |  8638 | `	pT = PH7_NativeAttr(pThis, RP_T);` |
|      9 |  8639 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|      9 |  8640 | `	if( pM && (pM->iFlags & MEMOBJ_STRING) ){` |
|      7 |  8641 | `		zMember = (const char *)SyBlobData(&pM->sBlob);` |
|      7 |  8642 | `		nMember = (int)SyBlobLength(&pM->sBlob);` |
|      3 |  8643 | `	}` |
|      - |  8644 | `	/* 32 = Attribute::TARGET_PARAMETER */` |
|     13 |  8645 | `	return ReflectBuildAttrs(pCtx, &sDesc.pArg->aAttrs, "param", pT, zMember, nMember,` |
|      8 |  8646 | `		(int)PH7_NativeAttrInt(pThis, RP_P), 32, nArg, apArg);` |
|      5 |  8647 | `}` |
|    170 |  8648 | `static int vm_builtin_ReflectionParameter_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  8649 | `{` |
|     85 |  8650 | `	SXUNUSED(nArg);` |
|     85 |  8651 | `	SXUNUSED(apArg);` |
|    175 |  8652 | `	return ReflectExportParamSelf(pCtx);` |
|      5 |  8653 | `}` |
|      - |  8654 | `/*` |
|      - |  8655 | ` * Declare chunk 2. Called from PH7_VmInstallReflection() where it used to be` |
|      - |  8656 | `` * compiled — after chunk 1, whose `Reflector` these implement and whose`` |
|      - |  8657 | `` * `ReflectionClass` they answer.`` |
|      - |  8658 | ` *` |
|      - |  8659 | ` * The method tables are in php's own DECLARATION order.` |
|      - |  8660 | ` */` |
|      - |  8661 | `/*` |
|      - |  8662 | `` * PropertyHookType — php's `enum PropertyHookType: string { Get; Set; }`, the`` |
|      - |  8663 | ` * argument ReflectionProperty::getHook()/hasHook() take.` |
|      - |  8664 | ` */` |
|   8445 |  8665 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionHookType(ph7_vm *pVm)` |
|      5 |  8666 | `{` |
|      - |  8667 | `	static const PH7_NativeEnumCase aCase[] = {` |
|      - |  8668 | `		{ "Get", { 0, 0, PH7_NATIVE_VAL_STRING, 0, "get", 0.0 } },` |
|      - |  8669 | `		{ "Set", { 0, 0, PH7_NATIVE_VAL_STRING, 0, "set", 0.0 } },` |
|      - |  8670 | `	};` |
|   8450 |  8671 | `	return PH7_InstallNativeEnum(&(*pVm), "PropertyHookType", MEMOBJ_STRING,` |
|      - |  8672 | `		aCase, SX_ARRAYSIZE(aCase), 0, 0);` |
|      5 |  8673 | `}` |
|   8445 |  8674 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionFunc(ph7_vm *pVm)` |
|      5 |  8675 | `{` |
|      - |  8676 | `	static const PH7_NativePropDef aAbstractProp[] = {` |
|      - |  8677 | `		{ "name",  PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  8678 | `		/* PHL-only: the Closure being reflected. php reaches the same state from` |
|      - |  8679 | `		 * the function record itself; PHL has no hidden-slot bit yet (recorded). */` |
|      - |  8680 | `		{ RF_CL,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  8681 | `	};` |
|      - |  8682 | `	static const PH7_NativeMethodDef aAbstractMethod[] = {` |
|      - |  8683 | `		{ "__clone",       PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - |  8684 | `		{ "inNamespace",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_inNamespace },` |
|      - |  8685 | `		{ "isClosure",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isClosure },` |
|      - |  8686 | `		{ "isDeprecated",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isDeprecated },` |
|      - |  8687 | `		{ "isInternal",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isInternal },` |
|      - |  8688 | `		{ "isUserDefined", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isUserDefined },` |
|      - |  8689 | `		{ "isGenerator",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isGenerator },` |
|      - |  8690 | `		{ "isVariadic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isVariadic },` |
|      - |  8691 | `		{ "isStatic",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isStatic },` |
|      - |  8692 | `		{ "getClosureThis", PH7_MOD_PUBLIC, "", "@?object", vm_builtin_ReflectionFunc_getClosureThis },` |
|      - |  8693 | `		{ "getClosureScopeClass", PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|      - |  8694 | `		  vm_builtin_ReflectionFunc_getClosureScopeClass },` |
|      - |  8695 | `		/* php answers the same class for both; PHL has no separate called-scope. */` |
|      - |  8696 | `		{ "getClosureCalledClass", PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|      - |  8697 | `		  vm_builtin_ReflectionFunc_getClosureCalledClass },` |
|      - |  8698 | `		{ "getClosureUsedVariables", PH7_MOD_PUBLIC, "", "array",` |
|      - |  8699 | `		  vm_builtin_ReflectionFunc_getClosureUsedVariables },` |
|      - |  8700 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getDocComment },` |
|      - |  8701 | `		{ "getEndLine",    PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionFunc_getEndLine },` |
|      - |  8702 | `		{ "getExtension",  PH7_MOD_PUBLIC, "", "@?ReflectionExtension", vm_builtin_ReflectionFunc_getExtension },` |
|      - |  8703 | `		{ "getExtensionName", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getExtensionName },` |
|      - |  8704 | `		{ "getFileName",   PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getFileName },` |
|      - |  8705 | `		{ "getName",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getName },` |
|      - |  8706 | `		{ "getNamespaceName", PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getNamespaceName },` |
|      - |  8707 | `		{ "getNumberOfParameters", PH7_MOD_PUBLIC, "", "@int",` |
|      - |  8708 | `		  vm_builtin_ReflectionFunc_getNumberOfParameters },` |
|      - |  8709 | `		{ "getNumberOfRequiredParameters", PH7_MOD_PUBLIC, "", "@int",` |
|      - |  8710 | `		  vm_builtin_ReflectionFunc_getNumberOfRequiredParameters },` |
|      - |  8711 | `		{ "getParameters", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionFunc_getParameters },` |
|      - |  8712 | `		{ "getShortName",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getShortName },` |
|      - |  8713 | `		{ "getStartLine",  PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionFunc_getStartLine },` |
|      - |  8714 | `		{ "getStaticVariables", PH7_MOD_PUBLIC, "", "@array",` |
|      - |  8715 | `		  vm_builtin_ReflectionFunc_getStaticVariables },` |
|      - |  8716 | `		{ "returnsReference", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_returnsReference },` |
|      - |  8717 | `		{ "hasReturnType", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_hasReturnType },` |
|      - |  8718 | `		{ "getReturnType", PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionFunc_getReturnType },` |
|      - |  8719 | `		{ "hasTentativeReturnType", PH7_MOD_PUBLIC, "", "bool",` |
|      - |  8720 | `		  vm_builtin_ReflectionFunc_hasTentativeReturnType },` |
|      - |  8721 | `		{ "getTentativeReturnType", PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|      - |  8722 | `		  vm_builtin_ReflectionFunc_getTentativeReturnType },` |
|      - |  8723 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - |  8724 | `		  vm_builtin_ReflectionFunc_getAttributes },` |
|      - |  8725 | `	};` |
|      - |  8726 | `	static const PH7_NativeConstDef aFunctionConst[] = {` |
|      - |  8727 | `		{ "IS_DEPRECATED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2048, 0, 0.0 },` |
|      - |  8728 | `	};` |
|      - |  8729 | `	static const PH7_NativeMethodDef aFunctionMethod[] = {` |
|      - |  8730 | `		{ "__construct", PH7_MOD_PUBLIC, "Closure\|string $function", "",` |
|      - |  8731 | `		  vm_builtin_ReflectionFunction_construct },` |
|      - |  8732 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionFunc_toString },` |
|      - |  8733 | `		{ "isAnonymous", PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionFunction_isAnonymous },` |
|      - |  8734 | `		{ "isDisabled",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunction_isDisabled },` |
|      - |  8735 | `		{ "invoke",      PH7_MOD_PUBLIC, "mixed ...$args", "@mixed",` |
|      - |  8736 | `		  vm_builtin_ReflectionFunction_invoke },` |
|      - |  8737 | `		{ "invokeArgs",  PH7_MOD_PUBLIC, "array $args", "@mixed",` |
|      - |  8738 | `		  vm_builtin_ReflectionFunction_invokeArgs },` |
|      - |  8739 | `		{ "getClosure",  PH7_MOD_PUBLIC, "", "@Closure", vm_builtin_ReflectionFunction_getClosure },` |
|      - |  8740 | `	};` |
|      - |  8741 | `	static const PH7_NativePropDef aMethodProp[] = {` |
|      - |  8742 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  8743 | ``		/* PHL-only: php's `intern->ce`, which no property publishes. */`` |
|      - |  8744 | `		{ RM_CE,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  8745 | `	};` |
|      - |  8746 | `	static const PH7_NativeConstDef aMethodConst[] = {` |
|      - |  8747 | `		{ "IS_STATIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16, 0, 0.0 },` |
|      - |  8748 | `		{ "IS_PUBLIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,  0, 0.0 },` |
|      - |  8749 | `		{ "IS_PROTECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,  0, 0.0 },` |
|      - |  8750 | `		{ "IS_PRIVATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,  0, 0.0 },` |
|      - |  8751 | `		{ "IS_ABSTRACT",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64, 0, 0.0 },` |
|      - |  8752 | `		{ "IS_FINAL",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|      - |  8753 | `	};` |
|      - |  8754 | `	static const PH7_NativeMethodDef aMethodMethod[] = {` |
|      - |  8755 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $objectOrMethod, ?string $method = null", "",` |
|      - |  8756 | `		  vm_builtin_ReflectionMethod_construct },` |
|      - |  8757 | `		{ "createFromMethodName", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $method", "static",` |
|      - |  8758 | `		  vm_builtin_ReflectionMethod_createFromMethodName },` |
|      - |  8759 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionFunc_toString },` |
|      - |  8760 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isPublic },` |
|      - |  8761 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isPrivate },` |
|      - |  8762 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isProtected },` |
|      - |  8763 | `		{ "isAbstract",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isAbstract },` |
|      - |  8764 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isFinal },` |
|      - |  8765 | `		{ "isConstructor", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isConstructor },` |
|      - |  8766 | `		{ "isDestructor",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isDestructor },` |
|      - |  8767 | `		{ "getClosure",  PH7_MOD_PUBLIC, "?object $object = null", "@Closure",` |
|      - |  8768 | `		  vm_builtin_ReflectionMethod_getClosure },` |
|      - |  8769 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionMethod_getModifiers },` |
|      - |  8770 | `		{ "invoke",      PH7_MOD_PUBLIC, "?object $object, mixed ...$args", "@mixed",` |
|      - |  8771 | `		  vm_builtin_ReflectionMethod_invoke },` |
|      - |  8772 | `		{ "invokeArgs",  PH7_MOD_PUBLIC, "?object $object, array $args", "@mixed",` |
|      - |  8773 | `		  vm_builtin_ReflectionMethod_invokeArgs },` |
|      - |  8774 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|      - |  8775 | `		  vm_builtin_ReflectionMethod_getDeclaringClass },` |
|      - |  8776 | `		{ "getPrototype", PH7_MOD_PUBLIC, "", "@ReflectionMethod", vm_builtin_ReflectionMethod_getPrototype },` |
|      - |  8777 | `		{ "hasPrototype", PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionMethod_hasPrototype },` |
|      - |  8778 | `		{ "setAccessible", PH7_MOD_PUBLIC, "bool $accessible", "@void",` |
|      - |  8779 | `		  vm_builtin_ReflectionMethod_setAccessible },` |
|      - |  8780 | `	};` |
|      - |  8781 | `	static const PH7_NativePropDef aParamProp[] = {` |
|      - |  8782 | `		{ "name", PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  8783 | `		/* PHL-only, the three that identify the parameter (recorded) */` |
|      - |  8784 | `		{ RP_T,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  8785 | `		{ RP_M,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  8786 | `		{ RP_P,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|      - |  8787 | `	};` |
|      - |  8788 | `	static const PH7_NativeMethodDef aParamMethod[] = {` |
|      - |  8789 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - |  8790 | `		/* php leaves $function UNTYPED here: it takes a name, a Closure, a` |
|      - |  8791 | ``		 * `[$obj, 'm']` pair or "C::m", and no declarable union covers all four. */`` |
|      - |  8792 | `		{ "__construct", PH7_MOD_PUBLIC, "$function, string\|int $param", "",` |
|      - |  8793 | `		  vm_builtin_ReflectionParameter_construct },` |
|      - |  8794 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionParameter_toString },` |
|      - |  8795 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionParameter_getName },` |
|      - |  8796 | `		{ "isPassedByReference", PH7_MOD_PUBLIC, "", "@bool",` |
|      - |  8797 | `		  vm_builtin_ReflectionParameter_isPassedByReference },` |
|      - |  8798 | `		{ "canBePassedByValue",  PH7_MOD_PUBLIC, "", "@bool",` |
|      - |  8799 | `		  vm_builtin_ReflectionParameter_canBePassedByValue },` |
|      - |  8800 | `		{ "getDeclaringFunction", PH7_MOD_PUBLIC, "", "@ReflectionFunctionAbstract",` |
|      - |  8801 | `		  vm_builtin_ReflectionParameter_getDeclaringFunction },` |
|      - |  8802 | `		{ "getDeclaringClass",   PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|      - |  8803 | `		  vm_builtin_ReflectionParameter_getDeclaringClass },` |
|      - |  8804 | `		{ "getClass",    PH7_MOD_PUBLIC, "", "@?ReflectionClass", vm_builtin_ReflectionParameter_getClass },` |
|      - |  8805 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_hasType },` |
|      - |  8806 | `		{ "getType",     PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionParameter_getType },` |
|      - |  8807 | `		{ "isArray",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isArray },` |
|      - |  8808 | `		{ "isCallable",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isCallable },` |
|      - |  8809 | `		{ "allowsNull",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_allowsNull },` |
|      - |  8810 | `		{ "getPosition", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionParameter_getPosition },` |
|      - |  8811 | `		{ "isOptional",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isOptional },` |
|      - |  8812 | `		{ "isDefaultValueAvailable", PH7_MOD_PUBLIC, "", "@bool",` |
|      - |  8813 | `		  vm_builtin_ReflectionParameter_isDefaultValueAvailable },` |
|      - |  8814 | `		{ "getDefaultValue", PH7_MOD_PUBLIC, "", "@mixed",` |
|      - |  8815 | `		  vm_builtin_ReflectionParameter_getDefaultValue },` |
|      - |  8816 | `		{ "isDefaultValueConstant", PH7_MOD_PUBLIC, "", "@bool",` |
|      - |  8817 | `		  vm_builtin_ReflectionParameter_isDefaultValueConstant },` |
|      - |  8818 | `		{ "getDefaultValueConstantName", PH7_MOD_PUBLIC, "", "@?string",` |
|      - |  8819 | `		  vm_builtin_ReflectionParameter_getDefaultValueConstantName },` |
|      - |  8820 | `		{ "isVariadic",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isVariadic },` |
|      - |  8821 | `		{ "isPromoted",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionParameter_isPromoted },` |
|      - |  8822 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - |  8823 | `		  vm_builtin_ReflectionParameter_getAttributes },` |
|      - |  8824 | `	};` |
|      - |  8825 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  8826 | `		{ "ReflectionFunctionAbstract", 0, "Reflector", PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  8827 | `		  aAbstractMethod, SX_ARRAYSIZE(aAbstractMethod), 0, 0,` |
|      - |  8828 | `		  aAbstractProp, SX_ARRAYSIZE(aAbstractProp), 0, 0, 0 },` |
|      - |  8829 | `		{ "ReflectionFunction", "ReflectionFunctionAbstract", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  8830 | `		  aFunctionMethod, SX_ARRAYSIZE(aFunctionMethod),` |
|      - |  8831 | `		  aFunctionConst, SX_ARRAYSIZE(aFunctionConst), 0, 0, 0, 0, 0 },` |
|      - |  8832 | `		{ "ReflectionMethod", "ReflectionFunctionAbstract", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  8833 | `		  aMethodMethod, SX_ARRAYSIZE(aMethodMethod),` |
|      - |  8834 | `		  aMethodConst, SX_ARRAYSIZE(aMethodConst),` |
|      - |  8835 | `		  aMethodProp, SX_ARRAYSIZE(aMethodProp), 0, 0, 0 },` |
|      - |  8836 | `		{ "ReflectionParameter", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  8837 | `		  aParamMethod, SX_ARRAYSIZE(aParamMethod), 0, 0,` |
|      - |  8838 | `		  aParamProp, SX_ARRAYSIZE(aParamProp), 0, 0, 0 },` |
|      - |  8839 | `	};` |
|   8450 |  8840 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  8841 | `}` |
|      - |  8842 | `/*` |
|      - |  8843 | ` * ---------------------------------------------------------------------------` |
|      - |  8844 | ` * ReflectionProperty and ReflectionClassConstant.` |
|      - |  8845 | ` *` |
|      - |  8846 | ` * Chunk 3, the last of the three that made up "the core". Seven thunks retire` |
|      - |  8847 | ` * with it — __reflect_prop_read, __reflect_prop_write, __reflect_prop_state,` |
|      - |  8848 | ` * __reflect_prop_default, __reflect_static_value, __reflect_static_set and` |
|      - |  8849 | ` * __reflect_const_value — because a native method reaches an instance's slot` |
|      - |  8850 | ` * table, a static's shared slot and a constant's lazy slot directly.` |
|      - |  8851 | ` * ---------------------------------------------------------------------------` |
|      - |  8852 | ` */` |
|      - |  8853 | `#define RP_DYNOBJ "__dynobj"   /* the instance a DYNAMIC property lives on */` |
|      - |  8854 |  |
|      - |  8855 | `/* skipLazyInitialization() is a no-op on an object that is not lazy, which is` |
|      - |  8856 | ` * every object PHL can build — so it answers what php answers rather than` |
|      - |  8857 | ` * refusing (recorded). */` |
|    ! 0 |  8858 | `static int vm_builtin_ReflectionProperty_noop(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  8859 | `{` |
|    ! 0 |  8860 | `	SXUNUSED(pCtx);` |
|    ! 0 |  8861 | `	SXUNUSED(nArg);` |
|    ! 0 |  8862 | `	SXUNUSED(apArg);` |
|    ! 0 |  8863 | `	return PH7_OK;` |
|    ! 0 |  8864 | `}` |
|      - |  8865 | `/*` |
|      - |  8866 | `` * A STATIC property's shared slot, read and written the way `C::$s` is: the`` |
|      - |  8867 | ` * class's static table materializes first (a default that threw at the` |
|      - |  8868 | ` * declaration raises HERE), and an uninitialized typed static is php's Error.` |
|      - |  8869 | ` */` |
|      6 |  8870 | `static int ReflectStaticSlotRead(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr)` |
|      2 |  8871 | `{` |
|      - |  8872 | `	SyHashEntry *pSlot;` |
|      - |  8873 | `	ph7_value *pVal;` |
|      8 |  8874 | `	sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|      8 |  8875 | `	if( rc != SXRET_OK ){` |
|      3 |  8876 | `		return rc;` |
|      - |  8877 | `	}` |
|      5 |  8878 | `	pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|      5 |  8879 | `	if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|    ! 0 |  8880 | `		ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|    ! 0 |  8881 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      - |  8882 | `			"Typed static property %z::$%z must not be accessed before initialization",` |
|    ! 0 |  8883 | `			&pDecl->sDisp, &pAttr->sName);` |
|      - |  8884 | `	}` |
|      5 |  8885 | `	pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      5 |  8886 | `	if( pVal ){` |
|      5 |  8887 | `		ph7_result_value(pCtx, pVal);` |
|      3 |  8888 | `	}else{` |
|    ! 0 |  8889 | `		ph7_result_null(pCtx);` |
|      - |  8890 | `	}` |
|      5 |  8891 | `	return PH7_OK;` |
|      5 |  8892 | `}` |
|      4 |  8893 | `static int ReflectStaticSlotWrite(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr,` |
|      - |  8894 | `	ph7_value *pValue)` |
|      2 |  8895 | `{` |
|      - |  8896 | `	ph7_value *pSlot;` |
|      6 |  8897 | `	sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|      6 |  8898 | `	if( rc != SXRET_OK ){` |
|      3 |  8899 | `		return rc;` |
|      - |  8900 | `	}` |
|      3 |  8901 | `	rc = ReflectEnforceStore(pCtx, pAttr->nIdx, pValue);` |
|      3 |  8902 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  8903 | `		return rc;` |
|      - |  8904 | `	}` |
|      3 |  8905 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      3 |  8906 | `	if( pSlot ){` |
|      3 |  8907 | `		PH7_MemObjStore(pValue, pSlot);` |
|      1 |  8908 | `	}` |
|      3 |  8909 | `	return PH7_OK;` |
|      4 |  8910 | `}` |
|      - |  8911 |  |
|      - |  8912 | `/* What a ReflectionProperty / ReflectionClassConstant is looking at. */` |
|      - |  8913 | `typedef struct ReflectMemberRef ReflectMemberRef;` |
|      - |  8914 | `struct ReflectMemberRef` |
|      - |  8915 | `{` |
|      - |  8916 | `	ph7_class *pClass;            /* the reflected class */` |
|      - |  8917 | `	ph7_class_attr *pAttr;        /* the declared member (NULL for a dynamic property) */` |
|      - |  8918 | `	ph7_class_instance *pDynObj;  /* the instance a dynamic property lives on */` |
|      - |  8919 | `	const char *zName;            /* the member's name (borrowed from the slot) */` |
|      - |  8920 | `	int nName;` |
|      - |  8921 | `};` |
|      - |  8922 | `/*` |
|      - |  8923 | `` * Resolve `$this->class` + `$this->name` into the member. Uses the LISTING`` |
|      - |  8924 | ` * answer of the member walk, which is php's: a base class's PRIVATE member is` |
|      - |  8925 | ``  * not reachable by name from the subclass (`new ReflectionProperty('B','pp')` `` |
|      - |  8926 | `` * raises where `B extends A` and `A::$pp` is private).`` |
|      - |  8927 | ` */` |
|   1566 |  8928 | `static int ReflectMemberOfThis(ph7_context *pCtx, ReflectMemberRef *pOut, int iKind)` |
|      5 |  8929 | `{` |
|   1571 |  8930 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  8931 | `	const char *zClass;` |
|      - |  8932 | `	int nClass;` |
|      - |  8933 | `	SySet aMembers;` |
|      - |  8934 | `	sxu32 n;` |
|   1571 |  8935 | `	SyZero(pOut, sizeof(*pOut));` |
|   1571 |  8936 | `	if( pThis == 0 ){` |
|    ! 0 |  8937 | `		return 0;` |
|      - |  8938 | `	}` |
|   1571 |  8939 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|   1571 |  8940 | `	PH7_NativeAttrStr(pThis, "name", &pOut->zName, &pOut->nName);` |
|   1571 |  8941 | `	if( nClass < 1 ){` |
|    ! 0 |  8942 | `		return 0;` |
|      - |  8943 | `	}` |
|   1571 |  8944 | `	pOut->pClass = PH7_VmExtractClass(pCtx->pVm, zClass, (sxu32)nClass, FALSE, 0);` |
|   1571 |  8945 | `	if( pOut->pClass == 0 ){` |
|    ! 0 |  8946 | `		return 0;` |
|      - |  8947 | `	}` |
|   1571 |  8948 | `	if( iKind == REFLECT_MEMBER_PROP ){` |
|   1441 |  8949 | `		pOut->pDynObj = PH7_NativeAttrObj(pThis, RP_DYNOBJ);` |
|    718 |  8950 | `	}` |
|   1571 |  8951 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|   1571 |  8952 | `	ReflectMembers(pCtx->pVm, pOut->pClass, &aMembers, 0);` |
|  10193 |  8953 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|  10187 |  8954 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|  10187 |  8955 | `		if( pM->iKind == iKind && ReflectKeyIs(pM, pOut->zName, pOut->nName) ){` |
|   1565 |  8956 | `			pOut->pAttr = pM->pAttr;` |
|   1565 |  8957 | `			break;` |
|      - |  8958 | `		}` |
|   4316 |  8959 | `	}` |
|   1571 |  8960 | `	SySetRelease(&aMembers);` |
|   1571 |  8961 | `	return pOut->pAttr != 0 \|\| pOut->pDynObj != 0;` |
|    788 |  8962 | `}` |
|      - |  8963 | `/* The class php reports as the member's declarer. */` |
|    276 |  8964 | `static ph7_class * ReflectMemberDecl(const ReflectMemberRef *pRef)` |
|      4 |  8965 | `{` |
|    280 |  8966 | `	if( pRef->pAttr && pRef->pAttr->pDeclClass ){` |
|    280 |  8967 | `		return PH7_VmMemberOwnerClass(pRef->pAttr->pDeclClass,pRef->pClass);` |
|      - |  8968 | `	}` |
|    ! 0 |  8969 | `	return pRef->pClass;` |
|    142 |  8970 | `}` |
|      - |  8971 | `/* The instance slot record for a dynamic (or any instance-owned) property. */` |
|     14 |  8972 | `static VmClassAttr * ReflectInstanceAttr(ph7_class_instance *pObj, const char *zName, int nName)` |
|      1 |  8973 | `{` |
|      - |  8974 | `	SyHashEntry *pEntry;` |
|     15 |  8975 | `	if( pObj == 0 \|\| nName < 1 ){` |
|      7 |  8976 | `		return 0;` |
|      - |  8977 | `	}` |
|      9 |  8978 | `	pEntry = SyHashGet(&pObj->hAttr, (const void *)zName, (sxu32)nName);` |
|      9 |  8979 | `	return pEntry ? (VmClassAttr *)pEntry->pUserData : 0;` |
|      8 |  8980 | `}` |
|      - |  8981 | `/*` |
|      - |  8982 | ` * The instance slot a resolved ReflectionProperty addresses. A base class's` |
|      - |  8983 | ` * PRIVATE property lives under php's MANGLED storage name on every object below` |
|      - |  8984 | ` * it, so looking it up by its plain name would find the same-named property of` |
|      - |  8985 | ` * the object's OWN class instead -- reading and writing the wrong slot.` |
|      - |  8986 | ` */` |
|     98 |  8987 | `static VmClassAttr * ReflectRefInstanceAttr(ph7_vm *pVm, ph7_class_instance *pObj,` |
|      - |  8988 | `	const ReflectMemberRef *pRef)` |
|      1 |  8989 | `{` |
|     99 |  8990 | `	if( pObj && pRef->pAttr ){` |
|     97 |  8991 | `		const SyString *pKey = PH7_ClassAttrStorageName(pVm, pObj->pClass, pRef->pAttr);` |
|    145 |  8992 | `		SyHashEntry *pEntry = SyHashGet(&pObj->hAttr,` |
|     96 |  8993 | `			(const void *)SyStringData(pKey), SyStringLength(pKey));` |
|     97 |  8994 | `		return pEntry ? (VmClassAttr *)pEntry->pUserData : 0;` |
|      - |  8995 | `	}` |
|      3 |  8996 | `	return ReflectInstanceAttr(pObj, pRef->zName, pRef->nName);` |
|     50 |  8997 | `}` |
|      - |  8998 | `/* ---- ReflectionProperty ---- */` |
|      - |  8999 | `/* ReflectionProperty::__construct(object\|string $class, string $property) */` |
|    778 |  9000 | `static int vm_builtin_ReflectionProperty_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  9001 | `{` |
|      - |  9002 | `	ph7_class_instance *pPrev;` |
|      - |  9003 | `	sxi32 rcLook;` |
|    783 |  9004 | `	ph7_vm *pVm = pCtx->pVm;` |
|    783 |  9005 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    783 |  9006 | `	ph7_class_instance *pObj = 0;` |
|      - |  9007 | `	ph7_class *pClass;` |
|      - |  9008 | `	const char *zProp;` |
|      - |  9009 | `	int nProp;` |
|      - |  9010 | `	SySet aMembers;` |
|      - |  9011 | `	sxu32 n;` |
|    783 |  9012 | `	int bFound = 0;` |
|    783 |  9013 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 |  9014 | `		return PH7_OK;` |
|      - |  9015 | `	}` |
|    783 |  9016 | `	if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|     83 |  9017 | `		pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     41 |  9018 | `	}` |
|    783 |  9019 | `	pClass = ReflectResolveClassFenced(pCtx, apArg[0], 0, &pPrev, &rcLook);` |
|    783 |  9020 | `	if( pClass == 0 ){` |
|      - |  9021 | `		const char *zName;` |
|      - |  9022 | `		int nName;` |
|      8 |  9023 | `		if( rcLook != PH7_OK ){` |
|    ! 0 |  9024 | `			return rcLook;` |
|      - |  9025 | `		}` |
|      8 |  9026 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|     11 |  9027 | `		rcLook = PH7_VmThrowExceptionPrev(pCtx, pPrev, "ReflectionException",` |
|      3 |  9028 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      8 |  9029 | `		if( pPrev ){` |
|      3 |  9030 | `			PH7_ClassInstanceUnref(pPrev);` |
|      1 |  9031 | `		}` |
|      8 |  9032 | `		return rcLook;` |
|      - |  9033 | `	}` |
|    777 |  9034 | `	zProp = ph7_value_to_string(apArg[1], &nProp);` |
|    777 |  9035 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|    777 |  9036 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|   5015 |  9037 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   5003 |  9038 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   5003 |  9039 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zProp, nProp) ){` |
|      - |  9040 | `			/* php's $class is the DECLARING class, not the one asked about. */` |
|    765 |  9041 | `			pClass = pM->pDecl ? pM->pDecl : pClass;` |
|    765 |  9042 | `			bFound = 1;` |
|    765 |  9043 | `			break;` |
|      - |  9044 | `		}` |
|   2124 |  9045 | `	}` |
|    777 |  9046 | `	SySetRelease(&aMembers);` |
|   1163 |  9047 | `	PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pClass->sName),` |
|    772 |  9048 | `		(int)SyStringLength(&pClass->sName));` |
|    777 |  9049 | `	if( bFound ){` |
|    765 |  9050 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", zProp, nProp);` |
|    765 |  9051 | `		return PH7_OK;` |
|      - |  9052 | `	}` |
|      - |  9053 | `	/* Not declared: an OBJECT may still own it as a dynamic property. */` |
|     13 |  9054 | `	if( ReflectInstanceAttr(pObj, zProp, nProp) ){` |
|      7 |  9055 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", zProp, nProp);` |
|      7 |  9056 | `		PH7_NativeSetAttrObj(pVm, pThis, RP_DYNOBJ, pObj);` |
|      7 |  9057 | `		return PH7_OK;` |
|      - |  9058 | `	}` |
|     10 |  9059 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 |  9060 | `		"Property %z::$%.*s does not exist", &pClass->sDisp, nProp, zProp);` |
|    394 |  9061 | `}` |
|    436 |  9062 | `static int vm_builtin_ReflectionProperty_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  9063 | `{` |
|    440 |  9064 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    440 |  9065 | `	const char *zName = "";` |
|    440 |  9066 | `	int nName = 0;` |
|    218 |  9067 | `	SXUNUSED(nArg);` |
|    218 |  9068 | `	SXUNUSED(apArg);` |
|    440 |  9069 | `	if( pThis ){` |
|    440 |  9070 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    218 |  9071 | `	}` |
|    440 |  9072 | `	ph7_result_string(pCtx, zName, nName);` |
|    440 |  9073 | `	return PH7_OK;` |
|      4 |  9074 | `}` |
|      - |  9075 | `/*` |
|      - |  9076 | ` * getMangledName(): the name php stores the slot under —  plain for a public` |
|      - |  9077 | ` * property, "\0*\0name" for a protected one and "\0Class\0name" for a private` |
|      - |  9078 | ` * one. PHL's tables are not mangled, so the name is COMPOSED here; what php` |
|      - |  9079 | ` * exposes is the spelling, not the storage.` |
|      - |  9080 | ` */` |
|      6 |  9081 | `static int vm_builtin_ReflectionProperty_getMangledName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9082 | `{` |
|      - |  9083 | `	ReflectMemberRef sRef;` |
|      - |  9084 | `	SyBlob sOut;` |
|      3 |  9085 | `	SXUNUSED(nArg);` |
|      3 |  9086 | `	SXUNUSED(apArg);` |
|      7 |  9087 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9088 | `		ph7_result_string(pCtx, sRef.zName, sRef.nName);` |
|    ! 0 |  9089 | `		return PH7_OK;` |
|      - |  9090 | `	}` |
|      7 |  9091 | `	if( sRef.pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|      3 |  9092 | `		ph7_result_string(pCtx, sRef.zName, sRef.nName);` |
|      3 |  9093 | `		return PH7_OK;` |
|      - |  9094 | `	}` |
|      5 |  9095 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|      5 |  9096 | `	SyBlobAppend(&sOut, "\0", 1);` |
|      5 |  9097 | `	if( sRef.pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|      3 |  9098 | `		SyBlobAppend(&sOut, "*", 1);` |
|      2 |  9099 | `	}else{` |
|      3 |  9100 | `		ph7_class *pDecl = ReflectMemberDecl(&sRef);` |
|      3 |  9101 | `		SyBlobAppend(&sOut, SyStringData(&pDecl->sName), SyStringLength(&pDecl->sName));` |
|      - |  9102 | `	}` |
|      5 |  9103 | `	SyBlobAppend(&sOut, "\0", 1);` |
|      5 |  9104 | `	SyBlobAppend(&sOut, sRef.zName, (sxu32)sRef.nName);` |
|      5 |  9105 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|      5 |  9106 | `	SyBlobRelease(&sOut);` |
|      5 |  9107 | `	return PH7_OK;` |
|      4 |  9108 | `}` |
|      - |  9109 | `/* The boolean predicates, all off the declared attribute. */` |
|    278 |  9110 | `static int ReflectPropFlag(ph7_context *pCtx, int iWhat)` |
|      3 |  9111 | `{` |
|      - |  9112 | `	ReflectMemberRef sRef;` |
|    281 |  9113 | `	int bYes = 0;` |
|    418 |  9114 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|    277 |  9115 | `		ph7_class_attr *pAttr = sRef.pAttr;` |
|    277 |  9116 | `		switch( iWhat ){` |
|      5 |  9117 | `		case 0: bYes = pAttr->iProtection == PH7_CLASS_PROT_PUBLIC; break;` |
|    ! 0 |  9118 | `		case 1: bYes = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE; break;` |
|      3 |  9119 | `		case 2: bYes = pAttr->iProtection == PH7_CLASS_PROT_PROTECTED; break;` |
|     28 |  9120 | `		case 3: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) != 0; break;` |
|     28 |  9121 | `		case 4: bYes = ReflectPropProtectedSet(pAttr); break;` |
|     16 |  9122 | `		case 5: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0; break;` |
|     70 |  9123 | `		case 6: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0; break;` |
|      3 |  9124 | `		case 7: bYes = 1; break;                                    /* isDefault */` |
|    ! 0 |  9125 | `		case 8: bYes = 0; break;                                    /* isDynamic */` |
|    177 |  9126 | `		case 9: bYes = (pAttr->iFlags` |
|    177 |  9127 | `			& (PH7_CLASS_ATTR_HOOK_VIRTUAL\|PH7_CLASS_ATTR_NATIVE_VIRTUAL)) != 0; break;` |
|      - |  9128 | ``		/* isFinal: the DECLARED `final` (PHP 8.4) or the one private(set) implies`` |
|      - |  9129 | `		 * -- php answers true for both, the same pair its modifier mask carries. */` |
|     17 |  9130 | `		case 10: bYes = (pAttr->iFlags` |
|     17 |  9131 | `			& (PH7_CLASS_ATTR_FINAL\|PH7_CLASS_ATTR_PRIVATE_SET)) != 0; break;` |
|      3 |  9132 | `		default:  /* 11: hasHooks */` |
|      7 |  9133 | `			bYes = (pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) != 0;` |
|      6 |  9134 | `			break;` |
|      - |  9135 | `		}` |
|    144 |  9136 | `	}else if( iWhat == 0 \|\| iWhat == 8 ){` |
|      - |  9137 | `		/* A dynamic property is public and, by definition, not a default one. */` |
|      3 |  9138 | `		bYes = 1;` |
|      1 |  9139 | `	}` |
|    281 |  9140 | `	ph7_result_bool(pCtx, bYes);` |
|    281 |  9141 | `	return PH7_OK;` |
|      3 |  9142 | `}` |
|      - |  9143 | `#define REFLECT_PROP_FLAG(NAME,WHAT) \` |
|      - |  9144 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  9145 | `	{ \` |
|      - |  9146 | `		SXUNUSED(nArg); \` |
|      - |  9147 | `		SXUNUSED(apArg); \` |
|      - |  9148 | `		return ReflectPropFlag(pCtx, WHAT); \` |
|      - |  9149 | `	}` |
|      5 |  9150 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPublic, 0)` |
|    ! 0 |  9151 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPrivate, 1)` |
|      3 |  9152 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isProtected, 2)` |
|     28 |  9153 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPrivateSet, 3)` |
|     28 |  9154 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isProtectedSet, 4)` |
|     16 |  9155 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isStatic, 5)` |
|     70 |  9156 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isReadOnly, 6)` |
|     12 |  9157 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isFinal, 10)` |
|      5 |  9158 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isDefault, 7)` |
|      3 |  9159 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isDynamic, 8)` |
|    119 |  9160 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isVirtual, 9)` |
|      7 |  9161 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_hasHooks, 11)` |
|      - |  9162 |  |
|      - |  9163 | `/* php has no abstract properties outside an interface stub, and PHL none at` |
|      - |  9164 | ` * all; isLazy() answers what is true of a VM without lazy objects. */` |
|    ! 0 |  9165 | `static int vm_builtin_ReflectionProperty_false(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  9166 | `{` |
|    ! 0 |  9167 | `	SXUNUSED(nArg);` |
|    ! 0 |  9168 | `	SXUNUSED(apArg);` |
|    ! 0 |  9169 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 |  9170 | `	return PH7_OK;` |
|    ! 0 |  9171 | `}` |
|      - |  9172 | `/*` |
|      - |  9173 | ` * isPromoted(): the property came from a constructor-promoted parameter. PHL` |
|      - |  9174 | ` * records promotion on the ARGUMENT (VM_FUNC_ARG_PROMOTED), not on the` |
|      - |  9175 | ` * attribute, so the constructor's parameter list is what answers.` |
|      - |  9176 | ` */` |
|      6 |  9177 | `static int vm_builtin_ReflectionProperty_isPromoted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9178 | `{` |
|      - |  9179 | `	ReflectMemberRef sRef;` |
|      - |  9180 | `	ph7_class_method *pCons;` |
|      - |  9181 | `	ph7_vm_func_arg *aArg;` |
|      - |  9182 | `	sxu32 n;` |
|      7 |  9183 | `	int bYes = 0;` |
|      3 |  9184 | `	SXUNUSED(nArg);` |
|      3 |  9185 | `	SXUNUSED(apArg);` |
|      7 |  9186 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|      7 |  9187 | `		ph7_class *pDecl = ReflectMemberDecl(&sRef);` |
|      7 |  9188 | `		pCons = PH7_ClassExtractMethod(pDecl, "__construct", sizeof("__construct")-1);` |
|      7 |  9189 | `		if( pCons ){` |
|      7 |  9190 | `			aArg = (ph7_vm_func_arg *)SySetBasePtr(&pCons->sFunc.aArgs);` |
|      9 |  9191 | `			for( n = 0 ; n < SySetUsed(&pCons->sFunc.aArgs) ; n++ ){` |
|      7 |  9192 | `				if( (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|    ! 0 |  9193 | `					continue;` |
|      - |  9194 | `				}` |
|      6 |  9195 | `				if( SyStringLength(&aArg[n].sName) == (sxu32)sRef.nName` |
|      6 |  9196 | `				 && SyMemcmp(SyStringData(&aArg[n].sName), sRef.zName, (sxu32)sRef.nName) == 0 ){` |
|      5 |  9197 | `					bYes = 1;` |
|      5 |  9198 | `					break;` |
|      - |  9199 | `				}` |
|      2 |  9200 | `			}` |
|      3 |  9201 | `		}` |
|      3 |  9202 | `	}` |
|      7 |  9203 | `	ph7_result_bool(pCtx, bYes);` |
|      7 |  9204 | `	return PH7_OK;` |
|      1 |  9205 | `}` |
|    206 |  9206 | `static int vm_builtin_ReflectionProperty_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  9207 | `{` |
|      - |  9208 | `	ReflectMemberRef sRef;` |
|    103 |  9209 | `	SXUNUSED(nArg);` |
|    103 |  9210 | `	SXUNUSED(apArg);` |
|    208 |  9211 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9212 | `		ph7_result_int(pCtx, 1);   /* a dynamic property is public */` |
|    ! 0 |  9213 | `		return PH7_OK;` |
|      - |  9214 | `	}` |
|    208 |  9215 | `	ph7_result_int64(pCtx, ReflectPropModifiers(sRef.pAttr));` |
|    208 |  9216 | `	return PH7_OK;` |
|    105 |  9217 | `}` |
|    222 |  9218 | `static int vm_builtin_ReflectionProperty_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  9219 | `{` |
|      - |  9220 | `	ReflectMemberRef sRef;` |
|    111 |  9221 | `	SXUNUSED(nArg);` |
|    111 |  9222 | `	SXUNUSED(apArg);` |
|    224 |  9223 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  9224 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9225 | `		return PH7_OK;` |
|      - |  9226 | `	}` |
|    224 |  9227 | `	return ReflectResultClassOf(pCtx, ReflectMemberDecl(&sRef));` |
|    113 |  9228 | `}` |
|      6 |  9229 | `static int vm_builtin_ReflectionProperty_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9230 | `{` |
|      - |  9231 | `	ReflectMemberRef sRef;` |
|      3 |  9232 | `	SXUNUSED(nArg);` |
|      3 |  9233 | `	SXUNUSED(apArg);` |
|      6 |  9234 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr` |
|      7 |  9235 | `	 && SyStringLength(&sRef.pAttr->sDoc) > 0 ){` |
|      7 |  9236 | `		ph7_result_string(pCtx, SyStringData(&sRef.pAttr->sDoc),` |
|      4 |  9237 | `			(int)SyStringLength(&sRef.pAttr->sDoc));` |
|      3 |  9238 | `	}else{` |
|      3 |  9239 | `		ph7_result_bool(pCtx, 0);` |
|      - |  9240 | `	}` |
|      7 |  9241 | `	return PH7_OK;` |
|      1 |  9242 | `}` |
|     82 |  9243 | `static int vm_builtin_ReflectionProperty_hasType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9244 | `{` |
|      - |  9245 | `	ReflectMemberRef sRef;` |
|     41 |  9246 | `	SXUNUSED(nArg);` |
|     41 |  9247 | `	SXUNUSED(apArg);` |
|    165 |  9248 | `	ph7_result_bool(pCtx, ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP)` |
|     82 |  9249 | `		&& sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);` |
|     83 |  9250 | `	return PH7_OK;` |
|      1 |  9251 | `}` |
|    196 |  9252 | `static int vm_builtin_ReflectionProperty_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  9253 | `{` |
|      - |  9254 | `	ReflectMemberRef sRef;` |
|     98 |  9255 | `	SXUNUSED(nArg);` |
|     98 |  9256 | `	SXUNUSED(apArg);` |
|    196 |  9257 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0` |
|    196 |  9258 | `	 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|    197 |  9259 | `	 \|\| SyStringLength(&sRef.pAttr->sTypeName) < 1 ){` |
|      7 |  9260 | `		ph7_result_null(pCtx);` |
|      7 |  9261 | `		return PH7_OK;` |
|      - |  9262 | `	}` |
|      - |  9263 | `	{` |
|      - |  9264 | `		char zType[192];` |
|    194 |  9265 | `		const char *zT = ReflectMemberTypeText(pCtx->pVm,sRef.pAttr,zType,sizeof(zType));` |
|    194 |  9266 | `		return ReflectResultObject(pCtx, ReflectMakeType(pCtx, zT, (int)SyStrlen(zT)));` |
|      - |  9267 | `	}` |
|    102 |  9268 | `}` |
|    166 |  9269 | `static int vm_builtin_ReflectionProperty_hasDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  9270 | `{` |
|      - |  9271 | `	ReflectMemberRef sRef;` |
|     83 |  9272 | `	SXUNUSED(nArg);` |
|     83 |  9273 | `	SXUNUSED(apArg);` |
|    169 |  9274 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9275 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  9276 | `		return PH7_OK;` |
|      - |  9277 | `	}` |
|    169 |  9278 | `	if( SySetUsed(&sRef.pAttr->aByteCode) > 0 \|\| sRef.pAttr->pNativeValue ){` |
|     64 |  9279 | `		ph7_result_bool(pCtx, 1);` |
|     64 |  9280 | `		return PH7_OK;` |
|      - |  9281 | `	}` |
|      - |  9282 | `	/* An UNTYPED property with no initializer still defaults to null. */` |
|    107 |  9283 | `	ph7_result_bool(pCtx, (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0);` |
|    107 |  9284 | `	return PH7_OK;` |
|     86 |  9285 | `}` |
|     72 |  9286 | `static int vm_builtin_ReflectionProperty_getDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  9287 | `{` |
|      - |  9288 | `	ReflectMemberRef sRef;` |
|      - |  9289 | `	ph7_value sValue;` |
|     36 |  9290 | `	SXUNUSED(nArg);` |
|     36 |  9291 | `	SXUNUSED(apArg);` |
|     75 |  9292 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  9293 | `		sRef.pAttr = 0;` |
|    ! 0 |  9294 | `	}` |
|     75 |  9295 | `	if( sRef.pAttr && sRef.pAttr->pNativeValue && SySetUsed(&sRef.pAttr->aByteCode) < 1 ){` |
|      - |  9296 | `		/* A NATIVE property's default is a LITERAL, not byte-code — the same` |
|      - |  9297 | ``		 * record `new` materializes (PH7_NativeLiteralValue). Reading only the`` |
|      - |  9298 | `		 * byte-code answered NULL for every declared native default and raised` |
|      - |  9299 | `		 * php's no-default deprecation on a slot that hasDefaultValue() had just` |
|      - |  9300 | `		 * reported true for. */` |
|     21 |  9301 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     21 |  9302 | `		PH7_NativeLiteralValue(pCtx->pVm, sRef.pAttr->pNativeValue, &sValue);` |
|     21 |  9303 | `		ph7_result_value(pCtx, &sValue);` |
|     21 |  9304 | `		PH7_MemObjRelease(&sValue);` |
|     21 |  9305 | `		return PH7_OK;` |
|      - |  9306 | `	}` |
|     55 |  9307 | `	if( sRef.pAttr == 0 \|\| SySetUsed(&sRef.pAttr->aByteCode) < 1 ){` |
|      - |  9308 | `		/* php 8.5 deprecates the question when there is no default — an` |
|      - |  9309 | `		 * UNTYPED property still has one (null), a typed one without an` |
|      - |  9310 | `		 * initializer does not. */` |
|      5 |  9311 | `		if( sRef.pAttr == 0 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|      3 |  9312 | `			PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED,` |
|      - |  9313 | `				"ReflectionProperty::getDefaultValue() for a property without a default "` |
|      - |  9314 | `				"value is deprecated, use ReflectionProperty::hasDefaultValue() to check "` |
|      - |  9315 | `				"if the default value exists");` |
|      1 |  9316 | `		}` |
|      5 |  9317 | `		ph7_result_null(pCtx);` |
|      5 |  9318 | `		return PH7_OK;` |
|      - |  9319 | `	}` |
|      - |  9320 | `	/* Same evaluation path the VM uses for an omitted call argument */` |
|     51 |  9321 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     51 |  9322 | `	VmLocalExec(pCtx->pVm, &sRef.pAttr->aByteCode, &sValue, FALSE);` |
|     51 |  9323 | `	PH7_VmResolvedDefault(pCtx->pVm, sRef.pClass, sRef.pAttr, &sValue);` |
|     51 |  9324 | `	ph7_result_value(pCtx, &sValue);` |
|     51 |  9325 | `	PH7_MemObjRelease(&sValue);` |
|     51 |  9326 | `	return PH7_OK;` |
|     39 |  9327 | `}` |
|      - |  9328 | `/* php 8.1 made every reflected member accessible; the setter is a no-op it kept. */` |
|    ! 0 |  9329 | `static int vm_builtin_ReflectionProperty_setAccessible(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  9330 | `{` |
|    ! 0 |  9331 | `	SXUNUSED(pCtx);` |
|    ! 0 |  9332 | `	SXUNUSED(nArg);` |
|    ! 0 |  9333 | `	SXUNUSED(apArg);` |
|    ! 0 |  9334 | `	return PH7_OK;` |
|    ! 0 |  9335 | `}` |
|      - |  9336 | `/* getValue()/setValue()/isInitialized() all need the same receiver check. */` |
|    100 |  9337 | `static sxi32 ReflectPropReceiver(ph7_context *pCtx, ReflectMemberRef *pRef,` |
|      - |  9338 | `	ph7_value *pObject, ph7_class_instance **ppThis, const char *zWho)` |
|      1 |  9339 | `{` |
|    101 |  9340 | `	*ppThis = 0;` |
|     50 |  9341 | `	SXUNUSED(pRef);` |
|    101 |  9342 | `	if( pObject && (pObject->iFlags & MEMOBJ_OBJ) ){` |
|     99 |  9343 | `		*ppThis = (ph7_class_instance *)pObject->x.pOther;` |
|     99 |  9344 | `		return PH7_OK;` |
|      - |  9345 | `	}` |
|      4 |  9346 | `	return PH7_VmThrowException(pCtx, "TypeError",` |
|      - |  9347 | `		"ReflectionProperty::%s(): Argument #1 ($object) must be provided for instance properties",` |
|      1 |  9348 | `		zWho);` |
|     51 |  9349 | `}` |
|     54 |  9350 | `static int vm_builtin_ReflectionProperty_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  9351 | `{` |
|      - |  9352 | `	ReflectMemberRef sRef;` |
|      - |  9353 | `	ph7_class_instance *pObj;` |
|      - |  9354 | `	VmClassAttr *pVmAttr;` |
|      - |  9355 | `	ph7_value *pValue;` |
|      - |  9356 | `	sxi32 rc;` |
|     56 |  9357 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  9358 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9359 | `		return PH7_OK;` |
|      - |  9360 | `	}` |
|     56 |  9361 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|      8 |  9362 | `		return ReflectStaticSlotRead(pCtx, sRef.pClass, sRef.pAttr);` |
|      - |  9363 | `	}` |
|     49 |  9364 | `	rc = ReflectPropReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pObj, "getValue");` |
|     49 |  9365 | `	if( rc != PH7_OK ){` |
|      3 |  9366 | `		return rc;` |
|      - |  9367 | `	}` |
|     47 |  9368 | `	pVmAttr = ReflectRefInstanceAttr(pCtx->pVm, pObj, &sRef);` |
|     47 |  9369 | `	if( pVmAttr == 0 ){` |
|      - |  9370 | `		/* No slot: a class whose properties are its own handlers answers here,` |
|      - |  9371 | ``		 * as php's does -- Reflection reads a PDORow's `queryString` through`` |
|      - |  9372 | ``		 * read_property exactly as `$row->queryString` does. */`` |
|      - |  9373 | `		PH7_NativePropCtx sNat;` |
|      - |  9374 | `		SyString sNatName;` |
|      - |  9375 | `		ph7_value sNatVal;` |
|     11 |  9376 | `		SyStringInitFromBuf(&sNatName, sRef.zName, (sxu32)sRef.nName);` |
|     11 |  9377 | `		PH7_MemObjInit(pCtx->pVm, &sNatVal);` |
|     11 |  9378 | `		if( PH7_ClassNativePropAsk(pObj, &sNat, PH7_NATIVE_PROP_READ, &sNatName, &sNatVal) ){` |
|      7 |  9379 | `			if( sNat.zThrowClass ){` |
|    ! 0 |  9380 | `				PH7_MemObjRelease(&sNatVal);` |
|    ! 0 |  9381 | `				return PH7_VmThrowExceptionCode(pCtx, sNat.zThrowClass, sNat.iThrowCode,` |
|    ! 0 |  9382 | `					"%s", sNat.zThrowMsg);` |
|      - |  9383 | `			}` |
|      7 |  9384 | `			ph7_result_value(pCtx, &sNatVal);` |
|      7 |  9385 | `			PH7_MemObjRelease(&sNatVal);` |
|      7 |  9386 | `			return PH7_OK;` |
|      - |  9387 | `		}` |
|      5 |  9388 | `		PH7_MemObjRelease(&sNatVal);` |
|      5 |  9389 | `		if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){` |
|      - |  9390 | `` 			/* A VIRTUAL property: php reads it through the same handler `$o->p` `` |
|      - |  9391 | `			 * reaches, so Reflection answers the VALUE rather than warning that a` |
|      - |  9392 | `			 * name the class declares is undefined. A class whose handlers are its` |
|      - |  9393 | `			 * magic trio (ext/dom) is read through __get, which is the door the` |
|      - |  9394 | `			 * member opcode takes for the very same name. */` |
|      - |  9395 | `			ph7_value sMagic;` |
|      - |  9396 | `			SyString sMagicName;` |
|    ! 0 |  9397 | `			SyStringInitFromBuf(&sMagicName, sRef.zName, (sxu32)sRef.nName);` |
|    ! 0 |  9398 | `			PH7_MemObjInit(pCtx->pVm, &sMagic);` |
|    ! 0 |  9399 | `			if( PH7_ClassInstanceCallMagicMethod(pCtx->pVm, pObj->pClass, pObj,` |
|    ! 0 |  9400 | `					"__get", sizeof("__get")-1, &sMagicName, &sMagic) == SXRET_OK ){` |
|    ! 0 |  9401 | `				ph7_result_value(pCtx, &sMagic);` |
|    ! 0 |  9402 | `				PH7_MemObjRelease(&sMagic);` |
|    ! 0 |  9403 | `				return PH7_OK;` |
|      - |  9404 | `			}` |
|    ! 0 |  9405 | `			PH7_MemObjRelease(&sMagic);` |
|    ! 0 |  9406 | `			ph7_result_null(pCtx);` |
|    ! 0 |  9407 | `			return PH7_OK;` |
|      - |  9408 | `		}` |
|      4 |  9409 | `		if( sRef.pAttr` |
|      5 |  9410 | `		 && (sRef.pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|      2 |  9411 | `		                           \|PH7_CLASS_ATTR_HIDDEN)) == 0 ){` |
|      - |  9412 | `			/* A DECLARED property the object no longer holds -- unset() took it.` |
|      - |  9413 | ``			 * php reads it the way `$o->p` does, and warns exactly the same:`` |
|      - |  9414 | ``			 * `Undefined property: C::$p`, naming the OBJECT's class. */`` |
|      7 |  9415 | `			VmErrorFormat(pCtx->pVm, PH7_CTX_WARNING, "Undefined property: %z::$%.*s",` |
|      4 |  9416 | `				&pObj->pClass->sDisp, sRef.nName, sRef.zName);` |
|      2 |  9417 | `		}` |
|      5 |  9418 | `		ph7_result_null(pCtx);` |
|      5 |  9419 | `		return PH7_OK;` |
|      - |  9420 | `	}` |
|     37 |  9421 | `	if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|      3 |  9422 | `		ph7_class *pDecl = PH7_VmMemberOwnerClass(` |
|      4 |  9423 | `			pVmAttr->pAttr ? pVmAttr->pAttr->pDeclClass : 0,pObj->pClass);` |
|      7 |  9424 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      - |  9425 | `			"Typed property %z::$%.*s must not be accessed before initialization",` |
|      2 |  9426 | `			&pDecl->sDisp, sRef.nName, sRef.zName);` |
|      - |  9427 | `	}` |
|     33 |  9428 | `	pValue = PH7_ClassInstanceExtractAttrValue(pObj, pVmAttr);` |
|     33 |  9429 | `	if( pValue ){` |
|     33 |  9430 | `		ph7_result_value(pCtx, pValue);` |
|     17 |  9431 | `	}else{` |
|    ! 0 |  9432 | `		ph7_result_null(pCtx);` |
|      - |  9433 | `	}` |
|     33 |  9434 | `	return PH7_OK;` |
|     29 |  9435 | `}` |
|     30 |  9436 | `static int vm_builtin_ReflectionProperty_setValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  9437 | `{` |
|      - |  9438 | `	ReflectMemberRef sRef;` |
|      - |  9439 | `	ph7_class_instance *pObj;` |
|      - |  9440 | `	VmClassAttr *pVmAttr;` |
|      - |  9441 | `	ph7_value *pSlot;` |
|      - |  9442 | `	sxi32 rc;` |
|     32 |  9443 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| nArg < 1 ){` |
|    ! 0 |  9444 | `		return PH7_OK;` |
|      - |  9445 | `	}` |
|     32 |  9446 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|      - |  9447 | `		/* php's one-argument spelling for a static: setValue($value). Two` |
|      - |  9448 | `		 * arguments, or one OBJECT, means the instance form was used and the` |
|      - |  9449 | `		 * value is the second. */` |
|      6 |  9450 | `		ph7_value *pVal = apArg[0];` |
|      6 |  9451 | `		if( nArg > 1 ){` |
|      6 |  9452 | `			pVal = apArg[1];` |
|      2 |  9453 | `		}else if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|    ! 0 |  9454 | `			return PH7_OK;` |
|      - |  9455 | `		}` |
|      6 |  9456 | `		return ReflectStaticSlotWrite(pCtx, sRef.pClass, sRef.pAttr, pVal);` |
|      - |  9457 | `	}` |
|     27 |  9458 | `	rc = ReflectPropReceiver(pCtx, &sRef, apArg[0], &pObj, "setValue");` |
|     27 |  9459 | `	if( rc != PH7_OK ){` |
|    ! 0 |  9460 | `		return rc;` |
|      - |  9461 | `	}` |
|     27 |  9462 | `	pVmAttr = ReflectRefInstanceAttr(pCtx->pVm, pObj, &sRef);` |
|     27 |  9463 | `	if( pVmAttr == 0 ){` |
|      - |  9464 | `		/* No slot: the write handler answers, and for a class that has one the` |
|      - |  9465 | `		 * answer is its own refusal (php's PDORow refuses Reflection's write` |
|      - |  9466 | ``		 * with the same sentence `$row->p = 1` takes). */`` |
|      - |  9467 | `		PH7_NativePropCtx sNat;` |
|      - |  9468 | `		SyString sNatName;` |
|      - |  9469 | `		ph7_value sNatVal;` |
|     11 |  9470 | `		SyStringInitFromBuf(&sNatName, sRef.zName, (sxu32)sRef.nName);` |
|     11 |  9471 | `		PH7_MemObjInit(pCtx->pVm, &sNatVal);` |
|     10 |  9472 | `		if( PH7_ClassNativePropAsk(pObj, &sNat, PH7_NATIVE_PROP_WRITE, &sNatName, &sNatVal)` |
|      7 |  9473 | `		 && sNat.zThrowClass ){` |
|      3 |  9474 | `			PH7_MemObjRelease(&sNatVal);` |
|      6 |  9475 | `			return PH7_VmThrowExceptionCode(pCtx, sNat.zThrowClass, sNat.iThrowCode,` |
|      1 |  9476 | `				"%s", sNat.zThrowMsg);` |
|      - |  9477 | `		}` |
|      9 |  9478 | `		PH7_MemObjRelease(&sNatVal);` |
|      9 |  9479 | `		if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){` |
|      - |  9480 | `` 			/* A VIRTUAL property: php's write goes to the same handler `$o->p = v` `` |
|      - |  9481 | `			 * reaches -- the class's own write_property, which refuses the read-only` |
|      - |  9482 | `			 * half of the surface with its own sentence. Creating a slot here would` |
|      - |  9483 | `			 * give the object a real property php has none of. */` |
|      5 |  9484 | `			ph7_value sMagicVal, *pMagicArg = nArg > 1 ? apArg[1] : 0;` |
|      5 |  9485 | `			ph7_class_method *pSet = PH7_ClassExtractMethod(pObj->pClass,` |
|      - |  9486 | `				"__set", sizeof("__set")-1);` |
|      5 |  9487 | `			PH7_MemObjInit(pCtx->pVm, &sMagicVal);` |
|      5 |  9488 | `			if( pMagicArg == 0 ){` |
|    ! 0 |  9489 | `				pMagicArg = &sMagicVal;` |
|    ! 0 |  9490 | `			}` |
|      5 |  9491 | `			if( PH7_ClassNativePropOwns(pObj, &sNatName) ){` |
|      - |  9492 | `				PH7_NativePropCtx sStore;` |
|      5 |  9493 | `				sxi32 rcSt = PH7_OK;` |
|      6 |  9494 | `				if( PH7_ClassNativePropAsk(pObj, &sStore, PH7_NATIVE_PROP_STORE,` |
|      2 |  9495 | `						&sNatName, pMagicArg)` |
|      5 |  9496 | `				 && sStore.zThrowClass ){` |
|      4 |  9497 | `					rcSt = PH7_VmThrowExceptionCode(pCtx, sStore.zThrowClass,` |
|      1 |  9498 | `						sStore.iThrowCode, "%s", sStore.zThrowMsg);` |
|      1 |  9499 | `				}` |
|      5 |  9500 | `				PH7_MemObjRelease(&sMagicVal);` |
|      5 |  9501 | `				return rcSt;` |
|      - |  9502 | `			}` |
|    ! 0 |  9503 | `			if( pSet ){` |
|      - |  9504 | `				ph7_value sMagicName, *apMagic[2];` |
|    ! 0 |  9505 | `				PH7_MemObjInitFromString(pCtx->pVm, &sMagicName, 0);` |
|    ! 0 |  9506 | `				PH7_MemObjStringAppend(&sMagicName, sRef.zName, (sxu32)sRef.nName);` |
|    ! 0 |  9507 | `				sMagicName.nIdx = SXU32_HIGH;` |
|    ! 0 |  9508 | `				apMagic[0] = &sMagicName;` |
|    ! 0 |  9509 | `				apMagic[1] = pMagicArg;` |
|    ! 0 |  9510 | `				rc = PH7_VmCallMagicMethod(pCtx->pVm, pObj, pSet, 0, 2, apMagic);` |
|    ! 0 |  9511 | `				PH7_MemObjRelease(&sMagicName);` |
|    ! 0 |  9512 | `				PH7_MemObjRelease(&sMagicVal);` |
|    ! 0 |  9513 | `				return rc == PH7_ABORT ? PH7_ABORT : PH7_OK;` |
|      - |  9514 | `			}` |
|    ! 0 |  9515 | `			PH7_MemObjRelease(&sMagicVal);` |
|    ! 0 |  9516 | `			return PH7_OK;` |
|      - |  9517 | `		}` |
|      4 |  9518 | `		if( sRef.pAttr` |
|      5 |  9519 | `		 && (sRef.pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|      2 |  9520 | `		                           \|PH7_CLASS_ATTR_HIDDEN)) == 0 ){` |
|      - |  9521 | `			/* A DECLARED property unset() took away: php's write RE-CREATES it,` |
|      - |  9522 | ``			 * exactly as `$o->p = v` does. PHL wrote nowhere and said nothing. */`` |
|      5 |  9523 | `			VmRecreateDeclaredAttr(pCtx->pVm, pObj, sRef.pAttr, &pVmAttr);` |
|      2 |  9524 | `		}` |
|      5 |  9525 | `		if( pVmAttr == 0 ){` |
|    ! 0 |  9526 | `			return PH7_OK;` |
|      - |  9527 | `		}` |
|      2 |  9528 | `	}` |
|     21 |  9529 | `	if( pVmAttr->pAttr && (pVmAttr->iState & VM_CLASS_ATTR_RDONLY) ){` |
|      - |  9530 | `		/* php's read-only handler refuses Reflection's write with the sentence` |
|      - |  9531 | ``		 * `$stmt->queryString = 'x'` takes. */`` |
|      3 |  9532 | `		return VmThrowNativeReadOnly(pCtx->pVm, pVmAttr->pAttr);` |
|      - |  9533 | `	}` |
|      - |  9534 | `	{` |
|     19 |  9535 | `		ph7_value *pVal = nArg > 1 ? apArg[1] : 0;` |
|      - |  9536 | `		ph7_value sNull;` |
|     19 |  9537 | `		PH7_MemObjInit(pCtx->pVm, &sNull);` |
|     19 |  9538 | `		if( pVal == 0 ){` |
|    ! 0 |  9539 | `			pVal = &sNull;` |
|    ! 0 |  9540 | `		}` |
|     19 |  9541 | `		rc = ReflectEnforceStore(pCtx, pVmAttr->nIdx, pVal);` |
|     19 |  9542 | `		if( rc != SXRET_OK ){` |
|      5 |  9543 | `			PH7_MemObjRelease(&sNull);` |
|      5 |  9544 | `			return rc;` |
|      - |  9545 | `		}` |
|     15 |  9546 | `		pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|     15 |  9547 | `		if( pSlot ){` |
|     15 |  9548 | `			PH7_MemObjStore(pVal, pSlot);` |
|     15 |  9549 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      7 |  9550 | `		}` |
|     15 |  9551 | `		PH7_MemObjRelease(&sNull);` |
|      - |  9552 | `	}` |
|     15 |  9553 | `	return PH7_OK;` |
|     17 |  9554 | `}` |
|     32 |  9555 | `static int vm_builtin_ReflectionProperty_isInitialized(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  9556 | `{` |
|      - |  9557 | `	ReflectMemberRef sRef;` |
|      - |  9558 | `	ph7_class_instance *pObj;` |
|      - |  9559 | `	VmClassAttr *pVmAttr;` |
|      - |  9560 | `	sxi32 rc;` |
|     34 |  9561 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  9562 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  9563 | `		return PH7_OK;` |
|      - |  9564 | `	}` |
|     34 |  9565 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|      - |  9566 | `		SyHashEntry *pSlot;` |
|      8 |  9567 | `		rc = ReflectMaterializeStatics(pCtx, sRef.pClass);` |
|      8 |  9568 | `		if( rc != SXRET_OK ){` |
|      5 |  9569 | `			return rc;` |
|      - |  9570 | `		}` |
|      3 |  9571 | `		pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&sRef.pAttr->nIdx, sizeof(sxu32));` |
|      5 |  9572 | `		ph7_result_bool(pCtx, pSlot == 0` |
|      2 |  9573 | `			\|\| (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) == 0);` |
|      3 |  9574 | `		return PH7_OK;` |
|      - |  9575 | `	}` |
|     27 |  9576 | `	rc = ReflectPropReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pObj, "isInitialized");` |
|     27 |  9577 | `	if( rc != PH7_OK ){` |
|    ! 0 |  9578 | `		return rc;` |
|      - |  9579 | `	}` |
|     27 |  9580 | `	pVmAttr = ReflectRefInstanceAttr(pCtx->pVm, pObj, &sRef);` |
|     26 |  9581 | `	if( pVmAttr == 0 && sRef.pAttr` |
|      5 |  9582 | `	 && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){` |
|      - |  9583 | `		/* A VIRTUAL property has no slot to be uninitialized: php asks the` |
|      - |  9584 | `		 * has_property handler, and ext/dom's answers yes for every name it` |
|      - |  9585 | `		 * declares. */` |
|      3 |  9586 | `		ph7_result_bool(pCtx, 1);` |
|      3 |  9587 | `		return PH7_OK;` |
|      - |  9588 | `	}` |
|     25 |  9589 | `	ph7_result_bool(pCtx, pVmAttr != 0 && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0);` |
|     25 |  9590 | `	return PH7_OK;` |
|     18 |  9591 | `}` |
|      - |  9592 | `/*` |
|      - |  9593 | `` * The property HOOKS (php 8.4). PHL compiles `public $p { get => ...; }` into`` |
|      - |  9594 | `` * synthesized methods named `__phl_hook_get_NAME` / `__phl_hook_set_NAME`, so`` |
|      - |  9595 | ` * the reflector php answers is a ReflectionMethod over one of those.` |
|      - |  9596 | ` */` |
|     54 |  9597 | `static int ReflectHookMethod(ph7_context *pCtx, ReflectMemberRef *pRef, int bSet,` |
|      - |  9598 | `	ph7_class_instance **ppOut)` |
|      4 |  9599 | `{` |
|      - |  9600 | `	char zName[128];` |
|      - |  9601 | `	ph7_value sClass, sName;` |
|      - |  9602 | `	ph7_value *apCtor[2];` |
|      - |  9603 | `	ph7_class *pDecl;` |
|      - |  9604 | `	sxi32 rc;` |
|      - |  9605 | `	int nName;` |
|     58 |  9606 | `	*ppOut = 0;` |
|     58 |  9607 | `	if( pRef->pAttr == 0 ){` |
|    ! 0 |  9608 | `		return PH7_OK;` |
|      - |  9609 | `	}` |
|     58 |  9610 | `	if( (pRef->pAttr->iFlags & (bSet ? PH7_CLASS_ATTR_HOOK_SET : PH7_CLASS_ATTR_HOOK_GET)) == 0 ){` |
|     17 |  9611 | `		return PH7_OK;` |
|      - |  9612 | `	}` |
|     44 |  9613 | `	pDecl = ReflectMemberDecl(pRef);` |
|     64 |  9614 | `	nName = SyBufferFormat(zName, sizeof(zName), "%s%.*s",` |
|     20 |  9615 | `		bSet ? "__phl_hook_set_" : "__phl_hook_get_", pRef->nName, pRef->zName);` |
|     44 |  9616 | `	PH7_MemObjInit(pCtx->pVm, &sClass);` |
|     44 |  9617 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|     44 |  9618 | `	ph7_value_string(&sClass, SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|     44 |  9619 | `	ph7_value_string(&sName, zName, nName);` |
|     44 |  9620 | `	apCtor[0] = &sClass;` |
|     44 |  9621 | `	apCtor[1] = &sName;` |
|     44 |  9622 | `	*ppOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|     44 |  9623 | `	PH7_MemObjRelease(&sClass);` |
|     44 |  9624 | `	PH7_MemObjRelease(&sName);` |
|     44 |  9625 | `	return rc;` |
|     31 |  9626 | `}` |
|     18 |  9627 | `static int vm_builtin_ReflectionProperty_getHooks(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  9628 | `{` |
|      - |  9629 | `	ReflectMemberRef sRef;` |
|     21 |  9630 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  9631 | `	int iHook;` |
|      9 |  9632 | `	SXUNUSED(nArg);` |
|      9 |  9633 | `	SXUNUSED(apArg);` |
|     21 |  9634 | `	if( pOut == 0 ){` |
|    ! 0 |  9635 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  9636 | `	}` |
|     21 |  9637 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  9638 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  9639 | `		return PH7_OK;` |
|      - |  9640 | `	}` |
|     57 |  9641 | `	for( iHook = 0 ; iHook < 2 ; iHook++ ){` |
|     39 |  9642 | `		ph7_class_instance *pMeth = 0;` |
|      - |  9643 | `		ph7_value sVal, *pKey;` |
|     39 |  9644 | `		sxi32 rc = ReflectHookMethod(pCtx, &sRef, iHook, &pMeth);` |
|     39 |  9645 | `		if( rc != PH7_OK ){` |
|    ! 0 |  9646 | `			return rc;` |
|      - |  9647 | `		}` |
|     39 |  9648 | `		if( pMeth == 0 ){` |
|     15 |  9649 | `			continue;` |
|      - |  9650 | `		}` |
|     27 |  9651 | `		pKey = ph7_context_new_scalar(pCtx);` |
|     27 |  9652 | `		if( pKey == 0 ){` |
|    ! 0 |  9653 | `			PH7_ClassInstanceUnref(pMeth);` |
|    ! 0 |  9654 | `			break;` |
|      - |  9655 | `		}` |
|     27 |  9656 | `		ph7_value_string(pKey, iHook ? "set" : "get", 3);` |
|     27 |  9657 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     27 |  9658 | `		sVal.x.pOther = pMeth;` |
|     27 |  9659 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|     27 |  9660 | `		ph7_array_add_elem(pOut, pKey, &sVal);   /* takes its own reference */` |
|     27 |  9661 | `		PH7_ClassInstanceUnref(pMeth);` |
|     15 |  9662 | `	}` |
|     21 |  9663 | `	ph7_result_value(pCtx, pOut);` |
|     21 |  9664 | `	return PH7_OK;` |
|     12 |  9665 | `}` |
|      - |  9666 | ``/* `get`/`set` out of a PropertyHookType case (or the bare string php also takes). */`` |
|     24 |  9667 | `static int ReflectHookKind(ph7_value *pArg)` |
|      2 |  9668 | `{` |
|      - |  9669 | `	const char *z;` |
|      - |  9670 | `	int n;` |
|     26 |  9671 | `	if( pArg == 0 ){` |
|    ! 0 |  9672 | `		return -1;` |
|      - |  9673 | `	}` |
|     26 |  9674 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|     26 |  9675 | `		ph7_class_instance *pCase = (ph7_class_instance *)pArg->x.pOther;` |
|     26 |  9676 | `		ph7_value *pVal = PH7_NativeAttr(pCase, "value");` |
|     26 |  9677 | `		if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  9678 | `			return -1;` |
|      - |  9679 | `		}` |
|     26 |  9680 | `		z = (const char *)SyBlobData(&pVal->sBlob);` |
|     26 |  9681 | `		n = (int)SyBlobLength(&pVal->sBlob);` |
|     14 |  9682 | `	}else{` |
|    ! 0 |  9683 | `		z = ph7_value_to_string(pArg, &n);` |
|      - |  9684 | `	}` |
|     26 |  9685 | `	if( n == 3 && SyMemcmp(z, "get", 3) == 0 ){` |
|     18 |  9686 | `		return 0;` |
|      - |  9687 | `	}` |
|      9 |  9688 | `	if( n == 3 && SyMemcmp(z, "set", 3) == 0 ){` |
|      9 |  9689 | `		return 1;` |
|      - |  9690 | `	}` |
|    ! 0 |  9691 | `	return -1;` |
|     14 |  9692 | `}` |
|      6 |  9693 | `static int vm_builtin_ReflectionProperty_hasHook(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9694 | `{` |
|      - |  9695 | `	ReflectMemberRef sRef;` |
|      7 |  9696 | `	int iHook = ReflectHookKind(nArg > 0 ? apArg[0] : 0);` |
|      7 |  9697 | `	int bYes = 0;` |
|      7 |  9698 | `	if( iHook >= 0 && ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|      7 |  9699 | `		bYes = (sRef.pAttr->iFlags & (iHook ? PH7_CLASS_ATTR_HOOK_SET : PH7_CLASS_ATTR_HOOK_GET)) != 0;` |
|      3 |  9700 | `	}` |
|      7 |  9701 | `	ph7_result_bool(pCtx, bYes);` |
|      7 |  9702 | `	return PH7_OK;` |
|      1 |  9703 | `}` |
|     18 |  9704 | `static int vm_builtin_ReflectionProperty_getHook(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  9705 | `{` |
|      - |  9706 | `	ReflectMemberRef sRef;` |
|     20 |  9707 | `	ph7_class_instance *pMeth = 0;` |
|     20 |  9708 | `	int iHook = ReflectHookKind(nArg > 0 ? apArg[0] : 0);` |
|      - |  9709 | `	sxi32 rc;` |
|     20 |  9710 | `	if( iHook < 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  9711 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9712 | `		return PH7_OK;` |
|      - |  9713 | `	}` |
|     20 |  9714 | `	rc = ReflectHookMethod(pCtx, &sRef, iHook, &pMeth);` |
|     20 |  9715 | `	if( rc != PH7_OK ){` |
|    ! 0 |  9716 | `		return rc;` |
|      - |  9717 | `	}` |
|     20 |  9718 | `	if( pMeth == 0 ){` |
|      3 |  9719 | `		ph7_result_null(pCtx);` |
|      3 |  9720 | `		return PH7_OK;` |
|      - |  9721 | `	}` |
|     18 |  9722 | `	return ReflectResultObject(pCtx, pMeth);` |
|     11 |  9723 | `}` |
|      6 |  9724 | `static int vm_builtin_ReflectionProperty_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9725 | `{` |
|      7 |  9726 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  9727 | `	ReflectMemberRef sRef;` |
|      - |  9728 | `	ph7_value sTarget;` |
|      - |  9729 | `	const char *zClass;` |
|      - |  9730 | `	int nClass, rc;` |
|      7 |  9731 | `	if( pThis == 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9732 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  9733 | `		return PH7_OK;` |
|      - |  9734 | `	}` |
|      7 |  9735 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|      7 |  9736 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      7 |  9737 | `	ph7_value_string(&sTarget, zClass, nClass);` |
|      - |  9738 | `	/* 8 = Attribute::TARGET_PROPERTY */` |
|     10 |  9739 | `	rc = ReflectBuildAttrs(pCtx, &sRef.pAttr->aAttrs, "attr", &sTarget,` |
|      3 |  9740 | `		sRef.zName, sRef.nName, 0, 8, nArg, apArg);` |
|      7 |  9741 | `	PH7_MemObjRelease(&sTarget);` |
|      7 |  9742 | `	return rc;` |
|      4 |  9743 | `}` |
|     32 |  9744 | `static int vm_builtin_ReflectionProperty_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  9745 | `{` |
|     16 |  9746 | `	SXUNUSED(nArg);` |
|     16 |  9747 | `	SXUNUSED(apArg);` |
|     35 |  9748 | `	return ReflectExportPropSelf(pCtx);` |
|      3 |  9749 | `}` |
|      - |  9750 | `/* ---- ReflectionClassConstant ---- */` |
|      - |  9751 | `/* ReflectionClassConstant::__construct(object\|string $class, string $constant) */` |
|     92 |  9752 | `static int vm_builtin_ReflectionClassConstant_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  9753 | `{` |
|      - |  9754 | `	ph7_class_instance *pPrev;` |
|      - |  9755 | `	sxi32 rcLook;` |
|     95 |  9756 | `	ph7_vm *pVm = pCtx->pVm;` |
|     95 |  9757 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     95 |  9758 | `	ph7_class *pClass, *pDecl = 0;` |
|      - |  9759 | `	const char *zConst;` |
|      - |  9760 | `	int nConst;` |
|      - |  9761 | `	SySet aMembers;` |
|      - |  9762 | `	sxu32 n;` |
|     95 |  9763 | `	int bFound = 0;` |
|     95 |  9764 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 |  9765 | `		return PH7_OK;` |
|      - |  9766 | `	}` |
|     95 |  9767 | `	pClass = ReflectResolveClassFenced(pCtx, apArg[0], 0, &pPrev, &rcLook);` |
|     95 |  9768 | `	if( pClass == 0 ){` |
|      - |  9769 | `		const char *zName;` |
|      - |  9770 | `		int nName;` |
|     13 |  9771 | `		if( rcLook != PH7_OK ){` |
|    ! 0 |  9772 | `			return rcLook;` |
|      - |  9773 | `		}` |
|     13 |  9774 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|     19 |  9775 | `		rcLook = PH7_VmThrowExceptionPrev(pCtx, pPrev, "ReflectionException",` |
|      6 |  9776 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|     13 |  9777 | `		if( pPrev ){` |
|      7 |  9778 | `			PH7_ClassInstanceUnref(pPrev);` |
|      3 |  9779 | `		}` |
|     13 |  9780 | `		return rcLook;` |
|      - |  9781 | `	}` |
|     82 |  9782 | `	zConst = ph7_value_to_string(apArg[1], &nConst);` |
|     82 |  9783 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|     82 |  9784 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|    372 |  9785 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    366 |  9786 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|    366 |  9787 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zConst, nConst) ){` |
|     76 |  9788 | `			pDecl = pM->pDecl ? pM->pDecl : pClass;` |
|     76 |  9789 | `			bFound = 1;` |
|     76 |  9790 | `			break;` |
|      - |  9791 | `		}` |
|    146 |  9792 | `	}` |
|     82 |  9793 | `	SySetRelease(&aMembers);` |
|     82 |  9794 | `	if( !bFound ){` |
|     10 |  9795 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 |  9796 | `			"Constant %z::%.*s does not exist", &pClass->sDisp, nConst, zConst);` |
|      - |  9797 | `	}` |
|      - |  9798 | `	/* php's $class is the DECLARING class, not the one asked about. */` |
|     76 |  9799 | `	pClass = pDecl;` |
|    113 |  9800 | `	PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pClass->sName),` |
|     74 |  9801 | `		(int)SyStringLength(&pClass->sName));` |
|     76 |  9802 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", zConst, nConst);` |
|     76 |  9803 | `	return PH7_OK;` |
|     49 |  9804 | `}` |
|     22 |  9805 | `static int vm_builtin_ReflectionClassConstant_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9806 | `{` |
|      - |  9807 | `	ReflectMemberRef sRef;` |
|      - |  9808 | `	ph7_value *pVal;` |
|      - |  9809 | `	sxi32 rc;` |
|     11 |  9810 | `	SXUNUSED(nArg);` |
|     11 |  9811 | `	SXUNUSED(apArg);` |
|     23 |  9812 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9813 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9814 | `		return PH7_OK;` |
|      - |  9815 | `	}` |
|     23 |  9816 | `	rc = ReflectConstSlot(pCtx, sRef.pClass, sRef.pAttr, &pVal);` |
|     23 |  9817 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  9818 | `		return rc;` |
|      - |  9819 | `	}` |
|     23 |  9820 | `	if( pVal ){` |
|     23 |  9821 | `		ph7_result_value(pCtx, pVal);` |
|     12 |  9822 | `	}else{` |
|    ! 0 |  9823 | `		ph7_result_null(pCtx);` |
|      - |  9824 | `	}` |
|     23 |  9825 | `	return PH7_OK;` |
|     12 |  9826 | `}` |
|     28 |  9827 | `static int ReflectConstFlag(ph7_context *pCtx, int iWhat)` |
|      1 |  9828 | `{` |
|      - |  9829 | `	ReflectMemberRef sRef;` |
|     29 |  9830 | `	int bYes = 0;` |
|     29 |  9831 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) && sRef.pAttr ){` |
|     29 |  9832 | `		ph7_class_attr *pAttr = sRef.pAttr;` |
|     29 |  9833 | `		switch( iWhat ){` |
|      3 |  9834 | `		case 0: bYes = pAttr->iProtection == PH7_CLASS_PROT_PUBLIC; break;` |
|      5 |  9835 | `		case 1: bYes = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE; break;` |
|      5 |  9836 | `		case 2: bYes = pAttr->iProtection == PH7_CLASS_PROT_PROTECTED; break;` |
|      3 |  9837 | `		case 3: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0; break;` |
|     13 |  9838 | `		case 4: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0; break;` |
|      3 |  9839 | `		case 5: bYes = ReflectHasDeprecated(&pAttr->aAttrs); break;` |
|      3 |  9840 | `		default: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0; break;` |
|      - |  9841 | `		}` |
|     14 |  9842 | `	}` |
|     29 |  9843 | `	ph7_result_bool(pCtx, bYes);` |
|     29 |  9844 | `	return PH7_OK;` |
|      1 |  9845 | `}` |
|      - |  9846 | `#define REFLECT_CONST_FLAG(NAME,WHAT) \` |
|      - |  9847 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  9848 | `	{ \` |
|      - |  9849 | `		SXUNUSED(nArg); \` |
|      - |  9850 | `		SXUNUSED(apArg); \` |
|      - |  9851 | `		return ReflectConstFlag(pCtx, WHAT); \` |
|      - |  9852 | `	}` |
|      3 |  9853 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isPublic, 0)` |
|      5 |  9854 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isPrivate, 1)` |
|      5 |  9855 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isProtected, 2)` |
|      3 |  9856 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isFinal, 3)` |
|     13 |  9857 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isEnumCase, 4)` |
|      3 |  9858 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isDeprecated, 5)` |
|      3 |  9859 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_hasType, 6)` |
|      - |  9860 |  |
|      8 |  9861 | `static int vm_builtin_ReflectionClassConstant_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9862 | `{` |
|      - |  9863 | `	ReflectMemberRef sRef;` |
|      4 |  9864 | `	SXUNUSED(nArg);` |
|      4 |  9865 | `	SXUNUSED(apArg);` |
|      9 |  9866 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9867 | `		ph7_result_int(pCtx, 0);` |
|    ! 0 |  9868 | `		return PH7_OK;` |
|      - |  9869 | `	}` |
|      9 |  9870 | `	ph7_result_int64(pCtx, ReflectConstModifiers(sRef.pAttr));` |
|      9 |  9871 | `	return PH7_OK;` |
|      5 |  9872 | `}` |
|      6 |  9873 | `static int vm_builtin_ReflectionClassConstant_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9874 | `{` |
|      - |  9875 | `	ReflectMemberRef sRef;` |
|      3 |  9876 | `	SXUNUSED(nArg);` |
|      3 |  9877 | `	SXUNUSED(apArg);` |
|      7 |  9878 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) ){` |
|    ! 0 |  9879 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9880 | `		return PH7_OK;` |
|      - |  9881 | `	}` |
|      7 |  9882 | `	return ReflectResultClassOf(pCtx, ReflectMemberDecl(&sRef));` |
|      4 |  9883 | `}` |
|      2 |  9884 | `static int vm_builtin_ReflectionClassConstant_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9885 | `{` |
|      - |  9886 | `	ReflectMemberRef sRef;` |
|      1 |  9887 | `	SXUNUSED(nArg);` |
|      1 |  9888 | `	SXUNUSED(apArg);` |
|      2 |  9889 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) && sRef.pAttr` |
|      3 |  9890 | `	 && SyStringLength(&sRef.pAttr->sDoc) > 0 ){` |
|      4 |  9891 | `		ph7_result_string(pCtx, SyStringData(&sRef.pAttr->sDoc),` |
|      2 |  9892 | `			(int)SyStringLength(&sRef.pAttr->sDoc));` |
|      2 |  9893 | `	}else{` |
|    ! 0 |  9894 | `		ph7_result_bool(pCtx, 0);` |
|      - |  9895 | `	}` |
|      3 |  9896 | `	return PH7_OK;` |
|      1 |  9897 | `}` |
|      4 |  9898 | `static int vm_builtin_ReflectionClassConstant_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  9899 | `{` |
|      - |  9900 | `	ReflectMemberRef sRef;` |
|      2 |  9901 | `	SXUNUSED(nArg);` |
|      2 |  9902 | `	SXUNUSED(apArg);` |
|      4 |  9903 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0` |
|      4 |  9904 | `	 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|      6 |  9905 | `	 \|\| SyStringLength(&sRef.pAttr->sTypeName) < 1 ){` |
|    ! 0 |  9906 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9907 | `		return PH7_OK;` |
|      - |  9908 | `	}` |
|      - |  9909 | `	{` |
|      - |  9910 | `		char zType[192];` |
|      6 |  9911 | `		const char *zT = ReflectMemberTypeText(pCtx->pVm,sRef.pAttr,zType,sizeof(zType));` |
|      6 |  9912 | `		return ReflectResultObject(pCtx, ReflectMakeType(pCtx, zT, (int)SyStrlen(zT)));` |
|      - |  9913 | `	}` |
|      4 |  9914 | `}` |
|      2 |  9915 | `static int vm_builtin_ReflectionClassConstant_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9916 | `{` |
|      3 |  9917 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  9918 | `	ReflectMemberRef sRef;` |
|      - |  9919 | `	ph7_value sTarget;` |
|      - |  9920 | `	const char *zClass;` |
|      - |  9921 | `	int nClass, rc;` |
|      3 |  9922 | `	if( pThis == 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9923 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  9924 | `		return PH7_OK;` |
|      - |  9925 | `	}` |
|      3 |  9926 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|      3 |  9927 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      3 |  9928 | `	ph7_value_string(&sTarget, zClass, nClass);` |
|      - |  9929 | `	/* 16 = Attribute::TARGET_CLASS_CONSTANT */` |
|      4 |  9930 | `	rc = ReflectBuildAttrs(pCtx, &sRef.pAttr->aAttrs, "attr", &sTarget,` |
|      1 |  9931 | `		sRef.zName, sRef.nName, 0, 16, nArg, apArg);` |
|      3 |  9932 | `	PH7_MemObjRelease(&sTarget);` |
|      3 |  9933 | `	return rc;` |
|      2 |  9934 | `}` |
|      6 |  9935 | `static int vm_builtin_ReflectionClassConstant_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9936 | `{` |
|      3 |  9937 | `	SXUNUSED(nArg);` |
|      3 |  9938 | `	SXUNUSED(apArg);` |
|      7 |  9939 | `	return ReflectExportConstSelf(pCtx);` |
|      1 |  9940 | `}` |
|      - |  9941 | `/*` |
|      - |  9942 | ` * Declare chunk 3. Called from PH7_VmInstallReflection() where it used to be` |
|      - |  9943 | ` * compiled — after chunks 1 and 2, whose ReflectionClass and ReflectionMethod` |
|      - |  9944 | ` * these answer, and after PropertyHookType, which hasHook()/getHook() declare.` |
|      - |  9945 | ` *` |
|      - |  9946 | ` * The method tables are in php's own DECLARATION order.` |
|      - |  9947 | ` */` |
|   8445 |  9948 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionMember(ph7_vm *pVm)` |
|      5 |  9949 | `{` |
|      - |  9950 | `	static const PH7_NativePropDef aMemberProp[] = {` |
|      - |  9951 | `		{ "name",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  9952 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  9953 | `	};` |
|      - |  9954 | `	static const PH7_NativePropDef aPropProp[] = {` |
|      - |  9955 | `		{ "name",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  9956 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  9957 | `		/* PHL-only: the instance a DYNAMIC property was reached through (recorded) */` |
|      - |  9958 | `		{ RP_DYNOBJ, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  9959 | `	};` |
|      - |  9960 | `	static const PH7_NativeConstDef aPropConst[] = {` |
|      - |  9961 | `		{ "IS_STATIC",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,   0, 0.0 },` |
|      - |  9962 | `		{ "IS_READONLY",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 128,  0, 0.0 },` |
|      - |  9963 | `		{ "IS_PUBLIC",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,    0, 0.0 },` |
|      - |  9964 | `		{ "IS_PROTECTED",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,    0, 0.0 },` |
|      - |  9965 | `		{ "IS_PRIVATE",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,    0, 0.0 },` |
|      - |  9966 | `		{ "IS_ABSTRACT",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64,   0, 0.0 },` |
|      - |  9967 | `		{ "IS_PROTECTED_SET", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2048, 0, 0.0 },` |
|      - |  9968 | `		{ "IS_PRIVATE_SET",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4096, 0, 0.0 },` |
|      - |  9969 | `		{ "IS_VIRTUAL",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 512,  0, 0.0 },` |
|      - |  9970 | `		{ "IS_FINAL",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32,   0, 0.0 },` |
|      - |  9971 | `	};` |
|      - |  9972 | `	static const PH7_NativeMethodDef aPropMethod[] = {` |
|      - |  9973 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - |  9974 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $property", "",` |
|      - |  9975 | `		  vm_builtin_ReflectionProperty_construct },` |
|      - |  9976 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionProperty_toString },` |
|      - |  9977 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionProperty_getName },` |
|      - |  9978 | `		{ "getMangledName", PH7_MOD_PUBLIC, "", "string",` |
|      - |  9979 | `		  vm_builtin_ReflectionProperty_getMangledName },` |
|      - |  9980 | `		{ "getValue",    PH7_MOD_PUBLIC, "?object $object = null", "@mixed",` |
|      - |  9981 | `		  vm_builtin_ReflectionProperty_getValue },` |
|      - |  9982 | `		{ "setValue",    PH7_MOD_PUBLIC, "mixed $objectOrValue, mixed $value = ?", "@void",` |
|      - |  9983 | `		  vm_builtin_ReflectionProperty_setValue },` |
|      - |  9984 | `		{ "getRawValue", PH7_MOD_PUBLIC, "object $object", "mixed",` |
|      - |  9985 | `		  vm_builtin_ReflectionProperty_getValue },` |
|      - |  9986 | `		{ "setRawValue", PH7_MOD_PUBLIC, "object $object, mixed $value", "void",` |
|      - |  9987 | `		  vm_builtin_ReflectionProperty_setValue },` |
|      - |  9988 | `		/* On a non-lazy object — the only kind PHL has — php's two lazy writers` |
|      - |  9989 | `		 * are an ordinary raw write and a no-op. */` |
|      - |  9990 | `		{ "setRawValueWithoutLazyInitialization", PH7_MOD_PUBLIC,` |
|      - |  9991 | `		  "object $object, mixed $value", "void", vm_builtin_ReflectionProperty_setValue },` |
|      - |  9992 | `		{ "skipLazyInitialization", PH7_MOD_PUBLIC, "object $object", "void",` |
|      - |  9993 | `		  vm_builtin_ReflectionProperty_noop },` |
|      - |  9994 | `		{ "isLazy",      PH7_MOD_PUBLIC, "object $object", "bool",` |
|      - |  9995 | `		  vm_builtin_ReflectionProperty_false },` |
|      - |  9996 | `		{ "isInitialized", PH7_MOD_PUBLIC, "?object $object = null", "@bool",` |
|      - |  9997 | `		  vm_builtin_ReflectionProperty_isInitialized },` |
|      - |  9998 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isPublic },` |
|      - |  9999 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isPrivate },` |
|      - | 10000 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isProtected },` |
|      - | 10001 | `		{ "isPrivateSet", PH7_MOD_PUBLIC, "", "bool",` |
|      - | 10002 | `		  vm_builtin_ReflectionProperty_isPrivateSet },` |
|      - | 10003 | `		{ "isProtectedSet", PH7_MOD_PUBLIC, "", "bool",` |
|      - | 10004 | `		  vm_builtin_ReflectionProperty_isProtectedSet },` |
|      - | 10005 | `		{ "isStatic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isStatic },` |
|      - | 10006 | `		{ "isReadOnly",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isReadOnly },` |
|      - | 10007 | `		{ "isDefault",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isDefault },` |
|      - | 10008 | `		{ "isDynamic",   PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isDynamic },` |
|      - | 10009 | `		/* PHL has no abstract properties: the modifier only exists on an` |
|      - | 10010 | `		 * interface's hooked property stub, which PHL does not model. */` |
|      - | 10011 | `		{ "isAbstract",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_false },` |
|      - | 10012 | `		{ "isVirtual",   PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isVirtual },` |
|      - | 10013 | `		{ "isPromoted",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isPromoted },` |
|      - | 10014 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionProperty_getModifiers },` |
|      - | 10015 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|      - | 10016 | `		  vm_builtin_ReflectionProperty_getDeclaringClass },` |
|      - | 10017 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionProperty_getDocComment },` |
|      - | 10018 | `		{ "setAccessible", PH7_MOD_PUBLIC, "bool $accessible", "@void",` |
|      - | 10019 | `		  vm_builtin_ReflectionProperty_setAccessible },` |
|      - | 10020 | `		{ "getType",     PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionProperty_getType },` |
|      - | 10021 | `		/* php's settable type differs from the declared one only for a hooked` |
|      - | 10022 | ``		 * property with a widening `set` — which PHL does not model. */`` |
|      - | 10023 | `		{ "getSettableType", PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|      - | 10024 | `		  vm_builtin_ReflectionProperty_getType },` |
|      - | 10025 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_hasType },` |
|      - | 10026 | `		{ "hasDefaultValue", PH7_MOD_PUBLIC, "", "bool",` |
|      - | 10027 | `		  vm_builtin_ReflectionProperty_hasDefaultValue },` |
|      - | 10028 | `		{ "getDefaultValue", PH7_MOD_PUBLIC, "", "@mixed",` |
|      - | 10029 | `		  vm_builtin_ReflectionProperty_getDefaultValue },` |
|      - | 10030 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - | 10031 | `		  vm_builtin_ReflectionProperty_getAttributes },` |
|      - | 10032 | `		{ "hasHooks",    PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_hasHooks },` |
|      - | 10033 | `		{ "getHooks",    PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionProperty_getHooks },` |
|      - | 10034 | `		{ "hasHook",     PH7_MOD_PUBLIC, "PropertyHookType $type", "bool",` |
|      - | 10035 | `		  vm_builtin_ReflectionProperty_hasHook },` |
|      - | 10036 | `		{ "getHook",     PH7_MOD_PUBLIC, "PropertyHookType $type", "?ReflectionMethod",` |
|      - | 10037 | `		  vm_builtin_ReflectionProperty_getHook },` |
|      - | 10038 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isFinal },` |
|      - | 10039 | `	};` |
|      - | 10040 | `	static const PH7_NativeConstDef aConstConst[] = {` |
|      - | 10041 | `		{ "IS_PUBLIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,  0, 0.0 },` |
|      - | 10042 | `		{ "IS_PROTECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,  0, 0.0 },` |
|      - | 10043 | `		{ "IS_PRIVATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,  0, 0.0 },` |
|      - | 10044 | `		{ "IS_FINAL",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|      - | 10045 | `	};` |
|      - | 10046 | `	static const PH7_NativeMethodDef aConstMethod[] = {` |
|      - | 10047 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - | 10048 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|      - | 10049 | `		  vm_builtin_ReflectionClassConstant_construct },` |
|      - | 10050 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string",` |
|      - | 10051 | `		  vm_builtin_ReflectionClassConstant_toString },` |
|      - | 10052 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionProperty_getName },` |
|      - | 10053 | `		{ "getValue",    PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_ReflectionClassConstant_getValue },` |
|      - | 10054 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClassConstant_isPublic },` |
|      - | 10055 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClassConstant_isPrivate },` |
|      - | 10056 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool",` |
|      - | 10057 | `		  vm_builtin_ReflectionClassConstant_isProtected },` |
|      - | 10058 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClassConstant_isFinal },` |
|      - | 10059 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int",` |
|      - | 10060 | `		  vm_builtin_ReflectionClassConstant_getModifiers },` |
|      - | 10061 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|      - | 10062 | `		  vm_builtin_ReflectionClassConstant_getDeclaringClass },` |
|      - | 10063 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false",` |
|      - | 10064 | `		  vm_builtin_ReflectionClassConstant_getDocComment },` |
|      - | 10065 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - | 10066 | `		  vm_builtin_ReflectionClassConstant_getAttributes },` |
|      - | 10067 | `		{ "isEnumCase",  PH7_MOD_PUBLIC, "", "bool",` |
|      - | 10068 | `		  vm_builtin_ReflectionClassConstant_isEnumCase },` |
|      - | 10069 | `		{ "isDeprecated", PH7_MOD_PUBLIC, "", "bool",` |
|      - | 10070 | `		  vm_builtin_ReflectionClassConstant_isDeprecated },` |
|      - | 10071 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClassConstant_hasType },` |
|      - | 10072 | `		{ "getType",     PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|      - | 10073 | `		  vm_builtin_ReflectionClassConstant_getType },` |
|      - | 10074 | `	};` |
|      - | 10075 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 10076 | `		{ "ReflectionProperty", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 10077 | `		  aPropMethod, SX_ARRAYSIZE(aPropMethod),` |
|      - | 10078 | `		  aPropConst, SX_ARRAYSIZE(aPropConst),` |
|      - | 10079 | `		  aPropProp, SX_ARRAYSIZE(aPropProp), 0, 0, 0 },` |
|      - | 10080 | `		{ "ReflectionClassConstant", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 10081 | `		  aConstMethod, SX_ARRAYSIZE(aConstMethod),` |
|      - | 10082 | `		  aConstConst, SX_ARRAYSIZE(aConstConst),` |
|      - | 10083 | `		  aMemberProp, SX_ARRAYSIZE(aMemberProp), 0, 0, 0 },` |
|      - | 10084 | `	};` |
|   8450 | 10085 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 | 10086 | `}` |
|      - | 10087 | `/*` |
|      - | 10088 | ` * ---------------------------------------------------------------------------` |
|      - | 10089 | ` * The ReflectionEnum family — chunk 6, and the end of __phl_rcinfo.` |
|      - | 10090 | ` *` |
|      - | 10091 | `` * Three classes that are very nearly their parents: `ReflectionEnum` IS a`` |
|      - | 10092 | `` * `ReflectionClass` that refuses a non-enum, and `ReflectionEnumUnitCase` IS a`` |
|      - | 10093 | `` * `ReflectionClassConstant` that refuses a constant which is not a case. What`` |
|      - | 10094 | ` * is their own is the CASE list, which the engine already holds in declaration` |
|      - | 10095 | `` * order on `ph7_class::aEnumCases` — the chunk read a marshalled copy of it out`` |
|      - | 10096 | `` * of `__phl_rcinfo`, the descriptor builder that dies with this conversion.`` |
|      - | 10097 | ` * ---------------------------------------------------------------------------` |
|      - | 10098 | ` */` |
|      - | 10099 |  |
|      - | 10100 | `/* The enum case named zName, or NULL. A case is a class CONSTANT, so the match` |
|      - | 10101 | `` * is case-SENSITIVE: php's hasCase('hearts') is false for `case Hearts`. */`` |
|     28 | 10102 | `static ph7_class_attr * ReflectEnumCase(ph7_class *pClass, const char *zName, int nName)` |
|      1 | 10103 | `{` |
|     29 | 10104 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      - | 10105 | `	sxu32 n;` |
|     29 | 10106 | `	if( nName < 1 ){` |
|    ! 0 | 10107 | `		return 0;` |
|      - | 10108 | `	}` |
|     67 | 10109 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     52 | 10110 | `		if( (int)SyStringLength(&apCase[n]->sName) == nName` |
|     38 | 10111 | `		 && SyMemcmp(SyStringData(&apCase[n]->sName), zName, (sxu32)nName) == 0 ){` |
|     15 | 10112 | `			return apCase[n];` |
|      - | 10113 | `		}` |
|     20 | 10114 | `	}` |
|     15 | 10115 | `	return 0;` |
|     15 | 10116 | `}` |
|      - | 10117 | `/*` |
|      - | 10118 | ` * One case reflector for an already-validated case. php answers a BackedCase` |
|      - | 10119 | ` * for a backed enum and a UnitCase for a pure one, and both carry the same two` |
|      - | 10120 | ` * slots ReflectionClassConstant does — an enum cannot extend anything, so the` |
|      - | 10121 | ` * declaring class is always the enum itself.` |
|      - | 10122 | ` */` |
|     54 | 10123 | `static ph7_class_instance * ReflectEnumCaseNew(ph7_context *pCtx, ph7_class *pEnum,` |
|      - | 10124 | `	const SyString *pCase)` |
|      2 | 10125 | `{` |
|     56 | 10126 | `	ph7_vm *pVm = pCtx->pVm;` |
|     83 | 10127 | `	const char *zClass = pEnum->nEnumBacking` |
|     27 | 10128 | `		? "ReflectionEnumBackedCase" : "ReflectionEnumUnitCase";` |
|     56 | 10129 | `	ph7_class *pRC = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|     56 | 10130 | `	ph7_class_instance *pObj = pRC ? PH7_NewClassInstance(pVm, pRC) : 0;` |
|     56 | 10131 | `	if( pObj == 0 ){` |
|    ! 0 | 10132 | `		return 0;` |
|      - | 10133 | `	}` |
|     83 | 10134 | `	PH7_NativeSetAttrStr(pVm, pObj, "class", SyStringData(&pEnum->sName),` |
|     54 | 10135 | `		(int)SyStringLength(&pEnum->sName));` |
|     56 | 10136 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(pCase), (int)SyStringLength(pCase));` |
|     56 | 10137 | `	return pObj;` |
|     29 | 10138 | `}` |
|      - | 10139 | `/* ReflectionEnum::__construct(object\|string $objectOrClass) */` |
|     36 | 10140 | `static int vm_builtin_ReflectionEnum_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 10141 | `{` |
|      - | 10142 | `	ph7_class *pClass;` |
|     39 | 10143 | `	sxi32 rc = vm_builtin_ReflectionClass_construct(pCtx, nArg, apArg);` |
|     39 | 10144 | `	if( rc != PH7_OK ){` |
|     10 | 10145 | `		return rc; /* "Class %s does not exist", already php's */` |
|      - | 10146 | `	}` |
|     30 | 10147 | `	pClass = ReflectClassOf(pCtx);` |
|     30 | 10148 | `	if( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     10 | 10149 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 | 10150 | `			"Class \"%z\" is not an enum", &pClass->sDisp);` |
|      - | 10151 | `	}` |
|     24 | 10152 | `	return PH7_OK;` |
|     21 | 10153 | `}` |
|     12 | 10154 | `static int vm_builtin_ReflectionEnum_hasCase(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 10155 | `{` |
|     13 | 10156 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 10157 | `	const char *zName;` |
|      - | 10158 | `	int nName;` |
|     13 | 10159 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 10160 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 10161 | `		return PH7_OK;` |
|      - | 10162 | `	}` |
|     13 | 10163 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     13 | 10164 | `	ph7_result_bool(pCtx, ReflectEnumCase(pClass, zName, nName) != 0);` |
|     13 | 10165 | `	return PH7_OK;` |
|      7 | 10166 | `}` |
|      - | 10167 | `/*` |
|      - | 10168 | ` * getCase(): php tells the two failures apart — a name that is a constant but` |
|      - | 10169 | ` * not a case is "X::K is not a case", one that is neither is "Case X::K does` |
|      - | 10170 | ` * not exist". The chunk answered the second for both.` |
|      - | 10171 | ` */` |
|     16 | 10172 | `static int vm_builtin_ReflectionEnum_getCase(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 10173 | `{` |
|     17 | 10174 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 10175 | `	ph7_class_attr *pCase;` |
|      - | 10176 | `	const char *zName;` |
|      - | 10177 | `	int nName;` |
|     17 | 10178 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 10179 | `		ph7_result_null(pCtx);` |
|    ! 0 | 10180 | `		return PH7_OK;` |
|      - | 10181 | `	}` |
|     17 | 10182 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     17 | 10183 | `	pCase = ReflectEnumCase(pClass, zName, nName);` |
|     17 | 10184 | `	if( pCase == 0 ){` |
|      7 | 10185 | `		if( nName > 0 && SyHashGet(&pClass->hConst, (const void *)zName, (sxu32)nName) ){` |
|      4 | 10186 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 10187 | `				"%z::%.*s is not a case", &pClass->sDisp, nName, zName);` |
|      - | 10188 | `		}` |
|      7 | 10189 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 | 10190 | `			"Case %z::%.*s does not exist", &pClass->sDisp, nName, zName);` |
|      - | 10191 | `	}` |
|     11 | 10192 | `	return ReflectResultObject(pCtx, ReflectEnumCaseNew(pCtx, pClass, &pCase->sName));` |
|      9 | 10193 | `}` |
|     16 | 10194 | `static int vm_builtin_ReflectionEnum_getCases(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 10195 | `{` |
|     18 | 10196 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     18 | 10197 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      8 | 10198 | `	SXUNUSED(nArg);` |
|      8 | 10199 | `	SXUNUSED(apArg);` |
|     18 | 10200 | `	if( pOut == 0 ){` |
|    ! 0 | 10201 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 10202 | `	}` |
|     18 | 10203 | `	if( pClass ){` |
|     18 | 10204 | `		ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      - | 10205 | `		sxu32 n;` |
|     62 | 10206 | `		for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     46 | 10207 | `			ReflectTypeListAdd(pCtx, pOut, ReflectEnumCaseNew(pCtx, pClass, &apCase[n]->sName));` |
|     24 | 10208 | `		}` |
|      8 | 10209 | `	}` |
|     18 | 10210 | `	ph7_result_value(pCtx, pOut);` |
|     18 | 10211 | `	return PH7_OK;` |
|     10 | 10212 | `}` |
|     12 | 10213 | `static int vm_builtin_ReflectionEnum_isBacked(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 10214 | `{` |
|     13 | 10215 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      6 | 10216 | `	SXUNUSED(nArg);` |
|      6 | 10217 | `	SXUNUSED(apArg);` |
|     13 | 10218 | `	ph7_result_bool(pCtx, pClass != 0 && pClass->nEnumBacking != 0);` |
|     13 | 10219 | `	return PH7_OK;` |
|      1 | 10220 | `}` |
|     18 | 10221 | `static int vm_builtin_ReflectionEnum_getBackingType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 10222 | `{` |
|     20 | 10223 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 10224 | `	const char *zText;` |
|      - | 10225 | `	ph7_class_instance *pType;` |
|      9 | 10226 | `	SXUNUSED(nArg);` |
|      9 | 10227 | `	SXUNUSED(apArg);` |
|     20 | 10228 | `	if( pClass == 0 \|\| pClass->nEnumBacking == 0 ){` |
|      5 | 10229 | `		ph7_result_null(pCtx); /* a PURE enum has no backing type */` |
|      5 | 10230 | `		return PH7_OK;` |
|      - | 10231 | `	}` |
|     16 | 10232 | `	zText = (pClass->nEnumBacking & MEMOBJ_INT) ? "int" : "string";` |
|     16 | 10233 | `	pType = ReflectMakeType(pCtx, zText, (int)SyStrlen(zText));` |
|     16 | 10234 | `	if( pType == 0 ){` |
|    ! 0 | 10235 | `		ph7_result_null(pCtx);` |
|    ! 0 | 10236 | `		return PH7_OK;` |
|      - | 10237 | `	}` |
|     16 | 10238 | `	PH7_NativeResultObject(pCtx, pType);` |
|     16 | 10239 | `	return PH7_OK;` |
|     11 | 10240 | `}` |
|      - | 10241 | `/*` |
|      - | 10242 | ` * ReflectionEnumUnitCase::__construct(object\|string $class, string $constant).` |
|      - | 10243 | ` *` |
|      - | 10244 | ` * The parent raises for a constant that does not exist; what is left is "not a` |
|      - | 10245 | ` * case", and php words that one WITHOUT asking whether the class is an enum at` |
|      - | 10246 | `` * all — `new ReflectionEnumUnitCase('Plain', 'K')` on an ordinary class says`` |
|      - | 10247 | `` * `Constant Plain::K is not a case`, not "is not an enum" as the chunk did.`` |
|      - | 10248 | ` * A non-enum's constant can never carry the ENUMCASE bit, so the one screen` |
|      - | 10249 | ` * answers both shapes.` |
|      - | 10250 | ` */` |
|     26 | 10251 | `static int vm_builtin_ReflectionEnumCase_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 10252 | `{` |
|      - | 10253 | `	ReflectMemberRef sRef;` |
|     28 | 10254 | `	sxi32 rc = vm_builtin_ReflectionClassConstant_construct(pCtx, nArg, apArg);` |
|     28 | 10255 | `	if( rc != PH7_OK ){` |
|     12 | 10256 | `		return rc;` |
|      - | 10257 | `	}` |
|     17 | 10258 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 10259 | `		return PH7_OK;` |
|      - | 10260 | `	}` |
|     17 | 10261 | `	if( (sRef.pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){` |
|     10 | 10262 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      6 | 10263 | `			"Constant %z::%.*s is not a case", &sRef.pClass->sDisp, sRef.nName, sRef.zName);` |
|      - | 10264 | `	}` |
|     11 | 10265 | `	return PH7_OK;` |
|     15 | 10266 | `}` |
|      - | 10267 | `/* ReflectionEnumBackedCase::__construct() — the same, plus php's screen for a` |
|      - | 10268 | ` * PURE enum's case, which has no backing value to answer with. */` |
|     10 | 10269 | `static int vm_builtin_ReflectionEnumBackedCase_construct(ph7_context *pCtx, int nArg,` |
|      - | 10270 | `	ph7_value **apArg)` |
|      2 | 10271 | `{` |
|      - | 10272 | `	ReflectMemberRef sRef;` |
|     12 | 10273 | `	sxi32 rc = vm_builtin_ReflectionEnumCase_construct(pCtx, nArg, apArg);` |
|     12 | 10274 | `	if( rc != PH7_OK ){` |
|      5 | 10275 | `		return rc;` |
|      - | 10276 | `	}` |
|      7 | 10277 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 10278 | `		return PH7_OK;` |
|      - | 10279 | `	}` |
|      7 | 10280 | `	if( sRef.pClass->nEnumBacking == 0 ){` |
|      4 | 10281 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 | 10282 | `			"Enum case %z::%.*s is not a backed case", &sRef.pClass->sDisp,` |
|      1 | 10283 | `			sRef.nName, sRef.zName);` |
|      - | 10284 | `	}` |
|      5 | 10285 | `	return PH7_OK;` |
|      7 | 10286 | `}` |
|      6 | 10287 | `static int vm_builtin_ReflectionEnumCase_getEnum(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 10288 | `{` |
|      7 | 10289 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 10290 | `	ReflectMemberRef sRef;` |
|      - | 10291 | `	ph7_class *pRC;` |
|      - | 10292 | `	ph7_class_instance *pObj;` |
|      3 | 10293 | `	SXUNUSED(nArg);` |
|      3 | 10294 | `	SXUNUSED(apArg);` |
|      7 | 10295 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) ){` |
|    ! 0 | 10296 | `		ph7_result_null(pCtx);` |
|    ! 0 | 10297 | `		return PH7_OK;` |
|      - | 10298 | `	}` |
|      7 | 10299 | `	pRC = PH7_VmExtractClass(pVm, "ReflectionEnum", sizeof("ReflectionEnum")-1, FALSE, 0);` |
|      7 | 10300 | `	pObj = pRC ? PH7_NewClassInstance(pVm, pRC) : 0;` |
|      7 | 10301 | `	if( pObj == 0 ){` |
|    ! 0 | 10302 | `		ph7_result_null(pCtx);` |
|    ! 0 | 10303 | `		return PH7_OK;` |
|      - | 10304 | `	}` |
|     10 | 10305 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(&sRef.pClass->sName),` |
|      6 | 10306 | `		(int)SyStringLength(&sRef.pClass->sName));` |
|      7 | 10307 | `	return ReflectResultObject(pCtx, pObj);` |
|      4 | 10308 | `}` |
|      - | 10309 | ``/* getBackingValue(): the `value` the case singleton carries. The singleton is`` |
|      - | 10310 | ` * the constant's own value, so the parent's accessor materializes it. */` |
|     24 | 10311 | `static int vm_builtin_ReflectionEnumCase_getBackingValue(ph7_context *pCtx, int nArg,` |
|      - | 10312 | `	ph7_value **apArg)` |
|      2 | 10313 | `{` |
|      - | 10314 | `	ReflectMemberRef sRef;` |
|      - | 10315 | `	ph7_value *pVal, *pBacking;` |
|      - | 10316 | `	sxi32 rc;` |
|     12 | 10317 | `	SXUNUSED(nArg);` |
|     12 | 10318 | `	SXUNUSED(apArg);` |
|     26 | 10319 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 10320 | `		ph7_result_null(pCtx);` |
|    ! 0 | 10321 | `		return PH7_OK;` |
|      - | 10322 | `	}` |
|     26 | 10323 | `	rc = ReflectConstSlot(pCtx, sRef.pClass, sRef.pAttr, &pVal);` |
|     26 | 10324 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10325 | `		return rc;` |
|      - | 10326 | `	}` |
|     26 | 10327 | `	pBacking = (pVal && (pVal->iFlags & MEMOBJ_OBJ))` |
|     36 | 10328 | `		? PH7_NativeAttr((ph7_class_instance *)pVal->x.pOther, "value") : 0;` |
|     26 | 10329 | `	if( pBacking ){` |
|     26 | 10330 | `		ph7_result_value(pCtx, pBacking);` |
|     14 | 10331 | `	}else{` |
|    ! 0 | 10332 | `		ph7_result_null(pCtx);` |
|      - | 10333 | `	}` |
|     26 | 10334 | `	return PH7_OK;` |
|     14 | 10335 | `}` |
|      - | 10336 | `/*` |
|      - | 10337 | ` * Declare the three. Called from PH7_VmInstallReflection() where chunk 6 used` |
|      - | 10338 | ` * to be compiled, so both parents are installed (PH7_ClassInherit COPIES a` |
|      - | 10339 | ` * base's methods down — a native subclass needs its parent declared first).` |
|      - | 10340 | ` *` |
|      - | 10341 | ` * The uncloneable/unserializable flags are php's answer for each class and do` |
|      - | 10342 | ``  * NOT come down with the inheritance: without them `clone $enumReflector` `` |
|      - | 10343 | ` * reached the private __clone it inherited and reported that instead.` |
|      - | 10344 | ` */` |
|   8445 | 10345 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionEnum(ph7_vm *pVm)` |
|      5 | 10346 | `{` |
|      - | 10347 | `	static const PH7_NativeMethodDef aEnumMethod[] = {` |
|      - | 10348 | `		{ "__construct",    PH7_MOD_PUBLIC, "object\|string $objectOrClass", "",` |
|      - | 10349 | `		  vm_builtin_ReflectionEnum_construct },` |
|      - | 10350 | `		{ "hasCase",        PH7_MOD_PUBLIC, "string $name", "bool",` |
|      - | 10351 | `		  vm_builtin_ReflectionEnum_hasCase },` |
|      - | 10352 | `		{ "getCase",        PH7_MOD_PUBLIC, "string $name", "ReflectionEnumUnitCase",` |
|      - | 10353 | `		  vm_builtin_ReflectionEnum_getCase },` |
|      - | 10354 | `		{ "getCases",       PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionEnum_getCases },` |
|      - | 10355 | `		{ "isBacked",       PH7_MOD_PUBLIC, "", "bool",  vm_builtin_ReflectionEnum_isBacked },` |
|      - | 10356 | `		{ "getBackingType", PH7_MOD_PUBLIC, "", "?ReflectionNamedType",` |
|      - | 10357 | `		  vm_builtin_ReflectionEnum_getBackingType },` |
|      - | 10358 | `	};` |
|      - | 10359 | `	static const PH7_NativeMethodDef aCaseMethod[] = {` |
|      - | 10360 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|      - | 10361 | `		  vm_builtin_ReflectionEnumCase_construct },` |
|      - | 10362 | `		{ "getEnum",     PH7_MOD_PUBLIC, "", "ReflectionEnum",` |
|      - | 10363 | `		  vm_builtin_ReflectionEnumCase_getEnum },` |
|      - | 10364 | `		/* php redeclares getValue() on the case reflector for its narrower` |
|      - | 10365 | `		 * return type; the body is the parent's. */` |
|      - | 10366 | `		{ "getValue",    PH7_MOD_PUBLIC, "", "UnitEnum",` |
|      - | 10367 | `		  vm_builtin_ReflectionClassConstant_getValue },` |
|      - | 10368 | `	};` |
|      - | 10369 | `	static const PH7_NativeMethodDef aBackedMethod[] = {` |
|      - | 10370 | `		{ "__construct",     PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|      - | 10371 | `		  vm_builtin_ReflectionEnumBackedCase_construct },` |
|      - | 10372 | `		{ "getBackingValue", PH7_MOD_PUBLIC, "", "string\|int",` |
|      - | 10373 | `		  vm_builtin_ReflectionEnumCase_getBackingValue },` |
|      - | 10374 | `	};` |
|      - | 10375 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 10376 | `		{ "ReflectionEnum", "ReflectionClass", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 10377 | `		  aEnumMethod, SX_ARRAYSIZE(aEnumMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 10378 | `		{ "ReflectionEnumUnitCase", "ReflectionClassConstant", 0,` |
|      - | 10379 | `		  PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 10380 | `		  aCaseMethod, SX_ARRAYSIZE(aCaseMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 10381 | `		{ "ReflectionEnumBackedCase", "ReflectionEnumUnitCase", 0,` |
|      - | 10382 | `		  PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 10383 | `		  aBackedMethod, SX_ARRAYSIZE(aBackedMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 10384 | `	};` |
|   8450 | 10385 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 | 10386 | `}` |
|      - | 10387 | `/*` |
|      - | 10388 | ` * ---------------------------------------------------------------------------` |
|      - | 10389 | ` * php's Reflection EXPORT format — chunk 9, the last of the library's PHP.` |
|      - | 10390 | ` *` |
|      - | 10391 | ` * Every Reflector's __toString(). It is a byte-exact format with no API of its` |
|      - | 10392 | ` * own, so the chunk was written against the PUBLIC reflection API of whatever` |
|      - | 10393 | ` * it was printing — the only thing PHP could reach. All of those targets are C` |
|      - | 10394 | ` * now, so this reads the engine directly: the member walk for order and` |
|      - | 10395 | ` * visibility, ReflectParamAt for parameters, ReflectExportValue (built for the` |
|      - | 10396 | ` * attribute-argument block) for every value.` |
|      - | 10397 | ` *` |
|      - | 10398 | ` * Defined at the END of the file because it needs all of that; the five entry` |
|      - | 10399 | ` * points are forward-declared above their callers.` |
|      - | 10400 | ` * ---------------------------------------------------------------------------` |
|      - | 10401 | ` */` |
|      - | 10402 |  |
|      - | 10403 | `/*` |
|      - | 10404 | ` * "internal:<extension>" / "user" — the first tag of every function and class` |
|      - | 10405 | `` * head. php NAMES the extension there (`<internal:json>`), and this printed`` |
|      - | 10406 | `` * `internal:Core` for all 878 functions and 197 classes because the engine had`` |
|      - | 10407 | ` * no partition to name one from. iExt < 0 is a userland target.` |
|      - | 10408 | ` */` |
|   5212 | 10409 | `static void ReflectExportKind(SyBlob *pOut, int bInternal, int iExt, int bDeprecated)` |
|      4 | 10410 | `{` |
|      - | 10411 | `	/* The two questions are separate: an INTERNAL target may still name no module` |
|      - | 10412 | ``	 * (php's fabricated `Closure::__invoke` prints `<internal>` with nothing after`` |
|      - | 10413 | `	 * it), so the extension is only appended when there is one. */` |
|   5216 | 10414 | `	SyBlobAppend(pOut, bInternal ? "internal" : "user", bInternal ? 8 : 4);` |
|      - | 10415 | `	/* php writes the three parts in this order and each on its own condition,` |
|      - | 10416 | ``	 * so a deprecated INTERNAL name reads `<internal, deprecated:curl>` -- the`` |
|      - | 10417 | ``	 * extension hangs off `deprecated`, not off `internal`. */`` |
|   5216 | 10418 | `	if( bDeprecated ){` |
|     93 | 10419 | `		SyBlobAppend(pOut, ", deprecated", sizeof(", deprecated")-1);` |
|     45 | 10420 | `	}` |
|   5216 | 10421 | `	if( bInternal && iExt >= 0 ){` |
|   5070 | 10422 | `		SyBlobFormat(pOut, ":%s", PH7_VmExtensionName(iExt));` |
|   2533 | 10423 | `	}` |
|   5216 | 10424 | `}` |
|      - | 10425 | `/*` |
|      - | 10426 | ` * A declared type followed by a space, or nothing at all.` |
|      - | 10427 | ` *` |
|      - | 10428 | `` * php's EXPORT spells a standalone `iterable` as the two types it stands for`` |
|      - | 10429 | `` * where getType() prints `iterable` -- the one place the two renderings of a`` |
|      - | 10430 | ` * declared type differ (ReflectExportIterable is that difference, named once).` |
|      - | 10431 | ` */` |
|   8498 | 10432 | `static const SyString * ReflectExportIterable(const SyString *pType, SyString *pOut)` |
|      5 | 10433 | `{` |
|   8503 | 10434 | `	const char *z = pType ? SyStringData(pType) : 0;` |
|   8503 | 10435 | `	sxu32 n = z ? SyStringLength(pType) : 0;` |
|   8503 | 10436 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|    ! 0 | 10437 | `		SyStringInitFromBuf(pOut,"Traversable\|array",sizeof("Traversable\|array")-1);` |
|    ! 0 | 10438 | `		return pOut;` |
|      - | 10439 | `	}` |
|   8503 | 10440 | `	if( n == 9 && z[0] == '?' && SyStrnicmp(&z[1],"iterable",8) == 0 ){` |
|    ! 0 | 10441 | `		SyStringInitFromBuf(pOut,"Traversable\|array\|null",sizeof("Traversable\|array\|null")-1);` |
|    ! 0 | 10442 | `		return pOut;` |
|      - | 10443 | `	}` |
|   8503 | 10444 | `	return pType;` |
|   4254 | 10445 | `}` |
|   4000 | 10446 | `static void ReflectExportTypeSp(SyBlob *pOut, const SyString *pType)` |
|      5 | 10447 | `{` |
|      - | 10448 | `	SyString sIter;` |
|   4005 | 10449 | `	pType = ReflectExportIterable(pType,&sIter);` |
|   4005 | 10450 | `	if( pType && SyStringLength(pType) > 0 ){` |
|   3653 | 10451 | `		SyBlobAppend(pOut, SyStringData(pType), SyStringLength(pType));` |
|   3653 | 10452 | `		SyBlobAppend(pOut, " ", sizeof(char));` |
|   1824 | 10453 | `	}` |
|   4005 | 10454 | `}` |
|      - | 10455 | `/* php's visibility word for a member. */` |
|   5686 | 10456 | `static const char * ReflectExportVis(sxi32 iProtection)` |
|      5 | 10457 | `{` |
|   5691 | 10458 | `	if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     56 | 10459 | `		return "private";` |
|      - | 10460 | `	}` |
|   5637 | 10461 | `	return iProtection == PH7_CLASS_PROT_PROTECTED ? "protected" : "public";` |
|   2848 | 10462 | `}` |
|      - | 10463 | `/*` |
|      - | 10464 | `` * The text after `= ` in a parameter default.`` |
|      - | 10465 | ` *` |
|      - | 10466 | ` * php prints a constant-reference default as the CONSTANT's name, not its value` |
|      - | 10467 | `` * (`$f = M_PI`) — that argument was never folded, so php still has the source`` |
|      - | 10468 | `` * expression. `<default>` is what the chunk answered when the value could not`` |
|      - | 10469 | `` * be produced at all, which is a declared `= ?` row in aBuiltinSig[].`` |
|      - | 10470 | ` */` |
|   1416 | 10471 | `static void ReflectExportDefault(ph7_context *pCtx, SyBlob *pOut, ReflectParamDesc *pDesc)` |
|      5 | 10472 | `{` |
|   1421 | 10473 | `	const char *z = 0;` |
|   1421 | 10474 | `	int n = 0;` |
|   1421 | 10475 | `	if( ReflectParamDefConst(pCtx, pDesc, &z, &n) ){` |
|    150 | 10476 | `		SyBlobAppend(pOut, z, (sxu32)n);` |
|    785 | 10477 | `		return;` |
|      - | 10478 | `	}` |
|   1275 | 10479 | `	if( pDesc->pArg ){` |
|      - | 10480 | `		ph7_value sValue;` |
|     76 | 10481 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     76 | 10482 | `		VmLocalExec(pCtx->pVm, &pDesc->pArg->aByteCode, &sValue, FALSE);` |
|      - | 10483 | `		/* php has two spellings for the same default and picks by whether the` |
|      - | 10484 | ``		 * function is INTERNAL: `null` and double quotes there, `NULL` and single`` |
|      - | 10485 | `		 * quotes for a userland one. A builtin written as prelude PHP is internal` |
|      - | 10486 | `		 * to php however this engine chose to implement it, so scandir()'s` |
|      - | 10487 | ``		 * `$context = NULL` and clearstatcache()'s `$filename = ''` were both`` |
|      - | 10488 | `		 * printed in the wrong one. */` |
|     74 | 10489 | `		if( pDesc->bInternal` |
|     42 | 10490 | `		 && (sValue.iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING ){` |
|      4 | 10491 | `			ReflectExportStrQ(pOut, (const char *)SyBlobData(&sValue.sBlob),` |
|      1 | 10492 | `				SyBlobLength(&sValue.sBlob), '"');` |
|     75 | 10493 | `		}else if( pDesc->bInternal && (sValue.iFlags & MEMOBJ_NULL) ){` |
|      3 | 10494 | `			SyBlobAppend(pOut, "null", sizeof("null")-1);` |
|      2 | 10495 | `		}else{` |
|     72 | 10496 | `			ReflectExportValue(pCtx, pOut, &sValue, 0);` |
|      - | 10497 | `		}` |
|     76 | 10498 | `		PH7_MemObjRelease(&sValue);` |
|     76 | 10499 | `		return;` |
|      - | 10500 | `	}` |
|      - | 10501 | `	{` |
|      - | 10502 | `		/* A DECLARED default is signature TEXT: reduce it the way` |
|      - | 10503 | `		 * getDefaultValue() does, and fall back to php's own placeholder.` |
|      - | 10504 | `		 * Only an INTERNAL parameter reaches this branch (a compiled one carries` |
|      - | 10505 | `		 * pArg above), which is exactly the case php prints DOUBLE-quoted. */` |
|   1201 | 10506 | `		const char *zDef = SyStringData(&pDesc->sDefText);` |
|   1201 | 10507 | `		int nDef = (int)SyStringLength(&pDesc->sDefText);` |
|   1201 | 10508 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|   1201 | 10509 | `		if( ReflectSigHas(zDef, nDef, "::", 2) ){` |
|      - | 10510 | `			/* Rule 28's split, on the declared side: php's stub keeps the SOURCE of a` |
|      - | 10511 | `			 * constant-expression default, so the export prints` |
|      - | 10512 | ``			 * `= SplFileInfo::class` and `= A::K \| A::J` verbatim while`` |
|      - | 10513 | `			 * getDefaultValue() answers what they evaluate to. */` |
|     57 | 10514 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|     57 | 10515 | `			return;` |
|      - | 10516 | `		}` |
|   1147 | 10517 | `		if( pVal && ReflectSigGlobalConst(pCtx, zDef, nDef, pVal) ){` |
|      - | 10518 | ``			/* Same split for a GLOBAL constant: `= E_ERROR`, never `= 1`. */`` |
|    ! 0 | 10519 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|    ! 0 | 10520 | `			return;` |
|      - | 10521 | `		}` |
|   1142 | 10522 | `		if( pVal && ReflectSigHas(zDef, nDef, "\|", 1)` |
|    583 | 10523 | `		 && ReflectSigConstExpr(pCtx, zDef, nDef, pVal) ){` |
|      - | 10524 | ``			/* And for the `\|` fold of them, which is the one shape whose VALUE`` |
|      - | 10525 | ``			 * names no constant at all: `SQLITE3_OPEN_READWRITE \|`` |
|      - | 10526 | ``			 * SQLITE3_OPEN_CREATE` prints as itself and answers 6. */`` |
|     16 | 10527 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|     16 | 10528 | `			return;` |
|      - | 10529 | `		}` |
|   1133 | 10530 | `		if( ReflectSigIsSourceExpr(zDef, nDef) ){` |
|      - | 10531 | `			/* The two arithmetic shapes: a RADIX integer and a product. php prints` |
|      - | 10532 | ``			 * `0777` and `2 * 1024 * 1024`, never the 511 and 2097152 they reduce`` |
|      - | 10533 | `			 * to -- and answers those numbers from getDefaultValue() all the same. */` |
|      7 | 10534 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|      7 | 10535 | `			return;` |
|      - | 10536 | `		}` |
|   1127 | 10537 | `		if( pVal && ReflectSigScalar(pCtx, zDef, nDef, pVal) ){` |
|   1079 | 10538 | `			if( (pVal->iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING ){` |
|    320 | 10539 | `				ReflectExportStrQ(pOut, (const char *)SyBlobData(&pVal->sBlob),` |
|    105 | 10540 | `					SyBlobLength(&pVal->sBlob), '"');` |
|    215 | 10541 | `				return;` |
|      - | 10542 | `			}` |
|    868 | 10543 | `			if( pVal->iFlags & MEMOBJ_NULL ){` |
|      - | 10544 | `				/* The same internal/user split as the quote character above: php` |
|      - | 10545 | ``				 * spells an INTERNAL parameter's null default `null` and a`` |
|      - | 10546 | ``				 * userland one `NULL` (`= null` for every `?T $x = null` stub row). */`` |
|    403 | 10547 | `				SyBlobAppend(pOut, "null", sizeof("null")-1);` |
|    403 | 10548 | `				return;` |
|      - | 10549 | `			}` |
|    468 | 10550 | `			ReflectExportValue(pCtx, pOut, pVal, 0);` |
|    468 | 10551 | `			return;` |
|      - | 10552 | `		}` |
|     48 | 10553 | `		if( (nDef >= 1 && zDef[0] == '[')` |
|     27 | 10554 | `		 \|\| (nDef >= (int)sizeof("array (")-1` |
|    ! 0 | 10555 | `		  && SyMemcmp(zDef, "array (", sizeof("array (")-1) == 0) ){` |
|     51 | 10556 | `			SyBlobAppend(pOut, "[]", sizeof("[]")-1);` |
|     51 | 10557 | `			return;` |
|      - | 10558 | `		}` |
|      - | 10559 | `	}` |
|    ! 0 | 10560 | `	SyBlobAppend(pOut, "<default>", sizeof("<default>")-1);` |
|    713 | 10561 | `}` |
|      - | 10562 | ``/* `Parameter #0 [ <optional> int &...$name = 5 ]` */`` |
|   3514 | 10563 | `static void ReflectExportParamLine(ph7_context *pCtx, SyBlob *pOut, ReflectParamDesc *pDesc)` |
|      5 | 10564 | `{` |
|   5276 | 10565 | `	SyBlobFormat(pOut, "Parameter #%d [ <%s> ", pDesc->iPos,` |
|   3514 | 10566 | `		pDesc->bOptional ? "optional" : "required");` |
|   3519 | 10567 | `	ReflectExportTypeSp(pOut, &pDesc->sType);` |
|   3519 | 10568 | `	if( pDesc->bByRef ){` |
|     80 | 10569 | `		SyBlobAppend(pOut, "&", sizeof(char));` |
|     38 | 10570 | `	}` |
|   3519 | 10571 | `	if( pDesc->bVariadic ){` |
|     82 | 10572 | `		SyBlobAppend(pOut, "...", sizeof("...")-1);` |
|     39 | 10573 | `	}` |
|   3519 | 10574 | `	SyBlobAppend(pOut, "$", sizeof(char));` |
|   3519 | 10575 | `	SyBlobAppend(pOut, SyStringData(&pDesc->sName), SyStringLength(&pDesc->sName));` |
|   3519 | 10576 | `	if( pDesc->bHasDef ){` |
|   1421 | 10577 | `		SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|   1421 | 10578 | `		ReflectExportDefault(pCtx, pOut, pDesc);` |
|   2811 | 10579 | `	}else if( pDesc->bOptional && !pDesc->bVariadic ){` |
|      - | 10580 | `		/* An OPTIONAL parameter with no default a caller could read still gets` |
|      - | 10581 | ``		 * an `= ` from php -- and php's own placeholder after it, since there is`` |
|      - | 10582 | ``		 * nothing to print. `mt_rand()`'s two bounds and `get_class()`'s`` |
|      - | 10583 | `		 * $object are that shape; a VARIADIC tail is not, because its "default"` |
|      - | 10584 | `		 * is having no further arguments. */` |
|     31 | 10585 | `		SyBlobAppend(pOut, " = <default>", sizeof(" = <default>")-1);` |
|     14 | 10586 | `	}` |
|   3519 | 10587 | `	SyBlobAppend(pOut, " ]", sizeof(" ]")-1);` |
|   3519 | 10588 | `}` |
|      - | 10589 | `/*` |
|      - | 10590 | `` * `Property [ public protected(set) readonly int $x = 5 ]` + newline.`` |
|      - | 10591 | ` *` |
|      - | 10592 | ` * The set-visibility is printed only when it DIFFERS from the get-visibility,` |
|      - | 10593 | `` * which is why a `public readonly` property shows php's implied`` |
|      - | 10594 | `` * `protected(set)` and a `protected readonly` one shows nothing.`` |
|      - | 10595 | ` */` |
|    612 | 10596 | `static void ReflectExportPropLine(ph7_context *pCtx, SyBlob *pOut, ph7_class_attr *pAttr,` |
|      - | 10597 | `	const SyString *pKey)` |
|      4 | 10598 | `{` |
|    616 | 10599 | `	sxi32 iSet = pAttr->iProtection;` |
|    616 | 10600 | `	SyBlobAppend(pOut, "Property [ ", sizeof("Property [ ")-1);` |
|      - | 10601 | `	/* php's modifier MASK implies two bits the declaration never wrote:` |
|      - | 10602 | `	 * private(set) implies final, and readonly implies protected(set). A property` |
|      - | 10603 | `	 * DECLARED final (PHP 8.4) prints the same word, and the two spellings do not` |
|      - | 10604 | `	 * print it twice. */` |
|    616 | 10605 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_FINAL\|PH7_CLASS_ATTR_PRIVATE_SET) ){` |
|      7 | 10606 | `		SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|      3 | 10607 | `	}` |
|    616 | 10608 | `	SyBlobFormat(pOut, "%s ", ReflectExportVis(pAttr->iProtection));` |
|    616 | 10609 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|      5 | 10610 | `		iSet = PH7_CLASS_PROT_PRIVATE;` |
|    611 | 10611 | `	}else if( (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET)` |
|    607 | 10612 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){` |
|     29 | 10613 | `		iSet = PH7_CLASS_PROT_PROTECTED;` |
|     14 | 10614 | `	}` |
|      - | 10615 | `	/* The set-visibility is printed only when it DIFFERS from the get one. */` |
|    616 | 10616 | `	if( iSet != pAttr->iProtection ){` |
|     29 | 10617 | `		SyBlobFormat(pOut, "%s(set) ", ReflectExportVis(iSet));` |
|     14 | 10618 | `	}` |
|    616 | 10619 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|     10 | 10620 | `		SyBlobAppend(pOut, "static ", sizeof("static ")-1);` |
|      4 | 10621 | `	}` |
|    616 | 10622 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|    ! 0 | 10623 | `		SyBlobAppend(pOut, "virtual ", sizeof("virtual ")-1);` |
|    ! 0 | 10624 | `	}` |
|    616 | 10625 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|     29 | 10626 | `		SyBlobAppend(pOut, "readonly ", sizeof("readonly ")-1);` |
|     14 | 10627 | `	}` |
|    616 | 10628 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      - | 10629 | `		char zType[192];` |
|      - | 10630 | `		SyString sType;` |
|    490 | 10631 | `		const char *zT = ReflectMemberTypeText(pCtx->pVm,pAttr,zType,sizeof(zType));` |
|    490 | 10632 | `		SyStringInitFromBuf(&sType,zT,(sxu32)SyStrlen(zT));` |
|    490 | 10633 | `		ReflectExportTypeSp(pOut, &sType);` |
|    243 | 10634 | `	}` |
|    616 | 10635 | `	SyBlobAppend(pOut, "$", sizeof(char));` |
|    616 | 10636 | `	SyBlobAppend(pOut, SyStringData(pKey), SyStringLength(pKey));` |
|    612 | 10637 | `	if( SySetUsed(&pAttr->aByteCode) > 0 \|\| pAttr->pNativeValue` |
|    447 | 10638 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      - | 10639 | `		ph7_value sValue;` |
|    289 | 10640 | `		SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|    289 | 10641 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|    289 | 10642 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     57 | 10643 | `			VmLocalExec(pCtx->pVm, &pAttr->aByteCode, &sValue, FALSE);` |
|     57 | 10644 | `			PH7_VmResolvedDefault(pCtx->pVm, pAttr->pDeclClass, pAttr, &sValue);` |
|    260 | 10645 | `		}else if( pAttr->pNativeValue ){` |
|    231 | 10646 | `			PH7_NativeLiteralValue(pCtx->pVm, pAttr->pNativeValue, &sValue);` |
|    115 | 10647 | `		}` |
|    289 | 10648 | `		ReflectExportValue(pCtx, pOut, &sValue, 0);` |
|    289 | 10649 | `		PH7_MemObjRelease(&sValue);` |
|    143 | 10650 | `	}` |
|      - | 10651 | `	/* A hooked property names its hooks; php lists no method for them. */` |
|    616 | 10652 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET) ){` |
|      3 | 10653 | `		SyBlobAppend(pOut, " {", sizeof(" {")-1);` |
|      3 | 10654 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET ){` |
|      3 | 10655 | `			SyBlobAppend(pOut, " get;", sizeof(" get;")-1);` |
|      1 | 10656 | `		}` |
|      3 | 10657 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET ){` |
|      3 | 10658 | `			SyBlobAppend(pOut, " set;", sizeof(" set;")-1);` |
|      1 | 10659 | `		}` |
|      3 | 10660 | `		SyBlobAppend(pOut, " }", sizeof(" }")-1);` |
|      1 | 10661 | `	}` |
|    616 | 10662 | `	SyBlobAppend(pOut, " ]\n", sizeof(" ]\n")-1);` |
|    616 | 10663 | `}` |
|      - | 10664 | `/*` |
|      - | 10665 | `` * `Constant [ final public int NAME ] { value }` + newline. The TYPE is the`` |
|      - | 10666 | ` * value's, not a declared one, and an object (an enum case) prints as the word` |
|      - | 10667 | ` * Object under its own class name.` |
|      - | 10668 | ` */` |
|    674 | 10669 | `static sxi32 ReflectExportConstLine(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass,` |
|      - | 10670 | `	ph7_class_attr *pAttr, const SyString *pKey)` |
|      2 | 10671 | `{` |
|    676 | 10672 | `	ph7_value *pVal = 0;` |
|    676 | 10673 | `	sxi32 rc = ReflectConstSlot(pCtx, pClass, pAttr, &pVal);` |
|      - | 10674 | `	const char *zType;` |
|    676 | 10675 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10676 | `		return rc;` |
|      - | 10677 | `	}` |
|    676 | 10678 | `	SyBlobAppend(pOut, "Constant [ ", sizeof("Constant [ ")-1);` |
|    676 | 10679 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      7 | 10680 | `		SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|      3 | 10681 | `	}` |
|    676 | 10682 | `	SyBlobFormat(pOut, "%s ", ReflectExportVis(pAttr->iProtection));` |
|    674 | 10683 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|    339 | 10684 | `	 && SyStringLength(&pAttr->sTypeName) > 0 ){` |
|      - | 10685 | `		/* php 8.3's typed class constant prints what it DECLARED; the word below` |
|      - | 10686 | `		 * is what an untyped one's VALUE happens to be. */` |
|      - | 10687 | `		char zDecl[192];` |
|      - | 10688 | `		SyString sDecl;` |
|    ! 0 | 10689 | `		const char *zT = ReflectMemberTypeText(pCtx->pVm,pAttr,zDecl,sizeof(zDecl));` |
|    ! 0 | 10690 | `		SyStringInitFromBuf(&sDecl,zT,(sxu32)SyStrlen(zT));` |
|    ! 0 | 10691 | `		ReflectExportTypeSp(pOut, &sDecl);` |
|    ! 0 | 10692 | `	}else{` |
|    676 | 10693 | `		zType = ReflectExportTypeWord(pVal);` |
|    676 | 10694 | `		if( zType ){` |
|    674 | 10695 | `			SyBlobFormat(pOut, "%s ", zType);` |
|    338 | 10696 | `		}else{` |
|      3 | 10697 | `			SyBlobFormat(pOut, "%z ", &((ph7_class_instance *)pVal->x.pOther)->pClass->sName);` |
|      - | 10698 | `		}` |
|      - | 10699 | `	}` |
|    676 | 10700 | `	SyBlobAppend(pOut, SyStringData(pKey), SyStringLength(pKey));` |
|    676 | 10701 | `	SyBlobAppend(pOut, " ] { ", sizeof(" ] { ")-1);` |
|    676 | 10702 | `	ReflectExportConstValue(pCtx, pOut, pVal);` |
|    676 | 10703 | `	SyBlobAppend(pOut, " }\n", sizeof(" }\n")-1);` |
|    676 | 10704 | `	return SXRET_OK;` |
|    339 | 10705 | `}` |
|      - | 10706 | `/* A native class METHOD as a function reference (ReflectFuncFill's tail, for a` |
|      - | 10707 | ` * method the member walk handed over rather than one a receiver names). */` |
|   4124 | 10708 | `static void ReflectFuncFromMethod(ph7_vm *pVm, ph7_class *pClass, ph7_class_method *pMeth,` |
|      - | 10709 | `	ReflectFuncRef *pOut)` |
|      2 | 10710 | `{` |
|   4126 | 10711 | `	SyZero(pOut, sizeof(*pOut));` |
|   4126 | 10712 | `	pOut->pVm = pVm;` |
|   4126 | 10713 | `	pOut->pClass = pClass;` |
|   4126 | 10714 | `	pOut->pMeth = pMeth;` |
|   4126 | 10715 | `	pOut->pFunc = &pMeth->sFunc;` |
|   4126 | 10716 | `	if( (pOut->pFunc->iFlags & VM_FUNC_NATIVE) && pOut->pFunc->pNative ){` |
|   4088 | 10717 | `		pOut->zSig = pOut->pFunc->pNative->zSig;` |
|   4088 | 10718 | `		if( SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|   4088 | 10719 | `			pOut->zRet = pOut->pFunc->pNative->zRet;` |
|   2043 | 10720 | `		}` |
|   2043 | 10721 | `	}` |
|   4126 | 10722 | `}` |
|      - | 10723 | `/*` |
|      - | 10724 | ` * The Method / Function / Closure block.` |
|      - | 10725 | ` *` |
|      - | 10726 | ` * pOwner is the class being EXPORTED when there is one: php's tags are` |
|      - | 10727 | ` * relative to it — a method it did not declare "inherits" from its declaring` |
|      - | 10728 | ` * class — and the reflector alone cannot say that, because $class is the` |
|      - | 10729 | `` * DECLARING class. `overwrites` is the other direction and needs no owner: the`` |
|      - | 10730 | ` * declaring class's own parent declares the same method.` |
|      - | 10731 | ` */` |
|   4900 | 10732 | `static sxi32 ReflectExportFuncBlock(ph7_context *pCtx, SyBlob *pOut, ReflectFuncRef *pRef,` |
|      - | 10733 | `	const char *zIndent, ph7_class *pOwner)` |
|      4 | 10734 | `{` |
|      - | 10735 | `	SyBlob sBody;` |
|   4904 | 10736 | `	int bInternal = ReflectFuncIsInternal(pRef);` |
|   7309 | 10737 | `	int bDeprecated = ReflectFuncDeprecated(pRef) != 0` |
|   4900 | 10738 | `		\|\| (pRef->pFunc != 0 && ReflectHasDeprecated(&pRef->pFunc->aAttrs));` |
|   4904 | 10739 | `	int nParam = ReflectParamCount(pRef);` |
|   4904 | 10740 | `	const char *zRet = 0;` |
|      - | 10741 | `	char zRetBuf[192];` |
|   4904 | 10742 | `	int nRet = 0, bHasRet, bTentRet = 0;` |
|   4904 | 10743 | `	sxi32 rc = SXRET_OK;` |
|   4904 | 10744 | `	SyBlobInit(&sBody, &pCtx->pVm->sAllocator);` |
|   4904 | 10745 | `	bHasRet = ReflectFuncRetTextEx(pRef, &zRet, &nRet, &bTentRet, zRetBuf, sizeof(zRetBuf));` |
|   4904 | 10746 | `	if( pRef->pMeth ){` |
|   4375 | 10747 | `		ph7_class *pDecl = ReflectFuncDeclClass(pRef);` |
|      - | 10748 | `		ph7_class *pProto;` |
|   4375 | 10749 | `		SyString *pName = &pRef->pMeth->sFunc.sName;` |
|      - | 10750 | `		int bInherits, bCtor;` |
|   4375 | 10751 | `		SyBlobAppend(&sBody, "Method [ <", sizeof("Method [ <")-1);` |
|      - | 10752 | `		/* A method names its DECLARING class's extension, which is why php's` |
|      - | 10753 | ``		 * `JsonException::__construct` prints `<internal:Core, inherits`` |
|      - | 10754 | ``		 * Exception, ctor>` rather than json's own name. */`` |
|   4375 | 10755 | `		ReflectExportKind(&sBody, bInternal, ReflectFuncExtId(pRef), bDeprecated);` |
|   4375 | 10756 | `		bInherits = (pOwner != 0 && pDecl != 0 && pDecl != pOwner);` |
|   4375 | 10757 | `		if( bInherits ){` |
|   2117 | 10758 | `			SyBlobFormat(&sBody, ", inherits %z", &pDecl->sName);` |
|   3317 | 10759 | `		}else if( pDecl ){` |
|   3387 | 10760 | `			ph7_class *pOver = ReflectOverwritesIn(pDecl,` |
|   2256 | 10761 | `				SyStringData(pName), (int)SyStringLength(pName));` |
|   2259 | 10762 | `			if( pOver ){` |
|    165 | 10763 | `				SyBlobFormat(&sBody, ", overwrites %z", &pOver->sName);` |
|     82 | 10764 | `			}` |
|   1128 | 10765 | `		}` |
|   6936 | 10766 | `		bCtor = SyStringLength(pName) == sizeof("__construct")-1` |
|   4372 | 10767 | `			&& SyStrnicmp(SyStringData(pName), "__construct", sizeof("__construct")-1) == 0;` |
|      - | 10768 | `		/* The prototype belongs to the entry in the class the export was` |
|      - | 10769 | `		 * reached THROUGH, which is php's anchor for it: a class that only` |
|      - | 10770 | `		 * inherits a method still re-assigns its prototype when it implements` |
|      - | 10771 | `		 * an interface declaring the same one. */` |
|   2189 | 10772 | `		pProto = ReflectPrototypeOf(pCtx, pOwner ? pOwner : pRef->pClass,` |
|   4372 | 10773 | `			SyStringData(pName), (int)SyStringLength(pName), 0);` |
|   4375 | 10774 | `		if( pProto ){` |
|   1383 | 10775 | `			SyBlobFormat(&sBody, ", prototype %z", &pProto->sName);` |
|    690 | 10776 | `		}` |
|      - | 10777 | `		/* php closes the bracket with the ctor word, AFTER the prototype: an` |
|      - | 10778 | `` 		 * interface's constructor prints `prototype F1, ctor`. There is no `dtor` `` |
|      - | 10779 | `		 * twin -- php's exporter prints the word for ZEND_ACC_CTOR only, so a` |
|      - | 10780 | ``		 * `__destruct` reads `Method [ <user> public method __destruct ]`. */`` |
|   4375 | 10781 | `		if( bCtor ){` |
|    210 | 10782 | `			SyBlobAppend(&sBody, ", ctor", sizeof(", ctor")-1);` |
|    104 | 10783 | `		}` |
|   4375 | 10784 | `		SyBlobAppend(&sBody, "> ", sizeof("> ")-1);` |
|   4375 | 10785 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|    205 | 10786 | `			SyBlobAppend(&sBody, "abstract ", sizeof("abstract ")-1);` |
|    102 | 10787 | `		}` |
|   4375 | 10788 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|    445 | 10789 | `			SyBlobAppend(&sBody, "final ", sizeof("final ")-1);` |
|    222 | 10790 | `		}` |
|   4375 | 10791 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|     78 | 10792 | `			SyBlobAppend(&sBody, "static ", sizeof("static ")-1);` |
|     38 | 10793 | `		}` |
|   4375 | 10794 | `		SyBlobFormat(&sBody, "%s method %z ]", ReflectExportVis(pRef->pMeth->iProtection), pName);` |
|   2718 | 10795 | `	}else if( pRef->bTrampoline ){` |
|      - | 10796 | `		/* php's trampoline is a METHOD record under the name that was asked for;` |
|      - | 10797 | `		 * its only modifier is the catch-all's staticness. */` |
|     12 | 10798 | `		ph7_value *pFn = ReflectClosureAttr(pRef, "__fn");` |
|     12 | 10799 | `		SyBlobAppend(&sBody, "Closure [ <", sizeof("Closure [ <")-1);` |
|     12 | 10800 | `		ReflectExportKind(&sBody, bInternal, -1, bDeprecated);` |
|     12 | 10801 | `		SyBlobAppend(&sBody, "> ", sizeof("> ")-1);` |
|     12 | 10802 | `		if( PH7_VmClosureIsStatic(pCtx->pVm, pRef->pClosure) ){` |
|      8 | 10803 | `			SyBlobAppend(&sBody, "static ", sizeof("static ")-1);` |
|      3 | 10804 | `		}` |
|     12 | 10805 | `		SyBlobAppend(&sBody, "public method ", sizeof("public method ")-1);` |
|     12 | 10806 | `		if( pFn && (pFn->iFlags & MEMOBJ_STRING) ){` |
|     12 | 10807 | `			SyBlobAppend(&sBody, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      5 | 10808 | `		}` |
|     12 | 10809 | `		SyBlobAppend(&sBody, " ]", sizeof(" ]")-1);` |
|      7 | 10810 | `	}else{` |
|    781 | 10811 | `		SyBlobAppend(&sBody, pRef->pClosure ? "Closure [ <" : "Function [ <",` |
|    518 | 10812 | `			pRef->pClosure ? sizeof("Closure [ <")-1 : sizeof("Function [ <")-1);` |
|    522 | 10813 | `		ReflectExportKind(&sBody, bInternal, ReflectFuncExtId(pRef), bDeprecated);` |
|    522 | 10814 | `		SyBlobAppend(&sBody, "> function ", sizeof("> function ")-1);` |
|    522 | 10815 | `		if( pRef->pFunc ){` |
|     11 | 10816 | `			SyBlobFormat(&sBody, "%z", &pRef->pFunc->sName);` |
|    517 | 10817 | `		}else if( pRef->pHost ){` |
|    512 | 10818 | `			SyBlobFormat(&sBody, "%z", &pRef->pHost->sName);` |
|    254 | 10819 | `		}` |
|    522 | 10820 | `		SyBlobAppend(&sBody, " ]", sizeof(" ]")-1);` |
|      - | 10821 | `	}` |
|   4904 | 10822 | `	SyBlobAppend(&sBody, " {\n", sizeof(" {\n")-1);` |
|   4904 | 10823 | `	if( !bInternal && pRef->pFunc ){` |
|    103 | 10824 | `		SyBlobAppend(&sBody, "  @@ ", sizeof("  @@ ")-1);` |
|    103 | 10825 | `		if( SyStringLength(&pRef->pFunc->sFile) > 0 ){` |
|    154 | 10826 | `			SyBlobAppend(&sBody, SyStringData(&pRef->pFunc->sFile),` |
|    102 | 10827 | `				SyStringLength(&pRef->pFunc->sFile));` |
|     51 | 10828 | `		}` |
|    103 | 10829 | `		SyBlobFormat(&sBody, " %u - %u\n", pRef->pFunc->nLine, pRef->pFunc->nEndLine);` |
|     51 | 10830 | `	}` |
|      - | 10831 | `	/* php prints the parameter block for every INTERNAL function, and for a` |
|      - | 10832 | `	 * user one only when there is something to say -- with one exception, the` |
|      - | 10833 | ``	 * class-level `Closure::__invoke`: php fabricates it with no parameter`` |
|      - | 10834 | `	 * information at all (not an empty list), and prints neither section. */` |
|   4900 | 10835 | `	if( (nParam > 0 \|\| bHasRet \|\| bInternal)` |
|   4872 | 10836 | `	 && !(pRef->bFabricated && pRef->pClosure == 0)` |
|   4875 | 10837 | `	 && !(pRef->bTrampoline && nParam == 0) ){` |
|      - | 10838 | `		int n;` |
|   4842 | 10839 | `		SyBlobFormat(&sBody, "\n  - Parameters [%d] {\n", nParam);` |
|   8186 | 10840 | `		for( n = 0 ; n < nParam ; n++ ){` |
|      - | 10841 | `			ReflectParamDesc sDesc;` |
|   3348 | 10842 | `			if( !ReflectParamAt(pRef, n, &sDesc) ){` |
|    ! 0 | 10843 | `				continue;` |
|      - | 10844 | `			}` |
|   3348 | 10845 | `			SyBlobAppend(&sBody, "    ", sizeof("    ")-1);` |
|   3348 | 10846 | `			ReflectExportParamLine(pCtx, &sBody, &sDesc);` |
|   3348 | 10847 | `			SyBlobAppend(&sBody, "\n", sizeof(char));` |
|   1676 | 10848 | `		}` |
|   4842 | 10849 | `		SyBlobAppend(&sBody, "  }\n", sizeof("  }\n")-1);` |
|   2419 | 10850 | `	}` |
|   4900 | 10851 | `	if( bHasRet ){` |
|      - | 10852 | ``		/* php's two spellings: `Return [ T ]` for a declared type, `Tentative return`` |
|      - | 10853 | ``		 * [ T ]` for a stub's @tentative-return-type. */`` |
|   4502 | 10854 | `		if( bTentRet ){` |
|   2701 | 10855 | `			SyBlobAppend(&sBody, "  - Tentative return [ ", sizeof("  - Tentative return [ ")-1);` |
|   1352 | 10856 | `		}else{` |
|   1804 | 10857 | `			SyBlobAppend(&sBody, "  - Return [ ", sizeof("  - Return [ ")-1);` |
|      - | 10858 | `		}` |
|      - | 10859 | `		{` |
|      - | 10860 | ``			/* ...and the export's own spelling of a standalone `iterable`. */`` |
|      - | 10861 | `			SyString sRet, sIter;` |
|      - | 10862 | `			const SyString *pRet;` |
|   4502 | 10863 | `			SyStringInitFromBuf(&sRet,zRet,(sxu32)nRet);` |
|   4502 | 10864 | `			pRet = ReflectExportIterable(&sRet,&sIter);` |
|   4502 | 10865 | `			SyBlobAppend(&sBody, SyStringData(pRet), SyStringLength(pRet));` |
|      - | 10866 | `		}` |
|   4502 | 10867 | `		SyBlobAppend(&sBody, " ]\n", sizeof(" ]\n")-1);` |
|   2249 | 10868 | `	}` |
|   4900 | 10869 | `	SyBlobAppend(&sBody, "}\n", sizeof("}\n")-1);` |
|      - | 10870 | `	/* Indent every non-empty line, the way the chunk's explode/implode did. */` |
|   4900 | 10871 | `	if( zIndent == 0 \|\| zIndent[0] == '\0' ){` |
|    784 | 10872 | `		SyBlobAppend(pOut, SyBlobData(&sBody), SyBlobLength(&sBody));` |
|    396 | 10873 | `	}else{` |
|   4126 | 10874 | `		const char *z = (const char *)SyBlobData(&sBody);` |
|   4126 | 10875 | `		sxu32 n = SyBlobLength(&sBody), i = 0, iStart = 0;` |
|   4126 | 10876 | `		sxu32 nIndent = (sxu32)SyStrlen(zIndent);` |
| 655541 | 10877 | `		for( i = 0 ; i < n ; i++ ){` |
| 651417 | 10878 | `			if( z[i] != '\n' ){` |
| 624817 | 10879 | `				continue;` |
|      - | 10880 | `			}` |
|  26602 | 10881 | `			if( i > iStart ){` |
|  22484 | 10882 | `				SyBlobAppend(pOut, zIndent, nIndent);` |
|  22484 | 10883 | `				SyBlobAppend(pOut, &z[iStart], i - iStart);` |
|  11241 | 10884 | `			}` |
|  26602 | 10885 | `			SyBlobAppend(pOut, "\n", sizeof(char));` |
|  26602 | 10886 | `			iStart = i + 1;` |
|  13302 | 10887 | `		}` |
|      - | 10888 | `	}` |
|   4908 | 10889 | `	SyBlobRelease(&sBody);` |
|   4908 | 10890 | `	return rc;` |
|      4 | 10891 | `}` |
|      - | 10892 | ``/* The `- Enum cases [N]` block php prints where an enum's cases would otherwise`` |
|      - | 10893 | ` * be listed among its constants. A backed case shows its value UNQUOTED. */` |
|      6 | 10894 | `static sxi32 ReflectExportEnumCases(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass)` |
|      1 | 10895 | `{` |
|      7 | 10896 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      - | 10897 | `	sxu32 n;` |
|      7 | 10898 | `	SyBlobFormat(pOut, "\n  - Enum cases [%u] {\n", SySetUsed(&pClass->aEnumCases));` |
|     17 | 10899 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     11 | 10900 | `		SyBlobAppend(pOut, "    Case ", sizeof("    Case ")-1);` |
|     11 | 10901 | `		SyBlobAppend(pOut, SyStringData(&apCase[n]->sName), SyStringLength(&apCase[n]->sName));` |
|     11 | 10902 | `		if( pClass->nEnumBacking ){` |
|      9 | 10903 | `			ph7_value *pVal = 0;` |
|      - | 10904 | `			ph7_class_instance *pObj;` |
|      9 | 10905 | `			sxi32 rc = ReflectConstSlot(pCtx, pClass, apCase[n], &pVal);` |
|      9 | 10906 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 10907 | `				return rc;` |
|      - | 10908 | `			}` |
|      9 | 10909 | `			pObj = (pVal && (pVal->iFlags & MEMOBJ_OBJ)) ? (ph7_class_instance *)pVal->x.pOther : 0;` |
|      9 | 10910 | `			if( pObj ){` |
|      9 | 10911 | `				ph7_value *pBacking = PH7_NativeAttr(pObj, "value");` |
|      9 | 10912 | `				if( pBacking ){` |
|      - | 10913 | `					ph7_value sTmp;` |
|      - | 10914 | `					const char *zText;` |
|      - | 10915 | `					int nText;` |
|      9 | 10916 | `					PH7_MemObjInit(pCtx->pVm, &sTmp);` |
|      9 | 10917 | `					PH7_MemObjStore(pBacking, &sTmp);` |
|      9 | 10918 | `					zText = ph7_value_to_string(&sTmp, &nText);` |
|      9 | 10919 | `					SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|      9 | 10920 | `					if( nText > 0 ){` |
|      9 | 10921 | `						SyBlobAppend(pOut, zText, (sxu32)nText);` |
|      4 | 10922 | `					}` |
|      9 | 10923 | `					PH7_MemObjRelease(&sTmp);` |
|      4 | 10924 | `				}` |
|      4 | 10925 | `			}` |
|      4 | 10926 | `		}` |
|     11 | 10927 | `		SyBlobAppend(pOut, "\n", sizeof(char));` |
|      6 | 10928 | `	}` |
|      7 | 10929 | `	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|      7 | 10930 | `	return SXRET_OK;` |
|      4 | 10931 | `}` |
|      - | 10932 | `/* The Class / Interface / Enum block. */` |
|    312 | 10933 | `static sxi32 ReflectExportClassBlock(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass)` |
|      2 | 10934 | `{` |
|    314 | 10935 | `	ph7_vm *pVm = pCtx->pVm;` |
|    314 | 10936 | `	int bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;` |
|    314 | 10937 | `	int bEnum = (pClass->iFlags & PH7_CLASS_ENUM) != 0;` |
|    314 | 10938 | `	int bIface = (pClass->iFlags & PH7_CLASS_INTERFACE) != 0;` |
|      - | 10939 | `	SySet aMembers, aIface;` |
|    314 | 10940 | `	sxu32 n, nConst = 0, nStaticProp = 0, nProp = 0, nStaticMeth = 0, nMeth = 0;` |
|    314 | 10941 | `	sxi32 rc = SXRET_OK;` |
|      - | 10942 | `	int iPass;` |
|    314 | 10943 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|    314 | 10944 | `	SySetInit(&aIface, &pVm->sAllocator, sizeof(ph7_class *));` |
|    314 | 10945 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|    314 | 10946 | `	PH7_ReflectInterfacesOf(pVm, pClass, &aIface);` |
|      - | 10947 | `	/* ---- head ---- */` |
|    314 | 10948 | `	if( bIface ){` |
|     57 | 10949 | `		SyBlobAppend(pOut, "Interface [ <", sizeof("Interface [ <")-1);` |
|    286 | 10950 | `	}else if( bEnum ){` |
|      7 | 10951 | `		SyBlobAppend(pOut, "Enum [ <", sizeof("Enum [ <")-1);` |
|      4 | 10952 | `	}else{` |
|    252 | 10953 | `		SyBlobAppend(pOut, "Class [ <", sizeof("Class [ <")-1);` |
|      - | 10954 | `	}` |
|      - | 10955 | `	{` |
|    314 | 10956 | `		int iClassExt = ReflectClassExtId(pClass);` |
|    314 | 10957 | `		ReflectExportKind(pOut, iClassExt >= 0, iClassExt, 0);` |
|      - | 10958 | `	}` |
|    314 | 10959 | `	SyBlobAppend(pOut, "> ", sizeof("> ")-1);` |
|      - | 10960 | `	/* php tags a class that has an iteration handler, which its Traversable` |
|      - | 10961 | `	 * implementers have — and so does anything with a HOOKED property, because` |
|      - | 10962 | `	 * that is how php 8.4 walks one. An INTERFACE never carries one: php` |
|      - | 10963 | `	 * installs the handler when a CLASS implements Traversable, so` |
|      - | 10964 | ``	 * `interface I extends Iterator` prints no tag. */`` |
|      - | 10965 | `	{` |
|    414 | 10966 | `		int bIterable = !bIface && pVm->pTraversableClass != 0` |
|    440 | 10967 | `			&& PH7_VmInstanceOf(pClass, pVm->pTraversableClass);` |
|   3788 | 10968 | `		for( n = 0 ; !bIterable && n < SySetUsed(&aMembers) ; n++ ){` |
|   3476 | 10969 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   3474 | 10970 | `			if( pM->iKind == REFLECT_MEMBER_PROP && pM->pAttr` |
|    578 | 10971 | `			 && (pM->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) ){` |
|      3 | 10972 | `				bIterable = 1;` |
|      1 | 10973 | `			}` |
|   1739 | 10974 | `		}` |
|    314 | 10975 | `		if( bIterable ){` |
|     87 | 10976 | `			SyBlobAppend(pOut, "<iterateable> ", sizeof("<iterateable> ")-1);` |
|     43 | 10977 | `		}` |
|      - | 10978 | `	}` |
|    314 | 10979 | `	if( bIface ){` |
|     57 | 10980 | `		SyBlobFormat(pOut, "interface %z", &pClass->sName);` |
|    286 | 10981 | `	}else if( bEnum ){` |
|      7 | 10982 | `		SyBlobFormat(pOut, "enum %z", &pClass->sName);` |
|      7 | 10983 | `		if( pClass->nEnumBacking ){` |
|      5 | 10984 | `			SyBlobFormat(pOut, ": %s", (pClass->nEnumBacking & MEMOBJ_INT) ? "int" : "string");` |
|      2 | 10985 | `		}` |
|      4 | 10986 | `	}else{` |
|    252 | 10987 | `		if( pClass->iFlags & PH7_CLASS_ABSTRACT ){` |
|     13 | 10988 | `			SyBlobAppend(pOut, "abstract ", sizeof("abstract ")-1);` |
|      6 | 10989 | `		}` |
|    252 | 10990 | `		if( pClass->iFlags & PH7_CLASS_FINAL ){` |
|     47 | 10991 | `			SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|     23 | 10992 | `		}` |
|    252 | 10993 | `		if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      5 | 10994 | `			SyBlobAppend(pOut, "readonly ", sizeof("readonly ")-1);` |
|      2 | 10995 | `		}` |
|    252 | 10996 | `		SyBlobFormat(pOut, "class %z", &pClass->sName);` |
|    252 | 10997 | `		if( pClass->pBase ){` |
|    135 | 10998 | `			SyBlobFormat(pOut, " extends %z", &pClass->pBase->sName);` |
|     67 | 10999 | `		}` |
|      - | 11000 | `	}` |
|    314 | 11001 | `	if( SySetUsed(&aIface) > 0 ){` |
|    234 | 11002 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIface);` |
|      - | 11003 | `		/* An interface EXTENDS what a class implements. */` |
|    350 | 11004 | `		SyBlobAppend(pOut, bIface ? " extends " : " implements ",` |
|    116 | 11005 | `			bIface ? sizeof(" extends ")-1 : sizeof(" implements ")-1);` |
|    826 | 11006 | `		for( n = 0 ; n < SySetUsed(&aIface) ; n++ ){` |
|    594 | 11007 | `			if( n > 0 ){` |
|    361 | 11008 | `				SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|    180 | 11009 | `			}` |
|    594 | 11010 | `			SyBlobFormat(pOut, "%z", &apIface[n]->sName);` |
|    298 | 11011 | `		}` |
|    116 | 11012 | `	}` |
|    314 | 11013 | `	SyBlobAppend(pOut, " ] {\n", sizeof(" ] {\n")-1);` |
|    314 | 11014 | `	if( !bInternal && SyStringLength(&pClass->sFile) > 0 ){` |
|     23 | 11015 | `		SyBlobAppend(pOut, "  @@ ", sizeof("  @@ ")-1);` |
|     23 | 11016 | `		SyBlobAppend(pOut, SyStringData(&pClass->sFile), SyStringLength(&pClass->sFile));` |
|     23 | 11017 | `		SyBlobFormat(pOut, " %u-%u\n", pClass->nLine, pClass->nEndLine);` |
|     11 | 11018 | `	}` |
|      - | 11019 | `	/* ---- counts ---- */` |
|   5698 | 11020 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   5386 | 11021 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   5386 | 11022 | `		if( pM->iKind == REFLECT_MEMBER_CONST ){` |
|    680 | 11023 | `			if( !(bEnum && (pM->pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)) ){` |
|    670 | 11024 | `				nConst++;` |
|    336 | 11025 | `			}` |
|   5047 | 11026 | `		}else if( pM->iKind == REFLECT_MEMBER_PROP ){` |
|    582 | 11027 | `			if( pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){ nStaticProp++; }else{ nProp++; }` |
|    292 | 11028 | `		}else{` |
|      - | 11029 | `			/* A FABRICATED method is absent from the class's own export, for the same` |
|      - | 11030 | `			 * reason get_class_methods() does not name it: php is printing its` |
|      - | 11031 | `			 * function table and this one is not in it. */` |
|   4128 | 11032 | `			if( pM->pMeth->iFlags & PH7_CLASS_ATTR_FABRICATED ){ continue; }` |
|   4126 | 11033 | `			if( pM->pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){ nStaticMeth++; }else{ nMeth++; }` |
|      - | 11034 | `		}` |
|   2693 | 11035 | `	}` |
|      - | 11036 | `	/* ---- enum cases, then constants ---- */` |
|    314 | 11037 | `	if( bEnum ){` |
|      7 | 11038 | `		rc = ReflectExportEnumCases(pCtx, pOut, pClass);` |
|      7 | 11039 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 11040 | `			goto done;` |
|      - | 11041 | `		}` |
|      3 | 11042 | `	}` |
|    314 | 11043 | `	SyBlobFormat(pOut, "\n  - Constants [%u] {\n", nConst);` |
|   5698 | 11044 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   5386 | 11045 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   5384 | 11046 | `		if( pM->iKind != REFLECT_MEMBER_CONST` |
|   3033 | 11047 | `		 \|\| (bEnum && (pM->pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)) ){` |
|   4718 | 11048 | `			continue;` |
|      - | 11049 | `		}` |
|    670 | 11050 | `		SyBlobAppend(pOut, "    ", sizeof("    ")-1);` |
|    670 | 11051 | `		rc = ReflectExportConstLine(pCtx, pOut, pClass, pM->pAttr, &pM->sKey);` |
|    670 | 11052 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 11053 | `			goto done;` |
|      - | 11054 | `		}` |
|    336 | 11055 | `	}` |
|    314 | 11056 | `	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|      - | 11057 | `	/* ---- properties and methods, statics first ---- */` |
|   1562 | 11058 | `	for( iPass = 0 ; iPass < 4 ; iPass++ ){` |
|   1250 | 11059 | `		int bStatic = (iPass == 0 \|\| iPass == 1);` |
|   1250 | 11060 | `		int bMethods = (iPass == 1 \|\| iPass == 3);` |
|   1250 | 11061 | `		int bFirst = 1;` |
|      - | 11062 | `		static const char *azTitle[] = {` |
|      - | 11063 | `			"\n  - Static properties [%u] {\n", "\n  - Static methods [%u] {\n",` |
|      - | 11064 | `			"\n  - Properties [%u] {\n", "\n  - Methods [%u] {\n"` |
|      - | 11065 | `		};` |
|      - | 11066 | `		sxu32 aCount[4];` |
|   1250 | 11067 | `		aCount[0] = nStaticProp;` |
|   1250 | 11068 | `		aCount[1] = nStaticMeth;` |
|   1250 | 11069 | `		aCount[2] = nProp;` |
|   1250 | 11070 | `		aCount[3] = nMeth;` |
|   1250 | 11071 | `		SyBlobFormat(pOut, azTitle[iPass], aCount[iPass]);` |
|  22786 | 11072 | `		for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|  21538 | 11073 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - | 11074 | `			int bIsStatic;` |
|  21538 | 11075 | `			if( pM->iKind == REFLECT_MEMBER_CONST ){` |
|   2714 | 11076 | `				continue;` |
|      - | 11077 | `			}` |
|  18826 | 11078 | `			if( bMethods != (pM->iKind == REFLECT_MEMBER_METHOD) ){` |
|   9414 | 11079 | `				continue;` |
|      - | 11080 | `			}` |
|   9414 | 11081 | `			if( bMethods && (pM->pMeth->iFlags & PH7_CLASS_ATTR_FABRICATED) ){` |
|      5 | 11082 | `				continue;   /* not in the function table this is printing */` |
|      - | 11083 | `			}` |
|  13534 | 11084 | `			bIsStatic = bMethods ? (pM->pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0` |
|   5284 | 11085 | `				: (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|   9410 | 11086 | `			if( bIsStatic != bStatic ){` |
|   4706 | 11087 | `				continue;` |
|      - | 11088 | `			}` |
|   4706 | 11089 | `			if( bMethods ){` |
|      - | 11090 | `				ReflectFuncRef sRef;` |
|   4126 | 11091 | `				if( !bFirst ){` |
|   3800 | 11092 | `					SyBlobAppend(pOut, "\n", sizeof(char));` |
|   1899 | 11093 | `				}` |
|   4126 | 11094 | `				bFirst = 0;` |
|   4126 | 11095 | `				ReflectFuncFromMethod(pCtx->pVm, pClass, pM->pMeth, &sRef);` |
|   4126 | 11096 | `				rc = ReflectExportFuncBlock(pCtx, pOut, &sRef, "    ", pClass);` |
|   4126 | 11097 | `				if( rc != SXRET_OK ){` |
|    ! 0 | 11098 | `					goto done;` |
|      - | 11099 | `				}` |
|   2064 | 11100 | `			}else{` |
|    582 | 11101 | `				SyBlobAppend(pOut, "    ", sizeof("    ")-1);` |
|    582 | 11102 | `				ReflectExportPropLine(pCtx, pOut, pM->pAttr, &pM->sKey);` |
|      - | 11103 | `			}` |
|   2354 | 11104 | `		}` |
|   1250 | 11105 | `		SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|    626 | 11106 | `	}` |
|    314 | 11107 | `	SyBlobAppend(pOut, "}\n", sizeof("}\n")-1);` |
|    156 | 11108 | `done:` |
|    314 | 11109 | `	SySetRelease(&aMembers);` |
|    314 | 11110 | `	SySetRelease(&aIface);` |
|    314 | 11111 | `	return rc;` |
|      2 | 11112 | `}` |
|      - | 11113 | `/* ---- the five __toString() entry points ---- */` |
|     60 | 11114 | `static int ReflectExportClassSelf(ph7_context *pCtx)` |
|      2 | 11115 | `{` |
|     62 | 11116 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 11117 | `	SyBlob sOut;` |
|      - | 11118 | `	sxi32 rc;` |
|     62 | 11119 | `	if( pClass == 0 ){` |
|    ! 0 | 11120 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 11121 | `		return PH7_OK;` |
|      - | 11122 | `	}` |
|     62 | 11123 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     62 | 11124 | `	rc = ReflectExportClassBlock(pCtx, &sOut, pClass);` |
|     62 | 11125 | `	if( rc == SXRET_OK ){` |
|     62 | 11126 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     30 | 11127 | `	}` |
|     62 | 11128 | `	SyBlobRelease(&sOut);` |
|     62 | 11129 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|     32 | 11130 | `}` |
|      - | 11131 | `/* The standalone Function block for a name the extension walk handed over. */` |
|    250 | 11132 | `static void ReflectExportFuncByName(ph7_context *pCtx, SyBlob *pOut,` |
|      - | 11133 | `	const char *zName, int nName)` |
|      2 | 11134 | `{` |
|      - | 11135 | `	ph7_value sTarget;` |
|      - | 11136 | `	ReflectFuncRef sRef;` |
|    252 | 11137 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|    252 | 11138 | `	ph7_value_string(&sTarget, zName, nName);` |
|    252 | 11139 | `	if( ReflectFuncFill(pCtx->pVm, &sTarget, 0, &sRef) ){` |
|    252 | 11140 | `		ReflectExportFuncBlock(pCtx, pOut, &sRef, "", 0);` |
|    125 | 11141 | `	}` |
|    252 | 11142 | `	PH7_MemObjRelease(&sTarget);` |
|    252 | 11143 | `}` |
|    526 | 11144 | `static int ReflectExportFuncSelf(ph7_context *pCtx)` |
|      4 | 11145 | `{` |
|      - | 11146 | `	ReflectFuncRef sRef;` |
|      - | 11147 | `	SyBlob sOut;` |
|      - | 11148 | `	sxi32 rc;` |
|    530 | 11149 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 | 11150 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 11151 | `		return PH7_OK;` |
|      - | 11152 | `	}` |
|    530 | 11153 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    530 | 11154 | `	rc = ReflectExportFuncBlock(pCtx, &sOut, &sRef, "", ReflectOwnerOfThis(pCtx));` |
|    530 | 11155 | `	if( rc == SXRET_OK ){` |
|    530 | 11156 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    263 | 11157 | `	}` |
|    530 | 11158 | `	SyBlobRelease(&sOut);` |
|    530 | 11159 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    267 | 11160 | `}` |
|    170 | 11161 | `static int ReflectExportParamSelf(ph7_context *pCtx)` |
|      5 | 11162 | `{` |
|      - | 11163 | `	ReflectFuncRef sRef;` |
|      - | 11164 | `	ReflectParamDesc sDesc;` |
|      - | 11165 | `	SyBlob sOut;` |
|    175 | 11166 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) ){` |
|    ! 0 | 11167 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 11168 | `		return PH7_OK;` |
|      - | 11169 | `	}` |
|    175 | 11170 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    175 | 11171 | `	ReflectExportParamLine(pCtx, &sOut, &sDesc);` |
|    175 | 11172 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    175 | 11173 | `	SyBlobRelease(&sOut);` |
|    175 | 11174 | `	return PH7_OK;` |
|     90 | 11175 | `}` |
|     32 | 11176 | `static int ReflectExportPropSelf(ph7_context *pCtx)` |
|      3 | 11177 | `{` |
|      - | 11178 | `	ReflectMemberRef sRef;` |
|      - | 11179 | `	SyBlob sOut;` |
|      - | 11180 | `	SyString sKey;` |
|     35 | 11181 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 11182 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 11183 | `		return PH7_OK;` |
|      - | 11184 | `	}` |
|     35 | 11185 | `	SyStringInitFromBuf(&sKey, sRef.zName, sRef.nName);` |
|     35 | 11186 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     35 | 11187 | `	ReflectExportPropLine(pCtx, &sOut, sRef.pAttr, &sKey);` |
|     35 | 11188 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     35 | 11189 | `	SyBlobRelease(&sOut);` |
|     35 | 11190 | `	return PH7_OK;` |
|     19 | 11191 | `}` |
|      6 | 11192 | `static int ReflectExportConstSelf(ph7_context *pCtx)` |
|      1 | 11193 | `{` |
|      - | 11194 | `	ReflectMemberRef sRef;` |
|      - | 11195 | `	SyBlob sOut;` |
|      - | 11196 | `	SyString sKey;` |
|      - | 11197 | `	sxi32 rc;` |
|      7 | 11198 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 11199 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 11200 | `		return PH7_OK;` |
|      - | 11201 | `	}` |
|      7 | 11202 | `	SyStringInitFromBuf(&sKey, sRef.zName, sRef.nName);` |
|      7 | 11203 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|      7 | 11204 | `	rc = ReflectExportConstLine(pCtx, &sOut, sRef.pClass, sRef.pAttr, &sKey);` |
|      7 | 11205 | `	if( rc == SXRET_OK ){` |
|      7 | 11206 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|      3 | 11207 | `	}` |
|      7 | 11208 | `	SyBlobRelease(&sOut);` |
|      7 | 11209 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|      4 | 11210 | `}` |
|      - | 11211 | `/*` |
|      - | 11212 | ` * Install the Reflection API — ~25 classes, all of them declared from C.` |
|      - | 11213 | ` *` |
|      - | 11214 | `` * There is no global thunk table left to register: every `__reflect_*` and`` |
|      - | 11215 | `` * `__phl_rcinfo` became a method of the class that always owned it, so`` |
|      - | 11216 | ` * Reflection adds no name to php's global function namespace at all. Called` |
|      - | 11217 | ` * from PH7_VmInit while pVm->bCompilingBuiltin is set, right after the core` |
|      - | 11218 | ` * builtin chunks (Exception and friends have to exist already).` |
|      - | 11219 | ` */` |
|   8445 | 11220 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|      5 | 11221 | `{` |
|      - | 11222 | `	sxi32 rc;` |
|      - | 11223 | `	/* Where chunk 1 was: Reflector, Reflection, ReflectionException,` |
|      - | 11224 | `	 * ReflectionClass and ReflectionObject are native now. Chunks 2 and 3 name` |
|      - | 11225 | ``	 * `Reflector` in their own `implements` clauses, so this has to run first. */`` |
|   8450 | 11226 | `	rc = PH7_VmInstallReflectionClass(&(*pVm));` |
|   8450 | 11227 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 11228 | `		return rc;` |
|      - | 11229 | `	}` |
|      - | 11230 | `	/* Where chunk 2 was: ReflectionFunctionAbstract, ReflectionFunction,` |
|      - | 11231 | `	 * ReflectionMethod and ReflectionParameter are native now. */` |
|   8450 | 11232 | `	rc = PH7_VmInstallReflectionFunc(&(*pVm));` |
|   8450 | 11233 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 11234 | `		return rc;` |
|      - | 11235 | `	}` |
|   8450 | 11236 | `	rc = PH7_VmInstallReflectionHookType(&(*pVm));` |
|   8450 | 11237 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 11238 | `		return rc;` |
|      - | 11239 | `	}` |
|      - | 11240 | `	/* Where chunk 3 was: ReflectionProperty and ReflectionClassConstant are` |
|      - | 11241 | `	 * native now, and PropertyHookType is a native ENUM. */` |
|   8450 | 11242 | `	rc = PH7_VmInstallReflectionMember(&(*pVm));` |
|   8450 | 11243 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 11244 | `		return rc;` |
|      - | 11245 | `	}` |
|      - | 11246 | `	/* Where chunk 4 was: the four type classes are native now (see` |
|      - | 11247 | `	 * PH7_VmInstallReflectionTypes). Stringable exists by this point. */` |
|   8450 | 11248 | `	rc = PH7_VmInstallReflectionTypes(&(*pVm));` |
|   8450 | 11249 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 11250 | `		return rc;` |
|      - | 11251 | `	}` |
|      - | 11252 | `	/* Where chunk 5 was: ReflectionGenerator/Fiber and the four standalone` |
|      - | 11253 | `	 * classes chunk 6 used to hold are native now. Reflector (chunk 1) exists. */` |
|   8450 | 11254 | `	rc = PH7_VmInstallReflectionSmall(&(*pVm));` |
|   8450 | 11255 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 11256 | `		return rc;` |
|      - | 11257 | `	}` |
|      - | 11258 | `	/* Where chunk 6 was: the three ReflectionEnum classes are native now, and` |
|      - | 11259 | `	 * the class DESCRIPTOR they read (__phl_rcinfo) has no callers left. */` |
|   8450 | 11260 | `	rc = PH7_VmInstallReflectionEnum(&(*pVm));` |
|   8450 | 11261 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 11262 | `		return rc;` |
|      - | 11263 | `	}` |
|      - | 11264 | `	/* Where chunk 7 was: ReflectionAttribute is native now, and with it the` |
|      - | 11265 | `	 * builder (__reflect_build_attrs) and the two helpers it needed. */` |
|   8450 | 11266 | `	rc = PH7_VmInstallReflectionAttribute(&(*pVm));` |
|   8450 | 11267 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 11268 | `		return rc;` |
|      - | 11269 | `	}` |
|      - | 11270 | `	/* Where chunk 9 was: the export format is ReflectExportClassSelf() and its` |
|      - | 11271 | `	 * four siblings above. Nothing of the Reflection library is PHP any more,` |
|      - | 11272 | `	 * so vm_builtin_reflection_lib.c is gone and this IS the installer. */` |
|   8450 | 11273 | `	return SXRET_OK;` |
|   4222 | 11274 | `}` |
|      - | 11275 |  |
