# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 5127/5945 lines (86.24%)

[Root index](../../index.md) | [Directory index](index.md)

|  Hits | Line | Source |
| ----: | ---: | :--- |
|     - |    1 | `/**` |
|     - |    2 | ` * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>` |
|     - |    3 | ` * SPDX-License-Identifier: BSD-3-Clause` |
|     - |    4 | ` */` |
|     - |    5 | `#include "ph7int.h"` |
|     - |    6 | `/*` |
|     - |    7 | ` * This file implements the PHP 8.5 Reflection API.` |
|     - |    8 | ` *` |
|     - |    9 | ` * Following the engine's builtin-class pattern (Generator/Fiber/Closure),` |
|     - |   10 | ` * the Reflection classes themselves are written in PHP, embedded below as` |
|     - |   11 | ` * C string chunks and compiled at VM init by PH7_VmInstallReflection().` |
|     - |   12 | ` * Native behavior is provided by a small set of global __reflect_* thunk` |
|     - |   13 | ` * functions implemented here: the PHP methods forward to them, passing` |
|     - |   14 | ` * their target (class name, object, ...) explicitly.` |
|     - |   15 | ` *` |
|     - |   16 | ` * The chunks are kept below 30 KB each: MSVC caps a concatenated string` |
|     - |   17 | ` * literal at 65,535 bytes and the Windows build is real (build-aux/nmake.mk).` |
|     - |   18 | ` */` |
|     - |   19 |  |
|     - |   20 | `/* Bound on hierarchy walks; matches PH7_INTERFACE_WALK_MAX_DEPTH in` |
|     - |   21 | ` * vm_builtin_class.c. */` |
|     - |   22 | `#define REFLECT_WALK_MAX_DEPTH 64` |
|     - |   23 |  |
|     - |   24 | `static sxi32 ReflectEnforceStore(ph7_context *pCtx, sxu32 nIdx, ph7_value *pValue);` |
|     - |   25 |  |
|     - |   26 | `/*` |
|     - |   27 | ` * Resolve a class-name string or object into a ph7_class pointer,` |
|     - |   28 | ` * triggering autoload for unknown string names. Returns NULL when the` |
|     - |   29 | ` * class does not exist (the PHP layer turns that into ReflectionException).` |
|     - |   30 | ` */` |
|  2548 |   31 | `static ph7_class * ReflectResolveClass(ph7_vm *pVm, ph7_value *pArg)` |
|     5 |   32 | `{` |
|     - |   33 | `	ph7_class *pClass;` |
|  2553 |   34 | `	pClass = PH7_VmExtractClassFromValue(pVm, pArg);` |
|  2553 |   35 | `	if( pClass == 0 && ph7_value_is_string(pArg) ){` |
|     - |   36 | `		const char *zName;` |
|     - |   37 | `		int nLen;` |
|    20 |   38 | `		zName = ph7_value_to_string(pArg, &nLen);` |
|    20 |   39 | `		if( nLen > 0 ){` |
|    20 |   40 | `			pClass = PH7_VmTriggerAutoload(pVm, zName, (sxu32)nLen, FALSE);` |
|     9 |   41 | `		}` |
|     9 |   42 | `	}` |
|  2553 |   43 | `	return pClass;` |
|     5 |   44 | `}` |
|     - |   45 | `/*` |
|     - |   46 | ` * Hand a freshly created class instance to the caller. The return slot` |
|     - |   47 | ` * takes over the initial reference from PH7_NewClassInstance (iRef=1):` |
|     - |   48 | ` * no extra iRef++ here (see the synthesized-object invariant — a stray` |
|     - |   49 | ` * bump leaks the object and disables its __destruct).` |
|     - |   50 | ` */` |
|   592 |   51 | `static int ReflectResultObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|     5 |   52 | `{` |
|   597 |   53 | `	if( pObj == 0 ){` |
|   ! 0 |   54 | `		ph7_result_null(pCtx);` |
|   ! 0 |   55 | `		return PH7_OK;` |
|     - |   56 | `	}` |
|   597 |   57 | `	PH7_MemObjRelease(pCtx->pRet);` |
|   597 |   58 | `	pCtx->pRet->x.pOther = pObj;` |
|   597 |   59 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|   597 |   60 | `	return PH7_OK;` |
|   301 |   61 | `}` |
|     - |   62 | `/* The last of the descriptor marshalling: ReflectMapAddDyn survives because` |
|     - |   63 | ` * ReflectAttrArgs() answers a php ARRAY whose named arguments are string keys.` |
|     - |   64 | ` * Its Bool/Int/Str/Null/Attrs/Doc siblings went with __phl_rcinfo. */` |
|     - |   65 | `/* Add an entry under a dynamic (SyString) key. */` |
|   102 |   66 | `static void ReflectMapAddDyn(ph7_context *pCtx, ph7_value *pMap,` |
|     - |   67 | `	const SyString *pKey, ph7_value *pVal)` |
|     2 |   68 | `{` |
|   104 |   69 | `	ph7_value *pK = ph7_context_new_scalar(pCtx);` |
|   104 |   70 | `	if( pK == 0 ){ return; }` |
|   104 |   71 | `	ph7_value_string(pK, pKey->zString, (int)pKey->nByte);` |
|   104 |   72 | `	ph7_array_add_elem(pMap, pK, pVal);` |
|    53 |   73 | `}` |
|     - |   74 | `/*` |
|     - |   75 | ` * Append pIface (and its parents / extended interfaces) to the dedup set` |
|     - |   76 | ` * of ph7_class pointers.` |
|     - |   77 | ` */` |
|   322 |   78 | `static void ReflectAddInterface(ph7_class *pIface, SySet *pOut, int iDepth)` |
|     2 |   79 | `{` |
|     - |   80 | `	ph7_class **apKnown;` |
|     - |   81 | `	sxu32 n;` |
|   324 |   82 | `	if( pIface == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|   ! 0 |   83 | `		return;` |
|     - |   84 | `	}` |
|     - |   85 | `	/* Parents of an interface come along too (interface B extends A) */` |
|   324 |   86 | `	if( pIface->pBase ){` |
|    95 |   87 | `		ReflectAddInterface(pIface->pBase, pOut, iDepth + 1);` |
|    47 |   88 | `	}` |
|     - |   89 | `	/* Some engines record extended interfaces in aInterface as well */` |
|   324 |   90 | `	apKnown = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|   324 |   91 | `	for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|   ! 0 |   92 | `		ReflectAddInterface(apKnown[n], pOut, iDepth + 1);` |
|   ! 0 |   93 | `	}` |
|     - |   94 | `	/* Dedup by pointer */` |
|   324 |   95 | `	apKnown = (ph7_class **)SySetBasePtr(pOut);` |
|   512 |   96 | `	for( n = 0 ; n < SySetUsed(pOut) ; n++ ){` |
|   225 |   97 | `		if( apKnown[n] == pIface ){` |
|    37 |   98 | `			return;` |
|     - |   99 | `		}` |
|    95 |  100 | `	}` |
|   288 |  101 | `	SySetPut(pOut, (const void *)&pIface);` |
|   163 |  102 | `}` |
|     - |  103 | `/*` |
|     - |  104 | ` * Collect the transitive set of interfaces implemented by pClass:` |
|     - |  105 | ` * the parent chain's interfaces first, then the class's own.` |
|     - |  106 | ` */` |
|   310 |  107 | `static void ReflectCollectInterfaces(ph7_class *pClass, SySet *pOut, int iDepth)` |
|     2 |  108 | `{` |
|     - |  109 | `	ph7_class **apIface;` |
|     - |  110 | `	sxu32 n;` |
|   312 |  111 | `	if( pClass == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|   ! 0 |  112 | `		return;` |
|     - |  113 | `	}` |
|   312 |  114 | `	if( pClass->pBase ){` |
|    88 |  115 | `		ReflectCollectInterfaces(pClass->pBase, pOut, iDepth + 1);` |
|    43 |  116 | `	}` |
|   312 |  117 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   492 |  118 | `	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){` |
|   182 |  119 | `		ReflectAddInterface(apIface[n], pOut, iDepth + 1);` |
|    92 |  120 | `	}` |
|   157 |  121 | `}` |
|     - |  122 | `/*` |
|     - |  123 | ` * Deepest base class whose method table maps the same name to the very` |
|     - |  124 | ` * same ph7_class_method pointer: inheritance shares member pointers` |
|     - |  125 | ` * (PH7_ClassInherit), so this identifies the declaring class. Methods` |
|     - |  126 | ` * copied in from traits are not on the pBase chain and thus report the` |
|     - |  127 | ` * using class, which is what PHP reports too.` |
|     - |  128 | ` */` |
|  9870 |  129 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth)` |
|     5 |  130 | `{` |
|  9875 |  131 | `	ph7_class *pDecl = pClass;` |
|  9875 |  132 | `	ph7_class *pBase = pClass->pBase;` |
|  9875 |  133 | `	int iDepth = 0;` |
| 11977 |  134 | `	while( pBase && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|     - |  135 | `		SyHashEntry *pEntry;` |
|  4870 |  136 | `		pEntry = SyHashGet(&pBase->hMethod, (const void *)SyStringData(&pMeth->sFunc.sName),` |
|  1623 |  137 | `			SyStringLength(&pMeth->sFunc.sName));` |
|  3247 |  138 | `		if( pEntry == 0 \|\| (ph7_class_method *)pEntry->pUserData != pMeth ){` |
|   573 |  139 | `			break;` |
|     - |  140 | `		}` |
|  2103 |  141 | `		pDecl = pBase;` |
|  2103 |  142 | `		pBase = pBase->pBase;` |
|  2103 |  143 | `		iDepth++;` |
|     1 |  144 | `	}` |
|  9875 |  145 | `	return pDecl;` |
|     5 |  146 | `}` |
|     - |  147 | `/*` |
|     - |  148 | ` * The interface list php reports for pClass: the transitive set, plus an` |
|     - |  149 | `` * INTERFACE's own parents (`interface B extends A` lists A). Caller owns the`` |
|     - |  150 | ` * set (SySetInit with sizeof(ph7_class *)).` |
|     - |  151 | ` */` |
|   224 |  152 | `static void ReflectInterfacesOf(ph7_class *pClass, SySet *pOut)` |
|     2 |  153 | `{` |
|   226 |  154 | `	ReflectCollectInterfaces(pClass, pOut, 0);` |
|   226 |  155 | `	if( (pClass->iFlags & PH7_CLASS_INTERFACE) && pClass->pBase ){` |
|    49 |  156 | `		ReflectAddInterface(pClass->pBase, pOut, 0);` |
|    24 |  157 | `	}` |
|   226 |  158 | `}` |
|     - |  159 | `/* Fetch a class attribute (property or constant) by plain name. */` |
|     4 |  160 | `static ph7_class_attr * ReflectFetchAttr(ph7_class *pClass, ph7_value *pName)` |
|     1 |  161 | `{` |
|     - |  162 | `	SyHashEntry *pEntry;` |
|     - |  163 | `	const char *zName;` |
|     - |  164 | `	int nLen;` |
|     5 |  165 | `	zName = ph7_value_to_string(pName, &nLen);` |
|     5 |  166 | `	if( nLen < 1 ){` |
|   ! 0 |  167 | `		return 0;` |
|     - |  168 | `	}` |
|     5 |  169 | `	pEntry = SyHashGet(&pClass->hAttr, (const void *)zName, (sxu32)nLen);` |
|     5 |  170 | `	if( pEntry == 0 ){` |
|     3 |  171 | `		return 0;` |
|     - |  172 | `	}` |
|     3 |  173 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|     3 |  174 | `}` |
|     - |  175 | `/* Fetch a class CONSTANT (or enum case) by name from the hConst namespace. */` |
|     2 |  176 | `static ph7_class_attr * ReflectFetchConst(ph7_class *pClass, ph7_value *pName)` |
|     1 |  177 | `{` |
|     - |  178 | `	const char *zName;` |
|     - |  179 | `	int nLen;` |
|     3 |  180 | `	zName = ph7_value_to_string(pName, &nLen);` |
|     3 |  181 | `	if( nLen < 1 ){` |
|   ! 0 |  182 | `		return 0;` |
|     - |  183 | `	}` |
|     3 |  184 | `	return PH7_ClassExtractConstant(pClass, zName, (sxu32)nLen);` |
|     2 |  185 | `}` |
|     - |  186 | `/* Fetch a member by name from EITHER namespace (property then constant) — used` |
|     - |  187 | ` * where the caller reflects over a member that may be either (e.g. attributes` |
|     - |  188 | ` * attached to a property or a constant). */` |
|     4 |  189 | `static ph7_class_attr * ReflectFetchMember(ph7_class *pClass, ph7_value *pName)` |
|     1 |  190 | `{` |
|     5 |  191 | `	ph7_class_attr *pAttr = ReflectFetchAttr(pClass, pName);` |
|     5 |  192 | `	if( pAttr == 0 ){` |
|     3 |  193 | `		pAttr = ReflectFetchConst(pClass, pName);` |
|     1 |  194 | `	}` |
|     5 |  195 | `	return pAttr;` |
|     1 |  196 | `}` |
|     - |  197 | `/*` |
|     - |  198 | ` * ---------------------------------------------------------------------------` |
|     - |  199 | ` * The member walk.` |
|     - |  200 | ` *` |
|     - |  201 | ` * php reports a class's members in ONE order — the class's own first (in` |
|     - |  202 | ` * declaration order), then each inheritance level's, outward — and every` |
|     - |  203 | ` * accessor that lists or looks one up has to agree with it. It used to live` |
|     - |  204 | ` * inside the descriptor builder alone; the native ReflectionClass needs the` |
|     - |  205 | ` * same order for getMethods()/getProperties()/getReflectionConstants() and the` |
|     - |  206 | ` * same visibility filtering for hasMethod()/getMethod()/getConstructor(), so` |
|     - |  207 | ` * the walk is factored out here and every one of them drives it.` |
|     - |  208 | ` *` |
|     - |  209 | ` * Per level the DECLARING class's own hash is iterated — a subclass hash` |
|     - |  210 | ` * interleaves inherited pointers unpredictably — and a pointer-identity lookup` |
|     - |  211 | ` * in the reflected class's hash drops what is not visible there (overridden` |
|     - |  212 | ` * entries). Methods come out reversed because hMethod is still a head-insert` |
|     - |  213 | ` * table, while hAttr/hConst insert at the tail.` |
|     - |  214 | ` * ---------------------------------------------------------------------------` |
|     - |  215 | ` */` |
|     - |  216 | `#define REFLECT_MEMBER_PROP   0` |
|     - |  217 | `#define REFLECT_MEMBER_CONST  1` |
|     - |  218 | `#define REFLECT_MEMBER_METHOD 2` |
|     - |  219 |  |
|     - |  220 | `typedef struct ReflectMember ReflectMember;` |
|     - |  221 | `struct ReflectMember` |
|     - |  222 | `{` |
|     - |  223 | `	int iKind;               /* REFLECT_MEMBER_* */` |
|     - |  224 | `	SyString sKey;           /* the name php reports it under: a trait` |
|     - |  225 | ``	                          * `use T { m as n; }` alias differs from the`` |
|     - |  226 | `	                          * method's own sFunc.sName, and php reports n */` |
|     - |  227 | `	ph7_class *pDecl;        /* declaring class */` |
|     - |  228 | `	ph7_class_attr *pAttr;   /* property or constant (NULL for a method) */` |
|     - |  229 | `	ph7_class_method *pMeth; /* method (NULL for a property or constant) */` |
|     - |  230 | `};` |
|     - |  231 | `/*` |
|     - |  232 | ` * Collect the members php would report for pClass, in php's own order, into a` |
|     - |  233 | ` * SySet of ReflectMember. The caller owns the set (SySetInit with` |
|     - |  234 | ` * sizeof(ReflectMember) / SySetRelease).` |
|     - |  235 | ` *` |
|     - |  236 | ` * bLookup selects which of php's TWO answers is wanted. The LISTING` |
|     - |  237 | ` * (getMethods()/getProperties()/getReflectionConstants(), bLookup = 0) hides a` |
|     - |  238 | ` * base class's private members; the LOOKUP (bLookup = 1) does not, because php` |
|     - |  239 | ` * keeps a parent's private in the child's tables and ReflectionMethod resolves` |
|     - |  240 | ` * against those. The two really do disagree: hasMethod('basePriv') is true on` |
|     - |  241 | ` * the subclass while getMethods() never mentions it.` |
|     - |  242 | ` */` |
|   964 |  243 | `static void ReflectMembers(ph7_vm *pVm, ph7_class *pClass, SySet *pOut, int bLookup)` |
|     5 |  244 | `{` |
|     - |  245 | `	ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];` |
|   969 |  246 | `	ph7_class *pWalk = pClass;` |
|     - |  247 | `	SyHashEntry *pEntry;` |
|     - |  248 | `	SySet aTmp;` |
|   969 |  249 | `	int bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;` |
|     - |  250 | `	int iPart;` |
|   969 |  251 | `	sxu32 nChain = 0, iLevel, nLev, nT;` |
|  2067 |  252 | `	while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){` |
|  1103 |  253 | `		aChain[nChain++] = pWalk;` |
|  1103 |  254 | `		pWalk = pWalk->pBase;` |
|     5 |  255 | `	}` |
|   969 |  256 | `	SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));` |
|     - |  257 | `	/* PROPERTIES of an INTERNAL class come out base-first, and that is php's` |
|     - |  258 | `	 * registration order rather than a rule of its own: a user class declares its` |
|     - |  259 | ``	 * own properties and THEN inherits (`class B extends A` reports B's before`` |
|     - |  260 | `	 * A's), while an internal one is registered against its parent and declares` |
|     - |  261 | `	 * afterwards — so ErrorException reports Exception's five and then its own` |
|     - |  262 | `	 * $severity. Only the property table flips: php lists an internal class's` |
|     - |  263 | `	 * METHODS and CONSTANTS own-first like everything else, so the walk below` |
|     - |  264 | `	 * runs the two tables in opposite level orders. */` |
|  3861 |  265 | `	for( iPart = 0 ; iPart < 3 ; iPart++ ){` |
|  6191 |  266 | `	  for( nLev = 0 ; nLev < nChain ; nLev++ ){` |
|     - |  267 | `		ph7_class *pLevel;` |
|  3299 |  268 | `		int iTab = iPart;` |
|  3299 |  269 | `		iLevel = (iPart == 0 && bInternal) ? nChain - 1 - nLev : nLev;` |
|  3299 |  270 | `		pLevel = aChain[iLevel];` |
|     - |  271 | `		/* --- Properties (hAttr, iPart 0) and constants/enum cases (hConst,` |
|     - |  272 | `		 * iPart 1) — php's two separate member namespaces. Each table is` |
|     - |  273 | `		 * collected and emitted independently; the CONSTANT flag still decides` |
|     - |  274 | `		 * which kind comes out. --- */` |
|  3299 |  275 | `		if( iPart < 2 ){` |
|  2201 |  276 | `			SyHash *pSrcHash = iTab ? &pLevel->hConst : &pLevel->hAttr;` |
|  2201 |  277 | `			SyHash *pRefHash = iTab ? &pClass->hConst : &pClass->hAttr;` |
|  2201 |  278 | `			SySetReset(&aTmp);` |
|  2201 |  279 | `			SyHashResetLoopCursor(pSrcHash);` |
|  9075 |  280 | `			while( (pEntry = SyHashGetNextEntry(pSrcHash)) != 0 ){` |
|  6879 |  281 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  6879 |  282 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|  6879 |  283 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_HIDDEN ){` |
|     - |  284 | `					/* A native class's engine slot: php holds that state in its own C` |
|     - |  285 | `					 * struct and reports no property for it at all. */` |
|   387 |  286 | `					continue;` |
|     - |  287 | `				}` |
|  6493 |  288 | `				if( iLevel == 0 ){` |
|     - |  289 | `					sxu32 j;` |
|     - |  290 | `					/* Own = declared here or by an off-chain provider (trait) */` |
|  6459 |  291 | `					for( j = 1 ; j < nChain ; j++ ){` |
|   627 |  292 | `						if( aChain[j] == pDecl ){ break; }` |
|   149 |  293 | `					}` |
|  6163 |  294 | `					if( j < nChain ){ continue; }` |
|  2919 |  295 | `				}else{` |
|     - |  296 | `					SyHashEntry *pSub;` |
|   331 |  297 | `					if( pDecl != pLevel ){ continue; }` |
|     - |  298 | `					/* A base's PRIVATE member is not part of the subclass's` |
|     - |  299 | `					 * surface — php reports neither a private property nor a` |
|     - |  300 | `					 * private constant of a parent on the child. PHL's` |
|     - |  301 | `					 * inheritance copies them down all the same, so the filter` |
|     - |  302 | `					 * has to be here. */` |
|   331 |  303 | `					if( !bLookup && pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){ continue; }` |
|     - |  304 | `					/* Must still be the visible member in the reflected class */` |
|   249 |  305 | `					pSub = SyHashGet(pRefHash, pEntry->pKey, pEntry->nKeyLen);` |
|   249 |  306 | `					if( pSub == 0 \|\| pSub->pUserData != (void *)pAttr ){ continue; }` |
|     - |  307 | `				}` |
|  6081 |  308 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     5 |  309 | `			}` |
|     - |  310 | `			/* Forward: hAttr/hConst iterate in DECLARATION order (tail inserts),` |
|     - |  311 | `			 * so members come out in the order php reports them. */` |
|  8277 |  312 | `			for( nT = 0 ; nT < SySetUsed(&aTmp) ; nT++ ){` |
|  6081 |  313 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT);` |
|  6081 |  314 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;` |
|     - |  315 | `				ReflectMember sMember;` |
|  6081 |  316 | `				sMember.iKind = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)` |
|  3038 |  317 | `					? REFLECT_MEMBER_CONST : REFLECT_MEMBER_PROP;` |
|  6081 |  318 | `				sMember.sKey = pAttr->sName;` |
|  6081 |  319 | `				sMember.pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|  6081 |  320 | `				sMember.pAttr = pAttr;` |
|  6081 |  321 | `				sMember.pMeth = 0;` |
|  6081 |  322 | `				SySetPut(pOut, (const void *)&sMember);` |
|  3043 |  323 | `			}` |
|  2201 |  324 | `			continue;` |
|     - |  325 | `		}` |
|     - |  326 | `		/* --- Methods. The reported name is the hash-entry KEY, not the` |
|     - |  327 | `		 * function's own name (see ReflectMember::sKey). --- */` |
|  1103 |  328 | `		SySetReset(&aTmp);` |
|  1103 |  329 | `		SyHashResetLoopCursor(&pLevel->hMethod);` |
|  6333 |  330 | `		while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){` |
|  5234 |  331 | `			ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  5234 |  332 | `			ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|     - |  333 | `			/* A property HOOK is compiled into a method (__phl_hook_get_NAME /` |
|     - |  334 | ``			 * __phl_hook_set_NAME) so that inheritance and `parent::` work on`` |
|     - |  335 | `			 * it, but php has no method there at all: it reports the hook on` |
|     - |  336 | `			 * the PROPERTY. Listing it would put a PHL-only name on a` |
|     - |  337 | `			 * php-visible surface. */` |
|  5230 |  338 | `			if( pEntry->nKeyLen > sizeof("__phl_hook_")-1` |
|  3384 |  339 | `			 && SyMemcmp(pEntry->pKey, "__phl_hook_", sizeof("__phl_hook_")-1) == 0 ){` |
|   713 |  340 | `				continue;` |
|     - |  341 | `			}` |
|  4523 |  342 | `			if( iLevel == 0 ){` |
|     - |  343 | `				sxu32 j;` |
|  4401 |  344 | `				for( j = 1 ; j < nChain ; j++ ){` |
|   881 |  345 | `					if( aChain[j] == pDecl ){ break; }` |
|   251 |  346 | `				}` |
|  3901 |  347 | `				if( j < nChain ){ continue; }` |
|  1762 |  348 | `			}else{` |
|     - |  349 | `				SyHashEntry *pSub;` |
|   623 |  350 | `				if( pDecl != pLevel ){ continue; }` |
|     - |  351 | `				/* Same rule as the members above: a base's PRIVATE method is` |
|     - |  352 | ``				 * not on the subclass's surface. `class B extends A` lists`` |
|     - |  353 | `				 * only A::q when A::p is private — php's inheritance never` |
|     - |  354 | `				 * hands the child a private, and PH7_ClassInherit's copy-down` |
|     - |  355 | `				 * does, so it is filtered here. */` |
|   433 |  356 | `				if( !bLookup && pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){ continue; }` |
|   391 |  357 | `				pSub = SyHashGet(&pClass->hMethod, pEntry->pKey, pEntry->nKeyLen);` |
|   391 |  358 | `				if( pSub == 0 \|\| pSub->pUserData != (void *)pMeth ){` |
|     - |  359 | `					/* Overridden below this level: already reported */` |
|    53 |  360 | `					continue;` |
|     - |  361 | `				}` |
|     - |  362 | `			}` |
|  3859 |  363 | `			SySetPut(&aTmp, (const void *)&pEntry);` |
|     3 |  364 | `		}` |
|  4959 |  365 | `		for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|  3859 |  366 | `			SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|     - |  367 | `			ReflectMember sMember;` |
|  3859 |  368 | `			sMember.iKind = REFLECT_MEMBER_METHOD;` |
|  3859 |  369 | `			SyStringInitFromBuf(&sMember.sKey, (const char *)pE->pKey, pE->nKeyLen);` |
|  3859 |  370 | `			sMember.pMeth = (ph7_class_method *)pE->pUserData;` |
|  3859 |  371 | `			sMember.pDecl = ReflectMethodDeclClass(pClass, sMember.pMeth);` |
|  3859 |  372 | `			sMember.pAttr = 0;` |
|  3859 |  373 | `			SySetPut(pOut, (const void *)&sMember);` |
|  1931 |  374 | `		}` |
|   554 |  375 | `	  }` |
|  1451 |  376 | `	}` |
|   969 |  377 | `	SySetRelease(&aTmp);` |
|   969 |  378 | `}` |
|     - |  379 | `/* Does a collected member name match zName exactly? */` |
|  2392 |  380 | `static int ReflectKeyIs(const ReflectMember *pM, const char *zName, int nName)` |
|     5 |  381 | `{` |
|  2929 |  382 | `	return SyStringLength(&pM->sKey) == (sxu32)nName` |
|  2392 |  383 | `		&& SyMemcmp(SyStringData(&pM->sKey), zName, (sxu32)nName) == 0;` |
|     5 |  384 | `}` |
|     - |  385 | `/*` |
|     - |  386 | ` * php's METHOD lookup, which is NOT the listing.` |
|     - |  387 | ` *` |
|     - |  388 | ` * getMethods() hides a base's private method, but hasMethod()/getMethod() find` |
|     - |  389 | ` * one: Zend keeps the parent's private in the child's function table and reads` |
|     - |  390 | ` * that table directly. PHL's inheritance copies methods down the same way, so` |
|     - |  391 | ` * the lookup is the class's own hMethod — case-insensitively, like php.` |
|     - |  392 | ` */` |
|   748 |  393 | `static SyHashEntry * ReflectFindMethodEntry(ph7_class *pClass, const char *zName, int nName)` |
|     5 |  394 | `{` |
|     - |  395 | `	SyHashEntry *pEntry;` |
|   753 |  396 | `	if( nName < 1 ){` |
|   ! 0 |  397 | `		return 0;` |
|     - |  398 | `	}` |
|   753 |  399 | `	pEntry = SyHashGet(&pClass->hMethod, (const void *)zName, (sxu32)nName);` |
|   753 |  400 | `	if( pEntry ){` |
|   563 |  401 | `		return pEntry;` |
|     - |  402 | `	}` |
|   193 |  403 | `	SyHashResetLoopCursor(&pClass->hMethod);` |
|   540 |  404 | `	while( (pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){` |
|   252 |  405 | `		if( (int)pEntry->nKeyLen == nName` |
|   140 |  406 | `		 && SyStrnicmp((const char *)pEntry->pKey, zName, (sxu32)nName) == 0 ){` |
|   ! 0 |  407 | `			return pEntry;` |
|     - |  408 | `		}` |
|     3 |  409 | `	}` |
|   193 |  410 | `	return 0;` |
|   379 |  411 | `}` |
|     - |  412 | `/*` |
|     - |  413 | `` * The visibility of the __construct / __clone `new` and `clone` would reach, or`` |
|     - |  414 | ` * 0 when the class has none — what isInstantiable() and isCloneable() screen on.` |
|     - |  415 | ` * The LOOKUP, not the listing: a class that inherits a private constructor is` |
|     - |  416 | ` * still not instantiable even though getMethods() does not report one.` |
|     - |  417 | ` */` |
|    24 |  418 | `static void ReflectCtorCloneVis(ph7_vm *pVm, ph7_class *pClass, sxi32 *piCtor, sxi32 *piClone)` |
|     3 |  419 | `{` |
|     - |  420 | `	ph7_class_method *pMeth;` |
|    12 |  421 | `	SXUNUSED(pVm);` |
|    27 |  422 | `	*piCtor = *piClone = 0;` |
|    27 |  423 | `	pMeth = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|    27 |  424 | `	if( pMeth ){` |
|    16 |  425 | `		*piCtor = pMeth->iProtection;` |
|     7 |  426 | `	}` |
|    27 |  427 | `	pMeth = PH7_ClassExtractMethod(pClass, "__clone", sizeof("__clone")-1);` |
|    27 |  428 | `	if( pMeth ){` |
|   ! 0 |  429 | `		*piClone = pMeth->iProtection;` |
|   ! 0 |  430 | `	}` |
|    27 |  431 | `}` |
|     - |  432 | `/* Where __phl_rcinfo() was: a full class DESCRIPTOR array — every constant,` |
|     - |  433 | ` * property and method of the whole inheritance chain, marshalled once and` |
|     - |  434 | ` * memoized on pVm->hClassInfo because the prelude classes rebuilt it once per` |
|     - |  435 | ` * member they constructed. Every one of its readers is a native class now and` |
|     - |  436 | ` * reads ph7_class directly, so the builder and the VM memo are both gone. */` |
|     - |  437 | `/*` |
|     - |  438 | ` * Collect a PHP array's values into a ph7_value* set (call arguments).` |
|     - |  439 | ` * When ppNames is non-NULL, string keys become named arguments: a name` |
|     - |  440 | ` * map is lazily allocated (like call_user_func_array's) with one entry` |
|     - |  441 | ` * per collected slot, empty entries meaning positional.` |
|     - |  442 | ` */` |
|    34 |  443 | `static sxi32 ReflectCollectArgs(ph7_context *pCtx, ph7_value *pArray, SySet *pOut, SyString **ppNames)` |
|     1 |  444 | `{` |
|     - |  445 | `	ph7_hashmap *pMap;` |
|     - |  446 | `	ph7_hashmap_node *pEntry;` |
|    35 |  447 | `	SyString *aNames = 0;` |
|    35 |  448 | `	sxu32 nSlot = 0;` |
|     - |  449 | `	sxu32 n;` |
|    35 |  450 | `	if( ppNames ){` |
|    29 |  451 | `		*ppNames = 0;` |
|    14 |  452 | `	}` |
|    35 |  453 | `	if( !ph7_value_is_array(pArray) ){` |
|   ! 0 |  454 | `		return SXRET_OK;` |
|     - |  455 | `	}` |
|    35 |  456 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|    35 |  457 | `	pEntry = pMap->pFirst;` |
|    85 |  458 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    51 |  459 | `		ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|    51 |  460 | `		if( pValue ){` |
|    51 |  461 | `			if( ppNames && pEntry->iType == HASHMAP_BLOB_NODE ){` |
|     5 |  462 | `				if( aNames == 0 ){` |
|     7 |  463 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     4 |  464 | `						pMap->nEntry * sizeof(SyString));` |
|     5 |  465 | `					if( aNames ){` |
|     5 |  466 | `						SyZero(aNames, pMap->nEntry * sizeof(SyString));` |
|     2 |  467 | `					}` |
|     2 |  468 | `				}` |
|     5 |  469 | `				if( aNames ){` |
|     5 |  470 | `					SyStringInitFromBuf(&aNames[nSlot],` |
|     - |  471 | `						SyBlobData(&pEntry->xKey.sKey), SyBlobLength(&pEntry->xKey.sKey));` |
|     2 |  472 | `				}` |
|     2 |  473 | `			}` |
|    51 |  474 | `			SySetPut(pOut, (const void *)&pValue);` |
|    51 |  475 | `			nSlot++;` |
|    25 |  476 | `		}` |
|    51 |  477 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|    26 |  478 | `	}` |
|    35 |  479 | `	if( ppNames ){` |
|    29 |  480 | `		*ppNames = aNames;` |
|    14 |  481 | `	}` |
|    35 |  482 | `	return SXRET_OK;` |
|    18 |  483 | `}` |
|     - |  484 | `/*` |
|     - |  485 | ` * Instantiate pClassName and run its constructor over pArgs (a PHP array;` |
|     - |  486 | `` * string keys become NAMED arguments, which is how `#[Attr(x: 1)]` arrives).`` |
|     - |  487 | ` * The object lands in the call's result slot.` |
|     - |  488 | ` *` |
|     - |  489 | ` * ReflectionAttribute::newInstance()'s back end. It is deliberately NOT the` |
|     - |  490 | ` * ReflectionClass one (ReflectNewInstance, below): php runs no instantiability` |
|     - |  491 | ` * or constructor-visibility screen here — the attribute's own #[Attribute]` |
|     - |  492 | ` * declaration is what was checked — so an abstract or private-ctor attribute` |
|     - |  493 | ` * class reaches the engine's own Error, not a ReflectionException.` |
|     - |  494 | ` */` |
|    30 |  495 | `static sxi32 ReflectAttrInstantiate(ph7_context *pCtx, ph7_value *pClassName, ph7_value *pArgs)` |
|     1 |  496 | `{` |
|    31 |  497 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  498 | `	ph7_class *pClass;` |
|     - |  499 | `	ph7_class_instance *pThis;` |
|     - |  500 | `	ph7_class_method *pCons;` |
|    31 |  501 | `	if( (pClass = ReflectResolveClass(pVm, pClassName)) == 0 ){` |
|   ! 0 |  502 | `		ph7_result_null(pCtx);` |
|   ! 0 |  503 | `		return PH7_OK;` |
|     - |  504 | `	}` |
|    31 |  505 | `	if( VmClassStaticDeferPending(pClass) ){` |
|     - |  506 | `		/* Instantiation materializes the static table (OP_NEW does it too), so a` |
|     - |  507 | `		 * broken default raises BEFORE any object exists. */` |
|   ! 0 |  508 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pVm, pClass);` |
|   ! 0 |  509 | `		if( rcMat != SXRET_OK ){` |
|   ! 0 |  510 | `			return rcMat;` |
|     - |  511 | `		}` |
|   ! 0 |  512 | `	}` |
|    31 |  513 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|    31 |  514 | `	if( pThis == 0 ){` |
|   ! 0 |  515 | `		ph7_result_null(pCtx);` |
|   ! 0 |  516 | `		return PH7_OK;` |
|     - |  517 | `	}` |
|    31 |  518 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|    31 |  519 | `	if( pCons ){` |
|     - |  520 | `		SySet aArg;` |
|     - |  521 | `		sxi32 rc;` |
|    27 |  522 | `		SyString *aNames = 0;` |
|    27 |  523 | `		SySetInit(&aArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    27 |  524 | `		if( pArgs ){` |
|    27 |  525 | `			ReflectCollectArgs(pCtx, pArgs, &aArg, &aNames);` |
|    13 |  526 | `		}` |
|    27 |  527 | `		if( aNames ){` |
|     - |  528 | `			VmCallArgMap sMap;` |
|     5 |  529 | `			SyZero(&sMap,sizeof(sMap)); /* new map fields must read unset, not stack garbage */` |
|     5 |  530 | `			sMap.bHasNamed = 1;` |
|     5 |  531 | `			sMap.bIsNamespaced = 0;` |
|     5 |  532 | `			sMap.bStrict = 0;` |
|     5 |  533 | `			sMap.nTotal = SySetUsed(&aArg);` |
|     5 |  534 | `			sMap.aNames = aNames;` |
|     7 |  535 | `			rc = PH7_VmCallClassMethodMap(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|     4 |  536 | `				(ph7_value **)SySetBasePtr(&aArg), &sMap);` |
|     5 |  537 | `			SyMemBackendFree(&pVm->sAllocator, aNames);` |
|     3 |  538 | `		}else{` |
|    34 |  539 | `			rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|    22 |  540 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|     - |  541 | `		}` |
|    27 |  542 | `		SySetRelease(&aArg);` |
|    27 |  543 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 |  544 | `			PH7_ClassInstanceUnref(pThis);` |
|   ! 0 |  545 | `			return rc;` |
|     - |  546 | `		}` |
|    13 |  547 | `	}` |
|    31 |  548 | `	return ReflectResultObject(pCtx, pThis);` |
|    16 |  549 | `}` |
|     - |  550 | `/* Where __reflect_new_no_ctor() was: the one caller was chunk 7's` |
|     - |  551 | ` * __reflect_build_attrs, which built a ReflectionAttribute and then filled it` |
|     - |  552 | ` * through a public __init() php does not have. C builds the instance itself` |
|     - |  553 | ` * (ReflectAttrNew), so both are gone. */` |
|     - |  554 | `/*` |
|     - |  555 | ` * Typed/readonly store enforcement for reflection writes. Like the VM's` |
|     - |  556 | ` * store path, except an UNINITIALIZED readonly property may be written from` |
|     - |  557 | ` * any scope (PHP lets ReflectionProperty::setValue initialize readonly): the` |
|     - |  558 | ` * READONLY bit is masked off for the enforcement call so the set-scope check` |
|     - |  559 | ` * is skipped, while an already-initialized readonly still gets PHP's` |
|     - |  560 | ` * "Cannot modify readonly property" Error. Returns SXRET_OK/PH7_EXCEPTION/` |
|     - |  561 | ` * PH7_ABORT; the value may be coerced in place.` |
|     - |  562 | ` */` |
|    14 |  563 | `static sxi32 ReflectEnforceStore(ph7_context *pCtx, sxu32 nIdx, ph7_value *pValue)` |
|     1 |  564 | `{` |
|    15 |  565 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  566 | `	SyHashEntry *pSlot;` |
|     - |  567 | `	VmClassAttr *pVmAttr;` |
|     - |  568 | `	ph7_class_attr *pAttr;` |
|     - |  569 | `	sxi32 iSaved, rc;` |
|    15 |  570 | `	pSlot = SyHashGet(&pVm->hTypedSlot, (const void *)&nIdx, sizeof(sxu32));` |
|    15 |  571 | `	if( pSlot == 0 ){` |
|     7 |  572 | `		return SXRET_OK; /* Untyped slot: plain store */` |
|     - |  573 | `	}` |
|     9 |  574 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     9 |  575 | `	pAttr = pVmAttr->pAttr;` |
|     9 |  576 | `	if( pAttr == 0 ){` |
|   ! 0 |  577 | `		return SXRET_OK;` |
|     - |  578 | `	}` |
|     9 |  579 | `	iSaved = pAttr->iFlags;` |
|     9 |  580 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) ){` |
|   ! 0 |  581 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_READONLY;` |
|   ! 0 |  582 | `	}` |
|     9 |  583 | `	rc = PH7_VmEnforcePropStore(pVm, nIdx, pValue);` |
|     9 |  584 | `	pAttr->iFlags = iSaved;` |
|     9 |  585 | `	return rc;` |
|     8 |  586 | `}` |
|     - |  587 | `/* Hand an EXISTING instance to the caller: takes an extra reference` |
|     - |  588 | ` * (unlike ReflectResultObject, which transfers a fresh instance's one). */` |
|     2 |  589 | `static int ReflectResultExistingObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|     1 |  590 | `{` |
|     3 |  591 | `	if( pObj == 0 ){` |
|   ! 0 |  592 | `		ph7_result_null(pCtx);` |
|   ! 0 |  593 | `		return PH7_OK;` |
|     - |  594 | `	}` |
|     3 |  595 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     3 |  596 | `	pObj->iRef++;` |
|     3 |  597 | `	pCtx->pRet->x.pOther = pObj;` |
|     3 |  598 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|     3 |  599 | `	return PH7_OK;` |
|     2 |  600 | `}` |
|     - |  601 | `/* pVal is a Closure instance? Return it, else NULL. */` |
|  1648 |  602 | `static ph7_class_instance * ReflectValueClosure(ph7_vm *pVm, ph7_value *pVal)` |
|     5 |  603 | `{` |
|     - |  604 | `	ph7_class_instance *pThis;` |
|  1653 |  605 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
|  1414 |  606 | `		return 0;` |
|     - |  607 | `	}` |
|   243 |  608 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|   243 |  609 | `	return (pThis->pClass == pVm->pClosureClass) ? pThis : 0;` |
|   829 |  610 | `}` |
|     - |  611 | `/*` |
|     - |  612 | ` * Resolve a reflection callable target into its compiled function.` |
|     - |  613 | ` *   - pMethodArg a non-empty string  -> method mode: pTarget is a class name` |
|     - |  614 | ` *     or object; outputs *ppClass and *ppMeth.` |
|     - |  615 | ` *   - pTarget a Closure              -> unwrap $__fn into hFunction; *ppClosure.` |
|     - |  616 | ` *   - pTarget a string               -> hFunction (user) or hHostFunction` |
|     - |  617 | ` *     (*ppHost set, returns NULL).` |
|     - |  618 | ` * Returns the ph7_vm_func, or NULL (host function or unresolvable).` |
|     - |  619 | ` */` |
|  2584 |  620 | `static ph7_vm_func * ReflectResolveCallable(ph7_vm *pVm, ph7_value *pTarget,` |
|     - |  621 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|     - |  622 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|     5 |  623 | `{` |
|     - |  624 | `	SyHashEntry *pEntry;` |
|  2589 |  625 | `	if( ppClass ){ *ppClass = 0; }` |
|  2589 |  626 | `	if( ppMeth ){ *ppMeth = 0; }` |
|  2589 |  627 | `	if( ppHost ){ *ppHost = 0; }` |
|  2589 |  628 | `	if( ppClosure ){ *ppClosure = 0; }` |
|  2589 |  629 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
|  1247 |  630 | `		ph7_class *pClass = ReflectResolveClass(pVm, pTarget);` |
|     - |  631 | `		ph7_class_method *pMeth;` |
|  1247 |  632 | `		if( pClass == 0 ){` |
|   ! 0 |  633 | `			return 0;` |
|     - |  634 | `		}` |
|  1868 |  635 | `		pMeth = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|   621 |  636 | `			SyBlobLength(&pMethodArg->sBlob));` |
|  1247 |  637 | `		if( pMeth == 0 ){` |
|     - |  638 | `			/* getMethods()/ReflectionClass reports a base class's PRIVATE methods on` |
|     - |  639 | `			 * the subclass (php copies them into the child's table), but private` |
|     - |  640 | `			 * methods are not inherited into the child's method table, so the plain` |
|     - |  641 | `			 * extract misses them when a ReflectionMethod obtained from the subclass` |
|     - |  642 | `			 * is re-resolved by (subclass, name). Walk the base chain to find the` |
|     - |  643 | `			 * declaring class's own copy. */` |
|   ! 0 |  644 | `			ph7_class *pWalk = pClass->pBase;` |
|   ! 0 |  645 | `			while( pWalk && pMeth == 0 ){` |
|   ! 0 |  646 | `				pMeth = PH7_ClassExtractMethod(pWalk, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|   ! 0 |  647 | `					SyBlobLength(&pMethodArg->sBlob));` |
|   ! 0 |  648 | `				pWalk = pWalk->pBase;` |
|   ! 0 |  649 | `			}` |
|   ! 0 |  650 | `		}` |
|  1247 |  651 | `		if( pMeth == 0 ){` |
|   ! 0 |  652 | `			return 0;` |
|     - |  653 | `		}` |
|  1247 |  654 | `		if( ppClass ){ *ppClass = pClass; }` |
|  1247 |  655 | `		if( ppMeth ){ *ppMeth = pMeth; }` |
|  1247 |  656 | `		return &pMeth->sFunc;` |
|     - |  657 | `	}` |
|     - |  658 | `	{` |
|  1347 |  659 | `		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|  1347 |  660 | `		if( pClo ){` |
|     - |  661 | `			SyString sAttr;` |
|     - |  662 | `			ph7_value *pFn;` |
|   181 |  663 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|   181 |  664 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|   181 |  665 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) < 1 ){` |
|   ! 0 |  666 | `				return 0;` |
|     - |  667 | `			}` |
|     - |  668 | `			/* A closure over an object method or __invoke object` |
|     - |  669 | `			 * (Closure::fromCallable([$obj,'m']) / fromCallable($invokeObj)) stores the` |
|     - |  670 | `			 * bare method name in $__fn and the class in $__scope. Resolve it as a METHOD` |
|     - |  671 | `			 * of $__scope FIRST — before the global function table — so a same-named` |
|     - |  672 | `			 * global function does not shadow the method (the bug: $__fn "add" hitting a` |
|     - |  673 | `			 * global add()). A plain anonymous closure's $__fn is its unique lambda name,` |
|     - |  674 | `			 * which is not a method, so this falls through cleanly. */` |
|     - |  675 | `			{` |
|     - |  676 | `				SyString sScope;` |
|     - |  677 | `				ph7_value *pScope;` |
|   181 |  678 | `				SyStringInitFromBuf(&sScope, "__scope", 7);` |
|   181 |  679 | `				pScope = PH7_ClassInstanceFetchAttr(pClo, &sScope);` |
|   181 |  680 | `				if( pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0 ){` |
|   119 |  681 | `					ph7_class *pScopeCls = PH7_VmExtractClass(pVm,` |
|    78 |  682 | `						(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|    80 |  683 | `					if( pScopeCls ){` |
|   119 |  684 | `						ph7_class_method *pScopeMeth = PH7_ClassExtractMethod(pScopeCls,` |
|    78 |  685 | `							(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    80 |  686 | `						if( pScopeMeth ){` |
|    66 |  687 | `							if( ppClass ){ *ppClass = pScopeCls; }` |
|    66 |  688 | `							if( ppMeth ){ *ppMeth = pScopeMeth; }` |
|    66 |  689 | `							if( ppClosure ){ *ppClosure = pClo; }` |
|    66 |  690 | `							return &pScopeMeth->sFunc;` |
|     - |  691 | `						}` |
|     7 |  692 | `					}` |
|     7 |  693 | `				}` |
|     - |  694 | `			}` |
|   117 |  695 | `			pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|   117 |  696 | `			if( pEntry == 0 ){` |
|     - |  697 | `				/* A Closure over a host function (Closure::fromCallable('strlen')) */` |
|     3 |  698 | `				pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|     3 |  699 | `				if( pEntry && ppHost ){` |
|     3 |  700 | `					*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|     3 |  701 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|     1 |  702 | `				}` |
|     3 |  703 | `				return 0;` |
|     - |  704 | `			}` |
|   115 |  705 | `			if( ppClosure ){ *ppClosure = pClo; }` |
|   115 |  706 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - |  707 | `		}` |
|     - |  708 | `	}` |
|  1170 |  709 | `	if( pTarget->iFlags & MEMOBJ_STRING ){` |
|  1170 |  710 | `		if( SyBlobLength(&pTarget->sBlob) < 1 ){` |
|   ! 0 |  711 | `			return 0;` |
|     - |  712 | `		}` |
|  1170 |  713 | `		pEntry = PH7_VmGetUserFunction(pVm, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob), FALSE);` |
|  1170 |  714 | `		if( pEntry ){` |
|   783 |  715 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - |  716 | `		}` |
|   389 |  717 | `		pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   389 |  718 | `		if( pEntry && ppHost ){` |
|   374 |  719 | `			*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|   186 |  720 | `		}` |
|   193 |  721 | `	}` |
|   389 |  722 | `	return 0;` |
|  1297 |  723 | `}` |
|     - |  724 | `/*` |
|     - |  725 | ` * ---------------------------------------------------------------------------` |
|     - |  726 | ` * The signature parser.` |
|     - |  727 | ` *` |
|     - |  728 | ` * A C builtin and a native method declare their parameters as ONE php-style` |
|     - |  729 | `` * string (`"string $name, ?int $len = null"`) — the aBuiltinSig[] row or the`` |
|     - |  730 | ` * PH7_NativeClassSpec's zSig — because neither has a compiled parameter list to` |
|     - |  731 | ` * read. Reflection needs that string as the same param-meta shape a compiled` |
|     - |  732 | ` * function produces, so it is parsed here and the descriptor comes out of` |
|     - |  733 | ` * __reflect_func_info() already uniform.` |
|     - |  734 | ` *` |
|     - |  735 | ` * This was chunk 8 of the reflection prelude (__reflect_sig_split /` |
|     - |  736 | ` * __reflect_sig_scalar / __reflect_parse_sig / __reflect_sig_fixup), which every` |
|     - |  737 | ` * caller had to remember to wrap around __reflect_func_info() — six call sites,` |
|     - |  738 | ` * and the one that FORGOT is why every native method reported zero parameters` |
|     - |  739 | ` * until 31 Jul. Producing the parsed form at the source removes the wrapper and` |
|     - |  740 | ` * the possibility of forgetting it.` |
|     - |  741 | ` * ---------------------------------------------------------------------------` |
|     - |  742 | ` */` |
|     - |  743 | `/* Trim ASCII spaces off both ends of [z, z+n). */` |
|  2314 |  744 | `static void ReflectSigTrim(const char **pz, int *pn)` |
|     3 |  745 | `{` |
|  2317 |  746 | `	const char *z = *pz;` |
|  2317 |  747 | `	int n = *pn;` |
|  4610 |  748 | `	while( n > 0 && (z[0] == ' ' \|\| z[0] == '\t') ){` |
|  1137 |  749 | `		z++;` |
|  1137 |  750 | `		n--;` |
|     1 |  751 | `	}` |
|  3718 |  752 | `	while( n > 0 && (z[n-1] == ' ' \|\| z[n-1] == '\t') ){` |
|   245 |  753 | `		n--;` |
|     1 |  754 | `	}` |
|  2317 |  755 | `	*pz = z;` |
|  2317 |  756 | `	*pn = n;` |
|  2317 |  757 | `}` |
|     - |  758 | ``/* Byte search that respects single-quoted runs (a default may be `'a,b'` or`` |
|     - |  759 | `` * `'it\'s'`), which is the whole reason the split is not SyByteFind. Answers`` |
|     - |  760 | ` * the offset of the first unquoted zWhat, or -1. */` |
|  4202 |  761 | `static int ReflectSigFindUnquoted(const char *z, int n, char cWhat)` |
|     4 |  762 | `{` |
|  4206 |  763 | `	int k, bQuote = 0;` |
| 55600 |  764 | `	for( k = 0 ; k < n ; k++ ){` |
| 53596 |  765 | `		if( bQuote ){` |
|   273 |  766 | `			if( z[k] == '\\' && k + 1 < n ){` |
|    17 |  767 | `				k++;` |
|   265 |  768 | `			}else if( z[k] == '\'' ){` |
|   137 |  769 | `				bQuote = 0;` |
|    69 |  770 | `			}` |
| 53460 |  771 | `		}else if( z[k] == '\'' ){` |
|   137 |  772 | `			bQuote = 1;` |
| 53256 |  773 | `		}else if( z[k] == cWhat ){` |
|  2201 |  774 | `			return k;` |
|     - |  775 | `		}` |
| 25701 |  776 | `	}` |
|  2008 |  777 | `	return -1;` |
|  2105 |  778 | `}` |
|     - |  779 | `/* Does [z,n) contain zNeedle? (case-sensitive; nNeedle > 0) */` |
|  1084 |  780 | `static int ReflectSigHas(const char *z, int n, const char *zNeedle, int nNeedle)` |
|     3 |  781 | `{` |
|     - |  782 | `	int k;` |
| 15279 |  783 | `	for( k = 0 ; k + nNeedle <= n ; k++ ){` |
| 14231 |  784 | `		if( SyMemcmp((const void *)&z[k], (const void *)zNeedle, (sxu32)nNeedle) == 0 ){` |
|    37 |  785 | `			return 1;` |
|     - |  786 | `		}` |
|  7099 |  787 | `	}` |
|  1051 |  788 | `	return 0;` |
|   545 |  789 | `}` |
|     - |  790 | ``/* Case-insensitive twin, for the `null` arm of a union type text. */`` |
|   482 |  791 | `static int ReflectSigHasNoCase(const char *z, int n, const char *zNeedle, int nNeedle)` |
|     4 |  792 | `{` |
|     - |  793 | `	int k, j;` |
|  1920 |  794 | `	for( k = 0 ; k + nNeedle <= n ; k++ ){` |
|  1512 |  795 | `		for( j = 0 ; j < nNeedle ; j++ ){` |
|  1508 |  796 | `			if( SyToLower(z[k+j]) != SyToLower(zNeedle[j]) ){` |
|  1438 |  797 | `				break;` |
|     - |  798 | `			}` |
|    37 |  799 | `		}` |
|  1442 |  800 | `		if( j == nNeedle ){` |
|     6 |  801 | `			return 1;` |
|     - |  802 | `		}` |
|   721 |  803 | `	}` |
|   482 |  804 | `	return 0;` |
|   245 |  805 | `}` |
|     - |  806 | `/*` |
|     - |  807 | `` * One `C::K` (or `C::class`) term of a declared default, evaluated.`` |
|     - |  808 | ` *` |
|     - |  809 | `` * php's stub writes these as SOURCE — `string $class = SplFileInfo::class`,`` |
|     - |  810 | `` * `int $flags = FilesystemIterator::KEY_AS_PATHNAME\|…` — and the reflector then`` |
|     - |  811 | ` * answers both the text (the export line) and the VALUE (getDefaultValue()). A` |
|     - |  812 | ` * native method's zSig is that same source, so the value has to be read out of` |
|     - |  813 | ` * the class here; there are no compiled parameter records to hold it.` |
|     - |  814 | ` */` |
|     4 |  815 | `static int ReflectSigClassConst(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|     1 |  816 | `{` |
|     5 |  817 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  818 | `	ph7_class_attr *pAttr;` |
|     - |  819 | `	ph7_class *pClass;` |
|     - |  820 | `	ph7_value *pValue;` |
|     - |  821 | `	int iSep;` |
|     5 |  822 | `	ReflectSigTrim(&z, &n);` |
|    45 |  823 | `	for( iSep = 0 ; iSep + 1 < n ; ++iSep ){` |
|    45 |  824 | `		if( z[iSep] == ':' && z[iSep+1] == ':' ){` |
|     5 |  825 | `			break;` |
|     - |  826 | `		}` |
|    21 |  827 | `	}` |
|     5 |  828 | `	if( iSep + 1 >= n \|\| iSep < 1 ){` |
|   ! 0 |  829 | `		return 0;` |
|     - |  830 | `	}` |
|     5 |  831 | `	pClass = PH7_VmExtractClass(pVm, z, (sxu32)iSep, TRUE, 0);` |
|     5 |  832 | `	if( pClass == 0 ){` |
|   ! 0 |  833 | `		return 0;` |
|     - |  834 | `	}` |
|     5 |  835 | `	z += iSep + 2;` |
|     5 |  836 | `	n -= iSep + 2;` |
|     5 |  837 | `	if( n == (int)sizeof("class")-1 && SyMemcmp(z, "class", sizeof("class")-1) == 0 ){` |
|     - |  838 | ``		/* `C::class` is the class NAME, and php prints the name it was DECLARED`` |
|     - |  839 | `		 * with rather than the spelling in the signature. */` |
|     3 |  840 | `		SyString *pName = &pClass->sName;` |
|     3 |  841 | `		ph7_value_string(pOut, SyStringData(pName), (int)SyStringLength(pName));` |
|     3 |  842 | `		return 1;` |
|     - |  843 | `	}` |
|     3 |  844 | `	pAttr = PH7_ClassExtractConstant(pClass, z, (sxu32)n);` |
|     3 |  845 | `	if( pAttr == 0 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|   ! 0 |  846 | `		return 0;` |
|     - |  847 | `	}` |
|     3 |  848 | `	if( pAttr->nIdx == SXU32_HIGH ){` |
|     - |  849 | `` 		/* Not materialized yet: run the initializer, exactly as a direct `C::K` `` |
|     - |  850 | `		 * read would (an unread native constant is a literal waiting on this). */` |
|     3 |  851 | `		if( VmClassConstEvalOnDemand(pVm, pClass, pAttr) != SXRET_OK ){` |
|   ! 0 |  852 | `			return 0;` |
|     - |  853 | `		}` |
|     1 |  854 | `	}` |
|     3 |  855 | `	pValue = (ph7_value *)SySetAt(&pVm->aMemObj, pAttr->nIdx);` |
|     3 |  856 | `	if( pValue == 0 ){` |
|   ! 0 |  857 | `		return 0;` |
|     - |  858 | `	}` |
|     3 |  859 | `	PH7_MemObjStore(pValue, pOut);` |
|     3 |  860 | `	return 1;` |
|     3 |  861 | `}` |
|     - |  862 | `/*` |
|     - |  863 | ` * A GLOBAL constant named by a declared default — php's stub writes` |
|     - |  864 | `` * `int $severity = E_ERROR` and both surfaces read it from here: the export`` |
|     - |  865 | ` * prints the NAME (rule 28: the argument was never folded, so php still has the` |
|     - |  866 | ` * source) and getDefaultValue() answers what it expands to. Answers 0 for` |
|     - |  867 | ` * anything that is not a plain identifier naming a defined constant, which is` |
|     - |  868 | `` * what keeps `null`/`true`/a bare number out of this branch.`` |
|     - |  869 | ` */` |
|    26 |  870 | `static int ReflectSigIsIdent(const char *z, int n)` |
|     1 |  871 | `{` |
|     - |  872 | `	int k;` |
|    27 |  873 | `	if( n < 1 \|\| (z[0] != '_' && !SyisAlpha(z[0])) ){` |
|    21 |  874 | `		return 0;` |
|     - |  875 | `	}` |
|    31 |  876 | `	for( k = 1 ; k < n ; ++k ){` |
|    25 |  877 | `		if( z[k] != '_' && z[k] != '\\' && !SyisAlphaNum(z[k]) ){` |
|   ! 0 |  878 | `			return 0;` |
|     - |  879 | `		}` |
|    13 |  880 | `	}` |
|     7 |  881 | `	return 1;` |
|    14 |  882 | `}` |
|    26 |  883 | `static int ReflectSigGlobalConst(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|     1 |  884 | `{` |
|     - |  885 | `	SyHashEntry *pEntry;` |
|     - |  886 | `	ph7_constant *pCons;` |
|    27 |  887 | `	ReflectSigTrim(&z, &n);` |
|    27 |  888 | `	if( !ReflectSigIsIdent(z, n) ){` |
|    21 |  889 | `		return 0;` |
|     - |  890 | `	}` |
|     7 |  891 | `	pEntry = SyHashGet(&pCtx->pVm->hConstant, (const void *)z, (sxu32)n);` |
|     7 |  892 | `	if( pEntry == 0 ){` |
|     5 |  893 | `		return 0;` |
|     - |  894 | `	}` |
|     3 |  895 | `	pCons = (ph7_constant *)pEntry->pUserData;` |
|     3 |  896 | `	if( pCons == 0 \|\| pCons->xExpand == 0 ){` |
|   ! 0 |  897 | `		return 0;` |
|     - |  898 | `	}` |
|     3 |  899 | `	pCons->xExpand(pOut, pCons->pUserData);` |
|     3 |  900 | `	return 1;` |
|    14 |  901 | `}` |
|     - |  902 | `/*` |
|     - |  903 | `` * A class-constant EXPRESSION: one term, or the `\|` fold php's own stubs write`` |
|     - |  904 | `` * for a flags default (`KEY_AS_PATHNAME \| CURRENT_AS_FILEINFO \| SKIP_DOTS`).`` |
|     - |  905 | ` */` |
|    16 |  906 | `static int ReflectSigConstExpr(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|     1 |  907 | `{` |
|    17 |  908 | `	sxi64 iAcc = 0;` |
|    17 |  909 | `	int iStart = 0;` |
|    17 |  910 | `	int k, nTerm = 0;` |
|    17 |  911 | `	if( !ReflectSigHas(z, n, "::", 2) ){` |
|    13 |  912 | `		return 0;` |
|     - |  913 | `	}` |
|    87 |  914 | `	for( k = 0 ; k <= n ; ++k ){` |
|    83 |  915 | `		if( k < n && z[k] != '\|' ){` |
|    79 |  916 | `			continue;` |
|     - |  917 | `		}` |
|     5 |  918 | `		if( !ReflectSigClassConst(pCtx, &z[iStart], k - iStart, pOut) ){` |
|   ! 0 |  919 | `			return 0;` |
|     - |  920 | `		}` |
|     5 |  921 | `		nTerm++;` |
|     5 |  922 | `		if( k < n \|\| nTerm > 1 ){` |
|     - |  923 | `			/* A fold is arithmetic, so every arm has to be an integer; a lone` |
|     - |  924 | ``			 * term keeps whatever type it had (`C::class` is a string). */`` |
|   ! 0 |  925 | `			if( (pOut->iFlags & (MEMOBJ_STRING\|MEMOBJ_HASHMAP\|MEMOBJ_OBJ)) != 0 ){` |
|   ! 0 |  926 | `				return 0;` |
|     - |  927 | `			}` |
|   ! 0 |  928 | `			PH7_MemObjToInteger(pOut);` |
|   ! 0 |  929 | `			iAcc \|= pOut->x.iVal;` |
|   ! 0 |  930 | `		}` |
|     5 |  931 | `		iStart = k + 1;` |
|     3 |  932 | `	}` |
|     5 |  933 | `	if( nTerm > 1 ){` |
|   ! 0 |  934 | `		ph7_value_int64(pOut, iAcc);` |
|   ! 0 |  935 | `	}` |
|     5 |  936 | `	return nTerm > 0;` |
|     9 |  937 | `}` |
|     - |  938 | `/*` |
|     - |  939 | ` * A default-value TEXT to a value, when the text denotes a scalar php can` |
|     - |  940 | `` * reproduce. Answers 1 and fills pOut, or 0 for anything else (`[]`,`` |
|     - |  941 | `` * `array (`, a constant name) which the caller reports its own way.`` |
|     - |  942 | ` */` |
|    44 |  943 | `static int ReflectSigScalar(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)` |
|     1 |  944 | `{` |
|    45 |  945 | `	sxu8 bReal = 0;` |
|    45 |  946 | `	if( n == 1 && z[0] == '?' ){` |
|   ! 0 |  947 | `		return 0;` |
|     - |  948 | `	}` |
|    45 |  949 | `	if( (n == 4 && (SyMemcmp(z,"NULL",4) == 0 \|\| SyMemcmp(z,"null",4) == 0)) ){` |
|    17 |  950 | `		ph7_value_null(pOut);` |
|    17 |  951 | `		return 1;` |
|     - |  952 | `	}` |
|    29 |  953 | `	if( n == 4 && SyMemcmp(z,"true",4) == 0 ){` |
|   ! 0 |  954 | `		ph7_value_bool(pOut,1);` |
|   ! 0 |  955 | `		return 1;` |
|     - |  956 | `	}` |
|    29 |  957 | `	if( n == 5 && SyMemcmp(z,"false",5) == 0 ){` |
|   ! 0 |  958 | `		ph7_value_bool(pOut,0);` |
|   ! 0 |  959 | `		return 1;` |
|     - |  960 | `	}` |
|    29 |  961 | `	if( n >= 2 && (z[0] == '\'' \|\| z[0] == '"') && z[n-1] == z[0] ){` |
|     - |  962 | `		/* Unescape the quote character and \\ , the only two escapes the signature` |
|     - |  963 | `		 * writer emits. BOTH quote spellings are accepted because both scanners in` |
|     - |  964 | `		 * vm_arg_check.c step over either one: a native method's zSig lives in C, so` |
|     - |  965 | ``		 * `= \"static\"` is the natural way to write Closure::bindTo's default and it`` |
|     - |  966 | ``		 * used to reduce to php's `<default>` placeholder here. */`` |
|     - |  967 | `		SyBlob sOut;` |
|    13 |  968 | `		char cQuote = z[0];` |
|     - |  969 | `		int k;` |
|    13 |  970 | `		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);` |
|    31 |  971 | `		for( k = 1 ; k < n - 1 ; k++ ){` |
|    19 |  972 | `			if( z[k] == '\\' && k + 1 < n - 1 && (z[k+1] == cQuote \|\| z[k+1] == '\\') ){` |
|     3 |  973 | `				k++;` |
|     1 |  974 | `			}` |
|    19 |  975 | `			SyBlobAppend(&sOut,(const void *)&z[k],sizeof(char));` |
|    10 |  976 | `		}` |
|    13 |  977 | `		ph7_value_string(pOut,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));` |
|    13 |  978 | `		SyBlobRelease(&sOut);` |
|    13 |  979 | `		return 1;` |
|     - |  980 | `	}` |
|    17 |  981 | `	if( ReflectSigConstExpr(pCtx,z,n,pOut) ){` |
|     5 |  982 | `		return 1;` |
|     - |  983 | `	}` |
|    13 |  984 | `	if( ReflectSigGlobalConst(pCtx,z,n,pOut) ){` |
|     3 |  985 | `		return 1;` |
|     - |  986 | `	}` |
|    11 |  987 | `	if( n > 0 && SyStrIsNumeric(z,(sxu32)n,&bReal,0) == SXRET_OK ){` |
|     - |  988 | `		/* php's own rule for the text form: a '.', an exponent or a hex marker` |
|     - |  989 | `		 * makes it a float, everything else an int. */` |
|     8 |  990 | `		if( bReal \|\| ReflectSigHas(z,n,".",1)` |
|     9 |  991 | `		 \|\| ReflectSigHasNoCase(z,n,"e",1) \|\| ReflectSigHasNoCase(z,n,"x",1) ){` |
|     - |  992 | `#ifndef PH7_OMIT_FLOATING_POINT` |
|   ! 0 |  993 | `			ph7_value_double(pOut,SyStrToReal(z,(sxu32)n,0,0));` |
|     - |  994 | `#else` |
|     - |  995 | `			ph7_value_int64(pOut,SyStrToInt64(z,(sxu32)n,0,0));` |
|     - |  996 | `#endif` |
|   ! 0 |  997 | `		}else{` |
|     9 |  998 | `			sxi64 iVal = 0;` |
|     9 |  999 | `			SyStrToInt64(z,(sxu32)n,(void *)&iVal,0);` |
|     9 | 1000 | `			ph7_value_int64(pOut,iVal);` |
|     - | 1001 | `		}` |
|     9 | 1002 | `		return 1;` |
|     - | 1003 | `	}` |
|     3 | 1004 | `	return 0;` |
|    23 | 1005 | `}` |
|     - | 1006 | `/*` |
|     - | 1007 | ` * One parameter, described uniformly.` |
|     - | 1008 | ` *` |
|     - | 1009 | ` * A reflected function's parameters come from one of TWO places — a compiled` |
|     - | 1010 | `` * function's `ph7_vm_func_arg` list, or the php-style SIGNATURE STRING a C`` |
|     - | 1011 | ` * builtin / native method declares — and both the descriptor array` |
|     - | 1012 | ` * (__reflect_func_info, for the chunks still in PHP) and the native` |
|     - | 1013 | ` * ReflectionParameter have to read them the same way. This struct is what they` |
|     - | 1014 | `` * agree on; `pArg` is set only on the compiled path, where a default is`` |
|     - | 1015 | ` * BYTE-CODE rather than text.` |
|     - | 1016 | ` */` |
|     - | 1017 | `typedef struct ReflectParamDesc ReflectParamDesc;` |
|     - | 1018 | `struct ReflectParamDesc` |
|     - | 1019 | `{` |
|     - | 1020 | `	SyString sName;` |
|     - | 1021 | `	int iPos;` |
|     - | 1022 | `	int bByRef, bVariadic, bHasDef, bOptional, bNullable, bPromoted;` |
|     - | 1023 | `	SyString sType;            /* nByte == 0 -> untyped */` |
|     - | 1024 | `	SyString sDefText;         /* signature-declared default TEXT (nByte == 0 -> none) */` |
|     - | 1025 | `	ph7_vm_func_arg *pArg;     /* compiled parameter, or NULL for a declared one */` |
|     - | 1026 | `};` |
|     - | 1027 | `/*` |
|     - | 1028 | ` * Split a signature string on its top-level commas (a quoted default may hold` |
|     - | 1029 | ` * its own). Answers the parameter COUNT; when iWant is in range, hands back` |
|     - | 1030 | ` * that part's bytes.` |
|     - | 1031 | ` */` |
|   992 | 1032 | `static int ReflectSigPart(const char *zSig, int nSig, int iWant,` |
|     - | 1033 | `	const char **pzPart, int *pnPart)` |
|     3 | 1034 | `{` |
|   995 | 1035 | `	int iPos = 0;` |
|  1887 | 1036 | `	while( nSig > 0 ){` |
|  1799 | 1037 | `		int iComma = ReflectSigFindUnquoted(zSig,nSig,',');` |
|  1799 | 1038 | `		const char *zPart = zSig;` |
|  1799 | 1039 | `		int nPart = iComma < 0 ? nSig : iComma;` |
|  1799 | 1040 | `		ReflectSigTrim(&zPart,&nPart);` |
|  1799 | 1041 | `		if( nPart > 0 ){` |
|  1799 | 1042 | `			if( iPos == iWant && pzPart ){` |
|   525 | 1043 | `				*pzPart = zPart;` |
|   525 | 1044 | `				*pnPart = nPart;` |
|   261 | 1045 | `			}` |
|  1799 | 1046 | `			iPos++;` |
|   898 | 1047 | `		}` |
|  1799 | 1048 | `		if( iComma < 0 ){` |
|   907 | 1049 | `			break;` |
|     - | 1050 | `		}` |
|   893 | 1051 | `		zSig += iComma + 1;` |
|   893 | 1052 | `		nSig -= iComma + 1;` |
|     1 | 1053 | `	}` |
|   995 | 1054 | `	return iPos;` |
|     3 | 1055 | `}` |
|     - | 1056 | ``/* One `type &$name = default` part into the uniform description. */`` |
|   522 | 1057 | `static void ReflectSigDescribe(const char *z, int n, int iPos, ReflectParamDesc *pOut)` |
|     3 | 1058 | `{` |
|   525 | 1059 | `	const char *zDef = 0;` |
|   525 | 1060 | `	int nDef = 0;` |
|     - | 1061 | `	int iEq, iDollar, iSpace;` |
|   525 | 1062 | `	SyZero(pOut,sizeof(*pOut));` |
|   525 | 1063 | `	pOut->iPos = iPos;` |
|     - | 1064 | ``	/* `= default` splits off first: everything after the first unquoted '='. */`` |
|   525 | 1065 | `	iEq = ReflectSigFindUnquoted(z,n,'=');` |
|   525 | 1066 | `	if( iEq >= 0 ){` |
|   245 | 1067 | `		zDef = &z[iEq+1];` |
|   245 | 1068 | `		nDef = n - iEq - 1;` |
|   245 | 1069 | `		ReflectSigTrim(&zDef,&nDef);` |
|   245 | 1070 | `		n = iEq;` |
|   245 | 1071 | `		ReflectSigTrim(&z,&n);` |
|   122 | 1072 | `	}` |
|   525 | 1073 | `	if( zDef && nDef == 1 && zDef[0] == '?' ){` |
|     - | 1074 | ``		/* `= ?` is the table's OPTIONAL-but-no-default marker, php's own shape`` |
|     - | 1075 | `		 * for a parameter like ReflectionClass::getStaticPropertyValue()'s` |
|     - | 1076 | `		 * $default: isOptional() true, isDefaultValueAvailable() FALSE, so` |
|     - | 1077 | `		 * getDefaultValue() raises. Reporting it as a default (which is what` |
|     - | 1078 | ``		 * a bare `hasdef` did) made that call answer NULL instead. */`` |
|    43 | 1079 | `		pOut->bOptional = 1;` |
|    43 | 1080 | `		zDef = 0;` |
|    43 | 1081 | `		nDef = 0;` |
|    21 | 1082 | `	}` |
|   525 | 1083 | `	pOut->bVariadic = ReflectSigHas(z,n,"...",3);` |
|   525 | 1084 | `	if( pOut->bVariadic ){` |
|     - | 1085 | `		/* php: a variadic parameter never HAS a default -- it defaults to "no` |
|     - | 1086 | `		 * further arguments", which is not a value. Several signature rows still` |
|     - | 1087 | ``		 * write `mixed ...$values = ?` (the table's "optional, unspecified"`` |
|     - | 1088 | `		 * marker), and honouring it made isDefaultValueAvailable() true where php` |
|     - | 1089 | `		 * says false, so getDefaultValue() then threw on printf/array_merge. */` |
|    25 | 1090 | `		zDef = 0;` |
|    25 | 1091 | `		nDef = 0;` |
|    12 | 1092 | `	}` |
|   525 | 1093 | `	iDollar = ReflectSigFindUnquoted(z,n,'$');` |
|   525 | 1094 | `	iSpace = ReflectSigFindUnquoted(z,n,' ');` |
|     - | 1095 | `	{` |
|   525 | 1096 | `		const char *zName = iDollar < 0 ? z : &z[iDollar+1];` |
|   525 | 1097 | `		int nName = iDollar < 0 ? n : n - iDollar - 1;` |
|   525 | 1098 | `		SyStringInitFromBuf(&pOut->sName,zName,nName);` |
|     - | 1099 | `	}` |
|   525 | 1100 | `	pOut->bByRef = ReflectSigHas(z,n,"&",1);` |
|   525 | 1101 | `	pOut->bHasDef = zDef != 0;` |
|   525 | 1102 | `	pOut->bOptional = pOut->bOptional \|\| pOut->bVariadic \|\| zDef != 0;` |
|   525 | 1103 | `	if( iSpace >= 0 && iDollar >= 0 && iSpace < iDollar ){` |
|     - | 1104 | `` 		/* The type is whatever precedes the first space, so `?DOMNode $child` `` |
|     - | 1105 | ``		 * types as `?DOMNode` and an untyped `$x` types as nothing. */`` |
|   519 | 1106 | `		pOut->bNullable = (z[0] == '?' \|\| ReflectSigHasNoCase(z,iSpace,"null",4));` |
|   519 | 1107 | `		SyStringInitFromBuf(&pOut->sType,z,iSpace);` |
|   258 | 1108 | `	}` |
|   525 | 1109 | `	if( zDef ){` |
|   203 | 1110 | `		SyStringInitFromBuf(&pOut->sDefText,zDef,nDef);` |
|   101 | 1111 | `	}` |
|   525 | 1112 | `}` |
|     - | 1113 | `/*` |
|     - | 1114 | ` * Resolve a Generator object into its wrapper. Mirrors the static` |
|     - | 1115 | ` * VmGeneratorExtractCtx in vm.c: the $__ctx attribute carries the` |
|     - | 1116 | ` * ph7_generator pointer as a resource value.` |
|     - | 1117 | ` */` |
|    42 | 1118 | `static ph7_generator * ReflectGeneratorCtx(ph7_vm *pVm, ph7_value *pVal)` |
|     1 | 1119 | `{` |
|     - | 1120 | `	ph7_class_instance *pThis;` |
|     - | 1121 | `	ph7_value *pAttr;` |
|     - | 1122 | `	SyString sAttr;` |
|    43 | 1123 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVm->pGeneratorClass == 0 ){` |
|   ! 0 | 1124 | `		return 0;` |
|     - | 1125 | `	}` |
|    43 | 1126 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    43 | 1127 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|   ! 0 | 1128 | `		return 0;` |
|     - | 1129 | `	}` |
|    43 | 1130 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    43 | 1131 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    43 | 1132 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|   ! 0 | 1133 | `		return 0;` |
|     - | 1134 | `	}` |
|    43 | 1135 | `	return (ph7_generator *)pAttr->x.pOther;` |
|    22 | 1136 | `}` |
|     - | 1137 | `/*` |
|     - | 1138 | ` * Evaluate the recorded argument expressions of one declared attribute and` |
|     - | 1139 | ` * answer them as a PHP array, or NULL when the target no longer resolves.` |
|     - | 1140 | ` *` |
|     - | 1141 | ` * apArg is the four-part SPEC a ReflectionAttribute carries — kind 'class'` |
|     - | 1142 | ` * (target = class), 'attr' (class + property/constant name), 'method' (class +` |
|     - | 1143 | ` * method), 'fn' (function name or Closure), 'param' (function spec + parameter` |
|     - | 1144 | ` * index), 'const' (global constant name) — plus which attribute of that target.` |
|     - | 1145 | ` * Named arguments become string keys.` |
|     - | 1146 | ` *` |
|     - | 1147 | ` * The values are evaluated HERE rather than when the reflector was built:` |
|     - | 1148 | `` * `#[Attr(self::SOME)]` runs php code, and php runs it at getArguments() time.`` |
|     - | 1149 | ` */` |
|   112 | 1150 | `static ph7_value * ReflectAttrArgs(ph7_context *pCtx, ph7_value **apArg, sxu32 nAttrIdx)` |
|     1 | 1151 | `{` |
|   113 | 1152 | `	ph7_vm *pVm = pCtx->pVm;` |
|   113 | 1153 | `	SySet *pAttrs = 0;` |
|   113 | 1154 | `	ph7_class *pDeclCls = 0; /* class an attribute is declared on: scope for self:: in its args */` |
|     - | 1155 | `	ph7_attribute *pAttrRec;` |
|     - | 1156 | `	ph7_value *pOut;` |
|     - | 1157 | `	const char *zKind;` |
|     - | 1158 | `	int nKind;` |
|     - | 1159 | `	sxu32 n;` |
|   113 | 1160 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|   156 | 1161 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|    87 | 1162 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|    87 | 1163 | `		if( pClass ){ pAttrs = &pClass->aAttrs; pDeclCls = pClass; }` |
|    72 | 1164 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|     5 | 1165 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|     5 | 1166 | `		ph7_class_attr *pMember = pClass ? ReflectFetchMember(pClass, apArg[2]) : 0;` |
|     5 | 1167 | `		if( pMember ){ pAttrs = &pMember->aAttrs; pDeclCls = pClass; }` |
|    30 | 1168 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|    11 | 1169 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], apArg[2], 0, 0, 0, 0);` |
|    11 | 1170 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|    22 | 1171 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|     9 | 1172 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], 0, 0, 0, 0, 0);` |
|     9 | 1173 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|    10 | 1174 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|     3 | 1175 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1176 | `		ph7_vm_func_arg *pParam = pFunc` |
|     2 | 1177 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|     3 | 1178 | `		if( pParam ){ pAttrs = &pParam->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|     4 | 1179 | `	}else if( nKind == 5 && SyMemcmp(zKind, "const", 5) == 0 ){` |
|     - | 1180 | ``		/* Global constant (php 8.5 attributes on `const` statements) */`` |
|     - | 1181 | `		const char *zCName;` |
|     - | 1182 | `		int nCName;` |
|     - | 1183 | `		SyHashEntry *pCEntry;` |
|     3 | 1184 | `		zCName = ph7_value_to_string(apArg[1], &nCName);` |
|     3 | 1185 | `		pCEntry = nCName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zCName, (sxu32)nCName) : 0;` |
|     3 | 1186 | `		if( pCEntry ){ pAttrs = &((ph7_constant *)pCEntry->pUserData)->aAttrs; }` |
|     1 | 1187 | `	}` |
|   112 | 1188 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|   113 | 1189 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 | 1190 | `		return 0;` |
|     - | 1191 | `	}` |
|   267 | 1192 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|   155 | 1193 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|     - | 1194 | `		ph7_value sValue;` |
|   155 | 1195 | `		PH7_MemObjInit(pVm, &sValue);` |
|   155 | 1196 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|     - | 1197 | `` 			/* Evaluate under the attribute's declaring-class scope so `self::class` `` |
|     - | 1198 | ``			 * / `self::CONST` / `parent::` resolve like php (else self:: would bind`` |
|     - | 1199 | `			 * to the reflection machinery's own class). */` |
|   141 | 1200 | `			PH7_VmExecAttrArg(pVm, &pArgRec->aByteCode, pDeclCls, &sValue);` |
|    85 | 1201 | `		}else if( pArgRec->pNativeValue ){` |
|     - | 1202 | ``			/* A NATIVE attribute (php's own `#[Attribute(...)]` on Attribute and`` |
|     - | 1203 | `			 * Deprecated): the argument is a literal, not byte-code. */` |
|    15 | 1204 | `			PH7_NativeLiteralValue(pVm, pArgRec->pNativeValue, &sValue);` |
|     7 | 1205 | `		}` |
|   155 | 1206 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|    13 | 1207 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|     7 | 1208 | `		}else{` |
|   143 | 1209 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|     - | 1210 | `		}` |
|   155 | 1211 | `		PH7_MemObjRelease(&sValue);` |
|    78 | 1212 | `	}` |
|   113 | 1213 | `	return pOut;` |
|    57 | 1214 | `}` |
|     - | 1215 | `/*` |
|     - | 1216 | ` * ---------------------------------------------------------------------------` |
|     - | 1217 | ` * The ReflectionType family.` |
|     - | 1218 | ` *` |
|     - | 1219 | ` * php's four type objects are pure VALUES: a text, a nullability flag, and for` |
|     - | 1220 | ` * the composites a list of members. Nothing in userland can build one — php` |
|     - | 1221 | `` * declares no constructor on any of them and refuses `clone` — so the prelude's`` |
|     - | 1222 | `` * public `__construct($name, $nullable, $text)` existed only because the factory`` |
|     - | 1223 | ` * that fills them was itself PHP. The factory is C now (ReflectMakeType), so the` |
|     - | 1224 | ` * constructors are gone and the classes say what php's say.` |
|     - | 1225 | ` * ---------------------------------------------------------------------------` |
|     - | 1226 | ` */` |
|     - | 1227 | `#define RT_TEXT     "__text"` |
|     - | 1228 | `#define RT_NULLABLE "__nullable"` |
|     - | 1229 | `#define RT_TNAME    "__tname"` |
|     - | 1230 | `#define RT_TYPES    "__types"` |
|     - | 1231 |  |
|    26 | 1232 | `static int vm_builtin_ReflectionType_allowsNull(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 1233 | `{` |
|    28 | 1234 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    13 | 1235 | `	SXUNUSED(nArg);` |
|    13 | 1236 | `	SXUNUSED(apArg);` |
|    28 | 1237 | `	ph7_result_bool(pCtx, pThis != 0 && PH7_NativeAttrTruthy(pThis, RT_NULLABLE));` |
|    28 | 1238 | `	return PH7_OK;` |
|     2 | 1239 | `}` |
|   386 | 1240 | `static int vm_builtin_ReflectionType_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     4 | 1241 | `{` |
|   390 | 1242 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   390 | 1243 | `	const char *zText = "";` |
|   390 | 1244 | `	int nText = 0;` |
|   193 | 1245 | `	SXUNUSED(nArg);` |
|   193 | 1246 | `	SXUNUSED(apArg);` |
|   390 | 1247 | `	if( pThis ){` |
|   390 | 1248 | `		PH7_NativeAttrStr(pThis, RT_TEXT, &zText, &nText);` |
|   193 | 1249 | `	}` |
|   390 | 1250 | `	ph7_result_string(pCtx, zText, nText);` |
|   390 | 1251 | `	return PH7_OK;` |
|     4 | 1252 | `}` |
|    20 | 1253 | `static int vm_builtin_ReflectionNamedType_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 1254 | `{` |
|    23 | 1255 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    23 | 1256 | `	const char *zName = "";` |
|    23 | 1257 | `	int nName = 0;` |
|    10 | 1258 | `	SXUNUSED(nArg);` |
|    10 | 1259 | `	SXUNUSED(apArg);` |
|    23 | 1260 | `	if( pThis ){` |
|    23 | 1261 | `		PH7_NativeAttrStr(pThis, RT_TNAME, &zName, &nName);` |
|    10 | 1262 | `	}` |
|    23 | 1263 | `	ph7_result_string(pCtx, zName, nName);` |
|    23 | 1264 | `	return PH7_OK;` |
|     3 | 1265 | `}` |
|     - | 1266 | `/* php's builtin-type set, case-insensitively. Anything else is a class name. */` |
|    32 | 1267 | `static int ReflectTypeIsBuiltin(const char *zName, int nName)` |
|     1 | 1268 | `{` |
|     - | 1269 | `	static const char *azBuiltin[] = {` |
|     - | 1270 | `		"int","float","string","bool","array","object","mixed",` |
|     - | 1271 | `		"void","never","null","callable","iterable","true","false"` |
|     - | 1272 | `	};` |
|     - | 1273 | `	sxu32 n;` |
|   245 | 1274 | `	for( n = 0 ; n < SX_ARRAYSIZE(azBuiltin) ; n++ ){` |
|   235 | 1275 | `		int nWant = (int)SyStrlen(azBuiltin[n]);` |
|     - | 1276 | `		int k;` |
|   235 | 1277 | `		if( nWant != nName ){` |
|   199 | 1278 | `			continue;` |
|     - | 1279 | `		}` |
|   147 | 1280 | `		for( k = 0 ; k < nWant ; k++ ){` |
|   125 | 1281 | `			if( SyToLower(zName[k]) != azBuiltin[n][k] ){` |
|    15 | 1282 | `				break;` |
|     - | 1283 | `			}` |
|    56 | 1284 | `		}` |
|    37 | 1285 | `		if( k == nWant ){` |
|    23 | 1286 | `			return 1;` |
|     - | 1287 | `		}` |
|     8 | 1288 | `	}` |
|    11 | 1289 | `	return 0;` |
|    17 | 1290 | `}` |
|    24 | 1291 | `static int vm_builtin_ReflectionNamedType_isBuiltin(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1292 | `{` |
|    25 | 1293 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    25 | 1294 | `	const char *zName = "";` |
|    25 | 1295 | `	int nName = 0;` |
|    12 | 1296 | `	SXUNUSED(nArg);` |
|    12 | 1297 | `	SXUNUSED(apArg);` |
|    25 | 1298 | `	if( pThis ){` |
|    25 | 1299 | `		PH7_NativeAttrStr(pThis, RT_TNAME, &zName, &nName);` |
|    12 | 1300 | `	}` |
|    25 | 1301 | `	ph7_result_bool(pCtx, ReflectTypeIsBuiltin(zName, nName));` |
|    25 | 1302 | `	return PH7_OK;` |
|     1 | 1303 | `}` |
|     6 | 1304 | `static int vm_builtin_ReflectionType_getTypes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1305 | `{` |
|     7 | 1306 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     7 | 1307 | `	ph7_value *pTypes = pThis ? PH7_NativeAttr(pThis, RT_TYPES) : 0;` |
|     3 | 1308 | `	SXUNUSED(nArg);` |
|     3 | 1309 | `	SXUNUSED(apArg);` |
|     7 | 1310 | `	if( pTypes && (pTypes->iFlags & MEMOBJ_HASHMAP) ){` |
|     7 | 1311 | `		ph7_result_value(pCtx, pTypes);` |
|     4 | 1312 | `	}else{` |
|     - | 1313 | `		/* The declared default is a native NULL slot (a spec cannot carry an` |
|     - | 1314 | `		 * array literal), but php's getTypes() always answers a list. */` |
|   ! 0 | 1315 | `		ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|   ! 0 | 1316 | `		if( pEmpty ){` |
|   ! 0 | 1317 | `			ph7_result_value(pCtx, pEmpty);` |
|   ! 0 | 1318 | `		}` |
|     - | 1319 | `	}` |
|     7 | 1320 | `	return PH7_OK;` |
|     1 | 1321 | `}` |
|     - | 1322 | `/* Build one of the three concrete types with its slots filled. The caller owns` |
|     - | 1323 | ` * the reference (PH7_NativeResultObject / a list insert drops it). */` |
|   612 | 1324 | `static ph7_class_instance * ReflectNewType(ph7_context *pCtx, const char *zClass,` |
|     - | 1325 | `	const char *zText, int nText, int bNullable)` |
|     4 | 1326 | `{` |
|   616 | 1327 | `	ph7_vm *pVm = pCtx->pVm;` |
|   616 | 1328 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|   616 | 1329 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|   616 | 1330 | `	if( pObj == 0 ){` |
|   ! 0 | 1331 | `		return 0;` |
|     - | 1332 | `	}` |
|   616 | 1333 | `	PH7_NativeSetAttrStr(pVm, pObj, RT_TEXT, zText, nText);` |
|   616 | 1334 | `	PH7_NativeSetAttrBool(pVm, pObj, RT_NULLABLE, bNullable);` |
|   616 | 1335 | `	return pObj;` |
|   310 | 1336 | `}` |
|     - | 1337 | `/*` |
|     - | 1338 | ` * Append pType to a composite's member list, handing over the caller's reference.` |
|     - | 1339 | ` *` |
|     - | 1340 | ` * The carrier is a STACK value, deliberately: a ph7_context_new_scalar() is` |
|     - | 1341 | ` * released with the call context, and releasing a MEMOBJ_OBJ carrier unrefs the` |
|     - | 1342 | ` * instance a second time — which freed every member of a union or intersection` |
|     - | 1343 | ` * the moment its factory returned, so the list came back holding dead objects.` |
|     - | 1344 | ` * PH7_NativeResultObject hands an instance over the same way.` |
|     - | 1345 | ` */` |
|   354 | 1346 | `static void ReflectTypeListAdd(ph7_context *pCtx, ph7_value *pList, ph7_class_instance *pType)` |
|     4 | 1347 | `{` |
|     - | 1348 | `	ph7_value sVal;` |
|   358 | 1349 | `	if( pType == 0 ){` |
|   ! 0 | 1350 | `		return;` |
|     - | 1351 | `	}` |
|   358 | 1352 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|   358 | 1353 | `	sVal.x.pOther = pType;` |
|   358 | 1354 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|   358 | 1355 | `	ph7_array_add_elem(pList, 0, &sVal);   /* takes its own reference */` |
|   358 | 1356 | `	PH7_ClassInstanceUnref(pType);` |
|   181 | 1357 | `}` |
|     - | 1358 | `/* Exact, case-insensitive name test (php lower-cases before comparing). */` |
|   942 | 1359 | `static int ReflectTypeNameIs(const char *z, int n, const char *zWant)` |
|     4 | 1360 | `{` |
|   946 | 1361 | `	int nWant = (int)SyStrlen(zWant), k;` |
|   946 | 1362 | `	if( n != nWant ){` |
|   800 | 1363 | `		return 0;` |
|     - | 1364 | `	}` |
|   334 | 1365 | `	for( k = 0 ; k < n ; k++ ){` |
|   298 | 1366 | `		if( SyToLower(z[k]) != zWant[k] ){` |
|   114 | 1367 | `			return 0;` |
|     - | 1368 | `		}` |
|    94 | 1369 | `	}` |
|    38 | 1370 | `	return 1;` |
|   475 | 1371 | `}` |
|     - | 1372 | `/*` |
|     - | 1373 | ` * A ReflectionNamedType for one name.` |
|     - | 1374 | ` *` |
|     - | 1375 | ` * bQMark says the text carried a leading '?', which is the ONLY thing that puts` |
|     - | 1376 | `` * one back on the rendered text — while `null` and `mixed` are nullable by their`` |
|     - | 1377 | ` * own meaning without ever rendering a '?'. Those two rules are independent, and` |
|     - | 1378 | `` * conflating them is how `?array` came out as `array`.`` |
|     - | 1379 | ` */` |
|   518 | 1380 | `static ph7_class_instance * ReflectNewNamed(ph7_context *pCtx, const char *z, int n, int bQMark)` |
|     4 | 1381 | `{` |
|     - | 1382 | `	ph7_class_instance *pObj;` |
|     - | 1383 | `	char zBuf[256];` |
|   522 | 1384 | `	const char *zText = z;` |
|   522 | 1385 | `	int nText = n;` |
|   522 | 1386 | `	int bNullable = bQMark \|\| ReflectTypeNameIs(z, n, "null") \|\| ReflectTypeNameIs(z, n, "mixed");` |
|   522 | 1387 | `	if( bQMark && n + 1 < (int)sizeof(zBuf) ){` |
|    63 | 1388 | `		zBuf[0] = '?';` |
|    63 | 1389 | `		SyMemcpy(z, &zBuf[1], (sxu32)n);` |
|    63 | 1390 | `		zText = zBuf;` |
|    63 | 1391 | `		nText = n + 1;` |
|    30 | 1392 | `	}` |
|   522 | 1393 | `	pObj = ReflectNewType(pCtx, "ReflectionNamedType", zText, nText, bNullable);` |
|   522 | 1394 | `	if( pObj ){` |
|   522 | 1395 | `		PH7_NativeSetAttrStr(pCtx->pVm, pObj, RT_TNAME, z, n);` |
|   259 | 1396 | `	}` |
|   522 | 1397 | `	return pObj;` |
|     4 | 1398 | `}` |
|     - | 1399 | `/*` |
|     - | 1400 | `` * One ATOM of a type text: `?X`, `(A&B)` or a plain name. An intersection is`` |
|     - | 1401 | ` * the only composite an atom can be, because php only nests that way.` |
|     - | 1402 | ` */` |
|   508 | 1403 | `static ph7_class_instance * ReflectMakeAtom(ph7_context *pCtx, const char *z, int n)` |
|     4 | 1404 | `{` |
|   512 | 1405 | `	int bQMark = 0;` |
|   512 | 1406 | `	if( n > 0 && z[0] == '?' ){` |
|    63 | 1407 | `		bQMark = 1;` |
|    63 | 1408 | `		z++;` |
|    63 | 1409 | `		n--;` |
|    30 | 1410 | `	}` |
|   512 | 1411 | `	if( n > 1 && z[0] == '(' && z[n-1] == ')' ){` |
|     8 | 1412 | `		z++;` |
|     8 | 1413 | `		n -= 2;` |
|     3 | 1414 | `	}` |
|   512 | 1415 | `	if( ReflectSigFindUnquoted(z, n, '&') >= 0 ){` |
|    12 | 1416 | `		ph7_class_instance *pObj = ReflectNewType(pCtx, "ReflectionIntersectionType", z, n, 0);` |
|    12 | 1417 | `		ph7_value *pList = ph7_context_new_array(pCtx);` |
|    12 | 1418 | `		const char *zCur = z;` |
|    12 | 1419 | `		int nCur = n;` |
|    12 | 1420 | `		if( pObj == 0 \|\| pList == 0 ){` |
|   ! 0 | 1421 | `			return pObj;` |
|     - | 1422 | `		}` |
|    22 | 1423 | `		while( nCur > 0 ){` |
|    22 | 1424 | `			int iCut = ReflectSigFindUnquoted(zCur, nCur, '&');` |
|    32 | 1425 | `			ReflectTypeListAdd(pCtx, pList,` |
|    10 | 1426 | `				ReflectNewNamed(pCtx, zCur, iCut < 0 ? nCur : iCut, 0));` |
|    22 | 1427 | `			if( iCut < 0 ){` |
|    12 | 1428 | `				break;` |
|     - | 1429 | `			}` |
|    12 | 1430 | `			zCur += iCut + 1;` |
|    12 | 1431 | `			nCur -= iCut + 1;` |
|     2 | 1432 | `		}` |
|    12 | 1433 | `		PH7_NativeSetProp(pCtx->pVm, pObj, RT_TYPES, sizeof(RT_TYPES)-1, pList);` |
|    12 | 1434 | `		return pObj;` |
|     - | 1435 | `	}` |
|   502 | 1436 | `	return ReflectNewNamed(pCtx, z, n, bQMark);` |
|   258 | 1437 | `}` |
|     - | 1438 | `/*` |
|     - | 1439 | ` * A declared type TEXT to the object php answers for it: a named type, a union,` |
|     - | 1440 | `` * or an intersection. NULL for an absent type. `X\|null` collapses back to a`` |
|     - | 1441 | `` * NULLABLE named type, which is what php reports (`?X`), but only when exactly`` |
|     - | 1442 | ` * one non-null arm is left and it is not itself an intersection.` |
|     - | 1443 | ` */` |
|   396 | 1444 | `static ph7_class_instance * ReflectMakeType(ph7_context *pCtx, const char *zText, int nText)` |
|     4 | 1445 | `{` |
|   400 | 1446 | `	const char *zBody = zText;` |
|   400 | 1447 | `	int nBody = nText;` |
|   400 | 1448 | `	int bNullable = 0, bHasNull = 0, nParts = 0, nNonNull = 0;` |
|   400 | 1449 | `	const char *zLastNonNull = 0;` |
|   400 | 1450 | `	int nLastNonNull = 0;` |
|     - | 1451 | `	const char *zCur;` |
|     - | 1452 | `	int nCur, iDepth, k, iStart;` |
|   400 | 1453 | `	if( nText < 1 ){` |
|   ! 0 | 1454 | `		return 0;` |
|     - | 1455 | `	}` |
|   400 | 1456 | `	if( zBody[0] == '?' ){` |
|    63 | 1457 | `		bNullable = 1;` |
|    63 | 1458 | `		zBody++;` |
|    63 | 1459 | `		nBody--;` |
|    30 | 1460 | `	}` |
|     - | 1461 | ``	/* Split on the TOP-LEVEL '\|' only: `A\|(B&C)` has one at depth 0 and none`` |
|     - | 1462 | `	 * inside the parentheses. */` |
|   400 | 1463 | `	iDepth = 0;` |
|   400 | 1464 | `	iStart = 0;` |
|  3708 | 1465 | `	for( k = 0 ; k <= nBody ; k++ ){` |
|  3312 | 1466 | `		if( k < nBody && zBody[k] == '(' ){` |
|     8 | 1467 | `			iDepth++;` |
|     8 | 1468 | `			continue;` |
|     - | 1469 | `		}` |
|  3306 | 1470 | `		if( k < nBody && zBody[k] == ')' ){` |
|     8 | 1471 | `			iDepth--;` |
|     8 | 1472 | `			continue;` |
|     - | 1473 | `		}` |
|  3300 | 1474 | `		if( k == nBody \|\| (zBody[k] == '\|' && iDepth == 0) ){` |
|   512 | 1475 | `			zCur = &zBody[iStart];` |
|   512 | 1476 | `			nCur = k - iStart;` |
|   512 | 1477 | `			nParts++;` |
|   512 | 1478 | `			if( nCur == 4 && ReflectSigHasNoCase(zCur, 4, "null", 4) ){` |
|     3 | 1479 | `				bHasNull = 1;` |
|     2 | 1480 | `			}else{` |
|   510 | 1481 | `				nNonNull++;` |
|   510 | 1482 | `				zLastNonNull = zCur;` |
|   510 | 1483 | `				nLastNonNull = nCur;` |
|     - | 1484 | `			}` |
|   512 | 1485 | `			iStart = k + 1;` |
|   254 | 1486 | `		}` |
|  1652 | 1487 | `	}` |
|   400 | 1488 | `	if( nParts > 1 ){` |
|     - | 1489 | `		ph7_class_instance *pObj;` |
|     - | 1490 | `		ph7_value *pList;` |
|    84 | 1491 | `		if( bHasNull && nNonNull == 1` |
|     5 | 1492 | `		 && ReflectSigFindUnquoted(zLastNonNull, nLastNonNull, '&') < 0 ){` |
|     - | 1493 | ``			/* `X\|null` IS `?X` to php. */`` |
|     - | 1494 | `			char zBuf[256];` |
|     - | 1495 | `			ph7_class_instance *pNamed;` |
|   ! 0 | 1496 | `			const char *zRender = zLastNonNull;` |
|   ! 0 | 1497 | `			int nRender = nLastNonNull;` |
|   ! 0 | 1498 | `			if( nLastNonNull + 1 < (int)sizeof(zBuf) ){` |
|   ! 0 | 1499 | `				zBuf[0] = '?';` |
|   ! 0 | 1500 | `				SyMemcpy(zLastNonNull, &zBuf[1], (sxu32)nLastNonNull);` |
|   ! 0 | 1501 | `				zRender = zBuf;` |
|   ! 0 | 1502 | `				nRender = nLastNonNull + 1;` |
|   ! 0 | 1503 | `			}` |
|   ! 0 | 1504 | `			pNamed = ReflectNewType(pCtx, "ReflectionNamedType", zRender, nRender, 1);` |
|   ! 0 | 1505 | `			if( pNamed ){` |
|   ! 0 | 1506 | `				PH7_NativeSetAttrStr(pCtx->pVm, pNamed, RT_TNAME, zLastNonNull, nLastNonNull);` |
|   ! 0 | 1507 | `			}` |
|   ! 0 | 1508 | `			return pNamed;` |
|     - | 1509 | `		}` |
|    88 | 1510 | `		pObj = ReflectNewType(pCtx, "ReflectionUnionType", zBody, nBody, bNullable \|\| bHasNull);` |
|    88 | 1511 | `		pList = ph7_context_new_array(pCtx);` |
|    88 | 1512 | `		if( pObj == 0 \|\| pList == 0 ){` |
|   ! 0 | 1513 | `			return pObj;` |
|     - | 1514 | `		}` |
|    88 | 1515 | `		iDepth = 0;` |
|    88 | 1516 | `		iStart = 0;` |
|  1334 | 1517 | `		for( k = 0 ; k <= nBody ; k++ ){` |
|  1250 | 1518 | `			if( k < nBody && zBody[k] == '(' ){` |
|     8 | 1519 | `				iDepth++;` |
|     8 | 1520 | `				continue;` |
|     - | 1521 | `			}` |
|  1244 | 1522 | `			if( k < nBody && zBody[k] == ')' ){` |
|     8 | 1523 | `				iDepth--;` |
|     8 | 1524 | `				continue;` |
|     - | 1525 | `			}` |
|  1238 | 1526 | `			if( k == nBody \|\| (zBody[k] == '\|' && iDepth == 0) ){` |
|   298 | 1527 | `				ReflectTypeListAdd(pCtx, pList,` |
|    98 | 1528 | `					ReflectMakeAtom(pCtx, &zBody[iStart], k - iStart));` |
|   200 | 1529 | `				iStart = k + 1;` |
|    98 | 1530 | `			}` |
|   621 | 1531 | `		}` |
|    88 | 1532 | `		PH7_NativeSetProp(pCtx->pVm, pObj, RT_TYPES, sizeof(RT_TYPES)-1, pList);` |
|    88 | 1533 | `		return pObj;` |
|     - | 1534 | `	}` |
|   316 | 1535 | `	if( ReflectSigFindUnquoted(zBody, nBody, '&') >= 0 ){` |
|     5 | 1536 | `		return ReflectMakeAtom(pCtx, zBody, nBody);` |
|     - | 1537 | `	}` |
|   312 | 1538 | `	if( bNullable ){` |
|     - | 1539 | `		char zBuf[256];` |
|    63 | 1540 | `		if( nBody + 1 < (int)sizeof(zBuf) ){` |
|    63 | 1541 | `			zBuf[0] = '?';` |
|    63 | 1542 | `			SyMemcpy(zBody, &zBuf[1], (sxu32)nBody);` |
|    63 | 1543 | `			return ReflectMakeAtom(pCtx, zBuf, nBody + 1);` |
|     - | 1544 | `		}` |
|   ! 0 | 1545 | `	}` |
|   252 | 1546 | `	return ReflectMakeAtom(pCtx, zBody, nBody);` |
|   202 | 1547 | `}` |
|     - | 1548 | `/*` |
|     - | 1549 | ` * Declare the four type classes. Called from PH7_VmInstallReflection() where` |
|     - | 1550 | `` * chunk 4 used to be compiled, so `Stringable` (a core interface) already`` |
|     - | 1551 | ` * exists. PH7_CLASS_NOCLONE is php's own rule for these: they are values the` |
|     - | 1552 | `` * engine hands out, and `clone $type` is an Error, not a copy.`` |
|     - | 1553 | ` */` |
|  5146 | 1554 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionTypes(ph7_vm *pVm)` |
|     5 | 1555 | `{` |
|     - | 1556 | `	static const PH7_NativePropDef aBaseProp[] = {` |
|     - | 1557 | `		{ RT_TEXT,     PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 1558 | `		{ RT_NULLABLE, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 }, 0 },` |
|     - | 1559 | `	};` |
|     - | 1560 | `	static const PH7_NativeMethodDef aBaseMethod[] = {` |
|     - | 1561 | `		{ "allowsNull", PH7_MOD_PUBLIC, "", "@bool",       vm_builtin_ReflectionType_allowsNull },` |
|     - | 1562 | `		{ "__toString", PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionType_toString },` |
|     - | 1563 | `	};` |
|     - | 1564 | `	static const PH7_NativePropDef aNamedProp[] = {` |
|     - | 1565 | `		{ RT_TNAME, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 1566 | `	};` |
|     - | 1567 | `	static const PH7_NativeMethodDef aNamedMethod[] = {` |
|     - | 1568 | `		{ "getName",   PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionNamedType_getName },` |
|     - | 1569 | `		{ "isBuiltin", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionNamedType_isBuiltin },` |
|     - | 1570 | `	};` |
|     - | 1571 | `	static const PH7_NativePropDef aCompProp[] = {` |
|     - | 1572 | `		{ RT_TYPES, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 1573 | `	};` |
|     - | 1574 | `	static const PH7_NativeMethodDef aCompMethod[] = {` |
|     - | 1575 | `		{ "getTypes", PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionType_getTypes },` |
|     - | 1576 | `	};` |
|     - | 1577 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 1578 | `		{ "ReflectionType", 0, "Stringable", PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 1579 | `		  aBaseMethod, SX_ARRAYSIZE(aBaseMethod), 0, 0, aBaseProp, SX_ARRAYSIZE(aBaseProp), 0, 0, 0 },` |
|     - | 1580 | `		{ "ReflectionNamedType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 1581 | `		  aNamedMethod, SX_ARRAYSIZE(aNamedMethod), 0, 0, aNamedProp, SX_ARRAYSIZE(aNamedProp), 0, 0, 0 },` |
|     - | 1582 | `		{ "ReflectionUnionType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 1583 | `		  aCompMethod, SX_ARRAYSIZE(aCompMethod), 0, 0, aCompProp, SX_ARRAYSIZE(aCompProp), 0, 0, 0 },` |
|     - | 1584 | `		{ "ReflectionIntersectionType", "ReflectionType", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 1585 | `		  aCompMethod, SX_ARRAYSIZE(aCompMethod), 0, 0, aCompProp, SX_ARRAYSIZE(aCompProp), 0, 0, 0 },` |
|     - | 1586 | `	};` |
|  5151 | 1587 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|     5 | 1588 | `}` |
|     - | 1589 | `/*` |
|     - | 1590 | ` * ---------------------------------------------------------------------------` |
|     - | 1591 | ` * The six standalone reflection classes.` |
|     - | 1592 | ` *` |
|     - | 1593 | ` * ReflectionGenerator, ReflectionFiber, ReflectionConstant, ReflectionExtension,` |
|     - | 1594 | ` * ReflectionZendExtension and ReflectionReference reflect ENGINE state rather` |
|     - | 1595 | ` * than a class hierarchy, so each was a prelude class over one or two thunks` |
|     - | 1596 | ` * whose only job was to hand that state to PHP. Their bodies are the C now, and` |
|     - | 1597 | ` * the four thunks that existed for them (__reflect_gen_info, __reflect_gen_exec,` |
|     - | 1598 | ` * __reflect_const_info, __reflect_ref_id) are gone with them.` |
|     - | 1599 | ` *` |
|     - | 1600 | ` * The ReflectionEnum family stays in the prelude: it extends ReflectionClass and` |
|     - | 1601 | ` * ReflectionClassConstant, which are still PHP.` |
|     - | 1602 | ` * ---------------------------------------------------------------------------` |
|     - | 1603 | ` */` |
|     - | 1604 | `#define RG_GEN   "__gen"` |
|     - | 1605 | `#define RF_FIBER "__fiber"` |
|     - | 1606 | `#define RR_ID    "__id"` |
|     - | 1607 |  |
|     - | 1608 | `/* Build an instance of a class that may still be PRELUDE PHP, running its` |
|     - | 1609 | ` * constructor. Answers 0 when the constructor threw (rc says which). */` |
|   786 | 1610 | `static ph7_class_instance * ReflectConstruct(ph7_context *pCtx, const char *zClass,` |
|     - | 1611 | `	int nArg, ph7_value **apArg, sxi32 *pRc)` |
|     5 | 1612 | `{` |
|   791 | 1613 | `	ph7_vm *pVm = pCtx->pVm;` |
|   791 | 1614 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|     - | 1615 | `	ph7_class_instance *pThis;` |
|     - | 1616 | `	ph7_class_method *pCons;` |
|   791 | 1617 | `	*pRc = PH7_OK;` |
|   791 | 1618 | `	if( pClass == 0 ){` |
|   ! 0 | 1619 | `		return 0;` |
|     - | 1620 | `	}` |
|   791 | 1621 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|   791 | 1622 | `	if( pThis == 0 ){` |
|   ! 0 | 1623 | `		return 0;` |
|     - | 1624 | `	}` |
|   791 | 1625 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|   791 | 1626 | `	if( pCons ){` |
|   791 | 1627 | `		sxi32 rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, nArg, apArg);` |
|   791 | 1628 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 | 1629 | `			PH7_ClassInstanceUnref(pThis);` |
|   ! 0 | 1630 | `			*pRc = rc;` |
|   ! 0 | 1631 | `			return 0;` |
|     - | 1632 | `		}` |
|   393 | 1633 | `	}` |
|   791 | 1634 | `	return pThis;` |
|   398 | 1635 | `}` |
|     - | 1636 | `/* The instance a native reflector wraps ($this->__gen / $this->__fiber). */` |
|    48 | 1637 | `static ph7_class_instance * ReflectWrapped(ph7_context *pCtx, const char *zSlot)` |
|     1 | 1638 | `{` |
|    49 | 1639 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    49 | 1640 | `	return pThis ? PH7_NativeAttrObj(pThis, zSlot) : 0;` |
|     1 | 1641 | `}` |
|     - | 1642 | `/* Hand back a wrapped instance the receiver already owns (a borrowed reference). */` |
|    10 | 1643 | `static int ReflectResultBorrowed(ph7_context *pCtx, ph7_class_instance *pObj)` |
|     1 | 1644 | `{` |
|     - | 1645 | `	ph7_value sVal;` |
|    11 | 1646 | `	if( pObj == 0 ){` |
|   ! 0 | 1647 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1648 | `		return PH7_OK;` |
|     - | 1649 | `	}` |
|    11 | 1650 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    11 | 1651 | `	sVal.x.pOther = pObj;` |
|    11 | 1652 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|    11 | 1653 | `	ph7_result_value(pCtx, &sVal);   /* takes its own reference */` |
|    11 | 1654 | `	return PH7_OK;` |
|     6 | 1655 | `}` |
|     - | 1656 | `/*` |
|     - | 1657 | ` * ---------------------------------------------------------------------------` |
|     - | 1658 | ` * ReflectionAttribute — chunk 7.` |
|     - | 1659 | ` *` |
|     - | 1660 | ` * php's own class, plus the shared getAttributes() body that produces it. The` |
|     - | 1661 | ` * chunk it replaces also carried __reflect_target_names (folded into the` |
|     - | 1662 | ` * "cannot target" diagnostic below) and __reflect_has_deprecated, which had` |
|     - | 1663 | ` * already lost its last caller when isDeprecated() became C.` |
|     - | 1664 | ` *` |
|     - | 1665 | ` * An instance holds a SPEC, not the arguments: [kind, target, member,` |
|     - | 1666 | ` * paramIdx] names something the engine can reopen, and getArguments()` |
|     - | 1667 | ` * evaluates the recorded expressions on every call, because php does too --` |
|     - | 1668 | `` * `#[A(self::X)]` is php code and it runs when it is asked for. That is also`` |
|     - | 1669 | ` * why the prelude needed a public __init(): PHP could not fill a fresh object` |
|     - | 1670 | ` * any other way. C fills it directly, so __init() (and the` |
|     - | 1671 | ` * __reflect_new_no_ctor that built the empty shell) are gone with it.` |
|     - | 1672 | ` * ---------------------------------------------------------------------------` |
|     - | 1673 | ` */` |
|     - | 1674 | ``#define RA_NAME   "name"      /* php declares this one PUBLIC: `$attr->name` */`` |
|     - | 1675 | `#define RA_SPEC   "__spec"    /* [kind, target, member, paramIdx] */` |
|     - | 1676 | `#define RA_IDX    "__idx"     /* which of that target's attributes this is */` |
|     - | 1677 | `#define RA_TARGET "__target"  /* the Attribute::TARGET_* bit it was found on */` |
|     - | 1678 | `#define RA_REP    "__rep"     /* the target carries more than one of this name */` |
|     - | 1679 |  |
|     - | 1680 | `/* Bound on the value exporter's own recursion. An attribute argument cannot be` |
|     - | 1681 | ` * cyclic, but an OBJECT argument's property graph can be. */` |
|     - | 1682 | `#define REFLECT_EXPORT_MAX_DEPTH 31` |
|     - | 1683 |  |
|     - | 1684 | `/* php's Attribute::TARGET_* names, in bit order (TARGET_CLASS is bit 0). */` |
|     - | 1685 | `static const char *const azReflectTarget[] = {` |
|     - | 1686 | `	"class", "function", "method", "property", "class constant", "parameter", "constant"` |
|     - | 1687 | `};` |
|     - | 1688 |  |
|     - | 1689 | `static void ReflectExportValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth);` |
|     - | 1690 |  |
|     - | 1691 | `/*` |
|     - | 1692 | ` * A string the way php's reflection prints one, in either of php's TWO quotings.` |
|     - | 1693 | ` *` |
|     - | 1694 | ` * A USERLAND default prints single-quoted, with the NON-PRINTABLE bytes escaped` |
|     - | 1695 | ` * (php's smart_str_append_escaped) and the quote itself NOT escaped — php's own` |
|     - | 1696 | ` * output for "q'q" is 'q'q'. An INTERNAL one prints DOUBLE-quoted and escapes the` |
|     - | 1697 | `` * quote: php answers `string $enclosure = "\""` for str_getcsv where PHL used to`` |
|     - | 1698 | `` * answer `'"'`. Every internal function with a string default was affected; the`` |
|     - | 1699 | `` * pair was found on Closure::bindTo's `= "static"` while making that class native.`` |
|     - | 1700 | ` */` |
|    72 | 1701 | `static void ReflectExportStrQ(SyBlob *pOut, const char *zIn, sxu32 nIn, char cQuote)` |
|     1 | 1702 | `{` |
|     - | 1703 | `	static const char zHexDigit[] = "0123456789ABCDEF";` |
|     - | 1704 | `	sxu32 i;` |
|    73 | 1705 | `	SyBlobAppend(pOut, &cQuote, sizeof(char));` |
|   209 | 1706 | `	for( i = 0 ; i < nIn ; i++ ){` |
|   137 | 1707 | `		unsigned char c = (unsigned char)zIn[i];` |
|   137 | 1708 | `		if( c == (unsigned char)cQuote && cQuote == '"' ){` |
|     3 | 1709 | `			SyBlobAppend(pOut, "\\\"", sizeof("\\\"")-1);` |
|     3 | 1710 | `			continue;` |
|     - | 1711 | `		}` |
|     - | 1712 | `		{` |
|   135 | 1713 | `		const char *zEsc = 0;` |
|   135 | 1714 | `		switch( c ){` |
|     3 | 1715 | `			case 0x09: zEsc = "\\t"; break;` |
|     9 | 1716 | `			case 0x0A: zEsc = "\\n"; break;` |
|     3 | 1717 | `			case 0x0B: zEsc = "\\v"; break;` |
|     3 | 1718 | `			case 0x0C: zEsc = "\\f"; break;` |
|     3 | 1719 | `			case 0x0D: zEsc = "\\r"; break;` |
|     3 | 1720 | `			case 0x1B: zEsc = "\\e"; break;` |
|     5 | 1721 | `			case '\\': zEsc = "\\\\"; break;` |
|   112 | 1722 | `			default:   break;` |
|     - | 1723 | `		}` |
|   135 | 1724 | `		if( zEsc ){` |
|    23 | 1725 | `			SyBlobAppend(pOut, zEsc, sizeof("\\t")-1);` |
|   127 | 1726 | `		}else if( c < 0x20 \|\| c > 0x7E ){` |
|     - | 1727 | `			char zHex[4];` |
|     7 | 1728 | `			zHex[0] = '\\';` |
|     7 | 1729 | `			zHex[1] = 'x';` |
|     7 | 1730 | `			zHex[2] = zHexDigit[(c >> 4) & 0x0F];` |
|     7 | 1731 | `			zHex[3] = zHexDigit[c & 0x0F];` |
|     7 | 1732 | `			SyBlobAppend(pOut, zHex, sizeof(zHex));` |
|     4 | 1733 | `		}else{` |
|   107 | 1734 | `			SyBlobAppend(pOut, (const char *)&c, sizeof(char));` |
|     - | 1735 | `		}` |
|     - | 1736 | `		}` |
|    68 | 1737 | `	}` |
|    73 | 1738 | `	SyBlobAppend(pOut, &cQuote, sizeof(char));` |
|    73 | 1739 | `}` |
|     - | 1740 | `/* php's userland quoting, which is what every existing caller means. */` |
|    62 | 1741 | `static void ReflectExportStr(SyBlob *pOut, const char *zIn, sxu32 nIn)` |
|     1 | 1742 | `{` |
|    63 | 1743 | `	ReflectExportStrQ(pOut, zIn, nIn, '\'');` |
|    63 | 1744 | `}` |
|     - | 1745 | `/*` |
|     - | 1746 | ` * A float the way php's reflection prints one: the plain string cast (which` |
|     - | 1747 | `` * PHL already renders php's way, 1.0E+15 and all), then php's `zero_frac` —`` |
|     - | 1748 | ` * a float that came out without a fraction gets ".0" so 1.0 does not print as` |
|     - | 1749 | ` * 1. INF and NAN render as words and are left alone.` |
|     - | 1750 | ` */` |
|    20 | 1751 | `static void ReflectExportReal(ph7_vm *pVm, SyBlob *pOut, ph7_real rVal)` |
|     1 | 1752 | `{` |
|     - | 1753 | `	ph7_value sTmp;` |
|     - | 1754 | `	const char *zText;` |
|    21 | 1755 | `	int nText, i, bPlain = 1;` |
|    21 | 1756 | `	PH7_MemObjInitFromReal(pVm, &sTmp, rVal);` |
|    21 | 1757 | `	zText = ph7_value_to_string(&sTmp, &nText);` |
|    21 | 1758 | `	if( nText > 0 ){` |
|    21 | 1759 | `		SyBlobAppend(pOut, zText, (sxu32)nText);` |
|    10 | 1760 | `	}` |
|    47 | 1761 | `	for( i = 0 ; i < nText ; i++ ){` |
|    35 | 1762 | `		if( (zText[i] < '0' \|\| zText[i] > '9') && !(i == 0 && zText[i] == '-') ){` |
|     9 | 1763 | `			bPlain = 0;` |
|     9 | 1764 | `			break;` |
|     - | 1765 | `		}` |
|    14 | 1766 | `	}` |
|    21 | 1767 | `	if( bPlain && nText > 0 ){` |
|    13 | 1768 | `		SyBlobAppend(pOut, ".0", sizeof(".0")-1);` |
|     6 | 1769 | `	}` |
|    21 | 1770 | `	PH7_MemObjRelease(&sTmp);` |
|    21 | 1771 | `}` |
|     - | 1772 | `/*` |
|     - | 1773 | ` * An array the way php's reflection prints one: a LIST — every key an integer,` |
|     - | 1774 | ` * in sequence from zero — prints no keys at all, and anything else prints one` |
|     - | 1775 | `` * for EVERY entry. That is why `[1, 'k' => 2]` comes out as`` |
|     - | 1776 | `` * `[0 => 1, 'k' => 2]` while `[[1], [2 => 3]]` keeps its outer keys silent.`` |
|     - | 1777 | ` */` |
|    30 | 1778 | `static void ReflectExportArray(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth)` |
|     1 | 1779 | `{` |
|    31 | 1780 | `	ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;` |
|     - | 1781 | `	ph7_hashmap_node *pEntry;` |
|    31 | 1782 | `	sxi64 iExpect = 0;` |
|     - | 1783 | `	sxu32 n;` |
|    31 | 1784 | `	int bList = 1;` |
|    61 | 1785 | `	for( pEntry = pMap->pFirst, n = 0 ; n < pMap->nEntry && pEntry ; n++ ){` |
|    43 | 1786 | `		if( pEntry->iType == HASHMAP_BLOB_NODE \|\| pEntry->xKey.iKey != iExpect ){` |
|    13 | 1787 | `			bList = 0;` |
|    13 | 1788 | `			break;` |
|     - | 1789 | `		}` |
|    31 | 1790 | `		iExpect++;` |
|    31 | 1791 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|    16 | 1792 | `	}` |
|    31 | 1793 | `	SyBlobAppend(pOut, "[", sizeof(char));` |
|    75 | 1794 | `	for( pEntry = pMap->pFirst, n = 0 ; n < pMap->nEntry && pEntry ; n++ ){` |
|    45 | 1795 | `		ph7_value *pMember = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|    45 | 1796 | `		if( n > 0 ){` |
|    17 | 1797 | `			SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|     8 | 1798 | `		}` |
|    45 | 1799 | `		if( !bList ){` |
|    21 | 1800 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|    13 | 1801 | `				ReflectExportStr(pOut, (const char *)SyBlobData(&pEntry->xKey.sKey),` |
|     4 | 1802 | `					SyBlobLength(&pEntry->xKey.sKey));` |
|     5 | 1803 | `			}else{` |
|    13 | 1804 | `				SyBlobFormat(pOut, "%qd", pEntry->xKey.iKey);` |
|     - | 1805 | `			}` |
|    21 | 1806 | `			SyBlobAppend(pOut, " => ", sizeof(" => ")-1);` |
|    10 | 1807 | `		}` |
|    45 | 1808 | `		ReflectExportValue(pCtx, pOut, pMember, iDepth + 1);` |
|    45 | 1809 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|    23 | 1810 | `	}` |
|    31 | 1811 | `	SyBlobAppend(pOut, "]", sizeof(char));` |
|    31 | 1812 | `}` |
|     - | 1813 | `/*` |
|     - | 1814 | `` * One value in php's reflection export syntax — the text after `= ` in a`` |
|     - | 1815 | `` * parameter default and inside `Argument #0 [ … ]` in an attribute dump.`` |
|     - | 1816 | ` *` |
|     - | 1817 | ` * The type dispatch is PH7_MemObjDump's, including its REAL-before-INT order:` |
|     - | 1818 | ` * an integer-valued float carries a cached int view too, and php prints 1.0.` |
|     - | 1819 | ` *` |
|     - | 1820 | ` * php renders an OBJECT argument by echoing the source expression it never` |
|     - | 1821 | `` * folded (`new \A(x: 1)`), which needs the AST. PHL evaluates attribute`` |
|     - | 1822 | ` * arguments from byte-code and has only the resulting VALUE, so a non-enum` |
|     - | 1823 | ` * object is rebuilt from its own properties: same shape, not always the same` |
|     - | 1824 | ` * bytes. An enum case — the one object php's own value formatter handles —` |
|     - | 1825 | ` * matches exactly.` |
|     - | 1826 | ` */` |
|   182 | 1827 | `static void ReflectExportValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth)` |
|     1 | 1828 | `{` |
|   183 | 1829 | `	if( pVal == 0 \|\| iDepth > REFLECT_EXPORT_MAX_DEPTH ){` |
|   ! 0 | 1830 | `		SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|   ! 0 | 1831 | `		return;` |
|     - | 1832 | `	}` |
|   183 | 1833 | `	if( (pVal->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|     3 | 1834 | `		ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;` |
|     3 | 1835 | `		if( pObj->pClass->iFlags & PH7_CLASS_ENUM ){` |
|     3 | 1836 | `			ph7_value *pName = PH7_EnumCaseNameValue(pObj);` |
|     - | 1837 | `			/* php writes the case as the source did, so a namespaced enum comes` |
|     - | 1838 | `			 * out fully qualified (\N\E::One) and a global one bare (E::One).` |
|     - | 1839 | `			 * The separator is the only thing left of that distinction here. */` |
|     3 | 1840 | `			if( SyByteFind(SyStringData(&pObj->pClass->sName),` |
|     4 | 1841 | `				SyStringLength(&pObj->pClass->sName), '\\', 0) == SXRET_OK ){` |
|   ! 0 | 1842 | `				SyBlobAppend(pOut, "\\", sizeof(char));` |
|   ! 0 | 1843 | `			}` |
|     3 | 1844 | `			SyBlobFormat(pOut, "%z::", &pObj->pClass->sName);` |
|     3 | 1845 | `			if( pName && SyBlobLength(&pName->sBlob) > 0 ){` |
|     3 | 1846 | `				SyBlobAppend(pOut, SyBlobData(&pName->sBlob), SyBlobLength(&pName->sBlob));` |
|     1 | 1847 | `			}` |
|     3 | 1848 | `			return;` |
|     - | 1849 | `		}` |
|   ! 0 | 1850 | `		SyBlobFormat(pOut, "new \\%z(", &pObj->pClass->sName);` |
|     - | 1851 | `		{` |
|     - | 1852 | `			SyHashEntry *pEntry;` |
|   ! 0 | 1853 | `			int nWritten = 0;` |
|   ! 0 | 1854 | `			SyHashResetLoopCursor(&pObj->hAttr);` |
|   ! 0 | 1855 | `			while((pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){` |
|   ! 0 | 1856 | `				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - | 1857 | `				ph7_value *pSlot;` |
|   ! 0 | 1858 | `				if( pVmAttr->pAttr->iFlags` |
|   ! 0 | 1859 | `					& (PH7_CLASS_ATTR_CONSTANT\|PH7_CLASS_ATTR_STATIC\|PH7_CLASS_ATTR_HOOK_VIRTUAL\|PH7_CLASS_ATTR_HIDDEN) ){` |
|   ! 0 | 1860 | `					continue; /* class-level members are not part of the object */` |
|     - | 1861 | `				}` |
|   ! 0 | 1862 | `				pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|   ! 0 | 1863 | `				if( nWritten++ ){` |
|   ! 0 | 1864 | `					SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|   ! 0 | 1865 | `				}` |
|   ! 0 | 1866 | `				ReflectExportValue(pCtx, pOut, pSlot, iDepth + 1);` |
|   ! 0 | 1867 | `			}` |
|     - | 1868 | `		}` |
|   ! 0 | 1869 | `		SyBlobAppend(pOut, ")", sizeof(char));` |
|   ! 0 | 1870 | `		return;` |
|     - | 1871 | `	}` |
|   181 | 1872 | `	if( pVal->iFlags & MEMOBJ_NULL ){` |
|    21 | 1873 | `		SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|    21 | 1874 | `		return;` |
|     - | 1875 | `	}` |
|   161 | 1876 | `	if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|    31 | 1877 | `		ReflectExportArray(pCtx, pOut, pVal, iDepth);` |
|    31 | 1878 | `		return;` |
|     - | 1879 | `	}` |
|   131 | 1880 | `	if( pVal->iFlags & MEMOBJ_BOOL ){` |
|     5 | 1881 | `		if( pVal->x.iVal != 0 ){` |
|     3 | 1882 | `			SyBlobAppend(pOut, "true", sizeof("true")-1);` |
|     2 | 1883 | `		}else{` |
|     3 | 1884 | `			SyBlobAppend(pOut, "false", sizeof("false")-1);` |
|     - | 1885 | `		}` |
|     5 | 1886 | `		return;` |
|     - | 1887 | `	}` |
|   127 | 1888 | `	if( pVal->iFlags & MEMOBJ_REAL ){` |
|    21 | 1889 | `		ReflectExportReal(pCtx->pVm, pOut, pVal->rVal);` |
|    21 | 1890 | `		return;` |
|     - | 1891 | `	}` |
|   107 | 1892 | `	if( pVal->iFlags & MEMOBJ_INT ){` |
|    53 | 1893 | `		SyBlobFormat(pOut, "%qd", pVal->x.iVal);` |
|    53 | 1894 | `		return;` |
|     - | 1895 | `	}` |
|    55 | 1896 | `	if( pVal->iFlags & MEMOBJ_STRING ){` |
|    55 | 1897 | `		ReflectExportStr(pOut, (const char *)SyBlobData(&pVal->sBlob), SyBlobLength(&pVal->sBlob));` |
|    55 | 1898 | `		return;` |
|     - | 1899 | `	}` |
|     - | 1900 | `	/* A resource, and anything else php has no export syntax for. */` |
|   ! 0 | 1901 | `	SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);` |
|    92 | 1902 | `}` |
|     - | 1903 | `/*` |
|     - | 1904 | ` * Build one ReflectionAttribute. pSpec is shared by every attribute of the` |
|     - | 1905 | ` * same target — it says how to reopen it, not which one this is.` |
|     - | 1906 | ` */` |
|   118 | 1907 | `static ph7_class_instance * ReflectAttrNew(ph7_context *pCtx, SyString *pName,` |
|     - | 1908 | `	ph7_value *pSpec, sxu32 nIdx, int iTargetBit, int bRepeated)` |
|     2 | 1909 | `{` |
|   120 | 1910 | `	ph7_vm *pVm = pCtx->pVm;` |
|   120 | 1911 | `	ph7_class *pClass = PH7_VmExtractClass(pVm, "ReflectionAttribute",` |
|     - | 1912 | `		sizeof("ReflectionAttribute")-1, FALSE, 0);` |
|   120 | 1913 | `	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|   120 | 1914 | `	if( pObj == 0 ){` |
|   ! 0 | 1915 | `		return 0;` |
|     - | 1916 | `	}` |
|   120 | 1917 | `	PH7_NativeSetAttrStr(pVm, pObj, RA_NAME, SyStringData(pName), (int)SyStringLength(pName));` |
|   120 | 1918 | `	PH7_NativeSetProp(pVm, pObj, RA_SPEC, sizeof(RA_SPEC)-1, pSpec);` |
|   120 | 1919 | `	PH7_NativeSetAttrInt(pVm, pObj, RA_IDX, (sxi64)nIdx);` |
|   120 | 1920 | `	PH7_NativeSetAttrInt(pVm, pObj, RA_TARGET, iTargetBit);` |
|   120 | 1921 | `	PH7_NativeSetAttrBool(pVm, pObj, RA_REP, bRepeated);` |
|   120 | 1922 | `	return pObj;` |
|    61 | 1923 | `}` |
|     - | 1924 | `/*` |
|     - | 1925 | ` * Unpack the receiver's spec into the four-value vector ReflectAttrArgs takes.` |
|     - | 1926 | ` * Returns 0 when the object is not one this file built.` |
|     - | 1927 | ` */` |
|    72 | 1928 | `static int ReflectAttrSpec(ph7_context *pCtx, ph7_class_instance *pThis, ph7_value **apOut)` |
|     1 | 1929 | `{` |
|    73 | 1930 | `	ph7_value *pSpec = pThis ? PH7_NativeAttr(pThis, RA_SPEC) : 0;` |
|     - | 1931 | `	ph7_hashmap *pMap;` |
|     - | 1932 | `	int i;` |
|    73 | 1933 | `	if( pSpec == 0 \|\| (pSpec->iFlags & MEMOBJ_HASHMAP) == 0 ){` |
|   ! 0 | 1934 | `		return 0;` |
|     - | 1935 | `	}` |
|    73 | 1936 | `	pMap = (ph7_hashmap *)pSpec->x.pOther;` |
|   361 | 1937 | `	for( i = 0 ; i < 4 ; i++ ){` |
|     - | 1938 | `		ph7_value sKey;` |
|   289 | 1939 | `		ph7_hashmap_node *pNode = 0;` |
|     - | 1940 | `		sxi32 rc;` |
|   289 | 1941 | `		PH7_MemObjInitFromInt(pCtx->pVm, &sKey, i);` |
|   289 | 1942 | `		rc = PH7_HashmapLookup(pMap, &sKey, &pNode);` |
|   289 | 1943 | `		PH7_MemObjRelease(&sKey);` |
|   289 | 1944 | `		if( rc != SXRET_OK \|\| pNode == 0 ){` |
|   ! 0 | 1945 | `			return 0;` |
|     - | 1946 | `		}` |
|   289 | 1947 | `		apOut[i] = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pNode->nValIdx);` |
|   289 | 1948 | `		if( apOut[i] == 0 ){` |
|   ! 0 | 1949 | `			return 0;` |
|     - | 1950 | `		}` |
|   145 | 1951 | `	}` |
|    73 | 1952 | `	return 1;` |
|    37 | 1953 | `}` |
|     - | 1954 | `/* The receiver's evaluated arguments, or an empty array when the target no` |
|     - | 1955 | `` * longer resolves (what the chunk's `$a === null ? array() : $a` said). */`` |
|    72 | 1956 | `static ph7_value * ReflectAttrOwnArgs(ph7_context *pCtx, ph7_class_instance *pThis)` |
|     1 | 1957 | `{` |
|     - | 1958 | `	ph7_value *apSpec[4];` |
|    73 | 1959 | `	ph7_value *pArgs = 0;` |
|    73 | 1960 | `	if( pThis && ReflectAttrSpec(pCtx, pThis, apSpec) ){` |
|    73 | 1961 | `		pArgs = ReflectAttrArgs(pCtx, apSpec, (sxu32)PH7_NativeAttrInt(pThis, RA_IDX));` |
|    36 | 1962 | `	}` |
|    73 | 1963 | `	return pArgs ? pArgs : ph7_context_new_array(pCtx);` |
|     1 | 1964 | `}` |
|    26 | 1965 | `static int vm_builtin_ReflectionAttribute_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 1966 | `{` |
|    28 | 1967 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    28 | 1968 | `	const char *zName = "";` |
|    28 | 1969 | `	int nName = 0;` |
|    13 | 1970 | `	SXUNUSED(nArg);` |
|    13 | 1971 | `	SXUNUSED(apArg);` |
|    28 | 1972 | `	if( pThis ){` |
|    28 | 1973 | `		PH7_NativeAttrStr(pThis, RA_NAME, &zName, &nName);` |
|    13 | 1974 | `	}` |
|    28 | 1975 | `	ph7_result_string(pCtx, zName, nName);` |
|    28 | 1976 | `	return PH7_OK;` |
|     2 | 1977 | `}` |
|    24 | 1978 | `static int vm_builtin_ReflectionAttribute_getTarget(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1979 | `{` |
|    25 | 1980 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    12 | 1981 | `	SXUNUSED(nArg);` |
|    12 | 1982 | `	SXUNUSED(apArg);` |
|    25 | 1983 | `	ph7_result_int64(pCtx, pThis ? PH7_NativeAttrInt(pThis, RA_TARGET) : 0);` |
|    25 | 1984 | `	return PH7_OK;` |
|     1 | 1985 | `}` |
|    18 | 1986 | `static int vm_builtin_ReflectionAttribute_isRepeated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1987 | `{` |
|    19 | 1988 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     9 | 1989 | `	SXUNUSED(nArg);` |
|     9 | 1990 | `	SXUNUSED(apArg);` |
|    19 | 1991 | `	ph7_result_bool(pCtx, pThis != 0 && PH7_NativeAttrTruthy(pThis, RA_REP));` |
|    19 | 1992 | `	return PH7_OK;` |
|     1 | 1993 | `}` |
|    30 | 1994 | `static int vm_builtin_ReflectionAttribute_getArguments(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1995 | `{` |
|    31 | 1996 | `	ph7_value *pArgs = ReflectAttrOwnArgs(pCtx, PH7_ContextThis(pCtx));` |
|    15 | 1997 | `	SXUNUSED(nArg);` |
|    15 | 1998 | `	SXUNUSED(apArg);` |
|    31 | 1999 | `	if( pArgs == 0 ){` |
|   ! 0 | 2000 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2001 | `	}` |
|    31 | 2002 | `	ph7_result_value(pCtx, pArgs);` |
|    31 | 2003 | `	return PH7_OK;` |
|    16 | 2004 | `}` |
|     - | 2005 | `/*` |
|     - | 2006 | ` * newInstance(): the four screens php runs before it constructs anything —` |
|     - | 2007 | ` * the attribute class exists, it is declared #[Attribute], that declaration` |
|     - | 2008 | ` * allows the target this was found on, and it allows repetition if it was` |
|     - | 2009 | ` * repeated. The messages are php's, byte for byte.` |
|     - | 2010 | ` */` |
|    48 | 2011 | `static int vm_builtin_ReflectionAttribute_newInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2012 | `{` |
|    49 | 2013 | `	ph7_vm *pVm = pCtx->pVm;` |
|    49 | 2014 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    49 | 2015 | `	ph7_value *pNameVal = pThis ? PH7_NativeAttr(pThis, RA_NAME) : 0;` |
|     - | 2016 | `	ph7_class *pClass;` |
|     - | 2017 | `	ph7_attribute *aA;` |
|     - | 2018 | `	ph7_value *pDeclArgs;` |
|    49 | 2019 | `	sxu32 n, nDecl = 0;` |
|    49 | 2020 | `	int bDecl = 0, iTarget, iBit;` |
|    49 | 2021 | `	sxi64 iFlags = 127; /* php's TARGET_ALL: #[Attribute] with no argument */` |
|    24 | 2022 | `	SXUNUSED(nArg);` |
|    24 | 2023 | `	SXUNUSED(apArg);` |
|    49 | 2024 | `	if( pNameVal == 0 ){` |
|   ! 0 | 2025 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2026 | `		return PH7_OK;` |
|     - | 2027 | `	}` |
|    49 | 2028 | `	pClass = ReflectResolveClass(pVm, pNameVal);` |
|    49 | 2029 | `	if( pClass == 0 ){` |
|     - | 2030 | `		SyString sName;` |
|     5 | 2031 | `		SyStringInitFromBuf(&sName, SyBlobData(&pNameVal->sBlob), SyBlobLength(&pNameVal->sBlob));` |
|     5 | 2032 | `		return PH7_VmThrowException(pCtx, "Error", "Attribute class \"%z\" not found", &sName);` |
|     - | 2033 | `	}` |
|     - | 2034 | `	/* Which #[...] on the attribute class is its own #[Attribute] declaration */` |
|    45 | 2035 | `	aA = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);` |
|    45 | 2036 | `	for( n = 0 ; n < SySetUsed(&pClass->aAttrs) ; n++ ){` |
|    40 | 2037 | `		if( SyStringLength(&aA[n].sName) == sizeof("Attribute")-1` |
|    41 | 2038 | `		 && SyStrnicmp(SyStringData(&aA[n].sName), "Attribute", sizeof("Attribute")-1) == 0 ){` |
|    41 | 2039 | `			nDecl = n;` |
|    41 | 2040 | `			bDecl = 1;` |
|    41 | 2041 | `			break;` |
|     - | 2042 | `		}` |
|   ! 0 | 2043 | `	}` |
|    45 | 2044 | `	if( !bDecl ){` |
|     7 | 2045 | `		return PH7_VmThrowException(pCtx, "Error",` |
|     2 | 2046 | `			"Attempting to use non-attribute class \"%z\" as attribute", &pClass->sName);` |
|     - | 2047 | `	}` |
|     - | 2048 | ``	/* Its argument is the target mask: positional, or named `flags:`. */`` |
|     - | 2049 | `	{` |
|     - | 2050 | `		ph7_value *apSpec[4];` |
|    41 | 2051 | `		ph7_value *pKind = ph7_context_new_scalar(pCtx);` |
|    41 | 2052 | `		ph7_value *pMem  = ph7_context_new_scalar(pCtx);` |
|    41 | 2053 | `		ph7_value *pIdx  = ph7_context_new_scalar(pCtx);` |
|    41 | 2054 | `		if( pKind == 0 \|\| pMem == 0 \|\| pIdx == 0 ){` |
|   ! 0 | 2055 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 2056 | `		}` |
|    41 | 2057 | `		ph7_value_string(pKind, "class", sizeof("class")-1);` |
|    41 | 2058 | `		ph7_value_null(pMem);` |
|    41 | 2059 | `		ph7_value_int(pIdx, 0);` |
|    41 | 2060 | `		apSpec[0] = pKind;` |
|    41 | 2061 | `		apSpec[1] = pNameVal;` |
|    41 | 2062 | `		apSpec[2] = pMem;` |
|    41 | 2063 | `		apSpec[3] = pIdx;` |
|    41 | 2064 | `		pDeclArgs = ReflectAttrArgs(pCtx, apSpec, nDecl);` |
|     - | 2065 | `	}` |
|    41 | 2066 | `	if( pDeclArgs ){` |
|    41 | 2067 | `		ph7_value *pFlags = ph7_array_fetch(pDeclArgs, "0", -1);` |
|    41 | 2068 | `		if( pFlags == 0 ){` |
|    17 | 2069 | `			pFlags = ph7_array_fetch(pDeclArgs, "flags", -1);` |
|     8 | 2070 | `		}` |
|    41 | 2071 | `		if( pFlags ){` |
|    25 | 2072 | `			iFlags = ph7_value_to_int64(pFlags);` |
|    12 | 2073 | `		}` |
|    20 | 2074 | `	}` |
|    41 | 2075 | `	iTarget = (int)PH7_NativeAttrInt(pThis, RA_TARGET);` |
|     - | 2076 | `	/* php's INTERNAL attributes carry a validator beside their mask, and` |
|     - | 2077 | ``	 * `#[\Deprecated]` has the one that matters here: its mask includes`` |
|     - | 2078 | `	 * TARGET_CLASS (php 8.5 marks a deprecated TRAIT with it), but every other` |
|     - | 2079 | ``	 * kind of class is refused by name — `Cannot apply #[\Deprecated] to class`` |
|     - | 2080 | ``	 * X`. PHL has no traits, so the class target is always the refusal. php runs`` |
|     - | 2081 | `	 * this at COMPILE time; PHL's attribute checks all happen here. */` |
|    40 | 2082 | `	if( iTarget == 1` |
|    30 | 2083 | `	 && SyStringLength(&pClass->sName) == sizeof("Deprecated")-1` |
|    11 | 2084 | `	 && SyStrnicmp(SyStringData(&pClass->sName), "Deprecated", sizeof("Deprecated")-1) == 0 ){` |
|   ! 0 | 2085 | `		ph7_class *pTarget = 0;` |
|   ! 0 | 2086 | `		ph7_value *pSpec = PH7_NativeAttr(pThis, RA_SPEC);` |
|   ! 0 | 2087 | `		if( pSpec && (pSpec->iFlags & MEMOBJ_HASHMAP) ){` |
|   ! 0 | 2088 | `			ph7_value *pName = ph7_array_fetch(pSpec, "1", -1);` |
|   ! 0 | 2089 | `			if( pName ){` |
|   ! 0 | 2090 | `				pTarget = ReflectResolveClass(pVm, pName);` |
|   ! 0 | 2091 | `			}` |
|   ! 0 | 2092 | `		}` |
|   ! 0 | 2093 | `		return PH7_VmThrowException(pCtx, "Error",` |
|     - | 2094 | `			"Cannot apply #[\\Deprecated] to class %z",` |
|   ! 0 | 2095 | `			pTarget ? &pTarget->sName : &pClass->sName);` |
|     - | 2096 | `	}` |
|    41 | 2097 | `	if( (iFlags & iTarget) == 0 ){` |
|     - | 2098 | `		SyBlob sAllowed;` |
|     - | 2099 | `		sxi32 rc;` |
|     7 | 2100 | `		SyBlobInit(&sAllowed, &pVm->sAllocator);` |
|    49 | 2101 | `		for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azReflectTarget) ; iBit++ ){` |
|    43 | 2102 | `			if( (iFlags & ((sxi64)1 << iBit)) == 0 ){` |
|    37 | 2103 | `				continue;` |
|     - | 2104 | `			}` |
|     7 | 2105 | `			if( SyBlobLength(&sAllowed) > 0 ){` |
|   ! 0 | 2106 | `				SyBlobAppend(&sAllowed, ", ", sizeof(", ")-1);` |
|   ! 0 | 2107 | `			}` |
|    10 | 2108 | `			SyBlobAppend(&sAllowed, azReflectTarget[iBit],` |
|     6 | 2109 | `				(sxu32)SyStrlen(azReflectTarget[iBit]));` |
|     4 | 2110 | `		}` |
|     7 | 2111 | `		SyBlobAppend(&sAllowed, "", sizeof(char)); /* NUL for the %s below */` |
|    15 | 2112 | `		for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azReflectTarget) ; iBit++ ){` |
|    15 | 2113 | `			if( iTarget == (1 << iBit) ){` |
|     7 | 2114 | `				break;` |
|     - | 2115 | `			}` |
|     5 | 2116 | `		}` |
|    10 | 2117 | `		rc = PH7_VmThrowException(pCtx, "Error",` |
|     3 | 2118 | `			"Attribute \"%z\" cannot target %s (allowed targets: %s)", &pClass->sName,` |
|     3 | 2119 | `			iBit < (int)SX_ARRAYSIZE(azReflectTarget) ? azReflectTarget[iBit] : "",` |
|     3 | 2120 | `			SyBlobData(&sAllowed));` |
|     7 | 2121 | `		SyBlobRelease(&sAllowed);` |
|     7 | 2122 | `		return rc;` |
|     - | 2123 | `	}` |
|    35 | 2124 | `	if( PH7_NativeAttrTruthy(pThis, RA_REP) && (iFlags & 128) == 0 ){` |
|     7 | 2125 | `		return PH7_VmThrowException(pCtx, "Error",` |
|     2 | 2126 | `			"Attribute \"%z\" must not be repeated", &pClass->sName);` |
|     - | 2127 | `	}` |
|    31 | 2128 | `	return ReflectAttrInstantiate(pCtx, pNameVal, ReflectAttrOwnArgs(pCtx, pThis));` |
|    25 | 2129 | `}` |
|     - | 2130 | `/*` |
|     - | 2131 | `` * php's export text. A bare `Attribute [ Name ]` when there are no arguments;`` |
|     - | 2132 | ` * otherwise the same head followed by the argument block, each argument` |
|     - | 2133 | `` * rendered in php's value-export syntax and named ones as `name = value`.`` |
|     - | 2134 | ` */` |
|    12 | 2135 | `static int vm_builtin_ReflectionAttribute_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2136 | `{` |
|    13 | 2137 | `	ph7_vm *pVm = pCtx->pVm;` |
|    13 | 2138 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    13 | 2139 | `	ph7_value *pArgs = ReflectAttrOwnArgs(pCtx, pThis);` |
|    13 | 2140 | `	const char *zName = "";` |
|    13 | 2141 | `	int nName = 0;` |
|     - | 2142 | `	SyBlob sOut;` |
|    13 | 2143 | `	sxu32 nCount = 0;` |
|     6 | 2144 | `	SXUNUSED(nArg);` |
|     6 | 2145 | `	SXUNUSED(apArg);` |
|    13 | 2146 | `	if( pThis ){` |
|    13 | 2147 | `		PH7_NativeAttrStr(pThis, RA_NAME, &zName, &nName);` |
|     6 | 2148 | `	}` |
|    13 | 2149 | `	if( pArgs && (pArgs->iFlags & MEMOBJ_HASHMAP) ){` |
|    13 | 2150 | `		nCount = ((ph7_hashmap *)pArgs->x.pOther)->nEntry;` |
|     6 | 2151 | `	}` |
|    13 | 2152 | `	SyBlobInit(&sOut, &pVm->sAllocator);` |
|    13 | 2153 | `	SyBlobAppend(&sOut, "Attribute [ ", sizeof("Attribute [ ")-1);` |
|    13 | 2154 | `	if( nName > 0 ){` |
|    13 | 2155 | `		SyBlobAppend(&sOut, zName, (sxu32)nName);` |
|     6 | 2156 | `	}` |
|    13 | 2157 | `	SyBlobAppend(&sOut, " ]", sizeof(" ]")-1);` |
|    13 | 2158 | `	if( nCount < 1 ){` |
|     3 | 2159 | `		SyBlobAppend(&sOut, "\n", sizeof(char));` |
|     2 | 2160 | `	}else{` |
|    11 | 2161 | `		ph7_hashmap *pMap = (ph7_hashmap *)pArgs->x.pOther;` |
|    11 | 2162 | `		ph7_hashmap_node *pEntry = pMap->pFirst;` |
|     - | 2163 | `		sxu32 n;` |
|    11 | 2164 | `		SyBlobFormat(&sOut, " {\n  - Arguments [%u] {\n", nCount);` |
|    81 | 2165 | `		for( n = 0 ; n < nCount && pEntry ; n++ ){` |
|    71 | 2166 | `			ph7_value *pMember = (ph7_value *)SySetAt(&pVm->aMemObj, pEntry->nValIdx);` |
|    71 | 2167 | `			SyBlobFormat(&sOut, "    Argument #%u [ ", n);` |
|    71 | 2168 | `			if( pEntry->iType == HASHMAP_BLOB_NODE ){` |
|     7 | 2169 | `				SyBlobAppend(&sOut, SyBlobData(&pEntry->xKey.sKey),` |
|     2 | 2170 | `					SyBlobLength(&pEntry->xKey.sKey));` |
|     5 | 2171 | `				SyBlobAppend(&sOut, " = ", sizeof(" = ")-1);` |
|     2 | 2172 | `			}` |
|    71 | 2173 | `			ReflectExportValue(pCtx, &sOut, pMember, 0);` |
|    71 | 2174 | `			SyBlobAppend(&sOut, " ]\n", sizeof(" ]\n")-1);` |
|    71 | 2175 | `			pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|    36 | 2176 | `		}` |
|    11 | 2177 | `		SyBlobAppend(&sOut, "  }\n}\n", sizeof("  }\n}\n")-1);` |
|     - | 2178 | `	}` |
|    13 | 2179 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    13 | 2180 | `	SyBlobRelease(&sOut);` |
|    13 | 2181 | `	return PH7_OK;` |
|     1 | 2182 | `}` |
|     - | 2183 | `/* The body of the two methods php declares PRIVATE and never calls. Neither is` |
|     - | 2184 | `` * reachable from php code: `new ReflectionAttribute` is the engine's own "Call`` |
|     - | 2185 | `` * to private … from global scope" Error, and `clone` is refused by`` |
|     - | 2186 | ` * PH7_CLASS_NOCLONE before any body runs. They exist so the class REPORTS them,` |
|     - | 2187 | ` * which is the only way php's own dump shows them too. */` |
|   ! 0 | 2188 | `static int vm_builtin_ReflectionAttribute_private(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 2189 | `{` |
|   ! 0 | 2190 | `	SXUNUSED(nArg);` |
|   ! 0 | 2191 | `	SXUNUSED(apArg);` |
|   ! 0 | 2192 | `	SXUNUSED(pCtx);` |
|   ! 0 | 2193 | `	return PH7_OK;` |
|   ! 0 | 2194 | `}` |
|     - | 2195 | `/*` |
|     - | 2196 | ` * Declare ReflectionAttribute. Called from PH7_VmInstallReflection() where` |
|     - | 2197 | ` * chunk 7 used to be compiled, so Reflector (chunk 1) already exists.` |
|     - | 2198 | ` *` |
|     - | 2199 | ` * PH7_CLASS_NOCLONE is php's rule for it (php declares __clone private AND` |
|     - | 2200 | ` * refuses the copy), and the class is NOT final — php 8.5 unsealed it.` |
|     - | 2201 | ` */` |
|  5146 | 2202 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionAttribute(ph7_vm *pVm)` |
|     5 | 2203 | `{` |
|     - | 2204 | `	static const PH7_NativeConstDef aConst[] = {` |
|     - | 2205 | `		{ "IS_INSTANCEOF", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },` |
|     - | 2206 | `	};` |
|     - | 2207 | `	static const PH7_NativePropDef aProp[] = {` |
|     - | 2208 | `		{ RA_NAME,   PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|     - | 2209 | `		/* PHL-only, and PROTECTED so php code cannot reach them: the spec that` |
|     - | 2210 | `		 * reopens the target. php holds the same state on the C struct behind the` |
|     - | 2211 | `		 * object, invisible; PHL has no hidden-slot bit yet (§7.4 (e)), so these` |
|     - | 2212 | `		 * four still show up in a var_dump where php shows only $name. */` |
|     - | 2213 | `		{ RA_SPEC,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0,  0.0 }, 0 },` |
|     - | 2214 | `		{ RA_IDX,    PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0,  0.0 }, 0 },` |
|     - | 2215 | `		{ RA_TARGET, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0,  0.0 }, 0 },` |
|     - | 2216 | `		{ RA_REP,    PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0,  0.0 }, 0 },` |
|     - | 2217 | `	};` |
|     - | 2218 | `	/* php's own listing order, which is what __toString() prints. */` |
|     - | 2219 | `	static const PH7_NativeMethodDef aMethod[] = {` |
|     - | 2220 | `		{ "getName",      PH7_MOD_PUBLIC,  "", "string", vm_builtin_ReflectionAttribute_getName },` |
|     - | 2221 | `		{ "getTarget",    PH7_MOD_PUBLIC,  "", "int",    vm_builtin_ReflectionAttribute_getTarget },` |
|     - | 2222 | `		{ "isRepeated",   PH7_MOD_PUBLIC,  "", "bool",   vm_builtin_ReflectionAttribute_isRepeated },` |
|     - | 2223 | `		{ "getArguments", PH7_MOD_PUBLIC,  "", "array",  vm_builtin_ReflectionAttribute_getArguments },` |
|     - | 2224 | `		{ "newInstance",  PH7_MOD_PUBLIC,  "", "object", vm_builtin_ReflectionAttribute_newInstance },` |
|     - | 2225 | `		{ "__toString",   PH7_MOD_PUBLIC,  "", "string", vm_builtin_ReflectionAttribute_toString },` |
|     - | 2226 | `		{ "__clone",      PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionAttribute_private },` |
|     - | 2227 | `		{ "__construct",  PH7_MOD_PRIVATE, "", 0,      vm_builtin_ReflectionAttribute_private },` |
|     - | 2228 | `	};` |
|     - | 2229 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 2230 | `		{ "ReflectionAttribute", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 2231 | `		  aMethod, SX_ARRAYSIZE(aMethod), aConst, SX_ARRAYSIZE(aConst),` |
|     - | 2232 | `		  aProp, SX_ARRAYSIZE(aProp), 0, 0, 0 },` |
|     - | 2233 | `	};` |
|  5151 | 2234 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|     5 | 2235 | `}` |
|     - | 2236 | `/*` |
|     - | 2237 | ` * The shared getAttributes() body.` |
|     - | 2238 | ` *` |
|     - | 2239 | ` * Turns the target's #[...] records into ReflectionAttribute objects, applying` |
|     - | 2240 | ` * php's name / IS_INSTANCEOF filter. Argument VALUES stay lazy — what each` |
|     - | 2241 | ` * object carries is the [kind, target, member, paramIdx] spec that reopens` |
|     - | 2242 | ` * them — so a reflector declaring getAttributes() only has to say WHICH target` |
|     - | 2243 | ` * it is.` |
|     - | 2244 | ` */` |
|    92 | 2245 | `static int ReflectBuildAttrs(ph7_context *pCtx, SySet *pAttrs, const char *zKind,` |
|     - | 2246 | `	ph7_value *pTarget, const char *zMember, int nMember,` |
|     - | 2247 | `	int iParamIdx, int iTargetBit, int nArg, ph7_value **apArg)` |
|     2 | 2248 | `{` |
|    94 | 2249 | `	ph7_vm *pVm = pCtx->pVm;` |
|    94 | 2250 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|    94 | 2251 | `	ph7_class *pFilter = 0;` |
|    94 | 2252 | `	const char *zFilter = 0;` |
|    94 | 2253 | `	int nFilter = 0;` |
|     - | 2254 | `	ph7_value *pSpec, *pOut;` |
|     - | 2255 | `	sxu32 n, i;` |
|    94 | 2256 | `	pOut = ph7_context_new_array(pCtx);` |
|    94 | 2257 | `	pSpec = ph7_context_new_array(pCtx);` |
|    94 | 2258 | `	if( pOut == 0 \|\| pSpec == 0 ){` |
|   ! 0 | 2259 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2260 | `	}` |
|    94 | 2261 | `	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){` |
|    15 | 2262 | `		zFilter = ph7_value_to_string(apArg[0], &nFilter);` |
|    15 | 2263 | `		if( nFilter < 1 ){` |
|   ! 0 | 2264 | `			zFilter = 0;` |
|   ! 0 | 2265 | `		}` |
|     - | 2266 | `		/* IS_INSTANCEOF: keep a SUBCLASS of the named attribute too. The exact` |
|     - | 2267 | `		 * name still matches on its own, so this only has to answer for the rest. */` |
|    15 | 2268 | `		if( zFilter && nArg > 1 && (ph7_value_to_int(apArg[1]) & 2) ){` |
|     5 | 2269 | `			pFilter = ReflectResolveClass(pVm, apArg[0]);` |
|     2 | 2270 | `		}` |
|     7 | 2271 | `	}` |
|     - | 2272 | `	{` |
|    94 | 2273 | `		ph7_value *pKind = ph7_context_new_scalar(pCtx);` |
|    94 | 2274 | `		ph7_value *pMem  = ph7_context_new_scalar(pCtx);` |
|    94 | 2275 | `		ph7_value *pIdx  = ph7_context_new_scalar(pCtx);` |
|    94 | 2276 | `		if( pKind == 0 \|\| pMem == 0 \|\| pIdx == 0 ){` |
|   ! 0 | 2277 | `			return PH7_ContextMemoryError(pCtx);` |
|     - | 2278 | `		}` |
|    94 | 2279 | `		ph7_value_string(pKind, zKind, -1);` |
|    94 | 2280 | `		if( zMember ){` |
|    25 | 2281 | `			ph7_value_string(pMem, zMember, nMember);` |
|    13 | 2282 | `		}else{` |
|    70 | 2283 | `			ph7_value_null(pMem);` |
|     - | 2284 | `		}` |
|    94 | 2285 | `		ph7_value_int(pIdx, iParamIdx);` |
|    94 | 2286 | `		ph7_array_add_elem(pSpec, 0, pKind);` |
|     - | 2287 | `		/* The target rides as a VALUE, not a name: a closure's attributes are` |
|     - | 2288 | `		 * reopened through the Closure object itself. */` |
|    94 | 2289 | `		ph7_array_add_elem(pSpec, 0, pTarget);` |
|    94 | 2290 | `		ph7_array_add_elem(pSpec, 0, pMem);` |
|    94 | 2291 | `		ph7_array_add_elem(pSpec, 0, pIdx);` |
|     - | 2292 | `	}` |
|   238 | 2293 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|   146 | 2294 | `		int bRepeated = 0;` |
|   146 | 2295 | `		if( zFilter ){` |
|    89 | 2296 | `			int bKeep = ((int)SyStringLength(&aA[n].sName) == nFilter` |
|    50 | 2297 | `				&& SyStrnicmp(SyStringData(&aA[n].sName), zFilter, (sxu32)nFilter) == 0);` |
|    51 | 2298 | `			if( !bKeep && pFilter ){` |
|    16 | 2299 | `				ph7_class *pCand = PH7_VmExtractClass(pVm, SyStringData(&aA[n].sName),` |
|    10 | 2300 | `					SyStringLength(&aA[n].sName), FALSE, 0);` |
|    11 | 2301 | `				if( pCand == 0 ){` |
|   ! 0 | 2302 | `					pCand = PH7_VmTriggerAutoload(pVm, SyStringData(&aA[n].sName),` |
|   ! 0 | 2303 | `						SyStringLength(&aA[n].sName), FALSE);` |
|   ! 0 | 2304 | `				}` |
|    11 | 2305 | `				bKeep = pCand != 0 && pCand != pFilter && PH7_VmInstanceOf(pCand, pFilter);` |
|     5 | 2306 | `			}` |
|    51 | 2307 | `			if( !bKeep ){` |
|    27 | 2308 | `				continue;` |
|     - | 2309 | `			}` |
|    12 | 2310 | `		}` |
|     - | 2311 | `		/* isRepeated() asks about the TARGET, not about the filtered result:` |
|     - | 2312 | `		 * two #[A] make both of them repeated even when only one is asked for. */` |
|   274 | 2313 | `		for( i = 0 ; i < SySetUsed(pAttrs) ; i++ ){` |
|   190 | 2314 | `			if( i != n && SyStringLength(&aA[i].sName) == SyStringLength(&aA[n].sName)` |
|    68 | 2315 | `			 && SyStrnicmp(SyStringData(&aA[i].sName), SyStringData(&aA[n].sName),` |
|    60 | 2316 | `				SyStringLength(&aA[n].sName)) == 0 ){` |
|    37 | 2317 | `				bRepeated = 1;` |
|    37 | 2318 | `				break;` |
|     - | 2319 | `			}` |
|    79 | 2320 | `		}` |
|   179 | 2321 | `		ReflectTypeListAdd(pCtx, pOut,` |
|   118 | 2322 | `			ReflectAttrNew(pCtx, &aA[n].sName, pSpec, n, iTargetBit, bRepeated));` |
|    61 | 2323 | `	}` |
|    94 | 2324 | `	ph7_result_value(pCtx, pOut);` |
|    94 | 2325 | `	return PH7_OK;` |
|    48 | 2326 | `}` |
|     - | 2327 | `/*` |
|     - | 2328 | ` * The three "no runtime line tracking" methods. php answers a file/line/trace` |
|     - | 2329 | ` * from the executing frame; PHL has no per-instruction line record (the same` |
|     - | 2330 | ` * gap debug_backtrace() has), so it says so loudly rather than inventing one.` |
|     - | 2331 | ` */` |
|   ! 0 | 2332 | `static int ReflectUnsupported(ph7_context *pCtx, const char *zWho)` |
|   ! 0 | 2333 | `{` |
|   ! 0 | 2334 | `	return PH7_VmThrowException(pCtx, "Error",` |
|   ! 0 | 2335 | `		"%s is not supported by PHL (no runtime line tracking)", zWho);` |
|   ! 0 | 2336 | `}` |
|     - | 2337 | `#define REFLECT_UNSUPPORTED(NAME,TEXT) \` |
|     - | 2338 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|     - | 2339 | `	{ \` |
|     - | 2340 | `		SXUNUSED(nArg); \` |
|     - | 2341 | `		SXUNUSED(apArg); \` |
|     - | 2342 | `		return ReflectUnsupported(pCtx, TEXT); \` |
|     - | 2343 | `	}` |
|   ! 0 | 2344 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_execLine,"ReflectionGenerator::getExecutingLine()")` |
|   ! 0 | 2345 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_execFile,"ReflectionGenerator::getExecutingFile()")` |
|   ! 0 | 2346 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_trace,"ReflectionGenerator::getTrace()")` |
|   ! 0 | 2347 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_execLine,"ReflectionFiber::getExecutingLine()")` |
|   ! 0 | 2348 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_execFile,"ReflectionFiber::getExecutingFile()")` |
|   ! 0 | 2349 | `REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_trace,"ReflectionFiber::getTrace()")` |
|     - | 2350 |  |
|     - | 2351 | `/* ReflectionGenerator::__construct(Generator $generator) */` |
|    16 | 2352 | `static int vm_builtin_ReflectionGenerator_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2353 | `{` |
|    17 | 2354 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    17 | 2355 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 | 2356 | `		return PH7_OK;` |
|     - | 2357 | `	}` |
|    17 | 2358 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RG_GEN, (ph7_class_instance *)apArg[0]->x.pOther);` |
|    17 | 2359 | `	return PH7_OK;` |
|     9 | 2360 | `}` |
|     - | 2361 | `/* The coroutine context of the wrapped generator, or NULL. */` |
|    34 | 2362 | `static ph7_exec_ctx * ReflectGenExec(ph7_context *pCtx)` |
|     1 | 2363 | `{` |
|    35 | 2364 | `	ph7_class_instance *pGenObj = ReflectWrapped(pCtx, RG_GEN);` |
|     - | 2365 | `	ph7_value sVal;` |
|     - | 2366 | `	ph7_generator *pGen;` |
|    35 | 2367 | `	if( pGenObj == 0 ){` |
|   ! 0 | 2368 | `		return 0;` |
|     - | 2369 | `	}` |
|    35 | 2370 | `	PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    35 | 2371 | `	sVal.x.pOther = pGenObj;` |
|    35 | 2372 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|    35 | 2373 | `	pGen = ReflectGeneratorCtx(pCtx->pVm, &sVal);` |
|    35 | 2374 | `	return pGen ? pGen->pCtx : 0;` |
|    18 | 2375 | `}` |
|     8 | 2376 | `static int vm_builtin_ReflectionGenerator_isClosed(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2377 | `{` |
|     9 | 2378 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|     4 | 2379 | `	SXUNUSED(nArg);` |
|     4 | 2380 | `	SXUNUSED(apArg);` |
|    17 | 2381 | `	ph7_result_bool(pCtx, pExec != 0` |
|    12 | 2382 | `		&& (pExec->iState == PH7_CTX_STATE_COMPLETED \|\| pExec->iState == PH7_CTX_STATE_CLOSED));` |
|     9 | 2383 | `	return PH7_OK;` |
|     1 | 2384 | `}` |
|     - | 2385 | `/* ReflectionGenerator::getThis(): ?object — the receiver the generator's body` |
|     - | 2386 | ` * runs against. A coroutine frame installs it as a frame VARIABLE (see` |
|     - | 2387 | ` * VmFiberSetupFrame), not as pFrame->pThis, so both are checked. */` |
|     8 | 2388 | `static int vm_builtin_ReflectionGenerator_getThis(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2389 | `{` |
|     9 | 2390 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|     4 | 2391 | `	SXUNUSED(nArg);` |
|     4 | 2392 | `	SXUNUSED(apArg);` |
|     9 | 2393 | `	if( pExec && pExec->pFrame ){` |
|     9 | 2394 | `		SyHashEntry *pVar = SyHashGet(&pExec->pFrame->hVar, "this", sizeof("this")-1);` |
|     9 | 2395 | `		if( pVar ){` |
|     7 | 2396 | `			ph7_value *pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,` |
|     4 | 2397 | `				(sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|     5 | 2398 | `			if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|     5 | 2399 | `				ph7_result_value(pCtx, pSlot);` |
|     5 | 2400 | `				return PH7_OK;` |
|     - | 2401 | `			}` |
|   ! 0 | 2402 | `		}` |
|     5 | 2403 | `		if( pExec->pFrame->pThis ){` |
|   ! 0 | 2404 | `			return ReflectResultBorrowed(pCtx, pExec->pFrame->pThis);` |
|     - | 2405 | `		}` |
|     2 | 2406 | `	}` |
|     5 | 2407 | `	ph7_result_null(pCtx);` |
|     5 | 2408 | `	return PH7_OK;` |
|     5 | 2409 | `}` |
|     - | 2410 | `/* ReflectionGenerator::getFunction(): ReflectionFunctionAbstract — still a` |
|     - | 2411 | ` * prelude class, so it is built through its own constructor. */` |
|    18 | 2412 | `static int vm_builtin_ReflectionGenerator_getFunction(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2413 | `{` |
|    19 | 2414 | `	ph7_vm *pVm = pCtx->pVm;` |
|    19 | 2415 | `	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);` |
|    19 | 2416 | `	ph7_vm_func *pFunc = pExec ? pExec->pFunc : 0;` |
|     - | 2417 | `	ph7_class_instance *pOut;` |
|     - | 2418 | `	ph7_value aArg[2];` |
|    19 | 2419 | `	int nCtorArg = 1;` |
|     - | 2420 | `	sxi32 rc;` |
|     9 | 2421 | `	SXUNUSED(nArg);` |
|     9 | 2422 | `	SXUNUSED(apArg);` |
|    19 | 2423 | `	if( pFunc == 0 ){` |
|   ! 0 | 2424 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2425 | `		return PH7_OK;` |
|     - | 2426 | `	}` |
|    19 | 2427 | `	PH7_MemObjInit(pVm, &aArg[0]);` |
|    19 | 2428 | `	PH7_MemObjInit(pVm, &aArg[1]);` |
|    19 | 2429 | `	if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     9 | 2430 | `		ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|     9 | 2431 | `		ph7_value_string(&aArg[0], SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|     9 | 2432 | `		ph7_value_string(&aArg[1], SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|     9 | 2433 | `		nCtorArg = 2;` |
|     5 | 2434 | `	}else{` |
|    11 | 2435 | `		ph7_value_string(&aArg[0], SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|     - | 2436 | `	}` |
|     - | 2437 | `	{` |
|     - | 2438 | `		ph7_value *apCtor[2];` |
|    19 | 2439 | `		apCtor[0] = &aArg[0];` |
|    19 | 2440 | `		apCtor[1] = &aArg[1];` |
|    28 | 2441 | `		pOut = ReflectConstruct(pCtx, nCtorArg == 2 ? "ReflectionMethod" : "ReflectionFunction",` |
|     9 | 2442 | `			nCtorArg, apCtor, &rc);` |
|     - | 2443 | `	}` |
|    19 | 2444 | `	PH7_MemObjRelease(&aArg[0]);` |
|    19 | 2445 | `	PH7_MemObjRelease(&aArg[1]);` |
|    19 | 2446 | `	if( pOut == 0 ){` |
|   ! 0 | 2447 | `		if( rc != PH7_OK ){` |
|   ! 0 | 2448 | `			return rc;` |
|     - | 2449 | `		}` |
|   ! 0 | 2450 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2451 | `		return PH7_OK;` |
|     - | 2452 | `	}` |
|    19 | 2453 | `	return ReflectResultObject(pCtx, pOut);` |
|    10 | 2454 | `}` |
|     - | 2455 | `` /* ReflectionGenerator::getExecutingGenerator(): Generator — follow `yield from` `` |
|     - | 2456 | ` * delegation to the innermost one that is actually running. */` |
|     6 | 2457 | `static int vm_builtin_ReflectionGenerator_getExecuting(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2458 | `{` |
|     7 | 2459 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 2460 | `	ph7_class_instance *pGenObj = ReflectWrapped(pCtx, RG_GEN);` |
|     - | 2461 | `	ph7_value sVal, *pCur;` |
|     - | 2462 | `	ph7_generator *pGen;` |
|     7 | 2463 | `	int iDepth = 0;` |
|     3 | 2464 | `	SXUNUSED(nArg);` |
|     3 | 2465 | `	SXUNUSED(apArg);` |
|     7 | 2466 | `	if( pGenObj == 0 ){` |
|   ! 0 | 2467 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2468 | `		return PH7_OK;` |
|     - | 2469 | `	}` |
|     7 | 2470 | `	PH7_MemObjInit(pVm, &sVal);` |
|     7 | 2471 | `	sVal.x.pOther = pGenObj;` |
|     7 | 2472 | `	sVal.iFlags = MEMOBJ_OBJ;` |
|     7 | 2473 | `	pCur = &sVal;` |
|     7 | 2474 | `	pGen = ReflectGeneratorCtx(pVm, &sVal);` |
|    12 | 2475 | `	while( pGen && pGen->pCtx && pGen->pCtx->iDelegateState == 3` |
|    10 | 2476 | `	 && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|     3 | 2477 | `		ph7_generator *pInner = ReflectGeneratorCtx(pVm, &pGen->pCtx->sDelegate);` |
|     3 | 2478 | `		if( pInner == 0 ){` |
|   ! 0 | 2479 | `			break;` |
|     - | 2480 | `		}` |
|     3 | 2481 | `		pCur = &pGen->pCtx->sDelegate;` |
|     3 | 2482 | `		pGen = pInner;` |
|     3 | 2483 | `		iDepth++;` |
|     1 | 2484 | `	}` |
|     7 | 2485 | `	return ReflectResultBorrowed(pCtx, (ph7_class_instance *)pCur->x.pOther);` |
|     4 | 2486 | `}` |
|     - | 2487 | `/* ReflectionFiber::__construct(Fiber $fiber) / getFiber() / getCallable() */` |
|     4 | 2488 | `static int vm_builtin_ReflectionFiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2489 | `{` |
|     5 | 2490 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 2491 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 | 2492 | `		return PH7_OK;` |
|     - | 2493 | `	}` |
|     5 | 2494 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RF_FIBER, (ph7_class_instance *)apArg[0]->x.pOther);` |
|     5 | 2495 | `	return PH7_OK;` |
|     3 | 2496 | `}` |
|     4 | 2497 | `static int vm_builtin_ReflectionFiber_getFiber(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2498 | `{` |
|     2 | 2499 | `	SXUNUSED(nArg);` |
|     2 | 2500 | `	SXUNUSED(apArg);` |
|     5 | 2501 | `	return ReflectResultBorrowed(pCtx, ReflectWrapped(pCtx, RF_FIBER));` |
|     1 | 2502 | `}` |
|     4 | 2503 | `static int vm_builtin_ReflectionFiber_getCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2504 | `{` |
|     5 | 2505 | `	ph7_class_instance *pFiber = ReflectWrapped(pCtx, RF_FIBER);` |
|     5 | 2506 | `	ph7_value *pVal = pFiber ? PH7_NativeAttr(pFiber, "__callable") : 0;` |
|     2 | 2507 | `	SXUNUSED(nArg);` |
|     2 | 2508 | `	SXUNUSED(apArg);` |
|     5 | 2509 | `	if( pVal ){` |
|     5 | 2510 | `		ph7_result_value(pCtx, pVal);` |
|     2 | 2511 | `	}` |
|     5 | 2512 | `	return PH7_OK;` |
|     1 | 2513 | `}` |
|     - | 2514 | `/* ---- ReflectionConstant ---- */` |
|     - | 2515 | `/* The engine's record for a global constant, or NULL when undefined. */` |
|    60 | 2516 | `static ph7_constant * ReflectConstEntry(ph7_vm *pVm, const char *zName, int nName)` |
|     1 | 2517 | `{` |
|    61 | 2518 | `	SyHashEntry *pEntry = nName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zName, (sxu32)nName) : 0;` |
|    61 | 2519 | `	return pEntry ? (ph7_constant *)pEntry->pUserData : 0;` |
|     1 | 2520 | `}` |
|     - | 2521 | `/* The receiver's constant record, resolved from its public $name. */` |
|    34 | 2522 | `static ph7_constant * ReflectConstOf(ph7_context *pCtx)` |
|     1 | 2523 | `{` |
|    35 | 2524 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    35 | 2525 | `	const char *zName = "";` |
|    35 | 2526 | `	int nName = 0;` |
|    35 | 2527 | `	if( pThis ){` |
|    35 | 2528 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    17 | 2529 | `	}` |
|    35 | 2530 | `	return ReflectConstEntry(pCtx->pVm, zName, nName);` |
|     1 | 2531 | `}` |
|    26 | 2532 | `static int vm_builtin_ReflectionConstant_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2533 | `{` |
|    27 | 2534 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    27 | 2535 | `	int nName = 0;` |
|    27 | 2536 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|    27 | 2537 | `	if( pThis == 0 ){` |
|   ! 0 | 2538 | `		return PH7_OK;` |
|     - | 2539 | `	}` |
|    27 | 2540 | `	if( ReflectConstEntry(pCtx->pVm, zName, nName) == 0 ){` |
|    10 | 2541 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     3 | 2542 | `			"Constant \"%.*s\" does not exist", nName, zName);` |
|     - | 2543 | `	}` |
|    21 | 2544 | `	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", zName, nName);` |
|    21 | 2545 | `	return PH7_OK;` |
|    14 | 2546 | `}` |
|     4 | 2547 | `static int vm_builtin_ReflectionConstant_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2548 | `{` |
|     5 | 2549 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 2550 | `	const char *zName = "";` |
|     5 | 2551 | `	int nName = 0;` |
|     2 | 2552 | `	SXUNUSED(nArg);` |
|     2 | 2553 | `	SXUNUSED(apArg);` |
|     5 | 2554 | `	if( pThis ){` |
|     5 | 2555 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     2 | 2556 | `	}` |
|     5 | 2557 | `	ph7_result_string(pCtx, zName, nName);` |
|     5 | 2558 | `	return PH7_OK;` |
|     1 | 2559 | `}` |
|     - | 2560 | `/* The namespace split php reports: everything before the last '\', and the rest. */` |
|     8 | 2561 | `static int ReflectConstNsCut(const char *zName, int nName)` |
|     1 | 2562 | `{` |
|     - | 2563 | `	int k;` |
|   101 | 2564 | `	for( k = nName - 1 ; k >= 0 ; k-- ){` |
|    93 | 2565 | `		if( zName[k] == '\\' ){` |
|   ! 0 | 2566 | `			return k;` |
|     - | 2567 | `		}` |
|    47 | 2568 | `	}` |
|     9 | 2569 | `	return -1;` |
|     5 | 2570 | `}` |
|     4 | 2571 | `static int vm_builtin_ReflectionConstant_getNamespaceName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2572 | `{` |
|     5 | 2573 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 2574 | `	const char *zName = "";` |
|     5 | 2575 | `	int nName = 0, iCut;` |
|     2 | 2576 | `	SXUNUSED(nArg);` |
|     2 | 2577 | `	SXUNUSED(apArg);` |
|     5 | 2578 | `	if( pThis ){` |
|     5 | 2579 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     2 | 2580 | `	}` |
|     5 | 2581 | `	iCut = ReflectConstNsCut(zName, nName);` |
|     5 | 2582 | `	ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|     5 | 2583 | `	return PH7_OK;` |
|     1 | 2584 | `}` |
|     4 | 2585 | `static int vm_builtin_ReflectionConstant_getShortName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2586 | `{` |
|     5 | 2587 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     5 | 2588 | `	const char *zName = "";` |
|     5 | 2589 | `	int nName = 0, iCut;` |
|     2 | 2590 | `	SXUNUSED(nArg);` |
|     2 | 2591 | `	SXUNUSED(apArg);` |
|     5 | 2592 | `	if( pThis ){` |
|     5 | 2593 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     2 | 2594 | `	}` |
|     5 | 2595 | `	iCut = ReflectConstNsCut(zName, nName);` |
|     5 | 2596 | `	if( iCut < 0 ){` |
|     5 | 2597 | `		ph7_result_string(pCtx, zName, nName);` |
|     3 | 2598 | `	}else{` |
|   ! 0 | 2599 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|     - | 2600 | `	}` |
|     5 | 2601 | `	return PH7_OK;` |
|     1 | 2602 | `}` |
|     8 | 2603 | `static int vm_builtin_ReflectionConstant_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2604 | `{` |
|     9 | 2605 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|     - | 2606 | `	ph7_value sValue;` |
|     4 | 2607 | `	SXUNUSED(nArg);` |
|     4 | 2608 | `	SXUNUSED(apArg);` |
|     9 | 2609 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     9 | 2610 | `	if( pCons && pCons->xExpand ){` |
|     9 | 2611 | `		pCons->xExpand(&sValue, pCons->pUserData);` |
|     4 | 2612 | `	}` |
|     9 | 2613 | `	ph7_result_value(pCtx, &sValue);` |
|     9 | 2614 | `	PH7_MemObjRelease(&sValue);` |
|     9 | 2615 | `	return PH7_OK;` |
|     1 | 2616 | `}` |
|     4 | 2617 | `static int vm_builtin_ReflectionConstant_isDeprecated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2618 | `{` |
|     2 | 2619 | `	SXUNUSED(nArg);` |
|     2 | 2620 | `	SXUNUSED(apArg);` |
|     5 | 2621 | `	ph7_result_bool(pCtx, 0);` |
|     5 | 2622 | `	return PH7_OK;` |
|     1 | 2623 | `}` |
|     8 | 2624 | `static int vm_builtin_ReflectionConstant_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2625 | `{` |
|     9 | 2626 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|     4 | 2627 | `	SXUNUSED(nArg);` |
|     4 | 2628 | `	SXUNUSED(apArg);` |
|     9 | 2629 | `	if( pCons && SyStringLength(&pCons->sFile) > 0 ){` |
|     5 | 2630 | `		ph7_result_string(pCtx, SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));` |
|     3 | 2631 | `	}else{` |
|     5 | 2632 | `		ph7_result_bool(pCtx, 0);` |
|     - | 2633 | `	}` |
|     9 | 2634 | `	return PH7_OK;` |
|     1 | 2635 | `}` |
|     - | 2636 | `/* An engine constant belongs to the synthetic "Core" extension; a userland` |
|     - | 2637 | ` * define() belongs to none, which php reports as null / false. */` |
|     8 | 2638 | `static int vm_builtin_ReflectionConstant_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2639 | `{` |
|     9 | 2640 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|     - | 2641 | `	ph7_class_instance *pExt;` |
|     - | 2642 | `	ph7_value sName, *apArgs[1];` |
|     - | 2643 | `	sxi32 rc;` |
|     4 | 2644 | `	SXUNUSED(nArg);` |
|     4 | 2645 | `	SXUNUSED(apArg);` |
|     9 | 2646 | `	if( pCons == 0 \|\| pCons->bUserDefined ){` |
|     3 | 2647 | `		ph7_result_null(pCtx);` |
|     3 | 2648 | `		return PH7_OK;` |
|     - | 2649 | `	}` |
|     7 | 2650 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|     7 | 2651 | `	ph7_value_string(&sName, "Core", 4);` |
|     7 | 2652 | `	apArgs[0] = &sName;` |
|     7 | 2653 | `	pExt = ReflectConstruct(pCtx, "ReflectionExtension", 1, apArgs, &rc);` |
|     7 | 2654 | `	PH7_MemObjRelease(&sName);` |
|     7 | 2655 | `	if( pExt == 0 ){` |
|   ! 0 | 2656 | `		if( rc != PH7_OK ){` |
|   ! 0 | 2657 | `			return rc;` |
|     - | 2658 | `		}` |
|   ! 0 | 2659 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2660 | `		return PH7_OK;` |
|     - | 2661 | `	}` |
|     7 | 2662 | `	return ReflectResultObject(pCtx, pExt);` |
|     5 | 2663 | `}` |
|     8 | 2664 | `static int vm_builtin_ReflectionConstant_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2665 | `{` |
|     9 | 2666 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|     4 | 2667 | `	SXUNUSED(nArg);` |
|     4 | 2668 | `	SXUNUSED(apArg);` |
|     9 | 2669 | `	if( pCons && pCons->bUserDefined == 0 ){` |
|     5 | 2670 | `		ph7_result_string(pCtx, "Core", 4);` |
|     3 | 2671 | `	}else{` |
|     5 | 2672 | `		ph7_result_bool(pCtx, 0);` |
|     - | 2673 | `	}` |
|     9 | 2674 | `	return PH7_OK;` |
|     1 | 2675 | `}` |
|     - | 2676 | `/* getAttributes() still routes through the prelude builder: chunk 7 owns the` |
|     - | 2677 | ` * ReflectionAttribute shape and the lazy argument evaluation. */` |
|     2 | 2678 | `static int vm_builtin_ReflectionConstant_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2679 | `{` |
|     3 | 2680 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     3 | 2681 | `	ph7_constant *pCons = ReflectConstOf(pCtx);` |
|     3 | 2682 | `	const char *zName = "";` |
|     3 | 2683 | `	int nName = 0;` |
|     3 | 2684 | `	if( pThis == 0 \|\| pCons == 0 ){` |
|   ! 0 | 2685 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|   ! 0 | 2686 | `		return PH7_OK;` |
|     - | 2687 | `	}` |
|     3 | 2688 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     - | 2689 | `	{` |
|     - | 2690 | `		ph7_value sTarget;` |
|     - | 2691 | `		int rc;` |
|     3 | 2692 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     3 | 2693 | `		ph7_value_string(&sTarget, zName, nName);` |
|     - | 2694 | `		/* 64 = Attribute::TARGET_CONSTANT */` |
|     4 | 2695 | `		rc = ReflectBuildAttrs(pCtx, &pCons->aAttrs, "const", &sTarget, 0, 0, 0, 64,` |
|     1 | 2696 | `			nArg, apArg);` |
|     3 | 2697 | `		PH7_MemObjRelease(&sTarget);` |
|     3 | 2698 | `		return rc;` |
|     - | 2699 | `	}` |
|     2 | 2700 | `}` |
|   ! 0 | 2701 | `static int vm_builtin_ReflectionConstant_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 2702 | `{` |
|   ! 0 | 2703 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   ! 0 | 2704 | `	const char *zName = "";` |
|   ! 0 | 2705 | `	int nName = 0;` |
|   ! 0 | 2706 | `	SXUNUSED(nArg);` |
|   ! 0 | 2707 | `	SXUNUSED(apArg);` |
|   ! 0 | 2708 | `	if( pThis ){` |
|   ! 0 | 2709 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|   ! 0 | 2710 | `	}` |
|   ! 0 | 2711 | `	ph7_result_string_format(pCtx, "Constant [ %.*s ]\n", nName, zName);` |
|   ! 0 | 2712 | `	return PH7_OK;` |
|   ! 0 | 2713 | `}` |
|     - | 2714 | `/* ---- ReflectionExtension: PHL has exactly one, the synthetic "Core" ---- */` |
|    24 | 2715 | `static int vm_builtin_ReflectionExtension_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2716 | `{` |
|    25 | 2717 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    25 | 2718 | `	int nName = 0;` |
|    25 | 2719 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|    25 | 2720 | `	if( pThis == 0 ){` |
|   ! 0 | 2721 | `		return PH7_OK;` |
|     - | 2722 | `	}` |
|    34 | 2723 | `	if( !(nName == 4 && SyToLower(zName[0]) == 'c' && SyToLower(zName[1]) == 'o'` |
|    18 | 2724 | `	   && SyToLower(zName[2]) == 'r' && SyToLower(zName[3]) == 'e') ){` |
|    10 | 2725 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     3 | 2726 | `			"Extension \"%.*s\" does not exist", nName, zName);` |
|     - | 2727 | `	}` |
|    19 | 2728 | `	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", "Core", 4);` |
|    19 | 2729 | `	return PH7_OK;` |
|    13 | 2730 | `}` |
|    10 | 2731 | `static int vm_builtin_ReflectionExtension_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2732 | `{` |
|    11 | 2733 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    11 | 2734 | `	const char *zName = "";` |
|    11 | 2735 | `	int nName = 0;` |
|     5 | 2736 | `	SXUNUSED(nArg);` |
|     5 | 2737 | `	SXUNUSED(apArg);` |
|    11 | 2738 | `	if( pThis ){` |
|    11 | 2739 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     5 | 2740 | `	}` |
|    11 | 2741 | `	ph7_result_string(pCtx, zName, nName);` |
|    11 | 2742 | `	return PH7_OK;` |
|     1 | 2743 | `}` |
|   ! 0 | 2744 | `static int vm_builtin_ReflectionExtension_getVersion(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 2745 | `{` |
|     - | 2746 | `	ph7_value sFn, sRes;` |
|     - | 2747 | `	SyString sStr;` |
|   ! 0 | 2748 | `	SXUNUSED(nArg);` |
|   ! 0 | 2749 | `	SXUNUSED(apArg);` |
|   ! 0 | 2750 | `	PH7_MemObjInit(pCtx->pVm, &sFn);` |
|   ! 0 | 2751 | `	PH7_MemObjInit(pCtx->pVm, &sRes);` |
|   ! 0 | 2752 | `	SyStringInitFromBuf(&sStr, "phpversion", sizeof("phpversion")-1);` |
|   ! 0 | 2753 | `	PH7_MemObjInitFromString(pCtx->pVm, &sFn, &sStr);` |
|   ! 0 | 2754 | `	if( PH7_VmCallUserFunction(pCtx->pVm, &sFn, 0, 0, &sRes) == SXRET_OK ){` |
|   ! 0 | 2755 | `		ph7_result_value(pCtx, &sRes);` |
|   ! 0 | 2756 | `	}` |
|   ! 0 | 2757 | `	PH7_MemObjRelease(&sFn);` |
|   ! 0 | 2758 | `	PH7_MemObjRelease(&sRes);` |
|   ! 0 | 2759 | `	return PH7_OK;` |
|   ! 0 | 2760 | `}` |
|   ! 0 | 2761 | `static int vm_builtin_ReflectionExtension_emptyArray(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 2762 | `{` |
|   ! 0 | 2763 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|   ! 0 | 2764 | `	SXUNUSED(nArg);` |
|   ! 0 | 2765 | `	SXUNUSED(apArg);` |
|   ! 0 | 2766 | `	if( pList ){` |
|   ! 0 | 2767 | `		ph7_result_value(pCtx, pList);` |
|   ! 0 | 2768 | `	}` |
|   ! 0 | 2769 | `	return PH7_OK;` |
|   ! 0 | 2770 | `}` |
|   ! 0 | 2771 | `static int vm_builtin_ReflectionExtension_isPersistent(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 2772 | `{` |
|   ! 0 | 2773 | `	SXUNUSED(nArg);` |
|   ! 0 | 2774 | `	SXUNUSED(apArg);` |
|   ! 0 | 2775 | `	ph7_result_bool(pCtx, 1);` |
|   ! 0 | 2776 | `	return PH7_OK;` |
|   ! 0 | 2777 | `}` |
|   ! 0 | 2778 | `static int vm_builtin_ReflectionExtension_isTemporary(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 2779 | `{` |
|   ! 0 | 2780 | `	SXUNUSED(nArg);` |
|   ! 0 | 2781 | `	SXUNUSED(apArg);` |
|   ! 0 | 2782 | `	ph7_result_bool(pCtx, 0);` |
|   ! 0 | 2783 | `	return PH7_OK;` |
|   ! 0 | 2784 | `}` |
|   ! 0 | 2785 | `static int vm_builtin_ReflectionExtension_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 2786 | `{` |
|   ! 0 | 2787 | `	SXUNUSED(nArg);` |
|   ! 0 | 2788 | `	SXUNUSED(apArg);` |
|   ! 0 | 2789 | `	SXUNUSED(pCtx);` |
|   ! 0 | 2790 | `	return PH7_OK;` |
|   ! 0 | 2791 | `}` |
|   ! 0 | 2792 | `static int vm_builtin_ReflectionExtension_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 2793 | `{` |
|   ! 0 | 2794 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   ! 0 | 2795 | `	const char *zName = "";` |
|   ! 0 | 2796 | `	int nName = 0;` |
|   ! 0 | 2797 | `	SXUNUSED(nArg);` |
|   ! 0 | 2798 | `	SXUNUSED(apArg);` |
|   ! 0 | 2799 | `	if( pThis ){` |
|   ! 0 | 2800 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|   ! 0 | 2801 | `	}` |
|   ! 0 | 2802 | `	ph7_result_string_format(pCtx, "Extension [ extension #1 %.*s ]\n", nName, zName);` |
|   ! 0 | 2803 | `	return PH7_OK;` |
|   ! 0 | 2804 | `}` |
|     - | 2805 | `/* ---- ReflectionZendExtension: none exist, so the constructor always refuses ---- */` |
|     4 | 2806 | `static int vm_builtin_ReflectionZendExtension_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2807 | `{` |
|     5 | 2808 | `	int nName = 0;` |
|     5 | 2809 | `	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";` |
|     7 | 2810 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     2 | 2811 | `		"Zend Extension \"%.*s\" does not exist", nName, zName);` |
|     1 | 2812 | `}` |
|   ! 0 | 2813 | `static int vm_builtin_ReflectionZendExtension_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 2814 | `{` |
|   ! 0 | 2815 | `	SXUNUSED(nArg);` |
|   ! 0 | 2816 | `	SXUNUSED(apArg);` |
|   ! 0 | 2817 | `	ph7_result_string(pCtx, "", 0);` |
|   ! 0 | 2818 | `	return PH7_OK;` |
|   ! 0 | 2819 | `}` |
|     - | 2820 | `/* ---- ReflectionReference ---- */` |
|     - | 2821 | `/*` |
|     - | 2822 | ` * ReflectionReference::fromArrayElement(array $array, string\|int $key)` |
|     - | 2823 | ` *` |
|     - | 2824 | ` * Answers a reflector only when the element IS a reference — its slot carries a` |
|     - | 2825 | ` * reference-table record with at least two links. The instance is built without` |
|     - | 2826 | ` * running the (private) constructor, exactly as php's factory does.` |
|     - | 2827 | ` */` |
|    18 | 2828 | `static int vm_builtin_ReflectionReference_fromArrayElement(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2829 | `{` |
|    19 | 2830 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 2831 | `	ph7_hashmap *pMap;` |
|    19 | 2832 | `	ph7_hashmap_node *pNode = 0;` |
|     - | 2833 | `	ph7_class *pClass;` |
|     - | 2834 | `	ph7_class_instance *pObj;` |
|     - | 2835 | `	char zId[64];` |
|    19 | 2836 | `	if( nArg < 1 ){` |
|   ! 0 | 2837 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2838 | `		return PH7_OK;` |
|     - | 2839 | `	}` |
|    19 | 2840 | `	if( !ph7_value_is_array(apArg[0]) ){` |
|     - | 2841 | ``		/* The shared ZPP screen leaves a SCALAR against a bare `array` parameter`` |
|     - | 2842 | `		 * unscreened (it only judges array/object/null/resource pairings and` |
|     - | 2843 | `		 * scalars against class-typed ones), so the refusal php words from the` |
|     - | 2844 | `		 * declared type is written here -- as the prelude's own is_array() check` |
|     - | 2845 | `		 * did. Recorded in PLAN §2 with the rest of that gap. */` |
|   ! 0 | 2846 | `		return PH7_VmThrowException(pCtx, "TypeError",` |
|     - | 2847 | `			"ReflectionReference::fromArrayElement(): Argument #1 ($array) "` |
|   ! 0 | 2848 | `			"must be of type array, %s given", ph7_type_name(apArg[0]));` |
|     - | 2849 | `	}` |
|    19 | 2850 | `	if( nArg < 2 ){` |
|   ! 0 | 2851 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2852 | `		return PH7_OK;` |
|     - | 2853 | `	}` |
|    19 | 2854 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|    19 | 2855 | `	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK \|\| pNode == 0 ){` |
|   ! 0 | 2856 | `		ph7_result_null(pCtx);` |
|   ! 0 | 2857 | `		return PH7_OK;` |
|     - | 2858 | `	}` |
|    19 | 2859 | `	if( PH7_VmSlotRefCount(pVm, pNode->nValIdx) < 2 ){` |
|     7 | 2860 | `		ph7_result_null(pCtx);` |
|     7 | 2861 | `		return PH7_OK;` |
|     - | 2862 | `	}` |
|    13 | 2863 | `	pClass = PH7_VmExtractClass(pVm, "ReflectionReference", sizeof("ReflectionReference")-1, FALSE, 0);` |
|    13 | 2864 | `	pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;` |
|    13 | 2865 | `	if( pObj == 0 ){` |
|   ! 0 | 2866 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 2867 | `	}` |
|     - | 2868 | `	/* php's ids are opaque strings; the slot index is the identity PHL has. */` |
|    13 | 2869 | `	SyBufferFormat(zId, sizeof(zId), "phlref%u", (unsigned)pNode->nValIdx);` |
|    13 | 2870 | `	PH7_NativeSetAttrStr(pVm, pObj, RR_ID, zId, (int)SyStrlen(zId));` |
|    13 | 2871 | `	return ReflectResultObject(pCtx, pObj);` |
|    10 | 2872 | `}` |
|    12 | 2873 | `static int vm_builtin_ReflectionReference_getId(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 2874 | `{` |
|    13 | 2875 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    13 | 2876 | `	const char *zId = "";` |
|    13 | 2877 | `	int nId = 0;` |
|     6 | 2878 | `	SXUNUSED(nArg);` |
|     6 | 2879 | `	SXUNUSED(apArg);` |
|    13 | 2880 | `	if( pThis ){` |
|    13 | 2881 | `		PH7_NativeAttrStr(pThis, RR_ID, &zId, &nId);` |
|     6 | 2882 | `	}` |
|    13 | 2883 | `	ph7_result_string(pCtx, zId, nId);` |
|    13 | 2884 | `	return PH7_OK;` |
|     1 | 2885 | `}` |
|     - | 2886 | `/* php declares this private and never calls it; reaching it is the diagnostic. */` |
|   ! 0 | 2887 | `static int vm_builtin_ReflectionReference_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 2888 | `{` |
|   ! 0 | 2889 | `	SXUNUSED(nArg);` |
|   ! 0 | 2890 | `	SXUNUSED(apArg);` |
|   ! 0 | 2891 | `	SXUNUSED(pCtx);` |
|   ! 0 | 2892 | `	return PH7_OK;` |
|   ! 0 | 2893 | `}` |
|     - | 2894 | `/*` |
|     - | 2895 | ` * Declare the six. Called from PH7_VmInstallReflection() where chunk 5 used to` |
|     - | 2896 | ` * be compiled, so Reflector (chunk 1) already exists.` |
|     - | 2897 | ` */` |
|  5146 | 2898 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionSmall(ph7_vm *pVm)` |
|     5 | 2899 | `{` |
|     - | 2900 | `	static const PH7_NativePropDef aGenProp[] = {` |
|     - | 2901 | `		{ RG_GEN, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2902 | `	};` |
|     - | 2903 | `	static const PH7_NativeMethodDef aGenMethod[] = {` |
|     - | 2904 | `		{ "__construct",           PH7_MOD_PUBLIC, "Generator $generator", "",` |
|     - | 2905 | `		  vm_builtin_ReflectionGenerator_construct },` |
|     - | 2906 | `		{ "getFunction",           PH7_MOD_PUBLIC, "", "ReflectionFunctionAbstract",` |
|     - | 2907 | `		  vm_builtin_ReflectionGenerator_getFunction },` |
|     - | 2908 | `		{ "getThis",               PH7_MOD_PUBLIC, "", "?object", vm_builtin_ReflectionGenerator_getThis },` |
|     - | 2909 | `		{ "getExecutingGenerator", PH7_MOD_PUBLIC, "", "Generator",` |
|     - | 2910 | `		  vm_builtin_ReflectionGenerator_getExecuting },` |
|     - | 2911 | `		{ "isClosed",              PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionGenerator_isClosed },` |
|     - | 2912 | `		{ "getExecutingLine",      PH7_MOD_PUBLIC, "", "int", vm_builtin_ReflectionGenerator_execLine },` |
|     - | 2913 | `		{ "getExecutingFile",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionGenerator_execFile },` |
|     - | 2914 | `		{ "getTrace",              PH7_MOD_PUBLIC, "int $options = 1", "array",` |
|     - | 2915 | `		  vm_builtin_ReflectionGenerator_trace },` |
|     - | 2916 | `	};` |
|     - | 2917 | `	static const PH7_NativePropDef aFiberProp[] = {` |
|     - | 2918 | `		{ RF_FIBER, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 2919 | `	};` |
|     - | 2920 | `	static const PH7_NativeMethodDef aFiberMethod[] = {` |
|     - | 2921 | `		{ "__construct",      PH7_MOD_PUBLIC, "Fiber $fiber", "", vm_builtin_ReflectionFiber_construct },` |
|     - | 2922 | `		{ "getFiber",         PH7_MOD_PUBLIC, "", "Fiber",    vm_builtin_ReflectionFiber_getFiber },` |
|     - | 2923 | `		{ "getCallable",      PH7_MOD_PUBLIC, "", "callable", vm_builtin_ReflectionFiber_getCallable },` |
|     - | 2924 | `		{ "getExecutingLine", PH7_MOD_PUBLIC, "", "?int",     vm_builtin_ReflectionFiber_execLine },` |
|     - | 2925 | `		{ "getExecutingFile", PH7_MOD_PUBLIC, "", "?string",  vm_builtin_ReflectionFiber_execFile },` |
|     - | 2926 | `		{ "getTrace",         PH7_MOD_PUBLIC, "int $options = 1", "array", vm_builtin_ReflectionFiber_trace },` |
|     - | 2927 | `	};` |
|     - | 2928 | `	static const PH7_NativePropDef aNameProp[] = {` |
|     - | 2929 | `		{ "name", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|     - | 2930 | `	};` |
|     - | 2931 | `	static const PH7_NativeMethodDef aConstMethod[] = {` |
|     - | 2932 | `		{ "__construct",       PH7_MOD_PUBLIC, "string $name", "",` |
|     - | 2933 | `		  vm_builtin_ReflectionConstant_construct },` |
|     - | 2934 | `		{ "getName",           PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_getName },` |
|     - | 2935 | `		{ "getNamespaceName",  PH7_MOD_PUBLIC, "", "string",` |
|     - | 2936 | `		  vm_builtin_ReflectionConstant_getNamespaceName },` |
|     - | 2937 | `		{ "getShortName",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_getShortName },` |
|     - | 2938 | `		{ "getValue",          PH7_MOD_PUBLIC, "", "mixed",  vm_builtin_ReflectionConstant_getValue },` |
|     - | 2939 | `		{ "isDeprecated",      PH7_MOD_PUBLIC, "", "bool",   vm_builtin_ReflectionConstant_isDeprecated },` |
|     - | 2940 | `		{ "getFileName",       PH7_MOD_PUBLIC, "", "string\|false",` |
|     - | 2941 | `		  vm_builtin_ReflectionConstant_getFileName },` |
|     - | 2942 | `		{ "getExtension",      PH7_MOD_PUBLIC, "", "?ReflectionExtension",` |
|     - | 2943 | `		  vm_builtin_ReflectionConstant_getExtension },` |
|     - | 2944 | `		{ "getExtensionName",  PH7_MOD_PUBLIC, "", "string\|false",` |
|     - | 2945 | `		  vm_builtin_ReflectionConstant_getExtensionName },` |
|     - | 2946 | `		{ "getAttributes",     PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|     - | 2947 | `		  vm_builtin_ReflectionConstant_getAttributes },` |
|     - | 2948 | `		{ "__toString",        PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_toString },` |
|     - | 2949 | `	};` |
|     - | 2950 | `	static const PH7_NativeMethodDef aExtMethod[] = {` |
|     - | 2951 | `		{ "__construct",     PH7_MOD_PUBLIC, "string $name", "", vm_builtin_ReflectionExtension_construct },` |
|     - | 2952 | `		{ "getName",         PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionExtension_getName },` |
|     - | 2953 | `		{ "getVersion",      PH7_MOD_PUBLIC, "", "@?string", vm_builtin_ReflectionExtension_getVersion },` |
|     - | 2954 | `		{ "getFunctions",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|     - | 2955 | `		{ "getConstants",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|     - | 2956 | `		{ "getINIEntries",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|     - | 2957 | `		{ "getClasses",      PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|     - | 2958 | `		{ "getClassNames",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|     - | 2959 | `		{ "getDependencies", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_emptyArray },` |
|     - | 2960 | `		{ "info",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_ReflectionExtension_info },` |
|     - | 2961 | `		{ "isPersistent",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionExtension_isPersistent },` |
|     - | 2962 | `		{ "isTemporary",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionExtension_isTemporary },` |
|     - | 2963 | `		{ "__toString",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionExtension_toString },` |
|     - | 2964 | `	};` |
|     - | 2965 | `	static const PH7_NativeMethodDef aZendMethod[] = {` |
|     - | 2966 | `		{ "__construct", PH7_MOD_PUBLIC, "string $name", "",` |
|     - | 2967 | `		  vm_builtin_ReflectionZendExtension_construct },` |
|     - | 2968 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionExtension_getName },` |
|     - | 2969 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionZendExtension_toString },` |
|     - | 2970 | `	};` |
|     - | 2971 | `	static const PH7_NativePropDef aRefProp[] = {` |
|     - | 2972 | `		{ RR_ID, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },` |
|     - | 2973 | `	};` |
|     - | 2974 | `	static const PH7_NativeMethodDef aRefMethod[] = {` |
|     - | 2975 | ``		/* php declares the constructor PRIVATE, so `new ReflectionReference` is`` |
|     - | 2976 | `		 * the engine's own "Call to private ... from global scope" Error rather` |
|     - | 2977 | `		 * than a throw the prelude had to write by hand. */` |
|     - | 2978 | `		{ "__construct",      PH7_MOD_PRIVATE, "", "", vm_builtin_ReflectionReference_construct },` |
|     - | 2979 | `		{ "fromArrayElement", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "array $array, string\|int $key",` |
|     - | 2980 | `		  "?ReflectionReference", vm_builtin_ReflectionReference_fromArrayElement },` |
|     - | 2981 | `		{ "getId",            PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionReference_getId },` |
|     - | 2982 | `	};` |
|     - | 2983 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 2984 | `		{ "ReflectionGenerator", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 2985 | `		  aGenMethod, SX_ARRAYSIZE(aGenMethod), 0, 0, aGenProp, SX_ARRAYSIZE(aGenProp), 0, 0, 0 },` |
|     - | 2986 | `		{ "ReflectionFiber", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 2987 | `		  aFiberMethod, SX_ARRAYSIZE(aFiberMethod), 0, 0, aFiberProp, SX_ARRAYSIZE(aFiberProp), 0, 0, 0 },` |
|     - | 2988 | `		{ "ReflectionConstant", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 2989 | `		  aConstMethod, SX_ARRAYSIZE(aConstMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|     - | 2990 | `		{ "ReflectionExtension", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 2991 | `		  aExtMethod, SX_ARRAYSIZE(aExtMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|     - | 2992 | `		{ "ReflectionZendExtension", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 2993 | `		  aZendMethod, SX_ARRAYSIZE(aZendMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },` |
|     - | 2994 | `		{ "ReflectionReference", 0, 0, PH7_CLASS_FINAL\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 2995 | `		  aRefMethod, SX_ARRAYSIZE(aRefMethod), 0, 0, aRefProp, SX_ARRAYSIZE(aRefProp), 0, 0, 0 },` |
|     - | 2996 | `	};` |
|  5151 | 2997 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|     5 | 2998 | `}` |
|     - | 2999 | `/*` |
|     - | 3000 | ` * ---------------------------------------------------------------------------` |
|     - | 3001 | ` * Reflector, Reflection, ReflectionException, ReflectionClass, ReflectionObject.` |
|     - | 3002 | ` *` |
|     - | 3003 | ` * Chunk 1 — the core of the whole API. Its ~350 lines of PHP funnelled every` |
|     - | 3004 | ` * accessor through __phl_rcinfo(), a memoized descriptor ARRAY built by a C` |
|     - | 3005 | ` * thunk: sixty methods that each rebuilt or re-read a marshalled copy of state` |
|     - | 3006 | ` * the engine was already holding. A native method reads ph7_class directly, so` |
|     - | 3007 | ` * the descriptor is gone from this path entirely and the memo it needed with it.` |
|     - | 3008 | ` *` |
|     - | 3009 | ` * What stays behind is the prelude that has not moved yet, and these classes` |
|     - | 3010 | ` * still reach it by name where php's own object graph does: getMethod() builds` |
|     - | 3011 | ` * a ReflectionMethod, getProperty() a ReflectionProperty, getAttributes() calls` |
|     - | 3012 | ` * __reflect_build_attrs, __toString() calls __reflect_export_class. Those become` |
|     - | 3013 | ` * direct C the moment chunks 2, 3, 7 and 9 land.` |
|     - | 3014 | ` * ---------------------------------------------------------------------------` |
|     - | 3015 | ` */` |
|     - | 3016 | `#define RC_OBJ "__obj"` |
|     - | 3017 |  |
|     - | 3018 | ``/* The class a ReflectionClass reflects: its `name` slot, resolved. The`` |
|     - | 3019 | ` * constructor already stored the canonical name, so no autoload can be needed` |
|     - | 3020 | ` * here. */` |
|   724 | 3021 | `static ph7_class * ReflectClassOf(ph7_context *pCtx)` |
|     5 | 3022 | `{` |
|   729 | 3023 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 3024 | `	const char *zName;` |
|     - | 3025 | `	int nName;` |
|   729 | 3026 | `	if( pThis == 0 ){` |
|   ! 0 | 3027 | `		return 0;` |
|     - | 3028 | `	}` |
|   729 | 3029 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|   729 | 3030 | `	if( nName < 1 ){` |
|   ! 0 | 3031 | `		return 0;` |
|     - | 3032 | `	}` |
|     - | 3033 | `	/* PH7_NativeAttrStr borrows bytes that are NOT NUL-terminated: the length` |
|     - | 3034 | `	 * has to travel with them. */` |
|   729 | 3035 | `	return PH7_VmExtractClass(pCtx->pVm, zName, (sxu32)nName, FALSE, 0);` |
|   367 | 3036 | `}` |
|     - | 3037 | `/* The instance a ReflectionObject was built over, or NULL for a plain` |
|     - | 3038 | ` * ReflectionClass — what makes DYNAMIC properties visible. */` |
|   132 | 3039 | `static ph7_class_instance * ReflectClassObj(ph7_context *pCtx)` |
|     3 | 3040 | `{` |
|   135 | 3041 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   135 | 3042 | `	return pThis ? PH7_NativeAttrObj(pThis, RC_OBJ) : 0;` |
|     3 | 3043 | `}` |
|     - | 3044 | `/* $this->name as bytes. */` |
|   464 | 3045 | `static void ReflectClassName(ph7_context *pCtx, const char **pzOut, int *pnOut)` |
|     5 | 3046 | `{` |
|   469 | 3047 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   469 | 3048 | `	*pzOut = "";` |
|   469 | 3049 | `	*pnOut = 0;` |
|   469 | 3050 | `	if( pThis ){` |
|   469 | 3051 | `		PH7_NativeAttrStr(pThis, "name", pzOut, pnOut);` |
|   232 | 3052 | `	}` |
|   469 | 3053 | `}` |
|     - | 3054 | `/* Answer the truth of one of the reflected class's iFlags bits. */` |
|    88 | 3055 | `static int ReflectClassFlag(ph7_context *pCtx, sxi32 iFlag)` |
|     3 | 3056 | `{` |
|    91 | 3057 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|    91 | 3058 | `	return pClass != 0 && (pClass->iFlags & iFlag) != 0;` |
|     3 | 3059 | `}` |
|     - | 3060 | `#define REFLECT_CLASS_FLAG(NAME,FLAG,NEGATE) \` |
|     - | 3061 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|     - | 3062 | `	{ \` |
|     - | 3063 | `		SXUNUSED(nArg); \` |
|     - | 3064 | `		SXUNUSED(apArg); \` |
|     - | 3065 | `		ph7_result_bool(pCtx, NEGATE ? !ReflectClassFlag(pCtx,FLAG) : ReflectClassFlag(pCtx,FLAG)); \` |
|     - | 3066 | `		return PH7_OK; \` |
|     - | 3067 | `	}` |
|    13 | 3068 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isInternal,    PH7_CLASS_INTERNAL,  0)` |
|     5 | 3069 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isUserDefined, PH7_CLASS_INTERNAL,  1)` |
|     9 | 3070 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isInterface,   PH7_CLASS_INTERFACE, 0)` |
|     5 | 3071 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isTrait,       PH7_CLASS_TRAIT,     0)` |
|    13 | 3072 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isAbstract,    PH7_CLASS_ABSTRACT,  0)` |
|    37 | 3073 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isFinal,       PH7_CLASS_FINAL,     0)` |
|   ! 0 | 3074 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isReadOnly,    PH7_CLASS_READONLY,  0)` |
|    11 | 3075 | `REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isEnum,        PH7_CLASS_ENUM,      0)` |
|     - | 3076 |  |
|     - | 3077 | ``/* Build a ReflectionClass over pTarget with its `name` already filled in: the`` |
|     - | 3078 | ` * constructor would only re-resolve a class this code is holding. */` |
|   228 | 3079 | `static int ReflectResultClassOf(ph7_context *pCtx, ph7_class *pTarget)` |
|     4 | 3080 | `{` |
|   232 | 3081 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 3082 | `	ph7_class *pRC;` |
|     - | 3083 | `	ph7_class_instance *pObj;` |
|   232 | 3084 | `	if( pTarget == 0 ){` |
|   ! 0 | 3085 | `		ph7_result_null(pCtx);` |
|   ! 0 | 3086 | `		return PH7_OK;` |
|     - | 3087 | `	}` |
|   232 | 3088 | `	pRC = PH7_VmExtractClass(pVm, "ReflectionClass", sizeof("ReflectionClass")-1, FALSE, 0);` |
|   232 | 3089 | `	if( pRC == 0 \|\| (pObj = PH7_NewClassInstance(pVm, pRC)) == 0 ){` |
|   ! 0 | 3090 | `		ph7_result_null(pCtx);` |
|   ! 0 | 3091 | `		return PH7_OK;` |
|     - | 3092 | `	}` |
|   346 | 3093 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(&pTarget->sName),` |
|   228 | 3094 | `		(int)SyStringLength(&pTarget->sName));` |
|   232 | 3095 | `	PH7_NativeResultObject(pCtx, pObj);` |
|   232 | 3096 | `	return PH7_OK;` |
|   118 | 3097 | `}` |
|     - | 3098 | ``/* The class an argument declared `ReflectionClass\|string` denotes: a reflector's`` |
|     - | 3099 | `` * `name` slot, or the string itself. NULL when it names nothing (after`` |
|     - | 3100 | ` * autoload). */` |
|    20 | 3101 | `static ph7_class * ReflectClassArg(ph7_context *pCtx, ph7_value *pArg)` |
|     1 | 3102 | `{` |
|    21 | 3103 | `	ph7_vm *pVm = pCtx->pVm;` |
|    21 | 3104 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|   ! 0 | 3105 | `		ph7_class_instance *pObj = (ph7_class_instance *)pArg->x.pOther;` |
|   ! 0 | 3106 | `		ph7_class *pRC = PH7_VmExtractClass(pVm, "ReflectionClass", sizeof("ReflectionClass")-1, FALSE, 0);` |
|   ! 0 | 3107 | `		if( pRC && PH7_VmInstanceOf(pObj->pClass, pRC) ){` |
|     - | 3108 | `			const char *zName;` |
|     - | 3109 | `			int nName;` |
|   ! 0 | 3110 | `			PH7_NativeAttrStr(pObj, "name", &zName, &nName);` |
|   ! 0 | 3111 | `			return nName > 0 ? PH7_VmExtractClass(pVm, zName, (sxu32)nName, FALSE, 0) : 0;` |
|     - | 3112 | `		}` |
|   ! 0 | 3113 | `	}` |
|    21 | 3114 | `	return ReflectResolveClass(pVm, pArg);` |
|    11 | 3115 | `}` |
|     - | 3116 | `/* ---- constructors ---- */` |
|     - | 3117 | `/* ReflectionClass::__construct(object\|string $objectOrClass) */` |
|   392 | 3118 | `static int vm_builtin_ReflectionClass_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 | 3119 | `{` |
|   397 | 3120 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 3121 | `	ph7_class *pClass;` |
|   397 | 3122 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 | 3123 | `		return PH7_OK;` |
|     - | 3124 | `	}` |
|   397 | 3125 | `	pClass = ReflectResolveClass(pCtx->pVm, apArg[0]);` |
|   397 | 3126 | `	if( pClass == 0 ){` |
|     - | 3127 | `		/* php reports the NAME it was handed, after the declared object\|string` |
|     - | 3128 | ``		 * has coerced a scalar — `new ReflectionClass(1.5)` says Class "1.5". */`` |
|     - | 3129 | `		const char *zName;` |
|     - | 3130 | `		int nName;` |
|    12 | 3131 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|    17 | 3132 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     5 | 3133 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|     - | 3134 | `	}` |
|   578 | 3135 | `	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", SyStringData(&pClass->sName),` |
|   382 | 3136 | `		(int)SyStringLength(&pClass->sName));` |
|   387 | 3137 | `	return PH7_OK;` |
|   201 | 3138 | `}` |
|     - | 3139 | `/* ReflectionObject::__construct(object $object) — the same, plus the receiver` |
|     - | 3140 | ` * whose DYNAMIC properties the inherited accessors then report. */` |
|     6 | 3141 | `static int vm_builtin_ReflectionObject_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3142 | `{` |
|     7 | 3143 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 3144 | `	sxi32 rc;` |
|     7 | 3145 | `	if( pThis == 0 \|\| nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 | 3146 | `		return PH7_OK;` |
|     - | 3147 | `	}` |
|     7 | 3148 | `	rc = vm_builtin_ReflectionClass_construct(pCtx, nArg, apArg);` |
|     7 | 3149 | `	if( rc != PH7_OK ){` |
|   ! 0 | 3150 | `		return rc;` |
|     - | 3151 | `	}` |
|     7 | 3152 | `	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RC_OBJ, (ph7_class_instance *)apArg[0]->x.pOther);` |
|     7 | 3153 | `	return PH7_OK;` |
|     4 | 3154 | `}` |
|     - | 3155 | `/* ReflectionClass::__clone(): void — php declares it private, and the class is` |
|     - | 3156 | ` * uncloneable besides (PH7_CLASS_NOCLONE answers before any body runs). */` |
|   ! 0 | 3157 | `static int vm_builtin_ReflectionClass_clone(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 3158 | `{` |
|   ! 0 | 3159 | `	SXUNUSED(pCtx);` |
|   ! 0 | 3160 | `	SXUNUSED(nArg);` |
|   ! 0 | 3161 | `	SXUNUSED(apArg);` |
|   ! 0 | 3162 | `	return PH7_OK;` |
|   ! 0 | 3163 | `}` |
|     - | 3164 | `/* ---- name ---- */` |
|   238 | 3165 | `static int vm_builtin_ReflectionClass_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     4 | 3166 | `{` |
|     - | 3167 | `	const char *zName;` |
|     - | 3168 | `	int nName;` |
|   119 | 3169 | `	SXUNUSED(nArg);` |
|   119 | 3170 | `	SXUNUSED(apArg);` |
|   242 | 3171 | `	ReflectClassName(pCtx, &zName, &nName);` |
|   242 | 3172 | `	ph7_result_string(pCtx, zName, nName);` |
|   242 | 3173 | `	return PH7_OK;` |
|     4 | 3174 | `}` |
|     - | 3175 | `/* Offset just past the last namespace separator, or -1 when there is none. */` |
|     6 | 3176 | `static int ReflectNsCut(const char *zName, int nName)` |
|     1 | 3177 | `{` |
|     - | 3178 | `	int i;` |
|    91 | 3179 | `	for( i = nName - 1 ; i >= 0 ; i-- ){` |
|    85 | 3180 | `		if( zName[i] == '\\' ){` |
|   ! 0 | 3181 | `			return i;` |
|     - | 3182 | `		}` |
|    43 | 3183 | `	}` |
|     7 | 3184 | `	return -1;` |
|     4 | 3185 | `}` |
|     2 | 3186 | `static int vm_builtin_ReflectionClass_getShortName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3187 | `{` |
|     - | 3188 | `	const char *zName;` |
|     - | 3189 | `	int nName, iCut;` |
|     1 | 3190 | `	SXUNUSED(nArg);` |
|     1 | 3191 | `	SXUNUSED(apArg);` |
|     3 | 3192 | `	ReflectClassName(pCtx, &zName, &nName);` |
|     3 | 3193 | `	iCut = ReflectNsCut(zName, nName);` |
|     3 | 3194 | `	if( iCut < 0 ){` |
|     3 | 3195 | `		ph7_result_string(pCtx, zName, nName);` |
|     2 | 3196 | `	}else{` |
|   ! 0 | 3197 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|     - | 3198 | `	}` |
|     3 | 3199 | `	return PH7_OK;` |
|     1 | 3200 | `}` |
|     2 | 3201 | `static int vm_builtin_ReflectionClass_getNamespaceName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3202 | `{` |
|     - | 3203 | `	const char *zName;` |
|     - | 3204 | `	int nName, iCut;` |
|     1 | 3205 | `	SXUNUSED(nArg);` |
|     1 | 3206 | `	SXUNUSED(apArg);` |
|     3 | 3207 | `	ReflectClassName(pCtx, &zName, &nName);` |
|     3 | 3208 | `	iCut = ReflectNsCut(zName, nName);` |
|     3 | 3209 | `	ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|     3 | 3210 | `	return PH7_OK;` |
|     1 | 3211 | `}` |
|     2 | 3212 | `static int vm_builtin_ReflectionClass_inNamespace(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3213 | `{` |
|     - | 3214 | `	const char *zName;` |
|     - | 3215 | `	int nName;` |
|     1 | 3216 | `	SXUNUSED(nArg);` |
|     1 | 3217 | `	SXUNUSED(apArg);` |
|     3 | 3218 | `	ReflectClassName(pCtx, &zName, &nName);` |
|     3 | 3219 | `	ph7_result_bool(pCtx, ReflectNsCut(zName, nName) >= 0);` |
|     3 | 3220 | `	return PH7_OK;` |
|     1 | 3221 | `}` |
|     2 | 3222 | `static int vm_builtin_ReflectionClass_isAnonymous(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3223 | `{` |
|     - | 3224 | `	const char *zName;` |
|     - | 3225 | `	int nName;` |
|     - | 3226 | `	static const char zAnon[] = "class@anonymous";` |
|     1 | 3227 | `	SXUNUSED(nArg);` |
|     1 | 3228 | `	SXUNUSED(apArg);` |
|     3 | 3229 | `	ReflectClassName(pCtx, &zName, &nName);` |
|     3 | 3230 | `	ph7_result_bool(pCtx, nName >= (int)sizeof(zAnon)-1` |
|     1 | 3231 | `		&& SyMemcmp(zName, zAnon, sizeof(zAnon)-1) == 0);` |
|     3 | 3232 | `	return PH7_OK;` |
|     1 | 3233 | `}` |
|     - | 3234 | `/* ---- shape ---- */` |
|     8 | 3235 | `static int vm_builtin_ReflectionClass_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3236 | `{` |
|     9 | 3237 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     9 | 3238 | `	sxi64 iMods = 0;` |
|     4 | 3239 | `	SXUNUSED(nArg);` |
|     4 | 3240 | `	SXUNUSED(apArg);` |
|     9 | 3241 | `	if( pClass ){` |
|     9 | 3242 | `		if( pClass->iFlags & PH7_CLASS_ABSTRACT ){ iMods \|= 64; }` |
|     9 | 3243 | `		if( pClass->iFlags & PH7_CLASS_FINAL ){ iMods \|= 32; }` |
|     9 | 3244 | `		if( pClass->iFlags & PH7_CLASS_READONLY ){ iMods \|= 65536; }` |
|     4 | 3245 | `	}` |
|     9 | 3246 | `	ph7_result_int64(pCtx, iMods);` |
|     9 | 3247 | `	return PH7_OK;` |
|     1 | 3248 | `}` |
|    22 | 3249 | `static int vm_builtin_ReflectionClass_getParentClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 3250 | `{` |
|    25 | 3251 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|    11 | 3252 | `	SXUNUSED(nArg);` |
|    11 | 3253 | `	SXUNUSED(apArg);` |
|    25 | 3254 | `	if( pClass == 0 \|\| pClass->pBase == 0 ){` |
|     9 | 3255 | `		ph7_result_bool(pCtx, 0);` |
|     9 | 3256 | `		return PH7_OK;` |
|     - | 3257 | `	}` |
|    19 | 3258 | `	return ReflectResultClassOf(pCtx, pClass->pBase);` |
|    14 | 3259 | `}` |
|     - | 3260 | `/* getInterfaceNames()/getInterfaces()/getTraitNames()/getTraits() — one walk,` |
|     - | 3261 | ` * four shapes. bReflector picks name-list vs {name: ReflectionClass}. */` |
|    44 | 3262 | `static int ReflectClassNameList(ph7_context *pCtx, int bTraits, int bReflector)` |
|     2 | 3263 | `{` |
|    46 | 3264 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|    46 | 3265 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|     - | 3266 | `	SySet aSet;` |
|     - | 3267 | `	ph7_class **apOut;` |
|     - | 3268 | `	sxu32 n, nOut;` |
|    46 | 3269 | `	if( pList == 0 ){` |
|   ! 0 | 3270 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 3271 | `	}` |
|    46 | 3272 | `	if( pClass == 0 ){` |
|   ! 0 | 3273 | `		ph7_result_value(pCtx, pList);` |
|   ! 0 | 3274 | `		return PH7_OK;` |
|     - | 3275 | `	}` |
|    46 | 3276 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|    46 | 3277 | `	if( bTraits ){` |
|     3 | 3278 | `		apOut = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|     3 | 3279 | `		nOut = SySetUsed(&pClass->aTrait);` |
|     2 | 3280 | `	}else{` |
|    44 | 3281 | `		ReflectInterfacesOf(pClass, &aSet);` |
|    44 | 3282 | `		apOut = (ph7_class **)SySetBasePtr(&aSet);` |
|    44 | 3283 | `		nOut = SySetUsed(&aSet);` |
|     - | 3284 | `	}` |
|   110 | 3285 | `	for( n = 0 ; n < nOut ; n++ ){` |
|    66 | 3286 | `		SyString *pName = &apOut[n]->sName;` |
|    66 | 3287 | `		if( bReflector ){` |
|     - | 3288 | `			/* {name: ReflectionClass} — the reflector is built here rather than` |
|     - | 3289 | `			 * through ReflectResultClassOf, which writes the RESULT slot. */` |
|     5 | 3290 | `			ph7_class *pRC = PH7_VmExtractClass(pCtx->pVm, "ReflectionClass",` |
|     - | 3291 | `				sizeof("ReflectionClass")-1, FALSE, 0);` |
|     5 | 3292 | `			ph7_class_instance *pObj = pRC ? PH7_NewClassInstance(pCtx->pVm, pRC) : 0;` |
|     5 | 3293 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|     - | 3294 | `			ph7_value sVal;` |
|     5 | 3295 | `			if( pObj == 0 \|\| pKey == 0 ){ break; }` |
|     7 | 3296 | `			PH7_NativeSetAttrStr(pCtx->pVm, pObj, "name", SyStringData(pName),` |
|     4 | 3297 | `				(int)SyStringLength(pName));` |
|     - | 3298 | `			/* A STACK carrier, never a context scalar: releasing a MEMOBJ_OBJ` |
|     - | 3299 | `			 * context value would unref the instance a second time. */` |
|     5 | 3300 | `			PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     5 | 3301 | `			sVal.x.pOther = pObj;` |
|     5 | 3302 | `			sVal.iFlags = MEMOBJ_OBJ;` |
|     5 | 3303 | `			ph7_value_string(pKey, SyStringData(pName), (int)SyStringLength(pName));` |
|     5 | 3304 | `			ph7_array_add_elem(pList, pKey, &sVal);   /* takes its own reference */` |
|     5 | 3305 | `			PH7_ClassInstanceUnref(pObj);` |
|     3 | 3306 | `		}else{` |
|    62 | 3307 | `			ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    62 | 3308 | `			if( pVal == 0 ){ break; }` |
|    62 | 3309 | `			ph7_value_string(pVal, SyStringData(pName), (int)SyStringLength(pName));` |
|    62 | 3310 | `			ph7_array_add_elem(pList, 0, pVal);` |
|     - | 3311 | `		}` |
|    34 | 3312 | `	}` |
|    46 | 3313 | `	SySetRelease(&aSet);` |
|    46 | 3314 | `	ph7_result_value(pCtx, pList);` |
|    46 | 3315 | `	return PH7_OK;` |
|    24 | 3316 | `}` |
|     - | 3317 | `#define REFLECT_CLASS_LIST(NAME,TRAITS,REFLECTOR) \` |
|     - | 3318 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|     - | 3319 | `	{ \` |
|     - | 3320 | `		SXUNUSED(nArg); \` |
|     - | 3321 | `		SXUNUSED(apArg); \` |
|     - | 3322 | `		return ReflectClassNameList(pCtx,TRAITS,REFLECTOR); \` |
|     - | 3323 | `	}` |
|    42 | 3324 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getInterfaceNames, 0, 0)` |
|     3 | 3325 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getInterfaces,     0, 1)` |
|     3 | 3326 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getTraitNames,     1, 0)` |
|   ! 0 | 3327 | `REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getTraits,         1, 1)` |
|     - | 3328 |  |
|     - | 3329 | ``/* getTraitAliases(): php reports `use T { m as n; }` renames; PHL's compiler`` |
|     - | 3330 | ` * installs the alias as a method and keeps no rename record, so the map is` |
|     - | 3331 | ` * empty (§7.4). */` |
|   ! 0 | 3332 | `static int vm_builtin_ReflectionClass_getTraitAliases(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 3333 | `{` |
|   ! 0 | 3334 | `	SXUNUSED(nArg);` |
|   ! 0 | 3335 | `	SXUNUSED(apArg);` |
|   ! 0 | 3336 | `	ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|   ! 0 | 3337 | `	return PH7_OK;` |
|   ! 0 | 3338 | `}` |
|     4 | 3339 | `static int vm_builtin_ReflectionClass_isIterable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3340 | `{` |
|     5 | 3341 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 3342 | `	SySet aSet;` |
|     - | 3343 | `	ph7_class **apIface;` |
|     - | 3344 | `	sxu32 n;` |
|     5 | 3345 | `	int bIterable = 0;` |
|     2 | 3346 | `	SXUNUSED(nArg);` |
|     2 | 3347 | `	SXUNUSED(apArg);` |
|     4 | 3348 | `	if( pClass == 0` |
|     5 | 3349 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT)) ){` |
|   ! 0 | 3350 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 3351 | `		return PH7_OK;` |
|     - | 3352 | `	}` |
|     5 | 3353 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|     5 | 3354 | `	ReflectInterfacesOf(pClass, &aSet);` |
|     5 | 3355 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|     5 | 3356 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|     5 | 3357 | `		if( pCtx->pVm->pTraversableClass && apIface[n] == pCtx->pVm->pTraversableClass ){` |
|     5 | 3358 | `			bIterable = 1;` |
|     5 | 3359 | `			break;` |
|     - | 3360 | `		}` |
|   ! 0 | 3361 | `	}` |
|     5 | 3362 | `	SySetRelease(&aSet);` |
|     5 | 3363 | `	ph7_result_bool(pCtx, bIterable);` |
|     5 | 3364 | `	return PH7_OK;` |
|     3 | 3365 | `}` |
|    14 | 3366 | `static int vm_builtin_ReflectionClass_implementsInterface(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3367 | `{` |
|    15 | 3368 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 3369 | `	ph7_class *pTarget;` |
|     - | 3370 | `	SySet aSet;` |
|     - | 3371 | `	ph7_class **apIface;` |
|     - | 3372 | `	sxu32 n;` |
|    15 | 3373 | `	int bYes = 0;` |
|    15 | 3374 | `	if( nArg < 1 ){` |
|   ! 0 | 3375 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 3376 | `		return PH7_OK;` |
|     - | 3377 | `	}` |
|    15 | 3378 | `	pTarget = ReflectClassArg(pCtx, apArg[0]);` |
|    15 | 3379 | `	if( pTarget == 0 ){` |
|     - | 3380 | `		const char *zName;` |
|     - | 3381 | `		int nName;` |
|     3 | 3382 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|     4 | 3383 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     1 | 3384 | `			"Interface \"%.*s\" does not exist", nName, zName);` |
|     - | 3385 | `	}` |
|    13 | 3386 | `	if( (pTarget->iFlags & PH7_CLASS_INTERFACE) == 0 ){` |
|     4 | 3387 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     1 | 3388 | `			"%z is not an interface", &pTarget->sName);` |
|     - | 3389 | `	}` |
|    11 | 3390 | `	if( pClass == pTarget ){` |
|     3 | 3391 | `		ph7_result_bool(pCtx, 1);` |
|     3 | 3392 | `		return PH7_OK;` |
|     - | 3393 | `	}` |
|     9 | 3394 | `	if( pClass == 0 ){` |
|   ! 0 | 3395 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 3396 | `		return PH7_OK;` |
|     - | 3397 | `	}` |
|     9 | 3398 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|     9 | 3399 | `	ReflectInterfacesOf(pClass, &aSet);` |
|     9 | 3400 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|    11 | 3401 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|    11 | 3402 | `		if( apIface[n] == pTarget ){` |
|     9 | 3403 | `			bYes = 1;` |
|     9 | 3404 | `			break;` |
|     - | 3405 | `		}` |
|     2 | 3406 | `	}` |
|     9 | 3407 | `	SySetRelease(&aSet);` |
|     9 | 3408 | `	ph7_result_bool(pCtx, bYes);` |
|     9 | 3409 | `	return PH7_OK;` |
|     8 | 3410 | `}` |
|     6 | 3411 | `static int vm_builtin_ReflectionClass_isSubclassOf(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3412 | `{` |
|     7 | 3413 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 3414 | `	ph7_class *pTarget, *pWalk;` |
|     - | 3415 | `	SySet aSet;` |
|     - | 3416 | `	ph7_class **apIface;` |
|     - | 3417 | `	sxu32 n;` |
|     7 | 3418 | `	int iDepth = 0, bYes = 0;` |
|     7 | 3419 | `	if( nArg < 1 ){` |
|   ! 0 | 3420 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 3421 | `		return PH7_OK;` |
|     - | 3422 | `	}` |
|     7 | 3423 | `	pTarget = ReflectClassArg(pCtx, apArg[0]);` |
|     7 | 3424 | `	if( pTarget == 0 ){` |
|     - | 3425 | `		const char *zName;` |
|     - | 3426 | `		int nName;` |
|   ! 0 | 3427 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|   ! 0 | 3428 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|   ! 0 | 3429 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|     - | 3430 | `	}` |
|     - | 3431 | `	/* php: a class is never a subclass of ITSELF */` |
|     7 | 3432 | `	if( pClass == 0 \|\| pClass == pTarget ){` |
|     3 | 3433 | `		ph7_result_bool(pCtx, 0);` |
|     3 | 3434 | `		return PH7_OK;` |
|     - | 3435 | `	}` |
|     7 | 3436 | `	for( pWalk = pClass->pBase ; pWalk && iDepth <= REFLECT_WALK_MAX_DEPTH ; pWalk = pWalk->pBase ){` |
|     5 | 3437 | `		if( pWalk == pTarget ){` |
|     3 | 3438 | `			ph7_result_bool(pCtx, 1);` |
|     3 | 3439 | `			return PH7_OK;` |
|     - | 3440 | `		}` |
|     3 | 3441 | `		iDepth++;` |
|     2 | 3442 | `	}` |
|     3 | 3443 | `	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|     3 | 3444 | `	ReflectInterfacesOf(pClass, &aSet);` |
|     3 | 3445 | `	apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|     3 | 3446 | `	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|     3 | 3447 | `		if( apIface[n] == pTarget ){` |
|     3 | 3448 | `			bYes = 1;` |
|     3 | 3449 | `			break;` |
|     - | 3450 | `		}` |
|   ! 0 | 3451 | `	}` |
|     3 | 3452 | `	SySetRelease(&aSet);` |
|     3 | 3453 | `	ph7_result_bool(pCtx, bYes);` |
|     3 | 3454 | `	return PH7_OK;` |
|     4 | 3455 | `}` |
|     8 | 3456 | `static int vm_builtin_ReflectionClass_isInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3457 | `{` |
|     9 | 3458 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 3459 | `	ph7_class_instance *pObj;` |
|     9 | 3460 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 \|\| pClass == 0 ){` |
|   ! 0 | 3461 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 3462 | `		return PH7_OK;` |
|     - | 3463 | `	}` |
|     9 | 3464 | `	pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     9 | 3465 | `	ph7_result_bool(pCtx, PH7_VmInstanceOf(pObj->pClass, pClass) != 0);` |
|     9 | 3466 | `	return PH7_OK;` |
|     5 | 3467 | `}` |
|     - | 3468 | `/* ---- source position ---- */` |
|     8 | 3469 | `static int vm_builtin_ReflectionClass_getStartLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3470 | `{` |
|     9 | 3471 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     4 | 3472 | `	SXUNUSED(nArg);` |
|     4 | 3473 | `	SXUNUSED(apArg);` |
|     9 | 3474 | `	if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_INTERNAL) ){` |
|     3 | 3475 | `		ph7_result_bool(pCtx, 0);` |
|     2 | 3476 | `	}else{` |
|     7 | 3477 | `		ph7_result_int64(pCtx, (sxi64)pClass->nLine);` |
|     - | 3478 | `	}` |
|     9 | 3479 | `	return PH7_OK;` |
|     1 | 3480 | `}` |
|     6 | 3481 | `static int vm_builtin_ReflectionClass_getEndLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3482 | `{` |
|     7 | 3483 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     3 | 3484 | `	SXUNUSED(nArg);` |
|     3 | 3485 | `	SXUNUSED(apArg);` |
|     7 | 3486 | `	if( pClass == 0 \|\| (pClass->iFlags & PH7_CLASS_INTERNAL) ){` |
|     3 | 3487 | `		ph7_result_bool(pCtx, 0);` |
|     2 | 3488 | `	}else{` |
|     5 | 3489 | `		ph7_result_int64(pCtx, (sxi64)pClass->nEndLine);` |
|     - | 3490 | `	}` |
|     7 | 3491 | `	return PH7_OK;` |
|     1 | 3492 | `}` |
|    12 | 3493 | `static int vm_builtin_ReflectionClass_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 3494 | `{` |
|    14 | 3495 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     6 | 3496 | `	SXUNUSED(nArg);` |
|     6 | 3497 | `	SXUNUSED(apArg);` |
|    14 | 3498 | `	if( pClass && SyStringLength(&pClass->sFile) > 0 ){` |
|     8 | 3499 | `		ph7_result_string(pCtx, SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));` |
|     5 | 3500 | `	}else{` |
|     7 | 3501 | `		ph7_result_bool(pCtx, 0);` |
|     - | 3502 | `	}` |
|    14 | 3503 | `	return PH7_OK;` |
|     2 | 3504 | `}` |
|     8 | 3505 | `static int vm_builtin_ReflectionClass_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3506 | `{` |
|     9 | 3507 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     4 | 3508 | `	SXUNUSED(nArg);` |
|     4 | 3509 | `	SXUNUSED(apArg);` |
|     9 | 3510 | `	if( pClass && SyStringLength(&pClass->sDoc) > 0 ){` |
|     3 | 3511 | `		ph7_result_string(pCtx, SyStringData(&pClass->sDoc), (int)SyStringLength(&pClass->sDoc));` |
|     2 | 3512 | `	}else{` |
|     7 | 3513 | `		ph7_result_bool(pCtx, 0);` |
|     - | 3514 | `	}` |
|     9 | 3515 | `	return PH7_OK;` |
|     1 | 3516 | `}` |
|     - | 3517 | `/* ---- instantiation ---- */` |
|    12 | 3518 | `static int vm_builtin_ReflectionClass_isInstantiable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3519 | `{` |
|    13 | 3520 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 3521 | `	sxi32 iCtor, iClone;` |
|     6 | 3522 | `	SXUNUSED(nArg);` |
|     6 | 3523 | `	SXUNUSED(apArg);` |
|    12 | 3524 | `	if( pClass == 0` |
|    13 | 3525 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT\|PH7_CLASS_ENUM)) ){` |
|     7 | 3526 | `		ph7_result_bool(pCtx, 0);` |
|     7 | 3527 | `		return PH7_OK;` |
|     - | 3528 | `	}` |
|     7 | 3529 | `	ReflectCtorCloneVis(pCtx->pVm, pClass, &iCtor, &iClone);` |
|     7 | 3530 | `	ph7_result_bool(pCtx, iCtor == 0 \|\| iCtor == PH7_CLASS_PROT_PUBLIC);` |
|     7 | 3531 | `	return PH7_OK;` |
|     7 | 3532 | `}` |
|     4 | 3533 | `static int vm_builtin_ReflectionClass_isCloneable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3534 | `{` |
|     5 | 3535 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 3536 | `	sxi32 iCtor, iClone;` |
|     2 | 3537 | `	SXUNUSED(nArg);` |
|     2 | 3538 | `	SXUNUSED(apArg);` |
|     4 | 3539 | `	if( pClass == 0` |
|     5 | 3540 | `	 \|\| (pClass->iFlags & (PH7_CLASS_INTERFACE\|PH7_CLASS_TRAIT\|PH7_CLASS_ABSTRACT)) ){` |
|     3 | 3541 | `		ph7_result_bool(pCtx, 0);` |
|     3 | 3542 | `		return PH7_OK;` |
|     - | 3543 | `	}` |
|     3 | 3544 | `	ReflectCtorCloneVis(pCtx->pVm, pClass, &iCtor, &iClone);` |
|     3 | 3545 | `	ph7_result_bool(pCtx, iClone == 0 \|\| iClone == PH7_CLASS_PROT_PUBLIC);` |
|     3 | 3546 | `	return PH7_OK;` |
|     3 | 3547 | `}` |
|     - | 3548 | `/* php's own gate, raised before any object exists. */` |
|    38 | 3549 | `static sxi32 ReflectCheckInstantiable(ph7_context *pCtx, ph7_class *pClass)` |
|     4 | 3550 | `{` |
|    42 | 3551 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|   ! 0 | 3552 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate interface %z", &pClass->sName);` |
|     - | 3553 | `	}` |
|    42 | 3554 | `	if( pClass->iFlags & PH7_CLASS_TRAIT ){` |
|   ! 0 | 3555 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate trait %z", &pClass->sName);` |
|     - | 3556 | `	}` |
|    42 | 3557 | `	if( pClass->iFlags & PH7_CLASS_ABSTRACT ){` |
|     5 | 3558 | `		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate abstract class %z", &pClass->sName);` |
|     - | 3559 | `	}` |
|    38 | 3560 | `	if( pClass->iFlags & PH7_CLASS_NOINSTANTIATE ){` |
|     - | 3561 | ``		/* The `new` path's create_object refusal, which php raises here too —`` |
|     - | 3562 | `		 * ReflectionClass::newInstance() on a Closure is the same Error, not a` |
|     - | 3563 | `		 * visibility one about its private constructor. */` |
|     6 | 3564 | `		if( pClass->zNewRefusal ){` |
|     3 | 3565 | `			return PH7_VmThrowException(pCtx, "Error", "%s", pClass->zNewRefusal);` |
|     - | 3566 | `		}` |
|     4 | 3567 | `		return PH7_VmThrowException(pCtx, "Error",` |
|     1 | 3568 | `			"Instantiation of class %z is not allowed", &pClass->sName);` |
|     - | 3569 | `	}` |
|    33 | 3570 | `	return PH7_OK;` |
|    23 | 3571 | `}` |
|     - | 3572 | `/*` |
|     - | 3573 | ` * newInstance()/newInstanceArgs(): the checks php runs, then the constructor.` |
|     - | 3574 | ` * apCtor/nCtor are already-collected positional arguments; pNames is the` |
|     - | 3575 | ` * name map when the caller handed an array with string keys (php 8.1 accepts` |
|     - | 3576 | ` * those as NAMED constructor arguments).` |
|     - | 3577 | ` */` |
|    22 | 3578 | `static int ReflectNewInstance(ph7_context *pCtx, int nCtor, ph7_value **apCtor,` |
|     - | 3579 | `	SyString *pNames)` |
|     4 | 3580 | `{` |
|    26 | 3581 | `	ph7_vm *pVm = pCtx->pVm;` |
|    26 | 3582 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 3583 | `	ph7_class_instance *pObj;` |
|     - | 3584 | `	ph7_class_method *pCons;` |
|     - | 3585 | `	sxi32 iCtorVis, iCloneVis, rc;` |
|    26 | 3586 | `	if( pClass == 0 ){` |
|   ! 0 | 3587 | `		ph7_result_null(pCtx);` |
|   ! 0 | 3588 | `		return PH7_OK;` |
|     - | 3589 | `	}` |
|    26 | 3590 | `	rc = ReflectCheckInstantiable(pCtx, pClass);` |
|    26 | 3591 | `	if( rc != PH7_OK ){` |
|     8 | 3592 | `		return rc;` |
|     - | 3593 | `	}` |
|    19 | 3594 | `	ReflectCtorCloneVis(pVm, pClass, &iCtorVis, &iCloneVis);` |
|    19 | 3595 | `	if( iCtorVis != 0 && iCtorVis != PH7_CLASS_PROT_PUBLIC ){` |
|     4 | 3596 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     1 | 3597 | `			"Access to non-public constructor of class %z", &pClass->sName);` |
|     - | 3598 | `	}` |
|    17 | 3599 | `	if( iCtorVis == 0 && nCtor > 0 ){` |
|     4 | 3600 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     - | 3601 | `			"Class %z does not have a constructor, so you cannot pass any constructor arguments",` |
|     1 | 3602 | `			&pClass->sName);` |
|     - | 3603 | `	}` |
|    15 | 3604 | `	if( VmClassStaticDeferPending(pClass) ){` |
|     - | 3605 | `		/* Instantiation materializes the static table (OP_NEW does it too), so a` |
|     - | 3606 | `		 * broken default raises BEFORE any object exists. */` |
|     3 | 3607 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pVm, pClass);` |
|     3 | 3608 | `		if( rcMat != SXRET_OK ){` |
|     3 | 3609 | `			return rcMat;` |
|     - | 3610 | `		}` |
|   ! 0 | 3611 | `	}` |
|    12 | 3612 | `	pObj = PH7_NewClassInstance(pVm, pClass);` |
|    12 | 3613 | `	if( pObj == 0 ){` |
|   ! 0 | 3614 | `		ph7_result_null(pCtx);` |
|   ! 0 | 3615 | `		return PH7_OK;` |
|     - | 3616 | `	}` |
|    12 | 3617 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|    12 | 3618 | `	if( pCons ){` |
|    10 | 3619 | `		if( pNames ){` |
|     - | 3620 | `			VmCallArgMap sMap;` |
|   ! 0 | 3621 | `			SyZero(&sMap, sizeof(sMap));` |
|   ! 0 | 3622 | `			sMap.bHasNamed = 1;` |
|   ! 0 | 3623 | `			sMap.nTotal = (sxu32)nCtor;` |
|   ! 0 | 3624 | `			sMap.aNames = pNames;` |
|   ! 0 | 3625 | `			rc = PH7_VmCallClassMethodMap(pVm, pObj, pCons, 0, nCtor, apCtor, &sMap);` |
|   ! 0 | 3626 | `		}else{` |
|    10 | 3627 | `			rc = PH7_VmCallClassMethod(pVm, pObj, pCons, 0, nCtor, apCtor);` |
|     - | 3628 | `		}` |
|    10 | 3629 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|     3 | 3630 | `			PH7_ClassInstanceUnref(pObj);` |
|     3 | 3631 | `			return rc;` |
|     - | 3632 | `		}` |
|     3 | 3633 | `	}` |
|     9 | 3634 | `	return ReflectResultObject(pCtx, pObj);` |
|    15 | 3635 | `}` |
|    20 | 3636 | `static int vm_builtin_ReflectionClass_newInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     4 | 3637 | `{` |
|    24 | 3638 | `	return ReflectNewInstance(pCtx, nArg, apArg, 0);` |
|     4 | 3639 | `}` |
|     2 | 3640 | `static int vm_builtin_ReflectionClass_newInstanceArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3641 | `{` |
|     - | 3642 | `	SySet aArg;` |
|     3 | 3643 | `	SyString *aNames = 0;` |
|     - | 3644 | `	int rc;` |
|     3 | 3645 | `	SySetInit(&aArg, &pCtx->pVm->sAllocator, sizeof(ph7_value *));` |
|     3 | 3646 | `	if( nArg > 0 ){` |
|     3 | 3647 | `		ReflectCollectArgs(pCtx, apArg[0], &aArg, &aNames);` |
|     1 | 3648 | `	}` |
|     4 | 3649 | `	rc = ReflectNewInstance(pCtx, (int)SySetUsed(&aArg),` |
|     2 | 3650 | `		(ph7_value **)SySetBasePtr(&aArg), aNames);` |
|     3 | 3651 | `	if( aNames ){` |
|   ! 0 | 3652 | `		SyMemBackendFree(&pCtx->pVm->sAllocator, aNames);` |
|   ! 0 | 3653 | `	}` |
|     3 | 3654 | `	SySetRelease(&aArg);` |
|     3 | 3655 | `	return rc;` |
|     1 | 3656 | `}` |
|    16 | 3657 | `static int vm_builtin_ReflectionClass_newInstanceWithoutConstructor(ph7_context *pCtx,` |
|     - | 3658 | `	int nArg, ph7_value **apArg)` |
|     2 | 3659 | `{` |
|    18 | 3660 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 3661 | `	sxi32 rc;` |
|     8 | 3662 | `	SXUNUSED(nArg);` |
|     8 | 3663 | `	SXUNUSED(apArg);` |
|    18 | 3664 | `	if( pClass == 0 ){` |
|   ! 0 | 3665 | `		ph7_result_null(pCtx);` |
|   ! 0 | 3666 | `		return PH7_OK;` |
|     - | 3667 | `	}` |
|    18 | 3668 | `	rc = ReflectCheckInstantiable(pCtx, pClass);` |
|    18 | 3669 | `	if( rc != PH7_OK ){` |
|     3 | 3670 | `		return rc;` |
|     - | 3671 | `	}` |
|    16 | 3672 | `	if( VmClassStaticDeferPending(pClass) ){` |
|     3 | 3673 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);` |
|     3 | 3674 | `		if( rcMat != SXRET_OK ){` |
|     3 | 3675 | `			return rcMat;` |
|     - | 3676 | `		}` |
|   ! 0 | 3677 | `	}` |
|    13 | 3678 | `	return ReflectResultObject(pCtx, PH7_NewClassInstance(pCtx->pVm, pClass));` |
|    10 | 3679 | `}` |
|     - | 3680 | `/* ---- members ---- */` |
|     - | 3681 | `/* The modifier mask php filters a member on. */` |
|   128 | 3682 | `static sxi64 ReflectVisMask(sxi32 iProt)` |
|     2 | 3683 | `{` |
|   130 | 3684 | `	if( iProt == PH7_CLASS_PROT_PUBLIC ){` |
|    78 | 3685 | `		return 1;` |
|     - | 3686 | `	}` |
|    53 | 3687 | `	return iProt == PH7_CLASS_PROT_PROTECTED ? 2 : 4;` |
|    66 | 3688 | `}` |
|    86 | 3689 | `static sxi64 ReflectPropModifiers(ph7_class_attr *pAttr)` |
|     2 | 3690 | `{` |
|    88 | 3691 | `	sxi64 iMods = ReflectVisMask(pAttr->iProtection);` |
|    88 | 3692 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){ iMods \|= 16; }` |
|    88 | 3693 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){ iMods \|= 512; }` |
|    88 | 3694 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|     - | 3695 | `		/* php models readonly as protected(set) and reports the bit. */` |
|    14 | 3696 | `		iMods \|= 128\|2048;` |
|     6 | 3697 | `	}` |
|    88 | 3698 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET ){ iMods \|= 2048; }` |
|    88 | 3699 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|     - | 3700 | `		/* private(set) cannot be widened by a subclass, so php reports it FINAL. */` |
|     5 | 3701 | `		iMods \|= 4096\|32;` |
|     2 | 3702 | `	}` |
|    88 | 3703 | `	return iMods;` |
|     2 | 3704 | `}` |
|    16 | 3705 | `static sxi64 ReflectMethodModifiers(ph7_class_method *pMeth)` |
|     1 | 3706 | `{` |
|    17 | 3707 | `	sxi64 iMods = ReflectVisMask(pMeth->iProtection);` |
|    17 | 3708 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){ iMods \|= 16; }` |
|    17 | 3709 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){ iMods \|= 64; }` |
|    17 | 3710 | `	if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods \|= 32; }` |
|    17 | 3711 | `	return iMods;` |
|     1 | 3712 | `}` |
|    26 | 3713 | `static sxi64 ReflectConstModifiers(ph7_class_attr *pAttr)` |
|     1 | 3714 | `{` |
|    27 | 3715 | `	sxi64 iMods = ReflectVisMask(pAttr->iProtection);` |
|    27 | 3716 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods \|= 32; }` |
|    27 | 3717 | `	return iMods;` |
|     1 | 3718 | `}` |
|     - | 3719 | `/* Does the reflected object own a property under this name? (1 = exists) */` |
|    16 | 3720 | `static int ReflectObjHasProp(ph7_class_instance *pObj, const char *zName, int nName)` |
|     1 | 3721 | `{` |
|    13 | 3722 | `	return pObj != 0 && nName > 0` |
|    18 | 3723 | `		&& SyHashGet(&pObj->hAttr, (const void *)zName, (sxu32)nName) != 0;` |
|     1 | 3724 | `}` |
|    12 | 3725 | `static int vm_builtin_ReflectionClass_hasMethod(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3726 | `{` |
|    13 | 3727 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 3728 | `	const char *zName;` |
|     - | 3729 | `	int nName;` |
|    13 | 3730 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|   ! 0 | 3731 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 3732 | `		return PH7_OK;` |
|     - | 3733 | `	}` |
|    13 | 3734 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|    13 | 3735 | `	ph7_result_bool(pCtx, ReflectFindMethodEntry(pClass, zName, nName) != 0);` |
|    13 | 3736 | `	return PH7_OK;` |
|     7 | 3737 | `}` |
|    16 | 3738 | `static int vm_builtin_ReflectionClass_hasProperty(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 3739 | `{` |
|    18 | 3740 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 3741 | `	const char *zName;` |
|    18 | 3742 | `	int nName, bFound = 0;` |
|     - | 3743 | `	SySet aMembers;` |
|     - | 3744 | `	sxu32 n;` |
|    18 | 3745 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|   ! 0 | 3746 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 3747 | `		return PH7_OK;` |
|     - | 3748 | `	}` |
|    18 | 3749 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|    18 | 3750 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|    18 | 3751 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|    48 | 3752 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    38 | 3753 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|    38 | 3754 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zName, nName) ){` |
|     8 | 3755 | `			bFound = 1;` |
|     8 | 3756 | `			break;` |
|     - | 3757 | `		}` |
|    16 | 3758 | `	}` |
|    18 | 3759 | `	SySetRelease(&aMembers);` |
|     - | 3760 | `	/* A ReflectionObject also sees the instance's own dynamic properties */` |
|    18 | 3761 | `	if( !bFound ){` |
|    11 | 3762 | `		bFound = ReflectObjHasProp(ReflectClassObj(pCtx), zName, nName);` |
|     5 | 3763 | `	}` |
|    18 | 3764 | `	ph7_result_bool(pCtx, bFound);` |
|    18 | 3765 | `	return PH7_OK;` |
|    10 | 3766 | `}` |
|     8 | 3767 | `static int vm_builtin_ReflectionClass_hasConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3768 | `{` |
|     9 | 3769 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 3770 | `	const char *zName;` |
|     9 | 3771 | `	int nName, bFound = 0;` |
|     - | 3772 | `	SySet aMembers;` |
|     - | 3773 | `	sxu32 n;` |
|     9 | 3774 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|   ! 0 | 3775 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 3776 | `		return PH7_OK;` |
|     - | 3777 | `	}` |
|     9 | 3778 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     9 | 3779 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     9 | 3780 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|    35 | 3781 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    31 | 3782 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|    31 | 3783 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|     5 | 3784 | `			bFound = 1;` |
|     5 | 3785 | `			break;` |
|     - | 3786 | `		}` |
|    14 | 3787 | `	}` |
|     9 | 3788 | `	SySetRelease(&aMembers);` |
|     9 | 3789 | `	ph7_result_bool(pCtx, bFound);` |
|     9 | 3790 | `	return PH7_OK;` |
|     5 | 3791 | `}` |
|     - | 3792 | `/*` |
|     - | 3793 | ` * The value of a class constant, materializing its lazily-evaluated slot.` |
|     - | 3794 | ` *` |
|     - | 3795 | ` * php re-evaluates a failed initializer on EVERY read, so this can raise; the` |
|     - | 3796 | ` * status is returned rather than swallowed, because a native body that answers` |
|     - | 3797 | ` * PH7_OK with a throw in flight lets the caller carry on and print the NULL it` |
|     - | 3798 | ` * never should have seen.` |
|     - | 3799 | ` */` |
|   124 | 3800 | `static sxi32 ReflectConstSlot(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr,` |
|     - | 3801 | `	ph7_value **ppOut)` |
|     3 | 3802 | `{` |
|   127 | 3803 | `	sxi32 rc = PH7_VmMaterializeClassConst(pCtx->pVm, pClass, pAttr);` |
|   127 | 3804 | `	*ppOut = 0;` |
|   127 | 3805 | `	if( rc != SXRET_OK ){` |
|     3 | 3806 | `		return rc;` |
|     - | 3807 | `	}` |
|   124 | 3808 | `	*ppOut = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|   124 | 3809 | `	return SXRET_OK;` |
|    65 | 3810 | `}` |
|     6 | 3811 | `static int vm_builtin_ReflectionClass_getConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 3812 | `{` |
|     8 | 3813 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 3814 | `	const char *zName;` |
|     - | 3815 | `	int nName;` |
|     - | 3816 | `	SySet aMembers;` |
|     - | 3817 | `	sxu32 n;` |
|     8 | 3818 | `	ph7_class_attr *pFound = 0;` |
|     8 | 3819 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|   ! 0 | 3820 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 3821 | `		return PH7_OK;` |
|     - | 3822 | `	}` |
|     8 | 3823 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     8 | 3824 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     8 | 3825 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|    16 | 3826 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    16 | 3827 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|    16 | 3828 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|     8 | 3829 | `			pFound = pM->pAttr;` |
|     8 | 3830 | `			break;` |
|     - | 3831 | `		}` |
|     5 | 3832 | `	}` |
|     8 | 3833 | `	SySetRelease(&aMembers);` |
|     8 | 3834 | `	if( pFound == 0 ){` |
|   ! 0 | 3835 | `		PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED,` |
|     - | 3836 | `			"ReflectionClass::getConstant() for a non-existent constant is deprecated, "` |
|     - | 3837 | `			"use ReflectionClass::hasConstant() to check if the constant exists");` |
|   ! 0 | 3838 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 3839 | `		return PH7_OK;` |
|     - | 3840 | `	}` |
|     - | 3841 | `	{` |
|     - | 3842 | `		ph7_value *pVal;` |
|     8 | 3843 | `		sxi32 rc = ReflectConstSlot(pCtx, pClass, pFound, &pVal);` |
|     8 | 3844 | `		if( rc != SXRET_OK ){` |
|     3 | 3845 | `			return rc;` |
|     - | 3846 | `		}` |
|     5 | 3847 | `		if( pVal ){` |
|     5 | 3848 | `			ph7_result_value(pCtx, pVal);` |
|     3 | 3849 | `		}else{` |
|   ! 0 | 3850 | `			ph7_result_null(pCtx);` |
|     - | 3851 | `		}` |
|     - | 3852 | `	}` |
|     5 | 3853 | `	return PH7_OK;` |
|     5 | 3854 | `}` |
|    16 | 3855 | `static int vm_builtin_ReflectionClass_getConstants(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 3856 | `{` |
|    18 | 3857 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|    18 | 3858 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     - | 3859 | `	SySet aMembers;` |
|     - | 3860 | `	sxu32 n;` |
|    18 | 3861 | `	int bFilter = (nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0);` |
|    18 | 3862 | `	sxi64 iFilter = bFilter ? ph7_value_to_int64(apArg[0]) : 0;` |
|    18 | 3863 | `	if( pOut == 0 ){` |
|   ! 0 | 3864 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 3865 | `	}` |
|    18 | 3866 | `	if( pClass == 0 ){` |
|   ! 0 | 3867 | `		ph7_result_value(pCtx, pOut);` |
|   ! 0 | 3868 | `		return PH7_OK;` |
|     - | 3869 | `	}` |
|    18 | 3870 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|    18 | 3871 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|   122 | 3872 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   106 | 3873 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     - | 3874 | `		ph7_value *pVal;` |
|   106 | 3875 | `		if( pM->iKind != REFLECT_MEMBER_CONST ){` |
|    50 | 3876 | `			continue;` |
|     - | 3877 | `		}` |
|    60 | 3878 | `		if( bFilter && (ReflectConstModifiers(pM->pAttr) & iFilter) == 0 ){` |
|     5 | 3879 | `			continue;` |
|     - | 3880 | `		}` |
|     - | 3881 | `		{` |
|    56 | 3882 | `			sxi32 rc = ReflectConstSlot(pCtx, pClass, pM->pAttr, &pVal);` |
|    56 | 3883 | `			if( rc != SXRET_OK ){` |
|   ! 0 | 3884 | `				SySetRelease(&aMembers);` |
|   ! 0 | 3885 | `				return rc;` |
|     - | 3886 | `			}` |
|     - | 3887 | `		}` |
|    56 | 3888 | `		if( pVal ){` |
|    56 | 3889 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, pVal);` |
|    27 | 3890 | `		}` |
|    29 | 3891 | `	}` |
|    18 | 3892 | `	SySetRelease(&aMembers);` |
|    18 | 3893 | `	ph7_result_value(pCtx, pOut);` |
|    18 | 3894 | `	return PH7_OK;` |
|    10 | 3895 | `}` |
|     - | 3896 | `/*` |
|     - | 3897 | ` * getMethod()/getProperty()/getReflectionConstant() build a class that is still` |
|     - | 3898 | ` * PRELUDE PHP, so they go through its constructor (ReflectConstruct). They` |
|     - | 3899 | ` * become direct C when chunks 2 and 3 land.` |
|     - | 3900 | ` */` |
|    52 | 3901 | `static int ReflectResultMember(ph7_context *pCtx, const char *zClass,` |
|     - | 3902 | `	ph7_value *pTarget, const SyString *pName)` |
|     4 | 3903 | `{` |
|     - | 3904 | `	ph7_value sName;` |
|     - | 3905 | `	ph7_value *apCtor[2];` |
|     - | 3906 | `	ph7_class_instance *pOut;` |
|     - | 3907 | `	sxi32 rc;` |
|    56 | 3908 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|    56 | 3909 | `	ph7_value_string(&sName, SyStringData(pName), (int)SyStringLength(pName));` |
|    56 | 3910 | `	apCtor[0] = pTarget;` |
|    56 | 3911 | `	apCtor[1] = &sName;` |
|    56 | 3912 | `	pOut = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|    56 | 3913 | `	PH7_MemObjRelease(&sName);` |
|    56 | 3914 | `	if( pOut == 0 ){` |
|   ! 0 | 3915 | `		if( rc != PH7_OK ){` |
|   ! 0 | 3916 | `			return rc;` |
|     - | 3917 | `		}` |
|   ! 0 | 3918 | `		ph7_result_null(pCtx);` |
|   ! 0 | 3919 | `		return PH7_OK;` |
|     - | 3920 | `	}` |
|    56 | 3921 | `	return ReflectResultObject(pCtx, pOut);` |
|    30 | 3922 | `}` |
|     - | 3923 | ``/* A `ReflectionMethod($this->name, ...)`-shaped first argument. */`` |
|   164 | 3924 | `static void ReflectSelfName(ph7_context *pCtx, ph7_value *pOut)` |
|     4 | 3925 | `{` |
|     - | 3926 | `	const char *zName;` |
|     - | 3927 | `	int nName;` |
|   168 | 3928 | `	ReflectClassName(pCtx, &zName, &nName);` |
|   168 | 3929 | `	PH7_MemObjInit(pCtx->pVm, pOut);` |
|   168 | 3930 | `	ph7_value_string(pOut, zName, nName);` |
|   168 | 3931 | `}` |
|    34 | 3932 | `static int vm_builtin_ReflectionClass_getMethod(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 3933 | `{` |
|    35 | 3934 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 3935 | `	SyHashEntry *pEntry;` |
|     - | 3936 | `	const char *zName;` |
|     - | 3937 | `	int nName;` |
|     - | 3938 | `	SyString sFound;` |
|    35 | 3939 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|   ! 0 | 3940 | `		ph7_result_null(pCtx);` |
|   ! 0 | 3941 | `		return PH7_OK;` |
|     - | 3942 | `	}` |
|    35 | 3943 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|    35 | 3944 | `	pEntry = ReflectFindMethodEntry(pClass, zName, nName);` |
|    35 | 3945 | `	if( pEntry == 0 ){` |
|     4 | 3946 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     1 | 3947 | `			"Method %z::%.*s() does not exist", &pClass->sName, nName, zName);` |
|     - | 3948 | `	}` |
|     - | 3949 | `	/* The reported name is the DECLARED spelling, whatever case was asked for. */` |
|    33 | 3950 | `	SyStringInitFromBuf(&sFound, (const char *)pEntry->pKey, pEntry->nKeyLen);` |
|     - | 3951 | `	{` |
|     - | 3952 | `		ph7_value sSelf;` |
|     - | 3953 | `		int rc;` |
|    33 | 3954 | `		ReflectSelfName(pCtx, &sSelf);` |
|    33 | 3955 | `		rc = ReflectResultMember(pCtx, "ReflectionMethod", &sSelf, &sFound);` |
|    33 | 3956 | `		PH7_MemObjRelease(&sSelf);` |
|    33 | 3957 | `		return rc;` |
|     - | 3958 | `	}` |
|    18 | 3959 | `}` |
|    20 | 3960 | `static int vm_builtin_ReflectionClass_getConstructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     4 | 3961 | `{` |
|    24 | 3962 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 3963 | `	SyHashEntry *pEntry;` |
|     - | 3964 | `	SyString sFound;` |
|    10 | 3965 | `	SXUNUSED(nArg);` |
|    10 | 3966 | `	SXUNUSED(apArg);` |
|    24 | 3967 | `	if( pClass == 0 ){` |
|   ! 0 | 3968 | `		ph7_result_null(pCtx);` |
|   ! 0 | 3969 | `		return PH7_OK;` |
|     - | 3970 | `	}` |
|     - | 3971 | `	/* No PHP-4 class-name constructor: a method named like the class is a plain` |
|     - | 3972 | `	 * method (removed in 8.0), so this stays null without an explicit one. */` |
|    24 | 3973 | `	pEntry = ReflectFindMethodEntry(pClass, "__construct", sizeof("__construct")-1);` |
|    24 | 3974 | `	if( pEntry == 0 ){` |
|     9 | 3975 | `		ph7_result_null(pCtx);` |
|     9 | 3976 | `		return PH7_OK;` |
|     - | 3977 | `	}` |
|    18 | 3978 | `	SyStringInitFromBuf(&sFound, (const char *)pEntry->pKey, pEntry->nKeyLen);` |
|     - | 3979 | `	{` |
|     - | 3980 | `		ph7_value sSelf;` |
|     - | 3981 | `		int rc;` |
|    18 | 3982 | `		ReflectSelfName(pCtx, &sSelf);` |
|    18 | 3983 | `		rc = ReflectResultMember(pCtx, "ReflectionMethod", &sSelf, &sFound);` |
|    18 | 3984 | `		PH7_MemObjRelease(&sSelf);` |
|    18 | 3985 | `		return rc;` |
|     - | 3986 | `	}` |
|    14 | 3987 | `}` |
|     - | 3988 | `/*` |
|     - | 3989 | ` * getMethods() / getProperties() / getReflectionConstants(): one member walk,` |
|     - | 3990 | ` * one reflector per surviving member. Each reflector is a prelude class built` |
|     - | 3991 | ` * through its constructor, and is appended through a STACK carrier so the list` |
|     - | 3992 | ` * takes its own reference rather than the creation one.` |
|     - | 3993 | ` */` |
|   114 | 3994 | `static int ReflectMemberList(ph7_context *pCtx, int iKind, const char *zClass,` |
|     - | 3995 | `	int nArg, ph7_value **apArg)` |
|     3 | 3996 | `{` |
|   117 | 3997 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|   117 | 3998 | `	ph7_class_instance *pObj = ReflectClassObj(pCtx);` |
|   117 | 3999 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     - | 4000 | `	ph7_value sSelf;` |
|     - | 4001 | `	SySet aMembers;` |
|     - | 4002 | `	sxu32 n;` |
|   117 | 4003 | `	int bFilter = (nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0);` |
|   117 | 4004 | `	sxi64 iFilter = bFilter ? ph7_value_to_int64(apArg[0]) : 0;` |
|   117 | 4005 | `	if( pOut == 0 ){` |
|   ! 0 | 4006 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 4007 | `	}` |
|   117 | 4008 | `	if( pClass == 0 ){` |
|   ! 0 | 4009 | `		ph7_result_value(pCtx, pOut);` |
|   ! 0 | 4010 | `		return PH7_OK;` |
|     - | 4011 | `	}` |
|   117 | 4012 | `	ReflectSelfName(pCtx, &sSelf);` |
|   117 | 4013 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|   117 | 4014 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|  1287 | 4015 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|  1173 | 4016 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     - | 4017 | `		ph7_value sName, sVal;` |
|     - | 4018 | `		ph7_value *apCtor[2];` |
|     - | 4019 | `		ph7_class_instance *pRef;` |
|     - | 4020 | `		sxi32 rc;` |
|  1173 | 4021 | `		if( pM->iKind != iKind ){` |
|   904 | 4022 | `			continue;` |
|     - | 4023 | `		}` |
|   281 | 4024 | `		if( bFilter ){` |
|    33 | 4025 | `			sxi64 iMods = iKind == REFLECT_MEMBER_METHOD ? ReflectMethodModifiers(pM->pMeth)` |
|    40 | 4026 | `				: (iKind == REFLECT_MEMBER_CONST ? ReflectConstModifiers(pM->pAttr)` |
|    20 | 4027 | `				                                 : ReflectPropModifiers(pM->pAttr));` |
|    33 | 4028 | `			if( (iMods & iFilter) == 0 ){` |
|    21 | 4029 | `				continue;` |
|     - | 4030 | `			}` |
|     6 | 4031 | `		}` |
|   261 | 4032 | `		PH7_MemObjInit(pCtx->pVm, &sName);` |
|   261 | 4033 | `		ph7_value_string(&sName, SyStringData(&pM->sKey), (int)SyStringLength(&pM->sKey));` |
|   261 | 4034 | `		apCtor[0] = &sSelf;` |
|   261 | 4035 | `		apCtor[1] = &sName;` |
|   261 | 4036 | `		pRef = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|   261 | 4037 | `		PH7_MemObjRelease(&sName);` |
|   261 | 4038 | `		if( pRef == 0 ){` |
|   ! 0 | 4039 | `			SySetRelease(&aMembers);` |
|   ! 0 | 4040 | `			PH7_MemObjRelease(&sSelf);` |
|   ! 0 | 4041 | `			if( rc != PH7_OK ){` |
|   ! 0 | 4042 | `				return rc;` |
|     - | 4043 | `			}` |
|   ! 0 | 4044 | `			ph7_result_value(pCtx, pOut);` |
|   ! 0 | 4045 | `			return PH7_OK;` |
|     - | 4046 | `		}` |
|   261 | 4047 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|   261 | 4048 | `		sVal.x.pOther = pRef;` |
|   261 | 4049 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|   261 | 4050 | `		ph7_array_add_elem(pOut, 0, &sVal);   /* takes its own reference */` |
|   261 | 4051 | `		PH7_ClassInstanceUnref(pRef);` |
|   132 | 4052 | `	}` |
|   117 | 4053 | `	SySetRelease(&aMembers);` |
|     - | 4054 | `	/* A ReflectionObject also reports the instance's own DYNAMIC properties,` |
|     - | 4055 | `	 * which no class declaration knows about. */` |
|   117 | 4056 | `	if( iKind == REFLECT_MEMBER_PROP && pObj != 0 && (!bFilter \|\| (iFilter & 1)) ){` |
|     - | 4057 | `		SyHashEntry *pEntry;` |
|     - | 4058 | `		ph7_value sTarget;` |
|     3 | 4059 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     3 | 4060 | `		sTarget.x.pOther = pObj;` |
|     3 | 4061 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|     3 | 4062 | `		SyHashResetLoopCursor(&pObj->hAttr);` |
|     7 | 4063 | `		while( (pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){` |
|     5 | 4064 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     - | 4065 | `			ph7_value sName, sVal;` |
|     - | 4066 | `			ph7_value *apCtor[2];` |
|     - | 4067 | `			ph7_class_instance *pRef;` |
|     - | 4068 | `			sxi32 rc;` |
|     5 | 4069 | `			if( pVmAttr->pAttr == 0 \|\| (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) == 0 ){` |
|   ! 0 | 4070 | `				continue;` |
|     - | 4071 | `			}` |
|     5 | 4072 | `			PH7_MemObjInit(pCtx->pVm, &sName);` |
|     7 | 4073 | `			ph7_value_string(&sName, SyStringData(&pVmAttr->pAttr->sName),` |
|     4 | 4074 | `				(int)SyStringLength(&pVmAttr->pAttr->sName));` |
|     5 | 4075 | `			apCtor[0] = &sTarget;` |
|     5 | 4076 | `			apCtor[1] = &sName;` |
|     5 | 4077 | `			pRef = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);` |
|     5 | 4078 | `			PH7_MemObjRelease(&sName);` |
|     5 | 4079 | `			if( pRef == 0 ){` |
|   ! 0 | 4080 | `				break;` |
|     - | 4081 | `			}` |
|     5 | 4082 | `			PH7_MemObjInit(pCtx->pVm, &sVal);` |
|     5 | 4083 | `			sVal.x.pOther = pRef;` |
|     5 | 4084 | `			sVal.iFlags = MEMOBJ_OBJ;` |
|     5 | 4085 | `			ph7_array_add_elem(pOut, 0, &sVal);` |
|     5 | 4086 | `			PH7_ClassInstanceUnref(pRef);` |
|     1 | 4087 | `		}` |
|     - | 4088 | `		/* sTarget borrows pObj and never took a reference: nothing to release. */` |
|     1 | 4089 | `	}` |
|   117 | 4090 | `	PH7_MemObjRelease(&sSelf);` |
|   117 | 4091 | `	ph7_result_value(pCtx, pOut);` |
|   117 | 4092 | `	return PH7_OK;` |
|    60 | 4093 | `}` |
|    48 | 4094 | `static int vm_builtin_ReflectionClass_getMethods(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 4095 | `{` |
|    50 | 4096 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_METHOD, "ReflectionMethod", nArg, apArg);` |
|     2 | 4097 | `}` |
|    60 | 4098 | `static int vm_builtin_ReflectionClass_getProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 4099 | `{` |
|    63 | 4100 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_PROP, "ReflectionProperty", nArg, apArg);` |
|     3 | 4101 | `}` |
|     6 | 4102 | `static int vm_builtin_ReflectionClass_getReflectionConstants(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 4103 | `{` |
|     7 | 4104 | `	return ReflectMemberList(pCtx, REFLECT_MEMBER_CONST, "ReflectionClassConstant", nArg, apArg);` |
|     1 | 4105 | `}` |
|     8 | 4106 | `static int vm_builtin_ReflectionClass_getProperty(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 4107 | `{` |
|     9 | 4108 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     9 | 4109 | `	ph7_class_instance *pObj = ReflectClassObj(pCtx);` |
|     - | 4110 | `	const char *zName;` |
|     9 | 4111 | `	int nName, bFound = 0;` |
|     - | 4112 | `	SySet aMembers;` |
|     - | 4113 | `	sxu32 n;` |
|     - | 4114 | `	SyString sFound;` |
|     9 | 4115 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|   ! 0 | 4116 | `		ph7_result_null(pCtx);` |
|   ! 0 | 4117 | `		return PH7_OK;` |
|     - | 4118 | `	}` |
|     9 | 4119 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     9 | 4120 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     9 | 4121 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|    41 | 4122 | `	for( n = 0 ; n < SySetUsed(&aMembers) && !bFound ; n++ ){` |
|    33 | 4123 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|    33 | 4124 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zName, nName) ){` |
|     3 | 4125 | `			sFound = pM->sKey;` |
|     3 | 4126 | `			bFound = 1;` |
|     1 | 4127 | `		}` |
|    17 | 4128 | `	}` |
|     9 | 4129 | `	SySetRelease(&aMembers);` |
|     9 | 4130 | `	if( bFound ){` |
|     - | 4131 | `		ph7_value sSelf;` |
|     - | 4132 | `		int rc;` |
|     3 | 4133 | `		ReflectSelfName(pCtx, &sSelf);` |
|     3 | 4134 | `		rc = ReflectResultMember(pCtx, "ReflectionProperty", &sSelf, &sFound);` |
|     3 | 4135 | `		PH7_MemObjRelease(&sSelf);` |
|     3 | 4136 | `		return rc;` |
|     - | 4137 | `	}` |
|     7 | 4138 | `	if( ReflectObjHasProp(pObj, zName, nName) ){` |
|     - | 4139 | `		/* A dynamic property: the reflector is built over the OBJECT, since the` |
|     - | 4140 | `		 * class declaration has no record of it. */` |
|     - | 4141 | `		ph7_value sTarget;` |
|     - | 4142 | `		SyString sName;` |
|     - | 4143 | `		int rc;` |
|     3 | 4144 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     3 | 4145 | `		sTarget.x.pOther = pObj;` |
|     3 | 4146 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|     3 | 4147 | `		SyStringInitFromBuf(&sName, zName, nName);` |
|     3 | 4148 | `		rc = ReflectResultMember(pCtx, "ReflectionProperty", &sTarget, &sName);` |
|     3 | 4149 | `		return rc;` |
|     - | 4150 | `	}` |
|     7 | 4151 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     2 | 4152 | `		"Property %z::$%.*s does not exist", &pClass->sName, nName, zName);` |
|     5 | 4153 | `}` |
|     4 | 4154 | `static int vm_builtin_ReflectionClass_getReflectionConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 4155 | `{` |
|     5 | 4156 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 4157 | `	const char *zName;` |
|     5 | 4158 | `	int nName, bFound = 0;` |
|     - | 4159 | `	SySet aMembers;` |
|     - | 4160 | `	sxu32 n;` |
|     - | 4161 | `	SyString sFound;` |
|     5 | 4162 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|   ! 0 | 4163 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 4164 | `		return PH7_OK;` |
|     - | 4165 | `	}` |
|     5 | 4166 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     5 | 4167 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     5 | 4168 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|    15 | 4169 | `	for( n = 0 ; n < SySetUsed(&aMembers) && !bFound ; n++ ){` |
|    11 | 4170 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|    11 | 4171 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){` |
|     3 | 4172 | `			sFound = pM->sKey;` |
|     3 | 4173 | `			bFound = 1;` |
|     1 | 4174 | `		}` |
|     6 | 4175 | `	}` |
|     5 | 4176 | `	SySetRelease(&aMembers);` |
|     5 | 4177 | `	if( !bFound ){` |
|     3 | 4178 | `		ph7_result_bool(pCtx, 0);` |
|     3 | 4179 | `		return PH7_OK;` |
|     - | 4180 | `	}` |
|     - | 4181 | `	{` |
|     - | 4182 | `		ph7_value sSelf;` |
|     - | 4183 | `		int rc;` |
|     3 | 4184 | `		ReflectSelfName(pCtx, &sSelf);` |
|     3 | 4185 | `		rc = ReflectResultMember(pCtx, "ReflectionClassConstant", &sSelf, &sFound);` |
|     3 | 4186 | `		PH7_MemObjRelease(&sSelf);` |
|     3 | 4187 | `		return rc;` |
|     - | 4188 | `	}` |
|     3 | 4189 | `}` |
|     - | 4190 | `/* ---- statics and defaults ---- */` |
|     - | 4191 | `/* Reading or writing a static through reflection materializes the class's` |
|     - | 4192 | `` * static table exactly as `C::$s` does, so a default that threw at the`` |
|     - | 4193 | ` * declaration raises HERE. */` |
|    50 | 4194 | `static sxi32 ReflectMaterializeStatics(ph7_context *pCtx, ph7_class *pClass)` |
|     2 | 4195 | `{` |
|    52 | 4196 | `	if( VmClassStaticDeferPending(pClass) ){` |
|    21 | 4197 | `		return PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);` |
|     - | 4198 | `	}` |
|    32 | 4199 | `	return SXRET_OK;` |
|    27 | 4200 | `}` |
|     6 | 4201 | `static int vm_builtin_ReflectionClass_getStaticProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 4202 | `{` |
|     8 | 4203 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     8 | 4204 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     - | 4205 | `	SySet aMembers;` |
|     - | 4206 | `	sxu32 n;` |
|     3 | 4207 | `	SXUNUSED(nArg);` |
|     3 | 4208 | `	SXUNUSED(apArg);` |
|     8 | 4209 | `	if( pOut == 0 ){` |
|   ! 0 | 4210 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 4211 | `	}` |
|     8 | 4212 | `	if( pClass == 0 ){` |
|   ! 0 | 4213 | `		ph7_result_value(pCtx, pOut);` |
|   ! 0 | 4214 | `		return PH7_OK;` |
|     - | 4215 | `	}` |
|     - | 4216 | `	{` |
|     8 | 4217 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|     8 | 4218 | `		if( rc != SXRET_OK ){` |
|     3 | 4219 | `			return rc;` |
|     - | 4220 | `		}` |
|     - | 4221 | `	}` |
|     5 | 4222 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     5 | 4223 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|    29 | 4224 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    25 | 4225 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     - | 4226 | `		ph7_value *pVal;` |
|     - | 4227 | `		SyHashEntry *pSlot;` |
|    24 | 4228 | `		if( pM->iKind != REFLECT_MEMBER_PROP` |
|    24 | 4229 | `		 \|\| (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|    15 | 4230 | `			continue;` |
|     - | 4231 | `		}` |
|     - | 4232 | `		/* An UNINITIALIZED typed static has no value to report, and php simply` |
|     - | 4233 | `		 * leaves it out rather than raising the read Error here. */` |
|    11 | 4234 | `		pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pM->pAttr->nIdx, sizeof(sxu32));` |
|    11 | 4235 | `		if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|     3 | 4236 | `			continue;` |
|     - | 4237 | `		}` |
|     9 | 4238 | `		pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pM->pAttr->nIdx);` |
|     9 | 4239 | `		if( pVal ){` |
|     9 | 4240 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, pVal);` |
|     4 | 4241 | `		}` |
|     5 | 4242 | `	}` |
|     5 | 4243 | `	SySetRelease(&aMembers);` |
|     5 | 4244 | `	ph7_result_value(pCtx, pOut);` |
|     5 | 4245 | `	return PH7_OK;` |
|     5 | 4246 | `}` |
|     - | 4247 | `/* The declared STATIC property of this name, or NULL. */` |
|    18 | 4248 | `static ph7_class_attr * ReflectStaticAttr(ph7_context *pCtx, ph7_class *pClass,` |
|     - | 4249 | `	const char *zName, int nName)` |
|     2 | 4250 | `{` |
|     - | 4251 | `	SyHashEntry *pEntry;` |
|     - | 4252 | `	ph7_class_attr *pAttr;` |
|    20 | 4253 | `	if( nName < 1 ){` |
|   ! 0 | 4254 | `		return 0;` |
|     - | 4255 | `	}` |
|    20 | 4256 | `	pEntry = SyHashGet(&pClass->hAttr, (const void *)zName, (sxu32)nName);` |
|    20 | 4257 | `	if( pEntry == 0 ){` |
|     9 | 4258 | `		return 0;` |
|     - | 4259 | `	}` |
|    12 | 4260 | `	pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|     5 | 4261 | `	SXUNUSED(pCtx);` |
|    12 | 4262 | `	return (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? pAttr : 0;` |
|    11 | 4263 | `}` |
|    20 | 4264 | `static int vm_builtin_ReflectionClass_getStaticPropertyValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 4265 | `{` |
|    22 | 4266 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 4267 | `	ph7_class_attr *pAttr;` |
|     - | 4268 | `	const char *zName;` |
|     - | 4269 | `	int nName;` |
|    22 | 4270 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|   ! 0 | 4271 | `		ph7_result_null(pCtx);` |
|   ! 0 | 4272 | `		return PH7_OK;` |
|     - | 4273 | `	}` |
|     - | 4274 | `	{` |
|    22 | 4275 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|    22 | 4276 | `		if( rc != SXRET_OK ){` |
|     7 | 4277 | `			return rc;` |
|     - | 4278 | `		}` |
|     - | 4279 | `	}` |
|    16 | 4280 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|    16 | 4281 | `	pAttr = ReflectStaticAttr(pCtx, pClass, zName, nName);` |
|    16 | 4282 | `	if( pAttr == 0 ){` |
|     7 | 4283 | `		if( nArg > 1 ){` |
|     5 | 4284 | `			ph7_result_value(pCtx, apArg[1]);` |
|     5 | 4285 | `			return PH7_OK;` |
|     - | 4286 | `		}` |
|     4 | 4287 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     1 | 4288 | `			"Property %z::$%.*s does not exist", &pClass->sName, nName, zName);` |
|     - | 4289 | `	}` |
|     - | 4290 | `	{` |
|     - | 4291 | `		/* Uninitialized typed static: the same Error the VM raises on read */` |
|    10 | 4292 | `		SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|     - | 4293 | `		ph7_value *pVal;` |
|    10 | 4294 | `		if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|     - | 4295 | `			/* php says "Typed property" HERE and "Typed static property" from` |
|     - | 4296 | ``			 * ReflectionProperty::getValue() and from `A::$s` itself — the`` |
|     - | 4297 | `			 * three wordings were checked against the oracle, they really do` |
|     - | 4298 | `			 * differ by call site. */` |
|     3 | 4299 | `			ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|     4 | 4300 | `			return PH7_VmThrowException(pCtx, "Error",` |
|     - | 4301 | `				"Typed property %z::$%z must not be accessed before initialization",` |
|     1 | 4302 | `				&pDecl->sName, &pAttr->sName);` |
|     - | 4303 | `		}` |
|     8 | 4304 | `		pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|     8 | 4305 | `		if( pVal ){` |
|     8 | 4306 | `			ph7_result_value(pCtx, pVal);` |
|     5 | 4307 | `		}else{` |
|   ! 0 | 4308 | `			ph7_result_null(pCtx);` |
|     - | 4309 | `		}` |
|     - | 4310 | `	}` |
|     8 | 4311 | `	return PH7_OK;` |
|    12 | 4312 | `}` |
|     8 | 4313 | `static int vm_builtin_ReflectionClass_setStaticPropertyValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 4314 | `{` |
|    10 | 4315 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 4316 | `	ph7_class_attr *pAttr;` |
|     - | 4317 | `	ph7_value *pSlot;` |
|     - | 4318 | `	const char *zName;` |
|     - | 4319 | `	int nName;` |
|    10 | 4320 | `	if( pClass == 0 \|\| nArg < 2 ){` |
|   ! 0 | 4321 | `		return PH7_OK;` |
|     - | 4322 | `	}` |
|     - | 4323 | `	{` |
|    10 | 4324 | `		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|    10 | 4325 | `		if( rc != SXRET_OK ){` |
|     5 | 4326 | `			return rc;` |
|     - | 4327 | `		}` |
|     - | 4328 | `	}` |
|     5 | 4329 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|     5 | 4330 | `	pAttr = ReflectStaticAttr(pCtx, pClass, zName, nName);` |
|     5 | 4331 | `	if( pAttr == 0 ){` |
|     4 | 4332 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     1 | 4333 | `			"Class %z does not have a property named %.*s", &pClass->sName, nName, zName);` |
|     - | 4334 | `	}` |
|     3 | 4335 | `	pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|     3 | 4336 | `	if( pSlot == 0 ){` |
|   ! 0 | 4337 | `		return PH7_OK;` |
|     - | 4338 | `	}` |
|     - | 4339 | `	{` |
|     3 | 4340 | `		sxi32 rc = ReflectEnforceStore(pCtx, pAttr->nIdx, apArg[1]);` |
|     3 | 4341 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 4342 | `			return rc;` |
|     - | 4343 | `		}` |
|     - | 4344 | `	}` |
|     3 | 4345 | `	PH7_MemObjStore(apArg[1], pSlot);` |
|     3 | 4346 | `	return PH7_OK;` |
|     6 | 4347 | `}` |
|     4 | 4348 | `static int vm_builtin_ReflectionClass_getDefaultProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 4349 | `{` |
|     5 | 4350 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     5 | 4351 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     - | 4352 | `	SySet aMembers;` |
|     - | 4353 | `	sxu32 n;` |
|     - | 4354 | `	int iPass;` |
|     2 | 4355 | `	SXUNUSED(nArg);` |
|     2 | 4356 | `	SXUNUSED(apArg);` |
|     5 | 4357 | `	if( pOut == 0 ){` |
|   ! 0 | 4358 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 4359 | `	}` |
|     5 | 4360 | `	if( pClass == 0 ){` |
|   ! 0 | 4361 | `		ph7_result_value(pCtx, pOut);` |
|   ! 0 | 4362 | `		return PH7_OK;` |
|     - | 4363 | `	}` |
|     5 | 4364 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|     5 | 4365 | `	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);` |
|     - | 4366 | `	/* php reports the STATIC properties first, then the instance ones. */` |
|    13 | 4367 | `	for( iPass = 0 ; iPass < 2 ; iPass++ ){` |
|    57 | 4368 | `		for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|    49 | 4369 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     - | 4370 | `			int bStatic;` |
|     - | 4371 | `			ph7_value sValue;` |
|    49 | 4372 | `			if( pM->iKind != REFLECT_MEMBER_PROP ){` |
|    18 | 4373 | `				continue;` |
|     - | 4374 | `			}` |
|    45 | 4375 | `			bStatic = (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|    45 | 4376 | `			if( bStatic != (iPass == 0) ){` |
|    23 | 4377 | `				continue;` |
|     - | 4378 | `			}` |
|    22 | 4379 | `			if( (pM->pAttr->iFlags & PH7_CLASS_ATTR_TYPED)` |
|    16 | 4380 | `			 && SySetUsed(&pM->pAttr->aByteCode) < 1 ){` |
|     - | 4381 | `				/* A TYPED property with no initializer has no default at all —` |
|     - | 4382 | `				 * it is uninitialized, and php leaves it out. An UNTYPED one` |
|     - | 4383 | `				 * without an initializer defaults to null and is listed. */` |
|     5 | 4384 | `				continue;` |
|     - | 4385 | `			}` |
|    19 | 4386 | `			PH7_MemObjInit(pCtx->pVm, &sValue);` |
|    19 | 4387 | `			if( SySetUsed(&pM->pAttr->aByteCode) > 0 ){` |
|     - | 4388 | `				/* Same evaluation path the VM uses for omitted call arguments */` |
|    15 | 4389 | `				VmLocalExec(pCtx->pVm, &pM->pAttr->aByteCode, &sValue, FALSE);` |
|     7 | 4390 | `			}` |
|    19 | 4391 | `			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, &sValue);` |
|    19 | 4392 | `			PH7_MemObjRelease(&sValue);` |
|    10 | 4393 | `		}` |
|     5 | 4394 | `	}` |
|     5 | 4395 | `	SySetRelease(&aMembers);` |
|     5 | 4396 | `	ph7_result_value(pCtx, pOut);` |
|     5 | 4397 | `	return PH7_OK;` |
|     3 | 4398 | `}` |
|     - | 4399 | `/* ---- attributes, extension, export ---- */` |
|    54 | 4400 | `static int vm_builtin_ReflectionClass_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 4401 | `{` |
|    56 | 4402 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 4403 | `	const char *zName;` |
|     - | 4404 | `	int nName;` |
|    56 | 4405 | `	if( pClass == 0 ){` |
|   ! 0 | 4406 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|   ! 0 | 4407 | `		return PH7_OK;` |
|     - | 4408 | `	}` |
|    56 | 4409 | `	ReflectClassName(pCtx, &zName, &nName);` |
|     - | 4410 | `	{` |
|     - | 4411 | `		ph7_value sTarget;` |
|     - | 4412 | `		int rc;` |
|    56 | 4413 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|    56 | 4414 | `		ph7_value_string(&sTarget, zName, nName);` |
|     - | 4415 | `		/* 1 = Attribute::TARGET_CLASS */` |
|    83 | 4416 | `		rc = ReflectBuildAttrs(pCtx, &pClass->aAttrs, "class", &sTarget, 0, 0, 0, 1,` |
|    27 | 4417 | `			nArg, apArg);` |
|    56 | 4418 | `		PH7_MemObjRelease(&sTarget);` |
|    56 | 4419 | `		return rc;` |
|     - | 4420 | `	}` |
|    29 | 4421 | `}` |
|   ! 0 | 4422 | `static int vm_builtin_ReflectionClass_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 4423 | `{` |
|   ! 0 | 4424 | `	SXUNUSED(nArg);` |
|   ! 0 | 4425 | `	SXUNUSED(apArg);` |
|   ! 0 | 4426 | `	if( ReflectClassFlag(pCtx, PH7_CLASS_INTERNAL) ){` |
|   ! 0 | 4427 | `		ph7_result_string(pCtx, "Core", sizeof("Core")-1);` |
|   ! 0 | 4428 | `	}else{` |
|   ! 0 | 4429 | `		ph7_result_bool(pCtx, 0);` |
|     - | 4430 | `	}` |
|   ! 0 | 4431 | `	return PH7_OK;` |
|   ! 0 | 4432 | `}` |
|     - | 4433 | `/*` |
|     - | 4434 | ` * The synthetic "Core" extension, which is the only one PHL has. Answered by` |
|     - | 4435 | ` * every reflector's getExtension() for an INTERNAL target.` |
|     - | 4436 | ` */` |
|     6 | 4437 | `static int ReflectCoreExtension(ph7_context *pCtx)` |
|     1 | 4438 | `{` |
|     - | 4439 | `	ph7_value sName;` |
|     - | 4440 | `	ph7_value *apCtor[1];` |
|     - | 4441 | `	ph7_class_instance *pExt;` |
|     - | 4442 | `	sxi32 rc;` |
|     7 | 4443 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|     7 | 4444 | `	ph7_value_string(&sName, "Core", sizeof("Core")-1);` |
|     7 | 4445 | `	apCtor[0] = &sName;` |
|     7 | 4446 | `	pExt = ReflectConstruct(pCtx, "ReflectionExtension", 1, apCtor, &rc);` |
|     7 | 4447 | `	PH7_MemObjRelease(&sName);` |
|     7 | 4448 | `	if( pExt == 0 ){` |
|   ! 0 | 4449 | `		if( rc != PH7_OK ){` |
|   ! 0 | 4450 | `			return rc;` |
|     - | 4451 | `		}` |
|   ! 0 | 4452 | `		ph7_result_null(pCtx);` |
|   ! 0 | 4453 | `		return PH7_OK;` |
|     - | 4454 | `	}` |
|     7 | 4455 | `	return ReflectResultObject(pCtx, pExt);` |
|     4 | 4456 | `}` |
|     - | 4457 | `/* php's export format (chunk 9) — defined at the END of this file, where the` |
|     - | 4458 | ` * member walk, the function reference and the parameter description it reads` |
|     - | 4459 | ` * are all in scope. */` |
|     - | 4460 | `static int ReflectExportClassSelf(ph7_context *pCtx);` |
|     - | 4461 | `static int ReflectExportFuncSelf(ph7_context *pCtx);` |
|     - | 4462 | `static int ReflectExportParamSelf(ph7_context *pCtx);` |
|     - | 4463 | `static int ReflectExportPropSelf(ph7_context *pCtx);` |
|     - | 4464 | `static int ReflectExportConstSelf(ph7_context *pCtx);` |
|     - | 4465 | `/*` |
|     - | 4466 | ` * __toString(): php's export format, still chunk 9 — a PHP function written` |
|     - | 4467 | `` * against the PUBLIC reflection API of its target, so it is called with `$this`.`` |
|     - | 4468 | ` * bIndentArg adds the export family's second "" indent argument.` |
|     - | 4469 | ` */` |
|     4 | 4470 | `static int vm_builtin_ReflectionClass_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 4471 | `{` |
|     2 | 4472 | `	SXUNUSED(nArg);` |
|     2 | 4473 | `	SXUNUSED(apArg);` |
|     5 | 4474 | `	if( !ReflectClassFlag(pCtx, PH7_CLASS_INTERNAL) ){` |
|     3 | 4475 | `		ph7_result_null(pCtx);` |
|     3 | 4476 | `		return PH7_OK;` |
|     - | 4477 | `	}` |
|     3 | 4478 | `	return ReflectCoreExtension(pCtx);` |
|     3 | 4479 | `}` |
|    28 | 4480 | `static int vm_builtin_ReflectionClass_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 4481 | `{` |
|    14 | 4482 | `	SXUNUSED(nArg);` |
|    14 | 4483 | `	SXUNUSED(apArg);` |
|    29 | 4484 | `	return ReflectExportClassSelf(pCtx);` |
|     1 | 4485 | `}` |
|     - | 4486 | `/* ---- lazy objects: PHL has none (§7.4) ---- */` |
|   ! 0 | 4487 | `static int ReflectNoLazy(ph7_context *pCtx, const char *zWho)` |
|   ! 0 | 4488 | `{` |
|   ! 0 | 4489 | `	return PH7_VmThrowException(pCtx, "Error",` |
|   ! 0 | 4490 | `		"%s is not supported by PHL (no lazy objects)", zWho);` |
|   ! 0 | 4491 | `}` |
|     - | 4492 | `#define REFLECT_NO_LAZY(NAME,TEXT) \` |
|     - | 4493 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|     - | 4494 | `	{ \` |
|     - | 4495 | `		SXUNUSED(nArg); \` |
|     - | 4496 | `		SXUNUSED(apArg); \` |
|     - | 4497 | `		return ReflectNoLazy(pCtx, TEXT); \` |
|     - | 4498 | `	}` |
|   ! 0 | 4499 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_newLazyGhost,"ReflectionClass::newLazyGhost()")` |
|   ! 0 | 4500 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_newLazyProxy,"ReflectionClass::newLazyProxy()")` |
|   ! 0 | 4501 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_resetAsLazyGhost,"ReflectionClass::resetAsLazyGhost()")` |
|   ! 0 | 4502 | `REFLECT_NO_LAZY(vm_builtin_ReflectionClass_resetAsLazyProxy,"ReflectionClass::resetAsLazyProxy()")` |
|     - | 4503 | `/* The four QUERIES answer what is true of a VM with no lazy objects, rather` |
|     - | 4504 | ` * than refusing: an object here is always initialized and never has one. */` |
|   ! 0 | 4505 | `static int vm_builtin_ReflectionClass_getLazyInitializer(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 4506 | `{` |
|   ! 0 | 4507 | `	SXUNUSED(nArg);` |
|   ! 0 | 4508 | `	SXUNUSED(apArg);` |
|   ! 0 | 4509 | `	ph7_result_null(pCtx);` |
|   ! 0 | 4510 | `	return PH7_OK;` |
|   ! 0 | 4511 | `}` |
|   ! 0 | 4512 | `static int vm_builtin_ReflectionClass_passThroughObject(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 4513 | `{` |
|   ! 0 | 4514 | `	if( nArg > 0 ){` |
|   ! 0 | 4515 | `		ph7_result_value(pCtx, apArg[0]);` |
|   ! 0 | 4516 | `	}else{` |
|   ! 0 | 4517 | `		ph7_result_null(pCtx);` |
|     - | 4518 | `	}` |
|   ! 0 | 4519 | `	return PH7_OK;` |
|   ! 0 | 4520 | `}` |
|   ! 0 | 4521 | `static int vm_builtin_ReflectionClass_isUninitializedLazyObject(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 4522 | `{` |
|   ! 0 | 4523 | `	SXUNUSED(nArg);` |
|   ! 0 | 4524 | `	SXUNUSED(apArg);` |
|   ! 0 | 4525 | `	ph7_result_bool(pCtx, 0);` |
|   ! 0 | 4526 | `	return PH7_OK;` |
|   ! 0 | 4527 | `}` |
|     - | 4528 | `/*` |
|     - | 4529 | ` * Reflection::getModifierNames(int $modifiers)` |
|     - | 4530 | ` *` |
|     - | 4531 | ` * php's own order and its own SWITCH: the visibility names come out of one` |
|     - | 4532 | ` * three-way choice and the set-visibility names out of another, so a mask with` |
|     - | 4533 | ` * two visibility bits set names NEITHER (which is what php answers).` |
|     - | 4534 | ` */` |
|    66 | 4535 | `static int vm_builtin_Reflection_getModifierNames(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 4536 | `{` |
|    68 | 4537 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|    68 | 4538 | `	sxi64 iMods = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;` |
|     - | 4539 | `	const char *azName[8];` |
|    68 | 4540 | `	int nName = 0, n;` |
|    68 | 4541 | `	if( pOut == 0 ){` |
|   ! 0 | 4542 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 4543 | `	}` |
|    68 | 4544 | `	if( iMods & 64 ){ azName[nName++] = "abstract"; }` |
|    68 | 4545 | `	if( iMods & 32 ){ azName[nName++] = "final"; }` |
|    68 | 4546 | `	if( iMods & 512 ){ azName[nName++] = "virtual"; }` |
|    68 | 4547 | `	switch( iMods & (1\|2\|4) ){` |
|    34 | 4548 | `	case 1: azName[nName++] = "public"; break;` |
|    17 | 4549 | `	case 2: azName[nName++] = "protected"; break;` |
|    11 | 4550 | `	case 4: azName[nName++] = "private"; break;` |
|     8 | 4551 | `	default: break;` |
|     - | 4552 | `	}` |
|    68 | 4553 | `	switch( iMods & (2048\|4096) ){` |
|    12 | 4554 | `	case 2048: azName[nName++] = "protected(set)"; break;` |
|     3 | 4555 | `	case 4096: azName[nName++] = "private(set)"; break;` |
|    54 | 4556 | `	default: break;` |
|     - | 4557 | `	}` |
|    68 | 4558 | `	if( iMods & 16 ){ azName[nName++] = "static"; }` |
|    68 | 4559 | `	if( iMods & 128 ){ azName[nName++] = "readonly"; }` |
|   174 | 4560 | `	for( n = 0 ; n < nName ; n++ ){` |
|   108 | 4561 | `		ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|   108 | 4562 | `		if( pName == 0 ){ break; }` |
|   108 | 4563 | `		ph7_value_string(pName, azName[n], -1);` |
|   108 | 4564 | `		ph7_array_add_elem(pOut, 0, pName);` |
|    55 | 4565 | `	}` |
|    68 | 4566 | `	ph7_result_value(pCtx, pOut);` |
|    68 | 4567 | `	return PH7_OK;` |
|    35 | 4568 | `}` |
|     - | 4569 | `/*` |
|     - | 4570 | ` * Declare chunk 1. Called from PH7_VmInstallReflection() where it used to be` |
|     - | 4571 | `` * compiled — before chunks 2 and 3, which name `Reflector` in their own`` |
|     - | 4572 | `` * `implements` clauses.`` |
|     - | 4573 | ` *` |
|     - | 4574 | ` * The method table is in php's own DECLARATION order, which is the order` |
|     - | 4575 | ` * ReflectionClass::getMethods() reports for these classes themselves.` |
|     - | 4576 | ` */` |
|  5146 | 4577 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionClass(ph7_vm *pVm)` |
|     5 | 4578 | `{` |
|     - | 4579 | `	static const PH7_NativePropDef aClassProp[] = {` |
|     - | 4580 | `		{ "name",  PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|     - | 4581 | `		/* PHL-only: the instance a ReflectionObject was built over. php keeps it` |
|     - | 4582 | `		 * out of sight; PHL has no hidden-slot bit yet (§7.4 (e)). */` |
|     - | 4583 | `		{ RC_OBJ,  PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 4584 | `	};` |
|     - | 4585 | `	static const PH7_NativeMethodDef aClassMethod[] = {` |
|     - | 4586 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionClass_clone },` |
|     - | 4587 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $objectOrClass", "",` |
|     - | 4588 | `		  vm_builtin_ReflectionClass_construct },` |
|     - | 4589 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionClass_toString },` |
|     - | 4590 | `		{ "getName",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getName },` |
|     - | 4591 | `		{ "isInternal",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInternal },` |
|     - | 4592 | `		{ "isUserDefined", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isUserDefined },` |
|     - | 4593 | `		{ "isAnonymous",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isAnonymous },` |
|     - | 4594 | `		{ "isInstantiable",PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInstantiable },` |
|     - | 4595 | `		{ "isCloneable",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isCloneable },` |
|     - | 4596 | `		{ "getFileName",   PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getFileName },` |
|     - | 4597 | `		{ "getStartLine",  PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionClass_getStartLine },` |
|     - | 4598 | `		{ "getEndLine",    PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionClass_getEndLine },` |
|     - | 4599 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getDocComment },` |
|     - | 4600 | `		{ "getConstructor",PH7_MOD_PUBLIC, "", "@?ReflectionMethod", vm_builtin_ReflectionClass_getConstructor },` |
|     - | 4601 | `		{ "hasMethod",     PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasMethod },` |
|     - | 4602 | `		{ "getMethod",     PH7_MOD_PUBLIC, "string $name", "@ReflectionMethod", vm_builtin_ReflectionClass_getMethod },` |
|     - | 4603 | `		{ "getMethods",    PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|     - | 4604 | `		  vm_builtin_ReflectionClass_getMethods },` |
|     - | 4605 | `		{ "hasProperty",   PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasProperty },` |
|     - | 4606 | `		{ "getProperty",   PH7_MOD_PUBLIC, "string $name", "@ReflectionProperty", vm_builtin_ReflectionClass_getProperty },` |
|     - | 4607 | `		{ "getProperties", PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|     - | 4608 | `		  vm_builtin_ReflectionClass_getProperties },` |
|     - | 4609 | `		{ "hasConstant",   PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasConstant },` |
|     - | 4610 | `		{ "getConstants",  PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|     - | 4611 | `		  vm_builtin_ReflectionClass_getConstants },` |
|     - | 4612 | `		{ "getReflectionConstants", PH7_MOD_PUBLIC, "?int $filter = null", "@array",` |
|     - | 4613 | `		  vm_builtin_ReflectionClass_getReflectionConstants },` |
|     - | 4614 | `		{ "getConstant",   PH7_MOD_PUBLIC, "string $name", "@mixed", vm_builtin_ReflectionClass_getConstant },` |
|     - | 4615 | `		{ "getReflectionConstant", PH7_MOD_PUBLIC, "string $name", "@ReflectionClassConstant\|false",` |
|     - | 4616 | `		  vm_builtin_ReflectionClass_getReflectionConstant },` |
|     - | 4617 | `		{ "getInterfaces",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getInterfaces },` |
|     - | 4618 | `		{ "getInterfaceNames", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getInterfaceNames },` |
|     - | 4619 | `		{ "isInterface",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInterface },` |
|     - | 4620 | `		{ "getTraits",         PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraits },` |
|     - | 4621 | `		{ "getTraitNames",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraitNames },` |
|     - | 4622 | `		{ "getTraitAliases",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraitAliases },` |
|     - | 4623 | `		{ "isTrait",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isTrait },` |
|     - | 4624 | `		{ "isEnum",            PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClass_isEnum },` |
|     - | 4625 | `		{ "isAbstract",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isAbstract },` |
|     - | 4626 | `		{ "isFinal",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isFinal },` |
|     - | 4627 | `		{ "isReadOnly",        PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClass_isReadOnly },` |
|     - | 4628 | `		{ "getModifiers",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionClass_getModifiers },` |
|     - | 4629 | `		{ "isInstance",        PH7_MOD_PUBLIC, "object $object", "@bool",` |
|     - | 4630 | `		  vm_builtin_ReflectionClass_isInstance },` |
|     - | 4631 | `		{ "newInstance",       PH7_MOD_PUBLIC, "mixed ...$args", "@object",` |
|     - | 4632 | `		  vm_builtin_ReflectionClass_newInstance },` |
|     - | 4633 | `		{ "newInstanceWithoutConstructor", PH7_MOD_PUBLIC, "", "@object",` |
|     - | 4634 | `		  vm_builtin_ReflectionClass_newInstanceWithoutConstructor },` |
|     - | 4635 | `		{ "newInstanceArgs",   PH7_MOD_PUBLIC, "array $args = []", "@?object",` |
|     - | 4636 | `		  vm_builtin_ReflectionClass_newInstanceArgs },` |
|     - | 4637 | `		{ "newLazyGhost",      PH7_MOD_PUBLIC, "callable $initializer, int $options = 0", "object",` |
|     - | 4638 | `		  vm_builtin_ReflectionClass_newLazyGhost },` |
|     - | 4639 | `		{ "newLazyProxy",      PH7_MOD_PUBLIC, "callable $factory, int $options = 0", "object",` |
|     - | 4640 | `		  vm_builtin_ReflectionClass_newLazyProxy },` |
|     - | 4641 | `		{ "resetAsLazyGhost",  PH7_MOD_PUBLIC,` |
|     - | 4642 | `		  "object $object, callable $initializer, int $options = 0", "void",` |
|     - | 4643 | `		  vm_builtin_ReflectionClass_resetAsLazyGhost },` |
|     - | 4644 | `		{ "resetAsLazyProxy",  PH7_MOD_PUBLIC,` |
|     - | 4645 | `		  "object $object, callable $factory, int $options = 0", "void",` |
|     - | 4646 | `		  vm_builtin_ReflectionClass_resetAsLazyProxy },` |
|     - | 4647 | `		{ "initializeLazyObject", PH7_MOD_PUBLIC, "object $object", "object",` |
|     - | 4648 | `		  vm_builtin_ReflectionClass_passThroughObject },` |
|     - | 4649 | `		{ "isUninitializedLazyObject", PH7_MOD_PUBLIC, "object $object", "bool",` |
|     - | 4650 | `		  vm_builtin_ReflectionClass_isUninitializedLazyObject },` |
|     - | 4651 | `		{ "markLazyObjectAsInitialized", PH7_MOD_PUBLIC, "object $object", "object",` |
|     - | 4652 | `		  vm_builtin_ReflectionClass_passThroughObject },` |
|     - | 4653 | `		{ "getLazyInitializer", PH7_MOD_PUBLIC, "object $object", "?callable",` |
|     - | 4654 | `		  vm_builtin_ReflectionClass_getLazyInitializer },` |
|     - | 4655 | `		{ "getParentClass",    PH7_MOD_PUBLIC, "", "@ReflectionClass\|false", vm_builtin_ReflectionClass_getParentClass },` |
|     - | 4656 | `		{ "isSubclassOf",      PH7_MOD_PUBLIC, "ReflectionClass\|string $class", "@bool",` |
|     - | 4657 | `		  vm_builtin_ReflectionClass_isSubclassOf },` |
|     - | 4658 | `		{ "getStaticProperties", PH7_MOD_PUBLIC, "", "@array",` |
|     - | 4659 | `		  vm_builtin_ReflectionClass_getStaticProperties },` |
|     - | 4660 | ``		/* `mixed $default = ?` is the table's "optional, no default VALUE"`` |
|     - | 4661 | `		 * marker — php's own shape here: isOptional() true,` |
|     - | 4662 | `		 * isDefaultValueAvailable() false. */` |
|     - | 4663 | `		{ "getStaticPropertyValue", PH7_MOD_PUBLIC, "string $name, mixed $default = ?", "@mixed",` |
|     - | 4664 | `		  vm_builtin_ReflectionClass_getStaticPropertyValue },` |
|     - | 4665 | `		{ "setStaticPropertyValue", PH7_MOD_PUBLIC, "string $name, mixed $value", "@void",` |
|     - | 4666 | `		  vm_builtin_ReflectionClass_setStaticPropertyValue },` |
|     - | 4667 | `		{ "getDefaultProperties", PH7_MOD_PUBLIC, "", "@array",` |
|     - | 4668 | `		  vm_builtin_ReflectionClass_getDefaultProperties },` |
|     - | 4669 | `		{ "isIterable",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isIterable },` |
|     - | 4670 | `		{ "isIterateable",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isIterable },` |
|     - | 4671 | `		{ "implementsInterface", PH7_MOD_PUBLIC, "ReflectionClass\|string $interface", "@bool",` |
|     - | 4672 | `		  vm_builtin_ReflectionClass_implementsInterface },` |
|     - | 4673 | `		{ "getExtension",      PH7_MOD_PUBLIC, "", "@?ReflectionExtension", vm_builtin_ReflectionClass_getExtension },` |
|     - | 4674 | `		{ "getExtensionName",  PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionClass_getExtensionName },` |
|     - | 4675 | `		{ "inNamespace",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_inNamespace },` |
|     - | 4676 | `		{ "getNamespaceName",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getNamespaceName },` |
|     - | 4677 | `		{ "getShortName",      PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getShortName },` |
|     - | 4678 | `		{ "getAttributes",     PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|     - | 4679 | `		  vm_builtin_ReflectionClass_getAttributes },` |
|     - | 4680 | `	};` |
|     - | 4681 | `	static const PH7_NativeConstDef aClassConst[] = {` |
|     - | 4682 | `		{ "IS_IMPLICIT_ABSTRACT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,    0, 0.0 },` |
|     - | 4683 | `		{ "IS_EXPLICIT_ABSTRACT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64,    0, 0.0 },` |
|     - | 4684 | `		{ "IS_FINAL",             PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32,    0, 0.0 },` |
|     - | 4685 | `		{ "IS_READONLY",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 65536, 0, 0.0 },` |
|     - | 4686 | `		{ "SKIP_INITIALIZATION_ON_SERIALIZE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 8, 0, 0.0 },` |
|     - | 4687 | `		{ "SKIP_DESTRUCTOR",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,    0, 0.0 },` |
|     - | 4688 | `	};` |
|     - | 4689 | `	static const PH7_NativeMethodDef aObjectMethod[] = {` |
|     - | 4690 | `		{ "__construct", PH7_MOD_PUBLIC, "object $object", "",` |
|     - | 4691 | `		  vm_builtin_ReflectionObject_construct },` |
|     - | 4692 | `	};` |
|     - | 4693 | `	static const PH7_NativeMethodDef aReflectionMethod[] = {` |
|     - | 4694 | `		{ "getModifierNames", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "int $modifiers", "@array",` |
|     - | 4695 | `		  vm_builtin_Reflection_getModifierNames },` |
|     - | 4696 | `	};` |
|     - | 4697 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 4698 | `		{ "Reflector", "Stringable", 0, PH7_CLASS_INTERFACE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 4699 | `		{ "ReflectionException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 4700 | `		{ "Reflection", 0, 0, 0,` |
|     - | 4701 | `		  aReflectionMethod, SX_ARRAYSIZE(aReflectionMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 4702 | ``		/* Uncloneable and unserializable in php too: `clone` is an Error and`` |
|     - | 4703 | `		 * serialize() a catchable Exception naming the class. */` |
|     - | 4704 | `		{ "ReflectionClass", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 4705 | `		  aClassMethod, SX_ARRAYSIZE(aClassMethod),` |
|     - | 4706 | `		  aClassConst, SX_ARRAYSIZE(aClassConst),` |
|     - | 4707 | `		  aClassProp, SX_ARRAYSIZE(aClassProp), 0, 0, 0 },` |
|     - | 4708 | `		{ "ReflectionObject", "ReflectionClass", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 4709 | `		  aObjectMethod, SX_ARRAYSIZE(aObjectMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 4710 | `	};` |
|  5151 | 4711 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|     5 | 4712 | `}` |
|     - | 4713 | `/*` |
|     - | 4714 | ` * ---------------------------------------------------------------------------` |
|     - | 4715 | ` * ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,` |
|     - | 4716 | ` * ReflectionParameter.` |
|     - | 4717 | ` *` |
|     - | 4718 | ` * Chunk 2. Its accessors funnelled through __reflect_func_info(), a descriptor` |
|     - | 4719 | ` * ARRAY built per call from either a compiled ph7_vm_func or a declared` |
|     - | 4720 | ` * SIGNATURE STRING; a native method reads whichever of the two the target` |
|     - | 4721 | ` * actually has, through the one uniform description both already agreed on` |
|     - | 4722 | ` * (ReflectParamDesc / ReflectSigDescribe).` |
|     - | 4723 | ` *` |
|     - | 4724 | ` * Five thunks retire with the chunk: __reflect_func_info, __reflect_invoke,` |
|     - | 4725 | ` * __reflect_closure, __reflect_param_default and __reflect_param_defconst.` |
|     - | 4726 | ` * ---------------------------------------------------------------------------` |
|     - | 4727 | ` */` |
|     - | 4728 | `#define RF_CL "__cl"     /* ReflectionFunctionAbstract: the reflected Closure  */` |
|     - | 4729 | `#define RP_T  "__t"      /* ReflectionParameter: the function/class it belongs to */` |
|     - | 4730 | `#define RP_M  "__m"      /* ReflectionParameter: the method name, or null      */` |
|     - | 4731 | `#define RP_P  "__p"      /* ReflectionParameter: the position                  */` |
|     - | 4732 |  |
|     - | 4733 | `/* Everything a reflected function IS, resolved once per call. */` |
|     - | 4734 | `typedef struct ReflectFuncRef ReflectFuncRef;` |
|     - | 4735 | `struct ReflectFuncRef` |
|     - | 4736 | `{` |
|     - | 4737 | `	ph7_vm_func *pFunc;           /* compiled body (NULL for a pure C builtin) */` |
|     - | 4738 | `	ph7_user_func *pHost;         /* C builtin (NULL otherwise) */` |
|     - | 4739 | `	ph7_class *pClass;            /* class a METHOD was reached through */` |
|     - | 4740 | `	ph7_class_method *pMeth;      /* the method */` |
|     - | 4741 | `	ph7_class_instance *pClosure; /* the Closure being reflected, if any */` |
|     - | 4742 | `	const char *zSig;             /* declared parameter signature, or NULL */` |
|     - | 4743 | `	const char *zRet;             /* declared return type, or NULL */` |
|     - | 4744 | `};` |
|     - | 4745 | `/*` |
|     - | 4746 | ` * Resolve a callable into the reference, and work out WHICH of the two` |
|     - | 4747 | ` * parameter sources describes it: a declared signature string (a C builtin, a` |
|     - | 4748 | ` * native method, or an embedded-PHP builtin declared argless over` |
|     - | 4749 | ` * func_get_args()) wins over the compiled argument list, exactly as` |
|     - | 4750 | ` * ReflectSigFixup made it win in the descriptor.` |
|     - | 4751 | ` */` |
|  2564 | 4752 | `static int ReflectFuncFill(ph7_vm *pVm, ph7_value *pTarget, ph7_value *pMethodArg,` |
|     - | 4753 | `	ReflectFuncRef *pOut)` |
|     5 | 4754 | `{` |
|  2569 | 4755 | `	SyZero(pOut, sizeof(*pOut));` |
|  3851 | 4756 | `	pOut->pFunc = ReflectResolveCallable(pVm, pTarget, pMethodArg,` |
|  1282 | 4757 | `		&pOut->pClass, &pOut->pMeth, &pOut->pHost, &pOut->pClosure);` |
|  2569 | 4758 | `	if( pOut->pFunc == 0 && pOut->pHost == 0 ){` |
|    17 | 4759 | `		return 0;` |
|     - | 4760 | `	}` |
|  2555 | 4761 | `	if( pOut->pFunc == 0 ){` |
|   377 | 4762 | `		pOut->zSig = pOut->pHost->zSig;` |
|   377 | 4763 | `		pOut->zRet = pOut->pHost->zRet;` |
|   377 | 4764 | `		return 1;` |
|     - | 4765 | `	}` |
|  2181 | 4766 | `	if( (pOut->pFunc->iFlags & VM_FUNC_NATIVE) && pOut->pFunc->pNative ){` |
|   987 | 4767 | `		pOut->zSig = pOut->pFunc->pNative->zSig;` |
|   987 | 4768 | `		if( SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|   987 | 4769 | `			pOut->zRet = pOut->pFunc->pNative->zRet;` |
|   492 | 4770 | `		}` |
|  1687 | 4771 | `	}else if( (pOut->pFunc->iFlags & VM_FUNC_INTERNAL)` |
|   672 | 4772 | `	 && SySetUsed(&pOut->pFunc->aArgs) == 0 && pOut->pMeth == 0 ){` |
|   ! 0 | 4773 | `		const char *zRet = 0;` |
|   ! 0 | 4774 | `		pOut->zSig = PH7_VmBuiltinSigLookup(SyStringData(&pOut->pFunc->sName),` |
|   ! 0 | 4775 | `			SyStringLength(&pOut->pFunc->sName), &zRet);` |
|   ! 0 | 4776 | `		if( zRet && SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|   ! 0 | 4777 | `			pOut->zRet = zRet;` |
|   ! 0 | 4778 | `		}` |
|   ! 0 | 4779 | `	}` |
|  2181 | 4780 | `	if( pOut->zSig && pOut->zSig[0] == '\0' ){` |
|     - | 4781 | `		/* A declared-EMPTY signature really is "no parameters", not "undescribed". */` |
|   414 | 4782 | `		return 1;` |
|     - | 4783 | `	}` |
|  1769 | 4784 | `	return 1;` |
|  1287 | 4785 | `}` |
|     - | 4786 | `/* Is this receiver a ReflectionMethod (rather than a ReflectionFunction)? */` |
|  1490 | 4787 | `static int ReflectIsMethodReflector(ph7_context *pCtx, ph7_class_instance *pThis)` |
|     5 | 4788 | `{` |
|     - | 4789 | `	ph7_class *pRM;` |
|  1495 | 4790 | `	if( pThis == 0 ){` |
|   ! 0 | 4791 | `		return 0;` |
|     - | 4792 | `	}` |
|  1495 | 4793 | `	pRM = PH7_VmExtractClass(pCtx->pVm, "ReflectionMethod", sizeof("ReflectionMethod")-1, FALSE, 0);` |
|  1495 | 4794 | `	return pRM != 0 && PH7_VmInstanceOf(pThis->pClass, pRM);` |
|   750 | 4795 | `}` |
|     - | 4796 | `/*` |
|     - | 4797 | `` * The function `$this` reflects. A ReflectionFunction built over a Closure keeps`` |
|     - | 4798 | `` * the object in `__cl` and resolves through it (its `$__fn` is a lambda name no`` |
|     - | 4799 | ` * function table holds); a ReflectionMethod resolves ($this->class, $this->name).` |
|     - | 4800 | ` */` |
|  1300 | 4801 | `static int ReflectFuncOfThis(ph7_context *pCtx, ReflectFuncRef *pOut)` |
|     5 | 4802 | `{` |
|  1305 | 4803 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 4804 | `	ph7_class_instance *pClo;` |
|     - | 4805 | `	ph7_value sTarget, sMethod;` |
|     - | 4806 | `	const char *zName, *zClass;` |
|     - | 4807 | `	int nName, nClass, rc;` |
|  1305 | 4808 | `	SyZero(pOut, sizeof(*pOut));` |
|  1305 | 4809 | `	if( pThis == 0 ){` |
|   ! 0 | 4810 | `		return 0;` |
|     - | 4811 | `	}` |
|  1305 | 4812 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|  1305 | 4813 | `	PH7_MemObjInit(pCtx->pVm, &sMethod);` |
|  1305 | 4814 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|  1305 | 4815 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|  1305 | 4816 | `	if( pClo ){` |
|    89 | 4817 | `		sTarget.x.pOther = pClo;` |
|    89 | 4818 | `		sTarget.iFlags = MEMOBJ_OBJ;` |
|  1262 | 4819 | `	}else if( ReflectIsMethodReflector(pCtx, pThis) ){` |
|   909 | 4820 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|   909 | 4821 | `		ph7_value_string(&sTarget, zClass, nClass);` |
|   909 | 4822 | `		ph7_value_string(&sMethod, zName, nName);` |
|   457 | 4823 | `	}else{` |
|   313 | 4824 | `		ph7_value_string(&sTarget, zName, nName);` |
|     - | 4825 | `	}` |
|  1305 | 4826 | `	rc = ReflectFuncFill(pCtx->pVm, &sTarget, (sMethod.iFlags & MEMOBJ_STRING) ? &sMethod : 0, pOut);` |
|     - | 4827 | `	/* sTarget only ever BORROWS the closure; releasing it would unref twice. */` |
|  1305 | 4828 | `	if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|  1219 | 4829 | `		PH7_MemObjRelease(&sTarget);` |
|   607 | 4830 | `	}` |
|  1305 | 4831 | `	PH7_MemObjRelease(&sMethod);` |
|  1305 | 4832 | `	return rc;` |
|   655 | 4833 | `}` |
|     - | 4834 | `/* How many parameters the target declares. */` |
|   952 | 4835 | `static int ReflectParamCount(const ReflectFuncRef *pRef)` |
|     5 | 4836 | `{` |
|   957 | 4837 | `	if( pRef->zSig ){` |
|   470 | 4838 | `		return ReflectSigPart(pRef->zSig, (int)SyStrlen(pRef->zSig), -1, 0, 0);` |
|     - | 4839 | `	}` |
|   488 | 4840 | `	return pRef->pFunc ? (int)SySetUsed(&pRef->pFunc->aArgs) : 0;` |
|   481 | 4841 | `}` |
|     - | 4842 | `/* Describe the iPos-th parameter. Answers 0 when there is none. */` |
|  1216 | 4843 | `static int ReflectParamAt(const ReflectFuncRef *pRef, int iPos, ReflectParamDesc *pOut)` |
|     4 | 4844 | `{` |
|  1220 | 4845 | `	SyZero(pOut, sizeof(*pOut));` |
|  1220 | 4846 | `	if( iPos < 0 ){` |
|   ! 0 | 4847 | `		return 0;` |
|     - | 4848 | `	}` |
|  1220 | 4849 | `	if( pRef->zSig ){` |
|   527 | 4850 | `		const char *zPart = 0;` |
|   527 | 4851 | `		int nPart = 0, nTotal;` |
|   527 | 4852 | `		nTotal = ReflectSigPart(pRef->zSig, (int)SyStrlen(pRef->zSig), iPos, &zPart, &nPart);` |
|   527 | 4853 | `		if( iPos >= nTotal \|\| zPart == 0 ){` |
|     3 | 4854 | `			return 0;` |
|     - | 4855 | `		}` |
|   525 | 4856 | `		ReflectSigDescribe(zPart, nPart, iPos, pOut);` |
|   525 | 4857 | `		return 1;` |
|     - | 4858 | `	}` |
|   695 | 4859 | `	if( pRef->pFunc == 0 ){` |
|   ! 0 | 4860 | `		return 0;` |
|     - | 4861 | `	}` |
|     - | 4862 | `	{` |
|   695 | 4863 | `		ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pRef->pFunc->aArgs, (sxu32)iPos);` |
|   695 | 4864 | `		if( pArg == 0 ){` |
|    13 | 4865 | `			return 0;` |
|     - | 4866 | `		}` |
|   683 | 4867 | `		pOut->iPos = iPos;` |
|   683 | 4868 | `		pOut->sName = pArg->sName;` |
|   683 | 4869 | `		pOut->bByRef = (pArg->iFlags & VM_FUNC_ARG_BY_REF) != 0;` |
|   683 | 4870 | `		pOut->bVariadic = (pArg->iFlags & VM_FUNC_ARG_VARIADIC) != 0;` |
|     - | 4871 | `		/* The compiler never sets ARG_HAS_DEF; a default IS compiled byte-code` |
|     - | 4872 | `		 * (the same test the OP_CALL default-value path uses). */` |
|   683 | 4873 | `		pOut->bHasDef = SySetUsed(&pArg->aByteCode) > 0;` |
|   683 | 4874 | `		pOut->bNullable = (pArg->iFlags & VM_FUNC_ARG_NULLABLE) != 0;` |
|   683 | 4875 | `		pOut->bPromoted = (pArg->iFlags & VM_FUNC_ARG_PROMOTED) != 0;` |
|   683 | 4876 | `		pOut->bOptional = pOut->bVariadic \|\| pOut->bHasDef;` |
|   683 | 4877 | `		pOut->sType = pArg->sTypeName;` |
|   683 | 4878 | `		pOut->pArg = pArg;` |
|   683 | 4879 | `		return 1;` |
|     - | 4880 | `	}` |
|   612 | 4881 | `}` |
|     - | 4882 | `/* The declaring class php reports for a reflected METHOD. */` |
|   342 | 4883 | `static ph7_class * ReflectFuncDeclClass(const ReflectFuncRef *pRef)` |
|     4 | 4884 | `{` |
|   346 | 4885 | `	if( pRef->pMeth == 0 \|\| pRef->pClass == 0 ){` |
|     3 | 4886 | `		return 0;` |
|     - | 4887 | `	}` |
|   344 | 4888 | `	return ReflectMethodDeclClass(pRef->pClass, pRef->pMeth);` |
|   175 | 4889 | `}` |
|     - | 4890 | `/*` |
|     - | 4891 | ` * The declared return type TEXT, or 0 — and whether php calls it TENTATIVE.` |
|     - | 4892 | ` *` |
|     - | 4893 | ` * php's stubs carry two kinds of internal return type and Reflection answers` |
|     - | 4894 | ` * differently for each: a real one is reported by getReturnType()/hasReturnType()` |
|     - | 4895 | `` * and printed `Return [ T ]`, while an `@tentative-return-type` answers NULL and`` |
|     - | 4896 | ` * false from those two, is reported by getTentativeReturnType()/` |
|     - | 4897 | `` * hasTentativeReturnType(), and prints `Tentative return [ T ]`. Nearly every`` |
|     - | 4898 | ` * internal SPL, date and Reflection method is the tentative kind.` |
|     - | 4899 | ` *` |
|     - | 4900 | `` * PHL has one field for both, so a leading `@` on a `zRet` — the same character`` |
|     - | 4901 | ` * php's stub annotation uses — marks the tentative kind. It is stripped here, so` |
|     - | 4902 | ` * nothing downstream of this function ever sees it.` |
|     - | 4903 | ` */` |
|   442 | 4904 | `static int ReflectFuncRetTextEx(const ReflectFuncRef *pRef, const char **pz, int *pn,` |
|     - | 4905 | `	int *pbTentative)` |
|     4 | 4906 | `{` |
|   446 | 4907 | `	if( pbTentative ){` |
|   446 | 4908 | `		*pbTentative = 0;` |
|   221 | 4909 | `	}` |
|   446 | 4910 | `	if( pRef->zRet && pRef->zRet[0] ){` |
|   345 | 4911 | `		const char *z = pRef->zRet;` |
|   345 | 4912 | `		if( z[0] == '@' ){` |
|   207 | 4913 | `			if( pbTentative ){` |
|   207 | 4914 | `				*pbTentative = 1;` |
|   103 | 4915 | `			}` |
|   207 | 4916 | `			z++;` |
|   103 | 4917 | `		}` |
|   345 | 4918 | `		*pz = z;` |
|   345 | 4919 | `		*pn = (int)SyStrlen(z);` |
|   345 | 4920 | `		return 1;` |
|     - | 4921 | `	}` |
|   103 | 4922 | `	if( pRef->pFunc == 0 ){` |
|   ! 0 | 4923 | `		return 0;` |
|     - | 4924 | `	}` |
|   103 | 4925 | `	if( SyStringLength(&pRef->pFunc->sReturnTypeName) > 0 ){` |
|    53 | 4926 | `		*pz = SyStringData(&pRef->pFunc->sReturnTypeName);` |
|    53 | 4927 | `		*pn = (int)SyStringLength(&pRef->pFunc->sReturnTypeName);` |
|    53 | 4928 | `		return 1;` |
|     - | 4929 | `	}` |
|     - | 4930 | `	/* The type-text renderer omits void/never atoms (compile.c notes the root fix` |
|     - | 4931 | `	 * belongs there); name them here for getReturnType(). */` |
|    51 | 4932 | `	if( pRef->pFunc->nReturnType == MEMOBJ_VOID ){` |
|    11 | 4933 | `		*pz = "void";` |
|    11 | 4934 | `		*pn = sizeof("void")-1;` |
|    11 | 4935 | `		return 1;` |
|     - | 4936 | `	}` |
|    41 | 4937 | `	if( pRef->pFunc->nReturnType == MEMOBJ_NEVER ){` |
|     3 | 4938 | `		*pz = "never";` |
|     3 | 4939 | `		*pn = sizeof("never")-1;` |
|     3 | 4940 | `		return 1;` |
|     - | 4941 | `	}` |
|    39 | 4942 | `	return 0;` |
|   225 | 4943 | `}` |
|     - | 4944 | `/* The declared return type php would report from getReturnType(): a TENTATIVE one` |
|     - | 4945 | ` * is not reported there at all, which is the whole distinction. */` |
|   200 | 4946 | `static int ReflectFuncRetText(const ReflectFuncRef *pRef, const char **pz, int *pn)` |
|     3 | 4947 | `{` |
|   203 | 4948 | `	int bTentative = 0;` |
|   203 | 4949 | `	if( !ReflectFuncRetTextEx(pRef, pz, pn, &bTentative) ){` |
|     9 | 4950 | `		return 0;` |
|     - | 4951 | `	}` |
|   195 | 4952 | `	return bTentative ? 0 : 1;` |
|   103 | 4953 | `}` |
|     - | 4954 | `/* Is the reflected function internal (a C builtin or an embedded-chunk one)? */` |
|   192 | 4955 | `static int ReflectFuncIsInternal(const ReflectFuncRef *pRef)` |
|     2 | 4956 | `{` |
|   194 | 4957 | `	if( pRef->pHost ){` |
|    30 | 4958 | `		return 1;` |
|     - | 4959 | `	}` |
|   165 | 4960 | `	return pRef->pFunc != 0 && (pRef->pFunc->iFlags & VM_FUNC_INTERNAL) != 0;` |
|    98 | 4961 | `}` |
|     - | 4962 | `/* Does this target carry a #[\Deprecated] attribute? */` |
|     8 | 4963 | `static int ReflectHasDeprecated(SySet *pAttrs)` |
|     1 | 4964 | `{` |
|     9 | 4965 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|     - | 4966 | `	sxu32 n;` |
|     9 | 4967 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|     6 | 4968 | `		if( SyStringLength(&aA[n].sName) == sizeof("deprecated")-1` |
|     7 | 4969 | `		 && SyStrnicmp(SyStringData(&aA[n].sName), "deprecated", sizeof("deprecated")-1) == 0 ){` |
|     7 | 4970 | `			return 1;` |
|     - | 4971 | `		}` |
|   ! 0 | 4972 | `	}` |
|     3 | 4973 | `	return 0;` |
|     5 | 4974 | `}` |
|     - | 4975 | ``/* Resolve `$this`, or answer a default when the target has gone missing. */`` |
|     - | 4976 | `#define REFLECT_FUNC_OR(REF,STMT) \` |
|     - | 4977 | `	if( !ReflectFuncOfThis(pCtx, &(REF)) ){ STMT; return PH7_OK; }` |
|     - | 4978 |  |
|     - | 4979 | `/* ---- ReflectionFunctionAbstract ---- */` |
|   ! 0 | 4980 | `static int vm_builtin_ReflectionFunc_clone(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 4981 | `{` |
|   ! 0 | 4982 | `	SXUNUSED(pCtx);` |
|   ! 0 | 4983 | `	SXUNUSED(nArg);` |
|   ! 0 | 4984 | `	SXUNUSED(apArg);` |
|   ! 0 | 4985 | `	return PH7_OK;` |
|   ! 0 | 4986 | `}` |
|   170 | 4987 | `static int vm_builtin_ReflectionFunc_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     4 | 4988 | `{` |
|   174 | 4989 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   174 | 4990 | `	const char *zName = "";` |
|   174 | 4991 | `	int nName = 0;` |
|    85 | 4992 | `	SXUNUSED(nArg);` |
|    85 | 4993 | `	SXUNUSED(apArg);` |
|   174 | 4994 | `	if( pThis ){` |
|   174 | 4995 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    85 | 4996 | `	}` |
|   174 | 4997 | `	ph7_result_string(pCtx, zName, nName);` |
|   174 | 4998 | `	return PH7_OK;` |
|     4 | 4999 | `}` |
|     - | 5000 | ``/* The `name` slot's namespace split — the same three answers ReflectionClass gives. */`` |
|   ! 0 | 5001 | `static int ReflectFuncNamePart(ph7_context *pCtx, int iWhat)` |
|   ! 0 | 5002 | `{` |
|   ! 0 | 5003 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   ! 0 | 5004 | `	const char *zName = "";` |
|   ! 0 | 5005 | `	int nName = 0, iCut;` |
|   ! 0 | 5006 | `	if( pThis ){` |
|   ! 0 | 5007 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|   ! 0 | 5008 | `	}` |
|   ! 0 | 5009 | `	iCut = ReflectNsCut(zName, nName);` |
|   ! 0 | 5010 | `	if( iWhat == 0 ){` |
|   ! 0 | 5011 | `		ph7_result_bool(pCtx, iCut >= 0);` |
|   ! 0 | 5012 | `	}else if( iWhat == 1 ){` |
|   ! 0 | 5013 | `		ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);` |
|   ! 0 | 5014 | `	}else if( iCut < 0 ){` |
|   ! 0 | 5015 | `		ph7_result_string(pCtx, zName, nName);` |
|   ! 0 | 5016 | `	}else{` |
|   ! 0 | 5017 | `		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);` |
|     - | 5018 | `	}` |
|   ! 0 | 5019 | `	return PH7_OK;` |
|   ! 0 | 5020 | `}` |
|     - | 5021 | `#define REFLECT_FUNC_NAMEPART(NAME,WHAT) \` |
|     - | 5022 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|     - | 5023 | `	{ \` |
|     - | 5024 | `		SXUNUSED(nArg); \` |
|     - | 5025 | `		SXUNUSED(apArg); \` |
|     - | 5026 | `		return ReflectFuncNamePart(pCtx, WHAT); \` |
|     - | 5027 | `	}` |
|   ! 0 | 5028 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_inNamespace, 0)` |
|   ! 0 | 5029 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_getNamespaceName, 1)` |
|   ! 0 | 5030 | `REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_getShortName, 2)` |
|     - | 5031 |  |
|    14 | 5032 | `static int vm_builtin_ReflectionFunc_isClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5033 | `{` |
|     - | 5034 | `	ReflectFuncRef sRef;` |
|    15 | 5035 | `	int bAnon = 0;` |
|     7 | 5036 | `	SXUNUSED(nArg);` |
|     7 | 5037 | `	SXUNUSED(apArg);` |
|    15 | 5038 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    15 | 5039 | `	if( sRef.pFunc ){` |
|     - | 5040 | ``		/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|     - | 5041 | `		 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|    15 | 5042 | `		bAnon = (sRef.pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|    14 | 5043 | `		if( !bAnon && SyStringLength(&sRef.pFunc->sName) > 9` |
|     3 | 5044 | `		 && (SyMemcmp(SyStringData(&sRef.pFunc->sName), "[lambda_", 8) == 0` |
|   ! 0 | 5045 | `		  \|\| SyMemcmp(SyStringData(&sRef.pFunc->sName), "[closure_", 9) == 0) ){` |
|   ! 0 | 5046 | `			bAnon = 1;` |
|   ! 0 | 5047 | `		}` |
|     7 | 5048 | `	}` |
|    15 | 5049 | `	ph7_result_bool(pCtx, bAnon);` |
|    15 | 5050 | `	return PH7_OK;` |
|     8 | 5051 | `}` |
|     8 | 5052 | `static int vm_builtin_ReflectionFunc_isDeprecated(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5053 | `{` |
|     - | 5054 | `	ReflectFuncRef sRef;` |
|     4 | 5055 | `	SXUNUSED(nArg);` |
|     4 | 5056 | `	SXUNUSED(apArg);` |
|     9 | 5057 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     9 | 5058 | `	ph7_result_bool(pCtx, sRef.pFunc != 0 && ReflectHasDeprecated(&sRef.pFunc->aAttrs));` |
|     9 | 5059 | `	return PH7_OK;` |
|     5 | 5060 | `}` |
|    36 | 5061 | `static int vm_builtin_ReflectionFunc_isInternal(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5062 | `{` |
|     - | 5063 | `	ReflectFuncRef sRef;` |
|    18 | 5064 | `	SXUNUSED(nArg);` |
|    18 | 5065 | `	SXUNUSED(apArg);` |
|    37 | 5066 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    37 | 5067 | `	ph7_result_bool(pCtx, ReflectFuncIsInternal(&sRef));` |
|    37 | 5068 | `	return PH7_OK;` |
|    19 | 5069 | `}` |
|   ! 0 | 5070 | `static int vm_builtin_ReflectionFunc_isUserDefined(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 5071 | `{` |
|     - | 5072 | `	ReflectFuncRef sRef;` |
|   ! 0 | 5073 | `	SXUNUSED(nArg);` |
|   ! 0 | 5074 | `	SXUNUSED(apArg);` |
|   ! 0 | 5075 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|   ! 0 | 5076 | `	ph7_result_bool(pCtx, !ReflectFuncIsInternal(&sRef));` |
|   ! 0 | 5077 | `	return PH7_OK;` |
|   ! 0 | 5078 | `}` |
|     4 | 5079 | `static int vm_builtin_ReflectionFunc_isGenerator(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5080 | `{` |
|     - | 5081 | `	ReflectFuncRef sRef;` |
|     2 | 5082 | `	SXUNUSED(nArg);` |
|     2 | 5083 | `	SXUNUSED(apArg);` |
|     5 | 5084 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     5 | 5085 | `	ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|     5 | 5086 | `	return PH7_OK;` |
|     3 | 5087 | `}` |
|    10 | 5088 | `static int vm_builtin_ReflectionFunc_isVariadic(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5089 | `{` |
|     - | 5090 | `	ReflectFuncRef sRef;` |
|     - | 5091 | `	ReflectParamDesc sDesc;` |
|    11 | 5092 | `	int n, nTotal, bVariadic = 0;` |
|     5 | 5093 | `	SXUNUSED(nArg);` |
|     5 | 5094 | `	SXUNUSED(apArg);` |
|    11 | 5095 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    11 | 5096 | `	nTotal = ReflectParamCount(&sRef);` |
|    31 | 5097 | `	for( n = 0 ; n < nTotal ; n++ ){` |
|    29 | 5098 | `		if( ReflectParamAt(&sRef, n, &sDesc) && sDesc.bVariadic ){` |
|     9 | 5099 | `			bVariadic = 1;` |
|     9 | 5100 | `			break;` |
|     - | 5101 | `		}` |
|    11 | 5102 | `	}` |
|    11 | 5103 | `	ph7_result_bool(pCtx, bVariadic);` |
|    11 | 5104 | `	return PH7_OK;` |
|     6 | 5105 | `}` |
|     - | 5106 | ``/* isStatic(): a METHOD's own staticness, a closure's `static function () {}`. */`` |
|    76 | 5107 | `static int vm_builtin_ReflectionFunc_isStatic(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 5108 | `{` |
|     - | 5109 | `	ReflectFuncRef sRef;` |
|    38 | 5110 | `	SXUNUSED(nArg);` |
|    38 | 5111 | `	SXUNUSED(apArg);` |
|    78 | 5112 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    78 | 5113 | `	if( sRef.pMeth ){` |
|    68 | 5114 | `		ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|    35 | 5115 | `	}else{` |
|    11 | 5116 | `		ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);` |
|     - | 5117 | `	}` |
|    78 | 5118 | `	return PH7_OK;` |
|    40 | 5119 | `}` |
|     6 | 5120 | `static int vm_builtin_ReflectionFunc_returnsReference(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5121 | `{` |
|     - | 5122 | `	ReflectFuncRef sRef;` |
|     3 | 5123 | `	SXUNUSED(nArg);` |
|     3 | 5124 | `	SXUNUSED(apArg);` |
|     7 | 5125 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     7 | 5126 | `	ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|     7 | 5127 | `	return PH7_OK;` |
|     4 | 5128 | `}` |
|     - | 5129 | `/* The Closure instance whose captured state the three getClosure* read. */` |
|    36 | 5130 | `static ph7_value * ReflectClosureAttr(ReflectFuncRef *pRef, const char *zName)` |
|     1 | 5131 | `{` |
|     - | 5132 | `	SyString sAttr;` |
|    37 | 5133 | `	if( pRef->pClosure == 0 ){` |
|   ! 0 | 5134 | `		return 0;` |
|     - | 5135 | `	}` |
|    37 | 5136 | `	SyStringInitFromBuf(&sAttr, zName, SyStrlen(zName));` |
|    37 | 5137 | `	return PH7_ClassInstanceFetchAttr(pRef->pClosure, &sAttr);` |
|    19 | 5138 | `}` |
|     4 | 5139 | `static int vm_builtin_ReflectionFunc_getClosureThis(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5140 | `{` |
|     - | 5141 | `	ReflectFuncRef sRef;` |
|     - | 5142 | `	ph7_value *pAttr;` |
|     2 | 5143 | `	SXUNUSED(nArg);` |
|     2 | 5144 | `	SXUNUSED(apArg);` |
|     5 | 5145 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|     5 | 5146 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|     5 | 5147 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|     3 | 5148 | `		ph7_result_value(pCtx, pAttr);` |
|     2 | 5149 | `	}else{` |
|     3 | 5150 | `		ph7_result_null(pCtx);` |
|     - | 5151 | `	}` |
|     5 | 5152 | `	return PH7_OK;` |
|     3 | 5153 | `}` |
|    18 | 5154 | `static int vm_builtin_ReflectionFunc_getClosureScopeClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5155 | `{` |
|     - | 5156 | `	ReflectFuncRef sRef;` |
|     - | 5157 | `	ph7_value *pAttr;` |
|     9 | 5158 | `	SXUNUSED(nArg);` |
|     9 | 5159 | `	SXUNUSED(apArg);` |
|    19 | 5160 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|    19 | 5161 | `	pAttr = ReflectClosureAttr(&sRef, "__scope");` |
|    19 | 5162 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|     - | 5163 | `		/* php reports the class that DECLARED the callee, not the one the callable NAMED:` |
|     - | 5164 | ``		 * `$__scope` is the CALLED scope (what `static::` answers, reported by`` |
|     - | 5165 | `		 * getClosureCalledClass below), and for an inherited or trait-composed method the` |
|     - | 5166 | `		 * two differ. PH7_VmClosureScopeClass is the one rule. */` |
|    22 | 5167 | `		return ReflectResultClassOf(pCtx,` |
|     7 | 5168 | `			PH7_VmClosureScopeClass(pCtx->pVm, sRef.pClosure));` |
|     - | 5169 | `	}` |
|     5 | 5170 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|     5 | 5171 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|   ! 0 | 5172 | `		return ReflectResultClassOf(pCtx, ((ph7_class_instance *)pAttr->x.pOther)->pClass);` |
|     - | 5173 | `	}` |
|     5 | 5174 | `	ph7_result_null(pCtx);` |
|     5 | 5175 | `	return PH7_OK;` |
|    10 | 5176 | `}` |
|     - | 5177 | `/*` |
|     - | 5178 | ` * ReflectionFunction::getClosureCalledClass() — php's CALLED scope, the other half of the` |
|     - | 5179 | `` * pair above: the class the call goes THROUGH, which is what `static::` answers. A bound`` |
|     - | 5180 | `` * closure's is its `$this`'s class; a static callable's is the class the callable NAMED. It`` |
|     - | 5181 | `` * shared getClosureScopeClass()'s body while `$__scope` answered both questions; it cannot`` |
|     - | 5182 | ` * now, because that one narrows a method closure to its DECLARING class.` |
|     - | 5183 | ` */` |
|     8 | 5184 | `static int vm_builtin_ReflectionFunc_getClosureCalledClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5185 | `{` |
|     - | 5186 | `	ReflectFuncRef sRef;` |
|     - | 5187 | `	ph7_value *pAttr;` |
|     4 | 5188 | `	SXUNUSED(nArg);` |
|     4 | 5189 | `	SXUNUSED(apArg);` |
|     9 | 5190 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|     9 | 5191 | `	pAttr = ReflectClosureAttr(&sRef, "__this");` |
|     9 | 5192 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|     7 | 5193 | `		return ReflectResultClassOf(pCtx, ((ph7_class_instance *)pAttr->x.pOther)->pClass);` |
|     - | 5194 | `	}` |
|     3 | 5195 | `	pAttr = ReflectClosureAttr(&sRef, "__scope");` |
|     3 | 5196 | `	if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|     4 | 5197 | `		return ReflectResultClassOf(pCtx, PH7_VmExtractClass(pCtx->pVm,` |
|     2 | 5198 | `			(const char *)SyBlobData(&pAttr->sBlob), SyBlobLength(&pAttr->sBlob), FALSE, 0));` |
|     - | 5199 | `	}` |
|   ! 0 | 5200 | `	ph7_result_null(pCtx);` |
|   ! 0 | 5201 | `	return PH7_OK;` |
|     5 | 5202 | `}` |
|    10 | 5203 | `static int vm_builtin_ReflectionFunc_getClosureUsedVariables(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 5204 | `{` |
|     - | 5205 | `	ReflectFuncRef sRef;` |
|    12 | 5206 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     - | 5207 | `	ph7_vm_func_closure_env *aEnv;` |
|     - | 5208 | `	sxu32 n;` |
|     5 | 5209 | `	SXUNUSED(nArg);` |
|     5 | 5210 | `	SXUNUSED(apArg);` |
|    12 | 5211 | `	if( pOut == 0 ){` |
|   ! 0 | 5212 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 5213 | `	}` |
|    12 | 5214 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pClosure == 0 \|\| sRef.pFunc == 0 ){` |
|   ! 0 | 5215 | `		ph7_result_value(pCtx, pOut);` |
|   ! 0 | 5216 | `		return PH7_OK;` |
|     - | 5217 | `	}` |
|     - | 5218 | `	/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */` |
|    12 | 5219 | `	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|    28 | 5220 | `	for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; n++ ){` |
|    18 | 5221 | `		if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){` |
|    12 | 5222 | `			continue;` |
|     - | 5223 | `		}` |
|     6 | 5224 | `		if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|     4 | 5225 | `		 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|   ! 0 | 5226 | `			continue;` |
|     - | 5227 | `		}` |
|     7 | 5228 | `		if( (aEnv[n].iFlags & VM_FUNC_ARG_BY_REF) && aEnv[n].nIdx != SXU32_HIGH ){` |
|     - | 5229 | `			/* Captured by reference: report the slot's live value */` |
|     3 | 5230 | `			ph7_value *pLive = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aEnv[n].nIdx);` |
|     3 | 5231 | `			ReflectMapAddDyn(pCtx, pOut, &aEnv[n].sName, pLive ? pLive : &aEnv[n].sValue);` |
|     3 | 5232 | `			continue;` |
|     - | 5233 | `		}` |
|     5 | 5234 | `		ReflectMapAddDyn(pCtx, pOut, &aEnv[n].sName, &aEnv[n].sValue);` |
|     3 | 5235 | `	}` |
|    12 | 5236 | `	ph7_result_value(pCtx, pOut);` |
|    12 | 5237 | `	return PH7_OK;` |
|     7 | 5238 | `}` |
|    14 | 5239 | `static int vm_builtin_ReflectionFunc_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5240 | `{` |
|     - | 5241 | `	ReflectFuncRef sRef;` |
|     7 | 5242 | `	SXUNUSED(nArg);` |
|     7 | 5243 | `	SXUNUSED(apArg);` |
|    15 | 5244 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    15 | 5245 | `	if( sRef.pFunc && SyStringLength(&sRef.pFunc->sDoc) > 0 ){` |
|    11 | 5246 | `		ph7_result_string(pCtx, SyStringData(&sRef.pFunc->sDoc), (int)SyStringLength(&sRef.pFunc->sDoc));` |
|     6 | 5247 | `	}else{` |
|     5 | 5248 | `		ph7_result_bool(pCtx, 0);` |
|     - | 5249 | `	}` |
|    15 | 5250 | `	return PH7_OK;` |
|     8 | 5251 | `}` |
|    22 | 5252 | `static int vm_builtin_ReflectionFunc_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5253 | `{` |
|     - | 5254 | `	ReflectFuncRef sRef;` |
|    11 | 5255 | `	SXUNUSED(nArg);` |
|    11 | 5256 | `	SXUNUSED(apArg);` |
|    23 | 5257 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    23 | 5258 | `	if( sRef.pFunc && SyStringLength(&sRef.pFunc->sFile) > 0 ){` |
|     3 | 5259 | `		ph7_result_string(pCtx, SyStringData(&sRef.pFunc->sFile), (int)SyStringLength(&sRef.pFunc->sFile));` |
|     2 | 5260 | `	}else{` |
|    21 | 5261 | `		ph7_result_bool(pCtx, 0);` |
|     - | 5262 | `	}` |
|    23 | 5263 | `	return PH7_OK;` |
|    12 | 5264 | `}` |
|    22 | 5265 | `static int ReflectFuncLine(ph7_context *pCtx, int bEnd)` |
|     1 | 5266 | `{` |
|     - | 5267 | `	ReflectFuncRef sRef;` |
|    23 | 5268 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| ReflectFuncIsInternal(&sRef) \|\| sRef.pFunc == 0 ){` |
|    17 | 5269 | `		ph7_result_bool(pCtx, 0);` |
|    17 | 5270 | `		return PH7_OK;` |
|     - | 5271 | `	}` |
|     7 | 5272 | `	ph7_result_int64(pCtx, (sxi64)(bEnd ? sRef.pFunc->nEndLine : sRef.pFunc->nLine));` |
|     7 | 5273 | `	return PH7_OK;` |
|    12 | 5274 | `}` |
|    20 | 5275 | `static int vm_builtin_ReflectionFunc_getStartLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5276 | `{` |
|    10 | 5277 | `	SXUNUSED(nArg);` |
|    10 | 5278 | `	SXUNUSED(apArg);` |
|    21 | 5279 | `	return ReflectFuncLine(pCtx, 0);` |
|     1 | 5280 | `}` |
|     2 | 5281 | `static int vm_builtin_ReflectionFunc_getEndLine(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5282 | `{` |
|     1 | 5283 | `	SXUNUSED(nArg);` |
|     1 | 5284 | `	SXUNUSED(apArg);` |
|     3 | 5285 | `	return ReflectFuncLine(pCtx, 1);` |
|     1 | 5286 | `}` |
|    92 | 5287 | `static int vm_builtin_ReflectionFunc_hasReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 5288 | `{` |
|     - | 5289 | `	ReflectFuncRef sRef;` |
|     - | 5290 | `	const char *z;` |
|     - | 5291 | `	int n;` |
|    46 | 5292 | `	SXUNUSED(nArg);` |
|    46 | 5293 | `	SXUNUSED(apArg);` |
|    94 | 5294 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    94 | 5295 | `	ph7_result_bool(pCtx, ReflectFuncRetText(&sRef, &z, &n));` |
|    94 | 5296 | `	return PH7_OK;` |
|    48 | 5297 | `}` |
|   108 | 5298 | `static int vm_builtin_ReflectionFunc_getReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 5299 | `{` |
|     - | 5300 | `	ReflectFuncRef sRef;` |
|     - | 5301 | `	const char *z;` |
|     - | 5302 | `	int n;` |
|    54 | 5303 | `	SXUNUSED(nArg);` |
|    54 | 5304 | `	SXUNUSED(apArg);` |
|   111 | 5305 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|   111 | 5306 | `	if( !ReflectFuncRetText(&sRef, &z, &n) ){` |
|    11 | 5307 | `		ph7_result_null(pCtx);` |
|    11 | 5308 | `		return PH7_OK;` |
|     - | 5309 | `	}` |
|   101 | 5310 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx, z, n));` |
|    57 | 5311 | `}` |
|     - | 5312 | ``/* A native method's `@`-marked zRet is php's `@tentative-return-type`: reported`` |
|     - | 5313 | ` * HERE and by nothing else, which is what separates it from a real one. */` |
|    64 | 5314 | `static int vm_builtin_ReflectionFunc_hasTentativeReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5315 | `{` |
|     - | 5316 | `	ReflectFuncRef sRef;` |
|     - | 5317 | `	const char *z;` |
|    65 | 5318 | `	int n, bTentative = 0;` |
|    32 | 5319 | `	SXUNUSED(nArg);` |
|    32 | 5320 | `	SXUNUSED(apArg);` |
|    65 | 5321 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|    65 | 5322 | `	ph7_result_bool(pCtx, ReflectFuncRetTextEx(&sRef, &z, &n, &bTentative) && bTentative);` |
|    65 | 5323 | `	return PH7_OK;` |
|    33 | 5324 | `}` |
|    50 | 5325 | `static int vm_builtin_ReflectionFunc_getTentativeReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5326 | `{` |
|     - | 5327 | `	ReflectFuncRef sRef;` |
|     - | 5328 | `	const char *z;` |
|    51 | 5329 | `	int n, bTentative = 0;` |
|    25 | 5330 | `	SXUNUSED(nArg);` |
|    25 | 5331 | `	SXUNUSED(apArg);` |
|    51 | 5332 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|    51 | 5333 | `	if( !ReflectFuncRetTextEx(&sRef, &z, &n, &bTentative) \|\| !bTentative ){` |
|   ! 0 | 5334 | `		ph7_result_null(pCtx);` |
|   ! 0 | 5335 | `		return PH7_OK;` |
|     - | 5336 | `	}` |
|    51 | 5337 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx, z, n));` |
|    26 | 5338 | `}` |
|    50 | 5339 | `static int vm_builtin_ReflectionFunc_getNumberOfParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 5340 | `{` |
|     - | 5341 | `	ReflectFuncRef sRef;` |
|    25 | 5342 | `	SXUNUSED(nArg);` |
|    25 | 5343 | `	SXUNUSED(apArg);` |
|    52 | 5344 | `	REFLECT_FUNC_OR(sRef, ph7_result_int(pCtx, 0))` |
|    52 | 5345 | `	if( sRef.pHost && sRef.zSig == 0 ){` |
|     - | 5346 | `		/* An UNDESCRIBED builtin: the arity table is all there is. */` |
|   ! 0 | 5347 | `		ph7_result_int64(pCtx, (sxi64)sRef.pHost->nMinArg);` |
|   ! 0 | 5348 | `		return PH7_OK;` |
|     - | 5349 | `	}` |
|    52 | 5350 | `	ph7_result_int(pCtx, ReflectParamCount(&sRef));` |
|    52 | 5351 | `	return PH7_OK;` |
|    27 | 5352 | `}` |
|    24 | 5353 | `static int vm_builtin_ReflectionFunc_getNumberOfRequiredParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5354 | `{` |
|     - | 5355 | `	ReflectFuncRef sRef;` |
|     - | 5356 | `	ReflectParamDesc sDesc;` |
|    25 | 5357 | `	int n, nTotal, nReq = 0;` |
|    12 | 5358 | `	SXUNUSED(nArg);` |
|    12 | 5359 | `	SXUNUSED(apArg);` |
|    25 | 5360 | `	REFLECT_FUNC_OR(sRef, ph7_result_int(pCtx, 0))` |
|    25 | 5361 | `	if( sRef.pHost && sRef.zSig == 0 ){` |
|   ! 0 | 5362 | `		ph7_result_int64(pCtx, (sxi64)sRef.pHost->nMinArg);` |
|   ! 0 | 5363 | `		return PH7_OK;` |
|     - | 5364 | `	}` |
|    25 | 5365 | `	nTotal = ReflectParamCount(&sRef);` |
|    55 | 5366 | `	for( n = nTotal ; n > 0 ; n-- ){` |
|    53 | 5367 | `		if( ReflectParamAt(&sRef, n - 1, &sDesc) && !sDesc.bVariadic && !sDesc.bOptional ){` |
|    23 | 5368 | `			nReq = n;` |
|    23 | 5369 | `			break;` |
|     - | 5370 | `		}` |
|    16 | 5371 | `	}` |
|    25 | 5372 | `	ph7_result_int(pCtx, nReq);` |
|    25 | 5373 | `	return PH7_OK;` |
|    13 | 5374 | `}` |
|     - | 5375 | `/* The (target, method) pair a ReflectionParameter is built over. */` |
|   286 | 5376 | `static void ReflectFuncParamSpec(ph7_context *pCtx, ph7_value *pSpec)` |
|     4 | 5377 | `{` |
|   290 | 5378 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 5379 | `	ph7_class_instance *pClo;` |
|     - | 5380 | `	const char *zName, *zClass;` |
|     - | 5381 | `	int nName, nClass;` |
|   290 | 5382 | `	PH7_MemObjInit(pCtx->pVm, pSpec);` |
|   290 | 5383 | `	if( pThis == 0 ){` |
|   ! 0 | 5384 | `		return;` |
|     - | 5385 | `	}` |
|   290 | 5386 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|   290 | 5387 | `	if( pClo ){` |
|    11 | 5388 | `		pSpec->x.pOther = pClo;` |
|    11 | 5389 | `		pSpec->iFlags = MEMOBJ_OBJ;` |
|    11 | 5390 | `		return;` |
|     - | 5391 | `	}` |
|   280 | 5392 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|   280 | 5393 | `	if( ReflectIsMethodReflector(pCtx, pThis) ){` |
|     - | 5394 | ``		/* `[class, method]`, which ReflectionParameter's constructor accepts */`` |
|   119 | 5395 | `		ph7_value *pList = ph7_context_new_array(pCtx);` |
|   119 | 5396 | `		ph7_value *pA = ph7_context_new_scalar(pCtx);` |
|   119 | 5397 | `		ph7_value *pB = ph7_context_new_scalar(pCtx);` |
|   119 | 5398 | `		if( pList == 0 \|\| pA == 0 \|\| pB == 0 ){` |
|   ! 0 | 5399 | `			return;` |
|     - | 5400 | `		}` |
|   119 | 5401 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|   119 | 5402 | `		ph7_value_string(pA, zClass, nClass);` |
|   119 | 5403 | `		ph7_value_string(pB, zName, nName);` |
|   119 | 5404 | `		ph7_array_add_elem(pList, 0, pA);` |
|   119 | 5405 | `		ph7_array_add_elem(pList, 0, pB);` |
|   119 | 5406 | `		PH7_MemObjStore(pList, pSpec);` |
|   119 | 5407 | `		return;` |
|     - | 5408 | `	}` |
|   162 | 5409 | `	ph7_value_string(pSpec, zName, nName);` |
|   147 | 5410 | `}` |
|   274 | 5411 | `static int vm_builtin_ReflectionFunc_getParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     4 | 5412 | `{` |
|     - | 5413 | `	ReflectFuncRef sRef;` |
|   278 | 5414 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     - | 5415 | `	ph7_value sSpec;` |
|     - | 5416 | `	int n, nTotal;` |
|   137 | 5417 | `	SXUNUSED(nArg);` |
|   137 | 5418 | `	SXUNUSED(apArg);` |
|   278 | 5419 | `	if( pOut == 0 ){` |
|   ! 0 | 5420 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 5421 | `	}` |
|   278 | 5422 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|   ! 0 | 5423 | `		ph7_result_value(pCtx, pOut);` |
|   ! 0 | 5424 | `		return PH7_OK;` |
|     - | 5425 | `	}` |
|   278 | 5426 | `	nTotal = ReflectParamCount(&sRef);` |
|   278 | 5427 | `	ReflectFuncParamSpec(pCtx, &sSpec);` |
|   672 | 5428 | `	for( n = 0 ; n < nTotal ; n++ ){` |
|     - | 5429 | `		ph7_value sPos, sVal;` |
|     - | 5430 | `		ph7_value *apCtor[2];` |
|     - | 5431 | `		ph7_class_instance *pParam;` |
|     - | 5432 | `		sxi32 rc;` |
|   398 | 5433 | `		PH7_MemObjInit(pCtx->pVm, &sPos);` |
|   398 | 5434 | `		ph7_value_int(&sPos, n);` |
|   398 | 5435 | `		apCtor[0] = &sSpec;` |
|   398 | 5436 | `		apCtor[1] = &sPos;` |
|   398 | 5437 | `		pParam = ReflectConstruct(pCtx, "ReflectionParameter", 2, apCtor, &rc);` |
|   398 | 5438 | `		PH7_MemObjRelease(&sPos);` |
|   398 | 5439 | `		if( pParam == 0 ){` |
|   ! 0 | 5440 | `			break;` |
|     - | 5441 | `		}` |
|   398 | 5442 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|   398 | 5443 | `		sVal.x.pOther = pParam;` |
|   398 | 5444 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|   398 | 5445 | `		ph7_array_add_elem(pOut, 0, &sVal);   /* takes its own reference */` |
|   398 | 5446 | `		PH7_ClassInstanceUnref(pParam);` |
|   201 | 5447 | `	}` |
|   278 | 5448 | `	if( (sSpec.iFlags & MEMOBJ_OBJ) == 0 ){` |
|   272 | 5449 | `		PH7_MemObjRelease(&sSpec);` |
|   134 | 5450 | `	}` |
|   278 | 5451 | `	ph7_result_value(pCtx, pOut);` |
|   278 | 5452 | `	return PH7_OK;` |
|   141 | 5453 | `}` |
|     4 | 5454 | `static int vm_builtin_ReflectionFunc_getStaticVariables(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5455 | `{` |
|     - | 5456 | `	ReflectFuncRef sRef;` |
|     5 | 5457 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     - | 5458 | `	ph7_vm_func_static_var *aStatic;` |
|     - | 5459 | `	sxu32 n;` |
|     2 | 5460 | `	SXUNUSED(nArg);` |
|     2 | 5461 | `	SXUNUSED(apArg);` |
|     5 | 5462 | `	if( pOut == 0 ){` |
|   ! 0 | 5463 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 5464 | `	}` |
|     5 | 5465 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pFunc == 0 ){` |
|   ! 0 | 5466 | `		ph7_result_value(pCtx, pOut);` |
|   ! 0 | 5467 | `		return PH7_OK;` |
|     - | 5468 | `	}` |
|     - | 5469 | `	/* Current value when the slot was initialized (first call), otherwise the` |
|     - | 5470 | `	 * evaluated default — php's getStaticVariables initializes on demand and` |
|     - | 5471 | `	 * reports the same values. */` |
|     5 | 5472 | `	aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&sRef.pFunc->aStatic);` |
|     9 | 5473 | `	for( n = 0 ; n < SySetUsed(&sRef.pFunc->aStatic) ; n++ ){` |
|     5 | 5474 | `		ph7_value *pVal = 0;` |
|     - | 5475 | `		ph7_value sScratch;` |
|     5 | 5476 | `		int bScratch = 0;` |
|     5 | 5477 | `		if( aStatic[n].nIdx != SXU32_HIGH ){` |
|     3 | 5478 | `			pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);` |
|     1 | 5479 | `		}` |
|     5 | 5480 | `		if( pVal == 0 ){` |
|     3 | 5481 | `			PH7_MemObjInit(pCtx->pVm, &sScratch);` |
|     3 | 5482 | `			if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|     3 | 5483 | `				VmLocalExec(pCtx->pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|     1 | 5484 | `			}` |
|     3 | 5485 | `			pVal = &sScratch;` |
|     3 | 5486 | `			bScratch = 1;` |
|     1 | 5487 | `		}` |
|     5 | 5488 | `		ReflectMapAddDyn(pCtx, pOut, &aStatic[n].sName, pVal);` |
|     5 | 5489 | `		if( bScratch ){` |
|     3 | 5490 | `			PH7_MemObjRelease(&sScratch);` |
|     1 | 5491 | `		}` |
|     3 | 5492 | `	}` |
|     5 | 5493 | `	ph7_result_value(pCtx, pOut);` |
|     5 | 5494 | `	return PH7_OK;` |
|     3 | 5495 | `}` |
|     - | 5496 | ``/* One `key => value` entry of a presented shape, by name. */`` |
|    46 | 5497 | `static void ClosurePresentAdd(ph7_vm *pVm, ph7_value *pOut, const char *zKey, ph7_value *pVal)` |
|     1 | 5498 | `{` |
|     - | 5499 | `	ph7_value sKey;` |
|    47 | 5500 | `	PH7_MemObjInitFromString(pVm, &sKey, 0);` |
|    47 | 5501 | `	PH7_MemObjStringAppend(&sKey, zKey, (sxu32)SyStrlen(zKey));` |
|    47 | 5502 | `	ph7_array_add_elem(pOut, &sKey, pVal);   /* takes its OWN reference */` |
|    47 | 5503 | `	PH7_MemObjRelease(&sKey);` |
|    47 | 5504 | `}` |
|     - | 5505 | `/*` |
|     - | 5506 | ` * php's DEBUG presentation for a Closure (ph7_class::xPresent) —` |
|     - | 5507 | `` * `zend_closure_get_debug_info` (Zend/zend_closures.c), key for key and in php's`` |
|     - | 5508 | `` * order. Every key is CONDITIONAL, which is why a plain `function(){}` shows three`` |
|     - | 5509 | ` * and a bound one with captures shows five:` |
|     - | 5510 | ` *` |
|     - | 5511 | ` *   - a FAKE closure (php's ZEND_ACC_FAKE_CLOSURE — a first-class callable or` |
|     - | 5512 | `` *     `Closure::fromCallable` over a NAMED function) shows one `function` key,`` |
|     - | 5513 | `` *     `Class::method` when it carries a scope and the bare name otherwise, and NO`` |
|     - | 5514 | `` *     name/file/line. A real closure shows `name` (php's `{closure:SCOPE:LINE}`,`` |
|     - | 5515 | `` *     which `PH7_VmFuncDisplayName` answers), `file` and an INT `line`;`` |
|     - | 5516 | `` *   - `static` is the closure's own variable state — the `use` captures AND the`` |
|     - | 5517 | `` *     body's `static $x` — keyed WITHOUT the `$`, and omitted when empty. php`` |
|     - | 5518 | ` *     shows it before the first call too, since the initializers are compiled;` |
|     - | 5519 | `` *   - `this` is the bound receiver, present only when there is one (`bindTo`,`` |
|     - | 5520 | ` *     an instance first-class callable, or the implicit capture in a method);` |
|     - | 5521 | `` *   - `parameter` is `"$name" => "<required>"\|"<optional>"`, `&$name` for a by-ref`` |
|     - | 5522 | ` *     parameter, omitted when the callee takes none. php's cut is` |
|     - | 5523 | `` *     `required_num_args`, which counts through the LAST required parameter — so`` |
|     - | 5524 | `` *     `function($a, $b = 1, $c)` reports all THREE as `<required>`, not two.`` |
|     - | 5525 | ` *` |
|     - | 5526 | `` * The non-debug half answers nothing: php's `get_properties` for a Closure is an`` |
|     - | 5527 | `` * empty table, and `(array)$closure` never reaches it at all (php special-cases a`` |
|     - | 5528 | `` * Closure in `convert_to_array` and wraps it as a SCALAR, `[0 => $closure]`; PHL`` |
|     - | 5529 | `` * answers `[]` there — PLAN §4).`` |
|     - | 5530 | ` */` |
|    18 | 5531 | `PH7_PRIVATE sxi32 PH7_ClosurePresent(ph7_vm *pVm, ph7_class_instance *pThis,` |
|     - | 5532 | `	ph7_value *pOut, int bDebug)` |
|     1 | 5533 | `{` |
|     - | 5534 | `	ReflectFuncRef sRef;` |
|     - | 5535 | `	ph7_value sCarrier, sVal;` |
|    19 | 5536 | `	int bFake = 1;` |
|    19 | 5537 | `	if( !bDebug \|\| pThis == 0 ){` |
|     5 | 5538 | `		return SXRET_OK;` |
|     - | 5539 | `	}` |
|     - | 5540 | `	/* Rule 16's other half: this carrier never took a reference, so it must be` |
|     - | 5541 | `	 * blanked before it is released or the Closure is unref'd a second time. */` |
|    15 | 5542 | `	PH7_MemObjInit(pVm, &sCarrier);` |
|    15 | 5543 | `	sCarrier.x.pOther = pThis;` |
|    15 | 5544 | `	MemObjSetType(&sCarrier, MEMOBJ_OBJ);` |
|    15 | 5545 | `	if( !ReflectFuncFill(pVm, &sCarrier, 0, &sRef) ){` |
|   ! 0 | 5546 | `		sCarrier.x.pOther = 0;` |
|   ! 0 | 5547 | `		sCarrier.iFlags = MEMOBJ_NULL;` |
|   ! 0 | 5548 | `		PH7_MemObjRelease(&sCarrier);` |
|   ! 0 | 5549 | `		return SXRET_OK;` |
|     - | 5550 | `	}` |
|    15 | 5551 | `	sCarrier.x.pOther = 0;` |
|    15 | 5552 | `	sCarrier.iFlags = MEMOBJ_NULL;` |
|    15 | 5553 | `	PH7_MemObjRelease(&sCarrier);` |
|     - | 5554 | `	/* php's FAKE-closure bit, read off what the callable actually resolved TO: a` |
|     - | 5555 | `	 * real closure's body is the anonymous function itself (the compiler's` |
|     - | 5556 | ``	 * `{closure:...}` name, or the synthesized key that stands in for it). */`` |
|    15 | 5557 | `	if( sRef.pFunc && sRef.pMeth == 0 ){` |
|     9 | 5558 | `		const SyString *pN = &sRef.pFunc->sName;` |
|     8 | 5559 | `		if( SyStringLength(&sRef.pFunc->sClosureName) > 0` |
|     4 | 5560 | `		 \|\| (pN->nByte > 8 && SyMemcmp(pN->zString, "[lambda_", 8) == 0)` |
|     1 | 5561 | `		 \|\| (pN->nByte > 9 && SyMemcmp(pN->zString, "[closure_", 9) == 0) ){` |
|     9 | 5562 | `			bFake = 0;` |
|     4 | 5563 | `		}` |
|     4 | 5564 | `	}` |
|    15 | 5565 | `	PH7_MemObjInit(pVm, &sVal);` |
|    15 | 5566 | `	if( bFake ){` |
|     7 | 5567 | `		ph7_class *pScope = ReflectFuncDeclClass(&sRef);` |
|     7 | 5568 | `		const SyString *pName = sRef.pFunc ? &sRef.pFunc->sName : &sRef.pHost->sName;` |
|     7 | 5569 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|     7 | 5570 | `		if( pScope == 0 && sRef.pClass ){` |
|   ! 0 | 5571 | `			pScope = sRef.pClass;` |
|   ! 0 | 5572 | `		}` |
|     7 | 5573 | `		if( pScope ){` |
|     7 | 5574 | `			PH7_MemObjStringAppend(&sVal, SyStringData(&pScope->sName),` |
|     2 | 5575 | `				SyStringLength(&pScope->sName));` |
|     5 | 5576 | `			PH7_MemObjStringAppend(&sVal, "::", 2);` |
|     2 | 5577 | `		}` |
|     7 | 5578 | `		PH7_MemObjStringAppend(&sVal, SyStringData(pName), SyStringLength(pName));` |
|     7 | 5579 | `		ClosurePresentAdd(pVm, pOut, "function", &sVal);` |
|     7 | 5580 | `		PH7_MemObjRelease(&sVal);` |
|     4 | 5581 | `	}else{` |
|     - | 5582 | `		const char *zShow;` |
|     9 | 5583 | `		int nShow = PH7_VmFuncDisplayName(pVm, sRef.pFunc, &zShow);` |
|     9 | 5584 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|     9 | 5585 | `		PH7_MemObjStringAppend(&sVal, zShow, (sxu32)(nShow > 0 ? nShow : 0));` |
|     9 | 5586 | `		ClosurePresentAdd(pVm, pOut, "name", &sVal);` |
|     9 | 5587 | `		PH7_MemObjRelease(&sVal);` |
|     9 | 5588 | `		PH7_MemObjInitFromString(pVm, &sVal, 0);` |
|    13 | 5589 | `		PH7_MemObjStringAppend(&sVal, SyStringData(&sRef.pFunc->sFile),` |
|     8 | 5590 | `			SyStringLength(&sRef.pFunc->sFile));` |
|     9 | 5591 | `		ClosurePresentAdd(pVm, pOut, "file", &sVal);` |
|     9 | 5592 | `		PH7_MemObjRelease(&sVal);` |
|     9 | 5593 | `		PH7_MemObjInitFromInt(pVm, &sVal, (sxi64)sRef.pFunc->nLine);` |
|     9 | 5594 | `		ClosurePresentAdd(pVm, pOut, "line", &sVal);` |
|     9 | 5595 | `		PH7_MemObjRelease(&sVal);` |
|     - | 5596 | `	}` |
|     - | 5597 | ``	/* `static`: the captured `use` variables first (php compiles them into the`` |
|     - | 5598 | ``	 * same table, ahead of the body's own statics), then the body's `static $x`,`` |
|     - | 5599 | `	 * whose value is the live slot once it exists and the compiled initializer` |
|     - | 5600 | `	 * before that — the rule getStaticVariables() already follows. */` |
|    15 | 5601 | `	if( sRef.pFunc ){` |
|    13 | 5602 | `		ph7_hashmap *pHm = PH7_NewHashmap(pVm, 0, 0);` |
|     - | 5603 | `		sxu32 n;` |
|    13 | 5604 | `		if( pHm ){` |
|     - | 5605 | `			ph7_value sMap;` |
|    13 | 5606 | `			ph7_value *pMap = &sMap;` |
|    13 | 5607 | `			ph7_vm_func_closure_env *aEnv =` |
|    12 | 5608 | `				(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|    13 | 5609 | `			ph7_vm_func_static_var *aStatic =` |
|    12 | 5610 | `				(ph7_vm_func_static_var *)SySetBasePtr(&sRef.pFunc->aStatic);` |
|    13 | 5611 | `			PH7_MemObjInitFromArray(pVm, pMap, pHm);` |
|    25 | 5612 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++n ){` |
|    13 | 5613 | `				ph7_value *pVal = &aEnv[n].sValue;` |
|     - | 5614 | `				ph7_value sKey;` |
|    12 | 5615 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|    11 | 5616 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|     - | 5617 | ``					/* PHL carries the auto-captured `$this` as an env entry; php keeps`` |
|     - | 5618 | ``					 * it in its own `this_ptr` slot and never lists it as a static. It`` |
|     - | 5619 | ``					 * is reported below, under `this`. */`` |
|     9 | 5620 | `					continue;` |
|     - | 5621 | `				}` |
|     5 | 5622 | `				if( aEnv[n].nIdx != SXU32_HIGH ){` |
|     - | 5623 | ``					/* `use (&$x)`: the capture is an alias onto a pinned slot, and`` |
|     - | 5624 | `					 * php reports what the slot holds NOW, not the birth value. */` |
|   ! 0 | 5625 | `					ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj, aEnv[n].nIdx);` |
|   ! 0 | 5626 | `					if( pSlot ){` |
|   ! 0 | 5627 | `						pVal = pSlot;` |
|   ! 0 | 5628 | `					}` |
|   ! 0 | 5629 | `				}` |
|     5 | 5630 | `				PH7_MemObjInitFromString(pVm, &sKey, &aEnv[n].sName);` |
|     5 | 5631 | `				ph7_array_add_elem(pMap, &sKey, pVal);` |
|     5 | 5632 | `				PH7_MemObjRelease(&sKey);` |
|     3 | 5633 | `			}` |
|    15 | 5634 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aStatic) ; ++n ){` |
|     3 | 5635 | `				ph7_value *pVal = 0;` |
|     - | 5636 | `				ph7_value sScratch, sKey;` |
|     3 | 5637 | `				int bScratch = 0;` |
|     3 | 5638 | `				if( aStatic[n].nIdx != SXU32_HIGH ){` |
|   ! 0 | 5639 | `					pVal = (ph7_value *)SySetAt(&pVm->aMemObj, aStatic[n].nIdx);` |
|   ! 0 | 5640 | `				}` |
|     3 | 5641 | `				if( pVal == 0 ){` |
|     3 | 5642 | `					PH7_MemObjInit(pVm, &sScratch);` |
|     3 | 5643 | `					if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|     3 | 5644 | `						VmLocalExec(pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|     1 | 5645 | `					}` |
|     3 | 5646 | `					pVal = &sScratch;` |
|     3 | 5647 | `					bScratch = 1;` |
|     1 | 5648 | `				}` |
|     3 | 5649 | `				PH7_MemObjInitFromString(pVm, &sKey, &aStatic[n].sName);` |
|     3 | 5650 | `				ph7_array_add_elem(pMap, &sKey, pVal);` |
|     3 | 5651 | `				PH7_MemObjRelease(&sKey);` |
|     3 | 5652 | `				if( bScratch ){` |
|     3 | 5653 | `					PH7_MemObjRelease(&sScratch);` |
|     1 | 5654 | `				}` |
|     2 | 5655 | `			}` |
|    13 | 5656 | `			if( ph7_array_count(pMap) > 0 ){` |
|     5 | 5657 | `				ClosurePresentAdd(pVm, pOut, "static", pMap);` |
|     2 | 5658 | `			}` |
|    13 | 5659 | `			PH7_MemObjRelease(pMap);` |
|     6 | 5660 | `		}` |
|     6 | 5661 | `	}` |
|     - | 5662 | ``	/* `this`: the bound receiver. php keeps ONE `this_ptr` however the binding`` |
|     - | 5663 | ``	 * happened; PHL keeps two — an explicit bind (`bindTo`, an instance`` |
|     - | 5664 | ``	 * first-class callable) writes the `$__this` slot, while the IMPLICIT capture a`` |
|     - | 5665 | `	 * closure written inside a method gets rides in the closure ENVIRONMENT under` |
|     - | 5666 | ``	 * the name `this`. Either one is php's answer here. */`` |
|     - | 5667 | `	{` |
|     - | 5668 | `		SyString sAttr;` |
|     - | 5669 | `		ph7_value *pBound;` |
|    15 | 5670 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|    15 | 5671 | `		pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    15 | 5672 | `		if( (pBound == 0 \|\| (pBound->iFlags & MEMOBJ_OBJ) == 0) && sRef.pFunc ){` |
|    11 | 5673 | `			ph7_vm_func_closure_env *aEnv =` |
|    10 | 5674 | `				(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);` |
|     - | 5675 | `			sxu32 n;` |
|    11 | 5676 | `			pBound = 0;` |
|    15 | 5677 | `			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++n ){` |
|    12 | 5678 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|    11 | 5679 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|     9 | 5680 | `					pBound = &aEnv[n].sValue;` |
|     9 | 5681 | `					break;` |
|     - | 5682 | `				}` |
|     3 | 5683 | `			}` |
|     5 | 5684 | `		}` |
|    15 | 5685 | `		if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){` |
|     5 | 5686 | `			ClosurePresentAdd(pVm, pOut, "this", pBound);` |
|     2 | 5687 | `		}` |
|     - | 5688 | `	}` |
|     - | 5689 | ``	/* `parameter`: php's cut is required_num_args, which runs through the LAST`` |
|     - | 5690 | `	 * required parameter rather than stopping at the first optional one. */` |
|     - | 5691 | `	{` |
|     - | 5692 | `		ReflectParamDesc sDesc;` |
|    15 | 5693 | `		int nArgTotal = 0, nRequired = 0, i;` |
|    31 | 5694 | `		while( ReflectParamAt(&sRef, nArgTotal, &sDesc) ){` |
|    17 | 5695 | `			if( !sDesc.bOptional ){` |
|    11 | 5696 | `				nRequired = nArgTotal + 1;` |
|     5 | 5697 | `			}` |
|    17 | 5698 | `			nArgTotal++;` |
|     1 | 5699 | `		}` |
|    15 | 5700 | `		if( nArgTotal > 0 ){` |
|     9 | 5701 | `			ph7_hashmap *pHm = PH7_NewHashmap(pVm, 0, 0);` |
|     9 | 5702 | `			if( pHm ){` |
|     - | 5703 | `				ph7_value sMap;` |
|     9 | 5704 | `				ph7_value *pMap = &sMap;` |
|     9 | 5705 | `				PH7_MemObjInitFromArray(pVm, pMap, pHm);` |
|    25 | 5706 | `				for( i = 0 ; i < nArgTotal ; ++i ){` |
|     - | 5707 | `					ph7_value sKey, sWhat;` |
|    17 | 5708 | `					if( !ReflectParamAt(&sRef, i, &sDesc) ){` |
|   ! 0 | 5709 | `						break;` |
|     - | 5710 | `					}` |
|    17 | 5711 | `					PH7_MemObjInitFromString(pVm, &sKey, 0);` |
|    17 | 5712 | `					if( sDesc.bByRef ){` |
|     3 | 5713 | `						PH7_MemObjStringAppend(&sKey, "&", 1);` |
|     1 | 5714 | `					}` |
|    17 | 5715 | `					PH7_MemObjStringAppend(&sKey, "$", 1);` |
|    25 | 5716 | `					PH7_MemObjStringAppend(&sKey, SyStringData(&sDesc.sName),` |
|     8 | 5717 | `						SyStringLength(&sDesc.sName));` |
|    17 | 5718 | `					PH7_MemObjInitFromString(pVm, &sWhat, 0);` |
|    17 | 5719 | `					PH7_MemObjStringAppend(&sWhat,` |
|     8 | 5720 | `						i >= nRequired ? "<optional>" : "<required>",` |
|     8 | 5721 | `						i >= nRequired ? sizeof("<optional>")-1 : sizeof("<required>")-1);` |
|    17 | 5722 | `					ph7_array_add_elem(pMap, &sKey, &sWhat);` |
|    17 | 5723 | `					PH7_MemObjRelease(&sKey);` |
|    17 | 5724 | `					PH7_MemObjRelease(&sWhat);` |
|     9 | 5725 | `				}` |
|     9 | 5726 | `				ClosurePresentAdd(pVm, pOut, "parameter", pMap);` |
|     9 | 5727 | `				PH7_MemObjRelease(pMap);` |
|     4 | 5728 | `			}` |
|     4 | 5729 | `		}` |
|     - | 5730 | `	}` |
|    15 | 5731 | `	return SXRET_OK;` |
|    10 | 5732 | `}` |
|     2 | 5733 | `static int vm_builtin_ReflectionFunc_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5734 | `{` |
|     - | 5735 | `	ReflectFuncRef sRef;` |
|     1 | 5736 | `	SXUNUSED(nArg);` |
|     1 | 5737 | `	SXUNUSED(apArg);` |
|     3 | 5738 | `	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))` |
|     3 | 5739 | `	if( ReflectFuncIsInternal(&sRef) ){` |
|     3 | 5740 | `		ph7_result_string(pCtx, "Core", sizeof("Core")-1);` |
|     2 | 5741 | `	}else{` |
|   ! 0 | 5742 | `		ph7_result_bool(pCtx, 0);` |
|     - | 5743 | `	}` |
|     3 | 5744 | `	return PH7_OK;` |
|     2 | 5745 | `}` |
|     4 | 5746 | `static int vm_builtin_ReflectionFunc_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5747 | `{` |
|     - | 5748 | `	ReflectFuncRef sRef;` |
|     2 | 5749 | `	SXUNUSED(nArg);` |
|     2 | 5750 | `	SXUNUSED(apArg);` |
|     5 | 5751 | `	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))` |
|     5 | 5752 | `	if( !ReflectFuncIsInternal(&sRef) ){` |
|   ! 0 | 5753 | `		ph7_result_null(pCtx);` |
|   ! 0 | 5754 | `		return PH7_OK;` |
|     - | 5755 | `	}` |
|     5 | 5756 | `	return ReflectCoreExtension(pCtx);` |
|     3 | 5757 | `}` |
|    24 | 5758 | `static int vm_builtin_ReflectionFunc_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5759 | `{` |
|    25 | 5760 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 5761 | `	ReflectFuncRef sRef;` |
|     - | 5762 | `	int rc;` |
|    25 | 5763 | `	if( pThis == 0 \|\| !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pFunc == 0 ){` |
|   ! 0 | 5764 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|   ! 0 | 5765 | `		return PH7_OK;` |
|     - | 5766 | `	}` |
|    25 | 5767 | `	if( sRef.pMeth ){` |
|     - | 5768 | `		/* 4 = Attribute::TARGET_METHOD */` |
|     - | 5769 | `		ph7_value sTarget;` |
|     - | 5770 | `		const char *zClass, *zName;` |
|     - | 5771 | `		int nClass, nName;` |
|    13 | 5772 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|    13 | 5773 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    13 | 5774 | `		PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|    13 | 5775 | `		ph7_value_string(&sTarget, zClass, nClass);` |
|    19 | 5776 | `		rc = ReflectBuildAttrs(pCtx, &sRef.pFunc->aAttrs, "method", &sTarget,` |
|     6 | 5777 | `			zName, nName, 0, 4, nArg, apArg);` |
|    13 | 5778 | `		PH7_MemObjRelease(&sTarget);` |
|    13 | 5779 | `		return rc;` |
|     - | 5780 | `	}` |
|     - | 5781 | `	{` |
|     - | 5782 | `		/* 2 = Attribute::TARGET_FUNCTION */` |
|     - | 5783 | `		ph7_value sTarget;` |
|    13 | 5784 | `		ReflectFuncParamSpec(pCtx, &sTarget);` |
|    19 | 5785 | `		rc = ReflectBuildAttrs(pCtx, &sRef.pFunc->aAttrs, "fn", &sTarget, 0, 0, 0, 2,` |
|     6 | 5786 | `			nArg, apArg);` |
|    13 | 5787 | `		if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|     9 | 5788 | `			PH7_MemObjRelease(&sTarget);` |
|     4 | 5789 | `		}` |
|    13 | 5790 | `		return rc;` |
|     - | 5791 | `	}` |
|    13 | 5792 | `}` |
|     - | 5793 | `/* __toString(): php's export format, still chunk 9. */` |
|    54 | 5794 | `static int vm_builtin_ReflectionFunc_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 5795 | `{` |
|    27 | 5796 | `	SXUNUSED(nArg);` |
|    27 | 5797 | `	SXUNUSED(apArg);` |
|    56 | 5798 | `	return ReflectExportFuncSelf(pCtx);` |
|     2 | 5799 | `}` |
|     - | 5800 | `/* ---- ReflectionFunction ---- */` |
|     - | 5801 | `/* ReflectionFunction::__construct(Closure\|string $function) */` |
|   306 | 5802 | `static int vm_builtin_ReflectionFunction_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 | 5803 | `{` |
|   311 | 5804 | `	ph7_vm *pVm = pCtx->pVm;` |
|   311 | 5805 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 5806 | `	ReflectFuncRef sRef;` |
|     - | 5807 | `	ph7_class_instance *pClo;` |
|   311 | 5808 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 | 5809 | `		return PH7_OK;` |
|     - | 5810 | `	}` |
|   311 | 5811 | `	pClo = ReflectValueClosure(pVm, apArg[0]);` |
|   311 | 5812 | `	if( !ReflectFuncFill(pCtx->pVm, apArg[0], 0, &sRef) ){` |
|     - | 5813 | `		const char *zName;` |
|     - | 5814 | `		int nName;` |
|    15 | 5815 | `		if( pClo ){` |
|     - | 5816 | `			/* A Closure whose body no table holds: still a valid reflection` |
|     - | 5817 | `			 * target, so record it and let the accessors answer emptily. */` |
|   ! 0 | 5818 | `			PH7_NativeSetAttrObj(pVm, pThis, RF_CL, pClo);` |
|   ! 0 | 5819 | `			return PH7_OK;` |
|     - | 5820 | `		}` |
|    15 | 5821 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|    21 | 5822 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     6 | 5823 | `			"Function %.*s() does not exist", nName, zName);` |
|     - | 5824 | `	}` |
|   299 | 5825 | `	if( pClo ){` |
|    66 | 5826 | `		PH7_NativeSetAttrObj(pVm, pThis, RF_CL, pClo);` |
|    31 | 5827 | `	}` |
|   299 | 5828 | `	if( sRef.pFunc ){` |
|   221 | 5829 | `		int bAnon = (sRef.pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|   216 | 5830 | `		if( !bAnon && SyStringLength(&sRef.pFunc->sName) > 9` |
|   115 | 5831 | `		 && (SyMemcmp(SyStringData(&sRef.pFunc->sName), "[lambda_", 8) == 0` |
|    39 | 5832 | `		  \|\| SyMemcmp(SyStringData(&sRef.pFunc->sName), "[closure_", 9) == 0) ){` |
|     3 | 5833 | `			bAnon = 1;` |
|     1 | 5834 | `		}` |
|   221 | 5835 | `		if( bAnon ){` |
|     - | 5836 | ``			/* php 8.4 names an anonymous function `{closure:SCOPE:LINE}`, where SCOPE is`` |
|     - | 5837 | `			 * the enclosing function and only the FILE at top level — the compiler` |
|     - | 5838 | `			 * recorded it (sClosureName); the file form is the fallback for a closure` |
|     - | 5839 | `			 * built without one. */` |
|     - | 5840 | `			char zBuf[512];` |
|    42 | 5841 | `			const char *zShow = SyStringData(&sRef.pFunc->sClosureName);` |
|    42 | 5842 | `			int nShow = (int)SyStringLength(&sRef.pFunc->sClosureName);` |
|    42 | 5843 | `			if( nShow < 1 ){` |
|   ! 0 | 5844 | `				const char *zFile = SyStringLength(&sRef.pFunc->sFile) > 0` |
|   ! 0 | 5845 | `					? SyStringData(&sRef.pFunc->sFile) : "";` |
|   ! 0 | 5846 | `				int nFile = (int)SyStringLength(&sRef.pFunc->sFile);` |
|   ! 0 | 5847 | `				nShow = SyBufferFormat(zBuf, sizeof(zBuf), "{closure:%.*s:%u}",` |
|   ! 0 | 5848 | `					nFile, zFile, sRef.pFunc->nLine);` |
|   ! 0 | 5849 | `				zShow = zBuf;` |
|   ! 0 | 5850 | `			}` |
|    42 | 5851 | `			PH7_NativeSetAttrStr(pVm, pThis, "name", zShow, nShow);` |
|    42 | 5852 | `			if( pClo == 0 ){` |
|     - | 5853 | `				/* Named by its lambda name: keep a Closure so the accessors can` |
|     - | 5854 | `				 * still reach the captured scope. */` |
|   ! 0 | 5855 | `				PH7_NativeSetAttrObj(pVm, pThis, RF_CL,` |
|   ! 0 | 5856 | `					PH7_VmNewClosure(pVm, &sRef.pFunc->sName, 0, 0));` |
|   ! 0 | 5857 | `			}` |
|    42 | 5858 | `			return PH7_OK;` |
|     - | 5859 | `		}` |
|   270 | 5860 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sRef.pFunc->sName),` |
|   178 | 5861 | `			(int)SyStringLength(&sRef.pFunc->sName));` |
|   181 | 5862 | `		return PH7_OK;` |
|     - | 5863 | `	}` |
|   119 | 5864 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sRef.pHost->sName),` |
|    78 | 5865 | `		(int)SyStringLength(&sRef.pHost->sName));` |
|    80 | 5866 | `	return PH7_OK;` |
|   158 | 5867 | `}` |
|     6 | 5868 | `static int vm_builtin_ReflectionFunction_isAnonymous(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5869 | `{` |
|     7 | 5870 | `	return vm_builtin_ReflectionFunc_isClosure(pCtx, nArg, apArg);` |
|     1 | 5871 | `}` |
|   ! 0 | 5872 | `static int vm_builtin_ReflectionFunction_isDisabled(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 5873 | `{` |
|   ! 0 | 5874 | `	SXUNUSED(nArg);` |
|   ! 0 | 5875 | `	SXUNUSED(apArg);` |
|   ! 0 | 5876 | `	ph7_result_bool(pCtx, 0);` |
|   ! 0 | 5877 | `	return PH7_OK;` |
|   ! 0 | 5878 | `}` |
|     - | 5879 | `/* The callable a ReflectionFunction invokes: the Closure it holds, or its name. */` |
|    18 | 5880 | `static void ReflectFunctionCallable(ph7_context *pCtx, ph7_value *pOut)` |
|     1 | 5881 | `{` |
|    19 | 5882 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 5883 | `	ph7_class_instance *pClo;` |
|    19 | 5884 | `	const char *zName = "";` |
|    19 | 5885 | `	int nName = 0;` |
|    19 | 5886 | `	PH7_MemObjInit(pCtx->pVm, pOut);` |
|    19 | 5887 | `	if( pThis == 0 ){` |
|   ! 0 | 5888 | `		return;` |
|     - | 5889 | `	}` |
|    19 | 5890 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|    19 | 5891 | `	if( pClo ){` |
|     9 | 5892 | `		pOut->x.pOther = pClo;` |
|     9 | 5893 | `		pOut->iFlags = MEMOBJ_OBJ;` |
|     9 | 5894 | `		return;` |
|     - | 5895 | `	}` |
|    11 | 5896 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    11 | 5897 | `	ph7_value_string(pOut, zName, nName);` |
|    10 | 5898 | `}` |
|    18 | 5899 | `static int ReflectFunctionInvoke(ph7_context *pCtx, int nCall, ph7_value **apCall)` |
|     1 | 5900 | `{` |
|     - | 5901 | `	ph7_value sTarget, sResult;` |
|     - | 5902 | `	sxi32 rc;` |
|    19 | 5903 | `	ReflectFunctionCallable(pCtx, &sTarget);` |
|    19 | 5904 | `	PH7_MemObjInit(pCtx->pVm, &sResult);` |
|    19 | 5905 | `	sResult.nIdx = SXU32_HIGH;` |
|    19 | 5906 | `	rc = PH7_VmCallUserFunction(pCtx->pVm, &sTarget, nCall, apCall, &sResult);` |
|    19 | 5907 | `	if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){` |
|    11 | 5908 | `		PH7_MemObjRelease(&sTarget);` |
|     5 | 5909 | `	}` |
|    19 | 5910 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 | 5911 | `		PH7_MemObjRelease(&sResult);` |
|   ! 0 | 5912 | `		return rc;` |
|     - | 5913 | `	}` |
|    19 | 5914 | `	ph7_result_value(pCtx, &sResult);` |
|    19 | 5915 | `	PH7_MemObjRelease(&sResult);` |
|    19 | 5916 | `	return PH7_OK;` |
|    10 | 5917 | `}` |
|    16 | 5918 | `static int vm_builtin_ReflectionFunction_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5919 | `{` |
|    17 | 5920 | `	return ReflectFunctionInvoke(pCtx, nArg, apArg);` |
|     1 | 5921 | `}` |
|     2 | 5922 | `static int vm_builtin_ReflectionFunction_invokeArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5923 | `{` |
|     - | 5924 | `	SySet aCall;` |
|     - | 5925 | `	int rc;` |
|     3 | 5926 | `	SySetInit(&aCall, &pCtx->pVm->sAllocator, sizeof(ph7_value *));` |
|     3 | 5927 | `	if( nArg > 0 ){` |
|     3 | 5928 | `		ReflectCollectArgs(pCtx, apArg[0], &aCall, 0);` |
|     1 | 5929 | `	}` |
|     3 | 5930 | `	rc = ReflectFunctionInvoke(pCtx, (int)SySetUsed(&aCall), (ph7_value **)SySetBasePtr(&aCall));` |
|     3 | 5931 | `	SySetRelease(&aCall);` |
|     3 | 5932 | `	return rc;` |
|     1 | 5933 | `}` |
|     6 | 5934 | `static int vm_builtin_ReflectionFunction_getClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 5935 | `{` |
|     7 | 5936 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 5937 | `	ph7_class_instance *pClo;` |
|     7 | 5938 | `	const char *zName = "";` |
|     7 | 5939 | `	int nName = 0;` |
|     - | 5940 | `	SyString sName;` |
|     3 | 5941 | `	SXUNUSED(nArg);` |
|     3 | 5942 | `	SXUNUSED(apArg);` |
|     7 | 5943 | `	if( pThis == 0 ){` |
|   ! 0 | 5944 | `		ph7_result_null(pCtx);` |
|   ! 0 | 5945 | `		return PH7_OK;` |
|     - | 5946 | `	}` |
|     7 | 5947 | `	pClo = PH7_NativeAttrObj(pThis, RF_CL);` |
|     7 | 5948 | `	if( pClo ){` |
|     - | 5949 | `		/* Already a Closure: hand the same instance back */` |
|     3 | 5950 | `		return ReflectResultExistingObject(pCtx, pClo);` |
|     - | 5951 | `	}` |
|     5 | 5952 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     5 | 5953 | `	SyStringInitFromBuf(&sName, zName, nName);` |
|     5 | 5954 | `	return ReflectResultObject(pCtx, PH7_VmNewClosure(pCtx->pVm, &sName, 0, 0));` |
|     4 | 5955 | `}` |
|     - | 5956 | `/* ---- ReflectionMethod ---- */` |
|     - | 5957 | `/* ReflectionMethod::__construct(object\|string $objectOrMethod, ?string $method = null) */` |
|   428 | 5958 | `static int vm_builtin_ReflectionMethod_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 | 5959 | `{` |
|   433 | 5960 | `	ph7_vm *pVm = pCtx->pVm;` |
|   433 | 5961 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 5962 | `	ph7_class *pClass;` |
|     - | 5963 | `	SyHashEntry *pEntry;` |
|     - | 5964 | `	ph7_value sClass, sMethod;` |
|     - | 5965 | `	const char *zMethod;` |
|   433 | 5966 | `	int nMethod, rc = PH7_OK;` |
|   433 | 5967 | `	if( pThis == 0 \|\| nArg < 1 ){` |
|   ! 0 | 5968 | `		return PH7_OK;` |
|     - | 5969 | `	}` |
|   433 | 5970 | `	PH7_MemObjInit(pVm, &sClass);` |
|   433 | 5971 | `	PH7_MemObjInit(pVm, &sMethod);` |
|   434 | 5972 | `	if( nArg < 2 \|\| ph7_value_is_null(apArg[1]) ){` |
|     - | 5973 | `		/* One-argument form: "Class::method" */` |
|     - | 5974 | `		const char *zSpec;` |
|     3 | 5975 | `		int nSpec, iSep = -1, k;` |
|     3 | 5976 | `		if( (apArg[0]->iFlags & MEMOBJ_STRING) == 0 ){` |
|   ! 0 | 5977 | `			PH7_MemObjRelease(&sClass);` |
|   ! 0 | 5978 | `			PH7_MemObjRelease(&sMethod);` |
|   ! 0 | 5979 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     - | 5980 | `				"The parameter class is expected to be either a string or an object");` |
|     - | 5981 | `		}` |
|     3 | 5982 | `		zSpec = ph7_value_to_string(apArg[0], &nSpec);` |
|    19 | 5983 | `		for( k = 0 ; k + 1 < nSpec ; k++ ){` |
|    19 | 5984 | `			if( zSpec[k] == ':' && zSpec[k+1] == ':' ){` |
|     3 | 5985 | `				iSep = k;` |
|     3 | 5986 | `				break;` |
|     - | 5987 | `			}` |
|     9 | 5988 | `		}` |
|     3 | 5989 | `		if( iSep < 0 ){` |
|   ! 0 | 5990 | `			PH7_MemObjRelease(&sClass);` |
|   ! 0 | 5991 | `			PH7_MemObjRelease(&sMethod);` |
|   ! 0 | 5992 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|   ! 0 | 5993 | `				"Invalid method name %.*s", nSpec, zSpec);` |
|     - | 5994 | `		}` |
|     - | 5995 | `		/* php 8.3 deprecated the one-argument spelling in favour of the static` |
|     - | 5996 | `		 * factory. createFromMethodName() splits the name ITSELF and constructs` |
|     - | 5997 | `		 * with two, so it does not trip this. */` |
|     3 | 5998 | `		PH7_VmThrowError(pVm, 0, E_DEPRECATED,` |
|     - | 5999 | `			"Calling ReflectionMethod::__construct() with 1 argument is deprecated, "` |
|     - | 6000 | `			"use ReflectionMethod::createFromMethodName() instead");` |
|     3 | 6001 | `		ph7_value_string(&sClass, zSpec, iSep);` |
|     3 | 6002 | `		ph7_value_string(&sMethod, &zSpec[iSep+2], nSpec - iSep - 2);` |
|     2 | 6003 | `	}else{` |
|   431 | 6004 | `		PH7_MemObjStore(apArg[0], &sClass);` |
|   431 | 6005 | `		PH7_MemObjStore(apArg[1], &sMethod);` |
|     - | 6006 | `	}` |
|   433 | 6007 | `	pClass = ReflectResolveClass(pVm, &sClass);` |
|   433 | 6008 | `	if( pClass == 0 ){` |
|     - | 6009 | `		const char *zName;` |
|     - | 6010 | `		int nName;` |
|     3 | 6011 | `		zName = ph7_value_to_string(&sClass, &nName);` |
|     4 | 6012 | `		rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|     1 | 6013 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|     3 | 6014 | `		goto Done;` |
|     - | 6015 | `	}` |
|   431 | 6016 | `	zMethod = ph7_value_to_string(&sMethod, &nMethod);` |
|   431 | 6017 | `	pEntry = ReflectFindMethodEntry(pClass, zMethod, nMethod);` |
|   431 | 6018 | `	if( pEntry == 0 ){` |
|    10 | 6019 | `		rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|     3 | 6020 | `			"Method %z::%.*s() does not exist", &pClass->sName, nMethod, zMethod);` |
|     7 | 6021 | `		goto Done;` |
|     - | 6022 | `	}` |
|     - | 6023 | `	{` |
|     - | 6024 | `		/* php's $class is the DECLARING class, not the one the lookup went` |
|     - | 6025 | ``		 * through: `new ReflectionMethod('Kid','bm')` on an inherited method`` |
|     - | 6026 | `		 * reports Base. */` |
|   425 | 6027 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, (ph7_class_method *)pEntry->pUserData);` |
|   635 | 6028 | `		PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pDecl->sName),` |
|   420 | 6029 | `			(int)SyStringLength(&pDecl->sName));` |
|     - | 6030 | `	}` |
|     - | 6031 | `	/* The DECLARED spelling, whatever case was asked for. */` |
|   425 | 6032 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", (const char *)pEntry->pKey, (int)pEntry->nKeyLen);` |
|   214 | 6033 | `Done:` |
|   433 | 6034 | `	PH7_MemObjRelease(&sClass);` |
|   433 | 6035 | `	PH7_MemObjRelease(&sMethod);` |
|   433 | 6036 | `	return rc;` |
|   219 | 6037 | `}` |
|     - | 6038 | `/* ReflectionMethod::createFromMethodName(string $method): static */` |
|     4 | 6039 | `static int vm_builtin_ReflectionMethod_createFromMethodName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6040 | `{` |
|     - | 6041 | `	ph7_class_instance *pOut;` |
|     - | 6042 | `	ph7_value sClass, sMethod;` |
|     - | 6043 | `	ph7_value *apCtor[2];` |
|     - | 6044 | `	const char *zSpec;` |
|     5 | 6045 | `	int nSpec, iSep = -1, k;` |
|     - | 6046 | `	sxi32 rc;` |
|     5 | 6047 | `	if( nArg < 1 ){` |
|   ! 0 | 6048 | `		ph7_result_null(pCtx);` |
|   ! 0 | 6049 | `		return PH7_OK;` |
|     - | 6050 | `	}` |
|     - | 6051 | `	/* Split here rather than letting the constructor do it: the one-argument` |
|     - | 6052 | `	 * constructor is the DEPRECATED spelling this factory exists to replace. */` |
|     5 | 6053 | `	zSpec = ph7_value_to_string(apArg[0], &nSpec);` |
|    43 | 6054 | `	for( k = 0 ; k + 1 < nSpec ; k++ ){` |
|    43 | 6055 | `		if( zSpec[k] == ':' && zSpec[k+1] == ':' ){` |
|     5 | 6056 | `			iSep = k;` |
|     5 | 6057 | `			break;` |
|     - | 6058 | `		}` |
|    20 | 6059 | `	}` |
|     5 | 6060 | `	if( iSep < 0 ){` |
|   ! 0 | 6061 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|   ! 0 | 6062 | `			"Invalid method name %.*s", nSpec, zSpec);` |
|     - | 6063 | `	}` |
|     5 | 6064 | `	PH7_MemObjInit(pCtx->pVm, &sClass);` |
|     5 | 6065 | `	PH7_MemObjInit(pCtx->pVm, &sMethod);` |
|     5 | 6066 | `	ph7_value_string(&sClass, zSpec, iSep);` |
|     5 | 6067 | `	ph7_value_string(&sMethod, &zSpec[iSep+2], nSpec - iSep - 2);` |
|     5 | 6068 | `	apCtor[0] = &sClass;` |
|     5 | 6069 | `	apCtor[1] = &sMethod;` |
|     5 | 6070 | `	pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|     5 | 6071 | `	PH7_MemObjRelease(&sClass);` |
|     5 | 6072 | `	PH7_MemObjRelease(&sMethod);` |
|     5 | 6073 | `	if( pOut == 0 ){` |
|   ! 0 | 6074 | `		if( rc != PH7_OK ){` |
|   ! 0 | 6075 | `			return rc;` |
|     - | 6076 | `		}` |
|   ! 0 | 6077 | `		ph7_result_null(pCtx);` |
|   ! 0 | 6078 | `		return PH7_OK;` |
|     - | 6079 | `	}` |
|     5 | 6080 | `	return ReflectResultObject(pCtx, pOut);` |
|     3 | 6081 | `}` |
|     - | 6082 | `/* The visibility/modifier predicates, all off the method record. */` |
|    24 | 6083 | `static int ReflectMethodFlag(ph7_context *pCtx, int iWhat)` |
|     1 | 6084 | `{` |
|     - | 6085 | `	ReflectFuncRef sRef;` |
|    25 | 6086 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|   ! 0 | 6087 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 6088 | `		return PH7_OK;` |
|     - | 6089 | `	}` |
|    25 | 6090 | `	switch( iWhat ){` |
|     5 | 6091 | `	case 0: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PUBLIC); break;` |
|     9 | 6092 | `	case 1: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PRIVATE); break;` |
|     3 | 6093 | `	case 2: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PROTECTED); break;` |
|   ! 0 | 6094 | `	case 3: ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0); break;` |
|    11 | 6095 | `	default: ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0); break;` |
|     - | 6096 | `	}` |
|    25 | 6097 | `	return PH7_OK;` |
|    13 | 6098 | `}` |
|     - | 6099 | `#define REFLECT_METHOD_FLAG(NAME,WHAT) \` |
|     - | 6100 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|     - | 6101 | `	{ \` |
|     - | 6102 | `		SXUNUSED(nArg); \` |
|     - | 6103 | `		SXUNUSED(apArg); \` |
|     - | 6104 | `		return ReflectMethodFlag(pCtx, WHAT); \` |
|     - | 6105 | `	}` |
|     5 | 6106 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isPublic, 0)` |
|     9 | 6107 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isPrivate, 1)` |
|     3 | 6108 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isProtected, 2)` |
|   ! 0 | 6109 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isAbstract, 3)` |
|    11 | 6110 | `REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isFinal, 4)` |
|     - | 6111 |  |
|     - | 6112 | `/* isConstructor()/isDestructor() read the NAME, not the record: a trait` |
|     - | 6113 | `` * `use T { m as __construct; }` alias is the constructor under that key. */`` |
|     4 | 6114 | `static int ReflectMethodNameIs(ph7_context *pCtx, const char *zWant, int nWant)` |
|     2 | 6115 | `{` |
|     6 | 6116 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     6 | 6117 | `	const char *zName = "";` |
|     6 | 6118 | `	int nName = 0;` |
|     6 | 6119 | `	if( pThis ){` |
|     6 | 6120 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|     2 | 6121 | `	}` |
|     6 | 6122 | `	ph7_result_bool(pCtx, nName == nWant && SyStrnicmp(zName, zWant, (sxu32)nWant) == 0);` |
|     6 | 6123 | `	return PH7_OK;` |
|     2 | 6124 | `}` |
|     4 | 6125 | `static int vm_builtin_ReflectionMethod_isConstructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 6126 | `{` |
|     2 | 6127 | `	SXUNUSED(nArg);` |
|     2 | 6128 | `	SXUNUSED(apArg);` |
|     6 | 6129 | `	return ReflectMethodNameIs(pCtx, "__construct", sizeof("__construct")-1);` |
|     2 | 6130 | `}` |
|   ! 0 | 6131 | `static int vm_builtin_ReflectionMethod_isDestructor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 6132 | `{` |
|   ! 0 | 6133 | `	SXUNUSED(nArg);` |
|   ! 0 | 6134 | `	SXUNUSED(apArg);` |
|   ! 0 | 6135 | `	return ReflectMethodNameIs(pCtx, "__destruct", sizeof("__destruct")-1);` |
|   ! 0 | 6136 | `}` |
|     8 | 6137 | `static int vm_builtin_ReflectionMethod_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6138 | `{` |
|     - | 6139 | `	ReflectFuncRef sRef;` |
|     4 | 6140 | `	SXUNUSED(nArg);` |
|     4 | 6141 | `	SXUNUSED(apArg);` |
|     9 | 6142 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|   ! 0 | 6143 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 | 6144 | `		return PH7_OK;` |
|     - | 6145 | `	}` |
|     9 | 6146 | `	ph7_result_int64(pCtx, ReflectMethodModifiers(sRef.pMeth));` |
|     9 | 6147 | `	return PH7_OK;` |
|     5 | 6148 | `}` |
|   178 | 6149 | `static int vm_builtin_ReflectionMethod_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 6150 | `{` |
|     - | 6151 | `	ReflectFuncRef sRef;` |
|    89 | 6152 | `	SXUNUSED(nArg);` |
|    89 | 6153 | `	SXUNUSED(apArg);` |
|   181 | 6154 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|   ! 0 | 6155 | `		ph7_result_null(pCtx);` |
|   ! 0 | 6156 | `		return PH7_OK;` |
|     - | 6157 | `	}` |
|   181 | 6158 | `	return ReflectResultClassOf(pCtx, ReflectFuncDeclClass(&sRef));` |
|    92 | 6159 | `}` |
|     - | 6160 | `/*` |
|     - | 6161 | ` * The receiver check php runs before invoking or binding: a non-static method` |
|     - | 6162 | ` * needs an instance OF ITS DECLARING CLASS; a static one ignores whatever was` |
|     - | 6163 | ` * handed over. *ppThis is cleared for a static method.` |
|     - | 6164 | ` */` |
|    40 | 6165 | `static sxi32 ReflectMethodReceiver(ph7_context *pCtx, ReflectFuncRef *pRef,` |
|     - | 6166 | `	ph7_value *pObject, ph7_class_instance **ppThis, int bClosure)` |
|     1 | 6167 | `{` |
|    41 | 6168 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    41 | 6169 | `	const char *zClass = "", *zName = "";` |
|    41 | 6170 | `	int nClass = 0, nName = 0;` |
|    41 | 6171 | `	ph7_class *pDecl = ReflectFuncDeclClass(pRef);` |
|    41 | 6172 | `	*ppThis = 0;` |
|    41 | 6173 | `	if( pThis ){` |
|    41 | 6174 | `		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|    41 | 6175 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    20 | 6176 | `	}` |
|    41 | 6177 | `	if( pRef->pMeth && (pRef->pMeth->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|    11 | 6178 | `		return PH7_OK;` |
|     - | 6179 | `	}` |
|    31 | 6180 | `	if( pObject == 0 \|\| (pObject->iFlags & MEMOBJ_OBJ) == 0 ){` |
|     9 | 6181 | `		if( bClosure ){` |
|     5 | 6182 | `			return PH7_VmThrowException(pCtx, "ValueError",` |
|     - | 6183 | `				"ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods");` |
|     - | 6184 | `		}` |
|     7 | 6185 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     - | 6186 | `			"Trying to invoke non static method %.*s::%.*s() without an object",` |
|     2 | 6187 | `			nClass, zClass, nName, zName);` |
|     - | 6188 | `	}` |
|    23 | 6189 | `	*ppThis = (ph7_class_instance *)pObject->x.pOther;` |
|    23 | 6190 | `	if( pDecl && !PH7_VmInstanceOf((*ppThis)->pClass, pDecl) ){` |
|     3 | 6191 | `		*ppThis = 0;` |
|     3 | 6192 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     - | 6193 | `			"Given object is not an instance of the class this method was declared in");` |
|     - | 6194 | `	}` |
|    21 | 6195 | `	return PH7_OK;` |
|    21 | 6196 | `}` |
|    24 | 6197 | `static int ReflectMethodInvoke(ph7_context *pCtx, ph7_value *pObject, int nCall, ph7_value **apCall)` |
|     1 | 6198 | `{` |
|    25 | 6199 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 6200 | `	ReflectFuncRef sRef;` |
|    25 | 6201 | `	ph7_class_instance *pRecv = 0;` |
|     - | 6202 | `	ph7_value sResult;` |
|     - | 6203 | `	sxi32 rc;` |
|    25 | 6204 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 ){` |
|   ! 0 | 6205 | `		ph7_result_null(pCtx);` |
|   ! 0 | 6206 | `		return PH7_OK;` |
|     - | 6207 | `	}` |
|    25 | 6208 | `	rc = ReflectMethodReceiver(pCtx, &sRef, pObject, &pRecv, 0);` |
|    25 | 6209 | `	if( rc != PH7_OK ){` |
|     7 | 6210 | `		return rc;` |
|     - | 6211 | `	}` |
|    19 | 6212 | `	PH7_MemObjInit(pVm, &sResult);` |
|    19 | 6213 | `	sResult.nIdx = SXU32_HIGH;` |
|     - | 6214 | `	/* Reflection ignores method visibility (PHP 8.1+); the flag is consumed by` |
|     - | 6215 | `	 * the first OP_CALL, i.e. this synthetic one. */` |
|    19 | 6216 | `	pVm->bReflectBypass = 1;` |
|    19 | 6217 | `	rc = PH7_VmCallClassMethod(pVm, pRecv, sRef.pMeth, &sResult, nCall, apCall);` |
|    19 | 6218 | `	pVm->bReflectBypass = 0;` |
|    19 | 6219 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 | 6220 | `		PH7_MemObjRelease(&sResult);` |
|   ! 0 | 6221 | `		return rc;` |
|     - | 6222 | `	}` |
|    19 | 6223 | `	ph7_result_value(pCtx, &sResult);` |
|    19 | 6224 | `	PH7_MemObjRelease(&sResult);` |
|    19 | 6225 | `	return PH7_OK;` |
|    13 | 6226 | `}` |
|    20 | 6227 | `static int vm_builtin_ReflectionMethod_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6228 | `{` |
|    41 | 6229 | `	return ReflectMethodInvoke(pCtx, nArg > 0 ? apArg[0] : 0,` |
|    20 | 6230 | `		nArg > 1 ? nArg - 1 : 0, nArg > 1 ? &apArg[1] : 0);` |
|     1 | 6231 | `}` |
|     4 | 6232 | `static int vm_builtin_ReflectionMethod_invokeArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6233 | `{` |
|     - | 6234 | `	SySet aCall;` |
|     - | 6235 | `	int rc;` |
|     5 | 6236 | `	SySetInit(&aCall, &pCtx->pVm->sAllocator, sizeof(ph7_value *));` |
|     5 | 6237 | `	if( nArg > 1 ){` |
|     5 | 6238 | `		ReflectCollectArgs(pCtx, apArg[1], &aCall, 0);` |
|     2 | 6239 | `	}` |
|     3 | 6240 | `	rc = ReflectMethodInvoke(pCtx, nArg > 0 ? apArg[0] : 0,` |
|     4 | 6241 | `		(int)SySetUsed(&aCall), (ph7_value **)SySetBasePtr(&aCall));` |
|     5 | 6242 | `	SySetRelease(&aCall);` |
|     5 | 6243 | `	return rc;` |
|     1 | 6244 | `}` |
|    16 | 6245 | `static int vm_builtin_ReflectionMethod_getClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6246 | `{` |
|     - | 6247 | `	ReflectFuncRef sRef;` |
|    17 | 6248 | `	ph7_class_instance *pRecv = 0;` |
|     - | 6249 | `	sxi32 rc;` |
|    17 | 6250 | `	if( !ReflectFuncOfThis(pCtx, &sRef) \|\| sRef.pMeth == 0 \|\| sRef.pClass == 0 ){` |
|   ! 0 | 6251 | `		ph7_result_null(pCtx);` |
|   ! 0 | 6252 | `		return PH7_OK;` |
|     - | 6253 | `	}` |
|    17 | 6254 | `	rc = ReflectMethodReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pRecv, 1);` |
|    17 | 6255 | `	if( rc != PH7_OK ){` |
|     5 | 6256 | `		return rc;` |
|     - | 6257 | `	}` |
|     - | 6258 | `	{` |
|     - | 6259 | `		/* php hands back a closure over the RESOLVED method and reflection is allowed past` |
|     - | 6260 | `		 * protection (8.1+), so this one must dispatch without a second visibility decision —` |
|     - | 6261 | `		 * the FCC_METHOD mark is what tells the unwrap its $__fn is a screened METHOD name. */` |
|    19 | 6262 | `		ph7_class_instance *pClo = PH7_VmNewClosure(pCtx->pVm, &sRef.pMeth->sFunc.sName,` |
|    12 | 6263 | `			pRecv, &sRef.pClass->sName);` |
|    13 | 6264 | `		if( pClo ){` |
|    13 | 6265 | `			pClo->iFlags \|= VM_INSTANCE_FCC_METHOD\|VM_INSTANCE_FCC_SCREENED;` |
|     6 | 6266 | `		}` |
|    13 | 6267 | `		return ReflectResultObject(pCtx, pClo);` |
|     - | 6268 | `	}` |
|     9 | 6269 | `}` |
|     - | 6270 | `/* php 8.1 made every reflected member accessible; the setter is a no-op it kept. */` |
|   ! 0 | 6271 | `static int vm_builtin_ReflectionMethod_setAccessible(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 6272 | `{` |
|   ! 0 | 6273 | `	SXUNUSED(pCtx);` |
|   ! 0 | 6274 | `	SXUNUSED(nArg);` |
|   ! 0 | 6275 | `	SXUNUSED(apArg);` |
|   ! 0 | 6276 | `	return PH7_OK;` |
|   ! 0 | 6277 | `}` |
|     - | 6278 | `/*` |
|     - | 6279 | ` * The class this method OVERRIDES it from: the nearest base declaring a` |
|     - | 6280 | ` * same-named non-private method, else the first interface that declares one.` |
|     - | 6281 | ` */` |
|   254 | 6282 | `static ph7_class * ReflectPrototypeIn(ph7_context *pCtx, ph7_class *pClass,` |
|     - | 6283 | `	const char *zName, int nName, int bIfaceToo)` |
|     1 | 6284 | `{` |
|     - | 6285 | `	ph7_class *pWalk;` |
|   255 | 6286 | `	int iDepth = 0;` |
|   255 | 6287 | `	if( pClass == 0 \|\| nName < 1 ){` |
|   ! 0 | 6288 | `		return 0;` |
|     - | 6289 | `	}` |
|   323 | 6290 | `	for( pWalk = pClass->pBase ; pWalk && iDepth <= REFLECT_WALK_MAX_DEPTH ; pWalk = pWalk->pBase ){` |
|    93 | 6291 | `		SyHashEntry *pEntry = ReflectFindMethodEntry(pWalk, zName, nName);` |
|    93 | 6292 | `		if( pEntry ){` |
|    25 | 6293 | `			ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    25 | 6294 | `			if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|    25 | 6295 | `				return ReflectMethodDeclClass(pWalk, pMeth);` |
|     - | 6296 | `			}` |
|   ! 0 | 6297 | `		}` |
|    69 | 6298 | `		iDepth++;` |
|    35 | 6299 | `	}` |
|   231 | 6300 | `	if( !bIfaceToo ){` |
|     - | 6301 | ``		/* php's `overwrites` tag asks only about the PARENT CHAIN: a method that`` |
|     - | 6302 | ``		 * first appears in an interface is a `prototype`, never an overwrite. */`` |
|    91 | 6303 | `		return 0;` |
|     - | 6304 | `	}` |
|     - | 6305 | `	{` |
|     - | 6306 | `		SySet aSet;` |
|     - | 6307 | `		ph7_class **apIface;` |
|   141 | 6308 | `		ph7_class *pFound = 0;` |
|     - | 6309 | `		sxu32 n;` |
|   141 | 6310 | `		SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));` |
|   141 | 6311 | `		ReflectInterfacesOf(pClass, &aSet);` |
|   141 | 6312 | `		apIface = (ph7_class **)SySetBasePtr(&aSet);` |
|   247 | 6313 | `		for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){` |
|   165 | 6314 | `			if( ReflectFindMethodEntry(apIface[n], zName, nName) ){` |
|    59 | 6315 | `				pFound = apIface[n];` |
|    59 | 6316 | `				break;` |
|     - | 6317 | `			}` |
|    54 | 6318 | `		}` |
|   141 | 6319 | `		SySetRelease(&aSet);` |
|   141 | 6320 | `		return pFound;` |
|     - | 6321 | `	}` |
|   128 | 6322 | `}` |
|     - | 6323 | `/* The same question asked by a ReflectionMethod receiver, which carries the` |
|     - | 6324 | `` * method name on `$this`. */`` |
|    48 | 6325 | `static ph7_class * ReflectMethodPrototype(ph7_context *pCtx, ReflectFuncRef *pRef)` |
|     1 | 6326 | `{` |
|    49 | 6327 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    49 | 6328 | `	const char *zName = "";` |
|    49 | 6329 | `	int nName = 0;` |
|    49 | 6330 | `	if( pThis == 0 ){` |
|   ! 0 | 6331 | `		return 0;` |
|     - | 6332 | `	}` |
|    49 | 6333 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    49 | 6334 | `	return ReflectPrototypeIn(pCtx, pRef->pClass, zName, nName, 1);` |
|    25 | 6335 | `}` |
|    22 | 6336 | `static int vm_builtin_ReflectionMethod_hasPrototype(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6337 | `{` |
|     - | 6338 | `	ReflectFuncRef sRef;` |
|    11 | 6339 | `	SXUNUSED(nArg);` |
|    11 | 6340 | `	SXUNUSED(apArg);` |
|    23 | 6341 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|   ! 0 | 6342 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 6343 | `		return PH7_OK;` |
|     - | 6344 | `	}` |
|    23 | 6345 | `	ph7_result_bool(pCtx, ReflectMethodPrototype(pCtx, &sRef) != 0);` |
|    23 | 6346 | `	return PH7_OK;` |
|    12 | 6347 | `}` |
|    26 | 6348 | `static int vm_builtin_ReflectionMethod_getPrototype(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6349 | `{` |
|    27 | 6350 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 6351 | `	ReflectFuncRef sRef;` |
|     - | 6352 | `	ph7_class *pProto;` |
|    27 | 6353 | `	const char *zClass = "", *zName = "";` |
|    27 | 6354 | `	int nClass = 0, nName = 0;` |
|    13 | 6355 | `	SXUNUSED(nArg);` |
|    13 | 6356 | `	SXUNUSED(apArg);` |
|    27 | 6357 | `	if( pThis == 0 \|\| !ReflectFuncOfThis(pCtx, &sRef) ){` |
|   ! 0 | 6358 | `		ph7_result_null(pCtx);` |
|   ! 0 | 6359 | `		return PH7_OK;` |
|     - | 6360 | `	}` |
|    27 | 6361 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|    27 | 6362 | `	PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    27 | 6363 | `	pProto = ReflectMethodPrototype(pCtx, &sRef);` |
|    27 | 6364 | `	if( pProto == 0 ){` |
|     7 | 6365 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     2 | 6366 | `			"Method %.*s::%.*s does not have a prototype", nClass, zClass, nName, zName);` |
|     - | 6367 | `	}` |
|     - | 6368 | `	{` |
|     - | 6369 | `		ph7_value sClass, sName;` |
|     - | 6370 | `		ph7_value *apCtor[2];` |
|     - | 6371 | `		ph7_class_instance *pOut;` |
|     - | 6372 | `		sxi32 rc;` |
|    23 | 6373 | `		PH7_MemObjInit(pCtx->pVm, &sClass);` |
|    23 | 6374 | `		PH7_MemObjInit(pCtx->pVm, &sName);` |
|    23 | 6375 | `		ph7_value_string(&sClass, SyStringData(&pProto->sName), (int)SyStringLength(&pProto->sName));` |
|    23 | 6376 | `		ph7_value_string(&sName, zName, nName);` |
|    23 | 6377 | `		apCtor[0] = &sClass;` |
|    23 | 6378 | `		apCtor[1] = &sName;` |
|    23 | 6379 | `		pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|    23 | 6380 | `		PH7_MemObjRelease(&sClass);` |
|    23 | 6381 | `		PH7_MemObjRelease(&sName);` |
|    23 | 6382 | `		if( pOut == 0 ){` |
|   ! 0 | 6383 | `			if( rc != PH7_OK ){` |
|   ! 0 | 6384 | `				return rc;` |
|     - | 6385 | `			}` |
|   ! 0 | 6386 | `			ph7_result_null(pCtx);` |
|   ! 0 | 6387 | `			return PH7_OK;` |
|     - | 6388 | `		}` |
|    23 | 6389 | `		return ReflectResultObject(pCtx, pOut);` |
|     - | 6390 | `	}` |
|    14 | 6391 | `}` |
|     - | 6392 | `/* ---- ReflectionParameter ---- */` |
|     - | 6393 | `/*` |
|     - | 6394 | ``  * The function a ReflectionParameter belongs to, from its own `__t`/`__m` `` |
|     - | 6395 | `` * slots. `__t` holds a Closure, a class name or a function name; `__m` the`` |
|     - | 6396 | ` * method name, or null.` |
|     - | 6397 | ` */` |
|   528 | 6398 | `static int ReflectParamOwner(ph7_context *pCtx, ReflectFuncRef *pRef, ReflectParamDesc *pDesc)` |
|     3 | 6399 | `{` |
|   531 | 6400 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 6401 | `	ph7_value *pT, *pM;` |
|     - | 6402 | `	int rc;` |
|   531 | 6403 | `	SyZero(pRef, sizeof(*pRef));` |
|   531 | 6404 | `	if( pDesc ){` |
|   475 | 6405 | `		SyZero(pDesc, sizeof(*pDesc));` |
|   236 | 6406 | `	}` |
|   531 | 6407 | `	if( pThis == 0 ){` |
|   ! 0 | 6408 | `		return 0;` |
|     - | 6409 | `	}` |
|   531 | 6410 | `	pT = PH7_NativeAttr(pThis, RP_T);` |
|   531 | 6411 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|   531 | 6412 | `	if( pT == 0 ){` |
|   ! 0 | 6413 | `		return 0;` |
|     - | 6414 | `	}` |
|   531 | 6415 | `	rc = ReflectFuncFill(pCtx->pVm, pT, (pM && (pM->iFlags & MEMOBJ_STRING)) ? pM : 0, pRef);` |
|   531 | 6416 | `	if( rc == 0 \|\| pDesc == 0 ){` |
|    57 | 6417 | `		return rc;` |
|     - | 6418 | `	}` |
|   475 | 6419 | `	return ReflectParamAt(pRef, (int)PH7_NativeAttrInt(pThis, RP_P), pDesc);` |
|   267 | 6420 | `}` |
|     - | 6421 | `/* ReflectionParameter::__construct($function, string\|int $param) */` |
|   416 | 6422 | `static int vm_builtin_ReflectionParameter_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     4 | 6423 | `{` |
|   420 | 6424 | `	ph7_vm *pVm = pCtx->pVm;` |
|   420 | 6425 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 6426 | `	ReflectFuncRef sRef;` |
|     - | 6427 | `	ReflectParamDesc sDesc;` |
|     - | 6428 | `	ph7_value sTarget, sMethod;` |
|   420 | 6429 | `	int nTotal, iFound = -1, rc = PH7_OK;` |
|   420 | 6430 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|   ! 0 | 6431 | `		return PH7_OK;` |
|     - | 6432 | `	}` |
|   420 | 6433 | `	SyZero(&sDesc, sizeof(sDesc));` |
|   420 | 6434 | `	PH7_MemObjInit(pVm, &sTarget);` |
|   420 | 6435 | `	PH7_MemObjInit(pVm, &sMethod);` |
|     - | 6436 | ``	/* `[$objOrClass, $method]`, `"C::m"` and a plain function/Closure are the`` |
|     - | 6437 | `	 * three spellings php accepts. */` |
|   420 | 6438 | `	if( ph7_value_is_array(apArg[0]) ){` |
|   109 | 6439 | `		ph7_value *pA = ph7_array_fetch(apArg[0], "0", 1);` |
|   109 | 6440 | `		ph7_value *pB = ph7_array_fetch(apArg[0], "1", 1);` |
|   109 | 6441 | `		if( pA && (pA->iFlags & MEMOBJ_OBJ) ){` |
|   ! 0 | 6442 | `			ph7_class_instance *pObj = (ph7_class_instance *)pA->x.pOther;` |
|   ! 0 | 6443 | `			ph7_value_string(&sTarget, SyStringData(&pObj->pClass->sName),` |
|   ! 0 | 6444 | `				(int)SyStringLength(&pObj->pClass->sName));` |
|   109 | 6445 | `		}else if( pA ){` |
|   109 | 6446 | `			PH7_MemObjStore(pA, &sTarget);` |
|    53 | 6447 | `		}` |
|   109 | 6448 | `		if( pB ){` |
|   109 | 6449 | `			PH7_MemObjStore(pB, &sMethod);` |
|    53 | 6450 | `		}` |
|    56 | 6451 | `	}else{` |
|     - | 6452 | ``		/* A STRING is a plain function name, `"C::m"` included: php does not`` |
|     - | 6453 | ``		 * split one here (it reports `Function C::m() does not exist`), unlike`` |
|     - | 6454 | `		 * ReflectionMethod's own one-argument form. The prelude split it and` |
|     - | 6455 | `		 * silently reflected the method. */` |
|   312 | 6456 | `		PH7_MemObjStore(apArg[0], &sTarget);` |
|     - | 6457 | `	}` |
|   420 | 6458 | `	if( !ReflectFuncFill(pCtx->pVm, &sTarget, (sMethod.iFlags & MEMOBJ_STRING) ? &sMethod : 0, &sRef) ){` |
|     - | 6459 | `		const char *zName;` |
|     - | 6460 | `		int nName;` |
|     3 | 6461 | `		if( sMethod.iFlags & MEMOBJ_STRING ){` |
|     - | 6462 | `			const char *zM;` |
|     - | 6463 | `			int nM;` |
|   ! 0 | 6464 | `			zName = ph7_value_to_string(&sTarget, &nName);` |
|   ! 0 | 6465 | `			zM = ph7_value_to_string(&sMethod, &nM);` |
|   ! 0 | 6466 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|   ! 0 | 6467 | `				"Method %.*s::%.*s() does not exist", nName, zName, nM, zM);` |
|   ! 0 | 6468 | `		}else{` |
|     3 | 6469 | `			zName = ph7_value_to_string(&sTarget, &nName);` |
|     4 | 6470 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|     1 | 6471 | `				"Function %.*s() does not exist", nName, zName);` |
|     - | 6472 | `		}` |
|     3 | 6473 | `		goto Done;` |
|     - | 6474 | `	}` |
|   418 | 6475 | `	nTotal = ReflectParamCount(&sRef);` |
|   418 | 6476 | `	if( apArg[1]->iFlags & (MEMOBJ_INT\|MEMOBJ_BOOL) ){` |
|   408 | 6477 | `		int iWant = (int)ph7_value_to_int(apArg[1]);` |
|   408 | 6478 | `		if( iWant >= 0 && iWant < nTotal && ReflectParamAt(&sRef, iWant, &sDesc) ){` |
|   406 | 6479 | `			iFound = iWant;` |
|   201 | 6480 | `		}` |
|   408 | 6481 | `		if( iFound < 0 ){` |
|     3 | 6482 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|     - | 6483 | `				"The parameter specified by its offset could not be found");` |
|     3 | 6484 | `			goto Done;` |
|     - | 6485 | `		}` |
|   205 | 6486 | `	}else{` |
|     - | 6487 | `		const char *zWant;` |
|     - | 6488 | `		int nWant, n;` |
|    11 | 6489 | `		zWant = ph7_value_to_string(apArg[1], &nWant);` |
|    33 | 6490 | `		for( n = 0 ; n < nTotal ; n++ ){` |
|    30 | 6491 | `			if( ReflectParamAt(&sRef, n, &sDesc)` |
|    30 | 6492 | `			 && SyStringLength(&sDesc.sName) == (sxu32)nWant` |
|    23 | 6493 | `			 && SyMemcmp(SyStringData(&sDesc.sName), zWant, (sxu32)nWant) == 0 ){` |
|     9 | 6494 | `				iFound = n;` |
|     9 | 6495 | `				break;` |
|     - | 6496 | `			}` |
|    12 | 6497 | `		}` |
|    11 | 6498 | `		if( iFound < 0 ){` |
|     3 | 6499 | `			rc = PH7_VmThrowException(pCtx, "ReflectionException",` |
|     - | 6500 | `				"The parameter specified by its name could not be found");` |
|     3 | 6501 | `			goto Done;` |
|     - | 6502 | `		}` |
|     - | 6503 | `	}` |
|   619 | 6504 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sDesc.sName),` |
|   410 | 6505 | `		(int)SyStringLength(&sDesc.sName));` |
|     - | 6506 | `	/* Record the CANONICAL target: the class the method really came through and` |
|     - | 6507 | `	 * its declared spelling, so a re-resolve cannot land somewhere else. */` |
|   414 | 6508 | `	if( sRef.pMeth && sRef.pClass ){` |
|   171 | 6509 | `		PH7_NativeSetAttrStr(pVm, pThis, RP_T, SyStringData(&sRef.pClass->sName),` |
|   112 | 6510 | `			(int)SyStringLength(&sRef.pClass->sName));` |
|   171 | 6511 | `		PH7_NativeSetAttrStr(pVm, pThis, RP_M, SyStringData(&sRef.pMeth->sFunc.sName),` |
|   112 | 6512 | `			(int)SyStringLength(&sRef.pMeth->sFunc.sName));` |
|    59 | 6513 | `	}else{` |
|   300 | 6514 | `		ph7_value *pSlot = PH7_NativeAttr(pThis, RP_T);` |
|   300 | 6515 | `		if( pSlot ){` |
|   300 | 6516 | `			PH7_MemObjStore(&sTarget, pSlot);` |
|   149 | 6517 | `		}` |
|     - | 6518 | `	}` |
|   414 | 6519 | `	PH7_NativeSetAttrInt(pVm, pThis, RP_P, (sxi64)iFound);` |
|   208 | 6520 | `Done:` |
|   420 | 6521 | `	PH7_MemObjRelease(&sTarget);` |
|   420 | 6522 | `	PH7_MemObjRelease(&sMethod);` |
|   420 | 6523 | `	return rc;` |
|   212 | 6524 | `}` |
|   260 | 6525 | `static int vm_builtin_ReflectionParameter_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 6526 | `{` |
|   262 | 6527 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   262 | 6528 | `	const char *zName = "";` |
|   262 | 6529 | `	int nName = 0;` |
|   130 | 6530 | `	SXUNUSED(nArg);` |
|   130 | 6531 | `	SXUNUSED(apArg);` |
|   262 | 6532 | `	if( pThis ){` |
|   262 | 6533 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|   130 | 6534 | `	}` |
|   262 | 6535 | `	ph7_result_string(pCtx, zName, nName);` |
|   262 | 6536 | `	return PH7_OK;` |
|     2 | 6537 | `}` |
|    36 | 6538 | `static int vm_builtin_ReflectionParameter_getPosition(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6539 | `{` |
|    37 | 6540 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    18 | 6541 | `	SXUNUSED(nArg);` |
|    18 | 6542 | `	SXUNUSED(apArg);` |
|    37 | 6543 | `	ph7_result_int64(pCtx, pThis ? PH7_NativeAttrInt(pThis, RP_P) : 0);` |
|    37 | 6544 | `	return PH7_OK;` |
|     1 | 6545 | `}` |
|     - | 6546 | `/* The boolean predicates that read one flag of the description. */` |
|   184 | 6547 | `static int ReflectParamFlag(ph7_context *pCtx, int iWhat)` |
|     1 | 6548 | `{` |
|     - | 6549 | `	ReflectFuncRef sRef;` |
|     - | 6550 | `	ReflectParamDesc sDesc;` |
|   185 | 6551 | `	int bYes = 0;` |
|   185 | 6552 | `	if( ReflectParamOwner(pCtx, &sRef, &sDesc) ){` |
|   185 | 6553 | `		switch( iWhat ){` |
|    15 | 6554 | `		case 0: bYes = sDesc.bByRef; break;` |
|     3 | 6555 | `		case 1: bYes = !sDesc.bByRef; break;` |
|    43 | 6556 | `		case 2: bYes = sDesc.bVariadic; break;` |
|   ! 0 | 6557 | `		case 3: bYes = sDesc.bPromoted; break;` |
|    75 | 6558 | `		case 4: bYes = sDesc.bHasDef; break;` |
|    43 | 6559 | `		case 5: bYes = SyStringLength(&sDesc.sType) > 0; break;` |
|     5 | 6560 | `		default:` |
|     - | 6561 | `			/* allowsNull(): untyped, nullable, or a type that INCLUDES null */` |
|    13 | 6562 | `			bYes = SyStringLength(&sDesc.sType) < 1 \|\| sDesc.bNullable` |
|     7 | 6563 | `				\|\| ReflectTypeNameIs(SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType), "mixed")` |
|    14 | 6564 | `				\|\| ReflectTypeNameIs(SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType), "null");` |
|    10 | 6565 | `			break;` |
|     - | 6566 | `		}` |
|    93 | 6567 | `	}else if( iWhat == 1 \|\| iWhat == 6 ){` |
|   ! 0 | 6568 | `		bYes = 1;` |
|   ! 0 | 6569 | `	}` |
|   185 | 6570 | `	ph7_result_bool(pCtx, bYes);` |
|   185 | 6571 | `	return PH7_OK;` |
|     1 | 6572 | `}` |
|     - | 6573 | `#define REFLECT_PARAM_FLAG(NAME,WHAT) \` |
|     - | 6574 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|     - | 6575 | `	{ \` |
|     - | 6576 | `		SXUNUSED(nArg); \` |
|     - | 6577 | `		SXUNUSED(apArg); \` |
|     - | 6578 | `		return ReflectParamFlag(pCtx, WHAT); \` |
|     - | 6579 | `	}` |
|    15 | 6580 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isPassedByReference, 0)` |
|     3 | 6581 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_canBePassedByValue, 1)` |
|    43 | 6582 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isVariadic, 2)` |
|   ! 0 | 6583 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isPromoted, 3)` |
|    75 | 6584 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isDefaultValueAvailable, 4)` |
|    43 | 6585 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_hasType, 5)` |
|    11 | 6586 | `REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_allowsNull, 6)` |
|     - | 6587 |  |
|     - | 6588 | `/* isOptional(): every parameter from here on has to be omissible too. */` |
|    52 | 6589 | `static int vm_builtin_ReflectionParameter_isOptional(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6590 | `{` |
|    53 | 6591 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 6592 | `	ReflectFuncRef sRef;` |
|     - | 6593 | `	ReflectParamDesc sDesc;` |
|     - | 6594 | `	int n, nTotal, iPos;` |
|    26 | 6595 | `	SXUNUSED(nArg);` |
|    26 | 6596 | `	SXUNUSED(apArg);` |
|    53 | 6597 | `	if( pThis == 0 \|\| !ReflectParamOwner(pCtx, &sRef, 0) ){` |
|   ! 0 | 6598 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 6599 | `		return PH7_OK;` |
|     - | 6600 | `	}` |
|    53 | 6601 | `	iPos = (int)PH7_NativeAttrInt(pThis, RP_P);` |
|    53 | 6602 | `	nTotal = ReflectParamCount(&sRef);` |
|   101 | 6603 | `	for( n = iPos ; n < nTotal ; n++ ){` |
|    71 | 6604 | `		if( ReflectParamAt(&sRef, n, &sDesc) && !sDesc.bVariadic && !sDesc.bOptional ){` |
|    23 | 6605 | `			ph7_result_bool(pCtx, 0);` |
|    23 | 6606 | `			return PH7_OK;` |
|     - | 6607 | `		}` |
|    25 | 6608 | `	}` |
|    31 | 6609 | `	ph7_result_bool(pCtx, 1);` |
|    31 | 6610 | `	return PH7_OK;` |
|    27 | 6611 | `}` |
|   186 | 6612 | `static int vm_builtin_ReflectionParameter_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 6613 | `{` |
|     - | 6614 | `	ReflectFuncRef sRef;` |
|     - | 6615 | `	ReflectParamDesc sDesc;` |
|    93 | 6616 | `	SXUNUSED(nArg);` |
|    93 | 6617 | `	SXUNUSED(apArg);` |
|   189 | 6618 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| SyStringLength(&sDesc.sType) < 1 ){` |
|     3 | 6619 | `		ph7_result_null(pCtx);` |
|     3 | 6620 | `		return PH7_OK;` |
|     - | 6621 | `	}` |
|   279 | 6622 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx,` |
|   184 | 6623 | `		SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType)));` |
|    96 | 6624 | `}` |
|     - | 6625 | `/*` |
|     - | 6626 | ` * getClass()/isArray()/isCallable() — php 8.0 deprecated these in favour of` |
|     - | 6627 | ` * getType() but still declares them, still answers, and emits an E_DEPRECATED` |
|     - | 6628 | ` * naming the replacement. PHL fatalled on the call; all three are here now,` |
|     - | 6629 | ` * notice included.` |
|     - | 6630 | ` */` |
|    24 | 6631 | `static void ReflectParamDeprecated(ph7_context *pCtx, const char *zWho)` |
|     1 | 6632 | `{` |
|     - | 6633 | `	char zMsg[160];` |
|    37 | 6634 | `	SyBufferFormat(zMsg, sizeof(zMsg),` |
|     - | 6635 | `		"Method ReflectionParameter::%s() is deprecated since 8.0, "` |
|    12 | 6636 | `		"use ReflectionParameter::getType() instead", zWho);` |
|    25 | 6637 | `	PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED, zMsg);` |
|    25 | 6638 | `}` |
|     8 | 6639 | `static int vm_builtin_ReflectionParameter_getClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6640 | `{` |
|     - | 6641 | `	ReflectFuncRef sRef;` |
|     - | 6642 | `	ReflectParamDesc sDesc;` |
|     - | 6643 | `	const char *zType;` |
|     - | 6644 | `	int nType;` |
|     4 | 6645 | `	SXUNUSED(nArg);` |
|     4 | 6646 | `	SXUNUSED(apArg);` |
|     9 | 6647 | `	ReflectParamDeprecated(pCtx, "getClass");` |
|     9 | 6648 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| SyStringLength(&sDesc.sType) < 1 ){` |
|   ! 0 | 6649 | `		ph7_result_null(pCtx);` |
|   ! 0 | 6650 | `		return PH7_OK;` |
|     - | 6651 | `	}` |
|     9 | 6652 | `	zType = SyStringData(&sDesc.sType);` |
|     9 | 6653 | `	nType = (int)SyStringLength(&sDesc.sType);` |
|     9 | 6654 | `	if( nType > 0 && zType[0] == '?' ){` |
|     3 | 6655 | `		zType++;` |
|     3 | 6656 | `		nType--;` |
|     1 | 6657 | `	}` |
|     9 | 6658 | `	if( ReflectTypeIsBuiltin(zType, nType) ){` |
|     7 | 6659 | `		ph7_result_null(pCtx);` |
|     7 | 6660 | `		return PH7_OK;` |
|     - | 6661 | `	}` |
|     3 | 6662 | `	return ReflectResultClassOf(pCtx, PH7_VmExtractClass(pCtx->pVm, zType, (sxu32)nType, FALSE, 0));` |
|     5 | 6663 | `}` |
|    16 | 6664 | `static int ReflectParamTypeIs(ph7_context *pCtx, const char *zWant)` |
|     1 | 6665 | `{` |
|     - | 6666 | `	ReflectFuncRef sRef;` |
|     - | 6667 | `	ReflectParamDesc sDesc;` |
|    17 | 6668 | `	int bYes = 0;` |
|    17 | 6669 | `	if( ReflectParamOwner(pCtx, &sRef, &sDesc) && SyStringLength(&sDesc.sType) > 0 ){` |
|    17 | 6670 | `		const char *zType = SyStringData(&sDesc.sType);` |
|    17 | 6671 | `		int nType = (int)SyStringLength(&sDesc.sType);` |
|     - | 6672 | ``		/* php answers true for `?array` too: the deprecated pair asks about the`` |
|     - | 6673 | `		 * type ATOM, and nullability is a separate question. */` |
|    17 | 6674 | `		if( zType[0] == '?' ){` |
|     5 | 6675 | `			zType++;` |
|     5 | 6676 | `			nType--;` |
|     2 | 6677 | `		}` |
|    17 | 6678 | `		bYes = ReflectTypeNameIs(zType, nType, zWant);` |
|     8 | 6679 | `	}` |
|    17 | 6680 | `	ph7_result_bool(pCtx, bYes);` |
|    17 | 6681 | `	return PH7_OK;` |
|     1 | 6682 | `}` |
|     8 | 6683 | `static int vm_builtin_ReflectionParameter_isArray(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6684 | `{` |
|     4 | 6685 | `	SXUNUSED(nArg);` |
|     4 | 6686 | `	SXUNUSED(apArg);` |
|     9 | 6687 | `	ReflectParamDeprecated(pCtx, "isArray");` |
|     9 | 6688 | `	return ReflectParamTypeIs(pCtx, "array");` |
|     1 | 6689 | `}` |
|     8 | 6690 | `static int vm_builtin_ReflectionParameter_isCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6691 | `{` |
|     4 | 6692 | `	SXUNUSED(nArg);` |
|     4 | 6693 | `	SXUNUSED(apArg);` |
|     9 | 6694 | `	ReflectParamDeprecated(pCtx, "isCallable");` |
|     9 | 6695 | `	return ReflectParamTypeIs(pCtx, "callable");` |
|     1 | 6696 | `}` |
|    42 | 6697 | `static int vm_builtin_ReflectionParameter_getDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6698 | `{` |
|     - | 6699 | `	ReflectFuncRef sRef;` |
|     - | 6700 | `	ReflectParamDesc sDesc;` |
|    21 | 6701 | `	SXUNUSED(nArg);` |
|    21 | 6702 | `	SXUNUSED(apArg);` |
|    43 | 6703 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|     5 | 6704 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     - | 6705 | `			"Internal error: Failed to retrieve the default value");` |
|     - | 6706 | `	}` |
|    39 | 6707 | `	if( sDesc.pArg ){` |
|     - | 6708 | `		/* Compiled: the same evaluation path the VM uses for an omitted argument */` |
|     - | 6709 | `		ph7_value sValue;` |
|     9 | 6710 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     9 | 6711 | `		VmLocalExec(pCtx->pVm, &sDesc.pArg->aByteCode, &sValue, FALSE);` |
|     9 | 6712 | `		ph7_result_value(pCtx, &sValue);` |
|     9 | 6713 | `		PH7_MemObjRelease(&sValue);` |
|     9 | 6714 | `		return PH7_OK;` |
|     - | 6715 | `	}` |
|     - | 6716 | `	{` |
|     - | 6717 | `		/* Declared: the signature's default TEXT, reduced. */` |
|    31 | 6718 | `		const char *zDef = SyStringData(&sDesc.sDefText);` |
|    31 | 6719 | `		int nDef = (int)SyStringLength(&sDesc.sDefText);` |
|    31 | 6720 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    31 | 6721 | `		if( pVal && ReflectSigScalar(pCtx, zDef, nDef, pVal) ){` |
|    29 | 6722 | `			ph7_result_value(pCtx, pVal);` |
|    29 | 6723 | `			return PH7_OK;` |
|     - | 6724 | `		}` |
|     2 | 6725 | `		if( (nDef >= 1 && zDef[0] == '[')` |
|     2 | 6726 | `		 \|\| (nDef >= (int)sizeof("array (")-1` |
|   ! 0 | 6727 | `		  && SyMemcmp(zDef, "array (", sizeof("array (")-1) == 0) ){` |
|     3 | 6728 | `			ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|     3 | 6729 | `			return PH7_OK;` |
|     - | 6730 | `		}` |
|     - | 6731 | `	}` |
|   ! 0 | 6732 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     - | 6733 | `		"Internal error: Failed to retrieve the default value");` |
|    22 | 6734 | `}` |
|     - | 6735 | `/*` |
|     - | 6736 | ` * A default that is a plain global-constant reference compiles to exactly` |
|     - | 6737 | ` * [ OP_LOADC (EXPAND), OP_DONE ] with the name in the literal table; a` |
|     - | 6738 | ` * DECLARED default is text and never a constant.` |
|     - | 6739 | ` */` |
|    80 | 6740 | `static int ReflectParamDefConst(ph7_context *pCtx, ReflectParamDesc *pDesc,` |
|     - | 6741 | `	const char **pz, int *pn)` |
|     1 | 6742 | `{` |
|     - | 6743 | `	VmInstr *aInstr;` |
|     - | 6744 | `	ph7_value *pLit;` |
|    81 | 6745 | `	if( pDesc->pArg == 0 \|\| SySetUsed(&pDesc->pArg->aByteCode) != 2 ){` |
|    25 | 6746 | `		return 0;` |
|     - | 6747 | `	}` |
|    57 | 6748 | `	aInstr = (VmInstr *)SySetBasePtr(&pDesc->pArg->aByteCode);` |
|    56 | 6749 | `	if( aInstr[0].iOp != PH7_OP_LOADC \|\| (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0` |
|    36 | 6750 | `	 \|\| aInstr[1].iOp != PH7_OP_DONE ){` |
|    43 | 6751 | `		return 0;` |
|     - | 6752 | `	}` |
|    15 | 6753 | `	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);` |
|    15 | 6754 | `	if( pLit == 0 \|\| SyBlobLength(&pLit->sBlob) < 1 ){` |
|   ! 0 | 6755 | `		return 0;` |
|     - | 6756 | `	}` |
|    15 | 6757 | `	*pz = (const char *)SyBlobData(&pLit->sBlob);` |
|    15 | 6758 | `	*pn = (int)SyBlobLength(&pLit->sBlob);` |
|    15 | 6759 | `	return 1;` |
|    41 | 6760 | `}` |
|    18 | 6761 | `static int vm_builtin_ReflectionParameter_isDefaultValueConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6762 | `{` |
|     - | 6763 | `	ReflectFuncRef sRef;` |
|     - | 6764 | `	ReflectParamDesc sDesc;` |
|     - | 6765 | `	const char *z;` |
|     - | 6766 | `	int n;` |
|     9 | 6767 | `	SXUNUSED(nArg);` |
|     9 | 6768 | `	SXUNUSED(apArg);` |
|    19 | 6769 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|     - | 6770 | `		/* php raises here rather than answering false: asking whether a default` |
|     - | 6771 | `		 * is a constant presupposes there IS one. The prelude answered false. */` |
|     3 | 6772 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     - | 6773 | `			"Internal error: Failed to retrieve the default value");` |
|     - | 6774 | `	}` |
|    17 | 6775 | `	ph7_result_bool(pCtx, ReflectParamDefConst(pCtx, &sDesc, &z, &n) != 0);` |
|    17 | 6776 | `	return PH7_OK;` |
|    10 | 6777 | `}` |
|     4 | 6778 | `static int vm_builtin_ReflectionParameter_getDefaultValueConstantName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6779 | `{` |
|     - | 6780 | `	ReflectFuncRef sRef;` |
|     - | 6781 | `	ReflectParamDesc sDesc;` |
|     - | 6782 | `	const char *z;` |
|     - | 6783 | `	int n;` |
|     2 | 6784 | `	SXUNUSED(nArg);` |
|     2 | 6785 | `	SXUNUSED(apArg);` |
|     5 | 6786 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| !sDesc.bHasDef ){` |
|   ! 0 | 6787 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     - | 6788 | `			"Internal error: Failed to retrieve the default value");` |
|     - | 6789 | `	}` |
|     5 | 6790 | `	if( ReflectParamDefConst(pCtx, &sDesc, &z, &n) ){` |
|     5 | 6791 | `		ph7_result_string(pCtx, z, n);` |
|     3 | 6792 | `	}else{` |
|   ! 0 | 6793 | `		ph7_result_null(pCtx);` |
|     - | 6794 | `	}` |
|     5 | 6795 | `	return PH7_OK;` |
|     3 | 6796 | `}` |
|     4 | 6797 | `static int vm_builtin_ReflectionParameter_getDeclaringFunction(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6798 | `{` |
|     5 | 6799 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 6800 | `	ph7_value *pT, *pM;` |
|     - | 6801 | `	ph7_value *apCtor[2];` |
|     - | 6802 | `	ph7_class_instance *pOut;` |
|     - | 6803 | `	sxi32 rc;` |
|     2 | 6804 | `	SXUNUSED(nArg);` |
|     2 | 6805 | `	SXUNUSED(apArg);` |
|     5 | 6806 | `	if( pThis == 0 \|\| (pT = PH7_NativeAttr(pThis, RP_T)) == 0 ){` |
|   ! 0 | 6807 | `		ph7_result_null(pCtx);` |
|   ! 0 | 6808 | `		return PH7_OK;` |
|     - | 6809 | `	}` |
|     5 | 6810 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|     5 | 6811 | `	apCtor[0] = pT;` |
|     5 | 6812 | `	apCtor[1] = pM;` |
|     5 | 6813 | `	if( pM && (pM->iFlags & MEMOBJ_STRING) ){` |
|     3 | 6814 | `		pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|     2 | 6815 | `	}else{` |
|     3 | 6816 | `		pOut = ReflectConstruct(pCtx, "ReflectionFunction", 1, apCtor, &rc);` |
|     - | 6817 | `	}` |
|     5 | 6818 | `	if( pOut == 0 ){` |
|   ! 0 | 6819 | `		if( rc != PH7_OK ){` |
|   ! 0 | 6820 | `			return rc;` |
|     - | 6821 | `		}` |
|   ! 0 | 6822 | `		ph7_result_null(pCtx);` |
|   ! 0 | 6823 | `		return PH7_OK;` |
|     - | 6824 | `	}` |
|     5 | 6825 | `	return ReflectResultObject(pCtx, pOut);` |
|     3 | 6826 | `}` |
|     4 | 6827 | `static int vm_builtin_ReflectionParameter_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6828 | `{` |
|     - | 6829 | `	ReflectFuncRef sRef;` |
|     2 | 6830 | `	SXUNUSED(nArg);` |
|     2 | 6831 | `	SXUNUSED(apArg);` |
|     5 | 6832 | `	if( !ReflectParamOwner(pCtx, &sRef, 0) \|\| sRef.pMeth == 0 ){` |
|     3 | 6833 | `		ph7_result_null(pCtx);` |
|     3 | 6834 | `		return PH7_OK;` |
|     - | 6835 | `	}` |
|     3 | 6836 | `	return ReflectResultClassOf(pCtx, ReflectFuncDeclClass(&sRef));` |
|     3 | 6837 | `}` |
|     4 | 6838 | `static int vm_builtin_ReflectionParameter_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6839 | `{` |
|     5 | 6840 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 6841 | `	ReflectFuncRef sRef;` |
|     - | 6842 | `	ReflectParamDesc sDesc;` |
|     - | 6843 | `	ph7_value *pT, *pM;` |
|     5 | 6844 | `	const char *zMember = 0;` |
|     5 | 6845 | `	int nMember = 0;` |
|     5 | 6846 | `	if( pThis == 0 \|\| !ReflectParamOwner(pCtx, &sRef, &sDesc) \|\| sDesc.pArg == 0 ){` |
|   ! 0 | 6847 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|   ! 0 | 6848 | `		return PH7_OK;` |
|     - | 6849 | `	}` |
|     5 | 6850 | `	pT = PH7_NativeAttr(pThis, RP_T);` |
|     5 | 6851 | `	pM = PH7_NativeAttr(pThis, RP_M);` |
|     5 | 6852 | `	if( pM && (pM->iFlags & MEMOBJ_STRING) ){` |
|     5 | 6853 | `		zMember = (const char *)SyBlobData(&pM->sBlob);` |
|     5 | 6854 | `		nMember = (int)SyBlobLength(&pM->sBlob);` |
|     2 | 6855 | `	}` |
|     - | 6856 | `	/* 32 = Attribute::TARGET_PARAMETER */` |
|     7 | 6857 | `	return ReflectBuildAttrs(pCtx, &sDesc.pArg->aAttrs, "param", pT, zMember, nMember,` |
|     4 | 6858 | `		(int)PH7_NativeAttrInt(pThis, RP_P), 32, nArg, apArg);` |
|     3 | 6859 | `}` |
|    10 | 6860 | `static int vm_builtin_ReflectionParameter_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 6861 | `{` |
|     5 | 6862 | `	SXUNUSED(nArg);` |
|     5 | 6863 | `	SXUNUSED(apArg);` |
|    11 | 6864 | `	return ReflectExportParamSelf(pCtx);` |
|     1 | 6865 | `}` |
|     - | 6866 | `/*` |
|     - | 6867 | ` * Declare chunk 2. Called from PH7_VmInstallReflection() where it used to be` |
|     - | 6868 | `` * compiled — after chunk 1, whose `Reflector` these implement and whose`` |
|     - | 6869 | `` * `ReflectionClass` they answer.`` |
|     - | 6870 | ` *` |
|     - | 6871 | ` * The method tables are in php's own DECLARATION order.` |
|     - | 6872 | ` */` |
|     - | 6873 | `/*` |
|     - | 6874 | `` * PropertyHookType — php's `enum PropertyHookType: string { Get; Set; }`, the`` |
|     - | 6875 | ` * argument ReflectionProperty::getHook()/hasHook() take.` |
|     - | 6876 | ` */` |
|  5146 | 6877 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionHookType(ph7_vm *pVm)` |
|     5 | 6878 | `{` |
|     - | 6879 | `	static const PH7_NativeEnumCase aCase[] = {` |
|     - | 6880 | `		{ "Get", { 0, 0, PH7_NATIVE_VAL_STRING, 0, "get", 0.0 } },` |
|     - | 6881 | `		{ "Set", { 0, 0, PH7_NATIVE_VAL_STRING, 0, "set", 0.0 } },` |
|     - | 6882 | `	};` |
|  5151 | 6883 | `	return PH7_InstallNativeEnum(&(*pVm), "PropertyHookType", MEMOBJ_STRING,` |
|     - | 6884 | `		aCase, SX_ARRAYSIZE(aCase), 0, 0);` |
|     5 | 6885 | `}` |
|  5146 | 6886 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionFunc(ph7_vm *pVm)` |
|     5 | 6887 | `{` |
|     - | 6888 | `	static const PH7_NativePropDef aAbstractProp[] = {` |
|     - | 6889 | `		{ "name",  PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|     - | 6890 | `		/* PHL-only: the Closure being reflected. php reaches the same state from` |
|     - | 6891 | `		 * the function record itself; PHL has no hidden-slot bit yet (§7.4 (e)). */` |
|     - | 6892 | `		{ RF_CL,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 6893 | `	};` |
|     - | 6894 | `	static const PH7_NativeMethodDef aAbstractMethod[] = {` |
|     - | 6895 | `		{ "__clone",       PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|     - | 6896 | `		{ "inNamespace",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_inNamespace },` |
|     - | 6897 | `		{ "isClosure",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isClosure },` |
|     - | 6898 | `		{ "isDeprecated",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isDeprecated },` |
|     - | 6899 | `		{ "isInternal",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isInternal },` |
|     - | 6900 | `		{ "isUserDefined", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isUserDefined },` |
|     - | 6901 | `		{ "isGenerator",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isGenerator },` |
|     - | 6902 | `		{ "isVariadic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isVariadic },` |
|     - | 6903 | `		{ "isStatic",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isStatic },` |
|     - | 6904 | `		{ "getClosureThis", PH7_MOD_PUBLIC, "", "@?object", vm_builtin_ReflectionFunc_getClosureThis },` |
|     - | 6905 | `		{ "getClosureScopeClass", PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|     - | 6906 | `		  vm_builtin_ReflectionFunc_getClosureScopeClass },` |
|     - | 6907 | `		/* php answers the same class for both; PHL has no separate called-scope. */` |
|     - | 6908 | `		{ "getClosureCalledClass", PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|     - | 6909 | `		  vm_builtin_ReflectionFunc_getClosureCalledClass },` |
|     - | 6910 | `		{ "getClosureUsedVariables", PH7_MOD_PUBLIC, "", "array",` |
|     - | 6911 | `		  vm_builtin_ReflectionFunc_getClosureUsedVariables },` |
|     - | 6912 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getDocComment },` |
|     - | 6913 | `		{ "getEndLine",    PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionFunc_getEndLine },` |
|     - | 6914 | `		{ "getExtension",  PH7_MOD_PUBLIC, "", "@?ReflectionExtension", vm_builtin_ReflectionFunc_getExtension },` |
|     - | 6915 | `		{ "getExtensionName", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getExtensionName },` |
|     - | 6916 | `		{ "getFileName",   PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionFunc_getFileName },` |
|     - | 6917 | `		{ "getName",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getName },` |
|     - | 6918 | `		{ "getNamespaceName", PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getNamespaceName },` |
|     - | 6919 | `		{ "getNumberOfParameters", PH7_MOD_PUBLIC, "", "@int",` |
|     - | 6920 | `		  vm_builtin_ReflectionFunc_getNumberOfParameters },` |
|     - | 6921 | `		{ "getNumberOfRequiredParameters", PH7_MOD_PUBLIC, "", "@int",` |
|     - | 6922 | `		  vm_builtin_ReflectionFunc_getNumberOfRequiredParameters },` |
|     - | 6923 | `		{ "getParameters", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionFunc_getParameters },` |
|     - | 6924 | `		{ "getShortName",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getShortName },` |
|     - | 6925 | `		{ "getStartLine",  PH7_MOD_PUBLIC, "", "@int\|false", vm_builtin_ReflectionFunc_getStartLine },` |
|     - | 6926 | `		{ "getStaticVariables", PH7_MOD_PUBLIC, "", "@array",` |
|     - | 6927 | `		  vm_builtin_ReflectionFunc_getStaticVariables },` |
|     - | 6928 | `		{ "returnsReference", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_returnsReference },` |
|     - | 6929 | `		{ "hasReturnType", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_hasReturnType },` |
|     - | 6930 | `		{ "getReturnType", PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionFunc_getReturnType },` |
|     - | 6931 | `		{ "hasTentativeReturnType", PH7_MOD_PUBLIC, "", "bool",` |
|     - | 6932 | `		  vm_builtin_ReflectionFunc_hasTentativeReturnType },` |
|     - | 6933 | `		{ "getTentativeReturnType", PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|     - | 6934 | `		  vm_builtin_ReflectionFunc_getTentativeReturnType },` |
|     - | 6935 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|     - | 6936 | `		  vm_builtin_ReflectionFunc_getAttributes },` |
|     - | 6937 | `	};` |
|     - | 6938 | `	static const PH7_NativeConstDef aFunctionConst[] = {` |
|     - | 6939 | `		{ "IS_DEPRECATED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2048, 0, 0.0 },` |
|     - | 6940 | `	};` |
|     - | 6941 | `	static const PH7_NativeMethodDef aFunctionMethod[] = {` |
|     - | 6942 | `		{ "__construct", PH7_MOD_PUBLIC, "Closure\|string $function", "",` |
|     - | 6943 | `		  vm_builtin_ReflectionFunction_construct },` |
|     - | 6944 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionFunc_toString },` |
|     - | 6945 | `		{ "isAnonymous", PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionFunction_isAnonymous },` |
|     - | 6946 | `		{ "isDisabled",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunction_isDisabled },` |
|     - | 6947 | `		{ "invoke",      PH7_MOD_PUBLIC, "mixed ...$args", "@mixed",` |
|     - | 6948 | `		  vm_builtin_ReflectionFunction_invoke },` |
|     - | 6949 | `		{ "invokeArgs",  PH7_MOD_PUBLIC, "array $args", "@mixed",` |
|     - | 6950 | `		  vm_builtin_ReflectionFunction_invokeArgs },` |
|     - | 6951 | `		{ "getClosure",  PH7_MOD_PUBLIC, "", "@Closure", vm_builtin_ReflectionFunction_getClosure },` |
|     - | 6952 | `	};` |
|     - | 6953 | `	static const PH7_NativePropDef aMethodProp[] = {` |
|     - | 6954 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|     - | 6955 | `	};` |
|     - | 6956 | `	static const PH7_NativeConstDef aMethodConst[] = {` |
|     - | 6957 | `		{ "IS_STATIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16, 0, 0.0 },` |
|     - | 6958 | `		{ "IS_PUBLIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,  0, 0.0 },` |
|     - | 6959 | `		{ "IS_PROTECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,  0, 0.0 },` |
|     - | 6960 | `		{ "IS_PRIVATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,  0, 0.0 },` |
|     - | 6961 | `		{ "IS_ABSTRACT",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64, 0, 0.0 },` |
|     - | 6962 | `		{ "IS_FINAL",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|     - | 6963 | `	};` |
|     - | 6964 | `	static const PH7_NativeMethodDef aMethodMethod[] = {` |
|     - | 6965 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $objectOrMethod, ?string $method = null", "",` |
|     - | 6966 | `		  vm_builtin_ReflectionMethod_construct },` |
|     - | 6967 | `		{ "createFromMethodName", PH7_MOD_PUBLIC\|PH7_MOD_STATIC, "string $method", "static",` |
|     - | 6968 | `		  vm_builtin_ReflectionMethod_createFromMethodName },` |
|     - | 6969 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionFunc_toString },` |
|     - | 6970 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isPublic },` |
|     - | 6971 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isPrivate },` |
|     - | 6972 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isProtected },` |
|     - | 6973 | `		{ "isAbstract",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isAbstract },` |
|     - | 6974 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isFinal },` |
|     - | 6975 | `		{ "isConstructor", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isConstructor },` |
|     - | 6976 | `		{ "isDestructor",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isDestructor },` |
|     - | 6977 | `		{ "getClosure",  PH7_MOD_PUBLIC, "?object $object = null", "@Closure",` |
|     - | 6978 | `		  vm_builtin_ReflectionMethod_getClosure },` |
|     - | 6979 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionMethod_getModifiers },` |
|     - | 6980 | `		{ "invoke",      PH7_MOD_PUBLIC, "?object $object, mixed ...$args", "@mixed",` |
|     - | 6981 | `		  vm_builtin_ReflectionMethod_invoke },` |
|     - | 6982 | `		{ "invokeArgs",  PH7_MOD_PUBLIC, "?object $object, array $args", "@mixed",` |
|     - | 6983 | `		  vm_builtin_ReflectionMethod_invokeArgs },` |
|     - | 6984 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|     - | 6985 | `		  vm_builtin_ReflectionMethod_getDeclaringClass },` |
|     - | 6986 | `		{ "getPrototype", PH7_MOD_PUBLIC, "", "@ReflectionMethod", vm_builtin_ReflectionMethod_getPrototype },` |
|     - | 6987 | `		{ "hasPrototype", PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionMethod_hasPrototype },` |
|     - | 6988 | `		{ "setAccessible", PH7_MOD_PUBLIC, "bool $accessible", "@void",` |
|     - | 6989 | `		  vm_builtin_ReflectionMethod_setAccessible },` |
|     - | 6990 | `	};` |
|     - | 6991 | `	static const PH7_NativePropDef aParamProp[] = {` |
|     - | 6992 | `		{ "name", PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|     - | 6993 | `		/* PHL-only, the three that identify the parameter (§7.4 (e)) */` |
|     - | 6994 | `		{ RP_T,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 6995 | `		{ RP_M,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 6996 | `		{ RP_P,   PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },` |
|     - | 6997 | `	};` |
|     - | 6998 | `	static const PH7_NativeMethodDef aParamMethod[] = {` |
|     - | 6999 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|     - | 7000 | `		/* php leaves $function UNTYPED here: it takes a name, a Closure, a` |
|     - | 7001 | ``		 * `[$obj, 'm']` pair or "C::m", and no declarable union covers all four. */`` |
|     - | 7002 | `		{ "__construct", PH7_MOD_PUBLIC, "$function, string\|int $param", "",` |
|     - | 7003 | `		  vm_builtin_ReflectionParameter_construct },` |
|     - | 7004 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionParameter_toString },` |
|     - | 7005 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionParameter_getName },` |
|     - | 7006 | `		{ "isPassedByReference", PH7_MOD_PUBLIC, "", "@bool",` |
|     - | 7007 | `		  vm_builtin_ReflectionParameter_isPassedByReference },` |
|     - | 7008 | `		{ "canBePassedByValue",  PH7_MOD_PUBLIC, "", "@bool",` |
|     - | 7009 | `		  vm_builtin_ReflectionParameter_canBePassedByValue },` |
|     - | 7010 | `		{ "getDeclaringFunction", PH7_MOD_PUBLIC, "", "@ReflectionFunctionAbstract",` |
|     - | 7011 | `		  vm_builtin_ReflectionParameter_getDeclaringFunction },` |
|     - | 7012 | `		{ "getDeclaringClass",   PH7_MOD_PUBLIC, "", "@?ReflectionClass",` |
|     - | 7013 | `		  vm_builtin_ReflectionParameter_getDeclaringClass },` |
|     - | 7014 | `		{ "getClass",    PH7_MOD_PUBLIC, "", "@?ReflectionClass", vm_builtin_ReflectionParameter_getClass },` |
|     - | 7015 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_hasType },` |
|     - | 7016 | `		{ "getType",     PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionParameter_getType },` |
|     - | 7017 | `		{ "isArray",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isArray },` |
|     - | 7018 | `		{ "isCallable",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isCallable },` |
|     - | 7019 | `		{ "allowsNull",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_allowsNull },` |
|     - | 7020 | `		{ "getPosition", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionParameter_getPosition },` |
|     - | 7021 | `		{ "isOptional",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isOptional },` |
|     - | 7022 | `		{ "isDefaultValueAvailable", PH7_MOD_PUBLIC, "", "@bool",` |
|     - | 7023 | `		  vm_builtin_ReflectionParameter_isDefaultValueAvailable },` |
|     - | 7024 | `		{ "getDefaultValue", PH7_MOD_PUBLIC, "", "@mixed",` |
|     - | 7025 | `		  vm_builtin_ReflectionParameter_getDefaultValue },` |
|     - | 7026 | `		{ "isDefaultValueConstant", PH7_MOD_PUBLIC, "", "@bool",` |
|     - | 7027 | `		  vm_builtin_ReflectionParameter_isDefaultValueConstant },` |
|     - | 7028 | `		{ "getDefaultValueConstantName", PH7_MOD_PUBLIC, "", "@?string",` |
|     - | 7029 | `		  vm_builtin_ReflectionParameter_getDefaultValueConstantName },` |
|     - | 7030 | `		{ "isVariadic",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isVariadic },` |
|     - | 7031 | `		{ "isPromoted",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionParameter_isPromoted },` |
|     - | 7032 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|     - | 7033 | `		  vm_builtin_ReflectionParameter_getAttributes },` |
|     - | 7034 | `	};` |
|     - | 7035 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 7036 | `		{ "ReflectionFunctionAbstract", 0, "Reflector", PH7_CLASS_ABSTRACT\|PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 7037 | `		  aAbstractMethod, SX_ARRAYSIZE(aAbstractMethod), 0, 0,` |
|     - | 7038 | `		  aAbstractProp, SX_ARRAYSIZE(aAbstractProp), 0, 0, 0 },` |
|     - | 7039 | `		{ "ReflectionFunction", "ReflectionFunctionAbstract", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 7040 | `		  aFunctionMethod, SX_ARRAYSIZE(aFunctionMethod),` |
|     - | 7041 | `		  aFunctionConst, SX_ARRAYSIZE(aFunctionConst), 0, 0, 0, 0, 0 },` |
|     - | 7042 | `		{ "ReflectionMethod", "ReflectionFunctionAbstract", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 7043 | `		  aMethodMethod, SX_ARRAYSIZE(aMethodMethod),` |
|     - | 7044 | `		  aMethodConst, SX_ARRAYSIZE(aMethodConst),` |
|     - | 7045 | `		  aMethodProp, SX_ARRAYSIZE(aMethodProp), 0, 0, 0 },` |
|     - | 7046 | `		{ "ReflectionParameter", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 7047 | `		  aParamMethod, SX_ARRAYSIZE(aParamMethod), 0, 0,` |
|     - | 7048 | `		  aParamProp, SX_ARRAYSIZE(aParamProp), 0, 0, 0 },` |
|     - | 7049 | `	};` |
|  5151 | 7050 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|     5 | 7051 | `}` |
|     - | 7052 | `/*` |
|     - | 7053 | ` * ---------------------------------------------------------------------------` |
|     - | 7054 | ` * ReflectionProperty and ReflectionClassConstant.` |
|     - | 7055 | ` *` |
|     - | 7056 | ` * Chunk 3, the last of the three that made up "the core". Seven thunks retire` |
|     - | 7057 | ` * with it — __reflect_prop_read, __reflect_prop_write, __reflect_prop_state,` |
|     - | 7058 | ` * __reflect_prop_default, __reflect_static_value, __reflect_static_set and` |
|     - | 7059 | ` * __reflect_const_value — because a native method reaches an instance's slot` |
|     - | 7060 | ` * table, a static's shared slot and a constant's lazy slot directly.` |
|     - | 7061 | ` * ---------------------------------------------------------------------------` |
|     - | 7062 | ` */` |
|     - | 7063 | `#define RP_DYNOBJ "__dynobj"   /* the instance a DYNAMIC property lives on */` |
|     - | 7064 |  |
|     - | 7065 | `/* skipLazyInitialization() is a no-op on an object that is not lazy, which is` |
|     - | 7066 | ` * every object PHL can build — so it answers what php answers rather than` |
|     - | 7067 | ` * refusing (§7.4). */` |
|   ! 0 | 7068 | `static int vm_builtin_ReflectionProperty_noop(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 7069 | `{` |
|   ! 0 | 7070 | `	SXUNUSED(pCtx);` |
|   ! 0 | 7071 | `	SXUNUSED(nArg);` |
|   ! 0 | 7072 | `	SXUNUSED(apArg);` |
|   ! 0 | 7073 | `	return PH7_OK;` |
|   ! 0 | 7074 | `}` |
|     - | 7075 | `/*` |
|     - | 7076 | `` * A STATIC property's shared slot, read and written the way `C::$s` is: the`` |
|     - | 7077 | ` * class's static table materializes first (a default that threw at the` |
|     - | 7078 | ` * declaration raises HERE), and an uninitialized typed static is php's Error.` |
|     - | 7079 | ` */` |
|     6 | 7080 | `static int ReflectStaticSlotRead(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr)` |
|     2 | 7081 | `{` |
|     - | 7082 | `	SyHashEntry *pSlot;` |
|     - | 7083 | `	ph7_value *pVal;` |
|     8 | 7084 | `	sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|     8 | 7085 | `	if( rc != SXRET_OK ){` |
|     3 | 7086 | `		return rc;` |
|     - | 7087 | `	}` |
|     5 | 7088 | `	pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|     5 | 7089 | `	if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|   ! 0 | 7090 | `		ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|   ! 0 | 7091 | `		return PH7_VmThrowException(pCtx, "Error",` |
|     - | 7092 | `			"Typed static property %z::$%z must not be accessed before initialization",` |
|   ! 0 | 7093 | `			&pDecl->sName, &pAttr->sName);` |
|     - | 7094 | `	}` |
|     5 | 7095 | `	pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|     5 | 7096 | `	if( pVal ){` |
|     5 | 7097 | `		ph7_result_value(pCtx, pVal);` |
|     3 | 7098 | `	}else{` |
|   ! 0 | 7099 | `		ph7_result_null(pCtx);` |
|     - | 7100 | `	}` |
|     5 | 7101 | `	return PH7_OK;` |
|     5 | 7102 | `}` |
|     4 | 7103 | `static int ReflectStaticSlotWrite(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr,` |
|     - | 7104 | `	ph7_value *pValue)` |
|     2 | 7105 | `{` |
|     - | 7106 | `	ph7_value *pSlot;` |
|     6 | 7107 | `	sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);` |
|     6 | 7108 | `	if( rc != SXRET_OK ){` |
|     3 | 7109 | `		return rc;` |
|     - | 7110 | `	}` |
|     3 | 7111 | `	rc = ReflectEnforceStore(pCtx, pAttr->nIdx, pValue);` |
|     3 | 7112 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 7113 | `		return rc;` |
|     - | 7114 | `	}` |
|     3 | 7115 | `	pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|     3 | 7116 | `	if( pSlot ){` |
|     3 | 7117 | `		PH7_MemObjStore(pValue, pSlot);` |
|     1 | 7118 | `	}` |
|     3 | 7119 | `	return PH7_OK;` |
|     4 | 7120 | `}` |
|     - | 7121 |  |
|     - | 7122 | `/* What a ReflectionProperty / ReflectionClassConstant is looking at. */` |
|     - | 7123 | `typedef struct ReflectMemberRef ReflectMemberRef;` |
|     - | 7124 | `struct ReflectMemberRef` |
|     - | 7125 | `{` |
|     - | 7126 | `	ph7_class *pClass;            /* the reflected class */` |
|     - | 7127 | `	ph7_class_attr *pAttr;        /* the declared member (NULL for a dynamic property) */` |
|     - | 7128 | `	ph7_class_instance *pDynObj;  /* the instance a dynamic property lives on */` |
|     - | 7129 | `	const char *zName;            /* the member's name (borrowed from the slot) */` |
|     - | 7130 | `	int nName;` |
|     - | 7131 | `};` |
|     - | 7132 | `/*` |
|     - | 7133 | `` * Resolve `$this->class` + `$this->name` into the member. Uses the LISTING`` |
|     - | 7134 | ` * answer of the member walk, which is php's: a base class's PRIVATE member is` |
|     - | 7135 | ``  * not reachable by name from the subclass (`new ReflectionProperty('B','pp')` `` |
|     - | 7136 | `` * raises where `B extends A` and `A::$pp` is private).`` |
|     - | 7137 | ` */` |
|   464 | 7138 | `static int ReflectMemberOfThis(ph7_context *pCtx, ReflectMemberRef *pOut, int iKind)` |
|     4 | 7139 | `{` |
|   468 | 7140 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 7141 | `	const char *zClass;` |
|     - | 7142 | `	int nClass;` |
|     - | 7143 | `	SySet aMembers;` |
|     - | 7144 | `	sxu32 n;` |
|   468 | 7145 | `	SyZero(pOut, sizeof(*pOut));` |
|   468 | 7146 | `	if( pThis == 0 ){` |
|   ! 0 | 7147 | `		return 0;` |
|     - | 7148 | `	}` |
|   468 | 7149 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|   468 | 7150 | `	PH7_NativeAttrStr(pThis, "name", &pOut->zName, &pOut->nName);` |
|   468 | 7151 | `	if( nClass < 1 ){` |
|   ! 0 | 7152 | `		return 0;` |
|     - | 7153 | `	}` |
|   468 | 7154 | `	pOut->pClass = PH7_VmExtractClass(pCtx->pVm, zClass, (sxu32)nClass, FALSE, 0);` |
|   468 | 7155 | `	if( pOut->pClass == 0 ){` |
|   ! 0 | 7156 | `		return 0;` |
|     - | 7157 | `	}` |
|   468 | 7158 | `	if( iKind == REFLECT_MEMBER_PROP ){` |
|   356 | 7159 | `		pOut->pDynObj = PH7_NativeAttrObj(pThis, RP_DYNOBJ);` |
|   176 | 7160 | `	}` |
|   468 | 7161 | `	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));` |
|   468 | 7162 | `	ReflectMembers(pCtx->pVm, pOut->pClass, &aMembers, 0);` |
|  1752 | 7163 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|  1746 | 7164 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|  1746 | 7165 | `		if( pM->iKind == iKind && ReflectKeyIs(pM, pOut->zName, pOut->nName) ){` |
|   462 | 7166 | `			pOut->pAttr = pM->pAttr;` |
|   462 | 7167 | `			break;` |
|     - | 7168 | `		}` |
|   646 | 7169 | `	}` |
|   468 | 7170 | `	SySetRelease(&aMembers);` |
|   468 | 7171 | `	return pOut->pAttr != 0 \|\| pOut->pDynObj != 0;` |
|   236 | 7172 | `}` |
|     - | 7173 | `/* The class php reports as the member's declarer. */` |
|    32 | 7174 | `static ph7_class * ReflectMemberDecl(const ReflectMemberRef *pRef)` |
|     3 | 7175 | `{` |
|    35 | 7176 | `	if( pRef->pAttr && pRef->pAttr->pDeclClass ){` |
|    35 | 7177 | `		return pRef->pAttr->pDeclClass;` |
|     - | 7178 | `	}` |
|   ! 0 | 7179 | `	return pRef->pClass;` |
|    19 | 7180 | `}` |
|     - | 7181 | `/* The instance slot record for a dynamic (or any instance-owned) property. */` |
|    54 | 7182 | `static VmClassAttr * ReflectInstanceAttr(ph7_class_instance *pObj, const char *zName, int nName)` |
|     1 | 7183 | `{` |
|     - | 7184 | `	SyHashEntry *pEntry;` |
|    55 | 7185 | `	if( pObj == 0 \|\| nName < 1 ){` |
|     5 | 7186 | `		return 0;` |
|     - | 7187 | `	}` |
|    51 | 7188 | `	pEntry = SyHashGet(&pObj->hAttr, (const void *)zName, (sxu32)nName);` |
|    51 | 7189 | `	return pEntry ? (VmClassAttr *)pEntry->pUserData : 0;` |
|    28 | 7190 | `}` |
|     - | 7191 | `/* ---- ReflectionProperty ---- */` |
|     - | 7192 | `/* ReflectionProperty::__construct(object\|string $class, string $property) */` |
|   222 | 7193 | `static int vm_builtin_ReflectionProperty_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     5 | 7194 | `{` |
|   227 | 7195 | `	ph7_vm *pVm = pCtx->pVm;` |
|   227 | 7196 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|   227 | 7197 | `	ph7_class_instance *pObj = 0;` |
|     - | 7198 | `	ph7_class *pClass;` |
|     - | 7199 | `	const char *zProp;` |
|     - | 7200 | `	int nProp;` |
|     - | 7201 | `	SySet aMembers;` |
|     - | 7202 | `	sxu32 n;` |
|   227 | 7203 | `	int bFound = 0;` |
|   227 | 7204 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|   ! 0 | 7205 | `		return PH7_OK;` |
|     - | 7206 | `	}` |
|   227 | 7207 | `	if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|     7 | 7208 | `		pObj = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     3 | 7209 | `	}` |
|   227 | 7210 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|   227 | 7211 | `	if( pClass == 0 ){` |
|     - | 7212 | `		const char *zName;` |
|     - | 7213 | `		int nName;` |
|     3 | 7214 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|     4 | 7215 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     1 | 7216 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|     - | 7217 | `	}` |
|   225 | 7218 | `	zProp = ph7_value_to_string(apArg[1], &nProp);` |
|   225 | 7219 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|   225 | 7220 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|   737 | 7221 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   727 | 7222 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   727 | 7223 | `		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zProp, nProp) ){` |
|     - | 7224 | `			/* php's $class is the DECLARING class, not the one asked about. */` |
|   215 | 7225 | `			pClass = pM->pDecl ? pM->pDecl : pClass;` |
|   215 | 7226 | `			bFound = 1;` |
|   215 | 7227 | `			break;` |
|     - | 7228 | `		}` |
|   260 | 7229 | `	}` |
|   225 | 7230 | `	SySetRelease(&aMembers);` |
|   335 | 7231 | `	PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pClass->sName),` |
|   220 | 7232 | `		(int)SyStringLength(&pClass->sName));` |
|   225 | 7233 | `	if( bFound ){` |
|   215 | 7234 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", zProp, nProp);` |
|   215 | 7235 | `		return PH7_OK;` |
|     - | 7236 | `	}` |
|     - | 7237 | `	/* Not declared: an OBJECT may still own it as a dynamic property. */` |
|    11 | 7238 | `	if( ReflectInstanceAttr(pObj, zProp, nProp) ){` |
|     7 | 7239 | `		PH7_NativeSetAttrStr(pVm, pThis, "name", zProp, nProp);` |
|     7 | 7240 | `		PH7_NativeSetAttrObj(pVm, pThis, RP_DYNOBJ, pObj);` |
|     7 | 7241 | `		return PH7_OK;` |
|     - | 7242 | `	}` |
|     7 | 7243 | `	return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     2 | 7244 | `		"Property %z::$%.*s does not exist", &pClass->sName, nProp, zProp);` |
|   116 | 7245 | `}` |
|    82 | 7246 | `static int vm_builtin_ReflectionProperty_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 7247 | `{` |
|    84 | 7248 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    84 | 7249 | `	const char *zName = "";` |
|    84 | 7250 | `	int nName = 0;` |
|    41 | 7251 | `	SXUNUSED(nArg);` |
|    41 | 7252 | `	SXUNUSED(apArg);` |
|    84 | 7253 | `	if( pThis ){` |
|    84 | 7254 | `		PH7_NativeAttrStr(pThis, "name", &zName, &nName);` |
|    41 | 7255 | `	}` |
|    84 | 7256 | `	ph7_result_string(pCtx, zName, nName);` |
|    84 | 7257 | `	return PH7_OK;` |
|     2 | 7258 | `}` |
|     - | 7259 | `/*` |
|     - | 7260 | ` * getMangledName(): the name php stores the slot under —  plain for a public` |
|     - | 7261 | ` * property, "\0*\0name" for a protected one and "\0Class\0name" for a private` |
|     - | 7262 | ` * one. PHL's tables are not mangled, so the name is COMPOSED here; what php` |
|     - | 7263 | ` * exposes is the spelling, not the storage.` |
|     - | 7264 | ` */` |
|     6 | 7265 | `static int vm_builtin_ReflectionProperty_getMangledName(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7266 | `{` |
|     - | 7267 | `	ReflectMemberRef sRef;` |
|     - | 7268 | `	SyBlob sOut;` |
|     3 | 7269 | `	SXUNUSED(nArg);` |
|     3 | 7270 | `	SXUNUSED(apArg);` |
|     7 | 7271 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|   ! 0 | 7272 | `		ph7_result_string(pCtx, sRef.zName, sRef.nName);` |
|   ! 0 | 7273 | `		return PH7_OK;` |
|     - | 7274 | `	}` |
|     7 | 7275 | `	if( sRef.pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){` |
|     3 | 7276 | `		ph7_result_string(pCtx, sRef.zName, sRef.nName);` |
|     3 | 7277 | `		return PH7_OK;` |
|     - | 7278 | `	}` |
|     5 | 7279 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     5 | 7280 | `	SyBlobAppend(&sOut, "\0", 1);` |
|     5 | 7281 | `	if( sRef.pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){` |
|     3 | 7282 | `		SyBlobAppend(&sOut, "*", 1);` |
|     2 | 7283 | `	}else{` |
|     3 | 7284 | `		ph7_class *pDecl = ReflectMemberDecl(&sRef);` |
|     3 | 7285 | `		SyBlobAppend(&sOut, SyStringData(&pDecl->sName), SyStringLength(&pDecl->sName));` |
|     - | 7286 | `	}` |
|     5 | 7287 | `	SyBlobAppend(&sOut, "\0", 1);` |
|     5 | 7288 | `	SyBlobAppend(&sOut, sRef.zName, (sxu32)sRef.nName);` |
|     5 | 7289 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     5 | 7290 | `	SyBlobRelease(&sOut);` |
|     5 | 7291 | `	return PH7_OK;` |
|     4 | 7292 | `}` |
|     - | 7293 | `/* The boolean predicates, all off the declared attribute. */` |
|    42 | 7294 | `static int ReflectPropFlag(ph7_context *pCtx, int iWhat)` |
|     1 | 7295 | `{` |
|     - | 7296 | `	ReflectMemberRef sRef;` |
|    43 | 7297 | `	int bYes = 0;` |
|    62 | 7298 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|    39 | 7299 | `		ph7_class_attr *pAttr = sRef.pAttr;` |
|    39 | 7300 | `		switch( iWhat ){` |
|   ! 0 | 7301 | `		case 0: bYes = pAttr->iProtection == PH7_CLASS_PROT_PUBLIC; break;` |
|   ! 0 | 7302 | `		case 1: bYes = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE; break;` |
|     3 | 7303 | `		case 2: bYes = pAttr->iProtection == PH7_CLASS_PROT_PROTECTED; break;` |
|     7 | 7304 | `		case 3: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) != 0; break;` |
|     7 | 7305 | `		case 4: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET) != 0; break;` |
|     3 | 7306 | `		case 5: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0; break;` |
|     9 | 7307 | `		case 6: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0; break;` |
|     3 | 7308 | `		case 7: bYes = 1; break;                                    /* isDefault */` |
|   ! 0 | 7309 | `		case 8: bYes = 0; break;                                    /* isDynamic */` |
|     7 | 7310 | `		case 9: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) != 0; break;` |
|     3 | 7311 | `		default:` |
|     7 | 7312 | `			bYes = (pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) != 0;` |
|     6 | 7313 | `			break;` |
|     - | 7314 | `		}` |
|    24 | 7315 | `	}else if( iWhat == 0 \|\| iWhat == 8 ){` |
|     - | 7316 | `		/* A dynamic property is public and, by definition, not a default one. */` |
|     3 | 7317 | `		bYes = 1;` |
|     1 | 7318 | `	}` |
|    43 | 7319 | `	ph7_result_bool(pCtx, bYes);` |
|    43 | 7320 | `	return PH7_OK;` |
|     1 | 7321 | `}` |
|     - | 7322 | `#define REFLECT_PROP_FLAG(NAME,WHAT) \` |
|     - | 7323 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|     - | 7324 | `	{ \` |
|     - | 7325 | `		SXUNUSED(nArg); \` |
|     - | 7326 | `		SXUNUSED(apArg); \` |
|     - | 7327 | `		return ReflectPropFlag(pCtx, WHAT); \` |
|     - | 7328 | `	}` |
|   ! 0 | 7329 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPublic, 0)` |
|   ! 0 | 7330 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPrivate, 1)` |
|     3 | 7331 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isProtected, 2)` |
|     7 | 7332 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPrivateSet, 3)` |
|     7 | 7333 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isProtectedSet, 4)` |
|     3 | 7334 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isStatic, 5)` |
|     9 | 7335 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isReadOnly, 6)` |
|     5 | 7336 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isDefault, 7)` |
|     3 | 7337 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isDynamic, 8)` |
|     7 | 7338 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isVirtual, 9)` |
|     7 | 7339 | `REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_hasHooks, 10)` |
|     - | 7340 |  |
|     - | 7341 | `/* php has no abstract properties outside an interface stub, and PHL none at` |
|     - | 7342 | ` * all; isLazy() answers what is true of a VM without lazy objects. */` |
|   ! 0 | 7343 | `static int vm_builtin_ReflectionProperty_false(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 7344 | `{` |
|   ! 0 | 7345 | `	SXUNUSED(nArg);` |
|   ! 0 | 7346 | `	SXUNUSED(apArg);` |
|   ! 0 | 7347 | `	ph7_result_bool(pCtx, 0);` |
|   ! 0 | 7348 | `	return PH7_OK;` |
|   ! 0 | 7349 | `}` |
|     - | 7350 | `/*` |
|     - | 7351 | ` * isPromoted(): the property came from a constructor-promoted parameter. PHL` |
|     - | 7352 | ` * records promotion on the ARGUMENT (VM_FUNC_ARG_PROMOTED), not on the` |
|     - | 7353 | ` * attribute, so the constructor's parameter list is what answers.` |
|     - | 7354 | ` */` |
|     4 | 7355 | `static int vm_builtin_ReflectionProperty_isPromoted(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7356 | `{` |
|     - | 7357 | `	ReflectMemberRef sRef;` |
|     - | 7358 | `	ph7_class_method *pCons;` |
|     - | 7359 | `	ph7_vm_func_arg *aArg;` |
|     - | 7360 | `	sxu32 n;` |
|     5 | 7361 | `	int bYes = 0;` |
|     2 | 7362 | `	SXUNUSED(nArg);` |
|     2 | 7363 | `	SXUNUSED(apArg);` |
|     5 | 7364 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|     5 | 7365 | `		ph7_class *pDecl = ReflectMemberDecl(&sRef);` |
|     5 | 7366 | `		pCons = PH7_ClassExtractMethod(pDecl, "__construct", sizeof("__construct")-1);` |
|     5 | 7367 | `		if( pCons ){` |
|     5 | 7368 | `			aArg = (ph7_vm_func_arg *)SySetBasePtr(&pCons->sFunc.aArgs);` |
|     7 | 7369 | `			for( n = 0 ; n < SySetUsed(&pCons->sFunc.aArgs) ; n++ ){` |
|     5 | 7370 | `				if( (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){` |
|   ! 0 | 7371 | `					continue;` |
|     - | 7372 | `				}` |
|     4 | 7373 | `				if( SyStringLength(&aArg[n].sName) == (sxu32)sRef.nName` |
|     4 | 7374 | `				 && SyMemcmp(SyStringData(&aArg[n].sName), sRef.zName, (sxu32)sRef.nName) == 0 ){` |
|     3 | 7375 | `					bYes = 1;` |
|     3 | 7376 | `					break;` |
|     - | 7377 | `				}` |
|     2 | 7378 | `			}` |
|     2 | 7379 | `		}` |
|     2 | 7380 | `	}` |
|     5 | 7381 | `	ph7_result_bool(pCtx, bYes);` |
|     5 | 7382 | `	return PH7_OK;` |
|     1 | 7383 | `}` |
|    70 | 7384 | `static int vm_builtin_ReflectionProperty_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 7385 | `{` |
|     - | 7386 | `	ReflectMemberRef sRef;` |
|    35 | 7387 | `	SXUNUSED(nArg);` |
|    35 | 7388 | `	SXUNUSED(apArg);` |
|    72 | 7389 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|   ! 0 | 7390 | `		ph7_result_int(pCtx, 1);   /* a dynamic property is public */` |
|   ! 0 | 7391 | `		return PH7_OK;` |
|     - | 7392 | `	}` |
|    72 | 7393 | `	ph7_result_int64(pCtx, ReflectPropModifiers(sRef.pAttr));` |
|    72 | 7394 | `	return PH7_OK;` |
|    37 | 7395 | `}` |
|     4 | 7396 | `static int vm_builtin_ReflectionProperty_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7397 | `{` |
|     - | 7398 | `	ReflectMemberRef sRef;` |
|     2 | 7399 | `	SXUNUSED(nArg);` |
|     2 | 7400 | `	SXUNUSED(apArg);` |
|     5 | 7401 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|   ! 0 | 7402 | `		ph7_result_null(pCtx);` |
|   ! 0 | 7403 | `		return PH7_OK;` |
|     - | 7404 | `	}` |
|     5 | 7405 | `	return ReflectResultClassOf(pCtx, ReflectMemberDecl(&sRef));` |
|     3 | 7406 | `}` |
|     6 | 7407 | `static int vm_builtin_ReflectionProperty_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7408 | `{` |
|     - | 7409 | `	ReflectMemberRef sRef;` |
|     3 | 7410 | `	SXUNUSED(nArg);` |
|     3 | 7411 | `	SXUNUSED(apArg);` |
|     6 | 7412 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr` |
|     7 | 7413 | `	 && SyStringLength(&sRef.pAttr->sDoc) > 0 ){` |
|     7 | 7414 | `		ph7_result_string(pCtx, SyStringData(&sRef.pAttr->sDoc),` |
|     4 | 7415 | `			(int)SyStringLength(&sRef.pAttr->sDoc));` |
|     3 | 7416 | `	}else{` |
|     3 | 7417 | `		ph7_result_bool(pCtx, 0);` |
|     - | 7418 | `	}` |
|     7 | 7419 | `	return PH7_OK;` |
|     1 | 7420 | `}` |
|     6 | 7421 | `static int vm_builtin_ReflectionProperty_hasType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7422 | `{` |
|     - | 7423 | `	ReflectMemberRef sRef;` |
|     3 | 7424 | `	SXUNUSED(nArg);` |
|     3 | 7425 | `	SXUNUSED(apArg);` |
|    13 | 7426 | `	ph7_result_bool(pCtx, ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP)` |
|     6 | 7427 | `		&& sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);` |
|     7 | 7428 | `	return PH7_OK;` |
|     1 | 7429 | `}` |
|    56 | 7430 | `static int vm_builtin_ReflectionProperty_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 7431 | `{` |
|     - | 7432 | `	ReflectMemberRef sRef;` |
|    28 | 7433 | `	SXUNUSED(nArg);` |
|    28 | 7434 | `	SXUNUSED(apArg);` |
|    56 | 7435 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0` |
|    56 | 7436 | `	 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|    55 | 7437 | `	 \|\| SyStringLength(&sRef.pAttr->sTypeName) < 1 ){` |
|     7 | 7438 | `		ph7_result_null(pCtx);` |
|     7 | 7439 | `		return PH7_OK;` |
|     - | 7440 | `	}` |
|    77 | 7441 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx,` |
|    50 | 7442 | `		SyStringData(&sRef.pAttr->sTypeName), (int)SyStringLength(&sRef.pAttr->sTypeName)));` |
|    30 | 7443 | `}` |
|    38 | 7444 | `static int vm_builtin_ReflectionProperty_hasDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7445 | `{` |
|     - | 7446 | `	ReflectMemberRef sRef;` |
|    19 | 7447 | `	SXUNUSED(nArg);` |
|    19 | 7448 | `	SXUNUSED(apArg);` |
|    39 | 7449 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|   ! 0 | 7450 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 7451 | `		return PH7_OK;` |
|     - | 7452 | `	}` |
|    39 | 7453 | `	if( SySetUsed(&sRef.pAttr->aByteCode) > 0 \|\| sRef.pAttr->pNativeValue ){` |
|    19 | 7454 | `		ph7_result_bool(pCtx, 1);` |
|    19 | 7455 | `		return PH7_OK;` |
|     - | 7456 | `	}` |
|     - | 7457 | `	/* An UNTYPED property with no initializer still defaults to null. */` |
|    21 | 7458 | `	ph7_result_bool(pCtx, (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0);` |
|    21 | 7459 | `	return PH7_OK;` |
|    20 | 7460 | `}` |
|    24 | 7461 | `static int vm_builtin_ReflectionProperty_getDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7462 | `{` |
|     - | 7463 | `	ReflectMemberRef sRef;` |
|     - | 7464 | `	ph7_value sValue;` |
|    12 | 7465 | `	SXUNUSED(nArg);` |
|    12 | 7466 | `	SXUNUSED(apArg);` |
|    25 | 7467 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|   ! 0 | 7468 | `		sRef.pAttr = 0;` |
|   ! 0 | 7469 | `	}` |
|    25 | 7470 | `	if( sRef.pAttr && sRef.pAttr->pNativeValue && SySetUsed(&sRef.pAttr->aByteCode) < 1 ){` |
|     - | 7471 | `		/* A NATIVE property's default is a LITERAL, not byte-code — the same` |
|     - | 7472 | ``		 * record `new` materializes (PH7_NativeLiteralValue). Reading only the`` |
|     - | 7473 | `		 * byte-code answered NULL for every declared native default and raised` |
|     - | 7474 | `		 * php's no-default deprecation on a slot that hasDefaultValue() had just` |
|     - | 7475 | `		 * reported true for. */` |
|    15 | 7476 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|    15 | 7477 | `		PH7_NativeLiteralValue(pCtx->pVm, sRef.pAttr->pNativeValue, &sValue);` |
|    15 | 7478 | `		ph7_result_value(pCtx, &sValue);` |
|    15 | 7479 | `		PH7_MemObjRelease(&sValue);` |
|    15 | 7480 | `		return PH7_OK;` |
|     - | 7481 | `	}` |
|    11 | 7482 | `	if( sRef.pAttr == 0 \|\| SySetUsed(&sRef.pAttr->aByteCode) < 1 ){` |
|     - | 7483 | `		/* php 8.5 deprecates the question when there is no default — an` |
|     - | 7484 | `		 * UNTYPED property still has one (null), a typed one without an` |
|     - | 7485 | `		 * initializer does not. */` |
|     5 | 7486 | `		if( sRef.pAttr == 0 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){` |
|     3 | 7487 | `			PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED,` |
|     - | 7488 | `				"ReflectionProperty::getDefaultValue() for a property without a default "` |
|     - | 7489 | `				"value is deprecated, use ReflectionProperty::hasDefaultValue() to check "` |
|     - | 7490 | `				"if the default value exists");` |
|     1 | 7491 | `		}` |
|     5 | 7492 | `		ph7_result_null(pCtx);` |
|     5 | 7493 | `		return PH7_OK;` |
|     - | 7494 | `	}` |
|     - | 7495 | `	/* Same evaluation path the VM uses for an omitted call argument */` |
|     7 | 7496 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     7 | 7497 | `	VmLocalExec(pCtx->pVm, &sRef.pAttr->aByteCode, &sValue, FALSE);` |
|     7 | 7498 | `	ph7_result_value(pCtx, &sValue);` |
|     7 | 7499 | `	PH7_MemObjRelease(&sValue);` |
|     7 | 7500 | `	return PH7_OK;` |
|    13 | 7501 | `}` |
|     - | 7502 | `/* php 8.1 made every reflected member accessible; the setter is a no-op it kept. */` |
|   ! 0 | 7503 | `static int vm_builtin_ReflectionProperty_setAccessible(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|   ! 0 | 7504 | `{` |
|   ! 0 | 7505 | `	SXUNUSED(pCtx);` |
|   ! 0 | 7506 | `	SXUNUSED(nArg);` |
|   ! 0 | 7507 | `	SXUNUSED(apArg);` |
|   ! 0 | 7508 | `	return PH7_OK;` |
|   ! 0 | 7509 | `}` |
|     - | 7510 | `/* getValue()/setValue()/isInitialized() all need the same receiver check. */` |
|    46 | 7511 | `static sxi32 ReflectPropReceiver(ph7_context *pCtx, ReflectMemberRef *pRef,` |
|     - | 7512 | `	ph7_value *pObject, ph7_class_instance **ppThis, const char *zWho)` |
|     1 | 7513 | `{` |
|    47 | 7514 | `	*ppThis = 0;` |
|    23 | 7515 | `	SXUNUSED(pRef);` |
|    47 | 7516 | `	if( pObject && (pObject->iFlags & MEMOBJ_OBJ) ){` |
|    45 | 7517 | `		*ppThis = (ph7_class_instance *)pObject->x.pOther;` |
|    45 | 7518 | `		return PH7_OK;` |
|     - | 7519 | `	}` |
|     4 | 7520 | `	return PH7_VmThrowException(pCtx, "TypeError",` |
|     - | 7521 | `		"ReflectionProperty::%s(): Argument #1 ($object) must be provided for instance properties",` |
|     1 | 7522 | `		zWho);` |
|    24 | 7523 | `}` |
|    32 | 7524 | `static int vm_builtin_ReflectionProperty_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 7525 | `{` |
|     - | 7526 | `	ReflectMemberRef sRef;` |
|     - | 7527 | `	ph7_class_instance *pObj;` |
|     - | 7528 | `	VmClassAttr *pVmAttr;` |
|     - | 7529 | `	ph7_value *pValue;` |
|     - | 7530 | `	sxi32 rc;` |
|    34 | 7531 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|   ! 0 | 7532 | `		ph7_result_null(pCtx);` |
|   ! 0 | 7533 | `		return PH7_OK;` |
|     - | 7534 | `	}` |
|    34 | 7535 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|     8 | 7536 | `		return ReflectStaticSlotRead(pCtx, sRef.pClass, sRef.pAttr);` |
|     - | 7537 | `	}` |
|    27 | 7538 | `	rc = ReflectPropReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pObj, "getValue");` |
|    27 | 7539 | `	if( rc != PH7_OK ){` |
|     3 | 7540 | `		return rc;` |
|     - | 7541 | `	}` |
|    25 | 7542 | `	pVmAttr = ReflectInstanceAttr(pObj, sRef.zName, sRef.nName);` |
|    25 | 7543 | `	if( pVmAttr == 0 ){` |
|   ! 0 | 7544 | `		ph7_result_null(pCtx);` |
|   ! 0 | 7545 | `		return PH7_OK;` |
|     - | 7546 | `	}` |
|    25 | 7547 | `	if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|     3 | 7548 | `		ph7_class *pDecl = pVmAttr->pAttr && pVmAttr->pAttr->pDeclClass` |
|     3 | 7549 | `			? pVmAttr->pAttr->pDeclClass : pObj->pClass;` |
|     4 | 7550 | `		return PH7_VmThrowException(pCtx, "Error",` |
|     - | 7551 | `			"Typed property %z::$%.*s must not be accessed before initialization",` |
|     1 | 7552 | `			&pDecl->sName, sRef.nName, sRef.zName);` |
|     - | 7553 | `	}` |
|    23 | 7554 | `	pValue = PH7_ClassInstanceExtractAttrValue(pObj, pVmAttr);` |
|    23 | 7555 | `	if( pValue ){` |
|    23 | 7556 | `		ph7_result_value(pCtx, pValue);` |
|    12 | 7557 | `	}else{` |
|   ! 0 | 7558 | `		ph7_result_null(pCtx);` |
|     - | 7559 | `	}` |
|    23 | 7560 | `	return PH7_OK;` |
|    18 | 7561 | `}` |
|    14 | 7562 | `static int vm_builtin_ReflectionProperty_setValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 7563 | `{` |
|     - | 7564 | `	ReflectMemberRef sRef;` |
|     - | 7565 | `	ph7_class_instance *pObj;` |
|     - | 7566 | `	VmClassAttr *pVmAttr;` |
|     - | 7567 | `	ph7_value *pSlot;` |
|     - | 7568 | `	sxi32 rc;` |
|    16 | 7569 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| nArg < 1 ){` |
|   ! 0 | 7570 | `		return PH7_OK;` |
|     - | 7571 | `	}` |
|    16 | 7572 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|     - | 7573 | `		/* php's one-argument spelling for a static: setValue($value). Two` |
|     - | 7574 | `		 * arguments, or one OBJECT, means the instance form was used and the` |
|     - | 7575 | `		 * value is the second. */` |
|     6 | 7576 | `		ph7_value *pVal = apArg[0];` |
|     6 | 7577 | `		if( nArg > 1 ){` |
|     6 | 7578 | `			pVal = apArg[1];` |
|     2 | 7579 | `		}else if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|   ! 0 | 7580 | `			return PH7_OK;` |
|     - | 7581 | `		}` |
|     6 | 7582 | `		return ReflectStaticSlotWrite(pCtx, sRef.pClass, sRef.pAttr, pVal);` |
|     - | 7583 | `	}` |
|    11 | 7584 | `	rc = ReflectPropReceiver(pCtx, &sRef, apArg[0], &pObj, "setValue");` |
|    11 | 7585 | `	if( rc != PH7_OK ){` |
|   ! 0 | 7586 | `		return rc;` |
|     - | 7587 | `	}` |
|    11 | 7588 | `	pVmAttr = ReflectInstanceAttr(pObj, sRef.zName, sRef.nName);` |
|    11 | 7589 | `	if( pVmAttr == 0 ){` |
|   ! 0 | 7590 | `		return PH7_OK;` |
|     - | 7591 | `	}` |
|     - | 7592 | `	{` |
|    11 | 7593 | `		ph7_value *pVal = nArg > 1 ? apArg[1] : 0;` |
|     - | 7594 | `		ph7_value sNull;` |
|    11 | 7595 | `		PH7_MemObjInit(pCtx->pVm, &sNull);` |
|    11 | 7596 | `		if( pVal == 0 ){` |
|   ! 0 | 7597 | `			pVal = &sNull;` |
|   ! 0 | 7598 | `		}` |
|    11 | 7599 | `		rc = ReflectEnforceStore(pCtx, pVmAttr->nIdx, pVal);` |
|    11 | 7600 | `		if( rc != SXRET_OK ){` |
|     5 | 7601 | `			PH7_MemObjRelease(&sNull);` |
|     5 | 7602 | `			return rc;` |
|     - | 7603 | `		}` |
|     7 | 7604 | `		pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|     7 | 7605 | `		if( pSlot ){` |
|     7 | 7606 | `			PH7_MemObjStore(pVal, pSlot);` |
|     7 | 7607 | `			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;` |
|     3 | 7608 | `		}` |
|     7 | 7609 | `		PH7_MemObjRelease(&sNull);` |
|     - | 7610 | `	}` |
|     7 | 7611 | `	return PH7_OK;` |
|     9 | 7612 | `}` |
|    16 | 7613 | `static int vm_builtin_ReflectionProperty_isInitialized(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 7614 | `{` |
|     - | 7615 | `	ReflectMemberRef sRef;` |
|     - | 7616 | `	ph7_class_instance *pObj;` |
|     - | 7617 | `	VmClassAttr *pVmAttr;` |
|     - | 7618 | `	sxi32 rc;` |
|    18 | 7619 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|   ! 0 | 7620 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 7621 | `		return PH7_OK;` |
|     - | 7622 | `	}` |
|    18 | 7623 | `	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|     - | 7624 | `		SyHashEntry *pSlot;` |
|     8 | 7625 | `		rc = ReflectMaterializeStatics(pCtx, sRef.pClass);` |
|     8 | 7626 | `		if( rc != SXRET_OK ){` |
|     5 | 7627 | `			return rc;` |
|     - | 7628 | `		}` |
|     3 | 7629 | `		pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&sRef.pAttr->nIdx, sizeof(sxu32));` |
|     5 | 7630 | `		ph7_result_bool(pCtx, pSlot == 0` |
|     2 | 7631 | `			\|\| (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) == 0);` |
|     3 | 7632 | `		return PH7_OK;` |
|     - | 7633 | `	}` |
|    11 | 7634 | `	rc = ReflectPropReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pObj, "isInitialized");` |
|    11 | 7635 | `	if( rc != PH7_OK ){` |
|   ! 0 | 7636 | `		return rc;` |
|     - | 7637 | `	}` |
|    11 | 7638 | `	pVmAttr = ReflectInstanceAttr(pObj, sRef.zName, sRef.nName);` |
|    11 | 7639 | `	ph7_result_bool(pCtx, pVmAttr != 0 && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0);` |
|    11 | 7640 | `	return PH7_OK;` |
|    10 | 7641 | `}` |
|     - | 7642 | `/*` |
|     - | 7643 | `` * The property HOOKS (php 8.4). PHL compiles `public $p { get => ...; }` into`` |
|     - | 7644 | `` * synthesized methods named `__phl_hook_get_NAME` / `__phl_hook_set_NAME`, so`` |
|     - | 7645 | ` * the reflector php answers is a ReflectionMethod over one of those.` |
|     - | 7646 | ` */` |
|    30 | 7647 | `static int ReflectHookMethod(ph7_context *pCtx, ReflectMemberRef *pRef, int bSet,` |
|     - | 7648 | `	ph7_class_instance **ppOut)` |
|     3 | 7649 | `{` |
|     - | 7650 | `	char zName[128];` |
|     - | 7651 | `	ph7_value sClass, sName;` |
|     - | 7652 | `	ph7_value *apCtor[2];` |
|     - | 7653 | `	ph7_class *pDecl;` |
|     - | 7654 | `	sxi32 rc;` |
|     - | 7655 | `	int nName;` |
|    33 | 7656 | `	*ppOut = 0;` |
|    33 | 7657 | `	if( pRef->pAttr == 0 ){` |
|   ! 0 | 7658 | `		return PH7_OK;` |
|     - | 7659 | `	}` |
|    33 | 7660 | `	if( (pRef->pAttr->iFlags & (bSet ? PH7_CLASS_ATTR_HOOK_SET : PH7_CLASS_ATTR_HOOK_GET)) == 0 ){` |
|    15 | 7661 | `		return PH7_OK;` |
|     - | 7662 | `	}` |
|    21 | 7663 | `	pDecl = ReflectMemberDecl(pRef);` |
|    30 | 7664 | `	nName = SyBufferFormat(zName, sizeof(zName), "%s%.*s",` |
|     9 | 7665 | `		bSet ? "__phl_hook_set_" : "__phl_hook_get_", pRef->nName, pRef->zName);` |
|    21 | 7666 | `	PH7_MemObjInit(pCtx->pVm, &sClass);` |
|    21 | 7667 | `	PH7_MemObjInit(pCtx->pVm, &sName);` |
|    21 | 7668 | `	ph7_value_string(&sClass, SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|    21 | 7669 | `	ph7_value_string(&sName, zName, nName);` |
|    21 | 7670 | `	apCtor[0] = &sClass;` |
|    21 | 7671 | `	apCtor[1] = &sName;` |
|    21 | 7672 | `	*ppOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);` |
|    21 | 7673 | `	PH7_MemObjRelease(&sClass);` |
|    21 | 7674 | `	PH7_MemObjRelease(&sName);` |
|    21 | 7675 | `	return rc;` |
|    18 | 7676 | `}` |
|    12 | 7677 | `static int vm_builtin_ReflectionProperty_getHooks(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 7678 | `{` |
|     - | 7679 | `	ReflectMemberRef sRef;` |
|    15 | 7680 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     - | 7681 | `	int iHook;` |
|     6 | 7682 | `	SXUNUSED(nArg);` |
|     6 | 7683 | `	SXUNUSED(apArg);` |
|    15 | 7684 | `	if( pOut == 0 ){` |
|   ! 0 | 7685 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 7686 | `	}` |
|    15 | 7687 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|   ! 0 | 7688 | `		ph7_result_value(pCtx, pOut);` |
|   ! 0 | 7689 | `		return PH7_OK;` |
|     - | 7690 | `	}` |
|    39 | 7691 | `	for( iHook = 0 ; iHook < 2 ; iHook++ ){` |
|    27 | 7692 | `		ph7_class_instance *pMeth = 0;` |
|     - | 7693 | `		ph7_value sVal, *pKey;` |
|    27 | 7694 | `		sxi32 rc = ReflectHookMethod(pCtx, &sRef, iHook, &pMeth);` |
|    27 | 7695 | `		if( rc != PH7_OK ){` |
|   ! 0 | 7696 | `			return rc;` |
|     - | 7697 | `		}` |
|    27 | 7698 | `		if( pMeth == 0 ){` |
|    13 | 7699 | `			continue;` |
|     - | 7700 | `		}` |
|    17 | 7701 | `		pKey = ph7_context_new_scalar(pCtx);` |
|    17 | 7702 | `		if( pKey == 0 ){` |
|   ! 0 | 7703 | `			PH7_ClassInstanceUnref(pMeth);` |
|   ! 0 | 7704 | `			break;` |
|     - | 7705 | `		}` |
|    17 | 7706 | `		ph7_value_string(pKey, iHook ? "set" : "get", 3);` |
|    17 | 7707 | `		PH7_MemObjInit(pCtx->pVm, &sVal);` |
|    17 | 7708 | `		sVal.x.pOther = pMeth;` |
|    17 | 7709 | `		sVal.iFlags = MEMOBJ_OBJ;` |
|    17 | 7710 | `		ph7_array_add_elem(pOut, pKey, &sVal);   /* takes its own reference */` |
|    17 | 7711 | `		PH7_ClassInstanceUnref(pMeth);` |
|    10 | 7712 | `	}` |
|    15 | 7713 | `	ph7_result_value(pCtx, pOut);` |
|    15 | 7714 | `	return PH7_OK;` |
|     9 | 7715 | `}` |
|     - | 7716 | ``/* `get`/`set` out of a PropertyHookType case (or the bare string php also takes). */`` |
|    12 | 7717 | `static int ReflectHookKind(ph7_value *pArg)` |
|     1 | 7718 | `{` |
|     - | 7719 | `	const char *z;` |
|     - | 7720 | `	int n;` |
|    13 | 7721 | `	if( pArg == 0 ){` |
|   ! 0 | 7722 | `		return -1;` |
|     - | 7723 | `	}` |
|    13 | 7724 | `	if( pArg->iFlags & MEMOBJ_OBJ ){` |
|    13 | 7725 | `		ph7_class_instance *pCase = (ph7_class_instance *)pArg->x.pOther;` |
|    13 | 7726 | `		ph7_value *pVal = PH7_NativeAttr(pCase, "value");` |
|    13 | 7727 | `		if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_STRING) == 0 ){` |
|   ! 0 | 7728 | `			return -1;` |
|     - | 7729 | `		}` |
|    13 | 7730 | `		z = (const char *)SyBlobData(&pVal->sBlob);` |
|    13 | 7731 | `		n = (int)SyBlobLength(&pVal->sBlob);` |
|     7 | 7732 | `	}else{` |
|   ! 0 | 7733 | `		z = ph7_value_to_string(pArg, &n);` |
|     - | 7734 | `	}` |
|    13 | 7735 | `	if( n == 3 && SyMemcmp(z, "get", 3) == 0 ){` |
|     7 | 7736 | `		return 0;` |
|     - | 7737 | `	}` |
|     7 | 7738 | `	if( n == 3 && SyMemcmp(z, "set", 3) == 0 ){` |
|     7 | 7739 | `		return 1;` |
|     - | 7740 | `	}` |
|   ! 0 | 7741 | `	return -1;` |
|     7 | 7742 | `}` |
|     6 | 7743 | `static int vm_builtin_ReflectionProperty_hasHook(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7744 | `{` |
|     - | 7745 | `	ReflectMemberRef sRef;` |
|     7 | 7746 | `	int iHook = ReflectHookKind(nArg > 0 ? apArg[0] : 0);` |
|     7 | 7747 | `	int bYes = 0;` |
|     7 | 7748 | `	if( iHook >= 0 && ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){` |
|     7 | 7749 | `		bYes = (sRef.pAttr->iFlags & (iHook ? PH7_CLASS_ATTR_HOOK_SET : PH7_CLASS_ATTR_HOOK_GET)) != 0;` |
|     3 | 7750 | `	}` |
|     7 | 7751 | `	ph7_result_bool(pCtx, bYes);` |
|     7 | 7752 | `	return PH7_OK;` |
|     1 | 7753 | `}` |
|     6 | 7754 | `static int vm_builtin_ReflectionProperty_getHook(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7755 | `{` |
|     - | 7756 | `	ReflectMemberRef sRef;` |
|     7 | 7757 | `	ph7_class_instance *pMeth = 0;` |
|     7 | 7758 | `	int iHook = ReflectHookKind(nArg > 0 ? apArg[0] : 0);` |
|     - | 7759 | `	sxi32 rc;` |
|     7 | 7760 | `	if( iHook < 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){` |
|   ! 0 | 7761 | `		ph7_result_null(pCtx);` |
|   ! 0 | 7762 | `		return PH7_OK;` |
|     - | 7763 | `	}` |
|     7 | 7764 | `	rc = ReflectHookMethod(pCtx, &sRef, iHook, &pMeth);` |
|     7 | 7765 | `	if( rc != PH7_OK ){` |
|   ! 0 | 7766 | `		return rc;` |
|     - | 7767 | `	}` |
|     7 | 7768 | `	if( pMeth == 0 ){` |
|     3 | 7769 | `		ph7_result_null(pCtx);` |
|     3 | 7770 | `		return PH7_OK;` |
|     - | 7771 | `	}` |
|     5 | 7772 | `	return ReflectResultObject(pCtx, pMeth);` |
|     4 | 7773 | `}` |
|     6 | 7774 | `static int vm_builtin_ReflectionProperty_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7775 | `{` |
|     7 | 7776 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 7777 | `	ReflectMemberRef sRef;` |
|     - | 7778 | `	ph7_value sTarget;` |
|     - | 7779 | `	const char *zClass;` |
|     - | 7780 | `	int nClass, rc;` |
|     7 | 7781 | `	if( pThis == 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|   ! 0 | 7782 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|   ! 0 | 7783 | `		return PH7_OK;` |
|     - | 7784 | `	}` |
|     7 | 7785 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     7 | 7786 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     7 | 7787 | `	ph7_value_string(&sTarget, zClass, nClass);` |
|     - | 7788 | `	/* 8 = Attribute::TARGET_PROPERTY */` |
|    10 | 7789 | `	rc = ReflectBuildAttrs(pCtx, &sRef.pAttr->aAttrs, "attr", &sTarget,` |
|     3 | 7790 | `		sRef.zName, sRef.nName, 0, 8, nArg, apArg);` |
|     7 | 7791 | `	PH7_MemObjRelease(&sTarget);` |
|     7 | 7792 | `	return rc;` |
|     4 | 7793 | `}` |
|     4 | 7794 | `static int vm_builtin_ReflectionProperty_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7795 | `{` |
|     2 | 7796 | `	SXUNUSED(nArg);` |
|     2 | 7797 | `	SXUNUSED(apArg);` |
|     5 | 7798 | `	return ReflectExportPropSelf(pCtx);` |
|     1 | 7799 | `}` |
|     - | 7800 | `/* ---- ReflectionClassConstant ---- */` |
|     - | 7801 | `/* ReflectionClassConstant::__construct(object\|string $class, string $constant) */` |
|    72 | 7802 | `static int vm_builtin_ReflectionClassConstant_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7803 | `{` |
|    73 | 7804 | `	ph7_vm *pVm = pCtx->pVm;` |
|    73 | 7805 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|    73 | 7806 | `	ph7_class *pClass, *pDecl = 0;` |
|     - | 7807 | `	const char *zConst;` |
|     - | 7808 | `	int nConst;` |
|     - | 7809 | `	SySet aMembers;` |
|     - | 7810 | `	sxu32 n;` |
|    73 | 7811 | `	int bFound = 0;` |
|    73 | 7812 | `	if( pThis == 0 \|\| nArg < 2 ){` |
|   ! 0 | 7813 | `		return PH7_OK;` |
|     - | 7814 | `	}` |
|    73 | 7815 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|    73 | 7816 | `	if( pClass == 0 ){` |
|     - | 7817 | `		const char *zName;` |
|     - | 7818 | `		int nName;` |
|   ! 0 | 7819 | `		zName = ph7_value_to_string(apArg[0], &nName);` |
|   ! 0 | 7820 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|   ! 0 | 7821 | `			"Class \"%.*s\" does not exist", nName, zName);` |
|     - | 7822 | `	}` |
|    73 | 7823 | `	zConst = ph7_value_to_string(apArg[1], &nConst);` |
|    73 | 7824 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|    73 | 7825 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|   357 | 7826 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   351 | 7827 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   351 | 7828 | `		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zConst, nConst) ){` |
|    67 | 7829 | `			pDecl = pM->pDecl ? pM->pDecl : pClass;` |
|    67 | 7830 | `			bFound = 1;` |
|    67 | 7831 | `			break;` |
|     - | 7832 | `		}` |
|   143 | 7833 | `	}` |
|    73 | 7834 | `	SySetRelease(&aMembers);` |
|    73 | 7835 | `	if( !bFound ){` |
|    10 | 7836 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     3 | 7837 | `			"Constant %z::%.*s does not exist", &pClass->sName, nConst, zConst);` |
|     - | 7838 | `	}` |
|     - | 7839 | `	/* php's $class is the DECLARING class, not the one asked about. */` |
|    67 | 7840 | `	pClass = pDecl;` |
|   100 | 7841 | `	PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pClass->sName),` |
|    66 | 7842 | `		(int)SyStringLength(&pClass->sName));` |
|    67 | 7843 | `	PH7_NativeSetAttrStr(pVm, pThis, "name", zConst, nConst);` |
|    67 | 7844 | `	return PH7_OK;` |
|    37 | 7845 | `}` |
|    20 | 7846 | `static int vm_builtin_ReflectionClassConstant_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7847 | `{` |
|     - | 7848 | `	ReflectMemberRef sRef;` |
|     - | 7849 | `	ph7_value *pVal;` |
|     - | 7850 | `	sxi32 rc;` |
|    10 | 7851 | `	SXUNUSED(nArg);` |
|    10 | 7852 | `	SXUNUSED(apArg);` |
|    21 | 7853 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|   ! 0 | 7854 | `		ph7_result_null(pCtx);` |
|   ! 0 | 7855 | `		return PH7_OK;` |
|     - | 7856 | `	}` |
|    21 | 7857 | `	rc = ReflectConstSlot(pCtx, sRef.pClass, sRef.pAttr, &pVal);` |
|    21 | 7858 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 7859 | `		return rc;` |
|     - | 7860 | `	}` |
|    21 | 7861 | `	if( pVal ){` |
|    21 | 7862 | `		ph7_result_value(pCtx, pVal);` |
|    11 | 7863 | `	}else{` |
|   ! 0 | 7864 | `		ph7_result_null(pCtx);` |
|     - | 7865 | `	}` |
|    21 | 7866 | `	return PH7_OK;` |
|    11 | 7867 | `}` |
|    24 | 7868 | `static int ReflectConstFlag(ph7_context *pCtx, int iWhat)` |
|     1 | 7869 | `{` |
|     - | 7870 | `	ReflectMemberRef sRef;` |
|    25 | 7871 | `	int bYes = 0;` |
|    25 | 7872 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) && sRef.pAttr ){` |
|    25 | 7873 | `		ph7_class_attr *pAttr = sRef.pAttr;` |
|    25 | 7874 | `		switch( iWhat ){` |
|     3 | 7875 | `		case 0: bYes = pAttr->iProtection == PH7_CLASS_PROT_PUBLIC; break;` |
|     3 | 7876 | `		case 1: bYes = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE; break;` |
|     3 | 7877 | `		case 2: bYes = pAttr->iProtection == PH7_CLASS_PROT_PROTECTED; break;` |
|     3 | 7878 | `		case 3: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0; break;` |
|    13 | 7879 | `		case 4: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0; break;` |
|     3 | 7880 | `		case 5: bYes = ReflectHasDeprecated(&pAttr->aAttrs); break;` |
|     3 | 7881 | `		default: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0; break;` |
|     - | 7882 | `		}` |
|    12 | 7883 | `	}` |
|    25 | 7884 | `	ph7_result_bool(pCtx, bYes);` |
|    25 | 7885 | `	return PH7_OK;` |
|     1 | 7886 | `}` |
|     - | 7887 | `#define REFLECT_CONST_FLAG(NAME,WHAT) \` |
|     - | 7888 | `	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \` |
|     - | 7889 | `	{ \` |
|     - | 7890 | `		SXUNUSED(nArg); \` |
|     - | 7891 | `		SXUNUSED(apArg); \` |
|     - | 7892 | `		return ReflectConstFlag(pCtx, WHAT); \` |
|     - | 7893 | `	}` |
|     3 | 7894 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isPublic, 0)` |
|     3 | 7895 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isPrivate, 1)` |
|     3 | 7896 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isProtected, 2)` |
|     3 | 7897 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isFinal, 3)` |
|    13 | 7898 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isEnumCase, 4)` |
|     3 | 7899 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isDeprecated, 5)` |
|     3 | 7900 | `REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_hasType, 6)` |
|     - | 7901 |  |
|     8 | 7902 | `static int vm_builtin_ReflectionClassConstant_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7903 | `{` |
|     - | 7904 | `	ReflectMemberRef sRef;` |
|     4 | 7905 | `	SXUNUSED(nArg);` |
|     4 | 7906 | `	SXUNUSED(apArg);` |
|     9 | 7907 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|   ! 0 | 7908 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 | 7909 | `		return PH7_OK;` |
|     - | 7910 | `	}` |
|     9 | 7911 | `	ph7_result_int64(pCtx, ReflectConstModifiers(sRef.pAttr));` |
|     9 | 7912 | `	return PH7_OK;` |
|     5 | 7913 | `}` |
|     4 | 7914 | `static int vm_builtin_ReflectionClassConstant_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7915 | `{` |
|     - | 7916 | `	ReflectMemberRef sRef;` |
|     2 | 7917 | `	SXUNUSED(nArg);` |
|     2 | 7918 | `	SXUNUSED(apArg);` |
|     5 | 7919 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) ){` |
|   ! 0 | 7920 | `		ph7_result_null(pCtx);` |
|   ! 0 | 7921 | `		return PH7_OK;` |
|     - | 7922 | `	}` |
|     5 | 7923 | `	return ReflectResultClassOf(pCtx, ReflectMemberDecl(&sRef));` |
|     3 | 7924 | `}` |
|     2 | 7925 | `static int vm_builtin_ReflectionClassConstant_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7926 | `{` |
|     - | 7927 | `	ReflectMemberRef sRef;` |
|     1 | 7928 | `	SXUNUSED(nArg);` |
|     1 | 7929 | `	SXUNUSED(apArg);` |
|     2 | 7930 | `	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) && sRef.pAttr` |
|     3 | 7931 | `	 && SyStringLength(&sRef.pAttr->sDoc) > 0 ){` |
|     4 | 7932 | `		ph7_result_string(pCtx, SyStringData(&sRef.pAttr->sDoc),` |
|     2 | 7933 | `			(int)SyStringLength(&sRef.pAttr->sDoc));` |
|     2 | 7934 | `	}else{` |
|   ! 0 | 7935 | `		ph7_result_bool(pCtx, 0);` |
|     - | 7936 | `	}` |
|     3 | 7937 | `	return PH7_OK;` |
|     1 | 7938 | `}` |
|     2 | 7939 | `static int vm_builtin_ReflectionClassConstant_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7940 | `{` |
|     - | 7941 | `	ReflectMemberRef sRef;` |
|     1 | 7942 | `	SXUNUSED(nArg);` |
|     1 | 7943 | `	SXUNUSED(apArg);` |
|     2 | 7944 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0` |
|     2 | 7945 | `	 \|\| (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0` |
|     3 | 7946 | `	 \|\| SyStringLength(&sRef.pAttr->sTypeName) < 1 ){` |
|   ! 0 | 7947 | `		ph7_result_null(pCtx);` |
|   ! 0 | 7948 | `		return PH7_OK;` |
|     - | 7949 | `	}` |
|     4 | 7950 | `	return ReflectResultObject(pCtx, ReflectMakeType(pCtx,` |
|     2 | 7951 | `		SyStringData(&sRef.pAttr->sTypeName), (int)SyStringLength(&sRef.pAttr->sTypeName)));` |
|     2 | 7952 | `}` |
|     2 | 7953 | `static int vm_builtin_ReflectionClassConstant_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7954 | `{` |
|     3 | 7955 | `	ph7_class_instance *pThis = PH7_ContextThis(pCtx);` |
|     - | 7956 | `	ReflectMemberRef sRef;` |
|     - | 7957 | `	ph7_value sTarget;` |
|     - | 7958 | `	const char *zClass;` |
|     - | 7959 | `	int nClass, rc;` |
|     3 | 7960 | `	if( pThis == 0 \|\| !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|   ! 0 | 7961 | `		ph7_result_value(pCtx, ph7_context_new_array(pCtx));` |
|   ! 0 | 7962 | `		return PH7_OK;` |
|     - | 7963 | `	}` |
|     3 | 7964 | `	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);` |
|     3 | 7965 | `	PH7_MemObjInit(pCtx->pVm, &sTarget);` |
|     3 | 7966 | `	ph7_value_string(&sTarget, zClass, nClass);` |
|     - | 7967 | `	/* 16 = Attribute::TARGET_CLASS_CONSTANT */` |
|     4 | 7968 | `	rc = ReflectBuildAttrs(pCtx, &sRef.pAttr->aAttrs, "attr", &sTarget,` |
|     1 | 7969 | `		sRef.zName, sRef.nName, 0, 16, nArg, apArg);` |
|     3 | 7970 | `	PH7_MemObjRelease(&sTarget);` |
|     3 | 7971 | `	return rc;` |
|     2 | 7972 | `}` |
|     6 | 7973 | `static int vm_builtin_ReflectionClassConstant_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 7974 | `{` |
|     3 | 7975 | `	SXUNUSED(nArg);` |
|     3 | 7976 | `	SXUNUSED(apArg);` |
|     7 | 7977 | `	return ReflectExportConstSelf(pCtx);` |
|     1 | 7978 | `}` |
|     - | 7979 | `/*` |
|     - | 7980 | ` * Declare chunk 3. Called from PH7_VmInstallReflection() where it used to be` |
|     - | 7981 | ` * compiled — after chunks 1 and 2, whose ReflectionClass and ReflectionMethod` |
|     - | 7982 | ` * these answer, and after PropertyHookType, which hasHook()/getHook() declare.` |
|     - | 7983 | ` *` |
|     - | 7984 | ` * The method tables are in php's own DECLARATION order.` |
|     - | 7985 | ` */` |
|  5146 | 7986 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionMember(ph7_vm *pVm)` |
|     5 | 7987 | `{` |
|     - | 7988 | `	static const PH7_NativePropDef aMemberProp[] = {` |
|     - | 7989 | `		{ "name",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|     - | 7990 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|     - | 7991 | `	};` |
|     - | 7992 | `	static const PH7_NativePropDef aPropProp[] = {` |
|     - | 7993 | `		{ "name",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|     - | 7994 | `		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },` |
|     - | 7995 | `		/* PHL-only: the instance a DYNAMIC property was reached through (§7.4 (e)) */` |
|     - | 7996 | `		{ RP_DYNOBJ, PH7_MOD_PROTECTED\|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },` |
|     - | 7997 | `	};` |
|     - | 7998 | `	static const PH7_NativeConstDef aPropConst[] = {` |
|     - | 7999 | `		{ "IS_STATIC",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,   0, 0.0 },` |
|     - | 8000 | `		{ "IS_READONLY",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 128,  0, 0.0 },` |
|     - | 8001 | `		{ "IS_PUBLIC",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,    0, 0.0 },` |
|     - | 8002 | `		{ "IS_PROTECTED",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,    0, 0.0 },` |
|     - | 8003 | `		{ "IS_PRIVATE",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,    0, 0.0 },` |
|     - | 8004 | `		{ "IS_ABSTRACT",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64,   0, 0.0 },` |
|     - | 8005 | `		{ "IS_PROTECTED_SET", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2048, 0, 0.0 },` |
|     - | 8006 | `		{ "IS_PRIVATE_SET",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4096, 0, 0.0 },` |
|     - | 8007 | `		{ "IS_VIRTUAL",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 512,  0, 0.0 },` |
|     - | 8008 | `		{ "IS_FINAL",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32,   0, 0.0 },` |
|     - | 8009 | `	};` |
|     - | 8010 | `	static const PH7_NativeMethodDef aPropMethod[] = {` |
|     - | 8011 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|     - | 8012 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $property", "",` |
|     - | 8013 | `		  vm_builtin_ReflectionProperty_construct },` |
|     - | 8014 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionProperty_toString },` |
|     - | 8015 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionProperty_getName },` |
|     - | 8016 | `		{ "getMangledName", PH7_MOD_PUBLIC, "", "string",` |
|     - | 8017 | `		  vm_builtin_ReflectionProperty_getMangledName },` |
|     - | 8018 | `		{ "getValue",    PH7_MOD_PUBLIC, "?object $object = null", "@mixed",` |
|     - | 8019 | `		  vm_builtin_ReflectionProperty_getValue },` |
|     - | 8020 | `		{ "setValue",    PH7_MOD_PUBLIC, "mixed $objectOrValue, mixed $value = ?", "@void",` |
|     - | 8021 | `		  vm_builtin_ReflectionProperty_setValue },` |
|     - | 8022 | `		{ "getRawValue", PH7_MOD_PUBLIC, "object $object", "mixed",` |
|     - | 8023 | `		  vm_builtin_ReflectionProperty_getValue },` |
|     - | 8024 | `		{ "setRawValue", PH7_MOD_PUBLIC, "object $object, mixed $value", "void",` |
|     - | 8025 | `		  vm_builtin_ReflectionProperty_setValue },` |
|     - | 8026 | `		/* On a non-lazy object — the only kind PHL has — php's two lazy writers` |
|     - | 8027 | `		 * are an ordinary raw write and a no-op. */` |
|     - | 8028 | `		{ "setRawValueWithoutLazyInitialization", PH7_MOD_PUBLIC,` |
|     - | 8029 | `		  "object $object, mixed $value", "void", vm_builtin_ReflectionProperty_setValue },` |
|     - | 8030 | `		{ "skipLazyInitialization", PH7_MOD_PUBLIC, "object $object", "void",` |
|     - | 8031 | `		  vm_builtin_ReflectionProperty_noop },` |
|     - | 8032 | `		{ "isLazy",      PH7_MOD_PUBLIC, "object $object", "bool",` |
|     - | 8033 | `		  vm_builtin_ReflectionProperty_false },` |
|     - | 8034 | `		{ "isInitialized", PH7_MOD_PUBLIC, "?object $object = null", "@bool",` |
|     - | 8035 | `		  vm_builtin_ReflectionProperty_isInitialized },` |
|     - | 8036 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isPublic },` |
|     - | 8037 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isPrivate },` |
|     - | 8038 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isProtected },` |
|     - | 8039 | `		{ "isPrivateSet", PH7_MOD_PUBLIC, "", "bool",` |
|     - | 8040 | `		  vm_builtin_ReflectionProperty_isPrivateSet },` |
|     - | 8041 | `		{ "isProtectedSet", PH7_MOD_PUBLIC, "", "bool",` |
|     - | 8042 | `		  vm_builtin_ReflectionProperty_isProtectedSet },` |
|     - | 8043 | `		{ "isStatic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isStatic },` |
|     - | 8044 | `		{ "isReadOnly",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isReadOnly },` |
|     - | 8045 | `		{ "isDefault",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isDefault },` |
|     - | 8046 | `		{ "isDynamic",   PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isDynamic },` |
|     - | 8047 | `		/* PHL has no abstract properties: the modifier only exists on an` |
|     - | 8048 | `		 * interface's hooked property stub, which PHL does not model. */` |
|     - | 8049 | `		{ "isAbstract",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_false },` |
|     - | 8050 | `		{ "isVirtual",   PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isVirtual },` |
|     - | 8051 | `		{ "isPromoted",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isPromoted },` |
|     - | 8052 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionProperty_getModifiers },` |
|     - | 8053 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|     - | 8054 | `		  vm_builtin_ReflectionProperty_getDeclaringClass },` |
|     - | 8055 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false", vm_builtin_ReflectionProperty_getDocComment },` |
|     - | 8056 | `		{ "setAccessible", PH7_MOD_PUBLIC, "bool $accessible", "@void",` |
|     - | 8057 | `		  vm_builtin_ReflectionProperty_setAccessible },` |
|     - | 8058 | `		{ "getType",     PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionProperty_getType },` |
|     - | 8059 | `		/* php's settable type differs from the declared one only for a hooked` |
|     - | 8060 | ``		 * property with a widening `set` — which PHL does not model. */`` |
|     - | 8061 | `		{ "getSettableType", PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|     - | 8062 | `		  vm_builtin_ReflectionProperty_getType },` |
|     - | 8063 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_hasType },` |
|     - | 8064 | `		{ "hasDefaultValue", PH7_MOD_PUBLIC, "", "bool",` |
|     - | 8065 | `		  vm_builtin_ReflectionProperty_hasDefaultValue },` |
|     - | 8066 | `		{ "getDefaultValue", PH7_MOD_PUBLIC, "", "@mixed",` |
|     - | 8067 | `		  vm_builtin_ReflectionProperty_getDefaultValue },` |
|     - | 8068 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|     - | 8069 | `		  vm_builtin_ReflectionProperty_getAttributes },` |
|     - | 8070 | `		{ "hasHooks",    PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_hasHooks },` |
|     - | 8071 | `		{ "getHooks",    PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionProperty_getHooks },` |
|     - | 8072 | `		{ "hasHook",     PH7_MOD_PUBLIC, "PropertyHookType $type", "bool",` |
|     - | 8073 | `		  vm_builtin_ReflectionProperty_hasHook },` |
|     - | 8074 | `		{ "getHook",     PH7_MOD_PUBLIC, "PropertyHookType $type", "?ReflectionMethod",` |
|     - | 8075 | `		  vm_builtin_ReflectionProperty_getHook },` |
|     - | 8076 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_false },` |
|     - | 8077 | `	};` |
|     - | 8078 | `	static const PH7_NativeConstDef aConstConst[] = {` |
|     - | 8079 | `		{ "IS_PUBLIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,  0, 0.0 },` |
|     - | 8080 | `		{ "IS_PROTECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,  0, 0.0 },` |
|     - | 8081 | `		{ "IS_PRIVATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,  0, 0.0 },` |
|     - | 8082 | `		{ "IS_FINAL",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },` |
|     - | 8083 | `	};` |
|     - | 8084 | `	static const PH7_NativeMethodDef aConstMethod[] = {` |
|     - | 8085 | `		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },` |
|     - | 8086 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|     - | 8087 | `		  vm_builtin_ReflectionClassConstant_construct },` |
|     - | 8088 | `		{ "__toString",  PH7_MOD_PUBLIC, "", "string",` |
|     - | 8089 | `		  vm_builtin_ReflectionClassConstant_toString },` |
|     - | 8090 | `		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionProperty_getName },` |
|     - | 8091 | `		{ "getValue",    PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_ReflectionClassConstant_getValue },` |
|     - | 8092 | `		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClassConstant_isPublic },` |
|     - | 8093 | `		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClassConstant_isPrivate },` |
|     - | 8094 | `		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool",` |
|     - | 8095 | `		  vm_builtin_ReflectionClassConstant_isProtected },` |
|     - | 8096 | `		{ "isFinal",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClassConstant_isFinal },` |
|     - | 8097 | `		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int",` |
|     - | 8098 | `		  vm_builtin_ReflectionClassConstant_getModifiers },` |
|     - | 8099 | `		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",` |
|     - | 8100 | `		  vm_builtin_ReflectionClassConstant_getDeclaringClass },` |
|     - | 8101 | `		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string\|false",` |
|     - | 8102 | `		  vm_builtin_ReflectionClassConstant_getDocComment },` |
|     - | 8103 | `		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",` |
|     - | 8104 | `		  vm_builtin_ReflectionClassConstant_getAttributes },` |
|     - | 8105 | `		{ "isEnumCase",  PH7_MOD_PUBLIC, "", "bool",` |
|     - | 8106 | `		  vm_builtin_ReflectionClassConstant_isEnumCase },` |
|     - | 8107 | `		{ "isDeprecated", PH7_MOD_PUBLIC, "", "bool",` |
|     - | 8108 | `		  vm_builtin_ReflectionClassConstant_isDeprecated },` |
|     - | 8109 | `		{ "hasType",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClassConstant_hasType },` |
|     - | 8110 | `		{ "getType",     PH7_MOD_PUBLIC, "", "?ReflectionType",` |
|     - | 8111 | `		  vm_builtin_ReflectionClassConstant_getType },` |
|     - | 8112 | `	};` |
|     - | 8113 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 8114 | `		{ "ReflectionProperty", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 8115 | `		  aPropMethod, SX_ARRAYSIZE(aPropMethod),` |
|     - | 8116 | `		  aPropConst, SX_ARRAYSIZE(aPropConst),` |
|     - | 8117 | `		  aPropProp, SX_ARRAYSIZE(aPropProp), 0, 0, 0 },` |
|     - | 8118 | `		{ "ReflectionClassConstant", 0, "Reflector", PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 8119 | `		  aConstMethod, SX_ARRAYSIZE(aConstMethod),` |
|     - | 8120 | `		  aConstConst, SX_ARRAYSIZE(aConstConst),` |
|     - | 8121 | `		  aMemberProp, SX_ARRAYSIZE(aMemberProp), 0, 0, 0 },` |
|     - | 8122 | `	};` |
|  5151 | 8123 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|     5 | 8124 | `}` |
|     - | 8125 | `/*` |
|     - | 8126 | ` * ---------------------------------------------------------------------------` |
|     - | 8127 | ` * The ReflectionEnum family — chunk 6, and the end of __phl_rcinfo.` |
|     - | 8128 | ` *` |
|     - | 8129 | `` * Three classes that are very nearly their parents: `ReflectionEnum` IS a`` |
|     - | 8130 | `` * `ReflectionClass` that refuses a non-enum, and `ReflectionEnumUnitCase` IS a`` |
|     - | 8131 | `` * `ReflectionClassConstant` that refuses a constant which is not a case. What`` |
|     - | 8132 | ` * is their own is the CASE list, which the engine already holds in declaration` |
|     - | 8133 | `` * order on `ph7_class::aEnumCases` — the chunk read a marshalled copy of it out`` |
|     - | 8134 | `` * of `__phl_rcinfo`, the descriptor builder that dies with this conversion.`` |
|     - | 8135 | ` * ---------------------------------------------------------------------------` |
|     - | 8136 | ` */` |
|     - | 8137 |  |
|     - | 8138 | `/* The enum case named zName, or NULL. A case is a class CONSTANT, so the match` |
|     - | 8139 | `` * is case-SENSITIVE: php's hasCase('hearts') is false for `case Hearts`. */`` |
|    26 | 8140 | `static ph7_class_attr * ReflectEnumCase(ph7_class *pClass, const char *zName, int nName)` |
|     1 | 8141 | `{` |
|    27 | 8142 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     - | 8143 | `	sxu32 n;` |
|    27 | 8144 | `	if( nName < 1 ){` |
|   ! 0 | 8145 | `		return 0;` |
|     - | 8146 | `	}` |
|    59 | 8147 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|    44 | 8148 | `		if( (int)SyStringLength(&apCase[n]->sName) == nName` |
|    33 | 8149 | `		 && SyMemcmp(SyStringData(&apCase[n]->sName), zName, (sxu32)nName) == 0 ){` |
|    13 | 8150 | `			return apCase[n];` |
|     - | 8151 | `		}` |
|    17 | 8152 | `	}` |
|    15 | 8153 | `	return 0;` |
|    14 | 8154 | `}` |
|     - | 8155 | `/*` |
|     - | 8156 | ` * One case reflector for an already-validated case. php answers a BackedCase` |
|     - | 8157 | ` * for a backed enum and a UnitCase for a pure one, and both carry the same two` |
|     - | 8158 | ` * slots ReflectionClassConstant does — an enum cannot extend anything, so the` |
|     - | 8159 | ` * declaring class is always the enum itself.` |
|     - | 8160 | ` */` |
|    28 | 8161 | `static ph7_class_instance * ReflectEnumCaseNew(ph7_context *pCtx, ph7_class *pEnum,` |
|     - | 8162 | `	const SyString *pCase)` |
|     1 | 8163 | `{` |
|    29 | 8164 | `	ph7_vm *pVm = pCtx->pVm;` |
|    43 | 8165 | `	const char *zClass = pEnum->nEnumBacking` |
|    14 | 8166 | `		? "ReflectionEnumBackedCase" : "ReflectionEnumUnitCase";` |
|    29 | 8167 | `	ph7_class *pRC = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);` |
|    29 | 8168 | `	ph7_class_instance *pObj = pRC ? PH7_NewClassInstance(pVm, pRC) : 0;` |
|    29 | 8169 | `	if( pObj == 0 ){` |
|   ! 0 | 8170 | `		return 0;` |
|     - | 8171 | `	}` |
|    43 | 8172 | `	PH7_NativeSetAttrStr(pVm, pObj, "class", SyStringData(&pEnum->sName),` |
|    28 | 8173 | `		(int)SyStringLength(&pEnum->sName));` |
|    29 | 8174 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(pCase), (int)SyStringLength(pCase));` |
|    29 | 8175 | `	return pObj;` |
|    15 | 8176 | `}` |
|     - | 8177 | `/* ReflectionEnum::__construct(object\|string $objectOrClass) */` |
|    24 | 8178 | `static int vm_builtin_ReflectionEnum_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 8179 | `{` |
|     - | 8180 | `	ph7_class *pClass;` |
|    25 | 8181 | `	sxi32 rc = vm_builtin_ReflectionClass_construct(pCtx, nArg, apArg);` |
|    25 | 8182 | `	if( rc != PH7_OK ){` |
|     5 | 8183 | `		return rc; /* "Class %s does not exist", already php's */` |
|     - | 8184 | `	}` |
|    21 | 8185 | `	pClass = ReflectClassOf(pCtx);` |
|    21 | 8186 | `	if( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){` |
|    10 | 8187 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     3 | 8188 | `			"Class \"%z\" is not an enum", &pClass->sName);` |
|     - | 8189 | `	}` |
|    15 | 8190 | `	return PH7_OK;` |
|    13 | 8191 | `}` |
|    12 | 8192 | `static int vm_builtin_ReflectionEnum_hasCase(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 8193 | `{` |
|    13 | 8194 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 8195 | `	const char *zName;` |
|     - | 8196 | `	int nName;` |
|    13 | 8197 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|   ! 0 | 8198 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 | 8199 | `		return PH7_OK;` |
|     - | 8200 | `	}` |
|    13 | 8201 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|    13 | 8202 | `	ph7_result_bool(pCtx, ReflectEnumCase(pClass, zName, nName) != 0);` |
|    13 | 8203 | `	return PH7_OK;` |
|     7 | 8204 | `}` |
|     - | 8205 | `/*` |
|     - | 8206 | ` * getCase(): php tells the two failures apart — a name that is a constant but` |
|     - | 8207 | ` * not a case is "X::K is not a case", one that is neither is "Case X::K does` |
|     - | 8208 | ` * not exist". The chunk answered the second for both.` |
|     - | 8209 | ` */` |
|    14 | 8210 | `static int vm_builtin_ReflectionEnum_getCase(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 8211 | `{` |
|    15 | 8212 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 8213 | `	ph7_class_attr *pCase;` |
|     - | 8214 | `	const char *zName;` |
|     - | 8215 | `	int nName;` |
|    15 | 8216 | `	if( pClass == 0 \|\| nArg < 1 ){` |
|   ! 0 | 8217 | `		ph7_result_null(pCtx);` |
|   ! 0 | 8218 | `		return PH7_OK;` |
|     - | 8219 | `	}` |
|    15 | 8220 | `	zName = ph7_value_to_string(apArg[0], &nName);` |
|    15 | 8221 | `	pCase = ReflectEnumCase(pClass, zName, nName);` |
|    15 | 8222 | `	if( pCase == 0 ){` |
|     7 | 8223 | `		if( nName > 0 && SyHashGet(&pClass->hConst, (const void *)zName, (sxu32)nName) ){` |
|     4 | 8224 | `			return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     1 | 8225 | `				"%z::%.*s is not a case", &pClass->sName, nName, zName);` |
|     - | 8226 | `		}` |
|     7 | 8227 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     2 | 8228 | `			"Case %z::%.*s does not exist", &pClass->sName, nName, zName);` |
|     - | 8229 | `	}` |
|     9 | 8230 | `	return ReflectResultObject(pCtx, ReflectEnumCaseNew(pCtx, pClass, &pCase->sName));` |
|     8 | 8231 | `}` |
|    12 | 8232 | `static int vm_builtin_ReflectionEnum_getCases(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 8233 | `{` |
|    13 | 8234 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|    13 | 8235 | `	ph7_value *pOut = ph7_context_new_array(pCtx);` |
|     6 | 8236 | `	SXUNUSED(nArg);` |
|     6 | 8237 | `	SXUNUSED(apArg);` |
|    13 | 8238 | `	if( pOut == 0 ){` |
|   ! 0 | 8239 | `		return PH7_ContextMemoryError(pCtx);` |
|     - | 8240 | `	}` |
|    13 | 8241 | `	if( pClass ){` |
|    13 | 8242 | `		ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     - | 8243 | `		sxu32 n;` |
|    33 | 8244 | `		for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|    21 | 8245 | `			ReflectTypeListAdd(pCtx, pOut, ReflectEnumCaseNew(pCtx, pClass, &apCase[n]->sName));` |
|    11 | 8246 | `		}` |
|     6 | 8247 | `	}` |
|    13 | 8248 | `	ph7_result_value(pCtx, pOut);` |
|    13 | 8249 | `	return PH7_OK;` |
|     7 | 8250 | `}` |
|    10 | 8251 | `static int vm_builtin_ReflectionEnum_isBacked(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 8252 | `{` |
|    11 | 8253 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     5 | 8254 | `	SXUNUSED(nArg);` |
|     5 | 8255 | `	SXUNUSED(apArg);` |
|    11 | 8256 | `	ph7_result_bool(pCtx, pClass != 0 && pClass->nEnumBacking != 0);` |
|    11 | 8257 | `	return PH7_OK;` |
|     1 | 8258 | `}` |
|    16 | 8259 | `static int vm_builtin_ReflectionEnum_getBackingType(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 8260 | `{` |
|    17 | 8261 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 8262 | `	const char *zText;` |
|     - | 8263 | `	ph7_class_instance *pType;` |
|     8 | 8264 | `	SXUNUSED(nArg);` |
|     8 | 8265 | `	SXUNUSED(apArg);` |
|    17 | 8266 | `	if( pClass == 0 \|\| pClass->nEnumBacking == 0 ){` |
|     5 | 8267 | `		ph7_result_null(pCtx); /* a PURE enum has no backing type */` |
|     5 | 8268 | `		return PH7_OK;` |
|     - | 8269 | `	}` |
|    13 | 8270 | `	zText = (pClass->nEnumBacking & MEMOBJ_INT) ? "int" : "string";` |
|    13 | 8271 | `	pType = ReflectMakeType(pCtx, zText, (int)SyStrlen(zText));` |
|    13 | 8272 | `	if( pType == 0 ){` |
|   ! 0 | 8273 | `		ph7_result_null(pCtx);` |
|   ! 0 | 8274 | `		return PH7_OK;` |
|     - | 8275 | `	}` |
|    13 | 8276 | `	PH7_NativeResultObject(pCtx, pType);` |
|    13 | 8277 | `	return PH7_OK;` |
|     9 | 8278 | `}` |
|     - | 8279 | `/*` |
|     - | 8280 | ` * ReflectionEnumUnitCase::__construct(object\|string $class, string $constant).` |
|     - | 8281 | ` *` |
|     - | 8282 | ` * The parent raises for a constant that does not exist; what is left is "not a` |
|     - | 8283 | ` * case", and php words that one WITHOUT asking whether the class is an enum at` |
|     - | 8284 | `` * all — `new ReflectionEnumUnitCase('Plain', 'K')` on an ordinary class says`` |
|     - | 8285 | `` * `Constant Plain::K is not a case`, not "is not an enum" as the chunk did.`` |
|     - | 8286 | ` * A non-enum's constant can never carry the ENUMCASE bit, so the one screen` |
|     - | 8287 | ` * answers both shapes.` |
|     - | 8288 | ` */` |
|    18 | 8289 | `static int vm_builtin_ReflectionEnumCase_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 8290 | `{` |
|     - | 8291 | `	ReflectMemberRef sRef;` |
|    19 | 8292 | `	sxi32 rc = vm_builtin_ReflectionClassConstant_construct(pCtx, nArg, apArg);` |
|    19 | 8293 | `	if( rc != PH7_OK ){` |
|     3 | 8294 | `		return rc;` |
|     - | 8295 | `	}` |
|    17 | 8296 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|   ! 0 | 8297 | `		return PH7_OK;` |
|     - | 8298 | `	}` |
|    17 | 8299 | `	if( (sRef.pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){` |
|    10 | 8300 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     6 | 8301 | `			"Constant %z::%.*s is not a case", &sRef.pClass->sName, sRef.nName, sRef.zName);` |
|     - | 8302 | `	}` |
|    11 | 8303 | `	return PH7_OK;` |
|    10 | 8304 | `}` |
|     - | 8305 | `/* ReflectionEnumBackedCase::__construct() — the same, plus php's screen for a` |
|     - | 8306 | ` * PURE enum's case, which has no backing value to answer with. */` |
|     6 | 8307 | `static int vm_builtin_ReflectionEnumBackedCase_construct(ph7_context *pCtx, int nArg,` |
|     - | 8308 | `	ph7_value **apArg)` |
|     1 | 8309 | `{` |
|     - | 8310 | `	ReflectMemberRef sRef;` |
|     7 | 8311 | `	sxi32 rc = vm_builtin_ReflectionEnumCase_construct(pCtx, nArg, apArg);` |
|     7 | 8312 | `	if( rc != PH7_OK ){` |
|   ! 0 | 8313 | `		return rc;` |
|     - | 8314 | `	}` |
|     7 | 8315 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|   ! 0 | 8316 | `		return PH7_OK;` |
|     - | 8317 | `	}` |
|     7 | 8318 | `	if( sRef.pClass->nEnumBacking == 0 ){` |
|     4 | 8319 | `		return PH7_VmThrowException(pCtx, "ReflectionException",` |
|     2 | 8320 | `			"Enum case %z::%.*s is not a backed case", &sRef.pClass->sName,` |
|     1 | 8321 | `			sRef.nName, sRef.zName);` |
|     - | 8322 | `	}` |
|     5 | 8323 | `	return PH7_OK;` |
|     4 | 8324 | `}` |
|     6 | 8325 | `static int vm_builtin_ReflectionEnumCase_getEnum(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 8326 | `{` |
|     7 | 8327 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 8328 | `	ReflectMemberRef sRef;` |
|     - | 8329 | `	ph7_class *pRC;` |
|     - | 8330 | `	ph7_class_instance *pObj;` |
|     3 | 8331 | `	SXUNUSED(nArg);` |
|     3 | 8332 | `	SXUNUSED(apArg);` |
|     7 | 8333 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) ){` |
|   ! 0 | 8334 | `		ph7_result_null(pCtx);` |
|   ! 0 | 8335 | `		return PH7_OK;` |
|     - | 8336 | `	}` |
|     7 | 8337 | `	pRC = PH7_VmExtractClass(pVm, "ReflectionEnum", sizeof("ReflectionEnum")-1, FALSE, 0);` |
|     7 | 8338 | `	pObj = pRC ? PH7_NewClassInstance(pVm, pRC) : 0;` |
|     7 | 8339 | `	if( pObj == 0 ){` |
|   ! 0 | 8340 | `		ph7_result_null(pCtx);` |
|   ! 0 | 8341 | `		return PH7_OK;` |
|     - | 8342 | `	}` |
|    10 | 8343 | `	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(&sRef.pClass->sName),` |
|     6 | 8344 | `		(int)SyStringLength(&sRef.pClass->sName));` |
|     7 | 8345 | `	return ReflectResultObject(pCtx, pObj);` |
|     4 | 8346 | `}` |
|     - | 8347 | ``/* getBackingValue(): the `value` the case singleton carries. The singleton is`` |
|     - | 8348 | ` * the constant's own value, so the parent's accessor materializes it. */` |
|    16 | 8349 | `static int vm_builtin_ReflectionEnumCase_getBackingValue(ph7_context *pCtx, int nArg,` |
|     - | 8350 | `	ph7_value **apArg)` |
|     1 | 8351 | `{` |
|     - | 8352 | `	ReflectMemberRef sRef;` |
|     - | 8353 | `	ph7_value *pVal, *pBacking;` |
|     - | 8354 | `	sxi32 rc;` |
|     8 | 8355 | `	SXUNUSED(nArg);` |
|     8 | 8356 | `	SXUNUSED(apArg);` |
|    17 | 8357 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|   ! 0 | 8358 | `		ph7_result_null(pCtx);` |
|   ! 0 | 8359 | `		return PH7_OK;` |
|     - | 8360 | `	}` |
|    17 | 8361 | `	rc = ReflectConstSlot(pCtx, sRef.pClass, sRef.pAttr, &pVal);` |
|    17 | 8362 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 8363 | `		return rc;` |
|     - | 8364 | `	}` |
|    17 | 8365 | `	pBacking = (pVal && (pVal->iFlags & MEMOBJ_OBJ))` |
|    24 | 8366 | `		? PH7_NativeAttr((ph7_class_instance *)pVal->x.pOther, "value") : 0;` |
|    17 | 8367 | `	if( pBacking ){` |
|    17 | 8368 | `		ph7_result_value(pCtx, pBacking);` |
|     9 | 8369 | `	}else{` |
|   ! 0 | 8370 | `		ph7_result_null(pCtx);` |
|     - | 8371 | `	}` |
|    17 | 8372 | `	return PH7_OK;` |
|     9 | 8373 | `}` |
|     - | 8374 | `/*` |
|     - | 8375 | ` * Declare the three. Called from PH7_VmInstallReflection() where chunk 6 used` |
|     - | 8376 | ` * to be compiled, so both parents are installed (PH7_ClassInherit COPIES a` |
|     - | 8377 | ` * base's methods down — a native subclass needs its parent declared first).` |
|     - | 8378 | ` *` |
|     - | 8379 | ` * The uncloneable/unserializable flags are php's answer for each class and do` |
|     - | 8380 | ``  * NOT come down with the inheritance: without them `clone $enumReflector` `` |
|     - | 8381 | ` * reached the private __clone it inherited and reported that instead.` |
|     - | 8382 | ` */` |
|  5146 | 8383 | `PH7_PRIVATE sxi32 PH7_VmInstallReflectionEnum(ph7_vm *pVm)` |
|     5 | 8384 | `{` |
|     - | 8385 | `	static const PH7_NativeMethodDef aEnumMethod[] = {` |
|     - | 8386 | `		{ "__construct",    PH7_MOD_PUBLIC, "object\|string $objectOrClass", "",` |
|     - | 8387 | `		  vm_builtin_ReflectionEnum_construct },` |
|     - | 8388 | `		{ "hasCase",        PH7_MOD_PUBLIC, "string $name", "bool",` |
|     - | 8389 | `		  vm_builtin_ReflectionEnum_hasCase },` |
|     - | 8390 | `		{ "getCase",        PH7_MOD_PUBLIC, "string $name", "ReflectionEnumUnitCase",` |
|     - | 8391 | `		  vm_builtin_ReflectionEnum_getCase },` |
|     - | 8392 | `		{ "getCases",       PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionEnum_getCases },` |
|     - | 8393 | `		{ "isBacked",       PH7_MOD_PUBLIC, "", "bool",  vm_builtin_ReflectionEnum_isBacked },` |
|     - | 8394 | `		{ "getBackingType", PH7_MOD_PUBLIC, "", "?ReflectionNamedType",` |
|     - | 8395 | `		  vm_builtin_ReflectionEnum_getBackingType },` |
|     - | 8396 | `	};` |
|     - | 8397 | `	static const PH7_NativeMethodDef aCaseMethod[] = {` |
|     - | 8398 | `		{ "__construct", PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|     - | 8399 | `		  vm_builtin_ReflectionEnumCase_construct },` |
|     - | 8400 | `		{ "getEnum",     PH7_MOD_PUBLIC, "", "ReflectionEnum",` |
|     - | 8401 | `		  vm_builtin_ReflectionEnumCase_getEnum },` |
|     - | 8402 | `		/* php redeclares getValue() on the case reflector for its narrower` |
|     - | 8403 | `		 * return type; the body is the parent's. */` |
|     - | 8404 | `		{ "getValue",    PH7_MOD_PUBLIC, "", "UnitEnum",` |
|     - | 8405 | `		  vm_builtin_ReflectionClassConstant_getValue },` |
|     - | 8406 | `	};` |
|     - | 8407 | `	static const PH7_NativeMethodDef aBackedMethod[] = {` |
|     - | 8408 | `		{ "__construct",     PH7_MOD_PUBLIC, "object\|string $class, string $constant", "",` |
|     - | 8409 | `		  vm_builtin_ReflectionEnumBackedCase_construct },` |
|     - | 8410 | `		{ "getBackingValue", PH7_MOD_PUBLIC, "", "string\|int",` |
|     - | 8411 | `		  vm_builtin_ReflectionEnumCase_getBackingValue },` |
|     - | 8412 | `	};` |
|     - | 8413 | `	static const PH7_NativeClassSpec aSpec[] = {` |
|     - | 8414 | `		{ "ReflectionEnum", "ReflectionClass", 0, PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 8415 | `		  aEnumMethod, SX_ARRAYSIZE(aEnumMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 8416 | `		{ "ReflectionEnumUnitCase", "ReflectionClassConstant", 0,` |
|     - | 8417 | `		  PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 8418 | `		  aCaseMethod, SX_ARRAYSIZE(aCaseMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 8419 | `		{ "ReflectionEnumBackedCase", "ReflectionEnumUnitCase", 0,` |
|     - | 8420 | `		  PH7_CLASS_NOCLONE\|PH7_CLASS_NOSERIALIZE,` |
|     - | 8421 | `		  aBackedMethod, SX_ARRAYSIZE(aBackedMethod), 0, 0, 0, 0, 0, 0, 0 },` |
|     - | 8422 | `	};` |
|  5151 | 8423 | `	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));` |
|     5 | 8424 | `}` |
|     - | 8425 | `/*` |
|     - | 8426 | ` * ---------------------------------------------------------------------------` |
|     - | 8427 | ` * php's Reflection EXPORT format — chunk 9, the last of the library's PHP.` |
|     - | 8428 | ` *` |
|     - | 8429 | ` * Every Reflector's __toString(). It is a byte-exact format with no API of its` |
|     - | 8430 | ` * own, so the chunk was written against the PUBLIC reflection API of whatever` |
|     - | 8431 | ` * it was printing — the only thing PHP could reach. All of those targets are C` |
|     - | 8432 | ` * now, so this reads the engine directly: the member walk for order and` |
|     - | 8433 | ` * visibility, ReflectParamAt for parameters, ReflectExportValue (built for the` |
|     - | 8434 | ` * attribute-argument block) for every value.` |
|     - | 8435 | ` *` |
|     - | 8436 | ` * Defined at the END of the file because it needs all of that; the five entry` |
|     - | 8437 | ` * points are forward-declared above their callers.` |
|     - | 8438 | ` * ---------------------------------------------------------------------------` |
|     - | 8439 | ` */` |
|     - | 8440 |  |
|     - | 8441 | `/* "internal:Core" / "user" — the first tag of every function and class head. */` |
|   156 | 8442 | `static void ReflectExportKind(SyBlob *pOut, int bInternal)` |
|     2 | 8443 | `{` |
|   158 | 8444 | `	if( bInternal ){` |
|    88 | 8445 | `		SyBlobAppend(pOut, "internal:Core", sizeof("internal:Core")-1);` |
|    45 | 8446 | `	}else{` |
|    71 | 8447 | `		SyBlobAppend(pOut, "user", sizeof("user")-1);` |
|     - | 8448 | `	}` |
|   158 | 8449 | `}` |
|     - | 8450 | `/* A declared type followed by a space, or nothing at all. */` |
|   174 | 8451 | `static void ReflectExportTypeSp(SyBlob *pOut, const SyString *pType)` |
|     2 | 8452 | `{` |
|   176 | 8453 | `	if( pType && SyStringLength(pType) > 0 ){` |
|   140 | 8454 | `		SyBlobAppend(pOut, SyStringData(pType), SyStringLength(pType));` |
|   140 | 8455 | `		SyBlobAppend(pOut, " ", sizeof(char));` |
|    69 | 8456 | `	}` |
|   176 | 8457 | `}` |
|     - | 8458 | `/* php's visibility word for a member. */` |
|   208 | 8459 | `static const char * ReflectExportVis(sxi32 iProtection)` |
|     1 | 8460 | `{` |
|   209 | 8461 | `	if( iProtection == PH7_CLASS_PROT_PRIVATE ){` |
|    11 | 8462 | `		return "private";` |
|     - | 8463 | `	}` |
|   199 | 8464 | `	return iProtection == PH7_CLASS_PROT_PROTECTED ? "protected" : "public";` |
|   105 | 8465 | `}` |
|     - | 8466 | `/*` |
|     - | 8467 | `` * The text after `= ` in a parameter default.`` |
|     - | 8468 | ` *` |
|     - | 8469 | ` * php prints a constant-reference default as the CONSTANT's name, not its value` |
|     - | 8470 | `` * (`$f = M_PI`) — that argument was never folded, so php still has the source`` |
|     - | 8471 | `` * expression. `<default>` is what the chunk answered when the value could not`` |
|     - | 8472 | `` * be produced at all, which is a declared `= ?` row in aBuiltinSig[].`` |
|     - | 8473 | ` */` |
|    60 | 8474 | `static void ReflectExportDefault(ph7_context *pCtx, SyBlob *pOut, ReflectParamDesc *pDesc)` |
|     1 | 8475 | `{` |
|    61 | 8476 | `	const char *z = 0;` |
|    61 | 8477 | `	int n = 0;` |
|    61 | 8478 | `	if( ReflectParamDefConst(pCtx, pDesc, &z, &n) ){` |
|     7 | 8479 | `		SyBlobAppend(pOut, z, (sxu32)n);` |
|    34 | 8480 | `		return;` |
|     - | 8481 | `	}` |
|    55 | 8482 | `	if( pDesc->pArg ){` |
|     - | 8483 | `		ph7_value sValue;` |
|    39 | 8484 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|    39 | 8485 | `		VmLocalExec(pCtx->pVm, &pDesc->pArg->aByteCode, &sValue, FALSE);` |
|    39 | 8486 | `		ReflectExportValue(pCtx, pOut, &sValue, 0);` |
|    39 | 8487 | `		PH7_MemObjRelease(&sValue);` |
|    39 | 8488 | `		return;` |
|     - | 8489 | `	}` |
|     - | 8490 | `	{` |
|     - | 8491 | `		/* A DECLARED default is signature TEXT: reduce it the way` |
|     - | 8492 | `		 * getDefaultValue() does, and fall back to php's own placeholder.` |
|     - | 8493 | `		 * Only an INTERNAL parameter reaches this branch (a compiled one carries` |
|     - | 8494 | `		 * pArg above), which is exactly the case php prints DOUBLE-quoted. */` |
|    17 | 8495 | `		const char *zDef = SyStringData(&pDesc->sDefText);` |
|    17 | 8496 | `		int nDef = (int)SyStringLength(&pDesc->sDefText);` |
|    17 | 8497 | `		ph7_value *pVal = ph7_context_new_scalar(pCtx);` |
|    17 | 8498 | `		if( ReflectSigHas(zDef, nDef, "::", 2) ){` |
|     - | 8499 | `			/* Rule 28's split, on the declared side: php's stub keeps the SOURCE of a` |
|     - | 8500 | `			 * constant-expression default, so the export prints` |
|     - | 8501 | ``			 * `= SplFileInfo::class` and `= A::K \| A::J` verbatim while`` |
|     - | 8502 | `			 * getDefaultValue() answers what they evaluate to. */` |
|     3 | 8503 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|     3 | 8504 | `			return;` |
|     - | 8505 | `		}` |
|    15 | 8506 | `		if( pVal && ReflectSigGlobalConst(pCtx, zDef, nDef, pVal) ){` |
|     - | 8507 | ``			/* Same split for a GLOBAL constant: `= E_ERROR`, never `= 1`. */`` |
|   ! 0 | 8508 | `			SyBlobAppend(pOut, zDef, (sxu32)nDef);` |
|   ! 0 | 8509 | `			return;` |
|     - | 8510 | `		}` |
|    15 | 8511 | `		if( pVal && ReflectSigScalar(pCtx, zDef, nDef, pVal) ){` |
|    15 | 8512 | `			if( (pVal->iFlags & (MEMOBJ_STRING\|MEMOBJ_NULL)) == MEMOBJ_STRING ){` |
|    16 | 8513 | `				ReflectExportStrQ(pOut, (const char *)SyBlobData(&pVal->sBlob),` |
|     5 | 8514 | `					SyBlobLength(&pVal->sBlob), '"');` |
|    11 | 8515 | `				return;` |
|     - | 8516 | `			}` |
|     5 | 8517 | `			if( pVal->iFlags & MEMOBJ_NULL ){` |
|     - | 8518 | `				/* The same internal/user split as the quote character above: php` |
|     - | 8519 | ``				 * spells an INTERNAL parameter's null default `null` and a`` |
|     - | 8520 | ``				 * userland one `NULL` (`= null` for every `?T $x = null` stub row). */`` |
|     5 | 8521 | `				SyBlobAppend(pOut, "null", sizeof("null")-1);` |
|     5 | 8522 | `				return;` |
|     - | 8523 | `			}` |
|   ! 0 | 8524 | `			ReflectExportValue(pCtx, pOut, pVal, 0);` |
|   ! 0 | 8525 | `			return;` |
|     - | 8526 | `		}` |
|   ! 0 | 8527 | `		if( (nDef >= 1 && zDef[0] == '[')` |
|   ! 0 | 8528 | `		 \|\| (nDef >= (int)sizeof("array (")-1` |
|   ! 0 | 8529 | `		  && SyMemcmp(zDef, "array (", sizeof("array (")-1) == 0) ){` |
|   ! 0 | 8530 | `			SyBlobAppend(pOut, "[]", sizeof("[]")-1);` |
|   ! 0 | 8531 | `			return;` |
|     - | 8532 | `		}` |
|     - | 8533 | `	}` |
|   ! 0 | 8534 | `	SyBlobAppend(pOut, "<default>", sizeof("<default>")-1);` |
|    31 | 8535 | `}` |
|     - | 8536 | ``/* `Parameter #0 [ <optional> int &...$name = 5 ]` */`` |
|   126 | 8537 | `static void ReflectExportParamLine(ph7_context *pCtx, SyBlob *pOut, ReflectParamDesc *pDesc)` |
|     2 | 8538 | `{` |
|   191 | 8539 | `	SyBlobFormat(pOut, "Parameter #%d [ <%s> ", pDesc->iPos,` |
|   126 | 8540 | `		pDesc->bOptional ? "optional" : "required");` |
|   128 | 8541 | `	ReflectExportTypeSp(pOut, &pDesc->sType);` |
|   128 | 8542 | `	if( pDesc->bByRef ){` |
|    11 | 8543 | `		SyBlobAppend(pOut, "&", sizeof(char));` |
|     5 | 8544 | `	}` |
|   128 | 8545 | `	if( pDesc->bVariadic ){` |
|    13 | 8546 | `		SyBlobAppend(pOut, "...", sizeof("...")-1);` |
|     6 | 8547 | `	}` |
|   128 | 8548 | `	SyBlobAppend(pOut, "$", sizeof(char));` |
|   128 | 8549 | `	SyBlobAppend(pOut, SyStringData(&pDesc->sName), SyStringLength(&pDesc->sName));` |
|   128 | 8550 | `	if( pDesc->bHasDef ){` |
|    61 | 8551 | `		SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|    61 | 8552 | `		ReflectExportDefault(pCtx, pOut, pDesc);` |
|    30 | 8553 | `	}` |
|   128 | 8554 | `	SyBlobAppend(pOut, " ]", sizeof(" ]")-1);` |
|   128 | 8555 | `}` |
|     - | 8556 | `/*` |
|     - | 8557 | `` * `Property [ public protected(set) readonly int $x = 5 ]` + newline.`` |
|     - | 8558 | ` *` |
|     - | 8559 | ` * The set-visibility is printed only when it DIFFERS from the get-visibility,` |
|     - | 8560 | `` * which is why a `public readonly` property shows php's implied`` |
|     - | 8561 | `` * `protected(set)` and a `protected readonly` one shows nothing.`` |
|     - | 8562 | ` */` |
|    58 | 8563 | `static void ReflectExportPropLine(ph7_context *pCtx, SyBlob *pOut, ph7_class_attr *pAttr,` |
|     - | 8564 | `	const SyString *pKey)` |
|     1 | 8565 | `{` |
|    59 | 8566 | `	sxi32 iSet = pAttr->iProtection;` |
|    59 | 8567 | `	SyBlobAppend(pOut, "Property [ ", sizeof("Property [ ")-1);` |
|     - | 8568 | `	/* php's modifier MASK implies two bits the declaration never wrote:` |
|     - | 8569 | `	 * private(set) implies final, and readonly implies protected(set). */` |
|    59 | 8570 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|     3 | 8571 | `		SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|     1 | 8572 | `	}` |
|    59 | 8573 | `	SyBlobFormat(pOut, "%s ", ReflectExportVis(pAttr->iProtection));` |
|    59 | 8574 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){` |
|     3 | 8575 | `		iSet = PH7_CLASS_PROT_PRIVATE;` |
|    58 | 8576 | `	}else if( (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET)` |
|    57 | 8577 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){` |
|    13 | 8578 | `		iSet = PH7_CLASS_PROT_PROTECTED;` |
|     6 | 8579 | `	}` |
|     - | 8580 | `	/* The set-visibility is printed only when it DIFFERS from the get one. */` |
|    59 | 8581 | `	if( iSet != pAttr->iProtection ){` |
|    11 | 8582 | `		SyBlobFormat(pOut, "%s(set) ", ReflectExportVis(iSet));` |
|     5 | 8583 | `	}` |
|    59 | 8584 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|     7 | 8585 | `		SyBlobAppend(pOut, "static ", sizeof("static ")-1);` |
|     3 | 8586 | `	}` |
|    59 | 8587 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){` |
|   ! 0 | 8588 | `		SyBlobAppend(pOut, "virtual ", sizeof("virtual ")-1);` |
|   ! 0 | 8589 | `	}` |
|    59 | 8590 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){` |
|    13 | 8591 | `		SyBlobAppend(pOut, "readonly ", sizeof("readonly ")-1);` |
|     6 | 8592 | `	}` |
|    59 | 8593 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){` |
|    49 | 8594 | `		ReflectExportTypeSp(pOut, &pAttr->sTypeName);` |
|    24 | 8595 | `	}` |
|    59 | 8596 | `	SyBlobAppend(pOut, "$", sizeof(char));` |
|    59 | 8597 | `	SyBlobAppend(pOut, SyStringData(pKey), SyStringLength(pKey));` |
|    58 | 8598 | `	if( SySetUsed(&pAttr->aByteCode) > 0 \|\| pAttr->pNativeValue` |
|    31 | 8599 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){` |
|     - | 8600 | `		ph7_value sValue;` |
|    31 | 8601 | `		SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|    31 | 8602 | `		PH7_MemObjInit(pCtx->pVm, &sValue);` |
|    31 | 8603 | `		if( SySetUsed(&pAttr->aByteCode) > 0 ){` |
|    29 | 8604 | `			VmLocalExec(pCtx->pVm, &pAttr->aByteCode, &sValue, FALSE);` |
|    17 | 8605 | `		}else if( pAttr->pNativeValue ){` |
|   ! 0 | 8606 | `			PH7_NativeLiteralValue(pCtx->pVm, pAttr->pNativeValue, &sValue);` |
|   ! 0 | 8607 | `		}` |
|    31 | 8608 | `		ReflectExportValue(pCtx, pOut, &sValue, 0);` |
|    31 | 8609 | `		PH7_MemObjRelease(&sValue);` |
|    15 | 8610 | `	}` |
|     - | 8611 | `	/* A hooked property names its hooks; php lists no method for them. */` |
|    59 | 8612 | `	if( pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET) ){` |
|     3 | 8613 | `		SyBlobAppend(pOut, " {", sizeof(" {")-1);` |
|     3 | 8614 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET ){` |
|     3 | 8615 | `			SyBlobAppend(pOut, " get;", sizeof(" get;")-1);` |
|     1 | 8616 | `		}` |
|     3 | 8617 | `		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET ){` |
|     3 | 8618 | `			SyBlobAppend(pOut, " set;", sizeof(" set;")-1);` |
|     1 | 8619 | `		}` |
|     3 | 8620 | `		SyBlobAppend(pOut, " }", sizeof(" }")-1);` |
|     1 | 8621 | `	}` |
|    59 | 8622 | `	SyBlobAppend(pOut, " ]\n", sizeof(" ]\n")-1);` |
|    59 | 8623 | `}` |
|     - | 8624 | `/*` |
|     - | 8625 | `` * `Constant [ final public int NAME ] { value }` + newline. The TYPE is the`` |
|     - | 8626 | ` * value's, not a declared one, and an object (an enum case) prints as the word` |
|     - | 8627 | ` * Object under its own class name.` |
|     - | 8628 | ` */` |
|    24 | 8629 | `static sxi32 ReflectExportConstLine(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass,` |
|     - | 8630 | `	ph7_class_attr *pAttr, const SyString *pKey)` |
|     1 | 8631 | `{` |
|    25 | 8632 | `	ph7_value *pVal = 0;` |
|    25 | 8633 | `	sxi32 rc = ReflectConstSlot(pCtx, pClass, pAttr, &pVal);` |
|    25 | 8634 | `	const char *zType = "null";` |
|    25 | 8635 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 8636 | `		return rc;` |
|     - | 8637 | `	}` |
|    25 | 8638 | `	SyBlobAppend(pOut, "Constant [ ", sizeof("Constant [ ")-1);` |
|    25 | 8639 | `	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|     7 | 8640 | `		SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|     3 | 8641 | `	}` |
|    25 | 8642 | `	SyBlobFormat(pOut, "%s ", ReflectExportVis(pAttr->iProtection));` |
|    25 | 8643 | `	if( pVal ){` |
|    25 | 8644 | `		if( (pVal->iFlags & (MEMOBJ_OBJ\|MEMOBJ_NULL)) == MEMOBJ_OBJ ){` |
|     3 | 8645 | `			zType = 0; /* the object's own class */` |
|    24 | 8646 | `		}else if( pVal->iFlags & MEMOBJ_NULL ){` |
|   ! 0 | 8647 | `			zType = "null";` |
|    23 | 8648 | `		}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|     3 | 8649 | `			zType = "array";` |
|    22 | 8650 | `		}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|   ! 0 | 8651 | `			zType = "bool";` |
|    21 | 8652 | `		}else if( pVal->iFlags & MEMOBJ_REAL ){` |
|   ! 0 | 8653 | `			zType = "float";` |
|    21 | 8654 | `		}else if( pVal->iFlags & MEMOBJ_INT ){` |
|    13 | 8655 | `			zType = "int";` |
|    15 | 8656 | `		}else if( pVal->iFlags & MEMOBJ_STRING ){` |
|     9 | 8657 | `			zType = "string";` |
|     4 | 8658 | `		}` |
|    12 | 8659 | `	}` |
|    25 | 8660 | `	if( zType ){` |
|    23 | 8661 | `		SyBlobFormat(pOut, "%s ", zType);` |
|    12 | 8662 | `	}else{` |
|     3 | 8663 | `		SyBlobFormat(pOut, "%z ", &((ph7_class_instance *)pVal->x.pOther)->pClass->sName);` |
|     - | 8664 | `	}` |
|    25 | 8665 | `	SyBlobAppend(pOut, SyStringData(pKey), SyStringLength(pKey));` |
|    25 | 8666 | `	SyBlobAppend(pOut, " ] { ", sizeof(" ] { ")-1);` |
|    25 | 8667 | `	if( pVal == 0 \|\| (pVal->iFlags & MEMOBJ_NULL) ){` |
|     - | 8668 | `		/* php prints nothing at all for null */` |
|    24 | 8669 | `	}else if( pVal->iFlags & MEMOBJ_HASHMAP ){` |
|     3 | 8670 | `		SyBlobAppend(pOut, "Array", sizeof("Array")-1);` |
|    24 | 8671 | `	}else if( (pVal->iFlags & MEMOBJ_OBJ) ){` |
|     3 | 8672 | `		SyBlobAppend(pOut, "Object", sizeof("Object")-1);` |
|    22 | 8673 | `	}else if( pVal->iFlags & MEMOBJ_BOOL ){` |
|   ! 0 | 8674 | `		if( pVal->x.iVal ){` |
|   ! 0 | 8675 | `			SyBlobAppend(pOut, "1", sizeof(char));` |
|   ! 0 | 8676 | `		}` |
|   ! 0 | 8677 | `	}else{` |
|     - | 8678 | `		ph7_value sTmp;` |
|     - | 8679 | `		const char *zText;` |
|     - | 8680 | `		int nText;` |
|    21 | 8681 | `		PH7_MemObjInit(pCtx->pVm, &sTmp);` |
|    21 | 8682 | `		PH7_MemObjStore(pVal, &sTmp);` |
|    21 | 8683 | `		zText = ph7_value_to_string(&sTmp, &nText);` |
|    21 | 8684 | `		if( nText > 0 ){` |
|    21 | 8685 | `			SyBlobAppend(pOut, zText, (sxu32)nText);` |
|    10 | 8686 | `		}` |
|    21 | 8687 | `		PH7_MemObjRelease(&sTmp);` |
|     - | 8688 | `	}` |
|    25 | 8689 | `	SyBlobAppend(pOut, " }\n", sizeof(" }\n")-1);` |
|    25 | 8690 | `	return SXRET_OK;` |
|    13 | 8691 | `}` |
|     - | 8692 | `/* A native class METHOD as a function reference (ReflectFuncFill's tail, for a` |
|     - | 8693 | ` * method the member walk handed over rather than one a receiver names). */` |
|    74 | 8694 | `static void ReflectFuncFromMethod(ph7_class *pClass, ph7_class_method *pMeth,` |
|     - | 8695 | `	ReflectFuncRef *pOut)` |
|     1 | 8696 | `{` |
|    75 | 8697 | `	SyZero(pOut, sizeof(*pOut));` |
|    75 | 8698 | `	pOut->pClass = pClass;` |
|    75 | 8699 | `	pOut->pMeth = pMeth;` |
|    75 | 8700 | `	pOut->pFunc = &pMeth->sFunc;` |
|    75 | 8701 | `	if( (pOut->pFunc->iFlags & VM_FUNC_NATIVE) && pOut->pFunc->pNative ){` |
|    37 | 8702 | `		pOut->zSig = pOut->pFunc->pNative->zSig;` |
|    37 | 8703 | `		if( SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){` |
|    37 | 8704 | `			pOut->zRet = pOut->pFunc->pNative->zRet;` |
|    18 | 8705 | `		}` |
|    18 | 8706 | `	}` |
|    75 | 8707 | `}` |
|     - | 8708 | `/*` |
|     - | 8709 | ` * The Method / Function / Closure block.` |
|     - | 8710 | ` *` |
|     - | 8711 | ` * pOwner is the class being EXPORTED when there is one: php's tags are` |
|     - | 8712 | ` * relative to it — a method it did not declare "inherits" from its declaring` |
|     - | 8713 | ` * class — and the reflector alone cannot say that, because $class is the` |
|     - | 8714 | `` * DECLARING class. `overwrites` is the other direction and needs no owner: the`` |
|     - | 8715 | ` * declaring class's own parent declares the same method.` |
|     - | 8716 | ` */` |
|   128 | 8717 | `static sxi32 ReflectExportFuncBlock(ph7_context *pCtx, SyBlob *pOut, ReflectFuncRef *pRef,` |
|     - | 8718 | `	const char *zIndent, ph7_class *pOwner)` |
|     2 | 8719 | `{` |
|     - | 8720 | `	SyBlob sBody;` |
|   130 | 8721 | `	int bInternal = ReflectFuncIsInternal(pRef);` |
|   130 | 8722 | `	int nParam = ReflectParamCount(pRef);` |
|   130 | 8723 | `	const char *zRet = 0;` |
|   130 | 8724 | `	int nRet = 0, bHasRet, bTentRet = 0;` |
|   130 | 8725 | `	sxi32 rc = SXRET_OK;` |
|   130 | 8726 | `	SyBlobInit(&sBody, &pCtx->pVm->sAllocator);` |
|   130 | 8727 | `	bHasRet = ReflectFuncRetTextEx(pRef, &zRet, &nRet, &bTentRet);` |
|   130 | 8728 | `	if( pRef->pMeth ){` |
|   117 | 8729 | `		ph7_class *pDecl = ReflectFuncDeclClass(pRef);` |
|     - | 8730 | `		ph7_class *pProto;` |
|   117 | 8731 | `		SyString *pName = &pRef->pMeth->sFunc.sName;` |
|     - | 8732 | `		int bInherits, bCtor;` |
|   117 | 8733 | `		SyBlobAppend(&sBody, "Method [ <", sizeof("Method [ <")-1);` |
|   117 | 8734 | `		ReflectExportKind(&sBody, bInternal);` |
|   117 | 8735 | `		bInherits = (pOwner != 0 && pDecl != 0 && pDecl != pOwner);` |
|   117 | 8736 | `		if( bInherits ){` |
|    19 | 8737 | `			SyBlobFormat(&sBody, ", inherits %z", &pDecl->sName);` |
|   108 | 8738 | `		}else if( pDecl ){` |
|   148 | 8739 | `			ph7_class *pOver = ReflectPrototypeIn(pCtx, pDecl,` |
|    98 | 8740 | `				SyStringData(pName), (int)SyStringLength(pName), 0);` |
|    99 | 8741 | `			if( pOver ){` |
|     9 | 8742 | `				SyBlobFormat(&sBody, ", overwrites %z", &pOver->sName);` |
|     4 | 8743 | `			}` |
|    49 | 8744 | `		}` |
|   182 | 8745 | `		bCtor = SyStringLength(pName) == sizeof("__construct")-1` |
|   116 | 8746 | `			&& SyStrnicmp(SyStringData(pName), "__construct", sizeof("__construct")-1) == 0;` |
|   117 | 8747 | `		if( bCtor ){` |
|     9 | 8748 | `			SyBlobAppend(&sBody, ", ctor", sizeof(", ctor")-1);` |
|   113 | 8749 | `		}else if( SyStringLength(pName) == sizeof("__destruct")-1` |
|    55 | 8750 | `		 && SyStrnicmp(SyStringData(pName), "__destruct", sizeof("__destruct")-1) == 0 ){` |
|   ! 0 | 8751 | `			SyBlobAppend(&sBody, ", dtor", sizeof(", dtor")-1);` |
|   ! 0 | 8752 | `		}` |
|     - | 8753 | `		/* The prototype belongs to the DECLARING class's own method — the` |
|     - | 8754 | `		 * abstract or interface declaration IT satisfies. Asked from the` |
|     - | 8755 | `		 * exported class instead, a plainly INHERITED method would claim its` |
|     - | 8756 | `` 		 * parent as a prototype, which php does not (it prints `inherits` `` |
|     - | 8757 | `		 * alone there, and both tags when the declarer really has one). */` |
|     - | 8758 | `		/* A CONSTRUCTOR never has one: zend excludes it from prototype` |
|     - | 8759 | `		 * inheritance (there is nothing to satisfy — a parent's ctor is not a` |
|     - | 8760 | ``		 * contract), so php prints `overwrites A, ctor` and stops. */`` |
|   171 | 8761 | `		pProto = bCtor ? 0 : ReflectPrototypeIn(pCtx, pDecl ? pDecl : pRef->pClass,` |
|   108 | 8762 | `			SyStringData(pName), (int)SyStringLength(pName), 1);` |
|   117 | 8763 | `		if( pProto ){` |
|    35 | 8764 | `			SyBlobFormat(&sBody, ", prototype %z", &pProto->sName);` |
|    17 | 8765 | `		}` |
|   117 | 8766 | `		SyBlobAppend(&sBody, "> ", sizeof("> ")-1);` |
|   117 | 8767 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){` |
|    31 | 8768 | `			SyBlobAppend(&sBody, "abstract ", sizeof("abstract ")-1);` |
|    15 | 8769 | `		}` |
|   117 | 8770 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){` |
|     5 | 8771 | `			SyBlobAppend(&sBody, "final ", sizeof("final ")-1);` |
|     2 | 8772 | `		}` |
|   117 | 8773 | `		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){` |
|    21 | 8774 | `			SyBlobAppend(&sBody, "static ", sizeof("static ")-1);` |
|    10 | 8775 | `		}` |
|   117 | 8776 | `		SyBlobFormat(&sBody, "%s method %z ]", ReflectExportVis(pRef->pMeth->iProtection), pName);` |
|    59 | 8777 | `	}else{` |
|    20 | 8778 | `		SyBlobAppend(&sBody, pRef->pClosure ? "Closure [ <" : "Function [ <",` |
|    12 | 8779 | `			pRef->pClosure ? sizeof("Closure [ <")-1 : sizeof("Function [ <")-1);` |
|    14 | 8780 | `		ReflectExportKind(&sBody, bInternal);` |
|    14 | 8781 | `		SyBlobAppend(&sBody, "> function ", sizeof("> function ")-1);` |
|    14 | 8782 | `		if( pRef->pFunc ){` |
|     7 | 8783 | `			SyBlobFormat(&sBody, "%z", &pRef->pFunc->sName);` |
|    11 | 8784 | `		}else if( pRef->pHost ){` |
|     8 | 8785 | `			SyBlobFormat(&sBody, "%z", &pRef->pHost->sName);` |
|     3 | 8786 | `		}` |
|    14 | 8787 | `		SyBlobAppend(&sBody, " ]", sizeof(" ]")-1);` |
|     - | 8788 | `	}` |
|   130 | 8789 | `	SyBlobAppend(&sBody, " {\n", sizeof(" {\n")-1);` |
|   130 | 8790 | `	if( !bInternal && pRef->pFunc ){` |
|    51 | 8791 | `		SyBlobAppend(&sBody, "  @@ ", sizeof("  @@ ")-1);` |
|    51 | 8792 | `		if( SyStringLength(&pRef->pFunc->sFile) > 0 ){` |
|    76 | 8793 | `			SyBlobAppend(&sBody, SyStringData(&pRef->pFunc->sFile),` |
|    50 | 8794 | `				SyStringLength(&pRef->pFunc->sFile));` |
|    25 | 8795 | `		}` |
|    51 | 8796 | `		SyBlobFormat(&sBody, " %u - %u\n", pRef->pFunc->nLine, pRef->pFunc->nEndLine);` |
|    25 | 8797 | `	}` |
|     - | 8798 | `	/* php prints the parameter block for every INTERNAL function, and for a` |
|     - | 8799 | `	 * user one only when there is something to say. */` |
|   130 | 8800 | `	if( nParam > 0 \|\| bHasRet \|\| bInternal ){` |
|     - | 8801 | `		int n;` |
|   124 | 8802 | `		SyBlobFormat(&sBody, "\n  - Parameters [%d] {\n", nParam);` |
|   240 | 8803 | `		for( n = 0 ; n < nParam ; n++ ){` |
|     - | 8804 | `			ReflectParamDesc sDesc;` |
|   118 | 8805 | `			if( !ReflectParamAt(pRef, n, &sDesc) ){` |
|   ! 0 | 8806 | `				continue;` |
|     - | 8807 | `			}` |
|   118 | 8808 | `			SyBlobAppend(&sBody, "    ", sizeof("    ")-1);` |
|   118 | 8809 | `			ReflectExportParamLine(pCtx, &sBody, &sDesc);` |
|   118 | 8810 | `			SyBlobAppend(&sBody, "\n", sizeof(char));` |
|    60 | 8811 | `		}` |
|   124 | 8812 | `		SyBlobAppend(&sBody, "  }\n", sizeof("  }\n")-1);` |
|    61 | 8813 | `	}` |
|   130 | 8814 | `	if( bHasRet ){` |
|     - | 8815 | ``		/* php's two spellings: `Return [ T ]` for a declared type, `Tentative return`` |
|     - | 8816 | ``		 * [ T ]` for a stub's @tentative-return-type. */`` |
|   106 | 8817 | `		if( bTentRet ){` |
|    49 | 8818 | `			SyBlobAppend(&sBody, "  - Tentative return [ ", sizeof("  - Tentative return [ ")-1);` |
|    25 | 8819 | `		}else{` |
|    58 | 8820 | `			SyBlobAppend(&sBody, "  - Return [ ", sizeof("  - Return [ ")-1);` |
|     - | 8821 | `		}` |
|   106 | 8822 | `		SyBlobAppend(&sBody, zRet, (sxu32)nRet);` |
|   106 | 8823 | `		SyBlobAppend(&sBody, " ]\n", sizeof(" ]\n")-1);` |
|    52 | 8824 | `	}` |
|   130 | 8825 | `	SyBlobAppend(&sBody, "}\n", sizeof("}\n")-1);` |
|     - | 8826 | `	/* Indent every non-empty line, the way the chunk's explode/implode did. */` |
|   130 | 8827 | `	if( zIndent == 0 \|\| zIndent[0] == '\0' ){` |
|    56 | 8828 | `		SyBlobAppend(pOut, SyBlobData(&sBody), SyBlobLength(&sBody));` |
|    29 | 8829 | `	}else{` |
|    75 | 8830 | `		const char *z = (const char *)SyBlobData(&sBody);` |
|    75 | 8831 | `		sxu32 n = SyBlobLength(&sBody), i = 0, iStart = 0;` |
|    75 | 8832 | `		sxu32 nIndent = (sxu32)SyStrlen(zIndent);` |
| 14120 | 8833 | `		for( i = 0 ; i < n ; i++ ){` |
| 14046 | 8834 | `			if( z[i] != '\n' ){` |
| 13552 | 8835 | `				continue;` |
|     - | 8836 | `			}` |
|   495 | 8837 | `			if( i > iStart ){` |
|   427 | 8838 | `				SyBlobAppend(pOut, zIndent, nIndent);` |
|   427 | 8839 | `				SyBlobAppend(pOut, &z[iStart], i - iStart);` |
|   213 | 8840 | `			}` |
|   495 | 8841 | `			SyBlobAppend(pOut, "\n", sizeof(char));` |
|   495 | 8842 | `			iStart = i + 1;` |
|   248 | 8843 | `		}` |
|     - | 8844 | `	}` |
|   130 | 8845 | `	SyBlobRelease(&sBody);` |
|   130 | 8846 | `	return rc;` |
|     2 | 8847 | `}` |
|     - | 8848 | ``/* The `- Enum cases [N]` block php prints where an enum's cases would otherwise`` |
|     - | 8849 | ` * be listed among its constants. A backed case shows its value UNQUOTED. */` |
|     4 | 8850 | `static sxi32 ReflectExportEnumCases(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass)` |
|     1 | 8851 | `{` |
|     5 | 8852 | `	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     - | 8853 | `	sxu32 n;` |
|     5 | 8854 | `	SyBlobFormat(pOut, "\n  - Enum cases [%u] {\n", SySetUsed(&pClass->aEnumCases));` |
|    11 | 8855 | `	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){` |
|     7 | 8856 | `		SyBlobAppend(pOut, "    Case ", sizeof("    Case ")-1);` |
|     7 | 8857 | `		SyBlobAppend(pOut, SyStringData(&apCase[n]->sName), SyStringLength(&apCase[n]->sName));` |
|     7 | 8858 | `		if( pClass->nEnumBacking ){` |
|     5 | 8859 | `			ph7_value *pVal = 0;` |
|     - | 8860 | `			ph7_class_instance *pObj;` |
|     5 | 8861 | `			sxi32 rc = ReflectConstSlot(pCtx, pClass, apCase[n], &pVal);` |
|     5 | 8862 | `			if( rc != SXRET_OK ){` |
|   ! 0 | 8863 | `				return rc;` |
|     - | 8864 | `			}` |
|     5 | 8865 | `			pObj = (pVal && (pVal->iFlags & MEMOBJ_OBJ)) ? (ph7_class_instance *)pVal->x.pOther : 0;` |
|     5 | 8866 | `			if( pObj ){` |
|     5 | 8867 | `				ph7_value *pBacking = PH7_NativeAttr(pObj, "value");` |
|     5 | 8868 | `				if( pBacking ){` |
|     - | 8869 | `					ph7_value sTmp;` |
|     - | 8870 | `					const char *zText;` |
|     - | 8871 | `					int nText;` |
|     5 | 8872 | `					PH7_MemObjInit(pCtx->pVm, &sTmp);` |
|     5 | 8873 | `					PH7_MemObjStore(pBacking, &sTmp);` |
|     5 | 8874 | `					zText = ph7_value_to_string(&sTmp, &nText);` |
|     5 | 8875 | `					SyBlobAppend(pOut, " = ", sizeof(" = ")-1);` |
|     5 | 8876 | `					if( nText > 0 ){` |
|     5 | 8877 | `						SyBlobAppend(pOut, zText, (sxu32)nText);` |
|     2 | 8878 | `					}` |
|     5 | 8879 | `					PH7_MemObjRelease(&sTmp);` |
|     2 | 8880 | `				}` |
|     2 | 8881 | `			}` |
|     2 | 8882 | `		}` |
|     7 | 8883 | `		SyBlobAppend(pOut, "\n", sizeof(char));` |
|     4 | 8884 | `	}` |
|     5 | 8885 | `	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|     5 | 8886 | `	return SXRET_OK;` |
|     3 | 8887 | `}` |
|     - | 8888 | `/* The Class / Interface / Enum block. */` |
|    28 | 8889 | `static sxi32 ReflectExportClassBlock(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass)` |
|     1 | 8890 | `{` |
|    29 | 8891 | `	ph7_vm *pVm = pCtx->pVm;` |
|    29 | 8892 | `	int bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;` |
|    29 | 8893 | `	int bEnum = (pClass->iFlags & PH7_CLASS_ENUM) != 0;` |
|    29 | 8894 | `	int bIface = (pClass->iFlags & PH7_CLASS_INTERFACE) != 0;` |
|     - | 8895 | `	SySet aMembers, aIface;` |
|    29 | 8896 | `	sxu32 n, nConst = 0, nStaticProp = 0, nProp = 0, nStaticMeth = 0, nMeth = 0;` |
|    29 | 8897 | `	sxi32 rc = SXRET_OK;` |
|     - | 8898 | `	int iPass;` |
|    29 | 8899 | `	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));` |
|    29 | 8900 | `	SySetInit(&aIface, &pVm->sAllocator, sizeof(ph7_class *));` |
|    29 | 8901 | `	ReflectMembers(pVm, pClass, &aMembers, 0);` |
|    29 | 8902 | `	ReflectInterfacesOf(pClass, &aIface);` |
|     - | 8903 | `	/* ---- head ---- */` |
|    29 | 8904 | `	if( bIface ){` |
|     9 | 8905 | `		SyBlobAppend(pOut, "Interface [ <", sizeof("Interface [ <")-1);` |
|    25 | 8906 | `	}else if( bEnum ){` |
|     5 | 8907 | `		SyBlobAppend(pOut, "Enum [ <", sizeof("Enum [ <")-1);` |
|     3 | 8908 | `	}else{` |
|    17 | 8909 | `		SyBlobAppend(pOut, "Class [ <", sizeof("Class [ <")-1);` |
|     - | 8910 | `	}` |
|    29 | 8911 | `	ReflectExportKind(pOut, bInternal);` |
|    29 | 8912 | `	SyBlobAppend(pOut, "> ", sizeof("> ")-1);` |
|     - | 8913 | `	/* php tags a class that has an iteration handler, which its Traversable` |
|     - | 8914 | `	 * implementers have — and so does anything with a HOOKED property, because` |
|     - | 8915 | `	 * that is how php 8.4 walks one. An INTERFACE never carries one: php` |
|     - | 8916 | `	 * installs the handler when a CLASS implements Traversable, so` |
|     - | 8917 | ``	 * `interface I extends Iterator` prints no tag. */`` |
|     - | 8918 | `	{` |
|    35 | 8919 | `		int bIterable = !bIface && pVm->pTraversableClass != 0` |
|    38 | 8920 | `			&& PH7_VmInstanceOf(pClass, pVm->pTraversableClass);` |
|   171 | 8921 | `		for( n = 0 ; !bIterable && n < SySetUsed(&aMembers) ; n++ ){` |
|   143 | 8922 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   142 | 8923 | `			if( pM->iKind == REFLECT_MEMBER_PROP && pM->pAttr` |
|    55 | 8924 | `			 && (pM->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET\|PH7_CLASS_ATTR_HOOK_SET)) ){` |
|     3 | 8925 | `				bIterable = 1;` |
|     1 | 8926 | `			}` |
|    72 | 8927 | `		}` |
|    29 | 8928 | `		if( bIterable ){` |
|     5 | 8929 | `			SyBlobAppend(pOut, "<iterateable> ", sizeof("<iterateable> ")-1);` |
|     2 | 8930 | `		}` |
|     - | 8931 | `	}` |
|    29 | 8932 | `	if( bIface ){` |
|     9 | 8933 | `		SyBlobFormat(pOut, "interface %z", &pClass->sName);` |
|    25 | 8934 | `	}else if( bEnum ){` |
|     5 | 8935 | `		SyBlobFormat(pOut, "enum %z", &pClass->sName);` |
|     5 | 8936 | `		if( pClass->nEnumBacking ){` |
|     3 | 8937 | `			SyBlobFormat(pOut, ": %s", (pClass->nEnumBacking & MEMOBJ_INT) ? "int" : "string");` |
|     1 | 8938 | `		}` |
|     3 | 8939 | `	}else{` |
|    17 | 8940 | `		if( pClass->iFlags & PH7_CLASS_ABSTRACT ){` |
|     3 | 8941 | `			SyBlobAppend(pOut, "abstract ", sizeof("abstract ")-1);` |
|     1 | 8942 | `		}` |
|    17 | 8943 | `		if( pClass->iFlags & PH7_CLASS_FINAL ){` |
|     3 | 8944 | `			SyBlobAppend(pOut, "final ", sizeof("final ")-1);` |
|     1 | 8945 | `		}` |
|    17 | 8946 | `		if( pClass->iFlags & PH7_CLASS_READONLY ){` |
|     3 | 8947 | `			SyBlobAppend(pOut, "readonly ", sizeof("readonly ")-1);` |
|     1 | 8948 | `		}` |
|    17 | 8949 | `		SyBlobFormat(pOut, "class %z", &pClass->sName);` |
|    17 | 8950 | `		if( pClass->pBase ){` |
|     5 | 8951 | `			SyBlobFormat(pOut, " extends %z", &pClass->pBase->sName);` |
|     2 | 8952 | `		}` |
|     - | 8953 | `	}` |
|    29 | 8954 | `	if( SySetUsed(&aIface) > 0 ){` |
|    17 | 8955 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIface);` |
|     - | 8956 | `		/* An interface EXTENDS what a class implements. */` |
|    25 | 8957 | `		SyBlobAppend(pOut, bIface ? " extends " : " implements ",` |
|     8 | 8958 | `			bIface ? sizeof(" extends ")-1 : sizeof(" implements ")-1);` |
|    39 | 8959 | `		for( n = 0 ; n < SySetUsed(&aIface) ; n++ ){` |
|    23 | 8960 | `			if( n > 0 ){` |
|     7 | 8961 | `				SyBlobAppend(pOut, ", ", sizeof(", ")-1);` |
|     3 | 8962 | `			}` |
|    23 | 8963 | `			SyBlobFormat(pOut, "%z", &apIface[n]->sName);` |
|    12 | 8964 | `		}` |
|     8 | 8965 | `	}` |
|    29 | 8966 | `	SyBlobAppend(pOut, " ] {\n", sizeof(" ] {\n")-1);` |
|    29 | 8967 | `	if( !bInternal && SyStringLength(&pClass->sFile) > 0 ){` |
|    21 | 8968 | `		SyBlobAppend(pOut, "  @@ ", sizeof("  @@ ")-1);` |
|    21 | 8969 | `		SyBlobAppend(pOut, SyStringData(&pClass->sFile), SyStringLength(&pClass->sFile));` |
|    21 | 8970 | `		SyBlobFormat(pOut, " %u-%u\n", pClass->nLine, pClass->nEndLine);` |
|    10 | 8971 | `	}` |
|     - | 8972 | `	/* ---- counts ---- */` |
|   181 | 8973 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   153 | 8974 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   153 | 8975 | `		if( pM->iKind == REFLECT_MEMBER_CONST ){` |
|    25 | 8976 | `			if( !(bEnum && (pM->pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)) ){` |
|    19 | 8977 | `				nConst++;` |
|    10 | 8978 | `			}` |
|   141 | 8979 | `		}else if( pM->iKind == REFLECT_MEMBER_PROP ){` |
|    55 | 8980 | `			if( pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){ nStaticProp++; }else{ nProp++; }` |
|    28 | 8981 | `		}else{` |
|    75 | 8982 | `			if( pM->pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){ nStaticMeth++; }else{ nMeth++; }` |
|     - | 8983 | `		}` |
|    77 | 8984 | `	}` |
|     - | 8985 | `	/* ---- enum cases, then constants ---- */` |
|    29 | 8986 | `	if( bEnum ){` |
|     5 | 8987 | `		rc = ReflectExportEnumCases(pCtx, pOut, pClass);` |
|     5 | 8988 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 8989 | `			goto done;` |
|     - | 8990 | `		}` |
|     2 | 8991 | `	}` |
|    29 | 8992 | `	SyBlobFormat(pOut, "\n  - Constants [%u] {\n", nConst);` |
|   181 | 8993 | `	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   153 | 8994 | `		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|   152 | 8995 | `		if( pM->iKind != REFLECT_MEMBER_CONST` |
|    89 | 8996 | `		 \|\| (bEnum && (pM->pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)) ){` |
|   135 | 8997 | `			continue;` |
|     - | 8998 | `		}` |
|    19 | 8999 | `		SyBlobAppend(pOut, "    ", sizeof("    ")-1);` |
|    19 | 9000 | `		rc = ReflectExportConstLine(pCtx, pOut, pClass, pM->pAttr, &pM->sKey);` |
|    19 | 9001 | `		if( rc != SXRET_OK ){` |
|   ! 0 | 9002 | `			goto done;` |
|     - | 9003 | `		}` |
|    10 | 9004 | `	}` |
|    29 | 9005 | `	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|     - | 9006 | `	/* ---- properties and methods, statics first ---- */` |
|   141 | 9007 | `	for( iPass = 0 ; iPass < 4 ; iPass++ ){` |
|   113 | 9008 | `		int bStatic = (iPass == 0 \|\| iPass == 1);` |
|   113 | 9009 | `		int bMethods = (iPass == 1 \|\| iPass == 3);` |
|   113 | 9010 | `		int bFirst = 1;` |
|     - | 9011 | `		static const char *azTitle[] = {` |
|     - | 9012 | `			"\n  - Static properties [%u] {\n", "\n  - Static methods [%u] {\n",` |
|     - | 9013 | `			"\n  - Properties [%u] {\n", "\n  - Methods [%u] {\n"` |
|     - | 9014 | `		};` |
|     - | 9015 | `		sxu32 aCount[4];` |
|   113 | 9016 | `		aCount[0] = nStaticProp;` |
|   113 | 9017 | `		aCount[1] = nStaticMeth;` |
|   113 | 9018 | `		aCount[2] = nProp;` |
|   113 | 9019 | `		aCount[3] = nMeth;` |
|   113 | 9020 | `		SyBlobFormat(pOut, azTitle[iPass], aCount[iPass]);` |
|   721 | 9021 | `		for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){` |
|   609 | 9022 | `			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);` |
|     - | 9023 | `			int bIsStatic;` |
|   609 | 9024 | `			if( pM->iKind == REFLECT_MEMBER_CONST ){` |
|    97 | 9025 | `				continue;` |
|     - | 9026 | `			}` |
|   513 | 9027 | `			if( bMethods != (pM->iKind == REFLECT_MEMBER_METHOD) ){` |
|   257 | 9028 | `				continue;` |
|     - | 9029 | `			}` |
|   331 | 9030 | `			bIsStatic = bMethods ? (pM->pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0` |
|   182 | 9031 | `				: (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0;` |
|   257 | 9032 | `			if( bIsStatic != bStatic ){` |
|   129 | 9033 | `				continue;` |
|     - | 9034 | `			}` |
|   129 | 9035 | `			if( bMethods ){` |
|     - | 9036 | `				ReflectFuncRef sRef;` |
|    75 | 9037 | `				if( !bFirst ){` |
|    47 | 9038 | `					SyBlobAppend(pOut, "\n", sizeof(char));` |
|    23 | 9039 | `				}` |
|    75 | 9040 | `				bFirst = 0;` |
|    75 | 9041 | `				ReflectFuncFromMethod(pClass, pM->pMeth, &sRef);` |
|    75 | 9042 | `				rc = ReflectExportFuncBlock(pCtx, pOut, &sRef, "    ", pClass);` |
|    75 | 9043 | `				if( rc != SXRET_OK ){` |
|   ! 0 | 9044 | `					goto done;` |
|     - | 9045 | `				}` |
|    38 | 9046 | `			}else{` |
|    55 | 9047 | `				SyBlobAppend(pOut, "    ", sizeof("    ")-1);` |
|    55 | 9048 | `				ReflectExportPropLine(pCtx, pOut, pM->pAttr, &pM->sKey);` |
|     - | 9049 | `			}` |
|    65 | 9050 | `		}` |
|   113 | 9051 | `		SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);` |
|    57 | 9052 | `	}` |
|    29 | 9053 | `	SyBlobAppend(pOut, "}\n", sizeof("}\n")-1);` |
|    14 | 9054 | `done:` |
|    29 | 9055 | `	SySetRelease(&aMembers);` |
|    29 | 9056 | `	SySetRelease(&aIface);` |
|    29 | 9057 | `	return rc;` |
|     1 | 9058 | `}` |
|     - | 9059 | `/* ---- the five __toString() entry points ---- */` |
|    28 | 9060 | `static int ReflectExportClassSelf(ph7_context *pCtx)` |
|     1 | 9061 | `{` |
|    29 | 9062 | `	ph7_class *pClass = ReflectClassOf(pCtx);` |
|     - | 9063 | `	SyBlob sOut;` |
|     - | 9064 | `	sxi32 rc;` |
|    29 | 9065 | `	if( pClass == 0 ){` |
|   ! 0 | 9066 | `		ph7_result_string(pCtx, "", 0);` |
|   ! 0 | 9067 | `		return PH7_OK;` |
|     - | 9068 | `	}` |
|    29 | 9069 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    29 | 9070 | `	rc = ReflectExportClassBlock(pCtx, &sOut, pClass);` |
|    29 | 9071 | `	if( rc == SXRET_OK ){` |
|    29 | 9072 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    14 | 9073 | `	}` |
|    29 | 9074 | `	SyBlobRelease(&sOut);` |
|    29 | 9075 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    15 | 9076 | `}` |
|    54 | 9077 | `static int ReflectExportFuncSelf(ph7_context *pCtx)` |
|     2 | 9078 | `{` |
|     - | 9079 | `	ReflectFuncRef sRef;` |
|     - | 9080 | `	SyBlob sOut;` |
|     - | 9081 | `	sxi32 rc;` |
|    56 | 9082 | `	if( !ReflectFuncOfThis(pCtx, &sRef) ){` |
|   ! 0 | 9083 | `		ph7_result_string(pCtx, "", 0);` |
|   ! 0 | 9084 | `		return PH7_OK;` |
|     - | 9085 | `	}` |
|    56 | 9086 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    56 | 9087 | `	rc = ReflectExportFuncBlock(pCtx, &sOut, &sRef, "", 0);` |
|    56 | 9088 | `	if( rc == SXRET_OK ){` |
|    56 | 9089 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    27 | 9090 | `	}` |
|    56 | 9091 | `	SyBlobRelease(&sOut);` |
|    56 | 9092 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|    29 | 9093 | `}` |
|    10 | 9094 | `static int ReflectExportParamSelf(ph7_context *pCtx)` |
|     1 | 9095 | `{` |
|     - | 9096 | `	ReflectFuncRef sRef;` |
|     - | 9097 | `	ReflectParamDesc sDesc;` |
|     - | 9098 | `	SyBlob sOut;` |
|    11 | 9099 | `	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) ){` |
|   ! 0 | 9100 | `		ph7_result_string(pCtx, "", 0);` |
|   ! 0 | 9101 | `		return PH7_OK;` |
|     - | 9102 | `	}` |
|    11 | 9103 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|    11 | 9104 | `	ReflectExportParamLine(pCtx, &sOut, &sDesc);` |
|    11 | 9105 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|    11 | 9106 | `	SyBlobRelease(&sOut);` |
|    11 | 9107 | `	return PH7_OK;` |
|     6 | 9108 | `}` |
|     4 | 9109 | `static int ReflectExportPropSelf(ph7_context *pCtx)` |
|     1 | 9110 | `{` |
|     - | 9111 | `	ReflectMemberRef sRef;` |
|     - | 9112 | `	SyBlob sOut;` |
|     - | 9113 | `	SyString sKey;` |
|     5 | 9114 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) \|\| sRef.pAttr == 0 ){` |
|   ! 0 | 9115 | `		ph7_result_string(pCtx, "", 0);` |
|   ! 0 | 9116 | `		return PH7_OK;` |
|     - | 9117 | `	}` |
|     5 | 9118 | `	SyStringInitFromBuf(&sKey, sRef.zName, sRef.nName);` |
|     5 | 9119 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     5 | 9120 | `	ReflectExportPropLine(pCtx, &sOut, sRef.pAttr, &sKey);` |
|     5 | 9121 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     5 | 9122 | `	SyBlobRelease(&sOut);` |
|     5 | 9123 | `	return PH7_OK;` |
|     3 | 9124 | `}` |
|     6 | 9125 | `static int ReflectExportConstSelf(ph7_context *pCtx)` |
|     1 | 9126 | `{` |
|     - | 9127 | `	ReflectMemberRef sRef;` |
|     - | 9128 | `	SyBlob sOut;` |
|     - | 9129 | `	SyString sKey;` |
|     - | 9130 | `	sxi32 rc;` |
|     7 | 9131 | `	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) \|\| sRef.pAttr == 0 ){` |
|   ! 0 | 9132 | `		ph7_result_string(pCtx, "", 0);` |
|   ! 0 | 9133 | `		return PH7_OK;` |
|     - | 9134 | `	}` |
|     7 | 9135 | `	SyStringInitFromBuf(&sKey, sRef.zName, sRef.nName);` |
|     7 | 9136 | `	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);` |
|     7 | 9137 | `	rc = ReflectExportConstLine(pCtx, &sOut, sRef.pClass, sRef.pAttr, &sKey);` |
|     7 | 9138 | `	if( rc == SXRET_OK ){` |
|     7 | 9139 | `		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));` |
|     3 | 9140 | `	}` |
|     7 | 9141 | `	SyBlobRelease(&sOut);` |
|     7 | 9142 | `	return rc == SXRET_OK ? PH7_OK : rc;` |
|     4 | 9143 | `}` |
|     - | 9144 | `/*` |
|     - | 9145 | ` * Install the Reflection API — ~25 classes, all of them declared from C.` |
|     - | 9146 | ` *` |
|     - | 9147 | `` * There is no global thunk table left to register: every `__reflect_*` and`` |
|     - | 9148 | `` * `__phl_rcinfo` became a method of the class that always owned it, so`` |
|     - | 9149 | ` * Reflection adds no name to php's global function namespace at all. Called` |
|     - | 9150 | ` * from PH7_VmInit while pVm->bCompilingBuiltin is set, right after the core` |
|     - | 9151 | ` * builtin chunks (Exception and friends have to exist already).` |
|     - | 9152 | ` */` |
|  5146 | 9153 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|     5 | 9154 | `{` |
|     - | 9155 | `	sxi32 rc;` |
|     - | 9156 | `	/* Where chunk 1 was: Reflector, Reflection, ReflectionException,` |
|     - | 9157 | `	 * ReflectionClass and ReflectionObject are native now. Chunks 2 and 3 name` |
|     - | 9158 | ``	 * `Reflector` in their own `implements` clauses, so this has to run first. */`` |
|  5151 | 9159 | `	rc = PH7_VmInstallReflectionClass(&(*pVm));` |
|  5151 | 9160 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9161 | `		return rc;` |
|     - | 9162 | `	}` |
|     - | 9163 | `	/* Where chunk 2 was: ReflectionFunctionAbstract, ReflectionFunction,` |
|     - | 9164 | `	 * ReflectionMethod and ReflectionParameter are native now. */` |
|  5151 | 9165 | `	rc = PH7_VmInstallReflectionFunc(&(*pVm));` |
|  5151 | 9166 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9167 | `		return rc;` |
|     - | 9168 | `	}` |
|  5151 | 9169 | `	rc = PH7_VmInstallReflectionHookType(&(*pVm));` |
|  5151 | 9170 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9171 | `		return rc;` |
|     - | 9172 | `	}` |
|     - | 9173 | `	/* Where chunk 3 was: ReflectionProperty and ReflectionClassConstant are` |
|     - | 9174 | `	 * native now, and PropertyHookType is a native ENUM. */` |
|  5151 | 9175 | `	rc = PH7_VmInstallReflectionMember(&(*pVm));` |
|  5151 | 9176 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9177 | `		return rc;` |
|     - | 9178 | `	}` |
|     - | 9179 | `	/* Where chunk 4 was: the four type classes are native now (see` |
|     - | 9180 | `	 * PH7_VmInstallReflectionTypes). Stringable exists by this point. */` |
|  5151 | 9181 | `	rc = PH7_VmInstallReflectionTypes(&(*pVm));` |
|  5151 | 9182 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9183 | `		return rc;` |
|     - | 9184 | `	}` |
|     - | 9185 | `	/* Where chunk 5 was: ReflectionGenerator/Fiber and the four standalone` |
|     - | 9186 | `	 * classes chunk 6 used to hold are native now. Reflector (chunk 1) exists. */` |
|  5151 | 9187 | `	rc = PH7_VmInstallReflectionSmall(&(*pVm));` |
|  5151 | 9188 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9189 | `		return rc;` |
|     - | 9190 | `	}` |
|     - | 9191 | `	/* Where chunk 6 was: the three ReflectionEnum classes are native now, and` |
|     - | 9192 | `	 * the class DESCRIPTOR they read (__phl_rcinfo) has no callers left. */` |
|  5151 | 9193 | `	rc = PH7_VmInstallReflectionEnum(&(*pVm));` |
|  5151 | 9194 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9195 | `		return rc;` |
|     - | 9196 | `	}` |
|     - | 9197 | `	/* Where chunk 7 was: ReflectionAttribute is native now, and with it the` |
|     - | 9198 | `	 * builder (__reflect_build_attrs) and the two helpers it needed. */` |
|  5151 | 9199 | `	rc = PH7_VmInstallReflectionAttribute(&(*pVm));` |
|  5151 | 9200 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 9201 | `		return rc;` |
|     - | 9202 | `	}` |
|     - | 9203 | `	/* Where chunk 9 was: the export format is ReflectExportClassSelf() and its` |
|     - | 9204 | `	 * four siblings above. Nothing of the Reflection library is PHP any more,` |
|     - | 9205 | `	 * so vm_builtin_reflection_lib.c is gone and this IS the installer. */` |
|  5151 | 9206 | `	return SXRET_OK;` |
|  2578 | 9207 | `}` |
|     - | 9208 |  |
