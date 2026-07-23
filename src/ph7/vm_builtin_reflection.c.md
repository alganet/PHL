# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1001/1172 lines (85.41%)

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
|  1258 |   31 | `static ph7_class * ReflectResolveClass(ph7_vm *pVm, ph7_value *pArg)` |
|     1 |   32 | `{` |
|     - |   33 | `	ph7_class *pClass;` |
|  1259 |   34 | `	pClass = PH7_VmExtractClassFromValue(pVm, pArg);` |
|  1259 |   35 | `	if( pClass == 0 && ph7_value_is_string(pArg) ){` |
|     - |   36 | `		const char *zName;` |
|     - |   37 | `		int nLen;` |
|    13 |   38 | `		zName = ph7_value_to_string(pArg, &nLen);` |
|    13 |   39 | `		if( nLen > 0 ){` |
|    13 |   40 | `			pClass = PH7_VmTriggerAutoload(pVm, zName, (sxu32)nLen, FALSE);` |
|     6 |   41 | `		}` |
|     6 |   42 | `	}` |
|  1259 |   43 | `	return pClass;` |
|     1 |   44 | `}` |
|     - |   45 | `/*` |
|     - |   46 | ` * Hand a freshly created class instance to the caller. The return slot` |
|     - |   47 | ` * takes over the initial reference from PH7_NewClassInstance (iRef=1):` |
|     - |   48 | ` * no extra iRef++ here (see the synthesized-object invariant — a stray` |
|     - |   49 | ` * bump leaks the object and disables its __destruct).` |
|     - |   50 | ` */` |
|    72 |   51 | `static int ReflectResultObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|     1 |   52 | `{` |
|    73 |   53 | `	if( pObj == 0 ){` |
|   ! 0 |   54 | `		ph7_result_null(pCtx);` |
|   ! 0 |   55 | `		return PH7_OK;` |
|     - |   56 | `	}` |
|    73 |   57 | `	PH7_MemObjRelease(pCtx->pRet);` |
|    73 |   58 | `	pCtx->pRet->x.pOther = pObj;` |
|    73 |   59 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|    73 |   60 | `	return PH7_OK;` |
|    37 |   61 | `}` |
|     - |   62 | `/* --- Marshaling helpers: build the descriptor arrays handed to the PHP layer --- */` |
| 35722 |   63 | `static void ReflectMapAddBool(ph7_context *pCtx, ph7_value *pMap, const char *zKey, int b)` |
|     1 |   64 | `{` |
| 35723 |   65 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 35723 |   66 | `	if( p == 0 ){ return; }` |
| 35723 |   67 | `	ph7_value_bool(p, b);` |
| 35723 |   68 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
| 17862 |   69 | `}` |
| 15694 |   70 | `static void ReflectMapAddInt(ph7_context *pCtx, ph7_value *pMap, const char *zKey, sxi64 iVal)` |
|     1 |   71 | `{` |
| 15695 |   72 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 15695 |   73 | `	if( p == 0 ){ return; }` |
| 15695 |   74 | `	ph7_value_int64(p, iVal);` |
| 15695 |   75 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  7848 |   76 | `}` |
| 10724 |   77 | `static void ReflectMapAddStr(ph7_context *pCtx, ph7_value *pMap, const char *zKey,` |
|     - |   78 | `	const char *zVal, int nVal)` |
|     1 |   79 | `{` |
| 10725 |   80 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 10725 |   81 | `	if( p == 0 ){ return; }` |
| 10725 |   82 | `	ph7_value_string(p, zVal, nVal);` |
| 10725 |   83 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  5363 |   84 | `}` |
|  3906 |   85 | `static void ReflectMapAddNull(ph7_context *pCtx, ph7_value *pMap, const char *zKey)` |
|     1 |   86 | `{` |
|  3907 |   87 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  3907 |   88 | `	if( p == 0 ){ return; }` |
|  3907 |   89 | `	ph7_value_null(p);` |
|  3907 |   90 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  1954 |   91 | `}` |
|     - |   92 | `/* Add an entry under a dynamic (SyString) key. */` |
|  4398 |   93 | `static void ReflectMapAddDyn(ph7_context *pCtx, ph7_value *pMap,` |
|     - |   94 | `	const SyString *pKey, ph7_value *pVal)` |
|     1 |   95 | `{` |
|  4399 |   96 | `	ph7_value *pK = ph7_context_new_scalar(pCtx);` |
|  4399 |   97 | `	if( pK == 0 ){ return; }` |
|  4399 |   98 | `	ph7_value_string(pK, pKey->zString, (int)pKey->nByte);` |
|  4399 |   99 | `	ph7_array_add_elem(pMap, pK, pVal);` |
|  2200 |  100 | `}` |
|     - |  101 | `/* Emit the declared #[...] attributes of a target as a summary list:` |
|     - |  102 | ` * [ {name, line} ... ]. Argument values stay lazy — the PHP layer pulls` |
|     - |  103 | ` * them through __reflect_attr_args when ReflectionAttribute needs them. */` |
|  5142 |  104 | `static void ReflectMapAddAttrs(ph7_context *pCtx, ph7_value *pMap, SySet *pAttrs)` |
|     1 |  105 | `{` |
|  5143 |  106 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|  5143 |  107 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|     - |  108 | `	sxu32 n;` |
|  5143 |  109 | `	if( pList == 0 ){` |
|   ! 0 |  110 | `		return;` |
|     - |  111 | `	}` |
|  5407 |  112 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|   265 |  113 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|   265 |  114 | `		if( pMeta == 0 ){ break; }` |
|   265 |  115 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aA[n].sName), (int)SyStringLength(&aA[n].sName));` |
|   265 |  116 | `		ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)aA[n].nLine);` |
|   265 |  117 | `		ph7_array_add_elem(pList, 0, pMeta);` |
|   133 |  118 | `	}` |
|  5143 |  119 | `	ph7_array_add_strkey_elem(pMap, "attrs", pList);` |
|  2572 |  120 | `}` |
|     - |  121 | `/* Emit a doc-comment field: the text when present, else boolean false` |
|     - |  122 | ` * (getDocComment()'s exact return contract). */` |
|  3890 |  123 | `static void ReflectMapAddDoc(ph7_context *pCtx, ph7_value *pMap, const SyString *pDoc)` |
|     1 |  124 | `{` |
|  3891 |  125 | `	if( SyStringLength(pDoc) > 0 ){` |
|    63 |  126 | `		ReflectMapAddStr(pCtx, pMap, "doc", SyStringData(pDoc), (int)SyStringLength(pDoc));` |
|    32 |  127 | `	}else{` |
|  3829 |  128 | `		ReflectMapAddBool(pCtx, pMap, "doc", 0);` |
|     - |  129 | `	}` |
|  3891 |  130 | `}` |
|     - |  131 | `/*` |
|     - |  132 | ` * Append pIface (and its parents / extended interfaces) to the dedup set` |
|     - |  133 | ` * of ph7_class pointers.` |
|     - |  134 | ` */` |
|   356 |  135 | `static void ReflectAddInterface(ph7_class *pIface, SySet *pOut, int iDepth)` |
|     1 |  136 | `{` |
|     - |  137 | `	ph7_class **apKnown;` |
|     - |  138 | `	sxu32 n;` |
|   357 |  139 | `	if( pIface == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|   ! 0 |  140 | `		return;` |
|     - |  141 | `	}` |
|     - |  142 | `	/* Parents of an interface come along too (interface B extends A) */` |
|   357 |  143 | `	if( pIface->pBase ){` |
|    43 |  144 | `		ReflectAddInterface(pIface->pBase, pOut, iDepth + 1);` |
|    21 |  145 | `	}` |
|     - |  146 | `	/* Some engines record extended interfaces in aInterface as well */` |
|   357 |  147 | `	apKnown = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|   357 |  148 | `	for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|   ! 0 |  149 | `		ReflectAddInterface(apKnown[n], pOut, iDepth + 1);` |
|   ! 0 |  150 | `	}` |
|     - |  151 | `	/* Dedup by pointer */` |
|   357 |  152 | `	apKnown = (ph7_class **)SySetBasePtr(pOut);` |
|   439 |  153 | `	for( n = 0 ; n < SySetUsed(pOut) ; n++ ){` |
|    93 |  154 | `		if( apKnown[n] == pIface ){` |
|    11 |  155 | `			return;` |
|     - |  156 | `		}` |
|    42 |  157 | `	}` |
|   347 |  158 | `	SySetPut(pOut, (const void *)&pIface);` |
|   179 |  159 | `}` |
|     - |  160 | `/*` |
|     - |  161 | ` * Collect the transitive set of interfaces implemented by pClass:` |
|     - |  162 | ` * the parent chain's interfaces first, then the class's own.` |
|     - |  163 | ` */` |
|  1086 |  164 | `static void ReflectCollectInterfaces(ph7_class *pClass, SySet *pOut, int iDepth)` |
|     1 |  165 | `{` |
|     - |  166 | `	ph7_class **apIface;` |
|     - |  167 | `	sxu32 n;` |
|  1087 |  168 | `	if( pClass == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|   ! 0 |  169 | `		return;` |
|     - |  170 | `	}` |
|  1087 |  171 | `	if( pClass->pBase ){` |
|   283 |  172 | `		ReflectCollectInterfaces(pClass->pBase, pOut, iDepth + 1);` |
|   141 |  173 | `	}` |
|  1087 |  174 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  1393 |  175 | `	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){` |
|   307 |  176 | `		ReflectAddInterface(apIface[n], pOut, iDepth + 1);` |
|   154 |  177 | `	}` |
|   544 |  178 | `}` |
|     - |  179 | `/*` |
|     - |  180 | ` * Deepest base class whose method table maps the same name to the very` |
|     - |  181 | ` * same ph7_class_method pointer: inheritance shares member pointers` |
|     - |  182 | ` * (PH7_ClassInherit), so this identifies the declaring class. Methods` |
|     - |  183 | ` * copied in from traits are not on the pBase chain and thus report the` |
|     - |  184 | ` * using class, which is what PHP reports too.` |
|     - |  185 | ` */` |
|  4212 |  186 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth)` |
|     1 |  187 | `{` |
|  4213 |  188 | `	ph7_class *pDecl = pClass;` |
|  4213 |  189 | `	ph7_class *pBase = pClass->pBase;` |
|  4213 |  190 | `	int iDepth = 0;` |
|  5293 |  191 | `	while( pBase && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|     - |  192 | `		SyHashEntry *pEntry;` |
|  4015 |  193 | `		pEntry = SyHashGet(&pBase->hMethod, (const void *)SyStringData(&pMeth->sFunc.sName),` |
|  1338 |  194 | `			SyStringLength(&pMeth->sFunc.sName));` |
|  2677 |  195 | `		if( pEntry == 0 \|\| (ph7_class_method *)pEntry->pUserData != pMeth ){` |
|   799 |  196 | `			break;` |
|     - |  197 | `		}` |
|  1081 |  198 | `		pDecl = pBase;` |
|  1081 |  199 | `		pBase = pBase->pBase;` |
|  1081 |  200 | `		iDepth++;` |
|     1 |  201 | `	}` |
|  4213 |  202 | `	return pDecl;` |
|     1 |  203 | `}` |
|     - |  204 | `/* Fetch a class attribute (property or constant) by plain name. */` |
|    72 |  205 | `static ph7_class_attr * ReflectFetchAttr(ph7_class *pClass, ph7_value *pName)` |
|     1 |  206 | `{` |
|     - |  207 | `	SyHashEntry *pEntry;` |
|     - |  208 | `	const char *zName;` |
|     - |  209 | `	int nLen;` |
|    73 |  210 | `	zName = ph7_value_to_string(pName, &nLen);` |
|    73 |  211 | `	if( nLen < 1 ){` |
|   ! 0 |  212 | `		return 0;` |
|     - |  213 | `	}` |
|    73 |  214 | `	pEntry = SyHashGet(&pClass->hAttr, (const void *)zName, (sxu32)nLen);` |
|    73 |  215 | `	if( pEntry == 0 ){` |
|   ! 0 |  216 | `		return 0;` |
|     - |  217 | `	}` |
|    73 |  218 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|    37 |  219 | `}` |
|     - |  220 | `/*` |
|     - |  221 | ` * array\|null __reflect_class_info(object\|string $target)` |
|     - |  222 | ` *` |
|     - |  223 | ` * Full class descriptor, or null when the class cannot be resolved (after` |
|     - |  224 | ` * an autoload attempt). Shape:` |
|     - |  225 | ` *   name, internal, interface, trait, abstract, final, readonly, iterable (bool),` |
|     - |  226 | ` *   parent (string\|null), interfaces (list), traits (list),` |
|     - |  227 | ` *   file (string\|false), line, endline (int),` |
|     - |  228 | ` *   ctorvis, clonevis (0 = absent, else PH7_CLASS_PROT_*),` |
|     - |  229 | ` *   consts  {name: {vis, final, decl, line}},` |
|     - |  230 | ` *   props   {name: {vis, static, readonly, hasdef, decl, line}},` |
|     - |  231 | ` *   methods {name: {vis, static, abstract, final, decl, line}}` |
|     - |  232 | ` */` |
|   816 |  233 | `static int vm_builtin_reflect_class_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  234 | `{` |
|   817 |  235 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  236 | `	ph7_class *pClass;` |
|     - |  237 | `	ph7_value *pInfo, *pConsts, *pProps, *pMethods, *pList;` |
|     - |  238 | `	SyHashEntry *pEntry;` |
|     - |  239 | `	SySet aIfaceSet;` |
|   817 |  240 | `	sxi32 iCtorVis = 0, iCloneVis = 0;` |
|   817 |  241 | `	int bIterable = 0;` |
|     - |  242 | `	sxu32 n;` |
|   817 |  243 | `	if( nArg < 1 ){` |
|   ! 0 |  244 | `		ph7_result_null(pCtx);` |
|   ! 0 |  245 | `		return PH7_OK;` |
|     - |  246 | `	}` |
|   817 |  247 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|   817 |  248 | `	if( pClass == 0 ){` |
|    13 |  249 | `		ph7_result_null(pCtx);` |
|    13 |  250 | `		return PH7_OK;` |
|     - |  251 | `	}` |
|   805 |  252 | `	pInfo = ph7_context_new_array(pCtx);` |
|   805 |  253 | `	pConsts = ph7_context_new_array(pCtx);` |
|   805 |  254 | `	pProps = ph7_context_new_array(pCtx);` |
|   805 |  255 | `	pMethods = ph7_context_new_array(pCtx);` |
|   805 |  256 | `	if( pInfo == 0 \|\| pConsts == 0 \|\| pProps == 0 \|\| pMethods == 0 ){` |
|   ! 0 |  257 | `		ph7_result_null(pCtx);` |
|   ! 0 |  258 | `		return PH7_OK;` |
|     - |  259 | `	}` |
|   805 |  260 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   805 |  261 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pClass->iFlags & PH7_CLASS_INTERNAL) != 0);` |
|   805 |  262 | `	ReflectMapAddBool(pCtx, pInfo, "interface", (pClass->iFlags & PH7_CLASS_INTERFACE) != 0);` |
|   805 |  263 | `	ReflectMapAddBool(pCtx, pInfo, "trait", (pClass->iFlags & PH7_CLASS_TRAIT) != 0);` |
|   805 |  264 | `	ReflectMapAddBool(pCtx, pInfo, "abstract", (pClass->iFlags & PH7_CLASS_ABSTRACT) != 0);` |
|   805 |  265 | `	ReflectMapAddBool(pCtx, pInfo, "final", (pClass->iFlags & PH7_CLASS_FINAL) != 0);` |
|   805 |  266 | `	ReflectMapAddBool(pCtx, pInfo, "readonly", (pClass->iFlags & PH7_CLASS_READONLY) != 0);` |
|   805 |  267 | `	if( pClass->pBase ){` |
|   418 |  268 | `		ReflectMapAddStr(pCtx, pInfo, "parent", SyStringData(&pClass->pBase->sName),` |
|   278 |  269 | `			(int)SyStringLength(&pClass->pBase->sName));` |
|   140 |  270 | `	}else{` |
|   527 |  271 | `		ReflectMapAddNull(pCtx, pInfo, "parent");` |
|     - |  272 | `	}` |
|     - |  273 | `	/* Transitive interfaces */` |
|   805 |  274 | `	SySetInit(&aIfaceSet, &pVm->sAllocator, sizeof(ph7_class *));` |
|   805 |  275 | `	ReflectCollectInterfaces(pClass, &aIfaceSet, 0);` |
|   805 |  276 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     - |  277 | `		/* An interface's own parents count as its interface list */` |
|    47 |  278 | `		if( pClass->pBase ){` |
|     9 |  279 | `			ReflectAddInterface(pClass->pBase, &aIfaceSet, 0);` |
|     4 |  280 | `		}` |
|    23 |  281 | `	}` |
|   805 |  282 | `	pList = ph7_context_new_array(pCtx);` |
|   805 |  283 | `	if( pList ){` |
|   805 |  284 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIfaceSet);` |
|  1151 |  285 | `		for( n = 0 ; n < SySetUsed(&aIfaceSet) ; n++ ){` |
|   347 |  286 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|   347 |  287 | `			if( pName == 0 ){ break; }` |
|   347 |  288 | `			ph7_value_string(pName, SyStringData(&apIface[n]->sName), (int)SyStringLength(&apIface[n]->sName));` |
|   347 |  289 | `			ph7_array_add_elem(pList, 0, pName);` |
|   347 |  290 | `			if( pVm->pTraversableClass && apIface[n] == pVm->pTraversableClass ){` |
|    13 |  291 | `				bIterable = 1;` |
|     6 |  292 | `			}` |
|   174 |  293 | `		}` |
|   805 |  294 | `		ph7_array_add_strkey_elem(pInfo, "interfaces", pList);` |
|   402 |  295 | `	}` |
|   805 |  296 | `	SySetRelease(&aIfaceSet);` |
|   805 |  297 | `	ReflectMapAddBool(pCtx, pInfo, "iterable", bIterable);` |
|     - |  298 | `	/* Used traits */` |
|   805 |  299 | `	pList = ph7_context_new_array(pCtx);` |
|   805 |  300 | `	if( pList ){` |
|   805 |  301 | `		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|   817 |  302 | `		for( n = 0 ; n < SySetUsed(&pClass->aTrait) ; n++ ){` |
|    13 |  303 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|    13 |  304 | `			if( pName == 0 ){ break; }` |
|    13 |  305 | `			ph7_value_string(pName, SyStringData(&apTrait[n]->sName), (int)SyStringLength(&apTrait[n]->sName));` |
|    13 |  306 | `			ph7_array_add_elem(pList, 0, pName);` |
|     7 |  307 | `		}` |
|   805 |  308 | `		ph7_array_add_strkey_elem(pInfo, "traits", pList);` |
|   402 |  309 | `	}` |
|     - |  310 | `	/* File / lines: no file recorded => false, like PHP internals */` |
|   805 |  311 | `	if( SyStringLength(&pClass->sFile) > 0 ){` |
|   729 |  312 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));` |
|   365 |  313 | `	}else{` |
|    77 |  314 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - |  315 | `	}` |
|   805 |  316 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pClass->nLine);` |
|   805 |  317 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pClass->nEndLine);` |
|   805 |  318 | `	ReflectMapAddDoc(pCtx, pInfo, &pClass->sDoc);` |
|   805 |  319 | `	ReflectMapAddAttrs(pCtx, pInfo, &pClass->aAttrs);` |
|     - |  320 | `	/* Members are emitted in PHP's reporting order: the class's own members` |
|     - |  321 | `	 * first (declaration order), then each inheritance level's, outward.` |
|     - |  322 | `	 * Per level we iterate the DECLARING class's own hash — subclass hashes` |
|     - |  323 | `	 * interleave inherited pointers unpredictably — and emit buffered` |
|     - |  324 | `	 * entries in reverse, because SyHash lists are LIFO. A pointer-identity` |
|     - |  325 | `	 * lookup in the reflected class's hash filters out members that are not` |
|     - |  326 | `	 * visible there (base privates, overridden entries). */` |
|     - |  327 | `	{` |
|     - |  328 | `		ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];` |
|   805 |  329 | `		ph7_class *pWalk = pClass;` |
|     - |  330 | `		SySet aTmp;` |
|   805 |  331 | `		sxu32 nChain = 0, iLevel, nT;` |
|  1891 |  332 | `		while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){` |
|  1087 |  333 | `			aChain[nChain++] = pWalk;` |
|  1087 |  334 | `			pWalk = pWalk->pBase;` |
|     1 |  335 | `		}` |
|   805 |  336 | `		SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));` |
|  1891 |  337 | `		for( iLevel = 0 ; iLevel < nChain ; iLevel++ ){` |
|  1087 |  338 | `			ph7_class *pLevel = aChain[iLevel];` |
|     - |  339 | `			/* --- Constants and properties (shared attribute table) --- */` |
|  1087 |  340 | `			SySetReset(&aTmp);` |
|  1087 |  341 | `			SyHashResetLoopCursor(&pLevel->hAttr);` |
|  3771 |  342 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hAttr)) != 0 ){` |
|  2685 |  343 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  2685 |  344 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|  2685 |  345 | `				if( iLevel == 0 ){` |
|     - |  346 | `					sxu32 j;` |
|     - |  347 | `					/* Own = declared here or by an off-chain provider (trait) */` |
|  3707 |  348 | `					for( j = 1 ; j < nChain ; j++ ){` |
|  1287 |  349 | `						if( aChain[j] == pDecl ){ break; }` |
|   584 |  350 | `					}` |
|  2541 |  351 | `					if( j < nChain ){ continue; }` |
|  1211 |  352 | `				}else{` |
|     - |  353 | `					SyHashEntry *pSub;` |
|   145 |  354 | `					if( pDecl != pLevel ){ continue; }` |
|     - |  355 | `					/* Must still be the visible member in the reflected class */` |
|   121 |  356 | `					pSub = SyHashGet(&pClass->hAttr, pEntry->pKey, pEntry->nKeyLen);` |
|   121 |  357 | `					if( pSub == 0 \|\| pSub->pUserData != (void *)pAttr ){ continue; }` |
|     - |  358 | `				}` |
|  2541 |  359 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     1 |  360 | `			}` |
|  3627 |  361 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|  2541 |  362 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|  2541 |  363 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;` |
|  2541 |  364 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|  2541 |  365 | `				ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|  2541 |  366 | `				if( pMeta == 0 ){ break; }` |
|  2541 |  367 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pAttr->iProtection);` |
|  2541 |  368 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|  2541 |  369 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pAttr->nLine);` |
|  2541 |  370 | `				ReflectMapAddDoc(pCtx, pMeta, &pAttr->sDoc);` |
|  2541 |  371 | `				ReflectMapAddAttrs(pCtx, pMeta, &pAttr->aAttrs);` |
|  2541 |  372 | `				ReflectMapAddBool(pCtx, pMeta, "typed", (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);` |
|  2541 |  373 | `				if( SyStringLength(&pAttr->sTypeName) > 0 ){` |
|   664 |  374 | `					ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&pAttr->sTypeName),` |
|   442 |  375 | `						(int)SyStringLength(&pAttr->sTypeName));` |
|   222 |  376 | `				}else{` |
|  2099 |  377 | `					ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - |  378 | `				}` |
|  2541 |  379 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|   649 |  380 | `					ReflectMapAddBool(pCtx, pMeta, "final", (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   649 |  381 | `					ReflectMapAddDyn(pCtx, pConsts, &pAttr->sName, pMeta);` |
|   325 |  382 | `				}else{` |
|  1893 |  383 | `					ReflectMapAddBool(pCtx, pMeta, "static", (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|  1893 |  384 | `					ReflectMapAddBool(pCtx, pMeta, "readonly", (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0);` |
|  1893 |  385 | `					ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&pAttr->aByteCode) > 0);` |
|  1893 |  386 | `					ReflectMapAddDyn(pCtx, pProps, &pAttr->sName, pMeta);` |
|     - |  387 | `				}` |
|  1271 |  388 | `			}` |
|     - |  389 | `			/* --- Methods. The reported name is the hash-entry key: trait` |
|     - |  390 | `			 * aliasing installs a shallow copy under the alias name while` |
|     - |  391 | `			 * sFunc.sName keeps the original, and PHP reports the alias. --- */` |
|  1087 |  392 | `			SySetReset(&aTmp);` |
|  1087 |  393 | `			SyHashResetLoopCursor(&pLevel->hMethod);` |
|  3229 |  394 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){` |
|  2143 |  395 | `				ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  2143 |  396 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|  2143 |  397 | `				if( iLevel == 0 ){` |
|     - |  398 | `					sxu32 j;` |
|  2535 |  399 | `					for( j = 1 ; j < nChain ; j++ ){` |
|  1011 |  400 | `						if( aChain[j] == pDecl ){ break; }` |
|   372 |  401 | `					}` |
|  1793 |  402 | `					if( j < nChain ){ continue; }` |
|   763 |  403 | `				}else{` |
|     - |  404 | `					SyHashEntry *pSub;` |
|   351 |  405 | `					if( pDecl != pLevel ){ continue; }` |
|   315 |  406 | `					pSub = SyHashGet(&pClass->hMethod, pEntry->pKey, pEntry->nKeyLen);` |
|   315 |  407 | `					if( pSub == 0 ){` |
|     - |  408 | `						/* Not in the subclass table: inheritance skips private` |
|     - |  409 | `						 * methods, but PHP still reports them on the subclass` |
|     - |  410 | `						 * (Zend copies privates into the child function table). */` |
|    21 |  411 | `						if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|   ! 0 |  412 | `							continue;` |
|     1 |  413 | `						}` |
|   305 |  414 | `					}else if( pSub->pUserData != (void *)pMeth ){` |
|     - |  415 | `						/* Overridden below this level: already reported */` |
|    27 |  416 | `						continue;` |
|     - |  417 | `					}` |
|     - |  418 | `				}` |
|  1813 |  419 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     1 |  420 | `			}` |
|  2899 |  421 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|  1813 |  422 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|  1813 |  423 | `				ph7_class_method *pMeth = (ph7_class_method *)pE->pUserData;` |
|  1813 |  424 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|     - |  425 | `				ph7_value *pMeta;` |
|     - |  426 | `				SyString sKey;` |
|     - |  427 | `				int bIsAlias;` |
|  1813 |  428 | `				SyStringInitFromBuf(&sKey, (const char *)pE->pKey, pE->nKeyLen);` |
|  3625 |  429 | `				bIsAlias = (sKey.nByte != SyStringLength(&pMeth->sFunc.sName)` |
|  1812 |  430 | `				 \|\| SyMemcmp(sKey.zString, SyStringData(&pMeth->sFunc.sName), sKey.nByte) != 0);` |
|  1812 |  431 | `				if( sKey.nByte == sizeof("__construct")-1` |
|  1109 |  432 | `				 && SyMemcmp(sKey.zString, "__construct", sKey.nByte) == 0 ){` |
|   375 |  433 | `					if( iCtorVis == 0 ){` |
|   375 |  434 | `						iCtorVis = pMeth->iProtection;` |
|   187 |  435 | `					}` |
|   375 |  436 | `					if( bIsAlias ){` |
|     - |  437 | `						/* Mount-time alias for a legacy class-name constructor:` |
|     - |  438 | `						 * the method is already listed under its declared name. */` |
|   ! 0 |  439 | `						continue;` |
|     - |  440 | `					}` |
|  1626 |  441 | `				}else if( sKey.nByte == sizeof("__clone")-1` |
|   781 |  442 | `				 && SyMemcmp(sKey.zString, "__clone", sKey.nByte) == 0 ){` |
|   ! 0 |  443 | `					if( iCloneVis == 0 ){` |
|   ! 0 |  444 | `						iCloneVis = pMeth->iProtection;` |
|   ! 0 |  445 | `					}` |
|  1438 |  446 | `				}else if( iCtorVis == 0` |
|   972 |  447 | `				 && sKey.nByte == SyStringLength(&pClass->sName)` |
|   254 |  448 | `				 && SyMemcmp(sKey.zString, SyStringData(&pClass->sName), sKey.nByte) == 0 ){` |
|     - |  449 | `					/* Legacy class-name constructor before the mount alias exists */` |
|   ! 0 |  450 | `					iCtorVis = pMeth->iProtection;` |
|   ! 0 |  451 | `				}` |
|  1813 |  452 | `				pMeta = ph7_context_new_array(pCtx);` |
|  1813 |  453 | `				if( pMeta == 0 ){ break; }` |
|  1813 |  454 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pMeth->iProtection);` |
|  1813 |  455 | `				ReflectMapAddBool(pCtx, pMeta, "static", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|  1813 |  456 | `				ReflectMapAddBool(pCtx, pMeta, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|  1813 |  457 | `				ReflectMapAddBool(pCtx, pMeta, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|  1813 |  458 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|  1813 |  459 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pMeth->nLine);` |
|  1813 |  460 | `				ReflectMapAddDyn(pCtx, pMethods, &sKey, pMeta);` |
|   907 |  461 | `			}` |
|   544 |  462 | `		}` |
|   805 |  463 | `		SySetRelease(&aTmp);` |
|     - |  464 | `	}` |
|   805 |  465 | `	ReflectMapAddInt(pCtx, pInfo, "ctorvis", (sxi64)iCtorVis);` |
|   805 |  466 | `	ReflectMapAddInt(pCtx, pInfo, "clonevis", (sxi64)iCloneVis);` |
|   805 |  467 | `	ph7_array_add_strkey_elem(pInfo, "consts", pConsts);` |
|   805 |  468 | `	ph7_array_add_strkey_elem(pInfo, "props", pProps);` |
|   805 |  469 | `	ph7_array_add_strkey_elem(pInfo, "methods", pMethods);` |
|   805 |  470 | `	ph7_result_value(pCtx, pInfo);` |
|   805 |  471 | `	return PH7_OK;` |
|   409 |  472 | `}` |
|     - |  473 | `/*` |
|     - |  474 | ` * mixed __reflect_const_value(string $class, string $name)` |
|     - |  475 | ` * Value of a class constant. The PHP layer guarantees existence.` |
|     - |  476 | ` */` |
|    28 |  477 | `static int vm_builtin_reflect_const_value(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  478 | `{` |
|     - |  479 | `	ph7_class *pClass;` |
|     - |  480 | `	ph7_class_attr *pAttr;` |
|     - |  481 | `	ph7_value *pValue;` |
|    28 |  482 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    28 |  483 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|    29 |  484 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|   ! 0 |  485 | `		ph7_result_null(pCtx);` |
|   ! 0 |  486 | `		return PH7_OK;` |
|     - |  487 | `	}` |
|     - |  488 | `	/* Constant slots are evaluated when the class is mounted */` |
|    29 |  489 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|    29 |  490 | `	if( pValue ){` |
|    29 |  491 | `		ph7_result_value(pCtx, pValue);` |
|    15 |  492 | `	}else{` |
|   ! 0 |  493 | `		ph7_result_null(pCtx);` |
|     - |  494 | `	}` |
|    29 |  495 | `	return PH7_OK;` |
|    15 |  496 | `}` |
|     - |  497 | `/*` |
|     - |  498 | ` * mixed __reflect_static_value(string $class, string $name)` |
|     - |  499 | ` * Current value of a static property (visibility ignored).` |
|     - |  500 | ` */` |
|    12 |  501 | `static int vm_builtin_reflect_static_value(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  502 | `{` |
|     - |  503 | `	ph7_class *pClass;` |
|     - |  504 | `	ph7_class_attr *pAttr;` |
|     - |  505 | `	ph7_value *pValue;` |
|    12 |  506 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    12 |  507 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|    13 |  508 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|   ! 0 |  509 | `		ph7_result_null(pCtx);` |
|   ! 0 |  510 | `		return PH7_OK;` |
|     - |  511 | `	}` |
|     - |  512 | `	{` |
|     - |  513 | `		/* Uninitialized typed static: same Error the VM raises on read */` |
|    13 |  514 | `		SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|    13 |  515 | `		if( pSlot ){` |
|   ! 0 |  516 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|   ! 0 |  517 | `			if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|   ! 0 |  518 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|   ! 0 |  519 | `				return PH7_VmThrowException(pCtx, "Error",` |
|     - |  520 | `					"Typed static property %z::$%z must not be accessed before initialization",` |
|   ! 0 |  521 | `					&pDecl->sName, &pAttr->sName);` |
|     - |  522 | `			}` |
|   ! 0 |  523 | `		}` |
|     - |  524 | `	}` |
|    13 |  525 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|    13 |  526 | `	if( pValue ){` |
|    13 |  527 | `		ph7_result_value(pCtx, pValue);` |
|     7 |  528 | `	}else{` |
|   ! 0 |  529 | `		ph7_result_null(pCtx);` |
|     - |  530 | `	}` |
|    13 |  531 | `	return PH7_OK;` |
|     7 |  532 | `}` |
|     - |  533 | `/*` |
|     - |  534 | ` * bool __reflect_static_set(string $class, string $name, mixed $value)` |
|     - |  535 | ` * Overwrite a static property's shared slot (visibility ignored).` |
|     - |  536 | ` */` |
|     4 |  537 | `static int vm_builtin_reflect_static_set(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  538 | `{` |
|     - |  539 | `	ph7_class *pClass;` |
|     - |  540 | `	ph7_class_attr *pAttr;` |
|     - |  541 | `	ph7_value *pValue;` |
|     4 |  542 | `	if( nArg < 3 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|     4 |  543 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|     5 |  544 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|   ! 0 |  545 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  546 | `		return PH7_OK;` |
|     - |  547 | `	}` |
|     5 |  548 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|     5 |  549 | `	if( pValue == 0 ){` |
|   ! 0 |  550 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  551 | `		return PH7_OK;` |
|     - |  552 | `	}` |
|     - |  553 | `	{` |
|     5 |  554 | `		sxi32 rc = ReflectEnforceStore(pCtx, pAttr->nIdx, apArg[2]);` |
|     5 |  555 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  556 | `			return rc;` |
|     - |  557 | `		}` |
|     - |  558 | `	}` |
|     5 |  559 | `	PH7_MemObjStore(apArg[2], pValue);` |
|     5 |  560 | `	ph7_result_bool(pCtx, 1);` |
|     5 |  561 | `	return PH7_OK;` |
|     3 |  562 | `}` |
|     - |  563 | `/*` |
|     - |  564 | ` * mixed __reflect_prop_default(string $class, string $name)` |
|     - |  565 | ` * Evaluate a non-static property's compiled default expression` |
|     - |  566 | ` * (null when the property has no default).` |
|     - |  567 | ` */` |
|    24 |  568 | `static int vm_builtin_reflect_prop_default(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  569 | `{` |
|     - |  570 | `	ph7_class *pClass;` |
|     - |  571 | `	ph7_class_attr *pAttr;` |
|     - |  572 | `	ph7_value sValue;` |
|    24 |  573 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    24 |  574 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|    24 |  575 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) != 0` |
|    25 |  576 | `	 \|\| SySetUsed(&pAttr->aByteCode) < 1 ){` |
|     3 |  577 | `		ph7_result_null(pCtx);` |
|     3 |  578 | `		return PH7_OK;` |
|     - |  579 | `	}` |
|    23 |  580 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     - |  581 | `	/* Same evaluation path the VM uses for omitted call arguments */` |
|    23 |  582 | `	VmLocalExec(pCtx->pVm, &pAttr->aByteCode, &sValue, FALSE);` |
|    23 |  583 | `	ph7_result_value(pCtx, &sValue);` |
|    23 |  584 | `	PH7_MemObjRelease(&sValue);` |
|    23 |  585 | `	return PH7_OK;` |
|    13 |  586 | `}` |
|     - |  587 | `/*` |
|     - |  588 | ` * Collect a PHP array's values into a ph7_value* set (call arguments).` |
|     - |  589 | ` * When ppNames is non-NULL, string keys become named arguments: a name` |
|     - |  590 | ` * map is lazily allocated (like call_user_func_array's) with one entry` |
|     - |  591 | ` * per collected slot, empty entries meaning positional.` |
|     - |  592 | ` */` |
|    30 |  593 | `static sxi32 ReflectCollectArgs(ph7_context *pCtx, ph7_value *pArray, SySet *pOut, SyString **ppNames)` |
|     1 |  594 | `{` |
|     - |  595 | `	ph7_hashmap *pMap;` |
|     - |  596 | `	ph7_hashmap_node *pEntry;` |
|    31 |  597 | `	SyString *aNames = 0;` |
|    31 |  598 | `	sxu32 nSlot = 0;` |
|     - |  599 | `	sxu32 n;` |
|    31 |  600 | `	if( ppNames ){` |
|    11 |  601 | `		*ppNames = 0;` |
|     5 |  602 | `	}` |
|    31 |  603 | `	if( !ph7_value_is_array(pArray) ){` |
|   ! 0 |  604 | `		return SXRET_OK;` |
|     - |  605 | `	}` |
|    31 |  606 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|    31 |  607 | `	pEntry = pMap->pFirst;` |
|    71 |  608 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    41 |  609 | `		ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|    41 |  610 | `		if( pValue ){` |
|    41 |  611 | `			if( ppNames && pEntry->iType == HASHMAP_BLOB_NODE ){` |
|     3 |  612 | `				if( aNames == 0 ){` |
|     4 |  613 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     2 |  614 | `						pMap->nEntry * sizeof(SyString));` |
|     3 |  615 | `					if( aNames ){` |
|     3 |  616 | `						SyZero(aNames, pMap->nEntry * sizeof(SyString));` |
|     1 |  617 | `					}` |
|     1 |  618 | `				}` |
|     3 |  619 | `				if( aNames ){` |
|     3 |  620 | `					SyStringInitFromBuf(&aNames[nSlot],` |
|     - |  621 | `						SyBlobData(&pEntry->xKey.sKey), SyBlobLength(&pEntry->xKey.sKey));` |
|     1 |  622 | `				}` |
|     1 |  623 | `			}` |
|    41 |  624 | `			SySetPut(pOut, (const void *)&pValue);` |
|    41 |  625 | `			nSlot++;` |
|    20 |  626 | `		}` |
|    41 |  627 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|    21 |  628 | `	}` |
|    31 |  629 | `	if( ppNames ){` |
|    11 |  630 | `		*ppNames = aNames;` |
|     5 |  631 | `	}` |
|    31 |  632 | `	return SXRET_OK;` |
|    16 |  633 | `}` |
|     - |  634 | `/*` |
|     - |  635 | ` * object __reflect_new_instance(string $class, array $args)` |
|     - |  636 | ` * Instantiate and run the constructor with positional arguments.` |
|     - |  637 | ` * The PHP layer has already validated instantiability and ctor visibility.` |
|     - |  638 | ` */` |
|    14 |  639 | `static int vm_builtin_reflect_new_instance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  640 | `{` |
|    15 |  641 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  642 | `	ph7_class *pClass;` |
|     - |  643 | `	ph7_class_instance *pThis;` |
|     - |  644 | `	ph7_class_method *pCons;` |
|    15 |  645 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pVm, apArg[0])) == 0 ){` |
|   ! 0 |  646 | `		ph7_result_null(pCtx);` |
|   ! 0 |  647 | `		return PH7_OK;` |
|     - |  648 | `	}` |
|    15 |  649 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|    15 |  650 | `	if( pThis == 0 ){` |
|   ! 0 |  651 | `		ph7_result_null(pCtx);` |
|   ! 0 |  652 | `		return PH7_OK;` |
|     - |  653 | `	}` |
|    15 |  654 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|    15 |  655 | `	if( pCons ){` |
|     - |  656 | `		SySet aArg;` |
|     - |  657 | `		sxi32 rc;` |
|    11 |  658 | `		SyString *aNames = 0;` |
|    11 |  659 | `		SySetInit(&aArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    11 |  660 | `		if( nArg > 1 ){` |
|    11 |  661 | `			ReflectCollectArgs(pCtx, apArg[1], &aArg, &aNames);` |
|     5 |  662 | `		}` |
|    11 |  663 | `		if( aNames ){` |
|     - |  664 | `			VmCallArgMap sMap;` |
|     3 |  665 | `			sMap.bHasNamed = 1;` |
|     3 |  666 | `			sMap.bIsNamespaced = 0;` |
|     3 |  667 | `			sMap.bStrict = 0;` |
|     3 |  668 | `			sMap.nTotal = SySetUsed(&aArg);` |
|     3 |  669 | `			sMap.aNames = aNames;` |
|     4 |  670 | `			rc = PH7_VmCallClassMethodMap(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|     2 |  671 | `				(ph7_value **)SySetBasePtr(&aArg), &sMap);` |
|     3 |  672 | `			SyMemBackendFree(&pVm->sAllocator, aNames);` |
|     2 |  673 | `		}else{` |
|    13 |  674 | `			rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|     8 |  675 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|     - |  676 | `		}` |
|    11 |  677 | `		SySetRelease(&aArg);` |
|    11 |  678 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 |  679 | `			PH7_ClassInstanceUnref(pThis);` |
|   ! 0 |  680 | `			return rc;` |
|     - |  681 | `		}` |
|     5 |  682 | `	}` |
|    15 |  683 | `	return ReflectResultObject(pCtx, pThis);` |
|     8 |  684 | `}` |
|     - |  685 | `/*` |
|     - |  686 | ` * object __reflect_new_no_ctor(string $class)` |
|     - |  687 | ` * Instantiate without running the constructor (property defaults still` |
|     - |  688 | ` * apply — PH7_NewClassInstance builds the attribute frame).` |
|     - |  689 | ` */` |
|    52 |  690 | `static int vm_builtin_reflect_new_no_ctor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  691 | `{` |
|     - |  692 | `	ph7_class *pClass;` |
|    53 |  693 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0 ){` |
|   ! 0 |  694 | `		ph7_result_null(pCtx);` |
|   ! 0 |  695 | `		return PH7_OK;` |
|     - |  696 | `	}` |
|    53 |  697 | `	return ReflectResultObject(pCtx, PH7_NewClassInstance(pCtx->pVm, pClass));` |
|    27 |  698 | `}` |
|     - |  699 | `/*` |
|     - |  700 | ` * Typed/readonly store enforcement for reflection writes. Like the VM's` |
|     - |  701 | ` * store path, except an UNINITIALIZED readonly property may be written from` |
|     - |  702 | ` * any scope (PHP lets ReflectionProperty::setValue initialize readonly): the` |
|     - |  703 | ` * READONLY bit is masked off for the enforcement call so the set-scope check` |
|     - |  704 | ` * is skipped, while an already-initialized readonly still gets PHP's` |
|     - |  705 | ` * "Cannot modify readonly property" Error. Returns SXRET_OK/PH7_EXCEPTION/` |
|     - |  706 | ` * PH7_ABORT; the value may be coerced in place.` |
|     - |  707 | ` */` |
|    10 |  708 | `static sxi32 ReflectEnforceStore(ph7_context *pCtx, sxu32 nIdx, ph7_value *pValue)` |
|     1 |  709 | `{` |
|    11 |  710 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  711 | `	SyHashEntry *pSlot;` |
|     - |  712 | `	VmClassAttr *pVmAttr;` |
|     - |  713 | `	ph7_class_attr *pAttr;` |
|     - |  714 | `	sxi32 iSaved, rc;` |
|    11 |  715 | `	pSlot = SyHashGet(&pVm->hTypedSlot, (const void *)&nIdx, sizeof(sxu32));` |
|    11 |  716 | `	if( pSlot == 0 ){` |
|     7 |  717 | `		return SXRET_OK; /* Untyped slot: plain store */` |
|     - |  718 | `	}` |
|     5 |  719 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     5 |  720 | `	pAttr = pVmAttr->pAttr;` |
|     5 |  721 | `	if( pAttr == 0 ){` |
|   ! 0 |  722 | `		return SXRET_OK;` |
|     - |  723 | `	}` |
|     5 |  724 | `	iSaved = pAttr->iFlags;` |
|     5 |  725 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) ){` |
|   ! 0 |  726 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_READONLY;` |
|   ! 0 |  727 | `	}` |
|     5 |  728 | `	rc = PH7_VmEnforcePropStore(pVm, nIdx, pValue);` |
|     5 |  729 | `	pAttr->iFlags = iSaved;` |
|     5 |  730 | `	return rc;` |
|     6 |  731 | `}` |
|     - |  732 | `/*` |
|     - |  733 | ` * mixed __reflect_prop_read(object $obj, string $name)` |
|     - |  734 | ` * Instance property read, visibility ignored. Throws PHP's Error for an` |
|     - |  735 | ` * uninitialized typed property.` |
|     - |  736 | ` */` |
|    20 |  737 | `static int vm_builtin_reflect_prop_read(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  738 | `{` |
|     - |  739 | `	ph7_class_instance *pThis;` |
|     - |  740 | `	SyHashEntry *pEntry;` |
|     - |  741 | `	VmClassAttr *pVmAttr;` |
|     - |  742 | `	ph7_value *pValue;` |
|     - |  743 | `	const char *zName;` |
|     - |  744 | `	int nLen;` |
|    21 |  745 | `	if( nArg < 2 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  746 | `		ph7_result_null(pCtx);` |
|   ! 0 |  747 | `		return PH7_OK;` |
|     - |  748 | `	}` |
|    21 |  749 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    21 |  750 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|    21 |  751 | `	pEntry = nLen > 0 ? SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen) : 0;` |
|    21 |  752 | `	if( pEntry == 0 ){` |
|   ! 0 |  753 | `		ph7_result_null(pCtx);` |
|   ! 0 |  754 | `		return PH7_OK;` |
|     - |  755 | `	}` |
|    21 |  756 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    21 |  757 | `	if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|     3 |  758 | `		ph7_class *pDecl = pVmAttr->pAttr->pDeclClass ? pVmAttr->pAttr->pDeclClass : pThis->pClass;` |
|     4 |  759 | `		return PH7_VmThrowException(pCtx, "Error",` |
|     - |  760 | `			"Typed property %z::$%z must not be accessed before initialization",` |
|     2 |  761 | `			&pDecl->sName, &pVmAttr->pAttr->sName);` |
|     - |  762 | `	}` |
|    19 |  763 | `	pValue = PH7_ClassInstanceExtractAttrValue(pThis, pVmAttr);` |
|    19 |  764 | `	if( pValue ){` |
|    19 |  765 | `		ph7_result_value(pCtx, pValue);` |
|    10 |  766 | `	}else{` |
|   ! 0 |  767 | `		ph7_result_null(pCtx);` |
|     - |  768 | `	}` |
|    19 |  769 | `	return PH7_OK;` |
|    11 |  770 | `}` |
|     - |  771 | `/*` |
|     - |  772 | ` * bool __reflect_prop_write(object $obj, string $name, mixed $value)` |
|     - |  773 | ` * Instance property write, visibility ignored; typed and readonly rules` |
|     - |  774 | ` * enforced (see ReflectEnforceStore).` |
|     - |  775 | ` */` |
|     6 |  776 | `static int vm_builtin_reflect_prop_write(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  777 | `{` |
|     - |  778 | `	ph7_class_instance *pThis;` |
|     - |  779 | `	SyHashEntry *pEntry;` |
|     - |  780 | `	VmClassAttr *pVmAttr;` |
|     - |  781 | `	ph7_value *pValue;` |
|     - |  782 | `	const char *zName;` |
|     - |  783 | `	sxi32 rc;` |
|     - |  784 | `	int nLen;` |
|     7 |  785 | `	if( nArg < 3 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  786 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  787 | `		return PH7_OK;` |
|     - |  788 | `	}` |
|     7 |  789 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     7 |  790 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|     7 |  791 | `	pEntry = nLen > 0 ? SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen) : 0;` |
|     7 |  792 | `	if( pEntry == 0 ){` |
|   ! 0 |  793 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  794 | `		return PH7_OK;` |
|     - |  795 | `	}` |
|     7 |  796 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     7 |  797 | `	rc = ReflectEnforceStore(pCtx, pVmAttr->nIdx, apArg[2]);` |
|     7 |  798 | `	if( rc != SXRET_OK ){` |
|     3 |  799 | `		return rc;` |
|     - |  800 | `	}` |
|     5 |  801 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|     5 |  802 | `	if( pValue == 0 ){` |
|   ! 0 |  803 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  804 | `		return PH7_OK;` |
|     - |  805 | `	}` |
|     5 |  806 | `	PH7_MemObjStore(apArg[2], pValue);` |
|     5 |  807 | `	ph7_result_bool(pCtx, 1);` |
|     5 |  808 | `	return PH7_OK;` |
|     4 |  809 | `}` |
|     - |  810 | `/*` |
|     - |  811 | ` * int __reflect_prop_state(object\|string $target, string $name)` |
|     - |  812 | ` * Bitfield: 1 = exists (instance attr / static slot), 2 = initialized,` |
|     - |  813 | ` * 4 = dynamic (instance-owned, not class-declared).` |
|     - |  814 | ` */` |
|    16 |  815 | `static int vm_builtin_reflect_prop_state(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  816 | `{` |
|    17 |  817 | `	int iState = 0;` |
|     - |  818 | `	const char *zName;` |
|     - |  819 | `	int nLen;` |
|    17 |  820 | `	if( nArg < 2 ){` |
|   ! 0 |  821 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  822 | `		return PH7_OK;` |
|     - |  823 | `	}` |
|    17 |  824 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|    17 |  825 | `	if( nLen < 1 ){` |
|   ! 0 |  826 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  827 | `		return PH7_OK;` |
|     - |  828 | `	}` |
|    17 |  829 | `	if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|    17 |  830 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    17 |  831 | `		SyHashEntry *pEntry = SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen);` |
|    17 |  832 | `		if( pEntry ){` |
|    17 |  833 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    17 |  834 | `			iState \|= 1;` |
|    17 |  835 | `			if( (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|    15 |  836 | `				iState \|= 2;` |
|     7 |  837 | `			}` |
|    17 |  838 | `			if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|    11 |  839 | `				iState \|= 4;` |
|     5 |  840 | `			}` |
|     8 |  841 | `		}` |
|     9 |  842 | `	}else{` |
|   ! 0 |  843 | `		ph7_class *pClass = ReflectResolveClass(pCtx->pVm, apArg[0]);` |
|   ! 0 |  844 | `		ph7_class_attr *pAttr = pClass ? ReflectFetchAttr(pClass, apArg[1]) : 0;` |
|   ! 0 |  845 | `		if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|   ! 0 |  846 | `			SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|   ! 0 |  847 | `			iState \|= 1 \| 2;` |
|   ! 0 |  848 | `			if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|   ! 0 |  849 | `				iState &= ~2;` |
|   ! 0 |  850 | `			}` |
|   ! 0 |  851 | `		}` |
|     - |  852 | `	}` |
|    17 |  853 | `	ph7_result_int(pCtx, iState);` |
|    17 |  854 | `	return PH7_OK;` |
|     9 |  855 | `}` |
|     - |  856 | `/*` |
|     - |  857 | ` * array __reflect_dyn_props(object $obj)` |
|     - |  858 | ` * Names of the instance's runtime-added (dynamic) properties, in creation` |
|     - |  859 | ` * order (the instance attr table inserts dynamics at the tail).` |
|     - |  860 | ` */` |
|     2 |  861 | `static int vm_builtin_reflect_dyn_props(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  862 | `{` |
|     - |  863 | `	ph7_class_instance *pThis;` |
|     - |  864 | `	SyHashEntry *pEntry;` |
|     - |  865 | `	ph7_value *pList;` |
|     2 |  866 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|     3 |  867 | `	 \|\| (pList = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 |  868 | `		ph7_result_null(pCtx);` |
|   ! 0 |  869 | `		return PH7_OK;` |
|     - |  870 | `	}` |
|     3 |  871 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     3 |  872 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     7 |  873 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     5 |  874 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     5 |  875 | `		if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|     5 |  876 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|     5 |  877 | `			if( pName == 0 ){ break; }` |
|     7 |  878 | `			ph7_value_string(pName, SyStringData(&pVmAttr->pAttr->sName),` |
|     4 |  879 | `				(int)SyStringLength(&pVmAttr->pAttr->sName));` |
|     5 |  880 | `			ph7_array_add_elem(pList, 0, pName);` |
|     2 |  881 | `		}` |
|     1 |  882 | `	}` |
|     3 |  883 | `	ph7_result_value(pCtx, pList);` |
|     3 |  884 | `	return PH7_OK;` |
|     2 |  885 | `}` |
|     - |  886 | `/* Hand an EXISTING instance to the caller: takes an extra reference` |
|     - |  887 | ` * (unlike ReflectResultObject, which transfers a fresh instance's one). */` |
|     4 |  888 | `static int ReflectResultExistingObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|     1 |  889 | `{` |
|     5 |  890 | `	if( pObj == 0 ){` |
|   ! 0 |  891 | `		ph7_result_null(pCtx);` |
|   ! 0 |  892 | `		return PH7_OK;` |
|     - |  893 | `	}` |
|     5 |  894 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     5 |  895 | `	pObj->iRef++;` |
|     5 |  896 | `	pCtx->pRet->x.pOther = pObj;` |
|     5 |  897 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|     5 |  898 | `	return PH7_OK;` |
|     3 |  899 | `}` |
|     - |  900 | `/* pVal is a Closure instance? Return it, else NULL. */` |
|   414 |  901 | `static ph7_class_instance * ReflectValueClosure(ph7_vm *pVm, ph7_value *pVal)` |
|     1 |  902 | `{` |
|     - |  903 | `	ph7_class_instance *pThis;` |
|   415 |  904 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
|   391 |  905 | `		return 0;` |
|     - |  906 | `	}` |
|    25 |  907 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    25 |  908 | `	return (pThis->pClass == pVm->pClosureClass) ? pThis : 0;` |
|   208 |  909 | `}` |
|     - |  910 | `/*` |
|     - |  911 | ` * Resolve a reflection callable target into its compiled function.` |
|     - |  912 | ` *   - pMethodArg a non-empty string  -> method mode: pTarget is a class name` |
|     - |  913 | ` *     or object; outputs *ppClass and *ppMeth.` |
|     - |  914 | ` *   - pTarget a Closure              -> unwrap $__fn into hFunction; *ppClosure.` |
|     - |  915 | ` *   - pTarget a string               -> hFunction (user) or hHostFunction` |
|     - |  916 | ` *     (*ppHost set, returns NULL).` |
|     - |  917 | ` * Returns the ph7_vm_func, or NULL (host function or unresolvable).` |
|     - |  918 | ` */` |
|   694 |  919 | `static ph7_vm_func * ReflectResolveCallable(ph7_context *pCtx, ph7_value *pTarget,` |
|     - |  920 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|     - |  921 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|     1 |  922 | `{` |
|   695 |  923 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  924 | `	SyHashEntry *pEntry;` |
|   695 |  925 | `	if( ppClass ){ *ppClass = 0; }` |
|   695 |  926 | `	if( ppMeth ){ *ppMeth = 0; }` |
|   695 |  927 | `	if( ppHost ){ *ppHost = 0; }` |
|   695 |  928 | `	if( ppClosure ){ *ppClosure = 0; }` |
|   695 |  929 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
|   281 |  930 | `		ph7_class *pClass = ReflectResolveClass(pVm, pTarget);` |
|     - |  931 | `		ph7_class_method *pMeth;` |
|   281 |  932 | `		if( pClass == 0 ){` |
|   ! 0 |  933 | `			return 0;` |
|     - |  934 | `		}` |
|   421 |  935 | `		pMeth = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|   140 |  936 | `			SyBlobLength(&pMethodArg->sBlob));` |
|   281 |  937 | `		if( pMeth == 0 ){` |
|   ! 0 |  938 | `			return 0;` |
|     - |  939 | `		}` |
|   281 |  940 | `		if( ppClass ){ *ppClass = pClass; }` |
|   281 |  941 | `		if( ppMeth ){ *ppMeth = pMeth; }` |
|   281 |  942 | `		return &pMeth->sFunc;` |
|     - |  943 | `	}` |
|     - |  944 | `	{` |
|   415 |  945 | `		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|   415 |  946 | `		if( pClo ){` |
|     - |  947 | `			SyString sAttr;` |
|     - |  948 | `			ph7_value *pFn;` |
|    25 |  949 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    25 |  950 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|    25 |  951 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) < 1 ){` |
|   ! 0 |  952 | `				return 0;` |
|     - |  953 | `			}` |
|    25 |  954 | `			pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    25 |  955 | `			if( pEntry == 0 ){` |
|     - |  956 | `				/* A Closure over a host function (Closure::fromCallable('strlen')) */` |
|   ! 0 |  957 | `				pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|   ! 0 |  958 | `				if( pEntry && ppHost ){` |
|   ! 0 |  959 | `					*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|   ! 0 |  960 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|   ! 0 |  961 | `				}` |
|   ! 0 |  962 | `				return 0;` |
|     - |  963 | `			}` |
|    25 |  964 | `			if( ppClosure ){ *ppClosure = pClo; }` |
|    25 |  965 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - |  966 | `		}` |
|     - |  967 | `	}` |
|   391 |  968 | `	if( pTarget->iFlags & MEMOBJ_STRING ){` |
|   391 |  969 | `		if( SyBlobLength(&pTarget->sBlob) < 1 ){` |
|   ! 0 |  970 | `			return 0;` |
|     - |  971 | `		}` |
|   391 |  972 | `		pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   391 |  973 | `		if( pEntry ){` |
|   285 |  974 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - |  975 | `		}` |
|   107 |  976 | `		pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   107 |  977 | `		if( pEntry && ppHost ){` |
|   105 |  978 | `			*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|    52 |  979 | `		}` |
|    53 |  980 | `	}` |
|   107 |  981 | `	return 0;` |
|   348 |  982 | `}` |
|     - |  983 | `/* Emit the shared descriptor fields of a compiled function. */` |
|   546 |  984 | `static void ReflectFillFuncCommon(ph7_context *pCtx, ph7_value *pInfo, ph7_vm_func *pFunc)` |
|     1 |  985 | `{` |
|     - |  986 | `	ph7_vm_func_arg *aArg;` |
|     - |  987 | `	ph7_value *pParams, *pStatics;` |
|   547 |  988 | `	int bVariadic = 0;` |
|     - |  989 | `	int bAnon;` |
|     - |  990 | `	sxu32 n;` |
|     - |  991 | ``	/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|     - |  992 | `	 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|   547 |  993 | `	bAnon = (pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|   546 |  994 | `	if( !bAnon && SyStringLength(&pFunc->sName) > 9` |
|   312 |  995 | `	 && (SyMemcmp(SyStringData(&pFunc->sName), "[lambda_", 8) == 0` |
|    85 |  996 | `	  \|\| SyMemcmp(SyStringData(&pFunc->sName), "[closure_", 9) == 0) ){` |
|    11 |  997 | `		bAnon = 1;` |
|     5 |  998 | `	}` |
|   547 |  999 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|   547 | 1000 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pFunc->iFlags & VM_FUNC_INTERNAL) != 0);` |
|   547 | 1001 | `	ReflectMapAddBool(pCtx, pInfo, "closure", bAnon);` |
|   547 | 1002 | `	ReflectMapAddBool(pCtx, pInfo, "byref", (pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|   547 | 1003 | `	ReflectMapAddBool(pCtx, pInfo, "generator", (pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|   547 | 1004 | `	ReflectMapAddBool(pCtx, pInfo, "strict", pFunc->bStrictTypes != 0);` |
|   547 | 1005 | `	if( SyStringLength(&pFunc->sFile) > 0 ){` |
|   529 | 1006 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pFunc->sFile), (int)SyStringLength(&pFunc->sFile));` |
|   265 | 1007 | `	}else{` |
|    19 | 1008 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1009 | `	}` |
|   547 | 1010 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pFunc->nLine);` |
|   547 | 1011 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pFunc->nEndLine);` |
|   547 | 1012 | `	ReflectMapAddDoc(pCtx, pInfo, &pFunc->sDoc);` |
|   547 | 1013 | `	ReflectMapAddAttrs(pCtx, pInfo, &pFunc->aAttrs);` |
|   547 | 1014 | `	if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|   145 | 1015 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", SyStringData(&pFunc->sReturnTypeName),` |
|    96 | 1016 | `			(int)SyStringLength(&pFunc->sReturnTypeName));` |
|   499 | 1017 | `	}else if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     - | 1018 | `		/* The type-text renderer omits void/never atoms (compile.c notes the` |
|     - | 1019 | `		 * root fix belongs there); name them here for getReturnType(). */` |
|     3 | 1020 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "void", sizeof("void")-1);` |
|   450 | 1021 | `	}else if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|     3 | 1022 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "never", sizeof("never")-1);` |
|     2 | 1023 | `	}else{` |
|   447 | 1024 | `		ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1025 | `	}` |
|   547 | 1026 | `	ReflectMapAddBool(pCtx, pInfo, "retnullable", (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) != 0);` |
|     - | 1027 | `	/* Parameters */` |
|   547 | 1028 | `	pParams = ph7_context_new_array(pCtx);` |
|   547 | 1029 | `	aArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|  1799 | 1030 | `	for( n = 0 ; pParams && n < SySetUsed(&pFunc->aArgs) ; n++ ){` |
|  1253 | 1031 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|  1253 | 1032 | `		if( pMeta == 0 ){ break; }` |
|  1253 | 1033 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aArg[n].sName), (int)SyStringLength(&aArg[n].sName));` |
|  1253 | 1034 | `		ReflectMapAddInt(pCtx, pMeta, "pos", (sxi64)n);` |
|  1253 | 1035 | `		ReflectMapAddBool(pCtx, pMeta, "byref", (aArg[n].iFlags & VM_FUNC_ARG_BY_REF) != 0);` |
|  1253 | 1036 | `		ReflectMapAddBool(pCtx, pMeta, "variadic", (aArg[n].iFlags & VM_FUNC_ARG_VARIADIC) != 0);` |
|     - | 1037 | `		/* The compiler never sets ARG_HAS_DEF; a default = compiled bytecode` |
|     - | 1038 | `		 * (same test the OP_CALL default-value path uses). */` |
|  1253 | 1039 | `		ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&aArg[n].aByteCode) > 0);` |
|  1253 | 1040 | `		ReflectMapAddBool(pCtx, pMeta, "nullable", (aArg[n].iFlags & VM_FUNC_ARG_NULLABLE) != 0);` |
|  1253 | 1041 | `		ReflectMapAddBool(pCtx, pMeta, "promoted", (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) != 0);` |
|  1253 | 1042 | `		if( SyStringLength(&aArg[n].sTypeName) > 0 ){` |
|   715 | 1043 | `			ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&aArg[n].sTypeName),` |
|   476 | 1044 | `				(int)SyStringLength(&aArg[n].sTypeName));` |
|   239 | 1045 | `		}else{` |
|   777 | 1046 | `			ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - | 1047 | `		}` |
|  1253 | 1048 | `		ReflectMapAddAttrs(pCtx, pMeta, &aArg[n].aAttrs);` |
|  1253 | 1049 | `		ph7_array_add_elem(pParams, 0, pMeta);` |
|  1253 | 1050 | `		if( aArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|   183 | 1051 | `			bVariadic = 1;` |
|    91 | 1052 | `		}` |
|   627 | 1053 | `	}` |
|   547 | 1054 | `	if( pParams ){` |
|   547 | 1055 | `		ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|   273 | 1056 | `	}` |
|   547 | 1057 | `	ReflectMapAddBool(pCtx, pInfo, "variadic", bVariadic);` |
|     - | 1058 | `	/* Static variables: current value when the slot was initialized (first` |
|     - | 1059 | `	 * call), otherwise the evaluated default — PHP's getStaticVariables` |
|     - | 1060 | `	 * initializes on demand and reports the same values. */` |
|   547 | 1061 | `	pStatics = ph7_context_new_array(pCtx);` |
|   547 | 1062 | `	if( pStatics ){` |
|   547 | 1063 | `		ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|   575 | 1064 | `		for( n = 0 ; n < SySetUsed(&pFunc->aStatic) ; n++ ){` |
|    29 | 1065 | `			ph7_value *pVal = 0;` |
|     - | 1066 | `			ph7_value sScratch;` |
|    29 | 1067 | `			int bScratch = 0;` |
|    29 | 1068 | `			if( aStatic[n].nIdx != SXU32_HIGH ){` |
|    11 | 1069 | `				pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);` |
|     5 | 1070 | `			}` |
|    29 | 1071 | `			if( pVal == 0 ){` |
|    19 | 1072 | `				PH7_MemObjInit(pCtx->pVm, &sScratch);` |
|    19 | 1073 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|    19 | 1074 | `					VmLocalExec(pCtx->pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|     9 | 1075 | `				}` |
|    19 | 1076 | `				pVal = &sScratch;` |
|    19 | 1077 | `				bScratch = 1;` |
|     9 | 1078 | `			}` |
|    29 | 1079 | `			ReflectMapAddDyn(pCtx, pStatics, &aStatic[n].sName, pVal);` |
|    29 | 1080 | `			if( bScratch ){` |
|    19 | 1081 | `				PH7_MemObjRelease(&sScratch);` |
|     9 | 1082 | `			}` |
|    15 | 1083 | `		}` |
|   547 | 1084 | `		ph7_array_add_strkey_elem(pInfo, "statics", pStatics);` |
|   273 | 1085 | `	}` |
|   547 | 1086 | `}` |
|     - | 1087 | `/*` |
|     - | 1088 | ` * array\|null __reflect_func_info(string\|Closure $target [, string $method])` |
|     - | 1089 | ` * Function/method/closure descriptor for the PHP layer.` |
|     - | 1090 | ` */` |
|   652 | 1091 | `static int vm_builtin_reflect_func_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1092 | `{` |
|     - | 1093 | `	ph7_vm_func *pFunc;` |
|   653 | 1094 | `	ph7_class *pClass = 0;` |
|   653 | 1095 | `	ph7_class_method *pMeth = 0;` |
|   653 | 1096 | `	ph7_user_func *pHost = 0;` |
|   653 | 1097 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1098 | `	ph7_value *pInfo;` |
|   653 | 1099 | `	if( nArg < 1 ){` |
|   ! 0 | 1100 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1101 | `		return PH7_OK;` |
|     - | 1102 | `	}` |
|   653 | 1103 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], nArg > 1 ? apArg[1] : 0,` |
|     - | 1104 | `		&pClass, &pMeth, &pHost, &pClosure);` |
|   653 | 1105 | `	if( pFunc == 0 && pHost == 0 ){` |
|     3 | 1106 | `		ph7_result_null(pCtx);` |
|     3 | 1107 | `		return PH7_OK;` |
|     - | 1108 | `	}` |
|   651 | 1109 | `	pInfo = ph7_context_new_array(pCtx);` |
|   651 | 1110 | `	if( pInfo == 0 ){` |
|   ! 0 | 1111 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1112 | `		return PH7_OK;` |
|     - | 1113 | `	}` |
|   651 | 1114 | `	if( pFunc == 0 ){` |
|     - | 1115 | `		/* Host (C builtin) function: no parameter metadata beyond arity */` |
|   105 | 1116 | `		ph7_value *pParams = ph7_context_new_array(pCtx);` |
|   105 | 1117 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pHost->sName), (int)SyStringLength(&pHost->sName));` |
|   105 | 1118 | `		ReflectMapAddBool(pCtx, pInfo, "internal", 1);` |
|   105 | 1119 | `		ReflectMapAddBool(pCtx, pInfo, "closure", 0);` |
|   105 | 1120 | `		ReflectMapAddBool(pCtx, pInfo, "byref", 0);` |
|   105 | 1121 | `		ReflectMapAddBool(pCtx, pInfo, "generator", 0);` |
|   105 | 1122 | `		ReflectMapAddBool(pCtx, pInfo, "strict", 0);` |
|   105 | 1123 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|   105 | 1124 | `		ReflectMapAddInt(pCtx, pInfo, "line", 0);` |
|   105 | 1125 | `		ReflectMapAddInt(pCtx, pInfo, "endline", 0);` |
|   105 | 1126 | `		ReflectMapAddBool(pCtx, pInfo, "doc", 0);` |
|     - | 1127 | `		{` |
|   105 | 1128 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|   105 | 1129 | `			if( pEmpty ){` |
|   105 | 1130 | `				ph7_array_add_strkey_elem(pInfo, "attrs", pEmpty);` |
|    52 | 1131 | `			}` |
|     - | 1132 | `		}` |
|   105 | 1133 | `		if( pHost->zRet ){` |
|   105 | 1134 | `			ReflectMapAddStr(pCtx, pInfo, "rettext", pHost->zRet, (int)SyStrlen(pHost->zRet));` |
|    53 | 1135 | `		}else{` |
|   ! 0 | 1136 | `			ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1137 | `		}` |
|   105 | 1138 | `		ReflectMapAddBool(pCtx, pInfo, "retnullable", 0);` |
|   105 | 1139 | `		if( pParams ){` |
|   105 | 1140 | `			ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|    52 | 1141 | `		}` |
|   105 | 1142 | `		ReflectMapAddBool(pCtx, pInfo, "variadic", 0);` |
|   105 | 1143 | `		ReflectMapAddInt(pCtx, pInfo, "minarg", (sxi64)pHost->nMinArg);` |
|   105 | 1144 | `		if( pHost->zSig ){` |
|   105 | 1145 | `			ReflectMapAddStr(pCtx, pInfo, "sig", pHost->zSig, (int)SyStrlen(pHost->zSig));` |
|    53 | 1146 | `		}else{` |
|   ! 0 | 1147 | `			ReflectMapAddStr(pCtx, pInfo, "sig", "", 0);` |
|     - | 1148 | `		}` |
|   105 | 1149 | `		ph7_result_value(pCtx, pInfo);` |
|   105 | 1150 | `		return PH7_OK;` |
|     - | 1151 | `	}` |
|   547 | 1152 | `	ReflectFillFuncCommon(pCtx, pInfo, pFunc);` |
|   547 | 1153 | `	ReflectMapAddInt(pCtx, pInfo, "minarg", -1);` |
|   547 | 1154 | `	if( (pFunc->iFlags & VM_FUNC_INTERNAL) && SySetUsed(&pFunc->aArgs) == 0 && pMeth == 0 ){` |
|     - | 1155 | `		/* Embedded-PHP builtin (max/min...): declared argless, actual` |
|     - | 1156 | `		 * signature comes from the static table */` |
|     5 | 1157 | `		const char *zRet = 0;` |
|     5 | 1158 | `		const char *zSig = PH7_VmBuiltinSigLookup(SyStringData(&pFunc->sName), SyStringLength(&pFunc->sName), &zRet);` |
|     5 | 1159 | `		if( zSig ){` |
|     5 | 1160 | `			ReflectMapAddStr(pCtx, pInfo, "sig", zSig, (int)SyStrlen(zSig));` |
|     2 | 1161 | `		}` |
|     5 | 1162 | `		if( zRet && SyStringLength(&pFunc->sReturnTypeName) == 0 ){` |
|     5 | 1163 | `			ReflectMapAddStr(pCtx, pInfo, "ret2", zRet, (int)SyStrlen(zRet));` |
|     2 | 1164 | `		}` |
|     2 | 1165 | `	}` |
|   547 | 1166 | `	if( pMeth && pClass ){` |
|   259 | 1167 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|   259 | 1168 | `		ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   259 | 1169 | `		ReflectMapAddStr(pCtx, pInfo, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   259 | 1170 | `		ReflectMapAddInt(pCtx, pInfo, "vis", (sxi64)pMeth->iProtection);` |
|   259 | 1171 | `		ReflectMapAddBool(pCtx, pInfo, "mstatic", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   259 | 1172 | `		ReflectMapAddBool(pCtx, pInfo, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|   259 | 1173 | `		ReflectMapAddBool(pCtx, pInfo, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   129 | 1174 | `	}` |
|   547 | 1175 | `	if( pClosure ){` |
|     - | 1176 | `		SyString sAttr;` |
|     - | 1177 | `		ph7_value *pAttr;` |
|     - | 1178 | `		ph7_value *pUsed;` |
|    25 | 1179 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|    25 | 1180 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    25 | 1181 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|   ! 0 | 1182 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 1183 | `			if( pKey ){` |
|   ! 0 | 1184 | `				ph7_value_string(pKey, "this", 4);` |
|   ! 0 | 1185 | `				ph7_array_add_elem(pInfo, pKey, pAttr);` |
|   ! 0 | 1186 | `			}` |
|   ! 0 | 1187 | `		}else{` |
|    25 | 1188 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|     - | 1189 | `		}` |
|    25 | 1190 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|    25 | 1191 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    25 | 1192 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|   ! 0 | 1193 | `			ReflectMapAddStr(pCtx, pInfo, "scope", (const char *)SyBlobData(&pAttr->sBlob),` |
|   ! 0 | 1194 | `				(int)SyBlobLength(&pAttr->sBlob));` |
|   ! 0 | 1195 | `		}else{` |
|    25 | 1196 | `			ReflectMapAddNull(pCtx, pInfo, "scope");` |
|     - | 1197 | `		}` |
|     - | 1198 | `		/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */` |
|    25 | 1199 | `		pUsed = ph7_context_new_array(pCtx);` |
|    25 | 1200 | `		if( pUsed ){` |
|    25 | 1201 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     - | 1202 | `			sxu32 n;` |
|    53 | 1203 | `			for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; n++ ){` |
|    29 | 1204 | `				if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){` |
|    15 | 1205 | `					continue;` |
|     - | 1206 | `				}` |
|    14 | 1207 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|     8 | 1208 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|   ! 0 | 1209 | `					continue;` |
|     - | 1210 | `				}` |
|    15 | 1211 | `				ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, &aEnv[n].sValue);` |
|     8 | 1212 | `			}` |
|    25 | 1213 | `			ph7_array_add_strkey_elem(pInfo, "used", pUsed);` |
|    12 | 1214 | `		}` |
|    12 | 1215 | `	}` |
|   547 | 1216 | `	ph7_result_value(pCtx, pInfo);` |
|   547 | 1217 | `	return PH7_OK;` |
|   327 | 1218 | `}` |
|     - | 1219 | `/*` |
|     - | 1220 | ` * mixed __reflect_param_default(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1221 | ` * Evaluate a parameter's compiled default expression.` |
|     - | 1222 | ` */` |
|    12 | 1223 | `static int vm_builtin_reflect_param_default(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1224 | `{` |
|     - | 1225 | `	ph7_vm_func *pFunc;` |
|     - | 1226 | `	ph7_vm_func_arg *pArg;` |
|     - | 1227 | `	ph7_value sValue;` |
|     - | 1228 | `	sxu32 nIdx;` |
|    13 | 1229 | `	if( nArg < 3 ){` |
|   ! 0 | 1230 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1231 | `		return PH7_OK;` |
|     - | 1232 | `	}` |
|    13 | 1233 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|    13 | 1234 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|    12 | 1235 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|    13 | 1236 | `	 \|\| SySetUsed(&pArg->aByteCode) < 1 ){` |
|   ! 0 | 1237 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1238 | `		return PH7_OK;` |
|     - | 1239 | `	}` |
|    13 | 1240 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|    13 | 1241 | `	VmLocalExec(pCtx->pVm, &pArg->aByteCode, &sValue, FALSE);` |
|    13 | 1242 | `	ph7_result_value(pCtx, &sValue);` |
|    13 | 1243 | `	PH7_MemObjRelease(&sValue);` |
|    13 | 1244 | `	return PH7_OK;` |
|     7 | 1245 | `}` |
|     - | 1246 | `/*` |
|     - | 1247 | ` * string\|null __reflect_param_defconst(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1248 | ` * When a parameter's default is a plain global-constant reference, its` |
|     - | 1249 | ` * source name; null otherwise. A constant default compiles to exactly` |
|     - | 1250 | ` * [ OP_LOADC (EXPAND) , OP_DONE ] with the name in the literal table.` |
|     - | 1251 | ` */` |
|     6 | 1252 | `static int vm_builtin_reflect_param_defconst(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1253 | `{` |
|     - | 1254 | `	ph7_vm_func *pFunc;` |
|     - | 1255 | `	ph7_vm_func_arg *pArg;` |
|     - | 1256 | `	VmInstr *aInstr;` |
|     - | 1257 | `	ph7_value *pLit;` |
|     - | 1258 | `	sxu32 nIdx;` |
|     7 | 1259 | `	if( nArg < 3 ){` |
|   ! 0 | 1260 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1261 | `		return PH7_OK;` |
|     - | 1262 | `	}` |
|     7 | 1263 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|     7 | 1264 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|     6 | 1265 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|     7 | 1266 | `	 \|\| SySetUsed(&pArg->aByteCode) != 2 ){` |
|     3 | 1267 | `		ph7_result_null(pCtx);` |
|     3 | 1268 | `		return PH7_OK;` |
|     - | 1269 | `	}` |
|     5 | 1270 | `	aInstr = (VmInstr *)SySetBasePtr(&pArg->aByteCode);` |
|     4 | 1271 | `	if( aInstr[0].iOp != PH7_OP_LOADC \|\| (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0` |
|     5 | 1272 | `	 \|\| aInstr[1].iOp != PH7_OP_DONE ){` |
|   ! 0 | 1273 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1274 | `		return PH7_OK;` |
|     - | 1275 | `	}` |
|     5 | 1276 | `	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);` |
|     5 | 1277 | `	if( pLit == 0 \|\| SyBlobLength(&pLit->sBlob) < 1 ){` |
|   ! 0 | 1278 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1279 | `		return PH7_OK;` |
|     - | 1280 | `	}` |
|     5 | 1281 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&pLit->sBlob), (int)SyBlobLength(&pLit->sBlob));` |
|     5 | 1282 | `	return PH7_OK;` |
|     4 | 1283 | `}` |
|     - | 1284 | `/*` |
|     - | 1285 | ` * mixed __reflect_invoke(mixed $target, ?string $method, ?object $this, array $args)` |
|     - | 1286 | ` * Visibility-bypassing invocation (methods dispatch by VM name; functions` |
|     - | 1287 | ` * and closures ride PH7_VmCallUserFunction like call_user_func_array).` |
|     - | 1288 | ` */` |
|    20 | 1289 | `static int vm_builtin_reflect_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1290 | `{` |
|    21 | 1291 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1292 | `	ph7_value sResult;` |
|     - | 1293 | `	SySet aCallArg;` |
|     - | 1294 | `	sxi32 rc;` |
|    21 | 1295 | `	if( nArg < 4 ){` |
|   ! 0 | 1296 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1297 | `		return PH7_OK;` |
|     - | 1298 | `	}` |
|    21 | 1299 | `	PH7_MemObjInit(pVm, &sResult);` |
|    21 | 1300 | `	sResult.nIdx = SXU32_HIGH;` |
|    21 | 1301 | `	SySetInit(&aCallArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    21 | 1302 | `	ReflectCollectArgs(pCtx, apArg[3], &aCallArg, 0);` |
|    21 | 1303 | `	if( (apArg[1]->iFlags & MEMOBJ_STRING) && SyBlobLength(&apArg[1]->sBlob) > 0 ){` |
|    11 | 1304 | `		ph7_class *pClass = 0;` |
|    11 | 1305 | `		ph7_class_method *pMeth = 0;` |
|    11 | 1306 | `		ph7_class_instance *pThis = 0;` |
|    11 | 1307 | `		ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, 0);` |
|    11 | 1308 | `		if( pMeth == 0 ){` |
|   ! 0 | 1309 | `			SySetRelease(&aCallArg);` |
|   ! 0 | 1310 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1311 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1312 | `			return PH7_OK;` |
|     - | 1313 | `		}` |
|    11 | 1314 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     7 | 1315 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     3 | 1316 | `		}` |
|     - | 1317 | `		/* Reflection ignores method visibility (PHP 8.1+); the flag is` |
|     - | 1318 | `		 * consumed by the first OP_CALL, i.e. this synthetic one. */` |
|    11 | 1319 | `		pVm->bReflectBypass = 1;` |
|    16 | 1320 | `		rc = PH7_VmCallClassMethod(pVm, pThis, pMeth, &sResult,` |
|    10 | 1321 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg));` |
|    11 | 1322 | `		pVm->bReflectBypass = 0;` |
|     6 | 1323 | `	}else{` |
|    16 | 1324 | `		rc = PH7_VmCallUserFunction(pVm, apArg[0],` |
|    10 | 1325 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg), &sResult);` |
|     - | 1326 | `	}` |
|    21 | 1327 | `	SySetRelease(&aCallArg);` |
|    21 | 1328 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 | 1329 | `		PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1330 | `		return rc;` |
|     - | 1331 | `	}` |
|    21 | 1332 | `	ph7_result_value(pCtx, &sResult);` |
|    21 | 1333 | `	PH7_MemObjRelease(&sResult);` |
|    21 | 1334 | `	return PH7_OK;` |
|    11 | 1335 | `}` |
|     - | 1336 | `/*` |
|     - | 1337 | ` * Closure __reflect_closure(mixed $target, ?string $method, ?object $this)` |
|     - | 1338 | ` * Mint a Closure for a function or method, bound and scoped like the` |
|     - | 1339 | ` * first-class-callable path.` |
|     - | 1340 | ` */` |
|     6 | 1341 | `static int vm_builtin_reflect_closure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1342 | `{` |
|     7 | 1343 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 1344 | `	ph7_class *pClass = 0;` |
|     7 | 1345 | `	ph7_class_method *pMeth = 0;` |
|     7 | 1346 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1347 | `	ph7_vm_func *pFunc;` |
|     7 | 1348 | `	if( nArg < 3 ){` |
|   ! 0 | 1349 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1350 | `		return PH7_OK;` |
|     - | 1351 | `	}` |
|     7 | 1352 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, &pClosure);` |
|     7 | 1353 | `	if( pClosure ){` |
|     - | 1354 | `		/* Already a Closure: hand the same instance back */` |
|   ! 0 | 1355 | `		return ReflectResultExistingObject(pCtx, pClosure);` |
|     - | 1356 | `	}` |
|     7 | 1357 | `	if( pMeth && pClass ){` |
|     5 | 1358 | `		ph7_class_instance *pThis = 0;` |
|     5 | 1359 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     3 | 1360 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     1 | 1361 | `		}` |
|     7 | 1362 | `		return ReflectResultObject(pCtx,` |
|     4 | 1363 | `			PH7_VmNewClosure(pVm, &pMeth->sFunc.sName, pThis, &pClass->sName));` |
|     - | 1364 | `	}` |
|     3 | 1365 | `	if( pFunc ){` |
|     3 | 1366 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &pFunc->sName, 0, 0));` |
|     - | 1367 | `	}` |
|     - | 1368 | `	/* Host function by name */` |
|   ! 0 | 1369 | `	if( apArg[0]->iFlags & MEMOBJ_STRING ){` |
|     - | 1370 | `		SyString sName;` |
|   ! 0 | 1371 | `		SyStringInitFromBuf(&sName, (const char *)SyBlobData(&apArg[0]->sBlob), SyBlobLength(&apArg[0]->sBlob));` |
|   ! 0 | 1372 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &sName, 0, 0));` |
|     - | 1373 | `	}` |
|   ! 0 | 1374 | `	ph7_result_null(pCtx);` |
|   ! 0 | 1375 | `	return PH7_OK;` |
|     4 | 1376 | `}` |
|     - | 1377 | `/*` |
|     - | 1378 | ` * Resolve a Generator object into its wrapper. Mirrors the static` |
|     - | 1379 | ` * VmGeneratorExtractCtx in vm.c: the $__ctx attribute carries the` |
|     - | 1380 | ` * ph7_generator pointer as a resource value.` |
|     - | 1381 | ` */` |
|    22 | 1382 | `static ph7_generator * ReflectGeneratorCtx(ph7_vm *pVm, ph7_value *pVal)` |
|     1 | 1383 | `{` |
|     - | 1384 | `	ph7_class_instance *pThis;` |
|     - | 1385 | `	ph7_value *pAttr;` |
|     - | 1386 | `	SyString sAttr;` |
|    23 | 1387 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVm->pGeneratorClass == 0 ){` |
|   ! 0 | 1388 | `		return 0;` |
|     - | 1389 | `	}` |
|    23 | 1390 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    23 | 1391 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|   ! 0 | 1392 | `		return 0;` |
|     - | 1393 | `	}` |
|    23 | 1394 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    23 | 1395 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    23 | 1396 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|   ! 0 | 1397 | `		return 0;` |
|     - | 1398 | `	}` |
|    23 | 1399 | `	return (ph7_generator *)pAttr->x.pOther;` |
|    12 | 1400 | `}` |
|     - | 1401 | `/*` |
|     - | 1402 | ` * array\|null __reflect_gen_info(Generator $g)` |
|     - | 1403 | ` * {state, closed, executing, kind ('fn'\|'method'), name, class?, this}` |
|     - | 1404 | ` */` |
|    16 | 1405 | `static int vm_builtin_reflect_gen_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1406 | `{` |
|    17 | 1407 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1408 | `	ph7_generator *pGen;` |
|     - | 1409 | `	ph7_exec_ctx *pExec;` |
|     - | 1410 | `	ph7_value *pInfo;` |
|    17 | 1411 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 \|\| pGen->pCtx == 0 ){` |
|   ! 0 | 1412 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1413 | `		return PH7_OK;` |
|     - | 1414 | `	}` |
|    17 | 1415 | `	pExec = pGen->pCtx;` |
|    17 | 1416 | `	pInfo = ph7_context_new_array(pCtx);` |
|    17 | 1417 | `	if( pInfo == 0 ){` |
|   ! 0 | 1418 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1419 | `		return PH7_OK;` |
|     - | 1420 | `	}` |
|    17 | 1421 | `	ReflectMapAddInt(pCtx, pInfo, "state", (sxi64)pExec->iState);` |
|    24 | 1422 | `	ReflectMapAddBool(pCtx, pInfo, "closed",` |
|    16 | 1423 | `		pExec->iState == PH7_CTX_STATE_COMPLETED \|\| pExec->iState == PH7_CTX_STATE_CLOSED);` |
|    17 | 1424 | `	ReflectMapAddBool(pCtx, pInfo, "executing", pVm->pActiveCtx == pExec);` |
|    17 | 1425 | `	if( pExec->pFunc ){` |
|    17 | 1426 | `		ph7_vm_func *pFunc = pExec->pFunc;` |
|    19 | 1427 | `		if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     5 | 1428 | `			ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|     5 | 1429 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "method", sizeof("method")-1);` |
|     5 | 1430 | `			ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|     3 | 1431 | `		}else{` |
|    13 | 1432 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "fn", sizeof("fn")-1);` |
|     - | 1433 | `		}` |
|    17 | 1434 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|     8 | 1435 | `	}` |
|     - | 1436 | `	{` |
|     - | 1437 | `		/* The coroutine frame installs $this as a frame VARIABLE (see` |
|     - | 1438 | `		 * VmFiberSetupFrame), not as pFrame->pThis — check both. */` |
|    17 | 1439 | `		ph7_value *pThisVal = 0;` |
|    17 | 1440 | `		if( pExec->pFrame ){` |
|    17 | 1441 | `			SyHashEntry *pVar = SyHashGet(&pExec->pFrame->hVar, "this", sizeof("this")-1);` |
|    17 | 1442 | `			if( pVar ){` |
|     5 | 1443 | `				ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj, (sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|     5 | 1444 | `				if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|     5 | 1445 | `					pThisVal = pSlot;` |
|     2 | 1446 | `				}` |
|     2 | 1447 | `			}` |
|    17 | 1448 | `			if( pThisVal == 0 && pExec->pFrame->pThis ){` |
|     - | 1449 | `				ph7_value sThis;` |
|   ! 0 | 1450 | `				ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 1451 | `				PH7_MemObjInit(pVm, &sThis);` |
|   ! 0 | 1452 | `				pExec->pFrame->pThis->iRef++;` |
|   ! 0 | 1453 | `				sThis.x.pOther = pExec->pFrame->pThis;` |
|   ! 0 | 1454 | `				MemObjSetType(&sThis, MEMOBJ_OBJ);` |
|   ! 0 | 1455 | `				if( pKey ){` |
|   ! 0 | 1456 | `					ph7_value_string(pKey, "this", 4);` |
|   ! 0 | 1457 | `					ph7_array_add_elem(pInfo, pKey, &sThis); /* copies (takes its own ref) */` |
|   ! 0 | 1458 | `				}` |
|   ! 0 | 1459 | `				PH7_MemObjRelease(&sThis);` |
|   ! 0 | 1460 | `				pThisVal = (ph7_value *)1; /* handled */` |
|   ! 0 | 1461 | `			}` |
|     8 | 1462 | `		}` |
|    17 | 1463 | `		if( pThisVal == 0 ){` |
|    13 | 1464 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|    11 | 1465 | `		}else if( pThisVal != (ph7_value *)1 ){` |
|     5 | 1466 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|     5 | 1467 | `			if( pKey ){` |
|     5 | 1468 | `				ph7_value_string(pKey, "this", 4);` |
|     5 | 1469 | `				ph7_array_add_elem(pInfo, pKey, pThisVal);` |
|     2 | 1470 | `			}` |
|     2 | 1471 | `		}` |
|     - | 1472 | `	}` |
|    17 | 1473 | `	ph7_result_value(pCtx, pInfo);` |
|    17 | 1474 | `	return PH7_OK;` |
|     9 | 1475 | `}` |
|     - | 1476 | `/*` |
|     - | 1477 | ` * Generator __reflect_gen_exec(Generator $g)` |
|     - | 1478 | `` * Follow `yield from` delegation to the innermost executing generator`` |
|     - | 1479 | ` * (PHP's ReflectionGenerator::getExecutingGenerator).` |
|     - | 1480 | ` */` |
|     4 | 1481 | `static int vm_builtin_reflect_gen_exec(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1482 | `{` |
|     5 | 1483 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1484 | `	ph7_generator *pGen;` |
|     - | 1485 | `	ph7_value *pCur;` |
|     5 | 1486 | `	int iDepth = 0;` |
|     5 | 1487 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 ){` |
|   ! 0 | 1488 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1489 | `		return PH7_OK;` |
|     - | 1490 | `	}` |
|     5 | 1491 | `	pCur = apArg[0];` |
|     9 | 1492 | `	while( pGen && pGen->pCtx && pGen->pCtx->iDelegateState == 3` |
|     8 | 1493 | `	 && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|     3 | 1494 | `		ph7_generator *pInner = ReflectGeneratorCtx(pVm, &pGen->pCtx->sDelegate);` |
|     3 | 1495 | `		if( pInner == 0 ){` |
|   ! 0 | 1496 | `			break;` |
|     - | 1497 | `		}` |
|     3 | 1498 | `		pCur = &pGen->pCtx->sDelegate;` |
|     3 | 1499 | `		pGen = pInner;` |
|     3 | 1500 | `		iDepth++;` |
|     1 | 1501 | `	}` |
|     5 | 1502 | `	return ReflectResultExistingObject(pCtx, (ph7_class_instance *)pCur->x.pOther);` |
|     3 | 1503 | `}` |
|     - | 1504 | `/*` |
|     - | 1505 | ` * array\|null __reflect_const_info(string $name)` |
|     - | 1506 | ` * Global-constant descriptor: {value}. Null when undefined. File/origin` |
|     - | 1507 | ` * metadata arrives with the C5 constant-metadata work.` |
|     - | 1508 | ` */` |
|    36 | 1509 | `static int vm_builtin_reflect_const_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1510 | `{` |
|    37 | 1511 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1512 | `	SyHashEntry *pEntry;` |
|     - | 1513 | `	ph7_constant *pCons;` |
|     - | 1514 | `	ph7_value *pInfo;` |
|     - | 1515 | `	ph7_value sValue;` |
|     - | 1516 | `	const char *zName;` |
|     - | 1517 | `	int nLen;` |
|    37 | 1518 | `	if( nArg < 1 ){` |
|   ! 0 | 1519 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1520 | `		return PH7_OK;` |
|     - | 1521 | `	}` |
|    37 | 1522 | `	zName = ph7_value_to_string(apArg[0], &nLen);` |
|    37 | 1523 | `	pEntry = nLen > 0 ? SyHashGet(&pVm->hConstant, (const void *)zName, (sxu32)nLen) : 0;` |
|    37 | 1524 | `	if( pEntry == 0 ){` |
|     3 | 1525 | `		ph7_result_null(pCtx);` |
|     3 | 1526 | `		return PH7_OK;` |
|     - | 1527 | `	}` |
|    35 | 1528 | `	pCons = (ph7_constant *)pEntry->pUserData;` |
|    35 | 1529 | `	pInfo = ph7_context_new_array(pCtx);` |
|    35 | 1530 | `	if( pInfo == 0 ){` |
|   ! 0 | 1531 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1532 | `		return PH7_OK;` |
|     - | 1533 | `	}` |
|    35 | 1534 | `	PH7_MemObjInit(pVm, &sValue);` |
|    35 | 1535 | `	if( pCons->xExpand ){` |
|    35 | 1536 | `		pCons->xExpand(&sValue, pCons->pUserData);` |
|    17 | 1537 | `	}` |
|     - | 1538 | `	{` |
|    35 | 1539 | `		ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|    35 | 1540 | `		if( pKey ){` |
|    35 | 1541 | `			ph7_value_string(pKey, "value", 5);` |
|    35 | 1542 | `			ph7_array_add_elem(pInfo, pKey, &sValue);` |
|    17 | 1543 | `		}` |
|     - | 1544 | `	}` |
|    35 | 1545 | `	PH7_MemObjRelease(&sValue);` |
|    35 | 1546 | `	ReflectMapAddBool(pCtx, pInfo, "internal", pCons->bUserDefined == 0);` |
|    35 | 1547 | `	if( SyStringLength(&pCons->sFile) > 0 ){` |
|    21 | 1548 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));` |
|    11 | 1549 | `	}else{` |
|    15 | 1550 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1551 | `	}` |
|    35 | 1552 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pCons->nLine);` |
|    35 | 1553 | `	ph7_result_value(pCtx, pInfo);` |
|    35 | 1554 | `	return PH7_OK;` |
|    19 | 1555 | `}` |
|     - | 1556 | `/*` |
|     - | 1557 | ` * int\|null __reflect_ref_id(array $arr, int\|string $key)` |
|     - | 1558 | ` * The element's slot index when the element is a reference (its slot has` |
|     - | 1559 | ` * a reference-table record with at least two links), null otherwise.` |
|     - | 1560 | ` */` |
|     6 | 1561 | `static int vm_builtin_reflect_ref_id(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1562 | `{` |
|     - | 1563 | `	ph7_hashmap *pMap;` |
|     7 | 1564 | `	ph7_hashmap_node *pNode = 0;` |
|     7 | 1565 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|   ! 0 | 1566 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1567 | `		return PH7_OK;` |
|     - | 1568 | `	}` |
|     7 | 1569 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 | 1570 | `	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK \|\| pNode == 0 ){` |
|   ! 0 | 1571 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1572 | `		return PH7_OK;` |
|     - | 1573 | `	}` |
|     7 | 1574 | `	if( PH7_VmSlotRefCount(pCtx->pVm, pNode->nValIdx) < 2 ){` |
|     3 | 1575 | `		ph7_result_null(pCtx);` |
|     3 | 1576 | `		return PH7_OK;` |
|     - | 1577 | `	}` |
|     5 | 1578 | `	ph7_result_int64(pCtx, (sxi64)pNode->nValIdx);` |
|     5 | 1579 | `	return PH7_OK;` |
|     4 | 1580 | `}` |
|     - | 1581 | `/*` |
|     - | 1582 | ` * array\|null __reflect_attr_args(string $kind, mixed $target, ?string $member,` |
|     - | 1583 | ` *                                int $paramIdx, int $attrIdx)` |
|     - | 1584 | ` * Evaluate the recorded argument expressions of one declared attribute:` |
|     - | 1585 | ` * kind 'class' (target = class), 'attr' (class + property/constant name),` |
|     - | 1586 | ` * 'method' (class + method), 'fn' (function name or Closure), 'param'` |
|     - | 1587 | ` * (function spec + parameter index). Named arguments become string keys.` |
|     - | 1588 | ` */` |
|    36 | 1589 | `static int vm_builtin_reflect_attr_args(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1590 | `{` |
|    37 | 1591 | `	ph7_vm *pVm = pCtx->pVm;` |
|    37 | 1592 | `	SySet *pAttrs = 0;` |
|     - | 1593 | `	ph7_attribute *pAttrRec;` |
|     - | 1594 | `	ph7_value *pOut;` |
|     - | 1595 | `	const char *zKind;` |
|     - | 1596 | `	int nKind;` |
|     - | 1597 | `	sxu32 nAttrIdx, n;` |
|    37 | 1598 | `	if( nArg < 5 ){` |
|   ! 0 | 1599 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1600 | `		return PH7_OK;` |
|     - | 1601 | `	}` |
|    37 | 1602 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|    37 | 1603 | `	nAttrIdx = (sxu32)ph7_value_to_int(apArg[4]);` |
|    49 | 1604 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|    25 | 1605 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|    25 | 1606 | `		if( pClass ){ pAttrs = &pClass->aAttrs; }` |
|    27 | 1607 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|     5 | 1608 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|     5 | 1609 | `		ph7_class_attr *pMember = pClass ? ReflectFetchAttr(pClass, apArg[2]) : 0;` |
|     5 | 1610 | `		if( pMember ){ pAttrs = &pMember->aAttrs; }` |
|    12 | 1611 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|     3 | 1612 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1613 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|    10 | 1614 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|     5 | 1615 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], 0, 0, 0, 0, 0);` |
|     5 | 1616 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|     5 | 1617 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|     3 | 1618 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1619 | `		ph7_vm_func_arg *pParam = pFunc` |
|     2 | 1620 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|     3 | 1621 | `		if( pParam ){ pAttrs = &pParam->aAttrs; }` |
|     1 | 1622 | `	}` |
|    36 | 1623 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|    37 | 1624 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 | 1625 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1626 | `		return PH7_OK;` |
|     - | 1627 | `	}` |
|    71 | 1628 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|    35 | 1629 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|     - | 1630 | `		ph7_value sValue;` |
|    35 | 1631 | `		PH7_MemObjInit(pVm, &sValue);` |
|    35 | 1632 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|    35 | 1633 | `			VmLocalExec(pVm, &pArgRec->aByteCode, &sValue, FALSE);` |
|    17 | 1634 | `		}` |
|    35 | 1635 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|     5 | 1636 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|     3 | 1637 | `		}else{` |
|    31 | 1638 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|     - | 1639 | `		}` |
|    35 | 1640 | `		PH7_MemObjRelease(&sValue);` |
|    18 | 1641 | `	}` |
|    37 | 1642 | `	ph7_result_value(pCtx, pOut);` |
|    37 | 1643 | `	return PH7_OK;` |
|    19 | 1644 | `}` |
|     - | 1645 | `/*` |
|     - | 1646 | ` * The Reflection classes, in PHP. Chunk 1: exceptions, Reflector,` |
|     - | 1647 | ` * Reflection, ReflectionClass, ReflectionObject (plus get_debug_type,` |
|     - | 1648 | ` * which the TypeError messages need and PHP 8.0 ships natively).` |
|     - | 1649 | ` */` |
|     - | 1650 | `static const char zReflectLib1[] =` |
|     - | 1651 | `"function get_debug_type($value){"` |
|     - | 1652 | `" if(is_object($value)){ return get_class($value); }"` |
|     - | 1653 | `" if(is_bool($value)){ return 'bool'; }"` |
|     - | 1654 | `" if(is_int($value)){ return 'int'; }"` |
|     - | 1655 | `" if(is_float($value)){ return 'float'; }"` |
|     - | 1656 | `" if(is_string($value)){ return 'string'; }"` |
|     - | 1657 | `" if(is_array($value)){ return 'array'; }"` |
|     - | 1658 | `" if($value === null){ return 'null'; }"` |
|     - | 1659 | `" return gettype($value);"` |
|     - | 1660 | `"}"` |
|     - | 1661 | `"interface Reflector extends Stringable {}"` |
|     - | 1662 | `"class ReflectionException extends Exception {}"` |
|     - | 1663 | `"class Reflection {"` |
|     - | 1664 | `" public static function getModifierNames($modifiers){"` |
|     - | 1665 | `"  $names = array();"` |
|     - | 1666 | `"  if($modifiers & 64){ $names[] = 'abstract'; }"` |
|     - | 1667 | `"  if($modifiers & 32){ $names[] = 'final'; }"` |
|     - | 1668 | `"  if($modifiers & 1){ $names[] = 'public'; }"` |
|     - | 1669 | `"  if($modifiers & 2){ $names[] = 'protected'; }"` |
|     - | 1670 | `"  if($modifiers & 4){ $names[] = 'private'; }"` |
|     - | 1671 | `"  if($modifiers & 16){ $names[] = 'static'; }"` |
|     - | 1672 | `"  if($modifiers & 128){ $names[] = 'readonly'; }"` |
|     - | 1673 | `"  return $names;"` |
|     - | 1674 | `" }"` |
|     - | 1675 | `"}"` |
|     - | 1676 | `"class ReflectionClass implements Reflector {"` |
|     - | 1677 | `" const IS_IMPLICIT_ABSTRACT = 16;"` |
|     - | 1678 | `" const IS_EXPLICIT_ABSTRACT = 64;"` |
|     - | 1679 | `" const IS_FINAL = 32;"` |
|     - | 1680 | `" const IS_READONLY = 65536;"` |
|     - | 1681 | `" const SKIP_INITIALIZATION_ON_SERIALIZE = 8;"` |
|     - | 1682 | `" const SKIP_DESTRUCTOR = 16;"` |
|     - | 1683 | `" public $name;"` |
|     - | 1684 | `" protected $__obj = null;"` |
|     - | 1685 | `" public function __construct($objectOrClass){"` |
|     - | 1686 | `"  if(!is_object($objectOrClass) && !is_string($objectOrClass)){"` |
|     - | 1687 | `"   if(is_int($objectOrClass) \|\| is_float($objectOrClass) \|\| is_bool($objectOrClass)){"` |
|     - | 1688 | `"    $objectOrClass = (string)$objectOrClass;"` |
|     - | 1689 | `"   }else{"` |
|     - | 1690 | `"    throw new TypeError('ReflectionClass::__construct(): Argument #1 ($objectOrClass) must be of type object\|string, '.get_debug_type($objectOrClass).' given');"` |
|     - | 1691 | `"   }"` |
|     - | 1692 | `"  }"` |
|     - | 1693 | `"  $info = __reflect_class_info($objectOrClass);"` |
|     - | 1694 | `"  if($info === null){"` |
|     - | 1695 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 1696 | `"  }"` |
|     - | 1697 | `"  $this->name = $info['name'];"` |
|     - | 1698 | `" }"` |
|     - | 1699 | `" protected function __rinfo(){ return __reflect_class_info($this->name); }"` |
|     - | 1700 | `" public function getName(){ return $this->name; }"` |
|     - | 1701 | `" public function getShortName(){"` |
|     - | 1702 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1703 | `"  if($p === false){ return $this->name; }"` |
|     - | 1704 | `"  return substr($this->name,$p+1);"` |
|     - | 1705 | `" }"` |
|     - | 1706 | `" public function getNamespaceName(){"` |
|     - | 1707 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1708 | `"  if($p === false){ return ''; }"` |
|     - | 1709 | `"  return substr($this->name,0,$p);"` |
|     - | 1710 | `" }"` |
|     - | 1711 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 1712 | `" public function isInternal(){ $i = $this->__rinfo(); return $i['internal']; }"` |
|     - | 1713 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 1714 | `" public function isInterface(){ $i = $this->__rinfo(); return $i['interface']; }"` |
|     - | 1715 | `" public function isTrait(){ $i = $this->__rinfo(); return $i['trait']; }"` |
|     - | 1716 | `" public function isAbstract(){ $i = $this->__rinfo(); return $i['abstract']; }"` |
|     - | 1717 | `" public function isFinal(){ $i = $this->__rinfo(); return $i['final']; }"` |
|     - | 1718 | `" public function isReadOnly(){ $i = $this->__rinfo(); return $i['readonly']; }"` |
|     - | 1719 | `" public function isEnum(){ return false; }"` |
|     - | 1720 | `" public function isAnonymous(){ return strpos($this->name,'class@anonymous') === 0; }"` |
|     - | 1721 | `" public function getModifiers(){"` |
|     - | 1722 | `"  $i = $this->__rinfo();"` |
|     - | 1723 | `"  $m = 0;"` |
|     - | 1724 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 1725 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 1726 | `"  if($i['readonly']){ $m \|= 65536; }"` |
|     - | 1727 | `"  return $m;"` |
|     - | 1728 | `" }"` |
|     - | 1729 | `" public function getParentClass(){"` |
|     - | 1730 | `"  $i = $this->__rinfo();"` |
|     - | 1731 | `"  if($i['parent'] === null){ return false; }"` |
|     - | 1732 | `"  return new ReflectionClass($i['parent']);"` |
|     - | 1733 | `" }"` |
|     - | 1734 | `" public function getInterfaceNames(){ $i = $this->__rinfo(); return $i['interfaces']; }"` |
|     - | 1735 | `" public function getInterfaces(){"` |
|     - | 1736 | `"  $i = $this->__rinfo();"` |
|     - | 1737 | `"  $out = array();"` |
|     - | 1738 | `"  foreach($i['interfaces'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1739 | `"  return $out;"` |
|     - | 1740 | `" }"` |
|     - | 1741 | `" public function getTraitNames(){ $i = $this->__rinfo(); return $i['traits']; }"` |
|     - | 1742 | `" public function getTraits(){"` |
|     - | 1743 | `"  $i = $this->__rinfo();"` |
|     - | 1744 | `"  $out = array();"` |
|     - | 1745 | `"  foreach($i['traits'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1746 | `"  return $out;"` |
|     - | 1747 | `" }"` |
|     - | 1748 | `" public function getTraitAliases(){ return array(); }"` |
|     - | 1749 | `" public function implementsInterface($interface){"` |
|     - | 1750 | `"  if($interface instanceof ReflectionClass){ $interface = $interface->name; }"` |
|     - | 1751 | `"  $target = __reflect_class_info($interface);"` |
|     - | 1752 | `"  if($target === null){"` |
|     - | 1753 | `"   throw new ReflectionException('Interface \"'.$interface.'\" does not exist');"` |
|     - | 1754 | `"  }"` |
|     - | 1755 | `"  if(!$target['interface']){"` |
|     - | 1756 | `"   throw new ReflectionException($target['name'].' is not an interface');"` |
|     - | 1757 | `"  }"` |
|     - | 1758 | `"  $name = $target['name'];"` |
|     - | 1759 | `"  if($this->name === $name){ return true; }"` |
|     - | 1760 | `"  $i = $this->__rinfo();"` |
|     - | 1761 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1762 | `"  return false;"` |
|     - | 1763 | `" }"` |
|     - | 1764 | `" public function isSubclassOf($class){"` |
|     - | 1765 | `"  if($class instanceof ReflectionClass){ $class = $class->name; }"` |
|     - | 1766 | `"  $target = __reflect_class_info($class);"` |
|     - | 1767 | `"  if($target === null){"` |
|     - | 1768 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 1769 | `"  }"` |
|     - | 1770 | `"  $name = $target['name'];"` |
|     - | 1771 | `"  if($name === $this->name){ return false; }"` |
|     - | 1772 | `"  $i = $this->__rinfo();"` |
|     - | 1773 | `"  $p = $i['parent'];"` |
|     - | 1774 | `"  while($p !== null){"` |
|     - | 1775 | `"   if($p === $name){ return true; }"` |
|     - | 1776 | `"   $pi = __reflect_class_info($p);"` |
|     - | 1777 | `"   $p = $pi['parent'];"` |
|     - | 1778 | `"  }"` |
|     - | 1779 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1780 | `"  return false;"` |
|     - | 1781 | `" }"` |
|     - | 1782 | `" public function isInstance($object){"` |
|     - | 1783 | `"  if(!is_object($object)){"` |
|     - | 1784 | `"   throw new TypeError('ReflectionClass::isInstance(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 1785 | `"  }"` |
|     - | 1786 | `"  return is_a($object,$this->name);"` |
|     - | 1787 | `" }"` |
|     - | 1788 | `" public function hasMethod($name){"` |
|     - | 1789 | `"  $i = $this->__rinfo();"` |
|     - | 1790 | `"  $l = strtolower($name);"` |
|     - | 1791 | `"  foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ return true; } }"` |
|     - | 1792 | `"  return false;"` |
|     - | 1793 | `" }"` |
|     - | 1794 | `" public function hasProperty($name){"` |
|     - | 1795 | `"  $i = $this->__rinfo();"` |
|     - | 1796 | `"  if(isset($i['props'][$name])){ return true; }"` |
|     - | 1797 | `"  if($this->__obj !== null){ return (__reflect_prop_state($this->__obj, $name) & 1) !== 0; }"` |
|     - | 1798 | `"  return false;"` |
|     - | 1799 | `" }"` |
|     - | 1800 | `" public function hasConstant($name){ $i = $this->__rinfo(); return isset($i['consts'][$name]); }"` |
|     - | 1801 | `" public function getConstant($name){"` |
|     - | 1802 | `"  $i = $this->__rinfo();"` |
|     - | 1803 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 1804 | `"  return __reflect_const_value($this->name,$name);"` |
|     - | 1805 | `" }"` |
|     - | 1806 | `" public function getConstants($filter = null){"` |
|     - | 1807 | `"  $i = $this->__rinfo();"` |
|     - | 1808 | `"  $out = array();"` |
|     - | 1809 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 1810 | `"   if($filter !== null){"` |
|     - | 1811 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 1812 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 1813 | `"   }"` |
|     - | 1814 | `"   $out[$k] = __reflect_const_value($this->name,$k);"` |
|     - | 1815 | `"  }"` |
|     - | 1816 | `"  return $out;"` |
|     - | 1817 | `" }"` |
|     - | 1818 | `" public function getStartLine(){"` |
|     - | 1819 | `"  $i = $this->__rinfo();"` |
|     - | 1820 | `"  if($i['internal']){ return false; }"` |
|     - | 1821 | `"  return $i['line'];"` |
|     - | 1822 | `" }"` |
|     - | 1823 | `" public function getEndLine(){"` |
|     - | 1824 | `"  $i = $this->__rinfo();"` |
|     - | 1825 | `"  if($i['internal']){ return false; }"` |
|     - | 1826 | `"  return $i['endline'];"` |
|     - | 1827 | `" }"` |
|     - | 1828 | `" public function getFileName(){ $i = $this->__rinfo(); return $i['file']; }"` |
|     - | 1829 | `" public function getDocComment(){ $i = $this->__rinfo(); return $i['doc']; }"` |
|     - | 1830 | `" public function isInstantiable(){"` |
|     - | 1831 | `"  $i = $this->__rinfo();"` |
|     - | 1832 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1833 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){ return false; }"` |
|     - | 1834 | `"  return true;"` |
|     - | 1835 | `" }"` |
|     - | 1836 | `" public function isCloneable(){"` |
|     - | 1837 | `"  $i = $this->__rinfo();"` |
|     - | 1838 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1839 | `"  if($i['clonevis'] !== 0 && $i['clonevis'] !== 1){ return false; }"` |
|     - | 1840 | `"  return true;"` |
|     - | 1841 | `" }"` |
|     - | 1842 | `" public function isIterable(){"` |
|     - | 1843 | `"  $i = $this->__rinfo();"` |
|     - | 1844 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1845 | `"  return $i['iterable'];"` |
|     - | 1846 | `" }"` |
|     - | 1847 | `" public function isIterateable(){ return $this->isIterable(); }"` |
|     - | 1848 | `" public function newInstance(...$args){ return $this->__rnew($args); }"` |
|     - | 1849 | `" public function newInstanceArgs(array $args = array()){ return $this->__rnew($args); }"` |
|     - | 1850 | `" protected function __rnew($args){"` |
|     - | 1851 | `"  $i = $this->__rinfo();"` |
|     - | 1852 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1853 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){"` |
|     - | 1854 | `"   throw new ReflectionException('Access to non-public constructor of class '.$this->name);"` |
|     - | 1855 | `"  }"` |
|     - | 1856 | `"  if($i['ctorvis'] === 0 && count($args) > 0){"` |
|     - | 1857 | `"   throw new ReflectionException('Class '.$this->name.' does not have a constructor, so you cannot pass any constructor arguments');"` |
|     - | 1858 | `"  }"` |
|     - | 1859 | `"  return __reflect_new_instance($this->name,$args);"` |
|     - | 1860 | `" }"` |
|     - | 1861 | `" protected function __rcheckInstantiable($i){"` |
|     - | 1862 | `"  if($i['interface']){ throw new Error('Cannot instantiate interface '.$this->name); }"` |
|     - | 1863 | `"  if($i['trait']){ throw new Error('Cannot instantiate trait '.$this->name); }"` |
|     - | 1864 | `"  if($i['abstract']){ throw new Error('Cannot instantiate abstract class '.$this->name); }"` |
|     - | 1865 | `" }"` |
|     - | 1866 | `" public function newInstanceWithoutConstructor(){"` |
|     - | 1867 | `"  $i = $this->__rinfo();"` |
|     - | 1868 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1869 | `"  return __reflect_new_no_ctor($this->name);"` |
|     - | 1870 | `" }"` |
|     - | 1871 | `" public function getStaticProperties(){"` |
|     - | 1872 | `"  $i = $this->__rinfo();"` |
|     - | 1873 | `"  $out = array();"` |
|     - | 1874 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1875 | `"   if($p['static']){ $out[$k] = __reflect_static_value($this->name,$k); }"` |
|     - | 1876 | `"  }"` |
|     - | 1877 | `"  return $out;"` |
|     - | 1878 | `" }"` |
|     - | 1879 | `" public function getStaticPropertyValue($name, ...$def){"` |
|     - | 1880 | `"  $i = $this->__rinfo();"` |
|     - | 1881 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 1882 | `"   if(count($def) > 0){ return $def[0]; }"` |
|     - | 1883 | `"   throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 1884 | `"  }"` |
|     - | 1885 | `"  return __reflect_static_value($this->name,$name);"` |
|     - | 1886 | `" }"` |
|     - | 1887 | `" public function setStaticPropertyValue($name,$value){"` |
|     - | 1888 | `"  $i = $this->__rinfo();"` |
|     - | 1889 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 1890 | `"   throw new ReflectionException('Class '.$this->name.' does not have a property named '.$name);"` |
|     - | 1891 | `"  }"` |
|     - | 1892 | `"  __reflect_static_set($this->name,$name,$value);"` |
|     - | 1893 | `" }"` |
|     - | 1894 | `" public function getDefaultProperties(){"` |
|     - | 1895 | `"  $i = $this->__rinfo();"` |
|     - | 1896 | `"  $out = array();"` |
|     - | 1897 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1898 | `"   if($p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 1899 | `"  }"` |
|     - | 1900 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1901 | `"   if(!$p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 1902 | `"  }"` |
|     - | 1903 | `"  return $out;"` |
|     - | 1904 | `" }"` |
|     - | 1905 | `" public function getProperty($name){"` |
|     - | 1906 | `"  $i = $this->__rinfo();"` |
|     - | 1907 | `"  if(isset($i['props'][$name])){"` |
|     - | 1908 | `"   return new ReflectionProperty($this->name, $name);"` |
|     - | 1909 | `"  }"` |
|     - | 1910 | `"  if($this->__obj !== null && (__reflect_prop_state($this->__obj, $name) & 1)){"` |
|     - | 1911 | `"   return new ReflectionProperty($this->__obj, $name);"` |
|     - | 1912 | `"  }"` |
|     - | 1913 | `"  throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 1914 | `" }"` |
|     - | 1915 | `" public function getProperties($filter = null){"` |
|     - | 1916 | `"  $i = $this->__rinfo();"` |
|     - | 1917 | `"  $out = array();"` |
|     - | 1918 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1919 | `"   if($filter !== null){"` |
|     - | 1920 | `"    $m = ($p['vis'] === 1 ? 1 : ($p['vis'] === 2 ? 2 : 4));"` |
|     - | 1921 | `"    if($p['static']){ $m \|= 16; }"` |
|     - | 1922 | `"    if($p['readonly']){ $m \|= 128; }"` |
|     - | 1923 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 1924 | `"   }"` |
|     - | 1925 | `"   $out[] = new ReflectionProperty($this->name, $k);"` |
|     - | 1926 | `"  }"` |
|     - | 1927 | `"  if($this->__obj !== null){"` |
|     - | 1928 | `"   foreach(__reflect_dyn_props($this->__obj) as $k){"` |
|     - | 1929 | `"    if(isset($i['props'][$k])){ continue; }"` |
|     - | 1930 | `"    if($filter !== null && ($filter & 1) === 0){ continue; }"` |
|     - | 1931 | `"    $out[] = new ReflectionProperty($this->__obj, $k);"` |
|     - | 1932 | `"   }"` |
|     - | 1933 | `"  }"` |
|     - | 1934 | `"  return $out;"` |
|     - | 1935 | `" }"` |
|     - | 1936 | `" public function getMethod($name){"` |
|     - | 1937 | `"  $i = $this->__rinfo();"` |
|     - | 1938 | `"  $found = null;"` |
|     - | 1939 | `"  if(isset($i['methods'][$name])){"` |
|     - | 1940 | `"   $found = $name;"` |
|     - | 1941 | `"  }else{"` |
|     - | 1942 | `"   $l = strtolower($name);"` |
|     - | 1943 | `"   foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ $found = $k; break; } }"` |
|     - | 1944 | `"  }"` |
|     - | 1945 | `"  if($found === null){"` |
|     - | 1946 | `"   throw new ReflectionException('Method '.$this->name.'::'.$name.'() does not exist');"` |
|     - | 1947 | `"  }"` |
|     - | 1948 | `"  return new ReflectionMethod($this->name, $found);"` |
|     - | 1949 | `" }"` |
|     - | 1950 | `" public function getMethods($filter = null){"` |
|     - | 1951 | `"  $i = $this->__rinfo();"` |
|     - | 1952 | `"  $out = array();"` |
|     - | 1953 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 1954 | `"   if($filter !== null){"` |
|     - | 1955 | `"    $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 1956 | `"    if($m['static']){ $mod \|= 16; }"` |
|     - | 1957 | `"    if($m['abstract']){ $mod \|= 64; }"` |
|     - | 1958 | `"    if($m['final']){ $mod \|= 32; }"` |
|     - | 1959 | `"    if(($mod & $filter) === 0){ continue; }"` |
|     - | 1960 | `"   }"` |
|     - | 1961 | `"   $out[] = new ReflectionMethod($this->name, $k);"` |
|     - | 1962 | `"  }"` |
|     - | 1963 | `"  return $out;"` |
|     - | 1964 | `" }"` |
|     - | 1965 | `" public function getConstructor(){"` |
|     - | 1966 | `"  $i = $this->__rinfo();"` |
|     - | 1967 | `"  if(isset($i['methods']['__construct'])){"` |
|     - | 1968 | `"   return new ReflectionMethod($this->name, '__construct');"` |
|     - | 1969 | `"  }"` |
|     - | 1970 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 1971 | `"   if(strtolower($k) === '__construct'){ return new ReflectionMethod($this->name, $k); }"` |
|     - | 1972 | `"  }"` |
|     - | 1973 | `"  if($i['ctorvis'] !== 0 && isset($i['methods'][$this->name])){"` |
|     - | 1974 | `"   return new ReflectionMethod($this->name, $this->name);"` |
|     - | 1975 | `"  }"` |
|     - | 1976 | `"  return null;"` |
|     - | 1977 | `" }"` |
|     - | 1978 | `" public function getReflectionConstant($name){"` |
|     - | 1979 | `"  $i = $this->__rinfo();"` |
|     - | 1980 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 1981 | `"  return new ReflectionClassConstant($this->name, $name);"` |
|     - | 1982 | `" }"` |
|     - | 1983 | `" public function getReflectionConstants($filter = null){"` |
|     - | 1984 | `"  $i = $this->__rinfo();"` |
|     - | 1985 | `"  $out = array();"` |
|     - | 1986 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 1987 | `"   if($filter !== null){"` |
|     - | 1988 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 1989 | `"    if($c['final']){ $m \|= 32; }"` |
|     - | 1990 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 1991 | `"   }"` |
|     - | 1992 | `"   $out[] = new ReflectionClassConstant($this->name, $k);"` |
|     - | 1993 | `"  }"` |
|     - | 1994 | `"  return $out;"` |
|     - | 1995 | `" }"` |
|     - | 1996 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 1997 | `"  $i = $this->__rinfo();"` |
|     - | 1998 | `"  return __reflect_build_attrs($i['attrs'], array('class', $this->name, null, 0), 1, $name, $flags);"` |
|     - | 1999 | `" }"` |
|     - | 2000 | `" public function getExtensionName(){ $i = $this->__rinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2001 | `" public function getExtension(){ $i = $this->__rinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2002 | `" public function newLazyGhost($initializer, $options = 0){"` |
|     - | 2003 | `"  throw new Error('ReflectionClass::newLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2004 | `" }"` |
|     - | 2005 | `" public function newLazyProxy($factory, $options = 0){"` |
|     - | 2006 | `"  throw new Error('ReflectionClass::newLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2007 | `" }"` |
|     - | 2008 | `" public function resetAsLazyGhost($object, $initializer, $options = 0){"` |
|     - | 2009 | `"  throw new Error('ReflectionClass::resetAsLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2010 | `" }"` |
|     - | 2011 | `" public function resetAsLazyProxy($object, $factory, $options = 0){"` |
|     - | 2012 | `"  throw new Error('ReflectionClass::resetAsLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2013 | `" }"` |
|     - | 2014 | `" public function getLazyInitializer($object){ return null; }"` |
|     - | 2015 | `" public function initializeLazyObject($object){ return $object; }"` |
|     - | 2016 | `" public function markLazyObjectAsInitialized($object){ return $object; }"` |
|     - | 2017 | `" public function isUninitializedLazyObject($object){ return false; }"` |
|     - | 2018 | `" public function __toString(){ return __reflect_export_class($this); }"` |
|     - | 2019 | `"}"` |
|     - | 2020 | `"class ReflectionObject extends ReflectionClass {"` |
|     - | 2021 | `" public function __construct($object){"` |
|     - | 2022 | `"  if(!is_object($object)){"` |
|     - | 2023 | `"   throw new TypeError('ReflectionObject::__construct(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 2024 | `"  }"` |
|     - | 2025 | `"  parent::__construct($object);"` |
|     - | 2026 | `"  $this->__obj = $object;"` |
|     - | 2027 | `" }"` |
|     - | 2028 | `"}"` |
|     - | 2029 | `;` |
|     - | 2030 | `/*` |
|     - | 2031 | ` * Chunk 2: ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,` |
|     - | 2032 | ` * ReflectionParameter.` |
|     - | 2033 | ` */` |
|     - | 2034 | `static const char zReflectLib2[] =` |
|     - | 2035 | `"abstract class ReflectionFunctionAbstract implements Reflector {"` |
|     - | 2036 | `" public $name;"` |
|     - | 2037 | `" protected $__cl = null;"` |
|     - | 2038 | `" protected function __rfinfo(){"` |
|     - | 2039 | `"  if($this->__cl !== null){ return __reflect_sig_fixup(__reflect_func_info($this->__cl)); }"` |
|     - | 2040 | `"  return __reflect_sig_fixup(__reflect_func_info($this->name));"` |
|     - | 2041 | `" }"` |
|     - | 2042 | `" protected function __rftarget(){ return $this->__cl !== null ? $this->__cl : $this->name; }"` |
|     - | 2043 | `" protected function __rpspec(){ return $this->__rftarget(); }"` |
|     - | 2044 | `" public function getName(){ return $this->name; }"` |
|     - | 2045 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 2046 | `" public function getNamespaceName(){"` |
|     - | 2047 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2048 | `"  if($p === false){ return ''; }"` |
|     - | 2049 | `"  return substr($this->name,0,$p);"` |
|     - | 2050 | `" }"` |
|     - | 2051 | `" public function getShortName(){"` |
|     - | 2052 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2053 | `"  if($p === false){ return $this->name; }"` |
|     - | 2054 | `"  return substr($this->name,$p+1);"` |
|     - | 2055 | `" }"` |
|     - | 2056 | `" public function isClosure(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2057 | `" public function isGenerator(){ $i = $this->__rfinfo(); return $i['generator']; }"` |
|     - | 2058 | `" public function isVariadic(){ $i = $this->__rfinfo(); return $i['variadic']; }"` |
|     - | 2059 | `" public function returnsReference(){ $i = $this->__rfinfo(); return $i['byref']; }"` |
|     - | 2060 | `" public function isInternal(){ $i = $this->__rfinfo(); return $i['internal']; }"` |
|     - | 2061 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 2062 | `" public function isDeprecated(){ $i = $this->__rfinfo(); return __reflect_has_deprecated($i['attrs']); }"` |
|     - | 2063 | `" public function isStatic(){ return false; }"` |
|     - | 2064 | `" public function getFileName(){ $i = $this->__rfinfo(); return $i['file']; }"` |
|     - | 2065 | `" public function getStartLine(){"` |
|     - | 2066 | `"  $i = $this->__rfinfo();"` |
|     - | 2067 | `"  if($i['internal']){ return false; }"` |
|     - | 2068 | `"  return $i['line'];"` |
|     - | 2069 | `" }"` |
|     - | 2070 | `" public function getEndLine(){"` |
|     - | 2071 | `"  $i = $this->__rfinfo();"` |
|     - | 2072 | `"  if($i['internal']){ return false; }"` |
|     - | 2073 | `"  return $i['endline'];"` |
|     - | 2074 | `" }"` |
|     - | 2075 | `" public function getDocComment(){ $i = $this->__rfinfo(); return $i['doc']; }"` |
|     - | 2076 | `" public function hasReturnType(){ $i = $this->__rfinfo(); return $i['rettext'] !== null; }"` |
|     - | 2077 | `" public function getReturnType(){ $i = $this->__rfinfo(); return __reflect_make_type($i['rettext']); }"` |
|     - | 2078 | `" public function hasTentativeReturnType(){ return false; }"` |
|     - | 2079 | `" public function getTentativeReturnType(){ return null; }"` |
|     - | 2080 | `" public function getNumberOfParameters(){"` |
|     - | 2081 | `"  $i = $this->__rfinfo();"` |
|     - | 2082 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2083 | `"  return count($i['params']);"` |
|     - | 2084 | `" }"` |
|     - | 2085 | `" public function getNumberOfRequiredParameters(){"` |
|     - | 2086 | `"  $i = $this->__rfinfo();"` |
|     - | 2087 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2088 | `"  $req = 0;"` |
|     - | 2089 | `"  $n = count($i['params']);"` |
|     - | 2090 | `"  for($k = $n - 1; $k >= 0; $k--){"` |
|     - | 2091 | `"   $p = $i['params'][$k];"` |
|     - | 2092 | `"   if(!$p['variadic'] && !$p['hasdef']){ $req = $k + 1; break; }"` |
|     - | 2093 | `"  }"` |
|     - | 2094 | `"  return $req;"` |
|     - | 2095 | `" }"` |
|     - | 2096 | `" public function getParameters(){"` |
|     - | 2097 | `"  $i = $this->__rfinfo();"` |
|     - | 2098 | `"  $out = array();"` |
|     - | 2099 | `"  $spec = $this->__rpspec();"` |
|     - | 2100 | `"  foreach($i['params'] as $p){"` |
|     - | 2101 | `"   $out[] = new ReflectionParameter($spec, $p['pos']);"` |
|     - | 2102 | `"  }"` |
|     - | 2103 | `"  return $out;"` |
|     - | 2104 | `" }"` |
|     - | 2105 | `" public function getStaticVariables(){ $i = $this->__rfinfo(); return $i['statics']; }"` |
|     - | 2106 | `" public function getClosureThis(){"` |
|     - | 2107 | `"  $i = $this->__rfinfo();"` |
|     - | 2108 | `"  return isset($i['this']) ? $i['this'] : null;"` |
|     - | 2109 | `" }"` |
|     - | 2110 | `" public function getClosureScopeClass(){"` |
|     - | 2111 | `"  $i = $this->__rfinfo();"` |
|     - | 2112 | `"  if(isset($i['scope'])){ return new ReflectionClass($i['scope']); }"` |
|     - | 2113 | `"  if(isset($i['this'])){ return new ReflectionClass(get_class($i['this'])); }"` |
|     - | 2114 | `"  return null;"` |
|     - | 2115 | `" }"` |
|     - | 2116 | `" public function getClosureCalledClass(){ return $this->getClosureScopeClass(); }"` |
|     - | 2117 | `" public function getClosureUsedVariables(){"` |
|     - | 2118 | `"  $i = $this->__rfinfo();"` |
|     - | 2119 | `"  return isset($i['used']) ? $i['used'] : array();"` |
|     - | 2120 | `" }"` |
|     - | 2121 | `" public function getExtensionName(){ $i = $this->__rfinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2122 | `" public function getExtension(){ $i = $this->__rfinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2123 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2124 | `"  $i = $this->__rfinfo();"` |
|     - | 2125 | `"  if($this instanceof ReflectionMethod){"` |
|     - | 2126 | `"   $spec = array('method', $this->class, $this->name, 0);"` |
|     - | 2127 | `"   $target = 4;"` |
|     - | 2128 | `"  }else{"` |
|     - | 2129 | `"   $spec = array('fn', $this->__rftarget(), null, 0);"` |
|     - | 2130 | `"   $target = 2;"` |
|     - | 2131 | `"  }"` |
|     - | 2132 | `"  return __reflect_build_attrs($i['attrs'], $spec, $target, $name, $flags);"` |
|     - | 2133 | `" }"` |
|     - | 2134 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2135 | `"}"` |
|     - | 2136 | `"class ReflectionFunction extends ReflectionFunctionAbstract {"` |
|     - | 2137 | `" const IS_DEPRECATED = 2048;"` |
|     - | 2138 | `" public function __construct($function){"` |
|     - | 2139 | `"  if($function instanceof Closure){"` |
|     - | 2140 | `"   $this->__cl = $function;"` |
|     - | 2141 | `"   $i = $this->__rfinfo();"` |
|     - | 2142 | `"   if($i['closure']){"` |
|     - | 2143 | `"    $f = $i['file'] === false ? '' : $i['file'];"` |
|     - | 2144 | `"    $this->name = '{closure:'.$f.':'.$i['line'].'}';"` |
|     - | 2145 | `"   }else{"` |
|     - | 2146 | `"    $this->name = $i['name'];"` |
|     - | 2147 | `"   }"` |
|     - | 2148 | `"   return;"` |
|     - | 2149 | `"  }"` |
|     - | 2150 | `"  if(!is_string($function)){"` |
|     - | 2151 | `"   throw new TypeError('ReflectionFunction::__construct(): Argument #1 ($function) must be of type Closure\|string, '.get_debug_type($function).' given');"` |
|     - | 2152 | `"  }"` |
|     - | 2153 | `"  $i = __reflect_func_info($function);"` |
|     - | 2154 | `"  if($i === null){"` |
|     - | 2155 | `"   throw new ReflectionException('Function '.$function.'() does not exist');"` |
|     - | 2156 | `"  }"` |
|     - | 2157 | `"  if($i['closure']){"` |
|     - | 2158 | `"   $this->name = '{closure:'.($i['file'] === false ? '' : $i['file']).':'.$i['line'].'}';"` |
|     - | 2159 | `"   $this->__cl = __reflect_closure($function, null, null);"` |
|     - | 2160 | `"  }else{"` |
|     - | 2161 | `"   $this->name = $i['name'];"` |
|     - | 2162 | `"  }"` |
|     - | 2163 | `" }"` |
|     - | 2164 | `" public function invoke(...$args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2165 | `" public function invokeArgs(array $args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2166 | `" public function getClosure(){"` |
|     - | 2167 | `"  if($this->__cl !== null){ return $this->__cl; }"` |
|     - | 2168 | `"  return __reflect_closure($this->name, null, null);"` |
|     - | 2169 | `" }"` |
|     - | 2170 | `" public function isAnonymous(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2171 | `" public function isDisabled(){ return false; }"` |
|     - | 2172 | `"}"` |
|     - | 2173 | `"class ReflectionMethod extends ReflectionFunctionAbstract {"` |
|     - | 2174 | `" const IS_PUBLIC = 1;"` |
|     - | 2175 | `" const IS_PROTECTED = 2;"` |
|     - | 2176 | `" const IS_PRIVATE = 4;"` |
|     - | 2177 | `" const IS_STATIC = 16;"` |
|     - | 2178 | `" const IS_FINAL = 32;"` |
|     - | 2179 | `" const IS_ABSTRACT = 64;"` |
|     - | 2180 | `" public $class;"` |
|     - | 2181 | `" public function __construct($objectOrMethod, $method = null){"` |
|     - | 2182 | `"  if($method === null){"` |
|     - | 2183 | `"   if(!is_string($objectOrMethod) \|\| strpos($objectOrMethod,'::') === false){"` |
|     - | 2184 | `"    throw new TypeError('ReflectionMethod::__construct(): Argument #1 ($objectOrMethod) must be of type object\|string, '.get_debug_type($objectOrMethod).' given');"` |
|     - | 2185 | `"   }"` |
|     - | 2186 | `"   $p = strpos($objectOrMethod,'::');"` |
|     - | 2187 | `"   $method = substr($objectOrMethod,$p+2);"` |
|     - | 2188 | `"   $objectOrMethod = substr($objectOrMethod,0,$p);"` |
|     - | 2189 | `"  }"` |
|     - | 2190 | `"  $ci = __reflect_class_info($objectOrMethod);"` |
|     - | 2191 | `"  if($ci === null){"` |
|     - | 2192 | `"   throw new ReflectionException('Class \"'.$objectOrMethod.'\" does not exist');"` |
|     - | 2193 | `"  }"` |
|     - | 2194 | `"  $this->class = $ci['name'];"` |
|     - | 2195 | `"  $found = null;"` |
|     - | 2196 | `"  if(isset($ci['methods'][$method])){"` |
|     - | 2197 | `"   $found = $method;"` |
|     - | 2198 | `"  }else{"` |
|     - | 2199 | `"   $l = strtolower($method);"` |
|     - | 2200 | `"   foreach($ci['methods'] as $k => $m){"` |
|     - | 2201 | `"    if(strtolower($k) === $l){ $found = $k; break; }"` |
|     - | 2202 | `"   }"` |
|     - | 2203 | `"  }"` |
|     - | 2204 | `"  if($found === null){"` |
|     - | 2205 | `"   throw new ReflectionException('Method '.$this->class.'::'.$method.'() does not exist');"` |
|     - | 2206 | `"  }"` |
|     - | 2207 | `"  $this->name = $found;"` |
|     - | 2208 | `" }"` |
|     - | 2209 | `" public static function createFromMethodName($name){"` |
|     - | 2210 | `"  return new ReflectionMethod($name);"` |
|     - | 2211 | `" }"` |
|     - | 2212 | `" protected function __rfinfo(){ return __reflect_func_info($this->class, $this->name); }"` |
|     - | 2213 | `" protected function __rpspec(){ return array($this->class, $this->name); }"` |
|     - | 2214 | `" public function getDeclaringClass(){"` |
|     - | 2215 | `"  $i = $this->__rfinfo();"` |
|     - | 2216 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2217 | `" }"` |
|     - | 2218 | `" public function getModifiers(){"` |
|     - | 2219 | `"  $i = $this->__rfinfo();"` |
|     - | 2220 | `"  $m = ($i['vis'] === 1 ? 1 : ($i['vis'] === 2 ? 2 : 4));"` |
|     - | 2221 | `"  if($i['mstatic']){ $m \|= 16; }"` |
|     - | 2222 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 2223 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 2224 | `"  return $m;"` |
|     - | 2225 | `" }"` |
|     - | 2226 | `" public function isPublic(){ $i = $this->__rfinfo(); return $i['vis'] === 1; }"` |
|     - | 2227 | `" public function isProtected(){ $i = $this->__rfinfo(); return $i['vis'] === 2; }"` |
|     - | 2228 | `" public function isPrivate(){ $i = $this->__rfinfo(); return $i['vis'] === 3; }"` |
|     - | 2229 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['mstatic']; }"` |
|     - | 2230 | `" public function isAbstract(){ $i = $this->__rfinfo(); return $i['abstract']; }"` |
|     - | 2231 | `" public function isFinal(){ $i = $this->__rfinfo(); return $i['final']; }"` |
|     - | 2232 | `" public function isConstructor(){ return strtolower($this->name) === '__construct'; }"` |
|     - | 2233 | `" public function isDestructor(){ return strtolower($this->name) === '__destruct'; }"` |
|     - | 2234 | `" public function invoke($object = null, ...$args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2235 | `" public function invokeArgs($object, array $args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2236 | `" protected function __rinvoke($object, $args){"` |
|     - | 2237 | `"  $i = $this->__rfinfo();"` |
|     - | 2238 | `"  if(!$i['mstatic']){"` |
|     - | 2239 | `"   if(!is_object($object)){"` |
|     - | 2240 | `"    throw new ReflectionException('Trying to invoke non static method '.$this->class.'::'.$this->name.'() without an object');"` |
|     - | 2241 | `"   }"` |
|     - | 2242 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2243 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2244 | `"   }"` |
|     - | 2245 | `"  }else{"` |
|     - | 2246 | `"   $object = null;"` |
|     - | 2247 | `"  }"` |
|     - | 2248 | `"  return __reflect_invoke($this->class, $this->name, $object, $args);"` |
|     - | 2249 | `" }"` |
|     - | 2250 | `" public function getClosure($object = null){"` |
|     - | 2251 | `"  $i = $this->__rfinfo();"` |
|     - | 2252 | `"  if(!$i['mstatic']){"` |
|     - | 2253 | `"   if($object === null){"` |
|     - | 2254 | `"    throw new ValueError('ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods');"` |
|     - | 2255 | `"   }"` |
|     - | 2256 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2257 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2258 | `"   }"` |
|     - | 2259 | `"  }else{"` |
|     - | 2260 | `"   $object = null;"` |
|     - | 2261 | `"  }"` |
|     - | 2262 | `"  return __reflect_closure($this->class, $this->name, $object);"` |
|     - | 2263 | `" }"` |
|     - | 2264 | `" public function setAccessible($accessible){ }"` |
|     - | 2265 | `" public function hasPrototype(){ return $this->__rproto() !== null; }"` |
|     - | 2266 | `" public function getPrototype(){"` |
|     - | 2267 | `"  $p = $this->__rproto();"` |
|     - | 2268 | `"  if($p === null){"` |
|     - | 2269 | `"   throw new ReflectionException('Method '.$this->class.'::'.$this->name.' does not have a prototype');"` |
|     - | 2270 | `"  }"` |
|     - | 2271 | `"  return new ReflectionMethod($p, $this->name);"` |
|     - | 2272 | `" }"` |
|     - | 2273 | `" protected function __rproto(){"` |
|     - | 2274 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2275 | `"  $l = strtolower($this->name);"` |
|     - | 2276 | `"  $p = $ci['parent'];"` |
|     - | 2277 | `"  while($p !== null){"` |
|     - | 2278 | `"   $pi = __reflect_class_info($p);"` |
|     - | 2279 | `"   foreach($pi['methods'] as $k => $m){"` |
|     - | 2280 | `"    if(strtolower($k) === $l && $m['vis'] !== 3){ return $m['decl']; }"` |
|     - | 2281 | `"   }"` |
|     - | 2282 | `"   $p = $pi['parent'];"` |
|     - | 2283 | `"  }"` |
|     - | 2284 | `"  foreach($ci['interfaces'] as $if){"` |
|     - | 2285 | `"   $ii = __reflect_class_info($if);"` |
|     - | 2286 | `"   foreach($ii['methods'] as $k => $m){"` |
|     - | 2287 | `"    if(strtolower($k) === $l){ return $ii['name']; }"` |
|     - | 2288 | `"   }"` |
|     - | 2289 | `"  }"` |
|     - | 2290 | `"  return null;"` |
|     - | 2291 | `" }"` |
|     - | 2292 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2293 | `"}"` |
|     - | 2294 | `"class ReflectionParameter implements Reflector {"` |
|     - | 2295 | `" public $name;"` |
|     - | 2296 | `" protected $__t;"` |
|     - | 2297 | `" protected $__m = null;"` |
|     - | 2298 | `" protected $__p = 0;"` |
|     - | 2299 | `" public function __construct($function, $param){"` |
|     - | 2300 | `"  $m = null;"` |
|     - | 2301 | `"  $t = $function;"` |
|     - | 2302 | `"  if(is_array($function)){"` |
|     - | 2303 | `"   $t = $function[0];"` |
|     - | 2304 | `"   $m = $function[1];"` |
|     - | 2305 | `"   if(is_object($t)){ $t = get_class($t); }"` |
|     - | 2306 | `"  }else if(is_string($function) && strpos($function,'::') !== false){"` |
|     - | 2307 | `"   $p = strpos($function,'::');"` |
|     - | 2308 | `"   $m = substr($function,$p+2);"` |
|     - | 2309 | `"   $t = substr($function,0,$p);"` |
|     - | 2310 | `"  }"` |
|     - | 2311 | `"  if($m !== null){"` |
|     - | 2312 | `"   $rm = new ReflectionMethod($t, $m);"` |
|     - | 2313 | `"   $t = $rm->class;"` |
|     - | 2314 | `"   $m = $rm->name;"` |
|     - | 2315 | `"   $i = __reflect_func_info($t, $m);"` |
|     - | 2316 | `"  }else if($function instanceof Closure){"` |
|     - | 2317 | `"   $t = $function;"` |
|     - | 2318 | `"   $i = __reflect_func_info($function);"` |
|     - | 2319 | `"  }else{"` |
|     - | 2320 | `"   $i = __reflect_sig_fixup(__reflect_func_info($t));"` |
|     - | 2321 | `"   if($i === null){"` |
|     - | 2322 | `"    throw new ReflectionException('Function '.$t.'() does not exist');"` |
|     - | 2323 | `"   }"` |
|     - | 2324 | `"  }"` |
|     - | 2325 | `"  $found = null;"` |
|     - | 2326 | `"  if(is_int($param)){"` |
|     - | 2327 | `"   if(isset($i['params'][$param])){ $found = $i['params'][$param]; }"` |
|     - | 2328 | `"   if($found === null){"` |
|     - | 2329 | `"    throw new ReflectionException('The parameter specified by its offset could not be found');"` |
|     - | 2330 | `"   }"` |
|     - | 2331 | `"  }else{"` |
|     - | 2332 | `"   foreach($i['params'] as $pp){"` |
|     - | 2333 | `"    if($pp['name'] === $param){ $found = $pp; break; }"` |
|     - | 2334 | `"   }"` |
|     - | 2335 | `"   if($found === null){"` |
|     - | 2336 | `"    throw new ReflectionException('The parameter specified by its name could not be found');"` |
|     - | 2337 | `"   }"` |
|     - | 2338 | `"  }"` |
|     - | 2339 | `"  $this->name = $found['name'];"` |
|     - | 2340 | `"  $this->__t = $t;"` |
|     - | 2341 | `"  $this->__m = $m;"` |
|     - | 2342 | `"  $this->__p = $found['pos'];"` |
|     - | 2343 | `" }"` |
|     - | 2344 | `" protected function __rffull(){"` |
|     - | 2345 | `"  if($this->__m !== null){ return __reflect_func_info($this->__t, $this->__m); }"` |
|     - | 2346 | `"  return __reflect_sig_fixup(__reflect_func_info($this->__t));"` |
|     - | 2347 | `" }"` |
|     - | 2348 | `" protected function __rpinfo(){"` |
|     - | 2349 | `"  $i = $this->__rffull();"` |
|     - | 2350 | `"  return $i['params'][$this->__p];"` |
|     - | 2351 | `" }"` |
|     - | 2352 | `" public function getName(){ return $this->name; }"` |
|     - | 2353 | `" public function getPosition(){ return $this->__p; }"` |
|     - | 2354 | `" public function isPassedByReference(){ $p = $this->__rpinfo(); return $p['byref']; }"` |
|     - | 2355 | `" public function canBePassedByValue(){ return !$this->isPassedByReference(); }"` |
|     - | 2356 | `" public function isVariadic(){ $p = $this->__rpinfo(); return $p['variadic']; }"` |
|     - | 2357 | `" public function isPromoted(){ $p = $this->__rpinfo(); return $p['promoted']; }"` |
|     - | 2358 | `" public function isDefaultValueAvailable(){ $p = $this->__rpinfo(); return $p['hasdef']; }"` |
|     - | 2359 | `" public function isOptional(){"` |
|     - | 2360 | `"  $i = $this->__rffull();"` |
|     - | 2361 | `"  $n = count($i['params']);"` |
|     - | 2362 | `"  for($k = $this->__p; $k < $n; $k++){"` |
|     - | 2363 | `"   $p = $i['params'][$k];"` |
|     - | 2364 | `"   if(!$p['variadic'] && !$p['hasdef']){ return false; }"` |
|     - | 2365 | `"  }"` |
|     - | 2366 | `"  return true;"` |
|     - | 2367 | `" }"` |
|     - | 2368 | `" public function getDefaultValue(){"` |
|     - | 2369 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2370 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2371 | `"  }"` |
|     - | 2372 | `"  $p = $this->__rpinfo();"` |
|     - | 2373 | `"  if(isset($p['deftext'])){"` |
|     - | 2374 | `"   $s = __reflect_sig_scalar($p['deftext']);"` |
|     - | 2375 | `"   if($s[0]){ return $s[1]; }"` |
|     - | 2376 | `"   if($p['deftext'] === 'array (' \|\| strpos($p['deftext'], '[') === 0){ return array(); }"` |
|     - | 2377 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2378 | `"  }"` |
|     - | 2379 | `"  return __reflect_param_default($this->__t, $this->__m, $this->__p);"` |
|     - | 2380 | `" }"` |
|     - | 2381 | `" public function isDefaultValueConstant(){"` |
|     - | 2382 | `"  if(!$this->isDefaultValueAvailable()){ return false; }"` |
|     - | 2383 | `"  $p = $this->__rpinfo();"` |
|     - | 2384 | `"  if(isset($p['deftext'])){ return false; }"` |
|     - | 2385 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p) !== null;"` |
|     - | 2386 | `" }"` |
|     - | 2387 | `" public function getDefaultValueConstantName(){"` |
|     - | 2388 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2389 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2390 | `"  }"` |
|     - | 2391 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p);"` |
|     - | 2392 | `" }"` |
|     - | 2393 | `" public function allowsNull(){"` |
|     - | 2394 | `"  $p = $this->__rpinfo();"` |
|     - | 2395 | `"  if($p['typetext'] === null){ return true; }"` |
|     - | 2396 | `"  if($p['nullable']){ return true; }"` |
|     - | 2397 | `"  return $p['typetext'] === 'mixed' \|\| $p['typetext'] === 'null';"` |
|     - | 2398 | `" }"` |
|     - | 2399 | `" public function hasType(){ $p = $this->__rpinfo(); return $p['typetext'] !== null; }"` |
|     - | 2400 | `" public function getType(){ $p = $this->__rpinfo(); return __reflect_make_type($p['typetext']); }"` |
|     - | 2401 | `" public function getDeclaringFunction(){"` |
|     - | 2402 | `"  if($this->__m !== null){ return new ReflectionMethod($this->__t, $this->__m); }"` |
|     - | 2403 | `"  return new ReflectionFunction($this->__t);"` |
|     - | 2404 | `" }"` |
|     - | 2405 | `" public function getDeclaringClass(){"` |
|     - | 2406 | `"  if($this->__m === null){ return null; }"` |
|     - | 2407 | `"  $i = $this->__rffull();"` |
|     - | 2408 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2409 | `" }"` |
|     - | 2410 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2411 | `"  $p = $this->__rpinfo();"` |
|     - | 2412 | `"  return __reflect_build_attrs($p['attrs'], array('param', $this->__t, $this->__m, $this->__p), 32, $name, $flags);"` |
|     - | 2413 | `" }"` |
|     - | 2414 | `" public function __toString(){ return __reflect_export_param($this); }"` |
|     - | 2415 | `"}"` |
|     - | 2416 | `;` |
|     - | 2417 | `/*` |
|     - | 2418 | ` * Chunk 3: ReflectionProperty, ReflectionClassConstant.` |
|     - | 2419 | ` */` |
|     - | 2420 | `static const char zReflectLib3[] =` |
|     - | 2421 | `"class ReflectionProperty implements Reflector {"` |
|     - | 2422 | `" const IS_PUBLIC = 1;"` |
|     - | 2423 | `" const IS_PROTECTED = 2;"` |
|     - | 2424 | `" const IS_PRIVATE = 4;"` |
|     - | 2425 | `" const IS_STATIC = 16;"` |
|     - | 2426 | `" const IS_FINAL = 32;"` |
|     - | 2427 | `" const IS_ABSTRACT = 64;"` |
|     - | 2428 | `" const IS_READONLY = 128;"` |
|     - | 2429 | `" const IS_VIRTUAL = 512;"` |
|     - | 2430 | `" const IS_PROTECTED_SET = 2048;"` |
|     - | 2431 | `" const IS_PRIVATE_SET = 4096;"` |
|     - | 2432 | `" public $name;"` |
|     - | 2433 | `" public $class;"` |
|     - | 2434 | `" protected $__dynobj = null;"` |
|     - | 2435 | `" public function __construct($class, $property){"` |
|     - | 2436 | `"  $obj = null;"` |
|     - | 2437 | `"  if(is_object($class)){ $obj = $class; }"` |
|     - | 2438 | `"  else if(!is_string($class)){"` |
|     - | 2439 | `"   throw new TypeError('ReflectionProperty::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2440 | `"  }"` |
|     - | 2441 | `"  $ci = __reflect_class_info($class);"` |
|     - | 2442 | `"  if($ci === null){"` |
|     - | 2443 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2444 | `"  }"` |
|     - | 2445 | `"  $this->class = $ci['name'];"` |
|     - | 2446 | `"  if(isset($ci['props'][$property])){"` |
|     - | 2447 | `"   $this->name = $property;"` |
|     - | 2448 | `"   return;"` |
|     - | 2449 | `"  }"` |
|     - | 2450 | `"  if($obj !== null && (__reflect_prop_state($obj, $property) & 1)){"` |
|     - | 2451 | `"   $this->name = $property;"` |
|     - | 2452 | `"   $this->__dynobj = $obj;"` |
|     - | 2453 | `"   return;"` |
|     - | 2454 | `"  }"` |
|     - | 2455 | `"  throw new ReflectionException('Property '.$this->class.'::$'.$property.' does not exist');"` |
|     - | 2456 | `" }"` |
|     - | 2457 | `" protected function __rpmeta(){"` |
|     - | 2458 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2459 | `"  if(isset($ci['props'][$this->name])){ return $ci['props'][$this->name]; }"` |
|     - | 2460 | `"  return array('vis' => 1, 'static' => false, 'readonly' => false, 'hasdef' => false,"` |
|     - | 2461 | `"   'typed' => false, 'typetext' => null, 'decl' => $this->class, 'line' => 0, 'dyn' => true);"` |
|     - | 2462 | `" }"` |
|     - | 2463 | `" public function getName(){ return $this->name; }"` |
|     - | 2464 | `" public function getDeclaringClass(){"` |
|     - | 2465 | `"  $m = $this->__rpmeta();"` |
|     - | 2466 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2467 | `" }"` |
|     - | 2468 | `" public function getModifiers(){"` |
|     - | 2469 | `"  $m = $this->__rpmeta();"` |
|     - | 2470 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2471 | `"  if($m['static']){ $mod \|= 16; }"` |
|     - | 2472 | `"  if($m['readonly']){ $mod \|= 128; }"` |
|     - | 2473 | `"  return $mod;"` |
|     - | 2474 | `" }"` |
|     - | 2475 | `" public function isPublic(){ $m = $this->__rpmeta(); return $m['vis'] === 1; }"` |
|     - | 2476 | `" public function isProtected(){ $m = $this->__rpmeta(); return $m['vis'] === 2; }"` |
|     - | 2477 | `" public function isPrivate(){ $m = $this->__rpmeta(); return $m['vis'] === 3; }"` |
|     - | 2478 | `" public function isStatic(){ $m = $this->__rpmeta(); return $m['static']; }"` |
|     - | 2479 | `" public function isReadOnly(){ $m = $this->__rpmeta(); return $m['readonly']; }"` |
|     - | 2480 | `" public function isDefault(){ $m = $this->__rpmeta(); return !isset($m['dyn']); }"` |
|     - | 2481 | `" public function isDynamic(){ $m = $this->__rpmeta(); return isset($m['dyn']); }"` |
|     - | 2482 | `" public function isAbstract(){ return false; }"` |
|     - | 2483 | `" public function isFinal(){ return false; }"` |
|     - | 2484 | `" public function isVirtual(){ return false; }"` |
|     - | 2485 | `" public function isPrivateSet(){ return false; }"` |
|     - | 2486 | `" public function isProtectedSet(){ return false; }"` |
|     - | 2487 | `" public function hasHooks(){ return false; }"` |
|     - | 2488 | `" public function getHooks(){ return array(); }"` |
|     - | 2489 | `" public function hasHook($type){ return false; }"` |
|     - | 2490 | `" public function getHook($type){ return null; }"` |
|     - | 2491 | `" public function isLazy($object){ return false; }"` |
|     - | 2492 | `" public function setAccessible($accessible){ }"` |
|     - | 2493 | `" public function getValue($object = null){"` |
|     - | 2494 | `"  $m = $this->__rpmeta();"` |
|     - | 2495 | `"  if($m['static']){ return __reflect_static_value($this->class, $this->name); }"` |
|     - | 2496 | `"  if(!is_object($object)){"` |
|     - | 2497 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2498 | `"  }"` |
|     - | 2499 | `"  return __reflect_prop_read($object, $this->name);"` |
|     - | 2500 | `" }"` |
|     - | 2501 | `" public function setValue($objectOrValue = null, $value = null){"` |
|     - | 2502 | `"  $m = $this->__rpmeta();"` |
|     - | 2503 | `"  if($m['static']){"` |
|     - | 2504 | `"   if($value === null && $objectOrValue !== null && !is_object($objectOrValue)){"` |
|     - | 2505 | `"    __reflect_static_set($this->class, $this->name, $objectOrValue);"` |
|     - | 2506 | `"   }else{"` |
|     - | 2507 | `"    __reflect_static_set($this->class, $this->name, $value);"` |
|     - | 2508 | `"   }"` |
|     - | 2509 | `"   return;"` |
|     - | 2510 | `"  }"` |
|     - | 2511 | `"  __reflect_prop_write($objectOrValue, $this->name, $value);"` |
|     - | 2512 | `" }"` |
|     - | 2513 | `" public function getRawValue($object){ return $this->getValue($object); }"` |
|     - | 2514 | `" public function setRawValue($object, $value){ $this->setValue($object, $value); }"` |
|     - | 2515 | `" public function isInitialized($object = null){"` |
|     - | 2516 | `"  $m = $this->__rpmeta();"` |
|     - | 2517 | `"  if($m['static']){ return (__reflect_prop_state($this->class, $this->name) & 2) !== 0; }"` |
|     - | 2518 | `"  if(!is_object($object)){"` |
|     - | 2519 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2520 | `"  }"` |
|     - | 2521 | `"  return (__reflect_prop_state($object, $this->name) & 2) !== 0;"` |
|     - | 2522 | `" }"` |
|     - | 2523 | `" public function hasDefaultValue(){"` |
|     - | 2524 | `"  $m = $this->__rpmeta();"` |
|     - | 2525 | `"  if(isset($m['dyn'])){ return false; }"` |
|     - | 2526 | `"  if($m['hasdef']){ return true; }"` |
|     - | 2527 | `"  return !$m['typed'];"` |
|     - | 2528 | `" }"` |
|     - | 2529 | `" public function getDefaultValue(){"` |
|     - | 2530 | `"  $m = $this->__rpmeta();"` |
|     - | 2531 | `"  if(isset($m['dyn']) \|\| !$m['hasdef']){ return null; }"` |
|     - | 2532 | `"  return __reflect_prop_default($this->class, $this->name);"` |
|     - | 2533 | `" }"` |
|     - | 2534 | `" public function hasType(){ $m = $this->__rpmeta(); return $m['typed']; }"` |
|     - | 2535 | `" public function getType(){ $m = $this->__rpmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2536 | `" public function getSettableType(){ return $this->getType(); }"` |
|     - | 2537 | `" public function setRawValueWithoutLazyInitialization($object, $value){"` |
|     - | 2538 | `"  throw new Error('ReflectionProperty::setRawValueWithoutLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2539 | `" }"` |
|     - | 2540 | `" public function skipLazyInitialization($object){"` |
|     - | 2541 | `"  throw new Error('ReflectionProperty::skipLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2542 | `" }"` |
|     - | 2543 | `" public function getDocComment(){ $m = $this->__rpmeta(); return isset($m['doc']) ? $m['doc'] : false; }"` |
|     - | 2544 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2545 | `"  $m = $this->__rpmeta();"` |
|     - | 2546 | `"  if(!isset($m['attrs'])){ return array(); }"` |
|     - | 2547 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 8, $name, $flags);"` |
|     - | 2548 | `" }"` |
|     - | 2549 | `" public function __toString(){ return __reflect_export_prop($this); }"` |
|     - | 2550 | `"}"` |
|     - | 2551 | `"class ReflectionClassConstant implements Reflector {"` |
|     - | 2552 | `" const IS_PUBLIC = 1;"` |
|     - | 2553 | `" const IS_PROTECTED = 2;"` |
|     - | 2554 | `" const IS_PRIVATE = 4;"` |
|     - | 2555 | `" const IS_FINAL = 32;"` |
|     - | 2556 | `" public $name;"` |
|     - | 2557 | `" public $class;"` |
|     - | 2558 | `" public function __construct($class, $constant){"` |
|     - | 2559 | `"  if(!is_object($class) && !is_string($class)){"` |
|     - | 2560 | `"   throw new TypeError('ReflectionClassConstant::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2561 | `"  }"` |
|     - | 2562 | `"  $ci = __reflect_class_info($class);"` |
|     - | 2563 | `"  if($ci === null){"` |
|     - | 2564 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2565 | `"  }"` |
|     - | 2566 | `"  $this->class = $ci['name'];"` |
|     - | 2567 | `"  if(!isset($ci['consts'][$constant])){"` |
|     - | 2568 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' does not exist');"` |
|     - | 2569 | `"  }"` |
|     - | 2570 | `"  $this->name = $constant;"` |
|     - | 2571 | `" }"` |
|     - | 2572 | `" protected function __rcmeta(){"` |
|     - | 2573 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2574 | `"  return $ci['consts'][$this->name];"` |
|     - | 2575 | `" }"` |
|     - | 2576 | `" public function getName(){ return $this->name; }"` |
|     - | 2577 | `" public function getValue(){ return __reflect_const_value($this->class, $this->name); }"` |
|     - | 2578 | `" public function getDeclaringClass(){"` |
|     - | 2579 | `"  $m = $this->__rcmeta();"` |
|     - | 2580 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2581 | `" }"` |
|     - | 2582 | `" public function getModifiers(){"` |
|     - | 2583 | `"  $m = $this->__rcmeta();"` |
|     - | 2584 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2585 | `"  if($m['final']){ $mod \|= 32; }"` |
|     - | 2586 | `"  return $mod;"` |
|     - | 2587 | `" }"` |
|     - | 2588 | `" public function isPublic(){ $m = $this->__rcmeta(); return $m['vis'] === 1; }"` |
|     - | 2589 | `" public function isProtected(){ $m = $this->__rcmeta(); return $m['vis'] === 2; }"` |
|     - | 2590 | `" public function isPrivate(){ $m = $this->__rcmeta(); return $m['vis'] === 3; }"` |
|     - | 2591 | `" public function isFinal(){ $m = $this->__rcmeta(); return $m['final']; }"` |
|     - | 2592 | `" public function isEnumCase(){ return false; }"` |
|     - | 2593 | `" public function isDeprecated(){ $m = $this->__rcmeta(); return __reflect_has_deprecated($m['attrs']); }"` |
|     - | 2594 | `" public function hasType(){ $m = $this->__rcmeta(); return $m['typed']; }"` |
|     - | 2595 | `" public function getType(){ $m = $this->__rcmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2596 | `" public function getDocComment(){ $m = $this->__rcmeta(); return $m['doc']; }"` |
|     - | 2597 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2598 | `"  $m = $this->__rcmeta();"` |
|     - | 2599 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 16, $name, $flags);"` |
|     - | 2600 | `" }"` |
|     - | 2601 | `" public function __toString(){ return __reflect_export_cconst($this); }"` |
|     - | 2602 | `"}"` |
|     - | 2603 | `;` |
|     - | 2604 | `/*` |
|     - | 2605 | ` * Chunk 4: the ReflectionType family, built from the engine's canonical` |
|     - | 2606 | ` * type text ("?int", "string\|float", "(A&B)\|C" — normalized at compile` |
|     - | 2607 | ` * time). __reflect_make_type is the internal factory; PHP itself never` |
|     - | 2608 | ` * lets user code construct these, so the public constructors here are a` |
|     - | 2609 | ` * recorded PHL-only surface.` |
|     - | 2610 | ` */` |
|     - | 2611 | `static const char zReflectLib4[] =` |
|     - | 2612 | `"abstract class ReflectionType implements Stringable {"` |
|     - | 2613 | `" protected $__text = '';"` |
|     - | 2614 | `" protected $__nullable = false;"` |
|     - | 2615 | `" public function allowsNull(){ return $this->__nullable; }"` |
|     - | 2616 | `" public function __toString(){ return $this->__text; }"` |
|     - | 2617 | `"}"` |
|     - | 2618 | `"class ReflectionNamedType extends ReflectionType {"` |
|     - | 2619 | `" protected $__tname = '';"` |
|     - | 2620 | `" public function __construct($name = '', $nullable = false, $text = null){"` |
|     - | 2621 | `"  $this->__tname = $name;"` |
|     - | 2622 | `"  $l = strtolower($name);"` |
|     - | 2623 | `"  $this->__nullable = $nullable \|\| $l === 'null' \|\| $l === 'mixed';"` |
|     - | 2624 | `"  $this->__text = $text === null ? $name : $text;"` |
|     - | 2625 | `" }"` |
|     - | 2626 | `" public function getName(){ return $this->__tname; }"` |
|     - | 2627 | `" public function isBuiltin(){"` |
|     - | 2628 | `"  $l = strtolower($this->__tname);"` |
|     - | 2629 | `"  return in_array($l, array('int','float','string','bool','array','object','mixed',"` |
|     - | 2630 | `"   'void','never','null','callable','iterable','true','false'), true);"` |
|     - | 2631 | `" }"` |
|     - | 2632 | `"}"` |
|     - | 2633 | `"class ReflectionUnionType extends ReflectionType {"` |
|     - | 2634 | `" protected $__types = array();"` |
|     - | 2635 | `" public function __construct($text = '', $nullable = false, $types = array()){"` |
|     - | 2636 | `"  $this->__text = $text;"` |
|     - | 2637 | `"  $this->__nullable = $nullable;"` |
|     - | 2638 | `"  $this->__types = $types;"` |
|     - | 2639 | `" }"` |
|     - | 2640 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2641 | `"}"` |
|     - | 2642 | `"class ReflectionIntersectionType extends ReflectionType {"` |
|     - | 2643 | `" protected $__types = array();"` |
|     - | 2644 | `" public function __construct($text = '', $types = array()){"` |
|     - | 2645 | `"  $this->__text = $text;"` |
|     - | 2646 | `"  $this->__nullable = false;"` |
|     - | 2647 | `"  $this->__types = $types;"` |
|     - | 2648 | `" }"` |
|     - | 2649 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2650 | `"}"` |
|     - | 2651 | `"function __reflect_make_atom($p){"` |
|     - | 2652 | `" $nullable = false;"` |
|     - | 2653 | `" if($p !== '' && $p[0] === '?'){ $nullable = true; $p = substr($p, 1); }"` |
|     - | 2654 | `" if($p !== '' && $p[0] === '('){ $p = substr($p, 1, strlen($p) - 2); }"` |
|     - | 2655 | `" if(strpos($p, '&') !== false){"` |
|     - | 2656 | `"  $subs = array();"` |
|     - | 2657 | `"  foreach(explode('&', $p) as $s){ $subs[] = new ReflectionNamedType($s, false, $s); }"` |
|     - | 2658 | `"  return new ReflectionIntersectionType($p, $subs);"` |
|     - | 2659 | `" }"` |
|     - | 2660 | `" return new ReflectionNamedType($p, $nullable, $nullable ? '?'.$p : $p);"` |
|     - | 2661 | `"}"` |
|     - | 2662 | `"function __reflect_make_type($text){"` |
|     - | 2663 | `" if($text === null \|\| $text === ''){ return null; }"` |
|     - | 2664 | `" $nullable = false;"` |
|     - | 2665 | `" $body = $text;"` |
|     - | 2666 | `" if($body[0] === '?'){ $nullable = true; $body = substr($body, 1); }"` |
|     - | 2667 | `" $parts = array();"` |
|     - | 2668 | `" $depth = 0;"` |
|     - | 2669 | `" $cur = '';"` |
|     - | 2670 | `" $n = strlen($body);"` |
|     - | 2671 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 2672 | `"  $ch = $body[$k];"` |
|     - | 2673 | `"  if($ch === '('){ $depth++; $cur .= $ch; }"` |
|     - | 2674 | `"  else if($ch === ')'){ $depth--; $cur .= $ch; }"` |
|     - | 2675 | `"  else if($ch === '\|' && $depth === 0){ $parts[] = $cur; $cur = ''; }"` |
|     - | 2676 | `"  else{ $cur .= $ch; }"` |
|     - | 2677 | `" }"` |
|     - | 2678 | `" $parts[] = $cur;"` |
|     - | 2679 | `" if(count($parts) > 1){"` |
|     - | 2680 | `"  $nonNull = array();"` |
|     - | 2681 | `"  $hasNull = false;"` |
|     - | 2682 | `"  foreach($parts as $p){"` |
|     - | 2683 | `"   if(strtolower($p) === 'null'){ $hasNull = true; }"` |
|     - | 2684 | `"   else{ $nonNull[] = $p; }"` |
|     - | 2685 | `"  }"` |
|     - | 2686 | `"  if($hasNull && count($nonNull) === 1 && strpos($nonNull[0], '&') === false){"` |
|     - | 2687 | `"   return new ReflectionNamedType($nonNull[0], true, '?'.$nonNull[0]);"` |
|     - | 2688 | `"  }"` |
|     - | 2689 | `"  $types = array();"` |
|     - | 2690 | `"  foreach($parts as $p){ $types[] = __reflect_make_atom($p); }"` |
|     - | 2691 | `"  return new ReflectionUnionType($body, $nullable \|\| $hasNull, $types);"` |
|     - | 2692 | `" }"` |
|     - | 2693 | `" if(strpos($body, '&') !== false){ return __reflect_make_atom($body); }"` |
|     - | 2694 | `" return __reflect_make_atom($nullable ? '?'.$body : $body);"` |
|     - | 2695 | `"}"` |
|     - | 2696 | `;` |
|     - | 2697 | `/*` |
|     - | 2698 | ` * Chunk 5: ReflectionGenerator, ReflectionFiber. Executing line/file and` |
|     - | 2699 | ` * traces need runtime line tracking the VM does not have (same gap as` |
|     - | 2700 | ` * debug_backtrace's line numbers) — those throw a loud Error, recorded in` |
|     - | 2701 | ` * the plan ledger.` |
|     - | 2702 | ` */` |
|     - | 2703 | `static const char zReflectLib5[] =` |
|     - | 2704 | `"class ReflectionGenerator {"` |
|     - | 2705 | `" protected $__gen;"` |
|     - | 2706 | `" public function __construct($generator){"` |
|     - | 2707 | `"  if(!($generator instanceof Generator)){"` |
|     - | 2708 | `"   throw new TypeError('ReflectionGenerator::__construct(): Argument #1 ($generator) must be of type Generator, '.get_debug_type($generator).' given');"` |
|     - | 2709 | `"  }"` |
|     - | 2710 | `"  $this->__gen = $generator;"` |
|     - | 2711 | `" }"` |
|     - | 2712 | `" protected function __rginfo(){ return __reflect_gen_info($this->__gen); }"` |
|     - | 2713 | `" public function getFunction(){"` |
|     - | 2714 | `"  $i = $this->__rginfo();"` |
|     - | 2715 | `"  if($i['kind'] === 'method'){ return new ReflectionMethod($i['class'], $i['name']); }"` |
|     - | 2716 | `"  return new ReflectionFunction($i['name']);"` |
|     - | 2717 | `" }"` |
|     - | 2718 | `" public function getThis(){ $i = $this->__rginfo(); return isset($i['this']) ? $i['this'] : null; }"` |
|     - | 2719 | `" public function getExecutingGenerator(){ return __reflect_gen_exec($this->__gen); }"` |
|     - | 2720 | `" public function isClosed(){ $i = $this->__rginfo(); return $i['closed']; }"` |
|     - | 2721 | `" public function getExecutingLine(){"` |
|     - | 2722 | `"  throw new Error('ReflectionGenerator::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2723 | `" }"` |
|     - | 2724 | `" public function getExecutingFile(){"` |
|     - | 2725 | `"  throw new Error('ReflectionGenerator::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2726 | `" }"` |
|     - | 2727 | `" public function getTrace($options = 1){"` |
|     - | 2728 | `"  throw new Error('ReflectionGenerator::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2729 | `" }"` |
|     - | 2730 | `"}"` |
|     - | 2731 | `"class ReflectionFiber {"` |
|     - | 2732 | `" protected $__fiber;"` |
|     - | 2733 | `" public function __construct($fiber){"` |
|     - | 2734 | `"  if(!($fiber instanceof Fiber)){"` |
|     - | 2735 | `"   throw new TypeError('ReflectionFiber::__construct(): Argument #1 ($fiber) must be of type Fiber, '.get_debug_type($fiber).' given');"` |
|     - | 2736 | `"  }"` |
|     - | 2737 | `"  $this->__fiber = $fiber;"` |
|     - | 2738 | `" }"` |
|     - | 2739 | `" public function getFiber(){ return $this->__fiber; }"` |
|     - | 2740 | `" public function getCallable(){ return __reflect_prop_read($this->__fiber, '__callable'); }"` |
|     - | 2741 | `" public function getExecutingLine(){"` |
|     - | 2742 | `"  throw new Error('ReflectionFiber::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2743 | `" }"` |
|     - | 2744 | `" public function getExecutingFile(){"` |
|     - | 2745 | `"  throw new Error('ReflectionFiber::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2746 | `" }"` |
|     - | 2747 | `" public function getTrace($options = 1){"` |
|     - | 2748 | `"  throw new Error('ReflectionFiber::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2749 | `" }"` |
|     - | 2750 | `"}"` |
|     - | 2751 | `;` |
|     - | 2752 | `/*` |
|     - | 2753 | ` * Chunk 6: the long tail — ReflectionConstant (PHP 8.5), the synthetic` |
|     - | 2754 | ` * "Core" ReflectionExtension, ReflectionZendExtension (throws: no Zend` |
|     - | 2755 | ` * extensions exist), the ReflectionEnum family (throws: enums are not a` |
|     - | 2756 | ` * PHL language feature yet), and ReflectionReference.` |
|     - | 2757 | ` */` |
|     - | 2758 | `static const char zReflectLib6[] =` |
|     - | 2759 | `"class ReflectionConstant implements Reflector {"` |
|     - | 2760 | `" public $name;"` |
|     - | 2761 | `" public function __construct($name){"` |
|     - | 2762 | `"  if(!is_string($name)){"` |
|     - | 2763 | `"   throw new TypeError('ReflectionConstant::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2764 | `"  }"` |
|     - | 2765 | `"  $i = __reflect_const_info($name);"` |
|     - | 2766 | `"  if($i === null){"` |
|     - | 2767 | `"   throw new ReflectionException('Constant \"'.$name.'\" does not exist');"` |
|     - | 2768 | `"  }"` |
|     - | 2769 | `"  $this->name = $name;"` |
|     - | 2770 | `" }"` |
|     - | 2771 | `" public function getName(){ return $this->name; }"` |
|     - | 2772 | `" public function getNamespaceName(){"` |
|     - | 2773 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2774 | `"  if($p === false){ return ''; }"` |
|     - | 2775 | `"  return substr($this->name,0,$p);"` |
|     - | 2776 | `" }"` |
|     - | 2777 | `" public function getShortName(){"` |
|     - | 2778 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2779 | `"  if($p === false){ return $this->name; }"` |
|     - | 2780 | `"  return substr($this->name,$p+1);"` |
|     - | 2781 | `" }"` |
|     - | 2782 | `" public function getValue(){"` |
|     - | 2783 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2784 | `"  return $i['value'];"` |
|     - | 2785 | `" }"` |
|     - | 2786 | `" public function isDeprecated(){ return false; }"` |
|     - | 2787 | `" public function getFileName(){"` |
|     - | 2788 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2789 | `"  return $i['file'];"` |
|     - | 2790 | `" }"` |
|     - | 2791 | `" public function getExtension(){"` |
|     - | 2792 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2793 | `"  return $i['internal'] ? new ReflectionExtension('Core') : null;"` |
|     - | 2794 | `" }"` |
|     - | 2795 | `" public function getExtensionName(){"` |
|     - | 2796 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2797 | `"  return $i['internal'] ? 'Core' : false;"` |
|     - | 2798 | `" }"` |
|     - | 2799 | `" public function getAttributes($name = null, $flags = 0){ return array(); }"` |
|     - | 2800 | `" public function __toString(){"` |
|     - | 2801 | `"  return 'Constant [ '.$this->name.' ]'.\"\\n\";"` |
|     - | 2802 | `" }"` |
|     - | 2803 | `"}"` |
|     - | 2804 | `"class ReflectionExtension implements Reflector {"` |
|     - | 2805 | `" public $name;"` |
|     - | 2806 | `" public function __construct($name){"` |
|     - | 2807 | `"  if(!is_string($name)){"` |
|     - | 2808 | `"   throw new TypeError('ReflectionExtension::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2809 | `"  }"` |
|     - | 2810 | `"  if(strtolower($name) !== 'core'){"` |
|     - | 2811 | `"   throw new ReflectionException('Extension \"'.$name.'\" does not exist');"` |
|     - | 2812 | `"  }"` |
|     - | 2813 | `"  $this->name = 'Core';"` |
|     - | 2814 | `" }"` |
|     - | 2815 | `" public function getName(){ return $this->name; }"` |
|     - | 2816 | `" public function getVersion(){ return phpversion(); }"` |
|     - | 2817 | `" public function getFunctions(){ return array(); }"` |
|     - | 2818 | `" public function getClasses(){ return array(); }"` |
|     - | 2819 | `" public function getClassNames(){ return array(); }"` |
|     - | 2820 | `" public function getConstants(){ return array(); }"` |
|     - | 2821 | `" public function getINIEntries(){ return array(); }"` |
|     - | 2822 | `" public function getDependencies(){ return array(); }"` |
|     - | 2823 | `" public function isPersistent(){ return true; }"` |
|     - | 2824 | `" public function isTemporary(){ return false; }"` |
|     - | 2825 | `" public function info(){ }"` |
|     - | 2826 | `" public function __toString(){"` |
|     - | 2827 | `"  return 'Extension [ extension #1 '.$this->name.' ]'.\"\\n\";"` |
|     - | 2828 | `" }"` |
|     - | 2829 | `"}"` |
|     - | 2830 | `"class ReflectionZendExtension implements Reflector {"` |
|     - | 2831 | `" public $name;"` |
|     - | 2832 | `" public function __construct($name){"` |
|     - | 2833 | `"  throw new ReflectionException('Zend Extension \"'.$name.'\" does not exist');"` |
|     - | 2834 | `" }"` |
|     - | 2835 | `" public function getName(){ return $this->name; }"` |
|     - | 2836 | `" public function __toString(){ return ''; }"` |
|     - | 2837 | `"}"` |
|     - | 2838 | `"class ReflectionEnum extends ReflectionClass {"` |
|     - | 2839 | `" public function __construct($objectOrClass){"` |
|     - | 2840 | `"  $info = __reflect_class_info($objectOrClass);"` |
|     - | 2841 | `"  if($info === null){"` |
|     - | 2842 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 2843 | `"  }"` |
|     - | 2844 | `"  throw new ReflectionException('Class \"'.$info['name'].'\" is not an enum');"` |
|     - | 2845 | `" }"` |
|     - | 2846 | `" public function hasCase($name){ return false; }"` |
|     - | 2847 | `" public function getCase($name){ throw new ReflectionException('Case '.$name.' does not exist'); }"` |
|     - | 2848 | `" public function getCases(){ return array(); }"` |
|     - | 2849 | `" public function isBacked(){ return false; }"` |
|     - | 2850 | `" public function getBackingType(){ return null; }"` |
|     - | 2851 | `"}"` |
|     - | 2852 | `"class ReflectionEnumUnitCase extends ReflectionClassConstant {"` |
|     - | 2853 | `" public function __construct($class, $constant){"` |
|     - | 2854 | `"  parent::__construct($class, $constant);"` |
|     - | 2855 | `"  throw new ReflectionException('Class \"'.$this->class.'\" is not an enum');"` |
|     - | 2856 | `" }"` |
|     - | 2857 | `" public function getEnum(){ return null; }"` |
|     - | 2858 | `"}"` |
|     - | 2859 | `"class ReflectionEnumBackedCase extends ReflectionEnumUnitCase {"` |
|     - | 2860 | `" public function getBackingValue(){ return null; }"` |
|     - | 2861 | `"}"` |
|     - | 2862 | `"final class ReflectionReference {"` |
|     - | 2863 | `" protected $__id = '';"` |
|     - | 2864 | `" public function __construct(){"` |
|     - | 2865 | `"  throw new Error('Call to private ReflectionReference::__construct() from global scope');"` |
|     - | 2866 | `" }"` |
|     - | 2867 | `" public static function fromArrayElement($array, $key){"` |
|     - | 2868 | `"  if(!is_array($array)){"` |
|     - | 2869 | `"   throw new TypeError('ReflectionReference::fromArrayElement(): Argument #1 ($array) must be of type array, '.get_debug_type($array).' given');"` |
|     - | 2870 | `"  }"` |
|     - | 2871 | `"  $id = __reflect_ref_id($array, $key);"` |
|     - | 2872 | `"  if($id === null){ return null; }"` |
|     - | 2873 | `"  $r = __reflect_new_no_ctor('ReflectionReference');"` |
|     - | 2874 | `"  $r->__setId('phlref'.$id);"` |
|     - | 2875 | `"  return $r;"` |
|     - | 2876 | `" }"` |
|     - | 2877 | `" public function __setId($id){ $this->__id = $id; }"` |
|     - | 2878 | `" public function getId(){ return $this->__id; }"` |
|     - | 2879 | `"}"` |
|     - | 2880 | `;` |
|     - | 2881 | `/*` |
|     - | 2882 | ` * Chunk 7: ReflectionAttribute and the shared getAttributes() builder.` |
|     - | 2883 | ` * The spec array rides as [kind, target, member, paramIdx]; argument` |
|     - | 2884 | ` * values evaluate lazily through __reflect_attr_args (PHP semantics).` |
|     - | 2885 | ` */` |
|     - | 2886 | `static const char zReflectLib7[] =` |
|     - | 2887 | `"function __reflect_has_deprecated($meta){"` |
|     - | 2888 | `" foreach($meta as $a){"` |
|     - | 2889 | `"  if(strtolower($a['name']) === 'deprecated'){ return true; }"` |
|     - | 2890 | `" }"` |
|     - | 2891 | `" return false;"` |
|     - | 2892 | `"}"` |
|     - | 2893 | `"function __reflect_target_names($mask){"` |
|     - | 2894 | `" $parts = array();"` |
|     - | 2895 | `" foreach(array('class' => 1, 'function' => 2, 'method' => 4, 'property' => 8,"` |
|     - | 2896 | `"  'class constant' => 16, 'parameter' => 32, 'constant' => 64) as $nm => $bit){"` |
|     - | 2897 | `"  if($mask & $bit){ $parts[] = $nm; }"` |
|     - | 2898 | `" }"` |
|     - | 2899 | `" return implode(', ', $parts);"` |
|     - | 2900 | `"}"` |
|     - | 2901 | `"function __reflect_build_attrs($meta, $spec, $target, $name, $flags){"` |
|     - | 2902 | `" $out = array();"` |
|     - | 2903 | `" $counts = array();"` |
|     - | 2904 | `" foreach($meta as $a){"` |
|     - | 2905 | `"  $k = strtolower($a['name']);"` |
|     - | 2906 | `"  $counts[$k] = isset($counts[$k]) ? $counts[$k] + 1 : 1;"` |
|     - | 2907 | `" }"` |
|     - | 2908 | `" $idx = 0;"` |
|     - | 2909 | `" foreach($meta as $a){"` |
|     - | 2910 | `"  $keep = true;"` |
|     - | 2911 | `"  if($name !== null){"` |
|     - | 2912 | `"   $keep = strtolower($a['name']) === strtolower($name);"` |
|     - | 2913 | `"   if(!$keep && ($flags & 2)){"` |
|     - | 2914 | `"    $keep = is_subclass_of($a['name'], $name);"` |
|     - | 2915 | `"   }"` |
|     - | 2916 | `"  }"` |
|     - | 2917 | `"  if($keep){"` |
|     - | 2918 | `"   $r = __reflect_new_no_ctor('ReflectionAttribute');"` |
|     - | 2919 | `"   $r->__init($a['name'], $spec, $idx, $target, $counts[strtolower($a['name'])] > 1);"` |
|     - | 2920 | `"   $out[] = $r;"` |
|     - | 2921 | `"  }"` |
|     - | 2922 | `"  $idx++;"` |
|     - | 2923 | `" }"` |
|     - | 2924 | `" return $out;"` |
|     - | 2925 | `"}"` |
|     - | 2926 | `"final class ReflectionAttribute {"` |
|     - | 2927 | `" const IS_INSTANCEOF = 2;"` |
|     - | 2928 | `" protected $__name = '';"` |
|     - | 2929 | `" protected $__spec = null;"` |
|     - | 2930 | `" protected $__idx = 0;"` |
|     - | 2931 | `" protected $__target = 0;"` |
|     - | 2932 | `" protected $__rep = false;"` |
|     - | 2933 | `" public function __construct(){"` |
|     - | 2934 | `"  throw new Error('Call to private ReflectionAttribute::__construct() from global scope');"` |
|     - | 2935 | `" }"` |
|     - | 2936 | `" public function __init($name, $spec, $idx, $target, $rep){"` |
|     - | 2937 | `"  $this->__name = $name;"` |
|     - | 2938 | `"  $this->__spec = $spec;"` |
|     - | 2939 | `"  $this->__idx = $idx;"` |
|     - | 2940 | `"  $this->__target = $target;"` |
|     - | 2941 | `"  $this->__rep = $rep;"` |
|     - | 2942 | `" }"` |
|     - | 2943 | `" public function getName(){ return $this->__name; }"` |
|     - | 2944 | `" public function getTarget(){ return $this->__target; }"` |
|     - | 2945 | `" public function isRepeated(){ return $this->__rep; }"` |
|     - | 2946 | `" public function getArguments(){"` |
|     - | 2947 | `"  $a = __reflect_attr_args($this->__spec[0], $this->__spec[1], $this->__spec[2], $this->__spec[3], $this->__idx);"` |
|     - | 2948 | `"  return $a === null ? array() : $a;"` |
|     - | 2949 | `" }"` |
|     - | 2950 | `" public function newInstance(){"` |
|     - | 2951 | `"  $name = $this->__name;"` |
|     - | 2952 | `"  $ci = __reflect_class_info($name);"` |
|     - | 2953 | `"  if($ci === null){"` |
|     - | 2954 | `"   throw new Error('Attribute class \"'.$name.'\" not found');"` |
|     - | 2955 | `"  }"` |
|     - | 2956 | `"  $name = $ci['name'];"` |
|     - | 2957 | `"  $decl = null;"` |
|     - | 2958 | `"  $didx = 0;"` |
|     - | 2959 | `"  foreach($ci['attrs'] as $a){"` |
|     - | 2960 | `"   if(strtolower($a['name']) === 'attribute'){ $decl = $didx; break; }"` |
|     - | 2961 | `"   $didx++;"` |
|     - | 2962 | `"  }"` |
|     - | 2963 | `"  if($decl === null){"` |
|     - | 2964 | `"   throw new Error('Attempting to use non-attribute class \"'.$name.'\" as attribute');"` |
|     - | 2965 | `"  }"` |
|     - | 2966 | `"  $dargs = __reflect_attr_args('class', $name, null, 0, $decl);"` |
|     - | 2967 | `"  $flags = 127;"` |
|     - | 2968 | `"  if(is_array($dargs)){"` |
|     - | 2969 | `"   if(isset($dargs[0])){ $flags = $dargs[0]; }"` |
|     - | 2970 | `"   else if(isset($dargs['flags'])){ $flags = $dargs['flags']; }"` |
|     - | 2971 | `"  }"` |
|     - | 2972 | `"  if(($flags & $this->__target) === 0){"` |
|     - | 2973 | `"   $tnames = array(1 => 'class', 2 => 'function', 4 => 'method', 8 => 'property',"` |
|     - | 2974 | `"    16 => 'class constant', 32 => 'parameter', 64 => 'constant');"` |
|     - | 2975 | `"   throw new Error('Attribute \"'.$name.'\" cannot target '.$tnames[$this->__target]"` |
|     - | 2976 | `"    .' (allowed targets: '.__reflect_target_names($flags).')');"` |
|     - | 2977 | `"  }"` |
|     - | 2978 | `"  if($this->__rep && ($flags & 128) === 0){"` |
|     - | 2979 | `"   throw new Error('Attribute \"'.$name.'\" must not be repeated');"` |
|     - | 2980 | `"  }"` |
|     - | 2981 | `"  return __reflect_new_instance($name, $this->getArguments());"` |
|     - | 2982 | `" }"` |
|     - | 2983 | `" public function __toString(){"` |
|     - | 2984 | `"  return 'Attribute [ '.$this->__name.' ]';"` |
|     - | 2985 | `" }"` |
|     - | 2986 | `"}"` |
|     - | 2987 | `;` |
|     - | 2988 | `/*` |
|     - | 2989 | ` * Chunk 8: signature-table support. Internal (C builtin) functions carry a` |
|     - | 2990 | ` * PHP-style parameter-list string; these helpers parse it into the same` |
|     - | 2991 | ` * param-meta shape user functions get, so ReflectionFunction and` |
|     - | 2992 | ` * ReflectionParameter work uniformly over builtins.` |
|     - | 2993 | ` */` |
|     - | 2994 | `static const char zReflectLib8[] =` |
|     - | 2995 | `"function __reflect_sig_split($sig){"` |
|     - | 2996 | `" $parts = array();"` |
|     - | 2997 | `" $cur = '';"` |
|     - | 2998 | `" $q = false;"` |
|     - | 2999 | `" $n = strlen($sig);"` |
|     - | 3000 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3001 | `"  $ch = $sig[$k];"` |
|     - | 3002 | `"  if($q){"` |
|     - | 3003 | `"   $cur .= $ch;"` |
|     - | 3004 | `"   if($ch === chr(92) && $k + 1 < $n){ $cur .= $sig[$k+1]; $k++; }"` |
|     - | 3005 | `"   else if($ch === chr(39)){ $q = false; }"` |
|     - | 3006 | `"  }else if($ch === chr(39)){ $q = true; $cur .= $ch; }"` |
|     - | 3007 | `"  else if($ch === ',' ){ $parts[] = trim($cur); $cur = ''; }"` |
|     - | 3008 | `"  else{ $cur .= $ch; }"` |
|     - | 3009 | `" }"` |
|     - | 3010 | `" if(trim($cur) !== ''){ $parts[] = trim($cur); }"` |
|     - | 3011 | `" return $parts;"` |
|     - | 3012 | `"}"` |
|     - | 3013 | `"function __reflect_sig_scalar($t){"` |
|     - | 3014 | `" if($t === '?'){ return array(false, null); }"` |
|     - | 3015 | `" if($t === 'NULL' \|\| $t === 'null'){ return array(true, null); }"` |
|     - | 3016 | `" if($t === 'true'){ return array(true, true); }"` |
|     - | 3017 | `" if($t === 'false'){ return array(true, false); }"` |
|     - | 3018 | `" if(is_numeric($t)){"` |
|     - | 3019 | `"  if(strpos($t, '.') === false && stripos($t, 'e') === false && strpos($t, 'x') === false){"` |
|     - | 3020 | `"   return array(true, (int)$t);"` |
|     - | 3021 | `"  }"` |
|     - | 3022 | `"  return array(true, (float)$t);"` |
|     - | 3023 | `" }"` |
|     - | 3024 | `" if(strlen($t) >= 2 && $t[0] === chr(39) && $t[strlen($t)-1] === chr(39)){"` |
|     - | 3025 | `"  $body = substr($t, 1, strlen($t) - 2);"` |
|     - | 3026 | `"  return array(true, strtr($body, array(chr(92).chr(39) => chr(39), chr(92).chr(92) => chr(92))));"` |
|     - | 3027 | `" }"` |
|     - | 3028 | `" return array(false, null);"` |
|     - | 3029 | `"}"` |
|     - | 3030 | `"function __reflect_parse_sig($sig){"` |
|     - | 3031 | `" $params = array();"` |
|     - | 3032 | `" $pos = 0;"` |
|     - | 3033 | `" foreach(__reflect_sig_split($sig) as $part){"` |
|     - | 3034 | `"  $deftext = null;"` |
|     - | 3035 | `"  $q = false;"` |
|     - | 3036 | `"  $n = strlen($part);"` |
|     - | 3037 | `"  for($k = 0; $k < $n; $k++){"` |
|     - | 3038 | `"   $ch = $part[$k];"` |
|     - | 3039 | `"   if($q){"` |
|     - | 3040 | `"    if($ch === chr(92)){ $k++; }"` |
|     - | 3041 | `"    else if($ch === chr(39)){ $q = false; }"` |
|     - | 3042 | `"   }else if($ch === chr(39)){ $q = true; }"` |
|     - | 3043 | `"   else if($ch === '=' ){"` |
|     - | 3044 | `"    $deftext = trim(substr($part, $k + 1));"` |
|     - | 3045 | `"    $part = trim(substr($part, 0, $k));"` |
|     - | 3046 | `"    break;"` |
|     - | 3047 | `"   }"` |
|     - | 3048 | `"  }"` |
|     - | 3049 | `"  $variadic = strpos($part, '...') !== false;"` |
|     - | 3050 | `"  $byref = strpos($part, '&') !== false;"` |
|     - | 3051 | `"  $d = strpos($part, '$');"` |
|     - | 3052 | `"  $name = $d === false ? $part : substr($part, $d + 1);"` |
|     - | 3053 | `"  $typetext = null;"` |
|     - | 3054 | `"  $sp = strpos($part, ' ');"` |
|     - | 3055 | `"  if($sp !== false && $d !== false && $sp < $d){ $typetext = substr($part, 0, $sp); }"` |
|     - | 3056 | `"  $nullable = $typetext !== null && ($typetext[0] === '?' \|\| stripos($typetext, 'null') !== false);"` |
|     - | 3057 | `"  $params[] = array('name' => $name, 'pos' => $pos, 'byref' => $byref,"` |
|     - | 3058 | `"   'variadic' => $variadic, 'hasdef' => $deftext !== null, 'nullable' => $nullable,"` |
|     - | 3059 | `"   'promoted' => false, 'typetext' => $typetext, 'attrs' => array(), 'deftext' => $deftext);"` |
|     - | 3060 | `"  $pos++;"` |
|     - | 3061 | `" }"` |
|     - | 3062 | `" return $params;"` |
|     - | 3063 | `"}"` |
|     - | 3064 | `"function __reflect_sig_fixup($i){"` |
|     - | 3065 | `" if($i === null){ return $i; }"` |
|     - | 3066 | `" if(isset($i['ret2'])){ $i['rettext'] = $i['ret2']; }"` |
|     - | 3067 | `" if(!isset($i['sig']) \|\| $i['sig'] === ''){ return $i; }"` |
|     - | 3068 | `" $i['params'] = __reflect_parse_sig($i['sig']);"` |
|     - | 3069 | `" $i['minarg'] = -1;"` |
|     - | 3070 | `" $v = false;"` |
|     - | 3071 | `" foreach($i['params'] as $p){ if($p['variadic']){ $v = true; } }"` |
|     - | 3072 | `" $i['variadic'] = $v;"` |
|     - | 3073 | `" return $i;"` |
|     - | 3074 | `"}"` |
|     - | 3075 | `;` |
|     - | 3076 | `/*` |
|     - | 3077 | ` * Chunk 9: PHP's Reflection export format (__toString on every Reflector).` |
|     - | 3078 | ` * Built entirely from the public reflection API of the target objects.` |
|     - | 3079 | ` */` |
|     - | 3080 | `static const char zReflectLib9[] =` |
|     - | 3081 | `"function __reflect_export_value($v){"` |
|     - | 3082 | `" if($v === null){ return 'NULL'; }"` |
|     - | 3083 | `" if($v === true){ return 'true'; }"` |
|     - | 3084 | `" if($v === false){ return 'false'; }"` |
|     - | 3085 | `" if(is_string($v)){ return chr(39).$v.chr(39); }"` |
|     - | 3086 | `" if(is_array($v)){"` |
|     - | 3087 | `"  $parts = array();"` |
|     - | 3088 | `"  $isList = true;"` |
|     - | 3089 | `"  $next = 0;"` |
|     - | 3090 | `"  foreach($v as $k => $x){"` |
|     - | 3091 | `"   if($k !== $next){ $isList = false; break; }"` |
|     - | 3092 | `"   $next++;"` |
|     - | 3093 | `"  }"` |
|     - | 3094 | `"  foreach($v as $k => $x){"` |
|     - | 3095 | `"   $parts[] = $isList ? __reflect_export_value($x)"` |
|     - | 3096 | `"    : (__reflect_export_value($k).' => '.__reflect_export_value($x));"` |
|     - | 3097 | `"  }"` |
|     - | 3098 | `"  return '['.implode(', ', $parts).']';"` |
|     - | 3099 | `" }"` |
|     - | 3100 | `" return (string)$v;"` |
|     - | 3101 | `"}"` |
|     - | 3102 | `"function __reflect_export_param($p){"` |
|     - | 3103 | `" $s = 'Parameter #'.$p->getPosition().' [ <'.($p->isOptional() ? 'optional' : 'required').'> ';"` |
|     - | 3104 | `" $t = $p->getType();"` |
|     - | 3105 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3106 | `" if($p->isPassedByReference()){ $s .= '&'; }"` |
|     - | 3107 | `" if($p->isVariadic()){ $s .= '...'; }"` |
|     - | 3108 | `" $s .= '$'.$p->getName();"` |
|     - | 3109 | `" if($p->isDefaultValueAvailable()){"` |
|     - | 3110 | `"  try{ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3111 | `"  catch(ReflectionException $e){ $s .= ' = <default>'; }"` |
|     - | 3112 | `" }"` |
|     - | 3113 | `" return $s.' ]';"` |
|     - | 3114 | `"}"` |
|     - | 3115 | `"function __reflect_export_prop($p){"` |
|     - | 3116 | `" $s = 'Property [ ';"` |
|     - | 3117 | `" $s .= $p->isPrivate() ? 'private ' : ($p->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3118 | `" if($p->isStatic()){ $s .= 'static '; }"` |
|     - | 3119 | `" if($p->isReadOnly()){ $s .= 'readonly '; }"` |
|     - | 3120 | `" $t = $p->getType();"` |
|     - | 3121 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3122 | `" $s .= '$'.$p->getName();"` |
|     - | 3123 | `" if($p->hasDefaultValue()){ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3124 | `" return $s.' ]'.chr(10);"` |
|     - | 3125 | `"}"` |
|     - | 3126 | `"function __reflect_export_cconst($c){"` |
|     - | 3127 | `" $v = $c->getValue();"` |
|     - | 3128 | `" if(is_int($v)){ $t = 'int'; }"` |
|     - | 3129 | `" else if(is_string($v)){ $t = 'string'; }"` |
|     - | 3130 | `" else if(is_float($v)){ $t = 'float'; }"` |
|     - | 3131 | `" else if(is_bool($v)){ $t = 'bool'; }"` |
|     - | 3132 | `" else if(is_array($v)){ $t = 'array'; }"` |
|     - | 3133 | `" else{ $t = 'null'; }"` |
|     - | 3134 | `" $vs = is_array($v) ? 'Array' : (is_bool($v) ? ($v ? '1' : '') : (string)$v);"` |
|     - | 3135 | `" $vis = $c->isPrivate() ? 'private' : ($c->isProtected() ? 'protected' : 'public');"` |
|     - | 3136 | `" return 'Constant [ '.$vis.' '.$t.' '.$c->name.' ] { '.$vs.' }'.chr(10);"` |
|     - | 3137 | `"}"` |
|     - | 3138 | `"function __reflect_export_fnabs($r, $indent){"` |
|     - | 3139 | `" $tags = $r->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3140 | `" if($r instanceof ReflectionMethod){"` |
|     - | 3141 | `"  if($r->isConstructor()){ $tags .= ', ctor'; }"` |
|     - | 3142 | `"  else if($r->isDestructor()){ $tags .= ', dtor'; }"` |
|     - | 3143 | `"  $decl = $r->getDeclaringClass()->name;"` |
|     - | 3144 | `"  if(strtolower($decl) !== strtolower($r->class)){ $tags .= ', inherits '.$decl; }"` |
|     - | 3145 | `"  else if($r->hasPrototype()){ $tags .= ', prototype '.$r->getPrototype()->class; }"` |
|     - | 3146 | `"  $head = 'Method [ <'.$tags.'> ';"` |
|     - | 3147 | `"  if($r->isAbstract()){ $head .= 'abstract '; }"` |
|     - | 3148 | `"  if($r->isFinal()){ $head .= 'final '; }"` |
|     - | 3149 | `"  if($r->isStatic()){ $head .= 'static '; }"` |
|     - | 3150 | `"  $head .= $r->isPrivate() ? 'private ' : ($r->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3151 | `"  $head .= 'method '.$r->name.' ]';"` |
|     - | 3152 | `" }else{"` |
|     - | 3153 | `"  $kind = $r->isClosure() ? 'Closure' : 'Function';"` |
|     - | 3154 | `"  $head = $kind.' [ <'.$tags.'> function '.$r->name.' ]';"` |
|     - | 3155 | `" }"` |
|     - | 3156 | `" $s = $head.' {'.chr(10);"` |
|     - | 3157 | `" if(!$r->isInternal()){"` |
|     - | 3158 | `"  $s .= '  @@ '.$r->getFileName().' '.$r->getStartLine().' - '.$r->getEndLine().chr(10);"` |
|     - | 3159 | `" }"` |
|     - | 3160 | `" $ps = $r->getParameters();"` |
|     - | 3161 | `" $ret = $r->getReturnType();"` |
|     - | 3162 | `" if(count($ps) > 0 \|\| $ret !== null){"` |
|     - | 3163 | `"  $s .= chr(10).'  - Parameters ['.count($ps).'] {'.chr(10);"` |
|     - | 3164 | `"  foreach($ps as $p){ $s .= '    '.__reflect_export_param($p).chr(10); }"` |
|     - | 3165 | `"  $s .= '  }'.chr(10);"` |
|     - | 3166 | `" }"` |
|     - | 3167 | `" if($ret !== null){ $s .= '  - Return [ '.(string)$ret.' ]'.chr(10); }"` |
|     - | 3168 | `" $s .= '}'.chr(10);"` |
|     - | 3169 | `" if($indent === ''){ return $s; }"` |
|     - | 3170 | `" $lines = explode(chr(10), $s);"` |
|     - | 3171 | `" $out = '';"` |
|     - | 3172 | `" $n = count($lines);"` |
|     - | 3173 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3174 | `"  if($lines[$k] === '' && $k === $n - 1){ break; }"` |
|     - | 3175 | `"  $out .= ($lines[$k] === '' ? '' : $indent.$lines[$k]).chr(10);"` |
|     - | 3176 | `" }"` |
|     - | 3177 | `" return $out;"` |
|     - | 3178 | `"}"` |
|     - | 3179 | `"function __reflect_export_class($rc){"` |
|     - | 3180 | `" $tags = $rc->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3181 | `" if($rc->isInterface()){"` |
|     - | 3182 | `"  $head = 'Interface [ <'.$tags.'> interface '.$rc->name.' ]';"` |
|     - | 3183 | `" }else{"` |
|     - | 3184 | `"  $mods = '';"` |
|     - | 3185 | `"  if($rc->isAbstract()){ $mods .= 'abstract '; }"` |
|     - | 3186 | `"  if($rc->isFinal()){ $mods .= 'final '; }"` |
|     - | 3187 | `"  $head = 'Class [ <'.$tags.'> '.$mods.'class '.$rc->name;"` |
|     - | 3188 | `"  $par = $rc->getParentClass();"` |
|     - | 3189 | `"  if($par !== false){ $head .= ' extends '.$par->name; }"` |
|     - | 3190 | `"  $ifs = $rc->getInterfaceNames();"` |
|     - | 3191 | `"  if(count($ifs) > 0){ $head .= ' implements '.implode(', ', $ifs); }"` |
|     - | 3192 | `"  $head .= ' ]';"` |
|     - | 3193 | `" }"` |
|     - | 3194 | `" $s = $head.' {'.chr(10);"` |
|     - | 3195 | `" if(!$rc->isInternal()){"` |
|     - | 3196 | `"  $s .= '  @@ '.$rc->getFileName().' '.$rc->getStartLine().'-'.$rc->getEndLine().chr(10);"` |
|     - | 3197 | `" }"` |
|     - | 3198 | `" $consts = $rc->getReflectionConstants();"` |
|     - | 3199 | `" $s .= chr(10).'  - Constants ['.count($consts).'] {'.chr(10);"` |
|     - | 3200 | `" foreach($consts as $c){ $s .= '    '.__reflect_export_cconst($c); }"` |
|     - | 3201 | `" $s .= '  }'.chr(10);"` |
|     - | 3202 | `" $sp = array();"` |
|     - | 3203 | `" $ip = array();"` |
|     - | 3204 | `" foreach($rc->getProperties() as $p){"` |
|     - | 3205 | `"  if($p->isStatic()){ $sp[] = $p; }else{ $ip[] = $p; }"` |
|     - | 3206 | `" }"` |
|     - | 3207 | `" $sm = array();"` |
|     - | 3208 | `" $im = array();"` |
|     - | 3209 | `" foreach($rc->getMethods() as $m){"` |
|     - | 3210 | `"  if($m->isStatic()){ $sm[] = $m; }else{ $im[] = $m; }"` |
|     - | 3211 | `" }"` |
|     - | 3212 | `" $s .= chr(10).'  - Static properties ['.count($sp).'] {'.chr(10);"` |
|     - | 3213 | `" foreach($sp as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3214 | `" $s .= '  }'.chr(10);"` |
|     - | 3215 | `" $s .= chr(10).'  - Static methods ['.count($sm).'] {'.chr(10);"` |
|     - | 3216 | `" $first = true;"` |
|     - | 3217 | `" foreach($sm as $m){"` |
|     - | 3218 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3219 | `"  $first = false;"` |
|     - | 3220 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3221 | `" }"` |
|     - | 3222 | `" $s .= '  }'.chr(10);"` |
|     - | 3223 | `" $s .= chr(10).'  - Properties ['.count($ip).'] {'.chr(10);"` |
|     - | 3224 | `" foreach($ip as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3225 | `" $s .= '  }'.chr(10);"` |
|     - | 3226 | `" $s .= chr(10).'  - Methods ['.count($im).'] {'.chr(10);"` |
|     - | 3227 | `" $first = true;"` |
|     - | 3228 | `" foreach($im as $m){"` |
|     - | 3229 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3230 | `"  $first = false;"` |
|     - | 3231 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3232 | `" }"` |
|     - | 3233 | `" $s .= '  }'.chr(10);"` |
|     - | 3234 | `" return $s.'}'.chr(10);"` |
|     - | 3235 | `"}"` |
|     - | 3236 | `;` |
|     - | 3237 | `/*` |
|     - | 3238 | ` * Register the __reflect_* thunks and compile the Reflection library.` |
|     - | 3239 | ` * Called from PH7_VmInit while pVm->bCompilingBuiltin is set, right after` |
|     - | 3240 | ` * the core builtin chunks (Exception and friends must exist already).` |
|     - | 3241 | ` */` |
|  3868 | 3242 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|     5 | 3243 | `{` |
|     - | 3244 | `	static const struct {` |
|     - | 3245 | `		const char *zName;` |
|     - | 3246 | `		ProchHostFunction xFunc;` |
|     - | 3247 | `	} aFunc[] = {` |
|     - | 3248 | `		{ "__reflect_class_info",     vm_builtin_reflect_class_info },` |
|     - | 3249 | `		{ "__reflect_const_value",    vm_builtin_reflect_const_value },` |
|     - | 3250 | `		{ "__reflect_static_value",   vm_builtin_reflect_static_value },` |
|     - | 3251 | `		{ "__reflect_static_set",     vm_builtin_reflect_static_set },` |
|     - | 3252 | `		{ "__reflect_prop_default",   vm_builtin_reflect_prop_default },` |
|     - | 3253 | `		{ "__reflect_new_instance",   vm_builtin_reflect_new_instance },` |
|     - | 3254 | `		{ "__reflect_new_no_ctor",    vm_builtin_reflect_new_no_ctor },` |
|     - | 3255 | `		{ "__reflect_func_info",      vm_builtin_reflect_func_info },` |
|     - | 3256 | `		{ "__reflect_param_default",  vm_builtin_reflect_param_default },` |
|     - | 3257 | `		{ "__reflect_param_defconst", vm_builtin_reflect_param_defconst },` |
|     - | 3258 | `		{ "__reflect_invoke",         vm_builtin_reflect_invoke },` |
|     - | 3259 | `		{ "__reflect_closure",        vm_builtin_reflect_closure },` |
|     - | 3260 | `		{ "__reflect_prop_read",      vm_builtin_reflect_prop_read },` |
|     - | 3261 | `		{ "__reflect_prop_write",     vm_builtin_reflect_prop_write },` |
|     - | 3262 | `		{ "__reflect_prop_state",     vm_builtin_reflect_prop_state },` |
|     - | 3263 | `		{ "__reflect_dyn_props",      vm_builtin_reflect_dyn_props },` |
|     - | 3264 | `		{ "__reflect_gen_info",       vm_builtin_reflect_gen_info },` |
|     - | 3265 | `		{ "__reflect_gen_exec",       vm_builtin_reflect_gen_exec },` |
|     - | 3266 | `		{ "__reflect_const_info",     vm_builtin_reflect_const_info },` |
|     - | 3267 | `		{ "__reflect_ref_id",         vm_builtin_reflect_ref_id },` |
|     - | 3268 | `		{ "__reflect_attr_args",      vm_builtin_reflect_attr_args },` |
|     - | 3269 | `	};` |
|     - | 3270 | `	sxu32 n;` |
|     - | 3271 | `	sxi32 rc;` |
| 85101 | 3272 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 81233 | 3273 | `		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);` |
| 40619 | 3274 | `	}` |
|  3873 | 3275 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib1, sizeof(zReflectLib1)-1);` |
|  3873 | 3276 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3277 | `		return rc;` |
|     - | 3278 | `	}` |
|  3873 | 3279 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib2, sizeof(zReflectLib2)-1);` |
|  3873 | 3280 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3281 | `		return rc;` |
|     - | 3282 | `	}` |
|  3873 | 3283 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib3, sizeof(zReflectLib3)-1);` |
|  3873 | 3284 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3285 | `		return rc;` |
|     - | 3286 | `	}` |
|  3873 | 3287 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib4, sizeof(zReflectLib4)-1);` |
|  3873 | 3288 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3289 | `		return rc;` |
|     - | 3290 | `	}` |
|  3873 | 3291 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib5, sizeof(zReflectLib5)-1);` |
|  3873 | 3292 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3293 | `		return rc;` |
|     - | 3294 | `	}` |
|  3873 | 3295 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib6, sizeof(zReflectLib6)-1);` |
|  3873 | 3296 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3297 | `		return rc;` |
|     - | 3298 | `	}` |
|  3873 | 3299 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib7, sizeof(zReflectLib7)-1);` |
|  3873 | 3300 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3301 | `		return rc;` |
|     - | 3302 | `	}` |
|  3873 | 3303 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib8, sizeof(zReflectLib8)-1);` |
|  3873 | 3304 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3305 | `		return rc;` |
|     - | 3306 | `	}` |
|  3873 | 3307 | `	return PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib9, sizeof(zReflectLib9)-1);` |
|  1939 | 3308 | `}` |
|     - | 3309 |  |
