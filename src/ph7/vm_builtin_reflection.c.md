# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1086/1261 lines (86.12%)

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
|    826 |   31 | `static ph7_class * ReflectResolveClass(ph7_vm *pVm, ph7_value *pArg)` |
|      5 |   32 | `{` |
|      - |   33 | `	ph7_class *pClass;` |
|    831 |   34 | `	pClass = PH7_VmExtractClassFromValue(pVm, pArg);` |
|    831 |   35 | `	if( pClass == 0 && ph7_value_is_string(pArg) ){` |
|      - |   36 | `		const char *zName;` |
|      - |   37 | `		int nLen;` |
|     16 |   38 | `		zName = ph7_value_to_string(pArg, &nLen);` |
|     16 |   39 | `		if( nLen > 0 ){` |
|     16 |   40 | `			pClass = PH7_VmTriggerAutoload(pVm, zName, (sxu32)nLen, FALSE);` |
|      7 |   41 | `		}` |
|      7 |   42 | `	}` |
|    831 |   43 | `	return pClass;` |
|      5 |   44 | `}` |
|      - |   45 | `/*` |
|      - |   46 | ` * Hand a freshly created class instance to the caller. The return slot` |
|      - |   47 | ` * takes over the initial reference from PH7_NewClassInstance (iRef=1):` |
|      - |   48 | ` * no extra iRef++ here (see the synthesized-object invariant — a stray` |
|      - |   49 | ` * bump leaks the object and disables its __destruct).` |
|      - |   50 | ` */` |
|    106 |   51 | `static int ReflectResultObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|      2 |   52 | `{` |
|    108 |   53 | `	if( pObj == 0 ){` |
|    ! 0 |   54 | `		ph7_result_null(pCtx);` |
|    ! 0 |   55 | `		return PH7_OK;` |
|      - |   56 | `	}` |
|    108 |   57 | `	PH7_MemObjRelease(pCtx->pRet);` |
|    108 |   58 | `	pCtx->pRet->x.pOther = pObj;` |
|    108 |   59 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|    108 |   60 | `	return PH7_OK;` |
|     55 |   61 | `}` |
|      - |   62 | `/* --- Marshaling helpers: build the descriptor arrays handed to the PHP layer --- */` |
|  26804 |   63 | `static void ReflectMapAddBool(ph7_context *pCtx, ph7_value *pMap, const char *zKey, int b)` |
|      5 |   64 | `{` |
|  26809 |   65 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  26809 |   66 | `	if( p == 0 ){ return; }` |
|  26809 |   67 | `	ph7_value_bool(p, b);` |
|  26809 |   68 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  13407 |   69 | `}` |
|   7972 |   70 | `static void ReflectMapAddInt(ph7_context *pCtx, ph7_value *pMap, const char *zKey, sxi64 iVal)` |
|      5 |   71 | `{` |
|   7977 |   72 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|   7977 |   73 | `	if( p == 0 ){ return; }` |
|   7977 |   74 | `	ph7_value_int64(p, iVal);` |
|   7977 |   75 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|   3991 |   76 | `}` |
|   7300 |   77 | `static void ReflectMapAddStr(ph7_context *pCtx, ph7_value *pMap, const char *zKey,` |
|      - |   78 | `	const char *zVal, int nVal)` |
|      5 |   79 | `{` |
|   7305 |   80 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|   7305 |   81 | `	if( p == 0 ){ return; }` |
|   7305 |   82 | `	ph7_value_string(p, zVal, nVal);` |
|   7305 |   83 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|   3655 |   84 | `}` |
|   2502 |   85 | `static void ReflectMapAddNull(ph7_context *pCtx, ph7_value *pMap, const char *zKey)` |
|      5 |   86 | `{` |
|   2507 |   87 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|   2507 |   88 | `	if( p == 0 ){ return; }` |
|   2507 |   89 | `	ph7_value_null(p);` |
|   2507 |   90 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|   1256 |   91 | `}` |
|      - |   92 | `/* Add an entry under a dynamic (SyString) key. */` |
|    678 |   93 | `static void ReflectMapAddDyn(ph7_context *pCtx, ph7_value *pMap,` |
|      - |   94 | `	const SyString *pKey, ph7_value *pVal)` |
|      5 |   95 | `{` |
|    683 |   96 | `	ph7_value *pK = ph7_context_new_scalar(pCtx);` |
|    683 |   97 | `	if( pK == 0 ){ return; }` |
|    683 |   98 | `	ph7_value_string(pK, pKey->zString, (int)pKey->nByte);` |
|    683 |   99 | `	ph7_array_add_elem(pMap, pK, pVal);` |
|    344 |  100 | `}` |
|      - |  101 | `/* Emit the declared #[...] attributes of a target as a summary list:` |
|      - |  102 | ` * [ {name, line} ... ]. Argument values stay lazy — the PHP layer pulls` |
|      - |  103 | ` * them through __reflect_attr_args when ReflectionAttribute needs them. */` |
|   3478 |  104 | `static void ReflectMapAddAttrs(ph7_context *pCtx, ph7_value *pMap, SySet *pAttrs)` |
|      5 |  105 | `{` |
|   3483 |  106 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|   3483 |  107 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|      - |  108 | `	sxu32 n;` |
|   3483 |  109 | `	if( pList == 0 ){` |
|    ! 0 |  110 | `		return;` |
|      - |  111 | `	}` |
|   3595 |  112 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|    114 |  113 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|    114 |  114 | `		if( pMeta == 0 ){ break; }` |
|    114 |  115 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aA[n].sName), (int)SyStringLength(&aA[n].sName));` |
|    114 |  116 | `		ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)aA[n].nLine);` |
|    114 |  117 | `		ph7_array_add_elem(pList, 0, pMeta);` |
|     58 |  118 | `	}` |
|   3483 |  119 | `	ph7_array_add_strkey_elem(pMap, "attrs", pList);` |
|   1744 |  120 | `}` |
|      - |  121 | `/* Emit a doc-comment field: the text when present, else boolean false` |
|      - |  122 | ` * (getDocComment()'s exact return contract). */` |
|   1522 |  123 | `static void ReflectMapAddDoc(ph7_context *pCtx, ph7_value *pMap, const SyString *pDoc)` |
|      5 |  124 | `{` |
|   1527 |  125 | `	if( SyStringLength(pDoc) > 0 ){` |
|     21 |  126 | `		ReflectMapAddStr(pCtx, pMap, "doc", SyStringData(pDoc), (int)SyStringLength(pDoc));` |
|     11 |  127 | `	}else{` |
|   1507 |  128 | `		ReflectMapAddBool(pCtx, pMap, "doc", 0);` |
|      - |  129 | `	}` |
|   1527 |  130 | `}` |
|      - |  131 | `/*` |
|      - |  132 | ` * Append pIface (and its parents / extended interfaces) to the dedup set` |
|      - |  133 | ` * of ph7_class pointers.` |
|      - |  134 | ` */` |
|     64 |  135 | `static void ReflectAddInterface(ph7_class *pIface, SySet *pOut, int iDepth)` |
|      2 |  136 | `{` |
|      - |  137 | `	ph7_class **apKnown;` |
|      - |  138 | `	sxu32 n;` |
|     66 |  139 | `	if( pIface == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|    ! 0 |  140 | `		return;` |
|      - |  141 | `	}` |
|      - |  142 | `	/* Parents of an interface come along too (interface B extends A) */` |
|     66 |  143 | `	if( pIface->pBase ){` |
|     13 |  144 | `		ReflectAddInterface(pIface->pBase, pOut, iDepth + 1);` |
|      6 |  145 | `	}` |
|      - |  146 | `	/* Some engines record extended interfaces in aInterface as well */` |
|     66 |  147 | `	apKnown = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|     66 |  148 | `	for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|    ! 0 |  149 | `		ReflectAddInterface(apKnown[n], pOut, iDepth + 1);` |
|    ! 0 |  150 | `	}` |
|      - |  151 | `	/* Dedup by pointer */` |
|     66 |  152 | `	apKnown = (ph7_class **)SySetBasePtr(pOut);` |
|    100 |  153 | `	for( n = 0 ; n < SySetUsed(pOut) ; n++ ){` |
|     43 |  154 | `		if( apKnown[n] == pIface ){` |
|      9 |  155 | `			return;` |
|      - |  156 | `		}` |
|     18 |  157 | `	}` |
|     58 |  158 | `	SySetPut(pOut, (const void *)&pIface);` |
|     34 |  159 | `}` |
|      - |  160 | `/*` |
|      - |  161 | ` * Collect the transitive set of interfaces implemented by pClass:` |
|      - |  162 | ` * the parent chain's interfaces first, then the class's own.` |
|      - |  163 | ` */` |
|    212 |  164 | `static void ReflectCollectInterfaces(ph7_class *pClass, SySet *pOut, int iDepth)` |
|      5 |  165 | `{` |
|      - |  166 | `	ph7_class **apIface;` |
|      - |  167 | `	sxu32 n;` |
|    217 |  168 | `	if( pClass == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|    ! 0 |  169 | `		return;` |
|      - |  170 | `	}` |
|    217 |  171 | `	if( pClass->pBase ){` |
|     32 |  172 | `		ReflectCollectInterfaces(pClass->pBase, pOut, iDepth + 1);` |
|     15 |  173 | `	}` |
|    217 |  174 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|    267 |  175 | `	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){` |
|     52 |  176 | `		ReflectAddInterface(apIface[n], pOut, iDepth + 1);` |
|     27 |  177 | `	}` |
|    111 |  178 | `}` |
|      - |  179 | `/*` |
|      - |  180 | ` * Deepest base class whose method table maps the same name to the very` |
|      - |  181 | ` * same ph7_class_method pointer: inheritance shares member pointers` |
|      - |  182 | ` * (PH7_ClassInherit), so this identifies the declaring class. Methods` |
|      - |  183 | ` * copied in from traits are not on the pBase chain and thus report the` |
|      - |  184 | ` * using class, which is what PHP reports too.` |
|      - |  185 | ` */` |
|   1108 |  186 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth)` |
|      5 |  187 | `{` |
|   1113 |  188 | `	ph7_class *pDecl = pClass;` |
|   1113 |  189 | `	ph7_class *pBase = pClass->pBase;` |
|   1113 |  190 | `	int iDepth = 0;` |
|   1435 |  191 | `	while( pBase && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|      - |  192 | `		SyHashEntry *pEntry;` |
|    880 |  193 | `		pEntry = SyHashGet(&pBase->hMethod, (const void *)SyStringData(&pMeth->sFunc.sName),` |
|    293 |  194 | `			SyStringLength(&pMeth->sFunc.sName));` |
|    587 |  195 | `		if( pEntry == 0 \|\| (ph7_class_method *)pEntry->pUserData != pMeth ){` |
|    133 |  196 | `			break;` |
|      - |  197 | `		}` |
|    323 |  198 | `		pDecl = pBase;` |
|    323 |  199 | `		pBase = pBase->pBase;` |
|    323 |  200 | `		iDepth++;` |
|      1 |  201 | `	}` |
|   1113 |  202 | `	return pDecl;` |
|      5 |  203 | `}` |
|      - |  204 | `/* Fetch a class attribute (property or constant) by plain name. */` |
|     54 |  205 | `static ph7_class_attr * ReflectFetchAttr(ph7_class *pClass, ph7_value *pName)` |
|      2 |  206 | `{` |
|      - |  207 | `	SyHashEntry *pEntry;` |
|      - |  208 | `	const char *zName;` |
|      - |  209 | `	int nLen;` |
|     56 |  210 | `	zName = ph7_value_to_string(pName, &nLen);` |
|     56 |  211 | `	if( nLen < 1 ){` |
|    ! 0 |  212 | `		return 0;` |
|      - |  213 | `	}` |
|     56 |  214 | `	pEntry = SyHashGet(&pClass->hAttr, (const void *)zName, (sxu32)nLen);` |
|     56 |  215 | `	if( pEntry == 0 ){` |
|      3 |  216 | `		return 0;` |
|      - |  217 | `	}` |
|     54 |  218 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|     29 |  219 | `}` |
|      - |  220 | `/* Fetch a class CONSTANT (or enum case) by name from the hConst namespace. */` |
|     60 |  221 | `static ph7_class_attr * ReflectFetchConst(ph7_class *pClass, ph7_value *pName)` |
|      3 |  222 | `{` |
|      - |  223 | `	const char *zName;` |
|      - |  224 | `	int nLen;` |
|     63 |  225 | `	zName = ph7_value_to_string(pName, &nLen);` |
|     63 |  226 | `	if( nLen < 1 ){` |
|    ! 0 |  227 | `		return 0;` |
|      - |  228 | `	}` |
|     63 |  229 | `	return PH7_ClassExtractConstant(pClass, zName, (sxu32)nLen);` |
|     33 |  230 | `}` |
|      - |  231 | `/* Fetch a member by name from EITHER namespace (property then constant) — used` |
|      - |  232 | ` * where the caller reflects over a member that may be either (e.g. attributes` |
|      - |  233 | ` * attached to a property or a constant). */` |
|      4 |  234 | `static ph7_class_attr * ReflectFetchMember(ph7_class *pClass, ph7_value *pName)` |
|      1 |  235 | `{` |
|      5 |  236 | `	ph7_class_attr *pAttr = ReflectFetchAttr(pClass, pName);` |
|      5 |  237 | `	if( pAttr == 0 ){` |
|      3 |  238 | `		pAttr = ReflectFetchConst(pClass, pName);` |
|      1 |  239 | `	}` |
|      5 |  240 | `	return pAttr;` |
|      1 |  241 | `}` |
|      - |  242 | `/*` |
|      - |  243 | ` * array\|null __phl_rcinfo(object\|string $target)` |
|      - |  244 | ` *` |
|      - |  245 | ` * Full class descriptor, or null when the class cannot be resolved (after` |
|      - |  246 | ` * an autoload attempt). Shape:` |
|      - |  247 | ` *   name, internal, interface, trait, abstract, final, readonly, iterable (bool),` |
|      - |  248 | ` *   parent (string\|null), interfaces (list), traits (list),` |
|      - |  249 | ` *   file (string\|false), line, endline (int),` |
|      - |  250 | ` *   ctorvis, clonevis (0 = absent, else PH7_CLASS_PROT_*),` |
|      - |  251 | ` *   consts  {name: {vis, final, decl, line}},` |
|      - |  252 | ` *   props   {name: {vis, static, readonly, hasdef, decl, line}},` |
|      - |  253 | ` *   methods {name: {vis, static, abstract, final, decl, line}}` |
|      - |  254 | ` */` |
|    196 |  255 | `static int vm_builtin_reflect_class_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 |  256 | `{` |
|    201 |  257 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  258 | `	ph7_class *pClass;` |
|      - |  259 | `	ph7_value *pInfo, *pConsts, *pProps, *pMethods, *pList;` |
|      - |  260 | `	SyHashEntry *pEntry;` |
|      - |  261 | `	SySet aIfaceSet;` |
|    201 |  262 | `	sxi32 iCtorVis = 0, iCloneVis = 0;` |
|    201 |  263 | `	int bIterable = 0;` |
|      - |  264 | `	sxu32 n;` |
|    201 |  265 | `	if( nArg < 1 ){` |
|    ! 0 |  266 | `		ph7_result_null(pCtx);` |
|    ! 0 |  267 | `		return PH7_OK;` |
|      - |  268 | `	}` |
|    201 |  269 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|    201 |  270 | `	if( pClass == 0 ){` |
|     16 |  271 | `		ph7_result_null(pCtx);` |
|     16 |  272 | `		return PH7_OK;` |
|      - |  273 | `	}` |
|    187 |  274 | `	pInfo = ph7_context_new_array(pCtx);` |
|    187 |  275 | `	pConsts = ph7_context_new_array(pCtx);` |
|    187 |  276 | `	pProps = ph7_context_new_array(pCtx);` |
|    187 |  277 | `	pMethods = ph7_context_new_array(pCtx);` |
|    187 |  278 | `	if( pInfo == 0 \|\| pConsts == 0 \|\| pProps == 0 \|\| pMethods == 0 ){` |
|    ! 0 |  279 | `		ph7_result_null(pCtx);` |
|    ! 0 |  280 | `		return PH7_OK;` |
|      - |  281 | `	}` |
|    187 |  282 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|    187 |  283 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pClass->iFlags & PH7_CLASS_INTERNAL) != 0);` |
|    187 |  284 | `	ReflectMapAddBool(pCtx, pInfo, "interface", (pClass->iFlags & PH7_CLASS_INTERFACE) != 0);` |
|    187 |  285 | `	ReflectMapAddBool(pCtx, pInfo, "trait", (pClass->iFlags & PH7_CLASS_TRAIT) != 0);` |
|    187 |  286 | `	ReflectMapAddBool(pCtx, pInfo, "abstract", (pClass->iFlags & PH7_CLASS_ABSTRACT) != 0);` |
|    187 |  287 | `	ReflectMapAddBool(pCtx, pInfo, "final", (pClass->iFlags & PH7_CLASS_FINAL) != 0);` |
|    187 |  288 | `	ReflectMapAddBool(pCtx, pInfo, "readonly", (pClass->iFlags & PH7_CLASS_READONLY) != 0);` |
|    187 |  289 | `	ReflectMapAddBool(pCtx, pInfo, "enum", (pClass->iFlags & PH7_CLASS_ENUM) != 0);` |
|    187 |  290 | `	if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|    ! 0 |  291 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "int", (int)sizeof("int")-1);` |
|    187 |  292 | `	}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|      3 |  293 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "string", (int)sizeof("string")-1);` |
|      2 |  294 | `	}else{` |
|    185 |  295 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "", 0);` |
|      - |  296 | `	}` |
|      - |  297 | `	{` |
|      - |  298 | `		/* Enum case names in declaration order (empty list for non-enums) */` |
|    187 |  299 | `		ph7_value *pCases = ph7_context_new_array(pCtx);` |
|    187 |  300 | `		if( pCases ){` |
|    187 |  301 | `			ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|      - |  302 | `			sxu32 nCase;` |
|    193 |  303 | `			for( nCase = 0 ; nCase < SySetUsed(&pClass->aEnumCases) ; nCase++ ){` |
|      7 |  304 | `				ph7_value *pNm = ph7_context_new_scalar(pCtx);` |
|      7 |  305 | `				if( pNm ){` |
|      7 |  306 | `					ph7_value_string(pNm,apCase[nCase]->sName.zString,(int)apCase[nCase]->sName.nByte);` |
|      7 |  307 | `					ph7_array_add_elem(pCases,0,pNm);` |
|      3 |  308 | `				}` |
|      4 |  309 | `			}` |
|    187 |  310 | `			ph7_array_add_strkey_elem(pInfo,"cases",pCases);` |
|     91 |  311 | `		}` |
|      - |  312 | `	}` |
|    187 |  313 | `	if( pClass->pBase ){` |
|     41 |  314 | `		ReflectMapAddStr(pCtx, pInfo, "parent", SyStringData(&pClass->pBase->sName),` |
|     26 |  315 | `			(int)SyStringLength(&pClass->pBase->sName));` |
|     15 |  316 | `	}else{` |
|    161 |  317 | `		ReflectMapAddNull(pCtx, pInfo, "parent");` |
|      - |  318 | `	}` |
|      - |  319 | `	/* Transitive interfaces */` |
|    187 |  320 | `	SySetInit(&aIfaceSet, &pVm->sAllocator, sizeof(ph7_class *));` |
|    187 |  321 | `	ReflectCollectInterfaces(pClass, &aIfaceSet, 0);` |
|    187 |  322 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|      - |  323 | `		/* An interface's own parents count as its interface list */` |
|     11 |  324 | `		if( pClass->pBase ){` |
|      3 |  325 | `			ReflectAddInterface(pClass->pBase, &aIfaceSet, 0);` |
|      1 |  326 | `		}` |
|      5 |  327 | `	}` |
|    187 |  328 | `	pList = ph7_context_new_array(pCtx);` |
|    187 |  329 | `	if( pList ){` |
|    187 |  330 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIfaceSet);` |
|    243 |  331 | `		for( n = 0 ; n < SySetUsed(&aIfaceSet) ; n++ ){` |
|     58 |  332 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|     58 |  333 | `			if( pName == 0 ){ break; }` |
|     58 |  334 | `			ph7_value_string(pName, SyStringData(&apIface[n]->sName), (int)SyStringLength(&apIface[n]->sName));` |
|     58 |  335 | `			ph7_array_add_elem(pList, 0, pName);` |
|     58 |  336 | `			if( pVm->pTraversableClass && apIface[n] == pVm->pTraversableClass ){` |
|      7 |  337 | `				bIterable = 1;` |
|      3 |  338 | `			}` |
|     30 |  339 | `		}` |
|    187 |  340 | `		ph7_array_add_strkey_elem(pInfo, "interfaces", pList);` |
|     91 |  341 | `	}` |
|    187 |  342 | `	SySetRelease(&aIfaceSet);` |
|    187 |  343 | `	ReflectMapAddBool(pCtx, pInfo, "iterable", bIterable);` |
|      - |  344 | `	/* Used traits */` |
|    187 |  345 | `	pList = ph7_context_new_array(pCtx);` |
|    187 |  346 | `	if( pList ){` |
|    187 |  347 | `		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|    195 |  348 | `		for( n = 0 ; n < SySetUsed(&pClass->aTrait) ; n++ ){` |
|     11 |  349 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|     11 |  350 | `			if( pName == 0 ){ break; }` |
|     11 |  351 | `			ph7_value_string(pName, SyStringData(&apTrait[n]->sName), (int)SyStringLength(&apTrait[n]->sName));` |
|     11 |  352 | `			ph7_array_add_elem(pList, 0, pName);` |
|      7 |  353 | `		}` |
|    187 |  354 | `		ph7_array_add_strkey_elem(pInfo, "traits", pList);` |
|     91 |  355 | `	}` |
|      - |  356 | `	/* File / lines: no file recorded => false, like PHP internals */` |
|    187 |  357 | `	if( SyStringLength(&pClass->sFile) > 0 ){` |
|    167 |  358 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));` |
|     86 |  359 | `	}else{` |
|     21 |  360 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|      - |  361 | `	}` |
|    187 |  362 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pClass->nLine);` |
|    187 |  363 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pClass->nEndLine);` |
|    187 |  364 | `	ReflectMapAddDoc(pCtx, pInfo, &pClass->sDoc);` |
|    187 |  365 | `	ReflectMapAddAttrs(pCtx, pInfo, &pClass->aAttrs);` |
|      - |  366 | `	/* Members are emitted in PHP's reporting order: the class's own members` |
|      - |  367 | `	 * first (declaration order), then each inheritance level's, outward.` |
|      - |  368 | `	 * Per level we iterate the DECLARING class's own hash — subclass hashes` |
|      - |  369 | `	 * interleave inherited pointers unpredictably — and emit buffered` |
|      - |  370 | `	 * entries in reverse, because SyHash lists are LIFO. A pointer-identity` |
|      - |  371 | `	 * lookup in the reflected class's hash filters out members that are not` |
|      - |  372 | `	 * visible there (base privates, overridden entries). */` |
|      - |  373 | `	{` |
|      - |  374 | `		ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];` |
|    187 |  375 | `		ph7_class *pWalk = pClass;` |
|      - |  376 | `		SySet aTmp;` |
|    187 |  377 | `		sxu32 nChain = 0, iLevel, nT;` |
|    399 |  378 | `		while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){` |
|    217 |  379 | `			aChain[nChain++] = pWalk;` |
|    217 |  380 | `			pWalk = pWalk->pBase;` |
|      5 |  381 | `		}` |
|    187 |  382 | `		SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));` |
|    399 |  383 | `		for( iLevel = 0 ; iLevel < nChain ; iLevel++ ){` |
|    217 |  384 | `			ph7_class *pLevel = aChain[iLevel];` |
|      - |  385 | `			/* --- Properties (hAttr) then constants/enum cases (hConst) — php's two` |
|      - |  386 | `			 * separate member namespaces. Each table is collected and emitted` |
|      - |  387 | `			 * independently; the CONSTANT flag still routes each to pConsts/pProps. --- */` |
|      - |  388 | `			{` |
|      - |  389 | `			int iTab;` |
|    641 |  390 | `			for( iTab = 0 ; iTab < 2 ; iTab++ ){` |
|    429 |  391 | `			SyHash *pSrcHash = iTab ? &pLevel->hConst : &pLevel->hAttr;` |
|    429 |  392 | `			SyHash *pRefHash = iTab ? &pClass->hConst : &pClass->hAttr;` |
|    429 |  393 | `			SySetReset(&aTmp);` |
|    429 |  394 | `			SyHashResetLoopCursor(pSrcHash);` |
|    751 |  395 | `			while( (pEntry = SyHashGetNextEntry(pSrcHash)) != 0 ){` |
|    327 |  396 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|    327 |  397 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|    327 |  398 | `				if( iLevel == 0 ){` |
|      - |  399 | `					sxu32 j;` |
|      - |  400 | `					/* Own = declared here or by an off-chain provider (trait) */` |
|    321 |  401 | `					for( j = 1 ; j < nChain ; j++ ){` |
|     79 |  402 | `						if( aChain[j] == pDecl ){ break; }` |
|     22 |  403 | `					}` |
|    279 |  404 | `					if( j < nChain ){ continue; }` |
|    124 |  405 | `				}else{` |
|      - |  406 | `					SyHashEntry *pSub;` |
|     49 |  407 | `					if( pDecl != pLevel ){ continue; }` |
|      - |  408 | `					/* Must still be the visible member in the reflected class */` |
|     37 |  409 | `					pSub = SyHashGet(pRefHash, pEntry->pKey, pEntry->nKeyLen);` |
|     37 |  410 | `					if( pSub == 0 \|\| pSub->pUserData != (void *)pAttr ){ continue; }` |
|      - |  411 | `				}` |
|    279 |  412 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|      5 |  413 | `			}` |
|      - |  414 | `			/* Forward: hAttr now iterates in DECLARATION order (its inserts are` |
|      - |  415 | `			 * tail inserts), so members come out in the order php reports them.` |
|      - |  416 | `			 * This walked aTmp backwards to undo the table's old head-insert` |
|      - |  417 | `			 * (LIFO) storage; with that reversal gone from the table, reversing` |
|      - |  418 | `			 * here would emit members back to front. The METHOD loop below keeps` |
|      - |  419 | `			 * its reverse walk — hMethod is still a head-insert table. */` |
|    703 |  420 | `			for( nT = 0 ; nT < SySetUsed(&aTmp) ; nT++ ){` |
|    279 |  421 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT);` |
|    279 |  422 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;` |
|    279 |  423 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|    279 |  424 | `				ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|    279 |  425 | `				if( pMeta == 0 ){ break; }` |
|    279 |  426 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pAttr->iProtection);` |
|    279 |  427 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|    279 |  428 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pAttr->nLine);` |
|    279 |  429 | `				ReflectMapAddDoc(pCtx, pMeta, &pAttr->sDoc);` |
|    279 |  430 | `				ReflectMapAddAttrs(pCtx, pMeta, &pAttr->aAttrs);` |
|    279 |  431 | `				ReflectMapAddBool(pCtx, pMeta, "typed", (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);` |
|    279 |  432 | `				if( SyStringLength(&pAttr->sTypeName) > 0 ){` |
|    108 |  433 | `					ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&pAttr->sTypeName),` |
|     70 |  434 | `						(int)SyStringLength(&pAttr->sTypeName));` |
|     38 |  435 | `				}else{` |
|    209 |  436 | `					ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|      - |  437 | `				}` |
|    279 |  438 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|     67 |  439 | `					ReflectMapAddBool(pCtx, pMeta, "final", (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|     67 |  440 | `					ReflectMapAddBool(pCtx, pMeta, "enumcase", (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0);` |
|     67 |  441 | `					ReflectMapAddDyn(pCtx, pConsts, &pAttr->sName, pMeta);` |
|     35 |  442 | `				}else{` |
|    215 |  443 | `					ReflectMapAddBool(pCtx, pMeta, "static", (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|    215 |  444 | `					ReflectMapAddBool(pCtx, pMeta, "readonly", (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0);` |
|    215 |  445 | `					ReflectMapAddBool(pCtx, pMeta, "privset", (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) != 0);` |
|    215 |  446 | `					ReflectMapAddBool(pCtx, pMeta, "protset", (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET) != 0);` |
|    215 |  447 | `					ReflectMapAddBool(pCtx, pMeta, "hookget", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) != 0);` |
|    215 |  448 | `					ReflectMapAddBool(pCtx, pMeta, "hookset", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) != 0);` |
|    215 |  449 | `					ReflectMapAddBool(pCtx, pMeta, "virtual", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) != 0);` |
|    215 |  450 | `					ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&pAttr->aByteCode) > 0);` |
|    215 |  451 | `					ReflectMapAddDyn(pCtx, pProps, &pAttr->sName, pMeta);` |
|      - |  452 | `				}` |
|    142 |  453 | `			}` |
|    217 |  454 | `			} /* for iTab */` |
|      - |  455 | `			}` |
|      - |  456 | `			/* --- Methods. The reported name is the hash-entry key: trait` |
|      - |  457 | `			 * aliasing installs a shallow copy under the alias name while` |
|      - |  458 | `			 * sFunc.sName keeps the original, and PHP reports the alias. --- */` |
|    217 |  459 | `			SySetReset(&aTmp);` |
|    217 |  460 | `			SyHashResetLoopCursor(&pLevel->hMethod);` |
|    643 |  461 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){` |
|    431 |  462 | `				ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|    431 |  463 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|    431 |  464 | `				if( iLevel == 0 ){` |
|      - |  465 | `					sxu32 j;` |
|    409 |  466 | `					for( j = 1 ; j < nChain ; j++ ){` |
|    111 |  467 | `						if( aChain[j] == pDecl ){ break; }` |
|     29 |  468 | `					}` |
|    353 |  469 | `					if( j < nChain ){ continue; }` |
|    152 |  470 | `				}else{` |
|      - |  471 | `					SyHashEntry *pSub;` |
|     79 |  472 | `					if( pDecl != pLevel ){ continue; }` |
|     57 |  473 | `					pSub = SyHashGet(&pClass->hMethod, pEntry->pKey, pEntry->nKeyLen);` |
|     57 |  474 | `					if( pSub == 0 ){` |
|      - |  475 | `						/* Not in the subclass table: inheritance skips private` |
|      - |  476 | `						 * methods, but PHP still reports them on the subclass` |
|      - |  477 | `						 * (Zend copies privates into the child function table). */` |
|    ! 0 |  478 | `						if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|    ! 0 |  479 | `							continue;` |
|    ! 0 |  480 | `						}` |
|     57 |  481 | `					}else if( pSub->pUserData != (void *)pMeth ){` |
|      - |  482 | `						/* Overridden below this level: already reported */` |
|      3 |  483 | `						continue;` |
|      - |  484 | `					}` |
|      - |  485 | `				}` |
|    353 |  486 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|      5 |  487 | `			}` |
|    565 |  488 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|    353 |  489 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|    353 |  490 | `				ph7_class_method *pMeth = (ph7_class_method *)pE->pUserData;` |
|    353 |  491 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|      - |  492 | `				ph7_value *pMeta;` |
|      - |  493 | `				SyString sKey;` |
|    353 |  494 | `				SyStringInitFromBuf(&sKey, (const char *)pE->pKey, pE->nKeyLen);` |
|    348 |  495 | `				if( sKey.nByte == sizeof("__construct")-1` |
|    208 |  496 | `				 && SyMemcmp(sKey.zString, "__construct", sKey.nByte) == 0 ){` |
|     44 |  497 | `					if( iCtorVis == 0 ){` |
|     44 |  498 | `						iCtorVis = pMeth->iProtection;` |
|     20 |  499 | `					}` |
|      - |  500 | `					/* A __construct key whose method has a DIFFERENT own name is a trait` |
|      - |  501 | ``					 * `use T { m as __construct; }` alias. php lists such a method under`` |
|      - |  502 | `					 * BOTH names (its own and __construct) and getConstructor() resolves` |
|      - |  503 | `					 * the __construct one, so emit the entry under this key too rather than` |
|      - |  504 | `					 * skipping it. (The legacy PHP-4 class-name-constructor mount alias that` |
|      - |  505 | `					 * also produced a __construct key is gone, removed in 8.0.) */` |
|    332 |  506 | `				}else if( sKey.nByte == sizeof("__clone")-1` |
|    177 |  507 | `				 && SyMemcmp(sKey.zString, "__clone", sKey.nByte) == 0 ){` |
|    ! 0 |  508 | `					if( iCloneVis == 0 ){` |
|    ! 0 |  509 | `						iCloneVis = pMeth->iProtection;` |
|    ! 0 |  510 | `					}` |
|    ! 0 |  511 | `				}` |
|      - |  512 | `				/* No PHP-4 class-name constructor: a method named like the class is a` |
|      - |  513 | `				 * plain method (removed in 8.0), so getConstructor() stays null unless` |
|      - |  514 | `				 * an explicit __construct exists. */` |
|    353 |  515 | `				pMeta = ph7_context_new_array(pCtx);` |
|    353 |  516 | `				if( pMeta == 0 ){ break; }` |
|    353 |  517 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pMeth->iProtection);` |
|    353 |  518 | `				ReflectMapAddBool(pCtx, pMeta, "static", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|    353 |  519 | `				ReflectMapAddBool(pCtx, pMeta, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|    353 |  520 | `				ReflectMapAddBool(pCtx, pMeta, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|    353 |  521 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|    353 |  522 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pMeth->nLine);` |
|    353 |  523 | `				ReflectMapAddDyn(pCtx, pMethods, &sKey, pMeta);` |
|    179 |  524 | `			}` |
|    111 |  525 | `		}` |
|    187 |  526 | `		SySetRelease(&aTmp);` |
|      - |  527 | `	}` |
|    187 |  528 | `	ReflectMapAddInt(pCtx, pInfo, "ctorvis", (sxi64)iCtorVis);` |
|    187 |  529 | `	ReflectMapAddInt(pCtx, pInfo, "clonevis", (sxi64)iCloneVis);` |
|    187 |  530 | `	ph7_array_add_strkey_elem(pInfo, "consts", pConsts);` |
|    187 |  531 | `	ph7_array_add_strkey_elem(pInfo, "props", pProps);` |
|    187 |  532 | `	ph7_array_add_strkey_elem(pInfo, "methods", pMethods);` |
|    187 |  533 | `	ph7_result_value(pCtx, pInfo);` |
|    187 |  534 | `	return PH7_OK;` |
|    103 |  535 | `}` |
|      - |  536 | `/*` |
|      - |  537 | ` * mixed __reflect_const_value(string $class, string $name)` |
|      - |  538 | ` * Value of a class constant. The PHP layer guarantees existence.` |
|      - |  539 | ` */` |
|     58 |  540 | `static int vm_builtin_reflect_const_value(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  541 | `{` |
|      - |  542 | `	ph7_class *pClass;` |
|      - |  543 | `	ph7_class_attr *pAttr;` |
|      - |  544 | `	ph7_value *pValue;` |
|     58 |  545 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|     58 |  546 | `	 \|\| (pAttr = ReflectFetchConst(pClass, apArg[1])) == 0` |
|     61 |  547 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|    ! 0 |  548 | `		ph7_result_null(pCtx);` |
|    ! 0 |  549 | `		return PH7_OK;` |
|      - |  550 | `	}` |
|      - |  551 | `	/* Constant slots are evaluated lazily on first access */` |
|     61 |  552 | `	if( PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr) != SXRET_OK ){` |
|      - |  553 | `		/* Initializer raised: the throw is in flight; report null here */` |
|      3 |  554 | `		ph7_result_null(pCtx);` |
|      3 |  555 | `		return PH7_OK;` |
|      - |  556 | `	}` |
|     58 |  557 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|     58 |  558 | `	if( pValue ){` |
|     58 |  559 | `		ph7_result_value(pCtx, pValue);` |
|     30 |  560 | `	}else{` |
|    ! 0 |  561 | `		ph7_result_null(pCtx);` |
|      - |  562 | `	}` |
|     58 |  563 | `	return PH7_OK;` |
|     32 |  564 | `}` |
|      - |  565 | `/*` |
|      - |  566 | ` * bool __reflect_static_materialize(string $class)` |
|      - |  567 | ` * Materialize the class's static table (php does this BEFORE looking a static` |
|      - |  568 | `` * property up, so `getStaticPropertyValue('nope')` on a class with a broken`` |
|      - |  569 | ` * default reports the default's error, not "property does not exist"). The` |
|      - |  570 | ` * chunk calls this first; the per-slot readers below gate again for the paths` |
|      - |  571 | ` * that reach them directly.` |
|      - |  572 | ` */` |
|     28 |  573 | `static int vm_builtin_reflect_static_materialize(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  574 | `{` |
|      - |  575 | `	ph7_class *pClass;` |
|     30 |  576 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0 ){` |
|    ! 0 |  577 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  578 | `		return PH7_OK;` |
|      - |  579 | `	}` |
|     30 |  580 | `	if( VmClassStaticDeferPending(pClass) ){` |
|     13 |  581 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);` |
|     13 |  582 | `		if( rcMat != SXRET_OK ){` |
|     13 |  583 | `			return rcMat;` |
|      - |  584 | `		}` |
|    ! 0 |  585 | `	}` |
|     18 |  586 | `	ph7_result_bool(pCtx, 1);` |
|     18 |  587 | `	return PH7_OK;` |
|     16 |  588 | `}` |
|      - |  589 | `/*` |
|      - |  590 | ` * mixed __reflect_static_value(string $class, string $name)` |
|      - |  591 | ` * Current value of a static property (visibility ignored).` |
|      - |  592 | ` */` |
|     16 |  593 | `static int vm_builtin_reflect_static_value(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  594 | `{` |
|      - |  595 | `	ph7_class *pClass;` |
|      - |  596 | `	ph7_class_attr *pAttr;` |
|      - |  597 | `	ph7_value *pValue;` |
|     16 |  598 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|     16 |  599 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|     18 |  600 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|    ! 0 |  601 | `		ph7_result_null(pCtx);` |
|    ! 0 |  602 | `		return PH7_OK;` |
|      - |  603 | `	}` |
|     18 |  604 | `	if( VmClassStaticDeferPending(pClass) ){` |
|      - |  605 | `		/* Reading a static through reflection materializes the class's static` |
|      - |  606 | ``		 * table exactly as `C::$s` does, so a default that threw at the`` |
|      - |  607 | `		 * declaration raises HERE (php: getStaticPropertyValue() /` |
|      - |  608 | `		 * getStaticProperties() / ReflectionProperty::getValue() all do). */` |
|      3 |  609 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);` |
|      3 |  610 | `		if( rcMat != SXRET_OK ){` |
|      3 |  611 | `			return rcMat;` |
|      - |  612 | `		}` |
|    ! 0 |  613 | `	}` |
|      - |  614 | `	{` |
|      - |  615 | `		/* Uninitialized typed static: same Error the VM raises on read */` |
|     16 |  616 | `		SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|     16 |  617 | `		if( pSlot ){` |
|      3 |  618 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      3 |  619 | `			if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|    ! 0 |  620 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|    ! 0 |  621 | `				return PH7_VmThrowException(pCtx, "Error",` |
|      - |  622 | `					"Typed static property %z::$%z must not be accessed before initialization",` |
|    ! 0 |  623 | `					&pDecl->sName, &pAttr->sName);` |
|      - |  624 | `			}` |
|      1 |  625 | `		}` |
|      - |  626 | `	}` |
|     16 |  627 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|     16 |  628 | `	if( pValue ){` |
|     16 |  629 | `		ph7_result_value(pCtx, pValue);` |
|      9 |  630 | `	}else{` |
|    ! 0 |  631 | `		ph7_result_null(pCtx);` |
|      - |  632 | `	}` |
|     16 |  633 | `	return PH7_OK;` |
|     10 |  634 | `}` |
|      - |  635 | `/*` |
|      - |  636 | ` * bool __reflect_static_set(string $class, string $name, mixed $value)` |
|      - |  637 | ` * Overwrite a static property's shared slot (visibility ignored).` |
|      - |  638 | ` */` |
|      6 |  639 | `static int vm_builtin_reflect_static_set(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  640 | `{` |
|      - |  641 | `	ph7_class *pClass;` |
|      - |  642 | `	ph7_class_attr *pAttr;` |
|      - |  643 | `	ph7_value *pValue;` |
|      6 |  644 | `	if( nArg < 3 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|      6 |  645 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|      8 |  646 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|    ! 0 |  647 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  648 | `		return PH7_OK;` |
|      - |  649 | `	}` |
|      8 |  650 | `	if( VmClassStaticDeferPending(pClass) ){` |
|      - |  651 | `		/* A WRITE materializes the table too (php's setStaticPropertyValue()` |
|      - |  652 | `		 * raises on a broken default before storing anything). */` |
|      3 |  653 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);` |
|      3 |  654 | `		if( rcMat != SXRET_OK ){` |
|      3 |  655 | `			return rcMat;` |
|      - |  656 | `		}` |
|    ! 0 |  657 | `	}` |
|      5 |  658 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|      5 |  659 | `	if( pValue == 0 ){` |
|    ! 0 |  660 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  661 | `		return PH7_OK;` |
|      - |  662 | `	}` |
|      - |  663 | `	{` |
|      5 |  664 | `		sxi32 rc = ReflectEnforceStore(pCtx, pAttr->nIdx, apArg[2]);` |
|      5 |  665 | `		if( rc != SXRET_OK ){` |
|    ! 0 |  666 | `			return rc;` |
|      - |  667 | `		}` |
|      - |  668 | `	}` |
|      5 |  669 | `	PH7_MemObjStore(apArg[2], pValue);` |
|      5 |  670 | `	ph7_result_bool(pCtx, 1);` |
|      5 |  671 | `	return PH7_OK;` |
|      5 |  672 | `}` |
|      - |  673 | `/*` |
|      - |  674 | ` * mixed __reflect_prop_default(string $class, string $name)` |
|      - |  675 | ` * Evaluate a non-static property's compiled default expression` |
|      - |  676 | ` * (null when the property has no default).` |
|      - |  677 | ` */` |
|     24 |  678 | `static int vm_builtin_reflect_prop_default(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  679 | `{` |
|      - |  680 | `	ph7_class *pClass;` |
|      - |  681 | `	ph7_class_attr *pAttr;` |
|      - |  682 | `	ph7_value sValue;` |
|     24 |  683 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|     24 |  684 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|     24 |  685 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) != 0` |
|     25 |  686 | `	 \|\| SySetUsed(&pAttr->aByteCode) < 1 ){` |
|      3 |  687 | `		ph7_result_null(pCtx);` |
|      3 |  688 | `		return PH7_OK;` |
|      - |  689 | `	}` |
|     23 |  690 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|      - |  691 | `	/* Same evaluation path the VM uses for omitted call arguments */` |
|     23 |  692 | `	VmLocalExec(pCtx->pVm, &pAttr->aByteCode, &sValue, FALSE);` |
|     23 |  693 | `	ph7_result_value(pCtx, &sValue);` |
|     23 |  694 | `	PH7_MemObjRelease(&sValue);` |
|     23 |  695 | `	return PH7_OK;` |
|     13 |  696 | `}` |
|      - |  697 | `/*` |
|      - |  698 | ` * Collect a PHP array's values into a ph7_value* set (call arguments).` |
|      - |  699 | ` * When ppNames is non-NULL, string keys become named arguments: a name` |
|      - |  700 | ` * map is lazily allocated (like call_user_func_array's) with one entry` |
|      - |  701 | ` * per collected slot, empty entries meaning positional.` |
|      - |  702 | ` */` |
|     46 |  703 | `static sxi32 ReflectCollectArgs(ph7_context *pCtx, ph7_value *pArray, SySet *pOut, SyString **ppNames)` |
|      1 |  704 | `{` |
|      - |  705 | `	ph7_hashmap *pMap;` |
|      - |  706 | `	ph7_hashmap_node *pEntry;` |
|     47 |  707 | `	SyString *aNames = 0;` |
|     47 |  708 | `	sxu32 nSlot = 0;` |
|      - |  709 | `	sxu32 n;` |
|     47 |  710 | `	if( ppNames ){` |
|     27 |  711 | `		*ppNames = 0;` |
|     13 |  712 | `	}` |
|     47 |  713 | `	if( !ph7_value_is_array(pArray) ){` |
|    ! 0 |  714 | `		return SXRET_OK;` |
|      - |  715 | `	}` |
|     47 |  716 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|     47 |  717 | `	pEntry = pMap->pFirst;` |
|    107 |  718 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|     61 |  719 | `		ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|     61 |  720 | `		if( pValue ){` |
|     61 |  721 | `			if( ppNames && pEntry->iType == HASHMAP_BLOB_NODE ){` |
|      3 |  722 | `				if( aNames == 0 ){` |
|      4 |  723 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|      2 |  724 | `						pMap->nEntry * sizeof(SyString));` |
|      3 |  725 | `					if( aNames ){` |
|      3 |  726 | `						SyZero(aNames, pMap->nEntry * sizeof(SyString));` |
|      1 |  727 | `					}` |
|      1 |  728 | `				}` |
|      3 |  729 | `				if( aNames ){` |
|      3 |  730 | `					SyStringInitFromBuf(&aNames[nSlot],` |
|      - |  731 | `						SyBlobData(&pEntry->xKey.sKey), SyBlobLength(&pEntry->xKey.sKey));` |
|      1 |  732 | `				}` |
|      1 |  733 | `			}` |
|     61 |  734 | `			SySetPut(pOut, (const void *)&pValue);` |
|     61 |  735 | `			nSlot++;` |
|     30 |  736 | `		}` |
|     61 |  737 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|     31 |  738 | `	}` |
|     47 |  739 | `	if( ppNames ){` |
|     27 |  740 | `		*ppNames = aNames;` |
|     13 |  741 | `	}` |
|     47 |  742 | `	return SXRET_OK;` |
|     24 |  743 | `}` |
|      - |  744 | `/*` |
|      - |  745 | ` * object __reflect_new_instance(string $class, array $args)` |
|      - |  746 | ` * Instantiate and run the constructor with positional arguments.` |
|      - |  747 | ` * The PHP layer has already validated instantiability and ctor visibility.` |
|      - |  748 | ` */` |
|     32 |  749 | `static int vm_builtin_reflect_new_instance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  750 | `{` |
|     34 |  751 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  752 | `	ph7_class *pClass;` |
|      - |  753 | `	ph7_class_instance *pThis;` |
|      - |  754 | `	ph7_class_method *pCons;` |
|     34 |  755 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pVm, apArg[0])) == 0 ){` |
|    ! 0 |  756 | `		ph7_result_null(pCtx);` |
|    ! 0 |  757 | `		return PH7_OK;` |
|      - |  758 | `	}` |
|     34 |  759 | `	if( VmClassStaticDeferPending(pClass) ){` |
|      - |  760 | `		/* Instantiation materializes the static table (OP_NEW does it too), so a` |
|      - |  761 | `		 * broken default raises BEFORE any object exists. */` |
|      3 |  762 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pVm, pClass);` |
|      3 |  763 | `		if( rcMat != SXRET_OK ){` |
|      3 |  764 | `			return rcMat;` |
|      - |  765 | `		}` |
|    ! 0 |  766 | `	}` |
|     31 |  767 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|     31 |  768 | `	if( pThis == 0 ){` |
|    ! 0 |  769 | `		ph7_result_null(pCtx);` |
|    ! 0 |  770 | `		return PH7_OK;` |
|      - |  771 | `	}` |
|     31 |  772 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|     31 |  773 | `	if( pCons ){` |
|      - |  774 | `		SySet aArg;` |
|      - |  775 | `		sxi32 rc;` |
|     27 |  776 | `		SyString *aNames = 0;` |
|     27 |  777 | `		SySetInit(&aArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|     27 |  778 | `		if( nArg > 1 ){` |
|     27 |  779 | `			ReflectCollectArgs(pCtx, apArg[1], &aArg, &aNames);` |
|     13 |  780 | `		}` |
|     27 |  781 | `		if( aNames ){` |
|      - |  782 | `			VmCallArgMap sMap;` |
|      3 |  783 | `			SyZero(&sMap,sizeof(sMap)); /* new map fields must read unset, not stack garbage */` |
|      3 |  784 | `			sMap.bHasNamed = 1;` |
|      3 |  785 | `			sMap.bIsNamespaced = 0;` |
|      3 |  786 | `			sMap.bStrict = 0;` |
|      3 |  787 | `			sMap.nTotal = SySetUsed(&aArg);` |
|      3 |  788 | `			sMap.aNames = aNames;` |
|      4 |  789 | `			rc = PH7_VmCallClassMethodMap(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|      2 |  790 | `				(ph7_value **)SySetBasePtr(&aArg), &sMap);` |
|      3 |  791 | `			SyMemBackendFree(&pVm->sAllocator, aNames);` |
|      2 |  792 | `		}else{` |
|     37 |  793 | `			rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|     24 |  794 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|      - |  795 | `		}` |
|     27 |  796 | `		SySetRelease(&aArg);` |
|     27 |  797 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|    ! 0 |  798 | `			PH7_ClassInstanceUnref(pThis);` |
|    ! 0 |  799 | `			return rc;` |
|      - |  800 | `		}` |
|     13 |  801 | `	}` |
|     31 |  802 | `	return ReflectResultObject(pCtx, pThis);` |
|     18 |  803 | `}` |
|      - |  804 | `/*` |
|      - |  805 | ` * object __reflect_new_no_ctor(string $class)` |
|      - |  806 | ` * Instantiate without running the constructor (property defaults still` |
|      - |  807 | ` * apply — PH7_NewClassInstance builds the attribute frame).` |
|      - |  808 | ` */` |
|     72 |  809 | `static int vm_builtin_reflect_new_no_ctor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      3 |  810 | `{` |
|      - |  811 | `	ph7_class *pClass;` |
|     75 |  812 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0 ){` |
|    ! 0 |  813 | `		ph7_result_null(pCtx);` |
|    ! 0 |  814 | `		return PH7_OK;` |
|      - |  815 | `	}` |
|     75 |  816 | `	if( VmClassStaticDeferPending(pClass) ){` |
|      - |  817 | `		/* Same materialization the engine's own access sites run (see` |
|      - |  818 | `		 * PH7_VmMaterializeClassStatics). */` |
|      3 |  819 | `		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);` |
|      3 |  820 | `		if( rcMat != SXRET_OK ){` |
|      3 |  821 | `			return rcMat;` |
|      - |  822 | `		}` |
|    ! 0 |  823 | `	}` |
|     72 |  824 | `	return ReflectResultObject(pCtx, PH7_NewClassInstance(pCtx->pVm, pClass));` |
|     39 |  825 | `}` |
|      - |  826 | `/*` |
|      - |  827 | ` * Typed/readonly store enforcement for reflection writes. Like the VM's` |
|      - |  828 | ` * store path, except an UNINITIALIZED readonly property may be written from` |
|      - |  829 | ` * any scope (PHP lets ReflectionProperty::setValue initialize readonly): the` |
|      - |  830 | ` * READONLY bit is masked off for the enforcement call so the set-scope check` |
|      - |  831 | ` * is skipped, while an already-initialized readonly still gets PHP's` |
|      - |  832 | ` * "Cannot modify readonly property" Error. Returns SXRET_OK/PH7_EXCEPTION/` |
|      - |  833 | ` * PH7_ABORT; the value may be coerced in place.` |
|      - |  834 | ` */` |
|     10 |  835 | `static sxi32 ReflectEnforceStore(ph7_context *pCtx, sxu32 nIdx, ph7_value *pValue)` |
|      1 |  836 | `{` |
|     11 |  837 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - |  838 | `	SyHashEntry *pSlot;` |
|      - |  839 | `	VmClassAttr *pVmAttr;` |
|      - |  840 | `	ph7_class_attr *pAttr;` |
|      - |  841 | `	sxi32 iSaved, rc;` |
|     11 |  842 | `	pSlot = SyHashGet(&pVm->hTypedSlot, (const void *)&nIdx, sizeof(sxu32));` |
|     11 |  843 | `	if( pSlot == 0 ){` |
|      7 |  844 | `		return SXRET_OK; /* Untyped slot: plain store */` |
|      - |  845 | `	}` |
|      5 |  846 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|      5 |  847 | `	pAttr = pVmAttr->pAttr;` |
|      5 |  848 | `	if( pAttr == 0 ){` |
|    ! 0 |  849 | `		return SXRET_OK;` |
|      - |  850 | `	}` |
|      5 |  851 | `	iSaved = pAttr->iFlags;` |
|      5 |  852 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) ){` |
|    ! 0 |  853 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_READONLY;` |
|    ! 0 |  854 | `	}` |
|      5 |  855 | `	rc = PH7_VmEnforcePropStore(pVm, nIdx, pValue);` |
|      5 |  856 | `	pAttr->iFlags = iSaved;` |
|      5 |  857 | `	return rc;` |
|      6 |  858 | `}` |
|      - |  859 | `/*` |
|      - |  860 | ` * mixed __reflect_prop_read(object $obj, string $name)` |
|      - |  861 | ` * Instance property read, visibility ignored. Throws PHP's Error for an` |
|      - |  862 | ` * uninitialized typed property.` |
|      - |  863 | ` */` |
|     20 |  864 | `static int vm_builtin_reflect_prop_read(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  865 | `{` |
|      - |  866 | `	ph7_class_instance *pThis;` |
|      - |  867 | `	SyHashEntry *pEntry;` |
|      - |  868 | `	VmClassAttr *pVmAttr;` |
|      - |  869 | `	ph7_value *pValue;` |
|      - |  870 | `	const char *zName;` |
|      - |  871 | `	int nLen;` |
|     21 |  872 | `	if( nArg < 2 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 |  873 | `		ph7_result_null(pCtx);` |
|    ! 0 |  874 | `		return PH7_OK;` |
|      - |  875 | `	}` |
|     21 |  876 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     21 |  877 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|     21 |  878 | `	pEntry = nLen > 0 ? SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen) : 0;` |
|     21 |  879 | `	if( pEntry == 0 ){` |
|    ! 0 |  880 | `		ph7_result_null(pCtx);` |
|    ! 0 |  881 | `		return PH7_OK;` |
|      - |  882 | `	}` |
|     21 |  883 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     21 |  884 | `	if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|      3 |  885 | `		ph7_class *pDecl = pVmAttr->pAttr->pDeclClass ? pVmAttr->pAttr->pDeclClass : pThis->pClass;` |
|      4 |  886 | `		return PH7_VmThrowException(pCtx, "Error",` |
|      - |  887 | `			"Typed property %z::$%z must not be accessed before initialization",` |
|      2 |  888 | `			&pDecl->sName, &pVmAttr->pAttr->sName);` |
|      - |  889 | `	}` |
|     19 |  890 | `	pValue = PH7_ClassInstanceExtractAttrValue(pThis, pVmAttr);` |
|     19 |  891 | `	if( pValue ){` |
|     19 |  892 | `		ph7_result_value(pCtx, pValue);` |
|     10 |  893 | `	}else{` |
|    ! 0 |  894 | `		ph7_result_null(pCtx);` |
|      - |  895 | `	}` |
|     19 |  896 | `	return PH7_OK;` |
|     11 |  897 | `}` |
|      - |  898 | `/*` |
|      - |  899 | ` * bool __reflect_prop_write(object $obj, string $name, mixed $value)` |
|      - |  900 | ` * Instance property write, visibility ignored; typed and readonly rules` |
|      - |  901 | ` * enforced (see ReflectEnforceStore).` |
|      - |  902 | ` */` |
|      6 |  903 | `static int vm_builtin_reflect_prop_write(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  904 | `{` |
|      - |  905 | `	ph7_class_instance *pThis;` |
|      - |  906 | `	SyHashEntry *pEntry;` |
|      - |  907 | `	VmClassAttr *pVmAttr;` |
|      - |  908 | `	ph7_value *pValue;` |
|      - |  909 | `	const char *zName;` |
|      - |  910 | `	sxi32 rc;` |
|      - |  911 | `	int nLen;` |
|      7 |  912 | `	if( nArg < 3 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|    ! 0 |  913 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  914 | `		return PH7_OK;` |
|      - |  915 | `	}` |
|      7 |  916 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      7 |  917 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|      7 |  918 | `	pEntry = nLen > 0 ? SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen) : 0;` |
|      7 |  919 | `	if( pEntry == 0 ){` |
|    ! 0 |  920 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  921 | `		return PH7_OK;` |
|      - |  922 | `	}` |
|      7 |  923 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      7 |  924 | `	rc = ReflectEnforceStore(pCtx, pVmAttr->nIdx, apArg[2]);` |
|      7 |  925 | `	if( rc != SXRET_OK ){` |
|      3 |  926 | `		return rc;` |
|      - |  927 | `	}` |
|      5 |  928 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|      5 |  929 | `	if( pValue == 0 ){` |
|    ! 0 |  930 | `		ph7_result_bool(pCtx, 0);` |
|    ! 0 |  931 | `		return PH7_OK;` |
|      - |  932 | `	}` |
|      5 |  933 | `	PH7_MemObjStore(apArg[2], pValue);` |
|      5 |  934 | `	ph7_result_bool(pCtx, 1);` |
|      5 |  935 | `	return PH7_OK;` |
|      4 |  936 | `}` |
|      - |  937 | `/*` |
|      - |  938 | ` * int __reflect_prop_state(object\|string $target, string $name)` |
|      - |  939 | ` * Bitfield: 1 = exists (instance attr / static slot), 2 = initialized,` |
|      - |  940 | ` * 4 = dynamic (instance-owned, not class-declared).` |
|      - |  941 | ` */` |
|     20 |  942 | `static int vm_builtin_reflect_prop_state(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      2 |  943 | `{` |
|     22 |  944 | `	int iState = 0;` |
|      - |  945 | `	const char *zName;` |
|      - |  946 | `	int nLen;` |
|     22 |  947 | `	if( nArg < 2 ){` |
|    ! 0 |  948 | `		ph7_result_int(pCtx, 0);` |
|    ! 0 |  949 | `		return PH7_OK;` |
|      - |  950 | `	}` |
|     22 |  951 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|     22 |  952 | `	if( nLen < 1 ){` |
|    ! 0 |  953 | `		ph7_result_int(pCtx, 0);` |
|    ! 0 |  954 | `		return PH7_OK;` |
|      - |  955 | `	}` |
|     22 |  956 | `	if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|     17 |  957 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     17 |  958 | `		SyHashEntry *pEntry = SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen);` |
|     17 |  959 | `		if( pEntry ){` |
|     17 |  960 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     17 |  961 | `			iState \|= 1;` |
|     17 |  962 | `			if( (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|     15 |  963 | `				iState \|= 2;` |
|      7 |  964 | `			}` |
|     17 |  965 | `			if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|     11 |  966 | `				iState \|= 4;` |
|      5 |  967 | `			}` |
|      8 |  968 | `		}` |
|      9 |  969 | `	}else{` |
|      5 |  970 | `		ph7_class *pClass = ReflectResolveClass(pCtx->pVm, apArg[0]);` |
|      5 |  971 | `		ph7_class_attr *pAttr = pClass ? ReflectFetchAttr(pClass, apArg[1]) : 0;` |
|      5 |  972 | `		if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|      - |  973 | `			SyHashEntry *pSlot;` |
|      5 |  974 | `			if( VmClassStaticDeferPending(pClass) ){` |
|      - |  975 | `				/* isInitialized() reads the slot state, so it materializes the` |
|      - |  976 | `				 * table too (php raises the default's error before answering). */` |
|      5 |  977 | `				sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);` |
|      5 |  978 | `				if( rcMat != SXRET_OK ){` |
|      5 |  979 | `					return rcMat;` |
|      - |  980 | `				}` |
|    ! 0 |  981 | `			}` |
|    ! 0 |  982 | `			pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|    ! 0 |  983 | `			iState \|= 1 \| 2;` |
|    ! 0 |  984 | `			if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|    ! 0 |  985 | `				iState &= ~2;` |
|    ! 0 |  986 | `			}` |
|    ! 0 |  987 | `		}` |
|      - |  988 | `	}` |
|     17 |  989 | `	ph7_result_int(pCtx, iState);` |
|     17 |  990 | `	return PH7_OK;` |
|     12 |  991 | `}` |
|      - |  992 | `/*` |
|      - |  993 | ` * array __reflect_dyn_props(object $obj)` |
|      - |  994 | ` * Names of the instance's runtime-added (dynamic) properties, in creation` |
|      - |  995 | ` * order (the instance attr table inserts dynamics at the tail).` |
|      - |  996 | ` */` |
|      2 |  997 | `static int vm_builtin_reflect_dyn_props(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 |  998 | `{` |
|      - |  999 | `	ph7_class_instance *pThis;` |
|      - | 1000 | `	SyHashEntry *pEntry;` |
|      - | 1001 | `	ph7_value *pList;` |
|      2 | 1002 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|      3 | 1003 | `	 \|\| (pList = ph7_context_new_array(pCtx)) == 0 ){` |
|    ! 0 | 1004 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1005 | `		return PH7_OK;` |
|      - | 1006 | `	}` |
|      3 | 1007 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|      3 | 1008 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|      7 | 1009 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|      5 | 1010 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|      5 | 1011 | `		if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|      5 | 1012 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|      5 | 1013 | `			if( pName == 0 ){ break; }` |
|      7 | 1014 | `			ph7_value_string(pName, SyStringData(&pVmAttr->pAttr->sName),` |
|      4 | 1015 | `				(int)SyStringLength(&pVmAttr->pAttr->sName));` |
|      5 | 1016 | `			ph7_array_add_elem(pList, 0, pName);` |
|      2 | 1017 | `		}` |
|      1 | 1018 | `	}` |
|      3 | 1019 | `	ph7_result_value(pCtx, pList);` |
|      3 | 1020 | `	return PH7_OK;` |
|      2 | 1021 | `}` |
|      - | 1022 | `/* Hand an EXISTING instance to the caller: takes an extra reference` |
|      - | 1023 | ` * (unlike ReflectResultObject, which transfers a fresh instance's one). */` |
|      4 | 1024 | `static int ReflectResultExistingObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|      1 | 1025 | `{` |
|      5 | 1026 | `	if( pObj == 0 ){` |
|    ! 0 | 1027 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1028 | `		return PH7_OK;` |
|      - | 1029 | `	}` |
|      5 | 1030 | `	PH7_MemObjRelease(pCtx->pRet);` |
|      5 | 1031 | `	pObj->iRef++;` |
|      5 | 1032 | `	pCtx->pRet->x.pOther = pObj;` |
|      5 | 1033 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|      5 | 1034 | `	return PH7_OK;` |
|      3 | 1035 | `}` |
|      - | 1036 | `/* pVal is a Closure instance? Return it, else NULL. */` |
|    908 | 1037 | `static ph7_class_instance * ReflectValueClosure(ph7_vm *pVm, ph7_value *pVal)` |
|      2 | 1038 | `{` |
|      - | 1039 | `	ph7_class_instance *pThis;` |
|    910 | 1040 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
|    834 | 1041 | `		return 0;` |
|      - | 1042 | `	}` |
|     78 | 1043 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     78 | 1044 | `	return (pThis->pClass == pVm->pClosureClass) ? pThis : 0;` |
|    456 | 1045 | `}` |
|      - | 1046 | `/*` |
|      - | 1047 | ` * Resolve a reflection callable target into its compiled function.` |
|      - | 1048 | ` *   - pMethodArg a non-empty string  -> method mode: pTarget is a class name` |
|      - | 1049 | ` *     or object; outputs *ppClass and *ppMeth.` |
|      - | 1050 | ` *   - pTarget a Closure              -> unwrap $__fn into hFunction; *ppClosure.` |
|      - | 1051 | ` *   - pTarget a string               -> hFunction (user) or hHostFunction` |
|      - | 1052 | ` *     (*ppHost set, returns NULL).` |
|      - | 1053 | ` * Returns the ph7_vm_func, or NULL (host function or unresolvable).` |
|      - | 1054 | ` */` |
|   1250 | 1055 | `static ph7_vm_func * ReflectResolveCallable(ph7_context *pCtx, ph7_value *pTarget,` |
|      - | 1056 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|      - | 1057 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|      5 | 1058 | `{` |
|   1255 | 1059 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1060 | `	SyHashEntry *pEntry;` |
|   1255 | 1061 | `	if( ppClass ){ *ppClass = 0; }` |
|   1255 | 1062 | `	if( ppMeth ){ *ppMeth = 0; }` |
|   1255 | 1063 | `	if( ppHost ){ *ppHost = 0; }` |
|   1255 | 1064 | `	if( ppClosure ){ *ppClosure = 0; }` |
|   1255 | 1065 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
|    347 | 1066 | `		ph7_class *pClass = ReflectResolveClass(pVm, pTarget);` |
|      - | 1067 | `		ph7_class_method *pMeth;` |
|    347 | 1068 | `		if( pClass == 0 ){` |
|    ! 0 | 1069 | `			return 0;` |
|      - | 1070 | `		}` |
|    518 | 1071 | `		pMeth = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|    171 | 1072 | `			SyBlobLength(&pMethodArg->sBlob));` |
|    347 | 1073 | `		if( pMeth == 0 ){` |
|      - | 1074 | `			/* getMethods()/ReflectionClass reports a base class's PRIVATE methods on` |
|      - | 1075 | `			 * the subclass (php copies them into the child's table), but private` |
|      - | 1076 | `			 * methods are not inherited into the child's method table, so the plain` |
|      - | 1077 | `			 * extract misses them when a ReflectionMethod obtained from the subclass` |
|      - | 1078 | `			 * is re-resolved by (subclass, name). Walk the base chain to find the` |
|      - | 1079 | `			 * declaring class's own copy. */` |
|    ! 0 | 1080 | `			ph7_class *pWalk = pClass->pBase;` |
|    ! 0 | 1081 | `			while( pWalk && pMeth == 0 ){` |
|    ! 0 | 1082 | `				pMeth = PH7_ClassExtractMethod(pWalk, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|    ! 0 | 1083 | `					SyBlobLength(&pMethodArg->sBlob));` |
|    ! 0 | 1084 | `				pWalk = pWalk->pBase;` |
|    ! 0 | 1085 | `			}` |
|    ! 0 | 1086 | `		}` |
|    347 | 1087 | `		if( pMeth == 0 ){` |
|    ! 0 | 1088 | `			return 0;` |
|      - | 1089 | `		}` |
|    347 | 1090 | `		if( ppClass ){ *ppClass = pClass; }` |
|    347 | 1091 | `		if( ppMeth ){ *ppMeth = pMeth; }` |
|    347 | 1092 | `		return &pMeth->sFunc;` |
|      - | 1093 | `	}` |
|      - | 1094 | `	{` |
|    910 | 1095 | `		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|    910 | 1096 | `		if( pClo ){` |
|      - | 1097 | `			SyString sAttr;` |
|      - | 1098 | `			ph7_value *pFn;` |
|     78 | 1099 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|     78 | 1100 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|     78 | 1101 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) < 1 ){` |
|    ! 0 | 1102 | `				return 0;` |
|      - | 1103 | `			}` |
|      - | 1104 | `			/* A closure over an object method or __invoke object` |
|      - | 1105 | `			 * (Closure::fromCallable([$obj,'m']) / fromCallable($invokeObj)) stores the` |
|      - | 1106 | `			 * bare method name in $__fn and the class in $__scope. Resolve it as a METHOD` |
|      - | 1107 | `			 * of $__scope FIRST — before the global function table — so a same-named` |
|      - | 1108 | `			 * global function does not shadow the method (the bug: $__fn "add" hitting a` |
|      - | 1109 | `			 * global add()). A plain anonymous closure's $__fn is its unique lambda name,` |
|      - | 1110 | `			 * which is not a method, so this falls through cleanly. */` |
|      - | 1111 | `			{` |
|      - | 1112 | `				SyString sScope;` |
|      - | 1113 | `				ph7_value *pScope;` |
|     78 | 1114 | `				SyStringInitFromBuf(&sScope, "__scope", 7);` |
|     78 | 1115 | `				pScope = PH7_ClassInstanceFetchAttr(pClo, &sScope);` |
|     78 | 1116 | `				if( pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0 ){` |
|     31 | 1117 | `					ph7_class *pScopeCls = PH7_VmExtractClass(pVm,` |
|     20 | 1118 | `						(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|     21 | 1119 | `					if( pScopeCls ){` |
|     31 | 1120 | `						ph7_class_method *pScopeMeth = PH7_ClassExtractMethod(pScopeCls,` |
|     20 | 1121 | `							(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|     21 | 1122 | `						if( pScopeMeth ){` |
|     21 | 1123 | `							if( ppClass ){ *ppClass = pScopeCls; }` |
|     21 | 1124 | `							if( ppMeth ){ *ppMeth = pScopeMeth; }` |
|     21 | 1125 | `							if( ppClosure ){ *ppClosure = pClo; }` |
|     21 | 1126 | `							return &pScopeMeth->sFunc;` |
|      - | 1127 | `						}` |
|    ! 0 | 1128 | `					}` |
|    ! 0 | 1129 | `				}` |
|      - | 1130 | `			}` |
|     58 | 1131 | `			pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|     58 | 1132 | `			if( pEntry == 0 ){` |
|      - | 1133 | `				/* A Closure over a host function (Closure::fromCallable('strlen')) */` |
|    ! 0 | 1134 | `				pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    ! 0 | 1135 | `				if( pEntry && ppHost ){` |
|    ! 0 | 1136 | `					*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|    ! 0 | 1137 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|    ! 0 | 1138 | `				}` |
|    ! 0 | 1139 | `				return 0;` |
|      - | 1140 | `			}` |
|     58 | 1141 | `			if( ppClosure ){ *ppClosure = pClo; }` |
|     58 | 1142 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|      - | 1143 | `		}` |
|      - | 1144 | `	}` |
|    834 | 1145 | `	if( pTarget->iFlags & MEMOBJ_STRING ){` |
|    834 | 1146 | `		if( SyBlobLength(&pTarget->sBlob) < 1 ){` |
|    ! 0 | 1147 | `			return 0;` |
|      - | 1148 | `		}` |
|    834 | 1149 | `		pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|    834 | 1150 | `		if( pEntry ){` |
|    702 | 1151 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|      - | 1152 | `		}` |
|    134 | 1153 | `		pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|    134 | 1154 | `		if( pEntry && ppHost ){` |
|    130 | 1155 | `			*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|     64 | 1156 | `		}` |
|     66 | 1157 | `	}` |
|    134 | 1158 | `	return 0;` |
|    630 | 1159 | `}` |
|      - | 1160 | `/* Emit the shared descriptor fields of a compiled function. */` |
|   1066 | 1161 | `static void ReflectFillFuncCommon(ph7_context *pCtx, ph7_value *pInfo, ph7_vm_func *pFunc)` |
|      5 | 1162 | `{` |
|      - | 1163 | `	ph7_vm_func_arg *aArg;` |
|      - | 1164 | `	ph7_value *pParams, *pStatics;` |
|   1071 | 1165 | `	int bVariadic = 0;` |
|      - | 1166 | `	int bAnon;` |
|      - | 1167 | `	sxu32 n;` |
|      - | 1168 | ``	/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|      - | 1169 | `	 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|   1071 | 1170 | `	bAnon = (pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|   1066 | 1171 | `	if( !bAnon && SyStringLength(&pFunc->sName) > 9` |
|    623 | 1172 | `	 && (SyMemcmp(SyStringData(&pFunc->sName), "[lambda_", 8) == 0` |
|    216 | 1173 | `	  \|\| SyMemcmp(SyStringData(&pFunc->sName), "[closure_", 9) == 0) ){` |
|      5 | 1174 | `		bAnon = 1;` |
|      2 | 1175 | `	}` |
|   1071 | 1176 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|   1071 | 1177 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pFunc->iFlags & VM_FUNC_INTERNAL) != 0);` |
|   1071 | 1178 | `	ReflectMapAddBool(pCtx, pInfo, "closure", bAnon);` |
|   1071 | 1179 | `	ReflectMapAddBool(pCtx, pInfo, "fstatic", (pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);` |
|   1071 | 1180 | `	ReflectMapAddBool(pCtx, pInfo, "byref", (pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|   1071 | 1181 | `	ReflectMapAddBool(pCtx, pInfo, "generator", (pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|   1071 | 1182 | `	ReflectMapAddBool(pCtx, pInfo, "strict", pFunc->bStrictTypes != 0);` |
|   1071 | 1183 | `	if( SyStringLength(&pFunc->sFile) > 0 ){` |
|    855 | 1184 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pFunc->sFile), (int)SyStringLength(&pFunc->sFile));` |
|    430 | 1185 | `	}else{` |
|    217 | 1186 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|      - | 1187 | `	}` |
|   1071 | 1188 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pFunc->nLine);` |
|   1071 | 1189 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pFunc->nEndLine);` |
|   1071 | 1190 | `	ReflectMapAddDoc(pCtx, pInfo, &pFunc->sDoc);` |
|   1071 | 1191 | `	ReflectMapAddAttrs(pCtx, pInfo, &pFunc->aAttrs);` |
|   1071 | 1192 | `	if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|    211 | 1193 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", SyStringData(&pFunc->sReturnTypeName),` |
|    138 | 1194 | `			(int)SyStringLength(&pFunc->sReturnTypeName));` |
|   1001 | 1195 | `	}else if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|      - | 1196 | `		/* The type-text renderer omits void/never atoms (compile.c notes the` |
|      - | 1197 | `		 * root fix belongs there); name them here for getReturnType(). */` |
|      3 | 1198 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "void", sizeof("void")-1);` |
|    931 | 1199 | `	}else if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|      3 | 1200 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "never", sizeof("never")-1);` |
|      2 | 1201 | `	}else{` |
|    928 | 1202 | `		ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|      - | 1203 | `	}` |
|   1071 | 1204 | `	ReflectMapAddBool(pCtx, pInfo, "retnullable", (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) != 0);` |
|      - | 1205 | `	/* Parameters */` |
|   1071 | 1206 | `	pParams = ph7_context_new_array(pCtx);` |
|   1071 | 1207 | `	aArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|   2989 | 1208 | `	for( n = 0 ; pParams && n < SySetUsed(&pFunc->aArgs) ; n++ ){` |
|   1921 | 1209 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|   1921 | 1210 | `		if( pMeta == 0 ){ break; }` |
|   1921 | 1211 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aArg[n].sName), (int)SyStringLength(&aArg[n].sName));` |
|   1921 | 1212 | `		ReflectMapAddInt(pCtx, pMeta, "pos", (sxi64)n);` |
|   1921 | 1213 | `		ReflectMapAddBool(pCtx, pMeta, "byref", (aArg[n].iFlags & VM_FUNC_ARG_BY_REF) != 0);` |
|   1921 | 1214 | `		ReflectMapAddBool(pCtx, pMeta, "variadic", (aArg[n].iFlags & VM_FUNC_ARG_VARIADIC) != 0);` |
|      - | 1215 | `		/* The compiler never sets ARG_HAS_DEF; a default = compiled bytecode` |
|      - | 1216 | `		 * (same test the OP_CALL default-value path uses). */` |
|   1921 | 1217 | `		ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&aArg[n].aByteCode) > 0);` |
|   1921 | 1218 | `		ReflectMapAddBool(pCtx, pMeta, "nullable", (aArg[n].iFlags & VM_FUNC_ARG_NULLABLE) != 0);` |
|   1921 | 1219 | `		ReflectMapAddBool(pCtx, pMeta, "promoted", (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) != 0);` |
|   1921 | 1220 | `		if( SyStringLength(&aArg[n].sTypeName) > 0 ){` |
|   1226 | 1221 | `			ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&aArg[n].sTypeName),` |
|    816 | 1222 | `				(int)SyStringLength(&aArg[n].sTypeName));` |
|    410 | 1223 | `		}else{` |
|   1104 | 1224 | `			ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|      - | 1225 | `		}` |
|   1921 | 1226 | `		ReflectMapAddAttrs(pCtx, pMeta, &aArg[n].aAttrs);` |
|   1921 | 1227 | `		ph7_array_add_elem(pParams, 0, pMeta);` |
|   1921 | 1228 | `		if( aArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|    209 | 1229 | `			bVariadic = 1;` |
|    104 | 1230 | `		}` |
|    962 | 1231 | `	}` |
|   1071 | 1232 | `	if( pParams ){` |
|   1071 | 1233 | `		ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|    533 | 1234 | `	}` |
|   1071 | 1235 | `	ReflectMapAddBool(pCtx, pInfo, "variadic", bVariadic);` |
|      - | 1236 | `	/* Static variables: current value when the slot was initialized (first` |
|      - | 1237 | `	 * call), otherwise the evaluated default — PHP's getStaticVariables` |
|      - | 1238 | `	 * initializes on demand and reports the same values. */` |
|   1071 | 1239 | `	pStatics = ph7_context_new_array(pCtx);` |
|   1071 | 1240 | `	if( pStatics ){` |
|   1071 | 1241 | `		ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|   1105 | 1242 | `		for( n = 0 ; n < SySetUsed(&pFunc->aStatic) ; n++ ){` |
|     35 | 1243 | `			ph7_value *pVal = 0;` |
|      - | 1244 | `			ph7_value sScratch;` |
|     35 | 1245 | `			int bScratch = 0;` |
|     35 | 1246 | `			if( aStatic[n].nIdx != SXU32_HIGH ){` |
|     17 | 1247 | `				pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);` |
|      8 | 1248 | `			}` |
|     35 | 1249 | `			if( pVal == 0 ){` |
|     19 | 1250 | `				PH7_MemObjInit(pCtx->pVm, &sScratch);` |
|     19 | 1251 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|     19 | 1252 | `					VmLocalExec(pCtx->pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|      9 | 1253 | `				}` |
|     19 | 1254 | `				pVal = &sScratch;` |
|     19 | 1255 | `				bScratch = 1;` |
|      9 | 1256 | `			}` |
|     35 | 1257 | `			ReflectMapAddDyn(pCtx, pStatics, &aStatic[n].sName, pVal);` |
|     35 | 1258 | `			if( bScratch ){` |
|     19 | 1259 | `				PH7_MemObjRelease(&sScratch);` |
|      9 | 1260 | `			}` |
|     18 | 1261 | `		}` |
|   1071 | 1262 | `		ph7_array_add_strkey_elem(pInfo, "statics", pStatics);` |
|    533 | 1263 | `	}` |
|   1071 | 1264 | `}` |
|      - | 1265 | `/*` |
|      - | 1266 | ` * array\|null __reflect_func_info(string\|Closure $target [, string $method])` |
|      - | 1267 | ` * Function/method/closure descriptor for the PHP layer.` |
|      - | 1268 | ` */` |
|   1198 | 1269 | `static int vm_builtin_reflect_func_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      5 | 1270 | `{` |
|      - | 1271 | `	ph7_vm_func *pFunc;` |
|   1203 | 1272 | `	ph7_class *pClass = 0;` |
|   1203 | 1273 | `	ph7_class_method *pMeth = 0;` |
|   1203 | 1274 | `	ph7_user_func *pHost = 0;` |
|   1203 | 1275 | `	ph7_class_instance *pClosure = 0;` |
|      - | 1276 | `	ph7_value *pInfo;` |
|   1203 | 1277 | `	if( nArg < 1 ){` |
|    ! 0 | 1278 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1279 | `		return PH7_OK;` |
|      - | 1280 | `	}` |
|   1203 | 1281 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], nArg > 1 ? apArg[1] : 0,` |
|      - | 1282 | `		&pClass, &pMeth, &pHost, &pClosure);` |
|   1203 | 1283 | `	if( pFunc == 0 && pHost == 0 ){` |
|      6 | 1284 | `		ph7_result_null(pCtx);` |
|      6 | 1285 | `		return PH7_OK;` |
|      - | 1286 | `	}` |
|   1199 | 1287 | `	pInfo = ph7_context_new_array(pCtx);` |
|   1199 | 1288 | `	if( pInfo == 0 ){` |
|    ! 0 | 1289 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1290 | `		return PH7_OK;` |
|      - | 1291 | `	}` |
|   1199 | 1292 | `	if( pFunc == 0 ){` |
|      - | 1293 | `		/* Host (C builtin) function: no parameter metadata beyond arity */` |
|    130 | 1294 | `		ph7_value *pParams = ph7_context_new_array(pCtx);` |
|    130 | 1295 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pHost->sName), (int)SyStringLength(&pHost->sName));` |
|    130 | 1296 | `		ReflectMapAddBool(pCtx, pInfo, "internal", 1);` |
|    130 | 1297 | `		ReflectMapAddBool(pCtx, pInfo, "closure", 0);` |
|    130 | 1298 | `		ReflectMapAddBool(pCtx, pInfo, "fstatic", 0);` |
|    130 | 1299 | `		ReflectMapAddBool(pCtx, pInfo, "byref", 0);` |
|    130 | 1300 | `		ReflectMapAddBool(pCtx, pInfo, "generator", 0);` |
|    130 | 1301 | `		ReflectMapAddBool(pCtx, pInfo, "strict", 0);` |
|    130 | 1302 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|    130 | 1303 | `		ReflectMapAddInt(pCtx, pInfo, "line", 0);` |
|    130 | 1304 | `		ReflectMapAddInt(pCtx, pInfo, "endline", 0);` |
|    130 | 1305 | `		ReflectMapAddBool(pCtx, pInfo, "doc", 0);` |
|      - | 1306 | `		{` |
|    130 | 1307 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|    130 | 1308 | `			if( pEmpty ){` |
|    130 | 1309 | `				ph7_array_add_strkey_elem(pInfo, "attrs", pEmpty);` |
|     64 | 1310 | `			}` |
|      - | 1311 | `		}` |
|    130 | 1312 | `		if( pHost->zRet ){` |
|    130 | 1313 | `			ReflectMapAddStr(pCtx, pInfo, "rettext", pHost->zRet, (int)SyStrlen(pHost->zRet));` |
|     66 | 1314 | `		}else{` |
|    ! 0 | 1315 | `			ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|      - | 1316 | `		}` |
|    130 | 1317 | `		ReflectMapAddBool(pCtx, pInfo, "retnullable", 0);` |
|    130 | 1318 | `		if( pParams ){` |
|    130 | 1319 | `			ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|     64 | 1320 | `		}` |
|    130 | 1321 | `		ReflectMapAddBool(pCtx, pInfo, "variadic", 0);` |
|    130 | 1322 | `		ReflectMapAddInt(pCtx, pInfo, "minarg", (sxi64)pHost->nMinArg);` |
|    130 | 1323 | `		if( pHost->zSig ){` |
|    130 | 1324 | `			ReflectMapAddStr(pCtx, pInfo, "sig", pHost->zSig, (int)SyStrlen(pHost->zSig));` |
|     66 | 1325 | `		}else{` |
|    ! 0 | 1326 | `			ReflectMapAddStr(pCtx, pInfo, "sig", "", 0);` |
|      - | 1327 | `		}` |
|    130 | 1328 | `		ph7_result_value(pCtx, pInfo);` |
|    130 | 1329 | `		return PH7_OK;` |
|      - | 1330 | `	}` |
|   1071 | 1331 | `	ReflectFillFuncCommon(pCtx, pInfo, pFunc);` |
|   1071 | 1332 | `	ReflectMapAddInt(pCtx, pInfo, "minarg", -1);` |
|   1071 | 1333 | `	if( (pFunc->iFlags & VM_FUNC_INTERNAL) && SySetUsed(&pFunc->aArgs) == 0 && pMeth == 0 ){` |
|      - | 1334 | `		/* Embedded-PHP builtin (max/min...): declared argless, actual` |
|      - | 1335 | `		 * signature comes from the static table */` |
|    ! 0 | 1336 | `		const char *zRet = 0;` |
|    ! 0 | 1337 | `		const char *zSig = PH7_VmBuiltinSigLookup(SyStringData(&pFunc->sName), SyStringLength(&pFunc->sName), &zRet);` |
|    ! 0 | 1338 | `		if( zSig ){` |
|    ! 0 | 1339 | `			ReflectMapAddStr(pCtx, pInfo, "sig", zSig, (int)SyStrlen(zSig));` |
|    ! 0 | 1340 | `		}` |
|    ! 0 | 1341 | `		if( zRet && SyStringLength(&pFunc->sReturnTypeName) == 0 ){` |
|    ! 0 | 1342 | `			ReflectMapAddStr(pCtx, pInfo, "ret2", zRet, (int)SyStrlen(zRet));` |
|    ! 0 | 1343 | `		}` |
|    ! 0 | 1344 | `	}` |
|   1071 | 1345 | `	if( pMeth && pClass ){` |
|    339 | 1346 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|    339 | 1347 | `		ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|    339 | 1348 | `		ReflectMapAddStr(pCtx, pInfo, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|    339 | 1349 | `		ReflectMapAddInt(pCtx, pInfo, "vis", (sxi64)pMeth->iProtection);` |
|    339 | 1350 | `		ReflectMapAddBool(pCtx, pInfo, "mstatic", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|    339 | 1351 | `		ReflectMapAddBool(pCtx, pInfo, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|    339 | 1352 | `		ReflectMapAddBool(pCtx, pInfo, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|    167 | 1353 | `	}` |
|   1071 | 1354 | `	if( pClosure ){` |
|      - | 1355 | `		SyString sAttr;` |
|      - | 1356 | `		ph7_value *pAttr;` |
|      - | 1357 | `		ph7_value *pUsed;` |
|     74 | 1358 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|     74 | 1359 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|     84 | 1360 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|     21 | 1361 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|     21 | 1362 | `			if( pKey ){` |
|     21 | 1363 | `				ph7_value_string(pKey, "this", 4);` |
|     21 | 1364 | `				ph7_array_add_elem(pInfo, pKey, pAttr);` |
|     10 | 1365 | `			}` |
|     11 | 1366 | `		}else{` |
|     54 | 1367 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|      - | 1368 | `		}` |
|     74 | 1369 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|     74 | 1370 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|     74 | 1371 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|     31 | 1372 | `			ReflectMapAddStr(pCtx, pInfo, "scope", (const char *)SyBlobData(&pAttr->sBlob),` |
|     20 | 1373 | `				(int)SyBlobLength(&pAttr->sBlob));` |
|     11 | 1374 | `		}else{` |
|     54 | 1375 | `			ReflectMapAddNull(pCtx, pInfo, "scope");` |
|      - | 1376 | `		}` |
|      - | 1377 | `		/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */` |
|     74 | 1378 | `		pUsed = ph7_context_new_array(pCtx);` |
|     74 | 1379 | `		if( pUsed ){` |
|     74 | 1380 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|      - | 1381 | `			sxu32 n;` |
|    136 | 1382 | `			for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; n++ ){` |
|     64 | 1383 | `				if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){` |
|     46 | 1384 | `					continue;` |
|      - | 1385 | `				}` |
|     18 | 1386 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|     10 | 1387 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|    ! 0 | 1388 | `					continue;` |
|      - | 1389 | `				}` |
|     19 | 1390 | `				if( (aEnv[n].iFlags & VM_FUNC_ARG_BY_REF) && aEnv[n].nIdx != SXU32_HIGH ){` |
|      - | 1391 | `					/* Captured by reference: report the slot's live value */` |
|      5 | 1392 | `					ph7_value *pLive = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aEnv[n].nIdx);` |
|      5 | 1393 | `					ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, pLive ? pLive : &aEnv[n].sValue);` |
|      5 | 1394 | `					continue;` |
|      - | 1395 | `				}` |
|     15 | 1396 | `				ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, &aEnv[n].sValue);` |
|      8 | 1397 | `			}` |
|     74 | 1398 | `			ph7_array_add_strkey_elem(pInfo, "used", pUsed);` |
|     36 | 1399 | `		}` |
|     36 | 1400 | `	}` |
|   1071 | 1401 | `	ph7_result_value(pCtx, pInfo);` |
|   1071 | 1402 | `	return PH7_OK;` |
|    604 | 1403 | `}` |
|      - | 1404 | `/*` |
|      - | 1405 | ` * mixed __reflect_param_default(string\|Closure $target, ?string $method, int $idx)` |
|      - | 1406 | ` * Evaluate a parameter's compiled default expression.` |
|      - | 1407 | ` */` |
|     12 | 1408 | `static int vm_builtin_reflect_param_default(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1409 | `{` |
|      - | 1410 | `	ph7_vm_func *pFunc;` |
|      - | 1411 | `	ph7_vm_func_arg *pArg;` |
|      - | 1412 | `	ph7_value sValue;` |
|      - | 1413 | `	sxu32 nIdx;` |
|     13 | 1414 | `	if( nArg < 3 ){` |
|    ! 0 | 1415 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1416 | `		return PH7_OK;` |
|      - | 1417 | `	}` |
|     13 | 1418 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|     13 | 1419 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|     12 | 1420 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|     13 | 1421 | `	 \|\| SySetUsed(&pArg->aByteCode) < 1 ){` |
|    ! 0 | 1422 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1423 | `		return PH7_OK;` |
|      - | 1424 | `	}` |
|     13 | 1425 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     13 | 1426 | `	VmLocalExec(pCtx->pVm, &pArg->aByteCode, &sValue, FALSE);` |
|     13 | 1427 | `	ph7_result_value(pCtx, &sValue);` |
|     13 | 1428 | `	PH7_MemObjRelease(&sValue);` |
|     13 | 1429 | `	return PH7_OK;` |
|      7 | 1430 | `}` |
|      - | 1431 | `/*` |
|      - | 1432 | ` * string\|null __reflect_param_defconst(string\|Closure $target, ?string $method, int $idx)` |
|      - | 1433 | ` * When a parameter's default is a plain global-constant reference, its` |
|      - | 1434 | ` * source name; null otherwise. A constant default compiles to exactly` |
|      - | 1435 | ` * [ OP_LOADC (EXPAND) , OP_DONE ] with the name in the literal table.` |
|      - | 1436 | ` */` |
|      6 | 1437 | `static int vm_builtin_reflect_param_defconst(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1438 | `{` |
|      - | 1439 | `	ph7_vm_func *pFunc;` |
|      - | 1440 | `	ph7_vm_func_arg *pArg;` |
|      - | 1441 | `	VmInstr *aInstr;` |
|      - | 1442 | `	ph7_value *pLit;` |
|      - | 1443 | `	sxu32 nIdx;` |
|      7 | 1444 | `	if( nArg < 3 ){` |
|    ! 0 | 1445 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1446 | `		return PH7_OK;` |
|      - | 1447 | `	}` |
|      7 | 1448 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|      7 | 1449 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|      6 | 1450 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|      7 | 1451 | `	 \|\| SySetUsed(&pArg->aByteCode) != 2 ){` |
|      3 | 1452 | `		ph7_result_null(pCtx);` |
|      3 | 1453 | `		return PH7_OK;` |
|      - | 1454 | `	}` |
|      5 | 1455 | `	aInstr = (VmInstr *)SySetBasePtr(&pArg->aByteCode);` |
|      4 | 1456 | `	if( aInstr[0].iOp != PH7_OP_LOADC \|\| (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0` |
|      5 | 1457 | `	 \|\| aInstr[1].iOp != PH7_OP_DONE ){` |
|    ! 0 | 1458 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1459 | `		return PH7_OK;` |
|      - | 1460 | `	}` |
|      5 | 1461 | `	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);` |
|      5 | 1462 | `	if( pLit == 0 \|\| SyBlobLength(&pLit->sBlob) < 1 ){` |
|    ! 0 | 1463 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1464 | `		return PH7_OK;` |
|      - | 1465 | `	}` |
|      5 | 1466 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&pLit->sBlob), (int)SyBlobLength(&pLit->sBlob));` |
|      5 | 1467 | `	return PH7_OK;` |
|      4 | 1468 | `}` |
|      - | 1469 | `/*` |
|      - | 1470 | ` * mixed __reflect_invoke(mixed $target, ?string $method, ?object $this, array $args)` |
|      - | 1471 | ` * Visibility-bypassing invocation (methods dispatch by VM name; functions` |
|      - | 1472 | ` * and closures ride PH7_VmCallUserFunction like call_user_func_array).` |
|      - | 1473 | ` */` |
|     20 | 1474 | `static int vm_builtin_reflect_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1475 | `{` |
|     21 | 1476 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1477 | `	ph7_value sResult;` |
|      - | 1478 | `	SySet aCallArg;` |
|      - | 1479 | `	sxi32 rc;` |
|     21 | 1480 | `	if( nArg < 4 ){` |
|    ! 0 | 1481 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1482 | `		return PH7_OK;` |
|      - | 1483 | `	}` |
|     21 | 1484 | `	PH7_MemObjInit(pVm, &sResult);` |
|     21 | 1485 | `	sResult.nIdx = SXU32_HIGH;` |
|     21 | 1486 | `	SySetInit(&aCallArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|     21 | 1487 | `	ReflectCollectArgs(pCtx, apArg[3], &aCallArg, 0);` |
|     21 | 1488 | `	if( (apArg[1]->iFlags & MEMOBJ_STRING) && SyBlobLength(&apArg[1]->sBlob) > 0 ){` |
|     11 | 1489 | `		ph7_class *pClass = 0;` |
|     11 | 1490 | `		ph7_class_method *pMeth = 0;` |
|     11 | 1491 | `		ph7_class_instance *pThis = 0;` |
|     11 | 1492 | `		ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, 0);` |
|     11 | 1493 | `		if( pMeth == 0 ){` |
|    ! 0 | 1494 | `			SySetRelease(&aCallArg);` |
|    ! 0 | 1495 | `			PH7_MemObjRelease(&sResult);` |
|    ! 0 | 1496 | `			ph7_result_null(pCtx);` |
|    ! 0 | 1497 | `			return PH7_OK;` |
|      - | 1498 | `		}` |
|     11 | 1499 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|      7 | 1500 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|      3 | 1501 | `		}` |
|      - | 1502 | `		/* Reflection ignores method visibility (PHP 8.1+); the flag is` |
|      - | 1503 | `		 * consumed by the first OP_CALL, i.e. this synthetic one. */` |
|     11 | 1504 | `		pVm->bReflectBypass = 1;` |
|     16 | 1505 | `		rc = PH7_VmCallClassMethod(pVm, pThis, pMeth, &sResult,` |
|     10 | 1506 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg));` |
|     11 | 1507 | `		pVm->bReflectBypass = 0;` |
|      6 | 1508 | `	}else{` |
|     16 | 1509 | `		rc = PH7_VmCallUserFunction(pVm, apArg[0],` |
|     10 | 1510 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg), &sResult);` |
|      - | 1511 | `	}` |
|     21 | 1512 | `	SySetRelease(&aCallArg);` |
|     21 | 1513 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|    ! 0 | 1514 | `		PH7_MemObjRelease(&sResult);` |
|    ! 0 | 1515 | `		return rc;` |
|      - | 1516 | `	}` |
|     21 | 1517 | `	ph7_result_value(pCtx, &sResult);` |
|     21 | 1518 | `	PH7_MemObjRelease(&sResult);` |
|     21 | 1519 | `	return PH7_OK;` |
|     11 | 1520 | `}` |
|      - | 1521 | `/*` |
|      - | 1522 | ` * Closure __reflect_closure(mixed $target, ?string $method, ?object $this)` |
|      - | 1523 | ` * Mint a Closure for a function or method, bound and scoped like the` |
|      - | 1524 | ` * first-class-callable path.` |
|      - | 1525 | ` */` |
|      6 | 1526 | `static int vm_builtin_reflect_closure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1527 | `{` |
|      7 | 1528 | `	ph7_vm *pVm = pCtx->pVm;` |
|      7 | 1529 | `	ph7_class *pClass = 0;` |
|      7 | 1530 | `	ph7_class_method *pMeth = 0;` |
|      7 | 1531 | `	ph7_class_instance *pClosure = 0;` |
|      - | 1532 | `	ph7_vm_func *pFunc;` |
|      7 | 1533 | `	if( nArg < 3 ){` |
|    ! 0 | 1534 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1535 | `		return PH7_OK;` |
|      - | 1536 | `	}` |
|      7 | 1537 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, &pClosure);` |
|      7 | 1538 | `	if( pClosure ){` |
|      - | 1539 | `		/* Already a Closure: hand the same instance back */` |
|    ! 0 | 1540 | `		return ReflectResultExistingObject(pCtx, pClosure);` |
|      - | 1541 | `	}` |
|      7 | 1542 | `	if( pMeth && pClass ){` |
|      5 | 1543 | `		ph7_class_instance *pThis = 0;` |
|      5 | 1544 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|      3 | 1545 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|      1 | 1546 | `		}` |
|      7 | 1547 | `		return ReflectResultObject(pCtx,` |
|      4 | 1548 | `			PH7_VmNewClosure(pVm, &pMeth->sFunc.sName, pThis, &pClass->sName));` |
|      - | 1549 | `	}` |
|      3 | 1550 | `	if( pFunc ){` |
|      3 | 1551 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &pFunc->sName, 0, 0));` |
|      - | 1552 | `	}` |
|      - | 1553 | `	/* Host function by name */` |
|    ! 0 | 1554 | `	if( apArg[0]->iFlags & MEMOBJ_STRING ){` |
|      - | 1555 | `		SyString sName;` |
|    ! 0 | 1556 | `		SyStringInitFromBuf(&sName, (const char *)SyBlobData(&apArg[0]->sBlob), SyBlobLength(&apArg[0]->sBlob));` |
|    ! 0 | 1557 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &sName, 0, 0));` |
|      - | 1558 | `	}` |
|    ! 0 | 1559 | `	ph7_result_null(pCtx);` |
|    ! 0 | 1560 | `	return PH7_OK;` |
|      4 | 1561 | `}` |
|      - | 1562 | `/*` |
|      - | 1563 | ` * Resolve a Generator object into its wrapper. Mirrors the static` |
|      - | 1564 | ` * VmGeneratorExtractCtx in vm.c: the $__ctx attribute carries the` |
|      - | 1565 | ` * ph7_generator pointer as a resource value.` |
|      - | 1566 | ` */` |
|     22 | 1567 | `static ph7_generator * ReflectGeneratorCtx(ph7_vm *pVm, ph7_value *pVal)` |
|      1 | 1568 | `{` |
|      - | 1569 | `	ph7_class_instance *pThis;` |
|      - | 1570 | `	ph7_value *pAttr;` |
|      - | 1571 | `	SyString sAttr;` |
|     23 | 1572 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVm->pGeneratorClass == 0 ){` |
|    ! 0 | 1573 | `		return 0;` |
|      - | 1574 | `	}` |
|     23 | 1575 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|     23 | 1576 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|    ! 0 | 1577 | `		return 0;` |
|      - | 1578 | `	}` |
|     23 | 1579 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|     23 | 1580 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|     23 | 1581 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|    ! 0 | 1582 | `		return 0;` |
|      - | 1583 | `	}` |
|     23 | 1584 | `	return (ph7_generator *)pAttr->x.pOther;` |
|     12 | 1585 | `}` |
|      - | 1586 | `/*` |
|      - | 1587 | ` * array\|null __reflect_gen_info(Generator $g)` |
|      - | 1588 | ` * {state, closed, executing, kind ('fn'\|'method'), name, class?, this}` |
|      - | 1589 | ` */` |
|     16 | 1590 | `static int vm_builtin_reflect_gen_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1591 | `{` |
|     17 | 1592 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1593 | `	ph7_generator *pGen;` |
|      - | 1594 | `	ph7_exec_ctx *pExec;` |
|      - | 1595 | `	ph7_value *pInfo;` |
|     17 | 1596 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 \|\| pGen->pCtx == 0 ){` |
|    ! 0 | 1597 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1598 | `		return PH7_OK;` |
|      - | 1599 | `	}` |
|     17 | 1600 | `	pExec = pGen->pCtx;` |
|     17 | 1601 | `	pInfo = ph7_context_new_array(pCtx);` |
|     17 | 1602 | `	if( pInfo == 0 ){` |
|    ! 0 | 1603 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1604 | `		return PH7_OK;` |
|      - | 1605 | `	}` |
|     17 | 1606 | `	ReflectMapAddInt(pCtx, pInfo, "state", (sxi64)pExec->iState);` |
|     24 | 1607 | `	ReflectMapAddBool(pCtx, pInfo, "closed",` |
|     16 | 1608 | `		pExec->iState == PH7_CTX_STATE_COMPLETED \|\| pExec->iState == PH7_CTX_STATE_CLOSED);` |
|     17 | 1609 | `	ReflectMapAddBool(pCtx, pInfo, "executing", pVm->pActiveCtx == pExec);` |
|     17 | 1610 | `	if( pExec->pFunc ){` |
|     17 | 1611 | `		ph7_vm_func *pFunc = pExec->pFunc;` |
|     19 | 1612 | `		if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|      5 | 1613 | `			ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|      5 | 1614 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "method", sizeof("method")-1);` |
|      5 | 1615 | `			ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|      3 | 1616 | `		}else{` |
|     13 | 1617 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "fn", sizeof("fn")-1);` |
|      - | 1618 | `		}` |
|     17 | 1619 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|      8 | 1620 | `	}` |
|      - | 1621 | `	{` |
|      - | 1622 | `		/* The coroutine frame installs $this as a frame VARIABLE (see` |
|      - | 1623 | `		 * VmFiberSetupFrame), not as pFrame->pThis — check both. */` |
|     17 | 1624 | `		ph7_value *pThisVal = 0;` |
|     17 | 1625 | `		if( pExec->pFrame ){` |
|     17 | 1626 | `			SyHashEntry *pVar = SyHashGet(&pExec->pFrame->hVar, "this", sizeof("this")-1);` |
|     17 | 1627 | `			if( pVar ){` |
|      5 | 1628 | `				ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj, (sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|      5 | 1629 | `				if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|      5 | 1630 | `					pThisVal = pSlot;` |
|      2 | 1631 | `				}` |
|      2 | 1632 | `			}` |
|     17 | 1633 | `			if( pThisVal == 0 && pExec->pFrame->pThis ){` |
|      - | 1634 | `				ph7_value sThis;` |
|    ! 0 | 1635 | `				ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|    ! 0 | 1636 | `				PH7_MemObjInit(pVm, &sThis);` |
|    ! 0 | 1637 | `				pExec->pFrame->pThis->iRef++;` |
|    ! 0 | 1638 | `				sThis.x.pOther = pExec->pFrame->pThis;` |
|    ! 0 | 1639 | `				MemObjSetType(&sThis, MEMOBJ_OBJ);` |
|    ! 0 | 1640 | `				if( pKey ){` |
|    ! 0 | 1641 | `					ph7_value_string(pKey, "this", 4);` |
|    ! 0 | 1642 | `					ph7_array_add_elem(pInfo, pKey, &sThis); /* copies (takes its own ref) */` |
|    ! 0 | 1643 | `				}` |
|    ! 0 | 1644 | `				PH7_MemObjRelease(&sThis);` |
|    ! 0 | 1645 | `				pThisVal = (ph7_value *)1; /* handled */` |
|    ! 0 | 1646 | `			}` |
|      8 | 1647 | `		}` |
|     17 | 1648 | `		if( pThisVal == 0 ){` |
|     13 | 1649 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|     11 | 1650 | `		}else if( pThisVal != (ph7_value *)1 ){` |
|      5 | 1651 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|      5 | 1652 | `			if( pKey ){` |
|      5 | 1653 | `				ph7_value_string(pKey, "this", 4);` |
|      5 | 1654 | `				ph7_array_add_elem(pInfo, pKey, pThisVal);` |
|      2 | 1655 | `			}` |
|      2 | 1656 | `		}` |
|      - | 1657 | `	}` |
|     17 | 1658 | `	ph7_result_value(pCtx, pInfo);` |
|     17 | 1659 | `	return PH7_OK;` |
|      9 | 1660 | `}` |
|      - | 1661 | `/*` |
|      - | 1662 | ` * Generator __reflect_gen_exec(Generator $g)` |
|      - | 1663 | `` * Follow `yield from` delegation to the innermost executing generator`` |
|      - | 1664 | ` * (PHP's ReflectionGenerator::getExecutingGenerator).` |
|      - | 1665 | ` */` |
|      4 | 1666 | `static int vm_builtin_reflect_gen_exec(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1667 | `{` |
|      5 | 1668 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1669 | `	ph7_generator *pGen;` |
|      - | 1670 | `	ph7_value *pCur;` |
|      5 | 1671 | `	int iDepth = 0;` |
|      5 | 1672 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 ){` |
|    ! 0 | 1673 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1674 | `		return PH7_OK;` |
|      - | 1675 | `	}` |
|      5 | 1676 | `	pCur = apArg[0];` |
|      9 | 1677 | `	while( pGen && pGen->pCtx && pGen->pCtx->iDelegateState == 3` |
|      8 | 1678 | `	 && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|      3 | 1679 | `		ph7_generator *pInner = ReflectGeneratorCtx(pVm, &pGen->pCtx->sDelegate);` |
|      3 | 1680 | `		if( pInner == 0 ){` |
|    ! 0 | 1681 | `			break;` |
|      - | 1682 | `		}` |
|      3 | 1683 | `		pCur = &pGen->pCtx->sDelegate;` |
|      3 | 1684 | `		pGen = pInner;` |
|      3 | 1685 | `		iDepth++;` |
|      1 | 1686 | `	}` |
|      5 | 1687 | `	return ReflectResultExistingObject(pCtx, (ph7_class_instance *)pCur->x.pOther);` |
|      3 | 1688 | `}` |
|      - | 1689 | `/*` |
|      - | 1690 | ` * array\|null __reflect_const_info(string $name)` |
|      - | 1691 | ` * Global-constant descriptor: {value}. Null when undefined. File/origin` |
|      - | 1692 | ` * metadata arrives with the C5 constant-metadata work.` |
|      - | 1693 | ` */` |
|     40 | 1694 | `static int vm_builtin_reflect_const_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1695 | `{` |
|     41 | 1696 | `	ph7_vm *pVm = pCtx->pVm;` |
|      - | 1697 | `	SyHashEntry *pEntry;` |
|      - | 1698 | `	ph7_constant *pCons;` |
|      - | 1699 | `	ph7_value *pInfo;` |
|      - | 1700 | `	ph7_value sValue;` |
|      - | 1701 | `	const char *zName;` |
|      - | 1702 | `	int nLen;` |
|     41 | 1703 | `	if( nArg < 1 ){` |
|    ! 0 | 1704 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1705 | `		return PH7_OK;` |
|      - | 1706 | `	}` |
|     41 | 1707 | `	zName = ph7_value_to_string(apArg[0], &nLen);` |
|     41 | 1708 | `	pEntry = nLen > 0 ? SyHashGet(&pVm->hConstant, (const void *)zName, (sxu32)nLen) : 0;` |
|     41 | 1709 | `	if( pEntry == 0 ){` |
|      3 | 1710 | `		ph7_result_null(pCtx);` |
|      3 | 1711 | `		return PH7_OK;` |
|      - | 1712 | `	}` |
|     39 | 1713 | `	pCons = (ph7_constant *)pEntry->pUserData;` |
|     39 | 1714 | `	pInfo = ph7_context_new_array(pCtx);` |
|     39 | 1715 | `	if( pInfo == 0 ){` |
|    ! 0 | 1716 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1717 | `		return PH7_OK;` |
|      - | 1718 | `	}` |
|     39 | 1719 | `	PH7_MemObjInit(pVm, &sValue);` |
|     39 | 1720 | `	if( pCons->xExpand ){` |
|     39 | 1721 | `		pCons->xExpand(&sValue, pCons->pUserData);` |
|     19 | 1722 | `	}` |
|      - | 1723 | `	{` |
|     39 | 1724 | `		ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|     39 | 1725 | `		if( pKey ){` |
|     39 | 1726 | `			ph7_value_string(pKey, "value", 5);` |
|     39 | 1727 | `			ph7_array_add_elem(pInfo, pKey, &sValue);` |
|     19 | 1728 | `		}` |
|      - | 1729 | `	}` |
|     39 | 1730 | `	PH7_MemObjRelease(&sValue);` |
|     39 | 1731 | `	ReflectMapAddBool(pCtx, pInfo, "internal", pCons->bUserDefined == 0);` |
|     39 | 1732 | `	if( SyStringLength(&pCons->sFile) > 0 ){` |
|     25 | 1733 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));` |
|     13 | 1734 | `	}else{` |
|     15 | 1735 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|      - | 1736 | `	}` |
|     39 | 1737 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pCons->nLine);` |
|     39 | 1738 | `	ReflectMapAddAttrs(pCtx, pInfo, &pCons->aAttrs);` |
|     39 | 1739 | `	ph7_result_value(pCtx, pInfo);` |
|     39 | 1740 | `	return PH7_OK;` |
|     21 | 1741 | `}` |
|      - | 1742 | `/*` |
|      - | 1743 | ` * int\|null __reflect_ref_id(array $arr, int\|string $key)` |
|      - | 1744 | ` * The element's slot index when the element is a reference (its slot has` |
|      - | 1745 | ` * a reference-table record with at least two links), null otherwise.` |
|      - | 1746 | ` */` |
|      6 | 1747 | `static int vm_builtin_reflect_ref_id(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1748 | `{` |
|      - | 1749 | `	ph7_hashmap *pMap;` |
|      7 | 1750 | `	ph7_hashmap_node *pNode = 0;` |
|      7 | 1751 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|    ! 0 | 1752 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1753 | `		return PH7_OK;` |
|      - | 1754 | `	}` |
|      7 | 1755 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|      7 | 1756 | `	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK \|\| pNode == 0 ){` |
|    ! 0 | 1757 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1758 | `		return PH7_OK;` |
|      - | 1759 | `	}` |
|      7 | 1760 | `	if( PH7_VmSlotRefCount(pCtx->pVm, pNode->nValIdx) < 2 ){` |
|      3 | 1761 | `		ph7_result_null(pCtx);` |
|      3 | 1762 | `		return PH7_OK;` |
|      - | 1763 | `	}` |
|      5 | 1764 | `	ph7_result_int64(pCtx, (sxi64)pNode->nValIdx);` |
|      5 | 1765 | `	return PH7_OK;` |
|      4 | 1766 | `}` |
|      - | 1767 | `/*` |
|      - | 1768 | ` * array\|null __reflect_attr_args(string $kind, mixed $target, ?string $member,` |
|      - | 1769 | ` *                                int $paramIdx, int $attrIdx)` |
|      - | 1770 | ` * Evaluate the recorded argument expressions of one declared attribute:` |
|      - | 1771 | ` * kind 'class' (target = class), 'attr' (class + property/constant name),` |
|      - | 1772 | ` * 'method' (class + method), 'fn' (function name or Closure), 'param'` |
|      - | 1773 | ` * (function spec + parameter index). Named arguments become string keys.` |
|      - | 1774 | ` */` |
|     68 | 1775 | `static int vm_builtin_reflect_attr_args(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|      1 | 1776 | `{` |
|     69 | 1777 | `	ph7_vm *pVm = pCtx->pVm;` |
|     69 | 1778 | `	SySet *pAttrs = 0;` |
|     69 | 1779 | `	ph7_class *pDeclCls = 0; /* class an attribute is declared on: scope for self:: in its args */` |
|      - | 1780 | `	ph7_attribute *pAttrRec;` |
|      - | 1781 | `	ph7_value *pOut;` |
|      - | 1782 | `	const char *zKind;` |
|      - | 1783 | `	int nKind;` |
|      - | 1784 | `	sxu32 nAttrIdx, n;` |
|     69 | 1785 | `	if( nArg < 5 ){` |
|    ! 0 | 1786 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1787 | `		return PH7_OK;` |
|      - | 1788 | `	}` |
|     69 | 1789 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|     69 | 1790 | `	nAttrIdx = (sxu32)ph7_value_to_int(apArg[4]);` |
|     91 | 1791 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|     45 | 1792 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|     45 | 1793 | `		if( pClass ){ pAttrs = &pClass->aAttrs; pDeclCls = pClass; }` |
|     49 | 1794 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|      5 | 1795 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|      5 | 1796 | `		ph7_class_attr *pMember = pClass ? ReflectFetchMember(pClass, apArg[2]) : 0;` |
|      5 | 1797 | `		if( pMember ){ pAttrs = &pMember->aAttrs; pDeclCls = pClass; }` |
|     27 | 1798 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|      9 | 1799 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|      9 | 1800 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|     21 | 1801 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|      9 | 1802 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], 0, 0, 0, 0, 0);` |
|      9 | 1803 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|     10 | 1804 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|      3 | 1805 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|      3 | 1806 | `		ph7_vm_func_arg *pParam = pFunc` |
|      2 | 1807 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|      3 | 1808 | `		if( pParam ){ pAttrs = &pParam->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|      4 | 1809 | `	}else if( nKind == 5 && SyMemcmp(zKind, "const", 5) == 0 ){` |
|      - | 1810 | ``		/* Global constant (php 8.5 attributes on `const` statements) */`` |
|      - | 1811 | `		const char *zCName;` |
|      - | 1812 | `		int nCName;` |
|      - | 1813 | `		SyHashEntry *pCEntry;` |
|      3 | 1814 | `		zCName = ph7_value_to_string(apArg[1], &nCName);` |
|      3 | 1815 | `		pCEntry = nCName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zCName, (sxu32)nCName) : 0;` |
|      3 | 1816 | `		if( pCEntry ){ pAttrs = &((ph7_constant *)pCEntry->pUserData)->aAttrs; }` |
|      1 | 1817 | `	}` |
|     68 | 1818 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|     69 | 1819 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|    ! 0 | 1820 | `		ph7_result_null(pCtx);` |
|    ! 0 | 1821 | `		return PH7_OK;` |
|      - | 1822 | `	}` |
|    123 | 1823 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|     55 | 1824 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|      - | 1825 | `		ph7_value sValue;` |
|     55 | 1826 | `		PH7_MemObjInit(pVm, &sValue);` |
|     55 | 1827 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|      - | 1828 | `` 			/* Evaluate under the attribute's declaring-class scope so `self::class` `` |
|      - | 1829 | ``			 * / `self::CONST` / `parent::` resolve like php (else self:: would bind`` |
|      - | 1830 | `			 * to the reflection machinery's own class). */` |
|     55 | 1831 | `			PH7_VmExecAttrArg(pVm, &pArgRec->aByteCode, pDeclCls, &sValue);` |
|     27 | 1832 | `		}` |
|     55 | 1833 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|      5 | 1834 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|      3 | 1835 | `		}else{` |
|     51 | 1836 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|      - | 1837 | `		}` |
|     55 | 1838 | `		PH7_MemObjRelease(&sValue);` |
|     28 | 1839 | `	}` |
|     69 | 1840 | `	ph7_result_value(pCtx, pOut);` |
|     69 | 1841 | `	return PH7_OK;` |
|     35 | 1842 | `}` |
|   4528 | 1843 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|      5 | 1844 | `{` |
|      - | 1845 | `	static const struct {` |
|      - | 1846 | `		const char *zName;` |
|      - | 1847 | `		ProchHostFunction xFunc;` |
|      - | 1848 | `	} aFunc[] = {` |
|      - | 1849 | `		{ "__reflect_class_info",     vm_builtin_reflect_class_info },` |
|      - | 1850 | `		{ "__reflect_const_value",    vm_builtin_reflect_const_value },` |
|      - | 1851 | `		{ "__reflect_static_materialize", vm_builtin_reflect_static_materialize },` |
|      - | 1852 | `		{ "__reflect_static_value",   vm_builtin_reflect_static_value },` |
|      - | 1853 | `		{ "__reflect_static_set",     vm_builtin_reflect_static_set },` |
|      - | 1854 | `		{ "__reflect_prop_default",   vm_builtin_reflect_prop_default },` |
|      - | 1855 | `		{ "__reflect_new_instance",   vm_builtin_reflect_new_instance },` |
|      - | 1856 | `		{ "__reflect_new_no_ctor",    vm_builtin_reflect_new_no_ctor },` |
|      - | 1857 | `		{ "__reflect_func_info",      vm_builtin_reflect_func_info },` |
|      - | 1858 | `		{ "__reflect_param_default",  vm_builtin_reflect_param_default },` |
|      - | 1859 | `		{ "__reflect_param_defconst", vm_builtin_reflect_param_defconst },` |
|      - | 1860 | `		{ "__reflect_invoke",         vm_builtin_reflect_invoke },` |
|      - | 1861 | `		{ "__reflect_closure",        vm_builtin_reflect_closure },` |
|      - | 1862 | `		{ "__reflect_prop_read",      vm_builtin_reflect_prop_read },` |
|      - | 1863 | `		{ "__reflect_prop_write",     vm_builtin_reflect_prop_write },` |
|      - | 1864 | `		{ "__reflect_prop_state",     vm_builtin_reflect_prop_state },` |
|      - | 1865 | `		{ "__reflect_dyn_props",      vm_builtin_reflect_dyn_props },` |
|      - | 1866 | `		{ "__reflect_gen_info",       vm_builtin_reflect_gen_info },` |
|      - | 1867 | `		{ "__reflect_gen_exec",       vm_builtin_reflect_gen_exec },` |
|      - | 1868 | `		{ "__reflect_const_info",     vm_builtin_reflect_const_info },` |
|      - | 1869 | `		{ "__reflect_ref_id",         vm_builtin_reflect_ref_id },` |
|      - | 1870 | `		{ "__reflect_attr_args",      vm_builtin_reflect_attr_args },` |
|      - | 1871 | `	};` |
|      - | 1872 | `	sxu32 n;` |
| 104149 | 1873 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
|  99621 | 1874 | `		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);` |
|  49813 | 1875 | `	}` |
|   4533 | 1876 | `	return PH7_VmInstallReflectionLib(&(*pVm));` |
|      5 | 1877 | `}` |
|      - | 1878 |  |
