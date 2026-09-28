# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5190/5987 lines (86.69%)

[Root index](../../index.md) | [Directory index](index.md)

|   Hits | Line | Source |
| -----: | ---: | :--- |
|      - |    1 | `/**` |
|      - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|      - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|      - |    4 | ` */` |
|      - |    5 | `#include "ph7int.h"` |
|      - |    6 | `/*` |
|      - |    7 | ` * This file implements the PHP 8.5 Reflection API.` |
|      - |    8 | ` *` |
|      - |    9 | ` * Following the engine's builtin-class pattern (Generator/Fiber/Closure),` |
|      - |   10 | ` * the Reflection classes themselves are written in PHP, embedded below as` |
|      - |   11 | ` * C string chunks and compiled at VM init by PH7_VmInstallReflection().` |
|      - |   12 | ` * Native behavior is provided by a small set of global __reflect_* thunk` |
|      - |   13 | ` * functions implemented here: the PHP methods forward to them, passing` |
|      - |   14 | ` * their target (class name, object, ...) explicitly.` |
|      - |   15 | ` *` |
|      - |   16 | ` * The chunks are kept below 30 KB each: MSVC caps a concatenated string` |
|      - |   17 | ` * literal at 65,535 bytes and the Windows build is real (build-aux/nmake.mk).` |
|      - |   18 | ` */` |
|      - |   19 |  |
|      - |   20 | `/* Bound on hierarchy walks; matches PH7_INTERFACE_WALK_MAX_DEPTH in` |
|      - |   21 | ` * vm_builtin_class.c. */` |
|      - |   22 | `#define REFLECT_WALK_MAX_DEPTH 64` |
|      - |   23 |  |
|      - |   24 | `static sxi32 ReflectEnforceStore(ph7_context *pCtx, sxu32 nIdx, ph7_value *pValue);` |
|      - |   25 |  |
|      - |   26 | `/*` |
|      - |   27 | ` * Resolve a class-name string or object into a ph7_class pointer,` |
|      - |   28 | ` * triggering autoload for unknown string names. Returns NULL when the` |
|      - |   29 | ` * class does not exist (the PHP layer turns that into ReflectionException).` |
|      - |   30 | ` */` |
|   4280 |   31 | `static ph7_class * ReflectResolveClass(ph7_vm *pVm, ph7_value *pArg)` |
|      5 |   32 | `{` |
|      - |   33 | `	ph7_class *pClass;` |
|   4285 |   34 | `	pClass = PH7_VmExtractClassFromValue(pVm, pArg);` |
|   4285 |   35 | `	if( pClass == 0 && ph7_value_is_string(pArg) ){` |
|      - |   36 | `		const char *zName;` |
|      - |   37 | `		int nLen;` |
|     20 |   38 | `		zName = ph7_value_to_string(pArg, &nLen);` |
|     20 |   39 | `		if( nLen > 0 ){` |
|     20 |   40 | `			pClass = PH7_VmTriggerAutoload(pVm, zName, (sxu32)nLen, FALSE);` |
|      9 |   41 | `		}` |
|      9 |   42 | `	}` |
|   4285 |   43 | `	return pClass;` |
|      5 |   44 | `}` |
|      - |   45 | `/*` |
|      - |   46 | ` * Hand a freshly created class instance to the caller. The return slot` |
|      - |   47 | ` * takes over the initial reference from PH7_NewClassInstance (iRef=1):` |
|      - |   48 | ` * no extra iRef++ here (see the synthesized-object invariant — a stray` |
|      - |   49 | ` * bump leaks the object and disables its __destruct).` |
|      - |   50 | ` */` |
|   1480 |   51 | `static int ReflectResultObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|      5 |   52 | `{` |
|   1485 |   53 | `	if( pObj == 0 ){` |
|    ! 0 |   54 | `		ph7_result_null(pCtx);` |
|    ! 0 |   55 | `		return PH7_OK;` |
|      - |   56 | `	}` |
|   1485 |   57 | `	PH7_MemObjRelease(pCtx->pRet);` |
|   1485 |   58 | `	pCtx->pRet->x.pOther = pObj;` |
|   1485 |   59 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|   1485 |   60 | `	return PH7_OK;` |
|    745 |   61 | `}` |
|      - |   62 | `/* The last of the descriptor marshalling: ReflectMapAddDyn survives because` |
|      - |   63 | ` * ReflectAttrArgs() answers a php ARRAY whose named arguments are string keys.` |
|      - |   64 | ` * Its Bool/Int/Str/Null/Attrs/Doc siblings went with __phl_rcinfo. */` |
|      - |   65 | `/* Add an entry under a dynamic (SyString) key. */` |
|    114 |   66 | `static void ReflectMapAddDyn(ph7_context *pCtx, ph7_value *pMap,` |
|      - |   67 | `	const SyString *pKey, ph7_value *pVal)` |
|      2 |   68 | `{` |
|    116 |   69 | `	ph7_value *pK = ph7_context_new_scalar(pCtx);` |
|    116 |   70 | `	if( pK == 0 ){ return; }` |
|    116 |   71 | `	ph7_value_string(pK, pKey->zString, (int)pKey->nByte);` |
|    116 |   72 | `	ph7_array_add_elem(pMap, pK, pVal);` |
|     59 |   73 | `}` |
|      - |   74 | `/*` |
|      - |   75 | ` * Append pIface (and its parents / extended interfaces) to the dedup set` |
|      - |   76 | ` * of ph7_class pointers.` |
|      - |   77 | ` */` |
|    374 |   78 | `static void ReflectAddInterface(ph7_class *pIface, SySet *pOut, int iDepth)` |
|      2 |   79 | `{` |
|      - |   80 | `	ph7_class **apKnown;` |
|      - |   81 | `	sxu32 n;` |
|    376 |   82 | `	if( pIface == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|    ! 0 |   83 | `		return;` |
|      - |   84 | `	}` |
|      - |   85 | `	/* Parents of an interface come along too (interface B extends A) */` |
|    376 |   86 | `	if( pIface->pBase ){` |
|    107 |   87 | `		ReflectAddInterface(pIface->pBase, pOut, iDepth + 1);` |
|     53 |   88 | `	}` |
|      - |   89 | `	/* Some engines record extended interfaces in aInterface as well */` |
|    376 |   90 | `	apKnown = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|    376 |   91 | `	for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|    ! 0 |   92 | `		ReflectAddInterface(apKnown[n], pOut, iDepth + 1);` |
|    ! 0 |   93 | `	}` |
|      - |   94 | `	/* Dedup by pointer */` |
|    376 |   95 | `	apKnown = (ph7_class **)SySetBasePtr(pOut);` |
|    616 |   96 | `	for( n = 0 ; n < SySetUsed(pOut) ; n++ ){` |
|    281 |   97 | `		if( apKnown[n] == pIface ){` |
|     41 |   98 | `			return;` |
|      - |   99 | `		}` |
|    121 |  100 | `	}` |
|    336 |  101 | `	SySetPut(pOut, (const void *)&pIface);` |
|    189 |  102 | `}` |
|      - |  103 | `/*` |
|      - |  104 | ` * Collect the transitive set of interfaces implemented by pClass:` |
|      - |  105 | ` * the parent chain's interfaces first, then the class's own.` |
|      - |  106 | ` */` |
|    368 |  107 | `static void ReflectCollectInterfaces(ph7_class *pClass, SySet *pOut, int iDepth)` |
|      2 |  108 | `{` |
|      - |  109 | `	ph7_class **apIface;` |
|      - |  110 | `	sxu32 n;` |
|    370 |  111 | `	if( pClass == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|    ! 0 |  112 | `		return;` |
|      - |  113 | `	}` |
|    370 |  114 | `	if( pClass->pBase ){` |
|    118 |  115 | `		ReflectCollectInterfaces(pClass->pBase, pOut, iDepth + 1);` |
|     58 |  116 | `	}` |
|    370 |  117 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    590 |  118 | `	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){` |
|    222 |  119 | `		ReflectAddInterface(apIface[n], pOut, iDepth + 1);` |
|    112 |  120 | `	}` |
|    186 |  121 | `}` |
|      - |  122 | `/*` |
|      - |  123 | ` * Deepest base class whose method table maps the same name to the very` |
|      - |  124 | ` * same ph7_class_method pointer: inheritance shares member pointers` |
|      - |  125 | ` * (PH7_ClassInherit), so this identifies the declaring class. Methods` |
|      - |  126 | ` * copied in from traits are not on the pBase chain and thus report the` |
|      - |  127 | ` * using class, which is what PHP reports too.` |
|      - |  128 | ` */` |
|  16262 |  129 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth)` |
|      5 |  130 | `{` |
|  16267 |  131 | `	ph7_class *pDecl = pClass;` |
|  16267 |  132 | `	ph7_class *pBase = pClass->pBase;` |
|  16267 |  133 | `	int iDepth = 0;` |
|  20379 |  134 | `	while( pBase && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|      - |  135 | `		SyHashEntry *pEntry;` |
|   8477 |  136 | `		pEntry = SyHashGet(&pBase->hMethod, (const void *)SyStringData(&pMeth->sFunc.sName),` |
|   2825 |  137 | `			SyStringLength(&pMeth->sFunc.sName));` |
|   5652 |  138 | `		if( pEntry == 0 \|\| (ph7_class_method *)pEntry->pUserData != pMeth ){` |
|    771 |  139 | `			break;` |
|      - |  140 | `		}` |
|   4113 |  141 | `		pDecl = pBase;` |
|   4113 |  142 | `		pBase = pBase->pBase;` |
|   4113 |  143 | `		iDepth++;` |
|      1 |  144 | `	}` |
|  16267 |  145 | `	return pDecl;` |
|      5 |  146 | `}` |
|      - |  147 | `/*` |
|      - |  148 | ` * The interface list php reports for pClass: the transitive set, plus an` |
|      - |  149 | `` * INTERFACE's own parents (`interface B extends A` lists A). Caller owns the`` |
|      - |  150 | ` * set (SySetInit with sizeof(ph7_class *)).` |
|      - |  151 | ` */` |
|    252 |  152 | `static void ReflectInterfacesOf(ph7_class *pClass, SySet *pOut)` |
|      2 |  153 | `{` |
|    254 |  154 | `	ReflectCollectInterfaces(pClass, pOut, 0);` |
|    254 |  155 | `	if( (pClass->iFlags & PH7_CLASS_INTERFACE) && pClass->pBase ){` |
|     49 |  156 | `		ReflectAddInterface(pClass->pBase, pOut, 0);` |
|     24 |  157 | `	}` |
|    254 |  158 | `}` |
|      - |  159 | `/* Fetch a class attribute (property or constant) by plain name. */` |
|      4 |  160 | `static ph7_class_attr * ReflectFetchAttr(ph7_class *pClass, ph7_value *pName)` |
|      1 |  161 | `{` |
|      - |  162 | `	SyHashEntry *pEntry;` |
|      - |  163 | `	const char *zName;` |
|      - |  164 | `	int nLen;` |
|      5 |  165 | `	zName = ph7_value_to_string(pName, &nLen);` |
|      5 |  166 | `	if( nLen < 1 ){` |
|    ! 0 |  167 | `		return 0;` |
|      - |  168 | `	}` |
|      5 |  169 | `	pEntry = SyHashGet(&pClass->hAttr, (const void *)zName, (sxu32)nLen);` |
|      5 |  170 | `	if( pEntry == 0 ){` |
|      3 |  171 | `		return 0;` |
|      - |  172 | `	}` |
|      3 |  173 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|      3 |  174 | `}` |
|      - |  175 | `/* Fetch a class CONSTANT (or enum case) by name from the hConst namespace. */` |
|      2 |  176 | `static ph7_class_attr * ReflectFetchConst(ph7_class *pClass, ph7_value *pName)` |
|      1 |  177 | `{` |
|      - |  178 | `	const char *zName;` |
|      - |  179 | `	int nLen;` |
|      3 |  180 | `	zName = ph7_value_to_string(pName, &nLen);` |
|      3 |  181 | `	if( nLen < 1 ){` |
|    ! 0 |  182 | `		return 0;` |
|      - |  183 | `	}` |
|      3 |  184 | `	return PH7_ClassExtractConstant(pClass, zName, (sxu32)nLen);` |
|      2 |  185 | `}` |
|      - |  186 | `/* Fetch a member by name from EITHER namespace (property then constant) — used` |
|      - |  187 | ` * where the caller reflects over a member that may be either (e.g. attributes` |
|      - |  188 | ` * attached to a property or a constant). */` |
|      4 |  189 | `static ph7_class_attr * ReflectFetchMember(ph7_class *pClass, ph7_value *pName)` |
|      1 |  190 | `{` |
|      5 |  191 | `	ph7_class_attr *pAttr = ReflectFetchAttr(pClass, pName);` |
|      5 |  192 | `	if( pAttr == 0 ){` |
|      3 |  193 | `		pAttr = ReflectFetchConst(pClass, pName);` |
|      1 |  194 | `	}` |
|      5 |  195 | `	return pAttr;` |
|      1 |  196 | `}` |
|      - |  197 | `/*` |
|      - |  198 | ` * ---------------------------------------------------------------------------` |
|      - |  199 | ` * The member walk.` |
|      - |  200 | ` *` |
|      - |  201 | ` * php reports a class's members in ONE order — the class's own first (in` |
|      - |  202 | ` * declaration order), then each inheritance level's, outward — and every` |
|      - |  203 | ` * accessor that lists or looks one up has to agree with it. It used to live` |
|      - |  204 | ` * inside the descriptor builder alone; the native ReflectionClass needs the` |
|      - |  205 | ` * same order for getMethods()/getProperties()/getReflectionConstants() and the` |
|      - |  206 | ` * same visibility filtering for hasMethod()/getMethod()/getConstructor(), so` |
|      - |  207 | ` * the walk is factored out here and every one of them drives it.` |
|      - |  208 | ` *` |
|      - |  209 | ` * Per level the DECLARING class's own hash is iterated — a subclass hash` |
|      - |  210 | ` * interleaves inherited pointers unpredictably — and a pointer-identity lookup` |
|      - |  211 | ` * in the reflected class's hash drops what is not visible there (overridden` |
|      - |  212 | ` * entries). Methods come out reversed because hMethod is still a head-insert` |
|      - |  213 | ` * table, while hAttr/hConst insert at the tail.` |
|      - |  214 | ` * ---------------------------------------------------------------------------` |
|      - |  215 | ` */` |
|      - |  216 | `#define REFLECT_MEMBER_PROP   0` |
|      - |  217 | `#define REFLECT_MEMBER_CONST  1` |
|      - |  218 | `#define REFLECT_MEMBER_METHOD 2` |
|      - |  219 |  |
|      - |  220 | `typedef struct ReflectMember ReflectMember;` |
|      - |  221 | `struct ReflectMember` |
|      - |  222 | `{` |
|      - |  223 | `	int iKind;               /* REFLECT_MEMBER_* */` |
|      - |  224 | `	SyString sKey;           /* the name php reports it under: a trait` |
|      - |  225 | ``	                          * `use T { m as n; }` alias differs from the`` |
|      - |  226 | `	                          * method's own sFunc.sName, and php reports n */` |
|      - |  227 | `	ph7_class *pDecl;        /* declaring class */` |
|      - |  228 | `	ph7_class_attr *pAttr;   /* property or constant (NULL for a method) */` |
|      - |  229 | `	ph7_class_method *pMeth; /* method (NULL for a property or constant) */` |
|      - |  230 | `};` |
|      - |  231 | `/*` |
|      - |  232 | ` * Collect the members php would report for pClass, in php's own order, into a` |
|      - |  233 | ` * SySet of ReflectMember. The caller owns the set (SySetInit with` |
|      - |  234 | ` * sizeof(ReflectMember) / SySetRelease).` |
|      - |  235 | ` *` |
|      - |  236 | ` * bLookup selects which of php's TWO answers is wanted. The LISTING` |
|      - |  237 | ` * (getMethods()/getProperties()/getReflectionConstants(), bLookup = 0) hides a` |
|      - |  238 | ` * base class's private members; the LOOKUP (bLookup = 1) does not, because php` |
|      - |  239 | ` * keeps a parent's private in the child's tables and ReflectionMethod resolves` |
|      - |  240 | ` * against those. The two really do disagree: hasMethod('basePriv') is true on` |
|      - |  241 | ` * the subclass while getMethods() never mentions it.` |
|      - |  242 | ` */` |
|   1378 |  243 | `static void ReflectMembers(ph7_vm *pVm, ph7_class *pClass, SySet *pOut, int bLookup)` |
|      4 |  244 | `{` |
|      - |  245 | `	ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];` |
|   1382 |  246 | `	ph7_class *pWalk = pClass;` |
|      - |  247 | `	SyHashEntry *pEntry;` |
|      - |  248 | `	SySet aTmp;` |
|   1382 |  249 | `	int bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;` |
|      - |  250 | `	int iPart;` |
|   1382 |  251 | `	sxu32 nChain = 0, iLevel, nLev, nT;` |
|   2924 |  252 | `	while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){` |
|   1546 |  253 | `		aChain[nChain++] = pWalk;` |
|   1546 |  254 | `		pWalk = pWalk->pBase;` |
|      4 |  255 | `	}` |
|   1382 |  256 | `	SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));` |
|      - |  257 | `	/* PROPERTIES of an INTERNAL class come out base-first, and that is php's` |
|      - |  258 | `	 * registration order rather than a rule of its own: a user class declares its` |
|      - |  259 | ``	 * own properties and THEN inherits (`class B extends A` reports B's before`` |
|      - |  260 | `	 * A's), while an internal one is registered against its parent and declares` |
|      - |  261 | `	 * afterwards — so ErrorException reports Exception's five and then its own` |
|      - |  262 | `	 * $severity. Only the property table flips: php lists an internal class's` |
|      - |  263 | `	 * METHODS and CONSTANTS own-first like everything else, so the walk below` |
|      - |  264 | `	 * runs the two tables in opposite level orders. */` |
|   5516 |  265 | `	for( iPart = 0 ; iPart < 3 ; iPart++ ){` |
|   8764 |  266 | `	  for( nLev = 0 ; nLev < nChain ; nLev++ ){` |
|      - |  267 | `		ph7_class *pLevel;` |
|   4630 |  268 | `		int iTab = iPart;` |
|   4630 |  269 | `		iLevel = (iPart == 0 && bInternal) ? nChain - 1 - nLev : nLev;` |
|   4630 |  270 | `		pLevel = aChain[iLevel];` |
|      - |  271 | `		/* --- Properties (hAttr, iPart 0) and constants/enum cases (hConst,` |
|      - |  272 | `		 * iPart 1) — php's two separate member namespaces. Each table is` |
|      - |  273 | `		 * collected and emitted independently; the CONSTANT flag still decides` |
|      - |  274 | `		 * which kind comes out. --- */` |
|   4630 |  275 | `		if( iPart < 2 ){` |
|   3088 |  276 | `			SyHash *pSrcHash = iTab ? &pLevel->hConst : &pLevel->hAttr;` |
|   3088 |  277 | `			SyHash *pRefHash = iTab ? &pClass->hConst : &pClass->hAttr;` |
|   3088 |  278 | `			SySetReset(&aTmp);` |
|   3088 |  279 | `			SyHashResetLoopCursor(pSrcHash);` |
|  14134 |  280 | `			while( (pEntry = SyHashGetNextEntry(pSrcHash)) != 0 ){` |
|  11050 |  281 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  11050 |  282 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|  11050 |  283 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_HIDDEN ){` |
|      - |  284 | `					/* A native class's engine slot: php holds that state in its own C` |
|      - |  285 | `					 * struct and reports no property for it at all. */` |
|    844 |  286 | `					continue;` |
|      - |  287 | `				}` |
|  10208 |  288 | `				if( iLevel == 0 ){` |
|      - |  289 | `					sxu32 j;` |
|      - |  290 | `					/* Own = declared here or by an off-chain provider (trait) */` |
|   9974 |  291 | `					for( j = 1 ; j < nChain ; j++ ){` |
|    887 |  292 | `						if( aChain[j] == pDecl ){ break; }` |
|    164 |  293 | `					}` |
|   9648 |  294 | `					if( j < nChain ){ continue; }` |
|   4546 |  295 | `				}else{` |
|      - |  296 | `					SyHashEntry *pSub;` |
|    561 |  297 | `					if( pDecl != pLevel ){ continue; }` |
|      - |  298 | `					/* A base's PRIVATE member is not part of the subclass's` |
|      - |  299 | `					 * surface — php reports neither a private property nor a` |
|      - |  300 | `					 * private constant of a parent on the child. PHL's` |
|      - |  301 | `					 * inheritance copies them down all the same, so the filter` |
|      - |  302 | `					 * has to be here. */` |
|    561 |  303 | `					if( !bLookup && pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){ continue; }` |
|      - |  304 | `					/* Must still be the visible member in the reflected class */` |
|    479 |  305 | `					pSub = SyHashGet(pRefHash, pEntry->pKey, pEntry->nKeyLen);` |
|    479 |  306 | `					if( pSub == 0 \|\| pSub->pUserData != (void *)pAttr ){ continue; }` |
|      - |  307 | `				}` |
|   9566 |  308 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|      4 |  309 | `			}` |
|      - |  310 | `			/* Forward: hAttr/hConst iterate in DECLARATION order (tail inserts),` |
|      - |  311 | `			 * so members come out in the order php reports them. */` |
|  12650 |  312 | `			for( nT = 0 ; nT < SySetUsed(&aTmp) ; nT++ ){` |
|   9566 |  313 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT);` |
|   9566 |  314 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;` |
|      - |  315 | `				ReflectMember sMember;` |
|   9566 |  316 | `				sMember.iKind = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|   4781 |  317 | `					? REFLECT_MEMBER_CONST : REFLECT_MEMBER_PROP;` |
|   9566 |  318 | `				sMember.sKey = pAttr->sName;` |
|   9566 |  319 | `				sMember.pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|   9566 |  320 | `				sMember.pAttr = pAttr;` |
|   9566 |  321 | `				sMember.pMeth = 0;` |
|   9566 |  322 | `				SySetPut(pOut, (const void *)&sMember);` |
|   4785 |  323 | `			}` |
|   3088 |  324 | `			continue;` |
|      - |  325 | `		}` |
|      - |  326 | `		/* --- Methods. The reported name is the hash-entry KEY, not the` |
|      - |  327 | `		 * function's own name (see ReflectMember::sKey). --- */` |
|   1546 |  328 | `		SySetReset(&aTmp);` |
|   1546 |  329 | `		SyHashResetLoopCursor(&pLevel->hMethod);` |
|  10068 |  330 | `		while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){` |
|   8526 |  331 | `			ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   8526 |  332 | `			ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|      - |  333 | `			/* A property HOOK is compiled into a method (__phl_hook_get_NAME /` |
|      - |  334 | ``			 * __phl_hook_set_NAME) so that inheritance and `parent::` work on`` |
|      - |  335 | `			 * it, but php has no method there at all: it reports the hook on` |
|      - |  336 | `			 * the PROPERTY. Listing it would put a PHL-only name on a` |
|      - |  337 | `			 * php-visible surface. */` |
|   8522 |  338 | `			if( pEntry->nKeyLen > sizeof("__phl_hook_")-1` |
|   5505 |  339 | `			 && SyMemcmp(pEntry->pKey, "__phl_hook_", sizeof("__phl_hook_")-1) == 0 ){` |
|    713 |  340 | `				continue;` |
|      - |  341 | `			}` |
|   7816 |  342 | `			if( iLevel == 0 ){` |
|      - |  343 | `				sxu32 j;` |
|   7402 |  344 | `				for( j = 1 ; j < nChain ; j++ ){` |
|   1451 |  345 | `					if( aChain[j] == pDecl ){ break; }` |
|    364 |  346 | `				}` |
|   6676 |  347 | `				if( j < nChain ){ continue; }` |
|   2978 |  348 | `			}else{` |
|      - |  349 | `				SyHashEntry *pSub;` |
|   1141 |  350 | `				if( pDecl != pLevel ){ continue; }` |
|      - |  351 | `				/* Same rule as the members above: a base's PRIVATE method is` |
|      - |  352 | ``				 * not on the subclass's surface. `class B extends A` lists`` |
|      - |  353 | `				 * only A::q when A::p is private — php's inheritance never` |
|      - |  354 | `				 * hands the child a private, and PH7_ClassInherit's copy-down` |
|      - |  355 | `				 * does, so it is filtered here. */` |
|    805 |  356 | `				if( !bLookup && pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){ continue; }` |
|    763 |  357 | `				pSub = SyHashGet(&pClass->hMethod, pEntry->pKey, pEntry->nKeyLen);` |
|    763 |  358 | `				if( pSub == 0 \|\| pSub->pUserData != (void *)pMeth ){` |
|      - |  359 | `					/* Overridden below this level: already reported */` |
|     81 |  360 | `					continue;` |
|      - |  361 | `				}` |
|      - |  362 | `			}` |
|   6634 |  363 | `			SySetPut(&aTmp, (const void *)&pEntry);` |
|      4 |  364 | `		}` |
|   8176 |  365 | `		for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|   6634 |  366 | `			SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|      - |  367 | `			ReflectMember sMember;` |
|   6634 |  368 | `			sMember.iKind = REFLECT_MEMBER_METHOD;` |
|   6634 |  369 | `			SyStringInitFromBuf(&sMember.sKey, (const char *)pE->pKey, pE->nKeyLen);` |
|   6634 |  370 | `			sMember.pMeth = (ph7_class_method *)pE->pUserData;` |
|   6634 |  371 | `			sMember.pDecl = ReflectMethodDeclClass(pClass, sMember.pMeth);` |
|   6634 |  372 | `			sMember.pAttr = 0;` |
|   6634 |  373 | `			SySetPut(pOut, (const void *)&sMember);` |
|   3319 |  374 | `		}` |
|    775 |  375 | `	  }` |
|   2071 |  376 | `	}` |
|   1382 |  377 | `	SySetRelease(&aTmp);` |
|   1382 |  378 | `}` |
|      - |  379 | `/* Does a collected member name match zName exactly? */` |
|   4562 |  380 | `static int ReflectKeyIs(const ReflectMember *pM, const char *zName, int nName)` |
|      3 |  381 | `{` |
|   5497 |  382 | `	return SyStringLength(&pM->sKey) == (sxu32)nName` |
|   4562 |  383 | `		&& SyMemcmp(SyStringData(&pM->sKey), zName, (sxu32)nName) == 0;` |
|      3 |  384 | `}` |
|      - |  385 | `/*` |
|      - |  386 | ` * php's METHOD lookup, which is NOT the listing.` |
|      - |  387 | ` *` |
|      - |  388 | ` * getMethods() hides a base's private method, but hasMethod()/getMethod() find` |
|      - |  389 | ` * one: Zend keeps the parent's private in the child's function table and reads` |
|      - |  390 | ` * that table directly. PHL's inheritance copies methods down the same way, so` |
|      - |  391 | ` * the lookup is the class's own hMethod — case-insensitively, like php.` |
|      - |  392 | ` */` |
|   1000 |  393 | `static SyHashEntry * ReflectFindMethodEntry(ph7_class *pClass, const char *zName, int nName)` |
|      5 |  394 | `{` |
|      - |  395 | `	SyHashEntry *pEntry;` |
|   1005 |  396 | `	if( nName < 1 ){` |
|    ! 0 |  397 | `		return 0;` |
|      - |  398 | `	}` |
|   1005 |  399 | `	pEntry = SyHashGet(&pClass->hMethod, (const void *)zName, (sxu32)nName);` |
|   1005 |  400 | `	if( pEntry ){` |
|    811 |  401 | `		return pEntry;` |
|      - |  402 | `	}` |
|    196 |  403 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|    615 |  404 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    322 |  405 | `		if( (int)pEntry->nKeyLen == nName` |
|    174 |  406 | `		 && SyStrnicmp((const char *)pEntry->pKey, zName, (sxu32)nName) == 0 ){` |
|    ! 0 |  407 | `			return pEntry;` |
|      - |  408 | `		}` |
|      2 |  409 | `	}` |
|    196 |  410 | `	return 0;` |
|    505 |  411 | `}` |
|      - |  412 | `/*` |
|      - |  413 | `` * The visibility of the __construct / __clone `new` and `clone` would reach, or`` |
|      - |  414 | ` * 0 when the class has none — what isInstantiable() and isCloneable() screen on.` |
|      - |  415 | ` * The LOOKUP, not the listing: a class that inherits a private constructor is` |
|      - |  416 | ` * still not instantiable even though getMethods() does not report one.` |
|      - |  417 | ` */` |
|     62 |  418 | `static void ReflectCtorCloneVis(ph7_vm *pVm, ph7_class *pClass, sxi32 *piCtor, sxi32 *piClone)` |
|      3 |  419 | `{` |
|      - |  420 | `	ph7_class_method *pMeth;` |
|     31 |  421 | `	SXUNUSED(pVm);` |
|     65 |  422 | `	*piCtor = *piClone = 0;` |
|     65 |  423 | `	pMeth = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|     65 |  424 | `	if( pMeth ){` |
|     32 |  425 | `		*piCtor = pMeth->iProtection;` |
|     15 |  426 | `	}` |
|     65 |  427 | `	pMeth = PH7_ClassExtractMethod(pClass, "__clone", sizeof("__clone")-1);` |
|     65 |  428 | `	if( pMeth ){` |
|      7 |  429 | `		*piClone = pMeth->iProtection;` |
|      3 |  430 | `	}` |
|     65 |  431 | `}` |
|      - |  432 | `/* Where __phl_rcinfo() was: a full class DESCRIPTOR array — every constant,` |
|      - |  433 | ` * property and method of the whole inheritance chain, marshalled once and` |
|      - |  434 | ` * memoized on pVm->hClassInfo because the prelude classes rebuilt it once per` |
|      - |  435 | ` * member they constructed. Every one of its readers is a native class now and` |
|      - |  436 | ` * reads ph7_class directly, so the builder and the VM memo are both gone. */` |
|      - |  437 | `/*` |
|      - |  438 | ` * Collect a PHP array's values into a ph7_value* set (call arguments).` |
|      - |  439 | ` * When ppNames is non-NULL, string keys become named arguments: a name` |
|      - |  440 | ` * map is lazily allocated (like call_user_func_array's) with one entry` |
|      - |  441 | ` * per collected slot, empty entries meaning positional.` |
|      - |  442 | ` */` |
|     44 |  443 | `static sxi32 ReflectCollectArgs(ph7_context *pCtx, ph7_value *pArray, SySet *pOut, SyString **ppNames)` |
|      2 |  444 | `{` |
|      - |  445 | `	ph7_hashmap *pMap;` |
|      - |  446 | `	ph7_hashmap_node *pEntry;` |
|     46 |  447 | `	SyString *aNames = 0;` |
|     46 |  448 | `	sxu32 nSlot = 0;` |
|      - |  449 | `	sxu32 n;` |
|     46 |  450 | `	if( ppNames ){` |
|     40 |  451 | `		*ppNames = 0;` |
|     19 |  452 | `	}` |
|     46 |  453 | `	if( !ph7_value_is_array(pArray) ){` |
|    ! 0 |  454 | `		return SXRET_OK;` |
|      - |  455 | `	}` |
|     46 |  456 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     46 |  457 | `	pEntry = pMap->pFirst;` |
|     96 |  458 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     51 |  459 | `		ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|     51 |  460 | `		if( pValue ){` |
|     51 |  461 | `			if( ppNames && pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      5 |  462 | `				if( aNames == 0 ){` |
|      7 |  463 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|      4 |  464 | `						pMap->nEntry * sizeof(SyString));` |
|      5 |  465 | `					if( aNames ){` |
|      5 |  466 | `						SyZero(aNames, pMap->nEntry * sizeof(SyString));` |
|      2 |  467 | `					}` |
|      2 |  468 | `				}` |
|      5 |  469 | `				if( aNames ){` |
|      5 |  470 | `					SyStringInitFromBuf(&aNames[nSlot],` |
|      - |  471 | `						SyBlobData(&pEntry->xKey.sKey), SyBlobLength(&pEntry->xKey.sKey));` |
|      2 |  472 | `				}` |
|      2 |  473 | `			}` |
|     51 |  474 | `			SySetPut(pOut, (const void *)&pValue);` |
|     51 |  475 | `			nSlot++;` |
|     25 |  476 | `		}` |
|     51 |  477 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     26 |  478 | `	}` |
|     46 |  479 | `	if( ppNames ){` |
|     40 |  480 | `		*ppNames = aNames;` |
|     19 |  481 | `	}` |
|     46 |  482 | `	return SXRET_OK;` |
|     24 |  483 | `}` |
|      - |  484 | `/*` |
|      - |  485 | ` * Instantiate pClassName and run its constructor over pArgs (a PHP array;` |
|      - |  486 | `` * string keys become NAMED arguments, which is how `#[Attr(x: 1)]` arrives).`` |
|      - |  487 | ` * The object lands in the call's result slot.` |
|      - |  488 | ` *` |
|      - |  489 | ` * ReflectionAttribute::newInstance()'s back end. It is deliberately NOT the` |
|      - |  490 | ` * ReflectionClass one (ReflectNewInstance, below): php runs no instantiability` |
|      - |  491 | ` * or constructor-visibility screen here — the attribute's own #[Attribute]` |
|      - |  492 | ` * declaration is what was checked — so an abstract or private-ctor attribute` |
|      - |  493 | ` * class reaches the engine's own Error, not a ReflectionException.` |
|      - |  494 | ` */` |
|     40 |  495 | `static sxi32 ReflectAttrInstantiate(ph7_context *pCtx, ph7_value *pClassName, ph7_value *pArgs)` |
|      2 |  496 | `{` |
|     42 |  497 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  498 | `	ph7_class *pClass;` |
|      - |  499 | `	ph7_class_instance *pThis;` |
|      - |  500 | `	ph7_class_method *pCons;` |
|     42 |  501 | `	if( (pClass = ReflectResolveClass(pVm, pClassName)) == 0 ){` |
|    ! 0 |  502 | `		ph7_result_null(pCtx);` |
|    ! 0 |  503 | `		return PH7_OK;` |
|      - |  504 | `	}` |
|     42 |  505 | `	if( VmClassStaticDeferPending(pClass) ){` |
|      - |  506 | `		/* Instantiation materializes the static table (OP_NEW does it too), so a` |
|      - |  507 | `		 * broken default raises BEFORE any object exists. */` |
|    ! 0 |  508 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pVm, pClass);` |
|    ! 0 |  509 | `		if( rcMat != SXRET_OK ){` |
|    ! 0 |  510 | `			return rcMat;` |
|      - |  511 | `		}` |
|    ! 0 |  512 | `	}` |
|     42 |  513 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|     42 |  514 | `	if( pThis == 0 ){` |
|    ! 0 |  515 | `		ph7_result_null(pCtx);` |
|    ! 0 |  516 | `		return PH7_OK;` |
|      - |  517 | `	}` |
|     42 |  518 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|     42 |  519 | `	if( pCons ){` |
|      - |  520 | `		SySet aArg;` |
|      - |  521 | `		sxi32 rc;` |
|     38 |  522 | `		SyString *aNames = 0;` |
|     38 |  523 | `		SySetInit(&aArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|     38 |  524 | `		if( pArgs ){` |
|     38 |  525 | `			ReflectCollectArgs(pCtx, pArgs, &aArg, &aNames);` |
|     18 |  526 | `		}` |
|     38 |  527 | `		if( aNames ){` |
|      - |  528 | `			VmCallArgMap sMap;` |
|      5 |  529 | `			SyZero(&sMap,sizeof(sMap)); /* new map fields must read unset, not stack garbage */` |
|      5 |  530 | `			sMap.bHasNamed = 1;` |
|      5 |  531 | `			sMap.bIsNamespaced = 0;` |
|      5 |  532 | `			sMap.bStrict = 0;` |
|      5 |  533 | `			sMap.nTotal = SySetUsed(&aArg);` |
|      5 |  534 | `			sMap.aNames = aNames;` |
|      7 |  535 | `			rc = PH7_VmCallClassMethodMap(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|      4 |  536 | `				(ph7_value **)SySetBasePtr(&aArg), &sMap);` |
|      5 |  537 | `			SyMemBackendFree(&pVm->sAllocator, aNames);` |
|      3 |  538 | `		}else{` |
|     50 |  539 | `			rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|     32 |  540 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|      - |  541 | `		}` |
|     38 |  542 | `		SySetRelease(&aArg);` |
|     38 |  543 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|    ! 0 |  544 | `			PH7_ClassInstanceUnref(pThis);` |
|    ! 0 |  545 | `			return rc;` |
|      - |  546 | `		}` |
|     18 |  547 | `	}` |
|     42 |  548 | `	return ReflectResultObject(pCtx, pThis);` |
|     22 |  549 | `}` |
|      - |  550 | `/* Where __reflect_new_no_ctor() was: the one caller was chunk 7's` |
|      - |  551 | ` * __reflect_build_attrs, which built a ReflectionAttribute and then filled it` |
|      - |  552 | ` * through a public __init() php does not have. C builds the instance itself` |
|      - |  553 | ` * (ReflectAttrNew), so both are gone. */` |
|      - |  554 | `/*` |
|      - |  555 | ` * Typed/readonly store enforcement for reflection writes. Like the VM's` |
|      - |  556 | ` * store path, except an UNINITIALIZED readonly property may be written from` |
|      - |  557 | ` * any scope (PHP lets ReflectionProperty::setValue initialize readonly): the` |
|      - |  558 | ` * READONLY bit is masked off for the enforcement call so the set-scope check` |
|      - |  559 | ` * is skipped, while an already-initialized readonly still gets PHP's` |
|      - |  560 | ` * "Cannot modify readonly property" Error. Returns SXRET_OK/PH7_EXCEPTION/` |
|      - |  561 | ` * PH7_ABORT; the value may be coerced in place.` |
|      - |  562 | ` */` |
|     14 |  563 | `static sxi32 ReflectEnforceStore(ph7_context *pCtx, sxu32 nIdx, ph7_value *pValue)` |
|      1 |  564 | `{` |
|     15 |  565 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  566 | `	SyHashEntry *pSlot;` |
|      - |  567 | `	VmClassAttr *pVmAttr;` |
|      - |  568 | `	ph7_class_attr *pAttr;` |
|      - |  569 | `	sxi32 iSaved, rc;` |
|     15 |  570 | `	pSlot = SyHashGet(&pVm->hTypedSlot, (const void *)&nIdx, sizeof(sxu32));` |
|     15 |  571 | `	if( pSlot == 0 ){` |
|      7 |  572 | `		return SXRET_OK; /* Untyped slot: plain store */` |
|      - |  573 | `	}` |
|      9 |  574 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      9 |  575 | `	pAttr = pVmAttr->pAttr;` |
|      9 |  576 | `	if( pAttr == 0 ){` |
|    ! 0 |  577 | `		return SXRET_OK;` |
|      - |  578 | `	}` |
|      9 |  579 | `	iSaved = pAttr->iFlags;` |
|      9 |  580 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) ){` |
|    ! 0 |  581 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_READONLY;` |
|    ! 0 |  582 | `	}` |
|      9 |  583 | `	rc = PH7_VmEnforcePropStore(pVm, nIdx, pValue);` |
|      9 |  584 | `	pAttr->iFlags = iSaved;` |
|      9 |  585 | `	return rc;` |
|      8 |  586 | `}` |
|      - |  587 | `/* Hand an EXISTING instance to the caller: takes an extra reference` |
|      - |  588 | ` * (unlike ReflectResultObject, which transfers a fresh instance's one). */` |
|      2 |  589 | `static int ReflectResultExistingObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|      1 |  590 | `{` |
|      3 |  591 | `	if( pObj == 0 ){` |
|    ! 0 |  592 | `		ph7_result_null(pCtx);` |
|    ! 0 |  593 | `		return PH7_OK;` |
|      - |  594 | `	}` |
|      3 |  595 | `	PH7_MemObjRelease(pCtx->pRet);` |
|      3 |  596 | `	pObj->iRef++;` |
|      3 |  597 | `	pCtx->pRet->x.pOther = pObj;` |
|      3 |  598 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|      3 |  599 | `	return PH7_OK;` |
|      2 |  600 | `}` |
|      - |  601 | `/* pVal is a Closure instance? Return it, else NULL. */` |
|   2888 |  602 | `static ph7_class_instance * ReflectValueClosure(ph7_vm *pVm, ph7_value *pVal)` |
|      4 |  603 | `{` |
|      - |  604 | `	ph7_class_instance *pThis;` |
|   2892 |  605 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
|   2653 |  606 | `		return 0;` |
|      - |  607 | `	}` |
|    242 |  608 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    242 |  609 | `	return (pThis->pClass == pVm->pClosureClass) ? pThis : 0;` |
|   1448 |  610 | `}` |
|      - |  611 | `/*` |
|      - |  612 | ` * Resolve a reflection callable target into its compiled function.` |
|      - |  613 | ` *   - pMethodArg a non-empty string  -> method mode: pTarget is a class name` |
|      - |  614 | ` *     or object; outputs *ppClass and *ppMeth.` |
|      - |  615 | ` *   - pTarget a Closure              -> unwrap $__fn into hFunction; *ppClosure.` |
|      - |  616 | ` *   - pTarget a string               -> hFunction (user) or hHostFunction` |
|      - |  617 | ` *     (*ppHost set, returns NULL).` |
|      - |  618 | ` * Returns the ph7_vm_func, or NULL (host function or unresolvable).` |
|      - |  619 | ` */` |
|   4728 |  620 | `static ph7_vm_func * ReflectResolveCallable(ph7_vm *pVm, ph7_value *pTarget,` |
|      - |  621 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|      - |  622 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|      5 |  623 | `{` |
|      - |  624 | `	SyHashEntry *pEntry;` |
|   4733 |  625 | `	if( ppClass ){ *ppClass = 0; }` |
|   4733 |  626 | `	if( ppMeth ){ *ppMeth = 0; }` |
|   4733 |  627 | `	if( ppHost ){ *ppHost = 0; }` |
|   4733 |  628 | `	if( ppClosure ){ *ppClosure = 0; }` |
|   4733 |  629 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
|   2277 |  630 | `		ph7_class *pClass = ReflectResolveClass(pVm, pTarget);` |
|      - |  631 | `		ph7_class_method *pMeth;` |
|   2277 |  632 | `		if( pClass == 0 ){` |
|    ! 0 |  633 | `			return 0;` |
|      - |  634 | `		}` |
|   3413 |  635 | `		pMeth = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|   1136 |  636 | `			SyBlobLength(&pMethodArg->sBlob));` |
|   2277 |  637 | `		if( pMeth == 0 ){` |
|      - |  638 | `			/* getMethods()/ReflectionClass reports a base class's PRIVATE methods on` |
|      - |  639 | `			 * the subclass (php copies them into the child's table), but private` |
|      - |  640 | `			 * methods are not inherited into the child's method table, so the plain` |
|      - |  641 | `			 * extract misses them when a ReflectionMethod obtained from the subclass` |
|      - |  642 | `			 * is re-resolved by (subclass, name). Walk the base chain to find the` |
|      - |  643 | `			 * declaring class's own copy. */` |
|    ! 0 |  644 | `			ph7_class *pWalk = pClass->pBase;` |
|    ! 0 |  645 | `			while( pWalk && pMeth == 0 ){` |
|    ! 0 |  646 | `				pMeth = PH7_ClassExtractMethod(pWalk, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|    ! 0 |  647 | `					SyBlobLength(&pMethodArg->sBlob));` |
|    ! 0 |  648 | `				pWalk = pWalk->pBase;` |
|    ! 0 |  649 | `			}` |
|    ! 0 |  650 | `		}` |
|   2277 |  651 | `		if( pMeth == 0 ){` |
|    ! 0 |  652 | `			return 0;` |
|      - |  653 | `		}` |
|   2277 |  654 | `		if( ppClass ){ *ppClass = pClass; }` |
|   2277 |  655 | `		if( ppMeth ){ *ppMeth = pMeth; }` |
|   2277 |  656 | `		return &pMeth->sFunc;` |
|      - |  657 | `	}` |
|      - |  658 | `	{` |
|   2460 |  659 | `		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|   2460 |  660 | `		if( pClo ){` |
|      - |  661 | `			SyString sAttr;` |
|      - |  662 | `			ph7_value *pFn;` |
|    180 |  663 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    180 |  664 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|    180 |  665 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) < 1 ){` |
|    ! 0 |  666 | `				return 0;` |
|      - |  667 | `			}` |
|      - |  668 | `			/* A closure over an object method or __invoke object` |
|      - |  669 | `			 * (Closure::fromCallable([$obj,'m']) / fromCallable($invokeObj)) stores the` |
|      - |  670 | `			 * bare method name in $__fn and the class in $__scope. Resolve it as a METHOD` |
|      - |  671 | `			 * of $__scope FIRST — before the global function table — so a same-named` |
|      - |  672 | `			 * global function does not shadow the method (the bug: $__fn "add" hitting a` |
|      - |  673 | `			 * global add()). A plain anonymous closure's $__fn is its unique lambda name,` |
|      - |  674 | `			 * which is not a method, so this falls through cleanly. */` |
|      - |  675 | `			{` |
|      - |  676 | `				SyString sScope;` |
|      - |  677 | `				ph7_value *pScope;` |
|    180 |  678 | `				SyStringInitFromBuf(&sScope, "__scope", 7);` |
|    180 |  679 | `				pScope = PH7_ClassInstanceFetchAttr(pClo, &sScope);` |
|    180 |  680 | `				if( pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0 ){` |
|    119 |  681 | `					ph7_class *pScopeCls = PH7_VmExtractClass(pVm,` |
|     78 |  682 | `						(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|     80 |  683 | `					if( pScopeCls ){` |
|    119 |  684 | `						ph7_class_method *pScopeMeth = PH7_ClassExtractMethod(pScopeCls,` |
|     78 |  685 | `							(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|     80 |  686 | `						if( pScopeMeth ){` |
|     66 |  687 | `							if( ppClass ){ *ppClass = pScopeCls; }` |
|     66 |  688 | `							if( ppMeth ){ *ppMeth = pScopeMeth; }` |
|     66 |  689 | `							if( ppClosure ){ *ppClosure = pClo; }` |
|     66 |  690 | `							return &pScopeMeth->sFunc;` |
|      - |  691 | `						}` |
|      7 |  692 | `					}` |
|      7 |  693 | `				}` |
|      - |  694 | `			}` |
|    116 |  695 | `			pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    116 |  696 | `			if( pEntry == 0 ){` |
|      - |  697 | `				/* A Closure over a host function (Closure::fromCallable('strlen')) */` |
|      3 |  698 | `				pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|      3 |  699 | `				if( pEntry && ppHost ){` |
|      3 |  700 | `					*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|      3 |  701 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|      1 |  702 | `				}` |
|      3 |  703 | `				return 0;` |
|      - |  704 | `			}` |
|    114 |  705 | `			if( ppClosure ){ *ppClosure = pClo; }` |
|    114 |  706 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|      - |  707 | `		}` |
|      - |  708 | `	}` |
|   2283 |  709 | `	if( pTarget->iFlags & MEMOBJ_STRING ){` |
|   2283 |  710 | `		if( SyBlobLength(&pTarget->sBlob) < 1 ){` |
|    ! 0 |  711 | `			return 0;` |
|      - |  712 | `		}` |
|   2283 |  713 | `		pEntry = PH7_VmGetUserFunction(pVm, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob), FALSE);` |
|   2283 |  714 | `		if( pEntry ){` |
|    800 |  715 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|      - |  716 | `		}` |
|   1485 |  717 | `		pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   1485 |  718 | `		if( pEntry && ppHost ){` |
|   1470 |  719 | `			*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|    734 |  720 | `		}` |
|    741 |  721 | `	}` |
|   1485 |  722 | `	return 0;` |
|   2369 |  723 | `}` |
|      - |  724 | `/*` |
|      - |  725 | ` * ---------------------------------------------------------------------------` |
|      - |  726 | ` * The signature parser.` |
|      - |  727 | ` *` |
|      - |  728 | ` * A C builtin and a native method declare their parameters as ONE php-style` |
|      - |  729 | `` * string (`"string $name, ?int $len = null"`) — the aBuiltinSig[] row or the`` |
|      - |  730 | ` * PH7_NativeClassSpec's zSig — because neither has a compiled parameter list to` |
|      - |  731 | ` * read. Reflection needs that string as the same param-meta shape a compiled` |
|      - |  732 | ` * function produces, so it is parsed here and the descriptor comes out of` |
|      - |  733 | ` * __reflect_func_info() already uniform.` |
|      - |  734 | ` *` |
|      - |  735 | ` * This was chunk 8 of the reflection prelude (__reflect_sig_split /` |
|      - |  736 | ` * __reflect_sig_scalar / __reflect_parse_sig / __reflect_sig_fixup), which every` |
|      - |  737 | ` * caller had to remember to wrap around __reflect_func_info() — six call sites,` |
|      - |  738 | ` * and the one that FORGOT is why every native method reported zero parameters` |
|      - |  739 | ` * until 31 Jul. Producing the parsed form at the source removes the wrapper and` |
|      - |  740 | ` * the possibility of forgetting it.` |
|      - |  741 | ` * ---------------------------------------------------------------------------` |
|      - |  742 | ` */` |
|      - |  743 | `/* Trim ASCII spaces off both ends of [z, z+n). */` |
|   8658 |  744 | `static void ReflectSigTrim(const char **pz, int *pn)` |
|      4 |  745 | `{` |
|   8662 |  746 | `	const char *z = *pz;` |
|   8662 |  747 | `	int n = *pn;` |
|  18001 |  748 | `	while( n > 0 && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|   5011 |  749 | `		z++;` |
|   5011 |  750 | `		n--;` |
|      1 |  751 | `	}` |
|  13723 |  752 | `	while( n > 0 && (z[n-1] == ' ' \|\| z[n-1] == '\t') ){` |
|    733 |  753 | `		n--;` |
|      1 |  754 | `	}` |
|   8662 |  755 | `	*pz = z;` |
|   8662 |  756 | `	*pn = n;` |
|   8662 |  757 | `}` |
|      - |  758 | ``/* Byte search that respects single-quoted runs (a default may be `'a,b'` or`` |
|      - |  759 | `` * `'it\'s'`), which is the whole reason the split is not SyByteFind. Answers`` |
|      - |  760 | ` * the offset of the first unquoted zWhat, or -1. */` |
|  15160 |  761 | `static int ReflectSigFindUnquoted(const char *z, int n, char cWhat)` |
|      5 |  762 | `{` |
|  15165 |  763 | `	int k, bQuote = 0;` |
| 226207 |  764 | `	for( k = 0 ; k < n ; k++ ){` |
| 219855 |  765 | `		if( bQuote ){` |
|   1553 |  766 | `			if( z[k] == '\\' && k + 1 < n ){` |
|     17 |  767 | `				k++;` |
|   1545 |  768 | `			}else if( z[k] == '\'' ){` |
|    441 |  769 | `				bQuote = 0;` |
|    221 |  770 | `			}` |
| 219079 |  771 | `		}else if( z[k] == '\'' ){` |
|    441 |  772 | `			bQuote = 1;` |
| 218083 |  773 | `		}else if( z[k] == cWhat ){` |
|   8813 |  774 | `			return k;` |
|      - |  775 | `		}` |
| 105526 |  776 | `	}` |
|   6357 |  777 | `	return -1;` |
|   7585 |  778 | `}` |
|      - |  779 | `/* Does [z,n) contain zNeedle? (case-sensitive; nNeedle > 0) */` |
|   3852 |  780 | `static int ReflectSigHas(const char *z, int n, const char *zNeedle, int nNeedle)` |
|      4 |  781 | `{` |
|      - |  782 | `	int k;` |
|  59584 |  783 | `	for( k = 0 ; k + nNeedle <= n ; k++ ){` |
|  55788 |  784 | `		if( SyMemcmp((const void *)&z[k], (const void *)zNeedle, (sxu32)nNeedle) == 0 ){` |
|     57 |  785 | `			return 1;` |
|      - |  786 | `		}` |
|  27868 |  787 | `	}` |
|   3800 |  788 | `	return 0;` |
|   1930 |  789 | `}` |
|      - |  790 | ``/* Case-insensitive twin, for the `null` arm of a union type text. */`` |
|   1624 |  791 | `static int ReflectSigHasNoCase(const char *z, int n, const char *zNeedle, int nNeedle)` |
|      5 |  792 | `{` |
|      - |  793 | `	int k, j;` |
|   8417 |  794 | `	for( k = 0 ; k + nNeedle <= n ; k++ ){` |
|   7215 |  795 | `		for( j = 0 ; j < nNeedle ; j++ ){` |
|   7211 |  796 | `			if( SyToLower(z[k+j]) != SyToLower(zNeedle[j]) ){` |
|   6793 |  797 | `				break;` |
|      - |  798 | `			}` |
|    211 |  799 | `		}` |
|   6797 |  800 | `		if( j == nNeedle ){` |
|      6 |  801 | `			return 1;` |
|      - |  802 | `		}` |
|   3399 |  803 | `	}` |
|   1625 |  804 | `	return 0;` |
|    817 |  805 | `}` |
|      - |  806 | `/*` |
|      - |  807 | `` * One `C::K` (or `C::class`) term of a declared default, evaluated.`` |
|      - |  808 | ` *` |
|      - |  809 | `` * php's stub writes these as SOURCE — `string $class = SplFileInfo::class`,`` |
|      - |  810 | `` * `int $flags = FilesystemIterator::KEY_AS_PATHNAME\|…` — and the reflector then`` |
|      - |  811 | ` * answers both the text (the export line) and the VALUE (getDefaultValue()). A` |
|      - |  812 | ` * native method's zSig is that same source, so the value has to be read out of` |
|      - |  813 | ` * the class here; there are no compiled parameter records to hold it.` |
|      - |  814 | ` */` |
|      8 |  815 | `static int ReflectSigClassConst(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      1 |  816 | `{` |
|      9 |  817 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  818 | `	ph7_class_attr *pAttr;` |
|      - |  819 | `	ph7_class *pClass;` |
|      - |  820 | `	ph7_value *pValue;` |
|      - |  821 | `	int iSep;` |
|      9 |  822 | `	ReflectSigTrim(&z, &n);` |
|     75 |  823 | `	for( iSep = 0 ; iSep + 1 < n ; ++iSep ){` |
|     75 |  824 | `		if( z[iSep] == ':' && z[iSep+1] == ':' ){` |
|      9 |  825 | `			break;` |
|      - |  826 | `		}` |
|     34 |  827 | `	}` |
|      9 |  828 | `	if( iSep + 1 >= n \|\| iSep < 1 ){` |
|    ! 0 |  829 | `		return 0;` |
|      - |  830 | `	}` |
|      9 |  831 | `	pClass = PH7_VmExtractClass(pVm, z, (sxu32)iSep, TRUE, 0);` |
|      9 |  832 | `	if( pClass == 0 ){` |
|    ! 0 |  833 | `		return 0;` |
|      - |  834 | `	}` |
|      9 |  835 | `	z += iSep + 2;` |
|      9 |  836 | `	n -= iSep + 2;` |
|      9 |  837 | `	if( n == (int)sizeof("class")-1 && SyMemcmp(z, "class", sizeof("class")-1) == 0 ){` |
|      - |  838 | ``		/* `C::class` is the class NAME, and php prints the name it was DECLARED`` |
|      - |  839 | `		 * with rather than the spelling in the signature. */` |
|      3 |  840 | `		SyString *pName = &pClass->sName;` |
|      3 |  841 | `		ph7_value_string(pOut, SyStringData(pName), (int)SyStringLength(pName));` |
|      3 |  842 | `		return 1;` |
|      - |  843 | `	}` |
|      7 |  844 | `	pAttr = PH7_ClassExtractConstant(pClass, z, (sxu32)n);` |
|      7 |  845 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|    ! 0 |  846 | `		return 0;` |
|      - |  847 | `	}` |
|      7 |  848 | `	if( pAttr->nIdx == SXU32_HIGH ){` |
|      - |  849 | `` 		/* Not materialized yet: run the initializer, exactly as a direct `C::K` `` |
|      - |  850 | `		 * read would (an unread native constant is a literal waiting on this). */` |
|      5 |  851 | `		if( VmClassConstEvalOnDemand(pVm, pClass, pAttr) != SXRET_OK ){` |
|    ! 0 |  852 | `			return 0;` |
|      - |  853 | `		}` |
|      2 |  854 | `	}` |
|      7 |  855 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj, pAttr->nIdx);` |
|      7 |  856 | `	if( pValue == 0 ){` |
|    ! 0 |  857 | `		return 0;` |
|      - |  858 | `	}` |
|      7 |  859 | `	PH7_MemObjStore(pValue, pOut);` |
|      7 |  860 | `	return 1;` |
|      5 |  861 | `}` |
|      - |  862 | `/*` |
|      - |  863 | ` * A GLOBAL constant named by a declared default — php's stub writes` |
|      - |  864 | `` * `int $severity = E_ERROR` and both surfaces read it from here: the export`` |
|      - |  865 | ` * prints the NAME (rule 28: the argument was never folded, so php still has the` |
|      - |  866 | ` * source) and getDefaultValue() answers what it expands to. Answers 0 for` |
|      - |  867 | ` * anything that is not a plain identifier naming a defined constant, which is` |
|      - |  868 | `` * what keeps `null`/`true`/a bare number out of this branch.`` |
|      - |  869 | ` */` |
|     36 |  870 | `static int ReflectSigIsIdent(const char *z, int n)` |
|      1 |  871 | `{` |
|      - |  872 | `	int k;` |
|     37 |  873 | `	if( n < 1 \|\| (z[0] != '_' && !SyisAlpha(z[0])) ){` |
|     31 |  874 | `		return 0;` |
|      - |  875 | `	}` |
|     31 |  876 | `	for( k = 1 ; k < n ; ++k ){` |
|     25 |  877 | `		if( z[k] != '_' && z[k] != '\\' && !SyisAlphaNum(z[k]) ){` |
|    ! 0 |  878 | `			return 0;` |
|      - |  879 | `		}` |
|     13 |  880 | `	}` |
|      7 |  881 | `	return 1;` |
|     19 |  882 | `}` |
|     36 |  883 | `static int ReflectSigGlobalConst(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      1 |  884 | `{` |
|      - |  885 | `	SyHashEntry *pEntry;` |
|      - |  886 | `	ph7_constant *pCons;` |
|     37 |  887 | `	ReflectSigTrim(&z, &n);` |
|     37 |  888 | `	if( !ReflectSigIsIdent(z, n) ){` |
|     31 |  889 | `		return 0;` |
|      - |  890 | `	}` |
|      7 |  891 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant, (const void *)z, (sxu32)n);` |
|      7 |  892 | `	if( pEntry == 0 ){` |
|      5 |  893 | `		return 0;` |
|      - |  894 | `	}` |
|      3 |  895 | `	pCons = (ph7_constant *)pEntry->pUserData;` |
|      3 |  896 | `	if( pCons == 0 \|\| pCons->xExpand == 0 ){` |
|    ! 0 |  897 | `		return 0;` |
|      - |  898 | `	}` |
|      3 |  899 | `	pCons->xExpand(pOut, pCons->pUserData);` |
|      3 |  900 | `	return 1;` |
|     19 |  901 | `}` |
|      - |  902 | `/*` |
|      - |  903 | `` * A class-constant EXPRESSION: one term, or the `\|` fold php's own stubs write`` |
|      - |  904 | `` * for a flags default (`KEY_AS_PATHNAME \| CURRENT_AS_FILEINFO \| SKIP_DOTS`).`` |
|      - |  905 | ` */` |
|     30 |  906 | `static int ReflectSigConstExpr(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      1 |  907 | `{` |
|     31 |  908 | `	sxi64 iAcc = 0;` |
|     31 |  909 | `	int iStart = 0;` |
|     31 |  910 | `	int k, nTerm = 0;` |
|     31 |  911 | `	if( !ReflectSigHas(z, n, "::", 2) ){` |
|     23 |  912 | `		return 0;` |
|      - |  913 | `	}` |
|    173 |  914 | `	for( k = 0 ; k <= n ; ++k ){` |
|    165 |  915 | `		if( k < n && z[k] != '\|' ){` |
|    157 |  916 | `			continue;` |
|      - |  917 | `		}` |
|      9 |  918 | `		if( !ReflectSigClassConst(pCtx, &z[iStart], k - iStart, pOut) ){` |
|    ! 0 |  919 | `			return 0;` |
|      - |  920 | `		}` |
|      9 |  921 | `		nTerm++;` |
|      9 |  922 | `		if( k < n \|\| nTerm > 1 ){` |
|      - |  923 | `			/* A fold is arithmetic, so every arm has to be an integer; a lone` |
|      - |  924 | ``			 * term keeps whatever type it had (`C::class` is a string). */`` |
|    ! 0 |  925 | `			if( (pOut->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) != 0 ){` |
|    ! 0 |  926 | `				return 0;` |
|      - |  927 | `			}` |
|    ! 0 |  928 | `			PH7_MemObjToInteger(pOut);` |
|    ! 0 |  929 | `			iAcc \|= pOut->x.iVal;` |
|    ! 0 |  930 | `		}` |
|      9 |  931 | `		iStart = k + 1;` |
|      5 |  932 | `	}` |
|      9 |  933 | `	if( nTerm > 1 ){` |
|    ! 0 |  934 | `		ph7_value_int64(pOut, iAcc);` |
|    ! 0 |  935 | `	}` |
|      9 |  936 | `	return nTerm > 0;` |
|     16 |  937 | `}` |
|      - |  938 | `/*` |
|      - |  939 | ` * A default-value TEXT to a value, when the text denotes a scalar php can` |
|      - |  940 | `` * reproduce. Answers 1 and fills pOut, or 0 for anything else (`[]`,`` |
|      - |  941 | `` * `array (`, a constant name) which the caller reports its own way.`` |
|      - |  942 | ` */` |
|    144 |  943 | `static int ReflectSigScalar(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      1 |  944 | `{` |
|    145 |  945 | `	sxu8 bReal = 0;` |
|    145 |  946 | `	if( n == 1 && z[0] == '?' ){` |
|    ! 0 |  947 | `		return 0;` |
|      - |  948 | `	}` |
|    145 |  949 | `	if( (n == 4 && (SyMemcmp(z,"NULL",4) == 0 \|\| SyMemcmp(z,"null",4) == 0)) ){` |
|     77 |  950 | `		ph7_value_null(pOut);` |
|     77 |  951 | `		return 1;` |
|      - |  952 | `	}` |
|     69 |  953 | `	if( n == 4 && SyMemcmp(z,"true",4) == 0 ){` |
|      5 |  954 | `		ph7_value_bool(pOut,1);` |
|      5 |  955 | `		return 1;` |
|      - |  956 | `	}` |
|     65 |  957 | `	if( n == 5 && SyMemcmp(z,"false",5) == 0 ){` |
|      5 |  958 | `		ph7_value_bool(pOut,0);` |
|      5 |  959 | `		return 1;` |
|      - |  960 | `	}` |
|     61 |  961 | `	if( n >= 2 && (z[0] == '\'' \|\| z[0] == '"') && z[n-1] == z[0] ){` |
|      - |  962 | `		/* Unescape the quote character and \\ , the only two escapes the signature` |
|      - |  963 | `		 * writer emits. BOTH quote spellings are accepted because both scanners in` |
|      - |  964 | `		 * vm_arg_check.c step over either one: a native method's zSig lives in C, so` |
|      - |  965 | ``		 * `= \"static\"` is the natural way to write Closure::bindTo's default and it`` |
|      - |  966 | ``		 * used to reduce to php's `<default>` placeholder here. */`` |
|      - |  967 | `		SyBlob sOut;` |
|     31 |  968 | `		char cQuote = z[0];` |
|      - |  969 | `		int k;` |
|     31 |  970 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    111 |  971 | `		for( k = 1 ; k < n - 1 ; k++ ){` |
|     81 |  972 | `			if( z[k] == '\\' && k + 1 < n - 1 && (z[k+1] == cQuote \|\| z[k+1] == '\\') ){` |
|      3 |  973 | `				k++;` |
|      1 |  974 | `			}` |
|     81 |  975 | `			SyBlobAppend(&sOut,(const void *)&z[k],sizeof(char));` |
|     41 |  976 | `		}` |
|     31 |  977 | `		ph7_value_string(pOut,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     31 |  978 | `		SyBlobRelease(&sOut);` |
|     31 |  979 | `		return 1;` |
|      - |  980 | `	}` |
|     31 |  981 | `	if( ReflectSigConstExpr(pCtx,z,n,pOut) ){` |
|      9 |  982 | `		return 1;` |
|      - |  983 | `	}` |
|     23 |  984 | `	if( ReflectSigGlobalConst(pCtx,z,n,pOut) ){` |
|      3 |  985 | `		return 1;` |
|      - |  986 | `	}` |
|     21 |  987 | `	if( n > 0 && SyStrIsNumeric(z,(sxu32)n,&bReal,0) == SXRET_OK ){` |
|      - |  988 | `		/* php's own rule for the text form: a '.', an exponent or a hex marker` |
|      - |  989 | `		 * makes it a float, everything else an int. */` |
|     18 |  990 | `		if( bReal \|\| ReflectSigHas(z,n,".",1)` |
|     19 |  991 | `		 \|\| ReflectSigHasNoCase(z,n,"e",1) \|\| ReflectSigHasNoCase(z,n,"x",1) ){` |
|      - |  992 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|    ! 0 |  993 | `			ph7_value_double(pOut,SyStrToReal(z,(sxu32)n,0,0));` |
|      - |  994 | `#else` |
|      - |  995 | `			ph7_value_int64(pOut,SyStrToInt64(z,(sxu32)n,0,0));` |
|      - |  996 | `#endif` |
|    ! 0 |  997 | `		}else{` |
|     19 |  998 | `			sxi64 iVal = 0;` |
|     19 |  999 | `			SyStrToInt64(z,(sxu32)n,(void *)&iVal,0);` |
|     19 | 1000 | `			ph7_value_int64(pOut,iVal);` |
|      - | 1001 | `		}` |
|     19 | 1002 | `		return 1;` |
|      - | 1003 | `	}` |
|      3 | 1004 | `	return 0;` |
|     73 | 1005 | `}` |
|      - | 1006 | `/*` |
|      - | 1007 | ` * One parameter, described uniformly.` |
|      - | 1008 | ` *` |
|      - | 1009 | ` * A reflected function's parameters come from one of TWO places — a compiled` |
|      - | 1010 | `` * function's `ph7_vm_func_arg` list, or the php-style SIGNATURE STRING a C`` |
|      - | 1011 | ` * builtin / native method declares — and both the descriptor array` |
|      - | 1012 | ` * (__reflect_func_info, for the chunks still in PHP) and the native` |
|      - | 1013 | ` * ReflectionParameter have to read them the same way. This struct is what they` |
|      - | 1014 | `` * agree on; `pArg` is set only on the compiled path, where a default is`` |
|      - | 1015 | ` * BYTE-CODE rather than text.` |
|      - | 1016 | ` */` |
|      - | 1017 | `typedef struct ReflectParamDesc ReflectParamDesc;` |
|      - | 1018 | `struct ReflectParamDesc` |
|      - | 1019 | `{` |
|      - | 1020 | `	SyString sName;` |
|      - | 1021 | `	int iPos;` |
|      - | 1022 | `	int bByRef, bVariadic, bHasDef, bOptional, bNullable, bPromoted;` |
|      - | 1023 | `	SyString sType;            /* nByte == 0 -> untyped */` |
|      - | 1024 | `	SyString sDefText;         /* signature-declared default TEXT (nByte == 0 -> none) */` |
|      - | 1025 | `	ph7_vm_func_arg *pArg;     /* compiled parameter, or NULL for a declared one */` |
|      - | 1026 | `};` |
|      - | 1027 | `/*` |
|      - | 1028 | ` * Split a signature string on its top-level commas (a quoted default may hold` |
|      - | 1029 | ` * its own). Answers the parameter COUNT; when iWant is in range, hands back` |
|      - | 1030 | ` * that part's bytes.` |
|      - | 1031 | ` */` |
|   2984 | 1032 | `static int ReflectSigPart(const char *zSig, int nSig, int iWant,` |
|      - | 1033 | `	const char **pzPart, int *pnPart)` |
|      4 | 1034 | `{` |
|   2988 | 1035 | `	int iPos = 0;` |
|   7266 | 1036 | `	while( nSig > 0 ){` |
|   7154 | 1037 | `		int iComma = ReflectSigFindUnquoted(zSig,nSig,',');` |
|   7154 | 1038 | `		const char *zPart = zSig;` |
|   7154 | 1039 | `		int nPart = iComma < 0 ? nSig : iComma;` |
|   7154 | 1040 | `		ReflectSigTrim(&zPart,&nPart);` |
|   7154 | 1041 | `		if( nPart > 0 ){` |
|   7154 | 1042 | `			if( iPos == iWant && pzPart ){` |
|   1898 | 1043 | `				*pzPart = zPart;` |
|   1898 | 1044 | `				*pnPart = nPart;` |
|    947 | 1045 | `			}` |
|   7154 | 1046 | `			iPos++;` |
|   3575 | 1047 | `		}` |
|   7154 | 1048 | `		if( iComma < 0 ){` |
|   2876 | 1049 | `			break;` |
|      - | 1050 | `		}` |
|   4279 | 1051 | `		zSig += iComma + 1;` |
|   4279 | 1052 | `		nSig -= iComma + 1;` |
|      1 | 1053 | `	}` |
|   2988 | 1054 | `	return iPos;` |
|      4 | 1055 | `}` |
|      - | 1056 | ``/* One `type &$name = default` part into the uniform description. */`` |
|   1894 | 1057 | `static void ReflectSigDescribe(const char *z, int n, int iPos, ReflectParamDesc *pOut)` |
|      4 | 1058 | `{` |
|   1898 | 1059 | `	const char *zDef = 0;` |
|   1898 | 1060 | `	int nDef = 0;` |
|      - | 1061 | `	int iEq, iDollar, iSpace;` |
|   1898 | 1062 | `	SyZero(pOut,sizeof(*pOut));` |
|   1898 | 1063 | `	pOut->iPos = iPos;` |
|   1898 | 1064 | `	if( n > 0 && z[0] == '~' ){` |
|      - | 1065 | `		/* The signature table's "declared here, screened by the builtin" marker` |
|      - | 1066 | `		 * (see vm_arg_check.c): php DECLARES this type and its C body asks for a` |
|      - | 1067 | `		 * tighter one, so Reflection reports what follows the marker. */` |
|    107 | 1068 | `		z++;` |
|    107 | 1069 | `		n--;` |
|     53 | 1070 | `	}` |
|      - | 1071 | ``	/* `= default` splits off first: everything after the first unquoted '='. */`` |
|   1898 | 1072 | `	iEq = ReflectSigFindUnquoted(z,n,'=');` |
|   1898 | 1073 | `	if( iEq >= 0 ){` |
|    733 | 1074 | `		zDef = &z[iEq+1];` |
|    733 | 1075 | `		nDef = n - iEq - 1;` |
|    733 | 1076 | `		ReflectSigTrim(&zDef,&nDef);` |
|    733 | 1077 | `		n = iEq;` |
|    733 | 1078 | `		ReflectSigTrim(&z,&n);` |
|    366 | 1079 | `	}` |
|   1898 | 1080 | `	if( zDef && nDef == 1 && zDef[0] == '?' ){` |
|      - | 1081 | ``		/* `= ?` is the table's OPTIONAL-but-no-default marker, php's own shape`` |
|      - | 1082 | `		 * for a parameter like ReflectionClass::getStaticPropertyValue()'s` |
|      - | 1083 | `		 * $default: isOptional() true, isDefaultValueAvailable() FALSE, so` |
|      - | 1084 | `		 * getDefaultValue() raises. Reporting it as a default (which is what` |
|      - | 1085 | ``		 * a bare `hasdef` did) made that call answer NULL instead. */`` |
|     51 | 1086 | `		pOut->bOptional = 1;` |
|     51 | 1087 | `		zDef = 0;` |
|     51 | 1088 | `		nDef = 0;` |
|     25 | 1089 | `	}` |
|   1898 | 1090 | `	pOut->bVariadic = ReflectSigHas(z,n,"...",3);` |
|   1898 | 1091 | `	if( pOut->bVariadic ){` |
|      - | 1092 | `		/* php: a variadic parameter never HAS a default -- it defaults to "no` |
|      - | 1093 | `		 * further arguments", which is not a value. Several signature rows still` |
|      - | 1094 | ``		 * write `mixed ...$values = ?` (the table's "optional, unspecified"`` |
|      - | 1095 | `		 * marker), and honouring it made isDefaultValueAvailable() true where php` |
|      - | 1096 | `		 * says false, so getDefaultValue() then threw on printf/array_merge. */` |
|     33 | 1097 | `		zDef = 0;` |
|     33 | 1098 | `		nDef = 0;` |
|     16 | 1099 | `	}` |
|   1898 | 1100 | `	iDollar = ReflectSigFindUnquoted(z,n,'$');` |
|   1898 | 1101 | `	iSpace = ReflectSigFindUnquoted(z,n,' ');` |
|      - | 1102 | `	{` |
|   1898 | 1103 | `		const char *zName = iDollar < 0 ? z : &z[iDollar+1];` |
|   1898 | 1104 | `		int nName = iDollar < 0 ? n : n - iDollar - 1;` |
|   1898 | 1105 | `		SyStringInitFromBuf(&pOut->sName,zName,nName);` |
|      - | 1106 | `	}` |
|   1898 | 1107 | `	pOut->bByRef = ReflectSigHas(z,n,"&",1);` |
|   1898 | 1108 | `	pOut->bHasDef = zDef != 0;` |
|   1898 | 1109 | `	pOut->bOptional = pOut->bOptional \|\| pOut->bVariadic \|\| zDef != 0;` |
|   1898 | 1110 | `	if( iSpace >= 0 && iDollar >= 0 && iSpace < iDollar ){` |
|      - | 1111 | `` 		/* The type is whatever precedes the first space, so `?DOMNode $child` `` |
|      - | 1112 | ``		 * types as `?DOMNode` and an untyped `$x` types as nothing. */`` |
|   1884 | 1113 | `		pOut->bNullable = (z[0] == '?' \|\| ReflectSigHasNoCase(z,iSpace,"null",4));` |
|   1884 | 1114 | `		SyStringInitFromBuf(&pOut->sType,z,iSpace);` |
|    940 | 1115 | `	}` |
|   1898 | 1116 | `	if( zDef ){` |
|    683 | 1117 | `		SyStringInitFromBuf(&pOut->sDefText,zDef,nDef);` |
|    341 | 1118 | `	}` |
|   1898 | 1119 | `}` |
|      - | 1120 | `/*` |
|      - | 1121 | ` * Resolve a Generator object into its wrapper. Mirrors the static` |
|      - | 1122 | ` * VmGeneratorExtractCtx in vm.c: the $__ctx attribute carries the` |
|      - | 1123 | ` * ph7_generator pointer as a resource value.` |
|      - | 1124 | ` */` |
|     42 | 1125 | `static ph7_generator * ReflectGeneratorCtx(ph7_vm *pVm, ph7_value *pVal)` |
|      1 | 1126 | `{` |
|      - | 1127 | `	ph7_class_instance *pThis;` |
|      - | 1128 | `	ph7_value *pAttr;` |
|      - | 1129 | `	SyString sAttr;` |
|     43 | 1130 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVm->pGeneratorClass == 0 ){` |
|    ! 0 | 1131 | `		return 0;` |
|      - | 1132 | `	}` |
|     43 | 1133 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     43 | 1134 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|    ! 0 | 1135 | `		return 0;` |
|      - | 1136 | `	}` |
|     43 | 1137 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|     43 | 1138 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     43 | 1139 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|    ! 0 | 1140 | `		return 0;` |
|      - | 1141 | `	}` |
|     43 | 1142 | `	return (ph7_generator *)pAttr->x.pOther;` |
|     22 | 1143 | `}` |
|      - | 1144 | `/*` |
|      - | 1145 | ` * Evaluate the recorded argument expressions of one declared attribute and` |
|      - | 1146 | ` * answer them as a PHP array, or NULL when the target no longer resolves.` |
|      - | 1147 | ` *` |
|      - | 1148 | ` * apArg is the four-part SPEC a ReflectionAttribute carries — kind 'class'` |
|      - | 1149 | ` * (target = class), 'attr' (class + property/constant name), 'method' (class +` |
|      - | 1150 | ` * method), 'fn' (function name or Closure), 'param' (function spec + parameter` |
|      - | 1151 | ` * index), 'const' (global constant name) — plus which attribute of that target.` |
|      - | 1152 | ` * Named arguments become string keys.` |
|      - | 1153 | ` *` |
|      - | 1154 | ` * The values are evaluated HERE rather than when the reflector was built:` |
|      - | 1155 | `` * `#[Attr(self::SOME)]` runs php code, and php runs it at getArguments() time.`` |
|      - | 1156 | ` */` |
|    142 | 1157 | `static ph7_value * ReflectAttrArgs(ph7_context *pCtx, ph7_value **apArg, sxu32 nAttrIdx)` |
|      3 | 1158 | `{` |
|    145 | 1159 | `	ph7_vm *pVm = pCtx->pVm;` |
|    145 | 1160 | `	SySet *pAttrs = 0;` |
|    145 | 1161 | `	ph7_class *pDeclCls = 0; /* class an attribute is declared on: scope for self:: in its args */` |
|      - | 1162 | `	ph7_attribute *pAttrRec;` |
|      - | 1163 | `	ph7_value *pOut;` |
|      - | 1164 | `	const char *zKind;` |
|      - | 1165 | `	int nKind;` |
|      - | 1166 | `	sxu32 n;` |
|    145 | 1167 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|    199 | 1168 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|    111 | 1169 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|    111 | 1170 | `		if( pClass ){ pAttrs = &pClass->aAttrs; pDeclCls = pClass; }` |
|     92 | 1171 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|      5 | 1172 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|      5 | 1173 | `		ph7_class_attr *pMember = pClass ? ReflectFetchMember(pClass, apArg[2]) : 0;` |
|      5 | 1174 | `		if( pMember ){ pAttrs = &pMember->aAttrs; pDeclCls = pClass; }` |
|     41 | 1175 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|     16 | 1176 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     16 | 1177 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|     30 | 1178 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|     12 | 1179 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], 0, 0, 0, 0, 0);` |
|     12 | 1180 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|     14 | 1181 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|      5 | 1182 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], apArg[2], 0, 0, 0, 0);` |
|      5 | 1183 | `		ph7_vm_func_arg *pParam = pFunc` |
|      4 | 1184 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|      5 | 1185 | `		if( pParam ){ pAttrs = &pParam->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|      5 | 1186 | `	}else if( nKind == 5 && SyMemcmp(zKind, "const", 5) == 0 ){` |
|      - | 1187 | ``		/* Global constant (php 8.5 attributes on `const` statements) */`` |
|      - | 1188 | `		const char *zCName;` |
|      - | 1189 | `		int nCName;` |
|      - | 1190 | `		SyHashEntry *pCEntry;` |
|      3 | 1191 | `		zCName = ph7_value_to_string(apArg[1], &nCName);` |
|      3 | 1192 | `		pCEntry = nCName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zCName, (sxu32)nCName) : 0;` |
|      3 | 1193 | `		if( pCEntry ){ pAttrs = &((ph7_constant *)pCEntry->pUserData)->aAttrs; }` |
|      1 | 1194 | `	}` |
|    142 | 1195 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|    145 | 1196 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|    ! 0 | 1197 | `		return 0;` |
|      - | 1198 | `	}` |
|    319 | 1199 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|    177 | 1200 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|      - | 1201 | `		ph7_value sValue;` |
|    177 | 1202 | `		PH7_MemObjInit(pVm, &sValue);` |
|    177 | 1203 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|      - | 1204 | `` 			/* Evaluate under the attribute's declaring-class scope so `self::class` `` |
|      - | 1205 | ``			 * / `self::CONST` / `parent::` resolve like php (else self:: would bind`` |
|      - | 1206 | `			 * to the reflection machinery's own class). */` |
|    146 | 1207 | `			PH7_VmExecAttrArg(pVm, &pArgRec->aByteCode, pDeclCls, &sValue);` |
|    104 | 1208 | `		}else if( pArgRec->pNativeValue ){` |
|      - | 1209 | ``			/* A NATIVE attribute (php's own `#[Attribute(...)]` on Attribute and`` |
|      - | 1210 | `			 * Deprecated): the argument is a literal, not byte-code. */` |
|     32 | 1211 | `			PH7_NativeLiteralValue(pVm, pArgRec->pNativeValue, &sValue);` |
|     15 | 1212 | `		}` |
|    177 | 1213 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|     13 | 1214 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|      7 | 1215 | `		}else{` |
|    165 | 1216 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|      - | 1217 | `		}` |
|    177 | 1218 | `		PH7_MemObjRelease(&sValue);` |
|     90 | 1219 | `	}` |
|    145 | 1220 | `	return pOut;` |
|     74 | 1221 | `}` |
|      - | 1222 | `/*` |
|      - | 1223 | ` * ---------------------------------------------------------------------------` |
|      - | 1224 | ` * The ReflectionType family.` |
|      - | 1225 | ` *` |
|      - | 1226 | ` * php's four type objects are pure VALUES: a text, a nullability flag, and for` |
|      - | 1227 | ` * the composites a list of members. Nothing in userland can build one — php` |
|      - | 1228 | `` * declares no constructor on any of them and refuses `clone` — so the prelude's`` |
|      - | 1229 | `` * public `__construct($name, $nullable, $text)` existed only because the factory`` |
|      - | 1230 | ` * that fills them was itself PHP. The factory is C now (ReflectMakeType), so the` |
|      - | 1231 | ` * constructors are gone and the classes say what php's say.` |
|      - | 1232 | ` * ---------------------------------------------------------------------------` |
|      - | 1233 | ` */` |
|      - | 1234 | `#define RT_TEXT     "__text"` |
|      - | 1235 | `#define RT_NULLABLE "__nullable"` |
|      - | 1236 | `#define RT_TNAME    "__tname"` |
|      - | 1237 | `#define RT_TYPES    "__types"` |
|      - | 1238 |  |
|     26 | 1239 | `static int vm_builtin_ReflectionType_allowsNull(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 1240 | `{` |
|     28 | 1241 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     13 | 1242 | `	SXUNUSED(nArg);` |
|     13 | 1243 | `	SXUNUSED(apArg);` |
|     28 | 1244 | `	ph7_result_bool(pCtx, pThis != 0 && PH7_NativeAttrTruthy(pThis, RT_NULLABLE));` |
|     28 | 1245 | `	return PH7_OK;` |
|      2 | 1246 | `}` |
|    996 | 1247 | `static int vm_builtin_ReflectionType_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 | 1248 | `{` |
|   1000 | 1249 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   1000 | 1250 | `	const char *zText = "";` |
|   1000 | 1251 | `	int nText = 0;` |
|    498 | 1252 | `	SXUNUSED(nArg);` |
|    498 | 1253 | `	SXUNUSED(apArg);` |
|   1000 | 1254 | `	if( pThis ){` |
|   1000 | 1255 | `		PH7_NativeAttrStr(pThis, RT_TEXT, &zText, &nText);` |
|    498 | 1256 | `	}` |
|   1000 | 1257 | `	ph7_result_string(pCtx, zText, nText);` |
|   1000 | 1258 | `	return PH7_OK;` |
|      4 | 1259 | `}` |
|     20 | 1260 | `static int vm_builtin_ReflectionNamedType_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 1261 | `{` |
|     23 | 1262 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     23 | 1263 | `	const char *zName = "";` |
|     23 | 1264 | `	int nName = 0;` |
|     10 | 1265 | `	SXUNUSED(nArg);` |
|     10 | 1266 | `	SXUNUSED(apArg);` |
|     23 | 1267 | `	if( pThis ){` |
|     23 | 1268 | `		PH7_NativeAttrStr(pThis, RT_TNAME, &zName, &nName);` |
|     10 | 1269 | `	}` |
|     23 | 1270 | `	ph7_result_string(pCtx, zName, nName);` |
|     23 | 1271 | `	return PH7_OK;` |
|      3 | 1272 | `}` |
|      - | 1273 | `/* php's builtin-type set, case-insensitively. Anything else is a class name. */` |
|     32 | 1274 | `static int ReflectTypeIsBuiltin(const char *zName, int nName)` |
|      1 | 1275 | `{` |
|      - | 1276 | `	static const char *azBuiltin[] = {` |
|      - | 1277 | `		"int","float","string","bool","array","object","mixed",` |
|      - | 1278 | `		"void","never","null","callable","iterable","true","false"` |
|      - | 1279 | `	};` |
|      - | 1280 | `	sxu32 n;` |
|    245 | 1281 | `	for( n = 0 ; n < SX_ARRAYSIZE(azBuiltin) ; n++ ){` |
|    235 | 1282 | `		int nWant = (int)SyStrlen(azBuiltin[n]);` |
|      - | 1283 | `		int k;` |
|    235 | 1284 | `		if( nWant != nName ){` |
|    199 | 1285 | `			continue;` |
|      - | 1286 | `		}` |
|    147 | 1287 | `		for( k = 0 ; k < nWant ; k++ ){` |
|    125 | 1288 | `			if( SyToLower(zName[k]) != azBuiltin[n][k] ){` |
|     15 | 1289 | `				break;` |
|      - | 1290 | `			}` |
|     56 | 1291 | `		}` |
|     37 | 1292 | `		if( k == nWant ){` |
|     23 | 1293 | `			return 1;` |
|      - | 1294 | `		}` |
|      8 | 1295 | `	}` |
|     11 | 1296 | `	return 0;` |
|     17 | 1297 | `}` |
|     24 | 1298 | `static int vm_builtin_ReflectionNamedType_isBuiltin(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1299 | `{` |
|     25 | 1300 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     25 | 1301 | `	const char *zName = "";` |
|     25 | 1302 | `	int nName = 0;` |
|     12 | 1303 | `	SXUNUSED(nArg);` |
|     12 | 1304 | `	SXUNUSED(apArg);` |
|     25 | 1305 | `	if( pThis ){` |
|     25 | 1306 | `		PH7_NativeAttrStr(pThis, RT_TNAME, &zName, &nName);` |
|     12 | 1307 | `	}` |
|     25 | 1308 | `	ph7_result_bool(pCtx, ReflectTypeIsBuiltin(zName, nName));` |
|     25 | 1309 | `	return PH7_OK;` |
|      1 | 1310 | `}` |
|      6 | 1311 | `static int vm_builtin_ReflectionType_getTypes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1312 | `{` |
|      7 | 1313 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      7 | 1314 | `	ph7_value *pTypes = pThis ? PH7_NativeAttr(pThis, RT_TYPES) : 0;` |
|      3 | 1315 | `	SXUNUSED(nArg);` |
|      3 | 1316 | `	SXUNUSED(apArg);` |
|      7 | 1317 | `	if( pTypes && (pTypes->iFlags & MEMOBJ_HASHMAP) ){` |
|      7 | 1318 | `		ph7_result_value(pCtx, pTypes);` |
|      4 | 1319 | `	}else{` |
|      - | 1320 | `		/* The declared default is a native NULL slot (a spec cannot carry an` |
|      - | 1321 | `		 * array literal), but php's getTypes() always answers a list. */` |
|    ! 0 | 1322 | `		ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|    ! 0 | 1323 | `		if( pEmpty ){` |
|    ! 0 | 1324 | `			ph7_result_value(pCtx, pEmpty);` |
|    ! 0 | 1325 | `		}` |
|      - | 1326 | `	}` |
|      7 | 1327 | `	return PH7_OK;` |
|      1 | 1328 | `}` |
|      - | 1329 | `/* Build one of the three concrete types with its slots filled. The caller owns` |
|      - | 1330 | ` * the reference (PH7_NativeResultObject / a list insert drops it). */` |
|   1524 | 1331 | `static ph7_class_instance * ReflectNewType(ph7_context *pCtx, const char *zClass,` |
|      - | 1332 | `	const char *zText, int nText, int bNullable)` |
|      4 | 1333 | `{` |
|   1528 | 1334 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1528 | 1335 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|   1528 | 1336 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|   1528 | 1337 | `	if( pObj == 0 ){` |
|    ! 0 | 1338 | `		return 0;` |
|      - | 1339 | `	}` |
|   1528 | 1340 | `	PH7_NativeSetAttrStr(pVm, pObj, RT_TEXT, zText, nText);` |
|   1528 | 1341 | `	PH7_NativeSetAttrBool(pVm, pObj, RT_NULLABLE, bNullable);` |
|   1528 | 1342 | `	return pObj;` |
|    766 | 1343 | `}` |
|      - | 1344 | `/*` |
|      - | 1345 | ` * Append pType to a composite's member list, handing over the caller's reference.` |
|      - | 1346 | ` *` |
|      - | 1347 | ` * The carrier is a STACK value, deliberately: a ph7_context_new_scalar() is` |
|      - | 1348 | ` * released with the call context, and releasing a MEMOBJ_OBJ carrier unrefs the` |
|      - | 1349 | ` * instance a second time — which freed every member of a union or intersection` |
|      - | 1350 | ` * the moment its factory returned, so the list came back holding dead objects.` |
|      - | 1351 | ` * PH7_NativeResultObject hands an instance over the same way.` |
|      - | 1352 | ` */` |
|    598 | 1353 | `static void ReflectTypeListAdd(ph7_context *pCtx, ph7_value *pList, ph7_class_instance *pType)` |
|      4 | 1354 | `{` |
|      - | 1355 | `	ph7_value sVal;` |
|    602 | 1356 | `	if( pType == 0 ){` |
|    ! 0 | 1357 | `		return;` |
|      - | 1358 | `	}` |
|    602 | 1359 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    602 | 1360 | `	sVal.x.pOther = pType;` |
|    602 | 1361 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|    602 | 1362 | `	ph7_array_add_elem(pList, 0, &sVal);   /* takes its own reference */` |
|    602 | 1363 | `	PH7_ClassInstanceUnref(pType);` |
|    303 | 1364 | `}` |
|      - | 1365 | `/* Exact, case-insensitive name test (php lower-cases before comparing). */` |
|   2366 | 1366 | `static int ReflectTypeNameIs(const char *z, int n, const char *zWant)` |
|      4 | 1367 | `{` |
|   2370 | 1368 | `	int nWant = (int)SyStrlen(zWant), k;` |
|   2370 | 1369 | `	if( n != nWant ){` |
|   2050 | 1370 | `		return 0;` |
|      - | 1371 | `	}` |
|    557 | 1372 | `	for( k = 0 ; k < n ; k++ ){` |
|    511 | 1373 | `		if( SyToLower(z[k]) != zWant[k] ){` |
|    277 | 1374 | `			return 0;` |
|      - | 1375 | `		}` |
|    119 | 1376 | `	}` |
|     48 | 1377 | `	return 1;` |
|   1187 | 1378 | `}` |
|      - | 1379 | `/*` |
|      - | 1380 | ` * A ReflectionNamedType for one name.` |
|      - | 1381 | ` *` |
|      - | 1382 | ` * bQMark says the text carried a leading '?', which is the ONLY thing that puts` |
|      - | 1383 | `` * one back on the rendered text — while `null` and `mixed` are nullable by their`` |
|      - | 1384 | ` * own meaning without ever rendering a '?'. Those two rules are independent, and` |
|      - | 1385 | `` * conflating them is how `?array` came out as `array`.`` |
|      - | 1386 | ` */` |
|   1356 | 1387 | `static ph7_class_instance * ReflectNewNamed(ph7_context *pCtx, const char *z, int n, int bQMark)` |
|      4 | 1388 | `{` |
|      - | 1389 | `	ph7_class_instance *pObj;` |
|      - | 1390 | `	char zBuf[256];` |
|   1360 | 1391 | `	const char *zText = z;` |
|   1360 | 1392 | `	int nText = n;` |
|   1360 | 1393 | `	int bNullable = bQMark \|\| ReflectTypeNameIs(z, n, "null") \|\| ReflectTypeNameIs(z, n, "mixed");` |
|   1360 | 1394 | `	if( bQMark && n + 1 < (int)sizeof(zBuf) ){` |
|    188 | 1395 | `		zBuf[0] = '?';` |
|    188 | 1396 | `		SyMemcpy(z, &zBuf[1], (sxu32)n);` |
|    188 | 1397 | `		zText = zBuf;` |
|    188 | 1398 | `		nText = n + 1;` |
|     93 | 1399 | `	}` |
|   1360 | 1400 | `	pObj = ReflectNewType(pCtx, "ReflectionNamedType", zText, nText, bNullable);` |
|   1360 | 1401 | `	if( pObj ){` |
|   1360 | 1402 | `		PH7_NativeSetAttrStr(pCtx->pVm, pObj, RT_TNAME, z, n);` |
|    678 | 1403 | `	}` |
|   1360 | 1404 | `	return pObj;` |
|      4 | 1405 | `}` |
|      - | 1406 | `/*` |
|      - | 1407 | `` * One ATOM of a type text: `?X`, `(A&B)` or a plain name. An intersection is`` |
|      - | 1408 | ` * the only composite an atom can be, because php only nests that way.` |
|      - | 1409 | ` */` |
|   1346 | 1410 | `static ph7_class_instance * ReflectMakeAtom(ph7_context *pCtx, const char *z, int n)` |
|      4 | 1411 | `{` |
|   1350 | 1412 | `	int bQMark = 0;` |
|   1350 | 1413 | `	if( n > 0 && z[0] == '?' ){` |
|    188 | 1414 | `		bQMark = 1;` |
|    188 | 1415 | `		z++;` |
|    188 | 1416 | `		n--;` |
|     93 | 1417 | `	}` |
|   1350 | 1418 | `	if( n > 1 && z[0] == '(' && z[n-1] == ')' ){` |
|      8 | 1419 | `		z++;` |
|      8 | 1420 | `		n -= 2;` |
|      3 | 1421 | `	}` |
|   1350 | 1422 | `	if( ReflectSigFindUnquoted(z, n, '&') >= 0 ){` |
|     12 | 1423 | `		ph7_class_instance *pObj = ReflectNewType(pCtx, "ReflectionIntersectionType", z, n, 0);` |
|     12 | 1424 | `		ph7_value *pList = ph7_context_new_array(pCtx);` |
|     12 | 1425 | `		const char *zCur = z;` |
|     12 | 1426 | `		int nCur = n;` |
|     12 | 1427 | `		if( pObj == 0 \|\| pList == 0 ){` |
|    ! 0 | 1428 | `			return pObj;` |
|      - | 1429 | `		}` |
|     22 | 1430 | `		while( nCur > 0 ){` |
|     22 | 1431 | `			int iCut = ReflectSigFindUnquoted(zCur, nCur, '&');` |
|     32 | 1432 | `			ReflectTypeListAdd(pCtx, pList,` |
|     10 | 1433 | `				ReflectNewNamed(pCtx, zCur, iCut < 0 ? nCur : iCut, 0));` |
|     22 | 1434 | `			if( iCut < 0 ){` |
|     12 | 1435 | `				break;` |
|      - | 1436 | `			}` |
|     12 | 1437 | `			zCur += iCut + 1;` |
|     12 | 1438 | `			nCur -= iCut + 1;` |
|      2 | 1439 | `		}` |
|     12 | 1440 | `		PH7_NativeSetProp(pCtx->pVm, pObj, RT_TYPES, sizeof(RT_TYPES)-1, pList);` |
|     12 | 1441 | `		return pObj;` |
|      - | 1442 | `	}` |
|   1340 | 1443 | `	return ReflectNewNamed(pCtx, z, n, bQMark);` |
|    677 | 1444 | `}` |
|      - | 1445 | `/*` |
|      - | 1446 | ` * A declared type TEXT to the object php answers for it: a named type, a union,` |
|      - | 1447 | `` * or an intersection. NULL for an absent type. `X\|null` collapses back to a`` |
|      - | 1448 | `` * NULLABLE named type, which is what php reports (`?X`), but only when exactly`` |
|      - | 1449 | ` * one non-null arm is left and it is not itself an intersection.` |
|      - | 1450 | ` */` |
|   1120 | 1451 | `static ph7_class_instance * ReflectMakeType(ph7_context *pCtx, const char *zText, int nText)` |
|      4 | 1452 | `{` |
|   1124 | 1453 | `	const char *zBody = zText;` |
|   1124 | 1454 | `	int nBody = nText;` |
|   1124 | 1455 | `	int bNullable = 0, bHasNull = 0, nParts = 0, nNonNull = 0;` |
|   1124 | 1456 | `	const char *zLastNonNull = 0;` |
|   1124 | 1457 | `	int nLastNonNull = 0;` |
|      - | 1458 | `	const char *zCur;` |
|      - | 1459 | `	int nCur, iDepth, k, iStart;` |
|   1124 | 1460 | `	if( nText < 1 ){` |
|    ! 0 | 1461 | `		return 0;` |
|      - | 1462 | `	}` |
|   1124 | 1463 | `	if( zBody[0] == '?' ){` |
|    188 | 1464 | `		bNullable = 1;` |
|    188 | 1465 | `		zBody++;` |
|    188 | 1466 | `		nBody--;` |
|     93 | 1467 | `	}` |
|      - | 1468 | ``	/* Split on the TOP-LEVEL '\|' only: `A\|(B&C)` has one at depth 0 and none`` |
|      - | 1469 | `	 * inside the parentheses. */` |
|   1124 | 1470 | `	iDepth = 0;` |
|   1124 | 1471 | `	iStart = 0;` |
|  10566 | 1472 | `	for( k = 0 ; k <= nBody ; k++ ){` |
|   9446 | 1473 | `		if( k < nBody && zBody[k] == '(' ){` |
|      8 | 1474 | `			iDepth++;` |
|      8 | 1475 | `			continue;` |
|      - | 1476 | `		}` |
|   9440 | 1477 | `		if( k < nBody && zBody[k] == ')' ){` |
|      8 | 1478 | `			iDepth--;` |
|      8 | 1479 | `			continue;` |
|      - | 1480 | `		}` |
|   9434 | 1481 | `		if( k == nBody \|\| (zBody[k] == '\|' && iDepth == 0) ){` |
|   1350 | 1482 | `			zCur = &zBody[iStart];` |
|   1350 | 1483 | `			nCur = k - iStart;` |
|   1350 | 1484 | `			nParts++;` |
|   1350 | 1485 | `			if( nCur == 4 && ReflectSigHasNoCase(zCur, 4, "null", 4) ){` |
|      3 | 1486 | `				bHasNull = 1;` |
|      2 | 1487 | `			}else{` |
|   1348 | 1488 | `				nNonNull++;` |
|   1348 | 1489 | `				zLastNonNull = zCur;` |
|   1348 | 1490 | `				nLastNonNull = nCur;` |
|      - | 1491 | `			}` |
|   1350 | 1492 | `			iStart = k + 1;` |
|    673 | 1493 | `		}` |
|   4719 | 1494 | `	}` |
|   1124 | 1495 | `	if( nParts > 1 ){` |
|      - | 1496 | `		ph7_class_instance *pObj;` |
|      - | 1497 | `		ph7_value *pList;` |
|    158 | 1498 | `		if( bHasNull && nNonNull == 1` |
|      4 | 1499 | `		 && ReflectSigFindUnquoted(zLastNonNull, nLastNonNull, '&') < 0 ){` |
|      - | 1500 | ``			/* `X\|null` IS `?X` to php. */`` |
|      - | 1501 | `			char zBuf[256];` |
|      - | 1502 | `			ph7_class_instance *pNamed;` |
|    ! 0 | 1503 | `			const char *zRender = zLastNonNull;` |
|    ! 0 | 1504 | `			int nRender = nLastNonNull;` |
|    ! 0 | 1505 | `			if( nLastNonNull + 1 < (int)sizeof(zBuf) ){` |
|    ! 0 | 1506 | `				zBuf[0] = '?';` |
|    ! 0 | 1507 | `				SyMemcpy(zLastNonNull, &zBuf[1], (sxu32)nLastNonNull);` |
|    ! 0 | 1508 | `				zRender = zBuf;` |
|    ! 0 | 1509 | `				nRender = nLastNonNull + 1;` |
|    ! 0 | 1510 | `			}` |
|    ! 0 | 1511 | `			pNamed = ReflectNewType(pCtx, "ReflectionNamedType", zRender, nRender, 1);` |
|    ! 0 | 1512 | `			if( pNamed ){` |
|    ! 0 | 1513 | `				PH7_NativeSetAttrStr(pCtx->pVm, pNamed, RT_TNAME, zLastNonNull, nLastNonNull);` |
|    ! 0 | 1514 | `			}` |
|    ! 0 | 1515 | `			return pNamed;` |
|      - | 1516 | `		}` |
|    161 | 1517 | `		pObj = ReflectNewType(pCtx, "ReflectionUnionType", zBody, nBody, bNullable \|\| bHasNull);` |
|    161 | 1518 | `		pList = ph7_context_new_array(pCtx);` |
|    161 | 1519 | `		if( pObj == 0 \|\| pList == 0 ){` |
|    ! 0 | 1520 | `			return pObj;` |
|      - | 1521 | `		}` |
|    161 | 1522 | `		iDepth = 0;` |
|    161 | 1523 | `		iStart = 0;` |
|   2895 | 1524 | `		for( k = 0 ; k <= nBody ; k++ ){` |
|   2737 | 1525 | `			if( k < nBody && zBody[k] == '(' ){` |
|      8 | 1526 | `				iDepth++;` |
|      8 | 1527 | `				continue;` |
|      - | 1528 | `			}` |
|   2731 | 1529 | `			if( k < nBody && zBody[k] == ')' ){` |
|      8 | 1530 | `				iDepth--;` |
|      8 | 1531 | `				continue;` |
|      - | 1532 | `			}` |
|   2725 | 1533 | `			if( k == nBody \|\| (zBody[k] == '\|' && iDepth == 0) ){` |
|    579 | 1534 | `				ReflectTypeListAdd(pCtx, pList,` |
|    192 | 1535 | `					ReflectMakeAtom(pCtx, &zBody[iStart], k - iStart));` |
|    387 | 1536 | `				iStart = k + 1;` |
|    192 | 1537 | `			}` |
|   1364 | 1538 | `		}` |
|    161 | 1539 | `		PH7_NativeSetProp(pCtx->pVm, pObj, RT_TYPES, sizeof(RT_TYPES)-1, pList);` |
|    161 | 1540 | `		return pObj;` |
|      - | 1541 | `	}` |
|    966 | 1542 | `	if( ReflectSigFindUnquoted(zBody, nBody, '&') >= 0 ){` |
|      5 | 1543 | `		return ReflectMakeAtom(pCtx, zBody, nBody);` |
|      - | 1544 | `	}` |
|    962 | 1545 | `	if( bNullable ){` |
|      - | 1546 | `		char zBuf[256];` |
|    188 | 1547 | `		if( nBody + 1 < (int)sizeof(zBuf) ){` |
|    188 | 1548 | `			zBuf[0] = '?';` |
|    188 | 1549 | `			SyMemcpy(zBody, &zBuf[1], (sxu32)nBody);` |
|    188 | 1550 | `			return ReflectMakeAtom(pCtx, zBuf, nBody + 1);` |
|      - | 1551 | `		}` |
|    ! 0 | 1552 | `	}` |
|    776 | 1553 | `	return ReflectMakeAtom(pCtx, zBody, nBody);` |
|    564 | 1554 | `}` |
|      - | 1555 | `/*` |
|      - | 1556 | ` * Declare the four type classes. Called from PH7_VmInstallReflection() where` |
|      - | 1557 | `` * chunk 4 used to be compiled, so `Stringable` (a core interface) already`` |
|      - | 1558 | ` * exists. PH7_CLASS_NOCLONE is php's own rule for these: they are values the` |
|      - | 1559 | `` * engine hands out, and `clone $type` is an Error, not a copy.`` |
|      - | 1560 | ` */` |
|   5740 | 1561 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionTypes(ph7_vm *pVm)` |
|      5 | 1562 | `{` |
|      - | 1563 | `	static const PH7_NativePropDef aBaseProp[] = {` |
|      - | 1564 | `		{ RT_TEXT,     PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - | 1565 | `		{ RT_NULLABLE, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 }, 0 },` |
|      - | 1566 | `	};` |
|      - | 1567 | `	static const PH7_NativeMethodDef aBaseMethod[] = {` |
|      - | 1568 | `		{ "allowsNull", PH7_MOD_PUBLIC, "", "@bool",       vm_builtin_ReflectionType_allowsNull },` |
|      - | 1569 | `		{ "__toString", PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionType_toString },` |
|      - | 1570 | `	};` |
|      - | 1571 | `	static const PH7_NativePropDef aNamedProp[] = {` |
|      - | 1572 | `		{ RT_TNAME, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - | 1573 | `	};` |
|      - | 1574 | `	static const PH7_NativeMethodDef aNamedMethod[] = {` |
|      - | 1575 | `		{ "getName",   PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionNamedType_getName },` |
|      - | 1576 | `		{ "isBuiltin", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionNamedType_isBuiltin },` |
|      - | 1577 | `	};` |
|      - | 1578 | `	static const PH7_NativePropDef aCompProp[] = {` |
|      - | 1579 | `		{ RT_TYPES, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 1580 | `	};` |
|      - | 1581 | `	static const PH7_NativeMethodDef aCompMethod[] = {` |
|      - | 1582 | `		{ "getTypes", PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionType_getTypes },` |
|      - | 1583 | `	};` |
|      - | 1584 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 1585 | `		{ "ReflectionType", 0, "Stringable", PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 1586 | `		  aBaseMethod, SX_ARRAYSIZE(aBaseMethod), 0, 0, aBaseProp, SX_ARRAYSIZE(aBaseProp), 0, 0, 0 },` |
|      - | 1587 | `		{ "ReflectionNamedType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 1588 | `		  aNamedMethod, SX_ARRAYSIZE(aNamedMethod), 0, 0, aNamedProp, SX_ARRAYSIZE(aNamedProp), 0, 0, 0 },` |
|      - | 1589 | `		{ "ReflectionUnionType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 1590 | `		  aCompMethod, SX_ARRAYSIZE(aCompMethod), 0, 0, aCompProp, SX_ARRAYSIZE(aCompProp), 0, 0, 0 },` |
|      - | 1591 | `		{ "ReflectionIntersectionType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 1592 | `		  aCompMethod, SX_ARRAYSIZE(aCompMethod), 0, 0, aCompProp, SX_ARRAYSIZE(aCompProp), 0, 0, 0 },` |
|      - | 1593 | `	};` |
|   5745 | 1594 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 | 1595 | `}` |
|      - | 1596 | `/*` |
|      - | 1597 | ` * ---------------------------------------------------------------------------` |
|      - | 1598 | ` * The six standalone reflection classes.` |
|      - | 1599 | ` *` |
|      - | 1600 | ` * ReflectionGenerator, ReflectionFiber, ReflectionConstant, ReflectionExtension,` |
|      - | 1601 | ` * ReflectionZendExtension and ReflectionReference reflect ENGINE state rather` |
|      - | 1602 | ` * than a class hierarchy, so each was a prelude class over one or two thunks` |
|      - | 1603 | ` * whose only job was to hand that state to PHP. Their bodies are the C now, and` |
|      - | 1604 | ` * the four thunks that existed for them (__reflect_gen_info, __reflect_gen_exec,` |
|      - | 1605 | ` * __reflect_const_info, __reflect_ref_id) are gone with them.` |
|      - | 1606 | ` *` |
|      - | 1607 | ` * The ReflectionEnum family stays in the prelude: it extends ReflectionClass and` |
|      - | 1608 | ` * ReflectionClassConstant, which are still PHP.` |
|      - | 1609 | ` * ---------------------------------------------------------------------------` |
|      - | 1610 | ` */` |
|      - | 1611 | `#define RG_GEN   "__gen"` |
|      - | 1612 | `#define RF_FIBER "__fiber"` |
|      - | 1613 | `#define RR_ID    "__id"` |
|      - | 1614 |  |
|      - | 1615 | `/* Build an instance of a class that may still be PRELUDE PHP, running its` |
|      - | 1616 | ` * constructor. Answers 0 when the constructor threw (rc says which). */` |
|   1510 | 1617 | `static ph7_class_instance * ReflectConstruct(ph7_context *pCtx, const char *zClass,` |
|      - | 1618 | `	int nArg, ph7_value **apArg, sxi32 *pRc)` |
|      5 | 1619 | `{` |
|   1515 | 1620 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1515 | 1621 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|      - | 1622 | `	ph7_class_instance *pThis;` |
|      - | 1623 | `	ph7_class_method *pCons;` |
|   1515 | 1624 | `	*pRc = PH7_OK;` |
|   1515 | 1625 | `	if( pClass == 0 ){` |
|    ! 0 | 1626 | `		return 0;` |
|      - | 1627 | `	}` |
|   1515 | 1628 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|   1515 | 1629 | `	if( pThis == 0 ){` |
|    ! 0 | 1630 | `		return 0;` |
|      - | 1631 | `	}` |
|   1515 | 1632 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|   1515 | 1633 | `	if( pCons ){` |
|   1515 | 1634 | `		sxi32 rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, nArg, apArg);` |
|   1515 | 1635 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|    ! 0 | 1636 | `			PH7_ClassInstanceUnref(pThis);` |
|    ! 0 | 1637 | `			*pRc = rc;` |
|    ! 0 | 1638 | `			return 0;` |
|      - | 1639 | `		}` |
|    755 | 1640 | `	}` |
|   1515 | 1641 | `	return pThis;` |
|    760 | 1642 | `}` |
|      - | 1643 | `/* The instance a native reflector wraps ($this->__gen / $this->__fiber). */` |
|     48 | 1644 | `static ph7_class_instance * ReflectWrapped(ph7_context *pCtx, const char *zSlot)` |
|      1 | 1645 | `{` |
|     49 | 1646 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     49 | 1647 | `	return pThis ? PH7_NativeAttrObj(pThis, zSlot) : 0;` |
|      1 | 1648 | `}` |
|      - | 1649 | `/* Hand back a wrapped instance the receiver already owns (a borrowed reference). */` |
|     10 | 1650 | `static int ReflectResultBorrowed(ph7_context *pCtx, ph7_class_instance *pObj)` |
|      1 | 1651 | `{` |
|      - | 1652 | `	ph7_value sVal;` |
|     11 | 1653 | `	if( pObj == 0 ){` |
|    ! 0 | 1654 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1655 | `		return PH7_OK;` |
|      - | 1656 | `	}` |
|     11 | 1657 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     11 | 1658 | `	sVal.x.pOther = pObj;` |
|     11 | 1659 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|     11 | 1660 | `	ph7_result_value(pCtx, &sVal);   /* takes its own reference */` |
|     11 | 1661 | `	return PH7_OK;` |
|      6 | 1662 | `}` |
|      - | 1663 | `/*` |
|      - | 1664 | ` * ---------------------------------------------------------------------------` |
|      - | 1665 | ` * ReflectionAttribute — chunk 7.` |
|      - | 1666 | ` *` |
|      - | 1667 | ` * php's own class, plus the shared getAttributes() body that produces it. The` |
|      - | 1668 | ` * chunk it replaces also carried __reflect_target_names (folded into the` |
|      - | 1669 | ` * "cannot target" diagnostic below) and __reflect_has_deprecated, which had` |
|      - | 1670 | ` * already lost its last caller when isDeprecated() became C.` |
|      - | 1671 | ` *` |
|      - | 1672 | ` * An instance holds a SPEC, not the arguments: [kind, target, member,` |
|      - | 1673 | ` * paramIdx] names something the engine can reopen, and getArguments()` |
|      - | 1674 | ` * evaluates the recorded expressions on every call, because php does too --` |
|      - | 1675 | `` * `#[A(self::X)]` is php code and it runs when it is asked for. That is also`` |
|      - | 1676 | ` * why the prelude needed a public __init(): PHP could not fill a fresh object` |
|      - | 1677 | ` * any other way. C fills it directly, so __init() (and the` |
|      - | 1678 | ` * __reflect_new_no_ctor that built the empty shell) are gone with it.` |
|      - | 1679 | ` * ---------------------------------------------------------------------------` |
|      - | 1680 | ` */` |
|      - | 1681 | ``#define RA_NAME   "name"      /* php declares this one PUBLIC: `$attr->name` */`` |
|      - | 1682 | `#define RA_SPEC   "__spec"    /* [kind, target, member, paramIdx] */` |
|      - | 1683 | `#define RA_IDX    "__idx"     /* which of that target's attributes this is */` |
|      - | 1684 | `#define RA_TARGET "__target"  /* the Attribute::TARGET_* bit it was found on */` |
|      - | 1685 | `#define RA_REP    "__rep"     /* the target carries more than one of this name */` |
|      - | 1686 |  |
|      - | 1687 | `/* Bound on the value exporter's own recursion. An attribute argument cannot be` |
|      - | 1688 | ` * cyclic, but an OBJECT argument's property graph can be. */` |
|      - | 1689 | `#define REFLECT_EXPORT_MAX_DEPTH 31` |
|      - | 1690 |  |
|      - | 1691 | `/* php's Attribute::TARGET_* names, in bit order (TARGET_CLASS is bit 0). */` |
|      - | 1692 | `static const char *const azReflectTarget[] = {` |
|      - | 1693 | `	"class", "function", "method", "property", "class constant", "parameter", "constant"` |
|      - | 1694 | `};` |
|      - | 1695 |  |
|      - | 1696 | `static void ReflectExportValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth);` |
|      - | 1697 |  |
|      - | 1698 | `/*` |
|      - | 1699 | ` * A string the way php's reflection prints one, in either of php's TWO quotings.` |
|      - | 1700 | ` *` |
|      - | 1701 | ` * A USERLAND default prints single-quoted, with the NON-PRINTABLE bytes escaped` |
|      - | 1702 | ` * (php's smart_str_append_escaped) and the quote itself NOT escaped — php's own` |
|      - | 1703 | ` * output for "q'q" is 'q'q'. An INTERNAL one prints DOUBLE-quoted and escapes the` |
|      - | 1704 | `` * quote: php answers `string $enclosure = "\""` for str_getcsv where PHL used to`` |
|      - | 1705 | `` * answer `'"'`. Every internal function with a string default was affected; the`` |
|      - | 1706 | `` * pair was found on Closure::bindTo's `= "static"` while making that class native.`` |
|      - | 1707 | ` */` |
|     72 | 1708 | `static void ReflectExportStrQ(SyBlob *pOut, const char *zIn, sxu32 nIn, char cQuote)` |
|      1 | 1709 | `{` |
|      - | 1710 | `	static const char zHexDigit[] = "0123456789ABCDEF";` |
|      - | 1711 | `	sxu32 i;` |
|     73 | 1712 | `	SyBlobAppend(pOut, &cQuote, sizeof(char));` |
|    209 | 1713 | `	for( i = 0 ; i < nIn ; i++ ){` |
|    137 | 1714 | `		unsigned char c = (unsigned char)zIn[i];` |
|    137 | 1715 | `		if( c == (unsigned char)cQuote && cQuote == '"' ){` |
|      3 | 1716 | `			SyBlobAppend(pOut, "\\\"", sizeof("\\\"")-1);` |
|      3 | 1717 | `			continue;` |
|      - | 1718 | `		}` |
|      - | 1719 | `		{` |
|    135 | 1720 | `		const char *zEsc = 0;` |
|    135 | 1721 | `		switch( c ){` |
|      3 | 1722 | `			case 0x09: zEsc = "\\t"; break;` |
|      9 | 1723 | `			case 0x0A: zEsc = "\\n"; break;` |
|      3 | 1724 | `			case 0x0B: zEsc = "\\v"; break;` |
|      3 | 1725 | `			case 0x0C: zEsc = "\\f"; break;` |
|      3 | 1726 | `			case 0x0D: zEsc = "\\r"; break;` |
|      3 | 1727 | `			case 0x1B: zEsc = "\\e"; break;` |
|      5 | 1728 | `			case '\\': zEsc = "\\\\"; break;` |
|    112 | 1729 | `			default:   break;` |
|      - | 1730 | `		}` |
|    135 | 1731 | `		if( zEsc ){` |
|     23 | 1732 | `			SyBlobAppend(pOut, zEsc, sizeof("\\t")-1);` |
|    127 | 1733 | `		}else if( c < 0x20 \|\| c > 0x7E ){` |
|      - | 1734 | `			char zHex[4];` |
|      7 | 1735 | `			zHex[0] = '\\';` |
|      7 | 1736 | `			zHex[1] = 'x';` |
|      7 | 1737 | `			zHex[2] = zHexDigit[(c >> 4) & 0x0F];` |
|      7 | 1738 | `			zHex[3] = zHexDigit[c & 0x0F];` |
|      7 | 1739 | `			SyBlobAppend(pOut, zHex, sizeof(zHex));` |
|      4 | 1740 | `		}else{` |
|    107 | 1741 | `			SyBlobAppend(pOut, (const char *)&c, sizeof(char));` |
|      - | 1742 | `		}` |
|      - | 1743 | `		}` |
|     68 | 1744 | `	}` |
|     73 | 1745 | `	SyBlobAppend(pOut, &cQuote, sizeof(char));` |
|     73 | 1746 | `}` |
|      - | 1747 | `/* php's userland quoting, which is what every existing caller means. */` |
|     62 | 1748 | `static void ReflectExportStr(SyBlob *pOut, const char *zIn, sxu32 nIn)` |
|      1 | 1749 | `{` |
|     63 | 1750 | `	ReflectExportStrQ(pOut, zIn, nIn, '\'');` |
|     63 | 1751 | `}` |
|      - | 1752 | `/*` |
|      - | 1753 | ` * A float the way php's reflection prints one: the plain string cast (which` |
|      - | 1754 | `` * PHL already renders php's way, 1.0E+15 and all), then php's `zero_frac` —`` |
|      - | 1755 | ` * a float that came out without a fraction gets ".0" so 1.0 does not print as` |
|      - | 1756 | ` * 1. INF and NAN render as words and are left alone.` |
|      - | 1757 | ` */` |
|     20 | 1758 | `static void ReflectExportReal(ph7_vm *pVm, SyBlob *pOut, ph7_real rVal)` |
|      1 | 1759 | `{` |
|      - | 1760 | `	ph7_value sTmp;` |
|      - | 1761 | `	const char *zText;` |
|     21 | 1762 | `	int nText, i, bPlain = 1;` |
|     21 | 1763 | `	PH7_MemObjInitFromReal(pVm, &sTmp, rVal);` |
|     21 | 1764 | `	zText = ph7_value_to_string(&sTmp, &nText);` |
|     21 | 1765 | `	if( nText > 0 ){` |
|     21 | 1766 | `		SyBlobAppend(pOut, zText, (sxu32)nText);` |
|     10 | 1767 | `	}` |
|     47 | 1768 | `	for( i = 0 ; i < nText ; i++ ){` |
|     35 | 1769 | `		if( (zText[i] < '0' \|\| zText[i] > '9') && !(i == 0 && zText[i] == '-') ){` |
|      9 | 1770 | `			bPlain = 0;` |
|      9 | 1771 | `			break;` |
|      - | 1772 | `		}` |
|     14 | 1773 | `	}` |
|     21 | 1774 | `	if( bPlain && nText > 0 ){` |
|     13 | 1775 | `		SyBlobAppend(pOut, ".0", sizeof(".0")-1);` |
|      6 | 1776 | `	}` |
|     21 | 1777 | `	PH7_MemObjRelease(&sTmp);` |
|     21 | 1778 | `}` |
|      - | 1779 | `/*` |
|      - | 1780 | ` * An array the way php's reflection prints one: a LIST — every key an integer,` |
|      - | 1781 | ` * in sequence from zero — prints no keys at all, and anything else prints one` |
|      - | 1782 | `` * for EVERY entry. That is why `[1, 'k' => 2]` comes out as`` |
|      - | 1783 | `` * `[0 => 1, 'k' => 2]` while `[[1], [2 => 3]]` keeps its outer keys silent.`` |
|      - | 1784 | ` */` |
|     30 | 1785 | `static void ReflectExportArray(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth)` |
|      1 | 1786 | `{` |
|     31 | 1787 | `	ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|      - | 1788 | `	ph7_hashmap_node *pEntry;` |
|     31 | 1789 | `	sxi64 iExpect = 0;` |
|      - | 1790 | `	sxu32 n;` |
|     31 | 1791 | `	int bList = 1;` |
|     61 | 1792 | `	for( pEntry = pMap->pFirst, n = 0 ; n < pMap->nEntry && pEntry ; n++ ){` |
|     43 | 1793 | `		if( pEntry->iType == HASHMAP_BLOB_NODE \|\| pEntry->xKey.iKey != iExpect ){` |
|     13 | 1794 | `			bList = 0;` |
|     13 | 1795 | `			break;` |
|      - | 1796 | `		}` |
|     31 | 1797 | `		iExpect++;` |
|     31 | 1798 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     16 | 1799 | `	}` |
|     31 | 1800 | `	SyBlobAppend(pOut, "[", sizeof(char));` |
|     75 | 1801 | `	for( pEntry = pMap->pFirst, n = 0 ; n < pMap->nEntry && pEntry ; n++ ){` |
|     45 | 1802 | `		ph7_value *pMember = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|     45 | 1803 | `		if( n > 0 ){` |
|     17 | 1804 | `			SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|      8 | 1805 | `		}` |
|     45 | 1806 | `		if( !bList ){` |
|     21 | 1807 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|     13 | 1808 | `				ReflectExportStr(pOut, (const char *)SyBlobData(&pEntry->xKey.sKey),` |
|      4 | 1809 | `					SyBlobLength(&pEntry->xKey.sKey));` |
|      5 | 1810 | `			}else{` |
|     13 | 1811 | `				SyBlobFormat(pOut, "%qd", pEntry->xKey.iKey);` |
|      - | 1812 | `			}` |
|     21 | 1813 | `			SyBlobAppend(pOut, " => ", sizeof(" => ")-1);` |
|     10 | 1814 | `		}` |
|     45 | 1815 | `		ReflectExportValue(pCtx, pOut, pMember, iDepth + 1);` |
|     45 | 1816 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     23 | 1817 | `	}` |
|     31 | 1818 | `	SyBlobAppend(pOut, "]", sizeof(char));` |
|     31 | 1819 | `}` |
|      - | 1820 | `/*` |
|      - | 1821 | `` * One value in php's reflection export syntax — the text after `= ` in a`` |
|      - | 1822 | `` * parameter default and inside `Argument #0 [ … ]` in an attribute dump.`` |
|      - | 1823 | ` *` |
|      - | 1824 | ` * The type dispatch is PH7_MemObjDump's, including its REAL-before-INT order:` |
|      - | 1825 | ` * an integer-valued float carries a cached int view too, and php prints 1.0.` |
|      - | 1826 | ` *` |
|      - | 1827 | ` * php renders an OBJECT argument by echoing the source expression it never` |
|      - | 1828 | `` * folded (`new \A(x: 1)`), which needs the AST. PHL evaluates attribute`` |
|      - | 1829 | ` * arguments from byte-code and has only the resulting VALUE, so a non-enum` |
|      - | 1830 | ` * object is rebuilt from its own properties: same shape, not always the same` |
|      - | 1831 | ` * bytes. An enum case — the one object php's own value formatter handles —` |
|      - | 1832 | ` * matches exactly.` |
|      - | 1833 | ` */` |
|    182 | 1834 | `static void ReflectExportValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth)` |
|      1 | 1835 | `{` |
|    183 | 1836 | `	if( pVal == 0 \|\| iDepth > REFLECT_EXPORT_MAX_DEPTH ){` |
|    ! 0 | 1837 | `		SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|    ! 0 | 1838 | `		return;` |
|      - | 1839 | `	}` |
|    183 | 1840 | `	if( (pVal->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      3 | 1841 | `		ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|      3 | 1842 | `		if( pObj->pClass->iFlags & PH7_CLASS_ENUM ){` |
|      3 | 1843 | `			ph7_value *pName = PH7_EnumCaseNameValue(pObj);` |
|      - | 1844 | `			/* php writes the case as the source did, so a namespaced enum comes` |
|      - | 1845 | `			 * out fully qualified (\N\E::One) and a global one bare (E::One).` |
|      - | 1846 | `			 * The separator is the only thing left of that distinction here. */` |
|      3 | 1847 | `			if( SyByteFind(SyStringData(&pObj->pClass->sName),` |
|      4 | 1848 | `				SyStringLength(&pObj->pClass->sName), '\\', 0) == SXRET_OK ){` |
|    ! 0 | 1849 | `				SyBlobAppend(pOut, "\\", sizeof(char));` |
|    ! 0 | 1850 | `			}` |
|      3 | 1851 | `			SyBlobFormat(pOut, "%z::", &pObj->pClass->sName);` |
|      3 | 1852 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|      3 | 1853 | `				SyBlobAppend(pOut, SyBlobData(&pName->sBlob), SyBlobLength(&pName->sBlob));` |
|      1 | 1854 | `			}` |
|      3 | 1855 | `			return;` |
|      - | 1856 | `		}` |
|    ! 0 | 1857 | `		SyBlobFormat(pOut, "new \\%z(", &pObj->pClass->sName);` |
|      - | 1858 | `		{` |
|      - | 1859 | `			SyHashEntry *pEntry;` |
|    ! 0 | 1860 | `			int nWritten = 0;` |
|    ! 0 | 1861 | `			SyHashResetLoopCursor(&pObj->hAttr);` |
|    ! 0 | 1862 | `			while((pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){` |
|    ! 0 | 1863 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1864 | `				ph7_value *pSlot;` |
|    ! 0 | 1865 | `				if( PH7_ATTR_UNPRESENTED(pVmAttr)` |
|    ! 0 | 1866 | `				 \|\| (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) ){` |
|    ! 0 | 1867 | `					continue; /* class-level members are not part of the object */` |
|      - | 1868 | `				}` |
|    ! 0 | 1869 | `				pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|    ! 0 | 1870 | `				if( nWritten++ ){` |
|    ! 0 | 1871 | `					SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|    ! 0 | 1872 | `				}` |
|    ! 0 | 1873 | `				ReflectExportValue(pCtx, pOut, pSlot, iDepth + 1);` |
|    ! 0 | 1874 | `			}` |
|      - | 1875 | `		}` |
|    ! 0 | 1876 | `		SyBlobAppend(pOut, ")", sizeof(char));` |
|    ! 0 | 1877 | `		return;` |
|      - | 1878 | `	}` |
|    181 | 1879 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|     21 | 1880 | `		SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|     21 | 1881 | `		return;` |
|      - | 1882 | `	}` |
|    161 | 1883 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|     31 | 1884 | `		ReflectExportArray(pCtx, pOut, pVal, iDepth);` |
|     31 | 1885 | `		return;` |
|      - | 1886 | `	}` |
|    131 | 1887 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      5 | 1888 | `		if( pVal->x.iVal != 0 ){` |
|      3 | 1889 | `			SyBlobAppend(pOut, "true", sizeof("true")-1);` |
|      2 | 1890 | `		}else{` |
|      3 | 1891 | `			SyBlobAppend(pOut, "false", sizeof("false")-1);` |
|      - | 1892 | `		}` |
|      5 | 1893 | `		return;` |
|      - | 1894 | `	}` |
|    127 | 1895 | `	if( pVal->iFlags & MEMOBJ_REAL ){` |
|     21 | 1896 | `		ReflectExportReal(pCtx->pVm, pOut, pVal->rVal);` |
|     21 | 1897 | `		return;` |
|      - | 1898 | `	}` |
|    107 | 1899 | `	if( pVal->iFlags & MEMOBJ_INT ){` |
|     53 | 1900 | `		SyBlobFormat(pOut, "%qd", pVal->x.iVal);` |
|     53 | 1901 | `		return;` |
|      - | 1902 | `	}` |
|     55 | 1903 | `	if( pVal->iFlags & MEMOBJ_STRING ){` |
|     55 | 1904 | `		ReflectExportStr(pOut, (const char *)SyBlobData(&pVal->sBlob), SyBlobLength(&pVal->sBlob));` |
|     55 | 1905 | `		return;` |
|      - | 1906 | `	}` |
|      - | 1907 | `	/* A resource, and anything else php has no export syntax for. */` |
|    ! 0 | 1908 | `	SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|     92 | 1909 | `}` |
|      - | 1910 | `/*` |
|      - | 1911 | ` * Build one ReflectionAttribute. pSpec is shared by every attribute of the` |
|      - | 1912 | ` * same target — it says how to reopen it, not which one this is.` |
|      - | 1913 | ` */` |
|    158 | 1914 | `static ph7_class_instance * ReflectAttrNew(ph7_context *pCtx, SyString *pName,` |
|      - | 1915 | `	ph7_value *pSpec, sxu32 nIdx, int iTargetBit, int bRepeated)` |
|      4 | 1916 | `{` |
|    162 | 1917 | `	ph7_vm *pVm = pCtx->pVm;` |
|    162 | 1918 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, "ReflectionAttribute",` |
|      - | 1919 | `		sizeof("ReflectionAttribute")-1, FALSE, 0);` |
|    162 | 1920 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|    162 | 1921 | `	if( pObj == 0 ){` |
|    ! 0 | 1922 | `		return 0;` |
|      - | 1923 | `	}` |
|    162 | 1924 | `	PH7_NativeSetAttrStr(pVm, pObj, RA_NAME, SyStringData(pName), (int)SyStringLength(pName));` |
|    162 | 1925 | `	PH7_NativeSetProp(pVm, pObj, RA_SPEC, sizeof(RA_SPEC)-1, pSpec);` |
|    162 | 1926 | `	PH7_NativeSetAttrInt(pVm, pObj, RA_IDX, (sxi64)nIdx);` |
|    162 | 1927 | `	PH7_NativeSetAttrInt(pVm, pObj, RA_TARGET, iTargetBit);` |
|    162 | 1928 | `	PH7_NativeSetAttrBool(pVm, pObj, RA_REP, bRepeated);` |
|    162 | 1929 | `	return pObj;` |
|     83 | 1930 | `}` |
|      - | 1931 | `/*` |
|      - | 1932 | ` * Unpack the receiver's spec into the four-value vector ReflectAttrArgs takes.` |
|      - | 1933 | ` * Returns 0 when the object is not one this file built.` |
|      - | 1934 | ` */` |
|     88 | 1935 | `static int ReflectAttrSpec(ph7_context *pCtx, ph7_class_instance *pThis, ph7_value **apOut)` |
|      2 | 1936 | `{` |
|     90 | 1937 | `	ph7_value *pSpec = pThis ? PH7_NativeAttr(pThis, RA_SPEC) : 0;` |
|      - | 1938 | `	ph7_hashmap *pMap;` |
|      - | 1939 | `	int i;` |
|     90 | 1940 | `	if( pSpec == 0 \|\| (pSpec->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 1941 | `		return 0;` |
|      - | 1942 | `	}` |
|     90 | 1943 | `	pMap = (ph7_hashmap *)pSpec->x.pOther;` |
|    442 | 1944 | `	for( i = 0 ; i < 4 ; i++ ){` |
|      - | 1945 | `		ph7_value sKey;` |
|    354 | 1946 | `		ph7_hashmap_node *pNode = 0;` |
|      - | 1947 | `		sxi32 rc;` |
|    354 | 1948 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sKey, i);` |
|    354 | 1949 | `		rc = PH7_HashmapLookup(pMap, &sKey, &pNode);` |
|    354 | 1950 | `		PH7_MemObjRelease(&sKey);` |
|    354 | 1951 | `		if( rc != SXRET_OK \|\| pNode == 0 ){` |
|    ! 0 | 1952 | `			return 0;` |
|      - | 1953 | `		}` |
|    354 | 1954 | `		apOut[i] = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pNode->nValIdx);` |
|    354 | 1955 | `		if( apOut[i] == 0 ){` |
|    ! 0 | 1956 | `			return 0;` |
|      - | 1957 | `		}` |
|    178 | 1958 | `	}` |
|     90 | 1959 | `	return 1;` |
|     46 | 1960 | `}` |
|      - | 1961 | `/* The receiver's evaluated arguments, or an empty array when the target no` |
|      - | 1962 | `` * longer resolves (what the chunk's `$a === null ? array() : $a` said). */`` |
|     88 | 1963 | `static ph7_value * ReflectAttrOwnArgs(ph7_context *pCtx, ph7_class_instance *pThis)` |
|      2 | 1964 | `{` |
|      - | 1965 | `	ph7_value *apSpec[4];` |
|     90 | 1966 | `	ph7_value *pArgs = 0;` |
|     90 | 1967 | `	if( pThis && ReflectAttrSpec(pCtx, pThis, apSpec) ){` |
|     90 | 1968 | `		pArgs = ReflectAttrArgs(pCtx, apSpec, (sxu32)PH7_NativeAttrInt(pThis, RA_IDX));` |
|     44 | 1969 | `	}` |
|     90 | 1970 | `	return pArgs ? pArgs : ph7_context_new_array(pCtx);` |
|      2 | 1971 | `}` |
|     48 | 1972 | `static int vm_builtin_ReflectionAttribute_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 1973 | `{` |
|     51 | 1974 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     51 | 1975 | `	const char *zName = "";` |
|     51 | 1976 | `	int nName = 0;` |
|     24 | 1977 | `	SXUNUSED(nArg);` |
|     24 | 1978 | `	SXUNUSED(apArg);` |
|     51 | 1979 | `	if( pThis ){` |
|     51 | 1980 | `		PH7_NativeAttrStr(pThis, RA_NAME, &zName, &nName);` |
|     24 | 1981 | `	}` |
|     51 | 1982 | `	ph7_result_string(pCtx, zName, nName);` |
|     51 | 1983 | `	return PH7_OK;` |
|      3 | 1984 | `}` |
|     24 | 1985 | `static int vm_builtin_ReflectionAttribute_getTarget(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1986 | `{` |
|     25 | 1987 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     12 | 1988 | `	SXUNUSED(nArg);` |
|     12 | 1989 | `	SXUNUSED(apArg);` |
|     25 | 1990 | `	ph7_result_int64(pCtx, pThis ? PH7_NativeAttrInt(pThis, RA_TARGET) : 0);` |
|     25 | 1991 | `	return PH7_OK;` |
|      1 | 1992 | `}` |
|     18 | 1993 | `static int vm_builtin_ReflectionAttribute_isRepeated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1994 | `{` |
|     19 | 1995 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      9 | 1996 | `	SXUNUSED(nArg);` |
|      9 | 1997 | `	SXUNUSED(apArg);` |
|     19 | 1998 | `	ph7_result_bool(pCtx, pThis != 0 && PH7_NativeAttrTruthy(pThis, RA_REP));` |
|     19 | 1999 | `	return PH7_OK;` |
|      1 | 2000 | `}` |
|     36 | 2001 | `static int vm_builtin_ReflectionAttribute_getArguments(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2002 | `{` |
|     37 | 2003 | `	ph7_value *pArgs = ReflectAttrOwnArgs(pCtx, PH7_ContextThis(pCtx));` |
|     18 | 2004 | `	SXUNUSED(nArg);` |
|     18 | 2005 | `	SXUNUSED(apArg);` |
|     37 | 2006 | `	if( pArgs == 0 ){` |
|    ! 0 | 2007 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2008 | `	}` |
|     37 | 2009 | `	ph7_result_value(pCtx, pArgs);` |
|     37 | 2010 | `	return PH7_OK;` |
|     19 | 2011 | `}` |
|      - | 2012 | `/*` |
|      - | 2013 | ` * newInstance(): the four screens php runs before it constructs anything —` |
|      - | 2014 | ` * the attribute class exists, it is declared #[Attribute], that declaration` |
|      - | 2015 | ` * allows the target this was found on, and it allows repetition if it was` |
|      - | 2016 | ` * repeated. The messages are php's, byte for byte.` |
|      - | 2017 | ` */` |
|     62 | 2018 | `static int vm_builtin_ReflectionAttribute_newInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 2019 | `{` |
|     65 | 2020 | `	ph7_vm *pVm = pCtx->pVm;` |
|     65 | 2021 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     65 | 2022 | `	ph7_value *pNameVal = pThis ? PH7_NativeAttr(pThis, RA_NAME) : 0;` |
|      - | 2023 | `	ph7_class *pClass;` |
|      - | 2024 | `	ph7_attribute *aA;` |
|      - | 2025 | `	ph7_value *pDeclArgs;` |
|     65 | 2026 | `	sxu32 n, nDecl = 0;` |
|     65 | 2027 | `	int bDecl = 0, iTarget, iBit;` |
|     65 | 2028 | `	sxi64 iFlags = 127; /* php's TARGET_ALL: #[Attribute] with no argument */` |
|     31 | 2029 | `	SXUNUSED(nArg);` |
|     31 | 2030 | `	SXUNUSED(apArg);` |
|     65 | 2031 | `	if( pNameVal == 0 ){` |
|    ! 0 | 2032 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2033 | `		return PH7_OK;` |
|      - | 2034 | `	}` |
|     65 | 2035 | `	pClass = ReflectResolveClass(pVm, pNameVal);` |
|     65 | 2036 | `	if( pClass == 0 ){` |
|      - | 2037 | `		SyString sName;` |
|      5 | 2038 | `		SyStringInitFromBuf(&sName, SyBlobData(&pNameVal->sBlob), SyBlobLength(&pNameVal->sBlob));` |
|      5 | 2039 | `		return PH7_VmThrowException(pCtx, "Error", "Attribute class \"%z\" not found", &sName);` |
|      - | 2040 | `	}` |
|      - | 2041 | `	/* Which #[...] on the attribute class is its own #[Attribute] declaration */` |
|     61 | 2042 | `	aA = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);` |
|     61 | 2043 | `	for( n = 0 ; n < SySetUsed(&pClass->aAttrs) ; n++ ){` |
|     54 | 2044 | `		if( SyStringLength(&aA[n].sName) == sizeof("Attribute")-1` |
|     57 | 2045 | `		 && SyStrnicmp(SyStringData(&aA[n].sName), "Attribute", sizeof("Attribute")-1) == 0 ){` |
|     57 | 2046 | `			nDecl = n;` |
|     57 | 2047 | `			bDecl = 1;` |
|     57 | 2048 | `			break;` |
|      - | 2049 | `		}` |
|    ! 0 | 2050 | `	}` |
|     61 | 2051 | `	if( !bDecl ){` |
|      7 | 2052 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      2 | 2053 | `			"Attempting to use non-attribute class \"%z\" as attribute", &pClass->sName);` |
|      - | 2054 | `	}` |
|      - | 2055 | ``	/* Its argument is the target mask: positional, or named `flags:`. */`` |
|      - | 2056 | `	{` |
|      - | 2057 | `		ph7_value *apSpec[4];` |
|     57 | 2058 | `		ph7_value *pKind = ph7_context_new_scalar(pCtx);` |
|     57 | 2059 | `		ph7_value *pMem  = ph7_context_new_scalar(pCtx);` |
|     57 | 2060 | `		ph7_value *pIdx  = ph7_context_new_scalar(pCtx);` |
|     57 | 2061 | `		if( pKind == 0 \|\| pMem == 0 \|\| pIdx == 0 ){` |
|    ! 0 | 2062 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2063 | `		}` |
|     57 | 2064 | `		ph7_value_string(pKind, "class", sizeof("class")-1);` |
|     57 | 2065 | `		ph7_value_null(pMem);` |
|     57 | 2066 | `		ph7_value_int(pIdx, 0);` |
|     57 | 2067 | `		apSpec[0] = pKind;` |
|     57 | 2068 | `		apSpec[1] = pNameVal;` |
|     57 | 2069 | `		apSpec[2] = pMem;` |
|     57 | 2070 | `		apSpec[3] = pIdx;` |
|     57 | 2071 | `		pDeclArgs = ReflectAttrArgs(pCtx, apSpec, nDecl);` |
|      - | 2072 | `	}` |
|     57 | 2073 | `	if( pDeclArgs ){` |
|     57 | 2074 | `		ph7_value *pFlags = ph7_array_fetch(pDeclArgs, "0", -1);` |
|     57 | 2075 | `		if( pFlags == 0 ){` |
|     17 | 2076 | `			pFlags = ph7_array_fetch(pDeclArgs, "flags", -1);` |
|      8 | 2077 | `		}` |
|     57 | 2078 | `		if( pFlags ){` |
|     41 | 2079 | `			iFlags = ph7_value_to_int64(pFlags);` |
|     19 | 2080 | `		}` |
|     27 | 2081 | `	}` |
|     57 | 2082 | `	iTarget = (int)PH7_NativeAttrInt(pThis, RA_TARGET);` |
|      - | 2083 | `	/* php's INTERNAL attributes carry a validator beside their mask, and the` |
|      - | 2084 | `	 * engine runs BOTH where the declaration compiles (GenStateCheckAttrPlacement)` |
|      - | 2085 | ``	 * -- so a misplaced `#[\Deprecated]` or `#[\Override]` never reaches this`` |
|      - | 2086 | `	 * far. What is left here is the USERLAND rule, which php checks only when` |
|      - | 2087 | `	 * someone asks: the mask below and the repetition test after it. */` |
|     57 | 2088 | `	if( (iFlags & iTarget) == 0 ){` |
|      - | 2089 | `		SyBlob sAllowed;` |
|      - | 2090 | `		sxi32 rc;` |
|     10 | 2091 | `		SyBlobInit(&sAllowed, &pVm->sAllocator);` |
|     66 | 2092 | `		for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azReflectTarget) ; iBit++ ){` |
|     58 | 2093 | `			if( (iFlags & ((sxi64)1 << iBit)) == 0 ){` |
|     50 | 2094 | `				continue;` |
|      - | 2095 | `			}` |
|     10 | 2096 | `			if( SyBlobLength(&sAllowed) > 0 ){` |
|    ! 0 | 2097 | `				SyBlobAppend(&sAllowed, ", ", sizeof(", ")-1);` |
|    ! 0 | 2098 | `			}` |
|     14 | 2099 | `			SyBlobAppend(&sAllowed, azReflectTarget[iBit],` |
|      8 | 2100 | `				(sxu32)SyStrlen(azReflectTarget[iBit]));` |
|      6 | 2101 | `		}` |
|     10 | 2102 | `		SyBlobAppend(&sAllowed, "", sizeof(char)); /* NUL for the %s below */` |
|     22 | 2103 | `		for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azReflectTarget) ; iBit++ ){` |
|     22 | 2104 | `			if( iTarget == (1 << iBit) ){` |
|     10 | 2105 | `				break;` |
|      - | 2106 | `			}` |
|      8 | 2107 | `		}` |
|     14 | 2108 | `		rc = PH7_VmThrowException(pCtx, "Error",` |
|      4 | 2109 | `			"Attribute \"%z\" cannot target %s (allowed targets: %s)", &pClass->sName,` |
|      4 | 2110 | `			iBit < (int)SX_ARRAYSIZE(azReflectTarget) ? azReflectTarget[iBit] : "",` |
|      4 | 2111 | `			SyBlobData(&sAllowed));` |
|     10 | 2112 | `		SyBlobRelease(&sAllowed);` |
|     10 | 2113 | `		return rc;` |
|      - | 2114 | `	}` |
|     49 | 2115 | `	if( PH7_NativeAttrTruthy(pThis, RA_REP) && (iFlags & 128) == 0 ){` |
|     11 | 2116 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      3 | 2117 | `			"Attribute \"%z\" must not be repeated", &pClass->sName);` |
|      - | 2118 | `	}` |
|     42 | 2119 | `	return ReflectAttrInstantiate(pCtx, pNameVal, ReflectAttrOwnArgs(pCtx, pThis));` |
|     34 | 2120 | `}` |
|      - | 2121 | `/*` |
|      - | 2122 | `` * php's export text. A bare `Attribute [ Name ]` when there are no arguments;`` |
|      - | 2123 | ` * otherwise the same head followed by the argument block, each argument` |
|      - | 2124 | `` * rendered in php's value-export syntax and named ones as `name = value`.`` |
|      - | 2125 | ` */` |
|     12 | 2126 | `static int vm_builtin_ReflectionAttribute_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2127 | `{` |
|     13 | 2128 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 | 2129 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     13 | 2130 | `	ph7_value *pArgs = ReflectAttrOwnArgs(pCtx, pThis);` |
|     13 | 2131 | `	const char *zName = "";` |
|     13 | 2132 | `	int nName = 0;` |
|      - | 2133 | `	SyBlob sOut;` |
|     13 | 2134 | `	sxu32 nCount = 0;` |
|      6 | 2135 | `	SXUNUSED(nArg);` |
|      6 | 2136 | `	SXUNUSED(apArg);` |
|     13 | 2137 | `	if( pThis ){` |
|     13 | 2138 | `		PH7_NativeAttrStr(pThis, RA_NAME, &zName, &nName);` |
|      6 | 2139 | `	}` |
|     13 | 2140 | `	if( pArgs && (pArgs->iFlags & MEMOBJ_HASHMAP) ){` |
|     13 | 2141 | `		nCount = ((ph7_hashmap *)pArgs->x.pOther)->nEntry;` |
|      6 | 2142 | `	}` |
|     13 | 2143 | `	SyBlobInit(&sOut, &pVm->sAllocator);` |
|     13 | 2144 | `	SyBlobAppend(&sOut, "Attribute [ ", sizeof("Attribute [ ")-1);` |
|     13 | 2145 | `	if( nName > 0 ){` |
|     13 | 2146 | `		SyBlobAppend(&sOut, zName, (sxu32)nName);` |
|      6 | 2147 | `	}` |
|     13 | 2148 | `	SyBlobAppend(&sOut, " ]", sizeof(" ]")-1);` |
|     13 | 2149 | `	if( nCount < 1 ){` |
|      3 | 2150 | `		SyBlobAppend(&sOut, "\n", sizeof(char));` |
|      2 | 2151 | `	}else{` |
|     11 | 2152 | `		ph7_hashmap *pMap = (ph7_hashmap *)pArgs->x.pOther;` |
|     11 | 2153 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|      - | 2154 | `		sxu32 n;` |
|     11 | 2155 | `		SyBlobFormat(&sOut, " {\n  - Arguments [%u] {\n", nCount);` |
|     81 | 2156 | `		for( n = 0 ; n < nCount && pEntry ; n++ ){` |
|     71 | 2157 | `			ph7_value *pMember = (ph7_value *)SySetAt(&pVm->aMemObj, pEntry->nValIdx);` |
|     71 | 2158 | `			SyBlobFormat(&sOut, "    Argument #%u [ ", n);` |
|     71 | 2159 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      7 | 2160 | `				SyBlobAppend(&sOut, SyBlobData(&pEntry->xKey.sKey),` |
|      2 | 2161 | `					SyBlobLength(&pEntry->xKey.sKey));` |
|      5 | 2162 | `				SyBlobAppend(&sOut, " = ", sizeof(" = ")-1);` |
|      2 | 2163 | `			}` |
|     71 | 2164 | `			ReflectExportValue(pCtx, &sOut, pMember, 0);` |
|     71 | 2165 | `			SyBlobAppend(&sOut, " ]\n", sizeof(" ]\n")-1);` |
|     71 | 2166 | `			pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     36 | 2167 | `		}` |
|     11 | 2168 | `		SyBlobAppend(&sOut, "  }\n}\n", sizeof("  }\n}\n")-1);` |
|      - | 2169 | `	}` |
|     13 | 2170 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     13 | 2171 | `	SyBlobRelease(&sOut);` |
|     13 | 2172 | `	return PH7_OK;` |
|      1 | 2173 | `}` |
|      - | 2174 | `/* The body of the two methods php declares PRIVATE and never calls. Neither is` |
|      - | 2175 | `` * reachable from php code: `new ReflectionAttribute` is the engine's own "Call`` |
|      - | 2176 | `` * to private … from global scope" Error, and `clone` is refused by`` |
|      - | 2177 | ` * PH7_CLASS_NOCLONE before any body runs. They exist so the class REPORTS them,` |
|      - | 2178 | ` * which is the only way php's own dump shows them too. */` |
|    ! 0 | 2179 | `static int vm_builtin_ReflectionAttribute_private(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2180 | `{` |
|    ! 0 | 2181 | `	SXUNUSED(nArg);` |
|    ! 0 | 2182 | `	SXUNUSED(apArg);` |
|    ! 0 | 2183 | `	SXUNUSED(pCtx);` |
|    ! 0 | 2184 | `	return PH7_OK;` |
|    ! 0 | 2185 | `}` |
|      - | 2186 | `/*` |
|      - | 2187 | ` * Declare ReflectionAttribute. Called from PH7_VmInstallReflection() where` |
|      - | 2188 | ` * chunk 7 used to be compiled, so Reflector (chunk 1) already exists.` |
|      - | 2189 | ` *` |
|      - | 2190 | ` * PH7_CLASS_NOCLONE is php's rule for it (php declares __clone private AND` |
|      - | 2191 | ` * refuses the copy), and the class is NOT final — php 8.5 unsealed it.` |
|      - | 2192 | ` */` |
|   5740 | 2193 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionAttribute(ph7_vm *pVm)` |
|      5 | 2194 | `{` |
|      - | 2195 | `	static const PH7_NativeConstDef aConst[] = {` |
|      - | 2196 | `		{ "IS_INSTANCEOF", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|      - | 2197 | `	};` |
|      - | 2198 | `	static const PH7_NativePropDef aProp[] = {` |
|      - | 2199 | `		{ RA_NAME,   PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 2200 | `		/* PHL-only, and PROTECTED so php code cannot reach them: the spec that` |
|      - | 2201 | `		 * reopens the target. php holds the same state on the C struct behind the` |
|      - | 2202 | `		 * object, invisible; PHL has no hidden-slot bit yet (§7.4 (e)), so these` |
|      - | 2203 | `		 * four still show up in a var_dump where php shows only $name. */` |
|      - | 2204 | `		{ RA_SPEC,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0,  0.0 }, 0 },` |
|      - | 2205 | `		{ RA_IDX,    PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0,  0.0 }, 0 },` |
|      - | 2206 | `		{ RA_TARGET, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0,  0.0 }, 0 },` |
|      - | 2207 | `		{ RA_REP,    PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0,  0.0 }, 0 },` |
|      - | 2208 | `	};` |
|      - | 2209 | `	/* php's own listing order, which is what __toString() prints. */` |
|      - | 2210 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|      - | 2211 | `		{ "getName",      PH7_MOD_PUBLIC,  "", "string", vm_builtin_ReflectionAttribute_getName },` |
|      - | 2212 | `		{ "getTarget",    PH7_MOD_PUBLIC,  "", "int",    vm_builtin_ReflectionAttribute_getTarget },` |
|      - | 2213 | `		{ "isRepeated",   PH7_MOD_PUBLIC,  "", "bool",   vm_builtin_ReflectionAttribute_isRepeated },` |
|      - | 2214 | `		{ "getArguments", PH7_MOD_PUBLIC,  "", "array",  vm_builtin_ReflectionAttribute_getArguments },` |
|      - | 2215 | `		{ "newInstance",  PH7_MOD_PUBLIC,  "", "object", vm_builtin_ReflectionAttribute_newInstance },` |
|      - | 2216 | `		{ "__toString",   PH7_MOD_PUBLIC,  "", "string", vm_builtin_ReflectionAttribute_toString },` |
|      - | 2217 | `		{ "__clone",      PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionAttribute_private },` |
|      - | 2218 | `		{ "__construct",  PH7_MOD_PRIVATE, "", 0,      vm_builtin_ReflectionAttribute_private },` |
|      - | 2219 | `	};` |
|      - | 2220 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 2221 | `		{ "ReflectionAttribute", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 2222 | `		  aMethod, SX_ARRAYSIZE(aMethod), aConst, SX_ARRAYSIZE(aConst),` |
|      - | 2223 | `		  aProp, SX_ARRAYSIZE(aProp), 0, 0, 0 },` |
|      - | 2224 | `	};` |
|   5745 | 2225 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 | 2226 | `}` |
|      - | 2227 | `/*` |
|      - | 2228 | ` * The shared getAttributes() body.` |
|      - | 2229 | ` *` |
|      - | 2230 | ` * Turns the target's #[...] records into ReflectionAttribute objects, applying` |
|      - | 2231 | ` * php's name / IS_INSTANCEOF filter. Argument VALUES stay lazy — what each` |
|      - | 2232 | ` * object carries is the [kind, target, member, paramIdx] spec that reopens` |
|      - | 2233 | ` * them — so a reflector declaring getAttributes() only has to say WHICH target` |
|      - | 2234 | ` * it is.` |
|      - | 2235 | ` */` |
|    162 | 2236 | `static int ReflectBuildAttrs(ph7_context *pCtx, SySet *pAttrs, const char *zKind,` |
|      - | 2237 | `	ph7_value *pTarget, const char *zMember, int nMember,` |
|      - | 2238 | `	int iParamIdx, int iTargetBit, int nArg, ph7_value **apArg)` |
|      4 | 2239 | `{` |
|    166 | 2240 | `	ph7_vm *pVm = pCtx->pVm;` |
|    166 | 2241 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|    166 | 2242 | `	ph7_class *pFilter = 0;` |
|    166 | 2243 | `	const char *zFilter = 0;` |
|    166 | 2244 | `	int nFilter = 0;` |
|      - | 2245 | `	ph7_value *pSpec, *pOut;` |
|      - | 2246 | `	sxu32 n, i;` |
|    166 | 2247 | `	pOut = ph7_context_new_array(pCtx);` |
|    166 | 2248 | `	pSpec = ph7_context_new_array(pCtx);` |
|    166 | 2249 | `	if( pOut == 0 \|\| pSpec == 0 ){` |
|    ! 0 | 2250 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2251 | `	}` |
|    166 | 2252 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     15 | 2253 | `		zFilter = ph7_value_to_string(apArg[0], &nFilter);` |
|     15 | 2254 | `		if( nFilter < 1 ){` |
|    ! 0 | 2255 | `			zFilter = 0;` |
|    ! 0 | 2256 | `		}` |
|      - | 2257 | `		/* IS_INSTANCEOF: keep a SUBCLASS of the named attribute too. The exact` |
|      - | 2258 | `		 * name still matches on its own, so this only has to answer for the rest. */` |
|     15 | 2259 | `		if( zFilter && nArg > 1 && (ph7_value_to_int(apArg[1]) & 2) ){` |
|      5 | 2260 | `			pFilter = ReflectResolveClass(pVm, apArg[0]);` |
|      2 | 2261 | `		}` |
|      7 | 2262 | `	}` |
|      - | 2263 | `	{` |
|    166 | 2264 | `		ph7_value *pKind = ph7_context_new_scalar(pCtx);` |
|    166 | 2265 | `		ph7_value *pMem  = ph7_context_new_scalar(pCtx);` |
|    166 | 2266 | `		ph7_value *pIdx  = ph7_context_new_scalar(pCtx);` |
|    166 | 2267 | `		if( pKind == 0 \|\| pMem == 0 \|\| pIdx == 0 ){` |
|    ! 0 | 2268 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2269 | `		}` |
|    166 | 2270 | `		ph7_value_string(pKind, zKind, -1);` |
|    166 | 2271 | `		if( zMember ){` |
|     83 | 2272 | `			ph7_value_string(pMem, zMember, nMember);` |
|     43 | 2273 | `		}else{` |
|     86 | 2274 | `			ph7_value_null(pMem);` |
|      - | 2275 | `		}` |
|    166 | 2276 | `		ph7_value_int(pIdx, iParamIdx);` |
|    166 | 2277 | `		ph7_array_add_elem(pSpec, 0, pKind);` |
|      - | 2278 | `		/* The target rides as a VALUE, not a name: a closure's attributes are` |
|      - | 2279 | `		 * reopened through the Closure object itself. */` |
|    166 | 2280 | `		ph7_array_add_elem(pSpec, 0, pTarget);` |
|    166 | 2281 | `		ph7_array_add_elem(pSpec, 0, pMem);` |
|    166 | 2282 | `		ph7_array_add_elem(pSpec, 0, pIdx);` |
|      - | 2283 | `	}` |
|    350 | 2284 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|    188 | 2285 | `		int bRepeated = 0;` |
|    188 | 2286 | `		if( zFilter ){` |
|     89 | 2287 | `			int bKeep = ((int)SyStringLength(&aA[n].sName) == nFilter` |
|     50 | 2288 | `				&& SyStrnicmp(SyStringData(&aA[n].sName), zFilter, (sxu32)nFilter) == 0);` |
|     51 | 2289 | `			if( !bKeep && pFilter ){` |
|     16 | 2290 | `				ph7_class *pCand = PH7_VmExtractClass(pVm, SyStringData(&aA[n].sName),` |
|     10 | 2291 | `					SyStringLength(&aA[n].sName), FALSE, 0);` |
|     11 | 2292 | `				if( pCand == 0 ){` |
|    ! 0 | 2293 | `					pCand = PH7_VmTriggerAutoload(pVm, SyStringData(&aA[n].sName),` |
|    ! 0 | 2294 | `						SyStringLength(&aA[n].sName), FALSE);` |
|    ! 0 | 2295 | `				}` |
|     11 | 2296 | `				bKeep = pCand != 0 && pCand != pFilter && PH7_VmInstanceOf(pCand, pFilter);` |
|      5 | 2297 | `			}` |
|     51 | 2298 | `			if( !bKeep ){` |
|     27 | 2299 | `				continue;` |
|      - | 2300 | `			}` |
|     12 | 2301 | `		}` |
|      - | 2302 | `		/* isRepeated() asks about the TARGET, not about the filtered result:` |
|      - | 2303 | `		 * two #[A] make both of them repeated even when only one is asked for. */` |
|    354 | 2304 | `		for( i = 0 ; i < SySetUsed(pAttrs) ; i++ ){` |
|    232 | 2305 | `			if( i != n && SyStringLength(&aA[i].sName) == SyStringLength(&aA[n].sName)` |
|     74 | 2306 | `			 && SyStrnicmp(SyStringData(&aA[i].sName), SyStringData(&aA[n].sName),` |
|     66 | 2307 | `				SyStringLength(&aA[n].sName)) == 0 ){` |
|     42 | 2308 | `				bRepeated = 1;` |
|     42 | 2309 | `				break;` |
|      - | 2310 | `			}` |
|    100 | 2311 | `		}` |
|    241 | 2312 | `		ReflectTypeListAdd(pCtx, pOut,` |
|    158 | 2313 | `			ReflectAttrNew(pCtx, &aA[n].sName, pSpec, n, iTargetBit, bRepeated));` |
|     83 | 2314 | `	}` |
|    166 | 2315 | `	ph7_result_value(pCtx, pOut);` |
|    166 | 2316 | `	return PH7_OK;` |
|     85 | 2317 | `}` |
|      - | 2318 | `/*` |
|      - | 2319 | ` * The three "no runtime line tracking" methods. php answers a file/line/trace` |
|      - | 2320 | ` * from the executing frame; PHL has no per-instruction line record (the same` |
|      - | 2321 | ` * gap debug_backtrace() has), so it says so loudly rather than inventing one.` |
|      - | 2322 | ` */` |
|    ! 0 | 2323 | `static int ReflectUnsupported(ph7_context *pCtx, const char *zWho)` |
|    ! 0 | 2324 | `{` |
|    ! 0 | 2325 | `	return PH7_VmThrowException(pCtx, "Error",` |
|    ! 0 | 2326 | `		"%s is not supported by PHL (no runtime line tracking)", zWho);` |
|    ! 0 | 2327 | `}` |
|      - | 2328 | `#define REFLECT_UNSUPPORTED(NAME,TEXT) \` |
|      - | 2329 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 2330 | `	{ \` |
|      - | 2331 | `		SXUNUSED(nArg); \` |
|      - | 2332 | `		SXUNUSED(apArg); \` |
|      - | 2333 | `		return ReflectUnsupported(pCtx, TEXT); \` |
|      - | 2334 | `	}` |
|    ! 0 | 2335 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_execLine,"ReflectionGenerator::getExecutingLine()")` |
|    ! 0 | 2336 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_execFile,"ReflectionGenerator::getExecutingFile()")` |
|    ! 0 | 2337 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_trace,"ReflectionGenerator::getTrace()")` |
|    ! 0 | 2338 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_execLine,"ReflectionFiber::getExecutingLine()")` |
|    ! 0 | 2339 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_execFile,"ReflectionFiber::getExecutingFile()")` |
|    ! 0 | 2340 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_trace,"ReflectionFiber::getTrace()")` |
|      - | 2341 |  |
|      - | 2342 | `/* ReflectionGenerator::__construct(Generator $generator) */` |
|     16 | 2343 | `static int vm_builtin_ReflectionGenerator_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2344 | `{` |
|     17 | 2345 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     17 | 2346 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 2347 | `		return PH7_OK;` |
|      - | 2348 | `	}` |
|     17 | 2349 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RG_GEN, (ph7_class_instance *)apArg[0]->x.pOther);` |
|     17 | 2350 | `	return PH7_OK;` |
|      9 | 2351 | `}` |
|      - | 2352 | `/* The coroutine context of the wrapped generator, or NULL. */` |
|     34 | 2353 | `static ph7_exec_ctx * ReflectGenExec(ph7_context *pCtx)` |
|      1 | 2354 | `{` |
|     35 | 2355 | `	ph7_class_instance *pGenObj = ReflectWrapped(pCtx, RG_GEN);` |
|      - | 2356 | `	ph7_value sVal;` |
|      - | 2357 | `	ph7_generator *pGen;` |
|     35 | 2358 | `	if( pGenObj == 0 ){` |
|    ! 0 | 2359 | `		return 0;` |
|      - | 2360 | `	}` |
|     35 | 2361 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     35 | 2362 | `	sVal.x.pOther = pGenObj;` |
|     35 | 2363 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|     35 | 2364 | `	pGen = ReflectGeneratorCtx(pCtx->pVm, &sVal);` |
|     35 | 2365 | `	return pGen ? pGen->pCtx : 0;` |
|     18 | 2366 | `}` |
|      8 | 2367 | `static int vm_builtin_ReflectionGenerator_isClosed(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2368 | `{` |
|      9 | 2369 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|      4 | 2370 | `	SXUNUSED(nArg);` |
|      4 | 2371 | `	SXUNUSED(apArg);` |
|     17 | 2372 | `	ph7_result_bool(pCtx, pExec != 0` |
|     12 | 2373 | `		&& (pExec->iState == PH7_CTX_STATE_COMPLETED \|\| pExec->iState == PH7_CTX_STATE_CLOSED));` |
|      9 | 2374 | `	return PH7_OK;` |
|      1 | 2375 | `}` |
|      - | 2376 | `/* ReflectionGenerator::getThis(): ?object — the receiver the generator's body` |
|      - | 2377 | ` * runs against. A coroutine frame installs it as a frame VARIABLE (see` |
|      - | 2378 | ` * VmFiberSetupFrame), not as pFrame->pThis, so both are checked. */` |
|      8 | 2379 | `static int vm_builtin_ReflectionGenerator_getThis(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2380 | `{` |
|      9 | 2381 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|      4 | 2382 | `	SXUNUSED(nArg);` |
|      4 | 2383 | `	SXUNUSED(apArg);` |
|      9 | 2384 | `	if( pExec && pExec->pFrame ){` |
|      9 | 2385 | `		SyHashEntry *pVar = SyHashGet(&pExec->pFrame->hVar, "this", sizeof("this")-1);` |
|      9 | 2386 | `		if( pVar ){` |
|      7 | 2387 | `			ph7_value *pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,` |
|      4 | 2388 | `				(sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|      5 | 2389 | `			if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|      5 | 2390 | `				ph7_result_value(pCtx, pSlot);` |
|      5 | 2391 | `				return PH7_OK;` |
|      - | 2392 | `			}` |
|    ! 0 | 2393 | `		}` |
|      5 | 2394 | `		if( pExec->pFrame->pThis ){` |
|    ! 0 | 2395 | `			return ReflectResultBorrowed(pCtx, pExec->pFrame->pThis);` |
|      - | 2396 | `		}` |
|      2 | 2397 | `	}` |
|      5 | 2398 | `	ph7_result_null(pCtx);` |
|      5 | 2399 | `	return PH7_OK;` |
|      5 | 2400 | `}` |
|      - | 2401 | `/* ReflectionGenerator::getFunction(): ReflectionFunctionAbstract — still a` |
|      - | 2402 | ` * prelude class, so it is built through its own constructor. */` |
|     18 | 2403 | `static int vm_builtin_ReflectionGenerator_getFunction(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2404 | `{` |
|     19 | 2405 | `	ph7_vm *pVm = pCtx->pVm;` |
|     19 | 2406 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|     19 | 2407 | `	ph7_vm_func *pFunc = pExec ? pExec->pFunc : 0;` |
|      - | 2408 | `	ph7_class_instance *pOut;` |
|      - | 2409 | `	ph7_value aArg[2];` |
|     19 | 2410 | `	int nCtorArg = 1;` |
|      - | 2411 | `	sxi32 rc;` |
|      9 | 2412 | `	SXUNUSED(nArg);` |
|      9 | 2413 | `	SXUNUSED(apArg);` |
|     19 | 2414 | `	if( pFunc == 0 ){` |
|    ! 0 | 2415 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2416 | `		return PH7_OK;` |
|      - | 2417 | `	}` |
|     19 | 2418 | `	PH7_MemObjInit(pVm, &aArg[0]);` |
|     19 | 2419 | `	PH7_MemObjInit(pVm, &aArg[1]);` |
|     19 | 2420 | `	if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      9 | 2421 | `		ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|      9 | 2422 | `		ph7_value_string(&aArg[0], SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|      9 | 2423 | `		ph7_value_string(&aArg[1], SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|      9 | 2424 | `		nCtorArg = 2;` |
|      5 | 2425 | `	}else{` |
|     11 | 2426 | `		ph7_value_string(&aArg[0], SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|      - | 2427 | `	}` |
|      - | 2428 | `	{` |
|      - | 2429 | `		ph7_value *apCtor[2];` |
|     19 | 2430 | `		apCtor[0] = &aArg[0];` |
|     19 | 2431 | `		apCtor[1] = &aArg[1];` |
|     28 | 2432 | `		pOut = ReflectConstruct(pCtx, nCtorArg == 2 ? "ReflectionMethod" : "ReflectionFunction",` |
|      9 | 2433 | `			nCtorArg, apCtor, &rc);` |
|      - | 2434 | `	}` |
|     19 | 2435 | `	PH7_MemObjRelease(&aArg[0]);` |
|     19 | 2436 | `	PH7_MemObjRelease(&aArg[1]);` |
|     19 | 2437 | `	if( pOut == 0 ){` |
|    ! 0 | 2438 | `		if( rc != PH7_OK ){` |
|    ! 0 | 2439 | `			return rc;` |
|      - | 2440 | `		}` |
|    ! 0 | 2441 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2442 | `		return PH7_OK;` |
|      - | 2443 | `	}` |
|     19 | 2444 | `	return ReflectResultObject(pCtx, pOut);` |
|     10 | 2445 | `}` |
|      - | 2446 | `` /* ReflectionGenerator::getExecutingGenerator(): Generator — follow `yield from` `` |
|      - | 2447 | ` * delegation to the innermost one that is actually running. */` |
|      6 | 2448 | `static int vm_builtin_ReflectionGenerator_getExecuting(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2449 | `{` |
|      7 | 2450 | `	ph7_vm *pVm = pCtx->pVm;` |
|      7 | 2451 | `	ph7_class_instance *pGenObj = ReflectWrapped(pCtx, RG_GEN);` |
|      - | 2452 | `	ph7_value sVal, *pCur;` |
|      - | 2453 | `	ph7_generator *pGen;` |
|      7 | 2454 | `	int iDepth = 0;` |
|      3 | 2455 | `	SXUNUSED(nArg);` |
|      3 | 2456 | `	SXUNUSED(apArg);` |
|      7 | 2457 | `	if( pGenObj == 0 ){` |
|    ! 0 | 2458 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2459 | `		return PH7_OK;` |
|      - | 2460 | `	}` |
|      7 | 2461 | `	PH7_MemObjInit(pVm, &sVal);` |
|      7 | 2462 | `	sVal.x.pOther = pGenObj;` |
|      7 | 2463 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|      7 | 2464 | `	pCur = &sVal;` |
|      7 | 2465 | `	pGen = ReflectGeneratorCtx(pVm, &sVal);` |
|     12 | 2466 | `	while( pGen && pGen->pCtx && pGen->pCtx->iDelegateState == 3` |
|     10 | 2467 | `	 && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|      3 | 2468 | `		ph7_generator *pInner = ReflectGeneratorCtx(pVm, &pGen->pCtx->sDelegate);` |
|      3 | 2469 | `		if( pInner == 0 ){` |
|    ! 0 | 2470 | `			break;` |
|      - | 2471 | `		}` |
|      3 | 2472 | `		pCur = &pGen->pCtx->sDelegate;` |
|      3 | 2473 | `		pGen = pInner;` |
|      3 | 2474 | `		iDepth++;` |
|      1 | 2475 | `	}` |
|      7 | 2476 | `	return ReflectResultBorrowed(pCtx, (ph7_class_instance *)pCur->x.pOther);` |
|      4 | 2477 | `}` |
|      - | 2478 | `/* ReflectionFiber::__construct(Fiber $fiber) / getFiber() / getCallable() */` |
|      4 | 2479 | `static int vm_builtin_ReflectionFiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2480 | `{` |
|      5 | 2481 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 | 2482 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 2483 | `		return PH7_OK;` |
|      - | 2484 | `	}` |
|      5 | 2485 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RF_FIBER, (ph7_class_instance *)apArg[0]->x.pOther);` |
|      5 | 2486 | `	return PH7_OK;` |
|      3 | 2487 | `}` |
|      4 | 2488 | `static int vm_builtin_ReflectionFiber_getFiber(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2489 | `{` |
|      2 | 2490 | `	SXUNUSED(nArg);` |
|      2 | 2491 | `	SXUNUSED(apArg);` |
|      5 | 2492 | `	return ReflectResultBorrowed(pCtx, ReflectWrapped(pCtx, RF_FIBER));` |
|      1 | 2493 | `}` |
|      4 | 2494 | `static int vm_builtin_ReflectionFiber_getCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2495 | `{` |
|      5 | 2496 | `	ph7_class_instance *pFiber = ReflectWrapped(pCtx, RF_FIBER);` |
|      5 | 2497 | `	ph7_value *pVal = pFiber ? PH7_NativeAttr(pFiber, "__callable") : 0;` |
|      2 | 2498 | `	SXUNUSED(nArg);` |
|      2 | 2499 | `	SXUNUSED(apArg);` |
|      5 | 2500 | `	if( pVal ){` |
|      5 | 2501 | `		ph7_result_value(pCtx, pVal);` |
|      2 | 2502 | `	}` |
|      5 | 2503 | `	return PH7_OK;` |
|      1 | 2504 | `}` |
|      - | 2505 | `/* ---- ReflectionConstant ---- */` |
|      - | 2506 | `/* The engine's record for a global constant, or NULL when undefined. */` |
|     60 | 2507 | `static ph7_constant * ReflectConstEntry(ph7_vm *pVm, const char *zName, int nName)` |
|      1 | 2508 | `{` |
|     61 | 2509 | `	SyHashEntry *pEntry = nName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zName, (sxu32)nName) : 0;` |
|     61 | 2510 | `	return pEntry ? (ph7_constant *)pEntry->pUserData : 0;` |
|      1 | 2511 | `}` |
|      - | 2512 | `/* The receiver's constant record, resolved from its public $name. */` |
|     34 | 2513 | `static ph7_constant * ReflectConstOf(ph7_context *pCtx)` |
|      1 | 2514 | `{` |
|     35 | 2515 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     35 | 2516 | `	const char *zName = "";` |
|     35 | 2517 | `	int nName = 0;` |
|     35 | 2518 | `	if( pThis ){` |
|     35 | 2519 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     17 | 2520 | `	}` |
|     35 | 2521 | `	return ReflectConstEntry(pCtx->pVm, zName, nName);` |
|      1 | 2522 | `}` |
|     26 | 2523 | `static int vm_builtin_ReflectionConstant_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2524 | `{` |
|     27 | 2525 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     27 | 2526 | `	int nName = 0;` |
|     27 | 2527 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|     27 | 2528 | `	if( pThis == 0 ){` |
|    ! 0 | 2529 | `		return PH7_OK;` |
|      - | 2530 | `	}` |
|     27 | 2531 | `	if( ReflectConstEntry(pCtx->pVm, zName, nName) == 0 ){` |
|     10 | 2532 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 | 2533 | `			"Constant \"%.*s\" does not exist", nName, zName);` |
|      - | 2534 | `	}` |
|     21 | 2535 | `	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", zName, nName);` |
|     21 | 2536 | `	return PH7_OK;` |
|     14 | 2537 | `}` |
|      4 | 2538 | `static int vm_builtin_ReflectionConstant_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2539 | `{` |
|      5 | 2540 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 | 2541 | `	const char *zName = "";` |
|      5 | 2542 | `	int nName = 0;` |
|      2 | 2543 | `	SXUNUSED(nArg);` |
|      2 | 2544 | `	SXUNUSED(apArg);` |
|      5 | 2545 | `	if( pThis ){` |
|      5 | 2546 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 | 2547 | `	}` |
|      5 | 2548 | `	ph7_result_string(pCtx, zName, nName);` |
|      5 | 2549 | `	return PH7_OK;` |
|      1 | 2550 | `}` |
|      - | 2551 | `/* The namespace split php reports: everything before the last '\', and the rest. */` |
|      8 | 2552 | `static int ReflectConstNsCut(const char *zName, int nName)` |
|      1 | 2553 | `{` |
|      - | 2554 | `	int k;` |
|    101 | 2555 | `	for( k = nName - 1 ; k >= 0 ; k-- ){` |
|     93 | 2556 | `		if( zName[k] == '\\' ){` |
|    ! 0 | 2557 | `			return k;` |
|      - | 2558 | `		}` |
|     47 | 2559 | `	}` |
|      9 | 2560 | `	return -1;` |
|      5 | 2561 | `}` |
|      4 | 2562 | `static int vm_builtin_ReflectionConstant_getNamespaceName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2563 | `{` |
|      5 | 2564 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 | 2565 | `	const char *zName = "";` |
|      5 | 2566 | `	int nName = 0, iCut;` |
|      2 | 2567 | `	SXUNUSED(nArg);` |
|      2 | 2568 | `	SXUNUSED(apArg);` |
|      5 | 2569 | `	if( pThis ){` |
|      5 | 2570 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 | 2571 | `	}` |
|      5 | 2572 | `	iCut = ReflectConstNsCut(zName, nName);` |
|      5 | 2573 | `	ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|      5 | 2574 | `	return PH7_OK;` |
|      1 | 2575 | `}` |
|      4 | 2576 | `static int vm_builtin_ReflectionConstant_getShortName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2577 | `{` |
|      5 | 2578 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 | 2579 | `	const char *zName = "";` |
|      5 | 2580 | `	int nName = 0, iCut;` |
|      2 | 2581 | `	SXUNUSED(nArg);` |
|      2 | 2582 | `	SXUNUSED(apArg);` |
|      5 | 2583 | `	if( pThis ){` |
|      5 | 2584 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 | 2585 | `	}` |
|      5 | 2586 | `	iCut = ReflectConstNsCut(zName, nName);` |
|      5 | 2587 | `	if( iCut < 0 ){` |
|      5 | 2588 | `		ph7_result_string(pCtx, zName, nName);` |
|      3 | 2589 | `	}else{` |
|    ! 0 | 2590 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|      - | 2591 | `	}` |
|      5 | 2592 | `	return PH7_OK;` |
|      1 | 2593 | `}` |
|      8 | 2594 | `static int vm_builtin_ReflectionConstant_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2595 | `{` |
|      9 | 2596 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      - | 2597 | `	ph7_value sValue;` |
|      4 | 2598 | `	SXUNUSED(nArg);` |
|      4 | 2599 | `	SXUNUSED(apArg);` |
|      9 | 2600 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|      9 | 2601 | `	if( pCons && pCons->xExpand ){` |
|      9 | 2602 | `		pCons->xExpand(&sValue, pCons->pUserData);` |
|      4 | 2603 | `	}` |
|      9 | 2604 | `	ph7_result_value(pCtx, &sValue);` |
|      9 | 2605 | `	PH7_MemObjRelease(&sValue);` |
|      9 | 2606 | `	return PH7_OK;` |
|      1 | 2607 | `}` |
|      4 | 2608 | `static int vm_builtin_ReflectionConstant_isDeprecated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2609 | `{` |
|      2 | 2610 | `	SXUNUSED(nArg);` |
|      2 | 2611 | `	SXUNUSED(apArg);` |
|      5 | 2612 | `	ph7_result_bool(pCtx, 0);` |
|      5 | 2613 | `	return PH7_OK;` |
|      1 | 2614 | `}` |
|      8 | 2615 | `static int vm_builtin_ReflectionConstant_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2616 | `{` |
|      9 | 2617 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      4 | 2618 | `	SXUNUSED(nArg);` |
|      4 | 2619 | `	SXUNUSED(apArg);` |
|      9 | 2620 | `	if( pCons && SyStringLength(&pCons->sFile) > 0 ){` |
|      5 | 2621 | `		ph7_result_string(pCtx, SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));` |
|      3 | 2622 | `	}else{` |
|      5 | 2623 | `		ph7_result_bool(pCtx, 0);` |
|      - | 2624 | `	}` |
|      9 | 2625 | `	return PH7_OK;` |
|      1 | 2626 | `}` |
|      - | 2627 | `/* An engine constant belongs to the synthetic "Core" extension; a userland` |
|      - | 2628 | ` * define() belongs to none, which php reports as null / false. */` |
|      8 | 2629 | `static int vm_builtin_ReflectionConstant_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2630 | `{` |
|      9 | 2631 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      - | 2632 | `	ph7_class_instance *pExt;` |
|      - | 2633 | `	ph7_value sName, *apArgs[1];` |
|      - | 2634 | `	sxi32 rc;` |
|      4 | 2635 | `	SXUNUSED(nArg);` |
|      4 | 2636 | `	SXUNUSED(apArg);` |
|      9 | 2637 | `	if( pCons == 0 \|\| pCons->bUserDefined ){` |
|      3 | 2638 | `		ph7_result_null(pCtx);` |
|      3 | 2639 | `		return PH7_OK;` |
|      - | 2640 | `	}` |
|      7 | 2641 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|      7 | 2642 | `	ph7_value_string(&sName, "Core", 4);` |
|      7 | 2643 | `	apArgs[0] = &sName;` |
|      7 | 2644 | `	pExt = ReflectConstruct(pCtx, "ReflectionExtension", 1, apArgs, &rc);` |
|      7 | 2645 | `	PH7_MemObjRelease(&sName);` |
|      7 | 2646 | `	if( pExt == 0 ){` |
|    ! 0 | 2647 | `		if( rc != PH7_OK ){` |
|    ! 0 | 2648 | `			return rc;` |
|      - | 2649 | `		}` |
|    ! 0 | 2650 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2651 | `		return PH7_OK;` |
|      - | 2652 | `	}` |
|      7 | 2653 | `	return ReflectResultObject(pCtx, pExt);` |
|      5 | 2654 | `}` |
|      8 | 2655 | `static int vm_builtin_ReflectionConstant_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2656 | `{` |
|      9 | 2657 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      4 | 2658 | `	SXUNUSED(nArg);` |
|      4 | 2659 | `	SXUNUSED(apArg);` |
|      9 | 2660 | `	if( pCons && pCons->bUserDefined == 0 ){` |
|      5 | 2661 | `		ph7_result_string(pCtx, "Core", 4);` |
|      3 | 2662 | `	}else{` |
|      5 | 2663 | `		ph7_result_bool(pCtx, 0);` |
|      - | 2664 | `	}` |
|      9 | 2665 | `	return PH7_OK;` |
|      1 | 2666 | `}` |
|      - | 2667 | `/* getAttributes() still routes through the prelude builder: chunk 7 owns the` |
|      - | 2668 | ` * ReflectionAttribute shape and the lazy argument evaluation. */` |
|      2 | 2669 | `static int vm_builtin_ReflectionConstant_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2670 | `{` |
|      3 | 2671 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      3 | 2672 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      3 | 2673 | `	const char *zName = "";` |
|      3 | 2674 | `	int nName = 0;` |
|      3 | 2675 | `	if( pThis == 0 \|\| pCons == 0 ){` |
|    ! 0 | 2676 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 | 2677 | `		return PH7_OK;` |
|      - | 2678 | `	}` |
|      3 | 2679 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      - | 2680 | `	{` |
|      - | 2681 | `		ph7_value sTarget;` |
|      - | 2682 | `		int rc;` |
|      3 | 2683 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      3 | 2684 | `		ph7_value_string(&sTarget, zName, nName);` |
|      - | 2685 | `		/* 64 = Attribute::TARGET_CONSTANT */` |
|      4 | 2686 | `		rc = ReflectBuildAttrs(pCtx, &pCons->aAttrs, "const", &sTarget, 0, 0, 0, 64,` |
|      1 | 2687 | `			nArg, apArg);` |
|      3 | 2688 | `		PH7_MemObjRelease(&sTarget);` |
|      3 | 2689 | `		return rc;` |
|      - | 2690 | `	}` |
|      2 | 2691 | `}` |
|    ! 0 | 2692 | `static int vm_builtin_ReflectionConstant_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2693 | `{` |
|    ! 0 | 2694 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    ! 0 | 2695 | `	const char *zName = "";` |
|    ! 0 | 2696 | `	int nName = 0;` |
|    ! 0 | 2697 | `	SXUNUSED(nArg);` |
|    ! 0 | 2698 | `	SXUNUSED(apArg);` |
|    ! 0 | 2699 | `	if( pThis ){` |
|    ! 0 | 2700 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    ! 0 | 2701 | `	}` |
|    ! 0 | 2702 | `	ph7_result_string_format(pCtx, "Constant [ %.*s ]\n", nName, zName);` |
|    ! 0 | 2703 | `	return PH7_OK;` |
|    ! 0 | 2704 | `}` |
|      - | 2705 | `/* ---- ReflectionExtension: PHL has exactly one, the synthetic "Core" ---- */` |
|     24 | 2706 | `static int vm_builtin_ReflectionExtension_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2707 | `{` |
|     25 | 2708 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     25 | 2709 | `	int nName = 0;` |
|     25 | 2710 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|     25 | 2711 | `	if( pThis == 0 ){` |
|    ! 0 | 2712 | `		return PH7_OK;` |
|      - | 2713 | `	}` |
|     34 | 2714 | `	if( !(nName == 4 && SyToLower(zName[0]) == 'c' && SyToLower(zName[1]) == 'o'` |
|     18 | 2715 | `	   && SyToLower(zName[2]) == 'r' && SyToLower(zName[3]) == 'e') ){` |
|     10 | 2716 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 | 2717 | `			"Extension \"%.*s\" does not exist", nName, zName);` |
|      - | 2718 | `	}` |
|     19 | 2719 | `	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", "Core", 4);` |
|     19 | 2720 | `	return PH7_OK;` |
|     13 | 2721 | `}` |
|     10 | 2722 | `static int vm_builtin_ReflectionExtension_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2723 | `{` |
|     11 | 2724 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     11 | 2725 | `	const char *zName = "";` |
|     11 | 2726 | `	int nName = 0;` |
|      5 | 2727 | `	SXUNUSED(nArg);` |
|      5 | 2728 | `	SXUNUSED(apArg);` |
|     11 | 2729 | `	if( pThis ){` |
|     11 | 2730 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      5 | 2731 | `	}` |
|     11 | 2732 | `	ph7_result_string(pCtx, zName, nName);` |
|     11 | 2733 | `	return PH7_OK;` |
|      1 | 2734 | `}` |
|    ! 0 | 2735 | `static int vm_builtin_ReflectionExtension_getVersion(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2736 | `{` |
|      - | 2737 | `	ph7_value sFn, sRes;` |
|      - | 2738 | `	SyString sStr;` |
|    ! 0 | 2739 | `	SXUNUSED(nArg);` |
|    ! 0 | 2740 | `	SXUNUSED(apArg);` |
|    ! 0 | 2741 | `	PH7_MemObjInit(pCtx->pVm, &sFn);` |
|    ! 0 | 2742 | `	PH7_MemObjInit(pCtx->pVm, &sRes);` |
|    ! 0 | 2743 | `	SyStringInitFromBuf(&sStr, "phpversion", sizeof("phpversion")-1);` |
|    ! 0 | 2744 | `	PH7_MemObjInitFromString(pCtx->pVm, &sFn, &sStr);` |
|    ! 0 | 2745 | `	if( PH7_VmCallUserFunction(pCtx->pVm, &sFn, 0, 0, &sRes) == SXRET_OK ){` |
|    ! 0 | 2746 | `		ph7_result_value(pCtx, &sRes);` |
|    ! 0 | 2747 | `	}` |
|    ! 0 | 2748 | `	PH7_MemObjRelease(&sFn);` |
|    ! 0 | 2749 | `	PH7_MemObjRelease(&sRes);` |
|    ! 0 | 2750 | `	return PH7_OK;` |
|    ! 0 | 2751 | `}` |
|    ! 0 | 2752 | `static int vm_builtin_ReflectionExtension_emptyArray(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2753 | `{` |
|    ! 0 | 2754 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|    ! 0 | 2755 | `	SXUNUSED(nArg);` |
|    ! 0 | 2756 | `	SXUNUSED(apArg);` |
|    ! 0 | 2757 | `	if( pList ){` |
|    ! 0 | 2758 | `		ph7_result_value(pCtx, pList);` |
|    ! 0 | 2759 | `	}` |
|    ! 0 | 2760 | `	return PH7_OK;` |
|    ! 0 | 2761 | `}` |
|    ! 0 | 2762 | `static int vm_builtin_ReflectionExtension_isPersistent(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2763 | `{` |
|    ! 0 | 2764 | `	SXUNUSED(nArg);` |
|    ! 0 | 2765 | `	SXUNUSED(apArg);` |
|    ! 0 | 2766 | `	ph7_result_bool(pCtx, 1);` |
|    ! 0 | 2767 | `	return PH7_OK;` |
|    ! 0 | 2768 | `}` |
|    ! 0 | 2769 | `static int vm_builtin_ReflectionExtension_isTemporary(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2770 | `{` |
|    ! 0 | 2771 | `	SXUNUSED(nArg);` |
|    ! 0 | 2772 | `	SXUNUSED(apArg);` |
|    ! 0 | 2773 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 | 2774 | `	return PH7_OK;` |
|    ! 0 | 2775 | `}` |
|    ! 0 | 2776 | `static int vm_builtin_ReflectionExtension_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2777 | `{` |
|    ! 0 | 2778 | `	SXUNUSED(nArg);` |
|    ! 0 | 2779 | `	SXUNUSED(apArg);` |
|    ! 0 | 2780 | `	SXUNUSED(pCtx);` |
|    ! 0 | 2781 | `	return PH7_OK;` |
|    ! 0 | 2782 | `}` |
|    ! 0 | 2783 | `static int vm_builtin_ReflectionExtension_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2784 | `{` |
|    ! 0 | 2785 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    ! 0 | 2786 | `	const char *zName = "";` |
|    ! 0 | 2787 | `	int nName = 0;` |
|    ! 0 | 2788 | `	SXUNUSED(nArg);` |
|    ! 0 | 2789 | `	SXUNUSED(apArg);` |
|    ! 0 | 2790 | `	if( pThis ){` |
|    ! 0 | 2791 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    ! 0 | 2792 | `	}` |
|    ! 0 | 2793 | `	ph7_result_string_format(pCtx, "Extension [ extension #1 %.*s ]\n", nName, zName);` |
|    ! 0 | 2794 | `	return PH7_OK;` |
|    ! 0 | 2795 | `}` |
|      - | 2796 | `/* ---- ReflectionZendExtension: none exist, so the constructor always refuses ---- */` |
|      4 | 2797 | `static int vm_builtin_ReflectionZendExtension_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2798 | `{` |
|      5 | 2799 | `	int nName = 0;` |
|      5 | 2800 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|      7 | 2801 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 | 2802 | `		"Zend Extension \"%.*s\" does not exist", nName, zName);` |
|      1 | 2803 | `}` |
|    ! 0 | 2804 | `static int vm_builtin_ReflectionZendExtension_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2805 | `{` |
|    ! 0 | 2806 | `	SXUNUSED(nArg);` |
|    ! 0 | 2807 | `	SXUNUSED(apArg);` |
|    ! 0 | 2808 | `	ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 2809 | `	return PH7_OK;` |
|    ! 0 | 2810 | `}` |
|      - | 2811 | `/* ---- ReflectionReference ---- */` |
|      - | 2812 | `/*` |
|      - | 2813 | ` * ReflectionReference::fromArrayElement(array $array, string\|int $key)` |
|      - | 2814 | ` *` |
|      - | 2815 | ` * Answers a reflector only when the element IS a reference — its slot carries a` |
|      - | 2816 | ` * reference-table record with at least two links. The instance is built without` |
|      - | 2817 | ` * running the (private) constructor, exactly as php's factory does.` |
|      - | 2818 | ` */` |
|     18 | 2819 | `static int vm_builtin_ReflectionReference_fromArrayElement(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2820 | `{` |
|     19 | 2821 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 2822 | `	ph7_hashmap *pMap;` |
|     19 | 2823 | `	ph7_hashmap_node *pNode = 0;` |
|      - | 2824 | `	ph7_class *pClass;` |
|      - | 2825 | `	ph7_class_instance *pObj;` |
|      - | 2826 | `	char zId[64];` |
|     19 | 2827 | `	if( nArg < 1 ){` |
|    ! 0 | 2828 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2829 | `		return PH7_OK;` |
|      - | 2830 | `	}` |
|     19 | 2831 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2832 | ``		/* The shared ZPP screen leaves a SCALAR against a bare `array` parameter`` |
|      - | 2833 | `		 * unscreened (it only judges array/object/null/resource pairings and` |
|      - | 2834 | `		 * scalars against class-typed ones), so the refusal php words from the` |
|      - | 2835 | `		 * declared type is written here -- as the prelude's own is_array() check` |
|      - | 2836 | `		 * did. Recorded in PLAN §2 with the rest of that gap. */` |
|    ! 0 | 2837 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|      - | 2838 | `			"ReflectionReference::fromArrayElement(): Argument #1 ($array) "` |
|    ! 0 | 2839 | `			"must be of type array, %s given", ph7_type_name(apArg[0]));` |
|      - | 2840 | `	}` |
|     19 | 2841 | `	if( nArg < 2 ){` |
|    ! 0 | 2842 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2843 | `		return PH7_OK;` |
|      - | 2844 | `	}` |
|     19 | 2845 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     19 | 2846 | `	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK \|\| pNode == 0 ){` |
|    ! 0 | 2847 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2848 | `		return PH7_OK;` |
|      - | 2849 | `	}` |
|     19 | 2850 | `	if( PH7_VmSlotRefCount(pVm, pNode->nValIdx) < 2 ){` |
|      7 | 2851 | `		ph7_result_null(pCtx);` |
|      7 | 2852 | `		return PH7_OK;` |
|      - | 2853 | `	}` |
|     13 | 2854 | `	pClass = PH7_VmExtractClass(pVm, "ReflectionReference", sizeof("ReflectionReference")-1, FALSE, 0);` |
|     13 | 2855 | `	pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|     13 | 2856 | `	if( pObj == 0 ){` |
|    ! 0 | 2857 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2858 | `	}` |
|      - | 2859 | `	/* php's ids are opaque strings; the slot index is the identity PHL has. */` |
|     13 | 2860 | `	SyBufferFormat(zId, sizeof(zId), "phlref%u", (unsigned)pNode->nValIdx);` |
|     13 | 2861 | `	PH7_NativeSetAttrStr(pVm, pObj, RR_ID, zId, (int)SyStrlen(zId));` |
|     13 | 2862 | `	return ReflectResultObject(pCtx, pObj);` |
|     10 | 2863 | `}` |
|     12 | 2864 | `static int vm_builtin_ReflectionReference_getId(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2865 | `{` |
|     13 | 2866 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     13 | 2867 | `	const char *zId = "";` |
|     13 | 2868 | `	int nId = 0;` |
|      6 | 2869 | `	SXUNUSED(nArg);` |
|      6 | 2870 | `	SXUNUSED(apArg);` |
|     13 | 2871 | `	if( pThis ){` |
|     13 | 2872 | `		PH7_NativeAttrStr(pThis, RR_ID, &zId, &nId);` |
|      6 | 2873 | `	}` |
|     13 | 2874 | `	ph7_result_string(pCtx, zId, nId);` |
|     13 | 2875 | `	return PH7_OK;` |
|      1 | 2876 | `}` |
|      - | 2877 | `/* php declares this private and never calls it; reaching it is the diagnostic. */` |
|    ! 0 | 2878 | `static int vm_builtin_ReflectionReference_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2879 | `{` |
|    ! 0 | 2880 | `	SXUNUSED(nArg);` |
|    ! 0 | 2881 | `	SXUNUSED(apArg);` |
|    ! 0 | 2882 | `	SXUNUSED(pCtx);` |
|    ! 0 | 2883 | `	return PH7_OK;` |
|    ! 0 | 2884 | `}` |
|      - | 2885 | `/*` |
|      - | 2886 | ` * Declare the six. Called from PH7_VmInstallReflection() where chunk 5 used to` |
|      - | 2887 | ` * be compiled, so Reflector (chunk 1) already exists.` |
|      - | 2888 | ` */` |
|   5740 | 2889 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionSmall(ph7_vm *pVm)` |
|      5 | 2890 | `{` |
|      - | 2891 | `	static const PH7_NativePropDef aGenProp[] = {` |
|      - | 2892 | `		{ RG_GEN, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 2893 | `	};` |
|      - | 2894 | `	static const PH7_NativeMethodDef aGenMethod[] = {` |
|      - | 2895 | `		{ "__construct",           PH7_MOD_PUBLIC, "Generator $generator", "",` |
|      - | 2896 | `		  vm_builtin_ReflectionGenerator_construct },` |
|      - | 2897 | `		{ "getFunction",           PH7_MOD_PUBLIC, "", "ReflectionFunctionAbstract",` |
|      - | 2898 | `		  vm_builtin_ReflectionGenerator_getFunction },` |
|      - | 2899 | `		{ "getThis",               PH7_MOD_PUBLIC, "", "?object", vm_builtin_ReflectionGenerator_getThis },` |
|      - | 2900 | `		{ "getExecutingGenerator", PH7_MOD_PUBLIC, "", "Generator",` |
|      - | 2901 | `		  vm_builtin_ReflectionGenerator_getExecuting },` |
|      - | 2902 | `		{ "isClosed",              PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionGenerator_isClosed },` |
|      - | 2903 | `		{ "getExecutingLine",      PH7_MOD_PUBLIC, "", "int", vm_builtin_ReflectionGenerator_execLine },` |
|      - | 2904 | `		{ "getExecutingFile",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionGenerator_execFile },` |
|      - | 2905 | `		{ "getTrace",              PH7_MOD_PUBLIC, "int $options = 1", "array",` |
|      - | 2906 | `		  vm_builtin_ReflectionGenerator_trace },` |
|      - | 2907 | `	};` |
|      - | 2908 | `	static const PH7_NativePropDef aFiberProp[] = {` |
|      - | 2909 | `		{ RF_FIBER, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 2910 | `	};` |
|      - | 2911 | `	static const PH7_NativeMethodDef aFiberMethod[] = {` |
|      - | 2912 | `		{ "__construct",      PH7_MOD_PUBLIC, "Fiber $fiber", "", vm_builtin_ReflectionFiber_construct },` |
|      - | 2913 | `		{ "getFiber",         PH7_MOD_PUBLIC, "", "Fiber",    vm_builtin_ReflectionFiber_getFiber },` |
|      - | 2914 | `		{ "getCallable",      PH7_MOD_PUBLIC, "", "callable", vm_builtin_ReflectionFiber_getCallable },` |
|      - | 2915 | `		{ "getExecutingLine", PH7_MOD_PUBLIC, "", "?int",     vm_builtin_ReflectionFiber_execLine },` |
|      - | 2916 | `		{ "getExecutingFile", PH7_MOD_PUBLIC, "", "?string",  vm_builtin_ReflectionFiber_execFile },` |
|      - | 2917 | `		{ "getTrace",         PH7_MOD_PUBLIC, "int $options = 1", "array", vm_builtin_ReflectionFiber_trace },` |
|      - | 2918 | `	};` |
|      - | 2919 | `	static const PH7_NativePropDef aNameProp[] = {` |
|      - | 2920 | `		{ "name", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 2921 | `	};` |
|      - | 2922 | `	static const PH7_NativeMethodDef aConstMethod[] = {` |
|      - | 2923 | `		{ "__construct",       PH7_MOD_PUBLIC, "string $name", "",` |
|      - | 2924 | `		  vm_builtin_ReflectionConstant_construct },` |
|      - | 2925 | `		{ "getName",           PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_getName },` |
|      - | 2926 | `		{ "getNamespaceName",  PH7_MOD_PUBLIC, "", "string",` |
|      - | 2927 | `		  vm_builtin_ReflectionConstant_getNamespaceName },` |
|      - | 2928 | `		{ "getShortName",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_getShortName },` |
|      - | 2929 | `		{ "getValue",          PH7_MOD_PUBLIC, "", "mixed",  vm_builtin_ReflectionConstant_getValue },` |
|      - | 2930 | `		{ "isDeprecated",      PH7_MOD_PUBLIC, "", "bool",   vm_builtin_ReflectionConstant_isDeprecated },` |
|      - | 2931 | `		{ "getFileName",       PH7_MOD_PUBLIC, "", "string\|false",` |
|      - | 2932 | `		  vm_builtin_ReflectionConstant_getFileName },` |
|      - | 2933 | `		{ "getExtension",      PH7_MOD_PUBLIC, "", "?ReflectionExtension",` |
|      - | 2934 | `		  vm_builtin_ReflectionConstant_getExtension },` |
|      - | 2935 | `		{ "getExtensionName",  PH7_MOD_PUBLIC, "", "string\|false",` |
|      - | 2936 | `		  vm_builtin_ReflectionConstant_getExtensionName },` |
|      - | 2937 | `		{ "getAttributes",     PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - | 2938 | `		  vm_builtin_ReflectionConstant_getAttributes },` |
|      - | 2939 | `		{ "__toString",        PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_toString },` |
|      - | 2940 | `	};` |
|      - | 2941 | `	static const PH7_NativeMethodDef aExtMethod[] = {` |
|      - | 2942 | `		{ "__construct",     PH7_MOD_PUBLIC, "string $name", "", vm_builtin_ReflectionExtension_construct },` |
|      - | 2943 | `		{ "getName",         PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionExtension_getName },` |
|      - | 2944 | `		{ "getVersion",      PH7_MOD_PUBLIC, "", "@?string", vm_builtin_ReflectionExtension_getVersion },` |
|      - | 2945 | `		{ "getFunctions",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|      - | 2946 | `		{ "getConstants",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|      - | 2947 | `		{ "getINIEntries",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|      - | 2948 | `		{ "getClasses",      PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|      - | 2949 | `		{ "getClassNames",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|      - | 2950 | `		{ "getDependencies", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|      - | 2951 | `		{ "info",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_ReflectionExtension_info },` |
|      - | 2952 | `		{ "isPersistent",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionExtension_isPersistent },` |
|      - | 2953 | `		{ "isTemporary",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionExtension_isTemporary },` |
|      - | 2954 | `		{ "__toString",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionExtension_toString },` |
|      - | 2955 | `	};` |
|      - | 2956 | `	static const PH7_NativeMethodDef aZendMethod[] = {` |
|      - | 2957 | `		{ "__construct", PH7_MOD_PUBLIC, "string $name", "",` |
|      - | 2958 | `		  vm_builtin_ReflectionZendExtension_construct },` |
|      - | 2959 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionExtension_getName },` |
|      - | 2960 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionZendExtension_toString },` |
|      - | 2961 | `	};` |
|      - | 2962 | `	static const PH7_NativePropDef aRefProp[] = {` |
|      - | 2963 | `		{ RR_ID, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - | 2964 | `	};` |
|      - | 2965 | `	static const PH7_NativeMethodDef aRefMethod[] = {` |
|      - | 2966 | ``		/* php declares the constructor PRIVATE, so `new ReflectionReference` is`` |
|      - | 2967 | `		 * the engine's own "Call to private ... from global scope" Error rather` |
|      - | 2968 | `		 * than a throw the prelude had to write by hand. */` |
|      - | 2969 | `		{ "__construct",      PH7_MOD_PRIVATE, "", "", vm_builtin_ReflectionReference_construct },` |
|      - | 2970 | `		{ "fromArrayElement", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array, string\|int $key",` |
|      - | 2971 | `		  "?ReflectionReference", vm_builtin_ReflectionReference_fromArrayElement },` |
|      - | 2972 | `		{ "getId",            PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionReference_getId },` |
|      - | 2973 | `	};` |
|      - | 2974 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 2975 | `		{ "ReflectionGenerator", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 2976 | `		  aGenMethod, SX_ARRAYSIZE(aGenMethod), 0, 0, aGenProp, SX_ARRAYSIZE(aGenProp), 0, 0, 0 },` |
|      - | 2977 | `		{ "ReflectionFiber", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 2978 | `		  aFiberMethod, SX_ARRAYSIZE(aFiberMethod), 0, 0, aFiberProp, SX_ARRAYSIZE(aFiberProp), 0, 0, 0 },` |
|      - | 2979 | `		{ "ReflectionConstant", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 2980 | `		  aConstMethod, SX_ARRAYSIZE(aConstMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|      - | 2981 | `		{ "ReflectionExtension", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 2982 | `		  aExtMethod, SX_ARRAYSIZE(aExtMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|      - | 2983 | `		{ "ReflectionZendExtension", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 2984 | `		  aZendMethod, SX_ARRAYSIZE(aZendMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|      - | 2985 | `		{ "ReflectionReference", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 2986 | `		  aRefMethod, SX_ARRAYSIZE(aRefMethod), 0, 0, aRefProp, SX_ARRAYSIZE(aRefProp), 0, 0, 0 },` |
|      - | 2987 | `	};` |
|   5745 | 2988 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 | 2989 | `}` |
|      - | 2990 | `/*` |
|      - | 2991 | ` * ---------------------------------------------------------------------------` |
|      - | 2992 | ` * Reflector, Reflection, ReflectionException, ReflectionClass, ReflectionObject.` |
|      - | 2993 | ` *` |
|      - | 2994 | ` * Chunk 1 — the core of the whole API. Its ~350 lines of PHP funnelled every` |
|      - | 2995 | ` * accessor through __phl_rcinfo(), a memoized descriptor ARRAY built by a C` |
|      - | 2996 | ` * thunk: sixty methods that each rebuilt or re-read a marshalled copy of state` |
|      - | 2997 | ` * the engine was already holding. A native method reads ph7_class directly, so` |
|      - | 2998 | ` * the descriptor is gone from this path entirely and the memo it needed with it.` |
|      - | 2999 | ` *` |
|      - | 3000 | ` * What stays behind is the prelude that has not moved yet, and these classes` |
|      - | 3001 | ` * still reach it by name where php's own object graph does: getMethod() builds` |
|      - | 3002 | ` * a ReflectionMethod, getProperty() a ReflectionProperty, getAttributes() calls` |
|      - | 3003 | ` * __reflect_build_attrs, __toString() calls __reflect_export_class. Those become` |
|      - | 3004 | ` * direct C the moment chunks 2, 3, 7 and 9 land.` |
|      - | 3005 | ` * ---------------------------------------------------------------------------` |
|      - | 3006 | ` */` |
|      - | 3007 | `#define RC_OBJ "__obj"` |
|      - | 3008 |  |
|      - | 3009 | ``/* The class a ReflectionClass reflects: its `name` slot, resolved. The`` |
|      - | 3010 | ` * constructor already stored the canonical name, so no autoload can be needed` |
|      - | 3011 | ` * here. */` |
|   1116 | 3012 | `static ph7_class * ReflectClassOf(ph7_context *pCtx)` |
|      5 | 3013 | `{` |
|   1121 | 3014 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 3015 | `	const char *zName;` |
|      - | 3016 | `	int nName;` |
|   1121 | 3017 | `	if( pThis == 0 ){` |
|    ! 0 | 3018 | `		return 0;` |
|      - | 3019 | `	}` |
|   1121 | 3020 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|   1121 | 3021 | `	if( nName < 1 ){` |
|    ! 0 | 3022 | `		return 0;` |
|      - | 3023 | `	}` |
|      - | 3024 | `	/* PH7_NativeAttrStr borrows bytes that are NOT NUL-terminated: the length` |
|      - | 3025 | `	 * has to travel with them. */` |
|   1121 | 3026 | `	return PH7_VmExtractClass(pCtx->pVm, zName, (sxu32)nName, FALSE, 0);` |
|    563 | 3027 | `}` |
|      - | 3028 | `/* The instance a ReflectionObject was built over, or NULL for a plain` |
|      - | 3029 | ` * ReflectionClass — what makes DYNAMIC properties visible. */` |
|    196 | 3030 | `static ph7_class_instance * ReflectClassObj(ph7_context *pCtx)` |
|      4 | 3031 | `{` |
|    200 | 3032 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    200 | 3033 | `	return pThis ? PH7_NativeAttrObj(pThis, RC_OBJ) : 0;` |
|      4 | 3034 | `}` |
|      - | 3035 | `/* $this->name as bytes. */` |
|    632 | 3036 | `static void ReflectClassName(ph7_context *pCtx, const char **pzOut, int *pnOut)` |
|      5 | 3037 | `{` |
|    637 | 3038 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    637 | 3039 | `	*pzOut = "";` |
|    637 | 3040 | `	*pnOut = 0;` |
|    637 | 3041 | `	if( pThis ){` |
|    637 | 3042 | `		PH7_NativeAttrStr(pThis, "name", pzOut, pnOut);` |
|    316 | 3043 | `	}` |
|    637 | 3044 | `}` |
|      - | 3045 | `/* Answer the truth of one of the reflected class's iFlags bits. */` |
|    130 | 3046 | `static int ReflectClassFlag(ph7_context *pCtx, sxi32 iFlag)` |
|      3 | 3047 | `{` |
|    133 | 3048 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|    133 | 3049 | `	return pClass != 0 && (pClass->iFlags & iFlag) != 0;` |
|      3 | 3050 | `}` |
|      - | 3051 | `#define REFLECT_CLASS_FLAG(NAME,FLAG,NEGATE) \` |
|      - | 3052 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 3053 | `	{ \` |
|      - | 3054 | `		SXUNUSED(nArg); \` |
|      - | 3055 | `		SXUNUSED(apArg); \` |
|      - | 3056 | `		ph7_result_bool(pCtx, NEGATE ? !ReflectClassFlag(pCtx,FLAG) : ReflectClassFlag(pCtx,FLAG)); \` |
|      - | 3057 | `		return PH7_OK; \` |
|      - | 3058 | `	}` |
|     25 | 3059 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isInternal,    PH7_CLASS_INTERNAL,  0)` |
|      5 | 3060 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isUserDefined, PH7_CLASS_INTERNAL,  1)` |
|      9 | 3061 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isInterface,   PH7_CLASS_INTERFACE, 0)` |
|      5 | 3062 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isTrait,       PH7_CLASS_TRAIT,     0)` |
|     13 | 3063 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isAbstract,    PH7_CLASS_ABSTRACT,  0)` |
|     63 | 3064 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isFinal,       PH7_CLASS_FINAL,     0)` |
|      3 | 3065 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isReadOnly,    PH7_CLASS_READONLY,  0)` |
|     13 | 3066 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isEnum,        PH7_CLASS_ENUM,      0)` |
|      - | 3067 |  |
|      - | 3068 | ``/* Build a ReflectionClass over pTarget with its `name` already filled in: the`` |
|      - | 3069 | ` * constructor would only re-resolve a class this code is holding. */` |
|    316 | 3070 | `static int ReflectResultClassOf(ph7_context *pCtx, ph7_class *pTarget)` |
|      5 | 3071 | `{` |
|    321 | 3072 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 3073 | `	ph7_class *pRC;` |
|      - | 3074 | `	ph7_class_instance *pObj;` |
|    321 | 3075 | `	if( pTarget == 0 ){` |
|    ! 0 | 3076 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3077 | `		return PH7_OK;` |
|      - | 3078 | `	}` |
|    321 | 3079 | `	pRC = PH7_VmExtractClass(pVm, "ReflectionClass", sizeof("ReflectionClass")-1, FALSE, 0);` |
|    321 | 3080 | `	if( pRC == 0 \|\| (pObj = PH7_NewClassInstance(pVm, pRC)) == 0 ){` |
|    ! 0 | 3081 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3082 | `		return PH7_OK;` |
|      - | 3083 | `	}` |
|    479 | 3084 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(&pTarget->sName),` |
|    316 | 3085 | `		(int)SyStringLength(&pTarget->sName));` |
|    321 | 3086 | `	PH7_NativeResultObject(pCtx, pObj);` |
|    321 | 3087 | `	return PH7_OK;` |
|    163 | 3088 | `}` |
|      - | 3089 | ``/* The class an argument declared `ReflectionClass\|string` denotes: a reflector's`` |
|      - | 3090 | `` * `name` slot, or the string itself. NULL when it names nothing (after`` |
|      - | 3091 | ` * autoload). */` |
|     22 | 3092 | `static ph7_class * ReflectClassArg(ph7_context *pCtx, ph7_value *pArg)` |
|      1 | 3093 | `{` |
|     23 | 3094 | `	ph7_vm *pVm = pCtx->pVm;` |
|     23 | 3095 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|    ! 0 | 3096 | `		ph7_class_instance *pObj = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 | 3097 | `		ph7_class *pRC = PH7_VmExtractClass(pVm, "ReflectionClass", sizeof("ReflectionClass")-1, FALSE, 0);` |
|    ! 0 | 3098 | `		if( pRC && PH7_VmInstanceOf(pObj->pClass, pRC) ){` |
|      - | 3099 | `			const char *zName;` |
|      - | 3100 | `			int nName;` |
|    ! 0 | 3101 | `			PH7_NativeAttrStr(pObj, "name", &zName, &nName);` |
|    ! 0 | 3102 | `			return nName > 0 ? PH7_VmExtractClass(pVm, zName, (sxu32)nName, FALSE, 0) : 0;` |
|      - | 3103 | `		}` |
|    ! 0 | 3104 | `	}` |
|     23 | 3105 | `	return ReflectResolveClass(pVm, pArg);` |
|     12 | 3106 | `}` |
|      - | 3107 | `/* ---- constructors ---- */` |
|      - | 3108 | `/* ReflectionClass::__construct(object\|string $objectOrClass) */` |
|    658 | 3109 | `static int vm_builtin_ReflectionClass_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 | 3110 | `{` |
|    663 | 3111 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 3112 | `	ph7_class *pClass;` |
|    663 | 3113 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 3114 | `		return PH7_OK;` |
|      - | 3115 | `	}` |
|    663 | 3116 | `	pClass = ReflectResolveClass(pCtx->pVm, apArg[0]);` |
|    663 | 3117 | `	if( pClass == 0 ){` |
|      - | 3118 | `		/* php reports the NAME it was handed, after the declared object\|string` |
|      - | 3119 | ``		 * has coerced a scalar — `new ReflectionClass(1.5)` says Class "1.5". */`` |
|      - | 3120 | `		const char *zName;` |
|      - | 3121 | `		int nName;` |
|     12 | 3122 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|     17 | 3123 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      5 | 3124 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      - | 3125 | `	}` |
|    977 | 3126 | `	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", SyStringData(&pClass->sName),` |
|    648 | 3127 | `		(int)SyStringLength(&pClass->sName));` |
|    653 | 3128 | `	return PH7_OK;` |
|    334 | 3129 | `}` |
|      - | 3130 | `/* ReflectionObject::__construct(object $object) — the same, plus the receiver` |
|      - | 3131 | ` * whose DYNAMIC properties the inherited accessors then report. */` |
|     20 | 3132 | `static int vm_builtin_ReflectionObject_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3133 | `{` |
|     21 | 3134 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 3135 | `	sxi32 rc;` |
|     21 | 3136 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 3137 | `		return PH7_OK;` |
|      - | 3138 | `	}` |
|     21 | 3139 | `	rc = vm_builtin_ReflectionClass_construct(pCtx, nArg, apArg);` |
|     21 | 3140 | `	if( rc != PH7_OK ){` |
|    ! 0 | 3141 | `		return rc;` |
|      - | 3142 | `	}` |
|     21 | 3143 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RC_OBJ, (ph7_class_instance *)apArg[0]->x.pOther);` |
|     21 | 3144 | `	return PH7_OK;` |
|     11 | 3145 | `}` |
|      - | 3146 | `/* ReflectionClass::__clone(): void — php declares it private, and the class is` |
|      - | 3147 | ` * uncloneable besides (PH7_CLASS_NOCLONE answers before any body runs). */` |
|    ! 0 | 3148 | `static int vm_builtin_ReflectionClass_clone(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 3149 | `{` |
|    ! 0 | 3150 | `	SXUNUSED(pCtx);` |
|    ! 0 | 3151 | `	SXUNUSED(nArg);` |
|    ! 0 | 3152 | `	SXUNUSED(apArg);` |
|    ! 0 | 3153 | `	return PH7_OK;` |
|    ! 0 | 3154 | `}` |
|      - | 3155 | `/* ---- name ---- */` |
|    328 | 3156 | `static int vm_builtin_ReflectionClass_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 | 3157 | `{` |
|      - | 3158 | `	const char *zName;` |
|      - | 3159 | `	int nName;` |
|    164 | 3160 | `	SXUNUSED(nArg);` |
|    164 | 3161 | `	SXUNUSED(apArg);` |
|    333 | 3162 | `	ReflectClassName(pCtx, &zName, &nName);` |
|    333 | 3163 | `	ph7_result_string(pCtx, zName, nName);` |
|    333 | 3164 | `	return PH7_OK;` |
|      5 | 3165 | `}` |
|      - | 3166 | `/* Offset just past the last namespace separator, or -1 when there is none. */` |
|      6 | 3167 | `static int ReflectNsCut(const char *zName, int nName)` |
|      1 | 3168 | `{` |
|      - | 3169 | `	int i;` |
|     91 | 3170 | `	for( i = nName - 1 ; i >= 0 ; i-- ){` |
|     85 | 3171 | `		if( zName[i] == '\\' ){` |
|    ! 0 | 3172 | `			return i;` |
|      - | 3173 | `		}` |
|     43 | 3174 | `	}` |
|      7 | 3175 | `	return -1;` |
|      4 | 3176 | `}` |
|      2 | 3177 | `static int vm_builtin_ReflectionClass_getShortName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3178 | `{` |
|      - | 3179 | `	const char *zName;` |
|      - | 3180 | `	int nName, iCut;` |
|      1 | 3181 | `	SXUNUSED(nArg);` |
|      1 | 3182 | `	SXUNUSED(apArg);` |
|      3 | 3183 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 | 3184 | `	iCut = ReflectNsCut(zName, nName);` |
|      3 | 3185 | `	if( iCut < 0 ){` |
|      3 | 3186 | `		ph7_result_string(pCtx, zName, nName);` |
|      2 | 3187 | `	}else{` |
|    ! 0 | 3188 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|      - | 3189 | `	}` |
|      3 | 3190 | `	return PH7_OK;` |
|      1 | 3191 | `}` |
|      2 | 3192 | `static int vm_builtin_ReflectionClass_getNamespaceName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3193 | `{` |
|      - | 3194 | `	const char *zName;` |
|      - | 3195 | `	int nName, iCut;` |
|      1 | 3196 | `	SXUNUSED(nArg);` |
|      1 | 3197 | `	SXUNUSED(apArg);` |
|      3 | 3198 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 | 3199 | `	iCut = ReflectNsCut(zName, nName);` |
|      3 | 3200 | `	ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|      3 | 3201 | `	return PH7_OK;` |
|      1 | 3202 | `}` |
|      2 | 3203 | `static int vm_builtin_ReflectionClass_inNamespace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3204 | `{` |
|      - | 3205 | `	const char *zName;` |
|      - | 3206 | `	int nName;` |
|      1 | 3207 | `	SXUNUSED(nArg);` |
|      1 | 3208 | `	SXUNUSED(apArg);` |
|      3 | 3209 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 | 3210 | `	ph7_result_bool(pCtx, ReflectNsCut(zName, nName) >= 0);` |
|      3 | 3211 | `	return PH7_OK;` |
|      1 | 3212 | `}` |
|      2 | 3213 | `static int vm_builtin_ReflectionClass_isAnonymous(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3214 | `{` |
|      - | 3215 | `	const char *zName;` |
|      - | 3216 | `	int nName;` |
|      - | 3217 | `	static const char zAnon[] = "class@anonymous";` |
|      1 | 3218 | `	SXUNUSED(nArg);` |
|      1 | 3219 | `	SXUNUSED(apArg);` |
|      3 | 3220 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 | 3221 | `	ph7_result_bool(pCtx, nName >= (int)sizeof(zAnon)-1` |
|      1 | 3222 | `		&& SyMemcmp(zName, zAnon, sizeof(zAnon)-1) == 0);` |
|      3 | 3223 | `	return PH7_OK;` |
|      1 | 3224 | `}` |
|      - | 3225 | `/* ---- shape ---- */` |
|     14 | 3226 | `static int vm_builtin_ReflectionClass_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3227 | `{` |
|     15 | 3228 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     15 | 3229 | `	sxi64 iMods = 0;` |
|      7 | 3230 | `	SXUNUSED(nArg);` |
|      7 | 3231 | `	SXUNUSED(apArg);` |
|     15 | 3232 | `	if( pClass ){` |
|     15 | 3233 | `		if( pClass->iFlags & PH7_CLASS_ABSTRACT ){ iMods \|= 64; }` |
|     15 | 3234 | `		if( pClass->iFlags & PH7_CLASS_FINAL ){ iMods \|= 32; }` |
|     15 | 3235 | `		if( pClass->iFlags & PH7_CLASS_READONLY ){ iMods \|= 65536; }` |
|      7 | 3236 | `	}` |
|     15 | 3237 | `	ph7_result_int64(pCtx, iMods);` |
|     15 | 3238 | `	return PH7_OK;` |
|      1 | 3239 | `}` |
|     32 | 3240 | `static int vm_builtin_ReflectionClass_getParentClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 3241 | `{` |
|     35 | 3242 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     16 | 3243 | `	SXUNUSED(nArg);` |
|     16 | 3244 | `	SXUNUSED(apArg);` |
|     35 | 3245 | `	if( pClass == 0 \|\| pClass->pBase == 0 ){` |
|     13 | 3246 | `		ph7_result_bool(pCtx, 0);` |
|     13 | 3247 | `		return PH7_OK;` |
|      - | 3248 | `	}` |
|     25 | 3249 | `	return ReflectResultClassOf(pCtx, pClass->pBase);` |
|     19 | 3250 | `}` |
|      - | 3251 | `/* getInterfaceNames()/getInterfaces()/getTraitNames()/getTraits() — one walk,` |
|      - | 3252 | ` * four shapes. bReflector picks name-list vs {name: ReflectionClass}. */` |
|     70 | 3253 | `static int ReflectClassNameList(ph7_context *pCtx, int bTraits, int bReflector)` |
|      2 | 3254 | `{` |
|     72 | 3255 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     72 | 3256 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|      - | 3257 | `	SySet aSet;` |
|      - | 3258 | `	ph7_class **apOut;` |
|      - | 3259 | `	sxu32 n, nOut;` |
|     72 | 3260 | `	if( pList == 0 ){` |
|    ! 0 | 3261 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3262 | `	}` |
|     72 | 3263 | `	if( pClass == 0 ){` |
|    ! 0 | 3264 | `		ph7_result_value(pCtx, pList);` |
|    ! 0 | 3265 | `		return PH7_OK;` |
|      - | 3266 | `	}` |
|     72 | 3267 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|     72 | 3268 | `	if( bTraits ){` |
|      3 | 3269 | `		apOut = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      3 | 3270 | `		nOut = SySetUsed(&pClass->aTrait);` |
|      2 | 3271 | `	}else{` |
|     70 | 3272 | `		ReflectInterfacesOf(pClass, &aSet);` |
|     70 | 3273 | `		apOut = (ph7_class **)SySetBasePtr(&aSet);` |
|     70 | 3274 | `		nOut = SySetUsed(&aSet);` |
|      - | 3275 | `	}` |
|    180 | 3276 | `	for( n = 0 ; n < nOut ; n++ ){` |
|    110 | 3277 | `		SyString *pName = &apOut[n]->sName;` |
|    110 | 3278 | `		if( bReflector ){` |
|      - | 3279 | `			/* {name: ReflectionClass} — the reflector is built here rather than` |
|      - | 3280 | `			 * through ReflectResultClassOf, which writes the RESULT slot. */` |
|      5 | 3281 | `			ph7_class *pRC = PH7_VmExtractClass(pCtx->pVm, "ReflectionClass",` |
|      - | 3282 | `				sizeof("ReflectionClass")-1, FALSE, 0);` |
|      5 | 3283 | `			ph7_class_instance *pObj = pRC ? PH7_NewClassInstance(pCtx->pVm, pRC) : 0;` |
|      5 | 3284 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|      - | 3285 | `			ph7_value sVal;` |
|      5 | 3286 | `			if( pObj == 0 \|\| pKey == 0 ){ break; }` |
|      7 | 3287 | `			PH7_NativeSetAttrStr(pCtx->pVm, pObj, "name", SyStringData(pName),` |
|      4 | 3288 | `				(int)SyStringLength(pName));` |
|      - | 3289 | `			/* A STACK carrier, never a context scalar: releasing a MEMOBJ_OBJ` |
|      - | 3290 | `			 * context value would unref the instance a second time. */` |
|      5 | 3291 | `			PH7_MemObjInit(pCtx->pVm, &sVal);` |
|      5 | 3292 | `			sVal.x.pOther = pObj;` |
|      5 | 3293 | `			sVal.iFlags = MEMOBJ_OBJ;` |
|      5 | 3294 | `			ph7_value_string(pKey, SyStringData(pName), (int)SyStringLength(pName));` |
|      5 | 3295 | `			ph7_array_add_elem(pList, pKey, &sVal);   /* takes its own reference */` |
|      5 | 3296 | `			PH7_ClassInstanceUnref(pObj);` |
|      3 | 3297 | `		}else{` |
|    106 | 3298 | `			ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    106 | 3299 | `			if( pVal == 0 ){ break; }` |
|    106 | 3300 | `			ph7_value_string(pVal, SyStringData(pName), (int)SyStringLength(pName));` |
|    106 | 3301 | `			ph7_array_add_elem(pList, 0, pVal);` |
|      - | 3302 | `		}` |
|     56 | 3303 | `	}` |
|     72 | 3304 | `	SySetRelease(&aSet);` |
|     72 | 3305 | `	ph7_result_value(pCtx, pList);` |
|     72 | 3306 | `	return PH7_OK;` |
|     37 | 3307 | `}` |
|      - | 3308 | `#define REFLECT_CLASS_LIST(NAME,TRAITS,REFLECTOR) \` |
|      - | 3309 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 3310 | `	{ \` |
|      - | 3311 | `		SXUNUSED(nArg); \` |
|      - | 3312 | `		SXUNUSED(apArg); \` |
|      - | 3313 | `		return ReflectClassNameList(pCtx,TRAITS,REFLECTOR); \` |
|      - | 3314 | `	}` |
|     68 | 3315 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getInterfaceNames, 0, 0)` |
|      3 | 3316 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getInterfaces,     0, 1)` |
|      3 | 3317 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getTraitNames,     1, 0)` |
|    ! 0 | 3318 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getTraits,         1, 1)` |
|      - | 3319 |  |
|      - | 3320 | ``/* getTraitAliases(): php reports `use T { m as n; }` renames; PHL's compiler`` |
|      - | 3321 | ` * installs the alias as a method and keeps no rename record, so the map is` |
|      - | 3322 | ` * empty (§7.4). */` |
|    ! 0 | 3323 | `static int vm_builtin_ReflectionClass_getTraitAliases(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 3324 | `{` |
|    ! 0 | 3325 | `	SXUNUSED(nArg);` |
|    ! 0 | 3326 | `	SXUNUSED(apArg);` |
|    ! 0 | 3327 | `	ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 | 3328 | `	return PH7_OK;` |
|    ! 0 | 3329 | `}` |
|      4 | 3330 | `static int vm_builtin_ReflectionClass_isIterable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3331 | `{` |
|      5 | 3332 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3333 | `	SySet aSet;` |
|      - | 3334 | `	ph7_class **apIface;` |
|      - | 3335 | `	sxu32 n;` |
|      5 | 3336 | `	int bIterable = 0;` |
|      2 | 3337 | `	SXUNUSED(nArg);` |
|      2 | 3338 | `	SXUNUSED(apArg);` |
|      4 | 3339 | `	if( pClass == 0` |
|      5 | 3340 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT)) ){` |
|    ! 0 | 3341 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3342 | `		return PH7_OK;` |
|      - | 3343 | `	}` |
|      5 | 3344 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|      5 | 3345 | `	ReflectInterfacesOf(pClass, &aSet);` |
|      5 | 3346 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|      5 | 3347 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|      5 | 3348 | `		if( pCtx->pVm->pTraversableClass && apIface[n] == pCtx->pVm->pTraversableClass ){` |
|      5 | 3349 | `			bIterable = 1;` |
|      5 | 3350 | `			break;` |
|      - | 3351 | `		}` |
|    ! 0 | 3352 | `	}` |
|      5 | 3353 | `	SySetRelease(&aSet);` |
|      5 | 3354 | `	ph7_result_bool(pCtx, bIterable);` |
|      5 | 3355 | `	return PH7_OK;` |
|      3 | 3356 | `}` |
|     16 | 3357 | `static int vm_builtin_ReflectionClass_implementsInterface(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3358 | `{` |
|     17 | 3359 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3360 | `	ph7_class *pTarget;` |
|      - | 3361 | `	SySet aSet;` |
|      - | 3362 | `	ph7_class **apIface;` |
|      - | 3363 | `	sxu32 n;` |
|     17 | 3364 | `	int bYes = 0;` |
|     17 | 3365 | `	if( nArg < 1 ){` |
|    ! 0 | 3366 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3367 | `		return PH7_OK;` |
|      - | 3368 | `	}` |
|     17 | 3369 | `	pTarget = ReflectClassArg(pCtx, apArg[0]);` |
|     17 | 3370 | `	if( pTarget == 0 ){` |
|      - | 3371 | `		const char *zName;` |
|      - | 3372 | `		int nName;` |
|      3 | 3373 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|      4 | 3374 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 3375 | `			"Interface \"%.*s\" does not exist", nName, zName);` |
|      - | 3376 | `	}` |
|     15 | 3377 | `	if( (pTarget->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      4 | 3378 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 3379 | `			"%z is not an interface", &pTarget->sName);` |
|      - | 3380 | `	}` |
|     13 | 3381 | `	if( pClass == pTarget ){` |
|      3 | 3382 | `		ph7_result_bool(pCtx, 1);` |
|      3 | 3383 | `		return PH7_OK;` |
|      - | 3384 | `	}` |
|     11 | 3385 | `	if( pClass == 0 ){` |
|    ! 0 | 3386 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3387 | `		return PH7_OK;` |
|      - | 3388 | `	}` |
|     11 | 3389 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|     11 | 3390 | `	ReflectInterfacesOf(pClass, &aSet);` |
|     11 | 3391 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|     13 | 3392 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|     13 | 3393 | `		if( apIface[n] == pTarget ){` |
|     11 | 3394 | `			bYes = 1;` |
|     11 | 3395 | `			break;` |
|      - | 3396 | `		}` |
|      2 | 3397 | `	}` |
|     11 | 3398 | `	SySetRelease(&aSet);` |
|     11 | 3399 | `	ph7_result_bool(pCtx, bYes);` |
|     11 | 3400 | `	return PH7_OK;` |
|      9 | 3401 | `}` |
|      6 | 3402 | `static int vm_builtin_ReflectionClass_isSubclassOf(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3403 | `{` |
|      7 | 3404 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3405 | `	ph7_class *pTarget, *pWalk;` |
|      - | 3406 | `	SySet aSet;` |
|      - | 3407 | `	ph7_class **apIface;` |
|      - | 3408 | `	sxu32 n;` |
|      7 | 3409 | `	int iDepth = 0, bYes = 0;` |
|      7 | 3410 | `	if( nArg < 1 ){` |
|    ! 0 | 3411 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3412 | `		return PH7_OK;` |
|      - | 3413 | `	}` |
|      7 | 3414 | `	pTarget = ReflectClassArg(pCtx, apArg[0]);` |
|      7 | 3415 | `	if( pTarget == 0 ){` |
|      - | 3416 | `		const char *zName;` |
|      - | 3417 | `		int nName;` |
|    ! 0 | 3418 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|    ! 0 | 3419 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 | 3420 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      - | 3421 | `	}` |
|      - | 3422 | `	/* php: a class is never a subclass of ITSELF */` |
|      7 | 3423 | `	if( pClass == 0 \|\| pClass == pTarget ){` |
|      3 | 3424 | `		ph7_result_bool(pCtx, 0);` |
|      3 | 3425 | `		return PH7_OK;` |
|      - | 3426 | `	}` |
|      7 | 3427 | `	for( pWalk = pClass->pBase ; pWalk && iDepth <= REFLECT_WALK_MAX_DEPTH ; pWalk = pWalk->pBase ){` |
|      5 | 3428 | `		if( pWalk == pTarget ){` |
|      3 | 3429 | `			ph7_result_bool(pCtx, 1);` |
|      3 | 3430 | `			return PH7_OK;` |
|      - | 3431 | `		}` |
|      3 | 3432 | `		iDepth++;` |
|      2 | 3433 | `	}` |
|      3 | 3434 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|      3 | 3435 | `	ReflectInterfacesOf(pClass, &aSet);` |
|      3 | 3436 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|      3 | 3437 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|      3 | 3438 | `		if( apIface[n] == pTarget ){` |
|      3 | 3439 | `			bYes = 1;` |
|      3 | 3440 | `			break;` |
|      - | 3441 | `		}` |
|    ! 0 | 3442 | `	}` |
|      3 | 3443 | `	SySetRelease(&aSet);` |
|      3 | 3444 | `	ph7_result_bool(pCtx, bYes);` |
|      3 | 3445 | `	return PH7_OK;` |
|      4 | 3446 | `}` |
|      8 | 3447 | `static int vm_builtin_ReflectionClass_isInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3448 | `{` |
|      9 | 3449 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3450 | `	ph7_class_instance *pObj;` |
|      9 | 3451 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 \|\| pClass == 0 ){` |
|    ! 0 | 3452 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3453 | `		return PH7_OK;` |
|      - | 3454 | `	}` |
|      9 | 3455 | `	pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      9 | 3456 | `	ph7_result_bool(pCtx, PH7_VmInstanceOf(pObj->pClass, pClass) != 0);` |
|      9 | 3457 | `	return PH7_OK;` |
|      5 | 3458 | `}` |
|      - | 3459 | `/* ---- source position ---- */` |
|      8 | 3460 | `static int vm_builtin_ReflectionClass_getStartLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3461 | `{` |
|      9 | 3462 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      4 | 3463 | `	SXUNUSED(nArg);` |
|      4 | 3464 | `	SXUNUSED(apArg);` |
|      9 | 3465 | `	if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_INTERNAL) ){` |
|      3 | 3466 | `		ph7_result_bool(pCtx, 0);` |
|      2 | 3467 | `	}else{` |
|      7 | 3468 | `		ph7_result_int64(pCtx, (sxi64)pClass->nLine);` |
|      - | 3469 | `	}` |
|      9 | 3470 | `	return PH7_OK;` |
|      1 | 3471 | `}` |
|      6 | 3472 | `static int vm_builtin_ReflectionClass_getEndLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3473 | `{` |
|      7 | 3474 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      3 | 3475 | `	SXUNUSED(nArg);` |
|      3 | 3476 | `	SXUNUSED(apArg);` |
|      7 | 3477 | `	if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_INTERNAL) ){` |
|      3 | 3478 | `		ph7_result_bool(pCtx, 0);` |
|      2 | 3479 | `	}else{` |
|      5 | 3480 | `		ph7_result_int64(pCtx, (sxi64)pClass->nEndLine);` |
|      - | 3481 | `	}` |
|      7 | 3482 | `	return PH7_OK;` |
|      1 | 3483 | `}` |
|     16 | 3484 | `static int vm_builtin_ReflectionClass_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 3485 | `{` |
|     18 | 3486 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      8 | 3487 | `	SXUNUSED(nArg);` |
|      8 | 3488 | `	SXUNUSED(apArg);` |
|     18 | 3489 | `	if( pClass && SyStringLength(&pClass->sFile) > 0 ){` |
|      8 | 3490 | `		ph7_result_string(pCtx, SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));` |
|      5 | 3491 | `	}else{` |
|     11 | 3492 | `		ph7_result_bool(pCtx, 0);` |
|      - | 3493 | `	}` |
|     18 | 3494 | `	return PH7_OK;` |
|      2 | 3495 | `}` |
|      8 | 3496 | `static int vm_builtin_ReflectionClass_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3497 | `{` |
|      9 | 3498 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      4 | 3499 | `	SXUNUSED(nArg);` |
|      4 | 3500 | `	SXUNUSED(apArg);` |
|      9 | 3501 | `	if( pClass && SyStringLength(&pClass->sDoc) > 0 ){` |
|      3 | 3502 | `		ph7_result_string(pCtx, SyStringData(&pClass->sDoc), (int)SyStringLength(&pClass->sDoc));` |
|      2 | 3503 | `	}else{` |
|      7 | 3504 | `		ph7_result_bool(pCtx, 0);` |
|      - | 3505 | `	}` |
|      9 | 3506 | `	return PH7_OK;` |
|      1 | 3507 | `}` |
|      - | 3508 | `/* ---- instantiation ---- */` |
|     22 | 3509 | `static int vm_builtin_ReflectionClass_isInstantiable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3510 | `{` |
|     23 | 3511 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3512 | `	sxi32 iCtor, iClone;` |
|     11 | 3513 | `	SXUNUSED(nArg);` |
|     11 | 3514 | `	SXUNUSED(apArg);` |
|     22 | 3515 | `	if( pClass == 0` |
|     23 | 3516 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT\|PH7_CLASS_ENUM)) ){` |
|      7 | 3517 | `		ph7_result_bool(pCtx, 0);` |
|      7 | 3518 | `		return PH7_OK;` |
|      - | 3519 | `	}` |
|     17 | 3520 | `	ReflectCtorCloneVis(pCtx->pVm, pClass, &iCtor, &iClone);` |
|     17 | 3521 | `	ph7_result_bool(pCtx, iCtor == 0 \|\| iCtor == PH7_CLASS_PROT_PUBLIC);` |
|     17 | 3522 | `	return PH7_OK;` |
|     12 | 3523 | `}` |
|     32 | 3524 | `static int vm_builtin_ReflectionClass_isCloneable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3525 | `{` |
|     33 | 3526 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3527 | `	sxi32 iCtor, iClone;` |
|     16 | 3528 | `	SXUNUSED(nArg);` |
|     16 | 3529 | `	SXUNUSED(apArg);` |
|     32 | 3530 | `	if( pClass == 0` |
|     33 | 3531 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT)) ){` |
|      3 | 3532 | `		ph7_result_bool(pCtx, 0);` |
|      3 | 3533 | `		return PH7_OK;` |
|      - | 3534 | `	}` |
|     31 | 3535 | `	ReflectCtorCloneVis(pCtx->pVm, pClass, &iCtor, &iClone);` |
|     31 | 3536 | `	if( iClone != 0 ){` |
|      - | 3537 | `		/* php consults a declared __clone FIRST and answers its visibility --` |
|      - | 3538 | ``		 * even when the clone_obj refusal would still answer `clone` itself, so`` |
|      - | 3539 | `		 * a subclass of Exception that declares a public __clone reports TRUE` |
|      - | 3540 | `		 * and refuses anyway. php's own inconsistency, kept. */` |
|      7 | 3541 | `		ph7_result_bool(pCtx, iClone == PH7_CLASS_PROT_PUBLIC);` |
|      7 | 3542 | `		return PH7_OK;` |
|      - | 3543 | `	}` |
|      - | 3544 | `	/* No __clone anywhere: php's answer is whether the clone_obj handler` |
|      - | 3545 | `	 * exists. The refusal flag walks the base chain like the handler it` |
|      - | 3546 | ``	 * models, so `class M extends IteratorIterator {}` reports false too. */`` |
|     25 | 3547 | `	ph7_result_bool(pCtx, !PH7_ClassIsUncloneable(pClass));` |
|     25 | 3548 | `	return PH7_OK;` |
|     17 | 3549 | `}` |
|      - | 3550 | `/*` |
|      - | 3551 | ` * php's own gate, raised before any object exists -- the one every C-side` |
|      - | 3552 | ` * instantiation asks, Reflection's newInstance() and PDO's FETCH_CLASS alike.` |
|      - | 3553 | ` */` |
|    252 | 3554 | `PH7_PRIVATE sxi32 PH7_VmCheckInstantiable(ph7_context *pCtx, ph7_class *pClass)` |
|      4 | 3555 | `{` |
|    256 | 3556 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|      3 | 3557 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate interface %z", &pClass->sName);` |
|      - | 3558 | `	}` |
|    254 | 3559 | `	if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|      3 | 3560 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate trait %z", &pClass->sName);` |
|      - | 3561 | `	}` |
|    252 | 3562 | `	if( pClass->iFlags & PH7_CLASS_ENUM ){` |
|      - | 3563 | `		/* php 8.1 names the enum rather than the FINAL class it also is. */` |
|      3 | 3564 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate enum %z", &pClass->sName);` |
|      - | 3565 | `	}` |
|    250 | 3566 | `	if( pClass->iFlags & PH7_CLASS_ABSTRACT ){` |
|      7 | 3567 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate abstract class %z", &pClass->sName);` |
|      - | 3568 | `	}` |
|    244 | 3569 | `	if( pClass->iFlags & PH7_CLASS_NOINSTANTIATE ){` |
|      - | 3570 | ``		/* The `new` path's create_object refusal, which php raises here too —`` |
|      - | 3571 | `		 * ReflectionClass::newInstance() on a Closure is the same Error, not a` |
|      - | 3572 | `		 * visibility one about its private constructor. */` |
|      8 | 3573 | `		if( pClass->zNewRefusal ){` |
|      6 | 3574 | `			return PH7_VmThrowException(pCtx,` |
|      4 | 3575 | `				pClass->zNewRefusalClass ? pClass->zNewRefusalClass : "Error",` |
|      2 | 3576 | `				"%s", pClass->zNewRefusal);` |
|      - | 3577 | `		}` |
|      4 | 3578 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      1 | 3579 | `			"Instantiation of class %z is not allowed", &pClass->sName);` |
|      - | 3580 | `	}` |
|    237 | 3581 | `	return PH7_OK;` |
|    130 | 3582 | `}` |
|      - | 3583 | `/*` |
|      - | 3584 | ` * newInstance()/newInstanceArgs(): the checks php runs, then the constructor.` |
|      - | 3585 | ` * apCtor/nCtor are already-collected positional arguments; pNames is the` |
|      - | 3586 | ` * name map when the caller handed an array with string keys (php 8.1 accepts` |
|      - | 3587 | ` * those as NAMED constructor arguments).` |
|      - | 3588 | ` */` |
|     24 | 3589 | `static int ReflectNewInstance(ph7_context *pCtx, int nCtor, ph7_value **apCtor,` |
|      - | 3590 | `	SyString *pNames)` |
|      4 | 3591 | `{` |
|     28 | 3592 | `	ph7_vm *pVm = pCtx->pVm;` |
|     28 | 3593 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3594 | `	ph7_class_instance *pObj;` |
|      - | 3595 | `	ph7_class_method *pCons;` |
|      - | 3596 | `	sxi32 iCtorVis, iCloneVis, rc;` |
|     28 | 3597 | `	if( pClass == 0 ){` |
|    ! 0 | 3598 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3599 | `		return PH7_OK;` |
|      - | 3600 | `	}` |
|     28 | 3601 | `	rc = PH7_VmCheckInstantiable(pCtx, pClass);` |
|     28 | 3602 | `	if( rc != PH7_OK ){` |
|     10 | 3603 | `		return rc;` |
|      - | 3604 | `	}` |
|     19 | 3605 | `	ReflectCtorCloneVis(pVm, pClass, &iCtorVis, &iCloneVis);` |
|     19 | 3606 | `	if( iCtorVis != 0 && iCtorVis != PH7_CLASS_PROT_PUBLIC ){` |
|      4 | 3607 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 3608 | `			"Access to non-public constructor of class %z", &pClass->sName);` |
|      - | 3609 | `	}` |
|     17 | 3610 | `	if( iCtorVis == 0 && nCtor > 0 ){` |
|      4 | 3611 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 3612 | `			"Class %z does not have a constructor, so you cannot pass any constructor arguments",` |
|      1 | 3613 | `			&pClass->sName);` |
|      - | 3614 | `	}` |
|     15 | 3615 | `	if( VmClassStaticDeferPending(pClass) ){` |
|      - | 3616 | `		/* Instantiation materializes the static table (OP_NEW does it too), so a` |
|      - | 3617 | `		 * broken default raises BEFORE any object exists. */` |
|      3 | 3618 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pVm, pClass);` |
|      3 | 3619 | `		if( rcMat != SXRET_OK ){` |
|      3 | 3620 | `			return rcMat;` |
|      - | 3621 | `		}` |
|    ! 0 | 3622 | `	}` |
|     12 | 3623 | `	pObj = PH7_NewClassInstance(pVm, pClass);` |
|     12 | 3624 | `	if( pObj == 0 ){` |
|    ! 0 | 3625 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3626 | `		return PH7_OK;` |
|      - | 3627 | `	}` |
|     12 | 3628 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|     12 | 3629 | `	if( pCons ){` |
|     10 | 3630 | `		if( pNames ){` |
|      - | 3631 | `			VmCallArgMap sMap;` |
|    ! 0 | 3632 | `			SyZero(&sMap, sizeof(sMap));` |
|    ! 0 | 3633 | `			sMap.bHasNamed = 1;` |
|    ! 0 | 3634 | `			sMap.nTotal = (sxu32)nCtor;` |
|    ! 0 | 3635 | `			sMap.aNames = pNames;` |
|    ! 0 | 3636 | `			rc = PH7_VmCallClassMethodMap(pVm, pObj, pCons, 0, nCtor, apCtor, &sMap);` |
|    ! 0 | 3637 | `		}else{` |
|     10 | 3638 | `			rc = PH7_VmCallClassMethod(pVm, pObj, pCons, 0, nCtor, apCtor);` |
|      - | 3639 | `		}` |
|     10 | 3640 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|      3 | 3641 | `			PH7_ClassInstanceUnref(pObj);` |
|      3 | 3642 | `			return rc;` |
|      - | 3643 | `		}` |
|      3 | 3644 | `	}` |
|      9 | 3645 | `	return ReflectResultObject(pCtx, pObj);` |
|     16 | 3646 | `}` |
|     22 | 3647 | `static int vm_builtin_ReflectionClass_newInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 | 3648 | `{` |
|     26 | 3649 | `	return ReflectNewInstance(pCtx, nArg, apArg, 0);` |
|      4 | 3650 | `}` |
|      2 | 3651 | `static int vm_builtin_ReflectionClass_newInstanceArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3652 | `{` |
|      - | 3653 | `	SySet aArg;` |
|      3 | 3654 | `	SyString *aNames = 0;` |
|      - | 3655 | `	int rc;` |
|      3 | 3656 | `	SySetInit(&aArg, &pCtx->pVm->sAllocator, sizeof(ph7_value *));` |
|      3 | 3657 | `	if( nArg > 0 ){` |
|      3 | 3658 | `		ReflectCollectArgs(pCtx, apArg[0], &aArg, &aNames);` |
|      1 | 3659 | `	}` |
|      4 | 3660 | `	rc = ReflectNewInstance(pCtx, (int)SySetUsed(&aArg),` |
|      2 | 3661 | `		(ph7_value **)SySetBasePtr(&aArg), aNames);` |
|      3 | 3662 | `	if( aNames ){` |
|    ! 0 | 3663 | `		SyMemBackendFree(&pCtx->pVm->sAllocator, aNames);` |
|    ! 0 | 3664 | `	}` |
|      3 | 3665 | `	SySetRelease(&aArg);` |
|      3 | 3666 | `	return rc;` |
|      1 | 3667 | `}` |
|    164 | 3668 | `static int vm_builtin_ReflectionClass_newInstanceWithoutConstructor(ph7_context *pCtx,` |
|      - | 3669 | `	int nArg, ph7_value **apArg)` |
|      2 | 3670 | `{` |
|    166 | 3671 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3672 | `	sxi32 rc;` |
|     82 | 3673 | `	SXUNUSED(nArg);` |
|     82 | 3674 | `	SXUNUSED(apArg);` |
|    166 | 3675 | `	if( pClass == 0 ){` |
|    ! 0 | 3676 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3677 | `		return PH7_OK;` |
|      - | 3678 | `	}` |
|    166 | 3679 | `	rc = PH7_VmCheckInstantiable(pCtx, pClass);` |
|    166 | 3680 | `	if( rc != PH7_OK ){` |
|      3 | 3681 | `		return rc;` |
|      - | 3682 | `	}` |
|    164 | 3683 | `	if( VmClassStaticDeferPending(pClass) ){` |
|      3 | 3684 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);` |
|      3 | 3685 | `		if( rcMat != SXRET_OK ){` |
|      3 | 3686 | `			return rcMat;` |
|      - | 3687 | `		}` |
|    ! 0 | 3688 | `	}` |
|    162 | 3689 | `	return ReflectResultObject(pCtx, PH7_NewClassInstance(pCtx->pVm, pClass));` |
|     84 | 3690 | `}` |
|      - | 3691 | `/* ---- members ---- */` |
|      - | 3692 | `/* The modifier mask php filters a member on. */` |
|    178 | 3693 | `static sxi64 ReflectVisMask(sxi32 iProt)` |
|      2 | 3694 | `{` |
|    180 | 3695 | `	if( iProt == PH7_CLASS_PROT_PUBLIC ){` |
|    114 | 3696 | `		return 1;` |
|      - | 3697 | `	}` |
|     67 | 3698 | `	return iProt == PH7_CLASS_PROT_PROTECTED ? 2 : 4;` |
|     91 | 3699 | `}` |
|      - | 3700 | `/*` |
|      - | 3701 | ` * Is this property protected(set) as far as php's Reflection is concerned?` |
|      - | 3702 | ` *` |
|      - | 3703 | `` * The bit is either DECLARED or implied by `readonly` — but readonly implies it`` |
|      - | 3704 | `` * only for a property whose read side is PUBLIC. A `protected readonly` or`` |
|      - | 3705 | `` * `private readonly` one already writes no wider than it reads, so php adds`` |
|      - | 3706 | `` * nothing (`private readonly int $v` is modifiers 132, not 2180), and an explicit`` |
|      - | 3707 | `` * `private(set)` beside readonly is the set visibility, so readonly adds nothing`` |
|      - | 3708 | `` * there either (`public private(set) readonly` is 4257).`` |
|      - | 3709 | ` */` |
|    158 | 3710 | `static int ReflectPropProtectedSet(ph7_class_attr *pAttr)` |
|      2 | 3711 | `{` |
|    160 | 3712 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET ){` |
|     28 | 3713 | `		return 1;` |
|      - | 3714 | `	}` |
|    153 | 3715 | `	return (pAttr->iFlags & PH7_CLASS_ATTR_READONLY)` |
|     86 | 3716 | `	    && (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) == 0` |
|    152 | 3717 | `	    && pAttr->iProtection == PH7_CLASS_PROT_PUBLIC;` |
|     81 | 3718 | `}` |
|    136 | 3719 | `static sxi64 ReflectPropModifiers(ph7_class_attr *pAttr)` |
|      2 | 3720 | `{` |
|    138 | 3721 | `	sxi64 iMods = ReflectVisMask(pAttr->iProtection);` |
|    138 | 3722 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){ iMods \|= 16; }` |
|      - | 3723 | `	/* php's IS_VIRTUAL is "there is no slot behind this name", and it has two` |
|      - | 3724 | `	 * sources: a HOOKED property with no backing store, and a NATIVE class's` |
|      - | 3725 | `	 * property that php fabricates from its own C struct (DatePeriod's, and` |
|      - | 3726 | `	 * BcMath\Number's value/scale). Both report virtual there. */` |
|    138 | 3727 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_VIRTUAL\|PH7_CLASS_ATTR_NATIVE_VIRTUAL) ){` |
|     13 | 3728 | `		iMods \|= 512;` |
|      6 | 3729 | `	}` |
|    138 | 3730 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){ iMods \|= 128; }` |
|    138 | 3731 | `	if( ReflectPropProtectedSet(pAttr) ){ iMods \|= 2048; }` |
|    138 | 3732 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|      - | 3733 | `		/* private(set) cannot be widened by a subclass, so php reports it FINAL. */` |
|     17 | 3734 | `		iMods \|= 4096\|32;` |
|      8 | 3735 | `	}` |
|    138 | 3736 | `	return iMods;` |
|      2 | 3737 | `}` |
|     16 | 3738 | `static sxi64 ReflectMethodModifiers(ph7_class_method *pMeth)` |
|      1 | 3739 | `{` |
|     17 | 3740 | `	sxi64 iMods = ReflectVisMask(pMeth->iProtection);` |
|     17 | 3741 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){ iMods \|= 16; }` |
|     17 | 3742 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){ iMods \|= 64; }` |
|     17 | 3743 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods \|= 32; }` |
|     17 | 3744 | `	return iMods;` |
|      1 | 3745 | `}` |
|     26 | 3746 | `static sxi64 ReflectConstModifiers(ph7_class_attr *pAttr)` |
|      1 | 3747 | `{` |
|     27 | 3748 | `	sxi64 iMods = ReflectVisMask(pAttr->iProtection);` |
|     27 | 3749 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods \|= 32; }` |
|     27 | 3750 | `	return iMods;` |
|      1 | 3751 | `}` |
|      - | 3752 | `/* Does the reflected object own a property under this name? (1 = exists) */` |
|     16 | 3753 | `static int ReflectObjHasProp(ph7_class_instance *pObj, const char *zName, int nName)` |
|      1 | 3754 | `{` |
|     13 | 3755 | `	return pObj != 0 && nName > 0` |
|     18 | 3756 | `		&& SyHashGet(&pObj->hAttr, (const void *)zName, (sxu32)nName) != 0;` |
|      1 | 3757 | `}` |
|     12 | 3758 | `static int vm_builtin_ReflectionClass_hasMethod(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3759 | `{` |
|     13 | 3760 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3761 | `	const char *zName;` |
|      - | 3762 | `	int nName;` |
|     13 | 3763 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 3764 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3765 | `		return PH7_OK;` |
|      - | 3766 | `	}` |
|     13 | 3767 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     13 | 3768 | `	ph7_result_bool(pCtx, ReflectFindMethodEntry(pClass, zName, nName) != 0);` |
|     13 | 3769 | `	return PH7_OK;` |
|      7 | 3770 | `}` |
|     16 | 3771 | `static int vm_builtin_ReflectionClass_hasProperty(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 3772 | `{` |
|     18 | 3773 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3774 | `	const char *zName;` |
|     18 | 3775 | `	int nName, bFound = 0;` |
|      - | 3776 | `	SySet aMembers;` |
|      - | 3777 | `	sxu32 n;` |
|     18 | 3778 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 3779 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3780 | `		return PH7_OK;` |
|      - | 3781 | `	}` |
|     18 | 3782 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     18 | 3783 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     18 | 3784 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     48 | 3785 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     38 | 3786 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     38 | 3787 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zName, nName) ){` |
|      8 | 3788 | `			bFound = 1;` |
|      8 | 3789 | `			break;` |
|      - | 3790 | `		}` |
|     16 | 3791 | `	}` |
|     18 | 3792 | `	SySetRelease(&aMembers);` |
|      - | 3793 | `	/* A ReflectionObject also sees the instance's own dynamic properties */` |
|     18 | 3794 | `	if( !bFound ){` |
|     11 | 3795 | `		bFound = ReflectObjHasProp(ReflectClassObj(pCtx), zName, nName);` |
|      5 | 3796 | `	}` |
|     18 | 3797 | `	ph7_result_bool(pCtx, bFound);` |
|     18 | 3798 | `	return PH7_OK;` |
|     10 | 3799 | `}` |
|     22 | 3800 | `static int vm_builtin_ReflectionClass_hasConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 3801 | `{` |
|     24 | 3802 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3803 | `	const char *zName;` |
|     24 | 3804 | `	int nName, bFound = 0;` |
|      - | 3805 | `	SySet aMembers;` |
|      - | 3806 | `	sxu32 n;` |
|     24 | 3807 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 3808 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3809 | `		return PH7_OK;` |
|      - | 3810 | `	}` |
|     24 | 3811 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     24 | 3812 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     24 | 3813 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|   1310 | 3814 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   1292 | 3815 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   1292 | 3816 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|      5 | 3817 | `			bFound = 1;` |
|      5 | 3818 | `			break;` |
|      - | 3819 | `		}` |
|    645 | 3820 | `	}` |
|     24 | 3821 | `	SySetRelease(&aMembers);` |
|     24 | 3822 | `	ph7_result_bool(pCtx, bFound);` |
|     24 | 3823 | `	return PH7_OK;` |
|     13 | 3824 | `}` |
|      - | 3825 | `/*` |
|      - | 3826 | ` * The value of a class constant, materializing its lazily-evaluated slot.` |
|      - | 3827 | ` *` |
|      - | 3828 | ` * php re-evaluates a failed initializer on EVERY read, so this can raise; the` |
|      - | 3829 | ` * status is returned rather than swallowed, because a native body that answers` |
|      - | 3830 | ` * PH7_OK with a throw in flight lets the caller carry on and print the NULL it` |
|      - | 3831 | ` * never should have seen.` |
|      - | 3832 | ` */` |
|    138 | 3833 | `static sxi32 ReflectConstSlot(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr,` |
|      - | 3834 | `	ph7_value **ppOut)` |
|      3 | 3835 | `{` |
|    141 | 3836 | `	sxi32 rc = PH7_VmMaterializeClassConst(pCtx->pVm, pClass, pAttr);` |
|    141 | 3837 | `	*ppOut = 0;` |
|    141 | 3838 | `	if( rc != SXRET_OK ){` |
|      3 | 3839 | `		return rc;` |
|      - | 3840 | `	}` |
|    138 | 3841 | `	*ppOut = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|    138 | 3842 | `	return SXRET_OK;` |
|     72 | 3843 | `}` |
|      6 | 3844 | `static int vm_builtin_ReflectionClass_getConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 3845 | `{` |
|      8 | 3846 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3847 | `	const char *zName;` |
|      - | 3848 | `	int nName;` |
|      - | 3849 | `	SySet aMembers;` |
|      - | 3850 | `	sxu32 n;` |
|      8 | 3851 | `	ph7_class_attr *pFound = 0;` |
|      8 | 3852 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 3853 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3854 | `		return PH7_OK;` |
|      - | 3855 | `	}` |
|      8 | 3856 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|      8 | 3857 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|      8 | 3858 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     16 | 3859 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     16 | 3860 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     16 | 3861 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|      8 | 3862 | `			pFound = pM->pAttr;` |
|      8 | 3863 | `			break;` |
|      - | 3864 | `		}` |
|      5 | 3865 | `	}` |
|      8 | 3866 | `	SySetRelease(&aMembers);` |
|      8 | 3867 | `	if( pFound == 0 ){` |
|    ! 0 | 3868 | `		PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED,` |
|      - | 3869 | `			"ReflectionClass::getConstant() for a non-existent constant is deprecated, "` |
|      - | 3870 | `			"use ReflectionClass::hasConstant() to check if the constant exists");` |
|    ! 0 | 3871 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3872 | `		return PH7_OK;` |
|      - | 3873 | `	}` |
|      - | 3874 | `	{` |
|      - | 3875 | `		ph7_value *pVal;` |
|      8 | 3876 | `		sxi32 rc = ReflectConstSlot(pCtx, pClass, pFound, &pVal);` |
|      8 | 3877 | `		if( rc != SXRET_OK ){` |
|      3 | 3878 | `			return rc;` |
|      - | 3879 | `		}` |
|      5 | 3880 | `		if( pVal ){` |
|      5 | 3881 | `			ph7_result_value(pCtx, pVal);` |
|      3 | 3882 | `		}else{` |
|    ! 0 | 3883 | `			ph7_result_null(pCtx);` |
|      - | 3884 | `		}` |
|      - | 3885 | `	}` |
|      5 | 3886 | `	return PH7_OK;` |
|      5 | 3887 | `}` |
|     24 | 3888 | `static int vm_builtin_ReflectionClass_getConstants(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 3889 | `{` |
|     26 | 3890 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     26 | 3891 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - | 3892 | `	SySet aMembers;` |
|      - | 3893 | `	sxu32 n;` |
|     26 | 3894 | `	int bFilter = (nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0);` |
|     26 | 3895 | `	sxi64 iFilter = bFilter ? ph7_value_to_int64(apArg[0]) : 0;` |
|     26 | 3896 | `	if( pOut == 0 ){` |
|    ! 0 | 3897 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3898 | `	}` |
|     26 | 3899 | `	if( pClass == 0 ){` |
|    ! 0 | 3900 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 | 3901 | `		return PH7_OK;` |
|      - | 3902 | `	}` |
|     26 | 3903 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     26 | 3904 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|    194 | 3905 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    170 | 3906 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - | 3907 | `		ph7_value *pVal;` |
|    170 | 3908 | `		if( pM->iKind != REFLECT_MEMBER_CONST ){` |
|    102 | 3909 | `			continue;` |
|      - | 3910 | `		}` |
|     72 | 3911 | `		if( bFilter && (ReflectConstModifiers(pM->pAttr) & iFilter) == 0 ){` |
|      5 | 3912 | `			continue;` |
|      - | 3913 | `		}` |
|      - | 3914 | `		{` |
|     68 | 3915 | `			sxi32 rc = ReflectConstSlot(pCtx, pClass, pM->pAttr, &pVal);` |
|     68 | 3916 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3917 | `				SySetRelease(&aMembers);` |
|    ! 0 | 3918 | `				return rc;` |
|      - | 3919 | `			}` |
|      - | 3920 | `		}` |
|     68 | 3921 | `		if( pVal ){` |
|     68 | 3922 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, pVal);` |
|     33 | 3923 | `		}` |
|     35 | 3924 | `	}` |
|     26 | 3925 | `	SySetRelease(&aMembers);` |
|     26 | 3926 | `	ph7_result_value(pCtx, pOut);` |
|     26 | 3927 | `	return PH7_OK;` |
|     14 | 3928 | `}` |
|      - | 3929 | `/*` |
|      - | 3930 | ` * getMethod()/getProperty()/getReflectionConstant() build a class that is still` |
|      - | 3931 | ` * PRELUDE PHP, so they go through its constructor (ReflectConstruct). They` |
|      - | 3932 | ` * become direct C when chunks 2 and 3 land.` |
|      - | 3933 | ` */` |
|     56 | 3934 | `static int ReflectResultMember(ph7_context *pCtx, const char *zClass,` |
|      - | 3935 | `	ph7_value *pTarget, const SyString *pName)` |
|      3 | 3936 | `{` |
|      - | 3937 | `	ph7_value sName;` |
|      - | 3938 | `	ph7_value *apCtor[2];` |
|      - | 3939 | `	ph7_class_instance *pOut;` |
|      - | 3940 | `	sxi32 rc;` |
|     59 | 3941 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|     59 | 3942 | `	ph7_value_string(&sName, SyStringData(pName), (int)SyStringLength(pName));` |
|     59 | 3943 | `	apCtor[0] = pTarget;` |
|     59 | 3944 | `	apCtor[1] = &sName;` |
|     59 | 3945 | `	pOut = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|     59 | 3946 | `	PH7_MemObjRelease(&sName);` |
|     59 | 3947 | `	if( pOut == 0 ){` |
|    ! 0 | 3948 | `		if( rc != PH7_OK ){` |
|    ! 0 | 3949 | `			return rc;` |
|      - | 3950 | `		}` |
|    ! 0 | 3951 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3952 | `		return PH7_OK;` |
|      - | 3953 | `	}` |
|     59 | 3954 | `	return ReflectResultObject(pCtx, pOut);` |
|     31 | 3955 | `}` |
|      - | 3956 | ``/* A `ReflectionMethod($this->name, ...)`-shaped first argument. */`` |
|    232 | 3957 | `static void ReflectSelfName(ph7_context *pCtx, ph7_value *pOut)` |
|      5 | 3958 | `{` |
|      - | 3959 | `	const char *zName;` |
|      - | 3960 | `	int nName;` |
|    237 | 3961 | `	ReflectClassName(pCtx, &zName, &nName);` |
|    237 | 3962 | `	PH7_MemObjInit(pCtx->pVm, pOut);` |
|    237 | 3963 | `	ph7_value_string(pOut, zName, nName);` |
|    237 | 3964 | `}` |
|     34 | 3965 | `static int vm_builtin_ReflectionClass_getMethod(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3966 | `{` |
|     35 | 3967 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3968 | `	SyHashEntry *pEntry;` |
|      - | 3969 | `	const char *zName;` |
|      - | 3970 | `	int nName;` |
|      - | 3971 | `	SyString sFound;` |
|     35 | 3972 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 3973 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3974 | `		return PH7_OK;` |
|      - | 3975 | `	}` |
|     35 | 3976 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     35 | 3977 | `	pEntry = ReflectFindMethodEntry(pClass, zName, nName);` |
|     35 | 3978 | `	if( pEntry == 0 ){` |
|      4 | 3979 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 3980 | `			"Method %z::%.*s() does not exist", &pClass->sName, nName, zName);` |
|      - | 3981 | `	}` |
|      - | 3982 | `	/* The reported name is the DECLARED spelling, whatever case was asked for. */` |
|     33 | 3983 | `	SyStringInitFromBuf(&sFound, (const char *)pEntry->pKey, pEntry->nKeyLen);` |
|      - | 3984 | `	{` |
|      - | 3985 | `		ph7_value sSelf;` |
|      - | 3986 | `		int rc;` |
|     33 | 3987 | `		ReflectSelfName(pCtx, &sSelf);` |
|     33 | 3988 | `		rc = ReflectResultMember(pCtx, "ReflectionMethod", &sSelf, &sFound);` |
|     33 | 3989 | `		PH7_MemObjRelease(&sSelf);` |
|     33 | 3990 | `		return rc;` |
|      - | 3991 | `	}` |
|     18 | 3992 | `}` |
|     26 | 3993 | `static int vm_builtin_ReflectionClass_getConstructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 3994 | `{` |
|     29 | 3995 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3996 | `	SyHashEntry *pEntry;` |
|      - | 3997 | `	SyString sFound;` |
|     13 | 3998 | `	SXUNUSED(nArg);` |
|     13 | 3999 | `	SXUNUSED(apArg);` |
|     29 | 4000 | `	if( pClass == 0 ){` |
|    ! 0 | 4001 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4002 | `		return PH7_OK;` |
|      - | 4003 | `	}` |
|      - | 4004 | `	/* No PHP-4 class-name constructor: a method named like the class is a plain` |
|      - | 4005 | `	 * method (removed in 8.0), so this stays null without an explicit one. */` |
|     29 | 4006 | `	pEntry = ReflectFindMethodEntry(pClass, "__construct", sizeof("__construct")-1);` |
|     29 | 4007 | `	if( pEntry == 0 ){` |
|     10 | 4008 | `		ph7_result_null(pCtx);` |
|     10 | 4009 | `		return PH7_OK;` |
|      - | 4010 | `	}` |
|     21 | 4011 | `	SyStringInitFromBuf(&sFound, (const char *)pEntry->pKey, pEntry->nKeyLen);` |
|      - | 4012 | `	{` |
|      - | 4013 | `		ph7_value sSelf;` |
|      - | 4014 | `		int rc;` |
|     21 | 4015 | `		ReflectSelfName(pCtx, &sSelf);` |
|     21 | 4016 | `		rc = ReflectResultMember(pCtx, "ReflectionMethod", &sSelf, &sFound);` |
|     21 | 4017 | `		PH7_MemObjRelease(&sSelf);` |
|     21 | 4018 | `		return rc;` |
|      - | 4019 | `	}` |
|     16 | 4020 | `}` |
|      - | 4021 | `/*` |
|      - | 4022 | ` * getMethods() / getProperties() / getReflectionConstants(): one member walk,` |
|      - | 4023 | ` * one reflector per surviving member. Each reflector is a prelude class built` |
|      - | 4024 | ` * through its constructor, and is appended through a STACK carrier so the list` |
|      - | 4025 | ` * takes its own reference rather than the creation one.` |
|      - | 4026 | ` */` |
|    178 | 4027 | `static int ReflectMemberList(ph7_context *pCtx, int iKind, const char *zClass,` |
|      - | 4028 | `	int nArg, ph7_value **apArg)` |
|      4 | 4029 | `{` |
|    182 | 4030 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|    182 | 4031 | `	ph7_class_instance *pObj = ReflectClassObj(pCtx);` |
|    182 | 4032 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - | 4033 | `	ph7_value sSelf;` |
|      - | 4034 | `	SySet aMembers;` |
|      - | 4035 | `	sxu32 n;` |
|    182 | 4036 | `	int bFilter = (nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0);` |
|    182 | 4037 | `	sxi64 iFilter = bFilter ? ph7_value_to_int64(apArg[0]) : 0;` |
|    182 | 4038 | `	if( pOut == 0 ){` |
|    ! 0 | 4039 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4040 | `	}` |
|    182 | 4041 | `	if( pClass == 0 ){` |
|    ! 0 | 4042 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 | 4043 | `		return PH7_OK;` |
|      - | 4044 | `	}` |
|    182 | 4045 | `	ReflectSelfName(pCtx, &sSelf);` |
|    182 | 4046 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|    182 | 4047 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|   2226 | 4048 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   2048 | 4049 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - | 4050 | `		ph7_value sName, sVal;` |
|      - | 4051 | `		ph7_value *apCtor[2];` |
|      - | 4052 | `		ph7_class_instance *pRef;` |
|      - | 4053 | `		sxi32 rc;` |
|   2048 | 4054 | `		if( pM->iKind != iKind ){` |
|   1433 | 4055 | `			continue;` |
|      - | 4056 | `		}` |
|    658 | 4057 | `		if( iKind == REFLECT_MEMBER_PROP && pObj != 0 && pM->pAttr != 0` |
|    142 | 4058 | `		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT) == 0` |
|    132 | 4059 | `		 && PH7_ATTR_LAZY_ABSENT(pM->pAttr,pObj) ){` |
|      - | 4060 | `			/* A ReflectionObject reflects the OBJECT, and a LAZY property php does` |
|      - | 4061 | `			 * not DECLARE (DateInterval's ten) is not on one that was never` |
|      - | 4062 | `			 * constructed -- php reports none there either. The declared-and-virtual` |
|      - | 4063 | `			 * kind (DatePeriod's seven, PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT) is` |
|      - | 4064 | `			 * reported whatever the object holds, because php declares it. */` |
|     45 | 4065 | `			continue;` |
|      - | 4066 | `		}` |
|    614 | 4067 | `		if( iKind == REFLECT_MEMBER_PROP && pObj == 0 && pM->pAttr != 0` |
|    150 | 4068 | `		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND) != 0 ){` |
|      - | 4069 | `			/* An ON-DEMAND property belongs to the objects that took it, so the` |
|      - | 4070 | `			 * CLASS's list -- which php builds from declarations and DateInterval` |
|      - | 4071 | `			 * has none of -- does not gain a name for it. */` |
|    ! 0 | 4072 | `			continue;` |
|      - | 4073 | `		}` |
|    614 | 4074 | `		if( iKind == REFLECT_MEMBER_PROP && pObj != 0 && pM->pAttr != 0` |
|     98 | 4075 | `		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY) != 0` |
|    100 | 4076 | `		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT) == 0 ){` |
|      - | 4077 | `			/* ...and for the same reason a property the object HIDES or never took` |
|      - | 4078 | `			 * is not on it either: php's from-string DateInterval reflects the two` |
|      - | 4079 | ``			 * names it presents, and an ordinary one has no `date_string` at all. */`` |
|    100 | 4080 | `			SyHashEntry *pOwn = SyHashGet(&pObj->hAttr,` |
|     66 | 4081 | `				SyStringData(&pM->pAttr->sName),SyStringLength(&pM->pAttr->sName));` |
|     66 | 4082 | `			if( pOwn == 0` |
|     65 | 4083 | `			 \|\| (((VmClassAttr *)pOwn->pUserData)->iState & VM_CLASS_ATTR_UNSEEN) ){` |
|     23 | 4084 | `				continue;` |
|      - | 4085 | `			}` |
|     22 | 4086 | `		}` |
|    596 | 4087 | `		if( bFilter ){` |
|     33 | 4088 | `			sxi64 iMods = iKind == REFLECT_MEMBER_METHOD ? ReflectMethodModifiers(pM->pMeth)` |
|     40 | 4089 | `				: (iKind == REFLECT_MEMBER_CONST ? ReflectConstModifiers(pM->pAttr)` |
|     20 | 4090 | `				                                 : ReflectPropModifiers(pM->pAttr));` |
|     33 | 4091 | `			if( (iMods & iFilter) == 0 ){` |
|     21 | 4092 | `				continue;` |
|      - | 4093 | `			}` |
|      6 | 4094 | `		}` |
|    576 | 4095 | `		PH7_MemObjInit(pCtx->pVm, &sName);` |
|    576 | 4096 | `		ph7_value_string(&sName, SyStringData(&pM->sKey), (int)SyStringLength(&pM->sKey));` |
|    576 | 4097 | `		apCtor[0] = &sSelf;` |
|    576 | 4098 | `		apCtor[1] = &sName;` |
|    576 | 4099 | `		pRef = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|    576 | 4100 | `		PH7_MemObjRelease(&sName);` |
|    576 | 4101 | `		if( pRef == 0 ){` |
|    ! 0 | 4102 | `			SySetRelease(&aMembers);` |
|    ! 0 | 4103 | `			PH7_MemObjRelease(&sSelf);` |
|    ! 0 | 4104 | `			if( rc != PH7_OK ){` |
|    ! 0 | 4105 | `				return rc;` |
|      - | 4106 | `			}` |
|    ! 0 | 4107 | `			ph7_result_value(pCtx, pOut);` |
|    ! 0 | 4108 | `			return PH7_OK;` |
|      - | 4109 | `		}` |
|    576 | 4110 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    576 | 4111 | `		sVal.x.pOther = pRef;` |
|    576 | 4112 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|    576 | 4113 | `		ph7_array_add_elem(pOut, 0, &sVal);   /* takes its own reference */` |
|    576 | 4114 | `		PH7_ClassInstanceUnref(pRef);` |
|    290 | 4115 | `	}` |
|    182 | 4116 | `	SySetRelease(&aMembers);` |
|      - | 4117 | `	/* A ReflectionObject also reports the instance's own DYNAMIC properties,` |
|      - | 4118 | `	 * which no class declaration knows about. */` |
|    182 | 4119 | `	if( iKind == REFLECT_MEMBER_PROP && pObj != 0 && (!bFilter \|\| (iFilter & 1)) ){` |
|      - | 4120 | `		SyHashEntry *pEntry;` |
|      - | 4121 | `		ph7_value sTarget;` |
|     17 | 4122 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     17 | 4123 | `		sTarget.x.pOther = pObj;` |
|     17 | 4124 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|     17 | 4125 | `		SyHashResetLoopCursor(&pObj->hAttr);` |
|    111 | 4126 | `		while( (pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){` |
|     95 | 4127 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 4128 | `			ph7_value sName, sVal;` |
|      - | 4129 | `			ph7_value *apCtor[2];` |
|      - | 4130 | `			ph7_class_instance *pRef;` |
|      - | 4131 | `			sxi32 rc;` |
|     95 | 4132 | `			if( pVmAttr->pAttr == 0 \|\| (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) == 0 ){` |
|     91 | 4133 | `				continue;` |
|      - | 4134 | `			}` |
|      5 | 4135 | `			PH7_MemObjInit(pCtx->pVm, &sName);` |
|      7 | 4136 | `			ph7_value_string(&sName, SyStringData(&pVmAttr->pAttr->sName),` |
|      4 | 4137 | `				(int)SyStringLength(&pVmAttr->pAttr->sName));` |
|      5 | 4138 | `			apCtor[0] = &sTarget;` |
|      5 | 4139 | `			apCtor[1] = &sName;` |
|      5 | 4140 | `			pRef = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|      5 | 4141 | `			PH7_MemObjRelease(&sName);` |
|      5 | 4142 | `			if( pRef == 0 ){` |
|    ! 0 | 4143 | `				break;` |
|      - | 4144 | `			}` |
|      5 | 4145 | `			PH7_MemObjInit(pCtx->pVm, &sVal);` |
|      5 | 4146 | `			sVal.x.pOther = pRef;` |
|      5 | 4147 | `			sVal.iFlags = MEMOBJ_OBJ;` |
|      5 | 4148 | `			ph7_array_add_elem(pOut, 0, &sVal);` |
|      5 | 4149 | `			PH7_ClassInstanceUnref(pRef);` |
|      1 | 4150 | `		}` |
|      - | 4151 | `		/* sTarget borrows pObj and never took a reference: nothing to release. */` |
|      8 | 4152 | `	}` |
|    182 | 4153 | `	PH7_MemObjRelease(&sSelf);` |
|    182 | 4154 | `	ph7_result_value(pCtx, pOut);` |
|    182 | 4155 | `	return PH7_OK;` |
|     93 | 4156 | `}` |
|     70 | 4157 | `static int vm_builtin_ReflectionClass_getMethods(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 4158 | `{` |
|     73 | 4159 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_METHOD, "ReflectionMethod", nArg, apArg);` |
|      3 | 4160 | `}` |
|    102 | 4161 | `static int vm_builtin_ReflectionClass_getProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 4162 | `{` |
|    105 | 4163 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_PROP, "ReflectionProperty", nArg, apArg);` |
|      3 | 4164 | `}` |
|      6 | 4165 | `static int vm_builtin_ReflectionClass_getReflectionConstants(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 4166 | `{` |
|      7 | 4167 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_CONST, "ReflectionClassConstant", nArg, apArg);` |
|      1 | 4168 | `}` |
|      8 | 4169 | `static int vm_builtin_ReflectionClass_getProperty(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 4170 | `{` |
|      9 | 4171 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      9 | 4172 | `	ph7_class_instance *pObj = ReflectClassObj(pCtx);` |
|      - | 4173 | `	const char *zName;` |
|      9 | 4174 | `	int nName, bFound = 0;` |
|      - | 4175 | `	SySet aMembers;` |
|      - | 4176 | `	sxu32 n;` |
|      - | 4177 | `	SyString sFound;` |
|      9 | 4178 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 4179 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4180 | `		return PH7_OK;` |
|      - | 4181 | `	}` |
|      9 | 4182 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|      9 | 4183 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|      9 | 4184 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     41 | 4185 | `	for( n = 0 ; n < SySetUsed(&aMembers) && !bFound ; n++ ){` |
|     33 | 4186 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     33 | 4187 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zName, nName) ){` |
|      3 | 4188 | `			sFound = pM->sKey;` |
|      3 | 4189 | `			bFound = 1;` |
|      1 | 4190 | `		}` |
|     17 | 4191 | `	}` |
|      9 | 4192 | `	SySetRelease(&aMembers);` |
|      9 | 4193 | `	if( bFound ){` |
|      - | 4194 | `		ph7_value sSelf;` |
|      - | 4195 | `		int rc;` |
|      3 | 4196 | `		ReflectSelfName(pCtx, &sSelf);` |
|      3 | 4197 | `		rc = ReflectResultMember(pCtx, "ReflectionProperty", &sSelf, &sFound);` |
|      3 | 4198 | `		PH7_MemObjRelease(&sSelf);` |
|      3 | 4199 | `		return rc;` |
|      - | 4200 | `	}` |
|      7 | 4201 | `	if( ReflectObjHasProp(pObj, zName, nName) ){` |
|      - | 4202 | `		/* A dynamic property: the reflector is built over the OBJECT, since the` |
|      - | 4203 | `		 * class declaration has no record of it. */` |
|      - | 4204 | `		ph7_value sTarget;` |
|      - | 4205 | `		SyString sName;` |
|      - | 4206 | `		int rc;` |
|      3 | 4207 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      3 | 4208 | `		sTarget.x.pOther = pObj;` |
|      3 | 4209 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|      3 | 4210 | `		SyStringInitFromBuf(&sName, zName, nName);` |
|      3 | 4211 | `		rc = ReflectResultMember(pCtx, "ReflectionProperty", &sTarget, &sName);` |
|      3 | 4212 | `		return rc;` |
|      - | 4213 | `	}` |
|      7 | 4214 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 | 4215 | `		"Property %z::$%.*s does not exist", &pClass->sName, nName, zName);` |
|      5 | 4216 | `}` |
|      4 | 4217 | `static int vm_builtin_ReflectionClass_getReflectionConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 4218 | `{` |
|      5 | 4219 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 4220 | `	const char *zName;` |
|      5 | 4221 | `	int nName, bFound = 0;` |
|      - | 4222 | `	SySet aMembers;` |
|      - | 4223 | `	sxu32 n;` |
|      - | 4224 | `	SyString sFound;` |
|      5 | 4225 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 4226 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 4227 | `		return PH7_OK;` |
|      - | 4228 | `	}` |
|      5 | 4229 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|      5 | 4230 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|      5 | 4231 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     15 | 4232 | `	for( n = 0 ; n < SySetUsed(&aMembers) && !bFound ; n++ ){` |
|     11 | 4233 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     11 | 4234 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|      3 | 4235 | `			sFound = pM->sKey;` |
|      3 | 4236 | `			bFound = 1;` |
|      1 | 4237 | `		}` |
|      6 | 4238 | `	}` |
|      5 | 4239 | `	SySetRelease(&aMembers);` |
|      5 | 4240 | `	if( !bFound ){` |
|      3 | 4241 | `		ph7_result_bool(pCtx, 0);` |
|      3 | 4242 | `		return PH7_OK;` |
|      - | 4243 | `	}` |
|      - | 4244 | `	{` |
|      - | 4245 | `		ph7_value sSelf;` |
|      - | 4246 | `		int rc;` |
|      3 | 4247 | `		ReflectSelfName(pCtx, &sSelf);` |
|      3 | 4248 | `		rc = ReflectResultMember(pCtx, "ReflectionClassConstant", &sSelf, &sFound);` |
|      3 | 4249 | `		PH7_MemObjRelease(&sSelf);` |
|      3 | 4250 | `		return rc;` |
|      - | 4251 | `	}` |
|      3 | 4252 | `}` |
|      - | 4253 | `/* ---- statics and defaults ---- */` |
|      - | 4254 | `/* Reading or writing a static through reflection materializes the class's` |
|      - | 4255 | `` * static table exactly as `C::$s` does, so a default that threw at the`` |
|      - | 4256 | ` * declaration raises HERE. */` |
|     50 | 4257 | `static sxi32 ReflectMaterializeStatics(ph7_context *pCtx, ph7_class *pClass)` |
|      2 | 4258 | `{` |
|     52 | 4259 | `	if( VmClassStaticDeferPending(pClass) ){` |
|     21 | 4260 | `		return PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);` |
|      - | 4261 | `	}` |
|     32 | 4262 | `	return SXRET_OK;` |
|     27 | 4263 | `}` |
|      6 | 4264 | `static int vm_builtin_ReflectionClass_getStaticProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 4265 | `{` |
|      8 | 4266 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      8 | 4267 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - | 4268 | `	SySet aMembers;` |
|      - | 4269 | `	sxu32 n;` |
|      3 | 4270 | `	SXUNUSED(nArg);` |
|      3 | 4271 | `	SXUNUSED(apArg);` |
|      8 | 4272 | `	if( pOut == 0 ){` |
|    ! 0 | 4273 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4274 | `	}` |
|      8 | 4275 | `	if( pClass == 0 ){` |
|    ! 0 | 4276 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 | 4277 | `		return PH7_OK;` |
|      - | 4278 | `	}` |
|      - | 4279 | `	{` |
|      8 | 4280 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|      8 | 4281 | `		if( rc != SXRET_OK ){` |
|      3 | 4282 | `			return rc;` |
|      - | 4283 | `		}` |
|      - | 4284 | `	}` |
|      5 | 4285 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|      5 | 4286 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     29 | 4287 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     25 | 4288 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - | 4289 | `		ph7_value *pVal;` |
|      - | 4290 | `		SyHashEntry *pSlot;` |
|     24 | 4291 | `		if( pM->iKind != REFLECT_MEMBER_PROP` |
|     24 | 4292 | `		 \|\| (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|     15 | 4293 | `			continue;` |
|      - | 4294 | `		}` |
|      - | 4295 | `		/* An UNINITIALIZED typed static has no value to report, and php simply` |
|      - | 4296 | `		 * leaves it out rather than raising the read Error here. */` |
|     11 | 4297 | `		pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pM->pAttr->nIdx, sizeof(sxu32));` |
|     11 | 4298 | `		if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|      3 | 4299 | `			continue;` |
|      - | 4300 | `		}` |
|      9 | 4301 | `		pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pM->pAttr->nIdx);` |
|      9 | 4302 | `		if( pVal ){` |
|      9 | 4303 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, pVal);` |
|      4 | 4304 | `		}` |
|      5 | 4305 | `	}` |
|      5 | 4306 | `	SySetRelease(&aMembers);` |
|      5 | 4307 | `	ph7_result_value(pCtx, pOut);` |
|      5 | 4308 | `	return PH7_OK;` |
|      5 | 4309 | `}` |
|      - | 4310 | `/* The declared STATIC property of this name, or NULL. */` |
|     18 | 4311 | `static ph7_class_attr * ReflectStaticAttr(ph7_context *pCtx, ph7_class *pClass,` |
|      - | 4312 | `	const char *zName, int nName)` |
|      2 | 4313 | `{` |
|      - | 4314 | `	SyHashEntry *pEntry;` |
|      - | 4315 | `	ph7_class_attr *pAttr;` |
|     20 | 4316 | `	if( nName < 1 ){` |
|    ! 0 | 4317 | `		return 0;` |
|      - | 4318 | `	}` |
|     20 | 4319 | `	pEntry = SyHashGet(&pClass->hAttr, (const void *)zName, (sxu32)nName);` |
|     20 | 4320 | `	if( pEntry == 0 ){` |
|      9 | 4321 | `		return 0;` |
|      - | 4322 | `	}` |
|     12 | 4323 | `	pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      5 | 4324 | `	SXUNUSED(pCtx);` |
|     12 | 4325 | `	return (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? pAttr : 0;` |
|     11 | 4326 | `}` |
|     20 | 4327 | `static int vm_builtin_ReflectionClass_getStaticPropertyValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 4328 | `{` |
|     22 | 4329 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 4330 | `	ph7_class_attr *pAttr;` |
|      - | 4331 | `	const char *zName;` |
|      - | 4332 | `	int nName;` |
|     22 | 4333 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 4334 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4335 | `		return PH7_OK;` |
|      - | 4336 | `	}` |
|      - | 4337 | `	{` |
|     22 | 4338 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|     22 | 4339 | `		if( rc != SXRET_OK ){` |
|      7 | 4340 | `			return rc;` |
|      - | 4341 | `		}` |
|      - | 4342 | `	}` |
|     16 | 4343 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     16 | 4344 | `	pAttr = ReflectStaticAttr(pCtx, pClass, zName, nName);` |
|     16 | 4345 | `	if( pAttr == 0 ){` |
|      7 | 4346 | `		if( nArg > 1 ){` |
|      5 | 4347 | `			ph7_result_value(pCtx, apArg[1]);` |
|      5 | 4348 | `			return PH7_OK;` |
|      - | 4349 | `		}` |
|      4 | 4350 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 4351 | `			"Property %z::$%.*s does not exist", &pClass->sName, nName, zName);` |
|      - | 4352 | `	}` |
|      - | 4353 | `	{` |
|      - | 4354 | `		/* Uninitialized typed static: the same Error the VM raises on read */` |
|     10 | 4355 | `		SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|      - | 4356 | `		ph7_value *pVal;` |
|     10 | 4357 | `		if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|      - | 4358 | `			/* php says "Typed property" HERE and "Typed static property" from` |
|      - | 4359 | ``			 * ReflectionProperty::getValue() and from `A::$s` itself — the`` |
|      - | 4360 | `			 * three wordings were checked against the oracle, they really do` |
|      - | 4361 | `			 * differ by call site. */` |
|      3 | 4362 | `			ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|      4 | 4363 | `			return PH7_VmThrowException(pCtx, "Error",` |
|      - | 4364 | `				"Typed property %z::$%z must not be accessed before initialization",` |
|      1 | 4365 | `				&pDecl->sName, &pAttr->sName);` |
|      - | 4366 | `		}` |
|      8 | 4367 | `		pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      8 | 4368 | `		if( pVal ){` |
|      8 | 4369 | `			ph7_result_value(pCtx, pVal);` |
|      5 | 4370 | `		}else{` |
|    ! 0 | 4371 | `			ph7_result_null(pCtx);` |
|      - | 4372 | `		}` |
|      - | 4373 | `	}` |
|      8 | 4374 | `	return PH7_OK;` |
|     12 | 4375 | `}` |
|      8 | 4376 | `static int vm_builtin_ReflectionClass_setStaticPropertyValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 4377 | `{` |
|     10 | 4378 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 4379 | `	ph7_class_attr *pAttr;` |
|      - | 4380 | `	ph7_value *pSlot;` |
|      - | 4381 | `	const char *zName;` |
|      - | 4382 | `	int nName;` |
|     10 | 4383 | `	if( pClass == 0 \|\| nArg < 2 ){` |
|    ! 0 | 4384 | `		return PH7_OK;` |
|      - | 4385 | `	}` |
|      - | 4386 | `	{` |
|     10 | 4387 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|     10 | 4388 | `		if( rc != SXRET_OK ){` |
|      5 | 4389 | `			return rc;` |
|      - | 4390 | `		}` |
|      - | 4391 | `	}` |
|      5 | 4392 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|      5 | 4393 | `	pAttr = ReflectStaticAttr(pCtx, pClass, zName, nName);` |
|      5 | 4394 | `	if( pAttr == 0 ){` |
|      4 | 4395 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 4396 | `			"Class %z does not have a property named %.*s", &pClass->sName, nName, zName);` |
|      - | 4397 | `	}` |
|      3 | 4398 | `	pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      3 | 4399 | `	if( pSlot == 0 ){` |
|    ! 0 | 4400 | `		return PH7_OK;` |
|      - | 4401 | `	}` |
|      - | 4402 | `	{` |
|      3 | 4403 | `		sxi32 rc = ReflectEnforceStore(pCtx, pAttr->nIdx, apArg[1]);` |
|      3 | 4404 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4405 | `			return rc;` |
|      - | 4406 | `		}` |
|      - | 4407 | `	}` |
|      3 | 4408 | `	PH7_MemObjStore(apArg[1], pSlot);` |
|      3 | 4409 | `	return PH7_OK;` |
|      6 | 4410 | `}` |
|      4 | 4411 | `static int vm_builtin_ReflectionClass_getDefaultProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 4412 | `{` |
|      5 | 4413 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      5 | 4414 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - | 4415 | `	SySet aMembers;` |
|      - | 4416 | `	sxu32 n;` |
|      - | 4417 | `	int iPass;` |
|      2 | 4418 | `	SXUNUSED(nArg);` |
|      2 | 4419 | `	SXUNUSED(apArg);` |
|      5 | 4420 | `	if( pOut == 0 ){` |
|    ! 0 | 4421 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4422 | `	}` |
|      5 | 4423 | `	if( pClass == 0 ){` |
|    ! 0 | 4424 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 | 4425 | `		return PH7_OK;` |
|      - | 4426 | `	}` |
|      5 | 4427 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|      5 | 4428 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|      - | 4429 | `	/* php reports the STATIC properties first, then the instance ones. */` |
|     13 | 4430 | `	for( iPass = 0 ; iPass < 2 ; iPass++ ){` |
|     57 | 4431 | `		for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     49 | 4432 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - | 4433 | `			int bStatic;` |
|      - | 4434 | `			ph7_value sValue;` |
|     49 | 4435 | `			if( pM->iKind != REFLECT_MEMBER_PROP ){` |
|     18 | 4436 | `				continue;` |
|      - | 4437 | `			}` |
|     45 | 4438 | `			bStatic = (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|     45 | 4439 | `			if( bStatic != (iPass == 0) ){` |
|     23 | 4440 | `				continue;` |
|      - | 4441 | `			}` |
|     22 | 4442 | `			if( (pM->pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|     16 | 4443 | `			 && SySetUsed(&pM->pAttr->aByteCode) < 1 ){` |
|      - | 4444 | `				/* A TYPED property with no initializer has no default at all —` |
|      - | 4445 | `				 * it is uninitialized, and php leaves it out. An UNTYPED one` |
|      - | 4446 | `				 * without an initializer defaults to null and is listed. */` |
|      5 | 4447 | `				continue;` |
|      - | 4448 | `			}` |
|     19 | 4449 | `			PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     19 | 4450 | `			if( SySetUsed(&pM->pAttr->aByteCode) > 0 ){` |
|      - | 4451 | `				/* Same evaluation path the VM uses for omitted call arguments */` |
|     15 | 4452 | `				VmLocalExec(pCtx->pVm, &pM->pAttr->aByteCode, &sValue, FALSE);` |
|      7 | 4453 | `			}` |
|     19 | 4454 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, &sValue);` |
|     19 | 4455 | `			PH7_MemObjRelease(&sValue);` |
|     10 | 4456 | `		}` |
|      5 | 4457 | `	}` |
|      5 | 4458 | `	SySetRelease(&aMembers);` |
|      5 | 4459 | `	ph7_result_value(pCtx, pOut);` |
|      5 | 4460 | `	return PH7_OK;` |
|      3 | 4461 | `}` |
|      - | 4462 | `/* ---- attributes, extension, export ---- */` |
|     64 | 4463 | `static int vm_builtin_ReflectionClass_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 4464 | `{` |
|     67 | 4465 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 4466 | `	const char *zName;` |
|      - | 4467 | `	int nName;` |
|     67 | 4468 | `	if( pClass == 0 ){` |
|    ! 0 | 4469 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 | 4470 | `		return PH7_OK;` |
|      - | 4471 | `	}` |
|     67 | 4472 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      - | 4473 | `	{` |
|      - | 4474 | `		ph7_value sTarget;` |
|      - | 4475 | `		int rc;` |
|     67 | 4476 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     67 | 4477 | `		ph7_value_string(&sTarget, zName, nName);` |
|      - | 4478 | `		/* 1 = Attribute::TARGET_CLASS */` |
|     99 | 4479 | `		rc = ReflectBuildAttrs(pCtx, &pClass->aAttrs, "class", &sTarget, 0, 0, 0, 1,` |
|     32 | 4480 | `			nArg, apArg);` |
|     67 | 4481 | `		PH7_MemObjRelease(&sTarget);` |
|     67 | 4482 | `		return rc;` |
|      - | 4483 | `	}` |
|     35 | 4484 | `}` |
|    ! 0 | 4485 | `static int vm_builtin_ReflectionClass_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 4486 | `{` |
|    ! 0 | 4487 | `	SXUNUSED(nArg);` |
|    ! 0 | 4488 | `	SXUNUSED(apArg);` |
|    ! 0 | 4489 | `	if( ReflectClassFlag(pCtx, PH7_CLASS_INTERNAL) ){` |
|    ! 0 | 4490 | `		ph7_result_string(pCtx, "Core", sizeof("Core")-1);` |
|    ! 0 | 4491 | `	}else{` |
|    ! 0 | 4492 | `		ph7_result_bool(pCtx, 0);` |
|      - | 4493 | `	}` |
|    ! 0 | 4494 | `	return PH7_OK;` |
|    ! 0 | 4495 | `}` |
|      - | 4496 | `/*` |
|      - | 4497 | ` * The synthetic "Core" extension, which is the only one PHL has. Answered by` |
|      - | 4498 | ` * every reflector's getExtension() for an INTERNAL target.` |
|      - | 4499 | ` */` |
|      6 | 4500 | `static int ReflectCoreExtension(ph7_context *pCtx)` |
|      1 | 4501 | `{` |
|      - | 4502 | `	ph7_value sName;` |
|      - | 4503 | `	ph7_value *apCtor[1];` |
|      - | 4504 | `	ph7_class_instance *pExt;` |
|      - | 4505 | `	sxi32 rc;` |
|      7 | 4506 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|      7 | 4507 | `	ph7_value_string(&sName, "Core", sizeof("Core")-1);` |
|      7 | 4508 | `	apCtor[0] = &sName;` |
|      7 | 4509 | `	pExt = ReflectConstruct(pCtx, "ReflectionExtension", 1, apCtor, &rc);` |
|      7 | 4510 | `	PH7_MemObjRelease(&sName);` |
|      7 | 4511 | `	if( pExt == 0 ){` |
|    ! 0 | 4512 | `		if( rc != PH7_OK ){` |
|    ! 0 | 4513 | `			return rc;` |
|      - | 4514 | `		}` |
|    ! 0 | 4515 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4516 | `		return PH7_OK;` |
|      - | 4517 | `	}` |
|      7 | 4518 | `	return ReflectResultObject(pCtx, pExt);` |
|      4 | 4519 | `}` |
|      - | 4520 | `/* php's export format (chunk 9) — defined at the END of this file, where the` |
|      - | 4521 | ` * member walk, the function reference and the parameter description it reads` |
|      - | 4522 | ` * are all in scope. */` |
|      - | 4523 | `static int ReflectExportClassSelf(ph7_context *pCtx);` |
|      - | 4524 | `static int ReflectExportFuncSelf(ph7_context *pCtx);` |
|      - | 4525 | `static int ReflectExportParamSelf(ph7_context *pCtx);` |
|      - | 4526 | `static int ReflectExportPropSelf(ph7_context *pCtx);` |
|      - | 4527 | `static int ReflectExportConstSelf(ph7_context *pCtx);` |
|      - | 4528 | `/*` |
|      - | 4529 | ` * __toString(): php's export format, still chunk 9 — a PHP function written` |
|      - | 4530 | `` * against the PUBLIC reflection API of its target, so it is called with `$this`.`` |
|      - | 4531 | ` * bIndentArg adds the export family's second "" indent argument.` |
|      - | 4532 | ` */` |
|      4 | 4533 | `static int vm_builtin_ReflectionClass_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 4534 | `{` |
|      2 | 4535 | `	SXUNUSED(nArg);` |
|      2 | 4536 | `	SXUNUSED(apArg);` |
|      5 | 4537 | `	if( !ReflectClassFlag(pCtx, PH7_CLASS_INTERNAL) ){` |
|      3 | 4538 | `		ph7_result_null(pCtx);` |
|      3 | 4539 | `		return PH7_OK;` |
|      - | 4540 | `	}` |
|      3 | 4541 | `	return ReflectCoreExtension(pCtx);` |
|      3 | 4542 | `}` |
|     28 | 4543 | `static int vm_builtin_ReflectionClass_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 4544 | `{` |
|     14 | 4545 | `	SXUNUSED(nArg);` |
|     14 | 4546 | `	SXUNUSED(apArg);` |
|     29 | 4547 | `	return ReflectExportClassSelf(pCtx);` |
|      1 | 4548 | `}` |
|      - | 4549 | `/* ---- lazy objects: PHL has none (§7.4) ---- */` |
|    ! 0 | 4550 | `static int ReflectNoLazy(ph7_context *pCtx, const char *zWho)` |
|    ! 0 | 4551 | `{` |
|    ! 0 | 4552 | `	return PH7_VmThrowException(pCtx, "Error",` |
|    ! 0 | 4553 | `		"%s is not supported by PHL (no lazy objects)", zWho);` |
|    ! 0 | 4554 | `}` |
|      - | 4555 | `#define REFLECT_NO_LAZY(NAME,TEXT) \` |
|      - | 4556 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 4557 | `	{ \` |
|      - | 4558 | `		SXUNUSED(nArg); \` |
|      - | 4559 | `		SXUNUSED(apArg); \` |
|      - | 4560 | `		return ReflectNoLazy(pCtx, TEXT); \` |
|      - | 4561 | `	}` |
|    ! 0 | 4562 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_newLazyGhost,"ReflectionClass::newLazyGhost()")` |
|    ! 0 | 4563 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_newLazyProxy,"ReflectionClass::newLazyProxy()")` |
|    ! 0 | 4564 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_resetAsLazyGhost,"ReflectionClass::resetAsLazyGhost()")` |
|    ! 0 | 4565 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_resetAsLazyProxy,"ReflectionClass::resetAsLazyProxy()")` |
|      - | 4566 | `/* The four QUERIES answer what is true of a VM with no lazy objects, rather` |
|      - | 4567 | ` * than refusing: an object here is always initialized and never has one. */` |
|    ! 0 | 4568 | `static int vm_builtin_ReflectionClass_getLazyInitializer(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 4569 | `{` |
|    ! 0 | 4570 | `	SXUNUSED(nArg);` |
|    ! 0 | 4571 | `	SXUNUSED(apArg);` |
|    ! 0 | 4572 | `	ph7_result_null(pCtx);` |
|    ! 0 | 4573 | `	return PH7_OK;` |
|    ! 0 | 4574 | `}` |
|    ! 0 | 4575 | `static int vm_builtin_ReflectionClass_passThroughObject(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 4576 | `{` |
|    ! 0 | 4577 | `	if( nArg > 0 ){` |
|    ! 0 | 4578 | `		ph7_result_value(pCtx, apArg[0]);` |
|    ! 0 | 4579 | `	}else{` |
|    ! 0 | 4580 | `		ph7_result_null(pCtx);` |
|      - | 4581 | `	}` |
|    ! 0 | 4582 | `	return PH7_OK;` |
|    ! 0 | 4583 | `}` |
|    ! 0 | 4584 | `static int vm_builtin_ReflectionClass_isUninitializedLazyObject(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 4585 | `{` |
|    ! 0 | 4586 | `	SXUNUSED(nArg);` |
|    ! 0 | 4587 | `	SXUNUSED(apArg);` |
|    ! 0 | 4588 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 | 4589 | `	return PH7_OK;` |
|    ! 0 | 4590 | `}` |
|      - | 4591 | `/*` |
|      - | 4592 | ` * Reflection::getModifierNames(int $modifiers)` |
|      - | 4593 | ` *` |
|      - | 4594 | ` * php's own order and its own SWITCH: the visibility names come out of one` |
|      - | 4595 | ` * three-way choice and the set-visibility names out of another, so a mask with` |
|      - | 4596 | ` * two visibility bits set names NEITHER (which is what php answers).` |
|      - | 4597 | ` */` |
|     96 | 4598 | `static int vm_builtin_Reflection_getModifierNames(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 4599 | `{` |
|     98 | 4600 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     98 | 4601 | `	sxi64 iMods = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|      - | 4602 | `	const char *azName[8];` |
|     98 | 4603 | `	int nName = 0, n;` |
|     98 | 4604 | `	if( pOut == 0 ){` |
|    ! 0 | 4605 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4606 | `	}` |
|     98 | 4607 | `	if( iMods & 64 ){ azName[nName++] = "abstract"; }` |
|     98 | 4608 | `	if( iMods & 32 ){ azName[nName++] = "final"; }` |
|     98 | 4609 | `	if( iMods & 512 ){ azName[nName++] = "virtual"; }` |
|     98 | 4610 | `	switch( iMods & (1\|2\|4) ){` |
|     56 | 4611 | `	case 1: azName[nName++] = "public"; break;` |
|     21 | 4612 | `	case 2: azName[nName++] = "protected"; break;` |
|     15 | 4613 | `	case 4: azName[nName++] = "private"; break;` |
|      8 | 4614 | `	default: break;` |
|      - | 4615 | `	}` |
|     98 | 4616 | `	switch( iMods & (2048\|4096) ){` |
|     18 | 4617 | `	case 2048: azName[nName++] = "protected(set)"; break;` |
|      9 | 4618 | `	case 4096: azName[nName++] = "private(set)"; break;` |
|     72 | 4619 | `	default: break;` |
|      - | 4620 | `	}` |
|     98 | 4621 | `	if( iMods & 16 ){ azName[nName++] = "static"; }` |
|     98 | 4622 | `	if( iMods & 128 ){ azName[nName++] = "readonly"; }` |
|    266 | 4623 | `	for( n = 0 ; n < nName ; n++ ){` |
|    170 | 4624 | `		ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|    170 | 4625 | `		if( pName == 0 ){ break; }` |
|    170 | 4626 | `		ph7_value_string(pName, azName[n], -1);` |
|    170 | 4627 | `		ph7_array_add_elem(pOut, 0, pName);` |
|     86 | 4628 | `	}` |
|     98 | 4629 | `	ph7_result_value(pCtx, pOut);` |
|     98 | 4630 | `	return PH7_OK;` |
|     50 | 4631 | `}` |
|      - | 4632 | `/*` |
|      - | 4633 | ` * Declare chunk 1. Called from PH7_VmInstallReflection() where it used to be` |
|      - | 4634 | `` * compiled — before chunks 2 and 3, which name `Reflector` in their own`` |
|      - | 4635 | `` * `implements` clauses.`` |
|      - | 4636 | ` *` |
|      - | 4637 | ` * The method table is in php's own DECLARATION order, which is the order` |
|      - | 4638 | ` * ReflectionClass::getMethods() reports for these classes themselves.` |
|      - | 4639 | ` */` |
|   5740 | 4640 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionClass(ph7_vm *pVm)` |
|      5 | 4641 | `{` |
|      - | 4642 | `	static const PH7_NativePropDef aClassProp[] = {` |
|      - | 4643 | `		{ "name",  PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 4644 | `		/* PHL-only: the instance a ReflectionObject was built over. php keeps it` |
|      - | 4645 | `		 * out of sight; PHL has no hidden-slot bit yet (§7.4 (e)). */` |
|      - | 4646 | `		{ RC_OBJ,  PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 4647 | `	};` |
|      - | 4648 | `	static const PH7_NativeMethodDef aClassMethod[] = {` |
|      - | 4649 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionClass_clone },` |
|      - | 4650 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $objectOrClass", "",` |
|      - | 4651 | `		  vm_builtin_ReflectionClass_construct },` |
|      - | 4652 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionClass_toString },` |
|      - | 4653 | `		{ "getName",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getName },` |
|      - | 4654 | `		{ "isInternal",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInternal },` |
|      - | 4655 | `		{ "isUserDefined", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isUserDefined },` |
|      - | 4656 | `		{ "isAnonymous",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isAnonymous },` |
|      - | 4657 | `		{ "isInstantiable",PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInstantiable },` |
|      - | 4658 | `		{ "isCloneable",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isCloneable },` |
|      - | 4659 | `		{ "getFileName",   PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getFileName },` |
|      - | 4660 | `		{ "getStartLine",  PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionClass_getStartLine },` |
|      - | 4661 | `		{ "getEndLine",    PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionClass_getEndLine },` |
|      - | 4662 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getDocComment },` |
|      - | 4663 | `		{ "getConstructor",PH7_MOD_PUBLIC, "", "@?ReflectionMethod", vm_builtin_ReflectionClass_getConstructor },` |
|      - | 4664 | `		{ "hasMethod",     PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasMethod },` |
|      - | 4665 | `		{ "getMethod",     PH7_MOD_PUBLIC, "string $name", "@ReflectionMethod", vm_builtin_ReflectionClass_getMethod },` |
|      - | 4666 | `		{ "getMethods",    PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - | 4667 | `		  vm_builtin_ReflectionClass_getMethods },` |
|      - | 4668 | `		{ "hasProperty",   PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasProperty },` |
|      - | 4669 | `		{ "getProperty",   PH7_MOD_PUBLIC, "string $name", "@ReflectionProperty", vm_builtin_ReflectionClass_getProperty },` |
|      - | 4670 | `		{ "getProperties", PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - | 4671 | `		  vm_builtin_ReflectionClass_getProperties },` |
|      - | 4672 | `		{ "hasConstant",   PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasConstant },` |
|      - | 4673 | `		{ "getConstants",  PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - | 4674 | `		  vm_builtin_ReflectionClass_getConstants },` |
|      - | 4675 | `		{ "getReflectionConstants", PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - | 4676 | `		  vm_builtin_ReflectionClass_getReflectionConstants },` |
|      - | 4677 | `		{ "getConstant",   PH7_MOD_PUBLIC, "string $name", "@mixed", vm_builtin_ReflectionClass_getConstant },` |
|      - | 4678 | `		{ "getReflectionConstant", PH7_MOD_PUBLIC, "string $name", "@ReflectionClassConstant\|false",` |
|      - | 4679 | `		  vm_builtin_ReflectionClass_getReflectionConstant },` |
|      - | 4680 | `		{ "getInterfaces",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getInterfaces },` |
|      - | 4681 | `		{ "getInterfaceNames", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getInterfaceNames },` |
|      - | 4682 | `		{ "isInterface",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInterface },` |
|      - | 4683 | `		{ "getTraits",         PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraits },` |
|      - | 4684 | `		{ "getTraitNames",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraitNames },` |
|      - | 4685 | `		{ "getTraitAliases",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraitAliases },` |
|      - | 4686 | `		{ "isTrait",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isTrait },` |
|      - | 4687 | `		{ "isEnum",            PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClass_isEnum },` |
|      - | 4688 | `		{ "isAbstract",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isAbstract },` |
|      - | 4689 | `		{ "isFinal",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isFinal },` |
|      - | 4690 | `		{ "isReadOnly",        PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClass_isReadOnly },` |
|      - | 4691 | `		{ "getModifiers",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionClass_getModifiers },` |
|      - | 4692 | `		{ "isInstance",        PH7_MOD_PUBLIC, "object $object", "@bool",` |
|      - | 4693 | `		  vm_builtin_ReflectionClass_isInstance },` |
|      - | 4694 | `		{ "newInstance",       PH7_MOD_PUBLIC, "mixed ...$args", "@object",` |
|      - | 4695 | `		  vm_builtin_ReflectionClass_newInstance },` |
|      - | 4696 | `		{ "newInstanceWithoutConstructor", PH7_MOD_PUBLIC, "", "@object",` |
|      - | 4697 | `		  vm_builtin_ReflectionClass_newInstanceWithoutConstructor },` |
|      - | 4698 | `		{ "newInstanceArgs",   PH7_MOD_PUBLIC, "array $args = []", "@?object",` |
|      - | 4699 | `		  vm_builtin_ReflectionClass_newInstanceArgs },` |
|      - | 4700 | `		{ "newLazyGhost",      PH7_MOD_PUBLIC, "callable $initializer, int $options = 0", "object",` |
|      - | 4701 | `		  vm_builtin_ReflectionClass_newLazyGhost },` |
|      - | 4702 | `		{ "newLazyProxy",      PH7_MOD_PUBLIC, "callable $factory, int $options = 0", "object",` |
|      - | 4703 | `		  vm_builtin_ReflectionClass_newLazyProxy },` |
|      - | 4704 | `		{ "resetAsLazyGhost",  PH7_MOD_PUBLIC,` |
|      - | 4705 | `		  "object $object, callable $initializer, int $options = 0", "void",` |
|      - | 4706 | `		  vm_builtin_ReflectionClass_resetAsLazyGhost },` |
|      - | 4707 | `		{ "resetAsLazyProxy",  PH7_MOD_PUBLIC,` |
|      - | 4708 | `		  "object $object, callable $factory, int $options = 0", "void",` |
|      - | 4709 | `		  vm_builtin_ReflectionClass_resetAsLazyProxy },` |
|      - | 4710 | `		{ "initializeLazyObject", PH7_MOD_PUBLIC, "object $object", "object",` |
|      - | 4711 | `		  vm_builtin_ReflectionClass_passThroughObject },` |
|      - | 4712 | `		{ "isUninitializedLazyObject", PH7_MOD_PUBLIC, "object $object", "bool",` |
|      - | 4713 | `		  vm_builtin_ReflectionClass_isUninitializedLazyObject },` |
|      - | 4714 | `		{ "markLazyObjectAsInitialized", PH7_MOD_PUBLIC, "object $object", "object",` |
|      - | 4715 | `		  vm_builtin_ReflectionClass_passThroughObject },` |
|      - | 4716 | `		{ "getLazyInitializer", PH7_MOD_PUBLIC, "object $object", "?callable",` |
|      - | 4717 | `		  vm_builtin_ReflectionClass_getLazyInitializer },` |
|      - | 4718 | `		{ "getParentClass",    PH7_MOD_PUBLIC, "", "@ReflectionClass\|false", vm_builtin_ReflectionClass_getParentClass },` |
|      - | 4719 | `		{ "isSubclassOf",      PH7_MOD_PUBLIC, "ReflectionClass\|string $class", "@bool",` |
|      - | 4720 | `		  vm_builtin_ReflectionClass_isSubclassOf },` |
|      - | 4721 | `		{ "getStaticProperties", PH7_MOD_PUBLIC, "", "@array",` |
|      - | 4722 | `		  vm_builtin_ReflectionClass_getStaticProperties },` |
|      - | 4723 | ``		/* `mixed $default = ?` is the table's "optional, no default VALUE"`` |
|      - | 4724 | `		 * marker — php's own shape here: isOptional() true,` |
|      - | 4725 | `		 * isDefaultValueAvailable() false. */` |
|      - | 4726 | `		{ "getStaticPropertyValue", PH7_MOD_PUBLIC, "string $name, mixed $default = ?", "@mixed",` |
|      - | 4727 | `		  vm_builtin_ReflectionClass_getStaticPropertyValue },` |
|      - | 4728 | `		{ "setStaticPropertyValue", PH7_MOD_PUBLIC, "string $name, mixed $value", "@void",` |
|      - | 4729 | `		  vm_builtin_ReflectionClass_setStaticPropertyValue },` |
|      - | 4730 | `		{ "getDefaultProperties", PH7_MOD_PUBLIC, "", "@array",` |
|      - | 4731 | `		  vm_builtin_ReflectionClass_getDefaultProperties },` |
|      - | 4732 | `		{ "isIterable",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isIterable },` |
|      - | 4733 | `		{ "isIterateable",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isIterable },` |
|      - | 4734 | `		{ "implementsInterface", PH7_MOD_PUBLIC, "ReflectionClass\|string $interface", "@bool",` |
|      - | 4735 | `		  vm_builtin_ReflectionClass_implementsInterface },` |
|      - | 4736 | `		{ "getExtension",      PH7_MOD_PUBLIC, "", "@?ReflectionExtension", vm_builtin_ReflectionClass_getExtension },` |
|      - | 4737 | `		{ "getExtensionName",  PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getExtensionName },` |
|      - | 4738 | `		{ "inNamespace",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_inNamespace },` |
|      - | 4739 | `		{ "getNamespaceName",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getNamespaceName },` |
|      - | 4740 | `		{ "getShortName",      PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getShortName },` |
|      - | 4741 | `		{ "getAttributes",     PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - | 4742 | `		  vm_builtin_ReflectionClass_getAttributes },` |
|      - | 4743 | `	};` |
|      - | 4744 | `	static const PH7_NativeConstDef aClassConst[] = {` |
|      - | 4745 | `		{ "IS_IMPLICIT_ABSTRACT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,    0, 0.0 },` |
|      - | 4746 | `		{ "IS_EXPLICIT_ABSTRACT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64,    0, 0.0 },` |
|      - | 4747 | `		{ "IS_FINAL",             PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32,    0, 0.0 },` |
|      - | 4748 | `		{ "IS_READONLY",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 65536, 0, 0.0 },` |
|      - | 4749 | `		{ "SKIP_INITIALIZATION_ON_SERIALIZE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 8, 0, 0.0 },` |
|      - | 4750 | `		{ "SKIP_DESTRUCTOR",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,    0, 0.0 },` |
|      - | 4751 | `	};` |
|      - | 4752 | `	static const PH7_NativeMethodDef aObjectMethod[] = {` |
|      - | 4753 | `		{ "__construct", PH7_MOD_PUBLIC, "object $object", "",` |
|      - | 4754 | `		  vm_builtin_ReflectionObject_construct },` |
|      - | 4755 | `	};` |
|      - | 4756 | `	static const PH7_NativeMethodDef aReflectionMethod[] = {` |
|      - | 4757 | `		{ "getModifierNames", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "int $modifiers", "@array",` |
|      - | 4758 | `		  vm_builtin_Reflection_getModifierNames },` |
|      - | 4759 | `	};` |
|      - | 4760 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 4761 | `		{ "Reflector", "Stringable", 0, PH7_CLASS_INTERFACE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 4762 | `		{ "ReflectionException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 4763 | `		{ "Reflection", 0, 0, 0,` |
|      - | 4764 | `		  aReflectionMethod, SX_ARRAYSIZE(aReflectionMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 4765 | ``		/* Uncloneable and unserializable in php too: `clone` is an Error and`` |
|      - | 4766 | `		 * serialize() a catchable Exception naming the class. */` |
|      - | 4767 | `		{ "ReflectionClass", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 4768 | `		  aClassMethod, SX_ARRAYSIZE(aClassMethod),` |
|      - | 4769 | `		  aClassConst, SX_ARRAYSIZE(aClassConst),` |
|      - | 4770 | `		  aClassProp, SX_ARRAYSIZE(aClassProp), 0, 0, 0 },` |
|      - | 4771 | `		{ "ReflectionObject", "ReflectionClass", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 4772 | `		  aObjectMethod, SX_ARRAYSIZE(aObjectMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 4773 | `	};` |
|   5745 | 4774 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 | 4775 | `}` |
|      - | 4776 | `/*` |
|      - | 4777 | ` * ---------------------------------------------------------------------------` |
|      - | 4778 | ` * ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,` |
|      - | 4779 | ` * ReflectionParameter.` |
|      - | 4780 | ` *` |
|      - | 4781 | ` * Chunk 2. Its accessors funnelled through __reflect_func_info(), a descriptor` |
|      - | 4782 | ` * ARRAY built per call from either a compiled ph7_vm_func or a declared` |
|      - | 4783 | ` * SIGNATURE STRING; a native method reads whichever of the two the target` |
|      - | 4784 | ` * actually has, through the one uniform description both already agreed on` |
|      - | 4785 | ` * (ReflectParamDesc / ReflectSigDescribe).` |
|      - | 4786 | ` *` |
|      - | 4787 | ` * Five thunks retire with the chunk: __reflect_func_info, __reflect_invoke,` |
|      - | 4788 | ` * __reflect_closure, __reflect_param_default and __reflect_param_defconst.` |
|      - | 4789 | ` * ---------------------------------------------------------------------------` |
|      - | 4790 | ` */` |
|      - | 4791 | `#define RF_CL "__cl"     /* ReflectionFunctionAbstract: the reflected Closure  */` |
|      - | 4792 | `#define RP_T  "__t"      /* ReflectionParameter: the function/class it belongs to */` |
|      - | 4793 | `#define RP_M  "__m"      /* ReflectionParameter: the method name, or null      */` |
|      - | 4794 | `#define RP_P  "__p"      /* ReflectionParameter: the position                  */` |
|      - | 4795 |  |
|      - | 4796 | `/* Everything a reflected function IS, resolved once per call. */` |
|      - | 4797 | `typedef struct ReflectFuncRef ReflectFuncRef;` |
|      - | 4798 | `struct ReflectFuncRef` |
|      - | 4799 | `{` |
|      - | 4800 | `	ph7_vm_func *pFunc;           /* compiled body (NULL for a pure C builtin) */` |
|      - | 4801 | `	ph7_user_func *pHost;         /* C builtin (NULL otherwise) */` |
|      - | 4802 | `	ph7_class *pClass;            /* class a METHOD was reached through */` |
|      - | 4803 | `	ph7_class_method *pMeth;      /* the method */` |
|      - | 4804 | `	ph7_class_instance *pClosure; /* the Closure being reflected, if any */` |
|      - | 4805 | `	const char *zSig;             /* declared parameter signature, or NULL */` |
|      - | 4806 | `	const char *zRet;             /* declared return type, or NULL */` |
|      - | 4807 | `};` |
|      - | 4808 | `/*` |
|      - | 4809 | ` * Resolve a callable into the reference, and work out WHICH of the two` |
|      - | 4810 | ` * parameter sources describes it: a declared signature string (a C builtin, a` |
|      - | 4811 | ` * native method, or an embedded-PHP builtin declared argless over` |
|      - | 4812 | ` * func_get_args()) wins over the compiled argument list, exactly as` |
|      - | 4813 | ` * ReflectSigFixup made it win in the descriptor.` |
|      - | 4814 | ` */` |
|   4700 | 4815 | `static int ReflectFuncFill(ph7_vm *pVm, ph7_value *pTarget, ph7_value *pMethodArg,` |
|      - | 4816 | `	ReflectFuncRef *pOut)` |
|      5 | 4817 | `{` |
|   4705 | 4818 | `	SyZero(pOut, sizeof(*pOut));` |
|   7055 | 4819 | `	pOut->pFunc = ReflectResolveCallable(pVm, pTarget, pMethodArg,` |
|   2350 | 4820 | `		&pOut->pClass, &pOut->pMeth, &pOut->pHost, &pOut->pClosure);` |
|   4705 | 4821 | `	if( pOut->pFunc == 0 && pOut->pHost == 0 ){` |
|     17 | 4822 | `		return 0;` |
|      - | 4823 | `	}` |
|   4691 | 4824 | `	if( pOut->pFunc == 0 ){` |
|   1473 | 4825 | `		pOut->zSig = pOut->pHost->zSig;` |
|   1473 | 4826 | `		pOut->zRet = pOut->pHost->zRet;` |
|   1473 | 4827 | `		return 1;` |
|      - | 4828 | `	}` |
|   3221 | 4829 | `	if( (pOut->pFunc->iFlags & VM_FUNC_NATIVE) && pOut->pFunc->pNative ){` |
|   2008 | 4830 | `		pOut->zSig = pOut->pFunc->pNative->zSig;` |
|   2008 | 4831 | `		if( SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|   2008 | 4832 | `			pOut->zRet = pOut->pFunc->pNative->zRet;` |
|   1002 | 4833 | `		}` |
|   2218 | 4834 | `	}else if( (pOut->pFunc->iFlags & VM_FUNC_INTERNAL)` |
|    682 | 4835 | `	 && SySetUsed(&pOut->pFunc->aArgs) == 0 && pOut->pMeth == 0 ){` |
|    ! 0 | 4836 | `		const char *zRet = 0;` |
|    ! 0 | 4837 | `		pOut->zSig = PH7_VmBuiltinSigLookup(SyStringData(&pOut->pFunc->sName),` |
|    ! 0 | 4838 | `			SyStringLength(&pOut->pFunc->sName), &zRet);` |
|    ! 0 | 4839 | `		if( zRet && SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|    ! 0 | 4840 | `			pOut->zRet = zRet;` |
|    ! 0 | 4841 | `		}` |
|    ! 0 | 4842 | `	}` |
|   3221 | 4843 | `	if( pOut->zSig && pOut->zSig[0] == '\0' ){` |
|      - | 4844 | `		/* A declared-EMPTY signature really is "no parameters", not "undescribed". */` |
|    543 | 4845 | `		return 1;` |
|      - | 4846 | `	}` |
|   2681 | 4847 | `	return 1;` |
|   2355 | 4848 | `}` |
|      - | 4849 | `/* Is this receiver a ReflectionMethod (rather than a ReflectionFunction)? */` |
|   2340 | 4850 | `static int ReflectIsMethodReflector(ph7_context *pCtx, ph7_class_instance *pThis)` |
|      5 | 4851 | `{` |
|      - | 4852 | `	ph7_class *pRM;` |
|   2345 | 4853 | `	if( pThis == 0 ){` |
|    ! 0 | 4854 | `		return 0;` |
|      - | 4855 | `	}` |
|   2345 | 4856 | `	pRM = PH7_VmExtractClass(pCtx->pVm, "ReflectionMethod", sizeof("ReflectionMethod")-1, FALSE, 0);` |
|   2345 | 4857 | `	return pRM != 0 && PH7_VmInstanceOf(pThis->pClass, pRM);` |
|   1175 | 4858 | `}` |
|      - | 4859 | `/*` |
|      - | 4860 | `` * The function `$this` reflects. A ReflectionFunction built over a Closure keeps`` |
|      - | 4861 | `` * the object in `__cl` and resolves through it (its `$__fn` is a lambda name no`` |
|      - | 4862 | ` * function table holds); a ReflectionMethod resolves ($this->class, $this->name).` |
|      - | 4863 | ` */` |
|   1932 | 4864 | `static int ReflectFuncOfThis(ph7_context *pCtx, ReflectFuncRef *pOut)` |
|      5 | 4865 | `{` |
|   1937 | 4866 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 4867 | `	ph7_class_instance *pClo;` |
|      - | 4868 | `	ph7_value sTarget, sMethod;` |
|      - | 4869 | `	const char *zName, *zClass;` |
|      - | 4870 | `	int nName, nClass, rc;` |
|   1937 | 4871 | `	SyZero(pOut, sizeof(*pOut));` |
|   1937 | 4872 | `	if( pThis == 0 ){` |
|    ! 0 | 4873 | `		return 0;` |
|      - | 4874 | `	}` |
|   1937 | 4875 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|   1937 | 4876 | `	PH7_MemObjInit(pCtx->pVm, &sMethod);` |
|   1937 | 4877 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|   1937 | 4878 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|   1937 | 4879 | `	if( pClo ){` |
|     89 | 4880 | `		sTarget.x.pOther = pClo;` |
|     89 | 4881 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|   1894 | 4882 | `	}else if( ReflectIsMethodReflector(pCtx, pThis) ){` |
|   1285 | 4883 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|   1285 | 4884 | `		ph7_value_string(&sTarget, zClass, nClass);` |
|   1285 | 4885 | `		ph7_value_string(&sMethod, zName, nName);` |
|    645 | 4886 | `	}else{` |
|    569 | 4887 | `		ph7_value_string(&sTarget, zName, nName);` |
|      - | 4888 | `	}` |
|   1937 | 4889 | `	rc = ReflectFuncFill(pCtx->pVm, &sTarget, (sMethod.iFlags & MEMOBJ_STRING) ? &sMethod : 0, pOut);` |
|      - | 4890 | `	/* sTarget only ever BORROWS the closure; releasing it would unref twice. */` |
|   1937 | 4891 | `	if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|   1851 | 4892 | `		PH7_MemObjRelease(&sTarget);` |
|    923 | 4893 | `	}` |
|   1937 | 4894 | `	PH7_MemObjRelease(&sMethod);` |
|   1937 | 4895 | `	return rc;` |
|    971 | 4896 | `}` |
|      - | 4897 | `/* How many parameters the target declares. */` |
|   1578 | 4898 | `static int ReflectParamCount(const ReflectFuncRef *pRef)` |
|      5 | 4899 | `{` |
|   1583 | 4900 | `	if( pRef->zSig ){` |
|   1091 | 4901 | `		return ReflectSigPart(pRef->zSig, (int)SyStrlen(pRef->zSig), -1, 0, 0);` |
|      - | 4902 | `	}` |
|    494 | 4903 | `	return pRef->pFunc ? (int)SySetUsed(&pRef->pFunc->aArgs) : 0;` |
|    794 | 4904 | `}` |
|      - | 4905 | `/* Describe the iPos-th parameter. Answers 0 when there is none. */` |
|   2594 | 4906 | `static int ReflectParamAt(const ReflectFuncRef *pRef, int iPos, ReflectParamDesc *pOut)` |
|      5 | 4907 | `{` |
|   2599 | 4908 | `	SyZero(pOut, sizeof(*pOut));` |
|   2599 | 4909 | `	if( iPos < 0 ){` |
|    ! 0 | 4910 | `		return 0;` |
|      - | 4911 | `	}` |
|   2599 | 4912 | `	if( pRef->zSig ){` |
|   1900 | 4913 | `		const char *zPart = 0;` |
|   1900 | 4914 | `		int nPart = 0, nTotal;` |
|   1900 | 4915 | `		nTotal = ReflectSigPart(pRef->zSig, (int)SyStrlen(pRef->zSig), iPos, &zPart, &nPart);` |
|   1900 | 4916 | `		if( iPos >= nTotal \|\| zPart == 0 ){` |
|      3 | 4917 | `			return 0;` |
|      - | 4918 | `		}` |
|   1898 | 4919 | `		ReflectSigDescribe(zPart, nPart, iPos, pOut);` |
|   1898 | 4920 | `		return 1;` |
|      - | 4921 | `	}` |
|    702 | 4922 | `	if( pRef->pFunc == 0 ){` |
|    ! 0 | 4923 | `		return 0;` |
|      - | 4924 | `	}` |
|      - | 4925 | `	{` |
|    702 | 4926 | `		ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pRef->pFunc->aArgs, (sxu32)iPos);` |
|    702 | 4927 | `		if( pArg == 0 ){` |
|     13 | 4928 | `			return 0;` |
|      - | 4929 | `		}` |
|    690 | 4930 | `		pOut->iPos = iPos;` |
|    690 | 4931 | `		pOut->sName = pArg->sName;` |
|    690 | 4932 | `		pOut->bByRef = (pArg->iFlags & VM_FUNC_ARG_BY_REF) != 0;` |
|    690 | 4933 | `		pOut->bVariadic = (pArg->iFlags & VM_FUNC_ARG_VARIADIC) != 0;` |
|      - | 4934 | `		/* The compiler never sets ARG_HAS_DEF; a default IS compiled byte-code` |
|      - | 4935 | `		 * (the same test the OP_CALL default-value path uses). */` |
|    690 | 4936 | `		pOut->bHasDef = SySetUsed(&pArg->aByteCode) > 0;` |
|    690 | 4937 | `		pOut->bNullable = (pArg->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|    690 | 4938 | `		pOut->bPromoted = (pArg->iFlags & VM_FUNC_ARG_PROMOTED) != 0;` |
|    690 | 4939 | `		pOut->bOptional = pOut->bVariadic \|\| pOut->bHasDef;` |
|    690 | 4940 | `		pOut->sType = pArg->sTypeName;` |
|    690 | 4941 | `		pOut->pArg = pArg;` |
|    690 | 4942 | `		return 1;` |
|      - | 4943 | `	}` |
|   1302 | 4944 | `}` |
|      - | 4945 | `/* The declaring class php reports for a reflected METHOD. */` |
|    424 | 4946 | `static ph7_class * ReflectFuncDeclClass(const ReflectFuncRef *pRef)` |
|      3 | 4947 | `{` |
|    427 | 4948 | `	if( pRef->pMeth == 0 \|\| pRef->pClass == 0 ){` |
|      3 | 4949 | `		return 0;` |
|      - | 4950 | `	}` |
|    425 | 4951 | `	return ReflectMethodDeclClass(pRef->pClass, pRef->pMeth);` |
|    215 | 4952 | `}` |
|      - | 4953 | `/*` |
|      - | 4954 | ` * The declared return type TEXT, or 0 — and whether php calls it TENTATIVE.` |
|      - | 4955 | ` *` |
|      - | 4956 | ` * php's stubs carry two kinds of internal return type and Reflection answers` |
|      - | 4957 | ` * differently for each: a real one is reported by getReturnType()/hasReturnType()` |
|      - | 4958 | `` * and printed `Return [ T ]`, while an `@tentative-return-type` answers NULL and`` |
|      - | 4959 | ` * false from those two, is reported by getTentativeReturnType()/` |
|      - | 4960 | `` * hasTentativeReturnType(), and prints `Tentative return [ T ]`. Nearly every`` |
|      - | 4961 | ` * internal SPL, date and Reflection method is the tentative kind.` |
|      - | 4962 | ` *` |
|      - | 4963 | `` * PHL has one field for both, so a leading `@` on a `zRet` — the same character`` |
|      - | 4964 | ` * php's stub annotation uses — marks the tentative kind. It is stripped here, so` |
|      - | 4965 | ` * nothing downstream of this function ever sees it.` |
|      - | 4966 | ` */` |
|    698 | 4967 | `static int ReflectFuncRetTextEx(const ReflectFuncRef *pRef, const char **pz, int *pn,` |
|      - | 4968 | `	int *pbTentative)` |
|      4 | 4969 | `{` |
|    702 | 4970 | `	if( pbTentative ){` |
|    702 | 4971 | `		*pbTentative = 0;` |
|    349 | 4972 | `	}` |
|    702 | 4973 | `	if( pRef->zRet && pRef->zRet[0] ){` |
|    578 | 4974 | `		const char *z = pRef->zRet;` |
|    578 | 4975 | `		if( z[0] == '@' ){` |
|    267 | 4976 | `			if( pbTentative ){` |
|    267 | 4977 | `				*pbTentative = 1;` |
|    133 | 4978 | `			}` |
|    267 | 4979 | `			z++;` |
|    133 | 4980 | `		}` |
|    578 | 4981 | `		*pz = z;` |
|    578 | 4982 | `		*pn = (int)SyStrlen(z);` |
|    578 | 4983 | `		return 1;` |
|      - | 4984 | `	}` |
|    126 | 4985 | `	if( pRef->pFunc == 0 ){` |
|    ! 0 | 4986 | `		return 0;` |
|      - | 4987 | `	}` |
|    126 | 4988 | `	if( SyStringLength(&pRef->pFunc->sReturnTypeName) > 0 ){` |
|     54 | 4989 | `		*pz = SyStringData(&pRef->pFunc->sReturnTypeName);` |
|     54 | 4990 | `		*pn = (int)SyStringLength(&pRef->pFunc->sReturnTypeName);` |
|     54 | 4991 | `		return 1;` |
|      - | 4992 | `	}` |
|      - | 4993 | `	/* The type-text renderer omits void/never atoms (compile.c notes the root fix` |
|      - | 4994 | `	 * belongs there); name them here for getReturnType(). */` |
|     73 | 4995 | `	if( pRef->pFunc->nReturnType == MEMOBJ_VOID ){` |
|     11 | 4996 | `		*pz = "void";` |
|     11 | 4997 | `		*pn = sizeof("void")-1;` |
|     11 | 4998 | `		return 1;` |
|      - | 4999 | `	}` |
|     63 | 5000 | `	if( pRef->pFunc->nReturnType == MEMOBJ_NEVER ){` |
|      3 | 5001 | `		*pz = "never";` |
|      3 | 5002 | `		*pn = sizeof("never")-1;` |
|      3 | 5003 | `		return 1;` |
|      - | 5004 | `	}` |
|     61 | 5005 | `	return 0;` |
|    353 | 5006 | `}` |
|      - | 5007 | `/* The declared return type php would report from getReturnType(): a TENTATIVE one` |
|      - | 5008 | ` * is not reported there at all, which is the whole distinction. */` |
|    392 | 5009 | `static int ReflectFuncRetText(const ReflectFuncRef *pRef, const char **pz, int *pn)` |
|      4 | 5010 | `{` |
|    396 | 5011 | `	int bTentative = 0;` |
|    396 | 5012 | `	if( !ReflectFuncRetTextEx(pRef, pz, pn, &bTentative) ){` |
|     21 | 5013 | `		return 0;` |
|      - | 5014 | `	}` |
|    376 | 5015 | `	return bTentative ? 0 : 1;` |
|    200 | 5016 | `}` |
|      - | 5017 | `/* Is the reflected function internal (a C builtin or an embedded-chunk one)? */` |
|    200 | 5018 | `static int ReflectFuncIsInternal(const ReflectFuncRef *pRef)` |
|      2 | 5019 | `{` |
|    202 | 5020 | `	if( pRef->pHost ){` |
|     38 | 5021 | `		return 1;` |
|      - | 5022 | `	}` |
|    165 | 5023 | `	return pRef->pFunc != 0 && (pRef->pFunc->iFlags & VM_FUNC_INTERNAL) != 0;` |
|    102 | 5024 | `}` |
|      - | 5025 | `/* Does this target carry a #[\Deprecated] attribute? */` |
|      8 | 5026 | `static int ReflectHasDeprecated(SySet *pAttrs)` |
|      1 | 5027 | `{` |
|      9 | 5028 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|      - | 5029 | `	sxu32 n;` |
|      9 | 5030 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|      6 | 5031 | `		if( SyStringLength(&aA[n].sName) == sizeof("deprecated")-1` |
|      7 | 5032 | `		 && SyStrnicmp(SyStringData(&aA[n].sName), "deprecated", sizeof("deprecated")-1) == 0 ){` |
|      7 | 5033 | `			return 1;` |
|      - | 5034 | `		}` |
|    ! 0 | 5035 | `	}` |
|      3 | 5036 | `	return 0;` |
|      5 | 5037 | `}` |
|      - | 5038 | ``/* Resolve `$this`, or answer a default when the target has gone missing. */`` |
|      - | 5039 | `#define REFLECT_FUNC_OR(REF,STMT) \` |
|      - | 5040 | `	if( !ReflectFuncOfThis(pCtx, &(REF)) ){ STMT; return PH7_OK; }` |
|      - | 5041 |  |
|      - | 5042 | `/* ---- ReflectionFunctionAbstract ---- */` |
|    ! 0 | 5043 | `static int vm_builtin_ReflectionFunc_clone(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 5044 | `{` |
|    ! 0 | 5045 | `	SXUNUSED(pCtx);` |
|    ! 0 | 5046 | `	SXUNUSED(nArg);` |
|    ! 0 | 5047 | `	SXUNUSED(apArg);` |
|    ! 0 | 5048 | `	return PH7_OK;` |
|    ! 0 | 5049 | `}` |
|    252 | 5050 | `static int vm_builtin_ReflectionFunc_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 | 5051 | `{` |
|    257 | 5052 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    257 | 5053 | `	const char *zName = "";` |
|    257 | 5054 | `	int nName = 0;` |
|    126 | 5055 | `	SXUNUSED(nArg);` |
|    126 | 5056 | `	SXUNUSED(apArg);` |
|    257 | 5057 | `	if( pThis ){` |
|    257 | 5058 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    126 | 5059 | `	}` |
|    257 | 5060 | `	ph7_result_string(pCtx, zName, nName);` |
|    257 | 5061 | `	return PH7_OK;` |
|      5 | 5062 | `}` |
|      - | 5063 | ``/* The `name` slot's namespace split — the same three answers ReflectionClass gives. */`` |
|    ! 0 | 5064 | `static int ReflectFuncNamePart(ph7_context *pCtx, int iWhat)` |
|    ! 0 | 5065 | `{` |
|    ! 0 | 5066 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    ! 0 | 5067 | `	const char *zName = "";` |
|    ! 0 | 5068 | `	int nName = 0, iCut;` |
|    ! 0 | 5069 | `	if( pThis ){` |
|    ! 0 | 5070 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    ! 0 | 5071 | `	}` |
|    ! 0 | 5072 | `	iCut = ReflectNsCut(zName, nName);` |
|    ! 0 | 5073 | `	if( iWhat == 0 ){` |
|    ! 0 | 5074 | `		ph7_result_bool(pCtx, iCut >= 0);` |
|    ! 0 | 5075 | `	}else if( iWhat == 1 ){` |
|    ! 0 | 5076 | `		ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|    ! 0 | 5077 | `	}else if( iCut < 0 ){` |
|    ! 0 | 5078 | `		ph7_result_string(pCtx, zName, nName);` |
|    ! 0 | 5079 | `	}else{` |
|    ! 0 | 5080 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|      - | 5081 | `	}` |
|    ! 0 | 5082 | `	return PH7_OK;` |
|    ! 0 | 5083 | `}` |
|      - | 5084 | `#define REFLECT_FUNC_NAMEPART(NAME,WHAT) \` |
|      - | 5085 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 5086 | `	{ \` |
|      - | 5087 | `		SXUNUSED(nArg); \` |
|      - | 5088 | `		SXUNUSED(apArg); \` |
|      - | 5089 | `		return ReflectFuncNamePart(pCtx, WHAT); \` |
|      - | 5090 | `	}` |
|    ! 0 | 5091 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_inNamespace, 0)` |
|    ! 0 | 5092 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_getNamespaceName, 1)` |
|    ! 0 | 5093 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_getShortName, 2)` |
|      - | 5094 |  |
|     14 | 5095 | `static int vm_builtin_ReflectionFunc_isClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5096 | `{` |
|      - | 5097 | `	ReflectFuncRef sRef;` |
|     15 | 5098 | `	int bAnon = 0;` |
|      7 | 5099 | `	SXUNUSED(nArg);` |
|      7 | 5100 | `	SXUNUSED(apArg);` |
|     15 | 5101 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     15 | 5102 | `	if( sRef.pFunc ){` |
|      - | 5103 | ``		/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|      - | 5104 | `		 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|     15 | 5105 | `		bAnon = (sRef.pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|     14 | 5106 | `		if( !bAnon && SyStringLength(&sRef.pFunc->sName) > 9` |
|      3 | 5107 | `		 && (SyMemcmp(SyStringData(&sRef.pFunc->sName), "[lambda_", 8) == 0` |
|    ! 0 | 5108 | `		  \|\| SyMemcmp(SyStringData(&sRef.pFunc->sName), "[closure_", 9) == 0) ){` |
|    ! 0 | 5109 | `			bAnon = 1;` |
|    ! 0 | 5110 | `		}` |
|      7 | 5111 | `	}` |
|     15 | 5112 | `	ph7_result_bool(pCtx, bAnon);` |
|     15 | 5113 | `	return PH7_OK;` |
|      8 | 5114 | `}` |
|      8 | 5115 | `static int vm_builtin_ReflectionFunc_isDeprecated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5116 | `{` |
|      - | 5117 | `	ReflectFuncRef sRef;` |
|      4 | 5118 | `	SXUNUSED(nArg);` |
|      4 | 5119 | `	SXUNUSED(apArg);` |
|      9 | 5120 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|      9 | 5121 | `	ph7_result_bool(pCtx, sRef.pFunc != 0 && ReflectHasDeprecated(&sRef.pFunc->aAttrs));` |
|      9 | 5122 | `	return PH7_OK;` |
|      5 | 5123 | `}` |
|     44 | 5124 | `static int vm_builtin_ReflectionFunc_isInternal(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5125 | `{` |
|      - | 5126 | `	ReflectFuncRef sRef;` |
|     22 | 5127 | `	SXUNUSED(nArg);` |
|     22 | 5128 | `	SXUNUSED(apArg);` |
|     45 | 5129 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     45 | 5130 | `	ph7_result_bool(pCtx, ReflectFuncIsInternal(&sRef));` |
|     45 | 5131 | `	return PH7_OK;` |
|     23 | 5132 | `}` |
|    ! 0 | 5133 | `static int vm_builtin_ReflectionFunc_isUserDefined(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 5134 | `{` |
|      - | 5135 | `	ReflectFuncRef sRef;` |
|    ! 0 | 5136 | `	SXUNUSED(nArg);` |
|    ! 0 | 5137 | `	SXUNUSED(apArg);` |
|    ! 0 | 5138 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    ! 0 | 5139 | `	ph7_result_bool(pCtx, !ReflectFuncIsInternal(&sRef));` |
|    ! 0 | 5140 | `	return PH7_OK;` |
|    ! 0 | 5141 | `}` |
|      4 | 5142 | `static int vm_builtin_ReflectionFunc_isGenerator(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5143 | `{` |
|      - | 5144 | `	ReflectFuncRef sRef;` |
|      2 | 5145 | `	SXUNUSED(nArg);` |
|      2 | 5146 | `	SXUNUSED(apArg);` |
|      5 | 5147 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|      5 | 5148 | `	ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|      5 | 5149 | `	return PH7_OK;` |
|      3 | 5150 | `}` |
|     10 | 5151 | `static int vm_builtin_ReflectionFunc_isVariadic(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5152 | `{` |
|      - | 5153 | `	ReflectFuncRef sRef;` |
|      - | 5154 | `	ReflectParamDesc sDesc;` |
|     11 | 5155 | `	int n, nTotal, bVariadic = 0;` |
|      5 | 5156 | `	SXUNUSED(nArg);` |
|      5 | 5157 | `	SXUNUSED(apArg);` |
|     11 | 5158 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     11 | 5159 | `	nTotal = ReflectParamCount(&sRef);` |
|     31 | 5160 | `	for( n = 0 ; n < nTotal ; n++ ){` |
|     29 | 5161 | `		if( ReflectParamAt(&sRef, n, &sDesc) && sDesc.bVariadic ){` |
|      9 | 5162 | `			bVariadic = 1;` |
|      9 | 5163 | `			break;` |
|      - | 5164 | `		}` |
|     11 | 5165 | `	}` |
|     11 | 5166 | `	ph7_result_bool(pCtx, bVariadic);` |
|     11 | 5167 | `	return PH7_OK;` |
|      6 | 5168 | `}` |
|      - | 5169 | ``/* isStatic(): a METHOD's own staticness, a closure's `static function () {}`. */`` |
|     82 | 5170 | `static int vm_builtin_ReflectionFunc_isStatic(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 5171 | `{` |
|      - | 5172 | `	ReflectFuncRef sRef;` |
|     41 | 5173 | `	SXUNUSED(nArg);` |
|     41 | 5174 | `	SXUNUSED(apArg);` |
|     84 | 5175 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     84 | 5176 | `	if( sRef.pMeth ){` |
|     74 | 5177 | `		ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|     38 | 5178 | `	}else{` |
|     11 | 5179 | `		ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);` |
|      - | 5180 | `	}` |
|     84 | 5181 | `	return PH7_OK;` |
|     43 | 5182 | `}` |
|      6 | 5183 | `static int vm_builtin_ReflectionFunc_returnsReference(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5184 | `{` |
|      - | 5185 | `	ReflectFuncRef sRef;` |
|      3 | 5186 | `	SXUNUSED(nArg);` |
|      3 | 5187 | `	SXUNUSED(apArg);` |
|      7 | 5188 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|      7 | 5189 | `	ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|      7 | 5190 | `	return PH7_OK;` |
|      4 | 5191 | `}` |
|      - | 5192 | `/* The Closure instance whose captured state the three getClosure* read. */` |
|     36 | 5193 | `static ph7_value * ReflectClosureAttr(ReflectFuncRef *pRef, const char *zName)` |
|      1 | 5194 | `{` |
|      - | 5195 | `	SyString sAttr;` |
|     37 | 5196 | `	if( pRef->pClosure == 0 ){` |
|    ! 0 | 5197 | `		return 0;` |
|      - | 5198 | `	}` |
|     37 | 5199 | `	SyStringInitFromBuf(&sAttr, zName, SyStrlen(zName));` |
|     37 | 5200 | `	return PH7_ClassInstanceFetchAttr(pRef->pClosure, &sAttr);` |
|     19 | 5201 | `}` |
|      4 | 5202 | `static int vm_builtin_ReflectionFunc_getClosureThis(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5203 | `{` |
|      - | 5204 | `	ReflectFuncRef sRef;` |
|      - | 5205 | `	ph7_value *pAttr;` |
|      2 | 5206 | `	SXUNUSED(nArg);` |
|      2 | 5207 | `	SXUNUSED(apArg);` |
|      5 | 5208 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|      5 | 5209 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|      5 | 5210 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|      3 | 5211 | `		ph7_result_value(pCtx, pAttr);` |
|      2 | 5212 | `	}else{` |
|      3 | 5213 | `		ph7_result_null(pCtx);` |
|      - | 5214 | `	}` |
|      5 | 5215 | `	return PH7_OK;` |
|      3 | 5216 | `}` |
|     18 | 5217 | `static int vm_builtin_ReflectionFunc_getClosureScopeClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5218 | `{` |
|      - | 5219 | `	ReflectFuncRef sRef;` |
|      - | 5220 | `	ph7_value *pAttr;` |
|      9 | 5221 | `	SXUNUSED(nArg);` |
|      9 | 5222 | `	SXUNUSED(apArg);` |
|     19 | 5223 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|     19 | 5224 | `	pAttr = ReflectClosureAttr(&sRef, "__scope");` |
|     19 | 5225 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|      - | 5226 | `		/* php reports the class that DECLARED the callee, not the one the callable NAMED:` |
|      - | 5227 | ``		 * `$__scope` is the CALLED scope (what `static::` answers, reported by`` |
|      - | 5228 | `		 * getClosureCalledClass below), and for an inherited or trait-composed method the` |
|      - | 5229 | `		 * two differ. PH7_VmClosureScopeClass is the one rule. */` |
|     22 | 5230 | `		return ReflectResultClassOf(pCtx,` |
|      7 | 5231 | `			PH7_VmClosureScopeClass(pCtx->pVm, sRef.pClosure));` |
|      - | 5232 | `	}` |
|      5 | 5233 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|      5 | 5234 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|    ! 0 | 5235 | `		return ReflectResultClassOf(pCtx, ((ph7_class_instance *)pAttr->x.pOther)->pClass);` |
|      - | 5236 | `	}` |
|      5 | 5237 | `	ph7_result_null(pCtx);` |
|      5 | 5238 | `	return PH7_OK;` |
|     10 | 5239 | `}` |
|      - | 5240 | `/*` |
|      - | 5241 | ` * ReflectionFunction::getClosureCalledClass() — php's CALLED scope, the other half of the` |
|      - | 5242 | `` * pair above: the class the call goes THROUGH, which is what `static::` answers. A bound`` |
|      - | 5243 | `` * closure's is its `$this`'s class; a static callable's is the class the callable NAMED. It`` |
|      - | 5244 | `` * shared getClosureScopeClass()'s body while `$__scope` answered both questions; it cannot`` |
|      - | 5245 | ` * now, because that one narrows a method closure to its DECLARING class.` |
|      - | 5246 | ` */` |
|      8 | 5247 | `static int vm_builtin_ReflectionFunc_getClosureCalledClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5248 | `{` |
|      - | 5249 | `	ReflectFuncRef sRef;` |
|      - | 5250 | `	ph7_value *pAttr;` |
|      4 | 5251 | `	SXUNUSED(nArg);` |
|      4 | 5252 | `	SXUNUSED(apArg);` |
|      9 | 5253 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|      9 | 5254 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|      9 | 5255 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|      7 | 5256 | `		return ReflectResultClassOf(pCtx, ((ph7_class_instance *)pAttr->x.pOther)->pClass);` |
|      - | 5257 | `	}` |
|      3 | 5258 | `	pAttr = ReflectClosureAttr(&sRef, "__scope");` |
|      3 | 5259 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|      4 | 5260 | `		return ReflectResultClassOf(pCtx, PH7_VmExtractClass(pCtx->pVm,` |
|      2 | 5261 | `			(const char *)SyBlobData(&pAttr->sBlob), SyBlobLength(&pAttr->sBlob), FALSE, 0));` |
|      - | 5262 | `	}` |
|    ! 0 | 5263 | `	ph7_result_null(pCtx);` |
|    ! 0 | 5264 | `	return PH7_OK;` |
|      5 | 5265 | `}` |
|     10 | 5266 | `static int vm_builtin_ReflectionFunc_getClosureUsedVariables(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 5267 | `{` |
|      - | 5268 | `	ReflectFuncRef sRef;` |
|     12 | 5269 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - | 5270 | `	ph7_vm_func_closure_env *aEnv;` |
|      - | 5271 | `	sxu32 n;` |
|      5 | 5272 | `	SXUNUSED(nArg);` |
|      5 | 5273 | `	SXUNUSED(apArg);` |
|     12 | 5274 | `	if( pOut == 0 ){` |
|    ! 0 | 5275 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5276 | `	}` |
|     12 | 5277 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pClosure == 0 \|\| sRef.pFunc == 0 ){` |
|    ! 0 | 5278 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 | 5279 | `		return PH7_OK;` |
|      - | 5280 | `	}` |
|      - | 5281 | `	/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */` |
|     12 | 5282 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|     28 | 5283 | `	for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; n++ ){` |
|     18 | 5284 | `		if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){` |
|     12 | 5285 | `			continue;` |
|      - | 5286 | `		}` |
|      6 | 5287 | `		if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|      4 | 5288 | `		 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|    ! 0 | 5289 | `			continue;` |
|      - | 5290 | `		}` |
|      7 | 5291 | `		if( (aEnv[n].iFlags & VM_FUNC_ARG_BY_REF) && aEnv[n].nIdx != SXU32_HIGH ){` |
|      - | 5292 | `			/* Captured by reference: report the slot's live value */` |
|      3 | 5293 | `			ph7_value *pLive = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aEnv[n].nIdx);` |
|      3 | 5294 | `			ReflectMapAddDyn(pCtx, pOut, &aEnv[n].sName, pLive ? pLive : &aEnv[n].sValue);` |
|      3 | 5295 | `			continue;` |
|      - | 5296 | `		}` |
|      5 | 5297 | `		ReflectMapAddDyn(pCtx, pOut, &aEnv[n].sName, &aEnv[n].sValue);` |
|      3 | 5298 | `	}` |
|     12 | 5299 | `	ph7_result_value(pCtx, pOut);` |
|     12 | 5300 | `	return PH7_OK;` |
|      7 | 5301 | `}` |
|     14 | 5302 | `static int vm_builtin_ReflectionFunc_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5303 | `{` |
|      - | 5304 | `	ReflectFuncRef sRef;` |
|      7 | 5305 | `	SXUNUSED(nArg);` |
|      7 | 5306 | `	SXUNUSED(apArg);` |
|     15 | 5307 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     15 | 5308 | `	if( sRef.pFunc && SyStringLength(&sRef.pFunc->sDoc) > 0 ){` |
|     11 | 5309 | `		ph7_result_string(pCtx, SyStringData(&sRef.pFunc->sDoc), (int)SyStringLength(&sRef.pFunc->sDoc));` |
|      6 | 5310 | `	}else{` |
|      5 | 5311 | `		ph7_result_bool(pCtx, 0);` |
|      - | 5312 | `	}` |
|     15 | 5313 | `	return PH7_OK;` |
|      8 | 5314 | `}` |
|     22 | 5315 | `static int vm_builtin_ReflectionFunc_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5316 | `{` |
|      - | 5317 | `	ReflectFuncRef sRef;` |
|     11 | 5318 | `	SXUNUSED(nArg);` |
|     11 | 5319 | `	SXUNUSED(apArg);` |
|     23 | 5320 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     23 | 5321 | `	if( sRef.pFunc && SyStringLength(&sRef.pFunc->sFile) > 0 ){` |
|      3 | 5322 | `		ph7_result_string(pCtx, SyStringData(&sRef.pFunc->sFile), (int)SyStringLength(&sRef.pFunc->sFile));` |
|      2 | 5323 | `	}else{` |
|     21 | 5324 | `		ph7_result_bool(pCtx, 0);` |
|      - | 5325 | `	}` |
|     23 | 5326 | `	return PH7_OK;` |
|     12 | 5327 | `}` |
|     22 | 5328 | `static int ReflectFuncLine(ph7_context *pCtx, int bEnd)` |
|      1 | 5329 | `{` |
|      - | 5330 | `	ReflectFuncRef sRef;` |
|     23 | 5331 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| ReflectFuncIsInternal(&sRef) \|\| sRef.pFunc == 0 ){` |
|     17 | 5332 | `		ph7_result_bool(pCtx, 0);` |
|     17 | 5333 | `		return PH7_OK;` |
|      - | 5334 | `	}` |
|      7 | 5335 | `	ph7_result_int64(pCtx, (sxi64)(bEnd ? sRef.pFunc->nEndLine : sRef.pFunc->nLine));` |
|      7 | 5336 | `	return PH7_OK;` |
|     12 | 5337 | `}` |
|     20 | 5338 | `static int vm_builtin_ReflectionFunc_getStartLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5339 | `{` |
|     10 | 5340 | `	SXUNUSED(nArg);` |
|     10 | 5341 | `	SXUNUSED(apArg);` |
|     21 | 5342 | `	return ReflectFuncLine(pCtx, 0);` |
|      1 | 5343 | `}` |
|      2 | 5344 | `static int vm_builtin_ReflectionFunc_getEndLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5345 | `{` |
|      1 | 5346 | `	SXUNUSED(nArg);` |
|      1 | 5347 | `	SXUNUSED(apArg);` |
|      3 | 5348 | `	return ReflectFuncLine(pCtx, 1);` |
|      1 | 5349 | `}` |
|    108 | 5350 | `static int vm_builtin_ReflectionFunc_hasReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 5351 | `{` |
|      - | 5352 | `	ReflectFuncRef sRef;` |
|      - | 5353 | `	const char *z;` |
|      - | 5354 | `	int n;` |
|     54 | 5355 | `	SXUNUSED(nArg);` |
|     54 | 5356 | `	SXUNUSED(apArg);` |
|    110 | 5357 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    110 | 5358 | `	ph7_result_bool(pCtx, ReflectFuncRetText(&sRef, &z, &n));` |
|    110 | 5359 | `	return PH7_OK;` |
|     56 | 5360 | `}` |
|    284 | 5361 | `static int vm_builtin_ReflectionFunc_getReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 | 5362 | `{` |
|      - | 5363 | `	ReflectFuncRef sRef;` |
|      - | 5364 | `	const char *z;` |
|      - | 5365 | `	int n;` |
|    142 | 5366 | `	SXUNUSED(nArg);` |
|    142 | 5367 | `	SXUNUSED(apArg);` |
|    288 | 5368 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|    288 | 5369 | `	if( !ReflectFuncRetText(&sRef, &z, &n) ){` |
|     21 | 5370 | `		ph7_result_null(pCtx);` |
|     21 | 5371 | `		return PH7_OK;` |
|      - | 5372 | `	}` |
|    268 | 5373 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx, z, n));` |
|    146 | 5374 | `}` |
|      - | 5375 | ``/* A native method's `@`-marked zRet is php's `@tentative-return-type`: reported`` |
|      - | 5376 | ` * HERE and by nothing else, which is what separates it from a real one. */` |
|     80 | 5377 | `static int vm_builtin_ReflectionFunc_hasTentativeReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5378 | `{` |
|      - | 5379 | `	ReflectFuncRef sRef;` |
|      - | 5380 | `	const char *z;` |
|     81 | 5381 | `	int n, bTentative = 0;` |
|     40 | 5382 | `	SXUNUSED(nArg);` |
|     40 | 5383 | `	SXUNUSED(apArg);` |
|     81 | 5384 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     81 | 5385 | `	ph7_result_bool(pCtx, ReflectFuncRetTextEx(&sRef, &z, &n, &bTentative) && bTentative);` |
|     81 | 5386 | `	return PH7_OK;` |
|     41 | 5387 | `}` |
|     98 | 5388 | `static int vm_builtin_ReflectionFunc_getTentativeReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5389 | `{` |
|      - | 5390 | `	ReflectFuncRef sRef;` |
|      - | 5391 | `	const char *z;` |
|     99 | 5392 | `	int n, bTentative = 0;` |
|     49 | 5393 | `	SXUNUSED(nArg);` |
|     49 | 5394 | `	SXUNUSED(apArg);` |
|     99 | 5395 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|     99 | 5396 | `	if( !ReflectFuncRetTextEx(&sRef, &z, &n, &bTentative) \|\| !bTentative ){` |
|     13 | 5397 | `		ph7_result_null(pCtx);` |
|     13 | 5398 | `		return PH7_OK;` |
|      - | 5399 | `	}` |
|     87 | 5400 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx, z, n));` |
|     50 | 5401 | `}` |
|     54 | 5402 | `static int vm_builtin_ReflectionFunc_getNumberOfParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 5403 | `{` |
|      - | 5404 | `	ReflectFuncRef sRef;` |
|     27 | 5405 | `	SXUNUSED(nArg);` |
|     27 | 5406 | `	SXUNUSED(apArg);` |
|     57 | 5407 | `	REFLECT_FUNC_OR(sRef, ph7_result_int(pCtx, 0))` |
|     57 | 5408 | `	if( sRef.pHost && sRef.zSig == 0 ){` |
|      - | 5409 | `		/* An UNDESCRIBED builtin: the arity table is all there is. */` |
|    ! 0 | 5410 | `		ph7_result_int64(pCtx, (sxi64)sRef.pHost->nMinArg);` |
|    ! 0 | 5411 | `		return PH7_OK;` |
|      - | 5412 | `	}` |
|     57 | 5413 | `	ph7_result_int(pCtx, ReflectParamCount(&sRef));` |
|     57 | 5414 | `	return PH7_OK;` |
|     30 | 5415 | `}` |
|     24 | 5416 | `static int vm_builtin_ReflectionFunc_getNumberOfRequiredParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5417 | `{` |
|      - | 5418 | `	ReflectFuncRef sRef;` |
|      - | 5419 | `	ReflectParamDesc sDesc;` |
|     25 | 5420 | `	int n, nTotal, nReq = 0;` |
|     12 | 5421 | `	SXUNUSED(nArg);` |
|     12 | 5422 | `	SXUNUSED(apArg);` |
|     25 | 5423 | `	REFLECT_FUNC_OR(sRef, ph7_result_int(pCtx, 0))` |
|     25 | 5424 | `	if( sRef.pHost && sRef.zSig == 0 ){` |
|    ! 0 | 5425 | `		ph7_result_int64(pCtx, (sxi64)sRef.pHost->nMinArg);` |
|    ! 0 | 5426 | `		return PH7_OK;` |
|      - | 5427 | `	}` |
|     25 | 5428 | `	nTotal = ReflectParamCount(&sRef);` |
|     55 | 5429 | `	for( n = nTotal ; n > 0 ; n-- ){` |
|     53 | 5430 | `		if( ReflectParamAt(&sRef, n - 1, &sDesc) && !sDesc.bVariadic && !sDesc.bOptional ){` |
|     23 | 5431 | `			nReq = n;` |
|     23 | 5432 | `			break;` |
|      - | 5433 | `		}` |
|     16 | 5434 | `	}` |
|     25 | 5435 | `	ph7_result_int(pCtx, nReq);` |
|     25 | 5436 | `	return PH7_OK;` |
|     13 | 5437 | `}` |
|      - | 5438 | `/* The (target, method) pair a ReflectionParameter is built over. */` |
|    504 | 5439 | `static void ReflectFuncParamSpec(ph7_context *pCtx, ph7_value *pSpec)` |
|      4 | 5440 | `{` |
|    508 | 5441 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 5442 | `	ph7_class_instance *pClo;` |
|      - | 5443 | `	const char *zName, *zClass;` |
|      - | 5444 | `	int nName, nClass;` |
|    508 | 5445 | `	PH7_MemObjInit(pCtx->pVm, pSpec);` |
|    508 | 5446 | `	if( pThis == 0 ){` |
|    ! 0 | 5447 | `		return;` |
|      - | 5448 | `	}` |
|    508 | 5449 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|    508 | 5450 | `	if( pClo ){` |
|     11 | 5451 | `		pSpec->x.pOther = pClo;` |
|     11 | 5452 | `		pSpec->iFlags = MEMOBJ_OBJ;` |
|     11 | 5453 | `		return;` |
|      - | 5454 | `	}` |
|    498 | 5455 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    498 | 5456 | `	if( ReflectIsMethodReflector(pCtx, pThis) ){` |
|      - | 5457 | ``		/* `[class, method]`, which ReflectionParameter's constructor accepts */`` |
|    213 | 5458 | `		ph7_value *pList = ph7_context_new_array(pCtx);` |
|    213 | 5459 | `		ph7_value *pA = ph7_context_new_scalar(pCtx);` |
|    213 | 5460 | `		ph7_value *pB = ph7_context_new_scalar(pCtx);` |
|    213 | 5461 | `		if( pList == 0 \|\| pA == 0 \|\| pB == 0 ){` |
|    ! 0 | 5462 | `			return;` |
|      - | 5463 | `		}` |
|    213 | 5464 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|    213 | 5465 | `		ph7_value_string(pA, zClass, nClass);` |
|    213 | 5466 | `		ph7_value_string(pB, zName, nName);` |
|    213 | 5467 | `		ph7_array_add_elem(pList, 0, pA);` |
|    213 | 5468 | `		ph7_array_add_elem(pList, 0, pB);` |
|    213 | 5469 | `		PH7_MemObjStore(pList, pSpec);` |
|    213 | 5470 | `		return;` |
|      - | 5471 | `	}` |
|    286 | 5472 | `	ph7_value_string(pSpec, zName, nName);` |
|    256 | 5473 | `}` |
|    490 | 5474 | `static int vm_builtin_ReflectionFunc_getParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 | 5475 | `{` |
|      - | 5476 | `	ReflectFuncRef sRef;` |
|    494 | 5477 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - | 5478 | `	ph7_value sSpec;` |
|      - | 5479 | `	int n, nTotal;` |
|    245 | 5480 | `	SXUNUSED(nArg);` |
|    245 | 5481 | `	SXUNUSED(apArg);` |
|    494 | 5482 | `	if( pOut == 0 ){` |
|    ! 0 | 5483 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5484 | `	}` |
|    494 | 5485 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 | 5486 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 | 5487 | `		return PH7_OK;` |
|      - | 5488 | `	}` |
|    494 | 5489 | `	nTotal = ReflectParamCount(&sRef);` |
|    494 | 5490 | `	ReflectFuncParamSpec(pCtx, &sSpec);` |
|   1294 | 5491 | `	for( n = 0 ; n < nTotal ; n++ ){` |
|      - | 5492 | `		ph7_value sPos, sVal;` |
|      - | 5493 | `		ph7_value *apCtor[2];` |
|      - | 5494 | `		ph7_class_instance *pParam;` |
|      - | 5495 | `		sxi32 rc;` |
|    804 | 5496 | `		PH7_MemObjInit(pCtx->pVm, &sPos);` |
|    804 | 5497 | `		ph7_value_int(&sPos, n);` |
|    804 | 5498 | `		apCtor[0] = &sSpec;` |
|    804 | 5499 | `		apCtor[1] = &sPos;` |
|    804 | 5500 | `		pParam = ReflectConstruct(pCtx, "ReflectionParameter", 2, apCtor, &rc);` |
|    804 | 5501 | `		PH7_MemObjRelease(&sPos);` |
|    804 | 5502 | `		if( pParam == 0 ){` |
|    ! 0 | 5503 | `			break;` |
|      - | 5504 | `		}` |
|    804 | 5505 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    804 | 5506 | `		sVal.x.pOther = pParam;` |
|    804 | 5507 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|    804 | 5508 | `		ph7_array_add_elem(pOut, 0, &sVal);   /* takes its own reference */` |
|    804 | 5509 | `		PH7_ClassInstanceUnref(pParam);` |
|    404 | 5510 | `	}` |
|    494 | 5511 | `	if( (sSpec.iFlags & MEMOBJ_OBJ) == 0 ){` |
|    488 | 5512 | `		PH7_MemObjRelease(&sSpec);` |
|    242 | 5513 | `	}` |
|    494 | 5514 | `	ph7_result_value(pCtx, pOut);` |
|    494 | 5515 | `	return PH7_OK;` |
|    249 | 5516 | `}` |
|      4 | 5517 | `static int vm_builtin_ReflectionFunc_getStaticVariables(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5518 | `{` |
|      - | 5519 | `	ReflectFuncRef sRef;` |
|      5 | 5520 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - | 5521 | `	ph7_vm_func_static_var *aStatic;` |
|      - | 5522 | `	sxu32 n;` |
|      2 | 5523 | `	SXUNUSED(nArg);` |
|      2 | 5524 | `	SXUNUSED(apArg);` |
|      5 | 5525 | `	if( pOut == 0 ){` |
|    ! 0 | 5526 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5527 | `	}` |
|      5 | 5528 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pFunc == 0 ){` |
|    ! 0 | 5529 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 | 5530 | `		return PH7_OK;` |
|      - | 5531 | `	}` |
|      - | 5532 | `	/* Current value when the slot was initialized (first call), otherwise the` |
|      - | 5533 | `	 * evaluated default — php's getStaticVariables initializes on demand and` |
|      - | 5534 | `	 * reports the same values. */` |
|      5 | 5535 | `	aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&sRef.pFunc->aStatic);` |
|      9 | 5536 | `	for( n = 0 ; n < SySetUsed(&sRef.pFunc->aStatic) ; n++ ){` |
|      5 | 5537 | `		ph7_value *pVal = 0;` |
|      - | 5538 | `		ph7_value sScratch;` |
|      5 | 5539 | `		int bScratch = 0;` |
|      5 | 5540 | `		if( aStatic[n].nIdx != SXU32_HIGH ){` |
|      3 | 5541 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);` |
|      1 | 5542 | `		}` |
|      5 | 5543 | `		if( pVal == 0 ){` |
|      3 | 5544 | `			PH7_MemObjInit(pCtx->pVm, &sScratch);` |
|      3 | 5545 | `			if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      3 | 5546 | `				VmLocalExec(pCtx->pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|      1 | 5547 | `			}` |
|      3 | 5548 | `			pVal = &sScratch;` |
|      3 | 5549 | `			bScratch = 1;` |
|      1 | 5550 | `		}` |
|      5 | 5551 | `		ReflectMapAddDyn(pCtx, pOut, &aStatic[n].sName, pVal);` |
|      5 | 5552 | `		if( bScratch ){` |
|      3 | 5553 | `			PH7_MemObjRelease(&sScratch);` |
|      1 | 5554 | `		}` |
|      3 | 5555 | `	}` |
|      5 | 5556 | `	ph7_result_value(pCtx, pOut);` |
|      5 | 5557 | `	return PH7_OK;` |
|      3 | 5558 | `}` |
|      - | 5559 | ``/* One `key => value` entry of a presented shape, by name. */`` |
|     46 | 5560 | `static void ClosurePresentAdd(ph7_vm *pVm, ph7_value *pOut, const char *zKey, ph7_value *pVal)` |
|      1 | 5561 | `{` |
|      - | 5562 | `	ph7_value sKey;` |
|     47 | 5563 | `	PH7_MemObjInitFromString(pVm, &sKey, 0);` |
|     47 | 5564 | `	PH7_MemObjStringAppend(&sKey, zKey, (sxu32)SyStrlen(zKey));` |
|     47 | 5565 | `	ph7_array_add_elem(pOut, &sKey, pVal);   /* takes its OWN reference */` |
|     47 | 5566 | `	PH7_MemObjRelease(&sKey);` |
|     47 | 5567 | `}` |
|      - | 5568 | `/*` |
|      - | 5569 | ` * php's DEBUG presentation for a Closure (ph7_class::xPresent) —` |
|      - | 5570 | `` * `zend_closure_get_debug_info` (Zend/zend_closures.c), key for key and in php's`` |
|      - | 5571 | `` * order. Every key is CONDITIONAL, which is why a plain `function(){}` shows three`` |
|      - | 5572 | ` * and a bound one with captures shows five:` |
|      - | 5573 | ` *` |
|      - | 5574 | ` *   - a FAKE closure (php's ZEND_ACC_FAKE_CLOSURE — a first-class callable or` |
|      - | 5575 | `` *     `Closure::fromCallable` over a NAMED function) shows one `function` key,`` |
|      - | 5576 | `` *     `Class::method` when it carries a scope and the bare name otherwise, and NO`` |
|      - | 5577 | `` *     name/file/line. A real closure shows `name` (php's `{closure:SCOPE:LINE}`,`` |
|      - | 5578 | `` *     which `PH7_VmFuncDisplayName` answers), `file` and an INT `line`;`` |
|      - | 5579 | `` *   - `static` is the closure's own variable state — the `use` captures AND the`` |
|      - | 5580 | `` *     body's `static $x` — keyed WITHOUT the `$`, and omitted when empty. php`` |
|      - | 5581 | ` *     shows it before the first call too, since the initializers are compiled;` |
|      - | 5582 | `` *   - `this` is the bound receiver, present only when there is one (`bindTo`,`` |
|      - | 5583 | ` *     an instance first-class callable, or the implicit capture in a method);` |
|      - | 5584 | `` *   - `parameter` is `"$name" => "<required>"\|"<optional>"`, `&$name` for a by-ref`` |
|      - | 5585 | ` *     parameter, omitted when the callee takes none. php's cut is` |
|      - | 5586 | `` *     `required_num_args`, which counts through the LAST required parameter — so`` |
|      - | 5587 | `` *     `function($a, $b = 1, $c)` reports all THREE as `<required>`, not two.`` |
|      - | 5588 | ` *` |
|      - | 5589 | `` * The non-debug half answers nothing: php's `get_properties` for a Closure is an`` |
|      - | 5590 | `` * empty table, and `(array)$closure` never reaches it at all (php special-cases a`` |
|      - | 5591 | `` * Closure in `convert_to_array` and wraps it as a SCALAR, `[0 => $closure]`; PHL`` |
|      - | 5592 | `` * answers `[]` there — PLAN §4).`` |
|      - | 5593 | ` */` |
|     20 | 5594 | `PH7_PRIVATE sxi32 PH7_ClosurePresent(ph7_vm *pVm, ph7_class_instance *pThis,` |
|      - | 5595 | `	ph7_value *pOut, int bDebug)` |
|      2 | 5596 | `{` |
|      - | 5597 | `	ReflectFuncRef sRef;` |
|      - | 5598 | `	ph7_value sCarrier, sVal;` |
|     22 | 5599 | `	int bFake = 1;` |
|     22 | 5600 | `	if( !bDebug \|\| pThis == 0 ){` |
|      8 | 5601 | `		return SXRET_OK;` |
|      - | 5602 | `	}` |
|      - | 5603 | `	/* Rule 16's other half: this carrier never took a reference, so it must be` |
|      - | 5604 | `	 * blanked before it is released or the Closure is unref'd a second time. */` |
|     15 | 5605 | `	PH7_MemObjInit(pVm, &sCarrier);` |
|     15 | 5606 | `	sCarrier.x.pOther = pThis;` |
|     15 | 5607 | `	MemObjSetType(&sCarrier, MEMOBJ_OBJ);` |
|     15 | 5608 | `	if( !ReflectFuncFill(pVm, &sCarrier, 0, &sRef) ){` |
|    ! 0 | 5609 | `		sCarrier.x.pOther = 0;` |
|    ! 0 | 5610 | `		sCarrier.iFlags = MEMOBJ_NULL;` |
|    ! 0 | 5611 | `		PH7_MemObjRelease(&sCarrier);` |
|    ! 0 | 5612 | `		return SXRET_OK;` |
|      - | 5613 | `	}` |
|     15 | 5614 | `	sCarrier.x.pOther = 0;` |
|     15 | 5615 | `	sCarrier.iFlags = MEMOBJ_NULL;` |
|     15 | 5616 | `	PH7_MemObjRelease(&sCarrier);` |
|      - | 5617 | `	/* php's FAKE-closure bit, read off what the callable actually resolved TO: a` |
|      - | 5618 | `	 * real closure's body is the anonymous function itself (the compiler's` |
|      - | 5619 | ``	 * `{closure:...}` name, or the synthesized key that stands in for it). */`` |
|     15 | 5620 | `	if( sRef.pFunc && sRef.pMeth == 0 ){` |
|      9 | 5621 | `		const SyString *pN = &sRef.pFunc->sName;` |
|      8 | 5622 | `		if( SyStringLength(&sRef.pFunc->sClosureName) > 0` |
|      4 | 5623 | `		 \|\| (pN->nByte > 8 && SyMemcmp(pN->zString, "[lambda_", 8) == 0)` |
|      1 | 5624 | `		 \|\| (pN->nByte > 9 && SyMemcmp(pN->zString, "[closure_", 9) == 0) ){` |
|      9 | 5625 | `			bFake = 0;` |
|      4 | 5626 | `		}` |
|      4 | 5627 | `	}` |
|     15 | 5628 | `	PH7_MemObjInit(pVm, &sVal);` |
|     15 | 5629 | `	if( bFake ){` |
|      7 | 5630 | `		ph7_class *pScope = ReflectFuncDeclClass(&sRef);` |
|      7 | 5631 | `		const SyString *pName = sRef.pFunc ? &sRef.pFunc->sName : &sRef.pHost->sName;` |
|      7 | 5632 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|      7 | 5633 | `		if( pScope == 0 && sRef.pClass ){` |
|    ! 0 | 5634 | `			pScope = sRef.pClass;` |
|    ! 0 | 5635 | `		}` |
|      7 | 5636 | `		if( pScope ){` |
|      7 | 5637 | `			PH7_MemObjStringAppend(&sVal, SyStringData(&pScope->sName),` |
|      2 | 5638 | `				SyStringLength(&pScope->sName));` |
|      5 | 5639 | `			PH7_MemObjStringAppend(&sVal, "::", 2);` |
|      2 | 5640 | `		}` |
|      7 | 5641 | `		PH7_MemObjStringAppend(&sVal, SyStringData(pName), SyStringLength(pName));` |
|      7 | 5642 | `		ClosurePresentAdd(pVm, pOut, "function", &sVal);` |
|      7 | 5643 | `		PH7_MemObjRelease(&sVal);` |
|      4 | 5644 | `	}else{` |
|      - | 5645 | `		const char *zShow;` |
|      9 | 5646 | `		int nShow = PH7_VmFuncDisplayName(pVm, sRef.pFunc, &zShow);` |
|      9 | 5647 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|      9 | 5648 | `		PH7_MemObjStringAppend(&sVal, zShow, (sxu32)(nShow > 0 ? nShow : 0));` |
|      9 | 5649 | `		ClosurePresentAdd(pVm, pOut, "name", &sVal);` |
|      9 | 5650 | `		PH7_MemObjRelease(&sVal);` |
|      9 | 5651 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|     13 | 5652 | `		PH7_MemObjStringAppend(&sVal, SyStringData(&sRef.pFunc->sFile),` |
|      8 | 5653 | `			SyStringLength(&sRef.pFunc->sFile));` |
|      9 | 5654 | `		ClosurePresentAdd(pVm, pOut, "file", &sVal);` |
|      9 | 5655 | `		PH7_MemObjRelease(&sVal);` |
|      9 | 5656 | `		PH7_MemObjInitFromInt(pVm, &sVal, (sxi64)sRef.pFunc->nLine);` |
|      9 | 5657 | `		ClosurePresentAdd(pVm, pOut, "line", &sVal);` |
|      9 | 5658 | `		PH7_MemObjRelease(&sVal);` |
|      - | 5659 | `	}` |
|      - | 5660 | ``	/* `static`: the captured `use` variables first (php compiles them into the`` |
|      - | 5661 | ``	 * same table, ahead of the body's own statics), then the body's `static $x`,`` |
|      - | 5662 | `	 * whose value is the live slot once it exists and the compiled initializer` |
|      - | 5663 | `	 * before that — the rule getStaticVariables() already follows. */` |
|     15 | 5664 | `	if( sRef.pFunc ){` |
|     13 | 5665 | `		ph7_hashmap *pHm = PH7_NewHashmap(pVm, 0, 0);` |
|      - | 5666 | `		sxu32 n;` |
|     13 | 5667 | `		if( pHm ){` |
|      - | 5668 | `			ph7_value sMap;` |
|     13 | 5669 | `			ph7_value *pMap = &sMap;` |
|     13 | 5670 | `			ph7_vm_func_closure_env *aEnv =` |
|     12 | 5671 | `				(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|     13 | 5672 | `			ph7_vm_func_static_var *aStatic =` |
|     12 | 5673 | `				(ph7_vm_func_static_var *)SySetBasePtr(&sRef.pFunc->aStatic);` |
|     13 | 5674 | `			PH7_MemObjInitFromArray(pVm, pMap, pHm);` |
|     25 | 5675 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++n ){` |
|     13 | 5676 | `				ph7_value *pVal = &aEnv[n].sValue;` |
|      - | 5677 | `				ph7_value sKey;` |
|     12 | 5678 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|     11 | 5679 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|      - | 5680 | ``					/* PHL carries the auto-captured `$this` as an env entry; php keeps`` |
|      - | 5681 | ``					 * it in its own `this_ptr` slot and never lists it as a static. It`` |
|      - | 5682 | ``					 * is reported below, under `this`. */`` |
|      9 | 5683 | `					continue;` |
|      - | 5684 | `				}` |
|      5 | 5685 | `				if( aEnv[n].nIdx != SXU32_HIGH ){` |
|      - | 5686 | ``					/* `use (&$x)`: the capture is an alias onto a pinned slot, and`` |
|      - | 5687 | `					 * php reports what the slot holds NOW, not the birth value. */` |
|    ! 0 | 5688 | `					ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj, aEnv[n].nIdx);` |
|    ! 0 | 5689 | `					if( pSlot ){` |
|    ! 0 | 5690 | `						pVal = pSlot;` |
|    ! 0 | 5691 | `					}` |
|    ! 0 | 5692 | `				}` |
|      5 | 5693 | `				PH7_MemObjInitFromString(pVm, &sKey, &aEnv[n].sName);` |
|      5 | 5694 | `				ph7_array_add_elem(pMap, &sKey, pVal);` |
|      5 | 5695 | `				PH7_MemObjRelease(&sKey);` |
|      3 | 5696 | `			}` |
|     15 | 5697 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aStatic) ; ++n ){` |
|      3 | 5698 | `				ph7_value *pVal = 0;` |
|      - | 5699 | `				ph7_value sScratch, sKey;` |
|      3 | 5700 | `				int bScratch = 0;` |
|      3 | 5701 | `				if( aStatic[n].nIdx != SXU32_HIGH ){` |
|    ! 0 | 5702 | `					pVal = (ph7_value *)SySetAt(&pVm->aMemObj, aStatic[n].nIdx);` |
|    ! 0 | 5703 | `				}` |
|      3 | 5704 | `				if( pVal == 0 ){` |
|      3 | 5705 | `					PH7_MemObjInit(pVm, &sScratch);` |
|      3 | 5706 | `					if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      3 | 5707 | `						VmLocalExec(pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|      1 | 5708 | `					}` |
|      3 | 5709 | `					pVal = &sScratch;` |
|      3 | 5710 | `					bScratch = 1;` |
|      1 | 5711 | `				}` |
|      3 | 5712 | `				PH7_MemObjInitFromString(pVm, &sKey, &aStatic[n].sName);` |
|      3 | 5713 | `				ph7_array_add_elem(pMap, &sKey, pVal);` |
|      3 | 5714 | `				PH7_MemObjRelease(&sKey);` |
|      3 | 5715 | `				if( bScratch ){` |
|      3 | 5716 | `					PH7_MemObjRelease(&sScratch);` |
|      1 | 5717 | `				}` |
|      2 | 5718 | `			}` |
|     13 | 5719 | `			if( ph7_array_count(pMap) > 0 ){` |
|      5 | 5720 | `				ClosurePresentAdd(pVm, pOut, "static", pMap);` |
|      2 | 5721 | `			}` |
|     13 | 5722 | `			PH7_MemObjRelease(pMap);` |
|      6 | 5723 | `		}` |
|      6 | 5724 | `	}` |
|      - | 5725 | ``	/* `this`: the bound receiver. php keeps ONE `this_ptr` however the binding`` |
|      - | 5726 | ``	 * happened; PHL keeps two — an explicit bind (`bindTo`, an instance`` |
|      - | 5727 | ``	 * first-class callable) writes the `$__this` slot, while the IMPLICIT capture a`` |
|      - | 5728 | `	 * closure written inside a method gets rides in the closure ENVIRONMENT under` |
|      - | 5729 | ``	 * the name `this`. Either one is php's answer here. */`` |
|      - | 5730 | `	{` |
|      - | 5731 | `		SyString sAttr;` |
|      - | 5732 | `		ph7_value *pBound;` |
|     15 | 5733 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     15 | 5734 | `		pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     15 | 5735 | `		if( (pBound == 0 \|\| (pBound->iFlags & MEMOBJ_OBJ) == 0) && sRef.pFunc ){` |
|     11 | 5736 | `			ph7_vm_func_closure_env *aEnv =` |
|     10 | 5737 | `				(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|      - | 5738 | `			sxu32 n;` |
|     11 | 5739 | `			pBound = 0;` |
|     15 | 5740 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++n ){` |
|     12 | 5741 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|     11 | 5742 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|      9 | 5743 | `					pBound = &aEnv[n].sValue;` |
|      9 | 5744 | `					break;` |
|      - | 5745 | `				}` |
|      3 | 5746 | `			}` |
|      5 | 5747 | `		}` |
|     15 | 5748 | `		if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|      5 | 5749 | `			ClosurePresentAdd(pVm, pOut, "this", pBound);` |
|      2 | 5750 | `		}` |
|      - | 5751 | `	}` |
|      - | 5752 | ``	/* `parameter`: php's cut is required_num_args, which runs through the LAST`` |
|      - | 5753 | `	 * required parameter rather than stopping at the first optional one. */` |
|      - | 5754 | `	{` |
|      - | 5755 | `		ReflectParamDesc sDesc;` |
|     15 | 5756 | `		int nArgTotal = 0, nRequired = 0, i;` |
|     31 | 5757 | `		while( ReflectParamAt(&sRef, nArgTotal, &sDesc) ){` |
|     17 | 5758 | `			if( !sDesc.bOptional ){` |
|     11 | 5759 | `				nRequired = nArgTotal + 1;` |
|      5 | 5760 | `			}` |
|     17 | 5761 | `			nArgTotal++;` |
|      1 | 5762 | `		}` |
|     15 | 5763 | `		if( nArgTotal > 0 ){` |
|      9 | 5764 | `			ph7_hashmap *pHm = PH7_NewHashmap(pVm, 0, 0);` |
|      9 | 5765 | `			if( pHm ){` |
|      - | 5766 | `				ph7_value sMap;` |
|      9 | 5767 | `				ph7_value *pMap = &sMap;` |
|      9 | 5768 | `				PH7_MemObjInitFromArray(pVm, pMap, pHm);` |
|     25 | 5769 | `				for( i = 0 ; i < nArgTotal ; ++i ){` |
|      - | 5770 | `					ph7_value sKey, sWhat;` |
|     17 | 5771 | `					if( !ReflectParamAt(&sRef, i, &sDesc) ){` |
|    ! 0 | 5772 | `						break;` |
|      - | 5773 | `					}` |
|     17 | 5774 | `					PH7_MemObjInitFromString(pVm, &sKey, 0);` |
|     17 | 5775 | `					if( sDesc.bByRef ){` |
|      3 | 5776 | `						PH7_MemObjStringAppend(&sKey, "&", 1);` |
|      1 | 5777 | `					}` |
|     17 | 5778 | `					PH7_MemObjStringAppend(&sKey, "$", 1);` |
|     25 | 5779 | `					PH7_MemObjStringAppend(&sKey, SyStringData(&sDesc.sName),` |
|      8 | 5780 | `						SyStringLength(&sDesc.sName));` |
|     17 | 5781 | `					PH7_MemObjInitFromString(pVm, &sWhat, 0);` |
|     17 | 5782 | `					PH7_MemObjStringAppend(&sWhat,` |
|      8 | 5783 | `						i >= nRequired ? "<optional>" : "<required>",` |
|      8 | 5784 | `						i >= nRequired ? sizeof("<optional>")-1 : sizeof("<required>")-1);` |
|     17 | 5785 | `					ph7_array_add_elem(pMap, &sKey, &sWhat);` |
|     17 | 5786 | `					PH7_MemObjRelease(&sKey);` |
|     17 | 5787 | `					PH7_MemObjRelease(&sWhat);` |
|      9 | 5788 | `				}` |
|      9 | 5789 | `				ClosurePresentAdd(pVm, pOut, "parameter", pMap);` |
|      9 | 5790 | `				PH7_MemObjRelease(pMap);` |
|      4 | 5791 | `			}` |
|      4 | 5792 | `		}` |
|      - | 5793 | `	}` |
|     15 | 5794 | `	return SXRET_OK;` |
|     12 | 5795 | `}` |
|      2 | 5796 | `static int vm_builtin_ReflectionFunc_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5797 | `{` |
|      - | 5798 | `	ReflectFuncRef sRef;` |
|      1 | 5799 | `	SXUNUSED(nArg);` |
|      1 | 5800 | `	SXUNUSED(apArg);` |
|      3 | 5801 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|      3 | 5802 | `	if( ReflectFuncIsInternal(&sRef) ){` |
|      3 | 5803 | `		ph7_result_string(pCtx, "Core", sizeof("Core")-1);` |
|      2 | 5804 | `	}else{` |
|    ! 0 | 5805 | `		ph7_result_bool(pCtx, 0);` |
|      - | 5806 | `	}` |
|      3 | 5807 | `	return PH7_OK;` |
|      2 | 5808 | `}` |
|      4 | 5809 | `static int vm_builtin_ReflectionFunc_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5810 | `{` |
|      - | 5811 | `	ReflectFuncRef sRef;` |
|      2 | 5812 | `	SXUNUSED(nArg);` |
|      2 | 5813 | `	SXUNUSED(apArg);` |
|      5 | 5814 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|      5 | 5815 | `	if( !ReflectFuncIsInternal(&sRef) ){` |
|    ! 0 | 5816 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5817 | `		return PH7_OK;` |
|      - | 5818 | `	}` |
|      5 | 5819 | `	return ReflectCoreExtension(pCtx);` |
|      3 | 5820 | `}` |
|     82 | 5821 | `static int vm_builtin_ReflectionFunc_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 5822 | `{` |
|     85 | 5823 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 5824 | `	ReflectFuncRef sRef;` |
|      - | 5825 | `	int rc;` |
|     85 | 5826 | `	if( pThis == 0 \|\| !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pFunc == 0 ){` |
|    ! 0 | 5827 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 | 5828 | `		return PH7_OK;` |
|      - | 5829 | `	}` |
|     85 | 5830 | `	if( sRef.pMeth ){` |
|      - | 5831 | `		/* 4 = Attribute::TARGET_METHOD */` |
|      - | 5832 | `		ph7_value sTarget;` |
|      - | 5833 | `		const char *zClass, *zName;` |
|      - | 5834 | `		int nClass, nName;` |
|     71 | 5835 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     71 | 5836 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     71 | 5837 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     71 | 5838 | `		ph7_value_string(&sTarget, zClass, nClass);` |
|    105 | 5839 | `		rc = ReflectBuildAttrs(pCtx, &sRef.pFunc->aAttrs, "method", &sTarget,` |
|     34 | 5840 | `			zName, nName, 0, 4, nArg, apArg);` |
|     71 | 5841 | `		PH7_MemObjRelease(&sTarget);` |
|     71 | 5842 | `		return rc;` |
|      - | 5843 | `	}` |
|      - | 5844 | `	{` |
|      - | 5845 | `		/* 2 = Attribute::TARGET_FUNCTION */` |
|      - | 5846 | `		ph7_value sTarget;` |
|     16 | 5847 | `		ReflectFuncParamSpec(pCtx, &sTarget);` |
|     23 | 5848 | `		rc = ReflectBuildAttrs(pCtx, &sRef.pFunc->aAttrs, "fn", &sTarget, 0, 0, 0, 2,` |
|      7 | 5849 | `			nArg, apArg);` |
|     16 | 5850 | `		if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|     12 | 5851 | `			PH7_MemObjRelease(&sTarget);` |
|      5 | 5852 | `		}` |
|     16 | 5853 | `		return rc;` |
|      - | 5854 | `	}` |
|     44 | 5855 | `}` |
|      - | 5856 | `/* __toString(): php's export format, still chunk 9. */` |
|     54 | 5857 | `static int vm_builtin_ReflectionFunc_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 5858 | `{` |
|     27 | 5859 | `	SXUNUSED(nArg);` |
|     27 | 5860 | `	SXUNUSED(apArg);` |
|     56 | 5861 | `	return ReflectExportFuncSelf(pCtx);` |
|      2 | 5862 | `}` |
|      - | 5863 | `/* ---- ReflectionFunction ---- */` |
|      - | 5864 | `/* ReflectionFunction::__construct(Closure\|string $function) */` |
|    432 | 5865 | `static int vm_builtin_ReflectionFunction_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 | 5866 | `{` |
|    436 | 5867 | `	ph7_vm *pVm = pCtx->pVm;` |
|    436 | 5868 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 5869 | `	ReflectFuncRef sRef;` |
|      - | 5870 | `	ph7_class_instance *pClo;` |
|    436 | 5871 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 5872 | `		return PH7_OK;` |
|      - | 5873 | `	}` |
|    436 | 5874 | `	pClo = ReflectValueClosure(pVm, apArg[0]);` |
|    436 | 5875 | `	if( !ReflectFuncFill(pCtx->pVm, apArg[0], 0, &sRef) ){` |
|      - | 5876 | `		const char *zName;` |
|      - | 5877 | `		int nName;` |
|     15 | 5878 | `		if( pClo ){` |
|      - | 5879 | `			/* A Closure whose body no table holds: still a valid reflection` |
|      - | 5880 | `			 * target, so record it and let the accessors answer emptily. */` |
|    ! 0 | 5881 | `			PH7_NativeSetAttrObj(pVm, pThis, RF_CL, pClo);` |
|    ! 0 | 5882 | `			return PH7_OK;` |
|      - | 5883 | `		}` |
|     15 | 5884 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|     21 | 5885 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      6 | 5886 | `			"Function %.*s() does not exist", nName, zName);` |
|      - | 5887 | `	}` |
|    424 | 5888 | `	if( pClo ){` |
|     66 | 5889 | `		PH7_NativeSetAttrObj(pVm, pThis, RF_CL, pClo);` |
|     31 | 5890 | `	}` |
|    424 | 5891 | `	if( sRef.pFunc ){` |
|    224 | 5892 | `		int bAnon = (sRef.pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|    220 | 5893 | `		if( !bAnon && SyStringLength(&sRef.pFunc->sName) > 9` |
|    117 | 5894 | `		 && (SyMemcmp(SyStringData(&sRef.pFunc->sName), "[lambda_", 8) == 0` |
|     41 | 5895 | `		  \|\| SyMemcmp(SyStringData(&sRef.pFunc->sName), "[closure_", 9) == 0) ){` |
|      3 | 5896 | `			bAnon = 1;` |
|      1 | 5897 | `		}` |
|    224 | 5898 | `		if( bAnon ){` |
|      - | 5899 | ``			/* php 8.4 names an anonymous function `{closure:SCOPE:LINE}`, where SCOPE is`` |
|      - | 5900 | `			 * the enclosing function and only the FILE at top level — the compiler` |
|      - | 5901 | `			 * recorded it (sClosureName); the file form is the fallback for a closure` |
|      - | 5902 | `			 * built without one. */` |
|      - | 5903 | `			char zBuf[512];` |
|     42 | 5904 | `			const char *zShow = SyStringData(&sRef.pFunc->sClosureName);` |
|     42 | 5905 | `			int nShow = (int)SyStringLength(&sRef.pFunc->sClosureName);` |
|     42 | 5906 | `			if( nShow < 1 ){` |
|    ! 0 | 5907 | `				const char *zFile = SyStringLength(&sRef.pFunc->sFile) > 0` |
|    ! 0 | 5908 | `					? SyStringData(&sRef.pFunc->sFile) : "";` |
|    ! 0 | 5909 | `				int nFile = (int)SyStringLength(&sRef.pFunc->sFile);` |
|    ! 0 | 5910 | `				nShow = SyBufferFormat(zBuf, sizeof(zBuf), "{closure:%.*s:%u}",` |
|    ! 0 | 5911 | `					nFile, zFile, sRef.pFunc->nLine);` |
|    ! 0 | 5912 | `				zShow = zBuf;` |
|    ! 0 | 5913 | `			}` |
|     42 | 5914 | `			PH7_NativeSetAttrStr(pVm, pThis, "name", zShow, nShow);` |
|     42 | 5915 | `			if( pClo == 0 ){` |
|      - | 5916 | `				/* Named by its lambda name: keep a Closure so the accessors can` |
|      - | 5917 | `				 * still reach the captured scope. */` |
|    ! 0 | 5918 | `				PH7_NativeSetAttrObj(pVm, pThis, RF_CL,` |
|    ! 0 | 5919 | `					PH7_VmNewClosure(pVm, &sRef.pFunc->sName, 0, 0));` |
|    ! 0 | 5920 | `			}` |
|     42 | 5921 | `			return PH7_OK;` |
|      - | 5922 | `		}` |
|    275 | 5923 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sRef.pFunc->sName),` |
|    182 | 5924 | `			(int)SyStringLength(&sRef.pFunc->sName));` |
|    184 | 5925 | `		return PH7_OK;` |
|      - | 5926 | `	}` |
|    302 | 5927 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sRef.pHost->sName),` |
|    200 | 5928 | `		(int)SyStringLength(&sRef.pHost->sName));` |
|    202 | 5929 | `	return PH7_OK;` |
|    220 | 5930 | `}` |
|      6 | 5931 | `static int vm_builtin_ReflectionFunction_isAnonymous(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5932 | `{` |
|      7 | 5933 | `	return vm_builtin_ReflectionFunc_isClosure(pCtx, nArg, apArg);` |
|      1 | 5934 | `}` |
|    ! 0 | 5935 | `static int vm_builtin_ReflectionFunction_isDisabled(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 5936 | `{` |
|    ! 0 | 5937 | `	SXUNUSED(nArg);` |
|    ! 0 | 5938 | `	SXUNUSED(apArg);` |
|    ! 0 | 5939 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 | 5940 | `	return PH7_OK;` |
|    ! 0 | 5941 | `}` |
|      - | 5942 | `/* The callable a ReflectionFunction invokes: the Closure it holds, or its name. */` |
|     18 | 5943 | `static void ReflectFunctionCallable(ph7_context *pCtx, ph7_value *pOut)` |
|      1 | 5944 | `{` |
|     19 | 5945 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 5946 | `	ph7_class_instance *pClo;` |
|     19 | 5947 | `	const char *zName = "";` |
|     19 | 5948 | `	int nName = 0;` |
|     19 | 5949 | `	PH7_MemObjInit(pCtx->pVm, pOut);` |
|     19 | 5950 | `	if( pThis == 0 ){` |
|    ! 0 | 5951 | `		return;` |
|      - | 5952 | `	}` |
|     19 | 5953 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|     19 | 5954 | `	if( pClo ){` |
|      9 | 5955 | `		pOut->x.pOther = pClo;` |
|      9 | 5956 | `		pOut->iFlags = MEMOBJ_OBJ;` |
|      9 | 5957 | `		return;` |
|      - | 5958 | `	}` |
|     11 | 5959 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     11 | 5960 | `	ph7_value_string(pOut, zName, nName);` |
|     10 | 5961 | `}` |
|     18 | 5962 | `static int ReflectFunctionInvoke(ph7_context *pCtx, int nCall, ph7_value **apCall)` |
|      1 | 5963 | `{` |
|      - | 5964 | `	ph7_value sTarget, sResult;` |
|      - | 5965 | `	sxi32 rc;` |
|     19 | 5966 | `	ReflectFunctionCallable(pCtx, &sTarget);` |
|     19 | 5967 | `	PH7_MemObjInit(pCtx->pVm, &sResult);` |
|     19 | 5968 | `	sResult.nIdx = SXU32_HIGH;` |
|     19 | 5969 | `	rc = PH7_VmCallUserFunction(pCtx->pVm, &sTarget, nCall, apCall, &sResult);` |
|     19 | 5970 | `	if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|     11 | 5971 | `		PH7_MemObjRelease(&sTarget);` |
|      5 | 5972 | `	}` |
|     19 | 5973 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|    ! 0 | 5974 | `		PH7_MemObjRelease(&sResult);` |
|    ! 0 | 5975 | `		return rc;` |
|      - | 5976 | `	}` |
|     19 | 5977 | `	ph7_result_value(pCtx, &sResult);` |
|     19 | 5978 | `	PH7_MemObjRelease(&sResult);` |
|     19 | 5979 | `	return PH7_OK;` |
|     10 | 5980 | `}` |
|     16 | 5981 | `static int vm_builtin_ReflectionFunction_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5982 | `{` |
|     17 | 5983 | `	return ReflectFunctionInvoke(pCtx, nArg, apArg);` |
|      1 | 5984 | `}` |
|      2 | 5985 | `static int vm_builtin_ReflectionFunction_invokeArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5986 | `{` |
|      - | 5987 | `	SySet aCall;` |
|      - | 5988 | `	int rc;` |
|      3 | 5989 | `	SySetInit(&aCall, &pCtx->pVm->sAllocator, sizeof(ph7_value *));` |
|      3 | 5990 | `	if( nArg > 0 ){` |
|      3 | 5991 | `		ReflectCollectArgs(pCtx, apArg[0], &aCall, 0);` |
|      1 | 5992 | `	}` |
|      3 | 5993 | `	rc = ReflectFunctionInvoke(pCtx, (int)SySetUsed(&aCall), (ph7_value **)SySetBasePtr(&aCall));` |
|      3 | 5994 | `	SySetRelease(&aCall);` |
|      3 | 5995 | `	return rc;` |
|      1 | 5996 | `}` |
|      6 | 5997 | `static int vm_builtin_ReflectionFunction_getClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5998 | `{` |
|      7 | 5999 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 6000 | `	ph7_class_instance *pClo;` |
|      7 | 6001 | `	const char *zName = "";` |
|      7 | 6002 | `	int nName = 0;` |
|      - | 6003 | `	SyString sName;` |
|      3 | 6004 | `	SXUNUSED(nArg);` |
|      3 | 6005 | `	SXUNUSED(apArg);` |
|      7 | 6006 | `	if( pThis == 0 ){` |
|    ! 0 | 6007 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6008 | `		return PH7_OK;` |
|      - | 6009 | `	}` |
|      7 | 6010 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|      7 | 6011 | `	if( pClo ){` |
|      - | 6012 | `		/* Already a Closure: hand the same instance back */` |
|      3 | 6013 | `		return ReflectResultExistingObject(pCtx, pClo);` |
|      - | 6014 | `	}` |
|      5 | 6015 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      5 | 6016 | `	SyStringInitFromBuf(&sName, zName, nName);` |
|      5 | 6017 | `	return ReflectResultObject(pCtx, PH7_VmNewClosure(pCtx->pVm, &sName, 0, 0));` |
|      4 | 6018 | `}` |
|      - | 6019 | `/* ---- ReflectionMethod ---- */` |
|      - | 6020 | `/* ReflectionMethod::__construct(object\|string $objectOrMethod, ?string $method = null) */` |
|    672 | 6021 | `static int vm_builtin_ReflectionMethod_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 | 6022 | `{` |
|    677 | 6023 | `	ph7_vm *pVm = pCtx->pVm;` |
|    677 | 6024 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 6025 | `	ph7_class *pClass;` |
|      - | 6026 | `	SyHashEntry *pEntry;` |
|      - | 6027 | `	ph7_value sClass, sMethod;` |
|      - | 6028 | `	const char *zMethod;` |
|    677 | 6029 | `	int nMethod, rc = PH7_OK;` |
|    677 | 6030 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 6031 | `		return PH7_OK;` |
|      - | 6032 | `	}` |
|    677 | 6033 | `	PH7_MemObjInit(pVm, &sClass);` |
|    677 | 6034 | `	PH7_MemObjInit(pVm, &sMethod);` |
|    678 | 6035 | `	if( nArg < 2 \|\| ph7_value_is_null(apArg[1]) ){` |
|      - | 6036 | `		/* One-argument form: "Class::method" */` |
|      - | 6037 | `		const char *zSpec;` |
|      3 | 6038 | `		int nSpec, iSep = -1, k;` |
|      3 | 6039 | `		if( (apArg[0]->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 6040 | `			PH7_MemObjRelease(&sClass);` |
|    ! 0 | 6041 | `			PH7_MemObjRelease(&sMethod);` |
|    ! 0 | 6042 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 6043 | `				"The parameter class is expected to be either a string or an object");` |
|      - | 6044 | `		}` |
|      3 | 6045 | `		zSpec = ph7_value_to_string(apArg[0], &nSpec);` |
|     19 | 6046 | `		for( k = 0 ; k + 1 < nSpec ; k++ ){` |
|     19 | 6047 | `			if( zSpec[k] == ':' && zSpec[k+1] == ':' ){` |
|      3 | 6048 | `				iSep = k;` |
|      3 | 6049 | `				break;` |
|      - | 6050 | `			}` |
|      9 | 6051 | `		}` |
|      3 | 6052 | `		if( iSep < 0 ){` |
|    ! 0 | 6053 | `			PH7_MemObjRelease(&sClass);` |
|    ! 0 | 6054 | `			PH7_MemObjRelease(&sMethod);` |
|    ! 0 | 6055 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 | 6056 | `				"Invalid method name %.*s", nSpec, zSpec);` |
|      - | 6057 | `		}` |
|      - | 6058 | `		/* php 8.3 deprecated the one-argument spelling in favour of the static` |
|      - | 6059 | `		 * factory. createFromMethodName() splits the name ITSELF and constructs` |
|      - | 6060 | `		 * with two, so it does not trip this. */` |
|      3 | 6061 | `		PH7_VmThrowError(pVm, 0, E_DEPRECATED,` |
|      - | 6062 | `			"Calling ReflectionMethod::__construct() with 1 argument is deprecated, "` |
|      - | 6063 | `			"use ReflectionMethod::createFromMethodName() instead");` |
|      3 | 6064 | `		ph7_value_string(&sClass, zSpec, iSep);` |
|      3 | 6065 | `		ph7_value_string(&sMethod, &zSpec[iSep+2], nSpec - iSep - 2);` |
|      2 | 6066 | `	}else{` |
|    675 | 6067 | `		PH7_MemObjStore(apArg[0], &sClass);` |
|    675 | 6068 | `		PH7_MemObjStore(apArg[1], &sMethod);` |
|      - | 6069 | `	}` |
|    677 | 6070 | `	pClass = ReflectResolveClass(pVm, &sClass);` |
|    677 | 6071 | `	if( pClass == 0 ){` |
|      - | 6072 | `		const char *zName;` |
|      - | 6073 | `		int nName;` |
|      3 | 6074 | `		zName = ph7_value_to_string(&sClass, &nName);` |
|      4 | 6075 | `		rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 6076 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      3 | 6077 | `		goto Done;` |
|      - | 6078 | `	}` |
|    675 | 6079 | `	zMethod = ph7_value_to_string(&sMethod, &nMethod);` |
|    675 | 6080 | `	pEntry = ReflectFindMethodEntry(pClass, zMethod, nMethod);` |
|    675 | 6081 | `	if( pEntry == 0 ){` |
|     10 | 6082 | `		rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 | 6083 | `			"Method %z::%.*s() does not exist", &pClass->sName, nMethod, zMethod);` |
|      7 | 6084 | `		goto Done;` |
|      - | 6085 | `	}` |
|      - | 6086 | `	{` |
|      - | 6087 | `		/* php's $class is the DECLARING class, not the one the lookup went` |
|      - | 6088 | ``		 * through: `new ReflectionMethod('Kid','bm')` on an inherited method`` |
|      - | 6089 | `		 * reports Base. */` |
|    669 | 6090 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, (ph7_class_method *)pEntry->pUserData);` |
|   1001 | 6091 | `		PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pDecl->sName),` |
|    664 | 6092 | `			(int)SyStringLength(&pDecl->sName));` |
|      - | 6093 | `	}` |
|      - | 6094 | `	/* The DECLARED spelling, whatever case was asked for. */` |
|    669 | 6095 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", (const char *)pEntry->pKey, (int)pEntry->nKeyLen);` |
|    336 | 6096 | `Done:` |
|    677 | 6097 | `	PH7_MemObjRelease(&sClass);` |
|    677 | 6098 | `	PH7_MemObjRelease(&sMethod);` |
|    677 | 6099 | `	return rc;` |
|    341 | 6100 | `}` |
|      - | 6101 | `/* ReflectionMethod::createFromMethodName(string $method): static */` |
|      4 | 6102 | `static int vm_builtin_ReflectionMethod_createFromMethodName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6103 | `{` |
|      - | 6104 | `	ph7_class_instance *pOut;` |
|      - | 6105 | `	ph7_value sClass, sMethod;` |
|      - | 6106 | `	ph7_value *apCtor[2];` |
|      - | 6107 | `	const char *zSpec;` |
|      5 | 6108 | `	int nSpec, iSep = -1, k;` |
|      - | 6109 | `	sxi32 rc;` |
|      5 | 6110 | `	if( nArg < 1 ){` |
|    ! 0 | 6111 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6112 | `		return PH7_OK;` |
|      - | 6113 | `	}` |
|      - | 6114 | `	/* Split here rather than letting the constructor do it: the one-argument` |
|      - | 6115 | `	 * constructor is the DEPRECATED spelling this factory exists to replace. */` |
|      5 | 6116 | `	zSpec = ph7_value_to_string(apArg[0], &nSpec);` |
|     43 | 6117 | `	for( k = 0 ; k + 1 < nSpec ; k++ ){` |
|     43 | 6118 | `		if( zSpec[k] == ':' && zSpec[k+1] == ':' ){` |
|      5 | 6119 | `			iSep = k;` |
|      5 | 6120 | `			break;` |
|      - | 6121 | `		}` |
|     20 | 6122 | `	}` |
|      5 | 6123 | `	if( iSep < 0 ){` |
|    ! 0 | 6124 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 | 6125 | `			"Invalid method name %.*s", nSpec, zSpec);` |
|      - | 6126 | `	}` |
|      5 | 6127 | `	PH7_MemObjInit(pCtx->pVm, &sClass);` |
|      5 | 6128 | `	PH7_MemObjInit(pCtx->pVm, &sMethod);` |
|      5 | 6129 | `	ph7_value_string(&sClass, zSpec, iSep);` |
|      5 | 6130 | `	ph7_value_string(&sMethod, &zSpec[iSep+2], nSpec - iSep - 2);` |
|      5 | 6131 | `	apCtor[0] = &sClass;` |
|      5 | 6132 | `	apCtor[1] = &sMethod;` |
|      5 | 6133 | `	pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|      5 | 6134 | `	PH7_MemObjRelease(&sClass);` |
|      5 | 6135 | `	PH7_MemObjRelease(&sMethod);` |
|      5 | 6136 | `	if( pOut == 0 ){` |
|    ! 0 | 6137 | `		if( rc != PH7_OK ){` |
|    ! 0 | 6138 | `			return rc;` |
|      - | 6139 | `		}` |
|    ! 0 | 6140 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6141 | `		return PH7_OK;` |
|      - | 6142 | `	}` |
|      5 | 6143 | `	return ReflectResultObject(pCtx, pOut);` |
|      3 | 6144 | `}` |
|      - | 6145 | `/* The visibility/modifier predicates, all off the method record. */` |
|     26 | 6146 | `static int ReflectMethodFlag(ph7_context *pCtx, int iWhat)` |
|      1 | 6147 | `{` |
|      - | 6148 | `	ReflectFuncRef sRef;` |
|     27 | 6149 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|    ! 0 | 6150 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6151 | `		return PH7_OK;` |
|      - | 6152 | `	}` |
|     27 | 6153 | `	switch( iWhat ){` |
|      5 | 6154 | `	case 0: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PUBLIC); break;` |
|      9 | 6155 | `	case 1: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PRIVATE); break;` |
|      3 | 6156 | `	case 2: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PROTECTED); break;` |
|      3 | 6157 | `	case 3: ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0); break;` |
|     11 | 6158 | `	default: ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0); break;` |
|      - | 6159 | `	}` |
|     27 | 6160 | `	return PH7_OK;` |
|     14 | 6161 | `}` |
|      - | 6162 | `#define REFLECT_METHOD_FLAG(NAME,WHAT) \` |
|      - | 6163 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 6164 | `	{ \` |
|      - | 6165 | `		SXUNUSED(nArg); \` |
|      - | 6166 | `		SXUNUSED(apArg); \` |
|      - | 6167 | `		return ReflectMethodFlag(pCtx, WHAT); \` |
|      - | 6168 | `	}` |
|      5 | 6169 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isPublic, 0)` |
|      9 | 6170 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isPrivate, 1)` |
|      3 | 6171 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isProtected, 2)` |
|      3 | 6172 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isAbstract, 3)` |
|     11 | 6173 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isFinal, 4)` |
|      - | 6174 |  |
|      - | 6175 | `/* isConstructor()/isDestructor() read the NAME, not the record: a trait` |
|      - | 6176 | `` * `use T { m as __construct; }` alias is the constructor under that key. */`` |
|      4 | 6177 | `static int ReflectMethodNameIs(ph7_context *pCtx, const char *zWant, int nWant)` |
|      2 | 6178 | `{` |
|      6 | 6179 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      6 | 6180 | `	const char *zName = "";` |
|      6 | 6181 | `	int nName = 0;` |
|      6 | 6182 | `	if( pThis ){` |
|      6 | 6183 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 | 6184 | `	}` |
|      6 | 6185 | `	ph7_result_bool(pCtx, nName == nWant && SyStrnicmp(zName, zWant, (sxu32)nWant) == 0);` |
|      6 | 6186 | `	return PH7_OK;` |
|      2 | 6187 | `}` |
|      4 | 6188 | `static int vm_builtin_ReflectionMethod_isConstructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 6189 | `{` |
|      2 | 6190 | `	SXUNUSED(nArg);` |
|      2 | 6191 | `	SXUNUSED(apArg);` |
|      6 | 6192 | `	return ReflectMethodNameIs(pCtx, "__construct", sizeof("__construct")-1);` |
|      2 | 6193 | `}` |
|    ! 0 | 6194 | `static int vm_builtin_ReflectionMethod_isDestructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 6195 | `{` |
|    ! 0 | 6196 | `	SXUNUSED(nArg);` |
|    ! 0 | 6197 | `	SXUNUSED(apArg);` |
|    ! 0 | 6198 | `	return ReflectMethodNameIs(pCtx, "__destruct", sizeof("__destruct")-1);` |
|    ! 0 | 6199 | `}` |
|      8 | 6200 | `static int vm_builtin_ReflectionMethod_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6201 | `{` |
|      - | 6202 | `	ReflectFuncRef sRef;` |
|      4 | 6203 | `	SXUNUSED(nArg);` |
|      4 | 6204 | `	SXUNUSED(apArg);` |
|      9 | 6205 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|    ! 0 | 6206 | `		ph7_result_int(pCtx, 0);` |
|    ! 0 | 6207 | `		return PH7_OK;` |
|      - | 6208 | `	}` |
|      9 | 6209 | `	ph7_result_int64(pCtx, ReflectMethodModifiers(sRef.pMeth));` |
|      9 | 6210 | `	return PH7_OK;` |
|      5 | 6211 | `}` |
|    260 | 6212 | `static int vm_builtin_ReflectionMethod_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 6213 | `{` |
|      - | 6214 | `	ReflectFuncRef sRef;` |
|    130 | 6215 | `	SXUNUSED(nArg);` |
|    130 | 6216 | `	SXUNUSED(apArg);` |
|    263 | 6217 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 | 6218 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6219 | `		return PH7_OK;` |
|      - | 6220 | `	}` |
|    263 | 6221 | `	return ReflectResultClassOf(pCtx, ReflectFuncDeclClass(&sRef));` |
|    133 | 6222 | `}` |
|      - | 6223 | `/*` |
|      - | 6224 | ` * The receiver check php runs before invoking or binding: a non-static method` |
|      - | 6225 | ` * needs an instance OF ITS DECLARING CLASS; a static one ignores whatever was` |
|      - | 6226 | ` * handed over. *ppThis is cleared for a static method.` |
|      - | 6227 | ` */` |
|     40 | 6228 | `static sxi32 ReflectMethodReceiver(ph7_context *pCtx, ReflectFuncRef *pRef,` |
|      - | 6229 | `	ph7_value *pObject, ph7_class_instance **ppThis, int bClosure)` |
|      1 | 6230 | `{` |
|     41 | 6231 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     41 | 6232 | `	const char *zClass = "", *zName = "";` |
|     41 | 6233 | `	int nClass = 0, nName = 0;` |
|     41 | 6234 | `	ph7_class *pDecl = ReflectFuncDeclClass(pRef);` |
|     41 | 6235 | `	*ppThis = 0;` |
|     41 | 6236 | `	if( pThis ){` |
|     41 | 6237 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     41 | 6238 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     20 | 6239 | `	}` |
|     41 | 6240 | `	if( pRef->pMeth && (pRef->pMeth->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|     11 | 6241 | `		return PH7_OK;` |
|      - | 6242 | `	}` |
|     31 | 6243 | `	if( pObject == 0 \|\| (pObject->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      9 | 6244 | `		if( bClosure ){` |
|      5 | 6245 | `			return PH7_VmThrowException(pCtx, "ValueError",` |
|      - | 6246 | `				"ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods");` |
|      - | 6247 | `		}` |
|      7 | 6248 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 6249 | `			"Trying to invoke non static method %.*s::%.*s() without an object",` |
|      2 | 6250 | `			nClass, zClass, nName, zName);` |
|      - | 6251 | `	}` |
|     23 | 6252 | `	*ppThis = (ph7_class_instance *)pObject->x.pOther;` |
|     23 | 6253 | `	if( pDecl && !PH7_VmInstanceOf((*ppThis)->pClass, pDecl) ){` |
|      3 | 6254 | `		*ppThis = 0;` |
|      3 | 6255 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 6256 | `			"Given object is not an instance of the class this method was declared in");` |
|      - | 6257 | `	}` |
|     21 | 6258 | `	return PH7_OK;` |
|     21 | 6259 | `}` |
|     24 | 6260 | `static int ReflectMethodInvoke(ph7_context *pCtx, ph7_value *pObject, int nCall, ph7_value **apCall)` |
|      1 | 6261 | `{` |
|     25 | 6262 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 6263 | `	ReflectFuncRef sRef;` |
|     25 | 6264 | `	ph7_class_instance *pRecv = 0;` |
|      - | 6265 | `	ph7_value sResult;` |
|      - | 6266 | `	sxi32 rc;` |
|     25 | 6267 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|    ! 0 | 6268 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6269 | `		return PH7_OK;` |
|      - | 6270 | `	}` |
|     25 | 6271 | `	rc = ReflectMethodReceiver(pCtx, &sRef, pObject, &pRecv, 0);` |
|     25 | 6272 | `	if( rc != PH7_OK ){` |
|      7 | 6273 | `		return rc;` |
|      - | 6274 | `	}` |
|     19 | 6275 | `	PH7_MemObjInit(pVm, &sResult);` |
|     19 | 6276 | `	sResult.nIdx = SXU32_HIGH;` |
|      - | 6277 | `	/* Reflection ignores method visibility (PHP 8.1+); the flag is consumed by` |
|      - | 6278 | `	 * the first OP_CALL, i.e. this synthetic one. */` |
|     19 | 6279 | `	pVm->bReflectBypass = 1;` |
|     19 | 6280 | `	rc = PH7_VmCallClassMethod(pVm, pRecv, sRef.pMeth, &sResult, nCall, apCall);` |
|     19 | 6281 | `	pVm->bReflectBypass = 0;` |
|     19 | 6282 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|    ! 0 | 6283 | `		PH7_MemObjRelease(&sResult);` |
|    ! 0 | 6284 | `		return rc;` |
|      - | 6285 | `	}` |
|     19 | 6286 | `	ph7_result_value(pCtx, &sResult);` |
|     19 | 6287 | `	PH7_MemObjRelease(&sResult);` |
|     19 | 6288 | `	return PH7_OK;` |
|     13 | 6289 | `}` |
|     20 | 6290 | `static int vm_builtin_ReflectionMethod_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6291 | `{` |
|     41 | 6292 | `	return ReflectMethodInvoke(pCtx, nArg > 0 ? apArg[0] : 0,` |
|     20 | 6293 | `		nArg > 1 ? nArg - 1 : 0, nArg > 1 ? &apArg[1] : 0);` |
|      1 | 6294 | `}` |
|      4 | 6295 | `static int vm_builtin_ReflectionMethod_invokeArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6296 | `{` |
|      - | 6297 | `	SySet aCall;` |
|      - | 6298 | `	int rc;` |
|      5 | 6299 | `	SySetInit(&aCall, &pCtx->pVm->sAllocator, sizeof(ph7_value *));` |
|      5 | 6300 | `	if( nArg > 1 ){` |
|      5 | 6301 | `		ReflectCollectArgs(pCtx, apArg[1], &aCall, 0);` |
|      2 | 6302 | `	}` |
|      3 | 6303 | `	rc = ReflectMethodInvoke(pCtx, nArg > 0 ? apArg[0] : 0,` |
|      4 | 6304 | `		(int)SySetUsed(&aCall), (ph7_value **)SySetBasePtr(&aCall));` |
|      5 | 6305 | `	SySetRelease(&aCall);` |
|      5 | 6306 | `	return rc;` |
|      1 | 6307 | `}` |
|     16 | 6308 | `static int vm_builtin_ReflectionMethod_getClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6309 | `{` |
|      - | 6310 | `	ReflectFuncRef sRef;` |
|     17 | 6311 | `	ph7_class_instance *pRecv = 0;` |
|      - | 6312 | `	sxi32 rc;` |
|     17 | 6313 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 \|\| sRef.pClass == 0 ){` |
|    ! 0 | 6314 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6315 | `		return PH7_OK;` |
|      - | 6316 | `	}` |
|     17 | 6317 | `	rc = ReflectMethodReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pRecv, 1);` |
|     17 | 6318 | `	if( rc != PH7_OK ){` |
|      5 | 6319 | `		return rc;` |
|      - | 6320 | `	}` |
|      - | 6321 | `	{` |
|      - | 6322 | `		/* php hands back a closure over the RESOLVED method and reflection is allowed past` |
|      - | 6323 | `		 * protection (8.1+), so this one must dispatch without a second visibility decision —` |
|      - | 6324 | `		 * the FCC_METHOD mark is what tells the unwrap its $__fn is a screened METHOD name. */` |
|     19 | 6325 | `		ph7_class_instance *pClo = PH7_VmNewClosure(pCtx->pVm, &sRef.pMeth->sFunc.sName,` |
|     12 | 6326 | `			pRecv, &sRef.pClass->sName);` |
|     13 | 6327 | `		if( pClo ){` |
|     13 | 6328 | `			pClo->iFlags \|= VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SCREENED;` |
|      6 | 6329 | `		}` |
|     13 | 6330 | `		return ReflectResultObject(pCtx, pClo);` |
|      - | 6331 | `	}` |
|      9 | 6332 | `}` |
|      - | 6333 | `/* php 8.1 made every reflected member accessible; the setter is a no-op it kept. */` |
|    ! 0 | 6334 | `static int vm_builtin_ReflectionMethod_setAccessible(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 6335 | `{` |
|    ! 0 | 6336 | `	SXUNUSED(pCtx);` |
|    ! 0 | 6337 | `	SXUNUSED(nArg);` |
|    ! 0 | 6338 | `	SXUNUSED(apArg);` |
|    ! 0 | 6339 | `	return PH7_OK;` |
|    ! 0 | 6340 | `}` |
|      - | 6341 | `/*` |
|      - | 6342 | ` * The class this method OVERRIDES it from: the nearest base declaring a` |
|      - | 6343 | ` * same-named non-private method, else the first interface that declares one.` |
|      - | 6344 | ` */` |
|    254 | 6345 | `static ph7_class * ReflectPrototypeIn(ph7_context *pCtx, ph7_class *pClass,` |
|      - | 6346 | `	const char *zName, int nName, int bIfaceToo)` |
|      1 | 6347 | `{` |
|      - | 6348 | `	ph7_class *pWalk;` |
|    255 | 6349 | `	int iDepth = 0;` |
|    255 | 6350 | `	if( pClass == 0 \|\| nName < 1 ){` |
|    ! 0 | 6351 | `		return 0;` |
|      - | 6352 | `	}` |
|    323 | 6353 | `	for( pWalk = pClass->pBase ; pWalk && iDepth <= REFLECT_WALK_MAX_DEPTH ; pWalk = pWalk->pBase ){` |
|     93 | 6354 | `		SyHashEntry *pEntry = ReflectFindMethodEntry(pWalk, zName, nName);` |
|     93 | 6355 | `		if( pEntry ){` |
|     25 | 6356 | `			ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     25 | 6357 | `			if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|     25 | 6358 | `				return ReflectMethodDeclClass(pWalk, pMeth);` |
|      - | 6359 | `			}` |
|    ! 0 | 6360 | `		}` |
|     69 | 6361 | `		iDepth++;` |
|     35 | 6362 | `	}` |
|    231 | 6363 | `	if( !bIfaceToo ){` |
|      - | 6364 | ``		/* php's `overwrites` tag asks only about the PARENT CHAIN: a method that`` |
|      - | 6365 | ``		 * first appears in an interface is a `prototype`, never an overwrite. */`` |
|     91 | 6366 | `		return 0;` |
|      - | 6367 | `	}` |
|      - | 6368 | `	{` |
|      - | 6369 | `		SySet aSet;` |
|      - | 6370 | `		ph7_class **apIface;` |
|    141 | 6371 | `		ph7_class *pFound = 0;` |
|      - | 6372 | `		sxu32 n;` |
|    141 | 6373 | `		SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|    141 | 6374 | `		ReflectInterfacesOf(pClass, &aSet);` |
|    141 | 6375 | `		apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|    249 | 6376 | `		for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|    167 | 6377 | `			if( ReflectFindMethodEntry(apIface[n], zName, nName) ){` |
|     59 | 6378 | `				pFound = apIface[n];` |
|     59 | 6379 | `				break;` |
|      - | 6380 | `			}` |
|     55 | 6381 | `		}` |
|    141 | 6382 | `		SySetRelease(&aSet);` |
|    141 | 6383 | `		return pFound;` |
|      - | 6384 | `	}` |
|    128 | 6385 | `}` |
|      - | 6386 | `/* The same question asked by a ReflectionMethod receiver, which carries the` |
|      - | 6387 | `` * method name on `$this`. */`` |
|     48 | 6388 | `static ph7_class * ReflectMethodPrototype(ph7_context *pCtx, ReflectFuncRef *pRef)` |
|      1 | 6389 | `{` |
|     49 | 6390 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     49 | 6391 | `	const char *zName = "";` |
|     49 | 6392 | `	int nName = 0;` |
|     49 | 6393 | `	if( pThis == 0 ){` |
|    ! 0 | 6394 | `		return 0;` |
|      - | 6395 | `	}` |
|     49 | 6396 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     49 | 6397 | `	return ReflectPrototypeIn(pCtx, pRef->pClass, zName, nName, 1);` |
|     25 | 6398 | `}` |
|     22 | 6399 | `static int vm_builtin_ReflectionMethod_hasPrototype(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6400 | `{` |
|      - | 6401 | `	ReflectFuncRef sRef;` |
|     11 | 6402 | `	SXUNUSED(nArg);` |
|     11 | 6403 | `	SXUNUSED(apArg);` |
|     23 | 6404 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 | 6405 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6406 | `		return PH7_OK;` |
|      - | 6407 | `	}` |
|     23 | 6408 | `	ph7_result_bool(pCtx, ReflectMethodPrototype(pCtx, &sRef) != 0);` |
|     23 | 6409 | `	return PH7_OK;` |
|     12 | 6410 | `}` |
|     26 | 6411 | `static int vm_builtin_ReflectionMethod_getPrototype(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6412 | `{` |
|     27 | 6413 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 6414 | `	ReflectFuncRef sRef;` |
|      - | 6415 | `	ph7_class *pProto;` |
|     27 | 6416 | `	const char *zClass = "", *zName = "";` |
|     27 | 6417 | `	int nClass = 0, nName = 0;` |
|     13 | 6418 | `	SXUNUSED(nArg);` |
|     13 | 6419 | `	SXUNUSED(apArg);` |
|     27 | 6420 | `	if( pThis == 0 \|\| !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 | 6421 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6422 | `		return PH7_OK;` |
|      - | 6423 | `	}` |
|     27 | 6424 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     27 | 6425 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     27 | 6426 | `	pProto = ReflectMethodPrototype(pCtx, &sRef);` |
|     27 | 6427 | `	if( pProto == 0 ){` |
|      7 | 6428 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 | 6429 | `			"Method %.*s::%.*s does not have a prototype", nClass, zClass, nName, zName);` |
|      - | 6430 | `	}` |
|      - | 6431 | `	{` |
|      - | 6432 | `		ph7_value sClass, sName;` |
|      - | 6433 | `		ph7_value *apCtor[2];` |
|      - | 6434 | `		ph7_class_instance *pOut;` |
|      - | 6435 | `		sxi32 rc;` |
|     23 | 6436 | `		PH7_MemObjInit(pCtx->pVm, &sClass);` |
|     23 | 6437 | `		PH7_MemObjInit(pCtx->pVm, &sName);` |
|     23 | 6438 | `		ph7_value_string(&sClass, SyStringData(&pProto->sName), (int)SyStringLength(&pProto->sName));` |
|     23 | 6439 | `		ph7_value_string(&sName, zName, nName);` |
|     23 | 6440 | `		apCtor[0] = &sClass;` |
|     23 | 6441 | `		apCtor[1] = &sName;` |
|     23 | 6442 | `		pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|     23 | 6443 | `		PH7_MemObjRelease(&sClass);` |
|     23 | 6444 | `		PH7_MemObjRelease(&sName);` |
|     23 | 6445 | `		if( pOut == 0 ){` |
|    ! 0 | 6446 | `			if( rc != PH7_OK ){` |
|    ! 0 | 6447 | `				return rc;` |
|      - | 6448 | `			}` |
|    ! 0 | 6449 | `			ph7_result_null(pCtx);` |
|    ! 0 | 6450 | `			return PH7_OK;` |
|      - | 6451 | `		}` |
|     23 | 6452 | `		return ReflectResultObject(pCtx, pOut);` |
|      - | 6453 | `	}` |
|     14 | 6454 | `}` |
|      - | 6455 | `/* ---- ReflectionParameter ---- */` |
|      - | 6456 | `/*` |
|      - | 6457 | ``  * The function a ReflectionParameter belongs to, from its own `__t`/`__m` `` |
|      - | 6458 | `` * slots. `__t` holds a Closure, a class name or a function name; `__m` the`` |
|      - | 6459 | ` * method name, or null.` |
|      - | 6460 | ` */` |
|   1500 | 6461 | `static int ReflectParamOwner(ph7_context *pCtx, ReflectFuncRef *pRef, ReflectParamDesc *pDesc)` |
|      3 | 6462 | `{` |
|   1503 | 6463 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 6464 | `	ph7_value *pT, *pM;` |
|      - | 6465 | `	int rc;` |
|   1503 | 6466 | `	SyZero(pRef, sizeof(*pRef));` |
|   1503 | 6467 | `	if( pDesc ){` |
|   1447 | 6468 | `		SyZero(pDesc, sizeof(*pDesc));` |
|    722 | 6469 | `	}` |
|   1503 | 6470 | `	if( pThis == 0 ){` |
|    ! 0 | 6471 | `		return 0;` |
|      - | 6472 | `	}` |
|   1503 | 6473 | `	pT = PH7_NativeAttr(pThis, RP_T);` |
|   1503 | 6474 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|   1503 | 6475 | `	if( pT == 0 ){` |
|    ! 0 | 6476 | `		return 0;` |
|      - | 6477 | `	}` |
|   1503 | 6478 | `	rc = ReflectFuncFill(pCtx->pVm, pT, (pM && (pM->iFlags & MEMOBJ_STRING)) ? pM : 0, pRef);` |
|   1503 | 6479 | `	if( rc == 0 \|\| pDesc == 0 ){` |
|     57 | 6480 | `		return rc;` |
|      - | 6481 | `	}` |
|   1447 | 6482 | `	return ReflectParamAt(pRef, (int)PH7_NativeAttrInt(pThis, RP_P), pDesc);` |
|    753 | 6483 | `}` |
|      - | 6484 | `/* ReflectionParameter::__construct($function, string\|int $param) */` |
|    822 | 6485 | `static int vm_builtin_ReflectionParameter_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 | 6486 | `{` |
|    826 | 6487 | `	ph7_vm *pVm = pCtx->pVm;` |
|    826 | 6488 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 6489 | `	ReflectFuncRef sRef;` |
|      - | 6490 | `	ReflectParamDesc sDesc;` |
|      - | 6491 | `	ph7_value sTarget, sMethod;` |
|    826 | 6492 | `	int nTotal, iFound = -1, rc = PH7_OK;` |
|    826 | 6493 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 | 6494 | `		return PH7_OK;` |
|      - | 6495 | `	}` |
|    826 | 6496 | `	SyZero(&sDesc, sizeof(sDesc));` |
|    826 | 6497 | `	PH7_MemObjInit(pVm, &sTarget);` |
|    826 | 6498 | `	PH7_MemObjInit(pVm, &sMethod);` |
|      - | 6499 | ``	/* `[$objOrClass, $method]`, `"C::m"` and a plain function/Closure are the`` |
|      - | 6500 | `	 * three spellings php accepts. */` |
|    826 | 6501 | `	if( ph7_value_is_array(apArg[0]) ){` |
|    271 | 6502 | `		ph7_value *pA = ph7_array_fetch(apArg[0], "0", 1);` |
|    271 | 6503 | `		ph7_value *pB = ph7_array_fetch(apArg[0], "1", 1);` |
|    271 | 6504 | `		if( pA && (pA->iFlags & MEMOBJ_OBJ) ){` |
|    ! 0 | 6505 | `			ph7_class_instance *pObj = (ph7_class_instance *)pA->x.pOther;` |
|    ! 0 | 6506 | `			ph7_value_string(&sTarget, SyStringData(&pObj->pClass->sName),` |
|    ! 0 | 6507 | `				(int)SyStringLength(&pObj->pClass->sName));` |
|    271 | 6508 | `		}else if( pA ){` |
|    271 | 6509 | `			PH7_MemObjStore(pA, &sTarget);` |
|    134 | 6510 | `		}` |
|    271 | 6511 | `		if( pB ){` |
|    271 | 6512 | `			PH7_MemObjStore(pB, &sMethod);` |
|    134 | 6513 | `		}` |
|    137 | 6514 | `	}else{` |
|      - | 6515 | ``		/* A STRING is a plain function name, `"C::m"` included: php does not`` |
|      - | 6516 | ``		 * split one here (it reports `Function C::m() does not exist`), unlike`` |
|      - | 6517 | `		 * ReflectionMethod's own one-argument form. The prelude split it and` |
|      - | 6518 | `		 * silently reflected the method. */` |
|    556 | 6519 | `		PH7_MemObjStore(apArg[0], &sTarget);` |
|      - | 6520 | `	}` |
|    826 | 6521 | `	if( !ReflectFuncFill(pCtx->pVm, &sTarget, (sMethod.iFlags & MEMOBJ_STRING) ? &sMethod : 0, &sRef) ){` |
|      - | 6522 | `		const char *zName;` |
|      - | 6523 | `		int nName;` |
|      3 | 6524 | `		if( sMethod.iFlags & MEMOBJ_STRING ){` |
|      - | 6525 | `			const char *zM;` |
|      - | 6526 | `			int nM;` |
|    ! 0 | 6527 | `			zName = ph7_value_to_string(&sTarget, &nName);` |
|    ! 0 | 6528 | `			zM = ph7_value_to_string(&sMethod, &nM);` |
|    ! 0 | 6529 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 | 6530 | `				"Method %.*s::%.*s() does not exist", nName, zName, nM, zM);` |
|    ! 0 | 6531 | `		}else{` |
|      3 | 6532 | `			zName = ph7_value_to_string(&sTarget, &nName);` |
|      4 | 6533 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 6534 | `				"Function %.*s() does not exist", nName, zName);` |
|      - | 6535 | `		}` |
|      3 | 6536 | `		goto Done;` |
|      - | 6537 | `	}` |
|    824 | 6538 | `	nTotal = ReflectParamCount(&sRef);` |
|    824 | 6539 | `	if( apArg[1]->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|    814 | 6540 | `		int iWant = (int)ph7_value_to_int(apArg[1]);` |
|    814 | 6541 | `		if( iWant >= 0 && iWant < nTotal && ReflectParamAt(&sRef, iWant, &sDesc) ){` |
|    812 | 6542 | `			iFound = iWant;` |
|    404 | 6543 | `		}` |
|    814 | 6544 | `		if( iFound < 0 ){` |
|      3 | 6545 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 6546 | `				"The parameter specified by its offset could not be found");` |
|      3 | 6547 | `			goto Done;` |
|      - | 6548 | `		}` |
|    408 | 6549 | `	}else{` |
|      - | 6550 | `		const char *zWant;` |
|      - | 6551 | `		int nWant, n;` |
|     11 | 6552 | `		zWant = ph7_value_to_string(apArg[1], &nWant);` |
|     33 | 6553 | `		for( n = 0 ; n < nTotal ; n++ ){` |
|     30 | 6554 | `			if( ReflectParamAt(&sRef, n, &sDesc)` |
|     30 | 6555 | `			 && SyStringLength(&sDesc.sName) == (sxu32)nWant` |
|     23 | 6556 | `			 && SyMemcmp(SyStringData(&sDesc.sName), zWant, (sxu32)nWant) == 0 ){` |
|      9 | 6557 | `				iFound = n;` |
|      9 | 6558 | `				break;` |
|      - | 6559 | `			}` |
|     12 | 6560 | `		}` |
|     11 | 6561 | `		if( iFound < 0 ){` |
|      3 | 6562 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 6563 | `				"The parameter specified by its name could not be found");` |
|      3 | 6564 | `			goto Done;` |
|      - | 6565 | `		}` |
|      - | 6566 | `	}` |
|   1228 | 6567 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sDesc.sName),` |
|    816 | 6568 | `		(int)SyStringLength(&sDesc.sName));` |
|      - | 6569 | `	/* Record the CANONICAL target: the class the method really came through and` |
|      - | 6570 | `	 * its declared spelling, so a re-resolve cannot land somewhere else. */` |
|    820 | 6571 | `	if( sRef.pMeth && sRef.pClass ){` |
|    414 | 6572 | `		PH7_NativeSetAttrStr(pVm, pThis, RP_T, SyStringData(&sRef.pClass->sName),` |
|    274 | 6573 | `			(int)SyStringLength(&sRef.pClass->sName));` |
|    414 | 6574 | `		PH7_NativeSetAttrStr(pVm, pThis, RP_M, SyStringData(&sRef.pMeth->sFunc.sName),` |
|    274 | 6575 | `			(int)SyStringLength(&sRef.pMeth->sFunc.sName));` |
|    140 | 6576 | `	}else{` |
|    544 | 6577 | `		ph7_value *pSlot = PH7_NativeAttr(pThis, RP_T);` |
|    544 | 6578 | `		if( pSlot ){` |
|    544 | 6579 | `			PH7_MemObjStore(&sTarget, pSlot);` |
|    271 | 6580 | `		}` |
|      - | 6581 | `	}` |
|    820 | 6582 | `	PH7_NativeSetAttrInt(pVm, pThis, RP_P, (sxi64)iFound);` |
|    411 | 6583 | `Done:` |
|    826 | 6584 | `	PH7_MemObjRelease(&sTarget);` |
|    826 | 6585 | `	PH7_MemObjRelease(&sMethod);` |
|    826 | 6586 | `	return rc;` |
|    415 | 6587 | `}` |
|    646 | 6588 | `static int vm_builtin_ReflectionParameter_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 6589 | `{` |
|    648 | 6590 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    648 | 6591 | `	const char *zName = "";` |
|    648 | 6592 | `	int nName = 0;` |
|    323 | 6593 | `	SXUNUSED(nArg);` |
|    323 | 6594 | `	SXUNUSED(apArg);` |
|    648 | 6595 | `	if( pThis ){` |
|    648 | 6596 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    323 | 6597 | `	}` |
|    648 | 6598 | `	ph7_result_string(pCtx, zName, nName);` |
|    648 | 6599 | `	return PH7_OK;` |
|      2 | 6600 | `}` |
|     36 | 6601 | `static int vm_builtin_ReflectionParameter_getPosition(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6602 | `{` |
|     37 | 6603 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     18 | 6604 | `	SXUNUSED(nArg);` |
|     18 | 6605 | `	SXUNUSED(apArg);` |
|     37 | 6606 | `	ph7_result_int64(pCtx, pThis ? PH7_NativeAttrInt(pThis, RP_P) : 0);` |
|     37 | 6607 | `	return PH7_OK;` |
|      1 | 6608 | `}` |
|      - | 6609 | `/* The boolean predicates that read one flag of the description. */` |
|    534 | 6610 | `static int ReflectParamFlag(ph7_context *pCtx, int iWhat)` |
|      1 | 6611 | `{` |
|      - | 6612 | `	ReflectFuncRef sRef;` |
|      - | 6613 | `	ReflectParamDesc sDesc;` |
|    535 | 6614 | `	int bYes = 0;` |
|    535 | 6615 | `	if( ReflectParamOwner(pCtx, &sRef, &sDesc) ){` |
|    535 | 6616 | `		switch( iWhat ){` |
|     25 | 6617 | `		case 0: bYes = sDesc.bByRef; break;` |
|      3 | 6618 | `		case 1: bYes = !sDesc.bByRef; break;` |
|     45 | 6619 | `		case 2: bYes = sDesc.bVariadic; break;` |
|    ! 0 | 6620 | `		case 3: bYes = sDesc.bPromoted; break;` |
|    411 | 6621 | `		case 4: bYes = sDesc.bHasDef; break;` |
|     45 | 6622 | `		case 5: bYes = SyStringLength(&sDesc.sType) > 0; break;` |
|      5 | 6623 | `		default:` |
|      - | 6624 | `			/* allowsNull(): untyped, nullable, or a type that INCLUDES null */` |
|     13 | 6625 | `			bYes = SyStringLength(&sDesc.sType) < 1 \|\| sDesc.bNullable` |
|      7 | 6626 | `				\|\| ReflectTypeNameIs(SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType), "mixed")` |
|     14 | 6627 | `				\|\| ReflectTypeNameIs(SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType), "null");` |
|     10 | 6628 | `			break;` |
|      - | 6629 | `		}` |
|    268 | 6630 | `	}else if( iWhat == 1 \|\| iWhat == 6 ){` |
|    ! 0 | 6631 | `		bYes = 1;` |
|    ! 0 | 6632 | `	}` |
|    535 | 6633 | `	ph7_result_bool(pCtx, bYes);` |
|    535 | 6634 | `	return PH7_OK;` |
|      1 | 6635 | `}` |
|      - | 6636 | `#define REFLECT_PARAM_FLAG(NAME,WHAT) \` |
|      - | 6637 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 6638 | `	{ \` |
|      - | 6639 | `		SXUNUSED(nArg); \` |
|      - | 6640 | `		SXUNUSED(apArg); \` |
|      - | 6641 | `		return ReflectParamFlag(pCtx, WHAT); \` |
|      - | 6642 | `	}` |
|     25 | 6643 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isPassedByReference, 0)` |
|      3 | 6644 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_canBePassedByValue, 1)` |
|     45 | 6645 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isVariadic, 2)` |
|    ! 0 | 6646 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isPromoted, 3)` |
|    411 | 6647 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isDefaultValueAvailable, 4)` |
|     45 | 6648 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_hasType, 5)` |
|     11 | 6649 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_allowsNull, 6)` |
|      - | 6650 |  |
|      - | 6651 | `/* isOptional(): every parameter from here on has to be omissible too. */` |
|     52 | 6652 | `static int vm_builtin_ReflectionParameter_isOptional(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6653 | `{` |
|     53 | 6654 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 6655 | `	ReflectFuncRef sRef;` |
|      - | 6656 | `	ReflectParamDesc sDesc;` |
|      - | 6657 | `	int n, nTotal, iPos;` |
|     26 | 6658 | `	SXUNUSED(nArg);` |
|     26 | 6659 | `	SXUNUSED(apArg);` |
|     53 | 6660 | `	if( pThis == 0 \|\| !ReflectParamOwner(pCtx, &sRef, 0) ){` |
|    ! 0 | 6661 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6662 | `		return PH7_OK;` |
|      - | 6663 | `	}` |
|     53 | 6664 | `	iPos = (int)PH7_NativeAttrInt(pThis, RP_P);` |
|     53 | 6665 | `	nTotal = ReflectParamCount(&sRef);` |
|    101 | 6666 | `	for( n = iPos ; n < nTotal ; n++ ){` |
|     71 | 6667 | `		if( ReflectParamAt(&sRef, n, &sDesc) && !sDesc.bVariadic && !sDesc.bOptional ){` |
|     23 | 6668 | `			ph7_result_bool(pCtx, 0);` |
|     23 | 6669 | `			return PH7_OK;` |
|      - | 6670 | `		}` |
|     25 | 6671 | `	}` |
|     31 | 6672 | `	ph7_result_bool(pCtx, 1);` |
|     31 | 6673 | `	return PH7_OK;` |
|     27 | 6674 | `}` |
|    684 | 6675 | `static int vm_builtin_ReflectionParameter_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 6676 | `{` |
|      - | 6677 | `	ReflectFuncRef sRef;` |
|      - | 6678 | `	ReflectParamDesc sDesc;` |
|    342 | 6679 | `	SXUNUSED(nArg);` |
|    342 | 6680 | `	SXUNUSED(apArg);` |
|    687 | 6681 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| SyStringLength(&sDesc.sType) < 1 ){` |
|      3 | 6682 | `		ph7_result_null(pCtx);` |
|      3 | 6683 | `		return PH7_OK;` |
|      - | 6684 | `	}` |
|   1026 | 6685 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx,` |
|    682 | 6686 | `		SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType)));` |
|    345 | 6687 | `}` |
|      - | 6688 | `/*` |
|      - | 6689 | ` * getClass()/isArray()/isCallable() — php 8.0 deprecated these in favour of` |
|      - | 6690 | ` * getType() but still declares them, still answers, and emits an E_DEPRECATED` |
|      - | 6691 | ` * naming the replacement. PHL fatalled on the call; all three are here now,` |
|      - | 6692 | ` * notice included.` |
|      - | 6693 | ` */` |
|     24 | 6694 | `static void ReflectParamDeprecated(ph7_context *pCtx, const char *zWho)` |
|      1 | 6695 | `{` |
|      - | 6696 | `	char zMsg[160];` |
|     37 | 6697 | `	SyBufferFormat(zMsg, sizeof(zMsg),` |
|      - | 6698 | `		"Method ReflectionParameter::%s() is deprecated since 8.0, "` |
|     12 | 6699 | `		"use ReflectionParameter::getType() instead", zWho);` |
|     25 | 6700 | `	PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED, zMsg);` |
|     25 | 6701 | `}` |
|      8 | 6702 | `static int vm_builtin_ReflectionParameter_getClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6703 | `{` |
|      - | 6704 | `	ReflectFuncRef sRef;` |
|      - | 6705 | `	ReflectParamDesc sDesc;` |
|      - | 6706 | `	const char *zType;` |
|      - | 6707 | `	int nType;` |
|      4 | 6708 | `	SXUNUSED(nArg);` |
|      4 | 6709 | `	SXUNUSED(apArg);` |
|      9 | 6710 | `	ReflectParamDeprecated(pCtx, "getClass");` |
|      9 | 6711 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| SyStringLength(&sDesc.sType) < 1 ){` |
|    ! 0 | 6712 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6713 | `		return PH7_OK;` |
|      - | 6714 | `	}` |
|      9 | 6715 | `	zType = SyStringData(&sDesc.sType);` |
|      9 | 6716 | `	nType = (int)SyStringLength(&sDesc.sType);` |
|      9 | 6717 | `	if( nType > 0 && zType[0] == '?' ){` |
|      3 | 6718 | `		zType++;` |
|      3 | 6719 | `		nType--;` |
|      1 | 6720 | `	}` |
|      9 | 6721 | `	if( ReflectTypeIsBuiltin(zType, nType) ){` |
|      7 | 6722 | `		ph7_result_null(pCtx);` |
|      7 | 6723 | `		return PH7_OK;` |
|      - | 6724 | `	}` |
|      3 | 6725 | `	return ReflectResultClassOf(pCtx, PH7_VmExtractClass(pCtx->pVm, zType, (sxu32)nType, FALSE, 0));` |
|      5 | 6726 | `}` |
|     16 | 6727 | `static int ReflectParamTypeIs(ph7_context *pCtx, const char *zWant)` |
|      1 | 6728 | `{` |
|      - | 6729 | `	ReflectFuncRef sRef;` |
|      - | 6730 | `	ReflectParamDesc sDesc;` |
|     17 | 6731 | `	int bYes = 0;` |
|     17 | 6732 | `	if( ReflectParamOwner(pCtx, &sRef, &sDesc) && SyStringLength(&sDesc.sType) > 0 ){` |
|     17 | 6733 | `		const char *zType = SyStringData(&sDesc.sType);` |
|     17 | 6734 | `		int nType = (int)SyStringLength(&sDesc.sType);` |
|      - | 6735 | ``		/* php answers true for `?array` too: the deprecated pair asks about the`` |
|      - | 6736 | `		 * type ATOM, and nullability is a separate question. */` |
|     17 | 6737 | `		if( zType[0] == '?' ){` |
|      5 | 6738 | `			zType++;` |
|      5 | 6739 | `			nType--;` |
|      2 | 6740 | `		}` |
|     17 | 6741 | `		bYes = ReflectTypeNameIs(zType, nType, zWant);` |
|      8 | 6742 | `	}` |
|     17 | 6743 | `	ph7_result_bool(pCtx, bYes);` |
|     17 | 6744 | `	return PH7_OK;` |
|      1 | 6745 | `}` |
|      8 | 6746 | `static int vm_builtin_ReflectionParameter_isArray(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6747 | `{` |
|      4 | 6748 | `	SXUNUSED(nArg);` |
|      4 | 6749 | `	SXUNUSED(apArg);` |
|      9 | 6750 | `	ReflectParamDeprecated(pCtx, "isArray");` |
|      9 | 6751 | `	return ReflectParamTypeIs(pCtx, "array");` |
|      1 | 6752 | `}` |
|      8 | 6753 | `static int vm_builtin_ReflectionParameter_isCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6754 | `{` |
|      4 | 6755 | `	SXUNUSED(nArg);` |
|      4 | 6756 | `	SXUNUSED(apArg);` |
|      9 | 6757 | `	ReflectParamDeprecated(pCtx, "isCallable");` |
|      9 | 6758 | `	return ReflectParamTypeIs(pCtx, "callable");` |
|      1 | 6759 | `}` |
|    142 | 6760 | `static int vm_builtin_ReflectionParameter_getDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6761 | `{` |
|      - | 6762 | `	ReflectFuncRef sRef;` |
|      - | 6763 | `	ReflectParamDesc sDesc;` |
|     71 | 6764 | `	SXUNUSED(nArg);` |
|     71 | 6765 | `	SXUNUSED(apArg);` |
|    143 | 6766 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|      5 | 6767 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 6768 | `			"Internal error: Failed to retrieve the default value");` |
|      - | 6769 | `	}` |
|    139 | 6770 | `	if( sDesc.pArg ){` |
|      - | 6771 | `		/* Compiled: the same evaluation path the VM uses for an omitted argument */` |
|      - | 6772 | `		ph7_value sValue;` |
|      9 | 6773 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|      9 | 6774 | `		VmLocalExec(pCtx->pVm, &sDesc.pArg->aByteCode, &sValue, FALSE);` |
|      9 | 6775 | `		ph7_result_value(pCtx, &sValue);` |
|      9 | 6776 | `		PH7_MemObjRelease(&sValue);` |
|      9 | 6777 | `		return PH7_OK;` |
|      - | 6778 | `	}` |
|      - | 6779 | `	{` |
|      - | 6780 | `		/* Declared: the signature's default TEXT, reduced. */` |
|    131 | 6781 | `		const char *zDef = SyStringData(&sDesc.sDefText);` |
|    131 | 6782 | `		int nDef = (int)SyStringLength(&sDesc.sDefText);` |
|    131 | 6783 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    131 | 6784 | `		if( pVal && ReflectSigScalar(pCtx, zDef, nDef, pVal) ){` |
|    129 | 6785 | `			ph7_result_value(pCtx, pVal);` |
|    129 | 6786 | `			return PH7_OK;` |
|      - | 6787 | `		}` |
|      2 | 6788 | `		if( (nDef >= 1 && zDef[0] == '[')` |
|      2 | 6789 | `		 \|\| (nDef >= (int)sizeof("array (")-1` |
|    ! 0 | 6790 | `		  && SyMemcmp(zDef, "array (", sizeof("array (")-1) == 0) ){` |
|      3 | 6791 | `			ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|      3 | 6792 | `			return PH7_OK;` |
|      - | 6793 | `		}` |
|      - | 6794 | `	}` |
|    ! 0 | 6795 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 6796 | `		"Internal error: Failed to retrieve the default value");` |
|     72 | 6797 | `}` |
|      - | 6798 | `/*` |
|      - | 6799 | ` * A default that is a plain global-constant reference compiles to exactly` |
|      - | 6800 | ` * [ OP_LOADC (EXPAND), OP_DONE ] with the name in the literal table; a` |
|      - | 6801 | ` * DECLARED default is text and never a constant.` |
|      - | 6802 | ` */` |
|    102 | 6803 | `static int ReflectParamDefConst(ph7_context *pCtx, ReflectParamDesc *pDesc,` |
|      - | 6804 | `	const char **pz, int *pn)` |
|      1 | 6805 | `{` |
|      - | 6806 | `	VmInstr *aInstr;` |
|      - | 6807 | `	ph7_value *pLit;` |
|    103 | 6808 | `	if( pDesc->pArg == 0 \|\| SySetUsed(&pDesc->pArg->aByteCode) != 2 ){` |
|     47 | 6809 | `		return 0;` |
|      - | 6810 | `	}` |
|     57 | 6811 | `	aInstr = (VmInstr *)SySetBasePtr(&pDesc->pArg->aByteCode);` |
|     56 | 6812 | `	if( aInstr[0].iOp != PH7_OP_LOADC \|\| (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0` |
|     36 | 6813 | `	 \|\| aInstr[1].iOp != PH7_OP_DONE ){` |
|     43 | 6814 | `		return 0;` |
|      - | 6815 | `	}` |
|     15 | 6816 | `	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);` |
|     15 | 6817 | `	if( pLit == 0 \|\| SyBlobLength(&pLit->sBlob) < 1 ){` |
|    ! 0 | 6818 | `		return 0;` |
|      - | 6819 | `	}` |
|     15 | 6820 | `	*pz = (const char *)SyBlobData(&pLit->sBlob);` |
|     15 | 6821 | `	*pn = (int)SyBlobLength(&pLit->sBlob);` |
|     15 | 6822 | `	return 1;` |
|     52 | 6823 | `}` |
|     40 | 6824 | `static int vm_builtin_ReflectionParameter_isDefaultValueConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6825 | `{` |
|      - | 6826 | `	ReflectFuncRef sRef;` |
|      - | 6827 | `	ReflectParamDesc sDesc;` |
|      - | 6828 | `	const char *z;` |
|      - | 6829 | `	int n;` |
|     20 | 6830 | `	SXUNUSED(nArg);` |
|     20 | 6831 | `	SXUNUSED(apArg);` |
|     41 | 6832 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|      - | 6833 | `		/* php raises here rather than answering false: asking whether a default` |
|      - | 6834 | `		 * is a constant presupposes there IS one. The prelude answered false. */` |
|      3 | 6835 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 6836 | `			"Internal error: Failed to retrieve the default value");` |
|      - | 6837 | `	}` |
|     39 | 6838 | `	ph7_result_bool(pCtx, ReflectParamDefConst(pCtx, &sDesc, &z, &n) != 0);` |
|     39 | 6839 | `	return PH7_OK;` |
|     21 | 6840 | `}` |
|      4 | 6841 | `static int vm_builtin_ReflectionParameter_getDefaultValueConstantName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6842 | `{` |
|      - | 6843 | `	ReflectFuncRef sRef;` |
|      - | 6844 | `	ReflectParamDesc sDesc;` |
|      - | 6845 | `	const char *z;` |
|      - | 6846 | `	int n;` |
|      2 | 6847 | `	SXUNUSED(nArg);` |
|      2 | 6848 | `	SXUNUSED(apArg);` |
|      5 | 6849 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|    ! 0 | 6850 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 6851 | `			"Internal error: Failed to retrieve the default value");` |
|      - | 6852 | `	}` |
|      5 | 6853 | `	if( ReflectParamDefConst(pCtx, &sDesc, &z, &n) ){` |
|      5 | 6854 | `		ph7_result_string(pCtx, z, n);` |
|      3 | 6855 | `	}else{` |
|    ! 0 | 6856 | `		ph7_result_null(pCtx);` |
|      - | 6857 | `	}` |
|      5 | 6858 | `	return PH7_OK;` |
|      3 | 6859 | `}` |
|      4 | 6860 | `static int vm_builtin_ReflectionParameter_getDeclaringFunction(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6861 | `{` |
|      5 | 6862 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 6863 | `	ph7_value *pT, *pM;` |
|      - | 6864 | `	ph7_value *apCtor[2];` |
|      - | 6865 | `	ph7_class_instance *pOut;` |
|      - | 6866 | `	sxi32 rc;` |
|      2 | 6867 | `	SXUNUSED(nArg);` |
|      2 | 6868 | `	SXUNUSED(apArg);` |
|      5 | 6869 | `	if( pThis == 0 \|\| (pT = PH7_NativeAttr(pThis, RP_T)) == 0 ){` |
|    ! 0 | 6870 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6871 | `		return PH7_OK;` |
|      - | 6872 | `	}` |
|      5 | 6873 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|      5 | 6874 | `	apCtor[0] = pT;` |
|      5 | 6875 | `	apCtor[1] = pM;` |
|      5 | 6876 | `	if( pM && (pM->iFlags & MEMOBJ_STRING) ){` |
|      3 | 6877 | `		pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|      2 | 6878 | `	}else{` |
|      3 | 6879 | `		pOut = ReflectConstruct(pCtx, "ReflectionFunction", 1, apCtor, &rc);` |
|      - | 6880 | `	}` |
|      5 | 6881 | `	if( pOut == 0 ){` |
|    ! 0 | 6882 | `		if( rc != PH7_OK ){` |
|    ! 0 | 6883 | `			return rc;` |
|      - | 6884 | `		}` |
|    ! 0 | 6885 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6886 | `		return PH7_OK;` |
|      - | 6887 | `	}` |
|      5 | 6888 | `	return ReflectResultObject(pCtx, pOut);` |
|      3 | 6889 | `}` |
|      4 | 6890 | `static int vm_builtin_ReflectionParameter_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6891 | `{` |
|      - | 6892 | `	ReflectFuncRef sRef;` |
|      2 | 6893 | `	SXUNUSED(nArg);` |
|      2 | 6894 | `	SXUNUSED(apArg);` |
|      5 | 6895 | `	if( !ReflectParamOwner(pCtx, &sRef, 0) \|\| sRef.pMeth == 0 ){` |
|      3 | 6896 | `		ph7_result_null(pCtx);` |
|      3 | 6897 | `		return PH7_OK;` |
|      - | 6898 | `	}` |
|      3 | 6899 | `	return ReflectResultClassOf(pCtx, ReflectFuncDeclClass(&sRef));` |
|      3 | 6900 | `}` |
|      6 | 6901 | `static int vm_builtin_ReflectionParameter_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6902 | `{` |
|      7 | 6903 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 6904 | `	ReflectFuncRef sRef;` |
|      - | 6905 | `	ReflectParamDesc sDesc;` |
|      - | 6906 | `	ph7_value *pT, *pM;` |
|      7 | 6907 | `	const char *zMember = 0;` |
|      7 | 6908 | `	int nMember = 0;` |
|      7 | 6909 | `	if( pThis == 0 \|\| !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| sDesc.pArg == 0 ){` |
|    ! 0 | 6910 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 | 6911 | `		return PH7_OK;` |
|      - | 6912 | `	}` |
|      7 | 6913 | `	pT = PH7_NativeAttr(pThis, RP_T);` |
|      7 | 6914 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|      7 | 6915 | `	if( pM && (pM->iFlags & MEMOBJ_STRING) ){` |
|      5 | 6916 | `		zMember = (const char *)SyBlobData(&pM->sBlob);` |
|      5 | 6917 | `		nMember = (int)SyBlobLength(&pM->sBlob);` |
|      2 | 6918 | `	}` |
|      - | 6919 | `	/* 32 = Attribute::TARGET_PARAMETER */` |
|     10 | 6920 | `	return ReflectBuildAttrs(pCtx, &sDesc.pArg->aAttrs, "param", pT, zMember, nMember,` |
|      6 | 6921 | `		(int)PH7_NativeAttrInt(pThis, RP_P), 32, nArg, apArg);` |
|      4 | 6922 | `}` |
|     10 | 6923 | `static int vm_builtin_ReflectionParameter_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6924 | `{` |
|      5 | 6925 | `	SXUNUSED(nArg);` |
|      5 | 6926 | `	SXUNUSED(apArg);` |
|     11 | 6927 | `	return ReflectExportParamSelf(pCtx);` |
|      1 | 6928 | `}` |
|      - | 6929 | `/*` |
|      - | 6930 | ` * Declare chunk 2. Called from PH7_VmInstallReflection() where it used to be` |
|      - | 6931 | `` * compiled — after chunk 1, whose `Reflector` these implement and whose`` |
|      - | 6932 | `` * `ReflectionClass` they answer.`` |
|      - | 6933 | ` *` |
|      - | 6934 | ` * The method tables are in php's own DECLARATION order.` |
|      - | 6935 | ` */` |
|      - | 6936 | `/*` |
|      - | 6937 | `` * PropertyHookType — php's `enum PropertyHookType: string { Get; Set; }`, the`` |
|      - | 6938 | ` * argument ReflectionProperty::getHook()/hasHook() take.` |
|      - | 6939 | ` */` |
|   5740 | 6940 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionHookType(ph7_vm *pVm)` |
|      5 | 6941 | `{` |
|      - | 6942 | `	static const PH7_NativeEnumCase aCase[] = {` |
|      - | 6943 | `		{ "Get", { 0, 0, PH7_NATIVE_VAL_STRING, 0, "get", 0.0 } },` |
|      - | 6944 | `		{ "Set", { 0, 0, PH7_NATIVE_VAL_STRING, 0, "set", 0.0 } },` |
|      - | 6945 | `	};` |
|   5745 | 6946 | `	return PH7_InstallNativeEnum(&(*pVm), "PropertyHookType", MEMOBJ_STRING,` |
|      - | 6947 | `		aCase, SX_ARRAYSIZE(aCase), 0, 0);` |
|      5 | 6948 | `}` |
|   5740 | 6949 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionFunc(ph7_vm *pVm)` |
|      5 | 6950 | `{` |
|      - | 6951 | `	static const PH7_NativePropDef aAbstractProp[] = {` |
|      - | 6952 | `		{ "name",  PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 6953 | `		/* PHL-only: the Closure being reflected. php reaches the same state from` |
|      - | 6954 | `		 * the function record itself; PHL has no hidden-slot bit yet (§7.4 (e)). */` |
|      - | 6955 | `		{ RF_CL,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 6956 | `	};` |
|      - | 6957 | `	static const PH7_NativeMethodDef aAbstractMethod[] = {` |
|      - | 6958 | `		{ "__clone",       PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - | 6959 | `		{ "inNamespace",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_inNamespace },` |
|      - | 6960 | `		{ "isClosure",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isClosure },` |
|      - | 6961 | `		{ "isDeprecated",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isDeprecated },` |
|      - | 6962 | `		{ "isInternal",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isInternal },` |
|      - | 6963 | `		{ "isUserDefined", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isUserDefined },` |
|      - | 6964 | `		{ "isGenerator",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isGenerator },` |
|      - | 6965 | `		{ "isVariadic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isVariadic },` |
|      - | 6966 | `		{ "isStatic",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isStatic },` |
|      - | 6967 | `		{ "getClosureThis", PH7_MOD_PUBLIC, "", "@?object", vm_builtin_ReflectionFunc_getClosureThis },` |
|      - | 6968 | `		{ "getClosureScopeClass", PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|      - | 6969 | `		  vm_builtin_ReflectionFunc_getClosureScopeClass },` |
|      - | 6970 | `		/* php answers the same class for both; PHL has no separate called-scope. */` |
|      - | 6971 | `		{ "getClosureCalledClass", PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|      - | 6972 | `		  vm_builtin_ReflectionFunc_getClosureCalledClass },` |
|      - | 6973 | `		{ "getClosureUsedVariables", PH7_MOD_PUBLIC, "", "array",` |
|      - | 6974 | `		  vm_builtin_ReflectionFunc_getClosureUsedVariables },` |
|      - | 6975 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getDocComment },` |
|      - | 6976 | `		{ "getEndLine",    PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionFunc_getEndLine },` |
|      - | 6977 | `		{ "getExtension",  PH7_MOD_PUBLIC, "", "@?ReflectionExtension", vm_builtin_ReflectionFunc_getExtension },` |
|      - | 6978 | `		{ "getExtensionName", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getExtensionName },` |
|      - | 6979 | `		{ "getFileName",   PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getFileName },` |
|      - | 6980 | `		{ "getName",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getName },` |
|      - | 6981 | `		{ "getNamespaceName", PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getNamespaceName },` |
|      - | 6982 | `		{ "getNumberOfParameters", PH7_MOD_PUBLIC, "", "@int",` |
|      - | 6983 | `		  vm_builtin_ReflectionFunc_getNumberOfParameters },` |
|      - | 6984 | `		{ "getNumberOfRequiredParameters", PH7_MOD_PUBLIC, "", "@int",` |
|      - | 6985 | `		  vm_builtin_ReflectionFunc_getNumberOfRequiredParameters },` |
|      - | 6986 | `		{ "getParameters", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionFunc_getParameters },` |
|      - | 6987 | `		{ "getShortName",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getShortName },` |
|      - | 6988 | `		{ "getStartLine",  PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionFunc_getStartLine },` |
|      - | 6989 | `		{ "getStaticVariables", PH7_MOD_PUBLIC, "", "@array",` |
|      - | 6990 | `		  vm_builtin_ReflectionFunc_getStaticVariables },` |
|      - | 6991 | `		{ "returnsReference", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_returnsReference },` |
|      - | 6992 | `		{ "hasReturnType", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_hasReturnType },` |
|      - | 6993 | `		{ "getReturnType", PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionFunc_getReturnType },` |
|      - | 6994 | `		{ "hasTentativeReturnType", PH7_MOD_PUBLIC, "", "bool",` |
|      - | 6995 | `		  vm_builtin_ReflectionFunc_hasTentativeReturnType },` |
|      - | 6996 | `		{ "getTentativeReturnType", PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|      - | 6997 | `		  vm_builtin_ReflectionFunc_getTentativeReturnType },` |
|      - | 6998 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - | 6999 | `		  vm_builtin_ReflectionFunc_getAttributes },` |
|      - | 7000 | `	};` |
|      - | 7001 | `	static const PH7_NativeConstDef aFunctionConst[] = {` |
|      - | 7002 | `		{ "IS_DEPRECATED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2048, 0, 0.0 },` |
|      - | 7003 | `	};` |
|      - | 7004 | `	static const PH7_NativeMethodDef aFunctionMethod[] = {` |
|      - | 7005 | `		{ "__construct", PH7_MOD_PUBLIC, "Closure\|string $function", "",` |
|      - | 7006 | `		  vm_builtin_ReflectionFunction_construct },` |
|      - | 7007 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionFunc_toString },` |
|      - | 7008 | `		{ "isAnonymous", PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionFunction_isAnonymous },` |
|      - | 7009 | `		{ "isDisabled",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunction_isDisabled },` |
|      - | 7010 | `		{ "invoke",      PH7_MOD_PUBLIC, "mixed ...$args", "@mixed",` |
|      - | 7011 | `		  vm_builtin_ReflectionFunction_invoke },` |
|      - | 7012 | `		{ "invokeArgs",  PH7_MOD_PUBLIC, "array $args", "@mixed",` |
|      - | 7013 | `		  vm_builtin_ReflectionFunction_invokeArgs },` |
|      - | 7014 | `		{ "getClosure",  PH7_MOD_PUBLIC, "", "@Closure", vm_builtin_ReflectionFunction_getClosure },` |
|      - | 7015 | `	};` |
|      - | 7016 | `	static const PH7_NativePropDef aMethodProp[] = {` |
|      - | 7017 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 7018 | `	};` |
|      - | 7019 | `	static const PH7_NativeConstDef aMethodConst[] = {` |
|      - | 7020 | `		{ "IS_STATIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16, 0, 0.0 },` |
|      - | 7021 | `		{ "IS_PUBLIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,  0, 0.0 },` |
|      - | 7022 | `		{ "IS_PROTECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,  0, 0.0 },` |
|      - | 7023 | `		{ "IS_PRIVATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,  0, 0.0 },` |
|      - | 7024 | `		{ "IS_ABSTRACT",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64, 0, 0.0 },` |
|      - | 7025 | `		{ "IS_FINAL",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|      - | 7026 | `	};` |
|      - | 7027 | `	static const PH7_NativeMethodDef aMethodMethod[] = {` |
|      - | 7028 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $objectOrMethod, ?string $method = null", "",` |
|      - | 7029 | `		  vm_builtin_ReflectionMethod_construct },` |
|      - | 7030 | `		{ "createFromMethodName", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $method", "static",` |
|      - | 7031 | `		  vm_builtin_ReflectionMethod_createFromMethodName },` |
|      - | 7032 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionFunc_toString },` |
|      - | 7033 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isPublic },` |
|      - | 7034 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isPrivate },` |
|      - | 7035 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isProtected },` |
|      - | 7036 | `		{ "isAbstract",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isAbstract },` |
|      - | 7037 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isFinal },` |
|      - | 7038 | `		{ "isConstructor", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isConstructor },` |
|      - | 7039 | `		{ "isDestructor",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isDestructor },` |
|      - | 7040 | `		{ "getClosure",  PH7_MOD_PUBLIC, "?object $object = null", "@Closure",` |
|      - | 7041 | `		  vm_builtin_ReflectionMethod_getClosure },` |
|      - | 7042 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionMethod_getModifiers },` |
|      - | 7043 | `		{ "invoke",      PH7_MOD_PUBLIC, "?object $object, mixed ...$args", "@mixed",` |
|      - | 7044 | `		  vm_builtin_ReflectionMethod_invoke },` |
|      - | 7045 | `		{ "invokeArgs",  PH7_MOD_PUBLIC, "?object $object, array $args", "@mixed",` |
|      - | 7046 | `		  vm_builtin_ReflectionMethod_invokeArgs },` |
|      - | 7047 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|      - | 7048 | `		  vm_builtin_ReflectionMethod_getDeclaringClass },` |
|      - | 7049 | `		{ "getPrototype", PH7_MOD_PUBLIC, "", "@ReflectionMethod", vm_builtin_ReflectionMethod_getPrototype },` |
|      - | 7050 | `		{ "hasPrototype", PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionMethod_hasPrototype },` |
|      - | 7051 | `		{ "setAccessible", PH7_MOD_PUBLIC, "bool $accessible", "@void",` |
|      - | 7052 | `		  vm_builtin_ReflectionMethod_setAccessible },` |
|      - | 7053 | `	};` |
|      - | 7054 | `	static const PH7_NativePropDef aParamProp[] = {` |
|      - | 7055 | `		{ "name", PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 7056 | `		/* PHL-only, the three that identify the parameter (§7.4 (e)) */` |
|      - | 7057 | `		{ RP_T,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 7058 | `		{ RP_M,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 7059 | `		{ RP_P,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|      - | 7060 | `	};` |
|      - | 7061 | `	static const PH7_NativeMethodDef aParamMethod[] = {` |
|      - | 7062 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - | 7063 | `		/* php leaves $function UNTYPED here: it takes a name, a Closure, a` |
|      - | 7064 | ``		 * `[$obj, 'm']` pair or "C::m", and no declarable union covers all four. */`` |
|      - | 7065 | `		{ "__construct", PH7_MOD_PUBLIC, "$function, string\|int $param", "",` |
|      - | 7066 | `		  vm_builtin_ReflectionParameter_construct },` |
|      - | 7067 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionParameter_toString },` |
|      - | 7068 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionParameter_getName },` |
|      - | 7069 | `		{ "isPassedByReference", PH7_MOD_PUBLIC, "", "@bool",` |
|      - | 7070 | `		  vm_builtin_ReflectionParameter_isPassedByReference },` |
|      - | 7071 | `		{ "canBePassedByValue",  PH7_MOD_PUBLIC, "", "@bool",` |
|      - | 7072 | `		  vm_builtin_ReflectionParameter_canBePassedByValue },` |
|      - | 7073 | `		{ "getDeclaringFunction", PH7_MOD_PUBLIC, "", "@ReflectionFunctionAbstract",` |
|      - | 7074 | `		  vm_builtin_ReflectionParameter_getDeclaringFunction },` |
|      - | 7075 | `		{ "getDeclaringClass",   PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|      - | 7076 | `		  vm_builtin_ReflectionParameter_getDeclaringClass },` |
|      - | 7077 | `		{ "getClass",    PH7_MOD_PUBLIC, "", "@?ReflectionClass", vm_builtin_ReflectionParameter_getClass },` |
|      - | 7078 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_hasType },` |
|      - | 7079 | `		{ "getType",     PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionParameter_getType },` |
|      - | 7080 | `		{ "isArray",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isArray },` |
|      - | 7081 | `		{ "isCallable",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isCallable },` |
|      - | 7082 | `		{ "allowsNull",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_allowsNull },` |
|      - | 7083 | `		{ "getPosition", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionParameter_getPosition },` |
|      - | 7084 | `		{ "isOptional",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isOptional },` |
|      - | 7085 | `		{ "isDefaultValueAvailable", PH7_MOD_PUBLIC, "", "@bool",` |
|      - | 7086 | `		  vm_builtin_ReflectionParameter_isDefaultValueAvailable },` |
|      - | 7087 | `		{ "getDefaultValue", PH7_MOD_PUBLIC, "", "@mixed",` |
|      - | 7088 | `		  vm_builtin_ReflectionParameter_getDefaultValue },` |
|      - | 7089 | `		{ "isDefaultValueConstant", PH7_MOD_PUBLIC, "", "@bool",` |
|      - | 7090 | `		  vm_builtin_ReflectionParameter_isDefaultValueConstant },` |
|      - | 7091 | `		{ "getDefaultValueConstantName", PH7_MOD_PUBLIC, "", "@?string",` |
|      - | 7092 | `		  vm_builtin_ReflectionParameter_getDefaultValueConstantName },` |
|      - | 7093 | `		{ "isVariadic",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isVariadic },` |
|      - | 7094 | `		{ "isPromoted",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionParameter_isPromoted },` |
|      - | 7095 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - | 7096 | `		  vm_builtin_ReflectionParameter_getAttributes },` |
|      - | 7097 | `	};` |
|      - | 7098 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 7099 | `		{ "ReflectionFunctionAbstract", 0, "Reflector", PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 7100 | `		  aAbstractMethod, SX_ARRAYSIZE(aAbstractMethod), 0, 0,` |
|      - | 7101 | `		  aAbstractProp, SX_ARRAYSIZE(aAbstractProp), 0, 0, 0 },` |
|      - | 7102 | `		{ "ReflectionFunction", "ReflectionFunctionAbstract", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 7103 | `		  aFunctionMethod, SX_ARRAYSIZE(aFunctionMethod),` |
|      - | 7104 | `		  aFunctionConst, SX_ARRAYSIZE(aFunctionConst), 0, 0, 0, 0, 0 },` |
|      - | 7105 | `		{ "ReflectionMethod", "ReflectionFunctionAbstract", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 7106 | `		  aMethodMethod, SX_ARRAYSIZE(aMethodMethod),` |
|      - | 7107 | `		  aMethodConst, SX_ARRAYSIZE(aMethodConst),` |
|      - | 7108 | `		  aMethodProp, SX_ARRAYSIZE(aMethodProp), 0, 0, 0 },` |
|      - | 7109 | `		{ "ReflectionParameter", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 7110 | `		  aParamMethod, SX_ARRAYSIZE(aParamMethod), 0, 0,` |
|      - | 7111 | `		  aParamProp, SX_ARRAYSIZE(aParamProp), 0, 0, 0 },` |
|      - | 7112 | `	};` |
|   5745 | 7113 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 | 7114 | `}` |
|      - | 7115 | `/*` |
|      - | 7116 | ` * ---------------------------------------------------------------------------` |
|      - | 7117 | ` * ReflectionProperty and ReflectionClassConstant.` |
|      - | 7118 | ` *` |
|      - | 7119 | ` * Chunk 3, the last of the three that made up "the core". Seven thunks retire` |
|      - | 7120 | ` * with it — __reflect_prop_read, __reflect_prop_write, __reflect_prop_state,` |
|      - | 7121 | ` * __reflect_prop_default, __reflect_static_value, __reflect_static_set and` |
|      - | 7122 | ` * __reflect_const_value — because a native method reaches an instance's slot` |
|      - | 7123 | ` * table, a static's shared slot and a constant's lazy slot directly.` |
|      - | 7124 | ` * ---------------------------------------------------------------------------` |
|      - | 7125 | ` */` |
|      - | 7126 | `#define RP_DYNOBJ "__dynobj"   /* the instance a DYNAMIC property lives on */` |
|      - | 7127 |  |
|      - | 7128 | `/* skipLazyInitialization() is a no-op on an object that is not lazy, which is` |
|      - | 7129 | ` * every object PHL can build — so it answers what php answers rather than` |
|      - | 7130 | ` * refusing (§7.4). */` |
|    ! 0 | 7131 | `static int vm_builtin_ReflectionProperty_noop(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 7132 | `{` |
|    ! 0 | 7133 | `	SXUNUSED(pCtx);` |
|    ! 0 | 7134 | `	SXUNUSED(nArg);` |
|    ! 0 | 7135 | `	SXUNUSED(apArg);` |
|    ! 0 | 7136 | `	return PH7_OK;` |
|    ! 0 | 7137 | `}` |
|      - | 7138 | `/*` |
|      - | 7139 | `` * A STATIC property's shared slot, read and written the way `C::$s` is: the`` |
|      - | 7140 | ` * class's static table materializes first (a default that threw at the` |
|      - | 7141 | ` * declaration raises HERE), and an uninitialized typed static is php's Error.` |
|      - | 7142 | ` */` |
|      6 | 7143 | `static int ReflectStaticSlotRead(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr)` |
|      2 | 7144 | `{` |
|      - | 7145 | `	SyHashEntry *pSlot;` |
|      - | 7146 | `	ph7_value *pVal;` |
|      8 | 7147 | `	sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|      8 | 7148 | `	if( rc != SXRET_OK ){` |
|      3 | 7149 | `		return rc;` |
|      - | 7150 | `	}` |
|      5 | 7151 | `	pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|      5 | 7152 | `	if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|    ! 0 | 7153 | `		ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|    ! 0 | 7154 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      - | 7155 | `			"Typed static property %z::$%z must not be accessed before initialization",` |
|    ! 0 | 7156 | `			&pDecl->sName, &pAttr->sName);` |
|      - | 7157 | `	}` |
|      5 | 7158 | `	pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      5 | 7159 | `	if( pVal ){` |
|      5 | 7160 | `		ph7_result_value(pCtx, pVal);` |
|      3 | 7161 | `	}else{` |
|    ! 0 | 7162 | `		ph7_result_null(pCtx);` |
|      - | 7163 | `	}` |
|      5 | 7164 | `	return PH7_OK;` |
|      5 | 7165 | `}` |
|      4 | 7166 | `static int ReflectStaticSlotWrite(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr,` |
|      - | 7167 | `	ph7_value *pValue)` |
|      2 | 7168 | `{` |
|      - | 7169 | `	ph7_value *pSlot;` |
|      6 | 7170 | `	sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|      6 | 7171 | `	if( rc != SXRET_OK ){` |
|      3 | 7172 | `		return rc;` |
|      - | 7173 | `	}` |
|      3 | 7174 | `	rc = ReflectEnforceStore(pCtx, pAttr->nIdx, pValue);` |
|      3 | 7175 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 7176 | `		return rc;` |
|      - | 7177 | `	}` |
|      3 | 7178 | `	pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      3 | 7179 | `	if( pSlot ){` |
|      3 | 7180 | `		PH7_MemObjStore(pValue, pSlot);` |
|      1 | 7181 | `	}` |
|      3 | 7182 | `	return PH7_OK;` |
|      4 | 7183 | `}` |
|      - | 7184 |  |
|      - | 7185 | `/* What a ReflectionProperty / ReflectionClassConstant is looking at. */` |
|      - | 7186 | `typedef struct ReflectMemberRef ReflectMemberRef;` |
|      - | 7187 | `struct ReflectMemberRef` |
|      - | 7188 | `{` |
|      - | 7189 | `	ph7_class *pClass;            /* the reflected class */` |
|      - | 7190 | `	ph7_class_attr *pAttr;        /* the declared member (NULL for a dynamic property) */` |
|      - | 7191 | `	ph7_class_instance *pDynObj;  /* the instance a dynamic property lives on */` |
|      - | 7192 | `	const char *zName;            /* the member's name (borrowed from the slot) */` |
|      - | 7193 | `	int nName;` |
|      - | 7194 | `};` |
|      - | 7195 | `/*` |
|      - | 7196 | `` * Resolve `$this->class` + `$this->name` into the member. Uses the LISTING`` |
|      - | 7197 | ` * answer of the member walk, which is php's: a base class's PRIVATE member is` |
|      - | 7198 | ``  * not reachable by name from the subclass (`new ReflectionProperty('B','pp')` `` |
|      - | 7199 | `` * raises where `B extends A` and `A::$pp` is private).`` |
|      - | 7200 | ` */` |
|    648 | 7201 | `static int ReflectMemberOfThis(ph7_context *pCtx, ReflectMemberRef *pOut, int iKind)` |
|      3 | 7202 | `{` |
|    651 | 7203 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 7204 | `	const char *zClass;` |
|      - | 7205 | `	int nClass;` |
|      - | 7206 | `	SySet aMembers;` |
|      - | 7207 | `	sxu32 n;` |
|    651 | 7208 | `	SyZero(pOut, sizeof(*pOut));` |
|    651 | 7209 | `	if( pThis == 0 ){` |
|    ! 0 | 7210 | `		return 0;` |
|      - | 7211 | `	}` |
|    651 | 7212 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|    651 | 7213 | `	PH7_NativeAttrStr(pThis, "name", &pOut->zName, &pOut->nName);` |
|    651 | 7214 | `	if( nClass < 1 ){` |
|    ! 0 | 7215 | `		return 0;` |
|      - | 7216 | `	}` |
|    651 | 7217 | `	pOut->pClass = PH7_VmExtractClass(pCtx->pVm, zClass, (sxu32)nClass, FALSE, 0);` |
|    651 | 7218 | `	if( pOut->pClass == 0 ){` |
|    ! 0 | 7219 | `		return 0;` |
|      - | 7220 | `	}` |
|    651 | 7221 | `	if( iKind == REFLECT_MEMBER_PROP ){` |
|    537 | 7222 | `		pOut->pDynObj = PH7_NativeAttrObj(pThis, RP_DYNOBJ);` |
|    267 | 7223 | `	}` |
|    651 | 7224 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|    651 | 7225 | `	ReflectMembers(pCtx->pVm, pOut->pClass, &aMembers, 0);` |
|   2315 | 7226 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   2309 | 7227 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   2309 | 7228 | `		if( pM->iKind == iKind && ReflectKeyIs(pM, pOut->zName, pOut->nName) ){` |
|    645 | 7229 | `			pOut->pAttr = pM->pAttr;` |
|    645 | 7230 | `			break;` |
|      - | 7231 | `		}` |
|    835 | 7232 | `	}` |
|    651 | 7233 | `	SySetRelease(&aMembers);` |
|    651 | 7234 | `	return pOut->pAttr != 0 \|\| pOut->pDynObj != 0;` |
|    327 | 7235 | `}` |
|      - | 7236 | `/* The class php reports as the member's declarer. */` |
|     32 | 7237 | `static ph7_class * ReflectMemberDecl(const ReflectMemberRef *pRef)` |
|      3 | 7238 | `{` |
|     35 | 7239 | `	if( pRef->pAttr && pRef->pAttr->pDeclClass ){` |
|     35 | 7240 | `		return pRef->pAttr->pDeclClass;` |
|      - | 7241 | `	}` |
|    ! 0 | 7242 | `	return pRef->pClass;` |
|     19 | 7243 | `}` |
|      - | 7244 | `/* The instance slot record for a dynamic (or any instance-owned) property. */` |
|     62 | 7245 | `static VmClassAttr * ReflectInstanceAttr(ph7_class_instance *pObj, const char *zName, int nName)` |
|      1 | 7246 | `{` |
|      - | 7247 | `	SyHashEntry *pEntry;` |
|     63 | 7248 | `	if( pObj == 0 \|\| nName < 1 ){` |
|      5 | 7249 | `		return 0;` |
|      - | 7250 | `	}` |
|     59 | 7251 | `	pEntry = SyHashGet(&pObj->hAttr, (const void *)zName, (sxu32)nName);` |
|     59 | 7252 | `	return pEntry ? (VmClassAttr *)pEntry->pUserData : 0;` |
|     32 | 7253 | `}` |
|      - | 7254 | `/* ---- ReflectionProperty ---- */` |
|      - | 7255 | `/* ReflectionProperty::__construct(object\|string $class, string $property) */` |
|    366 | 7256 | `static int vm_builtin_ReflectionProperty_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 7257 | `{` |
|    369 | 7258 | `	ph7_vm *pVm = pCtx->pVm;` |
|    369 | 7259 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    369 | 7260 | `	ph7_class_instance *pObj = 0;` |
|      - | 7261 | `	ph7_class *pClass;` |
|      - | 7262 | `	const char *zProp;` |
|      - | 7263 | `	int nProp;` |
|      - | 7264 | `	SySet aMembers;` |
|      - | 7265 | `	sxu32 n;` |
|    369 | 7266 | `	int bFound = 0;` |
|    369 | 7267 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 | 7268 | `		return PH7_OK;` |
|      - | 7269 | `	}` |
|    369 | 7270 | `	if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      7 | 7271 | `		pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 7272 | `	}` |
|    369 | 7273 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|    369 | 7274 | `	if( pClass == 0 ){` |
|      - | 7275 | `		const char *zName;` |
|      - | 7276 | `		int nName;` |
|      3 | 7277 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|      4 | 7278 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 7279 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      - | 7280 | `	}` |
|    367 | 7281 | `	zProp = ph7_value_to_string(apArg[1], &nProp);` |
|    367 | 7282 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|    367 | 7283 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|   1307 | 7284 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   1297 | 7285 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   1297 | 7286 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zProp, nProp) ){` |
|      - | 7287 | `			/* php's $class is the DECLARING class, not the one asked about. */` |
|    357 | 7288 | `			pClass = pM->pDecl ? pM->pDecl : pClass;` |
|    357 | 7289 | `			bFound = 1;` |
|    357 | 7290 | `			break;` |
|      - | 7291 | `		}` |
|    473 | 7292 | `	}` |
|    367 | 7293 | `	SySetRelease(&aMembers);` |
|    549 | 7294 | `	PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pClass->sName),` |
|    364 | 7295 | `		(int)SyStringLength(&pClass->sName));` |
|    367 | 7296 | `	if( bFound ){` |
|    357 | 7297 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", zProp, nProp);` |
|    357 | 7298 | `		return PH7_OK;` |
|      - | 7299 | `	}` |
|      - | 7300 | `	/* Not declared: an OBJECT may still own it as a dynamic property. */` |
|     11 | 7301 | `	if( ReflectInstanceAttr(pObj, zProp, nProp) ){` |
|      7 | 7302 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", zProp, nProp);` |
|      7 | 7303 | `		PH7_NativeSetAttrObj(pVm, pThis, RP_DYNOBJ, pObj);` |
|      7 | 7304 | `		return PH7_OK;` |
|      - | 7305 | `	}` |
|      7 | 7306 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 | 7307 | `		"Property %z::$%.*s does not exist", &pClass->sName, nProp, zProp);` |
|    186 | 7308 | `}` |
|    222 | 7309 | `static int vm_builtin_ReflectionProperty_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 7310 | `{` |
|    224 | 7311 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    224 | 7312 | `	const char *zName = "";` |
|    224 | 7313 | `	int nName = 0;` |
|    111 | 7314 | `	SXUNUSED(nArg);` |
|    111 | 7315 | `	SXUNUSED(apArg);` |
|    224 | 7316 | `	if( pThis ){` |
|    224 | 7317 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    111 | 7318 | `	}` |
|    224 | 7319 | `	ph7_result_string(pCtx, zName, nName);` |
|    224 | 7320 | `	return PH7_OK;` |
|      2 | 7321 | `}` |
|      - | 7322 | `/*` |
|      - | 7323 | ` * getMangledName(): the name php stores the slot under —  plain for a public` |
|      - | 7324 | ` * property, "\0*\0name" for a protected one and "\0Class\0name" for a private` |
|      - | 7325 | ` * one. PHL's tables are not mangled, so the name is COMPOSED here; what php` |
|      - | 7326 | ` * exposes is the spelling, not the storage.` |
|      - | 7327 | ` */` |
|      6 | 7328 | `static int vm_builtin_ReflectionProperty_getMangledName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7329 | `{` |
|      - | 7330 | `	ReflectMemberRef sRef;` |
|      - | 7331 | `	SyBlob sOut;` |
|      3 | 7332 | `	SXUNUSED(nArg);` |
|      3 | 7333 | `	SXUNUSED(apArg);` |
|      7 | 7334 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 7335 | `		ph7_result_string(pCtx, sRef.zName, sRef.nName);` |
|    ! 0 | 7336 | `		return PH7_OK;` |
|      - | 7337 | `	}` |
|      7 | 7338 | `	if( sRef.pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|      3 | 7339 | `		ph7_result_string(pCtx, sRef.zName, sRef.nName);` |
|      3 | 7340 | `		return PH7_OK;` |
|      - | 7341 | `	}` |
|      5 | 7342 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|      5 | 7343 | `	SyBlobAppend(&sOut, "\0", 1);` |
|      5 | 7344 | `	if( sRef.pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|      3 | 7345 | `		SyBlobAppend(&sOut, "*", 1);` |
|      2 | 7346 | `	}else{` |
|      3 | 7347 | `		ph7_class *pDecl = ReflectMemberDecl(&sRef);` |
|      3 | 7348 | `		SyBlobAppend(&sOut, SyStringData(&pDecl->sName), SyStringLength(&pDecl->sName));` |
|      - | 7349 | `	}` |
|      5 | 7350 | `	SyBlobAppend(&sOut, "\0", 1);` |
|      5 | 7351 | `	SyBlobAppend(&sOut, sRef.zName, (sxu32)sRef.nName);` |
|      5 | 7352 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|      5 | 7353 | `	SyBlobRelease(&sOut);` |
|      5 | 7354 | `	return PH7_OK;` |
|      4 | 7355 | `}` |
|      - | 7356 | `/* The boolean predicates, all off the declared attribute. */` |
|    122 | 7357 | `static int ReflectPropFlag(ph7_context *pCtx, int iWhat)` |
|      1 | 7358 | `{` |
|      - | 7359 | `	ReflectMemberRef sRef;` |
|    123 | 7360 | `	int bYes = 0;` |
|    182 | 7361 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|    119 | 7362 | `		ph7_class_attr *pAttr = sRef.pAttr;` |
|    119 | 7363 | `		switch( iWhat ){` |
|      5 | 7364 | `		case 0: bYes = pAttr->iProtection == PH7_CLASS_PROT_PUBLIC; break;` |
|    ! 0 | 7365 | `		case 1: bYes = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE; break;` |
|      3 | 7366 | `		case 2: bYes = pAttr->iProtection == PH7_CLASS_PROT_PROTECTED; break;` |
|     23 | 7367 | `		case 3: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) != 0; break;` |
|     23 | 7368 | `		case 4: bYes = ReflectPropProtectedSet(pAttr); break;` |
|      3 | 7369 | `		case 5: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0; break;` |
|     49 | 7370 | `		case 6: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0; break;` |
|      3 | 7371 | `		case 7: bYes = 1; break;                                    /* isDefault */` |
|    ! 0 | 7372 | `		case 8: bYes = 0; break;                                    /* isDynamic */` |
|     16 | 7373 | `		case 9: bYes = (pAttr->iFlags` |
|     16 | 7374 | `			& (PH7_CLASS_ATTR_HOOK_VIRTUAL\|PH7_CLASS_ATTR_NATIVE_VIRTUAL)) != 0; break;` |
|      3 | 7375 | `		default:` |
|      7 | 7376 | `			bYes = (pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) != 0;` |
|      6 | 7377 | `			break;` |
|      - | 7378 | `		}` |
|     64 | 7379 | `	}else if( iWhat == 0 \|\| iWhat == 8 ){` |
|      - | 7380 | `		/* A dynamic property is public and, by definition, not a default one. */` |
|      3 | 7381 | `		bYes = 1;` |
|      1 | 7382 | `	}` |
|    123 | 7383 | `	ph7_result_bool(pCtx, bYes);` |
|    123 | 7384 | `	return PH7_OK;` |
|      1 | 7385 | `}` |
|      - | 7386 | `#define REFLECT_PROP_FLAG(NAME,WHAT) \` |
|      - | 7387 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 7388 | `	{ \` |
|      - | 7389 | `		SXUNUSED(nArg); \` |
|      - | 7390 | `		SXUNUSED(apArg); \` |
|      - | 7391 | `		return ReflectPropFlag(pCtx, WHAT); \` |
|      - | 7392 | `	}` |
|      5 | 7393 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPublic, 0)` |
|    ! 0 | 7394 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPrivate, 1)` |
|      3 | 7395 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isProtected, 2)` |
|     23 | 7396 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPrivateSet, 3)` |
|     23 | 7397 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isProtectedSet, 4)` |
|      3 | 7398 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isStatic, 5)` |
|     49 | 7399 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isReadOnly, 6)` |
|      5 | 7400 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isDefault, 7)` |
|      3 | 7401 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isDynamic, 8)` |
|     11 | 7402 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isVirtual, 9)` |
|      7 | 7403 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_hasHooks, 10)` |
|      - | 7404 |  |
|      - | 7405 | `/* php has no abstract properties outside an interface stub, and PHL none at` |
|      - | 7406 | ` * all; isLazy() answers what is true of a VM without lazy objects. */` |
|    ! 0 | 7407 | `static int vm_builtin_ReflectionProperty_false(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 7408 | `{` |
|    ! 0 | 7409 | `	SXUNUSED(nArg);` |
|    ! 0 | 7410 | `	SXUNUSED(apArg);` |
|    ! 0 | 7411 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7412 | `	return PH7_OK;` |
|    ! 0 | 7413 | `}` |
|      - | 7414 | `/*` |
|      - | 7415 | ` * isPromoted(): the property came from a constructor-promoted parameter. PHL` |
|      - | 7416 | ` * records promotion on the ARGUMENT (VM_FUNC_ARG_PROMOTED), not on the` |
|      - | 7417 | ` * attribute, so the constructor's parameter list is what answers.` |
|      - | 7418 | ` */` |
|      4 | 7419 | `static int vm_builtin_ReflectionProperty_isPromoted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7420 | `{` |
|      - | 7421 | `	ReflectMemberRef sRef;` |
|      - | 7422 | `	ph7_class_method *pCons;` |
|      - | 7423 | `	ph7_vm_func_arg *aArg;` |
|      - | 7424 | `	sxu32 n;` |
|      5 | 7425 | `	int bYes = 0;` |
|      2 | 7426 | `	SXUNUSED(nArg);` |
|      2 | 7427 | `	SXUNUSED(apArg);` |
|      5 | 7428 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|      5 | 7429 | `		ph7_class *pDecl = ReflectMemberDecl(&sRef);` |
|      5 | 7430 | `		pCons = PH7_ClassExtractMethod(pDecl, "__construct", sizeof("__construct")-1);` |
|      5 | 7431 | `		if( pCons ){` |
|      5 | 7432 | `			aArg = (ph7_vm_func_arg *)SySetBasePtr(&pCons->sFunc.aArgs);` |
|      7 | 7433 | `			for( n = 0 ; n < SySetUsed(&pCons->sFunc.aArgs) ; n++ ){` |
|      5 | 7434 | `				if( (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|    ! 0 | 7435 | `					continue;` |
|      - | 7436 | `				}` |
|      4 | 7437 | `				if( SyStringLength(&aArg[n].sName) == (sxu32)sRef.nName` |
|      4 | 7438 | `				 && SyMemcmp(SyStringData(&aArg[n].sName), sRef.zName, (sxu32)sRef.nName) == 0 ){` |
|      3 | 7439 | `					bYes = 1;` |
|      3 | 7440 | `					break;` |
|      - | 7441 | `				}` |
|      2 | 7442 | `			}` |
|      2 | 7443 | `		}` |
|      2 | 7444 | `	}` |
|      5 | 7445 | `	ph7_result_bool(pCtx, bYes);` |
|      5 | 7446 | `	return PH7_OK;` |
|      1 | 7447 | `}` |
|    120 | 7448 | `static int vm_builtin_ReflectionProperty_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 7449 | `{` |
|      - | 7450 | `	ReflectMemberRef sRef;` |
|     60 | 7451 | `	SXUNUSED(nArg);` |
|     60 | 7452 | `	SXUNUSED(apArg);` |
|    122 | 7453 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 7454 | `		ph7_result_int(pCtx, 1);   /* a dynamic property is public */` |
|    ! 0 | 7455 | `		return PH7_OK;` |
|      - | 7456 | `	}` |
|    122 | 7457 | `	ph7_result_int64(pCtx, ReflectPropModifiers(sRef.pAttr));` |
|    122 | 7458 | `	return PH7_OK;` |
|     62 | 7459 | `}` |
|      4 | 7460 | `static int vm_builtin_ReflectionProperty_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7461 | `{` |
|      - | 7462 | `	ReflectMemberRef sRef;` |
|      2 | 7463 | `	SXUNUSED(nArg);` |
|      2 | 7464 | `	SXUNUSED(apArg);` |
|      5 | 7465 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 | 7466 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7467 | `		return PH7_OK;` |
|      - | 7468 | `	}` |
|      5 | 7469 | `	return ReflectResultClassOf(pCtx, ReflectMemberDecl(&sRef));` |
|      3 | 7470 | `}` |
|      6 | 7471 | `static int vm_builtin_ReflectionProperty_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7472 | `{` |
|      - | 7473 | `	ReflectMemberRef sRef;` |
|      3 | 7474 | `	SXUNUSED(nArg);` |
|      3 | 7475 | `	SXUNUSED(apArg);` |
|      6 | 7476 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr` |
|      7 | 7477 | `	 && SyStringLength(&sRef.pAttr->sDoc) > 0 ){` |
|      7 | 7478 | `		ph7_result_string(pCtx, SyStringData(&sRef.pAttr->sDoc),` |
|      4 | 7479 | `			(int)SyStringLength(&sRef.pAttr->sDoc));` |
|      3 | 7480 | `	}else{` |
|      3 | 7481 | `		ph7_result_bool(pCtx, 0);` |
|      - | 7482 | `	}` |
|      7 | 7483 | `	return PH7_OK;` |
|      1 | 7484 | `}` |
|      6 | 7485 | `static int vm_builtin_ReflectionProperty_hasType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7486 | `{` |
|      - | 7487 | `	ReflectMemberRef sRef;` |
|      3 | 7488 | `	SXUNUSED(nArg);` |
|      3 | 7489 | `	SXUNUSED(apArg);` |
|     13 | 7490 | `	ph7_result_bool(pCtx, ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP)` |
|      6 | 7491 | `		&& sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);` |
|      7 | 7492 | `	return PH7_OK;` |
|      1 | 7493 | `}` |
|     80 | 7494 | `static int vm_builtin_ReflectionProperty_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 7495 | `{` |
|      - | 7496 | `	ReflectMemberRef sRef;` |
|     40 | 7497 | `	SXUNUSED(nArg);` |
|     40 | 7498 | `	SXUNUSED(apArg);` |
|     80 | 7499 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0` |
|     80 | 7500 | `	 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|     79 | 7501 | `	 \|\| SyStringLength(&sRef.pAttr->sTypeName) < 1 ){` |
|      7 | 7502 | `		ph7_result_null(pCtx);` |
|      7 | 7503 | `		return PH7_OK;` |
|      - | 7504 | `	}` |
|    113 | 7505 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx,` |
|     74 | 7506 | `		SyStringData(&sRef.pAttr->sTypeName), (int)SyStringLength(&sRef.pAttr->sTypeName)));` |
|     42 | 7507 | `}` |
|     52 | 7508 | `static int vm_builtin_ReflectionProperty_hasDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7509 | `{` |
|      - | 7510 | `	ReflectMemberRef sRef;` |
|     26 | 7511 | `	SXUNUSED(nArg);` |
|     26 | 7512 | `	SXUNUSED(apArg);` |
|     53 | 7513 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 7514 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7515 | `		return PH7_OK;` |
|      - | 7516 | `	}` |
|     53 | 7517 | `	if( SySetUsed(&sRef.pAttr->aByteCode) > 0 \|\| sRef.pAttr->pNativeValue ){` |
|     25 | 7518 | `		ph7_result_bool(pCtx, 1);` |
|     25 | 7519 | `		return PH7_OK;` |
|      - | 7520 | `	}` |
|      - | 7521 | `	/* An UNTYPED property with no initializer still defaults to null. */` |
|     29 | 7522 | `	ph7_result_bool(pCtx, (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0);` |
|     29 | 7523 | `	return PH7_OK;` |
|     27 | 7524 | `}` |
|     30 | 7525 | `static int vm_builtin_ReflectionProperty_getDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7526 | `{` |
|      - | 7527 | `	ReflectMemberRef sRef;` |
|      - | 7528 | `	ph7_value sValue;` |
|     15 | 7529 | `	SXUNUSED(nArg);` |
|     15 | 7530 | `	SXUNUSED(apArg);` |
|     31 | 7531 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 | 7532 | `		sRef.pAttr = 0;` |
|    ! 0 | 7533 | `	}` |
|     31 | 7534 | `	if( sRef.pAttr && sRef.pAttr->pNativeValue && SySetUsed(&sRef.pAttr->aByteCode) < 1 ){` |
|      - | 7535 | `		/* A NATIVE property's default is a LITERAL, not byte-code — the same` |
|      - | 7536 | ``		 * record `new` materializes (PH7_NativeLiteralValue). Reading only the`` |
|      - | 7537 | `		 * byte-code answered NULL for every declared native default and raised` |
|      - | 7538 | `		 * php's no-default deprecation on a slot that hasDefaultValue() had just` |
|      - | 7539 | `		 * reported true for. */` |
|     21 | 7540 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     21 | 7541 | `		PH7_NativeLiteralValue(pCtx->pVm, sRef.pAttr->pNativeValue, &sValue);` |
|     21 | 7542 | `		ph7_result_value(pCtx, &sValue);` |
|     21 | 7543 | `		PH7_MemObjRelease(&sValue);` |
|     21 | 7544 | `		return PH7_OK;` |
|      - | 7545 | `	}` |
|     11 | 7546 | `	if( sRef.pAttr == 0 \|\| SySetUsed(&sRef.pAttr->aByteCode) < 1 ){` |
|      - | 7547 | `		/* php 8.5 deprecates the question when there is no default — an` |
|      - | 7548 | `		 * UNTYPED property still has one (null), a typed one without an` |
|      - | 7549 | `		 * initializer does not. */` |
|      5 | 7550 | `		if( sRef.pAttr == 0 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|      3 | 7551 | `			PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED,` |
|      - | 7552 | `				"ReflectionProperty::getDefaultValue() for a property without a default "` |
|      - | 7553 | `				"value is deprecated, use ReflectionProperty::hasDefaultValue() to check "` |
|      - | 7554 | `				"if the default value exists");` |
|      1 | 7555 | `		}` |
|      5 | 7556 | `		ph7_result_null(pCtx);` |
|      5 | 7557 | `		return PH7_OK;` |
|      - | 7558 | `	}` |
|      - | 7559 | `	/* Same evaluation path the VM uses for an omitted call argument */` |
|      7 | 7560 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|      7 | 7561 | `	VmLocalExec(pCtx->pVm, &sRef.pAttr->aByteCode, &sValue, FALSE);` |
|      7 | 7562 | `	ph7_result_value(pCtx, &sValue);` |
|      7 | 7563 | `	PH7_MemObjRelease(&sValue);` |
|      7 | 7564 | `	return PH7_OK;` |
|     16 | 7565 | `}` |
|      - | 7566 | `/* php 8.1 made every reflected member accessible; the setter is a no-op it kept. */` |
|    ! 0 | 7567 | `static int vm_builtin_ReflectionProperty_setAccessible(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 7568 | `{` |
|    ! 0 | 7569 | `	SXUNUSED(pCtx);` |
|    ! 0 | 7570 | `	SXUNUSED(nArg);` |
|    ! 0 | 7571 | `	SXUNUSED(apArg);` |
|    ! 0 | 7572 | `	return PH7_OK;` |
|    ! 0 | 7573 | `}` |
|      - | 7574 | `/* getValue()/setValue()/isInitialized() all need the same receiver check. */` |
|     54 | 7575 | `static sxi32 ReflectPropReceiver(ph7_context *pCtx, ReflectMemberRef *pRef,` |
|      - | 7576 | `	ph7_value *pObject, ph7_class_instance **ppThis, const char *zWho)` |
|      1 | 7577 | `{` |
|     55 | 7578 | `	*ppThis = 0;` |
|     27 | 7579 | `	SXUNUSED(pRef);` |
|     55 | 7580 | `	if( pObject && (pObject->iFlags & MEMOBJ_OBJ) ){` |
|     53 | 7581 | `		*ppThis = (ph7_class_instance *)pObject->x.pOther;` |
|     53 | 7582 | `		return PH7_OK;` |
|      - | 7583 | `	}` |
|      4 | 7584 | `	return PH7_VmThrowException(pCtx, "TypeError",` |
|      - | 7585 | `		"ReflectionProperty::%s(): Argument #1 ($object) must be provided for instance properties",` |
|      1 | 7586 | `		zWho);` |
|     28 | 7587 | `}` |
|     36 | 7588 | `static int vm_builtin_ReflectionProperty_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 7589 | `{` |
|      - | 7590 | `	ReflectMemberRef sRef;` |
|      - | 7591 | `	ph7_class_instance *pObj;` |
|      - | 7592 | `	VmClassAttr *pVmAttr;` |
|      - | 7593 | `	ph7_value *pValue;` |
|      - | 7594 | `	sxi32 rc;` |
|     38 | 7595 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 | 7596 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7597 | `		return PH7_OK;` |
|      - | 7598 | `	}` |
|     38 | 7599 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|      8 | 7600 | `		return ReflectStaticSlotRead(pCtx, sRef.pClass, sRef.pAttr);` |
|      - | 7601 | `	}` |
|     31 | 7602 | `	rc = ReflectPropReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pObj, "getValue");` |
|     31 | 7603 | `	if( rc != PH7_OK ){` |
|      3 | 7604 | `		return rc;` |
|      - | 7605 | `	}` |
|     29 | 7606 | `	pVmAttr = ReflectInstanceAttr(pObj, sRef.zName, sRef.nName);` |
|     29 | 7607 | `	if( pVmAttr == 0 ){` |
|      - | 7608 | `		/* No slot: a class whose properties are its own handlers answers here,` |
|      - | 7609 | ``		 * as php's does -- Reflection reads a PDORow's `queryString` through`` |
|      - | 7610 | ``		 * read_property exactly as `$row->queryString` does. */`` |
|      - | 7611 | `		PH7_NativePropCtx sNat;` |
|      - | 7612 | `		SyString sNatName;` |
|      - | 7613 | `		ph7_value sNatVal;` |
|      3 | 7614 | `		SyStringInitFromBuf(&sNatName, sRef.zName, (sxu32)sRef.nName);` |
|      3 | 7615 | `		PH7_MemObjInit(pCtx->pVm, &sNatVal);` |
|      3 | 7616 | `		if( PH7_ClassNativePropAsk(pObj, &sNat, PH7_NATIVE_PROP_READ, &sNatName, &sNatVal) ){` |
|      3 | 7617 | `			if( sNat.zThrowClass ){` |
|    ! 0 | 7618 | `				PH7_MemObjRelease(&sNatVal);` |
|    ! 0 | 7619 | `				return PH7_VmThrowException(pCtx, sNat.zThrowClass, "%s", sNat.zThrowMsg);` |
|      - | 7620 | `			}` |
|      3 | 7621 | `			ph7_result_value(pCtx, &sNatVal);` |
|      3 | 7622 | `			PH7_MemObjRelease(&sNatVal);` |
|      3 | 7623 | `			return PH7_OK;` |
|      - | 7624 | `		}` |
|    ! 0 | 7625 | `		PH7_MemObjRelease(&sNatVal);` |
|    ! 0 | 7626 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7627 | `		return PH7_OK;` |
|      - | 7628 | `	}` |
|     27 | 7629 | `	if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|      3 | 7630 | `		ph7_class *pDecl = pVmAttr->pAttr && pVmAttr->pAttr->pDeclClass` |
|      3 | 7631 | `			? pVmAttr->pAttr->pDeclClass : pObj->pClass;` |
|      4 | 7632 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      - | 7633 | `			"Typed property %z::$%.*s must not be accessed before initialization",` |
|      1 | 7634 | `			&pDecl->sName, sRef.nName, sRef.zName);` |
|      - | 7635 | `	}` |
|     25 | 7636 | `	pValue = PH7_ClassInstanceExtractAttrValue(pObj, pVmAttr);` |
|     25 | 7637 | `	if( pValue ){` |
|     25 | 7638 | `		ph7_result_value(pCtx, pValue);` |
|     13 | 7639 | `	}else{` |
|    ! 0 | 7640 | `		ph7_result_null(pCtx);` |
|      - | 7641 | `	}` |
|     25 | 7642 | `	return PH7_OK;` |
|     20 | 7643 | `}` |
|     18 | 7644 | `static int vm_builtin_ReflectionProperty_setValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 7645 | `{` |
|      - | 7646 | `	ReflectMemberRef sRef;` |
|      - | 7647 | `	ph7_class_instance *pObj;` |
|      - | 7648 | `	VmClassAttr *pVmAttr;` |
|      - | 7649 | `	ph7_value *pSlot;` |
|      - | 7650 | `	sxi32 rc;` |
|     20 | 7651 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| nArg < 1 ){` |
|    ! 0 | 7652 | `		return PH7_OK;` |
|      - | 7653 | `	}` |
|     20 | 7654 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|      - | 7655 | `		/* php's one-argument spelling for a static: setValue($value). Two` |
|      - | 7656 | `		 * arguments, or one OBJECT, means the instance form was used and the` |
|      - | 7657 | `		 * value is the second. */` |
|      6 | 7658 | `		ph7_value *pVal = apArg[0];` |
|      6 | 7659 | `		if( nArg > 1 ){` |
|      6 | 7660 | `			pVal = apArg[1];` |
|      2 | 7661 | `		}else if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|    ! 0 | 7662 | `			return PH7_OK;` |
|      - | 7663 | `		}` |
|      6 | 7664 | `		return ReflectStaticSlotWrite(pCtx, sRef.pClass, sRef.pAttr, pVal);` |
|      - | 7665 | `	}` |
|     15 | 7666 | `	rc = ReflectPropReceiver(pCtx, &sRef, apArg[0], &pObj, "setValue");` |
|     15 | 7667 | `	if( rc != PH7_OK ){` |
|    ! 0 | 7668 | `		return rc;` |
|      - | 7669 | `	}` |
|     15 | 7670 | `	pVmAttr = ReflectInstanceAttr(pObj, sRef.zName, sRef.nName);` |
|     15 | 7671 | `	if( pVmAttr == 0 ){` |
|      - | 7672 | `		/* No slot: the write handler answers, and for a class that has one the` |
|      - | 7673 | `		 * answer is its own refusal (php's PDORow refuses Reflection's write` |
|      - | 7674 | ``		 * with the same sentence `$row->p = 1` takes). */`` |
|      - | 7675 | `		PH7_NativePropCtx sNat;` |
|      - | 7676 | `		SyString sNatName;` |
|      - | 7677 | `		ph7_value sNatVal;` |
|      3 | 7678 | `		SyStringInitFromBuf(&sNatName, sRef.zName, (sxu32)sRef.nName);` |
|      3 | 7679 | `		PH7_MemObjInit(pCtx->pVm, &sNatVal);` |
|      2 | 7680 | `		if( PH7_ClassNativePropAsk(pObj, &sNat, PH7_NATIVE_PROP_WRITE, &sNatName, &sNatVal)` |
|      3 | 7681 | `		 && sNat.zThrowClass ){` |
|      3 | 7682 | `			PH7_MemObjRelease(&sNatVal);` |
|      3 | 7683 | `			return PH7_VmThrowException(pCtx, sNat.zThrowClass, "%s", sNat.zThrowMsg);` |
|      - | 7684 | `		}` |
|    ! 0 | 7685 | `		PH7_MemObjRelease(&sNatVal);` |
|    ! 0 | 7686 | `		return PH7_OK;` |
|      - | 7687 | `	}` |
|     13 | 7688 | `	if( pVmAttr->pAttr && (pVmAttr->iState & VM_CLASS_ATTR_RDONLY) ){` |
|      - | 7689 | `		/* php's read-only handler refuses Reflection's write with the sentence` |
|      - | 7690 | ``		 * `$stmt->queryString = 'x'` takes. */`` |
|      3 | 7691 | `		return VmThrowNativeReadOnly(pCtx->pVm, pVmAttr->pAttr);` |
|      - | 7692 | `	}` |
|      - | 7693 | `	{` |
|     11 | 7694 | `		ph7_value *pVal = nArg > 1 ? apArg[1] : 0;` |
|      - | 7695 | `		ph7_value sNull;` |
|     11 | 7696 | `		PH7_MemObjInit(pCtx->pVm, &sNull);` |
|     11 | 7697 | `		if( pVal == 0 ){` |
|    ! 0 | 7698 | `			pVal = &sNull;` |
|    ! 0 | 7699 | `		}` |
|     11 | 7700 | `		rc = ReflectEnforceStore(pCtx, pVmAttr->nIdx, pVal);` |
|     11 | 7701 | `		if( rc != SXRET_OK ){` |
|      5 | 7702 | `			PH7_MemObjRelease(&sNull);` |
|      5 | 7703 | `			return rc;` |
|      - | 7704 | `		}` |
|      7 | 7705 | `		pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|      7 | 7706 | `		if( pSlot ){` |
|      7 | 7707 | `			PH7_MemObjStore(pVal, pSlot);` |
|      7 | 7708 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      3 | 7709 | `		}` |
|      7 | 7710 | `		PH7_MemObjRelease(&sNull);` |
|      - | 7711 | `	}` |
|      7 | 7712 | `	return PH7_OK;` |
|     11 | 7713 | `}` |
|     16 | 7714 | `static int vm_builtin_ReflectionProperty_isInitialized(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 7715 | `{` |
|      - | 7716 | `	ReflectMemberRef sRef;` |
|      - | 7717 | `	ph7_class_instance *pObj;` |
|      - | 7718 | `	VmClassAttr *pVmAttr;` |
|      - | 7719 | `	sxi32 rc;` |
|     18 | 7720 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 | 7721 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7722 | `		return PH7_OK;` |
|      - | 7723 | `	}` |
|     18 | 7724 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|      - | 7725 | `		SyHashEntry *pSlot;` |
|      8 | 7726 | `		rc = ReflectMaterializeStatics(pCtx, sRef.pClass);` |
|      8 | 7727 | `		if( rc != SXRET_OK ){` |
|      5 | 7728 | `			return rc;` |
|      - | 7729 | `		}` |
|      3 | 7730 | `		pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&sRef.pAttr->nIdx, sizeof(sxu32));` |
|      5 | 7731 | `		ph7_result_bool(pCtx, pSlot == 0` |
|      2 | 7732 | `			\|\| (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) == 0);` |
|      3 | 7733 | `		return PH7_OK;` |
|      - | 7734 | `	}` |
|     11 | 7735 | `	rc = ReflectPropReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pObj, "isInitialized");` |
|     11 | 7736 | `	if( rc != PH7_OK ){` |
|    ! 0 | 7737 | `		return rc;` |
|      - | 7738 | `	}` |
|     11 | 7739 | `	pVmAttr = ReflectInstanceAttr(pObj, sRef.zName, sRef.nName);` |
|     11 | 7740 | `	ph7_result_bool(pCtx, pVmAttr != 0 && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0);` |
|     11 | 7741 | `	return PH7_OK;` |
|     10 | 7742 | `}` |
|      - | 7743 | `/*` |
|      - | 7744 | `` * The property HOOKS (php 8.4). PHL compiles `public $p { get => ...; }` into`` |
|      - | 7745 | `` * synthesized methods named `__phl_hook_get_NAME` / `__phl_hook_set_NAME`, so`` |
|      - | 7746 | ` * the reflector php answers is a ReflectionMethod over one of those.` |
|      - | 7747 | ` */` |
|     30 | 7748 | `static int ReflectHookMethod(ph7_context *pCtx, ReflectMemberRef *pRef, int bSet,` |
|      - | 7749 | `	ph7_class_instance **ppOut)` |
|      3 | 7750 | `{` |
|      - | 7751 | `	char zName[128];` |
|      - | 7752 | `	ph7_value sClass, sName;` |
|      - | 7753 | `	ph7_value *apCtor[2];` |
|      - | 7754 | `	ph7_class *pDecl;` |
|      - | 7755 | `	sxi32 rc;` |
|      - | 7756 | `	int nName;` |
|     33 | 7757 | `	*ppOut = 0;` |
|     33 | 7758 | `	if( pRef->pAttr == 0 ){` |
|    ! 0 | 7759 | `		return PH7_OK;` |
|      - | 7760 | `	}` |
|     33 | 7761 | `	if( (pRef->pAttr->iFlags & (bSet ? PH7_CLASS_ATTR_HOOK_SET : PH7_CLASS_ATTR_HOOK_GET)) == 0 ){` |
|     15 | 7762 | `		return PH7_OK;` |
|      - | 7763 | `	}` |
|     21 | 7764 | `	pDecl = ReflectMemberDecl(pRef);` |
|     30 | 7765 | `	nName = SyBufferFormat(zName, sizeof(zName), "%s%.*s",` |
|      9 | 7766 | `		bSet ? "__phl_hook_set_" : "__phl_hook_get_", pRef->nName, pRef->zName);` |
|     21 | 7767 | `	PH7_MemObjInit(pCtx->pVm, &sClass);` |
|     21 | 7768 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|     21 | 7769 | `	ph7_value_string(&sClass, SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|     21 | 7770 | `	ph7_value_string(&sName, zName, nName);` |
|     21 | 7771 | `	apCtor[0] = &sClass;` |
|     21 | 7772 | `	apCtor[1] = &sName;` |
|     21 | 7773 | `	*ppOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|     21 | 7774 | `	PH7_MemObjRelease(&sClass);` |
|     21 | 7775 | `	PH7_MemObjRelease(&sName);` |
|     21 | 7776 | `	return rc;` |
|     18 | 7777 | `}` |
|     12 | 7778 | `static int vm_builtin_ReflectionProperty_getHooks(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 7779 | `{` |
|      - | 7780 | `	ReflectMemberRef sRef;` |
|     15 | 7781 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - | 7782 | `	int iHook;` |
|      6 | 7783 | `	SXUNUSED(nArg);` |
|      6 | 7784 | `	SXUNUSED(apArg);` |
|     15 | 7785 | `	if( pOut == 0 ){` |
|    ! 0 | 7786 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7787 | `	}` |
|     15 | 7788 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 | 7789 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 | 7790 | `		return PH7_OK;` |
|      - | 7791 | `	}` |
|     39 | 7792 | `	for( iHook = 0 ; iHook < 2 ; iHook++ ){` |
|     27 | 7793 | `		ph7_class_instance *pMeth = 0;` |
|      - | 7794 | `		ph7_value sVal, *pKey;` |
|     27 | 7795 | `		sxi32 rc = ReflectHookMethod(pCtx, &sRef, iHook, &pMeth);` |
|     27 | 7796 | `		if( rc != PH7_OK ){` |
|    ! 0 | 7797 | `			return rc;` |
|      - | 7798 | `		}` |
|     27 | 7799 | `		if( pMeth == 0 ){` |
|     13 | 7800 | `			continue;` |
|      - | 7801 | `		}` |
|     17 | 7802 | `		pKey = ph7_context_new_scalar(pCtx);` |
|     17 | 7803 | `		if( pKey == 0 ){` |
|    ! 0 | 7804 | `			PH7_ClassInstanceUnref(pMeth);` |
|    ! 0 | 7805 | `			break;` |
|      - | 7806 | `		}` |
|     17 | 7807 | `		ph7_value_string(pKey, iHook ? "set" : "get", 3);` |
|     17 | 7808 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     17 | 7809 | `		sVal.x.pOther = pMeth;` |
|     17 | 7810 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|     17 | 7811 | `		ph7_array_add_elem(pOut, pKey, &sVal);   /* takes its own reference */` |
|     17 | 7812 | `		PH7_ClassInstanceUnref(pMeth);` |
|     10 | 7813 | `	}` |
|     15 | 7814 | `	ph7_result_value(pCtx, pOut);` |
|     15 | 7815 | `	return PH7_OK;` |
|      9 | 7816 | `}` |
|      - | 7817 | ``/* `get`/`set` out of a PropertyHookType case (or the bare string php also takes). */`` |
|     12 | 7818 | `static int ReflectHookKind(ph7_value *pArg)` |
|      1 | 7819 | `{` |
|      - | 7820 | `	const char *z;` |
|      - | 7821 | `	int n;` |
|     13 | 7822 | `	if( pArg == 0 ){` |
|    ! 0 | 7823 | `		return -1;` |
|      - | 7824 | `	}` |
|     13 | 7825 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|     13 | 7826 | `		ph7_class_instance *pCase = (ph7_class_instance *)pArg->x.pOther;` |
|     13 | 7827 | `		ph7_value *pVal = PH7_NativeAttr(pCase, "value");` |
|     13 | 7828 | `		if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 7829 | `			return -1;` |
|      - | 7830 | `		}` |
|     13 | 7831 | `		z = (const char *)SyBlobData(&pVal->sBlob);` |
|     13 | 7832 | `		n = (int)SyBlobLength(&pVal->sBlob);` |
|      7 | 7833 | `	}else{` |
|    ! 0 | 7834 | `		z = ph7_value_to_string(pArg, &n);` |
|      - | 7835 | `	}` |
|     13 | 7836 | `	if( n == 3 && SyMemcmp(z, "get", 3) == 0 ){` |
|      7 | 7837 | `		return 0;` |
|      - | 7838 | `	}` |
|      7 | 7839 | `	if( n == 3 && SyMemcmp(z, "set", 3) == 0 ){` |
|      7 | 7840 | `		return 1;` |
|      - | 7841 | `	}` |
|    ! 0 | 7842 | `	return -1;` |
|      7 | 7843 | `}` |
|      6 | 7844 | `static int vm_builtin_ReflectionProperty_hasHook(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7845 | `{` |
|      - | 7846 | `	ReflectMemberRef sRef;` |
|      7 | 7847 | `	int iHook = ReflectHookKind(nArg > 0 ? apArg[0] : 0);` |
|      7 | 7848 | `	int bYes = 0;` |
|      7 | 7849 | `	if( iHook >= 0 && ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|      7 | 7850 | `		bYes = (sRef.pAttr->iFlags & (iHook ? PH7_CLASS_ATTR_HOOK_SET : PH7_CLASS_ATTR_HOOK_GET)) != 0;` |
|      3 | 7851 | `	}` |
|      7 | 7852 | `	ph7_result_bool(pCtx, bYes);` |
|      7 | 7853 | `	return PH7_OK;` |
|      1 | 7854 | `}` |
|      6 | 7855 | `static int vm_builtin_ReflectionProperty_getHook(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7856 | `{` |
|      - | 7857 | `	ReflectMemberRef sRef;` |
|      7 | 7858 | `	ph7_class_instance *pMeth = 0;` |
|      7 | 7859 | `	int iHook = ReflectHookKind(nArg > 0 ? apArg[0] : 0);` |
|      - | 7860 | `	sxi32 rc;` |
|      7 | 7861 | `	if( iHook < 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 | 7862 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7863 | `		return PH7_OK;` |
|      - | 7864 | `	}` |
|      7 | 7865 | `	rc = ReflectHookMethod(pCtx, &sRef, iHook, &pMeth);` |
|      7 | 7866 | `	if( rc != PH7_OK ){` |
|    ! 0 | 7867 | `		return rc;` |
|      - | 7868 | `	}` |
|      7 | 7869 | `	if( pMeth == 0 ){` |
|      3 | 7870 | `		ph7_result_null(pCtx);` |
|      3 | 7871 | `		return PH7_OK;` |
|      - | 7872 | `	}` |
|      5 | 7873 | `	return ReflectResultObject(pCtx, pMeth);` |
|      4 | 7874 | `}` |
|      6 | 7875 | `static int vm_builtin_ReflectionProperty_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7876 | `{` |
|      7 | 7877 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 7878 | `	ReflectMemberRef sRef;` |
|      - | 7879 | `	ph7_value sTarget;` |
|      - | 7880 | `	const char *zClass;` |
|      - | 7881 | `	int nClass, rc;` |
|      7 | 7882 | `	if( pThis == 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 7883 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 | 7884 | `		return PH7_OK;` |
|      - | 7885 | `	}` |
|      7 | 7886 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|      7 | 7887 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      7 | 7888 | `	ph7_value_string(&sTarget, zClass, nClass);` |
|      - | 7889 | `	/* 8 = Attribute::TARGET_PROPERTY */` |
|     10 | 7890 | `	rc = ReflectBuildAttrs(pCtx, &sRef.pAttr->aAttrs, "attr", &sTarget,` |
|      3 | 7891 | `		sRef.zName, sRef.nName, 0, 8, nArg, apArg);` |
|      7 | 7892 | `	PH7_MemObjRelease(&sTarget);` |
|      7 | 7893 | `	return rc;` |
|      4 | 7894 | `}` |
|      4 | 7895 | `static int vm_builtin_ReflectionProperty_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7896 | `{` |
|      2 | 7897 | `	SXUNUSED(nArg);` |
|      2 | 7898 | `	SXUNUSED(apArg);` |
|      5 | 7899 | `	return ReflectExportPropSelf(pCtx);` |
|      1 | 7900 | `}` |
|      - | 7901 | `/* ---- ReflectionClassConstant ---- */` |
|      - | 7902 | `/* ReflectionClassConstant::__construct(object\|string $class, string $constant) */` |
|     72 | 7903 | `static int vm_builtin_ReflectionClassConstant_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7904 | `{` |
|     73 | 7905 | `	ph7_vm *pVm = pCtx->pVm;` |
|     73 | 7906 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     73 | 7907 | `	ph7_class *pClass, *pDecl = 0;` |
|      - | 7908 | `	const char *zConst;` |
|      - | 7909 | `	int nConst;` |
|      - | 7910 | `	SySet aMembers;` |
|      - | 7911 | `	sxu32 n;` |
|     73 | 7912 | `	int bFound = 0;` |
|     73 | 7913 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 | 7914 | `		return PH7_OK;` |
|      - | 7915 | `	}` |
|     73 | 7916 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|     73 | 7917 | `	if( pClass == 0 ){` |
|      - | 7918 | `		const char *zName;` |
|      - | 7919 | `		int nName;` |
|    ! 0 | 7920 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|    ! 0 | 7921 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 | 7922 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      - | 7923 | `	}` |
|     73 | 7924 | `	zConst = ph7_value_to_string(apArg[1], &nConst);` |
|     73 | 7925 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|     73 | 7926 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|    357 | 7927 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    351 | 7928 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|    351 | 7929 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zConst, nConst) ){` |
|     67 | 7930 | `			pDecl = pM->pDecl ? pM->pDecl : pClass;` |
|     67 | 7931 | `			bFound = 1;` |
|     67 | 7932 | `			break;` |
|      - | 7933 | `		}` |
|    143 | 7934 | `	}` |
|     73 | 7935 | `	SySetRelease(&aMembers);` |
|     73 | 7936 | `	if( !bFound ){` |
|     10 | 7937 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 | 7938 | `			"Constant %z::%.*s does not exist", &pClass->sName, nConst, zConst);` |
|      - | 7939 | `	}` |
|      - | 7940 | `	/* php's $class is the DECLARING class, not the one asked about. */` |
|     67 | 7941 | `	pClass = pDecl;` |
|    100 | 7942 | `	PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pClass->sName),` |
|     66 | 7943 | `		(int)SyStringLength(&pClass->sName));` |
|     67 | 7944 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", zConst, nConst);` |
|     67 | 7945 | `	return PH7_OK;` |
|     37 | 7946 | `}` |
|     22 | 7947 | `static int vm_builtin_ReflectionClassConstant_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7948 | `{` |
|      - | 7949 | `	ReflectMemberRef sRef;` |
|      - | 7950 | `	ph7_value *pVal;` |
|      - | 7951 | `	sxi32 rc;` |
|     11 | 7952 | `	SXUNUSED(nArg);` |
|     11 | 7953 | `	SXUNUSED(apArg);` |
|     23 | 7954 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 7955 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7956 | `		return PH7_OK;` |
|      - | 7957 | `	}` |
|     23 | 7958 | `	rc = ReflectConstSlot(pCtx, sRef.pClass, sRef.pAttr, &pVal);` |
|     23 | 7959 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 7960 | `		return rc;` |
|      - | 7961 | `	}` |
|     23 | 7962 | `	if( pVal ){` |
|     23 | 7963 | `		ph7_result_value(pCtx, pVal);` |
|     12 | 7964 | `	}else{` |
|    ! 0 | 7965 | `		ph7_result_null(pCtx);` |
|      - | 7966 | `	}` |
|     23 | 7967 | `	return PH7_OK;` |
|     12 | 7968 | `}` |
|     24 | 7969 | `static int ReflectConstFlag(ph7_context *pCtx, int iWhat)` |
|      1 | 7970 | `{` |
|      - | 7971 | `	ReflectMemberRef sRef;` |
|     25 | 7972 | `	int bYes = 0;` |
|     25 | 7973 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) && sRef.pAttr ){` |
|     25 | 7974 | `		ph7_class_attr *pAttr = sRef.pAttr;` |
|     25 | 7975 | `		switch( iWhat ){` |
|      3 | 7976 | `		case 0: bYes = pAttr->iProtection == PH7_CLASS_PROT_PUBLIC; break;` |
|      3 | 7977 | `		case 1: bYes = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE; break;` |
|      3 | 7978 | `		case 2: bYes = pAttr->iProtection == PH7_CLASS_PROT_PROTECTED; break;` |
|      3 | 7979 | `		case 3: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0; break;` |
|     13 | 7980 | `		case 4: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0; break;` |
|      3 | 7981 | `		case 5: bYes = ReflectHasDeprecated(&pAttr->aAttrs); break;` |
|      3 | 7982 | `		default: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0; break;` |
|      - | 7983 | `		}` |
|     12 | 7984 | `	}` |
|     25 | 7985 | `	ph7_result_bool(pCtx, bYes);` |
|     25 | 7986 | `	return PH7_OK;` |
|      1 | 7987 | `}` |
|      - | 7988 | `#define REFLECT_CONST_FLAG(NAME,WHAT) \` |
|      - | 7989 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 7990 | `	{ \` |
|      - | 7991 | `		SXUNUSED(nArg); \` |
|      - | 7992 | `		SXUNUSED(apArg); \` |
|      - | 7993 | `		return ReflectConstFlag(pCtx, WHAT); \` |
|      - | 7994 | `	}` |
|      3 | 7995 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isPublic, 0)` |
|      3 | 7996 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isPrivate, 1)` |
|      3 | 7997 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isProtected, 2)` |
|      3 | 7998 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isFinal, 3)` |
|     13 | 7999 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isEnumCase, 4)` |
|      3 | 8000 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isDeprecated, 5)` |
|      3 | 8001 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_hasType, 6)` |
|      - | 8002 |  |
|      8 | 8003 | `static int vm_builtin_ReflectionClassConstant_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8004 | `{` |
|      - | 8005 | `	ReflectMemberRef sRef;` |
|      4 | 8006 | `	SXUNUSED(nArg);` |
|      4 | 8007 | `	SXUNUSED(apArg);` |
|      9 | 8008 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 8009 | `		ph7_result_int(pCtx, 0);` |
|    ! 0 | 8010 | `		return PH7_OK;` |
|      - | 8011 | `	}` |
|      9 | 8012 | `	ph7_result_int64(pCtx, ReflectConstModifiers(sRef.pAttr));` |
|      9 | 8013 | `	return PH7_OK;` |
|      5 | 8014 | `}` |
|      4 | 8015 | `static int vm_builtin_ReflectionClassConstant_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8016 | `{` |
|      - | 8017 | `	ReflectMemberRef sRef;` |
|      2 | 8018 | `	SXUNUSED(nArg);` |
|      2 | 8019 | `	SXUNUSED(apArg);` |
|      5 | 8020 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) ){` |
|    ! 0 | 8021 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8022 | `		return PH7_OK;` |
|      - | 8023 | `	}` |
|      5 | 8024 | `	return ReflectResultClassOf(pCtx, ReflectMemberDecl(&sRef));` |
|      3 | 8025 | `}` |
|      2 | 8026 | `static int vm_builtin_ReflectionClassConstant_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8027 | `{` |
|      - | 8028 | `	ReflectMemberRef sRef;` |
|      1 | 8029 | `	SXUNUSED(nArg);` |
|      1 | 8030 | `	SXUNUSED(apArg);` |
|      2 | 8031 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) && sRef.pAttr` |
|      3 | 8032 | `	 && SyStringLength(&sRef.pAttr->sDoc) > 0 ){` |
|      4 | 8033 | `		ph7_result_string(pCtx, SyStringData(&sRef.pAttr->sDoc),` |
|      2 | 8034 | `			(int)SyStringLength(&sRef.pAttr->sDoc));` |
|      2 | 8035 | `	}else{` |
|    ! 0 | 8036 | `		ph7_result_bool(pCtx, 0);` |
|      - | 8037 | `	}` |
|      3 | 8038 | `	return PH7_OK;` |
|      1 | 8039 | `}` |
|      2 | 8040 | `static int vm_builtin_ReflectionClassConstant_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8041 | `{` |
|      - | 8042 | `	ReflectMemberRef sRef;` |
|      1 | 8043 | `	SXUNUSED(nArg);` |
|      1 | 8044 | `	SXUNUSED(apArg);` |
|      2 | 8045 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0` |
|      2 | 8046 | `	 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|      3 | 8047 | `	 \|\| SyStringLength(&sRef.pAttr->sTypeName) < 1 ){` |
|    ! 0 | 8048 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8049 | `		return PH7_OK;` |
|      - | 8050 | `	}` |
|      4 | 8051 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx,` |
|      2 | 8052 | `		SyStringData(&sRef.pAttr->sTypeName), (int)SyStringLength(&sRef.pAttr->sTypeName)));` |
|      2 | 8053 | `}` |
|      2 | 8054 | `static int vm_builtin_ReflectionClassConstant_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8055 | `{` |
|      3 | 8056 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 8057 | `	ReflectMemberRef sRef;` |
|      - | 8058 | `	ph7_value sTarget;` |
|      - | 8059 | `	const char *zClass;` |
|      - | 8060 | `	int nClass, rc;` |
|      3 | 8061 | `	if( pThis == 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 8062 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 | 8063 | `		return PH7_OK;` |
|      - | 8064 | `	}` |
|      3 | 8065 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|      3 | 8066 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      3 | 8067 | `	ph7_value_string(&sTarget, zClass, nClass);` |
|      - | 8068 | `	/* 16 = Attribute::TARGET_CLASS_CONSTANT */` |
|      4 | 8069 | `	rc = ReflectBuildAttrs(pCtx, &sRef.pAttr->aAttrs, "attr", &sTarget,` |
|      1 | 8070 | `		sRef.zName, sRef.nName, 0, 16, nArg, apArg);` |
|      3 | 8071 | `	PH7_MemObjRelease(&sTarget);` |
|      3 | 8072 | `	return rc;` |
|      2 | 8073 | `}` |
|      6 | 8074 | `static int vm_builtin_ReflectionClassConstant_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8075 | `{` |
|      3 | 8076 | `	SXUNUSED(nArg);` |
|      3 | 8077 | `	SXUNUSED(apArg);` |
|      7 | 8078 | `	return ReflectExportConstSelf(pCtx);` |
|      1 | 8079 | `}` |
|      - | 8080 | `/*` |
|      - | 8081 | ` * Declare chunk 3. Called from PH7_VmInstallReflection() where it used to be` |
|      - | 8082 | ` * compiled — after chunks 1 and 2, whose ReflectionClass and ReflectionMethod` |
|      - | 8083 | ` * these answer, and after PropertyHookType, which hasHook()/getHook() declare.` |
|      - | 8084 | ` *` |
|      - | 8085 | ` * The method tables are in php's own DECLARATION order.` |
|      - | 8086 | ` */` |
|   5740 | 8087 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionMember(ph7_vm *pVm)` |
|      5 | 8088 | `{` |
|      - | 8089 | `	static const PH7_NativePropDef aMemberProp[] = {` |
|      - | 8090 | `		{ "name",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 8091 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 8092 | `	};` |
|      - | 8093 | `	static const PH7_NativePropDef aPropProp[] = {` |
|      - | 8094 | `		{ "name",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 8095 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 8096 | `		/* PHL-only: the instance a DYNAMIC property was reached through (§7.4 (e)) */` |
|      - | 8097 | `		{ RP_DYNOBJ, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 8098 | `	};` |
|      - | 8099 | `	static const PH7_NativeConstDef aPropConst[] = {` |
|      - | 8100 | `		{ "IS_STATIC",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,   0, 0.0 },` |
|      - | 8101 | `		{ "IS_READONLY",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 128,  0, 0.0 },` |
|      - | 8102 | `		{ "IS_PUBLIC",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,    0, 0.0 },` |
|      - | 8103 | `		{ "IS_PROTECTED",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,    0, 0.0 },` |
|      - | 8104 | `		{ "IS_PRIVATE",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,    0, 0.0 },` |
|      - | 8105 | `		{ "IS_ABSTRACT",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64,   0, 0.0 },` |
|      - | 8106 | `		{ "IS_PROTECTED_SET", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2048, 0, 0.0 },` |
|      - | 8107 | `		{ "IS_PRIVATE_SET",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4096, 0, 0.0 },` |
|      - | 8108 | `		{ "IS_VIRTUAL",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 512,  0, 0.0 },` |
|      - | 8109 | `		{ "IS_FINAL",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32,   0, 0.0 },` |
|      - | 8110 | `	};` |
|      - | 8111 | `	static const PH7_NativeMethodDef aPropMethod[] = {` |
|      - | 8112 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - | 8113 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $property", "",` |
|      - | 8114 | `		  vm_builtin_ReflectionProperty_construct },` |
|      - | 8115 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionProperty_toString },` |
|      - | 8116 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionProperty_getName },` |
|      - | 8117 | `		{ "getMangledName", PH7_MOD_PUBLIC, "", "string",` |
|      - | 8118 | `		  vm_builtin_ReflectionProperty_getMangledName },` |
|      - | 8119 | `		{ "getValue",    PH7_MOD_PUBLIC, "?object $object = null", "@mixed",` |
|      - | 8120 | `		  vm_builtin_ReflectionProperty_getValue },` |
|      - | 8121 | `		{ "setValue",    PH7_MOD_PUBLIC, "mixed $objectOrValue, mixed $value = ?", "@void",` |
|      - | 8122 | `		  vm_builtin_ReflectionProperty_setValue },` |
|      - | 8123 | `		{ "getRawValue", PH7_MOD_PUBLIC, "object $object", "mixed",` |
|      - | 8124 | `		  vm_builtin_ReflectionProperty_getValue },` |
|      - | 8125 | `		{ "setRawValue", PH7_MOD_PUBLIC, "object $object, mixed $value", "void",` |
|      - | 8126 | `		  vm_builtin_ReflectionProperty_setValue },` |
|      - | 8127 | `		/* On a non-lazy object — the only kind PHL has — php's two lazy writers` |
|      - | 8128 | `		 * are an ordinary raw write and a no-op. */` |
|      - | 8129 | `		{ "setRawValueWithoutLazyInitialization", PH7_MOD_PUBLIC,` |
|      - | 8130 | `		  "object $object, mixed $value", "void", vm_builtin_ReflectionProperty_setValue },` |
|      - | 8131 | `		{ "skipLazyInitialization", PH7_MOD_PUBLIC, "object $object", "void",` |
|      - | 8132 | `		  vm_builtin_ReflectionProperty_noop },` |
|      - | 8133 | `		{ "isLazy",      PH7_MOD_PUBLIC, "object $object", "bool",` |
|      - | 8134 | `		  vm_builtin_ReflectionProperty_false },` |
|      - | 8135 | `		{ "isInitialized", PH7_MOD_PUBLIC, "?object $object = null", "@bool",` |
|      - | 8136 | `		  vm_builtin_ReflectionProperty_isInitialized },` |
|      - | 8137 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isPublic },` |
|      - | 8138 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isPrivate },` |
|      - | 8139 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isProtected },` |
|      - | 8140 | `		{ "isPrivateSet", PH7_MOD_PUBLIC, "", "bool",` |
|      - | 8141 | `		  vm_builtin_ReflectionProperty_isPrivateSet },` |
|      - | 8142 | `		{ "isProtectedSet", PH7_MOD_PUBLIC, "", "bool",` |
|      - | 8143 | `		  vm_builtin_ReflectionProperty_isProtectedSet },` |
|      - | 8144 | `		{ "isStatic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isStatic },` |
|      - | 8145 | `		{ "isReadOnly",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isReadOnly },` |
|      - | 8146 | `		{ "isDefault",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isDefault },` |
|      - | 8147 | `		{ "isDynamic",   PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isDynamic },` |
|      - | 8148 | `		/* PHL has no abstract properties: the modifier only exists on an` |
|      - | 8149 | `		 * interface's hooked property stub, which PHL does not model. */` |
|      - | 8150 | `		{ "isAbstract",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_false },` |
|      - | 8151 | `		{ "isVirtual",   PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isVirtual },` |
|      - | 8152 | `		{ "isPromoted",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isPromoted },` |
|      - | 8153 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionProperty_getModifiers },` |
|      - | 8154 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|      - | 8155 | `		  vm_builtin_ReflectionProperty_getDeclaringClass },` |
|      - | 8156 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionProperty_getDocComment },` |
|      - | 8157 | `		{ "setAccessible", PH7_MOD_PUBLIC, "bool $accessible", "@void",` |
|      - | 8158 | `		  vm_builtin_ReflectionProperty_setAccessible },` |
|      - | 8159 | `		{ "getType",     PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionProperty_getType },` |
|      - | 8160 | `		/* php's settable type differs from the declared one only for a hooked` |
|      - | 8161 | ``		 * property with a widening `set` — which PHL does not model. */`` |
|      - | 8162 | `		{ "getSettableType", PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|      - | 8163 | `		  vm_builtin_ReflectionProperty_getType },` |
|      - | 8164 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_hasType },` |
|      - | 8165 | `		{ "hasDefaultValue", PH7_MOD_PUBLIC, "", "bool",` |
|      - | 8166 | `		  vm_builtin_ReflectionProperty_hasDefaultValue },` |
|      - | 8167 | `		{ "getDefaultValue", PH7_MOD_PUBLIC, "", "@mixed",` |
|      - | 8168 | `		  vm_builtin_ReflectionProperty_getDefaultValue },` |
|      - | 8169 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - | 8170 | `		  vm_builtin_ReflectionProperty_getAttributes },` |
|      - | 8171 | `		{ "hasHooks",    PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_hasHooks },` |
|      - | 8172 | `		{ "getHooks",    PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionProperty_getHooks },` |
|      - | 8173 | `		{ "hasHook",     PH7_MOD_PUBLIC, "PropertyHookType $type", "bool",` |
|      - | 8174 | `		  vm_builtin_ReflectionProperty_hasHook },` |
|      - | 8175 | `		{ "getHook",     PH7_MOD_PUBLIC, "PropertyHookType $type", "?ReflectionMethod",` |
|      - | 8176 | `		  vm_builtin_ReflectionProperty_getHook },` |
|      - | 8177 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_false },` |
|      - | 8178 | `	};` |
|      - | 8179 | `	static const PH7_NativeConstDef aConstConst[] = {` |
|      - | 8180 | `		{ "IS_PUBLIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,  0, 0.0 },` |
|      - | 8181 | `		{ "IS_PROTECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,  0, 0.0 },` |
|      - | 8182 | `		{ "IS_PRIVATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,  0, 0.0 },` |
|      - | 8183 | `		{ "IS_FINAL",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|      - | 8184 | `	};` |
|      - | 8185 | `	static const PH7_NativeMethodDef aConstMethod[] = {` |
|      - | 8186 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - | 8187 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|      - | 8188 | `		  vm_builtin_ReflectionClassConstant_construct },` |
|      - | 8189 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string",` |
|      - | 8190 | `		  vm_builtin_ReflectionClassConstant_toString },` |
|      - | 8191 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionProperty_getName },` |
|      - | 8192 | `		{ "getValue",    PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_ReflectionClassConstant_getValue },` |
|      - | 8193 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClassConstant_isPublic },` |
|      - | 8194 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClassConstant_isPrivate },` |
|      - | 8195 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool",` |
|      - | 8196 | `		  vm_builtin_ReflectionClassConstant_isProtected },` |
|      - | 8197 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClassConstant_isFinal },` |
|      - | 8198 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int",` |
|      - | 8199 | `		  vm_builtin_ReflectionClassConstant_getModifiers },` |
|      - | 8200 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|      - | 8201 | `		  vm_builtin_ReflectionClassConstant_getDeclaringClass },` |
|      - | 8202 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false",` |
|      - | 8203 | `		  vm_builtin_ReflectionClassConstant_getDocComment },` |
|      - | 8204 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - | 8205 | `		  vm_builtin_ReflectionClassConstant_getAttributes },` |
|      - | 8206 | `		{ "isEnumCase",  PH7_MOD_PUBLIC, "", "bool",` |
|      - | 8207 | `		  vm_builtin_ReflectionClassConstant_isEnumCase },` |
|      - | 8208 | `		{ "isDeprecated", PH7_MOD_PUBLIC, "", "bool",` |
|      - | 8209 | `		  vm_builtin_ReflectionClassConstant_isDeprecated },` |
|      - | 8210 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClassConstant_hasType },` |
|      - | 8211 | `		{ "getType",     PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|      - | 8212 | `		  vm_builtin_ReflectionClassConstant_getType },` |
|      - | 8213 | `	};` |
|      - | 8214 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 8215 | `		{ "ReflectionProperty", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 8216 | `		  aPropMethod, SX_ARRAYSIZE(aPropMethod),` |
|      - | 8217 | `		  aPropConst, SX_ARRAYSIZE(aPropConst),` |
|      - | 8218 | `		  aPropProp, SX_ARRAYSIZE(aPropProp), 0, 0, 0 },` |
|      - | 8219 | `		{ "ReflectionClassConstant", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 8220 | `		  aConstMethod, SX_ARRAYSIZE(aConstMethod),` |
|      - | 8221 | `		  aConstConst, SX_ARRAYSIZE(aConstConst),` |
|      - | 8222 | `		  aMemberProp, SX_ARRAYSIZE(aMemberProp), 0, 0, 0 },` |
|      - | 8223 | `	};` |
|   5745 | 8224 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 | 8225 | `}` |
|      - | 8226 | `/*` |
|      - | 8227 | ` * ---------------------------------------------------------------------------` |
|      - | 8228 | ` * The ReflectionEnum family — chunk 6, and the end of __phl_rcinfo.` |
|      - | 8229 | ` *` |
|      - | 8230 | `` * Three classes that are very nearly their parents: `ReflectionEnum` IS a`` |
|      - | 8231 | `` * `ReflectionClass` that refuses a non-enum, and `ReflectionEnumUnitCase` IS a`` |
|      - | 8232 | `` * `ReflectionClassConstant` that refuses a constant which is not a case. What`` |
|      - | 8233 | ` * is their own is the CASE list, which the engine already holds in declaration` |
|      - | 8234 | `` * order on `ph7_class::aEnumCases` — the chunk read a marshalled copy of it out`` |
|      - | 8235 | `` * of `__phl_rcinfo`, the descriptor builder that dies with this conversion.`` |
|      - | 8236 | ` * ---------------------------------------------------------------------------` |
|      - | 8237 | ` */` |
|      - | 8238 |  |
|      - | 8239 | `/* The enum case named zName, or NULL. A case is a class CONSTANT, so the match` |
|      - | 8240 | `` * is case-SENSITIVE: php's hasCase('hearts') is false for `case Hearts`. */`` |
|     28 | 8241 | `static ph7_class_attr * ReflectEnumCase(ph7_class *pClass, const char *zName, int nName)` |
|      1 | 8242 | `{` |
|     29 | 8243 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      - | 8244 | `	sxu32 n;` |
|     29 | 8245 | `	if( nName < 1 ){` |
|    ! 0 | 8246 | `		return 0;` |
|      - | 8247 | `	}` |
|     67 | 8248 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     52 | 8249 | `		if( (int)SyStringLength(&apCase[n]->sName) == nName` |
|     38 | 8250 | `		 && SyMemcmp(SyStringData(&apCase[n]->sName), zName, (sxu32)nName) == 0 ){` |
|     15 | 8251 | `			return apCase[n];` |
|      - | 8252 | `		}` |
|     20 | 8253 | `	}` |
|     15 | 8254 | `	return 0;` |
|     15 | 8255 | `}` |
|      - | 8256 | `/*` |
|      - | 8257 | ` * One case reflector for an already-validated case. php answers a BackedCase` |
|      - | 8258 | ` * for a backed enum and a UnitCase for a pure one, and both carry the same two` |
|      - | 8259 | ` * slots ReflectionClassConstant does — an enum cannot extend anything, so the` |
|      - | 8260 | ` * declaring class is always the enum itself.` |
|      - | 8261 | ` */` |
|     46 | 8262 | `static ph7_class_instance * ReflectEnumCaseNew(ph7_context *pCtx, ph7_class *pEnum,` |
|      - | 8263 | `	const SyString *pCase)` |
|      1 | 8264 | `{` |
|     47 | 8265 | `	ph7_vm *pVm = pCtx->pVm;` |
|     70 | 8266 | `	const char *zClass = pEnum->nEnumBacking` |
|     23 | 8267 | `		? "ReflectionEnumBackedCase" : "ReflectionEnumUnitCase";` |
|     47 | 8268 | `	ph7_class *pRC = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|     47 | 8269 | `	ph7_class_instance *pObj = pRC ? PH7_NewClassInstance(pVm, pRC) : 0;` |
|     47 | 8270 | `	if( pObj == 0 ){` |
|    ! 0 | 8271 | `		return 0;` |
|      - | 8272 | `	}` |
|     70 | 8273 | `	PH7_NativeSetAttrStr(pVm, pObj, "class", SyStringData(&pEnum->sName),` |
|     46 | 8274 | `		(int)SyStringLength(&pEnum->sName));` |
|     47 | 8275 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(pCase), (int)SyStringLength(pCase));` |
|     47 | 8276 | `	return pObj;` |
|     24 | 8277 | `}` |
|      - | 8278 | `/* ReflectionEnum::__construct(object\|string $objectOrClass) */` |
|     30 | 8279 | `static int vm_builtin_ReflectionEnum_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8280 | `{` |
|      - | 8281 | `	ph7_class *pClass;` |
|     31 | 8282 | `	sxi32 rc = vm_builtin_ReflectionClass_construct(pCtx, nArg, apArg);` |
|     31 | 8283 | `	if( rc != PH7_OK ){` |
|      5 | 8284 | `		return rc; /* "Class %s does not exist", already php's */` |
|      - | 8285 | `	}` |
|     27 | 8286 | `	pClass = ReflectClassOf(pCtx);` |
|     27 | 8287 | `	if( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     10 | 8288 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 | 8289 | `			"Class \"%z\" is not an enum", &pClass->sName);` |
|      - | 8290 | `	}` |
|     21 | 8291 | `	return PH7_OK;` |
|     16 | 8292 | `}` |
|     12 | 8293 | `static int vm_builtin_ReflectionEnum_hasCase(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8294 | `{` |
|     13 | 8295 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 8296 | `	const char *zName;` |
|      - | 8297 | `	int nName;` |
|     13 | 8298 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 8299 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 8300 | `		return PH7_OK;` |
|      - | 8301 | `	}` |
|     13 | 8302 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     13 | 8303 | `	ph7_result_bool(pCtx, ReflectEnumCase(pClass, zName, nName) != 0);` |
|     13 | 8304 | `	return PH7_OK;` |
|      7 | 8305 | `}` |
|      - | 8306 | `/*` |
|      - | 8307 | ` * getCase(): php tells the two failures apart — a name that is a constant but` |
|      - | 8308 | ` * not a case is "X::K is not a case", one that is neither is "Case X::K does` |
|      - | 8309 | ` * not exist". The chunk answered the second for both.` |
|      - | 8310 | ` */` |
|     16 | 8311 | `static int vm_builtin_ReflectionEnum_getCase(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8312 | `{` |
|     17 | 8313 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 8314 | `	ph7_class_attr *pCase;` |
|      - | 8315 | `	const char *zName;` |
|      - | 8316 | `	int nName;` |
|     17 | 8317 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 8318 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8319 | `		return PH7_OK;` |
|      - | 8320 | `	}` |
|     17 | 8321 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     17 | 8322 | `	pCase = ReflectEnumCase(pClass, zName, nName);` |
|     17 | 8323 | `	if( pCase == 0 ){` |
|      7 | 8324 | `		if( nName > 0 && SyHashGet(&pClass->hConst, (const void *)zName, (sxu32)nName) ){` |
|      4 | 8325 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 8326 | `				"%z::%.*s is not a case", &pClass->sName, nName, zName);` |
|      - | 8327 | `		}` |
|      7 | 8328 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 | 8329 | `			"Case %z::%.*s does not exist", &pClass->sName, nName, zName);` |
|      - | 8330 | `	}` |
|     11 | 8331 | `	return ReflectResultObject(pCtx, ReflectEnumCaseNew(pCtx, pClass, &pCase->sName));` |
|      9 | 8332 | `}` |
|     14 | 8333 | `static int vm_builtin_ReflectionEnum_getCases(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8334 | `{` |
|     15 | 8335 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     15 | 8336 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      7 | 8337 | `	SXUNUSED(nArg);` |
|      7 | 8338 | `	SXUNUSED(apArg);` |
|     15 | 8339 | `	if( pOut == 0 ){` |
|    ! 0 | 8340 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8341 | `	}` |
|     15 | 8342 | `	if( pClass ){` |
|     15 | 8343 | `		ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      - | 8344 | `		sxu32 n;` |
|     51 | 8345 | `		for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     37 | 8346 | `			ReflectTypeListAdd(pCtx, pOut, ReflectEnumCaseNew(pCtx, pClass, &apCase[n]->sName));` |
|     19 | 8347 | `		}` |
|      7 | 8348 | `	}` |
|     15 | 8349 | `	ph7_result_value(pCtx, pOut);` |
|     15 | 8350 | `	return PH7_OK;` |
|      8 | 8351 | `}` |
|     12 | 8352 | `static int vm_builtin_ReflectionEnum_isBacked(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8353 | `{` |
|     13 | 8354 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      6 | 8355 | `	SXUNUSED(nArg);` |
|      6 | 8356 | `	SXUNUSED(apArg);` |
|     13 | 8357 | `	ph7_result_bool(pCtx, pClass != 0 && pClass->nEnumBacking != 0);` |
|     13 | 8358 | `	return PH7_OK;` |
|      1 | 8359 | `}` |
|     16 | 8360 | `static int vm_builtin_ReflectionEnum_getBackingType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8361 | `{` |
|     17 | 8362 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 8363 | `	const char *zText;` |
|      - | 8364 | `	ph7_class_instance *pType;` |
|      8 | 8365 | `	SXUNUSED(nArg);` |
|      8 | 8366 | `	SXUNUSED(apArg);` |
|     17 | 8367 | `	if( pClass == 0 \|\| pClass->nEnumBacking == 0 ){` |
|      5 | 8368 | `		ph7_result_null(pCtx); /* a PURE enum has no backing type */` |
|      5 | 8369 | `		return PH7_OK;` |
|      - | 8370 | `	}` |
|     13 | 8371 | `	zText = (pClass->nEnumBacking & MEMOBJ_INT) ? "int" : "string";` |
|     13 | 8372 | `	pType = ReflectMakeType(pCtx, zText, (int)SyStrlen(zText));` |
|     13 | 8373 | `	if( pType == 0 ){` |
|    ! 0 | 8374 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8375 | `		return PH7_OK;` |
|      - | 8376 | `	}` |
|     13 | 8377 | `	PH7_NativeResultObject(pCtx, pType);` |
|     13 | 8378 | `	return PH7_OK;` |
|      9 | 8379 | `}` |
|      - | 8380 | `/*` |
|      - | 8381 | ` * ReflectionEnumUnitCase::__construct(object\|string $class, string $constant).` |
|      - | 8382 | ` *` |
|      - | 8383 | ` * The parent raises for a constant that does not exist; what is left is "not a` |
|      - | 8384 | ` * case", and php words that one WITHOUT asking whether the class is an enum at` |
|      - | 8385 | `` * all — `new ReflectionEnumUnitCase('Plain', 'K')` on an ordinary class says`` |
|      - | 8386 | `` * `Constant Plain::K is not a case`, not "is not an enum" as the chunk did.`` |
|      - | 8387 | ` * A non-enum's constant can never carry the ENUMCASE bit, so the one screen` |
|      - | 8388 | ` * answers both shapes.` |
|      - | 8389 | ` */` |
|     18 | 8390 | `static int vm_builtin_ReflectionEnumCase_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8391 | `{` |
|      - | 8392 | `	ReflectMemberRef sRef;` |
|     19 | 8393 | `	sxi32 rc = vm_builtin_ReflectionClassConstant_construct(pCtx, nArg, apArg);` |
|     19 | 8394 | `	if( rc != PH7_OK ){` |
|      3 | 8395 | `		return rc;` |
|      - | 8396 | `	}` |
|     17 | 8397 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 8398 | `		return PH7_OK;` |
|      - | 8399 | `	}` |
|     17 | 8400 | `	if( (sRef.pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){` |
|     10 | 8401 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      6 | 8402 | `			"Constant %z::%.*s is not a case", &sRef.pClass->sName, sRef.nName, sRef.zName);` |
|      - | 8403 | `	}` |
|     11 | 8404 | `	return PH7_OK;` |
|     10 | 8405 | `}` |
|      - | 8406 | `/* ReflectionEnumBackedCase::__construct() — the same, plus php's screen for a` |
|      - | 8407 | ` * PURE enum's case, which has no backing value to answer with. */` |
|      6 | 8408 | `static int vm_builtin_ReflectionEnumBackedCase_construct(ph7_context *pCtx, int nArg,` |
|      - | 8409 | `	ph7_value **apArg)` |
|      1 | 8410 | `{` |
|      - | 8411 | `	ReflectMemberRef sRef;` |
|      7 | 8412 | `	sxi32 rc = vm_builtin_ReflectionEnumCase_construct(pCtx, nArg, apArg);` |
|      7 | 8413 | `	if( rc != PH7_OK ){` |
|    ! 0 | 8414 | `		return rc;` |
|      - | 8415 | `	}` |
|      7 | 8416 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 8417 | `		return PH7_OK;` |
|      - | 8418 | `	}` |
|      7 | 8419 | `	if( sRef.pClass->nEnumBacking == 0 ){` |
|      4 | 8420 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 | 8421 | `			"Enum case %z::%.*s is not a backed case", &sRef.pClass->sName,` |
|      1 | 8422 | `			sRef.nName, sRef.zName);` |
|      - | 8423 | `	}` |
|      5 | 8424 | `	return PH7_OK;` |
|      4 | 8425 | `}` |
|      6 | 8426 | `static int vm_builtin_ReflectionEnumCase_getEnum(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8427 | `{` |
|      7 | 8428 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 8429 | `	ReflectMemberRef sRef;` |
|      - | 8430 | `	ph7_class *pRC;` |
|      - | 8431 | `	ph7_class_instance *pObj;` |
|      3 | 8432 | `	SXUNUSED(nArg);` |
|      3 | 8433 | `	SXUNUSED(apArg);` |
|      7 | 8434 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) ){` |
|    ! 0 | 8435 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8436 | `		return PH7_OK;` |
|      - | 8437 | `	}` |
|      7 | 8438 | `	pRC = PH7_VmExtractClass(pVm, "ReflectionEnum", sizeof("ReflectionEnum")-1, FALSE, 0);` |
|      7 | 8439 | `	pObj = pRC ? PH7_NewClassInstance(pVm, pRC) : 0;` |
|      7 | 8440 | `	if( pObj == 0 ){` |
|    ! 0 | 8441 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8442 | `		return PH7_OK;` |
|      - | 8443 | `	}` |
|     10 | 8444 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(&sRef.pClass->sName),` |
|      6 | 8445 | `		(int)SyStringLength(&sRef.pClass->sName));` |
|      7 | 8446 | `	return ReflectResultObject(pCtx, pObj);` |
|      4 | 8447 | `}` |
|      - | 8448 | ``/* getBackingValue(): the `value` the case singleton carries. The singleton is`` |
|      - | 8449 | ` * the constant's own value, so the parent's accessor materializes it. */` |
|     16 | 8450 | `static int vm_builtin_ReflectionEnumCase_getBackingValue(ph7_context *pCtx, int nArg,` |
|      - | 8451 | `	ph7_value **apArg)` |
|      1 | 8452 | `{` |
|      - | 8453 | `	ReflectMemberRef sRef;` |
|      - | 8454 | `	ph7_value *pVal, *pBacking;` |
|      - | 8455 | `	sxi32 rc;` |
|      8 | 8456 | `	SXUNUSED(nArg);` |
|      8 | 8457 | `	SXUNUSED(apArg);` |
|     17 | 8458 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 8459 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8460 | `		return PH7_OK;` |
|      - | 8461 | `	}` |
|     17 | 8462 | `	rc = ReflectConstSlot(pCtx, sRef.pClass, sRef.pAttr, &pVal);` |
|     17 | 8463 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 8464 | `		return rc;` |
|      - | 8465 | `	}` |
|     17 | 8466 | `	pBacking = (pVal && (pVal->iFlags & MEMOBJ_OBJ))` |
|     24 | 8467 | `		? PH7_NativeAttr((ph7_class_instance *)pVal->x.pOther, "value") : 0;` |
|     17 | 8468 | `	if( pBacking ){` |
|     17 | 8469 | `		ph7_result_value(pCtx, pBacking);` |
|      9 | 8470 | `	}else{` |
|    ! 0 | 8471 | `		ph7_result_null(pCtx);` |
|      - | 8472 | `	}` |
|     17 | 8473 | `	return PH7_OK;` |
|      9 | 8474 | `}` |
|      - | 8475 | `/*` |
|      - | 8476 | ` * Declare the three. Called from PH7_VmInstallReflection() where chunk 6 used` |
|      - | 8477 | ` * to be compiled, so both parents are installed (PH7_ClassInherit COPIES a` |
|      - | 8478 | ` * base's methods down — a native subclass needs its parent declared first).` |
|      - | 8479 | ` *` |
|      - | 8480 | ` * The uncloneable/unserializable flags are php's answer for each class and do` |
|      - | 8481 | ``  * NOT come down with the inheritance: without them `clone $enumReflector` `` |
|      - | 8482 | ` * reached the private __clone it inherited and reported that instead.` |
|      - | 8483 | ` */` |
|   5740 | 8484 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionEnum(ph7_vm *pVm)` |
|      5 | 8485 | `{` |
|      - | 8486 | `	static const PH7_NativeMethodDef aEnumMethod[] = {` |
|      - | 8487 | `		{ "__construct",    PH7_MOD_PUBLIC, "object\|string $objectOrClass", "",` |
|      - | 8488 | `		  vm_builtin_ReflectionEnum_construct },` |
|      - | 8489 | `		{ "hasCase",        PH7_MOD_PUBLIC, "string $name", "bool",` |
|      - | 8490 | `		  vm_builtin_ReflectionEnum_hasCase },` |
|      - | 8491 | `		{ "getCase",        PH7_MOD_PUBLIC, "string $name", "ReflectionEnumUnitCase",` |
|      - | 8492 | `		  vm_builtin_ReflectionEnum_getCase },` |
|      - | 8493 | `		{ "getCases",       PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionEnum_getCases },` |
|      - | 8494 | `		{ "isBacked",       PH7_MOD_PUBLIC, "", "bool",  vm_builtin_ReflectionEnum_isBacked },` |
|      - | 8495 | `		{ "getBackingType", PH7_MOD_PUBLIC, "", "?ReflectionNamedType",` |
|      - | 8496 | `		  vm_builtin_ReflectionEnum_getBackingType },` |
|      - | 8497 | `	};` |
|      - | 8498 | `	static const PH7_NativeMethodDef aCaseMethod[] = {` |
|      - | 8499 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|      - | 8500 | `		  vm_builtin_ReflectionEnumCase_construct },` |
|      - | 8501 | `		{ "getEnum",     PH7_MOD_PUBLIC, "", "ReflectionEnum",` |
|      - | 8502 | `		  vm_builtin_ReflectionEnumCase_getEnum },` |
|      - | 8503 | `		/* php redeclares getValue() on the case reflector for its narrower` |
|      - | 8504 | `		 * return type; the body is the parent's. */` |
|      - | 8505 | `		{ "getValue",    PH7_MOD_PUBLIC, "", "UnitEnum",` |
|      - | 8506 | `		  vm_builtin_ReflectionClassConstant_getValue },` |
|      - | 8507 | `	};` |
|      - | 8508 | `	static const PH7_NativeMethodDef aBackedMethod[] = {` |
|      - | 8509 | `		{ "__construct",     PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|      - | 8510 | `		  vm_builtin_ReflectionEnumBackedCase_construct },` |
|      - | 8511 | `		{ "getBackingValue", PH7_MOD_PUBLIC, "", "string\|int",` |
|      - | 8512 | `		  vm_builtin_ReflectionEnumCase_getBackingValue },` |
|      - | 8513 | `	};` |
|      - | 8514 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 8515 | `		{ "ReflectionEnum", "ReflectionClass", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 8516 | `		  aEnumMethod, SX_ARRAYSIZE(aEnumMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 8517 | `		{ "ReflectionEnumUnitCase", "ReflectionClassConstant", 0,` |
|      - | 8518 | `		  PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 8519 | `		  aCaseMethod, SX_ARRAYSIZE(aCaseMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 8520 | `		{ "ReflectionEnumBackedCase", "ReflectionEnumUnitCase", 0,` |
|      - | 8521 | `		  PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 8522 | `		  aBackedMethod, SX_ARRAYSIZE(aBackedMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 8523 | `	};` |
|   5745 | 8524 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 | 8525 | `}` |
|      - | 8526 | `/*` |
|      - | 8527 | ` * ---------------------------------------------------------------------------` |
|      - | 8528 | ` * php's Reflection EXPORT format — chunk 9, the last of the library's PHP.` |
|      - | 8529 | ` *` |
|      - | 8530 | ` * Every Reflector's __toString(). It is a byte-exact format with no API of its` |
|      - | 8531 | ` * own, so the chunk was written against the PUBLIC reflection API of whatever` |
|      - | 8532 | ` * it was printing — the only thing PHP could reach. All of those targets are C` |
|      - | 8533 | ` * now, so this reads the engine directly: the member walk for order and` |
|      - | 8534 | ` * visibility, ReflectParamAt for parameters, ReflectExportValue (built for the` |
|      - | 8535 | ` * attribute-argument block) for every value.` |
|      - | 8536 | ` *` |
|      - | 8537 | ` * Defined at the END of the file because it needs all of that; the five entry` |
|      - | 8538 | ` * points are forward-declared above their callers.` |
|      - | 8539 | ` * ---------------------------------------------------------------------------` |
|      - | 8540 | ` */` |
|      - | 8541 |  |
|      - | 8542 | `/* "internal:Core" / "user" — the first tag of every function and class head. */` |
|    156 | 8543 | `static void ReflectExportKind(SyBlob *pOut, int bInternal)` |
|      2 | 8544 | `{` |
|    158 | 8545 | `	if( bInternal ){` |
|     88 | 8546 | `		SyBlobAppend(pOut, "internal:Core", sizeof("internal:Core")-1);` |
|     45 | 8547 | `	}else{` |
|     71 | 8548 | `		SyBlobAppend(pOut, "user", sizeof("user")-1);` |
|      - | 8549 | `	}` |
|    158 | 8550 | `}` |
|      - | 8551 | `/* A declared type followed by a space, or nothing at all. */` |
|    174 | 8552 | `static void ReflectExportTypeSp(SyBlob *pOut, const SyString *pType)` |
|      2 | 8553 | `{` |
|    176 | 8554 | `	if( pType && SyStringLength(pType) > 0 ){` |
|    140 | 8555 | `		SyBlobAppend(pOut, SyStringData(pType), SyStringLength(pType));` |
|    140 | 8556 | `		SyBlobAppend(pOut, " ", sizeof(char));` |
|     69 | 8557 | `	}` |
|    176 | 8558 | `}` |
|      - | 8559 | `/* php's visibility word for a member. */` |
|    208 | 8560 | `static const char * ReflectExportVis(sxi32 iProtection)` |
|      1 | 8561 | `{` |
|    209 | 8562 | `	if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     11 | 8563 | `		return "private";` |
|      - | 8564 | `	}` |
|    199 | 8565 | `	return iProtection == PH7_CLASS_PROT_PROTECTED ? "protected" : "public";` |
|    105 | 8566 | `}` |
|      - | 8567 | `/*` |
|      - | 8568 | `` * The text after `= ` in a parameter default.`` |
|      - | 8569 | ` *` |
|      - | 8570 | ` * php prints a constant-reference default as the CONSTANT's name, not its value` |
|      - | 8571 | `` * (`$f = M_PI`) — that argument was never folded, so php still has the source`` |
|      - | 8572 | `` * expression. `<default>` is what the chunk answered when the value could not`` |
|      - | 8573 | `` * be produced at all, which is a declared `= ?` row in aBuiltinSig[].`` |
|      - | 8574 | ` */` |
|     60 | 8575 | `static void ReflectExportDefault(ph7_context *pCtx, SyBlob *pOut, ReflectParamDesc *pDesc)` |
|      1 | 8576 | `{` |
|     61 | 8577 | `	const char *z = 0;` |
|     61 | 8578 | `	int n = 0;` |
|     61 | 8579 | `	if( ReflectParamDefConst(pCtx, pDesc, &z, &n) ){` |
|      7 | 8580 | `		SyBlobAppend(pOut, z, (sxu32)n);` |
|     34 | 8581 | `		return;` |
|      - | 8582 | `	}` |
|     55 | 8583 | `	if( pDesc->pArg ){` |
|      - | 8584 | `		ph7_value sValue;` |
|     39 | 8585 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     39 | 8586 | `		VmLocalExec(pCtx->pVm, &pDesc->pArg->aByteCode, &sValue, FALSE);` |
|     39 | 8587 | `		ReflectExportValue(pCtx, pOut, &sValue, 0);` |
|     39 | 8588 | `		PH7_MemObjRelease(&sValue);` |
|     39 | 8589 | `		return;` |
|      - | 8590 | `	}` |
|      - | 8591 | `	{` |
|      - | 8592 | `		/* A DECLARED default is signature TEXT: reduce it the way` |
|      - | 8593 | `		 * getDefaultValue() does, and fall back to php's own placeholder.` |
|      - | 8594 | `		 * Only an INTERNAL parameter reaches this branch (a compiled one carries` |
|      - | 8595 | `		 * pArg above), which is exactly the case php prints DOUBLE-quoted. */` |
|     17 | 8596 | `		const char *zDef = SyStringData(&pDesc->sDefText);` |
|     17 | 8597 | `		int nDef = (int)SyStringLength(&pDesc->sDefText);` |
|     17 | 8598 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|     17 | 8599 | `		if( ReflectSigHas(zDef, nDef, "::", 2) ){` |
|      - | 8600 | `			/* Rule 28's split, on the declared side: php's stub keeps the SOURCE of a` |
|      - | 8601 | `			 * constant-expression default, so the export prints` |
|      - | 8602 | ``			 * `= SplFileInfo::class` and `= A::K \| A::J` verbatim while`` |
|      - | 8603 | `			 * getDefaultValue() answers what they evaluate to. */` |
|      3 | 8604 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|      3 | 8605 | `			return;` |
|      - | 8606 | `		}` |
|     15 | 8607 | `		if( pVal && ReflectSigGlobalConst(pCtx, zDef, nDef, pVal) ){` |
|      - | 8608 | ``			/* Same split for a GLOBAL constant: `= E_ERROR`, never `= 1`. */`` |
|    ! 0 | 8609 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|    ! 0 | 8610 | `			return;` |
|      - | 8611 | `		}` |
|     15 | 8612 | `		if( pVal && ReflectSigScalar(pCtx, zDef, nDef, pVal) ){` |
|     15 | 8613 | `			if( (pVal->iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING ){` |
|     16 | 8614 | `				ReflectExportStrQ(pOut, (const char *)SyBlobData(&pVal->sBlob),` |
|      5 | 8615 | `					SyBlobLength(&pVal->sBlob), '"');` |
|     11 | 8616 | `				return;` |
|      - | 8617 | `			}` |
|      5 | 8618 | `			if( pVal->iFlags & MEMOBJ_NULL ){` |
|      - | 8619 | `				/* The same internal/user split as the quote character above: php` |
|      - | 8620 | ``				 * spells an INTERNAL parameter's null default `null` and a`` |
|      - | 8621 | ``				 * userland one `NULL` (`= null` for every `?T $x = null` stub row). */`` |
|      5 | 8622 | `				SyBlobAppend(pOut, "null", sizeof("null")-1);` |
|      5 | 8623 | `				return;` |
|      - | 8624 | `			}` |
|    ! 0 | 8625 | `			ReflectExportValue(pCtx, pOut, pVal, 0);` |
|    ! 0 | 8626 | `			return;` |
|      - | 8627 | `		}` |
|    ! 0 | 8628 | `		if( (nDef >= 1 && zDef[0] == '[')` |
|    ! 0 | 8629 | `		 \|\| (nDef >= (int)sizeof("array (")-1` |
|    ! 0 | 8630 | `		  && SyMemcmp(zDef, "array (", sizeof("array (")-1) == 0) ){` |
|    ! 0 | 8631 | `			SyBlobAppend(pOut, "[]", sizeof("[]")-1);` |
|    ! 0 | 8632 | `			return;` |
|      - | 8633 | `		}` |
|      - | 8634 | `	}` |
|    ! 0 | 8635 | `	SyBlobAppend(pOut, "<default>", sizeof("<default>")-1);` |
|     31 | 8636 | `}` |
|      - | 8637 | ``/* `Parameter #0 [ <optional> int &...$name = 5 ]` */`` |
|    126 | 8638 | `static void ReflectExportParamLine(ph7_context *pCtx, SyBlob *pOut, ReflectParamDesc *pDesc)` |
|      2 | 8639 | `{` |
|    191 | 8640 | `	SyBlobFormat(pOut, "Parameter #%d [ <%s> ", pDesc->iPos,` |
|    126 | 8641 | `		pDesc->bOptional ? "optional" : "required");` |
|    128 | 8642 | `	ReflectExportTypeSp(pOut, &pDesc->sType);` |
|    128 | 8643 | `	if( pDesc->bByRef ){` |
|     11 | 8644 | `		SyBlobAppend(pOut, "&", sizeof(char));` |
|      5 | 8645 | `	}` |
|    128 | 8646 | `	if( pDesc->bVariadic ){` |
|     13 | 8647 | `		SyBlobAppend(pOut, "...", sizeof("...")-1);` |
|      6 | 8648 | `	}` |
|    128 | 8649 | `	SyBlobAppend(pOut, "$", sizeof(char));` |
|    128 | 8650 | `	SyBlobAppend(pOut, SyStringData(&pDesc->sName), SyStringLength(&pDesc->sName));` |
|    128 | 8651 | `	if( pDesc->bHasDef ){` |
|     61 | 8652 | `		SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|     61 | 8653 | `		ReflectExportDefault(pCtx, pOut, pDesc);` |
|     30 | 8654 | `	}` |
|    128 | 8655 | `	SyBlobAppend(pOut, " ]", sizeof(" ]")-1);` |
|    128 | 8656 | `}` |
|      - | 8657 | `/*` |
|      - | 8658 | `` * `Property [ public protected(set) readonly int $x = 5 ]` + newline.`` |
|      - | 8659 | ` *` |
|      - | 8660 | ` * The set-visibility is printed only when it DIFFERS from the get-visibility,` |
|      - | 8661 | `` * which is why a `public readonly` property shows php's implied`` |
|      - | 8662 | `` * `protected(set)` and a `protected readonly` one shows nothing.`` |
|      - | 8663 | ` */` |
|     58 | 8664 | `static void ReflectExportPropLine(ph7_context *pCtx, SyBlob *pOut, ph7_class_attr *pAttr,` |
|      - | 8665 | `	const SyString *pKey)` |
|      1 | 8666 | `{` |
|     59 | 8667 | `	sxi32 iSet = pAttr->iProtection;` |
|     59 | 8668 | `	SyBlobAppend(pOut, "Property [ ", sizeof("Property [ ")-1);` |
|      - | 8669 | `	/* php's modifier MASK implies two bits the declaration never wrote:` |
|      - | 8670 | `	 * private(set) implies final, and readonly implies protected(set). */` |
|     59 | 8671 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|      3 | 8672 | `		SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|      1 | 8673 | `	}` |
|     59 | 8674 | `	SyBlobFormat(pOut, "%s ", ReflectExportVis(pAttr->iProtection));` |
|     59 | 8675 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|      3 | 8676 | `		iSet = PH7_CLASS_PROT_PRIVATE;` |
|     58 | 8677 | `	}else if( (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET)` |
|     57 | 8678 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){` |
|     13 | 8679 | `		iSet = PH7_CLASS_PROT_PROTECTED;` |
|      6 | 8680 | `	}` |
|      - | 8681 | `	/* The set-visibility is printed only when it DIFFERS from the get one. */` |
|     59 | 8682 | `	if( iSet != pAttr->iProtection ){` |
|     11 | 8683 | `		SyBlobFormat(pOut, "%s(set) ", ReflectExportVis(iSet));` |
|      5 | 8684 | `	}` |
|     59 | 8685 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|      7 | 8686 | `		SyBlobAppend(pOut, "static ", sizeof("static ")-1);` |
|      3 | 8687 | `	}` |
|     59 | 8688 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|    ! 0 | 8689 | `		SyBlobAppend(pOut, "virtual ", sizeof("virtual ")-1);` |
|    ! 0 | 8690 | `	}` |
|     59 | 8691 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|     13 | 8692 | `		SyBlobAppend(pOut, "readonly ", sizeof("readonly ")-1);` |
|      6 | 8693 | `	}` |
|     59 | 8694 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     49 | 8695 | `		ReflectExportTypeSp(pOut, &pAttr->sTypeName);` |
|     24 | 8696 | `	}` |
|     59 | 8697 | `	SyBlobAppend(pOut, "$", sizeof(char));` |
|     59 | 8698 | `	SyBlobAppend(pOut, SyStringData(pKey), SyStringLength(pKey));` |
|     58 | 8699 | `	if( SySetUsed(&pAttr->aByteCode) > 0 \|\| pAttr->pNativeValue` |
|     31 | 8700 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      - | 8701 | `		ph7_value sValue;` |
|     31 | 8702 | `		SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|     31 | 8703 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     31 | 8704 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     29 | 8705 | `			VmLocalExec(pCtx->pVm, &pAttr->aByteCode, &sValue, FALSE);` |
|     17 | 8706 | `		}else if( pAttr->pNativeValue ){` |
|    ! 0 | 8707 | `			PH7_NativeLiteralValue(pCtx->pVm, pAttr->pNativeValue, &sValue);` |
|    ! 0 | 8708 | `		}` |
|     31 | 8709 | `		ReflectExportValue(pCtx, pOut, &sValue, 0);` |
|     31 | 8710 | `		PH7_MemObjRelease(&sValue);` |
|     15 | 8711 | `	}` |
|      - | 8712 | `	/* A hooked property names its hooks; php lists no method for them. */` |
|     59 | 8713 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET) ){` |
|      3 | 8714 | `		SyBlobAppend(pOut, " {", sizeof(" {")-1);` |
|      3 | 8715 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET ){` |
|      3 | 8716 | `			SyBlobAppend(pOut, " get;", sizeof(" get;")-1);` |
|      1 | 8717 | `		}` |
|      3 | 8718 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET ){` |
|      3 | 8719 | `			SyBlobAppend(pOut, " set;", sizeof(" set;")-1);` |
|      1 | 8720 | `		}` |
|      3 | 8721 | `		SyBlobAppend(pOut, " }", sizeof(" }")-1);` |
|      1 | 8722 | `	}` |
|     59 | 8723 | `	SyBlobAppend(pOut, " ]\n", sizeof(" ]\n")-1);` |
|     59 | 8724 | `}` |
|      - | 8725 | `/*` |
|      - | 8726 | `` * `Constant [ final public int NAME ] { value }` + newline. The TYPE is the`` |
|      - | 8727 | ` * value's, not a declared one, and an object (an enum case) prints as the word` |
|      - | 8728 | ` * Object under its own class name.` |
|      - | 8729 | ` */` |
|     24 | 8730 | `static sxi32 ReflectExportConstLine(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass,` |
|      - | 8731 | `	ph7_class_attr *pAttr, const SyString *pKey)` |
|      1 | 8732 | `{` |
|     25 | 8733 | `	ph7_value *pVal = 0;` |
|     25 | 8734 | `	sxi32 rc = ReflectConstSlot(pCtx, pClass, pAttr, &pVal);` |
|     25 | 8735 | `	const char *zType = "null";` |
|     25 | 8736 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 8737 | `		return rc;` |
|      - | 8738 | `	}` |
|     25 | 8739 | `	SyBlobAppend(pOut, "Constant [ ", sizeof("Constant [ ")-1);` |
|     25 | 8740 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      7 | 8741 | `		SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|      3 | 8742 | `	}` |
|     25 | 8743 | `	SyBlobFormat(pOut, "%s ", ReflectExportVis(pAttr->iProtection));` |
|     25 | 8744 | `	if( pVal ){` |
|     25 | 8745 | `		if( (pVal->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      3 | 8746 | `			zType = 0; /* the object's own class */` |
|     24 | 8747 | `		}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|    ! 0 | 8748 | `			zType = "null";` |
|     23 | 8749 | `		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      3 | 8750 | `			zType = "array";` |
|     22 | 8751 | `		}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|    ! 0 | 8752 | `			zType = "bool";` |
|     21 | 8753 | `		}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|    ! 0 | 8754 | `			zType = "float";` |
|     21 | 8755 | `		}else if( pVal->iFlags & MEMOBJ_INT ){` |
|     13 | 8756 | `			zType = "int";` |
|     15 | 8757 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      9 | 8758 | `			zType = "string";` |
|      4 | 8759 | `		}` |
|     12 | 8760 | `	}` |
|     25 | 8761 | `	if( zType ){` |
|     23 | 8762 | `		SyBlobFormat(pOut, "%s ", zType);` |
|     12 | 8763 | `	}else{` |
|      3 | 8764 | `		SyBlobFormat(pOut, "%z ", &((ph7_class_instance *)pVal->x.pOther)->pClass->sName);` |
|      - | 8765 | `	}` |
|     25 | 8766 | `	SyBlobAppend(pOut, SyStringData(pKey), SyStringLength(pKey));` |
|     25 | 8767 | `	SyBlobAppend(pOut, " ] { ", sizeof(" ] { ")-1);` |
|     25 | 8768 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|      - | 8769 | `		/* php prints nothing at all for null */` |
|     24 | 8770 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      3 | 8771 | `		SyBlobAppend(pOut, "Array", sizeof("Array")-1);` |
|     24 | 8772 | `	}else if( (pVal->iFlags & MEMOBJ_OBJ) ){` |
|      3 | 8773 | `		SyBlobAppend(pOut, "Object", sizeof("Object")-1);` |
|     22 | 8774 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|    ! 0 | 8775 | `		if( pVal->x.iVal ){` |
|    ! 0 | 8776 | `			SyBlobAppend(pOut, "1", sizeof(char));` |
|    ! 0 | 8777 | `		}` |
|    ! 0 | 8778 | `	}else{` |
|      - | 8779 | `		ph7_value sTmp;` |
|      - | 8780 | `		const char *zText;` |
|      - | 8781 | `		int nText;` |
|     21 | 8782 | `		PH7_MemObjInit(pCtx->pVm, &sTmp);` |
|     21 | 8783 | `		PH7_MemObjStore(pVal, &sTmp);` |
|     21 | 8784 | `		zText = ph7_value_to_string(&sTmp, &nText);` |
|     21 | 8785 | `		if( nText > 0 ){` |
|     21 | 8786 | `			SyBlobAppend(pOut, zText, (sxu32)nText);` |
|     10 | 8787 | `		}` |
|     21 | 8788 | `		PH7_MemObjRelease(&sTmp);` |
|      - | 8789 | `	}` |
|     25 | 8790 | `	SyBlobAppend(pOut, " }\n", sizeof(" }\n")-1);` |
|     25 | 8791 | `	return SXRET_OK;` |
|     13 | 8792 | `}` |
|      - | 8793 | `/* A native class METHOD as a function reference (ReflectFuncFill's tail, for a` |
|      - | 8794 | ` * method the member walk handed over rather than one a receiver names). */` |
|     74 | 8795 | `static void ReflectFuncFromMethod(ph7_class *pClass, ph7_class_method *pMeth,` |
|      - | 8796 | `	ReflectFuncRef *pOut)` |
|      1 | 8797 | `{` |
|     75 | 8798 | `	SyZero(pOut, sizeof(*pOut));` |
|     75 | 8799 | `	pOut->pClass = pClass;` |
|     75 | 8800 | `	pOut->pMeth = pMeth;` |
|     75 | 8801 | `	pOut->pFunc = &pMeth->sFunc;` |
|     75 | 8802 | `	if( (pOut->pFunc->iFlags & VM_FUNC_NATIVE) && pOut->pFunc->pNative ){` |
|     37 | 8803 | `		pOut->zSig = pOut->pFunc->pNative->zSig;` |
|     37 | 8804 | `		if( SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|     37 | 8805 | `			pOut->zRet = pOut->pFunc->pNative->zRet;` |
|     18 | 8806 | `		}` |
|     18 | 8807 | `	}` |
|     75 | 8808 | `}` |
|      - | 8809 | `/*` |
|      - | 8810 | ` * The Method / Function / Closure block.` |
|      - | 8811 | ` *` |
|      - | 8812 | ` * pOwner is the class being EXPORTED when there is one: php's tags are` |
|      - | 8813 | ` * relative to it — a method it did not declare "inherits" from its declaring` |
|      - | 8814 | ` * class — and the reflector alone cannot say that, because $class is the` |
|      - | 8815 | `` * DECLARING class. `overwrites` is the other direction and needs no owner: the`` |
|      - | 8816 | ` * declaring class's own parent declares the same method.` |
|      - | 8817 | ` */` |
|    128 | 8818 | `static sxi32 ReflectExportFuncBlock(ph7_context *pCtx, SyBlob *pOut, ReflectFuncRef *pRef,` |
|      - | 8819 | `	const char *zIndent, ph7_class *pOwner)` |
|      2 | 8820 | `{` |
|      - | 8821 | `	SyBlob sBody;` |
|    130 | 8822 | `	int bInternal = ReflectFuncIsInternal(pRef);` |
|    130 | 8823 | `	int nParam = ReflectParamCount(pRef);` |
|    130 | 8824 | `	const char *zRet = 0;` |
|    130 | 8825 | `	int nRet = 0, bHasRet, bTentRet = 0;` |
|    130 | 8826 | `	sxi32 rc = SXRET_OK;` |
|    130 | 8827 | `	SyBlobInit(&sBody, &pCtx->pVm->sAllocator);` |
|    130 | 8828 | `	bHasRet = ReflectFuncRetTextEx(pRef, &zRet, &nRet, &bTentRet);` |
|    130 | 8829 | `	if( pRef->pMeth ){` |
|    117 | 8830 | `		ph7_class *pDecl = ReflectFuncDeclClass(pRef);` |
|      - | 8831 | `		ph7_class *pProto;` |
|    117 | 8832 | `		SyString *pName = &pRef->pMeth->sFunc.sName;` |
|      - | 8833 | `		int bInherits, bCtor;` |
|    117 | 8834 | `		SyBlobAppend(&sBody, "Method [ <", sizeof("Method [ <")-1);` |
|    117 | 8835 | `		ReflectExportKind(&sBody, bInternal);` |
|    117 | 8836 | `		bInherits = (pOwner != 0 && pDecl != 0 && pDecl != pOwner);` |
|    117 | 8837 | `		if( bInherits ){` |
|     19 | 8838 | `			SyBlobFormat(&sBody, ", inherits %z", &pDecl->sName);` |
|    108 | 8839 | `		}else if( pDecl ){` |
|    148 | 8840 | `			ph7_class *pOver = ReflectPrototypeIn(pCtx, pDecl,` |
|     98 | 8841 | `				SyStringData(pName), (int)SyStringLength(pName), 0);` |
|     99 | 8842 | `			if( pOver ){` |
|      9 | 8843 | `				SyBlobFormat(&sBody, ", overwrites %z", &pOver->sName);` |
|      4 | 8844 | `			}` |
|     49 | 8845 | `		}` |
|    182 | 8846 | `		bCtor = SyStringLength(pName) == sizeof("__construct")-1` |
|    116 | 8847 | `			&& SyStrnicmp(SyStringData(pName), "__construct", sizeof("__construct")-1) == 0;` |
|    117 | 8848 | `		if( bCtor ){` |
|      9 | 8849 | `			SyBlobAppend(&sBody, ", ctor", sizeof(", ctor")-1);` |
|    113 | 8850 | `		}else if( SyStringLength(pName) == sizeof("__destruct")-1` |
|     55 | 8851 | `		 && SyStrnicmp(SyStringData(pName), "__destruct", sizeof("__destruct")-1) == 0 ){` |
|    ! 0 | 8852 | `			SyBlobAppend(&sBody, ", dtor", sizeof(", dtor")-1);` |
|    ! 0 | 8853 | `		}` |
|      - | 8854 | `		/* The prototype belongs to the DECLARING class's own method — the` |
|      - | 8855 | `		 * abstract or interface declaration IT satisfies. Asked from the` |
|      - | 8856 | `		 * exported class instead, a plainly INHERITED method would claim its` |
|      - | 8857 | `` 		 * parent as a prototype, which php does not (it prints `inherits` `` |
|      - | 8858 | `		 * alone there, and both tags when the declarer really has one). */` |
|      - | 8859 | `		/* A CONSTRUCTOR never has one: zend excludes it from prototype` |
|      - | 8860 | `		 * inheritance (there is nothing to satisfy — a parent's ctor is not a` |
|      - | 8861 | ``		 * contract), so php prints `overwrites A, ctor` and stops. */`` |
|    171 | 8862 | `		pProto = bCtor ? 0 : ReflectPrototypeIn(pCtx, pDecl ? pDecl : pRef->pClass,` |
|    108 | 8863 | `			SyStringData(pName), (int)SyStringLength(pName), 1);` |
|    117 | 8864 | `		if( pProto ){` |
|     35 | 8865 | `			SyBlobFormat(&sBody, ", prototype %z", &pProto->sName);` |
|     17 | 8866 | `		}` |
|    117 | 8867 | `		SyBlobAppend(&sBody, "> ", sizeof("> ")-1);` |
|    117 | 8868 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|     31 | 8869 | `			SyBlobAppend(&sBody, "abstract ", sizeof("abstract ")-1);` |
|     15 | 8870 | `		}` |
|    117 | 8871 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      5 | 8872 | `			SyBlobAppend(&sBody, "final ", sizeof("final ")-1);` |
|      2 | 8873 | `		}` |
|    117 | 8874 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|     21 | 8875 | `			SyBlobAppend(&sBody, "static ", sizeof("static ")-1);` |
|     10 | 8876 | `		}` |
|    117 | 8877 | `		SyBlobFormat(&sBody, "%s method %z ]", ReflectExportVis(pRef->pMeth->iProtection), pName);` |
|     59 | 8878 | `	}else{` |
|     20 | 8879 | `		SyBlobAppend(&sBody, pRef->pClosure ? "Closure [ <" : "Function [ <",` |
|     12 | 8880 | `			pRef->pClosure ? sizeof("Closure [ <")-1 : sizeof("Function [ <")-1);` |
|     14 | 8881 | `		ReflectExportKind(&sBody, bInternal);` |
|     14 | 8882 | `		SyBlobAppend(&sBody, "> function ", sizeof("> function ")-1);` |
|     14 | 8883 | `		if( pRef->pFunc ){` |
|      7 | 8884 | `			SyBlobFormat(&sBody, "%z", &pRef->pFunc->sName);` |
|     11 | 8885 | `		}else if( pRef->pHost ){` |
|      8 | 8886 | `			SyBlobFormat(&sBody, "%z", &pRef->pHost->sName);` |
|      3 | 8887 | `		}` |
|     14 | 8888 | `		SyBlobAppend(&sBody, " ]", sizeof(" ]")-1);` |
|      - | 8889 | `	}` |
|    130 | 8890 | `	SyBlobAppend(&sBody, " {\n", sizeof(" {\n")-1);` |
|    130 | 8891 | `	if( !bInternal && pRef->pFunc ){` |
|     51 | 8892 | `		SyBlobAppend(&sBody, "  @@ ", sizeof("  @@ ")-1);` |
|     51 | 8893 | `		if( SyStringLength(&pRef->pFunc->sFile) > 0 ){` |
|     76 | 8894 | `			SyBlobAppend(&sBody, SyStringData(&pRef->pFunc->sFile),` |
|     50 | 8895 | `				SyStringLength(&pRef->pFunc->sFile));` |
|     25 | 8896 | `		}` |
|     51 | 8897 | `		SyBlobFormat(&sBody, " %u - %u\n", pRef->pFunc->nLine, pRef->pFunc->nEndLine);` |
|     25 | 8898 | `	}` |
|      - | 8899 | `	/* php prints the parameter block for every INTERNAL function, and for a` |
|      - | 8900 | `	 * user one only when there is something to say. */` |
|    130 | 8901 | `	if( nParam > 0 \|\| bHasRet \|\| bInternal ){` |
|      - | 8902 | `		int n;` |
|    124 | 8903 | `		SyBlobFormat(&sBody, "\n  - Parameters [%d] {\n", nParam);` |
|    240 | 8904 | `		for( n = 0 ; n < nParam ; n++ ){` |
|      - | 8905 | `			ReflectParamDesc sDesc;` |
|    118 | 8906 | `			if( !ReflectParamAt(pRef, n, &sDesc) ){` |
|    ! 0 | 8907 | `				continue;` |
|      - | 8908 | `			}` |
|    118 | 8909 | `			SyBlobAppend(&sBody, "    ", sizeof("    ")-1);` |
|    118 | 8910 | `			ReflectExportParamLine(pCtx, &sBody, &sDesc);` |
|    118 | 8911 | `			SyBlobAppend(&sBody, "\n", sizeof(char));` |
|     60 | 8912 | `		}` |
|    124 | 8913 | `		SyBlobAppend(&sBody, "  }\n", sizeof("  }\n")-1);` |
|     61 | 8914 | `	}` |
|    130 | 8915 | `	if( bHasRet ){` |
|      - | 8916 | ``		/* php's two spellings: `Return [ T ]` for a declared type, `Tentative return`` |
|      - | 8917 | ``		 * [ T ]` for a stub's @tentative-return-type. */`` |
|    106 | 8918 | `		if( bTentRet ){` |
|     49 | 8919 | `			SyBlobAppend(&sBody, "  - Tentative return [ ", sizeof("  - Tentative return [ ")-1);` |
|     25 | 8920 | `		}else{` |
|     58 | 8921 | `			SyBlobAppend(&sBody, "  - Return [ ", sizeof("  - Return [ ")-1);` |
|      - | 8922 | `		}` |
|    106 | 8923 | `		SyBlobAppend(&sBody, zRet, (sxu32)nRet);` |
|    106 | 8924 | `		SyBlobAppend(&sBody, " ]\n", sizeof(" ]\n")-1);` |
|     52 | 8925 | `	}` |
|    130 | 8926 | `	SyBlobAppend(&sBody, "}\n", sizeof("}\n")-1);` |
|      - | 8927 | `	/* Indent every non-empty line, the way the chunk's explode/implode did. */` |
|    130 | 8928 | `	if( zIndent == 0 \|\| zIndent[0] == '\0' ){` |
|     56 | 8929 | `		SyBlobAppend(pOut, SyBlobData(&sBody), SyBlobLength(&sBody));` |
|     29 | 8930 | `	}else{` |
|     75 | 8931 | `		const char *z = (const char *)SyBlobData(&sBody);` |
|     75 | 8932 | `		sxu32 n = SyBlobLength(&sBody), i = 0, iStart = 0;` |
|     75 | 8933 | `		sxu32 nIndent = (sxu32)SyStrlen(zIndent);` |
|  14120 | 8934 | `		for( i = 0 ; i < n ; i++ ){` |
|  14046 | 8935 | `			if( z[i] != '\n' ){` |
|  13552 | 8936 | `				continue;` |
|      - | 8937 | `			}` |
|    495 | 8938 | `			if( i > iStart ){` |
|    427 | 8939 | `				SyBlobAppend(pOut, zIndent, nIndent);` |
|    427 | 8940 | `				SyBlobAppend(pOut, &z[iStart], i - iStart);` |
|    213 | 8941 | `			}` |
|    495 | 8942 | `			SyBlobAppend(pOut, "\n", sizeof(char));` |
|    495 | 8943 | `			iStart = i + 1;` |
|    248 | 8944 | `		}` |
|      - | 8945 | `	}` |
|    130 | 8946 | `	SyBlobRelease(&sBody);` |
|    130 | 8947 | `	return rc;` |
|      2 | 8948 | `}` |
|      - | 8949 | ``/* The `- Enum cases [N]` block php prints where an enum's cases would otherwise`` |
|      - | 8950 | ` * be listed among its constants. A backed case shows its value UNQUOTED. */` |
|      4 | 8951 | `static sxi32 ReflectExportEnumCases(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass)` |
|      1 | 8952 | `{` |
|      5 | 8953 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      - | 8954 | `	sxu32 n;` |
|      5 | 8955 | `	SyBlobFormat(pOut, "\n  - Enum cases [%u] {\n", SySetUsed(&pClass->aEnumCases));` |
|     11 | 8956 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|      7 | 8957 | `		SyBlobAppend(pOut, "    Case ", sizeof("    Case ")-1);` |
|      7 | 8958 | `		SyBlobAppend(pOut, SyStringData(&apCase[n]->sName), SyStringLength(&apCase[n]->sName));` |
|      7 | 8959 | `		if( pClass->nEnumBacking ){` |
|      5 | 8960 | `			ph7_value *pVal = 0;` |
|      - | 8961 | `			ph7_class_instance *pObj;` |
|      5 | 8962 | `			sxi32 rc = ReflectConstSlot(pCtx, pClass, apCase[n], &pVal);` |
|      5 | 8963 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8964 | `				return rc;` |
|      - | 8965 | `			}` |
|      5 | 8966 | `			pObj = (pVal && (pVal->iFlags & MEMOBJ_OBJ)) ? (ph7_class_instance *)pVal->x.pOther : 0;` |
|      5 | 8967 | `			if( pObj ){` |
|      5 | 8968 | `				ph7_value *pBacking = PH7_NativeAttr(pObj, "value");` |
|      5 | 8969 | `				if( pBacking ){` |
|      - | 8970 | `					ph7_value sTmp;` |
|      - | 8971 | `					const char *zText;` |
|      - | 8972 | `					int nText;` |
|      5 | 8973 | `					PH7_MemObjInit(pCtx->pVm, &sTmp);` |
|      5 | 8974 | `					PH7_MemObjStore(pBacking, &sTmp);` |
|      5 | 8975 | `					zText = ph7_value_to_string(&sTmp, &nText);` |
|      5 | 8976 | `					SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|      5 | 8977 | `					if( nText > 0 ){` |
|      5 | 8978 | `						SyBlobAppend(pOut, zText, (sxu32)nText);` |
|      2 | 8979 | `					}` |
|      5 | 8980 | `					PH7_MemObjRelease(&sTmp);` |
|      2 | 8981 | `				}` |
|      2 | 8982 | `			}` |
|      2 | 8983 | `		}` |
|      7 | 8984 | `		SyBlobAppend(pOut, "\n", sizeof(char));` |
|      4 | 8985 | `	}` |
|      5 | 8986 | `	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|      5 | 8987 | `	return SXRET_OK;` |
|      3 | 8988 | `}` |
|      - | 8989 | `/* The Class / Interface / Enum block. */` |
|     28 | 8990 | `static sxi32 ReflectExportClassBlock(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass)` |
|      1 | 8991 | `{` |
|     29 | 8992 | `	ph7_vm *pVm = pCtx->pVm;` |
|     29 | 8993 | `	int bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;` |
|     29 | 8994 | `	int bEnum = (pClass->iFlags & PH7_CLASS_ENUM) != 0;` |
|     29 | 8995 | `	int bIface = (pClass->iFlags & PH7_CLASS_INTERFACE) != 0;` |
|      - | 8996 | `	SySet aMembers, aIface;` |
|     29 | 8997 | `	sxu32 n, nConst = 0, nStaticProp = 0, nProp = 0, nStaticMeth = 0, nMeth = 0;` |
|     29 | 8998 | `	sxi32 rc = SXRET_OK;` |
|      - | 8999 | `	int iPass;` |
|     29 | 9000 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|     29 | 9001 | `	SySetInit(&aIface, &pVm->sAllocator, sizeof(ph7_class *));` |
|     29 | 9002 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|     29 | 9003 | `	ReflectInterfacesOf(pClass, &aIface);` |
|      - | 9004 | `	/* ---- head ---- */` |
|     29 | 9005 | `	if( bIface ){` |
|      9 | 9006 | `		SyBlobAppend(pOut, "Interface [ <", sizeof("Interface [ <")-1);` |
|     25 | 9007 | `	}else if( bEnum ){` |
|      5 | 9008 | `		SyBlobAppend(pOut, "Enum [ <", sizeof("Enum [ <")-1);` |
|      3 | 9009 | `	}else{` |
|     17 | 9010 | `		SyBlobAppend(pOut, "Class [ <", sizeof("Class [ <")-1);` |
|      - | 9011 | `	}` |
|     29 | 9012 | `	ReflectExportKind(pOut, bInternal);` |
|     29 | 9013 | `	SyBlobAppend(pOut, "> ", sizeof("> ")-1);` |
|      - | 9014 | `	/* php tags a class that has an iteration handler, which its Traversable` |
|      - | 9015 | `	 * implementers have — and so does anything with a HOOKED property, because` |
|      - | 9016 | `	 * that is how php 8.4 walks one. An INTERFACE never carries one: php` |
|      - | 9017 | `	 * installs the handler when a CLASS implements Traversable, so` |
|      - | 9018 | ``	 * `interface I extends Iterator` prints no tag. */`` |
|      - | 9019 | `	{` |
|     35 | 9020 | `		int bIterable = !bIface && pVm->pTraversableClass != 0` |
|     38 | 9021 | `			&& PH7_VmInstanceOf(pClass, pVm->pTraversableClass);` |
|    171 | 9022 | `		for( n = 0 ; !bIterable && n < SySetUsed(&aMembers) ; n++ ){` |
|    143 | 9023 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|    142 | 9024 | `			if( pM->iKind == REFLECT_MEMBER_PROP && pM->pAttr` |
|     55 | 9025 | `			 && (pM->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) ){` |
|      3 | 9026 | `				bIterable = 1;` |
|      1 | 9027 | `			}` |
|     72 | 9028 | `		}` |
|     29 | 9029 | `		if( bIterable ){` |
|      5 | 9030 | `			SyBlobAppend(pOut, "<iterateable> ", sizeof("<iterateable> ")-1);` |
|      2 | 9031 | `		}` |
|      - | 9032 | `	}` |
|     29 | 9033 | `	if( bIface ){` |
|      9 | 9034 | `		SyBlobFormat(pOut, "interface %z", &pClass->sName);` |
|     25 | 9035 | `	}else if( bEnum ){` |
|      5 | 9036 | `		SyBlobFormat(pOut, "enum %z", &pClass->sName);` |
|      5 | 9037 | `		if( pClass->nEnumBacking ){` |
|      3 | 9038 | `			SyBlobFormat(pOut, ": %s", (pClass->nEnumBacking & MEMOBJ_INT) ? "int" : "string");` |
|      1 | 9039 | `		}` |
|      3 | 9040 | `	}else{` |
|     17 | 9041 | `		if( pClass->iFlags & PH7_CLASS_ABSTRACT ){` |
|      3 | 9042 | `			SyBlobAppend(pOut, "abstract ", sizeof("abstract ")-1);` |
|      1 | 9043 | `		}` |
|     17 | 9044 | `		if( pClass->iFlags & PH7_CLASS_FINAL ){` |
|      3 | 9045 | `			SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|      1 | 9046 | `		}` |
|     17 | 9047 | `		if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      3 | 9048 | `			SyBlobAppend(pOut, "readonly ", sizeof("readonly ")-1);` |
|      1 | 9049 | `		}` |
|     17 | 9050 | `		SyBlobFormat(pOut, "class %z", &pClass->sName);` |
|     17 | 9051 | `		if( pClass->pBase ){` |
|      5 | 9052 | `			SyBlobFormat(pOut, " extends %z", &pClass->pBase->sName);` |
|      2 | 9053 | `		}` |
|      - | 9054 | `	}` |
|     29 | 9055 | `	if( SySetUsed(&aIface) > 0 ){` |
|     17 | 9056 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIface);` |
|      - | 9057 | `		/* An interface EXTENDS what a class implements. */` |
|     25 | 9058 | `		SyBlobAppend(pOut, bIface ? " extends " : " implements ",` |
|      8 | 9059 | `			bIface ? sizeof(" extends ")-1 : sizeof(" implements ")-1);` |
|     39 | 9060 | `		for( n = 0 ; n < SySetUsed(&aIface) ; n++ ){` |
|     23 | 9061 | `			if( n > 0 ){` |
|      7 | 9062 | `				SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|      3 | 9063 | `			}` |
|     23 | 9064 | `			SyBlobFormat(pOut, "%z", &apIface[n]->sName);` |
|     12 | 9065 | `		}` |
|      8 | 9066 | `	}` |
|     29 | 9067 | `	SyBlobAppend(pOut, " ] {\n", sizeof(" ] {\n")-1);` |
|     29 | 9068 | `	if( !bInternal && SyStringLength(&pClass->sFile) > 0 ){` |
|     21 | 9069 | `		SyBlobAppend(pOut, "  @@ ", sizeof("  @@ ")-1);` |
|     21 | 9070 | `		SyBlobAppend(pOut, SyStringData(&pClass->sFile), SyStringLength(&pClass->sFile));` |
|     21 | 9071 | `		SyBlobFormat(pOut, " %u-%u\n", pClass->nLine, pClass->nEndLine);` |
|     10 | 9072 | `	}` |
|      - | 9073 | `	/* ---- counts ---- */` |
|    181 | 9074 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    153 | 9075 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|    153 | 9076 | `		if( pM->iKind == REFLECT_MEMBER_CONST ){` |
|     25 | 9077 | `			if( !(bEnum && (pM->pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)) ){` |
|     19 | 9078 | `				nConst++;` |
|     10 | 9079 | `			}` |
|    141 | 9080 | `		}else if( pM->iKind == REFLECT_MEMBER_PROP ){` |
|     55 | 9081 | `			if( pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){ nStaticProp++; }else{ nProp++; }` |
|     28 | 9082 | `		}else{` |
|     75 | 9083 | `			if( pM->pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){ nStaticMeth++; }else{ nMeth++; }` |
|      - | 9084 | `		}` |
|     77 | 9085 | `	}` |
|      - | 9086 | `	/* ---- enum cases, then constants ---- */` |
|     29 | 9087 | `	if( bEnum ){` |
|      5 | 9088 | `		rc = ReflectExportEnumCases(pCtx, pOut, pClass);` |
|      5 | 9089 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 9090 | `			goto done;` |
|      - | 9091 | `		}` |
|      2 | 9092 | `	}` |
|     29 | 9093 | `	SyBlobFormat(pOut, "\n  - Constants [%u] {\n", nConst);` |
|    181 | 9094 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    153 | 9095 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|    152 | 9096 | `		if( pM->iKind != REFLECT_MEMBER_CONST` |
|     89 | 9097 | `		 \|\| (bEnum && (pM->pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)) ){` |
|    135 | 9098 | `			continue;` |
|      - | 9099 | `		}` |
|     19 | 9100 | `		SyBlobAppend(pOut, "    ", sizeof("    ")-1);` |
|     19 | 9101 | `		rc = ReflectExportConstLine(pCtx, pOut, pClass, pM->pAttr, &pM->sKey);` |
|     19 | 9102 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 9103 | `			goto done;` |
|      - | 9104 | `		}` |
|     10 | 9105 | `	}` |
|     29 | 9106 | `	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|      - | 9107 | `	/* ---- properties and methods, statics first ---- */` |
|    141 | 9108 | `	for( iPass = 0 ; iPass < 4 ; iPass++ ){` |
|    113 | 9109 | `		int bStatic = (iPass == 0 \|\| iPass == 1);` |
|    113 | 9110 | `		int bMethods = (iPass == 1 \|\| iPass == 3);` |
|    113 | 9111 | `		int bFirst = 1;` |
|      - | 9112 | `		static const char *azTitle[] = {` |
|      - | 9113 | `			"\n  - Static properties [%u] {\n", "\n  - Static methods [%u] {\n",` |
|      - | 9114 | `			"\n  - Properties [%u] {\n", "\n  - Methods [%u] {\n"` |
|      - | 9115 | `		};` |
|      - | 9116 | `		sxu32 aCount[4];` |
|    113 | 9117 | `		aCount[0] = nStaticProp;` |
|    113 | 9118 | `		aCount[1] = nStaticMeth;` |
|    113 | 9119 | `		aCount[2] = nProp;` |
|    113 | 9120 | `		aCount[3] = nMeth;` |
|    113 | 9121 | `		SyBlobFormat(pOut, azTitle[iPass], aCount[iPass]);` |
|    721 | 9122 | `		for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    609 | 9123 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - | 9124 | `			int bIsStatic;` |
|    609 | 9125 | `			if( pM->iKind == REFLECT_MEMBER_CONST ){` |
|     97 | 9126 | `				continue;` |
|      - | 9127 | `			}` |
|    513 | 9128 | `			if( bMethods != (pM->iKind == REFLECT_MEMBER_METHOD) ){` |
|    257 | 9129 | `				continue;` |
|      - | 9130 | `			}` |
|    331 | 9131 | `			bIsStatic = bMethods ? (pM->pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0` |
|    182 | 9132 | `				: (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|    257 | 9133 | `			if( bIsStatic != bStatic ){` |
|    129 | 9134 | `				continue;` |
|      - | 9135 | `			}` |
|    129 | 9136 | `			if( bMethods ){` |
|      - | 9137 | `				ReflectFuncRef sRef;` |
|     75 | 9138 | `				if( !bFirst ){` |
|     47 | 9139 | `					SyBlobAppend(pOut, "\n", sizeof(char));` |
|     23 | 9140 | `				}` |
|     75 | 9141 | `				bFirst = 0;` |
|     75 | 9142 | `				ReflectFuncFromMethod(pClass, pM->pMeth, &sRef);` |
|     75 | 9143 | `				rc = ReflectExportFuncBlock(pCtx, pOut, &sRef, "    ", pClass);` |
|     75 | 9144 | `				if( rc != SXRET_OK ){` |
|    ! 0 | 9145 | `					goto done;` |
|      - | 9146 | `				}` |
|     38 | 9147 | `			}else{` |
|     55 | 9148 | `				SyBlobAppend(pOut, "    ", sizeof("    ")-1);` |
|     55 | 9149 | `				ReflectExportPropLine(pCtx, pOut, pM->pAttr, &pM->sKey);` |
|      - | 9150 | `			}` |
|     65 | 9151 | `		}` |
|    113 | 9152 | `		SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|     57 | 9153 | `	}` |
|     29 | 9154 | `	SyBlobAppend(pOut, "}\n", sizeof("}\n")-1);` |
|     14 | 9155 | `done:` |
|     29 | 9156 | `	SySetRelease(&aMembers);` |
|     29 | 9157 | `	SySetRelease(&aIface);` |
|     29 | 9158 | `	return rc;` |
|      1 | 9159 | `}` |
|      - | 9160 | `/* ---- the five __toString() entry points ---- */` |
|     28 | 9161 | `static int ReflectExportClassSelf(ph7_context *pCtx)` |
|      1 | 9162 | `{` |
|     29 | 9163 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 9164 | `	SyBlob sOut;` |
|      - | 9165 | `	sxi32 rc;` |
|     29 | 9166 | `	if( pClass == 0 ){` |
|    ! 0 | 9167 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 9168 | `		return PH7_OK;` |
|      - | 9169 | `	}` |
|     29 | 9170 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     29 | 9171 | `	rc = ReflectExportClassBlock(pCtx, &sOut, pClass);` |
|     29 | 9172 | `	if( rc == SXRET_OK ){` |
|     29 | 9173 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     14 | 9174 | `	}` |
|     29 | 9175 | `	SyBlobRelease(&sOut);` |
|     29 | 9176 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|     15 | 9177 | `}` |
|     54 | 9178 | `static int ReflectExportFuncSelf(ph7_context *pCtx)` |
|      2 | 9179 | `{` |
|      - | 9180 | `	ReflectFuncRef sRef;` |
|      - | 9181 | `	SyBlob sOut;` |
|      - | 9182 | `	sxi32 rc;` |
|     56 | 9183 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 | 9184 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 9185 | `		return PH7_OK;` |
|      - | 9186 | `	}` |
|     56 | 9187 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     56 | 9188 | `	rc = ReflectExportFuncBlock(pCtx, &sOut, &sRef, "", 0);` |
|     56 | 9189 | `	if( rc == SXRET_OK ){` |
|     56 | 9190 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     27 | 9191 | `	}` |
|     56 | 9192 | `	SyBlobRelease(&sOut);` |
|     56 | 9193 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|     29 | 9194 | `}` |
|     10 | 9195 | `static int ReflectExportParamSelf(ph7_context *pCtx)` |
|      1 | 9196 | `{` |
|      - | 9197 | `	ReflectFuncRef sRef;` |
|      - | 9198 | `	ReflectParamDesc sDesc;` |
|      - | 9199 | `	SyBlob sOut;` |
|     11 | 9200 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) ){` |
|    ! 0 | 9201 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 9202 | `		return PH7_OK;` |
|      - | 9203 | `	}` |
|     11 | 9204 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     11 | 9205 | `	ReflectExportParamLine(pCtx, &sOut, &sDesc);` |
|     11 | 9206 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     11 | 9207 | `	SyBlobRelease(&sOut);` |
|     11 | 9208 | `	return PH7_OK;` |
|      6 | 9209 | `}` |
|      4 | 9210 | `static int ReflectExportPropSelf(ph7_context *pCtx)` |
|      1 | 9211 | `{` |
|      - | 9212 | `	ReflectMemberRef sRef;` |
|      - | 9213 | `	SyBlob sOut;` |
|      - | 9214 | `	SyString sKey;` |
|      5 | 9215 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 9216 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 9217 | `		return PH7_OK;` |
|      - | 9218 | `	}` |
|      5 | 9219 | `	SyStringInitFromBuf(&sKey, sRef.zName, sRef.nName);` |
|      5 | 9220 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|      5 | 9221 | `	ReflectExportPropLine(pCtx, &sOut, sRef.pAttr, &sKey);` |
|      5 | 9222 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|      5 | 9223 | `	SyBlobRelease(&sOut);` |
|      5 | 9224 | `	return PH7_OK;` |
|      3 | 9225 | `}` |
|      6 | 9226 | `static int ReflectExportConstSelf(ph7_context *pCtx)` |
|      1 | 9227 | `{` |
|      - | 9228 | `	ReflectMemberRef sRef;` |
|      - | 9229 | `	SyBlob sOut;` |
|      - | 9230 | `	SyString sKey;` |
|      - | 9231 | `	sxi32 rc;` |
|      7 | 9232 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 9233 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 9234 | `		return PH7_OK;` |
|      - | 9235 | `	}` |
|      7 | 9236 | `	SyStringInitFromBuf(&sKey, sRef.zName, sRef.nName);` |
|      7 | 9237 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|      7 | 9238 | `	rc = ReflectExportConstLine(pCtx, &sOut, sRef.pClass, sRef.pAttr, &sKey);` |
|      7 | 9239 | `	if( rc == SXRET_OK ){` |
|      7 | 9240 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|      3 | 9241 | `	}` |
|      7 | 9242 | `	SyBlobRelease(&sOut);` |
|      7 | 9243 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|      4 | 9244 | `}` |
|      - | 9245 | `/*` |
|      - | 9246 | ` * Install the Reflection API — ~25 classes, all of them declared from C.` |
|      - | 9247 | ` *` |
|      - | 9248 | `` * There is no global thunk table left to register: every `__reflect_*` and`` |
|      - | 9249 | `` * `__phl_rcinfo` became a method of the class that always owned it, so`` |
|      - | 9250 | ` * Reflection adds no name to php's global function namespace at all. Called` |
|      - | 9251 | ` * from PH7_VmInit while pVm->bCompilingBuiltin is set, right after the core` |
|      - | 9252 | ` * builtin chunks (Exception and friends have to exist already).` |
|      - | 9253 | ` */` |
|   5740 | 9254 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|      5 | 9255 | `{` |
|      - | 9256 | `	sxi32 rc;` |
|      - | 9257 | `	/* Where chunk 1 was: Reflector, Reflection, ReflectionException,` |
|      - | 9258 | `	 * ReflectionClass and ReflectionObject are native now. Chunks 2 and 3 name` |
|      - | 9259 | ``	 * `Reflector` in their own `implements` clauses, so this has to run first. */`` |
|   5745 | 9260 | `	rc = PH7_VmInstallReflectionClass(&(*pVm));` |
|   5745 | 9261 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9262 | `		return rc;` |
|      - | 9263 | `	}` |
|      - | 9264 | `	/* Where chunk 2 was: ReflectionFunctionAbstract, ReflectionFunction,` |
|      - | 9265 | `	 * ReflectionMethod and ReflectionParameter are native now. */` |
|   5745 | 9266 | `	rc = PH7_VmInstallReflectionFunc(&(*pVm));` |
|   5745 | 9267 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9268 | `		return rc;` |
|      - | 9269 | `	}` |
|   5745 | 9270 | `	rc = PH7_VmInstallReflectionHookType(&(*pVm));` |
|   5745 | 9271 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9272 | `		return rc;` |
|      - | 9273 | `	}` |
|      - | 9274 | `	/* Where chunk 3 was: ReflectionProperty and ReflectionClassConstant are` |
|      - | 9275 | `	 * native now, and PropertyHookType is a native ENUM. */` |
|   5745 | 9276 | `	rc = PH7_VmInstallReflectionMember(&(*pVm));` |
|   5745 | 9277 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9278 | `		return rc;` |
|      - | 9279 | `	}` |
|      - | 9280 | `	/* Where chunk 4 was: the four type classes are native now (see` |
|      - | 9281 | `	 * PH7_VmInstallReflectionTypes). Stringable exists by this point. */` |
|   5745 | 9282 | `	rc = PH7_VmInstallReflectionTypes(&(*pVm));` |
|   5745 | 9283 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9284 | `		return rc;` |
|      - | 9285 | `	}` |
|      - | 9286 | `	/* Where chunk 5 was: ReflectionGenerator/Fiber and the four standalone` |
|      - | 9287 | `	 * classes chunk 6 used to hold are native now. Reflector (chunk 1) exists. */` |
|   5745 | 9288 | `	rc = PH7_VmInstallReflectionSmall(&(*pVm));` |
|   5745 | 9289 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9290 | `		return rc;` |
|      - | 9291 | `	}` |
|      - | 9292 | `	/* Where chunk 6 was: the three ReflectionEnum classes are native now, and` |
|      - | 9293 | `	 * the class DESCRIPTOR they read (__phl_rcinfo) has no callers left. */` |
|   5745 | 9294 | `	rc = PH7_VmInstallReflectionEnum(&(*pVm));` |
|   5745 | 9295 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9296 | `		return rc;` |
|      - | 9297 | `	}` |
|      - | 9298 | `	/* Where chunk 7 was: ReflectionAttribute is native now, and with it the` |
|      - | 9299 | `	 * builder (__reflect_build_attrs) and the two helpers it needed. */` |
|   5745 | 9300 | `	rc = PH7_VmInstallReflectionAttribute(&(*pVm));` |
|   5745 | 9301 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9302 | `		return rc;` |
|      - | 9303 | `	}` |
|      - | 9304 | `	/* Where chunk 9 was: the export format is ReflectExportClassSelf() and its` |
|      - | 9305 | `	 * four siblings above. Nothing of the Reflection library is PHP any more,` |
|      - | 9306 | `	 * so vm_builtin_reflection_lib.c is gone and this IS the installer. */` |
|   5745 | 9307 | `	return SXRET_OK;` |
|   2875 | 9308 | `}` |
|      - | 9309 |  |
