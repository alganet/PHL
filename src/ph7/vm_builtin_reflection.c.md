# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5138/5948 lines (86.38%)

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
|   3078 |   31 | `static ph7_class * ReflectResolveClass(ph7_vm *pVm, ph7_value *pArg)` |
|      5 |   32 | `{` |
|      - |   33 | `	ph7_class *pClass;` |
|   3083 |   34 | `	pClass = PH7_VmExtractClassFromValue(pVm, pArg);` |
|   3083 |   35 | `	if( pClass == 0 && ph7_value_is_string(pArg) ){` |
|      - |   36 | `		const char *zName;` |
|      - |   37 | `		int nLen;` |
|     20 |   38 | `		zName = ph7_value_to_string(pArg, &nLen);` |
|     20 |   39 | `		if( nLen > 0 ){` |
|     20 |   40 | `			pClass = PH7_VmTriggerAutoload(pVm, zName, (sxu32)nLen, FALSE);` |
|      9 |   41 | `		}` |
|      9 |   42 | `	}` |
|   3083 |   43 | `	return pClass;` |
|      5 |   44 | `}` |
|      - |   45 | `/*` |
|      - |   46 | ` * Hand a freshly created class instance to the caller. The return slot` |
|      - |   47 | ` * takes over the initial reference from PH7_NewClassInstance (iRef=1):` |
|      - |   48 | ` * no extra iRef++ here (see the synthesized-object invariant — a stray` |
|      - |   49 | ` * bump leaks the object and disables its __destruct).` |
|      - |   50 | ` */` |
|   1126 |   51 | `static int ReflectResultObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|      5 |   52 | `{` |
|   1131 |   53 | `	if( pObj == 0 ){` |
|    ! 0 |   54 | `		ph7_result_null(pCtx);` |
|    ! 0 |   55 | `		return PH7_OK;` |
|      - |   56 | `	}` |
|   1131 |   57 | `	PH7_MemObjRelease(pCtx->pRet);` |
|   1131 |   58 | `	pCtx->pRet->x.pOther = pObj;` |
|   1131 |   59 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|   1131 |   60 | `	return PH7_OK;` |
|    568 |   61 | `}` |
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
|    346 |   78 | `static void ReflectAddInterface(ph7_class *pIface, SySet *pOut, int iDepth)` |
|      2 |   79 | `{` |
|      - |   80 | `	ph7_class **apKnown;` |
|      - |   81 | `	sxu32 n;` |
|    348 |   82 | `	if( pIface == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|    ! 0 |   83 | `		return;` |
|      - |   84 | `	}` |
|      - |   85 | `	/* Parents of an interface come along too (interface B extends A) */` |
|    348 |   86 | `	if( pIface->pBase ){` |
|     95 |   87 | `		ReflectAddInterface(pIface->pBase, pOut, iDepth + 1);` |
|     47 |   88 | `	}` |
|      - |   89 | `	/* Some engines record extended interfaces in aInterface as well */` |
|    348 |   90 | `	apKnown = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|    348 |   91 | `	for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|    ! 0 |   92 | `		ReflectAddInterface(apKnown[n], pOut, iDepth + 1);` |
|    ! 0 |   93 | `	}` |
|      - |   94 | `	/* Dedup by pointer */` |
|    348 |   95 | `	apKnown = (ph7_class **)SySetBasePtr(pOut);` |
|    542 |   96 | `	for( n = 0 ; n < SySetUsed(pOut) ; n++ ){` |
|    231 |   97 | `		if( apKnown[n] == pIface ){` |
|     37 |   98 | `			return;` |
|      - |   99 | `		}` |
|     98 |  100 | `	}` |
|    312 |  101 | `	SySetPut(pOut, (const void *)&pIface);` |
|    175 |  102 | `}` |
|      - |  103 | `/*` |
|      - |  104 | ` * Collect the transitive set of interfaces implemented by pClass:` |
|      - |  105 | ` * the parent chain's interfaces first, then the class's own.` |
|      - |  106 | ` */` |
|    350 |  107 | `static void ReflectCollectInterfaces(ph7_class *pClass, SySet *pOut, int iDepth)` |
|      2 |  108 | `{` |
|      - |  109 | `	ph7_class **apIface;` |
|      - |  110 | `	sxu32 n;` |
|    352 |  111 | `	if( pClass == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|    ! 0 |  112 | `		return;` |
|      - |  113 | `	}` |
|    352 |  114 | `	if( pClass->pBase ){` |
|    110 |  115 | `		ReflectCollectInterfaces(pClass->pBase, pOut, iDepth + 1);` |
|     54 |  116 | `	}` |
|    352 |  117 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    556 |  118 | `	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){` |
|    206 |  119 | `		ReflectAddInterface(apIface[n], pOut, iDepth + 1);` |
|    104 |  120 | `	}` |
|    177 |  121 | `}` |
|      - |  122 | `/*` |
|      - |  123 | ` * Deepest base class whose method table maps the same name to the very` |
|      - |  124 | ` * same ph7_class_method pointer: inheritance shares member pointers` |
|      - |  125 | ` * (PH7_ClassInherit), so this identifies the declaring class. Methods` |
|      - |  126 | ` * copied in from traits are not on the pBase chain and thus report the` |
|      - |  127 | ` * using class, which is what PHP reports too.` |
|      - |  128 | ` */` |
|  10644 |  129 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth)` |
|      5 |  130 | `{` |
|  10649 |  131 | `	ph7_class *pDecl = pClass;` |
|  10649 |  132 | `	ph7_class *pBase = pClass->pBase;` |
|  10649 |  133 | `	int iDepth = 0;` |
|  12751 |  134 | `	while( pBase && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|      - |  135 | `		SyHashEntry *pEntry;` |
|   4918 |  136 | `		pEntry = SyHashGet(&pBase->hMethod, (const void *)SyStringData(&pMeth->sFunc.sName),` |
|   1639 |  137 | `			SyStringLength(&pMeth->sFunc.sName));` |
|   3279 |  138 | `		if( pEntry == 0 \|\| (ph7_class_method *)pEntry->pUserData != pMeth ){` |
|    589 |  139 | `			break;` |
|      - |  140 | `		}` |
|   2103 |  141 | `		pDecl = pBase;` |
|   2103 |  142 | `		pBase = pBase->pBase;` |
|   2103 |  143 | `		iDepth++;` |
|      1 |  144 | `	}` |
|  10649 |  145 | `	return pDecl;` |
|      5 |  146 | `}` |
|      - |  147 | `/*` |
|      - |  148 | ` * The interface list php reports for pClass: the transitive set, plus an` |
|      - |  149 | `` * INTERFACE's own parents (`interface B extends A` lists A). Caller owns the`` |
|      - |  150 | ` * set (SySetInit with sizeof(ph7_class *)).` |
|      - |  151 | ` */` |
|    242 |  152 | `static void ReflectInterfacesOf(ph7_class *pClass, SySet *pOut)` |
|      2 |  153 | `{` |
|    244 |  154 | `	ReflectCollectInterfaces(pClass, pOut, 0);` |
|    244 |  155 | `	if( (pClass->iFlags & PH7_CLASS_INTERFACE) && pClass->pBase ){` |
|     49 |  156 | `		ReflectAddInterface(pClass->pBase, pOut, 0);` |
|     24 |  157 | `	}` |
|    244 |  158 | `}` |
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
|    988 |  243 | `static void ReflectMembers(ph7_vm *pVm, ph7_class *pClass, SySet *pOut, int bLookup)` |
|      5 |  244 | `{` |
|      - |  245 | `	ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];` |
|    993 |  246 | `	ph7_class *pWalk = pClass;` |
|      - |  247 | `	SyHashEntry *pEntry;` |
|      - |  248 | `	SySet aTmp;` |
|    993 |  249 | `	int bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;` |
|      - |  250 | `	int iPart;` |
|    993 |  251 | `	sxu32 nChain = 0, iLevel, nLev, nT;` |
|   2115 |  252 | `	while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){` |
|   1127 |  253 | `		aChain[nChain++] = pWalk;` |
|   1127 |  254 | `		pWalk = pWalk->pBase;` |
|      5 |  255 | `	}` |
|    993 |  256 | `	SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));` |
|      - |  257 | `	/* PROPERTIES of an INTERNAL class come out base-first, and that is php's` |
|      - |  258 | `	 * registration order rather than a rule of its own: a user class declares its` |
|      - |  259 | ``	 * own properties and THEN inherits (`class B extends A` reports B's before`` |
|      - |  260 | `	 * A's), while an internal one is registered against its parent and declares` |
|      - |  261 | `	 * afterwards — so ErrorException reports Exception's five and then its own` |
|      - |  262 | `	 * $severity. Only the property table flips: php lists an internal class's` |
|      - |  263 | `	 * METHODS and CONSTANTS own-first like everything else, so the walk below` |
|      - |  264 | `	 * runs the two tables in opposite level orders. */` |
|   3957 |  265 | `	for( iPart = 0 ; iPart < 3 ; iPart++ ){` |
|   6335 |  266 | `	  for( nLev = 0 ; nLev < nChain ; nLev++ ){` |
|      - |  267 | `		ph7_class *pLevel;` |
|   3371 |  268 | `		int iTab = iPart;` |
|   3371 |  269 | `		iLevel = (iPart == 0 && bInternal) ? nChain - 1 - nLev : nLev;` |
|   3371 |  270 | `		pLevel = aChain[iLevel];` |
|      - |  271 | `		/* --- Properties (hAttr, iPart 0) and constants/enum cases (hConst,` |
|      - |  272 | `		 * iPart 1) — php's two separate member namespaces. Each table is` |
|      - |  273 | `		 * collected and emitted independently; the CONSTANT flag still decides` |
|      - |  274 | `		 * which kind comes out. --- */` |
|   3371 |  275 | `		if( iPart < 2 ){` |
|   2249 |  276 | `			SyHash *pSrcHash = iTab ? &pLevel->hConst : &pLevel->hAttr;` |
|   2249 |  277 | `			SyHash *pRefHash = iTab ? &pClass->hConst : &pClass->hAttr;` |
|   2249 |  278 | `			SySetReset(&aTmp);` |
|   2249 |  279 | `			SyHashResetLoopCursor(pSrcHash);` |
|  10217 |  280 | `			while( (pEntry = SyHashGetNextEntry(pSrcHash)) != 0 ){` |
|   7973 |  281 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   7973 |  282 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|   7973 |  283 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_HIDDEN ){` |
|      - |  284 | `					/* A native class's engine slot: php holds that state in its own C` |
|      - |  285 | `					 * struct and reports no property for it at all. */` |
|    430 |  286 | `					continue;` |
|      - |  287 | `				}` |
|   7545 |  288 | `				if( iLevel == 0 ){` |
|      - |  289 | `					sxu32 j;` |
|      - |  290 | `					/* Own = declared here or by an off-chain provider (trait) */` |
|   7511 |  291 | `					for( j = 1 ; j < nChain ; j++ ){` |
|    627 |  292 | `						if( aChain[j] == pDecl ){ break; }` |
|    149 |  293 | `					}` |
|   7215 |  294 | `					if( j < nChain ){ continue; }` |
|   3445 |  295 | `				}else{` |
|      - |  296 | `					SyHashEntry *pSub;` |
|    331 |  297 | `					if( pDecl != pLevel ){ continue; }` |
|      - |  298 | `					/* A base's PRIVATE member is not part of the subclass's` |
|      - |  299 | `					 * surface — php reports neither a private property nor a` |
|      - |  300 | `					 * private constant of a parent on the child. PHL's` |
|      - |  301 | `					 * inheritance copies them down all the same, so the filter` |
|      - |  302 | `					 * has to be here. */` |
|    331 |  303 | `					if( !bLookup && pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){ continue; }` |
|      - |  304 | `					/* Must still be the visible member in the reflected class */` |
|    249 |  305 | `					pSub = SyHashGet(pRefHash, pEntry->pKey, pEntry->nKeyLen);` |
|    249 |  306 | `					if( pSub == 0 \|\| pSub->pUserData != (void *)pAttr ){ continue; }` |
|      - |  307 | `				}` |
|   7133 |  308 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|      5 |  309 | `			}` |
|      - |  310 | `			/* Forward: hAttr/hConst iterate in DECLARATION order (tail inserts),` |
|      - |  311 | `			 * so members come out in the order php reports them. */` |
|   9377 |  312 | `			for( nT = 0 ; nT < SySetUsed(&aTmp) ; nT++ ){` |
|   7133 |  313 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT);` |
|   7133 |  314 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;` |
|      - |  315 | `				ReflectMember sMember;` |
|   7133 |  316 | `				sMember.iKind = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|   3564 |  317 | `					? REFLECT_MEMBER_CONST : REFLECT_MEMBER_PROP;` |
|   7133 |  318 | `				sMember.sKey = pAttr->sName;` |
|   7133 |  319 | `				sMember.pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|   7133 |  320 | `				sMember.pAttr = pAttr;` |
|   7133 |  321 | `				sMember.pMeth = 0;` |
|   7133 |  322 | `				SySetPut(pOut, (const void *)&sMember);` |
|   3569 |  323 | `			}` |
|   2249 |  324 | `			continue;` |
|      - |  325 | `		}` |
|      - |  326 | `		/* --- Methods. The reported name is the hash-entry KEY, not the` |
|      - |  327 | `		 * function's own name (see ReflectMember::sKey). --- */` |
|   1127 |  328 | `		SySetReset(&aTmp);` |
|   1127 |  329 | `		SyHashResetLoopCursor(&pLevel->hMethod);` |
|   6719 |  330 | `		while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){` |
|   5597 |  331 | `			ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   5597 |  332 | `			ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|      - |  333 | `			/* A property HOOK is compiled into a method (__phl_hook_get_NAME /` |
|      - |  334 | ``			 * __phl_hook_set_NAME) so that inheritance and `parent::` work on`` |
|      - |  335 | `			 * it, but php has no method there at all: it reports the hook on` |
|      - |  336 | `			 * the PROPERTY. Listing it would put a PHL-only name on a` |
|      - |  337 | `			 * php-visible surface. */` |
|   5592 |  338 | `			if( pEntry->nKeyLen > sizeof("__phl_hook_")-1` |
|   3638 |  339 | `			 && SyMemcmp(pEntry->pKey, "__phl_hook_", sizeof("__phl_hook_")-1) == 0 ){` |
|    713 |  340 | `				continue;` |
|      - |  341 | `			}` |
|   4885 |  342 | `			if( iLevel == 0 ){` |
|      - |  343 | `				sxu32 j;` |
|   4763 |  344 | `				for( j = 1 ; j < nChain ; j++ ){` |
|    881 |  345 | `					if( aChain[j] == pDecl ){ break; }` |
|    251 |  346 | `				}` |
|   4263 |  347 | `				if( j < nChain ){ continue; }` |
|   1943 |  348 | `			}else{` |
|      - |  349 | `				SyHashEntry *pSub;` |
|    623 |  350 | `				if( pDecl != pLevel ){ continue; }` |
|      - |  351 | `				/* Same rule as the members above: a base's PRIVATE method is` |
|      - |  352 | ``				 * not on the subclass's surface. `class B extends A` lists`` |
|      - |  353 | `				 * only A::q when A::p is private — php's inheritance never` |
|      - |  354 | `				 * hands the child a private, and PH7_ClassInherit's copy-down` |
|      - |  355 | `				 * does, so it is filtered here. */` |
|    433 |  356 | `				if( !bLookup && pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){ continue; }` |
|    391 |  357 | `				pSub = SyHashGet(&pClass->hMethod, pEntry->pKey, pEntry->nKeyLen);` |
|    391 |  358 | `				if( pSub == 0 \|\| pSub->pUserData != (void *)pMeth ){` |
|      - |  359 | `					/* Overridden below this level: already reported */` |
|     53 |  360 | `					continue;` |
|      - |  361 | `				}` |
|      - |  362 | `			}` |
|   4221 |  363 | `			SySetPut(&aTmp, (const void *)&pEntry);` |
|      3 |  364 | `		}` |
|   5345 |  365 | `		for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|   4221 |  366 | `			SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|      - |  367 | `			ReflectMember sMember;` |
|   4221 |  368 | `			sMember.iKind = REFLECT_MEMBER_METHOD;` |
|   4221 |  369 | `			SyStringInitFromBuf(&sMember.sKey, (const char *)pE->pKey, pE->nKeyLen);` |
|   4221 |  370 | `			sMember.pMeth = (ph7_class_method *)pE->pUserData;` |
|   4221 |  371 | `			sMember.pDecl = ReflectMethodDeclClass(pClass, sMember.pMeth);` |
|   4221 |  372 | `			sMember.pAttr = 0;` |
|   4221 |  373 | `			SySetPut(pOut, (const void *)&sMember);` |
|   2112 |  374 | `		}` |
|    566 |  375 | `	  }` |
|   1487 |  376 | `	}` |
|    993 |  377 | `	SySetRelease(&aTmp);` |
|    993 |  378 | `}` |
|      - |  379 | `/* Does a collected member name match zName exactly? */` |
|   3428 |  380 | `static int ReflectKeyIs(const ReflectMember *pM, const char *zName, int nName)` |
|      5 |  381 | `{` |
|   3976 |  382 | `	return SyStringLength(&pM->sKey) == (sxu32)nName` |
|   3428 |  383 | `		&& SyMemcmp(SyStringData(&pM->sKey), zName, (sxu32)nName) == 0;` |
|      5 |  384 | `}` |
|      - |  385 | `/*` |
|      - |  386 | ` * php's METHOD lookup, which is NOT the listing.` |
|      - |  387 | ` *` |
|      - |  388 | ` * getMethods() hides a base's private method, but hasMethod()/getMethod() find` |
|      - |  389 | ` * one: Zend keeps the parent's private in the child's function table and reads` |
|      - |  390 | ` * that table directly. PHL's inheritance copies methods down the same way, so` |
|      - |  391 | ` * the lookup is the class's own hMethod — case-insensitively, like php.` |
|      - |  392 | ` */` |
|    804 |  393 | `static SyHashEntry * ReflectFindMethodEntry(ph7_class *pClass, const char *zName, int nName)` |
|      5 |  394 | `{` |
|      - |  395 | `	SyHashEntry *pEntry;` |
|    809 |  396 | `	if( nName < 1 ){` |
|    ! 0 |  397 | `		return 0;` |
|      - |  398 | `	}` |
|    809 |  399 | `	pEntry = SyHashGet(&pClass->hMethod, (const void *)zName, (sxu32)nName);` |
|    809 |  400 | `	if( pEntry ){` |
|    617 |  401 | `		return pEntry;` |
|      - |  402 | `	}` |
|    195 |  403 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|    613 |  404 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|    322 |  405 | `		if( (int)pEntry->nKeyLen == nName` |
|    175 |  406 | `		 && SyStrnicmp((const char *)pEntry->pKey, zName, (sxu32)nName) == 0 ){` |
|    ! 0 |  407 | `			return pEntry;` |
|      - |  408 | `		}` |
|      3 |  409 | `	}` |
|    195 |  410 | `	return 0;` |
|    407 |  411 | `}` |
|      - |  412 | `/*` |
|      - |  413 | `` * The visibility of the __construct / __clone `new` and `clone` would reach, or`` |
|      - |  414 | ` * 0 when the class has none — what isInstantiable() and isCloneable() screen on.` |
|      - |  415 | ` * The LOOKUP, not the listing: a class that inherits a private constructor is` |
|      - |  416 | ` * still not instantiable even though getMethods() does not report one.` |
|      - |  417 | ` */` |
|     54 |  418 | `static void ReflectCtorCloneVis(ph7_vm *pVm, ph7_class *pClass, sxi32 *piCtor, sxi32 *piClone)` |
|      3 |  419 | `{` |
|      - |  420 | `	ph7_class_method *pMeth;` |
|     27 |  421 | `	SXUNUSED(pVm);` |
|     57 |  422 | `	*piCtor = *piClone = 0;` |
|     57 |  423 | `	pMeth = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|     57 |  424 | `	if( pMeth ){` |
|     32 |  425 | `		*piCtor = pMeth->iProtection;` |
|     15 |  426 | `	}` |
|     57 |  427 | `	pMeth = PH7_ClassExtractMethod(pClass, "__clone", sizeof("__clone")-1);` |
|     57 |  428 | `	if( pMeth ){` |
|      7 |  429 | `		*piClone = pMeth->iProtection;` |
|      3 |  430 | `	}` |
|     57 |  431 | `}` |
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
|     34 |  443 | `static sxi32 ReflectCollectArgs(ph7_context *pCtx, ph7_value *pArray, SySet *pOut, SyString **ppNames)` |
|      1 |  444 | `{` |
|      - |  445 | `	ph7_hashmap *pMap;` |
|      - |  446 | `	ph7_hashmap_node *pEntry;` |
|     35 |  447 | `	SyString *aNames = 0;` |
|     35 |  448 | `	sxu32 nSlot = 0;` |
|      - |  449 | `	sxu32 n;` |
|     35 |  450 | `	if( ppNames ){` |
|     29 |  451 | `		*ppNames = 0;` |
|     14 |  452 | `	}` |
|     35 |  453 | `	if( !ph7_value_is_array(pArray) ){` |
|    ! 0 |  454 | `		return SXRET_OK;` |
|      - |  455 | `	}` |
|     35 |  456 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     35 |  457 | `	pEntry = pMap->pFirst;` |
|     85 |  458 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
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
|     35 |  479 | `	if( ppNames ){` |
|     29 |  480 | `		*ppNames = aNames;` |
|     14 |  481 | `	}` |
|     35 |  482 | `	return SXRET_OK;` |
|     18 |  483 | `}` |
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
|     30 |  495 | `static sxi32 ReflectAttrInstantiate(ph7_context *pCtx, ph7_value *pClassName, ph7_value *pArgs)` |
|      1 |  496 | `{` |
|     31 |  497 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  498 | `	ph7_class *pClass;` |
|      - |  499 | `	ph7_class_instance *pThis;` |
|      - |  500 | `	ph7_class_method *pCons;` |
|     31 |  501 | `	if( (pClass = ReflectResolveClass(pVm, pClassName)) == 0 ){` |
|    ! 0 |  502 | `		ph7_result_null(pCtx);` |
|    ! 0 |  503 | `		return PH7_OK;` |
|      - |  504 | `	}` |
|     31 |  505 | `	if( VmClassStaticDeferPending(pClass) ){` |
|      - |  506 | `		/* Instantiation materializes the static table (OP_NEW does it too), so a` |
|      - |  507 | `		 * broken default raises BEFORE any object exists. */` |
|    ! 0 |  508 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pVm, pClass);` |
|    ! 0 |  509 | `		if( rcMat != SXRET_OK ){` |
|    ! 0 |  510 | `			return rcMat;` |
|      - |  511 | `		}` |
|    ! 0 |  512 | `	}` |
|     31 |  513 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|     31 |  514 | `	if( pThis == 0 ){` |
|    ! 0 |  515 | `		ph7_result_null(pCtx);` |
|    ! 0 |  516 | `		return PH7_OK;` |
|      - |  517 | `	}` |
|     31 |  518 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|     31 |  519 | `	if( pCons ){` |
|      - |  520 | `		SySet aArg;` |
|      - |  521 | `		sxi32 rc;` |
|     27 |  522 | `		SyString *aNames = 0;` |
|     27 |  523 | `		SySetInit(&aArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|     27 |  524 | `		if( pArgs ){` |
|     27 |  525 | `			ReflectCollectArgs(pCtx, pArgs, &aArg, &aNames);` |
|     13 |  526 | `		}` |
|     27 |  527 | `		if( aNames ){` |
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
|     34 |  539 | `			rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|     22 |  540 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|      - |  541 | `		}` |
|     27 |  542 | `		SySetRelease(&aArg);` |
|     27 |  543 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|    ! 0 |  544 | `			PH7_ClassInstanceUnref(pThis);` |
|    ! 0 |  545 | `			return rc;` |
|      - |  546 | `		}` |
|     13 |  547 | `	}` |
|     31 |  548 | `	return ReflectResultObject(pCtx, pThis);` |
|     16 |  549 | `}` |
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
|   2858 |  602 | `static ph7_class_instance * ReflectValueClosure(ph7_vm *pVm, ph7_value *pVal)` |
|      4 |  603 | `{` |
|      - |  604 | `	ph7_class_instance *pThis;` |
|   2862 |  605 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
|   2623 |  606 | `		return 0;` |
|      - |  607 | `	}` |
|    242 |  608 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    242 |  609 | `	return (pThis->pClass == pVm->pClosureClass) ? pThis : 0;` |
|   1433 |  610 | `}` |
|      - |  611 | `/*` |
|      - |  612 | ` * Resolve a reflection callable target into its compiled function.` |
|      - |  613 | ` *   - pMethodArg a non-empty string  -> method mode: pTarget is a class name` |
|      - |  614 | ` *     or object; outputs *ppClass and *ppMeth.` |
|      - |  615 | ` *   - pTarget a Closure              -> unwrap $__fn into hFunction; *ppClosure.` |
|      - |  616 | ` *   - pTarget a string               -> hFunction (user) or hHostFunction` |
|      - |  617 | ` *     (*ppHost set, returns NULL).` |
|      - |  618 | ` * Returns the ph7_vm_func, or NULL (host function or unresolvable).` |
|      - |  619 | ` */` |
|   4090 |  620 | `static ph7_vm_func * ReflectResolveCallable(ph7_vm *pVm, ph7_value *pTarget,` |
|      - |  621 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|      - |  622 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|      5 |  623 | `{` |
|      - |  624 | `	SyHashEntry *pEntry;` |
|   4095 |  625 | `	if( ppClass ){ *ppClass = 0; }` |
|   4095 |  626 | `	if( ppMeth ){ *ppMeth = 0; }` |
|   4095 |  627 | `	if( ppHost ){ *ppHost = 0; }` |
|   4095 |  628 | `	if( ppClosure ){ *ppClosure = 0; }` |
|   4095 |  629 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
|   1663 |  630 | `		ph7_class *pClass = ReflectResolveClass(pVm, pTarget);` |
|      - |  631 | `		ph7_class_method *pMeth;` |
|   1663 |  632 | `		if( pClass == 0 ){` |
|    ! 0 |  633 | `			return 0;` |
|      - |  634 | `		}` |
|   2492 |  635 | `		pMeth = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|    829 |  636 | `			SyBlobLength(&pMethodArg->sBlob));` |
|   1663 |  637 | `		if( pMeth == 0 ){` |
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
|   1663 |  651 | `		if( pMeth == 0 ){` |
|    ! 0 |  652 | `			return 0;` |
|      - |  653 | `		}` |
|   1663 |  654 | `		if( ppClass ){ *ppClass = pClass; }` |
|   1663 |  655 | `		if( ppMeth ){ *ppMeth = pMeth; }` |
|   1663 |  656 | `		return &pMeth->sFunc;` |
|      - |  657 | `	}` |
|      - |  658 | `	{` |
|   2436 |  659 | `		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|   2436 |  660 | `		if( pClo ){` |
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
|   2259 |  709 | `	if( pTarget->iFlags & MEMOBJ_STRING ){` |
|   2259 |  710 | `		if( SyBlobLength(&pTarget->sBlob) < 1 ){` |
|    ! 0 |  711 | `			return 0;` |
|      - |  712 | `		}` |
|   2259 |  713 | `		pEntry = PH7_VmGetUserFunction(pVm, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob), FALSE);` |
|   2259 |  714 | `		if( pEntry ){` |
|    783 |  715 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|      - |  716 | `		}` |
|   1479 |  717 | `		pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   1479 |  718 | `		if( pEntry && ppHost ){` |
|   1464 |  719 | `			*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|    731 |  720 | `		}` |
|    738 |  721 | `	}` |
|   1479 |  722 | `	return 0;` |
|   2050 |  723 | `}` |
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
|   7134 |  744 | `static void ReflectSigTrim(const char **pz, int *pn)` |
|      3 |  745 | `{` |
|   7137 |  746 | `	const char *z = *pz;` |
|   7137 |  747 | `	int n = *pn;` |
|  14848 |  748 | `	while( n > 0 && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|   4145 |  749 | `		z++;` |
|   4145 |  750 | `		n--;` |
|      1 |  751 | `	}` |
|  11248 |  752 | `	while( n > 0 && (z[n-1] == ' ' \|\| z[n-1] == '\t') ){` |
|    545 |  753 | `		n--;` |
|      1 |  754 | `	}` |
|   7137 |  755 | `	*pz = z;` |
|   7137 |  756 | `	*pn = n;` |
|   7137 |  757 | `}` |
|      - |  758 | ``/* Byte search that respects single-quoted runs (a default may be `'a,b'` or`` |
|      - |  759 | `` * `'it\'s'`), which is the whole reason the split is not SyByteFind. Answers`` |
|      - |  760 | ` * the offset of the first unquoted zWhat, or -1. */` |
|  12564 |  761 | `static int ReflectSigFindUnquoted(const char *z, int n, char cWhat)` |
|      4 |  762 | `{` |
|  12568 |  763 | `	int k, bQuote = 0;` |
| 180500 |  764 | `	for( k = 0 ; k < n ; k++ ){` |
| 175198 |  765 | `		if( bQuote ){` |
|    623 |  766 | `			if( z[k] == '\\' && k + 1 < n ){` |
|     17 |  767 | `				k++;` |
|    615 |  768 | `			}else if( z[k] == '\'' ){` |
|    367 |  769 | `				bQuote = 0;` |
|    184 |  770 | `			}` |
| 174887 |  771 | `		}else if( z[k] == '\'' ){` |
|    367 |  772 | `			bQuote = 1;` |
| 174393 |  773 | `		}else if( z[k] == cWhat ){` |
|   7265 |  774 | `			return k;` |
|      - |  775 | `		}` |
|  83970 |  776 | `	}` |
|   5306 |  777 | `	return -1;` |
|   6286 |  778 | `}` |
|      - |  779 | `/* Does [z,n) contain zNeedle? (case-sensitive; nNeedle > 0) */` |
|   3166 |  780 | `static int ReflectSigHas(const char *z, int n, const char *zNeedle, int nNeedle)` |
|      3 |  781 | `{` |
|      - |  782 | `	int k;` |
|  47575 |  783 | `	for( k = 0 ; k + nNeedle <= n ; k++ ){` |
|  44463 |  784 | `		if( SyMemcmp((const void *)&z[k], (const void *)zNeedle, (sxu32)nNeedle) == 0 ){` |
|     55 |  785 | `			return 1;` |
|      - |  786 | `		}` |
|  22206 |  787 | `	}` |
|   3115 |  788 | `	return 0;` |
|   1586 |  789 | `}` |
|      - |  790 | ``/* Case-insensitive twin, for the `null` arm of a union type text. */`` |
|   1418 |  791 | `static int ReflectSigHasNoCase(const char *z, int n, const char *zNeedle, int nNeedle)` |
|      4 |  792 | `{` |
|      - |  793 | `	int k, j;` |
|   6078 |  794 | `	for( k = 0 ; k + nNeedle <= n ; k++ ){` |
|   4818 |  795 | `		for( j = 0 ; j < nNeedle ; j++ ){` |
|   4814 |  796 | `			if( SyToLower(z[k+j]) != SyToLower(zNeedle[j]) ){` |
|   4660 |  797 | `				break;` |
|      - |  798 | `			}` |
|     79 |  799 | `		}` |
|   4664 |  800 | `		if( j == nNeedle ){` |
|      6 |  801 | `			return 1;` |
|      - |  802 | `		}` |
|   2332 |  803 | `	}` |
|   1418 |  804 | `	return 0;` |
|    713 |  805 | `}` |
|      - |  806 | `/*` |
|      - |  807 | `` * One `C::K` (or `C::class`) term of a declared default, evaluated.`` |
|      - |  808 | ` *` |
|      - |  809 | `` * php's stub writes these as SOURCE — `string $class = SplFileInfo::class`,`` |
|      - |  810 | `` * `int $flags = FilesystemIterator::KEY_AS_PATHNAME\|…` — and the reflector then`` |
|      - |  811 | ` * answers both the text (the export line) and the VALUE (getDefaultValue()). A` |
|      - |  812 | ` * native method's zSig is that same source, so the value has to be read out of` |
|      - |  813 | ` * the class here; there are no compiled parameter records to hold it.` |
|      - |  814 | ` */` |
|      6 |  815 | `static int ReflectSigClassConst(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      1 |  816 | `{` |
|      7 |  817 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  818 | `	ph7_class_attr *pAttr;` |
|      - |  819 | `	ph7_class *pClass;` |
|      - |  820 | `	ph7_value *pValue;` |
|      - |  821 | `	int iSep;` |
|      7 |  822 | `	ReflectSigTrim(&z, &n);` |
|     53 |  823 | `	for( iSep = 0 ; iSep + 1 < n ; ++iSep ){` |
|     53 |  824 | `		if( z[iSep] == ':' && z[iSep+1] == ':' ){` |
|      7 |  825 | `			break;` |
|      - |  826 | `		}` |
|     24 |  827 | `	}` |
|      7 |  828 | `	if( iSep + 1 >= n \|\| iSep < 1 ){` |
|    ! 0 |  829 | `		return 0;` |
|      - |  830 | `	}` |
|      7 |  831 | `	pClass = PH7_VmExtractClass(pVm, z, (sxu32)iSep, TRUE, 0);` |
|      7 |  832 | `	if( pClass == 0 ){` |
|    ! 0 |  833 | `		return 0;` |
|      - |  834 | `	}` |
|      7 |  835 | `	z += iSep + 2;` |
|      7 |  836 | `	n -= iSep + 2;` |
|      7 |  837 | `	if( n == (int)sizeof("class")-1 && SyMemcmp(z, "class", sizeof("class")-1) == 0 ){` |
|      - |  838 | ``		/* `C::class` is the class NAME, and php prints the name it was DECLARED`` |
|      - |  839 | `		 * with rather than the spelling in the signature. */` |
|      3 |  840 | `		SyString *pName = &pClass->sName;` |
|      3 |  841 | `		ph7_value_string(pOut, SyStringData(pName), (int)SyStringLength(pName));` |
|      3 |  842 | `		return 1;` |
|      - |  843 | `	}` |
|      5 |  844 | `	pAttr = PH7_ClassExtractConstant(pClass, z, (sxu32)n);` |
|      5 |  845 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|    ! 0 |  846 | `		return 0;` |
|      - |  847 | `	}` |
|      5 |  848 | `	if( pAttr->nIdx == SXU32_HIGH ){` |
|      - |  849 | `` 		/* Not materialized yet: run the initializer, exactly as a direct `C::K` `` |
|      - |  850 | `		 * read would (an unread native constant is a literal waiting on this). */` |
|      3 |  851 | `		if( VmClassConstEvalOnDemand(pVm, pClass, pAttr) != SXRET_OK ){` |
|    ! 0 |  852 | `			return 0;` |
|      - |  853 | `		}` |
|      1 |  854 | `	}` |
|      5 |  855 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj, pAttr->nIdx);` |
|      5 |  856 | `	if( pValue == 0 ){` |
|    ! 0 |  857 | `		return 0;` |
|      - |  858 | `	}` |
|      5 |  859 | `	PH7_MemObjStore(pValue, pOut);` |
|      5 |  860 | `	return 1;` |
|      4 |  861 | `}` |
|      - |  862 | `/*` |
|      - |  863 | ` * A GLOBAL constant named by a declared default — php's stub writes` |
|      - |  864 | `` * `int $severity = E_ERROR` and both surfaces read it from here: the export`` |
|      - |  865 | ` * prints the NAME (rule 28: the argument was never folded, so php still has the` |
|      - |  866 | ` * source) and getDefaultValue() answers what it expands to. Answers 0 for` |
|      - |  867 | ` * anything that is not a plain identifier naming a defined constant, which is` |
|      - |  868 | `` * what keeps `null`/`true`/a bare number out of this branch.`` |
|      - |  869 | ` */` |
|     34 |  870 | `static int ReflectSigIsIdent(const char *z, int n)` |
|      1 |  871 | `{` |
|      - |  872 | `	int k;` |
|     35 |  873 | `	if( n < 1 \|\| (z[0] != '_' && !SyisAlpha(z[0])) ){` |
|     29 |  874 | `		return 0;` |
|      - |  875 | `	}` |
|     31 |  876 | `	for( k = 1 ; k < n ; ++k ){` |
|     25 |  877 | `		if( z[k] != '_' && z[k] != '\\' && !SyisAlphaNum(z[k]) ){` |
|    ! 0 |  878 | `			return 0;` |
|      - |  879 | `		}` |
|     13 |  880 | `	}` |
|      7 |  881 | `	return 1;` |
|     18 |  882 | `}` |
|     34 |  883 | `static int ReflectSigGlobalConst(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      1 |  884 | `{` |
|      - |  885 | `	SyHashEntry *pEntry;` |
|      - |  886 | `	ph7_constant *pCons;` |
|     35 |  887 | `	ReflectSigTrim(&z, &n);` |
|     35 |  888 | `	if( !ReflectSigIsIdent(z, n) ){` |
|     29 |  889 | `		return 0;` |
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
|     18 |  901 | `}` |
|      - |  902 | `/*` |
|      - |  903 | `` * A class-constant EXPRESSION: one term, or the `\|` fold php's own stubs write`` |
|      - |  904 | `` * for a flags default (`KEY_AS_PATHNAME \| CURRENT_AS_FILEINFO \| SKIP_DOTS`).`` |
|      - |  905 | ` */` |
|     26 |  906 | `static int ReflectSigConstExpr(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      1 |  907 | `{` |
|     27 |  908 | `	sxi64 iAcc = 0;` |
|     27 |  909 | `	int iStart = 0;` |
|     27 |  910 | `	int k, nTerm = 0;` |
|     27 |  911 | `	if( !ReflectSigHas(z, n, "::", 2) ){` |
|     21 |  912 | `		return 0;` |
|      - |  913 | `	}` |
|    119 |  914 | `	for( k = 0 ; k <= n ; ++k ){` |
|    113 |  915 | `		if( k < n && z[k] != '\|' ){` |
|    107 |  916 | `			continue;` |
|      - |  917 | `		}` |
|      7 |  918 | `		if( !ReflectSigClassConst(pCtx, &z[iStart], k - iStart, pOut) ){` |
|    ! 0 |  919 | `			return 0;` |
|      - |  920 | `		}` |
|      7 |  921 | `		nTerm++;` |
|      7 |  922 | `		if( k < n \|\| nTerm > 1 ){` |
|      - |  923 | `			/* A fold is arithmetic, so every arm has to be an integer; a lone` |
|      - |  924 | ``			 * term keeps whatever type it had (`C::class` is a string). */`` |
|    ! 0 |  925 | `			if( (pOut->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) != 0 ){` |
|    ! 0 |  926 | `				return 0;` |
|      - |  927 | `			}` |
|    ! 0 |  928 | `			PH7_MemObjToInteger(pOut);` |
|    ! 0 |  929 | `			iAcc \|= pOut->x.iVal;` |
|    ! 0 |  930 | `		}` |
|      7 |  931 | `		iStart = k + 1;` |
|      4 |  932 | `	}` |
|      7 |  933 | `	if( nTerm > 1 ){` |
|    ! 0 |  934 | `		ph7_value_int64(pOut, iAcc);` |
|    ! 0 |  935 | `	}` |
|      7 |  936 | `	return nTerm > 0;` |
|     14 |  937 | `}` |
|      - |  938 | `/*` |
|      - |  939 | ` * A default-value TEXT to a value, when the text denotes a scalar php can` |
|      - |  940 | `` * reproduce. Answers 1 and fills pOut, or 0 for anything else (`[]`,`` |
|      - |  941 | `` * `array (`, a constant name) which the caller reports its own way.`` |
|      - |  942 | ` */` |
|    112 |  943 | `static int ReflectSigScalar(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|      1 |  944 | `{` |
|    113 |  945 | `	sxu8 bReal = 0;` |
|    113 |  946 | `	if( n == 1 && z[0] == '?' ){` |
|    ! 0 |  947 | `		return 0;` |
|      - |  948 | `	}` |
|    113 |  949 | `	if( (n == 4 && (SyMemcmp(z,"NULL",4) == 0 \|\| SyMemcmp(z,"null",4) == 0)) ){` |
|     53 |  950 | `		ph7_value_null(pOut);` |
|     53 |  951 | `		return 1;` |
|      - |  952 | `	}` |
|     61 |  953 | `	if( n == 4 && SyMemcmp(z,"true",4) == 0 ){` |
|      5 |  954 | `		ph7_value_bool(pOut,1);` |
|      5 |  955 | `		return 1;` |
|      - |  956 | `	}` |
|     57 |  957 | `	if( n == 5 && SyMemcmp(z,"false",5) == 0 ){` |
|      5 |  958 | `		ph7_value_bool(pOut,0);` |
|      5 |  959 | `		return 1;` |
|      - |  960 | `	}` |
|     53 |  961 | `	if( n >= 2 && (z[0] == '\'' \|\| z[0] == '"') && z[n-1] == z[0] ){` |
|      - |  962 | `		/* Unescape the quote character and \\ , the only two escapes the signature` |
|      - |  963 | `		 * writer emits. BOTH quote spellings are accepted because both scanners in` |
|      - |  964 | `		 * vm_arg_check.c step over either one: a native method's zSig lives in C, so` |
|      - |  965 | ``		 * `= \"static\"` is the natural way to write Closure::bindTo's default and it`` |
|      - |  966 | ``		 * used to reduce to php's `<default>` placeholder here. */`` |
|      - |  967 | `		SyBlob sOut;` |
|     27 |  968 | `		char cQuote = z[0];` |
|      - |  969 | `		int k;` |
|     27 |  970 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|     51 |  971 | `		for( k = 1 ; k < n - 1 ; k++ ){` |
|     25 |  972 | `			if( z[k] == '\\' && k + 1 < n - 1 && (z[k+1] == cQuote \|\| z[k+1] == '\\') ){` |
|      3 |  973 | `				k++;` |
|      1 |  974 | `			}` |
|     25 |  975 | `			SyBlobAppend(&sOut,(const void *)&z[k],sizeof(char));` |
|     13 |  976 | `		}` |
|     27 |  977 | `		ph7_value_string(pOut,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|     27 |  978 | `		SyBlobRelease(&sOut);` |
|     27 |  979 | `		return 1;` |
|      - |  980 | `	}` |
|     27 |  981 | `	if( ReflectSigConstExpr(pCtx,z,n,pOut) ){` |
|      7 |  982 | `		return 1;` |
|      - |  983 | `	}` |
|     21 |  984 | `	if( ReflectSigGlobalConst(pCtx,z,n,pOut) ){` |
|      3 |  985 | `		return 1;` |
|      - |  986 | `	}` |
|     19 |  987 | `	if( n > 0 && SyStrIsNumeric(z,(sxu32)n,&bReal,0) == SXRET_OK ){` |
|      - |  988 | `		/* php's own rule for the text form: a '.', an exponent or a hex marker` |
|      - |  989 | `		 * makes it a float, everything else an int. */` |
|     16 |  990 | `		if( bReal \|\| ReflectSigHas(z,n,".",1)` |
|     17 |  991 | `		 \|\| ReflectSigHasNoCase(z,n,"e",1) \|\| ReflectSigHasNoCase(z,n,"x",1) ){` |
|      - |  992 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|    ! 0 |  993 | `			ph7_value_double(pOut,SyStrToReal(z,(sxu32)n,0,0));` |
|      - |  994 | `#else` |
|      - |  995 | `			ph7_value_int64(pOut,SyStrToInt64(z,(sxu32)n,0,0));` |
|      - |  996 | `#endif` |
|    ! 0 |  997 | `		}else{` |
|     17 |  998 | `			sxi64 iVal = 0;` |
|     17 |  999 | `			SyStrToInt64(z,(sxu32)n,(void *)&iVal,0);` |
|     17 | 1000 | `			ph7_value_int64(pOut,iVal);` |
|      - | 1001 | `		}` |
|     17 | 1002 | `		return 1;` |
|      - | 1003 | `	}` |
|      3 | 1004 | `	return 0;` |
|     57 | 1005 | `}` |
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
|   2500 | 1032 | `static int ReflectSigPart(const char *zSig, int nSig, int iWant,` |
|      - | 1033 | `	const char **pzPart, int *pnPart)` |
|      3 | 1034 | `{` |
|   2503 | 1035 | `	int iPos = 0;` |
|   6103 | 1036 | `	while( nSig > 0 ){` |
|   6009 | 1037 | `		int iComma = ReflectSigFindUnquoted(zSig,nSig,',');` |
|   6009 | 1038 | `		const char *zPart = zSig;` |
|   6009 | 1039 | `		int nPart = iComma < 0 ? nSig : iComma;` |
|   6009 | 1040 | `		ReflectSigTrim(&zPart,&nPart);` |
|   6009 | 1041 | `		if( nPart > 0 ){` |
|   6009 | 1042 | `			if( iPos == iWant && pzPart ){` |
|   1557 | 1043 | `				*pzPart = zPart;` |
|   1557 | 1044 | `				*pnPart = nPart;` |
|    777 | 1045 | `			}` |
|   6009 | 1046 | `			iPos++;` |
|   3003 | 1047 | `		}` |
|   6009 | 1048 | `		if( iComma < 0 ){` |
|   2409 | 1049 | `			break;` |
|      - | 1050 | `		}` |
|   3601 | 1051 | `		zSig += iComma + 1;` |
|   3601 | 1052 | `		nSig -= iComma + 1;` |
|      1 | 1053 | `	}` |
|   2503 | 1054 | `	return iPos;` |
|      3 | 1055 | `}` |
|      - | 1056 | ``/* One `type &$name = default` part into the uniform description. */`` |
|   1554 | 1057 | `static void ReflectSigDescribe(const char *z, int n, int iPos, ReflectParamDesc *pOut)` |
|      3 | 1058 | `{` |
|   1557 | 1059 | `	const char *zDef = 0;` |
|   1557 | 1060 | `	int nDef = 0;` |
|      - | 1061 | `	int iEq, iDollar, iSpace;` |
|   1557 | 1062 | `	SyZero(pOut,sizeof(*pOut));` |
|   1557 | 1063 | `	pOut->iPos = iPos;` |
|      - | 1064 | ``	/* `= default` splits off first: everything after the first unquoted '='. */`` |
|   1557 | 1065 | `	iEq = ReflectSigFindUnquoted(z,n,'=');` |
|   1557 | 1066 | `	if( iEq >= 0 ){` |
|    545 | 1067 | `		zDef = &z[iEq+1];` |
|    545 | 1068 | `		nDef = n - iEq - 1;` |
|    545 | 1069 | `		ReflectSigTrim(&zDef,&nDef);` |
|    545 | 1070 | `		n = iEq;` |
|    545 | 1071 | `		ReflectSigTrim(&z,&n);` |
|    272 | 1072 | `	}` |
|   1557 | 1073 | `	if( zDef && nDef == 1 && zDef[0] == '?' ){` |
|      - | 1074 | ``		/* `= ?` is the table's OPTIONAL-but-no-default marker, php's own shape`` |
|      - | 1075 | `		 * for a parameter like ReflectionClass::getStaticPropertyValue()'s` |
|      - | 1076 | `		 * $default: isOptional() true, isDefaultValueAvailable() FALSE, so` |
|      - | 1077 | `		 * getDefaultValue() raises. Reporting it as a default (which is what` |
|      - | 1078 | ``		 * a bare `hasdef` did) made that call answer NULL instead. */`` |
|     43 | 1079 | `		pOut->bOptional = 1;` |
|     43 | 1080 | `		zDef = 0;` |
|     43 | 1081 | `		nDef = 0;` |
|     21 | 1082 | `	}` |
|   1557 | 1083 | `	pOut->bVariadic = ReflectSigHas(z,n,"...",3);` |
|   1557 | 1084 | `	if( pOut->bVariadic ){` |
|      - | 1085 | `		/* php: a variadic parameter never HAS a default -- it defaults to "no` |
|      - | 1086 | `		 * further arguments", which is not a value. Several signature rows still` |
|      - | 1087 | ``		 * write `mixed ...$values = ?` (the table's "optional, unspecified"`` |
|      - | 1088 | `		 * marker), and honouring it made isDefaultValueAvailable() true where php` |
|      - | 1089 | `		 * says false, so getDefaultValue() then threw on printf/array_merge. */` |
|     33 | 1090 | `		zDef = 0;` |
|     33 | 1091 | `		nDef = 0;` |
|     16 | 1092 | `	}` |
|   1557 | 1093 | `	iDollar = ReflectSigFindUnquoted(z,n,'$');` |
|   1557 | 1094 | `	iSpace = ReflectSigFindUnquoted(z,n,' ');` |
|      - | 1095 | `	{` |
|   1557 | 1096 | `		const char *zName = iDollar < 0 ? z : &z[iDollar+1];` |
|   1557 | 1097 | `		int nName = iDollar < 0 ? n : n - iDollar - 1;` |
|   1557 | 1098 | `		SyStringInitFromBuf(&pOut->sName,zName,nName);` |
|      - | 1099 | `	}` |
|   1557 | 1100 | `	pOut->bByRef = ReflectSigHas(z,n,"&",1);` |
|   1557 | 1101 | `	pOut->bHasDef = zDef != 0;` |
|   1557 | 1102 | `	pOut->bOptional = pOut->bOptional \|\| pOut->bVariadic \|\| zDef != 0;` |
|   1557 | 1103 | `	if( iSpace >= 0 && iDollar >= 0 && iSpace < iDollar ){` |
|      - | 1104 | `` 		/* The type is whatever precedes the first space, so `?DOMNode $child` `` |
|      - | 1105 | ``		 * types as `?DOMNode` and an untyped `$x` types as nothing. */`` |
|   1543 | 1106 | `		pOut->bNullable = (z[0] == '?' \|\| ReflectSigHasNoCase(z,iSpace,"null",4));` |
|   1543 | 1107 | `		SyStringInitFromBuf(&pOut->sType,z,iSpace);` |
|    770 | 1108 | `	}` |
|   1557 | 1109 | `	if( zDef ){` |
|    503 | 1110 | `		SyStringInitFromBuf(&pOut->sDefText,zDef,nDef);` |
|    251 | 1111 | `	}` |
|   1557 | 1112 | `}` |
|      - | 1113 | `/*` |
|      - | 1114 | ` * Resolve a Generator object into its wrapper. Mirrors the static` |
|      - | 1115 | ` * VmGeneratorExtractCtx in vm.c: the $__ctx attribute carries the` |
|      - | 1116 | ` * ph7_generator pointer as a resource value.` |
|      - | 1117 | ` */` |
|     42 | 1118 | `static ph7_generator * ReflectGeneratorCtx(ph7_vm *pVm, ph7_value *pVal)` |
|      1 | 1119 | `{` |
|      - | 1120 | `	ph7_class_instance *pThis;` |
|      - | 1121 | `	ph7_value *pAttr;` |
|      - | 1122 | `	SyString sAttr;` |
|     43 | 1123 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVm->pGeneratorClass == 0 ){` |
|    ! 0 | 1124 | `		return 0;` |
|      - | 1125 | `	}` |
|     43 | 1126 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     43 | 1127 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|    ! 0 | 1128 | `		return 0;` |
|      - | 1129 | `	}` |
|     43 | 1130 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|     43 | 1131 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     43 | 1132 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|    ! 0 | 1133 | `		return 0;` |
|      - | 1134 | `	}` |
|     43 | 1135 | `	return (ph7_generator *)pAttr->x.pOther;` |
|     22 | 1136 | `}` |
|      - | 1137 | `/*` |
|      - | 1138 | ` * Evaluate the recorded argument expressions of one declared attribute and` |
|      - | 1139 | ` * answer them as a PHP array, or NULL when the target no longer resolves.` |
|      - | 1140 | ` *` |
|      - | 1141 | ` * apArg is the four-part SPEC a ReflectionAttribute carries — kind 'class'` |
|      - | 1142 | ` * (target = class), 'attr' (class + property/constant name), 'method' (class +` |
|      - | 1143 | ` * method), 'fn' (function name or Closure), 'param' (function spec + parameter` |
|      - | 1144 | ` * index), 'const' (global constant name) — plus which attribute of that target.` |
|      - | 1145 | ` * Named arguments become string keys.` |
|      - | 1146 | ` *` |
|      - | 1147 | ` * The values are evaluated HERE rather than when the reflector was built:` |
|      - | 1148 | `` * `#[Attr(self::SOME)]` runs php code, and php runs it at getArguments() time.`` |
|      - | 1149 | ` */` |
|    112 | 1150 | `static ph7_value * ReflectAttrArgs(ph7_context *pCtx, ph7_value **apArg, sxu32 nAttrIdx)` |
|      1 | 1151 | `{` |
|    113 | 1152 | `	ph7_vm *pVm = pCtx->pVm;` |
|    113 | 1153 | `	SySet *pAttrs = 0;` |
|    113 | 1154 | `	ph7_class *pDeclCls = 0; /* class an attribute is declared on: scope for self:: in its args */` |
|      - | 1155 | `	ph7_attribute *pAttrRec;` |
|      - | 1156 | `	ph7_value *pOut;` |
|      - | 1157 | `	const char *zKind;` |
|      - | 1158 | `	int nKind;` |
|      - | 1159 | `	sxu32 n;` |
|    113 | 1160 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|    156 | 1161 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|     87 | 1162 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|     87 | 1163 | `		if( pClass ){ pAttrs = &pClass->aAttrs; pDeclCls = pClass; }` |
|     72 | 1164 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|      5 | 1165 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|      5 | 1166 | `		ph7_class_attr *pMember = pClass ? ReflectFetchMember(pClass, apArg[2]) : 0;` |
|      5 | 1167 | `		if( pMember ){ pAttrs = &pMember->aAttrs; pDeclCls = pClass; }` |
|     30 | 1168 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|     11 | 1169 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     11 | 1170 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|     22 | 1171 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|      9 | 1172 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], 0, 0, 0, 0, 0);` |
|      9 | 1173 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|     10 | 1174 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|      3 | 1175 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], apArg[2], 0, 0, 0, 0);` |
|      3 | 1176 | `		ph7_vm_func_arg *pParam = pFunc` |
|      2 | 1177 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|      3 | 1178 | `		if( pParam ){ pAttrs = &pParam->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|      4 | 1179 | `	}else if( nKind == 5 && SyMemcmp(zKind, "const", 5) == 0 ){` |
|      - | 1180 | ``		/* Global constant (php 8.5 attributes on `const` statements) */`` |
|      - | 1181 | `		const char *zCName;` |
|      - | 1182 | `		int nCName;` |
|      - | 1183 | `		SyHashEntry *pCEntry;` |
|      3 | 1184 | `		zCName = ph7_value_to_string(apArg[1], &nCName);` |
|      3 | 1185 | `		pCEntry = nCName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zCName, (sxu32)nCName) : 0;` |
|      3 | 1186 | `		if( pCEntry ){ pAttrs = &((ph7_constant *)pCEntry->pUserData)->aAttrs; }` |
|      1 | 1187 | `	}` |
|    112 | 1188 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|    113 | 1189 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|    ! 0 | 1190 | `		return 0;` |
|      - | 1191 | `	}` |
|    267 | 1192 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|    155 | 1193 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|      - | 1194 | `		ph7_value sValue;` |
|    155 | 1195 | `		PH7_MemObjInit(pVm, &sValue);` |
|    155 | 1196 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|      - | 1197 | `` 			/* Evaluate under the attribute's declaring-class scope so `self::class` `` |
|      - | 1198 | ``			 * / `self::CONST` / `parent::` resolve like php (else self:: would bind`` |
|      - | 1199 | `			 * to the reflection machinery's own class). */` |
|    141 | 1200 | `			PH7_VmExecAttrArg(pVm, &pArgRec->aByteCode, pDeclCls, &sValue);` |
|     85 | 1201 | `		}else if( pArgRec->pNativeValue ){` |
|      - | 1202 | ``			/* A NATIVE attribute (php's own `#[Attribute(...)]` on Attribute and`` |
|      - | 1203 | `			 * Deprecated): the argument is a literal, not byte-code. */` |
|     15 | 1204 | `			PH7_NativeLiteralValue(pVm, pArgRec->pNativeValue, &sValue);` |
|      7 | 1205 | `		}` |
|    155 | 1206 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|     13 | 1207 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|      7 | 1208 | `		}else{` |
|    143 | 1209 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|      - | 1210 | `		}` |
|    155 | 1211 | `		PH7_MemObjRelease(&sValue);` |
|     78 | 1212 | `	}` |
|    113 | 1213 | `	return pOut;` |
|     57 | 1214 | `}` |
|      - | 1215 | `/*` |
|      - | 1216 | ` * ---------------------------------------------------------------------------` |
|      - | 1217 | ` * The ReflectionType family.` |
|      - | 1218 | ` *` |
|      - | 1219 | ` * php's four type objects are pure VALUES: a text, a nullability flag, and for` |
|      - | 1220 | ` * the composites a list of members. Nothing in userland can build one — php` |
|      - | 1221 | `` * declares no constructor on any of them and refuses `clone` — so the prelude's`` |
|      - | 1222 | `` * public `__construct($name, $nullable, $text)` existed only because the factory`` |
|      - | 1223 | ` * that fills them was itself PHP. The factory is C now (ReflectMakeType), so the` |
|      - | 1224 | ` * constructors are gone and the classes say what php's say.` |
|      - | 1225 | ` * ---------------------------------------------------------------------------` |
|      - | 1226 | ` */` |
|      - | 1227 | `#define RT_TEXT     "__text"` |
|      - | 1228 | `#define RT_NULLABLE "__nullable"` |
|      - | 1229 | `#define RT_TNAME    "__tname"` |
|      - | 1230 | `#define RT_TYPES    "__types"` |
|      - | 1231 |  |
|     26 | 1232 | `static int vm_builtin_ReflectionType_allowsNull(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 1233 | `{` |
|     28 | 1234 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     13 | 1235 | `	SXUNUSED(nArg);` |
|     13 | 1236 | `	SXUNUSED(apArg);` |
|     28 | 1237 | `	ph7_result_bool(pCtx, pThis != 0 && PH7_NativeAttrTruthy(pThis, RT_NULLABLE));` |
|     28 | 1238 | `	return PH7_OK;` |
|      2 | 1239 | `}` |
|    848 | 1240 | `static int vm_builtin_ReflectionType_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 | 1241 | `{` |
|    852 | 1242 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    852 | 1243 | `	const char *zText = "";` |
|    852 | 1244 | `	int nText = 0;` |
|    424 | 1245 | `	SXUNUSED(nArg);` |
|    424 | 1246 | `	SXUNUSED(apArg);` |
|    852 | 1247 | `	if( pThis ){` |
|    852 | 1248 | `		PH7_NativeAttrStr(pThis, RT_TEXT, &zText, &nText);` |
|    424 | 1249 | `	}` |
|    852 | 1250 | `	ph7_result_string(pCtx, zText, nText);` |
|    852 | 1251 | `	return PH7_OK;` |
|      4 | 1252 | `}` |
|     20 | 1253 | `static int vm_builtin_ReflectionNamedType_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 1254 | `{` |
|     23 | 1255 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     23 | 1256 | `	const char *zName = "";` |
|     23 | 1257 | `	int nName = 0;` |
|     10 | 1258 | `	SXUNUSED(nArg);` |
|     10 | 1259 | `	SXUNUSED(apArg);` |
|     23 | 1260 | `	if( pThis ){` |
|     23 | 1261 | `		PH7_NativeAttrStr(pThis, RT_TNAME, &zName, &nName);` |
|     10 | 1262 | `	}` |
|     23 | 1263 | `	ph7_result_string(pCtx, zName, nName);` |
|     23 | 1264 | `	return PH7_OK;` |
|      3 | 1265 | `}` |
|      - | 1266 | `/* php's builtin-type set, case-insensitively. Anything else is a class name. */` |
|     32 | 1267 | `static int ReflectTypeIsBuiltin(const char *zName, int nName)` |
|      1 | 1268 | `{` |
|      - | 1269 | `	static const char *azBuiltin[] = {` |
|      - | 1270 | `		"int","float","string","bool","array","object","mixed",` |
|      - | 1271 | `		"void","never","null","callable","iterable","true","false"` |
|      - | 1272 | `	};` |
|      - | 1273 | `	sxu32 n;` |
|    245 | 1274 | `	for( n = 0 ; n < SX_ARRAYSIZE(azBuiltin) ; n++ ){` |
|    235 | 1275 | `		int nWant = (int)SyStrlen(azBuiltin[n]);` |
|      - | 1276 | `		int k;` |
|    235 | 1277 | `		if( nWant != nName ){` |
|    199 | 1278 | `			continue;` |
|      - | 1279 | `		}` |
|    147 | 1280 | `		for( k = 0 ; k < nWant ; k++ ){` |
|    125 | 1281 | `			if( SyToLower(zName[k]) != azBuiltin[n][k] ){` |
|     15 | 1282 | `				break;` |
|      - | 1283 | `			}` |
|     56 | 1284 | `		}` |
|     37 | 1285 | `		if( k == nWant ){` |
|     23 | 1286 | `			return 1;` |
|      - | 1287 | `		}` |
|      8 | 1288 | `	}` |
|     11 | 1289 | `	return 0;` |
|     17 | 1290 | `}` |
|     24 | 1291 | `static int vm_builtin_ReflectionNamedType_isBuiltin(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1292 | `{` |
|     25 | 1293 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     25 | 1294 | `	const char *zName = "";` |
|     25 | 1295 | `	int nName = 0;` |
|     12 | 1296 | `	SXUNUSED(nArg);` |
|     12 | 1297 | `	SXUNUSED(apArg);` |
|     25 | 1298 | `	if( pThis ){` |
|     25 | 1299 | `		PH7_NativeAttrStr(pThis, RT_TNAME, &zName, &nName);` |
|     12 | 1300 | `	}` |
|     25 | 1301 | `	ph7_result_bool(pCtx, ReflectTypeIsBuiltin(zName, nName));` |
|     25 | 1302 | `	return PH7_OK;` |
|      1 | 1303 | `}` |
|      6 | 1304 | `static int vm_builtin_ReflectionType_getTypes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1305 | `{` |
|      7 | 1306 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      7 | 1307 | `	ph7_value *pTypes = pThis ? PH7_NativeAttr(pThis, RT_TYPES) : 0;` |
|      3 | 1308 | `	SXUNUSED(nArg);` |
|      3 | 1309 | `	SXUNUSED(apArg);` |
|      7 | 1310 | `	if( pTypes && (pTypes->iFlags & MEMOBJ_HASHMAP) ){` |
|      7 | 1311 | `		ph7_result_value(pCtx, pTypes);` |
|      4 | 1312 | `	}else{` |
|      - | 1313 | `		/* The declared default is a native NULL slot (a spec cannot carry an` |
|      - | 1314 | `		 * array literal), but php's getTypes() always answers a list. */` |
|    ! 0 | 1315 | `		ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|    ! 0 | 1316 | `		if( pEmpty ){` |
|    ! 0 | 1317 | `			ph7_result_value(pCtx, pEmpty);` |
|    ! 0 | 1318 | `		}` |
|      - | 1319 | `	}` |
|      7 | 1320 | `	return PH7_OK;` |
|      1 | 1321 | `}` |
|      - | 1322 | `/* Build one of the three concrete types with its slots filled. The caller owns` |
|      - | 1323 | ` * the reference (PH7_NativeResultObject / a list insert drops it). */` |
|   1196 | 1324 | `static ph7_class_instance * ReflectNewType(ph7_context *pCtx, const char *zClass,` |
|      - | 1325 | `	const char *zText, int nText, int bNullable)` |
|      4 | 1326 | `{` |
|   1200 | 1327 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1200 | 1328 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|   1200 | 1329 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|   1200 | 1330 | `	if( pObj == 0 ){` |
|    ! 0 | 1331 | `		return 0;` |
|      - | 1332 | `	}` |
|   1200 | 1333 | `	PH7_NativeSetAttrStr(pVm, pObj, RT_TEXT, zText, nText);` |
|   1200 | 1334 | `	PH7_NativeSetAttrBool(pVm, pObj, RT_NULLABLE, bNullable);` |
|   1200 | 1335 | `	return pObj;` |
|    602 | 1336 | `}` |
|      - | 1337 | `/*` |
|      - | 1338 | ` * Append pType to a composite's member list, handing over the caller's reference.` |
|      - | 1339 | ` *` |
|      - | 1340 | ` * The carrier is a STACK value, deliberately: a ph7_context_new_scalar() is` |
|      - | 1341 | ` * released with the call context, and releasing a MEMOBJ_OBJ carrier unrefs the` |
|      - | 1342 | ` * instance a second time — which freed every member of a union or intersection` |
|      - | 1343 | ` * the moment its factory returned, so the list came back holding dead objects.` |
|      - | 1344 | ` * PH7_NativeResultObject hands an instance over the same way.` |
|      - | 1345 | ` */` |
|    410 | 1346 | `static void ReflectTypeListAdd(ph7_context *pCtx, ph7_value *pList, ph7_class_instance *pType)` |
|      3 | 1347 | `{` |
|      - | 1348 | `	ph7_value sVal;` |
|    413 | 1349 | `	if( pType == 0 ){` |
|    ! 0 | 1350 | `		return;` |
|      - | 1351 | `	}` |
|    413 | 1352 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    413 | 1353 | `	sVal.x.pOther = pType;` |
|    413 | 1354 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|    413 | 1355 | `	ph7_array_add_elem(pList, 0, &sVal);   /* takes its own reference */` |
|    413 | 1356 | `	PH7_ClassInstanceUnref(pType);` |
|    208 | 1357 | `}` |
|      - | 1358 | `/* Exact, case-insensitive name test (php lower-cases before comparing). */` |
|   1894 | 1359 | `static int ReflectTypeNameIs(const char *z, int n, const char *zWant)` |
|      4 | 1360 | `{` |
|   1898 | 1361 | `	int nWant = (int)SyStrlen(zWant), k;` |
|   1898 | 1362 | `	if( n != nWant ){` |
|   1598 | 1363 | `		return 0;` |
|      - | 1364 | `	}` |
|    528 | 1365 | `	for( k = 0 ; k < n ; k++ ){` |
|    484 | 1366 | `		if( SyToLower(z[k]) != zWant[k] ){` |
|    260 | 1367 | `			return 0;` |
|      - | 1368 | `		}` |
|    114 | 1369 | `	}` |
|     46 | 1370 | `	return 1;` |
|    951 | 1371 | `}` |
|      - | 1372 | `/*` |
|      - | 1373 | ` * A ReflectionNamedType for one name.` |
|      - | 1374 | ` *` |
|      - | 1375 | ` * bQMark says the text carried a leading '?', which is the ONLY thing that puts` |
|      - | 1376 | `` * one back on the rendered text — while `null` and `mixed` are nullable by their`` |
|      - | 1377 | ` * own meaning without ever rendering a '?'. Those two rules are independent, and` |
|      - | 1378 | `` * conflating them is how `?array` came out as `array`.`` |
|      - | 1379 | ` */` |
|   1074 | 1380 | `static ph7_class_instance * ReflectNewNamed(ph7_context *pCtx, const char *z, int n, int bQMark)` |
|      4 | 1381 | `{` |
|      - | 1382 | `	ph7_class_instance *pObj;` |
|      - | 1383 | `	char zBuf[256];` |
|   1078 | 1384 | `	const char *zText = z;` |
|   1078 | 1385 | `	int nText = n;` |
|   1078 | 1386 | `	int bNullable = bQMark \|\| ReflectTypeNameIs(z, n, "null") \|\| ReflectTypeNameIs(z, n, "mixed");` |
|   1078 | 1387 | `	if( bQMark && n + 1 < (int)sizeof(zBuf) ){` |
|    143 | 1388 | `		zBuf[0] = '?';` |
|    143 | 1389 | `		SyMemcpy(z, &zBuf[1], (sxu32)n);` |
|    143 | 1390 | `		zText = zBuf;` |
|    143 | 1391 | `		nText = n + 1;` |
|     70 | 1392 | `	}` |
|   1078 | 1393 | `	pObj = ReflectNewType(pCtx, "ReflectionNamedType", zText, nText, bNullable);` |
|   1078 | 1394 | `	if( pObj ){` |
|   1078 | 1395 | `		PH7_NativeSetAttrStr(pCtx->pVm, pObj, RT_TNAME, z, n);` |
|    537 | 1396 | `	}` |
|   1078 | 1397 | `	return pObj;` |
|      4 | 1398 | `}` |
|      - | 1399 | `/*` |
|      - | 1400 | `` * One ATOM of a type text: `?X`, `(A&B)` or a plain name. An intersection is`` |
|      - | 1401 | ` * the only composite an atom can be, because php only nests that way.` |
|      - | 1402 | ` */` |
|   1064 | 1403 | `static ph7_class_instance * ReflectMakeAtom(ph7_context *pCtx, const char *z, int n)` |
|      4 | 1404 | `{` |
|   1068 | 1405 | `	int bQMark = 0;` |
|   1068 | 1406 | `	if( n > 0 && z[0] == '?' ){` |
|    143 | 1407 | `		bQMark = 1;` |
|    143 | 1408 | `		z++;` |
|    143 | 1409 | `		n--;` |
|     70 | 1410 | `	}` |
|   1068 | 1411 | `	if( n > 1 && z[0] == '(' && z[n-1] == ')' ){` |
|      8 | 1412 | `		z++;` |
|      8 | 1413 | `		n -= 2;` |
|      3 | 1414 | `	}` |
|   1068 | 1415 | `	if( ReflectSigFindUnquoted(z, n, '&') >= 0 ){` |
|     12 | 1416 | `		ph7_class_instance *pObj = ReflectNewType(pCtx, "ReflectionIntersectionType", z, n, 0);` |
|     12 | 1417 | `		ph7_value *pList = ph7_context_new_array(pCtx);` |
|     12 | 1418 | `		const char *zCur = z;` |
|     12 | 1419 | `		int nCur = n;` |
|     12 | 1420 | `		if( pObj == 0 \|\| pList == 0 ){` |
|    ! 0 | 1421 | `			return pObj;` |
|      - | 1422 | `		}` |
|     22 | 1423 | `		while( nCur > 0 ){` |
|     22 | 1424 | `			int iCut = ReflectSigFindUnquoted(zCur, nCur, '&');` |
|     32 | 1425 | `			ReflectTypeListAdd(pCtx, pList,` |
|     10 | 1426 | `				ReflectNewNamed(pCtx, zCur, iCut < 0 ? nCur : iCut, 0));` |
|     22 | 1427 | `			if( iCut < 0 ){` |
|     12 | 1428 | `				break;` |
|      - | 1429 | `			}` |
|     12 | 1430 | `			zCur += iCut + 1;` |
|     12 | 1431 | `			nCur -= iCut + 1;` |
|      2 | 1432 | `		}` |
|     12 | 1433 | `		PH7_NativeSetProp(pCtx->pVm, pObj, RT_TYPES, sizeof(RT_TYPES)-1, pList);` |
|     12 | 1434 | `		return pObj;` |
|      - | 1435 | `	}` |
|   1058 | 1436 | `	return ReflectNewNamed(pCtx, z, n, bQMark);` |
|    536 | 1437 | `}` |
|      - | 1438 | `/*` |
|      - | 1439 | ` * A declared type TEXT to the object php answers for it: a named type, a union,` |
|      - | 1440 | `` * or an intersection. NULL for an absent type. `X\|null` collapses back to a`` |
|      - | 1441 | `` * NULLABLE named type, which is what php reports (`?X`), but only when exactly`` |
|      - | 1442 | ` * one non-null arm is left and it is not itself an intersection.` |
|      - | 1443 | ` */` |
|    924 | 1444 | `static ph7_class_instance * ReflectMakeType(ph7_context *pCtx, const char *zText, int nText)` |
|      4 | 1445 | `{` |
|    928 | 1446 | `	const char *zBody = zText;` |
|    928 | 1447 | `	int nBody = nText;` |
|    928 | 1448 | `	int bNullable = 0, bHasNull = 0, nParts = 0, nNonNull = 0;` |
|    928 | 1449 | `	const char *zLastNonNull = 0;` |
|    928 | 1450 | `	int nLastNonNull = 0;` |
|      - | 1451 | `	const char *zCur;` |
|      - | 1452 | `	int nCur, iDepth, k, iStart;` |
|    928 | 1453 | `	if( nText < 1 ){` |
|    ! 0 | 1454 | `		return 0;` |
|      - | 1455 | `	}` |
|    928 | 1456 | `	if( zBody[0] == '?' ){` |
|    143 | 1457 | `		bNullable = 1;` |
|    143 | 1458 | `		zBody++;` |
|    143 | 1459 | `		nBody--;` |
|     70 | 1460 | `	}` |
|      - | 1461 | ``	/* Split on the TOP-LEVEL '\|' only: `A\|(B&C)` has one at depth 0 and none`` |
|      - | 1462 | `	 * inside the parentheses. */` |
|    928 | 1463 | `	iDepth = 0;` |
|    928 | 1464 | `	iStart = 0;` |
|   8204 | 1465 | `	for( k = 0 ; k <= nBody ; k++ ){` |
|   7280 | 1466 | `		if( k < nBody && zBody[k] == '(' ){` |
|      8 | 1467 | `			iDepth++;` |
|      8 | 1468 | `			continue;` |
|      - | 1469 | `		}` |
|   7274 | 1470 | `		if( k < nBody && zBody[k] == ')' ){` |
|      8 | 1471 | `			iDepth--;` |
|      8 | 1472 | `			continue;` |
|      - | 1473 | `		}` |
|   7268 | 1474 | `		if( k == nBody \|\| (zBody[k] == '\|' && iDepth == 0) ){` |
|   1068 | 1475 | `			zCur = &zBody[iStart];` |
|   1068 | 1476 | `			nCur = k - iStart;` |
|   1068 | 1477 | `			nParts++;` |
|   1068 | 1478 | `			if( nCur == 4 && ReflectSigHasNoCase(zCur, 4, "null", 4) ){` |
|      3 | 1479 | `				bHasNull = 1;` |
|      2 | 1480 | `			}else{` |
|   1066 | 1481 | `				nNonNull++;` |
|   1066 | 1482 | `				zLastNonNull = zCur;` |
|   1066 | 1483 | `				nLastNonNull = nCur;` |
|      - | 1484 | `			}` |
|   1068 | 1485 | `			iStart = k + 1;` |
|    532 | 1486 | `		}` |
|   3636 | 1487 | `	}` |
|    928 | 1488 | `	if( nParts > 1 ){` |
|      - | 1489 | `		ph7_class_instance *pObj;` |
|      - | 1490 | `		ph7_value *pList;` |
|    112 | 1491 | `		if( bHasNull && nNonNull == 1` |
|      4 | 1492 | `		 && ReflectSigFindUnquoted(zLastNonNull, nLastNonNull, '&') < 0 ){` |
|      - | 1493 | ``			/* `X\|null` IS `?X` to php. */`` |
|      - | 1494 | `			char zBuf[256];` |
|      - | 1495 | `			ph7_class_instance *pNamed;` |
|    ! 0 | 1496 | `			const char *zRender = zLastNonNull;` |
|    ! 0 | 1497 | `			int nRender = nLastNonNull;` |
|    ! 0 | 1498 | `			if( nLastNonNull + 1 < (int)sizeof(zBuf) ){` |
|    ! 0 | 1499 | `				zBuf[0] = '?';` |
|    ! 0 | 1500 | `				SyMemcpy(zLastNonNull, &zBuf[1], (sxu32)nLastNonNull);` |
|    ! 0 | 1501 | `				zRender = zBuf;` |
|    ! 0 | 1502 | `				nRender = nLastNonNull + 1;` |
|    ! 0 | 1503 | `			}` |
|    ! 0 | 1504 | `			pNamed = ReflectNewType(pCtx, "ReflectionNamedType", zRender, nRender, 1);` |
|    ! 0 | 1505 | `			if( pNamed ){` |
|    ! 0 | 1506 | `				PH7_NativeSetAttrStr(pCtx->pVm, pNamed, RT_TNAME, zLastNonNull, nLastNonNull);` |
|    ! 0 | 1507 | `			}` |
|    ! 0 | 1508 | `			return pNamed;` |
|      - | 1509 | `		}` |
|    115 | 1510 | `		pObj = ReflectNewType(pCtx, "ReflectionUnionType", zBody, nBody, bNullable \|\| bHasNull);` |
|    115 | 1511 | `		pList = ph7_context_new_array(pCtx);` |
|    115 | 1512 | `		if( pObj == 0 \|\| pList == 0 ){` |
|    ! 0 | 1513 | `			return pObj;` |
|      - | 1514 | `		}` |
|    115 | 1515 | `		iDepth = 0;` |
|    115 | 1516 | `		iStart = 0;` |
|   1733 | 1517 | `		for( k = 0 ; k <= nBody ; k++ ){` |
|   1621 | 1518 | `			if( k < nBody && zBody[k] == '(' ){` |
|      8 | 1519 | `				iDepth++;` |
|      8 | 1520 | `				continue;` |
|      - | 1521 | `			}` |
|   1615 | 1522 | `			if( k < nBody && zBody[k] == ')' ){` |
|      8 | 1523 | `				iDepth--;` |
|      8 | 1524 | `				continue;` |
|      - | 1525 | `			}` |
|   1609 | 1526 | `			if( k == nBody \|\| (zBody[k] == '\|' && iDepth == 0) ){` |
|    381 | 1527 | `				ReflectTypeListAdd(pCtx, pList,` |
|    126 | 1528 | `					ReflectMakeAtom(pCtx, &zBody[iStart], k - iStart));` |
|    255 | 1529 | `				iStart = k + 1;` |
|    126 | 1530 | `			}` |
|    806 | 1531 | `		}` |
|    115 | 1532 | `		PH7_NativeSetProp(pCtx->pVm, pObj, RT_TYPES, sizeof(RT_TYPES)-1, pList);` |
|    115 | 1533 | `		return pObj;` |
|      - | 1534 | `	}` |
|    816 | 1535 | `	if( ReflectSigFindUnquoted(zBody, nBody, '&') >= 0 ){` |
|      5 | 1536 | `		return ReflectMakeAtom(pCtx, zBody, nBody);` |
|      - | 1537 | `	}` |
|    812 | 1538 | `	if( bNullable ){` |
|      - | 1539 | `		char zBuf[256];` |
|    143 | 1540 | `		if( nBody + 1 < (int)sizeof(zBuf) ){` |
|    143 | 1541 | `			zBuf[0] = '?';` |
|    143 | 1542 | `			SyMemcpy(zBody, &zBuf[1], (sxu32)nBody);` |
|    143 | 1543 | `			return ReflectMakeAtom(pCtx, zBuf, nBody + 1);` |
|      - | 1544 | `		}` |
|    ! 0 | 1545 | `	}` |
|    672 | 1546 | `	return ReflectMakeAtom(pCtx, zBody, nBody);` |
|    466 | 1547 | `}` |
|      - | 1548 | `/*` |
|      - | 1549 | ` * Declare the four type classes. Called from PH7_VmInstallReflection() where` |
|      - | 1550 | `` * chunk 4 used to be compiled, so `Stringable` (a core interface) already`` |
|      - | 1551 | ` * exists. PH7_CLASS_NOCLONE is php's own rule for these: they are values the` |
|      - | 1552 | `` * engine hands out, and `clone $type` is an Error, not a copy.`` |
|      - | 1553 | ` */` |
|   5254 | 1554 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionTypes(ph7_vm *pVm)` |
|      5 | 1555 | `{` |
|      - | 1556 | `	static const PH7_NativePropDef aBaseProp[] = {` |
|      - | 1557 | `		{ RT_TEXT,     PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - | 1558 | `		{ RT_NULLABLE, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 }, 0 },` |
|      - | 1559 | `	};` |
|      - | 1560 | `	static const PH7_NativeMethodDef aBaseMethod[] = {` |
|      - | 1561 | `		{ "allowsNull", PH7_MOD_PUBLIC, "", "@bool",       vm_builtin_ReflectionType_allowsNull },` |
|      - | 1562 | `		{ "__toString", PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionType_toString },` |
|      - | 1563 | `	};` |
|      - | 1564 | `	static const PH7_NativePropDef aNamedProp[] = {` |
|      - | 1565 | `		{ RT_TNAME, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - | 1566 | `	};` |
|      - | 1567 | `	static const PH7_NativeMethodDef aNamedMethod[] = {` |
|      - | 1568 | `		{ "getName",   PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionNamedType_getName },` |
|      - | 1569 | `		{ "isBuiltin", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionNamedType_isBuiltin },` |
|      - | 1570 | `	};` |
|      - | 1571 | `	static const PH7_NativePropDef aCompProp[] = {` |
|      - | 1572 | `		{ RT_TYPES, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 1573 | `	};` |
|      - | 1574 | `	static const PH7_NativeMethodDef aCompMethod[] = {` |
|      - | 1575 | `		{ "getTypes", PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionType_getTypes },` |
|      - | 1576 | `	};` |
|      - | 1577 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 1578 | `		{ "ReflectionType", 0, "Stringable", PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 1579 | `		  aBaseMethod, SX_ARRAYSIZE(aBaseMethod), 0, 0, aBaseProp, SX_ARRAYSIZE(aBaseProp), 0, 0, 0 },` |
|      - | 1580 | `		{ "ReflectionNamedType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 1581 | `		  aNamedMethod, SX_ARRAYSIZE(aNamedMethod), 0, 0, aNamedProp, SX_ARRAYSIZE(aNamedProp), 0, 0, 0 },` |
|      - | 1582 | `		{ "ReflectionUnionType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 1583 | `		  aCompMethod, SX_ARRAYSIZE(aCompMethod), 0, 0, aCompProp, SX_ARRAYSIZE(aCompProp), 0, 0, 0 },` |
|      - | 1584 | `		{ "ReflectionIntersectionType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 1585 | `		  aCompMethod, SX_ARRAYSIZE(aCompMethod), 0, 0, aCompProp, SX_ARRAYSIZE(aCompProp), 0, 0, 0 },` |
|      - | 1586 | `	};` |
|   5259 | 1587 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 | 1588 | `}` |
|      - | 1589 | `/*` |
|      - | 1590 | ` * ---------------------------------------------------------------------------` |
|      - | 1591 | ` * The six standalone reflection classes.` |
|      - | 1592 | ` *` |
|      - | 1593 | ` * ReflectionGenerator, ReflectionFiber, ReflectionConstant, ReflectionExtension,` |
|      - | 1594 | ` * ReflectionZendExtension and ReflectionReference reflect ENGINE state rather` |
|      - | 1595 | ` * than a class hierarchy, so each was a prelude class over one or two thunks` |
|      - | 1596 | ` * whose only job was to hand that state to PHP. Their bodies are the C now, and` |
|      - | 1597 | ` * the four thunks that existed for them (__reflect_gen_info, __reflect_gen_exec,` |
|      - | 1598 | ` * __reflect_const_info, __reflect_ref_id) are gone with them.` |
|      - | 1599 | ` *` |
|      - | 1600 | ` * The ReflectionEnum family stays in the prelude: it extends ReflectionClass and` |
|      - | 1601 | ` * ReflectionClassConstant, which are still PHP.` |
|      - | 1602 | ` * ---------------------------------------------------------------------------` |
|      - | 1603 | ` */` |
|      - | 1604 | `#define RG_GEN   "__gen"` |
|      - | 1605 | `#define RF_FIBER "__fiber"` |
|      - | 1606 | `#define RR_ID    "__id"` |
|      - | 1607 |  |
|      - | 1608 | `/* Build an instance of a class that may still be PRELUDE PHP, running its` |
|      - | 1609 | ` * constructor. Answers 0 when the constructor threw (rc says which). */` |
|   1114 | 1610 | `static ph7_class_instance * ReflectConstruct(ph7_context *pCtx, const char *zClass,` |
|      - | 1611 | `	int nArg, ph7_value **apArg, sxi32 *pRc)` |
|      5 | 1612 | `{` |
|   1119 | 1613 | `	ph7_vm *pVm = pCtx->pVm;` |
|   1119 | 1614 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|      - | 1615 | `	ph7_class_instance *pThis;` |
|      - | 1616 | `	ph7_class_method *pCons;` |
|   1119 | 1617 | `	*pRc = PH7_OK;` |
|   1119 | 1618 | `	if( pClass == 0 ){` |
|    ! 0 | 1619 | `		return 0;` |
|      - | 1620 | `	}` |
|   1119 | 1621 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|   1119 | 1622 | `	if( pThis == 0 ){` |
|    ! 0 | 1623 | `		return 0;` |
|      - | 1624 | `	}` |
|   1119 | 1625 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|   1119 | 1626 | `	if( pCons ){` |
|   1119 | 1627 | `		sxi32 rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, nArg, apArg);` |
|   1119 | 1628 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|    ! 0 | 1629 | `			PH7_ClassInstanceUnref(pThis);` |
|    ! 0 | 1630 | `			*pRc = rc;` |
|    ! 0 | 1631 | `			return 0;` |
|      - | 1632 | `		}` |
|    557 | 1633 | `	}` |
|   1119 | 1634 | `	return pThis;` |
|    562 | 1635 | `}` |
|      - | 1636 | `/* The instance a native reflector wraps ($this->__gen / $this->__fiber). */` |
|     48 | 1637 | `static ph7_class_instance * ReflectWrapped(ph7_context *pCtx, const char *zSlot)` |
|      1 | 1638 | `{` |
|     49 | 1639 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     49 | 1640 | `	return pThis ? PH7_NativeAttrObj(pThis, zSlot) : 0;` |
|      1 | 1641 | `}` |
|      - | 1642 | `/* Hand back a wrapped instance the receiver already owns (a borrowed reference). */` |
|     10 | 1643 | `static int ReflectResultBorrowed(ph7_context *pCtx, ph7_class_instance *pObj)` |
|      1 | 1644 | `{` |
|      - | 1645 | `	ph7_value sVal;` |
|     11 | 1646 | `	if( pObj == 0 ){` |
|    ! 0 | 1647 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1648 | `		return PH7_OK;` |
|      - | 1649 | `	}` |
|     11 | 1650 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     11 | 1651 | `	sVal.x.pOther = pObj;` |
|     11 | 1652 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|     11 | 1653 | `	ph7_result_value(pCtx, &sVal);   /* takes its own reference */` |
|     11 | 1654 | `	return PH7_OK;` |
|      6 | 1655 | `}` |
|      - | 1656 | `/*` |
|      - | 1657 | ` * ---------------------------------------------------------------------------` |
|      - | 1658 | ` * ReflectionAttribute — chunk 7.` |
|      - | 1659 | ` *` |
|      - | 1660 | ` * php's own class, plus the shared getAttributes() body that produces it. The` |
|      - | 1661 | ` * chunk it replaces also carried __reflect_target_names (folded into the` |
|      - | 1662 | ` * "cannot target" diagnostic below) and __reflect_has_deprecated, which had` |
|      - | 1663 | ` * already lost its last caller when isDeprecated() became C.` |
|      - | 1664 | ` *` |
|      - | 1665 | ` * An instance holds a SPEC, not the arguments: [kind, target, member,` |
|      - | 1666 | ` * paramIdx] names something the engine can reopen, and getArguments()` |
|      - | 1667 | ` * evaluates the recorded expressions on every call, because php does too --` |
|      - | 1668 | `` * `#[A(self::X)]` is php code and it runs when it is asked for. That is also`` |
|      - | 1669 | ` * why the prelude needed a public __init(): PHP could not fill a fresh object` |
|      - | 1670 | ` * any other way. C fills it directly, so __init() (and the` |
|      - | 1671 | ` * __reflect_new_no_ctor that built the empty shell) are gone with it.` |
|      - | 1672 | ` * ---------------------------------------------------------------------------` |
|      - | 1673 | ` */` |
|      - | 1674 | ``#define RA_NAME   "name"      /* php declares this one PUBLIC: `$attr->name` */`` |
|      - | 1675 | `#define RA_SPEC   "__spec"    /* [kind, target, member, paramIdx] */` |
|      - | 1676 | `#define RA_IDX    "__idx"     /* which of that target's attributes this is */` |
|      - | 1677 | `#define RA_TARGET "__target"  /* the Attribute::TARGET_* bit it was found on */` |
|      - | 1678 | `#define RA_REP    "__rep"     /* the target carries more than one of this name */` |
|      - | 1679 |  |
|      - | 1680 | `/* Bound on the value exporter's own recursion. An attribute argument cannot be` |
|      - | 1681 | ` * cyclic, but an OBJECT argument's property graph can be. */` |
|      - | 1682 | `#define REFLECT_EXPORT_MAX_DEPTH 31` |
|      - | 1683 |  |
|      - | 1684 | `/* php's Attribute::TARGET_* names, in bit order (TARGET_CLASS is bit 0). */` |
|      - | 1685 | `static const char *const azReflectTarget[] = {` |
|      - | 1686 | `	"class", "function", "method", "property", "class constant", "parameter", "constant"` |
|      - | 1687 | `};` |
|      - | 1688 |  |
|      - | 1689 | `static void ReflectExportValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth);` |
|      - | 1690 |  |
|      - | 1691 | `/*` |
|      - | 1692 | ` * A string the way php's reflection prints one, in either of php's TWO quotings.` |
|      - | 1693 | ` *` |
|      - | 1694 | ` * A USERLAND default prints single-quoted, with the NON-PRINTABLE bytes escaped` |
|      - | 1695 | ` * (php's smart_str_append_escaped) and the quote itself NOT escaped — php's own` |
|      - | 1696 | ` * output for "q'q" is 'q'q'. An INTERNAL one prints DOUBLE-quoted and escapes the` |
|      - | 1697 | `` * quote: php answers `string $enclosure = "\""` for str_getcsv where PHL used to`` |
|      - | 1698 | `` * answer `'"'`. Every internal function with a string default was affected; the`` |
|      - | 1699 | `` * pair was found on Closure::bindTo's `= "static"` while making that class native.`` |
|      - | 1700 | ` */` |
|     72 | 1701 | `static void ReflectExportStrQ(SyBlob *pOut, const char *zIn, sxu32 nIn, char cQuote)` |
|      1 | 1702 | `{` |
|      - | 1703 | `	static const char zHexDigit[] = "0123456789ABCDEF";` |
|      - | 1704 | `	sxu32 i;` |
|     73 | 1705 | `	SyBlobAppend(pOut, &cQuote, sizeof(char));` |
|    209 | 1706 | `	for( i = 0 ; i < nIn ; i++ ){` |
|    137 | 1707 | `		unsigned char c = (unsigned char)zIn[i];` |
|    137 | 1708 | `		if( c == (unsigned char)cQuote && cQuote == '"' ){` |
|      3 | 1709 | `			SyBlobAppend(pOut, "\\\"", sizeof("\\\"")-1);` |
|      3 | 1710 | `			continue;` |
|      - | 1711 | `		}` |
|      - | 1712 | `		{` |
|    135 | 1713 | `		const char *zEsc = 0;` |
|    135 | 1714 | `		switch( c ){` |
|      3 | 1715 | `			case 0x09: zEsc = "\\t"; break;` |
|      9 | 1716 | `			case 0x0A: zEsc = "\\n"; break;` |
|      3 | 1717 | `			case 0x0B: zEsc = "\\v"; break;` |
|      3 | 1718 | `			case 0x0C: zEsc = "\\f"; break;` |
|      3 | 1719 | `			case 0x0D: zEsc = "\\r"; break;` |
|      3 | 1720 | `			case 0x1B: zEsc = "\\e"; break;` |
|      5 | 1721 | `			case '\\': zEsc = "\\\\"; break;` |
|    112 | 1722 | `			default:   break;` |
|      - | 1723 | `		}` |
|    135 | 1724 | `		if( zEsc ){` |
|     23 | 1725 | `			SyBlobAppend(pOut, zEsc, sizeof("\\t")-1);` |
|    127 | 1726 | `		}else if( c < 0x20 \|\| c > 0x7E ){` |
|      - | 1727 | `			char zHex[4];` |
|      7 | 1728 | `			zHex[0] = '\\';` |
|      7 | 1729 | `			zHex[1] = 'x';` |
|      7 | 1730 | `			zHex[2] = zHexDigit[(c >> 4) & 0x0F];` |
|      7 | 1731 | `			zHex[3] = zHexDigit[c & 0x0F];` |
|      7 | 1732 | `			SyBlobAppend(pOut, zHex, sizeof(zHex));` |
|      4 | 1733 | `		}else{` |
|    107 | 1734 | `			SyBlobAppend(pOut, (const char *)&c, sizeof(char));` |
|      - | 1735 | `		}` |
|      - | 1736 | `		}` |
|     68 | 1737 | `	}` |
|     73 | 1738 | `	SyBlobAppend(pOut, &cQuote, sizeof(char));` |
|     73 | 1739 | `}` |
|      - | 1740 | `/* php's userland quoting, which is what every existing caller means. */` |
|     62 | 1741 | `static void ReflectExportStr(SyBlob *pOut, const char *zIn, sxu32 nIn)` |
|      1 | 1742 | `{` |
|     63 | 1743 | `	ReflectExportStrQ(pOut, zIn, nIn, '\'');` |
|     63 | 1744 | `}` |
|      - | 1745 | `/*` |
|      - | 1746 | ` * A float the way php's reflection prints one: the plain string cast (which` |
|      - | 1747 | `` * PHL already renders php's way, 1.0E+15 and all), then php's `zero_frac` —`` |
|      - | 1748 | ` * a float that came out without a fraction gets ".0" so 1.0 does not print as` |
|      - | 1749 | ` * 1. INF and NAN render as words and are left alone.` |
|      - | 1750 | ` */` |
|     20 | 1751 | `static void ReflectExportReal(ph7_vm *pVm, SyBlob *pOut, ph7_real rVal)` |
|      1 | 1752 | `{` |
|      - | 1753 | `	ph7_value sTmp;` |
|      - | 1754 | `	const char *zText;` |
|     21 | 1755 | `	int nText, i, bPlain = 1;` |
|     21 | 1756 | `	PH7_MemObjInitFromReal(pVm, &sTmp, rVal);` |
|     21 | 1757 | `	zText = ph7_value_to_string(&sTmp, &nText);` |
|     21 | 1758 | `	if( nText > 0 ){` |
|     21 | 1759 | `		SyBlobAppend(pOut, zText, (sxu32)nText);` |
|     10 | 1760 | `	}` |
|     47 | 1761 | `	for( i = 0 ; i < nText ; i++ ){` |
|     35 | 1762 | `		if( (zText[i] < '0' \|\| zText[i] > '9') && !(i == 0 && zText[i] == '-') ){` |
|      9 | 1763 | `			bPlain = 0;` |
|      9 | 1764 | `			break;` |
|      - | 1765 | `		}` |
|     14 | 1766 | `	}` |
|     21 | 1767 | `	if( bPlain && nText > 0 ){` |
|     13 | 1768 | `		SyBlobAppend(pOut, ".0", sizeof(".0")-1);` |
|      6 | 1769 | `	}` |
|     21 | 1770 | `	PH7_MemObjRelease(&sTmp);` |
|     21 | 1771 | `}` |
|      - | 1772 | `/*` |
|      - | 1773 | ` * An array the way php's reflection prints one: a LIST — every key an integer,` |
|      - | 1774 | ` * in sequence from zero — prints no keys at all, and anything else prints one` |
|      - | 1775 | `` * for EVERY entry. That is why `[1, 'k' => 2]` comes out as`` |
|      - | 1776 | `` * `[0 => 1, 'k' => 2]` while `[[1], [2 => 3]]` keeps its outer keys silent.`` |
|      - | 1777 | ` */` |
|     30 | 1778 | `static void ReflectExportArray(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth)` |
|      1 | 1779 | `{` |
|     31 | 1780 | `	ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|      - | 1781 | `	ph7_hashmap_node *pEntry;` |
|     31 | 1782 | `	sxi64 iExpect = 0;` |
|      - | 1783 | `	sxu32 n;` |
|     31 | 1784 | `	int bList = 1;` |
|     61 | 1785 | `	for( pEntry = pMap->pFirst, n = 0 ; n < pMap->nEntry && pEntry ; n++ ){` |
|     43 | 1786 | `		if( pEntry->iType == HASHMAP_BLOB_NODE \|\| pEntry->xKey.iKey != iExpect ){` |
|     13 | 1787 | `			bList = 0;` |
|     13 | 1788 | `			break;` |
|      - | 1789 | `		}` |
|     31 | 1790 | `		iExpect++;` |
|     31 | 1791 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     16 | 1792 | `	}` |
|     31 | 1793 | `	SyBlobAppend(pOut, "[", sizeof(char));` |
|     75 | 1794 | `	for( pEntry = pMap->pFirst, n = 0 ; n < pMap->nEntry && pEntry ; n++ ){` |
|     45 | 1795 | `		ph7_value *pMember = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|     45 | 1796 | `		if( n > 0 ){` |
|     17 | 1797 | `			SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|      8 | 1798 | `		}` |
|     45 | 1799 | `		if( !bList ){` |
|     21 | 1800 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|     13 | 1801 | `				ReflectExportStr(pOut, (const char *)SyBlobData(&pEntry->xKey.sKey),` |
|      4 | 1802 | `					SyBlobLength(&pEntry->xKey.sKey));` |
|      5 | 1803 | `			}else{` |
|     13 | 1804 | `				SyBlobFormat(pOut, "%qd", pEntry->xKey.iKey);` |
|      - | 1805 | `			}` |
|     21 | 1806 | `			SyBlobAppend(pOut, " => ", sizeof(" => ")-1);` |
|     10 | 1807 | `		}` |
|     45 | 1808 | `		ReflectExportValue(pCtx, pOut, pMember, iDepth + 1);` |
|     45 | 1809 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     23 | 1810 | `	}` |
|     31 | 1811 | `	SyBlobAppend(pOut, "]", sizeof(char));` |
|     31 | 1812 | `}` |
|      - | 1813 | `/*` |
|      - | 1814 | `` * One value in php's reflection export syntax — the text after `= ` in a`` |
|      - | 1815 | `` * parameter default and inside `Argument #0 [ … ]` in an attribute dump.`` |
|      - | 1816 | ` *` |
|      - | 1817 | ` * The type dispatch is PH7_MemObjDump's, including its REAL-before-INT order:` |
|      - | 1818 | ` * an integer-valued float carries a cached int view too, and php prints 1.0.` |
|      - | 1819 | ` *` |
|      - | 1820 | ` * php renders an OBJECT argument by echoing the source expression it never` |
|      - | 1821 | `` * folded (`new \A(x: 1)`), which needs the AST. PHL evaluates attribute`` |
|      - | 1822 | ` * arguments from byte-code and has only the resulting VALUE, so a non-enum` |
|      - | 1823 | ` * object is rebuilt from its own properties: same shape, not always the same` |
|      - | 1824 | ` * bytes. An enum case — the one object php's own value formatter handles —` |
|      - | 1825 | ` * matches exactly.` |
|      - | 1826 | ` */` |
|    182 | 1827 | `static void ReflectExportValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth)` |
|      1 | 1828 | `{` |
|    183 | 1829 | `	if( pVal == 0 \|\| iDepth > REFLECT_EXPORT_MAX_DEPTH ){` |
|    ! 0 | 1830 | `		SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|    ! 0 | 1831 | `		return;` |
|      - | 1832 | `	}` |
|    183 | 1833 | `	if( (pVal->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      3 | 1834 | `		ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|      3 | 1835 | `		if( pObj->pClass->iFlags & PH7_CLASS_ENUM ){` |
|      3 | 1836 | `			ph7_value *pName = PH7_EnumCaseNameValue(pObj);` |
|      - | 1837 | `			/* php writes the case as the source did, so a namespaced enum comes` |
|      - | 1838 | `			 * out fully qualified (\N\E::One) and a global one bare (E::One).` |
|      - | 1839 | `			 * The separator is the only thing left of that distinction here. */` |
|      3 | 1840 | `			if( SyByteFind(SyStringData(&pObj->pClass->sName),` |
|      4 | 1841 | `				SyStringLength(&pObj->pClass->sName), '\\', 0) == SXRET_OK ){` |
|    ! 0 | 1842 | `				SyBlobAppend(pOut, "\\", sizeof(char));` |
|    ! 0 | 1843 | `			}` |
|      3 | 1844 | `			SyBlobFormat(pOut, "%z::", &pObj->pClass->sName);` |
|      3 | 1845 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|      3 | 1846 | `				SyBlobAppend(pOut, SyBlobData(&pName->sBlob), SyBlobLength(&pName->sBlob));` |
|      1 | 1847 | `			}` |
|      3 | 1848 | `			return;` |
|      - | 1849 | `		}` |
|    ! 0 | 1850 | `		SyBlobFormat(pOut, "new \\%z(", &pObj->pClass->sName);` |
|      - | 1851 | `		{` |
|      - | 1852 | `			SyHashEntry *pEntry;` |
|    ! 0 | 1853 | `			int nWritten = 0;` |
|    ! 0 | 1854 | `			SyHashResetLoopCursor(&pObj->hAttr);` |
|    ! 0 | 1855 | `			while((pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){` |
|    ! 0 | 1856 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 1857 | `				ph7_value *pSlot;` |
|    ! 0 | 1858 | `				if( pVmAttr->pAttr->iFlags` |
|    ! 0 | 1859 | `					& (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL\|PH7_CLASS_ATTR_HIDDEN) ){` |
|    ! 0 | 1860 | `					continue; /* class-level members are not part of the object */` |
|      - | 1861 | `				}` |
|    ! 0 | 1862 | `				pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|    ! 0 | 1863 | `				if( nWritten++ ){` |
|    ! 0 | 1864 | `					SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|    ! 0 | 1865 | `				}` |
|    ! 0 | 1866 | `				ReflectExportValue(pCtx, pOut, pSlot, iDepth + 1);` |
|    ! 0 | 1867 | `			}` |
|      - | 1868 | `		}` |
|    ! 0 | 1869 | `		SyBlobAppend(pOut, ")", sizeof(char));` |
|    ! 0 | 1870 | `		return;` |
|      - | 1871 | `	}` |
|    181 | 1872 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|     21 | 1873 | `		SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|     21 | 1874 | `		return;` |
|      - | 1875 | `	}` |
|    161 | 1876 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|     31 | 1877 | `		ReflectExportArray(pCtx, pOut, pVal, iDepth);` |
|     31 | 1878 | `		return;` |
|      - | 1879 | `	}` |
|    131 | 1880 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|      5 | 1881 | `		if( pVal->x.iVal != 0 ){` |
|      3 | 1882 | `			SyBlobAppend(pOut, "true", sizeof("true")-1);` |
|      2 | 1883 | `		}else{` |
|      3 | 1884 | `			SyBlobAppend(pOut, "false", sizeof("false")-1);` |
|      - | 1885 | `		}` |
|      5 | 1886 | `		return;` |
|      - | 1887 | `	}` |
|    127 | 1888 | `	if( pVal->iFlags & MEMOBJ_REAL ){` |
|     21 | 1889 | `		ReflectExportReal(pCtx->pVm, pOut, pVal->rVal);` |
|     21 | 1890 | `		return;` |
|      - | 1891 | `	}` |
|    107 | 1892 | `	if( pVal->iFlags & MEMOBJ_INT ){` |
|     53 | 1893 | `		SyBlobFormat(pOut, "%qd", pVal->x.iVal);` |
|     53 | 1894 | `		return;` |
|      - | 1895 | `	}` |
|     55 | 1896 | `	if( pVal->iFlags & MEMOBJ_STRING ){` |
|     55 | 1897 | `		ReflectExportStr(pOut, (const char *)SyBlobData(&pVal->sBlob), SyBlobLength(&pVal->sBlob));` |
|     55 | 1898 | `		return;` |
|      - | 1899 | `	}` |
|      - | 1900 | `	/* A resource, and anything else php has no export syntax for. */` |
|    ! 0 | 1901 | `	SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|     92 | 1902 | `}` |
|      - | 1903 | `/*` |
|      - | 1904 | ` * Build one ReflectionAttribute. pSpec is shared by every attribute of the` |
|      - | 1905 | ` * same target — it says how to reopen it, not which one this is.` |
|      - | 1906 | ` */` |
|    118 | 1907 | `static ph7_class_instance * ReflectAttrNew(ph7_context *pCtx, SyString *pName,` |
|      - | 1908 | `	ph7_value *pSpec, sxu32 nIdx, int iTargetBit, int bRepeated)` |
|      2 | 1909 | `{` |
|    120 | 1910 | `	ph7_vm *pVm = pCtx->pVm;` |
|    120 | 1911 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, "ReflectionAttribute",` |
|      - | 1912 | `		sizeof("ReflectionAttribute")-1, FALSE, 0);` |
|    120 | 1913 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|    120 | 1914 | `	if( pObj == 0 ){` |
|    ! 0 | 1915 | `		return 0;` |
|      - | 1916 | `	}` |
|    120 | 1917 | `	PH7_NativeSetAttrStr(pVm, pObj, RA_NAME, SyStringData(pName), (int)SyStringLength(pName));` |
|    120 | 1918 | `	PH7_NativeSetProp(pVm, pObj, RA_SPEC, sizeof(RA_SPEC)-1, pSpec);` |
|    120 | 1919 | `	PH7_NativeSetAttrInt(pVm, pObj, RA_IDX, (sxi64)nIdx);` |
|    120 | 1920 | `	PH7_NativeSetAttrInt(pVm, pObj, RA_TARGET, iTargetBit);` |
|    120 | 1921 | `	PH7_NativeSetAttrBool(pVm, pObj, RA_REP, bRepeated);` |
|    120 | 1922 | `	return pObj;` |
|     61 | 1923 | `}` |
|      - | 1924 | `/*` |
|      - | 1925 | ` * Unpack the receiver's spec into the four-value vector ReflectAttrArgs takes.` |
|      - | 1926 | ` * Returns 0 when the object is not one this file built.` |
|      - | 1927 | ` */` |
|     72 | 1928 | `static int ReflectAttrSpec(ph7_context *pCtx, ph7_class_instance *pThis, ph7_value **apOut)` |
|      1 | 1929 | `{` |
|     73 | 1930 | `	ph7_value *pSpec = pThis ? PH7_NativeAttr(pThis, RA_SPEC) : 0;` |
|      - | 1931 | `	ph7_hashmap *pMap;` |
|      - | 1932 | `	int i;` |
|     73 | 1933 | `	if( pSpec == 0 \|\| (pSpec->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|    ! 0 | 1934 | `		return 0;` |
|      - | 1935 | `	}` |
|     73 | 1936 | `	pMap = (ph7_hashmap *)pSpec->x.pOther;` |
|    361 | 1937 | `	for( i = 0 ; i < 4 ; i++ ){` |
|      - | 1938 | `		ph7_value sKey;` |
|    289 | 1939 | `		ph7_hashmap_node *pNode = 0;` |
|      - | 1940 | `		sxi32 rc;` |
|    289 | 1941 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sKey, i);` |
|    289 | 1942 | `		rc = PH7_HashmapLookup(pMap, &sKey, &pNode);` |
|    289 | 1943 | `		PH7_MemObjRelease(&sKey);` |
|    289 | 1944 | `		if( rc != SXRET_OK \|\| pNode == 0 ){` |
|    ! 0 | 1945 | `			return 0;` |
|      - | 1946 | `		}` |
|    289 | 1947 | `		apOut[i] = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pNode->nValIdx);` |
|    289 | 1948 | `		if( apOut[i] == 0 ){` |
|    ! 0 | 1949 | `			return 0;` |
|      - | 1950 | `		}` |
|    145 | 1951 | `	}` |
|     73 | 1952 | `	return 1;` |
|     37 | 1953 | `}` |
|      - | 1954 | `/* The receiver's evaluated arguments, or an empty array when the target no` |
|      - | 1955 | `` * longer resolves (what the chunk's `$a === null ? array() : $a` said). */`` |
|     72 | 1956 | `static ph7_value * ReflectAttrOwnArgs(ph7_context *pCtx, ph7_class_instance *pThis)` |
|      1 | 1957 | `{` |
|      - | 1958 | `	ph7_value *apSpec[4];` |
|     73 | 1959 | `	ph7_value *pArgs = 0;` |
|     73 | 1960 | `	if( pThis && ReflectAttrSpec(pCtx, pThis, apSpec) ){` |
|     73 | 1961 | `		pArgs = ReflectAttrArgs(pCtx, apSpec, (sxu32)PH7_NativeAttrInt(pThis, RA_IDX));` |
|     36 | 1962 | `	}` |
|     73 | 1963 | `	return pArgs ? pArgs : ph7_context_new_array(pCtx);` |
|      1 | 1964 | `}` |
|     26 | 1965 | `static int vm_builtin_ReflectionAttribute_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 1966 | `{` |
|     28 | 1967 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     28 | 1968 | `	const char *zName = "";` |
|     28 | 1969 | `	int nName = 0;` |
|     13 | 1970 | `	SXUNUSED(nArg);` |
|     13 | 1971 | `	SXUNUSED(apArg);` |
|     28 | 1972 | `	if( pThis ){` |
|     28 | 1973 | `		PH7_NativeAttrStr(pThis, RA_NAME, &zName, &nName);` |
|     13 | 1974 | `	}` |
|     28 | 1975 | `	ph7_result_string(pCtx, zName, nName);` |
|     28 | 1976 | `	return PH7_OK;` |
|      2 | 1977 | `}` |
|     24 | 1978 | `static int vm_builtin_ReflectionAttribute_getTarget(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1979 | `{` |
|     25 | 1980 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     12 | 1981 | `	SXUNUSED(nArg);` |
|     12 | 1982 | `	SXUNUSED(apArg);` |
|     25 | 1983 | `	ph7_result_int64(pCtx, pThis ? PH7_NativeAttrInt(pThis, RA_TARGET) : 0);` |
|     25 | 1984 | `	return PH7_OK;` |
|      1 | 1985 | `}` |
|     18 | 1986 | `static int vm_builtin_ReflectionAttribute_isRepeated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1987 | `{` |
|     19 | 1988 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      9 | 1989 | `	SXUNUSED(nArg);` |
|      9 | 1990 | `	SXUNUSED(apArg);` |
|     19 | 1991 | `	ph7_result_bool(pCtx, pThis != 0 && PH7_NativeAttrTruthy(pThis, RA_REP));` |
|     19 | 1992 | `	return PH7_OK;` |
|      1 | 1993 | `}` |
|     30 | 1994 | `static int vm_builtin_ReflectionAttribute_getArguments(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1995 | `{` |
|     31 | 1996 | `	ph7_value *pArgs = ReflectAttrOwnArgs(pCtx, PH7_ContextThis(pCtx));` |
|     15 | 1997 | `	SXUNUSED(nArg);` |
|     15 | 1998 | `	SXUNUSED(apArg);` |
|     31 | 1999 | `	if( pArgs == 0 ){` |
|    ! 0 | 2000 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2001 | `	}` |
|     31 | 2002 | `	ph7_result_value(pCtx, pArgs);` |
|     31 | 2003 | `	return PH7_OK;` |
|     16 | 2004 | `}` |
|      - | 2005 | `/*` |
|      - | 2006 | ` * newInstance(): the four screens php runs before it constructs anything —` |
|      - | 2007 | ` * the attribute class exists, it is declared #[Attribute], that declaration` |
|      - | 2008 | ` * allows the target this was found on, and it allows repetition if it was` |
|      - | 2009 | ` * repeated. The messages are php's, byte for byte.` |
|      - | 2010 | ` */` |
|     48 | 2011 | `static int vm_builtin_ReflectionAttribute_newInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2012 | `{` |
|     49 | 2013 | `	ph7_vm *pVm = pCtx->pVm;` |
|     49 | 2014 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     49 | 2015 | `	ph7_value *pNameVal = pThis ? PH7_NativeAttr(pThis, RA_NAME) : 0;` |
|      - | 2016 | `	ph7_class *pClass;` |
|      - | 2017 | `	ph7_attribute *aA;` |
|      - | 2018 | `	ph7_value *pDeclArgs;` |
|     49 | 2019 | `	sxu32 n, nDecl = 0;` |
|     49 | 2020 | `	int bDecl = 0, iTarget, iBit;` |
|     49 | 2021 | `	sxi64 iFlags = 127; /* php's TARGET_ALL: #[Attribute] with no argument */` |
|     24 | 2022 | `	SXUNUSED(nArg);` |
|     24 | 2023 | `	SXUNUSED(apArg);` |
|     49 | 2024 | `	if( pNameVal == 0 ){` |
|    ! 0 | 2025 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2026 | `		return PH7_OK;` |
|      - | 2027 | `	}` |
|     49 | 2028 | `	pClass = ReflectResolveClass(pVm, pNameVal);` |
|     49 | 2029 | `	if( pClass == 0 ){` |
|      - | 2030 | `		SyString sName;` |
|      5 | 2031 | `		SyStringInitFromBuf(&sName, SyBlobData(&pNameVal->sBlob), SyBlobLength(&pNameVal->sBlob));` |
|      5 | 2032 | `		return PH7_VmThrowException(pCtx, "Error", "Attribute class \"%z\" not found", &sName);` |
|      - | 2033 | `	}` |
|      - | 2034 | `	/* Which #[...] on the attribute class is its own #[Attribute] declaration */` |
|     45 | 2035 | `	aA = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);` |
|     45 | 2036 | `	for( n = 0 ; n < SySetUsed(&pClass->aAttrs) ; n++ ){` |
|     40 | 2037 | `		if( SyStringLength(&aA[n].sName) == sizeof("Attribute")-1` |
|     41 | 2038 | `		 && SyStrnicmp(SyStringData(&aA[n].sName), "Attribute", sizeof("Attribute")-1) == 0 ){` |
|     41 | 2039 | `			nDecl = n;` |
|     41 | 2040 | `			bDecl = 1;` |
|     41 | 2041 | `			break;` |
|      - | 2042 | `		}` |
|    ! 0 | 2043 | `	}` |
|     45 | 2044 | `	if( !bDecl ){` |
|      7 | 2045 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      2 | 2046 | `			"Attempting to use non-attribute class \"%z\" as attribute", &pClass->sName);` |
|      - | 2047 | `	}` |
|      - | 2048 | ``	/* Its argument is the target mask: positional, or named `flags:`. */`` |
|      - | 2049 | `	{` |
|      - | 2050 | `		ph7_value *apSpec[4];` |
|     41 | 2051 | `		ph7_value *pKind = ph7_context_new_scalar(pCtx);` |
|     41 | 2052 | `		ph7_value *pMem  = ph7_context_new_scalar(pCtx);` |
|     41 | 2053 | `		ph7_value *pIdx  = ph7_context_new_scalar(pCtx);` |
|     41 | 2054 | `		if( pKind == 0 \|\| pMem == 0 \|\| pIdx == 0 ){` |
|    ! 0 | 2055 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2056 | `		}` |
|     41 | 2057 | `		ph7_value_string(pKind, "class", sizeof("class")-1);` |
|     41 | 2058 | `		ph7_value_null(pMem);` |
|     41 | 2059 | `		ph7_value_int(pIdx, 0);` |
|     41 | 2060 | `		apSpec[0] = pKind;` |
|     41 | 2061 | `		apSpec[1] = pNameVal;` |
|     41 | 2062 | `		apSpec[2] = pMem;` |
|     41 | 2063 | `		apSpec[3] = pIdx;` |
|     41 | 2064 | `		pDeclArgs = ReflectAttrArgs(pCtx, apSpec, nDecl);` |
|      - | 2065 | `	}` |
|     41 | 2066 | `	if( pDeclArgs ){` |
|     41 | 2067 | `		ph7_value *pFlags = ph7_array_fetch(pDeclArgs, "0", -1);` |
|     41 | 2068 | `		if( pFlags == 0 ){` |
|     17 | 2069 | `			pFlags = ph7_array_fetch(pDeclArgs, "flags", -1);` |
|      8 | 2070 | `		}` |
|     41 | 2071 | `		if( pFlags ){` |
|     25 | 2072 | `			iFlags = ph7_value_to_int64(pFlags);` |
|     12 | 2073 | `		}` |
|     20 | 2074 | `	}` |
|     41 | 2075 | `	iTarget = (int)PH7_NativeAttrInt(pThis, RA_TARGET);` |
|      - | 2076 | `	/* php's INTERNAL attributes carry a validator beside their mask, and` |
|      - | 2077 | ``	 * `#[\Deprecated]` has the one that matters here: its mask includes`` |
|      - | 2078 | `	 * TARGET_CLASS (php 8.5 marks a deprecated TRAIT with it), but every other` |
|      - | 2079 | ``	 * kind of class is refused by name — `Cannot apply #[\Deprecated] to class`` |
|      - | 2080 | ``	 * X`. PHL has no traits, so the class target is always the refusal. php runs`` |
|      - | 2081 | `	 * this at COMPILE time; PHL's attribute checks all happen here. */` |
|     40 | 2082 | `	if( iTarget == 1` |
|     30 | 2083 | `	 && SyStringLength(&pClass->sName) == sizeof("Deprecated")-1` |
|     11 | 2084 | `	 && SyStrnicmp(SyStringData(&pClass->sName), "Deprecated", sizeof("Deprecated")-1) == 0 ){` |
|    ! 0 | 2085 | `		ph7_class *pTarget = 0;` |
|    ! 0 | 2086 | `		ph7_value *pSpec = PH7_NativeAttr(pThis, RA_SPEC);` |
|    ! 0 | 2087 | `		if( pSpec && (pSpec->iFlags & MEMOBJ_HASHMAP) ){` |
|    ! 0 | 2088 | `			ph7_value *pName = ph7_array_fetch(pSpec, "1", -1);` |
|    ! 0 | 2089 | `			if( pName ){` |
|    ! 0 | 2090 | `				pTarget = ReflectResolveClass(pVm, pName);` |
|    ! 0 | 2091 | `			}` |
|    ! 0 | 2092 | `		}` |
|    ! 0 | 2093 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      - | 2094 | `			"Cannot apply #[\\Deprecated] to class %z",` |
|    ! 0 | 2095 | `			pTarget ? &pTarget->sName : &pClass->sName);` |
|      - | 2096 | `	}` |
|     41 | 2097 | `	if( (iFlags & iTarget) == 0 ){` |
|      - | 2098 | `		SyBlob sAllowed;` |
|      - | 2099 | `		sxi32 rc;` |
|      7 | 2100 | `		SyBlobInit(&sAllowed, &pVm->sAllocator);` |
|     49 | 2101 | `		for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azReflectTarget) ; iBit++ ){` |
|     43 | 2102 | `			if( (iFlags & ((sxi64)1 << iBit)) == 0 ){` |
|     37 | 2103 | `				continue;` |
|      - | 2104 | `			}` |
|      7 | 2105 | `			if( SyBlobLength(&sAllowed) > 0 ){` |
|    ! 0 | 2106 | `				SyBlobAppend(&sAllowed, ", ", sizeof(", ")-1);` |
|    ! 0 | 2107 | `			}` |
|     10 | 2108 | `			SyBlobAppend(&sAllowed, azReflectTarget[iBit],` |
|      6 | 2109 | `				(sxu32)SyStrlen(azReflectTarget[iBit]));` |
|      4 | 2110 | `		}` |
|      7 | 2111 | `		SyBlobAppend(&sAllowed, "", sizeof(char)); /* NUL for the %s below */` |
|     15 | 2112 | `		for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azReflectTarget) ; iBit++ ){` |
|     15 | 2113 | `			if( iTarget == (1 << iBit) ){` |
|      7 | 2114 | `				break;` |
|      - | 2115 | `			}` |
|      5 | 2116 | `		}` |
|     10 | 2117 | `		rc = PH7_VmThrowException(pCtx, "Error",` |
|      3 | 2118 | `			"Attribute \"%z\" cannot target %s (allowed targets: %s)", &pClass->sName,` |
|      3 | 2119 | `			iBit < (int)SX_ARRAYSIZE(azReflectTarget) ? azReflectTarget[iBit] : "",` |
|      3 | 2120 | `			SyBlobData(&sAllowed));` |
|      7 | 2121 | `		SyBlobRelease(&sAllowed);` |
|      7 | 2122 | `		return rc;` |
|      - | 2123 | `	}` |
|     35 | 2124 | `	if( PH7_NativeAttrTruthy(pThis, RA_REP) && (iFlags & 128) == 0 ){` |
|      7 | 2125 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      2 | 2126 | `			"Attribute \"%z\" must not be repeated", &pClass->sName);` |
|      - | 2127 | `	}` |
|     31 | 2128 | `	return ReflectAttrInstantiate(pCtx, pNameVal, ReflectAttrOwnArgs(pCtx, pThis));` |
|     25 | 2129 | `}` |
|      - | 2130 | `/*` |
|      - | 2131 | `` * php's export text. A bare `Attribute [ Name ]` when there are no arguments;`` |
|      - | 2132 | ` * otherwise the same head followed by the argument block, each argument` |
|      - | 2133 | `` * rendered in php's value-export syntax and named ones as `name = value`.`` |
|      - | 2134 | ` */` |
|     12 | 2135 | `static int vm_builtin_ReflectionAttribute_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2136 | `{` |
|     13 | 2137 | `	ph7_vm *pVm = pCtx->pVm;` |
|     13 | 2138 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     13 | 2139 | `	ph7_value *pArgs = ReflectAttrOwnArgs(pCtx, pThis);` |
|     13 | 2140 | `	const char *zName = "";` |
|     13 | 2141 | `	int nName = 0;` |
|      - | 2142 | `	SyBlob sOut;` |
|     13 | 2143 | `	sxu32 nCount = 0;` |
|      6 | 2144 | `	SXUNUSED(nArg);` |
|      6 | 2145 | `	SXUNUSED(apArg);` |
|     13 | 2146 | `	if( pThis ){` |
|     13 | 2147 | `		PH7_NativeAttrStr(pThis, RA_NAME, &zName, &nName);` |
|      6 | 2148 | `	}` |
|     13 | 2149 | `	if( pArgs && (pArgs->iFlags & MEMOBJ_HASHMAP) ){` |
|     13 | 2150 | `		nCount = ((ph7_hashmap *)pArgs->x.pOther)->nEntry;` |
|      6 | 2151 | `	}` |
|     13 | 2152 | `	SyBlobInit(&sOut, &pVm->sAllocator);` |
|     13 | 2153 | `	SyBlobAppend(&sOut, "Attribute [ ", sizeof("Attribute [ ")-1);` |
|     13 | 2154 | `	if( nName > 0 ){` |
|     13 | 2155 | `		SyBlobAppend(&sOut, zName, (sxu32)nName);` |
|      6 | 2156 | `	}` |
|     13 | 2157 | `	SyBlobAppend(&sOut, " ]", sizeof(" ]")-1);` |
|     13 | 2158 | `	if( nCount < 1 ){` |
|      3 | 2159 | `		SyBlobAppend(&sOut, "\n", sizeof(char));` |
|      2 | 2160 | `	}else{` |
|     11 | 2161 | `		ph7_hashmap *pMap = (ph7_hashmap *)pArgs->x.pOther;` |
|     11 | 2162 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|      - | 2163 | `		sxu32 n;` |
|     11 | 2164 | `		SyBlobFormat(&sOut, " {\n  - Arguments [%u] {\n", nCount);` |
|     81 | 2165 | `		for( n = 0 ; n < nCount && pEntry ; n++ ){` |
|     71 | 2166 | `			ph7_value *pMember = (ph7_value *)SySetAt(&pVm->aMemObj, pEntry->nValIdx);` |
|     71 | 2167 | `			SyBlobFormat(&sOut, "    Argument #%u [ ", n);` |
|     71 | 2168 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      7 | 2169 | `				SyBlobAppend(&sOut, SyBlobData(&pEntry->xKey.sKey),` |
|      2 | 2170 | `					SyBlobLength(&pEntry->xKey.sKey));` |
|      5 | 2171 | `				SyBlobAppend(&sOut, " = ", sizeof(" = ")-1);` |
|      2 | 2172 | `			}` |
|     71 | 2173 | `			ReflectExportValue(pCtx, &sOut, pMember, 0);` |
|     71 | 2174 | `			SyBlobAppend(&sOut, " ]\n", sizeof(" ]\n")-1);` |
|     71 | 2175 | `			pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     36 | 2176 | `		}` |
|     11 | 2177 | `		SyBlobAppend(&sOut, "  }\n}\n", sizeof("  }\n}\n")-1);` |
|      - | 2178 | `	}` |
|     13 | 2179 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     13 | 2180 | `	SyBlobRelease(&sOut);` |
|     13 | 2181 | `	return PH7_OK;` |
|      1 | 2182 | `}` |
|      - | 2183 | `/* The body of the two methods php declares PRIVATE and never calls. Neither is` |
|      - | 2184 | `` * reachable from php code: `new ReflectionAttribute` is the engine's own "Call`` |
|      - | 2185 | `` * to private … from global scope" Error, and `clone` is refused by`` |
|      - | 2186 | ` * PH7_CLASS_NOCLONE before any body runs. They exist so the class REPORTS them,` |
|      - | 2187 | ` * which is the only way php's own dump shows them too. */` |
|    ! 0 | 2188 | `static int vm_builtin_ReflectionAttribute_private(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2189 | `{` |
|    ! 0 | 2190 | `	SXUNUSED(nArg);` |
|    ! 0 | 2191 | `	SXUNUSED(apArg);` |
|    ! 0 | 2192 | `	SXUNUSED(pCtx);` |
|    ! 0 | 2193 | `	return PH7_OK;` |
|    ! 0 | 2194 | `}` |
|      - | 2195 | `/*` |
|      - | 2196 | ` * Declare ReflectionAttribute. Called from PH7_VmInstallReflection() where` |
|      - | 2197 | ` * chunk 7 used to be compiled, so Reflector (chunk 1) already exists.` |
|      - | 2198 | ` *` |
|      - | 2199 | ` * PH7_CLASS_NOCLONE is php's rule for it (php declares __clone private AND` |
|      - | 2200 | ` * refuses the copy), and the class is NOT final — php 8.5 unsealed it.` |
|      - | 2201 | ` */` |
|   5254 | 2202 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionAttribute(ph7_vm *pVm)` |
|      5 | 2203 | `{` |
|      - | 2204 | `	static const PH7_NativeConstDef aConst[] = {` |
|      - | 2205 | `		{ "IS_INSTANCEOF", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|      - | 2206 | `	};` |
|      - | 2207 | `	static const PH7_NativePropDef aProp[] = {` |
|      - | 2208 | `		{ RA_NAME,   PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 2209 | `		/* PHL-only, and PROTECTED so php code cannot reach them: the spec that` |
|      - | 2210 | `		 * reopens the target. php holds the same state on the C struct behind the` |
|      - | 2211 | `		 * object, invisible; PHL has no hidden-slot bit yet (§7.4 (e)), so these` |
|      - | 2212 | `		 * four still show up in a var_dump where php shows only $name. */` |
|      - | 2213 | `		{ RA_SPEC,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0,  0.0 }, 0 },` |
|      - | 2214 | `		{ RA_IDX,    PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0,  0.0 }, 0 },` |
|      - | 2215 | `		{ RA_TARGET, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0,  0.0 }, 0 },` |
|      - | 2216 | `		{ RA_REP,    PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0,  0.0 }, 0 },` |
|      - | 2217 | `	};` |
|      - | 2218 | `	/* php's own listing order, which is what __toString() prints. */` |
|      - | 2219 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|      - | 2220 | `		{ "getName",      PH7_MOD_PUBLIC,  "", "string", vm_builtin_ReflectionAttribute_getName },` |
|      - | 2221 | `		{ "getTarget",    PH7_MOD_PUBLIC,  "", "int",    vm_builtin_ReflectionAttribute_getTarget },` |
|      - | 2222 | `		{ "isRepeated",   PH7_MOD_PUBLIC,  "", "bool",   vm_builtin_ReflectionAttribute_isRepeated },` |
|      - | 2223 | `		{ "getArguments", PH7_MOD_PUBLIC,  "", "array",  vm_builtin_ReflectionAttribute_getArguments },` |
|      - | 2224 | `		{ "newInstance",  PH7_MOD_PUBLIC,  "", "object", vm_builtin_ReflectionAttribute_newInstance },` |
|      - | 2225 | `		{ "__toString",   PH7_MOD_PUBLIC,  "", "string", vm_builtin_ReflectionAttribute_toString },` |
|      - | 2226 | `		{ "__clone",      PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionAttribute_private },` |
|      - | 2227 | `		{ "__construct",  PH7_MOD_PRIVATE, "", 0,      vm_builtin_ReflectionAttribute_private },` |
|      - | 2228 | `	};` |
|      - | 2229 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 2230 | `		{ "ReflectionAttribute", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 2231 | `		  aMethod, SX_ARRAYSIZE(aMethod), aConst, SX_ARRAYSIZE(aConst),` |
|      - | 2232 | `		  aProp, SX_ARRAYSIZE(aProp), 0, 0, 0 },` |
|      - | 2233 | `	};` |
|   5259 | 2234 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 | 2235 | `}` |
|      - | 2236 | `/*` |
|      - | 2237 | ` * The shared getAttributes() body.` |
|      - | 2238 | ` *` |
|      - | 2239 | ` * Turns the target's #[...] records into ReflectionAttribute objects, applying` |
|      - | 2240 | ` * php's name / IS_INSTANCEOF filter. Argument VALUES stay lazy — what each` |
|      - | 2241 | ` * object carries is the [kind, target, member, paramIdx] spec that reopens` |
|      - | 2242 | ` * them — so a reflector declaring getAttributes() only has to say WHICH target` |
|      - | 2243 | ` * it is.` |
|      - | 2244 | ` */` |
|     92 | 2245 | `static int ReflectBuildAttrs(ph7_context *pCtx, SySet *pAttrs, const char *zKind,` |
|      - | 2246 | `	ph7_value *pTarget, const char *zMember, int nMember,` |
|      - | 2247 | `	int iParamIdx, int iTargetBit, int nArg, ph7_value **apArg)` |
|      2 | 2248 | `{` |
|     94 | 2249 | `	ph7_vm *pVm = pCtx->pVm;` |
|     94 | 2250 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|     94 | 2251 | `	ph7_class *pFilter = 0;` |
|     94 | 2252 | `	const char *zFilter = 0;` |
|     94 | 2253 | `	int nFilter = 0;` |
|      - | 2254 | `	ph7_value *pSpec, *pOut;` |
|      - | 2255 | `	sxu32 n, i;` |
|     94 | 2256 | `	pOut = ph7_context_new_array(pCtx);` |
|     94 | 2257 | `	pSpec = ph7_context_new_array(pCtx);` |
|     94 | 2258 | `	if( pOut == 0 \|\| pSpec == 0 ){` |
|    ! 0 | 2259 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2260 | `	}` |
|     94 | 2261 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|     15 | 2262 | `		zFilter = ph7_value_to_string(apArg[0], &nFilter);` |
|     15 | 2263 | `		if( nFilter < 1 ){` |
|    ! 0 | 2264 | `			zFilter = 0;` |
|    ! 0 | 2265 | `		}` |
|      - | 2266 | `		/* IS_INSTANCEOF: keep a SUBCLASS of the named attribute too. The exact` |
|      - | 2267 | `		 * name still matches on its own, so this only has to answer for the rest. */` |
|     15 | 2268 | `		if( zFilter && nArg > 1 && (ph7_value_to_int(apArg[1]) & 2) ){` |
|      5 | 2269 | `			pFilter = ReflectResolveClass(pVm, apArg[0]);` |
|      2 | 2270 | `		}` |
|      7 | 2271 | `	}` |
|      - | 2272 | `	{` |
|     94 | 2273 | `		ph7_value *pKind = ph7_context_new_scalar(pCtx);` |
|     94 | 2274 | `		ph7_value *pMem  = ph7_context_new_scalar(pCtx);` |
|     94 | 2275 | `		ph7_value *pIdx  = ph7_context_new_scalar(pCtx);` |
|     94 | 2276 | `		if( pKind == 0 \|\| pMem == 0 \|\| pIdx == 0 ){` |
|    ! 0 | 2277 | `			return PH7_ContextMemoryError(pCtx);` |
|      - | 2278 | `		}` |
|     94 | 2279 | `		ph7_value_string(pKind, zKind, -1);` |
|     94 | 2280 | `		if( zMember ){` |
|     25 | 2281 | `			ph7_value_string(pMem, zMember, nMember);` |
|     13 | 2282 | `		}else{` |
|     70 | 2283 | `			ph7_value_null(pMem);` |
|      - | 2284 | `		}` |
|     94 | 2285 | `		ph7_value_int(pIdx, iParamIdx);` |
|     94 | 2286 | `		ph7_array_add_elem(pSpec, 0, pKind);` |
|      - | 2287 | `		/* The target rides as a VALUE, not a name: a closure's attributes are` |
|      - | 2288 | `		 * reopened through the Closure object itself. */` |
|     94 | 2289 | `		ph7_array_add_elem(pSpec, 0, pTarget);` |
|     94 | 2290 | `		ph7_array_add_elem(pSpec, 0, pMem);` |
|     94 | 2291 | `		ph7_array_add_elem(pSpec, 0, pIdx);` |
|      - | 2292 | `	}` |
|    238 | 2293 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|    146 | 2294 | `		int bRepeated = 0;` |
|    146 | 2295 | `		if( zFilter ){` |
|     89 | 2296 | `			int bKeep = ((int)SyStringLength(&aA[n].sName) == nFilter` |
|     50 | 2297 | `				&& SyStrnicmp(SyStringData(&aA[n].sName), zFilter, (sxu32)nFilter) == 0);` |
|     51 | 2298 | `			if( !bKeep && pFilter ){` |
|     16 | 2299 | `				ph7_class *pCand = PH7_VmExtractClass(pVm, SyStringData(&aA[n].sName),` |
|     10 | 2300 | `					SyStringLength(&aA[n].sName), FALSE, 0);` |
|     11 | 2301 | `				if( pCand == 0 ){` |
|    ! 0 | 2302 | `					pCand = PH7_VmTriggerAutoload(pVm, SyStringData(&aA[n].sName),` |
|    ! 0 | 2303 | `						SyStringLength(&aA[n].sName), FALSE);` |
|    ! 0 | 2304 | `				}` |
|     11 | 2305 | `				bKeep = pCand != 0 && pCand != pFilter && PH7_VmInstanceOf(pCand, pFilter);` |
|      5 | 2306 | `			}` |
|     51 | 2307 | `			if( !bKeep ){` |
|     27 | 2308 | `				continue;` |
|      - | 2309 | `			}` |
|     12 | 2310 | `		}` |
|      - | 2311 | `		/* isRepeated() asks about the TARGET, not about the filtered result:` |
|      - | 2312 | `		 * two #[A] make both of them repeated even when only one is asked for. */` |
|    274 | 2313 | `		for( i = 0 ; i < SySetUsed(pAttrs) ; i++ ){` |
|    190 | 2314 | `			if( i != n && SyStringLength(&aA[i].sName) == SyStringLength(&aA[n].sName)` |
|     68 | 2315 | `			 && SyStrnicmp(SyStringData(&aA[i].sName), SyStringData(&aA[n].sName),` |
|     60 | 2316 | `				SyStringLength(&aA[n].sName)) == 0 ){` |
|     37 | 2317 | `				bRepeated = 1;` |
|     37 | 2318 | `				break;` |
|      - | 2319 | `			}` |
|     79 | 2320 | `		}` |
|    179 | 2321 | `		ReflectTypeListAdd(pCtx, pOut,` |
|    118 | 2322 | `			ReflectAttrNew(pCtx, &aA[n].sName, pSpec, n, iTargetBit, bRepeated));` |
|     61 | 2323 | `	}` |
|     94 | 2324 | `	ph7_result_value(pCtx, pOut);` |
|     94 | 2325 | `	return PH7_OK;` |
|     48 | 2326 | `}` |
|      - | 2327 | `/*` |
|      - | 2328 | ` * The three "no runtime line tracking" methods. php answers a file/line/trace` |
|      - | 2329 | ` * from the executing frame; PHL has no per-instruction line record (the same` |
|      - | 2330 | ` * gap debug_backtrace() has), so it says so loudly rather than inventing one.` |
|      - | 2331 | ` */` |
|    ! 0 | 2332 | `static int ReflectUnsupported(ph7_context *pCtx, const char *zWho)` |
|    ! 0 | 2333 | `{` |
|    ! 0 | 2334 | `	return PH7_VmThrowException(pCtx, "Error",` |
|    ! 0 | 2335 | `		"%s is not supported by PHL (no runtime line tracking)", zWho);` |
|    ! 0 | 2336 | `}` |
|      - | 2337 | `#define REFLECT_UNSUPPORTED(NAME,TEXT) \` |
|      - | 2338 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 2339 | `	{ \` |
|      - | 2340 | `		SXUNUSED(nArg); \` |
|      - | 2341 | `		SXUNUSED(apArg); \` |
|      - | 2342 | `		return ReflectUnsupported(pCtx, TEXT); \` |
|      - | 2343 | `	}` |
|    ! 0 | 2344 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_execLine,"ReflectionGenerator::getExecutingLine()")` |
|    ! 0 | 2345 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_execFile,"ReflectionGenerator::getExecutingFile()")` |
|    ! 0 | 2346 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_trace,"ReflectionGenerator::getTrace()")` |
|    ! 0 | 2347 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_execLine,"ReflectionFiber::getExecutingLine()")` |
|    ! 0 | 2348 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_execFile,"ReflectionFiber::getExecutingFile()")` |
|    ! 0 | 2349 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_trace,"ReflectionFiber::getTrace()")` |
|      - | 2350 |  |
|      - | 2351 | `/* ReflectionGenerator::__construct(Generator $generator) */` |
|     16 | 2352 | `static int vm_builtin_ReflectionGenerator_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2353 | `{` |
|     17 | 2354 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     17 | 2355 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 2356 | `		return PH7_OK;` |
|      - | 2357 | `	}` |
|     17 | 2358 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RG_GEN, (ph7_class_instance *)apArg[0]->x.pOther);` |
|     17 | 2359 | `	return PH7_OK;` |
|      9 | 2360 | `}` |
|      - | 2361 | `/* The coroutine context of the wrapped generator, or NULL. */` |
|     34 | 2362 | `static ph7_exec_ctx * ReflectGenExec(ph7_context *pCtx)` |
|      1 | 2363 | `{` |
|     35 | 2364 | `	ph7_class_instance *pGenObj = ReflectWrapped(pCtx, RG_GEN);` |
|      - | 2365 | `	ph7_value sVal;` |
|      - | 2366 | `	ph7_generator *pGen;` |
|     35 | 2367 | `	if( pGenObj == 0 ){` |
|    ! 0 | 2368 | `		return 0;` |
|      - | 2369 | `	}` |
|     35 | 2370 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     35 | 2371 | `	sVal.x.pOther = pGenObj;` |
|     35 | 2372 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|     35 | 2373 | `	pGen = ReflectGeneratorCtx(pCtx->pVm, &sVal);` |
|     35 | 2374 | `	return pGen ? pGen->pCtx : 0;` |
|     18 | 2375 | `}` |
|      8 | 2376 | `static int vm_builtin_ReflectionGenerator_isClosed(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2377 | `{` |
|      9 | 2378 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|      4 | 2379 | `	SXUNUSED(nArg);` |
|      4 | 2380 | `	SXUNUSED(apArg);` |
|     17 | 2381 | `	ph7_result_bool(pCtx, pExec != 0` |
|     12 | 2382 | `		&& (pExec->iState == PH7_CTX_STATE_COMPLETED \|\| pExec->iState == PH7_CTX_STATE_CLOSED));` |
|      9 | 2383 | `	return PH7_OK;` |
|      1 | 2384 | `}` |
|      - | 2385 | `/* ReflectionGenerator::getThis(): ?object — the receiver the generator's body` |
|      - | 2386 | ` * runs against. A coroutine frame installs it as a frame VARIABLE (see` |
|      - | 2387 | ` * VmFiberSetupFrame), not as pFrame->pThis, so both are checked. */` |
|      8 | 2388 | `static int vm_builtin_ReflectionGenerator_getThis(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2389 | `{` |
|      9 | 2390 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|      4 | 2391 | `	SXUNUSED(nArg);` |
|      4 | 2392 | `	SXUNUSED(apArg);` |
|      9 | 2393 | `	if( pExec && pExec->pFrame ){` |
|      9 | 2394 | `		SyHashEntry *pVar = SyHashGet(&pExec->pFrame->hVar, "this", sizeof("this")-1);` |
|      9 | 2395 | `		if( pVar ){` |
|      7 | 2396 | `			ph7_value *pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,` |
|      4 | 2397 | `				(sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|      5 | 2398 | `			if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|      5 | 2399 | `				ph7_result_value(pCtx, pSlot);` |
|      5 | 2400 | `				return PH7_OK;` |
|      - | 2401 | `			}` |
|    ! 0 | 2402 | `		}` |
|      5 | 2403 | `		if( pExec->pFrame->pThis ){` |
|    ! 0 | 2404 | `			return ReflectResultBorrowed(pCtx, pExec->pFrame->pThis);` |
|      - | 2405 | `		}` |
|      2 | 2406 | `	}` |
|      5 | 2407 | `	ph7_result_null(pCtx);` |
|      5 | 2408 | `	return PH7_OK;` |
|      5 | 2409 | `}` |
|      - | 2410 | `/* ReflectionGenerator::getFunction(): ReflectionFunctionAbstract — still a` |
|      - | 2411 | ` * prelude class, so it is built through its own constructor. */` |
|     18 | 2412 | `static int vm_builtin_ReflectionGenerator_getFunction(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2413 | `{` |
|     19 | 2414 | `	ph7_vm *pVm = pCtx->pVm;` |
|     19 | 2415 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|     19 | 2416 | `	ph7_vm_func *pFunc = pExec ? pExec->pFunc : 0;` |
|      - | 2417 | `	ph7_class_instance *pOut;` |
|      - | 2418 | `	ph7_value aArg[2];` |
|     19 | 2419 | `	int nCtorArg = 1;` |
|      - | 2420 | `	sxi32 rc;` |
|      9 | 2421 | `	SXUNUSED(nArg);` |
|      9 | 2422 | `	SXUNUSED(apArg);` |
|     19 | 2423 | `	if( pFunc == 0 ){` |
|    ! 0 | 2424 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2425 | `		return PH7_OK;` |
|      - | 2426 | `	}` |
|     19 | 2427 | `	PH7_MemObjInit(pVm, &aArg[0]);` |
|     19 | 2428 | `	PH7_MemObjInit(pVm, &aArg[1]);` |
|     19 | 2429 | `	if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      9 | 2430 | `		ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|      9 | 2431 | `		ph7_value_string(&aArg[0], SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|      9 | 2432 | `		ph7_value_string(&aArg[1], SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|      9 | 2433 | `		nCtorArg = 2;` |
|      5 | 2434 | `	}else{` |
|     11 | 2435 | `		ph7_value_string(&aArg[0], SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|      - | 2436 | `	}` |
|      - | 2437 | `	{` |
|      - | 2438 | `		ph7_value *apCtor[2];` |
|     19 | 2439 | `		apCtor[0] = &aArg[0];` |
|     19 | 2440 | `		apCtor[1] = &aArg[1];` |
|     28 | 2441 | `		pOut = ReflectConstruct(pCtx, nCtorArg == 2 ? "ReflectionMethod" : "ReflectionFunction",` |
|      9 | 2442 | `			nCtorArg, apCtor, &rc);` |
|      - | 2443 | `	}` |
|     19 | 2444 | `	PH7_MemObjRelease(&aArg[0]);` |
|     19 | 2445 | `	PH7_MemObjRelease(&aArg[1]);` |
|     19 | 2446 | `	if( pOut == 0 ){` |
|    ! 0 | 2447 | `		if( rc != PH7_OK ){` |
|    ! 0 | 2448 | `			return rc;` |
|      - | 2449 | `		}` |
|    ! 0 | 2450 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2451 | `		return PH7_OK;` |
|      - | 2452 | `	}` |
|     19 | 2453 | `	return ReflectResultObject(pCtx, pOut);` |
|     10 | 2454 | `}` |
|      - | 2455 | `` /* ReflectionGenerator::getExecutingGenerator(): Generator — follow `yield from` `` |
|      - | 2456 | ` * delegation to the innermost one that is actually running. */` |
|      6 | 2457 | `static int vm_builtin_ReflectionGenerator_getExecuting(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2458 | `{` |
|      7 | 2459 | `	ph7_vm *pVm = pCtx->pVm;` |
|      7 | 2460 | `	ph7_class_instance *pGenObj = ReflectWrapped(pCtx, RG_GEN);` |
|      - | 2461 | `	ph7_value sVal, *pCur;` |
|      - | 2462 | `	ph7_generator *pGen;` |
|      7 | 2463 | `	int iDepth = 0;` |
|      3 | 2464 | `	SXUNUSED(nArg);` |
|      3 | 2465 | `	SXUNUSED(apArg);` |
|      7 | 2466 | `	if( pGenObj == 0 ){` |
|    ! 0 | 2467 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2468 | `		return PH7_OK;` |
|      - | 2469 | `	}` |
|      7 | 2470 | `	PH7_MemObjInit(pVm, &sVal);` |
|      7 | 2471 | `	sVal.x.pOther = pGenObj;` |
|      7 | 2472 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|      7 | 2473 | `	pCur = &sVal;` |
|      7 | 2474 | `	pGen = ReflectGeneratorCtx(pVm, &sVal);` |
|     12 | 2475 | `	while( pGen && pGen->pCtx && pGen->pCtx->iDelegateState == 3` |
|     10 | 2476 | `	 && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|      3 | 2477 | `		ph7_generator *pInner = ReflectGeneratorCtx(pVm, &pGen->pCtx->sDelegate);` |
|      3 | 2478 | `		if( pInner == 0 ){` |
|    ! 0 | 2479 | `			break;` |
|      - | 2480 | `		}` |
|      3 | 2481 | `		pCur = &pGen->pCtx->sDelegate;` |
|      3 | 2482 | `		pGen = pInner;` |
|      3 | 2483 | `		iDepth++;` |
|      1 | 2484 | `	}` |
|      7 | 2485 | `	return ReflectResultBorrowed(pCtx, (ph7_class_instance *)pCur->x.pOther);` |
|      4 | 2486 | `}` |
|      - | 2487 | `/* ReflectionFiber::__construct(Fiber $fiber) / getFiber() / getCallable() */` |
|      4 | 2488 | `static int vm_builtin_ReflectionFiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2489 | `{` |
|      5 | 2490 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 | 2491 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 2492 | `		return PH7_OK;` |
|      - | 2493 | `	}` |
|      5 | 2494 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RF_FIBER, (ph7_class_instance *)apArg[0]->x.pOther);` |
|      5 | 2495 | `	return PH7_OK;` |
|      3 | 2496 | `}` |
|      4 | 2497 | `static int vm_builtin_ReflectionFiber_getFiber(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2498 | `{` |
|      2 | 2499 | `	SXUNUSED(nArg);` |
|      2 | 2500 | `	SXUNUSED(apArg);` |
|      5 | 2501 | `	return ReflectResultBorrowed(pCtx, ReflectWrapped(pCtx, RF_FIBER));` |
|      1 | 2502 | `}` |
|      4 | 2503 | `static int vm_builtin_ReflectionFiber_getCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2504 | `{` |
|      5 | 2505 | `	ph7_class_instance *pFiber = ReflectWrapped(pCtx, RF_FIBER);` |
|      5 | 2506 | `	ph7_value *pVal = pFiber ? PH7_NativeAttr(pFiber, "__callable") : 0;` |
|      2 | 2507 | `	SXUNUSED(nArg);` |
|      2 | 2508 | `	SXUNUSED(apArg);` |
|      5 | 2509 | `	if( pVal ){` |
|      5 | 2510 | `		ph7_result_value(pCtx, pVal);` |
|      2 | 2511 | `	}` |
|      5 | 2512 | `	return PH7_OK;` |
|      1 | 2513 | `}` |
|      - | 2514 | `/* ---- ReflectionConstant ---- */` |
|      - | 2515 | `/* The engine's record for a global constant, or NULL when undefined. */` |
|     60 | 2516 | `static ph7_constant * ReflectConstEntry(ph7_vm *pVm, const char *zName, int nName)` |
|      1 | 2517 | `{` |
|     61 | 2518 | `	SyHashEntry *pEntry = nName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zName, (sxu32)nName) : 0;` |
|     61 | 2519 | `	return pEntry ? (ph7_constant *)pEntry->pUserData : 0;` |
|      1 | 2520 | `}` |
|      - | 2521 | `/* The receiver's constant record, resolved from its public $name. */` |
|     34 | 2522 | `static ph7_constant * ReflectConstOf(ph7_context *pCtx)` |
|      1 | 2523 | `{` |
|     35 | 2524 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     35 | 2525 | `	const char *zName = "";` |
|     35 | 2526 | `	int nName = 0;` |
|     35 | 2527 | `	if( pThis ){` |
|     35 | 2528 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     17 | 2529 | `	}` |
|     35 | 2530 | `	return ReflectConstEntry(pCtx->pVm, zName, nName);` |
|      1 | 2531 | `}` |
|     26 | 2532 | `static int vm_builtin_ReflectionConstant_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2533 | `{` |
|     27 | 2534 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     27 | 2535 | `	int nName = 0;` |
|     27 | 2536 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|     27 | 2537 | `	if( pThis == 0 ){` |
|    ! 0 | 2538 | `		return PH7_OK;` |
|      - | 2539 | `	}` |
|     27 | 2540 | `	if( ReflectConstEntry(pCtx->pVm, zName, nName) == 0 ){` |
|     10 | 2541 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 | 2542 | `			"Constant \"%.*s\" does not exist", nName, zName);` |
|      - | 2543 | `	}` |
|     21 | 2544 | `	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", zName, nName);` |
|     21 | 2545 | `	return PH7_OK;` |
|     14 | 2546 | `}` |
|      4 | 2547 | `static int vm_builtin_ReflectionConstant_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2548 | `{` |
|      5 | 2549 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 | 2550 | `	const char *zName = "";` |
|      5 | 2551 | `	int nName = 0;` |
|      2 | 2552 | `	SXUNUSED(nArg);` |
|      2 | 2553 | `	SXUNUSED(apArg);` |
|      5 | 2554 | `	if( pThis ){` |
|      5 | 2555 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 | 2556 | `	}` |
|      5 | 2557 | `	ph7_result_string(pCtx, zName, nName);` |
|      5 | 2558 | `	return PH7_OK;` |
|      1 | 2559 | `}` |
|      - | 2560 | `/* The namespace split php reports: everything before the last '\', and the rest. */` |
|      8 | 2561 | `static int ReflectConstNsCut(const char *zName, int nName)` |
|      1 | 2562 | `{` |
|      - | 2563 | `	int k;` |
|    101 | 2564 | `	for( k = nName - 1 ; k >= 0 ; k-- ){` |
|     93 | 2565 | `		if( zName[k] == '\\' ){` |
|    ! 0 | 2566 | `			return k;` |
|      - | 2567 | `		}` |
|     47 | 2568 | `	}` |
|      9 | 2569 | `	return -1;` |
|      5 | 2570 | `}` |
|      4 | 2571 | `static int vm_builtin_ReflectionConstant_getNamespaceName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2572 | `{` |
|      5 | 2573 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 | 2574 | `	const char *zName = "";` |
|      5 | 2575 | `	int nName = 0, iCut;` |
|      2 | 2576 | `	SXUNUSED(nArg);` |
|      2 | 2577 | `	SXUNUSED(apArg);` |
|      5 | 2578 | `	if( pThis ){` |
|      5 | 2579 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 | 2580 | `	}` |
|      5 | 2581 | `	iCut = ReflectConstNsCut(zName, nName);` |
|      5 | 2582 | `	ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|      5 | 2583 | `	return PH7_OK;` |
|      1 | 2584 | `}` |
|      4 | 2585 | `static int vm_builtin_ReflectionConstant_getShortName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2586 | `{` |
|      5 | 2587 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      5 | 2588 | `	const char *zName = "";` |
|      5 | 2589 | `	int nName = 0, iCut;` |
|      2 | 2590 | `	SXUNUSED(nArg);` |
|      2 | 2591 | `	SXUNUSED(apArg);` |
|      5 | 2592 | `	if( pThis ){` |
|      5 | 2593 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 | 2594 | `	}` |
|      5 | 2595 | `	iCut = ReflectConstNsCut(zName, nName);` |
|      5 | 2596 | `	if( iCut < 0 ){` |
|      5 | 2597 | `		ph7_result_string(pCtx, zName, nName);` |
|      3 | 2598 | `	}else{` |
|    ! 0 | 2599 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|      - | 2600 | `	}` |
|      5 | 2601 | `	return PH7_OK;` |
|      1 | 2602 | `}` |
|      8 | 2603 | `static int vm_builtin_ReflectionConstant_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2604 | `{` |
|      9 | 2605 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      - | 2606 | `	ph7_value sValue;` |
|      4 | 2607 | `	SXUNUSED(nArg);` |
|      4 | 2608 | `	SXUNUSED(apArg);` |
|      9 | 2609 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|      9 | 2610 | `	if( pCons && pCons->xExpand ){` |
|      9 | 2611 | `		pCons->xExpand(&sValue, pCons->pUserData);` |
|      4 | 2612 | `	}` |
|      9 | 2613 | `	ph7_result_value(pCtx, &sValue);` |
|      9 | 2614 | `	PH7_MemObjRelease(&sValue);` |
|      9 | 2615 | `	return PH7_OK;` |
|      1 | 2616 | `}` |
|      4 | 2617 | `static int vm_builtin_ReflectionConstant_isDeprecated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2618 | `{` |
|      2 | 2619 | `	SXUNUSED(nArg);` |
|      2 | 2620 | `	SXUNUSED(apArg);` |
|      5 | 2621 | `	ph7_result_bool(pCtx, 0);` |
|      5 | 2622 | `	return PH7_OK;` |
|      1 | 2623 | `}` |
|      8 | 2624 | `static int vm_builtin_ReflectionConstant_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2625 | `{` |
|      9 | 2626 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      4 | 2627 | `	SXUNUSED(nArg);` |
|      4 | 2628 | `	SXUNUSED(apArg);` |
|      9 | 2629 | `	if( pCons && SyStringLength(&pCons->sFile) > 0 ){` |
|      5 | 2630 | `		ph7_result_string(pCtx, SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));` |
|      3 | 2631 | `	}else{` |
|      5 | 2632 | `		ph7_result_bool(pCtx, 0);` |
|      - | 2633 | `	}` |
|      9 | 2634 | `	return PH7_OK;` |
|      1 | 2635 | `}` |
|      - | 2636 | `/* An engine constant belongs to the synthetic "Core" extension; a userland` |
|      - | 2637 | ` * define() belongs to none, which php reports as null / false. */` |
|      8 | 2638 | `static int vm_builtin_ReflectionConstant_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2639 | `{` |
|      9 | 2640 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      - | 2641 | `	ph7_class_instance *pExt;` |
|      - | 2642 | `	ph7_value sName, *apArgs[1];` |
|      - | 2643 | `	sxi32 rc;` |
|      4 | 2644 | `	SXUNUSED(nArg);` |
|      4 | 2645 | `	SXUNUSED(apArg);` |
|      9 | 2646 | `	if( pCons == 0 \|\| pCons->bUserDefined ){` |
|      3 | 2647 | `		ph7_result_null(pCtx);` |
|      3 | 2648 | `		return PH7_OK;` |
|      - | 2649 | `	}` |
|      7 | 2650 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|      7 | 2651 | `	ph7_value_string(&sName, "Core", 4);` |
|      7 | 2652 | `	apArgs[0] = &sName;` |
|      7 | 2653 | `	pExt = ReflectConstruct(pCtx, "ReflectionExtension", 1, apArgs, &rc);` |
|      7 | 2654 | `	PH7_MemObjRelease(&sName);` |
|      7 | 2655 | `	if( pExt == 0 ){` |
|    ! 0 | 2656 | `		if( rc != PH7_OK ){` |
|    ! 0 | 2657 | `			return rc;` |
|      - | 2658 | `		}` |
|    ! 0 | 2659 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2660 | `		return PH7_OK;` |
|      - | 2661 | `	}` |
|      7 | 2662 | `	return ReflectResultObject(pCtx, pExt);` |
|      5 | 2663 | `}` |
|      8 | 2664 | `static int vm_builtin_ReflectionConstant_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2665 | `{` |
|      9 | 2666 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      4 | 2667 | `	SXUNUSED(nArg);` |
|      4 | 2668 | `	SXUNUSED(apArg);` |
|      9 | 2669 | `	if( pCons && pCons->bUserDefined == 0 ){` |
|      5 | 2670 | `		ph7_result_string(pCtx, "Core", 4);` |
|      3 | 2671 | `	}else{` |
|      5 | 2672 | `		ph7_result_bool(pCtx, 0);` |
|      - | 2673 | `	}` |
|      9 | 2674 | `	return PH7_OK;` |
|      1 | 2675 | `}` |
|      - | 2676 | `/* getAttributes() still routes through the prelude builder: chunk 7 owns the` |
|      - | 2677 | ` * ReflectionAttribute shape and the lazy argument evaluation. */` |
|      2 | 2678 | `static int vm_builtin_ReflectionConstant_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2679 | `{` |
|      3 | 2680 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      3 | 2681 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|      3 | 2682 | `	const char *zName = "";` |
|      3 | 2683 | `	int nName = 0;` |
|      3 | 2684 | `	if( pThis == 0 \|\| pCons == 0 ){` |
|    ! 0 | 2685 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 | 2686 | `		return PH7_OK;` |
|      - | 2687 | `	}` |
|      3 | 2688 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      - | 2689 | `	{` |
|      - | 2690 | `		ph7_value sTarget;` |
|      - | 2691 | `		int rc;` |
|      3 | 2692 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      3 | 2693 | `		ph7_value_string(&sTarget, zName, nName);` |
|      - | 2694 | `		/* 64 = Attribute::TARGET_CONSTANT */` |
|      4 | 2695 | `		rc = ReflectBuildAttrs(pCtx, &pCons->aAttrs, "const", &sTarget, 0, 0, 0, 64,` |
|      1 | 2696 | `			nArg, apArg);` |
|      3 | 2697 | `		PH7_MemObjRelease(&sTarget);` |
|      3 | 2698 | `		return rc;` |
|      - | 2699 | `	}` |
|      2 | 2700 | `}` |
|    ! 0 | 2701 | `static int vm_builtin_ReflectionConstant_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2702 | `{` |
|    ! 0 | 2703 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    ! 0 | 2704 | `	const char *zName = "";` |
|    ! 0 | 2705 | `	int nName = 0;` |
|    ! 0 | 2706 | `	SXUNUSED(nArg);` |
|    ! 0 | 2707 | `	SXUNUSED(apArg);` |
|    ! 0 | 2708 | `	if( pThis ){` |
|    ! 0 | 2709 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    ! 0 | 2710 | `	}` |
|    ! 0 | 2711 | `	ph7_result_string_format(pCtx, "Constant [ %.*s ]\n", nName, zName);` |
|    ! 0 | 2712 | `	return PH7_OK;` |
|    ! 0 | 2713 | `}` |
|      - | 2714 | `/* ---- ReflectionExtension: PHL has exactly one, the synthetic "Core" ---- */` |
|     24 | 2715 | `static int vm_builtin_ReflectionExtension_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2716 | `{` |
|     25 | 2717 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     25 | 2718 | `	int nName = 0;` |
|     25 | 2719 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|     25 | 2720 | `	if( pThis == 0 ){` |
|    ! 0 | 2721 | `		return PH7_OK;` |
|      - | 2722 | `	}` |
|     34 | 2723 | `	if( !(nName == 4 && SyToLower(zName[0]) == 'c' && SyToLower(zName[1]) == 'o'` |
|     18 | 2724 | `	   && SyToLower(zName[2]) == 'r' && SyToLower(zName[3]) == 'e') ){` |
|     10 | 2725 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 | 2726 | `			"Extension \"%.*s\" does not exist", nName, zName);` |
|      - | 2727 | `	}` |
|     19 | 2728 | `	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", "Core", 4);` |
|     19 | 2729 | `	return PH7_OK;` |
|     13 | 2730 | `}` |
|     10 | 2731 | `static int vm_builtin_ReflectionExtension_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2732 | `{` |
|     11 | 2733 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     11 | 2734 | `	const char *zName = "";` |
|     11 | 2735 | `	int nName = 0;` |
|      5 | 2736 | `	SXUNUSED(nArg);` |
|      5 | 2737 | `	SXUNUSED(apArg);` |
|     11 | 2738 | `	if( pThis ){` |
|     11 | 2739 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      5 | 2740 | `	}` |
|     11 | 2741 | `	ph7_result_string(pCtx, zName, nName);` |
|     11 | 2742 | `	return PH7_OK;` |
|      1 | 2743 | `}` |
|    ! 0 | 2744 | `static int vm_builtin_ReflectionExtension_getVersion(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2745 | `{` |
|      - | 2746 | `	ph7_value sFn, sRes;` |
|      - | 2747 | `	SyString sStr;` |
|    ! 0 | 2748 | `	SXUNUSED(nArg);` |
|    ! 0 | 2749 | `	SXUNUSED(apArg);` |
|    ! 0 | 2750 | `	PH7_MemObjInit(pCtx->pVm, &sFn);` |
|    ! 0 | 2751 | `	PH7_MemObjInit(pCtx->pVm, &sRes);` |
|    ! 0 | 2752 | `	SyStringInitFromBuf(&sStr, "phpversion", sizeof("phpversion")-1);` |
|    ! 0 | 2753 | `	PH7_MemObjInitFromString(pCtx->pVm, &sFn, &sStr);` |
|    ! 0 | 2754 | `	if( PH7_VmCallUserFunction(pCtx->pVm, &sFn, 0, 0, &sRes) == SXRET_OK ){` |
|    ! 0 | 2755 | `		ph7_result_value(pCtx, &sRes);` |
|    ! 0 | 2756 | `	}` |
|    ! 0 | 2757 | `	PH7_MemObjRelease(&sFn);` |
|    ! 0 | 2758 | `	PH7_MemObjRelease(&sRes);` |
|    ! 0 | 2759 | `	return PH7_OK;` |
|    ! 0 | 2760 | `}` |
|    ! 0 | 2761 | `static int vm_builtin_ReflectionExtension_emptyArray(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2762 | `{` |
|    ! 0 | 2763 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|    ! 0 | 2764 | `	SXUNUSED(nArg);` |
|    ! 0 | 2765 | `	SXUNUSED(apArg);` |
|    ! 0 | 2766 | `	if( pList ){` |
|    ! 0 | 2767 | `		ph7_result_value(pCtx, pList);` |
|    ! 0 | 2768 | `	}` |
|    ! 0 | 2769 | `	return PH7_OK;` |
|    ! 0 | 2770 | `}` |
|    ! 0 | 2771 | `static int vm_builtin_ReflectionExtension_isPersistent(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2772 | `{` |
|    ! 0 | 2773 | `	SXUNUSED(nArg);` |
|    ! 0 | 2774 | `	SXUNUSED(apArg);` |
|    ! 0 | 2775 | `	ph7_result_bool(pCtx, 1);` |
|    ! 0 | 2776 | `	return PH7_OK;` |
|    ! 0 | 2777 | `}` |
|    ! 0 | 2778 | `static int vm_builtin_ReflectionExtension_isTemporary(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2779 | `{` |
|    ! 0 | 2780 | `	SXUNUSED(nArg);` |
|    ! 0 | 2781 | `	SXUNUSED(apArg);` |
|    ! 0 | 2782 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 | 2783 | `	return PH7_OK;` |
|    ! 0 | 2784 | `}` |
|    ! 0 | 2785 | `static int vm_builtin_ReflectionExtension_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2786 | `{` |
|    ! 0 | 2787 | `	SXUNUSED(nArg);` |
|    ! 0 | 2788 | `	SXUNUSED(apArg);` |
|    ! 0 | 2789 | `	SXUNUSED(pCtx);` |
|    ! 0 | 2790 | `	return PH7_OK;` |
|    ! 0 | 2791 | `}` |
|    ! 0 | 2792 | `static int vm_builtin_ReflectionExtension_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2793 | `{` |
|    ! 0 | 2794 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    ! 0 | 2795 | `	const char *zName = "";` |
|    ! 0 | 2796 | `	int nName = 0;` |
|    ! 0 | 2797 | `	SXUNUSED(nArg);` |
|    ! 0 | 2798 | `	SXUNUSED(apArg);` |
|    ! 0 | 2799 | `	if( pThis ){` |
|    ! 0 | 2800 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    ! 0 | 2801 | `	}` |
|    ! 0 | 2802 | `	ph7_result_string_format(pCtx, "Extension [ extension #1 %.*s ]\n", nName, zName);` |
|    ! 0 | 2803 | `	return PH7_OK;` |
|    ! 0 | 2804 | `}` |
|      - | 2805 | `/* ---- ReflectionZendExtension: none exist, so the constructor always refuses ---- */` |
|      4 | 2806 | `static int vm_builtin_ReflectionZendExtension_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2807 | `{` |
|      5 | 2808 | `	int nName = 0;` |
|      5 | 2809 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|      7 | 2810 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 | 2811 | `		"Zend Extension \"%.*s\" does not exist", nName, zName);` |
|      1 | 2812 | `}` |
|    ! 0 | 2813 | `static int vm_builtin_ReflectionZendExtension_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2814 | `{` |
|    ! 0 | 2815 | `	SXUNUSED(nArg);` |
|    ! 0 | 2816 | `	SXUNUSED(apArg);` |
|    ! 0 | 2817 | `	ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 2818 | `	return PH7_OK;` |
|    ! 0 | 2819 | `}` |
|      - | 2820 | `/* ---- ReflectionReference ---- */` |
|      - | 2821 | `/*` |
|      - | 2822 | ` * ReflectionReference::fromArrayElement(array $array, string\|int $key)` |
|      - | 2823 | ` *` |
|      - | 2824 | ` * Answers a reflector only when the element IS a reference — its slot carries a` |
|      - | 2825 | ` * reference-table record with at least two links. The instance is built without` |
|      - | 2826 | ` * running the (private) constructor, exactly as php's factory does.` |
|      - | 2827 | ` */` |
|     18 | 2828 | `static int vm_builtin_ReflectionReference_fromArrayElement(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2829 | `{` |
|     19 | 2830 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 2831 | `	ph7_hashmap *pMap;` |
|     19 | 2832 | `	ph7_hashmap_node *pNode = 0;` |
|      - | 2833 | `	ph7_class *pClass;` |
|      - | 2834 | `	ph7_class_instance *pObj;` |
|      - | 2835 | `	char zId[64];` |
|     19 | 2836 | `	if( nArg < 1 ){` |
|    ! 0 | 2837 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2838 | `		return PH7_OK;` |
|      - | 2839 | `	}` |
|     19 | 2840 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|      - | 2841 | ``		/* The shared ZPP screen leaves a SCALAR against a bare `array` parameter`` |
|      - | 2842 | `		 * unscreened (it only judges array/object/null/resource pairings and` |
|      - | 2843 | `		 * scalars against class-typed ones), so the refusal php words from the` |
|      - | 2844 | `		 * declared type is written here -- as the prelude's own is_array() check` |
|      - | 2845 | `		 * did. Recorded in PLAN §2 with the rest of that gap. */` |
|    ! 0 | 2846 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|      - | 2847 | `			"ReflectionReference::fromArrayElement(): Argument #1 ($array) "` |
|    ! 0 | 2848 | `			"must be of type array, %s given", ph7_type_name(apArg[0]));` |
|      - | 2849 | `	}` |
|     19 | 2850 | `	if( nArg < 2 ){` |
|    ! 0 | 2851 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2852 | `		return PH7_OK;` |
|      - | 2853 | `	}` |
|     19 | 2854 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     19 | 2855 | `	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK \|\| pNode == 0 ){` |
|    ! 0 | 2856 | `		ph7_result_null(pCtx);` |
|    ! 0 | 2857 | `		return PH7_OK;` |
|      - | 2858 | `	}` |
|     19 | 2859 | `	if( PH7_VmSlotRefCount(pVm, pNode->nValIdx) < 2 ){` |
|      7 | 2860 | `		ph7_result_null(pCtx);` |
|      7 | 2861 | `		return PH7_OK;` |
|      - | 2862 | `	}` |
|     13 | 2863 | `	pClass = PH7_VmExtractClass(pVm, "ReflectionReference", sizeof("ReflectionReference")-1, FALSE, 0);` |
|     13 | 2864 | `	pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|     13 | 2865 | `	if( pObj == 0 ){` |
|    ! 0 | 2866 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 2867 | `	}` |
|      - | 2868 | `	/* php's ids are opaque strings; the slot index is the identity PHL has. */` |
|     13 | 2869 | `	SyBufferFormat(zId, sizeof(zId), "phlref%u", (unsigned)pNode->nValIdx);` |
|     13 | 2870 | `	PH7_NativeSetAttrStr(pVm, pObj, RR_ID, zId, (int)SyStrlen(zId));` |
|     13 | 2871 | `	return ReflectResultObject(pCtx, pObj);` |
|     10 | 2872 | `}` |
|     12 | 2873 | `static int vm_builtin_ReflectionReference_getId(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 2874 | `{` |
|     13 | 2875 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     13 | 2876 | `	const char *zId = "";` |
|     13 | 2877 | `	int nId = 0;` |
|      6 | 2878 | `	SXUNUSED(nArg);` |
|      6 | 2879 | `	SXUNUSED(apArg);` |
|     13 | 2880 | `	if( pThis ){` |
|     13 | 2881 | `		PH7_NativeAttrStr(pThis, RR_ID, &zId, &nId);` |
|      6 | 2882 | `	}` |
|     13 | 2883 | `	ph7_result_string(pCtx, zId, nId);` |
|     13 | 2884 | `	return PH7_OK;` |
|      1 | 2885 | `}` |
|      - | 2886 | `/* php declares this private and never calls it; reaching it is the diagnostic. */` |
|    ! 0 | 2887 | `static int vm_builtin_ReflectionReference_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 2888 | `{` |
|    ! 0 | 2889 | `	SXUNUSED(nArg);` |
|    ! 0 | 2890 | `	SXUNUSED(apArg);` |
|    ! 0 | 2891 | `	SXUNUSED(pCtx);` |
|    ! 0 | 2892 | `	return PH7_OK;` |
|    ! 0 | 2893 | `}` |
|      - | 2894 | `/*` |
|      - | 2895 | ` * Declare the six. Called from PH7_VmInstallReflection() where chunk 5 used to` |
|      - | 2896 | ` * be compiled, so Reflector (chunk 1) already exists.` |
|      - | 2897 | ` */` |
|   5254 | 2898 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionSmall(ph7_vm *pVm)` |
|      5 | 2899 | `{` |
|      - | 2900 | `	static const PH7_NativePropDef aGenProp[] = {` |
|      - | 2901 | `		{ RG_GEN, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 2902 | `	};` |
|      - | 2903 | `	static const PH7_NativeMethodDef aGenMethod[] = {` |
|      - | 2904 | `		{ "__construct",           PH7_MOD_PUBLIC, "Generator $generator", "",` |
|      - | 2905 | `		  vm_builtin_ReflectionGenerator_construct },` |
|      - | 2906 | `		{ "getFunction",           PH7_MOD_PUBLIC, "", "ReflectionFunctionAbstract",` |
|      - | 2907 | `		  vm_builtin_ReflectionGenerator_getFunction },` |
|      - | 2908 | `		{ "getThis",               PH7_MOD_PUBLIC, "", "?object", vm_builtin_ReflectionGenerator_getThis },` |
|      - | 2909 | `		{ "getExecutingGenerator", PH7_MOD_PUBLIC, "", "Generator",` |
|      - | 2910 | `		  vm_builtin_ReflectionGenerator_getExecuting },` |
|      - | 2911 | `		{ "isClosed",              PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionGenerator_isClosed },` |
|      - | 2912 | `		{ "getExecutingLine",      PH7_MOD_PUBLIC, "", "int", vm_builtin_ReflectionGenerator_execLine },` |
|      - | 2913 | `		{ "getExecutingFile",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionGenerator_execFile },` |
|      - | 2914 | `		{ "getTrace",              PH7_MOD_PUBLIC, "int $options = 1", "array",` |
|      - | 2915 | `		  vm_builtin_ReflectionGenerator_trace },` |
|      - | 2916 | `	};` |
|      - | 2917 | `	static const PH7_NativePropDef aFiberProp[] = {` |
|      - | 2918 | `		{ RF_FIBER, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 2919 | `	};` |
|      - | 2920 | `	static const PH7_NativeMethodDef aFiberMethod[] = {` |
|      - | 2921 | `		{ "__construct",      PH7_MOD_PUBLIC, "Fiber $fiber", "", vm_builtin_ReflectionFiber_construct },` |
|      - | 2922 | `		{ "getFiber",         PH7_MOD_PUBLIC, "", "Fiber",    vm_builtin_ReflectionFiber_getFiber },` |
|      - | 2923 | `		{ "getCallable",      PH7_MOD_PUBLIC, "", "callable", vm_builtin_ReflectionFiber_getCallable },` |
|      - | 2924 | `		{ "getExecutingLine", PH7_MOD_PUBLIC, "", "?int",     vm_builtin_ReflectionFiber_execLine },` |
|      - | 2925 | `		{ "getExecutingFile", PH7_MOD_PUBLIC, "", "?string",  vm_builtin_ReflectionFiber_execFile },` |
|      - | 2926 | `		{ "getTrace",         PH7_MOD_PUBLIC, "int $options = 1", "array", vm_builtin_ReflectionFiber_trace },` |
|      - | 2927 | `	};` |
|      - | 2928 | `	static const PH7_NativePropDef aNameProp[] = {` |
|      - | 2929 | `		{ "name", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 2930 | `	};` |
|      - | 2931 | `	static const PH7_NativeMethodDef aConstMethod[] = {` |
|      - | 2932 | `		{ "__construct",       PH7_MOD_PUBLIC, "string $name", "",` |
|      - | 2933 | `		  vm_builtin_ReflectionConstant_construct },` |
|      - | 2934 | `		{ "getName",           PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_getName },` |
|      - | 2935 | `		{ "getNamespaceName",  PH7_MOD_PUBLIC, "", "string",` |
|      - | 2936 | `		  vm_builtin_ReflectionConstant_getNamespaceName },` |
|      - | 2937 | `		{ "getShortName",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_getShortName },` |
|      - | 2938 | `		{ "getValue",          PH7_MOD_PUBLIC, "", "mixed",  vm_builtin_ReflectionConstant_getValue },` |
|      - | 2939 | `		{ "isDeprecated",      PH7_MOD_PUBLIC, "", "bool",   vm_builtin_ReflectionConstant_isDeprecated },` |
|      - | 2940 | `		{ "getFileName",       PH7_MOD_PUBLIC, "", "string\|false",` |
|      - | 2941 | `		  vm_builtin_ReflectionConstant_getFileName },` |
|      - | 2942 | `		{ "getExtension",      PH7_MOD_PUBLIC, "", "?ReflectionExtension",` |
|      - | 2943 | `		  vm_builtin_ReflectionConstant_getExtension },` |
|      - | 2944 | `		{ "getExtensionName",  PH7_MOD_PUBLIC, "", "string\|false",` |
|      - | 2945 | `		  vm_builtin_ReflectionConstant_getExtensionName },` |
|      - | 2946 | `		{ "getAttributes",     PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - | 2947 | `		  vm_builtin_ReflectionConstant_getAttributes },` |
|      - | 2948 | `		{ "__toString",        PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_toString },` |
|      - | 2949 | `	};` |
|      - | 2950 | `	static const PH7_NativeMethodDef aExtMethod[] = {` |
|      - | 2951 | `		{ "__construct",     PH7_MOD_PUBLIC, "string $name", "", vm_builtin_ReflectionExtension_construct },` |
|      - | 2952 | `		{ "getName",         PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionExtension_getName },` |
|      - | 2953 | `		{ "getVersion",      PH7_MOD_PUBLIC, "", "@?string", vm_builtin_ReflectionExtension_getVersion },` |
|      - | 2954 | `		{ "getFunctions",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|      - | 2955 | `		{ "getConstants",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|      - | 2956 | `		{ "getINIEntries",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|      - | 2957 | `		{ "getClasses",      PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|      - | 2958 | `		{ "getClassNames",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|      - | 2959 | `		{ "getDependencies", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|      - | 2960 | `		{ "info",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_ReflectionExtension_info },` |
|      - | 2961 | `		{ "isPersistent",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionExtension_isPersistent },` |
|      - | 2962 | `		{ "isTemporary",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionExtension_isTemporary },` |
|      - | 2963 | `		{ "__toString",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionExtension_toString },` |
|      - | 2964 | `	};` |
|      - | 2965 | `	static const PH7_NativeMethodDef aZendMethod[] = {` |
|      - | 2966 | `		{ "__construct", PH7_MOD_PUBLIC, "string $name", "",` |
|      - | 2967 | `		  vm_builtin_ReflectionZendExtension_construct },` |
|      - | 2968 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionExtension_getName },` |
|      - | 2969 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionZendExtension_toString },` |
|      - | 2970 | `	};` |
|      - | 2971 | `	static const PH7_NativePropDef aRefProp[] = {` |
|      - | 2972 | `		{ RR_ID, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|      - | 2973 | `	};` |
|      - | 2974 | `	static const PH7_NativeMethodDef aRefMethod[] = {` |
|      - | 2975 | ``		/* php declares the constructor PRIVATE, so `new ReflectionReference` is`` |
|      - | 2976 | `		 * the engine's own "Call to private ... from global scope" Error rather` |
|      - | 2977 | `		 * than a throw the prelude had to write by hand. */` |
|      - | 2978 | `		{ "__construct",      PH7_MOD_PRIVATE, "", "", vm_builtin_ReflectionReference_construct },` |
|      - | 2979 | `		{ "fromArrayElement", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array, string\|int $key",` |
|      - | 2980 | `		  "?ReflectionReference", vm_builtin_ReflectionReference_fromArrayElement },` |
|      - | 2981 | `		{ "getId",            PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionReference_getId },` |
|      - | 2982 | `	};` |
|      - | 2983 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 2984 | `		{ "ReflectionGenerator", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 2985 | `		  aGenMethod, SX_ARRAYSIZE(aGenMethod), 0, 0, aGenProp, SX_ARRAYSIZE(aGenProp), 0, 0, 0 },` |
|      - | 2986 | `		{ "ReflectionFiber", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 2987 | `		  aFiberMethod, SX_ARRAYSIZE(aFiberMethod), 0, 0, aFiberProp, SX_ARRAYSIZE(aFiberProp), 0, 0, 0 },` |
|      - | 2988 | `		{ "ReflectionConstant", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 2989 | `		  aConstMethod, SX_ARRAYSIZE(aConstMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|      - | 2990 | `		{ "ReflectionExtension", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 2991 | `		  aExtMethod, SX_ARRAYSIZE(aExtMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|      - | 2992 | `		{ "ReflectionZendExtension", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 2993 | `		  aZendMethod, SX_ARRAYSIZE(aZendMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|      - | 2994 | `		{ "ReflectionReference", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 2995 | `		  aRefMethod, SX_ARRAYSIZE(aRefMethod), 0, 0, aRefProp, SX_ARRAYSIZE(aRefProp), 0, 0, 0 },` |
|      - | 2996 | `	};` |
|   5259 | 2997 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 | 2998 | `}` |
|      - | 2999 | `/*` |
|      - | 3000 | ` * ---------------------------------------------------------------------------` |
|      - | 3001 | ` * Reflector, Reflection, ReflectionException, ReflectionClass, ReflectionObject.` |
|      - | 3002 | ` *` |
|      - | 3003 | ` * Chunk 1 — the core of the whole API. Its ~350 lines of PHP funnelled every` |
|      - | 3004 | ` * accessor through __phl_rcinfo(), a memoized descriptor ARRAY built by a C` |
|      - | 3005 | ` * thunk: sixty methods that each rebuilt or re-read a marshalled copy of state` |
|      - | 3006 | ` * the engine was already holding. A native method reads ph7_class directly, so` |
|      - | 3007 | ` * the descriptor is gone from this path entirely and the memo it needed with it.` |
|      - | 3008 | ` *` |
|      - | 3009 | ` * What stays behind is the prelude that has not moved yet, and these classes` |
|      - | 3010 | ` * still reach it by name where php's own object graph does: getMethod() builds` |
|      - | 3011 | ` * a ReflectionMethod, getProperty() a ReflectionProperty, getAttributes() calls` |
|      - | 3012 | ` * __reflect_build_attrs, __toString() calls __reflect_export_class. Those become` |
|      - | 3013 | ` * direct C the moment chunks 2, 3, 7 and 9 land.` |
|      - | 3014 | ` * ---------------------------------------------------------------------------` |
|      - | 3015 | ` */` |
|      - | 3016 | `#define RC_OBJ "__obj"` |
|      - | 3017 |  |
|      - | 3018 | ``/* The class a ReflectionClass reflects: its `name` slot, resolved. The`` |
|      - | 3019 | ` * constructor already stored the canonical name, so no autoload can be needed` |
|      - | 3020 | ` * here. */` |
|    812 | 3021 | `static ph7_class * ReflectClassOf(ph7_context *pCtx)` |
|      5 | 3022 | `{` |
|    817 | 3023 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 3024 | `	const char *zName;` |
|      - | 3025 | `	int nName;` |
|    817 | 3026 | `	if( pThis == 0 ){` |
|    ! 0 | 3027 | `		return 0;` |
|      - | 3028 | `	}` |
|    817 | 3029 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    817 | 3030 | `	if( nName < 1 ){` |
|    ! 0 | 3031 | `		return 0;` |
|      - | 3032 | `	}` |
|      - | 3033 | `	/* PH7_NativeAttrStr borrows bytes that are NOT NUL-terminated: the length` |
|      - | 3034 | `	 * has to travel with them. */` |
|    817 | 3035 | `	return PH7_VmExtractClass(pCtx->pVm, zName, (sxu32)nName, FALSE, 0);` |
|    411 | 3036 | `}` |
|      - | 3037 | `/* The instance a ReflectionObject was built over, or NULL for a plain` |
|      - | 3038 | ` * ReflectionClass — what makes DYNAMIC properties visible. */` |
|    138 | 3039 | `static ph7_class_instance * ReflectClassObj(ph7_context *pCtx)` |
|      3 | 3040 | `{` |
|    141 | 3041 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    141 | 3042 | `	return pThis ? PH7_NativeAttrObj(pThis, RC_OBJ) : 0;` |
|      3 | 3043 | `}` |
|      - | 3044 | `/* $this->name as bytes. */` |
|    478 | 3045 | `static void ReflectClassName(ph7_context *pCtx, const char **pzOut, int *pnOut)` |
|      5 | 3046 | `{` |
|    483 | 3047 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    483 | 3048 | `	*pzOut = "";` |
|    483 | 3049 | `	*pnOut = 0;` |
|    483 | 3050 | `	if( pThis ){` |
|    483 | 3051 | `		PH7_NativeAttrStr(pThis, "name", pzOut, pnOut);` |
|    239 | 3052 | `	}` |
|    483 | 3053 | `}` |
|      - | 3054 | `/* Answer the truth of one of the reflected class's iFlags bits. */` |
|     90 | 3055 | `static int ReflectClassFlag(ph7_context *pCtx, sxi32 iFlag)` |
|      2 | 3056 | `{` |
|     92 | 3057 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     92 | 3058 | `	return pClass != 0 && (pClass->iFlags & iFlag) != 0;` |
|      2 | 3059 | `}` |
|      - | 3060 | `#define REFLECT_CLASS_FLAG(NAME,FLAG,NEGATE) \` |
|      - | 3061 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 3062 | `	{ \` |
|      - | 3063 | `		SXUNUSED(nArg); \` |
|      - | 3064 | `		SXUNUSED(apArg); \` |
|      - | 3065 | `		ph7_result_bool(pCtx, NEGATE ? !ReflectClassFlag(pCtx,FLAG) : ReflectClassFlag(pCtx,FLAG)); \` |
|      - | 3066 | `		return PH7_OK; \` |
|      - | 3067 | `	}` |
|     13 | 3068 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isInternal,    PH7_CLASS_INTERNAL,  0)` |
|      5 | 3069 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isUserDefined, PH7_CLASS_INTERNAL,  1)` |
|      9 | 3070 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isInterface,   PH7_CLASS_INTERFACE, 0)` |
|      5 | 3071 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isTrait,       PH7_CLASS_TRAIT,     0)` |
|     13 | 3072 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isAbstract,    PH7_CLASS_ABSTRACT,  0)` |
|     38 | 3073 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isFinal,       PH7_CLASS_FINAL,     0)` |
|    ! 0 | 3074 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isReadOnly,    PH7_CLASS_READONLY,  0)` |
|     11 | 3075 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isEnum,        PH7_CLASS_ENUM,      0)` |
|      - | 3076 |  |
|      - | 3077 | ``/* Build a ReflectionClass over pTarget with its `name` already filled in: the`` |
|      - | 3078 | ` * constructor would only re-resolve a class this code is holding. */` |
|    232 | 3079 | `static int ReflectResultClassOf(ph7_context *pCtx, ph7_class *pTarget)` |
|      4 | 3080 | `{` |
|    236 | 3081 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 3082 | `	ph7_class *pRC;` |
|      - | 3083 | `	ph7_class_instance *pObj;` |
|    236 | 3084 | `	if( pTarget == 0 ){` |
|    ! 0 | 3085 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3086 | `		return PH7_OK;` |
|      - | 3087 | `	}` |
|    236 | 3088 | `	pRC = PH7_VmExtractClass(pVm, "ReflectionClass", sizeof("ReflectionClass")-1, FALSE, 0);` |
|    236 | 3089 | `	if( pRC == 0 \|\| (pObj = PH7_NewClassInstance(pVm, pRC)) == 0 ){` |
|    ! 0 | 3090 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3091 | `		return PH7_OK;` |
|      - | 3092 | `	}` |
|    352 | 3093 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(&pTarget->sName),` |
|    232 | 3094 | `		(int)SyStringLength(&pTarget->sName));` |
|    236 | 3095 | `	PH7_NativeResultObject(pCtx, pObj);` |
|    236 | 3096 | `	return PH7_OK;` |
|    120 | 3097 | `}` |
|      - | 3098 | ``/* The class an argument declared `ReflectionClass\|string` denotes: a reflector's`` |
|      - | 3099 | `` * `name` slot, or the string itself. NULL when it names nothing (after`` |
|      - | 3100 | ` * autoload). */` |
|     22 | 3101 | `static ph7_class * ReflectClassArg(ph7_context *pCtx, ph7_value *pArg)` |
|      1 | 3102 | `{` |
|     23 | 3103 | `	ph7_vm *pVm = pCtx->pVm;` |
|     23 | 3104 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|    ! 0 | 3105 | `		ph7_class_instance *pObj = (ph7_class_instance *)pArg->x.pOther;` |
|    ! 0 | 3106 | `		ph7_class *pRC = PH7_VmExtractClass(pVm, "ReflectionClass", sizeof("ReflectionClass")-1, FALSE, 0);` |
|    ! 0 | 3107 | `		if( pRC && PH7_VmInstanceOf(pObj->pClass, pRC) ){` |
|      - | 3108 | `			const char *zName;` |
|      - | 3109 | `			int nName;` |
|    ! 0 | 3110 | `			PH7_NativeAttrStr(pObj, "name", &zName, &nName);` |
|    ! 0 | 3111 | `			return nName > 0 ? PH7_VmExtractClass(pVm, zName, (sxu32)nName, FALSE, 0) : 0;` |
|      - | 3112 | `		}` |
|    ! 0 | 3113 | `	}` |
|     23 | 3114 | `	return ReflectResolveClass(pVm, pArg);` |
|     12 | 3115 | `}` |
|      - | 3116 | `/* ---- constructors ---- */` |
|      - | 3117 | `/* ReflectionClass::__construct(object\|string $objectOrClass) */` |
|    454 | 3118 | `static int vm_builtin_ReflectionClass_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 | 3119 | `{` |
|    459 | 3120 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 3121 | `	ph7_class *pClass;` |
|    459 | 3122 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 3123 | `		return PH7_OK;` |
|      - | 3124 | `	}` |
|    459 | 3125 | `	pClass = ReflectResolveClass(pCtx->pVm, apArg[0]);` |
|    459 | 3126 | `	if( pClass == 0 ){` |
|      - | 3127 | `		/* php reports the NAME it was handed, after the declared object\|string` |
|      - | 3128 | ``		 * has coerced a scalar — `new ReflectionClass(1.5)` says Class "1.5". */`` |
|      - | 3129 | `		const char *zName;` |
|      - | 3130 | `		int nName;` |
|     12 | 3131 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|     17 | 3132 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      5 | 3133 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      - | 3134 | `	}` |
|    671 | 3135 | `	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", SyStringData(&pClass->sName),` |
|    444 | 3136 | `		(int)SyStringLength(&pClass->sName));` |
|    449 | 3137 | `	return PH7_OK;` |
|    232 | 3138 | `}` |
|      - | 3139 | `/* ReflectionObject::__construct(object $object) — the same, plus the receiver` |
|      - | 3140 | ` * whose DYNAMIC properties the inherited accessors then report. */` |
|      6 | 3141 | `static int vm_builtin_ReflectionObject_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3142 | `{` |
|      7 | 3143 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 3144 | `	sxi32 rc;` |
|      7 | 3145 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 | 3146 | `		return PH7_OK;` |
|      - | 3147 | `	}` |
|      7 | 3148 | `	rc = vm_builtin_ReflectionClass_construct(pCtx, nArg, apArg);` |
|      7 | 3149 | `	if( rc != PH7_OK ){` |
|    ! 0 | 3150 | `		return rc;` |
|      - | 3151 | `	}` |
|      7 | 3152 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RC_OBJ, (ph7_class_instance *)apArg[0]->x.pOther);` |
|      7 | 3153 | `	return PH7_OK;` |
|      4 | 3154 | `}` |
|      - | 3155 | `/* ReflectionClass::__clone(): void — php declares it private, and the class is` |
|      - | 3156 | ` * uncloneable besides (PH7_CLASS_NOCLONE answers before any body runs). */` |
|    ! 0 | 3157 | `static int vm_builtin_ReflectionClass_clone(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 3158 | `{` |
|    ! 0 | 3159 | `	SXUNUSED(pCtx);` |
|    ! 0 | 3160 | `	SXUNUSED(nArg);` |
|    ! 0 | 3161 | `	SXUNUSED(apArg);` |
|    ! 0 | 3162 | `	return PH7_OK;` |
|    ! 0 | 3163 | `}` |
|      - | 3164 | `/* ---- name ---- */` |
|    242 | 3165 | `static int vm_builtin_ReflectionClass_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 | 3166 | `{` |
|      - | 3167 | `	const char *zName;` |
|      - | 3168 | `	int nName;` |
|    121 | 3169 | `	SXUNUSED(nArg);` |
|    121 | 3170 | `	SXUNUSED(apArg);` |
|    246 | 3171 | `	ReflectClassName(pCtx, &zName, &nName);` |
|    246 | 3172 | `	ph7_result_string(pCtx, zName, nName);` |
|    246 | 3173 | `	return PH7_OK;` |
|      4 | 3174 | `}` |
|      - | 3175 | `/* Offset just past the last namespace separator, or -1 when there is none. */` |
|      6 | 3176 | `static int ReflectNsCut(const char *zName, int nName)` |
|      1 | 3177 | `{` |
|      - | 3178 | `	int i;` |
|     91 | 3179 | `	for( i = nName - 1 ; i >= 0 ; i-- ){` |
|     85 | 3180 | `		if( zName[i] == '\\' ){` |
|    ! 0 | 3181 | `			return i;` |
|      - | 3182 | `		}` |
|     43 | 3183 | `	}` |
|      7 | 3184 | `	return -1;` |
|      4 | 3185 | `}` |
|      2 | 3186 | `static int vm_builtin_ReflectionClass_getShortName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3187 | `{` |
|      - | 3188 | `	const char *zName;` |
|      - | 3189 | `	int nName, iCut;` |
|      1 | 3190 | `	SXUNUSED(nArg);` |
|      1 | 3191 | `	SXUNUSED(apArg);` |
|      3 | 3192 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 | 3193 | `	iCut = ReflectNsCut(zName, nName);` |
|      3 | 3194 | `	if( iCut < 0 ){` |
|      3 | 3195 | `		ph7_result_string(pCtx, zName, nName);` |
|      2 | 3196 | `	}else{` |
|    ! 0 | 3197 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|      - | 3198 | `	}` |
|      3 | 3199 | `	return PH7_OK;` |
|      1 | 3200 | `}` |
|      2 | 3201 | `static int vm_builtin_ReflectionClass_getNamespaceName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3202 | `{` |
|      - | 3203 | `	const char *zName;` |
|      - | 3204 | `	int nName, iCut;` |
|      1 | 3205 | `	SXUNUSED(nArg);` |
|      1 | 3206 | `	SXUNUSED(apArg);` |
|      3 | 3207 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 | 3208 | `	iCut = ReflectNsCut(zName, nName);` |
|      3 | 3209 | `	ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|      3 | 3210 | `	return PH7_OK;` |
|      1 | 3211 | `}` |
|      2 | 3212 | `static int vm_builtin_ReflectionClass_inNamespace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3213 | `{` |
|      - | 3214 | `	const char *zName;` |
|      - | 3215 | `	int nName;` |
|      1 | 3216 | `	SXUNUSED(nArg);` |
|      1 | 3217 | `	SXUNUSED(apArg);` |
|      3 | 3218 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 | 3219 | `	ph7_result_bool(pCtx, ReflectNsCut(zName, nName) >= 0);` |
|      3 | 3220 | `	return PH7_OK;` |
|      1 | 3221 | `}` |
|      2 | 3222 | `static int vm_builtin_ReflectionClass_isAnonymous(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3223 | `{` |
|      - | 3224 | `	const char *zName;` |
|      - | 3225 | `	int nName;` |
|      - | 3226 | `	static const char zAnon[] = "class@anonymous";` |
|      1 | 3227 | `	SXUNUSED(nArg);` |
|      1 | 3228 | `	SXUNUSED(apArg);` |
|      3 | 3229 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      3 | 3230 | `	ph7_result_bool(pCtx, nName >= (int)sizeof(zAnon)-1` |
|      1 | 3231 | `		&& SyMemcmp(zName, zAnon, sizeof(zAnon)-1) == 0);` |
|      3 | 3232 | `	return PH7_OK;` |
|      1 | 3233 | `}` |
|      - | 3234 | `/* ---- shape ---- */` |
|      8 | 3235 | `static int vm_builtin_ReflectionClass_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3236 | `{` |
|      9 | 3237 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      9 | 3238 | `	sxi64 iMods = 0;` |
|      4 | 3239 | `	SXUNUSED(nArg);` |
|      4 | 3240 | `	SXUNUSED(apArg);` |
|      9 | 3241 | `	if( pClass ){` |
|      9 | 3242 | `		if( pClass->iFlags & PH7_CLASS_ABSTRACT ){ iMods \|= 64; }` |
|      9 | 3243 | `		if( pClass->iFlags & PH7_CLASS_FINAL ){ iMods \|= 32; }` |
|      9 | 3244 | `		if( pClass->iFlags & PH7_CLASS_READONLY ){ iMods \|= 65536; }` |
|      4 | 3245 | `	}` |
|      9 | 3246 | `	ph7_result_int64(pCtx, iMods);` |
|      9 | 3247 | `	return PH7_OK;` |
|      1 | 3248 | `}` |
|     28 | 3249 | `static int vm_builtin_ReflectionClass_getParentClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 3250 | `{` |
|     31 | 3251 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     14 | 3252 | `	SXUNUSED(nArg);` |
|     14 | 3253 | `	SXUNUSED(apArg);` |
|     31 | 3254 | `	if( pClass == 0 \|\| pClass->pBase == 0 ){` |
|     11 | 3255 | `		ph7_result_bool(pCtx, 0);` |
|     11 | 3256 | `		return PH7_OK;` |
|      - | 3257 | `	}` |
|     23 | 3258 | `	return ReflectResultClassOf(pCtx, pClass->pBase);` |
|     17 | 3259 | `}` |
|      - | 3260 | `/* getInterfaceNames()/getInterfaces()/getTraitNames()/getTraits() — one walk,` |
|      - | 3261 | ` * four shapes. bReflector picks name-list vs {name: ReflectionClass}. */` |
|     60 | 3262 | `static int ReflectClassNameList(ph7_context *pCtx, int bTraits, int bReflector)` |
|      2 | 3263 | `{` |
|     62 | 3264 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     62 | 3265 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|      - | 3266 | `	SySet aSet;` |
|      - | 3267 | `	ph7_class **apOut;` |
|      - | 3268 | `	sxu32 n, nOut;` |
|     62 | 3269 | `	if( pList == 0 ){` |
|    ! 0 | 3270 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3271 | `	}` |
|     62 | 3272 | `	if( pClass == 0 ){` |
|    ! 0 | 3273 | `		ph7_result_value(pCtx, pList);` |
|    ! 0 | 3274 | `		return PH7_OK;` |
|      - | 3275 | `	}` |
|     62 | 3276 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|     62 | 3277 | `	if( bTraits ){` |
|      3 | 3278 | `		apOut = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|      3 | 3279 | `		nOut = SySetUsed(&pClass->aTrait);` |
|      2 | 3280 | `	}else{` |
|     60 | 3281 | `		ReflectInterfacesOf(pClass, &aSet);` |
|     60 | 3282 | `		apOut = (ph7_class **)SySetBasePtr(&aSet);` |
|     60 | 3283 | `		nOut = SySetUsed(&aSet);` |
|      - | 3284 | `	}` |
|    146 | 3285 | `	for( n = 0 ; n < nOut ; n++ ){` |
|     86 | 3286 | `		SyString *pName = &apOut[n]->sName;` |
|     86 | 3287 | `		if( bReflector ){` |
|      - | 3288 | `			/* {name: ReflectionClass} — the reflector is built here rather than` |
|      - | 3289 | `			 * through ReflectResultClassOf, which writes the RESULT slot. */` |
|      5 | 3290 | `			ph7_class *pRC = PH7_VmExtractClass(pCtx->pVm, "ReflectionClass",` |
|      - | 3291 | `				sizeof("ReflectionClass")-1, FALSE, 0);` |
|      5 | 3292 | `			ph7_class_instance *pObj = pRC ? PH7_NewClassInstance(pCtx->pVm, pRC) : 0;` |
|      5 | 3293 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|      - | 3294 | `			ph7_value sVal;` |
|      5 | 3295 | `			if( pObj == 0 \|\| pKey == 0 ){ break; }` |
|      7 | 3296 | `			PH7_NativeSetAttrStr(pCtx->pVm, pObj, "name", SyStringData(pName),` |
|      4 | 3297 | `				(int)SyStringLength(pName));` |
|      - | 3298 | `			/* A STACK carrier, never a context scalar: releasing a MEMOBJ_OBJ` |
|      - | 3299 | `			 * context value would unref the instance a second time. */` |
|      5 | 3300 | `			PH7_MemObjInit(pCtx->pVm, &sVal);` |
|      5 | 3301 | `			sVal.x.pOther = pObj;` |
|      5 | 3302 | `			sVal.iFlags = MEMOBJ_OBJ;` |
|      5 | 3303 | `			ph7_value_string(pKey, SyStringData(pName), (int)SyStringLength(pName));` |
|      5 | 3304 | `			ph7_array_add_elem(pList, pKey, &sVal);   /* takes its own reference */` |
|      5 | 3305 | `			PH7_ClassInstanceUnref(pObj);` |
|      3 | 3306 | `		}else{` |
|     82 | 3307 | `			ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|     82 | 3308 | `			if( pVal == 0 ){ break; }` |
|     82 | 3309 | `			ph7_value_string(pVal, SyStringData(pName), (int)SyStringLength(pName));` |
|     82 | 3310 | `			ph7_array_add_elem(pList, 0, pVal);` |
|      - | 3311 | `		}` |
|     44 | 3312 | `	}` |
|     62 | 3313 | `	SySetRelease(&aSet);` |
|     62 | 3314 | `	ph7_result_value(pCtx, pList);` |
|     62 | 3315 | `	return PH7_OK;` |
|     32 | 3316 | `}` |
|      - | 3317 | `#define REFLECT_CLASS_LIST(NAME,TRAITS,REFLECTOR) \` |
|      - | 3318 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 3319 | `	{ \` |
|      - | 3320 | `		SXUNUSED(nArg); \` |
|      - | 3321 | `		SXUNUSED(apArg); \` |
|      - | 3322 | `		return ReflectClassNameList(pCtx,TRAITS,REFLECTOR); \` |
|      - | 3323 | `	}` |
|     58 | 3324 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getInterfaceNames, 0, 0)` |
|      3 | 3325 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getInterfaces,     0, 1)` |
|      3 | 3326 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getTraitNames,     1, 0)` |
|    ! 0 | 3327 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getTraits,         1, 1)` |
|      - | 3328 |  |
|      - | 3329 | ``/* getTraitAliases(): php reports `use T { m as n; }` renames; PHL's compiler`` |
|      - | 3330 | ` * installs the alias as a method and keeps no rename record, so the map is` |
|      - | 3331 | ` * empty (§7.4). */` |
|    ! 0 | 3332 | `static int vm_builtin_ReflectionClass_getTraitAliases(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 3333 | `{` |
|    ! 0 | 3334 | `	SXUNUSED(nArg);` |
|    ! 0 | 3335 | `	SXUNUSED(apArg);` |
|    ! 0 | 3336 | `	ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 | 3337 | `	return PH7_OK;` |
|    ! 0 | 3338 | `}` |
|      4 | 3339 | `static int vm_builtin_ReflectionClass_isIterable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3340 | `{` |
|      5 | 3341 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3342 | `	SySet aSet;` |
|      - | 3343 | `	ph7_class **apIface;` |
|      - | 3344 | `	sxu32 n;` |
|      5 | 3345 | `	int bIterable = 0;` |
|      2 | 3346 | `	SXUNUSED(nArg);` |
|      2 | 3347 | `	SXUNUSED(apArg);` |
|      4 | 3348 | `	if( pClass == 0` |
|      5 | 3349 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT)) ){` |
|    ! 0 | 3350 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3351 | `		return PH7_OK;` |
|      - | 3352 | `	}` |
|      5 | 3353 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|      5 | 3354 | `	ReflectInterfacesOf(pClass, &aSet);` |
|      5 | 3355 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|      5 | 3356 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|      5 | 3357 | `		if( pCtx->pVm->pTraversableClass && apIface[n] == pCtx->pVm->pTraversableClass ){` |
|      5 | 3358 | `			bIterable = 1;` |
|      5 | 3359 | `			break;` |
|      - | 3360 | `		}` |
|    ! 0 | 3361 | `	}` |
|      5 | 3362 | `	SySetRelease(&aSet);` |
|      5 | 3363 | `	ph7_result_bool(pCtx, bIterable);` |
|      5 | 3364 | `	return PH7_OK;` |
|      3 | 3365 | `}` |
|     16 | 3366 | `static int vm_builtin_ReflectionClass_implementsInterface(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3367 | `{` |
|     17 | 3368 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3369 | `	ph7_class *pTarget;` |
|      - | 3370 | `	SySet aSet;` |
|      - | 3371 | `	ph7_class **apIface;` |
|      - | 3372 | `	sxu32 n;` |
|     17 | 3373 | `	int bYes = 0;` |
|     17 | 3374 | `	if( nArg < 1 ){` |
|    ! 0 | 3375 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3376 | `		return PH7_OK;` |
|      - | 3377 | `	}` |
|     17 | 3378 | `	pTarget = ReflectClassArg(pCtx, apArg[0]);` |
|     17 | 3379 | `	if( pTarget == 0 ){` |
|      - | 3380 | `		const char *zName;` |
|      - | 3381 | `		int nName;` |
|      3 | 3382 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|      4 | 3383 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 3384 | `			"Interface \"%.*s\" does not exist", nName, zName);` |
|      - | 3385 | `	}` |
|     15 | 3386 | `	if( (pTarget->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|      4 | 3387 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 3388 | `			"%z is not an interface", &pTarget->sName);` |
|      - | 3389 | `	}` |
|     13 | 3390 | `	if( pClass == pTarget ){` |
|      3 | 3391 | `		ph7_result_bool(pCtx, 1);` |
|      3 | 3392 | `		return PH7_OK;` |
|      - | 3393 | `	}` |
|     11 | 3394 | `	if( pClass == 0 ){` |
|    ! 0 | 3395 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3396 | `		return PH7_OK;` |
|      - | 3397 | `	}` |
|     11 | 3398 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|     11 | 3399 | `	ReflectInterfacesOf(pClass, &aSet);` |
|     11 | 3400 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|     13 | 3401 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|     13 | 3402 | `		if( apIface[n] == pTarget ){` |
|     11 | 3403 | `			bYes = 1;` |
|     11 | 3404 | `			break;` |
|      - | 3405 | `		}` |
|      2 | 3406 | `	}` |
|     11 | 3407 | `	SySetRelease(&aSet);` |
|     11 | 3408 | `	ph7_result_bool(pCtx, bYes);` |
|     11 | 3409 | `	return PH7_OK;` |
|      9 | 3410 | `}` |
|      6 | 3411 | `static int vm_builtin_ReflectionClass_isSubclassOf(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3412 | `{` |
|      7 | 3413 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3414 | `	ph7_class *pTarget, *pWalk;` |
|      - | 3415 | `	SySet aSet;` |
|      - | 3416 | `	ph7_class **apIface;` |
|      - | 3417 | `	sxu32 n;` |
|      7 | 3418 | `	int iDepth = 0, bYes = 0;` |
|      7 | 3419 | `	if( nArg < 1 ){` |
|    ! 0 | 3420 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3421 | `		return PH7_OK;` |
|      - | 3422 | `	}` |
|      7 | 3423 | `	pTarget = ReflectClassArg(pCtx, apArg[0]);` |
|      7 | 3424 | `	if( pTarget == 0 ){` |
|      - | 3425 | `		const char *zName;` |
|      - | 3426 | `		int nName;` |
|    ! 0 | 3427 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|    ! 0 | 3428 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 | 3429 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      - | 3430 | `	}` |
|      - | 3431 | `	/* php: a class is never a subclass of ITSELF */` |
|      7 | 3432 | `	if( pClass == 0 \|\| pClass == pTarget ){` |
|      3 | 3433 | `		ph7_result_bool(pCtx, 0);` |
|      3 | 3434 | `		return PH7_OK;` |
|      - | 3435 | `	}` |
|      7 | 3436 | `	for( pWalk = pClass->pBase ; pWalk && iDepth <= REFLECT_WALK_MAX_DEPTH ; pWalk = pWalk->pBase ){` |
|      5 | 3437 | `		if( pWalk == pTarget ){` |
|      3 | 3438 | `			ph7_result_bool(pCtx, 1);` |
|      3 | 3439 | `			return PH7_OK;` |
|      - | 3440 | `		}` |
|      3 | 3441 | `		iDepth++;` |
|      2 | 3442 | `	}` |
|      3 | 3443 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|      3 | 3444 | `	ReflectInterfacesOf(pClass, &aSet);` |
|      3 | 3445 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|      3 | 3446 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|      3 | 3447 | `		if( apIface[n] == pTarget ){` |
|      3 | 3448 | `			bYes = 1;` |
|      3 | 3449 | `			break;` |
|      - | 3450 | `		}` |
|    ! 0 | 3451 | `	}` |
|      3 | 3452 | `	SySetRelease(&aSet);` |
|      3 | 3453 | `	ph7_result_bool(pCtx, bYes);` |
|      3 | 3454 | `	return PH7_OK;` |
|      4 | 3455 | `}` |
|      8 | 3456 | `static int vm_builtin_ReflectionClass_isInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3457 | `{` |
|      9 | 3458 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3459 | `	ph7_class_instance *pObj;` |
|      9 | 3460 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 \|\| pClass == 0 ){` |
|    ! 0 | 3461 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3462 | `		return PH7_OK;` |
|      - | 3463 | `	}` |
|      9 | 3464 | `	pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      9 | 3465 | `	ph7_result_bool(pCtx, PH7_VmInstanceOf(pObj->pClass, pClass) != 0);` |
|      9 | 3466 | `	return PH7_OK;` |
|      5 | 3467 | `}` |
|      - | 3468 | `/* ---- source position ---- */` |
|      8 | 3469 | `static int vm_builtin_ReflectionClass_getStartLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3470 | `{` |
|      9 | 3471 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      4 | 3472 | `	SXUNUSED(nArg);` |
|      4 | 3473 | `	SXUNUSED(apArg);` |
|      9 | 3474 | `	if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_INTERNAL) ){` |
|      3 | 3475 | `		ph7_result_bool(pCtx, 0);` |
|      2 | 3476 | `	}else{` |
|      7 | 3477 | `		ph7_result_int64(pCtx, (sxi64)pClass->nLine);` |
|      - | 3478 | `	}` |
|      9 | 3479 | `	return PH7_OK;` |
|      1 | 3480 | `}` |
|      6 | 3481 | `static int vm_builtin_ReflectionClass_getEndLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3482 | `{` |
|      7 | 3483 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      3 | 3484 | `	SXUNUSED(nArg);` |
|      3 | 3485 | `	SXUNUSED(apArg);` |
|      7 | 3486 | `	if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_INTERNAL) ){` |
|      3 | 3487 | `		ph7_result_bool(pCtx, 0);` |
|      2 | 3488 | `	}else{` |
|      5 | 3489 | `		ph7_result_int64(pCtx, (sxi64)pClass->nEndLine);` |
|      - | 3490 | `	}` |
|      7 | 3491 | `	return PH7_OK;` |
|      1 | 3492 | `}` |
|     12 | 3493 | `static int vm_builtin_ReflectionClass_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 3494 | `{` |
|     14 | 3495 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      6 | 3496 | `	SXUNUSED(nArg);` |
|      6 | 3497 | `	SXUNUSED(apArg);` |
|     14 | 3498 | `	if( pClass && SyStringLength(&pClass->sFile) > 0 ){` |
|      8 | 3499 | `		ph7_result_string(pCtx, SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));` |
|      5 | 3500 | `	}else{` |
|      7 | 3501 | `		ph7_result_bool(pCtx, 0);` |
|      - | 3502 | `	}` |
|     14 | 3503 | `	return PH7_OK;` |
|      2 | 3504 | `}` |
|      8 | 3505 | `static int vm_builtin_ReflectionClass_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3506 | `{` |
|      9 | 3507 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      4 | 3508 | `	SXUNUSED(nArg);` |
|      4 | 3509 | `	SXUNUSED(apArg);` |
|      9 | 3510 | `	if( pClass && SyStringLength(&pClass->sDoc) > 0 ){` |
|      3 | 3511 | `		ph7_result_string(pCtx, SyStringData(&pClass->sDoc), (int)SyStringLength(&pClass->sDoc));` |
|      2 | 3512 | `	}else{` |
|      7 | 3513 | `		ph7_result_bool(pCtx, 0);` |
|      - | 3514 | `	}` |
|      9 | 3515 | `	return PH7_OK;` |
|      1 | 3516 | `}` |
|      - | 3517 | `/* ---- instantiation ---- */` |
|     14 | 3518 | `static int vm_builtin_ReflectionClass_isInstantiable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3519 | `{` |
|     15 | 3520 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3521 | `	sxi32 iCtor, iClone;` |
|      7 | 3522 | `	SXUNUSED(nArg);` |
|      7 | 3523 | `	SXUNUSED(apArg);` |
|     14 | 3524 | `	if( pClass == 0` |
|     15 | 3525 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT\|PH7_CLASS_ENUM)) ){` |
|      7 | 3526 | `		ph7_result_bool(pCtx, 0);` |
|      7 | 3527 | `		return PH7_OK;` |
|      - | 3528 | `	}` |
|      9 | 3529 | `	ReflectCtorCloneVis(pCtx->pVm, pClass, &iCtor, &iClone);` |
|      9 | 3530 | `	ph7_result_bool(pCtx, iCtor == 0 \|\| iCtor == PH7_CLASS_PROT_PUBLIC);` |
|      9 | 3531 | `	return PH7_OK;` |
|      8 | 3532 | `}` |
|     32 | 3533 | `static int vm_builtin_ReflectionClass_isCloneable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3534 | `{` |
|     33 | 3535 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3536 | `	sxi32 iCtor, iClone;` |
|     16 | 3537 | `	SXUNUSED(nArg);` |
|     16 | 3538 | `	SXUNUSED(apArg);` |
|     32 | 3539 | `	if( pClass == 0` |
|     33 | 3540 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT)) ){` |
|      3 | 3541 | `		ph7_result_bool(pCtx, 0);` |
|      3 | 3542 | `		return PH7_OK;` |
|      - | 3543 | `	}` |
|     31 | 3544 | `	ReflectCtorCloneVis(pCtx->pVm, pClass, &iCtor, &iClone);` |
|     31 | 3545 | `	if( iClone != 0 ){` |
|      - | 3546 | `		/* php consults a declared __clone FIRST and answers its visibility --` |
|      - | 3547 | ``		 * even when the clone_obj refusal would still answer `clone` itself, so`` |
|      - | 3548 | `		 * a subclass of Exception that declares a public __clone reports TRUE` |
|      - | 3549 | `		 * and refuses anyway. php's own inconsistency, kept. */` |
|      7 | 3550 | `		ph7_result_bool(pCtx, iClone == PH7_CLASS_PROT_PUBLIC);` |
|      7 | 3551 | `		return PH7_OK;` |
|      - | 3552 | `	}` |
|      - | 3553 | `	/* No __clone anywhere: php's answer is whether the clone_obj handler` |
|      - | 3554 | `	 * exists. The refusal flag walks the base chain like the handler it` |
|      - | 3555 | ``	 * models, so `class M extends IteratorIterator {}` reports false too. */`` |
|     25 | 3556 | `	ph7_result_bool(pCtx, !PH7_ClassIsUncloneable(pClass));` |
|     25 | 3557 | `	return PH7_OK;` |
|     17 | 3558 | `}` |
|      - | 3559 | `/* php's own gate, raised before any object exists. */` |
|     42 | 3560 | `static sxi32 ReflectCheckInstantiable(ph7_context *pCtx, ph7_class *pClass)` |
|      3 | 3561 | `{` |
|     45 | 3562 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|    ! 0 | 3563 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate interface %z", &pClass->sName);` |
|      - | 3564 | `	}` |
|     45 | 3565 | `	if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|    ! 0 | 3566 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate trait %z", &pClass->sName);` |
|      - | 3567 | `	}` |
|     45 | 3568 | `	if( pClass->iFlags & PH7_CLASS_ABSTRACT ){` |
|      5 | 3569 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate abstract class %z", &pClass->sName);` |
|      - | 3570 | `	}` |
|     41 | 3571 | `	if( pClass->iFlags & PH7_CLASS_NOINSTANTIATE ){` |
|      - | 3572 | ``		/* The `new` path's create_object refusal, which php raises here too —`` |
|      - | 3573 | `		 * ReflectionClass::newInstance() on a Closure is the same Error, not a` |
|      - | 3574 | `		 * visibility one about its private constructor. */` |
|      8 | 3575 | `		if( pClass->zNewRefusal ){` |
|      6 | 3576 | `			return PH7_VmThrowException(pCtx, "Error", "%s", pClass->zNewRefusal);` |
|      - | 3577 | `		}` |
|      4 | 3578 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      1 | 3579 | `			"Instantiation of class %z is not allowed", &pClass->sName);` |
|      - | 3580 | `	}` |
|     35 | 3581 | `	return PH7_OK;` |
|     24 | 3582 | `}` |
|      - | 3583 | `/*` |
|      - | 3584 | ` * newInstance()/newInstanceArgs(): the checks php runs, then the constructor.` |
|      - | 3585 | ` * apCtor/nCtor are already-collected positional arguments; pNames is the` |
|      - | 3586 | ` * name map when the caller handed an array with string keys (php 8.1 accepts` |
|      - | 3587 | ` * those as NAMED constructor arguments).` |
|      - | 3588 | ` */` |
|     24 | 3589 | `static int ReflectNewInstance(ph7_context *pCtx, int nCtor, ph7_value **apCtor,` |
|      - | 3590 | `	SyString *pNames)` |
|      3 | 3591 | `{` |
|     27 | 3592 | `	ph7_vm *pVm = pCtx->pVm;` |
|     27 | 3593 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3594 | `	ph7_class_instance *pObj;` |
|      - | 3595 | `	ph7_class_method *pCons;` |
|      - | 3596 | `	sxi32 iCtorVis, iCloneVis, rc;` |
|     27 | 3597 | `	if( pClass == 0 ){` |
|    ! 0 | 3598 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3599 | `		return PH7_OK;` |
|      - | 3600 | `	}` |
|     27 | 3601 | `	rc = ReflectCheckInstantiable(pCtx, pClass);` |
|     27 | 3602 | `	if( rc != PH7_OK ){` |
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
|     15 | 3646 | `}` |
|     22 | 3647 | `static int vm_builtin_ReflectionClass_newInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 3648 | `{` |
|     25 | 3649 | `	return ReflectNewInstance(pCtx, nArg, apArg, 0);` |
|      3 | 3650 | `}` |
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
|     18 | 3668 | `static int vm_builtin_ReflectionClass_newInstanceWithoutConstructor(ph7_context *pCtx,` |
|      - | 3669 | `	int nArg, ph7_value **apArg)` |
|      2 | 3670 | `{` |
|     20 | 3671 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3672 | `	sxi32 rc;` |
|      9 | 3673 | `	SXUNUSED(nArg);` |
|      9 | 3674 | `	SXUNUSED(apArg);` |
|     20 | 3675 | `	if( pClass == 0 ){` |
|    ! 0 | 3676 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3677 | `		return PH7_OK;` |
|      - | 3678 | `	}` |
|     20 | 3679 | `	rc = ReflectCheckInstantiable(pCtx, pClass);` |
|     20 | 3680 | `	if( rc != PH7_OK ){` |
|      3 | 3681 | `		return rc;` |
|      - | 3682 | `	}` |
|     18 | 3683 | `	if( VmClassStaticDeferPending(pClass) ){` |
|      3 | 3684 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);` |
|      3 | 3685 | `		if( rcMat != SXRET_OK ){` |
|      3 | 3686 | `			return rcMat;` |
|      - | 3687 | `		}` |
|    ! 0 | 3688 | `	}` |
|     15 | 3689 | `	return ReflectResultObject(pCtx, PH7_NewClassInstance(pCtx->pVm, pClass));` |
|     11 | 3690 | `}` |
|      - | 3691 | `/* ---- members ---- */` |
|      - | 3692 | `/* The modifier mask php filters a member on. */` |
|    128 | 3693 | `static sxi64 ReflectVisMask(sxi32 iProt)` |
|      2 | 3694 | `{` |
|    130 | 3695 | `	if( iProt == PH7_CLASS_PROT_PUBLIC ){` |
|     78 | 3696 | `		return 1;` |
|      - | 3697 | `	}` |
|     53 | 3698 | `	return iProt == PH7_CLASS_PROT_PROTECTED ? 2 : 4;` |
|     66 | 3699 | `}` |
|     86 | 3700 | `static sxi64 ReflectPropModifiers(ph7_class_attr *pAttr)` |
|      2 | 3701 | `{` |
|     88 | 3702 | `	sxi64 iMods = ReflectVisMask(pAttr->iProtection);` |
|     88 | 3703 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){ iMods \|= 16; }` |
|     88 | 3704 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){ iMods \|= 512; }` |
|     88 | 3705 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|      - | 3706 | `		/* php models readonly as protected(set) and reports the bit. */` |
|     14 | 3707 | `		iMods \|= 128\|2048;` |
|      6 | 3708 | `	}` |
|     88 | 3709 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET ){ iMods \|= 2048; }` |
|     88 | 3710 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|      - | 3711 | `		/* private(set) cannot be widened by a subclass, so php reports it FINAL. */` |
|      5 | 3712 | `		iMods \|= 4096\|32;` |
|      2 | 3713 | `	}` |
|     88 | 3714 | `	return iMods;` |
|      2 | 3715 | `}` |
|     16 | 3716 | `static sxi64 ReflectMethodModifiers(ph7_class_method *pMeth)` |
|      1 | 3717 | `{` |
|     17 | 3718 | `	sxi64 iMods = ReflectVisMask(pMeth->iProtection);` |
|     17 | 3719 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){ iMods \|= 16; }` |
|     17 | 3720 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){ iMods \|= 64; }` |
|     17 | 3721 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods \|= 32; }` |
|     17 | 3722 | `	return iMods;` |
|      1 | 3723 | `}` |
|     26 | 3724 | `static sxi64 ReflectConstModifiers(ph7_class_attr *pAttr)` |
|      1 | 3725 | `{` |
|     27 | 3726 | `	sxi64 iMods = ReflectVisMask(pAttr->iProtection);` |
|     27 | 3727 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods \|= 32; }` |
|     27 | 3728 | `	return iMods;` |
|      1 | 3729 | `}` |
|      - | 3730 | `/* Does the reflected object own a property under this name? (1 = exists) */` |
|     16 | 3731 | `static int ReflectObjHasProp(ph7_class_instance *pObj, const char *zName, int nName)` |
|      1 | 3732 | `{` |
|     13 | 3733 | `	return pObj != 0 && nName > 0` |
|     18 | 3734 | `		&& SyHashGet(&pObj->hAttr, (const void *)zName, (sxu32)nName) != 0;` |
|      1 | 3735 | `}` |
|     12 | 3736 | `static int vm_builtin_ReflectionClass_hasMethod(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3737 | `{` |
|     13 | 3738 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3739 | `	const char *zName;` |
|      - | 3740 | `	int nName;` |
|     13 | 3741 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 3742 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3743 | `		return PH7_OK;` |
|      - | 3744 | `	}` |
|     13 | 3745 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     13 | 3746 | `	ph7_result_bool(pCtx, ReflectFindMethodEntry(pClass, zName, nName) != 0);` |
|     13 | 3747 | `	return PH7_OK;` |
|      7 | 3748 | `}` |
|     16 | 3749 | `static int vm_builtin_ReflectionClass_hasProperty(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 3750 | `{` |
|     18 | 3751 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3752 | `	const char *zName;` |
|     18 | 3753 | `	int nName, bFound = 0;` |
|      - | 3754 | `	SySet aMembers;` |
|      - | 3755 | `	sxu32 n;` |
|     18 | 3756 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 3757 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3758 | `		return PH7_OK;` |
|      - | 3759 | `	}` |
|     18 | 3760 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     18 | 3761 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     18 | 3762 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     48 | 3763 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     38 | 3764 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     38 | 3765 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zName, nName) ){` |
|      8 | 3766 | `			bFound = 1;` |
|      8 | 3767 | `			break;` |
|      - | 3768 | `		}` |
|     16 | 3769 | `	}` |
|     18 | 3770 | `	SySetRelease(&aMembers);` |
|      - | 3771 | `	/* A ReflectionObject also sees the instance's own dynamic properties */` |
|     18 | 3772 | `	if( !bFound ){` |
|     11 | 3773 | `		bFound = ReflectObjHasProp(ReflectClassObj(pCtx), zName, nName);` |
|      5 | 3774 | `	}` |
|     18 | 3775 | `	ph7_result_bool(pCtx, bFound);` |
|     18 | 3776 | `	return PH7_OK;` |
|     10 | 3777 | `}` |
|     22 | 3778 | `static int vm_builtin_ReflectionClass_hasConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 3779 | `{` |
|     24 | 3780 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3781 | `	const char *zName;` |
|     24 | 3782 | `	int nName, bFound = 0;` |
|      - | 3783 | `	SySet aMembers;` |
|      - | 3784 | `	sxu32 n;` |
|     24 | 3785 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 3786 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3787 | `		return PH7_OK;` |
|      - | 3788 | `	}` |
|     24 | 3789 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     24 | 3790 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     24 | 3791 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|   1310 | 3792 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   1292 | 3793 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   1292 | 3794 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|      5 | 3795 | `			bFound = 1;` |
|      5 | 3796 | `			break;` |
|      - | 3797 | `		}` |
|    645 | 3798 | `	}` |
|     24 | 3799 | `	SySetRelease(&aMembers);` |
|     24 | 3800 | `	ph7_result_bool(pCtx, bFound);` |
|     24 | 3801 | `	return PH7_OK;` |
|     13 | 3802 | `}` |
|      - | 3803 | `/*` |
|      - | 3804 | ` * The value of a class constant, materializing its lazily-evaluated slot.` |
|      - | 3805 | ` *` |
|      - | 3806 | ` * php re-evaluates a failed initializer on EVERY read, so this can raise; the` |
|      - | 3807 | ` * status is returned rather than swallowed, because a native body that answers` |
|      - | 3808 | ` * PH7_OK with a throw in flight lets the caller carry on and print the NULL it` |
|      - | 3809 | ` * never should have seen.` |
|      - | 3810 | ` */` |
|    136 | 3811 | `static sxi32 ReflectConstSlot(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr,` |
|      - | 3812 | `	ph7_value **ppOut)` |
|      3 | 3813 | `{` |
|    139 | 3814 | `	sxi32 rc = PH7_VmMaterializeClassConst(pCtx->pVm, pClass, pAttr);` |
|    139 | 3815 | `	*ppOut = 0;` |
|    139 | 3816 | `	if( rc != SXRET_OK ){` |
|      3 | 3817 | `		return rc;` |
|      - | 3818 | `	}` |
|    136 | 3819 | `	*ppOut = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|    136 | 3820 | `	return SXRET_OK;` |
|     71 | 3821 | `}` |
|      6 | 3822 | `static int vm_builtin_ReflectionClass_getConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 3823 | `{` |
|      8 | 3824 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3825 | `	const char *zName;` |
|      - | 3826 | `	int nName;` |
|      - | 3827 | `	SySet aMembers;` |
|      - | 3828 | `	sxu32 n;` |
|      8 | 3829 | `	ph7_class_attr *pFound = 0;` |
|      8 | 3830 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 3831 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3832 | `		return PH7_OK;` |
|      - | 3833 | `	}` |
|      8 | 3834 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|      8 | 3835 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|      8 | 3836 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     16 | 3837 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     16 | 3838 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     16 | 3839 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|      8 | 3840 | `			pFound = pM->pAttr;` |
|      8 | 3841 | `			break;` |
|      - | 3842 | `		}` |
|      5 | 3843 | `	}` |
|      8 | 3844 | `	SySetRelease(&aMembers);` |
|      8 | 3845 | `	if( pFound == 0 ){` |
|    ! 0 | 3846 | `		PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED,` |
|      - | 3847 | `			"ReflectionClass::getConstant() for a non-existent constant is deprecated, "` |
|      - | 3848 | `			"use ReflectionClass::hasConstant() to check if the constant exists");` |
|    ! 0 | 3849 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 3850 | `		return PH7_OK;` |
|      - | 3851 | `	}` |
|      - | 3852 | `	{` |
|      - | 3853 | `		ph7_value *pVal;` |
|      8 | 3854 | `		sxi32 rc = ReflectConstSlot(pCtx, pClass, pFound, &pVal);` |
|      8 | 3855 | `		if( rc != SXRET_OK ){` |
|      3 | 3856 | `			return rc;` |
|      - | 3857 | `		}` |
|      5 | 3858 | `		if( pVal ){` |
|      5 | 3859 | `			ph7_result_value(pCtx, pVal);` |
|      3 | 3860 | `		}else{` |
|    ! 0 | 3861 | `			ph7_result_null(pCtx);` |
|      - | 3862 | `		}` |
|      - | 3863 | `	}` |
|      5 | 3864 | `	return PH7_OK;` |
|      5 | 3865 | `}` |
|     20 | 3866 | `static int vm_builtin_ReflectionClass_getConstants(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 3867 | `{` |
|     22 | 3868 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     22 | 3869 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - | 3870 | `	SySet aMembers;` |
|      - | 3871 | `	sxu32 n;` |
|     22 | 3872 | `	int bFilter = (nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0);` |
|     22 | 3873 | `	sxi64 iFilter = bFilter ? ph7_value_to_int64(apArg[0]) : 0;` |
|     22 | 3874 | `	if( pOut == 0 ){` |
|    ! 0 | 3875 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 3876 | `	}` |
|     22 | 3877 | `	if( pClass == 0 ){` |
|    ! 0 | 3878 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 | 3879 | `		return PH7_OK;` |
|      - | 3880 | `	}` |
|     22 | 3881 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     22 | 3882 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|    190 | 3883 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    170 | 3884 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - | 3885 | `		ph7_value *pVal;` |
|    170 | 3886 | `		if( pM->iKind != REFLECT_MEMBER_CONST ){` |
|    102 | 3887 | `			continue;` |
|      - | 3888 | `		}` |
|     72 | 3889 | `		if( bFilter && (ReflectConstModifiers(pM->pAttr) & iFilter) == 0 ){` |
|      5 | 3890 | `			continue;` |
|      - | 3891 | `		}` |
|      - | 3892 | `		{` |
|     68 | 3893 | `			sxi32 rc = ReflectConstSlot(pCtx, pClass, pM->pAttr, &pVal);` |
|     68 | 3894 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 3895 | `				SySetRelease(&aMembers);` |
|    ! 0 | 3896 | `				return rc;` |
|      - | 3897 | `			}` |
|      - | 3898 | `		}` |
|     68 | 3899 | `		if( pVal ){` |
|     68 | 3900 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, pVal);` |
|     33 | 3901 | `		}` |
|     35 | 3902 | `	}` |
|     22 | 3903 | `	SySetRelease(&aMembers);` |
|     22 | 3904 | `	ph7_result_value(pCtx, pOut);` |
|     22 | 3905 | `	return PH7_OK;` |
|     12 | 3906 | `}` |
|      - | 3907 | `/*` |
|      - | 3908 | ` * getMethod()/getProperty()/getReflectionConstant() build a class that is still` |
|      - | 3909 | ` * PRELUDE PHP, so they go through its constructor (ReflectConstruct). They` |
|      - | 3910 | ` * become direct C when chunks 2 and 3 land.` |
|      - | 3911 | ` */` |
|     56 | 3912 | `static int ReflectResultMember(ph7_context *pCtx, const char *zClass,` |
|      - | 3913 | `	ph7_value *pTarget, const SyString *pName)` |
|      4 | 3914 | `{` |
|      - | 3915 | `	ph7_value sName;` |
|      - | 3916 | `	ph7_value *apCtor[2];` |
|      - | 3917 | `	ph7_class_instance *pOut;` |
|      - | 3918 | `	sxi32 rc;` |
|     60 | 3919 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|     60 | 3920 | `	ph7_value_string(&sName, SyStringData(pName), (int)SyStringLength(pName));` |
|     60 | 3921 | `	apCtor[0] = pTarget;` |
|     60 | 3922 | `	apCtor[1] = &sName;` |
|     60 | 3923 | `	pOut = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|     60 | 3924 | `	PH7_MemObjRelease(&sName);` |
|     60 | 3925 | `	if( pOut == 0 ){` |
|    ! 0 | 3926 | `		if( rc != PH7_OK ){` |
|    ! 0 | 3927 | `			return rc;` |
|      - | 3928 | `		}` |
|    ! 0 | 3929 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3930 | `		return PH7_OK;` |
|      - | 3931 | `	}` |
|     60 | 3932 | `	return ReflectResultObject(pCtx, pOut);` |
|     32 | 3933 | `}` |
|      - | 3934 | ``/* A `ReflectionMethod($this->name, ...)`-shaped first argument. */`` |
|    174 | 3935 | `static void ReflectSelfName(ph7_context *pCtx, ph7_value *pOut)` |
|      4 | 3936 | `{` |
|      - | 3937 | `	const char *zName;` |
|      - | 3938 | `	int nName;` |
|    178 | 3939 | `	ReflectClassName(pCtx, &zName, &nName);` |
|    178 | 3940 | `	PH7_MemObjInit(pCtx->pVm, pOut);` |
|    178 | 3941 | `	ph7_value_string(pOut, zName, nName);` |
|    178 | 3942 | `}` |
|     34 | 3943 | `static int vm_builtin_ReflectionClass_getMethod(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 3944 | `{` |
|     35 | 3945 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3946 | `	SyHashEntry *pEntry;` |
|      - | 3947 | `	const char *zName;` |
|      - | 3948 | `	int nName;` |
|      - | 3949 | `	SyString sFound;` |
|     35 | 3950 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 3951 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3952 | `		return PH7_OK;` |
|      - | 3953 | `	}` |
|     35 | 3954 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     35 | 3955 | `	pEntry = ReflectFindMethodEntry(pClass, zName, nName);` |
|     35 | 3956 | `	if( pEntry == 0 ){` |
|      4 | 3957 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 3958 | `			"Method %z::%.*s() does not exist", &pClass->sName, nName, zName);` |
|      - | 3959 | `	}` |
|      - | 3960 | `	/* The reported name is the DECLARED spelling, whatever case was asked for. */` |
|     33 | 3961 | `	SyStringInitFromBuf(&sFound, (const char *)pEntry->pKey, pEntry->nKeyLen);` |
|      - | 3962 | `	{` |
|      - | 3963 | `		ph7_value sSelf;` |
|      - | 3964 | `		int rc;` |
|     33 | 3965 | `		ReflectSelfName(pCtx, &sSelf);` |
|     33 | 3966 | `		rc = ReflectResultMember(pCtx, "ReflectionMethod", &sSelf, &sFound);` |
|     33 | 3967 | `		PH7_MemObjRelease(&sSelf);` |
|     33 | 3968 | `		return rc;` |
|      - | 3969 | `	}` |
|     18 | 3970 | `}` |
|     24 | 3971 | `static int vm_builtin_ReflectionClass_getConstructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 | 3972 | `{` |
|     28 | 3973 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 3974 | `	SyHashEntry *pEntry;` |
|      - | 3975 | `	SyString sFound;` |
|     12 | 3976 | `	SXUNUSED(nArg);` |
|     12 | 3977 | `	SXUNUSED(apArg);` |
|     28 | 3978 | `	if( pClass == 0 ){` |
|    ! 0 | 3979 | `		ph7_result_null(pCtx);` |
|    ! 0 | 3980 | `		return PH7_OK;` |
|      - | 3981 | `	}` |
|      - | 3982 | `	/* No PHP-4 class-name constructor: a method named like the class is a plain` |
|      - | 3983 | `	 * method (removed in 8.0), so this stays null without an explicit one. */` |
|     28 | 3984 | `	pEntry = ReflectFindMethodEntry(pClass, "__construct", sizeof("__construct")-1);` |
|     28 | 3985 | `	if( pEntry == 0 ){` |
|      9 | 3986 | `		ph7_result_null(pCtx);` |
|      9 | 3987 | `		return PH7_OK;` |
|      - | 3988 | `	}` |
|     22 | 3989 | `	SyStringInitFromBuf(&sFound, (const char *)pEntry->pKey, pEntry->nKeyLen);` |
|      - | 3990 | `	{` |
|      - | 3991 | `		ph7_value sSelf;` |
|      - | 3992 | `		int rc;` |
|     22 | 3993 | `		ReflectSelfName(pCtx, &sSelf);` |
|     22 | 3994 | `		rc = ReflectResultMember(pCtx, "ReflectionMethod", &sSelf, &sFound);` |
|     22 | 3995 | `		PH7_MemObjRelease(&sSelf);` |
|     22 | 3996 | `		return rc;` |
|      - | 3997 | `	}` |
|     16 | 3998 | `}` |
|      - | 3999 | `/*` |
|      - | 4000 | ` * getMethods() / getProperties() / getReflectionConstants(): one member walk,` |
|      - | 4001 | ` * one reflector per surviving member. Each reflector is a prelude class built` |
|      - | 4002 | ` * through its constructor, and is appended through a STACK carrier so the list` |
|      - | 4003 | ` * takes its own reference rather than the creation one.` |
|      - | 4004 | ` */` |
|    120 | 4005 | `static int ReflectMemberList(ph7_context *pCtx, int iKind, const char *zClass,` |
|      - | 4006 | `	int nArg, ph7_value **apArg)` |
|      3 | 4007 | `{` |
|    123 | 4008 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|    123 | 4009 | `	ph7_class_instance *pObj = ReflectClassObj(pCtx);` |
|    123 | 4010 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - | 4011 | `	ph7_value sSelf;` |
|      - | 4012 | `	SySet aMembers;` |
|      - | 4013 | `	sxu32 n;` |
|    123 | 4014 | `	int bFilter = (nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0);` |
|    123 | 4015 | `	sxi64 iFilter = bFilter ? ph7_value_to_int64(apArg[0]) : 0;` |
|    123 | 4016 | `	if( pOut == 0 ){` |
|    ! 0 | 4017 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4018 | `	}` |
|    123 | 4019 | `	if( pClass == 0 ){` |
|    ! 0 | 4020 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 | 4021 | `		return PH7_OK;` |
|      - | 4022 | `	}` |
|    123 | 4023 | `	ReflectSelfName(pCtx, &sSelf);` |
|    123 | 4024 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|    123 | 4025 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|   1359 | 4026 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   1239 | 4027 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - | 4028 | `		ph7_value sName, sVal;` |
|      - | 4029 | `		ph7_value *apCtor[2];` |
|      - | 4030 | `		ph7_class_instance *pRef;` |
|      - | 4031 | `		sxi32 rc;` |
|   1239 | 4032 | `		if( pM->iKind != iKind ){` |
|    962 | 4033 | `			continue;` |
|      - | 4034 | `		}` |
|    289 | 4035 | `		if( bFilter ){` |
|     33 | 4036 | `			sxi64 iMods = iKind == REFLECT_MEMBER_METHOD ? ReflectMethodModifiers(pM->pMeth)` |
|     40 | 4037 | `				: (iKind == REFLECT_MEMBER_CONST ? ReflectConstModifiers(pM->pAttr)` |
|     20 | 4038 | `				                                 : ReflectPropModifiers(pM->pAttr));` |
|     33 | 4039 | `			if( (iMods & iFilter) == 0 ){` |
|     21 | 4040 | `				continue;` |
|      - | 4041 | `			}` |
|      6 | 4042 | `		}` |
|    269 | 4043 | `		PH7_MemObjInit(pCtx->pVm, &sName);` |
|    269 | 4044 | `		ph7_value_string(&sName, SyStringData(&pM->sKey), (int)SyStringLength(&pM->sKey));` |
|    269 | 4045 | `		apCtor[0] = &sSelf;` |
|    269 | 4046 | `		apCtor[1] = &sName;` |
|    269 | 4047 | `		pRef = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|    269 | 4048 | `		PH7_MemObjRelease(&sName);` |
|    269 | 4049 | `		if( pRef == 0 ){` |
|    ! 0 | 4050 | `			SySetRelease(&aMembers);` |
|    ! 0 | 4051 | `			PH7_MemObjRelease(&sSelf);` |
|    ! 0 | 4052 | `			if( rc != PH7_OK ){` |
|    ! 0 | 4053 | `				return rc;` |
|      - | 4054 | `			}` |
|    ! 0 | 4055 | `			ph7_result_value(pCtx, pOut);` |
|    ! 0 | 4056 | `			return PH7_OK;` |
|      - | 4057 | `		}` |
|    269 | 4058 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    269 | 4059 | `		sVal.x.pOther = pRef;` |
|    269 | 4060 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|    269 | 4061 | `		ph7_array_add_elem(pOut, 0, &sVal);   /* takes its own reference */` |
|    269 | 4062 | `		PH7_ClassInstanceUnref(pRef);` |
|    136 | 4063 | `	}` |
|    123 | 4064 | `	SySetRelease(&aMembers);` |
|      - | 4065 | `	/* A ReflectionObject also reports the instance's own DYNAMIC properties,` |
|      - | 4066 | `	 * which no class declaration knows about. */` |
|    123 | 4067 | `	if( iKind == REFLECT_MEMBER_PROP && pObj != 0 && (!bFilter \|\| (iFilter & 1)) ){` |
|      - | 4068 | `		SyHashEntry *pEntry;` |
|      - | 4069 | `		ph7_value sTarget;` |
|      3 | 4070 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      3 | 4071 | `		sTarget.x.pOther = pObj;` |
|      3 | 4072 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|      3 | 4073 | `		SyHashResetLoopCursor(&pObj->hAttr);` |
|      7 | 4074 | `		while( (pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){` |
|      5 | 4075 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      - | 4076 | `			ph7_value sName, sVal;` |
|      - | 4077 | `			ph7_value *apCtor[2];` |
|      - | 4078 | `			ph7_class_instance *pRef;` |
|      - | 4079 | `			sxi32 rc;` |
|      5 | 4080 | `			if( pVmAttr->pAttr == 0 \|\| (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) == 0 ){` |
|    ! 0 | 4081 | `				continue;` |
|      - | 4082 | `			}` |
|      5 | 4083 | `			PH7_MemObjInit(pCtx->pVm, &sName);` |
|      7 | 4084 | `			ph7_value_string(&sName, SyStringData(&pVmAttr->pAttr->sName),` |
|      4 | 4085 | `				(int)SyStringLength(&pVmAttr->pAttr->sName));` |
|      5 | 4086 | `			apCtor[0] = &sTarget;` |
|      5 | 4087 | `			apCtor[1] = &sName;` |
|      5 | 4088 | `			pRef = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|      5 | 4089 | `			PH7_MemObjRelease(&sName);` |
|      5 | 4090 | `			if( pRef == 0 ){` |
|    ! 0 | 4091 | `				break;` |
|      - | 4092 | `			}` |
|      5 | 4093 | `			PH7_MemObjInit(pCtx->pVm, &sVal);` |
|      5 | 4094 | `			sVal.x.pOther = pRef;` |
|      5 | 4095 | `			sVal.iFlags = MEMOBJ_OBJ;` |
|      5 | 4096 | `			ph7_array_add_elem(pOut, 0, &sVal);` |
|      5 | 4097 | `			PH7_ClassInstanceUnref(pRef);` |
|      1 | 4098 | `		}` |
|      - | 4099 | `		/* sTarget borrows pObj and never took a reference: nothing to release. */` |
|      1 | 4100 | `	}` |
|    123 | 4101 | `	PH7_MemObjRelease(&sSelf);` |
|    123 | 4102 | `	ph7_result_value(pCtx, pOut);` |
|    123 | 4103 | `	return PH7_OK;` |
|     63 | 4104 | `}` |
|     52 | 4105 | `static int vm_builtin_ReflectionClass_getMethods(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 4106 | `{` |
|     54 | 4107 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_METHOD, "ReflectionMethod", nArg, apArg);` |
|      2 | 4108 | `}` |
|     62 | 4109 | `static int vm_builtin_ReflectionClass_getProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 4110 | `{` |
|     65 | 4111 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_PROP, "ReflectionProperty", nArg, apArg);` |
|      3 | 4112 | `}` |
|      6 | 4113 | `static int vm_builtin_ReflectionClass_getReflectionConstants(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 4114 | `{` |
|      7 | 4115 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_CONST, "ReflectionClassConstant", nArg, apArg);` |
|      1 | 4116 | `}` |
|      8 | 4117 | `static int vm_builtin_ReflectionClass_getProperty(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 4118 | `{` |
|      9 | 4119 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      9 | 4120 | `	ph7_class_instance *pObj = ReflectClassObj(pCtx);` |
|      - | 4121 | `	const char *zName;` |
|      9 | 4122 | `	int nName, bFound = 0;` |
|      - | 4123 | `	SySet aMembers;` |
|      - | 4124 | `	sxu32 n;` |
|      - | 4125 | `	SyString sFound;` |
|      9 | 4126 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 4127 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4128 | `		return PH7_OK;` |
|      - | 4129 | `	}` |
|      9 | 4130 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|      9 | 4131 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|      9 | 4132 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     41 | 4133 | `	for( n = 0 ; n < SySetUsed(&aMembers) && !bFound ; n++ ){` |
|     33 | 4134 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     33 | 4135 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zName, nName) ){` |
|      3 | 4136 | `			sFound = pM->sKey;` |
|      3 | 4137 | `			bFound = 1;` |
|      1 | 4138 | `		}` |
|     17 | 4139 | `	}` |
|      9 | 4140 | `	SySetRelease(&aMembers);` |
|      9 | 4141 | `	if( bFound ){` |
|      - | 4142 | `		ph7_value sSelf;` |
|      - | 4143 | `		int rc;` |
|      3 | 4144 | `		ReflectSelfName(pCtx, &sSelf);` |
|      3 | 4145 | `		rc = ReflectResultMember(pCtx, "ReflectionProperty", &sSelf, &sFound);` |
|      3 | 4146 | `		PH7_MemObjRelease(&sSelf);` |
|      3 | 4147 | `		return rc;` |
|      - | 4148 | `	}` |
|      7 | 4149 | `	if( ReflectObjHasProp(pObj, zName, nName) ){` |
|      - | 4150 | `		/* A dynamic property: the reflector is built over the OBJECT, since the` |
|      - | 4151 | `		 * class declaration has no record of it. */` |
|      - | 4152 | `		ph7_value sTarget;` |
|      - | 4153 | `		SyString sName;` |
|      - | 4154 | `		int rc;` |
|      3 | 4155 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      3 | 4156 | `		sTarget.x.pOther = pObj;` |
|      3 | 4157 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|      3 | 4158 | `		SyStringInitFromBuf(&sName, zName, nName);` |
|      3 | 4159 | `		rc = ReflectResultMember(pCtx, "ReflectionProperty", &sTarget, &sName);` |
|      3 | 4160 | `		return rc;` |
|      - | 4161 | `	}` |
|      7 | 4162 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 | 4163 | `		"Property %z::$%.*s does not exist", &pClass->sName, nName, zName);` |
|      5 | 4164 | `}` |
|      4 | 4165 | `static int vm_builtin_ReflectionClass_getReflectionConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 4166 | `{` |
|      5 | 4167 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 4168 | `	const char *zName;` |
|      5 | 4169 | `	int nName, bFound = 0;` |
|      - | 4170 | `	SySet aMembers;` |
|      - | 4171 | `	sxu32 n;` |
|      - | 4172 | `	SyString sFound;` |
|      5 | 4173 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 4174 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 4175 | `		return PH7_OK;` |
|      - | 4176 | `	}` |
|      5 | 4177 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|      5 | 4178 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|      5 | 4179 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     15 | 4180 | `	for( n = 0 ; n < SySetUsed(&aMembers) && !bFound ; n++ ){` |
|     11 | 4181 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     11 | 4182 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|      3 | 4183 | `			sFound = pM->sKey;` |
|      3 | 4184 | `			bFound = 1;` |
|      1 | 4185 | `		}` |
|      6 | 4186 | `	}` |
|      5 | 4187 | `	SySetRelease(&aMembers);` |
|      5 | 4188 | `	if( !bFound ){` |
|      3 | 4189 | `		ph7_result_bool(pCtx, 0);` |
|      3 | 4190 | `		return PH7_OK;` |
|      - | 4191 | `	}` |
|      - | 4192 | `	{` |
|      - | 4193 | `		ph7_value sSelf;` |
|      - | 4194 | `		int rc;` |
|      3 | 4195 | `		ReflectSelfName(pCtx, &sSelf);` |
|      3 | 4196 | `		rc = ReflectResultMember(pCtx, "ReflectionClassConstant", &sSelf, &sFound);` |
|      3 | 4197 | `		PH7_MemObjRelease(&sSelf);` |
|      3 | 4198 | `		return rc;` |
|      - | 4199 | `	}` |
|      3 | 4200 | `}` |
|      - | 4201 | `/* ---- statics and defaults ---- */` |
|      - | 4202 | `/* Reading or writing a static through reflection materializes the class's` |
|      - | 4203 | `` * static table exactly as `C::$s` does, so a default that threw at the`` |
|      - | 4204 | ` * declaration raises HERE. */` |
|     50 | 4205 | `static sxi32 ReflectMaterializeStatics(ph7_context *pCtx, ph7_class *pClass)` |
|      2 | 4206 | `{` |
|     52 | 4207 | `	if( VmClassStaticDeferPending(pClass) ){` |
|     21 | 4208 | `		return PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);` |
|      - | 4209 | `	}` |
|     32 | 4210 | `	return SXRET_OK;` |
|     27 | 4211 | `}` |
|      6 | 4212 | `static int vm_builtin_ReflectionClass_getStaticProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 4213 | `{` |
|      8 | 4214 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      8 | 4215 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - | 4216 | `	SySet aMembers;` |
|      - | 4217 | `	sxu32 n;` |
|      3 | 4218 | `	SXUNUSED(nArg);` |
|      3 | 4219 | `	SXUNUSED(apArg);` |
|      8 | 4220 | `	if( pOut == 0 ){` |
|    ! 0 | 4221 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4222 | `	}` |
|      8 | 4223 | `	if( pClass == 0 ){` |
|    ! 0 | 4224 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 | 4225 | `		return PH7_OK;` |
|      - | 4226 | `	}` |
|      - | 4227 | `	{` |
|      8 | 4228 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|      8 | 4229 | `		if( rc != SXRET_OK ){` |
|      3 | 4230 | `			return rc;` |
|      - | 4231 | `		}` |
|      - | 4232 | `	}` |
|      5 | 4233 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|      5 | 4234 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     29 | 4235 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     25 | 4236 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - | 4237 | `		ph7_value *pVal;` |
|      - | 4238 | `		SyHashEntry *pSlot;` |
|     24 | 4239 | `		if( pM->iKind != REFLECT_MEMBER_PROP` |
|     24 | 4240 | `		 \|\| (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|     15 | 4241 | `			continue;` |
|      - | 4242 | `		}` |
|      - | 4243 | `		/* An UNINITIALIZED typed static has no value to report, and php simply` |
|      - | 4244 | `		 * leaves it out rather than raising the read Error here. */` |
|     11 | 4245 | `		pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pM->pAttr->nIdx, sizeof(sxu32));` |
|     11 | 4246 | `		if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|      3 | 4247 | `			continue;` |
|      - | 4248 | `		}` |
|      9 | 4249 | `		pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pM->pAttr->nIdx);` |
|      9 | 4250 | `		if( pVal ){` |
|      9 | 4251 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, pVal);` |
|      4 | 4252 | `		}` |
|      5 | 4253 | `	}` |
|      5 | 4254 | `	SySetRelease(&aMembers);` |
|      5 | 4255 | `	ph7_result_value(pCtx, pOut);` |
|      5 | 4256 | `	return PH7_OK;` |
|      5 | 4257 | `}` |
|      - | 4258 | `/* The declared STATIC property of this name, or NULL. */` |
|     18 | 4259 | `static ph7_class_attr * ReflectStaticAttr(ph7_context *pCtx, ph7_class *pClass,` |
|      - | 4260 | `	const char *zName, int nName)` |
|      2 | 4261 | `{` |
|      - | 4262 | `	SyHashEntry *pEntry;` |
|      - | 4263 | `	ph7_class_attr *pAttr;` |
|     20 | 4264 | `	if( nName < 1 ){` |
|    ! 0 | 4265 | `		return 0;` |
|      - | 4266 | `	}` |
|     20 | 4267 | `	pEntry = SyHashGet(&pClass->hAttr, (const void *)zName, (sxu32)nName);` |
|     20 | 4268 | `	if( pEntry == 0 ){` |
|      9 | 4269 | `		return 0;` |
|      - | 4270 | `	}` |
|     12 | 4271 | `	pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|      5 | 4272 | `	SXUNUSED(pCtx);` |
|     12 | 4273 | `	return (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? pAttr : 0;` |
|     11 | 4274 | `}` |
|     20 | 4275 | `static int vm_builtin_ReflectionClass_getStaticPropertyValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 4276 | `{` |
|     22 | 4277 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 4278 | `	ph7_class_attr *pAttr;` |
|      - | 4279 | `	const char *zName;` |
|      - | 4280 | `	int nName;` |
|     22 | 4281 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 4282 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4283 | `		return PH7_OK;` |
|      - | 4284 | `	}` |
|      - | 4285 | `	{` |
|     22 | 4286 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|     22 | 4287 | `		if( rc != SXRET_OK ){` |
|      7 | 4288 | `			return rc;` |
|      - | 4289 | `		}` |
|      - | 4290 | `	}` |
|     16 | 4291 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     16 | 4292 | `	pAttr = ReflectStaticAttr(pCtx, pClass, zName, nName);` |
|     16 | 4293 | `	if( pAttr == 0 ){` |
|      7 | 4294 | `		if( nArg > 1 ){` |
|      5 | 4295 | `			ph7_result_value(pCtx, apArg[1]);` |
|      5 | 4296 | `			return PH7_OK;` |
|      - | 4297 | `		}` |
|      4 | 4298 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 4299 | `			"Property %z::$%.*s does not exist", &pClass->sName, nName, zName);` |
|      - | 4300 | `	}` |
|      - | 4301 | `	{` |
|      - | 4302 | `		/* Uninitialized typed static: the same Error the VM raises on read */` |
|     10 | 4303 | `		SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|      - | 4304 | `		ph7_value *pVal;` |
|     10 | 4305 | `		if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|      - | 4306 | `			/* php says "Typed property" HERE and "Typed static property" from` |
|      - | 4307 | ``			 * ReflectionProperty::getValue() and from `A::$s` itself — the`` |
|      - | 4308 | `			 * three wordings were checked against the oracle, they really do` |
|      - | 4309 | `			 * differ by call site. */` |
|      3 | 4310 | `			ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|      4 | 4311 | `			return PH7_VmThrowException(pCtx, "Error",` |
|      - | 4312 | `				"Typed property %z::$%z must not be accessed before initialization",` |
|      1 | 4313 | `				&pDecl->sName, &pAttr->sName);` |
|      - | 4314 | `		}` |
|      8 | 4315 | `		pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      8 | 4316 | `		if( pVal ){` |
|      8 | 4317 | `			ph7_result_value(pCtx, pVal);` |
|      5 | 4318 | `		}else{` |
|    ! 0 | 4319 | `			ph7_result_null(pCtx);` |
|      - | 4320 | `		}` |
|      - | 4321 | `	}` |
|      8 | 4322 | `	return PH7_OK;` |
|     12 | 4323 | `}` |
|      8 | 4324 | `static int vm_builtin_ReflectionClass_setStaticPropertyValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 4325 | `{` |
|     10 | 4326 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 4327 | `	ph7_class_attr *pAttr;` |
|      - | 4328 | `	ph7_value *pSlot;` |
|      - | 4329 | `	const char *zName;` |
|      - | 4330 | `	int nName;` |
|     10 | 4331 | `	if( pClass == 0 \|\| nArg < 2 ){` |
|    ! 0 | 4332 | `		return PH7_OK;` |
|      - | 4333 | `	}` |
|      - | 4334 | `	{` |
|     10 | 4335 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|     10 | 4336 | `		if( rc != SXRET_OK ){` |
|      5 | 4337 | `			return rc;` |
|      - | 4338 | `		}` |
|      - | 4339 | `	}` |
|      5 | 4340 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|      5 | 4341 | `	pAttr = ReflectStaticAttr(pCtx, pClass, zName, nName);` |
|      5 | 4342 | `	if( pAttr == 0 ){` |
|      4 | 4343 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 4344 | `			"Class %z does not have a property named %.*s", &pClass->sName, nName, zName);` |
|      - | 4345 | `	}` |
|      3 | 4346 | `	pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      3 | 4347 | `	if( pSlot == 0 ){` |
|    ! 0 | 4348 | `		return PH7_OK;` |
|      - | 4349 | `	}` |
|      - | 4350 | `	{` |
|      3 | 4351 | `		sxi32 rc = ReflectEnforceStore(pCtx, pAttr->nIdx, apArg[1]);` |
|      3 | 4352 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 4353 | `			return rc;` |
|      - | 4354 | `		}` |
|      - | 4355 | `	}` |
|      3 | 4356 | `	PH7_MemObjStore(apArg[1], pSlot);` |
|      3 | 4357 | `	return PH7_OK;` |
|      6 | 4358 | `}` |
|      4 | 4359 | `static int vm_builtin_ReflectionClass_getDefaultProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 4360 | `{` |
|      5 | 4361 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      5 | 4362 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - | 4363 | `	SySet aMembers;` |
|      - | 4364 | `	sxu32 n;` |
|      - | 4365 | `	int iPass;` |
|      2 | 4366 | `	SXUNUSED(nArg);` |
|      2 | 4367 | `	SXUNUSED(apArg);` |
|      5 | 4368 | `	if( pOut == 0 ){` |
|    ! 0 | 4369 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4370 | `	}` |
|      5 | 4371 | `	if( pClass == 0 ){` |
|    ! 0 | 4372 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 | 4373 | `		return PH7_OK;` |
|      - | 4374 | `	}` |
|      5 | 4375 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|      5 | 4376 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|      - | 4377 | `	/* php reports the STATIC properties first, then the instance ones. */` |
|     13 | 4378 | `	for( iPass = 0 ; iPass < 2 ; iPass++ ){` |
|     57 | 4379 | `		for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|     49 | 4380 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - | 4381 | `			int bStatic;` |
|      - | 4382 | `			ph7_value sValue;` |
|     49 | 4383 | `			if( pM->iKind != REFLECT_MEMBER_PROP ){` |
|     18 | 4384 | `				continue;` |
|      - | 4385 | `			}` |
|     45 | 4386 | `			bStatic = (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|     45 | 4387 | `			if( bStatic != (iPass == 0) ){` |
|     23 | 4388 | `				continue;` |
|      - | 4389 | `			}` |
|     22 | 4390 | `			if( (pM->pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|     16 | 4391 | `			 && SySetUsed(&pM->pAttr->aByteCode) < 1 ){` |
|      - | 4392 | `				/* A TYPED property with no initializer has no default at all —` |
|      - | 4393 | `				 * it is uninitialized, and php leaves it out. An UNTYPED one` |
|      - | 4394 | `				 * without an initializer defaults to null and is listed. */` |
|      5 | 4395 | `				continue;` |
|      - | 4396 | `			}` |
|     19 | 4397 | `			PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     19 | 4398 | `			if( SySetUsed(&pM->pAttr->aByteCode) > 0 ){` |
|      - | 4399 | `				/* Same evaluation path the VM uses for omitted call arguments */` |
|     15 | 4400 | `				VmLocalExec(pCtx->pVm, &pM->pAttr->aByteCode, &sValue, FALSE);` |
|      7 | 4401 | `			}` |
|     19 | 4402 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, &sValue);` |
|     19 | 4403 | `			PH7_MemObjRelease(&sValue);` |
|     10 | 4404 | `		}` |
|      5 | 4405 | `	}` |
|      5 | 4406 | `	SySetRelease(&aMembers);` |
|      5 | 4407 | `	ph7_result_value(pCtx, pOut);` |
|      5 | 4408 | `	return PH7_OK;` |
|      3 | 4409 | `}` |
|      - | 4410 | `/* ---- attributes, extension, export ---- */` |
|     54 | 4411 | `static int vm_builtin_ReflectionClass_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 4412 | `{` |
|     56 | 4413 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 4414 | `	const char *zName;` |
|      - | 4415 | `	int nName;` |
|     56 | 4416 | `	if( pClass == 0 ){` |
|    ! 0 | 4417 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 | 4418 | `		return PH7_OK;` |
|      - | 4419 | `	}` |
|     56 | 4420 | `	ReflectClassName(pCtx, &zName, &nName);` |
|      - | 4421 | `	{` |
|      - | 4422 | `		ph7_value sTarget;` |
|      - | 4423 | `		int rc;` |
|     56 | 4424 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     56 | 4425 | `		ph7_value_string(&sTarget, zName, nName);` |
|      - | 4426 | `		/* 1 = Attribute::TARGET_CLASS */` |
|     83 | 4427 | `		rc = ReflectBuildAttrs(pCtx, &pClass->aAttrs, "class", &sTarget, 0, 0, 0, 1,` |
|     27 | 4428 | `			nArg, apArg);` |
|     56 | 4429 | `		PH7_MemObjRelease(&sTarget);` |
|     56 | 4430 | `		return rc;` |
|      - | 4431 | `	}` |
|     29 | 4432 | `}` |
|    ! 0 | 4433 | `static int vm_builtin_ReflectionClass_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 4434 | `{` |
|    ! 0 | 4435 | `	SXUNUSED(nArg);` |
|    ! 0 | 4436 | `	SXUNUSED(apArg);` |
|    ! 0 | 4437 | `	if( ReflectClassFlag(pCtx, PH7_CLASS_INTERNAL) ){` |
|    ! 0 | 4438 | `		ph7_result_string(pCtx, "Core", sizeof("Core")-1);` |
|    ! 0 | 4439 | `	}else{` |
|    ! 0 | 4440 | `		ph7_result_bool(pCtx, 0);` |
|      - | 4441 | `	}` |
|    ! 0 | 4442 | `	return PH7_OK;` |
|    ! 0 | 4443 | `}` |
|      - | 4444 | `/*` |
|      - | 4445 | ` * The synthetic "Core" extension, which is the only one PHL has. Answered by` |
|      - | 4446 | ` * every reflector's getExtension() for an INTERNAL target.` |
|      - | 4447 | ` */` |
|      6 | 4448 | `static int ReflectCoreExtension(ph7_context *pCtx)` |
|      1 | 4449 | `{` |
|      - | 4450 | `	ph7_value sName;` |
|      - | 4451 | `	ph7_value *apCtor[1];` |
|      - | 4452 | `	ph7_class_instance *pExt;` |
|      - | 4453 | `	sxi32 rc;` |
|      7 | 4454 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|      7 | 4455 | `	ph7_value_string(&sName, "Core", sizeof("Core")-1);` |
|      7 | 4456 | `	apCtor[0] = &sName;` |
|      7 | 4457 | `	pExt = ReflectConstruct(pCtx, "ReflectionExtension", 1, apCtor, &rc);` |
|      7 | 4458 | `	PH7_MemObjRelease(&sName);` |
|      7 | 4459 | `	if( pExt == 0 ){` |
|    ! 0 | 4460 | `		if( rc != PH7_OK ){` |
|    ! 0 | 4461 | `			return rc;` |
|      - | 4462 | `		}` |
|    ! 0 | 4463 | `		ph7_result_null(pCtx);` |
|    ! 0 | 4464 | `		return PH7_OK;` |
|      - | 4465 | `	}` |
|      7 | 4466 | `	return ReflectResultObject(pCtx, pExt);` |
|      4 | 4467 | `}` |
|      - | 4468 | `/* php's export format (chunk 9) — defined at the END of this file, where the` |
|      - | 4469 | ` * member walk, the function reference and the parameter description it reads` |
|      - | 4470 | ` * are all in scope. */` |
|      - | 4471 | `static int ReflectExportClassSelf(ph7_context *pCtx);` |
|      - | 4472 | `static int ReflectExportFuncSelf(ph7_context *pCtx);` |
|      - | 4473 | `static int ReflectExportParamSelf(ph7_context *pCtx);` |
|      - | 4474 | `static int ReflectExportPropSelf(ph7_context *pCtx);` |
|      - | 4475 | `static int ReflectExportConstSelf(ph7_context *pCtx);` |
|      - | 4476 | `/*` |
|      - | 4477 | ` * __toString(): php's export format, still chunk 9 — a PHP function written` |
|      - | 4478 | `` * against the PUBLIC reflection API of its target, so it is called with `$this`.`` |
|      - | 4479 | ` * bIndentArg adds the export family's second "" indent argument.` |
|      - | 4480 | ` */` |
|      4 | 4481 | `static int vm_builtin_ReflectionClass_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 4482 | `{` |
|      2 | 4483 | `	SXUNUSED(nArg);` |
|      2 | 4484 | `	SXUNUSED(apArg);` |
|      5 | 4485 | `	if( !ReflectClassFlag(pCtx, PH7_CLASS_INTERNAL) ){` |
|      3 | 4486 | `		ph7_result_null(pCtx);` |
|      3 | 4487 | `		return PH7_OK;` |
|      - | 4488 | `	}` |
|      3 | 4489 | `	return ReflectCoreExtension(pCtx);` |
|      3 | 4490 | `}` |
|     28 | 4491 | `static int vm_builtin_ReflectionClass_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 4492 | `{` |
|     14 | 4493 | `	SXUNUSED(nArg);` |
|     14 | 4494 | `	SXUNUSED(apArg);` |
|     29 | 4495 | `	return ReflectExportClassSelf(pCtx);` |
|      1 | 4496 | `}` |
|      - | 4497 | `/* ---- lazy objects: PHL has none (§7.4) ---- */` |
|    ! 0 | 4498 | `static int ReflectNoLazy(ph7_context *pCtx, const char *zWho)` |
|    ! 0 | 4499 | `{` |
|    ! 0 | 4500 | `	return PH7_VmThrowException(pCtx, "Error",` |
|    ! 0 | 4501 | `		"%s is not supported by PHL (no lazy objects)", zWho);` |
|    ! 0 | 4502 | `}` |
|      - | 4503 | `#define REFLECT_NO_LAZY(NAME,TEXT) \` |
|      - | 4504 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 4505 | `	{ \` |
|      - | 4506 | `		SXUNUSED(nArg); \` |
|      - | 4507 | `		SXUNUSED(apArg); \` |
|      - | 4508 | `		return ReflectNoLazy(pCtx, TEXT); \` |
|      - | 4509 | `	}` |
|    ! 0 | 4510 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_newLazyGhost,"ReflectionClass::newLazyGhost()")` |
|    ! 0 | 4511 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_newLazyProxy,"ReflectionClass::newLazyProxy()")` |
|    ! 0 | 4512 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_resetAsLazyGhost,"ReflectionClass::resetAsLazyGhost()")` |
|    ! 0 | 4513 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_resetAsLazyProxy,"ReflectionClass::resetAsLazyProxy()")` |
|      - | 4514 | `/* The four QUERIES answer what is true of a VM with no lazy objects, rather` |
|      - | 4515 | ` * than refusing: an object here is always initialized and never has one. */` |
|    ! 0 | 4516 | `static int vm_builtin_ReflectionClass_getLazyInitializer(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 4517 | `{` |
|    ! 0 | 4518 | `	SXUNUSED(nArg);` |
|    ! 0 | 4519 | `	SXUNUSED(apArg);` |
|    ! 0 | 4520 | `	ph7_result_null(pCtx);` |
|    ! 0 | 4521 | `	return PH7_OK;` |
|    ! 0 | 4522 | `}` |
|    ! 0 | 4523 | `static int vm_builtin_ReflectionClass_passThroughObject(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 4524 | `{` |
|    ! 0 | 4525 | `	if( nArg > 0 ){` |
|    ! 0 | 4526 | `		ph7_result_value(pCtx, apArg[0]);` |
|    ! 0 | 4527 | `	}else{` |
|    ! 0 | 4528 | `		ph7_result_null(pCtx);` |
|      - | 4529 | `	}` |
|    ! 0 | 4530 | `	return PH7_OK;` |
|    ! 0 | 4531 | `}` |
|    ! 0 | 4532 | `static int vm_builtin_ReflectionClass_isUninitializedLazyObject(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 4533 | `{` |
|    ! 0 | 4534 | `	SXUNUSED(nArg);` |
|    ! 0 | 4535 | `	SXUNUSED(apArg);` |
|    ! 0 | 4536 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 | 4537 | `	return PH7_OK;` |
|    ! 0 | 4538 | `}` |
|      - | 4539 | `/*` |
|      - | 4540 | ` * Reflection::getModifierNames(int $modifiers)` |
|      - | 4541 | ` *` |
|      - | 4542 | ` * php's own order and its own SWITCH: the visibility names come out of one` |
|      - | 4543 | ` * three-way choice and the set-visibility names out of another, so a mask with` |
|      - | 4544 | ` * two visibility bits set names NEITHER (which is what php answers).` |
|      - | 4545 | ` */` |
|     66 | 4546 | `static int vm_builtin_Reflection_getModifierNames(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 4547 | `{` |
|     68 | 4548 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     68 | 4549 | `	sxi64 iMods = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|      - | 4550 | `	const char *azName[8];` |
|     68 | 4551 | `	int nName = 0, n;` |
|     68 | 4552 | `	if( pOut == 0 ){` |
|    ! 0 | 4553 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 4554 | `	}` |
|     68 | 4555 | `	if( iMods & 64 ){ azName[nName++] = "abstract"; }` |
|     68 | 4556 | `	if( iMods & 32 ){ azName[nName++] = "final"; }` |
|     68 | 4557 | `	if( iMods & 512 ){ azName[nName++] = "virtual"; }` |
|     68 | 4558 | `	switch( iMods & (1\|2\|4) ){` |
|     34 | 4559 | `	case 1: azName[nName++] = "public"; break;` |
|     17 | 4560 | `	case 2: azName[nName++] = "protected"; break;` |
|     11 | 4561 | `	case 4: azName[nName++] = "private"; break;` |
|      8 | 4562 | `	default: break;` |
|      - | 4563 | `	}` |
|     68 | 4564 | `	switch( iMods & (2048\|4096) ){` |
|     12 | 4565 | `	case 2048: azName[nName++] = "protected(set)"; break;` |
|      3 | 4566 | `	case 4096: azName[nName++] = "private(set)"; break;` |
|     54 | 4567 | `	default: break;` |
|      - | 4568 | `	}` |
|     68 | 4569 | `	if( iMods & 16 ){ azName[nName++] = "static"; }` |
|     68 | 4570 | `	if( iMods & 128 ){ azName[nName++] = "readonly"; }` |
|    174 | 4571 | `	for( n = 0 ; n < nName ; n++ ){` |
|    108 | 4572 | `		ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|    108 | 4573 | `		if( pName == 0 ){ break; }` |
|    108 | 4574 | `		ph7_value_string(pName, azName[n], -1);` |
|    108 | 4575 | `		ph7_array_add_elem(pOut, 0, pName);` |
|     55 | 4576 | `	}` |
|     68 | 4577 | `	ph7_result_value(pCtx, pOut);` |
|     68 | 4578 | `	return PH7_OK;` |
|     35 | 4579 | `}` |
|      - | 4580 | `/*` |
|      - | 4581 | ` * Declare chunk 1. Called from PH7_VmInstallReflection() where it used to be` |
|      - | 4582 | `` * compiled — before chunks 2 and 3, which name `Reflector` in their own`` |
|      - | 4583 | `` * `implements` clauses.`` |
|      - | 4584 | ` *` |
|      - | 4585 | ` * The method table is in php's own DECLARATION order, which is the order` |
|      - | 4586 | ` * ReflectionClass::getMethods() reports for these classes themselves.` |
|      - | 4587 | ` */` |
|   5254 | 4588 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionClass(ph7_vm *pVm)` |
|      5 | 4589 | `{` |
|      - | 4590 | `	static const PH7_NativePropDef aClassProp[] = {` |
|      - | 4591 | `		{ "name",  PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 4592 | `		/* PHL-only: the instance a ReflectionObject was built over. php keeps it` |
|      - | 4593 | `		 * out of sight; PHL has no hidden-slot bit yet (§7.4 (e)). */` |
|      - | 4594 | `		{ RC_OBJ,  PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 4595 | `	};` |
|      - | 4596 | `	static const PH7_NativeMethodDef aClassMethod[] = {` |
|      - | 4597 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionClass_clone },` |
|      - | 4598 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $objectOrClass", "",` |
|      - | 4599 | `		  vm_builtin_ReflectionClass_construct },` |
|      - | 4600 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionClass_toString },` |
|      - | 4601 | `		{ "getName",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getName },` |
|      - | 4602 | `		{ "isInternal",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInternal },` |
|      - | 4603 | `		{ "isUserDefined", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isUserDefined },` |
|      - | 4604 | `		{ "isAnonymous",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isAnonymous },` |
|      - | 4605 | `		{ "isInstantiable",PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInstantiable },` |
|      - | 4606 | `		{ "isCloneable",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isCloneable },` |
|      - | 4607 | `		{ "getFileName",   PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getFileName },` |
|      - | 4608 | `		{ "getStartLine",  PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionClass_getStartLine },` |
|      - | 4609 | `		{ "getEndLine",    PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionClass_getEndLine },` |
|      - | 4610 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getDocComment },` |
|      - | 4611 | `		{ "getConstructor",PH7_MOD_PUBLIC, "", "@?ReflectionMethod", vm_builtin_ReflectionClass_getConstructor },` |
|      - | 4612 | `		{ "hasMethod",     PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasMethod },` |
|      - | 4613 | `		{ "getMethod",     PH7_MOD_PUBLIC, "string $name", "@ReflectionMethod", vm_builtin_ReflectionClass_getMethod },` |
|      - | 4614 | `		{ "getMethods",    PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - | 4615 | `		  vm_builtin_ReflectionClass_getMethods },` |
|      - | 4616 | `		{ "hasProperty",   PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasProperty },` |
|      - | 4617 | `		{ "getProperty",   PH7_MOD_PUBLIC, "string $name", "@ReflectionProperty", vm_builtin_ReflectionClass_getProperty },` |
|      - | 4618 | `		{ "getProperties", PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - | 4619 | `		  vm_builtin_ReflectionClass_getProperties },` |
|      - | 4620 | `		{ "hasConstant",   PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasConstant },` |
|      - | 4621 | `		{ "getConstants",  PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - | 4622 | `		  vm_builtin_ReflectionClass_getConstants },` |
|      - | 4623 | `		{ "getReflectionConstants", PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|      - | 4624 | `		  vm_builtin_ReflectionClass_getReflectionConstants },` |
|      - | 4625 | `		{ "getConstant",   PH7_MOD_PUBLIC, "string $name", "@mixed", vm_builtin_ReflectionClass_getConstant },` |
|      - | 4626 | `		{ "getReflectionConstant", PH7_MOD_PUBLIC, "string $name", "@ReflectionClassConstant\|false",` |
|      - | 4627 | `		  vm_builtin_ReflectionClass_getReflectionConstant },` |
|      - | 4628 | `		{ "getInterfaces",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getInterfaces },` |
|      - | 4629 | `		{ "getInterfaceNames", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getInterfaceNames },` |
|      - | 4630 | `		{ "isInterface",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInterface },` |
|      - | 4631 | `		{ "getTraits",         PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraits },` |
|      - | 4632 | `		{ "getTraitNames",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraitNames },` |
|      - | 4633 | `		{ "getTraitAliases",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraitAliases },` |
|      - | 4634 | `		{ "isTrait",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isTrait },` |
|      - | 4635 | `		{ "isEnum",            PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClass_isEnum },` |
|      - | 4636 | `		{ "isAbstract",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isAbstract },` |
|      - | 4637 | `		{ "isFinal",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isFinal },` |
|      - | 4638 | `		{ "isReadOnly",        PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClass_isReadOnly },` |
|      - | 4639 | `		{ "getModifiers",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionClass_getModifiers },` |
|      - | 4640 | `		{ "isInstance",        PH7_MOD_PUBLIC, "object $object", "@bool",` |
|      - | 4641 | `		  vm_builtin_ReflectionClass_isInstance },` |
|      - | 4642 | `		{ "newInstance",       PH7_MOD_PUBLIC, "mixed ...$args", "@object",` |
|      - | 4643 | `		  vm_builtin_ReflectionClass_newInstance },` |
|      - | 4644 | `		{ "newInstanceWithoutConstructor", PH7_MOD_PUBLIC, "", "@object",` |
|      - | 4645 | `		  vm_builtin_ReflectionClass_newInstanceWithoutConstructor },` |
|      - | 4646 | `		{ "newInstanceArgs",   PH7_MOD_PUBLIC, "array $args = []", "@?object",` |
|      - | 4647 | `		  vm_builtin_ReflectionClass_newInstanceArgs },` |
|      - | 4648 | `		{ "newLazyGhost",      PH7_MOD_PUBLIC, "callable $initializer, int $options = 0", "object",` |
|      - | 4649 | `		  vm_builtin_ReflectionClass_newLazyGhost },` |
|      - | 4650 | `		{ "newLazyProxy",      PH7_MOD_PUBLIC, "callable $factory, int $options = 0", "object",` |
|      - | 4651 | `		  vm_builtin_ReflectionClass_newLazyProxy },` |
|      - | 4652 | `		{ "resetAsLazyGhost",  PH7_MOD_PUBLIC,` |
|      - | 4653 | `		  "object $object, callable $initializer, int $options = 0", "void",` |
|      - | 4654 | `		  vm_builtin_ReflectionClass_resetAsLazyGhost },` |
|      - | 4655 | `		{ "resetAsLazyProxy",  PH7_MOD_PUBLIC,` |
|      - | 4656 | `		  "object $object, callable $factory, int $options = 0", "void",` |
|      - | 4657 | `		  vm_builtin_ReflectionClass_resetAsLazyProxy },` |
|      - | 4658 | `		{ "initializeLazyObject", PH7_MOD_PUBLIC, "object $object", "object",` |
|      - | 4659 | `		  vm_builtin_ReflectionClass_passThroughObject },` |
|      - | 4660 | `		{ "isUninitializedLazyObject", PH7_MOD_PUBLIC, "object $object", "bool",` |
|      - | 4661 | `		  vm_builtin_ReflectionClass_isUninitializedLazyObject },` |
|      - | 4662 | `		{ "markLazyObjectAsInitialized", PH7_MOD_PUBLIC, "object $object", "object",` |
|      - | 4663 | `		  vm_builtin_ReflectionClass_passThroughObject },` |
|      - | 4664 | `		{ "getLazyInitializer", PH7_MOD_PUBLIC, "object $object", "?callable",` |
|      - | 4665 | `		  vm_builtin_ReflectionClass_getLazyInitializer },` |
|      - | 4666 | `		{ "getParentClass",    PH7_MOD_PUBLIC, "", "@ReflectionClass\|false", vm_builtin_ReflectionClass_getParentClass },` |
|      - | 4667 | `		{ "isSubclassOf",      PH7_MOD_PUBLIC, "ReflectionClass\|string $class", "@bool",` |
|      - | 4668 | `		  vm_builtin_ReflectionClass_isSubclassOf },` |
|      - | 4669 | `		{ "getStaticProperties", PH7_MOD_PUBLIC, "", "@array",` |
|      - | 4670 | `		  vm_builtin_ReflectionClass_getStaticProperties },` |
|      - | 4671 | ``		/* `mixed $default = ?` is the table's "optional, no default VALUE"`` |
|      - | 4672 | `		 * marker — php's own shape here: isOptional() true,` |
|      - | 4673 | `		 * isDefaultValueAvailable() false. */` |
|      - | 4674 | `		{ "getStaticPropertyValue", PH7_MOD_PUBLIC, "string $name, mixed $default = ?", "@mixed",` |
|      - | 4675 | `		  vm_builtin_ReflectionClass_getStaticPropertyValue },` |
|      - | 4676 | `		{ "setStaticPropertyValue", PH7_MOD_PUBLIC, "string $name, mixed $value", "@void",` |
|      - | 4677 | `		  vm_builtin_ReflectionClass_setStaticPropertyValue },` |
|      - | 4678 | `		{ "getDefaultProperties", PH7_MOD_PUBLIC, "", "@array",` |
|      - | 4679 | `		  vm_builtin_ReflectionClass_getDefaultProperties },` |
|      - | 4680 | `		{ "isIterable",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isIterable },` |
|      - | 4681 | `		{ "isIterateable",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isIterable },` |
|      - | 4682 | `		{ "implementsInterface", PH7_MOD_PUBLIC, "ReflectionClass\|string $interface", "@bool",` |
|      - | 4683 | `		  vm_builtin_ReflectionClass_implementsInterface },` |
|      - | 4684 | `		{ "getExtension",      PH7_MOD_PUBLIC, "", "@?ReflectionExtension", vm_builtin_ReflectionClass_getExtension },` |
|      - | 4685 | `		{ "getExtensionName",  PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getExtensionName },` |
|      - | 4686 | `		{ "inNamespace",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_inNamespace },` |
|      - | 4687 | `		{ "getNamespaceName",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getNamespaceName },` |
|      - | 4688 | `		{ "getShortName",      PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getShortName },` |
|      - | 4689 | `		{ "getAttributes",     PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - | 4690 | `		  vm_builtin_ReflectionClass_getAttributes },` |
|      - | 4691 | `	};` |
|      - | 4692 | `	static const PH7_NativeConstDef aClassConst[] = {` |
|      - | 4693 | `		{ "IS_IMPLICIT_ABSTRACT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,    0, 0.0 },` |
|      - | 4694 | `		{ "IS_EXPLICIT_ABSTRACT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64,    0, 0.0 },` |
|      - | 4695 | `		{ "IS_FINAL",             PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32,    0, 0.0 },` |
|      - | 4696 | `		{ "IS_READONLY",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 65536, 0, 0.0 },` |
|      - | 4697 | `		{ "SKIP_INITIALIZATION_ON_SERIALIZE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 8, 0, 0.0 },` |
|      - | 4698 | `		{ "SKIP_DESTRUCTOR",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,    0, 0.0 },` |
|      - | 4699 | `	};` |
|      - | 4700 | `	static const PH7_NativeMethodDef aObjectMethod[] = {` |
|      - | 4701 | `		{ "__construct", PH7_MOD_PUBLIC, "object $object", "",` |
|      - | 4702 | `		  vm_builtin_ReflectionObject_construct },` |
|      - | 4703 | `	};` |
|      - | 4704 | `	static const PH7_NativeMethodDef aReflectionMethod[] = {` |
|      - | 4705 | `		{ "getModifierNames", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "int $modifiers", "@array",` |
|      - | 4706 | `		  vm_builtin_Reflection_getModifierNames },` |
|      - | 4707 | `	};` |
|      - | 4708 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 4709 | `		{ "Reflector", "Stringable", 0, PH7_CLASS_INTERFACE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 4710 | `		{ "ReflectionException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 4711 | `		{ "Reflection", 0, 0, 0,` |
|      - | 4712 | `		  aReflectionMethod, SX_ARRAYSIZE(aReflectionMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 4713 | ``		/* Uncloneable and unserializable in php too: `clone` is an Error and`` |
|      - | 4714 | `		 * serialize() a catchable Exception naming the class. */` |
|      - | 4715 | `		{ "ReflectionClass", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 4716 | `		  aClassMethod, SX_ARRAYSIZE(aClassMethod),` |
|      - | 4717 | `		  aClassConst, SX_ARRAYSIZE(aClassConst),` |
|      - | 4718 | `		  aClassProp, SX_ARRAYSIZE(aClassProp), 0, 0, 0 },` |
|      - | 4719 | `		{ "ReflectionObject", "ReflectionClass", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 4720 | `		  aObjectMethod, SX_ARRAYSIZE(aObjectMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 4721 | `	};` |
|   5259 | 4722 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 | 4723 | `}` |
|      - | 4724 | `/*` |
|      - | 4725 | ` * ---------------------------------------------------------------------------` |
|      - | 4726 | ` * ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,` |
|      - | 4727 | ` * ReflectionParameter.` |
|      - | 4728 | ` *` |
|      - | 4729 | ` * Chunk 2. Its accessors funnelled through __reflect_func_info(), a descriptor` |
|      - | 4730 | ` * ARRAY built per call from either a compiled ph7_vm_func or a declared` |
|      - | 4731 | ` * SIGNATURE STRING; a native method reads whichever of the two the target` |
|      - | 4732 | ` * actually has, through the one uniform description both already agreed on` |
|      - | 4733 | ` * (ReflectParamDesc / ReflectSigDescribe).` |
|      - | 4734 | ` *` |
|      - | 4735 | ` * Five thunks retire with the chunk: __reflect_func_info, __reflect_invoke,` |
|      - | 4736 | ` * __reflect_closure, __reflect_param_default and __reflect_param_defconst.` |
|      - | 4737 | ` * ---------------------------------------------------------------------------` |
|      - | 4738 | ` */` |
|      - | 4739 | `#define RF_CL "__cl"     /* ReflectionFunctionAbstract: the reflected Closure  */` |
|      - | 4740 | `#define RP_T  "__t"      /* ReflectionParameter: the function/class it belongs to */` |
|      - | 4741 | `#define RP_M  "__m"      /* ReflectionParameter: the method name, or null      */` |
|      - | 4742 | `#define RP_P  "__p"      /* ReflectionParameter: the position                  */` |
|      - | 4743 |  |
|      - | 4744 | `/* Everything a reflected function IS, resolved once per call. */` |
|      - | 4745 | `typedef struct ReflectFuncRef ReflectFuncRef;` |
|      - | 4746 | `struct ReflectFuncRef` |
|      - | 4747 | `{` |
|      - | 4748 | `	ph7_vm_func *pFunc;           /* compiled body (NULL for a pure C builtin) */` |
|      - | 4749 | `	ph7_user_func *pHost;         /* C builtin (NULL otherwise) */` |
|      - | 4750 | `	ph7_class *pClass;            /* class a METHOD was reached through */` |
|      - | 4751 | `	ph7_class_method *pMeth;      /* the method */` |
|      - | 4752 | `	ph7_class_instance *pClosure; /* the Closure being reflected, if any */` |
|      - | 4753 | `	const char *zSig;             /* declared parameter signature, or NULL */` |
|      - | 4754 | `	const char *zRet;             /* declared return type, or NULL */` |
|      - | 4755 | `};` |
|      - | 4756 | `/*` |
|      - | 4757 | ` * Resolve a callable into the reference, and work out WHICH of the two` |
|      - | 4758 | ` * parameter sources describes it: a declared signature string (a C builtin, a` |
|      - | 4759 | ` * native method, or an embedded-PHP builtin declared argless over` |
|      - | 4760 | ` * func_get_args()) wins over the compiled argument list, exactly as` |
|      - | 4761 | ` * ReflectSigFixup made it win in the descriptor.` |
|      - | 4762 | ` */` |
|   4070 | 4763 | `static int ReflectFuncFill(ph7_vm *pVm, ph7_value *pTarget, ph7_value *pMethodArg,` |
|      - | 4764 | `	ReflectFuncRef *pOut)` |
|      5 | 4765 | `{` |
|   4075 | 4766 | `	SyZero(pOut, sizeof(*pOut));` |
|   6110 | 4767 | `	pOut->pFunc = ReflectResolveCallable(pVm, pTarget, pMethodArg,` |
|   2035 | 4768 | `		&pOut->pClass, &pOut->pMeth, &pOut->pHost, &pOut->pClosure);` |
|   4075 | 4769 | `	if( pOut->pFunc == 0 && pOut->pHost == 0 ){` |
|     17 | 4770 | `		return 0;` |
|      - | 4771 | `	}` |
|   4061 | 4772 | `	if( pOut->pFunc == 0 ){` |
|   1466 | 4773 | `		pOut->zSig = pOut->pHost->zSig;` |
|   1466 | 4774 | `		pOut->zRet = pOut->pHost->zRet;` |
|   1466 | 4775 | `		return 1;` |
|      - | 4776 | `	}` |
|   2597 | 4777 | `	if( (pOut->pFunc->iFlags & VM_FUNC_NATIVE) && pOut->pFunc->pNative ){` |
|   1402 | 4778 | `		pOut->zSig = pOut->pFunc->pNative->zSig;` |
|   1402 | 4779 | `		if( SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|   1402 | 4780 | `			pOut->zRet = pOut->pFunc->pNative->zRet;` |
|    700 | 4781 | `		}` |
|   1894 | 4782 | `	}else if( (pOut->pFunc->iFlags & VM_FUNC_INTERNAL)` |
|    672 | 4783 | `	 && SySetUsed(&pOut->pFunc->aArgs) == 0 && pOut->pMeth == 0 ){` |
|    ! 0 | 4784 | `		const char *zRet = 0;` |
|    ! 0 | 4785 | `		pOut->zSig = PH7_VmBuiltinSigLookup(SyStringData(&pOut->pFunc->sName),` |
|    ! 0 | 4786 | `			SyStringLength(&pOut->pFunc->sName), &zRet);` |
|    ! 0 | 4787 | `		if( zRet && SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|    ! 0 | 4788 | `			pOut->zRet = zRet;` |
|    ! 0 | 4789 | `		}` |
|    ! 0 | 4790 | `	}` |
|   2597 | 4791 | `	if( pOut->zSig && pOut->zSig[0] == '\0' ){` |
|      - | 4792 | `		/* A declared-EMPTY signature really is "no parameters", not "undescribed". */` |
|    418 | 4793 | `		return 1;` |
|      - | 4794 | `	}` |
|   2181 | 4795 | `	return 1;` |
|   2040 | 4796 | `}` |
|      - | 4797 | `/* Is this receiver a ReflectionMethod (rather than a ReflectionFunction)? */` |
|   2004 | 4798 | `static int ReflectIsMethodReflector(ph7_context *pCtx, ph7_class_instance *pThis)` |
|      5 | 4799 | `{` |
|      - | 4800 | `	ph7_class *pRM;` |
|   2009 | 4801 | `	if( pThis == 0 ){` |
|    ! 0 | 4802 | `		return 0;` |
|      - | 4803 | `	}` |
|   2009 | 4804 | `	pRM = PH7_VmExtractClass(pCtx->pVm, "ReflectionMethod", sizeof("ReflectionMethod")-1, FALSE, 0);` |
|   2009 | 4805 | `	return pRM != 0 && PH7_VmInstanceOf(pThis->pClass, pRM);` |
|   1007 | 4806 | `}` |
|      - | 4807 | `/*` |
|      - | 4808 | `` * The function `$this` reflects. A ReflectionFunction built over a Closure keeps`` |
|      - | 4809 | `` * the object in `__cl` and resolves through it (its `$__fn` is a lambda name no`` |
|      - | 4810 | ` * function table holds); a ReflectionMethod resolves ($this->class, $this->name).` |
|      - | 4811 | ` */` |
|   1654 | 4812 | `static int ReflectFuncOfThis(ph7_context *pCtx, ReflectFuncRef *pOut)` |
|      5 | 4813 | `{` |
|   1659 | 4814 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 4815 | `	ph7_class_instance *pClo;` |
|      - | 4816 | `	ph7_value sTarget, sMethod;` |
|      - | 4817 | `	const char *zName, *zClass;` |
|      - | 4818 | `	int nName, nClass, rc;` |
|   1659 | 4819 | `	SyZero(pOut, sizeof(*pOut));` |
|   1659 | 4820 | `	if( pThis == 0 ){` |
|    ! 0 | 4821 | `		return 0;` |
|      - | 4822 | `	}` |
|   1659 | 4823 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|   1659 | 4824 | `	PH7_MemObjInit(pCtx->pVm, &sMethod);` |
|   1659 | 4825 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|   1659 | 4826 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|   1659 | 4827 | `	if( pClo ){` |
|     89 | 4828 | `		sTarget.x.pOther = pClo;` |
|     89 | 4829 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|   1616 | 4830 | `	}else if( ReflectIsMethodReflector(pCtx, pThis) ){` |
|   1015 | 4831 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|   1015 | 4832 | `		ph7_value_string(&sTarget, zClass, nClass);` |
|   1015 | 4833 | `		ph7_value_string(&sMethod, zName, nName);` |
|    510 | 4834 | `	}else{` |
|    560 | 4835 | `		ph7_value_string(&sTarget, zName, nName);` |
|      - | 4836 | `	}` |
|   1659 | 4837 | `	rc = ReflectFuncFill(pCtx->pVm, &sTarget, (sMethod.iFlags & MEMOBJ_STRING) ? &sMethod : 0, pOut);` |
|      - | 4838 | `	/* sTarget only ever BORROWS the closure; releasing it would unref twice. */` |
|   1659 | 4839 | `	if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|   1573 | 4840 | `		PH7_MemObjRelease(&sTarget);` |
|    784 | 4841 | `	}` |
|   1659 | 4842 | `	PH7_MemObjRelease(&sMethod);` |
|   1659 | 4843 | `	return rc;` |
|    832 | 4844 | `}` |
|      - | 4845 | `/* How many parameters the target declares. */` |
|   1428 | 4846 | `static int ReflectParamCount(const ReflectFuncRef *pRef)` |
|      5 | 4847 | `{` |
|   1433 | 4848 | `	if( pRef->zSig ){` |
|    947 | 4849 | `		return ReflectSigPart(pRef->zSig, (int)SyStrlen(pRef->zSig), -1, 0, 0);` |
|      - | 4850 | `	}` |
|    488 | 4851 | `	return pRef->pFunc ? (int)SySetUsed(&pRef->pFunc->aArgs) : 0;` |
|    719 | 4852 | `}` |
|      - | 4853 | `/* Describe the iPos-th parameter. Answers 0 when there is none. */` |
|   2248 | 4854 | `static int ReflectParamAt(const ReflectFuncRef *pRef, int iPos, ReflectParamDesc *pOut)` |
|      4 | 4855 | `{` |
|   2252 | 4856 | `	SyZero(pOut, sizeof(*pOut));` |
|   2252 | 4857 | `	if( iPos < 0 ){` |
|    ! 0 | 4858 | `		return 0;` |
|      - | 4859 | `	}` |
|   2252 | 4860 | `	if( pRef->zSig ){` |
|   1559 | 4861 | `		const char *zPart = 0;` |
|   1559 | 4862 | `		int nPart = 0, nTotal;` |
|   1559 | 4863 | `		nTotal = ReflectSigPart(pRef->zSig, (int)SyStrlen(pRef->zSig), iPos, &zPart, &nPart);` |
|   1559 | 4864 | `		if( iPos >= nTotal \|\| zPart == 0 ){` |
|      3 | 4865 | `			return 0;` |
|      - | 4866 | `		}` |
|   1557 | 4867 | `		ReflectSigDescribe(zPart, nPart, iPos, pOut);` |
|   1557 | 4868 | `		return 1;` |
|      - | 4869 | `	}` |
|    695 | 4870 | `	if( pRef->pFunc == 0 ){` |
|    ! 0 | 4871 | `		return 0;` |
|      - | 4872 | `	}` |
|      - | 4873 | `	{` |
|    695 | 4874 | `		ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pRef->pFunc->aArgs, (sxu32)iPos);` |
|    695 | 4875 | `		if( pArg == 0 ){` |
|     13 | 4876 | `			return 0;` |
|      - | 4877 | `		}` |
|    683 | 4878 | `		pOut->iPos = iPos;` |
|    683 | 4879 | `		pOut->sName = pArg->sName;` |
|    683 | 4880 | `		pOut->bByRef = (pArg->iFlags & VM_FUNC_ARG_BY_REF) != 0;` |
|    683 | 4881 | `		pOut->bVariadic = (pArg->iFlags & VM_FUNC_ARG_VARIADIC) != 0;` |
|      - | 4882 | `		/* The compiler never sets ARG_HAS_DEF; a default IS compiled byte-code` |
|      - | 4883 | `		 * (the same test the OP_CALL default-value path uses). */` |
|    683 | 4884 | `		pOut->bHasDef = SySetUsed(&pArg->aByteCode) > 0;` |
|    683 | 4885 | `		pOut->bNullable = (pArg->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|    683 | 4886 | `		pOut->bPromoted = (pArg->iFlags & VM_FUNC_ARG_PROMOTED) != 0;` |
|    683 | 4887 | `		pOut->bOptional = pOut->bVariadic \|\| pOut->bHasDef;` |
|    683 | 4888 | `		pOut->sType = pArg->sTypeName;` |
|    683 | 4889 | `		pOut->pArg = pArg;` |
|    683 | 4890 | `		return 1;` |
|      - | 4891 | `	}` |
|   1128 | 4892 | `}` |
|      - | 4893 | `/* The declaring class php reports for a reflected METHOD. */` |
|    342 | 4894 | `static ph7_class * ReflectFuncDeclClass(const ReflectFuncRef *pRef)` |
|      4 | 4895 | `{` |
|    346 | 4896 | `	if( pRef->pMeth == 0 \|\| pRef->pClass == 0 ){` |
|      3 | 4897 | `		return 0;` |
|      - | 4898 | `	}` |
|    344 | 4899 | `	return ReflectMethodDeclClass(pRef->pClass, pRef->pMeth);` |
|    175 | 4900 | `}` |
|      - | 4901 | `/*` |
|      - | 4902 | ` * The declared return type TEXT, or 0 — and whether php calls it TENTATIVE.` |
|      - | 4903 | ` *` |
|      - | 4904 | ` * php's stubs carry two kinds of internal return type and Reflection answers` |
|      - | 4905 | ` * differently for each: a real one is reported by getReturnType()/hasReturnType()` |
|      - | 4906 | `` * and printed `Return [ T ]`, while an `@tentative-return-type` answers NULL and`` |
|      - | 4907 | ` * false from those two, is reported by getTentativeReturnType()/` |
|      - | 4908 | `` * hasTentativeReturnType(), and prints `Tentative return [ T ]`. Nearly every`` |
|      - | 4909 | ` * internal SPL, date and Reflection method is the tentative kind.` |
|      - | 4910 | ` *` |
|      - | 4911 | `` * PHL has one field for both, so a leading `@` on a `zRet` — the same character`` |
|      - | 4912 | ` * php's stub annotation uses — marks the tentative kind. It is stripped here, so` |
|      - | 4913 | ` * nothing downstream of this function ever sees it.` |
|      - | 4914 | ` */` |
|    622 | 4915 | `static int ReflectFuncRetTextEx(const ReflectFuncRef *pRef, const char **pz, int *pn,` |
|      - | 4916 | `	int *pbTentative)` |
|      4 | 4917 | `{` |
|    626 | 4918 | `	if( pbTentative ){` |
|    626 | 4919 | `		*pbTentative = 0;` |
|    311 | 4920 | `	}` |
|    626 | 4921 | `	if( pRef->zRet && pRef->zRet[0] ){` |
|    513 | 4922 | `		const char *z = pRef->zRet;` |
|    513 | 4923 | `		if( z[0] == '@' ){` |
|    237 | 4924 | `			if( pbTentative ){` |
|    237 | 4925 | `				*pbTentative = 1;` |
|    118 | 4926 | `			}` |
|    237 | 4927 | `			z++;` |
|    118 | 4928 | `		}` |
|    513 | 4929 | `		*pz = z;` |
|    513 | 4930 | `		*pn = (int)SyStrlen(z);` |
|    513 | 4931 | `		return 1;` |
|      - | 4932 | `	}` |
|    115 | 4933 | `	if( pRef->pFunc == 0 ){` |
|    ! 0 | 4934 | `		return 0;` |
|      - | 4935 | `	}` |
|    115 | 4936 | `	if( SyStringLength(&pRef->pFunc->sReturnTypeName) > 0 ){` |
|     53 | 4937 | `		*pz = SyStringData(&pRef->pFunc->sReturnTypeName);` |
|     53 | 4938 | `		*pn = (int)SyStringLength(&pRef->pFunc->sReturnTypeName);` |
|     53 | 4939 | `		return 1;` |
|      - | 4940 | `	}` |
|      - | 4941 | `	/* The type-text renderer omits void/never atoms (compile.c notes the root fix` |
|      - | 4942 | `	 * belongs there); name them here for getReturnType(). */` |
|     63 | 4943 | `	if( pRef->pFunc->nReturnType == MEMOBJ_VOID ){` |
|     11 | 4944 | `		*pz = "void";` |
|     11 | 4945 | `		*pn = sizeof("void")-1;` |
|     11 | 4946 | `		return 1;` |
|      - | 4947 | `	}` |
|     53 | 4948 | `	if( pRef->pFunc->nReturnType == MEMOBJ_NEVER ){` |
|      3 | 4949 | `		*pz = "never";` |
|      3 | 4950 | `		*pn = sizeof("never")-1;` |
|      3 | 4951 | `		return 1;` |
|      - | 4952 | `	}` |
|     51 | 4953 | `	return 0;` |
|    315 | 4954 | `}` |
|      - | 4955 | `/* The declared return type php would report from getReturnType(): a TENTATIVE one` |
|      - | 4956 | ` * is not reported there at all, which is the whole distinction. */` |
|    340 | 4957 | `static int ReflectFuncRetText(const ReflectFuncRef *pRef, const char **pz, int *pn)` |
|      4 | 4958 | `{` |
|    344 | 4959 | `	int bTentative = 0;` |
|    344 | 4960 | `	if( !ReflectFuncRetTextEx(pRef, pz, pn, &bTentative) ){` |
|     15 | 4961 | `		return 0;` |
|      - | 4962 | `	}` |
|    330 | 4963 | `	return bTentative ? 0 : 1;` |
|    174 | 4964 | `}` |
|      - | 4965 | `/* Is the reflected function internal (a C builtin or an embedded-chunk one)? */` |
|    200 | 4966 | `static int ReflectFuncIsInternal(const ReflectFuncRef *pRef)` |
|      2 | 4967 | `{` |
|    202 | 4968 | `	if( pRef->pHost ){` |
|     38 | 4969 | `		return 1;` |
|      - | 4970 | `	}` |
|    165 | 4971 | `	return pRef->pFunc != 0 && (pRef->pFunc->iFlags & VM_FUNC_INTERNAL) != 0;` |
|    102 | 4972 | `}` |
|      - | 4973 | `/* Does this target carry a #[\Deprecated] attribute? */` |
|      8 | 4974 | `static int ReflectHasDeprecated(SySet *pAttrs)` |
|      1 | 4975 | `{` |
|      9 | 4976 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|      - | 4977 | `	sxu32 n;` |
|      9 | 4978 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|      6 | 4979 | `		if( SyStringLength(&aA[n].sName) == sizeof("deprecated")-1` |
|      7 | 4980 | `		 && SyStrnicmp(SyStringData(&aA[n].sName), "deprecated", sizeof("deprecated")-1) == 0 ){` |
|      7 | 4981 | `			return 1;` |
|      - | 4982 | `		}` |
|    ! 0 | 4983 | `	}` |
|      3 | 4984 | `	return 0;` |
|      5 | 4985 | `}` |
|      - | 4986 | ``/* Resolve `$this`, or answer a default when the target has gone missing. */`` |
|      - | 4987 | `#define REFLECT_FUNC_OR(REF,STMT) \` |
|      - | 4988 | `	if( !ReflectFuncOfThis(pCtx, &(REF)) ){ STMT; return PH7_OK; }` |
|      - | 4989 |  |
|      - | 4990 | `/* ---- ReflectionFunctionAbstract ---- */` |
|    ! 0 | 4991 | `static int vm_builtin_ReflectionFunc_clone(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 4992 | `{` |
|    ! 0 | 4993 | `	SXUNUSED(pCtx);` |
|    ! 0 | 4994 | `	SXUNUSED(nArg);` |
|    ! 0 | 4995 | `	SXUNUSED(apArg);` |
|    ! 0 | 4996 | `	return PH7_OK;` |
|    ! 0 | 4997 | `}` |
|    182 | 4998 | `static int vm_builtin_ReflectionFunc_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 | 4999 | `{` |
|    187 | 5000 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    187 | 5001 | `	const char *zName = "";` |
|    187 | 5002 | `	int nName = 0;` |
|     91 | 5003 | `	SXUNUSED(nArg);` |
|     91 | 5004 | `	SXUNUSED(apArg);` |
|    187 | 5005 | `	if( pThis ){` |
|    187 | 5006 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     91 | 5007 | `	}` |
|    187 | 5008 | `	ph7_result_string(pCtx, zName, nName);` |
|    187 | 5009 | `	return PH7_OK;` |
|      5 | 5010 | `}` |
|      - | 5011 | ``/* The `name` slot's namespace split — the same three answers ReflectionClass gives. */`` |
|    ! 0 | 5012 | `static int ReflectFuncNamePart(ph7_context *pCtx, int iWhat)` |
|    ! 0 | 5013 | `{` |
|    ! 0 | 5014 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    ! 0 | 5015 | `	const char *zName = "";` |
|    ! 0 | 5016 | `	int nName = 0, iCut;` |
|    ! 0 | 5017 | `	if( pThis ){` |
|    ! 0 | 5018 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    ! 0 | 5019 | `	}` |
|    ! 0 | 5020 | `	iCut = ReflectNsCut(zName, nName);` |
|    ! 0 | 5021 | `	if( iWhat == 0 ){` |
|    ! 0 | 5022 | `		ph7_result_bool(pCtx, iCut >= 0);` |
|    ! 0 | 5023 | `	}else if( iWhat == 1 ){` |
|    ! 0 | 5024 | `		ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|    ! 0 | 5025 | `	}else if( iCut < 0 ){` |
|    ! 0 | 5026 | `		ph7_result_string(pCtx, zName, nName);` |
|    ! 0 | 5027 | `	}else{` |
|    ! 0 | 5028 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|      - | 5029 | `	}` |
|    ! 0 | 5030 | `	return PH7_OK;` |
|    ! 0 | 5031 | `}` |
|      - | 5032 | `#define REFLECT_FUNC_NAMEPART(NAME,WHAT) \` |
|      - | 5033 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 5034 | `	{ \` |
|      - | 5035 | `		SXUNUSED(nArg); \` |
|      - | 5036 | `		SXUNUSED(apArg); \` |
|      - | 5037 | `		return ReflectFuncNamePart(pCtx, WHAT); \` |
|      - | 5038 | `	}` |
|    ! 0 | 5039 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_inNamespace, 0)` |
|    ! 0 | 5040 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_getNamespaceName, 1)` |
|    ! 0 | 5041 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_getShortName, 2)` |
|      - | 5042 |  |
|     14 | 5043 | `static int vm_builtin_ReflectionFunc_isClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5044 | `{` |
|      - | 5045 | `	ReflectFuncRef sRef;` |
|     15 | 5046 | `	int bAnon = 0;` |
|      7 | 5047 | `	SXUNUSED(nArg);` |
|      7 | 5048 | `	SXUNUSED(apArg);` |
|     15 | 5049 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     15 | 5050 | `	if( sRef.pFunc ){` |
|      - | 5051 | ``		/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|      - | 5052 | `		 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|     15 | 5053 | `		bAnon = (sRef.pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|     14 | 5054 | `		if( !bAnon && SyStringLength(&sRef.pFunc->sName) > 9` |
|      3 | 5055 | `		 && (SyMemcmp(SyStringData(&sRef.pFunc->sName), "[lambda_", 8) == 0` |
|    ! 0 | 5056 | `		  \|\| SyMemcmp(SyStringData(&sRef.pFunc->sName), "[closure_", 9) == 0) ){` |
|    ! 0 | 5057 | `			bAnon = 1;` |
|    ! 0 | 5058 | `		}` |
|      7 | 5059 | `	}` |
|     15 | 5060 | `	ph7_result_bool(pCtx, bAnon);` |
|     15 | 5061 | `	return PH7_OK;` |
|      8 | 5062 | `}` |
|      8 | 5063 | `static int vm_builtin_ReflectionFunc_isDeprecated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5064 | `{` |
|      - | 5065 | `	ReflectFuncRef sRef;` |
|      4 | 5066 | `	SXUNUSED(nArg);` |
|      4 | 5067 | `	SXUNUSED(apArg);` |
|      9 | 5068 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|      9 | 5069 | `	ph7_result_bool(pCtx, sRef.pFunc != 0 && ReflectHasDeprecated(&sRef.pFunc->aAttrs));` |
|      9 | 5070 | `	return PH7_OK;` |
|      5 | 5071 | `}` |
|     44 | 5072 | `static int vm_builtin_ReflectionFunc_isInternal(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5073 | `{` |
|      - | 5074 | `	ReflectFuncRef sRef;` |
|     22 | 5075 | `	SXUNUSED(nArg);` |
|     22 | 5076 | `	SXUNUSED(apArg);` |
|     45 | 5077 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     45 | 5078 | `	ph7_result_bool(pCtx, ReflectFuncIsInternal(&sRef));` |
|     45 | 5079 | `	return PH7_OK;` |
|     23 | 5080 | `}` |
|    ! 0 | 5081 | `static int vm_builtin_ReflectionFunc_isUserDefined(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 5082 | `{` |
|      - | 5083 | `	ReflectFuncRef sRef;` |
|    ! 0 | 5084 | `	SXUNUSED(nArg);` |
|    ! 0 | 5085 | `	SXUNUSED(apArg);` |
|    ! 0 | 5086 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    ! 0 | 5087 | `	ph7_result_bool(pCtx, !ReflectFuncIsInternal(&sRef));` |
|    ! 0 | 5088 | `	return PH7_OK;` |
|    ! 0 | 5089 | `}` |
|      4 | 5090 | `static int vm_builtin_ReflectionFunc_isGenerator(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5091 | `{` |
|      - | 5092 | `	ReflectFuncRef sRef;` |
|      2 | 5093 | `	SXUNUSED(nArg);` |
|      2 | 5094 | `	SXUNUSED(apArg);` |
|      5 | 5095 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|      5 | 5096 | `	ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|      5 | 5097 | `	return PH7_OK;` |
|      3 | 5098 | `}` |
|     10 | 5099 | `static int vm_builtin_ReflectionFunc_isVariadic(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5100 | `{` |
|      - | 5101 | `	ReflectFuncRef sRef;` |
|      - | 5102 | `	ReflectParamDesc sDesc;` |
|     11 | 5103 | `	int n, nTotal, bVariadic = 0;` |
|      5 | 5104 | `	SXUNUSED(nArg);` |
|      5 | 5105 | `	SXUNUSED(apArg);` |
|     11 | 5106 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     11 | 5107 | `	nTotal = ReflectParamCount(&sRef);` |
|     31 | 5108 | `	for( n = 0 ; n < nTotal ; n++ ){` |
|     29 | 5109 | `		if( ReflectParamAt(&sRef, n, &sDesc) && sDesc.bVariadic ){` |
|      9 | 5110 | `			bVariadic = 1;` |
|      9 | 5111 | `			break;` |
|      - | 5112 | `		}` |
|     11 | 5113 | `	}` |
|     11 | 5114 | `	ph7_result_bool(pCtx, bVariadic);` |
|     11 | 5115 | `	return PH7_OK;` |
|      6 | 5116 | `}` |
|      - | 5117 | ``/* isStatic(): a METHOD's own staticness, a closure's `static function () {}`. */`` |
|     82 | 5118 | `static int vm_builtin_ReflectionFunc_isStatic(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 5119 | `{` |
|      - | 5120 | `	ReflectFuncRef sRef;` |
|     41 | 5121 | `	SXUNUSED(nArg);` |
|     41 | 5122 | `	SXUNUSED(apArg);` |
|     84 | 5123 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     84 | 5124 | `	if( sRef.pMeth ){` |
|     74 | 5125 | `		ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|     38 | 5126 | `	}else{` |
|     11 | 5127 | `		ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);` |
|      - | 5128 | `	}` |
|     84 | 5129 | `	return PH7_OK;` |
|     43 | 5130 | `}` |
|      6 | 5131 | `static int vm_builtin_ReflectionFunc_returnsReference(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5132 | `{` |
|      - | 5133 | `	ReflectFuncRef sRef;` |
|      3 | 5134 | `	SXUNUSED(nArg);` |
|      3 | 5135 | `	SXUNUSED(apArg);` |
|      7 | 5136 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|      7 | 5137 | `	ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|      7 | 5138 | `	return PH7_OK;` |
|      4 | 5139 | `}` |
|      - | 5140 | `/* The Closure instance whose captured state the three getClosure* read. */` |
|     36 | 5141 | `static ph7_value * ReflectClosureAttr(ReflectFuncRef *pRef, const char *zName)` |
|      1 | 5142 | `{` |
|      - | 5143 | `	SyString sAttr;` |
|     37 | 5144 | `	if( pRef->pClosure == 0 ){` |
|    ! 0 | 5145 | `		return 0;` |
|      - | 5146 | `	}` |
|     37 | 5147 | `	SyStringInitFromBuf(&sAttr, zName, SyStrlen(zName));` |
|     37 | 5148 | `	return PH7_ClassInstanceFetchAttr(pRef->pClosure, &sAttr);` |
|     19 | 5149 | `}` |
|      4 | 5150 | `static int vm_builtin_ReflectionFunc_getClosureThis(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5151 | `{` |
|      - | 5152 | `	ReflectFuncRef sRef;` |
|      - | 5153 | `	ph7_value *pAttr;` |
|      2 | 5154 | `	SXUNUSED(nArg);` |
|      2 | 5155 | `	SXUNUSED(apArg);` |
|      5 | 5156 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|      5 | 5157 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|      5 | 5158 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|      3 | 5159 | `		ph7_result_value(pCtx, pAttr);` |
|      2 | 5160 | `	}else{` |
|      3 | 5161 | `		ph7_result_null(pCtx);` |
|      - | 5162 | `	}` |
|      5 | 5163 | `	return PH7_OK;` |
|      3 | 5164 | `}` |
|     18 | 5165 | `static int vm_builtin_ReflectionFunc_getClosureScopeClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5166 | `{` |
|      - | 5167 | `	ReflectFuncRef sRef;` |
|      - | 5168 | `	ph7_value *pAttr;` |
|      9 | 5169 | `	SXUNUSED(nArg);` |
|      9 | 5170 | `	SXUNUSED(apArg);` |
|     19 | 5171 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|     19 | 5172 | `	pAttr = ReflectClosureAttr(&sRef, "__scope");` |
|     19 | 5173 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|      - | 5174 | `		/* php reports the class that DECLARED the callee, not the one the callable NAMED:` |
|      - | 5175 | ``		 * `$__scope` is the CALLED scope (what `static::` answers, reported by`` |
|      - | 5176 | `		 * getClosureCalledClass below), and for an inherited or trait-composed method the` |
|      - | 5177 | `		 * two differ. PH7_VmClosureScopeClass is the one rule. */` |
|     22 | 5178 | `		return ReflectResultClassOf(pCtx,` |
|      7 | 5179 | `			PH7_VmClosureScopeClass(pCtx->pVm, sRef.pClosure));` |
|      - | 5180 | `	}` |
|      5 | 5181 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|      5 | 5182 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|    ! 0 | 5183 | `		return ReflectResultClassOf(pCtx, ((ph7_class_instance *)pAttr->x.pOther)->pClass);` |
|      - | 5184 | `	}` |
|      5 | 5185 | `	ph7_result_null(pCtx);` |
|      5 | 5186 | `	return PH7_OK;` |
|     10 | 5187 | `}` |
|      - | 5188 | `/*` |
|      - | 5189 | ` * ReflectionFunction::getClosureCalledClass() — php's CALLED scope, the other half of the` |
|      - | 5190 | `` * pair above: the class the call goes THROUGH, which is what `static::` answers. A bound`` |
|      - | 5191 | `` * closure's is its `$this`'s class; a static callable's is the class the callable NAMED. It`` |
|      - | 5192 | `` * shared getClosureScopeClass()'s body while `$__scope` answered both questions; it cannot`` |
|      - | 5193 | ` * now, because that one narrows a method closure to its DECLARING class.` |
|      - | 5194 | ` */` |
|      8 | 5195 | `static int vm_builtin_ReflectionFunc_getClosureCalledClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5196 | `{` |
|      - | 5197 | `	ReflectFuncRef sRef;` |
|      - | 5198 | `	ph7_value *pAttr;` |
|      4 | 5199 | `	SXUNUSED(nArg);` |
|      4 | 5200 | `	SXUNUSED(apArg);` |
|      9 | 5201 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|      9 | 5202 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|      9 | 5203 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|      7 | 5204 | `		return ReflectResultClassOf(pCtx, ((ph7_class_instance *)pAttr->x.pOther)->pClass);` |
|      - | 5205 | `	}` |
|      3 | 5206 | `	pAttr = ReflectClosureAttr(&sRef, "__scope");` |
|      3 | 5207 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|      4 | 5208 | `		return ReflectResultClassOf(pCtx, PH7_VmExtractClass(pCtx->pVm,` |
|      2 | 5209 | `			(const char *)SyBlobData(&pAttr->sBlob), SyBlobLength(&pAttr->sBlob), FALSE, 0));` |
|      - | 5210 | `	}` |
|    ! 0 | 5211 | `	ph7_result_null(pCtx);` |
|    ! 0 | 5212 | `	return PH7_OK;` |
|      5 | 5213 | `}` |
|     10 | 5214 | `static int vm_builtin_ReflectionFunc_getClosureUsedVariables(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 5215 | `{` |
|      - | 5216 | `	ReflectFuncRef sRef;` |
|     12 | 5217 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - | 5218 | `	ph7_vm_func_closure_env *aEnv;` |
|      - | 5219 | `	sxu32 n;` |
|      5 | 5220 | `	SXUNUSED(nArg);` |
|      5 | 5221 | `	SXUNUSED(apArg);` |
|     12 | 5222 | `	if( pOut == 0 ){` |
|    ! 0 | 5223 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5224 | `	}` |
|     12 | 5225 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pClosure == 0 \|\| sRef.pFunc == 0 ){` |
|    ! 0 | 5226 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 | 5227 | `		return PH7_OK;` |
|      - | 5228 | `	}` |
|      - | 5229 | `	/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */` |
|     12 | 5230 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|     28 | 5231 | `	for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; n++ ){` |
|     18 | 5232 | `		if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){` |
|     12 | 5233 | `			continue;` |
|      - | 5234 | `		}` |
|      6 | 5235 | `		if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|      4 | 5236 | `		 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|    ! 0 | 5237 | `			continue;` |
|      - | 5238 | `		}` |
|      7 | 5239 | `		if( (aEnv[n].iFlags & VM_FUNC_ARG_BY_REF) && aEnv[n].nIdx != SXU32_HIGH ){` |
|      - | 5240 | `			/* Captured by reference: report the slot's live value */` |
|      3 | 5241 | `			ph7_value *pLive = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aEnv[n].nIdx);` |
|      3 | 5242 | `			ReflectMapAddDyn(pCtx, pOut, &aEnv[n].sName, pLive ? pLive : &aEnv[n].sValue);` |
|      3 | 5243 | `			continue;` |
|      - | 5244 | `		}` |
|      5 | 5245 | `		ReflectMapAddDyn(pCtx, pOut, &aEnv[n].sName, &aEnv[n].sValue);` |
|      3 | 5246 | `	}` |
|     12 | 5247 | `	ph7_result_value(pCtx, pOut);` |
|     12 | 5248 | `	return PH7_OK;` |
|      7 | 5249 | `}` |
|     14 | 5250 | `static int vm_builtin_ReflectionFunc_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5251 | `{` |
|      - | 5252 | `	ReflectFuncRef sRef;` |
|      7 | 5253 | `	SXUNUSED(nArg);` |
|      7 | 5254 | `	SXUNUSED(apArg);` |
|     15 | 5255 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     15 | 5256 | `	if( sRef.pFunc && SyStringLength(&sRef.pFunc->sDoc) > 0 ){` |
|     11 | 5257 | `		ph7_result_string(pCtx, SyStringData(&sRef.pFunc->sDoc), (int)SyStringLength(&sRef.pFunc->sDoc));` |
|      6 | 5258 | `	}else{` |
|      5 | 5259 | `		ph7_result_bool(pCtx, 0);` |
|      - | 5260 | `	}` |
|     15 | 5261 | `	return PH7_OK;` |
|      8 | 5262 | `}` |
|     22 | 5263 | `static int vm_builtin_ReflectionFunc_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5264 | `{` |
|      - | 5265 | `	ReflectFuncRef sRef;` |
|     11 | 5266 | `	SXUNUSED(nArg);` |
|     11 | 5267 | `	SXUNUSED(apArg);` |
|     23 | 5268 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     23 | 5269 | `	if( sRef.pFunc && SyStringLength(&sRef.pFunc->sFile) > 0 ){` |
|      3 | 5270 | `		ph7_result_string(pCtx, SyStringData(&sRef.pFunc->sFile), (int)SyStringLength(&sRef.pFunc->sFile));` |
|      2 | 5271 | `	}else{` |
|     21 | 5272 | `		ph7_result_bool(pCtx, 0);` |
|      - | 5273 | `	}` |
|     23 | 5274 | `	return PH7_OK;` |
|     12 | 5275 | `}` |
|     22 | 5276 | `static int ReflectFuncLine(ph7_context *pCtx, int bEnd)` |
|      1 | 5277 | `{` |
|      - | 5278 | `	ReflectFuncRef sRef;` |
|     23 | 5279 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| ReflectFuncIsInternal(&sRef) \|\| sRef.pFunc == 0 ){` |
|     17 | 5280 | `		ph7_result_bool(pCtx, 0);` |
|     17 | 5281 | `		return PH7_OK;` |
|      - | 5282 | `	}` |
|      7 | 5283 | `	ph7_result_int64(pCtx, (sxi64)(bEnd ? sRef.pFunc->nEndLine : sRef.pFunc->nLine));` |
|      7 | 5284 | `	return PH7_OK;` |
|     12 | 5285 | `}` |
|     20 | 5286 | `static int vm_builtin_ReflectionFunc_getStartLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5287 | `{` |
|     10 | 5288 | `	SXUNUSED(nArg);` |
|     10 | 5289 | `	SXUNUSED(apArg);` |
|     21 | 5290 | `	return ReflectFuncLine(pCtx, 0);` |
|      1 | 5291 | `}` |
|      2 | 5292 | `static int vm_builtin_ReflectionFunc_getEndLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5293 | `{` |
|      1 | 5294 | `	SXUNUSED(nArg);` |
|      1 | 5295 | `	SXUNUSED(apArg);` |
|      3 | 5296 | `	return ReflectFuncLine(pCtx, 1);` |
|      1 | 5297 | `}` |
|     94 | 5298 | `static int vm_builtin_ReflectionFunc_hasReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 5299 | `{` |
|      - | 5300 | `	ReflectFuncRef sRef;` |
|      - | 5301 | `	const char *z;` |
|      - | 5302 | `	int n;` |
|     47 | 5303 | `	SXUNUSED(nArg);` |
|     47 | 5304 | `	SXUNUSED(apArg);` |
|     96 | 5305 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     96 | 5306 | `	ph7_result_bool(pCtx, ReflectFuncRetText(&sRef, &z, &n));` |
|     96 | 5307 | `	return PH7_OK;` |
|     49 | 5308 | `}` |
|    246 | 5309 | `static int vm_builtin_ReflectionFunc_getReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 | 5310 | `{` |
|      - | 5311 | `	ReflectFuncRef sRef;` |
|      - | 5312 | `	const char *z;` |
|      - | 5313 | `	int n;` |
|    123 | 5314 | `	SXUNUSED(nArg);` |
|    123 | 5315 | `	SXUNUSED(apArg);` |
|    250 | 5316 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|    250 | 5317 | `	if( !ReflectFuncRetText(&sRef, &z, &n) ){` |
|     19 | 5318 | `		ph7_result_null(pCtx);` |
|     19 | 5319 | `		return PH7_OK;` |
|      - | 5320 | `	}` |
|    232 | 5321 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx, z, n));` |
|    127 | 5322 | `}` |
|      - | 5323 | ``/* A native method's `@`-marked zRet is php's `@tentative-return-type`: reported`` |
|      - | 5324 | ` * HERE and by nothing else, which is what separates it from a real one. */` |
|     66 | 5325 | `static int vm_builtin_ReflectionFunc_hasTentativeReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5326 | `{` |
|      - | 5327 | `	ReflectFuncRef sRef;` |
|      - | 5328 | `	const char *z;` |
|     67 | 5329 | `	int n, bTentative = 0;` |
|     33 | 5330 | `	SXUNUSED(nArg);` |
|     33 | 5331 | `	SXUNUSED(apArg);` |
|     67 | 5332 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     67 | 5333 | `	ph7_result_bool(pCtx, ReflectFuncRetTextEx(&sRef, &z, &n, &bTentative) && bTentative);` |
|     67 | 5334 | `	return PH7_OK;` |
|     34 | 5335 | `}` |
|     88 | 5336 | `static int vm_builtin_ReflectionFunc_getTentativeReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5337 | `{` |
|      - | 5338 | `	ReflectFuncRef sRef;` |
|      - | 5339 | `	const char *z;` |
|     89 | 5340 | `	int n, bTentative = 0;` |
|     44 | 5341 | `	SXUNUSED(nArg);` |
|     44 | 5342 | `	SXUNUSED(apArg);` |
|     89 | 5343 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|     89 | 5344 | `	if( !ReflectFuncRetTextEx(&sRef, &z, &n, &bTentative) \|\| !bTentative ){` |
|     13 | 5345 | `		ph7_result_null(pCtx);` |
|     13 | 5346 | `		return PH7_OK;` |
|      - | 5347 | `	}` |
|     77 | 5348 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx, z, n));` |
|     45 | 5349 | `}` |
|     50 | 5350 | `static int vm_builtin_ReflectionFunc_getNumberOfParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 5351 | `{` |
|      - | 5352 | `	ReflectFuncRef sRef;` |
|     25 | 5353 | `	SXUNUSED(nArg);` |
|     25 | 5354 | `	SXUNUSED(apArg);` |
|     53 | 5355 | `	REFLECT_FUNC_OR(sRef, ph7_result_int(pCtx, 0))` |
|     53 | 5356 | `	if( sRef.pHost && sRef.zSig == 0 ){` |
|      - | 5357 | `		/* An UNDESCRIBED builtin: the arity table is all there is. */` |
|    ! 0 | 5358 | `		ph7_result_int64(pCtx, (sxi64)sRef.pHost->nMinArg);` |
|    ! 0 | 5359 | `		return PH7_OK;` |
|      - | 5360 | `	}` |
|     53 | 5361 | `	ph7_result_int(pCtx, ReflectParamCount(&sRef));` |
|     53 | 5362 | `	return PH7_OK;` |
|     28 | 5363 | `}` |
|     24 | 5364 | `static int vm_builtin_ReflectionFunc_getNumberOfRequiredParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5365 | `{` |
|      - | 5366 | `	ReflectFuncRef sRef;` |
|      - | 5367 | `	ReflectParamDesc sDesc;` |
|     25 | 5368 | `	int n, nTotal, nReq = 0;` |
|     12 | 5369 | `	SXUNUSED(nArg);` |
|     12 | 5370 | `	SXUNUSED(apArg);` |
|     25 | 5371 | `	REFLECT_FUNC_OR(sRef, ph7_result_int(pCtx, 0))` |
|     25 | 5372 | `	if( sRef.pHost && sRef.zSig == 0 ){` |
|    ! 0 | 5373 | `		ph7_result_int64(pCtx, (sxi64)sRef.pHost->nMinArg);` |
|    ! 0 | 5374 | `		return PH7_OK;` |
|      - | 5375 | `	}` |
|     25 | 5376 | `	nTotal = ReflectParamCount(&sRef);` |
|     55 | 5377 | `	for( n = nTotal ; n > 0 ; n-- ){` |
|     53 | 5378 | `		if( ReflectParamAt(&sRef, n - 1, &sDesc) && !sDesc.bVariadic && !sDesc.bOptional ){` |
|     23 | 5379 | `			nReq = n;` |
|     23 | 5380 | `			break;` |
|      - | 5381 | `		}` |
|     16 | 5382 | `	}` |
|     25 | 5383 | `	ph7_result_int(pCtx, nReq);` |
|     25 | 5384 | `	return PH7_OK;` |
|     13 | 5385 | `}` |
|      - | 5386 | `/* The (target, method) pair a ReflectionParameter is built over. */` |
|    446 | 5387 | `static void ReflectFuncParamSpec(ph7_context *pCtx, ph7_value *pSpec)` |
|      4 | 5388 | `{` |
|    450 | 5389 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 5390 | `	ph7_class_instance *pClo;` |
|      - | 5391 | `	const char *zName, *zClass;` |
|      - | 5392 | `	int nName, nClass;` |
|    450 | 5393 | `	PH7_MemObjInit(pCtx->pVm, pSpec);` |
|    450 | 5394 | `	if( pThis == 0 ){` |
|    ! 0 | 5395 | `		return;` |
|      - | 5396 | `	}` |
|    450 | 5397 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|    450 | 5398 | `	if( pClo ){` |
|     11 | 5399 | `		pSpec->x.pOther = pClo;` |
|     11 | 5400 | `		pSpec->iFlags = MEMOBJ_OBJ;` |
|     11 | 5401 | `		return;` |
|      - | 5402 | `	}` |
|    440 | 5403 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    440 | 5404 | `	if( ReflectIsMethodReflector(pCtx, pThis) ){` |
|      - | 5405 | ``		/* `[class, method]`, which ReflectionParameter's constructor accepts */`` |
|    159 | 5406 | `		ph7_value *pList = ph7_context_new_array(pCtx);` |
|    159 | 5407 | `		ph7_value *pA = ph7_context_new_scalar(pCtx);` |
|    159 | 5408 | `		ph7_value *pB = ph7_context_new_scalar(pCtx);` |
|    159 | 5409 | `		if( pList == 0 \|\| pA == 0 \|\| pB == 0 ){` |
|    ! 0 | 5410 | `			return;` |
|      - | 5411 | `		}` |
|    159 | 5412 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|    159 | 5413 | `		ph7_value_string(pA, zClass, nClass);` |
|    159 | 5414 | `		ph7_value_string(pB, zName, nName);` |
|    159 | 5415 | `		ph7_array_add_elem(pList, 0, pA);` |
|    159 | 5416 | `		ph7_array_add_elem(pList, 0, pB);` |
|    159 | 5417 | `		PH7_MemObjStore(pList, pSpec);` |
|    159 | 5418 | `		return;` |
|      - | 5419 | `	}` |
|    282 | 5420 | `	ph7_value_string(pSpec, zName, nName);` |
|    227 | 5421 | `}` |
|    434 | 5422 | `static int vm_builtin_ReflectionFunc_getParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 | 5423 | `{` |
|      - | 5424 | `	ReflectFuncRef sRef;` |
|    438 | 5425 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - | 5426 | `	ph7_value sSpec;` |
|      - | 5427 | `	int n, nTotal;` |
|    217 | 5428 | `	SXUNUSED(nArg);` |
|    217 | 5429 | `	SXUNUSED(apArg);` |
|    438 | 5430 | `	if( pOut == 0 ){` |
|    ! 0 | 5431 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5432 | `	}` |
|    438 | 5433 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 | 5434 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 | 5435 | `		return PH7_OK;` |
|      - | 5436 | `	}` |
|    438 | 5437 | `	nTotal = ReflectParamCount(&sRef);` |
|    438 | 5438 | `	ReflectFuncParamSpec(pCtx, &sSpec);` |
|   1148 | 5439 | `	for( n = 0 ; n < nTotal ; n++ ){` |
|      - | 5440 | `		ph7_value sPos, sVal;` |
|      - | 5441 | `		ph7_value *apCtor[2];` |
|      - | 5442 | `		ph7_class_instance *pParam;` |
|      - | 5443 | `		sxi32 rc;` |
|    714 | 5444 | `		PH7_MemObjInit(pCtx->pVm, &sPos);` |
|    714 | 5445 | `		ph7_value_int(&sPos, n);` |
|    714 | 5446 | `		apCtor[0] = &sSpec;` |
|    714 | 5447 | `		apCtor[1] = &sPos;` |
|    714 | 5448 | `		pParam = ReflectConstruct(pCtx, "ReflectionParameter", 2, apCtor, &rc);` |
|    714 | 5449 | `		PH7_MemObjRelease(&sPos);` |
|    714 | 5450 | `		if( pParam == 0 ){` |
|    ! 0 | 5451 | `			break;` |
|      - | 5452 | `		}` |
|    714 | 5453 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    714 | 5454 | `		sVal.x.pOther = pParam;` |
|    714 | 5455 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|    714 | 5456 | `		ph7_array_add_elem(pOut, 0, &sVal);   /* takes its own reference */` |
|    714 | 5457 | `		PH7_ClassInstanceUnref(pParam);` |
|    359 | 5458 | `	}` |
|    438 | 5459 | `	if( (sSpec.iFlags & MEMOBJ_OBJ) == 0 ){` |
|    432 | 5460 | `		PH7_MemObjRelease(&sSpec);` |
|    214 | 5461 | `	}` |
|    438 | 5462 | `	ph7_result_value(pCtx, pOut);` |
|    438 | 5463 | `	return PH7_OK;` |
|    221 | 5464 | `}` |
|      4 | 5465 | `static int vm_builtin_ReflectionFunc_getStaticVariables(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5466 | `{` |
|      - | 5467 | `	ReflectFuncRef sRef;` |
|      5 | 5468 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - | 5469 | `	ph7_vm_func_static_var *aStatic;` |
|      - | 5470 | `	sxu32 n;` |
|      2 | 5471 | `	SXUNUSED(nArg);` |
|      2 | 5472 | `	SXUNUSED(apArg);` |
|      5 | 5473 | `	if( pOut == 0 ){` |
|    ! 0 | 5474 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 5475 | `	}` |
|      5 | 5476 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pFunc == 0 ){` |
|    ! 0 | 5477 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 | 5478 | `		return PH7_OK;` |
|      - | 5479 | `	}` |
|      - | 5480 | `	/* Current value when the slot was initialized (first call), otherwise the` |
|      - | 5481 | `	 * evaluated default — php's getStaticVariables initializes on demand and` |
|      - | 5482 | `	 * reports the same values. */` |
|      5 | 5483 | `	aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&sRef.pFunc->aStatic);` |
|      9 | 5484 | `	for( n = 0 ; n < SySetUsed(&sRef.pFunc->aStatic) ; n++ ){` |
|      5 | 5485 | `		ph7_value *pVal = 0;` |
|      - | 5486 | `		ph7_value sScratch;` |
|      5 | 5487 | `		int bScratch = 0;` |
|      5 | 5488 | `		if( aStatic[n].nIdx != SXU32_HIGH ){` |
|      3 | 5489 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);` |
|      1 | 5490 | `		}` |
|      5 | 5491 | `		if( pVal == 0 ){` |
|      3 | 5492 | `			PH7_MemObjInit(pCtx->pVm, &sScratch);` |
|      3 | 5493 | `			if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      3 | 5494 | `				VmLocalExec(pCtx->pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|      1 | 5495 | `			}` |
|      3 | 5496 | `			pVal = &sScratch;` |
|      3 | 5497 | `			bScratch = 1;` |
|      1 | 5498 | `		}` |
|      5 | 5499 | `		ReflectMapAddDyn(pCtx, pOut, &aStatic[n].sName, pVal);` |
|      5 | 5500 | `		if( bScratch ){` |
|      3 | 5501 | `			PH7_MemObjRelease(&sScratch);` |
|      1 | 5502 | `		}` |
|      3 | 5503 | `	}` |
|      5 | 5504 | `	ph7_result_value(pCtx, pOut);` |
|      5 | 5505 | `	return PH7_OK;` |
|      3 | 5506 | `}` |
|      - | 5507 | ``/* One `key => value` entry of a presented shape, by name. */`` |
|     46 | 5508 | `static void ClosurePresentAdd(ph7_vm *pVm, ph7_value *pOut, const char *zKey, ph7_value *pVal)` |
|      1 | 5509 | `{` |
|      - | 5510 | `	ph7_value sKey;` |
|     47 | 5511 | `	PH7_MemObjInitFromString(pVm, &sKey, 0);` |
|     47 | 5512 | `	PH7_MemObjStringAppend(&sKey, zKey, (sxu32)SyStrlen(zKey));` |
|     47 | 5513 | `	ph7_array_add_elem(pOut, &sKey, pVal);   /* takes its OWN reference */` |
|     47 | 5514 | `	PH7_MemObjRelease(&sKey);` |
|     47 | 5515 | `}` |
|      - | 5516 | `/*` |
|      - | 5517 | ` * php's DEBUG presentation for a Closure (ph7_class::xPresent) —` |
|      - | 5518 | `` * `zend_closure_get_debug_info` (Zend/zend_closures.c), key for key and in php's`` |
|      - | 5519 | `` * order. Every key is CONDITIONAL, which is why a plain `function(){}` shows three`` |
|      - | 5520 | ` * and a bound one with captures shows five:` |
|      - | 5521 | ` *` |
|      - | 5522 | ` *   - a FAKE closure (php's ZEND_ACC_FAKE_CLOSURE — a first-class callable or` |
|      - | 5523 | `` *     `Closure::fromCallable` over a NAMED function) shows one `function` key,`` |
|      - | 5524 | `` *     `Class::method` when it carries a scope and the bare name otherwise, and NO`` |
|      - | 5525 | `` *     name/file/line. A real closure shows `name` (php's `{closure:SCOPE:LINE}`,`` |
|      - | 5526 | `` *     which `PH7_VmFuncDisplayName` answers), `file` and an INT `line`;`` |
|      - | 5527 | `` *   - `static` is the closure's own variable state — the `use` captures AND the`` |
|      - | 5528 | `` *     body's `static $x` — keyed WITHOUT the `$`, and omitted when empty. php`` |
|      - | 5529 | ` *     shows it before the first call too, since the initializers are compiled;` |
|      - | 5530 | `` *   - `this` is the bound receiver, present only when there is one (`bindTo`,`` |
|      - | 5531 | ` *     an instance first-class callable, or the implicit capture in a method);` |
|      - | 5532 | `` *   - `parameter` is `"$name" => "<required>"\|"<optional>"`, `&$name` for a by-ref`` |
|      - | 5533 | ` *     parameter, omitted when the callee takes none. php's cut is` |
|      - | 5534 | `` *     `required_num_args`, which counts through the LAST required parameter — so`` |
|      - | 5535 | `` *     `function($a, $b = 1, $c)` reports all THREE as `<required>`, not two.`` |
|      - | 5536 | ` *` |
|      - | 5537 | `` * The non-debug half answers nothing: php's `get_properties` for a Closure is an`` |
|      - | 5538 | `` * empty table, and `(array)$closure` never reaches it at all (php special-cases a`` |
|      - | 5539 | `` * Closure in `convert_to_array` and wraps it as a SCALAR, `[0 => $closure]`; PHL`` |
|      - | 5540 | `` * answers `[]` there — PLAN §4).`` |
|      - | 5541 | ` */` |
|     18 | 5542 | `PH7_PRIVATE sxi32 PH7_ClosurePresent(ph7_vm *pVm, ph7_class_instance *pThis,` |
|      - | 5543 | `	ph7_value *pOut, int bDebug)` |
|      1 | 5544 | `{` |
|      - | 5545 | `	ReflectFuncRef sRef;` |
|      - | 5546 | `	ph7_value sCarrier, sVal;` |
|     19 | 5547 | `	int bFake = 1;` |
|     19 | 5548 | `	if( !bDebug \|\| pThis == 0 ){` |
|      5 | 5549 | `		return SXRET_OK;` |
|      - | 5550 | `	}` |
|      - | 5551 | `	/* Rule 16's other half: this carrier never took a reference, so it must be` |
|      - | 5552 | `	 * blanked before it is released or the Closure is unref'd a second time. */` |
|     15 | 5553 | `	PH7_MemObjInit(pVm, &sCarrier);` |
|     15 | 5554 | `	sCarrier.x.pOther = pThis;` |
|     15 | 5555 | `	MemObjSetType(&sCarrier, MEMOBJ_OBJ);` |
|     15 | 5556 | `	if( !ReflectFuncFill(pVm, &sCarrier, 0, &sRef) ){` |
|    ! 0 | 5557 | `		sCarrier.x.pOther = 0;` |
|    ! 0 | 5558 | `		sCarrier.iFlags = MEMOBJ_NULL;` |
|    ! 0 | 5559 | `		PH7_MemObjRelease(&sCarrier);` |
|    ! 0 | 5560 | `		return SXRET_OK;` |
|      - | 5561 | `	}` |
|     15 | 5562 | `	sCarrier.x.pOther = 0;` |
|     15 | 5563 | `	sCarrier.iFlags = MEMOBJ_NULL;` |
|     15 | 5564 | `	PH7_MemObjRelease(&sCarrier);` |
|      - | 5565 | `	/* php's FAKE-closure bit, read off what the callable actually resolved TO: a` |
|      - | 5566 | `	 * real closure's body is the anonymous function itself (the compiler's` |
|      - | 5567 | ``	 * `{closure:...}` name, or the synthesized key that stands in for it). */`` |
|     15 | 5568 | `	if( sRef.pFunc && sRef.pMeth == 0 ){` |
|      9 | 5569 | `		const SyString *pN = &sRef.pFunc->sName;` |
|      8 | 5570 | `		if( SyStringLength(&sRef.pFunc->sClosureName) > 0` |
|      4 | 5571 | `		 \|\| (pN->nByte > 8 && SyMemcmp(pN->zString, "[lambda_", 8) == 0)` |
|      1 | 5572 | `		 \|\| (pN->nByte > 9 && SyMemcmp(pN->zString, "[closure_", 9) == 0) ){` |
|      9 | 5573 | `			bFake = 0;` |
|      4 | 5574 | `		}` |
|      4 | 5575 | `	}` |
|     15 | 5576 | `	PH7_MemObjInit(pVm, &sVal);` |
|     15 | 5577 | `	if( bFake ){` |
|      7 | 5578 | `		ph7_class *pScope = ReflectFuncDeclClass(&sRef);` |
|      7 | 5579 | `		const SyString *pName = sRef.pFunc ? &sRef.pFunc->sName : &sRef.pHost->sName;` |
|      7 | 5580 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|      7 | 5581 | `		if( pScope == 0 && sRef.pClass ){` |
|    ! 0 | 5582 | `			pScope = sRef.pClass;` |
|    ! 0 | 5583 | `		}` |
|      7 | 5584 | `		if( pScope ){` |
|      7 | 5585 | `			PH7_MemObjStringAppend(&sVal, SyStringData(&pScope->sName),` |
|      2 | 5586 | `				SyStringLength(&pScope->sName));` |
|      5 | 5587 | `			PH7_MemObjStringAppend(&sVal, "::", 2);` |
|      2 | 5588 | `		}` |
|      7 | 5589 | `		PH7_MemObjStringAppend(&sVal, SyStringData(pName), SyStringLength(pName));` |
|      7 | 5590 | `		ClosurePresentAdd(pVm, pOut, "function", &sVal);` |
|      7 | 5591 | `		PH7_MemObjRelease(&sVal);` |
|      4 | 5592 | `	}else{` |
|      - | 5593 | `		const char *zShow;` |
|      9 | 5594 | `		int nShow = PH7_VmFuncDisplayName(pVm, sRef.pFunc, &zShow);` |
|      9 | 5595 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|      9 | 5596 | `		PH7_MemObjStringAppend(&sVal, zShow, (sxu32)(nShow > 0 ? nShow : 0));` |
|      9 | 5597 | `		ClosurePresentAdd(pVm, pOut, "name", &sVal);` |
|      9 | 5598 | `		PH7_MemObjRelease(&sVal);` |
|      9 | 5599 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|     13 | 5600 | `		PH7_MemObjStringAppend(&sVal, SyStringData(&sRef.pFunc->sFile),` |
|      8 | 5601 | `			SyStringLength(&sRef.pFunc->sFile));` |
|      9 | 5602 | `		ClosurePresentAdd(pVm, pOut, "file", &sVal);` |
|      9 | 5603 | `		PH7_MemObjRelease(&sVal);` |
|      9 | 5604 | `		PH7_MemObjInitFromInt(pVm, &sVal, (sxi64)sRef.pFunc->nLine);` |
|      9 | 5605 | `		ClosurePresentAdd(pVm, pOut, "line", &sVal);` |
|      9 | 5606 | `		PH7_MemObjRelease(&sVal);` |
|      - | 5607 | `	}` |
|      - | 5608 | ``	/* `static`: the captured `use` variables first (php compiles them into the`` |
|      - | 5609 | ``	 * same table, ahead of the body's own statics), then the body's `static $x`,`` |
|      - | 5610 | `	 * whose value is the live slot once it exists and the compiled initializer` |
|      - | 5611 | `	 * before that — the rule getStaticVariables() already follows. */` |
|     15 | 5612 | `	if( sRef.pFunc ){` |
|     13 | 5613 | `		ph7_hashmap *pHm = PH7_NewHashmap(pVm, 0, 0);` |
|      - | 5614 | `		sxu32 n;` |
|     13 | 5615 | `		if( pHm ){` |
|      - | 5616 | `			ph7_value sMap;` |
|     13 | 5617 | `			ph7_value *pMap = &sMap;` |
|     13 | 5618 | `			ph7_vm_func_closure_env *aEnv =` |
|     12 | 5619 | `				(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|     13 | 5620 | `			ph7_vm_func_static_var *aStatic =` |
|     12 | 5621 | `				(ph7_vm_func_static_var *)SySetBasePtr(&sRef.pFunc->aStatic);` |
|     13 | 5622 | `			PH7_MemObjInitFromArray(pVm, pMap, pHm);` |
|     25 | 5623 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++n ){` |
|     13 | 5624 | `				ph7_value *pVal = &aEnv[n].sValue;` |
|      - | 5625 | `				ph7_value sKey;` |
|     12 | 5626 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|     11 | 5627 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|      - | 5628 | ``					/* PHL carries the auto-captured `$this` as an env entry; php keeps`` |
|      - | 5629 | ``					 * it in its own `this_ptr` slot and never lists it as a static. It`` |
|      - | 5630 | ``					 * is reported below, under `this`. */`` |
|      9 | 5631 | `					continue;` |
|      - | 5632 | `				}` |
|      5 | 5633 | `				if( aEnv[n].nIdx != SXU32_HIGH ){` |
|      - | 5634 | ``					/* `use (&$x)`: the capture is an alias onto a pinned slot, and`` |
|      - | 5635 | `					 * php reports what the slot holds NOW, not the birth value. */` |
|    ! 0 | 5636 | `					ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj, aEnv[n].nIdx);` |
|    ! 0 | 5637 | `					if( pSlot ){` |
|    ! 0 | 5638 | `						pVal = pSlot;` |
|    ! 0 | 5639 | `					}` |
|    ! 0 | 5640 | `				}` |
|      5 | 5641 | `				PH7_MemObjInitFromString(pVm, &sKey, &aEnv[n].sName);` |
|      5 | 5642 | `				ph7_array_add_elem(pMap, &sKey, pVal);` |
|      5 | 5643 | `				PH7_MemObjRelease(&sKey);` |
|      3 | 5644 | `			}` |
|     15 | 5645 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aStatic) ; ++n ){` |
|      3 | 5646 | `				ph7_value *pVal = 0;` |
|      - | 5647 | `				ph7_value sScratch, sKey;` |
|      3 | 5648 | `				int bScratch = 0;` |
|      3 | 5649 | `				if( aStatic[n].nIdx != SXU32_HIGH ){` |
|    ! 0 | 5650 | `					pVal = (ph7_value *)SySetAt(&pVm->aMemObj, aStatic[n].nIdx);` |
|    ! 0 | 5651 | `				}` |
|      3 | 5652 | `				if( pVal == 0 ){` |
|      3 | 5653 | `					PH7_MemObjInit(pVm, &sScratch);` |
|      3 | 5654 | `					if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|      3 | 5655 | `						VmLocalExec(pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|      1 | 5656 | `					}` |
|      3 | 5657 | `					pVal = &sScratch;` |
|      3 | 5658 | `					bScratch = 1;` |
|      1 | 5659 | `				}` |
|      3 | 5660 | `				PH7_MemObjInitFromString(pVm, &sKey, &aStatic[n].sName);` |
|      3 | 5661 | `				ph7_array_add_elem(pMap, &sKey, pVal);` |
|      3 | 5662 | `				PH7_MemObjRelease(&sKey);` |
|      3 | 5663 | `				if( bScratch ){` |
|      3 | 5664 | `					PH7_MemObjRelease(&sScratch);` |
|      1 | 5665 | `				}` |
|      2 | 5666 | `			}` |
|     13 | 5667 | `			if( ph7_array_count(pMap) > 0 ){` |
|      5 | 5668 | `				ClosurePresentAdd(pVm, pOut, "static", pMap);` |
|      2 | 5669 | `			}` |
|     13 | 5670 | `			PH7_MemObjRelease(pMap);` |
|      6 | 5671 | `		}` |
|      6 | 5672 | `	}` |
|      - | 5673 | ``	/* `this`: the bound receiver. php keeps ONE `this_ptr` however the binding`` |
|      - | 5674 | ``	 * happened; PHL keeps two — an explicit bind (`bindTo`, an instance`` |
|      - | 5675 | ``	 * first-class callable) writes the `$__this` slot, while the IMPLICIT capture a`` |
|      - | 5676 | `	 * closure written inside a method gets rides in the closure ENVIRONMENT under` |
|      - | 5677 | ``	 * the name `this`. Either one is php's answer here. */`` |
|      - | 5678 | `	{` |
|      - | 5679 | `		SyString sAttr;` |
|      - | 5680 | `		ph7_value *pBound;` |
|     15 | 5681 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     15 | 5682 | `		pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     15 | 5683 | `		if( (pBound == 0 \|\| (pBound->iFlags & MEMOBJ_OBJ) == 0) && sRef.pFunc ){` |
|     11 | 5684 | `			ph7_vm_func_closure_env *aEnv =` |
|     10 | 5685 | `				(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|      - | 5686 | `			sxu32 n;` |
|     11 | 5687 | `			pBound = 0;` |
|     15 | 5688 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++n ){` |
|     12 | 5689 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|     11 | 5690 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|      9 | 5691 | `					pBound = &aEnv[n].sValue;` |
|      9 | 5692 | `					break;` |
|      - | 5693 | `				}` |
|      3 | 5694 | `			}` |
|      5 | 5695 | `		}` |
|     15 | 5696 | `		if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|      5 | 5697 | `			ClosurePresentAdd(pVm, pOut, "this", pBound);` |
|      2 | 5698 | `		}` |
|      - | 5699 | `	}` |
|      - | 5700 | ``	/* `parameter`: php's cut is required_num_args, which runs through the LAST`` |
|      - | 5701 | `	 * required parameter rather than stopping at the first optional one. */` |
|      - | 5702 | `	{` |
|      - | 5703 | `		ReflectParamDesc sDesc;` |
|     15 | 5704 | `		int nArgTotal = 0, nRequired = 0, i;` |
|     31 | 5705 | `		while( ReflectParamAt(&sRef, nArgTotal, &sDesc) ){` |
|     17 | 5706 | `			if( !sDesc.bOptional ){` |
|     11 | 5707 | `				nRequired = nArgTotal + 1;` |
|      5 | 5708 | `			}` |
|     17 | 5709 | `			nArgTotal++;` |
|      1 | 5710 | `		}` |
|     15 | 5711 | `		if( nArgTotal > 0 ){` |
|      9 | 5712 | `			ph7_hashmap *pHm = PH7_NewHashmap(pVm, 0, 0);` |
|      9 | 5713 | `			if( pHm ){` |
|      - | 5714 | `				ph7_value sMap;` |
|      9 | 5715 | `				ph7_value *pMap = &sMap;` |
|      9 | 5716 | `				PH7_MemObjInitFromArray(pVm, pMap, pHm);` |
|     25 | 5717 | `				for( i = 0 ; i < nArgTotal ; ++i ){` |
|      - | 5718 | `					ph7_value sKey, sWhat;` |
|     17 | 5719 | `					if( !ReflectParamAt(&sRef, i, &sDesc) ){` |
|    ! 0 | 5720 | `						break;` |
|      - | 5721 | `					}` |
|     17 | 5722 | `					PH7_MemObjInitFromString(pVm, &sKey, 0);` |
|     17 | 5723 | `					if( sDesc.bByRef ){` |
|      3 | 5724 | `						PH7_MemObjStringAppend(&sKey, "&", 1);` |
|      1 | 5725 | `					}` |
|     17 | 5726 | `					PH7_MemObjStringAppend(&sKey, "$", 1);` |
|     25 | 5727 | `					PH7_MemObjStringAppend(&sKey, SyStringData(&sDesc.sName),` |
|      8 | 5728 | `						SyStringLength(&sDesc.sName));` |
|     17 | 5729 | `					PH7_MemObjInitFromString(pVm, &sWhat, 0);` |
|     17 | 5730 | `					PH7_MemObjStringAppend(&sWhat,` |
|      8 | 5731 | `						i >= nRequired ? "<optional>" : "<required>",` |
|      8 | 5732 | `						i >= nRequired ? sizeof("<optional>")-1 : sizeof("<required>")-1);` |
|     17 | 5733 | `					ph7_array_add_elem(pMap, &sKey, &sWhat);` |
|     17 | 5734 | `					PH7_MemObjRelease(&sKey);` |
|     17 | 5735 | `					PH7_MemObjRelease(&sWhat);` |
|      9 | 5736 | `				}` |
|      9 | 5737 | `				ClosurePresentAdd(pVm, pOut, "parameter", pMap);` |
|      9 | 5738 | `				PH7_MemObjRelease(pMap);` |
|      4 | 5739 | `			}` |
|      4 | 5740 | `		}` |
|      - | 5741 | `	}` |
|     15 | 5742 | `	return SXRET_OK;` |
|     10 | 5743 | `}` |
|      2 | 5744 | `static int vm_builtin_ReflectionFunc_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5745 | `{` |
|      - | 5746 | `	ReflectFuncRef sRef;` |
|      1 | 5747 | `	SXUNUSED(nArg);` |
|      1 | 5748 | `	SXUNUSED(apArg);` |
|      3 | 5749 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|      3 | 5750 | `	if( ReflectFuncIsInternal(&sRef) ){` |
|      3 | 5751 | `		ph7_result_string(pCtx, "Core", sizeof("Core")-1);` |
|      2 | 5752 | `	}else{` |
|    ! 0 | 5753 | `		ph7_result_bool(pCtx, 0);` |
|      - | 5754 | `	}` |
|      3 | 5755 | `	return PH7_OK;` |
|      2 | 5756 | `}` |
|      4 | 5757 | `static int vm_builtin_ReflectionFunc_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5758 | `{` |
|      - | 5759 | `	ReflectFuncRef sRef;` |
|      2 | 5760 | `	SXUNUSED(nArg);` |
|      2 | 5761 | `	SXUNUSED(apArg);` |
|      5 | 5762 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|      5 | 5763 | `	if( !ReflectFuncIsInternal(&sRef) ){` |
|    ! 0 | 5764 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5765 | `		return PH7_OK;` |
|      - | 5766 | `	}` |
|      5 | 5767 | `	return ReflectCoreExtension(pCtx);` |
|      3 | 5768 | `}` |
|     24 | 5769 | `static int vm_builtin_ReflectionFunc_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5770 | `{` |
|     25 | 5771 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 5772 | `	ReflectFuncRef sRef;` |
|      - | 5773 | `	int rc;` |
|     25 | 5774 | `	if( pThis == 0 \|\| !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pFunc == 0 ){` |
|    ! 0 | 5775 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 | 5776 | `		return PH7_OK;` |
|      - | 5777 | `	}` |
|     25 | 5778 | `	if( sRef.pMeth ){` |
|      - | 5779 | `		/* 4 = Attribute::TARGET_METHOD */` |
|      - | 5780 | `		ph7_value sTarget;` |
|      - | 5781 | `		const char *zClass, *zName;` |
|      - | 5782 | `		int nClass, nName;` |
|     13 | 5783 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     13 | 5784 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     13 | 5785 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     13 | 5786 | `		ph7_value_string(&sTarget, zClass, nClass);` |
|     19 | 5787 | `		rc = ReflectBuildAttrs(pCtx, &sRef.pFunc->aAttrs, "method", &sTarget,` |
|      6 | 5788 | `			zName, nName, 0, 4, nArg, apArg);` |
|     13 | 5789 | `		PH7_MemObjRelease(&sTarget);` |
|     13 | 5790 | `		return rc;` |
|      - | 5791 | `	}` |
|      - | 5792 | `	{` |
|      - | 5793 | `		/* 2 = Attribute::TARGET_FUNCTION */` |
|      - | 5794 | `		ph7_value sTarget;` |
|     13 | 5795 | `		ReflectFuncParamSpec(pCtx, &sTarget);` |
|     19 | 5796 | `		rc = ReflectBuildAttrs(pCtx, &sRef.pFunc->aAttrs, "fn", &sTarget, 0, 0, 0, 2,` |
|      6 | 5797 | `			nArg, apArg);` |
|     13 | 5798 | `		if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|      9 | 5799 | `			PH7_MemObjRelease(&sTarget);` |
|      4 | 5800 | `		}` |
|     13 | 5801 | `		return rc;` |
|      - | 5802 | `	}` |
|     13 | 5803 | `}` |
|      - | 5804 | `/* __toString(): php's export format, still chunk 9. */` |
|     54 | 5805 | `static int vm_builtin_ReflectionFunc_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 5806 | `{` |
|     27 | 5807 | `	SXUNUSED(nArg);` |
|     27 | 5808 | `	SXUNUSED(apArg);` |
|     56 | 5809 | `	return ReflectExportFuncSelf(pCtx);` |
|      2 | 5810 | `}` |
|      - | 5811 | `/* ---- ReflectionFunction ---- */` |
|      - | 5812 | `/* ReflectionFunction::__construct(Closure\|string $function) */` |
|    426 | 5813 | `static int vm_builtin_ReflectionFunction_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 | 5814 | `{` |
|    430 | 5815 | `	ph7_vm *pVm = pCtx->pVm;` |
|    430 | 5816 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 5817 | `	ReflectFuncRef sRef;` |
|      - | 5818 | `	ph7_class_instance *pClo;` |
|    430 | 5819 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 5820 | `		return PH7_OK;` |
|      - | 5821 | `	}` |
|    430 | 5822 | `	pClo = ReflectValueClosure(pVm, apArg[0]);` |
|    430 | 5823 | `	if( !ReflectFuncFill(pCtx->pVm, apArg[0], 0, &sRef) ){` |
|      - | 5824 | `		const char *zName;` |
|      - | 5825 | `		int nName;` |
|     15 | 5826 | `		if( pClo ){` |
|      - | 5827 | `			/* A Closure whose body no table holds: still a valid reflection` |
|      - | 5828 | `			 * target, so record it and let the accessors answer emptily. */` |
|    ! 0 | 5829 | `			PH7_NativeSetAttrObj(pVm, pThis, RF_CL, pClo);` |
|    ! 0 | 5830 | `			return PH7_OK;` |
|      - | 5831 | `		}` |
|     15 | 5832 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|     21 | 5833 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      6 | 5834 | `			"Function %.*s() does not exist", nName, zName);` |
|      - | 5835 | `	}` |
|    418 | 5836 | `	if( pClo ){` |
|     66 | 5837 | `		PH7_NativeSetAttrObj(pVm, pThis, RF_CL, pClo);` |
|     31 | 5838 | `	}` |
|    418 | 5839 | `	if( sRef.pFunc ){` |
|    220 | 5840 | `		int bAnon = (sRef.pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|    216 | 5841 | `		if( !bAnon && SyStringLength(&sRef.pFunc->sName) > 9` |
|    114 | 5842 | `		 && (SyMemcmp(SyStringData(&sRef.pFunc->sName), "[lambda_", 8) == 0` |
|     39 | 5843 | `		  \|\| SyMemcmp(SyStringData(&sRef.pFunc->sName), "[closure_", 9) == 0) ){` |
|      3 | 5844 | `			bAnon = 1;` |
|      1 | 5845 | `		}` |
|    220 | 5846 | `		if( bAnon ){` |
|      - | 5847 | ``			/* php 8.4 names an anonymous function `{closure:SCOPE:LINE}`, where SCOPE is`` |
|      - | 5848 | `			 * the enclosing function and only the FILE at top level — the compiler` |
|      - | 5849 | `			 * recorded it (sClosureName); the file form is the fallback for a closure` |
|      - | 5850 | `			 * built without one. */` |
|      - | 5851 | `			char zBuf[512];` |
|     42 | 5852 | `			const char *zShow = SyStringData(&sRef.pFunc->sClosureName);` |
|     42 | 5853 | `			int nShow = (int)SyStringLength(&sRef.pFunc->sClosureName);` |
|     42 | 5854 | `			if( nShow < 1 ){` |
|    ! 0 | 5855 | `				const char *zFile = SyStringLength(&sRef.pFunc->sFile) > 0` |
|    ! 0 | 5856 | `					? SyStringData(&sRef.pFunc->sFile) : "";` |
|    ! 0 | 5857 | `				int nFile = (int)SyStringLength(&sRef.pFunc->sFile);` |
|    ! 0 | 5858 | `				nShow = SyBufferFormat(zBuf, sizeof(zBuf), "{closure:%.*s:%u}",` |
|    ! 0 | 5859 | `					nFile, zFile, sRef.pFunc->nLine);` |
|    ! 0 | 5860 | `				zShow = zBuf;` |
|    ! 0 | 5861 | `			}` |
|     42 | 5862 | `			PH7_NativeSetAttrStr(pVm, pThis, "name", zShow, nShow);` |
|     42 | 5863 | `			if( pClo == 0 ){` |
|      - | 5864 | `				/* Named by its lambda name: keep a Closure so the accessors can` |
|      - | 5865 | `				 * still reach the captured scope. */` |
|    ! 0 | 5866 | `				PH7_NativeSetAttrObj(pVm, pThis, RF_CL,` |
|    ! 0 | 5867 | `					PH7_VmNewClosure(pVm, &sRef.pFunc->sName, 0, 0));` |
|    ! 0 | 5868 | `			}` |
|     42 | 5869 | `			return PH7_OK;` |
|      - | 5870 | `		}` |
|    270 | 5871 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sRef.pFunc->sName),` |
|    178 | 5872 | `			(int)SyStringLength(&sRef.pFunc->sName));` |
|    181 | 5873 | `		return PH7_OK;` |
|      - | 5874 | `	}` |
|    299 | 5875 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sRef.pHost->sName),` |
|    198 | 5876 | `		(int)SyStringLength(&sRef.pHost->sName));` |
|    200 | 5877 | `	return PH7_OK;` |
|    217 | 5878 | `}` |
|      6 | 5879 | `static int vm_builtin_ReflectionFunction_isAnonymous(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5880 | `{` |
|      7 | 5881 | `	return vm_builtin_ReflectionFunc_isClosure(pCtx, nArg, apArg);` |
|      1 | 5882 | `}` |
|    ! 0 | 5883 | `static int vm_builtin_ReflectionFunction_isDisabled(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 5884 | `{` |
|    ! 0 | 5885 | `	SXUNUSED(nArg);` |
|    ! 0 | 5886 | `	SXUNUSED(apArg);` |
|    ! 0 | 5887 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 | 5888 | `	return PH7_OK;` |
|    ! 0 | 5889 | `}` |
|      - | 5890 | `/* The callable a ReflectionFunction invokes: the Closure it holds, or its name. */` |
|     18 | 5891 | `static void ReflectFunctionCallable(ph7_context *pCtx, ph7_value *pOut)` |
|      1 | 5892 | `{` |
|     19 | 5893 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 5894 | `	ph7_class_instance *pClo;` |
|     19 | 5895 | `	const char *zName = "";` |
|     19 | 5896 | `	int nName = 0;` |
|     19 | 5897 | `	PH7_MemObjInit(pCtx->pVm, pOut);` |
|     19 | 5898 | `	if( pThis == 0 ){` |
|    ! 0 | 5899 | `		return;` |
|      - | 5900 | `	}` |
|     19 | 5901 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|     19 | 5902 | `	if( pClo ){` |
|      9 | 5903 | `		pOut->x.pOther = pClo;` |
|      9 | 5904 | `		pOut->iFlags = MEMOBJ_OBJ;` |
|      9 | 5905 | `		return;` |
|      - | 5906 | `	}` |
|     11 | 5907 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     11 | 5908 | `	ph7_value_string(pOut, zName, nName);` |
|     10 | 5909 | `}` |
|     18 | 5910 | `static int ReflectFunctionInvoke(ph7_context *pCtx, int nCall, ph7_value **apCall)` |
|      1 | 5911 | `{` |
|      - | 5912 | `	ph7_value sTarget, sResult;` |
|      - | 5913 | `	sxi32 rc;` |
|     19 | 5914 | `	ReflectFunctionCallable(pCtx, &sTarget);` |
|     19 | 5915 | `	PH7_MemObjInit(pCtx->pVm, &sResult);` |
|     19 | 5916 | `	sResult.nIdx = SXU32_HIGH;` |
|     19 | 5917 | `	rc = PH7_VmCallUserFunction(pCtx->pVm, &sTarget, nCall, apCall, &sResult);` |
|     19 | 5918 | `	if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|     11 | 5919 | `		PH7_MemObjRelease(&sTarget);` |
|      5 | 5920 | `	}` |
|     19 | 5921 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|    ! 0 | 5922 | `		PH7_MemObjRelease(&sResult);` |
|    ! 0 | 5923 | `		return rc;` |
|      - | 5924 | `	}` |
|     19 | 5925 | `	ph7_result_value(pCtx, &sResult);` |
|     19 | 5926 | `	PH7_MemObjRelease(&sResult);` |
|     19 | 5927 | `	return PH7_OK;` |
|     10 | 5928 | `}` |
|     16 | 5929 | `static int vm_builtin_ReflectionFunction_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5930 | `{` |
|     17 | 5931 | `	return ReflectFunctionInvoke(pCtx, nArg, apArg);` |
|      1 | 5932 | `}` |
|      2 | 5933 | `static int vm_builtin_ReflectionFunction_invokeArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5934 | `{` |
|      - | 5935 | `	SySet aCall;` |
|      - | 5936 | `	int rc;` |
|      3 | 5937 | `	SySetInit(&aCall, &pCtx->pVm->sAllocator, sizeof(ph7_value *));` |
|      3 | 5938 | `	if( nArg > 0 ){` |
|      3 | 5939 | `		ReflectCollectArgs(pCtx, apArg[0], &aCall, 0);` |
|      1 | 5940 | `	}` |
|      3 | 5941 | `	rc = ReflectFunctionInvoke(pCtx, (int)SySetUsed(&aCall), (ph7_value **)SySetBasePtr(&aCall));` |
|      3 | 5942 | `	SySetRelease(&aCall);` |
|      3 | 5943 | `	return rc;` |
|      1 | 5944 | `}` |
|      6 | 5945 | `static int vm_builtin_ReflectionFunction_getClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 5946 | `{` |
|      7 | 5947 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 5948 | `	ph7_class_instance *pClo;` |
|      7 | 5949 | `	const char *zName = "";` |
|      7 | 5950 | `	int nName = 0;` |
|      - | 5951 | `	SyString sName;` |
|      3 | 5952 | `	SXUNUSED(nArg);` |
|      3 | 5953 | `	SXUNUSED(apArg);` |
|      7 | 5954 | `	if( pThis == 0 ){` |
|    ! 0 | 5955 | `		ph7_result_null(pCtx);` |
|    ! 0 | 5956 | `		return PH7_OK;` |
|      - | 5957 | `	}` |
|      7 | 5958 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|      7 | 5959 | `	if( pClo ){` |
|      - | 5960 | `		/* Already a Closure: hand the same instance back */` |
|      3 | 5961 | `		return ReflectResultExistingObject(pCtx, pClo);` |
|      - | 5962 | `	}` |
|      5 | 5963 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      5 | 5964 | `	SyStringInitFromBuf(&sName, zName, nName);` |
|      5 | 5965 | `	return ReflectResultObject(pCtx, PH7_VmNewClosure(pCtx->pVm, &sName, 0, 0));` |
|      4 | 5966 | `}` |
|      - | 5967 | `/* ---- ReflectionMethod ---- */` |
|      - | 5968 | `/* ReflectionMethod::__construct(object\|string $objectOrMethod, ?string $method = null) */` |
|    478 | 5969 | `static int vm_builtin_ReflectionMethod_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 | 5970 | `{` |
|    483 | 5971 | `	ph7_vm *pVm = pCtx->pVm;` |
|    483 | 5972 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 5973 | `	ph7_class *pClass;` |
|      - | 5974 | `	SyHashEntry *pEntry;` |
|      - | 5975 | `	ph7_value sClass, sMethod;` |
|      - | 5976 | `	const char *zMethod;` |
|    483 | 5977 | `	int nMethod, rc = PH7_OK;` |
|    483 | 5978 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|    ! 0 | 5979 | `		return PH7_OK;` |
|      - | 5980 | `	}` |
|    483 | 5981 | `	PH7_MemObjInit(pVm, &sClass);` |
|    483 | 5982 | `	PH7_MemObjInit(pVm, &sMethod);` |
|    484 | 5983 | `	if( nArg < 2 \|\| ph7_value_is_null(apArg[1]) ){` |
|      - | 5984 | `		/* One-argument form: "Class::method" */` |
|      - | 5985 | `		const char *zSpec;` |
|      3 | 5986 | `		int nSpec, iSep = -1, k;` |
|      3 | 5987 | `		if( (apArg[0]->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 5988 | `			PH7_MemObjRelease(&sClass);` |
|    ! 0 | 5989 | `			PH7_MemObjRelease(&sMethod);` |
|    ! 0 | 5990 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 5991 | `				"The parameter class is expected to be either a string or an object");` |
|      - | 5992 | `		}` |
|      3 | 5993 | `		zSpec = ph7_value_to_string(apArg[0], &nSpec);` |
|     19 | 5994 | `		for( k = 0 ; k + 1 < nSpec ; k++ ){` |
|     19 | 5995 | `			if( zSpec[k] == ':' && zSpec[k+1] == ':' ){` |
|      3 | 5996 | `				iSep = k;` |
|      3 | 5997 | `				break;` |
|      - | 5998 | `			}` |
|      9 | 5999 | `		}` |
|      3 | 6000 | `		if( iSep < 0 ){` |
|    ! 0 | 6001 | `			PH7_MemObjRelease(&sClass);` |
|    ! 0 | 6002 | `			PH7_MemObjRelease(&sMethod);` |
|    ! 0 | 6003 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 | 6004 | `				"Invalid method name %.*s", nSpec, zSpec);` |
|      - | 6005 | `		}` |
|      - | 6006 | `		/* php 8.3 deprecated the one-argument spelling in favour of the static` |
|      - | 6007 | `		 * factory. createFromMethodName() splits the name ITSELF and constructs` |
|      - | 6008 | `		 * with two, so it does not trip this. */` |
|      3 | 6009 | `		PH7_VmThrowError(pVm, 0, E_DEPRECATED,` |
|      - | 6010 | `			"Calling ReflectionMethod::__construct() with 1 argument is deprecated, "` |
|      - | 6011 | `			"use ReflectionMethod::createFromMethodName() instead");` |
|      3 | 6012 | `		ph7_value_string(&sClass, zSpec, iSep);` |
|      3 | 6013 | `		ph7_value_string(&sMethod, &zSpec[iSep+2], nSpec - iSep - 2);` |
|      2 | 6014 | `	}else{` |
|    481 | 6015 | `		PH7_MemObjStore(apArg[0], &sClass);` |
|    481 | 6016 | `		PH7_MemObjStore(apArg[1], &sMethod);` |
|      - | 6017 | `	}` |
|    483 | 6018 | `	pClass = ReflectResolveClass(pVm, &sClass);` |
|    483 | 6019 | `	if( pClass == 0 ){` |
|      - | 6020 | `		const char *zName;` |
|      - | 6021 | `		int nName;` |
|      3 | 6022 | `		zName = ph7_value_to_string(&sClass, &nName);` |
|      4 | 6023 | `		rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 6024 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      3 | 6025 | `		goto Done;` |
|      - | 6026 | `	}` |
|    481 | 6027 | `	zMethod = ph7_value_to_string(&sMethod, &nMethod);` |
|    481 | 6028 | `	pEntry = ReflectFindMethodEntry(pClass, zMethod, nMethod);` |
|    481 | 6029 | `	if( pEntry == 0 ){` |
|     10 | 6030 | `		rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 | 6031 | `			"Method %z::%.*s() does not exist", &pClass->sName, nMethod, zMethod);` |
|      7 | 6032 | `		goto Done;` |
|      - | 6033 | `	}` |
|      - | 6034 | `	{` |
|      - | 6035 | `		/* php's $class is the DECLARING class, not the one the lookup went` |
|      - | 6036 | ``		 * through: `new ReflectionMethod('Kid','bm')` on an inherited method`` |
|      - | 6037 | `		 * reports Base. */` |
|    475 | 6038 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, (ph7_class_method *)pEntry->pUserData);` |
|    710 | 6039 | `		PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pDecl->sName),` |
|    470 | 6040 | `			(int)SyStringLength(&pDecl->sName));` |
|      - | 6041 | `	}` |
|      - | 6042 | `	/* The DECLARED spelling, whatever case was asked for. */` |
|    475 | 6043 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", (const char *)pEntry->pKey, (int)pEntry->nKeyLen);` |
|    239 | 6044 | `Done:` |
|    483 | 6045 | `	PH7_MemObjRelease(&sClass);` |
|    483 | 6046 | `	PH7_MemObjRelease(&sMethod);` |
|    483 | 6047 | `	return rc;` |
|    244 | 6048 | `}` |
|      - | 6049 | `/* ReflectionMethod::createFromMethodName(string $method): static */` |
|      4 | 6050 | `static int vm_builtin_ReflectionMethod_createFromMethodName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6051 | `{` |
|      - | 6052 | `	ph7_class_instance *pOut;` |
|      - | 6053 | `	ph7_value sClass, sMethod;` |
|      - | 6054 | `	ph7_value *apCtor[2];` |
|      - | 6055 | `	const char *zSpec;` |
|      5 | 6056 | `	int nSpec, iSep = -1, k;` |
|      - | 6057 | `	sxi32 rc;` |
|      5 | 6058 | `	if( nArg < 1 ){` |
|    ! 0 | 6059 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6060 | `		return PH7_OK;` |
|      - | 6061 | `	}` |
|      - | 6062 | `	/* Split here rather than letting the constructor do it: the one-argument` |
|      - | 6063 | `	 * constructor is the DEPRECATED spelling this factory exists to replace. */` |
|      5 | 6064 | `	zSpec = ph7_value_to_string(apArg[0], &nSpec);` |
|     43 | 6065 | `	for( k = 0 ; k + 1 < nSpec ; k++ ){` |
|     43 | 6066 | `		if( zSpec[k] == ':' && zSpec[k+1] == ':' ){` |
|      5 | 6067 | `			iSep = k;` |
|      5 | 6068 | `			break;` |
|      - | 6069 | `		}` |
|     20 | 6070 | `	}` |
|      5 | 6071 | `	if( iSep < 0 ){` |
|    ! 0 | 6072 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 | 6073 | `			"Invalid method name %.*s", nSpec, zSpec);` |
|      - | 6074 | `	}` |
|      5 | 6075 | `	PH7_MemObjInit(pCtx->pVm, &sClass);` |
|      5 | 6076 | `	PH7_MemObjInit(pCtx->pVm, &sMethod);` |
|      5 | 6077 | `	ph7_value_string(&sClass, zSpec, iSep);` |
|      5 | 6078 | `	ph7_value_string(&sMethod, &zSpec[iSep+2], nSpec - iSep - 2);` |
|      5 | 6079 | `	apCtor[0] = &sClass;` |
|      5 | 6080 | `	apCtor[1] = &sMethod;` |
|      5 | 6081 | `	pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|      5 | 6082 | `	PH7_MemObjRelease(&sClass);` |
|      5 | 6083 | `	PH7_MemObjRelease(&sMethod);` |
|      5 | 6084 | `	if( pOut == 0 ){` |
|    ! 0 | 6085 | `		if( rc != PH7_OK ){` |
|    ! 0 | 6086 | `			return rc;` |
|      - | 6087 | `		}` |
|    ! 0 | 6088 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6089 | `		return PH7_OK;` |
|      - | 6090 | `	}` |
|      5 | 6091 | `	return ReflectResultObject(pCtx, pOut);` |
|      3 | 6092 | `}` |
|      - | 6093 | `/* The visibility/modifier predicates, all off the method record. */` |
|     24 | 6094 | `static int ReflectMethodFlag(ph7_context *pCtx, int iWhat)` |
|      1 | 6095 | `{` |
|      - | 6096 | `	ReflectFuncRef sRef;` |
|     25 | 6097 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|    ! 0 | 6098 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6099 | `		return PH7_OK;` |
|      - | 6100 | `	}` |
|     25 | 6101 | `	switch( iWhat ){` |
|      5 | 6102 | `	case 0: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PUBLIC); break;` |
|      9 | 6103 | `	case 1: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PRIVATE); break;` |
|      3 | 6104 | `	case 2: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PROTECTED); break;` |
|    ! 0 | 6105 | `	case 3: ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0); break;` |
|     11 | 6106 | `	default: ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0); break;` |
|      - | 6107 | `	}` |
|     25 | 6108 | `	return PH7_OK;` |
|     13 | 6109 | `}` |
|      - | 6110 | `#define REFLECT_METHOD_FLAG(NAME,WHAT) \` |
|      - | 6111 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 6112 | `	{ \` |
|      - | 6113 | `		SXUNUSED(nArg); \` |
|      - | 6114 | `		SXUNUSED(apArg); \` |
|      - | 6115 | `		return ReflectMethodFlag(pCtx, WHAT); \` |
|      - | 6116 | `	}` |
|      5 | 6117 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isPublic, 0)` |
|      9 | 6118 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isPrivate, 1)` |
|      3 | 6119 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isProtected, 2)` |
|    ! 0 | 6120 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isAbstract, 3)` |
|     11 | 6121 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isFinal, 4)` |
|      - | 6122 |  |
|      - | 6123 | `/* isConstructor()/isDestructor() read the NAME, not the record: a trait` |
|      - | 6124 | `` * `use T { m as __construct; }` alias is the constructor under that key. */`` |
|      4 | 6125 | `static int ReflectMethodNameIs(ph7_context *pCtx, const char *zWant, int nWant)` |
|      2 | 6126 | `{` |
|      6 | 6127 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      6 | 6128 | `	const char *zName = "";` |
|      6 | 6129 | `	int nName = 0;` |
|      6 | 6130 | `	if( pThis ){` |
|      6 | 6131 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|      2 | 6132 | `	}` |
|      6 | 6133 | `	ph7_result_bool(pCtx, nName == nWant && SyStrnicmp(zName, zWant, (sxu32)nWant) == 0);` |
|      6 | 6134 | `	return PH7_OK;` |
|      2 | 6135 | `}` |
|      4 | 6136 | `static int vm_builtin_ReflectionMethod_isConstructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 6137 | `{` |
|      2 | 6138 | `	SXUNUSED(nArg);` |
|      2 | 6139 | `	SXUNUSED(apArg);` |
|      6 | 6140 | `	return ReflectMethodNameIs(pCtx, "__construct", sizeof("__construct")-1);` |
|      2 | 6141 | `}` |
|    ! 0 | 6142 | `static int vm_builtin_ReflectionMethod_isDestructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 6143 | `{` |
|    ! 0 | 6144 | `	SXUNUSED(nArg);` |
|    ! 0 | 6145 | `	SXUNUSED(apArg);` |
|    ! 0 | 6146 | `	return ReflectMethodNameIs(pCtx, "__destruct", sizeof("__destruct")-1);` |
|    ! 0 | 6147 | `}` |
|      8 | 6148 | `static int vm_builtin_ReflectionMethod_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6149 | `{` |
|      - | 6150 | `	ReflectFuncRef sRef;` |
|      4 | 6151 | `	SXUNUSED(nArg);` |
|      4 | 6152 | `	SXUNUSED(apArg);` |
|      9 | 6153 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|    ! 0 | 6154 | `		ph7_result_int(pCtx, 0);` |
|    ! 0 | 6155 | `		return PH7_OK;` |
|      - | 6156 | `	}` |
|      9 | 6157 | `	ph7_result_int64(pCtx, ReflectMethodModifiers(sRef.pMeth));` |
|      9 | 6158 | `	return PH7_OK;` |
|      5 | 6159 | `}` |
|    178 | 6160 | `static int vm_builtin_ReflectionMethod_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 6161 | `{` |
|      - | 6162 | `	ReflectFuncRef sRef;` |
|     89 | 6163 | `	SXUNUSED(nArg);` |
|     89 | 6164 | `	SXUNUSED(apArg);` |
|    181 | 6165 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 | 6166 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6167 | `		return PH7_OK;` |
|      - | 6168 | `	}` |
|    181 | 6169 | `	return ReflectResultClassOf(pCtx, ReflectFuncDeclClass(&sRef));` |
|     92 | 6170 | `}` |
|      - | 6171 | `/*` |
|      - | 6172 | ` * The receiver check php runs before invoking or binding: a non-static method` |
|      - | 6173 | ` * needs an instance OF ITS DECLARING CLASS; a static one ignores whatever was` |
|      - | 6174 | ` * handed over. *ppThis is cleared for a static method.` |
|      - | 6175 | ` */` |
|     40 | 6176 | `static sxi32 ReflectMethodReceiver(ph7_context *pCtx, ReflectFuncRef *pRef,` |
|      - | 6177 | `	ph7_value *pObject, ph7_class_instance **ppThis, int bClosure)` |
|      1 | 6178 | `{` |
|     41 | 6179 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     41 | 6180 | `	const char *zClass = "", *zName = "";` |
|     41 | 6181 | `	int nClass = 0, nName = 0;` |
|     41 | 6182 | `	ph7_class *pDecl = ReflectFuncDeclClass(pRef);` |
|     41 | 6183 | `	*ppThis = 0;` |
|     41 | 6184 | `	if( pThis ){` |
|     41 | 6185 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     41 | 6186 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     20 | 6187 | `	}` |
|     41 | 6188 | `	if( pRef->pMeth && (pRef->pMeth->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|     11 | 6189 | `		return PH7_OK;` |
|      - | 6190 | `	}` |
|     31 | 6191 | `	if( pObject == 0 \|\| (pObject->iFlags & MEMOBJ_OBJ) == 0 ){` |
|      9 | 6192 | `		if( bClosure ){` |
|      5 | 6193 | `			return PH7_VmThrowException(pCtx, "ValueError",` |
|      - | 6194 | `				"ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods");` |
|      - | 6195 | `		}` |
|      7 | 6196 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 6197 | `			"Trying to invoke non static method %.*s::%.*s() without an object",` |
|      2 | 6198 | `			nClass, zClass, nName, zName);` |
|      - | 6199 | `	}` |
|     23 | 6200 | `	*ppThis = (ph7_class_instance *)pObject->x.pOther;` |
|     23 | 6201 | `	if( pDecl && !PH7_VmInstanceOf((*ppThis)->pClass, pDecl) ){` |
|      3 | 6202 | `		*ppThis = 0;` |
|      3 | 6203 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 6204 | `			"Given object is not an instance of the class this method was declared in");` |
|      - | 6205 | `	}` |
|     21 | 6206 | `	return PH7_OK;` |
|     21 | 6207 | `}` |
|     24 | 6208 | `static int ReflectMethodInvoke(ph7_context *pCtx, ph7_value *pObject, int nCall, ph7_value **apCall)` |
|      1 | 6209 | `{` |
|     25 | 6210 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 6211 | `	ReflectFuncRef sRef;` |
|     25 | 6212 | `	ph7_class_instance *pRecv = 0;` |
|      - | 6213 | `	ph7_value sResult;` |
|      - | 6214 | `	sxi32 rc;` |
|     25 | 6215 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|    ! 0 | 6216 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6217 | `		return PH7_OK;` |
|      - | 6218 | `	}` |
|     25 | 6219 | `	rc = ReflectMethodReceiver(pCtx, &sRef, pObject, &pRecv, 0);` |
|     25 | 6220 | `	if( rc != PH7_OK ){` |
|      7 | 6221 | `		return rc;` |
|      - | 6222 | `	}` |
|     19 | 6223 | `	PH7_MemObjInit(pVm, &sResult);` |
|     19 | 6224 | `	sResult.nIdx = SXU32_HIGH;` |
|      - | 6225 | `	/* Reflection ignores method visibility (PHP 8.1+); the flag is consumed by` |
|      - | 6226 | `	 * the first OP_CALL, i.e. this synthetic one. */` |
|     19 | 6227 | `	pVm->bReflectBypass = 1;` |
|     19 | 6228 | `	rc = PH7_VmCallClassMethod(pVm, pRecv, sRef.pMeth, &sResult, nCall, apCall);` |
|     19 | 6229 | `	pVm->bReflectBypass = 0;` |
|     19 | 6230 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|    ! 0 | 6231 | `		PH7_MemObjRelease(&sResult);` |
|    ! 0 | 6232 | `		return rc;` |
|      - | 6233 | `	}` |
|     19 | 6234 | `	ph7_result_value(pCtx, &sResult);` |
|     19 | 6235 | `	PH7_MemObjRelease(&sResult);` |
|     19 | 6236 | `	return PH7_OK;` |
|     13 | 6237 | `}` |
|     20 | 6238 | `static int vm_builtin_ReflectionMethod_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6239 | `{` |
|     41 | 6240 | `	return ReflectMethodInvoke(pCtx, nArg > 0 ? apArg[0] : 0,` |
|     20 | 6241 | `		nArg > 1 ? nArg - 1 : 0, nArg > 1 ? &apArg[1] : 0);` |
|      1 | 6242 | `}` |
|      4 | 6243 | `static int vm_builtin_ReflectionMethod_invokeArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6244 | `{` |
|      - | 6245 | `	SySet aCall;` |
|      - | 6246 | `	int rc;` |
|      5 | 6247 | `	SySetInit(&aCall, &pCtx->pVm->sAllocator, sizeof(ph7_value *));` |
|      5 | 6248 | `	if( nArg > 1 ){` |
|      5 | 6249 | `		ReflectCollectArgs(pCtx, apArg[1], &aCall, 0);` |
|      2 | 6250 | `	}` |
|      3 | 6251 | `	rc = ReflectMethodInvoke(pCtx, nArg > 0 ? apArg[0] : 0,` |
|      4 | 6252 | `		(int)SySetUsed(&aCall), (ph7_value **)SySetBasePtr(&aCall));` |
|      5 | 6253 | `	SySetRelease(&aCall);` |
|      5 | 6254 | `	return rc;` |
|      1 | 6255 | `}` |
|     16 | 6256 | `static int vm_builtin_ReflectionMethod_getClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6257 | `{` |
|      - | 6258 | `	ReflectFuncRef sRef;` |
|     17 | 6259 | `	ph7_class_instance *pRecv = 0;` |
|      - | 6260 | `	sxi32 rc;` |
|     17 | 6261 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 \|\| sRef.pClass == 0 ){` |
|    ! 0 | 6262 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6263 | `		return PH7_OK;` |
|      - | 6264 | `	}` |
|     17 | 6265 | `	rc = ReflectMethodReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pRecv, 1);` |
|     17 | 6266 | `	if( rc != PH7_OK ){` |
|      5 | 6267 | `		return rc;` |
|      - | 6268 | `	}` |
|      - | 6269 | `	{` |
|      - | 6270 | `		/* php hands back a closure over the RESOLVED method and reflection is allowed past` |
|      - | 6271 | `		 * protection (8.1+), so this one must dispatch without a second visibility decision —` |
|      - | 6272 | `		 * the FCC_METHOD mark is what tells the unwrap its $__fn is a screened METHOD name. */` |
|     19 | 6273 | `		ph7_class_instance *pClo = PH7_VmNewClosure(pCtx->pVm, &sRef.pMeth->sFunc.sName,` |
|     12 | 6274 | `			pRecv, &sRef.pClass->sName);` |
|     13 | 6275 | `		if( pClo ){` |
|     13 | 6276 | `			pClo->iFlags \|= VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SCREENED;` |
|      6 | 6277 | `		}` |
|     13 | 6278 | `		return ReflectResultObject(pCtx, pClo);` |
|      - | 6279 | `	}` |
|      9 | 6280 | `}` |
|      - | 6281 | `/* php 8.1 made every reflected member accessible; the setter is a no-op it kept. */` |
|    ! 0 | 6282 | `static int vm_builtin_ReflectionMethod_setAccessible(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 6283 | `{` |
|    ! 0 | 6284 | `	SXUNUSED(pCtx);` |
|    ! 0 | 6285 | `	SXUNUSED(nArg);` |
|    ! 0 | 6286 | `	SXUNUSED(apArg);` |
|    ! 0 | 6287 | `	return PH7_OK;` |
|    ! 0 | 6288 | `}` |
|      - | 6289 | `/*` |
|      - | 6290 | ` * The class this method OVERRIDES it from: the nearest base declaring a` |
|      - | 6291 | ` * same-named non-private method, else the first interface that declares one.` |
|      - | 6292 | ` */` |
|    254 | 6293 | `static ph7_class * ReflectPrototypeIn(ph7_context *pCtx, ph7_class *pClass,` |
|      - | 6294 | `	const char *zName, int nName, int bIfaceToo)` |
|      1 | 6295 | `{` |
|      - | 6296 | `	ph7_class *pWalk;` |
|    255 | 6297 | `	int iDepth = 0;` |
|    255 | 6298 | `	if( pClass == 0 \|\| nName < 1 ){` |
|    ! 0 | 6299 | `		return 0;` |
|      - | 6300 | `	}` |
|    323 | 6301 | `	for( pWalk = pClass->pBase ; pWalk && iDepth <= REFLECT_WALK_MAX_DEPTH ; pWalk = pWalk->pBase ){` |
|     93 | 6302 | `		SyHashEntry *pEntry = ReflectFindMethodEntry(pWalk, zName, nName);` |
|     93 | 6303 | `		if( pEntry ){` |
|     25 | 6304 | `			ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|     25 | 6305 | `			if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|     25 | 6306 | `				return ReflectMethodDeclClass(pWalk, pMeth);` |
|      - | 6307 | `			}` |
|    ! 0 | 6308 | `		}` |
|     69 | 6309 | `		iDepth++;` |
|     35 | 6310 | `	}` |
|    231 | 6311 | `	if( !bIfaceToo ){` |
|      - | 6312 | ``		/* php's `overwrites` tag asks only about the PARENT CHAIN: a method that`` |
|      - | 6313 | ``		 * first appears in an interface is a `prototype`, never an overwrite. */`` |
|     91 | 6314 | `		return 0;` |
|      - | 6315 | `	}` |
|      - | 6316 | `	{` |
|      - | 6317 | `		SySet aSet;` |
|      - | 6318 | `		ph7_class **apIface;` |
|    141 | 6319 | `		ph7_class *pFound = 0;` |
|      - | 6320 | `		sxu32 n;` |
|    141 | 6321 | `		SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|    141 | 6322 | `		ReflectInterfacesOf(pClass, &aSet);` |
|    141 | 6323 | `		apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|    249 | 6324 | `		for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|    167 | 6325 | `			if( ReflectFindMethodEntry(apIface[n], zName, nName) ){` |
|     59 | 6326 | `				pFound = apIface[n];` |
|     59 | 6327 | `				break;` |
|      - | 6328 | `			}` |
|     55 | 6329 | `		}` |
|    141 | 6330 | `		SySetRelease(&aSet);` |
|    141 | 6331 | `		return pFound;` |
|      - | 6332 | `	}` |
|    128 | 6333 | `}` |
|      - | 6334 | `/* The same question asked by a ReflectionMethod receiver, which carries the` |
|      - | 6335 | `` * method name on `$this`. */`` |
|     48 | 6336 | `static ph7_class * ReflectMethodPrototype(ph7_context *pCtx, ReflectFuncRef *pRef)` |
|      1 | 6337 | `{` |
|     49 | 6338 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     49 | 6339 | `	const char *zName = "";` |
|     49 | 6340 | `	int nName = 0;` |
|     49 | 6341 | `	if( pThis == 0 ){` |
|    ! 0 | 6342 | `		return 0;` |
|      - | 6343 | `	}` |
|     49 | 6344 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     49 | 6345 | `	return ReflectPrototypeIn(pCtx, pRef->pClass, zName, nName, 1);` |
|     25 | 6346 | `}` |
|     22 | 6347 | `static int vm_builtin_ReflectionMethod_hasPrototype(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6348 | `{` |
|      - | 6349 | `	ReflectFuncRef sRef;` |
|     11 | 6350 | `	SXUNUSED(nArg);` |
|     11 | 6351 | `	SXUNUSED(apArg);` |
|     23 | 6352 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 | 6353 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6354 | `		return PH7_OK;` |
|      - | 6355 | `	}` |
|     23 | 6356 | `	ph7_result_bool(pCtx, ReflectMethodPrototype(pCtx, &sRef) != 0);` |
|     23 | 6357 | `	return PH7_OK;` |
|     12 | 6358 | `}` |
|     26 | 6359 | `static int vm_builtin_ReflectionMethod_getPrototype(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6360 | `{` |
|     27 | 6361 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 6362 | `	ReflectFuncRef sRef;` |
|      - | 6363 | `	ph7_class *pProto;` |
|     27 | 6364 | `	const char *zClass = "", *zName = "";` |
|     27 | 6365 | `	int nClass = 0, nName = 0;` |
|     13 | 6366 | `	SXUNUSED(nArg);` |
|     13 | 6367 | `	SXUNUSED(apArg);` |
|     27 | 6368 | `	if( pThis == 0 \|\| !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 | 6369 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6370 | `		return PH7_OK;` |
|      - | 6371 | `	}` |
|     27 | 6372 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     27 | 6373 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     27 | 6374 | `	pProto = ReflectMethodPrototype(pCtx, &sRef);` |
|     27 | 6375 | `	if( pProto == 0 ){` |
|      7 | 6376 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 | 6377 | `			"Method %.*s::%.*s does not have a prototype", nClass, zClass, nName, zName);` |
|      - | 6378 | `	}` |
|      - | 6379 | `	{` |
|      - | 6380 | `		ph7_value sClass, sName;` |
|      - | 6381 | `		ph7_value *apCtor[2];` |
|      - | 6382 | `		ph7_class_instance *pOut;` |
|      - | 6383 | `		sxi32 rc;` |
|     23 | 6384 | `		PH7_MemObjInit(pCtx->pVm, &sClass);` |
|     23 | 6385 | `		PH7_MemObjInit(pCtx->pVm, &sName);` |
|     23 | 6386 | `		ph7_value_string(&sClass, SyStringData(&pProto->sName), (int)SyStringLength(&pProto->sName));` |
|     23 | 6387 | `		ph7_value_string(&sName, zName, nName);` |
|     23 | 6388 | `		apCtor[0] = &sClass;` |
|     23 | 6389 | `		apCtor[1] = &sName;` |
|     23 | 6390 | `		pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|     23 | 6391 | `		PH7_MemObjRelease(&sClass);` |
|     23 | 6392 | `		PH7_MemObjRelease(&sName);` |
|     23 | 6393 | `		if( pOut == 0 ){` |
|    ! 0 | 6394 | `			if( rc != PH7_OK ){` |
|    ! 0 | 6395 | `				return rc;` |
|      - | 6396 | `			}` |
|    ! 0 | 6397 | `			ph7_result_null(pCtx);` |
|    ! 0 | 6398 | `			return PH7_OK;` |
|      - | 6399 | `		}` |
|     23 | 6400 | `		return ReflectResultObject(pCtx, pOut);` |
|      - | 6401 | `	}` |
|     14 | 6402 | `}` |
|      - | 6403 | `/* ---- ReflectionParameter ---- */` |
|      - | 6404 | `/*` |
|      - | 6405 | ``  * The function a ReflectionParameter belongs to, from its own `__t`/`__m` `` |
|      - | 6406 | `` * slots. `__t` holds a Closure, a class name or a function name; `__m` the`` |
|      - | 6407 | ` * method name, or null.` |
|      - | 6408 | ` */` |
|   1244 | 6409 | `static int ReflectParamOwner(ph7_context *pCtx, ReflectFuncRef *pRef, ReflectParamDesc *pDesc)` |
|      3 | 6410 | `{` |
|   1247 | 6411 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 6412 | `	ph7_value *pT, *pM;` |
|      - | 6413 | `	int rc;` |
|   1247 | 6414 | `	SyZero(pRef, sizeof(*pRef));` |
|   1247 | 6415 | `	if( pDesc ){` |
|   1191 | 6416 | `		SyZero(pDesc, sizeof(*pDesc));` |
|    594 | 6417 | `	}` |
|   1247 | 6418 | `	if( pThis == 0 ){` |
|    ! 0 | 6419 | `		return 0;` |
|      - | 6420 | `	}` |
|   1247 | 6421 | `	pT = PH7_NativeAttr(pThis, RP_T);` |
|   1247 | 6422 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|   1247 | 6423 | `	if( pT == 0 ){` |
|    ! 0 | 6424 | `		return 0;` |
|      - | 6425 | `	}` |
|   1247 | 6426 | `	rc = ReflectFuncFill(pCtx->pVm, pT, (pM && (pM->iFlags & MEMOBJ_STRING)) ? pM : 0, pRef);` |
|   1247 | 6427 | `	if( rc == 0 \|\| pDesc == 0 ){` |
|     57 | 6428 | `		return rc;` |
|      - | 6429 | `	}` |
|   1191 | 6430 | `	return ReflectParamAt(pRef, (int)PH7_NativeAttrInt(pThis, RP_P), pDesc);` |
|    625 | 6431 | `}` |
|      - | 6432 | `/* ReflectionParameter::__construct($function, string\|int $param) */` |
|    732 | 6433 | `static int vm_builtin_ReflectionParameter_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      4 | 6434 | `{` |
|    736 | 6435 | `	ph7_vm *pVm = pCtx->pVm;` |
|    736 | 6436 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 6437 | `	ReflectFuncRef sRef;` |
|      - | 6438 | `	ReflectParamDesc sDesc;` |
|      - | 6439 | `	ph7_value sTarget, sMethod;` |
|    736 | 6440 | `	int nTotal, iFound = -1, rc = PH7_OK;` |
|    736 | 6441 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 | 6442 | `		return PH7_OK;` |
|      - | 6443 | `	}` |
|    736 | 6444 | `	SyZero(&sDesc, sizeof(sDesc));` |
|    736 | 6445 | `	PH7_MemObjInit(pVm, &sTarget);` |
|    736 | 6446 | `	PH7_MemObjInit(pVm, &sMethod);` |
|      - | 6447 | ``	/* `[$objOrClass, $method]`, `"C::m"` and a plain function/Closure are the`` |
|      - | 6448 | `	 * three spellings php accepts. */` |
|    736 | 6449 | `	if( ph7_value_is_array(apArg[0]) ){` |
|    185 | 6450 | `		ph7_value *pA = ph7_array_fetch(apArg[0], "0", 1);` |
|    185 | 6451 | `		ph7_value *pB = ph7_array_fetch(apArg[0], "1", 1);` |
|    185 | 6452 | `		if( pA && (pA->iFlags & MEMOBJ_OBJ) ){` |
|    ! 0 | 6453 | `			ph7_class_instance *pObj = (ph7_class_instance *)pA->x.pOther;` |
|    ! 0 | 6454 | `			ph7_value_string(&sTarget, SyStringData(&pObj->pClass->sName),` |
|    ! 0 | 6455 | `				(int)SyStringLength(&pObj->pClass->sName));` |
|    185 | 6456 | `		}else if( pA ){` |
|    185 | 6457 | `			PH7_MemObjStore(pA, &sTarget);` |
|     91 | 6458 | `		}` |
|    185 | 6459 | `		if( pB ){` |
|    185 | 6460 | `			PH7_MemObjStore(pB, &sMethod);` |
|     91 | 6461 | `		}` |
|     94 | 6462 | `	}else{` |
|      - | 6463 | ``		/* A STRING is a plain function name, `"C::m"` included: php does not`` |
|      - | 6464 | ``		 * split one here (it reports `Function C::m() does not exist`), unlike`` |
|      - | 6465 | `		 * ReflectionMethod's own one-argument form. The prelude split it and` |
|      - | 6466 | `		 * silently reflected the method. */` |
|    552 | 6467 | `		PH7_MemObjStore(apArg[0], &sTarget);` |
|      - | 6468 | `	}` |
|    736 | 6469 | `	if( !ReflectFuncFill(pCtx->pVm, &sTarget, (sMethod.iFlags & MEMOBJ_STRING) ? &sMethod : 0, &sRef) ){` |
|      - | 6470 | `		const char *zName;` |
|      - | 6471 | `		int nName;` |
|      3 | 6472 | `		if( sMethod.iFlags & MEMOBJ_STRING ){` |
|      - | 6473 | `			const char *zM;` |
|      - | 6474 | `			int nM;` |
|    ! 0 | 6475 | `			zName = ph7_value_to_string(&sTarget, &nName);` |
|    ! 0 | 6476 | `			zM = ph7_value_to_string(&sMethod, &nM);` |
|    ! 0 | 6477 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 | 6478 | `				"Method %.*s::%.*s() does not exist", nName, zName, nM, zM);` |
|    ! 0 | 6479 | `		}else{` |
|      3 | 6480 | `			zName = ph7_value_to_string(&sTarget, &nName);` |
|      4 | 6481 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 6482 | `				"Function %.*s() does not exist", nName, zName);` |
|      - | 6483 | `		}` |
|      3 | 6484 | `		goto Done;` |
|      - | 6485 | `	}` |
|    734 | 6486 | `	nTotal = ReflectParamCount(&sRef);` |
|    734 | 6487 | `	if( apArg[1]->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|    724 | 6488 | `		int iWant = (int)ph7_value_to_int(apArg[1]);` |
|    724 | 6489 | `		if( iWant >= 0 && iWant < nTotal && ReflectParamAt(&sRef, iWant, &sDesc) ){` |
|    722 | 6490 | `			iFound = iWant;` |
|    359 | 6491 | `		}` |
|    724 | 6492 | `		if( iFound < 0 ){` |
|      3 | 6493 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 6494 | `				"The parameter specified by its offset could not be found");` |
|      3 | 6495 | `			goto Done;` |
|      - | 6496 | `		}` |
|    363 | 6497 | `	}else{` |
|      - | 6498 | `		const char *zWant;` |
|      - | 6499 | `		int nWant, n;` |
|     11 | 6500 | `		zWant = ph7_value_to_string(apArg[1], &nWant);` |
|     33 | 6501 | `		for( n = 0 ; n < nTotal ; n++ ){` |
|     30 | 6502 | `			if( ReflectParamAt(&sRef, n, &sDesc)` |
|     30 | 6503 | `			 && SyStringLength(&sDesc.sName) == (sxu32)nWant` |
|     23 | 6504 | `			 && SyMemcmp(SyStringData(&sDesc.sName), zWant, (sxu32)nWant) == 0 ){` |
|      9 | 6505 | `				iFound = n;` |
|      9 | 6506 | `				break;` |
|      - | 6507 | `			}` |
|     12 | 6508 | `		}` |
|     11 | 6509 | `		if( iFound < 0 ){` |
|      3 | 6510 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 6511 | `				"The parameter specified by its name could not be found");` |
|      3 | 6512 | `			goto Done;` |
|      - | 6513 | `		}` |
|      - | 6514 | `	}` |
|   1093 | 6515 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sDesc.sName),` |
|    726 | 6516 | `		(int)SyStringLength(&sDesc.sName));` |
|      - | 6517 | `	/* Record the CANONICAL target: the class the method really came through and` |
|      - | 6518 | `	 * its declared spelling, so a re-resolve cannot land somewhere else. */` |
|    730 | 6519 | `	if( sRef.pMeth && sRef.pClass ){` |
|    285 | 6520 | `		PH7_NativeSetAttrStr(pVm, pThis, RP_T, SyStringData(&sRef.pClass->sName),` |
|    188 | 6521 | `			(int)SyStringLength(&sRef.pClass->sName));` |
|    285 | 6522 | `		PH7_NativeSetAttrStr(pVm, pThis, RP_M, SyStringData(&sRef.pMeth->sFunc.sName),` |
|    188 | 6523 | `			(int)SyStringLength(&sRef.pMeth->sFunc.sName));` |
|     97 | 6524 | `	}else{` |
|    540 | 6525 | `		ph7_value *pSlot = PH7_NativeAttr(pThis, RP_T);` |
|    540 | 6526 | `		if( pSlot ){` |
|    540 | 6527 | `			PH7_MemObjStore(&sTarget, pSlot);` |
|    269 | 6528 | `		}` |
|      - | 6529 | `	}` |
|    730 | 6530 | `	PH7_NativeSetAttrInt(pVm, pThis, RP_P, (sxi64)iFound);` |
|    366 | 6531 | `Done:` |
|    736 | 6532 | `	PH7_MemObjRelease(&sTarget);` |
|    736 | 6533 | `	PH7_MemObjRelease(&sMethod);` |
|    736 | 6534 | `	return rc;` |
|    370 | 6535 | `}` |
|    572 | 6536 | `static int vm_builtin_ReflectionParameter_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 6537 | `{` |
|    574 | 6538 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    574 | 6539 | `	const char *zName = "";` |
|    574 | 6540 | `	int nName = 0;` |
|    286 | 6541 | `	SXUNUSED(nArg);` |
|    286 | 6542 | `	SXUNUSED(apArg);` |
|    574 | 6543 | `	if( pThis ){` |
|    574 | 6544 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    286 | 6545 | `	}` |
|    574 | 6546 | `	ph7_result_string(pCtx, zName, nName);` |
|    574 | 6547 | `	return PH7_OK;` |
|      2 | 6548 | `}` |
|     36 | 6549 | `static int vm_builtin_ReflectionParameter_getPosition(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6550 | `{` |
|     37 | 6551 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     18 | 6552 | `	SXUNUSED(nArg);` |
|     18 | 6553 | `	SXUNUSED(apArg);` |
|     37 | 6554 | `	ph7_result_int64(pCtx, pThis ? PH7_NativeAttrInt(pThis, RP_P) : 0);` |
|     37 | 6555 | `	return PH7_OK;` |
|      1 | 6556 | `}` |
|      - | 6557 | `/* The boolean predicates that read one flag of the description. */` |
|    460 | 6558 | `static int ReflectParamFlag(ph7_context *pCtx, int iWhat)` |
|      1 | 6559 | `{` |
|      - | 6560 | `	ReflectFuncRef sRef;` |
|      - | 6561 | `	ReflectParamDesc sDesc;` |
|    461 | 6562 | `	int bYes = 0;` |
|    461 | 6563 | `	if( ReflectParamOwner(pCtx, &sRef, &sDesc) ){` |
|    461 | 6564 | `		switch( iWhat ){` |
|     25 | 6565 | `		case 0: bYes = sDesc.bByRef; break;` |
|      3 | 6566 | `		case 1: bYes = !sDesc.bByRef; break;` |
|     45 | 6567 | `		case 2: bYes = sDesc.bVariadic; break;` |
|    ! 0 | 6568 | `		case 3: bYes = sDesc.bPromoted; break;` |
|    337 | 6569 | `		case 4: bYes = sDesc.bHasDef; break;` |
|     45 | 6570 | `		case 5: bYes = SyStringLength(&sDesc.sType) > 0; break;` |
|      5 | 6571 | `		default:` |
|      - | 6572 | `			/* allowsNull(): untyped, nullable, or a type that INCLUDES null */` |
|     13 | 6573 | `			bYes = SyStringLength(&sDesc.sType) < 1 \|\| sDesc.bNullable` |
|      7 | 6574 | `				\|\| ReflectTypeNameIs(SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType), "mixed")` |
|     14 | 6575 | `				\|\| ReflectTypeNameIs(SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType), "null");` |
|     10 | 6576 | `			break;` |
|      - | 6577 | `		}` |
|    231 | 6578 | `	}else if( iWhat == 1 \|\| iWhat == 6 ){` |
|    ! 0 | 6579 | `		bYes = 1;` |
|    ! 0 | 6580 | `	}` |
|    461 | 6581 | `	ph7_result_bool(pCtx, bYes);` |
|    461 | 6582 | `	return PH7_OK;` |
|      1 | 6583 | `}` |
|      - | 6584 | `#define REFLECT_PARAM_FLAG(NAME,WHAT) \` |
|      - | 6585 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 6586 | `	{ \` |
|      - | 6587 | `		SXUNUSED(nArg); \` |
|      - | 6588 | `		SXUNUSED(apArg); \` |
|      - | 6589 | `		return ReflectParamFlag(pCtx, WHAT); \` |
|      - | 6590 | `	}` |
|     25 | 6591 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isPassedByReference, 0)` |
|      3 | 6592 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_canBePassedByValue, 1)` |
|     45 | 6593 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isVariadic, 2)` |
|    ! 0 | 6594 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isPromoted, 3)` |
|    337 | 6595 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isDefaultValueAvailable, 4)` |
|     45 | 6596 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_hasType, 5)` |
|     11 | 6597 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_allowsNull, 6)` |
|      - | 6598 |  |
|      - | 6599 | `/* isOptional(): every parameter from here on has to be omissible too. */` |
|     52 | 6600 | `static int vm_builtin_ReflectionParameter_isOptional(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6601 | `{` |
|     53 | 6602 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 6603 | `	ReflectFuncRef sRef;` |
|      - | 6604 | `	ReflectParamDesc sDesc;` |
|      - | 6605 | `	int n, nTotal, iPos;` |
|     26 | 6606 | `	SXUNUSED(nArg);` |
|     26 | 6607 | `	SXUNUSED(apArg);` |
|     53 | 6608 | `	if( pThis == 0 \|\| !ReflectParamOwner(pCtx, &sRef, 0) ){` |
|    ! 0 | 6609 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 6610 | `		return PH7_OK;` |
|      - | 6611 | `	}` |
|     53 | 6612 | `	iPos = (int)PH7_NativeAttrInt(pThis, RP_P);` |
|     53 | 6613 | `	nTotal = ReflectParamCount(&sRef);` |
|    101 | 6614 | `	for( n = iPos ; n < nTotal ; n++ ){` |
|     71 | 6615 | `		if( ReflectParamAt(&sRef, n, &sDesc) && !sDesc.bVariadic && !sDesc.bOptional ){` |
|     23 | 6616 | `			ph7_result_bool(pCtx, 0);` |
|     23 | 6617 | `			return PH7_OK;` |
|      - | 6618 | `		}` |
|     25 | 6619 | `	}` |
|     31 | 6620 | `	ph7_result_bool(pCtx, 1);` |
|     31 | 6621 | `	return PH7_OK;` |
|     27 | 6622 | `}` |
|    558 | 6623 | `static int vm_builtin_ReflectionParameter_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 6624 | `{` |
|      - | 6625 | `	ReflectFuncRef sRef;` |
|      - | 6626 | `	ReflectParamDesc sDesc;` |
|    279 | 6627 | `	SXUNUSED(nArg);` |
|    279 | 6628 | `	SXUNUSED(apArg);` |
|    561 | 6629 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| SyStringLength(&sDesc.sType) < 1 ){` |
|      3 | 6630 | `		ph7_result_null(pCtx);` |
|      3 | 6631 | `		return PH7_OK;` |
|      - | 6632 | `	}` |
|    837 | 6633 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx,` |
|    556 | 6634 | `		SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType)));` |
|    282 | 6635 | `}` |
|      - | 6636 | `/*` |
|      - | 6637 | ` * getClass()/isArray()/isCallable() — php 8.0 deprecated these in favour of` |
|      - | 6638 | ` * getType() but still declares them, still answers, and emits an E_DEPRECATED` |
|      - | 6639 | ` * naming the replacement. PHL fatalled on the call; all three are here now,` |
|      - | 6640 | ` * notice included.` |
|      - | 6641 | ` */` |
|     24 | 6642 | `static void ReflectParamDeprecated(ph7_context *pCtx, const char *zWho)` |
|      1 | 6643 | `{` |
|      - | 6644 | `	char zMsg[160];` |
|     37 | 6645 | `	SyBufferFormat(zMsg, sizeof(zMsg),` |
|      - | 6646 | `		"Method ReflectionParameter::%s() is deprecated since 8.0, "` |
|     12 | 6647 | `		"use ReflectionParameter::getType() instead", zWho);` |
|     25 | 6648 | `	PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED, zMsg);` |
|     25 | 6649 | `}` |
|      8 | 6650 | `static int vm_builtin_ReflectionParameter_getClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6651 | `{` |
|      - | 6652 | `	ReflectFuncRef sRef;` |
|      - | 6653 | `	ReflectParamDesc sDesc;` |
|      - | 6654 | `	const char *zType;` |
|      - | 6655 | `	int nType;` |
|      4 | 6656 | `	SXUNUSED(nArg);` |
|      4 | 6657 | `	SXUNUSED(apArg);` |
|      9 | 6658 | `	ReflectParamDeprecated(pCtx, "getClass");` |
|      9 | 6659 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| SyStringLength(&sDesc.sType) < 1 ){` |
|    ! 0 | 6660 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6661 | `		return PH7_OK;` |
|      - | 6662 | `	}` |
|      9 | 6663 | `	zType = SyStringData(&sDesc.sType);` |
|      9 | 6664 | `	nType = (int)SyStringLength(&sDesc.sType);` |
|      9 | 6665 | `	if( nType > 0 && zType[0] == '?' ){` |
|      3 | 6666 | `		zType++;` |
|      3 | 6667 | `		nType--;` |
|      1 | 6668 | `	}` |
|      9 | 6669 | `	if( ReflectTypeIsBuiltin(zType, nType) ){` |
|      7 | 6670 | `		ph7_result_null(pCtx);` |
|      7 | 6671 | `		return PH7_OK;` |
|      - | 6672 | `	}` |
|      3 | 6673 | `	return ReflectResultClassOf(pCtx, PH7_VmExtractClass(pCtx->pVm, zType, (sxu32)nType, FALSE, 0));` |
|      5 | 6674 | `}` |
|     16 | 6675 | `static int ReflectParamTypeIs(ph7_context *pCtx, const char *zWant)` |
|      1 | 6676 | `{` |
|      - | 6677 | `	ReflectFuncRef sRef;` |
|      - | 6678 | `	ReflectParamDesc sDesc;` |
|     17 | 6679 | `	int bYes = 0;` |
|     17 | 6680 | `	if( ReflectParamOwner(pCtx, &sRef, &sDesc) && SyStringLength(&sDesc.sType) > 0 ){` |
|     17 | 6681 | `		const char *zType = SyStringData(&sDesc.sType);` |
|     17 | 6682 | `		int nType = (int)SyStringLength(&sDesc.sType);` |
|      - | 6683 | ``		/* php answers true for `?array` too: the deprecated pair asks about the`` |
|      - | 6684 | `		 * type ATOM, and nullability is a separate question. */` |
|     17 | 6685 | `		if( zType[0] == '?' ){` |
|      5 | 6686 | `			zType++;` |
|      5 | 6687 | `			nType--;` |
|      2 | 6688 | `		}` |
|     17 | 6689 | `		bYes = ReflectTypeNameIs(zType, nType, zWant);` |
|      8 | 6690 | `	}` |
|     17 | 6691 | `	ph7_result_bool(pCtx, bYes);` |
|     17 | 6692 | `	return PH7_OK;` |
|      1 | 6693 | `}` |
|      8 | 6694 | `static int vm_builtin_ReflectionParameter_isArray(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6695 | `{` |
|      4 | 6696 | `	SXUNUSED(nArg);` |
|      4 | 6697 | `	SXUNUSED(apArg);` |
|      9 | 6698 | `	ReflectParamDeprecated(pCtx, "isArray");` |
|      9 | 6699 | `	return ReflectParamTypeIs(pCtx, "array");` |
|      1 | 6700 | `}` |
|      8 | 6701 | `static int vm_builtin_ReflectionParameter_isCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6702 | `{` |
|      4 | 6703 | `	SXUNUSED(nArg);` |
|      4 | 6704 | `	SXUNUSED(apArg);` |
|      9 | 6705 | `	ReflectParamDeprecated(pCtx, "isCallable");` |
|      9 | 6706 | `	return ReflectParamTypeIs(pCtx, "callable");` |
|      1 | 6707 | `}` |
|    110 | 6708 | `static int vm_builtin_ReflectionParameter_getDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6709 | `{` |
|      - | 6710 | `	ReflectFuncRef sRef;` |
|      - | 6711 | `	ReflectParamDesc sDesc;` |
|     55 | 6712 | `	SXUNUSED(nArg);` |
|     55 | 6713 | `	SXUNUSED(apArg);` |
|    111 | 6714 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|      5 | 6715 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 6716 | `			"Internal error: Failed to retrieve the default value");` |
|      - | 6717 | `	}` |
|    107 | 6718 | `	if( sDesc.pArg ){` |
|      - | 6719 | `		/* Compiled: the same evaluation path the VM uses for an omitted argument */` |
|      - | 6720 | `		ph7_value sValue;` |
|      9 | 6721 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|      9 | 6722 | `		VmLocalExec(pCtx->pVm, &sDesc.pArg->aByteCode, &sValue, FALSE);` |
|      9 | 6723 | `		ph7_result_value(pCtx, &sValue);` |
|      9 | 6724 | `		PH7_MemObjRelease(&sValue);` |
|      9 | 6725 | `		return PH7_OK;` |
|      - | 6726 | `	}` |
|      - | 6727 | `	{` |
|      - | 6728 | `		/* Declared: the signature's default TEXT, reduced. */` |
|     99 | 6729 | `		const char *zDef = SyStringData(&sDesc.sDefText);` |
|     99 | 6730 | `		int nDef = (int)SyStringLength(&sDesc.sDefText);` |
|     99 | 6731 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|     99 | 6732 | `		if( pVal && ReflectSigScalar(pCtx, zDef, nDef, pVal) ){` |
|     97 | 6733 | `			ph7_result_value(pCtx, pVal);` |
|     97 | 6734 | `			return PH7_OK;` |
|      - | 6735 | `		}` |
|      2 | 6736 | `		if( (nDef >= 1 && zDef[0] == '[')` |
|      2 | 6737 | `		 \|\| (nDef >= (int)sizeof("array (")-1` |
|    ! 0 | 6738 | `		  && SyMemcmp(zDef, "array (", sizeof("array (")-1) == 0) ){` |
|      3 | 6739 | `			ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|      3 | 6740 | `			return PH7_OK;` |
|      - | 6741 | `		}` |
|      - | 6742 | `	}` |
|    ! 0 | 6743 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 6744 | `		"Internal error: Failed to retrieve the default value");` |
|     56 | 6745 | `}` |
|      - | 6746 | `/*` |
|      - | 6747 | ` * A default that is a plain global-constant reference compiles to exactly` |
|      - | 6748 | ` * [ OP_LOADC (EXPAND), OP_DONE ] with the name in the literal table; a` |
|      - | 6749 | ` * DECLARED default is text and never a constant.` |
|      - | 6750 | ` */` |
|     80 | 6751 | `static int ReflectParamDefConst(ph7_context *pCtx, ReflectParamDesc *pDesc,` |
|      - | 6752 | `	const char **pz, int *pn)` |
|      1 | 6753 | `{` |
|      - | 6754 | `	VmInstr *aInstr;` |
|      - | 6755 | `	ph7_value *pLit;` |
|     81 | 6756 | `	if( pDesc->pArg == 0 \|\| SySetUsed(&pDesc->pArg->aByteCode) != 2 ){` |
|     25 | 6757 | `		return 0;` |
|      - | 6758 | `	}` |
|     57 | 6759 | `	aInstr = (VmInstr *)SySetBasePtr(&pDesc->pArg->aByteCode);` |
|     56 | 6760 | `	if( aInstr[0].iOp != PH7_OP_LOADC \|\| (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0` |
|     36 | 6761 | `	 \|\| aInstr[1].iOp != PH7_OP_DONE ){` |
|     43 | 6762 | `		return 0;` |
|      - | 6763 | `	}` |
|     15 | 6764 | `	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);` |
|     15 | 6765 | `	if( pLit == 0 \|\| SyBlobLength(&pLit->sBlob) < 1 ){` |
|    ! 0 | 6766 | `		return 0;` |
|      - | 6767 | `	}` |
|     15 | 6768 | `	*pz = (const char *)SyBlobData(&pLit->sBlob);` |
|     15 | 6769 | `	*pn = (int)SyBlobLength(&pLit->sBlob);` |
|     15 | 6770 | `	return 1;` |
|     41 | 6771 | `}` |
|     18 | 6772 | `static int vm_builtin_ReflectionParameter_isDefaultValueConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6773 | `{` |
|      - | 6774 | `	ReflectFuncRef sRef;` |
|      - | 6775 | `	ReflectParamDesc sDesc;` |
|      - | 6776 | `	const char *z;` |
|      - | 6777 | `	int n;` |
|      9 | 6778 | `	SXUNUSED(nArg);` |
|      9 | 6779 | `	SXUNUSED(apArg);` |
|     19 | 6780 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|      - | 6781 | `		/* php raises here rather than answering false: asking whether a default` |
|      - | 6782 | `		 * is a constant presupposes there IS one. The prelude answered false. */` |
|      3 | 6783 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 6784 | `			"Internal error: Failed to retrieve the default value");` |
|      - | 6785 | `	}` |
|     17 | 6786 | `	ph7_result_bool(pCtx, ReflectParamDefConst(pCtx, &sDesc, &z, &n) != 0);` |
|     17 | 6787 | `	return PH7_OK;` |
|     10 | 6788 | `}` |
|      4 | 6789 | `static int vm_builtin_ReflectionParameter_getDefaultValueConstantName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6790 | `{` |
|      - | 6791 | `	ReflectFuncRef sRef;` |
|      - | 6792 | `	ReflectParamDesc sDesc;` |
|      - | 6793 | `	const char *z;` |
|      - | 6794 | `	int n;` |
|      2 | 6795 | `	SXUNUSED(nArg);` |
|      2 | 6796 | `	SXUNUSED(apArg);` |
|      5 | 6797 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|    ! 0 | 6798 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      - | 6799 | `			"Internal error: Failed to retrieve the default value");` |
|      - | 6800 | `	}` |
|      5 | 6801 | `	if( ReflectParamDefConst(pCtx, &sDesc, &z, &n) ){` |
|      5 | 6802 | `		ph7_result_string(pCtx, z, n);` |
|      3 | 6803 | `	}else{` |
|    ! 0 | 6804 | `		ph7_result_null(pCtx);` |
|      - | 6805 | `	}` |
|      5 | 6806 | `	return PH7_OK;` |
|      3 | 6807 | `}` |
|      4 | 6808 | `static int vm_builtin_ReflectionParameter_getDeclaringFunction(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6809 | `{` |
|      5 | 6810 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 6811 | `	ph7_value *pT, *pM;` |
|      - | 6812 | `	ph7_value *apCtor[2];` |
|      - | 6813 | `	ph7_class_instance *pOut;` |
|      - | 6814 | `	sxi32 rc;` |
|      2 | 6815 | `	SXUNUSED(nArg);` |
|      2 | 6816 | `	SXUNUSED(apArg);` |
|      5 | 6817 | `	if( pThis == 0 \|\| (pT = PH7_NativeAttr(pThis, RP_T)) == 0 ){` |
|    ! 0 | 6818 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6819 | `		return PH7_OK;` |
|      - | 6820 | `	}` |
|      5 | 6821 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|      5 | 6822 | `	apCtor[0] = pT;` |
|      5 | 6823 | `	apCtor[1] = pM;` |
|      5 | 6824 | `	if( pM && (pM->iFlags & MEMOBJ_STRING) ){` |
|      3 | 6825 | `		pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|      2 | 6826 | `	}else{` |
|      3 | 6827 | `		pOut = ReflectConstruct(pCtx, "ReflectionFunction", 1, apCtor, &rc);` |
|      - | 6828 | `	}` |
|      5 | 6829 | `	if( pOut == 0 ){` |
|    ! 0 | 6830 | `		if( rc != PH7_OK ){` |
|    ! 0 | 6831 | `			return rc;` |
|      - | 6832 | `		}` |
|    ! 0 | 6833 | `		ph7_result_null(pCtx);` |
|    ! 0 | 6834 | `		return PH7_OK;` |
|      - | 6835 | `	}` |
|      5 | 6836 | `	return ReflectResultObject(pCtx, pOut);` |
|      3 | 6837 | `}` |
|      4 | 6838 | `static int vm_builtin_ReflectionParameter_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6839 | `{` |
|      - | 6840 | `	ReflectFuncRef sRef;` |
|      2 | 6841 | `	SXUNUSED(nArg);` |
|      2 | 6842 | `	SXUNUSED(apArg);` |
|      5 | 6843 | `	if( !ReflectParamOwner(pCtx, &sRef, 0) \|\| sRef.pMeth == 0 ){` |
|      3 | 6844 | `		ph7_result_null(pCtx);` |
|      3 | 6845 | `		return PH7_OK;` |
|      - | 6846 | `	}` |
|      3 | 6847 | `	return ReflectResultClassOf(pCtx, ReflectFuncDeclClass(&sRef));` |
|      3 | 6848 | `}` |
|      4 | 6849 | `static int vm_builtin_ReflectionParameter_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6850 | `{` |
|      5 | 6851 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 6852 | `	ReflectFuncRef sRef;` |
|      - | 6853 | `	ReflectParamDesc sDesc;` |
|      - | 6854 | `	ph7_value *pT, *pM;` |
|      5 | 6855 | `	const char *zMember = 0;` |
|      5 | 6856 | `	int nMember = 0;` |
|      5 | 6857 | `	if( pThis == 0 \|\| !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| sDesc.pArg == 0 ){` |
|    ! 0 | 6858 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 | 6859 | `		return PH7_OK;` |
|      - | 6860 | `	}` |
|      5 | 6861 | `	pT = PH7_NativeAttr(pThis, RP_T);` |
|      5 | 6862 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|      5 | 6863 | `	if( pM && (pM->iFlags & MEMOBJ_STRING) ){` |
|      5 | 6864 | `		zMember = (const char *)SyBlobData(&pM->sBlob);` |
|      5 | 6865 | `		nMember = (int)SyBlobLength(&pM->sBlob);` |
|      2 | 6866 | `	}` |
|      - | 6867 | `	/* 32 = Attribute::TARGET_PARAMETER */` |
|      7 | 6868 | `	return ReflectBuildAttrs(pCtx, &sDesc.pArg->aAttrs, "param", pT, zMember, nMember,` |
|      4 | 6869 | `		(int)PH7_NativeAttrInt(pThis, RP_P), 32, nArg, apArg);` |
|      3 | 6870 | `}` |
|     10 | 6871 | `static int vm_builtin_ReflectionParameter_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 6872 | `{` |
|      5 | 6873 | `	SXUNUSED(nArg);` |
|      5 | 6874 | `	SXUNUSED(apArg);` |
|     11 | 6875 | `	return ReflectExportParamSelf(pCtx);` |
|      1 | 6876 | `}` |
|      - | 6877 | `/*` |
|      - | 6878 | ` * Declare chunk 2. Called from PH7_VmInstallReflection() where it used to be` |
|      - | 6879 | `` * compiled — after chunk 1, whose `Reflector` these implement and whose`` |
|      - | 6880 | `` * `ReflectionClass` they answer.`` |
|      - | 6881 | ` *` |
|      - | 6882 | ` * The method tables are in php's own DECLARATION order.` |
|      - | 6883 | ` */` |
|      - | 6884 | `/*` |
|      - | 6885 | `` * PropertyHookType — php's `enum PropertyHookType: string { Get; Set; }`, the`` |
|      - | 6886 | ` * argument ReflectionProperty::getHook()/hasHook() take.` |
|      - | 6887 | ` */` |
|   5254 | 6888 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionHookType(ph7_vm *pVm)` |
|      5 | 6889 | `{` |
|      - | 6890 | `	static const PH7_NativeEnumCase aCase[] = {` |
|      - | 6891 | `		{ "Get", { 0, 0, PH7_NATIVE_VAL_STRING, 0, "get", 0.0 } },` |
|      - | 6892 | `		{ "Set", { 0, 0, PH7_NATIVE_VAL_STRING, 0, "set", 0.0 } },` |
|      - | 6893 | `	};` |
|   5259 | 6894 | `	return PH7_InstallNativeEnum(&(*pVm), "PropertyHookType", MEMOBJ_STRING,` |
|      - | 6895 | `		aCase, SX_ARRAYSIZE(aCase), 0, 0);` |
|      5 | 6896 | `}` |
|   5254 | 6897 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionFunc(ph7_vm *pVm)` |
|      5 | 6898 | `{` |
|      - | 6899 | `	static const PH7_NativePropDef aAbstractProp[] = {` |
|      - | 6900 | `		{ "name",  PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 6901 | `		/* PHL-only: the Closure being reflected. php reaches the same state from` |
|      - | 6902 | `		 * the function record itself; PHL has no hidden-slot bit yet (§7.4 (e)). */` |
|      - | 6903 | `		{ RF_CL,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 6904 | `	};` |
|      - | 6905 | `	static const PH7_NativeMethodDef aAbstractMethod[] = {` |
|      - | 6906 | `		{ "__clone",       PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - | 6907 | `		{ "inNamespace",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_inNamespace },` |
|      - | 6908 | `		{ "isClosure",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isClosure },` |
|      - | 6909 | `		{ "isDeprecated",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isDeprecated },` |
|      - | 6910 | `		{ "isInternal",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isInternal },` |
|      - | 6911 | `		{ "isUserDefined", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isUserDefined },` |
|      - | 6912 | `		{ "isGenerator",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isGenerator },` |
|      - | 6913 | `		{ "isVariadic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isVariadic },` |
|      - | 6914 | `		{ "isStatic",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isStatic },` |
|      - | 6915 | `		{ "getClosureThis", PH7_MOD_PUBLIC, "", "@?object", vm_builtin_ReflectionFunc_getClosureThis },` |
|      - | 6916 | `		{ "getClosureScopeClass", PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|      - | 6917 | `		  vm_builtin_ReflectionFunc_getClosureScopeClass },` |
|      - | 6918 | `		/* php answers the same class for both; PHL has no separate called-scope. */` |
|      - | 6919 | `		{ "getClosureCalledClass", PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|      - | 6920 | `		  vm_builtin_ReflectionFunc_getClosureCalledClass },` |
|      - | 6921 | `		{ "getClosureUsedVariables", PH7_MOD_PUBLIC, "", "array",` |
|      - | 6922 | `		  vm_builtin_ReflectionFunc_getClosureUsedVariables },` |
|      - | 6923 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getDocComment },` |
|      - | 6924 | `		{ "getEndLine",    PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionFunc_getEndLine },` |
|      - | 6925 | `		{ "getExtension",  PH7_MOD_PUBLIC, "", "@?ReflectionExtension", vm_builtin_ReflectionFunc_getExtension },` |
|      - | 6926 | `		{ "getExtensionName", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getExtensionName },` |
|      - | 6927 | `		{ "getFileName",   PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getFileName },` |
|      - | 6928 | `		{ "getName",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getName },` |
|      - | 6929 | `		{ "getNamespaceName", PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getNamespaceName },` |
|      - | 6930 | `		{ "getNumberOfParameters", PH7_MOD_PUBLIC, "", "@int",` |
|      - | 6931 | `		  vm_builtin_ReflectionFunc_getNumberOfParameters },` |
|      - | 6932 | `		{ "getNumberOfRequiredParameters", PH7_MOD_PUBLIC, "", "@int",` |
|      - | 6933 | `		  vm_builtin_ReflectionFunc_getNumberOfRequiredParameters },` |
|      - | 6934 | `		{ "getParameters", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionFunc_getParameters },` |
|      - | 6935 | `		{ "getShortName",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getShortName },` |
|      - | 6936 | `		{ "getStartLine",  PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionFunc_getStartLine },` |
|      - | 6937 | `		{ "getStaticVariables", PH7_MOD_PUBLIC, "", "@array",` |
|      - | 6938 | `		  vm_builtin_ReflectionFunc_getStaticVariables },` |
|      - | 6939 | `		{ "returnsReference", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_returnsReference },` |
|      - | 6940 | `		{ "hasReturnType", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_hasReturnType },` |
|      - | 6941 | `		{ "getReturnType", PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionFunc_getReturnType },` |
|      - | 6942 | `		{ "hasTentativeReturnType", PH7_MOD_PUBLIC, "", "bool",` |
|      - | 6943 | `		  vm_builtin_ReflectionFunc_hasTentativeReturnType },` |
|      - | 6944 | `		{ "getTentativeReturnType", PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|      - | 6945 | `		  vm_builtin_ReflectionFunc_getTentativeReturnType },` |
|      - | 6946 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - | 6947 | `		  vm_builtin_ReflectionFunc_getAttributes },` |
|      - | 6948 | `	};` |
|      - | 6949 | `	static const PH7_NativeConstDef aFunctionConst[] = {` |
|      - | 6950 | `		{ "IS_DEPRECATED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2048, 0, 0.0 },` |
|      - | 6951 | `	};` |
|      - | 6952 | `	static const PH7_NativeMethodDef aFunctionMethod[] = {` |
|      - | 6953 | `		{ "__construct", PH7_MOD_PUBLIC, "Closure\|string $function", "",` |
|      - | 6954 | `		  vm_builtin_ReflectionFunction_construct },` |
|      - | 6955 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionFunc_toString },` |
|      - | 6956 | `		{ "isAnonymous", PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionFunction_isAnonymous },` |
|      - | 6957 | `		{ "isDisabled",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunction_isDisabled },` |
|      - | 6958 | `		{ "invoke",      PH7_MOD_PUBLIC, "mixed ...$args", "@mixed",` |
|      - | 6959 | `		  vm_builtin_ReflectionFunction_invoke },` |
|      - | 6960 | `		{ "invokeArgs",  PH7_MOD_PUBLIC, "array $args", "@mixed",` |
|      - | 6961 | `		  vm_builtin_ReflectionFunction_invokeArgs },` |
|      - | 6962 | `		{ "getClosure",  PH7_MOD_PUBLIC, "", "@Closure", vm_builtin_ReflectionFunction_getClosure },` |
|      - | 6963 | `	};` |
|      - | 6964 | `	static const PH7_NativePropDef aMethodProp[] = {` |
|      - | 6965 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 6966 | `	};` |
|      - | 6967 | `	static const PH7_NativeConstDef aMethodConst[] = {` |
|      - | 6968 | `		{ "IS_STATIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16, 0, 0.0 },` |
|      - | 6969 | `		{ "IS_PUBLIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,  0, 0.0 },` |
|      - | 6970 | `		{ "IS_PROTECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,  0, 0.0 },` |
|      - | 6971 | `		{ "IS_PRIVATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,  0, 0.0 },` |
|      - | 6972 | `		{ "IS_ABSTRACT",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64, 0, 0.0 },` |
|      - | 6973 | `		{ "IS_FINAL",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|      - | 6974 | `	};` |
|      - | 6975 | `	static const PH7_NativeMethodDef aMethodMethod[] = {` |
|      - | 6976 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $objectOrMethod, ?string $method = null", "",` |
|      - | 6977 | `		  vm_builtin_ReflectionMethod_construct },` |
|      - | 6978 | `		{ "createFromMethodName", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $method", "static",` |
|      - | 6979 | `		  vm_builtin_ReflectionMethod_createFromMethodName },` |
|      - | 6980 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionFunc_toString },` |
|      - | 6981 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isPublic },` |
|      - | 6982 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isPrivate },` |
|      - | 6983 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isProtected },` |
|      - | 6984 | `		{ "isAbstract",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isAbstract },` |
|      - | 6985 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isFinal },` |
|      - | 6986 | `		{ "isConstructor", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isConstructor },` |
|      - | 6987 | `		{ "isDestructor",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isDestructor },` |
|      - | 6988 | `		{ "getClosure",  PH7_MOD_PUBLIC, "?object $object = null", "@Closure",` |
|      - | 6989 | `		  vm_builtin_ReflectionMethod_getClosure },` |
|      - | 6990 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionMethod_getModifiers },` |
|      - | 6991 | `		{ "invoke",      PH7_MOD_PUBLIC, "?object $object, mixed ...$args", "@mixed",` |
|      - | 6992 | `		  vm_builtin_ReflectionMethod_invoke },` |
|      - | 6993 | `		{ "invokeArgs",  PH7_MOD_PUBLIC, "?object $object, array $args", "@mixed",` |
|      - | 6994 | `		  vm_builtin_ReflectionMethod_invokeArgs },` |
|      - | 6995 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|      - | 6996 | `		  vm_builtin_ReflectionMethod_getDeclaringClass },` |
|      - | 6997 | `		{ "getPrototype", PH7_MOD_PUBLIC, "", "@ReflectionMethod", vm_builtin_ReflectionMethod_getPrototype },` |
|      - | 6998 | `		{ "hasPrototype", PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionMethod_hasPrototype },` |
|      - | 6999 | `		{ "setAccessible", PH7_MOD_PUBLIC, "bool $accessible", "@void",` |
|      - | 7000 | `		  vm_builtin_ReflectionMethod_setAccessible },` |
|      - | 7001 | `	};` |
|      - | 7002 | `	static const PH7_NativePropDef aParamProp[] = {` |
|      - | 7003 | `		{ "name", PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 7004 | `		/* PHL-only, the three that identify the parameter (§7.4 (e)) */` |
|      - | 7005 | `		{ RP_T,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 7006 | `		{ RP_M,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 7007 | `		{ RP_P,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|      - | 7008 | `	};` |
|      - | 7009 | `	static const PH7_NativeMethodDef aParamMethod[] = {` |
|      - | 7010 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - | 7011 | `		/* php leaves $function UNTYPED here: it takes a name, a Closure, a` |
|      - | 7012 | ``		 * `[$obj, 'm']` pair or "C::m", and no declarable union covers all four. */`` |
|      - | 7013 | `		{ "__construct", PH7_MOD_PUBLIC, "$function, string\|int $param", "",` |
|      - | 7014 | `		  vm_builtin_ReflectionParameter_construct },` |
|      - | 7015 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionParameter_toString },` |
|      - | 7016 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionParameter_getName },` |
|      - | 7017 | `		{ "isPassedByReference", PH7_MOD_PUBLIC, "", "@bool",` |
|      - | 7018 | `		  vm_builtin_ReflectionParameter_isPassedByReference },` |
|      - | 7019 | `		{ "canBePassedByValue",  PH7_MOD_PUBLIC, "", "@bool",` |
|      - | 7020 | `		  vm_builtin_ReflectionParameter_canBePassedByValue },` |
|      - | 7021 | `		{ "getDeclaringFunction", PH7_MOD_PUBLIC, "", "@ReflectionFunctionAbstract",` |
|      - | 7022 | `		  vm_builtin_ReflectionParameter_getDeclaringFunction },` |
|      - | 7023 | `		{ "getDeclaringClass",   PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|      - | 7024 | `		  vm_builtin_ReflectionParameter_getDeclaringClass },` |
|      - | 7025 | `		{ "getClass",    PH7_MOD_PUBLIC, "", "@?ReflectionClass", vm_builtin_ReflectionParameter_getClass },` |
|      - | 7026 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_hasType },` |
|      - | 7027 | `		{ "getType",     PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionParameter_getType },` |
|      - | 7028 | `		{ "isArray",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isArray },` |
|      - | 7029 | `		{ "isCallable",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isCallable },` |
|      - | 7030 | `		{ "allowsNull",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_allowsNull },` |
|      - | 7031 | `		{ "getPosition", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionParameter_getPosition },` |
|      - | 7032 | `		{ "isOptional",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isOptional },` |
|      - | 7033 | `		{ "isDefaultValueAvailable", PH7_MOD_PUBLIC, "", "@bool",` |
|      - | 7034 | `		  vm_builtin_ReflectionParameter_isDefaultValueAvailable },` |
|      - | 7035 | `		{ "getDefaultValue", PH7_MOD_PUBLIC, "", "@mixed",` |
|      - | 7036 | `		  vm_builtin_ReflectionParameter_getDefaultValue },` |
|      - | 7037 | `		{ "isDefaultValueConstant", PH7_MOD_PUBLIC, "", "@bool",` |
|      - | 7038 | `		  vm_builtin_ReflectionParameter_isDefaultValueConstant },` |
|      - | 7039 | `		{ "getDefaultValueConstantName", PH7_MOD_PUBLIC, "", "@?string",` |
|      - | 7040 | `		  vm_builtin_ReflectionParameter_getDefaultValueConstantName },` |
|      - | 7041 | `		{ "isVariadic",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isVariadic },` |
|      - | 7042 | `		{ "isPromoted",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionParameter_isPromoted },` |
|      - | 7043 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - | 7044 | `		  vm_builtin_ReflectionParameter_getAttributes },` |
|      - | 7045 | `	};` |
|      - | 7046 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 7047 | `		{ "ReflectionFunctionAbstract", 0, "Reflector", PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 7048 | `		  aAbstractMethod, SX_ARRAYSIZE(aAbstractMethod), 0, 0,` |
|      - | 7049 | `		  aAbstractProp, SX_ARRAYSIZE(aAbstractProp), 0, 0, 0 },` |
|      - | 7050 | `		{ "ReflectionFunction", "ReflectionFunctionAbstract", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 7051 | `		  aFunctionMethod, SX_ARRAYSIZE(aFunctionMethod),` |
|      - | 7052 | `		  aFunctionConst, SX_ARRAYSIZE(aFunctionConst), 0, 0, 0, 0, 0 },` |
|      - | 7053 | `		{ "ReflectionMethod", "ReflectionFunctionAbstract", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 7054 | `		  aMethodMethod, SX_ARRAYSIZE(aMethodMethod),` |
|      - | 7055 | `		  aMethodConst, SX_ARRAYSIZE(aMethodConst),` |
|      - | 7056 | `		  aMethodProp, SX_ARRAYSIZE(aMethodProp), 0, 0, 0 },` |
|      - | 7057 | `		{ "ReflectionParameter", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 7058 | `		  aParamMethod, SX_ARRAYSIZE(aParamMethod), 0, 0,` |
|      - | 7059 | `		  aParamProp, SX_ARRAYSIZE(aParamProp), 0, 0, 0 },` |
|      - | 7060 | `	};` |
|   5259 | 7061 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 | 7062 | `}` |
|      - | 7063 | `/*` |
|      - | 7064 | ` * ---------------------------------------------------------------------------` |
|      - | 7065 | ` * ReflectionProperty and ReflectionClassConstant.` |
|      - | 7066 | ` *` |
|      - | 7067 | ` * Chunk 3, the last of the three that made up "the core". Seven thunks retire` |
|      - | 7068 | ` * with it — __reflect_prop_read, __reflect_prop_write, __reflect_prop_state,` |
|      - | 7069 | ` * __reflect_prop_default, __reflect_static_value, __reflect_static_set and` |
|      - | 7070 | ` * __reflect_const_value — because a native method reaches an instance's slot` |
|      - | 7071 | ` * table, a static's shared slot and a constant's lazy slot directly.` |
|      - | 7072 | ` * ---------------------------------------------------------------------------` |
|      - | 7073 | ` */` |
|      - | 7074 | `#define RP_DYNOBJ "__dynobj"   /* the instance a DYNAMIC property lives on */` |
|      - | 7075 |  |
|      - | 7076 | `/* skipLazyInitialization() is a no-op on an object that is not lazy, which is` |
|      - | 7077 | ` * every object PHL can build — so it answers what php answers rather than` |
|      - | 7078 | ` * refusing (§7.4). */` |
|    ! 0 | 7079 | `static int vm_builtin_ReflectionProperty_noop(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 7080 | `{` |
|    ! 0 | 7081 | `	SXUNUSED(pCtx);` |
|    ! 0 | 7082 | `	SXUNUSED(nArg);` |
|    ! 0 | 7083 | `	SXUNUSED(apArg);` |
|    ! 0 | 7084 | `	return PH7_OK;` |
|    ! 0 | 7085 | `}` |
|      - | 7086 | `/*` |
|      - | 7087 | `` * A STATIC property's shared slot, read and written the way `C::$s` is: the`` |
|      - | 7088 | ` * class's static table materializes first (a default that threw at the` |
|      - | 7089 | ` * declaration raises HERE), and an uninitialized typed static is php's Error.` |
|      - | 7090 | ` */` |
|      6 | 7091 | `static int ReflectStaticSlotRead(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr)` |
|      2 | 7092 | `{` |
|      - | 7093 | `	SyHashEntry *pSlot;` |
|      - | 7094 | `	ph7_value *pVal;` |
|      8 | 7095 | `	sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|      8 | 7096 | `	if( rc != SXRET_OK ){` |
|      3 | 7097 | `		return rc;` |
|      - | 7098 | `	}` |
|      5 | 7099 | `	pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|      5 | 7100 | `	if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|    ! 0 | 7101 | `		ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|    ! 0 | 7102 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      - | 7103 | `			"Typed static property %z::$%z must not be accessed before initialization",` |
|    ! 0 | 7104 | `			&pDecl->sName, &pAttr->sName);` |
|      - | 7105 | `	}` |
|      5 | 7106 | `	pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      5 | 7107 | `	if( pVal ){` |
|      5 | 7108 | `		ph7_result_value(pCtx, pVal);` |
|      3 | 7109 | `	}else{` |
|    ! 0 | 7110 | `		ph7_result_null(pCtx);` |
|      - | 7111 | `	}` |
|      5 | 7112 | `	return PH7_OK;` |
|      5 | 7113 | `}` |
|      4 | 7114 | `static int ReflectStaticSlotWrite(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr,` |
|      - | 7115 | `	ph7_value *pValue)` |
|      2 | 7116 | `{` |
|      - | 7117 | `	ph7_value *pSlot;` |
|      6 | 7118 | `	sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|      6 | 7119 | `	if( rc != SXRET_OK ){` |
|      3 | 7120 | `		return rc;` |
|      - | 7121 | `	}` |
|      3 | 7122 | `	rc = ReflectEnforceStore(pCtx, pAttr->nIdx, pValue);` |
|      3 | 7123 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 7124 | `		return rc;` |
|      - | 7125 | `	}` |
|      3 | 7126 | `	pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      3 | 7127 | `	if( pSlot ){` |
|      3 | 7128 | `		PH7_MemObjStore(pValue, pSlot);` |
|      1 | 7129 | `	}` |
|      3 | 7130 | `	return PH7_OK;` |
|      4 | 7131 | `}` |
|      - | 7132 |  |
|      - | 7133 | `/* What a ReflectionProperty / ReflectionClassConstant is looking at. */` |
|      - | 7134 | `typedef struct ReflectMemberRef ReflectMemberRef;` |
|      - | 7135 | `struct ReflectMemberRef` |
|      - | 7136 | `{` |
|      - | 7137 | `	ph7_class *pClass;            /* the reflected class */` |
|      - | 7138 | `	ph7_class_attr *pAttr;        /* the declared member (NULL for a dynamic property) */` |
|      - | 7139 | `	ph7_class_instance *pDynObj;  /* the instance a dynamic property lives on */` |
|      - | 7140 | `	const char *zName;            /* the member's name (borrowed from the slot) */` |
|      - | 7141 | `	int nName;` |
|      - | 7142 | `};` |
|      - | 7143 | `/*` |
|      - | 7144 | `` * Resolve `$this->class` + `$this->name` into the member. Uses the LISTING`` |
|      - | 7145 | ` * answer of the member walk, which is php's: a base class's PRIVATE member is` |
|      - | 7146 | ``  * not reachable by name from the subclass (`new ReflectionProperty('B','pp')` `` |
|      - | 7147 | `` * raises where `B extends A` and `A::$pp` is private).`` |
|      - | 7148 | ` */` |
|    464 | 7149 | `static int ReflectMemberOfThis(ph7_context *pCtx, ReflectMemberRef *pOut, int iKind)` |
|      4 | 7150 | `{` |
|    468 | 7151 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 7152 | `	const char *zClass;` |
|      - | 7153 | `	int nClass;` |
|      - | 7154 | `	SySet aMembers;` |
|      - | 7155 | `	sxu32 n;` |
|    468 | 7156 | `	SyZero(pOut, sizeof(*pOut));` |
|    468 | 7157 | `	if( pThis == 0 ){` |
|    ! 0 | 7158 | `		return 0;` |
|      - | 7159 | `	}` |
|    468 | 7160 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|    468 | 7161 | `	PH7_NativeAttrStr(pThis, "name", &pOut->zName, &pOut->nName);` |
|    468 | 7162 | `	if( nClass < 1 ){` |
|    ! 0 | 7163 | `		return 0;` |
|      - | 7164 | `	}` |
|    468 | 7165 | `	pOut->pClass = PH7_VmExtractClass(pCtx->pVm, zClass, (sxu32)nClass, FALSE, 0);` |
|    468 | 7166 | `	if( pOut->pClass == 0 ){` |
|    ! 0 | 7167 | `		return 0;` |
|      - | 7168 | `	}` |
|    468 | 7169 | `	if( iKind == REFLECT_MEMBER_PROP ){` |
|    356 | 7170 | `		pOut->pDynObj = PH7_NativeAttrObj(pThis, RP_DYNOBJ);` |
|    176 | 7171 | `	}` |
|    468 | 7172 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|    468 | 7173 | `	ReflectMembers(pCtx->pVm, pOut->pClass, &aMembers, 0);` |
|   1752 | 7174 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   1746 | 7175 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   1746 | 7176 | `		if( pM->iKind == iKind && ReflectKeyIs(pM, pOut->zName, pOut->nName) ){` |
|    462 | 7177 | `			pOut->pAttr = pM->pAttr;` |
|    462 | 7178 | `			break;` |
|      - | 7179 | `		}` |
|    646 | 7180 | `	}` |
|    468 | 7181 | `	SySetRelease(&aMembers);` |
|    468 | 7182 | `	return pOut->pAttr != 0 \|\| pOut->pDynObj != 0;` |
|    236 | 7183 | `}` |
|      - | 7184 | `/* The class php reports as the member's declarer. */` |
|     32 | 7185 | `static ph7_class * ReflectMemberDecl(const ReflectMemberRef *pRef)` |
|      3 | 7186 | `{` |
|     35 | 7187 | `	if( pRef->pAttr && pRef->pAttr->pDeclClass ){` |
|     35 | 7188 | `		return pRef->pAttr->pDeclClass;` |
|      - | 7189 | `	}` |
|    ! 0 | 7190 | `	return pRef->pClass;` |
|     19 | 7191 | `}` |
|      - | 7192 | `/* The instance slot record for a dynamic (or any instance-owned) property. */` |
|     54 | 7193 | `static VmClassAttr * ReflectInstanceAttr(ph7_class_instance *pObj, const char *zName, int nName)` |
|      1 | 7194 | `{` |
|      - | 7195 | `	SyHashEntry *pEntry;` |
|     55 | 7196 | `	if( pObj == 0 \|\| nName < 1 ){` |
|      5 | 7197 | `		return 0;` |
|      - | 7198 | `	}` |
|     51 | 7199 | `	pEntry = SyHashGet(&pObj->hAttr, (const void *)zName, (sxu32)nName);` |
|     51 | 7200 | `	return pEntry ? (VmClassAttr *)pEntry->pUserData : 0;` |
|     28 | 7201 | `}` |
|      - | 7202 | `/* ---- ReflectionProperty ---- */` |
|      - | 7203 | `/* ReflectionProperty::__construct(object\|string $class, string $property) */` |
|    222 | 7204 | `static int vm_builtin_ReflectionProperty_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 | 7205 | `{` |
|    227 | 7206 | `	ph7_vm *pVm = pCtx->pVm;` |
|    227 | 7207 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    227 | 7208 | `	ph7_class_instance *pObj = 0;` |
|      - | 7209 | `	ph7_class *pClass;` |
|      - | 7210 | `	const char *zProp;` |
|      - | 7211 | `	int nProp;` |
|      - | 7212 | `	SySet aMembers;` |
|      - | 7213 | `	sxu32 n;` |
|    227 | 7214 | `	int bFound = 0;` |
|    227 | 7215 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 | 7216 | `		return PH7_OK;` |
|      - | 7217 | `	}` |
|    227 | 7218 | `	if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|      7 | 7219 | `		pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 7220 | `	}` |
|    227 | 7221 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|    227 | 7222 | `	if( pClass == 0 ){` |
|      - | 7223 | `		const char *zName;` |
|      - | 7224 | `		int nName;` |
|      3 | 7225 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|      4 | 7226 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 7227 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      - | 7228 | `	}` |
|    225 | 7229 | `	zProp = ph7_value_to_string(apArg[1], &nProp);` |
|    225 | 7230 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|    225 | 7231 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|    737 | 7232 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    727 | 7233 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|    727 | 7234 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zProp, nProp) ){` |
|      - | 7235 | `			/* php's $class is the DECLARING class, not the one asked about. */` |
|    215 | 7236 | `			pClass = pM->pDecl ? pM->pDecl : pClass;` |
|    215 | 7237 | `			bFound = 1;` |
|    215 | 7238 | `			break;` |
|      - | 7239 | `		}` |
|    260 | 7240 | `	}` |
|    225 | 7241 | `	SySetRelease(&aMembers);` |
|    335 | 7242 | `	PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pClass->sName),` |
|    220 | 7243 | `		(int)SyStringLength(&pClass->sName));` |
|    225 | 7244 | `	if( bFound ){` |
|    215 | 7245 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", zProp, nProp);` |
|    215 | 7246 | `		return PH7_OK;` |
|      - | 7247 | `	}` |
|      - | 7248 | `	/* Not declared: an OBJECT may still own it as a dynamic property. */` |
|     11 | 7249 | `	if( ReflectInstanceAttr(pObj, zProp, nProp) ){` |
|      7 | 7250 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", zProp, nProp);` |
|      7 | 7251 | `		PH7_NativeSetAttrObj(pVm, pThis, RP_DYNOBJ, pObj);` |
|      7 | 7252 | `		return PH7_OK;` |
|      - | 7253 | `	}` |
|      7 | 7254 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 | 7255 | `		"Property %z::$%.*s does not exist", &pClass->sName, nProp, zProp);` |
|    116 | 7256 | `}` |
|     82 | 7257 | `static int vm_builtin_ReflectionProperty_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 7258 | `{` |
|     84 | 7259 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     84 | 7260 | `	const char *zName = "";` |
|     84 | 7261 | `	int nName = 0;` |
|     41 | 7262 | `	SXUNUSED(nArg);` |
|     41 | 7263 | `	SXUNUSED(apArg);` |
|     84 | 7264 | `	if( pThis ){` |
|     84 | 7265 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     41 | 7266 | `	}` |
|     84 | 7267 | `	ph7_result_string(pCtx, zName, nName);` |
|     84 | 7268 | `	return PH7_OK;` |
|      2 | 7269 | `}` |
|      - | 7270 | `/*` |
|      - | 7271 | ` * getMangledName(): the name php stores the slot under —  plain for a public` |
|      - | 7272 | ` * property, "\0*\0name" for a protected one and "\0Class\0name" for a private` |
|      - | 7273 | ` * one. PHL's tables are not mangled, so the name is COMPOSED here; what php` |
|      - | 7274 | ` * exposes is the spelling, not the storage.` |
|      - | 7275 | ` */` |
|      6 | 7276 | `static int vm_builtin_ReflectionProperty_getMangledName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7277 | `{` |
|      - | 7278 | `	ReflectMemberRef sRef;` |
|      - | 7279 | `	SyBlob sOut;` |
|      3 | 7280 | `	SXUNUSED(nArg);` |
|      3 | 7281 | `	SXUNUSED(apArg);` |
|      7 | 7282 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 7283 | `		ph7_result_string(pCtx, sRef.zName, sRef.nName);` |
|    ! 0 | 7284 | `		return PH7_OK;` |
|      - | 7285 | `	}` |
|      7 | 7286 | `	if( sRef.pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|      3 | 7287 | `		ph7_result_string(pCtx, sRef.zName, sRef.nName);` |
|      3 | 7288 | `		return PH7_OK;` |
|      - | 7289 | `	}` |
|      5 | 7290 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|      5 | 7291 | `	SyBlobAppend(&sOut, "\0", 1);` |
|      5 | 7292 | `	if( sRef.pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|      3 | 7293 | `		SyBlobAppend(&sOut, "*", 1);` |
|      2 | 7294 | `	}else{` |
|      3 | 7295 | `		ph7_class *pDecl = ReflectMemberDecl(&sRef);` |
|      3 | 7296 | `		SyBlobAppend(&sOut, SyStringData(&pDecl->sName), SyStringLength(&pDecl->sName));` |
|      - | 7297 | `	}` |
|      5 | 7298 | `	SyBlobAppend(&sOut, "\0", 1);` |
|      5 | 7299 | `	SyBlobAppend(&sOut, sRef.zName, (sxu32)sRef.nName);` |
|      5 | 7300 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|      5 | 7301 | `	SyBlobRelease(&sOut);` |
|      5 | 7302 | `	return PH7_OK;` |
|      4 | 7303 | `}` |
|      - | 7304 | `/* The boolean predicates, all off the declared attribute. */` |
|     42 | 7305 | `static int ReflectPropFlag(ph7_context *pCtx, int iWhat)` |
|      1 | 7306 | `{` |
|      - | 7307 | `	ReflectMemberRef sRef;` |
|     43 | 7308 | `	int bYes = 0;` |
|     62 | 7309 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|     39 | 7310 | `		ph7_class_attr *pAttr = sRef.pAttr;` |
|     39 | 7311 | `		switch( iWhat ){` |
|    ! 0 | 7312 | `		case 0: bYes = pAttr->iProtection == PH7_CLASS_PROT_PUBLIC; break;` |
|    ! 0 | 7313 | `		case 1: bYes = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE; break;` |
|      3 | 7314 | `		case 2: bYes = pAttr->iProtection == PH7_CLASS_PROT_PROTECTED; break;` |
|      7 | 7315 | `		case 3: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) != 0; break;` |
|      7 | 7316 | `		case 4: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET) != 0; break;` |
|      3 | 7317 | `		case 5: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0; break;` |
|      9 | 7318 | `		case 6: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0; break;` |
|      3 | 7319 | `		case 7: bYes = 1; break;                                    /* isDefault */` |
|    ! 0 | 7320 | `		case 8: bYes = 0; break;                                    /* isDynamic */` |
|      7 | 7321 | `		case 9: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) != 0; break;` |
|      3 | 7322 | `		default:` |
|      7 | 7323 | `			bYes = (pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) != 0;` |
|      6 | 7324 | `			break;` |
|      - | 7325 | `		}` |
|     24 | 7326 | `	}else if( iWhat == 0 \|\| iWhat == 8 ){` |
|      - | 7327 | `		/* A dynamic property is public and, by definition, not a default one. */` |
|      3 | 7328 | `		bYes = 1;` |
|      1 | 7329 | `	}` |
|     43 | 7330 | `	ph7_result_bool(pCtx, bYes);` |
|     43 | 7331 | `	return PH7_OK;` |
|      1 | 7332 | `}` |
|      - | 7333 | `#define REFLECT_PROP_FLAG(NAME,WHAT) \` |
|      - | 7334 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 7335 | `	{ \` |
|      - | 7336 | `		SXUNUSED(nArg); \` |
|      - | 7337 | `		SXUNUSED(apArg); \` |
|      - | 7338 | `		return ReflectPropFlag(pCtx, WHAT); \` |
|      - | 7339 | `	}` |
|    ! 0 | 7340 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPublic, 0)` |
|    ! 0 | 7341 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPrivate, 1)` |
|      3 | 7342 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isProtected, 2)` |
|      7 | 7343 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPrivateSet, 3)` |
|      7 | 7344 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isProtectedSet, 4)` |
|      3 | 7345 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isStatic, 5)` |
|      9 | 7346 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isReadOnly, 6)` |
|      5 | 7347 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isDefault, 7)` |
|      3 | 7348 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isDynamic, 8)` |
|      7 | 7349 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isVirtual, 9)` |
|      7 | 7350 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_hasHooks, 10)` |
|      - | 7351 |  |
|      - | 7352 | `/* php has no abstract properties outside an interface stub, and PHL none at` |
|      - | 7353 | ` * all; isLazy() answers what is true of a VM without lazy objects. */` |
|    ! 0 | 7354 | `static int vm_builtin_ReflectionProperty_false(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 7355 | `{` |
|    ! 0 | 7356 | `	SXUNUSED(nArg);` |
|    ! 0 | 7357 | `	SXUNUSED(apArg);` |
|    ! 0 | 7358 | `	ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7359 | `	return PH7_OK;` |
|    ! 0 | 7360 | `}` |
|      - | 7361 | `/*` |
|      - | 7362 | ` * isPromoted(): the property came from a constructor-promoted parameter. PHL` |
|      - | 7363 | ` * records promotion on the ARGUMENT (VM_FUNC_ARG_PROMOTED), not on the` |
|      - | 7364 | ` * attribute, so the constructor's parameter list is what answers.` |
|      - | 7365 | ` */` |
|      4 | 7366 | `static int vm_builtin_ReflectionProperty_isPromoted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7367 | `{` |
|      - | 7368 | `	ReflectMemberRef sRef;` |
|      - | 7369 | `	ph7_class_method *pCons;` |
|      - | 7370 | `	ph7_vm_func_arg *aArg;` |
|      - | 7371 | `	sxu32 n;` |
|      5 | 7372 | `	int bYes = 0;` |
|      2 | 7373 | `	SXUNUSED(nArg);` |
|      2 | 7374 | `	SXUNUSED(apArg);` |
|      5 | 7375 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|      5 | 7376 | `		ph7_class *pDecl = ReflectMemberDecl(&sRef);` |
|      5 | 7377 | `		pCons = PH7_ClassExtractMethod(pDecl, "__construct", sizeof("__construct")-1);` |
|      5 | 7378 | `		if( pCons ){` |
|      5 | 7379 | `			aArg = (ph7_vm_func_arg *)SySetBasePtr(&pCons->sFunc.aArgs);` |
|      7 | 7380 | `			for( n = 0 ; n < SySetUsed(&pCons->sFunc.aArgs) ; n++ ){` |
|      5 | 7381 | `				if( (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|    ! 0 | 7382 | `					continue;` |
|      - | 7383 | `				}` |
|      4 | 7384 | `				if( SyStringLength(&aArg[n].sName) == (sxu32)sRef.nName` |
|      4 | 7385 | `				 && SyMemcmp(SyStringData(&aArg[n].sName), sRef.zName, (sxu32)sRef.nName) == 0 ){` |
|      3 | 7386 | `					bYes = 1;` |
|      3 | 7387 | `					break;` |
|      - | 7388 | `				}` |
|      2 | 7389 | `			}` |
|      2 | 7390 | `		}` |
|      2 | 7391 | `	}` |
|      5 | 7392 | `	ph7_result_bool(pCtx, bYes);` |
|      5 | 7393 | `	return PH7_OK;` |
|      1 | 7394 | `}` |
|     70 | 7395 | `static int vm_builtin_ReflectionProperty_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 7396 | `{` |
|      - | 7397 | `	ReflectMemberRef sRef;` |
|     35 | 7398 | `	SXUNUSED(nArg);` |
|     35 | 7399 | `	SXUNUSED(apArg);` |
|     72 | 7400 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 7401 | `		ph7_result_int(pCtx, 1);   /* a dynamic property is public */` |
|    ! 0 | 7402 | `		return PH7_OK;` |
|      - | 7403 | `	}` |
|     72 | 7404 | `	ph7_result_int64(pCtx, ReflectPropModifiers(sRef.pAttr));` |
|     72 | 7405 | `	return PH7_OK;` |
|     37 | 7406 | `}` |
|      4 | 7407 | `static int vm_builtin_ReflectionProperty_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7408 | `{` |
|      - | 7409 | `	ReflectMemberRef sRef;` |
|      2 | 7410 | `	SXUNUSED(nArg);` |
|      2 | 7411 | `	SXUNUSED(apArg);` |
|      5 | 7412 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 | 7413 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7414 | `		return PH7_OK;` |
|      - | 7415 | `	}` |
|      5 | 7416 | `	return ReflectResultClassOf(pCtx, ReflectMemberDecl(&sRef));` |
|      3 | 7417 | `}` |
|      6 | 7418 | `static int vm_builtin_ReflectionProperty_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7419 | `{` |
|      - | 7420 | `	ReflectMemberRef sRef;` |
|      3 | 7421 | `	SXUNUSED(nArg);` |
|      3 | 7422 | `	SXUNUSED(apArg);` |
|      6 | 7423 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr` |
|      7 | 7424 | `	 && SyStringLength(&sRef.pAttr->sDoc) > 0 ){` |
|      7 | 7425 | `		ph7_result_string(pCtx, SyStringData(&sRef.pAttr->sDoc),` |
|      4 | 7426 | `			(int)SyStringLength(&sRef.pAttr->sDoc));` |
|      3 | 7427 | `	}else{` |
|      3 | 7428 | `		ph7_result_bool(pCtx, 0);` |
|      - | 7429 | `	}` |
|      7 | 7430 | `	return PH7_OK;` |
|      1 | 7431 | `}` |
|      6 | 7432 | `static int vm_builtin_ReflectionProperty_hasType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7433 | `{` |
|      - | 7434 | `	ReflectMemberRef sRef;` |
|      3 | 7435 | `	SXUNUSED(nArg);` |
|      3 | 7436 | `	SXUNUSED(apArg);` |
|     13 | 7437 | `	ph7_result_bool(pCtx, ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP)` |
|      6 | 7438 | `		&& sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);` |
|      7 | 7439 | `	return PH7_OK;` |
|      1 | 7440 | `}` |
|     56 | 7441 | `static int vm_builtin_ReflectionProperty_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 7442 | `{` |
|      - | 7443 | `	ReflectMemberRef sRef;` |
|     28 | 7444 | `	SXUNUSED(nArg);` |
|     28 | 7445 | `	SXUNUSED(apArg);` |
|     56 | 7446 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0` |
|     56 | 7447 | `	 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|     55 | 7448 | `	 \|\| SyStringLength(&sRef.pAttr->sTypeName) < 1 ){` |
|      7 | 7449 | `		ph7_result_null(pCtx);` |
|      7 | 7450 | `		return PH7_OK;` |
|      - | 7451 | `	}` |
|     77 | 7452 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx,` |
|     50 | 7453 | `		SyStringData(&sRef.pAttr->sTypeName), (int)SyStringLength(&sRef.pAttr->sTypeName)));` |
|     30 | 7454 | `}` |
|     38 | 7455 | `static int vm_builtin_ReflectionProperty_hasDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7456 | `{` |
|      - | 7457 | `	ReflectMemberRef sRef;` |
|     19 | 7458 | `	SXUNUSED(nArg);` |
|     19 | 7459 | `	SXUNUSED(apArg);` |
|     39 | 7460 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 7461 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7462 | `		return PH7_OK;` |
|      - | 7463 | `	}` |
|     39 | 7464 | `	if( SySetUsed(&sRef.pAttr->aByteCode) > 0 \|\| sRef.pAttr->pNativeValue ){` |
|     19 | 7465 | `		ph7_result_bool(pCtx, 1);` |
|     19 | 7466 | `		return PH7_OK;` |
|      - | 7467 | `	}` |
|      - | 7468 | `	/* An UNTYPED property with no initializer still defaults to null. */` |
|     21 | 7469 | `	ph7_result_bool(pCtx, (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0);` |
|     21 | 7470 | `	return PH7_OK;` |
|     20 | 7471 | `}` |
|     24 | 7472 | `static int vm_builtin_ReflectionProperty_getDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7473 | `{` |
|      - | 7474 | `	ReflectMemberRef sRef;` |
|      - | 7475 | `	ph7_value sValue;` |
|     12 | 7476 | `	SXUNUSED(nArg);` |
|     12 | 7477 | `	SXUNUSED(apArg);` |
|     25 | 7478 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 | 7479 | `		sRef.pAttr = 0;` |
|    ! 0 | 7480 | `	}` |
|     25 | 7481 | `	if( sRef.pAttr && sRef.pAttr->pNativeValue && SySetUsed(&sRef.pAttr->aByteCode) < 1 ){` |
|      - | 7482 | `		/* A NATIVE property's default is a LITERAL, not byte-code — the same` |
|      - | 7483 | ``		 * record `new` materializes (PH7_NativeLiteralValue). Reading only the`` |
|      - | 7484 | `		 * byte-code answered NULL for every declared native default and raised` |
|      - | 7485 | `		 * php's no-default deprecation on a slot that hasDefaultValue() had just` |
|      - | 7486 | `		 * reported true for. */` |
|     15 | 7487 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     15 | 7488 | `		PH7_NativeLiteralValue(pCtx->pVm, sRef.pAttr->pNativeValue, &sValue);` |
|     15 | 7489 | `		ph7_result_value(pCtx, &sValue);` |
|     15 | 7490 | `		PH7_MemObjRelease(&sValue);` |
|     15 | 7491 | `		return PH7_OK;` |
|      - | 7492 | `	}` |
|     11 | 7493 | `	if( sRef.pAttr == 0 \|\| SySetUsed(&sRef.pAttr->aByteCode) < 1 ){` |
|      - | 7494 | `		/* php 8.5 deprecates the question when there is no default — an` |
|      - | 7495 | `		 * UNTYPED property still has one (null), a typed one without an` |
|      - | 7496 | `		 * initializer does not. */` |
|      5 | 7497 | `		if( sRef.pAttr == 0 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|      3 | 7498 | `			PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED,` |
|      - | 7499 | `				"ReflectionProperty::getDefaultValue() for a property without a default "` |
|      - | 7500 | `				"value is deprecated, use ReflectionProperty::hasDefaultValue() to check "` |
|      - | 7501 | `				"if the default value exists");` |
|      1 | 7502 | `		}` |
|      5 | 7503 | `		ph7_result_null(pCtx);` |
|      5 | 7504 | `		return PH7_OK;` |
|      - | 7505 | `	}` |
|      - | 7506 | `	/* Same evaluation path the VM uses for an omitted call argument */` |
|      7 | 7507 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|      7 | 7508 | `	VmLocalExec(pCtx->pVm, &sRef.pAttr->aByteCode, &sValue, FALSE);` |
|      7 | 7509 | `	ph7_result_value(pCtx, &sValue);` |
|      7 | 7510 | `	PH7_MemObjRelease(&sValue);` |
|      7 | 7511 | `	return PH7_OK;` |
|     13 | 7512 | `}` |
|      - | 7513 | `/* php 8.1 made every reflected member accessible; the setter is a no-op it kept. */` |
|    ! 0 | 7514 | `static int vm_builtin_ReflectionProperty_setAccessible(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|    ! 0 | 7515 | `{` |
|    ! 0 | 7516 | `	SXUNUSED(pCtx);` |
|    ! 0 | 7517 | `	SXUNUSED(nArg);` |
|    ! 0 | 7518 | `	SXUNUSED(apArg);` |
|    ! 0 | 7519 | `	return PH7_OK;` |
|    ! 0 | 7520 | `}` |
|      - | 7521 | `/* getValue()/setValue()/isInitialized() all need the same receiver check. */` |
|     46 | 7522 | `static sxi32 ReflectPropReceiver(ph7_context *pCtx, ReflectMemberRef *pRef,` |
|      - | 7523 | `	ph7_value *pObject, ph7_class_instance **ppThis, const char *zWho)` |
|      1 | 7524 | `{` |
|     47 | 7525 | `	*ppThis = 0;` |
|     23 | 7526 | `	SXUNUSED(pRef);` |
|     47 | 7527 | `	if( pObject && (pObject->iFlags & MEMOBJ_OBJ) ){` |
|     45 | 7528 | `		*ppThis = (ph7_class_instance *)pObject->x.pOther;` |
|     45 | 7529 | `		return PH7_OK;` |
|      - | 7530 | `	}` |
|      4 | 7531 | `	return PH7_VmThrowException(pCtx, "TypeError",` |
|      - | 7532 | `		"ReflectionProperty::%s(): Argument #1 ($object) must be provided for instance properties",` |
|      1 | 7533 | `		zWho);` |
|     24 | 7534 | `}` |
|     32 | 7535 | `static int vm_builtin_ReflectionProperty_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 7536 | `{` |
|      - | 7537 | `	ReflectMemberRef sRef;` |
|      - | 7538 | `	ph7_class_instance *pObj;` |
|      - | 7539 | `	VmClassAttr *pVmAttr;` |
|      - | 7540 | `	ph7_value *pValue;` |
|      - | 7541 | `	sxi32 rc;` |
|     34 | 7542 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 | 7543 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7544 | `		return PH7_OK;` |
|      - | 7545 | `	}` |
|     34 | 7546 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|      8 | 7547 | `		return ReflectStaticSlotRead(pCtx, sRef.pClass, sRef.pAttr);` |
|      - | 7548 | `	}` |
|     27 | 7549 | `	rc = ReflectPropReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pObj, "getValue");` |
|     27 | 7550 | `	if( rc != PH7_OK ){` |
|      3 | 7551 | `		return rc;` |
|      - | 7552 | `	}` |
|     25 | 7553 | `	pVmAttr = ReflectInstanceAttr(pObj, sRef.zName, sRef.nName);` |
|     25 | 7554 | `	if( pVmAttr == 0 ){` |
|    ! 0 | 7555 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7556 | `		return PH7_OK;` |
|      - | 7557 | `	}` |
|     25 | 7558 | `	if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|      3 | 7559 | `		ph7_class *pDecl = pVmAttr->pAttr && pVmAttr->pAttr->pDeclClass` |
|      3 | 7560 | `			? pVmAttr->pAttr->pDeclClass : pObj->pClass;` |
|      4 | 7561 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      - | 7562 | `			"Typed property %z::$%.*s must not be accessed before initialization",` |
|      1 | 7563 | `			&pDecl->sName, sRef.nName, sRef.zName);` |
|      - | 7564 | `	}` |
|     23 | 7565 | `	pValue = PH7_ClassInstanceExtractAttrValue(pObj, pVmAttr);` |
|     23 | 7566 | `	if( pValue ){` |
|     23 | 7567 | `		ph7_result_value(pCtx, pValue);` |
|     12 | 7568 | `	}else{` |
|    ! 0 | 7569 | `		ph7_result_null(pCtx);` |
|      - | 7570 | `	}` |
|     23 | 7571 | `	return PH7_OK;` |
|     18 | 7572 | `}` |
|     14 | 7573 | `static int vm_builtin_ReflectionProperty_setValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 7574 | `{` |
|      - | 7575 | `	ReflectMemberRef sRef;` |
|      - | 7576 | `	ph7_class_instance *pObj;` |
|      - | 7577 | `	VmClassAttr *pVmAttr;` |
|      - | 7578 | `	ph7_value *pSlot;` |
|      - | 7579 | `	sxi32 rc;` |
|     16 | 7580 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| nArg < 1 ){` |
|    ! 0 | 7581 | `		return PH7_OK;` |
|      - | 7582 | `	}` |
|     16 | 7583 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|      - | 7584 | `		/* php's one-argument spelling for a static: setValue($value). Two` |
|      - | 7585 | `		 * arguments, or one OBJECT, means the instance form was used and the` |
|      - | 7586 | `		 * value is the second. */` |
|      6 | 7587 | `		ph7_value *pVal = apArg[0];` |
|      6 | 7588 | `		if( nArg > 1 ){` |
|      6 | 7589 | `			pVal = apArg[1];` |
|      2 | 7590 | `		}else if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|    ! 0 | 7591 | `			return PH7_OK;` |
|      - | 7592 | `		}` |
|      6 | 7593 | `		return ReflectStaticSlotWrite(pCtx, sRef.pClass, sRef.pAttr, pVal);` |
|      - | 7594 | `	}` |
|     11 | 7595 | `	rc = ReflectPropReceiver(pCtx, &sRef, apArg[0], &pObj, "setValue");` |
|     11 | 7596 | `	if( rc != PH7_OK ){` |
|    ! 0 | 7597 | `		return rc;` |
|      - | 7598 | `	}` |
|     11 | 7599 | `	pVmAttr = ReflectInstanceAttr(pObj, sRef.zName, sRef.nName);` |
|     11 | 7600 | `	if( pVmAttr == 0 ){` |
|    ! 0 | 7601 | `		return PH7_OK;` |
|      - | 7602 | `	}` |
|      - | 7603 | `	{` |
|     11 | 7604 | `		ph7_value *pVal = nArg > 1 ? apArg[1] : 0;` |
|      - | 7605 | `		ph7_value sNull;` |
|     11 | 7606 | `		PH7_MemObjInit(pCtx->pVm, &sNull);` |
|     11 | 7607 | `		if( pVal == 0 ){` |
|    ! 0 | 7608 | `			pVal = &sNull;` |
|    ! 0 | 7609 | `		}` |
|     11 | 7610 | `		rc = ReflectEnforceStore(pCtx, pVmAttr->nIdx, pVal);` |
|     11 | 7611 | `		if( rc != SXRET_OK ){` |
|      5 | 7612 | `			PH7_MemObjRelease(&sNull);` |
|      5 | 7613 | `			return rc;` |
|      - | 7614 | `		}` |
|      7 | 7615 | `		pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|      7 | 7616 | `		if( pSlot ){` |
|      7 | 7617 | `			PH7_MemObjStore(pVal, pSlot);` |
|      7 | 7618 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|      3 | 7619 | `		}` |
|      7 | 7620 | `		PH7_MemObjRelease(&sNull);` |
|      - | 7621 | `	}` |
|      7 | 7622 | `	return PH7_OK;` |
|      9 | 7623 | `}` |
|     16 | 7624 | `static int vm_builtin_ReflectionProperty_isInitialized(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 | 7625 | `{` |
|      - | 7626 | `	ReflectMemberRef sRef;` |
|      - | 7627 | `	ph7_class_instance *pObj;` |
|      - | 7628 | `	VmClassAttr *pVmAttr;` |
|      - | 7629 | `	sxi32 rc;` |
|     18 | 7630 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 | 7631 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 7632 | `		return PH7_OK;` |
|      - | 7633 | `	}` |
|     18 | 7634 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|      - | 7635 | `		SyHashEntry *pSlot;` |
|      8 | 7636 | `		rc = ReflectMaterializeStatics(pCtx, sRef.pClass);` |
|      8 | 7637 | `		if( rc != SXRET_OK ){` |
|      5 | 7638 | `			return rc;` |
|      - | 7639 | `		}` |
|      3 | 7640 | `		pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&sRef.pAttr->nIdx, sizeof(sxu32));` |
|      5 | 7641 | `		ph7_result_bool(pCtx, pSlot == 0` |
|      2 | 7642 | `			\|\| (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) == 0);` |
|      3 | 7643 | `		return PH7_OK;` |
|      - | 7644 | `	}` |
|     11 | 7645 | `	rc = ReflectPropReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pObj, "isInitialized");` |
|     11 | 7646 | `	if( rc != PH7_OK ){` |
|    ! 0 | 7647 | `		return rc;` |
|      - | 7648 | `	}` |
|     11 | 7649 | `	pVmAttr = ReflectInstanceAttr(pObj, sRef.zName, sRef.nName);` |
|     11 | 7650 | `	ph7_result_bool(pCtx, pVmAttr != 0 && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0);` |
|     11 | 7651 | `	return PH7_OK;` |
|     10 | 7652 | `}` |
|      - | 7653 | `/*` |
|      - | 7654 | `` * The property HOOKS (php 8.4). PHL compiles `public $p { get => ...; }` into`` |
|      - | 7655 | `` * synthesized methods named `__phl_hook_get_NAME` / `__phl_hook_set_NAME`, so`` |
|      - | 7656 | ` * the reflector php answers is a ReflectionMethod over one of those.` |
|      - | 7657 | ` */` |
|     30 | 7658 | `static int ReflectHookMethod(ph7_context *pCtx, ReflectMemberRef *pRef, int bSet,` |
|      - | 7659 | `	ph7_class_instance **ppOut)` |
|      3 | 7660 | `{` |
|      - | 7661 | `	char zName[128];` |
|      - | 7662 | `	ph7_value sClass, sName;` |
|      - | 7663 | `	ph7_value *apCtor[2];` |
|      - | 7664 | `	ph7_class *pDecl;` |
|      - | 7665 | `	sxi32 rc;` |
|      - | 7666 | `	int nName;` |
|     33 | 7667 | `	*ppOut = 0;` |
|     33 | 7668 | `	if( pRef->pAttr == 0 ){` |
|    ! 0 | 7669 | `		return PH7_OK;` |
|      - | 7670 | `	}` |
|     33 | 7671 | `	if( (pRef->pAttr->iFlags & (bSet ? PH7_CLASS_ATTR_HOOK_SET : PH7_CLASS_ATTR_HOOK_GET)) == 0 ){` |
|     15 | 7672 | `		return PH7_OK;` |
|      - | 7673 | `	}` |
|     21 | 7674 | `	pDecl = ReflectMemberDecl(pRef);` |
|     30 | 7675 | `	nName = SyBufferFormat(zName, sizeof(zName), "%s%.*s",` |
|      9 | 7676 | `		bSet ? "__phl_hook_set_" : "__phl_hook_get_", pRef->nName, pRef->zName);` |
|     21 | 7677 | `	PH7_MemObjInit(pCtx->pVm, &sClass);` |
|     21 | 7678 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|     21 | 7679 | `	ph7_value_string(&sClass, SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|     21 | 7680 | `	ph7_value_string(&sName, zName, nName);` |
|     21 | 7681 | `	apCtor[0] = &sClass;` |
|     21 | 7682 | `	apCtor[1] = &sName;` |
|     21 | 7683 | `	*ppOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|     21 | 7684 | `	PH7_MemObjRelease(&sClass);` |
|     21 | 7685 | `	PH7_MemObjRelease(&sName);` |
|     21 | 7686 | `	return rc;` |
|     18 | 7687 | `}` |
|     12 | 7688 | `static int vm_builtin_ReflectionProperty_getHooks(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 | 7689 | `{` |
|      - | 7690 | `	ReflectMemberRef sRef;` |
|     15 | 7691 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      - | 7692 | `	int iHook;` |
|      6 | 7693 | `	SXUNUSED(nArg);` |
|      6 | 7694 | `	SXUNUSED(apArg);` |
|     15 | 7695 | `	if( pOut == 0 ){` |
|    ! 0 | 7696 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 7697 | `	}` |
|     15 | 7698 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 | 7699 | `		ph7_result_value(pCtx, pOut);` |
|    ! 0 | 7700 | `		return PH7_OK;` |
|      - | 7701 | `	}` |
|     39 | 7702 | `	for( iHook = 0 ; iHook < 2 ; iHook++ ){` |
|     27 | 7703 | `		ph7_class_instance *pMeth = 0;` |
|      - | 7704 | `		ph7_value sVal, *pKey;` |
|     27 | 7705 | `		sxi32 rc = ReflectHookMethod(pCtx, &sRef, iHook, &pMeth);` |
|     27 | 7706 | `		if( rc != PH7_OK ){` |
|    ! 0 | 7707 | `			return rc;` |
|      - | 7708 | `		}` |
|     27 | 7709 | `		if( pMeth == 0 ){` |
|     13 | 7710 | `			continue;` |
|      - | 7711 | `		}` |
|     17 | 7712 | `		pKey = ph7_context_new_scalar(pCtx);` |
|     17 | 7713 | `		if( pKey == 0 ){` |
|    ! 0 | 7714 | `			PH7_ClassInstanceUnref(pMeth);` |
|    ! 0 | 7715 | `			break;` |
|      - | 7716 | `		}` |
|     17 | 7717 | `		ph7_value_string(pKey, iHook ? "set" : "get", 3);` |
|     17 | 7718 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     17 | 7719 | `		sVal.x.pOther = pMeth;` |
|     17 | 7720 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|     17 | 7721 | `		ph7_array_add_elem(pOut, pKey, &sVal);   /* takes its own reference */` |
|     17 | 7722 | `		PH7_ClassInstanceUnref(pMeth);` |
|     10 | 7723 | `	}` |
|     15 | 7724 | `	ph7_result_value(pCtx, pOut);` |
|     15 | 7725 | `	return PH7_OK;` |
|      9 | 7726 | `}` |
|      - | 7727 | ``/* `get`/`set` out of a PropertyHookType case (or the bare string php also takes). */`` |
|     12 | 7728 | `static int ReflectHookKind(ph7_value *pArg)` |
|      1 | 7729 | `{` |
|      - | 7730 | `	const char *z;` |
|      - | 7731 | `	int n;` |
|     13 | 7732 | `	if( pArg == 0 ){` |
|    ! 0 | 7733 | `		return -1;` |
|      - | 7734 | `	}` |
|     13 | 7735 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|     13 | 7736 | `		ph7_class_instance *pCase = (ph7_class_instance *)pArg->x.pOther;` |
|     13 | 7737 | `		ph7_value *pVal = PH7_NativeAttr(pCase, "value");` |
|     13 | 7738 | `		if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|    ! 0 | 7739 | `			return -1;` |
|      - | 7740 | `		}` |
|     13 | 7741 | `		z = (const char *)SyBlobData(&pVal->sBlob);` |
|     13 | 7742 | `		n = (int)SyBlobLength(&pVal->sBlob);` |
|      7 | 7743 | `	}else{` |
|    ! 0 | 7744 | `		z = ph7_value_to_string(pArg, &n);` |
|      - | 7745 | `	}` |
|     13 | 7746 | `	if( n == 3 && SyMemcmp(z, "get", 3) == 0 ){` |
|      7 | 7747 | `		return 0;` |
|      - | 7748 | `	}` |
|      7 | 7749 | `	if( n == 3 && SyMemcmp(z, "set", 3) == 0 ){` |
|      7 | 7750 | `		return 1;` |
|      - | 7751 | `	}` |
|    ! 0 | 7752 | `	return -1;` |
|      7 | 7753 | `}` |
|      6 | 7754 | `static int vm_builtin_ReflectionProperty_hasHook(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7755 | `{` |
|      - | 7756 | `	ReflectMemberRef sRef;` |
|      7 | 7757 | `	int iHook = ReflectHookKind(nArg > 0 ? apArg[0] : 0);` |
|      7 | 7758 | `	int bYes = 0;` |
|      7 | 7759 | `	if( iHook >= 0 && ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|      7 | 7760 | `		bYes = (sRef.pAttr->iFlags & (iHook ? PH7_CLASS_ATTR_HOOK_SET : PH7_CLASS_ATTR_HOOK_GET)) != 0;` |
|      3 | 7761 | `	}` |
|      7 | 7762 | `	ph7_result_bool(pCtx, bYes);` |
|      7 | 7763 | `	return PH7_OK;` |
|      1 | 7764 | `}` |
|      6 | 7765 | `static int vm_builtin_ReflectionProperty_getHook(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7766 | `{` |
|      - | 7767 | `	ReflectMemberRef sRef;` |
|      7 | 7768 | `	ph7_class_instance *pMeth = 0;` |
|      7 | 7769 | `	int iHook = ReflectHookKind(nArg > 0 ? apArg[0] : 0);` |
|      - | 7770 | `	sxi32 rc;` |
|      7 | 7771 | `	if( iHook < 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|    ! 0 | 7772 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7773 | `		return PH7_OK;` |
|      - | 7774 | `	}` |
|      7 | 7775 | `	rc = ReflectHookMethod(pCtx, &sRef, iHook, &pMeth);` |
|      7 | 7776 | `	if( rc != PH7_OK ){` |
|    ! 0 | 7777 | `		return rc;` |
|      - | 7778 | `	}` |
|      7 | 7779 | `	if( pMeth == 0 ){` |
|      3 | 7780 | `		ph7_result_null(pCtx);` |
|      3 | 7781 | `		return PH7_OK;` |
|      - | 7782 | `	}` |
|      5 | 7783 | `	return ReflectResultObject(pCtx, pMeth);` |
|      4 | 7784 | `}` |
|      6 | 7785 | `static int vm_builtin_ReflectionProperty_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7786 | `{` |
|      7 | 7787 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 7788 | `	ReflectMemberRef sRef;` |
|      - | 7789 | `	ph7_value sTarget;` |
|      - | 7790 | `	const char *zClass;` |
|      - | 7791 | `	int nClass, rc;` |
|      7 | 7792 | `	if( pThis == 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 7793 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 | 7794 | `		return PH7_OK;` |
|      - | 7795 | `	}` |
|      7 | 7796 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|      7 | 7797 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      7 | 7798 | `	ph7_value_string(&sTarget, zClass, nClass);` |
|      - | 7799 | `	/* 8 = Attribute::TARGET_PROPERTY */` |
|     10 | 7800 | `	rc = ReflectBuildAttrs(pCtx, &sRef.pAttr->aAttrs, "attr", &sTarget,` |
|      3 | 7801 | `		sRef.zName, sRef.nName, 0, 8, nArg, apArg);` |
|      7 | 7802 | `	PH7_MemObjRelease(&sTarget);` |
|      7 | 7803 | `	return rc;` |
|      4 | 7804 | `}` |
|      4 | 7805 | `static int vm_builtin_ReflectionProperty_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7806 | `{` |
|      2 | 7807 | `	SXUNUSED(nArg);` |
|      2 | 7808 | `	SXUNUSED(apArg);` |
|      5 | 7809 | `	return ReflectExportPropSelf(pCtx);` |
|      1 | 7810 | `}` |
|      - | 7811 | `/* ---- ReflectionClassConstant ---- */` |
|      - | 7812 | `/* ReflectionClassConstant::__construct(object\|string $class, string $constant) */` |
|     72 | 7813 | `static int vm_builtin_ReflectionClassConstant_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7814 | `{` |
|     73 | 7815 | `	ph7_vm *pVm = pCtx->pVm;` |
|     73 | 7816 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     73 | 7817 | `	ph7_class *pClass, *pDecl = 0;` |
|      - | 7818 | `	const char *zConst;` |
|      - | 7819 | `	int nConst;` |
|      - | 7820 | `	SySet aMembers;` |
|      - | 7821 | `	sxu32 n;` |
|     73 | 7822 | `	int bFound = 0;` |
|     73 | 7823 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|    ! 0 | 7824 | `		return PH7_OK;` |
|      - | 7825 | `	}` |
|     73 | 7826 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|     73 | 7827 | `	if( pClass == 0 ){` |
|      - | 7828 | `		const char *zName;` |
|      - | 7829 | `		int nName;` |
|    ! 0 | 7830 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|    ! 0 | 7831 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|    ! 0 | 7832 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|      - | 7833 | `	}` |
|     73 | 7834 | `	zConst = ph7_value_to_string(apArg[1], &nConst);` |
|     73 | 7835 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|     73 | 7836 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|    357 | 7837 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    351 | 7838 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|    351 | 7839 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zConst, nConst) ){` |
|     67 | 7840 | `			pDecl = pM->pDecl ? pM->pDecl : pClass;` |
|     67 | 7841 | `			bFound = 1;` |
|     67 | 7842 | `			break;` |
|      - | 7843 | `		}` |
|    143 | 7844 | `	}` |
|     73 | 7845 | `	SySetRelease(&aMembers);` |
|     73 | 7846 | `	if( !bFound ){` |
|     10 | 7847 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 | 7848 | `			"Constant %z::%.*s does not exist", &pClass->sName, nConst, zConst);` |
|      - | 7849 | `	}` |
|      - | 7850 | `	/* php's $class is the DECLARING class, not the one asked about. */` |
|     67 | 7851 | `	pClass = pDecl;` |
|    100 | 7852 | `	PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pClass->sName),` |
|     66 | 7853 | `		(int)SyStringLength(&pClass->sName));` |
|     67 | 7854 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", zConst, nConst);` |
|     67 | 7855 | `	return PH7_OK;` |
|     37 | 7856 | `}` |
|     20 | 7857 | `static int vm_builtin_ReflectionClassConstant_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7858 | `{` |
|      - | 7859 | `	ReflectMemberRef sRef;` |
|      - | 7860 | `	ph7_value *pVal;` |
|      - | 7861 | `	sxi32 rc;` |
|     10 | 7862 | `	SXUNUSED(nArg);` |
|     10 | 7863 | `	SXUNUSED(apArg);` |
|     21 | 7864 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 7865 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7866 | `		return PH7_OK;` |
|      - | 7867 | `	}` |
|     21 | 7868 | `	rc = ReflectConstSlot(pCtx, sRef.pClass, sRef.pAttr, &pVal);` |
|     21 | 7869 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 7870 | `		return rc;` |
|      - | 7871 | `	}` |
|     21 | 7872 | `	if( pVal ){` |
|     21 | 7873 | `		ph7_result_value(pCtx, pVal);` |
|     11 | 7874 | `	}else{` |
|    ! 0 | 7875 | `		ph7_result_null(pCtx);` |
|      - | 7876 | `	}` |
|     21 | 7877 | `	return PH7_OK;` |
|     11 | 7878 | `}` |
|     24 | 7879 | `static int ReflectConstFlag(ph7_context *pCtx, int iWhat)` |
|      1 | 7880 | `{` |
|      - | 7881 | `	ReflectMemberRef sRef;` |
|     25 | 7882 | `	int bYes = 0;` |
|     25 | 7883 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) && sRef.pAttr ){` |
|     25 | 7884 | `		ph7_class_attr *pAttr = sRef.pAttr;` |
|     25 | 7885 | `		switch( iWhat ){` |
|      3 | 7886 | `		case 0: bYes = pAttr->iProtection == PH7_CLASS_PROT_PUBLIC; break;` |
|      3 | 7887 | `		case 1: bYes = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE; break;` |
|      3 | 7888 | `		case 2: bYes = pAttr->iProtection == PH7_CLASS_PROT_PROTECTED; break;` |
|      3 | 7889 | `		case 3: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0; break;` |
|     13 | 7890 | `		case 4: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0; break;` |
|      3 | 7891 | `		case 5: bYes = ReflectHasDeprecated(&pAttr->aAttrs); break;` |
|      3 | 7892 | `		default: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0; break;` |
|      - | 7893 | `		}` |
|     12 | 7894 | `	}` |
|     25 | 7895 | `	ph7_result_bool(pCtx, bYes);` |
|     25 | 7896 | `	return PH7_OK;` |
|      1 | 7897 | `}` |
|      - | 7898 | `#define REFLECT_CONST_FLAG(NAME,WHAT) \` |
|      - | 7899 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|      - | 7900 | `	{ \` |
|      - | 7901 | `		SXUNUSED(nArg); \` |
|      - | 7902 | `		SXUNUSED(apArg); \` |
|      - | 7903 | `		return ReflectConstFlag(pCtx, WHAT); \` |
|      - | 7904 | `	}` |
|      3 | 7905 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isPublic, 0)` |
|      3 | 7906 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isPrivate, 1)` |
|      3 | 7907 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isProtected, 2)` |
|      3 | 7908 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isFinal, 3)` |
|     13 | 7909 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isEnumCase, 4)` |
|      3 | 7910 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isDeprecated, 5)` |
|      3 | 7911 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_hasType, 6)` |
|      - | 7912 |  |
|      8 | 7913 | `static int vm_builtin_ReflectionClassConstant_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7914 | `{` |
|      - | 7915 | `	ReflectMemberRef sRef;` |
|      4 | 7916 | `	SXUNUSED(nArg);` |
|      4 | 7917 | `	SXUNUSED(apArg);` |
|      9 | 7918 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 7919 | `		ph7_result_int(pCtx, 0);` |
|    ! 0 | 7920 | `		return PH7_OK;` |
|      - | 7921 | `	}` |
|      9 | 7922 | `	ph7_result_int64(pCtx, ReflectConstModifiers(sRef.pAttr));` |
|      9 | 7923 | `	return PH7_OK;` |
|      5 | 7924 | `}` |
|      4 | 7925 | `static int vm_builtin_ReflectionClassConstant_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7926 | `{` |
|      - | 7927 | `	ReflectMemberRef sRef;` |
|      2 | 7928 | `	SXUNUSED(nArg);` |
|      2 | 7929 | `	SXUNUSED(apArg);` |
|      5 | 7930 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) ){` |
|    ! 0 | 7931 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7932 | `		return PH7_OK;` |
|      - | 7933 | `	}` |
|      5 | 7934 | `	return ReflectResultClassOf(pCtx, ReflectMemberDecl(&sRef));` |
|      3 | 7935 | `}` |
|      2 | 7936 | `static int vm_builtin_ReflectionClassConstant_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7937 | `{` |
|      - | 7938 | `	ReflectMemberRef sRef;` |
|      1 | 7939 | `	SXUNUSED(nArg);` |
|      1 | 7940 | `	SXUNUSED(apArg);` |
|      2 | 7941 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) && sRef.pAttr` |
|      3 | 7942 | `	 && SyStringLength(&sRef.pAttr->sDoc) > 0 ){` |
|      4 | 7943 | `		ph7_result_string(pCtx, SyStringData(&sRef.pAttr->sDoc),` |
|      2 | 7944 | `			(int)SyStringLength(&sRef.pAttr->sDoc));` |
|      2 | 7945 | `	}else{` |
|    ! 0 | 7946 | `		ph7_result_bool(pCtx, 0);` |
|      - | 7947 | `	}` |
|      3 | 7948 | `	return PH7_OK;` |
|      1 | 7949 | `}` |
|      2 | 7950 | `static int vm_builtin_ReflectionClassConstant_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7951 | `{` |
|      - | 7952 | `	ReflectMemberRef sRef;` |
|      1 | 7953 | `	SXUNUSED(nArg);` |
|      1 | 7954 | `	SXUNUSED(apArg);` |
|      2 | 7955 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0` |
|      2 | 7956 | `	 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|      3 | 7957 | `	 \|\| SyStringLength(&sRef.pAttr->sTypeName) < 1 ){` |
|    ! 0 | 7958 | `		ph7_result_null(pCtx);` |
|    ! 0 | 7959 | `		return PH7_OK;` |
|      - | 7960 | `	}` |
|      4 | 7961 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx,` |
|      2 | 7962 | `		SyStringData(&sRef.pAttr->sTypeName), (int)SyStringLength(&sRef.pAttr->sTypeName)));` |
|      2 | 7963 | `}` |
|      2 | 7964 | `static int vm_builtin_ReflectionClassConstant_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7965 | `{` |
|      3 | 7966 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|      - | 7967 | `	ReflectMemberRef sRef;` |
|      - | 7968 | `	ph7_value sTarget;` |
|      - | 7969 | `	const char *zClass;` |
|      - | 7970 | `	int nClass, rc;` |
|      3 | 7971 | `	if( pThis == 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 7972 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|    ! 0 | 7973 | `		return PH7_OK;` |
|      - | 7974 | `	}` |
|      3 | 7975 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|      3 | 7976 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|      3 | 7977 | `	ph7_value_string(&sTarget, zClass, nClass);` |
|      - | 7978 | `	/* 16 = Attribute::TARGET_CLASS_CONSTANT */` |
|      4 | 7979 | `	rc = ReflectBuildAttrs(pCtx, &sRef.pAttr->aAttrs, "attr", &sTarget,` |
|      1 | 7980 | `		sRef.zName, sRef.nName, 0, 16, nArg, apArg);` |
|      3 | 7981 | `	PH7_MemObjRelease(&sTarget);` |
|      3 | 7982 | `	return rc;` |
|      2 | 7983 | `}` |
|      6 | 7984 | `static int vm_builtin_ReflectionClassConstant_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 7985 | `{` |
|      3 | 7986 | `	SXUNUSED(nArg);` |
|      3 | 7987 | `	SXUNUSED(apArg);` |
|      7 | 7988 | `	return ReflectExportConstSelf(pCtx);` |
|      1 | 7989 | `}` |
|      - | 7990 | `/*` |
|      - | 7991 | ` * Declare chunk 3. Called from PH7_VmInstallReflection() where it used to be` |
|      - | 7992 | ` * compiled — after chunks 1 and 2, whose ReflectionClass and ReflectionMethod` |
|      - | 7993 | ` * these answer, and after PropertyHookType, which hasHook()/getHook() declare.` |
|      - | 7994 | ` *` |
|      - | 7995 | ` * The method tables are in php's own DECLARATION order.` |
|      - | 7996 | ` */` |
|   5254 | 7997 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionMember(ph7_vm *pVm)` |
|      5 | 7998 | `{` |
|      - | 7999 | `	static const PH7_NativePropDef aMemberProp[] = {` |
|      - | 8000 | `		{ "name",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 8001 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 8002 | `	};` |
|      - | 8003 | `	static const PH7_NativePropDef aPropProp[] = {` |
|      - | 8004 | `		{ "name",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 8005 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|      - | 8006 | `		/* PHL-only: the instance a DYNAMIC property was reached through (§7.4 (e)) */` |
|      - | 8007 | `		{ RP_DYNOBJ, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|      - | 8008 | `	};` |
|      - | 8009 | `	static const PH7_NativeConstDef aPropConst[] = {` |
|      - | 8010 | `		{ "IS_STATIC",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,   0, 0.0 },` |
|      - | 8011 | `		{ "IS_READONLY",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 128,  0, 0.0 },` |
|      - | 8012 | `		{ "IS_PUBLIC",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,    0, 0.0 },` |
|      - | 8013 | `		{ "IS_PROTECTED",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,    0, 0.0 },` |
|      - | 8014 | `		{ "IS_PRIVATE",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,    0, 0.0 },` |
|      - | 8015 | `		{ "IS_ABSTRACT",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64,   0, 0.0 },` |
|      - | 8016 | `		{ "IS_PROTECTED_SET", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2048, 0, 0.0 },` |
|      - | 8017 | `		{ "IS_PRIVATE_SET",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4096, 0, 0.0 },` |
|      - | 8018 | `		{ "IS_VIRTUAL",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 512,  0, 0.0 },` |
|      - | 8019 | `		{ "IS_FINAL",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32,   0, 0.0 },` |
|      - | 8020 | `	};` |
|      - | 8021 | `	static const PH7_NativeMethodDef aPropMethod[] = {` |
|      - | 8022 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - | 8023 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $property", "",` |
|      - | 8024 | `		  vm_builtin_ReflectionProperty_construct },` |
|      - | 8025 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionProperty_toString },` |
|      - | 8026 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionProperty_getName },` |
|      - | 8027 | `		{ "getMangledName", PH7_MOD_PUBLIC, "", "string",` |
|      - | 8028 | `		  vm_builtin_ReflectionProperty_getMangledName },` |
|      - | 8029 | `		{ "getValue",    PH7_MOD_PUBLIC, "?object $object = null", "@mixed",` |
|      - | 8030 | `		  vm_builtin_ReflectionProperty_getValue },` |
|      - | 8031 | `		{ "setValue",    PH7_MOD_PUBLIC, "mixed $objectOrValue, mixed $value = ?", "@void",` |
|      - | 8032 | `		  vm_builtin_ReflectionProperty_setValue },` |
|      - | 8033 | `		{ "getRawValue", PH7_MOD_PUBLIC, "object $object", "mixed",` |
|      - | 8034 | `		  vm_builtin_ReflectionProperty_getValue },` |
|      - | 8035 | `		{ "setRawValue", PH7_MOD_PUBLIC, "object $object, mixed $value", "void",` |
|      - | 8036 | `		  vm_builtin_ReflectionProperty_setValue },` |
|      - | 8037 | `		/* On a non-lazy object — the only kind PHL has — php's two lazy writers` |
|      - | 8038 | `		 * are an ordinary raw write and a no-op. */` |
|      - | 8039 | `		{ "setRawValueWithoutLazyInitialization", PH7_MOD_PUBLIC,` |
|      - | 8040 | `		  "object $object, mixed $value", "void", vm_builtin_ReflectionProperty_setValue },` |
|      - | 8041 | `		{ "skipLazyInitialization", PH7_MOD_PUBLIC, "object $object", "void",` |
|      - | 8042 | `		  vm_builtin_ReflectionProperty_noop },` |
|      - | 8043 | `		{ "isLazy",      PH7_MOD_PUBLIC, "object $object", "bool",` |
|      - | 8044 | `		  vm_builtin_ReflectionProperty_false },` |
|      - | 8045 | `		{ "isInitialized", PH7_MOD_PUBLIC, "?object $object = null", "@bool",` |
|      - | 8046 | `		  vm_builtin_ReflectionProperty_isInitialized },` |
|      - | 8047 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isPublic },` |
|      - | 8048 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isPrivate },` |
|      - | 8049 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isProtected },` |
|      - | 8050 | `		{ "isPrivateSet", PH7_MOD_PUBLIC, "", "bool",` |
|      - | 8051 | `		  vm_builtin_ReflectionProperty_isPrivateSet },` |
|      - | 8052 | `		{ "isProtectedSet", PH7_MOD_PUBLIC, "", "bool",` |
|      - | 8053 | `		  vm_builtin_ReflectionProperty_isProtectedSet },` |
|      - | 8054 | `		{ "isStatic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isStatic },` |
|      - | 8055 | `		{ "isReadOnly",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isReadOnly },` |
|      - | 8056 | `		{ "isDefault",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isDefault },` |
|      - | 8057 | `		{ "isDynamic",   PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isDynamic },` |
|      - | 8058 | `		/* PHL has no abstract properties: the modifier only exists on an` |
|      - | 8059 | `		 * interface's hooked property stub, which PHL does not model. */` |
|      - | 8060 | `		{ "isAbstract",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_false },` |
|      - | 8061 | `		{ "isVirtual",   PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isVirtual },` |
|      - | 8062 | `		{ "isPromoted",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isPromoted },` |
|      - | 8063 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionProperty_getModifiers },` |
|      - | 8064 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|      - | 8065 | `		  vm_builtin_ReflectionProperty_getDeclaringClass },` |
|      - | 8066 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionProperty_getDocComment },` |
|      - | 8067 | `		{ "setAccessible", PH7_MOD_PUBLIC, "bool $accessible", "@void",` |
|      - | 8068 | `		  vm_builtin_ReflectionProperty_setAccessible },` |
|      - | 8069 | `		{ "getType",     PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionProperty_getType },` |
|      - | 8070 | `		/* php's settable type differs from the declared one only for a hooked` |
|      - | 8071 | ``		 * property with a widening `set` — which PHL does not model. */`` |
|      - | 8072 | `		{ "getSettableType", PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|      - | 8073 | `		  vm_builtin_ReflectionProperty_getType },` |
|      - | 8074 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_hasType },` |
|      - | 8075 | `		{ "hasDefaultValue", PH7_MOD_PUBLIC, "", "bool",` |
|      - | 8076 | `		  vm_builtin_ReflectionProperty_hasDefaultValue },` |
|      - | 8077 | `		{ "getDefaultValue", PH7_MOD_PUBLIC, "", "@mixed",` |
|      - | 8078 | `		  vm_builtin_ReflectionProperty_getDefaultValue },` |
|      - | 8079 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - | 8080 | `		  vm_builtin_ReflectionProperty_getAttributes },` |
|      - | 8081 | `		{ "hasHooks",    PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_hasHooks },` |
|      - | 8082 | `		{ "getHooks",    PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionProperty_getHooks },` |
|      - | 8083 | `		{ "hasHook",     PH7_MOD_PUBLIC, "PropertyHookType $type", "bool",` |
|      - | 8084 | `		  vm_builtin_ReflectionProperty_hasHook },` |
|      - | 8085 | `		{ "getHook",     PH7_MOD_PUBLIC, "PropertyHookType $type", "?ReflectionMethod",` |
|      - | 8086 | `		  vm_builtin_ReflectionProperty_getHook },` |
|      - | 8087 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_false },` |
|      - | 8088 | `	};` |
|      - | 8089 | `	static const PH7_NativeConstDef aConstConst[] = {` |
|      - | 8090 | `		{ "IS_PUBLIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,  0, 0.0 },` |
|      - | 8091 | `		{ "IS_PROTECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,  0, 0.0 },` |
|      - | 8092 | `		{ "IS_PRIVATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,  0, 0.0 },` |
|      - | 8093 | `		{ "IS_FINAL",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|      - | 8094 | `	};` |
|      - | 8095 | `	static const PH7_NativeMethodDef aConstMethod[] = {` |
|      - | 8096 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|      - | 8097 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|      - | 8098 | `		  vm_builtin_ReflectionClassConstant_construct },` |
|      - | 8099 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string",` |
|      - | 8100 | `		  vm_builtin_ReflectionClassConstant_toString },` |
|      - | 8101 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionProperty_getName },` |
|      - | 8102 | `		{ "getValue",    PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_ReflectionClassConstant_getValue },` |
|      - | 8103 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClassConstant_isPublic },` |
|      - | 8104 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClassConstant_isPrivate },` |
|      - | 8105 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool",` |
|      - | 8106 | `		  vm_builtin_ReflectionClassConstant_isProtected },` |
|      - | 8107 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClassConstant_isFinal },` |
|      - | 8108 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int",` |
|      - | 8109 | `		  vm_builtin_ReflectionClassConstant_getModifiers },` |
|      - | 8110 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|      - | 8111 | `		  vm_builtin_ReflectionClassConstant_getDeclaringClass },` |
|      - | 8112 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false",` |
|      - | 8113 | `		  vm_builtin_ReflectionClassConstant_getDocComment },` |
|      - | 8114 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|      - | 8115 | `		  vm_builtin_ReflectionClassConstant_getAttributes },` |
|      - | 8116 | `		{ "isEnumCase",  PH7_MOD_PUBLIC, "", "bool",` |
|      - | 8117 | `		  vm_builtin_ReflectionClassConstant_isEnumCase },` |
|      - | 8118 | `		{ "isDeprecated", PH7_MOD_PUBLIC, "", "bool",` |
|      - | 8119 | `		  vm_builtin_ReflectionClassConstant_isDeprecated },` |
|      - | 8120 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClassConstant_hasType },` |
|      - | 8121 | `		{ "getType",     PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|      - | 8122 | `		  vm_builtin_ReflectionClassConstant_getType },` |
|      - | 8123 | `	};` |
|      - | 8124 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 8125 | `		{ "ReflectionProperty", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 8126 | `		  aPropMethod, SX_ARRAYSIZE(aPropMethod),` |
|      - | 8127 | `		  aPropConst, SX_ARRAYSIZE(aPropConst),` |
|      - | 8128 | `		  aPropProp, SX_ARRAYSIZE(aPropProp), 0, 0, 0 },` |
|      - | 8129 | `		{ "ReflectionClassConstant", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 8130 | `		  aConstMethod, SX_ARRAYSIZE(aConstMethod),` |
|      - | 8131 | `		  aConstConst, SX_ARRAYSIZE(aConstConst),` |
|      - | 8132 | `		  aMemberProp, SX_ARRAYSIZE(aMemberProp), 0, 0, 0 },` |
|      - | 8133 | `	};` |
|   5259 | 8134 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 | 8135 | `}` |
|      - | 8136 | `/*` |
|      - | 8137 | ` * ---------------------------------------------------------------------------` |
|      - | 8138 | ` * The ReflectionEnum family — chunk 6, and the end of __phl_rcinfo.` |
|      - | 8139 | ` *` |
|      - | 8140 | `` * Three classes that are very nearly their parents: `ReflectionEnum` IS a`` |
|      - | 8141 | `` * `ReflectionClass` that refuses a non-enum, and `ReflectionEnumUnitCase` IS a`` |
|      - | 8142 | `` * `ReflectionClassConstant` that refuses a constant which is not a case. What`` |
|      - | 8143 | ` * is their own is the CASE list, which the engine already holds in declaration` |
|      - | 8144 | `` * order on `ph7_class::aEnumCases` — the chunk read a marshalled copy of it out`` |
|      - | 8145 | `` * of `__phl_rcinfo`, the descriptor builder that dies with this conversion.`` |
|      - | 8146 | ` * ---------------------------------------------------------------------------` |
|      - | 8147 | ` */` |
|      - | 8148 |  |
|      - | 8149 | `/* The enum case named zName, or NULL. A case is a class CONSTANT, so the match` |
|      - | 8150 | `` * is case-SENSITIVE: php's hasCase('hearts') is false for `case Hearts`. */`` |
|     26 | 8151 | `static ph7_class_attr * ReflectEnumCase(ph7_class *pClass, const char *zName, int nName)` |
|      1 | 8152 | `{` |
|     27 | 8153 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      - | 8154 | `	sxu32 n;` |
|     27 | 8155 | `	if( nName < 1 ){` |
|    ! 0 | 8156 | `		return 0;` |
|      - | 8157 | `	}` |
|     59 | 8158 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     44 | 8159 | `		if( (int)SyStringLength(&apCase[n]->sName) == nName` |
|     33 | 8160 | `		 && SyMemcmp(SyStringData(&apCase[n]->sName), zName, (sxu32)nName) == 0 ){` |
|     13 | 8161 | `			return apCase[n];` |
|      - | 8162 | `		}` |
|     17 | 8163 | `	}` |
|     15 | 8164 | `	return 0;` |
|     14 | 8165 | `}` |
|      - | 8166 | `/*` |
|      - | 8167 | ` * One case reflector for an already-validated case. php answers a BackedCase` |
|      - | 8168 | ` * for a backed enum and a UnitCase for a pure one, and both carry the same two` |
|      - | 8169 | ` * slots ReflectionClassConstant does — an enum cannot extend anything, so the` |
|      - | 8170 | ` * declaring class is always the enum itself.` |
|      - | 8171 | ` */` |
|     28 | 8172 | `static ph7_class_instance * ReflectEnumCaseNew(ph7_context *pCtx, ph7_class *pEnum,` |
|      - | 8173 | `	const SyString *pCase)` |
|      1 | 8174 | `{` |
|     29 | 8175 | `	ph7_vm *pVm = pCtx->pVm;` |
|     43 | 8176 | `	const char *zClass = pEnum->nEnumBacking` |
|     14 | 8177 | `		? "ReflectionEnumBackedCase" : "ReflectionEnumUnitCase";` |
|     29 | 8178 | `	ph7_class *pRC = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|     29 | 8179 | `	ph7_class_instance *pObj = pRC ? PH7_NewClassInstance(pVm, pRC) : 0;` |
|     29 | 8180 | `	if( pObj == 0 ){` |
|    ! 0 | 8181 | `		return 0;` |
|      - | 8182 | `	}` |
|     43 | 8183 | `	PH7_NativeSetAttrStr(pVm, pObj, "class", SyStringData(&pEnum->sName),` |
|     28 | 8184 | `		(int)SyStringLength(&pEnum->sName));` |
|     29 | 8185 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(pCase), (int)SyStringLength(pCase));` |
|     29 | 8186 | `	return pObj;` |
|     15 | 8187 | `}` |
|      - | 8188 | `/* ReflectionEnum::__construct(object\|string $objectOrClass) */` |
|     24 | 8189 | `static int vm_builtin_ReflectionEnum_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8190 | `{` |
|      - | 8191 | `	ph7_class *pClass;` |
|     25 | 8192 | `	sxi32 rc = vm_builtin_ReflectionClass_construct(pCtx, nArg, apArg);` |
|     25 | 8193 | `	if( rc != PH7_OK ){` |
|      5 | 8194 | `		return rc; /* "Class %s does not exist", already php's */` |
|      - | 8195 | `	}` |
|     21 | 8196 | `	pClass = ReflectClassOf(pCtx);` |
|     21 | 8197 | `	if( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|     10 | 8198 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      3 | 8199 | `			"Class \"%z\" is not an enum", &pClass->sName);` |
|      - | 8200 | `	}` |
|     15 | 8201 | `	return PH7_OK;` |
|     13 | 8202 | `}` |
|     12 | 8203 | `static int vm_builtin_ReflectionEnum_hasCase(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8204 | `{` |
|     13 | 8205 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 8206 | `	const char *zName;` |
|      - | 8207 | `	int nName;` |
|     13 | 8208 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 8209 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 | 8210 | `		return PH7_OK;` |
|      - | 8211 | `	}` |
|     13 | 8212 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     13 | 8213 | `	ph7_result_bool(pCtx, ReflectEnumCase(pClass, zName, nName) != 0);` |
|     13 | 8214 | `	return PH7_OK;` |
|      7 | 8215 | `}` |
|      - | 8216 | `/*` |
|      - | 8217 | ` * getCase(): php tells the two failures apart — a name that is a constant but` |
|      - | 8218 | ` * not a case is "X::K is not a case", one that is neither is "Case X::K does` |
|      - | 8219 | ` * not exist". The chunk answered the second for both.` |
|      - | 8220 | ` */` |
|     14 | 8221 | `static int vm_builtin_ReflectionEnum_getCase(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8222 | `{` |
|     15 | 8223 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 8224 | `	ph7_class_attr *pCase;` |
|      - | 8225 | `	const char *zName;` |
|      - | 8226 | `	int nName;` |
|     15 | 8227 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|    ! 0 | 8228 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8229 | `		return PH7_OK;` |
|      - | 8230 | `	}` |
|     15 | 8231 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     15 | 8232 | `	pCase = ReflectEnumCase(pClass, zName, nName);` |
|     15 | 8233 | `	if( pCase == 0 ){` |
|      7 | 8234 | `		if( nName > 0 && SyHashGet(&pClass->hConst, (const void *)zName, (sxu32)nName) ){` |
|      4 | 8235 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      1 | 8236 | `				"%z::%.*s is not a case", &pClass->sName, nName, zName);` |
|      - | 8237 | `		}` |
|      7 | 8238 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 | 8239 | `			"Case %z::%.*s does not exist", &pClass->sName, nName, zName);` |
|      - | 8240 | `	}` |
|      9 | 8241 | `	return ReflectResultObject(pCtx, ReflectEnumCaseNew(pCtx, pClass, &pCase->sName));` |
|      8 | 8242 | `}` |
|     12 | 8243 | `static int vm_builtin_ReflectionEnum_getCases(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8244 | `{` |
|     13 | 8245 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     13 | 8246 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|      6 | 8247 | `	SXUNUSED(nArg);` |
|      6 | 8248 | `	SXUNUSED(apArg);` |
|     13 | 8249 | `	if( pOut == 0 ){` |
|    ! 0 | 8250 | `		return PH7_ContextMemoryError(pCtx);` |
|      - | 8251 | `	}` |
|     13 | 8252 | `	if( pClass ){` |
|     13 | 8253 | `		ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      - | 8254 | `		sxu32 n;` |
|     33 | 8255 | `		for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     21 | 8256 | `			ReflectTypeListAdd(pCtx, pOut, ReflectEnumCaseNew(pCtx, pClass, &apCase[n]->sName));` |
|     11 | 8257 | `		}` |
|      6 | 8258 | `	}` |
|     13 | 8259 | `	ph7_result_value(pCtx, pOut);` |
|     13 | 8260 | `	return PH7_OK;` |
|      7 | 8261 | `}` |
|     10 | 8262 | `static int vm_builtin_ReflectionEnum_isBacked(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8263 | `{` |
|     11 | 8264 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      5 | 8265 | `	SXUNUSED(nArg);` |
|      5 | 8266 | `	SXUNUSED(apArg);` |
|     11 | 8267 | `	ph7_result_bool(pCtx, pClass != 0 && pClass->nEnumBacking != 0);` |
|     11 | 8268 | `	return PH7_OK;` |
|      1 | 8269 | `}` |
|     16 | 8270 | `static int vm_builtin_ReflectionEnum_getBackingType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8271 | `{` |
|     17 | 8272 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 8273 | `	const char *zText;` |
|      - | 8274 | `	ph7_class_instance *pType;` |
|      8 | 8275 | `	SXUNUSED(nArg);` |
|      8 | 8276 | `	SXUNUSED(apArg);` |
|     17 | 8277 | `	if( pClass == 0 \|\| pClass->nEnumBacking == 0 ){` |
|      5 | 8278 | `		ph7_result_null(pCtx); /* a PURE enum has no backing type */` |
|      5 | 8279 | `		return PH7_OK;` |
|      - | 8280 | `	}` |
|     13 | 8281 | `	zText = (pClass->nEnumBacking & MEMOBJ_INT) ? "int" : "string";` |
|     13 | 8282 | `	pType = ReflectMakeType(pCtx, zText, (int)SyStrlen(zText));` |
|     13 | 8283 | `	if( pType == 0 ){` |
|    ! 0 | 8284 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8285 | `		return PH7_OK;` |
|      - | 8286 | `	}` |
|     13 | 8287 | `	PH7_NativeResultObject(pCtx, pType);` |
|     13 | 8288 | `	return PH7_OK;` |
|      9 | 8289 | `}` |
|      - | 8290 | `/*` |
|      - | 8291 | ` * ReflectionEnumUnitCase::__construct(object\|string $class, string $constant).` |
|      - | 8292 | ` *` |
|      - | 8293 | ` * The parent raises for a constant that does not exist; what is left is "not a` |
|      - | 8294 | ` * case", and php words that one WITHOUT asking whether the class is an enum at` |
|      - | 8295 | `` * all — `new ReflectionEnumUnitCase('Plain', 'K')` on an ordinary class says`` |
|      - | 8296 | `` * `Constant Plain::K is not a case`, not "is not an enum" as the chunk did.`` |
|      - | 8297 | ` * A non-enum's constant can never carry the ENUMCASE bit, so the one screen` |
|      - | 8298 | ` * answers both shapes.` |
|      - | 8299 | ` */` |
|     18 | 8300 | `static int vm_builtin_ReflectionEnumCase_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8301 | `{` |
|      - | 8302 | `	ReflectMemberRef sRef;` |
|     19 | 8303 | `	sxi32 rc = vm_builtin_ReflectionClassConstant_construct(pCtx, nArg, apArg);` |
|     19 | 8304 | `	if( rc != PH7_OK ){` |
|      3 | 8305 | `		return rc;` |
|      - | 8306 | `	}` |
|     17 | 8307 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 8308 | `		return PH7_OK;` |
|      - | 8309 | `	}` |
|     17 | 8310 | `	if( (sRef.pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){` |
|     10 | 8311 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      6 | 8312 | `			"Constant %z::%.*s is not a case", &sRef.pClass->sName, sRef.nName, sRef.zName);` |
|      - | 8313 | `	}` |
|     11 | 8314 | `	return PH7_OK;` |
|     10 | 8315 | `}` |
|      - | 8316 | `/* ReflectionEnumBackedCase::__construct() — the same, plus php's screen for a` |
|      - | 8317 | ` * PURE enum's case, which has no backing value to answer with. */` |
|      6 | 8318 | `static int vm_builtin_ReflectionEnumBackedCase_construct(ph7_context *pCtx, int nArg,` |
|      - | 8319 | `	ph7_value **apArg)` |
|      1 | 8320 | `{` |
|      - | 8321 | `	ReflectMemberRef sRef;` |
|      7 | 8322 | `	sxi32 rc = vm_builtin_ReflectionEnumCase_construct(pCtx, nArg, apArg);` |
|      7 | 8323 | `	if( rc != PH7_OK ){` |
|    ! 0 | 8324 | `		return rc;` |
|      - | 8325 | `	}` |
|      7 | 8326 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 8327 | `		return PH7_OK;` |
|      - | 8328 | `	}` |
|      7 | 8329 | `	if( sRef.pClass->nEnumBacking == 0 ){` |
|      4 | 8330 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|      2 | 8331 | `			"Enum case %z::%.*s is not a backed case", &sRef.pClass->sName,` |
|      1 | 8332 | `			sRef.nName, sRef.zName);` |
|      - | 8333 | `	}` |
|      5 | 8334 | `	return PH7_OK;` |
|      4 | 8335 | `}` |
|      6 | 8336 | `static int vm_builtin_ReflectionEnumCase_getEnum(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 8337 | `{` |
|      7 | 8338 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 8339 | `	ReflectMemberRef sRef;` |
|      - | 8340 | `	ph7_class *pRC;` |
|      - | 8341 | `	ph7_class_instance *pObj;` |
|      3 | 8342 | `	SXUNUSED(nArg);` |
|      3 | 8343 | `	SXUNUSED(apArg);` |
|      7 | 8344 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) ){` |
|    ! 0 | 8345 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8346 | `		return PH7_OK;` |
|      - | 8347 | `	}` |
|      7 | 8348 | `	pRC = PH7_VmExtractClass(pVm, "ReflectionEnum", sizeof("ReflectionEnum")-1, FALSE, 0);` |
|      7 | 8349 | `	pObj = pRC ? PH7_NewClassInstance(pVm, pRC) : 0;` |
|      7 | 8350 | `	if( pObj == 0 ){` |
|    ! 0 | 8351 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8352 | `		return PH7_OK;` |
|      - | 8353 | `	}` |
|     10 | 8354 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(&sRef.pClass->sName),` |
|      6 | 8355 | `		(int)SyStringLength(&sRef.pClass->sName));` |
|      7 | 8356 | `	return ReflectResultObject(pCtx, pObj);` |
|      4 | 8357 | `}` |
|      - | 8358 | ``/* getBackingValue(): the `value` the case singleton carries. The singleton is`` |
|      - | 8359 | ` * the constant's own value, so the parent's accessor materializes it. */` |
|     16 | 8360 | `static int vm_builtin_ReflectionEnumCase_getBackingValue(ph7_context *pCtx, int nArg,` |
|      - | 8361 | `	ph7_value **apArg)` |
|      1 | 8362 | `{` |
|      - | 8363 | `	ReflectMemberRef sRef;` |
|      - | 8364 | `	ph7_value *pVal, *pBacking;` |
|      - | 8365 | `	sxi32 rc;` |
|      8 | 8366 | `	SXUNUSED(nArg);` |
|      8 | 8367 | `	SXUNUSED(apArg);` |
|     17 | 8368 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 8369 | `		ph7_result_null(pCtx);` |
|    ! 0 | 8370 | `		return PH7_OK;` |
|      - | 8371 | `	}` |
|     17 | 8372 | `	rc = ReflectConstSlot(pCtx, sRef.pClass, sRef.pAttr, &pVal);` |
|     17 | 8373 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 8374 | `		return rc;` |
|      - | 8375 | `	}` |
|     17 | 8376 | `	pBacking = (pVal && (pVal->iFlags & MEMOBJ_OBJ))` |
|     24 | 8377 | `		? PH7_NativeAttr((ph7_class_instance *)pVal->x.pOther, "value") : 0;` |
|     17 | 8378 | `	if( pBacking ){` |
|     17 | 8379 | `		ph7_result_value(pCtx, pBacking);` |
|      9 | 8380 | `	}else{` |
|    ! 0 | 8381 | `		ph7_result_null(pCtx);` |
|      - | 8382 | `	}` |
|     17 | 8383 | `	return PH7_OK;` |
|      9 | 8384 | `}` |
|      - | 8385 | `/*` |
|      - | 8386 | ` * Declare the three. Called from PH7_VmInstallReflection() where chunk 6 used` |
|      - | 8387 | ` * to be compiled, so both parents are installed (PH7_ClassInherit COPIES a` |
|      - | 8388 | ` * base's methods down — a native subclass needs its parent declared first).` |
|      - | 8389 | ` *` |
|      - | 8390 | ` * The uncloneable/unserializable flags are php's answer for each class and do` |
|      - | 8391 | ``  * NOT come down with the inheritance: without them `clone $enumReflector` `` |
|      - | 8392 | ` * reached the private __clone it inherited and reported that instead.` |
|      - | 8393 | ` */` |
|   5254 | 8394 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionEnum(ph7_vm *pVm)` |
|      5 | 8395 | `{` |
|      - | 8396 | `	static const PH7_NativeMethodDef aEnumMethod[] = {` |
|      - | 8397 | `		{ "__construct",    PH7_MOD_PUBLIC, "object\|string $objectOrClass", "",` |
|      - | 8398 | `		  vm_builtin_ReflectionEnum_construct },` |
|      - | 8399 | `		{ "hasCase",        PH7_MOD_PUBLIC, "string $name", "bool",` |
|      - | 8400 | `		  vm_builtin_ReflectionEnum_hasCase },` |
|      - | 8401 | `		{ "getCase",        PH7_MOD_PUBLIC, "string $name", "ReflectionEnumUnitCase",` |
|      - | 8402 | `		  vm_builtin_ReflectionEnum_getCase },` |
|      - | 8403 | `		{ "getCases",       PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionEnum_getCases },` |
|      - | 8404 | `		{ "isBacked",       PH7_MOD_PUBLIC, "", "bool",  vm_builtin_ReflectionEnum_isBacked },` |
|      - | 8405 | `		{ "getBackingType", PH7_MOD_PUBLIC, "", "?ReflectionNamedType",` |
|      - | 8406 | `		  vm_builtin_ReflectionEnum_getBackingType },` |
|      - | 8407 | `	};` |
|      - | 8408 | `	static const PH7_NativeMethodDef aCaseMethod[] = {` |
|      - | 8409 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|      - | 8410 | `		  vm_builtin_ReflectionEnumCase_construct },` |
|      - | 8411 | `		{ "getEnum",     PH7_MOD_PUBLIC, "", "ReflectionEnum",` |
|      - | 8412 | `		  vm_builtin_ReflectionEnumCase_getEnum },` |
|      - | 8413 | `		/* php redeclares getValue() on the case reflector for its narrower` |
|      - | 8414 | `		 * return type; the body is the parent's. */` |
|      - | 8415 | `		{ "getValue",    PH7_MOD_PUBLIC, "", "UnitEnum",` |
|      - | 8416 | `		  vm_builtin_ReflectionClassConstant_getValue },` |
|      - | 8417 | `	};` |
|      - | 8418 | `	static const PH7_NativeMethodDef aBackedMethod[] = {` |
|      - | 8419 | `		{ "__construct",     PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|      - | 8420 | `		  vm_builtin_ReflectionEnumBackedCase_construct },` |
|      - | 8421 | `		{ "getBackingValue", PH7_MOD_PUBLIC, "", "string\|int",` |
|      - | 8422 | `		  vm_builtin_ReflectionEnumCase_getBackingValue },` |
|      - | 8423 | `	};` |
|      - | 8424 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|      - | 8425 | `		{ "ReflectionEnum", "ReflectionClass", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 8426 | `		  aEnumMethod, SX_ARRAYSIZE(aEnumMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 8427 | `		{ "ReflectionEnumUnitCase", "ReflectionClassConstant", 0,` |
|      - | 8428 | `		  PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 8429 | `		  aCaseMethod, SX_ARRAYSIZE(aCaseMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 8430 | `		{ "ReflectionEnumBackedCase", "ReflectionEnumUnitCase", 0,` |
|      - | 8431 | `		  PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|      - | 8432 | `		  aBackedMethod, SX_ARRAYSIZE(aBackedMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|      - | 8433 | `	};` |
|   5259 | 8434 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|      5 | 8435 | `}` |
|      - | 8436 | `/*` |
|      - | 8437 | ` * ---------------------------------------------------------------------------` |
|      - | 8438 | ` * php's Reflection EXPORT format — chunk 9, the last of the library's PHP.` |
|      - | 8439 | ` *` |
|      - | 8440 | ` * Every Reflector's __toString(). It is a byte-exact format with no API of its` |
|      - | 8441 | ` * own, so the chunk was written against the PUBLIC reflection API of whatever` |
|      - | 8442 | ` * it was printing — the only thing PHP could reach. All of those targets are C` |
|      - | 8443 | ` * now, so this reads the engine directly: the member walk for order and` |
|      - | 8444 | ` * visibility, ReflectParamAt for parameters, ReflectExportValue (built for the` |
|      - | 8445 | ` * attribute-argument block) for every value.` |
|      - | 8446 | ` *` |
|      - | 8447 | ` * Defined at the END of the file because it needs all of that; the five entry` |
|      - | 8448 | ` * points are forward-declared above their callers.` |
|      - | 8449 | ` * ---------------------------------------------------------------------------` |
|      - | 8450 | ` */` |
|      - | 8451 |  |
|      - | 8452 | `/* "internal:Core" / "user" — the first tag of every function and class head. */` |
|    156 | 8453 | `static void ReflectExportKind(SyBlob *pOut, int bInternal)` |
|      2 | 8454 | `{` |
|    158 | 8455 | `	if( bInternal ){` |
|     88 | 8456 | `		SyBlobAppend(pOut, "internal:Core", sizeof("internal:Core")-1);` |
|     45 | 8457 | `	}else{` |
|     71 | 8458 | `		SyBlobAppend(pOut, "user", sizeof("user")-1);` |
|      - | 8459 | `	}` |
|    158 | 8460 | `}` |
|      - | 8461 | `/* A declared type followed by a space, or nothing at all. */` |
|    174 | 8462 | `static void ReflectExportTypeSp(SyBlob *pOut, const SyString *pType)` |
|      2 | 8463 | `{` |
|    176 | 8464 | `	if( pType && SyStringLength(pType) > 0 ){` |
|    140 | 8465 | `		SyBlobAppend(pOut, SyStringData(pType), SyStringLength(pType));` |
|    140 | 8466 | `		SyBlobAppend(pOut, " ", sizeof(char));` |
|     69 | 8467 | `	}` |
|    176 | 8468 | `}` |
|      - | 8469 | `/* php's visibility word for a member. */` |
|    208 | 8470 | `static const char * ReflectExportVis(sxi32 iProtection)` |
|      1 | 8471 | `{` |
|    209 | 8472 | `	if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|     11 | 8473 | `		return "private";` |
|      - | 8474 | `	}` |
|    199 | 8475 | `	return iProtection == PH7_CLASS_PROT_PROTECTED ? "protected" : "public";` |
|    105 | 8476 | `}` |
|      - | 8477 | `/*` |
|      - | 8478 | `` * The text after `= ` in a parameter default.`` |
|      - | 8479 | ` *` |
|      - | 8480 | ` * php prints a constant-reference default as the CONSTANT's name, not its value` |
|      - | 8481 | `` * (`$f = M_PI`) — that argument was never folded, so php still has the source`` |
|      - | 8482 | `` * expression. `<default>` is what the chunk answered when the value could not`` |
|      - | 8483 | `` * be produced at all, which is a declared `= ?` row in aBuiltinSig[].`` |
|      - | 8484 | ` */` |
|     60 | 8485 | `static void ReflectExportDefault(ph7_context *pCtx, SyBlob *pOut, ReflectParamDesc *pDesc)` |
|      1 | 8486 | `{` |
|     61 | 8487 | `	const char *z = 0;` |
|     61 | 8488 | `	int n = 0;` |
|     61 | 8489 | `	if( ReflectParamDefConst(pCtx, pDesc, &z, &n) ){` |
|      7 | 8490 | `		SyBlobAppend(pOut, z, (sxu32)n);` |
|     34 | 8491 | `		return;` |
|      - | 8492 | `	}` |
|     55 | 8493 | `	if( pDesc->pArg ){` |
|      - | 8494 | `		ph7_value sValue;` |
|     39 | 8495 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     39 | 8496 | `		VmLocalExec(pCtx->pVm, &pDesc->pArg->aByteCode, &sValue, FALSE);` |
|     39 | 8497 | `		ReflectExportValue(pCtx, pOut, &sValue, 0);` |
|     39 | 8498 | `		PH7_MemObjRelease(&sValue);` |
|     39 | 8499 | `		return;` |
|      - | 8500 | `	}` |
|      - | 8501 | `	{` |
|      - | 8502 | `		/* A DECLARED default is signature TEXT: reduce it the way` |
|      - | 8503 | `		 * getDefaultValue() does, and fall back to php's own placeholder.` |
|      - | 8504 | `		 * Only an INTERNAL parameter reaches this branch (a compiled one carries` |
|      - | 8505 | `		 * pArg above), which is exactly the case php prints DOUBLE-quoted. */` |
|     17 | 8506 | `		const char *zDef = SyStringData(&pDesc->sDefText);` |
|     17 | 8507 | `		int nDef = (int)SyStringLength(&pDesc->sDefText);` |
|     17 | 8508 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|     17 | 8509 | `		if( ReflectSigHas(zDef, nDef, "::", 2) ){` |
|      - | 8510 | `			/* Rule 28's split, on the declared side: php's stub keeps the SOURCE of a` |
|      - | 8511 | `			 * constant-expression default, so the export prints` |
|      - | 8512 | ``			 * `= SplFileInfo::class` and `= A::K \| A::J` verbatim while`` |
|      - | 8513 | `			 * getDefaultValue() answers what they evaluate to. */` |
|      3 | 8514 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|      3 | 8515 | `			return;` |
|      - | 8516 | `		}` |
|     15 | 8517 | `		if( pVal && ReflectSigGlobalConst(pCtx, zDef, nDef, pVal) ){` |
|      - | 8518 | ``			/* Same split for a GLOBAL constant: `= E_ERROR`, never `= 1`. */`` |
|    ! 0 | 8519 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|    ! 0 | 8520 | `			return;` |
|      - | 8521 | `		}` |
|     15 | 8522 | `		if( pVal && ReflectSigScalar(pCtx, zDef, nDef, pVal) ){` |
|     15 | 8523 | `			if( (pVal->iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING ){` |
|     16 | 8524 | `				ReflectExportStrQ(pOut, (const char *)SyBlobData(&pVal->sBlob),` |
|      5 | 8525 | `					SyBlobLength(&pVal->sBlob), '"');` |
|     11 | 8526 | `				return;` |
|      - | 8527 | `			}` |
|      5 | 8528 | `			if( pVal->iFlags & MEMOBJ_NULL ){` |
|      - | 8529 | `				/* The same internal/user split as the quote character above: php` |
|      - | 8530 | ``				 * spells an INTERNAL parameter's null default `null` and a`` |
|      - | 8531 | ``				 * userland one `NULL` (`= null` for every `?T $x = null` stub row). */`` |
|      5 | 8532 | `				SyBlobAppend(pOut, "null", sizeof("null")-1);` |
|      5 | 8533 | `				return;` |
|      - | 8534 | `			}` |
|    ! 0 | 8535 | `			ReflectExportValue(pCtx, pOut, pVal, 0);` |
|    ! 0 | 8536 | `			return;` |
|      - | 8537 | `		}` |
|    ! 0 | 8538 | `		if( (nDef >= 1 && zDef[0] == '[')` |
|    ! 0 | 8539 | `		 \|\| (nDef >= (int)sizeof("array (")-1` |
|    ! 0 | 8540 | `		  && SyMemcmp(zDef, "array (", sizeof("array (")-1) == 0) ){` |
|    ! 0 | 8541 | `			SyBlobAppend(pOut, "[]", sizeof("[]")-1);` |
|    ! 0 | 8542 | `			return;` |
|      - | 8543 | `		}` |
|      - | 8544 | `	}` |
|    ! 0 | 8545 | `	SyBlobAppend(pOut, "<default>", sizeof("<default>")-1);` |
|     31 | 8546 | `}` |
|      - | 8547 | ``/* `Parameter #0 [ <optional> int &...$name = 5 ]` */`` |
|    126 | 8548 | `static void ReflectExportParamLine(ph7_context *pCtx, SyBlob *pOut, ReflectParamDesc *pDesc)` |
|      2 | 8549 | `{` |
|    191 | 8550 | `	SyBlobFormat(pOut, "Parameter #%d [ <%s> ", pDesc->iPos,` |
|    126 | 8551 | `		pDesc->bOptional ? "optional" : "required");` |
|    128 | 8552 | `	ReflectExportTypeSp(pOut, &pDesc->sType);` |
|    128 | 8553 | `	if( pDesc->bByRef ){` |
|     11 | 8554 | `		SyBlobAppend(pOut, "&", sizeof(char));` |
|      5 | 8555 | `	}` |
|    128 | 8556 | `	if( pDesc->bVariadic ){` |
|     13 | 8557 | `		SyBlobAppend(pOut, "...", sizeof("...")-1);` |
|      6 | 8558 | `	}` |
|    128 | 8559 | `	SyBlobAppend(pOut, "$", sizeof(char));` |
|    128 | 8560 | `	SyBlobAppend(pOut, SyStringData(&pDesc->sName), SyStringLength(&pDesc->sName));` |
|    128 | 8561 | `	if( pDesc->bHasDef ){` |
|     61 | 8562 | `		SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|     61 | 8563 | `		ReflectExportDefault(pCtx, pOut, pDesc);` |
|     30 | 8564 | `	}` |
|    128 | 8565 | `	SyBlobAppend(pOut, " ]", sizeof(" ]")-1);` |
|    128 | 8566 | `}` |
|      - | 8567 | `/*` |
|      - | 8568 | `` * `Property [ public protected(set) readonly int $x = 5 ]` + newline.`` |
|      - | 8569 | ` *` |
|      - | 8570 | ` * The set-visibility is printed only when it DIFFERS from the get-visibility,` |
|      - | 8571 | `` * which is why a `public readonly` property shows php's implied`` |
|      - | 8572 | `` * `protected(set)` and a `protected readonly` one shows nothing.`` |
|      - | 8573 | ` */` |
|     58 | 8574 | `static void ReflectExportPropLine(ph7_context *pCtx, SyBlob *pOut, ph7_class_attr *pAttr,` |
|      - | 8575 | `	const SyString *pKey)` |
|      1 | 8576 | `{` |
|     59 | 8577 | `	sxi32 iSet = pAttr->iProtection;` |
|     59 | 8578 | `	SyBlobAppend(pOut, "Property [ ", sizeof("Property [ ")-1);` |
|      - | 8579 | `	/* php's modifier MASK implies two bits the declaration never wrote:` |
|      - | 8580 | `	 * private(set) implies final, and readonly implies protected(set). */` |
|     59 | 8581 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|      3 | 8582 | `		SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|      1 | 8583 | `	}` |
|     59 | 8584 | `	SyBlobFormat(pOut, "%s ", ReflectExportVis(pAttr->iProtection));` |
|     59 | 8585 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|      3 | 8586 | `		iSet = PH7_CLASS_PROT_PRIVATE;` |
|     58 | 8587 | `	}else if( (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET)` |
|     57 | 8588 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){` |
|     13 | 8589 | `		iSet = PH7_CLASS_PROT_PROTECTED;` |
|      6 | 8590 | `	}` |
|      - | 8591 | `	/* The set-visibility is printed only when it DIFFERS from the get one. */` |
|     59 | 8592 | `	if( iSet != pAttr->iProtection ){` |
|     11 | 8593 | `		SyBlobFormat(pOut, "%s(set) ", ReflectExportVis(iSet));` |
|      5 | 8594 | `	}` |
|     59 | 8595 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|      7 | 8596 | `		SyBlobAppend(pOut, "static ", sizeof("static ")-1);` |
|      3 | 8597 | `	}` |
|     59 | 8598 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|    ! 0 | 8599 | `		SyBlobAppend(pOut, "virtual ", sizeof("virtual ")-1);` |
|    ! 0 | 8600 | `	}` |
|     59 | 8601 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|     13 | 8602 | `		SyBlobAppend(pOut, "readonly ", sizeof("readonly ")-1);` |
|      6 | 8603 | `	}` |
|     59 | 8604 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|     49 | 8605 | `		ReflectExportTypeSp(pOut, &pAttr->sTypeName);` |
|     24 | 8606 | `	}` |
|     59 | 8607 | `	SyBlobAppend(pOut, "$", sizeof(char));` |
|     59 | 8608 | `	SyBlobAppend(pOut, SyStringData(pKey), SyStringLength(pKey));` |
|     58 | 8609 | `	if( SySetUsed(&pAttr->aByteCode) > 0 \|\| pAttr->pNativeValue` |
|     31 | 8610 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|      - | 8611 | `		ph7_value sValue;` |
|     31 | 8612 | `		SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|     31 | 8613 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     31 | 8614 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|     29 | 8615 | `			VmLocalExec(pCtx->pVm, &pAttr->aByteCode, &sValue, FALSE);` |
|     17 | 8616 | `		}else if( pAttr->pNativeValue ){` |
|    ! 0 | 8617 | `			PH7_NativeLiteralValue(pCtx->pVm, pAttr->pNativeValue, &sValue);` |
|    ! 0 | 8618 | `		}` |
|     31 | 8619 | `		ReflectExportValue(pCtx, pOut, &sValue, 0);` |
|     31 | 8620 | `		PH7_MemObjRelease(&sValue);` |
|     15 | 8621 | `	}` |
|      - | 8622 | `	/* A hooked property names its hooks; php lists no method for them. */` |
|     59 | 8623 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET) ){` |
|      3 | 8624 | `		SyBlobAppend(pOut, " {", sizeof(" {")-1);` |
|      3 | 8625 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET ){` |
|      3 | 8626 | `			SyBlobAppend(pOut, " get;", sizeof(" get;")-1);` |
|      1 | 8627 | `		}` |
|      3 | 8628 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET ){` |
|      3 | 8629 | `			SyBlobAppend(pOut, " set;", sizeof(" set;")-1);` |
|      1 | 8630 | `		}` |
|      3 | 8631 | `		SyBlobAppend(pOut, " }", sizeof(" }")-1);` |
|      1 | 8632 | `	}` |
|     59 | 8633 | `	SyBlobAppend(pOut, " ]\n", sizeof(" ]\n")-1);` |
|     59 | 8634 | `}` |
|      - | 8635 | `/*` |
|      - | 8636 | `` * `Constant [ final public int NAME ] { value }` + newline. The TYPE is the`` |
|      - | 8637 | ` * value's, not a declared one, and an object (an enum case) prints as the word` |
|      - | 8638 | ` * Object under its own class name.` |
|      - | 8639 | ` */` |
|     24 | 8640 | `static sxi32 ReflectExportConstLine(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass,` |
|      - | 8641 | `	ph7_class_attr *pAttr, const SyString *pKey)` |
|      1 | 8642 | `{` |
|     25 | 8643 | `	ph7_value *pVal = 0;` |
|     25 | 8644 | `	sxi32 rc = ReflectConstSlot(pCtx, pClass, pAttr, &pVal);` |
|     25 | 8645 | `	const char *zType = "null";` |
|     25 | 8646 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 8647 | `		return rc;` |
|      - | 8648 | `	}` |
|     25 | 8649 | `	SyBlobAppend(pOut, "Constant [ ", sizeof("Constant [ ")-1);` |
|     25 | 8650 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      7 | 8651 | `		SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|      3 | 8652 | `	}` |
|     25 | 8653 | `	SyBlobFormat(pOut, "%s ", ReflectExportVis(pAttr->iProtection));` |
|     25 | 8654 | `	if( pVal ){` |
|     25 | 8655 | `		if( (pVal->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|      3 | 8656 | `			zType = 0; /* the object's own class */` |
|     24 | 8657 | `		}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|    ! 0 | 8658 | `			zType = "null";` |
|     23 | 8659 | `		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      3 | 8660 | `			zType = "array";` |
|     22 | 8661 | `		}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|    ! 0 | 8662 | `			zType = "bool";` |
|     21 | 8663 | `		}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|    ! 0 | 8664 | `			zType = "float";` |
|     21 | 8665 | `		}else if( pVal->iFlags & MEMOBJ_INT ){` |
|     13 | 8666 | `			zType = "int";` |
|     15 | 8667 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|      9 | 8668 | `			zType = "string";` |
|      4 | 8669 | `		}` |
|     12 | 8670 | `	}` |
|     25 | 8671 | `	if( zType ){` |
|     23 | 8672 | `		SyBlobFormat(pOut, "%s ", zType);` |
|     12 | 8673 | `	}else{` |
|      3 | 8674 | `		SyBlobFormat(pOut, "%z ", &((ph7_class_instance *)pVal->x.pOther)->pClass->sName);` |
|      - | 8675 | `	}` |
|     25 | 8676 | `	SyBlobAppend(pOut, SyStringData(pKey), SyStringLength(pKey));` |
|     25 | 8677 | `	SyBlobAppend(pOut, " ] { ", sizeof(" ] { ")-1);` |
|     25 | 8678 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|      - | 8679 | `		/* php prints nothing at all for null */` |
|     24 | 8680 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|      3 | 8681 | `		SyBlobAppend(pOut, "Array", sizeof("Array")-1);` |
|     24 | 8682 | `	}else if( (pVal->iFlags & MEMOBJ_OBJ) ){` |
|      3 | 8683 | `		SyBlobAppend(pOut, "Object", sizeof("Object")-1);` |
|     22 | 8684 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|    ! 0 | 8685 | `		if( pVal->x.iVal ){` |
|    ! 0 | 8686 | `			SyBlobAppend(pOut, "1", sizeof(char));` |
|    ! 0 | 8687 | `		}` |
|    ! 0 | 8688 | `	}else{` |
|      - | 8689 | `		ph7_value sTmp;` |
|      - | 8690 | `		const char *zText;` |
|      - | 8691 | `		int nText;` |
|     21 | 8692 | `		PH7_MemObjInit(pCtx->pVm, &sTmp);` |
|     21 | 8693 | `		PH7_MemObjStore(pVal, &sTmp);` |
|     21 | 8694 | `		zText = ph7_value_to_string(&sTmp, &nText);` |
|     21 | 8695 | `		if( nText > 0 ){` |
|     21 | 8696 | `			SyBlobAppend(pOut, zText, (sxu32)nText);` |
|     10 | 8697 | `		}` |
|     21 | 8698 | `		PH7_MemObjRelease(&sTmp);` |
|      - | 8699 | `	}` |
|     25 | 8700 | `	SyBlobAppend(pOut, " }\n", sizeof(" }\n")-1);` |
|     25 | 8701 | `	return SXRET_OK;` |
|     13 | 8702 | `}` |
|      - | 8703 | `/* A native class METHOD as a function reference (ReflectFuncFill's tail, for a` |
|      - | 8704 | ` * method the member walk handed over rather than one a receiver names). */` |
|     74 | 8705 | `static void ReflectFuncFromMethod(ph7_class *pClass, ph7_class_method *pMeth,` |
|      - | 8706 | `	ReflectFuncRef *pOut)` |
|      1 | 8707 | `{` |
|     75 | 8708 | `	SyZero(pOut, sizeof(*pOut));` |
|     75 | 8709 | `	pOut->pClass = pClass;` |
|     75 | 8710 | `	pOut->pMeth = pMeth;` |
|     75 | 8711 | `	pOut->pFunc = &pMeth->sFunc;` |
|     75 | 8712 | `	if( (pOut->pFunc->iFlags & VM_FUNC_NATIVE) && pOut->pFunc->pNative ){` |
|     37 | 8713 | `		pOut->zSig = pOut->pFunc->pNative->zSig;` |
|     37 | 8714 | `		if( SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|     37 | 8715 | `			pOut->zRet = pOut->pFunc->pNative->zRet;` |
|     18 | 8716 | `		}` |
|     18 | 8717 | `	}` |
|     75 | 8718 | `}` |
|      - | 8719 | `/*` |
|      - | 8720 | ` * The Method / Function / Closure block.` |
|      - | 8721 | ` *` |
|      - | 8722 | ` * pOwner is the class being EXPORTED when there is one: php's tags are` |
|      - | 8723 | ` * relative to it — a method it did not declare "inherits" from its declaring` |
|      - | 8724 | ` * class — and the reflector alone cannot say that, because $class is the` |
|      - | 8725 | `` * DECLARING class. `overwrites` is the other direction and needs no owner: the`` |
|      - | 8726 | ` * declaring class's own parent declares the same method.` |
|      - | 8727 | ` */` |
|    128 | 8728 | `static sxi32 ReflectExportFuncBlock(ph7_context *pCtx, SyBlob *pOut, ReflectFuncRef *pRef,` |
|      - | 8729 | `	const char *zIndent, ph7_class *pOwner)` |
|      2 | 8730 | `{` |
|      - | 8731 | `	SyBlob sBody;` |
|    130 | 8732 | `	int bInternal = ReflectFuncIsInternal(pRef);` |
|    130 | 8733 | `	int nParam = ReflectParamCount(pRef);` |
|    130 | 8734 | `	const char *zRet = 0;` |
|    130 | 8735 | `	int nRet = 0, bHasRet, bTentRet = 0;` |
|    130 | 8736 | `	sxi32 rc = SXRET_OK;` |
|    130 | 8737 | `	SyBlobInit(&sBody, &pCtx->pVm->sAllocator);` |
|    130 | 8738 | `	bHasRet = ReflectFuncRetTextEx(pRef, &zRet, &nRet, &bTentRet);` |
|    130 | 8739 | `	if( pRef->pMeth ){` |
|    117 | 8740 | `		ph7_class *pDecl = ReflectFuncDeclClass(pRef);` |
|      - | 8741 | `		ph7_class *pProto;` |
|    117 | 8742 | `		SyString *pName = &pRef->pMeth->sFunc.sName;` |
|      - | 8743 | `		int bInherits, bCtor;` |
|    117 | 8744 | `		SyBlobAppend(&sBody, "Method [ <", sizeof("Method [ <")-1);` |
|    117 | 8745 | `		ReflectExportKind(&sBody, bInternal);` |
|    117 | 8746 | `		bInherits = (pOwner != 0 && pDecl != 0 && pDecl != pOwner);` |
|    117 | 8747 | `		if( bInherits ){` |
|     19 | 8748 | `			SyBlobFormat(&sBody, ", inherits %z", &pDecl->sName);` |
|    108 | 8749 | `		}else if( pDecl ){` |
|    148 | 8750 | `			ph7_class *pOver = ReflectPrototypeIn(pCtx, pDecl,` |
|     98 | 8751 | `				SyStringData(pName), (int)SyStringLength(pName), 0);` |
|     99 | 8752 | `			if( pOver ){` |
|      9 | 8753 | `				SyBlobFormat(&sBody, ", overwrites %z", &pOver->sName);` |
|      4 | 8754 | `			}` |
|     49 | 8755 | `		}` |
|    182 | 8756 | `		bCtor = SyStringLength(pName) == sizeof("__construct")-1` |
|    116 | 8757 | `			&& SyStrnicmp(SyStringData(pName), "__construct", sizeof("__construct")-1) == 0;` |
|    117 | 8758 | `		if( bCtor ){` |
|      9 | 8759 | `			SyBlobAppend(&sBody, ", ctor", sizeof(", ctor")-1);` |
|    113 | 8760 | `		}else if( SyStringLength(pName) == sizeof("__destruct")-1` |
|     55 | 8761 | `		 && SyStrnicmp(SyStringData(pName), "__destruct", sizeof("__destruct")-1) == 0 ){` |
|    ! 0 | 8762 | `			SyBlobAppend(&sBody, ", dtor", sizeof(", dtor")-1);` |
|    ! 0 | 8763 | `		}` |
|      - | 8764 | `		/* The prototype belongs to the DECLARING class's own method — the` |
|      - | 8765 | `		 * abstract or interface declaration IT satisfies. Asked from the` |
|      - | 8766 | `		 * exported class instead, a plainly INHERITED method would claim its` |
|      - | 8767 | `` 		 * parent as a prototype, which php does not (it prints `inherits` `` |
|      - | 8768 | `		 * alone there, and both tags when the declarer really has one). */` |
|      - | 8769 | `		/* A CONSTRUCTOR never has one: zend excludes it from prototype` |
|      - | 8770 | `		 * inheritance (there is nothing to satisfy — a parent's ctor is not a` |
|      - | 8771 | ``		 * contract), so php prints `overwrites A, ctor` and stops. */`` |
|    171 | 8772 | `		pProto = bCtor ? 0 : ReflectPrototypeIn(pCtx, pDecl ? pDecl : pRef->pClass,` |
|    108 | 8773 | `			SyStringData(pName), (int)SyStringLength(pName), 1);` |
|    117 | 8774 | `		if( pProto ){` |
|     35 | 8775 | `			SyBlobFormat(&sBody, ", prototype %z", &pProto->sName);` |
|     17 | 8776 | `		}` |
|    117 | 8777 | `		SyBlobAppend(&sBody, "> ", sizeof("> ")-1);` |
|    117 | 8778 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|     31 | 8779 | `			SyBlobAppend(&sBody, "abstract ", sizeof("abstract ")-1);` |
|     15 | 8780 | `		}` |
|    117 | 8781 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|      5 | 8782 | `			SyBlobAppend(&sBody, "final ", sizeof("final ")-1);` |
|      2 | 8783 | `		}` |
|    117 | 8784 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|     21 | 8785 | `			SyBlobAppend(&sBody, "static ", sizeof("static ")-1);` |
|     10 | 8786 | `		}` |
|    117 | 8787 | `		SyBlobFormat(&sBody, "%s method %z ]", ReflectExportVis(pRef->pMeth->iProtection), pName);` |
|     59 | 8788 | `	}else{` |
|     20 | 8789 | `		SyBlobAppend(&sBody, pRef->pClosure ? "Closure [ <" : "Function [ <",` |
|     12 | 8790 | `			pRef->pClosure ? sizeof("Closure [ <")-1 : sizeof("Function [ <")-1);` |
|     14 | 8791 | `		ReflectExportKind(&sBody, bInternal);` |
|     14 | 8792 | `		SyBlobAppend(&sBody, "> function ", sizeof("> function ")-1);` |
|     14 | 8793 | `		if( pRef->pFunc ){` |
|      7 | 8794 | `			SyBlobFormat(&sBody, "%z", &pRef->pFunc->sName);` |
|     11 | 8795 | `		}else if( pRef->pHost ){` |
|      8 | 8796 | `			SyBlobFormat(&sBody, "%z", &pRef->pHost->sName);` |
|      3 | 8797 | `		}` |
|     14 | 8798 | `		SyBlobAppend(&sBody, " ]", sizeof(" ]")-1);` |
|      - | 8799 | `	}` |
|    130 | 8800 | `	SyBlobAppend(&sBody, " {\n", sizeof(" {\n")-1);` |
|    130 | 8801 | `	if( !bInternal && pRef->pFunc ){` |
|     51 | 8802 | `		SyBlobAppend(&sBody, "  @@ ", sizeof("  @@ ")-1);` |
|     51 | 8803 | `		if( SyStringLength(&pRef->pFunc->sFile) > 0 ){` |
|     76 | 8804 | `			SyBlobAppend(&sBody, SyStringData(&pRef->pFunc->sFile),` |
|     50 | 8805 | `				SyStringLength(&pRef->pFunc->sFile));` |
|     25 | 8806 | `		}` |
|     51 | 8807 | `		SyBlobFormat(&sBody, " %u - %u\n", pRef->pFunc->nLine, pRef->pFunc->nEndLine);` |
|     25 | 8808 | `	}` |
|      - | 8809 | `	/* php prints the parameter block for every INTERNAL function, and for a` |
|      - | 8810 | `	 * user one only when there is something to say. */` |
|    130 | 8811 | `	if( nParam > 0 \|\| bHasRet \|\| bInternal ){` |
|      - | 8812 | `		int n;` |
|    124 | 8813 | `		SyBlobFormat(&sBody, "\n  - Parameters [%d] {\n", nParam);` |
|    240 | 8814 | `		for( n = 0 ; n < nParam ; n++ ){` |
|      - | 8815 | `			ReflectParamDesc sDesc;` |
|    118 | 8816 | `			if( !ReflectParamAt(pRef, n, &sDesc) ){` |
|    ! 0 | 8817 | `				continue;` |
|      - | 8818 | `			}` |
|    118 | 8819 | `			SyBlobAppend(&sBody, "    ", sizeof("    ")-1);` |
|    118 | 8820 | `			ReflectExportParamLine(pCtx, &sBody, &sDesc);` |
|    118 | 8821 | `			SyBlobAppend(&sBody, "\n", sizeof(char));` |
|     60 | 8822 | `		}` |
|    124 | 8823 | `		SyBlobAppend(&sBody, "  }\n", sizeof("  }\n")-1);` |
|     61 | 8824 | `	}` |
|    130 | 8825 | `	if( bHasRet ){` |
|      - | 8826 | ``		/* php's two spellings: `Return [ T ]` for a declared type, `Tentative return`` |
|      - | 8827 | ``		 * [ T ]` for a stub's @tentative-return-type. */`` |
|    106 | 8828 | `		if( bTentRet ){` |
|     49 | 8829 | `			SyBlobAppend(&sBody, "  - Tentative return [ ", sizeof("  - Tentative return [ ")-1);` |
|     25 | 8830 | `		}else{` |
|     58 | 8831 | `			SyBlobAppend(&sBody, "  - Return [ ", sizeof("  - Return [ ")-1);` |
|      - | 8832 | `		}` |
|    106 | 8833 | `		SyBlobAppend(&sBody, zRet, (sxu32)nRet);` |
|    106 | 8834 | `		SyBlobAppend(&sBody, " ]\n", sizeof(" ]\n")-1);` |
|     52 | 8835 | `	}` |
|    130 | 8836 | `	SyBlobAppend(&sBody, "}\n", sizeof("}\n")-1);` |
|      - | 8837 | `	/* Indent every non-empty line, the way the chunk's explode/implode did. */` |
|    130 | 8838 | `	if( zIndent == 0 \|\| zIndent[0] == '\0' ){` |
|     56 | 8839 | `		SyBlobAppend(pOut, SyBlobData(&sBody), SyBlobLength(&sBody));` |
|     29 | 8840 | `	}else{` |
|     75 | 8841 | `		const char *z = (const char *)SyBlobData(&sBody);` |
|     75 | 8842 | `		sxu32 n = SyBlobLength(&sBody), i = 0, iStart = 0;` |
|     75 | 8843 | `		sxu32 nIndent = (sxu32)SyStrlen(zIndent);` |
|  14120 | 8844 | `		for( i = 0 ; i < n ; i++ ){` |
|  14046 | 8845 | `			if( z[i] != '\n' ){` |
|  13552 | 8846 | `				continue;` |
|      - | 8847 | `			}` |
|    495 | 8848 | `			if( i > iStart ){` |
|    427 | 8849 | `				SyBlobAppend(pOut, zIndent, nIndent);` |
|    427 | 8850 | `				SyBlobAppend(pOut, &z[iStart], i - iStart);` |
|    213 | 8851 | `			}` |
|    495 | 8852 | `			SyBlobAppend(pOut, "\n", sizeof(char));` |
|    495 | 8853 | `			iStart = i + 1;` |
|    248 | 8854 | `		}` |
|      - | 8855 | `	}` |
|    130 | 8856 | `	SyBlobRelease(&sBody);` |
|    130 | 8857 | `	return rc;` |
|      2 | 8858 | `}` |
|      - | 8859 | ``/* The `- Enum cases [N]` block php prints where an enum's cases would otherwise`` |
|      - | 8860 | ` * be listed among its constants. A backed case shows its value UNQUOTED. */` |
|      4 | 8861 | `static sxi32 ReflectExportEnumCases(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass)` |
|      1 | 8862 | `{` |
|      5 | 8863 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      - | 8864 | `	sxu32 n;` |
|      5 | 8865 | `	SyBlobFormat(pOut, "\n  - Enum cases [%u] {\n", SySetUsed(&pClass->aEnumCases));` |
|     11 | 8866 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|      7 | 8867 | `		SyBlobAppend(pOut, "    Case ", sizeof("    Case ")-1);` |
|      7 | 8868 | `		SyBlobAppend(pOut, SyStringData(&apCase[n]->sName), SyStringLength(&apCase[n]->sName));` |
|      7 | 8869 | `		if( pClass->nEnumBacking ){` |
|      5 | 8870 | `			ph7_value *pVal = 0;` |
|      - | 8871 | `			ph7_class_instance *pObj;` |
|      5 | 8872 | `			sxi32 rc = ReflectConstSlot(pCtx, pClass, apCase[n], &pVal);` |
|      5 | 8873 | `			if( rc != SXRET_OK ){` |
|    ! 0 | 8874 | `				return rc;` |
|      - | 8875 | `			}` |
|      5 | 8876 | `			pObj = (pVal && (pVal->iFlags & MEMOBJ_OBJ)) ? (ph7_class_instance *)pVal->x.pOther : 0;` |
|      5 | 8877 | `			if( pObj ){` |
|      5 | 8878 | `				ph7_value *pBacking = PH7_NativeAttr(pObj, "value");` |
|      5 | 8879 | `				if( pBacking ){` |
|      - | 8880 | `					ph7_value sTmp;` |
|      - | 8881 | `					const char *zText;` |
|      - | 8882 | `					int nText;` |
|      5 | 8883 | `					PH7_MemObjInit(pCtx->pVm, &sTmp);` |
|      5 | 8884 | `					PH7_MemObjStore(pBacking, &sTmp);` |
|      5 | 8885 | `					zText = ph7_value_to_string(&sTmp, &nText);` |
|      5 | 8886 | `					SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|      5 | 8887 | `					if( nText > 0 ){` |
|      5 | 8888 | `						SyBlobAppend(pOut, zText, (sxu32)nText);` |
|      2 | 8889 | `					}` |
|      5 | 8890 | `					PH7_MemObjRelease(&sTmp);` |
|      2 | 8891 | `				}` |
|      2 | 8892 | `			}` |
|      2 | 8893 | `		}` |
|      7 | 8894 | `		SyBlobAppend(pOut, "\n", sizeof(char));` |
|      4 | 8895 | `	}` |
|      5 | 8896 | `	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|      5 | 8897 | `	return SXRET_OK;` |
|      3 | 8898 | `}` |
|      - | 8899 | `/* The Class / Interface / Enum block. */` |
|     28 | 8900 | `static sxi32 ReflectExportClassBlock(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass)` |
|      1 | 8901 | `{` |
|     29 | 8902 | `	ph7_vm *pVm = pCtx->pVm;` |
|     29 | 8903 | `	int bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;` |
|     29 | 8904 | `	int bEnum = (pClass->iFlags & PH7_CLASS_ENUM) != 0;` |
|     29 | 8905 | `	int bIface = (pClass->iFlags & PH7_CLASS_INTERFACE) != 0;` |
|      - | 8906 | `	SySet aMembers, aIface;` |
|     29 | 8907 | `	sxu32 n, nConst = 0, nStaticProp = 0, nProp = 0, nStaticMeth = 0, nMeth = 0;` |
|     29 | 8908 | `	sxi32 rc = SXRET_OK;` |
|      - | 8909 | `	int iPass;` |
|     29 | 8910 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|     29 | 8911 | `	SySetInit(&aIface, &pVm->sAllocator, sizeof(ph7_class *));` |
|     29 | 8912 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|     29 | 8913 | `	ReflectInterfacesOf(pClass, &aIface);` |
|      - | 8914 | `	/* ---- head ---- */` |
|     29 | 8915 | `	if( bIface ){` |
|      9 | 8916 | `		SyBlobAppend(pOut, "Interface [ <", sizeof("Interface [ <")-1);` |
|     25 | 8917 | `	}else if( bEnum ){` |
|      5 | 8918 | `		SyBlobAppend(pOut, "Enum [ <", sizeof("Enum [ <")-1);` |
|      3 | 8919 | `	}else{` |
|     17 | 8920 | `		SyBlobAppend(pOut, "Class [ <", sizeof("Class [ <")-1);` |
|      - | 8921 | `	}` |
|     29 | 8922 | `	ReflectExportKind(pOut, bInternal);` |
|     29 | 8923 | `	SyBlobAppend(pOut, "> ", sizeof("> ")-1);` |
|      - | 8924 | `	/* php tags a class that has an iteration handler, which its Traversable` |
|      - | 8925 | `	 * implementers have — and so does anything with a HOOKED property, because` |
|      - | 8926 | `	 * that is how php 8.4 walks one. An INTERFACE never carries one: php` |
|      - | 8927 | `	 * installs the handler when a CLASS implements Traversable, so` |
|      - | 8928 | ``	 * `interface I extends Iterator` prints no tag. */`` |
|      - | 8929 | `	{` |
|     35 | 8930 | `		int bIterable = !bIface && pVm->pTraversableClass != 0` |
|     38 | 8931 | `			&& PH7_VmInstanceOf(pClass, pVm->pTraversableClass);` |
|    171 | 8932 | `		for( n = 0 ; !bIterable && n < SySetUsed(&aMembers) ; n++ ){` |
|    143 | 8933 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|    142 | 8934 | `			if( pM->iKind == REFLECT_MEMBER_PROP && pM->pAttr` |
|     55 | 8935 | `			 && (pM->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) ){` |
|      3 | 8936 | `				bIterable = 1;` |
|      1 | 8937 | `			}` |
|     72 | 8938 | `		}` |
|     29 | 8939 | `		if( bIterable ){` |
|      5 | 8940 | `			SyBlobAppend(pOut, "<iterateable> ", sizeof("<iterateable> ")-1);` |
|      2 | 8941 | `		}` |
|      - | 8942 | `	}` |
|     29 | 8943 | `	if( bIface ){` |
|      9 | 8944 | `		SyBlobFormat(pOut, "interface %z", &pClass->sName);` |
|     25 | 8945 | `	}else if( bEnum ){` |
|      5 | 8946 | `		SyBlobFormat(pOut, "enum %z", &pClass->sName);` |
|      5 | 8947 | `		if( pClass->nEnumBacking ){` |
|      3 | 8948 | `			SyBlobFormat(pOut, ": %s", (pClass->nEnumBacking & MEMOBJ_INT) ? "int" : "string");` |
|      1 | 8949 | `		}` |
|      3 | 8950 | `	}else{` |
|     17 | 8951 | `		if( pClass->iFlags & PH7_CLASS_ABSTRACT ){` |
|      3 | 8952 | `			SyBlobAppend(pOut, "abstract ", sizeof("abstract ")-1);` |
|      1 | 8953 | `		}` |
|     17 | 8954 | `		if( pClass->iFlags & PH7_CLASS_FINAL ){` |
|      3 | 8955 | `			SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|      1 | 8956 | `		}` |
|     17 | 8957 | `		if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|      3 | 8958 | `			SyBlobAppend(pOut, "readonly ", sizeof("readonly ")-1);` |
|      1 | 8959 | `		}` |
|     17 | 8960 | `		SyBlobFormat(pOut, "class %z", &pClass->sName);` |
|     17 | 8961 | `		if( pClass->pBase ){` |
|      5 | 8962 | `			SyBlobFormat(pOut, " extends %z", &pClass->pBase->sName);` |
|      2 | 8963 | `		}` |
|      - | 8964 | `	}` |
|     29 | 8965 | `	if( SySetUsed(&aIface) > 0 ){` |
|     17 | 8966 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIface);` |
|      - | 8967 | `		/* An interface EXTENDS what a class implements. */` |
|     25 | 8968 | `		SyBlobAppend(pOut, bIface ? " extends " : " implements ",` |
|      8 | 8969 | `			bIface ? sizeof(" extends ")-1 : sizeof(" implements ")-1);` |
|     39 | 8970 | `		for( n = 0 ; n < SySetUsed(&aIface) ; n++ ){` |
|     23 | 8971 | `			if( n > 0 ){` |
|      7 | 8972 | `				SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|      3 | 8973 | `			}` |
|     23 | 8974 | `			SyBlobFormat(pOut, "%z", &apIface[n]->sName);` |
|     12 | 8975 | `		}` |
|      8 | 8976 | `	}` |
|     29 | 8977 | `	SyBlobAppend(pOut, " ] {\n", sizeof(" ] {\n")-1);` |
|     29 | 8978 | `	if( !bInternal && SyStringLength(&pClass->sFile) > 0 ){` |
|     21 | 8979 | `		SyBlobAppend(pOut, "  @@ ", sizeof("  @@ ")-1);` |
|     21 | 8980 | `		SyBlobAppend(pOut, SyStringData(&pClass->sFile), SyStringLength(&pClass->sFile));` |
|     21 | 8981 | `		SyBlobFormat(pOut, " %u-%u\n", pClass->nLine, pClass->nEndLine);` |
|     10 | 8982 | `	}` |
|      - | 8983 | `	/* ---- counts ---- */` |
|    181 | 8984 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    153 | 8985 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|    153 | 8986 | `		if( pM->iKind == REFLECT_MEMBER_CONST ){` |
|     25 | 8987 | `			if( !(bEnum && (pM->pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)) ){` |
|     19 | 8988 | `				nConst++;` |
|     10 | 8989 | `			}` |
|    141 | 8990 | `		}else if( pM->iKind == REFLECT_MEMBER_PROP ){` |
|     55 | 8991 | `			if( pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){ nStaticProp++; }else{ nProp++; }` |
|     28 | 8992 | `		}else{` |
|     75 | 8993 | `			if( pM->pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){ nStaticMeth++; }else{ nMeth++; }` |
|      - | 8994 | `		}` |
|     77 | 8995 | `	}` |
|      - | 8996 | `	/* ---- enum cases, then constants ---- */` |
|     29 | 8997 | `	if( bEnum ){` |
|      5 | 8998 | `		rc = ReflectExportEnumCases(pCtx, pOut, pClass);` |
|      5 | 8999 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 9000 | `			goto done;` |
|      - | 9001 | `		}` |
|      2 | 9002 | `	}` |
|     29 | 9003 | `	SyBlobFormat(pOut, "\n  - Constants [%u] {\n", nConst);` |
|    181 | 9004 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    153 | 9005 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|    152 | 9006 | `		if( pM->iKind != REFLECT_MEMBER_CONST` |
|     89 | 9007 | `		 \|\| (bEnum && (pM->pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)) ){` |
|    135 | 9008 | `			continue;` |
|      - | 9009 | `		}` |
|     19 | 9010 | `		SyBlobAppend(pOut, "    ", sizeof("    ")-1);` |
|     19 | 9011 | `		rc = ReflectExportConstLine(pCtx, pOut, pClass, pM->pAttr, &pM->sKey);` |
|     19 | 9012 | `		if( rc != SXRET_OK ){` |
|    ! 0 | 9013 | `			goto done;` |
|      - | 9014 | `		}` |
|     10 | 9015 | `	}` |
|     29 | 9016 | `	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|      - | 9017 | `	/* ---- properties and methods, statics first ---- */` |
|    141 | 9018 | `	for( iPass = 0 ; iPass < 4 ; iPass++ ){` |
|    113 | 9019 | `		int bStatic = (iPass == 0 \|\| iPass == 1);` |
|    113 | 9020 | `		int bMethods = (iPass == 1 \|\| iPass == 3);` |
|    113 | 9021 | `		int bFirst = 1;` |
|      - | 9022 | `		static const char *azTitle[] = {` |
|      - | 9023 | `			"\n  - Static properties [%u] {\n", "\n  - Static methods [%u] {\n",` |
|      - | 9024 | `			"\n  - Properties [%u] {\n", "\n  - Methods [%u] {\n"` |
|      - | 9025 | `		};` |
|      - | 9026 | `		sxu32 aCount[4];` |
|    113 | 9027 | `		aCount[0] = nStaticProp;` |
|    113 | 9028 | `		aCount[1] = nStaticMeth;` |
|    113 | 9029 | `		aCount[2] = nProp;` |
|    113 | 9030 | `		aCount[3] = nMeth;` |
|    113 | 9031 | `		SyBlobFormat(pOut, azTitle[iPass], aCount[iPass]);` |
|    721 | 9032 | `		for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    609 | 9033 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|      - | 9034 | `			int bIsStatic;` |
|    609 | 9035 | `			if( pM->iKind == REFLECT_MEMBER_CONST ){` |
|     97 | 9036 | `				continue;` |
|      - | 9037 | `			}` |
|    513 | 9038 | `			if( bMethods != (pM->iKind == REFLECT_MEMBER_METHOD) ){` |
|    257 | 9039 | `				continue;` |
|      - | 9040 | `			}` |
|    331 | 9041 | `			bIsStatic = bMethods ? (pM->pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0` |
|    182 | 9042 | `				: (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|    257 | 9043 | `			if( bIsStatic != bStatic ){` |
|    129 | 9044 | `				continue;` |
|      - | 9045 | `			}` |
|    129 | 9046 | `			if( bMethods ){` |
|      - | 9047 | `				ReflectFuncRef sRef;` |
|     75 | 9048 | `				if( !bFirst ){` |
|     47 | 9049 | `					SyBlobAppend(pOut, "\n", sizeof(char));` |
|     23 | 9050 | `				}` |
|     75 | 9051 | `				bFirst = 0;` |
|     75 | 9052 | `				ReflectFuncFromMethod(pClass, pM->pMeth, &sRef);` |
|     75 | 9053 | `				rc = ReflectExportFuncBlock(pCtx, pOut, &sRef, "    ", pClass);` |
|     75 | 9054 | `				if( rc != SXRET_OK ){` |
|    ! 0 | 9055 | `					goto done;` |
|      - | 9056 | `				}` |
|     38 | 9057 | `			}else{` |
|     55 | 9058 | `				SyBlobAppend(pOut, "    ", sizeof("    ")-1);` |
|     55 | 9059 | `				ReflectExportPropLine(pCtx, pOut, pM->pAttr, &pM->sKey);` |
|      - | 9060 | `			}` |
|     65 | 9061 | `		}` |
|    113 | 9062 | `		SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|     57 | 9063 | `	}` |
|     29 | 9064 | `	SyBlobAppend(pOut, "}\n", sizeof("}\n")-1);` |
|     14 | 9065 | `done:` |
|     29 | 9066 | `	SySetRelease(&aMembers);` |
|     29 | 9067 | `	SySetRelease(&aIface);` |
|     29 | 9068 | `	return rc;` |
|      1 | 9069 | `}` |
|      - | 9070 | `/* ---- the five __toString() entry points ---- */` |
|     28 | 9071 | `static int ReflectExportClassSelf(ph7_context *pCtx)` |
|      1 | 9072 | `{` |
|     29 | 9073 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|      - | 9074 | `	SyBlob sOut;` |
|      - | 9075 | `	sxi32 rc;` |
|     29 | 9076 | `	if( pClass == 0 ){` |
|    ! 0 | 9077 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 9078 | `		return PH7_OK;` |
|      - | 9079 | `	}` |
|     29 | 9080 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     29 | 9081 | `	rc = ReflectExportClassBlock(pCtx, &sOut, pClass);` |
|     29 | 9082 | `	if( rc == SXRET_OK ){` |
|     29 | 9083 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     14 | 9084 | `	}` |
|     29 | 9085 | `	SyBlobRelease(&sOut);` |
|     29 | 9086 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|     15 | 9087 | `}` |
|     54 | 9088 | `static int ReflectExportFuncSelf(ph7_context *pCtx)` |
|      2 | 9089 | `{` |
|      - | 9090 | `	ReflectFuncRef sRef;` |
|      - | 9091 | `	SyBlob sOut;` |
|      - | 9092 | `	sxi32 rc;` |
|     56 | 9093 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|    ! 0 | 9094 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 9095 | `		return PH7_OK;` |
|      - | 9096 | `	}` |
|     56 | 9097 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     56 | 9098 | `	rc = ReflectExportFuncBlock(pCtx, &sOut, &sRef, "", 0);` |
|     56 | 9099 | `	if( rc == SXRET_OK ){` |
|     56 | 9100 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     27 | 9101 | `	}` |
|     56 | 9102 | `	SyBlobRelease(&sOut);` |
|     56 | 9103 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|     29 | 9104 | `}` |
|     10 | 9105 | `static int ReflectExportParamSelf(ph7_context *pCtx)` |
|      1 | 9106 | `{` |
|      - | 9107 | `	ReflectFuncRef sRef;` |
|      - | 9108 | `	ReflectParamDesc sDesc;` |
|      - | 9109 | `	SyBlob sOut;` |
|     11 | 9110 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) ){` |
|    ! 0 | 9111 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 9112 | `		return PH7_OK;` |
|      - | 9113 | `	}` |
|     11 | 9114 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     11 | 9115 | `	ReflectExportParamLine(pCtx, &sOut, &sDesc);` |
|     11 | 9116 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     11 | 9117 | `	SyBlobRelease(&sOut);` |
|     11 | 9118 | `	return PH7_OK;` |
|      6 | 9119 | `}` |
|      4 | 9120 | `static int ReflectExportPropSelf(ph7_context *pCtx)` |
|      1 | 9121 | `{` |
|      - | 9122 | `	ReflectMemberRef sRef;` |
|      - | 9123 | `	SyBlob sOut;` |
|      - | 9124 | `	SyString sKey;` |
|      5 | 9125 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 9126 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 9127 | `		return PH7_OK;` |
|      - | 9128 | `	}` |
|      5 | 9129 | `	SyStringInitFromBuf(&sKey, sRef.zName, sRef.nName);` |
|      5 | 9130 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|      5 | 9131 | `	ReflectExportPropLine(pCtx, &sOut, sRef.pAttr, &sKey);` |
|      5 | 9132 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|      5 | 9133 | `	SyBlobRelease(&sOut);` |
|      5 | 9134 | `	return PH7_OK;` |
|      3 | 9135 | `}` |
|      6 | 9136 | `static int ReflectExportConstSelf(ph7_context *pCtx)` |
|      1 | 9137 | `{` |
|      - | 9138 | `	ReflectMemberRef sRef;` |
|      - | 9139 | `	SyBlob sOut;` |
|      - | 9140 | `	SyString sKey;` |
|      - | 9141 | `	sxi32 rc;` |
|      7 | 9142 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|    ! 0 | 9143 | `		ph7_result_string(pCtx, "", 0);` |
|    ! 0 | 9144 | `		return PH7_OK;` |
|      - | 9145 | `	}` |
|      7 | 9146 | `	SyStringInitFromBuf(&sKey, sRef.zName, sRef.nName);` |
|      7 | 9147 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|      7 | 9148 | `	rc = ReflectExportConstLine(pCtx, &sOut, sRef.pClass, sRef.pAttr, &sKey);` |
|      7 | 9149 | `	if( rc == SXRET_OK ){` |
|      7 | 9150 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|      3 | 9151 | `	}` |
|      7 | 9152 | `	SyBlobRelease(&sOut);` |
|      7 | 9153 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|      4 | 9154 | `}` |
|      - | 9155 | `/*` |
|      - | 9156 | ` * Install the Reflection API — ~25 classes, all of them declared from C.` |
|      - | 9157 | ` *` |
|      - | 9158 | `` * There is no global thunk table left to register: every `__reflect_*` and`` |
|      - | 9159 | `` * `__phl_rcinfo` became a method of the class that always owned it, so`` |
|      - | 9160 | ` * Reflection adds no name to php's global function namespace at all. Called` |
|      - | 9161 | ` * from PH7_VmInit while pVm->bCompilingBuiltin is set, right after the core` |
|      - | 9162 | ` * builtin chunks (Exception and friends have to exist already).` |
|      - | 9163 | ` */` |
|   5254 | 9164 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|      5 | 9165 | `{` |
|      - | 9166 | `	sxi32 rc;` |
|      - | 9167 | `	/* Where chunk 1 was: Reflector, Reflection, ReflectionException,` |
|      - | 9168 | `	 * ReflectionClass and ReflectionObject are native now. Chunks 2 and 3 name` |
|      - | 9169 | ``	 * `Reflector` in their own `implements` clauses, so this has to run first. */`` |
|   5259 | 9170 | `	rc = PH7_VmInstallReflectionClass(&(*pVm));` |
|   5259 | 9171 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9172 | `		return rc;` |
|      - | 9173 | `	}` |
|      - | 9174 | `	/* Where chunk 2 was: ReflectionFunctionAbstract, ReflectionFunction,` |
|      - | 9175 | `	 * ReflectionMethod and ReflectionParameter are native now. */` |
|   5259 | 9176 | `	rc = PH7_VmInstallReflectionFunc(&(*pVm));` |
|   5259 | 9177 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9178 | `		return rc;` |
|      - | 9179 | `	}` |
|   5259 | 9180 | `	rc = PH7_VmInstallReflectionHookType(&(*pVm));` |
|   5259 | 9181 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9182 | `		return rc;` |
|      - | 9183 | `	}` |
|      - | 9184 | `	/* Where chunk 3 was: ReflectionProperty and ReflectionClassConstant are` |
|      - | 9185 | `	 * native now, and PropertyHookType is a native ENUM. */` |
|   5259 | 9186 | `	rc = PH7_VmInstallReflectionMember(&(*pVm));` |
|   5259 | 9187 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9188 | `		return rc;` |
|      - | 9189 | `	}` |
|      - | 9190 | `	/* Where chunk 4 was: the four type classes are native now (see` |
|      - | 9191 | `	 * PH7_VmInstallReflectionTypes). Stringable exists by this point. */` |
|   5259 | 9192 | `	rc = PH7_VmInstallReflectionTypes(&(*pVm));` |
|   5259 | 9193 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9194 | `		return rc;` |
|      - | 9195 | `	}` |
|      - | 9196 | `	/* Where chunk 5 was: ReflectionGenerator/Fiber and the four standalone` |
|      - | 9197 | `	 * classes chunk 6 used to hold are native now. Reflector (chunk 1) exists. */` |
|   5259 | 9198 | `	rc = PH7_VmInstallReflectionSmall(&(*pVm));` |
|   5259 | 9199 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9200 | `		return rc;` |
|      - | 9201 | `	}` |
|      - | 9202 | `	/* Where chunk 6 was: the three ReflectionEnum classes are native now, and` |
|      - | 9203 | `	 * the class DESCRIPTOR they read (__phl_rcinfo) has no callers left. */` |
|   5259 | 9204 | `	rc = PH7_VmInstallReflectionEnum(&(*pVm));` |
|   5259 | 9205 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9206 | `		return rc;` |
|      - | 9207 | `	}` |
|      - | 9208 | `	/* Where chunk 7 was: ReflectionAttribute is native now, and with it the` |
|      - | 9209 | `	 * builder (__reflect_build_attrs) and the two helpers it needed. */` |
|   5259 | 9210 | `	rc = PH7_VmInstallReflectionAttribute(&(*pVm));` |
|   5259 | 9211 | `	if( rc != SXRET_OK ){` |
|    ! 0 | 9212 | `		return rc;` |
|      - | 9213 | `	}` |
|      - | 9214 | `	/* Where chunk 9 was: the export format is ReflectExportClassSelf() and its` |
|      - | 9215 | `	 * four siblings above. Nothing of the Reflection library is PHP any more,` |
|      - | 9216 | `	 * so vm_builtin_reflection_lib.c is gone and this IS the installer. */` |
|   5259 | 9217 | `	return SXRET_OK;` |
|   2632 | 9218 | `}` |
|      - | 9219 |  |
