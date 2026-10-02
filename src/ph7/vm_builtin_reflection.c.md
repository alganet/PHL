# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 6045/6828 lines (88.53%)

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
|      - |    27 | ` * Resolve a class-name string or object into a ph7_class pointer,` |
|      - |    28 | ` * triggering autoload for unknown string names. Returns NULL when the` |
|      - |    29 | ` * class does not exist (the PHP layer turns that into ReflectionException).` |
|      - |    30 | ` */` |
|   6607 |    31 | `static ph7_class * ReflectResolveClass(ph7_vm *pVm, ph7_value *pArg)` |
|      5 |    32 | `{` |
|      - |    33 | `	ph7_class *pClass;` |
|   6612 |    34 | `	pClass = PH7_VmExtractClassFromValue(pVm, pArg);` |
|   6612 |    35 | `	if( pClass == 0 && ph7_value_is_string(pArg) ){` |
|      - |    36 | `		const char *zName;` |
|      - |    37 | `		int nLen;` |
|     20 |    38 | `		zName = ph7_value_to_string(pArg, &nLen);` |
|     20 |    39 | `		if( nLen > 0 ){` |
|     20 |    40 | `			pClass = PH7_VmTriggerAutoload(pVm, zName, (sxu32)nLen, FALSE);` |
|      9 |    41 | `		}` |
|      9 |    42 | `	}` |
|   6612 |    43 | `	return pClass;` |
|      5 |    44 | `}` |
|      - |    45 | `/*` |
|      - |    46 | ` * Hand a freshly created class instance to the caller. The return slot` |
|      - |    47 | ` * takes over the initial reference from PH7_NewClassInstance (iRef=1):` |
|      - |    48 | ` * no extra iRef++ here (see the synthesized-object invariant — a stray` |
|      - |    49 | ` * bump leaks the object and disables its __destruct).` |
|      - |    50 | ` */` |
|   1955 |    51 | `static int ReflectResultObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|      5 |    52 | `{` |
|   1960 |    53 | `	if( pObj == 0 ){` |
|    ! 0 |    54 | `		ph7_result_null(pCtx);` |
|    ! 0 |    55 | `		return PH7_OK;` |
|      - |    56 | `	}` |
|   1960 |    57 | `	PH7_MemObjRelease(pCtx->pRet);` |
|   1960 |    58 | `	pCtx->pRet->x.pOther = pObj;` |
|   1960 |    59 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|   1960 |    60 | `	return PH7_OK;` |
|    981 |    61 | `}` |
|      - |    62 | `/* The last of the descriptor marshalling: ReflectMapAddDyn survives because` |
|      - |    63 | ` * ReflectAttrArgs() answers a php ARRAY whose named arguments are string keys.` |
|      - |    64 | ` * Its Bool/Int/Str/Null/Attrs/Doc siblings went with __phl_rcinfo. */` |
|      - |    65 | `/* Add an entry under a dynamic (SyString) key. */` |
|    172 |    66 | `static void ReflectMapAddDyn(ph7_context *pCtx, ph7_value *pMap,` |
|      - |    67 | `	const SyString *pKey, ph7_value *pVal)` |
|      2 |    68 | `{` |
|    174 |    69 | `	ph7_value *pK = ph7_context_new_scalar(pCtx);` |
|    174 |    70 | `	if( pK == 0 ){ return; }` |
|    174 |    71 | `	ph7_value_string(pK, pKey->zString, (int)pKey->nByte);` |
|    174 |    72 | `	ph7_array_add_elem(pMap, pK, pVal);` |
|     74 |    73 | `}` |
|      - |    74 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth);` |
|      - |    75 | `/* Append pIface to the set unless it is already there (dedup by pointer). */` |
|   2553 |    76 | `static void ReflectIfaceAppend(SySet *pOut, ph7_class *pIface)` |
|      4 |    77 | `{` |
|      - |    78 | `	ph7_class **apKnown;` |
|      - |    79 | `	sxu32 n;` |
|   2557 |    80 | `	if( pIface == 0 ){` |
|    ! 0 |    81 | `		return;` |
|      - |    82 | `	}` |
|   2557 |    83 | `	apKnown = (ph7_class **)SySetBasePtr(pOut);` |
|   4909 |    84 | `	for( n = 0 ; n < SySetUsed(pOut) ; n++ ){` |
|   2617 |    85 | `		if( apKnown[n] == pIface ){` |
|    265 |    86 | `			return;` |
|      - |    87 | `		}` |
|   1026 |    88 | `	}` |
|   2295 |    89 | `	SySetPut(pOut, (const void *)&pIface);` |
|   1174 |    90 | `}` |
|      - |    91 | `static void ReflectFlattenIfaces(ph7_vm *pVm, ph7_class *pClass, SySet *pOut, int iDepth);` |
|      - |    92 | `/* Append pIface's OWN flattened list, BACKWARDS — see ReflectFlattenIfaces. */` |
|   1075 |    93 | `static void ReflectIfaceAppendOwn(ph7_vm *pVm, SySet *pOut, ph7_class *pIface, int iDepth)` |
|      4 |    94 | `{` |
|      - |    95 | `	SySet aSub;` |
|      - |    96 | `	ph7_class **ap;` |
|      - |    97 | `	sxu32 n;` |
|   1079 |    98 | `	SySetInit(&aSub, pOut->pAllocator, sizeof(ph7_class *));` |
|   1079 |    99 | `	ReflectFlattenIfaces(pVm, pIface, &aSub, iDepth + 1);` |
|   1079 |   100 | `	ap = (ph7_class **)SySetBasePtr(&aSub);` |
|   1642 |   101 | `	for( n = SySetUsed(&aSub) ; n > 0 ; n-- ){` |
|    566 |   102 | `		ReflectIfaceAppend(pOut, ap[n-1]);` |
|    265 |   103 | `	}` |
|   1079 |   104 | `	SySetRelease(&aSub);` |
|   1079 |   105 | `}` |
|      - |   106 | `/*` |
|      - |   107 | ` * The interface list php reports for pClass, IN PHP'S ORDER — the one` |
|      - |   108 | `` * `getInterfaceNames()`, `getInterfaces()`, `class_implements()` and the`` |
|      - |   109 | `` * export's `implements` / `extends` clause all publish. Caller owns the set`` |
|      - |   110 | ` * (SySetInit with sizeof(ph7_class *)).` |
|      - |   111 | ` *` |
|      - |   112 | ` * zend builds it once at link time out of two primitives, and every ordering` |
|      - |   113 | ` * rule below is one of them (all measured against php 8.5.9, 271 internal` |
|      - |   114 | ` * classes and interfaces agreeing row for row):` |
|      - |   115 | ` *` |
|      - |   116 | `` *   - `zend_do_inherit_interfaces` copies a list in BACKWARDS. That is what`` |
|      - |   117 | ``  *     puts `IteratorIterator` before its own `Traversable` and `Iterator` `` |
|      - |   118 | `` *     (`RecursiveIteratorIterator` reads `OuterIterator, Traversable,`` |
|      - |   119 | ``  *     Iterator`), and what makes `ErrorException` list `Throwable, Stringable` `` |
|      - |   120 | `` *     where its parent `Exception` lists them the other way round.`` |
|      - |   121 | ` *   - An INTERNAL class hands its interfaces over ONE AT A TIME` |
|      - |   122 | `` *     (`zend_class_implements`), so each is followed immediately by its own`` |
|      - |   123 | ` *     list; a compiled one declares them as a BLOCK, so all the declared ones` |
|      - |   124 | `` *     come first and the inherited ones after. `ArrayObject` is the first`` |
|      - |   125 | `` *     shape, `class Z implements P1, P2` the second.`` |
|      - |   126 | ` *   - The parent's list opens the answer, backwards for an internal class and` |
|      - |   127 | ` *     for a compiled one that declares NO interface of its own (which is the` |
|      - |   128 | `` *     `zend_do_inherit_interfaces` path again), forwards otherwise.`` |
|      - |   129 | ` *` |
|      - |   130 | ` * This engine walked a flattened set of its own and matched php on none of` |
|      - |   131 | ` * those: 71 of the 197 classes both engines share answered a different order.` |
|      - |   132 | ` */` |
|   1873 |   133 | `static void ReflectFlattenIfaces(ph7_vm *pVm, ph7_class *pClass, SySet *pOut, int iDepth)` |
|      4 |   134 | `{` |
|      - |   135 | `	SySet aDecl;` |
|      - |   136 | `	ph7_class **ap;` |
|      - |   137 | `	sxu32 n, nDecl;` |
|      - |   138 | `	int bInternal, bIface;` |
|   1877 |   139 | `	if( pClass == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|    ! 0 |   140 | `		return;` |
|      - |   141 | `	}` |
|   1877 |   142 | `	bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;` |
|   1877 |   143 | `	bIface = (pClass->iFlags & PH7_CLASS_INTERFACE) != 0;` |
|      - |   144 | `	/*` |
|      - |   145 | `	 * php auto-implements Stringable for an INTERNAL class that declares its` |
|      - |   146 | `	 * own __toString, and does it while REGISTERING the class -- before the` |
|      - |   147 | `	 * parent's interfaces are inherited and before its own are named. That is` |
|      - |   148 | ``	 * the whole reason `CachingIterator` opens with Stringable and its`` |
|      - |   149 | `	 * subclass, which only inherits the method, does not. A compiled class` |
|      - |   150 | `	 * gets the same auto-implement at the END of its declared list instead,` |
|      - |   151 | `	 * which compile_class.c already spells.` |
|      - |   152 | `	 */` |
|   1877 |   153 | `	if( bInternal && !bIface ){` |
|    602 |   154 | `		SyHashEntry *pTs = SyHashGet(&pClass->hMethod,` |
|      - |   155 | `			(const void *)"__toString", sizeof("__toString")-1);` |
|    797 |   156 | `		if( pTs && ReflectMethodDeclClass(pClass,` |
|    501 |   157 | `				(ph7_class_method *)pTs->pUserData) == pClass ){` |
|    191 |   158 | `			ReflectIfaceAppend(pOut, PH7_VmExtractClass(pVm,` |
|      - |   159 | `				"Stringable", sizeof("Stringable")-1, FALSE, 0));` |
|     85 |   160 | `		}` |
|    278 |   161 | `	}` |
|      - |   162 | `	/* What the class DECLARES, in declaration order. PHL keeps the first` |
|      - |   163 | ``	 * parent of an `interface B extends A, C` on the base chain and the rest`` |
|      - |   164 | `	 * in aInterface; php keeps no base chain for an interface at all. */` |
|   1877 |   165 | `	SySetInit(&aDecl, pOut->pAllocator, sizeof(ph7_class *));` |
|   1877 |   166 | `	if( bIface && pClass->pBase ){` |
|    492 |   167 | `		SySetPut(&aDecl, (const void *)&pClass->pBase);` |
|    231 |   168 | `	}` |
|   1877 |   169 | `	ap = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   2463 |   170 | `	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){` |
|    590 |   171 | `		SySetPut(&aDecl, (const void *)&ap[n]);` |
|    279 |   172 | `	}` |
|   1877 |   173 | `	nDecl = SySetUsed(&aDecl);` |
|      - |   174 | `	/* The parent class's own list opens the answer. */` |
|   1877 |   175 | `	if( !bIface && pClass->pBase ){` |
|      - |   176 | `		SySet aParent;` |
|    311 |   177 | `		SySetInit(&aParent, pOut->pAllocator, sizeof(ph7_class *));` |
|    311 |   178 | `		ReflectFlattenIfaces(pVm, pClass->pBase, &aParent, iDepth + 1);` |
|    311 |   179 | `		ap = (ph7_class **)SySetBasePtr(&aParent);` |
|    311 |   180 | `		if( bInternal \|\| nDecl == 0 ){` |
|   1028 |   181 | `			for( n = SySetUsed(&aParent) ; n > 0 ; n-- ){` |
|    726 |   182 | `				ReflectIfaceAppend(pOut, ap[n-1]);` |
|    318 |   183 | `			}` |
|    139 |   184 | `		}else{` |
|     11 |   185 | `			for( n = 0 ; n < SySetUsed(&aParent) ; n++ ){` |
|      5 |   186 | `				ReflectIfaceAppend(pOut, ap[n]);` |
|      3 |   187 | `			}` |
|      - |   188 | `		}` |
|    311 |   189 | `		SySetRelease(&aParent);` |
|    139 |   190 | `	}` |
|   1877 |   191 | `	ap = (ph7_class **)SySetBasePtr(&aDecl);` |
|   1877 |   192 | `	if( bInternal ){` |
|   2588 |   193 | `		for( n = 0 ; n < nDecl ; n++ ){` |
|    943 |   194 | `			ReflectIfaceAppend(pOut, ap[n]);` |
|    943 |   195 | `			ReflectIfaceAppendOwn(pVm, pOut, ap[n], iDepth);` |
|    442 |   196 | `		}` |
|    774 |   197 | `	}else{` |
|    366 |   198 | `		for( n = 0 ; n < nDecl ; n++ ){` |
|    138 |   199 | `			ReflectIfaceAppend(pOut, ap[n]);` |
|     70 |   200 | `		}` |
|    366 |   201 | `		for( n = 0 ; n < nDecl ; n++ ){` |
|    138 |   202 | `			ReflectIfaceAppendOwn(pVm, pOut, ap[n], iDepth);` |
|     70 |   203 | `		}` |
|      - |   204 | `	}` |
|   1877 |   205 | `	SySetRelease(&aDecl);` |
|    888 |   206 | `}` |
|      - |   207 | ``/* The list every Reflection door asks for -- and `class_implements()`, which is`` |
|      - |   208 | ` * the same answer under another name (vm_builtin_class.c). */` |
|    490 |   209 | `PH7_PRIVATE void PH7_ReflectInterfacesOf(ph7_vm *pVm, ph7_class *pClass, SySet *pOut)` |
|      4 |   210 | `{` |
|    494 |   211 | `	ReflectFlattenIfaces(pVm, pClass, pOut, 0);` |
|    494 |   212 | `}` |
|      - |   213 | `/*` |
|      - |   214 | ` * Deepest base class whose method table maps the same name to the very` |
|      - |   215 | ` * same ph7_class_method pointer: inheritance shares member pointers` |
|      - |   216 | ` * (PH7_ClassInherit), so this identifies the declaring class. Methods` |
|      - |   217 | ` * copied in from traits are not on the pBase chain and thus report the` |
|      - |   218 | ` * using class, which is what PHP reports too.` |
|      - |   219 | ` */` |
|  80125 |   220 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth)` |
|      5 |   221 | `{` |
|  80130 |   222 | `	ph7_class *pDecl = pClass;` |
|  80130 |   223 | `	ph7_class *pBase = pClass->pBase;` |
|  80130 |   224 | `	int iDepth = 0;` |
| 121947 |   225 | `	while( pBase && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|      - |   226 | `		SyHashEntry *pEntry;` |
|  80788 |   227 | `		pEntry = SyHashGet(&pBase->hMethod, (const void *)SyStringData(&pMeth->sFunc.sName),` |
|  26599 |   228 | `			SyStringLength(&pMeth->sFunc.sName));` |
|  54189 |   229 | `		if( pEntry == 0 \|\| (ph7_class_method *)pEntry->pUserData != pMeth ){` |
|   6086 |   230 | `			break;` |
|      - |   231 | `		}` |
|  41820 |   232 | `		pDecl = pBase;` |
|  41820 |   233 | `		pBase = pBase->pBase;` |
|  41820 |   234 | `		iDepth++;` |
|      3 |   235 | `	}` |
|      - |   236 | `	/* A record the class only got from an INTERFACE belongs to the interface:` |
|      - |   237 | ``	 * php's scope for `ReflectionMethod('AbstractImpl','ifaceMethod')` is the`` |
|      - |   238 | `	 * interface that declared it, and the base walk above cannot see one` |
|      - |   239 | `	 * (an interface is not on the pBase chain of the class implementing it). */` |
|  80130 |   240 | `	if( pDecl->iFlags & PH7_CLASS_INTERFACE ){` |
|   5099 |   241 | `		return pDecl;` |
|      - |   242 | `	}` |
|      - |   243 | `	{` |
|  75034 |   244 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&pDecl->aInterface);` |
|      - |   245 | `		sxu32 n;` |
| 134395 |   246 | `		for( n = 0 ; n < SySetUsed(&pDecl->aInterface) ; n++ ){` |
|  88862 |   247 | `			SyHashEntry *pEntry = SyHashGet(&apIface[n]->hMethod,` |
|  59399 |   248 | `				(const void *)SyStringData(&pMeth->sFunc.sName),` |
|  29458 |   249 | `				SyStringLength(&pMeth->sFunc.sName));` |
|  59404 |   250 | `			if( pEntry && (ph7_class_method *)pEntry->pUserData == pMeth ){` |
|     39 |   251 | `				return ReflectMethodDeclClass(apIface[n], pMeth);` |
|      - |   252 | `			}` |
|  29444 |   253 | `		}` |
|      - |   254 | `	}` |
|  74996 |   255 | `	return pDecl;` |
|  39881 |   256 | `}` |
|      - |   257 | `/* Fetch a class attribute (property or constant) by plain name. */` |
|      4 |   258 | `static ph7_class_attr * ReflectFetchAttr(ph7_class *pClass, ph7_value *pName)` |
|      1 |   259 | `{` |
|      - |   260 | `	SyHashEntry *pEntry;` |
|      - |   261 | `	const char *zName;` |
|      - |   262 | `	int nLen;` |
|      5 |   263 | `	zName = ph7_value_to_string(pName, &nLen);` |
|      5 |   264 | `	if( nLen < 1 ){` |
|    ! 0 |   265 | `		return 0;` |
|      - |   266 | `	}` |
|      5 |   267 | `	pEntry = SyHashGet(&pClass->hAttr, (const void *)zName, (sxu32)nLen);` |
|      5 |   268 | `	if( pEntry == 0 ){` |
|      3 |   269 | `		return 0;` |
|      - |   270 | `	}` |
|      3 |   271 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|      3 |   272 | `}` |
|      - |   273 | `/* Fetch a class CONSTANT (or enum case) by name from the hConst namespace. */` |
|      2 |   274 | `static ph7_class_attr * ReflectFetchConst(ph7_class *pClass, ph7_value *pName)` |
|      1 |   275 | `{` |
|      - |   276 | `	const char *zName;` |
|      - |   277 | `	int nLen;` |
|      3 |   278 | `	zName = ph7_value_to_string(pName, &nLen);` |
|      3 |   279 | `	if( nLen < 1 ){` |
|    ! 0 |   280 | `		return 0;` |
|      - |   281 | `	}` |
|      3 |   282 | `	return PH7_ClassExtractConstant(pClass, zName, (sxu32)nLen);` |
|      2 |   283 | `}` |
|      - |   284 | `/* Fetch a member by name from EITHER namespace (property then constant) — used` |
|      - |   285 | ` * where the caller reflects over a member that may be either (e.g. attributes` |
|      - |   286 | ` * attached to a property or a constant). */` |
|      4 |   287 | `static ph7_class_attr * ReflectFetchMember(ph7_class *pClass, ph7_value *pName)` |
|      1 |   288 | `{` |
|      5 |   289 | `	ph7_class_attr *pAttr = ReflectFetchAttr(pClass, pName);` |
|      5 |   290 | `	if( pAttr == 0 ){` |
|      3 |   291 | `		pAttr = ReflectFetchConst(pClass, pName);` |
|      1 |   292 | `	}` |
|      5 |   293 | `	return pAttr;` |
|      1 |   294 | `}` |
|      - |   295 | `/*` |
|      - |   296 | ` * ---------------------------------------------------------------------------` |
|      - |   297 | ` * The member walk.` |
|      - |   298 | ` *` |
|      - |   299 | ` * php reports a class's members in ONE order — the class's own first (in` |
|      - |   300 | ` * declaration order), then each inheritance level's, outward — and every` |
|      - |   301 | ` * accessor that lists or looks one up has to agree with it. It used to live` |
|      - |   302 | ` * inside the descriptor builder alone; the native ReflectionClass needs the` |
|      - |   303 | ` * same order for getMethods()/getProperties()/getReflectionConstants() and the` |
|      - |   304 | ` * same visibility filtering for hasMethod()/getMethod()/getConstructor(), so` |
|      - |   305 | ` * the walk is factored out here and every one of them drives it.` |
|      - |   306 | ` *` |
|      - |   307 | ` * Per level the DECLARING class's own hash is iterated — a subclass hash` |
|      - |   308 | ` * interleaves inherited pointers unpredictably — and a pointer-identity lookup` |
|      - |   309 | ` * in the reflected class's hash drops what is not visible there (overridden` |
|      - |   310 | ` * entries). Methods come out reversed because hMethod is still a head-insert` |
|      - |   311 | ` * table, while hAttr/hConst insert at the tail.` |
|      - |   312 | ` * ---------------------------------------------------------------------------` |
|      - |   313 | ` */` |
|      - |   314 | `#define REFLECT_MEMBER_PROP   0` |
|      - |   315 | `#define REFLECT_MEMBER_CONST  1` |
|      - |   316 | `#define REFLECT_MEMBER_METHOD 2` |
|      - |   317 |  |
|      - |   318 | `typedef struct ReflectMember ReflectMember;` |
|      - |   319 | `struct ReflectMember` |
|      - |   320 | `{` |
|      - |   321 | `	int iKind;               /* REFLECT_MEMBER_* */` |
|      - |   322 | `	SyString sKey;           /* the name php reports it under: a trait` |
|      - |   323 | ``	                          * `use T { m as n; }` alias differs from the`` |
|      - |   324 | `	                          * method's own sFunc.sName, and php reports n */` |
|      - |   325 | `	ph7_class *pDecl;        /* declaring class */` |
|      - |   326 | `	ph7_class_attr *pAttr;   /* property or constant (NULL for a method) */` |
|      - |   327 | `	ph7_class_method *pMeth; /* method (NULL for a property or constant) */` |
|      - |   328 | `};` |
|      - |   329 | `/*` |
|      - |   330 | ` * Collect the members php would report for pClass, in php's own order, into a` |
|      - |   331 | ` * SySet of ReflectMember. The caller owns the set (SySetInit with` |
|      - |   332 | ` * sizeof(ReflectMember) / SySetRelease).` |
|      - |   333 | ` *` |
|      - |   334 | ` * bLookup selects which of php's TWO answers is wanted. The LISTING` |
|      - |   335 | ` * (getMethods()/getProperties()/getReflectionConstants(), bLookup = 0) hides a` |
|      - |   336 | ` * base class's private members; the LOOKUP (bLookup = 1) does not, because php` |
|      - |   337 | ` * keeps a parent's private in the child's tables and ReflectionMethod resolves` |
|      - |   338 | ` * against those. The two really do disagree: hasMethod('basePriv') is true on` |
|      - |   339 | ` * the subclass while getMethods() never mentions it.` |
|      - |   340 | ` */` |
|   2463 |   341 | `static void ReflectMembers(ph7_vm *pVm, ph7_class *pClass, SySet *pOut, int bLookup)` |
|      5 |   342 | `{` |
|      - |   343 | `	ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];` |
|   2468 |   344 | `	ph7_class *pWalk = pClass;` |
|      - |   345 | `	SyHashEntry *pEntry;` |
|      - |   346 | `	SySet aTmp;` |
|   2468 |   347 | `	int bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;` |
|      - |   348 | `	int iPart;` |
|   2468 |   349 | `	sxu32 nChain = 0, iLevel, nLev, nT;` |
|   5537 |   350 | `	while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){` |
|   3074 |   351 | `		aChain[nChain++] = pWalk;` |
|   3074 |   352 | `		pWalk = pWalk->pBase;` |
|      5 |   353 | `	}` |
|   2468 |   354 | `	SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));` |
|      - |   355 | `	/* PROPERTIES of an INTERNAL class come out base-first, and that is php's` |
|      - |   356 | `	 * registration order rather than a rule of its own: a user class declares its` |
|      - |   357 | ``	 * own properties and THEN inherits (`class B extends A` reports B's before`` |
|      - |   358 | `	 * A's), while an internal one is registered against its parent and declares` |
|      - |   359 | `	 * afterwards — so ErrorException reports Exception's five and then its own` |
|      - |   360 | `	 * $severity. Only the property table flips: php lists an internal class's` |
|      - |   361 | `	 * METHODS and CONSTANTS own-first like everything else, so the walk below` |
|      - |   362 | `	 * runs the two tables in opposite level orders. */` |
|   9857 |   363 | `	for( iPart = 0 ; iPart < 3 ; iPart++ ){` |
|  16601 |   364 | `	  for( nLev = 0 ; nLev < nChain ; nLev++ ){` |
|      - |   365 | `		ph7_class *pLevel;` |
|   9212 |   366 | `		int iTab = iPart;` |
|   9212 |   367 | `		iLevel = (iPart == 0 && bInternal) ? nChain - 1 - nLev : nLev;` |
|   9212 |   368 | `		pLevel = aChain[iLevel];` |
|      - |   369 | `		/* --- Properties (hAttr, iPart 0) and constants/enum cases (hConst,` |
|      - |   370 | `		 * iPart 1) — php's two separate member namespaces. Each table is` |
|      - |   371 | `		 * collected and emitted independently; the CONSTANT flag still decides` |
|      - |   372 | `		 * which kind comes out. --- */` |
|   9212 |   373 | `		if( iPart < 2 ){` |
|   6143 |   374 | `			SyHash *pSrcHash = iTab ? &pLevel->hConst : &pLevel->hAttr;` |
|   6143 |   375 | `			SyHash *pRefHash = iTab ? &pClass->hConst : &pClass->hAttr;` |
|   6143 |   376 | `			SySetReset(&aTmp);` |
|   6143 |   377 | `			SyHashResetLoopCursor(pSrcHash);` |
|  44900 |   378 | `			while( (pEntry = SyHashGetNextEntry(pSrcHash)) != 0 ){` |
|  38762 |   379 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  38762 |   380 | `				ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pLevel);` |
|  38762 |   381 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_HIDDEN ){` |
|      - |   382 | `					/* A native class's engine slot: php holds that state in its own C` |
|      - |   383 | `					 * struct and reports no property for it at all. */` |
|   4097 |   384 | `					continue;` |
|      - |   385 | `				}` |
|  34669 |   386 | `				if( iLevel == 0 ){` |
|      - |   387 | `					sxu32 j;` |
|      - |   388 | `					/* Own = declared here or by an off-chain provider (trait) */` |
|  31449 |   389 | `					for( j = 1 ; j < nChain ; j++ ){` |
|   7028 |   390 | `						if( aChain[j] == pDecl ){ break; }` |
|   1346 |   391 | `					}` |
|  28685 |   392 | `					if( j < nChain ){ continue; }` |
|  12206 |   393 | `				}else{` |
|      - |   394 | `					SyHashEntry *pSub;` |
|   5985 |   395 | `					if( pDecl != pLevel ){ continue; }` |
|      - |   396 | `					/* A base's PRIVATE member is not part of the subclass's` |
|      - |   397 | `					 * surface — php reports neither a private property nor a` |
|      - |   398 | `					 * private constant of a parent on the child. PHL's` |
|      - |   399 | `					 * inheritance copies them down all the same, so the filter` |
|      - |   400 | `					 * has to be here. */` |
|   4309 |   401 | `					if( !bLookup && pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){ continue; }` |
|      - |   402 | `					/* Must still be the visible member in the reflected class */` |
|   3985 |   403 | `					pSub = SyHashGet(pRefHash, pEntry->pKey, pEntry->nKeyLen);` |
|   3985 |   404 | `					if( pSub == 0 \|\| pSub->pUserData != (void *)pAttr ){ continue; }` |
|      - |   405 | `				}` |
|  28387 |   406 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|      5 |   407 | `			}` |
|      - |   408 | `			/* Forward: hAttr/hConst iterate in DECLARATION order (tail inserts),` |
|      - |   409 | `			 * so members come out in the order php reports them. */` |
|  34525 |   410 | `			for( nT = 0 ; nT < SySetUsed(&aTmp) ; nT++ ){` |
|  28387 |   411 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT);` |
|  28387 |   412 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;` |
|      - |   413 | `				ReflectMember sMember;` |
|  28387 |   414 | `				sMember.iKind = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|  14205 |   415 | `					? REFLECT_MEMBER_CONST : REFLECT_MEMBER_PROP;` |
|  28387 |   416 | `				sMember.sKey = pAttr->sName;` |
|  28387 |   417 | `				sMember.pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pLevel);` |
|  28387 |   418 | `				sMember.pAttr = pAttr;` |
|  28387 |   419 | `				sMember.pMeth = 0;` |
|  28387 |   420 | `				SySetPut(pOut, (const void *)&sMember);` |
|  14182 |   421 | `			}` |
|   6143 |   422 | `			continue;` |
|      - |   423 | `		}` |
|      - |   424 | `		/* --- Methods. The reported name is the hash-entry KEY, not the` |
|      - |   425 | `		 * function's own name (see ReflectMember::sKey). --- */` |
|   3074 |   426 | `		SySetReset(&aTmp);` |
|   3074 |   427 | `		SyHashResetLoopCursor(&pLevel->hMethod);` |
|  38126 |   428 | `		while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){` |
|  35057 |   429 | `			ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  35057 |   430 | `			ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|      - |   431 | `			/* A property HOOK is compiled into a method (__phl_hook_get_NAME /` |
|      - |   432 | ``			 * __phl_hook_set_NAME) so that inheritance and `parent::` work on`` |
|      - |   433 | `			 * it, but php has no method there at all: it reports the hook on` |
|      - |   434 | `			 * the PROPERTY. Listing it would put a PHL-only name on a` |
|      - |   435 | `			 * php-visible surface. */` |
|  35052 |   436 | `			if( pEntry->nKeyLen > sizeof("__phl_hook_")-1` |
|  23026 |   437 | `			 && SyMemcmp(pEntry->pKey, "__phl_hook_", sizeof("__phl_hook_")-1) == 0 ){` |
|    713 |   438 | `				continue;` |
|      - |   439 | `			}` |
|  34347 |   440 | `			if( iLevel == 0 ){` |
|      - |   441 | `				sxu32 j;` |
|  30185 |   442 | `				for( j = 1 ; j < nChain ; j++ ){` |
|  11244 |   443 | `					if( aChain[j] == pDecl ){ break; }` |
|   2360 |   444 | `				}` |
|  25160 |   445 | `				if( j < nChain ){ continue; }` |
|   9446 |   446 | `			}else{` |
|      - |   447 | `				SyHashEntry *pSub;` |
|   9189 |   448 | `				if( pDecl != pLevel ){ continue; }` |
|      - |   449 | `				/* Same rule as the members above: a base's PRIVATE method is` |
|      - |   450 | ``				 * not on the subclass's surface. `class B extends A` lists`` |
|      - |   451 | `				 * only A::q when A::p is private — php's inheritance never` |
|      - |   452 | `				 * hands the child a private, and PH7_ClassInherit's copy-down` |
|      - |   453 | `				 * does, so it is filtered here. */` |
|   6438 |   454 | `				if( !bLookup && pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){ continue; }` |
|   6310 |   455 | `				pSub = SyHashGet(&pClass->hMethod, pEntry->pKey, pEntry->nKeyLen);` |
|   6310 |   456 | `				if( pSub == 0 \|\| pSub->pUserData != (void *)pMeth ){` |
|      - |   457 | `					/* Overridden below this level: already reported */` |
|    221 |   458 | `					continue;` |
|      - |   459 | `				}` |
|      - |   460 | `			}` |
|  25032 |   461 | `			SySetPut(&aTmp, (const void *)&pEntry);` |
|      5 |   462 | `		}` |
|  28101 |   463 | `		for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|  25032 |   464 | `			SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|      - |   465 | `			ReflectMember sMember;` |
|  25032 |   466 | `			sMember.iKind = REFLECT_MEMBER_METHOD;` |
|  25032 |   467 | `			SyStringInitFromBuf(&sMember.sKey, (const char *)pE->pKey, pE->nKeyLen);` |
|  25032 |   468 | `			sMember.pMeth = (ph7_class_method *)pE->pUserData;` |
|  25032 |   469 | `			sMember.pDecl = ReflectMethodDeclClass(pClass, sMember.pMeth);` |
|  25032 |   470 | `			sMember.pAttr = 0;` |
|  25032 |   471 | `			SySetPut(pOut, (const void *)&sMember);` |
|  12474 |   472 | `		}` |
|   1537 |   473 | `	  }` |
|   3698 |   474 | `	}` |
|   2468 |   475 | `	SySetRelease(&aTmp);` |
|   2468 |   476 | `}` |
|      - |   477 | `/* Does a collected member name match zName exactly? */` |
|   9508 |   478 | `static int ReflectKeyIs(const ReflectMember *pM, const char *zName, int nName)` |
|      5 |   479 | `{` |
|  10986 |   480 | `	return SyStringLength(&pM->sKey) == (sxu32)nName` |
|   9508 |   481 | `		&& SyMemcmp(SyStringData(&pM->sKey), zName, (sxu32)nName) == 0;` |
|      5 |   482 | `}` |
|      - |   483 | `/*` |
|      - |   484 | ` * php's METHOD lookup, which is NOT the listing.` |
|      - |   485 | ` *` |
|      - |   486 | ` * getMethods() hides a base's private method, but hasMethod()/getMethod() find` |
|      - |   487 | ` * one: Zend keeps the parent's private in the child's function table and reads` |
|      - |   488 | ` * that table directly. PHL's inheritance copies methods down the same way, so` |
|      - |   489 | ` * the lookup is the class's own hMethod — case-insensitively, like php.` |
|      - |   490 | ` */` |
|  18968 |   491 | `static SyHashEntry * ReflectFindMethodEntry(ph7_class *pClass, const char *zName, int nName)` |
|      5 |   492 | `{` |
|      - |   493 | `	SyHashEntry *pEntry;` |
|  18973 |   494 | `	if( nName < 1 ){` |
|    ! 0 |   495 | `		return 0;` |
|      - |   496 | `	}` |
|  18973 |   497 | `	pEntry = SyHashGet(&pClass->hMethod, (const void *)zName, (sxu32)nName);` |
|  18973 |   498 | `	if( pEntry ){` |
|  12139 |   499 | `		return pEntry;` |
|      - |   500 | `	}` |
|   6838 |   501 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  44109 |   502 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  33854 |   503 | `		if( (int)pEntry->nKeyLen == nName` |
|  18377 |   504 | `		 && SyStrnicmp((const char *)pEntry->pKey, zName, (sxu32)nName) == 0 ){` |
|    ! 0 |   505 | `			return pEntry;` |
|      - |   506 | `		}` |
|      4 |   507 | `	}` |
|   6838 |   508 | `	return 0;` |
|   9489 |   509 | `}` |
|      - |   510 | `/*` |
|      - |   511 | `` * The visibility of the __construct / __clone `new` and `clone` would reach, or`` |
|      - |   512 | ` * 0 when the class has none — what isInstantiable() and isCloneable() screen on.` |
|      - |   513 | ` * The LOOKUP, not the listing: a class that inherits a private constructor is` |
|      - |   514 | ` * still not instantiable even though getMethods() does not report one.` |
|      - |   515 | ` */` |
|     92 |   516 | `static void ReflectCtorCloneVis(ph7_vm *pVm, ph7_class *pClass, sxi32 *piCtor, sxi32 *piClone)` |
|      5 |   517 | `{` |
|      - |   518 | `	ph7_class_method *pMeth;` |
|     46 |   519 | `	SXUNUSED(pVm);` |
|     97 |   520 | `	*piCtor = *piClone = 0;` |
|     97 |   521 | `	pMeth = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|     97 |   522 | `	if( pMeth ){` |
|     45 |   523 | `		*piCtor = pMeth->iProtection;` |
|     20 |   524 | `	}` |
|     97 |   525 | `	pMeth = PH7_ClassExtractMethod(pClass, "__clone", sizeof("__clone")-1);` |
|     97 |   526 | `	if( pMeth ){` |
|      7 |   527 | `		*piClone = pMeth->iProtection;` |
|      3 |   528 | `	}` |
|     97 |   529 | `}` |
|      - |   530 | `/* Where __phl_rcinfo() was: a full class DESCRIPTOR array — every constant,` |
|      - |   531 | ` * property and method of the whole inheritance chain, marshalled once and` |
|      - |   532 | ` * memoized on pVm->hClassInfo because the prelude classes rebuilt it once per` |
|      - |   533 | ` * member they constructed. Every one of its readers is a native class now and` |
|      - |   534 | ` * reads ph7_class directly, so the builder and the VM memo are both gone. */` |
|      - |   535 | `/*` |
|      - |   536 | ` * Collect a PHP array's values into a ph7_value* set (call arguments).` |
|      - |   537 | ` * When ppNames is non-NULL, string keys become named arguments: a name` |
|      - |   538 | ` * map is lazily allocated (like call_user_func_array's) with one entry` |
|      - |   539 | ` * per collected slot, empty entries meaning positional.` |
|      - |   540 | ` */` |
|     74 |   541 | `static sxi32 ReflectCollectArgs(ph7_context *pCtx, ph7_value *pArray, SySet *pOut, SyString **ppNames)` |
|      4 |   542 | `{` |
|      - |   543 | `	ph7_hashmap *pMap;` |
|      - |   544 | `	ph7_hashmap_node *pEntry;` |
|     78 |   545 | `	SyString *aNames = 0;` |
|     78 |   546 | `	sxu32 nSlot = 0;` |
|      - |   547 | `	sxu32 n;` |
|     78 |   548 | `	if( ppNames ){` |
|     65 |   549 | `		*ppNames = 0;` |
|     31 |   550 | `	}` |
|     78 |   551 | `	if( !ph7_value_is_array(pArray) ){` |
|    ! 0 |   552 | `		return SXRET_OK;` |
|      - |   553 | `	}` |
|     78 |   554 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     78 |   555 | `	pEntry = pMap->pFirst;` |
|    156 |   556 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     82 |   557 | `		ph7_value *pValue = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|     82 |   558 | `		if( pValue ){` |
|     82 |   559 | `			if( ppNames && pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      5 |   560 | `				if( aNames == 0 ){` |
|      7 |   561 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|      4 |   562 | `						pMap->nEntry * sizeof(SyString));` |
|      5 |   563 | `					if( aNames ){` |
|      5 |   564 | `						SyZero(aNames, pMap->nEntry * sizeof(SyString));` |
|      2 |   565 | `					}` |
|      2 |   566 | `				}` |
|      5 |   567 | `				if( aNames ){` |
|      5 |   568 | `					SyStringInitFromBuf(&aNames[nSlot],` |
|      - |   569 | `						SyBlobData(&pEntry->xKey.sKey), SyBlobLength(&pEntry->xKey.sKey));` |
|      2 |   570 | `				}` |
|      2 |   571 | `			}` |
|     82 |   572 | `			SySetPut(pOut, (const void *)&pValue);` |
|     82 |   573 | `			nSlot++;` |
|     39 |   574 | `		}` |
|     82 |   575 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     43 |   576 | `	}` |
|     78 |   577 | `	if( ppNames ){` |
|     65 |   578 | `		*ppNames = aNames;` |
|     31 |   579 | `	}` |
|     78 |   580 | `	return SXRET_OK;` |
|     41 |   581 | `}` |
|      - |   582 | `/*` |
|      - |   583 | ` * Instantiate pClassName and run its constructor over pArgs (a PHP array;` |
|      - |   584 | `` * string keys become NAMED arguments, which is how `#[Attr(x: 1)]` arrives).`` |
|      - |   585 | ` * The object lands in the call's result slot.` |
|      - |   586 | ` *` |
|      - |   587 | ` * ReflectionAttribute::newInstance()'s back end. It is deliberately NOT the` |
|      - |   588 | ` * ReflectionClass one (ReflectNewInstance, below): php runs no instantiability` |
|      - |   589 | ` * or constructor-visibility screen here — the attribute's own #[Attribute]` |
|      - |   590 | ` * declaration is what was checked — so an abstract or private-ctor attribute` |
|      - |   591 | ` * class reaches the engine's own Error, not a ReflectionException.` |
|      - |   592 | ` */` |
|     58 |   593 | `static sxi32 ReflectAttrInstantiate(ph7_context *pCtx, ph7_value *pClassName, ph7_value *pArgs)` |
|      4 |   594 | `{` |
|     62 |   595 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |   596 | `	ph7_class *pClass;` |
|      - |   597 | `	ph7_class_instance *pThis;` |
|      - |   598 | `	ph7_class_method *pCons;` |
|     62 |   599 | `	if( (pClass = ReflectResolveClass(pVm, pClassName)) == 0 ){` |
|    ! 0 |   600 | `		ph7_result_null(pCtx);` |
|    ! 0 |   601 | `		return PH7_OK;` |
|      - |   602 | `	}` |
|     62 |   603 | `	if( VmClassStaticDeferPending(pClass) ){` |
|      - |   604 | `		/* Instantiation materializes the static table (OP_NEW does it too), so a` |
|      - |   605 | `		 * broken default raises BEFORE any object exists. */` |
|    ! 0 |   606 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pVm, pClass);` |
|    ! 0 |   607 | `		if( rcMat != SXRET_OK ){` |
|    ! 0 |   608 | `			return rcMat;` |
|      - |   609 | `		}` |
|    ! 0 |   610 | `	}` |
|     62 |   611 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|     62 |   612 | `	if( pThis == 0 ){` |
|    ! 0 |   613 | `		ph7_result_null(pCtx);` |
|    ! 0 |   614 | `		return PH7_OK;` |
|      - |   615 | `	}` |
|     62 |   616 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|     62 |   617 | `	if( pCons ){` |
|      - |   618 | `		SySet aArg;` |
|      - |   619 | `		sxi32 rc;` |
|     55 |   620 | `		SyString *aNames = 0;` |
|     55 |   621 | `		SySetInit(&aArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|     55 |   622 | `		if( pArgs ){` |
|     55 |   623 | `			ReflectCollectArgs(pCtx, pArgs, &aArg, &aNames);` |
|     26 |   624 | `		}` |
|     55 |   625 | `		if( aNames ){` |
|      - |   626 | `			VmCallArgMap sMap;` |
|      5 |   627 | `			SyZero(&sMap,sizeof(sMap)); /* new map fields must read unset, not stack garbage */` |
|      5 |   628 | `			sMap.bHasNamed = 1;` |
|      5 |   629 | `			sMap.bIsNamespaced = 0;` |
|      5 |   630 | `			sMap.bStrict = 0;` |
|      5 |   631 | `			sMap.nTotal = SySetUsed(&aArg);` |
|      5 |   632 | `			sMap.aNames = aNames;` |
|      7 |   633 | `			rc = PH7_VmCallClassMethodMap(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|      4 |   634 | `				(ph7_value **)SySetBasePtr(&aArg), &sMap);` |
|      5 |   635 | `			SyMemBackendFree(&pVm->sAllocator, aNames);` |
|      3 |   636 | `		}else{` |
|     75 |   637 | `			rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|     48 |   638 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|      - |   639 | `		}` |
|     55 |   640 | `		SySetRelease(&aArg);` |
|     55 |   641 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|      3 |   642 | `			PH7_ClassInstanceCtorFailed(pThis);` |
|      3 |   643 | `			PH7_ClassInstanceUnref(pThis);` |
|      3 |   644 | `			return rc;` |
|      - |   645 | `		}` |
|     25 |   646 | `	}` |
|     60 |   647 | `	return ReflectResultObject(pCtx, pThis);` |
|     33 |   648 | `}` |
|      - |   649 | `/* Where __reflect_new_no_ctor() was: the one caller was chunk 7's` |
|      - |   650 | ` * __reflect_build_attrs, which built a ReflectionAttribute and then filled it` |
|      - |   651 | ` * through a public __init() php does not have. C builds the instance itself` |
|      - |   652 | ` * (ReflectAttrNew), so both are gone. */` |
|      - |   653 | `/*` |
|      - |   654 | ` * Typed/readonly store enforcement for reflection writes. Like the VM's` |
|      - |   655 | ` * store path, except an UNINITIALIZED readonly property may be written from` |
|      - |   656 | ` * any scope (PHP lets ReflectionProperty::setValue initialize readonly): the` |
|      - |   657 | ` * READONLY bit is masked off for the enforcement call so the set-scope check` |
|      - |   658 | ` * is skipped, while an already-initialized readonly still gets PHP's` |
|      - |   659 | ` * "Cannot modify readonly property" Error. Returns SXRET_OK/PH7_EXCEPTION/` |
|      - |   660 | ` * PH7_ABORT; the value may be coerced in place.` |
|      - |   661 | ` */` |
|     22 |   662 | `static sxi32 ReflectEnforceStore(ph7_context *pCtx, sxu32 nIdx, ph7_value *pValue)` |
|      1 |   663 | `{` |
|     23 |   664 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |   665 | `	SyHashEntry *pSlot;` |
|      - |   666 | `	VmClassAttr *pVmAttr;` |
|      - |   667 | `	ph7_class_attr *pAttr;` |
|      - |   668 | `	sxi32 iSaved, rc;` |
|     23 |   669 | `	pSlot = SyHashGet(&pVm->hTypedSlot, (const void *)&nIdx, sizeof(sxu32));` |
|     23 |   670 | `	if( pSlot == 0 ){` |
|     13 |   671 | `		return SXRET_OK; /* Untyped slot: plain store */` |
|      - |   672 | `	}` |
|     11 |   673 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     11 |   674 | `	pAttr = pVmAttr->pAttr;` |
|     11 |   675 | `	if( pAttr == 0 ){` |
|    ! 0 |   676 | `		return SXRET_OK;` |
|      - |   677 | `	}` |
|     11 |   678 | `	iSaved = pAttr->iFlags;` |
|     11 |   679 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) ){` |
|    ! 0 |   680 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_READONLY;` |
|    ! 0 |   681 | `	}` |
|     11 |   682 | `	rc = PH7_VmEnforcePropStore(pVm, nIdx, pValue);` |
|     11 |   683 | `	pAttr->iFlags = iSaved;` |
|     11 |   684 | `	return rc;` |
|     12 |   685 | `}` |
|      - |   686 | `/* Hand an EXISTING instance to the caller: takes an extra reference` |
|      - |   687 | ` * (unlike ReflectResultObject, which transfers a fresh instance's one). */` |
|      2 |   688 | `static int ReflectResultExistingObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|      1 |   689 | `{` |
|      3 |   690 | `	if( pObj == 0 ){` |
|    ! 0 |   691 | `		ph7_result_null(pCtx);` |
|    ! 0 |   692 | `		return PH7_OK;` |
|      - |   693 | `	}` |
|      3 |   694 | `	PH7_MemObjRelease(pCtx->pRet);` |
|      3 |   695 | `	pObj->iRef++;` |
|      3 |   696 | `	pCtx->pRet->x.pOther = pObj;` |
|      3 |   697 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|      3 |   698 | `	return PH7_OK;` |
|      2 |   699 | `}` |
|      - |   700 | `/* pVal is a Closure instance? Return it, else NULL. */` |
|   5582 |   701 | `static ph7_class_instance * ReflectValueClosure(ph7_vm *pVm, ph7_value *pVal)` |
|      5 |   702 | `{` |
|      - |   703 | `	ph7_class_instance *pThis;` |
|   5587 |   704 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
|   5061 |   705 | `		return 0;` |
|      - |   706 | `	}` |
|    529 |   707 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    529 |   708 | `	return (pThis->pClass == pVm->pClosureClass) ? pThis : 0;` |
|   2790 |   709 | `}` |
|      - |   710 | `/*` |
|      - |   711 | ` * Resolve a reflection callable target into its compiled function.` |
|      - |   712 | ` *   - pMethodArg a non-empty string  -> method mode: pTarget is a class name` |
|      - |   713 | ` *     or object; outputs *ppClass and *ppMeth.` |
|      - |   714 | ` *   - pTarget a Closure              -> unwrap $__fn into hFunction; *ppClosure.` |
|      - |   715 | ` *   - pTarget a string               -> hFunction (user) or hHostFunction` |
|      - |   716 | ` *     (*ppHost set, returns NULL).` |
|      - |   717 | ` * Returns the ph7_vm_func, or NULL (host function or unresolvable).` |
|      - |   718 | ` */` |
|   8118 |   719 | `static ph7_vm_func * ReflectResolveCallable(ph7_vm *pVm, ph7_value *pTarget,` |
|      - |   720 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|      - |   721 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|      5 |   722 | `{` |
|      - |   723 | `	SyHashEntry *pEntry;` |
|   8123 |   724 | `	if( ppClass ){ *ppClass = 0; }` |
|   8123 |   725 | `	if( ppMeth ){ *ppMeth = 0; }` |
|   8123 |   726 | `	if( ppHost ){ *ppHost = 0; }` |
|   8123 |   727 | `	if( ppClosure ){ *ppClosure = 0; }` |
|   8123 |   728 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
|   3593 |   729 | `		ph7_class *pClass = ReflectResolveClass(pVm, pTarget);` |
|      - |   730 | `		ph7_class_method *pMeth;` |
|   3593 |   731 | `		if( pClass == 0 ){` |
|    ! 0 |   732 | `			return 0;` |
|      - |   733 | `		}` |
|   5387 |   734 | `		pMeth = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|   1794 |   735 | `			SyBlobLength(&pMethodArg->sBlob));` |
|   3593 |   736 | `		if( pMeth && (pMeth->iFlags & PH7_CLASS_ATTR_FABRICATED) ){` |
|      - |   737 | ``			/* `Closure::__invoke` named over an OBJECT is the one method whose IDENTITY`` |
|      - |   738 | `			 * and whose BODY come from different places: php builds an internal method` |
|      - |   739 | `			 * record on the Closure class and copies the closure's parameter list into` |
|      - |   740 | `			 * it. So report the method (name, class, modifiers) and the closure's own` |
|      - |   741 | `			 * function (parameters, return type, what invoke() runs) together. Named` |
|      - |   742 | `			 * over a class instead, there is no closure and the declaration -- which is` |
|      - |   743 | `` 			 * empty -- is all there is, which is why php's class-level `__invoke` `` |
|      - |   744 | `			 * answers zero parameters. */` |
|    105 |   745 | `			ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|    105 |   746 | `			if( pClo ){` |
|     99 |   747 | `				ph7_vm_func *pBody = ReflectResolveCallable(pVm, pTarget, 0, 0, 0, 0, 0);` |
|     99 |   748 | `				if( pBody ){` |
|     99 |   749 | `					if( ppClass ){ *ppClass = pClass; }` |
|     99 |   750 | `					if( ppMeth ){ *ppMeth = pMeth; }` |
|     99 |   751 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|     99 |   752 | `					return pBody;` |
|      - |   753 | `				}` |
|    ! 0 |   754 | `			}` |
|      3 |   755 | `		}` |
|   3495 |   756 | `		if( pMeth == 0 ){` |
|      - |   757 | `			/* getMethods()/ReflectionClass reports a base class's PRIVATE methods on` |
|      - |   758 | `			 * the subclass (php copies them into the child's table), but private` |
|      - |   759 | `			 * methods are not inherited into the child's method table, so the plain` |
|      - |   760 | `			 * extract misses them when a ReflectionMethod obtained from the subclass` |
|      - |   761 | `			 * is re-resolved by (subclass, name). Walk the base chain to find the` |
|      - |   762 | `			 * declaring class's own copy. */` |
|    ! 0 |   763 | `			ph7_class *pWalk = pClass->pBase;` |
|    ! 0 |   764 | `			while( pWalk && pMeth == 0 ){` |
|    ! 0 |   765 | `				pMeth = PH7_ClassExtractMethod(pWalk, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|    ! 0 |   766 | `					SyBlobLength(&pMethodArg->sBlob));` |
|    ! 0 |   767 | `				pWalk = pWalk->pBase;` |
|    ! 0 |   768 | `			}` |
|    ! 0 |   769 | `		}` |
|   3495 |   770 | `		if( pMeth == 0 ){` |
|    ! 0 |   771 | `			return 0;` |
|      - |   772 | `		}` |
|   3495 |   773 | `		if( ppClass ){ *ppClass = pClass; }` |
|   3495 |   774 | `		if( ppMeth ){ *ppMeth = pMeth; }` |
|   3495 |   775 | `		return &pMeth->sFunc;` |
|      - |   776 | `	}` |
|      - |   777 | `	{` |
|   4535 |   778 | `		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|   4535 |   779 | `		if( pClo ){` |
|      - |   780 | `			SyString sAttr;` |
|      - |   781 | `			ph7_value *pFn;` |
|    349 |   782 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    349 |   783 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|    349 |   784 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) < 1 ){` |
|    ! 0 |   785 | `				return 0;` |
|      - |   786 | `			}` |
|      - |   787 | `			/* A closure over an object method or __invoke object` |
|      - |   788 | `			 * (Closure::fromCallable([$obj,'m']) / fromCallable($invokeObj)) stores the` |
|      - |   789 | `			 * bare method name in $__fn and the class in $__scope. Resolve it as a METHOD` |
|      - |   790 | `			 * of $__scope FIRST — before the global function table — so a same-named` |
|      - |   791 | `			 * global function does not shadow the method (the bug: $__fn "add" hitting a` |
|      - |   792 | `			 * global add()). A plain anonymous closure's $__fn is its unique lambda name,` |
|      - |   793 | `			 * which is not a method, so this falls through cleanly. */` |
|      - |   794 | `			{` |
|      - |   795 | `				SyString sScope;` |
|      - |   796 | `				ph7_value *pScope;` |
|    349 |   797 | `				SyStringInitFromBuf(&sScope, "__scope", 7);` |
|    349 |   798 | `				pScope = PH7_ClassInstanceFetchAttr(pClo, &sScope);` |
|    349 |   799 | `				if( pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0 ){` |
|    203 |   800 | `					ph7_class *pScopeCls = PH7_VmExtractClass(pVm,` |
|    134 |   801 | `						(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|    136 |   802 | `					if( pScopeCls ){` |
|    203 |   803 | `						ph7_class_method *pScopeMeth = PH7_ClassExtractMethod(pScopeCls,` |
|    134 |   804 | `							(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    136 |   805 | `						if( pScopeMeth ){` |
|     98 |   806 | `							if( ppClass ){ *ppClass = pScopeCls; }` |
|     98 |   807 | `							if( ppMeth ){ *ppMeth = pScopeMeth; }` |
|     98 |   808 | `							if( ppClosure ){ *ppClosure = pClo; }` |
|     98 |   809 | `							return &pScopeMeth->sFunc;` |
|      - |   810 | `						}` |
|     19 |   811 | `					}` |
|     19 |   812 | `				}` |
|      - |   813 | `			}` |
|    253 |   814 | `			pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    253 |   815 | `			if( pEntry == 0 ){` |
|      - |   816 | `				/* A Closure over a host function (Closure::fromCallable('strlen')) */` |
|     28 |   817 | `				pEntry = PH7_VmGetHostFunction(pVm, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob), FALSE);` |
|     28 |   818 | `				if( pEntry && ppHost ){` |
|      3 |   819 | `					*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|      3 |   820 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|      1 |   821 | `				}` |
|     28 |   822 | `				return 0;` |
|      - |   823 | `			}` |
|    227 |   824 | `			if( ppClosure ){ *ppClosure = pClo; }` |
|    227 |   825 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|      - |   826 | `		}` |
|      - |   827 | `	}` |
|   4189 |   828 | `	if( pTarget->iFlags & MEMOBJ_STRING ){` |
|   4189 |   829 | `		if( SyBlobLength(&pTarget->sBlob) < 1 ){` |
|    ! 0 |   830 | `			return 0;` |
|      - |   831 | `		}` |
|   4189 |   832 | `		pEntry = PH7_VmGetUserFunction(pVm, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob), FALSE);` |
|   4189 |   833 | `		if( pEntry ){` |
|    945 |   834 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|      - |   835 | `		}` |
|   3249 |   836 | `		pEntry = PH7_VmGetHostFunction(pVm, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob), FALSE);` |
|   3249 |   837 | `		if( pEntry && ppHost ){` |
|   3231 |   838 | `			*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|   1609 |   839 | `		}` |
|   1618 |   840 | `	}` |
|   3249 |   841 | `	return 0;` |
|   4060 |   842 | `}` |
|      - |   843 | `/*` |
|      - |   844 | ` * ---------------------------------------------------------------------------` |
|      - |   845 | ` * The signature parser.` |
|      - |   846 | ` *` |
|      - |   847 | ` * A C builtin and a native method declare their parameters as ONE php-style` |
|      - |   848 | `` * string (`"string $name, ?int $len = null"`) — the aBuiltinSig[] row or the`` |
|      - |   849 | ` * PH7_NativeClassSpec's zSig — because neither has a compiled parameter list to` |
|      - |   850 | ` * read. Reflection needs that string as the same param-meta shape a compiled` |
|      - |   851 | ` * function produces, so it is parsed here and the descriptor comes out of` |
|      - |   852 | ` * __reflect_func_info() already uniform.` |
|      - |   853 | ` *` |
|      - |   854 | ` * This was chunk 8 of the reflection prelude (__reflect_sig_split /` |
|      - |   855 | ` * __reflect_sig_scalar / __reflect_parse_sig / __reflect_sig_fixup), which every` |
|      - |   856 | ` * caller had to remember to wrap around __reflect_func_info() — six call sites,` |
|      - |   857 | ` * and the one that FORGOT is why every native method reported zero parameters` |
|      - |   858 | ` * until 31 Jul. Producing the parsed form at the source removes the wrapper and` |
|      - |   859 | ` * the possibility of forgetting it.` |
|      - |   860 | ` * ---------------------------------------------------------------------------` |
|      - |   861 | ` */` |
|      - |   862 | `/* Trim ASCII spaces off both ends of [z, z+n). */` |
|  33106 |   863 | `static void ReflectSigTrim(const char **pz, int *pn)` |
|      5 |   864 | `{` |
|  33111 |   865 | `	const char *z = *pz;` |
|  33111 |   866 | `	int n = *pn;` |
|  64840 |   867 | `	while( n > 0 && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|  15180 |   868 | `		z++;` |
|  15180 |   869 | `		n--;` |
|      4 |   870 | `	}` |
|  52252 |   871 | `	while( n > 0 && (z[n-1] == ' ' \|\| z[n-1] == '\t') ){` |
|   2592 |   872 | `		n--;` |
|      4 |   873 | `	}` |
|  33111 |   874 | `	*pz = z;` |
|  33111 |   875 | `	*pn = n;` |
|  33111 |   876 | `}` |
|      - |   877 | ``/* Byte search that respects single-quoted runs (a default may be `'a,b'` or`` |
|      - |   878 | `` * `'it\'s'`), which is the whole reason the split is not SyByteFind. Answers`` |
|      - |   879 | ` * the offset of the first unquoted zWhat, or -1. */` |
|  42558 |   880 | `static int ReflectSigFindUnquoted(const char *z, int n, char cWhat)` |
|      5 |   881 | `{` |
|      - |   882 | `	int k;` |
|  42563 |   883 | `	char cQuote = 0;` |
| 649561 |   884 | `	for( k = 0 ; k < n ; k++ ){` |
| 632895 |   885 | `		if( cQuote ){` |
|   5294 |   886 | `			if( z[k] == '\\' && k + 1 < n ){` |
|    960 |   887 | `				k++;` |
|   4815 |   888 | `			}else if( z[k] == cQuote ){` |
|   2024 |   889 | `				cQuote = 0;` |
|   1014 |   890 | `			}` |
| 630250 |   891 | `		}else if( z[k] == '\'' \|\| z[k] == '"' ){` |
|      - |   892 | `			/* BOTH spellings open a run. A native method's zSig lives in C, so` |
|      - |   893 | ``			 * `string $separator = ","` is the natural way to write it -- and`` |
|      - |   894 | `			 * tracking only the single quote let the comma INSIDE that default` |
|      - |   895 | `			 * split the parameter in two, which is how setCsvControl() came to` |
|      - |   896 | ``			 * report four parameters, one of them named `$"`. */`` |
|   2024 |   897 | `			cQuote = z[k];` |
| 626595 |   898 | `		}else if( z[k] == cWhat ){` |
|  25897 |   899 | `			return k;` |
|      - |   900 | `		}` |
| 303492 |   901 | `	}` |
|  16671 |   902 | `	return -1;` |
|  21281 |   903 | `}` |
|      - |   904 | `/* Does [z,n) contain zNeedle? (case-sensitive; nNeedle > 0) */` |
|  15886 |   905 | `static int ReflectSigHas(const char *z, int n, const char *zNeedle, int nNeedle)` |
|      5 |   906 | `{` |
|      - |   907 | `	int k;` |
| 186725 |   908 | `	for( k = 0 ; k + nNeedle <= n ; k++ ){` |
| 171189 |   909 | `		if( SyMemcmp((const void *)&z[k], (const void *)zNeedle, (sxu32)nNeedle) == 0 ){` |
|    354 |   910 | `			return 1;` |
|      - |   911 | `		}` |
|  85422 |   912 | `	}` |
|  15541 |   913 | `	return 0;` |
|   7948 |   914 | `}` |
|      - |   915 | ``/* Case-insensitive twin, for the `null` arm of a union type text. */`` |
|   5317 |   916 | `static int ReflectSigHasNoCase(const char *z, int n, const char *zNeedle, int nNeedle)` |
|      5 |   917 | `{` |
|      - |   918 | `	int k, j;` |
|  21719 |   919 | `	for( k = 0 ; k + nNeedle <= n ; k++ ){` |
|  17236 |   920 | `		for( j = 0 ; j < nNeedle ; j++ ){` |
|  17212 |   921 | `			if( SyToLower(z[k+j]) != SyToLower(zNeedle[j]) ){` |
|  16402 |   922 | `				break;` |
|      - |   923 | `			}` |
|    410 |   924 | `		}` |
|  16426 |   925 | `		if( j == nNeedle ){` |
|     28 |   926 | `			return 1;` |
|      - |   927 | `		}` |
|   8202 |   928 | `	}` |
|   5298 |   929 | `	return 0;` |
|   2662 |   930 | `}` |
|      - |   931 | `/*` |
|      - |   932 | `` * One `C::K` (or `C::class`) term of a declared default, evaluated.`` |
|      - |   933 | ` *` |
|      - |   934 | `` * php's stub writes these as SOURCE — `string $class = SplFileInfo::class`,`` |
|      - |   935 | `` * `int $flags = FilesystemIterator::KEY_AS_PATHNAME\|…` — and the reflector then`` |
|      - |   936 | ` * answers both the text (the export line) and the VALUE (getDefaultValue()). A` |
|      - |   937 | ` * native method's zSig is that same source, so the value has to be read out of` |
|      - |   938 | ` * the class here; there are no compiled parameter records to hold it.` |
|      - |   939 | ` */` |
|   1262 |   940 | `static int ReflectSigClassConst(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      4 |   941 | `{` |
|   1266 |   942 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |   943 | `	ph7_class_attr *pAttr;` |
|      - |   944 | `	ph7_class *pClass;` |
|      - |   945 | `	ph7_value *pValue;` |
|      - |   946 | `	int iSep;` |
|   1266 |   947 | `	ReflectSigTrim(&z, &n);` |
|   6092 |   948 | `	for( iSep = 0 ; iSep + 1 < n ; ++iSep ){` |
|   4936 |   949 | `		if( z[iSep] == ':' && z[iSep+1] == ':' ){` |
|    110 |   950 | `			break;` |
|      - |   951 | `		}` |
|   2417 |   952 | `	}` |
|   1266 |   953 | `	if( iSep + 1 >= n \|\| iSep < 1 ){` |
|   1160 |   954 | `		return 0;` |
|      - |   955 | `	}` |
|    110 |   956 | `	pClass = PH7_VmExtractClass(pVm, z, (sxu32)iSep, TRUE, 0);` |
|    110 |   957 | `	if( pClass == 0 ){` |
|    ! 0 |   958 | `		return 0;` |
|      - |   959 | `	}` |
|    110 |   960 | `	z += iSep + 2;` |
|    110 |   961 | `	n -= iSep + 2;` |
|    110 |   962 | `	if( n == (int)sizeof("class")-1 && SyMemcmp(z, "class", sizeof("class")-1) == 0 ){` |
|      - |   963 | ``		/* `C::class` is the class NAME, and php prints the name it was DECLARED`` |
|      - |   964 | `		 * with rather than the spelling in the signature. */` |
|     11 |   965 | `		SyString *pName = &pClass->sName;` |
|     11 |   966 | `		ph7_value_string(pOut, SyStringData(pName), (int)SyStringLength(pName));` |
|     11 |   967 | `		return 1;` |
|      - |   968 | `	}` |
|    102 |   969 | `	pAttr = PH7_ClassExtractConstant(pClass, z, (sxu32)n);` |
|    102 |   970 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|    ! 0 |   971 | `		return 0;` |
|      - |   972 | `	}` |
|    102 |   973 | `	if( pAttr->nIdx == SXU32_HIGH ){` |
|      - |   974 | `` 		/* Not materialized yet: bring it into being exactly as a direct `C::K` `` |
|      - |   975 | `		 * read would. An enum CASE is a singleton with its own materializer --` |
|      - |   976 | `		 * running the constant initializer over one answers nothing, which is` |
|      - |   977 | ``		 * why `RoundingMode::HalfAwayFromZero` could not be reduced at all. */`` |
|     36 |   978 | `		sxi32 rcConst = (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)` |
|    ! 0 |   979 | `			? VmEnumMaterializeCase(pVm, pClass, pAttr)` |
|     22 |   980 | `			: VmClassConstEvalOnDemand(pVm, pClass, pAttr);` |
|     25 |   981 | `		if( rcConst != SXRET_OK ){` |
|    ! 0 |   982 | `			return 0;` |
|      - |   983 | `		}` |
|     11 |   984 | `	}` |
|    102 |   985 | `	pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, pAttr->nIdx);` |
|    102 |   986 | `	if( pValue == 0 ){` |
|    ! 0 |   987 | `		return 0;` |
|      - |   988 | `	}` |
|    102 |   989 | `	PH7_MemObjStore(pValue, pOut);` |
|    102 |   990 | `	return 1;` |
|    635 |   991 | `}` |
|      - |   992 | `/*` |
|      - |   993 | ` * A GLOBAL constant named by a declared default — php's stub writes` |
|      - |   994 | `` * `int $severity = E_ERROR` and both surfaces read it from here: the export`` |
|      - |   995 | ` * prints the NAME (rule 28: the argument was never folded, so php still has the` |
|      - |   996 | ` * source) and getDefaultValue() answers what it expands to. Answers 0 for` |
|      - |   997 | ` * anything that is not a plain identifier naming a defined constant, which is` |
|      - |   998 | `` * what keeps `null`/`true`/a bare number out of this branch.`` |
|      - |   999 | ` */` |
|   2780 |  1000 | `static int ReflectSigIsIdent(const char *z, int n)` |
|      4 |  1001 | `{` |
|      - |  1002 | `	int k;` |
|   2784 |  1003 | `	if( n < 1 \|\| (z[0] != '_' && !SyisAlpha(z[0])) ){` |
|   1530 |  1004 | `		return 0;` |
|      - |  1005 | `	}` |
|   8058 |  1006 | `	for( k = 1 ; k < n ; ++k ){` |
|   6884 |  1007 | `		if( z[k] != '_' && z[k] != '\\' && !SyisAlphaNum(z[k]) ){` |
|     83 |  1008 | `			return 0;` |
|      - |  1009 | `		}` |
|   3404 |  1010 | `	}` |
|   1178 |  1011 | `	return 1;` |
|   1394 |  1012 | `}` |
|   2780 |  1013 | `static int ReflectSigGlobalConst(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      4 |  1014 | `{` |
|      - |  1015 | `	SyHashEntry *pEntry;` |
|      - |  1016 | `	ph7_constant *pCons;` |
|   2784 |  1017 | `	ReflectSigTrim(&z, &n);` |
|   2784 |  1018 | `	if( !ReflectSigIsIdent(z, n) ){` |
|   1610 |  1019 | `		return 0;` |
|      - |  1020 | `	}` |
|   1178 |  1021 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant, (const void *)z, (sxu32)n);` |
|   1178 |  1022 | `	if( pEntry == 0 ){` |
|    965 |  1023 | `		return 0;` |
|      - |  1024 | `	}` |
|    216 |  1025 | `	pCons = (ph7_constant *)pEntry->pUserData;` |
|    216 |  1026 | `	if( pCons == 0 \|\| pCons->xExpand == 0 ){` |
|    ! 0 |  1027 | `		return 0;` |
|      - |  1028 | `	}` |
|    216 |  1029 | `	pCons->xExpand(pOut, pCons->pUserData);` |
|    216 |  1030 | `	return 1;` |
|   1394 |  1031 | `}` |
|      - |  1032 | `/*` |
|      - |  1033 | `` * A constant EXPRESSION: one term, or the `\|` fold php's own stubs write for a`` |
|      - |  1034 | `` * flags default (`KEY_AS_PATHNAME \| CURRENT_AS_FILEINFO \| SKIP_DOTS`, and`` |
|      - |  1035 | `` * `SQLITE3_OPEN_READWRITE \| SQLITE3_OPEN_CREATE`). Either kind of name may`` |
|      - |  1036 | ` * stand in it -- a class constant or a global one -- because php's stubs write` |
|      - |  1037 | ` * both, and a term is looked up as whichever it turns out to be.` |
|      - |  1038 | ` */` |
|    472 |  1039 | `static int ReflectSigConstExpr(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      4 |  1040 | `{` |
|    476 |  1041 | `	sxi64 iAcc = 0;` |
|    476 |  1042 | `	int iStart = 0;` |
|    476 |  1043 | `	int k, nTerm = 0;` |
|    476 |  1044 | `	if( !ReflectSigHas(z, n, "::", 2) && !ReflectSigHas(z, n, "\|", 1) ){` |
|    410 |  1045 | `		return 0;` |
|      - |  1046 | `	}` |
|   2537 |  1047 | `	for( k = 0 ; k <= n ; ++k ){` |
|   2471 |  1048 | `		if( k < n && z[k] != '\|' ){` |
|   2343 |  1049 | `			continue;` |
|      - |  1050 | `		}` |
|    128 |  1051 | `		if( !ReflectSigClassConst(pCtx, &z[iStart], k - iStart, pOut)` |
|    111 |  1052 | `		 && !ReflectSigGlobalConst(pCtx, &z[iStart], k - iStart, pOut) ){` |
|    ! 0 |  1053 | `			return 0;` |
|      - |  1054 | `		}` |
|    131 |  1055 | `		nTerm++;` |
|    131 |  1056 | `		if( k < n \|\| nTerm > 1 ){` |
|      - |  1057 | `			/* A fold is arithmetic, so every arm has to be an integer; a lone` |
|      - |  1058 | ``			 * term keeps whatever type it had (`C::class` is a string). */`` |
|    101 |  1059 | `			if( (pOut->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) != 0 ){` |
|    ! 0 |  1060 | `				return 0;` |
|      - |  1061 | `			}` |
|    101 |  1062 | `			PH7_MemObjToInteger(pOut);` |
|    101 |  1063 | `			iAcc \|= pOut->x.iVal;` |
|     49 |  1064 | `		}` |
|    131 |  1065 | `		iStart = k + 1;` |
|     67 |  1066 | `	}` |
|     69 |  1067 | `	if( nTerm > 1 ){` |
|     39 |  1068 | `		ph7_value_int64(pOut, iAcc);` |
|     18 |  1069 | `	}` |
|     69 |  1070 | `	return nTerm > 0;` |
|    240 |  1071 | `}` |
|      - |  1072 | `/* One hex digit's value, or -1. */` |
|     54 |  1073 | `static int ReflectHexVal(char c)` |
|      2 |  1074 | `{` |
|     56 |  1075 | `	if( c >= '0' && c <= '9' ) return c - '0';` |
|    ! 0 |  1076 | `	if( c >= 'a' && c <= 'f' ) return c - 'a' + 10;` |
|    ! 0 |  1077 | `	if( c >= 'A' && c <= 'F' ) return c - 'A' + 10;` |
|    ! 0 |  1078 | `	return -1;` |
|     29 |  1079 | `}` |
|      - |  1080 | `/*` |
|      - |  1081 | ` * php keeps the SOURCE of a constant EXPRESSION in a stub's default and prints` |
|      - |  1082 | ` * it back verbatim, and two more shapes of one live in the signature table` |
|      - |  1083 | `` * beside the `C::K` and `A\|B` forms already handled: an integer written in a`` |
|      - |  1084 | `` * RADIX (`0777` for mkdir's permissions) and a product (`2 * 1024 * 1024` for`` |
|      - |  1085 | ` * SplTempFileObject's memory bound). Both are text php never reduces on the` |
|      - |  1086 | ` * page; both have to be reduced for getDefaultValue().` |
|      - |  1087 | ` */` |
|   1420 |  1088 | `static int ReflectSigRadixInt(const char *z, int n, sxi64 *pOut)` |
|      4 |  1089 | `{` |
|   1424 |  1090 | `	int iRadix = 8, k = 1;` |
|   1424 |  1091 | `	sxi64 iVal = 0;` |
|   1424 |  1092 | `	ReflectSigTrim(&z, &n);` |
|   1424 |  1093 | `	if( n < 2 \|\| z[0] != '0' ){` |
|   1418 |  1094 | `		return 0;` |
|      - |  1095 | `	}` |
|      8 |  1096 | `	if( z[1] == 'x' \|\| z[1] == 'X' ){       iRadix = 16; k = 2; }` |
|      8 |  1097 | `	else if( z[1] == 'o' \|\| z[1] == 'O' ){  iRadix = 8;  k = 2; }` |
|      8 |  1098 | `	else if( z[1] == 'b' \|\| z[1] == 'B' ){  iRadix = 2;  k = 2; }` |
|      8 |  1099 | `	if( k >= n ){` |
|    ! 0 |  1100 | `		return 0;` |
|      - |  1101 | `	}` |
|     26 |  1102 | `	for( ; k < n ; ++k ){` |
|     20 |  1103 | `		int iDigit = ReflectHexVal(z[k]);` |
|     20 |  1104 | `		if( iDigit < 0 \|\| iDigit >= iRadix ){` |
|    ! 0 |  1105 | `			return 0;` |
|      - |  1106 | `		}` |
|     20 |  1107 | `		iVal = iVal * iRadix + iDigit;` |
|     11 |  1108 | `	}` |
|      8 |  1109 | `	*pOut = iVal;` |
|      8 |  1110 | `	return 1;` |
|    714 |  1111 | `}` |
|      - |  1112 | ``/* `a * b * c` over integer literals and integer constants. */`` |
|    390 |  1113 | `static int ReflectSigProduct(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      4 |  1114 | `{` |
|    394 |  1115 | `	sxi64 iAcc = 1;` |
|    394 |  1116 | `	int iStart = 0, k, nTerm = 0;` |
|    394 |  1117 | `	if( ReflectSigFindUnquoted(z, n, '*') < 0 ){` |
|    392 |  1118 | `		return 0;` |
|      - |  1119 | `	}` |
|     35 |  1120 | `	for( k = 0 ; k <= n ; ++k ){` |
|      - |  1121 | `		const char *zTerm;` |
|      - |  1122 | `		int nTermLen;` |
|     33 |  1123 | `		sxi64 iVal = 0;` |
|     33 |  1124 | `		sxu8 bReal = 0;` |
|     33 |  1125 | `		if( k < n && z[k] != '*' ){` |
|     27 |  1126 | `			continue;` |
|      - |  1127 | `		}` |
|      7 |  1128 | `		zTerm = &z[iStart];` |
|      7 |  1129 | `		nTermLen = k - iStart;` |
|      7 |  1130 | `		ReflectSigTrim(&zTerm, &nTermLen);` |
|      7 |  1131 | `		if( nTermLen < 1 ){` |
|    ! 0 |  1132 | `			return 0;` |
|      - |  1133 | `		}` |
|      7 |  1134 | `		if( !ReflectSigRadixInt(zTerm, nTermLen, &iVal) ){` |
|      - |  1135 | `			ph7_value *pTerm;` |
|      7 |  1136 | `			if( SyStrIsNumeric(zTerm, (sxu32)nTermLen, &bReal, 0) == SXRET_OK && !bReal ){` |
|      7 |  1137 | `				SyStrToInt64(zTerm, (sxu32)nTermLen, (void *)&iVal, 0);` |
|      4 |  1138 | `			}else if( (pTerm = ph7_context_new_scalar(pCtx)) != 0` |
|    ! 0 |  1139 | `			       && (ReflectSigClassConst(pCtx, zTerm, nTermLen, pTerm)` |
|    ! 0 |  1140 | `			        \|\| ReflectSigGlobalConst(pCtx, zTerm, nTermLen, pTerm))` |
|    ! 0 |  1141 | `			       && (pTerm->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|    ! 0 |  1142 | `				PH7_MemObjToInteger(pTerm);` |
|    ! 0 |  1143 | `				iVal = pTerm->x.iVal;` |
|    ! 0 |  1144 | `			}else{` |
|    ! 0 |  1145 | `				return 0;` |
|      - |  1146 | `			}` |
|      3 |  1147 | `		}` |
|      7 |  1148 | `		iAcc *= iVal;` |
|      7 |  1149 | `		nTerm++;` |
|      7 |  1150 | `		iStart = k + 1;` |
|      4 |  1151 | `	}` |
|      3 |  1152 | `	if( nTerm < 2 ){` |
|    ! 0 |  1153 | `		return 0;` |
|      - |  1154 | `	}` |
|      3 |  1155 | `	ph7_value_int64(pOut, iAcc);` |
|      3 |  1156 | `	return 1;` |
|    199 |  1157 | `}` |
|      - |  1158 | `/*` |
|      - |  1159 | ` * Is this default TEXT one php prints back as SOURCE rather than as a value?` |
|      - |  1160 | `` * The `C::K` and `A\|B` forms are answered by their own readers above; these`` |
|      - |  1161 | ` * two are the ones a plain literal reader would silently reduce to a number.` |
|      - |  1162 | ` */` |
|   1030 |  1163 | `static int ReflectSigIsSourceExpr(const char *z, int n)` |
|      4 |  1164 | `{` |
|   1034 |  1165 | `	sxi64 iIgnored = 0;` |
|   1547 |  1166 | `	return ReflectSigFindUnquoted(z, n, '*') >= 0` |
|   1030 |  1167 | `	    \|\| ReflectSigRadixInt(z, n, &iIgnored);` |
|      4 |  1168 | `}` |
|      - |  1169 | `/*` |
|      - |  1170 | ` * A default-value TEXT to a value, when the text denotes a scalar php can` |
|      - |  1171 | `` * reproduce. Answers 1 and fills pOut, or 0 for anything else (`[]`,`` |
|      - |  1172 | `` * `array (`, a constant name) which the caller reports its own way.`` |
|      - |  1173 | ` */` |
|   1256 |  1174 | `static int ReflectSigScalar(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      4 |  1175 | `{` |
|   1260 |  1176 | `	sxu8 bReal = 0;` |
|   1260 |  1177 | `	if( n == 1 && z[0] == '?' ){` |
|    ! 0 |  1178 | `		return 0;` |
|      - |  1179 | `	}` |
|   1260 |  1180 | `	if( (n == 4 && (SyMemcmp(z,"NULL",4) == 0 \|\| SyMemcmp(z,"null",4) == 0)) ){` |
|    439 |  1181 | `		ph7_value_null(pOut);` |
|    439 |  1182 | `		return 1;` |
|      - |  1183 | `	}` |
|    824 |  1184 | `	if( n == 4 && SyMemcmp(z,"true",4) == 0 ){` |
|     53 |  1185 | `		ph7_value_bool(pOut,1);` |
|     53 |  1186 | `		return 1;` |
|      - |  1187 | `	}` |
|    774 |  1188 | `	if( n == 5 && SyMemcmp(z,"false",5) == 0 ){` |
|     71 |  1189 | `		ph7_value_bool(pOut,0);` |
|     71 |  1190 | `		return 1;` |
|      - |  1191 | `	}` |
|    706 |  1192 | `	if( n >= 2 && (z[0] == '\'' \|\| z[0] == '"') && z[n-1] == z[0] ){` |
|      - |  1193 | `		/* Undo the escapes the signature writer emits, which are php's own set --` |
|      - |  1194 | `		 * the same one ReflectExportStrQ puts back when it prints the line. A row` |
|      - |  1195 | `		 * cannot hold these bytes any other way: a signature is a C string, so a` |
|      - |  1196 | ``		 * NUL would END it, which is why trim()'s ` \n\r\t\v\x00` had no default`` |
|      - |  1197 | `		 * row at all and its parameter answered isDefaultValueAvailable() false.` |
|      - |  1198 | `		 * BOTH quote spellings are accepted because both scanners in vm_arg_check.c` |
|      - |  1199 | `` 		 * step over either one: a native method's zSig lives in C, so `= \"static\"` `` |
|      - |  1200 | `		 * is the natural way to write Closure::bindTo's default. */` |
|      - |  1201 | `		SyBlob sOut;` |
|    248 |  1202 | `		char cQuote = z[0];` |
|      - |  1203 | `		int k;` |
|    248 |  1204 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    604 |  1205 | `		for( k = 1 ; k < n - 1 ; k++ ){` |
|    360 |  1206 | `			char c = z[k];` |
|    360 |  1207 | `			if( c == '\\' && k + 1 < n - 1 ){` |
|    162 |  1208 | `				char e = z[k+1];` |
|    162 |  1209 | `				int bTook = 1;` |
|    162 |  1210 | `				switch( e ){` |
|     31 |  1211 | `					case 'n': c = 0x0A; break;` |
|     27 |  1212 | `					case 'r': c = 0x0D; break;` |
|     23 |  1213 | `					case 't': c = 0x09; break;` |
|     23 |  1214 | `					case 'v': c = 0x0B; break;` |
|      5 |  1215 | `					case 'f': c = 0x0C; break;` |
|    ! 0 |  1216 | `					case 'e': c = 0x1B; break;` |
|      9 |  1217 | `					case 'x': {` |
|     19 |  1218 | `						int h1 = ReflectHexVal(k + 3 < n - 1 ? z[k+2] : 0);` |
|     19 |  1219 | `						int h2 = ReflectHexVal(k + 3 < n - 1 ? z[k+3] : 0);` |
|     19 |  1220 | `						if( h1 < 0 \|\| h2 < 0 ){` |
|    ! 0 |  1221 | `							bTook = 0;` |
|    ! 0 |  1222 | `							break;` |
|      - |  1223 | `						}` |
|     19 |  1224 | `						c = (char)((h1 << 4) \| h2);` |
|     19 |  1225 | `						k += 2;   /* the two hex digits; the 'x' below */` |
|     19 |  1226 | `						break;` |
|      - |  1227 | `					}` |
|     19 |  1228 | `					default:` |
|     40 |  1229 | `						bTook = (e == cQuote \|\| e == '\\');` |
|     40 |  1230 | `						c = e;` |
|     38 |  1231 | `						break;` |
|      - |  1232 | `				}` |
|    162 |  1233 | `				if( bTook ){` |
|    162 |  1234 | `					k++;` |
|     82 |  1235 | `				}else{` |
|    ! 0 |  1236 | `					c = '\\';` |
|      - |  1237 | `				}` |
|     80 |  1238 | `			}` |
|    360 |  1239 | `			SyBlobAppend(&sOut,(const void *)&c,sizeof(char));` |
|    182 |  1240 | `		}` |
|    248 |  1241 | `		ph7_value_string(pOut,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    248 |  1242 | `		SyBlobRelease(&sOut);` |
|    248 |  1243 | `		return 1;` |
|      - |  1244 | `	}` |
|    462 |  1245 | `	if( ReflectSigConstExpr(pCtx,z,n,pOut) ){` |
|     55 |  1246 | `		return 1;` |
|      - |  1247 | `	}` |
|    410 |  1248 | `	if( ReflectSigGlobalConst(pCtx,z,n,pOut) ){` |
|     19 |  1249 | `		return 1;` |
|      - |  1250 | `	}` |
|    394 |  1251 | `	if( ReflectSigProduct(pCtx,z,n,pOut) ){` |
|      3 |  1252 | `		return 1;` |
|      - |  1253 | `	}` |
|      - |  1254 | `	{` |
|    392 |  1255 | `		sxi64 iRadix = 0;` |
|    392 |  1256 | `		if( ReflectSigRadixInt(z,n,&iRadix) ){` |
|      6 |  1257 | `			ph7_value_int64(pOut,iRadix);` |
|      6 |  1258 | `			return 1;` |
|      - |  1259 | `		}` |
|      - |  1260 | `	}` |
|    388 |  1261 | `	if( n > 0 && SyStrIsNumeric(z,(sxu32)n,&bReal,0) == SXRET_OK ){` |
|      - |  1262 | `		/* php's own rule for the text form: a '.', an exponent or a hex marker` |
|      - |  1263 | `		 * makes it a float, everything else an int. */` |
|    328 |  1264 | `		if( bReal \|\| ReflectSigHas(z,n,".",1)` |
|    331 |  1265 | `		 \|\| ReflectSigHasNoCase(z,n,"e",1) \|\| ReflectSigHasNoCase(z,n,"x",1) ){` |
|      - |  1266 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - |  1267 | `			/* the VALUE is an out-parameter here; the return is a status, and` |
|      - |  1268 | `			 * reading it as the number made every float default 0.0 */` |
|      3 |  1269 | `			sxreal rVal = 0.0;` |
|      3 |  1270 | `			SyStrToReal(z,(sxu32)n,(void *)&rVal,0);` |
|      3 |  1271 | `			ph7_value_double(pOut,rVal);` |
|      - |  1272 | `#else` |
|      - |  1273 | `			sxi64 iRaw = 0;` |
|      - |  1274 | `			SyStrToInt64(z,(sxu32)n,(void *)&iRaw,0);` |
|      - |  1275 | `			ph7_value_int64(pOut,iRaw);` |
|      - |  1276 | `#endif` |
|      2 |  1277 | `		}else{` |
|    330 |  1278 | `			sxi64 iVal = 0;` |
|    330 |  1279 | `			SyStrToInt64(z,(sxu32)n,(void *)&iVal,0);` |
|    330 |  1280 | `			ph7_value_int64(pOut,iVal);` |
|      - |  1281 | `		}` |
|    332 |  1282 | `		return 1;` |
|      - |  1283 | `	}` |
|     59 |  1284 | `	return 0;` |
|    632 |  1285 | `}` |
|      - |  1286 | `/*` |
|      - |  1287 | ` * The same reduction, for the door OUTSIDE this file that needs it.` |
|      - |  1288 | ` *` |
|      - |  1289 | ` * A declared signature's default is TEXT, and two places have to turn it into a` |
|      - |  1290 | ` * value: getDefaultValue() above, and the named-argument binder in` |
|      - |  1291 | ` * vm_arg_check.c, which materializes the parameters a named call SKIPPED` |
|      - |  1292 | `` * (`mkdir($d, recursive: true)` has to supply `$permissions`). That binder`` |
|      - |  1293 | `` * carried a reader of its own -- null/true/false, `[]`, a quoted string and a`` |
|      - |  1294 | ` * DECIMAL number -- so the two doors onto one value disagreed on every default` |
|      - |  1295 | `` * the small reader could not spell: `0777` bound as 777 (a directory created`` |
|      - |  1296 | ` * mode 01411, which the next chdir() could not enter), and any default written` |
|      - |  1297 | `` * as a CONSTANT (`ENT_QUOTES \| …`, `M_E`, `SORT_REGULAR`, `SplFileObject::class`,`` |
|      - |  1298 | `` * `2 * 1024 * 1024` -- ~180 parameters) bound as "not passed", so php's own`` |
|      - |  1299 | `` * `htmlspecialchars("<a>", encoding: 'UTF-8')` was an ArgumentCountError here.`` |
|      - |  1300 | `` * There is one reader now and this is its door; `[]` stays with the caller`` |
|      - |  1301 | ` * because the value it builds is a hashmap rather than a scalar.` |
|      - |  1302 | ` */` |
|     30 |  1303 | `PH7_PRIVATE int PH7_VmSigDefaultToValue(ph7_context *pCtx,const char *z,int n,ph7_value *pOut)` |
|      3 |  1304 | `{` |
|     33 |  1305 | `	if( pCtx == 0 \|\| z == 0 \|\| n < 1 \|\| pOut == 0 ){` |
|    ! 0 |  1306 | `		return 0;` |
|      - |  1307 | `	}` |
|     33 |  1308 | `	return ReflectSigScalar(pCtx,z,n,pOut);` |
|     18 |  1309 | `}` |
|      - |  1310 | `/*` |
|      - |  1311 | ` * One parameter, described uniformly.` |
|      - |  1312 | ` *` |
|      - |  1313 | ` * A reflected function's parameters come from one of TWO places — a compiled` |
|      - |  1314 | `` * function's `ph7_vm_func_arg` list, or the php-style SIGNATURE STRING a C`` |
|      - |  1315 | ` * builtin / native method declares — and both the descriptor array` |
|      - |  1316 | ` * (__reflect_func_info, for the chunks still in PHP) and the native` |
|      - |  1317 | ` * ReflectionParameter have to read them the same way. This struct is what they` |
|      - |  1318 | `` * agree on; `pArg` is set only on the compiled path, where a default is`` |
|      - |  1319 | ` * BYTE-CODE rather than text.` |
|      - |  1320 | ` */` |
|      - |  1321 | `typedef struct ReflectParamDesc ReflectParamDesc;` |
|      - |  1322 | `struct ReflectParamDesc` |
|      - |  1323 | `{` |
|      - |  1324 | `	SyString sName;` |
|      - |  1325 | `	int iPos;` |
|      - |  1326 | `	int bByRef, bVariadic, bHasDef, bOptional, bNullable, bPromoted;` |
|      - |  1327 | `	SyString sType;            /* nByte == 0 -> untyped. For a COMPILED parameter this` |
|      - |  1328 | `	                            * points into zTypeBuf below, php's stored text with` |
|      - |  1329 | ``	                            * `self`/`parent` resolved (see ReflectDeclScope). */`` |
|      - |  1330 | `	char zTypeBuf[192];` |
|      - |  1331 | `	SyString sDefText;         /* signature-declared default TEXT (nByte == 0 -> none) */` |
|      - |  1332 | `	ph7_vm_func_arg *pArg;     /* compiled parameter, or NULL for a declared one */` |
|      - |  1333 | `	int bInternal;             /* owner is INTERNAL to php -- which for a COMPILED` |
|      - |  1334 | `	                            * parameter is the prelude's builtins, and decides` |
|      - |  1335 | `	                            * how php spells its default (see ReflectExportDefault) */` |
|      - |  1336 | `};` |
|      - |  1337 | `/*` |
|      - |  1338 | ` * Split a signature string on its top-level commas (a quoted default may hold` |
|      - |  1339 | ` * its own). Answers the parameter COUNT; when iWant is in range, hands back` |
|      - |  1340 | ` * that part's bytes.` |
|      - |  1341 | ` */` |
|  11684 |  1342 | `static int ReflectSigPart(const char *zSig, int nSig, int iWant,` |
|      - |  1343 | `	const char **pzPart, int *pnPart)` |
|      5 |  1344 | `{` |
|  11689 |  1345 | `	int iPos = 0;` |
|  24277 |  1346 | `	while( nSig > 0 ){` |
|  21355 |  1347 | `		int iComma = ReflectSigFindUnquoted(zSig,nSig,',');` |
|  21355 |  1348 | `		const char *zPart = zSig;` |
|  21355 |  1349 | `		int nPart = iComma < 0 ? nSig : iComma;` |
|  21355 |  1350 | `		ReflectSigTrim(&zPart,&nPart);` |
|  21355 |  1351 | `		if( nPart > 0 ){` |
|  21355 |  1352 | `			if( iPos == iWant && pzPart ){` |
|   5581 |  1353 | `				*pzPart = zPart;` |
|   5581 |  1354 | `				*pnPart = nPart;` |
|   2788 |  1355 | `			}` |
|  21355 |  1356 | `			iPos++;` |
|  10675 |  1357 | `		}` |
|  21355 |  1358 | `		if( iComma < 0 ){` |
|   8767 |  1359 | `			break;` |
|      - |  1360 | `		}` |
|  12592 |  1361 | `		zSig += iComma + 1;` |
|  12592 |  1362 | `		nSig -= iComma + 1;` |
|      4 |  1363 | `	}` |
|  11689 |  1364 | `	return iPos;` |
|      5 |  1365 | `}` |
|      - |  1366 | ``/* One `type &$name = default` part into the uniform description. */`` |
|   5576 |  1367 | `static void ReflectSigDescribe(const char *z, int n, int iPos, ReflectParamDesc *pOut)` |
|      5 |  1368 | `{` |
|   5581 |  1369 | `	const char *zDef = 0;` |
|   5581 |  1370 | `	int nDef = 0;` |
|      - |  1371 | `	int iEq, iDollar, iSpace;` |
|   5581 |  1372 | `	SyZero(pOut,sizeof(*pOut));` |
|   5581 |  1373 | `	pOut->iPos = iPos;` |
|   5581 |  1374 | `	if( n > 0 && z[0] == '~' ){` |
|      - |  1375 | `		/* The signature table's "declared here, screened by the builtin" marker` |
|      - |  1376 | `		 * (see vm_arg_check.c): php DECLARES this type and its C body asks for a` |
|      - |  1377 | `		 * tighter one, so Reflection reports what follows the marker. */` |
|    230 |  1378 | `		z++;` |
|    230 |  1379 | `		n--;` |
|    114 |  1380 | `	}` |
|      - |  1381 | ``	/* `= default` splits off first: everything after the first unquoted '='. */`` |
|   5581 |  1382 | `	iEq = ReflectSigFindUnquoted(z,n,'=');` |
|   5581 |  1383 | `	if( iEq >= 0 ){` |
|   2470 |  1384 | `		zDef = &z[iEq+1];` |
|   2470 |  1385 | `		nDef = n - iEq - 1;` |
|   2470 |  1386 | `		ReflectSigTrim(&zDef,&nDef);` |
|   2470 |  1387 | `		n = iEq;` |
|   2470 |  1388 | `		ReflectSigTrim(&z,&n);` |
|   1233 |  1389 | `	}` |
|   5581 |  1390 | `	if( zDef && nDef == 1 && zDef[0] == '?' ){` |
|      - |  1391 | ``		/* `= ?` is the table's OPTIONAL-but-no-default marker, php's own shape`` |
|      - |  1392 | `		 * for a parameter like ReflectionClass::getStaticPropertyValue()'s` |
|      - |  1393 | `		 * $default: isOptional() true, isDefaultValueAvailable() FALSE, so` |
|      - |  1394 | `		 * getDefaultValue() raises. Reporting it as a default (which is what` |
|      - |  1395 | ``		 * a bare `hasdef` did) made that call answer NULL instead. */`` |
|     98 |  1396 | `		pOut->bOptional = 1;` |
|     98 |  1397 | `		zDef = 0;` |
|     98 |  1398 | `		nDef = 0;` |
|     48 |  1399 | `	}` |
|   5581 |  1400 | `	pOut->bVariadic = ReflectSigHas(z,n,"...",3);` |
|   5581 |  1401 | `	if( pOut->bVariadic ){` |
|      - |  1402 | `		/* php: a variadic parameter never HAS a default -- it defaults to "no` |
|      - |  1403 | `		 * further arguments", which is not a value. Several signature rows still` |
|      - |  1404 | ``		 * write `mixed ...$values = ?` (the table's "optional, unspecified"`` |
|      - |  1405 | `		 * marker), and honouring it made isDefaultValueAvailable() true where php` |
|      - |  1406 | `		 * says false, so getDefaultValue() then threw on printf/array_merge. */` |
|     55 |  1407 | `		zDef = 0;` |
|     55 |  1408 | `		nDef = 0;` |
|     27 |  1409 | `	}` |
|   5581 |  1410 | `	iDollar = ReflectSigFindUnquoted(z,n,'$');` |
|   5581 |  1411 | `	iSpace = ReflectSigFindUnquoted(z,n,' ');` |
|      - |  1412 | `	{` |
|   5581 |  1413 | `		const char *zName = iDollar < 0 ? z : &z[iDollar+1];` |
|   5581 |  1414 | `		int nName = iDollar < 0 ? n : n - iDollar - 1;` |
|   5581 |  1415 | `		SyStringInitFromBuf(&pOut->sName,zName,nName);` |
|      - |  1416 | `	}` |
|   5581 |  1417 | `	pOut->bByRef = ReflectSigHas(z,n,"&",1);` |
|   5581 |  1418 | `	pOut->bHasDef = zDef != 0;` |
|   5581 |  1419 | `	pOut->bOptional = pOut->bOptional \|\| pOut->bVariadic \|\| zDef != 0;` |
|   5581 |  1420 | `	if( iSpace >= 0 && iDollar >= 0 && iSpace < iDollar ){` |
|      - |  1421 | `` 		/* The type is whatever precedes the first space, so `?DOMNode $child` `` |
|      - |  1422 | ``		 * types as `?DOMNode` and an untyped `$x` types as nothing. */`` |
|   5233 |  1423 | `		pOut->bNullable = (z[0] == '?' \|\| ReflectSigHasNoCase(z,iSpace,"null",4));` |
|   5233 |  1424 | `		SyStringInitFromBuf(&pOut->sType,z,iSpace);` |
|   2614 |  1425 | `	}` |
|   5581 |  1426 | `	if( zDef ){` |
|   2374 |  1427 | `		SyStringInitFromBuf(&pOut->sDefText,zDef,nDef);` |
|   1185 |  1428 | `	}` |
|   5581 |  1429 | `}` |
|      - |  1430 | `/*` |
|      - |  1431 | ` * Resolve a Generator object into its wrapper. Mirrors the static` |
|      - |  1432 | ` * VmGeneratorExtractCtx in vm.c: the $__ctx attribute carries the` |
|      - |  1433 | ` * ph7_generator pointer as a resource value.` |
|      - |  1434 | ` */` |
|     44 |  1435 | `static ph7_generator * ReflectGeneratorCtx(ph7_vm *pVm, ph7_value *pVal)` |
|      1 |  1436 | `{` |
|      - |  1437 | `	ph7_class_instance *pThis;` |
|      - |  1438 | `	ph7_value *pAttr;` |
|      - |  1439 | `	SyString sAttr;` |
|     45 |  1440 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVm->pGeneratorClass == 0 ){` |
|    ! 0 |  1441 | `		return 0;` |
|      - |  1442 | `	}` |
|     45 |  1443 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     45 |  1444 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|    ! 0 |  1445 | `		return 0;` |
|      - |  1446 | `	}` |
|     45 |  1447 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|     45 |  1448 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     45 |  1449 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|    ! 0 |  1450 | `		return 0;` |
|      - |  1451 | `	}` |
|     45 |  1452 | `	return (ph7_generator *)pAttr->x.pOther;` |
|     23 |  1453 | `}` |
|      - |  1454 | `/*` |
|      - |  1455 | ` * Evaluate the recorded argument expressions of one declared attribute and` |
|      - |  1456 | ` * answer them as a PHP array, or NULL when the target no longer resolves.` |
|      - |  1457 | ` *` |
|      - |  1458 | ` * apArg is the four-part SPEC a ReflectionAttribute carries — kind 'class'` |
|      - |  1459 | ` * (target = class), 'attr' (class + property/constant name), 'method' (class +` |
|      - |  1460 | ` * method), 'fn' (function name or Closure), 'param' (function spec + parameter` |
|      - |  1461 | ` * index), 'const' (global constant name) — plus which attribute of that target.` |
|      - |  1462 | ` * Named arguments become string keys.` |
|      - |  1463 | ` *` |
|      - |  1464 | ` * The values are evaluated HERE rather than when the reflector was built:` |
|      - |  1465 | `` * `#[Attr(self::SOME)]` runs php code, and php runs it at getArguments() time.`` |
|      - |  1466 | ` */` |
|    182 |  1467 | `static ph7_value * ReflectAttrArgs(ph7_context *pCtx, ph7_value **apArg, sxu32 nAttrIdx)` |
|      4 |  1468 | `{` |
|    186 |  1469 | `	ph7_vm *pVm = pCtx->pVm;` |
|    186 |  1470 | `	SySet *pAttrs = 0;` |
|    186 |  1471 | `	ph7_class *pDeclCls = 0; /* class an attribute is declared on: scope for self:: in its args */` |
|      - |  1472 | `	ph7_attribute *pAttrRec;` |
|      - |  1473 | `	ph7_value *pOut;` |
|      - |  1474 | `	const char *zKind;` |
|      - |  1475 | `	int nKind;` |
|      - |  1476 | `	sxu32 n;` |
|    186 |  1477 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|    258 |  1478 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|    148 |  1479 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|    148 |  1480 | `		if( pClass ){ pAttrs = &pClass->aAttrs; pDeclCls = pClass; }` |
|    116 |  1481 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|      5 |  1482 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|      5 |  1483 | `		ph7_class_attr *pMember = pClass ? ReflectFetchMember(pClass, apArg[2]) : 0;` |
|      5 |  1484 | `		if( pMember ){ pAttrs = &pMember->aAttrs; pDeclCls = pClass; }` |
|     47 |  1485 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|     16 |  1486 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     16 |  1487 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|     36 |  1488 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|     15 |  1489 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], 0, 0, 0, 0, 0);` |
|     15 |  1490 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|     18 |  1491 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|      7 |  1492 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], apArg[2], 0, 0, 0, 0);` |
|      7 |  1493 | `		ph7_vm_func_arg *pParam = pFunc` |
|      6 |  1494 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|      7 |  1495 | `		if( pParam ){ pAttrs = &pParam->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|      6 |  1496 | `	}else if( nKind == 5 && SyMemcmp(zKind, "const", 5) == 0 ){` |
|      - |  1497 | ``		/* Global constant (php 8.5 attributes on `const` statements) */`` |
|      - |  1498 | `		const char *zCName;` |
|      - |  1499 | `		int nCName;` |
|      - |  1500 | `		SyHashEntry *pCEntry;` |
|      3 |  1501 | `		zCName = ph7_value_to_string(apArg[1], &nCName);` |
|      3 |  1502 | `		pCEntry = nCName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zCName, (sxu32)nCName) : 0;` |
|      3 |  1503 | `		if( pCEntry ){ pAttrs = &((ph7_constant *)pCEntry->pUserData)->aAttrs; }` |
|      1 |  1504 | `	}` |
|    182 |  1505 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|    186 |  1506 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|    ! 0 |  1507 | `		return 0;` |
|      - |  1508 | `	}` |
|    402 |  1509 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|    220 |  1510 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|      - |  1511 | `		ph7_value sValue;` |
|    220 |  1512 | `		PH7_MemObjInit(pVm, &sValue);` |
|    220 |  1513 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|      - |  1514 | `` 			/* Evaluate under the attribute's declaring-class scope so `self::class` `` |
|      - |  1515 | ``			 * / `self::CONST` / `parent::` resolve like php (else self:: would bind`` |
|      - |  1516 | `			 * to the reflection machinery's own class). */` |
|    185 |  1517 | `			PH7_VmExecAttrArg(pVm, &pArgRec->aByteCode, pDeclCls, &sValue);` |
|    129 |  1518 | `		}else if( pArgRec->pNativeValue ){` |
|      - |  1519 | ``			/* A NATIVE attribute (php's own `#[Attribute(...)]` on Attribute and`` |
|      - |  1520 | `			 * Deprecated): the argument is a literal, not byte-code. */` |
|     38 |  1521 | `			PH7_NativeLiteralValue(pVm, pArgRec->pNativeValue, &sValue);` |
|     17 |  1522 | `		}` |
|    220 |  1523 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|     13 |  1524 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|      7 |  1525 | `		}else{` |
|    208 |  1526 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|      - |  1527 | `		}` |
|    220 |  1528 | `		PH7_MemObjRelease(&sValue);` |
|    112 |  1529 | `	}` |
|    186 |  1530 | `	return pOut;` |
|     95 |  1531 | `}` |
|      - |  1532 | `/*` |
|      - |  1533 | ` * ---------------------------------------------------------------------------` |
|      - |  1534 | ` * The ReflectionType family.` |
|      - |  1535 | ` *` |
|      - |  1536 | ` * php's four type objects are pure VALUES: a text, a nullability flag, and for` |
|      - |  1537 | ` * the composites a list of members. Nothing in userland can build one — php` |
|      - |  1538 | `` * declares no constructor on any of them and refuses `clone` — so the prelude's`` |
|      - |  1539 | `` * public `__construct($name, $nullable, $text)` existed only because the factory`` |
|      - |  1540 | ` * that fills them was itself PHP. The factory is C now (ReflectMakeType), so the` |
|      - |  1541 | ` * constructors are gone and the classes say what php's say.` |
|      - |  1542 | ` * ---------------------------------------------------------------------------` |
|      - |  1543 | ` */` |
|      - |  1544 | `#define RT_TEXT     "__text"` |
|      - |  1545 | `#define RT_NULLABLE "__nullable"` |
|      - |  1546 | `#define RT_TNAME    "__tname"` |
|      - |  1547 | `#define RT_TYPES    "__types"` |
|      - |  1548 |  |
|     26 |  1549 | `static int vm_builtin_ReflectionType_allowsNull(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  1550 | `{` |
|     28 |  1551 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     13 |  1552 | `	SXUNUSED(nArg);` |
|     13 |  1553 | `	SXUNUSED(apArg);` |
|     28 |  1554 | `	ph7_result_bool(pCtx, pThis != 0 && PH7_NativeAttrTruthy(pThis, RT_NULLABLE));` |
|     28 |  1555 | `	return PH7_OK;` |
|      2 |  1556 | `}` |
|   1341 |  1557 | `static int vm_builtin_ReflectionType_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  1558 | `{` |
|   1346 |  1559 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   1346 |  1560 | `	const char *zText = "";` |
|   1346 |  1561 | `	int nText = 0;` |
|    669 |  1562 | `	SXUNUSED(nArg);` |
|    669 |  1563 | `	SXUNUSED(apArg);` |
|   1346 |  1564 | `	if( pThis ){` |
|   1346 |  1565 | `		PH7_NativeAttrStr(pThis, RT_TEXT, &zText, &nText);` |
|    669 |  1566 | `	}` |
|   1346 |  1567 | `	ph7_result_string(pCtx, zText, nText);` |
|   1346 |  1568 | `	return PH7_OK;` |
|      5 |  1569 | `}` |
|     22 |  1570 | `static int vm_builtin_ReflectionNamedType_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  1571 | `{` |
|     25 |  1572 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     25 |  1573 | `	const char *zName = "";` |
|     25 |  1574 | `	int nName = 0;` |
|     11 |  1575 | `	SXUNUSED(nArg);` |
|     11 |  1576 | `	SXUNUSED(apArg);` |
|     25 |  1577 | `	if( pThis ){` |
|     25 |  1578 | `		PH7_NativeAttrStr(pThis, RT_TNAME, &zName, &nName);` |
|     11 |  1579 | `	}` |
|     25 |  1580 | `	ph7_result_string(pCtx, zName, nName);` |
|     25 |  1581 | `	return PH7_OK;` |
|      3 |  1582 | `}` |
|      - |  1583 | `/* php's builtin-type set, case-insensitively. Anything else is a class name. */` |
|     46 |  1584 | `static int ReflectTypeIsBuiltin(const char *zName, int nName)` |
|      1 |  1585 | `{` |
|      - |  1586 | `	static const char *azBuiltin[] = {` |
|      - |  1587 | `		"int","float","string","bool","array","object","mixed",` |
|      - |  1588 | `		"void","never","null","callable","iterable","true","false"` |
|      - |  1589 | `	};` |
|      - |  1590 | `	sxu32 n;` |
|    369 |  1591 | `	for( n = 0 ; n < SX_ARRAYSIZE(azBuiltin) ; n++ ){` |
|    359 |  1592 | `		int nWant = (int)SyStrlen(azBuiltin[n]);` |
|      - |  1593 | `		int k;` |
|    359 |  1594 | `		if( nWant != nName ){` |
|    283 |  1595 | `			continue;` |
|      - |  1596 | `		}` |
|    243 |  1597 | `		for( k = 0 ; k < nWant ; k++ ){` |
|    207 |  1598 | `			if( SyToLower(zName[k]) != azBuiltin[n][k] ){` |
|     41 |  1599 | `				break;` |
|      - |  1600 | `			}` |
|     84 |  1601 | `		}` |
|     77 |  1602 | `		if( k == nWant ){` |
|     37 |  1603 | `			return 1;` |
|      - |  1604 | `		}` |
|     21 |  1605 | `	}` |
|     11 |  1606 | `	return 0;` |
|     24 |  1607 | `}` |
|     38 |  1608 | `static int vm_builtin_ReflectionNamedType_isBuiltin(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  1609 | `{` |
|     39 |  1610 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     39 |  1611 | `	const char *zName = "";` |
|     39 |  1612 | `	int nName = 0;` |
|     19 |  1613 | `	SXUNUSED(nArg);` |
|     19 |  1614 | `	SXUNUSED(apArg);` |
|     39 |  1615 | `	if( pThis ){` |
|     39 |  1616 | `		PH7_NativeAttrStr(pThis, RT_TNAME, &zName, &nName);` |
|     19 |  1617 | `	}` |
|     39 |  1618 | `	ph7_result_bool(pCtx, ReflectTypeIsBuiltin(zName, nName));` |
|     39 |  1619 | `	return PH7_OK;` |
|      1 |  1620 | `}` |
|      6 |  1621 | `static int vm_builtin_ReflectionType_getTypes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  1622 | `{` |
|      7 |  1623 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      7 |  1624 | `	ph7_value *pTypes = pThis ? PH7_NativeAttr(pThis, RT_TYPES) : 0;` |
|      3 |  1625 | `	SXUNUSED(nArg);` |
|      3 |  1626 | `	SXUNUSED(apArg);` |
|      7 |  1627 | `	if( pTypes && (pTypes->iFlags & MEMOBJ_HASHMAP) ){` |
|      7 |  1628 | `		ph7_result_value(pCtx, pTypes);` |
|      4 |  1629 | `	}else{` |
|      - |  1630 | `		/* The declared default is a native NULL slot (a spec cannot carry an` |
|      - |  1631 | `		 * array literal), but php's getTypes() always answers a list. */` |
|    ! 0 |  1632 | `		ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|    ! 0 |  1633 | `		if( pEmpty ){` |
|    ! 0 |  1634 | `			ph7_result_value(pCtx, pEmpty);` |
|    ! 0 |  1635 | `		}` |
|      - |  1636 | `	}` |
|      7 |  1637 | `	return PH7_OK;` |
|      1 |  1638 | `}` |
|      - |  1639 | `/* Build one of the three concrete types with its slots filled. The caller owns` |
|      - |  1640 | ` * the reference (PH7_NativeResultObject / a list insert drops it). */` |
|   1991 |  1641 | `static ph7_class_instance * ReflectNewType(ph7_context *pCtx, const char *zClass,` |
|      - |  1642 | `	const char *zText, int nText, int bNullable)` |
|      5 |  1643 | `{` |
|   1996 |  1644 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1996 |  1645 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|   1996 |  1646 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|   1996 |  1647 | `	if( pObj == 0 ){` |
|    ! 0 |  1648 | `		return 0;` |
|      - |  1649 | `	}` |
|   1996 |  1650 | `	PH7_NativeSetAttrStr(pVm, pObj, RT_TEXT, zText, nText);` |
|   1996 |  1651 | `	PH7_NativeSetAttrBool(pVm, pObj, RT_NULLABLE, bNullable);` |
|   1996 |  1652 | `	return pObj;` |
|    999 |  1653 | `}` |
|      - |  1654 | `/*` |
|      - |  1655 | ` * Append pType to a composite's member list, handing over the caller's reference.` |
|      - |  1656 | ` *` |
|      - |  1657 | ` * The carrier is a STACK value, deliberately: a ph7_context_new_scalar() is` |
|      - |  1658 | ` * released with the call context, and releasing a MEMOBJ_OBJ carrier unrefs the` |
|      - |  1659 | ` * instance a second time — which freed every member of a union or intersection` |
|      - |  1660 | ` * the moment its factory returned, so the list came back holding dead objects.` |
|      - |  1661 | ` * PH7_NativeResultObject hands an instance over the same way.` |
|      - |  1662 | ` */` |
|    726 |  1663 | `static void ReflectTypeListAdd(ph7_context *pCtx, ph7_value *pList, ph7_class_instance *pType)` |
|      5 |  1664 | `{` |
|      - |  1665 | `	ph7_value sVal;` |
|    731 |  1666 | `	if( pType == 0 ){` |
|    ! 0 |  1667 | `		return;` |
|      - |  1668 | `	}` |
|    731 |  1669 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    731 |  1670 | `	sVal.x.pOther = pType;` |
|    731 |  1671 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|    731 |  1672 | `	ph7_array_add_elem(pList, 0, &sVal);   /* takes its own reference */` |
|    731 |  1673 | `	PH7_ClassInstanceUnref(pType);` |
|    368 |  1674 | `}` |
|      - |  1675 | `/* Exact, case-insensitive name test (php lower-cases before comparing). */` |
|   3062 |  1676 | `static int ReflectTypeNameIs(const char *z, int n, const char *zWant)` |
|      5 |  1677 | `{` |
|   3067 |  1678 | `	int nWant = (int)SyStrlen(zWant), k;` |
|   3067 |  1679 | `	if( n != nWant ){` |
|   2584 |  1680 | `		return 0;` |
|      - |  1681 | `	}` |
|    756 |  1682 | `	for( k = 0 ; k < n ; k++ ){` |
|    702 |  1683 | `		if( SyToLower(z[k]) != zWant[k] ){` |
|    434 |  1684 | `			return 0;` |
|      - |  1685 | `		}` |
|    137 |  1686 | `	}` |
|     57 |  1687 | `	return 1;` |
|   1533 |  1688 | `}` |
|      - |  1689 | `/*` |
|      - |  1690 | ` * A ReflectionNamedType for one name.` |
|      - |  1691 | ` *` |
|      - |  1692 | ` * bQMark says the text carried a leading '?', which is the ONLY thing that puts` |
|      - |  1693 | `` * one back on the rendered text — while `null` and `mixed` are nullable by their`` |
|      - |  1694 | ` * own meaning without ever rendering a '?'. Those two rules are independent, and` |
|      - |  1695 | `` * conflating them is how `?array` came out as `array`.`` |
|      - |  1696 | ` */` |
|   1773 |  1697 | `static ph7_class_instance * ReflectNewNamed(ph7_context *pCtx, const char *z, int n, int bQMark)` |
|      5 |  1698 | `{` |
|      - |  1699 | `	ph7_class_instance *pObj;` |
|      - |  1700 | `	char zBuf[256];` |
|   1778 |  1701 | `	const char *zText = z;` |
|   1778 |  1702 | `	int nText = n;` |
|   1778 |  1703 | `	int bNullable = bQMark \|\| ReflectTypeNameIs(z, n, "null") \|\| ReflectTypeNameIs(z, n, "mixed");` |
|   1778 |  1704 | `	if( bQMark && n + 1 < (int)sizeof(zBuf) ){` |
|    257 |  1705 | `		zBuf[0] = '?';` |
|    257 |  1706 | `		SyMemcpy(z, &zBuf[1], (sxu32)n);` |
|    257 |  1707 | `		zText = zBuf;` |
|    257 |  1708 | `		nText = n + 1;` |
|    126 |  1709 | `	}` |
|   1778 |  1710 | `	pObj = ReflectNewType(pCtx, "ReflectionNamedType", zText, nText, bNullable);` |
|   1778 |  1711 | `	if( pObj ){` |
|   1778 |  1712 | `		PH7_NativeSetAttrStr(pCtx->pVm, pObj, RT_TNAME, z, n);` |
|    885 |  1713 | `	}` |
|   1778 |  1714 | `	return pObj;` |
|      5 |  1715 | `}` |
|      - |  1716 | `/*` |
|      - |  1717 | `` * One ATOM of a type text: `?X`, `(A&B)` or a plain name. An intersection is`` |
|      - |  1718 | ` * the only composite an atom can be, because php only nests that way.` |
|      - |  1719 | ` */` |
|   1761 |  1720 | `static ph7_class_instance * ReflectMakeAtom(ph7_context *pCtx, const char *z, int n)` |
|      5 |  1721 | `{` |
|   1766 |  1722 | `	int bQMark = 0;` |
|   1766 |  1723 | `	if( n > 0 && z[0] == '?' ){` |
|    257 |  1724 | `		bQMark = 1;` |
|    257 |  1725 | `		z++;` |
|    257 |  1726 | `		n--;` |
|    126 |  1727 | `	}` |
|   1766 |  1728 | `	if( n > 1 && z[0] == '(' && z[n-1] == ')' ){` |
|     10 |  1729 | `		z++;` |
|     10 |  1730 | `		n -= 2;` |
|      4 |  1731 | `	}` |
|   1766 |  1732 | `	if( ReflectSigFindUnquoted(z, n, '&') >= 0 ){` |
|     14 |  1733 | `		ph7_class_instance *pObj = ReflectNewType(pCtx, "ReflectionIntersectionType", z, n, 0);` |
|     14 |  1734 | `		ph7_value *pList = ph7_context_new_array(pCtx);` |
|     14 |  1735 | `		const char *zCur = z;` |
|     14 |  1736 | `		int nCur = n;` |
|     14 |  1737 | `		if( pObj == 0 \|\| pList == 0 ){` |
|    ! 0 |  1738 | `			return pObj;` |
|      - |  1739 | `		}` |
|     26 |  1740 | `		while( nCur > 0 ){` |
|     26 |  1741 | `			int iCut = ReflectSigFindUnquoted(zCur, nCur, '&');` |
|     38 |  1742 | `			ReflectTypeListAdd(pCtx, pList,` |
|     12 |  1743 | `				ReflectNewNamed(pCtx, zCur, iCut < 0 ? nCur : iCut, 0));` |
|     26 |  1744 | `			if( iCut < 0 ){` |
|     14 |  1745 | `				break;` |
|      - |  1746 | `			}` |
|     14 |  1747 | `			zCur += iCut + 1;` |
|     14 |  1748 | `			nCur -= iCut + 1;` |
|      2 |  1749 | `		}` |
|     14 |  1750 | `		PH7_NativeSetProp(pCtx->pVm, pObj, RT_TYPES, sizeof(RT_TYPES)-1, pList);` |
|     14 |  1751 | `		return pObj;` |
|      - |  1752 | `	}` |
|   1754 |  1753 | `	return ReflectNewNamed(pCtx, z, n, bQMark);` |
|    884 |  1754 | `}` |
|      - |  1755 | `/*` |
|      - |  1756 | ` * A declared type TEXT to the object php answers for it: a named type, a union,` |
|      - |  1757 | `` * or an intersection. NULL for an absent type. `X\|null` collapses back to a`` |
|      - |  1758 | `` * NULLABLE named type, which is what php reports (`?X`), but only when exactly`` |
|      - |  1759 | ` * one non-null arm is left and it is not itself an intersection.` |
|      - |  1760 | ` */` |
|   1481 |  1761 | `static ph7_class_instance * ReflectMakeType(ph7_context *pCtx, const char *zText, int nText)` |
|      5 |  1762 | `{` |
|   1486 |  1763 | `	const char *zBody = zText;` |
|   1486 |  1764 | `	int nBody = nText;` |
|   1486 |  1765 | `	int bNullable = 0, bHasNull = 0, nParts = 0, nNonNull = 0;` |
|   1486 |  1766 | `	const char *zLastNonNull = 0;` |
|   1486 |  1767 | `	int nLastNonNull = 0;` |
|      - |  1768 | `	const char *zCur;` |
|      - |  1769 | `	int nCur, iDepth, k, iStart;` |
|   1486 |  1770 | `	if( nText < 1 ){` |
|    ! 0 |  1771 | `		return 0;` |
|      - |  1772 | `	}` |
|   1486 |  1773 | `	if( zBody[0] == '?' ){` |
|    257 |  1774 | `		bNullable = 1;` |
|    257 |  1775 | `		zBody++;` |
|    257 |  1776 | `		nBody--;` |
|    126 |  1777 | `	}` |
|      - |  1778 | ``	/* Split on the TOP-LEVEL '\|' only: `A\|(B&C)` has one at depth 0 and none`` |
|      - |  1779 | `	 * inside the parentheses. */` |
|   1486 |  1780 | `	iDepth = 0;` |
|   1486 |  1781 | `	iStart = 0;` |
|  13921 |  1782 | `	for( k = 0 ; k <= nBody ; k++ ){` |
|  12440 |  1783 | `		if( k < nBody && zBody[k] == '(' ){` |
|     10 |  1784 | `			iDepth++;` |
|     10 |  1785 | `			continue;` |
|      - |  1786 | `		}` |
|  12432 |  1787 | `		if( k < nBody && zBody[k] == ')' ){` |
|     10 |  1788 | `			iDepth--;` |
|     10 |  1789 | `			continue;` |
|      - |  1790 | `		}` |
|  12424 |  1791 | `		if( k == nBody \|\| (zBody[k] == '\|' && iDepth == 0) ){` |
|   1766 |  1792 | `			zCur = &zBody[iStart];` |
|   1766 |  1793 | `			nCur = k - iStart;` |
|   1766 |  1794 | `			nParts++;` |
|   1766 |  1795 | `			if( nCur == 4 && ReflectSigHasNoCase(zCur, 4, "null", 4) ){` |
|     10 |  1796 | `				bHasNull = 1;` |
|      6 |  1797 | `			}else{` |
|   1758 |  1798 | `				nNonNull++;` |
|   1758 |  1799 | `				zLastNonNull = zCur;` |
|   1758 |  1800 | `				nLastNonNull = nCur;` |
|      - |  1801 | `			}` |
|   1766 |  1802 | `			iStart = k + 1;` |
|    879 |  1803 | `		}` |
|   6207 |  1804 | `	}` |
|   1486 |  1805 | `	if( nParts > 1 ){` |
|      - |  1806 | `		ph7_class_instance *pObj;` |
|      - |  1807 | `		ph7_value *pList;` |
|    206 |  1808 | `		if( bHasNull && nNonNull == 1` |
|      8 |  1809 | `		 && ReflectSigFindUnquoted(zLastNonNull, nLastNonNull, '&') < 0 ){` |
|      - |  1810 | ``			/* `X\|null` IS `?X` to php. */`` |
|      - |  1811 | `			char zBuf[256];` |
|      - |  1812 | `			ph7_class_instance *pNamed;` |
|    ! 0 |  1813 | `			const char *zRender = zLastNonNull;` |
|    ! 0 |  1814 | `			int nRender = nLastNonNull;` |
|    ! 0 |  1815 | `			if( nLastNonNull + 1 < (int)sizeof(zBuf) ){` |
|    ! 0 |  1816 | `				zBuf[0] = '?';` |
|    ! 0 |  1817 | `				SyMemcpy(zLastNonNull, &zBuf[1], (sxu32)nLastNonNull);` |
|    ! 0 |  1818 | `				zRender = zBuf;` |
|    ! 0 |  1819 | `				nRender = nLastNonNull + 1;` |
|    ! 0 |  1820 | `			}` |
|    ! 0 |  1821 | `			pNamed = ReflectNewType(pCtx, "ReflectionNamedType", zRender, nRender, 1);` |
|    ! 0 |  1822 | `			if( pNamed ){` |
|    ! 0 |  1823 | `				PH7_NativeSetAttrStr(pCtx->pVm, pNamed, RT_TNAME, zLastNonNull, nLastNonNull);` |
|    ! 0 |  1824 | `			}` |
|    ! 0 |  1825 | `			return pNamed;` |
|      - |  1826 | `		}` |
|    211 |  1827 | `		pObj = ReflectNewType(pCtx, "ReflectionUnionType", zBody, nBody, bNullable \|\| bHasNull);` |
|    211 |  1828 | `		pList = ph7_context_new_array(pCtx);` |
|    211 |  1829 | `		if( pObj == 0 \|\| pList == 0 ){` |
|    ! 0 |  1830 | `			return pObj;` |
|      - |  1831 | `		}` |
|    211 |  1832 | `		iDepth = 0;` |
|    211 |  1833 | `		iStart = 0;` |
|   3581 |  1834 | `		for( k = 0 ; k <= nBody ; k++ ){` |
|   3375 |  1835 | `			if( k < nBody && zBody[k] == '(' ){` |
|     10 |  1836 | `				iDepth++;` |
|     10 |  1837 | `				continue;` |
|      - |  1838 | `			}` |
|   3367 |  1839 | `			if( k < nBody && zBody[k] == ')' ){` |
|     10 |  1840 | `				iDepth--;` |
|     10 |  1841 | `				continue;` |
|      - |  1842 | `			}` |
|   3359 |  1843 | `			if( k == nBody \|\| (zBody[k] == '\|' && iDepth == 0) ){` |
|    734 |  1844 | `				ReflectTypeListAdd(pCtx, pList,` |
|    243 |  1845 | `					ReflectMakeAtom(pCtx, &zBody[iStart], k - iStart));` |
|    491 |  1846 | `				iStart = k + 1;` |
|    243 |  1847 | `			}` |
|   1682 |  1848 | `		}` |
|    211 |  1849 | `		PH7_NativeSetProp(pCtx->pVm, pObj, RT_TYPES, sizeof(RT_TYPES)-1, pList);` |
|    211 |  1850 | `		return pObj;` |
|      - |  1851 | `	}` |
|   1280 |  1852 | `	if( ReflectSigFindUnquoted(zBody, nBody, '&') >= 0 ){` |
|      5 |  1853 | `		return ReflectMakeAtom(pCtx, zBody, nBody);` |
|      - |  1854 | `	}` |
|   1276 |  1855 | `	if( bNullable ){` |
|      - |  1856 | `		char zBuf[256];` |
|    257 |  1857 | `		if( nBody + 1 < (int)sizeof(zBuf) ){` |
|    257 |  1858 | `			zBuf[0] = '?';` |
|    257 |  1859 | `			SyMemcpy(zBody, &zBuf[1], (sxu32)nBody);` |
|    257 |  1860 | `			return ReflectMakeAtom(pCtx, zBuf, nBody + 1);` |
|      - |  1861 | `		}` |
|    ! 0 |  1862 | `	}` |
|   1024 |  1863 | `	return ReflectMakeAtom(pCtx, zBody, nBody);` |
|    744 |  1864 | `}` |
|      - |  1865 | `/*` |
|      - |  1866 | ` * Declare the four type classes. Called from PH7_VmInstallReflection() where` |
|      - |  1867 | `` * chunk 4 used to be compiled, so `Stringable` (a core interface) already`` |
|      - |  1868 | ` * exists. PH7_CLASS_NOCLONE is php's own rule for these: they are values the` |
|      - |  1869 | `` * engine hands out, and `clone $type` is an Error, not a copy.`` |
|      - |  1870 | ` */` |
|   7925 |  1871 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionTypes(ph7_vm *pVm)` |
|      5 |  1872 | `{` |
|      - |  1873 | `	static const PH7_NativePropDef aBaseProp[] = {` |
|      - |  1874 | `		{ RT_TEXT,     PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - |  1875 | `		{ RT_NULLABLE, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 }, 0 },` |
|      - |  1876 | `	};` |
|      - |  1877 | `	static const PH7_NativeMethodDef aBaseMethod[] = {` |
|      - |  1878 | `		{ "allowsNull", PH7_MOD_PUBLIC, "", "@bool",       vm_builtin_ReflectionType_allowsNull },` |
|      - |  1879 | `		{ "__toString", PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionType_toString },` |
|      - |  1880 | `	};` |
|      - |  1881 | `	static const PH7_NativePropDef aNamedProp[] = {` |
|      - |  1882 | `		{ RT_TNAME, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - |  1883 | `	};` |
|      - |  1884 | `	static const PH7_NativeMethodDef aNamedMethod[] = {` |
|      - |  1885 | `		{ "getName",   PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionNamedType_getName },` |
|      - |  1886 | `		{ "isBuiltin", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionNamedType_isBuiltin },` |
|      - |  1887 | `	};` |
|      - |  1888 | `	static const PH7_NativePropDef aCompProp[] = {` |
|      - |  1889 | `		{ RT_TYPES, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  1890 | `	};` |
|      - |  1891 | `	static const PH7_NativeMethodDef aCompMethod[] = {` |
|      - |  1892 | `		{ "getTypes", PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionType_getTypes },` |
|      - |  1893 | `	};` |
|      - |  1894 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  1895 | `		{ "ReflectionType", 0, "Stringable", PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  1896 | `		  aBaseMethod, SX_ARRAYSIZE(aBaseMethod), 0, 0, aBaseProp, SX_ARRAYSIZE(aBaseProp), 0, 0, 0 },` |
|      - |  1897 | `		{ "ReflectionNamedType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  1898 | `		  aNamedMethod, SX_ARRAYSIZE(aNamedMethod), 0, 0, aNamedProp, SX_ARRAYSIZE(aNamedProp), 0, 0, 0 },` |
|      - |  1899 | `		{ "ReflectionUnionType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  1900 | `		  aCompMethod, SX_ARRAYSIZE(aCompMethod), 0, 0, aCompProp, SX_ARRAYSIZE(aCompProp), 0, 0, 0 },` |
|      - |  1901 | `		{ "ReflectionIntersectionType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  1902 | `		  aCompMethod, SX_ARRAYSIZE(aCompMethod), 0, 0, aCompProp, SX_ARRAYSIZE(aCompProp), 0, 0, 0 },` |
|      - |  1903 | `	};` |
|   7930 |  1904 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  1905 | `}` |
|      - |  1906 | `/*` |
|      - |  1907 | ` * ---------------------------------------------------------------------------` |
|      - |  1908 | ` * The six standalone reflection classes.` |
|      - |  1909 | ` *` |
|      - |  1910 | ` * ReflectionGenerator, ReflectionFiber, ReflectionConstant, ReflectionExtension,` |
|      - |  1911 | ` * ReflectionZendExtension and ReflectionReference reflect ENGINE state rather` |
|      - |  1912 | ` * than a class hierarchy, so each was a prelude class over one or two thunks` |
|      - |  1913 | ` * whose only job was to hand that state to PHP. Their bodies are the C now, and` |
|      - |  1914 | ` * the four thunks that existed for them (__reflect_gen_info, __reflect_gen_exec,` |
|      - |  1915 | ` * __reflect_const_info, __reflect_ref_id) are gone with them.` |
|      - |  1916 | ` *` |
|      - |  1917 | ` * The ReflectionEnum family stays in the prelude: it extends ReflectionClass and` |
|      - |  1918 | ` * ReflectionClassConstant, which are still PHP.` |
|      - |  1919 | ` * ---------------------------------------------------------------------------` |
|      - |  1920 | ` */` |
|      - |  1921 | `#define RG_GEN   "__gen"` |
|      - |  1922 | `#define RF_FIBER "__fiber"` |
|      - |  1923 | `#define RR_ID    "__id"` |
|      - |  1924 |  |
|      - |  1925 | `/* Build an instance of a class that may still be PRELUDE PHP, running its` |
|      - |  1926 | ` * constructor. Answers 0 when the constructor threw (rc says which). */` |
|   2310 |  1927 | `static ph7_class_instance * ReflectConstruct(ph7_context *pCtx, const char *zClass,` |
|      - |  1928 | `	int nArg, ph7_value **apArg, sxi32 *pRc)` |
|      5 |  1929 | `{` |
|   2315 |  1930 | `	ph7_vm *pVm = pCtx->pVm;` |
|   2315 |  1931 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|      - |  1932 | `	ph7_class_instance *pThis;` |
|      - |  1933 | `	ph7_class_method *pCons;` |
|   2315 |  1934 | `	*pRc = PH7_OK;` |
|   2315 |  1935 | `	if( pClass == 0 ){` |
|    ! 0 |  1936 | `		return 0;` |
|      - |  1937 | `	}` |
|   2315 |  1938 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|   2315 |  1939 | `	if( pThis == 0 ){` |
|    ! 0 |  1940 | `		return 0;` |
|      - |  1941 | `	}` |
|   2315 |  1942 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|   2315 |  1943 | `	if( pCons ){` |
|   2315 |  1944 | `		sxi32 rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, nArg, apArg);` |
|   2315 |  1945 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|    ! 0 |  1946 | `			PH7_ClassInstanceCtorFailed(pThis);` |
|    ! 0 |  1947 | `			PH7_ClassInstanceUnref(pThis);` |
|    ! 0 |  1948 | `			*pRc = rc;` |
|    ! 0 |  1949 | `			return 0;` |
|      - |  1950 | `		}` |
|   1155 |  1951 | `	}` |
|   2315 |  1952 | `	return pThis;` |
|   1160 |  1953 | `}` |
|      - |  1954 | `/* The instance a native reflector wraps ($this->__gen / $this->__fiber). */` |
|     50 |  1955 | `static ph7_class_instance * ReflectWrapped(ph7_context *pCtx, const char *zSlot)` |
|      1 |  1956 | `{` |
|     51 |  1957 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     51 |  1958 | `	return pThis ? PH7_NativeAttrObj(pThis, zSlot) : 0;` |
|      1 |  1959 | `}` |
|      - |  1960 | `/* Hand back a wrapped instance the receiver already owns (a borrowed reference). */` |
|     10 |  1961 | `static int ReflectResultBorrowed(ph7_context *pCtx, ph7_class_instance *pObj)` |
|      1 |  1962 | `{` |
|      - |  1963 | `	ph7_value sVal;` |
|     11 |  1964 | `	if( pObj == 0 ){` |
|    ! 0 |  1965 | `		ph7_result_null(pCtx);` |
|    ! 0 |  1966 | `		return PH7_OK;` |
|      - |  1967 | `	}` |
|     11 |  1968 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     11 |  1969 | `	sVal.x.pOther = pObj;` |
|     11 |  1970 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|     11 |  1971 | `	ph7_result_value(pCtx, &sVal);   /* takes its own reference */` |
|     11 |  1972 | `	return PH7_OK;` |
|      6 |  1973 | `}` |
|      - |  1974 | `/*` |
|      - |  1975 | ` * ---------------------------------------------------------------------------` |
|      - |  1976 | ` * ReflectionAttribute — chunk 7.` |
|      - |  1977 | ` *` |
|      - |  1978 | ` * php's own class, plus the shared getAttributes() body that produces it. The` |
|      - |  1979 | ` * chunk it replaces also carried __reflect_target_names (folded into the` |
|      - |  1980 | ` * "cannot target" diagnostic below) and __reflect_has_deprecated, which had` |
|      - |  1981 | ` * already lost its last caller when isDeprecated() became C.` |
|      - |  1982 | ` *` |
|      - |  1983 | ` * An instance holds a SPEC, not the arguments: [kind, target, member,` |
|      - |  1984 | ` * paramIdx] names something the engine can reopen, and getArguments()` |
|      - |  1985 | ` * evaluates the recorded expressions on every call, because php does too --` |
|      - |  1986 | `` * `#[A(self::X)]` is php code and it runs when it is asked for. That is also`` |
|      - |  1987 | ` * why the prelude needed a public __init(): PHP could not fill a fresh object` |
|      - |  1988 | ` * any other way. C fills it directly, so __init() (and the` |
|      - |  1989 | ` * __reflect_new_no_ctor that built the empty shell) are gone with it.` |
|      - |  1990 | ` * ---------------------------------------------------------------------------` |
|      - |  1991 | ` */` |
|      - |  1992 | ``#define RA_NAME   "name"      /* php declares this one PUBLIC: `$attr->name` */`` |
|      - |  1993 | `#define RA_SPEC   "__spec"    /* [kind, target, member, paramIdx] */` |
|      - |  1994 | `#define RA_IDX    "__idx"     /* which of that target's attributes this is */` |
|      - |  1995 | `#define RA_TARGET "__target"  /* the Attribute::TARGET_* bit it was found on */` |
|      - |  1996 | `#define RA_REP    "__rep"     /* the target carries more than one of this name */` |
|      - |  1997 |  |
|      - |  1998 | `/* Bound on the value exporter's own recursion. An attribute argument cannot be` |
|      - |  1999 | ` * cyclic, but an OBJECT argument's property graph can be. */` |
|      - |  2000 | `#define REFLECT_EXPORT_MAX_DEPTH 31` |
|      - |  2001 |  |
|      - |  2002 | `/* php's Attribute::TARGET_* names, in bit order (TARGET_CLASS is bit 0). */` |
|      - |  2003 | `static const char *const azReflectTarget[] = {` |
|      - |  2004 | `	"class", "function", "method", "property", "class constant", "parameter", "constant"` |
|      - |  2005 | `};` |
|      - |  2006 |  |
|      - |  2007 | `static void ReflectExportValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth);` |
|      - |  2008 |  |
|      - |  2009 | `/*` |
|      - |  2010 | ` * A string the way php's reflection prints one, in either of php's TWO quotings.` |
|      - |  2011 | ` *` |
|      - |  2012 | ` * A USERLAND default prints single-quoted, with the NON-PRINTABLE bytes escaped` |
|      - |  2013 | ` * (php's smart_str_append_escaped) and the quote itself NOT escaped — php's own` |
|      - |  2014 | ` * output for "q'q" is 'q'q'. An INTERNAL one prints DOUBLE-quoted and escapes the` |
|      - |  2015 | `` * quote: php answers `string $enclosure = "\""` for str_getcsv where PHL used to`` |
|      - |  2016 | `` * answer `'"'`. Every internal function with a string default was affected; the`` |
|      - |  2017 | `` * pair was found on Closure::bindTo's `= "static"` while making that class native.`` |
|      - |  2018 | ` */` |
|    392 |  2019 | `static void ReflectExportStrQ(SyBlob *pOut, const char *zIn, sxu32 nIn, char cQuote)` |
|      4 |  2020 | `{` |
|      - |  2021 | `	static const char zHexDigit[] = "0123456789ABCDEF";` |
|      - |  2022 | `	sxu32 i;` |
|    396 |  2023 | `	SyBlobAppend(pOut, &cQuote, sizeof(char));` |
|    738 |  2024 | `	for( i = 0 ; i < nIn ; i++ ){` |
|    346 |  2025 | `		unsigned char c = (unsigned char)zIn[i];` |
|    346 |  2026 | `		if( c == (unsigned char)cQuote && cQuote == '"' ){` |
|     22 |  2027 | `			SyBlobAppend(pOut, "\\\"", sizeof("\\\"")-1);` |
|     22 |  2028 | `			continue;` |
|      - |  2029 | `		}` |
|      - |  2030 | `		{` |
|    326 |  2031 | `		const char *zEsc = 0;` |
|    326 |  2032 | `		switch( c ){` |
|     13 |  2033 | `			case 0x09: zEsc = "\\t"; break;` |
|     30 |  2034 | `			case 0x0A: zEsc = "\\n"; break;` |
|     13 |  2035 | `			case 0x0B: zEsc = "\\v"; break;` |
|      5 |  2036 | `			case 0x0C: zEsc = "\\f"; break;` |
|     15 |  2037 | `			case 0x0D: zEsc = "\\r"; break;` |
|      3 |  2038 | `			case 0x1B: zEsc = "\\e"; break;` |
|     24 |  2039 | `			case '\\': zEsc = "\\\\"; break;` |
|    228 |  2040 | `			default:   break;` |
|      - |  2041 | `		}` |
|    326 |  2042 | `		if( zEsc ){` |
|     96 |  2043 | `			SyBlobAppend(pOut, zEsc, sizeof("\\t")-1);` |
|    286 |  2044 | `		}else if( c < 0x20 \|\| c > 0x7E ){` |
|      - |  2045 | `			char zHex[4];` |
|     15 |  2046 | `			zHex[0] = '\\';` |
|     15 |  2047 | `			zHex[1] = 'x';` |
|     15 |  2048 | `			zHex[2] = zHexDigit[(c >> 4) & 0x0F];` |
|     15 |  2049 | `			zHex[3] = zHexDigit[c & 0x0F];` |
|     15 |  2050 | `			SyBlobAppend(pOut, zHex, sizeof(zHex));` |
|      8 |  2051 | `		}else{` |
|    218 |  2052 | `			SyBlobAppend(pOut, (const char *)&c, sizeof(char));` |
|      - |  2053 | `		}` |
|      - |  2054 | `		}` |
|    165 |  2055 | `	}` |
|    396 |  2056 | `	SyBlobAppend(pOut, &cQuote, sizeof(char));` |
|    396 |  2057 | `}` |
|      - |  2058 | `/* php's userland quoting, which is what every existing caller means. */` |
|    186 |  2059 | `static void ReflectExportStr(SyBlob *pOut, const char *zIn, sxu32 nIn)` |
|      1 |  2060 | `{` |
|    187 |  2061 | `	ReflectExportStrQ(pOut, zIn, nIn, '\'');` |
|    187 |  2062 | `}` |
|      - |  2063 | `/*` |
|      - |  2064 | ` * A float the way php's reflection prints one: the plain string cast (which` |
|      - |  2065 | `` * PHL already renders php's way, 1.0E+15 and all), then php's `zero_frac` —`` |
|      - |  2066 | ` * a float that came out without a fraction gets ".0" so 1.0 does not print as` |
|      - |  2067 | ` * 1. INF and NAN render as words and are left alone.` |
|      - |  2068 | ` */` |
|     22 |  2069 | `static void ReflectExportReal(ph7_vm *pVm, SyBlob *pOut, ph7_real rVal)` |
|      1 |  2070 | `{` |
|      - |  2071 | `	ph7_value sTmp;` |
|      - |  2072 | `	const char *zText;` |
|     23 |  2073 | `	int nText, i, bPlain = 1;` |
|     23 |  2074 | `	PH7_MemObjInitFromReal(pVm, &sTmp, rVal);` |
|     23 |  2075 | `	zText = ph7_value_to_string(&sTmp, &nText);` |
|     23 |  2076 | `	if( nText > 0 ){` |
|     23 |  2077 | `		SyBlobAppend(pOut, zText, (sxu32)nText);` |
|     11 |  2078 | `	}` |
|     51 |  2079 | `	for( i = 0 ; i < nText ; i++ ){` |
|     39 |  2080 | `		if( (zText[i] < '0' \|\| zText[i] > '9') && !(i == 0 && zText[i] == '-') ){` |
|     11 |  2081 | `			bPlain = 0;` |
|     11 |  2082 | `			break;` |
|      - |  2083 | `		}` |
|     15 |  2084 | `	}` |
|     23 |  2085 | `	if( bPlain && nText > 0 ){` |
|     13 |  2086 | `		SyBlobAppend(pOut, ".0", sizeof(".0")-1);` |
|      6 |  2087 | `	}` |
|     23 |  2088 | `	PH7_MemObjRelease(&sTmp);` |
|     23 |  2089 | `}` |
|      - |  2090 | `/*` |
|      - |  2091 | ` * An array the way php's reflection prints one: a LIST — every key an integer,` |
|      - |  2092 | ` * in sequence from zero — prints no keys at all, and anything else prints one` |
|      - |  2093 | `` * for EVERY entry. That is why `[1, 'k' => 2]` comes out as`` |
|      - |  2094 | `` * `[0 => 1, 'k' => 2]` while `[[1], [2 => 3]]` keeps its outer keys silent.`` |
|      - |  2095 | ` */` |
|     36 |  2096 | `static void ReflectExportArray(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth)` |
|      1 |  2097 | `{` |
|     37 |  2098 | `	ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|      - |  2099 | `	ph7_hashmap_node *pEntry;` |
|     37 |  2100 | `	sxi64 iExpect = 0;` |
|      - |  2101 | `	sxu32 n;` |
|     37 |  2102 | `	int bList = 1;` |
|     69 |  2103 | `	for( pEntry = pMap->pFirst, n = 0 ; n < pMap->nEntry && pEntry ; n++ ){` |
|     45 |  2104 | `		if( pEntry->iType == HASHMAP_BLOB_NODE \|\| pEntry->xKey.iKey != iExpect ){` |
|     13 |  2105 | `			bList = 0;` |
|     13 |  2106 | `			break;` |
|      - |  2107 | `		}` |
|     33 |  2108 | `		iExpect++;` |
|     33 |  2109 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     17 |  2110 | `	}` |
|     37 |  2111 | `	SyBlobAppend(pOut, "[", sizeof(char));` |
|     83 |  2112 | `	for( pEntry = pMap->pFirst, n = 0 ; n < pMap->nEntry && pEntry ; n++ ){` |
|     47 |  2113 | `		ph7_value *pMember = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|     47 |  2114 | `		if( n > 0 ){` |
|     17 |  2115 | `			SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|      8 |  2116 | `		}` |
|     47 |  2117 | `		if( !bList ){` |
|     21 |  2118 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|     13 |  2119 | `				ReflectExportStr(pOut, (const char *)SyBlobData(&pEntry->xKey.sKey),` |
|      4 |  2120 | `					SyBlobLength(&pEntry->xKey.sKey));` |
|      5 |  2121 | `			}else{` |
|     13 |  2122 | `				SyBlobFormat(pOut, "%qd", pEntry->xKey.iKey);` |
|      - |  2123 | `			}` |
|     21 |  2124 | `			SyBlobAppend(pOut, " => ", sizeof(" => ")-1);` |
|     10 |  2125 | `		}` |
|     47 |  2126 | `		ReflectExportValue(pCtx, pOut, pMember, iDepth + 1);` |
|     47 |  2127 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     24 |  2128 | `	}` |
|     37 |  2129 | `	SyBlobAppend(pOut, "]", sizeof(char));` |
|     37 |  2130 | `}` |
|      - |  2131 | `/*` |
|      - |  2132 | `` * php's TYPE word in a `Constant [ ... ]` head -- for a CLASS constant, a`` |
|      - |  2133 | ` * global one and an extension's listing alike. 0 means "an object", whose word` |
|      - |  2134 | ` * is its own class name and which only the caller can spell.` |
|      - |  2135 | ` */` |
|   1554 |  2136 | `static const char * ReflectExportTypeWord(ph7_value *pVal)` |
|      2 |  2137 | `{` |
|   1556 |  2138 | `	if( pVal == 0 ){` |
|    ! 0 |  2139 | `		return "null";` |
|      - |  2140 | `	}` |
|   1556 |  2141 | `	if( (pVal->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      3 |  2142 | `		return 0;` |
|      - |  2143 | `	}` |
|   1554 |  2144 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|      3 |  2145 | `		return "null";` |
|      - |  2146 | `	}` |
|   1552 |  2147 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      5 |  2148 | `		return "array";` |
|      - |  2149 | `	}` |
|   1548 |  2150 | `	if( pVal->iFlags & MEMOBJ_RES ){` |
|     13 |  2151 | `		return "resource";` |
|      - |  2152 | `	}` |
|   1536 |  2153 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      5 |  2154 | `		return "bool";` |
|      - |  2155 | `	}` |
|   1532 |  2156 | `	if( pVal->iFlags & MEMOBJ_REAL ){` |
|     11 |  2157 | `		return "float";` |
|      - |  2158 | `	}` |
|   1522 |  2159 | `	if( pVal->iFlags & MEMOBJ_INT ){` |
|   1414 |  2160 | `		return "int";` |
|      - |  2161 | `	}` |
|    110 |  2162 | `	if( pVal->iFlags & MEMOBJ_STRING ){` |
|    110 |  2163 | `		return "string";` |
|      - |  2164 | `	}` |
|    ! 0 |  2165 | `	return "null";` |
|    779 |  2166 | `}` |
|      - |  2167 | ``/* The text php prints between a constant's `{ ` and ` }`. NULL and FALSE are`` |
|      - |  2168 | `` * both nothing at all, an array is `Array` and an object `Object`. */`` |
|   1554 |  2169 | `static void ReflectExportConstValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal)` |
|      2 |  2170 | `{` |
|   1556 |  2171 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|      3 |  2172 | `		return;` |
|      - |  2173 | `	}` |
|   1554 |  2174 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      5 |  2175 | `		SyBlobAppend(pOut, "Array", sizeof("Array")-1);` |
|      5 |  2176 | `		return;` |
|      - |  2177 | `	}` |
|   1550 |  2178 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      3 |  2179 | `		SyBlobAppend(pOut, "Object", sizeof("Object")-1);` |
|      3 |  2180 | `		return;` |
|      - |  2181 | `	}` |
|   1548 |  2182 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      5 |  2183 | `		if( pVal->x.iVal ){` |
|      3 |  2184 | `			SyBlobAppend(pOut, "1", sizeof(char));` |
|      1 |  2185 | `		}` |
|      5 |  2186 | `		return;` |
|      - |  2187 | `	}` |
|      - |  2188 | `	{` |
|      - |  2189 | `		ph7_value sTmp;` |
|      - |  2190 | `		const char *zText;` |
|      - |  2191 | `		int nText;` |
|   1544 |  2192 | `		PH7_MemObjInit(pCtx->pVm, &sTmp);` |
|   1544 |  2193 | `		PH7_MemObjStore(pVal, &sTmp);` |
|   1544 |  2194 | `		zText = ph7_value_to_string(&sTmp, &nText);` |
|   1544 |  2195 | `		if( nText > 0 ){` |
|   1540 |  2196 | `			SyBlobAppend(pOut, zText, (sxu32)nText);` |
|    769 |  2197 | `		}` |
|   1544 |  2198 | `		PH7_MemObjRelease(&sTmp);` |
|      - |  2199 | `	}` |
|    779 |  2200 | `}` |
|      - |  2201 | `/*` |
|      - |  2202 | `` * One value in php's reflection export syntax — the text after `= ` in a`` |
|      - |  2203 | `` * parameter default and inside `Argument #0 [ … ]` in an attribute dump.`` |
|      - |  2204 | ` *` |
|      - |  2205 | ` * The type dispatch is PH7_MemObjDump's, including its REAL-before-INT order:` |
|      - |  2206 | ` * an integer-valued float carries a cached int view too, and php prints 1.0.` |
|      - |  2207 | ` *` |
|      - |  2208 | ` * php renders an OBJECT argument by echoing the source expression it never` |
|      - |  2209 | `` * folded (`new \A(x: 1)`), which needs the AST. PHL evaluates attribute`` |
|      - |  2210 | ` * arguments from byte-code and has only the resulting VALUE, so a non-enum` |
|      - |  2211 | ` * object is rebuilt from its own properties: same shape, not always the same` |
|      - |  2212 | ` * bytes. An enum case — the one object php's own value formatter handles —` |
|      - |  2213 | ` * matches exactly.` |
|      - |  2214 | ` */` |
|    848 |  2215 | `static void ReflectExportValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth)` |
|      3 |  2216 | `{` |
|    851 |  2217 | `	if( pVal == 0 \|\| iDepth > REFLECT_EXPORT_MAX_DEPTH ){` |
|    ! 0 |  2218 | `		SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|    ! 0 |  2219 | `		return;` |
|      - |  2220 | `	}` |
|    851 |  2221 | `	if( (pVal->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      3 |  2222 | `		ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|      3 |  2223 | `		if( pObj->pClass->iFlags & PH7_CLASS_ENUM ){` |
|      3 |  2224 | `			ph7_value *pName = PH7_EnumCaseNameValue(pObj);` |
|      - |  2225 | `			/* php writes the case as the source did, so a namespaced enum comes` |
|      - |  2226 | `			 * out fully qualified (\N\E::One) and a global one bare (E::One).` |
|      - |  2227 | `			 * The separator is the only thing left of that distinction here. */` |
|      3 |  2228 | `			if( SyByteFind(SyStringData(&pObj->pClass->sName),` |
|      4 |  2229 | `				SyStringLength(&pObj->pClass->sName), '\\', 0) == SXRET_OK ){` |
|    ! 0 |  2230 | `				SyBlobAppend(pOut, "\\", sizeof(char));` |
|    ! 0 |  2231 | `			}` |
|      3 |  2232 | `			SyBlobFormat(pOut, "%z::", &pObj->pClass->sName);` |
|      3 |  2233 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|      3 |  2234 | `				SyBlobAppend(pOut, SyBlobData(&pName->sBlob), SyBlobLength(&pName->sBlob));` |
|      1 |  2235 | `			}` |
|      3 |  2236 | `			return;` |
|      - |  2237 | `		}` |
|    ! 0 |  2238 | `		SyBlobFormat(pOut, "new \\%z(", &pObj->pClass->sName);` |
|      - |  2239 | `		{` |
|      - |  2240 | `			SyHashEntry *pEntry;` |
|    ! 0 |  2241 | `			int nWritten = 0;` |
|    ! 0 |  2242 | `			SyHashResetLoopCursor(&pObj->hAttr);` |
|    ! 0 |  2243 | `			while((pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){` |
|    ! 0 |  2244 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - |  2245 | `				ph7_value *pSlot;` |
|    ! 0 |  2246 | `				if( PH7_ATTR_UNPRESENTED(pVmAttr)` |
|    ! 0 |  2247 | `				 \|\| (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) ){` |
|    ! 0 |  2248 | `					continue; /* class-level members are not part of the object */` |
|      - |  2249 | `				}` |
|    ! 0 |  2250 | `				pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|    ! 0 |  2251 | `				if( nWritten++ ){` |
|    ! 0 |  2252 | `					SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|    ! 0 |  2253 | `				}` |
|    ! 0 |  2254 | `				ReflectExportValue(pCtx, pOut, pSlot, iDepth + 1);` |
|    ! 0 |  2255 | `			}` |
|      - |  2256 | `		}` |
|    ! 0 |  2257 | `		SyBlobAppend(pOut, ")", sizeof(char));` |
|    ! 0 |  2258 | `		return;` |
|      - |  2259 | `	}` |
|    849 |  2260 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|     31 |  2261 | `		SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|     31 |  2262 | `		return;` |
|      - |  2263 | `	}` |
|    819 |  2264 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|     37 |  2265 | `		ReflectExportArray(pCtx, pOut, pVal, iDepth);` |
|     37 |  2266 | `		return;` |
|      - |  2267 | `	}` |
|    783 |  2268 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|    119 |  2269 | `		if( pVal->x.iVal != 0 ){` |
|     53 |  2270 | `			SyBlobAppend(pOut, "true", sizeof("true")-1);` |
|     28 |  2271 | `		}else{` |
|     69 |  2272 | `			SyBlobAppend(pOut, "false", sizeof("false")-1);` |
|      - |  2273 | `		}` |
|    119 |  2274 | `		return;` |
|      - |  2275 | `	}` |
|    667 |  2276 | `	if( pVal->iFlags & MEMOBJ_REAL ){` |
|     23 |  2277 | `		ReflectExportReal(pCtx->pVm, pOut, pVal->rVal);` |
|     23 |  2278 | `		return;` |
|      - |  2279 | `	}` |
|    645 |  2280 | `	if( pVal->iFlags & MEMOBJ_INT ){` |
|    467 |  2281 | `		SyBlobFormat(pOut, "%qd", pVal->x.iVal);` |
|    467 |  2282 | `		return;` |
|      - |  2283 | `	}` |
|    179 |  2284 | `	if( pVal->iFlags & MEMOBJ_STRING ){` |
|    179 |  2285 | `		ReflectExportStr(pOut, (const char *)SyBlobData(&pVal->sBlob), SyBlobLength(&pVal->sBlob));` |
|    179 |  2286 | `		return;` |
|      - |  2287 | `	}` |
|      - |  2288 | `	/* A resource, and anything else php has no export syntax for. */` |
|    ! 0 |  2289 | `	SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|    427 |  2290 | `}` |
|      - |  2291 | `/*` |
|      - |  2292 | ` * Build one ReflectionAttribute. pSpec is shared by every attribute of the` |
|      - |  2293 | ` * same target — it says how to reopen it, not which one this is.` |
|      - |  2294 | ` */` |
|    180 |  2295 | `static ph7_class_instance * ReflectAttrNew(ph7_context *pCtx, SyString *pName,` |
|      - |  2296 | `	ph7_value *pSpec, sxu32 nIdx, int iTargetBit, int bRepeated)` |
|      4 |  2297 | `{` |
|    184 |  2298 | `	ph7_vm *pVm = pCtx->pVm;` |
|    184 |  2299 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, "ReflectionAttribute",` |
|      - |  2300 | `		sizeof("ReflectionAttribute")-1, FALSE, 0);` |
|    184 |  2301 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|    184 |  2302 | `	if( pObj == 0 ){` |
|    ! 0 |  2303 | `		return 0;` |
|      - |  2304 | `	}` |
|    184 |  2305 | `	PH7_NativeSetAttrStr(pVm, pObj, RA_NAME, SyStringData(pName), (int)SyStringLength(pName));` |
|    184 |  2306 | `	PH7_NativeSetProp(pVm, pObj, RA_SPEC, sizeof(RA_SPEC)-1, pSpec);` |
|    184 |  2307 | `	PH7_NativeSetAttrInt(pVm, pObj, RA_IDX, (sxi64)nIdx);` |
|    184 |  2308 | `	PH7_NativeSetAttrInt(pVm, pObj, RA_TARGET, iTargetBit);` |
|    184 |  2309 | `	PH7_NativeSetAttrBool(pVm, pObj, RA_REP, bRepeated);` |
|    184 |  2310 | `	return pObj;` |
|     94 |  2311 | `}` |
|      - |  2312 | `/*` |
|      - |  2313 | ` * Unpack the receiver's spec into the four-value vector ReflectAttrArgs takes.` |
|      - |  2314 | ` * Returns 0 when the object is not one this file built.` |
|      - |  2315 | ` */` |
|    110 |  2316 | `static int ReflectAttrSpec(ph7_context *pCtx, ph7_class_instance *pThis, ph7_value **apOut)` |
|      4 |  2317 | `{` |
|    114 |  2318 | `	ph7_value *pSpec = pThis ? PH7_NativeAttr(pThis, RA_SPEC) : 0;` |
|      - |  2319 | `	ph7_hashmap *pMap;` |
|      - |  2320 | `	int i;` |
|    114 |  2321 | `	if( pSpec == 0 \|\| (pSpec->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 |  2322 | `		return 0;` |
|      - |  2323 | `	}` |
|    114 |  2324 | `	pMap = (ph7_hashmap *)pSpec->x.pOther;` |
|    554 |  2325 | `	for( i = 0 ; i < 4 ; i++ ){` |
|      - |  2326 | `		ph7_value sKey;` |
|    444 |  2327 | `		ph7_hashmap_node *pNode = 0;` |
|      - |  2328 | `		sxi32 rc;` |
|    444 |  2329 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sKey, i);` |
|    444 |  2330 | `		rc = PH7_HashmapLookup(pMap, &sKey, &pNode);` |
|    444 |  2331 | `		PH7_MemObjRelease(&sKey);` |
|    444 |  2332 | `		if( rc != SXRET_OK \|\| pNode == 0 ){` |
|    ! 0 |  2333 | `			return 0;` |
|      - |  2334 | `		}` |
|    444 |  2335 | `		apOut[i] = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pNode->nValIdx);` |
|    444 |  2336 | `		if( apOut[i] == 0 ){` |
|    ! 0 |  2337 | `			return 0;` |
|      - |  2338 | `		}` |
|    224 |  2339 | `	}` |
|    114 |  2340 | `	return 1;` |
|     59 |  2341 | `}` |
|      - |  2342 | `/* The receiver's evaluated arguments, or an empty array when the target no` |
|      - |  2343 | `` * longer resolves (what the chunk's `$a === null ? array() : $a` said). */`` |
|    110 |  2344 | `static ph7_value * ReflectAttrOwnArgs(ph7_context *pCtx, ph7_class_instance *pThis)` |
|      4 |  2345 | `{` |
|      - |  2346 | `	ph7_value *apSpec[4];` |
|    114 |  2347 | `	ph7_value *pArgs = 0;` |
|    114 |  2348 | `	if( pThis && ReflectAttrSpec(pCtx, pThis, apSpec) ){` |
|    114 |  2349 | `		pArgs = ReflectAttrArgs(pCtx, apSpec, (sxu32)PH7_NativeAttrInt(pThis, RA_IDX));` |
|     55 |  2350 | `	}` |
|    114 |  2351 | `	return pArgs ? pArgs : ph7_context_new_array(pCtx);` |
|      4 |  2352 | `}` |
|     64 |  2353 | `static int vm_builtin_ReflectionAttribute_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  2354 | `{` |
|     68 |  2355 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     68 |  2356 | `	const char *zName = "";` |
|     68 |  2357 | `	int nName = 0;` |
|     32 |  2358 | `	SXUNUSED(nArg);` |
|     32 |  2359 | `	SXUNUSED(apArg);` |
|     68 |  2360 | `	if( pThis ){` |
|     68 |  2361 | `		PH7_NativeAttrStr(pThis, RA_NAME, &zName, &nName);` |
|     32 |  2362 | `	}` |
|     68 |  2363 | `	ph7_result_string(pCtx, zName, nName);` |
|     68 |  2364 | `	return PH7_OK;` |
|      4 |  2365 | `}` |
|     24 |  2366 | `static int vm_builtin_ReflectionAttribute_getTarget(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2367 | `{` |
|     25 |  2368 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     12 |  2369 | `	SXUNUSED(nArg);` |
|     12 |  2370 | `	SXUNUSED(apArg);` |
|     25 |  2371 | `	ph7_result_int64(pCtx, pThis ? PH7_NativeAttrInt(pThis, RA_TARGET) : 0);` |
|     25 |  2372 | `	return PH7_OK;` |
|      1 |  2373 | `}` |
|     18 |  2374 | `static int vm_builtin_ReflectionAttribute_isRepeated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2375 | `{` |
|     19 |  2376 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      9 |  2377 | `	SXUNUSED(nArg);` |
|      9 |  2378 | `	SXUNUSED(apArg);` |
|     19 |  2379 | `	ph7_result_bool(pCtx, pThis != 0 && PH7_NativeAttrTruthy(pThis, RA_REP));` |
|     19 |  2380 | `	return PH7_OK;` |
|      1 |  2381 | `}` |
|     40 |  2382 | `static int vm_builtin_ReflectionAttribute_getArguments(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  2383 | `{` |
|     43 |  2384 | `	ph7_value *pArgs = ReflectAttrOwnArgs(pCtx, PH7_ContextThis(pCtx));` |
|     20 |  2385 | `	SXUNUSED(nArg);` |
|     20 |  2386 | `	SXUNUSED(apArg);` |
|     43 |  2387 | `	if( pArgs == 0 ){` |
|    ! 0 |  2388 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  2389 | `	}` |
|     43 |  2390 | `	ph7_result_value(pCtx, pArgs);` |
|     43 |  2391 | `	return PH7_OK;` |
|     23 |  2392 | `}` |
|      - |  2393 | `/*` |
|      - |  2394 | ` * newInstance(): the four screens php runs before it constructs anything —` |
|      - |  2395 | ` * the attribute class exists, it is declared #[Attribute], that declaration` |
|      - |  2396 | ` * allows the target this was found on, and it allows repetition if it was` |
|      - |  2397 | ` * repeated. The messages are php's, byte for byte.` |
|      - |  2398 | ` */` |
|     80 |  2399 | `static int vm_builtin_ReflectionAttribute_newInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  2400 | `{` |
|     84 |  2401 | `	ph7_vm *pVm = pCtx->pVm;` |
|     84 |  2402 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     84 |  2403 | `	ph7_value *pNameVal = pThis ? PH7_NativeAttr(pThis, RA_NAME) : 0;` |
|      - |  2404 | `	ph7_class *pClass;` |
|      - |  2405 | `	ph7_attribute *aA;` |
|      - |  2406 | `	ph7_value *pDeclArgs;` |
|     84 |  2407 | `	sxu32 n, nDecl = 0;` |
|     84 |  2408 | `	int bDecl = 0, iTarget, iBit;` |
|     84 |  2409 | `	sxi64 iFlags = 127; /* php's TARGET_ALL: #[Attribute] with no argument */` |
|     40 |  2410 | `	SXUNUSED(nArg);` |
|     40 |  2411 | `	SXUNUSED(apArg);` |
|     84 |  2412 | `	if( pNameVal == 0 ){` |
|    ! 0 |  2413 | `		ph7_result_null(pCtx);` |
|    ! 0 |  2414 | `		return PH7_OK;` |
|      - |  2415 | `	}` |
|     84 |  2416 | `	pClass = ReflectResolveClass(pVm, pNameVal);` |
|     84 |  2417 | `	if( pClass == 0 ){` |
|      - |  2418 | `		SyString sName;` |
|      5 |  2419 | `		SyStringInitFromBuf(&sName, SyBlobData(&pNameVal->sBlob), SyBlobLength(&pNameVal->sBlob));` |
|      5 |  2420 | `		return PH7_VmThrowException(pCtx, "Error", "Attribute class \"%z\" not found", &sName);` |
|      - |  2421 | `	}` |
|      - |  2422 | `	/* Which #[...] on the attribute class is its own #[Attribute] declaration */` |
|     80 |  2423 | `	aA = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);` |
|     80 |  2424 | `	for( n = 0 ; n < SySetUsed(&pClass->aAttrs) ; n++ ){` |
|     72 |  2425 | `		if( SyStringLength(&aA[n].sName) == sizeof("Attribute")-1` |
|     76 |  2426 | `		 && SyStrnicmp(SyStringData(&aA[n].sName), "Attribute", sizeof("Attribute")-1) == 0 ){` |
|     76 |  2427 | `			nDecl = n;` |
|     76 |  2428 | `			bDecl = 1;` |
|     76 |  2429 | `			break;` |
|      - |  2430 | `		}` |
|    ! 0 |  2431 | `	}` |
|     80 |  2432 | `	if( !bDecl ){` |
|      7 |  2433 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      2 |  2434 | `			"Attempting to use non-attribute class \"%z\" as attribute", &pClass->sDisp);` |
|      - |  2435 | `	}` |
|      - |  2436 | ``	/* Its argument is the target mask: positional, or named `flags:`. */`` |
|      - |  2437 | `	{` |
|      - |  2438 | `		ph7_value *apSpec[4];` |
|     76 |  2439 | `		ph7_value *pKind = ph7_context_new_scalar(pCtx);` |
|     76 |  2440 | `		ph7_value *pMem  = ph7_context_new_scalar(pCtx);` |
|     76 |  2441 | `		ph7_value *pIdx  = ph7_context_new_scalar(pCtx);` |
|     76 |  2442 | `		if( pKind == 0 \|\| pMem == 0 \|\| pIdx == 0 ){` |
|    ! 0 |  2443 | `			return PH7_ContextMemoryError(pCtx);` |
|      - |  2444 | `		}` |
|     76 |  2445 | `		ph7_value_string(pKind, "class", sizeof("class")-1);` |
|     76 |  2446 | `		ph7_value_null(pMem);` |
|     76 |  2447 | `		ph7_value_int(pIdx, 0);` |
|     76 |  2448 | `		apSpec[0] = pKind;` |
|     76 |  2449 | `		apSpec[1] = pNameVal;` |
|     76 |  2450 | `		apSpec[2] = pMem;` |
|     76 |  2451 | `		apSpec[3] = pIdx;` |
|     76 |  2452 | `		pDeclArgs = ReflectAttrArgs(pCtx, apSpec, nDecl);` |
|      - |  2453 | `	}` |
|     76 |  2454 | `	if( pDeclArgs ){` |
|     76 |  2455 | `		ph7_value *pFlags = ph7_array_fetch(pDeclArgs, "0", -1);` |
|     76 |  2456 | `		if( pFlags == 0 ){` |
|     20 |  2457 | `			pFlags = ph7_array_fetch(pDeclArgs, "flags", -1);` |
|      9 |  2458 | `		}` |
|     76 |  2459 | `		if( pFlags ){` |
|     58 |  2460 | `			iFlags = ph7_value_to_int64(pFlags);` |
|     27 |  2461 | `		}` |
|     36 |  2462 | `	}` |
|     76 |  2463 | `	iTarget = (int)PH7_NativeAttrInt(pThis, RA_TARGET);` |
|      - |  2464 | `	/* php's INTERNAL attributes carry a validator beside their mask, and the` |
|      - |  2465 | `	 * engine runs BOTH where the declaration compiles (GenStateCheckAttrPlacement)` |
|      - |  2466 | ``	 * -- so a misplaced `#[\Deprecated]` or `#[\Override]` never reaches this`` |
|      - |  2467 | `	 * far. What is left here is the USERLAND rule, which php checks only when` |
|      - |  2468 | `	 * someone asks: the mask below and the repetition test after it. */` |
|     76 |  2469 | `	if( (iFlags & iTarget) == 0 ){` |
|      - |  2470 | `		SyBlob sAllowed;` |
|      - |  2471 | `		sxi32 rc;` |
|     10 |  2472 | `		SyBlobInit(&sAllowed, &pVm->sAllocator);` |
|     66 |  2473 | `		for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azReflectTarget) ; iBit++ ){` |
|     58 |  2474 | `			if( (iFlags & ((sxi64)1 << iBit)) == 0 ){` |
|     50 |  2475 | `				continue;` |
|      - |  2476 | `			}` |
|     10 |  2477 | `			if( SyBlobLength(&sAllowed) > 0 ){` |
|    ! 0 |  2478 | `				SyBlobAppend(&sAllowed, ", ", sizeof(", ")-1);` |
|    ! 0 |  2479 | `			}` |
|     14 |  2480 | `			SyBlobAppend(&sAllowed, azReflectTarget[iBit],` |
|      8 |  2481 | `				(sxu32)SyStrlen(azReflectTarget[iBit]));` |
|      6 |  2482 | `		}` |
|     10 |  2483 | `		SyBlobAppend(&sAllowed, "", sizeof(char)); /* NUL for the %s below */` |
|     22 |  2484 | `		for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azReflectTarget) ; iBit++ ){` |
|     22 |  2485 | `			if( iTarget == (1 << iBit) ){` |
|     10 |  2486 | `				break;` |
|      - |  2487 | `			}` |
|      8 |  2488 | `		}` |
|     14 |  2489 | `		rc = PH7_VmThrowException(pCtx, "Error",` |
|      4 |  2490 | `			"Attribute \"%z\" cannot target %s (allowed targets: %s)", &pClass->sDisp,` |
|      4 |  2491 | `			iBit < (int)SX_ARRAYSIZE(azReflectTarget) ? azReflectTarget[iBit] : "",` |
|      4 |  2492 | `			SyBlobData(&sAllowed));` |
|     10 |  2493 | `		SyBlobRelease(&sAllowed);` |
|     10 |  2494 | `		return rc;` |
|      - |  2495 | `	}` |
|     68 |  2496 | `	if( PH7_NativeAttrTruthy(pThis, RA_REP) && (iFlags & 128) == 0 ){` |
|     11 |  2497 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      3 |  2498 | `			"Attribute \"%z\" must not be repeated", &pClass->sDisp);` |
|      - |  2499 | `	}` |
|     62 |  2500 | `	return ReflectAttrInstantiate(pCtx, pNameVal, ReflectAttrOwnArgs(pCtx, pThis));` |
|     44 |  2501 | `}` |
|      - |  2502 | `/*` |
|      - |  2503 | `` * php's export text. A bare `Attribute [ Name ]` when there are no arguments;`` |
|      - |  2504 | ` * otherwise the same head followed by the argument block, each argument` |
|      - |  2505 | `` * rendered in php's value-export syntax and named ones as `name = value`.`` |
|      - |  2506 | ` */` |
|     12 |  2507 | `static int vm_builtin_ReflectionAttribute_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2508 | `{` |
|     13 |  2509 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 |  2510 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     13 |  2511 | `	ph7_value *pArgs = ReflectAttrOwnArgs(pCtx, pThis);` |
|     13 |  2512 | `	const char *zName = "";` |
|     13 |  2513 | `	int nName = 0;` |
|      - |  2514 | `	SyBlob sOut;` |
|     13 |  2515 | `	sxu32 nCount = 0;` |
|      6 |  2516 | `	SXUNUSED(nArg);` |
|      6 |  2517 | `	SXUNUSED(apArg);` |
|     13 |  2518 | `	if( pThis ){` |
|     13 |  2519 | `		PH7_NativeAttrStr(pThis, RA_NAME, &zName, &nName);` |
|      6 |  2520 | `	}` |
|     13 |  2521 | `	if( pArgs && (pArgs->iFlags & MEMOBJ_HASHMAP) ){` |
|     13 |  2522 | `		nCount = ((ph7_hashmap *)pArgs->x.pOther)->nEntry;` |
|      6 |  2523 | `	}` |
|     13 |  2524 | `	SyBlobInit(&sOut, &pVm->sAllocator);` |
|     13 |  2525 | `	SyBlobAppend(&sOut, "Attribute [ ", sizeof("Attribute [ ")-1);` |
|     13 |  2526 | `	if( nName > 0 ){` |
|     13 |  2527 | `		SyBlobAppend(&sOut, zName, (sxu32)nName);` |
|      6 |  2528 | `	}` |
|     13 |  2529 | `	SyBlobAppend(&sOut, " ]", sizeof(" ]")-1);` |
|     13 |  2530 | `	if( nCount < 1 ){` |
|      3 |  2531 | `		SyBlobAppend(&sOut, "\n", sizeof(char));` |
|      2 |  2532 | `	}else{` |
|     11 |  2533 | `		ph7_hashmap *pMap = (ph7_hashmap *)pArgs->x.pOther;` |
|     11 |  2534 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|      - |  2535 | `		sxu32 n;` |
|     11 |  2536 | `		SyBlobFormat(&sOut, " {\n  - Arguments [%u] {\n", nCount);` |
|     81 |  2537 | `		for( n = 0 ; n < nCount && pEntry ; n++ ){` |
|     71 |  2538 | `			ph7_value *pMember = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, pEntry->nValIdx);` |
|     71 |  2539 | `			SyBlobFormat(&sOut, "    Argument #%u [ ", n);` |
|     71 |  2540 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      7 |  2541 | `				SyBlobAppend(&sOut, SyBlobData(&pEntry->xKey.sKey),` |
|      2 |  2542 | `					SyBlobLength(&pEntry->xKey.sKey));` |
|      5 |  2543 | `				SyBlobAppend(&sOut, " = ", sizeof(" = ")-1);` |
|      2 |  2544 | `			}` |
|     71 |  2545 | `			ReflectExportValue(pCtx, &sOut, pMember, 0);` |
|     71 |  2546 | `			SyBlobAppend(&sOut, " ]\n", sizeof(" ]\n")-1);` |
|     71 |  2547 | `			pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     36 |  2548 | `		}` |
|     11 |  2549 | `		SyBlobAppend(&sOut, "  }\n}\n", sizeof("  }\n}\n")-1);` |
|      - |  2550 | `	}` |
|     13 |  2551 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     13 |  2552 | `	SyBlobRelease(&sOut);` |
|     13 |  2553 | `	return PH7_OK;` |
|      1 |  2554 | `}` |
|      - |  2555 | `/* The body of the two methods php declares PRIVATE and never calls. Neither is` |
|      - |  2556 | `` * reachable from php code: `new ReflectionAttribute` is the engine's own "Call`` |
|      - |  2557 | `` * to private … from global scope" Error, and `clone` is refused by`` |
|      - |  2558 | ` * PH7_CLASS_NOCLONE before any body runs. They exist so the class REPORTS them,` |
|      - |  2559 | ` * which is the only way php's own dump shows them too. */` |
|    ! 0 |  2560 | `static int vm_builtin_ReflectionAttribute_private(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  2561 | `{` |
|    ! 0 |  2562 | `	SXUNUSED(nArg);` |
|    ! 0 |  2563 | `	SXUNUSED(apArg);` |
|    ! 0 |  2564 | `	SXUNUSED(pCtx);` |
|    ! 0 |  2565 | `	return PH7_OK;` |
|    ! 0 |  2566 | `}` |
|      - |  2567 | `/*` |
|      - |  2568 | ` * Declare ReflectionAttribute. Called from PH7_VmInstallReflection() where` |
|      - |  2569 | ` * chunk 7 used to be compiled, so Reflector (chunk 1) already exists.` |
|      - |  2570 | ` *` |
|      - |  2571 | ` * PH7_CLASS_NOCLONE is php's rule for it (php declares __clone private AND` |
|      - |  2572 | ` * refuses the copy), and the class is NOT final — php 8.5 unsealed it.` |
|      - |  2573 | ` */` |
|   7925 |  2574 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionAttribute(ph7_vm *pVm)` |
|      5 |  2575 | `{` |
|      - |  2576 | `	static const PH7_NativeConstDef aConst[] = {` |
|      - |  2577 | `		{ "IS_INSTANCEOF", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|      - |  2578 | `	};` |
|      - |  2579 | `	static const PH7_NativePropDef aProp[] = {` |
|      - |  2580 | `		{ RA_NAME,   PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  2581 | `		/* PHL-only, and PROTECTED so php code cannot reach them: the spec that` |
|      - |  2582 | `		 * reopens the target. php holds the same state on the C struct behind the` |
|      - |  2583 | `		 * object, invisible; PHL has no hidden-slot bit yet (recorded), so these` |
|      - |  2584 | `		 * four still show up in a var_dump where php shows only $name. */` |
|      - |  2585 | `		{ RA_SPEC,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0,  0.0 }, 0 },` |
|      - |  2586 | `		{ RA_IDX,    PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0,  0.0 }, 0 },` |
|      - |  2587 | `		{ RA_TARGET, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0,  0.0 }, 0 },` |
|      - |  2588 | `		{ RA_REP,    PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0,  0.0 }, 0 },` |
|      - |  2589 | `	};` |
|      - |  2590 | `	/* php's own listing order, which is what __toString() prints. */` |
|      - |  2591 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|      - |  2592 | `		{ "getName",      PH7_MOD_PUBLIC,  "", "string", vm_builtin_ReflectionAttribute_getName },` |
|      - |  2593 | `		{ "getTarget",    PH7_MOD_PUBLIC,  "", "int",    vm_builtin_ReflectionAttribute_getTarget },` |
|      - |  2594 | `		{ "isRepeated",   PH7_MOD_PUBLIC,  "", "bool",   vm_builtin_ReflectionAttribute_isRepeated },` |
|      - |  2595 | `		{ "getArguments", PH7_MOD_PUBLIC,  "", "array",  vm_builtin_ReflectionAttribute_getArguments },` |
|      - |  2596 | `		{ "newInstance",  PH7_MOD_PUBLIC,  "", "object", vm_builtin_ReflectionAttribute_newInstance },` |
|      - |  2597 | `		{ "__toString",   PH7_MOD_PUBLIC,  "", "string", vm_builtin_ReflectionAttribute_toString },` |
|      - |  2598 | `		{ "__clone",      PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionAttribute_private },` |
|      - |  2599 | `		{ "__construct",  PH7_MOD_PRIVATE, "", 0,      vm_builtin_ReflectionAttribute_private },` |
|      - |  2600 | `	};` |
|      - |  2601 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  2602 | `		{ "ReflectionAttribute", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  2603 | `		  aMethod, SX_ARRAYSIZE(aMethod), aConst, SX_ARRAYSIZE(aConst),` |
|      - |  2604 | `		  aProp, SX_ARRAYSIZE(aProp), 0, 0, 0 },` |
|      - |  2605 | `	};` |
|   7930 |  2606 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  2607 | `}` |
|      - |  2608 | `/*` |
|      - |  2609 | ` * The shared getAttributes() body.` |
|      - |  2610 | ` *` |
|      - |  2611 | ` * Turns the target's #[...] records into ReflectionAttribute objects, applying` |
|      - |  2612 | ` * php's name / IS_INSTANCEOF filter. Argument VALUES stay lazy — what each` |
|      - |  2613 | ` * object carries is the [kind, target, member, paramIdx] spec that reopens` |
|      - |  2614 | ` * them — so a reflector declaring getAttributes() only has to say WHICH target` |
|      - |  2615 | ` * it is.` |
|      - |  2616 | ` */` |
|    184 |  2617 | `static int ReflectBuildAttrs(ph7_context *pCtx, SySet *pAttrs, const char *zKind,` |
|      - |  2618 | `	ph7_value *pTarget, const char *zMember, int nMember,` |
|      - |  2619 | `	int iParamIdx, int iTargetBit, int nArg, ph7_value **apArg)` |
|      4 |  2620 | `{` |
|    188 |  2621 | `	ph7_vm *pVm = pCtx->pVm;` |
|    188 |  2622 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|    188 |  2623 | `	ph7_class *pFilter = 0;` |
|    188 |  2624 | `	const char *zFilter = 0;` |
|    188 |  2625 | `	int nFilter = 0;` |
|      - |  2626 | `	ph7_value *pSpec, *pOut;` |
|      - |  2627 | `	sxu32 n, i;` |
|    188 |  2628 | `	pOut = ph7_context_new_array(pCtx);` |
|    188 |  2629 | `	pSpec = ph7_context_new_array(pCtx);` |
|    188 |  2630 | `	if( pOut == 0 \|\| pSpec == 0 ){` |
|    ! 0 |  2631 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  2632 | `	}` |
|    188 |  2633 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     15 |  2634 | `		zFilter = ph7_value_to_string(apArg[0], &nFilter);` |
|     15 |  2635 | `		if( nFilter < 1 ){` |
|    ! 0 |  2636 | `			zFilter = 0;` |
|    ! 0 |  2637 | `		}` |
|      - |  2638 | `		/* IS_INSTANCEOF: keep a SUBCLASS of the named attribute too. The exact` |
|      - |  2639 | `		 * name still matches on its own, so this only has to answer for the rest. */` |
|     15 |  2640 | `		if( zFilter && nArg > 1 && (ph7_value_to_int(apArg[1]) & 2) ){` |
|      5 |  2641 | `			pFilter = ReflectResolveClass(pVm, apArg[0]);` |
|      2 |  2642 | `		}` |
|      7 |  2643 | `	}` |
|      - |  2644 | `	{` |
|    188 |  2645 | `		ph7_value *pKind = ph7_context_new_scalar(pCtx);` |
|    188 |  2646 | `		ph7_value *pMem  = ph7_context_new_scalar(pCtx);` |
|    188 |  2647 | `		ph7_value *pIdx  = ph7_context_new_scalar(pCtx);` |
|    188 |  2648 | `		if( pKind == 0 \|\| pMem == 0 \|\| pIdx == 0 ){` |
|    ! 0 |  2649 | `			return PH7_ContextMemoryError(pCtx);` |
|      - |  2650 | `		}` |
|    188 |  2651 | `		ph7_value_string(pKind, zKind, -1);` |
|    188 |  2652 | `		if( zMember ){` |
|     85 |  2653 | `			ph7_value_string(pMem, zMember, nMember);` |
|     44 |  2654 | `		}else{` |
|    106 |  2655 | `			ph7_value_null(pMem);` |
|      - |  2656 | `		}` |
|    188 |  2657 | `		ph7_value_int(pIdx, iParamIdx);` |
|    188 |  2658 | `		ph7_array_add_elem(pSpec, 0, pKind);` |
|      - |  2659 | `		/* The target rides as a VALUE, not a name: a closure's attributes are` |
|      - |  2660 | `		 * reopened through the Closure object itself. */` |
|    188 |  2661 | `		ph7_array_add_elem(pSpec, 0, pTarget);` |
|    188 |  2662 | `		ph7_array_add_elem(pSpec, 0, pMem);` |
|    188 |  2663 | `		ph7_array_add_elem(pSpec, 0, pIdx);` |
|      - |  2664 | `	}` |
|    394 |  2665 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|    210 |  2666 | `		int bRepeated = 0;` |
|    210 |  2667 | `		if( zFilter ){` |
|     89 |  2668 | `			int bKeep = ((int)SyStringLength(&aA[n].sName) == nFilter` |
|     50 |  2669 | `				&& SyStrnicmp(SyStringData(&aA[n].sName), zFilter, (sxu32)nFilter) == 0);` |
|     51 |  2670 | `			if( !bKeep && pFilter ){` |
|     16 |  2671 | `				ph7_class *pCand = PH7_VmExtractClass(pVm, SyStringData(&aA[n].sName),` |
|     10 |  2672 | `					SyStringLength(&aA[n].sName), FALSE, 0);` |
|     11 |  2673 | `				if( pCand == 0 ){` |
|    ! 0 |  2674 | `					pCand = PH7_VmTriggerAutoload(pVm, SyStringData(&aA[n].sName),` |
|    ! 0 |  2675 | `						SyStringLength(&aA[n].sName), FALSE);` |
|    ! 0 |  2676 | `				}` |
|     11 |  2677 | `				bKeep = pCand != 0 && pCand != pFilter && PH7_VmInstanceOf(pCand, pFilter);` |
|      5 |  2678 | `			}` |
|     51 |  2679 | `			if( !bKeep ){` |
|     27 |  2680 | `				continue;` |
|      - |  2681 | `			}` |
|     12 |  2682 | `		}` |
|      - |  2683 | `		/* isRepeated() asks about the TARGET, not about the filtered result:` |
|      - |  2684 | `		 * two #[A] make both of them repeated even when only one is asked for. */` |
|    398 |  2685 | `		for( i = 0 ; i < SySetUsed(pAttrs) ; i++ ){` |
|    254 |  2686 | `			if( i != n && SyStringLength(&aA[i].sName) == SyStringLength(&aA[n].sName)` |
|     74 |  2687 | `			 && SyStrnicmp(SyStringData(&aA[i].sName), SyStringData(&aA[n].sName),` |
|     66 |  2688 | `				SyStringLength(&aA[n].sName)) == 0 ){` |
|     42 |  2689 | `				bRepeated = 1;` |
|     42 |  2690 | `				break;` |
|      - |  2691 | `			}` |
|    111 |  2692 | `		}` |
|    274 |  2693 | `		ReflectTypeListAdd(pCtx, pOut,` |
|    180 |  2694 | `			ReflectAttrNew(pCtx, &aA[n].sName, pSpec, n, iTargetBit, bRepeated));` |
|     94 |  2695 | `	}` |
|    188 |  2696 | `	ph7_result_value(pCtx, pOut);` |
|    188 |  2697 | `	return PH7_OK;` |
|     96 |  2698 | `}` |
|      - |  2699 | `/*` |
|      - |  2700 | ` * The three "no runtime line tracking" methods. php answers a file/line/trace` |
|      - |  2701 | ` * from the executing frame; PHL has no per-instruction line record (the same` |
|      - |  2702 | ` * gap debug_backtrace() has), so it says so loudly rather than inventing one.` |
|      - |  2703 | ` */` |
|    ! 0 |  2704 | `static int ReflectUnsupported(ph7_context *pCtx, const char *zWho)` |
|    ! 0 |  2705 | `{` |
|    ! 0 |  2706 | `	return PH7_VmThrowException(pCtx, "Error",` |
|    ! 0 |  2707 | `		"%s is not supported by PHL (no runtime line tracking)", zWho);` |
|    ! 0 |  2708 | `}` |
|      - |  2709 | `#define REFLECT_UNSUPPORTED(NAME,TEXT) \` |
|      - |  2710 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  2711 | `	{ \` |
|      - |  2712 | `		SXUNUSED(nArg); \` |
|      - |  2713 | `		SXUNUSED(apArg); \` |
|      - |  2714 | `		return ReflectUnsupported(pCtx, TEXT); \` |
|      - |  2715 | `	}` |
|    ! 0 |  2716 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_execLine,"ReflectionGenerator::getExecutingLine()")` |
|    ! 0 |  2717 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_execFile,"ReflectionGenerator::getExecutingFile()")` |
|    ! 0 |  2718 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_trace,"ReflectionGenerator::getTrace()")` |
|    ! 0 |  2719 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_execLine,"ReflectionFiber::getExecutingLine()")` |
|    ! 0 |  2720 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_execFile,"ReflectionFiber::getExecutingFile()")` |
|    ! 0 |  2721 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_trace,"ReflectionFiber::getTrace()")` |
|      - |  2722 |  |
|      - |  2723 | `/* ReflectionGenerator::__construct(Generator $generator) */` |
|     18 |  2724 | `static int vm_builtin_ReflectionGenerator_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2725 | `{` |
|     19 |  2726 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     19 |  2727 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 |  2728 | `		return PH7_OK;` |
|      - |  2729 | `	}` |
|     19 |  2730 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RG_GEN, (ph7_class_instance *)apArg[0]->x.pOther);` |
|     19 |  2731 | `	return PH7_OK;` |
|     10 |  2732 | `}` |
|      - |  2733 | `/* The coroutine context of the wrapped generator, or NULL. */` |
|     36 |  2734 | `static ph7_exec_ctx * ReflectGenExec(ph7_context *pCtx)` |
|      1 |  2735 | `{` |
|     37 |  2736 | `	ph7_class_instance *pGenObj = ReflectWrapped(pCtx, RG_GEN);` |
|      - |  2737 | `	ph7_value sVal;` |
|      - |  2738 | `	ph7_generator *pGen;` |
|     37 |  2739 | `	if( pGenObj == 0 ){` |
|    ! 0 |  2740 | `		return 0;` |
|      - |  2741 | `	}` |
|     37 |  2742 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     37 |  2743 | `	sVal.x.pOther = pGenObj;` |
|     37 |  2744 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|     37 |  2745 | `	pGen = ReflectGeneratorCtx(pCtx->pVm, &sVal);` |
|     37 |  2746 | `	return pGen ? pGen->pCtx : 0;` |
|     19 |  2747 | `}` |
|      8 |  2748 | `static int vm_builtin_ReflectionGenerator_isClosed(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2749 | `{` |
|      9 |  2750 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|      4 |  2751 | `	SXUNUSED(nArg);` |
|      4 |  2752 | `	SXUNUSED(apArg);` |
|     17 |  2753 | `	ph7_result_bool(pCtx, pExec != 0` |
|     12 |  2754 | `		&& (pExec->iState == PH7_CTX_STATE_COMPLETED \|\| pExec->iState == PH7_CTX_STATE_CLOSED));` |
|      9 |  2755 | `	return PH7_OK;` |
|      1 |  2756 | `}` |
|      - |  2757 | `/* ReflectionGenerator::getThis(): ?object — the receiver the generator's body` |
|      - |  2758 | ` * runs against. A coroutine frame installs it as a frame VARIABLE (see` |
|      - |  2759 | ` * VmFiberSetupFrame), not as pFrame->pThis, so both are checked. */` |
|     10 |  2760 | `static int vm_builtin_ReflectionGenerator_getThis(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2761 | `{` |
|     11 |  2762 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|      5 |  2763 | `	SXUNUSED(nArg);` |
|      5 |  2764 | `	SXUNUSED(apArg);` |
|     11 |  2765 | `	if( pExec && pExec->pFrame ){` |
|     11 |  2766 | `		SyHashEntry *pVar = SyHashGet(&pExec->pFrame->hVar, "this", sizeof("this")-1);` |
|     11 |  2767 | `		if( pVar ){` |
|     10 |  2768 | `			ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,` |
|      6 |  2769 | `				(sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|      7 |  2770 | `			if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|      7 |  2771 | `				ph7_result_value(pCtx, pSlot);` |
|      7 |  2772 | `				return PH7_OK;` |
|      - |  2773 | `			}` |
|    ! 0 |  2774 | `		}` |
|      5 |  2775 | `		if( pExec->pFrame->pThis ){` |
|    ! 0 |  2776 | `			return ReflectResultBorrowed(pCtx, pExec->pFrame->pThis);` |
|      - |  2777 | `		}` |
|      2 |  2778 | `	}` |
|      5 |  2779 | `	ph7_result_null(pCtx);` |
|      5 |  2780 | `	return PH7_OK;` |
|      6 |  2781 | `}` |
|      - |  2782 | `/* ReflectionGenerator::getFunction(): ReflectionFunctionAbstract — still a` |
|      - |  2783 | ` * prelude class, so it is built through its own constructor. */` |
|     18 |  2784 | `static int vm_builtin_ReflectionGenerator_getFunction(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2785 | `{` |
|     19 |  2786 | `	ph7_vm *pVm = pCtx->pVm;` |
|     19 |  2787 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|     19 |  2788 | `	ph7_vm_func *pFunc = pExec ? pExec->pFunc : 0;` |
|      - |  2789 | `	ph7_class_instance *pOut;` |
|      - |  2790 | `	ph7_value aArg[2];` |
|     19 |  2791 | `	int nCtorArg = 1;` |
|      - |  2792 | `	sxi32 rc;` |
|      9 |  2793 | `	SXUNUSED(nArg);` |
|      9 |  2794 | `	SXUNUSED(apArg);` |
|     19 |  2795 | `	if( pFunc == 0 ){` |
|    ! 0 |  2796 | `		ph7_result_null(pCtx);` |
|    ! 0 |  2797 | `		return PH7_OK;` |
|      - |  2798 | `	}` |
|     19 |  2799 | `	PH7_MemObjInit(pVm, &aArg[0]);` |
|     19 |  2800 | `	PH7_MemObjInit(pVm, &aArg[1]);` |
|     19 |  2801 | `	if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      9 |  2802 | `		ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|      9 |  2803 | `		ph7_value_string(&aArg[0], SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|      9 |  2804 | `		ph7_value_string(&aArg[1], SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|      9 |  2805 | `		nCtorArg = 2;` |
|      5 |  2806 | `	}else{` |
|     11 |  2807 | `		ph7_value_string(&aArg[0], SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|      - |  2808 | `	}` |
|      - |  2809 | `	{` |
|      - |  2810 | `		ph7_value *apCtor[2];` |
|     19 |  2811 | `		apCtor[0] = &aArg[0];` |
|     19 |  2812 | `		apCtor[1] = &aArg[1];` |
|     28 |  2813 | `		pOut = ReflectConstruct(pCtx, nCtorArg == 2 ? "ReflectionMethod" : "ReflectionFunction",` |
|      9 |  2814 | `			nCtorArg, apCtor, &rc);` |
|      - |  2815 | `	}` |
|     19 |  2816 | `	PH7_MemObjRelease(&aArg[0]);` |
|     19 |  2817 | `	PH7_MemObjRelease(&aArg[1]);` |
|     19 |  2818 | `	if( pOut == 0 ){` |
|    ! 0 |  2819 | `		if( rc != PH7_OK ){` |
|    ! 0 |  2820 | `			return rc;` |
|      - |  2821 | `		}` |
|    ! 0 |  2822 | `		ph7_result_null(pCtx);` |
|    ! 0 |  2823 | `		return PH7_OK;` |
|      - |  2824 | `	}` |
|     19 |  2825 | `	return ReflectResultObject(pCtx, pOut);` |
|     10 |  2826 | `}` |
|      - |  2827 | `` /* ReflectionGenerator::getExecutingGenerator(): Generator — follow `yield from` `` |
|      - |  2828 | ` * delegation to the innermost one that is actually running. */` |
|      6 |  2829 | `static int vm_builtin_ReflectionGenerator_getExecuting(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2830 | `{` |
|      7 |  2831 | `	ph7_vm *pVm = pCtx->pVm;` |
|      7 |  2832 | `	ph7_class_instance *pGenObj = ReflectWrapped(pCtx, RG_GEN);` |
|      - |  2833 | `	ph7_value sVal, *pCur;` |
|      - |  2834 | `	ph7_generator *pGen;` |
|      7 |  2835 | `	int iDepth = 0;` |
|      3 |  2836 | `	SXUNUSED(nArg);` |
|      3 |  2837 | `	SXUNUSED(apArg);` |
|      7 |  2838 | `	if( pGenObj == 0 ){` |
|    ! 0 |  2839 | `		ph7_result_null(pCtx);` |
|    ! 0 |  2840 | `		return PH7_OK;` |
|      - |  2841 | `	}` |
|      7 |  2842 | `	PH7_MemObjInit(pVm, &sVal);` |
|      7 |  2843 | `	sVal.x.pOther = pGenObj;` |
|      7 |  2844 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|      7 |  2845 | `	pCur = &sVal;` |
|      7 |  2846 | `	pGen = ReflectGeneratorCtx(pVm, &sVal);` |
|     12 |  2847 | `	while( pGen && pGen->pCtx && pGen->pCtx->iDelegateState == 3` |
|     10 |  2848 | `	 && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|      3 |  2849 | `		ph7_generator *pInner = ReflectGeneratorCtx(pVm, &pGen->pCtx->sDelegate);` |
|      3 |  2850 | `		if( pInner == 0 ){` |
|    ! 0 |  2851 | `			break;` |
|      - |  2852 | `		}` |
|      3 |  2853 | `		pCur = &pGen->pCtx->sDelegate;` |
|      3 |  2854 | `		pGen = pInner;` |
|      3 |  2855 | `		iDepth++;` |
|      1 |  2856 | `	}` |
|      7 |  2857 | `	return ReflectResultBorrowed(pCtx, (ph7_class_instance *)pCur->x.pOther);` |
|      4 |  2858 | `}` |
|      - |  2859 | `/* ReflectionFiber::__construct(Fiber $fiber) / getFiber() / getCallable() */` |
|      4 |  2860 | `static int vm_builtin_ReflectionFiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2861 | `{` |
|      5 |  2862 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 |  2863 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 |  2864 | `		return PH7_OK;` |
|      - |  2865 | `	}` |
|      5 |  2866 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RF_FIBER, (ph7_class_instance *)apArg[0]->x.pOther);` |
|      5 |  2867 | `	return PH7_OK;` |
|      3 |  2868 | `}` |
|      4 |  2869 | `static int vm_builtin_ReflectionFiber_getFiber(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2870 | `{` |
|      2 |  2871 | `	SXUNUSED(nArg);` |
|      2 |  2872 | `	SXUNUSED(apArg);` |
|      5 |  2873 | `	return ReflectResultBorrowed(pCtx, ReflectWrapped(pCtx, RF_FIBER));` |
|      1 |  2874 | `}` |
|      4 |  2875 | `static int vm_builtin_ReflectionFiber_getCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2876 | `{` |
|      5 |  2877 | `	ph7_class_instance *pFiber = ReflectWrapped(pCtx, RF_FIBER);` |
|      5 |  2878 | `	ph7_value *pVal = pFiber ? PH7_NativeAttr(pFiber, "__callable") : 0;` |
|      2 |  2879 | `	SXUNUSED(nArg);` |
|      2 |  2880 | `	SXUNUSED(apArg);` |
|      5 |  2881 | `	if( pVal ){` |
|      5 |  2882 | `		ph7_result_value(pCtx, pVal);` |
|      2 |  2883 | `	}` |
|      5 |  2884 | `	return PH7_OK;` |
|      1 |  2885 | `}` |
|      - |  2886 | `/* ---- ReflectionConstant ---- */` |
|      - |  2887 | `/* The engine's record for a global constant, or NULL when undefined. */` |
|   1614 |  2888 | `static ph7_constant * ReflectConstEntry(ph7_vm *pVm, const char *zName, int nName)` |
|      3 |  2889 | `{` |
|   1617 |  2890 | `	SyHashEntry *pEntry = nName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zName, (sxu32)nName) : 0;` |
|   1617 |  2891 | `	return pEntry ? (ph7_constant *)pEntry->pUserData : 0;` |
|      3 |  2892 | `}` |
|      - |  2893 | `/* The receiver's constant record, resolved from its public $name. */` |
|    148 |  2894 | `static ph7_constant * ReflectConstOf(ph7_context *pCtx)` |
|      1 |  2895 | `{` |
|    149 |  2896 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    149 |  2897 | `	const char *zName = "";` |
|    149 |  2898 | `	int nName = 0;` |
|    149 |  2899 | `	if( pThis ){` |
|    149 |  2900 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     74 |  2901 | `	}` |
|    149 |  2902 | `	return ReflectConstEntry(pCtx->pVm, zName, nName);` |
|      1 |  2903 | `}` |
|     94 |  2904 | `static int vm_builtin_ReflectionConstant_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2905 | `{` |
|     95 |  2906 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     95 |  2907 | `	int nName = 0;` |
|     95 |  2908 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|     95 |  2909 | `	if( pThis == 0 ){` |
|    ! 0 |  2910 | `		return PH7_OK;` |
|      - |  2911 | `	}` |
|     95 |  2912 | `	if( ReflectConstEntry(pCtx->pVm, zName, nName) == 0 ){` |
|     10 |  2913 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 |  2914 | `			"Constant \"%.*s\" does not exist", nName, zName);` |
|      - |  2915 | `	}` |
|     89 |  2916 | `	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", zName, nName);` |
|     89 |  2917 | `	return PH7_OK;` |
|     48 |  2918 | `}` |
|      4 |  2919 | `static int vm_builtin_ReflectionConstant_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2920 | `{` |
|      5 |  2921 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 |  2922 | `	const char *zName = "";` |
|      5 |  2923 | `	int nName = 0;` |
|      2 |  2924 | `	SXUNUSED(nArg);` |
|      2 |  2925 | `	SXUNUSED(apArg);` |
|      5 |  2926 | `	if( pThis ){` |
|      5 |  2927 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 |  2928 | `	}` |
|      5 |  2929 | `	ph7_result_string(pCtx, zName, nName);` |
|      5 |  2930 | `	return PH7_OK;` |
|      1 |  2931 | `}` |
|      - |  2932 | `/* The namespace split php reports: everything before the last '\', and the rest. */` |
|      8 |  2933 | `static int ReflectConstNsCut(const char *zName, int nName)` |
|      1 |  2934 | `{` |
|      - |  2935 | `	int k;` |
|    101 |  2936 | `	for( k = nName - 1 ; k >= 0 ; k-- ){` |
|     93 |  2937 | `		if( zName[k] == '\\' ){` |
|    ! 0 |  2938 | `			return k;` |
|      - |  2939 | `		}` |
|     47 |  2940 | `	}` |
|      9 |  2941 | `	return -1;` |
|      5 |  2942 | `}` |
|      4 |  2943 | `static int vm_builtin_ReflectionConstant_getNamespaceName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2944 | `{` |
|      5 |  2945 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 |  2946 | `	const char *zName = "";` |
|      5 |  2947 | `	int nName = 0, iCut;` |
|      2 |  2948 | `	SXUNUSED(nArg);` |
|      2 |  2949 | `	SXUNUSED(apArg);` |
|      5 |  2950 | `	if( pThis ){` |
|      5 |  2951 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 |  2952 | `	}` |
|      5 |  2953 | `	iCut = ReflectConstNsCut(zName, nName);` |
|      5 |  2954 | `	ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|      5 |  2955 | `	return PH7_OK;` |
|      1 |  2956 | `}` |
|      4 |  2957 | `static int vm_builtin_ReflectionConstant_getShortName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2958 | `{` |
|      5 |  2959 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 |  2960 | `	const char *zName = "";` |
|      5 |  2961 | `	int nName = 0, iCut;` |
|      2 |  2962 | `	SXUNUSED(nArg);` |
|      2 |  2963 | `	SXUNUSED(apArg);` |
|      5 |  2964 | `	if( pThis ){` |
|      5 |  2965 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 |  2966 | `	}` |
|      5 |  2967 | `	iCut = ReflectConstNsCut(zName, nName);` |
|      5 |  2968 | `	if( iCut < 0 ){` |
|      5 |  2969 | `		ph7_result_string(pCtx, zName, nName);` |
|      3 |  2970 | `	}else{` |
|    ! 0 |  2971 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|      - |  2972 | `	}` |
|      5 |  2973 | `	return PH7_OK;` |
|      1 |  2974 | `}` |
|      8 |  2975 | `static int vm_builtin_ReflectionConstant_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2976 | `{` |
|      9 |  2977 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      - |  2978 | `	ph7_value sValue;` |
|      4 |  2979 | `	SXUNUSED(nArg);` |
|      4 |  2980 | `	SXUNUSED(apArg);` |
|      9 |  2981 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|      9 |  2982 | `	if( pCons && pCons->xExpand ){` |
|      9 |  2983 | `		pCons->xExpand(&sValue, pCons->pUserData);` |
|      4 |  2984 | `	}` |
|      9 |  2985 | `	ph7_result_value(pCtx, &sValue);` |
|      9 |  2986 | `	PH7_MemObjRelease(&sValue);` |
|      9 |  2987 | `	return PH7_OK;` |
|      1 |  2988 | `}` |
|     48 |  2989 | `static int vm_builtin_ReflectionConstant_isDeprecated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2990 | `{` |
|     49 |  2991 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|     24 |  2992 | `	SXUNUSED(nArg);` |
|     24 |  2993 | `	SXUNUSED(apArg);` |
|      - |  2994 | `	/* The same fact the E_DEPRECATED at a NAMING reads, and the same one the` |
|      - |  2995 | `	 * export tags -- reading it here does not raise the notice, which is why` |
|      - |  2996 | `	 * this asks the record rather than expanding the constant. */` |
|     49 |  2997 | `	ph7_result_bool(pCtx, pCons != 0 && pCons->zDeprecated != 0);` |
|     49 |  2998 | `	return PH7_OK;` |
|      1 |  2999 | `}` |
|      8 |  3000 | `static int vm_builtin_ReflectionConstant_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3001 | `{` |
|      9 |  3002 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      4 |  3003 | `	SXUNUSED(nArg);` |
|      4 |  3004 | `	SXUNUSED(apArg);` |
|      9 |  3005 | `	if( pCons && SyStringLength(&pCons->sFile) > 0 ){` |
|      5 |  3006 | `		ph7_result_string(pCtx, SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));` |
|      3 |  3007 | `	}else{` |
|      5 |  3008 | `		ph7_result_bool(pCtx, 0);` |
|      - |  3009 | `	}` |
|      9 |  3010 | `	return PH7_OK;` |
|      1 |  3011 | `}` |
|      - |  3012 | `/* The ReflectionExtension builder every reflector's getExtension() answers with` |
|      - |  3013 | ` * -- defined further down, beside the class partition it reads. */` |
|      - |  3014 | `static int ReflectExtensionOf(ph7_context *pCtx, int iExt);` |
|      - |  3015 | `/* An engine constant belongs to the extension the partition places its name in;` |
|      - |  3016 | ` * a userland define() belongs to none, which php reports as null / false. */` |
|     38 |  3017 | `static int ReflectConstExtId(ph7_context *pCtx)` |
|      1 |  3018 | `{` |
|     39 |  3019 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|     39 |  3020 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     39 |  3021 | `	const char *zName = "";` |
|     39 |  3022 | `	int nName = 0;` |
|     39 |  3023 | `	if( pCons == 0 \|\| pCons->bUserDefined ){` |
|      9 |  3024 | `		return -1;` |
|      - |  3025 | `	}` |
|     31 |  3026 | `	if( pThis ){` |
|     31 |  3027 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     15 |  3028 | `	}` |
|     31 |  3029 | `	return PH7_VmExtOfConstant(zName, nName);` |
|     20 |  3030 | `}` |
|      8 |  3031 | `static int vm_builtin_ReflectionConstant_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3032 | `{` |
|      4 |  3033 | `	SXUNUSED(nArg);` |
|      4 |  3034 | `	SXUNUSED(apArg);` |
|      9 |  3035 | `	return ReflectExtensionOf(pCtx, ReflectConstExtId(pCtx));` |
|      1 |  3036 | `}` |
|     30 |  3037 | `static int vm_builtin_ReflectionConstant_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3038 | `{` |
|     31 |  3039 | `	int iExt = ReflectConstExtId(pCtx);` |
|     15 |  3040 | `	SXUNUSED(nArg);` |
|     15 |  3041 | `	SXUNUSED(apArg);` |
|     31 |  3042 | `	if( iExt < 0 ){` |
|      7 |  3043 | `		ph7_result_bool(pCtx, 0);` |
|      4 |  3044 | `	}else{` |
|     25 |  3045 | `		ph7_result_string(pCtx, PH7_VmExtensionName(iExt), -1);` |
|      - |  3046 | `	}` |
|     31 |  3047 | `	return PH7_OK;` |
|      1 |  3048 | `}` |
|      - |  3049 | `/* getAttributes() still routes through the prelude builder: chunk 7 owns the` |
|      - |  3050 | ` * ReflectionAttribute shape and the lazy argument evaluation. */` |
|      2 |  3051 | `static int vm_builtin_ReflectionConstant_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3052 | `{` |
|      3 |  3053 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      3 |  3054 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      3 |  3055 | `	const char *zName = "";` |
|      3 |  3056 | `	int nName = 0;` |
|      3 |  3057 | `	if( pThis == 0 \|\| pCons == 0 ){` |
|    ! 0 |  3058 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  3059 | `		return PH7_OK;` |
|      - |  3060 | `	}` |
|      3 |  3061 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      - |  3062 | `	{` |
|      - |  3063 | `		ph7_value sTarget;` |
|      - |  3064 | `		int rc;` |
|      3 |  3065 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      3 |  3066 | `		ph7_value_string(&sTarget, zName, nName);` |
|      - |  3067 | `		/* 64 = Attribute::TARGET_CONSTANT */` |
|      4 |  3068 | `		rc = ReflectBuildAttrs(pCtx, &pCons->aAttrs, "const", &sTarget, 0, 0, 0, 64,` |
|      1 |  3069 | `			nArg, apArg);` |
|      3 |  3070 | `		PH7_MemObjRelease(&sTarget);` |
|      3 |  3071 | `		return rc;` |
|      - |  3072 | `	}` |
|      2 |  3073 | `}` |
|      - |  3074 | `/* php's ZEND_ACC_NO_FILE_CACHE: the two constants whose value belongs to the` |
|      - |  3075 | ` * RUNNING process rather than to the build, so opcache must not bake them in. */` |
|    872 |  3076 | `static int ReflectConstNoFileCache(const SyString *pName)` |
|      1 |  3077 | `{` |
|      - |  3078 | `	static const char * const azNoCache[] = { "PHP_BINARY", "PHP_SAPI" };` |
|      - |  3079 | `	sxu32 n;` |
|   2609 |  3080 | `	for( n = 0 ; n < SX_ARRAYSIZE(azNoCache) ; ++n ){` |
|   1742 |  3081 | `		if( SyStringLength(pName) == SyStrlen(azNoCache[n])` |
|    932 |  3082 | `		 && SyMemcmp(SyStringData(pName), azNoCache[n], SyStringLength(pName)) == 0 ){` |
|      7 |  3083 | `			return 1;` |
|      - |  3084 | `		}` |
|    869 |  3085 | `	}` |
|    867 |  3086 | `	return 0;` |
|    437 |  3087 | `}` |
|      - |  3088 | `/*` |
|      - |  3089 | `` * php's `Constant [ <persistent> int JSON_HEX_TAG ] { 1 }` -- one line, and the`` |
|      - |  3090 | `` * same one an extension's Constants block lists. `<persistent>` is the ENGINE's`` |
|      - |  3091 | ` * own: a define()d constant carries no tag at all, and one php deprecated the` |
|      - |  3092 | `` * SYMBOL of reads `<persistent, deprecated>`. The value is taken from the`` |
|      - |  3093 | ` * constant's expander DIRECTLY, so asking for the export does not raise the` |
|      - |  3094 | ` * E_DEPRECATED that naming it would.` |
|      - |  3095 | ` */` |
|    916 |  3096 | `static void ReflectExportGlobalConstLine(ph7_context *pCtx, SyBlob *pOut, ph7_constant *pCons)` |
|      1 |  3097 | `{` |
|      - |  3098 | `	ph7_value sVal;` |
|      - |  3099 | `	const char *zType;` |
|    917 |  3100 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    917 |  3101 | `	if( pCons->xExpand ){` |
|    917 |  3102 | `		pCons->xExpand(&sVal, pCons->pUserData);` |
|    458 |  3103 | `	}` |
|    917 |  3104 | `	SyBlobAppend(pOut, "Constant [ ", sizeof("Constant [ ")-1);` |
|    917 |  3105 | `	zType = ReflectExportTypeWord(&sVal);` |
|      - |  3106 | ``	/* php's flag words, in its order. `persistent` is a MODULE's registration`` |
|      - |  3107 | `	 * and a define()d constant has none; the three standard streams are the` |
|      - |  3108 | `	 * CLI SAPI's own per-REQUEST constants and have none either, which is what` |
|      - |  3109 | ``	 * the resource test below says. `no_file_cache` is php's opcache flag, and`` |
|      - |  3110 | `	 * it marks exactly the two constants whose value is this process's. */` |
|    917 |  3111 | `	if( !pCons->bUserDefined && (sVal.iFlags & MEMOBJ_RES) == 0 ){` |
|    891 |  3112 | `		if( pCons->zDeprecated ){` |
|     19 |  3113 | `			SyBlobAppend(pOut, "<persistent, deprecated> ", sizeof("<persistent, deprecated> ")-1);` |
|    882 |  3114 | `		}else if( ReflectConstNoFileCache(&pCons->sName) ){` |
|      7 |  3115 | `			SyBlobAppend(pOut, "<persistent, no_file_cache> ",` |
|      - |  3116 | `				sizeof("<persistent, no_file_cache> ")-1);` |
|      4 |  3117 | `		}else{` |
|    867 |  3118 | `			SyBlobAppend(pOut, "<persistent> ", sizeof("<persistent> ")-1);` |
|      - |  3119 | `		}` |
|    445 |  3120 | `	}` |
|    917 |  3121 | `	if( zType ){` |
|    917 |  3122 | `		SyBlobFormat(pOut, "%s ", zType);` |
|    459 |  3123 | `	}else{` |
|    ! 0 |  3124 | `		SyBlobFormat(pOut, "%z ", &((ph7_class_instance *)sVal.x.pOther)->pClass->sName);` |
|      - |  3125 | `	}` |
|    917 |  3126 | `	SyBlobFormat(pOut, "%z ] { ", &pCons->sName);` |
|    917 |  3127 | `	ReflectExportConstValue(pCtx, pOut, &sVal);` |
|    917 |  3128 | `	SyBlobAppend(pOut, " }\n", sizeof(" }\n")-1);` |
|    917 |  3129 | `	PH7_MemObjRelease(&sVal);` |
|    917 |  3130 | `}` |
|     44 |  3131 | `static int vm_builtin_ReflectionConstant_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3132 | `{` |
|     45 |  3133 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      - |  3134 | `	SyBlob sOut;` |
|     22 |  3135 | `	SXUNUSED(nArg);` |
|     22 |  3136 | `	SXUNUSED(apArg);` |
|     45 |  3137 | `	if( pCons == 0 ){` |
|    ! 0 |  3138 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 |  3139 | `		return PH7_OK;` |
|      - |  3140 | `	}` |
|     45 |  3141 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     45 |  3142 | `	ReflectExportGlobalConstLine(pCtx, &sOut, pCons);` |
|     45 |  3143 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     45 |  3144 | `	SyBlobRelease(&sOut);` |
|     45 |  3145 | `	return PH7_OK;` |
|     23 |  3146 | `}` |
|      - |  3147 | `/* ---- ReflectionExtension: one per name this build reports as loaded ---- */` |
|      - |  3148 | `/*` |
|      - |  3149 | ` * php matches the name case-insensitively and then KEEPS its own spelling, so` |
|      - |  3150 | `` * `new ReflectionExtension('DATE')` reports `date` and `spl` reports `SPL`.`` |
|      - |  3151 | `` * A `phl.stub_extensions` name is loaded too and has no canonical spelling of`` |
|      - |  3152 | ` * its own, so it keeps the caller's.` |
|      - |  3153 | ` */` |
|     95 |  3154 | `static int vm_builtin_ReflectionExtension_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  3155 | `{` |
|     98 |  3156 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     98 |  3157 | `	int nName = 0, iExt;` |
|     98 |  3158 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|     98 |  3159 | `	if( pThis == 0 ){` |
|    ! 0 |  3160 | `		return PH7_OK;` |
|      - |  3161 | `	}` |
|     98 |  3162 | `	iExt = PH7_VmExtensionLookup(zName, nName);` |
|     98 |  3163 | `	if( iExt >= 0 ){` |
|     88 |  3164 | `		const char *zCanon = PH7_VmExtensionName(iExt);` |
|     88 |  3165 | `		PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", zCanon, (int)SyStrlen(zCanon));` |
|     88 |  3166 | `		return PH7_OK;` |
|      - |  3167 | `	}` |
|     12 |  3168 | `	if( PH7_VmExtensionIsLoaded(pCtx->pVm, zName, nName) ){` |
|    ! 0 |  3169 | `		PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", zName, nName);` |
|    ! 0 |  3170 | `		return PH7_OK;` |
|      - |  3171 | `	}` |
|     17 |  3172 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      5 |  3173 | `		"Extension \"%.*s\" does not exist", nName, zName);` |
|     49 |  3174 | `}` |
|     22 |  3175 | `static int vm_builtin_ReflectionExtension_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  3176 | `{` |
|     24 |  3177 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     24 |  3178 | `	const char *zName = "";` |
|     24 |  3179 | `	int nName = 0;` |
|     11 |  3180 | `	SXUNUSED(nArg);` |
|     11 |  3181 | `	SXUNUSED(apArg);` |
|     24 |  3182 | `	if( pThis ){` |
|     24 |  3183 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     11 |  3184 | `	}` |
|     24 |  3185 | `	ph7_result_string(pCtx, zName, nName);` |
|     24 |  3186 | `	return PH7_OK;` |
|      2 |  3187 | `}` |
|    ! 0 |  3188 | `static int vm_builtin_ReflectionExtension_getVersion(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3189 | `{` |
|      - |  3190 | `	ph7_value sFn, sRes;` |
|      - |  3191 | `	SyString sStr;` |
|    ! 0 |  3192 | `	SXUNUSED(nArg);` |
|    ! 0 |  3193 | `	SXUNUSED(apArg);` |
|    ! 0 |  3194 | `	PH7_MemObjInit(pCtx->pVm, &sFn);` |
|    ! 0 |  3195 | `	PH7_MemObjInit(pCtx->pVm, &sRes);` |
|    ! 0 |  3196 | `	SyStringInitFromBuf(&sStr, "phpversion", sizeof("phpversion")-1);` |
|    ! 0 |  3197 | `	PH7_MemObjInitFromString(pCtx->pVm, &sFn, &sStr);` |
|    ! 0 |  3198 | `	if( PH7_VmCallUserFunction(pCtx->pVm, &sFn, 0, 0, &sRes) == SXRET_OK ){` |
|    ! 0 |  3199 | `		ph7_result_value(pCtx, &sRes);` |
|    ! 0 |  3200 | `	}` |
|    ! 0 |  3201 | `	PH7_MemObjRelease(&sFn);` |
|    ! 0 |  3202 | `	PH7_MemObjRelease(&sRes);` |
|    ! 0 |  3203 | `	return PH7_OK;` |
|    ! 0 |  3204 | `}` |
|      - |  3205 | `/*` |
|      - |  3206 | ` * The four listings an extension answers about ITSELF -- getFunctions(),` |
|      - |  3207 | ` * getClasses()/getClassNames(), getConstants() and getINIEntries(). All of them` |
|      - |  3208 | ` * are the partition walked in php's own registration order (which is not` |
|      - |  3209 | ` * alphabetical) and filtered against the live VM, so a build without one of the` |
|      - |  3210 | ` * compile-time extensions simply lists nothing for it.` |
|      - |  3211 | ` */` |
|      - |  3212 | `#define REFLECT_EXT_MAP        0   /* name => a fresh reflector over it */` |
|      - |  3213 | `#define REFLECT_EXT_NAMES      1   /* a plain LIST of the names */` |
|      - |  3214 | `#define REFLECT_EXT_VALUES     2   /* name => the value the engine holds */` |
|      - |  3215 | `typedef struct ReflectExtList ReflectExtList;` |
|      - |  3216 | `struct ReflectExtList {` |
|      - |  3217 | `	ph7_context *pCtx;` |
|      - |  3218 | `	ph7_value *pList;   /* the array being built */` |
|      - |  3219 | `	ph7_value *pVal;    /* one scratch value, reused for every entry */` |
|      - |  3220 | `	int iKind;          /* PH7_EXT_KIND_* */` |
|      - |  3221 | `	int iShape;         /* REFLECT_EXT_* */` |
|      - |  3222 | `};` |
|      - |  3223 | ``/* A reflector over one internal name, built with its `name` slot already filled:`` |
|      - |  3224 | ` * the constructor would only re-resolve what this walk already has. */` |
|    286 |  3225 | `static int ReflectExtMakeReflector(ph7_context *pCtx, const char *zRefl,` |
|      - |  3226 | `	const char *zName, int nName, ph7_value *pOut)` |
|      2 |  3227 | `{` |
|    288 |  3228 | `	ph7_vm *pVm = pCtx->pVm;` |
|    288 |  3229 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, zRefl, (sxu32)SyStrlen(zRefl), FALSE, 0);` |
|      - |  3230 | `	ph7_class_instance *pObj;` |
|    288 |  3231 | `	if( pClass == 0 \|\| (pObj = PH7_NewClassInstance(pVm, pClass)) == 0 ){` |
|    ! 0 |  3232 | `		return 0;` |
|      - |  3233 | `	}` |
|    288 |  3234 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", zName, nName);` |
|    288 |  3235 | `	PH7_MemObjRelease(pOut);` |
|    288 |  3236 | `	pOut->x.pOther = pObj;` |
|    288 |  3237 | `	pOut->iFlags = MEMOBJ_OBJ;` |
|    288 |  3238 | `	return 1;` |
|    145 |  3239 | `}` |
|    964 |  3240 | `static int ReflectExtListStep(const char *zName, int nName, void *pData)` |
|      3 |  3241 | `{` |
|    967 |  3242 | `	ReflectExtList *p = (ReflectExtList *)pData;` |
|    967 |  3243 | `	ph7_context *pCtx = p->pCtx;` |
|      - |  3244 | `	SyBlob sKey;` |
|      - |  3245 | `	char *zKey;` |
|    967 |  3246 | `	int i, rc = 0;` |
|    967 |  3247 | `	if( !PH7_VmInternalNameExists(pCtx->pVm, p->iKind, zName, nName) ){` |
|    ! 0 |  3248 | `		return 0;` |
|      - |  3249 | `	}` |
|      - |  3250 | `	/* ph7_array_add_strkey_elem() takes a NUL-terminated key, and the walk has` |
|      - |  3251 | `	 * only a name and a length -- so the key is built rather than borrowed,` |
|      - |  3252 | `	 * which is also where the fold happens. */` |
|    967 |  3253 | `	SyBlobInit(&sKey, &pCtx->pVm->sAllocator);` |
|    967 |  3254 | `	SyBlobAppend(&sKey, (const void *)zName, (sxu32)nName);` |
|    967 |  3255 | `	SyBlobAppend(&sKey, (const void *)"", 1);` |
|    967 |  3256 | `	zKey = (char *)SyBlobData(&sKey);` |
|    967 |  3257 | `	if( p->iKind == PH7_EXT_KIND_FUNC ){` |
|      - |  3258 | `		/* php keys the map with the name the engine STORES, which is folded. */` |
|   5686 |  3259 | `		for( i = 0 ; i < nName ; ++i ){` |
|   5412 |  3260 | `			zKey[i] = (char)SyToLower(zKey[i]);` |
|   2707 |  3261 | `		}` |
|    137 |  3262 | `	}` |
|    967 |  3263 | `	switch( p->iShape ){` |
|     17 |  3264 | `		case REFLECT_EXT_NAMES:` |
|     33 |  3265 | `			ph7_value_reset_string_cursor(p->pVal);` |
|     33 |  3266 | `			ph7_value_string(p->pVal, zName, nName);` |
|     33 |  3267 | `			ph7_array_add_elem(p->pList, 0, p->pVal);` |
|     33 |  3268 | `			SyBlobRelease(&sKey);` |
|     33 |  3269 | `			return 0;` |
|    452 |  3270 | `		case REFLECT_EXT_VALUES:` |
|    651 |  3271 | `			if( p->iKind == PH7_EXT_KIND_INI ){` |
|      - |  3272 | `				SyBlob sVal;` |
|    151 |  3273 | `				SyBlobInit(&sVal, &pCtx->pVm->sAllocator);` |
|    151 |  3274 | `				PH7_VmIniGetStr(pCtx->pVm, zKey, &sVal);` |
|    151 |  3275 | `				ph7_value_reset_string_cursor(p->pVal);` |
|    151 |  3276 | `				if( PH7_VmIniIsUnset(pCtx->pVm, zKey) ){` |
|      - |  3277 | `					/* php shows the RAW value here, so a directive declared with` |
|      - |  3278 | `					 * no value is NULL rather than the empty string ini_get()` |
|      - |  3279 | `					 * makes of it. */` |
|      9 |  3280 | `					PH7_MemObjRelease(p->pVal);` |
|      5 |  3281 | `				}else{` |
|    197 |  3282 | `					ph7_value_string(p->pVal, (const char *)SyBlobData(&sVal),` |
|    141 |  3283 | `						(int)SyBlobLength(&sVal));` |
|      - |  3284 | `				}` |
|    151 |  3285 | `				SyBlobRelease(&sVal);` |
|     60 |  3286 | `			}else{` |
|    503 |  3287 | `				ph7_constant *pCons = ReflectConstEntry(pCtx->pVm, zName, nName);` |
|    503 |  3288 | `				PH7_MemObjRelease(p->pVal);` |
|    503 |  3289 | `				if( pCons && pCons->xExpand ){` |
|      - |  3290 | `					/* Describing the table is not READING an entry: php's` |
|      - |  3291 | `					 * deprecated constants report when a program names one,` |
|      - |  3292 | `					 * and a listing is silent (get_defined_constants()'s` |
|      - |  3293 | `					 * own rule, and the same switch). */` |
|    503 |  3294 | `					pCtx->pVm->bConstEnum++;` |
|    503 |  3295 | `					pCons->xExpand(p->pVal, pCons->pUserData);` |
|    503 |  3296 | `					pCtx->pVm->bConstEnum--;` |
|    139 |  3297 | `				}` |
|      - |  3298 | `			}` |
|    651 |  3299 | `			break;` |
|    143 |  3300 | `		default:` |
|    431 |  3301 | `			if( !ReflectExtMakeReflector(pCtx,` |
|    286 |  3302 | `					p->iKind == PH7_EXT_KIND_CLASS ? "ReflectionClass" : "ReflectionFunction",` |
|    143 |  3303 | `					zName, nName, p->pVal) ){` |
|    ! 0 |  3304 | `				SyBlobRelease(&sKey);` |
|    ! 0 |  3305 | `				return 0;` |
|      - |  3306 | `			}` |
|    286 |  3307 | `			break;` |
|      - |  3308 | `	}` |
|    937 |  3309 | `	ph7_array_add_strkey_elem(p->pList, zKey, p->pVal);` |
|    937 |  3310 | `	SyBlobRelease(&sKey);` |
|    937 |  3311 | `	return rc;` |
|    355 |  3312 | `}` |
|     60 |  3313 | `static int ReflectExtListing(ph7_context *pCtx, int iKind, int iShape)` |
|      3 |  3314 | `{` |
|     63 |  3315 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  3316 | `	ReflectExtList sWalk;` |
|     63 |  3317 | `	const char *zName = "";` |
|     63 |  3318 | `	int nName = 0, iExt;` |
|     63 |  3319 | `	sWalk.pCtx = pCtx;` |
|     63 |  3320 | `	sWalk.iKind = iKind;` |
|     63 |  3321 | `	sWalk.iShape = iShape;` |
|     63 |  3322 | `	sWalk.pList = ph7_context_new_array(pCtx);` |
|     63 |  3323 | `	sWalk.pVal = ph7_context_new_scalar(pCtx);` |
|     63 |  3324 | `	if( sWalk.pList == 0 \|\| sWalk.pVal == 0 ){` |
|    ! 0 |  3325 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  3326 | `	}` |
|     63 |  3327 | `	if( pThis ){` |
|     63 |  3328 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     28 |  3329 | `	}` |
|      - |  3330 | ``	/* A `phl.stub_extensions` name has no id and synthesizes nothing, so its`` |
|      - |  3331 | `	 * every listing is empty -- which is also what php answers for a module` |
|      - |  3332 | `	 * that registers none of that kind. */` |
|     63 |  3333 | `	iExt = PH7_VmExtensionLookup(zName, nName);` |
|     63 |  3334 | `	if( iExt >= 0 ){` |
|     63 |  3335 | `		PH7_VmExtWalk(iExt, iKind, ReflectExtListStep, &sWalk);` |
|     28 |  3336 | `	}` |
|     63 |  3337 | `	ph7_result_value(pCtx, sWalk.pList);` |
|     63 |  3338 | `	return PH7_OK;` |
|     31 |  3339 | `}` |
|      - |  3340 | `#define REFLECT_EXT_LISTING(NAME,KIND,SHAPE) \` |
|      - |  3341 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  3342 | `	{ \` |
|      - |  3343 | `		SXUNUSED(nArg); \` |
|      - |  3344 | `		SXUNUSED(apArg); \` |
|      - |  3345 | `		return ReflectExtListing(pCtx, KIND, SHAPE); \` |
|      - |  3346 | `	}` |
|     14 |  3347 | `REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getFunctions,` |
|      2 |  3348 | `	PH7_EXT_KIND_FUNC,  REFLECT_EXT_MAP)` |
|      7 |  3349 | `REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getClasses,` |
|      1 |  3350 | `	PH7_EXT_KIND_CLASS, REFLECT_EXT_MAP)` |
|     18 |  3351 | `REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getClassNames,` |
|      3 |  3352 | `	PH7_EXT_KIND_CLASS, REFLECT_EXT_NAMES)` |
|     18 |  3353 | `REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getConstants,` |
|      3 |  3354 | `	PH7_EXT_KIND_CONST, REFLECT_EXT_VALUES)` |
|     15 |  3355 | `REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getINIEntries,` |
|      3 |  3356 | `	PH7_EXT_KIND_INI,   REFLECT_EXT_VALUES)` |
|      6 |  3357 | `static int ReflectExtDepStep(const char *zOn, const char *zKind, void *pData)` |
|      1 |  3358 | `{` |
|      7 |  3359 | `	ReflectExtList *p = (ReflectExtList *)pData;` |
|      7 |  3360 | `	ph7_value_reset_string_cursor(p->pVal);` |
|      7 |  3361 | `	ph7_value_string(p->pVal, zKind, -1);` |
|      7 |  3362 | `	ph7_array_add_strkey_elem(p->pList, zOn, p->pVal);` |
|      7 |  3363 | `	return 0;` |
|      1 |  3364 | `}` |
|      8 |  3365 | `static int vm_builtin_ReflectionExtension_getDependencies(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3366 | `{` |
|      9 |  3367 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  3368 | `	ReflectExtList sWalk;` |
|      9 |  3369 | `	const char *zName = "";` |
|      9 |  3370 | `	int nName = 0, iExt;` |
|      4 |  3371 | `	SXUNUSED(nArg);` |
|      4 |  3372 | `	SXUNUSED(apArg);` |
|      9 |  3373 | `	sWalk.pList = ph7_context_new_array(pCtx);` |
|      9 |  3374 | `	sWalk.pVal = ph7_context_new_scalar(pCtx);` |
|      9 |  3375 | `	if( sWalk.pList == 0 \|\| sWalk.pVal == 0 ){` |
|    ! 0 |  3376 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  3377 | `	}` |
|      9 |  3378 | `	if( pThis ){` |
|      9 |  3379 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      4 |  3380 | `	}` |
|      9 |  3381 | `	iExt = PH7_VmExtensionLookup(zName, nName);` |
|      9 |  3382 | `	if( iExt >= 0 ){` |
|      9 |  3383 | `		sWalk.pCtx = pCtx;` |
|      9 |  3384 | `		PH7_VmExtWalkDep(iExt, ReflectExtDepStep, &sWalk);` |
|      4 |  3385 | `	}` |
|      9 |  3386 | `	ph7_result_value(pCtx, sWalk.pList);` |
|      9 |  3387 | `	return PH7_OK;` |
|      5 |  3388 | `}` |
|    ! 0 |  3389 | `static int vm_builtin_ReflectionExtension_isPersistent(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3390 | `{` |
|    ! 0 |  3391 | `	SXUNUSED(nArg);` |
|    ! 0 |  3392 | `	SXUNUSED(apArg);` |
|    ! 0 |  3393 | `	ph7_result_bool(pCtx, 1);` |
|    ! 0 |  3394 | `	return PH7_OK;` |
|    ! 0 |  3395 | `}` |
|    ! 0 |  3396 | `static int vm_builtin_ReflectionExtension_isTemporary(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3397 | `{` |
|    ! 0 |  3398 | `	SXUNUSED(nArg);` |
|    ! 0 |  3399 | `	SXUNUSED(apArg);` |
|    ! 0 |  3400 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 |  3401 | `	return PH7_OK;` |
|    ! 0 |  3402 | `}` |
|    ! 0 |  3403 | `static int vm_builtin_ReflectionExtension_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3404 | `{` |
|    ! 0 |  3405 | `	SXUNUSED(nArg);` |
|    ! 0 |  3406 | `	SXUNUSED(apArg);` |
|    ! 0 |  3407 | `	SXUNUSED(pCtx);` |
|    ! 0 |  3408 | `	return PH7_OK;` |
|    ! 0 |  3409 | `}` |
|      - |  3410 | `/*` |
|      - |  3411 | ` * ---------------------------------------------------------------------------` |
|      - |  3412 | ` * The EXTENSION block -- php's whole module, and the last export this engine` |
|      - |  3413 | `` * did not have (it printed one placeholder line, `Extension [ extension #1`` |
|      - |  3414 | `` * name ]`). It NESTS the function and class blocks, which is why it waited for`` |
|      - |  3415 | ` * them: its bytes cannot match php's until theirs do.` |
|      - |  3416 | ` *` |
|      - |  3417 | ` * php's shape, measured against 8.5.9:` |
|      - |  3418 | ` *` |
|      - |  3419 | ` *   Extension [ <persistent> extension #N name version V ] {` |
|      - |  3420 | ` *   <blank>` |
|      - |  3421 | ` *     - Dependencies { … }        each section only when it has rows,` |
|      - |  3422 | ` *   <blank>                       in this order, and each preceded by a` |
|      - |  3423 | ` *     - INI { … }                 blank line -- an extension with no rows` |
|      - |  3424 | `` *   <blank>                       at all prints `{` and `}` on consecutive`` |
|      - |  3425 | ` *     - Constants [C] { … }       lines instead.` |
|      - |  3426 | ` *   <blank>` |
|      - |  3427 | ` *     - Functions { … }` |
|      - |  3428 | ` *   <blank>` |
|      - |  3429 | ` *     - Classes [K] { … }` |
|      - |  3430 | ` *   }` |
|      - |  3431 | ` *` |
|      - |  3432 | ` * A nested block is the STANDALONE export with four spaces on every non-empty` |
|      - |  3433 | ` * line -- verified byte for byte against php, which is what lets this reuse` |
|      - |  3434 | ` * the two block builders rather than threading an indent through them.` |
|      - |  3435 | ` * ---------------------------------------------------------------------------` |
|      - |  3436 | ` */` |
|      - |  3437 | `static void ReflectExportFuncByName(ph7_context *pCtx, SyBlob *pOut,` |
|      - |  3438 | `	const char *zName, int nName);` |
|      - |  3439 | `static sxi32 ReflectExportClassBlock(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass);` |
|      - |  3440 | `/* Copy pIn into pOut with nPad spaces in front of every non-empty line. */` |
|   1374 |  3441 | `static void ReflectExportPad(SyBlob *pOut, SyBlob *pIn, int nPad)` |
|      2 |  3442 | `{` |
|   1376 |  3443 | `	const char *zIn = (const char *)SyBlobData(pIn);` |
|   1376 |  3444 | `	sxu32 nIn = SyBlobLength(pIn), i = 0;` |
|  33598 |  3445 | `	while( i < nIn ){` |
|  32224 |  3446 | `		sxu32 j = i;` |
| 772979 |  3447 | `		while( j < nIn && zIn[j] != '\n' ){` |
| 740757 |  3448 | `			j++;` |
|      2 |  3449 | `		}` |
|  32224 |  3450 | `		if( j > i ){` |
|      - |  3451 | `			int k;` |
| 120822 |  3452 | `			for( k = 0 ; k < nPad ; k++ ){` |
|  96658 |  3453 | `				SyBlobAppend(pOut, " ", sizeof(char));` |
|  48330 |  3454 | `			}` |
|  24166 |  3455 | `			SyBlobAppend(pOut, &zIn[i], j - i);` |
|  12082 |  3456 | `		}` |
|  32224 |  3457 | `		if( j < nIn ){` |
|  32224 |  3458 | `			SyBlobAppend(pOut, "\n", sizeof(char));` |
|  16111 |  3459 | `		}` |
|  32224 |  3460 | `		i = j + 1;` |
|      2 |  3461 | `	}` |
|   1376 |  3462 | `}` |
|      - |  3463 | `typedef struct ReflectExtDump ReflectExtDump;` |
|      - |  3464 | `struct ReflectExtDump {` |
|      - |  3465 | `	ph7_context *pCtx;` |
|      - |  3466 | `	SyBlob *pOut;     /* the section body, already indented */` |
|      - |  3467 | `	int iKind;` |
|      - |  3468 | `	sxu32 nRow;` |
|      - |  3469 | `};` |
|      - |  3470 | ``/* php's access word for an ini directive: the whole mask is `ALL`, and`` |
|      - |  3471 | ` * anything else is the set bits joined with a comma in php's own order. */` |
|     62 |  3472 | `static void ReflectExtIniAccess(SyBlob *pOut, sxi32 iAccess)` |
|      1 |  3473 | `{` |
|      - |  3474 | `	static const struct { sxi32 iBit; const char *zWord; } aBit[] = {` |
|      - |  3475 | `		{ 1, "USER" }, { 2, "PERDIR" }, { 4, "SYSTEM" }` |
|      - |  3476 | `	};` |
|      - |  3477 | `	sxu32 n;` |
|     63 |  3478 | `	int bFirst = 1;` |
|     63 |  3479 | `	if( (iAccess & 7) == 7 ){` |
|     43 |  3480 | `		SyBlobAppend(pOut, "ALL", sizeof("ALL")-1);` |
|     43 |  3481 | `		return;` |
|      - |  3482 | `	}` |
|     81 |  3483 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBit) ; ++n ){` |
|     61 |  3484 | `		if( iAccess & aBit[n].iBit ){` |
|     33 |  3485 | `			if( !bFirst ){` |
|     13 |  3486 | `				SyBlobAppend(pOut, ",", sizeof(char));` |
|      6 |  3487 | `			}` |
|     33 |  3488 | `			SyBlobAppend(pOut, aBit[n].zWord, SyStrlen(aBit[n].zWord));` |
|     33 |  3489 | `			bFirst = 0;` |
|     16 |  3490 | `		}` |
|     31 |  3491 | `	}` |
|     32 |  3492 | `}` |
|   1436 |  3493 | `static int ReflectExtDumpStep(const char *zName, int nName, void *pData)` |
|      2 |  3494 | `{` |
|   1438 |  3495 | `	ReflectExtDump *p = (ReflectExtDump *)pData;` |
|   1438 |  3496 | `	ph7_context *pCtx = p->pCtx;` |
|   1438 |  3497 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  3498 | `	SyBlob sBlock;` |
|   1438 |  3499 | `	if( !PH7_VmInternalNameExists(pVm, p->iKind, zName, nName) ){` |
|    ! 0 |  3500 | `		return 0;` |
|      - |  3501 | `	}` |
|   1438 |  3502 | `	p->nRow++;` |
|   1438 |  3503 | `	if( p->iKind == PH7_EXT_KIND_INI ){` |
|      - |  3504 | `		SyBlob sVal, sDef;` |
|     63 |  3505 | `		sxi32 iAccess = 0;` |
|     63 |  3506 | `		SyBlobInit(&sVal, &pVm->sAllocator);` |
|     63 |  3507 | `		SyBlobInit(&sDef, &pVm->sAllocator);` |
|     63 |  3508 | `		if( PH7_VmIniDescribe(pVm, zName, (sxu32)nName, &iAccess, &sVal, &sDef) ){` |
|     63 |  3509 | `			SyBlobAppend(p->pOut, "    Entry [ ", sizeof("    Entry [ ")-1);` |
|     63 |  3510 | `			SyBlobAppend(p->pOut, zName, (sxu32)nName);` |
|     63 |  3511 | `			SyBlobAppend(p->pOut, " <", sizeof(" <")-1);` |
|     63 |  3512 | `			ReflectExtIniAccess(p->pOut, iAccess);` |
|     63 |  3513 | `			SyBlobAppend(p->pOut, "> ]\n      Current = '", sizeof("> ]\n      Current = '")-1);` |
|     63 |  3514 | `			SyBlobAppend(p->pOut, SyBlobData(&sVal), SyBlobLength(&sVal));` |
|     63 |  3515 | `			SyBlobAppend(p->pOut, "'\n", sizeof("'\n")-1);` |
|      - |  3516 | `			/* php prints the DEFAULT only for a directive a script moved. */` |
|     62 |  3517 | `			if( SyBlobLength(&sVal) != SyBlobLength(&sDef)` |
|     63 |  3518 | `			 \|\| SyMemcmp(SyBlobData(&sVal), SyBlobData(&sDef), SyBlobLength(&sVal)) != 0 ){` |
|      3 |  3519 | `				SyBlobAppend(p->pOut, "      Default = '", sizeof("      Default = '")-1);` |
|      3 |  3520 | `				SyBlobAppend(p->pOut, SyBlobData(&sDef), SyBlobLength(&sDef));` |
|      3 |  3521 | `				SyBlobAppend(p->pOut, "'\n", sizeof("'\n")-1);` |
|      1 |  3522 | `			}` |
|     63 |  3523 | `			SyBlobAppend(p->pOut, "    }\n", sizeof("    }\n")-1);` |
|     31 |  3524 | `		}` |
|     63 |  3525 | `		SyBlobRelease(&sVal);` |
|     63 |  3526 | `		SyBlobRelease(&sDef);` |
|     63 |  3527 | `		return 0;` |
|      - |  3528 | `	}` |
|   1376 |  3529 | `	SyBlobInit(&sBlock, &pVm->sAllocator);` |
|   1376 |  3530 | `	if( p->iKind == PH7_EXT_KIND_CONST ){` |
|    873 |  3531 | `		ph7_constant *pCons = ReflectConstEntry(pVm, zName, nName);` |
|    873 |  3532 | `		if( pCons ){` |
|    873 |  3533 | `			pVm->bConstEnum++;   /* describing the table is not READING an entry */` |
|    873 |  3534 | `			ReflectExportGlobalConstLine(pCtx, &sBlock, pCons);` |
|    873 |  3535 | `			pVm->bConstEnum--;` |
|    437 |  3536 | `		}` |
|    940 |  3537 | `	}else if( p->iKind == PH7_EXT_KIND_CLASS ){` |
|    253 |  3538 | `		ph7_class *pClass = PH7_VmExtractClass(pVm, zName, (sxu32)nName, FALSE, 0);` |
|      - |  3539 | `		/* php separates one CLASS block from the next with a blank line, and` |
|      - |  3540 | `		 * does NOT do the same for functions or constants. */` |
|    253 |  3541 | `		if( p->nRow > 1 ){` |
|    239 |  3542 | `			SyBlobAppend(p->pOut, "\n", sizeof(char));` |
|    119 |  3543 | `		}` |
|    253 |  3544 | `		if( pClass ){` |
|    253 |  3545 | `			ReflectExportClassBlock(pCtx, &sBlock, pClass);` |
|    126 |  3546 | `		}` |
|    127 |  3547 | `	}else{` |
|    252 |  3548 | `		ReflectExportFuncByName(pCtx, &sBlock, zName, nName);` |
|      - |  3549 | `	}` |
|   1376 |  3550 | `	ReflectExportPad(p->pOut, &sBlock, 4);` |
|   1376 |  3551 | `	SyBlobRelease(&sBlock);` |
|   1376 |  3552 | `	return 0;` |
|    720 |  3553 | `}` |
|      - |  3554 | ``/* One `  - <title>[ [N]] { … }` section, written only when it has rows. */`` |
|    100 |  3555 | `static void ReflectExtSection(SyBlob *pOut, const char *zTitle, SyBlob *pBody,` |
|      - |  3556 | `	sxu32 nRow, int bCount)` |
|      2 |  3557 | `{` |
|    102 |  3558 | `	if( SyBlobLength(pBody) < 1 ){` |
|     56 |  3559 | `		return;` |
|      - |  3560 | `	}` |
|     48 |  3561 | `	SyBlobFormat(pOut, "\n  - %s ", zTitle);` |
|     48 |  3562 | `	if( bCount ){` |
|     25 |  3563 | `		SyBlobFormat(pOut, "[%u] ", nRow);` |
|     12 |  3564 | `	}` |
|     48 |  3565 | `	SyBlobAppend(pOut, "{\n", sizeof("{\n")-1);` |
|     48 |  3566 | `	SyBlobAppend(pOut, SyBlobData(pBody), SyBlobLength(pBody));` |
|     48 |  3567 | `	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|     52 |  3568 | `}` |
|      2 |  3569 | `static int ReflectExtDepLine(const char *zOn, const char *zKind, void *pData)` |
|      1 |  3570 | `{` |
|      3 |  3571 | `	ReflectExtDump *p = (ReflectExtDump *)pData;` |
|      3 |  3572 | `	p->nRow++;` |
|      3 |  3573 | `	SyBlobFormat(p->pOut, "    Dependency [ %s (%s) ]\n", zOn, zKind);` |
|      3 |  3574 | `	return 0;` |
|      1 |  3575 | `}` |
|     80 |  3576 | `static void ReflectExtOneSection(ph7_context *pCtx, SyBlob *pOut, int iExt,` |
|      - |  3577 | `	int iKind, const char *zTitle, int bCount)` |
|      2 |  3578 | `{` |
|      - |  3579 | `	ReflectExtDump sDump;` |
|      - |  3580 | `	SyBlob sBody;` |
|     82 |  3581 | `	SyBlobInit(&sBody, &pCtx->pVm->sAllocator);` |
|     82 |  3582 | `	sDump.pCtx = pCtx;` |
|     82 |  3583 | `	sDump.pOut = &sBody;` |
|     82 |  3584 | `	sDump.iKind = iKind;` |
|     82 |  3585 | `	sDump.nRow = 0;` |
|     82 |  3586 | `	PH7_VmExtWalk(iExt, iKind, ReflectExtDumpStep, &sDump);` |
|     82 |  3587 | `	ReflectExtSection(pOut, zTitle, &sBody, sDump.nRow, bCount);` |
|     82 |  3588 | `	SyBlobRelease(&sBody);` |
|     82 |  3589 | `}` |
|     20 |  3590 | `static int vm_builtin_ReflectionExtension_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  3591 | `{` |
|     22 |  3592 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     22 |  3593 | `	ph7_vm *pVm = pCtx->pVm;` |
|     22 |  3594 | `	const char *zName = "";` |
|     22 |  3595 | `	int nName = 0, iExt;` |
|      - |  3596 | `	SyBlob sOut;` |
|     10 |  3597 | `	SXUNUSED(nArg);` |
|     10 |  3598 | `	SXUNUSED(apArg);` |
|     22 |  3599 | `	if( pThis ){` |
|     22 |  3600 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     10 |  3601 | `	}` |
|     22 |  3602 | `	iExt = PH7_VmExtensionLookup(zName, nName);` |
|     22 |  3603 | `	SyBlobInit(&sOut, &pVm->sAllocator);` |
|      - |  3604 | `` 	/* php's `#N` is the module's REGISTRATION index; a `phl.stub_extensions` `` |
|      - |  3605 | `	 * name has no partition of its own and rides at the end of the table. */` |
|      - |  3606 | `	/* The version is the one getVersion() reports, which is php's answer for a` |
|      - |  3607 | `	 * BUNDLED module: the interpreter's own, not the extension's. */` |
|     22 |  3608 | `	SyBlobFormat(&sOut, "Extension [ <persistent> extension #%d %.*s version %s ] {\n",` |
|     10 |  3609 | `		iExt >= 0 ? iExt : PH7_VmExtensionCount(), nName, zName, PHP_COMPAT_VERSION);` |
|     22 |  3610 | `	if( iExt >= 0 ){` |
|      - |  3611 | `		ReflectExtDump sDep;` |
|      - |  3612 | `		SyBlob sBody;` |
|     22 |  3613 | `		SyBlobInit(&sBody, &pVm->sAllocator);` |
|     22 |  3614 | `		sDep.pCtx = pCtx;` |
|     22 |  3615 | `		sDep.pOut = &sBody;` |
|     22 |  3616 | `		sDep.iKind = -1;` |
|     22 |  3617 | `		sDep.nRow = 0;` |
|     22 |  3618 | `		PH7_VmExtWalkDep(iExt, ReflectExtDepLine, &sDep);` |
|     22 |  3619 | `		ReflectExtSection(&sOut, "Dependencies", &sBody, sDep.nRow, 0);` |
|     22 |  3620 | `		SyBlobRelease(&sBody);` |
|     22 |  3621 | `		ReflectExtOneSection(pCtx, &sOut, iExt, PH7_EXT_KIND_INI,   "INI",       0);` |
|     22 |  3622 | `		ReflectExtOneSection(pCtx, &sOut, iExt, PH7_EXT_KIND_CONST, "Constants", 1);` |
|     22 |  3623 | `		ReflectExtOneSection(pCtx, &sOut, iExt, PH7_EXT_KIND_FUNC,  "Functions", 0);` |
|     22 |  3624 | `		ReflectExtOneSection(pCtx, &sOut, iExt, PH7_EXT_KIND_CLASS, "Classes",   1);` |
|     10 |  3625 | `	}` |
|     22 |  3626 | `	SyBlobAppend(&sOut, "}\n", sizeof("}\n")-1);` |
|     22 |  3627 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     22 |  3628 | `	SyBlobRelease(&sOut);` |
|     22 |  3629 | `	return PH7_OK;` |
|      2 |  3630 | `}` |
|      - |  3631 | `/* ---- ReflectionZendExtension: none exist, so the constructor always refuses ---- */` |
|      6 |  3632 | `static int vm_builtin_ReflectionZendExtension_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  3633 | `{` |
|      8 |  3634 | `	int nName = 0;` |
|      8 |  3635 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|     11 |  3636 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 |  3637 | `		"Zend Extension \"%.*s\" does not exist", nName, zName);` |
|      2 |  3638 | `}` |
|    ! 0 |  3639 | `static int vm_builtin_ReflectionZendExtension_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3640 | `{` |
|    ! 0 |  3641 | `	SXUNUSED(nArg);` |
|    ! 0 |  3642 | `	SXUNUSED(apArg);` |
|    ! 0 |  3643 | `	ph7_result_string(pCtx, "", 0);` |
|    ! 0 |  3644 | `	return PH7_OK;` |
|    ! 0 |  3645 | `}` |
|      - |  3646 | `/* ---- ReflectionReference ---- */` |
|      - |  3647 | `/*` |
|      - |  3648 | ` * ReflectionReference::fromArrayElement(array $array, string\|int $key)` |
|      - |  3649 | ` *` |
|      - |  3650 | ` * Answers a reflector only when the element IS a reference — its slot carries a` |
|      - |  3651 | ` * reference-table record with at least two links. The instance is built without` |
|      - |  3652 | ` * running the (private) constructor, exactly as php's factory does.` |
|      - |  3653 | ` */` |
|     18 |  3654 | `static int vm_builtin_ReflectionReference_fromArrayElement(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3655 | `{` |
|      - |  3656 | `	char zGiven[64];` |
|     19 |  3657 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  3658 | `	ph7_hashmap *pMap;` |
|     19 |  3659 | `	ph7_hashmap_node *pNode = 0;` |
|      - |  3660 | `	ph7_class *pClass;` |
|      - |  3661 | `	ph7_class_instance *pObj;` |
|      - |  3662 | `	char zId[64];` |
|     19 |  3663 | `	if( nArg < 1 ){` |
|    ! 0 |  3664 | `		ph7_result_null(pCtx);` |
|    ! 0 |  3665 | `		return PH7_OK;` |
|      - |  3666 | `	}` |
|     19 |  3667 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - |  3668 | ``		/* The shared ZPP screen leaves a SCALAR against a bare `array` parameter`` |
|      - |  3669 | `		 * unscreened (it only judges array/object/null/resource pairings and` |
|      - |  3670 | `		 * scalars against class-typed ones), so the refusal php words from the` |
|      - |  3671 | `		 * declared type is written here -- as the prelude's own is_array() check` |
|      - |  3672 | `		 * did. Recorded with the rest of that gap. */` |
|    ! 0 |  3673 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|      - |  3674 | `			"ReflectionReference::fromArrayElement(): Argument #1 ($array) "` |
|    ! 0 |  3675 | `			"must be of type array, %s given", VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|      - |  3676 | `	}` |
|     19 |  3677 | `	if( nArg < 2 ){` |
|    ! 0 |  3678 | `		ph7_result_null(pCtx);` |
|    ! 0 |  3679 | `		return PH7_OK;` |
|      - |  3680 | `	}` |
|     19 |  3681 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     19 |  3682 | `	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK \|\| pNode == 0 ){` |
|    ! 0 |  3683 | `		ph7_result_null(pCtx);` |
|    ! 0 |  3684 | `		return PH7_OK;` |
|      - |  3685 | `	}` |
|     19 |  3686 | `	if( PH7_VmSlotRefCount(pVm, pNode->nValIdx) < 2 ){` |
|      7 |  3687 | `		ph7_result_null(pCtx);` |
|      7 |  3688 | `		return PH7_OK;` |
|      - |  3689 | `	}` |
|     13 |  3690 | `	pClass = PH7_VmExtractClass(pVm, "ReflectionReference", sizeof("ReflectionReference")-1, FALSE, 0);` |
|     13 |  3691 | `	pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|     13 |  3692 | `	if( pObj == 0 ){` |
|    ! 0 |  3693 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  3694 | `	}` |
|      - |  3695 | `	/* php's ids are opaque strings; the slot index is the identity PHL has. */` |
|     13 |  3696 | `	SyBufferFormat(zId, sizeof(zId), "phlref%u", (unsigned)pNode->nValIdx);` |
|     13 |  3697 | `	PH7_NativeSetAttrStr(pVm, pObj, RR_ID, zId, (int)SyStrlen(zId));` |
|     13 |  3698 | `	return ReflectResultObject(pCtx, pObj);` |
|     10 |  3699 | `}` |
|     12 |  3700 | `static int vm_builtin_ReflectionReference_getId(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3701 | `{` |
|     13 |  3702 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     13 |  3703 | `	const char *zId = "";` |
|     13 |  3704 | `	int nId = 0;` |
|      6 |  3705 | `	SXUNUSED(nArg);` |
|      6 |  3706 | `	SXUNUSED(apArg);` |
|     13 |  3707 | `	if( pThis ){` |
|     13 |  3708 | `		PH7_NativeAttrStr(pThis, RR_ID, &zId, &nId);` |
|      6 |  3709 | `	}` |
|     13 |  3710 | `	ph7_result_string(pCtx, zId, nId);` |
|     13 |  3711 | `	return PH7_OK;` |
|      1 |  3712 | `}` |
|      - |  3713 | `/* php declares this private and never calls it; reaching it is the diagnostic. */` |
|    ! 0 |  3714 | `static int vm_builtin_ReflectionReference_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3715 | `{` |
|    ! 0 |  3716 | `	SXUNUSED(nArg);` |
|    ! 0 |  3717 | `	SXUNUSED(apArg);` |
|    ! 0 |  3718 | `	SXUNUSED(pCtx);` |
|    ! 0 |  3719 | `	return PH7_OK;` |
|    ! 0 |  3720 | `}` |
|      - |  3721 | `/*` |
|      - |  3722 | ` * Declare the six. Called from PH7_VmInstallReflection() where chunk 5 used to` |
|      - |  3723 | ` * be compiled, so Reflector (chunk 1) already exists.` |
|      - |  3724 | ` */` |
|   7925 |  3725 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionSmall(ph7_vm *pVm)` |
|      5 |  3726 | `{` |
|      - |  3727 | `	static const PH7_NativePropDef aGenProp[] = {` |
|      - |  3728 | `		{ RG_GEN, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  3729 | `	};` |
|      - |  3730 | `	static const PH7_NativeMethodDef aGenMethod[] = {` |
|      - |  3731 | `		{ "__construct",           PH7_MOD_PUBLIC, "Generator $generator", "",` |
|      - |  3732 | `		  vm_builtin_ReflectionGenerator_construct },` |
|      - |  3733 | `		{ "getFunction",           PH7_MOD_PUBLIC, "", "ReflectionFunctionAbstract",` |
|      - |  3734 | `		  vm_builtin_ReflectionGenerator_getFunction },` |
|      - |  3735 | `		{ "getThis",               PH7_MOD_PUBLIC, "", "?object", vm_builtin_ReflectionGenerator_getThis },` |
|      - |  3736 | `		{ "getExecutingGenerator", PH7_MOD_PUBLIC, "", "Generator",` |
|      - |  3737 | `		  vm_builtin_ReflectionGenerator_getExecuting },` |
|      - |  3738 | `		{ "isClosed",              PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionGenerator_isClosed },` |
|      - |  3739 | `		{ "getExecutingLine",      PH7_MOD_PUBLIC, "", "int", vm_builtin_ReflectionGenerator_execLine },` |
|      - |  3740 | `		{ "getExecutingFile",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionGenerator_execFile },` |
|      - |  3741 | `		{ "getTrace",              PH7_MOD_PUBLIC,` |
|      - |  3742 | `		  "int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT", "array",` |
|      - |  3743 | `		  vm_builtin_ReflectionGenerator_trace },` |
|      - |  3744 | `	};` |
|      - |  3745 | `	static const PH7_NativePropDef aFiberProp[] = {` |
|      - |  3746 | `		{ RF_FIBER, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  3747 | `	};` |
|      - |  3748 | `	static const PH7_NativeMethodDef aFiberMethod[] = {` |
|      - |  3749 | `		{ "__construct",      PH7_MOD_PUBLIC, "Fiber $fiber", "", vm_builtin_ReflectionFiber_construct },` |
|      - |  3750 | `		{ "getFiber",         PH7_MOD_PUBLIC, "", "Fiber",    vm_builtin_ReflectionFiber_getFiber },` |
|      - |  3751 | `		{ "getCallable",      PH7_MOD_PUBLIC, "", "callable", vm_builtin_ReflectionFiber_getCallable },` |
|      - |  3752 | `		{ "getExecutingLine", PH7_MOD_PUBLIC, "", "?int",     vm_builtin_ReflectionFiber_execLine },` |
|      - |  3753 | `		{ "getExecutingFile", PH7_MOD_PUBLIC, "", "?string",  vm_builtin_ReflectionFiber_execFile },` |
|      - |  3754 | `		{ "getTrace",         PH7_MOD_PUBLIC, "int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT",` |
|      - |  3755 | `		  "array", vm_builtin_ReflectionFiber_trace },` |
|      - |  3756 | `	};` |
|      - |  3757 | `	static const PH7_NativePropDef aNameProp[] = {` |
|      - |  3758 | `		{ "name", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  3759 | `	};` |
|      - |  3760 | `	static const PH7_NativeMethodDef aConstMethod[] = {` |
|      - |  3761 | `		{ "__construct",       PH7_MOD_PUBLIC, "string $name", "",` |
|      - |  3762 | `		  vm_builtin_ReflectionConstant_construct },` |
|      - |  3763 | `		{ "getName",           PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_getName },` |
|      - |  3764 | `		{ "getNamespaceName",  PH7_MOD_PUBLIC, "", "string",` |
|      - |  3765 | `		  vm_builtin_ReflectionConstant_getNamespaceName },` |
|      - |  3766 | `		{ "getShortName",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_getShortName },` |
|      - |  3767 | `		{ "getValue",          PH7_MOD_PUBLIC, "", "mixed",  vm_builtin_ReflectionConstant_getValue },` |
|      - |  3768 | `		{ "isDeprecated",      PH7_MOD_PUBLIC, "", "bool",   vm_builtin_ReflectionConstant_isDeprecated },` |
|      - |  3769 | `		{ "getFileName",       PH7_MOD_PUBLIC, "", "string\|false",` |
|      - |  3770 | `		  vm_builtin_ReflectionConstant_getFileName },` |
|      - |  3771 | `		{ "getExtension",      PH7_MOD_PUBLIC, "", "?ReflectionExtension",` |
|      - |  3772 | `		  vm_builtin_ReflectionConstant_getExtension },` |
|      - |  3773 | `		{ "getExtensionName",  PH7_MOD_PUBLIC, "", "string\|false",` |
|      - |  3774 | `		  vm_builtin_ReflectionConstant_getExtensionName },` |
|      - |  3775 | `		{ "getAttributes",     PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - |  3776 | `		  vm_builtin_ReflectionConstant_getAttributes },` |
|      - |  3777 | `		{ "__toString",        PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_toString },` |
|      - |  3778 | `	};` |
|      - |  3779 | `	static const PH7_NativeMethodDef aExtMethod[] = {` |
|      - |  3780 | `		{ "__construct",     PH7_MOD_PUBLIC, "string $name", "", vm_builtin_ReflectionExtension_construct },` |
|      - |  3781 | `		{ "getName",         PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionExtension_getName },` |
|      - |  3782 | `		{ "getVersion",      PH7_MOD_PUBLIC, "", "@?string", vm_builtin_ReflectionExtension_getVersion },` |
|      - |  3783 | `		{ "getFunctions",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getFunctions },` |
|      - |  3784 | `		{ "getConstants",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getConstants },` |
|      - |  3785 | `		{ "getINIEntries",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getINIEntries },` |
|      - |  3786 | `		{ "getClasses",      PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getClasses },` |
|      - |  3787 | `		{ "getClassNames",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getClassNames },` |
|      - |  3788 | `		{ "getDependencies", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getDependencies },` |
|      - |  3789 | `		{ "info",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_ReflectionExtension_info },` |
|      - |  3790 | `		{ "isPersistent",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionExtension_isPersistent },` |
|      - |  3791 | `		{ "isTemporary",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionExtension_isTemporary },` |
|      - |  3792 | `		{ "__toString",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionExtension_toString },` |
|      - |  3793 | `	};` |
|      - |  3794 | `	static const PH7_NativeMethodDef aZendMethod[] = {` |
|      - |  3795 | `		{ "__construct", PH7_MOD_PUBLIC, "string $name", "",` |
|      - |  3796 | `		  vm_builtin_ReflectionZendExtension_construct },` |
|      - |  3797 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionExtension_getName },` |
|      - |  3798 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionZendExtension_toString },` |
|      - |  3799 | `	};` |
|      - |  3800 | `	static const PH7_NativePropDef aRefProp[] = {` |
|      - |  3801 | `		{ RR_ID, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - |  3802 | `	};` |
|      - |  3803 | `	static const PH7_NativeMethodDef aRefMethod[] = {` |
|      - |  3804 | ``		/* php declares the constructor PRIVATE, so `new ReflectionReference` is`` |
|      - |  3805 | `		 * the engine's own "Call to private ... from global scope" Error rather` |
|      - |  3806 | `		 * than a throw the prelude had to write by hand. */` |
|      - |  3807 | `		{ "__construct",      PH7_MOD_PRIVATE, "", "", vm_builtin_ReflectionReference_construct },` |
|      - |  3808 | `		{ "fromArrayElement", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array, string\|int $key",` |
|      - |  3809 | `		  "?ReflectionReference", vm_builtin_ReflectionReference_fromArrayElement },` |
|      - |  3810 | `		{ "getId",            PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionReference_getId },` |
|      - |  3811 | `	};` |
|      - |  3812 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  3813 | `		{ "ReflectionGenerator", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3814 | `		  aGenMethod, SX_ARRAYSIZE(aGenMethod), 0, 0, aGenProp, SX_ARRAYSIZE(aGenProp), 0, 0, 0 },` |
|      - |  3815 | `		{ "ReflectionFiber", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3816 | `		  aFiberMethod, SX_ARRAYSIZE(aFiberMethod), 0, 0, aFiberProp, SX_ARRAYSIZE(aFiberProp), 0, 0, 0 },` |
|      - |  3817 | `		{ "ReflectionConstant", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3818 | `		  aConstMethod, SX_ARRAYSIZE(aConstMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|      - |  3819 | `		{ "ReflectionExtension", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3820 | `		  aExtMethod, SX_ARRAYSIZE(aExtMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|      - |  3821 | `		{ "ReflectionZendExtension", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3822 | `		  aZendMethod, SX_ARRAYSIZE(aZendMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|      - |  3823 | `		{ "ReflectionReference", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3824 | `		  aRefMethod, SX_ARRAYSIZE(aRefMethod), 0, 0, aRefProp, SX_ARRAYSIZE(aRefProp), 0, 0, 0 },` |
|      - |  3825 | `	};` |
|   7930 |  3826 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  3827 | `}` |
|      - |  3828 | `/*` |
|      - |  3829 | ` * ---------------------------------------------------------------------------` |
|      - |  3830 | ` * Reflector, Reflection, ReflectionException, ReflectionClass, ReflectionObject.` |
|      - |  3831 | ` *` |
|      - |  3832 | ` * Chunk 1 — the core of the whole API. Its ~350 lines of PHP funnelled every` |
|      - |  3833 | ` * accessor through __phl_rcinfo(), a memoized descriptor ARRAY built by a C` |
|      - |  3834 | ` * thunk: sixty methods that each rebuilt or re-read a marshalled copy of state` |
|      - |  3835 | ` * the engine was already holding. A native method reads ph7_class directly, so` |
|      - |  3836 | ` * the descriptor is gone from this path entirely and the memo it needed with it.` |
|      - |  3837 | ` *` |
|      - |  3838 | ` * What stays behind is the prelude that has not moved yet, and these classes` |
|      - |  3839 | ` * still reach it by name where php's own object graph does: getMethod() builds` |
|      - |  3840 | ` * a ReflectionMethod, getProperty() a ReflectionProperty, getAttributes() calls` |
|      - |  3841 | ` * __reflect_build_attrs, __toString() calls __reflect_export_class. Those become` |
|      - |  3842 | ` * direct C the moment chunks 2, 3, 7 and 9 land.` |
|      - |  3843 | ` * ---------------------------------------------------------------------------` |
|      - |  3844 | ` */` |
|      - |  3845 | `#define RC_OBJ "__obj"` |
|      - |  3846 |  |
|      - |  3847 | ``/* The class a ReflectionClass reflects: its `name` slot, resolved. The`` |
|      - |  3848 | ` * constructor already stored the canonical name, so no autoload can be needed` |
|      - |  3849 | ` * here. */` |
|   1615 |  3850 | `static ph7_class * ReflectClassOf(ph7_context *pCtx)` |
|      5 |  3851 | `{` |
|   1620 |  3852 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  3853 | `	const char *zName;` |
|      - |  3854 | `	int nName;` |
|   1620 |  3855 | `	if( pThis == 0 ){` |
|    ! 0 |  3856 | `		return 0;` |
|      - |  3857 | `	}` |
|   1620 |  3858 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|   1620 |  3859 | `	if( nName < 1 ){` |
|    ! 0 |  3860 | `		return 0;` |
|      - |  3861 | `	}` |
|      - |  3862 | `	/* PH7_NativeAttrStr borrows bytes that are NOT NUL-terminated: the length` |
|      - |  3863 | `	 * has to travel with them. */` |
|   1620 |  3864 | `	return PH7_VmExtractClass(pCtx->pVm, zName, (sxu32)nName, FALSE, 0);` |
|    802 |  3865 | `}` |
|      - |  3866 | `/* The instance a ReflectionObject was built over, or NULL for a plain` |
|      - |  3867 | ` * ReflectionClass — what makes DYNAMIC properties visible. */` |
|    644 |  3868 | `static ph7_class_instance * ReflectClassObj(ph7_context *pCtx)` |
|      5 |  3869 | `{` |
|    649 |  3870 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    649 |  3871 | `	return pThis ? PH7_NativeAttrObj(pThis, RC_OBJ) : 0;` |
|      5 |  3872 | `}` |
|      - |  3873 | `/* $this->name as bytes. */` |
|    920 |  3874 | `static void ReflectClassName(ph7_context *pCtx, const char **pzOut, int *pnOut)` |
|      5 |  3875 | `{` |
|    925 |  3876 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    925 |  3877 | `	*pzOut = "";` |
|    925 |  3878 | `	*pnOut = 0;` |
|    925 |  3879 | `	if( pThis ){` |
|    925 |  3880 | `		PH7_NativeAttrStr(pThis, "name", pzOut, pnOut);` |
|    458 |  3881 | `	}` |
|    925 |  3882 | `}` |
|      - |  3883 | `/* Answer the truth of one of the reflected class's iFlags bits. */` |
|    170 |  3884 | `static int ReflectClassFlag(ph7_context *pCtx, sxi32 iFlag)` |
|      5 |  3885 | `{` |
|    175 |  3886 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|    175 |  3887 | `	return pClass != 0 && (pClass->iFlags & iFlag) != 0;` |
|      5 |  3888 | `}` |
|      - |  3889 | `#define REFLECT_CLASS_FLAG(NAME,FLAG,NEGATE) \` |
|      - |  3890 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  3891 | `	{ \` |
|      - |  3892 | `		SXUNUSED(nArg); \` |
|      - |  3893 | `		SXUNUSED(apArg); \` |
|      - |  3894 | `		ph7_result_bool(pCtx, NEGATE ? !ReflectClassFlag(pCtx,FLAG) : ReflectClassFlag(pCtx,FLAG)); \` |
|      - |  3895 | `		return PH7_OK; \` |
|      - |  3896 | `	}` |
|     35 |  3897 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isInternal,    PH7_CLASS_INTERNAL,  0)` |
|      5 |  3898 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isUserDefined, PH7_CLASS_INTERNAL,  1)` |
|      9 |  3899 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isInterface,   PH7_CLASS_INTERFACE, 0)` |
|      5 |  3900 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isTrait,       PH7_CLASS_TRAIT,     0)` |
|      - |  3901 | `/*` |
|      - |  3902 | ` * zend's IMPLICIT abstract bit, which this engine did not carry: an INTERFACE` |
|      - |  3903 | `` * that declares a method is abstract, so `(new ReflectionClass('Countable'))`` |
|      - |  3904 | `` * ->isAbstract()` is true where this said false for 24 of php's 25 interfaces`` |
|      - |  3905 | ` * and for every userland one besides. The bit follows the METHODS rather than` |
|      - |  3906 | `` * the keyword, which is why `Traversable` -- an interface with nothing in it --`` |
|      - |  3907 | ` * is php's one negative answer. Only this predicate reads it: getModifiers()` |
|      - |  3908 | `` * is 0 for an interface in php too, and the export writes `interface X`, never`` |
|      - |  3909 | `` * `abstract interface X`.`` |
|      - |  3910 | ` */` |
|     60 |  3911 | `static int vm_builtin_ReflectionClass_isAbstract(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  3912 | `{` |
|     63 |  3913 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     63 |  3914 | `	int bAbstract = 0;` |
|     30 |  3915 | `	SXUNUSED(nArg);` |
|     30 |  3916 | `	SXUNUSED(apArg);` |
|     63 |  3917 | `	if( pClass ){` |
|     89 |  3918 | `		bAbstract = (pClass->iFlags & PH7_CLASS_ABSTRACT) != 0` |
|     88 |  3919 | `			\|\| ((pClass->iFlags & PH7_CLASS_INTERFACE) != 0` |
|     40 |  3920 | `			 && SyHashTotalEntry(&pClass->hMethod) > 0);` |
|     30 |  3921 | `	}` |
|     63 |  3922 | `	ph7_result_bool(pCtx, bAbstract);` |
|     63 |  3923 | `	return PH7_OK;` |
|      3 |  3924 | `}` |
|    107 |  3925 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isFinal,       PH7_CLASS_FINAL,     0)` |
|      9 |  3926 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isReadOnly,    PH7_CLASS_READONLY,  0)` |
|     13 |  3927 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isEnum,        PH7_CLASS_ENUM,      0)` |
|      - |  3928 |  |
|      - |  3929 | ``/* Build a ReflectionClass over pTarget with its `name` already filled in: the`` |
|      - |  3930 | ` * constructor would only re-resolve a class this code is holding. */` |
|    486 |  3931 | `static int ReflectResultClassOf(ph7_context *pCtx, ph7_class *pTarget)` |
|      4 |  3932 | `{` |
|    490 |  3933 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  3934 | `	ph7_class *pRC;` |
|      - |  3935 | `	ph7_class_instance *pObj;` |
|    490 |  3936 | `	if( pTarget == 0 ){` |
|    ! 0 |  3937 | `		ph7_result_null(pCtx);` |
|    ! 0 |  3938 | `		return PH7_OK;` |
|      - |  3939 | `	}` |
|    490 |  3940 | `	pRC = PH7_VmExtractClass(pVm, "ReflectionClass", sizeof("ReflectionClass")-1, FALSE, 0);` |
|    490 |  3941 | `	if( pRC == 0 \|\| (pObj = PH7_NewClassInstance(pVm, pRC)) == 0 ){` |
|    ! 0 |  3942 | `		ph7_result_null(pCtx);` |
|    ! 0 |  3943 | `		return PH7_OK;` |
|      - |  3944 | `	}` |
|    729 |  3945 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(&pTarget->sName),` |
|    486 |  3946 | `		(int)SyStringLength(&pTarget->sName));` |
|    490 |  3947 | `	PH7_NativeResultObject(pCtx, pObj);` |
|    490 |  3948 | `	return PH7_OK;` |
|    243 |  3949 | `}` |
|      - |  3950 | ``/* The class an argument declared `ReflectionClass\|string` denotes: a reflector's`` |
|      - |  3951 | `` * `name` slot, or the string itself. NULL when it names nothing (after`` |
|      - |  3952 | ` * autoload). */` |
|     34 |  3953 | `static ph7_class * ReflectClassArg(ph7_context *pCtx, ph7_value *pArg)` |
|      1 |  3954 | `{` |
|     35 |  3955 | `	ph7_vm *pVm = pCtx->pVm;` |
|     35 |  3956 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|    ! 0 |  3957 | `		ph7_class_instance *pObj = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 |  3958 | `		ph7_class *pRC = PH7_VmExtractClass(pVm, "ReflectionClass", sizeof("ReflectionClass")-1, FALSE, 0);` |
|    ! 0 |  3959 | `		if( pRC && PH7_VmInstanceOf(pObj->pClass, pRC) ){` |
|      - |  3960 | `			const char *zName;` |
|      - |  3961 | `			int nName;` |
|    ! 0 |  3962 | `			PH7_NativeAttrStr(pObj, "name", &zName, &nName);` |
|    ! 0 |  3963 | `			return nName > 0 ? PH7_VmExtractClass(pVm, zName, (sxu32)nName, FALSE, 0) : 0;` |
|      - |  3964 | `		}` |
|    ! 0 |  3965 | `	}` |
|     35 |  3966 | `	return ReflectResolveClass(pVm, pArg);` |
|     12 |  3967 | `}` |
|      - |  3968 | `/* ---- constructors ---- */` |
|      - |  3969 | `/* ReflectionClass::__construct(object\|string $objectOrClass) */` |
|    953 |  3970 | `static int vm_builtin_ReflectionClass_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  3971 | `{` |
|    958 |  3972 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  3973 | `	ph7_class *pClass;` |
|    958 |  3974 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 |  3975 | `		return PH7_OK;` |
|      - |  3976 | `	}` |
|    958 |  3977 | `	pClass = ReflectResolveClass(pCtx->pVm, apArg[0]);` |
|    958 |  3978 | `	if( pClass == 0 ){` |
|      - |  3979 | `		/* php reports the NAME it was handed, after the declared object\|string` |
|      - |  3980 | ``		 * has coerced a scalar — `new ReflectionClass(1.5)` says Class "1.5". */`` |
|      - |  3981 | `		const char *zName;` |
|      - |  3982 | `		int nName;` |
|     12 |  3983 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|     17 |  3984 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      5 |  3985 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      - |  3986 | `	}` |
|   1417 |  3987 | `	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", SyStringData(&pClass->sName),` |
|    943 |  3988 | `		(int)SyStringLength(&pClass->sName));` |
|    948 |  3989 | `	return PH7_OK;` |
|    479 |  3990 | `}` |
|      - |  3991 | `/* ReflectionObject::__construct(object $object) — the same, plus the receiver` |
|      - |  3992 | ` * whose DYNAMIC properties the inherited accessors then report. */` |
|     24 |  3993 | `static int vm_builtin_ReflectionObject_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  3994 | `{` |
|     26 |  3995 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  3996 | `	sxi32 rc;` |
|     26 |  3997 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 |  3998 | `		return PH7_OK;` |
|      - |  3999 | `	}` |
|     26 |  4000 | `	rc = vm_builtin_ReflectionClass_construct(pCtx, nArg, apArg);` |
|     26 |  4001 | `	if( rc != PH7_OK ){` |
|    ! 0 |  4002 | `		return rc;` |
|      - |  4003 | `	}` |
|     26 |  4004 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RC_OBJ, (ph7_class_instance *)apArg[0]->x.pOther);` |
|     26 |  4005 | `	return PH7_OK;` |
|     14 |  4006 | `}` |
|      - |  4007 | `/* ReflectionClass::__clone(): void — php declares it private, and the class is` |
|      - |  4008 | ` * uncloneable besides (PH7_CLASS_NOCLONE answers before any body runs). */` |
|    ! 0 |  4009 | `static int vm_builtin_ReflectionClass_clone(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  4010 | `{` |
|    ! 0 |  4011 | `	SXUNUSED(pCtx);` |
|    ! 0 |  4012 | `	SXUNUSED(nArg);` |
|    ! 0 |  4013 | `	SXUNUSED(apArg);` |
|    ! 0 |  4014 | `	return PH7_OK;` |
|    ! 0 |  4015 | `}` |
|      - |  4016 | `/* ---- name ---- */` |
|    498 |  4017 | `static int vm_builtin_ReflectionClass_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  4018 | `{` |
|      - |  4019 | `	const char *zName;` |
|      - |  4020 | `	int nName;` |
|    247 |  4021 | `	SXUNUSED(nArg);` |
|    247 |  4022 | `	SXUNUSED(apArg);` |
|    502 |  4023 | `	ReflectClassName(pCtx, &zName, &nName);` |
|    502 |  4024 | `	ph7_result_string(pCtx, zName, nName);` |
|    502 |  4025 | `	return PH7_OK;` |
|      4 |  4026 | `}` |
|      - |  4027 | `/* Offset just past the last namespace separator, or -1 when there is none. */` |
|      6 |  4028 | `static int ReflectNsCut(const char *zName, int nName)` |
|      1 |  4029 | `{` |
|      - |  4030 | `	int i;` |
|     91 |  4031 | `	for( i = nName - 1 ; i >= 0 ; i-- ){` |
|     85 |  4032 | `		if( zName[i] == '\\' ){` |
|    ! 0 |  4033 | `			return i;` |
|      - |  4034 | `		}` |
|     43 |  4035 | `	}` |
|      7 |  4036 | `	return -1;` |
|      4 |  4037 | `}` |
|      2 |  4038 | `static int vm_builtin_ReflectionClass_getShortName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4039 | `{` |
|      - |  4040 | `	const char *zName;` |
|      - |  4041 | `	int nName, iCut;` |
|      1 |  4042 | `	SXUNUSED(nArg);` |
|      1 |  4043 | `	SXUNUSED(apArg);` |
|      3 |  4044 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 |  4045 | `	iCut = ReflectNsCut(zName, nName);` |
|      3 |  4046 | `	if( iCut < 0 ){` |
|      3 |  4047 | `		ph7_result_string(pCtx, zName, nName);` |
|      2 |  4048 | `	}else{` |
|    ! 0 |  4049 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|      - |  4050 | `	}` |
|      3 |  4051 | `	return PH7_OK;` |
|      1 |  4052 | `}` |
|      2 |  4053 | `static int vm_builtin_ReflectionClass_getNamespaceName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4054 | `{` |
|      - |  4055 | `	const char *zName;` |
|      - |  4056 | `	int nName, iCut;` |
|      1 |  4057 | `	SXUNUSED(nArg);` |
|      1 |  4058 | `	SXUNUSED(apArg);` |
|      3 |  4059 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 |  4060 | `	iCut = ReflectNsCut(zName, nName);` |
|      3 |  4061 | `	ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|      3 |  4062 | `	return PH7_OK;` |
|      1 |  4063 | `}` |
|      2 |  4064 | `static int vm_builtin_ReflectionClass_inNamespace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4065 | `{` |
|      - |  4066 | `	const char *zName;` |
|      - |  4067 | `	int nName;` |
|      1 |  4068 | `	SXUNUSED(nArg);` |
|      1 |  4069 | `	SXUNUSED(apArg);` |
|      3 |  4070 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 |  4071 | `	ph7_result_bool(pCtx, ReflectNsCut(zName, nName) >= 0);` |
|      3 |  4072 | `	return PH7_OK;` |
|      1 |  4073 | `}` |
|      6 |  4074 | `static int vm_builtin_ReflectionClass_isAnonymous(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4075 | `{` |
|      - |  4076 | `	/* An anonymous class is the only one whose name has two halves: php synthesizes` |
|      - |  4077 | ``	 * `<prefix>@anonymous` + NUL + `file:line$hex`, and PH7_NewRawClass cuts sDisp at`` |
|      - |  4078 | `	 * the NUL, so a shorter display name IS the marker. The old test read the name` |
|      - |  4079 | ``	 * for a literal `class@anonymous` prefix, which php only ever writes when the`` |
|      - |  4080 | `	 * class has no parent and no interface. */` |
|      7 |  4081 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      3 |  4082 | `	SXUNUSED(nArg);` |
|      3 |  4083 | `	SXUNUSED(apArg);` |
|      7 |  4084 | `	ph7_result_bool(pCtx, pClass != 0 && pClass->sDisp.nByte != pClass->sName.nByte);` |
|      7 |  4085 | `	return PH7_OK;` |
|      1 |  4086 | `}` |
|      - |  4087 | `/* ---- shape ---- */` |
|     48 |  4088 | `static int vm_builtin_ReflectionClass_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4089 | `{` |
|     49 |  4090 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     49 |  4091 | `	sxi64 iMods = 0;` |
|     24 |  4092 | `	SXUNUSED(nArg);` |
|     24 |  4093 | `	SXUNUSED(apArg);` |
|     49 |  4094 | `	if( pClass ){` |
|     49 |  4095 | `		if( pClass->iFlags & PH7_CLASS_ABSTRACT ){ iMods \|= 64; }` |
|     49 |  4096 | `		if( pClass->iFlags & PH7_CLASS_FINAL ){ iMods \|= 32; }` |
|     49 |  4097 | `		if( pClass->iFlags & PH7_CLASS_READONLY ){ iMods \|= 65536; }` |
|     24 |  4098 | `	}` |
|     49 |  4099 | `	ph7_result_int64(pCtx, iMods);` |
|     49 |  4100 | `	return PH7_OK;` |
|      1 |  4101 | `}` |
|     38 |  4102 | `static int vm_builtin_ReflectionClass_getParentClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  4103 | `{` |
|     42 |  4104 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     15 |  4105 | `	SXUNUSED(nArg);` |
|     15 |  4106 | `	SXUNUSED(apArg);` |
|     42 |  4107 | `	if( pClass == 0 \|\| pClass->pBase == 0 ){` |
|     13 |  4108 | `		ph7_result_bool(pCtx, 0);` |
|     13 |  4109 | `		return PH7_OK;` |
|      - |  4110 | `	}` |
|     31 |  4111 | `	return ReflectResultClassOf(pCtx, pClass->pBase);` |
|     19 |  4112 | `}` |
|      - |  4113 | `/* getInterfaceNames()/getInterfaces()/getTraitNames()/getTraits() — one walk,` |
|      - |  4114 | ` * four shapes. bReflector picks name-list vs {name: ReflectionClass}. */` |
|    104 |  4115 | `static int ReflectClassNameList(ph7_context *pCtx, int bTraits, int bReflector)` |
|      4 |  4116 | `{` |
|    108 |  4117 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|    108 |  4118 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|      - |  4119 | `	SySet aSet;` |
|      - |  4120 | `	ph7_class **apOut;` |
|      - |  4121 | `	sxu32 n, nOut;` |
|    108 |  4122 | `	if( pList == 0 ){` |
|    ! 0 |  4123 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  4124 | `	}` |
|    108 |  4125 | `	if( pClass == 0 ){` |
|    ! 0 |  4126 | `		ph7_result_value(pCtx, pList);` |
|    ! 0 |  4127 | `		return PH7_OK;` |
|      - |  4128 | `	}` |
|    108 |  4129 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|    108 |  4130 | `	if( bTraits ){` |
|      3 |  4131 | `		apOut = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      3 |  4132 | `		nOut = SySetUsed(&pClass->aTrait);` |
|      2 |  4133 | `	}else{` |
|    106 |  4134 | `		PH7_ReflectInterfacesOf(pCtx->pVm, pClass, &aSet);` |
|    106 |  4135 | `		apOut = (ph7_class **)SySetBasePtr(&aSet);` |
|    106 |  4136 | `		nOut = SySetUsed(&aSet);` |
|      - |  4137 | `	}` |
|    328 |  4138 | `	for( n = 0 ; n < nOut ; n++ ){` |
|    223 |  4139 | `		SyString *pName = &apOut[n]->sName;` |
|    223 |  4140 | `		if( bReflector ){` |
|      - |  4141 | `			/* {name: ReflectionClass} — the reflector is built here rather than` |
|      - |  4142 | `			 * through ReflectResultClassOf, which writes the RESULT slot. */` |
|      5 |  4143 | `			ph7_class *pRC = PH7_VmExtractClass(pCtx->pVm, "ReflectionClass",` |
|      - |  4144 | `				sizeof("ReflectionClass")-1, FALSE, 0);` |
|      5 |  4145 | `			ph7_class_instance *pObj = pRC ? PH7_NewClassInstance(pCtx->pVm, pRC) : 0;` |
|      5 |  4146 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|      - |  4147 | `			ph7_value sVal;` |
|      5 |  4148 | `			if( pObj == 0 \|\| pKey == 0 ){ break; }` |
|      7 |  4149 | `			PH7_NativeSetAttrStr(pCtx->pVm, pObj, "name", SyStringData(pName),` |
|      4 |  4150 | `				(int)SyStringLength(pName));` |
|      - |  4151 | `			/* A STACK carrier, never a context scalar: releasing a MEMOBJ_OBJ` |
|      - |  4152 | `			 * context value would unref the instance a second time. */` |
|      5 |  4153 | `			PH7_MemObjInit(pCtx->pVm, &sVal);` |
|      5 |  4154 | `			sVal.x.pOther = pObj;` |
|      5 |  4155 | `			sVal.iFlags = MEMOBJ_OBJ;` |
|      5 |  4156 | `			ph7_value_string(pKey, SyStringData(pName), (int)SyStringLength(pName));` |
|      5 |  4157 | `			ph7_array_add_elem(pList, pKey, &sVal);   /* takes its own reference */` |
|      5 |  4158 | `			PH7_ClassInstanceUnref(pObj);` |
|      3 |  4159 | `		}else{` |
|    219 |  4160 | `			ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    219 |  4161 | `			if( pVal == 0 ){ break; }` |
|    219 |  4162 | `			ph7_value_string(pVal, SyStringData(pName), (int)SyStringLength(pName));` |
|    219 |  4163 | `			ph7_array_add_elem(pList, 0, pVal);` |
|      - |  4164 | `		}` |
|    113 |  4165 | `	}` |
|    108 |  4166 | `	SySetRelease(&aSet);` |
|    108 |  4167 | `	ph7_result_value(pCtx, pList);` |
|    108 |  4168 | `	return PH7_OK;` |
|     56 |  4169 | `}` |
|      - |  4170 | `#define REFLECT_CLASS_LIST(NAME,TRAITS,REFLECTOR) \` |
|      - |  4171 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  4172 | `	{ \` |
|      - |  4173 | `		SXUNUSED(nArg); \` |
|      - |  4174 | `		SXUNUSED(apArg); \` |
|      - |  4175 | `		return ReflectClassNameList(pCtx,TRAITS,REFLECTOR); \` |
|      - |  4176 | `	}` |
|    104 |  4177 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getInterfaceNames, 0, 0)` |
|      3 |  4178 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getInterfaces,     0, 1)` |
|      3 |  4179 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getTraitNames,     1, 0)` |
|    ! 0 |  4180 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getTraits,         1, 1)` |
|      - |  4181 |  |
|      - |  4182 | ``/* getTraitAliases(): php reports `use T { m as n; }` renames; PHL's compiler`` |
|      - |  4183 | ` * installs the alias as a method and keeps no rename record, so the map is` |
|      - |  4184 | ` * empty (recorded). */` |
|    ! 0 |  4185 | `static int vm_builtin_ReflectionClass_getTraitAliases(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  4186 | `{` |
|    ! 0 |  4187 | `	SXUNUSED(nArg);` |
|    ! 0 |  4188 | `	SXUNUSED(apArg);` |
|    ! 0 |  4189 | `	ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  4190 | `	return PH7_OK;` |
|    ! 0 |  4191 | `}` |
|      4 |  4192 | `static int vm_builtin_ReflectionClass_isIterable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4193 | `{` |
|      5 |  4194 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4195 | `	SySet aSet;` |
|      - |  4196 | `	ph7_class **apIface;` |
|      - |  4197 | `	sxu32 n;` |
|      5 |  4198 | `	int bIterable = 0;` |
|      2 |  4199 | `	SXUNUSED(nArg);` |
|      2 |  4200 | `	SXUNUSED(apArg);` |
|      4 |  4201 | `	if( pClass == 0` |
|      5 |  4202 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT)) ){` |
|    ! 0 |  4203 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4204 | `		return PH7_OK;` |
|      - |  4205 | `	}` |
|      5 |  4206 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|      5 |  4207 | `	PH7_ReflectInterfacesOf(pCtx->pVm, pClass, &aSet);` |
|      5 |  4208 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|      9 |  4209 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|      9 |  4210 | `		if( pCtx->pVm->pTraversableClass && apIface[n] == pCtx->pVm->pTraversableClass ){` |
|      5 |  4211 | `			bIterable = 1;` |
|      5 |  4212 | `			break;` |
|      - |  4213 | `		}` |
|      3 |  4214 | `	}` |
|      5 |  4215 | `	SySetRelease(&aSet);` |
|      5 |  4216 | `	ph7_result_bool(pCtx, bIterable);` |
|      5 |  4217 | `	return PH7_OK;` |
|      3 |  4218 | `}` |
|     28 |  4219 | `static int vm_builtin_ReflectionClass_implementsInterface(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4220 | `{` |
|     29 |  4221 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4222 | `	ph7_class *pTarget;` |
|      - |  4223 | `	SySet aSet;` |
|      - |  4224 | `	ph7_class **apIface;` |
|      - |  4225 | `	sxu32 n;` |
|     29 |  4226 | `	int bYes = 0;` |
|     29 |  4227 | `	if( nArg < 1 ){` |
|    ! 0 |  4228 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4229 | `		return PH7_OK;` |
|      - |  4230 | `	}` |
|     29 |  4231 | `	pTarget = ReflectClassArg(pCtx, apArg[0]);` |
|     29 |  4232 | `	if( pTarget == 0 ){` |
|      - |  4233 | `		const char *zName;` |
|      - |  4234 | `		int nName;` |
|      3 |  4235 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|      4 |  4236 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  4237 | `			"Interface \"%.*s\" does not exist", nName, zName);` |
|      - |  4238 | `	}` |
|     27 |  4239 | `	if( (pTarget->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      4 |  4240 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  4241 | `			"%z is not an interface", &pTarget->sDisp);` |
|      - |  4242 | `	}` |
|     25 |  4243 | `	if( pClass == pTarget ){` |
|      3 |  4244 | `		ph7_result_bool(pCtx, 1);` |
|      3 |  4245 | `		return PH7_OK;` |
|      - |  4246 | `	}` |
|     23 |  4247 | `	if( pClass == 0 ){` |
|    ! 0 |  4248 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4249 | `		return PH7_OK;` |
|      - |  4250 | `	}` |
|     23 |  4251 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|     23 |  4252 | `	PH7_ReflectInterfacesOf(pCtx->pVm, pClass, &aSet);` |
|     23 |  4253 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|     58 |  4254 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|     52 |  4255 | `		if( apIface[n] == pTarget ){` |
|     17 |  4256 | `			bYes = 1;` |
|     17 |  4257 | `			break;` |
|      - |  4258 | `		}` |
|      2 |  4259 | `	}` |
|     23 |  4260 | `	SySetRelease(&aSet);` |
|     23 |  4261 | `	ph7_result_bool(pCtx, bYes);` |
|     23 |  4262 | `	return PH7_OK;` |
|      9 |  4263 | `}` |
|      6 |  4264 | `static int vm_builtin_ReflectionClass_isSubclassOf(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4265 | `{` |
|      7 |  4266 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4267 | `	ph7_class *pTarget, *pWalk;` |
|      - |  4268 | `	SySet aSet;` |
|      - |  4269 | `	ph7_class **apIface;` |
|      - |  4270 | `	sxu32 n;` |
|      7 |  4271 | `	int iDepth = 0, bYes = 0;` |
|      7 |  4272 | `	if( nArg < 1 ){` |
|    ! 0 |  4273 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4274 | `		return PH7_OK;` |
|      - |  4275 | `	}` |
|      7 |  4276 | `	pTarget = ReflectClassArg(pCtx, apArg[0]);` |
|      7 |  4277 | `	if( pTarget == 0 ){` |
|      - |  4278 | `		const char *zName;` |
|      - |  4279 | `		int nName;` |
|    ! 0 |  4280 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|    ! 0 |  4281 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 |  4282 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      - |  4283 | `	}` |
|      - |  4284 | `	/* php: a class is never a subclass of ITSELF */` |
|      7 |  4285 | `	if( pClass == 0 \|\| pClass == pTarget ){` |
|      3 |  4286 | `		ph7_result_bool(pCtx, 0);` |
|      3 |  4287 | `		return PH7_OK;` |
|      - |  4288 | `	}` |
|      7 |  4289 | `	for( pWalk = pClass->pBase ; pWalk && iDepth <= REFLECT_WALK_MAX_DEPTH ; pWalk = pWalk->pBase ){` |
|      5 |  4290 | `		if( pWalk == pTarget ){` |
|      3 |  4291 | `			ph7_result_bool(pCtx, 1);` |
|      3 |  4292 | `			return PH7_OK;` |
|      - |  4293 | `		}` |
|      3 |  4294 | `		iDepth++;` |
|      2 |  4295 | `	}` |
|      3 |  4296 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|      3 |  4297 | `	PH7_ReflectInterfacesOf(pCtx->pVm, pClass, &aSet);` |
|      3 |  4298 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|      3 |  4299 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|      3 |  4300 | `		if( apIface[n] == pTarget ){` |
|      3 |  4301 | `			bYes = 1;` |
|      3 |  4302 | `			break;` |
|      - |  4303 | `		}` |
|    ! 0 |  4304 | `	}` |
|      3 |  4305 | `	SySetRelease(&aSet);` |
|      3 |  4306 | `	ph7_result_bool(pCtx, bYes);` |
|      3 |  4307 | `	return PH7_OK;` |
|      4 |  4308 | `}` |
|      8 |  4309 | `static int vm_builtin_ReflectionClass_isInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4310 | `{` |
|      9 |  4311 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4312 | `	ph7_class_instance *pObj;` |
|      9 |  4313 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 \|\| pClass == 0 ){` |
|    ! 0 |  4314 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4315 | `		return PH7_OK;` |
|      - |  4316 | `	}` |
|      9 |  4317 | `	pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      9 |  4318 | `	ph7_result_bool(pCtx, PH7_VmInstanceOf(pObj->pClass, pClass) != 0);` |
|      9 |  4319 | `	return PH7_OK;` |
|      5 |  4320 | `}` |
|      - |  4321 | `/* ---- source position ---- */` |
|     10 |  4322 | `static int vm_builtin_ReflectionClass_getStartLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4323 | `{` |
|     11 |  4324 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      5 |  4325 | `	SXUNUSED(nArg);` |
|      5 |  4326 | `	SXUNUSED(apArg);` |
|     11 |  4327 | `	if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_INTERNAL) ){` |
|      3 |  4328 | `		ph7_result_bool(pCtx, 0);` |
|      2 |  4329 | `	}else{` |
|      9 |  4330 | `		ph7_result_int64(pCtx, (sxi64)pClass->nLine);` |
|      - |  4331 | `	}` |
|     11 |  4332 | `	return PH7_OK;` |
|      1 |  4333 | `}` |
|      6 |  4334 | `static int vm_builtin_ReflectionClass_getEndLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4335 | `{` |
|      7 |  4336 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      3 |  4337 | `	SXUNUSED(nArg);` |
|      3 |  4338 | `	SXUNUSED(apArg);` |
|      7 |  4339 | `	if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_INTERNAL) ){` |
|      3 |  4340 | `		ph7_result_bool(pCtx, 0);` |
|      2 |  4341 | `	}else{` |
|      5 |  4342 | `		ph7_result_int64(pCtx, (sxi64)pClass->nEndLine);` |
|      - |  4343 | `	}` |
|      7 |  4344 | `	return PH7_OK;` |
|      1 |  4345 | `}` |
|     18 |  4346 | `static int vm_builtin_ReflectionClass_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4347 | `{` |
|     20 |  4348 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      9 |  4349 | `	SXUNUSED(nArg);` |
|      9 |  4350 | `	SXUNUSED(apArg);` |
|     20 |  4351 | `	if( pClass && SyStringLength(&pClass->sFile) > 0 ){` |
|     10 |  4352 | `		ph7_result_string(pCtx, SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));` |
|      6 |  4353 | `	}else{` |
|     11 |  4354 | `		ph7_result_bool(pCtx, 0);` |
|      - |  4355 | `	}` |
|     20 |  4356 | `	return PH7_OK;` |
|      2 |  4357 | `}` |
|      8 |  4358 | `static int vm_builtin_ReflectionClass_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4359 | `{` |
|      9 |  4360 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      4 |  4361 | `	SXUNUSED(nArg);` |
|      4 |  4362 | `	SXUNUSED(apArg);` |
|      9 |  4363 | `	if( pClass && SyStringLength(&pClass->sDoc) > 0 ){` |
|      3 |  4364 | `		ph7_result_string(pCtx, SyStringData(&pClass->sDoc), (int)SyStringLength(&pClass->sDoc));` |
|      2 |  4365 | `	}else{` |
|      7 |  4366 | `		ph7_result_bool(pCtx, 0);` |
|      - |  4367 | `	}` |
|      9 |  4368 | `	return PH7_OK;` |
|      1 |  4369 | `}` |
|      - |  4370 | `/* ---- instantiation ---- */` |
|     40 |  4371 | `static int vm_builtin_ReflectionClass_isInstantiable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  4372 | `{` |
|     44 |  4373 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4374 | `	sxi32 iCtor, iClone;` |
|     20 |  4375 | `	SXUNUSED(nArg);` |
|     20 |  4376 | `	SXUNUSED(apArg);` |
|     40 |  4377 | `	if( pClass == 0` |
|     44 |  4378 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT\|PH7_CLASS_ENUM)) ){` |
|      7 |  4379 | `		ph7_result_bool(pCtx, 0);` |
|      7 |  4380 | `		return PH7_OK;` |
|      - |  4381 | `	}` |
|     38 |  4382 | `	ReflectCtorCloneVis(pCtx->pVm, pClass, &iCtor, &iClone);` |
|     38 |  4383 | `	ph7_result_bool(pCtx, iCtor == 0 \|\| iCtor == PH7_CLASS_PROT_PUBLIC);` |
|     38 |  4384 | `	return PH7_OK;` |
|     24 |  4385 | `}` |
|     36 |  4386 | `static int vm_builtin_ReflectionClass_isCloneable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4387 | `{` |
|     38 |  4388 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4389 | `	sxi32 iCtor, iClone;` |
|     18 |  4390 | `	SXUNUSED(nArg);` |
|     18 |  4391 | `	SXUNUSED(apArg);` |
|     36 |  4392 | `	if( pClass == 0` |
|     38 |  4393 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT)) ){` |
|      3 |  4394 | `		ph7_result_bool(pCtx, 0);` |
|      3 |  4395 | `		return PH7_OK;` |
|      - |  4396 | `	}` |
|     36 |  4397 | `	ReflectCtorCloneVis(pCtx->pVm, pClass, &iCtor, &iClone);` |
|     36 |  4398 | `	if( iClone != 0 ){` |
|      - |  4399 | `		/* php consults a declared __clone FIRST and answers its visibility --` |
|      - |  4400 | ``		 * even when the clone_obj refusal would still answer `clone` itself, so`` |
|      - |  4401 | `		 * a subclass of Exception that declares a public __clone reports TRUE` |
|      - |  4402 | `		 * and refuses anyway. php's own inconsistency, kept. */` |
|      7 |  4403 | `		ph7_result_bool(pCtx, iClone == PH7_CLASS_PROT_PUBLIC);` |
|      7 |  4404 | `		return PH7_OK;` |
|      - |  4405 | `	}` |
|      - |  4406 | `	/* No __clone anywhere: php's answer is whether the clone_obj handler` |
|      - |  4407 | `	 * exists. The refusal flag walks the base chain like the handler it` |
|      - |  4408 | ``	 * models, so `class M extends IteratorIterator {}` reports false too. */`` |
|     30 |  4409 | `	ph7_result_bool(pCtx, !PH7_ClassIsUncloneable(pClass));` |
|     30 |  4410 | `	return PH7_OK;` |
|     20 |  4411 | `}` |
|      - |  4412 | `/*` |
|      - |  4413 | ` * php's own gate, raised before any object exists -- the one every C-side` |
|      - |  4414 | ` * instantiation asks, Reflection's newInstance() and PDO's FETCH_CLASS alike.` |
|      - |  4415 | ` */` |
|    282 |  4416 | `PH7_PRIVATE sxi32 PH7_VmCheckInstantiable(ph7_context *pCtx, ph7_class *pClass)` |
|      5 |  4417 | `{` |
|    287 |  4418 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|      3 |  4419 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate interface %z", &pClass->sDisp);` |
|      - |  4420 | `	}` |
|    285 |  4421 | `	if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|      3 |  4422 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate trait %z", &pClass->sDisp);` |
|      - |  4423 | `	}` |
|    283 |  4424 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|      - |  4425 | `		/* php 8.1 names the enum rather than the FINAL class it also is. */` |
|      3 |  4426 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate enum %z", &pClass->sDisp);` |
|      - |  4427 | `	}` |
|    281 |  4428 | `	if( pClass->iFlags & PH7_CLASS_ABSTRACT ){` |
|      7 |  4429 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate abstract class %z", &pClass->sDisp);` |
|      - |  4430 | `	}` |
|    275 |  4431 | `	if( pClass->iFlags & PH7_CLASS_NOINSTANTIATE ){` |
|      - |  4432 | ``		/* The `new` path's create_object refusal, which php raises here too —`` |
|      - |  4433 | `		 * ReflectionClass::newInstance() on a Closure is the same Error, not a` |
|      - |  4434 | `		 * visibility one about its private constructor. */` |
|     12 |  4435 | `		if( pClass->zNewRefusal ){` |
|     10 |  4436 | `			return PH7_VmThrowException(pCtx,` |
|      8 |  4437 | `				pClass->zNewRefusalClass ? pClass->zNewRefusalClass : "Error",` |
|      4 |  4438 | `				"%s", pClass->zNewRefusal);` |
|      - |  4439 | `		}` |
|      4 |  4440 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      1 |  4441 | `			"Instantiation of class %z is not allowed", &pClass->sDisp);` |
|      - |  4442 | `	}` |
|    265 |  4443 | `	return PH7_OK;` |
|    146 |  4444 | `}` |
|      - |  4445 | `/*` |
|      - |  4446 | ` * newInstance()/newInstanceArgs(): the checks php runs, then the constructor.` |
|      - |  4447 | ` * apCtor/nCtor are already-collected positional arguments; pNames is the` |
|      - |  4448 | ` * name map when the caller handed an array with string keys (php 8.1 accepts` |
|      - |  4449 | ` * those as NAMED constructor arguments).` |
|      - |  4450 | ` */` |
|     36 |  4451 | `static int ReflectNewInstance(ph7_context *pCtx, int nCtor, ph7_value **apCtor,` |
|      - |  4452 | `	SyString *pNames)` |
|      5 |  4453 | `{` |
|     41 |  4454 | `	ph7_vm *pVm = pCtx->pVm;` |
|     41 |  4455 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4456 | `	ph7_class_instance *pObj;` |
|      - |  4457 | `	ph7_class_method *pCons;` |
|      - |  4458 | `	sxi32 iCtorVis, iCloneVis, rc;` |
|     41 |  4459 | `	if( pClass == 0 ){` |
|    ! 0 |  4460 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4461 | `		return PH7_OK;` |
|      - |  4462 | `	}` |
|     41 |  4463 | `	rc = PH7_VmCheckInstantiable(pCtx, pClass);` |
|     41 |  4464 | `	if( rc != PH7_OK ){` |
|     14 |  4465 | `		return rc;` |
|      - |  4466 | `	}` |
|     28 |  4467 | `	ReflectCtorCloneVis(pVm, pClass, &iCtorVis, &iCloneVis);` |
|     28 |  4468 | `	if( iCtorVis != 0 && iCtorVis != PH7_CLASS_PROT_PUBLIC ){` |
|      4 |  4469 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  4470 | `			"Access to non-public constructor of class %z", &pClass->sDisp);` |
|      - |  4471 | `	}` |
|     26 |  4472 | `	if( iCtorVis == 0 && nCtor > 0 ){` |
|      4 |  4473 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  4474 | `			"Class %z does not have a constructor, so you cannot pass any constructor arguments",` |
|      1 |  4475 | `			&pClass->sDisp);` |
|      - |  4476 | `	}` |
|     24 |  4477 | `	if( VmClassStaticDeferPending(pClass) ){` |
|      - |  4478 | `		/* Instantiation materializes the static table (OP_NEW does it too), so a` |
|      - |  4479 | `		 * broken default raises BEFORE any object exists. */` |
|      3 |  4480 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pVm, pClass);` |
|      3 |  4481 | `		if( rcMat != SXRET_OK ){` |
|      3 |  4482 | `			return rcMat;` |
|      - |  4483 | `		}` |
|    ! 0 |  4484 | `	}` |
|     22 |  4485 | `	pObj = PH7_NewClassInstance(pVm, pClass);` |
|     22 |  4486 | `	if( pObj == 0 ){` |
|    ! 0 |  4487 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4488 | `		return PH7_OK;` |
|      - |  4489 | `	}` |
|     22 |  4490 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|     22 |  4491 | `	if( pCons ){` |
|      - |  4492 | `		/* Weak binding, like ReflectMethodInvoke: the frame that makes this call is` |
|      - |  4493 | `		 * ReflectionClass::newInstance(), an internal function. */` |
|     20 |  4494 | `		pVm->bCallbackWeak = 1;` |
|     20 |  4495 | `		if( pNames ){` |
|      - |  4496 | `			VmCallArgMap sMap;` |
|    ! 0 |  4497 | `			SyZero(&sMap, sizeof(sMap));` |
|    ! 0 |  4498 | `			sMap.bHasNamed = 1;` |
|    ! 0 |  4499 | `			sMap.nTotal = (sxu32)nCtor;` |
|    ! 0 |  4500 | `			sMap.aNames = pNames;` |
|    ! 0 |  4501 | `			rc = PH7_VmCallClassMethodMap(pVm, pObj, pCons, 0, nCtor, apCtor, &sMap);` |
|    ! 0 |  4502 | `		}else{` |
|     20 |  4503 | `			rc = PH7_VmCallClassMethod(pVm, pObj, pCons, 0, nCtor, apCtor);` |
|      - |  4504 | `		}` |
|     20 |  4505 | `		pVm->bCallbackWeak = 0;` |
|     20 |  4506 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|      8 |  4507 | `			PH7_ClassInstanceCtorFailed(pObj);` |
|      8 |  4508 | `			PH7_ClassInstanceUnref(pObj);` |
|      8 |  4509 | `			return rc;` |
|      - |  4510 | `		}` |
|      5 |  4511 | `	}` |
|     14 |  4512 | `	return ReflectResultObject(pCtx, pObj);` |
|     23 |  4513 | `}` |
|     26 |  4514 | `static int vm_builtin_ReflectionClass_newInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  4515 | `{` |
|     31 |  4516 | `	return ReflectNewInstance(pCtx, nArg, apArg, 0);` |
|      5 |  4517 | `}` |
|     10 |  4518 | `static int vm_builtin_ReflectionClass_newInstanceArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  4519 | `{` |
|      - |  4520 | `	SySet aArg;` |
|     13 |  4521 | `	SyString *aNames = 0;` |
|      - |  4522 | `	int rc;` |
|     13 |  4523 | `	SySetInit(&aArg, &pCtx->pVm->sAllocator, sizeof(ph7_value *));` |
|     13 |  4524 | `	if( nArg > 0 ){` |
|     13 |  4525 | `		ReflectCollectArgs(pCtx, apArg[0], &aArg, &aNames);` |
|      5 |  4526 | `	}` |
|     18 |  4527 | `	rc = ReflectNewInstance(pCtx, (int)SySetUsed(&aArg),` |
|     10 |  4528 | `		(ph7_value **)SySetBasePtr(&aArg), aNames);` |
|     13 |  4529 | `	if( aNames ){` |
|    ! 0 |  4530 | `		SyMemBackendFree(&pCtx->pVm->sAllocator, aNames);` |
|    ! 0 |  4531 | `	}` |
|     13 |  4532 | `	SySetRelease(&aArg);` |
|     13 |  4533 | `	return rc;` |
|      3 |  4534 | `}` |
|    182 |  4535 | `static int vm_builtin_ReflectionClass_newInstanceWithoutConstructor(ph7_context *pCtx,` |
|      - |  4536 | `	int nArg, ph7_value **apArg)` |
|      3 |  4537 | `{` |
|    185 |  4538 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4539 | `	sxi32 rc;` |
|     91 |  4540 | `	SXUNUSED(nArg);` |
|     91 |  4541 | `	SXUNUSED(apArg);` |
|    185 |  4542 | `	if( pClass == 0 ){` |
|    ! 0 |  4543 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4544 | `		return PH7_OK;` |
|      - |  4545 | `	}` |
|    185 |  4546 | `	rc = PH7_VmCheckInstantiable(pCtx, pClass);` |
|    185 |  4547 | `	if( rc != PH7_OK ){` |
|      3 |  4548 | `		return rc;` |
|      - |  4549 | `	}` |
|    183 |  4550 | `	if( VmClassStaticDeferPending(pClass) ){` |
|      3 |  4551 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);` |
|      3 |  4552 | `		if( rcMat != SXRET_OK ){` |
|      3 |  4553 | `			return rcMat;` |
|      - |  4554 | `		}` |
|    ! 0 |  4555 | `	}` |
|    181 |  4556 | `	return ReflectResultObject(pCtx, PH7_NewClassInstance(pCtx->pVm, pClass));` |
|     94 |  4557 | `}` |
|      - |  4558 | `/* ---- members ---- */` |
|      - |  4559 | `/* The modifier mask php filters a member on. */` |
|    254 |  4560 | `static sxi64 ReflectVisMask(sxi32 iProt)` |
|      2 |  4561 | `{` |
|    256 |  4562 | `	if( iProt == PH7_CLASS_PROT_PUBLIC ){` |
|    178 |  4563 | `		return 1;` |
|      - |  4564 | `	}` |
|     79 |  4565 | `	return iProt == PH7_CLASS_PROT_PROTECTED ? 2 : 4;` |
|    129 |  4566 | `}` |
|      - |  4567 | `/*` |
|      - |  4568 | ` * Is this property protected(set) as far as php's Reflection is concerned?` |
|      - |  4569 | ` *` |
|      - |  4570 | `` * The bit is either DECLARED or implied by `readonly` — but readonly implies it`` |
|      - |  4571 | `` * only for a property whose read side is PUBLIC. A `protected readonly` or`` |
|      - |  4572 | `` * `private readonly` one already writes no wider than it reads, so php adds`` |
|      - |  4573 | `` * nothing (`private readonly int $v` is modifiers 132, not 2180), and an explicit`` |
|      - |  4574 | `` * `private(set)` beside readonly is the set visibility, so readonly adds nothing`` |
|      - |  4575 | `` * there either (`public private(set) readonly` is 4257).`` |
|      - |  4576 | ` */` |
|    230 |  4577 | `static int ReflectPropProtectedSet(ph7_class_attr *pAttr)` |
|      2 |  4578 | `{` |
|    232 |  4579 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET ){` |
|     28 |  4580 | `		return 1;` |
|      - |  4581 | `	}` |
|    225 |  4582 | `	return (pAttr->iFlags & PH7_CLASS_ATTR_READONLY)` |
|    122 |  4583 | `	    && (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) == 0` |
|    224 |  4584 | `	    && pAttr->iProtection == PH7_CLASS_PROT_PUBLIC;` |
|    117 |  4585 | `}` |
|    208 |  4586 | `static sxi64 ReflectPropModifiers(ph7_class_attr *pAttr)` |
|      2 |  4587 | `{` |
|    210 |  4588 | `	sxi64 iMods = ReflectVisMask(pAttr->iProtection);` |
|    210 |  4589 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){ iMods \|= 16; }` |
|      - |  4590 | `	/* php's IS_VIRTUAL is "there is no slot behind this name", and it has two` |
|      - |  4591 | `	 * sources: a HOOKED property with no backing store, and a NATIVE class's` |
|      - |  4592 | `	 * property that php fabricates from its own C struct (DatePeriod's, and` |
|      - |  4593 | `	 * BcMath\Number's value/scale). Both report virtual there. */` |
|    210 |  4594 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_VIRTUAL\|PH7_CLASS_ATTR_NATIVE_VIRTUAL) ){` |
|     65 |  4595 | `		iMods \|= 512;` |
|     32 |  4596 | `	}` |
|    210 |  4597 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){ iMods \|= 128; }` |
|    210 |  4598 | `	if( ReflectPropProtectedSet(pAttr) ){ iMods \|= 2048; }` |
|      - |  4599 | ``	/* PHP 8.4's `final` PROPERTY -- IS_FINAL, the same bit a method and a class`` |
|      - |  4600 | `	 * constant carry. */` |
|    210 |  4601 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods \|= 32; }` |
|    210 |  4602 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|      - |  4603 | `		/* private(set) cannot be widened by a subclass, so php reports it FINAL. */` |
|     19 |  4604 | `		iMods \|= 4096\|32;` |
|      9 |  4605 | `	}` |
|    210 |  4606 | `	return iMods;` |
|      2 |  4607 | `}` |
|     20 |  4608 | `static sxi64 ReflectMethodModifiers(ph7_class_method *pMeth)` |
|      2 |  4609 | `{` |
|     22 |  4610 | `	sxi64 iMods = ReflectVisMask(pMeth->iProtection);` |
|     22 |  4611 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){ iMods \|= 16; }` |
|     22 |  4612 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){ iMods \|= 64; }` |
|     22 |  4613 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods \|= 32; }` |
|     22 |  4614 | `	return iMods;` |
|      2 |  4615 | `}` |
|     26 |  4616 | `static sxi64 ReflectConstModifiers(ph7_class_attr *pAttr)` |
|      1 |  4617 | `{` |
|     27 |  4618 | `	sxi64 iMods = ReflectVisMask(pAttr->iProtection);` |
|     27 |  4619 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods \|= 32; }` |
|     27 |  4620 | `	return iMods;` |
|      1 |  4621 | `}` |
|      - |  4622 | `/* Does the reflected object own a property under this name? (1 = exists) */` |
|     18 |  4623 | `static int ReflectObjHasProp(ph7_class_instance *pObj, const char *zName, int nName)` |
|      1 |  4624 | `{` |
|     14 |  4625 | `	return pObj != 0 && nName > 0` |
|     20 |  4626 | `		&& SyHashGet(&pObj->hAttr, (const void *)zName, (sxu32)nName) != 0;` |
|      1 |  4627 | `}` |
|     18 |  4628 | `static int vm_builtin_ReflectionClass_hasMethod(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4629 | `{` |
|     20 |  4630 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4631 | `	const char *zName;` |
|      - |  4632 | `	int nName;` |
|     20 |  4633 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  4634 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4635 | `		return PH7_OK;` |
|      - |  4636 | `	}` |
|     20 |  4637 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     20 |  4638 | `	ph7_result_bool(pCtx, ReflectFindMethodEntry(pClass, zName, nName) != 0);` |
|     20 |  4639 | `	return PH7_OK;` |
|     11 |  4640 | `}` |
|     18 |  4641 | `static int vm_builtin_ReflectionClass_hasProperty(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4642 | `{` |
|     20 |  4643 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4644 | `	const char *zName;` |
|     20 |  4645 | `	int nName, bFound = 0;` |
|      - |  4646 | `	SySet aMembers;` |
|      - |  4647 | `	sxu32 n;` |
|     20 |  4648 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  4649 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4650 | `		return PH7_OK;` |
|      - |  4651 | `	}` |
|     20 |  4652 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     20 |  4653 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     20 |  4654 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     56 |  4655 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     44 |  4656 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     44 |  4657 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zName, nName) ){` |
|      8 |  4658 | `			bFound = 1;` |
|      8 |  4659 | `			break;` |
|      - |  4660 | `		}` |
|     19 |  4661 | `	}` |
|     20 |  4662 | `	SySetRelease(&aMembers);` |
|      - |  4663 | `	/* A ReflectionObject also sees the instance's own dynamic properties */` |
|     20 |  4664 | `	if( !bFound ){` |
|     13 |  4665 | `		bFound = ReflectObjHasProp(ReflectClassObj(pCtx), zName, nName);` |
|      6 |  4666 | `	}` |
|     20 |  4667 | `	ph7_result_bool(pCtx, bFound);` |
|     20 |  4668 | `	return PH7_OK;` |
|     11 |  4669 | `}` |
|     24 |  4670 | `static int vm_builtin_ReflectionClass_hasConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4671 | `{` |
|     26 |  4672 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4673 | `	const char *zName;` |
|     26 |  4674 | `	int nName, bFound = 0;` |
|      - |  4675 | `	SySet aMembers;` |
|      - |  4676 | `	sxu32 n;` |
|     26 |  4677 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  4678 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4679 | `		return PH7_OK;` |
|      - |  4680 | `	}` |
|     26 |  4681 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     26 |  4682 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     26 |  4683 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|   1320 |  4684 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   1300 |  4685 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   1300 |  4686 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|      5 |  4687 | `			bFound = 1;` |
|      5 |  4688 | `			break;` |
|      - |  4689 | `		}` |
|    649 |  4690 | `	}` |
|     26 |  4691 | `	SySetRelease(&aMembers);` |
|     26 |  4692 | `	ph7_result_bool(pCtx, bFound);` |
|     26 |  4693 | `	return PH7_OK;` |
|     14 |  4694 | `}` |
|      - |  4695 | `/*` |
|      - |  4696 | ` * The value of a class constant, materializing its lazily-evaluated slot.` |
|      - |  4697 | ` *` |
|      - |  4698 | ` * php re-evaluates a failed initializer on EVERY read, so this can raise; the` |
|      - |  4699 | ` * status is returned rather than swallowed, because a native body that answers` |
|      - |  4700 | ` * PH7_OK with a throw in flight lets the caller carry on and print the NULL it` |
|      - |  4701 | ` * never should have seen.` |
|      - |  4702 | ` */` |
|    816 |  4703 | `static sxi32 ReflectConstSlot(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr,` |
|      - |  4704 | `	ph7_value **ppOut)` |
|      3 |  4705 | `{` |
|    819 |  4706 | `	sxi32 rc = PH7_VmMaterializeClassConst(pCtx->pVm, pClass, pAttr);` |
|    819 |  4707 | `	*ppOut = 0;` |
|    819 |  4708 | `	if( rc != SXRET_OK ){` |
|      3 |  4709 | `		return rc;` |
|      - |  4710 | `	}` |
|    817 |  4711 | `	*ppOut = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|    817 |  4712 | `	return SXRET_OK;` |
|    397 |  4713 | `}` |
|     10 |  4714 | `static int vm_builtin_ReflectionClass_getConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4715 | `{` |
|     12 |  4716 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4717 | `	const char *zName;` |
|      - |  4718 | `	int nName;` |
|      - |  4719 | `	SySet aMembers;` |
|      - |  4720 | `	sxu32 n;` |
|     12 |  4721 | `	ph7_class_attr *pFound = 0;` |
|     12 |  4722 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  4723 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4724 | `		return PH7_OK;` |
|      - |  4725 | `	}` |
|     12 |  4726 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     12 |  4727 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     12 |  4728 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     28 |  4729 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     28 |  4730 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     28 |  4731 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|     12 |  4732 | `			pFound = pM->pAttr;` |
|     12 |  4733 | `			break;` |
|      - |  4734 | `		}` |
|      9 |  4735 | `	}` |
|     12 |  4736 | `	SySetRelease(&aMembers);` |
|     12 |  4737 | `	if( pFound == 0 ){` |
|    ! 0 |  4738 | `		PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED,` |
|      - |  4739 | `			"ReflectionClass::getConstant() for a non-existent constant is deprecated, "` |
|      - |  4740 | `			"use ReflectionClass::hasConstant() to check if the constant exists");` |
|    ! 0 |  4741 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4742 | `		return PH7_OK;` |
|      - |  4743 | `	}` |
|      - |  4744 | `	{` |
|      - |  4745 | `		ph7_value *pVal;` |
|     12 |  4746 | `		sxi32 rc = ReflectConstSlot(pCtx, pClass, pFound, &pVal);` |
|     12 |  4747 | `		if( rc != SXRET_OK ){` |
|      3 |  4748 | `			return rc;` |
|      - |  4749 | `		}` |
|      9 |  4750 | `		if( pVal ){` |
|      9 |  4751 | `			ph7_result_value(pCtx, pVal);` |
|      5 |  4752 | `		}else{` |
|    ! 0 |  4753 | `			ph7_result_null(pCtx);` |
|      - |  4754 | `		}` |
|      - |  4755 | `	}` |
|      9 |  4756 | `	return PH7_OK;` |
|      7 |  4757 | `}` |
|     55 |  4758 | `static int vm_builtin_ReflectionClass_getConstants(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  4759 | `{` |
|     60 |  4760 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     60 |  4761 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  4762 | `	SySet aMembers;` |
|      - |  4763 | `	sxu32 n;` |
|     60 |  4764 | `	int bFilter = (nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0);` |
|     60 |  4765 | `	sxi64 iFilter = bFilter ? ph7_value_to_int64(apArg[0]) : 0;` |
|     60 |  4766 | `	if( pOut == 0 ){` |
|    ! 0 |  4767 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  4768 | `	}` |
|     60 |  4769 | `	if( pClass == 0 ){` |
|    ! 0 |  4770 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  4771 | `		return PH7_OK;` |
|      - |  4772 | `	}` |
|     60 |  4773 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     60 |  4774 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|    485 |  4775 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    429 |  4776 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - |  4777 | `		ph7_value *pVal;` |
|    429 |  4778 | `		if( pM->iKind != REFLECT_MEMBER_CONST ){` |
|    305 |  4779 | `			continue;` |
|      - |  4780 | `		}` |
|    128 |  4781 | `		if( bFilter && (ReflectConstModifiers(pM->pAttr) & iFilter) == 0 ){` |
|      5 |  4782 | `			continue;` |
|      - |  4783 | `		}` |
|      - |  4784 | `		{` |
|    124 |  4785 | `			sxi32 rc = ReflectConstSlot(pCtx, pClass, pM->pAttr, &pVal);` |
|    124 |  4786 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  4787 | `				SySetRelease(&aMembers);` |
|    ! 0 |  4788 | `				return rc;` |
|      - |  4789 | `			}` |
|      - |  4790 | `		}` |
|    124 |  4791 | `		if( pVal ){` |
|    124 |  4792 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, pVal);` |
|     47 |  4793 | `		}` |
|     49 |  4794 | `	}` |
|     60 |  4795 | `	SySetRelease(&aMembers);` |
|     60 |  4796 | `	ph7_result_value(pCtx, pOut);` |
|     60 |  4797 | `	return PH7_OK;` |
|     32 |  4798 | `}` |
|      - |  4799 | `/*` |
|      - |  4800 | ` * getMethod()/getProperty()/getReflectionConstant() build a class that is still` |
|      - |  4801 | ` * PRELUDE PHP, so they go through its constructor (ReflectConstruct). They` |
|      - |  4802 | ` * become direct C when chunks 2 and 3 land.` |
|      - |  4803 | ` */` |
|     94 |  4804 | `static int ReflectResultMember(ph7_context *pCtx, const char *zClass,` |
|      - |  4805 | `	ph7_value *pTarget, const SyString *pName)` |
|      4 |  4806 | `{` |
|      - |  4807 | `	ph7_value sName;` |
|      - |  4808 | `	ph7_value *apCtor[2];` |
|      - |  4809 | `	ph7_class_instance *pOut;` |
|      - |  4810 | `	sxi32 rc;` |
|     98 |  4811 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|     98 |  4812 | `	ph7_value_string(&sName, SyStringData(pName), (int)SyStringLength(pName));` |
|     98 |  4813 | `	apCtor[0] = pTarget;` |
|     98 |  4814 | `	apCtor[1] = &sName;` |
|      - |  4815 | `	/* A FACTORY build, not a user constructor call — see ph7_vm::nReflectFactory. */` |
|     98 |  4816 | `	pCtx->pVm->nReflectFactory++;` |
|     98 |  4817 | `	pOut = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|     98 |  4818 | `	pCtx->pVm->nReflectFactory--;` |
|     98 |  4819 | `	PH7_MemObjRelease(&sName);` |
|     98 |  4820 | `	if( pOut == 0 ){` |
|    ! 0 |  4821 | `		if( rc != PH7_OK ){` |
|    ! 0 |  4822 | `			return rc;` |
|      - |  4823 | `		}` |
|    ! 0 |  4824 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4825 | `		return PH7_OK;` |
|      - |  4826 | `	}` |
|     98 |  4827 | `	return ReflectResultObject(pCtx, pOut);` |
|     51 |  4828 | `}` |
|      - |  4829 | `/*` |
|      - |  4830 | `` * A `ReflectionMethod($this->..., ...)`-shaped first argument.`` |
|      - |  4831 | ` *` |
|      - |  4832 | ` * The OBJECT when this reflector was built over one (a ReflectionObject), its` |
|      - |  4833 | ` * class NAME otherwise. For every ordinary member the two are interchangeable --` |
|      - |  4834 | ` * the constructor resolves an object to its class either way -- but not for a` |
|      - |  4835 | `` * FABRICATED method: `(new ReflectionObject($c))->getMethod('__invoke')` describes`` |
|      - |  4836 | `` * THE CLOSURE's parameters where `(new ReflectionClass('Closure'))`` |
|      - |  4837 | `` * ->getMethod('__invoke')` has nothing to describe, and php draws that line at`` |
|      - |  4838 | ` * exactly this question. Takes a reference, so the caller's release balances.` |
|      - |  4839 | ` */` |
|    356 |  4840 | `static void ReflectSelfName(ph7_context *pCtx, ph7_value *pOut)` |
|      5 |  4841 | `{` |
|    361 |  4842 | `	ph7_class_instance *pObj = ReflectClassObj(pCtx);` |
|      - |  4843 | `	const char *zName;` |
|      - |  4844 | `	int nName;` |
|    361 |  4845 | `	PH7_MemObjInit(pCtx->pVm, pOut);` |
|    361 |  4846 | `	if( pObj ){` |
|     24 |  4847 | `		pObj->iRef++;` |
|     24 |  4848 | `		pOut->x.pOther = pObj;` |
|     24 |  4849 | `		pOut->iFlags = MEMOBJ_OBJ;` |
|     24 |  4850 | `		return;` |
|      - |  4851 | `	}` |
|    339 |  4852 | `	ReflectClassName(pCtx, &zName, &nName);` |
|    339 |  4853 | `	ph7_value_string(pOut, zName, nName);` |
|    183 |  4854 | `}` |
|     60 |  4855 | `static int vm_builtin_ReflectionClass_getMethod(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4856 | `{` |
|     62 |  4857 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4858 | `	SyHashEntry *pEntry;` |
|      - |  4859 | `	const char *zName;` |
|      - |  4860 | `	int nName;` |
|      - |  4861 | `	SyString sFound;` |
|     62 |  4862 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  4863 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4864 | `		return PH7_OK;` |
|      - |  4865 | `	}` |
|     62 |  4866 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     62 |  4867 | `	pEntry = ReflectFindMethodEntry(pClass, zName, nName);` |
|     62 |  4868 | `	if( pEntry == 0 ){` |
|      4 |  4869 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  4870 | `			"Method %z::%.*s() does not exist", &pClass->sDisp, nName, zName);` |
|      - |  4871 | `	}` |
|      - |  4872 | `	/* The reported name is the DECLARED spelling, whatever case was asked for. */` |
|     60 |  4873 | `	SyStringInitFromBuf(&sFound, (const char *)pEntry->pKey, pEntry->nKeyLen);` |
|      - |  4874 | `	{` |
|      - |  4875 | `		ph7_value sSelf;` |
|      - |  4876 | `		int rc;` |
|     60 |  4877 | `		ReflectSelfName(pCtx, &sSelf);` |
|     60 |  4878 | `		rc = ReflectResultMember(pCtx, "ReflectionMethod", &sSelf, &sFound);` |
|     60 |  4879 | `		PH7_MemObjRelease(&sSelf);` |
|     60 |  4880 | `		return rc;` |
|      - |  4881 | `	}` |
|     32 |  4882 | `}` |
|     28 |  4883 | `static int vm_builtin_ReflectionClass_getConstructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  4884 | `{` |
|     31 |  4885 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4886 | `	SyHashEntry *pEntry;` |
|      - |  4887 | `	SyString sFound;` |
|     14 |  4888 | `	SXUNUSED(nArg);` |
|     14 |  4889 | `	SXUNUSED(apArg);` |
|     31 |  4890 | `	if( pClass == 0 ){` |
|    ! 0 |  4891 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4892 | `		return PH7_OK;` |
|      - |  4893 | `	}` |
|      - |  4894 | `	/* No PHP-4 class-name constructor: a method named like the class is a plain` |
|      - |  4895 | `	 * method (removed in 8.0), so this stays null without an explicit one. */` |
|     31 |  4896 | `	pEntry = ReflectFindMethodEntry(pClass, "__construct", sizeof("__construct")-1);` |
|     31 |  4897 | `	if( pEntry == 0 ){` |
|     11 |  4898 | `		ph7_result_null(pCtx);` |
|     11 |  4899 | `		return PH7_OK;` |
|      - |  4900 | `	}` |
|     23 |  4901 | `	SyStringInitFromBuf(&sFound, (const char *)pEntry->pKey, pEntry->nKeyLen);` |
|      - |  4902 | `	{` |
|      - |  4903 | `		ph7_value sSelf;` |
|      - |  4904 | `		int rc;` |
|     23 |  4905 | `		ReflectSelfName(pCtx, &sSelf);` |
|     23 |  4906 | `		rc = ReflectResultMember(pCtx, "ReflectionMethod", &sSelf, &sFound);` |
|     23 |  4907 | `		PH7_MemObjRelease(&sSelf);` |
|     23 |  4908 | `		return rc;` |
|      - |  4909 | `	}` |
|     17 |  4910 | `}` |
|      - |  4911 | `/*` |
|      - |  4912 | ` * getMethods() / getProperties() / getReflectionConstants(): one member walk,` |
|      - |  4913 | ` * one reflector per surviving member. Each reflector is a prelude class built` |
|      - |  4914 | ` * through its constructor, and is appended through a STACK carrier so the list` |
|      - |  4915 | ` * takes its own reference rather than the creation one.` |
|      - |  4916 | ` */` |
|    264 |  4917 | `static int ReflectMemberList(ph7_context *pCtx, int iKind, const char *zClass,` |
|      - |  4918 | `	int nArg, ph7_value **apArg)` |
|      5 |  4919 | `{` |
|    269 |  4920 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|    269 |  4921 | `	ph7_class_instance *pObj = ReflectClassObj(pCtx);` |
|    269 |  4922 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  4923 | `	ph7_value sSelf;` |
|      - |  4924 | `	SySet aMembers;` |
|      - |  4925 | `	sxu32 n;` |
|    269 |  4926 | `	int bFilter = (nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0);` |
|    269 |  4927 | `	sxi64 iFilter = bFilter ? ph7_value_to_int64(apArg[0]) : 0;` |
|    269 |  4928 | `	if( pOut == 0 ){` |
|    ! 0 |  4929 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  4930 | `	}` |
|    269 |  4931 | `	if( pClass == 0 ){` |
|    ! 0 |  4932 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  4933 | `		return PH7_OK;` |
|      - |  4934 | `	}` |
|    269 |  4935 | `	ReflectSelfName(pCtx, &sSelf);` |
|    269 |  4936 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|    269 |  4937 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|   3145 |  4938 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   2881 |  4939 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - |  4940 | `		ph7_value sName, sVal;` |
|      - |  4941 | `		ph7_value *apCtor[2];` |
|      - |  4942 | `		ph7_class_instance *pRef;` |
|      - |  4943 | `		sxi32 rc;` |
|   2881 |  4944 | `		if( pM->iKind != iKind ){` |
|   2006 |  4945 | `			continue;` |
|      - |  4946 | `		}` |
|    918 |  4947 | `		if( iKind == REFLECT_MEMBER_PROP && pObj != 0 && pM->pAttr != 0` |
|    142 |  4948 | `		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT) == 0` |
|    133 |  4949 | `		 && PH7_ATTR_LAZY_ABSENT(pM->pAttr,pObj) ){` |
|      - |  4950 | `			/* A ReflectionObject reflects the OBJECT, and a LAZY property php does` |
|      - |  4951 | `			 * not DECLARE (DateInterval's ten) is not on one that was never` |
|      - |  4952 | `			 * constructed -- php reports none there either. The declared-and-virtual` |
|      - |  4953 | `			 * kind (DatePeriod's seven, PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT) is` |
|      - |  4954 | `			 * reported whatever the object holds, because php declares it. */` |
|     45 |  4955 | `			continue;` |
|      - |  4956 | `		}` |
|    874 |  4957 | `		if( iKind == REFLECT_MEMBER_PROP && pObj == 0 && pM->pAttr != 0` |
|    237 |  4958 | `		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND) != 0 ){` |
|      - |  4959 | `			/* An ON-DEMAND property belongs to the objects that took it, so the` |
|      - |  4960 | `			 * CLASS's list -- which php builds from declarations and DateInterval` |
|      - |  4961 | `			 * has none of -- does not gain a name for it. */` |
|    ! 0 |  4962 | `			continue;` |
|      - |  4963 | `		}` |
|    874 |  4964 | `		if( iKind == REFLECT_MEMBER_PROP && pObj != 0 && pM->pAttr != 0` |
|     98 |  4965 | `		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY) != 0` |
|    101 |  4966 | `		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT) == 0 ){` |
|      - |  4967 | `			/* ...and for the same reason a property the object HIDES or never took` |
|      - |  4968 | `			 * is not on it either: php's from-string DateInterval reflects the two` |
|      - |  4969 | ``			 * names it presents, and an ordinary one has no `date_string` at all. */`` |
|    100 |  4970 | `			SyHashEntry *pOwn = SyHashGet(&pObj->hAttr,` |
|     66 |  4971 | `				SyStringData(&pM->pAttr->sName),SyStringLength(&pM->pAttr->sName));` |
|     66 |  4972 | `			if( pOwn == 0` |
|     65 |  4973 | `			 \|\| (((VmClassAttr *)pOwn->pUserData)->iState & VM_CLASS_ATTR_UNSEEN) ){` |
|     23 |  4974 | `				continue;` |
|      - |  4975 | `			}` |
|     22 |  4976 | `		}` |
|    857 |  4977 | `		if( bFilter ){` |
|     33 |  4978 | `			sxi64 iMods = iKind == REFLECT_MEMBER_METHOD ? ReflectMethodModifiers(pM->pMeth)` |
|     40 |  4979 | `				: (iKind == REFLECT_MEMBER_CONST ? ReflectConstModifiers(pM->pAttr)` |
|     20 |  4980 | `				                                 : ReflectPropModifiers(pM->pAttr));` |
|     33 |  4981 | `			if( (iMods & iFilter) == 0 ){` |
|     21 |  4982 | `				continue;` |
|      - |  4983 | `			}` |
|      6 |  4984 | `		}` |
|    837 |  4985 | `		PH7_MemObjInit(pCtx->pVm, &sName);` |
|    837 |  4986 | `		ph7_value_string(&sName, SyStringData(&pM->sKey), (int)SyStringLength(&pM->sKey));` |
|    837 |  4987 | `		apCtor[0] = &sSelf;` |
|    837 |  4988 | `		apCtor[1] = &sName;` |
|    837 |  4989 | `		pCtx->pVm->nReflectFactory++;   /* see ph7_vm::nReflectFactory */` |
|    837 |  4990 | `		pRef = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|    837 |  4991 | `		pCtx->pVm->nReflectFactory--;` |
|    837 |  4992 | `		PH7_MemObjRelease(&sName);` |
|    837 |  4993 | `		if( pRef == 0 ){` |
|    ! 0 |  4994 | `			SySetRelease(&aMembers);` |
|    ! 0 |  4995 | `			PH7_MemObjRelease(&sSelf);` |
|    ! 0 |  4996 | `			if( rc != PH7_OK ){` |
|    ! 0 |  4997 | `				return rc;` |
|      - |  4998 | `			}` |
|    ! 0 |  4999 | `			ph7_result_value(pCtx, pOut);` |
|    ! 0 |  5000 | `			return PH7_OK;` |
|      - |  5001 | `		}` |
|    837 |  5002 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    837 |  5003 | `		sVal.x.pOther = pRef;` |
|    837 |  5004 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|    837 |  5005 | `		ph7_array_add_elem(pOut, 0, &sVal);   /* takes its own reference */` |
|    837 |  5006 | `		PH7_ClassInstanceUnref(pRef);` |
|    421 |  5007 | `	}` |
|    269 |  5008 | `	SySetRelease(&aMembers);` |
|      - |  5009 | `	/* A ReflectionObject also reports the instance's own DYNAMIC properties,` |
|      - |  5010 | `	 * which no class declaration knows about. */` |
|    269 |  5011 | `	if( iKind == REFLECT_MEMBER_PROP && pObj != 0 && (!bFilter \|\| (iFilter & 1)) ){` |
|      - |  5012 | `		SyHashEntry *pEntry;` |
|      - |  5013 | `		ph7_value sTarget;` |
|     17 |  5014 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     17 |  5015 | `		sTarget.x.pOther = pObj;` |
|     17 |  5016 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|     17 |  5017 | `		SyHashResetLoopCursor(&pObj->hAttr);` |
|    111 |  5018 | `		while( (pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){` |
|     95 |  5019 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - |  5020 | `			ph7_value sName, sVal;` |
|      - |  5021 | `			ph7_value *apCtor[2];` |
|      - |  5022 | `			ph7_class_instance *pRef;` |
|      - |  5023 | `			sxi32 rc;` |
|     95 |  5024 | `			if( pVmAttr->pAttr == 0 \|\| (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) == 0 ){` |
|     91 |  5025 | `				continue;` |
|      - |  5026 | `			}` |
|      5 |  5027 | `			PH7_MemObjInit(pCtx->pVm, &sName);` |
|      7 |  5028 | `			ph7_value_string(&sName, SyStringData(&pVmAttr->pAttr->sName),` |
|      4 |  5029 | `				(int)SyStringLength(&pVmAttr->pAttr->sName));` |
|      5 |  5030 | `			apCtor[0] = &sTarget;` |
|      5 |  5031 | `			apCtor[1] = &sName;` |
|      5 |  5032 | `			pRef = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|      5 |  5033 | `			PH7_MemObjRelease(&sName);` |
|      5 |  5034 | `			if( pRef == 0 ){` |
|    ! 0 |  5035 | `				break;` |
|      - |  5036 | `			}` |
|      5 |  5037 | `			PH7_MemObjInit(pCtx->pVm, &sVal);` |
|      5 |  5038 | `			sVal.x.pOther = pRef;` |
|      5 |  5039 | `			sVal.iFlags = MEMOBJ_OBJ;` |
|      5 |  5040 | `			ph7_array_add_elem(pOut, 0, &sVal);` |
|      5 |  5041 | `			PH7_ClassInstanceUnref(pRef);` |
|      1 |  5042 | `		}` |
|      - |  5043 | `		/* sTarget borrows pObj and never took a reference: nothing to release. */` |
|      8 |  5044 | `	}` |
|    269 |  5045 | `	PH7_MemObjRelease(&sSelf);` |
|    269 |  5046 | `	ph7_result_value(pCtx, pOut);` |
|    269 |  5047 | `	return PH7_OK;` |
|    137 |  5048 | `}` |
|    114 |  5049 | `static int vm_builtin_ReflectionClass_getMethods(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  5050 | `{` |
|    119 |  5051 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_METHOD, "ReflectionMethod", nArg, apArg);` |
|      5 |  5052 | `}` |
|    144 |  5053 | `static int vm_builtin_ReflectionClass_getProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  5054 | `{` |
|    149 |  5055 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_PROP, "ReflectionProperty", nArg, apArg);` |
|      5 |  5056 | `}` |
|      6 |  5057 | `static int vm_builtin_ReflectionClass_getReflectionConstants(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  5058 | `{` |
|      7 |  5059 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_CONST, "ReflectionClassConstant", nArg, apArg);` |
|      1 |  5060 | `}` |
|     12 |  5061 | `static int vm_builtin_ReflectionClass_getProperty(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  5062 | `{` |
|     13 |  5063 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     13 |  5064 | `	ph7_class_instance *pObj = ReflectClassObj(pCtx);` |
|      - |  5065 | `	const char *zName;` |
|     13 |  5066 | `	int nName, bFound = 0;` |
|      - |  5067 | `	SySet aMembers;` |
|      - |  5068 | `	sxu32 n;` |
|      - |  5069 | `	SyString sFound;` |
|     13 |  5070 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  5071 | `		ph7_result_null(pCtx);` |
|    ! 0 |  5072 | `		return PH7_OK;` |
|      - |  5073 | `	}` |
|     13 |  5074 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     13 |  5075 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     13 |  5076 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     49 |  5077 | `	for( n = 0 ; n < SySetUsed(&aMembers) && !bFound ; n++ ){` |
|     37 |  5078 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     37 |  5079 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zName, nName) ){` |
|      7 |  5080 | `			sFound = pM->sKey;` |
|      7 |  5081 | `			bFound = 1;` |
|      3 |  5082 | `		}` |
|     19 |  5083 | `	}` |
|     13 |  5084 | `	SySetRelease(&aMembers);` |
|     13 |  5085 | `	if( bFound ){` |
|      - |  5086 | `		ph7_value sSelf;` |
|      - |  5087 | `		int rc;` |
|      7 |  5088 | `		ReflectSelfName(pCtx, &sSelf);` |
|      7 |  5089 | `		rc = ReflectResultMember(pCtx, "ReflectionProperty", &sSelf, &sFound);` |
|      7 |  5090 | `		PH7_MemObjRelease(&sSelf);` |
|      7 |  5091 | `		return rc;` |
|      - |  5092 | `	}` |
|      7 |  5093 | `	if( ReflectObjHasProp(pObj, zName, nName) ){` |
|      - |  5094 | `		/* A dynamic property: the reflector is built over the OBJECT, since the` |
|      - |  5095 | `		 * class declaration has no record of it. */` |
|      - |  5096 | `		ph7_value sTarget;` |
|      - |  5097 | `		SyString sName;` |
|      - |  5098 | `		int rc;` |
|      3 |  5099 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      3 |  5100 | `		sTarget.x.pOther = pObj;` |
|      3 |  5101 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|      3 |  5102 | `		SyStringInitFromBuf(&sName, zName, nName);` |
|      3 |  5103 | `		rc = ReflectResultMember(pCtx, "ReflectionProperty", &sTarget, &sName);` |
|      3 |  5104 | `		return rc;` |
|      - |  5105 | `	}` |
|      7 |  5106 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 |  5107 | `		"Property %z::$%.*s does not exist", &pClass->sDisp, nName, zName);` |
|      7 |  5108 | `}` |
|     10 |  5109 | `static int vm_builtin_ReflectionClass_getReflectionConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  5110 | `{` |
|     11 |  5111 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  5112 | `	const char *zName;` |
|     11 |  5113 | `	int nName, bFound = 0;` |
|      - |  5114 | `	SySet aMembers;` |
|      - |  5115 | `	sxu32 n;` |
|      - |  5116 | `	SyString sFound;` |
|     11 |  5117 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  5118 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  5119 | `		return PH7_OK;` |
|      - |  5120 | `	}` |
|     11 |  5121 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     11 |  5122 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     11 |  5123 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     33 |  5124 | `	for( n = 0 ; n < SySetUsed(&aMembers) && !bFound ; n++ ){` |
|     23 |  5125 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     23 |  5126 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|      9 |  5127 | `			sFound = pM->sKey;` |
|      9 |  5128 | `			bFound = 1;` |
|      4 |  5129 | `		}` |
|     12 |  5130 | `	}` |
|     11 |  5131 | `	SySetRelease(&aMembers);` |
|     11 |  5132 | `	if( !bFound ){` |
|      3 |  5133 | `		ph7_result_bool(pCtx, 0);` |
|      3 |  5134 | `		return PH7_OK;` |
|      - |  5135 | `	}` |
|      - |  5136 | `	{` |
|      - |  5137 | `		ph7_value sSelf;` |
|      - |  5138 | `		int rc;` |
|      9 |  5139 | `		ReflectSelfName(pCtx, &sSelf);` |
|      9 |  5140 | `		rc = ReflectResultMember(pCtx, "ReflectionClassConstant", &sSelf, &sFound);` |
|      9 |  5141 | `		PH7_MemObjRelease(&sSelf);` |
|      9 |  5142 | `		return rc;` |
|      - |  5143 | `	}` |
|      6 |  5144 | `}` |
|      - |  5145 | `/* ---- statics and defaults ---- */` |
|      - |  5146 | `/* Reading or writing a static through reflection materializes the class's` |
|      - |  5147 | `` * static table exactly as `C::$s` does, so a default that threw at the`` |
|      - |  5148 | ` * declaration raises HERE. */` |
|     52 |  5149 | `static sxi32 ReflectMaterializeStatics(ph7_context *pCtx, ph7_class *pClass)` |
|      2 |  5150 | `{` |
|     54 |  5151 | `	if( VmClassStaticDeferPending(pClass) ){` |
|     21 |  5152 | `		return PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);` |
|      - |  5153 | `	}` |
|     34 |  5154 | `	return SXRET_OK;` |
|     28 |  5155 | `}` |
|      8 |  5156 | `static int vm_builtin_ReflectionClass_getStaticProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  5157 | `{` |
|     10 |  5158 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     10 |  5159 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  5160 | `	SySet aMembers;` |
|      - |  5161 | `	sxu32 n;` |
|      4 |  5162 | `	SXUNUSED(nArg);` |
|      4 |  5163 | `	SXUNUSED(apArg);` |
|     10 |  5164 | `	if( pOut == 0 ){` |
|    ! 0 |  5165 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  5166 | `	}` |
|     10 |  5167 | `	if( pClass == 0 ){` |
|    ! 0 |  5168 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  5169 | `		return PH7_OK;` |
|      - |  5170 | `	}` |
|      - |  5171 | `	{` |
|     10 |  5172 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|     10 |  5173 | `		if( rc != SXRET_OK ){` |
|      3 |  5174 | `			return rc;` |
|      - |  5175 | `		}` |
|      - |  5176 | `	}` |
|      7 |  5177 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|      7 |  5178 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     39 |  5179 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     33 |  5180 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - |  5181 | `		ph7_value *pVal;` |
|      - |  5182 | `		SyHashEntry *pSlot;` |
|     32 |  5183 | `		if( pM->iKind != REFLECT_MEMBER_PROP` |
|     29 |  5184 | `		 \|\| (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|     21 |  5185 | `			continue;` |
|      - |  5186 | `		}` |
|      - |  5187 | `		/* An UNINITIALIZED typed static has no value to report, and php simply` |
|      - |  5188 | `		 * leaves it out rather than raising the read Error here. */` |
|     13 |  5189 | `		pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pM->pAttr->nIdx, sizeof(sxu32));` |
|     13 |  5190 | `		if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|      3 |  5191 | `			continue;` |
|      - |  5192 | `		}` |
|     11 |  5193 | `		pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pM->pAttr->nIdx);` |
|     11 |  5194 | `		if( pVal ){` |
|     11 |  5195 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, pVal);` |
|      5 |  5196 | `		}` |
|      6 |  5197 | `	}` |
|      7 |  5198 | `	SySetRelease(&aMembers);` |
|      7 |  5199 | `	ph7_result_value(pCtx, pOut);` |
|      7 |  5200 | `	return PH7_OK;` |
|      6 |  5201 | `}` |
|      - |  5202 | `/* The declared STATIC property of this name, or NULL. */` |
|     18 |  5203 | `static ph7_class_attr * ReflectStaticAttr(ph7_context *pCtx, ph7_class *pClass,` |
|      - |  5204 | `	const char *zName, int nName)` |
|      2 |  5205 | `{` |
|      - |  5206 | `	SyHashEntry *pEntry;` |
|      - |  5207 | `	ph7_class_attr *pAttr;` |
|     20 |  5208 | `	if( nName < 1 ){` |
|    ! 0 |  5209 | `		return 0;` |
|      - |  5210 | `	}` |
|     20 |  5211 | `	pEntry = SyHashGet(&pClass->hAttr, (const void *)zName, (sxu32)nName);` |
|     20 |  5212 | `	if( pEntry == 0 ){` |
|      9 |  5213 | `		return 0;` |
|      - |  5214 | `	}` |
|     12 |  5215 | `	pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      5 |  5216 | `	SXUNUSED(pCtx);` |
|     12 |  5217 | `	return (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? pAttr : 0;` |
|     11 |  5218 | `}` |
|     20 |  5219 | `static int vm_builtin_ReflectionClass_getStaticPropertyValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  5220 | `{` |
|     22 |  5221 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  5222 | `	ph7_class_attr *pAttr;` |
|      - |  5223 | `	const char *zName;` |
|      - |  5224 | `	int nName;` |
|     22 |  5225 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  5226 | `		ph7_result_null(pCtx);` |
|    ! 0 |  5227 | `		return PH7_OK;` |
|      - |  5228 | `	}` |
|      - |  5229 | `	{` |
|     22 |  5230 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|     22 |  5231 | `		if( rc != SXRET_OK ){` |
|      7 |  5232 | `			return rc;` |
|      - |  5233 | `		}` |
|      - |  5234 | `	}` |
|     16 |  5235 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     16 |  5236 | `	pAttr = ReflectStaticAttr(pCtx, pClass, zName, nName);` |
|     16 |  5237 | `	if( pAttr == 0 ){` |
|      7 |  5238 | `		if( nArg > 1 ){` |
|      5 |  5239 | `			ph7_result_value(pCtx, apArg[1]);` |
|      5 |  5240 | `			return PH7_OK;` |
|      - |  5241 | `		}` |
|      4 |  5242 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  5243 | `			"Property %z::$%.*s does not exist", &pClass->sDisp, nName, zName);` |
|      - |  5244 | `	}` |
|      - |  5245 | `	{` |
|      - |  5246 | `		/* Uninitialized typed static: the same Error the VM raises on read */` |
|     10 |  5247 | `		SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|      - |  5248 | `		ph7_value *pVal;` |
|     10 |  5249 | `		if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|      - |  5250 | `			/* php says "Typed property" HERE and "Typed static property" from` |
|      - |  5251 | ``			 * ReflectionProperty::getValue() and from `A::$s` itself — the`` |
|      - |  5252 | `			 * three wordings were checked against the oracle, they really do` |
|      - |  5253 | `			 * differ by call site. */` |
|      3 |  5254 | `			ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|      4 |  5255 | `			return PH7_VmThrowException(pCtx, "Error",` |
|      - |  5256 | `				"Typed property %z::$%z must not be accessed before initialization",` |
|      1 |  5257 | `				&pDecl->sDisp, &pAttr->sName);` |
|      - |  5258 | `		}` |
|      8 |  5259 | `		pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      8 |  5260 | `		if( pVal ){` |
|      8 |  5261 | `			ph7_result_value(pCtx, pVal);` |
|      5 |  5262 | `		}else{` |
|    ! 0 |  5263 | `			ph7_result_null(pCtx);` |
|      - |  5264 | `		}` |
|      - |  5265 | `	}` |
|      8 |  5266 | `	return PH7_OK;` |
|     12 |  5267 | `}` |
|      8 |  5268 | `static int vm_builtin_ReflectionClass_setStaticPropertyValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  5269 | `{` |
|     10 |  5270 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  5271 | `	ph7_class_attr *pAttr;` |
|      - |  5272 | `	ph7_value *pSlot;` |
|      - |  5273 | `	const char *zName;` |
|      - |  5274 | `	int nName;` |
|     10 |  5275 | `	if( pClass == 0 \|\| nArg < 2 ){` |
|    ! 0 |  5276 | `		return PH7_OK;` |
|      - |  5277 | `	}` |
|      - |  5278 | `	{` |
|     10 |  5279 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|     10 |  5280 | `		if( rc != SXRET_OK ){` |
|      5 |  5281 | `			return rc;` |
|      - |  5282 | `		}` |
|      - |  5283 | `	}` |
|      5 |  5284 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|      5 |  5285 | `	pAttr = ReflectStaticAttr(pCtx, pClass, zName, nName);` |
|      5 |  5286 | `	if( pAttr == 0 ){` |
|      4 |  5287 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  5288 | `			"Class %z does not have a property named %.*s", &pClass->sDisp, nName, zName);` |
|      - |  5289 | `	}` |
|      3 |  5290 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      3 |  5291 | `	if( pSlot == 0 ){` |
|    ! 0 |  5292 | `		return PH7_OK;` |
|      - |  5293 | `	}` |
|      - |  5294 | `	{` |
|      3 |  5295 | `		sxi32 rc = ReflectEnforceStore(pCtx, pAttr->nIdx, apArg[1]);` |
|      3 |  5296 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  5297 | `			return rc;` |
|      - |  5298 | `		}` |
|      - |  5299 | `	}` |
|      3 |  5300 | `	PH7_MemObjStore(apArg[1], pSlot);` |
|      3 |  5301 | `	return PH7_OK;` |
|      6 |  5302 | `}` |
|      4 |  5303 | `static int vm_builtin_ReflectionClass_getDefaultProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  5304 | `{` |
|      5 |  5305 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      5 |  5306 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  5307 | `	SySet aMembers;` |
|      - |  5308 | `	sxu32 n;` |
|      - |  5309 | `	int iPass;` |
|      2 |  5310 | `	SXUNUSED(nArg);` |
|      2 |  5311 | `	SXUNUSED(apArg);` |
|      5 |  5312 | `	if( pOut == 0 ){` |
|    ! 0 |  5313 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  5314 | `	}` |
|      5 |  5315 | `	if( pClass == 0 ){` |
|    ! 0 |  5316 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  5317 | `		return PH7_OK;` |
|      - |  5318 | `	}` |
|      5 |  5319 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|      5 |  5320 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|      - |  5321 | `	/* php reports the STATIC properties first, then the instance ones. */` |
|     13 |  5322 | `	for( iPass = 0 ; iPass < 2 ; iPass++ ){` |
|     57 |  5323 | `		for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     49 |  5324 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - |  5325 | `			int bStatic;` |
|      - |  5326 | `			ph7_value sValue;` |
|     49 |  5327 | `			if( pM->iKind != REFLECT_MEMBER_PROP ){` |
|     18 |  5328 | `				continue;` |
|      - |  5329 | `			}` |
|     45 |  5330 | `			bStatic = (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|     45 |  5331 | `			if( bStatic != (iPass == 0) ){` |
|     23 |  5332 | `				continue;` |
|      - |  5333 | `			}` |
|     22 |  5334 | `			if( (pM->pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|     16 |  5335 | `			 && SySetUsed(&pM->pAttr->aByteCode) < 1 ){` |
|      - |  5336 | `				/* A TYPED property with no initializer has no default at all —` |
|      - |  5337 | `				 * it is uninitialized, and php leaves it out. An UNTYPED one` |
|      - |  5338 | `				 * without an initializer defaults to null and is listed. */` |
|      5 |  5339 | `				continue;` |
|      - |  5340 | `			}` |
|     19 |  5341 | `			PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     19 |  5342 | `			if( SySetUsed(&pM->pAttr->aByteCode) > 0 ){` |
|      - |  5343 | `				/* Same evaluation path the VM uses for omitted call arguments */` |
|     15 |  5344 | `				VmLocalExec(pCtx->pVm, &pM->pAttr->aByteCode, &sValue, FALSE);` |
|      7 |  5345 | `			}` |
|     19 |  5346 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, &sValue);` |
|     19 |  5347 | `			PH7_MemObjRelease(&sValue);` |
|     10 |  5348 | `		}` |
|      5 |  5349 | `	}` |
|      5 |  5350 | `	SySetRelease(&aMembers);` |
|      5 |  5351 | `	ph7_result_value(pCtx, pOut);` |
|      5 |  5352 | `	return PH7_OK;` |
|      3 |  5353 | `}` |
|      - |  5354 | `/* ---- attributes, extension, export ---- */` |
|     82 |  5355 | `static int vm_builtin_ReflectionClass_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  5356 | `{` |
|     86 |  5357 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  5358 | `	const char *zName;` |
|      - |  5359 | `	int nName;` |
|     86 |  5360 | `	if( pClass == 0 ){` |
|    ! 0 |  5361 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  5362 | `		return PH7_OK;` |
|      - |  5363 | `	}` |
|     86 |  5364 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      - |  5365 | `	{` |
|      - |  5366 | `		ph7_value sTarget;` |
|      - |  5367 | `		int rc;` |
|     86 |  5368 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     86 |  5369 | `		ph7_value_string(&sTarget, zName, nName);` |
|      - |  5370 | `		/* 1 = Attribute::TARGET_CLASS */` |
|    127 |  5371 | `		rc = ReflectBuildAttrs(pCtx, &pClass->aAttrs, "class", &sTarget, 0, 0, 0, 1,` |
|     41 |  5372 | `			nArg, apArg);` |
|     86 |  5373 | `		PH7_MemObjRelease(&sTarget);` |
|     86 |  5374 | `		return rc;` |
|      - |  5375 | `	}` |
|     45 |  5376 | `}` |
|      - |  5377 | `/*` |
|      - |  5378 | ` * The ReflectionExtension for one extension id, which is what every reflector's` |
|      - |  5379 | ` * getExtension() answers for an INTERNAL target. A target the partition has no` |
|      - |  5380 | ` * row for (iExt < 0) belongs to no extension, which is php's null.` |
|      - |  5381 | ` */` |
|     22 |  5382 | `static int ReflectExtensionOf(ph7_context *pCtx, int iExt)` |
|      2 |  5383 | `{` |
|      - |  5384 | `	ph7_value sName;` |
|      - |  5385 | `	ph7_value *apCtor[1];` |
|      - |  5386 | `	ph7_class_instance *pExt;` |
|      - |  5387 | `	const char *zName;` |
|      - |  5388 | `	sxi32 rc;` |
|     24 |  5389 | `	if( iExt < 0 ){` |
|     10 |  5390 | `		ph7_result_null(pCtx);` |
|     10 |  5391 | `		return PH7_OK;` |
|      - |  5392 | `	}` |
|     15 |  5393 | `	zName = PH7_VmExtensionName(iExt);` |
|     15 |  5394 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|     15 |  5395 | `	ph7_value_string(&sName, zName, -1);` |
|     15 |  5396 | `	apCtor[0] = &sName;` |
|     15 |  5397 | `	pExt = ReflectConstruct(pCtx, "ReflectionExtension", 1, apCtor, &rc);` |
|     15 |  5398 | `	PH7_MemObjRelease(&sName);` |
|     15 |  5399 | `	if( pExt == 0 ){` |
|    ! 0 |  5400 | `		if( rc != PH7_OK ){` |
|    ! 0 |  5401 | `			return rc;` |
|      - |  5402 | `		}` |
|    ! 0 |  5403 | `		ph7_result_null(pCtx);` |
|    ! 0 |  5404 | `		return PH7_OK;` |
|      - |  5405 | `	}` |
|     15 |  5406 | `	return ReflectResultObject(pCtx, pExt);` |
|     13 |  5407 | `}` |
|      - |  5408 | `/*` |
|      - |  5409 | ` * The extension a reflected CLASS belongs to, or -1 for a userland one. A class` |
|      - |  5410 | ` * php has no row for keeps php's answer for an internal name it cannot place --` |
|      - |  5411 | ` * Core, the engine's own.` |
|      - |  5412 | ` */` |
|   4286 |  5413 | `static int ReflectClassExtId(ph7_class *pClass)` |
|      4 |  5414 | `{` |
|   4290 |  5415 | `	if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_INTERNAL) == 0 ){` |
|     35 |  5416 | `		return -1;` |
|      - |  5417 | `	}` |
|   6382 |  5418 | `	return PH7_VmExtOfClass(SyStringData(&pClass->sName),` |
|   4252 |  5419 | `		(int)SyStringLength(&pClass->sName));` |
|   2147 |  5420 | `}` |
|     32 |  5421 | `static int vm_builtin_ReflectionClass_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  5422 | `{` |
|     35 |  5423 | `	int iExt = ReflectClassExtId(ReflectClassOf(pCtx));` |
|     16 |  5424 | `	SXUNUSED(nArg);` |
|     16 |  5425 | `	SXUNUSED(apArg);` |
|     35 |  5426 | `	if( iExt < 0 ){` |
|      3 |  5427 | `		ph7_result_bool(pCtx, 0);` |
|      2 |  5428 | `	}else{` |
|     33 |  5429 | `		ph7_result_string(pCtx, PH7_VmExtensionName(iExt), -1);` |
|      - |  5430 | `	}` |
|     35 |  5431 | `	return PH7_OK;` |
|      3 |  5432 | `}` |
|      - |  5433 | `/* php's export format (chunk 9) — defined at the END of this file, where the` |
|      - |  5434 | ` * member walk, the function reference and the parameter description it reads` |
|      - |  5435 | ` * are all in scope. */` |
|      - |  5436 | `static int ReflectExportClassSelf(ph7_context *pCtx);` |
|      - |  5437 | `static int ReflectExportFuncSelf(ph7_context *pCtx);` |
|      - |  5438 | `static int ReflectExportParamSelf(ph7_context *pCtx);` |
|      - |  5439 | `static int ReflectExportPropSelf(ph7_context *pCtx);` |
|      - |  5440 | `static int ReflectExportConstSelf(ph7_context *pCtx);` |
|      - |  5441 | `/*` |
|      - |  5442 | ` * __toString(): php's export format, still chunk 9 — a PHP function written` |
|      - |  5443 | `` * against the PUBLIC reflection API of its target, so it is called with `$this`.`` |
|      - |  5444 | ` * bIndentArg adds the export family's second "" indent argument.` |
|      - |  5445 | ` */` |
|      4 |  5446 | `static int vm_builtin_ReflectionClass_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  5447 | `{` |
|      2 |  5448 | `	SXUNUSED(nArg);` |
|      2 |  5449 | `	SXUNUSED(apArg);` |
|      5 |  5450 | `	return ReflectExtensionOf(pCtx, ReflectClassExtId(ReflectClassOf(pCtx)));` |
|      1 |  5451 | `}` |
|     54 |  5452 | `static int vm_builtin_ReflectionClass_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  5453 | `{` |
|     27 |  5454 | `	SXUNUSED(nArg);` |
|     27 |  5455 | `	SXUNUSED(apArg);` |
|     56 |  5456 | `	return ReflectExportClassSelf(pCtx);` |
|      2 |  5457 | `}` |
|      - |  5458 | `/* ---- lazy objects: PHL has none (recorded) ---- */` |
|    ! 0 |  5459 | `static int ReflectNoLazy(ph7_context *pCtx, const char *zWho)` |
|    ! 0 |  5460 | `{` |
|    ! 0 |  5461 | `	return PH7_VmThrowException(pCtx, "Error",` |
|    ! 0 |  5462 | `		"%s is not supported by PHL (no lazy objects)", zWho);` |
|    ! 0 |  5463 | `}` |
|      - |  5464 | `#define REFLECT_NO_LAZY(NAME,TEXT) \` |
|      - |  5465 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  5466 | `	{ \` |
|      - |  5467 | `		SXUNUSED(nArg); \` |
|      - |  5468 | `		SXUNUSED(apArg); \` |
|      - |  5469 | `		return ReflectNoLazy(pCtx, TEXT); \` |
|      - |  5470 | `	}` |
|    ! 0 |  5471 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_newLazyGhost,"ReflectionClass::newLazyGhost()")` |
|    ! 0 |  5472 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_newLazyProxy,"ReflectionClass::newLazyProxy()")` |
|    ! 0 |  5473 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_resetAsLazyGhost,"ReflectionClass::resetAsLazyGhost()")` |
|    ! 0 |  5474 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_resetAsLazyProxy,"ReflectionClass::resetAsLazyProxy()")` |
|      - |  5475 | `/* The four QUERIES answer what is true of a VM with no lazy objects, rather` |
|      - |  5476 | ` * than refusing: an object here is always initialized and never has one. */` |
|    ! 0 |  5477 | `static int vm_builtin_ReflectionClass_getLazyInitializer(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  5478 | `{` |
|    ! 0 |  5479 | `	SXUNUSED(nArg);` |
|    ! 0 |  5480 | `	SXUNUSED(apArg);` |
|    ! 0 |  5481 | `	ph7_result_null(pCtx);` |
|    ! 0 |  5482 | `	return PH7_OK;` |
|    ! 0 |  5483 | `}` |
|    ! 0 |  5484 | `static int vm_builtin_ReflectionClass_passThroughObject(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  5485 | `{` |
|    ! 0 |  5486 | `	if( nArg > 0 ){` |
|    ! 0 |  5487 | `		ph7_result_value(pCtx, apArg[0]);` |
|    ! 0 |  5488 | `	}else{` |
|    ! 0 |  5489 | `		ph7_result_null(pCtx);` |
|      - |  5490 | `	}` |
|    ! 0 |  5491 | `	return PH7_OK;` |
|    ! 0 |  5492 | `}` |
|    ! 0 |  5493 | `static int vm_builtin_ReflectionClass_isUninitializedLazyObject(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  5494 | `{` |
|    ! 0 |  5495 | `	SXUNUSED(nArg);` |
|    ! 0 |  5496 | `	SXUNUSED(apArg);` |
|    ! 0 |  5497 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 |  5498 | `	return PH7_OK;` |
|    ! 0 |  5499 | `}` |
|      - |  5500 | `/*` |
|      - |  5501 | ` * Reflection::getModifierNames(int $modifiers)` |
|      - |  5502 | ` *` |
|      - |  5503 | ` * php's own order and its own SWITCH: the visibility names come out of one` |
|      - |  5504 | ` * three-way choice and the set-visibility names out of another, so a mask with` |
|      - |  5505 | ` * two visibility bits set names NEITHER (which is what php answers).` |
|      - |  5506 | ` */` |
|     96 |  5507 | `static int vm_builtin_Reflection_getModifierNames(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  5508 | `{` |
|     98 |  5509 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     98 |  5510 | `	sxi64 iMods = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|      - |  5511 | `	const char *azName[8];` |
|     98 |  5512 | `	int nName = 0, n;` |
|     98 |  5513 | `	if( pOut == 0 ){` |
|    ! 0 |  5514 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  5515 | `	}` |
|     98 |  5516 | `	if( iMods & 64 ){ azName[nName++] = "abstract"; }` |
|     98 |  5517 | `	if( iMods & 32 ){ azName[nName++] = "final"; }` |
|     98 |  5518 | `	if( iMods & 512 ){ azName[nName++] = "virtual"; }` |
|     98 |  5519 | `	switch( iMods & (1\|2\|4) ){` |
|     56 |  5520 | `	case 1: azName[nName++] = "public"; break;` |
|     21 |  5521 | `	case 2: azName[nName++] = "protected"; break;` |
|     15 |  5522 | `	case 4: azName[nName++] = "private"; break;` |
|      8 |  5523 | `	default: break;` |
|      - |  5524 | `	}` |
|     98 |  5525 | `	switch( iMods & (2048\|4096) ){` |
|     18 |  5526 | `	case 2048: azName[nName++] = "protected(set)"; break;` |
|      9 |  5527 | `	case 4096: azName[nName++] = "private(set)"; break;` |
|     72 |  5528 | `	default: break;` |
|      - |  5529 | `	}` |
|     98 |  5530 | `	if( iMods & 16 ){ azName[nName++] = "static"; }` |
|     98 |  5531 | `	if( iMods & 128 ){ azName[nName++] = "readonly"; }` |
|    266 |  5532 | `	for( n = 0 ; n < nName ; n++ ){` |
|    170 |  5533 | `		ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|    170 |  5534 | `		if( pName == 0 ){ break; }` |
|    170 |  5535 | `		ph7_value_string(pName, azName[n], -1);` |
|    170 |  5536 | `		ph7_array_add_elem(pOut, 0, pName);` |
|     86 |  5537 | `	}` |
|     98 |  5538 | `	ph7_result_value(pCtx, pOut);` |
|     98 |  5539 | `	return PH7_OK;` |
|     50 |  5540 | `}` |
|      - |  5541 | `/*` |
|      - |  5542 | ` * Declare chunk 1. Called from PH7_VmInstallReflection() where it used to be` |
|      - |  5543 | `` * compiled — before chunks 2 and 3, which name `Reflector` in their own`` |
|      - |  5544 | `` * `implements` clauses.`` |
|      - |  5545 | ` *` |
|      - |  5546 | ` * The method table is in php's own DECLARATION order, which is the order` |
|      - |  5547 | ` * ReflectionClass::getMethods() reports for these classes themselves.` |
|      - |  5548 | ` */` |
|   7925 |  5549 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionClass(ph7_vm *pVm)` |
|      5 |  5550 | `{` |
|      - |  5551 | `	static const PH7_NativePropDef aClassProp[] = {` |
|      - |  5552 | `		{ "name",  PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  5553 | `		/* PHL-only: the instance a ReflectionObject was built over. php keeps it` |
|      - |  5554 | `		 * out of sight; PHL has no hidden-slot bit yet (recorded). */` |
|      - |  5555 | `		{ RC_OBJ,  PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  5556 | `	};` |
|      - |  5557 | `	static const PH7_NativeMethodDef aClassMethod[] = {` |
|      - |  5558 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionClass_clone },` |
|      - |  5559 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $objectOrClass", "",` |
|      - |  5560 | `		  vm_builtin_ReflectionClass_construct },` |
|      - |  5561 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionClass_toString },` |
|      - |  5562 | `		{ "getName",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getName },` |
|      - |  5563 | `		{ "isInternal",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInternal },` |
|      - |  5564 | `		{ "isUserDefined", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isUserDefined },` |
|      - |  5565 | `		{ "isAnonymous",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isAnonymous },` |
|      - |  5566 | `		{ "isInstantiable",PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInstantiable },` |
|      - |  5567 | `		{ "isCloneable",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isCloneable },` |
|      - |  5568 | `		{ "getFileName",   PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getFileName },` |
|      - |  5569 | `		{ "getStartLine",  PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionClass_getStartLine },` |
|      - |  5570 | `		{ "getEndLine",    PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionClass_getEndLine },` |
|      - |  5571 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getDocComment },` |
|      - |  5572 | `		{ "getConstructor",PH7_MOD_PUBLIC, "", "@?ReflectionMethod", vm_builtin_ReflectionClass_getConstructor },` |
|      - |  5573 | `		{ "hasMethod",     PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasMethod },` |
|      - |  5574 | `		{ "getMethod",     PH7_MOD_PUBLIC, "string $name", "@ReflectionMethod", vm_builtin_ReflectionClass_getMethod },` |
|      - |  5575 | `		{ "getMethods",    PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - |  5576 | `		  vm_builtin_ReflectionClass_getMethods },` |
|      - |  5577 | `		{ "hasProperty",   PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasProperty },` |
|      - |  5578 | `		{ "getProperty",   PH7_MOD_PUBLIC, "string $name", "@ReflectionProperty", vm_builtin_ReflectionClass_getProperty },` |
|      - |  5579 | `		{ "getProperties", PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - |  5580 | `		  vm_builtin_ReflectionClass_getProperties },` |
|      - |  5581 | `		{ "hasConstant",   PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasConstant },` |
|      - |  5582 | `		{ "getConstants",  PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - |  5583 | `		  vm_builtin_ReflectionClass_getConstants },` |
|      - |  5584 | `		{ "getReflectionConstants", PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - |  5585 | `		  vm_builtin_ReflectionClass_getReflectionConstants },` |
|      - |  5586 | `		{ "getConstant",   PH7_MOD_PUBLIC, "string $name", "@mixed", vm_builtin_ReflectionClass_getConstant },` |
|      - |  5587 | `		{ "getReflectionConstant", PH7_MOD_PUBLIC, "string $name", "@ReflectionClassConstant\|false",` |
|      - |  5588 | `		  vm_builtin_ReflectionClass_getReflectionConstant },` |
|      - |  5589 | `		{ "getInterfaces",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getInterfaces },` |
|      - |  5590 | `		{ "getInterfaceNames", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getInterfaceNames },` |
|      - |  5591 | `		{ "isInterface",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInterface },` |
|      - |  5592 | `		{ "getTraits",         PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraits },` |
|      - |  5593 | `		{ "getTraitNames",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraitNames },` |
|      - |  5594 | `		{ "getTraitAliases",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraitAliases },` |
|      - |  5595 | `		{ "isTrait",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isTrait },` |
|      - |  5596 | `		{ "isEnum",            PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClass_isEnum },` |
|      - |  5597 | `		{ "isAbstract",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isAbstract },` |
|      - |  5598 | `		{ "isFinal",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isFinal },` |
|      - |  5599 | `		{ "isReadOnly",        PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClass_isReadOnly },` |
|      - |  5600 | `		{ "getModifiers",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionClass_getModifiers },` |
|      - |  5601 | `		{ "isInstance",        PH7_MOD_PUBLIC, "object $object", "@bool",` |
|      - |  5602 | `		  vm_builtin_ReflectionClass_isInstance },` |
|      - |  5603 | `		{ "newInstance",       PH7_MOD_PUBLIC, "mixed ...$args", "@object",` |
|      - |  5604 | `		  vm_builtin_ReflectionClass_newInstance },` |
|      - |  5605 | `		{ "newInstanceWithoutConstructor", PH7_MOD_PUBLIC, "", "@object",` |
|      - |  5606 | `		  vm_builtin_ReflectionClass_newInstanceWithoutConstructor },` |
|      - |  5607 | `		{ "newInstanceArgs",   PH7_MOD_PUBLIC, "array $args = []", "@?object",` |
|      - |  5608 | `		  vm_builtin_ReflectionClass_newInstanceArgs },` |
|      - |  5609 | `		{ "newLazyGhost",      PH7_MOD_PUBLIC, "callable $initializer, int $options = 0", "object",` |
|      - |  5610 | `		  vm_builtin_ReflectionClass_newLazyGhost },` |
|      - |  5611 | `		{ "newLazyProxy",      PH7_MOD_PUBLIC, "callable $factory, int $options = 0", "object",` |
|      - |  5612 | `		  vm_builtin_ReflectionClass_newLazyProxy },` |
|      - |  5613 | `		{ "resetAsLazyGhost",  PH7_MOD_PUBLIC,` |
|      - |  5614 | `		  "object $object, callable $initializer, int $options = 0", "void",` |
|      - |  5615 | `		  vm_builtin_ReflectionClass_resetAsLazyGhost },` |
|      - |  5616 | `		{ "resetAsLazyProxy",  PH7_MOD_PUBLIC,` |
|      - |  5617 | `		  "object $object, callable $factory, int $options = 0", "void",` |
|      - |  5618 | `		  vm_builtin_ReflectionClass_resetAsLazyProxy },` |
|      - |  5619 | `		{ "initializeLazyObject", PH7_MOD_PUBLIC, "object $object", "object",` |
|      - |  5620 | `		  vm_builtin_ReflectionClass_passThroughObject },` |
|      - |  5621 | `		{ "isUninitializedLazyObject", PH7_MOD_PUBLIC, "object $object", "bool",` |
|      - |  5622 | `		  vm_builtin_ReflectionClass_isUninitializedLazyObject },` |
|      - |  5623 | `		{ "markLazyObjectAsInitialized", PH7_MOD_PUBLIC, "object $object", "object",` |
|      - |  5624 | `		  vm_builtin_ReflectionClass_passThroughObject },` |
|      - |  5625 | `		{ "getLazyInitializer", PH7_MOD_PUBLIC, "object $object", "?callable",` |
|      - |  5626 | `		  vm_builtin_ReflectionClass_getLazyInitializer },` |
|      - |  5627 | `		{ "getParentClass",    PH7_MOD_PUBLIC, "", "@ReflectionClass\|false", vm_builtin_ReflectionClass_getParentClass },` |
|      - |  5628 | `		{ "isSubclassOf",      PH7_MOD_PUBLIC, "ReflectionClass\|string $class", "@bool",` |
|      - |  5629 | `		  vm_builtin_ReflectionClass_isSubclassOf },` |
|      - |  5630 | `		{ "getStaticProperties", PH7_MOD_PUBLIC, "", "@array",` |
|      - |  5631 | `		  vm_builtin_ReflectionClass_getStaticProperties },` |
|      - |  5632 | ``		/* `mixed $default = ?` is the table's "optional, no default VALUE"`` |
|      - |  5633 | `		 * marker — php's own shape here: isOptional() true,` |
|      - |  5634 | `		 * isDefaultValueAvailable() false. */` |
|      - |  5635 | `		{ "getStaticPropertyValue", PH7_MOD_PUBLIC, "string $name, mixed $default = ?", "@mixed",` |
|      - |  5636 | `		  vm_builtin_ReflectionClass_getStaticPropertyValue },` |
|      - |  5637 | `		{ "setStaticPropertyValue", PH7_MOD_PUBLIC, "string $name, mixed $value", "@void",` |
|      - |  5638 | `		  vm_builtin_ReflectionClass_setStaticPropertyValue },` |
|      - |  5639 | `		{ "getDefaultProperties", PH7_MOD_PUBLIC, "", "@array",` |
|      - |  5640 | `		  vm_builtin_ReflectionClass_getDefaultProperties },` |
|      - |  5641 | `		{ "isIterable",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isIterable },` |
|      - |  5642 | `		{ "isIterateable",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isIterable },` |
|      - |  5643 | `		{ "implementsInterface", PH7_MOD_PUBLIC, "ReflectionClass\|string $interface", "@bool",` |
|      - |  5644 | `		  vm_builtin_ReflectionClass_implementsInterface },` |
|      - |  5645 | `		{ "getExtension",      PH7_MOD_PUBLIC, "", "@?ReflectionExtension", vm_builtin_ReflectionClass_getExtension },` |
|      - |  5646 | `		{ "getExtensionName",  PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getExtensionName },` |
|      - |  5647 | `		{ "inNamespace",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_inNamespace },` |
|      - |  5648 | `		{ "getNamespaceName",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getNamespaceName },` |
|      - |  5649 | `		{ "getShortName",      PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getShortName },` |
|      - |  5650 | `		{ "getAttributes",     PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - |  5651 | `		  vm_builtin_ReflectionClass_getAttributes },` |
|      - |  5652 | `	};` |
|      - |  5653 | `	static const PH7_NativeConstDef aClassConst[] = {` |
|      - |  5654 | `		{ "IS_IMPLICIT_ABSTRACT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,    0, 0.0 },` |
|      - |  5655 | `		{ "IS_EXPLICIT_ABSTRACT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64,    0, 0.0 },` |
|      - |  5656 | `		{ "IS_FINAL",             PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32,    0, 0.0 },` |
|      - |  5657 | `		{ "IS_READONLY",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 65536, 0, 0.0 },` |
|      - |  5658 | `		{ "SKIP_INITIALIZATION_ON_SERIALIZE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 8, 0, 0.0 },` |
|      - |  5659 | `		{ "SKIP_DESTRUCTOR",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,    0, 0.0 },` |
|      - |  5660 | `	};` |
|      - |  5661 | `	static const PH7_NativeMethodDef aObjectMethod[] = {` |
|      - |  5662 | `		{ "__construct", PH7_MOD_PUBLIC, "object $object", "",` |
|      - |  5663 | `		  vm_builtin_ReflectionObject_construct },` |
|      - |  5664 | `	};` |
|      - |  5665 | `	static const PH7_NativeMethodDef aReflectionMethod[] = {` |
|      - |  5666 | `		{ "getModifierNames", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "int $modifiers", "@array",` |
|      - |  5667 | `		  vm_builtin_Reflection_getModifierNames },` |
|      - |  5668 | `	};` |
|      - |  5669 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  5670 | `		{ "Reflector", "Stringable", 0, PH7_CLASS_INTERFACE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  5671 | `		{ "ReflectionException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  5672 | `		{ "Reflection", 0, 0, 0,` |
|      - |  5673 | `		  aReflectionMethod, SX_ARRAYSIZE(aReflectionMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  5674 | ``		/* Uncloneable and unserializable in php too: `clone` is an Error and`` |
|      - |  5675 | `		 * serialize() a catchable Exception naming the class. */` |
|      - |  5676 | `		{ "ReflectionClass", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  5677 | `		  aClassMethod, SX_ARRAYSIZE(aClassMethod),` |
|      - |  5678 | `		  aClassConst, SX_ARRAYSIZE(aClassConst),` |
|      - |  5679 | `		  aClassProp, SX_ARRAYSIZE(aClassProp), 0, 0, 0 },` |
|      - |  5680 | `		{ "ReflectionObject", "ReflectionClass", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  5681 | `		  aObjectMethod, SX_ARRAYSIZE(aObjectMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  5682 | `	};` |
|   7930 |  5683 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  5684 | `}` |
|      - |  5685 | `/*` |
|      - |  5686 | ` * ---------------------------------------------------------------------------` |
|      - |  5687 | ` * ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,` |
|      - |  5688 | ` * ReflectionParameter.` |
|      - |  5689 | ` *` |
|      - |  5690 | ` * Chunk 2. Its accessors funnelled through __reflect_func_info(), a descriptor` |
|      - |  5691 | ` * ARRAY built per call from either a compiled ph7_vm_func or a declared` |
|      - |  5692 | ` * SIGNATURE STRING; a native method reads whichever of the two the target` |
|      - |  5693 | ` * actually has, through the one uniform description both already agreed on` |
|      - |  5694 | ` * (ReflectParamDesc / ReflectSigDescribe).` |
|      - |  5695 | ` *` |
|      - |  5696 | ` * Five thunks retire with the chunk: __reflect_func_info, __reflect_invoke,` |
|      - |  5697 | ` * __reflect_closure, __reflect_param_default and __reflect_param_defconst.` |
|      - |  5698 | ` * ---------------------------------------------------------------------------` |
|      - |  5699 | ` */` |
|      - |  5700 | `#define RF_CL "__cl"     /* ReflectionFunctionAbstract: the reflected Closure  */` |
|      - |  5701 | `/*` |
|      - |  5702 | ` * ReflectionMethod: the class the reflector was BUILT FOR, which is not the` |
|      - |  5703 | `` * one `$class` reports. php keeps both — `intern->ce` is the class the`` |
|      - |  5704 | ` * constructor (or the ReflectionClass that handed the method out) named, while` |
|      - |  5705 | `` * the public `$class` is the method's DECLARING class — and its export tags are`` |
|      - |  5706 | ` * relative to the first: a method whose declaring class is not the reflector's` |
|      - |  5707 | `` * own class prints `inherits <declaring>`. PHL had only the declaring one, so`` |
|      - |  5708 | ` * every directly-built reflector lost the tag.` |
|      - |  5709 | ` */` |
|      - |  5710 | `#define RM_CE "__ce"` |
|      - |  5711 | `#define RP_T  "__t"      /* ReflectionParameter: the function/class it belongs to */` |
|      - |  5712 | `#define RP_M  "__m"      /* ReflectionParameter: the method name, or null      */` |
|      - |  5713 | `#define RP_P  "__p"      /* ReflectionParameter: the position                  */` |
|      - |  5714 |  |
|      - |  5715 | `/* Everything a reflected function IS, resolved once per call. */` |
|      - |  5716 | `typedef struct ReflectFuncRef ReflectFuncRef;` |
|      - |  5717 | `struct ReflectFuncRef` |
|      - |  5718 | `{` |
|      - |  5719 | `	ph7_vm *pVm;                  /* the VM, for the declared-type text rewrite */` |
|      - |  5720 | `	ph7_vm_func *pFunc;           /* compiled body (NULL for a pure C builtin) */` |
|      - |  5721 | `	ph7_user_func *pHost;         /* C builtin (NULL otherwise) */` |
|      - |  5722 | `	ph7_class *pClass;            /* class a METHOD was reached through */` |
|      - |  5723 | `	ph7_class_method *pMeth;      /* the method */` |
|      - |  5724 | `	ph7_class_instance *pClosure; /* the Closure being reflected, if any */` |
|      - |  5725 | `	const char *zSig;             /* declared parameter signature, or NULL */` |
|      - |  5726 | `	const char *zRet;             /* declared return type, or NULL */` |
|      - |  5727 | ``	int bFabricated;              /* `Closure::__invoke`: pFunc is the CLOSURE's, borrowed for its`` |
|      - |  5728 | `	                               * parameter list alone. Everything that describes where the` |
|      - |  5729 | `	                               * function came FROM -- internal or user, file, lines, module --` |
|      - |  5730 | `	                               * belongs to the fabricated method instead, which php builds as` |
|      - |  5731 | `	                               * an internal one with no module at all. */` |
|      - |  5732 | `};` |
|      - |  5733 | `/*` |
|      - |  5734 | ` * Resolve a callable into the reference, and work out WHICH of the two` |
|      - |  5735 | ` * parameter sources describes it: a declared signature string (a C builtin, a` |
|      - |  5736 | ` * native method, or an embedded-PHP builtin declared argless over` |
|      - |  5737 | ` * func_get_args()) wins over the compiled argument list, exactly as` |
|      - |  5738 | ` * ReflectSigFixup made it win in the descriptor.` |
|      - |  5739 | ` */` |
|   7988 |  5740 | `static int ReflectFuncFill(ph7_vm *pVm, ph7_value *pTarget, ph7_value *pMethodArg,` |
|      - |  5741 | `	ReflectFuncRef *pOut)` |
|      5 |  5742 | `{` |
|   7993 |  5743 | `	SyZero(pOut, sizeof(*pOut));` |
|   7993 |  5744 | `	pOut->pVm = pVm;` |
|  11983 |  5745 | `	pOut->pFunc = ReflectResolveCallable(pVm, pTarget, pMethodArg,` |
|   3990 |  5746 | `		&pOut->pClass, &pOut->pMeth, &pOut->pHost, &pOut->pClosure);` |
|   7993 |  5747 | `	if( pOut->pFunc == 0 && pOut->pHost == 0 ){` |
|     45 |  5748 | `		return 0;` |
|      - |  5749 | `	}` |
|   7951 |  5750 | `	if( pOut->pFunc == 0 ){` |
|   3233 |  5751 | `		pOut->zSig = pOut->pHost->zSig;` |
|   3233 |  5752 | `		pOut->zRet = pOut->pHost->zRet;` |
|   3233 |  5753 | `		return 1;` |
|      - |  5754 | `	}` |
|   4723 |  5755 | `	if( pOut->pMeth && (pOut->pMeth->iFlags & PH7_CLASS_ATTR_FABRICATED) ){` |
|      - |  5756 | `		/* The split above: with a closure, pFunc is the CLOSURE's, so nothing of the` |
|      - |  5757 | `		 * method's own (empty) declaration may describe it -- and where it CAME from` |
|      - |  5758 | `		 * is still the method's, which bFabricated is what says. WITHOUT one (the` |
|      - |  5759 | `		 * class-level form) there is nothing to describe at all, which php prints as` |
|      - |  5760 | `		 * a block with no parameter section rather than an empty one. */` |
|    105 |  5761 | `		pOut->bFabricated = 1;` |
|    105 |  5762 | `		return 1;` |
|      - |  5763 | `	}` |
|   4619 |  5764 | `	if( (pOut->pFunc->iFlags & VM_FUNC_NATIVE) && pOut->pFunc->pNative ){` |
|   2943 |  5765 | `		pOut->zSig = pOut->pFunc->pNative->zSig;` |
|   2943 |  5766 | `		if( SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|   2943 |  5767 | `			pOut->zRet = pOut->pFunc->pNative->zRet;` |
|   1469 |  5768 | `		}` |
|   3150 |  5769 | `	}else if( (pOut->pFunc->iFlags & VM_FUNC_INTERNAL)` |
|    966 |  5770 | `	 && SySetUsed(&pOut->pFunc->aArgs) == 0 && pOut->pMeth == 0 ){` |
|    ! 0 |  5771 | `		const char *zRet = 0;` |
|    ! 0 |  5772 | `		pOut->zSig = PH7_VmBuiltinSigLookup(SyStringData(&pOut->pFunc->sName),` |
|    ! 0 |  5773 | `			SyStringLength(&pOut->pFunc->sName), &zRet);` |
|    ! 0 |  5774 | `		if( zRet && SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|    ! 0 |  5775 | `			pOut->zRet = zRet;` |
|    ! 0 |  5776 | `		}` |
|    ! 0 |  5777 | `	}` |
|   4619 |  5778 | `	if( pOut->zSig && pOut->zSig[0] == '\0' ){` |
|      - |  5779 | `		/* A declared-EMPTY signature really is "no parameters", not "undescribed". */` |
|    852 |  5780 | `		return 1;` |
|      - |  5781 | `	}` |
|   3771 |  5782 | `	return 1;` |
|   3995 |  5783 | `}` |
|      - |  5784 | `/* Is this receiver a ReflectionMethod (rather than a ReflectionFunction)? */` |
|   4090 |  5785 | `static int ReflectIsMethodReflector(ph7_context *pCtx, ph7_class_instance *pThis)` |
|      5 |  5786 | `{` |
|      - |  5787 | `	ph7_class *pRM;` |
|   4095 |  5788 | `	if( pThis == 0 ){` |
|    ! 0 |  5789 | `		return 0;` |
|      - |  5790 | `	}` |
|   4095 |  5791 | `	pRM = PH7_VmExtractClass(pCtx->pVm, "ReflectionMethod", sizeof("ReflectionMethod")-1, FALSE, 0);` |
|   4095 |  5792 | `	return pRM != 0 && PH7_VmInstanceOf(pThis->pClass, pRM);` |
|   2048 |  5793 | `}` |
|      - |  5794 | `/*` |
|      - |  5795 | `` * The function `$this` reflects. A ReflectionFunction built over a Closure keeps`` |
|      - |  5796 | `` * the object in `__cl` and resolves through it (its `$__fn` is a lambda name no`` |
|      - |  5797 | ` * function table holds); a ReflectionMethod resolves ($this->class, $this->name).` |
|      - |  5798 | ` */` |
|   3376 |  5799 | `static int ReflectFuncOfThis(ph7_context *pCtx, ReflectFuncRef *pOut)` |
|      5 |  5800 | `{` |
|   3381 |  5801 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  5802 | `	ph7_class_instance *pClo;` |
|      - |  5803 | `	ph7_value sTarget, sMethod;` |
|      - |  5804 | `	const char *zName, *zClass;` |
|      - |  5805 | `	int nName, nClass, rc, bMethod;` |
|   3381 |  5806 | `	SyZero(pOut, sizeof(*pOut));` |
|   3381 |  5807 | `	if( pThis == 0 ){` |
|    ! 0 |  5808 | `		return 0;` |
|      - |  5809 | `	}` |
|   3381 |  5810 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|   3381 |  5811 | `	PH7_MemObjInit(pCtx->pVm, &sMethod);` |
|   3381 |  5812 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|   3381 |  5813 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|   3381 |  5814 | `	bMethod = ReflectIsMethodReflector(pCtx, pThis);` |
|   3381 |  5815 | `	if( pClo && bMethod ){` |
|      - |  5816 | ``		/* The fabricated `Closure::__invoke`: name it over the OBJECT, which is what`` |
|      - |  5817 | `		 * makes the resolver hand back the method's identity and the closure's body` |
|      - |  5818 | `		 * together (see ReflectResolveCallable). */` |
|     39 |  5819 | `		sTarget.x.pOther = pClo;` |
|     39 |  5820 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|     39 |  5821 | `		ph7_value_string(&sMethod, zName, nName);` |
|   3362 |  5822 | `	}else if( pClo ){` |
|    141 |  5823 | `		sTarget.x.pOther = pClo;` |
|    141 |  5824 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|   3274 |  5825 | `	}else if( bMethod ){` |
|   2085 |  5826 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|   2085 |  5827 | `		ph7_value_string(&sTarget, zClass, nClass);` |
|   2085 |  5828 | `		ph7_value_string(&sMethod, zName, nName);` |
|   1045 |  5829 | `	}else{` |
|   1125 |  5830 | `		ph7_value_string(&sTarget, zName, nName);` |
|      - |  5831 | `	}` |
|   3381 |  5832 | `	rc = ReflectFuncFill(pCtx->pVm, &sTarget, (sMethod.iFlags & MEMOBJ_STRING) ? &sMethod : 0, pOut);` |
|      - |  5833 | `	/* sTarget only ever BORROWS the closure; releasing it would unref twice. */` |
|   3381 |  5834 | `	if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|   3205 |  5835 | `		PH7_MemObjRelease(&sTarget);` |
|   1598 |  5836 | `	}` |
|   3381 |  5837 | `	PH7_MemObjRelease(&sMethod);` |
|   3381 |  5838 | `	return rc;` |
|   1691 |  5839 | `}` |
|      - |  5840 | `/*` |
|      - |  5841 | `` * The class `$this` was BUILT FOR (php's `intern->ce`), or NULL — a`` |
|      - |  5842 | ` * ReflectionFunction has none, and so does a reflector whose class went away.` |
|      - |  5843 | ` */` |
|    658 |  5844 | `static ph7_class * ReflectOwnerOfThis(ph7_context *pCtx)` |
|      4 |  5845 | `{` |
|    662 |  5846 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    662 |  5847 | `	const char *zName = 0;` |
|    662 |  5848 | `	int nName = 0;` |
|    662 |  5849 | `	if( pThis == 0 ){` |
|    ! 0 |  5850 | `		return 0;` |
|      - |  5851 | `	}` |
|    662 |  5852 | `	PH7_NativeAttrStr(pThis, RM_CE, &zName, &nName);` |
|    662 |  5853 | `	if( zName == 0 \|\| nName < 1 ){` |
|    268 |  5854 | `		return 0;` |
|      - |  5855 | `	}` |
|    397 |  5856 | `	return PH7_VmExtractClass(pCtx->pVm, zName, (sxu32)nName, FALSE, 0);` |
|    333 |  5857 | `}` |
|      - |  5858 | `/* How many parameters the target declares. */` |
|   6762 |  5859 | `static int ReflectParamCount(const ReflectFuncRef *pRef)` |
|      5 |  5860 | `{` |
|   6767 |  5861 | `	if( pRef->zSig ){` |
|   6111 |  5862 | `		return ReflectSigPart(pRef->zSig, (int)SyStrlen(pRef->zSig), -1, 0, 0);` |
|      - |  5863 | `	}` |
|    660 |  5864 | `	return pRef->pFunc ? (int)SySetUsed(&pRef->pFunc->aArgs) : 0;` |
|   3386 |  5865 | `}` |
|      - |  5866 | `/*` |
|      - |  5867 | `` * The class a `self`/`parent` in this function's declared types resolves to for`` |
|      - |  5868 | ` * DISPLAY: php substitutes the declaring class into the stored text at compile` |
|      - |  5869 | ` * time, and has nothing to substitute for a TRAIT method (whose text keeps the` |
|      - |  5870 | ` * keyword however many classes composed it) or for a plain function.` |
|      - |  5871 | `` * `static` and `iterable` are printed as written -- PH7_HINT_TEXT_* off.`` |
|      - |  5872 | ` */` |
|   1020 |  5873 | `static ph7_class * ReflectDeclScope(const ReflectFuncRef *pRef)` |
|      5 |  5874 | `{` |
|   1025 |  5875 | `	if( pRef->pFunc == 0 \|\| (pRef->pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|    708 |  5876 | `		return 0; /* pUserData is a class only for a METHOD */` |
|      - |  5877 | `	}` |
|    321 |  5878 | `	return VmHintScopeDeclared((ph7_class *)pRef->pFunc->pUserData);` |
|    515 |  5879 | `}` |
|      - |  5880 | `/*` |
|      - |  5881 | ` * A MEMBER's declared type as php prints it: the same rewrite ReflectDeclScope` |
|      - |  5882 | ` * describes, keyed on the class that declared the member rather than the one it` |
|      - |  5883 | `` * was reached through -- so a trait's `?self` stays `?self`.`` |
|      - |  5884 | ` */` |
|    442 |  5885 | `static const char * ReflectMemberTypeText(ph7_vm *pVm,ph7_class_attr *pAttr,char *zBuf,sxu32 nBuf)` |
|      4 |  5886 | `{` |
|    667 |  5887 | `	return VmHintTextResolvedEx(pVm,&pAttr->sTypeName,` |
|    221 |  5888 | `		VmHintScopeDeclared(pAttr->pDeclClass),0,zBuf,nBuf);` |
|      4 |  5889 | `}` |
|      - |  5890 | `/* Describe the iPos-th parameter. Answers 0 when there is none. */` |
|   6506 |  5891 | `static int ReflectParamAt(const ReflectFuncRef *pRef, int iPos, ReflectParamDesc *pOut)` |
|      5 |  5892 | `{` |
|   6511 |  5893 | `	SyZero(pOut, sizeof(*pOut));` |
|   6511 |  5894 | `	if( iPos < 0 ){` |
|    ! 0 |  5895 | `		return 0;` |
|      - |  5896 | `	}` |
|   6511 |  5897 | `	if( pRef->zSig ){` |
|   5583 |  5898 | `		const char *zPart = 0;` |
|   5583 |  5899 | `		int nPart = 0, nTotal;` |
|   5583 |  5900 | `		nTotal = ReflectSigPart(pRef->zSig, (int)SyStrlen(pRef->zSig), iPos, &zPart, &nPart);` |
|   5583 |  5901 | `		if( iPos >= nTotal \|\| zPart == 0 ){` |
|      3 |  5902 | `			return 0;` |
|      - |  5903 | `		}` |
|   5581 |  5904 | `		ReflectSigDescribe(zPart, nPart, iPos, pOut);` |
|   5581 |  5905 | `		return 1;` |
|      - |  5906 | `	}` |
|    932 |  5907 | `	if( pRef->pFunc == 0 ){` |
|    ! 0 |  5908 | `		return 0;` |
|      - |  5909 | `	}` |
|      - |  5910 | `	{` |
|    932 |  5911 | `		ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pRef->pFunc->aArgs, (sxu32)iPos);` |
|    932 |  5912 | `		if( pArg == 0 ){` |
|     13 |  5913 | `			return 0;` |
|      - |  5914 | `		}` |
|    920 |  5915 | `		pOut->iPos = iPos;` |
|    920 |  5916 | `		pOut->sName = pArg->sName;` |
|    920 |  5917 | `		pOut->bByRef = (pArg->iFlags & VM_FUNC_ARG_BY_REF) != 0;` |
|    920 |  5918 | `		pOut->bVariadic = (pArg->iFlags & VM_FUNC_ARG_VARIADIC) != 0;` |
|      - |  5919 | `		/* The compiler never sets ARG_HAS_DEF; a default IS compiled byte-code` |
|      - |  5920 | `		 * (the same test the OP_CALL default-value path uses). */` |
|    920 |  5921 | `		pOut->bHasDef = SySetUsed(&pArg->aByteCode) > 0;` |
|    920 |  5922 | `		pOut->bNullable = (pArg->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|    920 |  5923 | `		pOut->bPromoted = (pArg->iFlags & VM_FUNC_ARG_PROMOTED) != 0;` |
|    920 |  5924 | `		pOut->bOptional = pOut->bVariadic \|\| pOut->bHasDef;` |
|    920 |  5925 | `		if( pRef->bFabricated ){` |
|      - |  5926 | `			/* php copies the closure's parameter list into an INTERNAL record, and a` |
|      - |  5927 | `			 * default VALUE does not survive that copy: every optional parameter of a` |
|      - |  5928 | ``			 * fabricated `Closure::__invoke` answers isDefaultValueAvailable() false`` |
|      - |  5929 | ``			 * and exports as `= <default>`, while staying optional. */`` |
|     69 |  5930 | `			pOut->bHasDef = 0;` |
|     34 |  5931 | `		}` |
|      - |  5932 | `		{` |
|   1378 |  5933 | `			const char *zT = VmHintTextResolvedEx(pRef->pVm,&pArg->sTypeName,` |
|    916 |  5934 | `				ReflectDeclScope(pRef),0,pOut->zTypeBuf,sizeof(pOut->zTypeBuf));` |
|    920 |  5935 | `			SyStringInitFromBuf(&pOut->sType,zT,(sxu32)SyStrlen(zT));` |
|      - |  5936 | `		}` |
|    920 |  5937 | `		pOut->pArg = pArg;` |
|   1802 |  5938 | `		pOut->bInternal = pRef->bFabricated` |
|    916 |  5939 | `			\|\| (pRef->pFunc->iFlags & VM_FUNC_INTERNAL) != 0;` |
|    920 |  5940 | `		return 1;` |
|      - |  5941 | `	}` |
|   3258 |  5942 | `}` |
|      - |  5943 | `/* The declaring class php reports for a reflected METHOD. */` |
|   8384 |  5944 | `static ph7_class * ReflectFuncDeclClass(const ReflectFuncRef *pRef)` |
|      3 |  5945 | `{` |
|   8387 |  5946 | `	if( pRef->pMeth == 0 \|\| pRef->pClass == 0 ){` |
|      3 |  5947 | `		return 0;` |
|      - |  5948 | `	}` |
|   8385 |  5949 | `	return ReflectMethodDeclClass(pRef->pClass, pRef->pMeth);` |
|   4195 |  5950 | `}` |
|      - |  5951 | `/*` |
|      - |  5952 | ` * The declared return type TEXT, or 0 — and whether php calls it TENTATIVE.` |
|      - |  5953 | ` *` |
|      - |  5954 | ` * php's stubs carry two kinds of internal return type and Reflection answers` |
|      - |  5955 | ` * differently for each: a real one is reported by getReturnType()/hasReturnType()` |
|      - |  5956 | `` * and printed `Return [ T ]`, while an `@tentative-return-type` answers NULL and`` |
|      - |  5957 | ` * false from those two, is reported by getTentativeReturnType()/` |
|      - |  5958 | `` * hasTentativeReturnType(), and prints `Tentative return [ T ]`. Nearly every`` |
|      - |  5959 | ` * internal SPL, date and Reflection method is the tentative kind.` |
|      - |  5960 | ` *` |
|      - |  5961 | `` * PHL has one field for both, so a leading `@` on a `zRet` — the same character`` |
|      - |  5962 | ` * php's stub annotation uses — marks the tentative kind. It is stripped here, so` |
|      - |  5963 | ` * nothing downstream of this function ever sees it.` |
|      - |  5964 | ` */` |
|   5477 |  5965 | `static int ReflectFuncRetTextEx(const ReflectFuncRef *pRef, const char **pz, int *pn,` |
|      - |  5966 | `	int *pbTentative, char *zBuf, sxu32 nBuf)` |
|      5 |  5967 | `{` |
|   5482 |  5968 | `	if( pbTentative ){` |
|   5482 |  5969 | `		*pbTentative = 0;` |
|   2737 |  5970 | `	}` |
|   5482 |  5971 | `	if( pRef->zRet && pRef->zRet[0] ){` |
|   4972 |  5972 | `		const char *z = pRef->zRet;` |
|   4972 |  5973 | `		if( z[0] == '@' ){` |
|   3121 |  5974 | `			if( pbTentative ){` |
|   3121 |  5975 | `				*pbTentative = 1;` |
|   1559 |  5976 | `			}` |
|   3121 |  5977 | `			z++;` |
|   1559 |  5978 | `		}` |
|   4972 |  5979 | `		*pz = z;` |
|   4972 |  5980 | `		*pn = (int)SyStrlen(z);` |
|   4972 |  5981 | `		return 1;` |
|      - |  5982 | `	}` |
|    515 |  5983 | `	if( pRef->pFunc == 0 ){` |
|     18 |  5984 | `		return 0;` |
|      - |  5985 | `	}` |
|    499 |  5986 | `	if( SyStringLength(&pRef->pFunc->sReturnTypeName) > 0 ){` |
|    161 |  5987 | `		*pz = VmHintTextResolvedEx(pRef->pVm,&pRef->pFunc->sReturnTypeName,` |
|     52 |  5988 | `			ReflectDeclScope(pRef),0,zBuf,nBuf);` |
|    109 |  5989 | `		*pn = (int)SyStrlen(*pz);` |
|    109 |  5990 | `		return 1;` |
|      - |  5991 | `	}` |
|    393 |  5992 | `	return 0;` |
|   2742 |  5993 | `}` |
|      - |  5994 | `/* The declared return type php would report from getReturnType(): a TENTATIVE one` |
|      - |  5995 | ` * is not reported there at all, which is the whole distinction. */` |
|    585 |  5996 | `static int ReflectFuncRetText(const ReflectFuncRef *pRef, const char **pz, int *pn,` |
|      - |  5997 | `	char *zBuf, sxu32 nBuf)` |
|      5 |  5998 | `{` |
|    590 |  5999 | `	int bTentative = 0;` |
|    590 |  6000 | `	if( !ReflectFuncRetTextEx(pRef, pz, pn, &bTentative, zBuf, nBuf) ){` |
|     24 |  6001 | `		return 0;` |
|      - |  6002 | `	}` |
|    568 |  6003 | `	return bTentative ? 0 : 1;` |
|    296 |  6004 | `}` |
|      - |  6005 | `/* Is the reflected function internal (a C builtin or an embedded-chunk one)? */` |
|   9239 |  6006 | `static int ReflectFuncIsInternal(const ReflectFuncRef *pRef)` |
|      4 |  6007 | `{` |
|   9243 |  6008 | `	if( pRef->pHost \|\| pRef->bFabricated ){` |
|   1105 |  6009 | `		return 1;` |
|      - |  6010 | `	}` |
|   8141 |  6011 | `	return pRef->pFunc != 0 && (pRef->pFunc->iFlags & VM_FUNC_INTERNAL) != 0;` |
|   4623 |  6012 | `}` |
|      - |  6013 | `/* Does this target carry a #[\Deprecated] attribute? */` |
|   3988 |  6014 | `static int ReflectHasDeprecated(SySet *pAttrs)` |
|      3 |  6015 | `{` |
|   3991 |  6016 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|      - |  6017 | `	sxu32 n;` |
|   3993 |  6018 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|      8 |  6019 | `		if( SyStringLength(&aA[n].sName) == sizeof("deprecated")-1` |
|      8 |  6020 | `		 && SyStrnicmp(SyStringData(&aA[n].sName), "deprecated", sizeof("deprecated")-1) == 0 ){` |
|      7 |  6021 | `			return 1;` |
|      - |  6022 | `		}` |
|      2 |  6023 | `	}` |
|   3985 |  6024 | `	return 0;` |
|   1997 |  6025 | `}` |
|      - |  6026 | ``/* Resolve `$this`, or answer a default when the target has gone missing. */`` |
|      - |  6027 | `#define REFLECT_FUNC_OR(REF,STMT) \` |
|      - |  6028 | `	if( !ReflectFuncOfThis(pCtx, &(REF)) ){ STMT; return PH7_OK; }` |
|      - |  6029 | `/*` |
|      - |  6030 | ` * The same, for the three getClosure* accessors: what they read is captured on` |
|      - |  6031 | ` * the Closure OBJECT, not in a function record, so a reflector over a closure` |
|      - |  6032 | ` * nothing resolves (php's magic-method trampoline) must still answer from the` |
|      - |  6033 | `` * Closure `$this` is holding. Without this they fell out at the resolve and`` |
|      - |  6034 | ` * reported no scope and no bound object for a callable php describes fully.` |
|      - |  6035 | ` */` |
|      - |  6036 | `#define REFLECT_CLOSURE_OR(REF,STMT) \` |
|      - |  6037 | `	if( !ReflectFuncOfThis(pCtx, &(REF)) ){ \` |
|      - |  6038 | `		ph7_class_instance *_pRcThis = PH7_ContextThis(pCtx); \` |
|      - |  6039 | `		SyZero(&(REF), sizeof(REF)); \` |
|      - |  6040 | `		(REF).pVm = pCtx->pVm; \` |
|      - |  6041 | `		(REF).pClosure = _pRcThis ? PH7_NativeAttrObj(_pRcThis, RF_CL) : 0; \` |
|      - |  6042 | `		if( (REF).pClosure == 0 ){ STMT; return PH7_OK; } \` |
|      - |  6043 | `	}` |
|      - |  6044 |  |
|      - |  6045 | `/* ---- ReflectionFunctionAbstract ---- */` |
|    ! 0 |  6046 | `static int vm_builtin_ReflectionFunc_clone(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  6047 | `{` |
|    ! 0 |  6048 | `	SXUNUSED(pCtx);` |
|    ! 0 |  6049 | `	SXUNUSED(nArg);` |
|    ! 0 |  6050 | `	SXUNUSED(apArg);` |
|    ! 0 |  6051 | `	return PH7_OK;` |
|    ! 0 |  6052 | `}` |
|    324 |  6053 | `static int vm_builtin_ReflectionFunc_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  6054 | `{` |
|    329 |  6055 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    329 |  6056 | `	const char *zName = "";` |
|    329 |  6057 | `	int nName = 0;` |
|    162 |  6058 | `	SXUNUSED(nArg);` |
|    162 |  6059 | `	SXUNUSED(apArg);` |
|    329 |  6060 | `	if( pThis ){` |
|    329 |  6061 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    162 |  6062 | `	}` |
|    329 |  6063 | `	ph7_result_string(pCtx, zName, nName);` |
|    329 |  6064 | `	return PH7_OK;` |
|      5 |  6065 | `}` |
|      - |  6066 | ``/* The `name` slot's namespace split — the same three answers ReflectionClass gives. */`` |
|    ! 0 |  6067 | `static int ReflectFuncNamePart(ph7_context *pCtx, int iWhat)` |
|    ! 0 |  6068 | `{` |
|    ! 0 |  6069 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    ! 0 |  6070 | `	const char *zName = "";` |
|    ! 0 |  6071 | `	int nName = 0, iCut;` |
|    ! 0 |  6072 | `	if( pThis ){` |
|    ! 0 |  6073 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    ! 0 |  6074 | `	}` |
|    ! 0 |  6075 | `	iCut = ReflectNsCut(zName, nName);` |
|    ! 0 |  6076 | `	if( iWhat == 0 ){` |
|    ! 0 |  6077 | `		ph7_result_bool(pCtx, iCut >= 0);` |
|    ! 0 |  6078 | `	}else if( iWhat == 1 ){` |
|    ! 0 |  6079 | `		ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|    ! 0 |  6080 | `	}else if( iCut < 0 ){` |
|    ! 0 |  6081 | `		ph7_result_string(pCtx, zName, nName);` |
|    ! 0 |  6082 | `	}else{` |
|    ! 0 |  6083 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|      - |  6084 | `	}` |
|    ! 0 |  6085 | `	return PH7_OK;` |
|    ! 0 |  6086 | `}` |
|      - |  6087 | `#define REFLECT_FUNC_NAMEPART(NAME,WHAT) \` |
|      - |  6088 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  6089 | `	{ \` |
|      - |  6090 | `		SXUNUSED(nArg); \` |
|      - |  6091 | `		SXUNUSED(apArg); \` |
|      - |  6092 | `		return ReflectFuncNamePart(pCtx, WHAT); \` |
|      - |  6093 | `	}` |
|    ! 0 |  6094 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_inNamespace, 0)` |
|    ! 0 |  6095 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_getNamespaceName, 1)` |
|    ! 0 |  6096 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_getShortName, 2)` |
|      - |  6097 |  |
|     14 |  6098 | `static int vm_builtin_ReflectionFunc_isClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6099 | `{` |
|      - |  6100 | `	ReflectFuncRef sRef;` |
|     15 |  6101 | `	int bAnon = 0;` |
|      7 |  6102 | `	SXUNUSED(nArg);` |
|      7 |  6103 | `	SXUNUSED(apArg);` |
|     15 |  6104 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     15 |  6105 | `	if( sRef.pFunc ){` |
|      - |  6106 | ``		/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|      - |  6107 | `		 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|     15 |  6108 | `		bAnon = (sRef.pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|     14 |  6109 | `		if( !bAnon && SyStringLength(&sRef.pFunc->sName) > 9` |
|      3 |  6110 | `		 && (SyMemcmp(SyStringData(&sRef.pFunc->sName), "[lambda_", 8) == 0` |
|    ! 0 |  6111 | `		  \|\| SyMemcmp(SyStringData(&sRef.pFunc->sName), "[closure_", 9) == 0) ){` |
|    ! 0 |  6112 | `			bAnon = 1;` |
|    ! 0 |  6113 | `		}` |
|      7 |  6114 | `	}` |
|     15 |  6115 | `	ph7_result_bool(pCtx, bAnon);` |
|     15 |  6116 | `	return PH7_OK;` |
|      8 |  6117 | `}` |
|      - |  6118 | `/*` |
|      - |  6119 | ` * php's deprecation for an INTERNAL name, or NULL. Userland's is the` |
|      - |  6120 | ` * #[\Deprecated] attribute; this is the engine's own table, stamped on the C` |
|      - |  6121 | ` * body a builtin and a native method share (aDeprecatedFunc[]).` |
|      - |  6122 | ` */` |
|   4588 |  6123 | `static const ph7_deprecated_name * ReflectFuncDeprecated(const ReflectFuncRef *pRef)` |
|      4 |  6124 | `{` |
|   4592 |  6125 | `	if( pRef->pHost ){` |
|    510 |  6126 | `		return pRef->pHost->pDeprecated;` |
|      - |  6127 | `	}` |
|   4085 |  6128 | `	if( pRef->pFunc && (pRef->pFunc->iFlags & VM_FUNC_NATIVE) && pRef->pFunc->pNative ){` |
|   3975 |  6129 | `		return pRef->pFunc->pNative->pDeprecated;` |
|      - |  6130 | `	}` |
|    112 |  6131 | `	return 0;` |
|   2298 |  6132 | `}` |
|     38 |  6133 | `static int vm_builtin_ReflectionFunc_isDeprecated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6134 | `{` |
|      - |  6135 | `	ReflectFuncRef sRef;` |
|     19 |  6136 | `	SXUNUSED(nArg);` |
|     19 |  6137 | `	SXUNUSED(apArg);` |
|     39 |  6138 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     49 |  6139 | `	ph7_result_bool(pCtx, ReflectFuncDeprecated(&sRef) != 0` |
|     24 |  6140 | `		\|\| (sRef.pFunc != 0 && ReflectHasDeprecated(&sRef.pFunc->aAttrs)));` |
|     39 |  6141 | `	return PH7_OK;` |
|     20 |  6142 | `}` |
|     54 |  6143 | `static int vm_builtin_ReflectionFunc_isInternal(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6144 | `{` |
|      - |  6145 | `	ReflectFuncRef sRef;` |
|     27 |  6146 | `	SXUNUSED(nArg);` |
|     27 |  6147 | `	SXUNUSED(apArg);` |
|     56 |  6148 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     56 |  6149 | `	ph7_result_bool(pCtx, ReflectFuncIsInternal(&sRef));` |
|     56 |  6150 | `	return PH7_OK;` |
|     29 |  6151 | `}` |
|      2 |  6152 | `static int vm_builtin_ReflectionFunc_isUserDefined(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6153 | `{` |
|      - |  6154 | `	ReflectFuncRef sRef;` |
|      1 |  6155 | `	SXUNUSED(nArg);` |
|      1 |  6156 | `	SXUNUSED(apArg);` |
|      3 |  6157 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|      3 |  6158 | `	ph7_result_bool(pCtx, !ReflectFuncIsInternal(&sRef));` |
|      3 |  6159 | `	return PH7_OK;` |
|      2 |  6160 | `}` |
|      6 |  6161 | `static int vm_builtin_ReflectionFunc_isGenerator(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6162 | `{` |
|      - |  6163 | `	ReflectFuncRef sRef;` |
|      3 |  6164 | `	SXUNUSED(nArg);` |
|      3 |  6165 | `	SXUNUSED(apArg);` |
|      7 |  6166 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|      7 |  6167 | `	ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|      7 |  6168 | `	return PH7_OK;` |
|      4 |  6169 | `}` |
|     10 |  6170 | `static int vm_builtin_ReflectionFunc_isVariadic(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6171 | `{` |
|      - |  6172 | `	ReflectFuncRef sRef;` |
|      - |  6173 | `	ReflectParamDesc sDesc;` |
|     11 |  6174 | `	int n, nTotal, bVariadic = 0;` |
|      5 |  6175 | `	SXUNUSED(nArg);` |
|      5 |  6176 | `	SXUNUSED(apArg);` |
|     11 |  6177 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     11 |  6178 | `	nTotal = ReflectParamCount(&sRef);` |
|     31 |  6179 | `	for( n = 0 ; n < nTotal ; n++ ){` |
|     29 |  6180 | `		if( ReflectParamAt(&sRef, n, &sDesc) && sDesc.bVariadic ){` |
|      9 |  6181 | `			bVariadic = 1;` |
|      9 |  6182 | `			break;` |
|      - |  6183 | `		}` |
|     11 |  6184 | `	}` |
|     11 |  6185 | `	ph7_result_bool(pCtx, bVariadic);` |
|     11 |  6186 | `	return PH7_OK;` |
|      6 |  6187 | `}` |
|      - |  6188 | ``/* isStatic(): a METHOD's own staticness, a closure's `static function () {}`. */`` |
|     88 |  6189 | `static int vm_builtin_ReflectionFunc_isStatic(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  6190 | `{` |
|      - |  6191 | `	ReflectFuncRef sRef;` |
|     44 |  6192 | `	SXUNUSED(nArg);` |
|     44 |  6193 | `	SXUNUSED(apArg);` |
|     92 |  6194 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     92 |  6195 | `	if( sRef.pMeth ){` |
|     82 |  6196 | `		ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|     43 |  6197 | `	}else{` |
|     11 |  6198 | `		ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);` |
|      - |  6199 | `	}` |
|     92 |  6200 | `	return PH7_OK;` |
|     48 |  6201 | `}` |
|      8 |  6202 | `static int vm_builtin_ReflectionFunc_returnsReference(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6203 | `{` |
|      - |  6204 | `	ReflectFuncRef sRef;` |
|      4 |  6205 | `	SXUNUSED(nArg);` |
|      4 |  6206 | `	SXUNUSED(apArg);` |
|      9 |  6207 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|      9 |  6208 | `	ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|      9 |  6209 | `	return PH7_OK;` |
|      5 |  6210 | `}` |
|      - |  6211 | `/* The Closure instance whose captured state the three getClosure* read. */` |
|     70 |  6212 | `static ph7_value * ReflectClosureAttr(ReflectFuncRef *pRef, const char *zName)` |
|      1 |  6213 | `{` |
|      - |  6214 | `	SyString sAttr;` |
|     71 |  6215 | `	if( pRef->pClosure == 0 ){` |
|    ! 0 |  6216 | `		return 0;` |
|      - |  6217 | `	}` |
|     71 |  6218 | `	SyStringInitFromBuf(&sAttr, zName, SyStrlen(zName));` |
|     71 |  6219 | `	return PH7_ClassInstanceFetchAttr(pRef->pClosure, &sAttr);` |
|     36 |  6220 | `}` |
|     20 |  6221 | `static int vm_builtin_ReflectionFunc_getClosureThis(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6222 | `{` |
|      - |  6223 | `	ReflectFuncRef sRef;` |
|      - |  6224 | `	ph7_value *pAttr;` |
|     10 |  6225 | `	SXUNUSED(nArg);` |
|     10 |  6226 | `	SXUNUSED(apArg);` |
|     21 |  6227 | `	REFLECT_CLOSURE_OR(sRef, ph7_result_null(pCtx))` |
|     21 |  6228 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|     21 |  6229 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|      9 |  6230 | `		ph7_result_value(pCtx, pAttr);` |
|      5 |  6231 | `	}else{` |
|     13 |  6232 | `		ph7_result_null(pCtx);` |
|      - |  6233 | `	}` |
|     21 |  6234 | `	return PH7_OK;` |
|     11 |  6235 | `}` |
|     34 |  6236 | `static int vm_builtin_ReflectionFunc_getClosureScopeClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6237 | `{` |
|      - |  6238 | `	ReflectFuncRef sRef;` |
|      - |  6239 | `	ph7_value *pAttr;` |
|     17 |  6240 | `	SXUNUSED(nArg);` |
|     17 |  6241 | `	SXUNUSED(apArg);` |
|     35 |  6242 | `	REFLECT_CLOSURE_OR(sRef, ph7_result_null(pCtx))` |
|     35 |  6243 | `	pAttr = ReflectClosureAttr(&sRef, "__scope");` |
|     35 |  6244 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|      - |  6245 | `		/* php reports the class that DECLARED the callee, not the one the callable NAMED:` |
|      - |  6246 | ``		 * `$__scope` is the CALLED scope (what `static::` answers, reported by`` |
|      - |  6247 | `		 * getClosureCalledClass below), and for an inherited or trait-composed method the` |
|      - |  6248 | `		 * two differ. PH7_VmClosureScopeClass is the one rule. */` |
|     43 |  6249 | `		return ReflectResultClassOf(pCtx,` |
|     14 |  6250 | `			PH7_VmClosureScopeClass(pCtx->pVm, sRef.pClosure));` |
|      - |  6251 | `	}` |
|      7 |  6252 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|      7 |  6253 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|    ! 0 |  6254 | `		return ReflectResultClassOf(pCtx, ((ph7_class_instance *)pAttr->x.pOther)->pClass);` |
|      - |  6255 | `	}` |
|      7 |  6256 | `	ph7_result_null(pCtx);` |
|      7 |  6257 | `	return PH7_OK;` |
|     18 |  6258 | `}` |
|      - |  6259 | `/*` |
|      - |  6260 | ` * ReflectionFunction::getClosureCalledClass() — php's CALLED scope, the other half of the` |
|      - |  6261 | `` * pair above: the class the call goes THROUGH, which is what `static::` answers. A bound`` |
|      - |  6262 | `` * closure's is its `$this`'s class; a static callable's is the class the callable NAMED. It`` |
|      - |  6263 | `` * shared getClosureScopeClass()'s body while `$__scope` answered both questions; it cannot`` |
|      - |  6264 | ` * now, because that one narrows a method closure to its DECLARING class.` |
|      - |  6265 | ` */` |
|      8 |  6266 | `static int vm_builtin_ReflectionFunc_getClosureCalledClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6267 | `{` |
|      - |  6268 | `	ReflectFuncRef sRef;` |
|      - |  6269 | `	ph7_value *pAttr;` |
|      4 |  6270 | `	SXUNUSED(nArg);` |
|      4 |  6271 | `	SXUNUSED(apArg);` |
|      9 |  6272 | `	REFLECT_CLOSURE_OR(sRef, ph7_result_null(pCtx))` |
|      9 |  6273 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|      9 |  6274 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|      7 |  6275 | `		return ReflectResultClassOf(pCtx, ((ph7_class_instance *)pAttr->x.pOther)->pClass);` |
|      - |  6276 | `	}` |
|      3 |  6277 | `	pAttr = ReflectClosureAttr(&sRef, "__scope");` |
|      3 |  6278 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|      4 |  6279 | `		return ReflectResultClassOf(pCtx, PH7_VmExtractClass(pCtx->pVm,` |
|      2 |  6280 | `			(const char *)SyBlobData(&pAttr->sBlob), SyBlobLength(&pAttr->sBlob), FALSE, 0));` |
|      - |  6281 | `	}` |
|    ! 0 |  6282 | `	ph7_result_null(pCtx);` |
|    ! 0 |  6283 | `	return PH7_OK;` |
|      5 |  6284 | `}` |
|     10 |  6285 | `static int vm_builtin_ReflectionFunc_getClosureUsedVariables(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6286 | `{` |
|      - |  6287 | `	ReflectFuncRef sRef;` |
|     12 |  6288 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  6289 | `	ph7_vm_func_closure_env *aEnv;` |
|      - |  6290 | `	sxu32 n;` |
|      5 |  6291 | `	SXUNUSED(nArg);` |
|      5 |  6292 | `	SXUNUSED(apArg);` |
|     12 |  6293 | `	if( pOut == 0 ){` |
|    ! 0 |  6294 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  6295 | `	}` |
|     12 |  6296 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pClosure == 0 \|\| sRef.pFunc == 0 ){` |
|    ! 0 |  6297 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  6298 | `		return PH7_OK;` |
|      - |  6299 | `	}` |
|      - |  6300 | `	/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */` |
|     12 |  6301 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|     28 |  6302 | `	for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; n++ ){` |
|     18 |  6303 | `		if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){` |
|     12 |  6304 | `			continue;` |
|      - |  6305 | `		}` |
|      6 |  6306 | `		if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|      4 |  6307 | `		 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|    ! 0 |  6308 | `			continue;` |
|      - |  6309 | `		}` |
|      7 |  6310 | `		if( (aEnv[n].iFlags & VM_FUNC_ARG_BY_REF) && aEnv[n].nIdx != SXU32_HIGH ){` |
|      - |  6311 | `			/* Captured by reference: report the slot's live value */` |
|      3 |  6312 | `			ph7_value *pLive = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, aEnv[n].nIdx);` |
|      3 |  6313 | `			ReflectMapAddDyn(pCtx, pOut, &aEnv[n].sName, pLive ? pLive : &aEnv[n].sValue);` |
|      3 |  6314 | `			continue;` |
|      - |  6315 | `		}` |
|      5 |  6316 | `		ReflectMapAddDyn(pCtx, pOut, &aEnv[n].sName, &aEnv[n].sValue);` |
|      3 |  6317 | `	}` |
|     12 |  6318 | `	ph7_result_value(pCtx, pOut);` |
|     12 |  6319 | `	return PH7_OK;` |
|      7 |  6320 | `}` |
|     14 |  6321 | `static int vm_builtin_ReflectionFunc_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6322 | `{` |
|      - |  6323 | `	ReflectFuncRef sRef;` |
|      7 |  6324 | `	SXUNUSED(nArg);` |
|      7 |  6325 | `	SXUNUSED(apArg);` |
|     15 |  6326 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     15 |  6327 | `	if( sRef.pFunc && SyStringLength(&sRef.pFunc->sDoc) > 0 ){` |
|     11 |  6328 | `		ph7_result_string(pCtx, SyStringData(&sRef.pFunc->sDoc), (int)SyStringLength(&sRef.pFunc->sDoc));` |
|      6 |  6329 | `	}else{` |
|      5 |  6330 | `		ph7_result_bool(pCtx, 0);` |
|      - |  6331 | `	}` |
|     15 |  6332 | `	return PH7_OK;` |
|      8 |  6333 | `}` |
|     28 |  6334 | `static int vm_builtin_ReflectionFunc_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6335 | `{` |
|      - |  6336 | `	ReflectFuncRef sRef;` |
|     14 |  6337 | `	SXUNUSED(nArg);` |
|     14 |  6338 | `	SXUNUSED(apArg);` |
|     30 |  6339 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     30 |  6340 | `	if( sRef.pFunc && !sRef.bFabricated && SyStringLength(&sRef.pFunc->sFile) > 0 ){` |
|      7 |  6341 | `		ph7_result_string(pCtx, SyStringData(&sRef.pFunc->sFile), (int)SyStringLength(&sRef.pFunc->sFile));` |
|      4 |  6342 | `	}else{` |
|     24 |  6343 | `		ph7_result_bool(pCtx, 0);` |
|      - |  6344 | `	}` |
|     30 |  6345 | `	return PH7_OK;` |
|     16 |  6346 | `}` |
|     28 |  6347 | `static int ReflectFuncLine(ph7_context *pCtx, int bEnd)` |
|      2 |  6348 | `{` |
|      - |  6349 | `	ReflectFuncRef sRef;` |
|     30 |  6350 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| ReflectFuncIsInternal(&sRef) \|\| sRef.pFunc == 0 ){` |
|     20 |  6351 | `		ph7_result_bool(pCtx, 0);` |
|     20 |  6352 | `		return PH7_OK;` |
|      - |  6353 | `	}` |
|     11 |  6354 | `	ph7_result_int64(pCtx, (sxi64)(bEnd ? sRef.pFunc->nEndLine : sRef.pFunc->nLine));` |
|     11 |  6355 | `	return PH7_OK;` |
|     16 |  6356 | `}` |
|     26 |  6357 | `static int vm_builtin_ReflectionFunc_getStartLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6358 | `{` |
|     13 |  6359 | `	SXUNUSED(nArg);` |
|     13 |  6360 | `	SXUNUSED(apArg);` |
|     28 |  6361 | `	return ReflectFuncLine(pCtx, 0);` |
|      2 |  6362 | `}` |
|      2 |  6363 | `static int vm_builtin_ReflectionFunc_getEndLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6364 | `{` |
|      1 |  6365 | `	SXUNUSED(nArg);` |
|      1 |  6366 | `	SXUNUSED(apArg);` |
|      3 |  6367 | `	return ReflectFuncLine(pCtx, 1);` |
|      1 |  6368 | `}` |
|    206 |  6369 | `static int vm_builtin_ReflectionFunc_hasReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6370 | `{` |
|      - |  6371 | `	ReflectFuncRef sRef;` |
|      - |  6372 | `	const char *z;` |
|      - |  6373 | `	char zRetBuf[192];` |
|      - |  6374 | `	int n;` |
|    103 |  6375 | `	SXUNUSED(nArg);` |
|    103 |  6376 | `	SXUNUSED(apArg);` |
|    208 |  6377 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    208 |  6378 | `	ph7_result_bool(pCtx, ReflectFuncRetText(&sRef, &z, &n, zRetBuf, sizeof(zRetBuf)));` |
|    208 |  6379 | `	return PH7_OK;` |
|    105 |  6380 | `}` |
|    379 |  6381 | `static int vm_builtin_ReflectionFunc_getReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  6382 | `{` |
|      - |  6383 | `	ReflectFuncRef sRef;` |
|      - |  6384 | `	const char *z;` |
|      - |  6385 | `	char zRetBuf[192];` |
|      - |  6386 | `	int n;` |
|    188 |  6387 | `	SXUNUSED(nArg);` |
|    188 |  6388 | `	SXUNUSED(apArg);` |
|    384 |  6389 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|    384 |  6390 | `	if( !ReflectFuncRetText(&sRef, &z, &n, zRetBuf, sizeof(zRetBuf)) ){` |
|     27 |  6391 | `		ph7_result_null(pCtx);` |
|     27 |  6392 | `		return PH7_OK;` |
|      - |  6393 | `	}` |
|    360 |  6394 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx, z, n));` |
|    193 |  6395 | `}` |
|      - |  6396 | ``/* A native method's `@`-marked zRet is php's `@tentative-return-type`: reported`` |
|      - |  6397 | ` * HERE and by nothing else, which is what separates it from a real one. */` |
|    162 |  6398 | `static int vm_builtin_ReflectionFunc_hasTentativeReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6399 | `{` |
|      - |  6400 | `	ReflectFuncRef sRef;` |
|      - |  6401 | `	const char *z;` |
|      - |  6402 | `	char zRetBuf[192];` |
|    163 |  6403 | `	int n, bTentative = 0;` |
|     81 |  6404 | `	SXUNUSED(nArg);` |
|     81 |  6405 | `	SXUNUSED(apArg);` |
|    163 |  6406 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    163 |  6407 | `	ph7_result_bool(pCtx, ReflectFuncRetTextEx(&sRef, &z, &n, &bTentative, zRetBuf, sizeof(zRetBuf)) && bTentative);` |
|    163 |  6408 | `	return PH7_OK;` |
|     82 |  6409 | `}` |
|    180 |  6410 | `static int vm_builtin_ReflectionFunc_getTentativeReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6411 | `{` |
|      - |  6412 | `	ReflectFuncRef sRef;` |
|      - |  6413 | `	const char *z;` |
|      - |  6414 | `	char zRetBuf[192];` |
|    181 |  6415 | `	int n, bTentative = 0;` |
|     90 |  6416 | `	SXUNUSED(nArg);` |
|     90 |  6417 | `	SXUNUSED(apArg);` |
|    181 |  6418 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|    181 |  6419 | `	if( !ReflectFuncRetTextEx(&sRef, &z, &n, &bTentative, zRetBuf, sizeof(zRetBuf)) \|\| !bTentative ){` |
|      7 |  6420 | `		ph7_result_null(pCtx);` |
|      7 |  6421 | `		return PH7_OK;` |
|      - |  6422 | `	}` |
|    175 |  6423 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx, z, n));` |
|     91 |  6424 | `}` |
|    104 |  6425 | `static int vm_builtin_ReflectionFunc_getNumberOfParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  6426 | `{` |
|      - |  6427 | `	ReflectFuncRef sRef;` |
|     52 |  6428 | `	SXUNUSED(nArg);` |
|     52 |  6429 | `	SXUNUSED(apArg);` |
|    107 |  6430 | `	REFLECT_FUNC_OR(sRef, ph7_result_int(pCtx, 0))` |
|    101 |  6431 | `	if( sRef.pHost && sRef.zSig == 0 ){` |
|      - |  6432 | `		/* An UNDESCRIBED builtin: the arity table is all there is. */` |
|    ! 0 |  6433 | `		ph7_result_int64(pCtx, (sxi64)sRef.pHost->nMinArg);` |
|    ! 0 |  6434 | `		return PH7_OK;` |
|      - |  6435 | `	}` |
|    101 |  6436 | `	ph7_result_int(pCtx, ReflectParamCount(&sRef));` |
|    101 |  6437 | `	return PH7_OK;` |
|     55 |  6438 | `}` |
|     46 |  6439 | `static int vm_builtin_ReflectionFunc_getNumberOfRequiredParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6440 | `{` |
|      - |  6441 | `	ReflectFuncRef sRef;` |
|      - |  6442 | `	ReflectParamDesc sDesc;` |
|     48 |  6443 | `	int n, nTotal, nReq = 0;` |
|     23 |  6444 | `	SXUNUSED(nArg);` |
|     23 |  6445 | `	SXUNUSED(apArg);` |
|     48 |  6446 | `	REFLECT_FUNC_OR(sRef, ph7_result_int(pCtx, 0))` |
|     48 |  6447 | `	if( sRef.pHost && sRef.zSig == 0 ){` |
|    ! 0 |  6448 | `		ph7_result_int64(pCtx, (sxi64)sRef.pHost->nMinArg);` |
|    ! 0 |  6449 | `		return PH7_OK;` |
|      - |  6450 | `	}` |
|     48 |  6451 | `	nTotal = ReflectParamCount(&sRef);` |
|    114 |  6452 | `	for( n = nTotal ; n > 0 ; n-- ){` |
|    104 |  6453 | `		if( ReflectParamAt(&sRef, n - 1, &sDesc) && !sDesc.bVariadic && !sDesc.bOptional ){` |
|     38 |  6454 | `			nReq = n;` |
|     38 |  6455 | `			break;` |
|      - |  6456 | `		}` |
|     35 |  6457 | `	}` |
|     48 |  6458 | `	ph7_result_int(pCtx, nReq);` |
|     48 |  6459 | `	return PH7_OK;` |
|     25 |  6460 | `}` |
|      - |  6461 | `/* The (target, method) pair a ReflectionParameter is built over. */` |
|    710 |  6462 | `static void ReflectFuncParamSpec(ph7_context *pCtx, ph7_value *pSpec)` |
|      5 |  6463 | `{` |
|    715 |  6464 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  6465 | `	ph7_class_instance *pClo;` |
|      - |  6466 | `	const char *zName, *zClass;` |
|      - |  6467 | `	int nName, nClass;` |
|    715 |  6468 | `	PH7_MemObjInit(pCtx->pVm, pSpec);` |
|    715 |  6469 | `	if( pThis == 0 ){` |
|    ! 0 |  6470 | `		return;` |
|      - |  6471 | `	}` |
|    715 |  6472 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|    715 |  6473 | `	if( pClo && !ReflectIsMethodReflector(pCtx, pThis) ){` |
|     11 |  6474 | `		pSpec->x.pOther = pClo;` |
|     11 |  6475 | `		pSpec->iFlags = MEMOBJ_OBJ;` |
|     11 |  6476 | `		return;` |
|      - |  6477 | `	}` |
|    705 |  6478 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    705 |  6479 | `	if( ReflectIsMethodReflector(pCtx, pThis) ){` |
|      - |  6480 | ``		/* `[class, method]`, which ReflectionParameter's constructor accepts -- and`` |
|      - |  6481 | ``		 * for the fabricated `Closure::__invoke` the OBJECT stands in for the class,`` |
|      - |  6482 | `		 * because that pair is what resolves to the closure's parameter list while` |
|      - |  6483 | ``		 * still naming a method (php's parameter reports `Closure` as its declaring`` |
|      - |  6484 | ``		 * class and `__invoke` as its declaring function). */`` |
|    303 |  6485 | `		ph7_value *pList = ph7_context_new_array(pCtx);` |
|    303 |  6486 | `		ph7_value *pA = ph7_context_new_scalar(pCtx);` |
|    303 |  6487 | `		ph7_value *pB = ph7_context_new_scalar(pCtx);` |
|    303 |  6488 | `		if( pList == 0 \|\| pA == 0 \|\| pB == 0 ){` |
|    ! 0 |  6489 | `			return;` |
|      - |  6490 | `		}` |
|    303 |  6491 | `		if( pClo ){` |
|      - |  6492 | `			/* pA is a CONTEXT value: the call's teardown releases it, so stamping an` |
|      - |  6493 | `			 * object into it has to take a reference the way any other holder does.` |
|      - |  6494 | `			 * (Without it the closure was unref'd once per getParameters() call and` |
|      - |  6495 | `			 * freed under its own Closure object -- a use-after-free that surfaced` |
|      - |  6496 | `			 * three hundred files into a phpstan run.) */` |
|      5 |  6497 | `			pClo->iRef++;` |
|      5 |  6498 | `			pA->x.pOther = pClo;` |
|      5 |  6499 | `			pA->iFlags = MEMOBJ_OBJ;` |
|      3 |  6500 | `		}else{` |
|    299 |  6501 | `			PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|    299 |  6502 | `			ph7_value_string(pA, zClass, nClass);` |
|      - |  6503 | `		}` |
|    303 |  6504 | `		ph7_value_string(pB, zName, nName);` |
|    303 |  6505 | `		ph7_array_add_elem(pList, 0, pA);` |
|    303 |  6506 | `		ph7_array_add_elem(pList, 0, pB);` |
|    303 |  6507 | `		PH7_MemObjStore(pList, pSpec);` |
|    303 |  6508 | `		return;` |
|      - |  6509 | `	}` |
|    407 |  6510 | `	ph7_value_string(pSpec, zName, nName);` |
|    360 |  6511 | `}` |
|    694 |  6512 | `static int vm_builtin_ReflectionFunc_getParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  6513 | `{` |
|      - |  6514 | `	ReflectFuncRef sRef;` |
|    699 |  6515 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  6516 | `	ph7_value sSpec;` |
|      - |  6517 | `	int n, nTotal;` |
|    347 |  6518 | `	SXUNUSED(nArg);` |
|    347 |  6519 | `	SXUNUSED(apArg);` |
|    699 |  6520 | `	if( pOut == 0 ){` |
|    ! 0 |  6521 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  6522 | `	}` |
|    699 |  6523 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 |  6524 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  6525 | `		return PH7_OK;` |
|      - |  6526 | `	}` |
|    699 |  6527 | `	nTotal = ReflectParamCount(&sRef);` |
|    699 |  6528 | `	ReflectFuncParamSpec(pCtx, &sSpec);` |
|   1967 |  6529 | `	for( n = 0 ; n < nTotal ; n++ ){` |
|      - |  6530 | `		ph7_value sPos, sVal;` |
|      - |  6531 | `		ph7_value *apCtor[2];` |
|      - |  6532 | `		ph7_class_instance *pParam;` |
|      - |  6533 | `		sxi32 rc;` |
|   1273 |  6534 | `		PH7_MemObjInit(pCtx->pVm, &sPos);` |
|   1273 |  6535 | `		ph7_value_int(&sPos, n);` |
|   1273 |  6536 | `		apCtor[0] = &sSpec;` |
|   1273 |  6537 | `		apCtor[1] = &sPos;` |
|   1273 |  6538 | `		pParam = ReflectConstruct(pCtx, "ReflectionParameter", 2, apCtor, &rc);` |
|   1273 |  6539 | `		PH7_MemObjRelease(&sPos);` |
|   1273 |  6540 | `		if( pParam == 0 ){` |
|    ! 0 |  6541 | `			break;` |
|      - |  6542 | `		}` |
|   1273 |  6543 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|   1273 |  6544 | `		sVal.x.pOther = pParam;` |
|   1273 |  6545 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|   1273 |  6546 | `		ph7_array_add_elem(pOut, 0, &sVal);   /* takes its own reference */` |
|   1273 |  6547 | `		PH7_ClassInstanceUnref(pParam);` |
|    639 |  6548 | `	}` |
|    699 |  6549 | `	if( (sSpec.iFlags & MEMOBJ_OBJ) == 0 ){` |
|    693 |  6550 | `		PH7_MemObjRelease(&sSpec);` |
|    344 |  6551 | `	}` |
|    699 |  6552 | `	ph7_result_value(pCtx, pOut);` |
|    699 |  6553 | `	return PH7_OK;` |
|    352 |  6554 | `}` |
|      4 |  6555 | `static int vm_builtin_ReflectionFunc_getStaticVariables(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6556 | `{` |
|      - |  6557 | `	ReflectFuncRef sRef;` |
|      5 |  6558 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  6559 | `	ph7_vm_func_static_var *aStatic;` |
|      - |  6560 | `	sxu32 n;` |
|      2 |  6561 | `	SXUNUSED(nArg);` |
|      2 |  6562 | `	SXUNUSED(apArg);` |
|      5 |  6563 | `	if( pOut == 0 ){` |
|    ! 0 |  6564 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  6565 | `	}` |
|      5 |  6566 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pFunc == 0 ){` |
|    ! 0 |  6567 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  6568 | `		return PH7_OK;` |
|      - |  6569 | `	}` |
|      - |  6570 | ``	/* php reports a closure's captured `use` variables here TOO, ahead of the body's`` |
|      - |  6571 | `	 * own statics: it compiles both into one static-variables table, and` |
|      - |  6572 | `	 * ReflectionFunction::getStaticVariables() hands back the whole thing. PHL listed` |
|      - |  6573 | ``	 * only the `static $x` ones, so a closure's captures were invisible to reflection.`` |
|      - |  6574 | `	 *` |
|      - |  6575 | `	 * Pest reads exactly this to find the closure a test case wraps --` |
|      - |  6576 | ``	 * `Reflection::getFunctionVariable($this->__test, 'closure')` is`` |
|      - |  6577 | ``	 * `(new ReflectionFunction($f))->getStaticVariables()['closure']` -- and with the`` |
|      - |  6578 | `	 * captures missing it got null and EVERY test in a Pest suite failed on the` |
|      - |  6579 | `	 * TypeError that followed.` |
|      - |  6580 | `	 *` |
|      - |  6581 | ``	 * `$this` is not one of them: php keeps the receiver in its own slot and never`` |
|      - |  6582 | `	 * lists it as a static, while PHL carries it as an ordinary env entry. A by-REFERENCE` |
|      - |  6583 | `	 * capture reports what its slot holds NOW, not the value at closure creation. */` |
|      - |  6584 | `	{` |
|      5 |  6585 | `		ph7_vm_func_closure_env *aEnv =` |
|      4 |  6586 | `			(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|      - |  6587 | `		sxu32 k;` |
|      5 |  6588 | `		for( k = 0 ; k < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++k ){` |
|    ! 0 |  6589 | `			ph7_value *pVal = &aEnv[k].sValue;` |
|    ! 0 |  6590 | `			if( SyStringLength(&aEnv[k].sName) == sizeof("this")-1` |
|    ! 0 |  6591 | `			 && SyMemcmp(SyStringData(&aEnv[k].sName), "this", sizeof("this")-1) == 0 ){` |
|    ! 0 |  6592 | `				continue;` |
|      - |  6593 | `			}` |
|    ! 0 |  6594 | `			if( aEnv[k].nIdx != SXU32_HIGH ){` |
|    ! 0 |  6595 | `				ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, aEnv[k].nIdx);` |
|    ! 0 |  6596 | `				if( pSlot ){` |
|    ! 0 |  6597 | `					pVal = pSlot;` |
|    ! 0 |  6598 | `				}` |
|    ! 0 |  6599 | `			}` |
|    ! 0 |  6600 | `			ReflectMapAddDyn(pCtx, pOut, &aEnv[k].sName, pVal);` |
|    ! 0 |  6601 | `		}` |
|      - |  6602 | `	}` |
|      - |  6603 | `	/* Current value when the slot was initialized (first call), otherwise the` |
|      - |  6604 | `	 * evaluated default — php's getStaticVariables initializes on demand and` |
|      - |  6605 | `	 * reports the same values. */` |
|      5 |  6606 | `	aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&sRef.pFunc->aStatic);` |
|      9 |  6607 | `	for( n = 0 ; n < SySetUsed(&sRef.pFunc->aStatic) ; n++ ){` |
|      5 |  6608 | `		ph7_value *pVal = 0;` |
|      - |  6609 | `		ph7_value sScratch;` |
|      5 |  6610 | `		int bScratch = 0;` |
|      5 |  6611 | `		if( aStatic[n].nIdx != SXU32_HIGH ){` |
|      3 |  6612 | `			pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);` |
|      1 |  6613 | `		}` |
|      5 |  6614 | `		if( pVal == 0 ){` |
|      3 |  6615 | `			PH7_MemObjInit(pCtx->pVm, &sScratch);` |
|      3 |  6616 | `			if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      3 |  6617 | `				VmLocalExec(pCtx->pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|      1 |  6618 | `			}` |
|      3 |  6619 | `			pVal = &sScratch;` |
|      3 |  6620 | `			bScratch = 1;` |
|      1 |  6621 | `		}` |
|      5 |  6622 | `		ReflectMapAddDyn(pCtx, pOut, &aStatic[n].sName, pVal);` |
|      5 |  6623 | `		if( bScratch ){` |
|      3 |  6624 | `			PH7_MemObjRelease(&sScratch);` |
|      1 |  6625 | `		}` |
|      3 |  6626 | `	}` |
|      5 |  6627 | `	ph7_result_value(pCtx, pOut);` |
|      5 |  6628 | `	return PH7_OK;` |
|      3 |  6629 | `}` |
|      - |  6630 | ``/* One `key => value` entry of a presented shape, by name. */`` |
|     46 |  6631 | `static void ClosurePresentAdd(ph7_vm *pVm, ph7_value *pOut, const char *zKey, ph7_value *pVal)` |
|      1 |  6632 | `{` |
|      - |  6633 | `	ph7_value sKey;` |
|     47 |  6634 | `	PH7_MemObjInitFromString(pVm, &sKey, 0);` |
|     47 |  6635 | `	PH7_MemObjStringAppend(&sKey, zKey, (sxu32)SyStrlen(zKey));` |
|     47 |  6636 | `	ph7_array_add_elem(pOut, &sKey, pVal);   /* takes its OWN reference */` |
|     47 |  6637 | `	PH7_MemObjRelease(&sKey);` |
|     47 |  6638 | `}` |
|      - |  6639 | `/*` |
|      - |  6640 | ` * php's DEBUG presentation for a Closure (ph7_class::xPresent) —` |
|      - |  6641 | `` * `zend_closure_get_debug_info` (Zend/zend_closures.c), key for key and in php's`` |
|      - |  6642 | `` * order. Every key is CONDITIONAL, which is why a plain `function(){}` shows three`` |
|      - |  6643 | ` * and a bound one with captures shows five:` |
|      - |  6644 | ` *` |
|      - |  6645 | ` *   - a FAKE closure (php's ZEND_ACC_FAKE_CLOSURE — a first-class callable or` |
|      - |  6646 | `` *     `Closure::fromCallable` over a NAMED function) shows one `function` key,`` |
|      - |  6647 | `` *     `Class::method` when it carries a scope and the bare name otherwise, and NO`` |
|      - |  6648 | `` *     name/file/line. A real closure shows `name` (php's `{closure:SCOPE:LINE}`,`` |
|      - |  6649 | `` *     which `PH7_VmFuncDisplayName` answers), `file` and an INT `line`;`` |
|      - |  6650 | `` *   - `static` is the closure's own variable state — the `use` captures AND the`` |
|      - |  6651 | `` *     body's `static $x` — keyed WITHOUT the `$`, and omitted when empty. php`` |
|      - |  6652 | ` *     shows it before the first call too, since the initializers are compiled;` |
|      - |  6653 | `` *   - `this` is the bound receiver, present only when there is one (`bindTo`,`` |
|      - |  6654 | ` *     an instance first-class callable, or the implicit capture in a method);` |
|      - |  6655 | `` *   - `parameter` is `"$name" => "<required>"\|"<optional>"`, `&$name` for a by-ref`` |
|      - |  6656 | ` *     parameter, omitted when the callee takes none. php's cut is` |
|      - |  6657 | `` *     `required_num_args`, which counts through the LAST required parameter — so`` |
|      - |  6658 | `` *     `function($a, $b = 1, $c)` reports all THREE as `<required>`, not two.`` |
|      - |  6659 | ` *` |
|      - |  6660 | `` * The non-debug half answers nothing: php's `get_properties` for a Closure is an`` |
|      - |  6661 | `` * empty table, and `(array)$closure` never reaches it at all (php special-cases a`` |
|      - |  6662 | `` * Closure in `convert_to_array` and wraps it as a SCALAR, `[0 => $closure]`; PHL`` |
|      - |  6663 | `` * answers `[]` there — recorded).`` |
|      - |  6664 | ` */` |
|     20 |  6665 | `PH7_PRIVATE sxi32 PH7_ClosurePresent(ph7_vm *pVm, ph7_class_instance *pThis,` |
|      - |  6666 | `	ph7_value *pOut, int bDebug)` |
|      2 |  6667 | `{` |
|      - |  6668 | `	ReflectFuncRef sRef;` |
|      - |  6669 | `	ph7_value sCarrier, sVal;` |
|     22 |  6670 | `	int bFake = 1;` |
|     22 |  6671 | `	if( !bDebug \|\| pThis == 0 ){` |
|      8 |  6672 | `		return SXRET_OK;` |
|      - |  6673 | `	}` |
|      - |  6674 | `	/* Rule 16's other half: this carrier never took a reference, so it must be` |
|      - |  6675 | `	 * blanked before it is released or the Closure is unref'd a second time. */` |
|     15 |  6676 | `	PH7_MemObjInit(pVm, &sCarrier);` |
|     15 |  6677 | `	sCarrier.x.pOther = pThis;` |
|     15 |  6678 | `	MemObjSetType(&sCarrier, MEMOBJ_OBJ);` |
|     15 |  6679 | `	if( !ReflectFuncFill(pVm, &sCarrier, 0, &sRef) ){` |
|    ! 0 |  6680 | `		sCarrier.x.pOther = 0;` |
|    ! 0 |  6681 | `		sCarrier.iFlags = MEMOBJ_NULL;` |
|    ! 0 |  6682 | `		PH7_MemObjRelease(&sCarrier);` |
|    ! 0 |  6683 | `		return SXRET_OK;` |
|      - |  6684 | `	}` |
|     15 |  6685 | `	sCarrier.x.pOther = 0;` |
|     15 |  6686 | `	sCarrier.iFlags = MEMOBJ_NULL;` |
|     15 |  6687 | `	PH7_MemObjRelease(&sCarrier);` |
|      - |  6688 | `	/* php's FAKE-closure bit, read off what the callable actually resolved TO: a` |
|      - |  6689 | `	 * real closure's body is the anonymous function itself (the compiler's` |
|      - |  6690 | ``	 * `{closure:...}` name, or the synthesized key that stands in for it). */`` |
|     15 |  6691 | `	if( sRef.pFunc && sRef.pMeth == 0 ){` |
|      9 |  6692 | `		const SyString *pN = &sRef.pFunc->sName;` |
|      8 |  6693 | `		if( SyStringLength(&sRef.pFunc->sClosureName) > 0` |
|      4 |  6694 | `		 \|\| (pN->nByte > 8 && SyMemcmp(pN->zString, "[lambda_", 8) == 0)` |
|      1 |  6695 | `		 \|\| (pN->nByte > 9 && SyMemcmp(pN->zString, "[closure_", 9) == 0) ){` |
|      9 |  6696 | `			bFake = 0;` |
|      4 |  6697 | `		}` |
|      4 |  6698 | `	}` |
|     15 |  6699 | `	PH7_MemObjInit(pVm, &sVal);` |
|     15 |  6700 | `	if( bFake ){` |
|      7 |  6701 | `		ph7_class *pScope = ReflectFuncDeclClass(&sRef);` |
|      7 |  6702 | `		const SyString *pName = sRef.pFunc ? &sRef.pFunc->sName : &sRef.pHost->sName;` |
|      7 |  6703 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|      7 |  6704 | `		if( pScope == 0 && sRef.pClass ){` |
|    ! 0 |  6705 | `			pScope = sRef.pClass;` |
|    ! 0 |  6706 | `		}` |
|      7 |  6707 | `		if( pScope ){` |
|      7 |  6708 | `			PH7_MemObjStringAppend(&sVal, SyStringData(&pScope->sName),` |
|      2 |  6709 | `				SyStringLength(&pScope->sName));` |
|      5 |  6710 | `			PH7_MemObjStringAppend(&sVal, "::", 2);` |
|      2 |  6711 | `		}` |
|      7 |  6712 | `		PH7_MemObjStringAppend(&sVal, SyStringData(pName), SyStringLength(pName));` |
|      7 |  6713 | `		ClosurePresentAdd(pVm, pOut, "function", &sVal);` |
|      7 |  6714 | `		PH7_MemObjRelease(&sVal);` |
|      4 |  6715 | `	}else{` |
|      - |  6716 | `		const char *zShow;` |
|      9 |  6717 | `		int nShow = PH7_VmFuncDisplayName(pVm, sRef.pFunc, &zShow);` |
|      9 |  6718 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|      9 |  6719 | `		PH7_MemObjStringAppend(&sVal, zShow, (sxu32)(nShow > 0 ? nShow : 0));` |
|      9 |  6720 | `		ClosurePresentAdd(pVm, pOut, "name", &sVal);` |
|      9 |  6721 | `		PH7_MemObjRelease(&sVal);` |
|      9 |  6722 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|     13 |  6723 | `		PH7_MemObjStringAppend(&sVal, SyStringData(&sRef.pFunc->sFile),` |
|      8 |  6724 | `			SyStringLength(&sRef.pFunc->sFile));` |
|      9 |  6725 | `		ClosurePresentAdd(pVm, pOut, "file", &sVal);` |
|      9 |  6726 | `		PH7_MemObjRelease(&sVal);` |
|      9 |  6727 | `		PH7_MemObjInitFromInt(pVm, &sVal, (sxi64)sRef.pFunc->nLine);` |
|      9 |  6728 | `		ClosurePresentAdd(pVm, pOut, "line", &sVal);` |
|      9 |  6729 | `		PH7_MemObjRelease(&sVal);` |
|      - |  6730 | `	}` |
|      - |  6731 | ``	/* `static`: the captured `use` variables first (php compiles them into the`` |
|      - |  6732 | ``	 * same table, ahead of the body's own statics), then the body's `static $x`,`` |
|      - |  6733 | `	 * whose value is the live slot once it exists and the compiled initializer` |
|      - |  6734 | `	 * before that — the rule getStaticVariables() already follows. */` |
|     15 |  6735 | `	if( sRef.pFunc ){` |
|     13 |  6736 | `		ph7_hashmap *pHm = PH7_NewHashmap(pVm, 0, 0);` |
|      - |  6737 | `		sxu32 n;` |
|     13 |  6738 | `		if( pHm ){` |
|      - |  6739 | `			ph7_value sMap;` |
|     13 |  6740 | `			ph7_value *pMap = &sMap;` |
|     13 |  6741 | `			ph7_vm_func_closure_env *aEnv =` |
|     12 |  6742 | `				(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|     13 |  6743 | `			ph7_vm_func_static_var *aStatic =` |
|     12 |  6744 | `				(ph7_vm_func_static_var *)SySetBasePtr(&sRef.pFunc->aStatic);` |
|     13 |  6745 | `			PH7_MemObjInitFromArray(pVm, pMap, pHm);` |
|     25 |  6746 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++n ){` |
|     13 |  6747 | `				ph7_value *pVal = &aEnv[n].sValue;` |
|      - |  6748 | `				ph7_value sKey;` |
|     12 |  6749 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|     11 |  6750 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|      - |  6751 | ``					/* PHL carries the auto-captured `$this` as an env entry; php keeps`` |
|      - |  6752 | ``					 * it in its own `this_ptr` slot and never lists it as a static. It`` |
|      - |  6753 | ``					 * is reported below, under `this`. */`` |
|      9 |  6754 | `					continue;` |
|      - |  6755 | `				}` |
|      5 |  6756 | `				if( aEnv[n].nIdx != SXU32_HIGH ){` |
|      - |  6757 | ``					/* `use (&$x)`: the capture is an alias onto a pinned slot, and`` |
|      - |  6758 | `					 * php reports what the slot holds NOW, not the birth value. */` |
|    ! 0 |  6759 | `					ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, aEnv[n].nIdx);` |
|    ! 0 |  6760 | `					if( pSlot ){` |
|    ! 0 |  6761 | `						pVal = pSlot;` |
|    ! 0 |  6762 | `					}` |
|    ! 0 |  6763 | `				}` |
|      5 |  6764 | `				PH7_MemObjInitFromString(pVm, &sKey, &aEnv[n].sName);` |
|      5 |  6765 | `				ph7_array_add_elem(pMap, &sKey, pVal);` |
|      5 |  6766 | `				PH7_MemObjRelease(&sKey);` |
|      3 |  6767 | `			}` |
|     15 |  6768 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aStatic) ; ++n ){` |
|      3 |  6769 | `				ph7_value *pVal = 0;` |
|      - |  6770 | `				ph7_value sScratch, sKey;` |
|      3 |  6771 | `				int bScratch = 0;` |
|      3 |  6772 | `				if( aStatic[n].nIdx != SXU32_HIGH ){` |
|    ! 0 |  6773 | `					pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, aStatic[n].nIdx);` |
|    ! 0 |  6774 | `				}` |
|      3 |  6775 | `				if( pVal == 0 ){` |
|      3 |  6776 | `					PH7_MemObjInit(pVm, &sScratch);` |
|      3 |  6777 | `					if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      3 |  6778 | `						VmLocalExec(pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|      1 |  6779 | `					}` |
|      3 |  6780 | `					pVal = &sScratch;` |
|      3 |  6781 | `					bScratch = 1;` |
|      1 |  6782 | `				}` |
|      3 |  6783 | `				PH7_MemObjInitFromString(pVm, &sKey, &aStatic[n].sName);` |
|      3 |  6784 | `				ph7_array_add_elem(pMap, &sKey, pVal);` |
|      3 |  6785 | `				PH7_MemObjRelease(&sKey);` |
|      3 |  6786 | `				if( bScratch ){` |
|      3 |  6787 | `					PH7_MemObjRelease(&sScratch);` |
|      1 |  6788 | `				}` |
|      2 |  6789 | `			}` |
|     13 |  6790 | `			if( ph7_array_count(pMap) > 0 ){` |
|      5 |  6791 | `				ClosurePresentAdd(pVm, pOut, "static", pMap);` |
|      2 |  6792 | `			}` |
|     13 |  6793 | `			PH7_MemObjRelease(pMap);` |
|      6 |  6794 | `		}` |
|      6 |  6795 | `	}` |
|      - |  6796 | ``	/* `this`: the bound receiver. php keeps ONE `this_ptr` however the binding`` |
|      - |  6797 | ``	 * happened; PHL keeps two — an explicit bind (`bindTo`, an instance`` |
|      - |  6798 | ``	 * first-class callable) writes the `$__this` slot, while the IMPLICIT capture a`` |
|      - |  6799 | `	 * closure written inside a method gets rides in the closure ENVIRONMENT under` |
|      - |  6800 | ``	 * the name `this`. Either one is php's answer here. */`` |
|      - |  6801 | `	{` |
|      - |  6802 | `		SyString sAttr;` |
|      - |  6803 | `		ph7_value *pBound;` |
|     15 |  6804 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     15 |  6805 | `		pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     15 |  6806 | `		if( (pBound == 0 \|\| (pBound->iFlags & MEMOBJ_OBJ) == 0) && sRef.pFunc ){` |
|     11 |  6807 | `			ph7_vm_func_closure_env *aEnv =` |
|     10 |  6808 | `				(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|      - |  6809 | `			sxu32 n;` |
|     11 |  6810 | `			pBound = 0;` |
|     15 |  6811 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++n ){` |
|     12 |  6812 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|     11 |  6813 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|      9 |  6814 | `					pBound = &aEnv[n].sValue;` |
|      9 |  6815 | `					break;` |
|      - |  6816 | `				}` |
|      3 |  6817 | `			}` |
|      5 |  6818 | `		}` |
|     15 |  6819 | `		if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|      5 |  6820 | `			ClosurePresentAdd(pVm, pOut, "this", pBound);` |
|      2 |  6821 | `		}` |
|      - |  6822 | `	}` |
|      - |  6823 | ``	/* `parameter`: php's cut is required_num_args, which runs through the LAST`` |
|      - |  6824 | `	 * required parameter rather than stopping at the first optional one. */` |
|      - |  6825 | `	{` |
|      - |  6826 | `		ReflectParamDesc sDesc;` |
|     15 |  6827 | `		int nArgTotal = 0, nRequired = 0, i;` |
|     31 |  6828 | `		while( ReflectParamAt(&sRef, nArgTotal, &sDesc) ){` |
|     17 |  6829 | `			if( !sDesc.bOptional ){` |
|     11 |  6830 | `				nRequired = nArgTotal + 1;` |
|      5 |  6831 | `			}` |
|     17 |  6832 | `			nArgTotal++;` |
|      1 |  6833 | `		}` |
|     15 |  6834 | `		if( nArgTotal > 0 ){` |
|      9 |  6835 | `			ph7_hashmap *pHm = PH7_NewHashmap(pVm, 0, 0);` |
|      9 |  6836 | `			if( pHm ){` |
|      - |  6837 | `				ph7_value sMap;` |
|      9 |  6838 | `				ph7_value *pMap = &sMap;` |
|      9 |  6839 | `				PH7_MemObjInitFromArray(pVm, pMap, pHm);` |
|     25 |  6840 | `				for( i = 0 ; i < nArgTotal ; ++i ){` |
|      - |  6841 | `					ph7_value sKey, sWhat;` |
|     17 |  6842 | `					if( !ReflectParamAt(&sRef, i, &sDesc) ){` |
|    ! 0 |  6843 | `						break;` |
|      - |  6844 | `					}` |
|     17 |  6845 | `					PH7_MemObjInitFromString(pVm, &sKey, 0);` |
|     17 |  6846 | `					if( sDesc.bByRef ){` |
|      3 |  6847 | `						PH7_MemObjStringAppend(&sKey, "&", 1);` |
|      1 |  6848 | `					}` |
|     17 |  6849 | `					PH7_MemObjStringAppend(&sKey, "$", 1);` |
|     25 |  6850 | `					PH7_MemObjStringAppend(&sKey, SyStringData(&sDesc.sName),` |
|      8 |  6851 | `						SyStringLength(&sDesc.sName));` |
|     17 |  6852 | `					PH7_MemObjInitFromString(pVm, &sWhat, 0);` |
|     17 |  6853 | `					PH7_MemObjStringAppend(&sWhat,` |
|      8 |  6854 | `						i >= nRequired ? "<optional>" : "<required>",` |
|      8 |  6855 | `						i >= nRequired ? sizeof("<optional>")-1 : sizeof("<required>")-1);` |
|     17 |  6856 | `					ph7_array_add_elem(pMap, &sKey, &sWhat);` |
|     17 |  6857 | `					PH7_MemObjRelease(&sKey);` |
|     17 |  6858 | `					PH7_MemObjRelease(&sWhat);` |
|      9 |  6859 | `				}` |
|      9 |  6860 | `				ClosurePresentAdd(pVm, pOut, "parameter", pMap);` |
|      9 |  6861 | `				PH7_MemObjRelease(pMap);` |
|      4 |  6862 | `			}` |
|      4 |  6863 | `		}` |
|      - |  6864 | `	}` |
|     15 |  6865 | `	return SXRET_OK;` |
|     12 |  6866 | `}` |
|      - |  6867 | `/*` |
|      - |  6868 | ` * A METHOD belongs to the extension its DECLARING class does -- php reports SPL` |
|      - |  6869 | ` * for a method a userland subclass inherited from ArrayObject -- and a plain` |
|      - |  6870 | ` * function to the one the partition places its own name in.` |
|      - |  6871 | ` */` |
|   4605 |  6872 | `static int ReflectFuncExtId(const ReflectFuncRef *pRef)` |
|      4 |  6873 | `{` |
|      - |  6874 | `	const SyString *pName;` |
|   4609 |  6875 | `	if( !ReflectFuncIsInternal(pRef) \|\| pRef->bFabricated ){` |
|      - |  6876 | `		/* A fabricated method belongs to no module: php answers false for` |
|      - |  6877 | `		 * getExtensionName() and NULL for getExtension(), and prints a bare` |
|      - |  6878 | ``		 * `<internal>` in the export. */`` |
|    116 |  6879 | `		return -1;` |
|      - |  6880 | `	}` |
|   4495 |  6881 | `	if( pRef->pMeth ){` |
|   3947 |  6882 | `		return ReflectClassExtId(ReflectFuncDeclClass(pRef));` |
|      - |  6883 | `	}` |
|    551 |  6884 | `	pName = pRef->pHost ? &pRef->pHost->sName : &pRef->pFunc->sName;` |
|    551 |  6885 | `	return PH7_VmExtOfFunc(SyStringData(pName), (int)SyStringLength(pName));` |
|   2306 |  6886 | `}` |
|     45 |  6887 | `static int vm_builtin_ReflectionFunc_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  6888 | `{` |
|      - |  6889 | `	ReflectFuncRef sRef;` |
|      - |  6890 | `	int iExt;` |
|     22 |  6891 | `	SXUNUSED(nArg);` |
|     22 |  6892 | `	SXUNUSED(apArg);` |
|     48 |  6893 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     48 |  6894 | `	iExt = ReflectFuncExtId(&sRef);` |
|     48 |  6895 | `	if( iExt < 0 ){` |
|      6 |  6896 | `		ph7_result_bool(pCtx, 0);` |
|      4 |  6897 | `	}else{` |
|     43 |  6898 | `		ph7_result_string(pCtx, PH7_VmExtensionName(iExt), -1);` |
|      - |  6899 | `	}` |
|     48 |  6900 | `	return PH7_OK;` |
|     25 |  6901 | `}` |
|     10 |  6902 | `static int vm_builtin_ReflectionFunc_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6903 | `{` |
|      - |  6904 | `	ReflectFuncRef sRef;` |
|      5 |  6905 | `	SXUNUSED(nArg);` |
|      5 |  6906 | `	SXUNUSED(apArg);` |
|     12 |  6907 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|     12 |  6908 | `	return ReflectExtensionOf(pCtx, ReflectFuncExtId(&sRef));` |
|      7 |  6909 | `}` |
|     84 |  6910 | `static int vm_builtin_ReflectionFunc_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  6911 | `{` |
|     88 |  6912 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  6913 | `	ReflectFuncRef sRef;` |
|      - |  6914 | `	int rc;` |
|     88 |  6915 | `	if( pThis == 0 \|\| !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pFunc == 0 ){` |
|    ! 0 |  6916 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  6917 | `		return PH7_OK;` |
|      - |  6918 | `	}` |
|     88 |  6919 | `	if( sRef.pMeth ){` |
|      - |  6920 | `		/* 4 = Attribute::TARGET_METHOD */` |
|      - |  6921 | `		ph7_value sTarget;` |
|      - |  6922 | `		const char *zClass, *zName;` |
|      - |  6923 | `		int nClass, nName;` |
|     71 |  6924 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     71 |  6925 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     71 |  6926 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     71 |  6927 | `		ph7_value_string(&sTarget, zClass, nClass);` |
|    105 |  6928 | `		rc = ReflectBuildAttrs(pCtx, &sRef.pFunc->aAttrs, "method", &sTarget,` |
|     34 |  6929 | `			zName, nName, 0, 4, nArg, apArg);` |
|     71 |  6930 | `		PH7_MemObjRelease(&sTarget);` |
|     71 |  6931 | `		return rc;` |
|      - |  6932 | `	}` |
|      - |  6933 | `	{` |
|      - |  6934 | `		/* 2 = Attribute::TARGET_FUNCTION */` |
|      - |  6935 | `		ph7_value sTarget;` |
|     19 |  6936 | `		ReflectFuncParamSpec(pCtx, &sTarget);` |
|     27 |  6937 | `		rc = ReflectBuildAttrs(pCtx, &sRef.pFunc->aAttrs, "fn", &sTarget, 0, 0, 0, 2,` |
|      8 |  6938 | `			nArg, apArg);` |
|     19 |  6939 | `		if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|     15 |  6940 | `			PH7_MemObjRelease(&sTarget);` |
|      6 |  6941 | `		}` |
|     19 |  6942 | `		return rc;` |
|      - |  6943 | `	}` |
|     46 |  6944 | `}` |
|      - |  6945 | `/* __toString(): php's export format, still chunk 9. */` |
|    512 |  6946 | `static int vm_builtin_ReflectionFunc_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  6947 | `{` |
|    256 |  6948 | `	SXUNUSED(nArg);` |
|    256 |  6949 | `	SXUNUSED(apArg);` |
|    516 |  6950 | `	return ReflectExportFuncSelf(pCtx);` |
|      4 |  6951 | `}` |
|      - |  6952 | `/* ---- ReflectionFunction ---- */` |
|      - |  6953 | `/* ReflectionFunction::__construct(Closure\|string $function) */` |
|    948 |  6954 | `static int vm_builtin_ReflectionFunction_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  6955 | `{` |
|    953 |  6956 | `	ph7_vm *pVm = pCtx->pVm;` |
|    953 |  6957 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  6958 | `	ReflectFuncRef sRef;` |
|      - |  6959 | `	ph7_class_instance *pClo;` |
|    953 |  6960 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 |  6961 | `		return PH7_OK;` |
|      - |  6962 | `	}` |
|    953 |  6963 | `	pClo = ReflectValueClosure(pVm, apArg[0]);` |
|    953 |  6964 | `	if( !ReflectFuncFill(pCtx->pVm, apArg[0], 0, &sRef) ){` |
|      - |  6965 | `		const char *zName;` |
|      - |  6966 | `		int nName;` |
|     25 |  6967 | `		if( pClo ){` |
|      - |  6968 | `			/* A Closure whose body no table holds -- php's magic-method TRAMPOLINE` |
|      - |  6969 | `` 			 * (`Closure::fromCallable([$o, 'zz'])` where the class reaches `zz` `` |
|      - |  6970 | `			 * only through __call) is the shape real libraries hit. php still` |
|      - |  6971 | `			 * NAMES it: the name is the one that was asked for, which the Closure` |
|      - |  6972 | `` 			 * carries in $__fn, and the scope is $__scope. Leaving `name` `` |
|      - |  6973 | `			 * uninitialized made every read of it a typed-property Error, and` |
|      - |  6974 | ``			 * `str_contains($r->name, '{closure')` is one line of twig's filter`` |
|      - |  6975 | `			 * compiler. The rest of the accessors still answer emptily: there is` |
|      - |  6976 | `			 * no body to describe, which is php's answer too (0 parameters, no` |
|      - |  6977 | `			 * file, no line). */` |
|      - |  6978 | `			SyString sAttr;` |
|      - |  6979 | `			ph7_value *pFn;` |
|      7 |  6980 | `			PH7_NativeSetAttrObj(pVm, pThis, RF_CL, pClo);` |
|      7 |  6981 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|      7 |  6982 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|      7 |  6983 | `			if( pFn && (pFn->iFlags & MEMOBJ_STRING) && SyBlobLength(&pFn->sBlob) > 0 ){` |
|     10 |  6984 | `				PH7_NativeSetAttrStr(pVm, pThis, "name",` |
|      6 |  6985 | `					(const char *)SyBlobData(&pFn->sBlob), (int)SyBlobLength(&pFn->sBlob));` |
|      3 |  6986 | `			}` |
|      7 |  6987 | `			return PH7_OK;` |
|      - |  6988 | `		}` |
|     19 |  6989 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|     27 |  6990 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      8 |  6991 | `			"Function %.*s() does not exist", nName, zName);` |
|      - |  6992 | `	}` |
|    931 |  6993 | `	if( pClo ){` |
|     79 |  6994 | `		PH7_NativeSetAttrObj(pVm, pThis, RF_CL, pClo);` |
|     38 |  6995 | `	}` |
|    931 |  6996 | `	if( sRef.pFunc ){` |
|    295 |  6997 | `		int bAnon = (sRef.pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|    290 |  6998 | `		if( !bAnon && SyStringLength(&sRef.pFunc->sName) > 9` |
|    161 |  6999 | `		 && (SyMemcmp(SyStringData(&sRef.pFunc->sName), "[lambda_", 8) == 0` |
|     61 |  7000 | `		  \|\| SyMemcmp(SyStringData(&sRef.pFunc->sName), "[closure_", 9) == 0) ){` |
|      3 |  7001 | `			bAnon = 1;` |
|      1 |  7002 | `		}` |
|    295 |  7003 | `		if( bAnon ){` |
|      - |  7004 | ``			/* php 8.4 names an anonymous function `{closure:SCOPE:LINE}`, where SCOPE is`` |
|      - |  7005 | `			 * the enclosing function and only the FILE at top level — the compiler` |
|      - |  7006 | `			 * recorded it (sClosureName); the file form is the fallback for a closure` |
|      - |  7007 | `			 * built without one. */` |
|      - |  7008 | `			char zBuf[512];` |
|     45 |  7009 | `			const char *zShow = SyStringData(&sRef.pFunc->sClosureName);` |
|     45 |  7010 | `			int nShow = (int)SyStringLength(&sRef.pFunc->sClosureName);` |
|     45 |  7011 | `			if( nShow < 1 ){` |
|    ! 0 |  7012 | `				const char *zFile = SyStringLength(&sRef.pFunc->sFile) > 0` |
|    ! 0 |  7013 | `					? SyStringData(&sRef.pFunc->sFile) : "";` |
|    ! 0 |  7014 | `				int nFile = (int)SyStringLength(&sRef.pFunc->sFile);` |
|    ! 0 |  7015 | `				nShow = SyBufferFormat(zBuf, sizeof(zBuf), "{closure:%.*s:%u}",` |
|    ! 0 |  7016 | `					nFile, zFile, sRef.pFunc->nLine);` |
|    ! 0 |  7017 | `				zShow = zBuf;` |
|    ! 0 |  7018 | `			}` |
|     45 |  7019 | `			PH7_NativeSetAttrStr(pVm, pThis, "name", zShow, nShow);` |
|     45 |  7020 | `			if( pClo == 0 ){` |
|      - |  7021 | `				/* Named by its lambda name: keep a Closure so the accessors can` |
|      - |  7022 | `				 * still reach the captured scope. */` |
|    ! 0 |  7023 | `				PH7_NativeSetAttrObj(pVm, pThis, RF_CL,` |
|    ! 0 |  7024 | `					PH7_VmNewClosure(pVm, &sRef.pFunc->sName, 0, 0));` |
|    ! 0 |  7025 | `			}` |
|     45 |  7026 | `			return PH7_OK;` |
|      - |  7027 | `		}` |
|    377 |  7028 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sRef.pFunc->sName),` |
|    248 |  7029 | `			(int)SyStringLength(&sRef.pFunc->sName));` |
|    253 |  7030 | `		return PH7_OK;` |
|      - |  7031 | `	}` |
|    957 |  7032 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sRef.pHost->sName),` |
|    636 |  7033 | `		(int)SyStringLength(&sRef.pHost->sName));` |
|    641 |  7034 | `	return PH7_OK;` |
|    477 |  7035 | `}` |
|      6 |  7036 | `static int vm_builtin_ReflectionFunction_isAnonymous(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7037 | `{` |
|      7 |  7038 | `	return vm_builtin_ReflectionFunc_isClosure(pCtx, nArg, apArg);` |
|      1 |  7039 | `}` |
|    ! 0 |  7040 | `static int vm_builtin_ReflectionFunction_isDisabled(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  7041 | `{` |
|    ! 0 |  7042 | `	SXUNUSED(nArg);` |
|    ! 0 |  7043 | `	SXUNUSED(apArg);` |
|    ! 0 |  7044 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 |  7045 | `	return PH7_OK;` |
|    ! 0 |  7046 | `}` |
|      - |  7047 | `/* The callable a ReflectionFunction invokes: the Closure it holds, or its name. */` |
|     28 |  7048 | `static void ReflectFunctionCallable(ph7_context *pCtx, ph7_value *pOut)` |
|      3 |  7049 | `{` |
|     31 |  7050 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7051 | `	ph7_class_instance *pClo;` |
|     31 |  7052 | `	const char *zName = "";` |
|     31 |  7053 | `	int nName = 0;` |
|     31 |  7054 | `	PH7_MemObjInit(pCtx->pVm, pOut);` |
|     31 |  7055 | `	if( pThis == 0 ){` |
|    ! 0 |  7056 | `		return;` |
|      - |  7057 | `	}` |
|     31 |  7058 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|     31 |  7059 | `	if( pClo ){` |
|      9 |  7060 | `		pOut->x.pOther = pClo;` |
|      9 |  7061 | `		pOut->iFlags = MEMOBJ_OBJ;` |
|      9 |  7062 | `		return;` |
|      - |  7063 | `	}` |
|     23 |  7064 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     23 |  7065 | `	ph7_value_string(pOut, zName, nName);` |
|     17 |  7066 | `}` |
|     28 |  7067 | `static int ReflectFunctionInvoke(ph7_context *pCtx, int nCall, ph7_value **apCall)` |
|      3 |  7068 | `{` |
|      - |  7069 | `	ph7_value sTarget, sResult;` |
|      - |  7070 | `	sxi32 rc;` |
|     31 |  7071 | `	ReflectFunctionCallable(pCtx, &sTarget);` |
|     31 |  7072 | `	PH7_MemObjInit(pCtx->pVm, &sResult);` |
|     31 |  7073 | `	sResult.nIdx = SXU32_HIGH;` |
|     31 |  7074 | `	rc = PH7_VmCallUserFunction(pCtx->pVm, &sTarget, nCall, apCall, &sResult);` |
|     31 |  7075 | `	if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|     23 |  7076 | `		PH7_MemObjRelease(&sTarget);` |
|     10 |  7077 | `	}` |
|     31 |  7078 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|      8 |  7079 | `		PH7_MemObjRelease(&sResult);` |
|      8 |  7080 | `		return rc;` |
|      - |  7081 | `	}` |
|     24 |  7082 | `	ph7_result_value(pCtx, &sResult);` |
|     24 |  7083 | `	PH7_MemObjRelease(&sResult);` |
|     24 |  7084 | `	return PH7_OK;` |
|     17 |  7085 | `}` |
|     22 |  7086 | `static int vm_builtin_ReflectionFunction_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  7087 | `{` |
|     25 |  7088 | `	return ReflectFunctionInvoke(pCtx, nArg, apArg);` |
|      3 |  7089 | `}` |
|      6 |  7090 | `static int vm_builtin_ReflectionFunction_invokeArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  7091 | `{` |
|      - |  7092 | `	SySet aCall;` |
|      - |  7093 | `	int rc;` |
|      9 |  7094 | `	SySetInit(&aCall, &pCtx->pVm->sAllocator, sizeof(ph7_value *));` |
|      9 |  7095 | `	if( nArg > 0 ){` |
|      9 |  7096 | `		ReflectCollectArgs(pCtx, apArg[0], &aCall, 0);` |
|      3 |  7097 | `	}` |
|      9 |  7098 | `	rc = ReflectFunctionInvoke(pCtx, (int)SySetUsed(&aCall), (ph7_value **)SySetBasePtr(&aCall));` |
|      9 |  7099 | `	SySetRelease(&aCall);` |
|      9 |  7100 | `	return rc;` |
|      3 |  7101 | `}` |
|      8 |  7102 | `static int vm_builtin_ReflectionFunction_getClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7103 | `{` |
|     10 |  7104 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7105 | `	ph7_class_instance *pClo;` |
|     10 |  7106 | `	const char *zName = "";` |
|     10 |  7107 | `	int nName = 0;` |
|      - |  7108 | `	SyString sName;` |
|      4 |  7109 | `	SXUNUSED(nArg);` |
|      4 |  7110 | `	SXUNUSED(apArg);` |
|     10 |  7111 | `	if( pThis == 0 ){` |
|    ! 0 |  7112 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7113 | `		return PH7_OK;` |
|      - |  7114 | `	}` |
|     10 |  7115 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|     10 |  7116 | `	if( pClo ){` |
|      - |  7117 | `		/* Already a Closure: hand the same instance back */` |
|      3 |  7118 | `		return ReflectResultExistingObject(pCtx, pClo);` |
|      - |  7119 | `	}` |
|      8 |  7120 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      8 |  7121 | `	SyStringInitFromBuf(&sName, zName, nName);` |
|      8 |  7122 | `	return ReflectResultObject(pCtx, PH7_VmNewClosure(pCtx->pVm, &sName, 0, 0));` |
|      6 |  7123 | `}` |
|      - |  7124 | `/* ---- ReflectionMethod ---- */` |
|      - |  7125 | `/* ReflectionMethod::__construct(object\|string $objectOrMethod, ?string $method = null) */` |
|   1166 |  7126 | `static int vm_builtin_ReflectionMethod_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  7127 | `{` |
|   1171 |  7128 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1171 |  7129 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7130 | `	ph7_class *pClass;` |
|      - |  7131 | `	SyHashEntry *pEntry;` |
|      - |  7132 | `	ph7_value sClass, sMethod;` |
|      - |  7133 | `	const char *zMethod;` |
|   1171 |  7134 | `	int nMethod, rc = PH7_OK;` |
|   1171 |  7135 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 |  7136 | `		return PH7_OK;` |
|      - |  7137 | `	}` |
|   1171 |  7138 | `	PH7_MemObjInit(pVm, &sClass);` |
|   1171 |  7139 | `	PH7_MemObjInit(pVm, &sMethod);` |
|   1172 |  7140 | `	if( nArg < 2 \|\| ph7_value_is_null(apArg[1]) ){` |
|      - |  7141 | `		/* One-argument form: "Class::method" */` |
|      - |  7142 | `		const char *zSpec;` |
|      3 |  7143 | `		int nSpec, iSep = -1, k;` |
|      3 |  7144 | `		if( (apArg[0]->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  7145 | `			PH7_MemObjRelease(&sClass);` |
|    ! 0 |  7146 | `			PH7_MemObjRelease(&sMethod);` |
|    ! 0 |  7147 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  7148 | `				"The parameter class is expected to be either a string or an object");` |
|      - |  7149 | `		}` |
|      3 |  7150 | `		zSpec = ph7_value_to_string(apArg[0], &nSpec);` |
|     19 |  7151 | `		for( k = 0 ; k + 1 < nSpec ; k++ ){` |
|     19 |  7152 | `			if( zSpec[k] == ':' && zSpec[k+1] == ':' ){` |
|      3 |  7153 | `				iSep = k;` |
|      3 |  7154 | `				break;` |
|      - |  7155 | `			}` |
|      9 |  7156 | `		}` |
|      3 |  7157 | `		if( iSep < 0 ){` |
|    ! 0 |  7158 | `			PH7_MemObjRelease(&sClass);` |
|    ! 0 |  7159 | `			PH7_MemObjRelease(&sMethod);` |
|    ! 0 |  7160 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 |  7161 | `				"Invalid method name %.*s", nSpec, zSpec);` |
|      - |  7162 | `		}` |
|      - |  7163 | `		/* php 8.3 deprecated the one-argument spelling in favour of the static` |
|      - |  7164 | `		 * factory. createFromMethodName() splits the name ITSELF and constructs` |
|      - |  7165 | `		 * with two, so it does not trip this. */` |
|      3 |  7166 | `		PH7_VmThrowError(pVm, 0, E_DEPRECATED,` |
|      - |  7167 | `			"Calling ReflectionMethod::__construct() with 1 argument is deprecated, "` |
|      - |  7168 | `			"use ReflectionMethod::createFromMethodName() instead");` |
|      3 |  7169 | `		ph7_value_string(&sClass, zSpec, iSep);` |
|      3 |  7170 | `		ph7_value_string(&sMethod, &zSpec[iSep+2], nSpec - iSep - 2);` |
|      2 |  7171 | `	}else{` |
|   1169 |  7172 | `		PH7_MemObjStore(apArg[0], &sClass);` |
|   1169 |  7173 | `		PH7_MemObjStore(apArg[1], &sMethod);` |
|      - |  7174 | `	}` |
|   1171 |  7175 | `	pClass = ReflectResolveClass(pVm, &sClass);` |
|   1171 |  7176 | `	if( pClass == 0 ){` |
|      - |  7177 | `		const char *zName;` |
|      - |  7178 | `		int nName;` |
|      3 |  7179 | `		zName = ph7_value_to_string(&sClass, &nName);` |
|      4 |  7180 | `		rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  7181 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      3 |  7182 | `		goto Done;` |
|      - |  7183 | `	}` |
|   1169 |  7184 | `	zMethod = ph7_value_to_string(&sMethod, &nMethod);` |
|   1169 |  7185 | `	pEntry = ReflectFindMethodEntry(pClass, zMethod, nMethod);` |
|   1169 |  7186 | `	if( pEntry == 0 ){` |
|     10 |  7187 | `		rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 |  7188 | `			"Method %z::%.*s() does not exist", &pClass->sDisp, nMethod, zMethod);` |
|      7 |  7189 | `		goto Done;` |
|      - |  7190 | `	}` |
|   1163 |  7191 | `	if( ((ph7_class_method *)pEntry->pUserData)->iFlags & PH7_CLASS_ATTR_FABRICATED ){` |
|      - |  7192 | `		/* php has no such row in the class's function table, so a CONSTRUCTOR asked` |
|      - |  7193 | ``		 * for it by class name finds nothing: `new ReflectionMethod('Closure',`` |
|      - |  7194 | ``		 * '__invoke')` is a ReflectionException even though the method answers`` |
|      - |  7195 | `		 * everywhere else. Given the OBJECT, php builds it -- and so does a` |
|      - |  7196 | `		 * ReflectionClass door, which is the nReflectFactory window. */` |
|     21 |  7197 | `		if( (sClass.iFlags & MEMOBJ_OBJ) == 0 && pVm->nReflectFactory < 1 ){` |
|      4 |  7198 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  7199 | `				"Method %z::%.*s() does not exist", &pClass->sDisp, nMethod, zMethod);` |
|      3 |  7200 | `			goto Done;` |
|      - |  7201 | `		}` |
|     19 |  7202 | `		if( sClass.iFlags & MEMOBJ_OBJ ){` |
|      - |  7203 | `			/* ...and what it builds describes THAT closure: park the instance where` |
|      - |  7204 | `			 * ReflectFuncOfThis already looks for a reflected Closure, so the` |
|      - |  7205 | `			 * parameter list, the return type and invoke() are the closure's own. */` |
|     15 |  7206 | `			PH7_NativeSetAttrObj(pVm, pThis, RF_CL, (ph7_class_instance *)sClass.x.pOther);` |
|      7 |  7207 | `		}` |
|      9 |  7208 | `	}` |
|      - |  7209 | `	{` |
|      - |  7210 | `		/* php's $class is the DECLARING class, not the one the lookup went` |
|      - |  7211 | ``		 * through: `new ReflectionMethod('Kid','bm')` on an inherited method`` |
|      - |  7212 | `		 * reports Base. */` |
|   1161 |  7213 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, (ph7_class_method *)pEntry->pUserData);` |
|   1739 |  7214 | `		PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pDecl->sName),` |
|   1156 |  7215 | `			(int)SyStringLength(&pDecl->sName));` |
|      - |  7216 | `	}` |
|      - |  7217 | `	/* The class the reflector was built FOR, which the export tags read. Every` |
|      - |  7218 | `	 * ReflectionClass door that hands a method out passes its OWN name as the` |
|      - |  7219 | `	 * first constructor argument, so they all land here too. */` |
|   1739 |  7220 | `	PH7_NativeSetAttrStr(pVm, pThis, RM_CE, SyStringData(&pClass->sName),` |
|   1156 |  7221 | `		(int)SyStringLength(&pClass->sName));` |
|      - |  7222 | `	/* The DECLARED spelling, whatever case was asked for. */` |
|   1161 |  7223 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", (const char *)pEntry->pKey, (int)pEntry->nKeyLen);` |
|    583 |  7224 | `Done:` |
|   1171 |  7225 | `	PH7_MemObjRelease(&sClass);` |
|   1171 |  7226 | `	PH7_MemObjRelease(&sMethod);` |
|   1171 |  7227 | `	return rc;` |
|    588 |  7228 | `}` |
|      - |  7229 | `/* ReflectionMethod::createFromMethodName(string $method): static */` |
|      4 |  7230 | `static int vm_builtin_ReflectionMethod_createFromMethodName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7231 | `{` |
|      - |  7232 | `	ph7_class_instance *pOut;` |
|      - |  7233 | `	ph7_value sClass, sMethod;` |
|      - |  7234 | `	ph7_value *apCtor[2];` |
|      - |  7235 | `	const char *zSpec;` |
|      5 |  7236 | `	int nSpec, iSep = -1, k;` |
|      - |  7237 | `	sxi32 rc;` |
|      5 |  7238 | `	if( nArg < 1 ){` |
|    ! 0 |  7239 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7240 | `		return PH7_OK;` |
|      - |  7241 | `	}` |
|      - |  7242 | `	/* Split here rather than letting the constructor do it: the one-argument` |
|      - |  7243 | `	 * constructor is the DEPRECATED spelling this factory exists to replace. */` |
|      5 |  7244 | `	zSpec = ph7_value_to_string(apArg[0], &nSpec);` |
|     43 |  7245 | `	for( k = 0 ; k + 1 < nSpec ; k++ ){` |
|     43 |  7246 | `		if( zSpec[k] == ':' && zSpec[k+1] == ':' ){` |
|      5 |  7247 | `			iSep = k;` |
|      5 |  7248 | `			break;` |
|      - |  7249 | `		}` |
|     20 |  7250 | `	}` |
|      5 |  7251 | `	if( iSep < 0 ){` |
|    ! 0 |  7252 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 |  7253 | `			"Invalid method name %.*s", nSpec, zSpec);` |
|      - |  7254 | `	}` |
|      5 |  7255 | `	PH7_MemObjInit(pCtx->pVm, &sClass);` |
|      5 |  7256 | `	PH7_MemObjInit(pCtx->pVm, &sMethod);` |
|      5 |  7257 | `	ph7_value_string(&sClass, zSpec, iSep);` |
|      5 |  7258 | `	ph7_value_string(&sMethod, &zSpec[iSep+2], nSpec - iSep - 2);` |
|      5 |  7259 | `	apCtor[0] = &sClass;` |
|      5 |  7260 | `	apCtor[1] = &sMethod;` |
|      5 |  7261 | `	pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|      5 |  7262 | `	PH7_MemObjRelease(&sClass);` |
|      5 |  7263 | `	PH7_MemObjRelease(&sMethod);` |
|      5 |  7264 | `	if( pOut == 0 ){` |
|    ! 0 |  7265 | `		if( rc != PH7_OK ){` |
|    ! 0 |  7266 | `			return rc;` |
|      - |  7267 | `		}` |
|    ! 0 |  7268 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7269 | `		return PH7_OK;` |
|      - |  7270 | `	}` |
|      5 |  7271 | `	return ReflectResultObject(pCtx, pOut);` |
|      3 |  7272 | `}` |
|      - |  7273 | `/* The visibility/modifier predicates, all off the method record. */` |
|     40 |  7274 | `static int ReflectMethodFlag(ph7_context *pCtx, int iWhat)` |
|      3 |  7275 | `{` |
|      - |  7276 | `	ReflectFuncRef sRef;` |
|     43 |  7277 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|    ! 0 |  7278 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  7279 | `		return PH7_OK;` |
|      - |  7280 | `	}` |
|     43 |  7281 | `	switch( iWhat ){` |
|     15 |  7282 | `	case 0: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PUBLIC); break;` |
|     11 |  7283 | `	case 1: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PRIVATE); break;` |
|      3 |  7284 | `	case 2: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PROTECTED); break;` |
|      7 |  7285 | `	case 3: ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0); break;` |
|     11 |  7286 | `	default: ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0); break;` |
|      - |  7287 | `	}` |
|     43 |  7288 | `	return PH7_OK;` |
|     23 |  7289 | `}` |
|      - |  7290 | `#define REFLECT_METHOD_FLAG(NAME,WHAT) \` |
|      - |  7291 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  7292 | `	{ \` |
|      - |  7293 | `		SXUNUSED(nArg); \` |
|      - |  7294 | `		SXUNUSED(apArg); \` |
|      - |  7295 | `		return ReflectMethodFlag(pCtx, WHAT); \` |
|      - |  7296 | `	}` |
|     15 |  7297 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isPublic, 0)` |
|     11 |  7298 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isPrivate, 1)` |
|      3 |  7299 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isProtected, 2)` |
|      7 |  7300 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isAbstract, 3)` |
|     11 |  7301 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isFinal, 4)` |
|      - |  7302 |  |
|      - |  7303 | `/* isConstructor()/isDestructor() read the NAME, not the record: a trait` |
|      - |  7304 | `` * `use T { m as __construct; }` alias is the constructor under that key. */`` |
|      4 |  7305 | `static int ReflectMethodNameIs(ph7_context *pCtx, const char *zWant, int nWant)` |
|      2 |  7306 | `{` |
|      6 |  7307 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      6 |  7308 | `	const char *zName = "";` |
|      6 |  7309 | `	int nName = 0;` |
|      6 |  7310 | `	if( pThis ){` |
|      6 |  7311 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 |  7312 | `	}` |
|      6 |  7313 | `	ph7_result_bool(pCtx, nName == nWant && SyStrnicmp(zName, zWant, (sxu32)nWant) == 0);` |
|      6 |  7314 | `	return PH7_OK;` |
|      2 |  7315 | `}` |
|      4 |  7316 | `static int vm_builtin_ReflectionMethod_isConstructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7317 | `{` |
|      2 |  7318 | `	SXUNUSED(nArg);` |
|      2 |  7319 | `	SXUNUSED(apArg);` |
|      6 |  7320 | `	return ReflectMethodNameIs(pCtx, "__construct", sizeof("__construct")-1);` |
|      2 |  7321 | `}` |
|    ! 0 |  7322 | `static int vm_builtin_ReflectionMethod_isDestructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  7323 | `{` |
|    ! 0 |  7324 | `	SXUNUSED(nArg);` |
|    ! 0 |  7325 | `	SXUNUSED(apArg);` |
|    ! 0 |  7326 | `	return ReflectMethodNameIs(pCtx, "__destruct", sizeof("__destruct")-1);` |
|    ! 0 |  7327 | `}` |
|     12 |  7328 | `static int vm_builtin_ReflectionMethod_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7329 | `{` |
|      - |  7330 | `	ReflectFuncRef sRef;` |
|      6 |  7331 | `	SXUNUSED(nArg);` |
|      6 |  7332 | `	SXUNUSED(apArg);` |
|     14 |  7333 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|    ! 0 |  7334 | `		ph7_result_int(pCtx, 0);` |
|    ! 0 |  7335 | `		return PH7_OK;` |
|      - |  7336 | `	}` |
|     14 |  7337 | `	ph7_result_int64(pCtx, ReflectMethodModifiers(sRef.pMeth));` |
|     14 |  7338 | `	return PH7_OK;` |
|      8 |  7339 | `}` |
|    340 |  7340 | `static int vm_builtin_ReflectionMethod_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  7341 | `{` |
|      - |  7342 | `	ReflectFuncRef sRef;` |
|    170 |  7343 | `	SXUNUSED(nArg);` |
|    170 |  7344 | `	SXUNUSED(apArg);` |
|    343 |  7345 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 |  7346 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7347 | `		return PH7_OK;` |
|      - |  7348 | `	}` |
|    343 |  7349 | `	return ReflectResultClassOf(pCtx, ReflectFuncDeclClass(&sRef));` |
|    173 |  7350 | `}` |
|      - |  7351 | `/*` |
|      - |  7352 | ` * The receiver check php runs before invoking or binding: a non-static method` |
|      - |  7353 | ` * needs an instance OF ITS DECLARING CLASS; a static one ignores whatever was` |
|      - |  7354 | ` * handed over. *ppThis is cleared for a static method.` |
|      - |  7355 | ` */` |
|     50 |  7356 | `static sxi32 ReflectMethodReceiver(ph7_context *pCtx, ReflectFuncRef *pRef,` |
|      - |  7357 | `	ph7_value *pObject, ph7_class_instance **ppThis, int bClosure)` |
|      3 |  7358 | `{` |
|     53 |  7359 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     53 |  7360 | `	const char *zClass = "", *zName = "";` |
|     53 |  7361 | `	int nClass = 0, nName = 0;` |
|     53 |  7362 | `	ph7_class *pDecl = ReflectFuncDeclClass(pRef);` |
|     53 |  7363 | `	*ppThis = 0;` |
|     53 |  7364 | `	if( pThis ){` |
|     53 |  7365 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     53 |  7366 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     25 |  7367 | `	}` |
|     53 |  7368 | `	if( pRef->pMeth && (pRef->pMeth->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|     14 |  7369 | `		return PH7_OK;` |
|      - |  7370 | `	}` |
|     41 |  7371 | `	if( pObject == 0 \|\| (pObject->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      9 |  7372 | `		if( bClosure ){` |
|      5 |  7373 | `			return PH7_VmThrowException(pCtx, "ValueError",` |
|      - |  7374 | `				"ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods");` |
|      - |  7375 | `		}` |
|      7 |  7376 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  7377 | `			"Trying to invoke non static method %.*s::%.*s() without an object",` |
|      2 |  7378 | `			nClass, zClass, nName, zName);` |
|      - |  7379 | `	}` |
|     33 |  7380 | `	*ppThis = (ph7_class_instance *)pObject->x.pOther;` |
|     33 |  7381 | `	if( pDecl && !PH7_VmInstanceOf((*ppThis)->pClass, pDecl) ){` |
|      3 |  7382 | `		*ppThis = 0;` |
|      3 |  7383 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  7384 | `			"Given object is not an instance of the class this method was declared in");` |
|      - |  7385 | `	}` |
|     31 |  7386 | `	return PH7_OK;` |
|     28 |  7387 | `}` |
|     32 |  7388 | `static int ReflectMethodInvoke(ph7_context *pCtx, ph7_value *pObject, int nCall, ph7_value **apCall)` |
|      3 |  7389 | `{` |
|     35 |  7390 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  7391 | `	ReflectFuncRef sRef;` |
|     35 |  7392 | `	ph7_class_instance *pRecv = 0;` |
|      - |  7393 | `	ph7_value sResult;` |
|      - |  7394 | `	sxi32 rc;` |
|     35 |  7395 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|    ! 0 |  7396 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7397 | `		return PH7_OK;` |
|      - |  7398 | `	}` |
|     35 |  7399 | `	rc = ReflectMethodReceiver(pCtx, &sRef, pObject, &pRecv, 0);` |
|     35 |  7400 | `	if( rc != PH7_OK ){` |
|      7 |  7401 | `		return rc;` |
|      - |  7402 | `	}` |
|     29 |  7403 | `	PH7_MemObjInit(pVm, &sResult);` |
|     29 |  7404 | `	sResult.nIdx = SXU32_HIGH;` |
|      - |  7405 | `	/* Reflection ignores method visibility (PHP 8.1+); the flag is consumed by` |
|      - |  7406 | `	 * the first OP_CALL, i.e. this synthetic one. */` |
|     29 |  7407 | `	pVm->bReflectBypass = 1;` |
|      - |  7408 | `	/* And it binds the arguments WEAKLY however strict the file that called` |
|      - |  7409 | `	 * invoke() is: php reads strict mode off the frame that MADE the call, and` |
|      - |  7410 | `	 * that frame is ReflectionMethod::invoke itself, an internal function with no` |
|      - |  7411 | `	 * strict_types of its own. PHPUnit's mock builder reaches every original` |
|      - |  7412 | `	 * constructor this way (Generator::instantiate -> getConstructor()->invokeArgs),` |
|      - |  7413 | `	 * from a file that declares strict_types=1. */` |
|     29 |  7414 | `	pVm->bCallbackWeak = 1;` |
|     29 |  7415 | `	rc = PH7_VmCallClassMethod(pVm, pRecv, sRef.pMeth, &sResult, nCall, apCall);` |
|     29 |  7416 | `	pVm->bCallbackWeak = 0; /* clear if the dispatch never reached an OP_CALL */` |
|     29 |  7417 | `	pVm->bReflectBypass = 0;` |
|     29 |  7418 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|    ! 0 |  7419 | `		PH7_MemObjRelease(&sResult);` |
|    ! 0 |  7420 | `		return rc;` |
|      - |  7421 | `	}` |
|     29 |  7422 | `	ph7_result_value(pCtx, &sResult);` |
|     29 |  7423 | `	PH7_MemObjRelease(&sResult);` |
|     29 |  7424 | `	return PH7_OK;` |
|     19 |  7425 | `}` |
|     26 |  7426 | `static int vm_builtin_ReflectionMethod_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  7427 | `{` |
|     55 |  7428 | `	return ReflectMethodInvoke(pCtx, nArg > 0 ? apArg[0] : 0,` |
|     26 |  7429 | `		nArg > 1 ? nArg - 1 : 0, nArg > 1 ? &apArg[1] : 0);` |
|      3 |  7430 | `}` |
|      6 |  7431 | `static int vm_builtin_ReflectionMethod_invokeArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7432 | `{` |
|      - |  7433 | `	SySet aCall;` |
|      - |  7434 | `	int rc;` |
|      8 |  7435 | `	SySetInit(&aCall, &pCtx->pVm->sAllocator, sizeof(ph7_value *));` |
|      8 |  7436 | `	if( nArg > 1 ){` |
|      8 |  7437 | `		ReflectCollectArgs(pCtx, apArg[1], &aCall, 0);` |
|      3 |  7438 | `	}` |
|      5 |  7439 | `	rc = ReflectMethodInvoke(pCtx, nArg > 0 ? apArg[0] : 0,` |
|      6 |  7440 | `		(int)SySetUsed(&aCall), (ph7_value **)SySetBasePtr(&aCall));` |
|      8 |  7441 | `	SySetRelease(&aCall);` |
|      8 |  7442 | `	return rc;` |
|      2 |  7443 | `}` |
|     18 |  7444 | `static int vm_builtin_ReflectionMethod_getClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7445 | `{` |
|      - |  7446 | `	ReflectFuncRef sRef;` |
|     20 |  7447 | `	ph7_class_instance *pRecv = 0;` |
|      - |  7448 | `	sxi32 rc;` |
|     20 |  7449 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 \|\| sRef.pClass == 0 ){` |
|    ! 0 |  7450 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7451 | `		return PH7_OK;` |
|      - |  7452 | `	}` |
|     20 |  7453 | `	rc = ReflectMethodReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pRecv, 1);` |
|     20 |  7454 | `	if( rc != PH7_OK ){` |
|      5 |  7455 | `		return rc;` |
|      - |  7456 | `	}` |
|      - |  7457 | `	{` |
|      - |  7458 | `		/* php hands back a closure over the RESOLVED method and reflection is allowed past` |
|      - |  7459 | `		 * protection (8.1+), so this one must dispatch without a second visibility decision —` |
|      - |  7460 | `		 * the FCC_METHOD mark is what tells the unwrap its $__fn is a screened METHOD name. */` |
|     23 |  7461 | `		ph7_class_instance *pClo = PH7_VmNewClosure(pCtx->pVm, &sRef.pMeth->sFunc.sName,` |
|     14 |  7462 | `			pRecv, &sRef.pClass->sName);` |
|     16 |  7463 | `		if( pClo ){` |
|     16 |  7464 | `			pClo->iFlags \|= VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SCREENED;` |
|      7 |  7465 | `		}` |
|     16 |  7466 | `		return ReflectResultObject(pCtx, pClo);` |
|      - |  7467 | `	}` |
|     11 |  7468 | `}` |
|      - |  7469 | `/* php 8.1 made every reflected member accessible; the setter is a no-op it kept. */` |
|    ! 0 |  7470 | `static int vm_builtin_ReflectionMethod_setAccessible(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  7471 | `{` |
|    ! 0 |  7472 | `	SXUNUSED(pCtx);` |
|    ! 0 |  7473 | `	SXUNUSED(nArg);` |
|    ! 0 |  7474 | `	SXUNUSED(apArg);` |
|    ! 0 |  7475 | `	return PH7_OK;` |
|    ! 0 |  7476 | `}` |
|      - |  7477 | `/*` |
|      - |  7478 | `` * php's `overwrites`: the class the entry found in the PARENT's method table`` |
|      - |  7479 | ` * belongs to — which is an interface when the parent only inherited it from` |
|      - |  7480 | `` * one, so `ReflectionFunction::__toString` overwrites Stringable. A method`` |
|      - |  7481 | ` * that first appears in an interface the class itself implements is a` |
|      - |  7482 | `` * `prototype` instead, never an overwrite.`` |
|      - |  7483 | ` */` |
|   2102 |  7484 | `static ph7_class * ReflectOverwritesIn(ph7_class *pClass, const char *zName, int nName)` |
|      3 |  7485 | `{` |
|      - |  7486 | `	ph7_class *pWalk;` |
|   2105 |  7487 | `	int iDepth = 0;` |
|   2105 |  7488 | `	if( pClass == 0 \|\| nName < 1 ){` |
|    ! 0 |  7489 | `		return 0;` |
|      - |  7490 | `	}` |
|   2527 |  7491 | `	for( pWalk = pClass->pBase ; pWalk && iDepth <= REFLECT_WALK_MAX_DEPTH ; pWalk = pWalk->pBase ){` |
|    587 |  7492 | `		SyHashEntry *pEntry = ReflectFindMethodEntry(pWalk, zName, nName);` |
|    587 |  7493 | `		if( pEntry ){` |
|    165 |  7494 | `			ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    165 |  7495 | `			if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|    165 |  7496 | `				return ReflectMethodDeclClass(pWalk, pMeth);` |
|      - |  7497 | `			}` |
|    ! 0 |  7498 | `		}` |
|    423 |  7499 | `		iDepth++;` |
|    212 |  7500 | `	}` |
|   1941 |  7501 | `	return 0;` |
|   1054 |  7502 | `}` |
|      - |  7503 | `/*` |
|      - |  7504 | `` * php's `prototype`: the ROOT-most declaration the entry in pClass's method`` |
|      - |  7505 | ` * table answers to, or NULL.` |
|      - |  7506 | ` *` |
|      - |  7507 | `` * zend assigns it once, at link time, as `child->prototype = parent->prototype`` |
|      - |  7508 | `` * ? parent->prototype : parent` — so it CHAINS past every intermediate`` |
|      - |  7509 | ` * override and names the class where the contract began, not the nearest one` |
|      - |  7510 | `` * (`AppendIterator::current` is `prototype Iterator`, three classes up, where`` |
|      - |  7511 | ` * this engine answered its immediate parent). Two rules ride on it, both` |
|      - |  7512 | ` * measured against php 8.5.9:` |
|      - |  7513 | ` *` |
|      - |  7514 | ` *   - An INTERFACE wins over the parent chain. zend inherits from the parent` |
|      - |  7515 | ` *     class first and implements the interfaces after, and each implementation` |
|      - |  7516 | `` *     re-assigns the prototype — so `class C extends B implements I`, with both`` |
|      - |  7517 | ` *     declaring f(), reports I and not B.` |
|      - |  7518 | ` *   - A CONSTRUCTOR takes one only where the contract is really a contract:` |
|      - |  7519 | ` *     the chain link that would have STARTED it is dropped unless it is` |
|      - |  7520 | ` *     abstract (an interface's ctor is abstract too). An inherited prototype` |
|      - |  7521 | ` *     still rides through, so a ctor three deep from an abstract one keeps it.` |
|      - |  7522 | ` */` |
|   8404 |  7523 | `static ph7_class * ReflectPrototypeOf(ph7_context *pCtx, ph7_class *pClass,` |
|      - |  7524 | `	const char *zName, int nName, int iDepth)` |
|      3 |  7525 | `{` |
|   8407 |  7526 | `	ph7_class *pWalk, *pDecl, *pRes = 0;` |
|      - |  7527 | `	SyHashEntry *pOwn;` |
|      - |  7528 | `	int bCtor;` |
|   8407 |  7529 | `	if( pClass == 0 \|\| nName < 1 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|    ! 0 |  7530 | `		return 0;` |
|      - |  7531 | `	}` |
|   8407 |  7532 | `	pOwn = ReflectFindMethodEntry(pClass, zName, nName);` |
|   8407 |  7533 | `	if( pOwn == 0 ){` |
|    ! 0 |  7534 | `		return 0;` |
|      - |  7535 | `	}` |
|   9070 |  7536 | `	bCtor = nName == sizeof("__construct")-1` |
|   8404 |  7537 | `		&& SyStrnicmp(zName, "__construct", sizeof("__construct")-1) == 0;` |
|   8407 |  7538 | `	pDecl = ReflectMethodDeclClass(pClass, (ph7_class_method *)pOwn->pUserData);` |
|   8407 |  7539 | `	if( pDecl != pClass ){` |
|      - |  7540 | `		/* The class did not declare this one: zend copies the record in and` |
|      - |  7541 | `		 * assigns NOTHING, so whatever prototype the record already carries is` |
|      - |  7542 | ``		 * what a reflector reads here. `DOMAttr::C14N` therefore has none at`` |
|      - |  7543 | `		 * all, where this engine named the nearest declaring base. */` |
|   2430 |  7544 | `		pRes = ReflectPrototypeOf(pCtx, pDecl, zName, nName, iDepth + 1);` |
|   7193 |  7545 | `	}else if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      - |  7546 | `		/* Its own declaration, checked against the parent's at link time. An` |
|      - |  7547 | `		 * interface has no such link — its parents are declared parents, and` |
|      - |  7548 | `		 * are walked with the interface list below. */` |
|   4893 |  7549 | `		for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){` |
|    913 |  7550 | `			SyHashEntry *pEntry = ReflectFindMethodEntry(pWalk, zName, nName);` |
|      - |  7551 | `			ph7_class_method *pMeth;` |
|    913 |  7552 | `			if( pEntry == 0 ){` |
|    551 |  7553 | `				continue;` |
|      - |  7554 | `			}` |
|    363 |  7555 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    363 |  7556 | `			if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|    ! 0 |  7557 | `				break;` |
|      - |  7558 | `			}` |
|    363 |  7559 | `			pRes = ReflectPrototypeOf(pCtx, pWalk, zName, nName, iDepth + 1);` |
|    363 |  7560 | `			if( pRes == 0 && !(bCtor && (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0) ){` |
|    123 |  7561 | `				pRes = ReflectMethodDeclClass(pWalk, pMeth);` |
|     61 |  7562 | `			}` |
|    363 |  7563 | `			break;` |
|    ! 0 |  7564 | `		}` |
|   2170 |  7565 | `	}` |
|      - |  7566 | `	/* Each interface this class DECLARES re-assigns it, the last one winning —` |
|      - |  7567 | `	 * which is why an interface beats the parent chain. PHL keeps the first` |
|      - |  7568 | `	 * parent of an INTERFACE on the base chain, so it joins the list here. */` |
|      - |  7569 | `	{` |
|   8407 |  7570 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   8407 |  7571 | `		sxu32 n, nIface = SySetUsed(&pClass->aInterface);` |
|  22965 |  7572 | `		for( n = 0 ; n <= nIface ; n++ ){` |
|      - |  7573 | `			ph7_class *pIface;` |
|      - |  7574 | `			SyHashEntry *pEntry;` |
|  14561 |  7575 | `			if( n == nIface ){` |
|   8407 |  7576 | `				pIface = (pClass->iFlags & PH7_CLASS_INTERFACE) ? pClass->pBase : 0;` |
|   4205 |  7577 | `			}else{` |
|   6157 |  7578 | `				pIface = apIface[n];` |
|      - |  7579 | `			}` |
|  14561 |  7580 | `			if( pIface == 0 ){` |
|   6765 |  7581 | `				continue;` |
|      - |  7582 | `			}` |
|   7799 |  7583 | `			pEntry = ReflectFindMethodEntry(pIface, zName, nName);` |
|      - |  7584 | `			/* Nothing to check against when the class holds the interface's` |
|      - |  7585 | `			 * very own record: an interface that merely EXTENDS another copies` |
|      - |  7586 | ``			 * the method in and stops, so `interface B extends A` prints`` |
|      - |  7587 | ``			 * `inherits A` and no prototype at all. */`` |
|   7799 |  7588 | `			if( pEntry == 0 \|\| pOwn->pUserData == pEntry->pUserData ){` |
|   6367 |  7589 | `				continue;` |
|      - |  7590 | `			}` |
|   1435 |  7591 | `			pRes = ReflectPrototypeOf(pCtx, pIface, zName, nName, iDepth + 1);` |
|   1435 |  7592 | `			if( pRes == 0 ){` |
|   2151 |  7593 | `				pRes = ReflectMethodDeclClass(pIface,` |
|   1432 |  7594 | `					(ph7_class_method *)pEntry->pUserData);` |
|    716 |  7595 | `			}` |
|    719 |  7596 | `		}` |
|      - |  7597 | `	}` |
|   8407 |  7598 | `	return pRes;` |
|   4205 |  7599 | `}` |
|      - |  7600 | `/* The same question asked by a ReflectionMethod receiver, which carries the` |
|      - |  7601 | `` * method name on `$this`. The prototype belongs to the entry in the class the`` |
|      - |  7602 | ` * reflector was BUILT FOR, which is php's anchor for it too. */` |
|    146 |  7603 | `static ph7_class * ReflectMethodPrototype(ph7_context *pCtx, ReflectFuncRef *pRef)` |
|      1 |  7604 | `{` |
|    147 |  7605 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    147 |  7606 | `	ph7_class *pOwner = ReflectOwnerOfThis(pCtx);` |
|    147 |  7607 | `	const char *zName = "";` |
|    147 |  7608 | `	int nName = 0;` |
|    147 |  7609 | `	if( pThis == 0 ){` |
|    ! 0 |  7610 | `		return 0;` |
|      - |  7611 | `	}` |
|    147 |  7612 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    147 |  7613 | `	return ReflectPrototypeOf(pCtx, pOwner ? pOwner : pRef->pClass, zName, nName, 0);` |
|     74 |  7614 | `}` |
|     94 |  7615 | `static int vm_builtin_ReflectionMethod_hasPrototype(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7616 | `{` |
|      - |  7617 | `	ReflectFuncRef sRef;` |
|     47 |  7618 | `	SXUNUSED(nArg);` |
|     47 |  7619 | `	SXUNUSED(apArg);` |
|     95 |  7620 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 |  7621 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  7622 | `		return PH7_OK;` |
|      - |  7623 | `	}` |
|     95 |  7624 | `	ph7_result_bool(pCtx, ReflectMethodPrototype(pCtx, &sRef) != 0);` |
|     95 |  7625 | `	return PH7_OK;` |
|     48 |  7626 | `}` |
|     52 |  7627 | `static int vm_builtin_ReflectionMethod_getPrototype(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7628 | `{` |
|     53 |  7629 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7630 | `	ReflectFuncRef sRef;` |
|      - |  7631 | `	ph7_class *pProto;` |
|     53 |  7632 | `	const char *zClass = "", *zName = "";` |
|     53 |  7633 | `	int nClass = 0, nName = 0;` |
|     26 |  7634 | `	SXUNUSED(nArg);` |
|     26 |  7635 | `	SXUNUSED(apArg);` |
|     53 |  7636 | `	if( pThis == 0 \|\| !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 |  7637 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7638 | `		return PH7_OK;` |
|      - |  7639 | `	}` |
|     53 |  7640 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     53 |  7641 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     53 |  7642 | `	pProto = ReflectMethodPrototype(pCtx, &sRef);` |
|     53 |  7643 | `	if( pProto == 0 ){` |
|      7 |  7644 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 |  7645 | `			"Method %.*s::%.*s does not have a prototype", nClass, zClass, nName, zName);` |
|      - |  7646 | `	}` |
|      - |  7647 | `	{` |
|      - |  7648 | `		ph7_value sClass, sName;` |
|      - |  7649 | `		ph7_value *apCtor[2];` |
|      - |  7650 | `		ph7_class_instance *pOut;` |
|      - |  7651 | `		sxi32 rc;` |
|     49 |  7652 | `		PH7_MemObjInit(pCtx->pVm, &sClass);` |
|     49 |  7653 | `		PH7_MemObjInit(pCtx->pVm, &sName);` |
|     49 |  7654 | `		ph7_value_string(&sClass, SyStringData(&pProto->sName), (int)SyStringLength(&pProto->sName));` |
|     49 |  7655 | `		ph7_value_string(&sName, zName, nName);` |
|     49 |  7656 | `		apCtor[0] = &sClass;` |
|     49 |  7657 | `		apCtor[1] = &sName;` |
|     49 |  7658 | `		pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|     49 |  7659 | `		PH7_MemObjRelease(&sClass);` |
|     49 |  7660 | `		PH7_MemObjRelease(&sName);` |
|     49 |  7661 | `		if( pOut == 0 ){` |
|    ! 0 |  7662 | `			if( rc != PH7_OK ){` |
|    ! 0 |  7663 | `				return rc;` |
|      - |  7664 | `			}` |
|    ! 0 |  7665 | `			ph7_result_null(pCtx);` |
|    ! 0 |  7666 | `			return PH7_OK;` |
|      - |  7667 | `		}` |
|     49 |  7668 | `		return ReflectResultObject(pCtx, pOut);` |
|      - |  7669 | `	}` |
|     27 |  7670 | `}` |
|      - |  7671 | `/* ---- ReflectionParameter ---- */` |
|      - |  7672 | `/*` |
|      - |  7673 | ``  * The function a ReflectionParameter belongs to, from its own `__t`/`__m` `` |
|      - |  7674 | `` * slots. `__t` holds a Closure, a class name or a function name; `__m` the`` |
|      - |  7675 | ` * method name, or null.` |
|      - |  7676 | ` */` |
|   2110 |  7677 | `static int ReflectParamOwner(ph7_context *pCtx, ReflectFuncRef *pRef, ReflectParamDesc *pDesc)` |
|      5 |  7678 | `{` |
|   2115 |  7679 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7680 | `	ph7_value *pT, *pM;` |
|      - |  7681 | `	int rc;` |
|   2115 |  7682 | `	SyZero(pRef, sizeof(*pRef));` |
|   2115 |  7683 | `	if( pDesc ){` |
|   2029 |  7684 | `		SyZero(pDesc, sizeof(*pDesc));` |
|   1012 |  7685 | `	}` |
|   2115 |  7686 | `	if( pThis == 0 ){` |
|    ! 0 |  7687 | `		return 0;` |
|      - |  7688 | `	}` |
|   2115 |  7689 | `	pT = PH7_NativeAttr(pThis, RP_T);` |
|   2115 |  7690 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|   2115 |  7691 | `	if( pT == 0 ){` |
|    ! 0 |  7692 | `		return 0;` |
|      - |  7693 | `	}` |
|   2115 |  7694 | `	rc = ReflectFuncFill(pCtx->pVm, pT, (pM && (pM->iFlags & MEMOBJ_STRING)) ? pM : 0, pRef);` |
|   2115 |  7695 | `	if( rc == 0 \|\| pDesc == 0 ){` |
|     89 |  7696 | `		return rc;` |
|      - |  7697 | `	}` |
|   2029 |  7698 | `	return ReflectParamAt(pRef, (int)PH7_NativeAttrInt(pThis, RP_P), pDesc);` |
|   1060 |  7699 | `}` |
|      - |  7700 | `/* ReflectionParameter::__construct($function, string\|int $param) */` |
|   1290 |  7701 | `static int vm_builtin_ReflectionParameter_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  7702 | `{` |
|   1295 |  7703 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1295 |  7704 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7705 | `	ReflectFuncRef sRef;` |
|      - |  7706 | `	ReflectParamDesc sDesc;` |
|      - |  7707 | `	ph7_value sTarget, sMethod;` |
|   1295 |  7708 | `	int nTotal, iFound = -1, rc = PH7_OK;` |
|   1295 |  7709 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 |  7710 | `		return PH7_OK;` |
|      - |  7711 | `	}` |
|   1295 |  7712 | `	SyZero(&sDesc, sizeof(sDesc));` |
|   1295 |  7713 | `	PH7_MemObjInit(pVm, &sTarget);` |
|   1295 |  7714 | `	PH7_MemObjInit(pVm, &sMethod);` |
|      - |  7715 | ``	/* `[$objOrClass, $method]`, `"C::m"` and a plain function/Closure are the`` |
|      - |  7716 | `	 * three spellings php accepts. */` |
|   1295 |  7717 | `	if( ph7_value_is_array(apArg[0]) ){` |
|    441 |  7718 | `		ph7_value *pA = ph7_array_fetch(apArg[0], "0", 1);` |
|    441 |  7719 | `		ph7_value *pB = ph7_array_fetch(apArg[0], "1", 1);` |
|    446 |  7720 | `		if( pA && (pA->iFlags & MEMOBJ_OBJ) ){` |
|     11 |  7721 | `			ph7_class_instance *pObj = (ph7_class_instance *)pA->x.pOther;` |
|     11 |  7722 | `			ph7_class_method *pM0 = pB && (pB->iFlags & MEMOBJ_STRING)` |
|     15 |  7723 | `				? PH7_ClassExtractMethod(pObj->pClass,` |
|     10 |  7724 | `					(const char *)SyBlobData(&pB->sBlob), SyBlobLength(&pB->sBlob))` |
|      5 |  7725 | `				: 0;` |
|     11 |  7726 | `			if( pM0 && (pM0->iFlags & PH7_CLASS_ATTR_FABRICATED) ){` |
|      - |  7727 | ``				/* `[$closure, '__invoke']` has to keep the OBJECT: the class name alone`` |
|      - |  7728 | `				 * resolves to the empty class-level declaration, and it is THIS` |
|      - |  7729 | `				 * closure's parameter list the pair is naming. Every other pair` |
|      - |  7730 | `				 * reduces to its class, which is what php reports as the parameter's` |
|      - |  7731 | `				 * declaring class either way. */` |
|     11 |  7732 | `				PH7_MemObjStore(pA, &sTarget);` |
|      6 |  7733 | `			}else{` |
|    ! 0 |  7734 | `				ph7_value_string(&sTarget, SyStringData(&pObj->pClass->sName),` |
|    ! 0 |  7735 | `					(int)SyStringLength(&pObj->pClass->sName));` |
|      1 |  7736 | `			}` |
|    436 |  7737 | `		}else if( pA ){` |
|    431 |  7738 | `			PH7_MemObjStore(pA, &sTarget);` |
|    213 |  7739 | `		}` |
|    441 |  7740 | `		if( pB ){` |
|    441 |  7741 | `			PH7_MemObjStore(pB, &sMethod);` |
|    218 |  7742 | `		}` |
|    223 |  7743 | `	}else{` |
|      - |  7744 | ``		/* A STRING is a plain function name, `"C::m"` included: php does not`` |
|      - |  7745 | ``		 * split one here (it reports `Function C::m() does not exist`), unlike`` |
|      - |  7746 | `		 * ReflectionMethod's own one-argument form. The prelude split it and` |
|      - |  7747 | `		 * silently reflected the method. */` |
|    859 |  7748 | `		PH7_MemObjStore(apArg[0], &sTarget);` |
|      - |  7749 | `	}` |
|   1295 |  7750 | `	if( !ReflectFuncFill(pCtx->pVm, &sTarget, (sMethod.iFlags & MEMOBJ_STRING) ? &sMethod : 0, &sRef) ){` |
|      - |  7751 | `		const char *zName;` |
|      - |  7752 | `		int nName;` |
|      3 |  7753 | `		if( sMethod.iFlags & MEMOBJ_STRING ){` |
|      - |  7754 | `			const char *zM;` |
|      - |  7755 | `			int nM;` |
|    ! 0 |  7756 | `			zName = ph7_value_to_string(&sTarget, &nName);` |
|    ! 0 |  7757 | `			zM = ph7_value_to_string(&sMethod, &nM);` |
|    ! 0 |  7758 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 |  7759 | `				"Method %.*s::%.*s() does not exist", nName, zName, nM, zM);` |
|    ! 0 |  7760 | `		}else{` |
|      3 |  7761 | `			zName = ph7_value_to_string(&sTarget, &nName);` |
|      4 |  7762 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  7763 | `				"Function %.*s() does not exist", nName, zName);` |
|      - |  7764 | `		}` |
|      3 |  7765 | `		goto Done;` |
|      - |  7766 | `	}` |
|   1293 |  7767 | `	nTotal = ReflectParamCount(&sRef);` |
|   1293 |  7768 | `	if( apArg[1]->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|   1283 |  7769 | `		int iWant = (int)ph7_value_to_int(apArg[1]);` |
|   1283 |  7770 | `		if( iWant >= 0 && iWant < nTotal && ReflectParamAt(&sRef, iWant, &sDesc) ){` |
|   1281 |  7771 | `			iFound = iWant;` |
|    638 |  7772 | `		}` |
|   1283 |  7773 | `		if( iFound < 0 ){` |
|      3 |  7774 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  7775 | `				"The parameter specified by its offset could not be found");` |
|      3 |  7776 | `			goto Done;` |
|      - |  7777 | `		}` |
|    643 |  7778 | `	}else{` |
|      - |  7779 | `		const char *zWant;` |
|      - |  7780 | `		int nWant, n;` |
|     11 |  7781 | `		zWant = ph7_value_to_string(apArg[1], &nWant);` |
|     33 |  7782 | `		for( n = 0 ; n < nTotal ; n++ ){` |
|     30 |  7783 | `			if( ReflectParamAt(&sRef, n, &sDesc)` |
|     30 |  7784 | `			 && SyStringLength(&sDesc.sName) == (sxu32)nWant` |
|     23 |  7785 | `			 && SyMemcmp(SyStringData(&sDesc.sName), zWant, (sxu32)nWant) == 0 ){` |
|      9 |  7786 | `				iFound = n;` |
|      9 |  7787 | `				break;` |
|      - |  7788 | `			}` |
|     12 |  7789 | `		}` |
|     11 |  7790 | `		if( iFound < 0 ){` |
|      3 |  7791 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  7792 | `				"The parameter specified by its name could not be found");` |
|      3 |  7793 | `			goto Done;` |
|      - |  7794 | `		}` |
|      - |  7795 | `	}` |
|   1931 |  7796 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sDesc.sName),` |
|   1284 |  7797 | `		(int)SyStringLength(&sDesc.sName));` |
|      - |  7798 | `	/* Record the CANONICAL target: the class the method really came through and` |
|      - |  7799 | `	 * its declared spelling, so a re-resolve cannot land somewhere else. */` |
|   1289 |  7800 | `	if( sRef.bFabricated && sRef.pClosure && sRef.pMeth ){` |
|      - |  7801 | `		/* Its target is the OBJECT plus the method name: re-resolving by class name` |
|      - |  7802 | `		 * would find the empty class-level declaration instead of this closure. */` |
|     11 |  7803 | `		PH7_NativeSetAttrObj(pVm, pThis, RP_T, sRef.pClosure);` |
|     16 |  7804 | `		PH7_NativeSetAttrStr(pVm, pThis, RP_M, SyStringData(&sRef.pMeth->sFunc.sName),` |
|     10 |  7805 | `			(int)SyStringLength(&sRef.pMeth->sFunc.sName));` |
|   1284 |  7806 | `	}else if( sRef.pMeth && sRef.pClass ){` |
|    653 |  7807 | `		PH7_NativeSetAttrStr(pVm, pThis, RP_T, SyStringData(&sRef.pClass->sName),` |
|    432 |  7808 | `			(int)SyStringLength(&sRef.pClass->sName));` |
|    653 |  7809 | `		PH7_NativeSetAttrStr(pVm, pThis, RP_M, SyStringData(&sRef.pMeth->sFunc.sName),` |
|    432 |  7810 | `			(int)SyStringLength(&sRef.pMeth->sFunc.sName));` |
|    221 |  7811 | `	}else{` |
|    847 |  7812 | `		ph7_value *pSlot = PH7_NativeAttr(pThis, RP_T);` |
|    847 |  7813 | `		if( pSlot ){` |
|    847 |  7814 | `			PH7_MemObjStore(&sTarget, pSlot);` |
|    421 |  7815 | `		}` |
|      - |  7816 | `	}` |
|   1289 |  7817 | `	PH7_NativeSetAttrInt(pVm, pThis, RP_P, (sxi64)iFound);` |
|    645 |  7818 | `Done:` |
|   1295 |  7819 | `	PH7_MemObjRelease(&sTarget);` |
|   1295 |  7820 | `	PH7_MemObjRelease(&sMethod);` |
|   1295 |  7821 | `	return rc;` |
|    650 |  7822 | `}` |
|    770 |  7823 | `static int vm_builtin_ReflectionParameter_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  7824 | `{` |
|    775 |  7825 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    775 |  7826 | `	const char *zName = "";` |
|    775 |  7827 | `	int nName = 0;` |
|    385 |  7828 | `	SXUNUSED(nArg);` |
|    385 |  7829 | `	SXUNUSED(apArg);` |
|    775 |  7830 | `	if( pThis ){` |
|    775 |  7831 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    385 |  7832 | `	}` |
|    775 |  7833 | `	ph7_result_string(pCtx, zName, nName);` |
|    775 |  7834 | `	return PH7_OK;` |
|      5 |  7835 | `}` |
|     58 |  7836 | `static int vm_builtin_ReflectionParameter_getPosition(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7837 | `{` |
|     60 |  7838 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     29 |  7839 | `	SXUNUSED(nArg);` |
|     29 |  7840 | `	SXUNUSED(apArg);` |
|     60 |  7841 | `	ph7_result_int64(pCtx, pThis ? PH7_NativeAttrInt(pThis, RP_P) : 0);` |
|     60 |  7842 | `	return PH7_OK;` |
|      2 |  7843 | `}` |
|      - |  7844 | `/* The boolean predicates that read one flag of the description. */` |
|    636 |  7845 | `static int ReflectParamFlag(ph7_context *pCtx, int iWhat)` |
|      3 |  7846 | `{` |
|      - |  7847 | `	ReflectFuncRef sRef;` |
|      - |  7848 | `	ReflectParamDesc sDesc;` |
|    639 |  7849 | `	int bYes = 0;` |
|    639 |  7850 | `	if( ReflectParamOwner(pCtx, &sRef, &sDesc) ){` |
|    639 |  7851 | `		switch( iWhat ){` |
|     53 |  7852 | `		case 0: bYes = sDesc.bByRef; break;` |
|      3 |  7853 | `		case 1: bYes = !sDesc.bByRef; break;` |
|     52 |  7854 | `		case 2: bYes = sDesc.bVariadic; break;` |
|    ! 0 |  7855 | `		case 3: bYes = sDesc.bPromoted; break;` |
|    446 |  7856 | `		case 4: bYes = sDesc.bHasDef; break;` |
|     81 |  7857 | `		case 5: bYes = SyStringLength(&sDesc.sType) > 0; break;` |
|      5 |  7858 | `		default:` |
|      - |  7859 | `			/* allowsNull(): untyped, nullable, or a type that INCLUDES null */` |
|     13 |  7860 | `			bYes = SyStringLength(&sDesc.sType) < 1 \|\| sDesc.bNullable` |
|      7 |  7861 | `				\|\| ReflectTypeNameIs(SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType), "mixed")` |
|     14 |  7862 | `				\|\| ReflectTypeNameIs(SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType), "null");` |
|     10 |  7863 | `			break;` |
|      - |  7864 | `		}` |
|    321 |  7865 | `	}else if( iWhat == 1 \|\| iWhat == 6 ){` |
|    ! 0 |  7866 | `		bYes = 1;` |
|    ! 0 |  7867 | `	}` |
|    639 |  7868 | `	ph7_result_bool(pCtx, bYes);` |
|    639 |  7869 | `	return PH7_OK;` |
|      3 |  7870 | `}` |
|      - |  7871 | `#define REFLECT_PARAM_FLAG(NAME,WHAT) \` |
|      - |  7872 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  7873 | `	{ \` |
|      - |  7874 | `		SXUNUSED(nArg); \` |
|      - |  7875 | `		SXUNUSED(apArg); \` |
|      - |  7876 | `		return ReflectParamFlag(pCtx, WHAT); \` |
|      - |  7877 | `	}` |
|     53 |  7878 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isPassedByReference, 0)` |
|      3 |  7879 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_canBePassedByValue, 1)` |
|     52 |  7880 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isVariadic, 2)` |
|    ! 0 |  7881 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isPromoted, 3)` |
|    446 |  7882 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isDefaultValueAvailable, 4)` |
|     81 |  7883 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_hasType, 5)` |
|     11 |  7884 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_allowsNull, 6)` |
|      - |  7885 |  |
|      - |  7886 | `/* isOptional(): every parameter from here on has to be omissible too. */` |
|     76 |  7887 | `static int vm_builtin_ReflectionParameter_isOptional(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  7888 | `{` |
|     79 |  7889 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7890 | `	ReflectFuncRef sRef;` |
|      - |  7891 | `	ReflectParamDesc sDesc;` |
|      - |  7892 | `	int n, nTotal, iPos;` |
|     38 |  7893 | `	SXUNUSED(nArg);` |
|     38 |  7894 | `	SXUNUSED(apArg);` |
|     79 |  7895 | `	if( pThis == 0 \|\| !ReflectParamOwner(pCtx, &sRef, 0) ){` |
|    ! 0 |  7896 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  7897 | `		return PH7_OK;` |
|      - |  7898 | `	}` |
|     79 |  7899 | `	iPos = (int)PH7_NativeAttrInt(pThis, RP_P);` |
|     79 |  7900 | `	nTotal = ReflectParamCount(&sRef);` |
|    151 |  7901 | `	for( n = iPos ; n < nTotal ; n++ ){` |
|    105 |  7902 | `		if( ReflectParamAt(&sRef, n, &sDesc) && !sDesc.bVariadic && !sDesc.bOptional ){` |
|     33 |  7903 | `			ph7_result_bool(pCtx, 0);` |
|     33 |  7904 | `			return PH7_OK;` |
|      - |  7905 | `		}` |
|     39 |  7906 | `	}` |
|     49 |  7907 | `	ph7_result_bool(pCtx, 1);` |
|     49 |  7908 | `	return PH7_OK;` |
|     41 |  7909 | `}` |
|    786 |  7910 | `static int vm_builtin_ReflectionParameter_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  7911 | `{` |
|      - |  7912 | `	ReflectFuncRef sRef;` |
|      - |  7913 | `	ReflectParamDesc sDesc;` |
|    393 |  7914 | `	SXUNUSED(nArg);` |
|    393 |  7915 | `	SXUNUSED(apArg);` |
|    791 |  7916 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| SyStringLength(&sDesc.sType) < 1 ){` |
|     11 |  7917 | `		ph7_result_null(pCtx);` |
|     11 |  7918 | `		return PH7_OK;` |
|      - |  7919 | `	}` |
|   1172 |  7920 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx,` |
|    778 |  7921 | `		SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType)));` |
|    398 |  7922 | `}` |
|      - |  7923 | `/*` |
|      - |  7924 | ` * getClass()/isArray()/isCallable() — php 8.0 deprecated these in favour of` |
|      - |  7925 | ` * getType() but still declares them and still answers. The E_DEPRECATED that` |
|      - |  7926 | ` * comes with a call is raised at the CALL now, from the one table every` |
|      - |  7927 | ` * deprecated internal name is stamped from (aDeprecatedFunc[]), which is where` |
|      - |  7928 | ` * php raises it too — before the callee's own screens rather than inside it.` |
|      - |  7929 | ` */` |
|      8 |  7930 | `static int vm_builtin_ReflectionParameter_getClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7931 | `{` |
|      - |  7932 | `	ReflectFuncRef sRef;` |
|      - |  7933 | `	ReflectParamDesc sDesc;` |
|      - |  7934 | `	const char *zType;` |
|      - |  7935 | `	int nType;` |
|      4 |  7936 | `	SXUNUSED(nArg);` |
|      4 |  7937 | `	SXUNUSED(apArg);` |
|      9 |  7938 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| SyStringLength(&sDesc.sType) < 1 ){` |
|    ! 0 |  7939 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7940 | `		return PH7_OK;` |
|      - |  7941 | `	}` |
|      9 |  7942 | `	zType = SyStringData(&sDesc.sType);` |
|      9 |  7943 | `	nType = (int)SyStringLength(&sDesc.sType);` |
|      9 |  7944 | `	if( nType > 0 && zType[0] == '?' ){` |
|      3 |  7945 | `		zType++;` |
|      3 |  7946 | `		nType--;` |
|      1 |  7947 | `	}` |
|      9 |  7948 | `	if( ReflectTypeIsBuiltin(zType, nType) ){` |
|      7 |  7949 | `		ph7_result_null(pCtx);` |
|      7 |  7950 | `		return PH7_OK;` |
|      - |  7951 | `	}` |
|      3 |  7952 | `	return ReflectResultClassOf(pCtx, PH7_VmExtractClass(pCtx->pVm, zType, (sxu32)nType, FALSE, 0));` |
|      5 |  7953 | `}` |
|     16 |  7954 | `static int ReflectParamTypeIs(ph7_context *pCtx, const char *zWant)` |
|      1 |  7955 | `{` |
|      - |  7956 | `	ReflectFuncRef sRef;` |
|      - |  7957 | `	ReflectParamDesc sDesc;` |
|     17 |  7958 | `	int bYes = 0;` |
|     17 |  7959 | `	if( ReflectParamOwner(pCtx, &sRef, &sDesc) && SyStringLength(&sDesc.sType) > 0 ){` |
|     17 |  7960 | `		const char *zType = SyStringData(&sDesc.sType);` |
|     17 |  7961 | `		int nType = (int)SyStringLength(&sDesc.sType);` |
|      - |  7962 | ``		/* php answers true for `?array` too: the deprecated pair asks about the`` |
|      - |  7963 | `		 * type ATOM, and nullability is a separate question. */` |
|     17 |  7964 | `		if( zType[0] == '?' ){` |
|      5 |  7965 | `			zType++;` |
|      5 |  7966 | `			nType--;` |
|      2 |  7967 | `		}` |
|     17 |  7968 | `		bYes = ReflectTypeNameIs(zType, nType, zWant);` |
|      8 |  7969 | `	}` |
|     17 |  7970 | `	ph7_result_bool(pCtx, bYes);` |
|     17 |  7971 | `	return PH7_OK;` |
|      1 |  7972 | `}` |
|      8 |  7973 | `static int vm_builtin_ReflectionParameter_isArray(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7974 | `{` |
|      4 |  7975 | `	SXUNUSED(nArg);` |
|      4 |  7976 | `	SXUNUSED(apArg);` |
|      9 |  7977 | `	return ReflectParamTypeIs(pCtx, "array");` |
|      1 |  7978 | `}` |
|      8 |  7979 | `static int vm_builtin_ReflectionParameter_isCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7980 | `{` |
|      4 |  7981 | `	SXUNUSED(nArg);` |
|      4 |  7982 | `	SXUNUSED(apArg);` |
|      9 |  7983 | `	return ReflectParamTypeIs(pCtx, "callable");` |
|      1 |  7984 | `}` |
|      - |  7985 | `/*` |
|      - |  7986 | `` * The class `self`/`parent` resolve against inside a parameter's default: the one`` |
|      - |  7987 | ` * that DECLARED the method (a trait's members belong to the composing class). 0 for` |
|      - |  7988 | ` * a plain function, whose defaults can name neither.` |
|      - |  7989 | ` */` |
|     28 |  7990 | `static ph7_class * ReflectParamSelfClass(const ReflectFuncRef *pRef)` |
|      1 |  7991 | `{` |
|     29 |  7992 | `	if( pRef->pMeth == 0 ){` |
|      7 |  7993 | `		return 0;` |
|      - |  7994 | `	}` |
|     23 |  7995 | `	return PH7_VmMemberOwnerClass((ph7_class *)pRef->pMeth->sFunc.pUserData,` |
|     22 |  7996 | `		pRef->pClass ? pRef->pClass : (ph7_class *)pRef->pMeth->sFunc.pUserData);` |
|     15 |  7997 | `}` |
|    234 |  7998 | `static int vm_builtin_ReflectionParameter_getDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7999 | `{` |
|      - |  8000 | `	ReflectFuncRef sRef;` |
|      - |  8001 | `	ReflectParamDesc sDesc;` |
|    117 |  8002 | `	SXUNUSED(nArg);` |
|    117 |  8003 | `	SXUNUSED(apArg);` |
|    236 |  8004 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|      5 |  8005 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  8006 | `			"Internal error: Failed to retrieve the default value");` |
|      - |  8007 | `	}` |
|    232 |  8008 | `	if( sDesc.pArg ){` |
|      - |  8009 | `		/* Compiled: the same evaluation path the VM uses for an omitted argument --` |
|      - |  8010 | `		 * except that one runs INSIDE the call, where the frame already names the` |
|      - |  8011 | ``		 * class `self::K` resolves against. Reflection has no such frame, so a`` |
|      - |  8012 | ``		 * default written `self::K` answered `Class "self" not found` where php`` |
|      - |  8013 | `		 * answers its value. Mark the declaring class the way a member` |
|      - |  8014 | `		 * initializer's evaluation does (PH7_VmPeekDeclaringClass reads the pair). */` |
|      - |  8015 | `		ph7_value sValue;` |
|     29 |  8016 | `		ph7_class *pSaveCls = pCtx->pVm->pConstEvalClass;` |
|     29 |  8017 | `		void *pSaveFrame = pCtx->pVm->pConstEvalFrame;` |
|     29 |  8018 | `		ph7_class *pDecl = ReflectParamSelfClass(&sRef);` |
|     29 |  8019 | `		if( pDecl ){` |
|     23 |  8020 | `			pCtx->pVm->pConstEvalClass = pDecl;` |
|     23 |  8021 | `			pCtx->pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pCtx->pVm->pFrame);` |
|     11 |  8022 | `		}` |
|     29 |  8023 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     29 |  8024 | `		VmLocalExec(pCtx->pVm, &sDesc.pArg->aByteCode, &sValue, FALSE);` |
|     29 |  8025 | `		pCtx->pVm->pConstEvalClass = pSaveCls;` |
|     29 |  8026 | `		pCtx->pVm->pConstEvalFrame = pSaveFrame;` |
|     29 |  8027 | `		ph7_result_value(pCtx, &sValue);` |
|     29 |  8028 | `		PH7_MemObjRelease(&sValue);` |
|     29 |  8029 | `		return PH7_OK;` |
|      - |  8030 | `	}` |
|      - |  8031 | `	{` |
|      - |  8032 | `		/* Declared: the signature's default TEXT, reduced. */` |
|    204 |  8033 | `		const char *zDef = SyStringData(&sDesc.sDefText);` |
|    204 |  8034 | `		int nDef = (int)SyStringLength(&sDesc.sDefText);` |
|    204 |  8035 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    204 |  8036 | `		if( pVal && ReflectSigScalar(pCtx, zDef, nDef, pVal) ){` |
|    192 |  8037 | `			ph7_result_value(pCtx, pVal);` |
|    192 |  8038 | `			return PH7_OK;` |
|      - |  8039 | `		}` |
|     12 |  8040 | `		if( (nDef >= 1 && zDef[0] == '[')` |
|      7 |  8041 | `		 \|\| (nDef >= (int)sizeof("array (")-1` |
|    ! 0 |  8042 | `		  && SyMemcmp(zDef, "array (", sizeof("array (")-1) == 0) ){` |
|     13 |  8043 | `			ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|     13 |  8044 | `			return PH7_OK;` |
|      - |  8045 | `		}` |
|      - |  8046 | `	}` |
|    ! 0 |  8047 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  8048 | `		"Internal error: Failed to retrieve the default value");` |
|    119 |  8049 | `}` |
|      - |  8050 | `/*` |
|      - |  8051 | ` * A default that is a plain global-constant reference compiles to exactly` |
|      - |  8052 | ` * [ OP_LOADC (EXPAND), OP_DONE ] with the name in the literal table.` |
|      - |  8053 | ` *` |
|      - |  8054 | ` * A DECLARED default is TEXT, and php answers the same two questions about one:` |
|      - |  8055 | `` * `int $type = PDO::PARAM_STR` is a constant default and its name is what the`` |
|      - |  8056 | `` * stub wrote. Only a SINGLE reference counts -- the `\|` fold beside it`` |
|      - |  8057 | ` * evaluates to a number that no constant carries, and php answers false and` |
|      - |  8058 | ` * null for it while still printing the source in the export.` |
|      - |  8059 | ` */` |
|   1486 |  8060 | `static int ReflectParamDefConst(ph7_context *pCtx, ReflectParamDesc *pDesc,` |
|      - |  8061 | `	const char **pz, int *pn)` |
|      4 |  8062 | `{` |
|      - |  8063 | `	VmInstr *aInstr;` |
|      - |  8064 | `	ph7_value *pLit;` |
|   1490 |  8065 | `	if( pDesc->pArg == 0 && pDesc->bHasDef ){` |
|   1360 |  8066 | `		const char *zDef = SyStringData(&pDesc->sDefText);` |
|   1360 |  8067 | `		int nDef = (int)SyStringLength(&pDesc->sDefText);` |
|      - |  8068 | `		ph7_value *pVal;` |
|   1360 |  8069 | `		ReflectSigTrim(&zDef, &nDef);` |
|   1360 |  8070 | `		if( nDef < 1 \|\| ReflectSigHas(zDef, nDef, "\|", 1) ){` |
|     62 |  8071 | `			return 0;` |
|      - |  8072 | `		}` |
|   1300 |  8073 | `		pVal = ph7_context_new_scalar(pCtx);` |
|   1300 |  8074 | `		if( pVal == 0 ){` |
|    ! 0 |  8075 | `			return 0;` |
|      - |  8076 | `		}` |
|   1296 |  8077 | `		if( nDef > (int)sizeof("::class")-1` |
|    773 |  8078 | `		 && SyMemcmp(&zDef[nDef - (sizeof("::class")-1)], "::class",` |
|    121 |  8079 | `		             sizeof("::class")-1) == 0 ){` |
|      - |  8080 | ``			/* `C::class` reads a class NAME rather than a constant, and php`` |
|      - |  8081 | `			 * answers false for it while still printing it in the export. */` |
|     57 |  8082 | `			return 0;` |
|      - |  8083 | `		}` |
|   1242 |  8084 | `		if( !ReflectSigGlobalConst(pCtx, zDef, nDef, pVal)` |
|   1192 |  8085 | `		 && !ReflectSigClassConst(pCtx, zDef, nDef, pVal) ){` |
|   1072 |  8086 | `			return 0;` |
|      - |  8087 | `		}` |
|    178 |  8088 | `		*pz = zDef;` |
|    178 |  8089 | `		*pn = nDef;` |
|    178 |  8090 | `		return 1;` |
|      - |  8091 | `	}` |
|    131 |  8092 | `	if( pDesc->pArg == 0 ){` |
|    ! 0 |  8093 | `		return 0;` |
|      - |  8094 | `	}` |
|    131 |  8095 | `	if( SySetUsed(&pDesc->pArg->aByteCode) == 4 ){` |
|      - |  8096 | `` 		/* A CLASS constant compiles to the class name, the member name and the `::` `` |
|      - |  8097 | `		 * fetch: [ LOADC <class>, LOADC <member>, MEMBER(static, bareword), DONE ].` |
|      - |  8098 | `		 * php answers true for one and names it the way the source spelled it --` |
|      - |  8099 | ``		 * `self::K` stays `self::K` -- minus a leading `\`, which it drops. Only the`` |
|      - |  8100 | `		 * plain global-constant shape below was recognised, so every class-constant` |
|      - |  8101 | `		 * default reported itself as no constant at all. */` |
|     49 |  8102 | `		VmInstr *aCC = (VmInstr *)SySetBasePtr(&pDesc->pArg->aByteCode);` |
|     48 |  8103 | `		if( aCC[0].iOp == PH7_OP_LOADC && aCC[1].iOp == PH7_OP_LOADC` |
|     48 |  8104 | `		 && aCC[2].iOp == PH7_OP_MEMBER && aCC[2].iP1 == 1 && aCC[2].p3 == 0` |
|     47 |  8105 | `		 && aCC[3].iOp == PH7_OP_DONE ){` |
|     47 |  8106 | `			ph7_value *pCls = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj,aCC[0].iP2);` |
|     47 |  8107 | `			ph7_value *pMem = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj,aCC[1].iP2);` |
|     46 |  8108 | `			if( pCls && pMem && SyBlobLength(&pCls->sBlob) > 0` |
|     46 |  8109 | `			 && SyBlobLength(&pMem->sBlob) > 0` |
|     49 |  8110 | `			 && !(SyBlobLength(&pMem->sBlob) == sizeof("class")-1` |
|     25 |  8111 | `			   && SyStrnicmp((const char *)SyBlobData(&pMem->sBlob),"class",` |
|      2 |  8112 | `			                 sizeof("class")-1) == 0) ){` |
|      - |  8113 | ``				/* `C::class` reads a class NAME rather than a constant, and php`` |
|      - |  8114 | `				 * answers false for it -- the same rule the declared-signature path` |
|      - |  8115 | `				 * above makes. */` |
|     43 |  8116 | `				const char *zCls = (const char *)SyBlobData(&pCls->sBlob);` |
|     43 |  8117 | `				sxu32 nCls = SyBlobLength(&pCls->sBlob);` |
|     43 |  8118 | `				SyBlob *pOut = &pCtx->pVm->sReflectConstName;` |
|     43 |  8119 | `				while( nCls > 0 && zCls[0] == '\\' ){` |
|    ! 0 |  8120 | `					zCls++;` |
|    ! 0 |  8121 | `					nCls--;` |
|    ! 0 |  8122 | `				}` |
|     43 |  8123 | `				SyBlobReset(pOut);` |
|     43 |  8124 | `				SyBlobAppend(pOut,zCls,nCls);` |
|     43 |  8125 | `				SyBlobAppend(pOut,"::",sizeof("::")-1);` |
|     43 |  8126 | `				SyBlobAppend(pOut,SyBlobData(&pMem->sBlob),SyBlobLength(&pMem->sBlob));` |
|     43 |  8127 | `				*pz = (const char *)SyBlobData(pOut);` |
|     43 |  8128 | `				*pn = (int)SyBlobLength(pOut);` |
|     43 |  8129 | `				return 1;` |
|      - |  8130 | `			}` |
|      2 |  8131 | `		}` |
|      7 |  8132 | `		return 0;` |
|      - |  8133 | `	}` |
|     83 |  8134 | `	if( SySetUsed(&pDesc->pArg->aByteCode) != 2 ){` |
|      7 |  8135 | `		return 0;` |
|      - |  8136 | `	}` |
|     77 |  8137 | `	aInstr = (VmInstr *)SySetBasePtr(&pDesc->pArg->aByteCode);` |
|     76 |  8138 | `	if( aInstr[0].iOp != PH7_OP_LOADC \|\| (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0` |
|     46 |  8139 | `	 \|\| aInstr[1].iOp != PH7_OP_DONE ){` |
|     63 |  8140 | `		return 0;` |
|      - |  8141 | `	}` |
|     15 |  8142 | `	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);` |
|     15 |  8143 | `	if( pLit == 0 \|\| SyBlobLength(&pLit->sBlob) < 1 ){` |
|    ! 0 |  8144 | `		return 0;` |
|      - |  8145 | `	}` |
|     15 |  8146 | `	*pz = (const char *)SyBlobData(&pLit->sBlob);` |
|     15 |  8147 | `	*pn = (int)SyBlobLength(&pLit->sBlob);` |
|     15 |  8148 | `	return 1;` |
|    747 |  8149 | `}` |
|    128 |  8150 | `static int vm_builtin_ReflectionParameter_isDefaultValueConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  8151 | `{` |
|      - |  8152 | `	ReflectFuncRef sRef;` |
|      - |  8153 | `	ReflectParamDesc sDesc;` |
|      - |  8154 | `	const char *z;` |
|      - |  8155 | `	int n;` |
|     64 |  8156 | `	SXUNUSED(nArg);` |
|     64 |  8157 | `	SXUNUSED(apArg);` |
|    130 |  8158 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|      - |  8159 | `		/* php raises here rather than answering false: asking whether a default` |
|      - |  8160 | `		 * is a constant presupposes there IS one. The prelude answered false. */` |
|      3 |  8161 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  8162 | `			"Internal error: Failed to retrieve the default value");` |
|      - |  8163 | `	}` |
|    128 |  8164 | `	ph7_result_bool(pCtx, ReflectParamDefConst(pCtx, &sDesc, &z, &n) != 0);` |
|    128 |  8165 | `	return PH7_OK;` |
|     66 |  8166 | `}` |
|     64 |  8167 | `static int vm_builtin_ReflectionParameter_getDefaultValueConstantName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  8168 | `{` |
|      - |  8169 | `	ReflectFuncRef sRef;` |
|      - |  8170 | `	ReflectParamDesc sDesc;` |
|      - |  8171 | `	const char *z;` |
|      - |  8172 | `	int n;` |
|     32 |  8173 | `	SXUNUSED(nArg);` |
|     32 |  8174 | `	SXUNUSED(apArg);` |
|     66 |  8175 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|    ! 0 |  8176 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  8177 | `			"Internal error: Failed to retrieve the default value");` |
|      - |  8178 | `	}` |
|     66 |  8179 | `	if( ReflectParamDefConst(pCtx, &sDesc, &z, &n) ){` |
|     38 |  8180 | `		ph7_result_string(pCtx, z, n);` |
|     20 |  8181 | `	}else{` |
|     30 |  8182 | `		ph7_result_null(pCtx);` |
|      - |  8183 | `	}` |
|     66 |  8184 | `	return PH7_OK;` |
|     34 |  8185 | `}` |
|     10 |  8186 | `static int vm_builtin_ReflectionParameter_getDeclaringFunction(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  8187 | `{` |
|     12 |  8188 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  8189 | `	ph7_value *pT, *pM;` |
|      - |  8190 | `	ph7_value *apCtor[2];` |
|      - |  8191 | `	ph7_class_instance *pOut;` |
|      - |  8192 | `	sxi32 rc;` |
|      5 |  8193 | `	SXUNUSED(nArg);` |
|      5 |  8194 | `	SXUNUSED(apArg);` |
|     12 |  8195 | `	if( pThis == 0 \|\| (pT = PH7_NativeAttr(pThis, RP_T)) == 0 ){` |
|    ! 0 |  8196 | `		ph7_result_null(pCtx);` |
|    ! 0 |  8197 | `		return PH7_OK;` |
|      - |  8198 | `	}` |
|     12 |  8199 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|     12 |  8200 | `	apCtor[0] = pT;` |
|     12 |  8201 | `	apCtor[1] = pM;` |
|     12 |  8202 | `	if( pM && (pM->iFlags & MEMOBJ_STRING) ){` |
|     10 |  8203 | `		pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|      6 |  8204 | `	}else{` |
|      3 |  8205 | `		pOut = ReflectConstruct(pCtx, "ReflectionFunction", 1, apCtor, &rc);` |
|      - |  8206 | `	}` |
|     12 |  8207 | `	if( pOut == 0 ){` |
|    ! 0 |  8208 | `		if( rc != PH7_OK ){` |
|    ! 0 |  8209 | `			return rc;` |
|      - |  8210 | `		}` |
|    ! 0 |  8211 | `		ph7_result_null(pCtx);` |
|    ! 0 |  8212 | `		return PH7_OK;` |
|      - |  8213 | `	}` |
|     12 |  8214 | `	return ReflectResultObject(pCtx, pOut);` |
|      7 |  8215 | `}` |
|     10 |  8216 | `static int vm_builtin_ReflectionParameter_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  8217 | `{` |
|      - |  8218 | `	ReflectFuncRef sRef;` |
|      5 |  8219 | `	SXUNUSED(nArg);` |
|      5 |  8220 | `	SXUNUSED(apArg);` |
|     12 |  8221 | `	if( !ReflectParamOwner(pCtx, &sRef, 0) \|\| sRef.pMeth == 0 ){` |
|      3 |  8222 | `		ph7_result_null(pCtx);` |
|      3 |  8223 | `		return PH7_OK;` |
|      - |  8224 | `	}` |
|     10 |  8225 | `	return ReflectResultClassOf(pCtx, ReflectFuncDeclClass(&sRef));` |
|      7 |  8226 | `}` |
|      8 |  8227 | `static int vm_builtin_ReflectionParameter_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8228 | `{` |
|      9 |  8229 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  8230 | `	ReflectFuncRef sRef;` |
|      - |  8231 | `	ReflectParamDesc sDesc;` |
|      - |  8232 | `	ph7_value *pT, *pM;` |
|      9 |  8233 | `	const char *zMember = 0;` |
|      9 |  8234 | `	int nMember = 0;` |
|      9 |  8235 | `	if( pThis == 0 \|\| !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| sDesc.pArg == 0 ){` |
|    ! 0 |  8236 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  8237 | `		return PH7_OK;` |
|      - |  8238 | `	}` |
|      9 |  8239 | `	pT = PH7_NativeAttr(pThis, RP_T);` |
|      9 |  8240 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|      9 |  8241 | `	if( pM && (pM->iFlags & MEMOBJ_STRING) ){` |
|      7 |  8242 | `		zMember = (const char *)SyBlobData(&pM->sBlob);` |
|      7 |  8243 | `		nMember = (int)SyBlobLength(&pM->sBlob);` |
|      3 |  8244 | `	}` |
|      - |  8245 | `	/* 32 = Attribute::TARGET_PARAMETER */` |
|     13 |  8246 | `	return ReflectBuildAttrs(pCtx, &sDesc.pArg->aAttrs, "param", pT, zMember, nMember,` |
|      8 |  8247 | `		(int)PH7_NativeAttrInt(pThis, RP_P), 32, nArg, apArg);` |
|      5 |  8248 | `}` |
|    144 |  8249 | `static int vm_builtin_ReflectionParameter_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  8250 | `{` |
|     72 |  8251 | `	SXUNUSED(nArg);` |
|     72 |  8252 | `	SXUNUSED(apArg);` |
|    147 |  8253 | `	return ReflectExportParamSelf(pCtx);` |
|      3 |  8254 | `}` |
|      - |  8255 | `/*` |
|      - |  8256 | ` * Declare chunk 2. Called from PH7_VmInstallReflection() where it used to be` |
|      - |  8257 | `` * compiled — after chunk 1, whose `Reflector` these implement and whose`` |
|      - |  8258 | `` * `ReflectionClass` they answer.`` |
|      - |  8259 | ` *` |
|      - |  8260 | ` * The method tables are in php's own DECLARATION order.` |
|      - |  8261 | ` */` |
|      - |  8262 | `/*` |
|      - |  8263 | `` * PropertyHookType — php's `enum PropertyHookType: string { Get; Set; }`, the`` |
|      - |  8264 | ` * argument ReflectionProperty::getHook()/hasHook() take.` |
|      - |  8265 | ` */` |
|   7925 |  8266 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionHookType(ph7_vm *pVm)` |
|      5 |  8267 | `{` |
|      - |  8268 | `	static const PH7_NativeEnumCase aCase[] = {` |
|      - |  8269 | `		{ "Get", { 0, 0, PH7_NATIVE_VAL_STRING, 0, "get", 0.0 } },` |
|      - |  8270 | `		{ "Set", { 0, 0, PH7_NATIVE_VAL_STRING, 0, "set", 0.0 } },` |
|      - |  8271 | `	};` |
|   7930 |  8272 | `	return PH7_InstallNativeEnum(&(*pVm), "PropertyHookType", MEMOBJ_STRING,` |
|      - |  8273 | `		aCase, SX_ARRAYSIZE(aCase), 0, 0);` |
|      5 |  8274 | `}` |
|   7925 |  8275 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionFunc(ph7_vm *pVm)` |
|      5 |  8276 | `{` |
|      - |  8277 | `	static const PH7_NativePropDef aAbstractProp[] = {` |
|      - |  8278 | `		{ "name",  PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  8279 | `		/* PHL-only: the Closure being reflected. php reaches the same state from` |
|      - |  8280 | `		 * the function record itself; PHL has no hidden-slot bit yet (recorded). */` |
|      - |  8281 | `		{ RF_CL,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  8282 | `	};` |
|      - |  8283 | `	static const PH7_NativeMethodDef aAbstractMethod[] = {` |
|      - |  8284 | `		{ "__clone",       PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - |  8285 | `		{ "inNamespace",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_inNamespace },` |
|      - |  8286 | `		{ "isClosure",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isClosure },` |
|      - |  8287 | `		{ "isDeprecated",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isDeprecated },` |
|      - |  8288 | `		{ "isInternal",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isInternal },` |
|      - |  8289 | `		{ "isUserDefined", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isUserDefined },` |
|      - |  8290 | `		{ "isGenerator",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isGenerator },` |
|      - |  8291 | `		{ "isVariadic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isVariadic },` |
|      - |  8292 | `		{ "isStatic",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isStatic },` |
|      - |  8293 | `		{ "getClosureThis", PH7_MOD_PUBLIC, "", "@?object", vm_builtin_ReflectionFunc_getClosureThis },` |
|      - |  8294 | `		{ "getClosureScopeClass", PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|      - |  8295 | `		  vm_builtin_ReflectionFunc_getClosureScopeClass },` |
|      - |  8296 | `		/* php answers the same class for both; PHL has no separate called-scope. */` |
|      - |  8297 | `		{ "getClosureCalledClass", PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|      - |  8298 | `		  vm_builtin_ReflectionFunc_getClosureCalledClass },` |
|      - |  8299 | `		{ "getClosureUsedVariables", PH7_MOD_PUBLIC, "", "array",` |
|      - |  8300 | `		  vm_builtin_ReflectionFunc_getClosureUsedVariables },` |
|      - |  8301 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getDocComment },` |
|      - |  8302 | `		{ "getEndLine",    PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionFunc_getEndLine },` |
|      - |  8303 | `		{ "getExtension",  PH7_MOD_PUBLIC, "", "@?ReflectionExtension", vm_builtin_ReflectionFunc_getExtension },` |
|      - |  8304 | `		{ "getExtensionName", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getExtensionName },` |
|      - |  8305 | `		{ "getFileName",   PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getFileName },` |
|      - |  8306 | `		{ "getName",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getName },` |
|      - |  8307 | `		{ "getNamespaceName", PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getNamespaceName },` |
|      - |  8308 | `		{ "getNumberOfParameters", PH7_MOD_PUBLIC, "", "@int",` |
|      - |  8309 | `		  vm_builtin_ReflectionFunc_getNumberOfParameters },` |
|      - |  8310 | `		{ "getNumberOfRequiredParameters", PH7_MOD_PUBLIC, "", "@int",` |
|      - |  8311 | `		  vm_builtin_ReflectionFunc_getNumberOfRequiredParameters },` |
|      - |  8312 | `		{ "getParameters", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionFunc_getParameters },` |
|      - |  8313 | `		{ "getShortName",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getShortName },` |
|      - |  8314 | `		{ "getStartLine",  PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionFunc_getStartLine },` |
|      - |  8315 | `		{ "getStaticVariables", PH7_MOD_PUBLIC, "", "@array",` |
|      - |  8316 | `		  vm_builtin_ReflectionFunc_getStaticVariables },` |
|      - |  8317 | `		{ "returnsReference", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_returnsReference },` |
|      - |  8318 | `		{ "hasReturnType", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_hasReturnType },` |
|      - |  8319 | `		{ "getReturnType", PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionFunc_getReturnType },` |
|      - |  8320 | `		{ "hasTentativeReturnType", PH7_MOD_PUBLIC, "", "bool",` |
|      - |  8321 | `		  vm_builtin_ReflectionFunc_hasTentativeReturnType },` |
|      - |  8322 | `		{ "getTentativeReturnType", PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|      - |  8323 | `		  vm_builtin_ReflectionFunc_getTentativeReturnType },` |
|      - |  8324 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - |  8325 | `		  vm_builtin_ReflectionFunc_getAttributes },` |
|      - |  8326 | `	};` |
|      - |  8327 | `	static const PH7_NativeConstDef aFunctionConst[] = {` |
|      - |  8328 | `		{ "IS_DEPRECATED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2048, 0, 0.0 },` |
|      - |  8329 | `	};` |
|      - |  8330 | `	static const PH7_NativeMethodDef aFunctionMethod[] = {` |
|      - |  8331 | `		{ "__construct", PH7_MOD_PUBLIC, "Closure\|string $function", "",` |
|      - |  8332 | `		  vm_builtin_ReflectionFunction_construct },` |
|      - |  8333 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionFunc_toString },` |
|      - |  8334 | `		{ "isAnonymous", PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionFunction_isAnonymous },` |
|      - |  8335 | `		{ "isDisabled",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunction_isDisabled },` |
|      - |  8336 | `		{ "invoke",      PH7_MOD_PUBLIC, "mixed ...$args", "@mixed",` |
|      - |  8337 | `		  vm_builtin_ReflectionFunction_invoke },` |
|      - |  8338 | `		{ "invokeArgs",  PH7_MOD_PUBLIC, "array $args", "@mixed",` |
|      - |  8339 | `		  vm_builtin_ReflectionFunction_invokeArgs },` |
|      - |  8340 | `		{ "getClosure",  PH7_MOD_PUBLIC, "", "@Closure", vm_builtin_ReflectionFunction_getClosure },` |
|      - |  8341 | `	};` |
|      - |  8342 | `	static const PH7_NativePropDef aMethodProp[] = {` |
|      - |  8343 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  8344 | ``		/* PHL-only: php's `intern->ce`, which no property publishes. */`` |
|      - |  8345 | `		{ RM_CE,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  8346 | `	};` |
|      - |  8347 | `	static const PH7_NativeConstDef aMethodConst[] = {` |
|      - |  8348 | `		{ "IS_STATIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16, 0, 0.0 },` |
|      - |  8349 | `		{ "IS_PUBLIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,  0, 0.0 },` |
|      - |  8350 | `		{ "IS_PROTECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,  0, 0.0 },` |
|      - |  8351 | `		{ "IS_PRIVATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,  0, 0.0 },` |
|      - |  8352 | `		{ "IS_ABSTRACT",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64, 0, 0.0 },` |
|      - |  8353 | `		{ "IS_FINAL",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|      - |  8354 | `	};` |
|      - |  8355 | `	static const PH7_NativeMethodDef aMethodMethod[] = {` |
|      - |  8356 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $objectOrMethod, ?string $method = null", "",` |
|      - |  8357 | `		  vm_builtin_ReflectionMethod_construct },` |
|      - |  8358 | `		{ "createFromMethodName", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $method", "static",` |
|      - |  8359 | `		  vm_builtin_ReflectionMethod_createFromMethodName },` |
|      - |  8360 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionFunc_toString },` |
|      - |  8361 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isPublic },` |
|      - |  8362 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isPrivate },` |
|      - |  8363 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isProtected },` |
|      - |  8364 | `		{ "isAbstract",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isAbstract },` |
|      - |  8365 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isFinal },` |
|      - |  8366 | `		{ "isConstructor", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isConstructor },` |
|      - |  8367 | `		{ "isDestructor",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isDestructor },` |
|      - |  8368 | `		{ "getClosure",  PH7_MOD_PUBLIC, "?object $object = null", "@Closure",` |
|      - |  8369 | `		  vm_builtin_ReflectionMethod_getClosure },` |
|      - |  8370 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionMethod_getModifiers },` |
|      - |  8371 | `		{ "invoke",      PH7_MOD_PUBLIC, "?object $object, mixed ...$args", "@mixed",` |
|      - |  8372 | `		  vm_builtin_ReflectionMethod_invoke },` |
|      - |  8373 | `		{ "invokeArgs",  PH7_MOD_PUBLIC, "?object $object, array $args", "@mixed",` |
|      - |  8374 | `		  vm_builtin_ReflectionMethod_invokeArgs },` |
|      - |  8375 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|      - |  8376 | `		  vm_builtin_ReflectionMethod_getDeclaringClass },` |
|      - |  8377 | `		{ "getPrototype", PH7_MOD_PUBLIC, "", "@ReflectionMethod", vm_builtin_ReflectionMethod_getPrototype },` |
|      - |  8378 | `		{ "hasPrototype", PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionMethod_hasPrototype },` |
|      - |  8379 | `		{ "setAccessible", PH7_MOD_PUBLIC, "bool $accessible", "@void",` |
|      - |  8380 | `		  vm_builtin_ReflectionMethod_setAccessible },` |
|      - |  8381 | `	};` |
|      - |  8382 | `	static const PH7_NativePropDef aParamProp[] = {` |
|      - |  8383 | `		{ "name", PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  8384 | `		/* PHL-only, the three that identify the parameter (recorded) */` |
|      - |  8385 | `		{ RP_T,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  8386 | `		{ RP_M,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  8387 | `		{ RP_P,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|      - |  8388 | `	};` |
|      - |  8389 | `	static const PH7_NativeMethodDef aParamMethod[] = {` |
|      - |  8390 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - |  8391 | `		/* php leaves $function UNTYPED here: it takes a name, a Closure, a` |
|      - |  8392 | ``		 * `[$obj, 'm']` pair or "C::m", and no declarable union covers all four. */`` |
|      - |  8393 | `		{ "__construct", PH7_MOD_PUBLIC, "$function, string\|int $param", "",` |
|      - |  8394 | `		  vm_builtin_ReflectionParameter_construct },` |
|      - |  8395 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionParameter_toString },` |
|      - |  8396 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionParameter_getName },` |
|      - |  8397 | `		{ "isPassedByReference", PH7_MOD_PUBLIC, "", "@bool",` |
|      - |  8398 | `		  vm_builtin_ReflectionParameter_isPassedByReference },` |
|      - |  8399 | `		{ "canBePassedByValue",  PH7_MOD_PUBLIC, "", "@bool",` |
|      - |  8400 | `		  vm_builtin_ReflectionParameter_canBePassedByValue },` |
|      - |  8401 | `		{ "getDeclaringFunction", PH7_MOD_PUBLIC, "", "@ReflectionFunctionAbstract",` |
|      - |  8402 | `		  vm_builtin_ReflectionParameter_getDeclaringFunction },` |
|      - |  8403 | `		{ "getDeclaringClass",   PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|      - |  8404 | `		  vm_builtin_ReflectionParameter_getDeclaringClass },` |
|      - |  8405 | `		{ "getClass",    PH7_MOD_PUBLIC, "", "@?ReflectionClass", vm_builtin_ReflectionParameter_getClass },` |
|      - |  8406 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_hasType },` |
|      - |  8407 | `		{ "getType",     PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionParameter_getType },` |
|      - |  8408 | `		{ "isArray",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isArray },` |
|      - |  8409 | `		{ "isCallable",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isCallable },` |
|      - |  8410 | `		{ "allowsNull",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_allowsNull },` |
|      - |  8411 | `		{ "getPosition", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionParameter_getPosition },` |
|      - |  8412 | `		{ "isOptional",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isOptional },` |
|      - |  8413 | `		{ "isDefaultValueAvailable", PH7_MOD_PUBLIC, "", "@bool",` |
|      - |  8414 | `		  vm_builtin_ReflectionParameter_isDefaultValueAvailable },` |
|      - |  8415 | `		{ "getDefaultValue", PH7_MOD_PUBLIC, "", "@mixed",` |
|      - |  8416 | `		  vm_builtin_ReflectionParameter_getDefaultValue },` |
|      - |  8417 | `		{ "isDefaultValueConstant", PH7_MOD_PUBLIC, "", "@bool",` |
|      - |  8418 | `		  vm_builtin_ReflectionParameter_isDefaultValueConstant },` |
|      - |  8419 | `		{ "getDefaultValueConstantName", PH7_MOD_PUBLIC, "", "@?string",` |
|      - |  8420 | `		  vm_builtin_ReflectionParameter_getDefaultValueConstantName },` |
|      - |  8421 | `		{ "isVariadic",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isVariadic },` |
|      - |  8422 | `		{ "isPromoted",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionParameter_isPromoted },` |
|      - |  8423 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - |  8424 | `		  vm_builtin_ReflectionParameter_getAttributes },` |
|      - |  8425 | `	};` |
|      - |  8426 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  8427 | `		{ "ReflectionFunctionAbstract", 0, "Reflector", PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  8428 | `		  aAbstractMethod, SX_ARRAYSIZE(aAbstractMethod), 0, 0,` |
|      - |  8429 | `		  aAbstractProp, SX_ARRAYSIZE(aAbstractProp), 0, 0, 0 },` |
|      - |  8430 | `		{ "ReflectionFunction", "ReflectionFunctionAbstract", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  8431 | `		  aFunctionMethod, SX_ARRAYSIZE(aFunctionMethod),` |
|      - |  8432 | `		  aFunctionConst, SX_ARRAYSIZE(aFunctionConst), 0, 0, 0, 0, 0 },` |
|      - |  8433 | `		{ "ReflectionMethod", "ReflectionFunctionAbstract", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  8434 | `		  aMethodMethod, SX_ARRAYSIZE(aMethodMethod),` |
|      - |  8435 | `		  aMethodConst, SX_ARRAYSIZE(aMethodConst),` |
|      - |  8436 | `		  aMethodProp, SX_ARRAYSIZE(aMethodProp), 0, 0, 0 },` |
|      - |  8437 | `		{ "ReflectionParameter", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  8438 | `		  aParamMethod, SX_ARRAYSIZE(aParamMethod), 0, 0,` |
|      - |  8439 | `		  aParamProp, SX_ARRAYSIZE(aParamProp), 0, 0, 0 },` |
|      - |  8440 | `	};` |
|   7930 |  8441 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  8442 | `}` |
|      - |  8443 | `/*` |
|      - |  8444 | ` * ---------------------------------------------------------------------------` |
|      - |  8445 | ` * ReflectionProperty and ReflectionClassConstant.` |
|      - |  8446 | ` *` |
|      - |  8447 | ` * Chunk 3, the last of the three that made up "the core". Seven thunks retire` |
|      - |  8448 | ` * with it — __reflect_prop_read, __reflect_prop_write, __reflect_prop_state,` |
|      - |  8449 | ` * __reflect_prop_default, __reflect_static_value, __reflect_static_set and` |
|      - |  8450 | ` * __reflect_const_value — because a native method reaches an instance's slot` |
|      - |  8451 | ` * table, a static's shared slot and a constant's lazy slot directly.` |
|      - |  8452 | ` * ---------------------------------------------------------------------------` |
|      - |  8453 | ` */` |
|      - |  8454 | `#define RP_DYNOBJ "__dynobj"   /* the instance a DYNAMIC property lives on */` |
|      - |  8455 |  |
|      - |  8456 | `/* skipLazyInitialization() is a no-op on an object that is not lazy, which is` |
|      - |  8457 | ` * every object PHL can build — so it answers what php answers rather than` |
|      - |  8458 | ` * refusing (recorded). */` |
|    ! 0 |  8459 | `static int vm_builtin_ReflectionProperty_noop(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  8460 | `{` |
|    ! 0 |  8461 | `	SXUNUSED(pCtx);` |
|    ! 0 |  8462 | `	SXUNUSED(nArg);` |
|    ! 0 |  8463 | `	SXUNUSED(apArg);` |
|    ! 0 |  8464 | `	return PH7_OK;` |
|    ! 0 |  8465 | `}` |
|      - |  8466 | `/*` |
|      - |  8467 | `` * A STATIC property's shared slot, read and written the way `C::$s` is: the`` |
|      - |  8468 | ` * class's static table materializes first (a default that threw at the` |
|      - |  8469 | ` * declaration raises HERE), and an uninitialized typed static is php's Error.` |
|      - |  8470 | ` */` |
|      6 |  8471 | `static int ReflectStaticSlotRead(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr)` |
|      2 |  8472 | `{` |
|      - |  8473 | `	SyHashEntry *pSlot;` |
|      - |  8474 | `	ph7_value *pVal;` |
|      8 |  8475 | `	sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|      8 |  8476 | `	if( rc != SXRET_OK ){` |
|      3 |  8477 | `		return rc;` |
|      - |  8478 | `	}` |
|      5 |  8479 | `	pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|      5 |  8480 | `	if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|    ! 0 |  8481 | `		ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|    ! 0 |  8482 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      - |  8483 | `			"Typed static property %z::$%z must not be accessed before initialization",` |
|    ! 0 |  8484 | `			&pDecl->sDisp, &pAttr->sName);` |
|      - |  8485 | `	}` |
|      5 |  8486 | `	pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      5 |  8487 | `	if( pVal ){` |
|      5 |  8488 | `		ph7_result_value(pCtx, pVal);` |
|      3 |  8489 | `	}else{` |
|    ! 0 |  8490 | `		ph7_result_null(pCtx);` |
|      - |  8491 | `	}` |
|      5 |  8492 | `	return PH7_OK;` |
|      5 |  8493 | `}` |
|      4 |  8494 | `static int ReflectStaticSlotWrite(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr,` |
|      - |  8495 | `	ph7_value *pValue)` |
|      2 |  8496 | `{` |
|      - |  8497 | `	ph7_value *pSlot;` |
|      6 |  8498 | `	sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|      6 |  8499 | `	if( rc != SXRET_OK ){` |
|      3 |  8500 | `		return rc;` |
|      - |  8501 | `	}` |
|      3 |  8502 | `	rc = ReflectEnforceStore(pCtx, pAttr->nIdx, pValue);` |
|      3 |  8503 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  8504 | `		return rc;` |
|      - |  8505 | `	}` |
|      3 |  8506 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      3 |  8507 | `	if( pSlot ){` |
|      3 |  8508 | `		PH7_MemObjStore(pValue, pSlot);` |
|      1 |  8509 | `	}` |
|      3 |  8510 | `	return PH7_OK;` |
|      4 |  8511 | `}` |
|      - |  8512 |  |
|      - |  8513 | `/* What a ReflectionProperty / ReflectionClassConstant is looking at. */` |
|      - |  8514 | `typedef struct ReflectMemberRef ReflectMemberRef;` |
|      - |  8515 | `struct ReflectMemberRef` |
|      - |  8516 | `{` |
|      - |  8517 | `	ph7_class *pClass;            /* the reflected class */` |
|      - |  8518 | `	ph7_class_attr *pAttr;        /* the declared member (NULL for a dynamic property) */` |
|      - |  8519 | `	ph7_class_instance *pDynObj;  /* the instance a dynamic property lives on */` |
|      - |  8520 | `	const char *zName;            /* the member's name (borrowed from the slot) */` |
|      - |  8521 | `	int nName;` |
|      - |  8522 | `};` |
|      - |  8523 | `/*` |
|      - |  8524 | `` * Resolve `$this->class` + `$this->name` into the member. Uses the LISTING`` |
|      - |  8525 | ` * answer of the member walk, which is php's: a base class's PRIVATE member is` |
|      - |  8526 | ``  * not reachable by name from the subclass (`new ReflectionProperty('B','pp')` `` |
|      - |  8527 | `` * raises where `B extends A` and `A::$pp` is private).`` |
|      - |  8528 | ` */` |
|   1180 |  8529 | `static int ReflectMemberOfThis(ph7_context *pCtx, ReflectMemberRef *pOut, int iKind)` |
|      5 |  8530 | `{` |
|   1185 |  8531 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  8532 | `	const char *zClass;` |
|      - |  8533 | `	int nClass;` |
|      - |  8534 | `	SySet aMembers;` |
|      - |  8535 | `	sxu32 n;` |
|   1185 |  8536 | `	SyZero(pOut, sizeof(*pOut));` |
|   1185 |  8537 | `	if( pThis == 0 ){` |
|    ! 0 |  8538 | `		return 0;` |
|      - |  8539 | `	}` |
|   1185 |  8540 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|   1185 |  8541 | `	PH7_NativeAttrStr(pThis, "name", &pOut->zName, &pOut->nName);` |
|   1185 |  8542 | `	if( nClass < 1 ){` |
|    ! 0 |  8543 | `		return 0;` |
|      - |  8544 | `	}` |
|   1185 |  8545 | `	pOut->pClass = PH7_VmExtractClass(pCtx->pVm, zClass, (sxu32)nClass, FALSE, 0);` |
|   1185 |  8546 | `	if( pOut->pClass == 0 ){` |
|    ! 0 |  8547 | `		return 0;` |
|      - |  8548 | `	}` |
|   1185 |  8549 | `	if( iKind == REFLECT_MEMBER_PROP ){` |
|   1063 |  8550 | `		pOut->pDynObj = PH7_NativeAttrObj(pThis, RP_DYNOBJ);` |
|    529 |  8551 | `	}` |
|   1185 |  8552 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|   1185 |  8553 | `	ReflectMembers(pCtx->pVm, pOut->pClass, &aMembers, 0);` |
|   6451 |  8554 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   6445 |  8555 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   6445 |  8556 | `		if( pM->iKind == iKind && ReflectKeyIs(pM, pOut->zName, pOut->nName) ){` |
|   1179 |  8557 | `			pOut->pAttr = pM->pAttr;` |
|   1179 |  8558 | `			break;` |
|      - |  8559 | `		}` |
|   2638 |  8560 | `	}` |
|   1185 |  8561 | `	SySetRelease(&aMembers);` |
|   1185 |  8562 | `	return pOut->pAttr != 0 \|\| pOut->pDynObj != 0;` |
|    595 |  8563 | `}` |
|      - |  8564 | `/* The class php reports as the member's declarer. */` |
|     98 |  8565 | `static ph7_class * ReflectMemberDecl(const ReflectMemberRef *pRef)` |
|      3 |  8566 | `{` |
|    101 |  8567 | `	if( pRef->pAttr && pRef->pAttr->pDeclClass ){` |
|    101 |  8568 | `		return PH7_VmMemberOwnerClass(pRef->pAttr->pDeclClass,pRef->pClass);` |
|      - |  8569 | `	}` |
|    ! 0 |  8570 | `	return pRef->pClass;` |
|     52 |  8571 | `}` |
|      - |  8572 | `/* The instance slot record for a dynamic (or any instance-owned) property. */` |
|     14 |  8573 | `static VmClassAttr * ReflectInstanceAttr(ph7_class_instance *pObj, const char *zName, int nName)` |
|      1 |  8574 | `{` |
|      - |  8575 | `	SyHashEntry *pEntry;` |
|     15 |  8576 | `	if( pObj == 0 \|\| nName < 1 ){` |
|      7 |  8577 | `		return 0;` |
|      - |  8578 | `	}` |
|      9 |  8579 | `	pEntry = SyHashGet(&pObj->hAttr, (const void *)zName, (sxu32)nName);` |
|      9 |  8580 | `	return pEntry ? (VmClassAttr *)pEntry->pUserData : 0;` |
|      8 |  8581 | `}` |
|      - |  8582 | `/*` |
|      - |  8583 | ` * The instance slot a resolved ReflectionProperty addresses. A base class's` |
|      - |  8584 | ` * PRIVATE property lives under php's MANGLED storage name on every object below` |
|      - |  8585 | ` * it, so looking it up by its plain name would find the same-named property of` |
|      - |  8586 | ` * the object's OWN class instead -- reading and writing the wrong slot.` |
|      - |  8587 | ` */` |
|     98 |  8588 | `static VmClassAttr * ReflectRefInstanceAttr(ph7_vm *pVm, ph7_class_instance *pObj,` |
|      - |  8589 | `	const ReflectMemberRef *pRef)` |
|      1 |  8590 | `{` |
|     99 |  8591 | `	if( pObj && pRef->pAttr ){` |
|     97 |  8592 | `		const SyString *pKey = PH7_ClassAttrStorageName(pVm, pObj->pClass, pRef->pAttr);` |
|    145 |  8593 | `		SyHashEntry *pEntry = SyHashGet(&pObj->hAttr,` |
|     96 |  8594 | `			(const void *)SyStringData(pKey), SyStringLength(pKey));` |
|     97 |  8595 | `		return pEntry ? (VmClassAttr *)pEntry->pUserData : 0;` |
|      - |  8596 | `	}` |
|      3 |  8597 | `	return ReflectInstanceAttr(pObj, pRef->zName, pRef->nName);` |
|     50 |  8598 | `}` |
|      - |  8599 | `/* ---- ReflectionProperty ---- */` |
|      - |  8600 | `/* ReflectionProperty::__construct(object\|string $class, string $property) */` |
|    496 |  8601 | `static int vm_builtin_ReflectionProperty_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  8602 | `{` |
|    501 |  8603 | `	ph7_vm *pVm = pCtx->pVm;` |
|    501 |  8604 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    501 |  8605 | `	ph7_class_instance *pObj = 0;` |
|      - |  8606 | `	ph7_class *pClass;` |
|      - |  8607 | `	const char *zProp;` |
|      - |  8608 | `	int nProp;` |
|      - |  8609 | `	SySet aMembers;` |
|      - |  8610 | `	sxu32 n;` |
|    501 |  8611 | `	int bFound = 0;` |
|    501 |  8612 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 |  8613 | `		return PH7_OK;` |
|      - |  8614 | `	}` |
|    501 |  8615 | `	if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|     83 |  8616 | `		pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     41 |  8617 | `	}` |
|    501 |  8618 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|    501 |  8619 | `	if( pClass == 0 ){` |
|      - |  8620 | `		const char *zName;` |
|      - |  8621 | `		int nName;` |
|      3 |  8622 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|      4 |  8623 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  8624 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      - |  8625 | `	}` |
|    499 |  8626 | `	zProp = ph7_value_to_string(apArg[1], &nProp);` |
|    499 |  8627 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|    499 |  8628 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|   2087 |  8629 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   2075 |  8630 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   2075 |  8631 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zProp, nProp) ){` |
|      - |  8632 | `			/* php's $class is the DECLARING class, not the one asked about. */` |
|    487 |  8633 | `			pClass = pM->pDecl ? pM->pDecl : pClass;` |
|    487 |  8634 | `			bFound = 1;` |
|    487 |  8635 | `			break;` |
|      - |  8636 | `		}` |
|    799 |  8637 | `	}` |
|    499 |  8638 | `	SySetRelease(&aMembers);` |
|    746 |  8639 | `	PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pClass->sName),` |
|    494 |  8640 | `		(int)SyStringLength(&pClass->sName));` |
|    499 |  8641 | `	if( bFound ){` |
|    487 |  8642 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", zProp, nProp);` |
|    487 |  8643 | `		return PH7_OK;` |
|      - |  8644 | `	}` |
|      - |  8645 | `	/* Not declared: an OBJECT may still own it as a dynamic property. */` |
|     13 |  8646 | `	if( ReflectInstanceAttr(pObj, zProp, nProp) ){` |
|      7 |  8647 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", zProp, nProp);` |
|      7 |  8648 | `		PH7_NativeSetAttrObj(pVm, pThis, RP_DYNOBJ, pObj);` |
|      7 |  8649 | `		return PH7_OK;` |
|      - |  8650 | `	}` |
|     10 |  8651 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 |  8652 | `		"Property %z::$%.*s does not exist", &pClass->sDisp, nProp, zProp);` |
|    253 |  8653 | `}` |
|    310 |  8654 | `static int vm_builtin_ReflectionProperty_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  8655 | `{` |
|    313 |  8656 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    313 |  8657 | `	const char *zName = "";` |
|    313 |  8658 | `	int nName = 0;` |
|    155 |  8659 | `	SXUNUSED(nArg);` |
|    155 |  8660 | `	SXUNUSED(apArg);` |
|    313 |  8661 | `	if( pThis ){` |
|    313 |  8662 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    155 |  8663 | `	}` |
|    313 |  8664 | `	ph7_result_string(pCtx, zName, nName);` |
|    313 |  8665 | `	return PH7_OK;` |
|      3 |  8666 | `}` |
|      - |  8667 | `/*` |
|      - |  8668 | ` * getMangledName(): the name php stores the slot under —  plain for a public` |
|      - |  8669 | ` * property, "\0*\0name" for a protected one and "\0Class\0name" for a private` |
|      - |  8670 | ` * one. PHL's tables are not mangled, so the name is COMPOSED here; what php` |
|      - |  8671 | ` * exposes is the spelling, not the storage.` |
|      - |  8672 | ` */` |
|      6 |  8673 | `static int vm_builtin_ReflectionProperty_getMangledName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8674 | `{` |
|      - |  8675 | `	ReflectMemberRef sRef;` |
|      - |  8676 | `	SyBlob sOut;` |
|      3 |  8677 | `	SXUNUSED(nArg);` |
|      3 |  8678 | `	SXUNUSED(apArg);` |
|      7 |  8679 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  8680 | `		ph7_result_string(pCtx, sRef.zName, sRef.nName);` |
|    ! 0 |  8681 | `		return PH7_OK;` |
|      - |  8682 | `	}` |
|      7 |  8683 | `	if( sRef.pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|      3 |  8684 | `		ph7_result_string(pCtx, sRef.zName, sRef.nName);` |
|      3 |  8685 | `		return PH7_OK;` |
|      - |  8686 | `	}` |
|      5 |  8687 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|      5 |  8688 | `	SyBlobAppend(&sOut, "\0", 1);` |
|      5 |  8689 | `	if( sRef.pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|      3 |  8690 | `		SyBlobAppend(&sOut, "*", 1);` |
|      2 |  8691 | `	}else{` |
|      3 |  8692 | `		ph7_class *pDecl = ReflectMemberDecl(&sRef);` |
|      3 |  8693 | `		SyBlobAppend(&sOut, SyStringData(&pDecl->sName), SyStringLength(&pDecl->sName));` |
|      - |  8694 | `	}` |
|      5 |  8695 | `	SyBlobAppend(&sOut, "\0", 1);` |
|      5 |  8696 | `	SyBlobAppend(&sOut, sRef.zName, (sxu32)sRef.nName);` |
|      5 |  8697 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|      5 |  8698 | `	SyBlobRelease(&sOut);` |
|      5 |  8699 | `	return PH7_OK;` |
|      4 |  8700 | `}` |
|      - |  8701 | `/* The boolean predicates, all off the declared attribute. */` |
|    228 |  8702 | `static int ReflectPropFlag(ph7_context *pCtx, int iWhat)` |
|      2 |  8703 | `{` |
|      - |  8704 | `	ReflectMemberRef sRef;` |
|    230 |  8705 | `	int bYes = 0;` |
|    342 |  8706 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|    226 |  8707 | `		ph7_class_attr *pAttr = sRef.pAttr;` |
|    226 |  8708 | `		switch( iWhat ){` |
|      5 |  8709 | `		case 0: bYes = pAttr->iProtection == PH7_CLASS_PROT_PUBLIC; break;` |
|    ! 0 |  8710 | `		case 1: bYes = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE; break;` |
|      3 |  8711 | `		case 2: bYes = pAttr->iProtection == PH7_CLASS_PROT_PROTECTED; break;` |
|     23 |  8712 | `		case 3: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) != 0; break;` |
|     23 |  8713 | `		case 4: bYes = ReflectPropProtectedSet(pAttr); break;` |
|     16 |  8714 | `		case 5: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0; break;` |
|     64 |  8715 | `		case 6: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0; break;` |
|      3 |  8716 | `		case 7: bYes = 1; break;                                    /* isDefault */` |
|    ! 0 |  8717 | `		case 8: bYes = 0; break;                                    /* isDynamic */` |
|    128 |  8718 | `		case 9: bYes = (pAttr->iFlags` |
|    128 |  8719 | `			& (PH7_CLASS_ATTR_HOOK_VIRTUAL\|PH7_CLASS_ATTR_NATIVE_VIRTUAL)) != 0; break;` |
|      - |  8720 | ``		/* isFinal: the DECLARED `final` (PHP 8.4) or the one private(set) implies`` |
|      - |  8721 | `		 * -- php answers true for both, the same pair its modifier mask carries. */` |
|     10 |  8722 | `		case 10: bYes = (pAttr->iFlags` |
|     10 |  8723 | `			& (PH7_CLASS_ATTR_FINAL\|PH7_CLASS_ATTR_PRIVATE_SET)) != 0; break;` |
|      3 |  8724 | `		default:  /* 11: hasHooks */` |
|      7 |  8725 | `			bYes = (pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) != 0;` |
|      6 |  8726 | `			break;` |
|      - |  8727 | `		}` |
|    118 |  8728 | `	}else if( iWhat == 0 \|\| iWhat == 8 ){` |
|      - |  8729 | `		/* A dynamic property is public and, by definition, not a default one. */` |
|      3 |  8730 | `		bYes = 1;` |
|      1 |  8731 | `	}` |
|    230 |  8732 | `	ph7_result_bool(pCtx, bYes);` |
|    230 |  8733 | `	return PH7_OK;` |
|      2 |  8734 | `}` |
|      - |  8735 | `#define REFLECT_PROP_FLAG(NAME,WHAT) \` |
|      - |  8736 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  8737 | `	{ \` |
|      - |  8738 | `		SXUNUSED(nArg); \` |
|      - |  8739 | `		SXUNUSED(apArg); \` |
|      - |  8740 | `		return ReflectPropFlag(pCtx, WHAT); \` |
|      - |  8741 | `	}` |
|      5 |  8742 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPublic, 0)` |
|    ! 0 |  8743 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPrivate, 1)` |
|      3 |  8744 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isProtected, 2)` |
|     23 |  8745 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPrivateSet, 3)` |
|     23 |  8746 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isProtectedSet, 4)` |
|     16 |  8747 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isStatic, 5)` |
|     64 |  8748 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isReadOnly, 6)` |
|      7 |  8749 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isFinal, 10)` |
|      5 |  8750 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isDefault, 7)` |
|      3 |  8751 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isDynamic, 8)` |
|     86 |  8752 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isVirtual, 9)` |
|      7 |  8753 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_hasHooks, 11)` |
|      - |  8754 |  |
|      - |  8755 | `/* php has no abstract properties outside an interface stub, and PHL none at` |
|      - |  8756 | ` * all; isLazy() answers what is true of a VM without lazy objects. */` |
|    ! 0 |  8757 | `static int vm_builtin_ReflectionProperty_false(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  8758 | `{` |
|    ! 0 |  8759 | `	SXUNUSED(nArg);` |
|    ! 0 |  8760 | `	SXUNUSED(apArg);` |
|    ! 0 |  8761 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 |  8762 | `	return PH7_OK;` |
|    ! 0 |  8763 | `}` |
|      - |  8764 | `/*` |
|      - |  8765 | ` * isPromoted(): the property came from a constructor-promoted parameter. PHL` |
|      - |  8766 | ` * records promotion on the ARGUMENT (VM_FUNC_ARG_PROMOTED), not on the` |
|      - |  8767 | ` * attribute, so the constructor's parameter list is what answers.` |
|      - |  8768 | ` */` |
|      6 |  8769 | `static int vm_builtin_ReflectionProperty_isPromoted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8770 | `{` |
|      - |  8771 | `	ReflectMemberRef sRef;` |
|      - |  8772 | `	ph7_class_method *pCons;` |
|      - |  8773 | `	ph7_vm_func_arg *aArg;` |
|      - |  8774 | `	sxu32 n;` |
|      7 |  8775 | `	int bYes = 0;` |
|      3 |  8776 | `	SXUNUSED(nArg);` |
|      3 |  8777 | `	SXUNUSED(apArg);` |
|      7 |  8778 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|      7 |  8779 | `		ph7_class *pDecl = ReflectMemberDecl(&sRef);` |
|      7 |  8780 | `		pCons = PH7_ClassExtractMethod(pDecl, "__construct", sizeof("__construct")-1);` |
|      7 |  8781 | `		if( pCons ){` |
|      7 |  8782 | `			aArg = (ph7_vm_func_arg *)SySetBasePtr(&pCons->sFunc.aArgs);` |
|      9 |  8783 | `			for( n = 0 ; n < SySetUsed(&pCons->sFunc.aArgs) ; n++ ){` |
|      7 |  8784 | `				if( (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|    ! 0 |  8785 | `					continue;` |
|      - |  8786 | `				}` |
|      6 |  8787 | `				if( SyStringLength(&aArg[n].sName) == (sxu32)sRef.nName` |
|      6 |  8788 | `				 && SyMemcmp(SyStringData(&aArg[n].sName), sRef.zName, (sxu32)sRef.nName) == 0 ){` |
|      5 |  8789 | `					bYes = 1;` |
|      5 |  8790 | `					break;` |
|      - |  8791 | `				}` |
|      2 |  8792 | `			}` |
|      3 |  8793 | `		}` |
|      3 |  8794 | `	}` |
|      7 |  8795 | `	ph7_result_bool(pCtx, bYes);` |
|      7 |  8796 | `	return PH7_OK;` |
|      1 |  8797 | `}` |
|    192 |  8798 | `static int vm_builtin_ReflectionProperty_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  8799 | `{` |
|      - |  8800 | `	ReflectMemberRef sRef;` |
|     96 |  8801 | `	SXUNUSED(nArg);` |
|     96 |  8802 | `	SXUNUSED(apArg);` |
|    194 |  8803 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  8804 | `		ph7_result_int(pCtx, 1);   /* a dynamic property is public */` |
|    ! 0 |  8805 | `		return PH7_OK;` |
|      - |  8806 | `	}` |
|    194 |  8807 | `	ph7_result_int64(pCtx, ReflectPropModifiers(sRef.pAttr));` |
|    194 |  8808 | `	return PH7_OK;` |
|     98 |  8809 | `}` |
|     66 |  8810 | `static int vm_builtin_ReflectionProperty_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8811 | `{` |
|      - |  8812 | `	ReflectMemberRef sRef;` |
|     33 |  8813 | `	SXUNUSED(nArg);` |
|     33 |  8814 | `	SXUNUSED(apArg);` |
|     67 |  8815 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  8816 | `		ph7_result_null(pCtx);` |
|    ! 0 |  8817 | `		return PH7_OK;` |
|      - |  8818 | `	}` |
|     67 |  8819 | `	return ReflectResultClassOf(pCtx, ReflectMemberDecl(&sRef));` |
|     34 |  8820 | `}` |
|      6 |  8821 | `static int vm_builtin_ReflectionProperty_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8822 | `{` |
|      - |  8823 | `	ReflectMemberRef sRef;` |
|      3 |  8824 | `	SXUNUSED(nArg);` |
|      3 |  8825 | `	SXUNUSED(apArg);` |
|      6 |  8826 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr` |
|      7 |  8827 | `	 && SyStringLength(&sRef.pAttr->sDoc) > 0 ){` |
|      7 |  8828 | `		ph7_result_string(pCtx, SyStringData(&sRef.pAttr->sDoc),` |
|      4 |  8829 | `			(int)SyStringLength(&sRef.pAttr->sDoc));` |
|      3 |  8830 | `	}else{` |
|      3 |  8831 | `		ph7_result_bool(pCtx, 0);` |
|      - |  8832 | `	}` |
|      7 |  8833 | `	return PH7_OK;` |
|      1 |  8834 | `}` |
|     74 |  8835 | `static int vm_builtin_ReflectionProperty_hasType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8836 | `{` |
|      - |  8837 | `	ReflectMemberRef sRef;` |
|     37 |  8838 | `	SXUNUSED(nArg);` |
|     37 |  8839 | `	SXUNUSED(apArg);` |
|    149 |  8840 | `	ph7_result_bool(pCtx, ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP)` |
|     74 |  8841 | `		&& sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);` |
|     75 |  8842 | `	return PH7_OK;` |
|      1 |  8843 | `}` |
|    164 |  8844 | `static int vm_builtin_ReflectionProperty_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  8845 | `{` |
|      - |  8846 | `	ReflectMemberRef sRef;` |
|     82 |  8847 | `	SXUNUSED(nArg);` |
|     82 |  8848 | `	SXUNUSED(apArg);` |
|    164 |  8849 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0` |
|    164 |  8850 | `	 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|    165 |  8851 | `	 \|\| SyStringLength(&sRef.pAttr->sTypeName) < 1 ){` |
|      7 |  8852 | `		ph7_result_null(pCtx);` |
|      7 |  8853 | `		return PH7_OK;` |
|      - |  8854 | `	}` |
|      - |  8855 | `	{` |
|      - |  8856 | `		char zType[192];` |
|    162 |  8857 | `		const char *zT = ReflectMemberTypeText(pCtx->pVm,sRef.pAttr,zType,sizeof(zType));` |
|    162 |  8858 | `		return ReflectResultObject(pCtx, ReflectMakeType(pCtx, zT, (int)SyStrlen(zT)));` |
|      - |  8859 | `	}` |
|     86 |  8860 | `}` |
|    132 |  8861 | `static int vm_builtin_ReflectionProperty_hasDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  8862 | `{` |
|      - |  8863 | `	ReflectMemberRef sRef;` |
|     66 |  8864 | `	SXUNUSED(nArg);` |
|     66 |  8865 | `	SXUNUSED(apArg);` |
|    134 |  8866 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  8867 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  8868 | `		return PH7_OK;` |
|      - |  8869 | `	}` |
|    134 |  8870 | `	if( SySetUsed(&sRef.pAttr->aByteCode) > 0 \|\| sRef.pAttr->pNativeValue ){` |
|     35 |  8871 | `		ph7_result_bool(pCtx, 1);` |
|     35 |  8872 | `		return PH7_OK;` |
|      - |  8873 | `	}` |
|      - |  8874 | `	/* An UNTYPED property with no initializer still defaults to null. */` |
|    100 |  8875 | `	ph7_result_bool(pCtx, (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0);` |
|    100 |  8876 | `	return PH7_OK;` |
|     68 |  8877 | `}` |
|     30 |  8878 | `static int vm_builtin_ReflectionProperty_getDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8879 | `{` |
|      - |  8880 | `	ReflectMemberRef sRef;` |
|      - |  8881 | `	ph7_value sValue;` |
|     15 |  8882 | `	SXUNUSED(nArg);` |
|     15 |  8883 | `	SXUNUSED(apArg);` |
|     31 |  8884 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  8885 | `		sRef.pAttr = 0;` |
|    ! 0 |  8886 | `	}` |
|     31 |  8887 | `	if( sRef.pAttr && sRef.pAttr->pNativeValue && SySetUsed(&sRef.pAttr->aByteCode) < 1 ){` |
|      - |  8888 | `		/* A NATIVE property's default is a LITERAL, not byte-code — the same` |
|      - |  8889 | ``		 * record `new` materializes (PH7_NativeLiteralValue). Reading only the`` |
|      - |  8890 | `		 * byte-code answered NULL for every declared native default and raised` |
|      - |  8891 | `		 * php's no-default deprecation on a slot that hasDefaultValue() had just` |
|      - |  8892 | `		 * reported true for. */` |
|     21 |  8893 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     21 |  8894 | `		PH7_NativeLiteralValue(pCtx->pVm, sRef.pAttr->pNativeValue, &sValue);` |
|     21 |  8895 | `		ph7_result_value(pCtx, &sValue);` |
|     21 |  8896 | `		PH7_MemObjRelease(&sValue);` |
|     21 |  8897 | `		return PH7_OK;` |
|      - |  8898 | `	}` |
|     11 |  8899 | `	if( sRef.pAttr == 0 \|\| SySetUsed(&sRef.pAttr->aByteCode) < 1 ){` |
|      - |  8900 | `		/* php 8.5 deprecates the question when there is no default — an` |
|      - |  8901 | `		 * UNTYPED property still has one (null), a typed one without an` |
|      - |  8902 | `		 * initializer does not. */` |
|      5 |  8903 | `		if( sRef.pAttr == 0 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|      3 |  8904 | `			PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED,` |
|      - |  8905 | `				"ReflectionProperty::getDefaultValue() for a property without a default "` |
|      - |  8906 | `				"value is deprecated, use ReflectionProperty::hasDefaultValue() to check "` |
|      - |  8907 | `				"if the default value exists");` |
|      1 |  8908 | `		}` |
|      5 |  8909 | `		ph7_result_null(pCtx);` |
|      5 |  8910 | `		return PH7_OK;` |
|      - |  8911 | `	}` |
|      - |  8912 | `	/* Same evaluation path the VM uses for an omitted call argument */` |
|      7 |  8913 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|      7 |  8914 | `	VmLocalExec(pCtx->pVm, &sRef.pAttr->aByteCode, &sValue, FALSE);` |
|      7 |  8915 | `	ph7_result_value(pCtx, &sValue);` |
|      7 |  8916 | `	PH7_MemObjRelease(&sValue);` |
|      7 |  8917 | `	return PH7_OK;` |
|     16 |  8918 | `}` |
|      - |  8919 | `/* php 8.1 made every reflected member accessible; the setter is a no-op it kept. */` |
|    ! 0 |  8920 | `static int vm_builtin_ReflectionProperty_setAccessible(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  8921 | `{` |
|    ! 0 |  8922 | `	SXUNUSED(pCtx);` |
|    ! 0 |  8923 | `	SXUNUSED(nArg);` |
|    ! 0 |  8924 | `	SXUNUSED(apArg);` |
|    ! 0 |  8925 | `	return PH7_OK;` |
|    ! 0 |  8926 | `}` |
|      - |  8927 | `/* getValue()/setValue()/isInitialized() all need the same receiver check. */` |
|    100 |  8928 | `static sxi32 ReflectPropReceiver(ph7_context *pCtx, ReflectMemberRef *pRef,` |
|      - |  8929 | `	ph7_value *pObject, ph7_class_instance **ppThis, const char *zWho)` |
|      1 |  8930 | `{` |
|    101 |  8931 | `	*ppThis = 0;` |
|     50 |  8932 | `	SXUNUSED(pRef);` |
|    101 |  8933 | `	if( pObject && (pObject->iFlags & MEMOBJ_OBJ) ){` |
|     99 |  8934 | `		*ppThis = (ph7_class_instance *)pObject->x.pOther;` |
|     99 |  8935 | `		return PH7_OK;` |
|      - |  8936 | `	}` |
|      4 |  8937 | `	return PH7_VmThrowException(pCtx, "TypeError",` |
|      - |  8938 | `		"ReflectionProperty::%s(): Argument #1 ($object) must be provided for instance properties",` |
|      1 |  8939 | `		zWho);` |
|     51 |  8940 | `}` |
|     54 |  8941 | `static int vm_builtin_ReflectionProperty_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  8942 | `{` |
|      - |  8943 | `	ReflectMemberRef sRef;` |
|      - |  8944 | `	ph7_class_instance *pObj;` |
|      - |  8945 | `	VmClassAttr *pVmAttr;` |
|      - |  8946 | `	ph7_value *pValue;` |
|      - |  8947 | `	sxi32 rc;` |
|     56 |  8948 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  8949 | `		ph7_result_null(pCtx);` |
|    ! 0 |  8950 | `		return PH7_OK;` |
|      - |  8951 | `	}` |
|     56 |  8952 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|      8 |  8953 | `		return ReflectStaticSlotRead(pCtx, sRef.pClass, sRef.pAttr);` |
|      - |  8954 | `	}` |
|     49 |  8955 | `	rc = ReflectPropReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pObj, "getValue");` |
|     49 |  8956 | `	if( rc != PH7_OK ){` |
|      3 |  8957 | `		return rc;` |
|      - |  8958 | `	}` |
|     47 |  8959 | `	pVmAttr = ReflectRefInstanceAttr(pCtx->pVm, pObj, &sRef);` |
|     47 |  8960 | `	if( pVmAttr == 0 ){` |
|      - |  8961 | `		/* No slot: a class whose properties are its own handlers answers here,` |
|      - |  8962 | ``		 * as php's does -- Reflection reads a PDORow's `queryString` through`` |
|      - |  8963 | ``		 * read_property exactly as `$row->queryString` does. */`` |
|      - |  8964 | `		PH7_NativePropCtx sNat;` |
|      - |  8965 | `		SyString sNatName;` |
|      - |  8966 | `		ph7_value sNatVal;` |
|     11 |  8967 | `		SyStringInitFromBuf(&sNatName, sRef.zName, (sxu32)sRef.nName);` |
|     11 |  8968 | `		PH7_MemObjInit(pCtx->pVm, &sNatVal);` |
|     11 |  8969 | `		if( PH7_ClassNativePropAsk(pObj, &sNat, PH7_NATIVE_PROP_READ, &sNatName, &sNatVal) ){` |
|      7 |  8970 | `			if( sNat.zThrowClass ){` |
|    ! 0 |  8971 | `				PH7_MemObjRelease(&sNatVal);` |
|    ! 0 |  8972 | `				return PH7_VmThrowExceptionCode(pCtx, sNat.zThrowClass, sNat.iThrowCode,` |
|    ! 0 |  8973 | `					"%s", sNat.zThrowMsg);` |
|      - |  8974 | `			}` |
|      7 |  8975 | `			ph7_result_value(pCtx, &sNatVal);` |
|      7 |  8976 | `			PH7_MemObjRelease(&sNatVal);` |
|      7 |  8977 | `			return PH7_OK;` |
|      - |  8978 | `		}` |
|      5 |  8979 | `		PH7_MemObjRelease(&sNatVal);` |
|      5 |  8980 | `		if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){` |
|      - |  8981 | `` 			/* A VIRTUAL property: php reads it through the same handler `$o->p` `` |
|      - |  8982 | `			 * reaches, so Reflection answers the VALUE rather than warning that a` |
|      - |  8983 | `			 * name the class declares is undefined. A class whose handlers are its` |
|      - |  8984 | `			 * magic trio (ext/dom) is read through __get, which is the door the` |
|      - |  8985 | `			 * member opcode takes for the very same name. */` |
|      - |  8986 | `			ph7_value sMagic;` |
|      - |  8987 | `			SyString sMagicName;` |
|    ! 0 |  8988 | `			SyStringInitFromBuf(&sMagicName, sRef.zName, (sxu32)sRef.nName);` |
|    ! 0 |  8989 | `			PH7_MemObjInit(pCtx->pVm, &sMagic);` |
|    ! 0 |  8990 | `			if( PH7_ClassInstanceCallMagicMethod(pCtx->pVm, pObj->pClass, pObj,` |
|    ! 0 |  8991 | `					"__get", sizeof("__get")-1, &sMagicName, &sMagic) == SXRET_OK ){` |
|    ! 0 |  8992 | `				ph7_result_value(pCtx, &sMagic);` |
|    ! 0 |  8993 | `				PH7_MemObjRelease(&sMagic);` |
|    ! 0 |  8994 | `				return PH7_OK;` |
|      - |  8995 | `			}` |
|    ! 0 |  8996 | `			PH7_MemObjRelease(&sMagic);` |
|    ! 0 |  8997 | `			ph7_result_null(pCtx);` |
|    ! 0 |  8998 | `			return PH7_OK;` |
|      - |  8999 | `		}` |
|      4 |  9000 | `		if( sRef.pAttr` |
|      5 |  9001 | `		 && (sRef.pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|      2 |  9002 | `		                           \|PH7_CLASS_ATTR_HIDDEN)) == 0 ){` |
|      - |  9003 | `			/* A DECLARED property the object no longer holds -- unset() took it.` |
|      - |  9004 | ``			 * php reads it the way `$o->p` does, and warns exactly the same:`` |
|      - |  9005 | ``			 * `Undefined property: C::$p`, naming the OBJECT's class. */`` |
|      7 |  9006 | `			VmErrorFormat(pCtx->pVm, PH7_CTX_WARNING, "Undefined property: %z::$%.*s",` |
|      4 |  9007 | `				&pObj->pClass->sDisp, sRef.nName, sRef.zName);` |
|      2 |  9008 | `		}` |
|      5 |  9009 | `		ph7_result_null(pCtx);` |
|      5 |  9010 | `		return PH7_OK;` |
|      - |  9011 | `	}` |
|     37 |  9012 | `	if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|      3 |  9013 | `		ph7_class *pDecl = PH7_VmMemberOwnerClass(` |
|      4 |  9014 | `			pVmAttr->pAttr ? pVmAttr->pAttr->pDeclClass : 0,pObj->pClass);` |
|      7 |  9015 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      - |  9016 | `			"Typed property %z::$%.*s must not be accessed before initialization",` |
|      2 |  9017 | `			&pDecl->sDisp, sRef.nName, sRef.zName);` |
|      - |  9018 | `	}` |
|     33 |  9019 | `	pValue = PH7_ClassInstanceExtractAttrValue(pObj, pVmAttr);` |
|     33 |  9020 | `	if( pValue ){` |
|     33 |  9021 | `		ph7_result_value(pCtx, pValue);` |
|     17 |  9022 | `	}else{` |
|    ! 0 |  9023 | `		ph7_result_null(pCtx);` |
|      - |  9024 | `	}` |
|     33 |  9025 | `	return PH7_OK;` |
|     29 |  9026 | `}` |
|     30 |  9027 | `static int vm_builtin_ReflectionProperty_setValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  9028 | `{` |
|      - |  9029 | `	ReflectMemberRef sRef;` |
|      - |  9030 | `	ph7_class_instance *pObj;` |
|      - |  9031 | `	VmClassAttr *pVmAttr;` |
|      - |  9032 | `	ph7_value *pSlot;` |
|      - |  9033 | `	sxi32 rc;` |
|     32 |  9034 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| nArg < 1 ){` |
|    ! 0 |  9035 | `		return PH7_OK;` |
|      - |  9036 | `	}` |
|     32 |  9037 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|      - |  9038 | `		/* php's one-argument spelling for a static: setValue($value). Two` |
|      - |  9039 | `		 * arguments, or one OBJECT, means the instance form was used and the` |
|      - |  9040 | `		 * value is the second. */` |
|      6 |  9041 | `		ph7_value *pVal = apArg[0];` |
|      6 |  9042 | `		if( nArg > 1 ){` |
|      6 |  9043 | `			pVal = apArg[1];` |
|      2 |  9044 | `		}else if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|    ! 0 |  9045 | `			return PH7_OK;` |
|      - |  9046 | `		}` |
|      6 |  9047 | `		return ReflectStaticSlotWrite(pCtx, sRef.pClass, sRef.pAttr, pVal);` |
|      - |  9048 | `	}` |
|     27 |  9049 | `	rc = ReflectPropReceiver(pCtx, &sRef, apArg[0], &pObj, "setValue");` |
|     27 |  9050 | `	if( rc != PH7_OK ){` |
|    ! 0 |  9051 | `		return rc;` |
|      - |  9052 | `	}` |
|     27 |  9053 | `	pVmAttr = ReflectRefInstanceAttr(pCtx->pVm, pObj, &sRef);` |
|     27 |  9054 | `	if( pVmAttr == 0 ){` |
|      - |  9055 | `		/* No slot: the write handler answers, and for a class that has one the` |
|      - |  9056 | `		 * answer is its own refusal (php's PDORow refuses Reflection's write` |
|      - |  9057 | ``		 * with the same sentence `$row->p = 1` takes). */`` |
|      - |  9058 | `		PH7_NativePropCtx sNat;` |
|      - |  9059 | `		SyString sNatName;` |
|      - |  9060 | `		ph7_value sNatVal;` |
|     11 |  9061 | `		SyStringInitFromBuf(&sNatName, sRef.zName, (sxu32)sRef.nName);` |
|     11 |  9062 | `		PH7_MemObjInit(pCtx->pVm, &sNatVal);` |
|     10 |  9063 | `		if( PH7_ClassNativePropAsk(pObj, &sNat, PH7_NATIVE_PROP_WRITE, &sNatName, &sNatVal)` |
|      7 |  9064 | `		 && sNat.zThrowClass ){` |
|      3 |  9065 | `			PH7_MemObjRelease(&sNatVal);` |
|      6 |  9066 | `			return PH7_VmThrowExceptionCode(pCtx, sNat.zThrowClass, sNat.iThrowCode,` |
|      1 |  9067 | `				"%s", sNat.zThrowMsg);` |
|      - |  9068 | `		}` |
|      9 |  9069 | `		PH7_MemObjRelease(&sNatVal);` |
|      9 |  9070 | `		if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){` |
|      - |  9071 | `` 			/* A VIRTUAL property: php's write goes to the same handler `$o->p = v` `` |
|      - |  9072 | `			 * reaches -- the class's own write_property, which refuses the read-only` |
|      - |  9073 | `			 * half of the surface with its own sentence. Creating a slot here would` |
|      - |  9074 | `			 * give the object a real property php has none of. */` |
|      5 |  9075 | `			ph7_value sMagicVal, *pMagicArg = nArg > 1 ? apArg[1] : 0;` |
|      5 |  9076 | `			ph7_class_method *pSet = PH7_ClassExtractMethod(pObj->pClass,` |
|      - |  9077 | `				"__set", sizeof("__set")-1);` |
|      5 |  9078 | `			PH7_MemObjInit(pCtx->pVm, &sMagicVal);` |
|      5 |  9079 | `			if( pMagicArg == 0 ){` |
|    ! 0 |  9080 | `				pMagicArg = &sMagicVal;` |
|    ! 0 |  9081 | `			}` |
|      5 |  9082 | `			if( PH7_ClassNativePropOwns(pObj, &sNatName) ){` |
|      - |  9083 | `				PH7_NativePropCtx sStore;` |
|      5 |  9084 | `				sxi32 rcSt = PH7_OK;` |
|      6 |  9085 | `				if( PH7_ClassNativePropAsk(pObj, &sStore, PH7_NATIVE_PROP_STORE,` |
|      2 |  9086 | `						&sNatName, pMagicArg)` |
|      5 |  9087 | `				 && sStore.zThrowClass ){` |
|      4 |  9088 | `					rcSt = PH7_VmThrowExceptionCode(pCtx, sStore.zThrowClass,` |
|      1 |  9089 | `						sStore.iThrowCode, "%s", sStore.zThrowMsg);` |
|      1 |  9090 | `				}` |
|      5 |  9091 | `				PH7_MemObjRelease(&sMagicVal);` |
|      5 |  9092 | `				return rcSt;` |
|      - |  9093 | `			}` |
|    ! 0 |  9094 | `			if( pSet ){` |
|      - |  9095 | `				ph7_value sMagicName, *apMagic[2];` |
|    ! 0 |  9096 | `				PH7_MemObjInitFromString(pCtx->pVm, &sMagicName, 0);` |
|    ! 0 |  9097 | `				PH7_MemObjStringAppend(&sMagicName, sRef.zName, (sxu32)sRef.nName);` |
|    ! 0 |  9098 | `				sMagicName.nIdx = SXU32_HIGH;` |
|    ! 0 |  9099 | `				apMagic[0] = &sMagicName;` |
|    ! 0 |  9100 | `				apMagic[1] = pMagicArg;` |
|    ! 0 |  9101 | `				rc = PH7_VmCallMagicMethod(pCtx->pVm, pObj, pSet, 0, 2, apMagic);` |
|    ! 0 |  9102 | `				PH7_MemObjRelease(&sMagicName);` |
|    ! 0 |  9103 | `				PH7_MemObjRelease(&sMagicVal);` |
|    ! 0 |  9104 | `				return rc == PH7_ABORT ? PH7_ABORT : PH7_OK;` |
|      - |  9105 | `			}` |
|    ! 0 |  9106 | `			PH7_MemObjRelease(&sMagicVal);` |
|    ! 0 |  9107 | `			return PH7_OK;` |
|      - |  9108 | `		}` |
|      4 |  9109 | `		if( sRef.pAttr` |
|      5 |  9110 | `		 && (sRef.pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|      2 |  9111 | `		                           \|PH7_CLASS_ATTR_HIDDEN)) == 0 ){` |
|      - |  9112 | `			/* A DECLARED property unset() took away: php's write RE-CREATES it,` |
|      - |  9113 | ``			 * exactly as `$o->p = v` does. PHL wrote nowhere and said nothing. */`` |
|      5 |  9114 | `			VmRecreateDeclaredAttr(pCtx->pVm, pObj, sRef.pAttr, &pVmAttr);` |
|      2 |  9115 | `		}` |
|      5 |  9116 | `		if( pVmAttr == 0 ){` |
|    ! 0 |  9117 | `			return PH7_OK;` |
|      - |  9118 | `		}` |
|      2 |  9119 | `	}` |
|     21 |  9120 | `	if( pVmAttr->pAttr && (pVmAttr->iState & VM_CLASS_ATTR_RDONLY) ){` |
|      - |  9121 | `		/* php's read-only handler refuses Reflection's write with the sentence` |
|      - |  9122 | ``		 * `$stmt->queryString = 'x'` takes. */`` |
|      3 |  9123 | `		return VmThrowNativeReadOnly(pCtx->pVm, pVmAttr->pAttr);` |
|      - |  9124 | `	}` |
|      - |  9125 | `	{` |
|     19 |  9126 | `		ph7_value *pVal = nArg > 1 ? apArg[1] : 0;` |
|      - |  9127 | `		ph7_value sNull;` |
|     19 |  9128 | `		PH7_MemObjInit(pCtx->pVm, &sNull);` |
|     19 |  9129 | `		if( pVal == 0 ){` |
|    ! 0 |  9130 | `			pVal = &sNull;` |
|    ! 0 |  9131 | `		}` |
|     19 |  9132 | `		rc = ReflectEnforceStore(pCtx, pVmAttr->nIdx, pVal);` |
|     19 |  9133 | `		if( rc != SXRET_OK ){` |
|      5 |  9134 | `			PH7_MemObjRelease(&sNull);` |
|      5 |  9135 | `			return rc;` |
|      - |  9136 | `		}` |
|     15 |  9137 | `		pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|     15 |  9138 | `		if( pSlot ){` |
|     15 |  9139 | `			PH7_MemObjStore(pVal, pSlot);` |
|     15 |  9140 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      7 |  9141 | `		}` |
|     15 |  9142 | `		PH7_MemObjRelease(&sNull);` |
|      - |  9143 | `	}` |
|     15 |  9144 | `	return PH7_OK;` |
|     17 |  9145 | `}` |
|     32 |  9146 | `static int vm_builtin_ReflectionProperty_isInitialized(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  9147 | `{` |
|      - |  9148 | `	ReflectMemberRef sRef;` |
|      - |  9149 | `	ph7_class_instance *pObj;` |
|      - |  9150 | `	VmClassAttr *pVmAttr;` |
|      - |  9151 | `	sxi32 rc;` |
|     34 |  9152 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  9153 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  9154 | `		return PH7_OK;` |
|      - |  9155 | `	}` |
|     34 |  9156 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|      - |  9157 | `		SyHashEntry *pSlot;` |
|      8 |  9158 | `		rc = ReflectMaterializeStatics(pCtx, sRef.pClass);` |
|      8 |  9159 | `		if( rc != SXRET_OK ){` |
|      5 |  9160 | `			return rc;` |
|      - |  9161 | `		}` |
|      3 |  9162 | `		pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&sRef.pAttr->nIdx, sizeof(sxu32));` |
|      5 |  9163 | `		ph7_result_bool(pCtx, pSlot == 0` |
|      2 |  9164 | `			\|\| (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) == 0);` |
|      3 |  9165 | `		return PH7_OK;` |
|      - |  9166 | `	}` |
|     27 |  9167 | `	rc = ReflectPropReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pObj, "isInitialized");` |
|     27 |  9168 | `	if( rc != PH7_OK ){` |
|    ! 0 |  9169 | `		return rc;` |
|      - |  9170 | `	}` |
|     27 |  9171 | `	pVmAttr = ReflectRefInstanceAttr(pCtx->pVm, pObj, &sRef);` |
|     26 |  9172 | `	if( pVmAttr == 0 && sRef.pAttr` |
|      5 |  9173 | `	 && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){` |
|      - |  9174 | `		/* A VIRTUAL property has no slot to be uninitialized: php asks the` |
|      - |  9175 | `		 * has_property handler, and ext/dom's answers yes for every name it` |
|      - |  9176 | `		 * declares. */` |
|      3 |  9177 | `		ph7_result_bool(pCtx, 1);` |
|      3 |  9178 | `		return PH7_OK;` |
|      - |  9179 | `	}` |
|     25 |  9180 | `	ph7_result_bool(pCtx, pVmAttr != 0 && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0);` |
|     25 |  9181 | `	return PH7_OK;` |
|     18 |  9182 | `}` |
|      - |  9183 | `/*` |
|      - |  9184 | `` * The property HOOKS (php 8.4). PHL compiles `public $p { get => ...; }` into`` |
|      - |  9185 | `` * synthesized methods named `__phl_hook_get_NAME` / `__phl_hook_set_NAME`, so`` |
|      - |  9186 | ` * the reflector php answers is a ReflectionMethod over one of those.` |
|      - |  9187 | ` */` |
|     30 |  9188 | `static int ReflectHookMethod(ph7_context *pCtx, ReflectMemberRef *pRef, int bSet,` |
|      - |  9189 | `	ph7_class_instance **ppOut)` |
|      3 |  9190 | `{` |
|      - |  9191 | `	char zName[128];` |
|      - |  9192 | `	ph7_value sClass, sName;` |
|      - |  9193 | `	ph7_value *apCtor[2];` |
|      - |  9194 | `	ph7_class *pDecl;` |
|      - |  9195 | `	sxi32 rc;` |
|      - |  9196 | `	int nName;` |
|     33 |  9197 | `	*ppOut = 0;` |
|     33 |  9198 | `	if( pRef->pAttr == 0 ){` |
|    ! 0 |  9199 | `		return PH7_OK;` |
|      - |  9200 | `	}` |
|     33 |  9201 | `	if( (pRef->pAttr->iFlags & (bSet ? PH7_CLASS_ATTR_HOOK_SET : PH7_CLASS_ATTR_HOOK_GET)) == 0 ){` |
|     15 |  9202 | `		return PH7_OK;` |
|      - |  9203 | `	}` |
|     21 |  9204 | `	pDecl = ReflectMemberDecl(pRef);` |
|     30 |  9205 | `	nName = SyBufferFormat(zName, sizeof(zName), "%s%.*s",` |
|      9 |  9206 | `		bSet ? "__phl_hook_set_" : "__phl_hook_get_", pRef->nName, pRef->zName);` |
|     21 |  9207 | `	PH7_MemObjInit(pCtx->pVm, &sClass);` |
|     21 |  9208 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|     21 |  9209 | `	ph7_value_string(&sClass, SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|     21 |  9210 | `	ph7_value_string(&sName, zName, nName);` |
|     21 |  9211 | `	apCtor[0] = &sClass;` |
|     21 |  9212 | `	apCtor[1] = &sName;` |
|     21 |  9213 | `	*ppOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|     21 |  9214 | `	PH7_MemObjRelease(&sClass);` |
|     21 |  9215 | `	PH7_MemObjRelease(&sName);` |
|     21 |  9216 | `	return rc;` |
|     18 |  9217 | `}` |
|     12 |  9218 | `static int vm_builtin_ReflectionProperty_getHooks(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  9219 | `{` |
|      - |  9220 | `	ReflectMemberRef sRef;` |
|     15 |  9221 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  9222 | `	int iHook;` |
|      6 |  9223 | `	SXUNUSED(nArg);` |
|      6 |  9224 | `	SXUNUSED(apArg);` |
|     15 |  9225 | `	if( pOut == 0 ){` |
|    ! 0 |  9226 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  9227 | `	}` |
|     15 |  9228 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  9229 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  9230 | `		return PH7_OK;` |
|      - |  9231 | `	}` |
|     39 |  9232 | `	for( iHook = 0 ; iHook < 2 ; iHook++ ){` |
|     27 |  9233 | `		ph7_class_instance *pMeth = 0;` |
|      - |  9234 | `		ph7_value sVal, *pKey;` |
|     27 |  9235 | `		sxi32 rc = ReflectHookMethod(pCtx, &sRef, iHook, &pMeth);` |
|     27 |  9236 | `		if( rc != PH7_OK ){` |
|    ! 0 |  9237 | `			return rc;` |
|      - |  9238 | `		}` |
|     27 |  9239 | `		if( pMeth == 0 ){` |
|     13 |  9240 | `			continue;` |
|      - |  9241 | `		}` |
|     17 |  9242 | `		pKey = ph7_context_new_scalar(pCtx);` |
|     17 |  9243 | `		if( pKey == 0 ){` |
|    ! 0 |  9244 | `			PH7_ClassInstanceUnref(pMeth);` |
|    ! 0 |  9245 | `			break;` |
|      - |  9246 | `		}` |
|     17 |  9247 | `		ph7_value_string(pKey, iHook ? "set" : "get", 3);` |
|     17 |  9248 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     17 |  9249 | `		sVal.x.pOther = pMeth;` |
|     17 |  9250 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|     17 |  9251 | `		ph7_array_add_elem(pOut, pKey, &sVal);   /* takes its own reference */` |
|     17 |  9252 | `		PH7_ClassInstanceUnref(pMeth);` |
|     10 |  9253 | `	}` |
|     15 |  9254 | `	ph7_result_value(pCtx, pOut);` |
|     15 |  9255 | `	return PH7_OK;` |
|      9 |  9256 | `}` |
|      - |  9257 | ``/* `get`/`set` out of a PropertyHookType case (or the bare string php also takes). */`` |
|     12 |  9258 | `static int ReflectHookKind(ph7_value *pArg)` |
|      1 |  9259 | `{` |
|      - |  9260 | `	const char *z;` |
|      - |  9261 | `	int n;` |
|     13 |  9262 | `	if( pArg == 0 ){` |
|    ! 0 |  9263 | `		return -1;` |
|      - |  9264 | `	}` |
|     13 |  9265 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|     13 |  9266 | `		ph7_class_instance *pCase = (ph7_class_instance *)pArg->x.pOther;` |
|     13 |  9267 | `		ph7_value *pVal = PH7_NativeAttr(pCase, "value");` |
|     13 |  9268 | `		if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  9269 | `			return -1;` |
|      - |  9270 | `		}` |
|     13 |  9271 | `		z = (const char *)SyBlobData(&pVal->sBlob);` |
|     13 |  9272 | `		n = (int)SyBlobLength(&pVal->sBlob);` |
|      7 |  9273 | `	}else{` |
|    ! 0 |  9274 | `		z = ph7_value_to_string(pArg, &n);` |
|      - |  9275 | `	}` |
|     13 |  9276 | `	if( n == 3 && SyMemcmp(z, "get", 3) == 0 ){` |
|      7 |  9277 | `		return 0;` |
|      - |  9278 | `	}` |
|      7 |  9279 | `	if( n == 3 && SyMemcmp(z, "set", 3) == 0 ){` |
|      7 |  9280 | `		return 1;` |
|      - |  9281 | `	}` |
|    ! 0 |  9282 | `	return -1;` |
|      7 |  9283 | `}` |
|      6 |  9284 | `static int vm_builtin_ReflectionProperty_hasHook(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9285 | `{` |
|      - |  9286 | `	ReflectMemberRef sRef;` |
|      7 |  9287 | `	int iHook = ReflectHookKind(nArg > 0 ? apArg[0] : 0);` |
|      7 |  9288 | `	int bYes = 0;` |
|      7 |  9289 | `	if( iHook >= 0 && ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|      7 |  9290 | `		bYes = (sRef.pAttr->iFlags & (iHook ? PH7_CLASS_ATTR_HOOK_SET : PH7_CLASS_ATTR_HOOK_GET)) != 0;` |
|      3 |  9291 | `	}` |
|      7 |  9292 | `	ph7_result_bool(pCtx, bYes);` |
|      7 |  9293 | `	return PH7_OK;` |
|      1 |  9294 | `}` |
|      6 |  9295 | `static int vm_builtin_ReflectionProperty_getHook(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9296 | `{` |
|      - |  9297 | `	ReflectMemberRef sRef;` |
|      7 |  9298 | `	ph7_class_instance *pMeth = 0;` |
|      7 |  9299 | `	int iHook = ReflectHookKind(nArg > 0 ? apArg[0] : 0);` |
|      - |  9300 | `	sxi32 rc;` |
|      7 |  9301 | `	if( iHook < 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  9302 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9303 | `		return PH7_OK;` |
|      - |  9304 | `	}` |
|      7 |  9305 | `	rc = ReflectHookMethod(pCtx, &sRef, iHook, &pMeth);` |
|      7 |  9306 | `	if( rc != PH7_OK ){` |
|    ! 0 |  9307 | `		return rc;` |
|      - |  9308 | `	}` |
|      7 |  9309 | `	if( pMeth == 0 ){` |
|      3 |  9310 | `		ph7_result_null(pCtx);` |
|      3 |  9311 | `		return PH7_OK;` |
|      - |  9312 | `	}` |
|      5 |  9313 | `	return ReflectResultObject(pCtx, pMeth);` |
|      4 |  9314 | `}` |
|      6 |  9315 | `static int vm_builtin_ReflectionProperty_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9316 | `{` |
|      7 |  9317 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  9318 | `	ReflectMemberRef sRef;` |
|      - |  9319 | `	ph7_value sTarget;` |
|      - |  9320 | `	const char *zClass;` |
|      - |  9321 | `	int nClass, rc;` |
|      7 |  9322 | `	if( pThis == 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9323 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  9324 | `		return PH7_OK;` |
|      - |  9325 | `	}` |
|      7 |  9326 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|      7 |  9327 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      7 |  9328 | `	ph7_value_string(&sTarget, zClass, nClass);` |
|      - |  9329 | `	/* 8 = Attribute::TARGET_PROPERTY */` |
|     10 |  9330 | `	rc = ReflectBuildAttrs(pCtx, &sRef.pAttr->aAttrs, "attr", &sTarget,` |
|      3 |  9331 | `		sRef.zName, sRef.nName, 0, 8, nArg, apArg);` |
|      7 |  9332 | `	PH7_MemObjRelease(&sTarget);` |
|      7 |  9333 | `	return rc;` |
|      4 |  9334 | `}` |
|      8 |  9335 | `static int vm_builtin_ReflectionProperty_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9336 | `{` |
|      4 |  9337 | `	SXUNUSED(nArg);` |
|      4 |  9338 | `	SXUNUSED(apArg);` |
|      9 |  9339 | `	return ReflectExportPropSelf(pCtx);` |
|      1 |  9340 | `}` |
|      - |  9341 | `/* ---- ReflectionClassConstant ---- */` |
|      - |  9342 | `/* ReflectionClassConstant::__construct(object\|string $class, string $constant) */` |
|     80 |  9343 | `static int vm_builtin_ReflectionClassConstant_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  9344 | `{` |
|     82 |  9345 | `	ph7_vm *pVm = pCtx->pVm;` |
|     82 |  9346 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     82 |  9347 | `	ph7_class *pClass, *pDecl = 0;` |
|      - |  9348 | `	const char *zConst;` |
|      - |  9349 | `	int nConst;` |
|      - |  9350 | `	SySet aMembers;` |
|      - |  9351 | `	sxu32 n;` |
|     82 |  9352 | `	int bFound = 0;` |
|     82 |  9353 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 |  9354 | `		return PH7_OK;` |
|      - |  9355 | `	}` |
|     82 |  9356 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|     82 |  9357 | `	if( pClass == 0 ){` |
|      - |  9358 | `		const char *zName;` |
|      - |  9359 | `		int nName;` |
|    ! 0 |  9360 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|    ! 0 |  9361 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 |  9362 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      - |  9363 | `	}` |
|     82 |  9364 | `	zConst = ph7_value_to_string(apArg[1], &nConst);` |
|     82 |  9365 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|     82 |  9366 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|    372 |  9367 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    366 |  9368 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|    366 |  9369 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zConst, nConst) ){` |
|     76 |  9370 | `			pDecl = pM->pDecl ? pM->pDecl : pClass;` |
|     76 |  9371 | `			bFound = 1;` |
|     76 |  9372 | `			break;` |
|      - |  9373 | `		}` |
|    146 |  9374 | `	}` |
|     82 |  9375 | `	SySetRelease(&aMembers);` |
|     82 |  9376 | `	if( !bFound ){` |
|     10 |  9377 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 |  9378 | `			"Constant %z::%.*s does not exist", &pClass->sDisp, nConst, zConst);` |
|      - |  9379 | `	}` |
|      - |  9380 | `	/* php's $class is the DECLARING class, not the one asked about. */` |
|     76 |  9381 | `	pClass = pDecl;` |
|    113 |  9382 | `	PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pClass->sName),` |
|     74 |  9383 | `		(int)SyStringLength(&pClass->sName));` |
|     76 |  9384 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", zConst, nConst);` |
|     76 |  9385 | `	return PH7_OK;` |
|     42 |  9386 | `}` |
|     22 |  9387 | `static int vm_builtin_ReflectionClassConstant_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9388 | `{` |
|      - |  9389 | `	ReflectMemberRef sRef;` |
|      - |  9390 | `	ph7_value *pVal;` |
|      - |  9391 | `	sxi32 rc;` |
|     11 |  9392 | `	SXUNUSED(nArg);` |
|     11 |  9393 | `	SXUNUSED(apArg);` |
|     23 |  9394 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9395 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9396 | `		return PH7_OK;` |
|      - |  9397 | `	}` |
|     23 |  9398 | `	rc = ReflectConstSlot(pCtx, sRef.pClass, sRef.pAttr, &pVal);` |
|     23 |  9399 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  9400 | `		return rc;` |
|      - |  9401 | `	}` |
|     23 |  9402 | `	if( pVal ){` |
|     23 |  9403 | `		ph7_result_value(pCtx, pVal);` |
|     12 |  9404 | `	}else{` |
|    ! 0 |  9405 | `		ph7_result_null(pCtx);` |
|      - |  9406 | `	}` |
|     23 |  9407 | `	return PH7_OK;` |
|     12 |  9408 | `}` |
|     28 |  9409 | `static int ReflectConstFlag(ph7_context *pCtx, int iWhat)` |
|      1 |  9410 | `{` |
|      - |  9411 | `	ReflectMemberRef sRef;` |
|     29 |  9412 | `	int bYes = 0;` |
|     29 |  9413 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) && sRef.pAttr ){` |
|     29 |  9414 | `		ph7_class_attr *pAttr = sRef.pAttr;` |
|     29 |  9415 | `		switch( iWhat ){` |
|      3 |  9416 | `		case 0: bYes = pAttr->iProtection == PH7_CLASS_PROT_PUBLIC; break;` |
|      5 |  9417 | `		case 1: bYes = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE; break;` |
|      5 |  9418 | `		case 2: bYes = pAttr->iProtection == PH7_CLASS_PROT_PROTECTED; break;` |
|      3 |  9419 | `		case 3: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0; break;` |
|     13 |  9420 | `		case 4: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0; break;` |
|      3 |  9421 | `		case 5: bYes = ReflectHasDeprecated(&pAttr->aAttrs); break;` |
|      3 |  9422 | `		default: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0; break;` |
|      - |  9423 | `		}` |
|     14 |  9424 | `	}` |
|     29 |  9425 | `	ph7_result_bool(pCtx, bYes);` |
|     29 |  9426 | `	return PH7_OK;` |
|      1 |  9427 | `}` |
|      - |  9428 | `#define REFLECT_CONST_FLAG(NAME,WHAT) \` |
|      - |  9429 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  9430 | `	{ \` |
|      - |  9431 | `		SXUNUSED(nArg); \` |
|      - |  9432 | `		SXUNUSED(apArg); \` |
|      - |  9433 | `		return ReflectConstFlag(pCtx, WHAT); \` |
|      - |  9434 | `	}` |
|      3 |  9435 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isPublic, 0)` |
|      5 |  9436 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isPrivate, 1)` |
|      5 |  9437 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isProtected, 2)` |
|      3 |  9438 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isFinal, 3)` |
|     13 |  9439 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isEnumCase, 4)` |
|      3 |  9440 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isDeprecated, 5)` |
|      3 |  9441 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_hasType, 6)` |
|      - |  9442 |  |
|      8 |  9443 | `static int vm_builtin_ReflectionClassConstant_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9444 | `{` |
|      - |  9445 | `	ReflectMemberRef sRef;` |
|      4 |  9446 | `	SXUNUSED(nArg);` |
|      4 |  9447 | `	SXUNUSED(apArg);` |
|      9 |  9448 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9449 | `		ph7_result_int(pCtx, 0);` |
|    ! 0 |  9450 | `		return PH7_OK;` |
|      - |  9451 | `	}` |
|      9 |  9452 | `	ph7_result_int64(pCtx, ReflectConstModifiers(sRef.pAttr));` |
|      9 |  9453 | `	return PH7_OK;` |
|      5 |  9454 | `}` |
|      6 |  9455 | `static int vm_builtin_ReflectionClassConstant_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9456 | `{` |
|      - |  9457 | `	ReflectMemberRef sRef;` |
|      3 |  9458 | `	SXUNUSED(nArg);` |
|      3 |  9459 | `	SXUNUSED(apArg);` |
|      7 |  9460 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) ){` |
|    ! 0 |  9461 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9462 | `		return PH7_OK;` |
|      - |  9463 | `	}` |
|      7 |  9464 | `	return ReflectResultClassOf(pCtx, ReflectMemberDecl(&sRef));` |
|      4 |  9465 | `}` |
|      2 |  9466 | `static int vm_builtin_ReflectionClassConstant_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9467 | `{` |
|      - |  9468 | `	ReflectMemberRef sRef;` |
|      1 |  9469 | `	SXUNUSED(nArg);` |
|      1 |  9470 | `	SXUNUSED(apArg);` |
|      2 |  9471 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) && sRef.pAttr` |
|      3 |  9472 | `	 && SyStringLength(&sRef.pAttr->sDoc) > 0 ){` |
|      4 |  9473 | `		ph7_result_string(pCtx, SyStringData(&sRef.pAttr->sDoc),` |
|      2 |  9474 | `			(int)SyStringLength(&sRef.pAttr->sDoc));` |
|      2 |  9475 | `	}else{` |
|    ! 0 |  9476 | `		ph7_result_bool(pCtx, 0);` |
|      - |  9477 | `	}` |
|      3 |  9478 | `	return PH7_OK;` |
|      1 |  9479 | `}` |
|      4 |  9480 | `static int vm_builtin_ReflectionClassConstant_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  9481 | `{` |
|      - |  9482 | `	ReflectMemberRef sRef;` |
|      2 |  9483 | `	SXUNUSED(nArg);` |
|      2 |  9484 | `	SXUNUSED(apArg);` |
|      4 |  9485 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0` |
|      4 |  9486 | `	 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|      6 |  9487 | `	 \|\| SyStringLength(&sRef.pAttr->sTypeName) < 1 ){` |
|    ! 0 |  9488 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9489 | `		return PH7_OK;` |
|      - |  9490 | `	}` |
|      - |  9491 | `	{` |
|      - |  9492 | `		char zType[192];` |
|      6 |  9493 | `		const char *zT = ReflectMemberTypeText(pCtx->pVm,sRef.pAttr,zType,sizeof(zType));` |
|      6 |  9494 | `		return ReflectResultObject(pCtx, ReflectMakeType(pCtx, zT, (int)SyStrlen(zT)));` |
|      - |  9495 | `	}` |
|      4 |  9496 | `}` |
|      2 |  9497 | `static int vm_builtin_ReflectionClassConstant_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9498 | `{` |
|      3 |  9499 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  9500 | `	ReflectMemberRef sRef;` |
|      - |  9501 | `	ph7_value sTarget;` |
|      - |  9502 | `	const char *zClass;` |
|      - |  9503 | `	int nClass, rc;` |
|      3 |  9504 | `	if( pThis == 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9505 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  9506 | `		return PH7_OK;` |
|      - |  9507 | `	}` |
|      3 |  9508 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|      3 |  9509 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      3 |  9510 | `	ph7_value_string(&sTarget, zClass, nClass);` |
|      - |  9511 | `	/* 16 = Attribute::TARGET_CLASS_CONSTANT */` |
|      4 |  9512 | `	rc = ReflectBuildAttrs(pCtx, &sRef.pAttr->aAttrs, "attr", &sTarget,` |
|      1 |  9513 | `		sRef.zName, sRef.nName, 0, 16, nArg, apArg);` |
|      3 |  9514 | `	PH7_MemObjRelease(&sTarget);` |
|      3 |  9515 | `	return rc;` |
|      2 |  9516 | `}` |
|      6 |  9517 | `static int vm_builtin_ReflectionClassConstant_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9518 | `{` |
|      3 |  9519 | `	SXUNUSED(nArg);` |
|      3 |  9520 | `	SXUNUSED(apArg);` |
|      7 |  9521 | `	return ReflectExportConstSelf(pCtx);` |
|      1 |  9522 | `}` |
|      - |  9523 | `/*` |
|      - |  9524 | ` * Declare chunk 3. Called from PH7_VmInstallReflection() where it used to be` |
|      - |  9525 | ` * compiled — after chunks 1 and 2, whose ReflectionClass and ReflectionMethod` |
|      - |  9526 | ` * these answer, and after PropertyHookType, which hasHook()/getHook() declare.` |
|      - |  9527 | ` *` |
|      - |  9528 | ` * The method tables are in php's own DECLARATION order.` |
|      - |  9529 | ` */` |
|   7925 |  9530 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionMember(ph7_vm *pVm)` |
|      5 |  9531 | `{` |
|      - |  9532 | `	static const PH7_NativePropDef aMemberProp[] = {` |
|      - |  9533 | `		{ "name",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  9534 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  9535 | `	};` |
|      - |  9536 | `	static const PH7_NativePropDef aPropProp[] = {` |
|      - |  9537 | `		{ "name",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  9538 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  9539 | `		/* PHL-only: the instance a DYNAMIC property was reached through (recorded) */` |
|      - |  9540 | `		{ RP_DYNOBJ, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  9541 | `	};` |
|      - |  9542 | `	static const PH7_NativeConstDef aPropConst[] = {` |
|      - |  9543 | `		{ "IS_STATIC",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,   0, 0.0 },` |
|      - |  9544 | `		{ "IS_READONLY",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 128,  0, 0.0 },` |
|      - |  9545 | `		{ "IS_PUBLIC",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,    0, 0.0 },` |
|      - |  9546 | `		{ "IS_PROTECTED",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,    0, 0.0 },` |
|      - |  9547 | `		{ "IS_PRIVATE",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,    0, 0.0 },` |
|      - |  9548 | `		{ "IS_ABSTRACT",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64,   0, 0.0 },` |
|      - |  9549 | `		{ "IS_PROTECTED_SET", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2048, 0, 0.0 },` |
|      - |  9550 | `		{ "IS_PRIVATE_SET",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4096, 0, 0.0 },` |
|      - |  9551 | `		{ "IS_VIRTUAL",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 512,  0, 0.0 },` |
|      - |  9552 | `		{ "IS_FINAL",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32,   0, 0.0 },` |
|      - |  9553 | `	};` |
|      - |  9554 | `	static const PH7_NativeMethodDef aPropMethod[] = {` |
|      - |  9555 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - |  9556 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $property", "",` |
|      - |  9557 | `		  vm_builtin_ReflectionProperty_construct },` |
|      - |  9558 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionProperty_toString },` |
|      - |  9559 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionProperty_getName },` |
|      - |  9560 | `		{ "getMangledName", PH7_MOD_PUBLIC, "", "string",` |
|      - |  9561 | `		  vm_builtin_ReflectionProperty_getMangledName },` |
|      - |  9562 | `		{ "getValue",    PH7_MOD_PUBLIC, "?object $object = null", "@mixed",` |
|      - |  9563 | `		  vm_builtin_ReflectionProperty_getValue },` |
|      - |  9564 | `		{ "setValue",    PH7_MOD_PUBLIC, "mixed $objectOrValue, mixed $value = ?", "@void",` |
|      - |  9565 | `		  vm_builtin_ReflectionProperty_setValue },` |
|      - |  9566 | `		{ "getRawValue", PH7_MOD_PUBLIC, "object $object", "mixed",` |
|      - |  9567 | `		  vm_builtin_ReflectionProperty_getValue },` |
|      - |  9568 | `		{ "setRawValue", PH7_MOD_PUBLIC, "object $object, mixed $value", "void",` |
|      - |  9569 | `		  vm_builtin_ReflectionProperty_setValue },` |
|      - |  9570 | `		/* On a non-lazy object — the only kind PHL has — php's two lazy writers` |
|      - |  9571 | `		 * are an ordinary raw write and a no-op. */` |
|      - |  9572 | `		{ "setRawValueWithoutLazyInitialization", PH7_MOD_PUBLIC,` |
|      - |  9573 | `		  "object $object, mixed $value", "void", vm_builtin_ReflectionProperty_setValue },` |
|      - |  9574 | `		{ "skipLazyInitialization", PH7_MOD_PUBLIC, "object $object", "void",` |
|      - |  9575 | `		  vm_builtin_ReflectionProperty_noop },` |
|      - |  9576 | `		{ "isLazy",      PH7_MOD_PUBLIC, "object $object", "bool",` |
|      - |  9577 | `		  vm_builtin_ReflectionProperty_false },` |
|      - |  9578 | `		{ "isInitialized", PH7_MOD_PUBLIC, "?object $object = null", "@bool",` |
|      - |  9579 | `		  vm_builtin_ReflectionProperty_isInitialized },` |
|      - |  9580 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isPublic },` |
|      - |  9581 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isPrivate },` |
|      - |  9582 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isProtected },` |
|      - |  9583 | `		{ "isPrivateSet", PH7_MOD_PUBLIC, "", "bool",` |
|      - |  9584 | `		  vm_builtin_ReflectionProperty_isPrivateSet },` |
|      - |  9585 | `		{ "isProtectedSet", PH7_MOD_PUBLIC, "", "bool",` |
|      - |  9586 | `		  vm_builtin_ReflectionProperty_isProtectedSet },` |
|      - |  9587 | `		{ "isStatic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isStatic },` |
|      - |  9588 | `		{ "isReadOnly",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isReadOnly },` |
|      - |  9589 | `		{ "isDefault",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isDefault },` |
|      - |  9590 | `		{ "isDynamic",   PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isDynamic },` |
|      - |  9591 | `		/* PHL has no abstract properties: the modifier only exists on an` |
|      - |  9592 | `		 * interface's hooked property stub, which PHL does not model. */` |
|      - |  9593 | `		{ "isAbstract",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_false },` |
|      - |  9594 | `		{ "isVirtual",   PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isVirtual },` |
|      - |  9595 | `		{ "isPromoted",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isPromoted },` |
|      - |  9596 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionProperty_getModifiers },` |
|      - |  9597 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|      - |  9598 | `		  vm_builtin_ReflectionProperty_getDeclaringClass },` |
|      - |  9599 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionProperty_getDocComment },` |
|      - |  9600 | `		{ "setAccessible", PH7_MOD_PUBLIC, "bool $accessible", "@void",` |
|      - |  9601 | `		  vm_builtin_ReflectionProperty_setAccessible },` |
|      - |  9602 | `		{ "getType",     PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionProperty_getType },` |
|      - |  9603 | `		/* php's settable type differs from the declared one only for a hooked` |
|      - |  9604 | ``		 * property with a widening `set` — which PHL does not model. */`` |
|      - |  9605 | `		{ "getSettableType", PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|      - |  9606 | `		  vm_builtin_ReflectionProperty_getType },` |
|      - |  9607 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_hasType },` |
|      - |  9608 | `		{ "hasDefaultValue", PH7_MOD_PUBLIC, "", "bool",` |
|      - |  9609 | `		  vm_builtin_ReflectionProperty_hasDefaultValue },` |
|      - |  9610 | `		{ "getDefaultValue", PH7_MOD_PUBLIC, "", "@mixed",` |
|      - |  9611 | `		  vm_builtin_ReflectionProperty_getDefaultValue },` |
|      - |  9612 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - |  9613 | `		  vm_builtin_ReflectionProperty_getAttributes },` |
|      - |  9614 | `		{ "hasHooks",    PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_hasHooks },` |
|      - |  9615 | `		{ "getHooks",    PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionProperty_getHooks },` |
|      - |  9616 | `		{ "hasHook",     PH7_MOD_PUBLIC, "PropertyHookType $type", "bool",` |
|      - |  9617 | `		  vm_builtin_ReflectionProperty_hasHook },` |
|      - |  9618 | `		{ "getHook",     PH7_MOD_PUBLIC, "PropertyHookType $type", "?ReflectionMethod",` |
|      - |  9619 | `		  vm_builtin_ReflectionProperty_getHook },` |
|      - |  9620 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isFinal },` |
|      - |  9621 | `	};` |
|      - |  9622 | `	static const PH7_NativeConstDef aConstConst[] = {` |
|      - |  9623 | `		{ "IS_PUBLIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,  0, 0.0 },` |
|      - |  9624 | `		{ "IS_PROTECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,  0, 0.0 },` |
|      - |  9625 | `		{ "IS_PRIVATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,  0, 0.0 },` |
|      - |  9626 | `		{ "IS_FINAL",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|      - |  9627 | `	};` |
|      - |  9628 | `	static const PH7_NativeMethodDef aConstMethod[] = {` |
|      - |  9629 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - |  9630 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|      - |  9631 | `		  vm_builtin_ReflectionClassConstant_construct },` |
|      - |  9632 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string",` |
|      - |  9633 | `		  vm_builtin_ReflectionClassConstant_toString },` |
|      - |  9634 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionProperty_getName },` |
|      - |  9635 | `		{ "getValue",    PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_ReflectionClassConstant_getValue },` |
|      - |  9636 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClassConstant_isPublic },` |
|      - |  9637 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClassConstant_isPrivate },` |
|      - |  9638 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool",` |
|      - |  9639 | `		  vm_builtin_ReflectionClassConstant_isProtected },` |
|      - |  9640 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClassConstant_isFinal },` |
|      - |  9641 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int",` |
|      - |  9642 | `		  vm_builtin_ReflectionClassConstant_getModifiers },` |
|      - |  9643 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|      - |  9644 | `		  vm_builtin_ReflectionClassConstant_getDeclaringClass },` |
|      - |  9645 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false",` |
|      - |  9646 | `		  vm_builtin_ReflectionClassConstant_getDocComment },` |
|      - |  9647 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - |  9648 | `		  vm_builtin_ReflectionClassConstant_getAttributes },` |
|      - |  9649 | `		{ "isEnumCase",  PH7_MOD_PUBLIC, "", "bool",` |
|      - |  9650 | `		  vm_builtin_ReflectionClassConstant_isEnumCase },` |
|      - |  9651 | `		{ "isDeprecated", PH7_MOD_PUBLIC, "", "bool",` |
|      - |  9652 | `		  vm_builtin_ReflectionClassConstant_isDeprecated },` |
|      - |  9653 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClassConstant_hasType },` |
|      - |  9654 | `		{ "getType",     PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|      - |  9655 | `		  vm_builtin_ReflectionClassConstant_getType },` |
|      - |  9656 | `	};` |
|      - |  9657 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  9658 | `		{ "ReflectionProperty", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  9659 | `		  aPropMethod, SX_ARRAYSIZE(aPropMethod),` |
|      - |  9660 | `		  aPropConst, SX_ARRAYSIZE(aPropConst),` |
|      - |  9661 | `		  aPropProp, SX_ARRAYSIZE(aPropProp), 0, 0, 0 },` |
|      - |  9662 | `		{ "ReflectionClassConstant", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  9663 | `		  aConstMethod, SX_ARRAYSIZE(aConstMethod),` |
|      - |  9664 | `		  aConstConst, SX_ARRAYSIZE(aConstConst),` |
|      - |  9665 | `		  aMemberProp, SX_ARRAYSIZE(aMemberProp), 0, 0, 0 },` |
|      - |  9666 | `	};` |
|   7930 |  9667 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  9668 | `}` |
|      - |  9669 | `/*` |
|      - |  9670 | ` * ---------------------------------------------------------------------------` |
|      - |  9671 | ` * The ReflectionEnum family — chunk 6, and the end of __phl_rcinfo.` |
|      - |  9672 | ` *` |
|      - |  9673 | `` * Three classes that are very nearly their parents: `ReflectionEnum` IS a`` |
|      - |  9674 | `` * `ReflectionClass` that refuses a non-enum, and `ReflectionEnumUnitCase` IS a`` |
|      - |  9675 | `` * `ReflectionClassConstant` that refuses a constant which is not a case. What`` |
|      - |  9676 | ` * is their own is the CASE list, which the engine already holds in declaration` |
|      - |  9677 | `` * order on `ph7_class::aEnumCases` — the chunk read a marshalled copy of it out`` |
|      - |  9678 | `` * of `__phl_rcinfo`, the descriptor builder that dies with this conversion.`` |
|      - |  9679 | ` * ---------------------------------------------------------------------------` |
|      - |  9680 | ` */` |
|      - |  9681 |  |
|      - |  9682 | `/* The enum case named zName, or NULL. A case is a class CONSTANT, so the match` |
|      - |  9683 | `` * is case-SENSITIVE: php's hasCase('hearts') is false for `case Hearts`. */`` |
|     28 |  9684 | `static ph7_class_attr * ReflectEnumCase(ph7_class *pClass, const char *zName, int nName)` |
|      1 |  9685 | `{` |
|     29 |  9686 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      - |  9687 | `	sxu32 n;` |
|     29 |  9688 | `	if( nName < 1 ){` |
|    ! 0 |  9689 | `		return 0;` |
|      - |  9690 | `	}` |
|     67 |  9691 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     52 |  9692 | `		if( (int)SyStringLength(&apCase[n]->sName) == nName` |
|     38 |  9693 | `		 && SyMemcmp(SyStringData(&apCase[n]->sName), zName, (sxu32)nName) == 0 ){` |
|     15 |  9694 | `			return apCase[n];` |
|      - |  9695 | `		}` |
|     20 |  9696 | `	}` |
|     15 |  9697 | `	return 0;` |
|     15 |  9698 | `}` |
|      - |  9699 | `/*` |
|      - |  9700 | ` * One case reflector for an already-validated case. php answers a BackedCase` |
|      - |  9701 | ` * for a backed enum and a UnitCase for a pure one, and both carry the same two` |
|      - |  9702 | ` * slots ReflectionClassConstant does — an enum cannot extend anything, so the` |
|      - |  9703 | ` * declaring class is always the enum itself.` |
|      - |  9704 | ` */` |
|     46 |  9705 | `static ph7_class_instance * ReflectEnumCaseNew(ph7_context *pCtx, ph7_class *pEnum,` |
|      - |  9706 | `	const SyString *pCase)` |
|      1 |  9707 | `{` |
|     47 |  9708 | `	ph7_vm *pVm = pCtx->pVm;` |
|     70 |  9709 | `	const char *zClass = pEnum->nEnumBacking` |
|     23 |  9710 | `		? "ReflectionEnumBackedCase" : "ReflectionEnumUnitCase";` |
|     47 |  9711 | `	ph7_class *pRC = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|     47 |  9712 | `	ph7_class_instance *pObj = pRC ? PH7_NewClassInstance(pVm, pRC) : 0;` |
|     47 |  9713 | `	if( pObj == 0 ){` |
|    ! 0 |  9714 | `		return 0;` |
|      - |  9715 | `	}` |
|     70 |  9716 | `	PH7_NativeSetAttrStr(pVm, pObj, "class", SyStringData(&pEnum->sName),` |
|     46 |  9717 | `		(int)SyStringLength(&pEnum->sName));` |
|     47 |  9718 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(pCase), (int)SyStringLength(pCase));` |
|     47 |  9719 | `	return pObj;` |
|     24 |  9720 | `}` |
|      - |  9721 | `/* ReflectionEnum::__construct(object\|string $objectOrClass) */` |
|     30 |  9722 | `static int vm_builtin_ReflectionEnum_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9723 | `{` |
|      - |  9724 | `	ph7_class *pClass;` |
|     31 |  9725 | `	sxi32 rc = vm_builtin_ReflectionClass_construct(pCtx, nArg, apArg);` |
|     31 |  9726 | `	if( rc != PH7_OK ){` |
|      5 |  9727 | `		return rc; /* "Class %s does not exist", already php's */` |
|      - |  9728 | `	}` |
|     27 |  9729 | `	pClass = ReflectClassOf(pCtx);` |
|     27 |  9730 | `	if( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     10 |  9731 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 |  9732 | `			"Class \"%z\" is not an enum", &pClass->sDisp);` |
|      - |  9733 | `	}` |
|     21 |  9734 | `	return PH7_OK;` |
|     16 |  9735 | `}` |
|     12 |  9736 | `static int vm_builtin_ReflectionEnum_hasCase(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9737 | `{` |
|     13 |  9738 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  9739 | `	const char *zName;` |
|      - |  9740 | `	int nName;` |
|     13 |  9741 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  9742 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  9743 | `		return PH7_OK;` |
|      - |  9744 | `	}` |
|     13 |  9745 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     13 |  9746 | `	ph7_result_bool(pCtx, ReflectEnumCase(pClass, zName, nName) != 0);` |
|     13 |  9747 | `	return PH7_OK;` |
|      7 |  9748 | `}` |
|      - |  9749 | `/*` |
|      - |  9750 | ` * getCase(): php tells the two failures apart — a name that is a constant but` |
|      - |  9751 | ` * not a case is "X::K is not a case", one that is neither is "Case X::K does` |
|      - |  9752 | ` * not exist". The chunk answered the second for both.` |
|      - |  9753 | ` */` |
|     16 |  9754 | `static int vm_builtin_ReflectionEnum_getCase(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9755 | `{` |
|     17 |  9756 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  9757 | `	ph7_class_attr *pCase;` |
|      - |  9758 | `	const char *zName;` |
|      - |  9759 | `	int nName;` |
|     17 |  9760 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  9761 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9762 | `		return PH7_OK;` |
|      - |  9763 | `	}` |
|     17 |  9764 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     17 |  9765 | `	pCase = ReflectEnumCase(pClass, zName, nName);` |
|     17 |  9766 | `	if( pCase == 0 ){` |
|      7 |  9767 | `		if( nName > 0 && SyHashGet(&pClass->hConst, (const void *)zName, (sxu32)nName) ){` |
|      4 |  9768 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  9769 | `				"%z::%.*s is not a case", &pClass->sDisp, nName, zName);` |
|      - |  9770 | `		}` |
|      7 |  9771 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 |  9772 | `			"Case %z::%.*s does not exist", &pClass->sDisp, nName, zName);` |
|      - |  9773 | `	}` |
|     11 |  9774 | `	return ReflectResultObject(pCtx, ReflectEnumCaseNew(pCtx, pClass, &pCase->sName));` |
|      9 |  9775 | `}` |
|     14 |  9776 | `static int vm_builtin_ReflectionEnum_getCases(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9777 | `{` |
|     15 |  9778 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     15 |  9779 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      7 |  9780 | `	SXUNUSED(nArg);` |
|      7 |  9781 | `	SXUNUSED(apArg);` |
|     15 |  9782 | `	if( pOut == 0 ){` |
|    ! 0 |  9783 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  9784 | `	}` |
|     15 |  9785 | `	if( pClass ){` |
|     15 |  9786 | `		ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      - |  9787 | `		sxu32 n;` |
|     51 |  9788 | `		for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     37 |  9789 | `			ReflectTypeListAdd(pCtx, pOut, ReflectEnumCaseNew(pCtx, pClass, &apCase[n]->sName));` |
|     19 |  9790 | `		}` |
|      7 |  9791 | `	}` |
|     15 |  9792 | `	ph7_result_value(pCtx, pOut);` |
|     15 |  9793 | `	return PH7_OK;` |
|      8 |  9794 | `}` |
|     12 |  9795 | `static int vm_builtin_ReflectionEnum_isBacked(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9796 | `{` |
|     13 |  9797 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      6 |  9798 | `	SXUNUSED(nArg);` |
|      6 |  9799 | `	SXUNUSED(apArg);` |
|     13 |  9800 | `	ph7_result_bool(pCtx, pClass != 0 && pClass->nEnumBacking != 0);` |
|     13 |  9801 | `	return PH7_OK;` |
|      1 |  9802 | `}` |
|     16 |  9803 | `static int vm_builtin_ReflectionEnum_getBackingType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9804 | `{` |
|     17 |  9805 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  9806 | `	const char *zText;` |
|      - |  9807 | `	ph7_class_instance *pType;` |
|      8 |  9808 | `	SXUNUSED(nArg);` |
|      8 |  9809 | `	SXUNUSED(apArg);` |
|     17 |  9810 | `	if( pClass == 0 \|\| pClass->nEnumBacking == 0 ){` |
|      5 |  9811 | `		ph7_result_null(pCtx); /* a PURE enum has no backing type */` |
|      5 |  9812 | `		return PH7_OK;` |
|      - |  9813 | `	}` |
|     13 |  9814 | `	zText = (pClass->nEnumBacking & MEMOBJ_INT) ? "int" : "string";` |
|     13 |  9815 | `	pType = ReflectMakeType(pCtx, zText, (int)SyStrlen(zText));` |
|     13 |  9816 | `	if( pType == 0 ){` |
|    ! 0 |  9817 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9818 | `		return PH7_OK;` |
|      - |  9819 | `	}` |
|     13 |  9820 | `	PH7_NativeResultObject(pCtx, pType);` |
|     13 |  9821 | `	return PH7_OK;` |
|      9 |  9822 | `}` |
|      - |  9823 | `/*` |
|      - |  9824 | ` * ReflectionEnumUnitCase::__construct(object\|string $class, string $constant).` |
|      - |  9825 | ` *` |
|      - |  9826 | ` * The parent raises for a constant that does not exist; what is left is "not a` |
|      - |  9827 | ` * case", and php words that one WITHOUT asking whether the class is an enum at` |
|      - |  9828 | `` * all — `new ReflectionEnumUnitCase('Plain', 'K')` on an ordinary class says`` |
|      - |  9829 | `` * `Constant Plain::K is not a case`, not "is not an enum" as the chunk did.`` |
|      - |  9830 | ` * A non-enum's constant can never carry the ENUMCASE bit, so the one screen` |
|      - |  9831 | ` * answers both shapes.` |
|      - |  9832 | ` */` |
|     18 |  9833 | `static int vm_builtin_ReflectionEnumCase_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9834 | `{` |
|      - |  9835 | `	ReflectMemberRef sRef;` |
|     19 |  9836 | `	sxi32 rc = vm_builtin_ReflectionClassConstant_construct(pCtx, nArg, apArg);` |
|     19 |  9837 | `	if( rc != PH7_OK ){` |
|      3 |  9838 | `		return rc;` |
|      - |  9839 | `	}` |
|     17 |  9840 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9841 | `		return PH7_OK;` |
|      - |  9842 | `	}` |
|     17 |  9843 | `	if( (sRef.pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){` |
|     10 |  9844 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      6 |  9845 | `			"Constant %z::%.*s is not a case", &sRef.pClass->sDisp, sRef.nName, sRef.zName);` |
|      - |  9846 | `	}` |
|     11 |  9847 | `	return PH7_OK;` |
|     10 |  9848 | `}` |
|      - |  9849 | `/* ReflectionEnumBackedCase::__construct() — the same, plus php's screen for a` |
|      - |  9850 | ` * PURE enum's case, which has no backing value to answer with. */` |
|      6 |  9851 | `static int vm_builtin_ReflectionEnumBackedCase_construct(ph7_context *pCtx, int nArg,` |
|      - |  9852 | `	ph7_value **apArg)` |
|      1 |  9853 | `{` |
|      - |  9854 | `	ReflectMemberRef sRef;` |
|      7 |  9855 | `	sxi32 rc = vm_builtin_ReflectionEnumCase_construct(pCtx, nArg, apArg);` |
|      7 |  9856 | `	if( rc != PH7_OK ){` |
|    ! 0 |  9857 | `		return rc;` |
|      - |  9858 | `	}` |
|      7 |  9859 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9860 | `		return PH7_OK;` |
|      - |  9861 | `	}` |
|      7 |  9862 | `	if( sRef.pClass->nEnumBacking == 0 ){` |
|      4 |  9863 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 |  9864 | `			"Enum case %z::%.*s is not a backed case", &sRef.pClass->sDisp,` |
|      1 |  9865 | `			sRef.nName, sRef.zName);` |
|      - |  9866 | `	}` |
|      5 |  9867 | `	return PH7_OK;` |
|      4 |  9868 | `}` |
|      6 |  9869 | `static int vm_builtin_ReflectionEnumCase_getEnum(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9870 | `{` |
|      7 |  9871 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  9872 | `	ReflectMemberRef sRef;` |
|      - |  9873 | `	ph7_class *pRC;` |
|      - |  9874 | `	ph7_class_instance *pObj;` |
|      3 |  9875 | `	SXUNUSED(nArg);` |
|      3 |  9876 | `	SXUNUSED(apArg);` |
|      7 |  9877 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) ){` |
|    ! 0 |  9878 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9879 | `		return PH7_OK;` |
|      - |  9880 | `	}` |
|      7 |  9881 | `	pRC = PH7_VmExtractClass(pVm, "ReflectionEnum", sizeof("ReflectionEnum")-1, FALSE, 0);` |
|      7 |  9882 | `	pObj = pRC ? PH7_NewClassInstance(pVm, pRC) : 0;` |
|      7 |  9883 | `	if( pObj == 0 ){` |
|    ! 0 |  9884 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9885 | `		return PH7_OK;` |
|      - |  9886 | `	}` |
|     10 |  9887 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(&sRef.pClass->sName),` |
|      6 |  9888 | `		(int)SyStringLength(&sRef.pClass->sName));` |
|      7 |  9889 | `	return ReflectResultObject(pCtx, pObj);` |
|      4 |  9890 | `}` |
|      - |  9891 | ``/* getBackingValue(): the `value` the case singleton carries. The singleton is`` |
|      - |  9892 | ` * the constant's own value, so the parent's accessor materializes it. */` |
|     16 |  9893 | `static int vm_builtin_ReflectionEnumCase_getBackingValue(ph7_context *pCtx, int nArg,` |
|      - |  9894 | `	ph7_value **apArg)` |
|      1 |  9895 | `{` |
|      - |  9896 | `	ReflectMemberRef sRef;` |
|      - |  9897 | `	ph7_value *pVal, *pBacking;` |
|      - |  9898 | `	sxi32 rc;` |
|      8 |  9899 | `	SXUNUSED(nArg);` |
|      8 |  9900 | `	SXUNUSED(apArg);` |
|     17 |  9901 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9902 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9903 | `		return PH7_OK;` |
|      - |  9904 | `	}` |
|     17 |  9905 | `	rc = ReflectConstSlot(pCtx, sRef.pClass, sRef.pAttr, &pVal);` |
|     17 |  9906 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  9907 | `		return rc;` |
|      - |  9908 | `	}` |
|     17 |  9909 | `	pBacking = (pVal && (pVal->iFlags & MEMOBJ_OBJ))` |
|     24 |  9910 | `		? PH7_NativeAttr((ph7_class_instance *)pVal->x.pOther, "value") : 0;` |
|     17 |  9911 | `	if( pBacking ){` |
|     17 |  9912 | `		ph7_result_value(pCtx, pBacking);` |
|      9 |  9913 | `	}else{` |
|    ! 0 |  9914 | `		ph7_result_null(pCtx);` |
|      - |  9915 | `	}` |
|     17 |  9916 | `	return PH7_OK;` |
|      9 |  9917 | `}` |
|      - |  9918 | `/*` |
|      - |  9919 | ` * Declare the three. Called from PH7_VmInstallReflection() where chunk 6 used` |
|      - |  9920 | ` * to be compiled, so both parents are installed (PH7_ClassInherit COPIES a` |
|      - |  9921 | ` * base's methods down — a native subclass needs its parent declared first).` |
|      - |  9922 | ` *` |
|      - |  9923 | ` * The uncloneable/unserializable flags are php's answer for each class and do` |
|      - |  9924 | ``  * NOT come down with the inheritance: without them `clone $enumReflector` `` |
|      - |  9925 | ` * reached the private __clone it inherited and reported that instead.` |
|      - |  9926 | ` */` |
|   7925 |  9927 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionEnum(ph7_vm *pVm)` |
|      5 |  9928 | `{` |
|      - |  9929 | `	static const PH7_NativeMethodDef aEnumMethod[] = {` |
|      - |  9930 | `		{ "__construct",    PH7_MOD_PUBLIC, "object\|string $objectOrClass", "",` |
|      - |  9931 | `		  vm_builtin_ReflectionEnum_construct },` |
|      - |  9932 | `		{ "hasCase",        PH7_MOD_PUBLIC, "string $name", "bool",` |
|      - |  9933 | `		  vm_builtin_ReflectionEnum_hasCase },` |
|      - |  9934 | `		{ "getCase",        PH7_MOD_PUBLIC, "string $name", "ReflectionEnumUnitCase",` |
|      - |  9935 | `		  vm_builtin_ReflectionEnum_getCase },` |
|      - |  9936 | `		{ "getCases",       PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionEnum_getCases },` |
|      - |  9937 | `		{ "isBacked",       PH7_MOD_PUBLIC, "", "bool",  vm_builtin_ReflectionEnum_isBacked },` |
|      - |  9938 | `		{ "getBackingType", PH7_MOD_PUBLIC, "", "?ReflectionNamedType",` |
|      - |  9939 | `		  vm_builtin_ReflectionEnum_getBackingType },` |
|      - |  9940 | `	};` |
|      - |  9941 | `	static const PH7_NativeMethodDef aCaseMethod[] = {` |
|      - |  9942 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|      - |  9943 | `		  vm_builtin_ReflectionEnumCase_construct },` |
|      - |  9944 | `		{ "getEnum",     PH7_MOD_PUBLIC, "", "ReflectionEnum",` |
|      - |  9945 | `		  vm_builtin_ReflectionEnumCase_getEnum },` |
|      - |  9946 | `		/* php redeclares getValue() on the case reflector for its narrower` |
|      - |  9947 | `		 * return type; the body is the parent's. */` |
|      - |  9948 | `		{ "getValue",    PH7_MOD_PUBLIC, "", "UnitEnum",` |
|      - |  9949 | `		  vm_builtin_ReflectionClassConstant_getValue },` |
|      - |  9950 | `	};` |
|      - |  9951 | `	static const PH7_NativeMethodDef aBackedMethod[] = {` |
|      - |  9952 | `		{ "__construct",     PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|      - |  9953 | `		  vm_builtin_ReflectionEnumBackedCase_construct },` |
|      - |  9954 | `		{ "getBackingValue", PH7_MOD_PUBLIC, "", "string\|int",` |
|      - |  9955 | `		  vm_builtin_ReflectionEnumCase_getBackingValue },` |
|      - |  9956 | `	};` |
|      - |  9957 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  9958 | `		{ "ReflectionEnum", "ReflectionClass", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  9959 | `		  aEnumMethod, SX_ARRAYSIZE(aEnumMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  9960 | `		{ "ReflectionEnumUnitCase", "ReflectionClassConstant", 0,` |
|      - |  9961 | `		  PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  9962 | `		  aCaseMethod, SX_ARRAYSIZE(aCaseMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  9963 | `		{ "ReflectionEnumBackedCase", "ReflectionEnumUnitCase", 0,` |
|      - |  9964 | `		  PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  9965 | `		  aBackedMethod, SX_ARRAYSIZE(aBackedMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  9966 | `	};` |
|   7930 |  9967 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  9968 | `}` |
|      - |  9969 | `/*` |
|      - |  9970 | ` * ---------------------------------------------------------------------------` |
|      - |  9971 | ` * php's Reflection EXPORT format — chunk 9, the last of the library's PHP.` |
|      - |  9972 | ` *` |
|      - |  9973 | ` * Every Reflector's __toString(). It is a byte-exact format with no API of its` |
|      - |  9974 | ` * own, so the chunk was written against the PUBLIC reflection API of whatever` |
|      - |  9975 | ` * it was printing — the only thing PHP could reach. All of those targets are C` |
|      - |  9976 | ` * now, so this reads the engine directly: the member walk for order and` |
|      - |  9977 | ` * visibility, ReflectParamAt for parameters, ReflectExportValue (built for the` |
|      - |  9978 | ` * attribute-argument block) for every value.` |
|      - |  9979 | ` *` |
|      - |  9980 | ` * Defined at the END of the file because it needs all of that; the five entry` |
|      - |  9981 | ` * points are forward-declared above their callers.` |
|      - |  9982 | ` * ---------------------------------------------------------------------------` |
|      - |  9983 | ` */` |
|      - |  9984 |  |
|      - |  9985 | `/*` |
|      - |  9986 | ` * "internal:<extension>" / "user" — the first tag of every function and class` |
|      - |  9987 | `` * head. php NAMES the extension there (`<internal:json>`), and this printed`` |
|      - |  9988 | `` * `internal:Core` for all 878 functions and 197 classes because the engine had`` |
|      - |  9989 | ` * no partition to name one from. iExt < 0 is a userland target.` |
|      - |  9990 | ` */` |
|   4856 |  9991 | `static void ReflectExportKind(SyBlob *pOut, int bInternal, int iExt, int bDeprecated)` |
|      4 |  9992 | `{` |
|      - |  9993 | `	/* The two questions are separate: an INTERNAL target may still name no module` |
|      - |  9994 | ``	 * (php's fabricated `Closure::__invoke` prints `<internal>` with nothing after`` |
|      - |  9995 | `	 * it), so the extension is only appended when there is one. */` |
|   4860 |  9996 | `	SyBlobAppend(pOut, bInternal ? "internal" : "user", bInternal ? 8 : 4);` |
|      - |  9997 | `	/* php writes the three parts in this order and each on its own condition,` |
|      - |  9998 | ``	 * so a deprecated INTERNAL name reads `<internal, deprecated:curl>` -- the`` |
|      - |  9999 | ``	 * extension hangs off `deprecated`, not off `internal`. */`` |
|   4860 | 10000 | `	if( bDeprecated ){` |
|     92 | 10001 | `		SyBlobAppend(pOut, ", deprecated", sizeof(", deprecated")-1);` |
|     45 | 10002 | `	}` |
|   4860 | 10003 | `	if( bInternal && iExt >= 0 ){` |
|   4724 | 10004 | `		SyBlobFormat(pOut, ":%s", PH7_VmExtensionName(iExt));` |
|   2360 | 10005 | `	}` |
|   4860 | 10006 | `}` |
|      - | 10007 | `/*` |
|      - | 10008 | ` * A declared type followed by a space, or nothing at all.` |
|      - | 10009 | ` *` |
|      - | 10010 | `` * php's EXPORT spells a standalone `iterable` as the two types it stands for`` |
|      - | 10011 | `` * where getType() prints `iterable` -- the one place the two renderings of a`` |
|      - | 10012 | ` * declared type differ (ReflectExportIterable is that difference, named once).` |
|      - | 10013 | ` */` |
|   7504 | 10014 | `static const SyString * ReflectExportIterable(const SyString *pType, SyString *pOut)` |
|      5 | 10015 | `{` |
|   7509 | 10016 | `	const char *z = pType ? SyStringData(pType) : 0;` |
|   7509 | 10017 | `	sxu32 n = z ? SyStringLength(pType) : 0;` |
|   7509 | 10018 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|    ! 0 | 10019 | `		SyStringInitFromBuf(pOut,"Traversable\|array",sizeof("Traversable\|array")-1);` |
|    ! 0 | 10020 | `		return pOut;` |
|      - | 10021 | `	}` |
|   7509 | 10022 | `	if( n == 9 && z[0] == '?' && SyStrnicmp(&z[1],"iterable",8) == 0 ){` |
|    ! 0 | 10023 | `		SyStringInitFromBuf(pOut,"Traversable\|array\|null",sizeof("Traversable\|array\|null")-1);` |
|    ! 0 | 10024 | `		return pOut;` |
|      - | 10025 | `	}` |
|   7509 | 10026 | `	return pType;` |
|   3757 | 10027 | `}` |
|   3322 | 10028 | `static void ReflectExportTypeSp(SyBlob *pOut, const SyString *pType)` |
|      5 | 10029 | `{` |
|      - | 10030 | `	SyString sIter;` |
|   3327 | 10031 | `	pType = ReflectExportIterable(pType,&sIter);` |
|   3327 | 10032 | `	if( pType && SyStringLength(pType) > 0 ){` |
|   2987 | 10033 | `		SyBlobAppend(pOut, SyStringData(pType), SyStringLength(pType));` |
|   2987 | 10034 | `		SyBlobAppend(pOut, " ", sizeof(char));` |
|   1491 | 10035 | `	}` |
|   3327 | 10036 | `}` |
|      - | 10037 | `/* php's visibility word for a member. */` |
|   5108 | 10038 | `static const char * ReflectExportVis(sxi32 iProtection)` |
|      3 | 10039 | `{` |
|   5111 | 10040 | `	if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     53 | 10041 | `		return "private";` |
|      - | 10042 | `	}` |
|   5059 | 10043 | `	return iProtection == PH7_CLASS_PROT_PROTECTED ? "protected" : "public";` |
|   2557 | 10044 | `}` |
|      - | 10045 | `/*` |
|      - | 10046 | `` * The text after `= ` in a parameter default.`` |
|      - | 10047 | ` *` |
|      - | 10048 | ` * php prints a constant-reference default as the CONSTANT's name, not its value` |
|      - | 10049 | `` * (`$f = M_PI`) — that argument was never folded, so php still has the source`` |
|      - | 10050 | `` * expression. `<default>` is what the chunk answered when the value could not`` |
|      - | 10051 | `` * be produced at all, which is a declared `= ?` row in aBuiltinSig[].`` |
|      - | 10052 | ` */` |
|   1296 | 10053 | `static void ReflectExportDefault(ph7_context *pCtx, SyBlob *pOut, ReflectParamDesc *pDesc)` |
|      4 | 10054 | `{` |
|   1300 | 10055 | `	const char *z = 0;` |
|   1300 | 10056 | `	int n = 0;` |
|   1300 | 10057 | `	if( ReflectParamDefConst(pCtx, pDesc, &z, &n) ){` |
|    146 | 10058 | `		SyBlobAppend(pOut, z, (sxu32)n);` |
|    723 | 10059 | `		return;` |
|      - | 10060 | `	}` |
|   1158 | 10061 | `	if( pDesc->pArg ){` |
|      - | 10062 | `		ph7_value sValue;` |
|     57 | 10063 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     57 | 10064 | `		VmLocalExec(pCtx->pVm, &pDesc->pArg->aByteCode, &sValue, FALSE);` |
|      - | 10065 | `		/* php has two spellings for the same default and picks by whether the` |
|      - | 10066 | ``		 * function is INTERNAL: `null` and double quotes there, `NULL` and single`` |
|      - | 10067 | `		 * quotes for a userland one. A builtin written as prelude PHP is internal` |
|      - | 10068 | `		 * to php however this engine chose to implement it, so scandir()'s` |
|      - | 10069 | ``		 * `$context = NULL` and clearstatcache()'s `$filename = ''` were both`` |
|      - | 10070 | `		 * printed in the wrong one. */` |
|     56 | 10071 | `		if( pDesc->bInternal` |
|     32 | 10072 | `		 && (sValue.iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING ){` |
|      4 | 10073 | `			ReflectExportStrQ(pOut, (const char *)SyBlobData(&sValue.sBlob),` |
|      1 | 10074 | `				SyBlobLength(&sValue.sBlob), '"');` |
|     56 | 10075 | `		}else if( pDesc->bInternal && (sValue.iFlags & MEMOBJ_NULL) ){` |
|      3 | 10076 | `			SyBlobAppend(pOut, "null", sizeof("null")-1);` |
|      2 | 10077 | `		}else{` |
|     53 | 10078 | `			ReflectExportValue(pCtx, pOut, &sValue, 0);` |
|      - | 10079 | `		}` |
|     57 | 10080 | `		PH7_MemObjRelease(&sValue);` |
|     57 | 10081 | `		return;` |
|      - | 10082 | `	}` |
|      - | 10083 | `	{` |
|      - | 10084 | `		/* A DECLARED default is signature TEXT: reduce it the way` |
|      - | 10085 | `		 * getDefaultValue() does, and fall back to php's own placeholder.` |
|      - | 10086 | `		 * Only an INTERNAL parameter reaches this branch (a compiled one carries` |
|      - | 10087 | `		 * pArg above), which is exactly the case php prints DOUBLE-quoted. */` |
|   1102 | 10088 | `		const char *zDef = SyStringData(&pDesc->sDefText);` |
|   1102 | 10089 | `		int nDef = (int)SyStringLength(&pDesc->sDefText);` |
|   1102 | 10090 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|   1102 | 10091 | `		if( ReflectSigHas(zDef, nDef, "::", 2) ){` |
|      - | 10092 | `			/* Rule 28's split, on the declared side: php's stub keeps the SOURCE of a` |
|      - | 10093 | `			 * constant-expression default, so the export prints` |
|      - | 10094 | ``			 * `= SplFileInfo::class` and `= A::K \| A::J` verbatim while`` |
|      - | 10095 | `			 * getDefaultValue() answers what they evaluate to. */` |
|     57 | 10096 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|     57 | 10097 | `			return;` |
|      - | 10098 | `		}` |
|   1048 | 10099 | `		if( pVal && ReflectSigGlobalConst(pCtx, zDef, nDef, pVal) ){` |
|      - | 10100 | ``			/* Same split for a GLOBAL constant: `= E_ERROR`, never `= 1`. */`` |
|    ! 0 | 10101 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|    ! 0 | 10102 | `			return;` |
|      - | 10103 | `		}` |
|   1044 | 10104 | `		if( pVal && ReflectSigHas(zDef, nDef, "\|", 1)` |
|    533 | 10105 | `		 && ReflectSigConstExpr(pCtx, zDef, nDef, pVal) ){` |
|      - | 10106 | ``			/* And for the `\|` fold of them, which is the one shape whose VALUE`` |
|      - | 10107 | ``			 * names no constant at all: `SQLITE3_OPEN_READWRITE \|`` |
|      - | 10108 | ``			 * SQLITE3_OPEN_CREATE` prints as itself and answers 6. */`` |
|     16 | 10109 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|     16 | 10110 | `			return;` |
|      - | 10111 | `		}` |
|   1034 | 10112 | `		if( ReflectSigIsSourceExpr(zDef, nDef) ){` |
|      - | 10113 | `			/* The two arithmetic shapes: a RADIX integer and a product. php prints` |
|      - | 10114 | ``			 * `0777` and `2 * 1024 * 1024`, never the 511 and 2097152 they reduce`` |
|      - | 10115 | `			 * to -- and answers those numbers from getDefaultValue() all the same. */` |
|      7 | 10116 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|      7 | 10117 | `			return;` |
|      - | 10118 | `		}` |
|   1028 | 10119 | `		if( pVal && ReflectSigScalar(pCtx, zDef, nDef, pVal) ){` |
|    984 | 10120 | `			if( (pVal->iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING ){` |
|    310 | 10121 | `				ReflectExportStrQ(pOut, (const char *)SyBlobData(&pVal->sBlob),` |
|    102 | 10122 | `					SyBlobLength(&pVal->sBlob), '"');` |
|    208 | 10123 | `				return;` |
|      - | 10124 | `			}` |
|    779 | 10125 | `			if( pVal->iFlags & MEMOBJ_NULL ){` |
|      - | 10126 | `				/* The same internal/user split as the quote character above: php` |
|      - | 10127 | ``				 * spells an INTERNAL parameter's null default `null` and a`` |
|      - | 10128 | ``				 * userland one `NULL` (`= null` for every `?T $x = null` stub row). */`` |
|    363 | 10129 | `				SyBlobAppend(pOut, "null", sizeof("null")-1);` |
|    363 | 10130 | `				return;` |
|      - | 10131 | `			}` |
|    419 | 10132 | `			ReflectExportValue(pCtx, pOut, pVal, 0);` |
|    419 | 10133 | `			return;` |
|      - | 10134 | `		}` |
|     44 | 10135 | `		if( (nDef >= 1 && zDef[0] == '[')` |
|     25 | 10136 | `		 \|\| (nDef >= (int)sizeof("array (")-1` |
|    ! 0 | 10137 | `		  && SyMemcmp(zDef, "array (", sizeof("array (")-1) == 0) ){` |
|     47 | 10138 | `			SyBlobAppend(pOut, "[]", sizeof("[]")-1);` |
|     47 | 10139 | `			return;` |
|      - | 10140 | `		}` |
|      - | 10141 | `	}` |
|    ! 0 | 10142 | `	SyBlobAppend(pOut, "<default>", sizeof("<default>")-1);` |
|    652 | 10143 | `}` |
|      - | 10144 | ``/* `Parameter #0 [ <optional> int &...$name = 5 ]` */`` |
|   3042 | 10145 | `static void ReflectExportParamLine(ph7_context *pCtx, SyBlob *pOut, ReflectParamDesc *pDesc)` |
|      5 | 10146 | `{` |
|   4568 | 10147 | `	SyBlobFormat(pOut, "Parameter #%d [ <%s> ", pDesc->iPos,` |
|   3042 | 10148 | `		pDesc->bOptional ? "optional" : "required");` |
|   3047 | 10149 | `	ReflectExportTypeSp(pOut, &pDesc->sType);` |
|   3047 | 10150 | `	if( pDesc->bByRef ){` |
|     79 | 10151 | `		SyBlobAppend(pOut, "&", sizeof(char));` |
|     38 | 10152 | `	}` |
|   3047 | 10153 | `	if( pDesc->bVariadic ){` |
|     40 | 10154 | `		SyBlobAppend(pOut, "...", sizeof("...")-1);` |
|     19 | 10155 | `	}` |
|   3047 | 10156 | `	SyBlobAppend(pOut, "$", sizeof(char));` |
|   3047 | 10157 | `	SyBlobAppend(pOut, SyStringData(&pDesc->sName), SyStringLength(&pDesc->sName));` |
|   3047 | 10158 | `	if( pDesc->bHasDef ){` |
|   1300 | 10159 | `		SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|   1300 | 10160 | `		ReflectExportDefault(pCtx, pOut, pDesc);` |
|   2398 | 10161 | `	}else if( pDesc->bOptional && !pDesc->bVariadic ){` |
|      - | 10162 | `		/* An OPTIONAL parameter with no default a caller could read still gets` |
|      - | 10163 | ``		 * an `= ` from php -- and php's own placeholder after it, since there is`` |
|      - | 10164 | ``		 * nothing to print. `mt_rand()`'s two bounds and `get_class()`'s`` |
|      - | 10165 | `		 * $object are that shape; a VARIADIC tail is not, because its "default"` |
|      - | 10166 | `		 * is having no further arguments. */` |
|     30 | 10167 | `		SyBlobAppend(pOut, " = <default>", sizeof(" = <default>")-1);` |
|     14 | 10168 | `	}` |
|   3047 | 10169 | `	SyBlobAppend(pOut, " ]", sizeof(" ]")-1);` |
|   3047 | 10170 | `}` |
|      - | 10171 | `/*` |
|      - | 10172 | `` * `Property [ public protected(set) readonly int $x = 5 ]` + newline.`` |
|      - | 10173 | ` *` |
|      - | 10174 | ` * The set-visibility is printed only when it DIFFERS from the get-visibility,` |
|      - | 10175 | `` * which is why a `public readonly` property shows php's implied`` |
|      - | 10176 | `` * `protected(set)` and a `protected readonly` one shows nothing.`` |
|      - | 10177 | ` */` |
|    406 | 10178 | `static void ReflectExportPropLine(ph7_context *pCtx, SyBlob *pOut, ph7_class_attr *pAttr,` |
|      - | 10179 | `	const SyString *pKey)` |
|      2 | 10180 | `{` |
|    408 | 10181 | `	sxi32 iSet = pAttr->iProtection;` |
|    408 | 10182 | `	SyBlobAppend(pOut, "Property [ ", sizeof("Property [ ")-1);` |
|      - | 10183 | `	/* php's modifier MASK implies two bits the declaration never wrote:` |
|      - | 10184 | `	 * private(set) implies final, and readonly implies protected(set). A property` |
|      - | 10185 | `	 * DECLARED final (PHP 8.4) prints the same word, and the two spellings do not` |
|      - | 10186 | `	 * print it twice. */` |
|    408 | 10187 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_FINAL\|PH7_CLASS_ATTR_PRIVATE_SET) ){` |
|      7 | 10188 | `		SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|      3 | 10189 | `	}` |
|    408 | 10190 | `	SyBlobFormat(pOut, "%s ", ReflectExportVis(pAttr->iProtection));` |
|    408 | 10191 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|      5 | 10192 | `		iSet = PH7_CLASS_PROT_PRIVATE;` |
|    405 | 10193 | `	}else if( (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET)` |
|    399 | 10194 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){` |
|     29 | 10195 | `		iSet = PH7_CLASS_PROT_PROTECTED;` |
|     14 | 10196 | `	}` |
|      - | 10197 | `	/* The set-visibility is printed only when it DIFFERS from the get one. */` |
|    408 | 10198 | `	if( iSet != pAttr->iProtection ){` |
|     29 | 10199 | `		SyBlobFormat(pOut, "%s(set) ", ReflectExportVis(iSet));` |
|     14 | 10200 | `	}` |
|    408 | 10201 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|      7 | 10202 | `		SyBlobAppend(pOut, "static ", sizeof("static ")-1);` |
|      3 | 10203 | `	}` |
|    408 | 10204 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|    ! 0 | 10205 | `		SyBlobAppend(pOut, "virtual ", sizeof("virtual ")-1);` |
|    ! 0 | 10206 | `	}` |
|    408 | 10207 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|     29 | 10208 | `		SyBlobAppend(pOut, "readonly ", sizeof("readonly ")-1);` |
|     14 | 10209 | `	}` |
|    408 | 10210 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      - | 10211 | `		char zType[192];` |
|      - | 10212 | `		SyString sType;` |
|    282 | 10213 | `		const char *zT = ReflectMemberTypeText(pCtx->pVm,pAttr,zType,sizeof(zType));` |
|    282 | 10214 | `		SyStringInitFromBuf(&sType,zT,(sxu32)SyStrlen(zT));` |
|    282 | 10215 | `		ReflectExportTypeSp(pOut, &sType);` |
|    140 | 10216 | `	}` |
|    408 | 10217 | `	SyBlobAppend(pOut, "$", sizeof(char));` |
|    408 | 10218 | `	SyBlobAppend(pOut, SyStringData(pKey), SyStringLength(pKey));` |
|    406 | 10219 | `	if( SySetUsed(&pAttr->aByteCode) > 0 \|\| pAttr->pNativeValue` |
|    261 | 10220 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      - | 10221 | `		ph7_value sValue;` |
|    265 | 10222 | `		SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|    265 | 10223 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|    265 | 10224 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     33 | 10225 | `			VmLocalExec(pCtx->pVm, &pAttr->aByteCode, &sValue, FALSE);` |
|    249 | 10226 | `		}else if( pAttr->pNativeValue ){` |
|    231 | 10227 | `			PH7_NativeLiteralValue(pCtx->pVm, pAttr->pNativeValue, &sValue);` |
|    115 | 10228 | `		}` |
|    265 | 10229 | `		ReflectExportValue(pCtx, pOut, &sValue, 0);` |
|    265 | 10230 | `		PH7_MemObjRelease(&sValue);` |
|    132 | 10231 | `	}` |
|      - | 10232 | `	/* A hooked property names its hooks; php lists no method for them. */` |
|    408 | 10233 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET) ){` |
|      3 | 10234 | `		SyBlobAppend(pOut, " {", sizeof(" {")-1);` |
|      3 | 10235 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET ){` |
|      3 | 10236 | `			SyBlobAppend(pOut, " get;", sizeof(" get;")-1);` |
|      1 | 10237 | `		}` |
|      3 | 10238 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET ){` |
|      3 | 10239 | `			SyBlobAppend(pOut, " set;", sizeof(" set;")-1);` |
|      1 | 10240 | `		}` |
|      3 | 10241 | `		SyBlobAppend(pOut, " }", sizeof(" }")-1);` |
|      1 | 10242 | `	}` |
|    408 | 10243 | `	SyBlobAppend(pOut, " ]\n", sizeof(" ]\n")-1);` |
|    408 | 10244 | `}` |
|      - | 10245 | `/*` |
|      - | 10246 | `` * `Constant [ final public int NAME ] { value }` + newline. The TYPE is the`` |
|      - | 10247 | ` * value's, not a declared one, and an object (an enum case) prints as the word` |
|      - | 10248 | ` * Object under its own class name.` |
|      - | 10249 | ` */` |
|    638 | 10250 | `static sxi32 ReflectExportConstLine(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass,` |
|      - | 10251 | `	ph7_class_attr *pAttr, const SyString *pKey)` |
|      2 | 10252 | `{` |
|    640 | 10253 | `	ph7_value *pVal = 0;` |
|    640 | 10254 | `	sxi32 rc = ReflectConstSlot(pCtx, pClass, pAttr, &pVal);` |
|      - | 10255 | `	const char *zType;` |
|    640 | 10256 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10257 | `		return rc;` |
|      - | 10258 | `	}` |
|    640 | 10259 | `	SyBlobAppend(pOut, "Constant [ ", sizeof("Constant [ ")-1);` |
|    640 | 10260 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      7 | 10261 | `		SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|      3 | 10262 | `	}` |
|    640 | 10263 | `	SyBlobFormat(pOut, "%s ", ReflectExportVis(pAttr->iProtection));` |
|    638 | 10264 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|    321 | 10265 | `	 && SyStringLength(&pAttr->sTypeName) > 0 ){` |
|      - | 10266 | `		/* php 8.3's typed class constant prints what it DECLARED; the word below` |
|      - | 10267 | `		 * is what an untyped one's VALUE happens to be. */` |
|      - | 10268 | `		char zDecl[192];` |
|      - | 10269 | `		SyString sDecl;` |
|    ! 0 | 10270 | `		const char *zT = ReflectMemberTypeText(pCtx->pVm,pAttr,zDecl,sizeof(zDecl));` |
|    ! 0 | 10271 | `		SyStringInitFromBuf(&sDecl,zT,(sxu32)SyStrlen(zT));` |
|    ! 0 | 10272 | `		ReflectExportTypeSp(pOut, &sDecl);` |
|    ! 0 | 10273 | `	}else{` |
|    640 | 10274 | `		zType = ReflectExportTypeWord(pVal);` |
|    640 | 10275 | `		if( zType ){` |
|    638 | 10276 | `			SyBlobFormat(pOut, "%s ", zType);` |
|    320 | 10277 | `		}else{` |
|      3 | 10278 | `			SyBlobFormat(pOut, "%z ", &((ph7_class_instance *)pVal->x.pOther)->pClass->sName);` |
|      - | 10279 | `		}` |
|      - | 10280 | `	}` |
|    640 | 10281 | `	SyBlobAppend(pOut, SyStringData(pKey), SyStringLength(pKey));` |
|    640 | 10282 | `	SyBlobAppend(pOut, " ] { ", sizeof(" ] { ")-1);` |
|    640 | 10283 | `	ReflectExportConstValue(pCtx, pOut, pVal);` |
|    640 | 10284 | `	SyBlobAppend(pOut, " }\n", sizeof(" }\n")-1);` |
|    640 | 10285 | `	return SXRET_OK;` |
|    321 | 10286 | `}` |
|      - | 10287 | `/* A native class METHOD as a function reference (ReflectFuncFill's tail, for a` |
|      - | 10288 | ` * method the member walk handed over rather than one a receiver names). */` |
|   3788 | 10289 | `static void ReflectFuncFromMethod(ph7_vm *pVm, ph7_class *pClass, ph7_class_method *pMeth,` |
|      - | 10290 | `	ReflectFuncRef *pOut)` |
|      2 | 10291 | `{` |
|   3790 | 10292 | `	SyZero(pOut, sizeof(*pOut));` |
|   3790 | 10293 | `	pOut->pVm = pVm;` |
|   3790 | 10294 | `	pOut->pClass = pClass;` |
|   3790 | 10295 | `	pOut->pMeth = pMeth;` |
|   3790 | 10296 | `	pOut->pFunc = &pMeth->sFunc;` |
|   3790 | 10297 | `	if( (pOut->pFunc->iFlags & VM_FUNC_NATIVE) && pOut->pFunc->pNative ){` |
|   3752 | 10298 | `		pOut->zSig = pOut->pFunc->pNative->zSig;` |
|   3752 | 10299 | `		if( SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|   3752 | 10300 | `			pOut->zRet = pOut->pFunc->pNative->zRet;` |
|   1875 | 10301 | `		}` |
|   1875 | 10302 | `	}` |
|   3790 | 10303 | `}` |
|      - | 10304 | `/*` |
|      - | 10305 | ` * The Method / Function / Closure block.` |
|      - | 10306 | ` *` |
|      - | 10307 | ` * pOwner is the class being EXPORTED when there is one: php's tags are` |
|      - | 10308 | ` * relative to it — a method it did not declare "inherits" from its declaring` |
|      - | 10309 | ` * class — and the reflector alone cannot say that, because $class is the` |
|      - | 10310 | `` * DECLARING class. `overwrites` is the other direction and needs no owner: the`` |
|      - | 10311 | ` * declaring class's own parent declares the same method.` |
|      - | 10312 | ` */` |
|   4550 | 10313 | `static sxi32 ReflectExportFuncBlock(ph7_context *pCtx, SyBlob *pOut, ReflectFuncRef *pRef,` |
|      - | 10314 | `	const char *zIndent, ph7_class *pOwner)` |
|      4 | 10315 | `{` |
|      - | 10316 | `	SyBlob sBody;` |
|   4554 | 10317 | `	int bInternal = ReflectFuncIsInternal(pRef);` |
|   6784 | 10318 | `	int bDeprecated = ReflectFuncDeprecated(pRef) != 0` |
|   4550 | 10319 | `		\|\| (pRef->pFunc != 0 && ReflectHasDeprecated(&pRef->pFunc->aAttrs));` |
|   4554 | 10320 | `	int nParam = ReflectParamCount(pRef);` |
|   4554 | 10321 | `	const char *zRet = 0;` |
|      - | 10322 | `	char zRetBuf[192];` |
|   4554 | 10323 | `	int nRet = 0, bHasRet, bTentRet = 0;` |
|   4554 | 10324 | `	sxi32 rc = SXRET_OK;` |
|   4554 | 10325 | `	SyBlobInit(&sBody, &pCtx->pVm->sAllocator);` |
|   4554 | 10326 | `	bHasRet = ReflectFuncRetTextEx(pRef, &zRet, &nRet, &bTentRet, zRetBuf, sizeof(zRetBuf));` |
|   4554 | 10327 | `	if( pRef->pMeth ){` |
|   4039 | 10328 | `		ph7_class *pDecl = ReflectFuncDeclClass(pRef);` |
|      - | 10329 | `		ph7_class *pProto;` |
|   4039 | 10330 | `		SyString *pName = &pRef->pMeth->sFunc.sName;` |
|      - | 10331 | `		int bInherits, bCtor;` |
|   4039 | 10332 | `		SyBlobAppend(&sBody, "Method [ <", sizeof("Method [ <")-1);` |
|      - | 10333 | `		/* A method names its DECLARING class's extension, which is why php's` |
|      - | 10334 | ``		 * `JsonException::__construct` prints `<internal:Core, inherits`` |
|      - | 10335 | ``		 * Exception, ctor>` rather than json's own name. */`` |
|   4039 | 10336 | `		ReflectExportKind(&sBody, bInternal, ReflectFuncExtId(pRef), bDeprecated);` |
|   4039 | 10337 | `		bInherits = (pOwner != 0 && pDecl != 0 && pDecl != pOwner);` |
|   4039 | 10338 | `		if( bInherits ){` |
|   1935 | 10339 | `			SyBlobFormat(&sBody, ", inherits %z", &pDecl->sName);` |
|   3072 | 10340 | `		}else if( pDecl ){` |
|   3156 | 10341 | `			ph7_class *pOver = ReflectOverwritesIn(pDecl,` |
|   2102 | 10342 | `				SyStringData(pName), (int)SyStringLength(pName));` |
|   2105 | 10343 | `			if( pOver ){` |
|    165 | 10344 | `				SyBlobFormat(&sBody, ", overwrites %z", &pOver->sName);` |
|     82 | 10345 | `			}` |
|   1051 | 10346 | `		}` |
|   6411 | 10347 | `		bCtor = SyStringLength(pName) == sizeof("__construct")-1` |
|   4036 | 10348 | `			&& SyStrnicmp(SyStringData(pName), "__construct", sizeof("__construct")-1) == 0;` |
|      - | 10349 | `		/* The prototype belongs to the entry in the class the export was` |
|      - | 10350 | `		 * reached THROUGH, which is php's anchor for it: a class that only` |
|      - | 10351 | `		 * inherits a method still re-assigns its prototype when it implements` |
|      - | 10352 | `		 * an interface declaring the same one. */` |
|   2021 | 10353 | `		pProto = ReflectPrototypeOf(pCtx, pOwner ? pOwner : pRef->pClass,` |
|   4036 | 10354 | `			SyStringData(pName), (int)SyStringLength(pName), 0);` |
|   4039 | 10355 | `		if( pProto ){` |
|   1341 | 10356 | `			SyBlobFormat(&sBody, ", prototype %z", &pProto->sName);` |
|    669 | 10357 | `		}` |
|      - | 10358 | `		/* php closes the bracket with the ctor word, AFTER the prototype: an` |
|      - | 10359 | `` 		 * interface's constructor prints `prototype F1, ctor`. There is no `dtor` `` |
|      - | 10360 | `		 * twin -- php's exporter prints the word for ZEND_ACC_CTOR only, so a` |
|      - | 10361 | ``		 * `__destruct` reads `Method [ <user> public method __destruct ]`. */`` |
|   4039 | 10362 | `		if( bCtor ){` |
|    209 | 10363 | `			SyBlobAppend(&sBody, ", ctor", sizeof(", ctor")-1);` |
|    103 | 10364 | `		}` |
|   4039 | 10365 | `		SyBlobAppend(&sBody, "> ", sizeof("> ")-1);` |
|   4039 | 10366 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|    205 | 10367 | `			SyBlobAppend(&sBody, "abstract ", sizeof("abstract ")-1);` |
|    102 | 10368 | `		}` |
|   4039 | 10369 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|    445 | 10370 | `			SyBlobAppend(&sBody, "final ", sizeof("final ")-1);` |
|    222 | 10371 | `		}` |
|   4039 | 10372 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|     72 | 10373 | `			SyBlobAppend(&sBody, "static ", sizeof("static ")-1);` |
|     35 | 10374 | `		}` |
|   4039 | 10375 | `		SyBlobFormat(&sBody, "%s method %z ]", ReflectExportVis(pRef->pMeth->iProtection), pName);` |
|   2021 | 10376 | `	}else{` |
|    775 | 10377 | `		SyBlobAppend(&sBody, pRef->pClosure ? "Closure [ <" : "Function [ <",` |
|    514 | 10378 | `			pRef->pClosure ? sizeof("Closure [ <")-1 : sizeof("Function [ <")-1);` |
|    518 | 10379 | `		ReflectExportKind(&sBody, bInternal, ReflectFuncExtId(pRef), bDeprecated);` |
|    518 | 10380 | `		SyBlobAppend(&sBody, "> function ", sizeof("> function ")-1);` |
|    518 | 10381 | `		if( pRef->pFunc ){` |
|     11 | 10382 | `			SyBlobFormat(&sBody, "%z", &pRef->pFunc->sName);` |
|    513 | 10383 | `		}else if( pRef->pHost ){` |
|    508 | 10384 | `			SyBlobFormat(&sBody, "%z", &pRef->pHost->sName);` |
|    252 | 10385 | `		}` |
|    518 | 10386 | `		SyBlobAppend(&sBody, " ]", sizeof(" ]")-1);` |
|      - | 10387 | `	}` |
|   4554 | 10388 | `	SyBlobAppend(&sBody, " {\n", sizeof(" {\n")-1);` |
|   4554 | 10389 | `	if( !bInternal && pRef->pFunc ){` |
|    103 | 10390 | `		SyBlobAppend(&sBody, "  @@ ", sizeof("  @@ ")-1);` |
|    103 | 10391 | `		if( SyStringLength(&pRef->pFunc->sFile) > 0 ){` |
|    154 | 10392 | `			SyBlobAppend(&sBody, SyStringData(&pRef->pFunc->sFile),` |
|    102 | 10393 | `				SyStringLength(&pRef->pFunc->sFile));` |
|     51 | 10394 | `		}` |
|    103 | 10395 | `		SyBlobFormat(&sBody, " %u - %u\n", pRef->pFunc->nLine, pRef->pFunc->nEndLine);` |
|     51 | 10396 | `	}` |
|      - | 10397 | `	/* php prints the parameter block for every INTERNAL function, and for a` |
|      - | 10398 | `	 * user one only when there is something to say -- with one exception, the` |
|      - | 10399 | ``	 * class-level `Closure::__invoke`: php fabricates it with no parameter`` |
|      - | 10400 | `	 * information at all (not an empty list), and prints neither section. */` |
|   4550 | 10401 | `	if( (nParam > 0 \|\| bHasRet \|\| bInternal)` |
|   4526 | 10402 | `	 && !(pRef->bFabricated && pRef->pClosure == 0) ){` |
|      - | 10403 | `		int n;` |
|   4550 | 10404 | `		SyBlobFormat(&sBody, "\n  - Parameters [%d] {\n", nParam);` |
|   7448 | 10405 | `		for( n = 0 ; n < nParam ; n++ ){` |
|      - | 10406 | `			ReflectParamDesc sDesc;` |
|   2902 | 10407 | `			if( !ReflectParamAt(pRef, n, &sDesc) ){` |
|    ! 0 | 10408 | `				continue;` |
|      - | 10409 | `			}` |
|   2902 | 10410 | `			SyBlobAppend(&sBody, "    ", sizeof("    ")-1);` |
|   2902 | 10411 | `			ReflectExportParamLine(pCtx, &sBody, &sDesc);` |
|   2902 | 10412 | `			SyBlobAppend(&sBody, "\n", sizeof(char));` |
|   1453 | 10413 | `		}` |
|   4496 | 10414 | `		SyBlobAppend(&sBody, "  }\n", sizeof("  }\n")-1);` |
|   2246 | 10415 | `	}` |
|   4550 | 10416 | `	if( bHasRet ){` |
|      - | 10417 | ``		/* php's two spellings: `Return [ T ]` for a declared type, `Tentative return`` |
|      - | 10418 | ``		 * [ T ]` for a stub's @tentative-return-type. */`` |
|   4186 | 10419 | `		if( bTentRet ){` |
|   2649 | 10420 | `			SyBlobAppend(&sBody, "  - Tentative return [ ", sizeof("  - Tentative return [ ")-1);` |
|   1326 | 10421 | `		}else{` |
|   1540 | 10422 | `			SyBlobAppend(&sBody, "  - Return [ ", sizeof("  - Return [ ")-1);` |
|      - | 10423 | `		}` |
|      - | 10424 | `		{` |
|      - | 10425 | ``			/* ...and the export's own spelling of a standalone `iterable`. */`` |
|      - | 10426 | `			SyString sRet, sIter;` |
|      - | 10427 | `			const SyString *pRet;` |
|   4186 | 10428 | `			SyStringInitFromBuf(&sRet,zRet,(sxu32)nRet);` |
|   4186 | 10429 | `			pRet = ReflectExportIterable(&sRet,&sIter);` |
|   4186 | 10430 | `			SyBlobAppend(&sBody, SyStringData(pRet), SyStringLength(pRet));` |
|      - | 10431 | `		}` |
|   4186 | 10432 | `		SyBlobAppend(&sBody, " ]\n", sizeof(" ]\n")-1);` |
|   2091 | 10433 | `	}` |
|   4550 | 10434 | `	SyBlobAppend(&sBody, "}\n", sizeof("}\n")-1);` |
|      - | 10435 | `	/* Indent every non-empty line, the way the chunk's explode/implode did. */` |
|   4550 | 10436 | `	if( zIndent == 0 \|\| zIndent[0] == '\0' ){` |
|    770 | 10437 | `		SyBlobAppend(pOut, SyBlobData(&sBody), SyBlobLength(&sBody));` |
|    389 | 10438 | `	}else{` |
|   3790 | 10439 | `		const char *z = (const char *)SyBlobData(&sBody);` |
|   3790 | 10440 | `		sxu32 n = SyBlobLength(&sBody), i = 0, iStart = 0;` |
|   3790 | 10441 | `		sxu32 nIndent = (sxu32)SyStrlen(zIndent);` |
| 592269 | 10442 | `		for( i = 0 ; i < n ; i++ ){` |
| 588481 | 10443 | `			if( z[i] != '\n' ){` |
| 564309 | 10444 | `				continue;` |
|      - | 10445 | `			}` |
|  24174 | 10446 | `			if( i > iStart ){` |
|  20392 | 10447 | `				SyBlobAppend(pOut, zIndent, nIndent);` |
|  20392 | 10448 | `				SyBlobAppend(pOut, &z[iStart], i - iStart);` |
|  10195 | 10449 | `			}` |
|  24174 | 10450 | `			SyBlobAppend(pOut, "\n", sizeof(char));` |
|  24174 | 10451 | `			iStart = i + 1;` |
|  12088 | 10452 | `		}` |
|      - | 10453 | `	}` |
|   4558 | 10454 | `	SyBlobRelease(&sBody);` |
|   4558 | 10455 | `	return rc;` |
|      4 | 10456 | `}` |
|      - | 10457 | ``/* The `- Enum cases [N]` block php prints where an enum's cases would otherwise`` |
|      - | 10458 | ` * be listed among its constants. A backed case shows its value UNQUOTED. */` |
|      6 | 10459 | `static sxi32 ReflectExportEnumCases(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass)` |
|      1 | 10460 | `{` |
|      7 | 10461 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      - | 10462 | `	sxu32 n;` |
|      7 | 10463 | `	SyBlobFormat(pOut, "\n  - Enum cases [%u] {\n", SySetUsed(&pClass->aEnumCases));` |
|     17 | 10464 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     11 | 10465 | `		SyBlobAppend(pOut, "    Case ", sizeof("    Case ")-1);` |
|     11 | 10466 | `		SyBlobAppend(pOut, SyStringData(&apCase[n]->sName), SyStringLength(&apCase[n]->sName));` |
|     11 | 10467 | `		if( pClass->nEnumBacking ){` |
|      9 | 10468 | `			ph7_value *pVal = 0;` |
|      - | 10469 | `			ph7_class_instance *pObj;` |
|      9 | 10470 | `			sxi32 rc = ReflectConstSlot(pCtx, pClass, apCase[n], &pVal);` |
|      9 | 10471 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 10472 | `				return rc;` |
|      - | 10473 | `			}` |
|      9 | 10474 | `			pObj = (pVal && (pVal->iFlags & MEMOBJ_OBJ)) ? (ph7_class_instance *)pVal->x.pOther : 0;` |
|      9 | 10475 | `			if( pObj ){` |
|      9 | 10476 | `				ph7_value *pBacking = PH7_NativeAttr(pObj, "value");` |
|      9 | 10477 | `				if( pBacking ){` |
|      - | 10478 | `					ph7_value sTmp;` |
|      - | 10479 | `					const char *zText;` |
|      - | 10480 | `					int nText;` |
|      9 | 10481 | `					PH7_MemObjInit(pCtx->pVm, &sTmp);` |
|      9 | 10482 | `					PH7_MemObjStore(pBacking, &sTmp);` |
|      9 | 10483 | `					zText = ph7_value_to_string(&sTmp, &nText);` |
|      9 | 10484 | `					SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|      9 | 10485 | `					if( nText > 0 ){` |
|      9 | 10486 | `						SyBlobAppend(pOut, zText, (sxu32)nText);` |
|      4 | 10487 | `					}` |
|      9 | 10488 | `					PH7_MemObjRelease(&sTmp);` |
|      4 | 10489 | `				}` |
|      4 | 10490 | `			}` |
|      4 | 10491 | `		}` |
|     11 | 10492 | `		SyBlobAppend(pOut, "\n", sizeof(char));` |
|      6 | 10493 | `	}` |
|      7 | 10494 | `	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|      7 | 10495 | `	return SXRET_OK;` |
|      4 | 10496 | `}` |
|      - | 10497 | `/* The Class / Interface / Enum block. */` |
|    306 | 10498 | `static sxi32 ReflectExportClassBlock(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass)` |
|      2 | 10499 | `{` |
|    308 | 10500 | `	ph7_vm *pVm = pCtx->pVm;` |
|    308 | 10501 | `	int bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;` |
|    308 | 10502 | `	int bEnum = (pClass->iFlags & PH7_CLASS_ENUM) != 0;` |
|    308 | 10503 | `	int bIface = (pClass->iFlags & PH7_CLASS_INTERFACE) != 0;` |
|      - | 10504 | `	SySet aMembers, aIface;` |
|    308 | 10505 | `	sxu32 n, nConst = 0, nStaticProp = 0, nProp = 0, nStaticMeth = 0, nMeth = 0;` |
|    308 | 10506 | `	sxi32 rc = SXRET_OK;` |
|      - | 10507 | `	int iPass;` |
|    308 | 10508 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|    308 | 10509 | `	SySetInit(&aIface, &pVm->sAllocator, sizeof(ph7_class *));` |
|    308 | 10510 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|    308 | 10511 | `	PH7_ReflectInterfacesOf(pVm, pClass, &aIface);` |
|      - | 10512 | `	/* ---- head ---- */` |
|    308 | 10513 | `	if( bIface ){` |
|     57 | 10514 | `		SyBlobAppend(pOut, "Interface [ <", sizeof("Interface [ <")-1);` |
|    280 | 10515 | `	}else if( bEnum ){` |
|      7 | 10516 | `		SyBlobAppend(pOut, "Enum [ <", sizeof("Enum [ <")-1);` |
|      4 | 10517 | `	}else{` |
|    246 | 10518 | `		SyBlobAppend(pOut, "Class [ <", sizeof("Class [ <")-1);` |
|      - | 10519 | `	}` |
|      - | 10520 | `	{` |
|    308 | 10521 | `		int iClassExt = ReflectClassExtId(pClass);` |
|    308 | 10522 | `		ReflectExportKind(pOut, iClassExt >= 0, iClassExt, 0);` |
|      - | 10523 | `	}` |
|    308 | 10524 | `	SyBlobAppend(pOut, "> ", sizeof("> ")-1);` |
|      - | 10525 | `	/* php tags a class that has an iteration handler, which its Traversable` |
|      - | 10526 | `	 * implementers have — and so does anything with a HOOKED property, because` |
|      - | 10527 | `	 * that is how php 8.4 walks one. An INTERFACE never carries one: php` |
|      - | 10528 | `	 * installs the handler when a CLASS implements Traversable, so` |
|      - | 10529 | ``	 * `interface I extends Iterator` prints no tag. */`` |
|      - | 10530 | `	{` |
|    405 | 10531 | `		int bIterable = !bIface && pVm->pTraversableClass != 0` |
|    431 | 10532 | `			&& PH7_VmInstanceOf(pClass, pVm->pTraversableClass);` |
|   3228 | 10533 | `		for( n = 0 ; !bIterable && n < SySetUsed(&aMembers) ; n++ ){` |
|   2922 | 10534 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   2920 | 10535 | `			if( pM->iKind == REFLECT_MEMBER_PROP && pM->pAttr` |
|    396 | 10536 | `			 && (pM->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) ){` |
|      3 | 10537 | `				bIterable = 1;` |
|      1 | 10538 | `			}` |
|   1462 | 10539 | `		}` |
|    308 | 10540 | `		if( bIterable ){` |
|     87 | 10541 | `			SyBlobAppend(pOut, "<iterateable> ", sizeof("<iterateable> ")-1);` |
|     43 | 10542 | `		}` |
|      - | 10543 | `	}` |
|    308 | 10544 | `	if( bIface ){` |
|     57 | 10545 | `		SyBlobFormat(pOut, "interface %z", &pClass->sName);` |
|    280 | 10546 | `	}else if( bEnum ){` |
|      7 | 10547 | `		SyBlobFormat(pOut, "enum %z", &pClass->sName);` |
|      7 | 10548 | `		if( pClass->nEnumBacking ){` |
|      5 | 10549 | `			SyBlobFormat(pOut, ": %s", (pClass->nEnumBacking & MEMOBJ_INT) ? "int" : "string");` |
|      2 | 10550 | `		}` |
|      4 | 10551 | `	}else{` |
|    246 | 10552 | `		if( pClass->iFlags & PH7_CLASS_ABSTRACT ){` |
|     13 | 10553 | `			SyBlobAppend(pOut, "abstract ", sizeof("abstract ")-1);` |
|      6 | 10554 | `		}` |
|    246 | 10555 | `		if( pClass->iFlags & PH7_CLASS_FINAL ){` |
|     45 | 10556 | `			SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|     22 | 10557 | `		}` |
|    246 | 10558 | `		if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      5 | 10559 | `			SyBlobAppend(pOut, "readonly ", sizeof("readonly ")-1);` |
|      2 | 10560 | `		}` |
|    246 | 10561 | `		SyBlobFormat(pOut, "class %z", &pClass->sName);` |
|    246 | 10562 | `		if( pClass->pBase ){` |
|    129 | 10563 | `			SyBlobFormat(pOut, " extends %z", &pClass->pBase->sName);` |
|     64 | 10564 | `		}` |
|      - | 10565 | `	}` |
|    308 | 10566 | `	if( SySetUsed(&aIface) > 0 ){` |
|    228 | 10567 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIface);` |
|      - | 10568 | `		/* An interface EXTENDS what a class implements. */` |
|    341 | 10569 | `		SyBlobAppend(pOut, bIface ? " extends " : " implements ",` |
|    113 | 10570 | `			bIface ? sizeof(" extends ")-1 : sizeof(" implements ")-1);` |
|    810 | 10571 | `		for( n = 0 ; n < SySetUsed(&aIface) ; n++ ){` |
|    584 | 10572 | `			if( n > 0 ){` |
|    357 | 10573 | `				SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|    178 | 10574 | `			}` |
|    584 | 10575 | `			SyBlobFormat(pOut, "%z", &apIface[n]->sName);` |
|    293 | 10576 | `		}` |
|    113 | 10577 | `	}` |
|    308 | 10578 | `	SyBlobAppend(pOut, " ] {\n", sizeof(" ] {\n")-1);` |
|    308 | 10579 | `	if( !bInternal && SyStringLength(&pClass->sFile) > 0 ){` |
|     23 | 10580 | `		SyBlobAppend(pOut, "  @@ ", sizeof("  @@ ")-1);` |
|     23 | 10581 | `		SyBlobAppend(pOut, SyStringData(&pClass->sFile), SyStringLength(&pClass->sFile));` |
|     23 | 10582 | `		SyBlobFormat(pOut, " %u-%u\n", pClass->nLine, pClass->nEndLine);` |
|     11 | 10583 | `	}` |
|      - | 10584 | `	/* ---- counts ---- */` |
|   5138 | 10585 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   4832 | 10586 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   4832 | 10587 | `		if( pM->iKind == REFLECT_MEMBER_CONST ){` |
|    644 | 10588 | `			if( !(bEnum && (pM->pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)) ){` |
|    634 | 10589 | `				nConst++;` |
|    318 | 10590 | `			}` |
|   4511 | 10591 | `		}else if( pM->iKind == REFLECT_MEMBER_PROP ){` |
|    400 | 10592 | `			if( pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){ nStaticProp++; }else{ nProp++; }` |
|    201 | 10593 | `		}else{` |
|      - | 10594 | `			/* A FABRICATED method is absent from the class's own export, for the same` |
|      - | 10595 | `			 * reason get_class_methods() does not name it: php is printing its` |
|      - | 10596 | `			 * function table and this one is not in it. */` |
|   3792 | 10597 | `			if( pM->pMeth->iFlags & PH7_CLASS_ATTR_FABRICATED ){ continue; }` |
|   3790 | 10598 | `			if( pM->pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){ nStaticMeth++; }else{ nMeth++; }` |
|      - | 10599 | `		}` |
|   2416 | 10600 | `	}` |
|      - | 10601 | `	/* ---- enum cases, then constants ---- */` |
|    308 | 10602 | `	if( bEnum ){` |
|      7 | 10603 | `		rc = ReflectExportEnumCases(pCtx, pOut, pClass);` |
|      7 | 10604 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 10605 | `			goto done;` |
|      - | 10606 | `		}` |
|      3 | 10607 | `	}` |
|    308 | 10608 | `	SyBlobFormat(pOut, "\n  - Constants [%u] {\n", nConst);` |
|   5138 | 10609 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   4832 | 10610 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   4830 | 10611 | `		if( pM->iKind != REFLECT_MEMBER_CONST` |
|   2738 | 10612 | `		 \|\| (bEnum && (pM->pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)) ){` |
|   4200 | 10613 | `			continue;` |
|      - | 10614 | `		}` |
|    634 | 10615 | `		SyBlobAppend(pOut, "    ", sizeof("    ")-1);` |
|    634 | 10616 | `		rc = ReflectExportConstLine(pCtx, pOut, pClass, pM->pAttr, &pM->sKey);` |
|    634 | 10617 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 10618 | `			goto done;` |
|      - | 10619 | `		}` |
|    318 | 10620 | `	}` |
|    308 | 10621 | `	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|      - | 10622 | `	/* ---- properties and methods, statics first ---- */` |
|   1532 | 10623 | `	for( iPass = 0 ; iPass < 4 ; iPass++ ){` |
|   1226 | 10624 | `		int bStatic = (iPass == 0 \|\| iPass == 1);` |
|   1226 | 10625 | `		int bMethods = (iPass == 1 \|\| iPass == 3);` |
|   1226 | 10626 | `		int bFirst = 1;` |
|      - | 10627 | `		static const char *azTitle[] = {` |
|      - | 10628 | `			"\n  - Static properties [%u] {\n", "\n  - Static methods [%u] {\n",` |
|      - | 10629 | `			"\n  - Properties [%u] {\n", "\n  - Methods [%u] {\n"` |
|      - | 10630 | `		};` |
|      - | 10631 | `		sxu32 aCount[4];` |
|   1226 | 10632 | `		aCount[0] = nStaticProp;` |
|   1226 | 10633 | `		aCount[1] = nStaticMeth;` |
|   1226 | 10634 | `		aCount[2] = nProp;` |
|   1226 | 10635 | `		aCount[3] = nMeth;` |
|   1226 | 10636 | `		SyBlobFormat(pOut, azTitle[iPass], aCount[iPass]);` |
|  20546 | 10637 | `		for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|  19322 | 10638 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - | 10639 | `			int bIsStatic;` |
|  19322 | 10640 | `			if( pM->iKind == REFLECT_MEMBER_CONST ){` |
|   2570 | 10641 | `				continue;` |
|      - | 10642 | `			}` |
|  16754 | 10643 | `			if( bMethods != (pM->iKind == REFLECT_MEMBER_METHOD) ){` |
|   8378 | 10644 | `				continue;` |
|      - | 10645 | `			}` |
|   8378 | 10646 | `			if( bMethods && (pM->pMeth->iFlags & PH7_CLASS_ATTR_FABRICATED) ){` |
|      5 | 10647 | `				continue;   /* not in the function table this is printing */` |
|      - | 10648 | `			}` |
|  12162 | 10649 | `			bIsStatic = bMethods ? (pM->pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0` |
|   4584 | 10650 | `				: (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|   8374 | 10651 | `			if( bIsStatic != bStatic ){` |
|   4188 | 10652 | `				continue;` |
|      - | 10653 | `			}` |
|   4188 | 10654 | `			if( bMethods ){` |
|      - | 10655 | `				ReflectFuncRef sRef;` |
|   3790 | 10656 | `				if( !bFirst ){` |
|   3472 | 10657 | `					SyBlobAppend(pOut, "\n", sizeof(char));` |
|   1735 | 10658 | `				}` |
|   3790 | 10659 | `				bFirst = 0;` |
|   3790 | 10660 | `				ReflectFuncFromMethod(pCtx->pVm, pClass, pM->pMeth, &sRef);` |
|   3790 | 10661 | `				rc = ReflectExportFuncBlock(pCtx, pOut, &sRef, "    ", pClass);` |
|   3790 | 10662 | `				if( rc != SXRET_OK ){` |
|    ! 0 | 10663 | `					goto done;` |
|      - | 10664 | `				}` |
|   1896 | 10665 | `			}else{` |
|    400 | 10666 | `				SyBlobAppend(pOut, "    ", sizeof("    ")-1);` |
|    400 | 10667 | `				ReflectExportPropLine(pCtx, pOut, pM->pAttr, &pM->sKey);` |
|      - | 10668 | `			}` |
|   2095 | 10669 | `		}` |
|   1226 | 10670 | `		SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|    614 | 10671 | `	}` |
|    308 | 10672 | `	SyBlobAppend(pOut, "}\n", sizeof("}\n")-1);` |
|    153 | 10673 | `done:` |
|    308 | 10674 | `	SySetRelease(&aMembers);` |
|    308 | 10675 | `	SySetRelease(&aIface);` |
|    308 | 10676 | `	return rc;` |
|      2 | 10677 | `}` |
|      - | 10678 | `/* ---- the five __toString() entry points ---- */` |
|     54 | 10679 | `static int ReflectExportClassSelf(ph7_context *pCtx)` |
|      2 | 10680 | `{` |
|     56 | 10681 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 10682 | `	SyBlob sOut;` |
|      - | 10683 | `	sxi32 rc;` |
|     56 | 10684 | `	if( pClass == 0 ){` |
|    ! 0 | 10685 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 10686 | `		return PH7_OK;` |
|      - | 10687 | `	}` |
|     56 | 10688 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     56 | 10689 | `	rc = ReflectExportClassBlock(pCtx, &sOut, pClass);` |
|     56 | 10690 | `	if( rc == SXRET_OK ){` |
|     56 | 10691 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     27 | 10692 | `	}` |
|     56 | 10693 | `	SyBlobRelease(&sOut);` |
|     56 | 10694 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|     29 | 10695 | `}` |
|      - | 10696 | `/* The standalone Function block for a name the extension walk handed over. */` |
|    250 | 10697 | `static void ReflectExportFuncByName(ph7_context *pCtx, SyBlob *pOut,` |
|      - | 10698 | `	const char *zName, int nName)` |
|      2 | 10699 | `{` |
|      - | 10700 | `	ph7_value sTarget;` |
|      - | 10701 | `	ReflectFuncRef sRef;` |
|    252 | 10702 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|    252 | 10703 | `	ph7_value_string(&sTarget, zName, nName);` |
|    252 | 10704 | `	if( ReflectFuncFill(pCtx->pVm, &sTarget, 0, &sRef) ){` |
|    252 | 10705 | `		ReflectExportFuncBlock(pCtx, pOut, &sRef, "", 0);` |
|    125 | 10706 | `	}` |
|    252 | 10707 | `	PH7_MemObjRelease(&sTarget);` |
|    252 | 10708 | `}` |
|    512 | 10709 | `static int ReflectExportFuncSelf(ph7_context *pCtx)` |
|      4 | 10710 | `{` |
|      - | 10711 | `	ReflectFuncRef sRef;` |
|      - | 10712 | `	SyBlob sOut;` |
|      - | 10713 | `	sxi32 rc;` |
|    516 | 10714 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 | 10715 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 10716 | `		return PH7_OK;` |
|      - | 10717 | `	}` |
|    516 | 10718 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    516 | 10719 | `	rc = ReflectExportFuncBlock(pCtx, &sOut, &sRef, "", ReflectOwnerOfThis(pCtx));` |
|    516 | 10720 | `	if( rc == SXRET_OK ){` |
|    516 | 10721 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    256 | 10722 | `	}` |
|    516 | 10723 | `	SyBlobRelease(&sOut);` |
|    516 | 10724 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    260 | 10725 | `}` |
|    144 | 10726 | `static int ReflectExportParamSelf(ph7_context *pCtx)` |
|      3 | 10727 | `{` |
|      - | 10728 | `	ReflectFuncRef sRef;` |
|      - | 10729 | `	ReflectParamDesc sDesc;` |
|      - | 10730 | `	SyBlob sOut;` |
|    147 | 10731 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) ){` |
|    ! 0 | 10732 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 10733 | `		return PH7_OK;` |
|      - | 10734 | `	}` |
|    147 | 10735 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    147 | 10736 | `	ReflectExportParamLine(pCtx, &sOut, &sDesc);` |
|    147 | 10737 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    147 | 10738 | `	SyBlobRelease(&sOut);` |
|    147 | 10739 | `	return PH7_OK;` |
|     75 | 10740 | `}` |
|      8 | 10741 | `static int ReflectExportPropSelf(ph7_context *pCtx)` |
|      1 | 10742 | `{` |
|      - | 10743 | `	ReflectMemberRef sRef;` |
|      - | 10744 | `	SyBlob sOut;` |
|      - | 10745 | `	SyString sKey;` |
|      9 | 10746 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 10747 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 10748 | `		return PH7_OK;` |
|      - | 10749 | `	}` |
|      9 | 10750 | `	SyStringInitFromBuf(&sKey, sRef.zName, sRef.nName);` |
|      9 | 10751 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|      9 | 10752 | `	ReflectExportPropLine(pCtx, &sOut, sRef.pAttr, &sKey);` |
|      9 | 10753 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|      9 | 10754 | `	SyBlobRelease(&sOut);` |
|      9 | 10755 | `	return PH7_OK;` |
|      5 | 10756 | `}` |
|      6 | 10757 | `static int ReflectExportConstSelf(ph7_context *pCtx)` |
|      1 | 10758 | `{` |
|      - | 10759 | `	ReflectMemberRef sRef;` |
|      - | 10760 | `	SyBlob sOut;` |
|      - | 10761 | `	SyString sKey;` |
|      - | 10762 | `	sxi32 rc;` |
|      7 | 10763 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 10764 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 10765 | `		return PH7_OK;` |
|      - | 10766 | `	}` |
|      7 | 10767 | `	SyStringInitFromBuf(&sKey, sRef.zName, sRef.nName);` |
|      7 | 10768 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|      7 | 10769 | `	rc = ReflectExportConstLine(pCtx, &sOut, sRef.pClass, sRef.pAttr, &sKey);` |
|      7 | 10770 | `	if( rc == SXRET_OK ){` |
|      7 | 10771 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|      3 | 10772 | `	}` |
|      7 | 10773 | `	SyBlobRelease(&sOut);` |
|      7 | 10774 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|      4 | 10775 | `}` |
|      - | 10776 | `/*` |
|      - | 10777 | ` * Install the Reflection API — ~25 classes, all of them declared from C.` |
|      - | 10778 | ` *` |
|      - | 10779 | `` * There is no global thunk table left to register: every `__reflect_*` and`` |
|      - | 10780 | `` * `__phl_rcinfo` became a method of the class that always owned it, so`` |
|      - | 10781 | ` * Reflection adds no name to php's global function namespace at all. Called` |
|      - | 10782 | ` * from PH7_VmInit while pVm->bCompilingBuiltin is set, right after the core` |
|      - | 10783 | ` * builtin chunks (Exception and friends have to exist already).` |
|      - | 10784 | ` */` |
|   7925 | 10785 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|      5 | 10786 | `{` |
|      - | 10787 | `	sxi32 rc;` |
|      - | 10788 | `	/* Where chunk 1 was: Reflector, Reflection, ReflectionException,` |
|      - | 10789 | `	 * ReflectionClass and ReflectionObject are native now. Chunks 2 and 3 name` |
|      - | 10790 | ``	 * `Reflector` in their own `implements` clauses, so this has to run first. */`` |
|   7930 | 10791 | `	rc = PH7_VmInstallReflectionClass(&(*pVm));` |
|   7930 | 10792 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10793 | `		return rc;` |
|      - | 10794 | `	}` |
|      - | 10795 | `	/* Where chunk 2 was: ReflectionFunctionAbstract, ReflectionFunction,` |
|      - | 10796 | `	 * ReflectionMethod and ReflectionParameter are native now. */` |
|   7930 | 10797 | `	rc = PH7_VmInstallReflectionFunc(&(*pVm));` |
|   7930 | 10798 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10799 | `		return rc;` |
|      - | 10800 | `	}` |
|   7930 | 10801 | `	rc = PH7_VmInstallReflectionHookType(&(*pVm));` |
|   7930 | 10802 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10803 | `		return rc;` |
|      - | 10804 | `	}` |
|      - | 10805 | `	/* Where chunk 3 was: ReflectionProperty and ReflectionClassConstant are` |
|      - | 10806 | `	 * native now, and PropertyHookType is a native ENUM. */` |
|   7930 | 10807 | `	rc = PH7_VmInstallReflectionMember(&(*pVm));` |
|   7930 | 10808 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10809 | `		return rc;` |
|      - | 10810 | `	}` |
|      - | 10811 | `	/* Where chunk 4 was: the four type classes are native now (see` |
|      - | 10812 | `	 * PH7_VmInstallReflectionTypes). Stringable exists by this point. */` |
|   7930 | 10813 | `	rc = PH7_VmInstallReflectionTypes(&(*pVm));` |
|   7930 | 10814 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10815 | `		return rc;` |
|      - | 10816 | `	}` |
|      - | 10817 | `	/* Where chunk 5 was: ReflectionGenerator/Fiber and the four standalone` |
|      - | 10818 | `	 * classes chunk 6 used to hold are native now. Reflector (chunk 1) exists. */` |
|   7930 | 10819 | `	rc = PH7_VmInstallReflectionSmall(&(*pVm));` |
|   7930 | 10820 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10821 | `		return rc;` |
|      - | 10822 | `	}` |
|      - | 10823 | `	/* Where chunk 6 was: the three ReflectionEnum classes are native now, and` |
|      - | 10824 | `	 * the class DESCRIPTOR they read (__phl_rcinfo) has no callers left. */` |
|   7930 | 10825 | `	rc = PH7_VmInstallReflectionEnum(&(*pVm));` |
|   7930 | 10826 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10827 | `		return rc;` |
|      - | 10828 | `	}` |
|      - | 10829 | `	/* Where chunk 7 was: ReflectionAttribute is native now, and with it the` |
|      - | 10830 | `	 * builder (__reflect_build_attrs) and the two helpers it needed. */` |
|   7930 | 10831 | `	rc = PH7_VmInstallReflectionAttribute(&(*pVm));` |
|   7930 | 10832 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10833 | `		return rc;` |
|      - | 10834 | `	}` |
|      - | 10835 | `	/* Where chunk 9 was: the export format is ReflectExportClassSelf() and its` |
|      - | 10836 | `	 * four siblings above. Nothing of the Reflection library is PHP any more,` |
|      - | 10837 | `	 * so vm_builtin_reflection_lib.c is gone and this IS the installer. */` |
|   7930 | 10838 | `	return SXRET_OK;` |
|   3962 | 10839 | `}` |
|      - | 10840 |  |
