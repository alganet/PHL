# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5967/6759 lines (88.28%)

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
|   6439 |    31 | `static ph7_class * ReflectResolveClass(ph7_vm *pVm, ph7_value *pArg)` |
|      5 |    32 | `{` |
|      - |    33 | `	ph7_class *pClass;` |
|   6444 |    34 | `	pClass = PH7_VmExtractClassFromValue(pVm, pArg);` |
|   6444 |    35 | `	if( pClass == 0 && ph7_value_is_string(pArg) ){` |
|      - |    36 | `		const char *zName;` |
|      - |    37 | `		int nLen;` |
|     20 |    38 | `		zName = ph7_value_to_string(pArg, &nLen);` |
|     20 |    39 | `		if( nLen > 0 ){` |
|     20 |    40 | `			pClass = PH7_VmTriggerAutoload(pVm, zName, (sxu32)nLen, FALSE);` |
|      9 |    41 | `		}` |
|      9 |    42 | `	}` |
|   6444 |    43 | `	return pClass;` |
|      5 |    44 | `}` |
|      - |    45 | `/*` |
|      - |    46 | ` * Hand a freshly created class instance to the caller. The return slot` |
|      - |    47 | ` * takes over the initial reference from PH7_NewClassInstance (iRef=1):` |
|      - |    48 | ` * no extra iRef++ here (see the synthesized-object invariant — a stray` |
|      - |    49 | ` * bump leaks the object and disables its __destruct).` |
|      - |    50 | ` */` |
|   1883 |    51 | `static int ReflectResultObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|      5 |    52 | `{` |
|   1888 |    53 | `	if( pObj == 0 ){` |
|    ! 0 |    54 | `		ph7_result_null(pCtx);` |
|    ! 0 |    55 | `		return PH7_OK;` |
|      - |    56 | `	}` |
|   1888 |    57 | `	PH7_MemObjRelease(pCtx->pRet);` |
|   1888 |    58 | `	pCtx->pRet->x.pOther = pObj;` |
|   1888 |    59 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|   1888 |    60 | `	return PH7_OK;` |
|    945 |    61 | `}` |
|      - |    62 | `/* The last of the descriptor marshalling: ReflectMapAddDyn survives because` |
|      - |    63 | ` * ReflectAttrArgs() answers a php ARRAY whose named arguments are string keys.` |
|      - |    64 | ` * Its Bool/Int/Str/Null/Attrs/Doc siblings went with __phl_rcinfo. */` |
|      - |    65 | `/* Add an entry under a dynamic (SyString) key. */` |
|    144 |    66 | `static void ReflectMapAddDyn(ph7_context *pCtx, ph7_value *pMap,` |
|      - |    67 | `	const SyString *pKey, ph7_value *pVal)` |
|      2 |    68 | `{` |
|    146 |    69 | `	ph7_value *pK = ph7_context_new_scalar(pCtx);` |
|    146 |    70 | `	if( pK == 0 ){ return; }` |
|    146 |    71 | `	ph7_value_string(pK, pKey->zString, (int)pKey->nByte);` |
|    146 |    72 | `	ph7_array_add_elem(pMap, pK, pVal);` |
|     60 |    73 | `}` |
|      - |    74 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth);` |
|      - |    75 | `/* Append pIface to the set unless it is already there (dedup by pointer). */` |
|   2551 |    76 | `static void ReflectIfaceAppend(SySet *pOut, ph7_class *pIface)` |
|      4 |    77 | `{` |
|      - |    78 | `	ph7_class **apKnown;` |
|      - |    79 | `	sxu32 n;` |
|   2555 |    80 | `	if( pIface == 0 ){` |
|    ! 0 |    81 | `		return;` |
|      - |    82 | `	}` |
|   2555 |    83 | `	apKnown = (ph7_class **)SySetBasePtr(pOut);` |
|   4907 |    84 | `	for( n = 0 ; n < SySetUsed(pOut) ; n++ ){` |
|   2617 |    85 | `		if( apKnown[n] == pIface ){` |
|    265 |    86 | `			return;` |
|      - |    87 | `		}` |
|   1026 |    88 | `	}` |
|   2293 |    89 | `	SySetPut(pOut, (const void *)&pIface);` |
|   1173 |    90 | `}` |
|      - |    91 | `static void ReflectFlattenIfaces(ph7_vm *pVm, ph7_class *pClass, SySet *pOut, int iDepth);` |
|      - |    92 | `/* Append pIface's OWN flattened list, BACKWARDS — see ReflectFlattenIfaces. */` |
|   1073 |    93 | `static void ReflectIfaceAppendOwn(ph7_vm *pVm, SySet *pOut, ph7_class *pIface, int iDepth)` |
|      4 |    94 | `{` |
|      - |    95 | `	SySet aSub;` |
|      - |    96 | `	ph7_class **ap;` |
|      - |    97 | `	sxu32 n;` |
|   1077 |    98 | `	SySetInit(&aSub, pOut->pAllocator, sizeof(ph7_class *));` |
|   1077 |    99 | `	ReflectFlattenIfaces(pVm, pIface, &aSub, iDepth + 1);` |
|   1077 |   100 | `	ap = (ph7_class **)SySetBasePtr(&aSub);` |
|   1640 |   101 | `	for( n = SySetUsed(&aSub) ; n > 0 ; n-- ){` |
|    566 |   102 | `		ReflectIfaceAppend(pOut, ap[n-1]);` |
|    265 |   103 | `	}` |
|   1077 |   104 | `	SySetRelease(&aSub);` |
|   1077 |   105 | `}` |
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
|   1867 |   133 | `static void ReflectFlattenIfaces(ph7_vm *pVm, ph7_class *pClass, SySet *pOut, int iDepth)` |
|      5 |   134 | `{` |
|      - |   135 | `	SySet aDecl;` |
|      - |   136 | `	ph7_class **ap;` |
|      - |   137 | `	sxu32 n, nDecl;` |
|      - |   138 | `	int bInternal, bIface;` |
|   1872 |   139 | `	if( pClass == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|    ! 0 |   140 | `		return;` |
|      - |   141 | `	}` |
|   1872 |   142 | `	bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;` |
|   1872 |   143 | `	bIface = (pClass->iFlags & PH7_CLASS_INTERFACE) != 0;` |
|      - |   144 | `	/*` |
|      - |   145 | `	 * php auto-implements Stringable for an INTERNAL class that declares its` |
|      - |   146 | `	 * own __toString, and does it while REGISTERING the class -- before the` |
|      - |   147 | `	 * parent's interfaces are inherited and before its own are named. That is` |
|      - |   148 | ``	 * the whole reason `CachingIterator` opens with Stringable and its`` |
|      - |   149 | `	 * subclass, which only inherits the method, does not. A compiled class` |
|      - |   150 | `	 * gets the same auto-implement at the END of its declared list instead,` |
|      - |   151 | `	 * which compile_class.c already spells.` |
|      - |   152 | `	 */` |
|   1872 |   153 | `	if( bInternal && !bIface ){` |
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
|   1872 |   165 | `	SySetInit(&aDecl, pOut->pAllocator, sizeof(ph7_class *));` |
|   1872 |   166 | `	if( bIface && pClass->pBase ){` |
|    492 |   167 | `		SySetPut(&aDecl, (const void *)&pClass->pBase);` |
|    231 |   168 | `	}` |
|   1872 |   169 | `	ap = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   2456 |   170 | `	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){` |
|    588 |   171 | `		SySetPut(&aDecl, (const void *)&ap[n]);` |
|    278 |   172 | `	}` |
|   1872 |   173 | `	nDecl = SySetUsed(&aDecl);` |
|      - |   174 | `	/* The parent class's own list opens the answer. */` |
|   1872 |   175 | `	if( !bIface && pClass->pBase ){` |
|      - |   176 | `		SySet aParent;` |
|    310 |   177 | `		SySetInit(&aParent, pOut->pAllocator, sizeof(ph7_class *));` |
|    310 |   178 | `		ReflectFlattenIfaces(pVm, pClass->pBase, &aParent, iDepth + 1);` |
|    310 |   179 | `		ap = (ph7_class **)SySetBasePtr(&aParent);` |
|    310 |   180 | `		if( bInternal \|\| nDecl == 0 ){` |
|   1029 |   181 | `			for( n = SySetUsed(&aParent) ; n > 0 ; n-- ){` |
|    727 |   182 | `				ReflectIfaceAppend(pOut, ap[n-1]);` |
|    319 |   183 | `			}` |
|    140 |   184 | `		}else{` |
|      9 |   185 | `			for( n = 0 ; n < SySetUsed(&aParent) ; n++ ){` |
|      5 |   186 | `				ReflectIfaceAppend(pOut, ap[n]);` |
|      3 |   187 | `			}` |
|      - |   188 | `		}` |
|    310 |   189 | `		SySetRelease(&aParent);` |
|    138 |   190 | `	}` |
|   1872 |   191 | `	ap = (ph7_class **)SySetBasePtr(&aDecl);` |
|   1872 |   192 | `	if( bInternal ){` |
|   2588 |   193 | `		for( n = 0 ; n < nDecl ; n++ ){` |
|    942 |   194 | `			ReflectIfaceAppend(pOut, ap[n]);` |
|    942 |   195 | `			ReflectIfaceAppendOwn(pVm, pOut, ap[n], iDepth);` |
|    441 |   196 | `		}` |
|    774 |   197 | `	}else{` |
|    358 |   198 | `		for( n = 0 ; n < nDecl ; n++ ){` |
|    136 |   199 | `			ReflectIfaceAppend(pOut, ap[n]);` |
|     69 |   200 | `		}` |
|    358 |   201 | `		for( n = 0 ; n < nDecl ; n++ ){` |
|    136 |   202 | `			ReflectIfaceAppendOwn(pVm, pOut, ap[n], iDepth);` |
|     69 |   203 | `		}` |
|      - |   204 | `	}` |
|   1872 |   205 | `	SySetRelease(&aDecl);` |
|    886 |   206 | `}` |
|      - |   207 | ``/* The list every Reflection door asks for -- and `class_implements()`, which is`` |
|      - |   208 | ` * the same answer under another name (vm_builtin_class.c). */` |
|    488 |   209 | `PH7_PRIVATE void PH7_ReflectInterfacesOf(ph7_vm *pVm, ph7_class *pClass, SySet *pOut)` |
|      5 |   210 | `{` |
|    493 |   211 | `	ReflectFlattenIfaces(pVm, pClass, pOut, 0);` |
|    493 |   212 | `}` |
|      - |   213 | `/*` |
|      - |   214 | ` * Deepest base class whose method table maps the same name to the very` |
|      - |   215 | ` * same ph7_class_method pointer: inheritance shares member pointers` |
|      - |   216 | ` * (PH7_ClassInherit), so this identifies the declaring class. Methods` |
|      - |   217 | ` * copied in from traits are not on the pBase chain and thus report the` |
|      - |   218 | ` * using class, which is what PHP reports too.` |
|      - |   219 | ` */` |
|  79949 |   220 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth)` |
|      5 |   221 | `{` |
|  79954 |   222 | `	ph7_class *pDecl = pClass;` |
|  79954 |   223 | `	ph7_class *pBase = pClass->pBase;` |
|  79954 |   224 | `	int iDepth = 0;` |
| 121771 |   225 | `	while( pBase && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|      - |   226 | `		SyHashEntry *pEntry;` |
|  80788 |   227 | `		pEntry = SyHashGet(&pBase->hMethod, (const void *)SyStringData(&pMeth->sFunc.sName),` |
|  26599 |   228 | `			SyStringLength(&pMeth->sFunc.sName));` |
|  54189 |   229 | `		if( pEntry == 0 \|\| (ph7_class_method *)pEntry->pUserData != pMeth ){` |
|   6085 |   230 | `			break;` |
|      - |   231 | `		}` |
|  41820 |   232 | `		pDecl = pBase;` |
|  41820 |   233 | `		pBase = pBase->pBase;` |
|  41820 |   234 | `		iDepth++;` |
|      3 |   235 | `	}` |
|      - |   236 | `	/* A record the class only got from an INTERFACE belongs to the interface:` |
|      - |   237 | ``	 * php's scope for `ReflectionMethod('AbstractImpl','ifaceMethod')` is the`` |
|      - |   238 | `	 * interface that declared it, and the base walk above cannot see one` |
|      - |   239 | `	 * (an interface is not on the pBase chain of the class implementing it). */` |
|  79954 |   240 | `	if( pDecl->iFlags & PH7_CLASS_INTERFACE ){` |
|   5098 |   241 | `		return pDecl;` |
|      - |   242 | `	}` |
|      - |   243 | `	{` |
|  74858 |   244 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&pDecl->aInterface);` |
|      - |   245 | `		sxu32 n;` |
| 134219 |   246 | `		for( n = 0 ; n < SySetUsed(&pDecl->aInterface) ; n++ ){` |
|  88862 |   247 | `			SyHashEntry *pEntry = SyHashGet(&apIface[n]->hMethod,` |
|  59399 |   248 | `				(const void *)SyStringData(&pMeth->sFunc.sName),` |
|  29458 |   249 | `				SyStringLength(&pMeth->sFunc.sName));` |
|  59404 |   250 | `			if( pEntry && (ph7_class_method *)pEntry->pUserData == pMeth ){` |
|     39 |   251 | `				return ReflectMethodDeclClass(apIface[n], pMeth);` |
|      - |   252 | `			}` |
|  29444 |   253 | `		}` |
|      - |   254 | `	}` |
|  74820 |   255 | `	return pDecl;` |
|  39793 |   256 | `}` |
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
|   2457 |   341 | `static void ReflectMembers(ph7_vm *pVm, ph7_class *pClass, SySet *pOut, int bLookup)` |
|      5 |   342 | `{` |
|      - |   343 | `	ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];` |
|   2462 |   344 | `	ph7_class *pWalk = pClass;` |
|      - |   345 | `	SyHashEntry *pEntry;` |
|      - |   346 | `	SySet aTmp;` |
|   2462 |   347 | `	int bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;` |
|      - |   348 | `	int iPart;` |
|   2462 |   349 | `	sxu32 nChain = 0, iLevel, nLev, nT;` |
|   5525 |   350 | `	while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){` |
|   3068 |   351 | `		aChain[nChain++] = pWalk;` |
|   3068 |   352 | `		pWalk = pWalk->pBase;` |
|      5 |   353 | `	}` |
|   2462 |   354 | `	SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));` |
|      - |   355 | `	/* PROPERTIES of an INTERNAL class come out base-first, and that is php's` |
|      - |   356 | `	 * registration order rather than a rule of its own: a user class declares its` |
|      - |   357 | ``	 * own properties and THEN inherits (`class B extends A` reports B's before`` |
|      - |   358 | `	 * A's), while an internal one is registered against its parent and declares` |
|      - |   359 | `	 * afterwards — so ErrorException reports Exception's five and then its own` |
|      - |   360 | `	 * $severity. Only the property table flips: php lists an internal class's` |
|      - |   361 | `	 * METHODS and CONSTANTS own-first like everything else, so the walk below` |
|      - |   362 | `	 * runs the two tables in opposite level orders. */` |
|   9833 |   363 | `	for( iPart = 0 ; iPart < 3 ; iPart++ ){` |
|  16565 |   364 | `	  for( nLev = 0 ; nLev < nChain ; nLev++ ){` |
|      - |   365 | `		ph7_class *pLevel;` |
|   9194 |   366 | `		int iTab = iPart;` |
|   9194 |   367 | `		iLevel = (iPart == 0 && bInternal) ? nChain - 1 - nLev : nLev;` |
|   9194 |   368 | `		pLevel = aChain[iLevel];` |
|      - |   369 | `		/* --- Properties (hAttr, iPart 0) and constants/enum cases (hConst,` |
|      - |   370 | `		 * iPart 1) — php's two separate member namespaces. Each table is` |
|      - |   371 | `		 * collected and emitted independently; the CONSTANT flag still decides` |
|      - |   372 | `		 * which kind comes out. --- */` |
|   9194 |   373 | `		if( iPart < 2 ){` |
|   6131 |   374 | `			SyHash *pSrcHash = iTab ? &pLevel->hConst : &pLevel->hAttr;` |
|   6131 |   375 | `			SyHash *pRefHash = iTab ? &pClass->hConst : &pClass->hAttr;` |
|   6131 |   376 | `			SySetReset(&aTmp);` |
|   6131 |   377 | `			SyHashResetLoopCursor(pSrcHash);` |
|  44812 |   378 | `			while( (pEntry = SyHashGetNextEntry(pSrcHash)) != 0 ){` |
|  38686 |   379 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  38686 |   380 | `				ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pLevel);` |
|  38686 |   381 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_HIDDEN ){` |
|      - |   382 | `					/* A native class's engine slot: php holds that state in its own C` |
|      - |   383 | `					 * struct and reports no property for it at all. */` |
|   4078 |   384 | `					continue;` |
|      - |   385 | `				}` |
|  34613 |   386 | `				if( iLevel == 0 ){` |
|      - |   387 | `					sxu32 j;` |
|      - |   388 | `					/* Own = declared here or by an off-chain provider (trait) */` |
|  31393 |   389 | `					for( j = 1 ; j < nChain ; j++ ){` |
|   7028 |   390 | `						if( aChain[j] == pDecl ){ break; }` |
|   1346 |   391 | `					}` |
|  28629 |   392 | `					if( j < nChain ){ continue; }` |
|  12178 |   393 | `				}else{` |
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
|  28331 |   406 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|      5 |   407 | `			}` |
|      - |   408 | `			/* Forward: hAttr/hConst iterate in DECLARATION order (tail inserts),` |
|      - |   409 | `			 * so members come out in the order php reports them. */` |
|  34457 |   410 | `			for( nT = 0 ; nT < SySetUsed(&aTmp) ; nT++ ){` |
|  28331 |   411 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT);` |
|  28331 |   412 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;` |
|      - |   413 | `				ReflectMember sMember;` |
|  28331 |   414 | `				sMember.iKind = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|  14177 |   415 | `					? REFLECT_MEMBER_CONST : REFLECT_MEMBER_PROP;` |
|  28331 |   416 | `				sMember.sKey = pAttr->sName;` |
|  28331 |   417 | `				sMember.pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pLevel);` |
|  28331 |   418 | `				sMember.pAttr = pAttr;` |
|  28331 |   419 | `				sMember.pMeth = 0;` |
|  28331 |   420 | `				SySetPut(pOut, (const void *)&sMember);` |
|  14154 |   421 | `			}` |
|   6131 |   422 | `			continue;` |
|      - |   423 | `		}` |
|      - |   424 | `		/* --- Methods. The reported name is the hash-entry KEY, not the` |
|      - |   425 | `		 * function's own name (see ReflectMember::sKey). --- */` |
|   3068 |   426 | `		SySetReset(&aTmp);` |
|   3068 |   427 | `		SyHashResetLoopCursor(&pLevel->hMethod);` |
|  38062 |   428 | `		while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){` |
|  34999 |   429 | `			ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  34999 |   430 | `			ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|      - |   431 | `			/* A property HOOK is compiled into a method (__phl_hook_get_NAME /` |
|      - |   432 | ``			 * __phl_hook_set_NAME) so that inheritance and `parent::` work on`` |
|      - |   433 | `			 * it, but php has no method there at all: it reports the hook on` |
|      - |   434 | `			 * the PROPERTY. Listing it would put a PHL-only name on a` |
|      - |   435 | `			 * php-visible surface. */` |
|  34994 |   436 | `			if( pEntry->nKeyLen > sizeof("__phl_hook_")-1` |
|  22988 |   437 | `			 && SyMemcmp(pEntry->pKey, "__phl_hook_", sizeof("__phl_hook_")-1) == 0 ){` |
|    713 |   438 | `				continue;` |
|      - |   439 | `			}` |
|  34289 |   440 | `			if( iLevel == 0 ){` |
|      - |   441 | `				sxu32 j;` |
|  30127 |   442 | `				for( j = 1 ; j < nChain ; j++ ){` |
|  11244 |   443 | `					if( aChain[j] == pDecl ){ break; }` |
|   2360 |   444 | `				}` |
|  25102 |   445 | `				if( j < nChain ){ continue; }` |
|   9417 |   446 | `			}else{` |
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
|  24974 |   461 | `			SySetPut(&aTmp, (const void *)&pEntry);` |
|      5 |   462 | `		}` |
|  28037 |   463 | `		for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|  24974 |   464 | `			SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|      - |   465 | `			ReflectMember sMember;` |
|  24974 |   466 | `			sMember.iKind = REFLECT_MEMBER_METHOD;` |
|  24974 |   467 | `			SyStringInitFromBuf(&sMember.sKey, (const char *)pE->pKey, pE->nKeyLen);` |
|  24974 |   468 | `			sMember.pMeth = (ph7_class_method *)pE->pUserData;` |
|  24974 |   469 | `			sMember.pDecl = ReflectMethodDeclClass(pClass, sMember.pMeth);` |
|  24974 |   470 | `			sMember.pAttr = 0;` |
|  24974 |   471 | `			SySetPut(pOut, (const void *)&sMember);` |
|  12445 |   472 | `		}` |
|   1534 |   473 | `	  }` |
|   3689 |   474 | `	}` |
|   2462 |   475 | `	SySetRelease(&aTmp);` |
|   2462 |   476 | `}` |
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
|  18912 |   491 | `static SyHashEntry * ReflectFindMethodEntry(ph7_class *pClass, const char *zName, int nName)` |
|      5 |   492 | `{` |
|      - |   493 | `	SyHashEntry *pEntry;` |
|  18917 |   494 | `	if( nName < 1 ){` |
|    ! 0 |   495 | `		return 0;` |
|      - |   496 | `	}` |
|  18917 |   497 | `	pEntry = SyHashGet(&pClass->hMethod, (const void *)zName, (sxu32)nName);` |
|  18917 |   498 | `	if( pEntry ){` |
|  12083 |   499 | `		return pEntry;` |
|      - |   500 | `	}` |
|   6838 |   501 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|  44109 |   502 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|  33854 |   503 | `		if( (int)pEntry->nKeyLen == nName` |
|  18377 |   504 | `		 && SyStrnicmp((const char *)pEntry->pKey, zName, (sxu32)nName) == 0 ){` |
|    ! 0 |   505 | `			return pEntry;` |
|      - |   506 | `		}` |
|      4 |   507 | `	}` |
|   6838 |   508 | `	return 0;` |
|   9461 |   509 | `}` |
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
|     44 |   523 | `		*piCtor = pMeth->iProtection;` |
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
|     72 |   541 | `static sxi32 ReflectCollectArgs(ph7_context *pCtx, ph7_value *pArray, SySet *pOut, SyString **ppNames)` |
|      4 |   542 | `{` |
|      - |   543 | `	ph7_hashmap *pMap;` |
|      - |   544 | `	ph7_hashmap_node *pEntry;` |
|     76 |   545 | `	SyString *aNames = 0;` |
|     76 |   546 | `	sxu32 nSlot = 0;` |
|      - |   547 | `	sxu32 n;` |
|     76 |   548 | `	if( ppNames ){` |
|     66 |   549 | `		*ppNames = 0;` |
|     31 |   550 | `	}` |
|     76 |   551 | `	if( !ph7_value_is_array(pArray) ){` |
|    ! 0 |   552 | `		return SXRET_OK;` |
|      - |   553 | `	}` |
|     76 |   554 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     76 |   555 | `	pEntry = pMap->pFirst;` |
|    152 |   556 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     79 |   557 | `		ph7_value *pValue = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|     79 |   558 | `		if( pValue ){` |
|     79 |   559 | `			if( ppNames && pEntry->iType == HASHMAP_BLOB_NODE ){` |
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
|     79 |   572 | `			SySetPut(pOut, (const void *)&pValue);` |
|     79 |   573 | `			nSlot++;` |
|     38 |   574 | `		}` |
|     79 |   575 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     41 |   576 | `	}` |
|     76 |   577 | `	if( ppNames ){` |
|     66 |   578 | `		*ppNames = aNames;` |
|     31 |   579 | `	}` |
|     76 |   580 | `	return SXRET_OK;` |
|     40 |   581 | `}` |
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
|   5198 |   701 | `static ph7_class_instance * ReflectValueClosure(ph7_vm *pVm, ph7_value *pVal)` |
|      5 |   702 | `{` |
|      - |   703 | `	ph7_class_instance *pThis;` |
|   5203 |   704 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
|   4873 |   705 | `		return 0;` |
|      - |   706 | `	}` |
|    334 |   707 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    334 |   708 | `	return (pThis->pClass == pVm->pClosureClass) ? pThis : 0;` |
|   2598 |   709 | `}` |
|      - |   710 | `/*` |
|      - |   711 | ` * Resolve a reflection callable target into its compiled function.` |
|      - |   712 | ` *   - pMethodArg a non-empty string  -> method mode: pTarget is a class name` |
|      - |   713 | ` *     or object; outputs *ppClass and *ppMeth.` |
|      - |   714 | ` *   - pTarget a Closure              -> unwrap $__fn into hFunction; *ppClosure.` |
|      - |   715 | ` *   - pTarget a string               -> hFunction (user) or hHostFunction` |
|      - |   716 | ` *     (*ppHost set, returns NULL).` |
|      - |   717 | ` * Returns the ph7_vm_func, or NULL (host function or unresolvable).` |
|      - |   718 | ` */` |
|   7762 |   719 | `static ph7_vm_func * ReflectResolveCallable(ph7_vm *pVm, ph7_value *pTarget,` |
|      - |   720 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|      - |   721 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|      5 |   722 | `{` |
|      - |   723 | `	SyHashEntry *pEntry;` |
|   7767 |   724 | `	if( ppClass ){ *ppClass = 0; }` |
|   7767 |   725 | `	if( ppMeth ){ *ppMeth = 0; }` |
|   7767 |   726 | `	if( ppHost ){ *ppHost = 0; }` |
|   7767 |   727 | `	if( ppClosure ){ *ppClosure = 0; }` |
|   7767 |   728 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
|   3485 |   729 | `		ph7_class *pClass = ReflectResolveClass(pVm, pTarget);` |
|      - |   730 | `		ph7_class_method *pMeth;` |
|   3485 |   731 | `		if( pClass == 0 ){` |
|    ! 0 |   732 | `			return 0;` |
|      - |   733 | `		}` |
|   5225 |   734 | `		pMeth = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|   1740 |   735 | `			SyBlobLength(&pMethodArg->sBlob));` |
|   3485 |   736 | `		if( pMeth == 0 ){` |
|      - |   737 | `			/* getMethods()/ReflectionClass reports a base class's PRIVATE methods on` |
|      - |   738 | `			 * the subclass (php copies them into the child's table), but private` |
|      - |   739 | `			 * methods are not inherited into the child's method table, so the plain` |
|      - |   740 | `			 * extract misses them when a ReflectionMethod obtained from the subclass` |
|      - |   741 | `			 * is re-resolved by (subclass, name). Walk the base chain to find the` |
|      - |   742 | `			 * declaring class's own copy. */` |
|    ! 0 |   743 | `			ph7_class *pWalk = pClass->pBase;` |
|    ! 0 |   744 | `			while( pWalk && pMeth == 0 ){` |
|    ! 0 |   745 | `				pMeth = PH7_ClassExtractMethod(pWalk, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|    ! 0 |   746 | `					SyBlobLength(&pMethodArg->sBlob));` |
|    ! 0 |   747 | `				pWalk = pWalk->pBase;` |
|    ! 0 |   748 | `			}` |
|    ! 0 |   749 | `		}` |
|   3485 |   750 | `		if( pMeth == 0 ){` |
|    ! 0 |   751 | `			return 0;` |
|      - |   752 | `		}` |
|   3485 |   753 | `		if( ppClass ){ *ppClass = pClass; }` |
|   3485 |   754 | `		if( ppMeth ){ *ppMeth = pMeth; }` |
|   3485 |   755 | `		return &pMeth->sFunc;` |
|      - |   756 | `	}` |
|      - |   757 | `	{` |
|   4287 |   758 | `		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|   4287 |   759 | `		if( pClo ){` |
|      - |   760 | `			SyString sAttr;` |
|      - |   761 | `			ph7_value *pFn;` |
|    252 |   762 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    252 |   763 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|    252 |   764 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) < 1 ){` |
|    ! 0 |   765 | `				return 0;` |
|      - |   766 | `			}` |
|      - |   767 | `			/* A closure over an object method or __invoke object` |
|      - |   768 | `			 * (Closure::fromCallable([$obj,'m']) / fromCallable($invokeObj)) stores the` |
|      - |   769 | `			 * bare method name in $__fn and the class in $__scope. Resolve it as a METHOD` |
|      - |   770 | `			 * of $__scope FIRST — before the global function table — so a same-named` |
|      - |   771 | `			 * global function does not shadow the method (the bug: $__fn "add" hitting a` |
|      - |   772 | `			 * global add()). A plain anonymous closure's $__fn is its unique lambda name,` |
|      - |   773 | `			 * which is not a method, so this falls through cleanly. */` |
|      - |   774 | `			{` |
|      - |   775 | `				SyString sScope;` |
|      - |   776 | `				ph7_value *pScope;` |
|    252 |   777 | `				SyStringInitFromBuf(&sScope, "__scope", 7);` |
|    252 |   778 | `				pScope = PH7_ClassInstanceFetchAttr(pClo, &sScope);` |
|    252 |   779 | `				if( pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0 ){` |
|    203 |   780 | `					ph7_class *pScopeCls = PH7_VmExtractClass(pVm,` |
|    134 |   781 | `						(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|    136 |   782 | `					if( pScopeCls ){` |
|    203 |   783 | `						ph7_class_method *pScopeMeth = PH7_ClassExtractMethod(pScopeCls,` |
|    134 |   784 | `							(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    136 |   785 | `						if( pScopeMeth ){` |
|     98 |   786 | `							if( ppClass ){ *ppClass = pScopeCls; }` |
|     98 |   787 | `							if( ppMeth ){ *ppMeth = pScopeMeth; }` |
|     98 |   788 | `							if( ppClosure ){ *ppClosure = pClo; }` |
|     98 |   789 | `							return &pScopeMeth->sFunc;` |
|      - |   790 | `						}` |
|     19 |   791 | `					}` |
|     19 |   792 | `				}` |
|      - |   793 | `			}` |
|    156 |   794 | `			pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    156 |   795 | `			if( pEntry == 0 ){` |
|      - |   796 | `				/* A Closure over a host function (Closure::fromCallable('strlen')) */` |
|     28 |   797 | `				pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|     28 |   798 | `				if( pEntry && ppHost ){` |
|      3 |   799 | `					*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|      3 |   800 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|      1 |   801 | `				}` |
|     28 |   802 | `				return 0;` |
|      - |   803 | `			}` |
|    130 |   804 | `			if( ppClosure ){ *ppClosure = pClo; }` |
|    130 |   805 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|      - |   806 | `		}` |
|      - |   807 | `	}` |
|   4039 |   808 | `	if( pTarget->iFlags & MEMOBJ_STRING ){` |
|   4039 |   809 | `		if( SyBlobLength(&pTarget->sBlob) < 1 ){` |
|    ! 0 |   810 | `			return 0;` |
|      - |   811 | `		}` |
|   4039 |   812 | `		pEntry = PH7_VmGetUserFunction(pVm, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob), FALSE);` |
|   4039 |   813 | `		if( pEntry ){` |
|    943 |   814 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|      - |   815 | `		}` |
|   3101 |   816 | `		pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   3101 |   817 | `		if( pEntry && ppHost ){` |
|   3087 |   818 | `			*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|   1537 |   819 | `		}` |
|   1544 |   820 | `	}` |
|   3101 |   821 | `	return 0;` |
|   3882 |   822 | `}` |
|      - |   823 | `/*` |
|      - |   824 | ` * ---------------------------------------------------------------------------` |
|      - |   825 | ` * The signature parser.` |
|      - |   826 | ` *` |
|      - |   827 | ` * A C builtin and a native method declare their parameters as ONE php-style` |
|      - |   828 | `` * string (`"string $name, ?int $len = null"`) — the aBuiltinSig[] row or the`` |
|      - |   829 | ` * PH7_NativeClassSpec's zSig — because neither has a compiled parameter list to` |
|      - |   830 | ` * read. Reflection needs that string as the same param-meta shape a compiled` |
|      - |   831 | ` * function produces, so it is parsed here and the descriptor comes out of` |
|      - |   832 | ` * __reflect_func_info() already uniform.` |
|      - |   833 | ` *` |
|      - |   834 | ` * This was chunk 8 of the reflection prelude (__reflect_sig_split /` |
|      - |   835 | ` * __reflect_sig_scalar / __reflect_parse_sig / __reflect_sig_fixup), which every` |
|      - |   836 | ` * caller had to remember to wrap around __reflect_func_info() — six call sites,` |
|      - |   837 | ` * and the one that FORGOT is why every native method reported zero parameters` |
|      - |   838 | ` * until 31 Jul. Producing the parsed form at the source removes the wrapper and` |
|      - |   839 | ` * the possibility of forgetting it.` |
|      - |   840 | ` * ---------------------------------------------------------------------------` |
|      - |   841 | ` */` |
|      - |   842 | `/* Trim ASCII spaces off both ends of [z, z+n). */` |
|  32528 |   843 | `static void ReflectSigTrim(const char **pz, int *pn)` |
|      5 |   844 | `{` |
|  32533 |   845 | `	const char *z = *pz;` |
|  32533 |   846 | `	int n = *pn;` |
|  63649 |   847 | `	while( n > 0 && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|  14857 |   848 | `		z++;` |
|  14857 |   849 | `		n--;` |
|      5 |   850 | `	}` |
|  51317 |   851 | `	while( n > 0 && (z[n-1] == ' ' \|\| z[n-1] == '\t') ){` |
|   2525 |   852 | `		n--;` |
|      5 |   853 | `	}` |
|  32533 |   854 | `	*pz = z;` |
|  32533 |   855 | `	*pn = n;` |
|  32533 |   856 | `}` |
|      - |   857 | ``/* Byte search that respects single-quoted runs (a default may be `'a,b'` or`` |
|      - |   858 | `` * `'it\'s'`), which is the whole reason the split is not SyByteFind. Answers`` |
|      - |   859 | ` * the offset of the first unquoted zWhat, or -1. */` |
|  41702 |   860 | `static int ReflectSigFindUnquoted(const char *z, int n, char cWhat)` |
|      5 |   861 | `{` |
|      - |   862 | `	int k;` |
|  41707 |   863 | `	char cQuote = 0;` |
| 636813 |   864 | `	for( k = 0 ; k < n ; k++ ){` |
| 620493 |   865 | `		if( cQuote ){` |
|   5294 |   866 | `			if( z[k] == '\\' && k + 1 < n ){` |
|    960 |   867 | `				k++;` |
|   4815 |   868 | `			}else if( z[k] == cQuote ){` |
|   2024 |   869 | `				cQuote = 0;` |
|   1014 |   870 | `			}` |
| 617848 |   871 | `		}else if( z[k] == '\'' \|\| z[k] == '"' ){` |
|      - |   872 | `			/* BOTH spellings open a run. A native method's zSig lives in C, so` |
|      - |   873 | ``			 * `string $separator = ","` is the natural way to write it -- and`` |
|      - |   874 | `			 * tracking only the single quote let the comma INSIDE that default` |
|      - |   875 | `			 * split the parameter in two, which is how setCsvControl() came to` |
|      - |   876 | ``			 * report four parameters, one of them named `$"`. */`` |
|   2024 |   877 | `			cQuote = z[k];` |
| 614193 |   878 | `		}else if( z[k] == cWhat ){` |
|  25387 |   879 | `			return k;` |
|      - |   880 | `		}` |
| 297546 |   881 | `	}` |
|  16325 |   882 | `	return -1;` |
|  20853 |   883 | `}` |
|      - |   884 | `/* Does [z,n) contain zNeedle? (case-sensitive; nNeedle > 0) */` |
|  15638 |   885 | `static int ReflectSigHas(const char *z, int n, const char *zNeedle, int nNeedle)` |
|      5 |   886 | `{` |
|      - |   887 | `	int k;` |
| 183105 |   888 | `	for( k = 0 ; k + nNeedle <= n ; k++ ){` |
| 167801 |   889 | `		if( SyMemcmp((const void *)&z[k], (const void *)zNeedle, (sxu32)nNeedle) == 0 ){` |
|    338 |   890 | `			return 1;` |
|      - |   891 | `		}` |
|  83736 |   892 | `	}` |
|  15309 |   893 | `	return 0;` |
|   7824 |   894 | `}` |
|      - |   895 | ``/* Case-insensitive twin, for the `null` arm of a union type text. */`` |
|   5225 |   896 | `static int ReflectSigHasNoCase(const char *z, int n, const char *zNeedle, int nNeedle)` |
|      5 |   897 | `{` |
|      - |   898 | `	int k, j;` |
|  21439 |   899 | `	for( k = 0 ; k + nNeedle <= n ; k++ ){` |
|  17048 |   900 | `		for( j = 0 ; j < nNeedle ; j++ ){` |
|  17024 |   901 | `			if( SyToLower(z[k+j]) != SyToLower(zNeedle[j]) ){` |
|  16214 |   902 | `				break;` |
|      - |   903 | `			}` |
|    409 |   904 | `		}` |
|  16238 |   905 | `		if( j == nNeedle ){` |
|     27 |   906 | `			return 1;` |
|      - |   907 | `		}` |
|   8108 |   908 | `	}` |
|   5206 |   909 | `	return 0;` |
|   2616 |   910 | `}` |
|      - |   911 | `/*` |
|      - |   912 | `` * One `C::K` (or `C::class`) term of a declared default, evaluated.`` |
|      - |   913 | ` *` |
|      - |   914 | `` * php's stub writes these as SOURCE — `string $class = SplFileInfo::class`,`` |
|      - |   915 | `` * `int $flags = FilesystemIterator::KEY_AS_PATHNAME\|…` — and the reflector then`` |
|      - |   916 | ` * answers both the text (the export line) and the VALUE (getDefaultValue()). A` |
|      - |   917 | ` * native method's zSig is that same source, so the value has to be read out of` |
|      - |   918 | ` * the class here; there are no compiled parameter records to hold it.` |
|      - |   919 | ` */` |
|   1244 |   920 | `static int ReflectSigClassConst(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      5 |   921 | `{` |
|   1249 |   922 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |   923 | `	ph7_class_attr *pAttr;` |
|      - |   924 | `	ph7_class *pClass;` |
|      - |   925 | `	ph7_value *pValue;` |
|      - |   926 | `	int iSep;` |
|   1249 |   927 | `	ReflectSigTrim(&z, &n);` |
|   5839 |   928 | `	for( iSep = 0 ; iSep + 1 < n ; ++iSep ){` |
|   4695 |   929 | `		if( z[iSep] == ':' && z[iSep+1] == ':' ){` |
|    103 |   930 | `			break;` |
|      - |   931 | `		}` |
|   2300 |   932 | `	}` |
|   1249 |   933 | `	if( iSep + 1 >= n \|\| iSep < 1 ){` |
|   1149 |   934 | `		return 0;` |
|      - |   935 | `	}` |
|    103 |   936 | `	pClass = PH7_VmExtractClass(pVm, z, (sxu32)iSep, TRUE, 0);` |
|    103 |   937 | `	if( pClass == 0 ){` |
|    ! 0 |   938 | `		return 0;` |
|      - |   939 | `	}` |
|    103 |   940 | `	z += iSep + 2;` |
|    103 |   941 | `	n -= iSep + 2;` |
|    103 |   942 | `	if( n == (int)sizeof("class")-1 && SyMemcmp(z, "class", sizeof("class")-1) == 0 ){` |
|      - |   943 | ``		/* `C::class` is the class NAME, and php prints the name it was DECLARED`` |
|      - |   944 | `		 * with rather than the spelling in the signature. */` |
|      8 |   945 | `		SyString *pName = &pClass->sName;` |
|      8 |   946 | `		ph7_value_string(pOut, SyStringData(pName), (int)SyStringLength(pName));` |
|      8 |   947 | `		return 1;` |
|      - |   948 | `	}` |
|     97 |   949 | `	pAttr = PH7_ClassExtractConstant(pClass, z, (sxu32)n);` |
|     97 |   950 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|    ! 0 |   951 | `		return 0;` |
|      - |   952 | `	}` |
|     97 |   953 | `	if( pAttr->nIdx == SXU32_HIGH ){` |
|      - |   954 | `` 		/* Not materialized yet: bring it into being exactly as a direct `C::K` `` |
|      - |   955 | `		 * read would. An enum CASE is a singleton with its own materializer --` |
|      - |   956 | `		 * running the constant initializer over one answers nothing, which is` |
|      - |   957 | ``		 * why `RoundingMode::HalfAwayFromZero` could not be reduced at all. */`` |
|     29 |   958 | `		sxi32 rcConst = (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)` |
|    ! 0 |   959 | `			? VmEnumMaterializeCase(pVm, pClass, pAttr)` |
|     18 |   960 | `			: VmClassConstEvalOnDemand(pVm, pClass, pAttr);` |
|     20 |   961 | `		if( rcConst != SXRET_OK ){` |
|    ! 0 |   962 | `			return 0;` |
|      - |   963 | `		}` |
|      9 |   964 | `	}` |
|     97 |   965 | `	pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, pAttr->nIdx);` |
|     97 |   966 | `	if( pValue == 0 ){` |
|    ! 0 |   967 | `		return 0;` |
|      - |   968 | `	}` |
|     97 |   969 | `	PH7_MemObjStore(pValue, pOut);` |
|     97 |   970 | `	return 1;` |
|    627 |   971 | `}` |
|      - |   972 | `/*` |
|      - |   973 | ` * A GLOBAL constant named by a declared default — php's stub writes` |
|      - |   974 | `` * `int $severity = E_ERROR` and both surfaces read it from here: the export`` |
|      - |   975 | ` * prints the NAME (rule 28: the argument was never folded, so php still has the` |
|      - |   976 | ` * source) and getDefaultValue() answers what it expands to. Answers 0 for` |
|      - |   977 | ` * anything that is not a plain identifier naming a defined constant, which is` |
|      - |   978 | `` * what keeps `null`/`true`/a bare number out of this branch.`` |
|      - |   979 | ` */` |
|   2756 |   980 | `static int ReflectSigIsIdent(const char *z, int n)` |
|      5 |   981 | `{` |
|      - |   982 | `	int k;` |
|   2761 |   983 | `	if( n < 1 \|\| (z[0] != '_' && !SyisAlpha(z[0])) ){` |
|   1526 |   984 | `		return 0;` |
|      - |   985 | `	}` |
|   7771 |   986 | `	for( k = 1 ; k < n ; ++k ){` |
|   6617 |   987 | `		if( z[k] != '_' && z[k] != '\\' && !SyisAlphaNum(z[k]) ){` |
|     83 |   988 | `			return 0;` |
|      - |   989 | `		}` |
|   3271 |   990 | `	}` |
|   1159 |   991 | `	return 1;` |
|   1383 |   992 | `}` |
|   2756 |   993 | `static int ReflectSigGlobalConst(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      5 |   994 | `{` |
|      - |   995 | `	SyHashEntry *pEntry;` |
|      - |   996 | `	ph7_constant *pCons;` |
|   2761 |   997 | `	ReflectSigTrim(&z, &n);` |
|   2761 |   998 | `	if( !ReflectSigIsIdent(z, n) ){` |
|   1606 |   999 | `		return 0;` |
|      - |  1000 | `	}` |
|   1159 |  1001 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant, (const void *)z, (sxu32)n);` |
|   1159 |  1002 | `	if( pEntry == 0 ){` |
|    966 |  1003 | `		return 0;` |
|      - |  1004 | `	}` |
|    197 |  1005 | `	pCons = (ph7_constant *)pEntry->pUserData;` |
|    197 |  1006 | `	if( pCons == 0 \|\| pCons->xExpand == 0 ){` |
|    ! 0 |  1007 | `		return 0;` |
|      - |  1008 | `	}` |
|    197 |  1009 | `	pCons->xExpand(pOut, pCons->pUserData);` |
|    197 |  1010 | `	return 1;` |
|   1383 |  1011 | `}` |
|      - |  1012 | `/*` |
|      - |  1013 | `` * A constant EXPRESSION: one term, or the `\|` fold php's own stubs write for a`` |
|      - |  1014 | `` * flags default (`KEY_AS_PATHNAME \| CURRENT_AS_FILEINFO \| SKIP_DOTS`, and`` |
|      - |  1015 | `` * `SQLITE3_OPEN_READWRITE \| SQLITE3_OPEN_CREATE`). Either kind of name may`` |
|      - |  1016 | ` * stand in it -- a class constant or a global one -- because php's stubs write` |
|      - |  1017 | ` * both, and a term is looked up as whichever it turns out to be.` |
|      - |  1018 | ` */` |
|    450 |  1019 | `static int ReflectSigConstExpr(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      4 |  1020 | `{` |
|    454 |  1021 | `	sxi64 iAcc = 0;` |
|    454 |  1022 | `	int iStart = 0;` |
|    454 |  1023 | `	int k, nTerm = 0;` |
|    454 |  1024 | `	if( !ReflectSigHas(z, n, "::", 2) && !ReflectSigHas(z, n, "\|", 1) ){` |
|    398 |  1025 | `		return 0;` |
|      - |  1026 | `	}` |
|   2190 |  1027 | `	for( k = 0 ; k <= n ; ++k ){` |
|   2134 |  1028 | `		if( k < n && z[k] != '\|' ){` |
|   2024 |  1029 | `			continue;` |
|      - |  1030 | `		}` |
|    110 |  1031 | `		if( !ReflectSigClassConst(pCtx, &z[iStart], k - iStart, pOut)` |
|     95 |  1032 | `		 && !ReflectSigGlobalConst(pCtx, &z[iStart], k - iStart, pOut) ){` |
|    ! 0 |  1033 | `			return 0;` |
|      - |  1034 | `		}` |
|    112 |  1035 | `		nTerm++;` |
|    112 |  1036 | `		if( k < n \|\| nTerm > 1 ){` |
|      - |  1037 | `			/* A fold is arithmetic, so every arm has to be an integer; a lone` |
|      - |  1038 | ``			 * term keeps whatever type it had (`C::class` is a string). */`` |
|     88 |  1039 | `			if( (pOut->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) != 0 ){` |
|    ! 0 |  1040 | `				return 0;` |
|      - |  1041 | `			}` |
|     88 |  1042 | `			PH7_MemObjToInteger(pOut);` |
|     88 |  1043 | `			iAcc \|= pOut->x.iVal;` |
|     43 |  1044 | `		}` |
|    112 |  1045 | `		iStart = k + 1;` |
|     57 |  1046 | `	}` |
|     58 |  1047 | `	if( nTerm > 1 ){` |
|     34 |  1048 | `		ph7_value_int64(pOut, iAcc);` |
|     16 |  1049 | `	}` |
|     58 |  1050 | `	return nTerm > 0;` |
|    229 |  1051 | `}` |
|      - |  1052 | `/* One hex digit's value, or -1. */` |
|     48 |  1053 | `static int ReflectHexVal(char c)` |
|      1 |  1054 | `{` |
|     49 |  1055 | `	if( c >= '0' && c <= '9' ) return c - '0';` |
|    ! 0 |  1056 | `	if( c >= 'a' && c <= 'f' ) return c - 'a' + 10;` |
|    ! 0 |  1057 | `	if( c >= 'A' && c <= 'F' ) return c - 'A' + 10;` |
|    ! 0 |  1058 | `	return -1;` |
|     25 |  1059 | `}` |
|      - |  1060 | `/*` |
|      - |  1061 | ` * php keeps the SOURCE of a constant EXPRESSION in a stub's default and prints` |
|      - |  1062 | ` * it back verbatim, and two more shapes of one live in the signature table` |
|      - |  1063 | `` * beside the `C::K` and `A\|B` forms already handled: an integer written in a`` |
|      - |  1064 | `` * RADIX (`0777` for mkdir's permissions) and a product (`2 * 1024 * 1024` for`` |
|      - |  1065 | ` * SplTempFileObject's memory bound). Both are text php never reduces on the` |
|      - |  1066 | ` * page; both have to be reduced for getDefaultValue().` |
|      - |  1067 | ` */` |
|   1416 |  1068 | `static int ReflectSigRadixInt(const char *z, int n, sxi64 *pOut)` |
|      5 |  1069 | `{` |
|   1421 |  1070 | `	int iRadix = 8, k = 1;` |
|   1421 |  1071 | `	sxi64 iVal = 0;` |
|   1421 |  1072 | `	ReflectSigTrim(&z, &n);` |
|   1421 |  1073 | `	if( n < 2 \|\| z[0] != '0' ){` |
|   1417 |  1074 | `		return 0;` |
|      - |  1075 | `	}` |
|      5 |  1076 | `	if( z[1] == 'x' \|\| z[1] == 'X' ){       iRadix = 16; k = 2; }` |
|      5 |  1077 | `	else if( z[1] == 'o' \|\| z[1] == 'O' ){  iRadix = 8;  k = 2; }` |
|      5 |  1078 | `	else if( z[1] == 'b' \|\| z[1] == 'B' ){  iRadix = 2;  k = 2; }` |
|      5 |  1079 | `	if( k >= n ){` |
|    ! 0 |  1080 | `		return 0;` |
|      - |  1081 | `	}` |
|     17 |  1082 | `	for( ; k < n ; ++k ){` |
|     13 |  1083 | `		int iDigit = ReflectHexVal(z[k]);` |
|     13 |  1084 | `		if( iDigit < 0 \|\| iDigit >= iRadix ){` |
|    ! 0 |  1085 | `			return 0;` |
|      - |  1086 | `		}` |
|     13 |  1087 | `		iVal = iVal * iRadix + iDigit;` |
|      7 |  1088 | `	}` |
|      5 |  1089 | `	*pOut = iVal;` |
|      5 |  1090 | `	return 1;` |
|    713 |  1091 | `}` |
|      - |  1092 | ``/* `a * b * c` over integer literals and integer constants. */`` |
|    386 |  1093 | `static int ReflectSigProduct(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      4 |  1094 | `{` |
|    390 |  1095 | `	sxi64 iAcc = 1;` |
|    390 |  1096 | `	int iStart = 0, k, nTerm = 0;` |
|    390 |  1097 | `	if( ReflectSigFindUnquoted(z, n, '*') < 0 ){` |
|    388 |  1098 | `		return 0;` |
|      - |  1099 | `	}` |
|     35 |  1100 | `	for( k = 0 ; k <= n ; ++k ){` |
|      - |  1101 | `		const char *zTerm;` |
|      - |  1102 | `		int nTermLen;` |
|     33 |  1103 | `		sxi64 iVal = 0;` |
|     33 |  1104 | `		sxu8 bReal = 0;` |
|     33 |  1105 | `		if( k < n && z[k] != '*' ){` |
|     27 |  1106 | `			continue;` |
|      - |  1107 | `		}` |
|      7 |  1108 | `		zTerm = &z[iStart];` |
|      7 |  1109 | `		nTermLen = k - iStart;` |
|      7 |  1110 | `		ReflectSigTrim(&zTerm, &nTermLen);` |
|      7 |  1111 | `		if( nTermLen < 1 ){` |
|    ! 0 |  1112 | `			return 0;` |
|      - |  1113 | `		}` |
|      7 |  1114 | `		if( !ReflectSigRadixInt(zTerm, nTermLen, &iVal) ){` |
|      - |  1115 | `			ph7_value *pTerm;` |
|      7 |  1116 | `			if( SyStrIsNumeric(zTerm, (sxu32)nTermLen, &bReal, 0) == SXRET_OK && !bReal ){` |
|      7 |  1117 | `				SyStrToInt64(zTerm, (sxu32)nTermLen, (void *)&iVal, 0);` |
|      4 |  1118 | `			}else if( (pTerm = ph7_context_new_scalar(pCtx)) != 0` |
|    ! 0 |  1119 | `			       && (ReflectSigClassConst(pCtx, zTerm, nTermLen, pTerm)` |
|    ! 0 |  1120 | `			        \|\| ReflectSigGlobalConst(pCtx, zTerm, nTermLen, pTerm))` |
|    ! 0 |  1121 | `			       && (pTerm->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) == 0 ){` |
|    ! 0 |  1122 | `				PH7_MemObjToInteger(pTerm);` |
|    ! 0 |  1123 | `				iVal = pTerm->x.iVal;` |
|    ! 0 |  1124 | `			}else{` |
|    ! 0 |  1125 | `				return 0;` |
|      - |  1126 | `			}` |
|      3 |  1127 | `		}` |
|      7 |  1128 | `		iAcc *= iVal;` |
|      7 |  1129 | `		nTerm++;` |
|      7 |  1130 | `		iStart = k + 1;` |
|      4 |  1131 | `	}` |
|      3 |  1132 | `	if( nTerm < 2 ){` |
|    ! 0 |  1133 | `		return 0;` |
|      - |  1134 | `	}` |
|      3 |  1135 | `	ph7_value_int64(pOut, iAcc);` |
|      3 |  1136 | `	return 1;` |
|    197 |  1137 | `}` |
|      - |  1138 | `/*` |
|      - |  1139 | ` * Is this default TEXT one php prints back as SOURCE rather than as a value?` |
|      - |  1140 | `` * The `C::K` and `A\|B` forms are answered by their own readers above; these`` |
|      - |  1141 | ` * two are the ones a plain literal reader would silently reduce to a number.` |
|      - |  1142 | ` */` |
|   1030 |  1143 | `static int ReflectSigIsSourceExpr(const char *z, int n)` |
|      5 |  1144 | `{` |
|   1035 |  1145 | `	sxi64 iIgnored = 0;` |
|   1548 |  1146 | `	return ReflectSigFindUnquoted(z, n, '*') >= 0` |
|   1030 |  1147 | `	    \|\| ReflectSigRadixInt(z, n, &iIgnored);` |
|      5 |  1148 | `}` |
|      - |  1149 | `/*` |
|      - |  1150 | ` * A default-value TEXT to a value, when the text denotes a scalar php can` |
|      - |  1151 | `` * reproduce. Answers 1 and fills pOut, or 0 for anything else (`[]`,`` |
|      - |  1152 | `` * `array (`, a constant name) which the caller reports its own way.`` |
|      - |  1153 | ` */` |
|   1226 |  1154 | `static int ReflectSigScalar(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      5 |  1155 | `{` |
|   1231 |  1156 | `	sxu8 bReal = 0;` |
|   1231 |  1157 | `	if( n == 1 && z[0] == '?' ){` |
|    ! 0 |  1158 | `		return 0;` |
|      - |  1159 | `	}` |
|   1231 |  1160 | `	if( (n == 4 && (SyMemcmp(z,"NULL",4) == 0 \|\| SyMemcmp(z,"null",4) == 0)) ){` |
|    436 |  1161 | `		ph7_value_null(pOut);` |
|    436 |  1162 | `		return 1;` |
|      - |  1163 | `	}` |
|    798 |  1164 | `	if( n == 4 && SyMemcmp(z,"true",4) == 0 ){` |
|     53 |  1165 | `		ph7_value_bool(pOut,1);` |
|     53 |  1166 | `		return 1;` |
|      - |  1167 | `	}` |
|    748 |  1168 | `	if( n == 5 && SyMemcmp(z,"false",5) == 0 ){` |
|     71 |  1169 | `		ph7_value_bool(pOut,0);` |
|     71 |  1170 | `		return 1;` |
|      - |  1171 | `	}` |
|    680 |  1172 | `	if( n >= 2 && (z[0] == '\'' \|\| z[0] == '"') && z[n-1] == z[0] ){` |
|      - |  1173 | `		/* Undo the escapes the signature writer emits, which are php's own set --` |
|      - |  1174 | `		 * the same one ReflectExportStrQ puts back when it prints the line. A row` |
|      - |  1175 | `		 * cannot hold these bytes any other way: a signature is a C string, so a` |
|      - |  1176 | ``		 * NUL would END it, which is why trim()'s ` \n\r\t\v\x00` had no default`` |
|      - |  1177 | `		 * row at all and its parameter answered isDefaultValueAvailable() false.` |
|      - |  1178 | `		 * BOTH quote spellings are accepted because both scanners in vm_arg_check.c` |
|      - |  1179 | `` 		 * step over either one: a native method's zSig lives in C, so `= \"static\"` `` |
|      - |  1180 | `		 * is the natural way to write Closure::bindTo's default. */` |
|      - |  1181 | `		SyBlob sOut;` |
|    244 |  1182 | `		char cQuote = z[0];` |
|      - |  1183 | `		int k;` |
|    244 |  1184 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    596 |  1185 | `		for( k = 1 ; k < n - 1 ; k++ ){` |
|    356 |  1186 | `			char c = z[k];` |
|    356 |  1187 | `			if( c == '\\' && k + 1 < n - 1 ){` |
|    162 |  1188 | `				char e = z[k+1];` |
|    162 |  1189 | `				int bTook = 1;` |
|    162 |  1190 | `				switch( e ){` |
|     31 |  1191 | `					case 'n': c = 0x0A; break;` |
|     27 |  1192 | `					case 'r': c = 0x0D; break;` |
|     23 |  1193 | `					case 't': c = 0x09; break;` |
|     23 |  1194 | `					case 'v': c = 0x0B; break;` |
|      5 |  1195 | `					case 'f': c = 0x0C; break;` |
|    ! 0 |  1196 | `					case 'e': c = 0x1B; break;` |
|      9 |  1197 | `					case 'x': {` |
|     19 |  1198 | `						int h1 = ReflectHexVal(k + 3 < n - 1 ? z[k+2] : 0);` |
|     19 |  1199 | `						int h2 = ReflectHexVal(k + 3 < n - 1 ? z[k+3] : 0);` |
|     19 |  1200 | `						if( h1 < 0 \|\| h2 < 0 ){` |
|    ! 0 |  1201 | `							bTook = 0;` |
|    ! 0 |  1202 | `							break;` |
|      - |  1203 | `						}` |
|     19 |  1204 | `						c = (char)((h1 << 4) \| h2);` |
|     19 |  1205 | `						k += 2;   /* the two hex digits; the 'x' below */` |
|     19 |  1206 | `						break;` |
|      - |  1207 | `					}` |
|     19 |  1208 | `					default:` |
|     40 |  1209 | `						bTook = (e == cQuote \|\| e == '\\');` |
|     40 |  1210 | `						c = e;` |
|     38 |  1211 | `						break;` |
|      - |  1212 | `				}` |
|    162 |  1213 | `				if( bTook ){` |
|    162 |  1214 | `					k++;` |
|     82 |  1215 | `				}else{` |
|    ! 0 |  1216 | `					c = '\\';` |
|      - |  1217 | `				}` |
|     80 |  1218 | `			}` |
|    356 |  1219 | `			SyBlobAppend(&sOut,(const void *)&c,sizeof(char));` |
|    180 |  1220 | `		}` |
|    244 |  1221 | `		ph7_value_string(pOut,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    244 |  1222 | `		SyBlobRelease(&sOut);` |
|    244 |  1223 | `		return 1;` |
|      - |  1224 | `	}` |
|    440 |  1225 | `	if( ReflectSigConstExpr(pCtx,z,n,pOut) ){` |
|     44 |  1226 | `		return 1;` |
|      - |  1227 | `	}` |
|    398 |  1228 | `	if( ReflectSigGlobalConst(pCtx,z,n,pOut) ){` |
|     10 |  1229 | `		return 1;` |
|      - |  1230 | `	}` |
|    390 |  1231 | `	if( ReflectSigProduct(pCtx,z,n,pOut) ){` |
|      3 |  1232 | `		return 1;` |
|      - |  1233 | `	}` |
|      - |  1234 | `	{` |
|    388 |  1235 | `		sxi64 iRadix = 0;` |
|    388 |  1236 | `		if( ReflectSigRadixInt(z,n,&iRadix) ){` |
|      3 |  1237 | `			ph7_value_int64(pOut,iRadix);` |
|      3 |  1238 | `			return 1;` |
|      - |  1239 | `		}` |
|      - |  1240 | `	}` |
|    386 |  1241 | `	if( n > 0 && SyStrIsNumeric(z,(sxu32)n,&bReal,0) == SXRET_OK ){` |
|      - |  1242 | `		/* php's own rule for the text form: a '.', an exponent or a hex marker` |
|      - |  1243 | `		 * makes it a float, everything else an int. */` |
|    326 |  1244 | `		if( bReal \|\| ReflectSigHas(z,n,".",1)` |
|    329 |  1245 | `		 \|\| ReflectSigHasNoCase(z,n,"e",1) \|\| ReflectSigHasNoCase(z,n,"x",1) ){` |
|      - |  1246 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|      - |  1247 | `			/* the VALUE is an out-parameter here; the return is a status, and` |
|      - |  1248 | `			 * reading it as the number made every float default 0.0 */` |
|      3 |  1249 | `			sxreal rVal = 0.0;` |
|      3 |  1250 | `			SyStrToReal(z,(sxu32)n,(void *)&rVal,0);` |
|      3 |  1251 | `			ph7_value_double(pOut,rVal);` |
|      - |  1252 | `#else` |
|      - |  1253 | `			sxi64 iRaw = 0;` |
|      - |  1254 | `			SyStrToInt64(z,(sxu32)n,(void *)&iRaw,0);` |
|      - |  1255 | `			ph7_value_int64(pOut,iRaw);` |
|      - |  1256 | `#endif` |
|      2 |  1257 | `		}else{` |
|    327 |  1258 | `			sxi64 iVal = 0;` |
|    327 |  1259 | `			SyStrToInt64(z,(sxu32)n,(void *)&iVal,0);` |
|    327 |  1260 | `			ph7_value_int64(pOut,iVal);` |
|      - |  1261 | `		}` |
|    330 |  1262 | `		return 1;` |
|      - |  1263 | `	}` |
|     59 |  1264 | `	return 0;` |
|    618 |  1265 | `}` |
|      - |  1266 | `/*` |
|      - |  1267 | ` * One parameter, described uniformly.` |
|      - |  1268 | ` *` |
|      - |  1269 | ` * A reflected function's parameters come from one of TWO places — a compiled` |
|      - |  1270 | `` * function's `ph7_vm_func_arg` list, or the php-style SIGNATURE STRING a C`` |
|      - |  1271 | ` * builtin / native method declares — and both the descriptor array` |
|      - |  1272 | ` * (__reflect_func_info, for the chunks still in PHP) and the native` |
|      - |  1273 | ` * ReflectionParameter have to read them the same way. This struct is what they` |
|      - |  1274 | `` * agree on; `pArg` is set only on the compiled path, where a default is`` |
|      - |  1275 | ` * BYTE-CODE rather than text.` |
|      - |  1276 | ` */` |
|      - |  1277 | `typedef struct ReflectParamDesc ReflectParamDesc;` |
|      - |  1278 | `struct ReflectParamDesc` |
|      - |  1279 | `{` |
|      - |  1280 | `	SyString sName;` |
|      - |  1281 | `	int iPos;` |
|      - |  1282 | `	int bByRef, bVariadic, bHasDef, bOptional, bNullable, bPromoted;` |
|      - |  1283 | `	SyString sType;            /* nByte == 0 -> untyped. For a COMPILED parameter this` |
|      - |  1284 | `	                            * points into zTypeBuf below, php's stored text with` |
|      - |  1285 | ``	                            * `self`/`parent` resolved (see ReflectDeclScope). */`` |
|      - |  1286 | `	char zTypeBuf[192];` |
|      - |  1287 | `	SyString sDefText;         /* signature-declared default TEXT (nByte == 0 -> none) */` |
|      - |  1288 | `	ph7_vm_func_arg *pArg;     /* compiled parameter, or NULL for a declared one */` |
|      - |  1289 | `	int bInternal;             /* owner is INTERNAL to php -- which for a COMPILED` |
|      - |  1290 | `	                            * parameter is the prelude's builtins, and decides` |
|      - |  1291 | `	                            * how php spells its default (see ReflectExportDefault) */` |
|      - |  1292 | `};` |
|      - |  1293 | `/*` |
|      - |  1294 | ` * Split a signature string on its top-level commas (a quoted default may hold` |
|      - |  1295 | ` * its own). Answers the parameter COUNT; when iWant is in range, hands back` |
|      - |  1296 | ` * that part's bytes.` |
|      - |  1297 | ` */` |
|  11506 |  1298 | `static int ReflectSigPart(const char *zSig, int nSig, int iWant,` |
|      - |  1299 | `	const char **pzPart, int *pnPart)` |
|      5 |  1300 | `{` |
|  11511 |  1301 | `	int iPos = 0;` |
|  23843 |  1302 | `	while( nSig > 0 ){` |
|  20927 |  1303 | `		int iComma = ReflectSigFindUnquoted(zSig,nSig,',');` |
|  20927 |  1304 | `		const char *zPart = zSig;` |
|  20927 |  1305 | `		int nPart = iComma < 0 ? nSig : iComma;` |
|  20927 |  1306 | `		ReflectSigTrim(&zPart,&nPart);` |
|  20927 |  1307 | `		if( nPart > 0 ){` |
|  20927 |  1308 | `			if( iPos == iWant && pzPart ){` |
|   5477 |  1309 | `				*pzPart = zPart;` |
|   5477 |  1310 | `				*pnPart = nPart;` |
|   2736 |  1311 | `			}` |
|  20927 |  1312 | `			iPos++;` |
|  10461 |  1313 | `		}` |
|  20927 |  1314 | `		if( iComma < 0 ){` |
|   8595 |  1315 | `			break;` |
|      - |  1316 | `		}` |
|  12337 |  1317 | `		zSig += iComma + 1;` |
|  12337 |  1318 | `		nSig -= iComma + 1;` |
|      5 |  1319 | `	}` |
|  11511 |  1320 | `	return iPos;` |
|      5 |  1321 | `}` |
|      - |  1322 | ``/* One `type &$name = default` part into the uniform description. */`` |
|   5472 |  1323 | `static void ReflectSigDescribe(const char *z, int n, int iPos, ReflectParamDesc *pOut)` |
|      5 |  1324 | `{` |
|   5477 |  1325 | `	const char *zDef = 0;` |
|   5477 |  1326 | `	int nDef = 0;` |
|      - |  1327 | `	int iEq, iDollar, iSpace;` |
|   5477 |  1328 | `	SyZero(pOut,sizeof(*pOut));` |
|   5477 |  1329 | `	pOut->iPos = iPos;` |
|   5477 |  1330 | `	if( n > 0 && z[0] == '~' ){` |
|      - |  1331 | `		/* The signature table's "declared here, screened by the builtin" marker` |
|      - |  1332 | `		 * (see vm_arg_check.c): php DECLARES this type and its C body asks for a` |
|      - |  1333 | `		 * tighter one, so Reflection reports what follows the marker. */` |
|    230 |  1334 | `		z++;` |
|    230 |  1335 | `		n--;` |
|    114 |  1336 | `	}` |
|      - |  1337 | ``	/* `= default` splits off first: everything after the first unquoted '='. */`` |
|   5477 |  1338 | `	iEq = ReflectSigFindUnquoted(z,n,'=');` |
|   5477 |  1339 | `	if( iEq >= 0 ){` |
|   2419 |  1340 | `		zDef = &z[iEq+1];` |
|   2419 |  1341 | `		nDef = n - iEq - 1;` |
|   2419 |  1342 | `		ReflectSigTrim(&zDef,&nDef);` |
|   2419 |  1343 | `		n = iEq;` |
|   2419 |  1344 | `		ReflectSigTrim(&z,&n);` |
|   1207 |  1345 | `	}` |
|   5477 |  1346 | `	if( zDef && nDef == 1 && zDef[0] == '?' ){` |
|      - |  1347 | ``		/* `= ?` is the table's OPTIONAL-but-no-default marker, php's own shape`` |
|      - |  1348 | `		 * for a parameter like ReflectionClass::getStaticPropertyValue()'s` |
|      - |  1349 | `		 * $default: isOptional() true, isDefaultValueAvailable() FALSE, so` |
|      - |  1350 | `		 * getDefaultValue() raises. Reporting it as a default (which is what` |
|      - |  1351 | ``		 * a bare `hasdef` did) made that call answer NULL instead. */`` |
|     98 |  1352 | `		pOut->bOptional = 1;` |
|     98 |  1353 | `		zDef = 0;` |
|     98 |  1354 | `		nDef = 0;` |
|     48 |  1355 | `	}` |
|   5477 |  1356 | `	pOut->bVariadic = ReflectSigHas(z,n,"...",3);` |
|   5477 |  1357 | `	if( pOut->bVariadic ){` |
|      - |  1358 | `		/* php: a variadic parameter never HAS a default -- it defaults to "no` |
|      - |  1359 | `		 * further arguments", which is not a value. Several signature rows still` |
|      - |  1360 | ``		 * write `mixed ...$values = ?` (the table's "optional, unspecified"`` |
|      - |  1361 | `		 * marker), and honouring it made isDefaultValueAvailable() true where php` |
|      - |  1362 | `		 * says false, so getDefaultValue() then threw on printf/array_merge. */` |
|     55 |  1363 | `		zDef = 0;` |
|     55 |  1364 | `		nDef = 0;` |
|     27 |  1365 | `	}` |
|   5477 |  1366 | `	iDollar = ReflectSigFindUnquoted(z,n,'$');` |
|   5477 |  1367 | `	iSpace = ReflectSigFindUnquoted(z,n,' ');` |
|      - |  1368 | `	{` |
|   5477 |  1369 | `		const char *zName = iDollar < 0 ? z : &z[iDollar+1];` |
|   5477 |  1370 | `		int nName = iDollar < 0 ? n : n - iDollar - 1;` |
|   5477 |  1371 | `		SyStringInitFromBuf(&pOut->sName,zName,nName);` |
|      - |  1372 | `	}` |
|   5477 |  1373 | `	pOut->bByRef = ReflectSigHas(z,n,"&",1);` |
|   5477 |  1374 | `	pOut->bHasDef = zDef != 0;` |
|   5477 |  1375 | `	pOut->bOptional = pOut->bOptional \|\| pOut->bVariadic \|\| zDef != 0;` |
|   5477 |  1376 | `	if( iSpace >= 0 && iDollar >= 0 && iSpace < iDollar ){` |
|      - |  1377 | `` 		/* The type is whatever precedes the first space, so `?DOMNode $child` `` |
|      - |  1378 | ``		 * types as `?DOMNode` and an untyped `$x` types as nothing. */`` |
|   5135 |  1379 | `		pOut->bNullable = (z[0] == '?' \|\| ReflectSigHasNoCase(z,iSpace,"null",4));` |
|   5135 |  1380 | `		SyStringInitFromBuf(&pOut->sType,z,iSpace);` |
|   2565 |  1381 | `	}` |
|   5477 |  1382 | `	if( zDef ){` |
|   2323 |  1383 | `		SyStringInitFromBuf(&pOut->sDefText,zDef,nDef);` |
|   1159 |  1384 | `	}` |
|   5477 |  1385 | `}` |
|      - |  1386 | `/*` |
|      - |  1387 | ` * Resolve a Generator object into its wrapper. Mirrors the static` |
|      - |  1388 | ` * VmGeneratorExtractCtx in vm.c: the $__ctx attribute carries the` |
|      - |  1389 | ` * ph7_generator pointer as a resource value.` |
|      - |  1390 | ` */` |
|     44 |  1391 | `static ph7_generator * ReflectGeneratorCtx(ph7_vm *pVm, ph7_value *pVal)` |
|      1 |  1392 | `{` |
|      - |  1393 | `	ph7_class_instance *pThis;` |
|      - |  1394 | `	ph7_value *pAttr;` |
|      - |  1395 | `	SyString sAttr;` |
|     45 |  1396 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVm->pGeneratorClass == 0 ){` |
|    ! 0 |  1397 | `		return 0;` |
|      - |  1398 | `	}` |
|     45 |  1399 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     45 |  1400 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|    ! 0 |  1401 | `		return 0;` |
|      - |  1402 | `	}` |
|     45 |  1403 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|     45 |  1404 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     45 |  1405 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|    ! 0 |  1406 | `		return 0;` |
|      - |  1407 | `	}` |
|     45 |  1408 | `	return (ph7_generator *)pAttr->x.pOther;` |
|     23 |  1409 | `}` |
|      - |  1410 | `/*` |
|      - |  1411 | ` * Evaluate the recorded argument expressions of one declared attribute and` |
|      - |  1412 | ` * answer them as a PHP array, or NULL when the target no longer resolves.` |
|      - |  1413 | ` *` |
|      - |  1414 | ` * apArg is the four-part SPEC a ReflectionAttribute carries — kind 'class'` |
|      - |  1415 | ` * (target = class), 'attr' (class + property/constant name), 'method' (class +` |
|      - |  1416 | ` * method), 'fn' (function name or Closure), 'param' (function spec + parameter` |
|      - |  1417 | ` * index), 'const' (global constant name) — plus which attribute of that target.` |
|      - |  1418 | ` * Named arguments become string keys.` |
|      - |  1419 | ` *` |
|      - |  1420 | ` * The values are evaluated HERE rather than when the reflector was built:` |
|      - |  1421 | `` * `#[Attr(self::SOME)]` runs php code, and php runs it at getArguments() time.`` |
|      - |  1422 | ` */` |
|    182 |  1423 | `static ph7_value * ReflectAttrArgs(ph7_context *pCtx, ph7_value **apArg, sxu32 nAttrIdx)` |
|      5 |  1424 | `{` |
|    187 |  1425 | `	ph7_vm *pVm = pCtx->pVm;` |
|    187 |  1426 | `	SySet *pAttrs = 0;` |
|    187 |  1427 | `	ph7_class *pDeclCls = 0; /* class an attribute is declared on: scope for self:: in its args */` |
|      - |  1428 | `	ph7_attribute *pAttrRec;` |
|      - |  1429 | `	ph7_value *pOut;` |
|      - |  1430 | `	const char *zKind;` |
|      - |  1431 | `	int nKind;` |
|      - |  1432 | `	sxu32 n;` |
|    187 |  1433 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|    259 |  1434 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|    149 |  1435 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|    149 |  1436 | `		if( pClass ){ pAttrs = &pClass->aAttrs; pDeclCls = pClass; }` |
|    116 |  1437 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|      5 |  1438 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|      5 |  1439 | `		ph7_class_attr *pMember = pClass ? ReflectFetchMember(pClass, apArg[2]) : 0;` |
|      5 |  1440 | `		if( pMember ){ pAttrs = &pMember->aAttrs; pDeclCls = pClass; }` |
|     47 |  1441 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|     16 |  1442 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     16 |  1443 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|     36 |  1444 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|     15 |  1445 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], 0, 0, 0, 0, 0);` |
|     15 |  1446 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|     18 |  1447 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|      7 |  1448 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], apArg[2], 0, 0, 0, 0);` |
|      7 |  1449 | `		ph7_vm_func_arg *pParam = pFunc` |
|      6 |  1450 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|      7 |  1451 | `		if( pParam ){ pAttrs = &pParam->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|      6 |  1452 | `	}else if( nKind == 5 && SyMemcmp(zKind, "const", 5) == 0 ){` |
|      - |  1453 | ``		/* Global constant (php 8.5 attributes on `const` statements) */`` |
|      - |  1454 | `		const char *zCName;` |
|      - |  1455 | `		int nCName;` |
|      - |  1456 | `		SyHashEntry *pCEntry;` |
|      3 |  1457 | `		zCName = ph7_value_to_string(apArg[1], &nCName);` |
|      3 |  1458 | `		pCEntry = nCName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zCName, (sxu32)nCName) : 0;` |
|      3 |  1459 | `		if( pCEntry ){ pAttrs = &((ph7_constant *)pCEntry->pUserData)->aAttrs; }` |
|      1 |  1460 | `	}` |
|    182 |  1461 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|    187 |  1462 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|    ! 0 |  1463 | `		return 0;` |
|      - |  1464 | `	}` |
|    403 |  1465 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|    221 |  1466 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|      - |  1467 | `		ph7_value sValue;` |
|    221 |  1468 | `		PH7_MemObjInit(pVm, &sValue);` |
|    221 |  1469 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|      - |  1470 | `` 			/* Evaluate under the attribute's declaring-class scope so `self::class` `` |
|      - |  1471 | ``			 * / `self::CONST` / `parent::` resolve like php (else self:: would bind`` |
|      - |  1472 | `			 * to the reflection machinery's own class). */` |
|    185 |  1473 | `			PH7_VmExecAttrArg(pVm, &pArgRec->aByteCode, pDeclCls, &sValue);` |
|    129 |  1474 | `		}else if( pArgRec->pNativeValue ){` |
|      - |  1475 | ``			/* A NATIVE attribute (php's own `#[Attribute(...)]` on Attribute and`` |
|      - |  1476 | `			 * Deprecated): the argument is a literal, not byte-code. */` |
|     38 |  1477 | `			PH7_NativeLiteralValue(pVm, pArgRec->pNativeValue, &sValue);` |
|     17 |  1478 | `		}` |
|    221 |  1479 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|     13 |  1480 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|      7 |  1481 | `		}else{` |
|    209 |  1482 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|      - |  1483 | `		}` |
|    221 |  1484 | `		PH7_MemObjRelease(&sValue);` |
|    113 |  1485 | `	}` |
|    187 |  1486 | `	return pOut;` |
|     96 |  1487 | `}` |
|      - |  1488 | `/*` |
|      - |  1489 | ` * ---------------------------------------------------------------------------` |
|      - |  1490 | ` * The ReflectionType family.` |
|      - |  1491 | ` *` |
|      - |  1492 | ` * php's four type objects are pure VALUES: a text, a nullability flag, and for` |
|      - |  1493 | ` * the composites a list of members. Nothing in userland can build one — php` |
|      - |  1494 | `` * declares no constructor on any of them and refuses `clone` — so the prelude's`` |
|      - |  1495 | `` * public `__construct($name, $nullable, $text)` existed only because the factory`` |
|      - |  1496 | ` * that fills them was itself PHP. The factory is C now (ReflectMakeType), so the` |
|      - |  1497 | ` * constructors are gone and the classes say what php's say.` |
|      - |  1498 | ` * ---------------------------------------------------------------------------` |
|      - |  1499 | ` */` |
|      - |  1500 | `#define RT_TEXT     "__text"` |
|      - |  1501 | `#define RT_NULLABLE "__nullable"` |
|      - |  1502 | `#define RT_TNAME    "__tname"` |
|      - |  1503 | `#define RT_TYPES    "__types"` |
|      - |  1504 |  |
|     26 |  1505 | `static int vm_builtin_ReflectionType_allowsNull(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  1506 | `{` |
|     28 |  1507 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     13 |  1508 | `	SXUNUSED(nArg);` |
|     13 |  1509 | `	SXUNUSED(apArg);` |
|     28 |  1510 | `	ph7_result_bool(pCtx, pThis != 0 && PH7_NativeAttrTruthy(pThis, RT_NULLABLE));` |
|     28 |  1511 | `	return PH7_OK;` |
|      2 |  1512 | `}` |
|   1299 |  1513 | `static int vm_builtin_ReflectionType_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  1514 | `{` |
|   1304 |  1515 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   1304 |  1516 | `	const char *zText = "";` |
|   1304 |  1517 | `	int nText = 0;` |
|    648 |  1518 | `	SXUNUSED(nArg);` |
|    648 |  1519 | `	SXUNUSED(apArg);` |
|   1304 |  1520 | `	if( pThis ){` |
|   1304 |  1521 | `		PH7_NativeAttrStr(pThis, RT_TEXT, &zText, &nText);` |
|    648 |  1522 | `	}` |
|   1304 |  1523 | `	ph7_result_string(pCtx, zText, nText);` |
|   1304 |  1524 | `	return PH7_OK;` |
|      5 |  1525 | `}` |
|     22 |  1526 | `static int vm_builtin_ReflectionNamedType_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  1527 | `{` |
|     25 |  1528 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     25 |  1529 | `	const char *zName = "";` |
|     25 |  1530 | `	int nName = 0;` |
|     11 |  1531 | `	SXUNUSED(nArg);` |
|     11 |  1532 | `	SXUNUSED(apArg);` |
|     25 |  1533 | `	if( pThis ){` |
|     25 |  1534 | `		PH7_NativeAttrStr(pThis, RT_TNAME, &zName, &nName);` |
|     11 |  1535 | `	}` |
|     25 |  1536 | `	ph7_result_string(pCtx, zName, nName);` |
|     25 |  1537 | `	return PH7_OK;` |
|      3 |  1538 | `}` |
|      - |  1539 | `/* php's builtin-type set, case-insensitively. Anything else is a class name. */` |
|     46 |  1540 | `static int ReflectTypeIsBuiltin(const char *zName, int nName)` |
|      1 |  1541 | `{` |
|      - |  1542 | `	static const char *azBuiltin[] = {` |
|      - |  1543 | `		"int","float","string","bool","array","object","mixed",` |
|      - |  1544 | `		"void","never","null","callable","iterable","true","false"` |
|      - |  1545 | `	};` |
|      - |  1546 | `	sxu32 n;` |
|    369 |  1547 | `	for( n = 0 ; n < SX_ARRAYSIZE(azBuiltin) ; n++ ){` |
|    359 |  1548 | `		int nWant = (int)SyStrlen(azBuiltin[n]);` |
|      - |  1549 | `		int k;` |
|    359 |  1550 | `		if( nWant != nName ){` |
|    283 |  1551 | `			continue;` |
|      - |  1552 | `		}` |
|    243 |  1553 | `		for( k = 0 ; k < nWant ; k++ ){` |
|    207 |  1554 | `			if( SyToLower(zName[k]) != azBuiltin[n][k] ){` |
|     41 |  1555 | `				break;` |
|      - |  1556 | `			}` |
|     84 |  1557 | `		}` |
|     77 |  1558 | `		if( k == nWant ){` |
|     37 |  1559 | `			return 1;` |
|      - |  1560 | `		}` |
|     21 |  1561 | `	}` |
|     11 |  1562 | `	return 0;` |
|     24 |  1563 | `}` |
|     38 |  1564 | `static int vm_builtin_ReflectionNamedType_isBuiltin(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  1565 | `{` |
|     39 |  1566 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     39 |  1567 | `	const char *zName = "";` |
|     39 |  1568 | `	int nName = 0;` |
|     19 |  1569 | `	SXUNUSED(nArg);` |
|     19 |  1570 | `	SXUNUSED(apArg);` |
|     39 |  1571 | `	if( pThis ){` |
|     39 |  1572 | `		PH7_NativeAttrStr(pThis, RT_TNAME, &zName, &nName);` |
|     19 |  1573 | `	}` |
|     39 |  1574 | `	ph7_result_bool(pCtx, ReflectTypeIsBuiltin(zName, nName));` |
|     39 |  1575 | `	return PH7_OK;` |
|      1 |  1576 | `}` |
|      6 |  1577 | `static int vm_builtin_ReflectionType_getTypes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  1578 | `{` |
|      7 |  1579 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      7 |  1580 | `	ph7_value *pTypes = pThis ? PH7_NativeAttr(pThis, RT_TYPES) : 0;` |
|      3 |  1581 | `	SXUNUSED(nArg);` |
|      3 |  1582 | `	SXUNUSED(apArg);` |
|      7 |  1583 | `	if( pTypes && (pTypes->iFlags & MEMOBJ_HASHMAP) ){` |
|      7 |  1584 | `		ph7_result_value(pCtx, pTypes);` |
|      4 |  1585 | `	}else{` |
|      - |  1586 | `		/* The declared default is a native NULL slot (a spec cannot carry an` |
|      - |  1587 | `		 * array literal), but php's getTypes() always answers a list. */` |
|    ! 0 |  1588 | `		ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|    ! 0 |  1589 | `		if( pEmpty ){` |
|    ! 0 |  1590 | `			ph7_result_value(pCtx, pEmpty);` |
|    ! 0 |  1591 | `		}` |
|      - |  1592 | `	}` |
|      7 |  1593 | `	return PH7_OK;` |
|      1 |  1594 | `}` |
|      - |  1595 | `/* Build one of the three concrete types with its slots filled. The caller owns` |
|      - |  1596 | ` * the reference (PH7_NativeResultObject / a list insert drops it). */` |
|   1919 |  1597 | `static ph7_class_instance * ReflectNewType(ph7_context *pCtx, const char *zClass,` |
|      - |  1598 | `	const char *zText, int nText, int bNullable)` |
|      5 |  1599 | `{` |
|   1924 |  1600 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1924 |  1601 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|   1924 |  1602 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|   1924 |  1603 | `	if( pObj == 0 ){` |
|    ! 0 |  1604 | `		return 0;` |
|      - |  1605 | `	}` |
|   1924 |  1606 | `	PH7_NativeSetAttrStr(pVm, pObj, RT_TEXT, zText, nText);` |
|   1924 |  1607 | `	PH7_NativeSetAttrBool(pVm, pObj, RT_NULLABLE, bNullable);` |
|   1924 |  1608 | `	return pObj;` |
|    963 |  1609 | `}` |
|      - |  1610 | `/*` |
|      - |  1611 | ` * Append pType to a composite's member list, handing over the caller's reference.` |
|      - |  1612 | ` *` |
|      - |  1613 | ` * The carrier is a STACK value, deliberately: a ph7_context_new_scalar() is` |
|      - |  1614 | ` * released with the call context, and releasing a MEMOBJ_OBJ carrier unrefs the` |
|      - |  1615 | ` * instance a second time — which freed every member of a union or intersection` |
|      - |  1616 | ` * the moment its factory returned, so the list came back holding dead objects.` |
|      - |  1617 | ` * PH7_NativeResultObject hands an instance over the same way.` |
|      - |  1618 | ` */` |
|    710 |  1619 | `static void ReflectTypeListAdd(ph7_context *pCtx, ph7_value *pList, ph7_class_instance *pType)` |
|      5 |  1620 | `{` |
|      - |  1621 | `	ph7_value sVal;` |
|    715 |  1622 | `	if( pType == 0 ){` |
|    ! 0 |  1623 | `		return;` |
|      - |  1624 | `	}` |
|    715 |  1625 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    715 |  1626 | `	sVal.x.pOther = pType;` |
|    715 |  1627 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|    715 |  1628 | `	ph7_array_add_elem(pList, 0, &sVal);   /* takes its own reference */` |
|    715 |  1629 | `	PH7_ClassInstanceUnref(pType);` |
|    360 |  1630 | `}` |
|      - |  1631 | `/* Exact, case-insensitive name test (php lower-cases before comparing). */` |
|   2946 |  1632 | `static int ReflectTypeNameIs(const char *z, int n, const char *zWant)` |
|      5 |  1633 | `{` |
|   2951 |  1634 | `	int nWant = (int)SyStrlen(zWant), k;` |
|   2951 |  1635 | `	if( n != nWant ){` |
|   2490 |  1636 | `		return 0;` |
|      - |  1637 | `	}` |
|    734 |  1638 | `	for( k = 0 ; k < n ; k++ ){` |
|    680 |  1639 | `		if( SyToLower(z[k]) != zWant[k] ){` |
|    412 |  1640 | `			return 0;` |
|      - |  1641 | `		}` |
|    136 |  1642 | `	}` |
|     56 |  1643 | `	return 1;` |
|   1475 |  1644 | `}` |
|      - |  1645 | `/*` |
|      - |  1646 | ` * A ReflectionNamedType for one name.` |
|      - |  1647 | ` *` |
|      - |  1648 | ` * bQMark says the text carried a leading '?', which is the ONLY thing that puts` |
|      - |  1649 | `` * one back on the rendered text — while `null` and `mixed` are nullable by their`` |
|      - |  1650 | ` * own meaning without ever rendering a '?'. Those two rules are independent, and` |
|      - |  1651 | `` * conflating them is how `?array` came out as `array`.`` |
|      - |  1652 | ` */` |
|   1709 |  1653 | `static ph7_class_instance * ReflectNewNamed(ph7_context *pCtx, const char *z, int n, int bQMark)` |
|      5 |  1654 | `{` |
|      - |  1655 | `	ph7_class_instance *pObj;` |
|      - |  1656 | `	char zBuf[256];` |
|   1714 |  1657 | `	const char *zText = z;` |
|   1714 |  1658 | `	int nText = n;` |
|   1714 |  1659 | `	int bNullable = bQMark \|\| ReflectTypeNameIs(z, n, "null") \|\| ReflectTypeNameIs(z, n, "mixed");` |
|   1714 |  1660 | `	if( bQMark && n + 1 < (int)sizeof(zBuf) ){` |
|    249 |  1661 | `		zBuf[0] = '?';` |
|    249 |  1662 | `		SyMemcpy(z, &zBuf[1], (sxu32)n);` |
|    249 |  1663 | `		zText = zBuf;` |
|    249 |  1664 | `		nText = n + 1;` |
|    123 |  1665 | `	}` |
|   1714 |  1666 | `	pObj = ReflectNewType(pCtx, "ReflectionNamedType", zText, nText, bNullable);` |
|   1714 |  1667 | `	if( pObj ){` |
|   1714 |  1668 | `		PH7_NativeSetAttrStr(pCtx->pVm, pObj, RT_TNAME, z, n);` |
|    853 |  1669 | `	}` |
|   1714 |  1670 | `	return pObj;` |
|      5 |  1671 | `}` |
|      - |  1672 | `/*` |
|      - |  1673 | `` * One ATOM of a type text: `?X`, `(A&B)` or a plain name. An intersection is`` |
|      - |  1674 | ` * the only composite an atom can be, because php only nests that way.` |
|      - |  1675 | ` */` |
|   1697 |  1676 | `static ph7_class_instance * ReflectMakeAtom(ph7_context *pCtx, const char *z, int n)` |
|      5 |  1677 | `{` |
|   1702 |  1678 | `	int bQMark = 0;` |
|   1702 |  1679 | `	if( n > 0 && z[0] == '?' ){` |
|    249 |  1680 | `		bQMark = 1;` |
|    249 |  1681 | `		z++;` |
|    249 |  1682 | `		n--;` |
|    123 |  1683 | `	}` |
|   1702 |  1684 | `	if( n > 1 && z[0] == '(' && z[n-1] == ')' ){` |
|     11 |  1685 | `		z++;` |
|     11 |  1686 | `		n -= 2;` |
|      4 |  1687 | `	}` |
|   1702 |  1688 | `	if( ReflectSigFindUnquoted(z, n, '&') >= 0 ){` |
|     15 |  1689 | `		ph7_class_instance *pObj = ReflectNewType(pCtx, "ReflectionIntersectionType", z, n, 0);` |
|     15 |  1690 | `		ph7_value *pList = ph7_context_new_array(pCtx);` |
|     15 |  1691 | `		const char *zCur = z;` |
|     15 |  1692 | `		int nCur = n;` |
|     15 |  1693 | `		if( pObj == 0 \|\| pList == 0 ){` |
|    ! 0 |  1694 | `			return pObj;` |
|      - |  1695 | `		}` |
|     27 |  1696 | `		while( nCur > 0 ){` |
|     27 |  1697 | `			int iCut = ReflectSigFindUnquoted(zCur, nCur, '&');` |
|     39 |  1698 | `			ReflectTypeListAdd(pCtx, pList,` |
|     12 |  1699 | `				ReflectNewNamed(pCtx, zCur, iCut < 0 ? nCur : iCut, 0));` |
|     27 |  1700 | `			if( iCut < 0 ){` |
|     15 |  1701 | `				break;` |
|      - |  1702 | `			}` |
|     15 |  1703 | `			zCur += iCut + 1;` |
|     15 |  1704 | `			nCur -= iCut + 1;` |
|      3 |  1705 | `		}` |
|     15 |  1706 | `		PH7_NativeSetProp(pCtx->pVm, pObj, RT_TYPES, sizeof(RT_TYPES)-1, pList);` |
|     15 |  1707 | `		return pObj;` |
|      - |  1708 | `	}` |
|   1690 |  1709 | `	return ReflectNewNamed(pCtx, z, n, bQMark);` |
|    852 |  1710 | `}` |
|      - |  1711 | `/*` |
|      - |  1712 | ` * A declared type TEXT to the object php answers for it: a named type, a union,` |
|      - |  1713 | `` * or an intersection. NULL for an absent type. `X\|null` collapses back to a`` |
|      - |  1714 | `` * NULLABLE named type, which is what php reports (`?X`), but only when exactly`` |
|      - |  1715 | ` * one non-null arm is left and it is not itself an intersection.` |
|      - |  1716 | ` */` |
|   1425 |  1717 | `static ph7_class_instance * ReflectMakeType(ph7_context *pCtx, const char *zText, int nText)` |
|      5 |  1718 | `{` |
|   1430 |  1719 | `	const char *zBody = zText;` |
|   1430 |  1720 | `	int nBody = nText;` |
|   1430 |  1721 | `	int bNullable = 0, bHasNull = 0, nParts = 0, nNonNull = 0;` |
|   1430 |  1722 | `	const char *zLastNonNull = 0;` |
|   1430 |  1723 | `	int nLastNonNull = 0;` |
|      - |  1724 | `	const char *zCur;` |
|      - |  1725 | `	int nCur, iDepth, k, iStart;` |
|   1430 |  1726 | `	if( nText < 1 ){` |
|    ! 0 |  1727 | `		return 0;` |
|      - |  1728 | `	}` |
|   1430 |  1729 | `	if( zBody[0] == '?' ){` |
|    249 |  1730 | `		bNullable = 1;` |
|    249 |  1731 | `		zBody++;` |
|    249 |  1732 | `		nBody--;` |
|    123 |  1733 | `	}` |
|      - |  1734 | ``	/* Split on the TOP-LEVEL '\|' only: `A\|(B&C)` has one at depth 0 and none`` |
|      - |  1735 | `	 * inside the parentheses. */` |
|   1430 |  1736 | `	iDepth = 0;` |
|   1430 |  1737 | `	iStart = 0;` |
|  13475 |  1738 | `	for( k = 0 ; k <= nBody ; k++ ){` |
|  12050 |  1739 | `		if( k < nBody && zBody[k] == '(' ){` |
|     11 |  1740 | `			iDepth++;` |
|     11 |  1741 | `			continue;` |
|      - |  1742 | `		}` |
|  12042 |  1743 | `		if( k < nBody && zBody[k] == ')' ){` |
|     11 |  1744 | `			iDepth--;` |
|     11 |  1745 | `			continue;` |
|      - |  1746 | `		}` |
|  12034 |  1747 | `		if( k == nBody \|\| (zBody[k] == '\|' && iDepth == 0) ){` |
|   1702 |  1748 | `			zCur = &zBody[iStart];` |
|   1702 |  1749 | `			nCur = k - iStart;` |
|   1702 |  1750 | `			nParts++;` |
|   1702 |  1751 | `			if( nCur == 4 && ReflectSigHasNoCase(zCur, 4, "null", 4) ){` |
|     10 |  1752 | `				bHasNull = 1;` |
|      6 |  1753 | `			}else{` |
|   1694 |  1754 | `				nNonNull++;` |
|   1694 |  1755 | `				zLastNonNull = zCur;` |
|   1694 |  1756 | `				nLastNonNull = nCur;` |
|      - |  1757 | `			}` |
|   1702 |  1758 | `			iStart = k + 1;` |
|    847 |  1759 | `		}` |
|   6012 |  1760 | `	}` |
|   1430 |  1761 | `	if( nParts > 1 ){` |
|      - |  1762 | `		ph7_class_instance *pObj;` |
|      - |  1763 | `		ph7_value *pList;` |
|    198 |  1764 | `		if( bHasNull && nNonNull == 1` |
|      7 |  1765 | `		 && ReflectSigFindUnquoted(zLastNonNull, nLastNonNull, '&') < 0 ){` |
|      - |  1766 | ``			/* `X\|null` IS `?X` to php. */`` |
|      - |  1767 | `			char zBuf[256];` |
|      - |  1768 | `			ph7_class_instance *pNamed;` |
|    ! 0 |  1769 | `			const char *zRender = zLastNonNull;` |
|    ! 0 |  1770 | `			int nRender = nLastNonNull;` |
|    ! 0 |  1771 | `			if( nLastNonNull + 1 < (int)sizeof(zBuf) ){` |
|    ! 0 |  1772 | `				zBuf[0] = '?';` |
|    ! 0 |  1773 | `				SyMemcpy(zLastNonNull, &zBuf[1], (sxu32)nLastNonNull);` |
|    ! 0 |  1774 | `				zRender = zBuf;` |
|    ! 0 |  1775 | `				nRender = nLastNonNull + 1;` |
|    ! 0 |  1776 | `			}` |
|    ! 0 |  1777 | `			pNamed = ReflectNewType(pCtx, "ReflectionNamedType", zRender, nRender, 1);` |
|    ! 0 |  1778 | `			if( pNamed ){` |
|    ! 0 |  1779 | `				PH7_NativeSetAttrStr(pCtx->pVm, pNamed, RT_TNAME, zLastNonNull, nLastNonNull);` |
|    ! 0 |  1780 | `			}` |
|    ! 0 |  1781 | `			return pNamed;` |
|      - |  1782 | `		}` |
|    202 |  1783 | `		pObj = ReflectNewType(pCtx, "ReflectionUnionType", zBody, nBody, bNullable \|\| bHasNull);` |
|    202 |  1784 | `		pList = ph7_context_new_array(pCtx);` |
|    202 |  1785 | `		if( pObj == 0 \|\| pList == 0 ){` |
|    ! 0 |  1786 | `			return pObj;` |
|      - |  1787 | `		}` |
|    202 |  1788 | `		iDepth = 0;` |
|    202 |  1789 | `		iStart = 0;` |
|   3474 |  1790 | `		for( k = 0 ; k <= nBody ; k++ ){` |
|   3276 |  1791 | `			if( k < nBody && zBody[k] == '(' ){` |
|     11 |  1792 | `				iDepth++;` |
|     11 |  1793 | `				continue;` |
|      - |  1794 | `			}` |
|   3268 |  1795 | `			if( k < nBody && zBody[k] == ')' ){` |
|     11 |  1796 | `				iDepth--;` |
|     11 |  1797 | `				continue;` |
|      - |  1798 | `			}` |
|   3260 |  1799 | `			if( k == nBody \|\| (zBody[k] == '\|' && iDepth == 0) ){` |
|    709 |  1800 | `				ReflectTypeListAdd(pCtx, pList,` |
|    235 |  1801 | `					ReflectMakeAtom(pCtx, &zBody[iStart], k - iStart));` |
|    474 |  1802 | `				iStart = k + 1;` |
|    235 |  1803 | `			}` |
|   1632 |  1804 | `		}` |
|    202 |  1805 | `		PH7_NativeSetProp(pCtx->pVm, pObj, RT_TYPES, sizeof(RT_TYPES)-1, pList);` |
|    202 |  1806 | `		return pObj;` |
|      - |  1807 | `	}` |
|   1232 |  1808 | `	if( ReflectSigFindUnquoted(zBody, nBody, '&') >= 0 ){` |
|      5 |  1809 | `		return ReflectMakeAtom(pCtx, zBody, nBody);` |
|      - |  1810 | `	}` |
|   1228 |  1811 | `	if( bNullable ){` |
|      - |  1812 | `		char zBuf[256];` |
|    249 |  1813 | `		if( nBody + 1 < (int)sizeof(zBuf) ){` |
|    249 |  1814 | `			zBuf[0] = '?';` |
|    249 |  1815 | `			SyMemcpy(zBody, &zBuf[1], (sxu32)nBody);` |
|    249 |  1816 | `			return ReflectMakeAtom(pCtx, zBuf, nBody + 1);` |
|      - |  1817 | `		}` |
|    ! 0 |  1818 | `	}` |
|    982 |  1819 | `	return ReflectMakeAtom(pCtx, zBody, nBody);` |
|    716 |  1820 | `}` |
|      - |  1821 | `/*` |
|      - |  1822 | ` * Declare the four type classes. Called from PH7_VmInstallReflection() where` |
|      - |  1823 | `` * chunk 4 used to be compiled, so `Stringable` (a core interface) already`` |
|      - |  1824 | ` * exists. PH7_CLASS_NOCLONE is php's own rule for these: they are values the` |
|      - |  1825 | `` * engine hands out, and `clone $type` is an Error, not a copy.`` |
|      - |  1826 | ` */` |
|   6721 |  1827 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionTypes(ph7_vm *pVm)` |
|      5 |  1828 | `{` |
|      - |  1829 | `	static const PH7_NativePropDef aBaseProp[] = {` |
|      - |  1830 | `		{ RT_TEXT,     PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - |  1831 | `		{ RT_NULLABLE, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 }, 0 },` |
|      - |  1832 | `	};` |
|      - |  1833 | `	static const PH7_NativeMethodDef aBaseMethod[] = {` |
|      - |  1834 | `		{ "allowsNull", PH7_MOD_PUBLIC, "", "@bool",       vm_builtin_ReflectionType_allowsNull },` |
|      - |  1835 | `		{ "__toString", PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionType_toString },` |
|      - |  1836 | `	};` |
|      - |  1837 | `	static const PH7_NativePropDef aNamedProp[] = {` |
|      - |  1838 | `		{ RT_TNAME, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - |  1839 | `	};` |
|      - |  1840 | `	static const PH7_NativeMethodDef aNamedMethod[] = {` |
|      - |  1841 | `		{ "getName",   PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionNamedType_getName },` |
|      - |  1842 | `		{ "isBuiltin", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionNamedType_isBuiltin },` |
|      - |  1843 | `	};` |
|      - |  1844 | `	static const PH7_NativePropDef aCompProp[] = {` |
|      - |  1845 | `		{ RT_TYPES, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  1846 | `	};` |
|      - |  1847 | `	static const PH7_NativeMethodDef aCompMethod[] = {` |
|      - |  1848 | `		{ "getTypes", PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionType_getTypes },` |
|      - |  1849 | `	};` |
|      - |  1850 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  1851 | `		{ "ReflectionType", 0, "Stringable", PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  1852 | `		  aBaseMethod, SX_ARRAYSIZE(aBaseMethod), 0, 0, aBaseProp, SX_ARRAYSIZE(aBaseProp), 0, 0, 0 },` |
|      - |  1853 | `		{ "ReflectionNamedType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  1854 | `		  aNamedMethod, SX_ARRAYSIZE(aNamedMethod), 0, 0, aNamedProp, SX_ARRAYSIZE(aNamedProp), 0, 0, 0 },` |
|      - |  1855 | `		{ "ReflectionUnionType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  1856 | `		  aCompMethod, SX_ARRAYSIZE(aCompMethod), 0, 0, aCompProp, SX_ARRAYSIZE(aCompProp), 0, 0, 0 },` |
|      - |  1857 | `		{ "ReflectionIntersectionType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  1858 | `		  aCompMethod, SX_ARRAYSIZE(aCompMethod), 0, 0, aCompProp, SX_ARRAYSIZE(aCompProp), 0, 0, 0 },` |
|      - |  1859 | `	};` |
|   6726 |  1860 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  1861 | `}` |
|      - |  1862 | `/*` |
|      - |  1863 | ` * ---------------------------------------------------------------------------` |
|      - |  1864 | ` * The six standalone reflection classes.` |
|      - |  1865 | ` *` |
|      - |  1866 | ` * ReflectionGenerator, ReflectionFiber, ReflectionConstant, ReflectionExtension,` |
|      - |  1867 | ` * ReflectionZendExtension and ReflectionReference reflect ENGINE state rather` |
|      - |  1868 | ` * than a class hierarchy, so each was a prelude class over one or two thunks` |
|      - |  1869 | ` * whose only job was to hand that state to PHP. Their bodies are the C now, and` |
|      - |  1870 | ` * the four thunks that existed for them (__reflect_gen_info, __reflect_gen_exec,` |
|      - |  1871 | ` * __reflect_const_info, __reflect_ref_id) are gone with them.` |
|      - |  1872 | ` *` |
|      - |  1873 | ` * The ReflectionEnum family stays in the prelude: it extends ReflectionClass and` |
|      - |  1874 | ` * ReflectionClassConstant, which are still PHP.` |
|      - |  1875 | ` * ---------------------------------------------------------------------------` |
|      - |  1876 | ` */` |
|      - |  1877 | `#define RG_GEN   "__gen"` |
|      - |  1878 | `#define RF_FIBER "__fiber"` |
|      - |  1879 | `#define RR_ID    "__id"` |
|      - |  1880 |  |
|      - |  1881 | `/* Build an instance of a class that may still be PRELUDE PHP, running its` |
|      - |  1882 | ` * constructor. Answers 0 when the constructor threw (rc says which). */` |
|   2230 |  1883 | `static ph7_class_instance * ReflectConstruct(ph7_context *pCtx, const char *zClass,` |
|      - |  1884 | `	int nArg, ph7_value **apArg, sxi32 *pRc)` |
|      5 |  1885 | `{` |
|   2235 |  1886 | `	ph7_vm *pVm = pCtx->pVm;` |
|   2235 |  1887 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|      - |  1888 | `	ph7_class_instance *pThis;` |
|      - |  1889 | `	ph7_class_method *pCons;` |
|   2235 |  1890 | `	*pRc = PH7_OK;` |
|   2235 |  1891 | `	if( pClass == 0 ){` |
|    ! 0 |  1892 | `		return 0;` |
|      - |  1893 | `	}` |
|   2235 |  1894 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|   2235 |  1895 | `	if( pThis == 0 ){` |
|    ! 0 |  1896 | `		return 0;` |
|      - |  1897 | `	}` |
|   2235 |  1898 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|   2235 |  1899 | `	if( pCons ){` |
|   2235 |  1900 | `		sxi32 rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, nArg, apArg);` |
|   2235 |  1901 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|    ! 0 |  1902 | `			PH7_ClassInstanceCtorFailed(pThis);` |
|    ! 0 |  1903 | `			PH7_ClassInstanceUnref(pThis);` |
|    ! 0 |  1904 | `			*pRc = rc;` |
|    ! 0 |  1905 | `			return 0;` |
|      - |  1906 | `		}` |
|   1115 |  1907 | `	}` |
|   2235 |  1908 | `	return pThis;` |
|   1120 |  1909 | `}` |
|      - |  1910 | `/* The instance a native reflector wraps ($this->__gen / $this->__fiber). */` |
|     50 |  1911 | `static ph7_class_instance * ReflectWrapped(ph7_context *pCtx, const char *zSlot)` |
|      1 |  1912 | `{` |
|     51 |  1913 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     51 |  1914 | `	return pThis ? PH7_NativeAttrObj(pThis, zSlot) : 0;` |
|      1 |  1915 | `}` |
|      - |  1916 | `/* Hand back a wrapped instance the receiver already owns (a borrowed reference). */` |
|     10 |  1917 | `static int ReflectResultBorrowed(ph7_context *pCtx, ph7_class_instance *pObj)` |
|      1 |  1918 | `{` |
|      - |  1919 | `	ph7_value sVal;` |
|     11 |  1920 | `	if( pObj == 0 ){` |
|    ! 0 |  1921 | `		ph7_result_null(pCtx);` |
|    ! 0 |  1922 | `		return PH7_OK;` |
|      - |  1923 | `	}` |
|     11 |  1924 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     11 |  1925 | `	sVal.x.pOther = pObj;` |
|     11 |  1926 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|     11 |  1927 | `	ph7_result_value(pCtx, &sVal);   /* takes its own reference */` |
|     11 |  1928 | `	return PH7_OK;` |
|      6 |  1929 | `}` |
|      - |  1930 | `/*` |
|      - |  1931 | ` * ---------------------------------------------------------------------------` |
|      - |  1932 | ` * ReflectionAttribute — chunk 7.` |
|      - |  1933 | ` *` |
|      - |  1934 | ` * php's own class, plus the shared getAttributes() body that produces it. The` |
|      - |  1935 | ` * chunk it replaces also carried __reflect_target_names (folded into the` |
|      - |  1936 | ` * "cannot target" diagnostic below) and __reflect_has_deprecated, which had` |
|      - |  1937 | ` * already lost its last caller when isDeprecated() became C.` |
|      - |  1938 | ` *` |
|      - |  1939 | ` * An instance holds a SPEC, not the arguments: [kind, target, member,` |
|      - |  1940 | ` * paramIdx] names something the engine can reopen, and getArguments()` |
|      - |  1941 | ` * evaluates the recorded expressions on every call, because php does too --` |
|      - |  1942 | `` * `#[A(self::X)]` is php code and it runs when it is asked for. That is also`` |
|      - |  1943 | ` * why the prelude needed a public __init(): PHP could not fill a fresh object` |
|      - |  1944 | ` * any other way. C fills it directly, so __init() (and the` |
|      - |  1945 | ` * __reflect_new_no_ctor that built the empty shell) are gone with it.` |
|      - |  1946 | ` * ---------------------------------------------------------------------------` |
|      - |  1947 | ` */` |
|      - |  1948 | ``#define RA_NAME   "name"      /* php declares this one PUBLIC: `$attr->name` */`` |
|      - |  1949 | `#define RA_SPEC   "__spec"    /* [kind, target, member, paramIdx] */` |
|      - |  1950 | `#define RA_IDX    "__idx"     /* which of that target's attributes this is */` |
|      - |  1951 | `#define RA_TARGET "__target"  /* the Attribute::TARGET_* bit it was found on */` |
|      - |  1952 | `#define RA_REP    "__rep"     /* the target carries more than one of this name */` |
|      - |  1953 |  |
|      - |  1954 | `/* Bound on the value exporter's own recursion. An attribute argument cannot be` |
|      - |  1955 | ` * cyclic, but an OBJECT argument's property graph can be. */` |
|      - |  1956 | `#define REFLECT_EXPORT_MAX_DEPTH 31` |
|      - |  1957 |  |
|      - |  1958 | `/* php's Attribute::TARGET_* names, in bit order (TARGET_CLASS is bit 0). */` |
|      - |  1959 | `static const char *const azReflectTarget[] = {` |
|      - |  1960 | `	"class", "function", "method", "property", "class constant", "parameter", "constant"` |
|      - |  1961 | `};` |
|      - |  1962 |  |
|      - |  1963 | `static void ReflectExportValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth);` |
|      - |  1964 |  |
|      - |  1965 | `/*` |
|      - |  1966 | ` * A string the way php's reflection prints one, in either of php's TWO quotings.` |
|      - |  1967 | ` *` |
|      - |  1968 | ` * A USERLAND default prints single-quoted, with the NON-PRINTABLE bytes escaped` |
|      - |  1969 | ` * (php's smart_str_append_escaped) and the quote itself NOT escaped — php's own` |
|      - |  1970 | ` * output for "q'q" is 'q'q'. An INTERNAL one prints DOUBLE-quoted and escapes the` |
|      - |  1971 | `` * quote: php answers `string $enclosure = "\""` for str_getcsv where PHL used to`` |
|      - |  1972 | `` * answer `'"'`. Every internal function with a string default was affected; the`` |
|      - |  1973 | `` * pair was found on Closure::bindTo's `= "static"` while making that class native.`` |
|      - |  1974 | ` */` |
|    392 |  1975 | `static void ReflectExportStrQ(SyBlob *pOut, const char *zIn, sxu32 nIn, char cQuote)` |
|      4 |  1976 | `{` |
|      - |  1977 | `	static const char zHexDigit[] = "0123456789ABCDEF";` |
|      - |  1978 | `	sxu32 i;` |
|    396 |  1979 | `	SyBlobAppend(pOut, &cQuote, sizeof(char));` |
|    738 |  1980 | `	for( i = 0 ; i < nIn ; i++ ){` |
|    346 |  1981 | `		unsigned char c = (unsigned char)zIn[i];` |
|    346 |  1982 | `		if( c == (unsigned char)cQuote && cQuote == '"' ){` |
|     22 |  1983 | `			SyBlobAppend(pOut, "\\\"", sizeof("\\\"")-1);` |
|     22 |  1984 | `			continue;` |
|      - |  1985 | `		}` |
|      - |  1986 | `		{` |
|    326 |  1987 | `		const char *zEsc = 0;` |
|    326 |  1988 | `		switch( c ){` |
|     13 |  1989 | `			case 0x09: zEsc = "\\t"; break;` |
|     30 |  1990 | `			case 0x0A: zEsc = "\\n"; break;` |
|     13 |  1991 | `			case 0x0B: zEsc = "\\v"; break;` |
|      5 |  1992 | `			case 0x0C: zEsc = "\\f"; break;` |
|     15 |  1993 | `			case 0x0D: zEsc = "\\r"; break;` |
|      3 |  1994 | `			case 0x1B: zEsc = "\\e"; break;` |
|     24 |  1995 | `			case '\\': zEsc = "\\\\"; break;` |
|    228 |  1996 | `			default:   break;` |
|      - |  1997 | `		}` |
|    326 |  1998 | `		if( zEsc ){` |
|     96 |  1999 | `			SyBlobAppend(pOut, zEsc, sizeof("\\t")-1);` |
|    286 |  2000 | `		}else if( c < 0x20 \|\| c > 0x7E ){` |
|      - |  2001 | `			char zHex[4];` |
|     15 |  2002 | `			zHex[0] = '\\';` |
|     15 |  2003 | `			zHex[1] = 'x';` |
|     15 |  2004 | `			zHex[2] = zHexDigit[(c >> 4) & 0x0F];` |
|     15 |  2005 | `			zHex[3] = zHexDigit[c & 0x0F];` |
|     15 |  2006 | `			SyBlobAppend(pOut, zHex, sizeof(zHex));` |
|      8 |  2007 | `		}else{` |
|    218 |  2008 | `			SyBlobAppend(pOut, (const char *)&c, sizeof(char));` |
|      - |  2009 | `		}` |
|      - |  2010 | `		}` |
|    165 |  2011 | `	}` |
|    396 |  2012 | `	SyBlobAppend(pOut, &cQuote, sizeof(char));` |
|    396 |  2013 | `}` |
|      - |  2014 | `/* php's userland quoting, which is what every existing caller means. */` |
|    186 |  2015 | `static void ReflectExportStr(SyBlob *pOut, const char *zIn, sxu32 nIn)` |
|      1 |  2016 | `{` |
|    187 |  2017 | `	ReflectExportStrQ(pOut, zIn, nIn, '\'');` |
|    187 |  2018 | `}` |
|      - |  2019 | `/*` |
|      - |  2020 | ` * A float the way php's reflection prints one: the plain string cast (which` |
|      - |  2021 | `` * PHL already renders php's way, 1.0E+15 and all), then php's `zero_frac` —`` |
|      - |  2022 | ` * a float that came out without a fraction gets ".0" so 1.0 does not print as` |
|      - |  2023 | ` * 1. INF and NAN render as words and are left alone.` |
|      - |  2024 | ` */` |
|     22 |  2025 | `static void ReflectExportReal(ph7_vm *pVm, SyBlob *pOut, ph7_real rVal)` |
|      1 |  2026 | `{` |
|      - |  2027 | `	ph7_value sTmp;` |
|      - |  2028 | `	const char *zText;` |
|     23 |  2029 | `	int nText, i, bPlain = 1;` |
|     23 |  2030 | `	PH7_MemObjInitFromReal(pVm, &sTmp, rVal);` |
|     23 |  2031 | `	zText = ph7_value_to_string(&sTmp, &nText);` |
|     23 |  2032 | `	if( nText > 0 ){` |
|     23 |  2033 | `		SyBlobAppend(pOut, zText, (sxu32)nText);` |
|     11 |  2034 | `	}` |
|     51 |  2035 | `	for( i = 0 ; i < nText ; i++ ){` |
|     39 |  2036 | `		if( (zText[i] < '0' \|\| zText[i] > '9') && !(i == 0 && zText[i] == '-') ){` |
|     11 |  2037 | `			bPlain = 0;` |
|     11 |  2038 | `			break;` |
|      - |  2039 | `		}` |
|     15 |  2040 | `	}` |
|     23 |  2041 | `	if( bPlain && nText > 0 ){` |
|     13 |  2042 | `		SyBlobAppend(pOut, ".0", sizeof(".0")-1);` |
|      6 |  2043 | `	}` |
|     23 |  2044 | `	PH7_MemObjRelease(&sTmp);` |
|     23 |  2045 | `}` |
|      - |  2046 | `/*` |
|      - |  2047 | ` * An array the way php's reflection prints one: a LIST — every key an integer,` |
|      - |  2048 | ` * in sequence from zero — prints no keys at all, and anything else prints one` |
|      - |  2049 | `` * for EVERY entry. That is why `[1, 'k' => 2]` comes out as`` |
|      - |  2050 | `` * `[0 => 1, 'k' => 2]` while `[[1], [2 => 3]]` keeps its outer keys silent.`` |
|      - |  2051 | ` */` |
|     36 |  2052 | `static void ReflectExportArray(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth)` |
|      1 |  2053 | `{` |
|     37 |  2054 | `	ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|      - |  2055 | `	ph7_hashmap_node *pEntry;` |
|     37 |  2056 | `	sxi64 iExpect = 0;` |
|      - |  2057 | `	sxu32 n;` |
|     37 |  2058 | `	int bList = 1;` |
|     69 |  2059 | `	for( pEntry = pMap->pFirst, n = 0 ; n < pMap->nEntry && pEntry ; n++ ){` |
|     45 |  2060 | `		if( pEntry->iType == HASHMAP_BLOB_NODE \|\| pEntry->xKey.iKey != iExpect ){` |
|     13 |  2061 | `			bList = 0;` |
|     13 |  2062 | `			break;` |
|      - |  2063 | `		}` |
|     33 |  2064 | `		iExpect++;` |
|     33 |  2065 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     17 |  2066 | `	}` |
|     37 |  2067 | `	SyBlobAppend(pOut, "[", sizeof(char));` |
|     83 |  2068 | `	for( pEntry = pMap->pFirst, n = 0 ; n < pMap->nEntry && pEntry ; n++ ){` |
|     47 |  2069 | `		ph7_value *pMember = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|     47 |  2070 | `		if( n > 0 ){` |
|     17 |  2071 | `			SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|      8 |  2072 | `		}` |
|     47 |  2073 | `		if( !bList ){` |
|     21 |  2074 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|     13 |  2075 | `				ReflectExportStr(pOut, (const char *)SyBlobData(&pEntry->xKey.sKey),` |
|      4 |  2076 | `					SyBlobLength(&pEntry->xKey.sKey));` |
|      5 |  2077 | `			}else{` |
|     13 |  2078 | `				SyBlobFormat(pOut, "%qd", pEntry->xKey.iKey);` |
|      - |  2079 | `			}` |
|     21 |  2080 | `			SyBlobAppend(pOut, " => ", sizeof(" => ")-1);` |
|     10 |  2081 | `		}` |
|     47 |  2082 | `		ReflectExportValue(pCtx, pOut, pMember, iDepth + 1);` |
|     47 |  2083 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     24 |  2084 | `	}` |
|     37 |  2085 | `	SyBlobAppend(pOut, "]", sizeof(char));` |
|     37 |  2086 | `}` |
|      - |  2087 | `/*` |
|      - |  2088 | `` * php's TYPE word in a `Constant [ ... ]` head -- for a CLASS constant, a`` |
|      - |  2089 | ` * global one and an extension's listing alike. 0 means "an object", whose word` |
|      - |  2090 | ` * is its own class name and which only the caller can spell.` |
|      - |  2091 | ` */` |
|   1550 |  2092 | `static const char * ReflectExportTypeWord(ph7_value *pVal)` |
|      2 |  2093 | `{` |
|   1552 |  2094 | `	if( pVal == 0 ){` |
|    ! 0 |  2095 | `		return "null";` |
|      - |  2096 | `	}` |
|   1552 |  2097 | `	if( (pVal->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      3 |  2098 | `		return 0;` |
|      - |  2099 | `	}` |
|   1550 |  2100 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|      3 |  2101 | `		return "null";` |
|      - |  2102 | `	}` |
|   1548 |  2103 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      5 |  2104 | `		return "array";` |
|      - |  2105 | `	}` |
|   1544 |  2106 | `	if( pVal->iFlags & MEMOBJ_RES ){` |
|     13 |  2107 | `		return "resource";` |
|      - |  2108 | `	}` |
|   1532 |  2109 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      5 |  2110 | `		return "bool";` |
|      - |  2111 | `	}` |
|   1528 |  2112 | `	if( pVal->iFlags & MEMOBJ_REAL ){` |
|     11 |  2113 | `		return "float";` |
|      - |  2114 | `	}` |
|   1518 |  2115 | `	if( pVal->iFlags & MEMOBJ_INT ){` |
|   1410 |  2116 | `		return "int";` |
|      - |  2117 | `	}` |
|    110 |  2118 | `	if( pVal->iFlags & MEMOBJ_STRING ){` |
|    110 |  2119 | `		return "string";` |
|      - |  2120 | `	}` |
|    ! 0 |  2121 | `	return "null";` |
|    777 |  2122 | `}` |
|      - |  2123 | ``/* The text php prints between a constant's `{ ` and ` }`. NULL and FALSE are`` |
|      - |  2124 | `` * both nothing at all, an array is `Array` and an object `Object`. */`` |
|   1550 |  2125 | `static void ReflectExportConstValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal)` |
|      2 |  2126 | `{` |
|   1552 |  2127 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|      3 |  2128 | `		return;` |
|      - |  2129 | `	}` |
|   1550 |  2130 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      5 |  2131 | `		SyBlobAppend(pOut, "Array", sizeof("Array")-1);` |
|      5 |  2132 | `		return;` |
|      - |  2133 | `	}` |
|   1546 |  2134 | `	if( pVal->iFlags & MEMOBJ_OBJ ){` |
|      3 |  2135 | `		SyBlobAppend(pOut, "Object", sizeof("Object")-1);` |
|      3 |  2136 | `		return;` |
|      - |  2137 | `	}` |
|   1544 |  2138 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      5 |  2139 | `		if( pVal->x.iVal ){` |
|      3 |  2140 | `			SyBlobAppend(pOut, "1", sizeof(char));` |
|      1 |  2141 | `		}` |
|      5 |  2142 | `		return;` |
|      - |  2143 | `	}` |
|      - |  2144 | `	{` |
|      - |  2145 | `		ph7_value sTmp;` |
|      - |  2146 | `		const char *zText;` |
|      - |  2147 | `		int nText;` |
|   1540 |  2148 | `		PH7_MemObjInit(pCtx->pVm, &sTmp);` |
|   1540 |  2149 | `		PH7_MemObjStore(pVal, &sTmp);` |
|   1540 |  2150 | `		zText = ph7_value_to_string(&sTmp, &nText);` |
|   1540 |  2151 | `		if( nText > 0 ){` |
|   1536 |  2152 | `			SyBlobAppend(pOut, zText, (sxu32)nText);` |
|    767 |  2153 | `		}` |
|   1540 |  2154 | `		PH7_MemObjRelease(&sTmp);` |
|      - |  2155 | `	}` |
|    777 |  2156 | `}` |
|      - |  2157 | `/*` |
|      - |  2158 | `` * One value in php's reflection export syntax — the text after `= ` in a`` |
|      - |  2159 | `` * parameter default and inside `Argument #0 [ … ]` in an attribute dump.`` |
|      - |  2160 | ` *` |
|      - |  2161 | ` * The type dispatch is PH7_MemObjDump's, including its REAL-before-INT order:` |
|      - |  2162 | ` * an integer-valued float carries a cached int view too, and php prints 1.0.` |
|      - |  2163 | ` *` |
|      - |  2164 | ` * php renders an OBJECT argument by echoing the source expression it never` |
|      - |  2165 | `` * folded (`new \A(x: 1)`), which needs the AST. PHL evaluates attribute`` |
|      - |  2166 | ` * arguments from byte-code and has only the resulting VALUE, so a non-enum` |
|      - |  2167 | ` * object is rebuilt from its own properties: same shape, not always the same` |
|      - |  2168 | ` * bytes. An enum case — the one object php's own value formatter handles —` |
|      - |  2169 | ` * matches exactly.` |
|      - |  2170 | ` */` |
|    848 |  2171 | `static void ReflectExportValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth)` |
|      3 |  2172 | `{` |
|    851 |  2173 | `	if( pVal == 0 \|\| iDepth > REFLECT_EXPORT_MAX_DEPTH ){` |
|    ! 0 |  2174 | `		SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|    ! 0 |  2175 | `		return;` |
|      - |  2176 | `	}` |
|    851 |  2177 | `	if( (pVal->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      3 |  2178 | `		ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|      3 |  2179 | `		if( pObj->pClass->iFlags & PH7_CLASS_ENUM ){` |
|      3 |  2180 | `			ph7_value *pName = PH7_EnumCaseNameValue(pObj);` |
|      - |  2181 | `			/* php writes the case as the source did, so a namespaced enum comes` |
|      - |  2182 | `			 * out fully qualified (\N\E::One) and a global one bare (E::One).` |
|      - |  2183 | `			 * The separator is the only thing left of that distinction here. */` |
|      3 |  2184 | `			if( SyByteFind(SyStringData(&pObj->pClass->sName),` |
|      4 |  2185 | `				SyStringLength(&pObj->pClass->sName), '\\', 0) == SXRET_OK ){` |
|    ! 0 |  2186 | `				SyBlobAppend(pOut, "\\", sizeof(char));` |
|    ! 0 |  2187 | `			}` |
|      3 |  2188 | `			SyBlobFormat(pOut, "%z::", &pObj->pClass->sName);` |
|      3 |  2189 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|      3 |  2190 | `				SyBlobAppend(pOut, SyBlobData(&pName->sBlob), SyBlobLength(&pName->sBlob));` |
|      1 |  2191 | `			}` |
|      3 |  2192 | `			return;` |
|      - |  2193 | `		}` |
|    ! 0 |  2194 | `		SyBlobFormat(pOut, "new \\%z(", &pObj->pClass->sName);` |
|      - |  2195 | `		{` |
|      - |  2196 | `			SyHashEntry *pEntry;` |
|    ! 0 |  2197 | `			int nWritten = 0;` |
|    ! 0 |  2198 | `			SyHashResetLoopCursor(&pObj->hAttr);` |
|    ! 0 |  2199 | `			while((pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){` |
|    ! 0 |  2200 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - |  2201 | `				ph7_value *pSlot;` |
|    ! 0 |  2202 | `				if( PH7_ATTR_UNPRESENTED(pVmAttr)` |
|    ! 0 |  2203 | `				 \|\| (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) ){` |
|    ! 0 |  2204 | `					continue; /* class-level members are not part of the object */` |
|      - |  2205 | `				}` |
|    ! 0 |  2206 | `				pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|    ! 0 |  2207 | `				if( nWritten++ ){` |
|    ! 0 |  2208 | `					SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|    ! 0 |  2209 | `				}` |
|    ! 0 |  2210 | `				ReflectExportValue(pCtx, pOut, pSlot, iDepth + 1);` |
|    ! 0 |  2211 | `			}` |
|      - |  2212 | `		}` |
|    ! 0 |  2213 | `		SyBlobAppend(pOut, ")", sizeof(char));` |
|    ! 0 |  2214 | `		return;` |
|      - |  2215 | `	}` |
|    849 |  2216 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|     31 |  2217 | `		SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|     31 |  2218 | `		return;` |
|      - |  2219 | `	}` |
|    819 |  2220 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|     37 |  2221 | `		ReflectExportArray(pCtx, pOut, pVal, iDepth);` |
|     37 |  2222 | `		return;` |
|      - |  2223 | `	}` |
|    783 |  2224 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|    119 |  2225 | `		if( pVal->x.iVal != 0 ){` |
|     53 |  2226 | `			SyBlobAppend(pOut, "true", sizeof("true")-1);` |
|     28 |  2227 | `		}else{` |
|     69 |  2228 | `			SyBlobAppend(pOut, "false", sizeof("false")-1);` |
|      - |  2229 | `		}` |
|    119 |  2230 | `		return;` |
|      - |  2231 | `	}` |
|    667 |  2232 | `	if( pVal->iFlags & MEMOBJ_REAL ){` |
|     23 |  2233 | `		ReflectExportReal(pCtx->pVm, pOut, pVal->rVal);` |
|     23 |  2234 | `		return;` |
|      - |  2235 | `	}` |
|    645 |  2236 | `	if( pVal->iFlags & MEMOBJ_INT ){` |
|    467 |  2237 | `		SyBlobFormat(pOut, "%qd", pVal->x.iVal);` |
|    467 |  2238 | `		return;` |
|      - |  2239 | `	}` |
|    179 |  2240 | `	if( pVal->iFlags & MEMOBJ_STRING ){` |
|    179 |  2241 | `		ReflectExportStr(pOut, (const char *)SyBlobData(&pVal->sBlob), SyBlobLength(&pVal->sBlob));` |
|    179 |  2242 | `		return;` |
|      - |  2243 | `	}` |
|      - |  2244 | `	/* A resource, and anything else php has no export syntax for. */` |
|    ! 0 |  2245 | `	SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|    427 |  2246 | `}` |
|      - |  2247 | `/*` |
|      - |  2248 | ` * Build one ReflectionAttribute. pSpec is shared by every attribute of the` |
|      - |  2249 | ` * same target — it says how to reopen it, not which one this is.` |
|      - |  2250 | ` */` |
|    180 |  2251 | `static ph7_class_instance * ReflectAttrNew(ph7_context *pCtx, SyString *pName,` |
|      - |  2252 | `	ph7_value *pSpec, sxu32 nIdx, int iTargetBit, int bRepeated)` |
|      5 |  2253 | `{` |
|    185 |  2254 | `	ph7_vm *pVm = pCtx->pVm;` |
|    185 |  2255 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, "ReflectionAttribute",` |
|      - |  2256 | `		sizeof("ReflectionAttribute")-1, FALSE, 0);` |
|    185 |  2257 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|    185 |  2258 | `	if( pObj == 0 ){` |
|    ! 0 |  2259 | `		return 0;` |
|      - |  2260 | `	}` |
|    185 |  2261 | `	PH7_NativeSetAttrStr(pVm, pObj, RA_NAME, SyStringData(pName), (int)SyStringLength(pName));` |
|    185 |  2262 | `	PH7_NativeSetProp(pVm, pObj, RA_SPEC, sizeof(RA_SPEC)-1, pSpec);` |
|    185 |  2263 | `	PH7_NativeSetAttrInt(pVm, pObj, RA_IDX, (sxi64)nIdx);` |
|    185 |  2264 | `	PH7_NativeSetAttrInt(pVm, pObj, RA_TARGET, iTargetBit);` |
|    185 |  2265 | `	PH7_NativeSetAttrBool(pVm, pObj, RA_REP, bRepeated);` |
|    185 |  2266 | `	return pObj;` |
|     95 |  2267 | `}` |
|      - |  2268 | `/*` |
|      - |  2269 | ` * Unpack the receiver's spec into the four-value vector ReflectAttrArgs takes.` |
|      - |  2270 | ` * Returns 0 when the object is not one this file built.` |
|      - |  2271 | ` */` |
|    110 |  2272 | `static int ReflectAttrSpec(ph7_context *pCtx, ph7_class_instance *pThis, ph7_value **apOut)` |
|      5 |  2273 | `{` |
|    115 |  2274 | `	ph7_value *pSpec = pThis ? PH7_NativeAttr(pThis, RA_SPEC) : 0;` |
|      - |  2275 | `	ph7_hashmap *pMap;` |
|      - |  2276 | `	int i;` |
|    115 |  2277 | `	if( pSpec == 0 \|\| (pSpec->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 |  2278 | `		return 0;` |
|      - |  2279 | `	}` |
|    115 |  2280 | `	pMap = (ph7_hashmap *)pSpec->x.pOther;` |
|    555 |  2281 | `	for( i = 0 ; i < 4 ; i++ ){` |
|      - |  2282 | `		ph7_value sKey;` |
|    445 |  2283 | `		ph7_hashmap_node *pNode = 0;` |
|      - |  2284 | `		sxi32 rc;` |
|    445 |  2285 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sKey, i);` |
|    445 |  2286 | `		rc = PH7_HashmapLookup(pMap, &sKey, &pNode);` |
|    445 |  2287 | `		PH7_MemObjRelease(&sKey);` |
|    445 |  2288 | `		if( rc != SXRET_OK \|\| pNode == 0 ){` |
|    ! 0 |  2289 | `			return 0;` |
|      - |  2290 | `		}` |
|    445 |  2291 | `		apOut[i] = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pNode->nValIdx);` |
|    445 |  2292 | `		if( apOut[i] == 0 ){` |
|    ! 0 |  2293 | `			return 0;` |
|      - |  2294 | `		}` |
|    225 |  2295 | `	}` |
|    115 |  2296 | `	return 1;` |
|     60 |  2297 | `}` |
|      - |  2298 | `/* The receiver's evaluated arguments, or an empty array when the target no` |
|      - |  2299 | `` * longer resolves (what the chunk's `$a === null ? array() : $a` said). */`` |
|    110 |  2300 | `static ph7_value * ReflectAttrOwnArgs(ph7_context *pCtx, ph7_class_instance *pThis)` |
|      5 |  2301 | `{` |
|      - |  2302 | `	ph7_value *apSpec[4];` |
|    115 |  2303 | `	ph7_value *pArgs = 0;` |
|    115 |  2304 | `	if( pThis && ReflectAttrSpec(pCtx, pThis, apSpec) ){` |
|    115 |  2305 | `		pArgs = ReflectAttrArgs(pCtx, apSpec, (sxu32)PH7_NativeAttrInt(pThis, RA_IDX));` |
|     55 |  2306 | `	}` |
|    115 |  2307 | `	return pArgs ? pArgs : ph7_context_new_array(pCtx);` |
|      5 |  2308 | `}` |
|     64 |  2309 | `static int vm_builtin_ReflectionAttribute_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  2310 | `{` |
|     69 |  2311 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     69 |  2312 | `	const char *zName = "";` |
|     69 |  2313 | `	int nName = 0;` |
|     32 |  2314 | `	SXUNUSED(nArg);` |
|     32 |  2315 | `	SXUNUSED(apArg);` |
|     69 |  2316 | `	if( pThis ){` |
|     69 |  2317 | `		PH7_NativeAttrStr(pThis, RA_NAME, &zName, &nName);` |
|     32 |  2318 | `	}` |
|     69 |  2319 | `	ph7_result_string(pCtx, zName, nName);` |
|     69 |  2320 | `	return PH7_OK;` |
|      5 |  2321 | `}` |
|     24 |  2322 | `static int vm_builtin_ReflectionAttribute_getTarget(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2323 | `{` |
|     25 |  2324 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     12 |  2325 | `	SXUNUSED(nArg);` |
|     12 |  2326 | `	SXUNUSED(apArg);` |
|     25 |  2327 | `	ph7_result_int64(pCtx, pThis ? PH7_NativeAttrInt(pThis, RA_TARGET) : 0);` |
|     25 |  2328 | `	return PH7_OK;` |
|      1 |  2329 | `}` |
|     18 |  2330 | `static int vm_builtin_ReflectionAttribute_isRepeated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2331 | `{` |
|     19 |  2332 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      9 |  2333 | `	SXUNUSED(nArg);` |
|      9 |  2334 | `	SXUNUSED(apArg);` |
|     19 |  2335 | `	ph7_result_bool(pCtx, pThis != 0 && PH7_NativeAttrTruthy(pThis, RA_REP));` |
|     19 |  2336 | `	return PH7_OK;` |
|      1 |  2337 | `}` |
|     40 |  2338 | `static int vm_builtin_ReflectionAttribute_getArguments(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  2339 | `{` |
|     43 |  2340 | `	ph7_value *pArgs = ReflectAttrOwnArgs(pCtx, PH7_ContextThis(pCtx));` |
|     20 |  2341 | `	SXUNUSED(nArg);` |
|     20 |  2342 | `	SXUNUSED(apArg);` |
|     43 |  2343 | `	if( pArgs == 0 ){` |
|    ! 0 |  2344 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  2345 | `	}` |
|     43 |  2346 | `	ph7_result_value(pCtx, pArgs);` |
|     43 |  2347 | `	return PH7_OK;` |
|     23 |  2348 | `}` |
|      - |  2349 | `/*` |
|      - |  2350 | ` * newInstance(): the four screens php runs before it constructs anything —` |
|      - |  2351 | ` * the attribute class exists, it is declared #[Attribute], that declaration` |
|      - |  2352 | ` * allows the target this was found on, and it allows repetition if it was` |
|      - |  2353 | ` * repeated. The messages are php's, byte for byte.` |
|      - |  2354 | ` */` |
|     80 |  2355 | `static int vm_builtin_ReflectionAttribute_newInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  2356 | `{` |
|     85 |  2357 | `	ph7_vm *pVm = pCtx->pVm;` |
|     85 |  2358 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     85 |  2359 | `	ph7_value *pNameVal = pThis ? PH7_NativeAttr(pThis, RA_NAME) : 0;` |
|      - |  2360 | `	ph7_class *pClass;` |
|      - |  2361 | `	ph7_attribute *aA;` |
|      - |  2362 | `	ph7_value *pDeclArgs;` |
|     85 |  2363 | `	sxu32 n, nDecl = 0;` |
|     85 |  2364 | `	int bDecl = 0, iTarget, iBit;` |
|     85 |  2365 | `	sxi64 iFlags = 127; /* php's TARGET_ALL: #[Attribute] with no argument */` |
|     40 |  2366 | `	SXUNUSED(nArg);` |
|     40 |  2367 | `	SXUNUSED(apArg);` |
|     85 |  2368 | `	if( pNameVal == 0 ){` |
|    ! 0 |  2369 | `		ph7_result_null(pCtx);` |
|    ! 0 |  2370 | `		return PH7_OK;` |
|      - |  2371 | `	}` |
|     85 |  2372 | `	pClass = ReflectResolveClass(pVm, pNameVal);` |
|     85 |  2373 | `	if( pClass == 0 ){` |
|      - |  2374 | `		SyString sName;` |
|      5 |  2375 | `		SyStringInitFromBuf(&sName, SyBlobData(&pNameVal->sBlob), SyBlobLength(&pNameVal->sBlob));` |
|      5 |  2376 | `		return PH7_VmThrowException(pCtx, "Error", "Attribute class \"%z\" not found", &sName);` |
|      - |  2377 | `	}` |
|      - |  2378 | `	/* Which #[...] on the attribute class is its own #[Attribute] declaration */` |
|     81 |  2379 | `	aA = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);` |
|     81 |  2380 | `	for( n = 0 ; n < SySetUsed(&pClass->aAttrs) ; n++ ){` |
|     72 |  2381 | `		if( SyStringLength(&aA[n].sName) == sizeof("Attribute")-1` |
|     77 |  2382 | `		 && SyStrnicmp(SyStringData(&aA[n].sName), "Attribute", sizeof("Attribute")-1) == 0 ){` |
|     77 |  2383 | `			nDecl = n;` |
|     77 |  2384 | `			bDecl = 1;` |
|     77 |  2385 | `			break;` |
|      - |  2386 | `		}` |
|    ! 0 |  2387 | `	}` |
|     81 |  2388 | `	if( !bDecl ){` |
|      7 |  2389 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      2 |  2390 | `			"Attempting to use non-attribute class \"%z\" as attribute", &pClass->sName);` |
|      - |  2391 | `	}` |
|      - |  2392 | ``	/* Its argument is the target mask: positional, or named `flags:`. */`` |
|      - |  2393 | `	{` |
|      - |  2394 | `		ph7_value *apSpec[4];` |
|     77 |  2395 | `		ph7_value *pKind = ph7_context_new_scalar(pCtx);` |
|     77 |  2396 | `		ph7_value *pMem  = ph7_context_new_scalar(pCtx);` |
|     77 |  2397 | `		ph7_value *pIdx  = ph7_context_new_scalar(pCtx);` |
|     77 |  2398 | `		if( pKind == 0 \|\| pMem == 0 \|\| pIdx == 0 ){` |
|    ! 0 |  2399 | `			return PH7_ContextMemoryError(pCtx);` |
|      - |  2400 | `		}` |
|     77 |  2401 | `		ph7_value_string(pKind, "class", sizeof("class")-1);` |
|     77 |  2402 | `		ph7_value_null(pMem);` |
|     77 |  2403 | `		ph7_value_int(pIdx, 0);` |
|     77 |  2404 | `		apSpec[0] = pKind;` |
|     77 |  2405 | `		apSpec[1] = pNameVal;` |
|     77 |  2406 | `		apSpec[2] = pMem;` |
|     77 |  2407 | `		apSpec[3] = pIdx;` |
|     77 |  2408 | `		pDeclArgs = ReflectAttrArgs(pCtx, apSpec, nDecl);` |
|      - |  2409 | `	}` |
|     77 |  2410 | `	if( pDeclArgs ){` |
|     77 |  2411 | `		ph7_value *pFlags = ph7_array_fetch(pDeclArgs, "0", -1);` |
|     77 |  2412 | `		if( pFlags == 0 ){` |
|     20 |  2413 | `			pFlags = ph7_array_fetch(pDeclArgs, "flags", -1);` |
|      9 |  2414 | `		}` |
|     77 |  2415 | `		if( pFlags ){` |
|     59 |  2416 | `			iFlags = ph7_value_to_int64(pFlags);` |
|     27 |  2417 | `		}` |
|     36 |  2418 | `	}` |
|     77 |  2419 | `	iTarget = (int)PH7_NativeAttrInt(pThis, RA_TARGET);` |
|      - |  2420 | `	/* php's INTERNAL attributes carry a validator beside their mask, and the` |
|      - |  2421 | `	 * engine runs BOTH where the declaration compiles (GenStateCheckAttrPlacement)` |
|      - |  2422 | ``	 * -- so a misplaced `#[\Deprecated]` or `#[\Override]` never reaches this`` |
|      - |  2423 | `	 * far. What is left here is the USERLAND rule, which php checks only when` |
|      - |  2424 | `	 * someone asks: the mask below and the repetition test after it. */` |
|     77 |  2425 | `	if( (iFlags & iTarget) == 0 ){` |
|      - |  2426 | `		SyBlob sAllowed;` |
|      - |  2427 | `		sxi32 rc;` |
|     10 |  2428 | `		SyBlobInit(&sAllowed, &pVm->sAllocator);` |
|     66 |  2429 | `		for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azReflectTarget) ; iBit++ ){` |
|     58 |  2430 | `			if( (iFlags & ((sxi64)1 << iBit)) == 0 ){` |
|     50 |  2431 | `				continue;` |
|      - |  2432 | `			}` |
|     10 |  2433 | `			if( SyBlobLength(&sAllowed) > 0 ){` |
|    ! 0 |  2434 | `				SyBlobAppend(&sAllowed, ", ", sizeof(", ")-1);` |
|    ! 0 |  2435 | `			}` |
|     14 |  2436 | `			SyBlobAppend(&sAllowed, azReflectTarget[iBit],` |
|      8 |  2437 | `				(sxu32)SyStrlen(azReflectTarget[iBit]));` |
|      6 |  2438 | `		}` |
|     10 |  2439 | `		SyBlobAppend(&sAllowed, "", sizeof(char)); /* NUL for the %s below */` |
|     22 |  2440 | `		for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azReflectTarget) ; iBit++ ){` |
|     22 |  2441 | `			if( iTarget == (1 << iBit) ){` |
|     10 |  2442 | `				break;` |
|      - |  2443 | `			}` |
|      8 |  2444 | `		}` |
|     14 |  2445 | `		rc = PH7_VmThrowException(pCtx, "Error",` |
|      4 |  2446 | `			"Attribute \"%z\" cannot target %s (allowed targets: %s)", &pClass->sName,` |
|      4 |  2447 | `			iBit < (int)SX_ARRAYSIZE(azReflectTarget) ? azReflectTarget[iBit] : "",` |
|      4 |  2448 | `			SyBlobData(&sAllowed));` |
|     10 |  2449 | `		SyBlobRelease(&sAllowed);` |
|     10 |  2450 | `		return rc;` |
|      - |  2451 | `	}` |
|     69 |  2452 | `	if( PH7_NativeAttrTruthy(pThis, RA_REP) && (iFlags & 128) == 0 ){` |
|     11 |  2453 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      3 |  2454 | `			"Attribute \"%z\" must not be repeated", &pClass->sName);` |
|      - |  2455 | `	}` |
|     62 |  2456 | `	return ReflectAttrInstantiate(pCtx, pNameVal, ReflectAttrOwnArgs(pCtx, pThis));` |
|     45 |  2457 | `}` |
|      - |  2458 | `/*` |
|      - |  2459 | `` * php's export text. A bare `Attribute [ Name ]` when there are no arguments;`` |
|      - |  2460 | ` * otherwise the same head followed by the argument block, each argument` |
|      - |  2461 | `` * rendered in php's value-export syntax and named ones as `name = value`.`` |
|      - |  2462 | ` */` |
|     12 |  2463 | `static int vm_builtin_ReflectionAttribute_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2464 | `{` |
|     13 |  2465 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 |  2466 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     13 |  2467 | `	ph7_value *pArgs = ReflectAttrOwnArgs(pCtx, pThis);` |
|     13 |  2468 | `	const char *zName = "";` |
|     13 |  2469 | `	int nName = 0;` |
|      - |  2470 | `	SyBlob sOut;` |
|     13 |  2471 | `	sxu32 nCount = 0;` |
|      6 |  2472 | `	SXUNUSED(nArg);` |
|      6 |  2473 | `	SXUNUSED(apArg);` |
|     13 |  2474 | `	if( pThis ){` |
|     13 |  2475 | `		PH7_NativeAttrStr(pThis, RA_NAME, &zName, &nName);` |
|      6 |  2476 | `	}` |
|     13 |  2477 | `	if( pArgs && (pArgs->iFlags & MEMOBJ_HASHMAP) ){` |
|     13 |  2478 | `		nCount = ((ph7_hashmap *)pArgs->x.pOther)->nEntry;` |
|      6 |  2479 | `	}` |
|     13 |  2480 | `	SyBlobInit(&sOut, &pVm->sAllocator);` |
|     13 |  2481 | `	SyBlobAppend(&sOut, "Attribute [ ", sizeof("Attribute [ ")-1);` |
|     13 |  2482 | `	if( nName > 0 ){` |
|     13 |  2483 | `		SyBlobAppend(&sOut, zName, (sxu32)nName);` |
|      6 |  2484 | `	}` |
|     13 |  2485 | `	SyBlobAppend(&sOut, " ]", sizeof(" ]")-1);` |
|     13 |  2486 | `	if( nCount < 1 ){` |
|      3 |  2487 | `		SyBlobAppend(&sOut, "\n", sizeof(char));` |
|      2 |  2488 | `	}else{` |
|     11 |  2489 | `		ph7_hashmap *pMap = (ph7_hashmap *)pArgs->x.pOther;` |
|     11 |  2490 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|      - |  2491 | `		sxu32 n;` |
|     11 |  2492 | `		SyBlobFormat(&sOut, " {\n  - Arguments [%u] {\n", nCount);` |
|     81 |  2493 | `		for( n = 0 ; n < nCount && pEntry ; n++ ){` |
|     71 |  2494 | `			ph7_value *pMember = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, pEntry->nValIdx);` |
|     71 |  2495 | `			SyBlobFormat(&sOut, "    Argument #%u [ ", n);` |
|     71 |  2496 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      7 |  2497 | `				SyBlobAppend(&sOut, SyBlobData(&pEntry->xKey.sKey),` |
|      2 |  2498 | `					SyBlobLength(&pEntry->xKey.sKey));` |
|      5 |  2499 | `				SyBlobAppend(&sOut, " = ", sizeof(" = ")-1);` |
|      2 |  2500 | `			}` |
|     71 |  2501 | `			ReflectExportValue(pCtx, &sOut, pMember, 0);` |
|     71 |  2502 | `			SyBlobAppend(&sOut, " ]\n", sizeof(" ]\n")-1);` |
|     71 |  2503 | `			pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     36 |  2504 | `		}` |
|     11 |  2505 | `		SyBlobAppend(&sOut, "  }\n}\n", sizeof("  }\n}\n")-1);` |
|      - |  2506 | `	}` |
|     13 |  2507 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     13 |  2508 | `	SyBlobRelease(&sOut);` |
|     13 |  2509 | `	return PH7_OK;` |
|      1 |  2510 | `}` |
|      - |  2511 | `/* The body of the two methods php declares PRIVATE and never calls. Neither is` |
|      - |  2512 | `` * reachable from php code: `new ReflectionAttribute` is the engine's own "Call`` |
|      - |  2513 | `` * to private … from global scope" Error, and `clone` is refused by`` |
|      - |  2514 | ` * PH7_CLASS_NOCLONE before any body runs. They exist so the class REPORTS them,` |
|      - |  2515 | ` * which is the only way php's own dump shows them too. */` |
|    ! 0 |  2516 | `static int vm_builtin_ReflectionAttribute_private(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  2517 | `{` |
|    ! 0 |  2518 | `	SXUNUSED(nArg);` |
|    ! 0 |  2519 | `	SXUNUSED(apArg);` |
|    ! 0 |  2520 | `	SXUNUSED(pCtx);` |
|    ! 0 |  2521 | `	return PH7_OK;` |
|    ! 0 |  2522 | `}` |
|      - |  2523 | `/*` |
|      - |  2524 | ` * Declare ReflectionAttribute. Called from PH7_VmInstallReflection() where` |
|      - |  2525 | ` * chunk 7 used to be compiled, so Reflector (chunk 1) already exists.` |
|      - |  2526 | ` *` |
|      - |  2527 | ` * PH7_CLASS_NOCLONE is php's rule for it (php declares __clone private AND` |
|      - |  2528 | ` * refuses the copy), and the class is NOT final — php 8.5 unsealed it.` |
|      - |  2529 | ` */` |
|   6721 |  2530 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionAttribute(ph7_vm *pVm)` |
|      5 |  2531 | `{` |
|      - |  2532 | `	static const PH7_NativeConstDef aConst[] = {` |
|      - |  2533 | `		{ "IS_INSTANCEOF", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|      - |  2534 | `	};` |
|      - |  2535 | `	static const PH7_NativePropDef aProp[] = {` |
|      - |  2536 | `		{ RA_NAME,   PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  2537 | `		/* PHL-only, and PROTECTED so php code cannot reach them: the spec that` |
|      - |  2538 | `		 * reopens the target. php holds the same state on the C struct behind the` |
|      - |  2539 | `		 * object, invisible; PHL has no hidden-slot bit yet (§7.4 (e)), so these` |
|      - |  2540 | `		 * four still show up in a var_dump where php shows only $name. */` |
|      - |  2541 | `		{ RA_SPEC,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0,  0.0 }, 0 },` |
|      - |  2542 | `		{ RA_IDX,    PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0,  0.0 }, 0 },` |
|      - |  2543 | `		{ RA_TARGET, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0,  0.0 }, 0 },` |
|      - |  2544 | `		{ RA_REP,    PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0,  0.0 }, 0 },` |
|      - |  2545 | `	};` |
|      - |  2546 | `	/* php's own listing order, which is what __toString() prints. */` |
|      - |  2547 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|      - |  2548 | `		{ "getName",      PH7_MOD_PUBLIC,  "", "string", vm_builtin_ReflectionAttribute_getName },` |
|      - |  2549 | `		{ "getTarget",    PH7_MOD_PUBLIC,  "", "int",    vm_builtin_ReflectionAttribute_getTarget },` |
|      - |  2550 | `		{ "isRepeated",   PH7_MOD_PUBLIC,  "", "bool",   vm_builtin_ReflectionAttribute_isRepeated },` |
|      - |  2551 | `		{ "getArguments", PH7_MOD_PUBLIC,  "", "array",  vm_builtin_ReflectionAttribute_getArguments },` |
|      - |  2552 | `		{ "newInstance",  PH7_MOD_PUBLIC,  "", "object", vm_builtin_ReflectionAttribute_newInstance },` |
|      - |  2553 | `		{ "__toString",   PH7_MOD_PUBLIC,  "", "string", vm_builtin_ReflectionAttribute_toString },` |
|      - |  2554 | `		{ "__clone",      PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionAttribute_private },` |
|      - |  2555 | `		{ "__construct",  PH7_MOD_PRIVATE, "", 0,      vm_builtin_ReflectionAttribute_private },` |
|      - |  2556 | `	};` |
|      - |  2557 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  2558 | `		{ "ReflectionAttribute", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  2559 | `		  aMethod, SX_ARRAYSIZE(aMethod), aConst, SX_ARRAYSIZE(aConst),` |
|      - |  2560 | `		  aProp, SX_ARRAYSIZE(aProp), 0, 0, 0 },` |
|      - |  2561 | `	};` |
|   6726 |  2562 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  2563 | `}` |
|      - |  2564 | `/*` |
|      - |  2565 | ` * The shared getAttributes() body.` |
|      - |  2566 | ` *` |
|      - |  2567 | ` * Turns the target's #[...] records into ReflectionAttribute objects, applying` |
|      - |  2568 | ` * php's name / IS_INSTANCEOF filter. Argument VALUES stay lazy — what each` |
|      - |  2569 | ` * object carries is the [kind, target, member, paramIdx] spec that reopens` |
|      - |  2570 | ` * them — so a reflector declaring getAttributes() only has to say WHICH target` |
|      - |  2571 | ` * it is.` |
|      - |  2572 | ` */` |
|    184 |  2573 | `static int ReflectBuildAttrs(ph7_context *pCtx, SySet *pAttrs, const char *zKind,` |
|      - |  2574 | `	ph7_value *pTarget, const char *zMember, int nMember,` |
|      - |  2575 | `	int iParamIdx, int iTargetBit, int nArg, ph7_value **apArg)` |
|      5 |  2576 | `{` |
|    189 |  2577 | `	ph7_vm *pVm = pCtx->pVm;` |
|    189 |  2578 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|    189 |  2579 | `	ph7_class *pFilter = 0;` |
|    189 |  2580 | `	const char *zFilter = 0;` |
|    189 |  2581 | `	int nFilter = 0;` |
|      - |  2582 | `	ph7_value *pSpec, *pOut;` |
|      - |  2583 | `	sxu32 n, i;` |
|    189 |  2584 | `	pOut = ph7_context_new_array(pCtx);` |
|    189 |  2585 | `	pSpec = ph7_context_new_array(pCtx);` |
|    189 |  2586 | `	if( pOut == 0 \|\| pSpec == 0 ){` |
|    ! 0 |  2587 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  2588 | `	}` |
|    189 |  2589 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     15 |  2590 | `		zFilter = ph7_value_to_string(apArg[0], &nFilter);` |
|     15 |  2591 | `		if( nFilter < 1 ){` |
|    ! 0 |  2592 | `			zFilter = 0;` |
|    ! 0 |  2593 | `		}` |
|      - |  2594 | `		/* IS_INSTANCEOF: keep a SUBCLASS of the named attribute too. The exact` |
|      - |  2595 | `		 * name still matches on its own, so this only has to answer for the rest. */` |
|     15 |  2596 | `		if( zFilter && nArg > 1 && (ph7_value_to_int(apArg[1]) & 2) ){` |
|      5 |  2597 | `			pFilter = ReflectResolveClass(pVm, apArg[0]);` |
|      2 |  2598 | `		}` |
|      7 |  2599 | `	}` |
|      - |  2600 | `	{` |
|    189 |  2601 | `		ph7_value *pKind = ph7_context_new_scalar(pCtx);` |
|    189 |  2602 | `		ph7_value *pMem  = ph7_context_new_scalar(pCtx);` |
|    189 |  2603 | `		ph7_value *pIdx  = ph7_context_new_scalar(pCtx);` |
|    189 |  2604 | `		if( pKind == 0 \|\| pMem == 0 \|\| pIdx == 0 ){` |
|    ! 0 |  2605 | `			return PH7_ContextMemoryError(pCtx);` |
|      - |  2606 | `		}` |
|    189 |  2607 | `		ph7_value_string(pKind, zKind, -1);` |
|    189 |  2608 | `		if( zMember ){` |
|     86 |  2609 | `			ph7_value_string(pMem, zMember, nMember);` |
|     45 |  2610 | `		}else{` |
|    106 |  2611 | `			ph7_value_null(pMem);` |
|      - |  2612 | `		}` |
|    189 |  2613 | `		ph7_value_int(pIdx, iParamIdx);` |
|    189 |  2614 | `		ph7_array_add_elem(pSpec, 0, pKind);` |
|      - |  2615 | `		/* The target rides as a VALUE, not a name: a closure's attributes are` |
|      - |  2616 | `		 * reopened through the Closure object itself. */` |
|    189 |  2617 | `		ph7_array_add_elem(pSpec, 0, pTarget);` |
|    189 |  2618 | `		ph7_array_add_elem(pSpec, 0, pMem);` |
|    189 |  2619 | `		ph7_array_add_elem(pSpec, 0, pIdx);` |
|      - |  2620 | `	}` |
|    395 |  2621 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|    211 |  2622 | `		int bRepeated = 0;` |
|    211 |  2623 | `		if( zFilter ){` |
|     89 |  2624 | `			int bKeep = ((int)SyStringLength(&aA[n].sName) == nFilter` |
|     50 |  2625 | `				&& SyStrnicmp(SyStringData(&aA[n].sName), zFilter, (sxu32)nFilter) == 0);` |
|     51 |  2626 | `			if( !bKeep && pFilter ){` |
|     16 |  2627 | `				ph7_class *pCand = PH7_VmExtractClass(pVm, SyStringData(&aA[n].sName),` |
|     10 |  2628 | `					SyStringLength(&aA[n].sName), FALSE, 0);` |
|     11 |  2629 | `				if( pCand == 0 ){` |
|    ! 0 |  2630 | `					pCand = PH7_VmTriggerAutoload(pVm, SyStringData(&aA[n].sName),` |
|    ! 0 |  2631 | `						SyStringLength(&aA[n].sName), FALSE);` |
|    ! 0 |  2632 | `				}` |
|     11 |  2633 | `				bKeep = pCand != 0 && pCand != pFilter && PH7_VmInstanceOf(pCand, pFilter);` |
|      5 |  2634 | `			}` |
|     51 |  2635 | `			if( !bKeep ){` |
|     27 |  2636 | `				continue;` |
|      - |  2637 | `			}` |
|     12 |  2638 | `		}` |
|      - |  2639 | `		/* isRepeated() asks about the TARGET, not about the filtered result:` |
|      - |  2640 | `		 * two #[A] make both of them repeated even when only one is asked for. */` |
|    399 |  2641 | `		for( i = 0 ; i < SySetUsed(pAttrs) ; i++ ){` |
|    254 |  2642 | `			if( i != n && SyStringLength(&aA[i].sName) == SyStringLength(&aA[n].sName)` |
|     75 |  2643 | `			 && SyStrnicmp(SyStringData(&aA[i].sName), SyStringData(&aA[n].sName),` |
|     66 |  2644 | `				SyStringLength(&aA[n].sName)) == 0 ){` |
|     42 |  2645 | `				bRepeated = 1;` |
|     42 |  2646 | `				break;` |
|      - |  2647 | `			}` |
|    112 |  2648 | `		}` |
|    275 |  2649 | `		ReflectTypeListAdd(pCtx, pOut,` |
|    180 |  2650 | `			ReflectAttrNew(pCtx, &aA[n].sName, pSpec, n, iTargetBit, bRepeated));` |
|     95 |  2651 | `	}` |
|    189 |  2652 | `	ph7_result_value(pCtx, pOut);` |
|    189 |  2653 | `	return PH7_OK;` |
|     97 |  2654 | `}` |
|      - |  2655 | `/*` |
|      - |  2656 | ` * The three "no runtime line tracking" methods. php answers a file/line/trace` |
|      - |  2657 | ` * from the executing frame; PHL has no per-instruction line record (the same` |
|      - |  2658 | ` * gap debug_backtrace() has), so it says so loudly rather than inventing one.` |
|      - |  2659 | ` */` |
|    ! 0 |  2660 | `static int ReflectUnsupported(ph7_context *pCtx, const char *zWho)` |
|    ! 0 |  2661 | `{` |
|    ! 0 |  2662 | `	return PH7_VmThrowException(pCtx, "Error",` |
|    ! 0 |  2663 | `		"%s is not supported by PHL (no runtime line tracking)", zWho);` |
|    ! 0 |  2664 | `}` |
|      - |  2665 | `#define REFLECT_UNSUPPORTED(NAME,TEXT) \` |
|      - |  2666 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  2667 | `	{ \` |
|      - |  2668 | `		SXUNUSED(nArg); \` |
|      - |  2669 | `		SXUNUSED(apArg); \` |
|      - |  2670 | `		return ReflectUnsupported(pCtx, TEXT); \` |
|      - |  2671 | `	}` |
|    ! 0 |  2672 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_execLine,"ReflectionGenerator::getExecutingLine()")` |
|    ! 0 |  2673 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_execFile,"ReflectionGenerator::getExecutingFile()")` |
|    ! 0 |  2674 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_trace,"ReflectionGenerator::getTrace()")` |
|    ! 0 |  2675 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_execLine,"ReflectionFiber::getExecutingLine()")` |
|    ! 0 |  2676 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_execFile,"ReflectionFiber::getExecutingFile()")` |
|    ! 0 |  2677 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_trace,"ReflectionFiber::getTrace()")` |
|      - |  2678 |  |
|      - |  2679 | `/* ReflectionGenerator::__construct(Generator $generator) */` |
|     18 |  2680 | `static int vm_builtin_ReflectionGenerator_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2681 | `{` |
|     19 |  2682 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     19 |  2683 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 |  2684 | `		return PH7_OK;` |
|      - |  2685 | `	}` |
|     19 |  2686 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RG_GEN, (ph7_class_instance *)apArg[0]->x.pOther);` |
|     19 |  2687 | `	return PH7_OK;` |
|     10 |  2688 | `}` |
|      - |  2689 | `/* The coroutine context of the wrapped generator, or NULL. */` |
|     36 |  2690 | `static ph7_exec_ctx * ReflectGenExec(ph7_context *pCtx)` |
|      1 |  2691 | `{` |
|     37 |  2692 | `	ph7_class_instance *pGenObj = ReflectWrapped(pCtx, RG_GEN);` |
|      - |  2693 | `	ph7_value sVal;` |
|      - |  2694 | `	ph7_generator *pGen;` |
|     37 |  2695 | `	if( pGenObj == 0 ){` |
|    ! 0 |  2696 | `		return 0;` |
|      - |  2697 | `	}` |
|     37 |  2698 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     37 |  2699 | `	sVal.x.pOther = pGenObj;` |
|     37 |  2700 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|     37 |  2701 | `	pGen = ReflectGeneratorCtx(pCtx->pVm, &sVal);` |
|     37 |  2702 | `	return pGen ? pGen->pCtx : 0;` |
|     19 |  2703 | `}` |
|      8 |  2704 | `static int vm_builtin_ReflectionGenerator_isClosed(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2705 | `{` |
|      9 |  2706 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|      4 |  2707 | `	SXUNUSED(nArg);` |
|      4 |  2708 | `	SXUNUSED(apArg);` |
|     17 |  2709 | `	ph7_result_bool(pCtx, pExec != 0` |
|     12 |  2710 | `		&& (pExec->iState == PH7_CTX_STATE_COMPLETED \|\| pExec->iState == PH7_CTX_STATE_CLOSED));` |
|      9 |  2711 | `	return PH7_OK;` |
|      1 |  2712 | `}` |
|      - |  2713 | `/* ReflectionGenerator::getThis(): ?object — the receiver the generator's body` |
|      - |  2714 | ` * runs against. A coroutine frame installs it as a frame VARIABLE (see` |
|      - |  2715 | ` * VmFiberSetupFrame), not as pFrame->pThis, so both are checked. */` |
|     10 |  2716 | `static int vm_builtin_ReflectionGenerator_getThis(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2717 | `{` |
|     11 |  2718 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|      5 |  2719 | `	SXUNUSED(nArg);` |
|      5 |  2720 | `	SXUNUSED(apArg);` |
|     11 |  2721 | `	if( pExec && pExec->pFrame ){` |
|     11 |  2722 | `		SyHashEntry *pVar = SyHashGet(&pExec->pFrame->hVar, "this", sizeof("this")-1);` |
|     11 |  2723 | `		if( pVar ){` |
|     10 |  2724 | `			ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,` |
|      6 |  2725 | `				(sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|      7 |  2726 | `			if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|      7 |  2727 | `				ph7_result_value(pCtx, pSlot);` |
|      7 |  2728 | `				return PH7_OK;` |
|      - |  2729 | `			}` |
|    ! 0 |  2730 | `		}` |
|      5 |  2731 | `		if( pExec->pFrame->pThis ){` |
|    ! 0 |  2732 | `			return ReflectResultBorrowed(pCtx, pExec->pFrame->pThis);` |
|      - |  2733 | `		}` |
|      2 |  2734 | `	}` |
|      5 |  2735 | `	ph7_result_null(pCtx);` |
|      5 |  2736 | `	return PH7_OK;` |
|      6 |  2737 | `}` |
|      - |  2738 | `/* ReflectionGenerator::getFunction(): ReflectionFunctionAbstract — still a` |
|      - |  2739 | ` * prelude class, so it is built through its own constructor. */` |
|     18 |  2740 | `static int vm_builtin_ReflectionGenerator_getFunction(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2741 | `{` |
|     19 |  2742 | `	ph7_vm *pVm = pCtx->pVm;` |
|     19 |  2743 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|     19 |  2744 | `	ph7_vm_func *pFunc = pExec ? pExec->pFunc : 0;` |
|      - |  2745 | `	ph7_class_instance *pOut;` |
|      - |  2746 | `	ph7_value aArg[2];` |
|     19 |  2747 | `	int nCtorArg = 1;` |
|      - |  2748 | `	sxi32 rc;` |
|      9 |  2749 | `	SXUNUSED(nArg);` |
|      9 |  2750 | `	SXUNUSED(apArg);` |
|     19 |  2751 | `	if( pFunc == 0 ){` |
|    ! 0 |  2752 | `		ph7_result_null(pCtx);` |
|    ! 0 |  2753 | `		return PH7_OK;` |
|      - |  2754 | `	}` |
|     19 |  2755 | `	PH7_MemObjInit(pVm, &aArg[0]);` |
|     19 |  2756 | `	PH7_MemObjInit(pVm, &aArg[1]);` |
|     19 |  2757 | `	if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      9 |  2758 | `		ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|      9 |  2759 | `		ph7_value_string(&aArg[0], SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|      9 |  2760 | `		ph7_value_string(&aArg[1], SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|      9 |  2761 | `		nCtorArg = 2;` |
|      5 |  2762 | `	}else{` |
|     11 |  2763 | `		ph7_value_string(&aArg[0], SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|      - |  2764 | `	}` |
|      - |  2765 | `	{` |
|      - |  2766 | `		ph7_value *apCtor[2];` |
|     19 |  2767 | `		apCtor[0] = &aArg[0];` |
|     19 |  2768 | `		apCtor[1] = &aArg[1];` |
|     28 |  2769 | `		pOut = ReflectConstruct(pCtx, nCtorArg == 2 ? "ReflectionMethod" : "ReflectionFunction",` |
|      9 |  2770 | `			nCtorArg, apCtor, &rc);` |
|      - |  2771 | `	}` |
|     19 |  2772 | `	PH7_MemObjRelease(&aArg[0]);` |
|     19 |  2773 | `	PH7_MemObjRelease(&aArg[1]);` |
|     19 |  2774 | `	if( pOut == 0 ){` |
|    ! 0 |  2775 | `		if( rc != PH7_OK ){` |
|    ! 0 |  2776 | `			return rc;` |
|      - |  2777 | `		}` |
|    ! 0 |  2778 | `		ph7_result_null(pCtx);` |
|    ! 0 |  2779 | `		return PH7_OK;` |
|      - |  2780 | `	}` |
|     19 |  2781 | `	return ReflectResultObject(pCtx, pOut);` |
|     10 |  2782 | `}` |
|      - |  2783 | `` /* ReflectionGenerator::getExecutingGenerator(): Generator — follow `yield from` `` |
|      - |  2784 | ` * delegation to the innermost one that is actually running. */` |
|      6 |  2785 | `static int vm_builtin_ReflectionGenerator_getExecuting(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2786 | `{` |
|      7 |  2787 | `	ph7_vm *pVm = pCtx->pVm;` |
|      7 |  2788 | `	ph7_class_instance *pGenObj = ReflectWrapped(pCtx, RG_GEN);` |
|      - |  2789 | `	ph7_value sVal, *pCur;` |
|      - |  2790 | `	ph7_generator *pGen;` |
|      7 |  2791 | `	int iDepth = 0;` |
|      3 |  2792 | `	SXUNUSED(nArg);` |
|      3 |  2793 | `	SXUNUSED(apArg);` |
|      7 |  2794 | `	if( pGenObj == 0 ){` |
|    ! 0 |  2795 | `		ph7_result_null(pCtx);` |
|    ! 0 |  2796 | `		return PH7_OK;` |
|      - |  2797 | `	}` |
|      7 |  2798 | `	PH7_MemObjInit(pVm, &sVal);` |
|      7 |  2799 | `	sVal.x.pOther = pGenObj;` |
|      7 |  2800 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|      7 |  2801 | `	pCur = &sVal;` |
|      7 |  2802 | `	pGen = ReflectGeneratorCtx(pVm, &sVal);` |
|     12 |  2803 | `	while( pGen && pGen->pCtx && pGen->pCtx->iDelegateState == 3` |
|     10 |  2804 | `	 && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|      3 |  2805 | `		ph7_generator *pInner = ReflectGeneratorCtx(pVm, &pGen->pCtx->sDelegate);` |
|      3 |  2806 | `		if( pInner == 0 ){` |
|    ! 0 |  2807 | `			break;` |
|      - |  2808 | `		}` |
|      3 |  2809 | `		pCur = &pGen->pCtx->sDelegate;` |
|      3 |  2810 | `		pGen = pInner;` |
|      3 |  2811 | `		iDepth++;` |
|      1 |  2812 | `	}` |
|      7 |  2813 | `	return ReflectResultBorrowed(pCtx, (ph7_class_instance *)pCur->x.pOther);` |
|      4 |  2814 | `}` |
|      - |  2815 | `/* ReflectionFiber::__construct(Fiber $fiber) / getFiber() / getCallable() */` |
|      4 |  2816 | `static int vm_builtin_ReflectionFiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2817 | `{` |
|      5 |  2818 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 |  2819 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 |  2820 | `		return PH7_OK;` |
|      - |  2821 | `	}` |
|      5 |  2822 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RF_FIBER, (ph7_class_instance *)apArg[0]->x.pOther);` |
|      5 |  2823 | `	return PH7_OK;` |
|      3 |  2824 | `}` |
|      4 |  2825 | `static int vm_builtin_ReflectionFiber_getFiber(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2826 | `{` |
|      2 |  2827 | `	SXUNUSED(nArg);` |
|      2 |  2828 | `	SXUNUSED(apArg);` |
|      5 |  2829 | `	return ReflectResultBorrowed(pCtx, ReflectWrapped(pCtx, RF_FIBER));` |
|      1 |  2830 | `}` |
|      4 |  2831 | `static int vm_builtin_ReflectionFiber_getCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2832 | `{` |
|      5 |  2833 | `	ph7_class_instance *pFiber = ReflectWrapped(pCtx, RF_FIBER);` |
|      5 |  2834 | `	ph7_value *pVal = pFiber ? PH7_NativeAttr(pFiber, "__callable") : 0;` |
|      2 |  2835 | `	SXUNUSED(nArg);` |
|      2 |  2836 | `	SXUNUSED(apArg);` |
|      5 |  2837 | `	if( pVal ){` |
|      5 |  2838 | `		ph7_result_value(pCtx, pVal);` |
|      2 |  2839 | `	}` |
|      5 |  2840 | `	return PH7_OK;` |
|      1 |  2841 | `}` |
|      - |  2842 | `/* ---- ReflectionConstant ---- */` |
|      - |  2843 | `/* The engine's record for a global constant, or NULL when undefined. */` |
|   1606 |  2844 | `static ph7_constant * ReflectConstEntry(ph7_vm *pVm, const char *zName, int nName)` |
|      4 |  2845 | `{` |
|   1610 |  2846 | `	SyHashEntry *pEntry = nName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zName, (sxu32)nName) : 0;` |
|   1610 |  2847 | `	return pEntry ? (ph7_constant *)pEntry->pUserData : 0;` |
|      4 |  2848 | `}` |
|      - |  2849 | `/* The receiver's constant record, resolved from its public $name. */` |
|    144 |  2850 | `static ph7_constant * ReflectConstOf(ph7_context *pCtx)` |
|      1 |  2851 | `{` |
|    145 |  2852 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    145 |  2853 | `	const char *zName = "";` |
|    145 |  2854 | `	int nName = 0;` |
|    145 |  2855 | `	if( pThis ){` |
|    145 |  2856 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     72 |  2857 | `	}` |
|    145 |  2858 | `	return ReflectConstEntry(pCtx->pVm, zName, nName);` |
|      1 |  2859 | `}` |
|     92 |  2860 | `static int vm_builtin_ReflectionConstant_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2861 | `{` |
|     93 |  2862 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     93 |  2863 | `	int nName = 0;` |
|     93 |  2864 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|     93 |  2865 | `	if( pThis == 0 ){` |
|    ! 0 |  2866 | `		return PH7_OK;` |
|      - |  2867 | `	}` |
|     93 |  2868 | `	if( ReflectConstEntry(pCtx->pVm, zName, nName) == 0 ){` |
|     10 |  2869 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 |  2870 | `			"Constant \"%.*s\" does not exist", nName, zName);` |
|      - |  2871 | `	}` |
|     87 |  2872 | `	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", zName, nName);` |
|     87 |  2873 | `	return PH7_OK;` |
|     47 |  2874 | `}` |
|      4 |  2875 | `static int vm_builtin_ReflectionConstant_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2876 | `{` |
|      5 |  2877 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 |  2878 | `	const char *zName = "";` |
|      5 |  2879 | `	int nName = 0;` |
|      2 |  2880 | `	SXUNUSED(nArg);` |
|      2 |  2881 | `	SXUNUSED(apArg);` |
|      5 |  2882 | `	if( pThis ){` |
|      5 |  2883 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 |  2884 | `	}` |
|      5 |  2885 | `	ph7_result_string(pCtx, zName, nName);` |
|      5 |  2886 | `	return PH7_OK;` |
|      1 |  2887 | `}` |
|      - |  2888 | `/* The namespace split php reports: everything before the last '\', and the rest. */` |
|      8 |  2889 | `static int ReflectConstNsCut(const char *zName, int nName)` |
|      1 |  2890 | `{` |
|      - |  2891 | `	int k;` |
|    101 |  2892 | `	for( k = nName - 1 ; k >= 0 ; k-- ){` |
|     93 |  2893 | `		if( zName[k] == '\\' ){` |
|    ! 0 |  2894 | `			return k;` |
|      - |  2895 | `		}` |
|     47 |  2896 | `	}` |
|      9 |  2897 | `	return -1;` |
|      5 |  2898 | `}` |
|      4 |  2899 | `static int vm_builtin_ReflectionConstant_getNamespaceName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2900 | `{` |
|      5 |  2901 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 |  2902 | `	const char *zName = "";` |
|      5 |  2903 | `	int nName = 0, iCut;` |
|      2 |  2904 | `	SXUNUSED(nArg);` |
|      2 |  2905 | `	SXUNUSED(apArg);` |
|      5 |  2906 | `	if( pThis ){` |
|      5 |  2907 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 |  2908 | `	}` |
|      5 |  2909 | `	iCut = ReflectConstNsCut(zName, nName);` |
|      5 |  2910 | `	ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|      5 |  2911 | `	return PH7_OK;` |
|      1 |  2912 | `}` |
|      4 |  2913 | `static int vm_builtin_ReflectionConstant_getShortName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2914 | `{` |
|      5 |  2915 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 |  2916 | `	const char *zName = "";` |
|      5 |  2917 | `	int nName = 0, iCut;` |
|      2 |  2918 | `	SXUNUSED(nArg);` |
|      2 |  2919 | `	SXUNUSED(apArg);` |
|      5 |  2920 | `	if( pThis ){` |
|      5 |  2921 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 |  2922 | `	}` |
|      5 |  2923 | `	iCut = ReflectConstNsCut(zName, nName);` |
|      5 |  2924 | `	if( iCut < 0 ){` |
|      5 |  2925 | `		ph7_result_string(pCtx, zName, nName);` |
|      3 |  2926 | `	}else{` |
|    ! 0 |  2927 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|      - |  2928 | `	}` |
|      5 |  2929 | `	return PH7_OK;` |
|      1 |  2930 | `}` |
|      8 |  2931 | `static int vm_builtin_ReflectionConstant_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2932 | `{` |
|      9 |  2933 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      - |  2934 | `	ph7_value sValue;` |
|      4 |  2935 | `	SXUNUSED(nArg);` |
|      4 |  2936 | `	SXUNUSED(apArg);` |
|      9 |  2937 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|      9 |  2938 | `	if( pCons && pCons->xExpand ){` |
|      9 |  2939 | `		pCons->xExpand(&sValue, pCons->pUserData);` |
|      4 |  2940 | `	}` |
|      9 |  2941 | `	ph7_result_value(pCtx, &sValue);` |
|      9 |  2942 | `	PH7_MemObjRelease(&sValue);` |
|      9 |  2943 | `	return PH7_OK;` |
|      1 |  2944 | `}` |
|     46 |  2945 | `static int vm_builtin_ReflectionConstant_isDeprecated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2946 | `{` |
|     47 |  2947 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|     23 |  2948 | `	SXUNUSED(nArg);` |
|     23 |  2949 | `	SXUNUSED(apArg);` |
|      - |  2950 | `	/* The same fact the E_DEPRECATED at a NAMING reads, and the same one the` |
|      - |  2951 | `	 * export tags -- reading it here does not raise the notice, which is why` |
|      - |  2952 | `	 * this asks the record rather than expanding the constant. */` |
|     47 |  2953 | `	ph7_result_bool(pCtx, pCons != 0 && pCons->zDeprecated != 0);` |
|     47 |  2954 | `	return PH7_OK;` |
|      1 |  2955 | `}` |
|      8 |  2956 | `static int vm_builtin_ReflectionConstant_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2957 | `{` |
|      9 |  2958 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      4 |  2959 | `	SXUNUSED(nArg);` |
|      4 |  2960 | `	SXUNUSED(apArg);` |
|      9 |  2961 | `	if( pCons && SyStringLength(&pCons->sFile) > 0 ){` |
|      5 |  2962 | `		ph7_result_string(pCtx, SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));` |
|      3 |  2963 | `	}else{` |
|      5 |  2964 | `		ph7_result_bool(pCtx, 0);` |
|      - |  2965 | `	}` |
|      9 |  2966 | `	return PH7_OK;` |
|      1 |  2967 | `}` |
|      - |  2968 | `/* The ReflectionExtension builder every reflector's getExtension() answers with` |
|      - |  2969 | ` * -- defined further down, beside the class partition it reads. */` |
|      - |  2970 | `static int ReflectExtensionOf(ph7_context *pCtx, int iExt);` |
|      - |  2971 | `/* An engine constant belongs to the extension the partition places its name in;` |
|      - |  2972 | ` * a userland define() belongs to none, which php reports as null / false. */` |
|     38 |  2973 | `static int ReflectConstExtId(ph7_context *pCtx)` |
|      1 |  2974 | `{` |
|     39 |  2975 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|     39 |  2976 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     39 |  2977 | `	const char *zName = "";` |
|     39 |  2978 | `	int nName = 0;` |
|     39 |  2979 | `	if( pCons == 0 \|\| pCons->bUserDefined ){` |
|      9 |  2980 | `		return -1;` |
|      - |  2981 | `	}` |
|     31 |  2982 | `	if( pThis ){` |
|     31 |  2983 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     15 |  2984 | `	}` |
|     31 |  2985 | `	return PH7_VmExtOfConstant(zName, nName);` |
|     20 |  2986 | `}` |
|      8 |  2987 | `static int vm_builtin_ReflectionConstant_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2988 | `{` |
|      4 |  2989 | `	SXUNUSED(nArg);` |
|      4 |  2990 | `	SXUNUSED(apArg);` |
|      9 |  2991 | `	return ReflectExtensionOf(pCtx, ReflectConstExtId(pCtx));` |
|      1 |  2992 | `}` |
|     30 |  2993 | `static int vm_builtin_ReflectionConstant_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  2994 | `{` |
|     31 |  2995 | `	int iExt = ReflectConstExtId(pCtx);` |
|     15 |  2996 | `	SXUNUSED(nArg);` |
|     15 |  2997 | `	SXUNUSED(apArg);` |
|     31 |  2998 | `	if( iExt < 0 ){` |
|      7 |  2999 | `		ph7_result_bool(pCtx, 0);` |
|      4 |  3000 | `	}else{` |
|     25 |  3001 | `		ph7_result_string(pCtx, PH7_VmExtensionName(iExt), -1);` |
|      - |  3002 | `	}` |
|     31 |  3003 | `	return PH7_OK;` |
|      1 |  3004 | `}` |
|      - |  3005 | `/* getAttributes() still routes through the prelude builder: chunk 7 owns the` |
|      - |  3006 | ` * ReflectionAttribute shape and the lazy argument evaluation. */` |
|      2 |  3007 | `static int vm_builtin_ReflectionConstant_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3008 | `{` |
|      3 |  3009 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      3 |  3010 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      3 |  3011 | `	const char *zName = "";` |
|      3 |  3012 | `	int nName = 0;` |
|      3 |  3013 | `	if( pThis == 0 \|\| pCons == 0 ){` |
|    ! 0 |  3014 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  3015 | `		return PH7_OK;` |
|      - |  3016 | `	}` |
|      3 |  3017 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      - |  3018 | `	{` |
|      - |  3019 | `		ph7_value sTarget;` |
|      - |  3020 | `		int rc;` |
|      3 |  3021 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      3 |  3022 | `		ph7_value_string(&sTarget, zName, nName);` |
|      - |  3023 | `		/* 64 = Attribute::TARGET_CONSTANT */` |
|      4 |  3024 | `		rc = ReflectBuildAttrs(pCtx, &pCons->aAttrs, "const", &sTarget, 0, 0, 0, 64,` |
|      1 |  3025 | `			nArg, apArg);` |
|      3 |  3026 | `		PH7_MemObjRelease(&sTarget);` |
|      3 |  3027 | `		return rc;` |
|      - |  3028 | `	}` |
|      2 |  3029 | `}` |
|      - |  3030 | `/* php's ZEND_ACC_NO_FILE_CACHE: the two constants whose value belongs to the` |
|      - |  3031 | ` * RUNNING process rather than to the build, so opcache must not bake them in. */` |
|    872 |  3032 | `static int ReflectConstNoFileCache(const SyString *pName)` |
|      1 |  3033 | `{` |
|      - |  3034 | `	static const char * const azNoCache[] = { "PHP_BINARY", "PHP_SAPI" };` |
|      - |  3035 | `	sxu32 n;` |
|   2609 |  3036 | `	for( n = 0 ; n < SX_ARRAYSIZE(azNoCache) ; ++n ){` |
|   1742 |  3037 | `		if( SyStringLength(pName) == SyStrlen(azNoCache[n])` |
|    932 |  3038 | `		 && SyMemcmp(SyStringData(pName), azNoCache[n], SyStringLength(pName)) == 0 ){` |
|      7 |  3039 | `			return 1;` |
|      - |  3040 | `		}` |
|    869 |  3041 | `	}` |
|    867 |  3042 | `	return 0;` |
|    437 |  3043 | `}` |
|      - |  3044 | `/*` |
|      - |  3045 | `` * php's `Constant [ <persistent> int JSON_HEX_TAG ] { 1 }` -- one line, and the`` |
|      - |  3046 | `` * same one an extension's Constants block lists. `<persistent>` is the ENGINE's`` |
|      - |  3047 | ` * own: a define()d constant carries no tag at all, and one php deprecated the` |
|      - |  3048 | `` * SYMBOL of reads `<persistent, deprecated>`. The value is taken from the`` |
|      - |  3049 | ` * constant's expander DIRECTLY, so asking for the export does not raise the` |
|      - |  3050 | ` * E_DEPRECATED that naming it would.` |
|      - |  3051 | ` */` |
|    912 |  3052 | `static void ReflectExportGlobalConstLine(ph7_context *pCtx, SyBlob *pOut, ph7_constant *pCons)` |
|      1 |  3053 | `{` |
|      - |  3054 | `	ph7_value sVal;` |
|      - |  3055 | `	const char *zType;` |
|    913 |  3056 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    913 |  3057 | `	if( pCons->xExpand ){` |
|    913 |  3058 | `		pCons->xExpand(&sVal, pCons->pUserData);` |
|    456 |  3059 | `	}` |
|    913 |  3060 | `	SyBlobAppend(pOut, "Constant [ ", sizeof("Constant [ ")-1);` |
|    913 |  3061 | `	zType = ReflectExportTypeWord(&sVal);` |
|      - |  3062 | ``	/* php's flag words, in its order. `persistent` is a MODULE's registration`` |
|      - |  3063 | `	 * and a define()d constant has none; the three standard streams are the` |
|      - |  3064 | `	 * CLI SAPI's own per-REQUEST constants and have none either, which is what` |
|      - |  3065 | ``	 * the resource test below says. `no_file_cache` is php's opcache flag, and`` |
|      - |  3066 | `	 * it marks exactly the two constants whose value is this process's. */` |
|    913 |  3067 | `	if( !pCons->bUserDefined && (sVal.iFlags & MEMOBJ_RES) == 0 ){` |
|    887 |  3068 | `		if( pCons->zDeprecated ){` |
|     15 |  3069 | `			SyBlobAppend(pOut, "<persistent, deprecated> ", sizeof("<persistent, deprecated> ")-1);` |
|    880 |  3070 | `		}else if( ReflectConstNoFileCache(&pCons->sName) ){` |
|      7 |  3071 | `			SyBlobAppend(pOut, "<persistent, no_file_cache> ",` |
|      - |  3072 | `				sizeof("<persistent, no_file_cache> ")-1);` |
|      4 |  3073 | `		}else{` |
|    867 |  3074 | `			SyBlobAppend(pOut, "<persistent> ", sizeof("<persistent> ")-1);` |
|      - |  3075 | `		}` |
|    443 |  3076 | `	}` |
|    913 |  3077 | `	if( zType ){` |
|    913 |  3078 | `		SyBlobFormat(pOut, "%s ", zType);` |
|    457 |  3079 | `	}else{` |
|    ! 0 |  3080 | `		SyBlobFormat(pOut, "%z ", &((ph7_class_instance *)sVal.x.pOther)->pClass->sName);` |
|      - |  3081 | `	}` |
|    913 |  3082 | `	SyBlobFormat(pOut, "%z ] { ", &pCons->sName);` |
|    913 |  3083 | `	ReflectExportConstValue(pCtx, pOut, &sVal);` |
|    913 |  3084 | `	SyBlobAppend(pOut, " }\n", sizeof(" }\n")-1);` |
|    913 |  3085 | `	PH7_MemObjRelease(&sVal);` |
|    913 |  3086 | `}` |
|     42 |  3087 | `static int vm_builtin_ReflectionConstant_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3088 | `{` |
|     43 |  3089 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      - |  3090 | `	SyBlob sOut;` |
|     21 |  3091 | `	SXUNUSED(nArg);` |
|     21 |  3092 | `	SXUNUSED(apArg);` |
|     43 |  3093 | `	if( pCons == 0 ){` |
|    ! 0 |  3094 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 |  3095 | `		return PH7_OK;` |
|      - |  3096 | `	}` |
|     43 |  3097 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     43 |  3098 | `	ReflectExportGlobalConstLine(pCtx, &sOut, pCons);` |
|     43 |  3099 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     43 |  3100 | `	SyBlobRelease(&sOut);` |
|     43 |  3101 | `	return PH7_OK;` |
|     22 |  3102 | `}` |
|      - |  3103 | `/* ---- ReflectionExtension: one per name this build reports as loaded ---- */` |
|      - |  3104 | `/*` |
|      - |  3105 | ` * php matches the name case-insensitively and then KEEPS its own spelling, so` |
|      - |  3106 | `` * `new ReflectionExtension('DATE')` reports `date` and `spl` reports `SPL`.`` |
|      - |  3107 | `` * A `phl.stub_extensions` name is loaded too and has no canonical spelling of`` |
|      - |  3108 | ` * its own, so it keeps the caller's.` |
|      - |  3109 | ` */` |
|     95 |  3110 | `static int vm_builtin_ReflectionExtension_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  3111 | `{` |
|     99 |  3112 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     99 |  3113 | `	int nName = 0, iExt;` |
|     99 |  3114 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|     99 |  3115 | `	if( pThis == 0 ){` |
|    ! 0 |  3116 | `		return PH7_OK;` |
|      - |  3117 | `	}` |
|     99 |  3118 | `	iExt = PH7_VmExtensionLookup(zName, nName);` |
|     99 |  3119 | `	if( iExt >= 0 ){` |
|     89 |  3120 | `		const char *zCanon = PH7_VmExtensionName(iExt);` |
|     89 |  3121 | `		PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", zCanon, (int)SyStrlen(zCanon));` |
|     89 |  3122 | `		return PH7_OK;` |
|      - |  3123 | `	}` |
|     12 |  3124 | `	if( PH7_VmExtensionIsLoaded(pCtx->pVm, zName, nName) ){` |
|    ! 0 |  3125 | `		PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", zName, nName);` |
|    ! 0 |  3126 | `		return PH7_OK;` |
|      - |  3127 | `	}` |
|     17 |  3128 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      5 |  3129 | `		"Extension \"%.*s\" does not exist", nName, zName);` |
|     50 |  3130 | `}` |
|     22 |  3131 | `static int vm_builtin_ReflectionExtension_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  3132 | `{` |
|     24 |  3133 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     24 |  3134 | `	const char *zName = "";` |
|     24 |  3135 | `	int nName = 0;` |
|     11 |  3136 | `	SXUNUSED(nArg);` |
|     11 |  3137 | `	SXUNUSED(apArg);` |
|     24 |  3138 | `	if( pThis ){` |
|     24 |  3139 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     11 |  3140 | `	}` |
|     24 |  3141 | `	ph7_result_string(pCtx, zName, nName);` |
|     24 |  3142 | `	return PH7_OK;` |
|      2 |  3143 | `}` |
|    ! 0 |  3144 | `static int vm_builtin_ReflectionExtension_getVersion(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3145 | `{` |
|      - |  3146 | `	ph7_value sFn, sRes;` |
|      - |  3147 | `	SyString sStr;` |
|    ! 0 |  3148 | `	SXUNUSED(nArg);` |
|    ! 0 |  3149 | `	SXUNUSED(apArg);` |
|    ! 0 |  3150 | `	PH7_MemObjInit(pCtx->pVm, &sFn);` |
|    ! 0 |  3151 | `	PH7_MemObjInit(pCtx->pVm, &sRes);` |
|    ! 0 |  3152 | `	SyStringInitFromBuf(&sStr, "phpversion", sizeof("phpversion")-1);` |
|    ! 0 |  3153 | `	PH7_MemObjInitFromString(pCtx->pVm, &sFn, &sStr);` |
|    ! 0 |  3154 | `	if( PH7_VmCallUserFunction(pCtx->pVm, &sFn, 0, 0, &sRes) == SXRET_OK ){` |
|    ! 0 |  3155 | `		ph7_result_value(pCtx, &sRes);` |
|    ! 0 |  3156 | `	}` |
|    ! 0 |  3157 | `	PH7_MemObjRelease(&sFn);` |
|    ! 0 |  3158 | `	PH7_MemObjRelease(&sRes);` |
|    ! 0 |  3159 | `	return PH7_OK;` |
|    ! 0 |  3160 | `}` |
|      - |  3161 | `/*` |
|      - |  3162 | ` * The four listings an extension answers about ITSELF -- getFunctions(),` |
|      - |  3163 | ` * getClasses()/getClassNames(), getConstants() and getINIEntries(). All of them` |
|      - |  3164 | ` * are the partition walked in php's own registration order (which is not` |
|      - |  3165 | ` * alphabetical) and filtered against the live VM, so a build without one of the` |
|      - |  3166 | ` * compile-time extensions simply lists nothing for it.` |
|      - |  3167 | ` */` |
|      - |  3168 | `#define REFLECT_EXT_MAP        0   /* name => a fresh reflector over it */` |
|      - |  3169 | `#define REFLECT_EXT_NAMES      1   /* a plain LIST of the names */` |
|      - |  3170 | `#define REFLECT_EXT_VALUES     2   /* name => the value the engine holds */` |
|      - |  3171 | `typedef struct ReflectExtList ReflectExtList;` |
|      - |  3172 | `struct ReflectExtList {` |
|      - |  3173 | `	ph7_context *pCtx;` |
|      - |  3174 | `	ph7_value *pList;   /* the array being built */` |
|      - |  3175 | `	ph7_value *pVal;    /* one scratch value, reused for every entry */` |
|      - |  3176 | `	int iKind;          /* PH7_EXT_KIND_* */` |
|      - |  3177 | `	int iShape;         /* REFLECT_EXT_* */` |
|      - |  3178 | `};` |
|      - |  3179 | ``/* A reflector over one internal name, built with its `name` slot already filled:`` |
|      - |  3180 | ` * the constructor would only re-resolve what this walk already has. */` |
|    286 |  3181 | `static int ReflectExtMakeReflector(ph7_context *pCtx, const char *zRefl,` |
|      - |  3182 | `	const char *zName, int nName, ph7_value *pOut)` |
|      2 |  3183 | `{` |
|    288 |  3184 | `	ph7_vm *pVm = pCtx->pVm;` |
|    288 |  3185 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, zRefl, (sxu32)SyStrlen(zRefl), FALSE, 0);` |
|      - |  3186 | `	ph7_class_instance *pObj;` |
|    288 |  3187 | `	if( pClass == 0 \|\| (pObj = PH7_NewClassInstance(pVm, pClass)) == 0 ){` |
|    ! 0 |  3188 | `		return 0;` |
|      - |  3189 | `	}` |
|    288 |  3190 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", zName, nName);` |
|    288 |  3191 | `	PH7_MemObjRelease(pOut);` |
|    288 |  3192 | `	pOut->x.pOther = pObj;` |
|    288 |  3193 | `	pOut->iFlags = MEMOBJ_OBJ;` |
|    288 |  3194 | `	return 1;` |
|    145 |  3195 | `}` |
|    964 |  3196 | `static int ReflectExtListStep(const char *zName, int nName, void *pData)` |
|      4 |  3197 | `{` |
|    968 |  3198 | `	ReflectExtList *p = (ReflectExtList *)pData;` |
|    968 |  3199 | `	ph7_context *pCtx = p->pCtx;` |
|      - |  3200 | `	SyBlob sKey;` |
|      - |  3201 | `	char *zKey;` |
|    968 |  3202 | `	int i, rc = 0;` |
|    968 |  3203 | `	if( !PH7_VmInternalNameExists(pCtx->pVm, p->iKind, zName, nName) ){` |
|    ! 0 |  3204 | `		return 0;` |
|      - |  3205 | `	}` |
|      - |  3206 | `	/* ph7_array_add_strkey_elem() takes a NUL-terminated key, and the walk has` |
|      - |  3207 | `	 * only a name and a length -- so the key is built rather than borrowed,` |
|      - |  3208 | `	 * which is also where the fold happens. */` |
|    968 |  3209 | `	SyBlobInit(&sKey, &pCtx->pVm->sAllocator);` |
|    968 |  3210 | `	SyBlobAppend(&sKey, (const void *)zName, (sxu32)nName);` |
|    968 |  3211 | `	SyBlobAppend(&sKey, (const void *)"", 1);` |
|    968 |  3212 | `	zKey = (char *)SyBlobData(&sKey);` |
|    968 |  3213 | `	if( p->iKind == PH7_EXT_KIND_FUNC ){` |
|      - |  3214 | `		/* php keys the map with the name the engine STORES, which is folded. */` |
|   5686 |  3215 | `		for( i = 0 ; i < nName ; ++i ){` |
|   5412 |  3216 | `			zKey[i] = (char)SyToLower(zKey[i]);` |
|   2707 |  3217 | `		}` |
|    137 |  3218 | `	}` |
|    968 |  3219 | `	switch( p->iShape ){` |
|     17 |  3220 | `		case REFLECT_EXT_NAMES:` |
|     34 |  3221 | `			ph7_value_reset_string_cursor(p->pVal);` |
|     34 |  3222 | `			ph7_value_string(p->pVal, zName, nName);` |
|     34 |  3223 | `			ph7_array_add_elem(p->pList, 0, p->pVal);` |
|     34 |  3224 | `			SyBlobRelease(&sKey);` |
|     34 |  3225 | `			return 0;` |
|    452 |  3226 | `		case REFLECT_EXT_VALUES:` |
|    652 |  3227 | `			if( p->iKind == PH7_EXT_KIND_INI ){` |
|      - |  3228 | `				SyBlob sVal;` |
|    151 |  3229 | `				SyBlobInit(&sVal, &pCtx->pVm->sAllocator);` |
|    151 |  3230 | `				PH7_VmIniGetStr(pCtx->pVm, zKey, &sVal);` |
|    151 |  3231 | `				ph7_value_reset_string_cursor(p->pVal);` |
|    151 |  3232 | `				if( PH7_VmIniIsUnset(pCtx->pVm, zKey) ){` |
|      - |  3233 | `					/* php shows the RAW value here, so a directive declared with` |
|      - |  3234 | `					 * no value is NULL rather than the empty string ini_get()` |
|      - |  3235 | `					 * makes of it. */` |
|      9 |  3236 | `					PH7_MemObjRelease(p->pVal);` |
|      5 |  3237 | `				}else{` |
|    197 |  3238 | `					ph7_value_string(p->pVal, (const char *)SyBlobData(&sVal),` |
|    141 |  3239 | `						(int)SyBlobLength(&sVal));` |
|      - |  3240 | `				}` |
|    151 |  3241 | `				SyBlobRelease(&sVal);` |
|     60 |  3242 | `			}else{` |
|    504 |  3243 | `				ph7_constant *pCons = ReflectConstEntry(pCtx->pVm, zName, nName);` |
|    504 |  3244 | `				PH7_MemObjRelease(p->pVal);` |
|    504 |  3245 | `				if( pCons && pCons->xExpand ){` |
|      - |  3246 | `					/* Describing the table is not READING an entry: php's` |
|      - |  3247 | `					 * deprecated constants report when a program names one,` |
|      - |  3248 | `					 * and a listing is silent (get_defined_constants()'s` |
|      - |  3249 | `					 * own rule, and the same switch). */` |
|    504 |  3250 | `					pCtx->pVm->bConstEnum++;` |
|    504 |  3251 | `					pCons->xExpand(p->pVal, pCons->pUserData);` |
|    504 |  3252 | `					pCtx->pVm->bConstEnum--;` |
|    139 |  3253 | `				}` |
|      - |  3254 | `			}` |
|    652 |  3255 | `			break;` |
|    143 |  3256 | `		default:` |
|    431 |  3257 | `			if( !ReflectExtMakeReflector(pCtx,` |
|    286 |  3258 | `					p->iKind == PH7_EXT_KIND_CLASS ? "ReflectionClass" : "ReflectionFunction",` |
|    143 |  3259 | `					zName, nName, p->pVal) ){` |
|    ! 0 |  3260 | `				SyBlobRelease(&sKey);` |
|    ! 0 |  3261 | `				return 0;` |
|      - |  3262 | `			}` |
|    286 |  3263 | `			break;` |
|      - |  3264 | `	}` |
|    938 |  3265 | `	ph7_array_add_strkey_elem(p->pList, zKey, p->pVal);` |
|    938 |  3266 | `	SyBlobRelease(&sKey);` |
|    938 |  3267 | `	return rc;` |
|    356 |  3268 | `}` |
|     60 |  3269 | `static int ReflectExtListing(ph7_context *pCtx, int iKind, int iShape)` |
|      4 |  3270 | `{` |
|     64 |  3271 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  3272 | `	ReflectExtList sWalk;` |
|     64 |  3273 | `	const char *zName = "";` |
|     64 |  3274 | `	int nName = 0, iExt;` |
|     64 |  3275 | `	sWalk.pCtx = pCtx;` |
|     64 |  3276 | `	sWalk.iKind = iKind;` |
|     64 |  3277 | `	sWalk.iShape = iShape;` |
|     64 |  3278 | `	sWalk.pList = ph7_context_new_array(pCtx);` |
|     64 |  3279 | `	sWalk.pVal = ph7_context_new_scalar(pCtx);` |
|     64 |  3280 | `	if( sWalk.pList == 0 \|\| sWalk.pVal == 0 ){` |
|    ! 0 |  3281 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  3282 | `	}` |
|     64 |  3283 | `	if( pThis ){` |
|     64 |  3284 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     28 |  3285 | `	}` |
|      - |  3286 | ``	/* A `phl.stub_extensions` name has no id and synthesizes nothing, so its`` |
|      - |  3287 | `	 * every listing is empty -- which is also what php answers for a module` |
|      - |  3288 | `	 * that registers none of that kind. */` |
|     64 |  3289 | `	iExt = PH7_VmExtensionLookup(zName, nName);` |
|     64 |  3290 | `	if( iExt >= 0 ){` |
|     64 |  3291 | `		PH7_VmExtWalk(iExt, iKind, ReflectExtListStep, &sWalk);` |
|     28 |  3292 | `	}` |
|     64 |  3293 | `	ph7_result_value(pCtx, sWalk.pList);` |
|     64 |  3294 | `	return PH7_OK;` |
|     32 |  3295 | `}` |
|      - |  3296 | `#define REFLECT_EXT_LISTING(NAME,KIND,SHAPE) \` |
|      - |  3297 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  3298 | `	{ \` |
|      - |  3299 | `		SXUNUSED(nArg); \` |
|      - |  3300 | `		SXUNUSED(apArg); \` |
|      - |  3301 | `		return ReflectExtListing(pCtx, KIND, SHAPE); \` |
|      - |  3302 | `	}` |
|     14 |  3303 | `REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getFunctions,` |
|      2 |  3304 | `	PH7_EXT_KIND_FUNC,  REFLECT_EXT_MAP)` |
|      7 |  3305 | `REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getClasses,` |
|      1 |  3306 | `	PH7_EXT_KIND_CLASS, REFLECT_EXT_MAP)` |
|     19 |  3307 | `REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getClassNames,` |
|      4 |  3308 | `	PH7_EXT_KIND_CLASS, REFLECT_EXT_NAMES)` |
|     19 |  3309 | `REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getConstants,` |
|      4 |  3310 | `	PH7_EXT_KIND_CONST, REFLECT_EXT_VALUES)` |
|     15 |  3311 | `REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getINIEntries,` |
|      3 |  3312 | `	PH7_EXT_KIND_INI,   REFLECT_EXT_VALUES)` |
|      6 |  3313 | `static int ReflectExtDepStep(const char *zOn, const char *zKind, void *pData)` |
|      1 |  3314 | `{` |
|      7 |  3315 | `	ReflectExtList *p = (ReflectExtList *)pData;` |
|      7 |  3316 | `	ph7_value_reset_string_cursor(p->pVal);` |
|      7 |  3317 | `	ph7_value_string(p->pVal, zKind, -1);` |
|      7 |  3318 | `	ph7_array_add_strkey_elem(p->pList, zOn, p->pVal);` |
|      7 |  3319 | `	return 0;` |
|      1 |  3320 | `}` |
|      8 |  3321 | `static int vm_builtin_ReflectionExtension_getDependencies(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3322 | `{` |
|      9 |  3323 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  3324 | `	ReflectExtList sWalk;` |
|      9 |  3325 | `	const char *zName = "";` |
|      9 |  3326 | `	int nName = 0, iExt;` |
|      4 |  3327 | `	SXUNUSED(nArg);` |
|      4 |  3328 | `	SXUNUSED(apArg);` |
|      9 |  3329 | `	sWalk.pList = ph7_context_new_array(pCtx);` |
|      9 |  3330 | `	sWalk.pVal = ph7_context_new_scalar(pCtx);` |
|      9 |  3331 | `	if( sWalk.pList == 0 \|\| sWalk.pVal == 0 ){` |
|    ! 0 |  3332 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  3333 | `	}` |
|      9 |  3334 | `	if( pThis ){` |
|      9 |  3335 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      4 |  3336 | `	}` |
|      9 |  3337 | `	iExt = PH7_VmExtensionLookup(zName, nName);` |
|      9 |  3338 | `	if( iExt >= 0 ){` |
|      9 |  3339 | `		sWalk.pCtx = pCtx;` |
|      9 |  3340 | `		PH7_VmExtWalkDep(iExt, ReflectExtDepStep, &sWalk);` |
|      4 |  3341 | `	}` |
|      9 |  3342 | `	ph7_result_value(pCtx, sWalk.pList);` |
|      9 |  3343 | `	return PH7_OK;` |
|      5 |  3344 | `}` |
|    ! 0 |  3345 | `static int vm_builtin_ReflectionExtension_isPersistent(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3346 | `{` |
|    ! 0 |  3347 | `	SXUNUSED(nArg);` |
|    ! 0 |  3348 | `	SXUNUSED(apArg);` |
|    ! 0 |  3349 | `	ph7_result_bool(pCtx, 1);` |
|    ! 0 |  3350 | `	return PH7_OK;` |
|    ! 0 |  3351 | `}` |
|    ! 0 |  3352 | `static int vm_builtin_ReflectionExtension_isTemporary(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3353 | `{` |
|    ! 0 |  3354 | `	SXUNUSED(nArg);` |
|    ! 0 |  3355 | `	SXUNUSED(apArg);` |
|    ! 0 |  3356 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 |  3357 | `	return PH7_OK;` |
|    ! 0 |  3358 | `}` |
|    ! 0 |  3359 | `static int vm_builtin_ReflectionExtension_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3360 | `{` |
|    ! 0 |  3361 | `	SXUNUSED(nArg);` |
|    ! 0 |  3362 | `	SXUNUSED(apArg);` |
|    ! 0 |  3363 | `	SXUNUSED(pCtx);` |
|    ! 0 |  3364 | `	return PH7_OK;` |
|    ! 0 |  3365 | `}` |
|      - |  3366 | `/*` |
|      - |  3367 | ` * ---------------------------------------------------------------------------` |
|      - |  3368 | ` * The EXTENSION block -- php's whole module, and the last export this engine` |
|      - |  3369 | `` * did not have (it printed one placeholder line, `Extension [ extension #1`` |
|      - |  3370 | `` * name ]`). It NESTS the function and class blocks, which is why it waited for`` |
|      - |  3371 | ` * them: its bytes cannot match php's until theirs do.` |
|      - |  3372 | ` *` |
|      - |  3373 | ` * php's shape, measured against 8.5.9:` |
|      - |  3374 | ` *` |
|      - |  3375 | ` *   Extension [ <persistent> extension #N name version V ] {` |
|      - |  3376 | ` *   <blank>` |
|      - |  3377 | ` *     - Dependencies { … }        each section only when it has rows,` |
|      - |  3378 | ` *   <blank>                       in this order, and each preceded by a` |
|      - |  3379 | ` *     - INI { … }                 blank line -- an extension with no rows` |
|      - |  3380 | `` *   <blank>                       at all prints `{` and `}` on consecutive`` |
|      - |  3381 | ` *     - Constants [C] { … }       lines instead.` |
|      - |  3382 | ` *   <blank>` |
|      - |  3383 | ` *     - Functions { … }` |
|      - |  3384 | ` *   <blank>` |
|      - |  3385 | ` *     - Classes [K] { … }` |
|      - |  3386 | ` *   }` |
|      - |  3387 | ` *` |
|      - |  3388 | ` * A nested block is the STANDALONE export with four spaces on every non-empty` |
|      - |  3389 | ` * line -- verified byte for byte against php, which is what lets this reuse` |
|      - |  3390 | ` * the two block builders rather than threading an indent through them.` |
|      - |  3391 | ` * ---------------------------------------------------------------------------` |
|      - |  3392 | ` */` |
|      - |  3393 | `static void ReflectExportFuncByName(ph7_context *pCtx, SyBlob *pOut,` |
|      - |  3394 | `	const char *zName, int nName);` |
|      - |  3395 | `static sxi32 ReflectExportClassBlock(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass);` |
|      - |  3396 | `/* Copy pIn into pOut with nPad spaces in front of every non-empty line. */` |
|   1392 |  3397 | `static void ReflectExportPad(SyBlob *pOut, SyBlob *pIn, int nPad)` |
|      2 |  3398 | `{` |
|   1394 |  3399 | `	const char *zIn = (const char *)SyBlobData(pIn);` |
|   1394 |  3400 | `	sxu32 nIn = SyBlobLength(pIn), i = 0;` |
|  33714 |  3401 | `	while( i < nIn ){` |
|  32322 |  3402 | `		sxu32 j = i;` |
| 774371 |  3403 | `		while( j < nIn && zIn[j] != '\n' ){` |
| 742051 |  3404 | `			j++;` |
|      2 |  3405 | `		}` |
|  32322 |  3406 | `		if( j > i ){` |
|      - |  3407 | `			int k;` |
| 121212 |  3408 | `			for( k = 0 ; k < nPad ; k++ ){` |
|  96970 |  3409 | `				SyBlobAppend(pOut, " ", sizeof(char));` |
|  48486 |  3410 | `			}` |
|  24244 |  3411 | `			SyBlobAppend(pOut, &zIn[i], j - i);` |
|  12121 |  3412 | `		}` |
|  32322 |  3413 | `		if( j < nIn ){` |
|  32322 |  3414 | `			SyBlobAppend(pOut, "\n", sizeof(char));` |
|  16160 |  3415 | `		}` |
|  32322 |  3416 | `		i = j + 1;` |
|      2 |  3417 | `	}` |
|   1394 |  3418 | `}` |
|      - |  3419 | `typedef struct ReflectExtDump ReflectExtDump;` |
|      - |  3420 | `struct ReflectExtDump {` |
|      - |  3421 | `	ph7_context *pCtx;` |
|      - |  3422 | `	SyBlob *pOut;     /* the section body, already indented */` |
|      - |  3423 | `	int iKind;` |
|      - |  3424 | `	sxu32 nRow;` |
|      - |  3425 | `};` |
|      - |  3426 | ``/* php's access word for an ini directive: the whole mask is `ALL`, and`` |
|      - |  3427 | ` * anything else is the set bits joined with a comma in php's own order. */` |
|     62 |  3428 | `static void ReflectExtIniAccess(SyBlob *pOut, sxi32 iAccess)` |
|      1 |  3429 | `{` |
|      - |  3430 | `	static const struct { sxi32 iBit; const char *zWord; } aBit[] = {` |
|      - |  3431 | `		{ 1, "USER" }, { 2, "PERDIR" }, { 4, "SYSTEM" }` |
|      - |  3432 | `	};` |
|      - |  3433 | `	sxu32 n;` |
|     63 |  3434 | `	int bFirst = 1;` |
|     63 |  3435 | `	if( (iAccess & 7) == 7 ){` |
|     43 |  3436 | `		SyBlobAppend(pOut, "ALL", sizeof("ALL")-1);` |
|     43 |  3437 | `		return;` |
|      - |  3438 | `	}` |
|     81 |  3439 | `	for( n = 0 ; n < SX_ARRAYSIZE(aBit) ; ++n ){` |
|     61 |  3440 | `		if( iAccess & aBit[n].iBit ){` |
|     33 |  3441 | `			if( !bFirst ){` |
|     13 |  3442 | `				SyBlobAppend(pOut, ",", sizeof(char));` |
|      6 |  3443 | `			}` |
|     33 |  3444 | `			SyBlobAppend(pOut, aBit[n].zWord, SyStrlen(aBit[n].zWord));` |
|     33 |  3445 | `			bFirst = 0;` |
|     16 |  3446 | `		}` |
|     31 |  3447 | `	}` |
|     32 |  3448 | `}` |
|   1454 |  3449 | `static int ReflectExtDumpStep(const char *zName, int nName, void *pData)` |
|      2 |  3450 | `{` |
|   1456 |  3451 | `	ReflectExtDump *p = (ReflectExtDump *)pData;` |
|   1456 |  3452 | `	ph7_context *pCtx = p->pCtx;` |
|   1456 |  3453 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  3454 | `	SyBlob sBlock;` |
|   1456 |  3455 | `	if( !PH7_VmInternalNameExists(pVm, p->iKind, zName, nName) ){` |
|    ! 0 |  3456 | `		return 0;` |
|      - |  3457 | `	}` |
|   1456 |  3458 | `	p->nRow++;` |
|   1456 |  3459 | `	if( p->iKind == PH7_EXT_KIND_INI ){` |
|      - |  3460 | `		SyBlob sVal, sDef;` |
|     63 |  3461 | `		sxi32 iAccess = 0;` |
|     63 |  3462 | `		SyBlobInit(&sVal, &pVm->sAllocator);` |
|     63 |  3463 | `		SyBlobInit(&sDef, &pVm->sAllocator);` |
|     63 |  3464 | `		if( PH7_VmIniDescribe(pVm, zName, (sxu32)nName, &iAccess, &sVal, &sDef) ){` |
|     63 |  3465 | `			SyBlobAppend(p->pOut, "    Entry [ ", sizeof("    Entry [ ")-1);` |
|     63 |  3466 | `			SyBlobAppend(p->pOut, zName, (sxu32)nName);` |
|     63 |  3467 | `			SyBlobAppend(p->pOut, " <", sizeof(" <")-1);` |
|     63 |  3468 | `			ReflectExtIniAccess(p->pOut, iAccess);` |
|     63 |  3469 | `			SyBlobAppend(p->pOut, "> ]\n      Current = '", sizeof("> ]\n      Current = '")-1);` |
|     63 |  3470 | `			SyBlobAppend(p->pOut, SyBlobData(&sVal), SyBlobLength(&sVal));` |
|     63 |  3471 | `			SyBlobAppend(p->pOut, "'\n", sizeof("'\n")-1);` |
|      - |  3472 | `			/* php prints the DEFAULT only for a directive a script moved. */` |
|     62 |  3473 | `			if( SyBlobLength(&sVal) != SyBlobLength(&sDef)` |
|     63 |  3474 | `			 \|\| SyMemcmp(SyBlobData(&sVal), SyBlobData(&sDef), SyBlobLength(&sVal)) != 0 ){` |
|      3 |  3475 | `				SyBlobAppend(p->pOut, "      Default = '", sizeof("      Default = '")-1);` |
|      3 |  3476 | `				SyBlobAppend(p->pOut, SyBlobData(&sDef), SyBlobLength(&sDef));` |
|      3 |  3477 | `				SyBlobAppend(p->pOut, "'\n", sizeof("'\n")-1);` |
|      1 |  3478 | `			}` |
|     63 |  3479 | `			SyBlobAppend(p->pOut, "    }\n", sizeof("    }\n")-1);` |
|     31 |  3480 | `		}` |
|     63 |  3481 | `		SyBlobRelease(&sVal);` |
|     63 |  3482 | `		SyBlobRelease(&sDef);` |
|     63 |  3483 | `		return 0;` |
|      - |  3484 | `	}` |
|   1394 |  3485 | `	SyBlobInit(&sBlock, &pVm->sAllocator);` |
|   1394 |  3486 | `	if( p->iKind == PH7_EXT_KIND_CONST ){` |
|    871 |  3487 | `		ph7_constant *pCons = ReflectConstEntry(pVm, zName, nName);` |
|    871 |  3488 | `		if( pCons ){` |
|    871 |  3489 | `			pVm->bConstEnum++;   /* describing the table is not READING an entry */` |
|    871 |  3490 | `			ReflectExportGlobalConstLine(pCtx, &sBlock, pCons);` |
|    871 |  3491 | `			pVm->bConstEnum--;` |
|    436 |  3492 | `		}` |
|    959 |  3493 | `	}else if( p->iKind == PH7_EXT_KIND_CLASS ){` |
|    253 |  3494 | `		ph7_class *pClass = PH7_VmExtractClass(pVm, zName, (sxu32)nName, FALSE, 0);` |
|      - |  3495 | `		/* php separates one CLASS block from the next with a blank line, and` |
|      - |  3496 | `		 * does NOT do the same for functions or constants. */` |
|    253 |  3497 | `		if( p->nRow > 1 ){` |
|    239 |  3498 | `			SyBlobAppend(p->pOut, "\n", sizeof(char));` |
|    119 |  3499 | `		}` |
|    253 |  3500 | `		if( pClass ){` |
|    253 |  3501 | `			ReflectExportClassBlock(pCtx, &sBlock, pClass);` |
|    126 |  3502 | `		}` |
|    127 |  3503 | `	}else{` |
|    272 |  3504 | `		ReflectExportFuncByName(pCtx, &sBlock, zName, nName);` |
|      - |  3505 | `	}` |
|   1394 |  3506 | `	ReflectExportPad(p->pOut, &sBlock, 4);` |
|   1394 |  3507 | `	SyBlobRelease(&sBlock);` |
|   1394 |  3508 | `	return 0;` |
|    729 |  3509 | `}` |
|      - |  3510 | ``/* One `  - <title>[ [N]] { … }` section, written only when it has rows. */`` |
|    100 |  3511 | `static void ReflectExtSection(SyBlob *pOut, const char *zTitle, SyBlob *pBody,` |
|      - |  3512 | `	sxu32 nRow, int bCount)` |
|      2 |  3513 | `{` |
|    102 |  3514 | `	if( SyBlobLength(pBody) < 1 ){` |
|     56 |  3515 | `		return;` |
|      - |  3516 | `	}` |
|     48 |  3517 | `	SyBlobFormat(pOut, "\n  - %s ", zTitle);` |
|     48 |  3518 | `	if( bCount ){` |
|     25 |  3519 | `		SyBlobFormat(pOut, "[%u] ", nRow);` |
|     12 |  3520 | `	}` |
|     48 |  3521 | `	SyBlobAppend(pOut, "{\n", sizeof("{\n")-1);` |
|     48 |  3522 | `	SyBlobAppend(pOut, SyBlobData(pBody), SyBlobLength(pBody));` |
|     48 |  3523 | `	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|     52 |  3524 | `}` |
|      2 |  3525 | `static int ReflectExtDepLine(const char *zOn, const char *zKind, void *pData)` |
|      1 |  3526 | `{` |
|      3 |  3527 | `	ReflectExtDump *p = (ReflectExtDump *)pData;` |
|      3 |  3528 | `	p->nRow++;` |
|      3 |  3529 | `	SyBlobFormat(p->pOut, "    Dependency [ %s (%s) ]\n", zOn, zKind);` |
|      3 |  3530 | `	return 0;` |
|      1 |  3531 | `}` |
|     80 |  3532 | `static void ReflectExtOneSection(ph7_context *pCtx, SyBlob *pOut, int iExt,` |
|      - |  3533 | `	int iKind, const char *zTitle, int bCount)` |
|      2 |  3534 | `{` |
|      - |  3535 | `	ReflectExtDump sDump;` |
|      - |  3536 | `	SyBlob sBody;` |
|     82 |  3537 | `	SyBlobInit(&sBody, &pCtx->pVm->sAllocator);` |
|     82 |  3538 | `	sDump.pCtx = pCtx;` |
|     82 |  3539 | `	sDump.pOut = &sBody;` |
|     82 |  3540 | `	sDump.iKind = iKind;` |
|     82 |  3541 | `	sDump.nRow = 0;` |
|     82 |  3542 | `	PH7_VmExtWalk(iExt, iKind, ReflectExtDumpStep, &sDump);` |
|     82 |  3543 | `	ReflectExtSection(pOut, zTitle, &sBody, sDump.nRow, bCount);` |
|     82 |  3544 | `	SyBlobRelease(&sBody);` |
|     82 |  3545 | `}` |
|     20 |  3546 | `static int vm_builtin_ReflectionExtension_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  3547 | `{` |
|     22 |  3548 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     22 |  3549 | `	ph7_vm *pVm = pCtx->pVm;` |
|     22 |  3550 | `	const char *zName = "";` |
|     22 |  3551 | `	int nName = 0, iExt;` |
|      - |  3552 | `	SyBlob sOut;` |
|     10 |  3553 | `	SXUNUSED(nArg);` |
|     10 |  3554 | `	SXUNUSED(apArg);` |
|     22 |  3555 | `	if( pThis ){` |
|     22 |  3556 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     10 |  3557 | `	}` |
|     22 |  3558 | `	iExt = PH7_VmExtensionLookup(zName, nName);` |
|     22 |  3559 | `	SyBlobInit(&sOut, &pVm->sAllocator);` |
|      - |  3560 | `` 	/* php's `#N` is the module's REGISTRATION index; a `phl.stub_extensions` `` |
|      - |  3561 | `	 * name has no partition of its own and rides at the end of the table. */` |
|      - |  3562 | `	/* The version is the one getVersion() reports, which is php's answer for a` |
|      - |  3563 | `	 * BUNDLED module: the interpreter's own, not the extension's. */` |
|     22 |  3564 | `	SyBlobFormat(&sOut, "Extension [ <persistent> extension #%d %.*s version %s ] {\n",` |
|     10 |  3565 | `		iExt >= 0 ? iExt : PH7_VmExtensionCount(), nName, zName, PHP_COMPAT_VERSION);` |
|     22 |  3566 | `	if( iExt >= 0 ){` |
|      - |  3567 | `		ReflectExtDump sDep;` |
|      - |  3568 | `		SyBlob sBody;` |
|     22 |  3569 | `		SyBlobInit(&sBody, &pVm->sAllocator);` |
|     22 |  3570 | `		sDep.pCtx = pCtx;` |
|     22 |  3571 | `		sDep.pOut = &sBody;` |
|     22 |  3572 | `		sDep.iKind = -1;` |
|     22 |  3573 | `		sDep.nRow = 0;` |
|     22 |  3574 | `		PH7_VmExtWalkDep(iExt, ReflectExtDepLine, &sDep);` |
|     22 |  3575 | `		ReflectExtSection(&sOut, "Dependencies", &sBody, sDep.nRow, 0);` |
|     22 |  3576 | `		SyBlobRelease(&sBody);` |
|     22 |  3577 | `		ReflectExtOneSection(pCtx, &sOut, iExt, PH7_EXT_KIND_INI,   "INI",       0);` |
|     22 |  3578 | `		ReflectExtOneSection(pCtx, &sOut, iExt, PH7_EXT_KIND_CONST, "Constants", 1);` |
|     22 |  3579 | `		ReflectExtOneSection(pCtx, &sOut, iExt, PH7_EXT_KIND_FUNC,  "Functions", 0);` |
|     22 |  3580 | `		ReflectExtOneSection(pCtx, &sOut, iExt, PH7_EXT_KIND_CLASS, "Classes",   1);` |
|     10 |  3581 | `	}` |
|     22 |  3582 | `	SyBlobAppend(&sOut, "}\n", sizeof("}\n")-1);` |
|     22 |  3583 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     22 |  3584 | `	SyBlobRelease(&sOut);` |
|     22 |  3585 | `	return PH7_OK;` |
|      2 |  3586 | `}` |
|      - |  3587 | `/* ---- ReflectionZendExtension: none exist, so the constructor always refuses ---- */` |
|      6 |  3588 | `static int vm_builtin_ReflectionZendExtension_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  3589 | `{` |
|      8 |  3590 | `	int nName = 0;` |
|      8 |  3591 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|     11 |  3592 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 |  3593 | `		"Zend Extension \"%.*s\" does not exist", nName, zName);` |
|      2 |  3594 | `}` |
|    ! 0 |  3595 | `static int vm_builtin_ReflectionZendExtension_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3596 | `{` |
|    ! 0 |  3597 | `	SXUNUSED(nArg);` |
|    ! 0 |  3598 | `	SXUNUSED(apArg);` |
|    ! 0 |  3599 | `	ph7_result_string(pCtx, "", 0);` |
|    ! 0 |  3600 | `	return PH7_OK;` |
|    ! 0 |  3601 | `}` |
|      - |  3602 | `/* ---- ReflectionReference ---- */` |
|      - |  3603 | `/*` |
|      - |  3604 | ` * ReflectionReference::fromArrayElement(array $array, string\|int $key)` |
|      - |  3605 | ` *` |
|      - |  3606 | ` * Answers a reflector only when the element IS a reference — its slot carries a` |
|      - |  3607 | ` * reference-table record with at least two links. The instance is built without` |
|      - |  3608 | ` * running the (private) constructor, exactly as php's factory does.` |
|      - |  3609 | ` */` |
|     18 |  3610 | `static int vm_builtin_ReflectionReference_fromArrayElement(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3611 | `{` |
|      - |  3612 | `	char zGiven[64];` |
|     19 |  3613 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  3614 | `	ph7_hashmap *pMap;` |
|     19 |  3615 | `	ph7_hashmap_node *pNode = 0;` |
|      - |  3616 | `	ph7_class *pClass;` |
|      - |  3617 | `	ph7_class_instance *pObj;` |
|      - |  3618 | `	char zId[64];` |
|     19 |  3619 | `	if( nArg < 1 ){` |
|    ! 0 |  3620 | `		ph7_result_null(pCtx);` |
|    ! 0 |  3621 | `		return PH7_OK;` |
|      - |  3622 | `	}` |
|     19 |  3623 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - |  3624 | ``		/* The shared ZPP screen leaves a SCALAR against a bare `array` parameter`` |
|      - |  3625 | `		 * unscreened (it only judges array/object/null/resource pairings and` |
|      - |  3626 | `		 * scalars against class-typed ones), so the refusal php words from the` |
|      - |  3627 | `		 * declared type is written here -- as the prelude's own is_array() check` |
|      - |  3628 | `		 * did. Recorded in PLAN §2 with the rest of that gap. */` |
|    ! 0 |  3629 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|      - |  3630 | `			"ReflectionReference::fromArrayElement(): Argument #1 ($array) "` |
|    ! 0 |  3631 | `			"must be of type array, %s given", VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));` |
|      - |  3632 | `	}` |
|     19 |  3633 | `	if( nArg < 2 ){` |
|    ! 0 |  3634 | `		ph7_result_null(pCtx);` |
|    ! 0 |  3635 | `		return PH7_OK;` |
|      - |  3636 | `	}` |
|     19 |  3637 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     19 |  3638 | `	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK \|\| pNode == 0 ){` |
|    ! 0 |  3639 | `		ph7_result_null(pCtx);` |
|    ! 0 |  3640 | `		return PH7_OK;` |
|      - |  3641 | `	}` |
|     19 |  3642 | `	if( PH7_VmSlotRefCount(pVm, pNode->nValIdx) < 2 ){` |
|      7 |  3643 | `		ph7_result_null(pCtx);` |
|      7 |  3644 | `		return PH7_OK;` |
|      - |  3645 | `	}` |
|     13 |  3646 | `	pClass = PH7_VmExtractClass(pVm, "ReflectionReference", sizeof("ReflectionReference")-1, FALSE, 0);` |
|     13 |  3647 | `	pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|     13 |  3648 | `	if( pObj == 0 ){` |
|    ! 0 |  3649 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  3650 | `	}` |
|      - |  3651 | `	/* php's ids are opaque strings; the slot index is the identity PHL has. */` |
|     13 |  3652 | `	SyBufferFormat(zId, sizeof(zId), "phlref%u", (unsigned)pNode->nValIdx);` |
|     13 |  3653 | `	PH7_NativeSetAttrStr(pVm, pObj, RR_ID, zId, (int)SyStrlen(zId));` |
|     13 |  3654 | `	return ReflectResultObject(pCtx, pObj);` |
|     10 |  3655 | `}` |
|     12 |  3656 | `static int vm_builtin_ReflectionReference_getId(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3657 | `{` |
|     13 |  3658 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     13 |  3659 | `	const char *zId = "";` |
|     13 |  3660 | `	int nId = 0;` |
|      6 |  3661 | `	SXUNUSED(nArg);` |
|      6 |  3662 | `	SXUNUSED(apArg);` |
|     13 |  3663 | `	if( pThis ){` |
|     13 |  3664 | `		PH7_NativeAttrStr(pThis, RR_ID, &zId, &nId);` |
|      6 |  3665 | `	}` |
|     13 |  3666 | `	ph7_result_string(pCtx, zId, nId);` |
|     13 |  3667 | `	return PH7_OK;` |
|      1 |  3668 | `}` |
|      - |  3669 | `/* php declares this private and never calls it; reaching it is the diagnostic. */` |
|    ! 0 |  3670 | `static int vm_builtin_ReflectionReference_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3671 | `{` |
|    ! 0 |  3672 | `	SXUNUSED(nArg);` |
|    ! 0 |  3673 | `	SXUNUSED(apArg);` |
|    ! 0 |  3674 | `	SXUNUSED(pCtx);` |
|    ! 0 |  3675 | `	return PH7_OK;` |
|    ! 0 |  3676 | `}` |
|      - |  3677 | `/*` |
|      - |  3678 | ` * Declare the six. Called from PH7_VmInstallReflection() where chunk 5 used to` |
|      - |  3679 | ` * be compiled, so Reflector (chunk 1) already exists.` |
|      - |  3680 | ` */` |
|   6721 |  3681 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionSmall(ph7_vm *pVm)` |
|      5 |  3682 | `{` |
|      - |  3683 | `	static const PH7_NativePropDef aGenProp[] = {` |
|      - |  3684 | `		{ RG_GEN, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  3685 | `	};` |
|      - |  3686 | `	static const PH7_NativeMethodDef aGenMethod[] = {` |
|      - |  3687 | `		{ "__construct",           PH7_MOD_PUBLIC, "Generator $generator", "",` |
|      - |  3688 | `		  vm_builtin_ReflectionGenerator_construct },` |
|      - |  3689 | `		{ "getFunction",           PH7_MOD_PUBLIC, "", "ReflectionFunctionAbstract",` |
|      - |  3690 | `		  vm_builtin_ReflectionGenerator_getFunction },` |
|      - |  3691 | `		{ "getThis",               PH7_MOD_PUBLIC, "", "?object", vm_builtin_ReflectionGenerator_getThis },` |
|      - |  3692 | `		{ "getExecutingGenerator", PH7_MOD_PUBLIC, "", "Generator",` |
|      - |  3693 | `		  vm_builtin_ReflectionGenerator_getExecuting },` |
|      - |  3694 | `		{ "isClosed",              PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionGenerator_isClosed },` |
|      - |  3695 | `		{ "getExecutingLine",      PH7_MOD_PUBLIC, "", "int", vm_builtin_ReflectionGenerator_execLine },` |
|      - |  3696 | `		{ "getExecutingFile",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionGenerator_execFile },` |
|      - |  3697 | `		{ "getTrace",              PH7_MOD_PUBLIC,` |
|      - |  3698 | `		  "int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT", "array",` |
|      - |  3699 | `		  vm_builtin_ReflectionGenerator_trace },` |
|      - |  3700 | `	};` |
|      - |  3701 | `	static const PH7_NativePropDef aFiberProp[] = {` |
|      - |  3702 | `		{ RF_FIBER, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  3703 | `	};` |
|      - |  3704 | `	static const PH7_NativeMethodDef aFiberMethod[] = {` |
|      - |  3705 | `		{ "__construct",      PH7_MOD_PUBLIC, "Fiber $fiber", "", vm_builtin_ReflectionFiber_construct },` |
|      - |  3706 | `		{ "getFiber",         PH7_MOD_PUBLIC, "", "Fiber",    vm_builtin_ReflectionFiber_getFiber },` |
|      - |  3707 | `		{ "getCallable",      PH7_MOD_PUBLIC, "", "callable", vm_builtin_ReflectionFiber_getCallable },` |
|      - |  3708 | `		{ "getExecutingLine", PH7_MOD_PUBLIC, "", "?int",     vm_builtin_ReflectionFiber_execLine },` |
|      - |  3709 | `		{ "getExecutingFile", PH7_MOD_PUBLIC, "", "?string",  vm_builtin_ReflectionFiber_execFile },` |
|      - |  3710 | `		{ "getTrace",         PH7_MOD_PUBLIC, "int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT",` |
|      - |  3711 | `		  "array", vm_builtin_ReflectionFiber_trace },` |
|      - |  3712 | `	};` |
|      - |  3713 | `	static const PH7_NativePropDef aNameProp[] = {` |
|      - |  3714 | `		{ "name", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  3715 | `	};` |
|      - |  3716 | `	static const PH7_NativeMethodDef aConstMethod[] = {` |
|      - |  3717 | `		{ "__construct",       PH7_MOD_PUBLIC, "string $name", "",` |
|      - |  3718 | `		  vm_builtin_ReflectionConstant_construct },` |
|      - |  3719 | `		{ "getName",           PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_getName },` |
|      - |  3720 | `		{ "getNamespaceName",  PH7_MOD_PUBLIC, "", "string",` |
|      - |  3721 | `		  vm_builtin_ReflectionConstant_getNamespaceName },` |
|      - |  3722 | `		{ "getShortName",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_getShortName },` |
|      - |  3723 | `		{ "getValue",          PH7_MOD_PUBLIC, "", "mixed",  vm_builtin_ReflectionConstant_getValue },` |
|      - |  3724 | `		{ "isDeprecated",      PH7_MOD_PUBLIC, "", "bool",   vm_builtin_ReflectionConstant_isDeprecated },` |
|      - |  3725 | `		{ "getFileName",       PH7_MOD_PUBLIC, "", "string\|false",` |
|      - |  3726 | `		  vm_builtin_ReflectionConstant_getFileName },` |
|      - |  3727 | `		{ "getExtension",      PH7_MOD_PUBLIC, "", "?ReflectionExtension",` |
|      - |  3728 | `		  vm_builtin_ReflectionConstant_getExtension },` |
|      - |  3729 | `		{ "getExtensionName",  PH7_MOD_PUBLIC, "", "string\|false",` |
|      - |  3730 | `		  vm_builtin_ReflectionConstant_getExtensionName },` |
|      - |  3731 | `		{ "getAttributes",     PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - |  3732 | `		  vm_builtin_ReflectionConstant_getAttributes },` |
|      - |  3733 | `		{ "__toString",        PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_toString },` |
|      - |  3734 | `	};` |
|      - |  3735 | `	static const PH7_NativeMethodDef aExtMethod[] = {` |
|      - |  3736 | `		{ "__construct",     PH7_MOD_PUBLIC, "string $name", "", vm_builtin_ReflectionExtension_construct },` |
|      - |  3737 | `		{ "getName",         PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionExtension_getName },` |
|      - |  3738 | `		{ "getVersion",      PH7_MOD_PUBLIC, "", "@?string", vm_builtin_ReflectionExtension_getVersion },` |
|      - |  3739 | `		{ "getFunctions",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getFunctions },` |
|      - |  3740 | `		{ "getConstants",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getConstants },` |
|      - |  3741 | `		{ "getINIEntries",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getINIEntries },` |
|      - |  3742 | `		{ "getClasses",      PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getClasses },` |
|      - |  3743 | `		{ "getClassNames",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getClassNames },` |
|      - |  3744 | `		{ "getDependencies", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getDependencies },` |
|      - |  3745 | `		{ "info",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_ReflectionExtension_info },` |
|      - |  3746 | `		{ "isPersistent",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionExtension_isPersistent },` |
|      - |  3747 | `		{ "isTemporary",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionExtension_isTemporary },` |
|      - |  3748 | `		{ "__toString",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionExtension_toString },` |
|      - |  3749 | `	};` |
|      - |  3750 | `	static const PH7_NativeMethodDef aZendMethod[] = {` |
|      - |  3751 | `		{ "__construct", PH7_MOD_PUBLIC, "string $name", "",` |
|      - |  3752 | `		  vm_builtin_ReflectionZendExtension_construct },` |
|      - |  3753 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionExtension_getName },` |
|      - |  3754 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionZendExtension_toString },` |
|      - |  3755 | `	};` |
|      - |  3756 | `	static const PH7_NativePropDef aRefProp[] = {` |
|      - |  3757 | `		{ RR_ID, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - |  3758 | `	};` |
|      - |  3759 | `	static const PH7_NativeMethodDef aRefMethod[] = {` |
|      - |  3760 | ``		/* php declares the constructor PRIVATE, so `new ReflectionReference` is`` |
|      - |  3761 | `		 * the engine's own "Call to private ... from global scope" Error rather` |
|      - |  3762 | `		 * than a throw the prelude had to write by hand. */` |
|      - |  3763 | `		{ "__construct",      PH7_MOD_PRIVATE, "", "", vm_builtin_ReflectionReference_construct },` |
|      - |  3764 | `		{ "fromArrayElement", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array, string\|int $key",` |
|      - |  3765 | `		  "?ReflectionReference", vm_builtin_ReflectionReference_fromArrayElement },` |
|      - |  3766 | `		{ "getId",            PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionReference_getId },` |
|      - |  3767 | `	};` |
|      - |  3768 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  3769 | `		{ "ReflectionGenerator", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3770 | `		  aGenMethod, SX_ARRAYSIZE(aGenMethod), 0, 0, aGenProp, SX_ARRAYSIZE(aGenProp), 0, 0, 0 },` |
|      - |  3771 | `		{ "ReflectionFiber", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3772 | `		  aFiberMethod, SX_ARRAYSIZE(aFiberMethod), 0, 0, aFiberProp, SX_ARRAYSIZE(aFiberProp), 0, 0, 0 },` |
|      - |  3773 | `		{ "ReflectionConstant", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3774 | `		  aConstMethod, SX_ARRAYSIZE(aConstMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|      - |  3775 | `		{ "ReflectionExtension", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3776 | `		  aExtMethod, SX_ARRAYSIZE(aExtMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|      - |  3777 | `		{ "ReflectionZendExtension", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3778 | `		  aZendMethod, SX_ARRAYSIZE(aZendMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|      - |  3779 | `		{ "ReflectionReference", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  3780 | `		  aRefMethod, SX_ARRAYSIZE(aRefMethod), 0, 0, aRefProp, SX_ARRAYSIZE(aRefProp), 0, 0, 0 },` |
|      - |  3781 | `	};` |
|   6726 |  3782 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  3783 | `}` |
|      - |  3784 | `/*` |
|      - |  3785 | ` * ---------------------------------------------------------------------------` |
|      - |  3786 | ` * Reflector, Reflection, ReflectionException, ReflectionClass, ReflectionObject.` |
|      - |  3787 | ` *` |
|      - |  3788 | ` * Chunk 1 — the core of the whole API. Its ~350 lines of PHP funnelled every` |
|      - |  3789 | ` * accessor through __phl_rcinfo(), a memoized descriptor ARRAY built by a C` |
|      - |  3790 | ` * thunk: sixty methods that each rebuilt or re-read a marshalled copy of state` |
|      - |  3791 | ` * the engine was already holding. A native method reads ph7_class directly, so` |
|      - |  3792 | ` * the descriptor is gone from this path entirely and the memo it needed with it.` |
|      - |  3793 | ` *` |
|      - |  3794 | ` * What stays behind is the prelude that has not moved yet, and these classes` |
|      - |  3795 | ` * still reach it by name where php's own object graph does: getMethod() builds` |
|      - |  3796 | ` * a ReflectionMethod, getProperty() a ReflectionProperty, getAttributes() calls` |
|      - |  3797 | ` * __reflect_build_attrs, __toString() calls __reflect_export_class. Those become` |
|      - |  3798 | ` * direct C the moment chunks 2, 3, 7 and 9 land.` |
|      - |  3799 | ` * ---------------------------------------------------------------------------` |
|      - |  3800 | ` */` |
|      - |  3801 | `#define RC_OBJ "__obj"` |
|      - |  3802 |  |
|      - |  3803 | ``/* The class a ReflectionClass reflects: its `name` slot, resolved. The`` |
|      - |  3804 | ` * constructor already stored the canonical name, so no autoload can be needed` |
|      - |  3805 | ` * here. */` |
|   1593 |  3806 | `static ph7_class * ReflectClassOf(ph7_context *pCtx)` |
|      5 |  3807 | `{` |
|   1598 |  3808 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  3809 | `	const char *zName;` |
|      - |  3810 | `	int nName;` |
|   1598 |  3811 | `	if( pThis == 0 ){` |
|    ! 0 |  3812 | `		return 0;` |
|      - |  3813 | `	}` |
|   1598 |  3814 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|   1598 |  3815 | `	if( nName < 1 ){` |
|    ! 0 |  3816 | `		return 0;` |
|      - |  3817 | `	}` |
|      - |  3818 | `	/* PH7_NativeAttrStr borrows bytes that are NOT NUL-terminated: the length` |
|      - |  3819 | `	 * has to travel with them. */` |
|   1598 |  3820 | `	return PH7_VmExtractClass(pCtx->pVm, zName, (sxu32)nName, FALSE, 0);` |
|    791 |  3821 | `}` |
|      - |  3822 | `/* The instance a ReflectionObject was built over, or NULL for a plain` |
|      - |  3823 | ` * ReflectionClass — what makes DYNAMIC properties visible. */` |
|    284 |  3824 | `static ph7_class_instance * ReflectClassObj(ph7_context *pCtx)` |
|      5 |  3825 | `{` |
|    289 |  3826 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    289 |  3827 | `	return pThis ? PH7_NativeAttrObj(pThis, RC_OBJ) : 0;` |
|      5 |  3828 | `}` |
|      - |  3829 | `/* $this->name as bytes. */` |
|    924 |  3830 | `static void ReflectClassName(ph7_context *pCtx, const char **pzOut, int *pnOut)` |
|      5 |  3831 | `{` |
|    929 |  3832 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    929 |  3833 | `	*pzOut = "";` |
|    929 |  3834 | `	*pnOut = 0;` |
|    929 |  3835 | `	if( pThis ){` |
|    929 |  3836 | `		PH7_NativeAttrStr(pThis, "name", pzOut, pnOut);` |
|    460 |  3837 | `	}` |
|    929 |  3838 | `}` |
|      - |  3839 | `/* Answer the truth of one of the reflected class's iFlags bits. */` |
|    170 |  3840 | `static int ReflectClassFlag(ph7_context *pCtx, sxi32 iFlag)` |
|      4 |  3841 | `{` |
|    174 |  3842 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|    174 |  3843 | `	return pClass != 0 && (pClass->iFlags & iFlag) != 0;` |
|      4 |  3844 | `}` |
|      - |  3845 | `#define REFLECT_CLASS_FLAG(NAME,FLAG,NEGATE) \` |
|      - |  3846 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  3847 | `	{ \` |
|      - |  3848 | `		SXUNUSED(nArg); \` |
|      - |  3849 | `		SXUNUSED(apArg); \` |
|      - |  3850 | `		ph7_result_bool(pCtx, NEGATE ? !ReflectClassFlag(pCtx,FLAG) : ReflectClassFlag(pCtx,FLAG)); \` |
|      - |  3851 | `		return PH7_OK; \` |
|      - |  3852 | `	}` |
|     35 |  3853 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isInternal,    PH7_CLASS_INTERNAL,  0)` |
|      5 |  3854 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isUserDefined, PH7_CLASS_INTERNAL,  1)` |
|      9 |  3855 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isInterface,   PH7_CLASS_INTERFACE, 0)` |
|      5 |  3856 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isTrait,       PH7_CLASS_TRAIT,     0)` |
|      - |  3857 | `/*` |
|      - |  3858 | ` * zend's IMPLICIT abstract bit, which this engine did not carry: an INTERFACE` |
|      - |  3859 | `` * that declares a method is abstract, so `(new ReflectionClass('Countable'))`` |
|      - |  3860 | `` * ->isAbstract()` is true where this said false for 24 of php's 25 interfaces`` |
|      - |  3861 | ` * and for every userland one besides. The bit follows the METHODS rather than` |
|      - |  3862 | `` * the keyword, which is why `Traversable` -- an interface with nothing in it --`` |
|      - |  3863 | ` * is php's one negative answer. Only this predicate reads it: getModifiers()` |
|      - |  3864 | `` * is 0 for an interface in php too, and the export writes `interface X`, never`` |
|      - |  3865 | `` * `abstract interface X`.`` |
|      - |  3866 | ` */` |
|     60 |  3867 | `static int vm_builtin_ReflectionClass_isAbstract(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  3868 | `{` |
|     63 |  3869 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     63 |  3870 | `	int bAbstract = 0;` |
|     30 |  3871 | `	SXUNUSED(nArg);` |
|     30 |  3872 | `	SXUNUSED(apArg);` |
|     63 |  3873 | `	if( pClass ){` |
|     89 |  3874 | `		bAbstract = (pClass->iFlags & PH7_CLASS_ABSTRACT) != 0` |
|     88 |  3875 | `			\|\| ((pClass->iFlags & PH7_CLASS_INTERFACE) != 0` |
|     40 |  3876 | `			 && SyHashTotalEntry(&pClass->hMethod) > 0);` |
|     30 |  3877 | `	}` |
|     63 |  3878 | `	ph7_result_bool(pCtx, bAbstract);` |
|     63 |  3879 | `	return PH7_OK;` |
|      3 |  3880 | `}` |
|    106 |  3881 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isFinal,       PH7_CLASS_FINAL,     0)` |
|      9 |  3882 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isReadOnly,    PH7_CLASS_READONLY,  0)` |
|     13 |  3883 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isEnum,        PH7_CLASS_ENUM,      0)` |
|      - |  3884 |  |
|      - |  3885 | ``/* Build a ReflectionClass over pTarget with its `name` already filled in: the`` |
|      - |  3886 | ` * constructor would only re-resolve a class this code is holding. */` |
|    478 |  3887 | `static int ReflectResultClassOf(ph7_context *pCtx, ph7_class *pTarget)` |
|      5 |  3888 | `{` |
|    483 |  3889 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  3890 | `	ph7_class *pRC;` |
|      - |  3891 | `	ph7_class_instance *pObj;` |
|    483 |  3892 | `	if( pTarget == 0 ){` |
|    ! 0 |  3893 | `		ph7_result_null(pCtx);` |
|    ! 0 |  3894 | `		return PH7_OK;` |
|      - |  3895 | `	}` |
|    483 |  3896 | `	pRC = PH7_VmExtractClass(pVm, "ReflectionClass", sizeof("ReflectionClass")-1, FALSE, 0);` |
|    483 |  3897 | `	if( pRC == 0 \|\| (pObj = PH7_NewClassInstance(pVm, pRC)) == 0 ){` |
|    ! 0 |  3898 | `		ph7_result_null(pCtx);` |
|    ! 0 |  3899 | `		return PH7_OK;` |
|      - |  3900 | `	}` |
|    718 |  3901 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(&pTarget->sName),` |
|    478 |  3902 | `		(int)SyStringLength(&pTarget->sName));` |
|    483 |  3903 | `	PH7_NativeResultObject(pCtx, pObj);` |
|    483 |  3904 | `	return PH7_OK;` |
|    240 |  3905 | `}` |
|      - |  3906 | ``/* The class an argument declared `ReflectionClass\|string` denotes: a reflector's`` |
|      - |  3907 | `` * `name` slot, or the string itself. NULL when it names nothing (after`` |
|      - |  3908 | ` * autoload). */` |
|     34 |  3909 | `static ph7_class * ReflectClassArg(ph7_context *pCtx, ph7_value *pArg)` |
|      1 |  3910 | `{` |
|     35 |  3911 | `	ph7_vm *pVm = pCtx->pVm;` |
|     35 |  3912 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|    ! 0 |  3913 | `		ph7_class_instance *pObj = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 |  3914 | `		ph7_class *pRC = PH7_VmExtractClass(pVm, "ReflectionClass", sizeof("ReflectionClass")-1, FALSE, 0);` |
|    ! 0 |  3915 | `		if( pRC && PH7_VmInstanceOf(pObj->pClass, pRC) ){` |
|      - |  3916 | `			const char *zName;` |
|      - |  3917 | `			int nName;` |
|    ! 0 |  3918 | `			PH7_NativeAttrStr(pObj, "name", &zName, &nName);` |
|    ! 0 |  3919 | `			return nName > 0 ? PH7_VmExtractClass(pVm, zName, (sxu32)nName, FALSE, 0) : 0;` |
|      - |  3920 | `		}` |
|    ! 0 |  3921 | `	}` |
|     35 |  3922 | `	return ReflectResolveClass(pVm, pArg);` |
|     12 |  3923 | `}` |
|      - |  3924 | `/* ---- constructors ---- */` |
|      - |  3925 | `/* ReflectionClass::__construct(object\|string $objectOrClass) */` |
|    935 |  3926 | `static int vm_builtin_ReflectionClass_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  3927 | `{` |
|    940 |  3928 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  3929 | `	ph7_class *pClass;` |
|    940 |  3930 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 |  3931 | `		return PH7_OK;` |
|      - |  3932 | `	}` |
|    940 |  3933 | `	pClass = ReflectResolveClass(pCtx->pVm, apArg[0]);` |
|    940 |  3934 | `	if( pClass == 0 ){` |
|      - |  3935 | `		/* php reports the NAME it was handed, after the declared object\|string` |
|      - |  3936 | ``		 * has coerced a scalar — `new ReflectionClass(1.5)` says Class "1.5". */`` |
|      - |  3937 | `		const char *zName;` |
|      - |  3938 | `		int nName;` |
|     12 |  3939 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|     17 |  3940 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      5 |  3941 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      - |  3942 | `	}` |
|   1390 |  3943 | `	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", SyStringData(&pClass->sName),` |
|    925 |  3944 | `		(int)SyStringLength(&pClass->sName));` |
|    930 |  3945 | `	return PH7_OK;` |
|    470 |  3946 | `}` |
|      - |  3947 | `/* ReflectionObject::__construct(object $object) — the same, plus the receiver` |
|      - |  3948 | ` * whose DYNAMIC properties the inherited accessors then report. */` |
|     20 |  3949 | `static int vm_builtin_ReflectionObject_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3950 | `{` |
|     21 |  3951 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  3952 | `	sxi32 rc;` |
|     21 |  3953 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 |  3954 | `		return PH7_OK;` |
|      - |  3955 | `	}` |
|     21 |  3956 | `	rc = vm_builtin_ReflectionClass_construct(pCtx, nArg, apArg);` |
|     21 |  3957 | `	if( rc != PH7_OK ){` |
|    ! 0 |  3958 | `		return rc;` |
|      - |  3959 | `	}` |
|     21 |  3960 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RC_OBJ, (ph7_class_instance *)apArg[0]->x.pOther);` |
|     21 |  3961 | `	return PH7_OK;` |
|     11 |  3962 | `}` |
|      - |  3963 | `/* ReflectionClass::__clone(): void — php declares it private, and the class is` |
|      - |  3964 | ` * uncloneable besides (PH7_CLASS_NOCLONE answers before any body runs). */` |
|    ! 0 |  3965 | `static int vm_builtin_ReflectionClass_clone(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  3966 | `{` |
|    ! 0 |  3967 | `	SXUNUSED(pCtx);` |
|    ! 0 |  3968 | `	SXUNUSED(nArg);` |
|    ! 0 |  3969 | `	SXUNUSED(apArg);` |
|    ! 0 |  3970 | `	return PH7_OK;` |
|    ! 0 |  3971 | `}` |
|      - |  3972 | `/* ---- name ---- */` |
|    488 |  3973 | `static int vm_builtin_ReflectionClass_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  3974 | `{` |
|      - |  3975 | `	const char *zName;` |
|      - |  3976 | `	int nName;` |
|    242 |  3977 | `	SXUNUSED(nArg);` |
|    242 |  3978 | `	SXUNUSED(apArg);` |
|    493 |  3979 | `	ReflectClassName(pCtx, &zName, &nName);` |
|    493 |  3980 | `	ph7_result_string(pCtx, zName, nName);` |
|    493 |  3981 | `	return PH7_OK;` |
|      5 |  3982 | `}` |
|      - |  3983 | `/* Offset just past the last namespace separator, or -1 when there is none. */` |
|      6 |  3984 | `static int ReflectNsCut(const char *zName, int nName)` |
|      1 |  3985 | `{` |
|      - |  3986 | `	int i;` |
|     91 |  3987 | `	for( i = nName - 1 ; i >= 0 ; i-- ){` |
|     85 |  3988 | `		if( zName[i] == '\\' ){` |
|    ! 0 |  3989 | `			return i;` |
|      - |  3990 | `		}` |
|     43 |  3991 | `	}` |
|      7 |  3992 | `	return -1;` |
|      4 |  3993 | `}` |
|      2 |  3994 | `static int vm_builtin_ReflectionClass_getShortName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  3995 | `{` |
|      - |  3996 | `	const char *zName;` |
|      - |  3997 | `	int nName, iCut;` |
|      1 |  3998 | `	SXUNUSED(nArg);` |
|      1 |  3999 | `	SXUNUSED(apArg);` |
|      3 |  4000 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 |  4001 | `	iCut = ReflectNsCut(zName, nName);` |
|      3 |  4002 | `	if( iCut < 0 ){` |
|      3 |  4003 | `		ph7_result_string(pCtx, zName, nName);` |
|      2 |  4004 | `	}else{` |
|    ! 0 |  4005 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|      - |  4006 | `	}` |
|      3 |  4007 | `	return PH7_OK;` |
|      1 |  4008 | `}` |
|      2 |  4009 | `static int vm_builtin_ReflectionClass_getNamespaceName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4010 | `{` |
|      - |  4011 | `	const char *zName;` |
|      - |  4012 | `	int nName, iCut;` |
|      1 |  4013 | `	SXUNUSED(nArg);` |
|      1 |  4014 | `	SXUNUSED(apArg);` |
|      3 |  4015 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 |  4016 | `	iCut = ReflectNsCut(zName, nName);` |
|      3 |  4017 | `	ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|      3 |  4018 | `	return PH7_OK;` |
|      1 |  4019 | `}` |
|      2 |  4020 | `static int vm_builtin_ReflectionClass_inNamespace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4021 | `{` |
|      - |  4022 | `	const char *zName;` |
|      - |  4023 | `	int nName;` |
|      1 |  4024 | `	SXUNUSED(nArg);` |
|      1 |  4025 | `	SXUNUSED(apArg);` |
|      3 |  4026 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 |  4027 | `	ph7_result_bool(pCtx, ReflectNsCut(zName, nName) >= 0);` |
|      3 |  4028 | `	return PH7_OK;` |
|      1 |  4029 | `}` |
|      2 |  4030 | `static int vm_builtin_ReflectionClass_isAnonymous(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4031 | `{` |
|      - |  4032 | `	const char *zName;` |
|      - |  4033 | `	int nName;` |
|      - |  4034 | `	static const char zAnon[] = "class@anonymous";` |
|      1 |  4035 | `	SXUNUSED(nArg);` |
|      1 |  4036 | `	SXUNUSED(apArg);` |
|      3 |  4037 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 |  4038 | `	ph7_result_bool(pCtx, nName >= (int)sizeof(zAnon)-1` |
|      1 |  4039 | `		&& SyMemcmp(zName, zAnon, sizeof(zAnon)-1) == 0);` |
|      3 |  4040 | `	return PH7_OK;` |
|      1 |  4041 | `}` |
|      - |  4042 | `/* ---- shape ---- */` |
|     48 |  4043 | `static int vm_builtin_ReflectionClass_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4044 | `{` |
|     49 |  4045 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     49 |  4046 | `	sxi64 iMods = 0;` |
|     24 |  4047 | `	SXUNUSED(nArg);` |
|     24 |  4048 | `	SXUNUSED(apArg);` |
|     49 |  4049 | `	if( pClass ){` |
|     49 |  4050 | `		if( pClass->iFlags & PH7_CLASS_ABSTRACT ){ iMods \|= 64; }` |
|     49 |  4051 | `		if( pClass->iFlags & PH7_CLASS_FINAL ){ iMods \|= 32; }` |
|     49 |  4052 | `		if( pClass->iFlags & PH7_CLASS_READONLY ){ iMods \|= 65536; }` |
|     24 |  4053 | `	}` |
|     49 |  4054 | `	ph7_result_int64(pCtx, iMods);` |
|     49 |  4055 | `	return PH7_OK;` |
|      1 |  4056 | `}` |
|     38 |  4057 | `static int vm_builtin_ReflectionClass_getParentClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  4058 | `{` |
|     41 |  4059 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     15 |  4060 | `	SXUNUSED(nArg);` |
|     15 |  4061 | `	SXUNUSED(apArg);` |
|     41 |  4062 | `	if( pClass == 0 \|\| pClass->pBase == 0 ){` |
|     13 |  4063 | `		ph7_result_bool(pCtx, 0);` |
|     13 |  4064 | `		return PH7_OK;` |
|      - |  4065 | `	}` |
|     31 |  4066 | `	return ReflectResultClassOf(pCtx, pClass->pBase);` |
|     18 |  4067 | `}` |
|      - |  4068 | `/* getInterfaceNames()/getInterfaces()/getTraitNames()/getTraits() — one walk,` |
|      - |  4069 | ` * four shapes. bReflector picks name-list vs {name: ReflectionClass}. */` |
|    104 |  4070 | `static int ReflectClassNameList(ph7_context *pCtx, int bTraits, int bReflector)` |
|      4 |  4071 | `{` |
|    108 |  4072 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|    108 |  4073 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|      - |  4074 | `	SySet aSet;` |
|      - |  4075 | `	ph7_class **apOut;` |
|      - |  4076 | `	sxu32 n, nOut;` |
|    108 |  4077 | `	if( pList == 0 ){` |
|    ! 0 |  4078 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  4079 | `	}` |
|    108 |  4080 | `	if( pClass == 0 ){` |
|    ! 0 |  4081 | `		ph7_result_value(pCtx, pList);` |
|    ! 0 |  4082 | `		return PH7_OK;` |
|      - |  4083 | `	}` |
|    108 |  4084 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|    108 |  4085 | `	if( bTraits ){` |
|      3 |  4086 | `		apOut = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      3 |  4087 | `		nOut = SySetUsed(&pClass->aTrait);` |
|      2 |  4088 | `	}else{` |
|    106 |  4089 | `		PH7_ReflectInterfacesOf(pCtx->pVm, pClass, &aSet);` |
|    106 |  4090 | `		apOut = (ph7_class **)SySetBasePtr(&aSet);` |
|    106 |  4091 | `		nOut = SySetUsed(&aSet);` |
|      - |  4092 | `	}` |
|    328 |  4093 | `	for( n = 0 ; n < nOut ; n++ ){` |
|    223 |  4094 | `		SyString *pName = &apOut[n]->sName;` |
|    223 |  4095 | `		if( bReflector ){` |
|      - |  4096 | `			/* {name: ReflectionClass} — the reflector is built here rather than` |
|      - |  4097 | `			 * through ReflectResultClassOf, which writes the RESULT slot. */` |
|      5 |  4098 | `			ph7_class *pRC = PH7_VmExtractClass(pCtx->pVm, "ReflectionClass",` |
|      - |  4099 | `				sizeof("ReflectionClass")-1, FALSE, 0);` |
|      5 |  4100 | `			ph7_class_instance *pObj = pRC ? PH7_NewClassInstance(pCtx->pVm, pRC) : 0;` |
|      5 |  4101 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|      - |  4102 | `			ph7_value sVal;` |
|      5 |  4103 | `			if( pObj == 0 \|\| pKey == 0 ){ break; }` |
|      7 |  4104 | `			PH7_NativeSetAttrStr(pCtx->pVm, pObj, "name", SyStringData(pName),` |
|      4 |  4105 | `				(int)SyStringLength(pName));` |
|      - |  4106 | `			/* A STACK carrier, never a context scalar: releasing a MEMOBJ_OBJ` |
|      - |  4107 | `			 * context value would unref the instance a second time. */` |
|      5 |  4108 | `			PH7_MemObjInit(pCtx->pVm, &sVal);` |
|      5 |  4109 | `			sVal.x.pOther = pObj;` |
|      5 |  4110 | `			sVal.iFlags = MEMOBJ_OBJ;` |
|      5 |  4111 | `			ph7_value_string(pKey, SyStringData(pName), (int)SyStringLength(pName));` |
|      5 |  4112 | `			ph7_array_add_elem(pList, pKey, &sVal);   /* takes its own reference */` |
|      5 |  4113 | `			PH7_ClassInstanceUnref(pObj);` |
|      3 |  4114 | `		}else{` |
|    219 |  4115 | `			ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    219 |  4116 | `			if( pVal == 0 ){ break; }` |
|    219 |  4117 | `			ph7_value_string(pVal, SyStringData(pName), (int)SyStringLength(pName));` |
|    219 |  4118 | `			ph7_array_add_elem(pList, 0, pVal);` |
|      - |  4119 | `		}` |
|    113 |  4120 | `	}` |
|    108 |  4121 | `	SySetRelease(&aSet);` |
|    108 |  4122 | `	ph7_result_value(pCtx, pList);` |
|    108 |  4123 | `	return PH7_OK;` |
|     56 |  4124 | `}` |
|      - |  4125 | `#define REFLECT_CLASS_LIST(NAME,TRAITS,REFLECTOR) \` |
|      - |  4126 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  4127 | `	{ \` |
|      - |  4128 | `		SXUNUSED(nArg); \` |
|      - |  4129 | `		SXUNUSED(apArg); \` |
|      - |  4130 | `		return ReflectClassNameList(pCtx,TRAITS,REFLECTOR); \` |
|      - |  4131 | `	}` |
|    104 |  4132 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getInterfaceNames, 0, 0)` |
|      3 |  4133 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getInterfaces,     0, 1)` |
|      3 |  4134 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getTraitNames,     1, 0)` |
|    ! 0 |  4135 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getTraits,         1, 1)` |
|      - |  4136 |  |
|      - |  4137 | ``/* getTraitAliases(): php reports `use T { m as n; }` renames; PHL's compiler`` |
|      - |  4138 | ` * installs the alias as a method and keeps no rename record, so the map is` |
|      - |  4139 | ` * empty (§7.4). */` |
|    ! 0 |  4140 | `static int vm_builtin_ReflectionClass_getTraitAliases(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  4141 | `{` |
|    ! 0 |  4142 | `	SXUNUSED(nArg);` |
|    ! 0 |  4143 | `	SXUNUSED(apArg);` |
|    ! 0 |  4144 | `	ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  4145 | `	return PH7_OK;` |
|    ! 0 |  4146 | `}` |
|      4 |  4147 | `static int vm_builtin_ReflectionClass_isIterable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4148 | `{` |
|      5 |  4149 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4150 | `	SySet aSet;` |
|      - |  4151 | `	ph7_class **apIface;` |
|      - |  4152 | `	sxu32 n;` |
|      5 |  4153 | `	int bIterable = 0;` |
|      2 |  4154 | `	SXUNUSED(nArg);` |
|      2 |  4155 | `	SXUNUSED(apArg);` |
|      4 |  4156 | `	if( pClass == 0` |
|      5 |  4157 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT)) ){` |
|    ! 0 |  4158 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4159 | `		return PH7_OK;` |
|      - |  4160 | `	}` |
|      5 |  4161 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|      5 |  4162 | `	PH7_ReflectInterfacesOf(pCtx->pVm, pClass, &aSet);` |
|      5 |  4163 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|      9 |  4164 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|      9 |  4165 | `		if( pCtx->pVm->pTraversableClass && apIface[n] == pCtx->pVm->pTraversableClass ){` |
|      5 |  4166 | `			bIterable = 1;` |
|      5 |  4167 | `			break;` |
|      - |  4168 | `		}` |
|      3 |  4169 | `	}` |
|      5 |  4170 | `	SySetRelease(&aSet);` |
|      5 |  4171 | `	ph7_result_bool(pCtx, bIterable);` |
|      5 |  4172 | `	return PH7_OK;` |
|      3 |  4173 | `}` |
|     28 |  4174 | `static int vm_builtin_ReflectionClass_implementsInterface(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4175 | `{` |
|     29 |  4176 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4177 | `	ph7_class *pTarget;` |
|      - |  4178 | `	SySet aSet;` |
|      - |  4179 | `	ph7_class **apIface;` |
|      - |  4180 | `	sxu32 n;` |
|     29 |  4181 | `	int bYes = 0;` |
|     29 |  4182 | `	if( nArg < 1 ){` |
|    ! 0 |  4183 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4184 | `		return PH7_OK;` |
|      - |  4185 | `	}` |
|     29 |  4186 | `	pTarget = ReflectClassArg(pCtx, apArg[0]);` |
|     29 |  4187 | `	if( pTarget == 0 ){` |
|      - |  4188 | `		const char *zName;` |
|      - |  4189 | `		int nName;` |
|      3 |  4190 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|      4 |  4191 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  4192 | `			"Interface \"%.*s\" does not exist", nName, zName);` |
|      - |  4193 | `	}` |
|     27 |  4194 | `	if( (pTarget->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      4 |  4195 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  4196 | `			"%z is not an interface", &pTarget->sName);` |
|      - |  4197 | `	}` |
|     25 |  4198 | `	if( pClass == pTarget ){` |
|      3 |  4199 | `		ph7_result_bool(pCtx, 1);` |
|      3 |  4200 | `		return PH7_OK;` |
|      - |  4201 | `	}` |
|     23 |  4202 | `	if( pClass == 0 ){` |
|    ! 0 |  4203 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4204 | `		return PH7_OK;` |
|      - |  4205 | `	}` |
|     23 |  4206 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|     23 |  4207 | `	PH7_ReflectInterfacesOf(pCtx->pVm, pClass, &aSet);` |
|     23 |  4208 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|     58 |  4209 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|     52 |  4210 | `		if( apIface[n] == pTarget ){` |
|     17 |  4211 | `			bYes = 1;` |
|     17 |  4212 | `			break;` |
|      - |  4213 | `		}` |
|      2 |  4214 | `	}` |
|     23 |  4215 | `	SySetRelease(&aSet);` |
|     23 |  4216 | `	ph7_result_bool(pCtx, bYes);` |
|     23 |  4217 | `	return PH7_OK;` |
|      9 |  4218 | `}` |
|      6 |  4219 | `static int vm_builtin_ReflectionClass_isSubclassOf(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4220 | `{` |
|      7 |  4221 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4222 | `	ph7_class *pTarget, *pWalk;` |
|      - |  4223 | `	SySet aSet;` |
|      - |  4224 | `	ph7_class **apIface;` |
|      - |  4225 | `	sxu32 n;` |
|      7 |  4226 | `	int iDepth = 0, bYes = 0;` |
|      7 |  4227 | `	if( nArg < 1 ){` |
|    ! 0 |  4228 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4229 | `		return PH7_OK;` |
|      - |  4230 | `	}` |
|      7 |  4231 | `	pTarget = ReflectClassArg(pCtx, apArg[0]);` |
|      7 |  4232 | `	if( pTarget == 0 ){` |
|      - |  4233 | `		const char *zName;` |
|      - |  4234 | `		int nName;` |
|    ! 0 |  4235 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|    ! 0 |  4236 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 |  4237 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      - |  4238 | `	}` |
|      - |  4239 | `	/* php: a class is never a subclass of ITSELF */` |
|      7 |  4240 | `	if( pClass == 0 \|\| pClass == pTarget ){` |
|      3 |  4241 | `		ph7_result_bool(pCtx, 0);` |
|      3 |  4242 | `		return PH7_OK;` |
|      - |  4243 | `	}` |
|      7 |  4244 | `	for( pWalk = pClass->pBase ; pWalk && iDepth <= REFLECT_WALK_MAX_DEPTH ; pWalk = pWalk->pBase ){` |
|      5 |  4245 | `		if( pWalk == pTarget ){` |
|      3 |  4246 | `			ph7_result_bool(pCtx, 1);` |
|      3 |  4247 | `			return PH7_OK;` |
|      - |  4248 | `		}` |
|      3 |  4249 | `		iDepth++;` |
|      2 |  4250 | `	}` |
|      3 |  4251 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|      3 |  4252 | `	PH7_ReflectInterfacesOf(pCtx->pVm, pClass, &aSet);` |
|      3 |  4253 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|      3 |  4254 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|      3 |  4255 | `		if( apIface[n] == pTarget ){` |
|      3 |  4256 | `			bYes = 1;` |
|      3 |  4257 | `			break;` |
|      - |  4258 | `		}` |
|    ! 0 |  4259 | `	}` |
|      3 |  4260 | `	SySetRelease(&aSet);` |
|      3 |  4261 | `	ph7_result_bool(pCtx, bYes);` |
|      3 |  4262 | `	return PH7_OK;` |
|      4 |  4263 | `}` |
|      8 |  4264 | `static int vm_builtin_ReflectionClass_isInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4265 | `{` |
|      9 |  4266 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4267 | `	ph7_class_instance *pObj;` |
|      9 |  4268 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 \|\| pClass == 0 ){` |
|    ! 0 |  4269 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4270 | `		return PH7_OK;` |
|      - |  4271 | `	}` |
|      9 |  4272 | `	pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      9 |  4273 | `	ph7_result_bool(pCtx, PH7_VmInstanceOf(pObj->pClass, pClass) != 0);` |
|      9 |  4274 | `	return PH7_OK;` |
|      5 |  4275 | `}` |
|      - |  4276 | `/* ---- source position ---- */` |
|     10 |  4277 | `static int vm_builtin_ReflectionClass_getStartLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4278 | `{` |
|     11 |  4279 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      5 |  4280 | `	SXUNUSED(nArg);` |
|      5 |  4281 | `	SXUNUSED(apArg);` |
|     11 |  4282 | `	if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_INTERNAL) ){` |
|      3 |  4283 | `		ph7_result_bool(pCtx, 0);` |
|      2 |  4284 | `	}else{` |
|      9 |  4285 | `		ph7_result_int64(pCtx, (sxi64)pClass->nLine);` |
|      - |  4286 | `	}` |
|     11 |  4287 | `	return PH7_OK;` |
|      1 |  4288 | `}` |
|      6 |  4289 | `static int vm_builtin_ReflectionClass_getEndLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4290 | `{` |
|      7 |  4291 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      3 |  4292 | `	SXUNUSED(nArg);` |
|      3 |  4293 | `	SXUNUSED(apArg);` |
|      7 |  4294 | `	if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_INTERNAL) ){` |
|      3 |  4295 | `		ph7_result_bool(pCtx, 0);` |
|      2 |  4296 | `	}else{` |
|      5 |  4297 | `		ph7_result_int64(pCtx, (sxi64)pClass->nEndLine);` |
|      - |  4298 | `	}` |
|      7 |  4299 | `	return PH7_OK;` |
|      1 |  4300 | `}` |
|     18 |  4301 | `static int vm_builtin_ReflectionClass_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4302 | `{` |
|     20 |  4303 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      9 |  4304 | `	SXUNUSED(nArg);` |
|      9 |  4305 | `	SXUNUSED(apArg);` |
|     20 |  4306 | `	if( pClass && SyStringLength(&pClass->sFile) > 0 ){` |
|     10 |  4307 | `		ph7_result_string(pCtx, SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));` |
|      6 |  4308 | `	}else{` |
|     11 |  4309 | `		ph7_result_bool(pCtx, 0);` |
|      - |  4310 | `	}` |
|     20 |  4311 | `	return PH7_OK;` |
|      2 |  4312 | `}` |
|      8 |  4313 | `static int vm_builtin_ReflectionClass_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4314 | `{` |
|      9 |  4315 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      4 |  4316 | `	SXUNUSED(nArg);` |
|      4 |  4317 | `	SXUNUSED(apArg);` |
|      9 |  4318 | `	if( pClass && SyStringLength(&pClass->sDoc) > 0 ){` |
|      3 |  4319 | `		ph7_result_string(pCtx, SyStringData(&pClass->sDoc), (int)SyStringLength(&pClass->sDoc));` |
|      2 |  4320 | `	}else{` |
|      7 |  4321 | `		ph7_result_bool(pCtx, 0);` |
|      - |  4322 | `	}` |
|      9 |  4323 | `	return PH7_OK;` |
|      1 |  4324 | `}` |
|      - |  4325 | `/* ---- instantiation ---- */` |
|     40 |  4326 | `static int vm_builtin_ReflectionClass_isInstantiable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  4327 | `{` |
|     44 |  4328 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4329 | `	sxi32 iCtor, iClone;` |
|     20 |  4330 | `	SXUNUSED(nArg);` |
|     20 |  4331 | `	SXUNUSED(apArg);` |
|     40 |  4332 | `	if( pClass == 0` |
|     44 |  4333 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT\|PH7_CLASS_ENUM)) ){` |
|      7 |  4334 | `		ph7_result_bool(pCtx, 0);` |
|      7 |  4335 | `		return PH7_OK;` |
|      - |  4336 | `	}` |
|     38 |  4337 | `	ReflectCtorCloneVis(pCtx->pVm, pClass, &iCtor, &iClone);` |
|     38 |  4338 | `	ph7_result_bool(pCtx, iCtor == 0 \|\| iCtor == PH7_CLASS_PROT_PUBLIC);` |
|     38 |  4339 | `	return PH7_OK;` |
|     24 |  4340 | `}` |
|     36 |  4341 | `static int vm_builtin_ReflectionClass_isCloneable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4342 | `{` |
|     38 |  4343 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4344 | `	sxi32 iCtor, iClone;` |
|     18 |  4345 | `	SXUNUSED(nArg);` |
|     18 |  4346 | `	SXUNUSED(apArg);` |
|     36 |  4347 | `	if( pClass == 0` |
|     38 |  4348 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT)) ){` |
|      3 |  4349 | `		ph7_result_bool(pCtx, 0);` |
|      3 |  4350 | `		return PH7_OK;` |
|      - |  4351 | `	}` |
|     36 |  4352 | `	ReflectCtorCloneVis(pCtx->pVm, pClass, &iCtor, &iClone);` |
|     36 |  4353 | `	if( iClone != 0 ){` |
|      - |  4354 | `		/* php consults a declared __clone FIRST and answers its visibility --` |
|      - |  4355 | ``		 * even when the clone_obj refusal would still answer `clone` itself, so`` |
|      - |  4356 | `		 * a subclass of Exception that declares a public __clone reports TRUE` |
|      - |  4357 | `		 * and refuses anyway. php's own inconsistency, kept. */` |
|      7 |  4358 | `		ph7_result_bool(pCtx, iClone == PH7_CLASS_PROT_PUBLIC);` |
|      7 |  4359 | `		return PH7_OK;` |
|      - |  4360 | `	}` |
|      - |  4361 | `	/* No __clone anywhere: php's answer is whether the clone_obj handler` |
|      - |  4362 | `	 * exists. The refusal flag walks the base chain like the handler it` |
|      - |  4363 | ``	 * models, so `class M extends IteratorIterator {}` reports false too. */`` |
|     30 |  4364 | `	ph7_result_bool(pCtx, !PH7_ClassIsUncloneable(pClass));` |
|     30 |  4365 | `	return PH7_OK;` |
|     20 |  4366 | `}` |
|      - |  4367 | `/*` |
|      - |  4368 | ` * php's own gate, raised before any object exists -- the one every C-side` |
|      - |  4369 | ` * instantiation asks, Reflection's newInstance() and PDO's FETCH_CLASS alike.` |
|      - |  4370 | ` */` |
|    282 |  4371 | `PH7_PRIVATE sxi32 PH7_VmCheckInstantiable(ph7_context *pCtx, ph7_class *pClass)` |
|      4 |  4372 | `{` |
|    286 |  4373 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|      3 |  4374 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate interface %z", &pClass->sName);` |
|      - |  4375 | `	}` |
|    284 |  4376 | `	if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|      3 |  4377 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate trait %z", &pClass->sName);` |
|      - |  4378 | `	}` |
|    282 |  4379 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|      - |  4380 | `		/* php 8.1 names the enum rather than the FINAL class it also is. */` |
|      3 |  4381 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate enum %z", &pClass->sName);` |
|      - |  4382 | `	}` |
|    280 |  4383 | `	if( pClass->iFlags & PH7_CLASS_ABSTRACT ){` |
|      7 |  4384 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate abstract class %z", &pClass->sName);` |
|      - |  4385 | `	}` |
|    274 |  4386 | `	if( pClass->iFlags & PH7_CLASS_NOINSTANTIATE ){` |
|      - |  4387 | ``		/* The `new` path's create_object refusal, which php raises here too —`` |
|      - |  4388 | `		 * ReflectionClass::newInstance() on a Closure is the same Error, not a` |
|      - |  4389 | `		 * visibility one about its private constructor. */` |
|     12 |  4390 | `		if( pClass->zNewRefusal ){` |
|     10 |  4391 | `			return PH7_VmThrowException(pCtx,` |
|      8 |  4392 | `				pClass->zNewRefusalClass ? pClass->zNewRefusalClass : "Error",` |
|      4 |  4393 | `				"%s", pClass->zNewRefusal);` |
|      - |  4394 | `		}` |
|      4 |  4395 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      1 |  4396 | `			"Instantiation of class %z is not allowed", &pClass->sName);` |
|      - |  4397 | `	}` |
|    264 |  4398 | `	return PH7_OK;` |
|    145 |  4399 | `}` |
|      - |  4400 | `/*` |
|      - |  4401 | ` * newInstance()/newInstanceArgs(): the checks php runs, then the constructor.` |
|      - |  4402 | ` * apCtor/nCtor are already-collected positional arguments; pNames is the` |
|      - |  4403 | ` * name map when the caller handed an array with string keys (php 8.1 accepts` |
|      - |  4404 | ` * those as NAMED constructor arguments).` |
|      - |  4405 | ` */` |
|     36 |  4406 | `static int ReflectNewInstance(ph7_context *pCtx, int nCtor, ph7_value **apCtor,` |
|      - |  4407 | `	SyString *pNames)` |
|      4 |  4408 | `{` |
|     40 |  4409 | `	ph7_vm *pVm = pCtx->pVm;` |
|     40 |  4410 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4411 | `	ph7_class_instance *pObj;` |
|      - |  4412 | `	ph7_class_method *pCons;` |
|      - |  4413 | `	sxi32 iCtorVis, iCloneVis, rc;` |
|     40 |  4414 | `	if( pClass == 0 ){` |
|    ! 0 |  4415 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4416 | `		return PH7_OK;` |
|      - |  4417 | `	}` |
|     40 |  4418 | `	rc = PH7_VmCheckInstantiable(pCtx, pClass);` |
|     40 |  4419 | `	if( rc != PH7_OK ){` |
|     14 |  4420 | `		return rc;` |
|      - |  4421 | `	}` |
|     28 |  4422 | `	ReflectCtorCloneVis(pVm, pClass, &iCtorVis, &iCloneVis);` |
|     28 |  4423 | `	if( iCtorVis != 0 && iCtorVis != PH7_CLASS_PROT_PUBLIC ){` |
|      4 |  4424 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  4425 | `			"Access to non-public constructor of class %z", &pClass->sName);` |
|      - |  4426 | `	}` |
|     26 |  4427 | `	if( iCtorVis == 0 && nCtor > 0 ){` |
|      4 |  4428 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  4429 | `			"Class %z does not have a constructor, so you cannot pass any constructor arguments",` |
|      1 |  4430 | `			&pClass->sName);` |
|      - |  4431 | `	}` |
|     24 |  4432 | `	if( VmClassStaticDeferPending(pClass) ){` |
|      - |  4433 | `		/* Instantiation materializes the static table (OP_NEW does it too), so a` |
|      - |  4434 | `		 * broken default raises BEFORE any object exists. */` |
|      3 |  4435 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pVm, pClass);` |
|      3 |  4436 | `		if( rcMat != SXRET_OK ){` |
|      3 |  4437 | `			return rcMat;` |
|      - |  4438 | `		}` |
|    ! 0 |  4439 | `	}` |
|     21 |  4440 | `	pObj = PH7_NewClassInstance(pVm, pClass);` |
|     21 |  4441 | `	if( pObj == 0 ){` |
|    ! 0 |  4442 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4443 | `		return PH7_OK;` |
|      - |  4444 | `	}` |
|     21 |  4445 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|     21 |  4446 | `	if( pCons ){` |
|      - |  4447 | `		/* Weak binding, like ReflectMethodInvoke: the frame that makes this call is` |
|      - |  4448 | `		 * ReflectionClass::newInstance(), an internal function. */` |
|     19 |  4449 | `		pVm->bCallbackWeak = 1;` |
|     19 |  4450 | `		if( pNames ){` |
|      - |  4451 | `			VmCallArgMap sMap;` |
|    ! 0 |  4452 | `			SyZero(&sMap, sizeof(sMap));` |
|    ! 0 |  4453 | `			sMap.bHasNamed = 1;` |
|    ! 0 |  4454 | `			sMap.nTotal = (sxu32)nCtor;` |
|    ! 0 |  4455 | `			sMap.aNames = pNames;` |
|    ! 0 |  4456 | `			rc = PH7_VmCallClassMethodMap(pVm, pObj, pCons, 0, nCtor, apCtor, &sMap);` |
|    ! 0 |  4457 | `		}else{` |
|     19 |  4458 | `			rc = PH7_VmCallClassMethod(pVm, pObj, pCons, 0, nCtor, apCtor);` |
|      - |  4459 | `		}` |
|     19 |  4460 | `		pVm->bCallbackWeak = 0;` |
|     19 |  4461 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|      7 |  4462 | `			PH7_ClassInstanceCtorFailed(pObj);` |
|      7 |  4463 | `			PH7_ClassInstanceUnref(pObj);` |
|      7 |  4464 | `			return rc;` |
|      - |  4465 | `		}` |
|      5 |  4466 | `	}` |
|     14 |  4467 | `	return ReflectResultObject(pCtx, pObj);` |
|     22 |  4468 | `}` |
|     26 |  4469 | `static int vm_builtin_ReflectionClass_newInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  4470 | `{` |
|     30 |  4471 | `	return ReflectNewInstance(pCtx, nArg, apArg, 0);` |
|      4 |  4472 | `}` |
|     10 |  4473 | `static int vm_builtin_ReflectionClass_newInstanceArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  4474 | `{` |
|      - |  4475 | `	SySet aArg;` |
|     13 |  4476 | `	SyString *aNames = 0;` |
|      - |  4477 | `	int rc;` |
|     13 |  4478 | `	SySetInit(&aArg, &pCtx->pVm->sAllocator, sizeof(ph7_value *));` |
|     13 |  4479 | `	if( nArg > 0 ){` |
|     13 |  4480 | `		ReflectCollectArgs(pCtx, apArg[0], &aArg, &aNames);` |
|      5 |  4481 | `	}` |
|     18 |  4482 | `	rc = ReflectNewInstance(pCtx, (int)SySetUsed(&aArg),` |
|     10 |  4483 | `		(ph7_value **)SySetBasePtr(&aArg), aNames);` |
|     13 |  4484 | `	if( aNames ){` |
|    ! 0 |  4485 | `		SyMemBackendFree(&pCtx->pVm->sAllocator, aNames);` |
|    ! 0 |  4486 | `	}` |
|     13 |  4487 | `	SySetRelease(&aArg);` |
|     13 |  4488 | `	return rc;` |
|      3 |  4489 | `}` |
|    182 |  4490 | `static int vm_builtin_ReflectionClass_newInstanceWithoutConstructor(ph7_context *pCtx,` |
|      - |  4491 | `	int nArg, ph7_value **apArg)` |
|      3 |  4492 | `{` |
|    185 |  4493 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4494 | `	sxi32 rc;` |
|     91 |  4495 | `	SXUNUSED(nArg);` |
|     91 |  4496 | `	SXUNUSED(apArg);` |
|    185 |  4497 | `	if( pClass == 0 ){` |
|    ! 0 |  4498 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4499 | `		return PH7_OK;` |
|      - |  4500 | `	}` |
|    185 |  4501 | `	rc = PH7_VmCheckInstantiable(pCtx, pClass);` |
|    185 |  4502 | `	if( rc != PH7_OK ){` |
|      3 |  4503 | `		return rc;` |
|      - |  4504 | `	}` |
|    183 |  4505 | `	if( VmClassStaticDeferPending(pClass) ){` |
|      3 |  4506 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);` |
|      3 |  4507 | `		if( rcMat != SXRET_OK ){` |
|      3 |  4508 | `			return rcMat;` |
|      - |  4509 | `		}` |
|    ! 0 |  4510 | `	}` |
|    181 |  4511 | `	return ReflectResultObject(pCtx, PH7_NewClassInstance(pCtx->pVm, pClass));` |
|     94 |  4512 | `}` |
|      - |  4513 | `/* ---- members ---- */` |
|      - |  4514 | `/* The modifier mask php filters a member on. */` |
|    252 |  4515 | `static sxi64 ReflectVisMask(sxi32 iProt)` |
|      2 |  4516 | `{` |
|    254 |  4517 | `	if( iProt == PH7_CLASS_PROT_PUBLIC ){` |
|    176 |  4518 | `		return 1;` |
|      - |  4519 | `	}` |
|     79 |  4520 | `	return iProt == PH7_CLASS_PROT_PROTECTED ? 2 : 4;` |
|    128 |  4521 | `}` |
|      - |  4522 | `/*` |
|      - |  4523 | ` * Is this property protected(set) as far as php's Reflection is concerned?` |
|      - |  4524 | ` *` |
|      - |  4525 | `` * The bit is either DECLARED or implied by `readonly` — but readonly implies it`` |
|      - |  4526 | `` * only for a property whose read side is PUBLIC. A `protected readonly` or`` |
|      - |  4527 | `` * `private readonly` one already writes no wider than it reads, so php adds`` |
|      - |  4528 | `` * nothing (`private readonly int $v` is modifiers 132, not 2180), and an explicit`` |
|      - |  4529 | `` * `private(set)` beside readonly is the set visibility, so readonly adds nothing`` |
|      - |  4530 | `` * there either (`public private(set) readonly` is 4257).`` |
|      - |  4531 | ` */` |
|    230 |  4532 | `static int ReflectPropProtectedSet(ph7_class_attr *pAttr)` |
|      2 |  4533 | `{` |
|    232 |  4534 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET ){` |
|     28 |  4535 | `		return 1;` |
|      - |  4536 | `	}` |
|    225 |  4537 | `	return (pAttr->iFlags & PH7_CLASS_ATTR_READONLY)` |
|    122 |  4538 | `	    && (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) == 0` |
|    224 |  4539 | `	    && pAttr->iProtection == PH7_CLASS_PROT_PUBLIC;` |
|    117 |  4540 | `}` |
|    208 |  4541 | `static sxi64 ReflectPropModifiers(ph7_class_attr *pAttr)` |
|      2 |  4542 | `{` |
|    210 |  4543 | `	sxi64 iMods = ReflectVisMask(pAttr->iProtection);` |
|    210 |  4544 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){ iMods \|= 16; }` |
|      - |  4545 | `	/* php's IS_VIRTUAL is "there is no slot behind this name", and it has two` |
|      - |  4546 | `	 * sources: a HOOKED property with no backing store, and a NATIVE class's` |
|      - |  4547 | `	 * property that php fabricates from its own C struct (DatePeriod's, and` |
|      - |  4548 | `	 * BcMath\Number's value/scale). Both report virtual there. */` |
|    210 |  4549 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_VIRTUAL\|PH7_CLASS_ATTR_NATIVE_VIRTUAL) ){` |
|     65 |  4550 | `		iMods \|= 512;` |
|     32 |  4551 | `	}` |
|    210 |  4552 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){ iMods \|= 128; }` |
|    210 |  4553 | `	if( ReflectPropProtectedSet(pAttr) ){ iMods \|= 2048; }` |
|      - |  4554 | ``	/* PHP 8.4's `final` PROPERTY -- IS_FINAL, the same bit a method and a class`` |
|      - |  4555 | `	 * constant carry. */` |
|    210 |  4556 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods \|= 32; }` |
|    210 |  4557 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|      - |  4558 | `		/* private(set) cannot be widened by a subclass, so php reports it FINAL. */` |
|     19 |  4559 | `		iMods \|= 4096\|32;` |
|      9 |  4560 | `	}` |
|    210 |  4561 | `	return iMods;` |
|      2 |  4562 | `}` |
|     18 |  4563 | `static sxi64 ReflectMethodModifiers(ph7_class_method *pMeth)` |
|      1 |  4564 | `{` |
|     19 |  4565 | `	sxi64 iMods = ReflectVisMask(pMeth->iProtection);` |
|     19 |  4566 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){ iMods \|= 16; }` |
|     19 |  4567 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){ iMods \|= 64; }` |
|     19 |  4568 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods \|= 32; }` |
|     19 |  4569 | `	return iMods;` |
|      1 |  4570 | `}` |
|     26 |  4571 | `static sxi64 ReflectConstModifiers(ph7_class_attr *pAttr)` |
|      1 |  4572 | `{` |
|     27 |  4573 | `	sxi64 iMods = ReflectVisMask(pAttr->iProtection);` |
|     27 |  4574 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods \|= 32; }` |
|     27 |  4575 | `	return iMods;` |
|      1 |  4576 | `}` |
|      - |  4577 | `/* Does the reflected object own a property under this name? (1 = exists) */` |
|     18 |  4578 | `static int ReflectObjHasProp(ph7_class_instance *pObj, const char *zName, int nName)` |
|      1 |  4579 | `{` |
|     14 |  4580 | `	return pObj != 0 && nName > 0` |
|     20 |  4581 | `		&& SyHashGet(&pObj->hAttr, (const void *)zName, (sxu32)nName) != 0;` |
|      1 |  4582 | `}` |
|     14 |  4583 | `static int vm_builtin_ReflectionClass_hasMethod(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4584 | `{` |
|     15 |  4585 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4586 | `	const char *zName;` |
|      - |  4587 | `	int nName;` |
|     15 |  4588 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  4589 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4590 | `		return PH7_OK;` |
|      - |  4591 | `	}` |
|     15 |  4592 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     15 |  4593 | `	ph7_result_bool(pCtx, ReflectFindMethodEntry(pClass, zName, nName) != 0);` |
|     15 |  4594 | `	return PH7_OK;` |
|      8 |  4595 | `}` |
|     18 |  4596 | `static int vm_builtin_ReflectionClass_hasProperty(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4597 | `{` |
|     20 |  4598 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4599 | `	const char *zName;` |
|     20 |  4600 | `	int nName, bFound = 0;` |
|      - |  4601 | `	SySet aMembers;` |
|      - |  4602 | `	sxu32 n;` |
|     20 |  4603 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  4604 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4605 | `		return PH7_OK;` |
|      - |  4606 | `	}` |
|     20 |  4607 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     20 |  4608 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     20 |  4609 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     56 |  4610 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     44 |  4611 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     44 |  4612 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zName, nName) ){` |
|      8 |  4613 | `			bFound = 1;` |
|      8 |  4614 | `			break;` |
|      - |  4615 | `		}` |
|     19 |  4616 | `	}` |
|     20 |  4617 | `	SySetRelease(&aMembers);` |
|      - |  4618 | `	/* A ReflectionObject also sees the instance's own dynamic properties */` |
|     20 |  4619 | `	if( !bFound ){` |
|     13 |  4620 | `		bFound = ReflectObjHasProp(ReflectClassObj(pCtx), zName, nName);` |
|      6 |  4621 | `	}` |
|     20 |  4622 | `	ph7_result_bool(pCtx, bFound);` |
|     20 |  4623 | `	return PH7_OK;` |
|     11 |  4624 | `}` |
|     24 |  4625 | `static int vm_builtin_ReflectionClass_hasConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4626 | `{` |
|     26 |  4627 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4628 | `	const char *zName;` |
|     26 |  4629 | `	int nName, bFound = 0;` |
|      - |  4630 | `	SySet aMembers;` |
|      - |  4631 | `	sxu32 n;` |
|     26 |  4632 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  4633 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4634 | `		return PH7_OK;` |
|      - |  4635 | `	}` |
|     26 |  4636 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     26 |  4637 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     26 |  4638 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|   1320 |  4639 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   1300 |  4640 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   1300 |  4641 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|      5 |  4642 | `			bFound = 1;` |
|      5 |  4643 | `			break;` |
|      - |  4644 | `		}` |
|    649 |  4645 | `	}` |
|     26 |  4646 | `	SySetRelease(&aMembers);` |
|     26 |  4647 | `	ph7_result_bool(pCtx, bFound);` |
|     26 |  4648 | `	return PH7_OK;` |
|     14 |  4649 | `}` |
|      - |  4650 | `/*` |
|      - |  4651 | ` * The value of a class constant, materializing its lazily-evaluated slot.` |
|      - |  4652 | ` *` |
|      - |  4653 | ` * php re-evaluates a failed initializer on EVERY read, so this can raise; the` |
|      - |  4654 | ` * status is returned rather than swallowed, because a native body that answers` |
|      - |  4655 | ` * PH7_OK with a throw in flight lets the caller carry on and print the NULL it` |
|      - |  4656 | ` * never should have seen.` |
|      - |  4657 | ` */` |
|    788 |  4658 | `static sxi32 ReflectConstSlot(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr,` |
|      - |  4659 | `	ph7_value **ppOut)` |
|      4 |  4660 | `{` |
|    792 |  4661 | `	sxi32 rc = PH7_VmMaterializeClassConst(pCtx->pVm, pClass, pAttr);` |
|    792 |  4662 | `	*ppOut = 0;` |
|    792 |  4663 | `	if( rc != SXRET_OK ){` |
|      3 |  4664 | `		return rc;` |
|      - |  4665 | `	}` |
|    789 |  4666 | `	*ppOut = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|    789 |  4667 | `	return SXRET_OK;` |
|    384 |  4668 | `}` |
|     10 |  4669 | `static int vm_builtin_ReflectionClass_getConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  4670 | `{` |
|     12 |  4671 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4672 | `	const char *zName;` |
|      - |  4673 | `	int nName;` |
|      - |  4674 | `	SySet aMembers;` |
|      - |  4675 | `	sxu32 n;` |
|     12 |  4676 | `	ph7_class_attr *pFound = 0;` |
|     12 |  4677 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  4678 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4679 | `		return PH7_OK;` |
|      - |  4680 | `	}` |
|     12 |  4681 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     12 |  4682 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     12 |  4683 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     28 |  4684 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     28 |  4685 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     28 |  4686 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|     12 |  4687 | `			pFound = pM->pAttr;` |
|     12 |  4688 | `			break;` |
|      - |  4689 | `		}` |
|      9 |  4690 | `	}` |
|     12 |  4691 | `	SySetRelease(&aMembers);` |
|     12 |  4692 | `	if( pFound == 0 ){` |
|    ! 0 |  4693 | `		PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED,` |
|      - |  4694 | `			"ReflectionClass::getConstant() for a non-existent constant is deprecated, "` |
|      - |  4695 | `			"use ReflectionClass::hasConstant() to check if the constant exists");` |
|    ! 0 |  4696 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  4697 | `		return PH7_OK;` |
|      - |  4698 | `	}` |
|      - |  4699 | `	{` |
|      - |  4700 | `		ph7_value *pVal;` |
|     12 |  4701 | `		sxi32 rc = ReflectConstSlot(pCtx, pClass, pFound, &pVal);` |
|     12 |  4702 | `		if( rc != SXRET_OK ){` |
|      3 |  4703 | `			return rc;` |
|      - |  4704 | `		}` |
|      9 |  4705 | `		if( pVal ){` |
|      9 |  4706 | `			ph7_result_value(pCtx, pVal);` |
|      5 |  4707 | `		}else{` |
|    ! 0 |  4708 | `			ph7_result_null(pCtx);` |
|      - |  4709 | `		}` |
|      - |  4710 | `	}` |
|      9 |  4711 | `	return PH7_OK;` |
|      7 |  4712 | `}` |
|     53 |  4713 | `static int vm_builtin_ReflectionClass_getConstants(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  4714 | `{` |
|     57 |  4715 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     57 |  4716 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  4717 | `	SySet aMembers;` |
|      - |  4718 | `	sxu32 n;` |
|     57 |  4719 | `	int bFilter = (nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0);` |
|     57 |  4720 | `	sxi64 iFilter = bFilter ? ph7_value_to_int64(apArg[0]) : 0;` |
|     57 |  4721 | `	if( pOut == 0 ){` |
|    ! 0 |  4722 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  4723 | `	}` |
|     57 |  4724 | `	if( pClass == 0 ){` |
|    ! 0 |  4725 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  4726 | `		return PH7_OK;` |
|      - |  4727 | `	}` |
|     57 |  4728 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     57 |  4729 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|    432 |  4730 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    378 |  4731 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - |  4732 | `		ph7_value *pVal;` |
|    378 |  4733 | `		if( pM->iKind != REFLECT_MEMBER_CONST ){` |
|    282 |  4734 | `			continue;` |
|      - |  4735 | `		}` |
|    100 |  4736 | `		if( bFilter && (ReflectConstModifiers(pM->pAttr) & iFilter) == 0 ){` |
|      5 |  4737 | `			continue;` |
|      - |  4738 | `		}` |
|      - |  4739 | `		{` |
|     96 |  4740 | `			sxi32 rc = ReflectConstSlot(pCtx, pClass, pM->pAttr, &pVal);` |
|     96 |  4741 | `			if( rc != SXRET_OK ){` |
|    ! 0 |  4742 | `				SySetRelease(&aMembers);` |
|    ! 0 |  4743 | `				return rc;` |
|      - |  4744 | `			}` |
|      - |  4745 | `		}` |
|     96 |  4746 | `		if( pVal ){` |
|     96 |  4747 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, pVal);` |
|     33 |  4748 | `		}` |
|     35 |  4749 | `	}` |
|     57 |  4750 | `	SySetRelease(&aMembers);` |
|     57 |  4751 | `	ph7_result_value(pCtx, pOut);` |
|     57 |  4752 | `	return PH7_OK;` |
|     30 |  4753 | `}` |
|      - |  4754 | `/*` |
|      - |  4755 | ` * getMethod()/getProperty()/getReflectionConstant() build a class that is still` |
|      - |  4756 | ` * PRELUDE PHP, so they go through its constructor (ReflectConstruct). They` |
|      - |  4757 | ` * become direct C when chunks 2 and 3 land.` |
|      - |  4758 | ` */` |
|     88 |  4759 | `static int ReflectResultMember(ph7_context *pCtx, const char *zClass,` |
|      - |  4760 | `	ph7_value *pTarget, const SyString *pName)` |
|      3 |  4761 | `{` |
|      - |  4762 | `	ph7_value sName;` |
|      - |  4763 | `	ph7_value *apCtor[2];` |
|      - |  4764 | `	ph7_class_instance *pOut;` |
|      - |  4765 | `	sxi32 rc;` |
|     91 |  4766 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|     91 |  4767 | `	ph7_value_string(&sName, SyStringData(pName), (int)SyStringLength(pName));` |
|     91 |  4768 | `	apCtor[0] = pTarget;` |
|     91 |  4769 | `	apCtor[1] = &sName;` |
|     91 |  4770 | `	pOut = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|     91 |  4771 | `	PH7_MemObjRelease(&sName);` |
|     91 |  4772 | `	if( pOut == 0 ){` |
|    ! 0 |  4773 | `		if( rc != PH7_OK ){` |
|    ! 0 |  4774 | `			return rc;` |
|      - |  4775 | `		}` |
|    ! 0 |  4776 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4777 | `		return PH7_OK;` |
|      - |  4778 | `	}` |
|     91 |  4779 | `	return ReflectResultObject(pCtx, pOut);` |
|     47 |  4780 | `}` |
|      - |  4781 | ``/* A `ReflectionMethod($this->name, ...)`-shaped first argument. */`` |
|    346 |  4782 | `static void ReflectSelfName(ph7_context *pCtx, ph7_value *pOut)` |
|      5 |  4783 | `{` |
|      - |  4784 | `	const char *zName;` |
|      - |  4785 | `	int nName;` |
|    351 |  4786 | `	ReflectClassName(pCtx, &zName, &nName);` |
|    351 |  4787 | `	PH7_MemObjInit(pCtx->pVm, pOut);` |
|    351 |  4788 | `	ph7_value_string(pOut, zName, nName);` |
|    351 |  4789 | `}` |
|     54 |  4790 | `static int vm_builtin_ReflectionClass_getMethod(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4791 | `{` |
|     55 |  4792 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4793 | `	SyHashEntry *pEntry;` |
|      - |  4794 | `	const char *zName;` |
|      - |  4795 | `	int nName;` |
|      - |  4796 | `	SyString sFound;` |
|     55 |  4797 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  4798 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4799 | `		return PH7_OK;` |
|      - |  4800 | `	}` |
|     55 |  4801 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     55 |  4802 | `	pEntry = ReflectFindMethodEntry(pClass, zName, nName);` |
|     55 |  4803 | `	if( pEntry == 0 ){` |
|      4 |  4804 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  4805 | `			"Method %z::%.*s() does not exist", &pClass->sName, nName, zName);` |
|      - |  4806 | `	}` |
|      - |  4807 | `	/* The reported name is the DECLARED spelling, whatever case was asked for. */` |
|     53 |  4808 | `	SyStringInitFromBuf(&sFound, (const char *)pEntry->pKey, pEntry->nKeyLen);` |
|      - |  4809 | `	{` |
|      - |  4810 | `		ph7_value sSelf;` |
|      - |  4811 | `		int rc;` |
|     53 |  4812 | `		ReflectSelfName(pCtx, &sSelf);` |
|     53 |  4813 | `		rc = ReflectResultMember(pCtx, "ReflectionMethod", &sSelf, &sFound);` |
|     53 |  4814 | `		PH7_MemObjRelease(&sSelf);` |
|     53 |  4815 | `		return rc;` |
|      - |  4816 | `	}` |
|     28 |  4817 | `}` |
|     28 |  4818 | `static int vm_builtin_ReflectionClass_getConstructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  4819 | `{` |
|     31 |  4820 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  4821 | `	SyHashEntry *pEntry;` |
|      - |  4822 | `	SyString sFound;` |
|     14 |  4823 | `	SXUNUSED(nArg);` |
|     14 |  4824 | `	SXUNUSED(apArg);` |
|     31 |  4825 | `	if( pClass == 0 ){` |
|    ! 0 |  4826 | `		ph7_result_null(pCtx);` |
|    ! 0 |  4827 | `		return PH7_OK;` |
|      - |  4828 | `	}` |
|      - |  4829 | `	/* No PHP-4 class-name constructor: a method named like the class is a plain` |
|      - |  4830 | `	 * method (removed in 8.0), so this stays null without an explicit one. */` |
|     31 |  4831 | `	pEntry = ReflectFindMethodEntry(pClass, "__construct", sizeof("__construct")-1);` |
|     31 |  4832 | `	if( pEntry == 0 ){` |
|     11 |  4833 | `		ph7_result_null(pCtx);` |
|     11 |  4834 | `		return PH7_OK;` |
|      - |  4835 | `	}` |
|     23 |  4836 | `	SyStringInitFromBuf(&sFound, (const char *)pEntry->pKey, pEntry->nKeyLen);` |
|      - |  4837 | `	{` |
|      - |  4838 | `		ph7_value sSelf;` |
|      - |  4839 | `		int rc;` |
|     23 |  4840 | `		ReflectSelfName(pCtx, &sSelf);` |
|     23 |  4841 | `		rc = ReflectResultMember(pCtx, "ReflectionMethod", &sSelf, &sFound);` |
|     23 |  4842 | `		PH7_MemObjRelease(&sSelf);` |
|     23 |  4843 | `		return rc;` |
|      - |  4844 | `	}` |
|     17 |  4845 | `}` |
|      - |  4846 | `/*` |
|      - |  4847 | ` * getMethods() / getProperties() / getReflectionConstants(): one member walk,` |
|      - |  4848 | ` * one reflector per surviving member. Each reflector is a prelude class built` |
|      - |  4849 | ` * through its constructor, and is appended through a STACK carrier so the list` |
|      - |  4850 | ` * takes its own reference rather than the creation one.` |
|      - |  4851 | ` */` |
|    260 |  4852 | `static int ReflectMemberList(ph7_context *pCtx, int iKind, const char *zClass,` |
|      - |  4853 | `	int nArg, ph7_value **apArg)` |
|      5 |  4854 | `{` |
|    265 |  4855 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|    265 |  4856 | `	ph7_class_instance *pObj = ReflectClassObj(pCtx);` |
|    265 |  4857 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  4858 | `	ph7_value sSelf;` |
|      - |  4859 | `	SySet aMembers;` |
|      - |  4860 | `	sxu32 n;` |
|    265 |  4861 | `	int bFilter = (nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0);` |
|    265 |  4862 | `	sxi64 iFilter = bFilter ? ph7_value_to_int64(apArg[0]) : 0;` |
|    265 |  4863 | `	if( pOut == 0 ){` |
|    ! 0 |  4864 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  4865 | `	}` |
|    265 |  4866 | `	if( pClass == 0 ){` |
|    ! 0 |  4867 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  4868 | `		return PH7_OK;` |
|      - |  4869 | `	}` |
|    265 |  4870 | `	ReflectSelfName(pCtx, &sSelf);` |
|    265 |  4871 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|    265 |  4872 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|   3079 |  4873 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   2819 |  4874 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - |  4875 | `		ph7_value sName, sVal;` |
|      - |  4876 | `		ph7_value *apCtor[2];` |
|      - |  4877 | `		ph7_class_instance *pRef;` |
|      - |  4878 | `		sxi32 rc;` |
|   2819 |  4879 | `		if( pM->iKind != iKind ){` |
|   1968 |  4880 | `			continue;` |
|      - |  4881 | `		}` |
|    894 |  4882 | `		if( iKind == REFLECT_MEMBER_PROP && pObj != 0 && pM->pAttr != 0` |
|    142 |  4883 | `		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT) == 0` |
|    133 |  4884 | `		 && PH7_ATTR_LAZY_ABSENT(pM->pAttr,pObj) ){` |
|      - |  4885 | `			/* A ReflectionObject reflects the OBJECT, and a LAZY property php does` |
|      - |  4886 | `			 * not DECLARE (DateInterval's ten) is not on one that was never` |
|      - |  4887 | `			 * constructed -- php reports none there either. The declared-and-virtual` |
|      - |  4888 | `			 * kind (DatePeriod's seven, PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT) is` |
|      - |  4889 | `			 * reported whatever the object holds, because php declares it. */` |
|     45 |  4890 | `			continue;` |
|      - |  4891 | `		}` |
|    850 |  4892 | `		if( iKind == REFLECT_MEMBER_PROP && pObj == 0 && pM->pAttr != 0` |
|    237 |  4893 | `		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND) != 0 ){` |
|      - |  4894 | `			/* An ON-DEMAND property belongs to the objects that took it, so the` |
|      - |  4895 | `			 * CLASS's list -- which php builds from declarations and DateInterval` |
|      - |  4896 | `			 * has none of -- does not gain a name for it. */` |
|    ! 0 |  4897 | `			continue;` |
|      - |  4898 | `		}` |
|    850 |  4899 | `		if( iKind == REFLECT_MEMBER_PROP && pObj != 0 && pM->pAttr != 0` |
|     98 |  4900 | `		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY) != 0` |
|    101 |  4901 | `		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT) == 0 ){` |
|      - |  4902 | `			/* ...and for the same reason a property the object HIDES or never took` |
|      - |  4903 | `			 * is not on it either: php's from-string DateInterval reflects the two` |
|      - |  4904 | ``			 * names it presents, and an ordinary one has no `date_string` at all. */`` |
|    100 |  4905 | `			SyHashEntry *pOwn = SyHashGet(&pObj->hAttr,` |
|     66 |  4906 | `				SyStringData(&pM->pAttr->sName),SyStringLength(&pM->pAttr->sName));` |
|     66 |  4907 | `			if( pOwn == 0` |
|     65 |  4908 | `			 \|\| (((VmClassAttr *)pOwn->pUserData)->iState & VM_CLASS_ATTR_UNSEEN) ){` |
|     23 |  4909 | `				continue;` |
|      - |  4910 | `			}` |
|     22 |  4911 | `		}` |
|    833 |  4912 | `		if( bFilter ){` |
|     33 |  4913 | `			sxi64 iMods = iKind == REFLECT_MEMBER_METHOD ? ReflectMethodModifiers(pM->pMeth)` |
|     40 |  4914 | `				: (iKind == REFLECT_MEMBER_CONST ? ReflectConstModifiers(pM->pAttr)` |
|     20 |  4915 | `				                                 : ReflectPropModifiers(pM->pAttr));` |
|     33 |  4916 | `			if( (iMods & iFilter) == 0 ){` |
|     21 |  4917 | `				continue;` |
|      - |  4918 | `			}` |
|      6 |  4919 | `		}` |
|    813 |  4920 | `		PH7_MemObjInit(pCtx->pVm, &sName);` |
|    813 |  4921 | `		ph7_value_string(&sName, SyStringData(&pM->sKey), (int)SyStringLength(&pM->sKey));` |
|    813 |  4922 | `		apCtor[0] = &sSelf;` |
|    813 |  4923 | `		apCtor[1] = &sName;` |
|    813 |  4924 | `		pRef = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|    813 |  4925 | `		PH7_MemObjRelease(&sName);` |
|    813 |  4926 | `		if( pRef == 0 ){` |
|    ! 0 |  4927 | `			SySetRelease(&aMembers);` |
|    ! 0 |  4928 | `			PH7_MemObjRelease(&sSelf);` |
|    ! 0 |  4929 | `			if( rc != PH7_OK ){` |
|    ! 0 |  4930 | `				return rc;` |
|      - |  4931 | `			}` |
|    ! 0 |  4932 | `			ph7_result_value(pCtx, pOut);` |
|    ! 0 |  4933 | `			return PH7_OK;` |
|      - |  4934 | `		}` |
|    813 |  4935 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    813 |  4936 | `		sVal.x.pOther = pRef;` |
|    813 |  4937 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|    813 |  4938 | `		ph7_array_add_elem(pOut, 0, &sVal);   /* takes its own reference */` |
|    813 |  4939 | `		PH7_ClassInstanceUnref(pRef);` |
|    409 |  4940 | `	}` |
|    265 |  4941 | `	SySetRelease(&aMembers);` |
|      - |  4942 | `	/* A ReflectionObject also reports the instance's own DYNAMIC properties,` |
|      - |  4943 | `	 * which no class declaration knows about. */` |
|    265 |  4944 | `	if( iKind == REFLECT_MEMBER_PROP && pObj != 0 && (!bFilter \|\| (iFilter & 1)) ){` |
|      - |  4945 | `		SyHashEntry *pEntry;` |
|      - |  4946 | `		ph7_value sTarget;` |
|     17 |  4947 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     17 |  4948 | `		sTarget.x.pOther = pObj;` |
|     17 |  4949 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|     17 |  4950 | `		SyHashResetLoopCursor(&pObj->hAttr);` |
|    111 |  4951 | `		while( (pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){` |
|     95 |  4952 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - |  4953 | `			ph7_value sName, sVal;` |
|      - |  4954 | `			ph7_value *apCtor[2];` |
|      - |  4955 | `			ph7_class_instance *pRef;` |
|      - |  4956 | `			sxi32 rc;` |
|     95 |  4957 | `			if( pVmAttr->pAttr == 0 \|\| (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) == 0 ){` |
|     91 |  4958 | `				continue;` |
|      - |  4959 | `			}` |
|      5 |  4960 | `			PH7_MemObjInit(pCtx->pVm, &sName);` |
|      7 |  4961 | `			ph7_value_string(&sName, SyStringData(&pVmAttr->pAttr->sName),` |
|      4 |  4962 | `				(int)SyStringLength(&pVmAttr->pAttr->sName));` |
|      5 |  4963 | `			apCtor[0] = &sTarget;` |
|      5 |  4964 | `			apCtor[1] = &sName;` |
|      5 |  4965 | `			pRef = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|      5 |  4966 | `			PH7_MemObjRelease(&sName);` |
|      5 |  4967 | `			if( pRef == 0 ){` |
|    ! 0 |  4968 | `				break;` |
|      - |  4969 | `			}` |
|      5 |  4970 | `			PH7_MemObjInit(pCtx->pVm, &sVal);` |
|      5 |  4971 | `			sVal.x.pOther = pRef;` |
|      5 |  4972 | `			sVal.iFlags = MEMOBJ_OBJ;` |
|      5 |  4973 | `			ph7_array_add_elem(pOut, 0, &sVal);` |
|      5 |  4974 | `			PH7_ClassInstanceUnref(pRef);` |
|      1 |  4975 | `		}` |
|      - |  4976 | `		/* sTarget borrows pObj and never took a reference: nothing to release. */` |
|      8 |  4977 | `	}` |
|    265 |  4978 | `	PH7_MemObjRelease(&sSelf);` |
|    265 |  4979 | `	ph7_result_value(pCtx, pOut);` |
|    265 |  4980 | `	return PH7_OK;` |
|    135 |  4981 | `}` |
|    110 |  4982 | `static int vm_builtin_ReflectionClass_getMethods(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  4983 | `{` |
|    115 |  4984 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_METHOD, "ReflectionMethod", nArg, apArg);` |
|      5 |  4985 | `}` |
|    144 |  4986 | `static int vm_builtin_ReflectionClass_getProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  4987 | `{` |
|    149 |  4988 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_PROP, "ReflectionProperty", nArg, apArg);` |
|      5 |  4989 | `}` |
|      6 |  4990 | `static int vm_builtin_ReflectionClass_getReflectionConstants(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4991 | `{` |
|      7 |  4992 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_CONST, "ReflectionClassConstant", nArg, apArg);` |
|      1 |  4993 | `}` |
|     12 |  4994 | `static int vm_builtin_ReflectionClass_getProperty(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  4995 | `{` |
|     13 |  4996 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     13 |  4997 | `	ph7_class_instance *pObj = ReflectClassObj(pCtx);` |
|      - |  4998 | `	const char *zName;` |
|     13 |  4999 | `	int nName, bFound = 0;` |
|      - |  5000 | `	SySet aMembers;` |
|      - |  5001 | `	sxu32 n;` |
|      - |  5002 | `	SyString sFound;` |
|     13 |  5003 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  5004 | `		ph7_result_null(pCtx);` |
|    ! 0 |  5005 | `		return PH7_OK;` |
|      - |  5006 | `	}` |
|     13 |  5007 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     13 |  5008 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     13 |  5009 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     49 |  5010 | `	for( n = 0 ; n < SySetUsed(&aMembers) && !bFound ; n++ ){` |
|     37 |  5011 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     37 |  5012 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zName, nName) ){` |
|      7 |  5013 | `			sFound = pM->sKey;` |
|      7 |  5014 | `			bFound = 1;` |
|      3 |  5015 | `		}` |
|     19 |  5016 | `	}` |
|     13 |  5017 | `	SySetRelease(&aMembers);` |
|     13 |  5018 | `	if( bFound ){` |
|      - |  5019 | `		ph7_value sSelf;` |
|      - |  5020 | `		int rc;` |
|      7 |  5021 | `		ReflectSelfName(pCtx, &sSelf);` |
|      7 |  5022 | `		rc = ReflectResultMember(pCtx, "ReflectionProperty", &sSelf, &sFound);` |
|      7 |  5023 | `		PH7_MemObjRelease(&sSelf);` |
|      7 |  5024 | `		return rc;` |
|      - |  5025 | `	}` |
|      7 |  5026 | `	if( ReflectObjHasProp(pObj, zName, nName) ){` |
|      - |  5027 | `		/* A dynamic property: the reflector is built over the OBJECT, since the` |
|      - |  5028 | `		 * class declaration has no record of it. */` |
|      - |  5029 | `		ph7_value sTarget;` |
|      - |  5030 | `		SyString sName;` |
|      - |  5031 | `		int rc;` |
|      3 |  5032 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      3 |  5033 | `		sTarget.x.pOther = pObj;` |
|      3 |  5034 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|      3 |  5035 | `		SyStringInitFromBuf(&sName, zName, nName);` |
|      3 |  5036 | `		rc = ReflectResultMember(pCtx, "ReflectionProperty", &sTarget, &sName);` |
|      3 |  5037 | `		return rc;` |
|      - |  5038 | `	}` |
|      7 |  5039 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 |  5040 | `		"Property %z::$%.*s does not exist", &pClass->sName, nName, zName);` |
|      7 |  5041 | `}` |
|     10 |  5042 | `static int vm_builtin_ReflectionClass_getReflectionConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  5043 | `{` |
|     11 |  5044 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  5045 | `	const char *zName;` |
|     11 |  5046 | `	int nName, bFound = 0;` |
|      - |  5047 | `	SySet aMembers;` |
|      - |  5048 | `	sxu32 n;` |
|      - |  5049 | `	SyString sFound;` |
|     11 |  5050 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  5051 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  5052 | `		return PH7_OK;` |
|      - |  5053 | `	}` |
|     11 |  5054 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     11 |  5055 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     11 |  5056 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     33 |  5057 | `	for( n = 0 ; n < SySetUsed(&aMembers) && !bFound ; n++ ){` |
|     23 |  5058 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     23 |  5059 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|      9 |  5060 | `			sFound = pM->sKey;` |
|      9 |  5061 | `			bFound = 1;` |
|      4 |  5062 | `		}` |
|     12 |  5063 | `	}` |
|     11 |  5064 | `	SySetRelease(&aMembers);` |
|     11 |  5065 | `	if( !bFound ){` |
|      3 |  5066 | `		ph7_result_bool(pCtx, 0);` |
|      3 |  5067 | `		return PH7_OK;` |
|      - |  5068 | `	}` |
|      - |  5069 | `	{` |
|      - |  5070 | `		ph7_value sSelf;` |
|      - |  5071 | `		int rc;` |
|      9 |  5072 | `		ReflectSelfName(pCtx, &sSelf);` |
|      9 |  5073 | `		rc = ReflectResultMember(pCtx, "ReflectionClassConstant", &sSelf, &sFound);` |
|      9 |  5074 | `		PH7_MemObjRelease(&sSelf);` |
|      9 |  5075 | `		return rc;` |
|      - |  5076 | `	}` |
|      6 |  5077 | `}` |
|      - |  5078 | `/* ---- statics and defaults ---- */` |
|      - |  5079 | `/* Reading or writing a static through reflection materializes the class's` |
|      - |  5080 | `` * static table exactly as `C::$s` does, so a default that threw at the`` |
|      - |  5081 | ` * declaration raises HERE. */` |
|     52 |  5082 | `static sxi32 ReflectMaterializeStatics(ph7_context *pCtx, ph7_class *pClass)` |
|      2 |  5083 | `{` |
|     54 |  5084 | `	if( VmClassStaticDeferPending(pClass) ){` |
|     21 |  5085 | `		return PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);` |
|      - |  5086 | `	}` |
|     34 |  5087 | `	return SXRET_OK;` |
|     28 |  5088 | `}` |
|      8 |  5089 | `static int vm_builtin_ReflectionClass_getStaticProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  5090 | `{` |
|     10 |  5091 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     10 |  5092 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  5093 | `	SySet aMembers;` |
|      - |  5094 | `	sxu32 n;` |
|      4 |  5095 | `	SXUNUSED(nArg);` |
|      4 |  5096 | `	SXUNUSED(apArg);` |
|     10 |  5097 | `	if( pOut == 0 ){` |
|    ! 0 |  5098 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  5099 | `	}` |
|     10 |  5100 | `	if( pClass == 0 ){` |
|    ! 0 |  5101 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  5102 | `		return PH7_OK;` |
|      - |  5103 | `	}` |
|      - |  5104 | `	{` |
|     10 |  5105 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|     10 |  5106 | `		if( rc != SXRET_OK ){` |
|      3 |  5107 | `			return rc;` |
|      - |  5108 | `		}` |
|      - |  5109 | `	}` |
|      7 |  5110 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|      7 |  5111 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     39 |  5112 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     33 |  5113 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - |  5114 | `		ph7_value *pVal;` |
|      - |  5115 | `		SyHashEntry *pSlot;` |
|     32 |  5116 | `		if( pM->iKind != REFLECT_MEMBER_PROP` |
|     29 |  5117 | `		 \|\| (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|     21 |  5118 | `			continue;` |
|      - |  5119 | `		}` |
|      - |  5120 | `		/* An UNINITIALIZED typed static has no value to report, and php simply` |
|      - |  5121 | `		 * leaves it out rather than raising the read Error here. */` |
|     13 |  5122 | `		pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pM->pAttr->nIdx, sizeof(sxu32));` |
|     13 |  5123 | `		if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|      3 |  5124 | `			continue;` |
|      - |  5125 | `		}` |
|     11 |  5126 | `		pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pM->pAttr->nIdx);` |
|     11 |  5127 | `		if( pVal ){` |
|     11 |  5128 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, pVal);` |
|      5 |  5129 | `		}` |
|      6 |  5130 | `	}` |
|      7 |  5131 | `	SySetRelease(&aMembers);` |
|      7 |  5132 | `	ph7_result_value(pCtx, pOut);` |
|      7 |  5133 | `	return PH7_OK;` |
|      6 |  5134 | `}` |
|      - |  5135 | `/* The declared STATIC property of this name, or NULL. */` |
|     18 |  5136 | `static ph7_class_attr * ReflectStaticAttr(ph7_context *pCtx, ph7_class *pClass,` |
|      - |  5137 | `	const char *zName, int nName)` |
|      2 |  5138 | `{` |
|      - |  5139 | `	SyHashEntry *pEntry;` |
|      - |  5140 | `	ph7_class_attr *pAttr;` |
|     20 |  5141 | `	if( nName < 1 ){` |
|    ! 0 |  5142 | `		return 0;` |
|      - |  5143 | `	}` |
|     20 |  5144 | `	pEntry = SyHashGet(&pClass->hAttr, (const void *)zName, (sxu32)nName);` |
|     20 |  5145 | `	if( pEntry == 0 ){` |
|      9 |  5146 | `		return 0;` |
|      - |  5147 | `	}` |
|     12 |  5148 | `	pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      5 |  5149 | `	SXUNUSED(pCtx);` |
|     12 |  5150 | `	return (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? pAttr : 0;` |
|     11 |  5151 | `}` |
|     20 |  5152 | `static int vm_builtin_ReflectionClass_getStaticPropertyValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  5153 | `{` |
|     22 |  5154 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  5155 | `	ph7_class_attr *pAttr;` |
|      - |  5156 | `	const char *zName;` |
|      - |  5157 | `	int nName;` |
|     22 |  5158 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  5159 | `		ph7_result_null(pCtx);` |
|    ! 0 |  5160 | `		return PH7_OK;` |
|      - |  5161 | `	}` |
|      - |  5162 | `	{` |
|     22 |  5163 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|     22 |  5164 | `		if( rc != SXRET_OK ){` |
|      7 |  5165 | `			return rc;` |
|      - |  5166 | `		}` |
|      - |  5167 | `	}` |
|     16 |  5168 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     16 |  5169 | `	pAttr = ReflectStaticAttr(pCtx, pClass, zName, nName);` |
|     16 |  5170 | `	if( pAttr == 0 ){` |
|      7 |  5171 | `		if( nArg > 1 ){` |
|      5 |  5172 | `			ph7_result_value(pCtx, apArg[1]);` |
|      5 |  5173 | `			return PH7_OK;` |
|      - |  5174 | `		}` |
|      4 |  5175 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  5176 | `			"Property %z::$%.*s does not exist", &pClass->sName, nName, zName);` |
|      - |  5177 | `	}` |
|      - |  5178 | `	{` |
|      - |  5179 | `		/* Uninitialized typed static: the same Error the VM raises on read */` |
|     10 |  5180 | `		SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|      - |  5181 | `		ph7_value *pVal;` |
|     10 |  5182 | `		if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|      - |  5183 | `			/* php says "Typed property" HERE and "Typed static property" from` |
|      - |  5184 | ``			 * ReflectionProperty::getValue() and from `A::$s` itself — the`` |
|      - |  5185 | `			 * three wordings were checked against the oracle, they really do` |
|      - |  5186 | `			 * differ by call site. */` |
|      3 |  5187 | `			ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|      4 |  5188 | `			return PH7_VmThrowException(pCtx, "Error",` |
|      - |  5189 | `				"Typed property %z::$%z must not be accessed before initialization",` |
|      1 |  5190 | `				&pDecl->sName, &pAttr->sName);` |
|      - |  5191 | `		}` |
|      8 |  5192 | `		pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      8 |  5193 | `		if( pVal ){` |
|      8 |  5194 | `			ph7_result_value(pCtx, pVal);` |
|      5 |  5195 | `		}else{` |
|    ! 0 |  5196 | `			ph7_result_null(pCtx);` |
|      - |  5197 | `		}` |
|      - |  5198 | `	}` |
|      8 |  5199 | `	return PH7_OK;` |
|     12 |  5200 | `}` |
|      8 |  5201 | `static int vm_builtin_ReflectionClass_setStaticPropertyValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  5202 | `{` |
|     10 |  5203 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  5204 | `	ph7_class_attr *pAttr;` |
|      - |  5205 | `	ph7_value *pSlot;` |
|      - |  5206 | `	const char *zName;` |
|      - |  5207 | `	int nName;` |
|     10 |  5208 | `	if( pClass == 0 \|\| nArg < 2 ){` |
|    ! 0 |  5209 | `		return PH7_OK;` |
|      - |  5210 | `	}` |
|      - |  5211 | `	{` |
|     10 |  5212 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|     10 |  5213 | `		if( rc != SXRET_OK ){` |
|      5 |  5214 | `			return rc;` |
|      - |  5215 | `		}` |
|      - |  5216 | `	}` |
|      5 |  5217 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|      5 |  5218 | `	pAttr = ReflectStaticAttr(pCtx, pClass, zName, nName);` |
|      5 |  5219 | `	if( pAttr == 0 ){` |
|      4 |  5220 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  5221 | `			"Class %z does not have a property named %.*s", &pClass->sName, nName, zName);` |
|      - |  5222 | `	}` |
|      3 |  5223 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      3 |  5224 | `	if( pSlot == 0 ){` |
|    ! 0 |  5225 | `		return PH7_OK;` |
|      - |  5226 | `	}` |
|      - |  5227 | `	{` |
|      3 |  5228 | `		sxi32 rc = ReflectEnforceStore(pCtx, pAttr->nIdx, apArg[1]);` |
|      3 |  5229 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  5230 | `			return rc;` |
|      - |  5231 | `		}` |
|      - |  5232 | `	}` |
|      3 |  5233 | `	PH7_MemObjStore(apArg[1], pSlot);` |
|      3 |  5234 | `	return PH7_OK;` |
|      6 |  5235 | `}` |
|      4 |  5236 | `static int vm_builtin_ReflectionClass_getDefaultProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  5237 | `{` |
|      5 |  5238 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      5 |  5239 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  5240 | `	SySet aMembers;` |
|      - |  5241 | `	sxu32 n;` |
|      - |  5242 | `	int iPass;` |
|      2 |  5243 | `	SXUNUSED(nArg);` |
|      2 |  5244 | `	SXUNUSED(apArg);` |
|      5 |  5245 | `	if( pOut == 0 ){` |
|    ! 0 |  5246 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  5247 | `	}` |
|      5 |  5248 | `	if( pClass == 0 ){` |
|    ! 0 |  5249 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  5250 | `		return PH7_OK;` |
|      - |  5251 | `	}` |
|      5 |  5252 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|      5 |  5253 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|      - |  5254 | `	/* php reports the STATIC properties first, then the instance ones. */` |
|     13 |  5255 | `	for( iPass = 0 ; iPass < 2 ; iPass++ ){` |
|     57 |  5256 | `		for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     49 |  5257 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - |  5258 | `			int bStatic;` |
|      - |  5259 | `			ph7_value sValue;` |
|     49 |  5260 | `			if( pM->iKind != REFLECT_MEMBER_PROP ){` |
|     18 |  5261 | `				continue;` |
|      - |  5262 | `			}` |
|     45 |  5263 | `			bStatic = (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|     45 |  5264 | `			if( bStatic != (iPass == 0) ){` |
|     23 |  5265 | `				continue;` |
|      - |  5266 | `			}` |
|     22 |  5267 | `			if( (pM->pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|     16 |  5268 | `			 && SySetUsed(&pM->pAttr->aByteCode) < 1 ){` |
|      - |  5269 | `				/* A TYPED property with no initializer has no default at all —` |
|      - |  5270 | `				 * it is uninitialized, and php leaves it out. An UNTYPED one` |
|      - |  5271 | `				 * without an initializer defaults to null and is listed. */` |
|      5 |  5272 | `				continue;` |
|      - |  5273 | `			}` |
|     19 |  5274 | `			PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     19 |  5275 | `			if( SySetUsed(&pM->pAttr->aByteCode) > 0 ){` |
|      - |  5276 | `				/* Same evaluation path the VM uses for omitted call arguments */` |
|     15 |  5277 | `				VmLocalExec(pCtx->pVm, &pM->pAttr->aByteCode, &sValue, FALSE);` |
|      7 |  5278 | `			}` |
|     19 |  5279 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, &sValue);` |
|     19 |  5280 | `			PH7_MemObjRelease(&sValue);` |
|     10 |  5281 | `		}` |
|      5 |  5282 | `	}` |
|      5 |  5283 | `	SySetRelease(&aMembers);` |
|      5 |  5284 | `	ph7_result_value(pCtx, pOut);` |
|      5 |  5285 | `	return PH7_OK;` |
|      3 |  5286 | `}` |
|      - |  5287 | `/* ---- attributes, extension, export ---- */` |
|     82 |  5288 | `static int vm_builtin_ReflectionClass_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  5289 | `{` |
|     86 |  5290 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  5291 | `	const char *zName;` |
|      - |  5292 | `	int nName;` |
|     86 |  5293 | `	if( pClass == 0 ){` |
|    ! 0 |  5294 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  5295 | `		return PH7_OK;` |
|      - |  5296 | `	}` |
|     86 |  5297 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      - |  5298 | `	{` |
|      - |  5299 | `		ph7_value sTarget;` |
|      - |  5300 | `		int rc;` |
|     86 |  5301 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     86 |  5302 | `		ph7_value_string(&sTarget, zName, nName);` |
|      - |  5303 | `		/* 1 = Attribute::TARGET_CLASS */` |
|    127 |  5304 | `		rc = ReflectBuildAttrs(pCtx, &pClass->aAttrs, "class", &sTarget, 0, 0, 0, 1,` |
|     41 |  5305 | `			nArg, apArg);` |
|     86 |  5306 | `		PH7_MemObjRelease(&sTarget);` |
|     86 |  5307 | `		return rc;` |
|      - |  5308 | `	}` |
|     45 |  5309 | `}` |
|      - |  5310 | `/*` |
|      - |  5311 | ` * The ReflectionExtension for one extension id, which is what every reflector's` |
|      - |  5312 | ` * getExtension() answers for an INTERNAL target. A target the partition has no` |
|      - |  5313 | ` * row for (iExt < 0) belongs to no extension, which is php's null.` |
|      - |  5314 | ` */` |
|     20 |  5315 | `static int ReflectExtensionOf(ph7_context *pCtx, int iExt)` |
|      1 |  5316 | `{` |
|      - |  5317 | `	ph7_value sName;` |
|      - |  5318 | `	ph7_value *apCtor[1];` |
|      - |  5319 | `	ph7_class_instance *pExt;` |
|      - |  5320 | `	const char *zName;` |
|      - |  5321 | `	sxi32 rc;` |
|     21 |  5322 | `	if( iExt < 0 ){` |
|      7 |  5323 | `		ph7_result_null(pCtx);` |
|      7 |  5324 | `		return PH7_OK;` |
|      - |  5325 | `	}` |
|     15 |  5326 | `	zName = PH7_VmExtensionName(iExt);` |
|     15 |  5327 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|     15 |  5328 | `	ph7_value_string(&sName, zName, -1);` |
|     15 |  5329 | `	apCtor[0] = &sName;` |
|     15 |  5330 | `	pExt = ReflectConstruct(pCtx, "ReflectionExtension", 1, apCtor, &rc);` |
|     15 |  5331 | `	PH7_MemObjRelease(&sName);` |
|     15 |  5332 | `	if( pExt == 0 ){` |
|    ! 0 |  5333 | `		if( rc != PH7_OK ){` |
|    ! 0 |  5334 | `			return rc;` |
|      - |  5335 | `		}` |
|    ! 0 |  5336 | `		ph7_result_null(pCtx);` |
|    ! 0 |  5337 | `		return PH7_OK;` |
|      - |  5338 | `	}` |
|     15 |  5339 | `	return ReflectResultObject(pCtx, pExt);` |
|     11 |  5340 | `}` |
|      - |  5341 | `/*` |
|      - |  5342 | ` * The extension a reflected CLASS belongs to, or -1 for a userland one. A class` |
|      - |  5343 | ` * php has no row for keeps php's answer for an internal name it cannot place --` |
|      - |  5344 | ` * Core, the engine's own.` |
|      - |  5345 | ` */` |
|   4286 |  5346 | `static int ReflectClassExtId(ph7_class *pClass)` |
|      3 |  5347 | `{` |
|   4289 |  5348 | `	if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_INTERNAL) == 0 ){` |
|     35 |  5349 | `		return -1;` |
|      - |  5350 | `	}` |
|   6381 |  5351 | `	return PH7_VmExtOfClass(SyStringData(&pClass->sName),` |
|   4252 |  5352 | `		(int)SyStringLength(&pClass->sName));` |
|   2146 |  5353 | `}` |
|     32 |  5354 | `static int vm_builtin_ReflectionClass_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  5355 | `{` |
|     35 |  5356 | `	int iExt = ReflectClassExtId(ReflectClassOf(pCtx));` |
|     16 |  5357 | `	SXUNUSED(nArg);` |
|     16 |  5358 | `	SXUNUSED(apArg);` |
|     35 |  5359 | `	if( iExt < 0 ){` |
|      3 |  5360 | `		ph7_result_bool(pCtx, 0);` |
|      2 |  5361 | `	}else{` |
|     33 |  5362 | `		ph7_result_string(pCtx, PH7_VmExtensionName(iExt), -1);` |
|      - |  5363 | `	}` |
|     35 |  5364 | `	return PH7_OK;` |
|      3 |  5365 | `}` |
|      - |  5366 | `/* php's export format (chunk 9) — defined at the END of this file, where the` |
|      - |  5367 | ` * member walk, the function reference and the parameter description it reads` |
|      - |  5368 | ` * are all in scope. */` |
|      - |  5369 | `static int ReflectExportClassSelf(ph7_context *pCtx);` |
|      - |  5370 | `static int ReflectExportFuncSelf(ph7_context *pCtx);` |
|      - |  5371 | `static int ReflectExportParamSelf(ph7_context *pCtx);` |
|      - |  5372 | `static int ReflectExportPropSelf(ph7_context *pCtx);` |
|      - |  5373 | `static int ReflectExportConstSelf(ph7_context *pCtx);` |
|      - |  5374 | `/*` |
|      - |  5375 | ` * __toString(): php's export format, still chunk 9 — a PHP function written` |
|      - |  5376 | `` * against the PUBLIC reflection API of its target, so it is called with `$this`.`` |
|      - |  5377 | ` * bIndentArg adds the export family's second "" indent argument.` |
|      - |  5378 | ` */` |
|      4 |  5379 | `static int vm_builtin_ReflectionClass_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  5380 | `{` |
|      2 |  5381 | `	SXUNUSED(nArg);` |
|      2 |  5382 | `	SXUNUSED(apArg);` |
|      5 |  5383 | `	return ReflectExtensionOf(pCtx, ReflectClassExtId(ReflectClassOf(pCtx)));` |
|      1 |  5384 | `}` |
|     54 |  5385 | `static int vm_builtin_ReflectionClass_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  5386 | `{` |
|     27 |  5387 | `	SXUNUSED(nArg);` |
|     27 |  5388 | `	SXUNUSED(apArg);` |
|     56 |  5389 | `	return ReflectExportClassSelf(pCtx);` |
|      2 |  5390 | `}` |
|      - |  5391 | `/* ---- lazy objects: PHL has none (§7.4) ---- */` |
|    ! 0 |  5392 | `static int ReflectNoLazy(ph7_context *pCtx, const char *zWho)` |
|    ! 0 |  5393 | `{` |
|    ! 0 |  5394 | `	return PH7_VmThrowException(pCtx, "Error",` |
|    ! 0 |  5395 | `		"%s is not supported by PHL (no lazy objects)", zWho);` |
|    ! 0 |  5396 | `}` |
|      - |  5397 | `#define REFLECT_NO_LAZY(NAME,TEXT) \` |
|      - |  5398 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  5399 | `	{ \` |
|      - |  5400 | `		SXUNUSED(nArg); \` |
|      - |  5401 | `		SXUNUSED(apArg); \` |
|      - |  5402 | `		return ReflectNoLazy(pCtx, TEXT); \` |
|      - |  5403 | `	}` |
|    ! 0 |  5404 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_newLazyGhost,"ReflectionClass::newLazyGhost()")` |
|    ! 0 |  5405 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_newLazyProxy,"ReflectionClass::newLazyProxy()")` |
|    ! 0 |  5406 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_resetAsLazyGhost,"ReflectionClass::resetAsLazyGhost()")` |
|    ! 0 |  5407 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_resetAsLazyProxy,"ReflectionClass::resetAsLazyProxy()")` |
|      - |  5408 | `/* The four QUERIES answer what is true of a VM with no lazy objects, rather` |
|      - |  5409 | ` * than refusing: an object here is always initialized and never has one. */` |
|    ! 0 |  5410 | `static int vm_builtin_ReflectionClass_getLazyInitializer(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  5411 | `{` |
|    ! 0 |  5412 | `	SXUNUSED(nArg);` |
|    ! 0 |  5413 | `	SXUNUSED(apArg);` |
|    ! 0 |  5414 | `	ph7_result_null(pCtx);` |
|    ! 0 |  5415 | `	return PH7_OK;` |
|    ! 0 |  5416 | `}` |
|    ! 0 |  5417 | `static int vm_builtin_ReflectionClass_passThroughObject(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  5418 | `{` |
|    ! 0 |  5419 | `	if( nArg > 0 ){` |
|    ! 0 |  5420 | `		ph7_result_value(pCtx, apArg[0]);` |
|    ! 0 |  5421 | `	}else{` |
|    ! 0 |  5422 | `		ph7_result_null(pCtx);` |
|      - |  5423 | `	}` |
|    ! 0 |  5424 | `	return PH7_OK;` |
|    ! 0 |  5425 | `}` |
|    ! 0 |  5426 | `static int vm_builtin_ReflectionClass_isUninitializedLazyObject(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  5427 | `{` |
|    ! 0 |  5428 | `	SXUNUSED(nArg);` |
|    ! 0 |  5429 | `	SXUNUSED(apArg);` |
|    ! 0 |  5430 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 |  5431 | `	return PH7_OK;` |
|    ! 0 |  5432 | `}` |
|      - |  5433 | `/*` |
|      - |  5434 | ` * Reflection::getModifierNames(int $modifiers)` |
|      - |  5435 | ` *` |
|      - |  5436 | ` * php's own order and its own SWITCH: the visibility names come out of one` |
|      - |  5437 | ` * three-way choice and the set-visibility names out of another, so a mask with` |
|      - |  5438 | ` * two visibility bits set names NEITHER (which is what php answers).` |
|      - |  5439 | ` */` |
|     96 |  5440 | `static int vm_builtin_Reflection_getModifierNames(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  5441 | `{` |
|     98 |  5442 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     98 |  5443 | `	sxi64 iMods = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|      - |  5444 | `	const char *azName[8];` |
|     98 |  5445 | `	int nName = 0, n;` |
|     98 |  5446 | `	if( pOut == 0 ){` |
|    ! 0 |  5447 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  5448 | `	}` |
|     98 |  5449 | `	if( iMods & 64 ){ azName[nName++] = "abstract"; }` |
|     98 |  5450 | `	if( iMods & 32 ){ azName[nName++] = "final"; }` |
|     98 |  5451 | `	if( iMods & 512 ){ azName[nName++] = "virtual"; }` |
|     98 |  5452 | `	switch( iMods & (1\|2\|4) ){` |
|     56 |  5453 | `	case 1: azName[nName++] = "public"; break;` |
|     21 |  5454 | `	case 2: azName[nName++] = "protected"; break;` |
|     15 |  5455 | `	case 4: azName[nName++] = "private"; break;` |
|      8 |  5456 | `	default: break;` |
|      - |  5457 | `	}` |
|     98 |  5458 | `	switch( iMods & (2048\|4096) ){` |
|     18 |  5459 | `	case 2048: azName[nName++] = "protected(set)"; break;` |
|      9 |  5460 | `	case 4096: azName[nName++] = "private(set)"; break;` |
|     72 |  5461 | `	default: break;` |
|      - |  5462 | `	}` |
|     98 |  5463 | `	if( iMods & 16 ){ azName[nName++] = "static"; }` |
|     98 |  5464 | `	if( iMods & 128 ){ azName[nName++] = "readonly"; }` |
|    266 |  5465 | `	for( n = 0 ; n < nName ; n++ ){` |
|    170 |  5466 | `		ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|    170 |  5467 | `		if( pName == 0 ){ break; }` |
|    170 |  5468 | `		ph7_value_string(pName, azName[n], -1);` |
|    170 |  5469 | `		ph7_array_add_elem(pOut, 0, pName);` |
|     86 |  5470 | `	}` |
|     98 |  5471 | `	ph7_result_value(pCtx, pOut);` |
|     98 |  5472 | `	return PH7_OK;` |
|     50 |  5473 | `}` |
|      - |  5474 | `/*` |
|      - |  5475 | ` * Declare chunk 1. Called from PH7_VmInstallReflection() where it used to be` |
|      - |  5476 | `` * compiled — before chunks 2 and 3, which name `Reflector` in their own`` |
|      - |  5477 | `` * `implements` clauses.`` |
|      - |  5478 | ` *` |
|      - |  5479 | ` * The method table is in php's own DECLARATION order, which is the order` |
|      - |  5480 | ` * ReflectionClass::getMethods() reports for these classes themselves.` |
|      - |  5481 | ` */` |
|   6721 |  5482 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionClass(ph7_vm *pVm)` |
|      5 |  5483 | `{` |
|      - |  5484 | `	static const PH7_NativePropDef aClassProp[] = {` |
|      - |  5485 | `		{ "name",  PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  5486 | `		/* PHL-only: the instance a ReflectionObject was built over. php keeps it` |
|      - |  5487 | `		 * out of sight; PHL has no hidden-slot bit yet (§7.4 (e)). */` |
|      - |  5488 | `		{ RC_OBJ,  PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  5489 | `	};` |
|      - |  5490 | `	static const PH7_NativeMethodDef aClassMethod[] = {` |
|      - |  5491 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionClass_clone },` |
|      - |  5492 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $objectOrClass", "",` |
|      - |  5493 | `		  vm_builtin_ReflectionClass_construct },` |
|      - |  5494 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionClass_toString },` |
|      - |  5495 | `		{ "getName",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getName },` |
|      - |  5496 | `		{ "isInternal",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInternal },` |
|      - |  5497 | `		{ "isUserDefined", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isUserDefined },` |
|      - |  5498 | `		{ "isAnonymous",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isAnonymous },` |
|      - |  5499 | `		{ "isInstantiable",PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInstantiable },` |
|      - |  5500 | `		{ "isCloneable",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isCloneable },` |
|      - |  5501 | `		{ "getFileName",   PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getFileName },` |
|      - |  5502 | `		{ "getStartLine",  PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionClass_getStartLine },` |
|      - |  5503 | `		{ "getEndLine",    PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionClass_getEndLine },` |
|      - |  5504 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getDocComment },` |
|      - |  5505 | `		{ "getConstructor",PH7_MOD_PUBLIC, "", "@?ReflectionMethod", vm_builtin_ReflectionClass_getConstructor },` |
|      - |  5506 | `		{ "hasMethod",     PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasMethod },` |
|      - |  5507 | `		{ "getMethod",     PH7_MOD_PUBLIC, "string $name", "@ReflectionMethod", vm_builtin_ReflectionClass_getMethod },` |
|      - |  5508 | `		{ "getMethods",    PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - |  5509 | `		  vm_builtin_ReflectionClass_getMethods },` |
|      - |  5510 | `		{ "hasProperty",   PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasProperty },` |
|      - |  5511 | `		{ "getProperty",   PH7_MOD_PUBLIC, "string $name", "@ReflectionProperty", vm_builtin_ReflectionClass_getProperty },` |
|      - |  5512 | `		{ "getProperties", PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - |  5513 | `		  vm_builtin_ReflectionClass_getProperties },` |
|      - |  5514 | `		{ "hasConstant",   PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasConstant },` |
|      - |  5515 | `		{ "getConstants",  PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - |  5516 | `		  vm_builtin_ReflectionClass_getConstants },` |
|      - |  5517 | `		{ "getReflectionConstants", PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - |  5518 | `		  vm_builtin_ReflectionClass_getReflectionConstants },` |
|      - |  5519 | `		{ "getConstant",   PH7_MOD_PUBLIC, "string $name", "@mixed", vm_builtin_ReflectionClass_getConstant },` |
|      - |  5520 | `		{ "getReflectionConstant", PH7_MOD_PUBLIC, "string $name", "@ReflectionClassConstant\|false",` |
|      - |  5521 | `		  vm_builtin_ReflectionClass_getReflectionConstant },` |
|      - |  5522 | `		{ "getInterfaces",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getInterfaces },` |
|      - |  5523 | `		{ "getInterfaceNames", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getInterfaceNames },` |
|      - |  5524 | `		{ "isInterface",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInterface },` |
|      - |  5525 | `		{ "getTraits",         PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraits },` |
|      - |  5526 | `		{ "getTraitNames",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraitNames },` |
|      - |  5527 | `		{ "getTraitAliases",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraitAliases },` |
|      - |  5528 | `		{ "isTrait",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isTrait },` |
|      - |  5529 | `		{ "isEnum",            PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClass_isEnum },` |
|      - |  5530 | `		{ "isAbstract",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isAbstract },` |
|      - |  5531 | `		{ "isFinal",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isFinal },` |
|      - |  5532 | `		{ "isReadOnly",        PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClass_isReadOnly },` |
|      - |  5533 | `		{ "getModifiers",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionClass_getModifiers },` |
|      - |  5534 | `		{ "isInstance",        PH7_MOD_PUBLIC, "object $object", "@bool",` |
|      - |  5535 | `		  vm_builtin_ReflectionClass_isInstance },` |
|      - |  5536 | `		{ "newInstance",       PH7_MOD_PUBLIC, "mixed ...$args", "@object",` |
|      - |  5537 | `		  vm_builtin_ReflectionClass_newInstance },` |
|      - |  5538 | `		{ "newInstanceWithoutConstructor", PH7_MOD_PUBLIC, "", "@object",` |
|      - |  5539 | `		  vm_builtin_ReflectionClass_newInstanceWithoutConstructor },` |
|      - |  5540 | `		{ "newInstanceArgs",   PH7_MOD_PUBLIC, "array $args = []", "@?object",` |
|      - |  5541 | `		  vm_builtin_ReflectionClass_newInstanceArgs },` |
|      - |  5542 | `		{ "newLazyGhost",      PH7_MOD_PUBLIC, "callable $initializer, int $options = 0", "object",` |
|      - |  5543 | `		  vm_builtin_ReflectionClass_newLazyGhost },` |
|      - |  5544 | `		{ "newLazyProxy",      PH7_MOD_PUBLIC, "callable $factory, int $options = 0", "object",` |
|      - |  5545 | `		  vm_builtin_ReflectionClass_newLazyProxy },` |
|      - |  5546 | `		{ "resetAsLazyGhost",  PH7_MOD_PUBLIC,` |
|      - |  5547 | `		  "object $object, callable $initializer, int $options = 0", "void",` |
|      - |  5548 | `		  vm_builtin_ReflectionClass_resetAsLazyGhost },` |
|      - |  5549 | `		{ "resetAsLazyProxy",  PH7_MOD_PUBLIC,` |
|      - |  5550 | `		  "object $object, callable $factory, int $options = 0", "void",` |
|      - |  5551 | `		  vm_builtin_ReflectionClass_resetAsLazyProxy },` |
|      - |  5552 | `		{ "initializeLazyObject", PH7_MOD_PUBLIC, "object $object", "object",` |
|      - |  5553 | `		  vm_builtin_ReflectionClass_passThroughObject },` |
|      - |  5554 | `		{ "isUninitializedLazyObject", PH7_MOD_PUBLIC, "object $object", "bool",` |
|      - |  5555 | `		  vm_builtin_ReflectionClass_isUninitializedLazyObject },` |
|      - |  5556 | `		{ "markLazyObjectAsInitialized", PH7_MOD_PUBLIC, "object $object", "object",` |
|      - |  5557 | `		  vm_builtin_ReflectionClass_passThroughObject },` |
|      - |  5558 | `		{ "getLazyInitializer", PH7_MOD_PUBLIC, "object $object", "?callable",` |
|      - |  5559 | `		  vm_builtin_ReflectionClass_getLazyInitializer },` |
|      - |  5560 | `		{ "getParentClass",    PH7_MOD_PUBLIC, "", "@ReflectionClass\|false", vm_builtin_ReflectionClass_getParentClass },` |
|      - |  5561 | `		{ "isSubclassOf",      PH7_MOD_PUBLIC, "ReflectionClass\|string $class", "@bool",` |
|      - |  5562 | `		  vm_builtin_ReflectionClass_isSubclassOf },` |
|      - |  5563 | `		{ "getStaticProperties", PH7_MOD_PUBLIC, "", "@array",` |
|      - |  5564 | `		  vm_builtin_ReflectionClass_getStaticProperties },` |
|      - |  5565 | ``		/* `mixed $default = ?` is the table's "optional, no default VALUE"`` |
|      - |  5566 | `		 * marker — php's own shape here: isOptional() true,` |
|      - |  5567 | `		 * isDefaultValueAvailable() false. */` |
|      - |  5568 | `		{ "getStaticPropertyValue", PH7_MOD_PUBLIC, "string $name, mixed $default = ?", "@mixed",` |
|      - |  5569 | `		  vm_builtin_ReflectionClass_getStaticPropertyValue },` |
|      - |  5570 | `		{ "setStaticPropertyValue", PH7_MOD_PUBLIC, "string $name, mixed $value", "@void",` |
|      - |  5571 | `		  vm_builtin_ReflectionClass_setStaticPropertyValue },` |
|      - |  5572 | `		{ "getDefaultProperties", PH7_MOD_PUBLIC, "", "@array",` |
|      - |  5573 | `		  vm_builtin_ReflectionClass_getDefaultProperties },` |
|      - |  5574 | `		{ "isIterable",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isIterable },` |
|      - |  5575 | `		{ "isIterateable",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isIterable },` |
|      - |  5576 | `		{ "implementsInterface", PH7_MOD_PUBLIC, "ReflectionClass\|string $interface", "@bool",` |
|      - |  5577 | `		  vm_builtin_ReflectionClass_implementsInterface },` |
|      - |  5578 | `		{ "getExtension",      PH7_MOD_PUBLIC, "", "@?ReflectionExtension", vm_builtin_ReflectionClass_getExtension },` |
|      - |  5579 | `		{ "getExtensionName",  PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getExtensionName },` |
|      - |  5580 | `		{ "inNamespace",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_inNamespace },` |
|      - |  5581 | `		{ "getNamespaceName",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getNamespaceName },` |
|      - |  5582 | `		{ "getShortName",      PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getShortName },` |
|      - |  5583 | `		{ "getAttributes",     PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - |  5584 | `		  vm_builtin_ReflectionClass_getAttributes },` |
|      - |  5585 | `	};` |
|      - |  5586 | `	static const PH7_NativeConstDef aClassConst[] = {` |
|      - |  5587 | `		{ "IS_IMPLICIT_ABSTRACT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,    0, 0.0 },` |
|      - |  5588 | `		{ "IS_EXPLICIT_ABSTRACT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64,    0, 0.0 },` |
|      - |  5589 | `		{ "IS_FINAL",             PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32,    0, 0.0 },` |
|      - |  5590 | `		{ "IS_READONLY",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 65536, 0, 0.0 },` |
|      - |  5591 | `		{ "SKIP_INITIALIZATION_ON_SERIALIZE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 8, 0, 0.0 },` |
|      - |  5592 | `		{ "SKIP_DESTRUCTOR",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,    0, 0.0 },` |
|      - |  5593 | `	};` |
|      - |  5594 | `	static const PH7_NativeMethodDef aObjectMethod[] = {` |
|      - |  5595 | `		{ "__construct", PH7_MOD_PUBLIC, "object $object", "",` |
|      - |  5596 | `		  vm_builtin_ReflectionObject_construct },` |
|      - |  5597 | `	};` |
|      - |  5598 | `	static const PH7_NativeMethodDef aReflectionMethod[] = {` |
|      - |  5599 | `		{ "getModifierNames", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "int $modifiers", "@array",` |
|      - |  5600 | `		  vm_builtin_Reflection_getModifierNames },` |
|      - |  5601 | `	};` |
|      - |  5602 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  5603 | `		{ "Reflector", "Stringable", 0, PH7_CLASS_INTERFACE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  5604 | `		{ "ReflectionException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  5605 | `		{ "Reflection", 0, 0, 0,` |
|      - |  5606 | `		  aReflectionMethod, SX_ARRAYSIZE(aReflectionMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  5607 | ``		/* Uncloneable and unserializable in php too: `clone` is an Error and`` |
|      - |  5608 | `		 * serialize() a catchable Exception naming the class. */` |
|      - |  5609 | `		{ "ReflectionClass", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  5610 | `		  aClassMethod, SX_ARRAYSIZE(aClassMethod),` |
|      - |  5611 | `		  aClassConst, SX_ARRAYSIZE(aClassConst),` |
|      - |  5612 | `		  aClassProp, SX_ARRAYSIZE(aClassProp), 0, 0, 0 },` |
|      - |  5613 | `		{ "ReflectionObject", "ReflectionClass", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  5614 | `		  aObjectMethod, SX_ARRAYSIZE(aObjectMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  5615 | `	};` |
|   6726 |  5616 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  5617 | `}` |
|      - |  5618 | `/*` |
|      - |  5619 | ` * ---------------------------------------------------------------------------` |
|      - |  5620 | ` * ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,` |
|      - |  5621 | ` * ReflectionParameter.` |
|      - |  5622 | ` *` |
|      - |  5623 | ` * Chunk 2. Its accessors funnelled through __reflect_func_info(), a descriptor` |
|      - |  5624 | ` * ARRAY built per call from either a compiled ph7_vm_func or a declared` |
|      - |  5625 | ` * SIGNATURE STRING; a native method reads whichever of the two the target` |
|      - |  5626 | ` * actually has, through the one uniform description both already agreed on` |
|      - |  5627 | ` * (ReflectParamDesc / ReflectSigDescribe).` |
|      - |  5628 | ` *` |
|      - |  5629 | ` * Five thunks retire with the chunk: __reflect_func_info, __reflect_invoke,` |
|      - |  5630 | ` * __reflect_closure, __reflect_param_default and __reflect_param_defconst.` |
|      - |  5631 | ` * ---------------------------------------------------------------------------` |
|      - |  5632 | ` */` |
|      - |  5633 | `#define RF_CL "__cl"     /* ReflectionFunctionAbstract: the reflected Closure  */` |
|      - |  5634 | `/*` |
|      - |  5635 | ` * ReflectionMethod: the class the reflector was BUILT FOR, which is not the` |
|      - |  5636 | `` * one `$class` reports. php keeps both — `intern->ce` is the class the`` |
|      - |  5637 | ` * constructor (or the ReflectionClass that handed the method out) named, while` |
|      - |  5638 | `` * the public `$class` is the method's DECLARING class — and its export tags are`` |
|      - |  5639 | ` * relative to the first: a method whose declaring class is not the reflector's` |
|      - |  5640 | `` * own class prints `inherits <declaring>`. PHL had only the declaring one, so`` |
|      - |  5641 | ` * every directly-built reflector lost the tag.` |
|      - |  5642 | ` */` |
|      - |  5643 | `#define RM_CE "__ce"` |
|      - |  5644 | `#define RP_T  "__t"      /* ReflectionParameter: the function/class it belongs to */` |
|      - |  5645 | `#define RP_M  "__m"      /* ReflectionParameter: the method name, or null      */` |
|      - |  5646 | `#define RP_P  "__p"      /* ReflectionParameter: the position                  */` |
|      - |  5647 |  |
|      - |  5648 | `/* Everything a reflected function IS, resolved once per call. */` |
|      - |  5649 | `typedef struct ReflectFuncRef ReflectFuncRef;` |
|      - |  5650 | `struct ReflectFuncRef` |
|      - |  5651 | `{` |
|      - |  5652 | `	ph7_vm *pVm;                  /* the VM, for the declared-type text rewrite */` |
|      - |  5653 | `	ph7_vm_func *pFunc;           /* compiled body (NULL for a pure C builtin) */` |
|      - |  5654 | `	ph7_user_func *pHost;         /* C builtin (NULL otherwise) */` |
|      - |  5655 | `	ph7_class *pClass;            /* class a METHOD was reached through */` |
|      - |  5656 | `	ph7_class_method *pMeth;      /* the method */` |
|      - |  5657 | `	ph7_class_instance *pClosure; /* the Closure being reflected, if any */` |
|      - |  5658 | `	const char *zSig;             /* declared parameter signature, or NULL */` |
|      - |  5659 | `	const char *zRet;             /* declared return type, or NULL */` |
|      - |  5660 | `};` |
|      - |  5661 | `/*` |
|      - |  5662 | ` * Resolve a callable into the reference, and work out WHICH of the two` |
|      - |  5663 | ` * parameter sources describes it: a declared signature string (a C builtin, a` |
|      - |  5664 | ` * native method, or an embedded-PHP builtin declared argless over` |
|      - |  5665 | ` * func_get_args()) wins over the compiled argument list, exactly as` |
|      - |  5666 | ` * ReflectSigFixup made it win in the descriptor.` |
|      - |  5667 | ` */` |
|   7730 |  5668 | `static int ReflectFuncFill(ph7_vm *pVm, ph7_value *pTarget, ph7_value *pMethodArg,` |
|      - |  5669 | `	ReflectFuncRef *pOut)` |
|      5 |  5670 | `{` |
|   7735 |  5671 | `	SyZero(pOut, sizeof(*pOut));` |
|   7735 |  5672 | `	pOut->pVm = pVm;` |
|  11596 |  5673 | `	pOut->pFunc = ReflectResolveCallable(pVm, pTarget, pMethodArg,` |
|   3861 |  5674 | `		&pOut->pClass, &pOut->pMeth, &pOut->pHost, &pOut->pClosure);` |
|   7735 |  5675 | `	if( pOut->pFunc == 0 && pOut->pHost == 0 ){` |
|     41 |  5676 | `		return 0;` |
|      - |  5677 | `	}` |
|   7697 |  5678 | `	if( pOut->pFunc == 0 ){` |
|   3089 |  5679 | `		pOut->zSig = pOut->pHost->zSig;` |
|   3089 |  5680 | `		pOut->zRet = pOut->pHost->zRet;` |
|   3089 |  5681 | `		return 1;` |
|      - |  5682 | `	}` |
|   4613 |  5683 | `	if( (pOut->pFunc->iFlags & VM_FUNC_NATIVE) && pOut->pFunc->pNative ){` |
|   2939 |  5684 | `		pOut->zSig = pOut->pFunc->pNative->zSig;` |
|   2939 |  5685 | `		if( SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|   2939 |  5686 | `			pOut->zRet = pOut->pFunc->pNative->zRet;` |
|   1467 |  5687 | `		}` |
|   3146 |  5688 | `	}else if( (pOut->pFunc->iFlags & VM_FUNC_INTERNAL)` |
|    965 |  5689 | `	 && SySetUsed(&pOut->pFunc->aArgs) == 0 && pOut->pMeth == 0 ){` |
|    ! 0 |  5690 | `		const char *zRet = 0;` |
|    ! 0 |  5691 | `		pOut->zSig = PH7_VmBuiltinSigLookup(SyStringData(&pOut->pFunc->sName),` |
|    ! 0 |  5692 | `			SyStringLength(&pOut->pFunc->sName), &zRet);` |
|    ! 0 |  5693 | `		if( zRet && SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|    ! 0 |  5694 | `			pOut->zRet = zRet;` |
|    ! 0 |  5695 | `		}` |
|    ! 0 |  5696 | `	}` |
|   4613 |  5697 | `	if( pOut->zSig && pOut->zSig[0] == '\0' ){` |
|      - |  5698 | `		/* A declared-EMPTY signature really is "no parameters", not "undescribed". */` |
|    848 |  5699 | `		return 1;` |
|      - |  5700 | `	}` |
|   3769 |  5701 | `	return 1;` |
|   3866 |  5702 | `}` |
|      - |  5703 | `/* Is this receiver a ReflectionMethod (rather than a ReflectionFunction)? */` |
|   3826 |  5704 | `static int ReflectIsMethodReflector(ph7_context *pCtx, ph7_class_instance *pThis)` |
|      5 |  5705 | `{` |
|      - |  5706 | `	ph7_class *pRM;` |
|   3831 |  5707 | `	if( pThis == 0 ){` |
|    ! 0 |  5708 | `		return 0;` |
|      - |  5709 | `	}` |
|   3831 |  5710 | `	pRM = PH7_VmExtractClass(pCtx->pVm, "ReflectionMethod", sizeof("ReflectionMethod")-1, FALSE, 0);` |
|   3831 |  5711 | `	return pRM != 0 && PH7_VmInstanceOf(pThis->pClass, pRM);` |
|   1916 |  5712 | `}` |
|      - |  5713 | `/*` |
|      - |  5714 | `` * The function `$this` reflects. A ReflectionFunction built over a Closure keeps`` |
|      - |  5715 | `` * the object in `__cl` and resolves through it (its `$__fn` is a lambda name no`` |
|      - |  5716 | ` * function table holds); a ReflectionMethod resolves ($this->class, $this->name).` |
|      - |  5717 | ` */` |
|   3288 |  5718 | `static int ReflectFuncOfThis(ph7_context *pCtx, ReflectFuncRef *pOut)` |
|      5 |  5719 | `{` |
|   3293 |  5720 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  5721 | `	ph7_class_instance *pClo;` |
|      - |  5722 | `	ph7_value sTarget, sMethod;` |
|      - |  5723 | `	const char *zName, *zClass;` |
|      - |  5724 | `	int nName, nClass, rc;` |
|   3293 |  5725 | `	SyZero(pOut, sizeof(*pOut));` |
|   3293 |  5726 | `	if( pThis == 0 ){` |
|    ! 0 |  5727 | `		return 0;` |
|      - |  5728 | `	}` |
|   3293 |  5729 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|   3293 |  5730 | `	PH7_MemObjInit(pCtx->pVm, &sMethod);` |
|   3293 |  5731 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|   3293 |  5732 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|   3293 |  5733 | `	if( pClo ){` |
|    141 |  5734 | `		sTarget.x.pOther = pClo;` |
|    141 |  5735 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|   3224 |  5736 | `	}else if( ReflectIsMethodReflector(pCtx, pThis) ){` |
|   2075 |  5737 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|   2075 |  5738 | `		ph7_value_string(&sTarget, zClass, nClass);` |
|   2075 |  5739 | `		ph7_value_string(&sMethod, zName, nName);` |
|   1040 |  5740 | `	}else{` |
|   1085 |  5741 | `		ph7_value_string(&sTarget, zName, nName);` |
|      - |  5742 | `	}` |
|   3293 |  5743 | `	rc = ReflectFuncFill(pCtx->pVm, &sTarget, (sMethod.iFlags & MEMOBJ_STRING) ? &sMethod : 0, pOut);` |
|      - |  5744 | `	/* sTarget only ever BORROWS the closure; releasing it would unref twice. */` |
|   3293 |  5745 | `	if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|   3155 |  5746 | `		PH7_MemObjRelease(&sTarget);` |
|   1573 |  5747 | `	}` |
|   3293 |  5748 | `	PH7_MemObjRelease(&sMethod);` |
|   3293 |  5749 | `	return rc;` |
|   1647 |  5750 | `}` |
|      - |  5751 | `/*` |
|      - |  5752 | `` * The class `$this` was BUILT FOR (php's `intern->ce`), or NULL — a`` |
|      - |  5753 | ` * ReflectionFunction has none, and so does a reflector whose class went away.` |
|      - |  5754 | ` */` |
|    654 |  5755 | `static ph7_class * ReflectOwnerOfThis(ph7_context *pCtx)` |
|      5 |  5756 | `{` |
|    659 |  5757 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    659 |  5758 | `	const char *zName = 0;` |
|    659 |  5759 | `	int nName = 0;` |
|    659 |  5760 | `	if( pThis == 0 ){` |
|    ! 0 |  5761 | `		return 0;` |
|      - |  5762 | `	}` |
|    659 |  5763 | `	PH7_NativeAttrStr(pThis, RM_CE, &zName, &nName);` |
|    659 |  5764 | `	if( zName == 0 \|\| nName < 1 ){` |
|    269 |  5765 | `		return 0;` |
|      - |  5766 | `	}` |
|    393 |  5767 | `	return PH7_VmExtractClass(pCtx->pVm, zName, (sxu32)nName, FALSE, 0);` |
|    332 |  5768 | `}` |
|      - |  5769 | `/* How many parameters the target declares. */` |
|   6676 |  5770 | `static int ReflectParamCount(const ReflectFuncRef *pRef)` |
|      5 |  5771 | `{` |
|   6681 |  5772 | `	if( pRef->zSig ){` |
|   6037 |  5773 | `		return ReflectSigPart(pRef->zSig, (int)SyStrlen(pRef->zSig), -1, 0, 0);` |
|      - |  5774 | `	}` |
|    648 |  5775 | `	return pRef->pFunc ? (int)SySetUsed(&pRef->pFunc->aArgs) : 0;` |
|   3343 |  5776 | `}` |
|      - |  5777 | `/*` |
|      - |  5778 | `` * The class a `self`/`parent` in this function's declared types resolves to for`` |
|      - |  5779 | ` * DISPLAY: php substitutes the declaring class into the stored text at compile` |
|      - |  5780 | ` * time, and has nothing to substitute for a TRAIT method (whose text keeps the` |
|      - |  5781 | ` * keyword however many classes composed it) or for a plain function.` |
|      - |  5782 | `` * `static` and `iterable` are printed as written -- PH7_HINT_TEXT_* off.`` |
|      - |  5783 | ` */` |
|    948 |  5784 | `static ph7_class * ReflectDeclScope(const ReflectFuncRef *pRef)` |
|      5 |  5785 | `{` |
|    953 |  5786 | `	if( pRef->pFunc == 0 \|\| (pRef->pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){` |
|    634 |  5787 | `		return 0; /* pUserData is a class only for a METHOD */` |
|      - |  5788 | `	}` |
|    321 |  5789 | `	return VmHintScopeDeclared((ph7_class *)pRef->pFunc->pUserData);` |
|    479 |  5790 | `}` |
|      - |  5791 | `/*` |
|      - |  5792 | ` * A MEMBER's declared type as php prints it: the same rewrite ReflectDeclScope` |
|      - |  5793 | ` * describes, keyed on the class that declared the member rather than the one it` |
|      - |  5794 | `` * was reached through -- so a trait's `?self` stays `?self`.`` |
|      - |  5795 | ` */` |
|    442 |  5796 | `static const char * ReflectMemberTypeText(ph7_vm *pVm,ph7_class_attr *pAttr,char *zBuf,sxu32 nBuf)` |
|      4 |  5797 | `{` |
|    667 |  5798 | `	return VmHintTextResolvedEx(pVm,&pAttr->sTypeName,` |
|    221 |  5799 | `		VmHintScopeDeclared(pAttr->pDeclClass),0,zBuf,nBuf);` |
|      4 |  5800 | `}` |
|      - |  5801 | `/* Describe the iPos-th parameter. Answers 0 when there is none. */` |
|   6334 |  5802 | `static int ReflectParamAt(const ReflectFuncRef *pRef, int iPos, ReflectParamDesc *pOut)` |
|      5 |  5803 | `{` |
|   6339 |  5804 | `	SyZero(pOut, sizeof(*pOut));` |
|   6339 |  5805 | `	if( iPos < 0 ){` |
|    ! 0 |  5806 | `		return 0;` |
|      - |  5807 | `	}` |
|   6339 |  5808 | `	if( pRef->zSig ){` |
|   5479 |  5809 | `		const char *zPart = 0;` |
|   5479 |  5810 | `		int nPart = 0, nTotal;` |
|   5479 |  5811 | `		nTotal = ReflectSigPart(pRef->zSig, (int)SyStrlen(pRef->zSig), iPos, &zPart, &nPart);` |
|   5479 |  5812 | `		if( iPos >= nTotal \|\| zPart == 0 ){` |
|      3 |  5813 | `			return 0;` |
|      - |  5814 | `		}` |
|   5477 |  5815 | `		ReflectSigDescribe(zPart, nPart, iPos, pOut);` |
|   5477 |  5816 | `		return 1;` |
|      - |  5817 | `	}` |
|    864 |  5818 | `	if( pRef->pFunc == 0 ){` |
|    ! 0 |  5819 | `		return 0;` |
|      - |  5820 | `	}` |
|      - |  5821 | `	{` |
|    864 |  5822 | `		ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pRef->pFunc->aArgs, (sxu32)iPos);` |
|    864 |  5823 | `		if( pArg == 0 ){` |
|     13 |  5824 | `			return 0;` |
|      - |  5825 | `		}` |
|    852 |  5826 | `		pOut->iPos = iPos;` |
|    852 |  5827 | `		pOut->sName = pArg->sName;` |
|    852 |  5828 | `		pOut->bByRef = (pArg->iFlags & VM_FUNC_ARG_BY_REF) != 0;` |
|    852 |  5829 | `		pOut->bVariadic = (pArg->iFlags & VM_FUNC_ARG_VARIADIC) != 0;` |
|      - |  5830 | `		/* The compiler never sets ARG_HAS_DEF; a default IS compiled byte-code` |
|      - |  5831 | `		 * (the same test the OP_CALL default-value path uses). */` |
|    852 |  5832 | `		pOut->bHasDef = SySetUsed(&pArg->aByteCode) > 0;` |
|    852 |  5833 | `		pOut->bNullable = (pArg->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|    852 |  5834 | `		pOut->bPromoted = (pArg->iFlags & VM_FUNC_ARG_PROMOTED) != 0;` |
|    852 |  5835 | `		pOut->bOptional = pOut->bVariadic \|\| pOut->bHasDef;` |
|      - |  5836 | `		{` |
|   1276 |  5837 | `			const char *zT = VmHintTextResolvedEx(pRef->pVm,&pArg->sTypeName,` |
|    848 |  5838 | `				ReflectDeclScope(pRef),0,pOut->zTypeBuf,sizeof(pOut->zTypeBuf));` |
|    852 |  5839 | `			SyStringInitFromBuf(&pOut->sType,zT,(sxu32)SyStrlen(zT));` |
|      - |  5840 | `		}` |
|    852 |  5841 | `		pOut->pArg = pArg;` |
|    852 |  5842 | `		pOut->bInternal = (pRef->pFunc->iFlags & VM_FUNC_INTERNAL) != 0;` |
|    852 |  5843 | `		return 1;` |
|      - |  5844 | `	}` |
|   3172 |  5845 | `}` |
|      - |  5846 | `/* The declaring class php reports for a reflected METHOD. */` |
|   8368 |  5847 | `static ph7_class * ReflectFuncDeclClass(const ReflectFuncRef *pRef)` |
|      5 |  5848 | `{` |
|   8373 |  5849 | `	if( pRef->pMeth == 0 \|\| pRef->pClass == 0 ){` |
|      3 |  5850 | `		return 0;` |
|      - |  5851 | `	}` |
|   8371 |  5852 | `	return ReflectMethodDeclClass(pRef->pClass, pRef->pMeth);` |
|   4189 |  5853 | `}` |
|      - |  5854 | `/*` |
|      - |  5855 | ` * The declared return type TEXT, or 0 — and whether php calls it TENTATIVE.` |
|      - |  5856 | ` *` |
|      - |  5857 | ` * php's stubs carry two kinds of internal return type and Reflection answers` |
|      - |  5858 | ` * differently for each: a real one is reported by getReturnType()/hasReturnType()` |
|      - |  5859 | `` * and printed `Return [ T ]`, while an `@tentative-return-type` answers NULL and`` |
|      - |  5860 | ` * false from those two, is reported by getTentativeReturnType()/` |
|      - |  5861 | `` * hasTentativeReturnType(), and prints `Tentative return [ T ]`. Nearly every`` |
|      - |  5862 | ` * internal SPL, date and Reflection method is the tentative kind.` |
|      - |  5863 | ` *` |
|      - |  5864 | `` * PHL has one field for both, so a leading `@` on a `zRet` — the same character`` |
|      - |  5865 | ` * php's stub annotation uses — marks the tentative kind. It is stripped here, so` |
|      - |  5866 | ` * nothing downstream of this function ever sees it.` |
|      - |  5867 | ` */` |
|   5467 |  5868 | `static int ReflectFuncRetTextEx(const ReflectFuncRef *pRef, const char **pz, int *pn,` |
|      - |  5869 | `	int *pbTentative, char *zBuf, sxu32 nBuf)` |
|      5 |  5870 | `{` |
|   5472 |  5871 | `	if( pbTentative ){` |
|   5472 |  5872 | `		*pbTentative = 0;` |
|   2732 |  5873 | `	}` |
|   5472 |  5874 | `	if( pRef->zRet && pRef->zRet[0] ){` |
|   4950 |  5875 | `		const char *z = pRef->zRet;` |
|   4950 |  5876 | `		if( z[0] == '@' ){` |
|   3119 |  5877 | `			if( pbTentative ){` |
|   3119 |  5878 | `				*pbTentative = 1;` |
|   1558 |  5879 | `			}` |
|   3119 |  5880 | `			z++;` |
|   1558 |  5881 | `		}` |
|   4950 |  5882 | `		*pz = z;` |
|   4950 |  5883 | `		*pn = (int)SyStrlen(z);` |
|   4950 |  5884 | `		return 1;` |
|      - |  5885 | `	}` |
|    527 |  5886 | `	if( pRef->pFunc == 0 ){` |
|     38 |  5887 | `		return 0;` |
|      - |  5888 | `	}` |
|    491 |  5889 | `	if( SyStringLength(&pRef->pFunc->sReturnTypeName) > 0 ){` |
|    155 |  5890 | `		*pz = VmHintTextResolvedEx(pRef->pVm,&pRef->pFunc->sReturnTypeName,` |
|     50 |  5891 | `			ReflectDeclScope(pRef),0,zBuf,nBuf);` |
|    105 |  5892 | `		*pn = (int)SyStrlen(*pz);` |
|    105 |  5893 | `		return 1;` |
|      - |  5894 | `	}` |
|    389 |  5895 | `	return 0;` |
|   2737 |  5896 | `}` |
|      - |  5897 | `/* The declared return type php would report from getReturnType(): a TENTATIVE one` |
|      - |  5898 | ` * is not reported there at all, which is the whole distinction. */` |
|    559 |  5899 | `static int ReflectFuncRetText(const ReflectFuncRef *pRef, const char **pz, int *pn,` |
|      - |  5900 | `	char *zBuf, sxu32 nBuf)` |
|      5 |  5901 | `{` |
|    564 |  5902 | `	int bTentative = 0;` |
|    564 |  5903 | `	if( !ReflectFuncRetTextEx(pRef, pz, pn, &bTentative, zBuf, nBuf) ){` |
|     21 |  5904 | `		return 0;` |
|      - |  5905 | `	}` |
|    544 |  5906 | `	return bTentative ? 0 : 1;` |
|    283 |  5907 | `}` |
|      - |  5908 | `/* Is the reflected function internal (a C builtin or an embedded-chunk one)? */` |
|   9261 |  5909 | `static int ReflectFuncIsInternal(const ReflectFuncRef *pRef)` |
|      5 |  5910 | `{` |
|   9266 |  5911 | `	if( pRef->pHost ){` |
|   1128 |  5912 | `		return 1;` |
|      - |  5913 | `	}` |
|   8141 |  5914 | `	return pRef->pFunc != 0 && (pRef->pFunc->iFlags & VM_FUNC_INTERNAL) != 0;` |
|   4635 |  5915 | `}` |
|      - |  5916 | `/* Does this target carry a #[\Deprecated] attribute? */` |
|   3984 |  5917 | `static int ReflectHasDeprecated(SySet *pAttrs)` |
|      3 |  5918 | `{` |
|   3987 |  5919 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|      - |  5920 | `	sxu32 n;` |
|   3989 |  5921 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|      8 |  5922 | `		if( SyStringLength(&aA[n].sName) == sizeof("deprecated")-1` |
|      8 |  5923 | `		 && SyStrnicmp(SyStringData(&aA[n].sName), "deprecated", sizeof("deprecated")-1) == 0 ){` |
|      7 |  5924 | `			return 1;` |
|      - |  5925 | `		}` |
|      2 |  5926 | `	}` |
|   3981 |  5927 | `	return 0;` |
|   1995 |  5928 | `}` |
|      - |  5929 | ``/* Resolve `$this`, or answer a default when the target has gone missing. */`` |
|      - |  5930 | `#define REFLECT_FUNC_OR(REF,STMT) \` |
|      - |  5931 | `	if( !ReflectFuncOfThis(pCtx, &(REF)) ){ STMT; return PH7_OK; }` |
|      - |  5932 | `/*` |
|      - |  5933 | ` * The same, for the three getClosure* accessors: what they read is captured on` |
|      - |  5934 | ` * the Closure OBJECT, not in a function record, so a reflector over a closure` |
|      - |  5935 | ` * nothing resolves (php's magic-method trampoline) must still answer from the` |
|      - |  5936 | `` * Closure `$this` is holding. Without this they fell out at the resolve and`` |
|      - |  5937 | ` * reported no scope and no bound object for a callable php describes fully.` |
|      - |  5938 | ` */` |
|      - |  5939 | `#define REFLECT_CLOSURE_OR(REF,STMT) \` |
|      - |  5940 | `	if( !ReflectFuncOfThis(pCtx, &(REF)) ){ \` |
|      - |  5941 | `		ph7_class_instance *_pRcThis = PH7_ContextThis(pCtx); \` |
|      - |  5942 | `		SyZero(&(REF), sizeof(REF)); \` |
|      - |  5943 | `		(REF).pVm = pCtx->pVm; \` |
|      - |  5944 | `		(REF).pClosure = _pRcThis ? PH7_NativeAttrObj(_pRcThis, RF_CL) : 0; \` |
|      - |  5945 | `		if( (REF).pClosure == 0 ){ STMT; return PH7_OK; } \` |
|      - |  5946 | `	}` |
|      - |  5947 |  |
|      - |  5948 | `/* ---- ReflectionFunctionAbstract ---- */` |
|    ! 0 |  5949 | `static int vm_builtin_ReflectionFunc_clone(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  5950 | `{` |
|    ! 0 |  5951 | `	SXUNUSED(pCtx);` |
|    ! 0 |  5952 | `	SXUNUSED(nArg);` |
|    ! 0 |  5953 | `	SXUNUSED(apArg);` |
|    ! 0 |  5954 | `	return PH7_OK;` |
|    ! 0 |  5955 | `}` |
|    290 |  5956 | `static int vm_builtin_ReflectionFunc_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  5957 | `{` |
|    294 |  5958 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    294 |  5959 | `	const char *zName = "";` |
|    294 |  5960 | `	int nName = 0;` |
|    145 |  5961 | `	SXUNUSED(nArg);` |
|    145 |  5962 | `	SXUNUSED(apArg);` |
|    294 |  5963 | `	if( pThis ){` |
|    294 |  5964 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    145 |  5965 | `	}` |
|    294 |  5966 | `	ph7_result_string(pCtx, zName, nName);` |
|    294 |  5967 | `	return PH7_OK;` |
|      4 |  5968 | `}` |
|      - |  5969 | ``/* The `name` slot's namespace split — the same three answers ReflectionClass gives. */`` |
|    ! 0 |  5970 | `static int ReflectFuncNamePart(ph7_context *pCtx, int iWhat)` |
|    ! 0 |  5971 | `{` |
|    ! 0 |  5972 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    ! 0 |  5973 | `	const char *zName = "";` |
|    ! 0 |  5974 | `	int nName = 0, iCut;` |
|    ! 0 |  5975 | `	if( pThis ){` |
|    ! 0 |  5976 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    ! 0 |  5977 | `	}` |
|    ! 0 |  5978 | `	iCut = ReflectNsCut(zName, nName);` |
|    ! 0 |  5979 | `	if( iWhat == 0 ){` |
|    ! 0 |  5980 | `		ph7_result_bool(pCtx, iCut >= 0);` |
|    ! 0 |  5981 | `	}else if( iWhat == 1 ){` |
|    ! 0 |  5982 | `		ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|    ! 0 |  5983 | `	}else if( iCut < 0 ){` |
|    ! 0 |  5984 | `		ph7_result_string(pCtx, zName, nName);` |
|    ! 0 |  5985 | `	}else{` |
|    ! 0 |  5986 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|      - |  5987 | `	}` |
|    ! 0 |  5988 | `	return PH7_OK;` |
|    ! 0 |  5989 | `}` |
|      - |  5990 | `#define REFLECT_FUNC_NAMEPART(NAME,WHAT) \` |
|      - |  5991 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  5992 | `	{ \` |
|      - |  5993 | `		SXUNUSED(nArg); \` |
|      - |  5994 | `		SXUNUSED(apArg); \` |
|      - |  5995 | `		return ReflectFuncNamePart(pCtx, WHAT); \` |
|      - |  5996 | `	}` |
|    ! 0 |  5997 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_inNamespace, 0)` |
|    ! 0 |  5998 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_getNamespaceName, 1)` |
|    ! 0 |  5999 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_getShortName, 2)` |
|      - |  6000 |  |
|     14 |  6001 | `static int vm_builtin_ReflectionFunc_isClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6002 | `{` |
|      - |  6003 | `	ReflectFuncRef sRef;` |
|     15 |  6004 | `	int bAnon = 0;` |
|      7 |  6005 | `	SXUNUSED(nArg);` |
|      7 |  6006 | `	SXUNUSED(apArg);` |
|     15 |  6007 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     15 |  6008 | `	if( sRef.pFunc ){` |
|      - |  6009 | ``		/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|      - |  6010 | `		 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|     15 |  6011 | `		bAnon = (sRef.pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|     14 |  6012 | `		if( !bAnon && SyStringLength(&sRef.pFunc->sName) > 9` |
|      3 |  6013 | `		 && (SyMemcmp(SyStringData(&sRef.pFunc->sName), "[lambda_", 8) == 0` |
|    ! 0 |  6014 | `		  \|\| SyMemcmp(SyStringData(&sRef.pFunc->sName), "[closure_", 9) == 0) ){` |
|    ! 0 |  6015 | `			bAnon = 1;` |
|    ! 0 |  6016 | `		}` |
|      7 |  6017 | `	}` |
|     15 |  6018 | `	ph7_result_bool(pCtx, bAnon);` |
|     15 |  6019 | `	return PH7_OK;` |
|      8 |  6020 | `}` |
|      - |  6021 | `/*` |
|      - |  6022 | ` * php's deprecation for an INTERNAL name, or NULL. Userland's is the` |
|      - |  6023 | ` * #[\Deprecated] attribute; this is the engine's own table, stamped on the C` |
|      - |  6024 | ` * body a builtin and a native method share (aDeprecatedFunc[]).` |
|      - |  6025 | ` */` |
|   4604 |  6026 | `static const ph7_deprecated_name * ReflectFuncDeprecated(const ReflectFuncRef *pRef)` |
|      5 |  6027 | `{` |
|   4609 |  6028 | `	if( pRef->pHost ){` |
|    531 |  6029 | `		return pRef->pHost->pDeprecated;` |
|      - |  6030 | `	}` |
|   4081 |  6031 | `	if( pRef->pFunc && (pRef->pFunc->iFlags & VM_FUNC_NATIVE) && pRef->pFunc->pNative ){` |
|   3973 |  6032 | `		return pRef->pFunc->pNative->pDeprecated;` |
|      - |  6033 | `	}` |
|    109 |  6034 | `	return 0;` |
|   2307 |  6035 | `}` |
|     38 |  6036 | `static int vm_builtin_ReflectionFunc_isDeprecated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6037 | `{` |
|      - |  6038 | `	ReflectFuncRef sRef;` |
|     19 |  6039 | `	SXUNUSED(nArg);` |
|     19 |  6040 | `	SXUNUSED(apArg);` |
|     39 |  6041 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     49 |  6042 | `	ph7_result_bool(pCtx, ReflectFuncDeprecated(&sRef) != 0` |
|     24 |  6043 | `		\|\| (sRef.pFunc != 0 && ReflectHasDeprecated(&sRef.pFunc->aAttrs)));` |
|     39 |  6044 | `	return PH7_OK;` |
|     20 |  6045 | `}` |
|     52 |  6046 | `static int vm_builtin_ReflectionFunc_isInternal(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6047 | `{` |
|      - |  6048 | `	ReflectFuncRef sRef;` |
|     26 |  6049 | `	SXUNUSED(nArg);` |
|     26 |  6050 | `	SXUNUSED(apArg);` |
|     53 |  6051 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     53 |  6052 | `	ph7_result_bool(pCtx, ReflectFuncIsInternal(&sRef));` |
|     53 |  6053 | `	return PH7_OK;` |
|     27 |  6054 | `}` |
|    ! 0 |  6055 | `static int vm_builtin_ReflectionFunc_isUserDefined(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  6056 | `{` |
|      - |  6057 | `	ReflectFuncRef sRef;` |
|    ! 0 |  6058 | `	SXUNUSED(nArg);` |
|    ! 0 |  6059 | `	SXUNUSED(apArg);` |
|    ! 0 |  6060 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    ! 0 |  6061 | `	ph7_result_bool(pCtx, !ReflectFuncIsInternal(&sRef));` |
|    ! 0 |  6062 | `	return PH7_OK;` |
|    ! 0 |  6063 | `}` |
|      6 |  6064 | `static int vm_builtin_ReflectionFunc_isGenerator(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6065 | `{` |
|      - |  6066 | `	ReflectFuncRef sRef;` |
|      3 |  6067 | `	SXUNUSED(nArg);` |
|      3 |  6068 | `	SXUNUSED(apArg);` |
|      7 |  6069 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|      7 |  6070 | `	ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|      7 |  6071 | `	return PH7_OK;` |
|      4 |  6072 | `}` |
|     10 |  6073 | `static int vm_builtin_ReflectionFunc_isVariadic(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6074 | `{` |
|      - |  6075 | `	ReflectFuncRef sRef;` |
|      - |  6076 | `	ReflectParamDesc sDesc;` |
|     11 |  6077 | `	int n, nTotal, bVariadic = 0;` |
|      5 |  6078 | `	SXUNUSED(nArg);` |
|      5 |  6079 | `	SXUNUSED(apArg);` |
|     11 |  6080 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     11 |  6081 | `	nTotal = ReflectParamCount(&sRef);` |
|     31 |  6082 | `	for( n = 0 ; n < nTotal ; n++ ){` |
|     29 |  6083 | `		if( ReflectParamAt(&sRef, n, &sDesc) && sDesc.bVariadic ){` |
|      9 |  6084 | `			bVariadic = 1;` |
|      9 |  6085 | `			break;` |
|      - |  6086 | `		}` |
|     11 |  6087 | `	}` |
|     11 |  6088 | `	ph7_result_bool(pCtx, bVariadic);` |
|     11 |  6089 | `	return PH7_OK;` |
|      6 |  6090 | `}` |
|      - |  6091 | ``/* isStatic(): a METHOD's own staticness, a closure's `static function () {}`. */`` |
|     86 |  6092 | `static int vm_builtin_ReflectionFunc_isStatic(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6093 | `{` |
|      - |  6094 | `	ReflectFuncRef sRef;` |
|     43 |  6095 | `	SXUNUSED(nArg);` |
|     43 |  6096 | `	SXUNUSED(apArg);` |
|     88 |  6097 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     88 |  6098 | `	if( sRef.pMeth ){` |
|     78 |  6099 | `		ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|     40 |  6100 | `	}else{` |
|     11 |  6101 | `		ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);` |
|      - |  6102 | `	}` |
|     88 |  6103 | `	return PH7_OK;` |
|     45 |  6104 | `}` |
|      8 |  6105 | `static int vm_builtin_ReflectionFunc_returnsReference(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6106 | `{` |
|      - |  6107 | `	ReflectFuncRef sRef;` |
|      4 |  6108 | `	SXUNUSED(nArg);` |
|      4 |  6109 | `	SXUNUSED(apArg);` |
|      9 |  6110 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|      9 |  6111 | `	ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|      9 |  6112 | `	return PH7_OK;` |
|      5 |  6113 | `}` |
|      - |  6114 | `/* The Closure instance whose captured state the three getClosure* read. */` |
|     70 |  6115 | `static ph7_value * ReflectClosureAttr(ReflectFuncRef *pRef, const char *zName)` |
|      1 |  6116 | `{` |
|      - |  6117 | `	SyString sAttr;` |
|     71 |  6118 | `	if( pRef->pClosure == 0 ){` |
|    ! 0 |  6119 | `		return 0;` |
|      - |  6120 | `	}` |
|     71 |  6121 | `	SyStringInitFromBuf(&sAttr, zName, SyStrlen(zName));` |
|     71 |  6122 | `	return PH7_ClassInstanceFetchAttr(pRef->pClosure, &sAttr);` |
|     36 |  6123 | `}` |
|     20 |  6124 | `static int vm_builtin_ReflectionFunc_getClosureThis(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6125 | `{` |
|      - |  6126 | `	ReflectFuncRef sRef;` |
|      - |  6127 | `	ph7_value *pAttr;` |
|     10 |  6128 | `	SXUNUSED(nArg);` |
|     10 |  6129 | `	SXUNUSED(apArg);` |
|     21 |  6130 | `	REFLECT_CLOSURE_OR(sRef, ph7_result_null(pCtx))` |
|     21 |  6131 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|     21 |  6132 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|      9 |  6133 | `		ph7_result_value(pCtx, pAttr);` |
|      5 |  6134 | `	}else{` |
|     13 |  6135 | `		ph7_result_null(pCtx);` |
|      - |  6136 | `	}` |
|     21 |  6137 | `	return PH7_OK;` |
|     11 |  6138 | `}` |
|     34 |  6139 | `static int vm_builtin_ReflectionFunc_getClosureScopeClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6140 | `{` |
|      - |  6141 | `	ReflectFuncRef sRef;` |
|      - |  6142 | `	ph7_value *pAttr;` |
|     17 |  6143 | `	SXUNUSED(nArg);` |
|     17 |  6144 | `	SXUNUSED(apArg);` |
|     35 |  6145 | `	REFLECT_CLOSURE_OR(sRef, ph7_result_null(pCtx))` |
|     35 |  6146 | `	pAttr = ReflectClosureAttr(&sRef, "__scope");` |
|     35 |  6147 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|      - |  6148 | `		/* php reports the class that DECLARED the callee, not the one the callable NAMED:` |
|      - |  6149 | ``		 * `$__scope` is the CALLED scope (what `static::` answers, reported by`` |
|      - |  6150 | `		 * getClosureCalledClass below), and for an inherited or trait-composed method the` |
|      - |  6151 | `		 * two differ. PH7_VmClosureScopeClass is the one rule. */` |
|     43 |  6152 | `		return ReflectResultClassOf(pCtx,` |
|     14 |  6153 | `			PH7_VmClosureScopeClass(pCtx->pVm, sRef.pClosure));` |
|      - |  6154 | `	}` |
|      7 |  6155 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|      7 |  6156 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|    ! 0 |  6157 | `		return ReflectResultClassOf(pCtx, ((ph7_class_instance *)pAttr->x.pOther)->pClass);` |
|      - |  6158 | `	}` |
|      7 |  6159 | `	ph7_result_null(pCtx);` |
|      7 |  6160 | `	return PH7_OK;` |
|     18 |  6161 | `}` |
|      - |  6162 | `/*` |
|      - |  6163 | ` * ReflectionFunction::getClosureCalledClass() — php's CALLED scope, the other half of the` |
|      - |  6164 | `` * pair above: the class the call goes THROUGH, which is what `static::` answers. A bound`` |
|      - |  6165 | `` * closure's is its `$this`'s class; a static callable's is the class the callable NAMED. It`` |
|      - |  6166 | `` * shared getClosureScopeClass()'s body while `$__scope` answered both questions; it cannot`` |
|      - |  6167 | ` * now, because that one narrows a method closure to its DECLARING class.` |
|      - |  6168 | ` */` |
|      8 |  6169 | `static int vm_builtin_ReflectionFunc_getClosureCalledClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6170 | `{` |
|      - |  6171 | `	ReflectFuncRef sRef;` |
|      - |  6172 | `	ph7_value *pAttr;` |
|      4 |  6173 | `	SXUNUSED(nArg);` |
|      4 |  6174 | `	SXUNUSED(apArg);` |
|      9 |  6175 | `	REFLECT_CLOSURE_OR(sRef, ph7_result_null(pCtx))` |
|      9 |  6176 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|      9 |  6177 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|      7 |  6178 | `		return ReflectResultClassOf(pCtx, ((ph7_class_instance *)pAttr->x.pOther)->pClass);` |
|      - |  6179 | `	}` |
|      3 |  6180 | `	pAttr = ReflectClosureAttr(&sRef, "__scope");` |
|      3 |  6181 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|      4 |  6182 | `		return ReflectResultClassOf(pCtx, PH7_VmExtractClass(pCtx->pVm,` |
|      2 |  6183 | `			(const char *)SyBlobData(&pAttr->sBlob), SyBlobLength(&pAttr->sBlob), FALSE, 0));` |
|      - |  6184 | `	}` |
|    ! 0 |  6185 | `	ph7_result_null(pCtx);` |
|    ! 0 |  6186 | `	return PH7_OK;` |
|      5 |  6187 | `}` |
|     10 |  6188 | `static int vm_builtin_ReflectionFunc_getClosureUsedVariables(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6189 | `{` |
|      - |  6190 | `	ReflectFuncRef sRef;` |
|     12 |  6191 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  6192 | `	ph7_vm_func_closure_env *aEnv;` |
|      - |  6193 | `	sxu32 n;` |
|      5 |  6194 | `	SXUNUSED(nArg);` |
|      5 |  6195 | `	SXUNUSED(apArg);` |
|     12 |  6196 | `	if( pOut == 0 ){` |
|    ! 0 |  6197 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  6198 | `	}` |
|     12 |  6199 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pClosure == 0 \|\| sRef.pFunc == 0 ){` |
|    ! 0 |  6200 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  6201 | `		return PH7_OK;` |
|      - |  6202 | `	}` |
|      - |  6203 | `	/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */` |
|     12 |  6204 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|     28 |  6205 | `	for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; n++ ){` |
|     18 |  6206 | `		if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){` |
|     12 |  6207 | `			continue;` |
|      - |  6208 | `		}` |
|      6 |  6209 | `		if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|      4 |  6210 | `		 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|    ! 0 |  6211 | `			continue;` |
|      - |  6212 | `		}` |
|      7 |  6213 | `		if( (aEnv[n].iFlags & VM_FUNC_ARG_BY_REF) && aEnv[n].nIdx != SXU32_HIGH ){` |
|      - |  6214 | `			/* Captured by reference: report the slot's live value */` |
|      3 |  6215 | `			ph7_value *pLive = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, aEnv[n].nIdx);` |
|      3 |  6216 | `			ReflectMapAddDyn(pCtx, pOut, &aEnv[n].sName, pLive ? pLive : &aEnv[n].sValue);` |
|      3 |  6217 | `			continue;` |
|      - |  6218 | `		}` |
|      5 |  6219 | `		ReflectMapAddDyn(pCtx, pOut, &aEnv[n].sName, &aEnv[n].sValue);` |
|      3 |  6220 | `	}` |
|     12 |  6221 | `	ph7_result_value(pCtx, pOut);` |
|     12 |  6222 | `	return PH7_OK;` |
|      7 |  6223 | `}` |
|     14 |  6224 | `static int vm_builtin_ReflectionFunc_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6225 | `{` |
|      - |  6226 | `	ReflectFuncRef sRef;` |
|      7 |  6227 | `	SXUNUSED(nArg);` |
|      7 |  6228 | `	SXUNUSED(apArg);` |
|     15 |  6229 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     15 |  6230 | `	if( sRef.pFunc && SyStringLength(&sRef.pFunc->sDoc) > 0 ){` |
|     11 |  6231 | `		ph7_result_string(pCtx, SyStringData(&sRef.pFunc->sDoc), (int)SyStringLength(&sRef.pFunc->sDoc));` |
|      6 |  6232 | `	}else{` |
|      5 |  6233 | `		ph7_result_bool(pCtx, 0);` |
|      - |  6234 | `	}` |
|     15 |  6235 | `	return PH7_OK;` |
|      8 |  6236 | `}` |
|     26 |  6237 | `static int vm_builtin_ReflectionFunc_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6238 | `{` |
|      - |  6239 | `	ReflectFuncRef sRef;` |
|     13 |  6240 | `	SXUNUSED(nArg);` |
|     13 |  6241 | `	SXUNUSED(apArg);` |
|     27 |  6242 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     27 |  6243 | `	if( sRef.pFunc && SyStringLength(&sRef.pFunc->sFile) > 0 ){` |
|      7 |  6244 | `		ph7_result_string(pCtx, SyStringData(&sRef.pFunc->sFile), (int)SyStringLength(&sRef.pFunc->sFile));` |
|      4 |  6245 | `	}else{` |
|     21 |  6246 | `		ph7_result_bool(pCtx, 0);` |
|      - |  6247 | `	}` |
|     27 |  6248 | `	return PH7_OK;` |
|     14 |  6249 | `}` |
|     26 |  6250 | `static int ReflectFuncLine(ph7_context *pCtx, int bEnd)` |
|      1 |  6251 | `{` |
|      - |  6252 | `	ReflectFuncRef sRef;` |
|     27 |  6253 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| ReflectFuncIsInternal(&sRef) \|\| sRef.pFunc == 0 ){` |
|     17 |  6254 | `		ph7_result_bool(pCtx, 0);` |
|     17 |  6255 | `		return PH7_OK;` |
|      - |  6256 | `	}` |
|     11 |  6257 | `	ph7_result_int64(pCtx, (sxi64)(bEnd ? sRef.pFunc->nEndLine : sRef.pFunc->nLine));` |
|     11 |  6258 | `	return PH7_OK;` |
|     14 |  6259 | `}` |
|     24 |  6260 | `static int vm_builtin_ReflectionFunc_getStartLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6261 | `{` |
|     12 |  6262 | `	SXUNUSED(nArg);` |
|     12 |  6263 | `	SXUNUSED(apArg);` |
|     25 |  6264 | `	return ReflectFuncLine(pCtx, 0);` |
|      1 |  6265 | `}` |
|      2 |  6266 | `static int vm_builtin_ReflectionFunc_getEndLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6267 | `{` |
|      1 |  6268 | `	SXUNUSED(nArg);` |
|      1 |  6269 | `	SXUNUSED(apArg);` |
|      3 |  6270 | `	return ReflectFuncLine(pCtx, 1);` |
|      1 |  6271 | `}` |
|    206 |  6272 | `static int vm_builtin_ReflectionFunc_hasReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6273 | `{` |
|      - |  6274 | `	ReflectFuncRef sRef;` |
|      - |  6275 | `	const char *z;` |
|      - |  6276 | `	char zRetBuf[192];` |
|      - |  6277 | `	int n;` |
|    103 |  6278 | `	SXUNUSED(nArg);` |
|    103 |  6279 | `	SXUNUSED(apArg);` |
|    208 |  6280 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    208 |  6281 | `	ph7_result_bool(pCtx, ReflectFuncRetText(&sRef, &z, &n, zRetBuf, sizeof(zRetBuf)));` |
|    208 |  6282 | `	return PH7_OK;` |
|    105 |  6283 | `}` |
|    353 |  6284 | `static int vm_builtin_ReflectionFunc_getReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  6285 | `{` |
|      - |  6286 | `	ReflectFuncRef sRef;` |
|      - |  6287 | `	const char *z;` |
|      - |  6288 | `	char zRetBuf[192];` |
|      - |  6289 | `	int n;` |
|    175 |  6290 | `	SXUNUSED(nArg);` |
|    175 |  6291 | `	SXUNUSED(apArg);` |
|    358 |  6292 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|    358 |  6293 | `	if( !ReflectFuncRetText(&sRef, &z, &n, zRetBuf, sizeof(zRetBuf)) ){` |
|     21 |  6294 | `		ph7_result_null(pCtx);` |
|     21 |  6295 | `		return PH7_OK;` |
|      - |  6296 | `	}` |
|    338 |  6297 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx, z, n));` |
|    180 |  6298 | `}` |
|      - |  6299 | ``/* A native method's `@`-marked zRet is php's `@tentative-return-type`: reported`` |
|      - |  6300 | ` * HERE and by nothing else, which is what separates it from a real one. */` |
|    162 |  6301 | `static int vm_builtin_ReflectionFunc_hasTentativeReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6302 | `{` |
|      - |  6303 | `	ReflectFuncRef sRef;` |
|      - |  6304 | `	const char *z;` |
|      - |  6305 | `	char zRetBuf[192];` |
|    163 |  6306 | `	int n, bTentative = 0;` |
|     81 |  6307 | `	SXUNUSED(nArg);` |
|     81 |  6308 | `	SXUNUSED(apArg);` |
|    163 |  6309 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    163 |  6310 | `	ph7_result_bool(pCtx, ReflectFuncRetTextEx(&sRef, &z, &n, &bTentative, zRetBuf, sizeof(zRetBuf)) && bTentative);` |
|    163 |  6311 | `	return PH7_OK;` |
|     82 |  6312 | `}` |
|    180 |  6313 | `static int vm_builtin_ReflectionFunc_getTentativeReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6314 | `{` |
|      - |  6315 | `	ReflectFuncRef sRef;` |
|      - |  6316 | `	const char *z;` |
|      - |  6317 | `	char zRetBuf[192];` |
|    181 |  6318 | `	int n, bTentative = 0;` |
|     90 |  6319 | `	SXUNUSED(nArg);` |
|     90 |  6320 | `	SXUNUSED(apArg);` |
|    181 |  6321 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|    181 |  6322 | `	if( !ReflectFuncRetTextEx(&sRef, &z, &n, &bTentative, zRetBuf, sizeof(zRetBuf)) \|\| !bTentative ){` |
|      7 |  6323 | `		ph7_result_null(pCtx);` |
|      7 |  6324 | `		return PH7_OK;` |
|      - |  6325 | `	}` |
|    175 |  6326 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx, z, n));` |
|     91 |  6327 | `}` |
|     96 |  6328 | `static int vm_builtin_ReflectionFunc_getNumberOfParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  6329 | `{` |
|      - |  6330 | `	ReflectFuncRef sRef;` |
|     48 |  6331 | `	SXUNUSED(nArg);` |
|     48 |  6332 | `	SXUNUSED(apArg);` |
|     99 |  6333 | `	REFLECT_FUNC_OR(sRef, ph7_result_int(pCtx, 0))` |
|     93 |  6334 | `	if( sRef.pHost && sRef.zSig == 0 ){` |
|      - |  6335 | `		/* An UNDESCRIBED builtin: the arity table is all there is. */` |
|    ! 0 |  6336 | `		ph7_result_int64(pCtx, (sxi64)sRef.pHost->nMinArg);` |
|    ! 0 |  6337 | `		return PH7_OK;` |
|      - |  6338 | `	}` |
|     93 |  6339 | `	ph7_result_int(pCtx, ReflectParamCount(&sRef));` |
|     93 |  6340 | `	return PH7_OK;` |
|     51 |  6341 | `}` |
|     44 |  6342 | `static int vm_builtin_ReflectionFunc_getNumberOfRequiredParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  6343 | `{` |
|      - |  6344 | `	ReflectFuncRef sRef;` |
|      - |  6345 | `	ReflectParamDesc sDesc;` |
|     47 |  6346 | `	int n, nTotal, nReq = 0;` |
|     22 |  6347 | `	SXUNUSED(nArg);` |
|     22 |  6348 | `	SXUNUSED(apArg);` |
|     47 |  6349 | `	REFLECT_FUNC_OR(sRef, ph7_result_int(pCtx, 0))` |
|     47 |  6350 | `	if( sRef.pHost && sRef.zSig == 0 ){` |
|    ! 0 |  6351 | `		ph7_result_int64(pCtx, (sxi64)sRef.pHost->nMinArg);` |
|    ! 0 |  6352 | `		return PH7_OK;` |
|      - |  6353 | `	}` |
|     47 |  6354 | `	nTotal = ReflectParamCount(&sRef);` |
|    109 |  6355 | `	for( n = nTotal ; n > 0 ; n-- ){` |
|     99 |  6356 | `		if( ReflectParamAt(&sRef, n - 1, &sDesc) && !sDesc.bVariadic && !sDesc.bOptional ){` |
|     37 |  6357 | `			nReq = n;` |
|     37 |  6358 | `			break;` |
|      - |  6359 | `		}` |
|     33 |  6360 | `	}` |
|     47 |  6361 | `	ph7_result_int(pCtx, nReq);` |
|     47 |  6362 | `	return PH7_OK;` |
|     25 |  6363 | `}` |
|      - |  6364 | `/* The (target, method) pair a ReflectionParameter is built over. */` |
|    686 |  6365 | `static void ReflectFuncParamSpec(ph7_context *pCtx, ph7_value *pSpec)` |
|      5 |  6366 | `{` |
|    691 |  6367 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  6368 | `	ph7_class_instance *pClo;` |
|      - |  6369 | `	const char *zName, *zClass;` |
|      - |  6370 | `	int nName, nClass;` |
|    691 |  6371 | `	PH7_MemObjInit(pCtx->pVm, pSpec);` |
|    691 |  6372 | `	if( pThis == 0 ){` |
|    ! 0 |  6373 | `		return;` |
|      - |  6374 | `	}` |
|    691 |  6375 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|    691 |  6376 | `	if( pClo ){` |
|     11 |  6377 | `		pSpec->x.pOther = pClo;` |
|     11 |  6378 | `		pSpec->iFlags = MEMOBJ_OBJ;` |
|     11 |  6379 | `		return;` |
|      - |  6380 | `	}` |
|    681 |  6381 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    681 |  6382 | `	if( ReflectIsMethodReflector(pCtx, pThis) ){` |
|      - |  6383 | ``		/* `[class, method]`, which ReflectionParameter's constructor accepts */`` |
|    298 |  6384 | `		ph7_value *pList = ph7_context_new_array(pCtx);` |
|    298 |  6385 | `		ph7_value *pA = ph7_context_new_scalar(pCtx);` |
|    298 |  6386 | `		ph7_value *pB = ph7_context_new_scalar(pCtx);` |
|    298 |  6387 | `		if( pList == 0 \|\| pA == 0 \|\| pB == 0 ){` |
|    ! 0 |  6388 | `			return;` |
|      - |  6389 | `		}` |
|    298 |  6390 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|    298 |  6391 | `		ph7_value_string(pA, zClass, nClass);` |
|    298 |  6392 | `		ph7_value_string(pB, zName, nName);` |
|    298 |  6393 | `		ph7_array_add_elem(pList, 0, pA);` |
|    298 |  6394 | `		ph7_array_add_elem(pList, 0, pB);` |
|    298 |  6395 | `		PH7_MemObjStore(pList, pSpec);` |
|    298 |  6396 | `		return;` |
|      - |  6397 | `	}` |
|    387 |  6398 | `	ph7_value_string(pSpec, zName, nName);` |
|    348 |  6399 | `}` |
|    670 |  6400 | `static int vm_builtin_ReflectionFunc_getParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  6401 | `{` |
|      - |  6402 | `	ReflectFuncRef sRef;` |
|    674 |  6403 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  6404 | `	ph7_value sSpec;` |
|      - |  6405 | `	int n, nTotal;` |
|    335 |  6406 | `	SXUNUSED(nArg);` |
|    335 |  6407 | `	SXUNUSED(apArg);` |
|    674 |  6408 | `	if( pOut == 0 ){` |
|    ! 0 |  6409 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  6410 | `	}` |
|    674 |  6411 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 |  6412 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  6413 | `		return PH7_OK;` |
|      - |  6414 | `	}` |
|    674 |  6415 | `	nTotal = ReflectParamCount(&sRef);` |
|    674 |  6416 | `	ReflectFuncParamSpec(pCtx, &sSpec);` |
|   1898 |  6417 | `	for( n = 0 ; n < nTotal ; n++ ){` |
|      - |  6418 | `		ph7_value sPos, sVal;` |
|      - |  6419 | `		ph7_value *apCtor[2];` |
|      - |  6420 | `		ph7_class_instance *pParam;` |
|      - |  6421 | `		sxi32 rc;` |
|   1228 |  6422 | `		PH7_MemObjInit(pCtx->pVm, &sPos);` |
|   1228 |  6423 | `		ph7_value_int(&sPos, n);` |
|   1228 |  6424 | `		apCtor[0] = &sSpec;` |
|   1228 |  6425 | `		apCtor[1] = &sPos;` |
|   1228 |  6426 | `		pParam = ReflectConstruct(pCtx, "ReflectionParameter", 2, apCtor, &rc);` |
|   1228 |  6427 | `		PH7_MemObjRelease(&sPos);` |
|   1228 |  6428 | `		if( pParam == 0 ){` |
|    ! 0 |  6429 | `			break;` |
|      - |  6430 | `		}` |
|   1228 |  6431 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|   1228 |  6432 | `		sVal.x.pOther = pParam;` |
|   1228 |  6433 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|   1228 |  6434 | `		ph7_array_add_elem(pOut, 0, &sVal);   /* takes its own reference */` |
|   1228 |  6435 | `		PH7_ClassInstanceUnref(pParam);` |
|    616 |  6436 | `	}` |
|    674 |  6437 | `	if( (sSpec.iFlags & MEMOBJ_OBJ) == 0 ){` |
|    668 |  6438 | `		PH7_MemObjRelease(&sSpec);` |
|    332 |  6439 | `	}` |
|    674 |  6440 | `	ph7_result_value(pCtx, pOut);` |
|    674 |  6441 | `	return PH7_OK;` |
|    339 |  6442 | `}` |
|      4 |  6443 | `static int vm_builtin_ReflectionFunc_getStaticVariables(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6444 | `{` |
|      - |  6445 | `	ReflectFuncRef sRef;` |
|      5 |  6446 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  6447 | `	ph7_vm_func_static_var *aStatic;` |
|      - |  6448 | `	sxu32 n;` |
|      2 |  6449 | `	SXUNUSED(nArg);` |
|      2 |  6450 | `	SXUNUSED(apArg);` |
|      5 |  6451 | `	if( pOut == 0 ){` |
|    ! 0 |  6452 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  6453 | `	}` |
|      5 |  6454 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pFunc == 0 ){` |
|    ! 0 |  6455 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  6456 | `		return PH7_OK;` |
|      - |  6457 | `	}` |
|      - |  6458 | ``	/* php reports a closure's captured `use` variables here TOO, ahead of the body's`` |
|      - |  6459 | `	 * own statics: it compiles both into one static-variables table, and` |
|      - |  6460 | `	 * ReflectionFunction::getStaticVariables() hands back the whole thing. PHL listed` |
|      - |  6461 | ``	 * only the `static $x` ones, so a closure's captures were invisible to reflection.`` |
|      - |  6462 | `	 *` |
|      - |  6463 | `	 * Pest reads exactly this to find the closure a test case wraps --` |
|      - |  6464 | ``	 * `Reflection::getFunctionVariable($this->__test, 'closure')` is`` |
|      - |  6465 | ``	 * `(new ReflectionFunction($f))->getStaticVariables()['closure']` -- and with the`` |
|      - |  6466 | `	 * captures missing it got null and EVERY test in a Pest suite failed on the` |
|      - |  6467 | `	 * TypeError that followed.` |
|      - |  6468 | `	 *` |
|      - |  6469 | ``	 * `$this` is not one of them: php keeps the receiver in its own slot and never`` |
|      - |  6470 | `	 * lists it as a static, while PHL carries it as an ordinary env entry. A by-REFERENCE` |
|      - |  6471 | `	 * capture reports what its slot holds NOW, not the value at closure creation. */` |
|      - |  6472 | `	{` |
|      5 |  6473 | `		ph7_vm_func_closure_env *aEnv =` |
|      4 |  6474 | `			(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|      - |  6475 | `		sxu32 k;` |
|      5 |  6476 | `		for( k = 0 ; k < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++k ){` |
|    ! 0 |  6477 | `			ph7_value *pVal = &aEnv[k].sValue;` |
|    ! 0 |  6478 | `			if( SyStringLength(&aEnv[k].sName) == sizeof("this")-1` |
|    ! 0 |  6479 | `			 && SyMemcmp(SyStringData(&aEnv[k].sName), "this", sizeof("this")-1) == 0 ){` |
|    ! 0 |  6480 | `				continue;` |
|      - |  6481 | `			}` |
|    ! 0 |  6482 | `			if( aEnv[k].nIdx != SXU32_HIGH ){` |
|    ! 0 |  6483 | `				ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, aEnv[k].nIdx);` |
|    ! 0 |  6484 | `				if( pSlot ){` |
|    ! 0 |  6485 | `					pVal = pSlot;` |
|    ! 0 |  6486 | `				}` |
|    ! 0 |  6487 | `			}` |
|    ! 0 |  6488 | `			ReflectMapAddDyn(pCtx, pOut, &aEnv[k].sName, pVal);` |
|    ! 0 |  6489 | `		}` |
|      - |  6490 | `	}` |
|      - |  6491 | `	/* Current value when the slot was initialized (first call), otherwise the` |
|      - |  6492 | `	 * evaluated default — php's getStaticVariables initializes on demand and` |
|      - |  6493 | `	 * reports the same values. */` |
|      5 |  6494 | `	aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&sRef.pFunc->aStatic);` |
|      9 |  6495 | `	for( n = 0 ; n < SySetUsed(&sRef.pFunc->aStatic) ; n++ ){` |
|      5 |  6496 | `		ph7_value *pVal = 0;` |
|      - |  6497 | `		ph7_value sScratch;` |
|      5 |  6498 | `		int bScratch = 0;` |
|      5 |  6499 | `		if( aStatic[n].nIdx != SXU32_HIGH ){` |
|      3 |  6500 | `			pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);` |
|      1 |  6501 | `		}` |
|      5 |  6502 | `		if( pVal == 0 ){` |
|      3 |  6503 | `			PH7_MemObjInit(pCtx->pVm, &sScratch);` |
|      3 |  6504 | `			if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      3 |  6505 | `				VmLocalExec(pCtx->pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|      1 |  6506 | `			}` |
|      3 |  6507 | `			pVal = &sScratch;` |
|      3 |  6508 | `			bScratch = 1;` |
|      1 |  6509 | `		}` |
|      5 |  6510 | `		ReflectMapAddDyn(pCtx, pOut, &aStatic[n].sName, pVal);` |
|      5 |  6511 | `		if( bScratch ){` |
|      3 |  6512 | `			PH7_MemObjRelease(&sScratch);` |
|      1 |  6513 | `		}` |
|      3 |  6514 | `	}` |
|      5 |  6515 | `	ph7_result_value(pCtx, pOut);` |
|      5 |  6516 | `	return PH7_OK;` |
|      3 |  6517 | `}` |
|      - |  6518 | ``/* One `key => value` entry of a presented shape, by name. */`` |
|     46 |  6519 | `static void ClosurePresentAdd(ph7_vm *pVm, ph7_value *pOut, const char *zKey, ph7_value *pVal)` |
|      1 |  6520 | `{` |
|      - |  6521 | `	ph7_value sKey;` |
|     47 |  6522 | `	PH7_MemObjInitFromString(pVm, &sKey, 0);` |
|     47 |  6523 | `	PH7_MemObjStringAppend(&sKey, zKey, (sxu32)SyStrlen(zKey));` |
|     47 |  6524 | `	ph7_array_add_elem(pOut, &sKey, pVal);   /* takes its OWN reference */` |
|     47 |  6525 | `	PH7_MemObjRelease(&sKey);` |
|     47 |  6526 | `}` |
|      - |  6527 | `/*` |
|      - |  6528 | ` * php's DEBUG presentation for a Closure (ph7_class::xPresent) —` |
|      - |  6529 | `` * `zend_closure_get_debug_info` (Zend/zend_closures.c), key for key and in php's`` |
|      - |  6530 | `` * order. Every key is CONDITIONAL, which is why a plain `function(){}` shows three`` |
|      - |  6531 | ` * and a bound one with captures shows five:` |
|      - |  6532 | ` *` |
|      - |  6533 | ` *   - a FAKE closure (php's ZEND_ACC_FAKE_CLOSURE — a first-class callable or` |
|      - |  6534 | `` *     `Closure::fromCallable` over a NAMED function) shows one `function` key,`` |
|      - |  6535 | `` *     `Class::method` when it carries a scope and the bare name otherwise, and NO`` |
|      - |  6536 | `` *     name/file/line. A real closure shows `name` (php's `{closure:SCOPE:LINE}`,`` |
|      - |  6537 | `` *     which `PH7_VmFuncDisplayName` answers), `file` and an INT `line`;`` |
|      - |  6538 | `` *   - `static` is the closure's own variable state — the `use` captures AND the`` |
|      - |  6539 | `` *     body's `static $x` — keyed WITHOUT the `$`, and omitted when empty. php`` |
|      - |  6540 | ` *     shows it before the first call too, since the initializers are compiled;` |
|      - |  6541 | `` *   - `this` is the bound receiver, present only when there is one (`bindTo`,`` |
|      - |  6542 | ` *     an instance first-class callable, or the implicit capture in a method);` |
|      - |  6543 | `` *   - `parameter` is `"$name" => "<required>"\|"<optional>"`, `&$name` for a by-ref`` |
|      - |  6544 | ` *     parameter, omitted when the callee takes none. php's cut is` |
|      - |  6545 | `` *     `required_num_args`, which counts through the LAST required parameter — so`` |
|      - |  6546 | `` *     `function($a, $b = 1, $c)` reports all THREE as `<required>`, not two.`` |
|      - |  6547 | ` *` |
|      - |  6548 | `` * The non-debug half answers nothing: php's `get_properties` for a Closure is an`` |
|      - |  6549 | `` * empty table, and `(array)$closure` never reaches it at all (php special-cases a`` |
|      - |  6550 | `` * Closure in `convert_to_array` and wraps it as a SCALAR, `[0 => $closure]`; PHL`` |
|      - |  6551 | `` * answers `[]` there — PLAN §4).`` |
|      - |  6552 | ` */` |
|     20 |  6553 | `PH7_PRIVATE sxi32 PH7_ClosurePresent(ph7_vm *pVm, ph7_class_instance *pThis,` |
|      - |  6554 | `	ph7_value *pOut, int bDebug)` |
|      2 |  6555 | `{` |
|      - |  6556 | `	ReflectFuncRef sRef;` |
|      - |  6557 | `	ph7_value sCarrier, sVal;` |
|     22 |  6558 | `	int bFake = 1;` |
|     22 |  6559 | `	if( !bDebug \|\| pThis == 0 ){` |
|      8 |  6560 | `		return SXRET_OK;` |
|      - |  6561 | `	}` |
|      - |  6562 | `	/* Rule 16's other half: this carrier never took a reference, so it must be` |
|      - |  6563 | `	 * blanked before it is released or the Closure is unref'd a second time. */` |
|     15 |  6564 | `	PH7_MemObjInit(pVm, &sCarrier);` |
|     15 |  6565 | `	sCarrier.x.pOther = pThis;` |
|     15 |  6566 | `	MemObjSetType(&sCarrier, MEMOBJ_OBJ);` |
|     15 |  6567 | `	if( !ReflectFuncFill(pVm, &sCarrier, 0, &sRef) ){` |
|    ! 0 |  6568 | `		sCarrier.x.pOther = 0;` |
|    ! 0 |  6569 | `		sCarrier.iFlags = MEMOBJ_NULL;` |
|    ! 0 |  6570 | `		PH7_MemObjRelease(&sCarrier);` |
|    ! 0 |  6571 | `		return SXRET_OK;` |
|      - |  6572 | `	}` |
|     15 |  6573 | `	sCarrier.x.pOther = 0;` |
|     15 |  6574 | `	sCarrier.iFlags = MEMOBJ_NULL;` |
|     15 |  6575 | `	PH7_MemObjRelease(&sCarrier);` |
|      - |  6576 | `	/* php's FAKE-closure bit, read off what the callable actually resolved TO: a` |
|      - |  6577 | `	 * real closure's body is the anonymous function itself (the compiler's` |
|      - |  6578 | ``	 * `{closure:...}` name, or the synthesized key that stands in for it). */`` |
|     15 |  6579 | `	if( sRef.pFunc && sRef.pMeth == 0 ){` |
|      9 |  6580 | `		const SyString *pN = &sRef.pFunc->sName;` |
|      8 |  6581 | `		if( SyStringLength(&sRef.pFunc->sClosureName) > 0` |
|      4 |  6582 | `		 \|\| (pN->nByte > 8 && SyMemcmp(pN->zString, "[lambda_", 8) == 0)` |
|      1 |  6583 | `		 \|\| (pN->nByte > 9 && SyMemcmp(pN->zString, "[closure_", 9) == 0) ){` |
|      9 |  6584 | `			bFake = 0;` |
|      4 |  6585 | `		}` |
|      4 |  6586 | `	}` |
|     15 |  6587 | `	PH7_MemObjInit(pVm, &sVal);` |
|     15 |  6588 | `	if( bFake ){` |
|      7 |  6589 | `		ph7_class *pScope = ReflectFuncDeclClass(&sRef);` |
|      7 |  6590 | `		const SyString *pName = sRef.pFunc ? &sRef.pFunc->sName : &sRef.pHost->sName;` |
|      7 |  6591 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|      7 |  6592 | `		if( pScope == 0 && sRef.pClass ){` |
|    ! 0 |  6593 | `			pScope = sRef.pClass;` |
|    ! 0 |  6594 | `		}` |
|      7 |  6595 | `		if( pScope ){` |
|      7 |  6596 | `			PH7_MemObjStringAppend(&sVal, SyStringData(&pScope->sName),` |
|      2 |  6597 | `				SyStringLength(&pScope->sName));` |
|      5 |  6598 | `			PH7_MemObjStringAppend(&sVal, "::", 2);` |
|      2 |  6599 | `		}` |
|      7 |  6600 | `		PH7_MemObjStringAppend(&sVal, SyStringData(pName), SyStringLength(pName));` |
|      7 |  6601 | `		ClosurePresentAdd(pVm, pOut, "function", &sVal);` |
|      7 |  6602 | `		PH7_MemObjRelease(&sVal);` |
|      4 |  6603 | `	}else{` |
|      - |  6604 | `		const char *zShow;` |
|      9 |  6605 | `		int nShow = PH7_VmFuncDisplayName(pVm, sRef.pFunc, &zShow);` |
|      9 |  6606 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|      9 |  6607 | `		PH7_MemObjStringAppend(&sVal, zShow, (sxu32)(nShow > 0 ? nShow : 0));` |
|      9 |  6608 | `		ClosurePresentAdd(pVm, pOut, "name", &sVal);` |
|      9 |  6609 | `		PH7_MemObjRelease(&sVal);` |
|      9 |  6610 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|     13 |  6611 | `		PH7_MemObjStringAppend(&sVal, SyStringData(&sRef.pFunc->sFile),` |
|      8 |  6612 | `			SyStringLength(&sRef.pFunc->sFile));` |
|      9 |  6613 | `		ClosurePresentAdd(pVm, pOut, "file", &sVal);` |
|      9 |  6614 | `		PH7_MemObjRelease(&sVal);` |
|      9 |  6615 | `		PH7_MemObjInitFromInt(pVm, &sVal, (sxi64)sRef.pFunc->nLine);` |
|      9 |  6616 | `		ClosurePresentAdd(pVm, pOut, "line", &sVal);` |
|      9 |  6617 | `		PH7_MemObjRelease(&sVal);` |
|      - |  6618 | `	}` |
|      - |  6619 | ``	/* `static`: the captured `use` variables first (php compiles them into the`` |
|      - |  6620 | ``	 * same table, ahead of the body's own statics), then the body's `static $x`,`` |
|      - |  6621 | `	 * whose value is the live slot once it exists and the compiled initializer` |
|      - |  6622 | `	 * before that — the rule getStaticVariables() already follows. */` |
|     15 |  6623 | `	if( sRef.pFunc ){` |
|     13 |  6624 | `		ph7_hashmap *pHm = PH7_NewHashmap(pVm, 0, 0);` |
|      - |  6625 | `		sxu32 n;` |
|     13 |  6626 | `		if( pHm ){` |
|      - |  6627 | `			ph7_value sMap;` |
|     13 |  6628 | `			ph7_value *pMap = &sMap;` |
|     13 |  6629 | `			ph7_vm_func_closure_env *aEnv =` |
|     12 |  6630 | `				(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|     13 |  6631 | `			ph7_vm_func_static_var *aStatic =` |
|     12 |  6632 | `				(ph7_vm_func_static_var *)SySetBasePtr(&sRef.pFunc->aStatic);` |
|     13 |  6633 | `			PH7_MemObjInitFromArray(pVm, pMap, pHm);` |
|     25 |  6634 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++n ){` |
|     13 |  6635 | `				ph7_value *pVal = &aEnv[n].sValue;` |
|      - |  6636 | `				ph7_value sKey;` |
|     12 |  6637 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|     11 |  6638 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|      - |  6639 | ``					/* PHL carries the auto-captured `$this` as an env entry; php keeps`` |
|      - |  6640 | ``					 * it in its own `this_ptr` slot and never lists it as a static. It`` |
|      - |  6641 | ``					 * is reported below, under `this`. */`` |
|      9 |  6642 | `					continue;` |
|      - |  6643 | `				}` |
|      5 |  6644 | `				if( aEnv[n].nIdx != SXU32_HIGH ){` |
|      - |  6645 | ``					/* `use (&$x)`: the capture is an alias onto a pinned slot, and`` |
|      - |  6646 | `					 * php reports what the slot holds NOW, not the birth value. */` |
|    ! 0 |  6647 | `					ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, aEnv[n].nIdx);` |
|    ! 0 |  6648 | `					if( pSlot ){` |
|    ! 0 |  6649 | `						pVal = pSlot;` |
|    ! 0 |  6650 | `					}` |
|    ! 0 |  6651 | `				}` |
|      5 |  6652 | `				PH7_MemObjInitFromString(pVm, &sKey, &aEnv[n].sName);` |
|      5 |  6653 | `				ph7_array_add_elem(pMap, &sKey, pVal);` |
|      5 |  6654 | `				PH7_MemObjRelease(&sKey);` |
|      3 |  6655 | `			}` |
|     15 |  6656 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aStatic) ; ++n ){` |
|      3 |  6657 | `				ph7_value *pVal = 0;` |
|      - |  6658 | `				ph7_value sScratch, sKey;` |
|      3 |  6659 | `				int bScratch = 0;` |
|      3 |  6660 | `				if( aStatic[n].nIdx != SXU32_HIGH ){` |
|    ! 0 |  6661 | `					pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, aStatic[n].nIdx);` |
|    ! 0 |  6662 | `				}` |
|      3 |  6663 | `				if( pVal == 0 ){` |
|      3 |  6664 | `					PH7_MemObjInit(pVm, &sScratch);` |
|      3 |  6665 | `					if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      3 |  6666 | `						VmLocalExec(pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|      1 |  6667 | `					}` |
|      3 |  6668 | `					pVal = &sScratch;` |
|      3 |  6669 | `					bScratch = 1;` |
|      1 |  6670 | `				}` |
|      3 |  6671 | `				PH7_MemObjInitFromString(pVm, &sKey, &aStatic[n].sName);` |
|      3 |  6672 | `				ph7_array_add_elem(pMap, &sKey, pVal);` |
|      3 |  6673 | `				PH7_MemObjRelease(&sKey);` |
|      3 |  6674 | `				if( bScratch ){` |
|      3 |  6675 | `					PH7_MemObjRelease(&sScratch);` |
|      1 |  6676 | `				}` |
|      2 |  6677 | `			}` |
|     13 |  6678 | `			if( ph7_array_count(pMap) > 0 ){` |
|      5 |  6679 | `				ClosurePresentAdd(pVm, pOut, "static", pMap);` |
|      2 |  6680 | `			}` |
|     13 |  6681 | `			PH7_MemObjRelease(pMap);` |
|      6 |  6682 | `		}` |
|      6 |  6683 | `	}` |
|      - |  6684 | ``	/* `this`: the bound receiver. php keeps ONE `this_ptr` however the binding`` |
|      - |  6685 | ``	 * happened; PHL keeps two — an explicit bind (`bindTo`, an instance`` |
|      - |  6686 | ``	 * first-class callable) writes the `$__this` slot, while the IMPLICIT capture a`` |
|      - |  6687 | `	 * closure written inside a method gets rides in the closure ENVIRONMENT under` |
|      - |  6688 | ``	 * the name `this`. Either one is php's answer here. */`` |
|      - |  6689 | `	{` |
|      - |  6690 | `		SyString sAttr;` |
|      - |  6691 | `		ph7_value *pBound;` |
|     15 |  6692 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     15 |  6693 | `		pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     15 |  6694 | `		if( (pBound == 0 \|\| (pBound->iFlags & MEMOBJ_OBJ) == 0) && sRef.pFunc ){` |
|     11 |  6695 | `			ph7_vm_func_closure_env *aEnv =` |
|     10 |  6696 | `				(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|      - |  6697 | `			sxu32 n;` |
|     11 |  6698 | `			pBound = 0;` |
|     15 |  6699 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++n ){` |
|     12 |  6700 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|     11 |  6701 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|      9 |  6702 | `					pBound = &aEnv[n].sValue;` |
|      9 |  6703 | `					break;` |
|      - |  6704 | `				}` |
|      3 |  6705 | `			}` |
|      5 |  6706 | `		}` |
|     15 |  6707 | `		if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|      5 |  6708 | `			ClosurePresentAdd(pVm, pOut, "this", pBound);` |
|      2 |  6709 | `		}` |
|      - |  6710 | `	}` |
|      - |  6711 | ``	/* `parameter`: php's cut is required_num_args, which runs through the LAST`` |
|      - |  6712 | `	 * required parameter rather than stopping at the first optional one. */` |
|      - |  6713 | `	{` |
|      - |  6714 | `		ReflectParamDesc sDesc;` |
|     15 |  6715 | `		int nArgTotal = 0, nRequired = 0, i;` |
|     31 |  6716 | `		while( ReflectParamAt(&sRef, nArgTotal, &sDesc) ){` |
|     17 |  6717 | `			if( !sDesc.bOptional ){` |
|     11 |  6718 | `				nRequired = nArgTotal + 1;` |
|      5 |  6719 | `			}` |
|     17 |  6720 | `			nArgTotal++;` |
|      1 |  6721 | `		}` |
|     15 |  6722 | `		if( nArgTotal > 0 ){` |
|      9 |  6723 | `			ph7_hashmap *pHm = PH7_NewHashmap(pVm, 0, 0);` |
|      9 |  6724 | `			if( pHm ){` |
|      - |  6725 | `				ph7_value sMap;` |
|      9 |  6726 | `				ph7_value *pMap = &sMap;` |
|      9 |  6727 | `				PH7_MemObjInitFromArray(pVm, pMap, pHm);` |
|     25 |  6728 | `				for( i = 0 ; i < nArgTotal ; ++i ){` |
|      - |  6729 | `					ph7_value sKey, sWhat;` |
|     17 |  6730 | `					if( !ReflectParamAt(&sRef, i, &sDesc) ){` |
|    ! 0 |  6731 | `						break;` |
|      - |  6732 | `					}` |
|     17 |  6733 | `					PH7_MemObjInitFromString(pVm, &sKey, 0);` |
|     17 |  6734 | `					if( sDesc.bByRef ){` |
|      3 |  6735 | `						PH7_MemObjStringAppend(&sKey, "&", 1);` |
|      1 |  6736 | `					}` |
|     17 |  6737 | `					PH7_MemObjStringAppend(&sKey, "$", 1);` |
|     25 |  6738 | `					PH7_MemObjStringAppend(&sKey, SyStringData(&sDesc.sName),` |
|      8 |  6739 | `						SyStringLength(&sDesc.sName));` |
|     17 |  6740 | `					PH7_MemObjInitFromString(pVm, &sWhat, 0);` |
|     17 |  6741 | `					PH7_MemObjStringAppend(&sWhat,` |
|      8 |  6742 | `						i >= nRequired ? "<optional>" : "<required>",` |
|      8 |  6743 | `						i >= nRequired ? sizeof("<optional>")-1 : sizeof("<required>")-1);` |
|     17 |  6744 | `					ph7_array_add_elem(pMap, &sKey, &sWhat);` |
|     17 |  6745 | `					PH7_MemObjRelease(&sKey);` |
|     17 |  6746 | `					PH7_MemObjRelease(&sWhat);` |
|      9 |  6747 | `				}` |
|      9 |  6748 | `				ClosurePresentAdd(pVm, pOut, "parameter", pMap);` |
|      9 |  6749 | `				PH7_MemObjRelease(pMap);` |
|      4 |  6750 | `			}` |
|      4 |  6751 | `		}` |
|      - |  6752 | `	}` |
|     15 |  6753 | `	return SXRET_OK;` |
|     12 |  6754 | `}` |
|      - |  6755 | `/*` |
|      - |  6756 | ` * A METHOD belongs to the extension its DECLARING class does -- php reports SPL` |
|      - |  6757 | ` * for a method a userland subclass inherited from ArrayObject -- and a plain` |
|      - |  6758 | ` * function to the one the partition places its own name in.` |
|      - |  6759 | ` */` |
|   4617 |  6760 | `static int ReflectFuncExtId(const ReflectFuncRef *pRef)` |
|      5 |  6761 | `{` |
|      - |  6762 | `	const SyString *pName;` |
|   4622 |  6763 | `	if( !ReflectFuncIsInternal(pRef) ){` |
|    107 |  6764 | `		return -1;` |
|      - |  6765 | `	}` |
|   4516 |  6766 | `	if( pRef->pMeth ){` |
|   3947 |  6767 | `		return ReflectClassExtId(ReflectFuncDeclClass(pRef));` |
|      - |  6768 | `	}` |
|    572 |  6769 | `	pName = pRef->pHost ? &pRef->pHost->sName : &pRef->pFunc->sName;` |
|    572 |  6770 | `	return PH7_VmExtOfFunc(SyStringData(pName), (int)SyStringLength(pName));` |
|   2313 |  6771 | `}` |
|     43 |  6772 | `static int vm_builtin_ReflectionFunc_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6773 | `{` |
|      - |  6774 | `	ReflectFuncRef sRef;` |
|      - |  6775 | `	int iExt;` |
|     21 |  6776 | `	SXUNUSED(nArg);` |
|     21 |  6777 | `	SXUNUSED(apArg);` |
|     45 |  6778 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     45 |  6779 | `	iExt = ReflectFuncExtId(&sRef);` |
|     45 |  6780 | `	if( iExt < 0 ){` |
|      3 |  6781 | `		ph7_result_bool(pCtx, 0);` |
|      2 |  6782 | `	}else{` |
|     43 |  6783 | `		ph7_result_string(pCtx, PH7_VmExtensionName(iExt), -1);` |
|      - |  6784 | `	}` |
|     45 |  6785 | `	return PH7_OK;` |
|     23 |  6786 | `}` |
|      8 |  6787 | `static int vm_builtin_ReflectionFunc_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6788 | `{` |
|      - |  6789 | `	ReflectFuncRef sRef;` |
|      4 |  6790 | `	SXUNUSED(nArg);` |
|      4 |  6791 | `	SXUNUSED(apArg);` |
|      9 |  6792 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|      9 |  6793 | `	return ReflectExtensionOf(pCtx, ReflectFuncExtId(&sRef));` |
|      5 |  6794 | `}` |
|     84 |  6795 | `static int vm_builtin_ReflectionFunc_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  6796 | `{` |
|     89 |  6797 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  6798 | `	ReflectFuncRef sRef;` |
|      - |  6799 | `	int rc;` |
|     89 |  6800 | `	if( pThis == 0 \|\| !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pFunc == 0 ){` |
|    ! 0 |  6801 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  6802 | `		return PH7_OK;` |
|      - |  6803 | `	}` |
|     89 |  6804 | `	if( sRef.pMeth ){` |
|      - |  6805 | `		/* 4 = Attribute::TARGET_METHOD */` |
|      - |  6806 | `		ph7_value sTarget;` |
|      - |  6807 | `		const char *zClass, *zName;` |
|      - |  6808 | `		int nClass, nName;` |
|     72 |  6809 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     72 |  6810 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     72 |  6811 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     72 |  6812 | `		ph7_value_string(&sTarget, zClass, nClass);` |
|    106 |  6813 | `		rc = ReflectBuildAttrs(pCtx, &sRef.pFunc->aAttrs, "method", &sTarget,` |
|     34 |  6814 | `			zName, nName, 0, 4, nArg, apArg);` |
|     72 |  6815 | `		PH7_MemObjRelease(&sTarget);` |
|     72 |  6816 | `		return rc;` |
|      - |  6817 | `	}` |
|      - |  6818 | `	{` |
|      - |  6819 | `		/* 2 = Attribute::TARGET_FUNCTION */` |
|      - |  6820 | `		ph7_value sTarget;` |
|     19 |  6821 | `		ReflectFuncParamSpec(pCtx, &sTarget);` |
|     27 |  6822 | `		rc = ReflectBuildAttrs(pCtx, &sRef.pFunc->aAttrs, "fn", &sTarget, 0, 0, 0, 2,` |
|      8 |  6823 | `			nArg, apArg);` |
|     19 |  6824 | `		if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|     15 |  6825 | `			PH7_MemObjRelease(&sTarget);` |
|      6 |  6826 | `		}` |
|     19 |  6827 | `		return rc;` |
|      - |  6828 | `	}` |
|     47 |  6829 | `}` |
|      - |  6830 | `/* __toString(): php's export format, still chunk 9. */` |
|    508 |  6831 | `static int vm_builtin_ReflectionFunc_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  6832 | `{` |
|    254 |  6833 | `	SXUNUSED(nArg);` |
|    254 |  6834 | `	SXUNUSED(apArg);` |
|    513 |  6835 | `	return ReflectExportFuncSelf(pCtx);` |
|      5 |  6836 | `}` |
|      - |  6837 | `/* ---- ReflectionFunction ---- */` |
|      - |  6838 | `/* ReflectionFunction::__construct(Closure\|string $function) */` |
|    916 |  6839 | `static int vm_builtin_ReflectionFunction_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  6840 | `{` |
|    921 |  6841 | `	ph7_vm *pVm = pCtx->pVm;` |
|    921 |  6842 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  6843 | `	ReflectFuncRef sRef;` |
|      - |  6844 | `	ph7_class_instance *pClo;` |
|    921 |  6845 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 |  6846 | `		return PH7_OK;` |
|      - |  6847 | `	}` |
|    921 |  6848 | `	pClo = ReflectValueClosure(pVm, apArg[0]);` |
|    921 |  6849 | `	if( !ReflectFuncFill(pCtx->pVm, apArg[0], 0, &sRef) ){` |
|      - |  6850 | `		const char *zName;` |
|      - |  6851 | `		int nName;` |
|     21 |  6852 | `		if( pClo ){` |
|      - |  6853 | `			/* A Closure whose body no table holds -- php's magic-method TRAMPOLINE` |
|      - |  6854 | `` 			 * (`Closure::fromCallable([$o, 'zz'])` where the class reaches `zz` `` |
|      - |  6855 | `			 * only through __call) is the shape real libraries hit. php still` |
|      - |  6856 | `			 * NAMES it: the name is the one that was asked for, which the Closure` |
|      - |  6857 | `` 			 * carries in $__fn, and the scope is $__scope. Leaving `name` `` |
|      - |  6858 | `			 * uninitialized made every read of it a typed-property Error, and` |
|      - |  6859 | ``			 * `str_contains($r->name, '{closure')` is one line of twig's filter`` |
|      - |  6860 | `			 * compiler. The rest of the accessors still answer emptily: there is` |
|      - |  6861 | `			 * no body to describe, which is php's answer too (0 parameters, no` |
|      - |  6862 | `			 * file, no line). */` |
|      - |  6863 | `			SyString sAttr;` |
|      - |  6864 | `			ph7_value *pFn;` |
|      7 |  6865 | `			PH7_NativeSetAttrObj(pVm, pThis, RF_CL, pClo);` |
|      7 |  6866 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|      7 |  6867 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|      7 |  6868 | `			if( pFn && (pFn->iFlags & MEMOBJ_STRING) && SyBlobLength(&pFn->sBlob) > 0 ){` |
|     10 |  6869 | `				PH7_NativeSetAttrStr(pVm, pThis, "name",` |
|      6 |  6870 | `					(const char *)SyBlobData(&pFn->sBlob), (int)SyBlobLength(&pFn->sBlob));` |
|      3 |  6871 | `			}` |
|      7 |  6872 | `			return PH7_OK;` |
|      - |  6873 | `		}` |
|     15 |  6874 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|     21 |  6875 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      6 |  6876 | `			"Function %.*s() does not exist", nName, zName);` |
|      - |  6877 | `	}` |
|    903 |  6878 | `	if( pClo ){` |
|     79 |  6879 | `		PH7_NativeSetAttrObj(pVm, pThis, RF_CL, pClo);` |
|     38 |  6880 | `	}` |
|    903 |  6881 | `	if( sRef.pFunc ){` |
|    293 |  6882 | `		int bAnon = (sRef.pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|    288 |  6883 | `		if( !bAnon && SyStringLength(&sRef.pFunc->sName) > 9` |
|    160 |  6884 | `		 && (SyMemcmp(SyStringData(&sRef.pFunc->sName), "[lambda_", 8) == 0` |
|     61 |  6885 | `		  \|\| SyMemcmp(SyStringData(&sRef.pFunc->sName), "[closure_", 9) == 0) ){` |
|      3 |  6886 | `			bAnon = 1;` |
|      1 |  6887 | `		}` |
|    293 |  6888 | `		if( bAnon ){` |
|      - |  6889 | ``			/* php 8.4 names an anonymous function `{closure:SCOPE:LINE}`, where SCOPE is`` |
|      - |  6890 | `			 * the enclosing function and only the FILE at top level — the compiler` |
|      - |  6891 | `			 * recorded it (sClosureName); the file form is the fallback for a closure` |
|      - |  6892 | `			 * built without one. */` |
|      - |  6893 | `			char zBuf[512];` |
|     45 |  6894 | `			const char *zShow = SyStringData(&sRef.pFunc->sClosureName);` |
|     45 |  6895 | `			int nShow = (int)SyStringLength(&sRef.pFunc->sClosureName);` |
|     45 |  6896 | `			if( nShow < 1 ){` |
|    ! 0 |  6897 | `				const char *zFile = SyStringLength(&sRef.pFunc->sFile) > 0` |
|    ! 0 |  6898 | `					? SyStringData(&sRef.pFunc->sFile) : "";` |
|    ! 0 |  6899 | `				int nFile = (int)SyStringLength(&sRef.pFunc->sFile);` |
|    ! 0 |  6900 | `				nShow = SyBufferFormat(zBuf, sizeof(zBuf), "{closure:%.*s:%u}",` |
|    ! 0 |  6901 | `					nFile, zFile, sRef.pFunc->nLine);` |
|    ! 0 |  6902 | `				zShow = zBuf;` |
|    ! 0 |  6903 | `			}` |
|     45 |  6904 | `			PH7_NativeSetAttrStr(pVm, pThis, "name", zShow, nShow);` |
|     45 |  6905 | `			if( pClo == 0 ){` |
|      - |  6906 | `				/* Named by its lambda name: keep a Closure so the accessors can` |
|      - |  6907 | `				 * still reach the captured scope. */` |
|    ! 0 |  6908 | `				PH7_NativeSetAttrObj(pVm, pThis, RF_CL,` |
|    ! 0 |  6909 | `					PH7_VmNewClosure(pVm, &sRef.pFunc->sName, 0, 0));` |
|    ! 0 |  6910 | `			}` |
|     45 |  6911 | `			return PH7_OK;` |
|      - |  6912 | `		}` |
|    374 |  6913 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sRef.pFunc->sName),` |
|    246 |  6914 | `			(int)SyStringLength(&sRef.pFunc->sName));` |
|    251 |  6915 | `		return PH7_OK;` |
|      - |  6916 | `	}` |
|    918 |  6917 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sRef.pHost->sName),` |
|    610 |  6918 | `		(int)SyStringLength(&sRef.pHost->sName));` |
|    615 |  6919 | `	return PH7_OK;` |
|    461 |  6920 | `}` |
|      6 |  6921 | `static int vm_builtin_ReflectionFunction_isAnonymous(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6922 | `{` |
|      7 |  6923 | `	return vm_builtin_ReflectionFunc_isClosure(pCtx, nArg, apArg);` |
|      1 |  6924 | `}` |
|    ! 0 |  6925 | `static int vm_builtin_ReflectionFunction_isDisabled(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  6926 | `{` |
|    ! 0 |  6927 | `	SXUNUSED(nArg);` |
|    ! 0 |  6928 | `	SXUNUSED(apArg);` |
|    ! 0 |  6929 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 |  6930 | `	return PH7_OK;` |
|    ! 0 |  6931 | `}` |
|      - |  6932 | `/* The callable a ReflectionFunction invokes: the Closure it holds, or its name. */` |
|     22 |  6933 | `static void ReflectFunctionCallable(ph7_context *pCtx, ph7_value *pOut)` |
|      2 |  6934 | `{` |
|     24 |  6935 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  6936 | `	ph7_class_instance *pClo;` |
|     24 |  6937 | `	const char *zName = "";` |
|     24 |  6938 | `	int nName = 0;` |
|     24 |  6939 | `	PH7_MemObjInit(pCtx->pVm, pOut);` |
|     24 |  6940 | `	if( pThis == 0 ){` |
|    ! 0 |  6941 | `		return;` |
|      - |  6942 | `	}` |
|     24 |  6943 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|     24 |  6944 | `	if( pClo ){` |
|      9 |  6945 | `		pOut->x.pOther = pClo;` |
|      9 |  6946 | `		pOut->iFlags = MEMOBJ_OBJ;` |
|      9 |  6947 | `		return;` |
|      - |  6948 | `	}` |
|     16 |  6949 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     16 |  6950 | `	ph7_value_string(pOut, zName, nName);` |
|     13 |  6951 | `}` |
|     22 |  6952 | `static int ReflectFunctionInvoke(ph7_context *pCtx, int nCall, ph7_value **apCall)` |
|      2 |  6953 | `{` |
|      - |  6954 | `	ph7_value sTarget, sResult;` |
|      - |  6955 | `	sxi32 rc;` |
|     24 |  6956 | `	ReflectFunctionCallable(pCtx, &sTarget);` |
|     24 |  6957 | `	PH7_MemObjInit(pCtx->pVm, &sResult);` |
|     24 |  6958 | `	sResult.nIdx = SXU32_HIGH;` |
|     24 |  6959 | `	rc = PH7_VmCallUserFunction(pCtx->pVm, &sTarget, nCall, apCall, &sResult);` |
|     24 |  6960 | `	if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|     16 |  6961 | `		PH7_MemObjRelease(&sTarget);` |
|      7 |  6962 | `	}` |
|     24 |  6963 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|    ! 0 |  6964 | `		PH7_MemObjRelease(&sResult);` |
|    ! 0 |  6965 | `		return rc;` |
|      - |  6966 | `	}` |
|     24 |  6967 | `	ph7_result_value(pCtx, &sResult);` |
|     24 |  6968 | `	PH7_MemObjRelease(&sResult);` |
|     24 |  6969 | `	return PH7_OK;` |
|     13 |  6970 | `}` |
|     18 |  6971 | `static int vm_builtin_ReflectionFunction_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6972 | `{` |
|     20 |  6973 | `	return ReflectFunctionInvoke(pCtx, nArg, apArg);` |
|      2 |  6974 | `}` |
|      4 |  6975 | `static int vm_builtin_ReflectionFunction_invokeArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  6976 | `{` |
|      - |  6977 | `	SySet aCall;` |
|      - |  6978 | `	int rc;` |
|      6 |  6979 | `	SySetInit(&aCall, &pCtx->pVm->sAllocator, sizeof(ph7_value *));` |
|      6 |  6980 | `	if( nArg > 0 ){` |
|      6 |  6981 | `		ReflectCollectArgs(pCtx, apArg[0], &aCall, 0);` |
|      2 |  6982 | `	}` |
|      6 |  6983 | `	rc = ReflectFunctionInvoke(pCtx, (int)SySetUsed(&aCall), (ph7_value **)SySetBasePtr(&aCall));` |
|      6 |  6984 | `	SySetRelease(&aCall);` |
|      6 |  6985 | `	return rc;` |
|      2 |  6986 | `}` |
|      6 |  6987 | `static int vm_builtin_ReflectionFunction_getClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  6988 | `{` |
|      7 |  6989 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  6990 | `	ph7_class_instance *pClo;` |
|      7 |  6991 | `	const char *zName = "";` |
|      7 |  6992 | `	int nName = 0;` |
|      - |  6993 | `	SyString sName;` |
|      3 |  6994 | `	SXUNUSED(nArg);` |
|      3 |  6995 | `	SXUNUSED(apArg);` |
|      7 |  6996 | `	if( pThis == 0 ){` |
|    ! 0 |  6997 | `		ph7_result_null(pCtx);` |
|    ! 0 |  6998 | `		return PH7_OK;` |
|      - |  6999 | `	}` |
|      7 |  7000 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|      7 |  7001 | `	if( pClo ){` |
|      - |  7002 | `		/* Already a Closure: hand the same instance back */` |
|      3 |  7003 | `		return ReflectResultExistingObject(pCtx, pClo);` |
|      - |  7004 | `	}` |
|      5 |  7005 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      5 |  7006 | `	SyStringInitFromBuf(&sName, zName, nName);` |
|      5 |  7007 | `	return ReflectResultObject(pCtx, PH7_VmNewClosure(pCtx->pVm, &sName, 0, 0));` |
|      4 |  7008 | `}` |
|      - |  7009 | `/* ---- ReflectionMethod ---- */` |
|      - |  7010 | `/* ReflectionMethod::__construct(object\|string $objectOrMethod, ?string $method = null) */` |
|   1124 |  7011 | `static int vm_builtin_ReflectionMethod_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  7012 | `{` |
|   1129 |  7013 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1129 |  7014 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7015 | `	ph7_class *pClass;` |
|      - |  7016 | `	SyHashEntry *pEntry;` |
|      - |  7017 | `	ph7_value sClass, sMethod;` |
|      - |  7018 | `	const char *zMethod;` |
|   1129 |  7019 | `	int nMethod, rc = PH7_OK;` |
|   1129 |  7020 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 |  7021 | `		return PH7_OK;` |
|      - |  7022 | `	}` |
|   1129 |  7023 | `	PH7_MemObjInit(pVm, &sClass);` |
|   1129 |  7024 | `	PH7_MemObjInit(pVm, &sMethod);` |
|   1130 |  7025 | `	if( nArg < 2 \|\| ph7_value_is_null(apArg[1]) ){` |
|      - |  7026 | `		/* One-argument form: "Class::method" */` |
|      - |  7027 | `		const char *zSpec;` |
|      3 |  7028 | `		int nSpec, iSep = -1, k;` |
|      3 |  7029 | `		if( (apArg[0]->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  7030 | `			PH7_MemObjRelease(&sClass);` |
|    ! 0 |  7031 | `			PH7_MemObjRelease(&sMethod);` |
|    ! 0 |  7032 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  7033 | `				"The parameter class is expected to be either a string or an object");` |
|      - |  7034 | `		}` |
|      3 |  7035 | `		zSpec = ph7_value_to_string(apArg[0], &nSpec);` |
|     19 |  7036 | `		for( k = 0 ; k + 1 < nSpec ; k++ ){` |
|     19 |  7037 | `			if( zSpec[k] == ':' && zSpec[k+1] == ':' ){` |
|      3 |  7038 | `				iSep = k;` |
|      3 |  7039 | `				break;` |
|      - |  7040 | `			}` |
|      9 |  7041 | `		}` |
|      3 |  7042 | `		if( iSep < 0 ){` |
|    ! 0 |  7043 | `			PH7_MemObjRelease(&sClass);` |
|    ! 0 |  7044 | `			PH7_MemObjRelease(&sMethod);` |
|    ! 0 |  7045 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 |  7046 | `				"Invalid method name %.*s", nSpec, zSpec);` |
|      - |  7047 | `		}` |
|      - |  7048 | `		/* php 8.3 deprecated the one-argument spelling in favour of the static` |
|      - |  7049 | `		 * factory. createFromMethodName() splits the name ITSELF and constructs` |
|      - |  7050 | `		 * with two, so it does not trip this. */` |
|      3 |  7051 | `		PH7_VmThrowError(pVm, 0, E_DEPRECATED,` |
|      - |  7052 | `			"Calling ReflectionMethod::__construct() with 1 argument is deprecated, "` |
|      - |  7053 | `			"use ReflectionMethod::createFromMethodName() instead");` |
|      3 |  7054 | `		ph7_value_string(&sClass, zSpec, iSep);` |
|      3 |  7055 | `		ph7_value_string(&sMethod, &zSpec[iSep+2], nSpec - iSep - 2);` |
|      2 |  7056 | `	}else{` |
|   1127 |  7057 | `		PH7_MemObjStore(apArg[0], &sClass);` |
|   1127 |  7058 | `		PH7_MemObjStore(apArg[1], &sMethod);` |
|      - |  7059 | `	}` |
|   1129 |  7060 | `	pClass = ReflectResolveClass(pVm, &sClass);` |
|   1129 |  7061 | `	if( pClass == 0 ){` |
|      - |  7062 | `		const char *zName;` |
|      - |  7063 | `		int nName;` |
|      3 |  7064 | `		zName = ph7_value_to_string(&sClass, &nName);` |
|      4 |  7065 | `		rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  7066 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      3 |  7067 | `		goto Done;` |
|      - |  7068 | `	}` |
|   1127 |  7069 | `	zMethod = ph7_value_to_string(&sMethod, &nMethod);` |
|   1127 |  7070 | `	pEntry = ReflectFindMethodEntry(pClass, zMethod, nMethod);` |
|   1127 |  7071 | `	if( pEntry == 0 ){` |
|     10 |  7072 | `		rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 |  7073 | `			"Method %z::%.*s() does not exist", &pClass->sName, nMethod, zMethod);` |
|      7 |  7074 | `		goto Done;` |
|      - |  7075 | `	}` |
|      - |  7076 | `	{` |
|      - |  7077 | `		/* php's $class is the DECLARING class, not the one the lookup went` |
|      - |  7078 | ``		 * through: `new ReflectionMethod('Kid','bm')` on an inherited method`` |
|      - |  7079 | `		 * reports Base. */` |
|   1121 |  7080 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, (ph7_class_method *)pEntry->pUserData);` |
|   1679 |  7081 | `		PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pDecl->sName),` |
|   1116 |  7082 | `			(int)SyStringLength(&pDecl->sName));` |
|      - |  7083 | `	}` |
|      - |  7084 | `	/* The class the reflector was built FOR, which the export tags read. Every` |
|      - |  7085 | `	 * ReflectionClass door that hands a method out passes its OWN name as the` |
|      - |  7086 | `	 * first constructor argument, so they all land here too. */` |
|   1679 |  7087 | `	PH7_NativeSetAttrStr(pVm, pThis, RM_CE, SyStringData(&pClass->sName),` |
|   1116 |  7088 | `		(int)SyStringLength(&pClass->sName));` |
|      - |  7089 | `	/* The DECLARED spelling, whatever case was asked for. */` |
|   1121 |  7090 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", (const char *)pEntry->pKey, (int)pEntry->nKeyLen);` |
|    562 |  7091 | `Done:` |
|   1129 |  7092 | `	PH7_MemObjRelease(&sClass);` |
|   1129 |  7093 | `	PH7_MemObjRelease(&sMethod);` |
|   1129 |  7094 | `	return rc;` |
|    567 |  7095 | `}` |
|      - |  7096 | `/* ReflectionMethod::createFromMethodName(string $method): static */` |
|      4 |  7097 | `static int vm_builtin_ReflectionMethod_createFromMethodName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7098 | `{` |
|      - |  7099 | `	ph7_class_instance *pOut;` |
|      - |  7100 | `	ph7_value sClass, sMethod;` |
|      - |  7101 | `	ph7_value *apCtor[2];` |
|      - |  7102 | `	const char *zSpec;` |
|      5 |  7103 | `	int nSpec, iSep = -1, k;` |
|      - |  7104 | `	sxi32 rc;` |
|      5 |  7105 | `	if( nArg < 1 ){` |
|    ! 0 |  7106 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7107 | `		return PH7_OK;` |
|      - |  7108 | `	}` |
|      - |  7109 | `	/* Split here rather than letting the constructor do it: the one-argument` |
|      - |  7110 | `	 * constructor is the DEPRECATED spelling this factory exists to replace. */` |
|      5 |  7111 | `	zSpec = ph7_value_to_string(apArg[0], &nSpec);` |
|     43 |  7112 | `	for( k = 0 ; k + 1 < nSpec ; k++ ){` |
|     43 |  7113 | `		if( zSpec[k] == ':' && zSpec[k+1] == ':' ){` |
|      5 |  7114 | `			iSep = k;` |
|      5 |  7115 | `			break;` |
|      - |  7116 | `		}` |
|     20 |  7117 | `	}` |
|      5 |  7118 | `	if( iSep < 0 ){` |
|    ! 0 |  7119 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 |  7120 | `			"Invalid method name %.*s", nSpec, zSpec);` |
|      - |  7121 | `	}` |
|      5 |  7122 | `	PH7_MemObjInit(pCtx->pVm, &sClass);` |
|      5 |  7123 | `	PH7_MemObjInit(pCtx->pVm, &sMethod);` |
|      5 |  7124 | `	ph7_value_string(&sClass, zSpec, iSep);` |
|      5 |  7125 | `	ph7_value_string(&sMethod, &zSpec[iSep+2], nSpec - iSep - 2);` |
|      5 |  7126 | `	apCtor[0] = &sClass;` |
|      5 |  7127 | `	apCtor[1] = &sMethod;` |
|      5 |  7128 | `	pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|      5 |  7129 | `	PH7_MemObjRelease(&sClass);` |
|      5 |  7130 | `	PH7_MemObjRelease(&sMethod);` |
|      5 |  7131 | `	if( pOut == 0 ){` |
|    ! 0 |  7132 | `		if( rc != PH7_OK ){` |
|    ! 0 |  7133 | `			return rc;` |
|      - |  7134 | `		}` |
|    ! 0 |  7135 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7136 | `		return PH7_OK;` |
|      - |  7137 | `	}` |
|      5 |  7138 | `	return ReflectResultObject(pCtx, pOut);` |
|      3 |  7139 | `}` |
|      - |  7140 | `/* The visibility/modifier predicates, all off the method record. */` |
|     38 |  7141 | `static int ReflectMethodFlag(ph7_context *pCtx, int iWhat)` |
|      2 |  7142 | `{` |
|      - |  7143 | `	ReflectFuncRef sRef;` |
|     40 |  7144 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|    ! 0 |  7145 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  7146 | `		return PH7_OK;` |
|      - |  7147 | `	}` |
|     40 |  7148 | `	switch( iWhat ){` |
|     12 |  7149 | `	case 0: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PUBLIC); break;` |
|     11 |  7150 | `	case 1: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PRIVATE); break;` |
|      3 |  7151 | `	case 2: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PROTECTED); break;` |
|      7 |  7152 | `	case 3: ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0); break;` |
|     11 |  7153 | `	default: ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0); break;` |
|      - |  7154 | `	}` |
|     40 |  7155 | `	return PH7_OK;` |
|     21 |  7156 | `}` |
|      - |  7157 | `#define REFLECT_METHOD_FLAG(NAME,WHAT) \` |
|      - |  7158 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  7159 | `	{ \` |
|      - |  7160 | `		SXUNUSED(nArg); \` |
|      - |  7161 | `		SXUNUSED(apArg); \` |
|      - |  7162 | `		return ReflectMethodFlag(pCtx, WHAT); \` |
|      - |  7163 | `	}` |
|     12 |  7164 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isPublic, 0)` |
|     11 |  7165 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isPrivate, 1)` |
|      3 |  7166 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isProtected, 2)` |
|      7 |  7167 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isAbstract, 3)` |
|     11 |  7168 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isFinal, 4)` |
|      - |  7169 |  |
|      - |  7170 | `/* isConstructor()/isDestructor() read the NAME, not the record: a trait` |
|      - |  7171 | `` * `use T { m as __construct; }` alias is the constructor under that key. */`` |
|      4 |  7172 | `static int ReflectMethodNameIs(ph7_context *pCtx, const char *zWant, int nWant)` |
|      2 |  7173 | `{` |
|      6 |  7174 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      6 |  7175 | `	const char *zName = "";` |
|      6 |  7176 | `	int nName = 0;` |
|      6 |  7177 | `	if( pThis ){` |
|      6 |  7178 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 |  7179 | `	}` |
|      6 |  7180 | `	ph7_result_bool(pCtx, nName == nWant && SyStrnicmp(zName, zWant, (sxu32)nWant) == 0);` |
|      6 |  7181 | `	return PH7_OK;` |
|      2 |  7182 | `}` |
|      4 |  7183 | `static int vm_builtin_ReflectionMethod_isConstructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7184 | `{` |
|      2 |  7185 | `	SXUNUSED(nArg);` |
|      2 |  7186 | `	SXUNUSED(apArg);` |
|      6 |  7187 | `	return ReflectMethodNameIs(pCtx, "__construct", sizeof("__construct")-1);` |
|      2 |  7188 | `}` |
|    ! 0 |  7189 | `static int vm_builtin_ReflectionMethod_isDestructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  7190 | `{` |
|    ! 0 |  7191 | `	SXUNUSED(nArg);` |
|    ! 0 |  7192 | `	SXUNUSED(apArg);` |
|    ! 0 |  7193 | `	return ReflectMethodNameIs(pCtx, "__destruct", sizeof("__destruct")-1);` |
|    ! 0 |  7194 | `}` |
|     10 |  7195 | `static int vm_builtin_ReflectionMethod_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7196 | `{` |
|      - |  7197 | `	ReflectFuncRef sRef;` |
|      5 |  7198 | `	SXUNUSED(nArg);` |
|      5 |  7199 | `	SXUNUSED(apArg);` |
|     11 |  7200 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|    ! 0 |  7201 | `		ph7_result_int(pCtx, 0);` |
|    ! 0 |  7202 | `		return PH7_OK;` |
|      - |  7203 | `	}` |
|     11 |  7204 | `	ph7_result_int64(pCtx, ReflectMethodModifiers(sRef.pMeth));` |
|     11 |  7205 | `	return PH7_OK;` |
|      6 |  7206 | `}` |
|    338 |  7207 | `static int vm_builtin_ReflectionMethod_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  7208 | `{` |
|      - |  7209 | `	ReflectFuncRef sRef;` |
|    169 |  7210 | `	SXUNUSED(nArg);` |
|    169 |  7211 | `	SXUNUSED(apArg);` |
|    342 |  7212 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 |  7213 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7214 | `		return PH7_OK;` |
|      - |  7215 | `	}` |
|    342 |  7216 | `	return ReflectResultClassOf(pCtx, ReflectFuncDeclClass(&sRef));` |
|    173 |  7217 | `}` |
|      - |  7218 | `/*` |
|      - |  7219 | ` * The receiver check php runs before invoking or binding: a non-static method` |
|      - |  7220 | ` * needs an instance OF ITS DECLARING CLASS; a static one ignores whatever was` |
|      - |  7221 | ` * handed over. *ppThis is cleared for a static method.` |
|      - |  7222 | ` */` |
|     46 |  7223 | `static sxi32 ReflectMethodReceiver(ph7_context *pCtx, ReflectFuncRef *pRef,` |
|      - |  7224 | `	ph7_value *pObject, ph7_class_instance **ppThis, int bClosure)` |
|      2 |  7225 | `{` |
|     48 |  7226 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     48 |  7227 | `	const char *zClass = "", *zName = "";` |
|     48 |  7228 | `	int nClass = 0, nName = 0;` |
|     48 |  7229 | `	ph7_class *pDecl = ReflectFuncDeclClass(pRef);` |
|     48 |  7230 | `	*ppThis = 0;` |
|     48 |  7231 | `	if( pThis ){` |
|     48 |  7232 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     48 |  7233 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     23 |  7234 | `	}` |
|     48 |  7235 | `	if( pRef->pMeth && (pRef->pMeth->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|     14 |  7236 | `		return PH7_OK;` |
|      - |  7237 | `	}` |
|     36 |  7238 | `	if( pObject == 0 \|\| (pObject->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      9 |  7239 | `		if( bClosure ){` |
|      5 |  7240 | `			return PH7_VmThrowException(pCtx, "ValueError",` |
|      - |  7241 | `				"ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods");` |
|      - |  7242 | `		}` |
|      7 |  7243 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  7244 | `			"Trying to invoke non static method %.*s::%.*s() without an object",` |
|      2 |  7245 | `			nClass, zClass, nName, zName);` |
|      - |  7246 | `	}` |
|     28 |  7247 | `	*ppThis = (ph7_class_instance *)pObject->x.pOther;` |
|     28 |  7248 | `	if( pDecl && !PH7_VmInstanceOf((*ppThis)->pClass, pDecl) ){` |
|      3 |  7249 | `		*ppThis = 0;` |
|      3 |  7250 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  7251 | `			"Given object is not an instance of the class this method was declared in");` |
|      - |  7252 | `	}` |
|     26 |  7253 | `	return PH7_OK;` |
|     25 |  7254 | `}` |
|     30 |  7255 | `static int ReflectMethodInvoke(ph7_context *pCtx, ph7_value *pObject, int nCall, ph7_value **apCall)` |
|      2 |  7256 | `{` |
|     32 |  7257 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  7258 | `	ReflectFuncRef sRef;` |
|     32 |  7259 | `	ph7_class_instance *pRecv = 0;` |
|      - |  7260 | `	ph7_value sResult;` |
|      - |  7261 | `	sxi32 rc;` |
|     32 |  7262 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|    ! 0 |  7263 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7264 | `		return PH7_OK;` |
|      - |  7265 | `	}` |
|     32 |  7266 | `	rc = ReflectMethodReceiver(pCtx, &sRef, pObject, &pRecv, 0);` |
|     32 |  7267 | `	if( rc != PH7_OK ){` |
|      7 |  7268 | `		return rc;` |
|      - |  7269 | `	}` |
|     26 |  7270 | `	PH7_MemObjInit(pVm, &sResult);` |
|     26 |  7271 | `	sResult.nIdx = SXU32_HIGH;` |
|      - |  7272 | `	/* Reflection ignores method visibility (PHP 8.1+); the flag is consumed by` |
|      - |  7273 | `	 * the first OP_CALL, i.e. this synthetic one. */` |
|     26 |  7274 | `	pVm->bReflectBypass = 1;` |
|      - |  7275 | `	/* And it binds the arguments WEAKLY however strict the file that called` |
|      - |  7276 | `	 * invoke() is: php reads strict mode off the frame that MADE the call, and` |
|      - |  7277 | `	 * that frame is ReflectionMethod::invoke itself, an internal function with no` |
|      - |  7278 | `	 * strict_types of its own. PHPUnit's mock builder reaches every original` |
|      - |  7279 | `	 * constructor this way (Generator::instantiate -> getConstructor()->invokeArgs),` |
|      - |  7280 | `	 * from a file that declares strict_types=1. */` |
|     26 |  7281 | `	pVm->bCallbackWeak = 1;` |
|     26 |  7282 | `	rc = PH7_VmCallClassMethod(pVm, pRecv, sRef.pMeth, &sResult, nCall, apCall);` |
|     26 |  7283 | `	pVm->bCallbackWeak = 0; /* clear if the dispatch never reached an OP_CALL */` |
|     26 |  7284 | `	pVm->bReflectBypass = 0;` |
|     26 |  7285 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|    ! 0 |  7286 | `		PH7_MemObjRelease(&sResult);` |
|    ! 0 |  7287 | `		return rc;` |
|      - |  7288 | `	}` |
|     26 |  7289 | `	ph7_result_value(pCtx, &sResult);` |
|     26 |  7290 | `	PH7_MemObjRelease(&sResult);` |
|     26 |  7291 | `	return PH7_OK;` |
|     17 |  7292 | `}` |
|     24 |  7293 | `static int vm_builtin_ReflectionMethod_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7294 | `{` |
|     50 |  7295 | `	return ReflectMethodInvoke(pCtx, nArg > 0 ? apArg[0] : 0,` |
|     24 |  7296 | `		nArg > 1 ? nArg - 1 : 0, nArg > 1 ? &apArg[1] : 0);` |
|      2 |  7297 | `}` |
|      6 |  7298 | `static int vm_builtin_ReflectionMethod_invokeArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7299 | `{` |
|      - |  7300 | `	SySet aCall;` |
|      - |  7301 | `	int rc;` |
|      8 |  7302 | `	SySetInit(&aCall, &pCtx->pVm->sAllocator, sizeof(ph7_value *));` |
|      8 |  7303 | `	if( nArg > 1 ){` |
|      8 |  7304 | `		ReflectCollectArgs(pCtx, apArg[1], &aCall, 0);` |
|      3 |  7305 | `	}` |
|      5 |  7306 | `	rc = ReflectMethodInvoke(pCtx, nArg > 0 ? apArg[0] : 0,` |
|      6 |  7307 | `		(int)SySetUsed(&aCall), (ph7_value **)SySetBasePtr(&aCall));` |
|      8 |  7308 | `	SySetRelease(&aCall);` |
|      8 |  7309 | `	return rc;` |
|      2 |  7310 | `}` |
|     16 |  7311 | `static int vm_builtin_ReflectionMethod_getClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7312 | `{` |
|      - |  7313 | `	ReflectFuncRef sRef;` |
|     17 |  7314 | `	ph7_class_instance *pRecv = 0;` |
|      - |  7315 | `	sxi32 rc;` |
|     17 |  7316 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 \|\| sRef.pClass == 0 ){` |
|    ! 0 |  7317 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7318 | `		return PH7_OK;` |
|      - |  7319 | `	}` |
|     17 |  7320 | `	rc = ReflectMethodReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pRecv, 1);` |
|     17 |  7321 | `	if( rc != PH7_OK ){` |
|      5 |  7322 | `		return rc;` |
|      - |  7323 | `	}` |
|      - |  7324 | `	{` |
|      - |  7325 | `		/* php hands back a closure over the RESOLVED method and reflection is allowed past` |
|      - |  7326 | `		 * protection (8.1+), so this one must dispatch without a second visibility decision —` |
|      - |  7327 | `		 * the FCC_METHOD mark is what tells the unwrap its $__fn is a screened METHOD name. */` |
|     19 |  7328 | `		ph7_class_instance *pClo = PH7_VmNewClosure(pCtx->pVm, &sRef.pMeth->sFunc.sName,` |
|     12 |  7329 | `			pRecv, &sRef.pClass->sName);` |
|     13 |  7330 | `		if( pClo ){` |
|     13 |  7331 | `			pClo->iFlags \|= VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SCREENED;` |
|      6 |  7332 | `		}` |
|     13 |  7333 | `		return ReflectResultObject(pCtx, pClo);` |
|      - |  7334 | `	}` |
|      9 |  7335 | `}` |
|      - |  7336 | `/* php 8.1 made every reflected member accessible; the setter is a no-op it kept. */` |
|    ! 0 |  7337 | `static int vm_builtin_ReflectionMethod_setAccessible(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  7338 | `{` |
|    ! 0 |  7339 | `	SXUNUSED(pCtx);` |
|    ! 0 |  7340 | `	SXUNUSED(nArg);` |
|    ! 0 |  7341 | `	SXUNUSED(apArg);` |
|    ! 0 |  7342 | `	return PH7_OK;` |
|    ! 0 |  7343 | `}` |
|      - |  7344 | `/*` |
|      - |  7345 | `` * php's `overwrites`: the class the entry found in the PARENT's method table`` |
|      - |  7346 | ` * belongs to — which is an interface when the parent only inherited it from` |
|      - |  7347 | `` * one, so `ReflectionFunction::__toString` overwrites Stringable. A method`` |
|      - |  7348 | ` * that first appears in an interface the class itself implements is a` |
|      - |  7349 | `` * `prototype` instead, never an overwrite.`` |
|      - |  7350 | ` */` |
|   2098 |  7351 | `static ph7_class * ReflectOverwritesIn(ph7_class *pClass, const char *zName, int nName)` |
|      3 |  7352 | `{` |
|      - |  7353 | `	ph7_class *pWalk;` |
|   2101 |  7354 | `	int iDepth = 0;` |
|   2101 |  7355 | `	if( pClass == 0 \|\| nName < 1 ){` |
|    ! 0 |  7356 | `		return 0;` |
|      - |  7357 | `	}` |
|   2523 |  7358 | `	for( pWalk = pClass->pBase ; pWalk && iDepth <= REFLECT_WALK_MAX_DEPTH ; pWalk = pWalk->pBase ){` |
|    587 |  7359 | `		SyHashEntry *pEntry = ReflectFindMethodEntry(pWalk, zName, nName);` |
|    587 |  7360 | `		if( pEntry ){` |
|    165 |  7361 | `			ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    165 |  7362 | `			if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|    165 |  7363 | `				return ReflectMethodDeclClass(pWalk, pMeth);` |
|      - |  7364 | `			}` |
|    ! 0 |  7365 | `		}` |
|    423 |  7366 | `		iDepth++;` |
|    212 |  7367 | `	}` |
|   1937 |  7368 | `	return 0;` |
|   1052 |  7369 | `}` |
|      - |  7370 | `/*` |
|      - |  7371 | `` * php's `prototype`: the ROOT-most declaration the entry in pClass's method`` |
|      - |  7372 | ` * table answers to, or NULL.` |
|      - |  7373 | ` *` |
|      - |  7374 | `` * zend assigns it once, at link time, as `child->prototype = parent->prototype`` |
|      - |  7375 | `` * ? parent->prototype : parent` — so it CHAINS past every intermediate`` |
|      - |  7376 | ` * override and names the class where the contract began, not the nearest one` |
|      - |  7377 | `` * (`AppendIterator::current` is `prototype Iterator`, three classes up, where`` |
|      - |  7378 | ` * this engine answered its immediate parent). Two rules ride on it, both` |
|      - |  7379 | ` * measured against php 8.5.9:` |
|      - |  7380 | ` *` |
|      - |  7381 | ` *   - An INTERFACE wins over the parent chain. zend inherits from the parent` |
|      - |  7382 | ` *     class first and implements the interfaces after, and each implementation` |
|      - |  7383 | `` *     re-assigns the prototype — so `class C extends B implements I`, with both`` |
|      - |  7384 | ` *     declaring f(), reports I and not B.` |
|      - |  7385 | ` *   - A CONSTRUCTOR takes one only where the contract is really a contract:` |
|      - |  7386 | ` *     the chain link that would have STARTED it is dropped unless it is` |
|      - |  7387 | ` *     abstract (an interface's ctor is abstract too). An inherited prototype` |
|      - |  7388 | ` *     still rides through, so a ctor three deep from an abstract one keeps it.` |
|      - |  7389 | ` */` |
|   8400 |  7390 | `static ph7_class * ReflectPrototypeOf(ph7_context *pCtx, ph7_class *pClass,` |
|      - |  7391 | `	const char *zName, int nName, int iDepth)` |
|      3 |  7392 | `{` |
|   8403 |  7393 | `	ph7_class *pWalk, *pDecl, *pRes = 0;` |
|      - |  7394 | `	SyHashEntry *pOwn;` |
|      - |  7395 | `	int bCtor;` |
|   8403 |  7396 | `	if( pClass == 0 \|\| nName < 1 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|    ! 0 |  7397 | `		return 0;` |
|      - |  7398 | `	}` |
|   8403 |  7399 | `	pOwn = ReflectFindMethodEntry(pClass, zName, nName);` |
|   8403 |  7400 | `	if( pOwn == 0 ){` |
|    ! 0 |  7401 | `		return 0;` |
|      - |  7402 | `	}` |
|   9066 |  7403 | `	bCtor = nName == sizeof("__construct")-1` |
|   8400 |  7404 | `		&& SyStrnicmp(zName, "__construct", sizeof("__construct")-1) == 0;` |
|   8403 |  7405 | `	pDecl = ReflectMethodDeclClass(pClass, (ph7_class_method *)pOwn->pUserData);` |
|   8403 |  7406 | `	if( pDecl != pClass ){` |
|      - |  7407 | `		/* The class did not declare this one: zend copies the record in and` |
|      - |  7408 | `		 * assigns NOTHING, so whatever prototype the record already carries is` |
|      - |  7409 | ``		 * what a reflector reads here. `DOMAttr::C14N` therefore has none at`` |
|      - |  7410 | `		 * all, where this engine named the nearest declaring base. */` |
|   2430 |  7411 | `		pRes = ReflectPrototypeOf(pCtx, pDecl, zName, nName, iDepth + 1);` |
|   7189 |  7412 | `	}else if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      - |  7413 | `		/* Its own declaration, checked against the parent's at link time. An` |
|      - |  7414 | `		 * interface has no such link — its parents are declared parents, and` |
|      - |  7415 | `		 * are walked with the interface list below. */` |
|   4889 |  7416 | `		for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){` |
|    913 |  7417 | `			SyHashEntry *pEntry = ReflectFindMethodEntry(pWalk, zName, nName);` |
|      - |  7418 | `			ph7_class_method *pMeth;` |
|    913 |  7419 | `			if( pEntry == 0 ){` |
|    551 |  7420 | `				continue;` |
|      - |  7421 | `			}` |
|    363 |  7422 | `			pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    363 |  7423 | `			if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|    ! 0 |  7424 | `				break;` |
|      - |  7425 | `			}` |
|    363 |  7426 | `			pRes = ReflectPrototypeOf(pCtx, pWalk, zName, nName, iDepth + 1);` |
|    363 |  7427 | `			if( pRes == 0 && !(bCtor && (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0) ){` |
|    123 |  7428 | `				pRes = ReflectMethodDeclClass(pWalk, pMeth);` |
|     61 |  7429 | `			}` |
|    363 |  7430 | `			break;` |
|    ! 0 |  7431 | `		}` |
|   2168 |  7432 | `	}` |
|      - |  7433 | `	/* Each interface this class DECLARES re-assigns it, the last one winning —` |
|      - |  7434 | `	 * which is why an interface beats the parent chain. PHL keeps the first` |
|      - |  7435 | `	 * parent of an INTERFACE on the base chain, so it joins the list here. */` |
|      - |  7436 | `	{` |
|   8403 |  7437 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   8403 |  7438 | `		sxu32 n, nIface = SySetUsed(&pClass->aInterface);` |
|  22957 |  7439 | `		for( n = 0 ; n <= nIface ; n++ ){` |
|      - |  7440 | `			ph7_class *pIface;` |
|      - |  7441 | `			SyHashEntry *pEntry;` |
|  14557 |  7442 | `			if( n == nIface ){` |
|   8403 |  7443 | `				pIface = (pClass->iFlags & PH7_CLASS_INTERFACE) ? pClass->pBase : 0;` |
|   4203 |  7444 | `			}else{` |
|   6156 |  7445 | `				pIface = apIface[n];` |
|      - |  7446 | `			}` |
|  14557 |  7447 | `			if( pIface == 0 ){` |
|   6761 |  7448 | `				continue;` |
|      - |  7449 | `			}` |
|   7798 |  7450 | `			pEntry = ReflectFindMethodEntry(pIface, zName, nName);` |
|      - |  7451 | `			/* Nothing to check against when the class holds the interface's` |
|      - |  7452 | `			 * very own record: an interface that merely EXTENDS another copies` |
|      - |  7453 | ``			 * the method in and stops, so `interface B extends A` prints`` |
|      - |  7454 | ``			 * `inherits A` and no prototype at all. */`` |
|   7798 |  7455 | `			if( pEntry == 0 \|\| pOwn->pUserData == pEntry->pUserData ){` |
|   6366 |  7456 | `				continue;` |
|      - |  7457 | `			}` |
|   1434 |  7458 | `			pRes = ReflectPrototypeOf(pCtx, pIface, zName, nName, iDepth + 1);` |
|   1434 |  7459 | `			if( pRes == 0 ){` |
|   2150 |  7460 | `				pRes = ReflectMethodDeclClass(pIface,` |
|   1432 |  7461 | `					(ph7_class_method *)pEntry->pUserData);` |
|    716 |  7462 | `			}` |
|    718 |  7463 | `		}` |
|      - |  7464 | `	}` |
|   8403 |  7465 | `	return pRes;` |
|   4203 |  7466 | `}` |
|      - |  7467 | `/* The same question asked by a ReflectionMethod receiver, which carries the` |
|      - |  7468 | `` * method name on `$this`. The prototype belongs to the entry in the class the`` |
|      - |  7469 | ` * reflector was BUILT FOR, which is php's anchor for it too. */` |
|    146 |  7470 | `static ph7_class * ReflectMethodPrototype(ph7_context *pCtx, ReflectFuncRef *pRef)` |
|      1 |  7471 | `{` |
|    147 |  7472 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    147 |  7473 | `	ph7_class *pOwner = ReflectOwnerOfThis(pCtx);` |
|    147 |  7474 | `	const char *zName = "";` |
|    147 |  7475 | `	int nName = 0;` |
|    147 |  7476 | `	if( pThis == 0 ){` |
|    ! 0 |  7477 | `		return 0;` |
|      - |  7478 | `	}` |
|    147 |  7479 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    147 |  7480 | `	return ReflectPrototypeOf(pCtx, pOwner ? pOwner : pRef->pClass, zName, nName, 0);` |
|     74 |  7481 | `}` |
|     94 |  7482 | `static int vm_builtin_ReflectionMethod_hasPrototype(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7483 | `{` |
|      - |  7484 | `	ReflectFuncRef sRef;` |
|     47 |  7485 | `	SXUNUSED(nArg);` |
|     47 |  7486 | `	SXUNUSED(apArg);` |
|     95 |  7487 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 |  7488 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  7489 | `		return PH7_OK;` |
|      - |  7490 | `	}` |
|     95 |  7491 | `	ph7_result_bool(pCtx, ReflectMethodPrototype(pCtx, &sRef) != 0);` |
|     95 |  7492 | `	return PH7_OK;` |
|     48 |  7493 | `}` |
|     52 |  7494 | `static int vm_builtin_ReflectionMethod_getPrototype(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7495 | `{` |
|     53 |  7496 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7497 | `	ReflectFuncRef sRef;` |
|      - |  7498 | `	ph7_class *pProto;` |
|     53 |  7499 | `	const char *zClass = "", *zName = "";` |
|     53 |  7500 | `	int nClass = 0, nName = 0;` |
|     26 |  7501 | `	SXUNUSED(nArg);` |
|     26 |  7502 | `	SXUNUSED(apArg);` |
|     53 |  7503 | `	if( pThis == 0 \|\| !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 |  7504 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7505 | `		return PH7_OK;` |
|      - |  7506 | `	}` |
|     53 |  7507 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     53 |  7508 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     53 |  7509 | `	pProto = ReflectMethodPrototype(pCtx, &sRef);` |
|     53 |  7510 | `	if( pProto == 0 ){` |
|      7 |  7511 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 |  7512 | `			"Method %.*s::%.*s does not have a prototype", nClass, zClass, nName, zName);` |
|      - |  7513 | `	}` |
|      - |  7514 | `	{` |
|      - |  7515 | `		ph7_value sClass, sName;` |
|      - |  7516 | `		ph7_value *apCtor[2];` |
|      - |  7517 | `		ph7_class_instance *pOut;` |
|      - |  7518 | `		sxi32 rc;` |
|     49 |  7519 | `		PH7_MemObjInit(pCtx->pVm, &sClass);` |
|     49 |  7520 | `		PH7_MemObjInit(pCtx->pVm, &sName);` |
|     49 |  7521 | `		ph7_value_string(&sClass, SyStringData(&pProto->sName), (int)SyStringLength(&pProto->sName));` |
|     49 |  7522 | `		ph7_value_string(&sName, zName, nName);` |
|     49 |  7523 | `		apCtor[0] = &sClass;` |
|     49 |  7524 | `		apCtor[1] = &sName;` |
|     49 |  7525 | `		pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|     49 |  7526 | `		PH7_MemObjRelease(&sClass);` |
|     49 |  7527 | `		PH7_MemObjRelease(&sName);` |
|     49 |  7528 | `		if( pOut == 0 ){` |
|    ! 0 |  7529 | `			if( rc != PH7_OK ){` |
|    ! 0 |  7530 | `				return rc;` |
|      - |  7531 | `			}` |
|    ! 0 |  7532 | `			ph7_result_null(pCtx);` |
|    ! 0 |  7533 | `			return PH7_OK;` |
|      - |  7534 | `		}` |
|     49 |  7535 | `		return ReflectResultObject(pCtx, pOut);` |
|      - |  7536 | `	}` |
|     27 |  7537 | `}` |
|      - |  7538 | `/* ---- ReflectionParameter ---- */` |
|      - |  7539 | `/*` |
|      - |  7540 | ``  * The function a ReflectionParameter belongs to, from its own `__t`/`__m` `` |
|      - |  7541 | `` * slots. `__t` holds a Closure, a class name or a function name; `__m` the`` |
|      - |  7542 | ` * method name, or null.` |
|      - |  7543 | ` */` |
|   1996 |  7544 | `static int ReflectParamOwner(ph7_context *pCtx, ReflectFuncRef *pRef, ReflectParamDesc *pDesc)` |
|      4 |  7545 | `{` |
|   2000 |  7546 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7547 | `	ph7_value *pT, *pM;` |
|      - |  7548 | `	int rc;` |
|   2000 |  7549 | `	SyZero(pRef, sizeof(*pRef));` |
|   2000 |  7550 | `	if( pDesc ){` |
|   1944 |  7551 | `		SyZero(pDesc, sizeof(*pDesc));` |
|    970 |  7552 | `	}` |
|   2000 |  7553 | `	if( pThis == 0 ){` |
|    ! 0 |  7554 | `		return 0;` |
|      - |  7555 | `	}` |
|   2000 |  7556 | `	pT = PH7_NativeAttr(pThis, RP_T);` |
|   2000 |  7557 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|   2000 |  7558 | `	if( pT == 0 ){` |
|    ! 0 |  7559 | `		return 0;` |
|      - |  7560 | `	}` |
|   2000 |  7561 | `	rc = ReflectFuncFill(pCtx->pVm, pT, (pM && (pM->iFlags & MEMOBJ_STRING)) ? pM : 0, pRef);` |
|   2000 |  7562 | `	if( rc == 0 \|\| pDesc == 0 ){` |
|     57 |  7563 | `		return rc;` |
|      - |  7564 | `	}` |
|   1944 |  7565 | `	return ReflectParamAt(pRef, (int)PH7_NativeAttrInt(pThis, RP_P), pDesc);` |
|   1002 |  7566 | `}` |
|      - |  7567 | `/* ReflectionParameter::__construct($function, string\|int $param) */` |
|   1246 |  7568 | `static int vm_builtin_ReflectionParameter_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  7569 | `{` |
|   1250 |  7570 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1250 |  7571 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7572 | `	ReflectFuncRef sRef;` |
|      - |  7573 | `	ReflectParamDesc sDesc;` |
|      - |  7574 | `	ph7_value sTarget, sMethod;` |
|   1250 |  7575 | `	int nTotal, iFound = -1, rc = PH7_OK;` |
|   1250 |  7576 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 |  7577 | `		return PH7_OK;` |
|      - |  7578 | `	}` |
|   1250 |  7579 | `	SyZero(&sDesc, sizeof(sDesc));` |
|   1250 |  7580 | `	PH7_MemObjInit(pVm, &sTarget);` |
|   1250 |  7581 | `	PH7_MemObjInit(pVm, &sMethod);` |
|      - |  7582 | ``	/* `[$objOrClass, $method]`, `"C::m"` and a plain function/Closure are the`` |
|      - |  7583 | `	 * three spellings php accepts. */` |
|   1250 |  7584 | `	if( ph7_value_is_array(apArg[0]) ){` |
|    430 |  7585 | `		ph7_value *pA = ph7_array_fetch(apArg[0], "0", 1);` |
|    430 |  7586 | `		ph7_value *pB = ph7_array_fetch(apArg[0], "1", 1);` |
|    430 |  7587 | `		if( pA && (pA->iFlags & MEMOBJ_OBJ) ){` |
|    ! 0 |  7588 | `			ph7_class_instance *pObj = (ph7_class_instance *)pA->x.pOther;` |
|    ! 0 |  7589 | `			ph7_value_string(&sTarget, SyStringData(&pObj->pClass->sName),` |
|    ! 0 |  7590 | `				(int)SyStringLength(&pObj->pClass->sName));` |
|    430 |  7591 | `		}else if( pA ){` |
|    430 |  7592 | `			PH7_MemObjStore(pA, &sTarget);` |
|    213 |  7593 | `		}` |
|    430 |  7594 | `		if( pB ){` |
|    430 |  7595 | `			PH7_MemObjStore(pB, &sMethod);` |
|    213 |  7596 | `		}` |
|    217 |  7597 | `	}else{` |
|      - |  7598 | ``		/* A STRING is a plain function name, `"C::m"` included: php does not`` |
|      - |  7599 | ``		 * split one here (it reports `Function C::m() does not exist`), unlike`` |
|      - |  7600 | `		 * ReflectionMethod's own one-argument form. The prelude split it and` |
|      - |  7601 | `		 * silently reflected the method. */` |
|    824 |  7602 | `		PH7_MemObjStore(apArg[0], &sTarget);` |
|      - |  7603 | `	}` |
|   1250 |  7604 | `	if( !ReflectFuncFill(pCtx->pVm, &sTarget, (sMethod.iFlags & MEMOBJ_STRING) ? &sMethod : 0, &sRef) ){` |
|      - |  7605 | `		const char *zName;` |
|      - |  7606 | `		int nName;` |
|      3 |  7607 | `		if( sMethod.iFlags & MEMOBJ_STRING ){` |
|      - |  7608 | `			const char *zM;` |
|      - |  7609 | `			int nM;` |
|    ! 0 |  7610 | `			zName = ph7_value_to_string(&sTarget, &nName);` |
|    ! 0 |  7611 | `			zM = ph7_value_to_string(&sMethod, &nM);` |
|    ! 0 |  7612 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 |  7613 | `				"Method %.*s::%.*s() does not exist", nName, zName, nM, zM);` |
|    ! 0 |  7614 | `		}else{` |
|      3 |  7615 | `			zName = ph7_value_to_string(&sTarget, &nName);` |
|      4 |  7616 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  7617 | `				"Function %.*s() does not exist", nName, zName);` |
|      - |  7618 | `		}` |
|      3 |  7619 | `		goto Done;` |
|      - |  7620 | `	}` |
|   1248 |  7621 | `	nTotal = ReflectParamCount(&sRef);` |
|   1248 |  7622 | `	if( apArg[1]->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|   1238 |  7623 | `		int iWant = (int)ph7_value_to_int(apArg[1]);` |
|   1238 |  7624 | `		if( iWant >= 0 && iWant < nTotal && ReflectParamAt(&sRef, iWant, &sDesc) ){` |
|   1236 |  7625 | `			iFound = iWant;` |
|    616 |  7626 | `		}` |
|   1238 |  7627 | `		if( iFound < 0 ){` |
|      3 |  7628 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  7629 | `				"The parameter specified by its offset could not be found");` |
|      3 |  7630 | `			goto Done;` |
|      - |  7631 | `		}` |
|    620 |  7632 | `	}else{` |
|      - |  7633 | `		const char *zWant;` |
|      - |  7634 | `		int nWant, n;` |
|     11 |  7635 | `		zWant = ph7_value_to_string(apArg[1], &nWant);` |
|     33 |  7636 | `		for( n = 0 ; n < nTotal ; n++ ){` |
|     30 |  7637 | `			if( ReflectParamAt(&sRef, n, &sDesc)` |
|     30 |  7638 | `			 && SyStringLength(&sDesc.sName) == (sxu32)nWant` |
|     23 |  7639 | `			 && SyMemcmp(SyStringData(&sDesc.sName), zWant, (sxu32)nWant) == 0 ){` |
|      9 |  7640 | `				iFound = n;` |
|      9 |  7641 | `				break;` |
|      - |  7642 | `			}` |
|     12 |  7643 | `		}` |
|     11 |  7644 | `		if( iFound < 0 ){` |
|      3 |  7645 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  7646 | `				"The parameter specified by its name could not be found");` |
|      3 |  7647 | `			goto Done;` |
|      - |  7648 | `		}` |
|      - |  7649 | `	}` |
|   1864 |  7650 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sDesc.sName),` |
|   1240 |  7651 | `		(int)SyStringLength(&sDesc.sName));` |
|      - |  7652 | `	/* Record the CANONICAL target: the class the method really came through and` |
|      - |  7653 | `	 * its declared spelling, so a re-resolve cannot land somewhere else. */` |
|   1244 |  7654 | `	if( sRef.pMeth && sRef.pClass ){` |
|    652 |  7655 | `		PH7_NativeSetAttrStr(pVm, pThis, RP_T, SyStringData(&sRef.pClass->sName),` |
|    432 |  7656 | `			(int)SyStringLength(&sRef.pClass->sName));` |
|    652 |  7657 | `		PH7_NativeSetAttrStr(pVm, pThis, RP_M, SyStringData(&sRef.pMeth->sFunc.sName),` |
|    432 |  7658 | `			(int)SyStringLength(&sRef.pMeth->sFunc.sName));` |
|    220 |  7659 | `	}else{` |
|    812 |  7660 | `		ph7_value *pSlot = PH7_NativeAttr(pThis, RP_T);` |
|    812 |  7661 | `		if( pSlot ){` |
|    812 |  7662 | `			PH7_MemObjStore(&sTarget, pSlot);` |
|    404 |  7663 | `		}` |
|      - |  7664 | `	}` |
|   1244 |  7665 | `	PH7_NativeSetAttrInt(pVm, pThis, RP_P, (sxi64)iFound);` |
|    623 |  7666 | `Done:` |
|   1250 |  7667 | `	PH7_MemObjRelease(&sTarget);` |
|   1250 |  7668 | `	PH7_MemObjRelease(&sMethod);` |
|   1250 |  7669 | `	return rc;` |
|    627 |  7670 | `}` |
|    726 |  7671 | `static int vm_builtin_ReflectionParameter_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  7672 | `{` |
|    729 |  7673 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    729 |  7674 | `	const char *zName = "";` |
|    729 |  7675 | `	int nName = 0;` |
|    363 |  7676 | `	SXUNUSED(nArg);` |
|    363 |  7677 | `	SXUNUSED(apArg);` |
|    729 |  7678 | `	if( pThis ){` |
|    729 |  7679 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    363 |  7680 | `	}` |
|    729 |  7681 | `	ph7_result_string(pCtx, zName, nName);` |
|    729 |  7682 | `	return PH7_OK;` |
|      3 |  7683 | `}` |
|     52 |  7684 | `static int vm_builtin_ReflectionParameter_getPosition(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7685 | `{` |
|     53 |  7686 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     26 |  7687 | `	SXUNUSED(nArg);` |
|     26 |  7688 | `	SXUNUSED(apArg);` |
|     53 |  7689 | `	ph7_result_int64(pCtx, pThis ? PH7_NativeAttrInt(pThis, RP_P) : 0);` |
|     53 |  7690 | `	return PH7_OK;` |
|      1 |  7691 | `}` |
|      - |  7692 | `/* The boolean predicates that read one flag of the description. */` |
|    598 |  7693 | `static int ReflectParamFlag(ph7_context *pCtx, int iWhat)` |
|      2 |  7694 | `{` |
|      - |  7695 | `	ReflectFuncRef sRef;` |
|      - |  7696 | `	ReflectParamDesc sDesc;` |
|    600 |  7697 | `	int bYes = 0;` |
|    600 |  7698 | `	if( ReflectParamOwner(pCtx, &sRef, &sDesc) ){` |
|    600 |  7699 | `		switch( iWhat ){` |
|     25 |  7700 | `		case 0: bYes = sDesc.bByRef; break;` |
|      3 |  7701 | `		case 1: bYes = !sDesc.bByRef; break;` |
|     45 |  7702 | `		case 2: bYes = sDesc.bVariadic; break;` |
|    ! 0 |  7703 | `		case 3: bYes = sDesc.bPromoted; break;` |
|    440 |  7704 | `		case 4: bYes = sDesc.bHasDef; break;` |
|     81 |  7705 | `		case 5: bYes = SyStringLength(&sDesc.sType) > 0; break;` |
|      5 |  7706 | `		default:` |
|      - |  7707 | `			/* allowsNull(): untyped, nullable, or a type that INCLUDES null */` |
|     13 |  7708 | `			bYes = SyStringLength(&sDesc.sType) < 1 \|\| sDesc.bNullable` |
|      7 |  7709 | `				\|\| ReflectTypeNameIs(SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType), "mixed")` |
|     14 |  7710 | `				\|\| ReflectTypeNameIs(SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType), "null");` |
|     10 |  7711 | `			break;` |
|      - |  7712 | `		}` |
|    301 |  7713 | `	}else if( iWhat == 1 \|\| iWhat == 6 ){` |
|    ! 0 |  7714 | `		bYes = 1;` |
|    ! 0 |  7715 | `	}` |
|    600 |  7716 | `	ph7_result_bool(pCtx, bYes);` |
|    600 |  7717 | `	return PH7_OK;` |
|      2 |  7718 | `}` |
|      - |  7719 | `#define REFLECT_PARAM_FLAG(NAME,WHAT) \` |
|      - |  7720 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  7721 | `	{ \` |
|      - |  7722 | `		SXUNUSED(nArg); \` |
|      - |  7723 | `		SXUNUSED(apArg); \` |
|      - |  7724 | `		return ReflectParamFlag(pCtx, WHAT); \` |
|      - |  7725 | `	}` |
|     25 |  7726 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isPassedByReference, 0)` |
|      3 |  7727 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_canBePassedByValue, 1)` |
|     45 |  7728 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isVariadic, 2)` |
|    ! 0 |  7729 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isPromoted, 3)` |
|    440 |  7730 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isDefaultValueAvailable, 4)` |
|     81 |  7731 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_hasType, 5)` |
|     11 |  7732 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_allowsNull, 6)` |
|      - |  7733 |  |
|      - |  7734 | `/* isOptional(): every parameter from here on has to be omissible too. */` |
|     52 |  7735 | `static int vm_builtin_ReflectionParameter_isOptional(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7736 | `{` |
|     53 |  7737 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  7738 | `	ReflectFuncRef sRef;` |
|      - |  7739 | `	ReflectParamDesc sDesc;` |
|      - |  7740 | `	int n, nTotal, iPos;` |
|     26 |  7741 | `	SXUNUSED(nArg);` |
|     26 |  7742 | `	SXUNUSED(apArg);` |
|     53 |  7743 | `	if( pThis == 0 \|\| !ReflectParamOwner(pCtx, &sRef, 0) ){` |
|    ! 0 |  7744 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  7745 | `		return PH7_OK;` |
|      - |  7746 | `	}` |
|     53 |  7747 | `	iPos = (int)PH7_NativeAttrInt(pThis, RP_P);` |
|     53 |  7748 | `	nTotal = ReflectParamCount(&sRef);` |
|    101 |  7749 | `	for( n = iPos ; n < nTotal ; n++ ){` |
|     71 |  7750 | `		if( ReflectParamAt(&sRef, n, &sDesc) && !sDesc.bVariadic && !sDesc.bOptional ){` |
|     23 |  7751 | `			ph7_result_bool(pCtx, 0);` |
|     23 |  7752 | `			return PH7_OK;` |
|      - |  7753 | `		}` |
|     25 |  7754 | `	}` |
|     31 |  7755 | `	ph7_result_bool(pCtx, 1);` |
|     31 |  7756 | `	return PH7_OK;` |
|     27 |  7757 | `}` |
|    746 |  7758 | `static int vm_builtin_ReflectionParameter_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  7759 | `{` |
|      - |  7760 | `	ReflectFuncRef sRef;` |
|      - |  7761 | `	ReflectParamDesc sDesc;` |
|    373 |  7762 | `	SXUNUSED(nArg);` |
|    373 |  7763 | `	SXUNUSED(apArg);` |
|    750 |  7764 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| SyStringLength(&sDesc.sType) < 1 ){` |
|      3 |  7765 | `		ph7_result_null(pCtx);` |
|      3 |  7766 | `		return PH7_OK;` |
|      - |  7767 | `	}` |
|   1120 |  7768 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx,` |
|    744 |  7769 | `		SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType)));` |
|    377 |  7770 | `}` |
|      - |  7771 | `/*` |
|      - |  7772 | ` * getClass()/isArray()/isCallable() — php 8.0 deprecated these in favour of` |
|      - |  7773 | ` * getType() but still declares them and still answers. The E_DEPRECATED that` |
|      - |  7774 | ` * comes with a call is raised at the CALL now, from the one table every` |
|      - |  7775 | ` * deprecated internal name is stamped from (aDeprecatedFunc[]), which is where` |
|      - |  7776 | ` * php raises it too — before the callee's own screens rather than inside it.` |
|      - |  7777 | ` */` |
|      8 |  7778 | `static int vm_builtin_ReflectionParameter_getClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7779 | `{` |
|      - |  7780 | `	ReflectFuncRef sRef;` |
|      - |  7781 | `	ReflectParamDesc sDesc;` |
|      - |  7782 | `	const char *zType;` |
|      - |  7783 | `	int nType;` |
|      4 |  7784 | `	SXUNUSED(nArg);` |
|      4 |  7785 | `	SXUNUSED(apArg);` |
|      9 |  7786 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| SyStringLength(&sDesc.sType) < 1 ){` |
|    ! 0 |  7787 | `		ph7_result_null(pCtx);` |
|    ! 0 |  7788 | `		return PH7_OK;` |
|      - |  7789 | `	}` |
|      9 |  7790 | `	zType = SyStringData(&sDesc.sType);` |
|      9 |  7791 | `	nType = (int)SyStringLength(&sDesc.sType);` |
|      9 |  7792 | `	if( nType > 0 && zType[0] == '?' ){` |
|      3 |  7793 | `		zType++;` |
|      3 |  7794 | `		nType--;` |
|      1 |  7795 | `	}` |
|      9 |  7796 | `	if( ReflectTypeIsBuiltin(zType, nType) ){` |
|      7 |  7797 | `		ph7_result_null(pCtx);` |
|      7 |  7798 | `		return PH7_OK;` |
|      - |  7799 | `	}` |
|      3 |  7800 | `	return ReflectResultClassOf(pCtx, PH7_VmExtractClass(pCtx->pVm, zType, (sxu32)nType, FALSE, 0));` |
|      5 |  7801 | `}` |
|     16 |  7802 | `static int ReflectParamTypeIs(ph7_context *pCtx, const char *zWant)` |
|      1 |  7803 | `{` |
|      - |  7804 | `	ReflectFuncRef sRef;` |
|      - |  7805 | `	ReflectParamDesc sDesc;` |
|     17 |  7806 | `	int bYes = 0;` |
|     17 |  7807 | `	if( ReflectParamOwner(pCtx, &sRef, &sDesc) && SyStringLength(&sDesc.sType) > 0 ){` |
|     17 |  7808 | `		const char *zType = SyStringData(&sDesc.sType);` |
|     17 |  7809 | `		int nType = (int)SyStringLength(&sDesc.sType);` |
|      - |  7810 | ``		/* php answers true for `?array` too: the deprecated pair asks about the`` |
|      - |  7811 | `		 * type ATOM, and nullability is a separate question. */` |
|     17 |  7812 | `		if( zType[0] == '?' ){` |
|      5 |  7813 | `			zType++;` |
|      5 |  7814 | `			nType--;` |
|      2 |  7815 | `		}` |
|     17 |  7816 | `		bYes = ReflectTypeNameIs(zType, nType, zWant);` |
|      8 |  7817 | `	}` |
|     17 |  7818 | `	ph7_result_bool(pCtx, bYes);` |
|     17 |  7819 | `	return PH7_OK;` |
|      1 |  7820 | `}` |
|      8 |  7821 | `static int vm_builtin_ReflectionParameter_isArray(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7822 | `{` |
|      4 |  7823 | `	SXUNUSED(nArg);` |
|      4 |  7824 | `	SXUNUSED(apArg);` |
|      9 |  7825 | `	return ReflectParamTypeIs(pCtx, "array");` |
|      1 |  7826 | `}` |
|      8 |  7827 | `static int vm_builtin_ReflectionParameter_isCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  7828 | `{` |
|      4 |  7829 | `	SXUNUSED(nArg);` |
|      4 |  7830 | `	SXUNUSED(apArg);` |
|      9 |  7831 | `	return ReflectParamTypeIs(pCtx, "callable");` |
|      1 |  7832 | `}` |
|      - |  7833 | `/*` |
|      - |  7834 | `` * The class `self`/`parent` resolve against inside a parameter's default: the one`` |
|      - |  7835 | ` * that DECLARED the method (a trait's members belong to the composing class). 0 for` |
|      - |  7836 | ` * a plain function, whose defaults can name neither.` |
|      - |  7837 | ` */` |
|     28 |  7838 | `static ph7_class * ReflectParamSelfClass(const ReflectFuncRef *pRef)` |
|      1 |  7839 | `{` |
|     29 |  7840 | `	if( pRef->pMeth == 0 ){` |
|      7 |  7841 | `		return 0;` |
|      - |  7842 | `	}` |
|     23 |  7843 | `	return PH7_VmMemberOwnerClass((ph7_class *)pRef->pMeth->sFunc.pUserData,` |
|     22 |  7844 | `		pRef->pClass ? pRef->pClass : (ph7_class *)pRef->pMeth->sFunc.pUserData);` |
|     15 |  7845 | `}` |
|    234 |  7846 | `static int vm_builtin_ReflectionParameter_getDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7847 | `{` |
|      - |  7848 | `	ReflectFuncRef sRef;` |
|      - |  7849 | `	ReflectParamDesc sDesc;` |
|    117 |  7850 | `	SXUNUSED(nArg);` |
|    117 |  7851 | `	SXUNUSED(apArg);` |
|    236 |  7852 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|      5 |  7853 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  7854 | `			"Internal error: Failed to retrieve the default value");` |
|      - |  7855 | `	}` |
|    232 |  7856 | `	if( sDesc.pArg ){` |
|      - |  7857 | `		/* Compiled: the same evaluation path the VM uses for an omitted argument --` |
|      - |  7858 | `		 * except that one runs INSIDE the call, where the frame already names the` |
|      - |  7859 | ``		 * class `self::K` resolves against. Reflection has no such frame, so a`` |
|      - |  7860 | ``		 * default written `self::K` answered `Class "self" not found` where php`` |
|      - |  7861 | `		 * answers its value. Mark the declaring class the way a member` |
|      - |  7862 | `		 * initializer's evaluation does (PH7_VmPeekDeclaringClass reads the pair). */` |
|      - |  7863 | `		ph7_value sValue;` |
|     29 |  7864 | `		ph7_class *pSaveCls = pCtx->pVm->pConstEvalClass;` |
|     29 |  7865 | `		void *pSaveFrame = pCtx->pVm->pConstEvalFrame;` |
|     29 |  7866 | `		ph7_class *pDecl = ReflectParamSelfClass(&sRef);` |
|     29 |  7867 | `		if( pDecl ){` |
|     23 |  7868 | `			pCtx->pVm->pConstEvalClass = pDecl;` |
|     23 |  7869 | `			pCtx->pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pCtx->pVm->pFrame);` |
|     11 |  7870 | `		}` |
|     29 |  7871 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     29 |  7872 | `		VmLocalExec(pCtx->pVm, &sDesc.pArg->aByteCode, &sValue, FALSE);` |
|     29 |  7873 | `		pCtx->pVm->pConstEvalClass = pSaveCls;` |
|     29 |  7874 | `		pCtx->pVm->pConstEvalFrame = pSaveFrame;` |
|     29 |  7875 | `		ph7_result_value(pCtx, &sValue);` |
|     29 |  7876 | `		PH7_MemObjRelease(&sValue);` |
|     29 |  7877 | `		return PH7_OK;` |
|      - |  7878 | `	}` |
|      - |  7879 | `	{` |
|      - |  7880 | `		/* Declared: the signature's default TEXT, reduced. */` |
|    204 |  7881 | `		const char *zDef = SyStringData(&sDesc.sDefText);` |
|    204 |  7882 | `		int nDef = (int)SyStringLength(&sDesc.sDefText);` |
|    204 |  7883 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    204 |  7884 | `		if( pVal && ReflectSigScalar(pCtx, zDef, nDef, pVal) ){` |
|    192 |  7885 | `			ph7_result_value(pCtx, pVal);` |
|    192 |  7886 | `			return PH7_OK;` |
|      - |  7887 | `		}` |
|     12 |  7888 | `		if( (nDef >= 1 && zDef[0] == '[')` |
|      7 |  7889 | `		 \|\| (nDef >= (int)sizeof("array (")-1` |
|    ! 0 |  7890 | `		  && SyMemcmp(zDef, "array (", sizeof("array (")-1) == 0) ){` |
|     13 |  7891 | `			ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|     13 |  7892 | `			return PH7_OK;` |
|      - |  7893 | `		}` |
|      - |  7894 | `	}` |
|    ! 0 |  7895 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  7896 | `		"Internal error: Failed to retrieve the default value");` |
|    119 |  7897 | `}` |
|      - |  7898 | `/*` |
|      - |  7899 | ` * A default that is a plain global-constant reference compiles to exactly` |
|      - |  7900 | ` * [ OP_LOADC (EXPAND), OP_DONE ] with the name in the literal table.` |
|      - |  7901 | ` *` |
|      - |  7902 | ` * A DECLARED default is TEXT, and php answers the same two questions about one:` |
|      - |  7903 | `` * `int $type = PDO::PARAM_STR` is a constant default and its name is what the`` |
|      - |  7904 | `` * stub wrote. Only a SINGLE reference counts -- the `\|` fold beside it`` |
|      - |  7905 | ` * evaluates to a number that no constant carries, and php answers false and` |
|      - |  7906 | ` * null for it while still printing the source in the export.` |
|      - |  7907 | ` */` |
|   1486 |  7908 | `static int ReflectParamDefConst(ph7_context *pCtx, ReflectParamDesc *pDesc,` |
|      - |  7909 | `	const char **pz, int *pn)` |
|      5 |  7910 | `{` |
|      - |  7911 | `	VmInstr *aInstr;` |
|      - |  7912 | `	ph7_value *pLit;` |
|   1491 |  7913 | `	if( pDesc->pArg == 0 && pDesc->bHasDef ){` |
|   1361 |  7914 | `		const char *zDef = SyStringData(&pDesc->sDefText);` |
|   1361 |  7915 | `		int nDef = (int)SyStringLength(&pDesc->sDefText);` |
|      - |  7916 | `		ph7_value *pVal;` |
|   1361 |  7917 | `		ReflectSigTrim(&zDef, &nDef);` |
|   1361 |  7918 | `		if( nDef < 1 \|\| ReflectSigHas(zDef, nDef, "\|", 1) ){` |
|     62 |  7919 | `			return 0;` |
|      - |  7920 | `		}` |
|   1301 |  7921 | `		pVal = ph7_context_new_scalar(pCtx);` |
|   1301 |  7922 | `		if( pVal == 0 ){` |
|    ! 0 |  7923 | `			return 0;` |
|      - |  7924 | `		}` |
|   1296 |  7925 | `		if( nDef > (int)sizeof("::class")-1` |
|    774 |  7926 | `		 && SyMemcmp(&zDef[nDef - (sizeof("::class")-1)], "::class",` |
|    121 |  7927 | `		             sizeof("::class")-1) == 0 ){` |
|      - |  7928 | ``			/* `C::class` reads a class NAME rather than a constant, and php`` |
|      - |  7929 | `			 * answers false for it while still printing it in the export. */` |
|     57 |  7930 | `			return 0;` |
|      - |  7931 | `		}` |
|   1242 |  7932 | `		if( !ReflectSigGlobalConst(pCtx, zDef, nDef, pVal)` |
|   1193 |  7933 | `		 && !ReflectSigClassConst(pCtx, zDef, nDef, pVal) ){` |
|   1073 |  7934 | `			return 0;` |
|      - |  7935 | `		}` |
|    179 |  7936 | `		*pz = zDef;` |
|    179 |  7937 | `		*pn = nDef;` |
|    179 |  7938 | `		return 1;` |
|      - |  7939 | `	}` |
|    131 |  7940 | `	if( pDesc->pArg == 0 ){` |
|    ! 0 |  7941 | `		return 0;` |
|      - |  7942 | `	}` |
|    131 |  7943 | `	if( SySetUsed(&pDesc->pArg->aByteCode) == 4 ){` |
|      - |  7944 | `` 		/* A CLASS constant compiles to the class name, the member name and the `::` `` |
|      - |  7945 | `		 * fetch: [ LOADC <class>, LOADC <member>, MEMBER(static, bareword), DONE ].` |
|      - |  7946 | `		 * php answers true for one and names it the way the source spelled it --` |
|      - |  7947 | ``		 * `self::K` stays `self::K` -- minus a leading `\`, which it drops. Only the`` |
|      - |  7948 | `		 * plain global-constant shape below was recognised, so every class-constant` |
|      - |  7949 | `		 * default reported itself as no constant at all. */` |
|     49 |  7950 | `		VmInstr *aCC = (VmInstr *)SySetBasePtr(&pDesc->pArg->aByteCode);` |
|     48 |  7951 | `		if( aCC[0].iOp == PH7_OP_LOADC && aCC[1].iOp == PH7_OP_LOADC` |
|     48 |  7952 | `		 && aCC[2].iOp == PH7_OP_MEMBER && aCC[2].iP1 == 1 && aCC[2].p3 == 0` |
|     47 |  7953 | `		 && aCC[3].iOp == PH7_OP_DONE ){` |
|     47 |  7954 | `			ph7_value *pCls = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj,aCC[0].iP2);` |
|     47 |  7955 | `			ph7_value *pMem = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj,aCC[1].iP2);` |
|     46 |  7956 | `			if( pCls && pMem && SyBlobLength(&pCls->sBlob) > 0` |
|     46 |  7957 | `			 && SyBlobLength(&pMem->sBlob) > 0` |
|     49 |  7958 | `			 && !(SyBlobLength(&pMem->sBlob) == sizeof("class")-1` |
|     25 |  7959 | `			   && SyStrnicmp((const char *)SyBlobData(&pMem->sBlob),"class",` |
|      2 |  7960 | `			                 sizeof("class")-1) == 0) ){` |
|      - |  7961 | ``				/* `C::class` reads a class NAME rather than a constant, and php`` |
|      - |  7962 | `				 * answers false for it -- the same rule the declared-signature path` |
|      - |  7963 | `				 * above makes. */` |
|     43 |  7964 | `				const char *zCls = (const char *)SyBlobData(&pCls->sBlob);` |
|     43 |  7965 | `				sxu32 nCls = SyBlobLength(&pCls->sBlob);` |
|     43 |  7966 | `				SyBlob *pOut = &pCtx->pVm->sReflectConstName;` |
|     43 |  7967 | `				while( nCls > 0 && zCls[0] == '\\' ){` |
|    ! 0 |  7968 | `					zCls++;` |
|    ! 0 |  7969 | `					nCls--;` |
|    ! 0 |  7970 | `				}` |
|     43 |  7971 | `				SyBlobReset(pOut);` |
|     43 |  7972 | `				SyBlobAppend(pOut,zCls,nCls);` |
|     43 |  7973 | `				SyBlobAppend(pOut,"::",sizeof("::")-1);` |
|     43 |  7974 | `				SyBlobAppend(pOut,SyBlobData(&pMem->sBlob),SyBlobLength(&pMem->sBlob));` |
|     43 |  7975 | `				*pz = (const char *)SyBlobData(pOut);` |
|     43 |  7976 | `				*pn = (int)SyBlobLength(pOut);` |
|     43 |  7977 | `				return 1;` |
|      - |  7978 | `			}` |
|      2 |  7979 | `		}` |
|      7 |  7980 | `		return 0;` |
|      - |  7981 | `	}` |
|     83 |  7982 | `	if( SySetUsed(&pDesc->pArg->aByteCode) != 2 ){` |
|      7 |  7983 | `		return 0;` |
|      - |  7984 | `	}` |
|     77 |  7985 | `	aInstr = (VmInstr *)SySetBasePtr(&pDesc->pArg->aByteCode);` |
|     76 |  7986 | `	if( aInstr[0].iOp != PH7_OP_LOADC \|\| (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0` |
|     46 |  7987 | `	 \|\| aInstr[1].iOp != PH7_OP_DONE ){` |
|     63 |  7988 | `		return 0;` |
|      - |  7989 | `	}` |
|     15 |  7990 | `	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);` |
|     15 |  7991 | `	if( pLit == 0 \|\| SyBlobLength(&pLit->sBlob) < 1 ){` |
|    ! 0 |  7992 | `		return 0;` |
|      - |  7993 | `	}` |
|     15 |  7994 | `	*pz = (const char *)SyBlobData(&pLit->sBlob);` |
|     15 |  7995 | `	*pn = (int)SyBlobLength(&pLit->sBlob);` |
|     15 |  7996 | `	return 1;` |
|    748 |  7997 | `}` |
|    128 |  7998 | `static int vm_builtin_ReflectionParameter_isDefaultValueConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  7999 | `{` |
|      - |  8000 | `	ReflectFuncRef sRef;` |
|      - |  8001 | `	ReflectParamDesc sDesc;` |
|      - |  8002 | `	const char *z;` |
|      - |  8003 | `	int n;` |
|     64 |  8004 | `	SXUNUSED(nArg);` |
|     64 |  8005 | `	SXUNUSED(apArg);` |
|    130 |  8006 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|      - |  8007 | `		/* php raises here rather than answering false: asking whether a default` |
|      - |  8008 | `		 * is a constant presupposes there IS one. The prelude answered false. */` |
|      3 |  8009 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  8010 | `			"Internal error: Failed to retrieve the default value");` |
|      - |  8011 | `	}` |
|    128 |  8012 | `	ph7_result_bool(pCtx, ReflectParamDefConst(pCtx, &sDesc, &z, &n) != 0);` |
|    128 |  8013 | `	return PH7_OK;` |
|     66 |  8014 | `}` |
|     64 |  8015 | `static int vm_builtin_ReflectionParameter_getDefaultValueConstantName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  8016 | `{` |
|      - |  8017 | `	ReflectFuncRef sRef;` |
|      - |  8018 | `	ReflectParamDesc sDesc;` |
|      - |  8019 | `	const char *z;` |
|      - |  8020 | `	int n;` |
|     32 |  8021 | `	SXUNUSED(nArg);` |
|     32 |  8022 | `	SXUNUSED(apArg);` |
|     66 |  8023 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|    ! 0 |  8024 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - |  8025 | `			"Internal error: Failed to retrieve the default value");` |
|      - |  8026 | `	}` |
|     66 |  8027 | `	if( ReflectParamDefConst(pCtx, &sDesc, &z, &n) ){` |
|     38 |  8028 | `		ph7_result_string(pCtx, z, n);` |
|     20 |  8029 | `	}else{` |
|     30 |  8030 | `		ph7_result_null(pCtx);` |
|      - |  8031 | `	}` |
|     66 |  8032 | `	return PH7_OK;` |
|     34 |  8033 | `}` |
|      4 |  8034 | `static int vm_builtin_ReflectionParameter_getDeclaringFunction(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8035 | `{` |
|      5 |  8036 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  8037 | `	ph7_value *pT, *pM;` |
|      - |  8038 | `	ph7_value *apCtor[2];` |
|      - |  8039 | `	ph7_class_instance *pOut;` |
|      - |  8040 | `	sxi32 rc;` |
|      2 |  8041 | `	SXUNUSED(nArg);` |
|      2 |  8042 | `	SXUNUSED(apArg);` |
|      5 |  8043 | `	if( pThis == 0 \|\| (pT = PH7_NativeAttr(pThis, RP_T)) == 0 ){` |
|    ! 0 |  8044 | `		ph7_result_null(pCtx);` |
|    ! 0 |  8045 | `		return PH7_OK;` |
|      - |  8046 | `	}` |
|      5 |  8047 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|      5 |  8048 | `	apCtor[0] = pT;` |
|      5 |  8049 | `	apCtor[1] = pM;` |
|      5 |  8050 | `	if( pM && (pM->iFlags & MEMOBJ_STRING) ){` |
|      3 |  8051 | `		pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|      2 |  8052 | `	}else{` |
|      3 |  8053 | `		pOut = ReflectConstruct(pCtx, "ReflectionFunction", 1, apCtor, &rc);` |
|      - |  8054 | `	}` |
|      5 |  8055 | `	if( pOut == 0 ){` |
|    ! 0 |  8056 | `		if( rc != PH7_OK ){` |
|    ! 0 |  8057 | `			return rc;` |
|      - |  8058 | `		}` |
|    ! 0 |  8059 | `		ph7_result_null(pCtx);` |
|    ! 0 |  8060 | `		return PH7_OK;` |
|      - |  8061 | `	}` |
|      5 |  8062 | `	return ReflectResultObject(pCtx, pOut);` |
|      3 |  8063 | `}` |
|      4 |  8064 | `static int vm_builtin_ReflectionParameter_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8065 | `{` |
|      - |  8066 | `	ReflectFuncRef sRef;` |
|      2 |  8067 | `	SXUNUSED(nArg);` |
|      2 |  8068 | `	SXUNUSED(apArg);` |
|      5 |  8069 | `	if( !ReflectParamOwner(pCtx, &sRef, 0) \|\| sRef.pMeth == 0 ){` |
|      3 |  8070 | `		ph7_result_null(pCtx);` |
|      3 |  8071 | `		return PH7_OK;` |
|      - |  8072 | `	}` |
|      3 |  8073 | `	return ReflectResultClassOf(pCtx, ReflectFuncDeclClass(&sRef));` |
|      3 |  8074 | `}` |
|      8 |  8075 | `static int vm_builtin_ReflectionParameter_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8076 | `{` |
|      9 |  8077 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  8078 | `	ReflectFuncRef sRef;` |
|      - |  8079 | `	ReflectParamDesc sDesc;` |
|      - |  8080 | `	ph7_value *pT, *pM;` |
|      9 |  8081 | `	const char *zMember = 0;` |
|      9 |  8082 | `	int nMember = 0;` |
|      9 |  8083 | `	if( pThis == 0 \|\| !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| sDesc.pArg == 0 ){` |
|    ! 0 |  8084 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  8085 | `		return PH7_OK;` |
|      - |  8086 | `	}` |
|      9 |  8087 | `	pT = PH7_NativeAttr(pThis, RP_T);` |
|      9 |  8088 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|      9 |  8089 | `	if( pM && (pM->iFlags & MEMOBJ_STRING) ){` |
|      7 |  8090 | `		zMember = (const char *)SyBlobData(&pM->sBlob);` |
|      7 |  8091 | `		nMember = (int)SyBlobLength(&pM->sBlob);` |
|      3 |  8092 | `	}` |
|      - |  8093 | `	/* 32 = Attribute::TARGET_PARAMETER */` |
|     13 |  8094 | `	return ReflectBuildAttrs(pCtx, &sDesc.pArg->aAttrs, "param", pT, zMember, nMember,` |
|      8 |  8095 | `		(int)PH7_NativeAttrInt(pThis, RP_P), 32, nArg, apArg);` |
|      5 |  8096 | `}` |
|    138 |  8097 | `static int vm_builtin_ReflectionParameter_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  8098 | `{` |
|     69 |  8099 | `	SXUNUSED(nArg);` |
|     69 |  8100 | `	SXUNUSED(apArg);` |
|    141 |  8101 | `	return ReflectExportParamSelf(pCtx);` |
|      3 |  8102 | `}` |
|      - |  8103 | `/*` |
|      - |  8104 | ` * Declare chunk 2. Called from PH7_VmInstallReflection() where it used to be` |
|      - |  8105 | `` * compiled — after chunk 1, whose `Reflector` these implement and whose`` |
|      - |  8106 | `` * `ReflectionClass` they answer.`` |
|      - |  8107 | ` *` |
|      - |  8108 | ` * The method tables are in php's own DECLARATION order.` |
|      - |  8109 | ` */` |
|      - |  8110 | `/*` |
|      - |  8111 | `` * PropertyHookType — php's `enum PropertyHookType: string { Get; Set; }`, the`` |
|      - |  8112 | ` * argument ReflectionProperty::getHook()/hasHook() take.` |
|      - |  8113 | ` */` |
|   6721 |  8114 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionHookType(ph7_vm *pVm)` |
|      5 |  8115 | `{` |
|      - |  8116 | `	static const PH7_NativeEnumCase aCase[] = {` |
|      - |  8117 | `		{ "Get", { 0, 0, PH7_NATIVE_VAL_STRING, 0, "get", 0.0 } },` |
|      - |  8118 | `		{ "Set", { 0, 0, PH7_NATIVE_VAL_STRING, 0, "set", 0.0 } },` |
|      - |  8119 | `	};` |
|   6726 |  8120 | `	return PH7_InstallNativeEnum(&(*pVm), "PropertyHookType", MEMOBJ_STRING,` |
|      - |  8121 | `		aCase, SX_ARRAYSIZE(aCase), 0, 0);` |
|      5 |  8122 | `}` |
|   6721 |  8123 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionFunc(ph7_vm *pVm)` |
|      5 |  8124 | `{` |
|      - |  8125 | `	static const PH7_NativePropDef aAbstractProp[] = {` |
|      - |  8126 | `		{ "name",  PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  8127 | `		/* PHL-only: the Closure being reflected. php reaches the same state from` |
|      - |  8128 | `		 * the function record itself; PHL has no hidden-slot bit yet (§7.4 (e)). */` |
|      - |  8129 | `		{ RF_CL,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  8130 | `	};` |
|      - |  8131 | `	static const PH7_NativeMethodDef aAbstractMethod[] = {` |
|      - |  8132 | `		{ "__clone",       PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - |  8133 | `		{ "inNamespace",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_inNamespace },` |
|      - |  8134 | `		{ "isClosure",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isClosure },` |
|      - |  8135 | `		{ "isDeprecated",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isDeprecated },` |
|      - |  8136 | `		{ "isInternal",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isInternal },` |
|      - |  8137 | `		{ "isUserDefined", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isUserDefined },` |
|      - |  8138 | `		{ "isGenerator",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isGenerator },` |
|      - |  8139 | `		{ "isVariadic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isVariadic },` |
|      - |  8140 | `		{ "isStatic",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isStatic },` |
|      - |  8141 | `		{ "getClosureThis", PH7_MOD_PUBLIC, "", "@?object", vm_builtin_ReflectionFunc_getClosureThis },` |
|      - |  8142 | `		{ "getClosureScopeClass", PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|      - |  8143 | `		  vm_builtin_ReflectionFunc_getClosureScopeClass },` |
|      - |  8144 | `		/* php answers the same class for both; PHL has no separate called-scope. */` |
|      - |  8145 | `		{ "getClosureCalledClass", PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|      - |  8146 | `		  vm_builtin_ReflectionFunc_getClosureCalledClass },` |
|      - |  8147 | `		{ "getClosureUsedVariables", PH7_MOD_PUBLIC, "", "array",` |
|      - |  8148 | `		  vm_builtin_ReflectionFunc_getClosureUsedVariables },` |
|      - |  8149 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getDocComment },` |
|      - |  8150 | `		{ "getEndLine",    PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionFunc_getEndLine },` |
|      - |  8151 | `		{ "getExtension",  PH7_MOD_PUBLIC, "", "@?ReflectionExtension", vm_builtin_ReflectionFunc_getExtension },` |
|      - |  8152 | `		{ "getExtensionName", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getExtensionName },` |
|      - |  8153 | `		{ "getFileName",   PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getFileName },` |
|      - |  8154 | `		{ "getName",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getName },` |
|      - |  8155 | `		{ "getNamespaceName", PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getNamespaceName },` |
|      - |  8156 | `		{ "getNumberOfParameters", PH7_MOD_PUBLIC, "", "@int",` |
|      - |  8157 | `		  vm_builtin_ReflectionFunc_getNumberOfParameters },` |
|      - |  8158 | `		{ "getNumberOfRequiredParameters", PH7_MOD_PUBLIC, "", "@int",` |
|      - |  8159 | `		  vm_builtin_ReflectionFunc_getNumberOfRequiredParameters },` |
|      - |  8160 | `		{ "getParameters", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionFunc_getParameters },` |
|      - |  8161 | `		{ "getShortName",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getShortName },` |
|      - |  8162 | `		{ "getStartLine",  PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionFunc_getStartLine },` |
|      - |  8163 | `		{ "getStaticVariables", PH7_MOD_PUBLIC, "", "@array",` |
|      - |  8164 | `		  vm_builtin_ReflectionFunc_getStaticVariables },` |
|      - |  8165 | `		{ "returnsReference", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_returnsReference },` |
|      - |  8166 | `		{ "hasReturnType", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_hasReturnType },` |
|      - |  8167 | `		{ "getReturnType", PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionFunc_getReturnType },` |
|      - |  8168 | `		{ "hasTentativeReturnType", PH7_MOD_PUBLIC, "", "bool",` |
|      - |  8169 | `		  vm_builtin_ReflectionFunc_hasTentativeReturnType },` |
|      - |  8170 | `		{ "getTentativeReturnType", PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|      - |  8171 | `		  vm_builtin_ReflectionFunc_getTentativeReturnType },` |
|      - |  8172 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - |  8173 | `		  vm_builtin_ReflectionFunc_getAttributes },` |
|      - |  8174 | `	};` |
|      - |  8175 | `	static const PH7_NativeConstDef aFunctionConst[] = {` |
|      - |  8176 | `		{ "IS_DEPRECATED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2048, 0, 0.0 },` |
|      - |  8177 | `	};` |
|      - |  8178 | `	static const PH7_NativeMethodDef aFunctionMethod[] = {` |
|      - |  8179 | `		{ "__construct", PH7_MOD_PUBLIC, "Closure\|string $function", "",` |
|      - |  8180 | `		  vm_builtin_ReflectionFunction_construct },` |
|      - |  8181 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionFunc_toString },` |
|      - |  8182 | `		{ "isAnonymous", PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionFunction_isAnonymous },` |
|      - |  8183 | `		{ "isDisabled",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunction_isDisabled },` |
|      - |  8184 | `		{ "invoke",      PH7_MOD_PUBLIC, "mixed ...$args", "@mixed",` |
|      - |  8185 | `		  vm_builtin_ReflectionFunction_invoke },` |
|      - |  8186 | `		{ "invokeArgs",  PH7_MOD_PUBLIC, "array $args", "@mixed",` |
|      - |  8187 | `		  vm_builtin_ReflectionFunction_invokeArgs },` |
|      - |  8188 | `		{ "getClosure",  PH7_MOD_PUBLIC, "", "@Closure", vm_builtin_ReflectionFunction_getClosure },` |
|      - |  8189 | `	};` |
|      - |  8190 | `	static const PH7_NativePropDef aMethodProp[] = {` |
|      - |  8191 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  8192 | ``		/* PHL-only: php's `intern->ce`, which no property publishes. */`` |
|      - |  8193 | `		{ RM_CE,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  8194 | `	};` |
|      - |  8195 | `	static const PH7_NativeConstDef aMethodConst[] = {` |
|      - |  8196 | `		{ "IS_STATIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16, 0, 0.0 },` |
|      - |  8197 | `		{ "IS_PUBLIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,  0, 0.0 },` |
|      - |  8198 | `		{ "IS_PROTECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,  0, 0.0 },` |
|      - |  8199 | `		{ "IS_PRIVATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,  0, 0.0 },` |
|      - |  8200 | `		{ "IS_ABSTRACT",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64, 0, 0.0 },` |
|      - |  8201 | `		{ "IS_FINAL",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|      - |  8202 | `	};` |
|      - |  8203 | `	static const PH7_NativeMethodDef aMethodMethod[] = {` |
|      - |  8204 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $objectOrMethod, ?string $method = null", "",` |
|      - |  8205 | `		  vm_builtin_ReflectionMethod_construct },` |
|      - |  8206 | `		{ "createFromMethodName", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $method", "static",` |
|      - |  8207 | `		  vm_builtin_ReflectionMethod_createFromMethodName },` |
|      - |  8208 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionFunc_toString },` |
|      - |  8209 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isPublic },` |
|      - |  8210 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isPrivate },` |
|      - |  8211 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isProtected },` |
|      - |  8212 | `		{ "isAbstract",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isAbstract },` |
|      - |  8213 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isFinal },` |
|      - |  8214 | `		{ "isConstructor", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isConstructor },` |
|      - |  8215 | `		{ "isDestructor",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isDestructor },` |
|      - |  8216 | `		{ "getClosure",  PH7_MOD_PUBLIC, "?object $object = null", "@Closure",` |
|      - |  8217 | `		  vm_builtin_ReflectionMethod_getClosure },` |
|      - |  8218 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionMethod_getModifiers },` |
|      - |  8219 | `		{ "invoke",      PH7_MOD_PUBLIC, "?object $object, mixed ...$args", "@mixed",` |
|      - |  8220 | `		  vm_builtin_ReflectionMethod_invoke },` |
|      - |  8221 | `		{ "invokeArgs",  PH7_MOD_PUBLIC, "?object $object, array $args", "@mixed",` |
|      - |  8222 | `		  vm_builtin_ReflectionMethod_invokeArgs },` |
|      - |  8223 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|      - |  8224 | `		  vm_builtin_ReflectionMethod_getDeclaringClass },` |
|      - |  8225 | `		{ "getPrototype", PH7_MOD_PUBLIC, "", "@ReflectionMethod", vm_builtin_ReflectionMethod_getPrototype },` |
|      - |  8226 | `		{ "hasPrototype", PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionMethod_hasPrototype },` |
|      - |  8227 | `		{ "setAccessible", PH7_MOD_PUBLIC, "bool $accessible", "@void",` |
|      - |  8228 | `		  vm_builtin_ReflectionMethod_setAccessible },` |
|      - |  8229 | `	};` |
|      - |  8230 | `	static const PH7_NativePropDef aParamProp[] = {` |
|      - |  8231 | `		{ "name", PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  8232 | `		/* PHL-only, the three that identify the parameter (§7.4 (e)) */` |
|      - |  8233 | `		{ RP_T,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  8234 | `		{ RP_M,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  8235 | `		{ RP_P,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|      - |  8236 | `	};` |
|      - |  8237 | `	static const PH7_NativeMethodDef aParamMethod[] = {` |
|      - |  8238 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - |  8239 | `		/* php leaves $function UNTYPED here: it takes a name, a Closure, a` |
|      - |  8240 | ``		 * `[$obj, 'm']` pair or "C::m", and no declarable union covers all four. */`` |
|      - |  8241 | `		{ "__construct", PH7_MOD_PUBLIC, "$function, string\|int $param", "",` |
|      - |  8242 | `		  vm_builtin_ReflectionParameter_construct },` |
|      - |  8243 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionParameter_toString },` |
|      - |  8244 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionParameter_getName },` |
|      - |  8245 | `		{ "isPassedByReference", PH7_MOD_PUBLIC, "", "@bool",` |
|      - |  8246 | `		  vm_builtin_ReflectionParameter_isPassedByReference },` |
|      - |  8247 | `		{ "canBePassedByValue",  PH7_MOD_PUBLIC, "", "@bool",` |
|      - |  8248 | `		  vm_builtin_ReflectionParameter_canBePassedByValue },` |
|      - |  8249 | `		{ "getDeclaringFunction", PH7_MOD_PUBLIC, "", "@ReflectionFunctionAbstract",` |
|      - |  8250 | `		  vm_builtin_ReflectionParameter_getDeclaringFunction },` |
|      - |  8251 | `		{ "getDeclaringClass",   PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|      - |  8252 | `		  vm_builtin_ReflectionParameter_getDeclaringClass },` |
|      - |  8253 | `		{ "getClass",    PH7_MOD_PUBLIC, "", "@?ReflectionClass", vm_builtin_ReflectionParameter_getClass },` |
|      - |  8254 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_hasType },` |
|      - |  8255 | `		{ "getType",     PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionParameter_getType },` |
|      - |  8256 | `		{ "isArray",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isArray },` |
|      - |  8257 | `		{ "isCallable",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isCallable },` |
|      - |  8258 | `		{ "allowsNull",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_allowsNull },` |
|      - |  8259 | `		{ "getPosition", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionParameter_getPosition },` |
|      - |  8260 | `		{ "isOptional",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isOptional },` |
|      - |  8261 | `		{ "isDefaultValueAvailable", PH7_MOD_PUBLIC, "", "@bool",` |
|      - |  8262 | `		  vm_builtin_ReflectionParameter_isDefaultValueAvailable },` |
|      - |  8263 | `		{ "getDefaultValue", PH7_MOD_PUBLIC, "", "@mixed",` |
|      - |  8264 | `		  vm_builtin_ReflectionParameter_getDefaultValue },` |
|      - |  8265 | `		{ "isDefaultValueConstant", PH7_MOD_PUBLIC, "", "@bool",` |
|      - |  8266 | `		  vm_builtin_ReflectionParameter_isDefaultValueConstant },` |
|      - |  8267 | `		{ "getDefaultValueConstantName", PH7_MOD_PUBLIC, "", "@?string",` |
|      - |  8268 | `		  vm_builtin_ReflectionParameter_getDefaultValueConstantName },` |
|      - |  8269 | `		{ "isVariadic",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isVariadic },` |
|      - |  8270 | `		{ "isPromoted",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionParameter_isPromoted },` |
|      - |  8271 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - |  8272 | `		  vm_builtin_ReflectionParameter_getAttributes },` |
|      - |  8273 | `	};` |
|      - |  8274 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  8275 | `		{ "ReflectionFunctionAbstract", 0, "Reflector", PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  8276 | `		  aAbstractMethod, SX_ARRAYSIZE(aAbstractMethod), 0, 0,` |
|      - |  8277 | `		  aAbstractProp, SX_ARRAYSIZE(aAbstractProp), 0, 0, 0 },` |
|      - |  8278 | `		{ "ReflectionFunction", "ReflectionFunctionAbstract", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  8279 | `		  aFunctionMethod, SX_ARRAYSIZE(aFunctionMethod),` |
|      - |  8280 | `		  aFunctionConst, SX_ARRAYSIZE(aFunctionConst), 0, 0, 0, 0, 0 },` |
|      - |  8281 | `		{ "ReflectionMethod", "ReflectionFunctionAbstract", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  8282 | `		  aMethodMethod, SX_ARRAYSIZE(aMethodMethod),` |
|      - |  8283 | `		  aMethodConst, SX_ARRAYSIZE(aMethodConst),` |
|      - |  8284 | `		  aMethodProp, SX_ARRAYSIZE(aMethodProp), 0, 0, 0 },` |
|      - |  8285 | `		{ "ReflectionParameter", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  8286 | `		  aParamMethod, SX_ARRAYSIZE(aParamMethod), 0, 0,` |
|      - |  8287 | `		  aParamProp, SX_ARRAYSIZE(aParamProp), 0, 0, 0 },` |
|      - |  8288 | `	};` |
|   6726 |  8289 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  8290 | `}` |
|      - |  8291 | `/*` |
|      - |  8292 | ` * ---------------------------------------------------------------------------` |
|      - |  8293 | ` * ReflectionProperty and ReflectionClassConstant.` |
|      - |  8294 | ` *` |
|      - |  8295 | ` * Chunk 3, the last of the three that made up "the core". Seven thunks retire` |
|      - |  8296 | ` * with it — __reflect_prop_read, __reflect_prop_write, __reflect_prop_state,` |
|      - |  8297 | ` * __reflect_prop_default, __reflect_static_value, __reflect_static_set and` |
|      - |  8298 | ` * __reflect_const_value — because a native method reaches an instance's slot` |
|      - |  8299 | ` * table, a static's shared slot and a constant's lazy slot directly.` |
|      - |  8300 | ` * ---------------------------------------------------------------------------` |
|      - |  8301 | ` */` |
|      - |  8302 | `#define RP_DYNOBJ "__dynobj"   /* the instance a DYNAMIC property lives on */` |
|      - |  8303 |  |
|      - |  8304 | `/* skipLazyInitialization() is a no-op on an object that is not lazy, which is` |
|      - |  8305 | ` * every object PHL can build — so it answers what php answers rather than` |
|      - |  8306 | ` * refusing (§7.4). */` |
|    ! 0 |  8307 | `static int vm_builtin_ReflectionProperty_noop(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  8308 | `{` |
|    ! 0 |  8309 | `	SXUNUSED(pCtx);` |
|    ! 0 |  8310 | `	SXUNUSED(nArg);` |
|    ! 0 |  8311 | `	SXUNUSED(apArg);` |
|    ! 0 |  8312 | `	return PH7_OK;` |
|    ! 0 |  8313 | `}` |
|      - |  8314 | `/*` |
|      - |  8315 | `` * A STATIC property's shared slot, read and written the way `C::$s` is: the`` |
|      - |  8316 | ` * class's static table materializes first (a default that threw at the` |
|      - |  8317 | ` * declaration raises HERE), and an uninitialized typed static is php's Error.` |
|      - |  8318 | ` */` |
|      6 |  8319 | `static int ReflectStaticSlotRead(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr)` |
|      2 |  8320 | `{` |
|      - |  8321 | `	SyHashEntry *pSlot;` |
|      - |  8322 | `	ph7_value *pVal;` |
|      8 |  8323 | `	sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|      8 |  8324 | `	if( rc != SXRET_OK ){` |
|      3 |  8325 | `		return rc;` |
|      - |  8326 | `	}` |
|      5 |  8327 | `	pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|      5 |  8328 | `	if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|    ! 0 |  8329 | `		ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);` |
|    ! 0 |  8330 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      - |  8331 | `			"Typed static property %z::$%z must not be accessed before initialization",` |
|    ! 0 |  8332 | `			&pDecl->sName, &pAttr->sName);` |
|      - |  8333 | `	}` |
|      5 |  8334 | `	pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      5 |  8335 | `	if( pVal ){` |
|      5 |  8336 | `		ph7_result_value(pCtx, pVal);` |
|      3 |  8337 | `	}else{` |
|    ! 0 |  8338 | `		ph7_result_null(pCtx);` |
|      - |  8339 | `	}` |
|      5 |  8340 | `	return PH7_OK;` |
|      5 |  8341 | `}` |
|      4 |  8342 | `static int ReflectStaticSlotWrite(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr,` |
|      - |  8343 | `	ph7_value *pValue)` |
|      2 |  8344 | `{` |
|      - |  8345 | `	ph7_value *pSlot;` |
|      6 |  8346 | `	sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|      6 |  8347 | `	if( rc != SXRET_OK ){` |
|      3 |  8348 | `		return rc;` |
|      - |  8349 | `	}` |
|      3 |  8350 | `	rc = ReflectEnforceStore(pCtx, pAttr->nIdx, pValue);` |
|      3 |  8351 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  8352 | `		return rc;` |
|      - |  8353 | `	}` |
|      3 |  8354 | `	pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      3 |  8355 | `	if( pSlot ){` |
|      3 |  8356 | `		PH7_MemObjStore(pValue, pSlot);` |
|      1 |  8357 | `	}` |
|      3 |  8358 | `	return PH7_OK;` |
|      4 |  8359 | `}` |
|      - |  8360 |  |
|      - |  8361 | `/* What a ReflectionProperty / ReflectionClassConstant is looking at. */` |
|      - |  8362 | `typedef struct ReflectMemberRef ReflectMemberRef;` |
|      - |  8363 | `struct ReflectMemberRef` |
|      - |  8364 | `{` |
|      - |  8365 | `	ph7_class *pClass;            /* the reflected class */` |
|      - |  8366 | `	ph7_class_attr *pAttr;        /* the declared member (NULL for a dynamic property) */` |
|      - |  8367 | `	ph7_class_instance *pDynObj;  /* the instance a dynamic property lives on */` |
|      - |  8368 | `	const char *zName;            /* the member's name (borrowed from the slot) */` |
|      - |  8369 | `	int nName;` |
|      - |  8370 | `};` |
|      - |  8371 | `/*` |
|      - |  8372 | `` * Resolve `$this->class` + `$this->name` into the member. Uses the LISTING`` |
|      - |  8373 | ` * answer of the member walk, which is php's: a base class's PRIVATE member is` |
|      - |  8374 | ``  * not reachable by name from the subclass (`new ReflectionProperty('B','pp')` `` |
|      - |  8375 | `` * raises where `B extends A` and `A::$pp` is private).`` |
|      - |  8376 | ` */` |
|   1180 |  8377 | `static int ReflectMemberOfThis(ph7_context *pCtx, ReflectMemberRef *pOut, int iKind)` |
|      5 |  8378 | `{` |
|   1185 |  8379 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  8380 | `	const char *zClass;` |
|      - |  8381 | `	int nClass;` |
|      - |  8382 | `	SySet aMembers;` |
|      - |  8383 | `	sxu32 n;` |
|   1185 |  8384 | `	SyZero(pOut, sizeof(*pOut));` |
|   1185 |  8385 | `	if( pThis == 0 ){` |
|    ! 0 |  8386 | `		return 0;` |
|      - |  8387 | `	}` |
|   1185 |  8388 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|   1185 |  8389 | `	PH7_NativeAttrStr(pThis, "name", &pOut->zName, &pOut->nName);` |
|   1185 |  8390 | `	if( nClass < 1 ){` |
|    ! 0 |  8391 | `		return 0;` |
|      - |  8392 | `	}` |
|   1185 |  8393 | `	pOut->pClass = PH7_VmExtractClass(pCtx->pVm, zClass, (sxu32)nClass, FALSE, 0);` |
|   1185 |  8394 | `	if( pOut->pClass == 0 ){` |
|    ! 0 |  8395 | `		return 0;` |
|      - |  8396 | `	}` |
|   1185 |  8397 | `	if( iKind == REFLECT_MEMBER_PROP ){` |
|   1063 |  8398 | `		pOut->pDynObj = PH7_NativeAttrObj(pThis, RP_DYNOBJ);` |
|    529 |  8399 | `	}` |
|   1185 |  8400 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|   1185 |  8401 | `	ReflectMembers(pCtx->pVm, pOut->pClass, &aMembers, 0);` |
|   6451 |  8402 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   6445 |  8403 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   6445 |  8404 | `		if( pM->iKind == iKind && ReflectKeyIs(pM, pOut->zName, pOut->nName) ){` |
|   1179 |  8405 | `			pOut->pAttr = pM->pAttr;` |
|   1179 |  8406 | `			break;` |
|      - |  8407 | `		}` |
|   2638 |  8408 | `	}` |
|   1185 |  8409 | `	SySetRelease(&aMembers);` |
|   1185 |  8410 | `	return pOut->pAttr != 0 \|\| pOut->pDynObj != 0;` |
|    595 |  8411 | `}` |
|      - |  8412 | `/* The class php reports as the member's declarer. */` |
|     98 |  8413 | `static ph7_class * ReflectMemberDecl(const ReflectMemberRef *pRef)` |
|      3 |  8414 | `{` |
|    101 |  8415 | `	if( pRef->pAttr && pRef->pAttr->pDeclClass ){` |
|    101 |  8416 | `		return PH7_VmMemberOwnerClass(pRef->pAttr->pDeclClass,pRef->pClass);` |
|      - |  8417 | `	}` |
|    ! 0 |  8418 | `	return pRef->pClass;` |
|     52 |  8419 | `}` |
|      - |  8420 | `/* The instance slot record for a dynamic (or any instance-owned) property. */` |
|     14 |  8421 | `static VmClassAttr * ReflectInstanceAttr(ph7_class_instance *pObj, const char *zName, int nName)` |
|      1 |  8422 | `{` |
|      - |  8423 | `	SyHashEntry *pEntry;` |
|     15 |  8424 | `	if( pObj == 0 \|\| nName < 1 ){` |
|      7 |  8425 | `		return 0;` |
|      - |  8426 | `	}` |
|      9 |  8427 | `	pEntry = SyHashGet(&pObj->hAttr, (const void *)zName, (sxu32)nName);` |
|      9 |  8428 | `	return pEntry ? (VmClassAttr *)pEntry->pUserData : 0;` |
|      8 |  8429 | `}` |
|      - |  8430 | `/*` |
|      - |  8431 | ` * The instance slot a resolved ReflectionProperty addresses. A base class's` |
|      - |  8432 | ` * PRIVATE property lives under php's MANGLED storage name on every object below` |
|      - |  8433 | ` * it, so looking it up by its plain name would find the same-named property of` |
|      - |  8434 | ` * the object's OWN class instead -- reading and writing the wrong slot.` |
|      - |  8435 | ` */` |
|     98 |  8436 | `static VmClassAttr * ReflectRefInstanceAttr(ph7_vm *pVm, ph7_class_instance *pObj,` |
|      - |  8437 | `	const ReflectMemberRef *pRef)` |
|      1 |  8438 | `{` |
|     99 |  8439 | `	if( pObj && pRef->pAttr ){` |
|     97 |  8440 | `		const SyString *pKey = PH7_ClassAttrStorageName(pVm, pObj->pClass, pRef->pAttr);` |
|    145 |  8441 | `		SyHashEntry *pEntry = SyHashGet(&pObj->hAttr,` |
|     96 |  8442 | `			(const void *)SyStringData(pKey), SyStringLength(pKey));` |
|     97 |  8443 | `		return pEntry ? (VmClassAttr *)pEntry->pUserData : 0;` |
|      - |  8444 | `	}` |
|      3 |  8445 | `	return ReflectInstanceAttr(pObj, pRef->zName, pRef->nName);` |
|     50 |  8446 | `}` |
|      - |  8447 | `/* ---- ReflectionProperty ---- */` |
|      - |  8448 | `/* ReflectionProperty::__construct(object\|string $class, string $property) */` |
|    496 |  8449 | `static int vm_builtin_ReflectionProperty_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  8450 | `{` |
|    501 |  8451 | `	ph7_vm *pVm = pCtx->pVm;` |
|    501 |  8452 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    501 |  8453 | `	ph7_class_instance *pObj = 0;` |
|      - |  8454 | `	ph7_class *pClass;` |
|      - |  8455 | `	const char *zProp;` |
|      - |  8456 | `	int nProp;` |
|      - |  8457 | `	SySet aMembers;` |
|      - |  8458 | `	sxu32 n;` |
|    501 |  8459 | `	int bFound = 0;` |
|    501 |  8460 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 |  8461 | `		return PH7_OK;` |
|      - |  8462 | `	}` |
|    501 |  8463 | `	if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      7 |  8464 | `		pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 |  8465 | `	}` |
|    501 |  8466 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|    501 |  8467 | `	if( pClass == 0 ){` |
|      - |  8468 | `		const char *zName;` |
|      - |  8469 | `		int nName;` |
|      3 |  8470 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|      4 |  8471 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  8472 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      - |  8473 | `	}` |
|    499 |  8474 | `	zProp = ph7_value_to_string(apArg[1], &nProp);` |
|    499 |  8475 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|    499 |  8476 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|   2087 |  8477 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   2075 |  8478 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   2075 |  8479 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zProp, nProp) ){` |
|      - |  8480 | `			/* php's $class is the DECLARING class, not the one asked about. */` |
|    487 |  8481 | `			pClass = pM->pDecl ? pM->pDecl : pClass;` |
|    487 |  8482 | `			bFound = 1;` |
|    487 |  8483 | `			break;` |
|      - |  8484 | `		}` |
|    799 |  8485 | `	}` |
|    499 |  8486 | `	SySetRelease(&aMembers);` |
|    746 |  8487 | `	PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pClass->sName),` |
|    494 |  8488 | `		(int)SyStringLength(&pClass->sName));` |
|    499 |  8489 | `	if( bFound ){` |
|    487 |  8490 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", zProp, nProp);` |
|    487 |  8491 | `		return PH7_OK;` |
|      - |  8492 | `	}` |
|      - |  8493 | `	/* Not declared: an OBJECT may still own it as a dynamic property. */` |
|     13 |  8494 | `	if( ReflectInstanceAttr(pObj, zProp, nProp) ){` |
|      7 |  8495 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", zProp, nProp);` |
|      7 |  8496 | `		PH7_NativeSetAttrObj(pVm, pThis, RP_DYNOBJ, pObj);` |
|      7 |  8497 | `		return PH7_OK;` |
|      - |  8498 | `	}` |
|     10 |  8499 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 |  8500 | `		"Property %z::$%.*s does not exist", &pClass->sName, nProp, zProp);` |
|    253 |  8501 | `}` |
|    310 |  8502 | `static int vm_builtin_ReflectionProperty_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 |  8503 | `{` |
|    314 |  8504 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    314 |  8505 | `	const char *zName = "";` |
|    314 |  8506 | `	int nName = 0;` |
|    155 |  8507 | `	SXUNUSED(nArg);` |
|    155 |  8508 | `	SXUNUSED(apArg);` |
|    314 |  8509 | `	if( pThis ){` |
|    314 |  8510 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    155 |  8511 | `	}` |
|    314 |  8512 | `	ph7_result_string(pCtx, zName, nName);` |
|    314 |  8513 | `	return PH7_OK;` |
|      4 |  8514 | `}` |
|      - |  8515 | `/*` |
|      - |  8516 | ` * getMangledName(): the name php stores the slot under —  plain for a public` |
|      - |  8517 | ` * property, "\0*\0name" for a protected one and "\0Class\0name" for a private` |
|      - |  8518 | ` * one. PHL's tables are not mangled, so the name is COMPOSED here; what php` |
|      - |  8519 | ` * exposes is the spelling, not the storage.` |
|      - |  8520 | ` */` |
|      6 |  8521 | `static int vm_builtin_ReflectionProperty_getMangledName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8522 | `{` |
|      - |  8523 | `	ReflectMemberRef sRef;` |
|      - |  8524 | `	SyBlob sOut;` |
|      3 |  8525 | `	SXUNUSED(nArg);` |
|      3 |  8526 | `	SXUNUSED(apArg);` |
|      7 |  8527 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  8528 | `		ph7_result_string(pCtx, sRef.zName, sRef.nName);` |
|    ! 0 |  8529 | `		return PH7_OK;` |
|      - |  8530 | `	}` |
|      7 |  8531 | `	if( sRef.pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|      3 |  8532 | `		ph7_result_string(pCtx, sRef.zName, sRef.nName);` |
|      3 |  8533 | `		return PH7_OK;` |
|      - |  8534 | `	}` |
|      5 |  8535 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|      5 |  8536 | `	SyBlobAppend(&sOut, "\0", 1);` |
|      5 |  8537 | `	if( sRef.pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|      3 |  8538 | `		SyBlobAppend(&sOut, "*", 1);` |
|      2 |  8539 | `	}else{` |
|      3 |  8540 | `		ph7_class *pDecl = ReflectMemberDecl(&sRef);` |
|      3 |  8541 | `		SyBlobAppend(&sOut, SyStringData(&pDecl->sName), SyStringLength(&pDecl->sName));` |
|      - |  8542 | `	}` |
|      5 |  8543 | `	SyBlobAppend(&sOut, "\0", 1);` |
|      5 |  8544 | `	SyBlobAppend(&sOut, sRef.zName, (sxu32)sRef.nName);` |
|      5 |  8545 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|      5 |  8546 | `	SyBlobRelease(&sOut);` |
|      5 |  8547 | `	return PH7_OK;` |
|      4 |  8548 | `}` |
|      - |  8549 | `/* The boolean predicates, all off the declared attribute. */` |
|    228 |  8550 | `static int ReflectPropFlag(ph7_context *pCtx, int iWhat)` |
|      2 |  8551 | `{` |
|      - |  8552 | `	ReflectMemberRef sRef;` |
|    230 |  8553 | `	int bYes = 0;` |
|    342 |  8554 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|    226 |  8555 | `		ph7_class_attr *pAttr = sRef.pAttr;` |
|    226 |  8556 | `		switch( iWhat ){` |
|      5 |  8557 | `		case 0: bYes = pAttr->iProtection == PH7_CLASS_PROT_PUBLIC; break;` |
|    ! 0 |  8558 | `		case 1: bYes = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE; break;` |
|      3 |  8559 | `		case 2: bYes = pAttr->iProtection == PH7_CLASS_PROT_PROTECTED; break;` |
|     23 |  8560 | `		case 3: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) != 0; break;` |
|     23 |  8561 | `		case 4: bYes = ReflectPropProtectedSet(pAttr); break;` |
|     16 |  8562 | `		case 5: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0; break;` |
|     64 |  8563 | `		case 6: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0; break;` |
|      3 |  8564 | `		case 7: bYes = 1; break;                                    /* isDefault */` |
|    ! 0 |  8565 | `		case 8: bYes = 0; break;                                    /* isDynamic */` |
|    128 |  8566 | `		case 9: bYes = (pAttr->iFlags` |
|    128 |  8567 | `			& (PH7_CLASS_ATTR_HOOK_VIRTUAL\|PH7_CLASS_ATTR_NATIVE_VIRTUAL)) != 0; break;` |
|      - |  8568 | ``		/* isFinal: the DECLARED `final` (PHP 8.4) or the one private(set) implies`` |
|      - |  8569 | `		 * -- php answers true for both, the same pair its modifier mask carries. */` |
|     10 |  8570 | `		case 10: bYes = (pAttr->iFlags` |
|     10 |  8571 | `			& (PH7_CLASS_ATTR_FINAL\|PH7_CLASS_ATTR_PRIVATE_SET)) != 0; break;` |
|      3 |  8572 | `		default:  /* 11: hasHooks */` |
|      7 |  8573 | `			bYes = (pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) != 0;` |
|      6 |  8574 | `			break;` |
|      - |  8575 | `		}` |
|    118 |  8576 | `	}else if( iWhat == 0 \|\| iWhat == 8 ){` |
|      - |  8577 | `		/* A dynamic property is public and, by definition, not a default one. */` |
|      3 |  8578 | `		bYes = 1;` |
|      1 |  8579 | `	}` |
|    230 |  8580 | `	ph7_result_bool(pCtx, bYes);` |
|    230 |  8581 | `	return PH7_OK;` |
|      2 |  8582 | `}` |
|      - |  8583 | `#define REFLECT_PROP_FLAG(NAME,WHAT) \` |
|      - |  8584 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  8585 | `	{ \` |
|      - |  8586 | `		SXUNUSED(nArg); \` |
|      - |  8587 | `		SXUNUSED(apArg); \` |
|      - |  8588 | `		return ReflectPropFlag(pCtx, WHAT); \` |
|      - |  8589 | `	}` |
|      5 |  8590 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPublic, 0)` |
|    ! 0 |  8591 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPrivate, 1)` |
|      3 |  8592 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isProtected, 2)` |
|     23 |  8593 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPrivateSet, 3)` |
|     23 |  8594 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isProtectedSet, 4)` |
|     16 |  8595 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isStatic, 5)` |
|     64 |  8596 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isReadOnly, 6)` |
|      7 |  8597 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isFinal, 10)` |
|      5 |  8598 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isDefault, 7)` |
|      3 |  8599 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isDynamic, 8)` |
|     86 |  8600 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isVirtual, 9)` |
|      7 |  8601 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_hasHooks, 11)` |
|      - |  8602 |  |
|      - |  8603 | `/* php has no abstract properties outside an interface stub, and PHL none at` |
|      - |  8604 | ` * all; isLazy() answers what is true of a VM without lazy objects. */` |
|    ! 0 |  8605 | `static int vm_builtin_ReflectionProperty_false(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  8606 | `{` |
|    ! 0 |  8607 | `	SXUNUSED(nArg);` |
|    ! 0 |  8608 | `	SXUNUSED(apArg);` |
|    ! 0 |  8609 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 |  8610 | `	return PH7_OK;` |
|    ! 0 |  8611 | `}` |
|      - |  8612 | `/*` |
|      - |  8613 | ` * isPromoted(): the property came from a constructor-promoted parameter. PHL` |
|      - |  8614 | ` * records promotion on the ARGUMENT (VM_FUNC_ARG_PROMOTED), not on the` |
|      - |  8615 | ` * attribute, so the constructor's parameter list is what answers.` |
|      - |  8616 | ` */` |
|      6 |  8617 | `static int vm_builtin_ReflectionProperty_isPromoted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8618 | `{` |
|      - |  8619 | `	ReflectMemberRef sRef;` |
|      - |  8620 | `	ph7_class_method *pCons;` |
|      - |  8621 | `	ph7_vm_func_arg *aArg;` |
|      - |  8622 | `	sxu32 n;` |
|      7 |  8623 | `	int bYes = 0;` |
|      3 |  8624 | `	SXUNUSED(nArg);` |
|      3 |  8625 | `	SXUNUSED(apArg);` |
|      7 |  8626 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|      7 |  8627 | `		ph7_class *pDecl = ReflectMemberDecl(&sRef);` |
|      7 |  8628 | `		pCons = PH7_ClassExtractMethod(pDecl, "__construct", sizeof("__construct")-1);` |
|      7 |  8629 | `		if( pCons ){` |
|      7 |  8630 | `			aArg = (ph7_vm_func_arg *)SySetBasePtr(&pCons->sFunc.aArgs);` |
|      9 |  8631 | `			for( n = 0 ; n < SySetUsed(&pCons->sFunc.aArgs) ; n++ ){` |
|      7 |  8632 | `				if( (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|    ! 0 |  8633 | `					continue;` |
|      - |  8634 | `				}` |
|      6 |  8635 | `				if( SyStringLength(&aArg[n].sName) == (sxu32)sRef.nName` |
|      6 |  8636 | `				 && SyMemcmp(SyStringData(&aArg[n].sName), sRef.zName, (sxu32)sRef.nName) == 0 ){` |
|      5 |  8637 | `					bYes = 1;` |
|      5 |  8638 | `					break;` |
|      - |  8639 | `				}` |
|      2 |  8640 | `			}` |
|      3 |  8641 | `		}` |
|      3 |  8642 | `	}` |
|      7 |  8643 | `	ph7_result_bool(pCtx, bYes);` |
|      7 |  8644 | `	return PH7_OK;` |
|      1 |  8645 | `}` |
|    192 |  8646 | `static int vm_builtin_ReflectionProperty_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  8647 | `{` |
|      - |  8648 | `	ReflectMemberRef sRef;` |
|     96 |  8649 | `	SXUNUSED(nArg);` |
|     96 |  8650 | `	SXUNUSED(apArg);` |
|    194 |  8651 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  8652 | `		ph7_result_int(pCtx, 1);   /* a dynamic property is public */` |
|    ! 0 |  8653 | `		return PH7_OK;` |
|      - |  8654 | `	}` |
|    194 |  8655 | `	ph7_result_int64(pCtx, ReflectPropModifiers(sRef.pAttr));` |
|    194 |  8656 | `	return PH7_OK;` |
|     98 |  8657 | `}` |
|     66 |  8658 | `static int vm_builtin_ReflectionProperty_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8659 | `{` |
|      - |  8660 | `	ReflectMemberRef sRef;` |
|     33 |  8661 | `	SXUNUSED(nArg);` |
|     33 |  8662 | `	SXUNUSED(apArg);` |
|     67 |  8663 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  8664 | `		ph7_result_null(pCtx);` |
|    ! 0 |  8665 | `		return PH7_OK;` |
|      - |  8666 | `	}` |
|     67 |  8667 | `	return ReflectResultClassOf(pCtx, ReflectMemberDecl(&sRef));` |
|     34 |  8668 | `}` |
|      6 |  8669 | `static int vm_builtin_ReflectionProperty_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8670 | `{` |
|      - |  8671 | `	ReflectMemberRef sRef;` |
|      3 |  8672 | `	SXUNUSED(nArg);` |
|      3 |  8673 | `	SXUNUSED(apArg);` |
|      6 |  8674 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr` |
|      7 |  8675 | `	 && SyStringLength(&sRef.pAttr->sDoc) > 0 ){` |
|      7 |  8676 | `		ph7_result_string(pCtx, SyStringData(&sRef.pAttr->sDoc),` |
|      4 |  8677 | `			(int)SyStringLength(&sRef.pAttr->sDoc));` |
|      3 |  8678 | `	}else{` |
|      3 |  8679 | `		ph7_result_bool(pCtx, 0);` |
|      - |  8680 | `	}` |
|      7 |  8681 | `	return PH7_OK;` |
|      1 |  8682 | `}` |
|     74 |  8683 | `static int vm_builtin_ReflectionProperty_hasType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8684 | `{` |
|      - |  8685 | `	ReflectMemberRef sRef;` |
|     37 |  8686 | `	SXUNUSED(nArg);` |
|     37 |  8687 | `	SXUNUSED(apArg);` |
|    149 |  8688 | `	ph7_result_bool(pCtx, ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP)` |
|     74 |  8689 | `		&& sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);` |
|     75 |  8690 | `	return PH7_OK;` |
|      1 |  8691 | `}` |
|    164 |  8692 | `static int vm_builtin_ReflectionProperty_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  8693 | `{` |
|      - |  8694 | `	ReflectMemberRef sRef;` |
|     82 |  8695 | `	SXUNUSED(nArg);` |
|     82 |  8696 | `	SXUNUSED(apArg);` |
|    164 |  8697 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0` |
|    164 |  8698 | `	 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|    164 |  8699 | `	 \|\| SyStringLength(&sRef.pAttr->sTypeName) < 1 ){` |
|      7 |  8700 | `		ph7_result_null(pCtx);` |
|      7 |  8701 | `		return PH7_OK;` |
|      - |  8702 | `	}` |
|      - |  8703 | `	{` |
|      - |  8704 | `		char zType[192];` |
|    161 |  8705 | `		const char *zT = ReflectMemberTypeText(pCtx->pVm,sRef.pAttr,zType,sizeof(zType));` |
|    161 |  8706 | `		return ReflectResultObject(pCtx, ReflectMakeType(pCtx, zT, (int)SyStrlen(zT)));` |
|      - |  8707 | `	}` |
|     85 |  8708 | `}` |
|    132 |  8709 | `static int vm_builtin_ReflectionProperty_hasDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  8710 | `{` |
|      - |  8711 | `	ReflectMemberRef sRef;` |
|     66 |  8712 | `	SXUNUSED(nArg);` |
|     66 |  8713 | `	SXUNUSED(apArg);` |
|    134 |  8714 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  8715 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  8716 | `		return PH7_OK;` |
|      - |  8717 | `	}` |
|    134 |  8718 | `	if( SySetUsed(&sRef.pAttr->aByteCode) > 0 \|\| sRef.pAttr->pNativeValue ){` |
|     35 |  8719 | `		ph7_result_bool(pCtx, 1);` |
|     35 |  8720 | `		return PH7_OK;` |
|      - |  8721 | `	}` |
|      - |  8722 | `	/* An UNTYPED property with no initializer still defaults to null. */` |
|    100 |  8723 | `	ph7_result_bool(pCtx, (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0);` |
|    100 |  8724 | `	return PH7_OK;` |
|     68 |  8725 | `}` |
|     30 |  8726 | `static int vm_builtin_ReflectionProperty_getDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  8727 | `{` |
|      - |  8728 | `	ReflectMemberRef sRef;` |
|      - |  8729 | `	ph7_value sValue;` |
|     15 |  8730 | `	SXUNUSED(nArg);` |
|     15 |  8731 | `	SXUNUSED(apArg);` |
|     31 |  8732 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  8733 | `		sRef.pAttr = 0;` |
|    ! 0 |  8734 | `	}` |
|     31 |  8735 | `	if( sRef.pAttr && sRef.pAttr->pNativeValue && SySetUsed(&sRef.pAttr->aByteCode) < 1 ){` |
|      - |  8736 | `		/* A NATIVE property's default is a LITERAL, not byte-code — the same` |
|      - |  8737 | ``		 * record `new` materializes (PH7_NativeLiteralValue). Reading only the`` |
|      - |  8738 | `		 * byte-code answered NULL for every declared native default and raised` |
|      - |  8739 | `		 * php's no-default deprecation on a slot that hasDefaultValue() had just` |
|      - |  8740 | `		 * reported true for. */` |
|     21 |  8741 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     21 |  8742 | `		PH7_NativeLiteralValue(pCtx->pVm, sRef.pAttr->pNativeValue, &sValue);` |
|     21 |  8743 | `		ph7_result_value(pCtx, &sValue);` |
|     21 |  8744 | `		PH7_MemObjRelease(&sValue);` |
|     21 |  8745 | `		return PH7_OK;` |
|      - |  8746 | `	}` |
|     11 |  8747 | `	if( sRef.pAttr == 0 \|\| SySetUsed(&sRef.pAttr->aByteCode) < 1 ){` |
|      - |  8748 | `		/* php 8.5 deprecates the question when there is no default — an` |
|      - |  8749 | `		 * UNTYPED property still has one (null), a typed one without an` |
|      - |  8750 | `		 * initializer does not. */` |
|      5 |  8751 | `		if( sRef.pAttr == 0 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|      3 |  8752 | `			PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED,` |
|      - |  8753 | `				"ReflectionProperty::getDefaultValue() for a property without a default "` |
|      - |  8754 | `				"value is deprecated, use ReflectionProperty::hasDefaultValue() to check "` |
|      - |  8755 | `				"if the default value exists");` |
|      1 |  8756 | `		}` |
|      5 |  8757 | `		ph7_result_null(pCtx);` |
|      5 |  8758 | `		return PH7_OK;` |
|      - |  8759 | `	}` |
|      - |  8760 | `	/* Same evaluation path the VM uses for an omitted call argument */` |
|      7 |  8761 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|      7 |  8762 | `	VmLocalExec(pCtx->pVm, &sRef.pAttr->aByteCode, &sValue, FALSE);` |
|      7 |  8763 | `	ph7_result_value(pCtx, &sValue);` |
|      7 |  8764 | `	PH7_MemObjRelease(&sValue);` |
|      7 |  8765 | `	return PH7_OK;` |
|     16 |  8766 | `}` |
|      - |  8767 | `/* php 8.1 made every reflected member accessible; the setter is a no-op it kept. */` |
|    ! 0 |  8768 | `static int vm_builtin_ReflectionProperty_setAccessible(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 |  8769 | `{` |
|    ! 0 |  8770 | `	SXUNUSED(pCtx);` |
|    ! 0 |  8771 | `	SXUNUSED(nArg);` |
|    ! 0 |  8772 | `	SXUNUSED(apArg);` |
|    ! 0 |  8773 | `	return PH7_OK;` |
|    ! 0 |  8774 | `}` |
|      - |  8775 | `/* getValue()/setValue()/isInitialized() all need the same receiver check. */` |
|    100 |  8776 | `static sxi32 ReflectPropReceiver(ph7_context *pCtx, ReflectMemberRef *pRef,` |
|      - |  8777 | `	ph7_value *pObject, ph7_class_instance **ppThis, const char *zWho)` |
|      1 |  8778 | `{` |
|    101 |  8779 | `	*ppThis = 0;` |
|     50 |  8780 | `	SXUNUSED(pRef);` |
|    101 |  8781 | `	if( pObject && (pObject->iFlags & MEMOBJ_OBJ) ){` |
|     99 |  8782 | `		*ppThis = (ph7_class_instance *)pObject->x.pOther;` |
|     99 |  8783 | `		return PH7_OK;` |
|      - |  8784 | `	}` |
|      4 |  8785 | `	return PH7_VmThrowException(pCtx, "TypeError",` |
|      - |  8786 | `		"ReflectionProperty::%s(): Argument #1 ($object) must be provided for instance properties",` |
|      1 |  8787 | `		zWho);` |
|     51 |  8788 | `}` |
|     54 |  8789 | `static int vm_builtin_ReflectionProperty_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  8790 | `{` |
|      - |  8791 | `	ReflectMemberRef sRef;` |
|      - |  8792 | `	ph7_class_instance *pObj;` |
|      - |  8793 | `	VmClassAttr *pVmAttr;` |
|      - |  8794 | `	ph7_value *pValue;` |
|      - |  8795 | `	sxi32 rc;` |
|     56 |  8796 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  8797 | `		ph7_result_null(pCtx);` |
|    ! 0 |  8798 | `		return PH7_OK;` |
|      - |  8799 | `	}` |
|     56 |  8800 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|      8 |  8801 | `		return ReflectStaticSlotRead(pCtx, sRef.pClass, sRef.pAttr);` |
|      - |  8802 | `	}` |
|     49 |  8803 | `	rc = ReflectPropReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pObj, "getValue");` |
|     49 |  8804 | `	if( rc != PH7_OK ){` |
|      3 |  8805 | `		return rc;` |
|      - |  8806 | `	}` |
|     47 |  8807 | `	pVmAttr = ReflectRefInstanceAttr(pCtx->pVm, pObj, &sRef);` |
|     47 |  8808 | `	if( pVmAttr == 0 ){` |
|      - |  8809 | `		/* No slot: a class whose properties are its own handlers answers here,` |
|      - |  8810 | ``		 * as php's does -- Reflection reads a PDORow's `queryString` through`` |
|      - |  8811 | ``		 * read_property exactly as `$row->queryString` does. */`` |
|      - |  8812 | `		PH7_NativePropCtx sNat;` |
|      - |  8813 | `		SyString sNatName;` |
|      - |  8814 | `		ph7_value sNatVal;` |
|     11 |  8815 | `		SyStringInitFromBuf(&sNatName, sRef.zName, (sxu32)sRef.nName);` |
|     11 |  8816 | `		PH7_MemObjInit(pCtx->pVm, &sNatVal);` |
|     11 |  8817 | `		if( PH7_ClassNativePropAsk(pObj, &sNat, PH7_NATIVE_PROP_READ, &sNatName, &sNatVal) ){` |
|      7 |  8818 | `			if( sNat.zThrowClass ){` |
|    ! 0 |  8819 | `				PH7_MemObjRelease(&sNatVal);` |
|    ! 0 |  8820 | `				return PH7_VmThrowExceptionCode(pCtx, sNat.zThrowClass, sNat.iThrowCode,` |
|    ! 0 |  8821 | `					"%s", sNat.zThrowMsg);` |
|      - |  8822 | `			}` |
|      7 |  8823 | `			ph7_result_value(pCtx, &sNatVal);` |
|      7 |  8824 | `			PH7_MemObjRelease(&sNatVal);` |
|      7 |  8825 | `			return PH7_OK;` |
|      - |  8826 | `		}` |
|      5 |  8827 | `		PH7_MemObjRelease(&sNatVal);` |
|      5 |  8828 | `		if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){` |
|      - |  8829 | `` 			/* A VIRTUAL property: php reads it through the same handler `$o->p` `` |
|      - |  8830 | `			 * reaches, so Reflection answers the VALUE rather than warning that a` |
|      - |  8831 | `			 * name the class declares is undefined. A class whose handlers are its` |
|      - |  8832 | `			 * magic trio (ext/dom) is read through __get, which is the door the` |
|      - |  8833 | `			 * member opcode takes for the very same name. */` |
|      - |  8834 | `			ph7_value sMagic;` |
|      - |  8835 | `			SyString sMagicName;` |
|    ! 0 |  8836 | `			SyStringInitFromBuf(&sMagicName, sRef.zName, (sxu32)sRef.nName);` |
|    ! 0 |  8837 | `			PH7_MemObjInit(pCtx->pVm, &sMagic);` |
|    ! 0 |  8838 | `			if( PH7_ClassInstanceCallMagicMethod(pCtx->pVm, pObj->pClass, pObj,` |
|    ! 0 |  8839 | `					"__get", sizeof("__get")-1, &sMagicName, &sMagic) == SXRET_OK ){` |
|    ! 0 |  8840 | `				ph7_result_value(pCtx, &sMagic);` |
|    ! 0 |  8841 | `				PH7_MemObjRelease(&sMagic);` |
|    ! 0 |  8842 | `				return PH7_OK;` |
|      - |  8843 | `			}` |
|    ! 0 |  8844 | `			PH7_MemObjRelease(&sMagic);` |
|    ! 0 |  8845 | `			ph7_result_null(pCtx);` |
|    ! 0 |  8846 | `			return PH7_OK;` |
|      - |  8847 | `		}` |
|      4 |  8848 | `		if( sRef.pAttr` |
|      5 |  8849 | `		 && (sRef.pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|      2 |  8850 | `		                           \|PH7_CLASS_ATTR_HIDDEN)) == 0 ){` |
|      - |  8851 | `			/* A DECLARED property the object no longer holds -- unset() took it.` |
|      - |  8852 | ``			 * php reads it the way `$o->p` does, and warns exactly the same:`` |
|      - |  8853 | ``			 * `Undefined property: C::$p`, naming the OBJECT's class. */`` |
|      7 |  8854 | `			VmErrorFormat(pCtx->pVm, PH7_CTX_WARNING, "Undefined property: %z::$%.*s",` |
|      4 |  8855 | `				&pObj->pClass->sName, sRef.nName, sRef.zName);` |
|      2 |  8856 | `		}` |
|      5 |  8857 | `		ph7_result_null(pCtx);` |
|      5 |  8858 | `		return PH7_OK;` |
|      - |  8859 | `	}` |
|     37 |  8860 | `	if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|      3 |  8861 | `		ph7_class *pDecl = PH7_VmMemberOwnerClass(` |
|      4 |  8862 | `			pVmAttr->pAttr ? pVmAttr->pAttr->pDeclClass : 0,pObj->pClass);` |
|      7 |  8863 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      - |  8864 | `			"Typed property %z::$%.*s must not be accessed before initialization",` |
|      2 |  8865 | `			&pDecl->sName, sRef.nName, sRef.zName);` |
|      - |  8866 | `	}` |
|     33 |  8867 | `	pValue = PH7_ClassInstanceExtractAttrValue(pObj, pVmAttr);` |
|     33 |  8868 | `	if( pValue ){` |
|     33 |  8869 | `		ph7_result_value(pCtx, pValue);` |
|     17 |  8870 | `	}else{` |
|    ! 0 |  8871 | `		ph7_result_null(pCtx);` |
|      - |  8872 | `	}` |
|     33 |  8873 | `	return PH7_OK;` |
|     29 |  8874 | `}` |
|     30 |  8875 | `static int vm_builtin_ReflectionProperty_setValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  8876 | `{` |
|      - |  8877 | `	ReflectMemberRef sRef;` |
|      - |  8878 | `	ph7_class_instance *pObj;` |
|      - |  8879 | `	VmClassAttr *pVmAttr;` |
|      - |  8880 | `	ph7_value *pSlot;` |
|      - |  8881 | `	sxi32 rc;` |
|     32 |  8882 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| nArg < 1 ){` |
|    ! 0 |  8883 | `		return PH7_OK;` |
|      - |  8884 | `	}` |
|     32 |  8885 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|      - |  8886 | `		/* php's one-argument spelling for a static: setValue($value). Two` |
|      - |  8887 | `		 * arguments, or one OBJECT, means the instance form was used and the` |
|      - |  8888 | `		 * value is the second. */` |
|      6 |  8889 | `		ph7_value *pVal = apArg[0];` |
|      6 |  8890 | `		if( nArg > 1 ){` |
|      6 |  8891 | `			pVal = apArg[1];` |
|      2 |  8892 | `		}else if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|    ! 0 |  8893 | `			return PH7_OK;` |
|      - |  8894 | `		}` |
|      6 |  8895 | `		return ReflectStaticSlotWrite(pCtx, sRef.pClass, sRef.pAttr, pVal);` |
|      - |  8896 | `	}` |
|     27 |  8897 | `	rc = ReflectPropReceiver(pCtx, &sRef, apArg[0], &pObj, "setValue");` |
|     27 |  8898 | `	if( rc != PH7_OK ){` |
|    ! 0 |  8899 | `		return rc;` |
|      - |  8900 | `	}` |
|     27 |  8901 | `	pVmAttr = ReflectRefInstanceAttr(pCtx->pVm, pObj, &sRef);` |
|     27 |  8902 | `	if( pVmAttr == 0 ){` |
|      - |  8903 | `		/* No slot: the write handler answers, and for a class that has one the` |
|      - |  8904 | `		 * answer is its own refusal (php's PDORow refuses Reflection's write` |
|      - |  8905 | ``		 * with the same sentence `$row->p = 1` takes). */`` |
|      - |  8906 | `		PH7_NativePropCtx sNat;` |
|      - |  8907 | `		SyString sNatName;` |
|      - |  8908 | `		ph7_value sNatVal;` |
|     11 |  8909 | `		SyStringInitFromBuf(&sNatName, sRef.zName, (sxu32)sRef.nName);` |
|     11 |  8910 | `		PH7_MemObjInit(pCtx->pVm, &sNatVal);` |
|     10 |  8911 | `		if( PH7_ClassNativePropAsk(pObj, &sNat, PH7_NATIVE_PROP_WRITE, &sNatName, &sNatVal)` |
|      7 |  8912 | `		 && sNat.zThrowClass ){` |
|      3 |  8913 | `			PH7_MemObjRelease(&sNatVal);` |
|      6 |  8914 | `			return PH7_VmThrowExceptionCode(pCtx, sNat.zThrowClass, sNat.iThrowCode,` |
|      1 |  8915 | `				"%s", sNat.zThrowMsg);` |
|      - |  8916 | `		}` |
|      9 |  8917 | `		PH7_MemObjRelease(&sNatVal);` |
|      9 |  8918 | `		if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){` |
|      - |  8919 | `` 			/* A VIRTUAL property: php's write goes to the same handler `$o->p = v` `` |
|      - |  8920 | `			 * reaches -- the class's own write_property, which refuses the read-only` |
|      - |  8921 | `			 * half of the surface with its own sentence. Creating a slot here would` |
|      - |  8922 | `			 * give the object a real property php has none of. */` |
|      5 |  8923 | `			ph7_value sMagicVal, *pMagicArg = nArg > 1 ? apArg[1] : 0;` |
|      5 |  8924 | `			ph7_class_method *pSet = PH7_ClassExtractMethod(pObj->pClass,` |
|      - |  8925 | `				"__set", sizeof("__set")-1);` |
|      5 |  8926 | `			PH7_MemObjInit(pCtx->pVm, &sMagicVal);` |
|      5 |  8927 | `			if( pMagicArg == 0 ){` |
|    ! 0 |  8928 | `				pMagicArg = &sMagicVal;` |
|    ! 0 |  8929 | `			}` |
|      5 |  8930 | `			if( PH7_ClassNativePropOwns(pObj, &sNatName) ){` |
|      - |  8931 | `				PH7_NativePropCtx sStore;` |
|      5 |  8932 | `				sxi32 rcSt = PH7_OK;` |
|      6 |  8933 | `				if( PH7_ClassNativePropAsk(pObj, &sStore, PH7_NATIVE_PROP_STORE,` |
|      2 |  8934 | `						&sNatName, pMagicArg)` |
|      5 |  8935 | `				 && sStore.zThrowClass ){` |
|      4 |  8936 | `					rcSt = PH7_VmThrowExceptionCode(pCtx, sStore.zThrowClass,` |
|      1 |  8937 | `						sStore.iThrowCode, "%s", sStore.zThrowMsg);` |
|      1 |  8938 | `				}` |
|      5 |  8939 | `				PH7_MemObjRelease(&sMagicVal);` |
|      5 |  8940 | `				return rcSt;` |
|      - |  8941 | `			}` |
|    ! 0 |  8942 | `			if( pSet ){` |
|      - |  8943 | `				ph7_value sMagicName, *apMagic[2];` |
|    ! 0 |  8944 | `				PH7_MemObjInitFromString(pCtx->pVm, &sMagicName, 0);` |
|    ! 0 |  8945 | `				PH7_MemObjStringAppend(&sMagicName, sRef.zName, (sxu32)sRef.nName);` |
|    ! 0 |  8946 | `				sMagicName.nIdx = SXU32_HIGH;` |
|    ! 0 |  8947 | `				apMagic[0] = &sMagicName;` |
|    ! 0 |  8948 | `				apMagic[1] = pMagicArg;` |
|    ! 0 |  8949 | `				rc = PH7_VmCallMagicMethod(pCtx->pVm, pObj, pSet, 0, 2, apMagic);` |
|    ! 0 |  8950 | `				PH7_MemObjRelease(&sMagicName);` |
|    ! 0 |  8951 | `				PH7_MemObjRelease(&sMagicVal);` |
|    ! 0 |  8952 | `				return rc == PH7_ABORT ? PH7_ABORT : PH7_OK;` |
|      - |  8953 | `			}` |
|    ! 0 |  8954 | `			PH7_MemObjRelease(&sMagicVal);` |
|    ! 0 |  8955 | `			return PH7_OK;` |
|      - |  8956 | `		}` |
|      4 |  8957 | `		if( sRef.pAttr` |
|      5 |  8958 | `		 && (sRef.pAttr->iFlags & (PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_CONSTANT` |
|      2 |  8959 | `		                           \|PH7_CLASS_ATTR_HIDDEN)) == 0 ){` |
|      - |  8960 | `			/* A DECLARED property unset() took away: php's write RE-CREATES it,` |
|      - |  8961 | ``			 * exactly as `$o->p = v` does. PHL wrote nowhere and said nothing. */`` |
|      5 |  8962 | `			VmRecreateDeclaredAttr(pCtx->pVm, pObj, sRef.pAttr, &pVmAttr);` |
|      2 |  8963 | `		}` |
|      5 |  8964 | `		if( pVmAttr == 0 ){` |
|    ! 0 |  8965 | `			return PH7_OK;` |
|      - |  8966 | `		}` |
|      2 |  8967 | `	}` |
|     21 |  8968 | `	if( pVmAttr->pAttr && (pVmAttr->iState & VM_CLASS_ATTR_RDONLY) ){` |
|      - |  8969 | `		/* php's read-only handler refuses Reflection's write with the sentence` |
|      - |  8970 | ``		 * `$stmt->queryString = 'x'` takes. */`` |
|      3 |  8971 | `		return VmThrowNativeReadOnly(pCtx->pVm, pVmAttr->pAttr);` |
|      - |  8972 | `	}` |
|      - |  8973 | `	{` |
|     19 |  8974 | `		ph7_value *pVal = nArg > 1 ? apArg[1] : 0;` |
|      - |  8975 | `		ph7_value sNull;` |
|     19 |  8976 | `		PH7_MemObjInit(pCtx->pVm, &sNull);` |
|     19 |  8977 | `		if( pVal == 0 ){` |
|    ! 0 |  8978 | `			pVal = &sNull;` |
|    ! 0 |  8979 | `		}` |
|     19 |  8980 | `		rc = ReflectEnforceStore(pCtx, pVmAttr->nIdx, pVal);` |
|     19 |  8981 | `		if( rc != SXRET_OK ){` |
|      5 |  8982 | `			PH7_MemObjRelease(&sNull);` |
|      5 |  8983 | `			return rc;` |
|      - |  8984 | `		}` |
|     15 |  8985 | `		pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|     15 |  8986 | `		if( pSlot ){` |
|     15 |  8987 | `			PH7_MemObjStore(pVal, pSlot);` |
|     15 |  8988 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      7 |  8989 | `		}` |
|     15 |  8990 | `		PH7_MemObjRelease(&sNull);` |
|      - |  8991 | `	}` |
|     15 |  8992 | `	return PH7_OK;` |
|     17 |  8993 | `}` |
|     32 |  8994 | `static int vm_builtin_ReflectionProperty_isInitialized(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  8995 | `{` |
|      - |  8996 | `	ReflectMemberRef sRef;` |
|      - |  8997 | `	ph7_class_instance *pObj;` |
|      - |  8998 | `	VmClassAttr *pVmAttr;` |
|      - |  8999 | `	sxi32 rc;` |
|     34 |  9000 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  9001 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  9002 | `		return PH7_OK;` |
|      - |  9003 | `	}` |
|     34 |  9004 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|      - |  9005 | `		SyHashEntry *pSlot;` |
|      8 |  9006 | `		rc = ReflectMaterializeStatics(pCtx, sRef.pClass);` |
|      8 |  9007 | `		if( rc != SXRET_OK ){` |
|      5 |  9008 | `			return rc;` |
|      - |  9009 | `		}` |
|      3 |  9010 | `		pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&sRef.pAttr->nIdx, sizeof(sxu32));` |
|      5 |  9011 | `		ph7_result_bool(pCtx, pSlot == 0` |
|      2 |  9012 | `			\|\| (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) == 0);` |
|      3 |  9013 | `		return PH7_OK;` |
|      - |  9014 | `	}` |
|     27 |  9015 | `	rc = ReflectPropReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pObj, "isInitialized");` |
|     27 |  9016 | `	if( rc != PH7_OK ){` |
|    ! 0 |  9017 | `		return rc;` |
|      - |  9018 | `	}` |
|     27 |  9019 | `	pVmAttr = ReflectRefInstanceAttr(pCtx->pVm, pObj, &sRef);` |
|     26 |  9020 | `	if( pVmAttr == 0 && sRef.pAttr` |
|      5 |  9021 | `	 && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){` |
|      - |  9022 | `		/* A VIRTUAL property has no slot to be uninitialized: php asks the` |
|      - |  9023 | `		 * has_property handler, and ext/dom's answers yes for every name it` |
|      - |  9024 | `		 * declares. */` |
|      3 |  9025 | `		ph7_result_bool(pCtx, 1);` |
|      3 |  9026 | `		return PH7_OK;` |
|      - |  9027 | `	}` |
|     25 |  9028 | `	ph7_result_bool(pCtx, pVmAttr != 0 && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0);` |
|     25 |  9029 | `	return PH7_OK;` |
|     18 |  9030 | `}` |
|      - |  9031 | `/*` |
|      - |  9032 | `` * The property HOOKS (php 8.4). PHL compiles `public $p { get => ...; }` into`` |
|      - |  9033 | `` * synthesized methods named `__phl_hook_get_NAME` / `__phl_hook_set_NAME`, so`` |
|      - |  9034 | ` * the reflector php answers is a ReflectionMethod over one of those.` |
|      - |  9035 | ` */` |
|     30 |  9036 | `static int ReflectHookMethod(ph7_context *pCtx, ReflectMemberRef *pRef, int bSet,` |
|      - |  9037 | `	ph7_class_instance **ppOut)` |
|      3 |  9038 | `{` |
|      - |  9039 | `	char zName[128];` |
|      - |  9040 | `	ph7_value sClass, sName;` |
|      - |  9041 | `	ph7_value *apCtor[2];` |
|      - |  9042 | `	ph7_class *pDecl;` |
|      - |  9043 | `	sxi32 rc;` |
|      - |  9044 | `	int nName;` |
|     33 |  9045 | `	*ppOut = 0;` |
|     33 |  9046 | `	if( pRef->pAttr == 0 ){` |
|    ! 0 |  9047 | `		return PH7_OK;` |
|      - |  9048 | `	}` |
|     33 |  9049 | `	if( (pRef->pAttr->iFlags & (bSet ? PH7_CLASS_ATTR_HOOK_SET : PH7_CLASS_ATTR_HOOK_GET)) == 0 ){` |
|     15 |  9050 | `		return PH7_OK;` |
|      - |  9051 | `	}` |
|     21 |  9052 | `	pDecl = ReflectMemberDecl(pRef);` |
|     30 |  9053 | `	nName = SyBufferFormat(zName, sizeof(zName), "%s%.*s",` |
|      9 |  9054 | `		bSet ? "__phl_hook_set_" : "__phl_hook_get_", pRef->nName, pRef->zName);` |
|     21 |  9055 | `	PH7_MemObjInit(pCtx->pVm, &sClass);` |
|     21 |  9056 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|     21 |  9057 | `	ph7_value_string(&sClass, SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|     21 |  9058 | `	ph7_value_string(&sName, zName, nName);` |
|     21 |  9059 | `	apCtor[0] = &sClass;` |
|     21 |  9060 | `	apCtor[1] = &sName;` |
|     21 |  9061 | `	*ppOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|     21 |  9062 | `	PH7_MemObjRelease(&sClass);` |
|     21 |  9063 | `	PH7_MemObjRelease(&sName);` |
|     21 |  9064 | `	return rc;` |
|     18 |  9065 | `}` |
|     12 |  9066 | `static int vm_builtin_ReflectionProperty_getHooks(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  9067 | `{` |
|      - |  9068 | `	ReflectMemberRef sRef;` |
|     15 |  9069 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - |  9070 | `	int iHook;` |
|      6 |  9071 | `	SXUNUSED(nArg);` |
|      6 |  9072 | `	SXUNUSED(apArg);` |
|     15 |  9073 | `	if( pOut == 0 ){` |
|    ! 0 |  9074 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  9075 | `	}` |
|     15 |  9076 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  9077 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 |  9078 | `		return PH7_OK;` |
|      - |  9079 | `	}` |
|     39 |  9080 | `	for( iHook = 0 ; iHook < 2 ; iHook++ ){` |
|     27 |  9081 | `		ph7_class_instance *pMeth = 0;` |
|      - |  9082 | `		ph7_value sVal, *pKey;` |
|     27 |  9083 | `		sxi32 rc = ReflectHookMethod(pCtx, &sRef, iHook, &pMeth);` |
|     27 |  9084 | `		if( rc != PH7_OK ){` |
|    ! 0 |  9085 | `			return rc;` |
|      - |  9086 | `		}` |
|     27 |  9087 | `		if( pMeth == 0 ){` |
|     13 |  9088 | `			continue;` |
|      - |  9089 | `		}` |
|     17 |  9090 | `		pKey = ph7_context_new_scalar(pCtx);` |
|     17 |  9091 | `		if( pKey == 0 ){` |
|    ! 0 |  9092 | `			PH7_ClassInstanceUnref(pMeth);` |
|    ! 0 |  9093 | `			break;` |
|      - |  9094 | `		}` |
|     17 |  9095 | `		ph7_value_string(pKey, iHook ? "set" : "get", 3);` |
|     17 |  9096 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     17 |  9097 | `		sVal.x.pOther = pMeth;` |
|     17 |  9098 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|     17 |  9099 | `		ph7_array_add_elem(pOut, pKey, &sVal);   /* takes its own reference */` |
|     17 |  9100 | `		PH7_ClassInstanceUnref(pMeth);` |
|     10 |  9101 | `	}` |
|     15 |  9102 | `	ph7_result_value(pCtx, pOut);` |
|     15 |  9103 | `	return PH7_OK;` |
|      9 |  9104 | `}` |
|      - |  9105 | ``/* `get`/`set` out of a PropertyHookType case (or the bare string php also takes). */`` |
|     12 |  9106 | `static int ReflectHookKind(ph7_value *pArg)` |
|      1 |  9107 | `{` |
|      - |  9108 | `	const char *z;` |
|      - |  9109 | `	int n;` |
|     13 |  9110 | `	if( pArg == 0 ){` |
|    ! 0 |  9111 | `		return -1;` |
|      - |  9112 | `	}` |
|     13 |  9113 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|     13 |  9114 | `		ph7_class_instance *pCase = (ph7_class_instance *)pArg->x.pOther;` |
|     13 |  9115 | `		ph7_value *pVal = PH7_NativeAttr(pCase, "value");` |
|     13 |  9116 | `		if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 |  9117 | `			return -1;` |
|      - |  9118 | `		}` |
|     13 |  9119 | `		z = (const char *)SyBlobData(&pVal->sBlob);` |
|     13 |  9120 | `		n = (int)SyBlobLength(&pVal->sBlob);` |
|      7 |  9121 | `	}else{` |
|    ! 0 |  9122 | `		z = ph7_value_to_string(pArg, &n);` |
|      - |  9123 | `	}` |
|     13 |  9124 | `	if( n == 3 && SyMemcmp(z, "get", 3) == 0 ){` |
|      7 |  9125 | `		return 0;` |
|      - |  9126 | `	}` |
|      7 |  9127 | `	if( n == 3 && SyMemcmp(z, "set", 3) == 0 ){` |
|      7 |  9128 | `		return 1;` |
|      - |  9129 | `	}` |
|    ! 0 |  9130 | `	return -1;` |
|      7 |  9131 | `}` |
|      6 |  9132 | `static int vm_builtin_ReflectionProperty_hasHook(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9133 | `{` |
|      - |  9134 | `	ReflectMemberRef sRef;` |
|      7 |  9135 | `	int iHook = ReflectHookKind(nArg > 0 ? apArg[0] : 0);` |
|      7 |  9136 | `	int bYes = 0;` |
|      7 |  9137 | `	if( iHook >= 0 && ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|      7 |  9138 | `		bYes = (sRef.pAttr->iFlags & (iHook ? PH7_CLASS_ATTR_HOOK_SET : PH7_CLASS_ATTR_HOOK_GET)) != 0;` |
|      3 |  9139 | `	}` |
|      7 |  9140 | `	ph7_result_bool(pCtx, bYes);` |
|      7 |  9141 | `	return PH7_OK;` |
|      1 |  9142 | `}` |
|      6 |  9143 | `static int vm_builtin_ReflectionProperty_getHook(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9144 | `{` |
|      - |  9145 | `	ReflectMemberRef sRef;` |
|      7 |  9146 | `	ph7_class_instance *pMeth = 0;` |
|      7 |  9147 | `	int iHook = ReflectHookKind(nArg > 0 ? apArg[0] : 0);` |
|      - |  9148 | `	sxi32 rc;` |
|      7 |  9149 | `	if( iHook < 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 |  9150 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9151 | `		return PH7_OK;` |
|      - |  9152 | `	}` |
|      7 |  9153 | `	rc = ReflectHookMethod(pCtx, &sRef, iHook, &pMeth);` |
|      7 |  9154 | `	if( rc != PH7_OK ){` |
|    ! 0 |  9155 | `		return rc;` |
|      - |  9156 | `	}` |
|      7 |  9157 | `	if( pMeth == 0 ){` |
|      3 |  9158 | `		ph7_result_null(pCtx);` |
|      3 |  9159 | `		return PH7_OK;` |
|      - |  9160 | `	}` |
|      5 |  9161 | `	return ReflectResultObject(pCtx, pMeth);` |
|      4 |  9162 | `}` |
|      6 |  9163 | `static int vm_builtin_ReflectionProperty_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9164 | `{` |
|      7 |  9165 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  9166 | `	ReflectMemberRef sRef;` |
|      - |  9167 | `	ph7_value sTarget;` |
|      - |  9168 | `	const char *zClass;` |
|      - |  9169 | `	int nClass, rc;` |
|      7 |  9170 | `	if( pThis == 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9171 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  9172 | `		return PH7_OK;` |
|      - |  9173 | `	}` |
|      7 |  9174 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|      7 |  9175 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      7 |  9176 | `	ph7_value_string(&sTarget, zClass, nClass);` |
|      - |  9177 | `	/* 8 = Attribute::TARGET_PROPERTY */` |
|     10 |  9178 | `	rc = ReflectBuildAttrs(pCtx, &sRef.pAttr->aAttrs, "attr", &sTarget,` |
|      3 |  9179 | `		sRef.zName, sRef.nName, 0, 8, nArg, apArg);` |
|      7 |  9180 | `	PH7_MemObjRelease(&sTarget);` |
|      7 |  9181 | `	return rc;` |
|      4 |  9182 | `}` |
|      8 |  9183 | `static int vm_builtin_ReflectionProperty_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9184 | `{` |
|      4 |  9185 | `	SXUNUSED(nArg);` |
|      4 |  9186 | `	SXUNUSED(apArg);` |
|      9 |  9187 | `	return ReflectExportPropSelf(pCtx);` |
|      1 |  9188 | `}` |
|      - |  9189 | `/* ---- ReflectionClassConstant ---- */` |
|      - |  9190 | `/* ReflectionClassConstant::__construct(object\|string $class, string $constant) */` |
|     80 |  9191 | `static int vm_builtin_ReflectionClassConstant_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  9192 | `{` |
|     82 |  9193 | `	ph7_vm *pVm = pCtx->pVm;` |
|     82 |  9194 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     82 |  9195 | `	ph7_class *pClass, *pDecl = 0;` |
|      - |  9196 | `	const char *zConst;` |
|      - |  9197 | `	int nConst;` |
|      - |  9198 | `	SySet aMembers;` |
|      - |  9199 | `	sxu32 n;` |
|     82 |  9200 | `	int bFound = 0;` |
|     82 |  9201 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 |  9202 | `		return PH7_OK;` |
|      - |  9203 | `	}` |
|     82 |  9204 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|     82 |  9205 | `	if( pClass == 0 ){` |
|      - |  9206 | `		const char *zName;` |
|      - |  9207 | `		int nName;` |
|    ! 0 |  9208 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|    ! 0 |  9209 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 |  9210 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      - |  9211 | `	}` |
|     82 |  9212 | `	zConst = ph7_value_to_string(apArg[1], &nConst);` |
|     82 |  9213 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|     82 |  9214 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|    372 |  9215 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    366 |  9216 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|    366 |  9217 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zConst, nConst) ){` |
|     76 |  9218 | `			pDecl = pM->pDecl ? pM->pDecl : pClass;` |
|     76 |  9219 | `			bFound = 1;` |
|     76 |  9220 | `			break;` |
|      - |  9221 | `		}` |
|    146 |  9222 | `	}` |
|     82 |  9223 | `	SySetRelease(&aMembers);` |
|     82 |  9224 | `	if( !bFound ){` |
|     10 |  9225 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 |  9226 | `			"Constant %z::%.*s does not exist", &pClass->sName, nConst, zConst);` |
|      - |  9227 | `	}` |
|      - |  9228 | `	/* php's $class is the DECLARING class, not the one asked about. */` |
|     76 |  9229 | `	pClass = pDecl;` |
|    113 |  9230 | `	PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pClass->sName),` |
|     74 |  9231 | `		(int)SyStringLength(&pClass->sName));` |
|     76 |  9232 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", zConst, nConst);` |
|     76 |  9233 | `	return PH7_OK;` |
|     42 |  9234 | `}` |
|     22 |  9235 | `static int vm_builtin_ReflectionClassConstant_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9236 | `{` |
|      - |  9237 | `	ReflectMemberRef sRef;` |
|      - |  9238 | `	ph7_value *pVal;` |
|      - |  9239 | `	sxi32 rc;` |
|     11 |  9240 | `	SXUNUSED(nArg);` |
|     11 |  9241 | `	SXUNUSED(apArg);` |
|     23 |  9242 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9243 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9244 | `		return PH7_OK;` |
|      - |  9245 | `	}` |
|     23 |  9246 | `	rc = ReflectConstSlot(pCtx, sRef.pClass, sRef.pAttr, &pVal);` |
|     23 |  9247 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  9248 | `		return rc;` |
|      - |  9249 | `	}` |
|     23 |  9250 | `	if( pVal ){` |
|     23 |  9251 | `		ph7_result_value(pCtx, pVal);` |
|     12 |  9252 | `	}else{` |
|    ! 0 |  9253 | `		ph7_result_null(pCtx);` |
|      - |  9254 | `	}` |
|     23 |  9255 | `	return PH7_OK;` |
|     12 |  9256 | `}` |
|     28 |  9257 | `static int ReflectConstFlag(ph7_context *pCtx, int iWhat)` |
|      1 |  9258 | `{` |
|      - |  9259 | `	ReflectMemberRef sRef;` |
|     29 |  9260 | `	int bYes = 0;` |
|     29 |  9261 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) && sRef.pAttr ){` |
|     29 |  9262 | `		ph7_class_attr *pAttr = sRef.pAttr;` |
|     29 |  9263 | `		switch( iWhat ){` |
|      3 |  9264 | `		case 0: bYes = pAttr->iProtection == PH7_CLASS_PROT_PUBLIC; break;` |
|      5 |  9265 | `		case 1: bYes = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE; break;` |
|      5 |  9266 | `		case 2: bYes = pAttr->iProtection == PH7_CLASS_PROT_PROTECTED; break;` |
|      3 |  9267 | `		case 3: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0; break;` |
|     13 |  9268 | `		case 4: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0; break;` |
|      3 |  9269 | `		case 5: bYes = ReflectHasDeprecated(&pAttr->aAttrs); break;` |
|      3 |  9270 | `		default: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0; break;` |
|      - |  9271 | `		}` |
|     14 |  9272 | `	}` |
|     29 |  9273 | `	ph7_result_bool(pCtx, bYes);` |
|     29 |  9274 | `	return PH7_OK;` |
|      1 |  9275 | `}` |
|      - |  9276 | `#define REFLECT_CONST_FLAG(NAME,WHAT) \` |
|      - |  9277 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - |  9278 | `	{ \` |
|      - |  9279 | `		SXUNUSED(nArg); \` |
|      - |  9280 | `		SXUNUSED(apArg); \` |
|      - |  9281 | `		return ReflectConstFlag(pCtx, WHAT); \` |
|      - |  9282 | `	}` |
|      3 |  9283 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isPublic, 0)` |
|      5 |  9284 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isPrivate, 1)` |
|      5 |  9285 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isProtected, 2)` |
|      3 |  9286 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isFinal, 3)` |
|     13 |  9287 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isEnumCase, 4)` |
|      3 |  9288 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isDeprecated, 5)` |
|      3 |  9289 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_hasType, 6)` |
|      - |  9290 |  |
|      8 |  9291 | `static int vm_builtin_ReflectionClassConstant_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9292 | `{` |
|      - |  9293 | `	ReflectMemberRef sRef;` |
|      4 |  9294 | `	SXUNUSED(nArg);` |
|      4 |  9295 | `	SXUNUSED(apArg);` |
|      9 |  9296 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9297 | `		ph7_result_int(pCtx, 0);` |
|    ! 0 |  9298 | `		return PH7_OK;` |
|      - |  9299 | `	}` |
|      9 |  9300 | `	ph7_result_int64(pCtx, ReflectConstModifiers(sRef.pAttr));` |
|      9 |  9301 | `	return PH7_OK;` |
|      5 |  9302 | `}` |
|      6 |  9303 | `static int vm_builtin_ReflectionClassConstant_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9304 | `{` |
|      - |  9305 | `	ReflectMemberRef sRef;` |
|      3 |  9306 | `	SXUNUSED(nArg);` |
|      3 |  9307 | `	SXUNUSED(apArg);` |
|      7 |  9308 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) ){` |
|    ! 0 |  9309 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9310 | `		return PH7_OK;` |
|      - |  9311 | `	}` |
|      7 |  9312 | `	return ReflectResultClassOf(pCtx, ReflectMemberDecl(&sRef));` |
|      4 |  9313 | `}` |
|      2 |  9314 | `static int vm_builtin_ReflectionClassConstant_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9315 | `{` |
|      - |  9316 | `	ReflectMemberRef sRef;` |
|      1 |  9317 | `	SXUNUSED(nArg);` |
|      1 |  9318 | `	SXUNUSED(apArg);` |
|      2 |  9319 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) && sRef.pAttr` |
|      3 |  9320 | `	 && SyStringLength(&sRef.pAttr->sDoc) > 0 ){` |
|      4 |  9321 | `		ph7_result_string(pCtx, SyStringData(&sRef.pAttr->sDoc),` |
|      2 |  9322 | `			(int)SyStringLength(&sRef.pAttr->sDoc));` |
|      2 |  9323 | `	}else{` |
|    ! 0 |  9324 | `		ph7_result_bool(pCtx, 0);` |
|      - |  9325 | `	}` |
|      3 |  9326 | `	return PH7_OK;` |
|      1 |  9327 | `}` |
|      4 |  9328 | `static int vm_builtin_ReflectionClassConstant_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  9329 | `{` |
|      - |  9330 | `	ReflectMemberRef sRef;` |
|      2 |  9331 | `	SXUNUSED(nArg);` |
|      2 |  9332 | `	SXUNUSED(apArg);` |
|      4 |  9333 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0` |
|      4 |  9334 | `	 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|      6 |  9335 | `	 \|\| SyStringLength(&sRef.pAttr->sTypeName) < 1 ){` |
|    ! 0 |  9336 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9337 | `		return PH7_OK;` |
|      - |  9338 | `	}` |
|      - |  9339 | `	{` |
|      - |  9340 | `		char zType[192];` |
|      6 |  9341 | `		const char *zT = ReflectMemberTypeText(pCtx->pVm,sRef.pAttr,zType,sizeof(zType));` |
|      6 |  9342 | `		return ReflectResultObject(pCtx, ReflectMakeType(pCtx, zT, (int)SyStrlen(zT)));` |
|      - |  9343 | `	}` |
|      4 |  9344 | `}` |
|      2 |  9345 | `static int vm_builtin_ReflectionClassConstant_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9346 | `{` |
|      3 |  9347 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - |  9348 | `	ReflectMemberRef sRef;` |
|      - |  9349 | `	ph7_value sTarget;` |
|      - |  9350 | `	const char *zClass;` |
|      - |  9351 | `	int nClass, rc;` |
|      3 |  9352 | `	if( pThis == 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9353 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 |  9354 | `		return PH7_OK;` |
|      - |  9355 | `	}` |
|      3 |  9356 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|      3 |  9357 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      3 |  9358 | `	ph7_value_string(&sTarget, zClass, nClass);` |
|      - |  9359 | `	/* 16 = Attribute::TARGET_CLASS_CONSTANT */` |
|      4 |  9360 | `	rc = ReflectBuildAttrs(pCtx, &sRef.pAttr->aAttrs, "attr", &sTarget,` |
|      1 |  9361 | `		sRef.zName, sRef.nName, 0, 16, nArg, apArg);` |
|      3 |  9362 | `	PH7_MemObjRelease(&sTarget);` |
|      3 |  9363 | `	return rc;` |
|      2 |  9364 | `}` |
|      6 |  9365 | `static int vm_builtin_ReflectionClassConstant_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9366 | `{` |
|      3 |  9367 | `	SXUNUSED(nArg);` |
|      3 |  9368 | `	SXUNUSED(apArg);` |
|      7 |  9369 | `	return ReflectExportConstSelf(pCtx);` |
|      1 |  9370 | `}` |
|      - |  9371 | `/*` |
|      - |  9372 | ` * Declare chunk 3. Called from PH7_VmInstallReflection() where it used to be` |
|      - |  9373 | ` * compiled — after chunks 1 and 2, whose ReflectionClass and ReflectionMethod` |
|      - |  9374 | ` * these answer, and after PropertyHookType, which hasHook()/getHook() declare.` |
|      - |  9375 | ` *` |
|      - |  9376 | ` * The method tables are in php's own DECLARATION order.` |
|      - |  9377 | ` */` |
|   6721 |  9378 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionMember(ph7_vm *pVm)` |
|      5 |  9379 | `{` |
|      - |  9380 | `	static const PH7_NativePropDef aMemberProp[] = {` |
|      - |  9381 | `		{ "name",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  9382 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  9383 | `	};` |
|      - |  9384 | `	static const PH7_NativePropDef aPropProp[] = {` |
|      - |  9385 | `		{ "name",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  9386 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - |  9387 | `		/* PHL-only: the instance a DYNAMIC property was reached through (§7.4 (e)) */` |
|      - |  9388 | `		{ RP_DYNOBJ, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - |  9389 | `	};` |
|      - |  9390 | `	static const PH7_NativeConstDef aPropConst[] = {` |
|      - |  9391 | `		{ "IS_STATIC",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,   0, 0.0 },` |
|      - |  9392 | `		{ "IS_READONLY",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 128,  0, 0.0 },` |
|      - |  9393 | `		{ "IS_PUBLIC",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,    0, 0.0 },` |
|      - |  9394 | `		{ "IS_PROTECTED",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,    0, 0.0 },` |
|      - |  9395 | `		{ "IS_PRIVATE",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,    0, 0.0 },` |
|      - |  9396 | `		{ "IS_ABSTRACT",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64,   0, 0.0 },` |
|      - |  9397 | `		{ "IS_PROTECTED_SET", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2048, 0, 0.0 },` |
|      - |  9398 | `		{ "IS_PRIVATE_SET",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4096, 0, 0.0 },` |
|      - |  9399 | `		{ "IS_VIRTUAL",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 512,  0, 0.0 },` |
|      - |  9400 | `		{ "IS_FINAL",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32,   0, 0.0 },` |
|      - |  9401 | `	};` |
|      - |  9402 | `	static const PH7_NativeMethodDef aPropMethod[] = {` |
|      - |  9403 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - |  9404 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $property", "",` |
|      - |  9405 | `		  vm_builtin_ReflectionProperty_construct },` |
|      - |  9406 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionProperty_toString },` |
|      - |  9407 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionProperty_getName },` |
|      - |  9408 | `		{ "getMangledName", PH7_MOD_PUBLIC, "", "string",` |
|      - |  9409 | `		  vm_builtin_ReflectionProperty_getMangledName },` |
|      - |  9410 | `		{ "getValue",    PH7_MOD_PUBLIC, "?object $object = null", "@mixed",` |
|      - |  9411 | `		  vm_builtin_ReflectionProperty_getValue },` |
|      - |  9412 | `		{ "setValue",    PH7_MOD_PUBLIC, "mixed $objectOrValue, mixed $value = ?", "@void",` |
|      - |  9413 | `		  vm_builtin_ReflectionProperty_setValue },` |
|      - |  9414 | `		{ "getRawValue", PH7_MOD_PUBLIC, "object $object", "mixed",` |
|      - |  9415 | `		  vm_builtin_ReflectionProperty_getValue },` |
|      - |  9416 | `		{ "setRawValue", PH7_MOD_PUBLIC, "object $object, mixed $value", "void",` |
|      - |  9417 | `		  vm_builtin_ReflectionProperty_setValue },` |
|      - |  9418 | `		/* On a non-lazy object — the only kind PHL has — php's two lazy writers` |
|      - |  9419 | `		 * are an ordinary raw write and a no-op. */` |
|      - |  9420 | `		{ "setRawValueWithoutLazyInitialization", PH7_MOD_PUBLIC,` |
|      - |  9421 | `		  "object $object, mixed $value", "void", vm_builtin_ReflectionProperty_setValue },` |
|      - |  9422 | `		{ "skipLazyInitialization", PH7_MOD_PUBLIC, "object $object", "void",` |
|      - |  9423 | `		  vm_builtin_ReflectionProperty_noop },` |
|      - |  9424 | `		{ "isLazy",      PH7_MOD_PUBLIC, "object $object", "bool",` |
|      - |  9425 | `		  vm_builtin_ReflectionProperty_false },` |
|      - |  9426 | `		{ "isInitialized", PH7_MOD_PUBLIC, "?object $object = null", "@bool",` |
|      - |  9427 | `		  vm_builtin_ReflectionProperty_isInitialized },` |
|      - |  9428 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isPublic },` |
|      - |  9429 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isPrivate },` |
|      - |  9430 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isProtected },` |
|      - |  9431 | `		{ "isPrivateSet", PH7_MOD_PUBLIC, "", "bool",` |
|      - |  9432 | `		  vm_builtin_ReflectionProperty_isPrivateSet },` |
|      - |  9433 | `		{ "isProtectedSet", PH7_MOD_PUBLIC, "", "bool",` |
|      - |  9434 | `		  vm_builtin_ReflectionProperty_isProtectedSet },` |
|      - |  9435 | `		{ "isStatic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isStatic },` |
|      - |  9436 | `		{ "isReadOnly",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isReadOnly },` |
|      - |  9437 | `		{ "isDefault",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isDefault },` |
|      - |  9438 | `		{ "isDynamic",   PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isDynamic },` |
|      - |  9439 | `		/* PHL has no abstract properties: the modifier only exists on an` |
|      - |  9440 | `		 * interface's hooked property stub, which PHL does not model. */` |
|      - |  9441 | `		{ "isAbstract",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_false },` |
|      - |  9442 | `		{ "isVirtual",   PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isVirtual },` |
|      - |  9443 | `		{ "isPromoted",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isPromoted },` |
|      - |  9444 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionProperty_getModifiers },` |
|      - |  9445 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|      - |  9446 | `		  vm_builtin_ReflectionProperty_getDeclaringClass },` |
|      - |  9447 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionProperty_getDocComment },` |
|      - |  9448 | `		{ "setAccessible", PH7_MOD_PUBLIC, "bool $accessible", "@void",` |
|      - |  9449 | `		  vm_builtin_ReflectionProperty_setAccessible },` |
|      - |  9450 | `		{ "getType",     PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionProperty_getType },` |
|      - |  9451 | `		/* php's settable type differs from the declared one only for a hooked` |
|      - |  9452 | ``		 * property with a widening `set` — which PHL does not model. */`` |
|      - |  9453 | `		{ "getSettableType", PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|      - |  9454 | `		  vm_builtin_ReflectionProperty_getType },` |
|      - |  9455 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_hasType },` |
|      - |  9456 | `		{ "hasDefaultValue", PH7_MOD_PUBLIC, "", "bool",` |
|      - |  9457 | `		  vm_builtin_ReflectionProperty_hasDefaultValue },` |
|      - |  9458 | `		{ "getDefaultValue", PH7_MOD_PUBLIC, "", "@mixed",` |
|      - |  9459 | `		  vm_builtin_ReflectionProperty_getDefaultValue },` |
|      - |  9460 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - |  9461 | `		  vm_builtin_ReflectionProperty_getAttributes },` |
|      - |  9462 | `		{ "hasHooks",    PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_hasHooks },` |
|      - |  9463 | `		{ "getHooks",    PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionProperty_getHooks },` |
|      - |  9464 | `		{ "hasHook",     PH7_MOD_PUBLIC, "PropertyHookType $type", "bool",` |
|      - |  9465 | `		  vm_builtin_ReflectionProperty_hasHook },` |
|      - |  9466 | `		{ "getHook",     PH7_MOD_PUBLIC, "PropertyHookType $type", "?ReflectionMethod",` |
|      - |  9467 | `		  vm_builtin_ReflectionProperty_getHook },` |
|      - |  9468 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isFinal },` |
|      - |  9469 | `	};` |
|      - |  9470 | `	static const PH7_NativeConstDef aConstConst[] = {` |
|      - |  9471 | `		{ "IS_PUBLIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,  0, 0.0 },` |
|      - |  9472 | `		{ "IS_PROTECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,  0, 0.0 },` |
|      - |  9473 | `		{ "IS_PRIVATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,  0, 0.0 },` |
|      - |  9474 | `		{ "IS_FINAL",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|      - |  9475 | `	};` |
|      - |  9476 | `	static const PH7_NativeMethodDef aConstMethod[] = {` |
|      - |  9477 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - |  9478 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|      - |  9479 | `		  vm_builtin_ReflectionClassConstant_construct },` |
|      - |  9480 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string",` |
|      - |  9481 | `		  vm_builtin_ReflectionClassConstant_toString },` |
|      - |  9482 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionProperty_getName },` |
|      - |  9483 | `		{ "getValue",    PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_ReflectionClassConstant_getValue },` |
|      - |  9484 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClassConstant_isPublic },` |
|      - |  9485 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClassConstant_isPrivate },` |
|      - |  9486 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool",` |
|      - |  9487 | `		  vm_builtin_ReflectionClassConstant_isProtected },` |
|      - |  9488 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClassConstant_isFinal },` |
|      - |  9489 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int",` |
|      - |  9490 | `		  vm_builtin_ReflectionClassConstant_getModifiers },` |
|      - |  9491 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|      - |  9492 | `		  vm_builtin_ReflectionClassConstant_getDeclaringClass },` |
|      - |  9493 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false",` |
|      - |  9494 | `		  vm_builtin_ReflectionClassConstant_getDocComment },` |
|      - |  9495 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - |  9496 | `		  vm_builtin_ReflectionClassConstant_getAttributes },` |
|      - |  9497 | `		{ "isEnumCase",  PH7_MOD_PUBLIC, "", "bool",` |
|      - |  9498 | `		  vm_builtin_ReflectionClassConstant_isEnumCase },` |
|      - |  9499 | `		{ "isDeprecated", PH7_MOD_PUBLIC, "", "bool",` |
|      - |  9500 | `		  vm_builtin_ReflectionClassConstant_isDeprecated },` |
|      - |  9501 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClassConstant_hasType },` |
|      - |  9502 | `		{ "getType",     PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|      - |  9503 | `		  vm_builtin_ReflectionClassConstant_getType },` |
|      - |  9504 | `	};` |
|      - |  9505 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  9506 | `		{ "ReflectionProperty", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  9507 | `		  aPropMethod, SX_ARRAYSIZE(aPropMethod),` |
|      - |  9508 | `		  aPropConst, SX_ARRAYSIZE(aPropConst),` |
|      - |  9509 | `		  aPropProp, SX_ARRAYSIZE(aPropProp), 0, 0, 0 },` |
|      - |  9510 | `		{ "ReflectionClassConstant", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  9511 | `		  aConstMethod, SX_ARRAYSIZE(aConstMethod),` |
|      - |  9512 | `		  aConstConst, SX_ARRAYSIZE(aConstConst),` |
|      - |  9513 | `		  aMemberProp, SX_ARRAYSIZE(aMemberProp), 0, 0, 0 },` |
|      - |  9514 | `	};` |
|   6726 |  9515 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  9516 | `}` |
|      - |  9517 | `/*` |
|      - |  9518 | ` * ---------------------------------------------------------------------------` |
|      - |  9519 | ` * The ReflectionEnum family — chunk 6, and the end of __phl_rcinfo.` |
|      - |  9520 | ` *` |
|      - |  9521 | `` * Three classes that are very nearly their parents: `ReflectionEnum` IS a`` |
|      - |  9522 | `` * `ReflectionClass` that refuses a non-enum, and `ReflectionEnumUnitCase` IS a`` |
|      - |  9523 | `` * `ReflectionClassConstant` that refuses a constant which is not a case. What`` |
|      - |  9524 | ` * is their own is the CASE list, which the engine already holds in declaration` |
|      - |  9525 | `` * order on `ph7_class::aEnumCases` — the chunk read a marshalled copy of it out`` |
|      - |  9526 | `` * of `__phl_rcinfo`, the descriptor builder that dies with this conversion.`` |
|      - |  9527 | ` * ---------------------------------------------------------------------------` |
|      - |  9528 | ` */` |
|      - |  9529 |  |
|      - |  9530 | `/* The enum case named zName, or NULL. A case is a class CONSTANT, so the match` |
|      - |  9531 | `` * is case-SENSITIVE: php's hasCase('hearts') is false for `case Hearts`. */`` |
|     28 |  9532 | `static ph7_class_attr * ReflectEnumCase(ph7_class *pClass, const char *zName, int nName)` |
|      1 |  9533 | `{` |
|     29 |  9534 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      - |  9535 | `	sxu32 n;` |
|     29 |  9536 | `	if( nName < 1 ){` |
|    ! 0 |  9537 | `		return 0;` |
|      - |  9538 | `	}` |
|     67 |  9539 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     52 |  9540 | `		if( (int)SyStringLength(&apCase[n]->sName) == nName` |
|     38 |  9541 | `		 && SyMemcmp(SyStringData(&apCase[n]->sName), zName, (sxu32)nName) == 0 ){` |
|     15 |  9542 | `			return apCase[n];` |
|      - |  9543 | `		}` |
|     20 |  9544 | `	}` |
|     15 |  9545 | `	return 0;` |
|     15 |  9546 | `}` |
|      - |  9547 | `/*` |
|      - |  9548 | ` * One case reflector for an already-validated case. php answers a BackedCase` |
|      - |  9549 | ` * for a backed enum and a UnitCase for a pure one, and both carry the same two` |
|      - |  9550 | ` * slots ReflectionClassConstant does — an enum cannot extend anything, so the` |
|      - |  9551 | ` * declaring class is always the enum itself.` |
|      - |  9552 | ` */` |
|     46 |  9553 | `static ph7_class_instance * ReflectEnumCaseNew(ph7_context *pCtx, ph7_class *pEnum,` |
|      - |  9554 | `	const SyString *pCase)` |
|      1 |  9555 | `{` |
|     47 |  9556 | `	ph7_vm *pVm = pCtx->pVm;` |
|     70 |  9557 | `	const char *zClass = pEnum->nEnumBacking` |
|     23 |  9558 | `		? "ReflectionEnumBackedCase" : "ReflectionEnumUnitCase";` |
|     47 |  9559 | `	ph7_class *pRC = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|     47 |  9560 | `	ph7_class_instance *pObj = pRC ? PH7_NewClassInstance(pVm, pRC) : 0;` |
|     47 |  9561 | `	if( pObj == 0 ){` |
|    ! 0 |  9562 | `		return 0;` |
|      - |  9563 | `	}` |
|     70 |  9564 | `	PH7_NativeSetAttrStr(pVm, pObj, "class", SyStringData(&pEnum->sName),` |
|     46 |  9565 | `		(int)SyStringLength(&pEnum->sName));` |
|     47 |  9566 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(pCase), (int)SyStringLength(pCase));` |
|     47 |  9567 | `	return pObj;` |
|     24 |  9568 | `}` |
|      - |  9569 | `/* ReflectionEnum::__construct(object\|string $objectOrClass) */` |
|     30 |  9570 | `static int vm_builtin_ReflectionEnum_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9571 | `{` |
|      - |  9572 | `	ph7_class *pClass;` |
|     31 |  9573 | `	sxi32 rc = vm_builtin_ReflectionClass_construct(pCtx, nArg, apArg);` |
|     31 |  9574 | `	if( rc != PH7_OK ){` |
|      5 |  9575 | `		return rc; /* "Class %s does not exist", already php's */` |
|      - |  9576 | `	}` |
|     27 |  9577 | `	pClass = ReflectClassOf(pCtx);` |
|     27 |  9578 | `	if( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     10 |  9579 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 |  9580 | `			"Class \"%z\" is not an enum", &pClass->sName);` |
|      - |  9581 | `	}` |
|     21 |  9582 | `	return PH7_OK;` |
|     16 |  9583 | `}` |
|     12 |  9584 | `static int vm_builtin_ReflectionEnum_hasCase(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9585 | `{` |
|     13 |  9586 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  9587 | `	const char *zName;` |
|      - |  9588 | `	int nName;` |
|     13 |  9589 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  9590 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  9591 | `		return PH7_OK;` |
|      - |  9592 | `	}` |
|     13 |  9593 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     13 |  9594 | `	ph7_result_bool(pCtx, ReflectEnumCase(pClass, zName, nName) != 0);` |
|     13 |  9595 | `	return PH7_OK;` |
|      7 |  9596 | `}` |
|      - |  9597 | `/*` |
|      - |  9598 | ` * getCase(): php tells the two failures apart — a name that is a constant but` |
|      - |  9599 | ` * not a case is "X::K is not a case", one that is neither is "Case X::K does` |
|      - |  9600 | ` * not exist". The chunk answered the second for both.` |
|      - |  9601 | ` */` |
|     16 |  9602 | `static int vm_builtin_ReflectionEnum_getCase(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9603 | `{` |
|     17 |  9604 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  9605 | `	ph7_class_attr *pCase;` |
|      - |  9606 | `	const char *zName;` |
|      - |  9607 | `	int nName;` |
|     17 |  9608 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 |  9609 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9610 | `		return PH7_OK;` |
|      - |  9611 | `	}` |
|     17 |  9612 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     17 |  9613 | `	pCase = ReflectEnumCase(pClass, zName, nName);` |
|     17 |  9614 | `	if( pCase == 0 ){` |
|      7 |  9615 | `		if( nName > 0 && SyHashGet(&pClass->hConst, (const void *)zName, (sxu32)nName) ){` |
|      4 |  9616 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 |  9617 | `				"%z::%.*s is not a case", &pClass->sName, nName, zName);` |
|      - |  9618 | `		}` |
|      7 |  9619 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 |  9620 | `			"Case %z::%.*s does not exist", &pClass->sName, nName, zName);` |
|      - |  9621 | `	}` |
|     11 |  9622 | `	return ReflectResultObject(pCtx, ReflectEnumCaseNew(pCtx, pClass, &pCase->sName));` |
|      9 |  9623 | `}` |
|     14 |  9624 | `static int vm_builtin_ReflectionEnum_getCases(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9625 | `{` |
|     15 |  9626 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     15 |  9627 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      7 |  9628 | `	SXUNUSED(nArg);` |
|      7 |  9629 | `	SXUNUSED(apArg);` |
|     15 |  9630 | `	if( pOut == 0 ){` |
|    ! 0 |  9631 | `		return PH7_ContextMemoryError(pCtx);` |
|      - |  9632 | `	}` |
|     15 |  9633 | `	if( pClass ){` |
|     15 |  9634 | `		ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      - |  9635 | `		sxu32 n;` |
|     51 |  9636 | `		for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     37 |  9637 | `			ReflectTypeListAdd(pCtx, pOut, ReflectEnumCaseNew(pCtx, pClass, &apCase[n]->sName));` |
|     19 |  9638 | `		}` |
|      7 |  9639 | `	}` |
|     15 |  9640 | `	ph7_result_value(pCtx, pOut);` |
|     15 |  9641 | `	return PH7_OK;` |
|      8 |  9642 | `}` |
|     12 |  9643 | `static int vm_builtin_ReflectionEnum_isBacked(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9644 | `{` |
|     13 |  9645 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      6 |  9646 | `	SXUNUSED(nArg);` |
|      6 |  9647 | `	SXUNUSED(apArg);` |
|     13 |  9648 | `	ph7_result_bool(pCtx, pClass != 0 && pClass->nEnumBacking != 0);` |
|     13 |  9649 | `	return PH7_OK;` |
|      1 |  9650 | `}` |
|     16 |  9651 | `static int vm_builtin_ReflectionEnum_getBackingType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9652 | `{` |
|     17 |  9653 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - |  9654 | `	const char *zText;` |
|      - |  9655 | `	ph7_class_instance *pType;` |
|      8 |  9656 | `	SXUNUSED(nArg);` |
|      8 |  9657 | `	SXUNUSED(apArg);` |
|     17 |  9658 | `	if( pClass == 0 \|\| pClass->nEnumBacking == 0 ){` |
|      5 |  9659 | `		ph7_result_null(pCtx); /* a PURE enum has no backing type */` |
|      5 |  9660 | `		return PH7_OK;` |
|      - |  9661 | `	}` |
|     13 |  9662 | `	zText = (pClass->nEnumBacking & MEMOBJ_INT) ? "int" : "string";` |
|     13 |  9663 | `	pType = ReflectMakeType(pCtx, zText, (int)SyStrlen(zText));` |
|     13 |  9664 | `	if( pType == 0 ){` |
|    ! 0 |  9665 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9666 | `		return PH7_OK;` |
|      - |  9667 | `	}` |
|     13 |  9668 | `	PH7_NativeResultObject(pCtx, pType);` |
|     13 |  9669 | `	return PH7_OK;` |
|      9 |  9670 | `}` |
|      - |  9671 | `/*` |
|      - |  9672 | ` * ReflectionEnumUnitCase::__construct(object\|string $class, string $constant).` |
|      - |  9673 | ` *` |
|      - |  9674 | ` * The parent raises for a constant that does not exist; what is left is "not a` |
|      - |  9675 | ` * case", and php words that one WITHOUT asking whether the class is an enum at` |
|      - |  9676 | `` * all — `new ReflectionEnumUnitCase('Plain', 'K')` on an ordinary class says`` |
|      - |  9677 | `` * `Constant Plain::K is not a case`, not "is not an enum" as the chunk did.`` |
|      - |  9678 | ` * A non-enum's constant can never carry the ENUMCASE bit, so the one screen` |
|      - |  9679 | ` * answers both shapes.` |
|      - |  9680 | ` */` |
|     18 |  9681 | `static int vm_builtin_ReflectionEnumCase_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9682 | `{` |
|      - |  9683 | `	ReflectMemberRef sRef;` |
|     19 |  9684 | `	sxi32 rc = vm_builtin_ReflectionClassConstant_construct(pCtx, nArg, apArg);` |
|     19 |  9685 | `	if( rc != PH7_OK ){` |
|      3 |  9686 | `		return rc;` |
|      - |  9687 | `	}` |
|     17 |  9688 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9689 | `		return PH7_OK;` |
|      - |  9690 | `	}` |
|     17 |  9691 | `	if( (sRef.pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){` |
|     10 |  9692 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      6 |  9693 | `			"Constant %z::%.*s is not a case", &sRef.pClass->sName, sRef.nName, sRef.zName);` |
|      - |  9694 | `	}` |
|     11 |  9695 | `	return PH7_OK;` |
|     10 |  9696 | `}` |
|      - |  9697 | `/* ReflectionEnumBackedCase::__construct() — the same, plus php's screen for a` |
|      - |  9698 | ` * PURE enum's case, which has no backing value to answer with. */` |
|      6 |  9699 | `static int vm_builtin_ReflectionEnumBackedCase_construct(ph7_context *pCtx, int nArg,` |
|      - |  9700 | `	ph7_value **apArg)` |
|      1 |  9701 | `{` |
|      - |  9702 | `	ReflectMemberRef sRef;` |
|      7 |  9703 | `	sxi32 rc = vm_builtin_ReflectionEnumCase_construct(pCtx, nArg, apArg);` |
|      7 |  9704 | `	if( rc != PH7_OK ){` |
|    ! 0 |  9705 | `		return rc;` |
|      - |  9706 | `	}` |
|      7 |  9707 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9708 | `		return PH7_OK;` |
|      - |  9709 | `	}` |
|      7 |  9710 | `	if( sRef.pClass->nEnumBacking == 0 ){` |
|      4 |  9711 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 |  9712 | `			"Enum case %z::%.*s is not a backed case", &sRef.pClass->sName,` |
|      1 |  9713 | `			sRef.nName, sRef.zName);` |
|      - |  9714 | `	}` |
|      5 |  9715 | `	return PH7_OK;` |
|      4 |  9716 | `}` |
|      6 |  9717 | `static int vm_builtin_ReflectionEnumCase_getEnum(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  9718 | `{` |
|      7 |  9719 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  9720 | `	ReflectMemberRef sRef;` |
|      - |  9721 | `	ph7_class *pRC;` |
|      - |  9722 | `	ph7_class_instance *pObj;` |
|      3 |  9723 | `	SXUNUSED(nArg);` |
|      3 |  9724 | `	SXUNUSED(apArg);` |
|      7 |  9725 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) ){` |
|    ! 0 |  9726 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9727 | `		return PH7_OK;` |
|      - |  9728 | `	}` |
|      7 |  9729 | `	pRC = PH7_VmExtractClass(pVm, "ReflectionEnum", sizeof("ReflectionEnum")-1, FALSE, 0);` |
|      7 |  9730 | `	pObj = pRC ? PH7_NewClassInstance(pVm, pRC) : 0;` |
|      7 |  9731 | `	if( pObj == 0 ){` |
|    ! 0 |  9732 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9733 | `		return PH7_OK;` |
|      - |  9734 | `	}` |
|     10 |  9735 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(&sRef.pClass->sName),` |
|      6 |  9736 | `		(int)SyStringLength(&sRef.pClass->sName));` |
|      7 |  9737 | `	return ReflectResultObject(pCtx, pObj);` |
|      4 |  9738 | `}` |
|      - |  9739 | ``/* getBackingValue(): the `value` the case singleton carries. The singleton is`` |
|      - |  9740 | ` * the constant's own value, so the parent's accessor materializes it. */` |
|     16 |  9741 | `static int vm_builtin_ReflectionEnumCase_getBackingValue(ph7_context *pCtx, int nArg,` |
|      - |  9742 | `	ph7_value **apArg)` |
|      1 |  9743 | `{` |
|      - |  9744 | `	ReflectMemberRef sRef;` |
|      - |  9745 | `	ph7_value *pVal, *pBacking;` |
|      - |  9746 | `	sxi32 rc;` |
|      8 |  9747 | `	SXUNUSED(nArg);` |
|      8 |  9748 | `	SXUNUSED(apArg);` |
|     17 |  9749 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 |  9750 | `		ph7_result_null(pCtx);` |
|    ! 0 |  9751 | `		return PH7_OK;` |
|      - |  9752 | `	}` |
|     17 |  9753 | `	rc = ReflectConstSlot(pCtx, sRef.pClass, sRef.pAttr, &pVal);` |
|     17 |  9754 | `	if( rc != SXRET_OK ){` |
|    ! 0 |  9755 | `		return rc;` |
|      - |  9756 | `	}` |
|     17 |  9757 | `	pBacking = (pVal && (pVal->iFlags & MEMOBJ_OBJ))` |
|     24 |  9758 | `		? PH7_NativeAttr((ph7_class_instance *)pVal->x.pOther, "value") : 0;` |
|     17 |  9759 | `	if( pBacking ){` |
|     17 |  9760 | `		ph7_result_value(pCtx, pBacking);` |
|      9 |  9761 | `	}else{` |
|    ! 0 |  9762 | `		ph7_result_null(pCtx);` |
|      - |  9763 | `	}` |
|     17 |  9764 | `	return PH7_OK;` |
|      9 |  9765 | `}` |
|      - |  9766 | `/*` |
|      - |  9767 | ` * Declare the three. Called from PH7_VmInstallReflection() where chunk 6 used` |
|      - |  9768 | ` * to be compiled, so both parents are installed (PH7_ClassInherit COPIES a` |
|      - |  9769 | ` * base's methods down — a native subclass needs its parent declared first).` |
|      - |  9770 | ` *` |
|      - |  9771 | ` * The uncloneable/unserializable flags are php's answer for each class and do` |
|      - |  9772 | ``  * NOT come down with the inheritance: without them `clone $enumReflector` `` |
|      - |  9773 | ` * reached the private __clone it inherited and reported that instead.` |
|      - |  9774 | ` */` |
|   6721 |  9775 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionEnum(ph7_vm *pVm)` |
|      5 |  9776 | `{` |
|      - |  9777 | `	static const PH7_NativeMethodDef aEnumMethod[] = {` |
|      - |  9778 | `		{ "__construct",    PH7_MOD_PUBLIC, "object\|string $objectOrClass", "",` |
|      - |  9779 | `		  vm_builtin_ReflectionEnum_construct },` |
|      - |  9780 | `		{ "hasCase",        PH7_MOD_PUBLIC, "string $name", "bool",` |
|      - |  9781 | `		  vm_builtin_ReflectionEnum_hasCase },` |
|      - |  9782 | `		{ "getCase",        PH7_MOD_PUBLIC, "string $name", "ReflectionEnumUnitCase",` |
|      - |  9783 | `		  vm_builtin_ReflectionEnum_getCase },` |
|      - |  9784 | `		{ "getCases",       PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionEnum_getCases },` |
|      - |  9785 | `		{ "isBacked",       PH7_MOD_PUBLIC, "", "bool",  vm_builtin_ReflectionEnum_isBacked },` |
|      - |  9786 | `		{ "getBackingType", PH7_MOD_PUBLIC, "", "?ReflectionNamedType",` |
|      - |  9787 | `		  vm_builtin_ReflectionEnum_getBackingType },` |
|      - |  9788 | `	};` |
|      - |  9789 | `	static const PH7_NativeMethodDef aCaseMethod[] = {` |
|      - |  9790 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|      - |  9791 | `		  vm_builtin_ReflectionEnumCase_construct },` |
|      - |  9792 | `		{ "getEnum",     PH7_MOD_PUBLIC, "", "ReflectionEnum",` |
|      - |  9793 | `		  vm_builtin_ReflectionEnumCase_getEnum },` |
|      - |  9794 | `		/* php redeclares getValue() on the case reflector for its narrower` |
|      - |  9795 | `		 * return type; the body is the parent's. */` |
|      - |  9796 | `		{ "getValue",    PH7_MOD_PUBLIC, "", "UnitEnum",` |
|      - |  9797 | `		  vm_builtin_ReflectionClassConstant_getValue },` |
|      - |  9798 | `	};` |
|      - |  9799 | `	static const PH7_NativeMethodDef aBackedMethod[] = {` |
|      - |  9800 | `		{ "__construct",     PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|      - |  9801 | `		  vm_builtin_ReflectionEnumBackedCase_construct },` |
|      - |  9802 | `		{ "getBackingValue", PH7_MOD_PUBLIC, "", "string\|int",` |
|      - |  9803 | `		  vm_builtin_ReflectionEnumCase_getBackingValue },` |
|      - |  9804 | `	};` |
|      - |  9805 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - |  9806 | `		{ "ReflectionEnum", "ReflectionClass", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  9807 | `		  aEnumMethod, SX_ARRAYSIZE(aEnumMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  9808 | `		{ "ReflectionEnumUnitCase", "ReflectionClassConstant", 0,` |
|      - |  9809 | `		  PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  9810 | `		  aCaseMethod, SX_ARRAYSIZE(aCaseMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  9811 | `		{ "ReflectionEnumBackedCase", "ReflectionEnumUnitCase", 0,` |
|      - |  9812 | `		  PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - |  9813 | `		  aBackedMethod, SX_ARRAYSIZE(aBackedMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - |  9814 | `	};` |
|   6726 |  9815 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 |  9816 | `}` |
|      - |  9817 | `/*` |
|      - |  9818 | ` * ---------------------------------------------------------------------------` |
|      - |  9819 | ` * php's Reflection EXPORT format — chunk 9, the last of the library's PHP.` |
|      - |  9820 | ` *` |
|      - |  9821 | ` * Every Reflector's __toString(). It is a byte-exact format with no API of its` |
|      - |  9822 | ` * own, so the chunk was written against the PUBLIC reflection API of whatever` |
|      - |  9823 | ` * it was printing — the only thing PHP could reach. All of those targets are C` |
|      - |  9824 | ` * now, so this reads the engine directly: the member walk for order and` |
|      - |  9825 | ` * visibility, ReflectParamAt for parameters, ReflectExportValue (built for the` |
|      - |  9826 | ` * attribute-argument block) for every value.` |
|      - |  9827 | ` *` |
|      - |  9828 | ` * Defined at the END of the file because it needs all of that; the five entry` |
|      - |  9829 | ` * points are forward-declared above their callers.` |
|      - |  9830 | ` * ---------------------------------------------------------------------------` |
|      - |  9831 | ` */` |
|      - |  9832 |  |
|      - |  9833 | `/*` |
|      - |  9834 | ` * "internal:<extension>" / "user" — the first tag of every function and class` |
|      - |  9835 | `` * head. php NAMES the extension there (`<internal:json>`), and this printed`` |
|      - |  9836 | `` * `internal:Core` for all 878 functions and 197 classes because the engine had`` |
|      - |  9837 | ` * no partition to name one from. iExt < 0 is a userland target.` |
|      - |  9838 | ` */` |
|   4872 |  9839 | `static void ReflectExportKind(SyBlob *pOut, int iExt, int bDeprecated)` |
|      5 |  9840 | `{` |
|   4877 |  9841 | `	SyBlobAppend(pOut, iExt >= 0 ? "internal" : "user", iExt >= 0 ? 8 : 4);` |
|      - |  9842 | `	/* php writes the three parts in this order and each on its own condition,` |
|      - |  9843 | ``	 * so a deprecated INTERNAL name reads `<internal, deprecated:curl>` -- the`` |
|      - |  9844 | ``	 * extension hangs off `deprecated`, not off `internal`. */`` |
|   4877 |  9845 | `	if( bDeprecated ){` |
|     93 |  9846 | `		SyBlobAppend(pOut, ", deprecated", sizeof(", deprecated")-1);` |
|     45 |  9847 | `	}` |
|   4877 |  9848 | `	if( iExt >= 0 ){` |
|   4745 |  9849 | `		SyBlobFormat(pOut, ":%s", PH7_VmExtensionName(iExt));` |
|   2370 |  9850 | `	}` |
|   4877 |  9851 | `}` |
|      - |  9852 | `/*` |
|      - |  9853 | ` * A declared type followed by a space, or nothing at all.` |
|      - |  9854 | ` *` |
|      - |  9855 | `` * php's EXPORT spells a standalone `iterable` as the two types it stands for`` |
|      - |  9856 | `` * where getType() prints `iterable` -- the one place the two renderings of a`` |
|      - |  9857 | ` * declared type differ (ReflectExportIterable is that difference, named once).` |
|      - |  9858 | ` */` |
|   7490 |  9859 | `static const SyString * ReflectExportIterable(const SyString *pType, SyString *pOut)` |
|      5 |  9860 | `{` |
|   7495 |  9861 | `	const char *z = pType ? SyStringData(pType) : 0;` |
|   7495 |  9862 | `	sxu32 n = z ? SyStringLength(pType) : 0;` |
|   7495 |  9863 | `	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){` |
|    ! 0 |  9864 | `		SyStringInitFromBuf(pOut,"Traversable\|array",sizeof("Traversable\|array")-1);` |
|    ! 0 |  9865 | `		return pOut;` |
|      - |  9866 | `	}` |
|   7495 |  9867 | `	if( n == 9 && z[0] == '?' && SyStrnicmp(&z[1],"iterable",8) == 0 ){` |
|    ! 0 |  9868 | `		SyStringInitFromBuf(pOut,"Traversable\|array\|null",sizeof("Traversable\|array\|null")-1);` |
|    ! 0 |  9869 | `		return pOut;` |
|      - |  9870 | `	}` |
|   7495 |  9871 | `	return pType;` |
|   3750 |  9872 | `}` |
|   3310 |  9873 | `static void ReflectExportTypeSp(SyBlob *pOut, const SyString *pType)` |
|      5 |  9874 | `{` |
|      - |  9875 | `	SyString sIter;` |
|   3315 |  9876 | `	pType = ReflectExportIterable(pType,&sIter);` |
|   3315 |  9877 | `	if( pType && SyStringLength(pType) > 0 ){` |
|   2979 |  9878 | `		SyBlobAppend(pOut, SyStringData(pType), SyStringLength(pType));` |
|   2979 |  9879 | `		SyBlobAppend(pOut, " ", sizeof(char));` |
|   1487 |  9880 | `	}` |
|   3315 |  9881 | `}` |
|      - |  9882 | `/* php's visibility word for a member. */` |
|   5104 |  9883 | `static const char * ReflectExportVis(sxi32 iProtection)` |
|      3 |  9884 | `{` |
|   5107 |  9885 | `	if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     53 |  9886 | `		return "private";` |
|      - |  9887 | `	}` |
|   5055 |  9888 | `	return iProtection == PH7_CLASS_PROT_PROTECTED ? "protected" : "public";` |
|   2555 |  9889 | `}` |
|      - |  9890 | `/*` |
|      - |  9891 | `` * The text after `= ` in a parameter default.`` |
|      - |  9892 | ` *` |
|      - |  9893 | ` * php prints a constant-reference default as the CONSTANT's name, not its value` |
|      - |  9894 | `` * (`$f = M_PI`) — that argument was never folded, so php still has the source`` |
|      - |  9895 | `` * expression. `<default>` is what the chunk answered when the value could not`` |
|      - |  9896 | `` * be produced at all, which is a declared `= ?` row in aBuiltinSig[].`` |
|      - |  9897 | ` */` |
|   1296 |  9898 | `static void ReflectExportDefault(ph7_context *pCtx, SyBlob *pOut, ReflectParamDesc *pDesc)` |
|      5 |  9899 | `{` |
|   1301 |  9900 | `	const char *z = 0;` |
|   1301 |  9901 | `	int n = 0;` |
|   1301 |  9902 | `	if( ReflectParamDefConst(pCtx, pDesc, &z, &n) ){` |
|    147 |  9903 | `		SyBlobAppend(pOut, z, (sxu32)n);` |
|    724 |  9904 | `		return;` |
|      - |  9905 | `	}` |
|   1159 |  9906 | `	if( pDesc->pArg ){` |
|      - |  9907 | `		ph7_value sValue;` |
|     57 |  9908 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     57 |  9909 | `		VmLocalExec(pCtx->pVm, &pDesc->pArg->aByteCode, &sValue, FALSE);` |
|      - |  9910 | `		/* php has two spellings for the same default and picks by whether the` |
|      - |  9911 | ``		 * function is INTERNAL: `null` and double quotes there, `NULL` and single`` |
|      - |  9912 | `		 * quotes for a userland one. A builtin written as prelude PHP is internal` |
|      - |  9913 | `		 * to php however this engine chose to implement it, so scandir()'s` |
|      - |  9914 | ``		 * `$context = NULL` and clearstatcache()'s `$filename = ''` were both`` |
|      - |  9915 | `		 * printed in the wrong one. */` |
|     56 |  9916 | `		if( pDesc->bInternal` |
|     32 |  9917 | `		 && (sValue.iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING ){` |
|      4 |  9918 | `			ReflectExportStrQ(pOut, (const char *)SyBlobData(&sValue.sBlob),` |
|      1 |  9919 | `				SyBlobLength(&sValue.sBlob), '"');` |
|     56 |  9920 | `		}else if( pDesc->bInternal && (sValue.iFlags & MEMOBJ_NULL) ){` |
|      3 |  9921 | `			SyBlobAppend(pOut, "null", sizeof("null")-1);` |
|      2 |  9922 | `		}else{` |
|     53 |  9923 | `			ReflectExportValue(pCtx, pOut, &sValue, 0);` |
|      - |  9924 | `		}` |
|     57 |  9925 | `		PH7_MemObjRelease(&sValue);` |
|     57 |  9926 | `		return;` |
|      - |  9927 | `	}` |
|      - |  9928 | `	{` |
|      - |  9929 | `		/* A DECLARED default is signature TEXT: reduce it the way` |
|      - |  9930 | `		 * getDefaultValue() does, and fall back to php's own placeholder.` |
|      - |  9931 | `		 * Only an INTERNAL parameter reaches this branch (a compiled one carries` |
|      - |  9932 | `		 * pArg above), which is exactly the case php prints DOUBLE-quoted. */` |
|   1103 |  9933 | `		const char *zDef = SyStringData(&pDesc->sDefText);` |
|   1103 |  9934 | `		int nDef = (int)SyStringLength(&pDesc->sDefText);` |
|   1103 |  9935 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|   1103 |  9936 | `		if( ReflectSigHas(zDef, nDef, "::", 2) ){` |
|      - |  9937 | `			/* Rule 28's split, on the declared side: php's stub keeps the SOURCE of a` |
|      - |  9938 | `			 * constant-expression default, so the export prints` |
|      - |  9939 | ``			 * `= SplFileInfo::class` and `= A::K \| A::J` verbatim while`` |
|      - |  9940 | `			 * getDefaultValue() answers what they evaluate to. */` |
|     57 |  9941 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|     57 |  9942 | `			return;` |
|      - |  9943 | `		}` |
|   1049 |  9944 | `		if( pVal && ReflectSigGlobalConst(pCtx, zDef, nDef, pVal) ){` |
|      - |  9945 | ``			/* Same split for a GLOBAL constant: `= E_ERROR`, never `= 1`. */`` |
|    ! 0 |  9946 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|    ! 0 |  9947 | `			return;` |
|      - |  9948 | `		}` |
|   1044 |  9949 | `		if( pVal && ReflectSigHas(zDef, nDef, "\|", 1)` |
|    534 |  9950 | `		 && ReflectSigConstExpr(pCtx, zDef, nDef, pVal) ){` |
|      - |  9951 | ``			/* And for the `\|` fold of them, which is the one shape whose VALUE`` |
|      - |  9952 | ``			 * names no constant at all: `SQLITE3_OPEN_READWRITE \|`` |
|      - |  9953 | ``			 * SQLITE3_OPEN_CREATE` prints as itself and answers 6. */`` |
|     16 |  9954 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|     16 |  9955 | `			return;` |
|      - |  9956 | `		}` |
|   1035 |  9957 | `		if( ReflectSigIsSourceExpr(zDef, nDef) ){` |
|      - |  9958 | `			/* The two arithmetic shapes: a RADIX integer and a product. php prints` |
|      - |  9959 | ``			 * `0777` and `2 * 1024 * 1024`, never the 511 and 2097152 they reduce`` |
|      - |  9960 | `			 * to -- and answers those numbers from getDefaultValue() all the same. */` |
|      7 |  9961 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|      7 |  9962 | `			return;` |
|      - |  9963 | `		}` |
|   1029 |  9964 | `		if( pVal && ReflectSigScalar(pCtx, zDef, nDef, pVal) ){` |
|    985 |  9965 | `			if( (pVal->iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING ){` |
|    310 |  9966 | `				ReflectExportStrQ(pOut, (const char *)SyBlobData(&pVal->sBlob),` |
|    102 |  9967 | `					SyBlobLength(&pVal->sBlob), '"');` |
|    208 |  9968 | `				return;` |
|      - |  9969 | `			}` |
|    780 |  9970 | `			if( pVal->iFlags & MEMOBJ_NULL ){` |
|      - |  9971 | `				/* The same internal/user split as the quote character above: php` |
|      - |  9972 | ``				 * spells an INTERNAL parameter's null default `null` and a`` |
|      - |  9973 | ``				 * userland one `NULL` (`= null` for every `?T $x = null` stub row). */`` |
|    364 |  9974 | `				SyBlobAppend(pOut, "null", sizeof("null")-1);` |
|    364 |  9975 | `				return;` |
|      - |  9976 | `			}` |
|    419 |  9977 | `			ReflectExportValue(pCtx, pOut, pVal, 0);` |
|    419 |  9978 | `			return;` |
|      - |  9979 | `		}` |
|     44 |  9980 | `		if( (nDef >= 1 && zDef[0] == '[')` |
|     25 |  9981 | `		 \|\| (nDef >= (int)sizeof("array (")-1` |
|    ! 0 |  9982 | `		  && SyMemcmp(zDef, "array (", sizeof("array (")-1) == 0) ){` |
|     47 |  9983 | `			SyBlobAppend(pOut, "[]", sizeof("[]")-1);` |
|     47 |  9984 | `			return;` |
|      - |  9985 | `		}` |
|      - |  9986 | `	}` |
|    ! 0 |  9987 | `	SyBlobAppend(pOut, "<default>", sizeof("<default>")-1);` |
|    653 |  9988 | `}` |
|      - |  9989 | ``/* `Parameter #0 [ <optional> int &...$name = 5 ]` */`` |
|   3030 |  9990 | `static void ReflectExportParamLine(ph7_context *pCtx, SyBlob *pOut, ReflectParamDesc *pDesc)` |
|      5 |  9991 | `{` |
|   4550 |  9992 | `	SyBlobFormat(pOut, "Parameter #%d [ <%s> ", pDesc->iPos,` |
|   3030 |  9993 | `		pDesc->bOptional ? "optional" : "required");` |
|   3035 |  9994 | `	ReflectExportTypeSp(pOut, &pDesc->sType);` |
|   3035 |  9995 | `	if( pDesc->bByRef ){` |
|     79 |  9996 | `		SyBlobAppend(pOut, "&", sizeof(char));` |
|     38 |  9997 | `	}` |
|   3035 |  9998 | `	if( pDesc->bVariadic ){` |
|     35 |  9999 | `		SyBlobAppend(pOut, "...", sizeof("...")-1);` |
|     17 | 10000 | `	}` |
|   3035 | 10001 | `	SyBlobAppend(pOut, "$", sizeof(char));` |
|   3035 | 10002 | `	SyBlobAppend(pOut, SyStringData(&pDesc->sName), SyStringLength(&pDesc->sName));` |
|   3035 | 10003 | `	if( pDesc->bHasDef ){` |
|   1301 | 10004 | `		SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|   1301 | 10005 | `		ReflectExportDefault(pCtx, pOut, pDesc);` |
|   2387 | 10006 | `	}else if( pDesc->bOptional && !pDesc->bVariadic ){` |
|      - | 10007 | `		/* An OPTIONAL parameter with no default a caller could read still gets` |
|      - | 10008 | ``		 * an `= ` from php -- and php's own placeholder after it, since there is`` |
|      - | 10009 | ``		 * nothing to print. `mt_rand()`'s two bounds and `get_class()`'s`` |
|      - | 10010 | `		 * $object are that shape; a VARIADIC tail is not, because its "default"` |
|      - | 10011 | `		 * is having no further arguments. */` |
|     26 | 10012 | `		SyBlobAppend(pOut, " = <default>", sizeof(" = <default>")-1);` |
|     12 | 10013 | `	}` |
|   3035 | 10014 | `	SyBlobAppend(pOut, " ]", sizeof(" ]")-1);` |
|   3035 | 10015 | `}` |
|      - | 10016 | `/*` |
|      - | 10017 | `` * `Property [ public protected(set) readonly int $x = 5 ]` + newline.`` |
|      - | 10018 | ` *` |
|      - | 10019 | ` * The set-visibility is printed only when it DIFFERS from the get-visibility,` |
|      - | 10020 | `` * which is why a `public readonly` property shows php's implied`` |
|      - | 10021 | `` * `protected(set)` and a `protected readonly` one shows nothing.`` |
|      - | 10022 | ` */` |
|    406 | 10023 | `static void ReflectExportPropLine(ph7_context *pCtx, SyBlob *pOut, ph7_class_attr *pAttr,` |
|      - | 10024 | `	const SyString *pKey)` |
|      2 | 10025 | `{` |
|    408 | 10026 | `	sxi32 iSet = pAttr->iProtection;` |
|    408 | 10027 | `	SyBlobAppend(pOut, "Property [ ", sizeof("Property [ ")-1);` |
|      - | 10028 | `	/* php's modifier MASK implies two bits the declaration never wrote:` |
|      - | 10029 | `	 * private(set) implies final, and readonly implies protected(set). A property` |
|      - | 10030 | `	 * DECLARED final (PHP 8.4) prints the same word, and the two spellings do not` |
|      - | 10031 | `	 * print it twice. */` |
|    408 | 10032 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_FINAL\|PH7_CLASS_ATTR_PRIVATE_SET) ){` |
|      7 | 10033 | `		SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|      3 | 10034 | `	}` |
|    408 | 10035 | `	SyBlobFormat(pOut, "%s ", ReflectExportVis(pAttr->iProtection));` |
|    408 | 10036 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|      5 | 10037 | `		iSet = PH7_CLASS_PROT_PRIVATE;` |
|    405 | 10038 | `	}else if( (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET)` |
|    399 | 10039 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){` |
|     29 | 10040 | `		iSet = PH7_CLASS_PROT_PROTECTED;` |
|     14 | 10041 | `	}` |
|      - | 10042 | `	/* The set-visibility is printed only when it DIFFERS from the get one. */` |
|    408 | 10043 | `	if( iSet != pAttr->iProtection ){` |
|     29 | 10044 | `		SyBlobFormat(pOut, "%s(set) ", ReflectExportVis(iSet));` |
|     14 | 10045 | `	}` |
|    408 | 10046 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|      7 | 10047 | `		SyBlobAppend(pOut, "static ", sizeof("static ")-1);` |
|      3 | 10048 | `	}` |
|    408 | 10049 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|    ! 0 | 10050 | `		SyBlobAppend(pOut, "virtual ", sizeof("virtual ")-1);` |
|    ! 0 | 10051 | `	}` |
|    408 | 10052 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|     29 | 10053 | `		SyBlobAppend(pOut, "readonly ", sizeof("readonly ")-1);` |
|     14 | 10054 | `	}` |
|    408 | 10055 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|      - | 10056 | `		char zType[192];` |
|      - | 10057 | `		SyString sType;` |
|    282 | 10058 | `		const char *zT = ReflectMemberTypeText(pCtx->pVm,pAttr,zType,sizeof(zType));` |
|    282 | 10059 | `		SyStringInitFromBuf(&sType,zT,(sxu32)SyStrlen(zT));` |
|    282 | 10060 | `		ReflectExportTypeSp(pOut, &sType);` |
|    140 | 10061 | `	}` |
|    408 | 10062 | `	SyBlobAppend(pOut, "$", sizeof(char));` |
|    408 | 10063 | `	SyBlobAppend(pOut, SyStringData(pKey), SyStringLength(pKey));` |
|    406 | 10064 | `	if( SySetUsed(&pAttr->aByteCode) > 0 \|\| pAttr->pNativeValue` |
|    261 | 10065 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      - | 10066 | `		ph7_value sValue;` |
|    265 | 10067 | `		SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|    265 | 10068 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|    265 | 10069 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     33 | 10070 | `			VmLocalExec(pCtx->pVm, &pAttr->aByteCode, &sValue, FALSE);` |
|    249 | 10071 | `		}else if( pAttr->pNativeValue ){` |
|    231 | 10072 | `			PH7_NativeLiteralValue(pCtx->pVm, pAttr->pNativeValue, &sValue);` |
|    115 | 10073 | `		}` |
|    265 | 10074 | `		ReflectExportValue(pCtx, pOut, &sValue, 0);` |
|    265 | 10075 | `		PH7_MemObjRelease(&sValue);` |
|    132 | 10076 | `	}` |
|      - | 10077 | `	/* A hooked property names its hooks; php lists no method for them. */` |
|    408 | 10078 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET) ){` |
|      3 | 10079 | `		SyBlobAppend(pOut, " {", sizeof(" {")-1);` |
|      3 | 10080 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET ){` |
|      3 | 10081 | `			SyBlobAppend(pOut, " get;", sizeof(" get;")-1);` |
|      1 | 10082 | `		}` |
|      3 | 10083 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET ){` |
|      3 | 10084 | `			SyBlobAppend(pOut, " set;", sizeof(" set;")-1);` |
|      1 | 10085 | `		}` |
|      3 | 10086 | `		SyBlobAppend(pOut, " }", sizeof(" }")-1);` |
|      1 | 10087 | `	}` |
|    408 | 10088 | `	SyBlobAppend(pOut, " ]\n", sizeof(" ]\n")-1);` |
|    408 | 10089 | `}` |
|      - | 10090 | `/*` |
|      - | 10091 | `` * `Constant [ final public int NAME ] { value }` + newline. The TYPE is the`` |
|      - | 10092 | ` * value's, not a declared one, and an object (an enum case) prints as the word` |
|      - | 10093 | ` * Object under its own class name.` |
|      - | 10094 | ` */` |
|    638 | 10095 | `static sxi32 ReflectExportConstLine(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass,` |
|      - | 10096 | `	ph7_class_attr *pAttr, const SyString *pKey)` |
|      2 | 10097 | `{` |
|    640 | 10098 | `	ph7_value *pVal = 0;` |
|    640 | 10099 | `	sxi32 rc = ReflectConstSlot(pCtx, pClass, pAttr, &pVal);` |
|      - | 10100 | `	const char *zType;` |
|    640 | 10101 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10102 | `		return rc;` |
|      - | 10103 | `	}` |
|    640 | 10104 | `	SyBlobAppend(pOut, "Constant [ ", sizeof("Constant [ ")-1);` |
|    640 | 10105 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      7 | 10106 | `		SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|      3 | 10107 | `	}` |
|    640 | 10108 | `	SyBlobFormat(pOut, "%s ", ReflectExportVis(pAttr->iProtection));` |
|    638 | 10109 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|    321 | 10110 | `	 && SyStringLength(&pAttr->sTypeName) > 0 ){` |
|      - | 10111 | `		/* php 8.3's typed class constant prints what it DECLARED; the word below` |
|      - | 10112 | `		 * is what an untyped one's VALUE happens to be. */` |
|      - | 10113 | `		char zDecl[192];` |
|      - | 10114 | `		SyString sDecl;` |
|    ! 0 | 10115 | `		const char *zT = ReflectMemberTypeText(pCtx->pVm,pAttr,zDecl,sizeof(zDecl));` |
|    ! 0 | 10116 | `		SyStringInitFromBuf(&sDecl,zT,(sxu32)SyStrlen(zT));` |
|    ! 0 | 10117 | `		ReflectExportTypeSp(pOut, &sDecl);` |
|    ! 0 | 10118 | `	}else{` |
|    640 | 10119 | `		zType = ReflectExportTypeWord(pVal);` |
|    640 | 10120 | `		if( zType ){` |
|    638 | 10121 | `			SyBlobFormat(pOut, "%s ", zType);` |
|    320 | 10122 | `		}else{` |
|      3 | 10123 | `			SyBlobFormat(pOut, "%z ", &((ph7_class_instance *)pVal->x.pOther)->pClass->sName);` |
|      - | 10124 | `		}` |
|      - | 10125 | `	}` |
|    640 | 10126 | `	SyBlobAppend(pOut, SyStringData(pKey), SyStringLength(pKey));` |
|    640 | 10127 | `	SyBlobAppend(pOut, " ] { ", sizeof(" ] { ")-1);` |
|    640 | 10128 | `	ReflectExportConstValue(pCtx, pOut, pVal);` |
|    640 | 10129 | `	SyBlobAppend(pOut, " }\n", sizeof(" }\n")-1);` |
|    640 | 10130 | `	return SXRET_OK;` |
|    321 | 10131 | `}` |
|      - | 10132 | `/* A native class METHOD as a function reference (ReflectFuncFill's tail, for a` |
|      - | 10133 | ` * method the member walk handed over rather than one a receiver names). */` |
|   3788 | 10134 | `static void ReflectFuncFromMethod(ph7_vm *pVm, ph7_class *pClass, ph7_class_method *pMeth,` |
|      - | 10135 | `	ReflectFuncRef *pOut)` |
|      2 | 10136 | `{` |
|   3790 | 10137 | `	SyZero(pOut, sizeof(*pOut));` |
|   3790 | 10138 | `	pOut->pVm = pVm;` |
|   3790 | 10139 | `	pOut->pClass = pClass;` |
|   3790 | 10140 | `	pOut->pMeth = pMeth;` |
|   3790 | 10141 | `	pOut->pFunc = &pMeth->sFunc;` |
|   3790 | 10142 | `	if( (pOut->pFunc->iFlags & VM_FUNC_NATIVE) && pOut->pFunc->pNative ){` |
|   3752 | 10143 | `		pOut->zSig = pOut->pFunc->pNative->zSig;` |
|   3752 | 10144 | `		if( SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|   3752 | 10145 | `			pOut->zRet = pOut->pFunc->pNative->zRet;` |
|   1875 | 10146 | `		}` |
|   1875 | 10147 | `	}` |
|   3790 | 10148 | `}` |
|      - | 10149 | `/*` |
|      - | 10150 | ` * The Method / Function / Closure block.` |
|      - | 10151 | ` *` |
|      - | 10152 | ` * pOwner is the class being EXPORTED when there is one: php's tags are` |
|      - | 10153 | ` * relative to it — a method it did not declare "inherits" from its declaring` |
|      - | 10154 | ` * class — and the reflector alone cannot say that, because $class is the` |
|      - | 10155 | `` * DECLARING class. `overwrites` is the other direction and needs no owner: the`` |
|      - | 10156 | ` * declaring class's own parent declares the same method.` |
|      - | 10157 | ` */` |
|   4566 | 10158 | `static sxi32 ReflectExportFuncBlock(ph7_context *pCtx, SyBlob *pOut, ReflectFuncRef *pRef,` |
|      - | 10159 | `	const char *zIndent, ph7_class *pOwner)` |
|      5 | 10160 | `{` |
|      - | 10161 | `	SyBlob sBody;` |
|   4571 | 10162 | `	int bInternal = ReflectFuncIsInternal(pRef);` |
|   6809 | 10163 | `	int bDeprecated = ReflectFuncDeprecated(pRef) != 0` |
|   4566 | 10164 | `		\|\| (pRef->pFunc != 0 && ReflectHasDeprecated(&pRef->pFunc->aAttrs));` |
|   4571 | 10165 | `	int nParam = ReflectParamCount(pRef);` |
|   4571 | 10166 | `	const char *zRet = 0;` |
|      - | 10167 | `	char zRetBuf[192];` |
|   4571 | 10168 | `	int nRet = 0, bHasRet, bTentRet = 0;` |
|   4571 | 10169 | `	sxi32 rc = SXRET_OK;` |
|   4571 | 10170 | `	SyBlobInit(&sBody, &pCtx->pVm->sAllocator);` |
|   4571 | 10171 | `	bHasRet = ReflectFuncRetTextEx(pRef, &zRet, &nRet, &bTentRet, zRetBuf, sizeof(zRetBuf));` |
|   4571 | 10172 | `	if( pRef->pMeth ){` |
|   4035 | 10173 | `		ph7_class *pDecl = ReflectFuncDeclClass(pRef);` |
|      - | 10174 | `		ph7_class *pProto;` |
|   4035 | 10175 | `		SyString *pName = &pRef->pMeth->sFunc.sName;` |
|      - | 10176 | `		int bInherits, bCtor;` |
|   4035 | 10177 | `		SyBlobAppend(&sBody, "Method [ <", sizeof("Method [ <")-1);` |
|      - | 10178 | `		/* A method names its DECLARING class's extension, which is why php's` |
|      - | 10179 | ``		 * `JsonException::__construct` prints `<internal:Core, inherits`` |
|      - | 10180 | ``		 * Exception, ctor>` rather than json's own name. */`` |
|   4035 | 10181 | `		ReflectExportKind(&sBody, ReflectFuncExtId(pRef), bDeprecated);` |
|   4035 | 10182 | `		bInherits = (pOwner != 0 && pDecl != 0 && pDecl != pOwner);` |
|   4035 | 10183 | `		if( bInherits ){` |
|   1935 | 10184 | `			SyBlobFormat(&sBody, ", inherits %z", &pDecl->sName);` |
|   3068 | 10185 | `		}else if( pDecl ){` |
|   3150 | 10186 | `			ph7_class *pOver = ReflectOverwritesIn(pDecl,` |
|   2098 | 10187 | `				SyStringData(pName), (int)SyStringLength(pName));` |
|   2101 | 10188 | `			if( pOver ){` |
|    165 | 10189 | `				SyBlobFormat(&sBody, ", overwrites %z", &pOver->sName);` |
|     82 | 10190 | `			}` |
|   1049 | 10191 | `		}` |
|   6405 | 10192 | `		bCtor = SyStringLength(pName) == sizeof("__construct")-1` |
|   4032 | 10193 | `			&& SyStrnicmp(SyStringData(pName), "__construct", sizeof("__construct")-1) == 0;` |
|      - | 10194 | `		/* The prototype belongs to the entry in the class the export was` |
|      - | 10195 | `		 * reached THROUGH, which is php's anchor for it: a class that only` |
|      - | 10196 | `		 * inherits a method still re-assigns its prototype when it implements` |
|      - | 10197 | `		 * an interface declaring the same one. */` |
|   2019 | 10198 | `		pProto = ReflectPrototypeOf(pCtx, pOwner ? pOwner : pRef->pClass,` |
|   4032 | 10199 | `			SyStringData(pName), (int)SyStringLength(pName), 0);` |
|   4035 | 10200 | `		if( pProto ){` |
|   1340 | 10201 | `			SyBlobFormat(&sBody, ", prototype %z", &pProto->sName);` |
|    669 | 10202 | `		}` |
|      - | 10203 | `		/* php closes the bracket with the ctor word, AFTER the prototype: an` |
|      - | 10204 | `` 		 * interface's constructor prints `prototype F1, ctor`. There is no `dtor` `` |
|      - | 10205 | `		 * twin -- php's exporter prints the word for ZEND_ACC_CTOR only, so a` |
|      - | 10206 | ``		 * `__destruct` reads `Method [ <user> public method __destruct ]`. */`` |
|   4035 | 10207 | `		if( bCtor ){` |
|    209 | 10208 | `			SyBlobAppend(&sBody, ", ctor", sizeof(", ctor")-1);` |
|    103 | 10209 | `		}` |
|   4035 | 10210 | `		SyBlobAppend(&sBody, "> ", sizeof("> ")-1);` |
|   4035 | 10211 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|    205 | 10212 | `			SyBlobAppend(&sBody, "abstract ", sizeof("abstract ")-1);` |
|    102 | 10213 | `		}` |
|   4035 | 10214 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|    445 | 10215 | `			SyBlobAppend(&sBody, "final ", sizeof("final ")-1);` |
|    222 | 10216 | `		}` |
|   4035 | 10217 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|     72 | 10218 | `			SyBlobAppend(&sBody, "static ", sizeof("static ")-1);` |
|     35 | 10219 | `		}` |
|   4035 | 10220 | `		SyBlobFormat(&sBody, "%s method %z ]", ReflectExportVis(pRef->pMeth->iProtection), pName);` |
|   2019 | 10221 | `	}else{` |
|    806 | 10222 | `		SyBlobAppend(&sBody, pRef->pClosure ? "Closure [ <" : "Function [ <",` |
|    534 | 10223 | `			pRef->pClosure ? sizeof("Closure [ <")-1 : sizeof("Function [ <")-1);` |
|    539 | 10224 | `		ReflectExportKind(&sBody, ReflectFuncExtId(pRef), bDeprecated);` |
|    539 | 10225 | `		SyBlobAppend(&sBody, "> function ", sizeof("> function ")-1);` |
|    539 | 10226 | `		if( pRef->pFunc ){` |
|     11 | 10227 | `			SyBlobFormat(&sBody, "%z", &pRef->pFunc->sName);` |
|    534 | 10228 | `		}else if( pRef->pHost ){` |
|    529 | 10229 | `			SyBlobFormat(&sBody, "%z", &pRef->pHost->sName);` |
|    262 | 10230 | `		}` |
|    539 | 10231 | `		SyBlobAppend(&sBody, " ]", sizeof(" ]")-1);` |
|      - | 10232 | `	}` |
|   4571 | 10233 | `	SyBlobAppend(&sBody, " {\n", sizeof(" {\n")-1);` |
|   4571 | 10234 | `	if( !bInternal && pRef->pFunc ){` |
|    103 | 10235 | `		SyBlobAppend(&sBody, "  @@ ", sizeof("  @@ ")-1);` |
|    103 | 10236 | `		if( SyStringLength(&pRef->pFunc->sFile) > 0 ){` |
|    154 | 10237 | `			SyBlobAppend(&sBody, SyStringData(&pRef->pFunc->sFile),` |
|    102 | 10238 | `				SyStringLength(&pRef->pFunc->sFile));` |
|     51 | 10239 | `		}` |
|    103 | 10240 | `		SyBlobFormat(&sBody, " %u - %u\n", pRef->pFunc->nLine, pRef->pFunc->nEndLine);` |
|     51 | 10241 | `	}` |
|      - | 10242 | `	/* php prints the parameter block for every INTERNAL function, and for a` |
|      - | 10243 | `	 * user one only when there is something to say. */` |
|   4571 | 10244 | `	if( nParam > 0 \|\| bHasRet \|\| bInternal ){` |
|      - | 10245 | `		int n;` |
|   4515 | 10246 | `		SyBlobFormat(&sBody, "\n  - Parameters [%d] {\n", nParam);` |
|   7407 | 10247 | `		for( n = 0 ; n < nParam ; n++ ){` |
|      - | 10248 | `			ReflectParamDesc sDesc;` |
|   2897 | 10249 | `			if( !ReflectParamAt(pRef, n, &sDesc) ){` |
|    ! 0 | 10250 | `				continue;` |
|      - | 10251 | `			}` |
|   2897 | 10252 | `			SyBlobAppend(&sBody, "    ", sizeof("    ")-1);` |
|   2897 | 10253 | `			ReflectExportParamLine(pCtx, &sBody, &sDesc);` |
|   2897 | 10254 | `			SyBlobAppend(&sBody, "\n", sizeof(char));` |
|   1451 | 10255 | `		}` |
|   4515 | 10256 | `		SyBlobAppend(&sBody, "  }\n", sizeof("  }\n")-1);` |
|   2255 | 10257 | `	}` |
|   4571 | 10258 | `	if( bHasRet ){` |
|      - | 10259 | ``		/* php's two spellings: `Return [ T ]` for a declared type, `Tentative return`` |
|      - | 10260 | ``		 * [ T ]` for a stub's @tentative-return-type. */`` |
|   4185 | 10261 | `		if( bTentRet ){` |
|   2649 | 10262 | `			SyBlobAppend(&sBody, "  - Tentative return [ ", sizeof("  - Tentative return [ ")-1);` |
|   1326 | 10263 | `		}else{` |
|   1539 | 10264 | `			SyBlobAppend(&sBody, "  - Return [ ", sizeof("  - Return [ ")-1);` |
|      - | 10265 | `		}` |
|      - | 10266 | `		{` |
|      - | 10267 | ``			/* ...and the export's own spelling of a standalone `iterable`. */`` |
|      - | 10268 | `			SyString sRet, sIter;` |
|      - | 10269 | `			const SyString *pRet;` |
|   4185 | 10270 | `			SyStringInitFromBuf(&sRet,zRet,(sxu32)nRet);` |
|   4185 | 10271 | `			pRet = ReflectExportIterable(&sRet,&sIter);` |
|   4185 | 10272 | `			SyBlobAppend(&sBody, SyStringData(pRet), SyStringLength(pRet));` |
|      - | 10273 | `		}` |
|   4185 | 10274 | `		SyBlobAppend(&sBody, " ]\n", sizeof(" ]\n")-1);` |
|   2090 | 10275 | `	}` |
|   4571 | 10276 | `	SyBlobAppend(&sBody, "}\n", sizeof("}\n")-1);` |
|      - | 10277 | `	/* Indent every non-empty line, the way the chunk's explode/implode did. */` |
|   4571 | 10278 | `	if( zIndent == 0 \|\| zIndent[0] == '\0' ){` |
|    783 | 10279 | `		SyBlobAppend(pOut, SyBlobData(&sBody), SyBlobLength(&sBody));` |
|    394 | 10280 | `	}else{` |
|   3790 | 10281 | `		const char *z = (const char *)SyBlobData(&sBody);` |
|   3790 | 10282 | `		sxu32 n = SyBlobLength(&sBody), i = 0, iStart = 0;` |
|   3790 | 10283 | `		sxu32 nIndent = (sxu32)SyStrlen(zIndent);` |
| 592237 | 10284 | `		for( i = 0 ; i < n ; i++ ){` |
| 588449 | 10285 | `			if( z[i] != '\n' ){` |
| 564277 | 10286 | `				continue;` |
|      - | 10287 | `			}` |
|  24174 | 10288 | `			if( i > iStart ){` |
|  20392 | 10289 | `				SyBlobAppend(pOut, zIndent, nIndent);` |
|  20392 | 10290 | `				SyBlobAppend(pOut, &z[iStart], i - iStart);` |
|  10195 | 10291 | `			}` |
|  24174 | 10292 | `			SyBlobAppend(pOut, "\n", sizeof(char));` |
|  24174 | 10293 | `			iStart = i + 1;` |
|  12088 | 10294 | `		}` |
|      - | 10295 | `	}` |
|   4571 | 10296 | `	SyBlobRelease(&sBody);` |
|   4571 | 10297 | `	return rc;` |
|      5 | 10298 | `}` |
|      - | 10299 | ``/* The `- Enum cases [N]` block php prints where an enum's cases would otherwise`` |
|      - | 10300 | ` * be listed among its constants. A backed case shows its value UNQUOTED. */` |
|      6 | 10301 | `static sxi32 ReflectExportEnumCases(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass)` |
|      1 | 10302 | `{` |
|      7 | 10303 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      - | 10304 | `	sxu32 n;` |
|      7 | 10305 | `	SyBlobFormat(pOut, "\n  - Enum cases [%u] {\n", SySetUsed(&pClass->aEnumCases));` |
|     17 | 10306 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     11 | 10307 | `		SyBlobAppend(pOut, "    Case ", sizeof("    Case ")-1);` |
|     11 | 10308 | `		SyBlobAppend(pOut, SyStringData(&apCase[n]->sName), SyStringLength(&apCase[n]->sName));` |
|     11 | 10309 | `		if( pClass->nEnumBacking ){` |
|      9 | 10310 | `			ph7_value *pVal = 0;` |
|      - | 10311 | `			ph7_class_instance *pObj;` |
|      9 | 10312 | `			sxi32 rc = ReflectConstSlot(pCtx, pClass, apCase[n], &pVal);` |
|      9 | 10313 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 10314 | `				return rc;` |
|      - | 10315 | `			}` |
|      9 | 10316 | `			pObj = (pVal && (pVal->iFlags & MEMOBJ_OBJ)) ? (ph7_class_instance *)pVal->x.pOther : 0;` |
|      9 | 10317 | `			if( pObj ){` |
|      9 | 10318 | `				ph7_value *pBacking = PH7_NativeAttr(pObj, "value");` |
|      9 | 10319 | `				if( pBacking ){` |
|      - | 10320 | `					ph7_value sTmp;` |
|      - | 10321 | `					const char *zText;` |
|      - | 10322 | `					int nText;` |
|      9 | 10323 | `					PH7_MemObjInit(pCtx->pVm, &sTmp);` |
|      9 | 10324 | `					PH7_MemObjStore(pBacking, &sTmp);` |
|      9 | 10325 | `					zText = ph7_value_to_string(&sTmp, &nText);` |
|      9 | 10326 | `					SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|      9 | 10327 | `					if( nText > 0 ){` |
|      9 | 10328 | `						SyBlobAppend(pOut, zText, (sxu32)nText);` |
|      4 | 10329 | `					}` |
|      9 | 10330 | `					PH7_MemObjRelease(&sTmp);` |
|      4 | 10331 | `				}` |
|      4 | 10332 | `			}` |
|      4 | 10333 | `		}` |
|     11 | 10334 | `		SyBlobAppend(pOut, "\n", sizeof(char));` |
|      6 | 10335 | `	}` |
|      7 | 10336 | `	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|      7 | 10337 | `	return SXRET_OK;` |
|      4 | 10338 | `}` |
|      - | 10339 | `/* The Class / Interface / Enum block. */` |
|    306 | 10340 | `static sxi32 ReflectExportClassBlock(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass)` |
|      2 | 10341 | `{` |
|    308 | 10342 | `	ph7_vm *pVm = pCtx->pVm;` |
|    308 | 10343 | `	int bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;` |
|    308 | 10344 | `	int bEnum = (pClass->iFlags & PH7_CLASS_ENUM) != 0;` |
|    308 | 10345 | `	int bIface = (pClass->iFlags & PH7_CLASS_INTERFACE) != 0;` |
|      - | 10346 | `	SySet aMembers, aIface;` |
|    308 | 10347 | `	sxu32 n, nConst = 0, nStaticProp = 0, nProp = 0, nStaticMeth = 0, nMeth = 0;` |
|    308 | 10348 | `	sxi32 rc = SXRET_OK;` |
|      - | 10349 | `	int iPass;` |
|    308 | 10350 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|    308 | 10351 | `	SySetInit(&aIface, &pVm->sAllocator, sizeof(ph7_class *));` |
|    308 | 10352 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|    308 | 10353 | `	PH7_ReflectInterfacesOf(pVm, pClass, &aIface);` |
|      - | 10354 | `	/* ---- head ---- */` |
|    308 | 10355 | `	if( bIface ){` |
|     57 | 10356 | `		SyBlobAppend(pOut, "Interface [ <", sizeof("Interface [ <")-1);` |
|    280 | 10357 | `	}else if( bEnum ){` |
|      7 | 10358 | `		SyBlobAppend(pOut, "Enum [ <", sizeof("Enum [ <")-1);` |
|      4 | 10359 | `	}else{` |
|    246 | 10360 | `		SyBlobAppend(pOut, "Class [ <", sizeof("Class [ <")-1);` |
|      - | 10361 | `	}` |
|    308 | 10362 | `	ReflectExportKind(pOut, ReflectClassExtId(pClass), 0);` |
|    308 | 10363 | `	SyBlobAppend(pOut, "> ", sizeof("> ")-1);` |
|      - | 10364 | `	/* php tags a class that has an iteration handler, which its Traversable` |
|      - | 10365 | `	 * implementers have — and so does anything with a HOOKED property, because` |
|      - | 10366 | `	 * that is how php 8.4 walks one. An INTERFACE never carries one: php` |
|      - | 10367 | `	 * installs the handler when a CLASS implements Traversable, so` |
|      - | 10368 | ``	 * `interface I extends Iterator` prints no tag. */`` |
|      - | 10369 | `	{` |
|    405 | 10370 | `		int bIterable = !bIface && pVm->pTraversableClass != 0` |
|    431 | 10371 | `			&& PH7_VmInstanceOf(pClass, pVm->pTraversableClass);` |
|   3226 | 10372 | `		for( n = 0 ; !bIterable && n < SySetUsed(&aMembers) ; n++ ){` |
|   2920 | 10373 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   2918 | 10374 | `			if( pM->iKind == REFLECT_MEMBER_PROP && pM->pAttr` |
|    396 | 10375 | `			 && (pM->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) ){` |
|      3 | 10376 | `				bIterable = 1;` |
|      1 | 10377 | `			}` |
|   1461 | 10378 | `		}` |
|    308 | 10379 | `		if( bIterable ){` |
|     87 | 10380 | `			SyBlobAppend(pOut, "<iterateable> ", sizeof("<iterateable> ")-1);` |
|     43 | 10381 | `		}` |
|      - | 10382 | `	}` |
|    308 | 10383 | `	if( bIface ){` |
|     57 | 10384 | `		SyBlobFormat(pOut, "interface %z", &pClass->sName);` |
|    280 | 10385 | `	}else if( bEnum ){` |
|      7 | 10386 | `		SyBlobFormat(pOut, "enum %z", &pClass->sName);` |
|      7 | 10387 | `		if( pClass->nEnumBacking ){` |
|      5 | 10388 | `			SyBlobFormat(pOut, ": %s", (pClass->nEnumBacking & MEMOBJ_INT) ? "int" : "string");` |
|      2 | 10389 | `		}` |
|      4 | 10390 | `	}else{` |
|    246 | 10391 | `		if( pClass->iFlags & PH7_CLASS_ABSTRACT ){` |
|     13 | 10392 | `			SyBlobAppend(pOut, "abstract ", sizeof("abstract ")-1);` |
|      6 | 10393 | `		}` |
|    246 | 10394 | `		if( pClass->iFlags & PH7_CLASS_FINAL ){` |
|     45 | 10395 | `			SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|     22 | 10396 | `		}` |
|    246 | 10397 | `		if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      5 | 10398 | `			SyBlobAppend(pOut, "readonly ", sizeof("readonly ")-1);` |
|      2 | 10399 | `		}` |
|    246 | 10400 | `		SyBlobFormat(pOut, "class %z", &pClass->sName);` |
|    246 | 10401 | `		if( pClass->pBase ){` |
|    129 | 10402 | `			SyBlobFormat(pOut, " extends %z", &pClass->pBase->sName);` |
|     64 | 10403 | `		}` |
|      - | 10404 | `	}` |
|    308 | 10405 | `	if( SySetUsed(&aIface) > 0 ){` |
|    228 | 10406 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIface);` |
|      - | 10407 | `		/* An interface EXTENDS what a class implements. */` |
|    341 | 10408 | `		SyBlobAppend(pOut, bIface ? " extends " : " implements ",` |
|    113 | 10409 | `			bIface ? sizeof(" extends ")-1 : sizeof(" implements ")-1);` |
|    810 | 10410 | `		for( n = 0 ; n < SySetUsed(&aIface) ; n++ ){` |
|    584 | 10411 | `			if( n > 0 ){` |
|    357 | 10412 | `				SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|    178 | 10413 | `			}` |
|    584 | 10414 | `			SyBlobFormat(pOut, "%z", &apIface[n]->sName);` |
|    293 | 10415 | `		}` |
|    113 | 10416 | `	}` |
|    308 | 10417 | `	SyBlobAppend(pOut, " ] {\n", sizeof(" ] {\n")-1);` |
|    308 | 10418 | `	if( !bInternal && SyStringLength(&pClass->sFile) > 0 ){` |
|     23 | 10419 | `		SyBlobAppend(pOut, "  @@ ", sizeof("  @@ ")-1);` |
|     23 | 10420 | `		SyBlobAppend(pOut, SyStringData(&pClass->sFile), SyStringLength(&pClass->sFile));` |
|     23 | 10421 | `		SyBlobFormat(pOut, " %u-%u\n", pClass->nLine, pClass->nEndLine);` |
|     11 | 10422 | `	}` |
|      - | 10423 | `	/* ---- counts ---- */` |
|   5136 | 10424 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   4830 | 10425 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   4830 | 10426 | `		if( pM->iKind == REFLECT_MEMBER_CONST ){` |
|    644 | 10427 | `			if( !(bEnum && (pM->pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)) ){` |
|    634 | 10428 | `				nConst++;` |
|    318 | 10429 | `			}` |
|   4509 | 10430 | `		}else if( pM->iKind == REFLECT_MEMBER_PROP ){` |
|    400 | 10431 | `			if( pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){ nStaticProp++; }else{ nProp++; }` |
|    201 | 10432 | `		}else{` |
|   3790 | 10433 | `			if( pM->pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){ nStaticMeth++; }else{ nMeth++; }` |
|      - | 10434 | `		}` |
|   2416 | 10435 | `	}` |
|      - | 10436 | `	/* ---- enum cases, then constants ---- */` |
|    308 | 10437 | `	if( bEnum ){` |
|      7 | 10438 | `		rc = ReflectExportEnumCases(pCtx, pOut, pClass);` |
|      7 | 10439 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 10440 | `			goto done;` |
|      - | 10441 | `		}` |
|      3 | 10442 | `	}` |
|    308 | 10443 | `	SyBlobFormat(pOut, "\n  - Constants [%u] {\n", nConst);` |
|   5136 | 10444 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   4830 | 10445 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   4828 | 10446 | `		if( pM->iKind != REFLECT_MEMBER_CONST` |
|   2737 | 10447 | `		 \|\| (bEnum && (pM->pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)) ){` |
|   4198 | 10448 | `			continue;` |
|      - | 10449 | `		}` |
|    634 | 10450 | `		SyBlobAppend(pOut, "    ", sizeof("    ")-1);` |
|    634 | 10451 | `		rc = ReflectExportConstLine(pCtx, pOut, pClass, pM->pAttr, &pM->sKey);` |
|    634 | 10452 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 10453 | `			goto done;` |
|      - | 10454 | `		}` |
|    318 | 10455 | `	}` |
|    308 | 10456 | `	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|      - | 10457 | `	/* ---- properties and methods, statics first ---- */` |
|   1532 | 10458 | `	for( iPass = 0 ; iPass < 4 ; iPass++ ){` |
|   1226 | 10459 | `		int bStatic = (iPass == 0 \|\| iPass == 1);` |
|   1226 | 10460 | `		int bMethods = (iPass == 1 \|\| iPass == 3);` |
|   1226 | 10461 | `		int bFirst = 1;` |
|      - | 10462 | `		static const char *azTitle[] = {` |
|      - | 10463 | `			"\n  - Static properties [%u] {\n", "\n  - Static methods [%u] {\n",` |
|      - | 10464 | `			"\n  - Properties [%u] {\n", "\n  - Methods [%u] {\n"` |
|      - | 10465 | `		};` |
|      - | 10466 | `		sxu32 aCount[4];` |
|   1226 | 10467 | `		aCount[0] = nStaticProp;` |
|   1226 | 10468 | `		aCount[1] = nStaticMeth;` |
|   1226 | 10469 | `		aCount[2] = nProp;` |
|   1226 | 10470 | `		aCount[3] = nMeth;` |
|   1226 | 10471 | `		SyBlobFormat(pOut, azTitle[iPass], aCount[iPass]);` |
|  20538 | 10472 | `		for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|  19314 | 10473 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - | 10474 | `			int bIsStatic;` |
|  19314 | 10475 | `			if( pM->iKind == REFLECT_MEMBER_CONST ){` |
|   2570 | 10476 | `				continue;` |
|      - | 10477 | `			}` |
|  16746 | 10478 | `			if( bMethods != (pM->iKind == REFLECT_MEMBER_METHOD) ){` |
|   8374 | 10479 | `				continue;` |
|      - | 10480 | `			}` |
|  12162 | 10481 | `			bIsStatic = bMethods ? (pM->pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0` |
|   4584 | 10482 | `				: (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|   8374 | 10483 | `			if( bIsStatic != bStatic ){` |
|   4188 | 10484 | `				continue;` |
|      - | 10485 | `			}` |
|   4188 | 10486 | `			if( bMethods ){` |
|      - | 10487 | `				ReflectFuncRef sRef;` |
|   3790 | 10488 | `				if( !bFirst ){` |
|   3472 | 10489 | `					SyBlobAppend(pOut, "\n", sizeof(char));` |
|   1735 | 10490 | `				}` |
|   3790 | 10491 | `				bFirst = 0;` |
|   3790 | 10492 | `				ReflectFuncFromMethod(pCtx->pVm, pClass, pM->pMeth, &sRef);` |
|   3790 | 10493 | `				rc = ReflectExportFuncBlock(pCtx, pOut, &sRef, "    ", pClass);` |
|   3790 | 10494 | `				if( rc != SXRET_OK ){` |
|    ! 0 | 10495 | `					goto done;` |
|      - | 10496 | `				}` |
|   1896 | 10497 | `			}else{` |
|    400 | 10498 | `				SyBlobAppend(pOut, "    ", sizeof("    ")-1);` |
|    400 | 10499 | `				ReflectExportPropLine(pCtx, pOut, pM->pAttr, &pM->sKey);` |
|      - | 10500 | `			}` |
|   2095 | 10501 | `		}` |
|   1226 | 10502 | `		SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|    614 | 10503 | `	}` |
|    308 | 10504 | `	SyBlobAppend(pOut, "}\n", sizeof("}\n")-1);` |
|    153 | 10505 | `done:` |
|    308 | 10506 | `	SySetRelease(&aMembers);` |
|    308 | 10507 | `	SySetRelease(&aIface);` |
|    308 | 10508 | `	return rc;` |
|      2 | 10509 | `}` |
|      - | 10510 | `/* ---- the five __toString() entry points ---- */` |
|     54 | 10511 | `static int ReflectExportClassSelf(ph7_context *pCtx)` |
|      2 | 10512 | `{` |
|     56 | 10513 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 10514 | `	SyBlob sOut;` |
|      - | 10515 | `	sxi32 rc;` |
|     56 | 10516 | `	if( pClass == 0 ){` |
|    ! 0 | 10517 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 10518 | `		return PH7_OK;` |
|      - | 10519 | `	}` |
|     56 | 10520 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     56 | 10521 | `	rc = ReflectExportClassBlock(pCtx, &sOut, pClass);` |
|     56 | 10522 | `	if( rc == SXRET_OK ){` |
|     56 | 10523 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     27 | 10524 | `	}` |
|     56 | 10525 | `	SyBlobRelease(&sOut);` |
|     56 | 10526 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|     29 | 10527 | `}` |
|      - | 10528 | `/* The standalone Function block for a name the extension walk handed over. */` |
|    270 | 10529 | `static void ReflectExportFuncByName(ph7_context *pCtx, SyBlob *pOut,` |
|      - | 10530 | `	const char *zName, int nName)` |
|      2 | 10531 | `{` |
|      - | 10532 | `	ph7_value sTarget;` |
|      - | 10533 | `	ReflectFuncRef sRef;` |
|    272 | 10534 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|    272 | 10535 | `	ph7_value_string(&sTarget, zName, nName);` |
|    272 | 10536 | `	if( ReflectFuncFill(pCtx->pVm, &sTarget, 0, &sRef) ){` |
|    272 | 10537 | `		ReflectExportFuncBlock(pCtx, pOut, &sRef, "", 0);` |
|    135 | 10538 | `	}` |
|    272 | 10539 | `	PH7_MemObjRelease(&sTarget);` |
|    272 | 10540 | `}` |
|    508 | 10541 | `static int ReflectExportFuncSelf(ph7_context *pCtx)` |
|      5 | 10542 | `{` |
|      - | 10543 | `	ReflectFuncRef sRef;` |
|      - | 10544 | `	SyBlob sOut;` |
|      - | 10545 | `	sxi32 rc;` |
|    513 | 10546 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 | 10547 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 10548 | `		return PH7_OK;` |
|      - | 10549 | `	}` |
|    513 | 10550 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    513 | 10551 | `	rc = ReflectExportFuncBlock(pCtx, &sOut, &sRef, "", ReflectOwnerOfThis(pCtx));` |
|    513 | 10552 | `	if( rc == SXRET_OK ){` |
|    513 | 10553 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    254 | 10554 | `	}` |
|    513 | 10555 | `	SyBlobRelease(&sOut);` |
|    513 | 10556 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    259 | 10557 | `}` |
|    138 | 10558 | `static int ReflectExportParamSelf(ph7_context *pCtx)` |
|      3 | 10559 | `{` |
|      - | 10560 | `	ReflectFuncRef sRef;` |
|      - | 10561 | `	ReflectParamDesc sDesc;` |
|      - | 10562 | `	SyBlob sOut;` |
|    141 | 10563 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) ){` |
|    ! 0 | 10564 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 10565 | `		return PH7_OK;` |
|      - | 10566 | `	}` |
|    141 | 10567 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    141 | 10568 | `	ReflectExportParamLine(pCtx, &sOut, &sDesc);` |
|    141 | 10569 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    141 | 10570 | `	SyBlobRelease(&sOut);` |
|    141 | 10571 | `	return PH7_OK;` |
|     72 | 10572 | `}` |
|      8 | 10573 | `static int ReflectExportPropSelf(ph7_context *pCtx)` |
|      1 | 10574 | `{` |
|      - | 10575 | `	ReflectMemberRef sRef;` |
|      - | 10576 | `	SyBlob sOut;` |
|      - | 10577 | `	SyString sKey;` |
|      9 | 10578 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 10579 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 10580 | `		return PH7_OK;` |
|      - | 10581 | `	}` |
|      9 | 10582 | `	SyStringInitFromBuf(&sKey, sRef.zName, sRef.nName);` |
|      9 | 10583 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|      9 | 10584 | `	ReflectExportPropLine(pCtx, &sOut, sRef.pAttr, &sKey);` |
|      9 | 10585 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|      9 | 10586 | `	SyBlobRelease(&sOut);` |
|      9 | 10587 | `	return PH7_OK;` |
|      5 | 10588 | `}` |
|      6 | 10589 | `static int ReflectExportConstSelf(ph7_context *pCtx)` |
|      1 | 10590 | `{` |
|      - | 10591 | `	ReflectMemberRef sRef;` |
|      - | 10592 | `	SyBlob sOut;` |
|      - | 10593 | `	SyString sKey;` |
|      - | 10594 | `	sxi32 rc;` |
|      7 | 10595 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 10596 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 10597 | `		return PH7_OK;` |
|      - | 10598 | `	}` |
|      7 | 10599 | `	SyStringInitFromBuf(&sKey, sRef.zName, sRef.nName);` |
|      7 | 10600 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|      7 | 10601 | `	rc = ReflectExportConstLine(pCtx, &sOut, sRef.pClass, sRef.pAttr, &sKey);` |
|      7 | 10602 | `	if( rc == SXRET_OK ){` |
|      7 | 10603 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|      3 | 10604 | `	}` |
|      7 | 10605 | `	SyBlobRelease(&sOut);` |
|      7 | 10606 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|      4 | 10607 | `}` |
|      - | 10608 | `/*` |
|      - | 10609 | ` * Install the Reflection API — ~25 classes, all of them declared from C.` |
|      - | 10610 | ` *` |
|      - | 10611 | `` * There is no global thunk table left to register: every `__reflect_*` and`` |
|      - | 10612 | `` * `__phl_rcinfo` became a method of the class that always owned it, so`` |
|      - | 10613 | ` * Reflection adds no name to php's global function namespace at all. Called` |
|      - | 10614 | ` * from PH7_VmInit while pVm->bCompilingBuiltin is set, right after the core` |
|      - | 10615 | ` * builtin chunks (Exception and friends have to exist already).` |
|      - | 10616 | ` */` |
|   6721 | 10617 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|      5 | 10618 | `{` |
|      - | 10619 | `	sxi32 rc;` |
|      - | 10620 | `	/* Where chunk 1 was: Reflector, Reflection, ReflectionException,` |
|      - | 10621 | `	 * ReflectionClass and ReflectionObject are native now. Chunks 2 and 3 name` |
|      - | 10622 | ``	 * `Reflector` in their own `implements` clauses, so this has to run first. */`` |
|   6726 | 10623 | `	rc = PH7_VmInstallReflectionClass(&(*pVm));` |
|   6726 | 10624 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10625 | `		return rc;` |
|      - | 10626 | `	}` |
|      - | 10627 | `	/* Where chunk 2 was: ReflectionFunctionAbstract, ReflectionFunction,` |
|      - | 10628 | `	 * ReflectionMethod and ReflectionParameter are native now. */` |
|   6726 | 10629 | `	rc = PH7_VmInstallReflectionFunc(&(*pVm));` |
|   6726 | 10630 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10631 | `		return rc;` |
|      - | 10632 | `	}` |
|   6726 | 10633 | `	rc = PH7_VmInstallReflectionHookType(&(*pVm));` |
|   6726 | 10634 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10635 | `		return rc;` |
|      - | 10636 | `	}` |
|      - | 10637 | `	/* Where chunk 3 was: ReflectionProperty and ReflectionClassConstant are` |
|      - | 10638 | `	 * native now, and PropertyHookType is a native ENUM. */` |
|   6726 | 10639 | `	rc = PH7_VmInstallReflectionMember(&(*pVm));` |
|   6726 | 10640 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10641 | `		return rc;` |
|      - | 10642 | `	}` |
|      - | 10643 | `	/* Where chunk 4 was: the four type classes are native now (see` |
|      - | 10644 | `	 * PH7_VmInstallReflectionTypes). Stringable exists by this point. */` |
|   6726 | 10645 | `	rc = PH7_VmInstallReflectionTypes(&(*pVm));` |
|   6726 | 10646 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10647 | `		return rc;` |
|      - | 10648 | `	}` |
|      - | 10649 | `	/* Where chunk 5 was: ReflectionGenerator/Fiber and the four standalone` |
|      - | 10650 | `	 * classes chunk 6 used to hold are native now. Reflector (chunk 1) exists. */` |
|   6726 | 10651 | `	rc = PH7_VmInstallReflectionSmall(&(*pVm));` |
|   6726 | 10652 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10653 | `		return rc;` |
|      - | 10654 | `	}` |
|      - | 10655 | `	/* Where chunk 6 was: the three ReflectionEnum classes are native now, and` |
|      - | 10656 | `	 * the class DESCRIPTOR they read (__phl_rcinfo) has no callers left. */` |
|   6726 | 10657 | `	rc = PH7_VmInstallReflectionEnum(&(*pVm));` |
|   6726 | 10658 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10659 | `		return rc;` |
|      - | 10660 | `	}` |
|      - | 10661 | `	/* Where chunk 7 was: ReflectionAttribute is native now, and with it the` |
|      - | 10662 | `	 * builder (__reflect_build_attrs) and the two helpers it needed. */` |
|   6726 | 10663 | `	rc = PH7_VmInstallReflectionAttribute(&(*pVm));` |
|   6726 | 10664 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 10665 | `		return rc;` |
|      - | 10666 | `	}` |
|      - | 10667 | `	/* Where chunk 9 was: the export format is ReflectExportClassSelf() and its` |
|      - | 10668 | `	 * four siblings above. Nothing of the Reflection library is PHP any more,` |
|      - | 10669 | `	 * so vm_builtin_reflection_lib.c is gone and this IS the installer. */` |
|   6726 | 10670 | `	return SXRET_OK;` |
|   3361 | 10671 | `}` |
|      - | 10672 |  |
