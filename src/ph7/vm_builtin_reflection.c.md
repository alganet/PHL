# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1057/1223 lines (86.43%)

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
|   716 |   31 | `static ph7_class * ReflectResolveClass(ph7_vm *pVm, ph7_value *pArg)` |
|     4 |   32 | `{` |
|     - |   33 | `	ph7_class *pClass;` |
|   720 |   34 | `	pClass = PH7_VmExtractClassFromValue(pVm, pArg);` |
|   720 |   35 | `	if( pClass == 0 && ph7_value_is_string(pArg) ){` |
|     - |   36 | `		const char *zName;` |
|     - |   37 | `		int nLen;` |
|    16 |   38 | `		zName = ph7_value_to_string(pArg, &nLen);` |
|    16 |   39 | `		if( nLen > 0 ){` |
|    16 |   40 | `			pClass = PH7_VmTriggerAutoload(pVm, zName, (sxu32)nLen, FALSE);` |
|     7 |   41 | `		}` |
|     7 |   42 | `	}` |
|   720 |   43 | `	return pClass;` |
|     4 |   44 | `}` |
|     - |   45 | `/*` |
|     - |   46 | ` * Hand a freshly created class instance to the caller. The return slot` |
|     - |   47 | ` * takes over the initial reference from PH7_NewClassInstance (iRef=1):` |
|     - |   48 | ` * no extra iRef++ here (see the synthesized-object invariant — a stray` |
|     - |   49 | ` * bump leaks the object and disables its __destruct).` |
|     - |   50 | ` */` |
|   104 |   51 | `static int ReflectResultObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|     1 |   52 | `{` |
|   105 |   53 | `	if( pObj == 0 ){` |
|   ! 0 |   54 | `		ph7_result_null(pCtx);` |
|   ! 0 |   55 | `		return PH7_OK;` |
|     - |   56 | `	}` |
|   105 |   57 | `	PH7_MemObjRelease(pCtx->pRet);` |
|   105 |   58 | `	pCtx->pRet->x.pOther = pObj;` |
|   105 |   59 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|   105 |   60 | `	return PH7_OK;` |
|    53 |   61 | `}` |
|     - |   62 | `/* --- Marshaling helpers: build the descriptor arrays handed to the PHP layer --- */` |
| 18158 |   63 | `static void ReflectMapAddBool(ph7_context *pCtx, ph7_value *pMap, const char *zKey, int b)` |
|     4 |   64 | `{` |
| 18162 |   65 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 18162 |   66 | `	if( p == 0 ){ return; }` |
| 18162 |   67 | `	ph7_value_bool(p, b);` |
| 18162 |   68 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  9083 |   69 | `}` |
|  5490 |   70 | `static void ReflectMapAddInt(ph7_context *pCtx, ph7_value *pMap, const char *zKey, sxi64 iVal)` |
|     4 |   71 | `{` |
|  5494 |   72 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  5494 |   73 | `	if( p == 0 ){ return; }` |
|  5494 |   74 | `	ph7_value_int64(p, iVal);` |
|  5494 |   75 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  2749 |   76 | `}` |
|  5340 |   77 | `static void ReflectMapAddStr(ph7_context *pCtx, ph7_value *pMap, const char *zKey,` |
|     - |   78 | `	const char *zVal, int nVal)` |
|     4 |   79 | `{` |
|  5344 |   80 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  5344 |   81 | `	if( p == 0 ){ return; }` |
|  5344 |   82 | `	ph7_value_string(p, zVal, nVal);` |
|  5344 |   83 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  2674 |   84 | `}` |
|  1686 |   85 | `static void ReflectMapAddNull(ph7_context *pCtx, ph7_value *pMap, const char *zKey)` |
|     4 |   86 | `{` |
|  1690 |   87 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  1690 |   88 | `	if( p == 0 ){ return; }` |
|  1690 |   89 | `	ph7_value_null(p);` |
|  1690 |   90 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|   847 |   91 | `}` |
|     - |   92 | `/* Add an entry under a dynamic (SyString) key. */` |
|   480 |   93 | `static void ReflectMapAddDyn(ph7_context *pCtx, ph7_value *pMap,` |
|     - |   94 | `	const SyString *pKey, ph7_value *pVal)` |
|     4 |   95 | `{` |
|   484 |   96 | `	ph7_value *pK = ph7_context_new_scalar(pCtx);` |
|   484 |   97 | `	if( pK == 0 ){ return; }` |
|   484 |   98 | `	ph7_value_string(pK, pKey->zString, (int)pKey->nByte);` |
|   484 |   99 | `	ph7_array_add_elem(pMap, pK, pVal);` |
|   244 |  100 | `}` |
|     - |  101 | `/* Emit the declared #[...] attributes of a target as a summary list:` |
|     - |  102 | ` * [ {name, line} ... ]. Argument values stay lazy — the PHP layer pulls` |
|     - |  103 | ` * them through __reflect_attr_args when ReflectionAttribute needs them. */` |
|  2324 |  104 | `static void ReflectMapAddAttrs(ph7_context *pCtx, ph7_value *pMap, SySet *pAttrs)` |
|     4 |  105 | `{` |
|  2328 |  106 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|  2328 |  107 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|     - |  108 | `	sxu32 n;` |
|  2328 |  109 | `	if( pList == 0 ){` |
|   ! 0 |  110 | `		return;` |
|     - |  111 | `	}` |
|  2438 |  112 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|   111 |  113 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|   111 |  114 | `		if( pMeta == 0 ){ break; }` |
|   111 |  115 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aA[n].sName), (int)SyStringLength(&aA[n].sName));` |
|   111 |  116 | `		ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)aA[n].nLine);` |
|   111 |  117 | `		ph7_array_add_elem(pList, 0, pMeta);` |
|    56 |  118 | `	}` |
|  2328 |  119 | `	ph7_array_add_strkey_elem(pMap, "attrs", pList);` |
|  1166 |  120 | `}` |
|     - |  121 | `/* Emit a doc-comment field: the text when present, else boolean false` |
|     - |  122 | ` * (getDocComment()'s exact return contract). */` |
|   984 |  123 | `static void ReflectMapAddDoc(ph7_context *pCtx, ph7_value *pMap, const SyString *pDoc)` |
|     4 |  124 | `{` |
|   988 |  125 | `	if( SyStringLength(pDoc) > 0 ){` |
|    21 |  126 | `		ReflectMapAddStr(pCtx, pMap, "doc", SyStringData(pDoc), (int)SyStringLength(pDoc));` |
|    11 |  127 | `	}else{` |
|   968 |  128 | `		ReflectMapAddBool(pCtx, pMap, "doc", 0);` |
|     - |  129 | `	}` |
|   988 |  130 | `}` |
|     - |  131 | `/*` |
|     - |  132 | ` * Append pIface (and its parents / extended interfaces) to the dedup set` |
|     - |  133 | ` * of ph7_class pointers.` |
|     - |  134 | ` */` |
|    48 |  135 | `static void ReflectAddInterface(ph7_class *pIface, SySet *pOut, int iDepth)` |
|     2 |  136 | `{` |
|     - |  137 | `	ph7_class **apKnown;` |
|     - |  138 | `	sxu32 n;` |
|    50 |  139 | `	if( pIface == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|   ! 0 |  140 | `		return;` |
|     - |  141 | `	}` |
|     - |  142 | `	/* Parents of an interface come along too (interface B extends A) */` |
|    50 |  143 | `	if( pIface->pBase ){` |
|     9 |  144 | `		ReflectAddInterface(pIface->pBase, pOut, iDepth + 1);` |
|     4 |  145 | `	}` |
|     - |  146 | `	/* Some engines record extended interfaces in aInterface as well */` |
|    50 |  147 | `	apKnown = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|    50 |  148 | `	for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|   ! 0 |  149 | `		ReflectAddInterface(apKnown[n], pOut, iDepth + 1);` |
|   ! 0 |  150 | `	}` |
|     - |  151 | `	/* Dedup by pointer */` |
|    50 |  152 | `	apKnown = (ph7_class **)SySetBasePtr(pOut);` |
|    70 |  153 | `	for( n = 0 ; n < SySetUsed(pOut) ; n++ ){` |
|    29 |  154 | `		if( apKnown[n] == pIface ){` |
|     9 |  155 | `			return;` |
|     - |  156 | `		}` |
|    11 |  157 | `	}` |
|    42 |  158 | `	SySetPut(pOut, (const void *)&pIface);` |
|    26 |  159 | `}` |
|     - |  160 | `/*` |
|     - |  161 | ` * Collect the transitive set of interfaces implemented by pClass:` |
|     - |  162 | ` * the parent chain's interfaces first, then the class's own.` |
|     - |  163 | ` */` |
|   180 |  164 | `static void ReflectCollectInterfaces(ph7_class *pClass, SySet *pOut, int iDepth)` |
|     4 |  165 | `{` |
|     - |  166 | `	ph7_class **apIface;` |
|     - |  167 | `	sxu32 n;` |
|   184 |  168 | `	if( pClass == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|   ! 0 |  169 | `		return;` |
|     - |  170 | `	}` |
|   184 |  171 | `	if( pClass->pBase ){` |
|    32 |  172 | `		ReflectCollectInterfaces(pClass->pBase, pOut, iDepth + 1);` |
|    15 |  173 | `	}` |
|   184 |  174 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   222 |  175 | `	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){` |
|    40 |  176 | `		ReflectAddInterface(apIface[n], pOut, iDepth + 1);` |
|    21 |  177 | `	}` |
|    94 |  178 | `}` |
|     - |  179 | `/*` |
|     - |  180 | ` * Deepest base class whose method table maps the same name to the very` |
|     - |  181 | ` * same ph7_class_method pointer: inheritance shares member pointers` |
|     - |  182 | ` * (PH7_ClassInherit), so this identifies the declaring class. Methods` |
|     - |  183 | ` * copied in from traits are not on the pBase chain and thus report the` |
|     - |  184 | ` * using class, which is what PHP reports too.` |
|     - |  185 | ` */` |
|   824 |  186 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth)` |
|     4 |  187 | `{` |
|   828 |  188 | `	ph7_class *pDecl = pClass;` |
|   828 |  189 | `	ph7_class *pBase = pClass->pBase;` |
|   828 |  190 | `	int iDepth = 0;` |
|  1150 |  191 | `	while( pBase && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|     - |  192 | `		SyHashEntry *pEntry;` |
|   880 |  193 | `		pEntry = SyHashGet(&pBase->hMethod, (const void *)SyStringData(&pMeth->sFunc.sName),` |
|   293 |  194 | `			SyStringLength(&pMeth->sFunc.sName));` |
|   587 |  195 | `		if( pEntry == 0 \|\| (ph7_class_method *)pEntry->pUserData != pMeth ){` |
|   133 |  196 | `			break;` |
|     - |  197 | `		}` |
|   323 |  198 | `		pDecl = pBase;` |
|   323 |  199 | `		pBase = pBase->pBase;` |
|   323 |  200 | `		iDepth++;` |
|     1 |  201 | `	}` |
|   828 |  202 | `	return pDecl;` |
|     4 |  203 | `}` |
|     - |  204 | `/* Fetch a class attribute (property or constant) by plain name. */` |
|    44 |  205 | `static ph7_class_attr * ReflectFetchAttr(ph7_class *pClass, ph7_value *pName)` |
|     1 |  206 | `{` |
|     - |  207 | `	SyHashEntry *pEntry;` |
|     - |  208 | `	const char *zName;` |
|     - |  209 | `	int nLen;` |
|    45 |  210 | `	zName = ph7_value_to_string(pName, &nLen);` |
|    45 |  211 | `	if( nLen < 1 ){` |
|   ! 0 |  212 | `		return 0;` |
|     - |  213 | `	}` |
|    45 |  214 | `	pEntry = SyHashGet(&pClass->hAttr, (const void *)zName, (sxu32)nLen);` |
|    45 |  215 | `	if( pEntry == 0 ){` |
|     3 |  216 | `		return 0;` |
|     - |  217 | `	}` |
|    43 |  218 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|    23 |  219 | `}` |
|     - |  220 | `/* Fetch a class CONSTANT (or enum case) by name from the hConst namespace. */` |
|    58 |  221 | `static ph7_class_attr * ReflectFetchConst(ph7_class *pClass, ph7_value *pName)` |
|     2 |  222 | `{` |
|     - |  223 | `	const char *zName;` |
|     - |  224 | `	int nLen;` |
|    60 |  225 | `	zName = ph7_value_to_string(pName, &nLen);` |
|    60 |  226 | `	if( nLen < 1 ){` |
|   ! 0 |  227 | `		return 0;` |
|     - |  228 | `	}` |
|    60 |  229 | `	return PH7_ClassExtractConstant(pClass, zName, (sxu32)nLen);` |
|    31 |  230 | `}` |
|     - |  231 | `/* Fetch a member by name from EITHER namespace (property then constant) — used` |
|     - |  232 | ` * where the caller reflects over a member that may be either (e.g. attributes` |
|     - |  233 | ` * attached to a property or a constant). */` |
|     4 |  234 | `static ph7_class_attr * ReflectFetchMember(ph7_class *pClass, ph7_value *pName)` |
|     1 |  235 | `{` |
|     5 |  236 | `	ph7_class_attr *pAttr = ReflectFetchAttr(pClass, pName);` |
|     5 |  237 | `	if( pAttr == 0 ){` |
|     3 |  238 | `		pAttr = ReflectFetchConst(pClass, pName);` |
|     1 |  239 | `	}` |
|     5 |  240 | `	return pAttr;` |
|     1 |  241 | `}` |
|     - |  242 | `/*` |
|     - |  243 | ` * array\|null __phl_rcinfo(object\|string $target)` |
|     - |  244 | ` *` |
|     - |  245 | ` * Full class descriptor, or null when the class cannot be resolved (after` |
|     - |  246 | ` * an autoload attempt). Shape:` |
|     - |  247 | ` *   name, internal, interface, trait, abstract, final, readonly, iterable (bool),` |
|     - |  248 | ` *   parent (string\|null), interfaces (list), traits (list),` |
|     - |  249 | ` *   file (string\|false), line, endline (int),` |
|     - |  250 | ` *   ctorvis, clonevis (0 = absent, else PH7_CLASS_PROT_*),` |
|     - |  251 | ` *   consts  {name: {vis, final, decl, line}},` |
|     - |  252 | ` *   props   {name: {vis, static, readonly, hasdef, decl, line}},` |
|     - |  253 | ` *   methods {name: {vis, static, abstract, final, decl, line}}` |
|     - |  254 | ` */` |
|   164 |  255 | `static int vm_builtin_reflect_class_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     4 |  256 | `{` |
|   168 |  257 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  258 | `	ph7_class *pClass;` |
|     - |  259 | `	ph7_value *pInfo, *pConsts, *pProps, *pMethods, *pList;` |
|     - |  260 | `	SyHashEntry *pEntry;` |
|     - |  261 | `	SySet aIfaceSet;` |
|   168 |  262 | `	sxi32 iCtorVis = 0, iCloneVis = 0;` |
|   168 |  263 | `	int bIterable = 0;` |
|     - |  264 | `	sxu32 n;` |
|   168 |  265 | `	if( nArg < 1 ){` |
|   ! 0 |  266 | `		ph7_result_null(pCtx);` |
|   ! 0 |  267 | `		return PH7_OK;` |
|     - |  268 | `	}` |
|   168 |  269 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|   168 |  270 | `	if( pClass == 0 ){` |
|    16 |  271 | `		ph7_result_null(pCtx);` |
|    16 |  272 | `		return PH7_OK;` |
|     - |  273 | `	}` |
|   154 |  274 | `	pInfo = ph7_context_new_array(pCtx);` |
|   154 |  275 | `	pConsts = ph7_context_new_array(pCtx);` |
|   154 |  276 | `	pProps = ph7_context_new_array(pCtx);` |
|   154 |  277 | `	pMethods = ph7_context_new_array(pCtx);` |
|   154 |  278 | `	if( pInfo == 0 \|\| pConsts == 0 \|\| pProps == 0 \|\| pMethods == 0 ){` |
|   ! 0 |  279 | `		ph7_result_null(pCtx);` |
|   ! 0 |  280 | `		return PH7_OK;` |
|     - |  281 | `	}` |
|   154 |  282 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   154 |  283 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pClass->iFlags & PH7_CLASS_INTERNAL) != 0);` |
|   154 |  284 | `	ReflectMapAddBool(pCtx, pInfo, "interface", (pClass->iFlags & PH7_CLASS_INTERFACE) != 0);` |
|   154 |  285 | `	ReflectMapAddBool(pCtx, pInfo, "trait", (pClass->iFlags & PH7_CLASS_TRAIT) != 0);` |
|   154 |  286 | `	ReflectMapAddBool(pCtx, pInfo, "abstract", (pClass->iFlags & PH7_CLASS_ABSTRACT) != 0);` |
|   154 |  287 | `	ReflectMapAddBool(pCtx, pInfo, "final", (pClass->iFlags & PH7_CLASS_FINAL) != 0);` |
|   154 |  288 | `	ReflectMapAddBool(pCtx, pInfo, "readonly", (pClass->iFlags & PH7_CLASS_READONLY) != 0);` |
|   154 |  289 | `	ReflectMapAddBool(pCtx, pInfo, "enum", (pClass->iFlags & PH7_CLASS_ENUM) != 0);` |
|   154 |  290 | `	if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|   ! 0 |  291 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "int", (int)sizeof("int")-1);` |
|   154 |  292 | `	}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|     3 |  293 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "string", (int)sizeof("string")-1);` |
|     2 |  294 | `	}else{` |
|   152 |  295 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "", 0);` |
|     - |  296 | `	}` |
|     - |  297 | `	{` |
|     - |  298 | `		/* Enum case names in declaration order (empty list for non-enums) */` |
|   154 |  299 | `		ph7_value *pCases = ph7_context_new_array(pCtx);` |
|   154 |  300 | `		if( pCases ){` |
|   154 |  301 | `			ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     - |  302 | `			sxu32 nCase;` |
|   160 |  303 | `			for( nCase = 0 ; nCase < SySetUsed(&pClass->aEnumCases) ; nCase++ ){` |
|     7 |  304 | `				ph7_value *pNm = ph7_context_new_scalar(pCtx);` |
|     7 |  305 | `				if( pNm ){` |
|     7 |  306 | `					ph7_value_string(pNm,apCase[nCase]->sName.zString,(int)apCase[nCase]->sName.nByte);` |
|     7 |  307 | `					ph7_array_add_elem(pCases,0,pNm);` |
|     3 |  308 | `				}` |
|     4 |  309 | `			}` |
|   154 |  310 | `			ph7_array_add_strkey_elem(pInfo,"cases",pCases);` |
|    75 |  311 | `		}` |
|     - |  312 | `	}` |
|   154 |  313 | `	if( pClass->pBase ){` |
|    41 |  314 | `		ReflectMapAddStr(pCtx, pInfo, "parent", SyStringData(&pClass->pBase->sName),` |
|    26 |  315 | `			(int)SyStringLength(&pClass->pBase->sName));` |
|    15 |  316 | `	}else{` |
|   128 |  317 | `		ReflectMapAddNull(pCtx, pInfo, "parent");` |
|     - |  318 | `	}` |
|     - |  319 | `	/* Transitive interfaces */` |
|   154 |  320 | `	SySetInit(&aIfaceSet, &pVm->sAllocator, sizeof(ph7_class *));` |
|   154 |  321 | `	ReflectCollectInterfaces(pClass, &aIfaceSet, 0);` |
|   154 |  322 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     - |  323 | `		/* An interface's own parents count as its interface list */` |
|    11 |  324 | `		if( pClass->pBase ){` |
|     3 |  325 | `			ReflectAddInterface(pClass->pBase, &aIfaceSet, 0);` |
|     1 |  326 | `		}` |
|     5 |  327 | `	}` |
|   154 |  328 | `	pList = ph7_context_new_array(pCtx);` |
|   154 |  329 | `	if( pList ){` |
|   154 |  330 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIfaceSet);` |
|   194 |  331 | `		for( n = 0 ; n < SySetUsed(&aIfaceSet) ; n++ ){` |
|    42 |  332 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|    42 |  333 | `			if( pName == 0 ){ break; }` |
|    42 |  334 | `			ph7_value_string(pName, SyStringData(&apIface[n]->sName), (int)SyStringLength(&apIface[n]->sName));` |
|    42 |  335 | `			ph7_array_add_elem(pList, 0, pName);` |
|    42 |  336 | `			if( pVm->pTraversableClass && apIface[n] == pVm->pTraversableClass ){` |
|     3 |  337 | `				bIterable = 1;` |
|     1 |  338 | `			}` |
|    22 |  339 | `		}` |
|   154 |  340 | `		ph7_array_add_strkey_elem(pInfo, "interfaces", pList);` |
|    75 |  341 | `	}` |
|   154 |  342 | `	SySetRelease(&aIfaceSet);` |
|   154 |  343 | `	ReflectMapAddBool(pCtx, pInfo, "iterable", bIterable);` |
|     - |  344 | `	/* Used traits */` |
|   154 |  345 | `	pList = ph7_context_new_array(pCtx);` |
|   154 |  346 | `	if( pList ){` |
|   154 |  347 | `		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|   160 |  348 | `		for( n = 0 ; n < SySetUsed(&pClass->aTrait) ; n++ ){` |
|     9 |  349 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|     9 |  350 | `			if( pName == 0 ){ break; }` |
|     9 |  351 | `			ph7_value_string(pName, SyStringData(&apTrait[n]->sName), (int)SyStringLength(&apTrait[n]->sName));` |
|     9 |  352 | `			ph7_array_add_elem(pList, 0, pName);` |
|     6 |  353 | `		}` |
|   154 |  354 | `		ph7_array_add_strkey_elem(pInfo, "traits", pList);` |
|    75 |  355 | `	}` |
|     - |  356 | `	/* File / lines: no file recorded => false, like PHP internals */` |
|   154 |  357 | `	if( SyStringLength(&pClass->sFile) > 0 ){` |
|   142 |  358 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));` |
|    73 |  359 | `	}else{` |
|    13 |  360 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - |  361 | `	}` |
|   154 |  362 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pClass->nLine);` |
|   154 |  363 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pClass->nEndLine);` |
|   154 |  364 | `	ReflectMapAddDoc(pCtx, pInfo, &pClass->sDoc);` |
|   154 |  365 | `	ReflectMapAddAttrs(pCtx, pInfo, &pClass->aAttrs);` |
|     - |  366 | `	/* Members are emitted in PHP's reporting order: the class's own members` |
|     - |  367 | `	 * first (declaration order), then each inheritance level's, outward.` |
|     - |  368 | `	 * Per level we iterate the DECLARING class's own hash — subclass hashes` |
|     - |  369 | `	 * interleave inherited pointers unpredictably — and emit buffered` |
|     - |  370 | `	 * entries in reverse, because SyHash lists are LIFO. A pointer-identity` |
|     - |  371 | `	 * lookup in the reflected class's hash filters out members that are not` |
|     - |  372 | `	 * visible there (base privates, overridden entries). */` |
|     - |  373 | `	{` |
|     - |  374 | `		ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];` |
|   154 |  375 | `		ph7_class *pWalk = pClass;` |
|     - |  376 | `		SySet aTmp;` |
|   154 |  377 | `		sxu32 nChain = 0, iLevel, nT;` |
|   334 |  378 | `		while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){` |
|   184 |  379 | `			aChain[nChain++] = pWalk;` |
|   184 |  380 | `			pWalk = pWalk->pBase;` |
|     4 |  381 | `		}` |
|   154 |  382 | `		SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));` |
|   334 |  383 | `		for( iLevel = 0 ; iLevel < nChain ; iLevel++ ){` |
|   184 |  384 | `			ph7_class *pLevel = aChain[iLevel];` |
|     - |  385 | `			/* --- Properties (hAttr) then constants/enum cases (hConst) — php's two` |
|     - |  386 | `			 * separate member namespaces. Each table is collected and emitted` |
|     - |  387 | `			 * independently; the CONSTANT flag still routes each to pConsts/pProps. --- */` |
|     - |  388 | `			{` |
|     - |  389 | `			int iTab;` |
|   544 |  390 | `			for( iTab = 0 ; iTab < 2 ; iTab++ ){` |
|   364 |  391 | `			SyHash *pSrcHash = iTab ? &pLevel->hConst : &pLevel->hAttr;` |
|   364 |  392 | `			SyHash *pRefHash = iTab ? &pClass->hConst : &pClass->hAttr;` |
|   364 |  393 | `			SySetReset(&aTmp);` |
|   364 |  394 | `			SyHashResetLoopCursor(pSrcHash);` |
|   620 |  395 | `			while( (pEntry = SyHashGetNextEntry(pSrcHash)) != 0 ){` |
|   260 |  396 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   260 |  397 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|   260 |  398 | `				if( iLevel == 0 ){` |
|     - |  399 | `					sxu32 j;` |
|     - |  400 | `					/* Own = declared here or by an off-chain provider (trait) */` |
|   254 |  401 | `					for( j = 1 ; j < nChain ; j++ ){` |
|    79 |  402 | `						if( aChain[j] == pDecl ){ break; }` |
|    22 |  403 | `					}` |
|   212 |  404 | `					if( j < nChain ){ continue; }` |
|    90 |  405 | `				}else{` |
|     - |  406 | `					SyHashEntry *pSub;` |
|    49 |  407 | `					if( pDecl != pLevel ){ continue; }` |
|     - |  408 | `					/* Must still be the visible member in the reflected class */` |
|    37 |  409 | `					pSub = SyHashGet(pRefHash, pEntry->pKey, pEntry->nKeyLen);` |
|    37 |  410 | `					if( pSub == 0 \|\| pSub->pUserData != (void *)pAttr ){ continue; }` |
|     - |  411 | `				}` |
|   212 |  412 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     4 |  413 | `			}` |
|     - |  414 | `			/* Forward: hAttr now iterates in DECLARATION order (its inserts are` |
|     - |  415 | `			 * tail inserts), so members come out in the order php reports them.` |
|     - |  416 | `			 * This walked aTmp backwards to undo the table's old head-insert` |
|     - |  417 | `			 * (LIFO) storage; with that reversal gone from the table, reversing` |
|     - |  418 | `			 * here would emit members back to front. The METHOD loop below keeps` |
|     - |  419 | `			 * its reverse walk — hMethod is still a head-insert table. */` |
|   572 |  420 | `			for( nT = 0 ; nT < SySetUsed(&aTmp) ; nT++ ){` |
|   212 |  421 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT);` |
|   212 |  422 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;` |
|   212 |  423 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|   212 |  424 | `				ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|   212 |  425 | `				if( pMeta == 0 ){ break; }` |
|   212 |  426 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pAttr->iProtection);` |
|   212 |  427 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   212 |  428 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pAttr->nLine);` |
|   212 |  429 | `				ReflectMapAddDoc(pCtx, pMeta, &pAttr->sDoc);` |
|   212 |  430 | `				ReflectMapAddAttrs(pCtx, pMeta, &pAttr->aAttrs);` |
|   212 |  431 | `				ReflectMapAddBool(pCtx, pMeta, "typed", (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);` |
|   212 |  432 | `				if( SyStringLength(&pAttr->sTypeName) > 0 ){` |
|    70 |  433 | `					ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&pAttr->sTypeName),` |
|    46 |  434 | `						(int)SyStringLength(&pAttr->sTypeName));` |
|    24 |  435 | `				}else{` |
|   166 |  436 | `					ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - |  437 | `				}` |
|   212 |  438 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|    60 |  439 | `					ReflectMapAddBool(pCtx, pMeta, "final", (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|    60 |  440 | `					ReflectMapAddBool(pCtx, pMeta, "enumcase", (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0);` |
|    60 |  441 | `					ReflectMapAddDyn(pCtx, pConsts, &pAttr->sName, pMeta);` |
|    31 |  442 | `				}else{` |
|   154 |  443 | `					ReflectMapAddBool(pCtx, pMeta, "static", (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   154 |  444 | `					ReflectMapAddBool(pCtx, pMeta, "readonly", (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0);` |
|   154 |  445 | `					ReflectMapAddBool(pCtx, pMeta, "privset", (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) != 0);` |
|   154 |  446 | `					ReflectMapAddBool(pCtx, pMeta, "protset", (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET) != 0);` |
|   154 |  447 | `					ReflectMapAddBool(pCtx, pMeta, "hookget", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) != 0);` |
|   154 |  448 | `					ReflectMapAddBool(pCtx, pMeta, "hookset", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) != 0);` |
|   154 |  449 | `					ReflectMapAddBool(pCtx, pMeta, "virtual", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) != 0);` |
|   154 |  450 | `					ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&pAttr->aByteCode) > 0);` |
|   154 |  451 | `					ReflectMapAddDyn(pCtx, pProps, &pAttr->sName, pMeta);` |
|     - |  452 | `				}` |
|   108 |  453 | `			}` |
|   184 |  454 | `			} /* for iTab */` |
|     - |  455 | `			}` |
|     - |  456 | `			/* --- Methods. The reported name is the hash-entry key: trait` |
|     - |  457 | `			 * aliasing installs a shallow copy under the alias name while` |
|     - |  458 | `			 * sFunc.sName keeps the original, and PHP reports the alias. --- */` |
|   184 |  459 | `			SySetReset(&aTmp);` |
|   184 |  460 | `			SyHashResetLoopCursor(&pLevel->hMethod);` |
|   484 |  461 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){` |
|   304 |  462 | `				ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   304 |  463 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|   304 |  464 | `				if( iLevel == 0 ){` |
|     - |  465 | `					sxu32 j;` |
|   282 |  466 | `					for( j = 1 ; j < nChain ; j++ ){` |
|   111 |  467 | `						if( aChain[j] == pDecl ){ break; }` |
|    29 |  468 | `					}` |
|   226 |  469 | `					if( j < nChain ){ continue; }` |
|    88 |  470 | `				}else{` |
|     - |  471 | `					SyHashEntry *pSub;` |
|    79 |  472 | `					if( pDecl != pLevel ){ continue; }` |
|    57 |  473 | `					pSub = SyHashGet(&pClass->hMethod, pEntry->pKey, pEntry->nKeyLen);` |
|    57 |  474 | `					if( pSub == 0 ){` |
|     - |  475 | `						/* Not in the subclass table: inheritance skips private` |
|     - |  476 | `						 * methods, but PHP still reports them on the subclass` |
|     - |  477 | `						 * (Zend copies privates into the child function table). */` |
|   ! 0 |  478 | `						if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|   ! 0 |  479 | `							continue;` |
|   ! 0 |  480 | `						}` |
|    57 |  481 | `					}else if( pSub->pUserData != (void *)pMeth ){` |
|     - |  482 | `						/* Overridden below this level: already reported */` |
|     3 |  483 | `						continue;` |
|     - |  484 | `					}` |
|     - |  485 | `				}` |
|   226 |  486 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     4 |  487 | `			}` |
|   406 |  488 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|   226 |  489 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|   226 |  490 | `				ph7_class_method *pMeth = (ph7_class_method *)pE->pUserData;` |
|   226 |  491 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|     - |  492 | `				ph7_value *pMeta;` |
|     - |  493 | `				SyString sKey;` |
|   226 |  494 | `				SyStringInitFromBuf(&sKey, (const char *)pE->pKey, pE->nKeyLen);` |
|   222 |  495 | `				if( sKey.nByte == sizeof("__construct")-1` |
|   137 |  496 | `				 && SyMemcmp(sKey.zString, "__construct", sKey.nByte) == 0 ){` |
|    38 |  497 | `					if( iCtorVis == 0 ){` |
|    38 |  498 | `						iCtorVis = pMeth->iProtection;` |
|    17 |  499 | `					}` |
|     - |  500 | `					/* A __construct key whose method has a DIFFERENT own name is a trait` |
|     - |  501 | ``					 * `use T { m as __construct; }` alias. php lists such a method under`` |
|     - |  502 | `					 * BOTH names (its own and __construct) and getConstructor() resolves` |
|     - |  503 | `					 * the __construct one, so emit the entry under this key too rather than` |
|     - |  504 | `					 * skipping it. (The legacy PHP-4 class-name-constructor mount alias that` |
|     - |  505 | `					 * also produced a __construct key is gone, removed in 8.0.) */` |
|   209 |  506 | `				}else if( sKey.nByte == sizeof("__clone")-1` |
|   111 |  507 | `				 && SyMemcmp(sKey.zString, "__clone", sKey.nByte) == 0 ){` |
|   ! 0 |  508 | `					if( iCloneVis == 0 ){` |
|   ! 0 |  509 | `						iCloneVis = pMeth->iProtection;` |
|   ! 0 |  510 | `					}` |
|   ! 0 |  511 | `				}` |
|     - |  512 | `				/* No PHP-4 class-name constructor: a method named like the class is a` |
|     - |  513 | `				 * plain method (removed in 8.0), so getConstructor() stays null unless` |
|     - |  514 | `				 * an explicit __construct exists. */` |
|   226 |  515 | `				pMeta = ph7_context_new_array(pCtx);` |
|   226 |  516 | `				if( pMeta == 0 ){ break; }` |
|   226 |  517 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pMeth->iProtection);` |
|   226 |  518 | `				ReflectMapAddBool(pCtx, pMeta, "static", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   226 |  519 | `				ReflectMapAddBool(pCtx, pMeta, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|   226 |  520 | `				ReflectMapAddBool(pCtx, pMeta, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   226 |  521 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   226 |  522 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pMeth->nLine);` |
|   226 |  523 | `				ReflectMapAddDyn(pCtx, pMethods, &sKey, pMeta);` |
|   115 |  524 | `			}` |
|    94 |  525 | `		}` |
|   154 |  526 | `		SySetRelease(&aTmp);` |
|     - |  527 | `	}` |
|   154 |  528 | `	ReflectMapAddInt(pCtx, pInfo, "ctorvis", (sxi64)iCtorVis);` |
|   154 |  529 | `	ReflectMapAddInt(pCtx, pInfo, "clonevis", (sxi64)iCloneVis);` |
|   154 |  530 | `	ph7_array_add_strkey_elem(pInfo, "consts", pConsts);` |
|   154 |  531 | `	ph7_array_add_strkey_elem(pInfo, "props", pProps);` |
|   154 |  532 | `	ph7_array_add_strkey_elem(pInfo, "methods", pMethods);` |
|   154 |  533 | `	ph7_result_value(pCtx, pInfo);` |
|   154 |  534 | `	return PH7_OK;` |
|    86 |  535 | `}` |
|     - |  536 | `/*` |
|     - |  537 | ` * mixed __reflect_const_value(string $class, string $name)` |
|     - |  538 | ` * Value of a class constant. The PHP layer guarantees existence.` |
|     - |  539 | ` */` |
|    56 |  540 | `static int vm_builtin_reflect_const_value(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 |  541 | `{` |
|     - |  542 | `	ph7_class *pClass;` |
|     - |  543 | `	ph7_class_attr *pAttr;` |
|     - |  544 | `	ph7_value *pValue;` |
|    56 |  545 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    56 |  546 | `	 \|\| (pAttr = ReflectFetchConst(pClass, apArg[1])) == 0` |
|    58 |  547 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|   ! 0 |  548 | `		ph7_result_null(pCtx);` |
|   ! 0 |  549 | `		return PH7_OK;` |
|     - |  550 | `	}` |
|     - |  551 | `	/* Constant slots are evaluated lazily on first access */` |
|    58 |  552 | `	if( PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr) != SXRET_OK ){` |
|     - |  553 | `		/* Initializer raised: the throw is in flight; report null here */` |
|   ! 0 |  554 | `		ph7_result_null(pCtx);` |
|   ! 0 |  555 | `		return PH7_OK;` |
|     - |  556 | `	}` |
|    58 |  557 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|    58 |  558 | `	if( pValue ){` |
|    58 |  559 | `		ph7_result_value(pCtx, pValue);` |
|    30 |  560 | `	}else{` |
|   ! 0 |  561 | `		ph7_result_null(pCtx);` |
|     - |  562 | `	}` |
|    58 |  563 | `	return PH7_OK;` |
|    30 |  564 | `}` |
|     - |  565 | `/*` |
|     - |  566 | ` * mixed __reflect_static_value(string $class, string $name)` |
|     - |  567 | ` * Current value of a static property (visibility ignored).` |
|     - |  568 | ` */` |
|    12 |  569 | `static int vm_builtin_reflect_static_value(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  570 | `{` |
|     - |  571 | `	ph7_class *pClass;` |
|     - |  572 | `	ph7_class_attr *pAttr;` |
|     - |  573 | `	ph7_value *pValue;` |
|    12 |  574 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    12 |  575 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|    13 |  576 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|   ! 0 |  577 | `		ph7_result_null(pCtx);` |
|   ! 0 |  578 | `		return PH7_OK;` |
|     - |  579 | `	}` |
|     - |  580 | `	{` |
|     - |  581 | `		/* Uninitialized typed static: same Error the VM raises on read */` |
|    13 |  582 | `		SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|    13 |  583 | `		if( pSlot ){` |
|   ! 0 |  584 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|   ! 0 |  585 | `			if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|   ! 0 |  586 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|   ! 0 |  587 | `				return PH7_VmThrowException(pCtx, "Error",` |
|     - |  588 | `					"Typed static property %z::$%z must not be accessed before initialization",` |
|   ! 0 |  589 | `					&pDecl->sName, &pAttr->sName);` |
|     - |  590 | `			}` |
|   ! 0 |  591 | `		}` |
|     - |  592 | `	}` |
|    13 |  593 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|    13 |  594 | `	if( pValue ){` |
|    13 |  595 | `		ph7_result_value(pCtx, pValue);` |
|     7 |  596 | `	}else{` |
|   ! 0 |  597 | `		ph7_result_null(pCtx);` |
|     - |  598 | `	}` |
|    13 |  599 | `	return PH7_OK;` |
|     7 |  600 | `}` |
|     - |  601 | `/*` |
|     - |  602 | ` * bool __reflect_static_set(string $class, string $name, mixed $value)` |
|     - |  603 | ` * Overwrite a static property's shared slot (visibility ignored).` |
|     - |  604 | ` */` |
|     4 |  605 | `static int vm_builtin_reflect_static_set(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  606 | `{` |
|     - |  607 | `	ph7_class *pClass;` |
|     - |  608 | `	ph7_class_attr *pAttr;` |
|     - |  609 | `	ph7_value *pValue;` |
|     4 |  610 | `	if( nArg < 3 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|     4 |  611 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|     5 |  612 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|   ! 0 |  613 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  614 | `		return PH7_OK;` |
|     - |  615 | `	}` |
|     5 |  616 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|     5 |  617 | `	if( pValue == 0 ){` |
|   ! 0 |  618 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  619 | `		return PH7_OK;` |
|     - |  620 | `	}` |
|     - |  621 | `	{` |
|     5 |  622 | `		sxi32 rc = ReflectEnforceStore(pCtx, pAttr->nIdx, apArg[2]);` |
|     5 |  623 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  624 | `			return rc;` |
|     - |  625 | `		}` |
|     - |  626 | `	}` |
|     5 |  627 | `	PH7_MemObjStore(apArg[2], pValue);` |
|     5 |  628 | `	ph7_result_bool(pCtx, 1);` |
|     5 |  629 | `	return PH7_OK;` |
|     3 |  630 | `}` |
|     - |  631 | `/*` |
|     - |  632 | ` * mixed __reflect_prop_default(string $class, string $name)` |
|     - |  633 | ` * Evaluate a non-static property's compiled default expression` |
|     - |  634 | ` * (null when the property has no default).` |
|     - |  635 | ` */` |
|    24 |  636 | `static int vm_builtin_reflect_prop_default(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  637 | `{` |
|     - |  638 | `	ph7_class *pClass;` |
|     - |  639 | `	ph7_class_attr *pAttr;` |
|     - |  640 | `	ph7_value sValue;` |
|    24 |  641 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    24 |  642 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|    24 |  643 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) != 0` |
|    25 |  644 | `	 \|\| SySetUsed(&pAttr->aByteCode) < 1 ){` |
|     3 |  645 | `		ph7_result_null(pCtx);` |
|     3 |  646 | `		return PH7_OK;` |
|     - |  647 | `	}` |
|    23 |  648 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     - |  649 | `	/* Same evaluation path the VM uses for omitted call arguments */` |
|    23 |  650 | `	VmLocalExec(pCtx->pVm, &pAttr->aByteCode, &sValue, FALSE);` |
|    23 |  651 | `	ph7_result_value(pCtx, &sValue);` |
|    23 |  652 | `	PH7_MemObjRelease(&sValue);` |
|    23 |  653 | `	return PH7_OK;` |
|    13 |  654 | `}` |
|     - |  655 | `/*` |
|     - |  656 | ` * Collect a PHP array's values into a ph7_value* set (call arguments).` |
|     - |  657 | ` * When ppNames is non-NULL, string keys become named arguments: a name` |
|     - |  658 | ` * map is lazily allocated (like call_user_func_array's) with one entry` |
|     - |  659 | ` * per collected slot, empty entries meaning positional.` |
|     - |  660 | ` */` |
|    46 |  661 | `static sxi32 ReflectCollectArgs(ph7_context *pCtx, ph7_value *pArray, SySet *pOut, SyString **ppNames)` |
|     1 |  662 | `{` |
|     - |  663 | `	ph7_hashmap *pMap;` |
|     - |  664 | `	ph7_hashmap_node *pEntry;` |
|    47 |  665 | `	SyString *aNames = 0;` |
|    47 |  666 | `	sxu32 nSlot = 0;` |
|     - |  667 | `	sxu32 n;` |
|    47 |  668 | `	if( ppNames ){` |
|    27 |  669 | `		*ppNames = 0;` |
|    13 |  670 | `	}` |
|    47 |  671 | `	if( !ph7_value_is_array(pArray) ){` |
|   ! 0 |  672 | `		return SXRET_OK;` |
|     - |  673 | `	}` |
|    47 |  674 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|    47 |  675 | `	pEntry = pMap->pFirst;` |
|   107 |  676 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    61 |  677 | `		ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|    61 |  678 | `		if( pValue ){` |
|    61 |  679 | `			if( ppNames && pEntry->iType == HASHMAP_BLOB_NODE ){` |
|     3 |  680 | `				if( aNames == 0 ){` |
|     4 |  681 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     2 |  682 | `						pMap->nEntry * sizeof(SyString));` |
|     3 |  683 | `					if( aNames ){` |
|     3 |  684 | `						SyZero(aNames, pMap->nEntry * sizeof(SyString));` |
|     1 |  685 | `					}` |
|     1 |  686 | `				}` |
|     3 |  687 | `				if( aNames ){` |
|     3 |  688 | `					SyStringInitFromBuf(&aNames[nSlot],` |
|     - |  689 | `						SyBlobData(&pEntry->xKey.sKey), SyBlobLength(&pEntry->xKey.sKey));` |
|     1 |  690 | `				}` |
|     1 |  691 | `			}` |
|    61 |  692 | `			SySetPut(pOut, (const void *)&pValue);` |
|    61 |  693 | `			nSlot++;` |
|    30 |  694 | `		}` |
|    61 |  695 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|    31 |  696 | `	}` |
|    47 |  697 | `	if( ppNames ){` |
|    27 |  698 | `		*ppNames = aNames;` |
|    13 |  699 | `	}` |
|    47 |  700 | `	return SXRET_OK;` |
|    24 |  701 | `}` |
|     - |  702 | `/*` |
|     - |  703 | ` * object __reflect_new_instance(string $class, array $args)` |
|     - |  704 | ` * Instantiate and run the constructor with positional arguments.` |
|     - |  705 | ` * The PHP layer has already validated instantiability and ctor visibility.` |
|     - |  706 | ` */` |
|    30 |  707 | `static int vm_builtin_reflect_new_instance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  708 | `{` |
|    31 |  709 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  710 | `	ph7_class *pClass;` |
|     - |  711 | `	ph7_class_instance *pThis;` |
|     - |  712 | `	ph7_class_method *pCons;` |
|    31 |  713 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pVm, apArg[0])) == 0 ){` |
|   ! 0 |  714 | `		ph7_result_null(pCtx);` |
|   ! 0 |  715 | `		return PH7_OK;` |
|     - |  716 | `	}` |
|    31 |  717 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|    31 |  718 | `	if( pThis == 0 ){` |
|   ! 0 |  719 | `		ph7_result_null(pCtx);` |
|   ! 0 |  720 | `		return PH7_OK;` |
|     - |  721 | `	}` |
|    31 |  722 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|    31 |  723 | `	if( pCons ){` |
|     - |  724 | `		SySet aArg;` |
|     - |  725 | `		sxi32 rc;` |
|    27 |  726 | `		SyString *aNames = 0;` |
|    27 |  727 | `		SySetInit(&aArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    27 |  728 | `		if( nArg > 1 ){` |
|    27 |  729 | `			ReflectCollectArgs(pCtx, apArg[1], &aArg, &aNames);` |
|    13 |  730 | `		}` |
|    27 |  731 | `		if( aNames ){` |
|     - |  732 | `			VmCallArgMap sMap;` |
|     3 |  733 | `			SyZero(&sMap,sizeof(sMap)); /* new map fields must read unset, not stack garbage */` |
|     3 |  734 | `			sMap.bHasNamed = 1;` |
|     3 |  735 | `			sMap.bIsNamespaced = 0;` |
|     3 |  736 | `			sMap.bStrict = 0;` |
|     3 |  737 | `			sMap.nTotal = SySetUsed(&aArg);` |
|     3 |  738 | `			sMap.aNames = aNames;` |
|     4 |  739 | `			rc = PH7_VmCallClassMethodMap(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|     2 |  740 | `				(ph7_value **)SySetBasePtr(&aArg), &sMap);` |
|     3 |  741 | `			SyMemBackendFree(&pVm->sAllocator, aNames);` |
|     2 |  742 | `		}else{` |
|    37 |  743 | `			rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|    24 |  744 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|     - |  745 | `		}` |
|    27 |  746 | `		SySetRelease(&aArg);` |
|    27 |  747 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 |  748 | `			PH7_ClassInstanceUnref(pThis);` |
|   ! 0 |  749 | `			return rc;` |
|     - |  750 | `		}` |
|    13 |  751 | `	}` |
|    31 |  752 | `	return ReflectResultObject(pCtx, pThis);` |
|    16 |  753 | `}` |
|     - |  754 | `/*` |
|     - |  755 | ` * object __reflect_new_no_ctor(string $class)` |
|     - |  756 | ` * Instantiate without running the constructor (property defaults still` |
|     - |  757 | ` * apply — PH7_NewClassInstance builds the attribute frame).` |
|     - |  758 | ` */` |
|    68 |  759 | `static int vm_builtin_reflect_new_no_ctor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  760 | `{` |
|     - |  761 | `	ph7_class *pClass;` |
|    69 |  762 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0 ){` |
|   ! 0 |  763 | `		ph7_result_null(pCtx);` |
|   ! 0 |  764 | `		return PH7_OK;` |
|     - |  765 | `	}` |
|    69 |  766 | `	return ReflectResultObject(pCtx, PH7_NewClassInstance(pCtx->pVm, pClass));` |
|    35 |  767 | `}` |
|     - |  768 | `/*` |
|     - |  769 | ` * Typed/readonly store enforcement for reflection writes. Like the VM's` |
|     - |  770 | ` * store path, except an UNINITIALIZED readonly property may be written from` |
|     - |  771 | ` * any scope (PHP lets ReflectionProperty::setValue initialize readonly): the` |
|     - |  772 | ` * READONLY bit is masked off for the enforcement call so the set-scope check` |
|     - |  773 | ` * is skipped, while an already-initialized readonly still gets PHP's` |
|     - |  774 | ` * "Cannot modify readonly property" Error. Returns SXRET_OK/PH7_EXCEPTION/` |
|     - |  775 | ` * PH7_ABORT; the value may be coerced in place.` |
|     - |  776 | ` */` |
|    10 |  777 | `static sxi32 ReflectEnforceStore(ph7_context *pCtx, sxu32 nIdx, ph7_value *pValue)` |
|     1 |  778 | `{` |
|    11 |  779 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  780 | `	SyHashEntry *pSlot;` |
|     - |  781 | `	VmClassAttr *pVmAttr;` |
|     - |  782 | `	ph7_class_attr *pAttr;` |
|     - |  783 | `	sxi32 iSaved, rc;` |
|    11 |  784 | `	pSlot = SyHashGet(&pVm->hTypedSlot, (const void *)&nIdx, sizeof(sxu32));` |
|    11 |  785 | `	if( pSlot == 0 ){` |
|     7 |  786 | `		return SXRET_OK; /* Untyped slot: plain store */` |
|     - |  787 | `	}` |
|     5 |  788 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     5 |  789 | `	pAttr = pVmAttr->pAttr;` |
|     5 |  790 | `	if( pAttr == 0 ){` |
|   ! 0 |  791 | `		return SXRET_OK;` |
|     - |  792 | `	}` |
|     5 |  793 | `	iSaved = pAttr->iFlags;` |
|     5 |  794 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) ){` |
|   ! 0 |  795 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_READONLY;` |
|   ! 0 |  796 | `	}` |
|     5 |  797 | `	rc = PH7_VmEnforcePropStore(pVm, nIdx, pValue);` |
|     5 |  798 | `	pAttr->iFlags = iSaved;` |
|     5 |  799 | `	return rc;` |
|     6 |  800 | `}` |
|     - |  801 | `/*` |
|     - |  802 | ` * mixed __reflect_prop_read(object $obj, string $name)` |
|     - |  803 | ` * Instance property read, visibility ignored. Throws PHP's Error for an` |
|     - |  804 | ` * uninitialized typed property.` |
|     - |  805 | ` */` |
|    20 |  806 | `static int vm_builtin_reflect_prop_read(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  807 | `{` |
|     - |  808 | `	ph7_class_instance *pThis;` |
|     - |  809 | `	SyHashEntry *pEntry;` |
|     - |  810 | `	VmClassAttr *pVmAttr;` |
|     - |  811 | `	ph7_value *pValue;` |
|     - |  812 | `	const char *zName;` |
|     - |  813 | `	int nLen;` |
|    21 |  814 | `	if( nArg < 2 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  815 | `		ph7_result_null(pCtx);` |
|   ! 0 |  816 | `		return PH7_OK;` |
|     - |  817 | `	}` |
|    21 |  818 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    21 |  819 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|    21 |  820 | `	pEntry = nLen > 0 ? SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen) : 0;` |
|    21 |  821 | `	if( pEntry == 0 ){` |
|   ! 0 |  822 | `		ph7_result_null(pCtx);` |
|   ! 0 |  823 | `		return PH7_OK;` |
|     - |  824 | `	}` |
|    21 |  825 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    21 |  826 | `	if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|     3 |  827 | `		ph7_class *pDecl = pVmAttr->pAttr->pDeclClass ? pVmAttr->pAttr->pDeclClass : pThis->pClass;` |
|     4 |  828 | `		return PH7_VmThrowException(pCtx, "Error",` |
|     - |  829 | `			"Typed property %z::$%z must not be accessed before initialization",` |
|     2 |  830 | `			&pDecl->sName, &pVmAttr->pAttr->sName);` |
|     - |  831 | `	}` |
|    19 |  832 | `	pValue = PH7_ClassInstanceExtractAttrValue(pThis, pVmAttr);` |
|    19 |  833 | `	if( pValue ){` |
|    19 |  834 | `		ph7_result_value(pCtx, pValue);` |
|    10 |  835 | `	}else{` |
|   ! 0 |  836 | `		ph7_result_null(pCtx);` |
|     - |  837 | `	}` |
|    19 |  838 | `	return PH7_OK;` |
|    11 |  839 | `}` |
|     - |  840 | `/*` |
|     - |  841 | ` * bool __reflect_prop_write(object $obj, string $name, mixed $value)` |
|     - |  842 | ` * Instance property write, visibility ignored; typed and readonly rules` |
|     - |  843 | ` * enforced (see ReflectEnforceStore).` |
|     - |  844 | ` */` |
|     6 |  845 | `static int vm_builtin_reflect_prop_write(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  846 | `{` |
|     - |  847 | `	ph7_class_instance *pThis;` |
|     - |  848 | `	SyHashEntry *pEntry;` |
|     - |  849 | `	VmClassAttr *pVmAttr;` |
|     - |  850 | `	ph7_value *pValue;` |
|     - |  851 | `	const char *zName;` |
|     - |  852 | `	sxi32 rc;` |
|     - |  853 | `	int nLen;` |
|     7 |  854 | `	if( nArg < 3 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  855 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  856 | `		return PH7_OK;` |
|     - |  857 | `	}` |
|     7 |  858 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     7 |  859 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|     7 |  860 | `	pEntry = nLen > 0 ? SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen) : 0;` |
|     7 |  861 | `	if( pEntry == 0 ){` |
|   ! 0 |  862 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  863 | `		return PH7_OK;` |
|     - |  864 | `	}` |
|     7 |  865 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     7 |  866 | `	rc = ReflectEnforceStore(pCtx, pVmAttr->nIdx, apArg[2]);` |
|     7 |  867 | `	if( rc != SXRET_OK ){` |
|     3 |  868 | `		return rc;` |
|     - |  869 | `	}` |
|     5 |  870 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|     5 |  871 | `	if( pValue == 0 ){` |
|   ! 0 |  872 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  873 | `		return PH7_OK;` |
|     - |  874 | `	}` |
|     5 |  875 | `	PH7_MemObjStore(apArg[2], pValue);` |
|     5 |  876 | `	ph7_result_bool(pCtx, 1);` |
|     5 |  877 | `	return PH7_OK;` |
|     4 |  878 | `}` |
|     - |  879 | `/*` |
|     - |  880 | ` * int __reflect_prop_state(object\|string $target, string $name)` |
|     - |  881 | ` * Bitfield: 1 = exists (instance attr / static slot), 2 = initialized,` |
|     - |  882 | ` * 4 = dynamic (instance-owned, not class-declared).` |
|     - |  883 | ` */` |
|    16 |  884 | `static int vm_builtin_reflect_prop_state(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  885 | `{` |
|    17 |  886 | `	int iState = 0;` |
|     - |  887 | `	const char *zName;` |
|     - |  888 | `	int nLen;` |
|    17 |  889 | `	if( nArg < 2 ){` |
|   ! 0 |  890 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  891 | `		return PH7_OK;` |
|     - |  892 | `	}` |
|    17 |  893 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|    17 |  894 | `	if( nLen < 1 ){` |
|   ! 0 |  895 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  896 | `		return PH7_OK;` |
|     - |  897 | `	}` |
|    17 |  898 | `	if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|    17 |  899 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    17 |  900 | `		SyHashEntry *pEntry = SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen);` |
|    17 |  901 | `		if( pEntry ){` |
|    17 |  902 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    17 |  903 | `			iState \|= 1;` |
|    17 |  904 | `			if( (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|    15 |  905 | `				iState \|= 2;` |
|     7 |  906 | `			}` |
|    17 |  907 | `			if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|    11 |  908 | `				iState \|= 4;` |
|     5 |  909 | `			}` |
|     8 |  910 | `		}` |
|     9 |  911 | `	}else{` |
|   ! 0 |  912 | `		ph7_class *pClass = ReflectResolveClass(pCtx->pVm, apArg[0]);` |
|   ! 0 |  913 | `		ph7_class_attr *pAttr = pClass ? ReflectFetchAttr(pClass, apArg[1]) : 0;` |
|   ! 0 |  914 | `		if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|   ! 0 |  915 | `			SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|   ! 0 |  916 | `			iState \|= 1 \| 2;` |
|   ! 0 |  917 | `			if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|   ! 0 |  918 | `				iState &= ~2;` |
|   ! 0 |  919 | `			}` |
|   ! 0 |  920 | `		}` |
|     - |  921 | `	}` |
|    17 |  922 | `	ph7_result_int(pCtx, iState);` |
|    17 |  923 | `	return PH7_OK;` |
|     9 |  924 | `}` |
|     - |  925 | `/*` |
|     - |  926 | ` * array __reflect_dyn_props(object $obj)` |
|     - |  927 | ` * Names of the instance's runtime-added (dynamic) properties, in creation` |
|     - |  928 | ` * order (the instance attr table inserts dynamics at the tail).` |
|     - |  929 | ` */` |
|     2 |  930 | `static int vm_builtin_reflect_dyn_props(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  931 | `{` |
|     - |  932 | `	ph7_class_instance *pThis;` |
|     - |  933 | `	SyHashEntry *pEntry;` |
|     - |  934 | `	ph7_value *pList;` |
|     2 |  935 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|     3 |  936 | `	 \|\| (pList = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 |  937 | `		ph7_result_null(pCtx);` |
|   ! 0 |  938 | `		return PH7_OK;` |
|     - |  939 | `	}` |
|     3 |  940 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     3 |  941 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     7 |  942 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     5 |  943 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     5 |  944 | `		if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|     5 |  945 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|     5 |  946 | `			if( pName == 0 ){ break; }` |
|     7 |  947 | `			ph7_value_string(pName, SyStringData(&pVmAttr->pAttr->sName),` |
|     4 |  948 | `				(int)SyStringLength(&pVmAttr->pAttr->sName));` |
|     5 |  949 | `			ph7_array_add_elem(pList, 0, pName);` |
|     2 |  950 | `		}` |
|     1 |  951 | `	}` |
|     3 |  952 | `	ph7_result_value(pCtx, pList);` |
|     3 |  953 | `	return PH7_OK;` |
|     2 |  954 | `}` |
|     - |  955 | `/* Hand an EXISTING instance to the caller: takes an extra reference` |
|     - |  956 | ` * (unlike ReflectResultObject, which transfers a fresh instance's one). */` |
|     4 |  957 | `static int ReflectResultExistingObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|     1 |  958 | `{` |
|     5 |  959 | `	if( pObj == 0 ){` |
|   ! 0 |  960 | `		ph7_result_null(pCtx);` |
|   ! 0 |  961 | `		return PH7_OK;` |
|     - |  962 | `	}` |
|     5 |  963 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     5 |  964 | `	pObj->iRef++;` |
|     5 |  965 | `	pCtx->pRet->x.pOther = pObj;` |
|     5 |  966 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|     5 |  967 | `	return PH7_OK;` |
|     3 |  968 | `}` |
|     - |  969 | `/* pVal is a Closure instance? Return it, else NULL. */` |
|   500 |  970 | `static ph7_class_instance * ReflectValueClosure(ph7_vm *pVm, ph7_value *pVal)` |
|     3 |  971 | `{` |
|     - |  972 | `	ph7_class_instance *pThis;` |
|   503 |  973 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
|   426 |  974 | `		return 0;` |
|     - |  975 | `	}` |
|    78 |  976 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    78 |  977 | `	return (pThis->pClass == pVm->pClosureClass) ? pThis : 0;` |
|   253 |  978 | `}` |
|     - |  979 | `/*` |
|     - |  980 | ` * Resolve a reflection callable target into its compiled function.` |
|     - |  981 | ` *   - pMethodArg a non-empty string  -> method mode: pTarget is a class name` |
|     - |  982 | ` *     or object; outputs *ppClass and *ppMeth.` |
|     - |  983 | ` *   - pTarget a Closure              -> unwrap $__fn into hFunction; *ppClosure.` |
|     - |  984 | ` *   - pTarget a string               -> hFunction (user) or hHostFunction` |
|     - |  985 | ` *     (*ppHost set, returns NULL).` |
|     - |  986 | ` * Returns the ph7_vm_func, or NULL (host function or unresolvable).` |
|     - |  987 | ` */` |
|   810 |  988 | `static ph7_vm_func * ReflectResolveCallable(ph7_context *pCtx, ph7_value *pTarget,` |
|     - |  989 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|     - |  990 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|     4 |  991 | `{` |
|   814 |  992 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  993 | `	SyHashEntry *pEntry;` |
|   814 |  994 | `	if( ppClass ){ *ppClass = 0; }` |
|   814 |  995 | `	if( ppMeth ){ *ppMeth = 0; }` |
|   814 |  996 | `	if( ppHost ){ *ppHost = 0; }` |
|   814 |  997 | `	if( ppClosure ){ *ppClosure = 0; }` |
|   814 |  998 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
|   313 |  999 | `		ph7_class *pClass = ReflectResolveClass(pVm, pTarget);` |
|     - | 1000 | `		ph7_class_method *pMeth;` |
|   313 | 1001 | `		if( pClass == 0 ){` |
|   ! 0 | 1002 | `			return 0;` |
|     - | 1003 | `		}` |
|   468 | 1004 | `		pMeth = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|   155 | 1005 | `			SyBlobLength(&pMethodArg->sBlob));` |
|   313 | 1006 | `		if( pMeth == 0 ){` |
|     - | 1007 | `			/* getMethods()/ReflectionClass reports a base class's PRIVATE methods on` |
|     - | 1008 | `			 * the subclass (php copies them into the child's table), but private` |
|     - | 1009 | `			 * methods are not inherited into the child's method table, so the plain` |
|     - | 1010 | `			 * extract misses them when a ReflectionMethod obtained from the subclass` |
|     - | 1011 | `			 * is re-resolved by (subclass, name). Walk the base chain to find the` |
|     - | 1012 | `			 * declaring class's own copy. */` |
|   ! 0 | 1013 | `			ph7_class *pWalk = pClass->pBase;` |
|   ! 0 | 1014 | `			while( pWalk && pMeth == 0 ){` |
|   ! 0 | 1015 | `				pMeth = PH7_ClassExtractMethod(pWalk, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|   ! 0 | 1016 | `					SyBlobLength(&pMethodArg->sBlob));` |
|   ! 0 | 1017 | `				pWalk = pWalk->pBase;` |
|   ! 0 | 1018 | `			}` |
|   ! 0 | 1019 | `		}` |
|   313 | 1020 | `		if( pMeth == 0 ){` |
|   ! 0 | 1021 | `			return 0;` |
|     - | 1022 | `		}` |
|   313 | 1023 | `		if( ppClass ){ *ppClass = pClass; }` |
|   313 | 1024 | `		if( ppMeth ){ *ppMeth = pMeth; }` |
|   313 | 1025 | `		return &pMeth->sFunc;` |
|     - | 1026 | `	}` |
|     - | 1027 | `	{` |
|   503 | 1028 | `		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|   503 | 1029 | `		if( pClo ){` |
|     - | 1030 | `			SyString sAttr;` |
|     - | 1031 | `			ph7_value *pFn;` |
|    78 | 1032 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    78 | 1033 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|    78 | 1034 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) < 1 ){` |
|   ! 0 | 1035 | `				return 0;` |
|     - | 1036 | `			}` |
|     - | 1037 | `			/* A closure over an object method or __invoke object` |
|     - | 1038 | `			 * (Closure::fromCallable([$obj,'m']) / fromCallable($invokeObj)) stores the` |
|     - | 1039 | `			 * bare method name in $__fn and the class in $__scope. Resolve it as a METHOD` |
|     - | 1040 | `			 * of $__scope FIRST — before the global function table — so a same-named` |
|     - | 1041 | `			 * global function does not shadow the method (the bug: $__fn "add" hitting a` |
|     - | 1042 | `			 * global add()). A plain anonymous closure's $__fn is its unique lambda name,` |
|     - | 1043 | `			 * which is not a method, so this falls through cleanly. */` |
|     - | 1044 | `			{` |
|     - | 1045 | `				SyString sScope;` |
|     - | 1046 | `				ph7_value *pScope;` |
|    78 | 1047 | `				SyStringInitFromBuf(&sScope, "__scope", 7);` |
|    78 | 1048 | `				pScope = PH7_ClassInstanceFetchAttr(pClo, &sScope);` |
|    78 | 1049 | `				if( pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0 ){` |
|    31 | 1050 | `					ph7_class *pScopeCls = PH7_VmExtractClass(pVm,` |
|    20 | 1051 | `						(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|    21 | 1052 | `					if( pScopeCls ){` |
|    31 | 1053 | `						ph7_class_method *pScopeMeth = PH7_ClassExtractMethod(pScopeCls,` |
|    20 | 1054 | `							(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    21 | 1055 | `						if( pScopeMeth ){` |
|    21 | 1056 | `							if( ppClass ){ *ppClass = pScopeCls; }` |
|    21 | 1057 | `							if( ppMeth ){ *ppMeth = pScopeMeth; }` |
|    21 | 1058 | `							if( ppClosure ){ *ppClosure = pClo; }` |
|    21 | 1059 | `							return &pScopeMeth->sFunc;` |
|     - | 1060 | `						}` |
|   ! 0 | 1061 | `					}` |
|   ! 0 | 1062 | `				}` |
|     - | 1063 | `			}` |
|    58 | 1064 | `			pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    58 | 1065 | `			if( pEntry == 0 ){` |
|     - | 1066 | `				/* A Closure over a host function (Closure::fromCallable('strlen')) */` |
|   ! 0 | 1067 | `				pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|   ! 0 | 1068 | `				if( pEntry && ppHost ){` |
|   ! 0 | 1069 | `					*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|   ! 0 | 1070 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|   ! 0 | 1071 | `				}` |
|   ! 0 | 1072 | `				return 0;` |
|     - | 1073 | `			}` |
|    58 | 1074 | `			if( ppClosure ){ *ppClosure = pClo; }` |
|    58 | 1075 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - | 1076 | `		}` |
|     - | 1077 | `	}` |
|   426 | 1078 | `	if( pTarget->iFlags & MEMOBJ_STRING ){` |
|   426 | 1079 | `		if( SyBlobLength(&pTarget->sBlob) < 1 ){` |
|   ! 0 | 1080 | `			return 0;` |
|     - | 1081 | `		}` |
|   426 | 1082 | `		pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   426 | 1083 | `		if( pEntry ){` |
|   293 | 1084 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - | 1085 | `		}` |
|   134 | 1086 | `		pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   134 | 1087 | `		if( pEntry && ppHost ){` |
|   130 | 1088 | `			*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|    64 | 1089 | `		}` |
|    66 | 1090 | `	}` |
|   134 | 1091 | `	return 0;` |
|   409 | 1092 | `}` |
|     - | 1093 | `/* Emit the shared descriptor fields of a compiled function. */` |
|   626 | 1094 | `static void ReflectFillFuncCommon(ph7_context *pCtx, ph7_value *pInfo, ph7_vm_func *pFunc)` |
|     4 | 1095 | `{` |
|     - | 1096 | `	ph7_vm_func_arg *aArg;` |
|     - | 1097 | `	ph7_value *pParams, *pStatics;` |
|   630 | 1098 | `	int bVariadic = 0;` |
|     - | 1099 | `	int bAnon;` |
|     - | 1100 | `	sxu32 n;` |
|     - | 1101 | ``	/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|     - | 1102 | `	 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|   630 | 1103 | `	bAnon = (pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|   626 | 1104 | `	if( !bAnon && SyStringLength(&pFunc->sName) > 9` |
|   340 | 1105 | `	 && (SyMemcmp(SyStringData(&pFunc->sName), "[lambda_", 8) == 0` |
|    92 | 1106 | `	  \|\| SyMemcmp(SyStringData(&pFunc->sName), "[closure_", 9) == 0) ){` |
|     5 | 1107 | `		bAnon = 1;` |
|     2 | 1108 | `	}` |
|   630 | 1109 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|   630 | 1110 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pFunc->iFlags & VM_FUNC_INTERNAL) != 0);` |
|   630 | 1111 | `	ReflectMapAddBool(pCtx, pInfo, "closure", bAnon);` |
|   630 | 1112 | `	ReflectMapAddBool(pCtx, pInfo, "fstatic", (pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);` |
|   630 | 1113 | `	ReflectMapAddBool(pCtx, pInfo, "byref", (pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|   630 | 1114 | `	ReflectMapAddBool(pCtx, pInfo, "generator", (pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|   630 | 1115 | `	ReflectMapAddBool(pCtx, pInfo, "strict", pFunc->bStrictTypes != 0);` |
|   630 | 1116 | `	if( SyStringLength(&pFunc->sFile) > 0 ){` |
|   626 | 1117 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pFunc->sFile), (int)SyStringLength(&pFunc->sFile));` |
|   315 | 1118 | `	}else{` |
|     5 | 1119 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1120 | `	}` |
|   630 | 1121 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pFunc->nLine);` |
|   630 | 1122 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pFunc->nEndLine);` |
|   630 | 1123 | `	ReflectMapAddDoc(pCtx, pInfo, &pFunc->sDoc);` |
|   630 | 1124 | `	ReflectMapAddAttrs(pCtx, pInfo, &pFunc->aAttrs);` |
|   630 | 1125 | `	if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|   178 | 1126 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", SyStringData(&pFunc->sReturnTypeName),` |
|   118 | 1127 | `			(int)SyStringLength(&pFunc->sReturnTypeName));` |
|   571 | 1128 | `	}else if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     - | 1129 | `		/* The type-text renderer omits void/never atoms (compile.c notes the` |
|     - | 1130 | `		 * root fix belongs there); name them here for getReturnType(). */` |
|     3 | 1131 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "void", sizeof("void")-1);` |
|   511 | 1132 | `	}else if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|     3 | 1133 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "never", sizeof("never")-1);` |
|     2 | 1134 | `	}else{` |
|   508 | 1135 | `		ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1136 | `	}` |
|   630 | 1137 | `	ReflectMapAddBool(pCtx, pInfo, "retnullable", (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) != 0);` |
|     - | 1138 | `	/* Parameters */` |
|   630 | 1139 | `	pParams = ph7_context_new_array(pCtx);` |
|   630 | 1140 | `	aArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|  1932 | 1141 | `	for( n = 0 ; pParams && n < SySetUsed(&pFunc->aArgs) ; n++ ){` |
|  1304 | 1142 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|  1304 | 1143 | `		if( pMeta == 0 ){ break; }` |
|  1304 | 1144 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aArg[n].sName), (int)SyStringLength(&aArg[n].sName));` |
|  1304 | 1145 | `		ReflectMapAddInt(pCtx, pMeta, "pos", (sxi64)n);` |
|  1304 | 1146 | `		ReflectMapAddBool(pCtx, pMeta, "byref", (aArg[n].iFlags & VM_FUNC_ARG_BY_REF) != 0);` |
|  1304 | 1147 | `		ReflectMapAddBool(pCtx, pMeta, "variadic", (aArg[n].iFlags & VM_FUNC_ARG_VARIADIC) != 0);` |
|     - | 1148 | `		/* The compiler never sets ARG_HAS_DEF; a default = compiled bytecode` |
|     - | 1149 | `		 * (same test the OP_CALL default-value path uses). */` |
|  1304 | 1150 | `		ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&aArg[n].aByteCode) > 0);` |
|  1304 | 1151 | `		ReflectMapAddBool(pCtx, pMeta, "nullable", (aArg[n].iFlags & VM_FUNC_ARG_NULLABLE) != 0);` |
|  1304 | 1152 | `		ReflectMapAddBool(pCtx, pMeta, "promoted", (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) != 0);` |
|  1304 | 1153 | `		if( SyStringLength(&aArg[n].sTypeName) > 0 ){` |
|   784 | 1154 | `			ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&aArg[n].sTypeName),` |
|   522 | 1155 | `				(int)SyStringLength(&aArg[n].sTypeName));` |
|   262 | 1156 | `		}else{` |
|   782 | 1157 | `			ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - | 1158 | `		}` |
|  1304 | 1159 | `		ReflectMapAddAttrs(pCtx, pMeta, &aArg[n].aAttrs);` |
|  1304 | 1160 | `		ph7_array_add_elem(pParams, 0, pMeta);` |
|  1304 | 1161 | `		if( aArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|   189 | 1162 | `			bVariadic = 1;` |
|    94 | 1163 | `		}` |
|   653 | 1164 | `	}` |
|   630 | 1165 | `	if( pParams ){` |
|   630 | 1166 | `		ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|   313 | 1167 | `	}` |
|   630 | 1168 | `	ReflectMapAddBool(pCtx, pInfo, "variadic", bVariadic);` |
|     - | 1169 | `	/* Static variables: current value when the slot was initialized (first` |
|     - | 1170 | `	 * call), otherwise the evaluated default — PHP's getStaticVariables` |
|     - | 1171 | `	 * initializes on demand and reports the same values. */` |
|   630 | 1172 | `	pStatics = ph7_context_new_array(pCtx);` |
|   630 | 1173 | `	if( pStatics ){` |
|   630 | 1174 | `		ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|   658 | 1175 | `		for( n = 0 ; n < SySetUsed(&pFunc->aStatic) ; n++ ){` |
|    29 | 1176 | `			ph7_value *pVal = 0;` |
|     - | 1177 | `			ph7_value sScratch;` |
|    29 | 1178 | `			int bScratch = 0;` |
|    29 | 1179 | `			if( aStatic[n].nIdx != SXU32_HIGH ){` |
|    11 | 1180 | `				pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);` |
|     5 | 1181 | `			}` |
|    29 | 1182 | `			if( pVal == 0 ){` |
|    19 | 1183 | `				PH7_MemObjInit(pCtx->pVm, &sScratch);` |
|    19 | 1184 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|    19 | 1185 | `					VmLocalExec(pCtx->pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|     9 | 1186 | `				}` |
|    19 | 1187 | `				pVal = &sScratch;` |
|    19 | 1188 | `				bScratch = 1;` |
|     9 | 1189 | `			}` |
|    29 | 1190 | `			ReflectMapAddDyn(pCtx, pStatics, &aStatic[n].sName, pVal);` |
|    29 | 1191 | `			if( bScratch ){` |
|    19 | 1192 | `				PH7_MemObjRelease(&sScratch);` |
|     9 | 1193 | `			}` |
|    15 | 1194 | `		}` |
|   630 | 1195 | `		ph7_array_add_strkey_elem(pInfo, "statics", pStatics);` |
|   313 | 1196 | `	}` |
|   630 | 1197 | `}` |
|     - | 1198 | `/*` |
|     - | 1199 | ` * array\|null __reflect_func_info(string\|Closure $target [, string $method])` |
|     - | 1200 | ` * Function/method/closure descriptor for the PHP layer.` |
|     - | 1201 | ` */` |
|   758 | 1202 | `static int vm_builtin_reflect_func_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     4 | 1203 | `{` |
|     - | 1204 | `	ph7_vm_func *pFunc;` |
|   762 | 1205 | `	ph7_class *pClass = 0;` |
|   762 | 1206 | `	ph7_class_method *pMeth = 0;` |
|   762 | 1207 | `	ph7_user_func *pHost = 0;` |
|   762 | 1208 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1209 | `	ph7_value *pInfo;` |
|   762 | 1210 | `	if( nArg < 1 ){` |
|   ! 0 | 1211 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1212 | `		return PH7_OK;` |
|     - | 1213 | `	}` |
|   762 | 1214 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], nArg > 1 ? apArg[1] : 0,` |
|     - | 1215 | `		&pClass, &pMeth, &pHost, &pClosure);` |
|   762 | 1216 | `	if( pFunc == 0 && pHost == 0 ){` |
|     6 | 1217 | `		ph7_result_null(pCtx);` |
|     6 | 1218 | `		return PH7_OK;` |
|     - | 1219 | `	}` |
|   758 | 1220 | `	pInfo = ph7_context_new_array(pCtx);` |
|   758 | 1221 | `	if( pInfo == 0 ){` |
|   ! 0 | 1222 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1223 | `		return PH7_OK;` |
|     - | 1224 | `	}` |
|   758 | 1225 | `	if( pFunc == 0 ){` |
|     - | 1226 | `		/* Host (C builtin) function: no parameter metadata beyond arity */` |
|   130 | 1227 | `		ph7_value *pParams = ph7_context_new_array(pCtx);` |
|   130 | 1228 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pHost->sName), (int)SyStringLength(&pHost->sName));` |
|   130 | 1229 | `		ReflectMapAddBool(pCtx, pInfo, "internal", 1);` |
|   130 | 1230 | `		ReflectMapAddBool(pCtx, pInfo, "closure", 0);` |
|   130 | 1231 | `		ReflectMapAddBool(pCtx, pInfo, "fstatic", 0);` |
|   130 | 1232 | `		ReflectMapAddBool(pCtx, pInfo, "byref", 0);` |
|   130 | 1233 | `		ReflectMapAddBool(pCtx, pInfo, "generator", 0);` |
|   130 | 1234 | `		ReflectMapAddBool(pCtx, pInfo, "strict", 0);` |
|   130 | 1235 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|   130 | 1236 | `		ReflectMapAddInt(pCtx, pInfo, "line", 0);` |
|   130 | 1237 | `		ReflectMapAddInt(pCtx, pInfo, "endline", 0);` |
|   130 | 1238 | `		ReflectMapAddBool(pCtx, pInfo, "doc", 0);` |
|     - | 1239 | `		{` |
|   130 | 1240 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|   130 | 1241 | `			if( pEmpty ){` |
|   130 | 1242 | `				ph7_array_add_strkey_elem(pInfo, "attrs", pEmpty);` |
|    64 | 1243 | `			}` |
|     - | 1244 | `		}` |
|   130 | 1245 | `		if( pHost->zRet ){` |
|   130 | 1246 | `			ReflectMapAddStr(pCtx, pInfo, "rettext", pHost->zRet, (int)SyStrlen(pHost->zRet));` |
|    66 | 1247 | `		}else{` |
|   ! 0 | 1248 | `			ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1249 | `		}` |
|   130 | 1250 | `		ReflectMapAddBool(pCtx, pInfo, "retnullable", 0);` |
|   130 | 1251 | `		if( pParams ){` |
|   130 | 1252 | `			ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|    64 | 1253 | `		}` |
|   130 | 1254 | `		ReflectMapAddBool(pCtx, pInfo, "variadic", 0);` |
|   130 | 1255 | `		ReflectMapAddInt(pCtx, pInfo, "minarg", (sxi64)pHost->nMinArg);` |
|   130 | 1256 | `		if( pHost->zSig ){` |
|   130 | 1257 | `			ReflectMapAddStr(pCtx, pInfo, "sig", pHost->zSig, (int)SyStrlen(pHost->zSig));` |
|    66 | 1258 | `		}else{` |
|   ! 0 | 1259 | `			ReflectMapAddStr(pCtx, pInfo, "sig", "", 0);` |
|     - | 1260 | `		}` |
|   130 | 1261 | `		ph7_result_value(pCtx, pInfo);` |
|   130 | 1262 | `		return PH7_OK;` |
|     - | 1263 | `	}` |
|   630 | 1264 | `	ReflectFillFuncCommon(pCtx, pInfo, pFunc);` |
|   630 | 1265 | `	ReflectMapAddInt(pCtx, pInfo, "minarg", -1);` |
|   630 | 1266 | `	if( (pFunc->iFlags & VM_FUNC_INTERNAL) && SySetUsed(&pFunc->aArgs) == 0 && pMeth == 0 ){` |
|     - | 1267 | `		/* Embedded-PHP builtin (max/min...): declared argless, actual` |
|     - | 1268 | `		 * signature comes from the static table */` |
|     5 | 1269 | `		const char *zRet = 0;` |
|     5 | 1270 | `		const char *zSig = PH7_VmBuiltinSigLookup(SyStringData(&pFunc->sName), SyStringLength(&pFunc->sName), &zRet);` |
|     5 | 1271 | `		if( zSig ){` |
|     5 | 1272 | `			ReflectMapAddStr(pCtx, pInfo, "sig", zSig, (int)SyStrlen(zSig));` |
|     2 | 1273 | `		}` |
|     5 | 1274 | `		if( zRet && SyStringLength(&pFunc->sReturnTypeName) == 0 ){` |
|     5 | 1275 | `			ReflectMapAddStr(pCtx, pInfo, "ret2", zRet, (int)SyStrlen(zRet));` |
|     2 | 1276 | `		}` |
|     2 | 1277 | `	}` |
|   630 | 1278 | `	if( pMeth && pClass ){` |
|   305 | 1279 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|   305 | 1280 | `		ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   305 | 1281 | `		ReflectMapAddStr(pCtx, pInfo, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   305 | 1282 | `		ReflectMapAddInt(pCtx, pInfo, "vis", (sxi64)pMeth->iProtection);` |
|   305 | 1283 | `		ReflectMapAddBool(pCtx, pInfo, "mstatic", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   305 | 1284 | `		ReflectMapAddBool(pCtx, pInfo, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|   305 | 1285 | `		ReflectMapAddBool(pCtx, pInfo, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   151 | 1286 | `	}` |
|   630 | 1287 | `	if( pClosure ){` |
|     - | 1288 | `		SyString sAttr;` |
|     - | 1289 | `		ph7_value *pAttr;` |
|     - | 1290 | `		ph7_value *pUsed;` |
|    74 | 1291 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|    74 | 1292 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    84 | 1293 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|    21 | 1294 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|    21 | 1295 | `			if( pKey ){` |
|    21 | 1296 | `				ph7_value_string(pKey, "this", 4);` |
|    21 | 1297 | `				ph7_array_add_elem(pInfo, pKey, pAttr);` |
|    10 | 1298 | `			}` |
|    11 | 1299 | `		}else{` |
|    54 | 1300 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|     - | 1301 | `		}` |
|    74 | 1302 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|    74 | 1303 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    74 | 1304 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|    31 | 1305 | `			ReflectMapAddStr(pCtx, pInfo, "scope", (const char *)SyBlobData(&pAttr->sBlob),` |
|    20 | 1306 | `				(int)SyBlobLength(&pAttr->sBlob));` |
|    11 | 1307 | `		}else{` |
|    54 | 1308 | `			ReflectMapAddNull(pCtx, pInfo, "scope");` |
|     - | 1309 | `		}` |
|     - | 1310 | `		/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */` |
|    74 | 1311 | `		pUsed = ph7_context_new_array(pCtx);` |
|    74 | 1312 | `		if( pUsed ){` |
|    74 | 1313 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     - | 1314 | `			sxu32 n;` |
|   136 | 1315 | `			for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; n++ ){` |
|    64 | 1316 | `				if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){` |
|    46 | 1317 | `					continue;` |
|     - | 1318 | `				}` |
|    18 | 1319 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|    10 | 1320 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|   ! 0 | 1321 | `					continue;` |
|     - | 1322 | `				}` |
|    19 | 1323 | `				if( (aEnv[n].iFlags & VM_FUNC_ARG_BY_REF) && aEnv[n].nIdx != SXU32_HIGH ){` |
|     - | 1324 | `					/* Captured by reference: report the slot's live value */` |
|     5 | 1325 | `					ph7_value *pLive = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aEnv[n].nIdx);` |
|     5 | 1326 | `					ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, pLive ? pLive : &aEnv[n].sValue);` |
|     5 | 1327 | `					continue;` |
|     - | 1328 | `				}` |
|    15 | 1329 | `				ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, &aEnv[n].sValue);` |
|     8 | 1330 | `			}` |
|    74 | 1331 | `			ph7_array_add_strkey_elem(pInfo, "used", pUsed);` |
|    36 | 1332 | `		}` |
|    36 | 1333 | `	}` |
|   630 | 1334 | `	ph7_result_value(pCtx, pInfo);` |
|   630 | 1335 | `	return PH7_OK;` |
|   383 | 1336 | `}` |
|     - | 1337 | `/*` |
|     - | 1338 | ` * mixed __reflect_param_default(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1339 | ` * Evaluate a parameter's compiled default expression.` |
|     - | 1340 | ` */` |
|    12 | 1341 | `static int vm_builtin_reflect_param_default(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1342 | `{` |
|     - | 1343 | `	ph7_vm_func *pFunc;` |
|     - | 1344 | `	ph7_vm_func_arg *pArg;` |
|     - | 1345 | `	ph7_value sValue;` |
|     - | 1346 | `	sxu32 nIdx;` |
|    13 | 1347 | `	if( nArg < 3 ){` |
|   ! 0 | 1348 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1349 | `		return PH7_OK;` |
|     - | 1350 | `	}` |
|    13 | 1351 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|    13 | 1352 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|    12 | 1353 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|    13 | 1354 | `	 \|\| SySetUsed(&pArg->aByteCode) < 1 ){` |
|   ! 0 | 1355 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1356 | `		return PH7_OK;` |
|     - | 1357 | `	}` |
|    13 | 1358 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|    13 | 1359 | `	VmLocalExec(pCtx->pVm, &pArg->aByteCode, &sValue, FALSE);` |
|    13 | 1360 | `	ph7_result_value(pCtx, &sValue);` |
|    13 | 1361 | `	PH7_MemObjRelease(&sValue);` |
|    13 | 1362 | `	return PH7_OK;` |
|     7 | 1363 | `}` |
|     - | 1364 | `/*` |
|     - | 1365 | ` * string\|null __reflect_param_defconst(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1366 | ` * When a parameter's default is a plain global-constant reference, its` |
|     - | 1367 | ` * source name; null otherwise. A constant default compiles to exactly` |
|     - | 1368 | ` * [ OP_LOADC (EXPAND) , OP_DONE ] with the name in the literal table.` |
|     - | 1369 | ` */` |
|     6 | 1370 | `static int vm_builtin_reflect_param_defconst(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1371 | `{` |
|     - | 1372 | `	ph7_vm_func *pFunc;` |
|     - | 1373 | `	ph7_vm_func_arg *pArg;` |
|     - | 1374 | `	VmInstr *aInstr;` |
|     - | 1375 | `	ph7_value *pLit;` |
|     - | 1376 | `	sxu32 nIdx;` |
|     7 | 1377 | `	if( nArg < 3 ){` |
|   ! 0 | 1378 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1379 | `		return PH7_OK;` |
|     - | 1380 | `	}` |
|     7 | 1381 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|     7 | 1382 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|     6 | 1383 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|     7 | 1384 | `	 \|\| SySetUsed(&pArg->aByteCode) != 2 ){` |
|     3 | 1385 | `		ph7_result_null(pCtx);` |
|     3 | 1386 | `		return PH7_OK;` |
|     - | 1387 | `	}` |
|     5 | 1388 | `	aInstr = (VmInstr *)SySetBasePtr(&pArg->aByteCode);` |
|     4 | 1389 | `	if( aInstr[0].iOp != PH7_OP_LOADC \|\| (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0` |
|     5 | 1390 | `	 \|\| aInstr[1].iOp != PH7_OP_DONE ){` |
|   ! 0 | 1391 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1392 | `		return PH7_OK;` |
|     - | 1393 | `	}` |
|     5 | 1394 | `	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);` |
|     5 | 1395 | `	if( pLit == 0 \|\| SyBlobLength(&pLit->sBlob) < 1 ){` |
|   ! 0 | 1396 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1397 | `		return PH7_OK;` |
|     - | 1398 | `	}` |
|     5 | 1399 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&pLit->sBlob), (int)SyBlobLength(&pLit->sBlob));` |
|     5 | 1400 | `	return PH7_OK;` |
|     4 | 1401 | `}` |
|     - | 1402 | `/*` |
|     - | 1403 | ` * mixed __reflect_invoke(mixed $target, ?string $method, ?object $this, array $args)` |
|     - | 1404 | ` * Visibility-bypassing invocation (methods dispatch by VM name; functions` |
|     - | 1405 | ` * and closures ride PH7_VmCallUserFunction like call_user_func_array).` |
|     - | 1406 | ` */` |
|    20 | 1407 | `static int vm_builtin_reflect_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1408 | `{` |
|    21 | 1409 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1410 | `	ph7_value sResult;` |
|     - | 1411 | `	SySet aCallArg;` |
|     - | 1412 | `	sxi32 rc;` |
|    21 | 1413 | `	if( nArg < 4 ){` |
|   ! 0 | 1414 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1415 | `		return PH7_OK;` |
|     - | 1416 | `	}` |
|    21 | 1417 | `	PH7_MemObjInit(pVm, &sResult);` |
|    21 | 1418 | `	sResult.nIdx = SXU32_HIGH;` |
|    21 | 1419 | `	SySetInit(&aCallArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    21 | 1420 | `	ReflectCollectArgs(pCtx, apArg[3], &aCallArg, 0);` |
|    21 | 1421 | `	if( (apArg[1]->iFlags & MEMOBJ_STRING) && SyBlobLength(&apArg[1]->sBlob) > 0 ){` |
|    11 | 1422 | `		ph7_class *pClass = 0;` |
|    11 | 1423 | `		ph7_class_method *pMeth = 0;` |
|    11 | 1424 | `		ph7_class_instance *pThis = 0;` |
|    11 | 1425 | `		ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, 0);` |
|    11 | 1426 | `		if( pMeth == 0 ){` |
|   ! 0 | 1427 | `			SySetRelease(&aCallArg);` |
|   ! 0 | 1428 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1429 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1430 | `			return PH7_OK;` |
|     - | 1431 | `		}` |
|    11 | 1432 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     7 | 1433 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     3 | 1434 | `		}` |
|     - | 1435 | `		/* Reflection ignores method visibility (PHP 8.1+); the flag is` |
|     - | 1436 | `		 * consumed by the first OP_CALL, i.e. this synthetic one. */` |
|    11 | 1437 | `		pVm->bReflectBypass = 1;` |
|    16 | 1438 | `		rc = PH7_VmCallClassMethod(pVm, pThis, pMeth, &sResult,` |
|    10 | 1439 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg));` |
|    11 | 1440 | `		pVm->bReflectBypass = 0;` |
|     6 | 1441 | `	}else{` |
|    16 | 1442 | `		rc = PH7_VmCallUserFunction(pVm, apArg[0],` |
|    10 | 1443 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg), &sResult);` |
|     - | 1444 | `	}` |
|    21 | 1445 | `	SySetRelease(&aCallArg);` |
|    21 | 1446 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 | 1447 | `		PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1448 | `		return rc;` |
|     - | 1449 | `	}` |
|    21 | 1450 | `	ph7_result_value(pCtx, &sResult);` |
|    21 | 1451 | `	PH7_MemObjRelease(&sResult);` |
|    21 | 1452 | `	return PH7_OK;` |
|    11 | 1453 | `}` |
|     - | 1454 | `/*` |
|     - | 1455 | ` * Closure __reflect_closure(mixed $target, ?string $method, ?object $this)` |
|     - | 1456 | ` * Mint a Closure for a function or method, bound and scoped like the` |
|     - | 1457 | ` * first-class-callable path.` |
|     - | 1458 | ` */` |
|     6 | 1459 | `static int vm_builtin_reflect_closure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1460 | `{` |
|     7 | 1461 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 1462 | `	ph7_class *pClass = 0;` |
|     7 | 1463 | `	ph7_class_method *pMeth = 0;` |
|     7 | 1464 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1465 | `	ph7_vm_func *pFunc;` |
|     7 | 1466 | `	if( nArg < 3 ){` |
|   ! 0 | 1467 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1468 | `		return PH7_OK;` |
|     - | 1469 | `	}` |
|     7 | 1470 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, &pClosure);` |
|     7 | 1471 | `	if( pClosure ){` |
|     - | 1472 | `		/* Already a Closure: hand the same instance back */` |
|   ! 0 | 1473 | `		return ReflectResultExistingObject(pCtx, pClosure);` |
|     - | 1474 | `	}` |
|     7 | 1475 | `	if( pMeth && pClass ){` |
|     5 | 1476 | `		ph7_class_instance *pThis = 0;` |
|     5 | 1477 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     3 | 1478 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     1 | 1479 | `		}` |
|     7 | 1480 | `		return ReflectResultObject(pCtx,` |
|     4 | 1481 | `			PH7_VmNewClosure(pVm, &pMeth->sFunc.sName, pThis, &pClass->sName));` |
|     - | 1482 | `	}` |
|     3 | 1483 | `	if( pFunc ){` |
|     3 | 1484 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &pFunc->sName, 0, 0));` |
|     - | 1485 | `	}` |
|     - | 1486 | `	/* Host function by name */` |
|   ! 0 | 1487 | `	if( apArg[0]->iFlags & MEMOBJ_STRING ){` |
|     - | 1488 | `		SyString sName;` |
|   ! 0 | 1489 | `		SyStringInitFromBuf(&sName, (const char *)SyBlobData(&apArg[0]->sBlob), SyBlobLength(&apArg[0]->sBlob));` |
|   ! 0 | 1490 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &sName, 0, 0));` |
|     - | 1491 | `	}` |
|   ! 0 | 1492 | `	ph7_result_null(pCtx);` |
|   ! 0 | 1493 | `	return PH7_OK;` |
|     4 | 1494 | `}` |
|     - | 1495 | `/*` |
|     - | 1496 | ` * Resolve a Generator object into its wrapper. Mirrors the static` |
|     - | 1497 | ` * VmGeneratorExtractCtx in vm.c: the $__ctx attribute carries the` |
|     - | 1498 | ` * ph7_generator pointer as a resource value.` |
|     - | 1499 | ` */` |
|    22 | 1500 | `static ph7_generator * ReflectGeneratorCtx(ph7_vm *pVm, ph7_value *pVal)` |
|     1 | 1501 | `{` |
|     - | 1502 | `	ph7_class_instance *pThis;` |
|     - | 1503 | `	ph7_value *pAttr;` |
|     - | 1504 | `	SyString sAttr;` |
|    23 | 1505 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVm->pGeneratorClass == 0 ){` |
|   ! 0 | 1506 | `		return 0;` |
|     - | 1507 | `	}` |
|    23 | 1508 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    23 | 1509 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|   ! 0 | 1510 | `		return 0;` |
|     - | 1511 | `	}` |
|    23 | 1512 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    23 | 1513 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    23 | 1514 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|   ! 0 | 1515 | `		return 0;` |
|     - | 1516 | `	}` |
|    23 | 1517 | `	return (ph7_generator *)pAttr->x.pOther;` |
|    12 | 1518 | `}` |
|     - | 1519 | `/*` |
|     - | 1520 | ` * array\|null __reflect_gen_info(Generator $g)` |
|     - | 1521 | ` * {state, closed, executing, kind ('fn'\|'method'), name, class?, this}` |
|     - | 1522 | ` */` |
|    16 | 1523 | `static int vm_builtin_reflect_gen_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1524 | `{` |
|    17 | 1525 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1526 | `	ph7_generator *pGen;` |
|     - | 1527 | `	ph7_exec_ctx *pExec;` |
|     - | 1528 | `	ph7_value *pInfo;` |
|    17 | 1529 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 \|\| pGen->pCtx == 0 ){` |
|   ! 0 | 1530 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1531 | `		return PH7_OK;` |
|     - | 1532 | `	}` |
|    17 | 1533 | `	pExec = pGen->pCtx;` |
|    17 | 1534 | `	pInfo = ph7_context_new_array(pCtx);` |
|    17 | 1535 | `	if( pInfo == 0 ){` |
|   ! 0 | 1536 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1537 | `		return PH7_OK;` |
|     - | 1538 | `	}` |
|    17 | 1539 | `	ReflectMapAddInt(pCtx, pInfo, "state", (sxi64)pExec->iState);` |
|    24 | 1540 | `	ReflectMapAddBool(pCtx, pInfo, "closed",` |
|    16 | 1541 | `		pExec->iState == PH7_CTX_STATE_COMPLETED \|\| pExec->iState == PH7_CTX_STATE_CLOSED);` |
|    17 | 1542 | `	ReflectMapAddBool(pCtx, pInfo, "executing", pVm->pActiveCtx == pExec);` |
|    17 | 1543 | `	if( pExec->pFunc ){` |
|    17 | 1544 | `		ph7_vm_func *pFunc = pExec->pFunc;` |
|    19 | 1545 | `		if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     5 | 1546 | `			ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|     5 | 1547 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "method", sizeof("method")-1);` |
|     5 | 1548 | `			ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|     3 | 1549 | `		}else{` |
|    13 | 1550 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "fn", sizeof("fn")-1);` |
|     - | 1551 | `		}` |
|    17 | 1552 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|     8 | 1553 | `	}` |
|     - | 1554 | `	{` |
|     - | 1555 | `		/* The coroutine frame installs $this as a frame VARIABLE (see` |
|     - | 1556 | `		 * VmFiberSetupFrame), not as pFrame->pThis — check both. */` |
|    17 | 1557 | `		ph7_value *pThisVal = 0;` |
|    17 | 1558 | `		if( pExec->pFrame ){` |
|    17 | 1559 | `			SyHashEntry *pVar = SyHashGet(&pExec->pFrame->hVar, "this", sizeof("this")-1);` |
|    17 | 1560 | `			if( pVar ){` |
|     5 | 1561 | `				ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj, (sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|     5 | 1562 | `				if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|     5 | 1563 | `					pThisVal = pSlot;` |
|     2 | 1564 | `				}` |
|     2 | 1565 | `			}` |
|    17 | 1566 | `			if( pThisVal == 0 && pExec->pFrame->pThis ){` |
|     - | 1567 | `				ph7_value sThis;` |
|   ! 0 | 1568 | `				ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 1569 | `				PH7_MemObjInit(pVm, &sThis);` |
|   ! 0 | 1570 | `				pExec->pFrame->pThis->iRef++;` |
|   ! 0 | 1571 | `				sThis.x.pOther = pExec->pFrame->pThis;` |
|   ! 0 | 1572 | `				MemObjSetType(&sThis, MEMOBJ_OBJ);` |
|   ! 0 | 1573 | `				if( pKey ){` |
|   ! 0 | 1574 | `					ph7_value_string(pKey, "this", 4);` |
|   ! 0 | 1575 | `					ph7_array_add_elem(pInfo, pKey, &sThis); /* copies (takes its own ref) */` |
|   ! 0 | 1576 | `				}` |
|   ! 0 | 1577 | `				PH7_MemObjRelease(&sThis);` |
|   ! 0 | 1578 | `				pThisVal = (ph7_value *)1; /* handled */` |
|   ! 0 | 1579 | `			}` |
|     8 | 1580 | `		}` |
|    17 | 1581 | `		if( pThisVal == 0 ){` |
|    13 | 1582 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|    11 | 1583 | `		}else if( pThisVal != (ph7_value *)1 ){` |
|     5 | 1584 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|     5 | 1585 | `			if( pKey ){` |
|     5 | 1586 | `				ph7_value_string(pKey, "this", 4);` |
|     5 | 1587 | `				ph7_array_add_elem(pInfo, pKey, pThisVal);` |
|     2 | 1588 | `			}` |
|     2 | 1589 | `		}` |
|     - | 1590 | `	}` |
|    17 | 1591 | `	ph7_result_value(pCtx, pInfo);` |
|    17 | 1592 | `	return PH7_OK;` |
|     9 | 1593 | `}` |
|     - | 1594 | `/*` |
|     - | 1595 | ` * Generator __reflect_gen_exec(Generator $g)` |
|     - | 1596 | `` * Follow `yield from` delegation to the innermost executing generator`` |
|     - | 1597 | ` * (PHP's ReflectionGenerator::getExecutingGenerator).` |
|     - | 1598 | ` */` |
|     4 | 1599 | `static int vm_builtin_reflect_gen_exec(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1600 | `{` |
|     5 | 1601 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1602 | `	ph7_generator *pGen;` |
|     - | 1603 | `	ph7_value *pCur;` |
|     5 | 1604 | `	int iDepth = 0;` |
|     5 | 1605 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 ){` |
|   ! 0 | 1606 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1607 | `		return PH7_OK;` |
|     - | 1608 | `	}` |
|     5 | 1609 | `	pCur = apArg[0];` |
|     9 | 1610 | `	while( pGen && pGen->pCtx && pGen->pCtx->iDelegateState == 3` |
|     8 | 1611 | `	 && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|     3 | 1612 | `		ph7_generator *pInner = ReflectGeneratorCtx(pVm, &pGen->pCtx->sDelegate);` |
|     3 | 1613 | `		if( pInner == 0 ){` |
|   ! 0 | 1614 | `			break;` |
|     - | 1615 | `		}` |
|     3 | 1616 | `		pCur = &pGen->pCtx->sDelegate;` |
|     3 | 1617 | `		pGen = pInner;` |
|     3 | 1618 | `		iDepth++;` |
|     1 | 1619 | `	}` |
|     5 | 1620 | `	return ReflectResultExistingObject(pCtx, (ph7_class_instance *)pCur->x.pOther);` |
|     3 | 1621 | `}` |
|     - | 1622 | `/*` |
|     - | 1623 | ` * array\|null __reflect_const_info(string $name)` |
|     - | 1624 | ` * Global-constant descriptor: {value}. Null when undefined. File/origin` |
|     - | 1625 | ` * metadata arrives with the C5 constant-metadata work.` |
|     - | 1626 | ` */` |
|    40 | 1627 | `static int vm_builtin_reflect_const_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1628 | `{` |
|    41 | 1629 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1630 | `	SyHashEntry *pEntry;` |
|     - | 1631 | `	ph7_constant *pCons;` |
|     - | 1632 | `	ph7_value *pInfo;` |
|     - | 1633 | `	ph7_value sValue;` |
|     - | 1634 | `	const char *zName;` |
|     - | 1635 | `	int nLen;` |
|    41 | 1636 | `	if( nArg < 1 ){` |
|   ! 0 | 1637 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1638 | `		return PH7_OK;` |
|     - | 1639 | `	}` |
|    41 | 1640 | `	zName = ph7_value_to_string(apArg[0], &nLen);` |
|    41 | 1641 | `	pEntry = nLen > 0 ? SyHashGet(&pVm->hConstant, (const void *)zName, (sxu32)nLen) : 0;` |
|    41 | 1642 | `	if( pEntry == 0 ){` |
|     3 | 1643 | `		ph7_result_null(pCtx);` |
|     3 | 1644 | `		return PH7_OK;` |
|     - | 1645 | `	}` |
|    39 | 1646 | `	pCons = (ph7_constant *)pEntry->pUserData;` |
|    39 | 1647 | `	pInfo = ph7_context_new_array(pCtx);` |
|    39 | 1648 | `	if( pInfo == 0 ){` |
|   ! 0 | 1649 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1650 | `		return PH7_OK;` |
|     - | 1651 | `	}` |
|    39 | 1652 | `	PH7_MemObjInit(pVm, &sValue);` |
|    39 | 1653 | `	if( pCons->xExpand ){` |
|    39 | 1654 | `		pCons->xExpand(&sValue, pCons->pUserData);` |
|    19 | 1655 | `	}` |
|     - | 1656 | `	{` |
|    39 | 1657 | `		ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|    39 | 1658 | `		if( pKey ){` |
|    39 | 1659 | `			ph7_value_string(pKey, "value", 5);` |
|    39 | 1660 | `			ph7_array_add_elem(pInfo, pKey, &sValue);` |
|    19 | 1661 | `		}` |
|     - | 1662 | `	}` |
|    39 | 1663 | `	PH7_MemObjRelease(&sValue);` |
|    39 | 1664 | `	ReflectMapAddBool(pCtx, pInfo, "internal", pCons->bUserDefined == 0);` |
|    39 | 1665 | `	if( SyStringLength(&pCons->sFile) > 0 ){` |
|    25 | 1666 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));` |
|    13 | 1667 | `	}else{` |
|    15 | 1668 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1669 | `	}` |
|    39 | 1670 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pCons->nLine);` |
|    39 | 1671 | `	ReflectMapAddAttrs(pCtx, pInfo, &pCons->aAttrs);` |
|    39 | 1672 | `	ph7_result_value(pCtx, pInfo);` |
|    39 | 1673 | `	return PH7_OK;` |
|    21 | 1674 | `}` |
|     - | 1675 | `/*` |
|     - | 1676 | ` * int\|null __reflect_ref_id(array $arr, int\|string $key)` |
|     - | 1677 | ` * The element's slot index when the element is a reference (its slot has` |
|     - | 1678 | ` * a reference-table record with at least two links), null otherwise.` |
|     - | 1679 | ` */` |
|     6 | 1680 | `static int vm_builtin_reflect_ref_id(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1681 | `{` |
|     - | 1682 | `	ph7_hashmap *pMap;` |
|     7 | 1683 | `	ph7_hashmap_node *pNode = 0;` |
|     7 | 1684 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|   ! 0 | 1685 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1686 | `		return PH7_OK;` |
|     - | 1687 | `	}` |
|     7 | 1688 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 | 1689 | `	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK \|\| pNode == 0 ){` |
|   ! 0 | 1690 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1691 | `		return PH7_OK;` |
|     - | 1692 | `	}` |
|     7 | 1693 | `	if( PH7_VmSlotRefCount(pCtx->pVm, pNode->nValIdx) < 2 ){` |
|     3 | 1694 | `		ph7_result_null(pCtx);` |
|     3 | 1695 | `		return PH7_OK;` |
|     - | 1696 | `	}` |
|     5 | 1697 | `	ph7_result_int64(pCtx, (sxi64)pNode->nValIdx);` |
|     5 | 1698 | `	return PH7_OK;` |
|     4 | 1699 | `}` |
|     - | 1700 | `/*` |
|     - | 1701 | ` * array\|null __reflect_attr_args(string $kind, mixed $target, ?string $member,` |
|     - | 1702 | ` *                                int $paramIdx, int $attrIdx)` |
|     - | 1703 | ` * Evaluate the recorded argument expressions of one declared attribute:` |
|     - | 1704 | ` * kind 'class' (target = class), 'attr' (class + property/constant name),` |
|     - | 1705 | ` * 'method' (class + method), 'fn' (function name or Closure), 'param'` |
|     - | 1706 | ` * (function spec + parameter index). Named arguments become string keys.` |
|     - | 1707 | ` */` |
|    68 | 1708 | `static int vm_builtin_reflect_attr_args(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1709 | `{` |
|    69 | 1710 | `	ph7_vm *pVm = pCtx->pVm;` |
|    69 | 1711 | `	SySet *pAttrs = 0;` |
|    69 | 1712 | `	ph7_class *pDeclCls = 0; /* class an attribute is declared on: scope for self:: in its args */` |
|     - | 1713 | `	ph7_attribute *pAttrRec;` |
|     - | 1714 | `	ph7_value *pOut;` |
|     - | 1715 | `	const char *zKind;` |
|     - | 1716 | `	int nKind;` |
|     - | 1717 | `	sxu32 nAttrIdx, n;` |
|    69 | 1718 | `	if( nArg < 5 ){` |
|   ! 0 | 1719 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1720 | `		return PH7_OK;` |
|     - | 1721 | `	}` |
|    69 | 1722 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|    69 | 1723 | `	nAttrIdx = (sxu32)ph7_value_to_int(apArg[4]);` |
|    91 | 1724 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|    45 | 1725 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|    45 | 1726 | `		if( pClass ){ pAttrs = &pClass->aAttrs; pDeclCls = pClass; }` |
|    49 | 1727 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|     5 | 1728 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|     5 | 1729 | `		ph7_class_attr *pMember = pClass ? ReflectFetchMember(pClass, apArg[2]) : 0;` |
|     5 | 1730 | `		if( pMember ){ pAttrs = &pMember->aAttrs; pDeclCls = pClass; }` |
|    27 | 1731 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|     9 | 1732 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     9 | 1733 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|    21 | 1734 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|     9 | 1735 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], 0, 0, 0, 0, 0);` |
|     9 | 1736 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|    10 | 1737 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|     3 | 1738 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1739 | `		ph7_vm_func_arg *pParam = pFunc` |
|     2 | 1740 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|     3 | 1741 | `		if( pParam ){ pAttrs = &pParam->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|     4 | 1742 | `	}else if( nKind == 5 && SyMemcmp(zKind, "const", 5) == 0 ){` |
|     - | 1743 | ``		/* Global constant (php 8.5 attributes on `const` statements) */`` |
|     - | 1744 | `		const char *zCName;` |
|     - | 1745 | `		int nCName;` |
|     - | 1746 | `		SyHashEntry *pCEntry;` |
|     3 | 1747 | `		zCName = ph7_value_to_string(apArg[1], &nCName);` |
|     3 | 1748 | `		pCEntry = nCName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zCName, (sxu32)nCName) : 0;` |
|     3 | 1749 | `		if( pCEntry ){ pAttrs = &((ph7_constant *)pCEntry->pUserData)->aAttrs; }` |
|     1 | 1750 | `	}` |
|    68 | 1751 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|    69 | 1752 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 | 1753 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1754 | `		return PH7_OK;` |
|     - | 1755 | `	}` |
|   123 | 1756 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|    55 | 1757 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|     - | 1758 | `		ph7_value sValue;` |
|    55 | 1759 | `		PH7_MemObjInit(pVm, &sValue);` |
|    55 | 1760 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|     - | 1761 | `` 			/* Evaluate under the attribute's declaring-class scope so `self::class` `` |
|     - | 1762 | ``			 * / `self::CONST` / `parent::` resolve like php (else self:: would bind`` |
|     - | 1763 | `			 * to the reflection machinery's own class). */` |
|    55 | 1764 | `			PH7_VmExecAttrArg(pVm, &pArgRec->aByteCode, pDeclCls, &sValue);` |
|    27 | 1765 | `		}` |
|    55 | 1766 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|     5 | 1767 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|     3 | 1768 | `		}else{` |
|    51 | 1769 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|     - | 1770 | `		}` |
|    55 | 1771 | `		PH7_MemObjRelease(&sValue);` |
|    28 | 1772 | `	}` |
|    69 | 1773 | `	ph7_result_value(pCtx, pOut);` |
|    69 | 1774 | `	return PH7_OK;` |
|    35 | 1775 | `}` |
|  4140 | 1776 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|     5 | 1777 | `{` |
|     - | 1778 | `	static const struct {` |
|     - | 1779 | `		const char *zName;` |
|     - | 1780 | `		ProchHostFunction xFunc;` |
|     - | 1781 | `	} aFunc[] = {` |
|     - | 1782 | `		{ "__reflect_class_info",     vm_builtin_reflect_class_info },` |
|     - | 1783 | `		{ "__reflect_const_value",    vm_builtin_reflect_const_value },` |
|     - | 1784 | `		{ "__reflect_static_value",   vm_builtin_reflect_static_value },` |
|     - | 1785 | `		{ "__reflect_static_set",     vm_builtin_reflect_static_set },` |
|     - | 1786 | `		{ "__reflect_prop_default",   vm_builtin_reflect_prop_default },` |
|     - | 1787 | `		{ "__reflect_new_instance",   vm_builtin_reflect_new_instance },` |
|     - | 1788 | `		{ "__reflect_new_no_ctor",    vm_builtin_reflect_new_no_ctor },` |
|     - | 1789 | `		{ "__reflect_func_info",      vm_builtin_reflect_func_info },` |
|     - | 1790 | `		{ "__reflect_param_default",  vm_builtin_reflect_param_default },` |
|     - | 1791 | `		{ "__reflect_param_defconst", vm_builtin_reflect_param_defconst },` |
|     - | 1792 | `		{ "__reflect_invoke",         vm_builtin_reflect_invoke },` |
|     - | 1793 | `		{ "__reflect_closure",        vm_builtin_reflect_closure },` |
|     - | 1794 | `		{ "__reflect_prop_read",      vm_builtin_reflect_prop_read },` |
|     - | 1795 | `		{ "__reflect_prop_write",     vm_builtin_reflect_prop_write },` |
|     - | 1796 | `		{ "__reflect_prop_state",     vm_builtin_reflect_prop_state },` |
|     - | 1797 | `		{ "__reflect_dyn_props",      vm_builtin_reflect_dyn_props },` |
|     - | 1798 | `		{ "__reflect_gen_info",       vm_builtin_reflect_gen_info },` |
|     - | 1799 | `		{ "__reflect_gen_exec",       vm_builtin_reflect_gen_exec },` |
|     - | 1800 | `		{ "__reflect_const_info",     vm_builtin_reflect_const_info },` |
|     - | 1801 | `		{ "__reflect_ref_id",         vm_builtin_reflect_ref_id },` |
|     - | 1802 | `		{ "__reflect_attr_args",      vm_builtin_reflect_attr_args },` |
|     - | 1803 | `	};` |
|     - | 1804 | `	sxu32 n;` |
| 91085 | 1805 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 86945 | 1806 | `		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);` |
| 43475 | 1807 | `	}` |
|  4145 | 1808 | `	return PH7_VmInstallReflectionLib(&(*pVm));` |
|     5 | 1809 | `}` |
|     - | 1810 |  |
