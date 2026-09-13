# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1059/1235 lines (85.75%)

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
|   678 |   31 | `static ph7_class * ReflectResolveClass(ph7_vm *pVm, ph7_value *pArg)` |
|     4 |   32 | `{` |
|     - |   33 | `	ph7_class *pClass;` |
|   682 |   34 | `	pClass = PH7_VmExtractClassFromValue(pVm, pArg);` |
|   682 |   35 | `	if( pClass == 0 && ph7_value_is_string(pArg) ){` |
|     - |   36 | `		const char *zName;` |
|     - |   37 | `		int nLen;` |
|    16 |   38 | `		zName = ph7_value_to_string(pArg, &nLen);` |
|    16 |   39 | `		if( nLen > 0 ){` |
|    16 |   40 | `			pClass = PH7_VmTriggerAutoload(pVm, zName, (sxu32)nLen, FALSE);` |
|     7 |   41 | `		}` |
|     7 |   42 | `	}` |
|   682 |   43 | `	return pClass;` |
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
| 17534 |   63 | `static void ReflectMapAddBool(ph7_context *pCtx, ph7_value *pMap, const char *zKey, int b)` |
|     4 |   64 | `{` |
| 17538 |   65 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 17538 |   66 | `	if( p == 0 ){ return; }` |
| 17538 |   67 | `	ph7_value_bool(p, b);` |
| 17538 |   68 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  8771 |   69 | `}` |
|  5252 |   70 | `static void ReflectMapAddInt(ph7_context *pCtx, ph7_value *pMap, const char *zKey, sxi64 iVal)` |
|     4 |   71 | `{` |
|  5256 |   72 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  5256 |   73 | `	if( p == 0 ){ return; }` |
|  5256 |   74 | `	ph7_value_int64(p, iVal);` |
|  5256 |   75 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  2630 |   76 | `}` |
|  5164 |   77 | `static void ReflectMapAddStr(ph7_context *pCtx, ph7_value *pMap, const char *zKey,` |
|     - |   78 | `	const char *zVal, int nVal)` |
|     4 |   79 | `{` |
|  5168 |   80 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  5168 |   81 | `	if( p == 0 ){ return; }` |
|  5168 |   82 | `	ph7_value_string(p, zVal, nVal);` |
|  5168 |   83 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  2586 |   84 | `}` |
|  1620 |   85 | `static void ReflectMapAddNull(ph7_context *pCtx, ph7_value *pMap, const char *zKey)` |
|     3 |   86 | `{` |
|  1623 |   87 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  1623 |   88 | `	if( p == 0 ){ return; }` |
|  1623 |   89 | `	ph7_value_null(p);` |
|  1623 |   90 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|   813 |   91 | `}` |
|     - |   92 | `/* Add an entry under a dynamic (SyString) key. */` |
|   432 |   93 | `static void ReflectMapAddDyn(ph7_context *pCtx, ph7_value *pMap,` |
|     - |   94 | `	const SyString *pKey, ph7_value *pVal)` |
|     1 |   95 | `{` |
|   433 |   96 | `	ph7_value *pK = ph7_context_new_scalar(pCtx);` |
|   433 |   97 | `	if( pK == 0 ){ return; }` |
|   433 |   98 | `	ph7_value_string(pK, pKey->zString, (int)pKey->nByte);` |
|   433 |   99 | `	ph7_array_add_elem(pMap, pK, pVal);` |
|   217 |  100 | `}` |
|     - |  101 | `/* Emit the declared #[...] attributes of a target as a summary list:` |
|     - |  102 | ` * [ {name, line} ... ]. Argument values stay lazy — the PHP layer pulls` |
|     - |  103 | ` * them through __reflect_attr_args when ReflectionAttribute needs them. */` |
|  2250 |  104 | `static void ReflectMapAddAttrs(ph7_context *pCtx, ph7_value *pMap, SySet *pAttrs)` |
|     3 |  105 | `{` |
|  2253 |  106 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|  2253 |  107 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|     - |  108 | `	sxu32 n;` |
|  2253 |  109 | `	if( pList == 0 ){` |
|   ! 0 |  110 | `		return;` |
|     - |  111 | `	}` |
|  2363 |  112 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|   111 |  113 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|   111 |  114 | `		if( pMeta == 0 ){ break; }` |
|   111 |  115 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aA[n].sName), (int)SyStringLength(&aA[n].sName));` |
|   111 |  116 | `		ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)aA[n].nLine);` |
|   111 |  117 | `		ph7_array_add_elem(pList, 0, pMeta);` |
|    56 |  118 | `	}` |
|  2253 |  119 | `	ph7_array_add_strkey_elem(pMap, "attrs", pList);` |
|  1128 |  120 | `}` |
|     - |  121 | `/* Emit a doc-comment field: the text when present, else boolean false` |
|     - |  122 | ` * (getDocComment()'s exact return contract). */` |
|   920 |  123 | `static void ReflectMapAddDoc(ph7_context *pCtx, ph7_value *pMap, const SyString *pDoc)` |
|     3 |  124 | `{` |
|   923 |  125 | `	if( SyStringLength(pDoc) > 0 ){` |
|    21 |  126 | `		ReflectMapAddStr(pCtx, pMap, "doc", SyStringData(pDoc), (int)SyStringLength(pDoc));` |
|    11 |  127 | `	}else{` |
|   903 |  128 | `		ReflectMapAddBool(pCtx, pMap, "doc", 0);` |
|     - |  129 | `	}` |
|   923 |  130 | `}` |
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
|   162 |  164 | `static void ReflectCollectInterfaces(ph7_class *pClass, SySet *pOut, int iDepth)` |
|     3 |  165 | `{` |
|     - |  166 | `	ph7_class **apIface;` |
|     - |  167 | `	sxu32 n;` |
|   165 |  168 | `	if( pClass == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|   ! 0 |  169 | `		return;` |
|     - |  170 | `	}` |
|   165 |  171 | `	if( pClass->pBase ){` |
|    32 |  172 | `		ReflectCollectInterfaces(pClass->pBase, pOut, iDepth + 1);` |
|    15 |  173 | `	}` |
|   165 |  174 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   203 |  175 | `	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){` |
|    40 |  176 | `		ReflectAddInterface(apIface[n], pOut, iDepth + 1);` |
|    21 |  177 | `	}` |
|    84 |  178 | `}` |
|     - |  179 | `/*` |
|     - |  180 | ` * Deepest base class whose method table maps the same name to the very` |
|     - |  181 | ` * same ph7_class_method pointer: inheritance shares member pointers` |
|     - |  182 | ` * (PH7_ClassInherit), so this identifies the declaring class. Methods` |
|     - |  183 | ` * copied in from traits are not on the pBase chain and thus report the` |
|     - |  184 | ` * using class, which is what PHP reports too.` |
|     - |  185 | ` */` |
|   778 |  186 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth)` |
|     1 |  187 | `{` |
|   779 |  188 | `	ph7_class *pDecl = pClass;` |
|   779 |  189 | `	ph7_class *pBase = pClass->pBase;` |
|   779 |  190 | `	int iDepth = 0;` |
|  1101 |  191 | `	while( pBase && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
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
|   779 |  202 | `	return pDecl;` |
|     1 |  203 | `}` |
|     - |  204 | `/* Fetch a class attribute (property or constant) by plain name. */` |
|    86 |  205 | `static ph7_class_attr * ReflectFetchAttr(ph7_class *pClass, ph7_value *pName)` |
|     1 |  206 | `{` |
|     - |  207 | `	SyHashEntry *pEntry;` |
|     - |  208 | `	const char *zName;` |
|     - |  209 | `	int nLen;` |
|    87 |  210 | `	zName = ph7_value_to_string(pName, &nLen);` |
|    87 |  211 | `	if( nLen < 1 ){` |
|   ! 0 |  212 | `		return 0;` |
|     - |  213 | `	}` |
|    87 |  214 | `	pEntry = SyHashGet(&pClass->hAttr, (const void *)zName, (sxu32)nLen);` |
|    87 |  215 | `	if( pEntry == 0 ){` |
|   ! 0 |  216 | `		return 0;` |
|     - |  217 | `	}` |
|    87 |  218 | `	return (ph7_class_attr *)pEntry->pUserData;` |
|    44 |  219 | `}` |
|     - |  220 | `/*` |
|     - |  221 | ` * array\|null __phl_rcinfo(object\|string $target)` |
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
|   146 |  233 | `static int vm_builtin_reflect_class_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     4 |  234 | `{` |
|   150 |  235 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  236 | `	ph7_class *pClass;` |
|     - |  237 | `	ph7_value *pInfo, *pConsts, *pProps, *pMethods, *pList;` |
|     - |  238 | `	SyHashEntry *pEntry;` |
|     - |  239 | `	SySet aIfaceSet;` |
|   150 |  240 | `	sxi32 iCtorVis = 0, iCloneVis = 0;` |
|   150 |  241 | `	int bIterable = 0;` |
|     - |  242 | `	sxu32 n;` |
|   150 |  243 | `	if( nArg < 1 ){` |
|   ! 0 |  244 | `		ph7_result_null(pCtx);` |
|   ! 0 |  245 | `		return PH7_OK;` |
|     - |  246 | `	}` |
|   150 |  247 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|   150 |  248 | `	if( pClass == 0 ){` |
|    16 |  249 | `		ph7_result_null(pCtx);` |
|    16 |  250 | `		return PH7_OK;` |
|     - |  251 | `	}` |
|   135 |  252 | `	pInfo = ph7_context_new_array(pCtx);` |
|   135 |  253 | `	pConsts = ph7_context_new_array(pCtx);` |
|   135 |  254 | `	pProps = ph7_context_new_array(pCtx);` |
|   135 |  255 | `	pMethods = ph7_context_new_array(pCtx);` |
|   135 |  256 | `	if( pInfo == 0 \|\| pConsts == 0 \|\| pProps == 0 \|\| pMethods == 0 ){` |
|   ! 0 |  257 | `		ph7_result_null(pCtx);` |
|   ! 0 |  258 | `		return PH7_OK;` |
|     - |  259 | `	}` |
|   135 |  260 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   135 |  261 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pClass->iFlags & PH7_CLASS_INTERNAL) != 0);` |
|   135 |  262 | `	ReflectMapAddBool(pCtx, pInfo, "interface", (pClass->iFlags & PH7_CLASS_INTERFACE) != 0);` |
|   135 |  263 | `	ReflectMapAddBool(pCtx, pInfo, "trait", (pClass->iFlags & PH7_CLASS_TRAIT) != 0);` |
|   135 |  264 | `	ReflectMapAddBool(pCtx, pInfo, "abstract", (pClass->iFlags & PH7_CLASS_ABSTRACT) != 0);` |
|   135 |  265 | `	ReflectMapAddBool(pCtx, pInfo, "final", (pClass->iFlags & PH7_CLASS_FINAL) != 0);` |
|   135 |  266 | `	ReflectMapAddBool(pCtx, pInfo, "readonly", (pClass->iFlags & PH7_CLASS_READONLY) != 0);` |
|   135 |  267 | `	ReflectMapAddBool(pCtx, pInfo, "enum", (pClass->iFlags & PH7_CLASS_ENUM) != 0);` |
|   135 |  268 | `	if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|   ! 0 |  269 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "int", (int)sizeof("int")-1);` |
|   135 |  270 | `	}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|     3 |  271 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "string", (int)sizeof("string")-1);` |
|     2 |  272 | `	}else{` |
|   133 |  273 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "", 0);` |
|     - |  274 | `	}` |
|     - |  275 | `	{` |
|     - |  276 | `		/* Enum case names in declaration order (empty list for non-enums) */` |
|   135 |  277 | `		ph7_value *pCases = ph7_context_new_array(pCtx);` |
|   135 |  278 | `		if( pCases ){` |
|   135 |  279 | `			ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     - |  280 | `			sxu32 nCase;` |
|   141 |  281 | `			for( nCase = 0 ; nCase < SySetUsed(&pClass->aEnumCases) ; nCase++ ){` |
|     7 |  282 | `				ph7_value *pNm = ph7_context_new_scalar(pCtx);` |
|     7 |  283 | `				if( pNm ){` |
|     7 |  284 | `					ph7_value_string(pNm,apCase[nCase]->sName.zString,(int)apCase[nCase]->sName.nByte);` |
|     7 |  285 | `					ph7_array_add_elem(pCases,0,pNm);` |
|     3 |  286 | `				}` |
|     4 |  287 | `			}` |
|   135 |  288 | `			ph7_array_add_strkey_elem(pInfo,"cases",pCases);` |
|    66 |  289 | `		}` |
|     - |  290 | `	}` |
|   135 |  291 | `	if( pClass->pBase ){` |
|    41 |  292 | `		ReflectMapAddStr(pCtx, pInfo, "parent", SyStringData(&pClass->pBase->sName),` |
|    26 |  293 | `			(int)SyStringLength(&pClass->pBase->sName));` |
|    15 |  294 | `	}else{` |
|   109 |  295 | `		ReflectMapAddNull(pCtx, pInfo, "parent");` |
|     - |  296 | `	}` |
|     - |  297 | `	/* Transitive interfaces */` |
|   135 |  298 | `	SySetInit(&aIfaceSet, &pVm->sAllocator, sizeof(ph7_class *));` |
|   135 |  299 | `	ReflectCollectInterfaces(pClass, &aIfaceSet, 0);` |
|   135 |  300 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     - |  301 | `		/* An interface's own parents count as its interface list */` |
|    11 |  302 | `		if( pClass->pBase ){` |
|     3 |  303 | `			ReflectAddInterface(pClass->pBase, &aIfaceSet, 0);` |
|     1 |  304 | `		}` |
|     5 |  305 | `	}` |
|   135 |  306 | `	pList = ph7_context_new_array(pCtx);` |
|   135 |  307 | `	if( pList ){` |
|   135 |  308 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIfaceSet);` |
|   175 |  309 | `		for( n = 0 ; n < SySetUsed(&aIfaceSet) ; n++ ){` |
|    42 |  310 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|    42 |  311 | `			if( pName == 0 ){ break; }` |
|    42 |  312 | `			ph7_value_string(pName, SyStringData(&apIface[n]->sName), (int)SyStringLength(&apIface[n]->sName));` |
|    42 |  313 | `			ph7_array_add_elem(pList, 0, pName);` |
|    42 |  314 | `			if( pVm->pTraversableClass && apIface[n] == pVm->pTraversableClass ){` |
|     3 |  315 | `				bIterable = 1;` |
|     1 |  316 | `			}` |
|    22 |  317 | `		}` |
|   135 |  318 | `		ph7_array_add_strkey_elem(pInfo, "interfaces", pList);` |
|    66 |  319 | `	}` |
|   135 |  320 | `	SySetRelease(&aIfaceSet);` |
|   135 |  321 | `	ReflectMapAddBool(pCtx, pInfo, "iterable", bIterable);` |
|     - |  322 | `	/* Used traits */` |
|   135 |  323 | `	pList = ph7_context_new_array(pCtx);` |
|   135 |  324 | `	if( pList ){` |
|   135 |  325 | `		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|   137 |  326 | `		for( n = 0 ; n < SySetUsed(&pClass->aTrait) ; n++ ){` |
|     3 |  327 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|     3 |  328 | `			if( pName == 0 ){ break; }` |
|     3 |  329 | `			ph7_value_string(pName, SyStringData(&apTrait[n]->sName), (int)SyStringLength(&apTrait[n]->sName));` |
|     3 |  330 | `			ph7_array_add_elem(pList, 0, pName);` |
|     2 |  331 | `		}` |
|   135 |  332 | `		ph7_array_add_strkey_elem(pInfo, "traits", pList);` |
|    66 |  333 | `	}` |
|     - |  334 | `	/* File / lines: no file recorded => false, like PHP internals */` |
|   135 |  335 | `	if( SyStringLength(&pClass->sFile) > 0 ){` |
|   123 |  336 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));` |
|    63 |  337 | `	}else{` |
|    13 |  338 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - |  339 | `	}` |
|   135 |  340 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pClass->nLine);` |
|   135 |  341 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pClass->nEndLine);` |
|   135 |  342 | `	ReflectMapAddDoc(pCtx, pInfo, &pClass->sDoc);` |
|   135 |  343 | `	ReflectMapAddAttrs(pCtx, pInfo, &pClass->aAttrs);` |
|     - |  344 | `	/* Members are emitted in PHP's reporting order: the class's own members` |
|     - |  345 | `	 * first (declaration order), then each inheritance level's, outward.` |
|     - |  346 | `	 * Per level we iterate the DECLARING class's own hash — subclass hashes` |
|     - |  347 | `	 * interleave inherited pointers unpredictably — and emit buffered` |
|     - |  348 | `	 * entries in reverse, because SyHash lists are LIFO. A pointer-identity` |
|     - |  349 | `	 * lookup in the reflected class's hash filters out members that are not` |
|     - |  350 | `	 * visible there (base privates, overridden entries). */` |
|     - |  351 | `	{` |
|     - |  352 | `		ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];` |
|   135 |  353 | `		ph7_class *pWalk = pClass;` |
|     - |  354 | `		SySet aTmp;` |
|   135 |  355 | `		sxu32 nChain = 0, iLevel, nT;` |
|   297 |  356 | `		while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){` |
|   165 |  357 | `			aChain[nChain++] = pWalk;` |
|   165 |  358 | `			pWalk = pWalk->pBase;` |
|     3 |  359 | `		}` |
|   135 |  360 | `		SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));` |
|   297 |  361 | `		for( iLevel = 0 ; iLevel < nChain ; iLevel++ ){` |
|   165 |  362 | `			ph7_class *pLevel = aChain[iLevel];` |
|     - |  363 | `			/* --- Constants and properties (shared attribute table) --- */` |
|   165 |  364 | `			SySetReset(&aTmp);` |
|   165 |  365 | `			SyHashResetLoopCursor(&pLevel->hAttr);` |
|   393 |  366 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hAttr)) != 0 ){` |
|   229 |  367 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   229 |  368 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|   229 |  369 | `				if( iLevel == 0 ){` |
|     - |  370 | `					sxu32 j;` |
|     - |  371 | `					/* Own = declared here or by an off-chain provider (trait) */` |
|   223 |  372 | `					for( j = 1 ; j < nChain ; j++ ){` |
|    79 |  373 | `						if( aChain[j] == pDecl ){ break; }` |
|    22 |  374 | `					}` |
|   181 |  375 | `					if( j < nChain ){ continue; }` |
|    73 |  376 | `				}else{` |
|     - |  377 | `					SyHashEntry *pSub;` |
|    49 |  378 | `					if( pDecl != pLevel ){ continue; }` |
|     - |  379 | `					/* Must still be the visible member in the reflected class */` |
|    37 |  380 | `					pSub = SyHashGet(&pClass->hAttr, pEntry->pKey, pEntry->nKeyLen);` |
|    37 |  381 | `					if( pSub == 0 \|\| pSub->pUserData != (void *)pAttr ){ continue; }` |
|     - |  382 | `				}` |
|   181 |  383 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     1 |  384 | `			}` |
|   345 |  385 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|   181 |  386 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|   181 |  387 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;` |
|   181 |  388 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|   181 |  389 | `				ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|   181 |  390 | `				if( pMeta == 0 ){ break; }` |
|   181 |  391 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pAttr->iProtection);` |
|   181 |  392 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   181 |  393 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pAttr->nLine);` |
|   181 |  394 | `				ReflectMapAddDoc(pCtx, pMeta, &pAttr->sDoc);` |
|   181 |  395 | `				ReflectMapAddAttrs(pCtx, pMeta, &pAttr->aAttrs);` |
|   181 |  396 | `				ReflectMapAddBool(pCtx, pMeta, "typed", (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);` |
|   181 |  397 | `				if( SyStringLength(&pAttr->sTypeName) > 0 ){` |
|    67 |  398 | `					ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&pAttr->sTypeName),` |
|    44 |  399 | `						(int)SyStringLength(&pAttr->sTypeName));` |
|    23 |  400 | `				}else{` |
|   137 |  401 | `					ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - |  402 | `				}` |
|   181 |  403 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|    41 |  404 | `					ReflectMapAddBool(pCtx, pMeta, "final", (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|    41 |  405 | `					ReflectMapAddBool(pCtx, pMeta, "enumcase", (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0);` |
|    41 |  406 | `					ReflectMapAddDyn(pCtx, pConsts, &pAttr->sName, pMeta);` |
|    21 |  407 | `				}else{` |
|   141 |  408 | `					ReflectMapAddBool(pCtx, pMeta, "static", (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   141 |  409 | `					ReflectMapAddBool(pCtx, pMeta, "readonly", (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0);` |
|   141 |  410 | `					ReflectMapAddBool(pCtx, pMeta, "privset", (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) != 0);` |
|   141 |  411 | `					ReflectMapAddBool(pCtx, pMeta, "protset", (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET) != 0);` |
|   141 |  412 | `					ReflectMapAddBool(pCtx, pMeta, "hookget", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) != 0);` |
|   141 |  413 | `					ReflectMapAddBool(pCtx, pMeta, "hookset", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) != 0);` |
|   141 |  414 | `					ReflectMapAddBool(pCtx, pMeta, "virtual", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) != 0);` |
|   141 |  415 | `					ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&pAttr->aByteCode) > 0);` |
|   141 |  416 | `					ReflectMapAddDyn(pCtx, pProps, &pAttr->sName, pMeta);` |
|     - |  417 | `				}` |
|    91 |  418 | `			}` |
|     - |  419 | `			/* --- Methods. The reported name is the hash-entry key: trait` |
|     - |  420 | `			 * aliasing installs a shallow copy under the alias name while` |
|     - |  421 | `			 * sFunc.sName keeps the original, and PHP reports the alias. --- */` |
|   165 |  422 | `			SySetReset(&aTmp);` |
|   165 |  423 | `			SyHashResetLoopCursor(&pLevel->hMethod);` |
|   445 |  424 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){` |
|   281 |  425 | `				ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   281 |  426 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|   281 |  427 | `				if( iLevel == 0 ){` |
|     - |  428 | `					sxu32 j;` |
|   259 |  429 | `					for( j = 1 ; j < nChain ; j++ ){` |
|   111 |  430 | `						if( aChain[j] == pDecl ){ break; }` |
|    29 |  431 | `					}` |
|   203 |  432 | `					if( j < nChain ){ continue; }` |
|    75 |  433 | `				}else{` |
|     - |  434 | `					SyHashEntry *pSub;` |
|    79 |  435 | `					if( pDecl != pLevel ){ continue; }` |
|    57 |  436 | `					pSub = SyHashGet(&pClass->hMethod, pEntry->pKey, pEntry->nKeyLen);` |
|    57 |  437 | `					if( pSub == 0 ){` |
|     - |  438 | `						/* Not in the subclass table: inheritance skips private` |
|     - |  439 | `						 * methods, but PHP still reports them on the subclass` |
|     - |  440 | `						 * (Zend copies privates into the child function table). */` |
|   ! 0 |  441 | `						if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|   ! 0 |  442 | `							continue;` |
|   ! 0 |  443 | `						}` |
|    57 |  444 | `					}else if( pSub->pUserData != (void *)pMeth ){` |
|     - |  445 | `						/* Overridden below this level: already reported */` |
|     3 |  446 | `						continue;` |
|     - |  447 | `					}` |
|     - |  448 | `				}` |
|   203 |  449 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     1 |  450 | `			}` |
|   367 |  451 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|   203 |  452 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|   203 |  453 | `				ph7_class_method *pMeth = (ph7_class_method *)pE->pUserData;` |
|   203 |  454 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|     - |  455 | `				ph7_value *pMeta;` |
|     - |  456 | `				SyString sKey;` |
|     - |  457 | `				int bIsAlias;` |
|   203 |  458 | `				SyStringInitFromBuf(&sKey, (const char *)pE->pKey, pE->nKeyLen);` |
|   405 |  459 | `				bIsAlias = (sKey.nByte != SyStringLength(&pMeth->sFunc.sName)` |
|   202 |  460 | `				 \|\| SyMemcmp(sKey.zString, SyStringData(&pMeth->sFunc.sName), sKey.nByte) != 0);` |
|   202 |  461 | `				if( sKey.nByte == sizeof("__construct")-1` |
|   119 |  462 | `				 && SyMemcmp(sKey.zString, "__construct", sKey.nByte) == 0 ){` |
|    27 |  463 | `					if( iCtorVis == 0 ){` |
|    27 |  464 | `						iCtorVis = pMeth->iProtection;` |
|    13 |  465 | `					}` |
|    27 |  466 | `					if( bIsAlias ){` |
|     - |  467 | `						/* Mount-time alias for a legacy class-name constructor:` |
|     - |  468 | `						 * the method is already listed under its declared name. */` |
|   ! 0 |  469 | `						continue;` |
|     - |  470 | `					}` |
|   190 |  471 | `				}else if( sKey.nByte == sizeof("__clone")-1` |
|   102 |  472 | `				 && SyMemcmp(sKey.zString, "__clone", sKey.nByte) == 0 ){` |
|   ! 0 |  473 | `					if( iCloneVis == 0 ){` |
|   ! 0 |  474 | `						iCloneVis = pMeth->iProtection;` |
|   ! 0 |  475 | `					}` |
|   176 |  476 | `				}else if( iCtorVis == 0` |
|   144 |  477 | `				 && sKey.nByte == SyStringLength(&pClass->sName)` |
|    59 |  478 | `				 && SyMemcmp(sKey.zString, SyStringData(&pClass->sName), sKey.nByte) == 0 ){` |
|     - |  479 | `					/* Legacy class-name constructor before the mount alias exists */` |
|   ! 0 |  480 | `					iCtorVis = pMeth->iProtection;` |
|   ! 0 |  481 | `				}` |
|   203 |  482 | `				pMeta = ph7_context_new_array(pCtx);` |
|   203 |  483 | `				if( pMeta == 0 ){ break; }` |
|   203 |  484 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pMeth->iProtection);` |
|   203 |  485 | `				ReflectMapAddBool(pCtx, pMeta, "static", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   203 |  486 | `				ReflectMapAddBool(pCtx, pMeta, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|   203 |  487 | `				ReflectMapAddBool(pCtx, pMeta, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   203 |  488 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   203 |  489 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pMeth->nLine);` |
|   203 |  490 | `				ReflectMapAddDyn(pCtx, pMethods, &sKey, pMeta);` |
|   102 |  491 | `			}` |
|    84 |  492 | `		}` |
|   135 |  493 | `		SySetRelease(&aTmp);` |
|     - |  494 | `	}` |
|   135 |  495 | `	ReflectMapAddInt(pCtx, pInfo, "ctorvis", (sxi64)iCtorVis);` |
|   135 |  496 | `	ReflectMapAddInt(pCtx, pInfo, "clonevis", (sxi64)iCloneVis);` |
|   135 |  497 | `	ph7_array_add_strkey_elem(pInfo, "consts", pConsts);` |
|   135 |  498 | `	ph7_array_add_strkey_elem(pInfo, "props", pProps);` |
|   135 |  499 | `	ph7_array_add_strkey_elem(pInfo, "methods", pMethods);` |
|   135 |  500 | `	ph7_result_value(pCtx, pInfo);` |
|   135 |  501 | `	return PH7_OK;` |
|    77 |  502 | `}` |
|     - |  503 | `/*` |
|     - |  504 | ` * mixed __reflect_const_value(string $class, string $name)` |
|     - |  505 | ` * Value of a class constant. The PHP layer guarantees existence.` |
|     - |  506 | ` */` |
|    42 |  507 | `static int vm_builtin_reflect_const_value(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  508 | `{` |
|     - |  509 | `	ph7_class *pClass;` |
|     - |  510 | `	ph7_class_attr *pAttr;` |
|     - |  511 | `	ph7_value *pValue;` |
|    42 |  512 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    42 |  513 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|    43 |  514 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|   ! 0 |  515 | `		ph7_result_null(pCtx);` |
|   ! 0 |  516 | `		return PH7_OK;` |
|     - |  517 | `	}` |
|     - |  518 | `	/* Constant slots are evaluated lazily on first access */` |
|    43 |  519 | `	if( PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr) != SXRET_OK ){` |
|     - |  520 | `		/* Initializer raised: the throw is in flight; report null here */` |
|   ! 0 |  521 | `		ph7_result_null(pCtx);` |
|   ! 0 |  522 | `		return PH7_OK;` |
|     - |  523 | `	}` |
|    43 |  524 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|    43 |  525 | `	if( pValue ){` |
|    43 |  526 | `		ph7_result_value(pCtx, pValue);` |
|    22 |  527 | `	}else{` |
|   ! 0 |  528 | `		ph7_result_null(pCtx);` |
|     - |  529 | `	}` |
|    43 |  530 | `	return PH7_OK;` |
|    22 |  531 | `}` |
|     - |  532 | `/*` |
|     - |  533 | ` * mixed __reflect_static_value(string $class, string $name)` |
|     - |  534 | ` * Current value of a static property (visibility ignored).` |
|     - |  535 | ` */` |
|    12 |  536 | `static int vm_builtin_reflect_static_value(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  537 | `{` |
|     - |  538 | `	ph7_class *pClass;` |
|     - |  539 | `	ph7_class_attr *pAttr;` |
|     - |  540 | `	ph7_value *pValue;` |
|    12 |  541 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    12 |  542 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|    13 |  543 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|   ! 0 |  544 | `		ph7_result_null(pCtx);` |
|   ! 0 |  545 | `		return PH7_OK;` |
|     - |  546 | `	}` |
|     - |  547 | `	{` |
|     - |  548 | `		/* Uninitialized typed static: same Error the VM raises on read */` |
|    13 |  549 | `		SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|    13 |  550 | `		if( pSlot ){` |
|   ! 0 |  551 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|   ! 0 |  552 | `			if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|   ! 0 |  553 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|   ! 0 |  554 | `				return PH7_VmThrowException(pCtx, "Error",` |
|     - |  555 | `					"Typed static property %z::$%z must not be accessed before initialization",` |
|   ! 0 |  556 | `					&pDecl->sName, &pAttr->sName);` |
|     - |  557 | `			}` |
|   ! 0 |  558 | `		}` |
|     - |  559 | `	}` |
|    13 |  560 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|    13 |  561 | `	if( pValue ){` |
|    13 |  562 | `		ph7_result_value(pCtx, pValue);` |
|     7 |  563 | `	}else{` |
|   ! 0 |  564 | `		ph7_result_null(pCtx);` |
|     - |  565 | `	}` |
|    13 |  566 | `	return PH7_OK;` |
|     7 |  567 | `}` |
|     - |  568 | `/*` |
|     - |  569 | ` * bool __reflect_static_set(string $class, string $name, mixed $value)` |
|     - |  570 | ` * Overwrite a static property's shared slot (visibility ignored).` |
|     - |  571 | ` */` |
|     4 |  572 | `static int vm_builtin_reflect_static_set(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  573 | `{` |
|     - |  574 | `	ph7_class *pClass;` |
|     - |  575 | `	ph7_class_attr *pAttr;` |
|     - |  576 | `	ph7_value *pValue;` |
|     4 |  577 | `	if( nArg < 3 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|     4 |  578 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|     5 |  579 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|   ! 0 |  580 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  581 | `		return PH7_OK;` |
|     - |  582 | `	}` |
|     5 |  583 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|     5 |  584 | `	if( pValue == 0 ){` |
|   ! 0 |  585 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  586 | `		return PH7_OK;` |
|     - |  587 | `	}` |
|     - |  588 | `	{` |
|     5 |  589 | `		sxi32 rc = ReflectEnforceStore(pCtx, pAttr->nIdx, apArg[2]);` |
|     5 |  590 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  591 | `			return rc;` |
|     - |  592 | `		}` |
|     - |  593 | `	}` |
|     5 |  594 | `	PH7_MemObjStore(apArg[2], pValue);` |
|     5 |  595 | `	ph7_result_bool(pCtx, 1);` |
|     5 |  596 | `	return PH7_OK;` |
|     3 |  597 | `}` |
|     - |  598 | `/*` |
|     - |  599 | ` * mixed __reflect_prop_default(string $class, string $name)` |
|     - |  600 | ` * Evaluate a non-static property's compiled default expression` |
|     - |  601 | ` * (null when the property has no default).` |
|     - |  602 | ` */` |
|    24 |  603 | `static int vm_builtin_reflect_prop_default(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  604 | `{` |
|     - |  605 | `	ph7_class *pClass;` |
|     - |  606 | `	ph7_class_attr *pAttr;` |
|     - |  607 | `	ph7_value sValue;` |
|    24 |  608 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    24 |  609 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|    24 |  610 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) != 0` |
|    25 |  611 | `	 \|\| SySetUsed(&pAttr->aByteCode) < 1 ){` |
|     3 |  612 | `		ph7_result_null(pCtx);` |
|     3 |  613 | `		return PH7_OK;` |
|     - |  614 | `	}` |
|    23 |  615 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     - |  616 | `	/* Same evaluation path the VM uses for omitted call arguments */` |
|    23 |  617 | `	VmLocalExec(pCtx->pVm, &pAttr->aByteCode, &sValue, FALSE);` |
|    23 |  618 | `	ph7_result_value(pCtx, &sValue);` |
|    23 |  619 | `	PH7_MemObjRelease(&sValue);` |
|    23 |  620 | `	return PH7_OK;` |
|    13 |  621 | `}` |
|     - |  622 | `/*` |
|     - |  623 | ` * Collect a PHP array's values into a ph7_value* set (call arguments).` |
|     - |  624 | ` * When ppNames is non-NULL, string keys become named arguments: a name` |
|     - |  625 | ` * map is lazily allocated (like call_user_func_array's) with one entry` |
|     - |  626 | ` * per collected slot, empty entries meaning positional.` |
|     - |  627 | ` */` |
|    46 |  628 | `static sxi32 ReflectCollectArgs(ph7_context *pCtx, ph7_value *pArray, SySet *pOut, SyString **ppNames)` |
|     1 |  629 | `{` |
|     - |  630 | `	ph7_hashmap *pMap;` |
|     - |  631 | `	ph7_hashmap_node *pEntry;` |
|    47 |  632 | `	SyString *aNames = 0;` |
|    47 |  633 | `	sxu32 nSlot = 0;` |
|     - |  634 | `	sxu32 n;` |
|    47 |  635 | `	if( ppNames ){` |
|    27 |  636 | `		*ppNames = 0;` |
|    13 |  637 | `	}` |
|    47 |  638 | `	if( !ph7_value_is_array(pArray) ){` |
|   ! 0 |  639 | `		return SXRET_OK;` |
|     - |  640 | `	}` |
|    47 |  641 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|    47 |  642 | `	pEntry = pMap->pFirst;` |
|   107 |  643 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    61 |  644 | `		ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|    61 |  645 | `		if( pValue ){` |
|    61 |  646 | `			if( ppNames && pEntry->iType == HASHMAP_BLOB_NODE ){` |
|     3 |  647 | `				if( aNames == 0 ){` |
|     4 |  648 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     2 |  649 | `						pMap->nEntry * sizeof(SyString));` |
|     3 |  650 | `					if( aNames ){` |
|     3 |  651 | `						SyZero(aNames, pMap->nEntry * sizeof(SyString));` |
|     1 |  652 | `					}` |
|     1 |  653 | `				}` |
|     3 |  654 | `				if( aNames ){` |
|     3 |  655 | `					SyStringInitFromBuf(&aNames[nSlot],` |
|     - |  656 | `						SyBlobData(&pEntry->xKey.sKey), SyBlobLength(&pEntry->xKey.sKey));` |
|     1 |  657 | `				}` |
|     1 |  658 | `			}` |
|    61 |  659 | `			SySetPut(pOut, (const void *)&pValue);` |
|    61 |  660 | `			nSlot++;` |
|    30 |  661 | `		}` |
|    61 |  662 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|    31 |  663 | `	}` |
|    47 |  664 | `	if( ppNames ){` |
|    27 |  665 | `		*ppNames = aNames;` |
|    13 |  666 | `	}` |
|    47 |  667 | `	return SXRET_OK;` |
|    24 |  668 | `}` |
|     - |  669 | `/*` |
|     - |  670 | ` * object __reflect_new_instance(string $class, array $args)` |
|     - |  671 | ` * Instantiate and run the constructor with positional arguments.` |
|     - |  672 | ` * The PHP layer has already validated instantiability and ctor visibility.` |
|     - |  673 | ` */` |
|    30 |  674 | `static int vm_builtin_reflect_new_instance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  675 | `{` |
|    31 |  676 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  677 | `	ph7_class *pClass;` |
|     - |  678 | `	ph7_class_instance *pThis;` |
|     - |  679 | `	ph7_class_method *pCons;` |
|    31 |  680 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pVm, apArg[0])) == 0 ){` |
|   ! 0 |  681 | `		ph7_result_null(pCtx);` |
|   ! 0 |  682 | `		return PH7_OK;` |
|     - |  683 | `	}` |
|    31 |  684 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|    31 |  685 | `	if( pThis == 0 ){` |
|   ! 0 |  686 | `		ph7_result_null(pCtx);` |
|   ! 0 |  687 | `		return PH7_OK;` |
|     - |  688 | `	}` |
|    31 |  689 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|    31 |  690 | `	if( pCons ){` |
|     - |  691 | `		SySet aArg;` |
|     - |  692 | `		sxi32 rc;` |
|    27 |  693 | `		SyString *aNames = 0;` |
|    27 |  694 | `		SySetInit(&aArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    27 |  695 | `		if( nArg > 1 ){` |
|    27 |  696 | `			ReflectCollectArgs(pCtx, apArg[1], &aArg, &aNames);` |
|    13 |  697 | `		}` |
|    27 |  698 | `		if( aNames ){` |
|     - |  699 | `			VmCallArgMap sMap;` |
|     3 |  700 | `			sMap.bHasNamed = 1;` |
|     3 |  701 | `			sMap.bIsNamespaced = 0;` |
|     3 |  702 | `			sMap.bStrict = 0;` |
|     3 |  703 | `			sMap.nTotal = SySetUsed(&aArg);` |
|     3 |  704 | `			sMap.aNames = aNames;` |
|     4 |  705 | `			rc = PH7_VmCallClassMethodMap(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|     2 |  706 | `				(ph7_value **)SySetBasePtr(&aArg), &sMap);` |
|     3 |  707 | `			SyMemBackendFree(&pVm->sAllocator, aNames);` |
|     2 |  708 | `		}else{` |
|    37 |  709 | `			rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|    24 |  710 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|     - |  711 | `		}` |
|    27 |  712 | `		SySetRelease(&aArg);` |
|    27 |  713 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 |  714 | `			PH7_ClassInstanceUnref(pThis);` |
|   ! 0 |  715 | `			return rc;` |
|     - |  716 | `		}` |
|    13 |  717 | `	}` |
|    31 |  718 | `	return ReflectResultObject(pCtx, pThis);` |
|    16 |  719 | `}` |
|     - |  720 | `/*` |
|     - |  721 | ` * object __reflect_new_no_ctor(string $class)` |
|     - |  722 | ` * Instantiate without running the constructor (property defaults still` |
|     - |  723 | ` * apply — PH7_NewClassInstance builds the attribute frame).` |
|     - |  724 | ` */` |
|    68 |  725 | `static int vm_builtin_reflect_new_no_ctor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  726 | `{` |
|     - |  727 | `	ph7_class *pClass;` |
|    69 |  728 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0 ){` |
|   ! 0 |  729 | `		ph7_result_null(pCtx);` |
|   ! 0 |  730 | `		return PH7_OK;` |
|     - |  731 | `	}` |
|    69 |  732 | `	return ReflectResultObject(pCtx, PH7_NewClassInstance(pCtx->pVm, pClass));` |
|    35 |  733 | `}` |
|     - |  734 | `/*` |
|     - |  735 | ` * Typed/readonly store enforcement for reflection writes. Like the VM's` |
|     - |  736 | ` * store path, except an UNINITIALIZED readonly property may be written from` |
|     - |  737 | ` * any scope (PHP lets ReflectionProperty::setValue initialize readonly): the` |
|     - |  738 | ` * READONLY bit is masked off for the enforcement call so the set-scope check` |
|     - |  739 | ` * is skipped, while an already-initialized readonly still gets PHP's` |
|     - |  740 | ` * "Cannot modify readonly property" Error. Returns SXRET_OK/PH7_EXCEPTION/` |
|     - |  741 | ` * PH7_ABORT; the value may be coerced in place.` |
|     - |  742 | ` */` |
|    10 |  743 | `static sxi32 ReflectEnforceStore(ph7_context *pCtx, sxu32 nIdx, ph7_value *pValue)` |
|     1 |  744 | `{` |
|    11 |  745 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  746 | `	SyHashEntry *pSlot;` |
|     - |  747 | `	VmClassAttr *pVmAttr;` |
|     - |  748 | `	ph7_class_attr *pAttr;` |
|     - |  749 | `	sxi32 iSaved, rc;` |
|    11 |  750 | `	pSlot = SyHashGet(&pVm->hTypedSlot, (const void *)&nIdx, sizeof(sxu32));` |
|    11 |  751 | `	if( pSlot == 0 ){` |
|     7 |  752 | `		return SXRET_OK; /* Untyped slot: plain store */` |
|     - |  753 | `	}` |
|     5 |  754 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     5 |  755 | `	pAttr = pVmAttr->pAttr;` |
|     5 |  756 | `	if( pAttr == 0 ){` |
|   ! 0 |  757 | `		return SXRET_OK;` |
|     - |  758 | `	}` |
|     5 |  759 | `	iSaved = pAttr->iFlags;` |
|     5 |  760 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) ){` |
|   ! 0 |  761 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_READONLY;` |
|   ! 0 |  762 | `	}` |
|     5 |  763 | `	rc = PH7_VmEnforcePropStore(pVm, nIdx, pValue);` |
|     5 |  764 | `	pAttr->iFlags = iSaved;` |
|     5 |  765 | `	return rc;` |
|     6 |  766 | `}` |
|     - |  767 | `/*` |
|     - |  768 | ` * mixed __reflect_prop_read(object $obj, string $name)` |
|     - |  769 | ` * Instance property read, visibility ignored. Throws PHP's Error for an` |
|     - |  770 | ` * uninitialized typed property.` |
|     - |  771 | ` */` |
|    20 |  772 | `static int vm_builtin_reflect_prop_read(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  773 | `{` |
|     - |  774 | `	ph7_class_instance *pThis;` |
|     - |  775 | `	SyHashEntry *pEntry;` |
|     - |  776 | `	VmClassAttr *pVmAttr;` |
|     - |  777 | `	ph7_value *pValue;` |
|     - |  778 | `	const char *zName;` |
|     - |  779 | `	int nLen;` |
|    21 |  780 | `	if( nArg < 2 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  781 | `		ph7_result_null(pCtx);` |
|   ! 0 |  782 | `		return PH7_OK;` |
|     - |  783 | `	}` |
|    21 |  784 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    21 |  785 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|    21 |  786 | `	pEntry = nLen > 0 ? SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen) : 0;` |
|    21 |  787 | `	if( pEntry == 0 ){` |
|   ! 0 |  788 | `		ph7_result_null(pCtx);` |
|   ! 0 |  789 | `		return PH7_OK;` |
|     - |  790 | `	}` |
|    21 |  791 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    21 |  792 | `	if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|     3 |  793 | `		ph7_class *pDecl = pVmAttr->pAttr->pDeclClass ? pVmAttr->pAttr->pDeclClass : pThis->pClass;` |
|     4 |  794 | `		return PH7_VmThrowException(pCtx, "Error",` |
|     - |  795 | `			"Typed property %z::$%z must not be accessed before initialization",` |
|     2 |  796 | `			&pDecl->sName, &pVmAttr->pAttr->sName);` |
|     - |  797 | `	}` |
|    19 |  798 | `	pValue = PH7_ClassInstanceExtractAttrValue(pThis, pVmAttr);` |
|    19 |  799 | `	if( pValue ){` |
|    19 |  800 | `		ph7_result_value(pCtx, pValue);` |
|    10 |  801 | `	}else{` |
|   ! 0 |  802 | `		ph7_result_null(pCtx);` |
|     - |  803 | `	}` |
|    19 |  804 | `	return PH7_OK;` |
|    11 |  805 | `}` |
|     - |  806 | `/*` |
|     - |  807 | ` * bool __reflect_prop_write(object $obj, string $name, mixed $value)` |
|     - |  808 | ` * Instance property write, visibility ignored; typed and readonly rules` |
|     - |  809 | ` * enforced (see ReflectEnforceStore).` |
|     - |  810 | ` */` |
|     6 |  811 | `static int vm_builtin_reflect_prop_write(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  812 | `{` |
|     - |  813 | `	ph7_class_instance *pThis;` |
|     - |  814 | `	SyHashEntry *pEntry;` |
|     - |  815 | `	VmClassAttr *pVmAttr;` |
|     - |  816 | `	ph7_value *pValue;` |
|     - |  817 | `	const char *zName;` |
|     - |  818 | `	sxi32 rc;` |
|     - |  819 | `	int nLen;` |
|     7 |  820 | `	if( nArg < 3 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  821 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  822 | `		return PH7_OK;` |
|     - |  823 | `	}` |
|     7 |  824 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     7 |  825 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|     7 |  826 | `	pEntry = nLen > 0 ? SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen) : 0;` |
|     7 |  827 | `	if( pEntry == 0 ){` |
|   ! 0 |  828 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  829 | `		return PH7_OK;` |
|     - |  830 | `	}` |
|     7 |  831 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     7 |  832 | `	rc = ReflectEnforceStore(pCtx, pVmAttr->nIdx, apArg[2]);` |
|     7 |  833 | `	if( rc != SXRET_OK ){` |
|     3 |  834 | `		return rc;` |
|     - |  835 | `	}` |
|     5 |  836 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|     5 |  837 | `	if( pValue == 0 ){` |
|   ! 0 |  838 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  839 | `		return PH7_OK;` |
|     - |  840 | `	}` |
|     5 |  841 | `	PH7_MemObjStore(apArg[2], pValue);` |
|     5 |  842 | `	ph7_result_bool(pCtx, 1);` |
|     5 |  843 | `	return PH7_OK;` |
|     4 |  844 | `}` |
|     - |  845 | `/*` |
|     - |  846 | ` * int __reflect_prop_state(object\|string $target, string $name)` |
|     - |  847 | ` * Bitfield: 1 = exists (instance attr / static slot), 2 = initialized,` |
|     - |  848 | ` * 4 = dynamic (instance-owned, not class-declared).` |
|     - |  849 | ` */` |
|    16 |  850 | `static int vm_builtin_reflect_prop_state(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  851 | `{` |
|    17 |  852 | `	int iState = 0;` |
|     - |  853 | `	const char *zName;` |
|     - |  854 | `	int nLen;` |
|    17 |  855 | `	if( nArg < 2 ){` |
|   ! 0 |  856 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  857 | `		return PH7_OK;` |
|     - |  858 | `	}` |
|    17 |  859 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|    17 |  860 | `	if( nLen < 1 ){` |
|   ! 0 |  861 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  862 | `		return PH7_OK;` |
|     - |  863 | `	}` |
|    17 |  864 | `	if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|    17 |  865 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    17 |  866 | `		SyHashEntry *pEntry = SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen);` |
|    17 |  867 | `		if( pEntry ){` |
|    17 |  868 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    17 |  869 | `			iState \|= 1;` |
|    17 |  870 | `			if( (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|    15 |  871 | `				iState \|= 2;` |
|     7 |  872 | `			}` |
|    17 |  873 | `			if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|    11 |  874 | `				iState \|= 4;` |
|     5 |  875 | `			}` |
|     8 |  876 | `		}` |
|     9 |  877 | `	}else{` |
|   ! 0 |  878 | `		ph7_class *pClass = ReflectResolveClass(pCtx->pVm, apArg[0]);` |
|   ! 0 |  879 | `		ph7_class_attr *pAttr = pClass ? ReflectFetchAttr(pClass, apArg[1]) : 0;` |
|   ! 0 |  880 | `		if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|   ! 0 |  881 | `			SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|   ! 0 |  882 | `			iState \|= 1 \| 2;` |
|   ! 0 |  883 | `			if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|   ! 0 |  884 | `				iState &= ~2;` |
|   ! 0 |  885 | `			}` |
|   ! 0 |  886 | `		}` |
|     - |  887 | `	}` |
|    17 |  888 | `	ph7_result_int(pCtx, iState);` |
|    17 |  889 | `	return PH7_OK;` |
|     9 |  890 | `}` |
|     - |  891 | `/*` |
|     - |  892 | ` * array __reflect_dyn_props(object $obj)` |
|     - |  893 | ` * Names of the instance's runtime-added (dynamic) properties, in creation` |
|     - |  894 | ` * order (the instance attr table inserts dynamics at the tail).` |
|     - |  895 | ` */` |
|     2 |  896 | `static int vm_builtin_reflect_dyn_props(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  897 | `{` |
|     - |  898 | `	ph7_class_instance *pThis;` |
|     - |  899 | `	SyHashEntry *pEntry;` |
|     - |  900 | `	ph7_value *pList;` |
|     2 |  901 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|     3 |  902 | `	 \|\| (pList = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 |  903 | `		ph7_result_null(pCtx);` |
|   ! 0 |  904 | `		return PH7_OK;` |
|     - |  905 | `	}` |
|     3 |  906 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     3 |  907 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     7 |  908 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     5 |  909 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     5 |  910 | `		if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|     5 |  911 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|     5 |  912 | `			if( pName == 0 ){ break; }` |
|     7 |  913 | `			ph7_value_string(pName, SyStringData(&pVmAttr->pAttr->sName),` |
|     4 |  914 | `				(int)SyStringLength(&pVmAttr->pAttr->sName));` |
|     5 |  915 | `			ph7_array_add_elem(pList, 0, pName);` |
|     2 |  916 | `		}` |
|     1 |  917 | `	}` |
|     3 |  918 | `	ph7_result_value(pCtx, pList);` |
|     3 |  919 | `	return PH7_OK;` |
|     2 |  920 | `}` |
|     - |  921 | `/* Hand an EXISTING instance to the caller: takes an extra reference` |
|     - |  922 | ` * (unlike ReflectResultObject, which transfers a fresh instance's one). */` |
|     4 |  923 | `static int ReflectResultExistingObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|     1 |  924 | `{` |
|     5 |  925 | `	if( pObj == 0 ){` |
|   ! 0 |  926 | `		ph7_result_null(pCtx);` |
|   ! 0 |  927 | `		return PH7_OK;` |
|     - |  928 | `	}` |
|     5 |  929 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     5 |  930 | `	pObj->iRef++;` |
|     5 |  931 | `	pCtx->pRet->x.pOther = pObj;` |
|     5 |  932 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|     5 |  933 | `	return PH7_OK;` |
|     3 |  934 | `}` |
|     - |  935 | `/* pVal is a Closure instance? Return it, else NULL. */` |
|   488 |  936 | `static ph7_class_instance * ReflectValueClosure(ph7_vm *pVm, ph7_value *pVal)` |
|     2 |  937 | `{` |
|     - |  938 | `	ph7_class_instance *pThis;` |
|   490 |  939 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
|   418 |  940 | `		return 0;` |
|     - |  941 | `	}` |
|    73 |  942 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    73 |  943 | `	return (pThis->pClass == pVm->pClosureClass) ? pThis : 0;` |
|   246 |  944 | `}` |
|     - |  945 | `/*` |
|     - |  946 | ` * Resolve a reflection callable target into its compiled function.` |
|     - |  947 | ` *   - pMethodArg a non-empty string  -> method mode: pTarget is a class name` |
|     - |  948 | ` *     or object; outputs *ppClass and *ppMeth.` |
|     - |  949 | ` *   - pTarget a Closure              -> unwrap $__fn into hFunction; *ppClosure.` |
|     - |  950 | ` *   - pTarget a string               -> hFunction (user) or hHostFunction` |
|     - |  951 | ` *     (*ppHost set, returns NULL).` |
|     - |  952 | ` * Returns the ph7_vm_func, or NULL (host function or unresolvable).` |
|     - |  953 | ` */` |
|   792 |  954 | `static ph7_vm_func * ReflectResolveCallable(ph7_context *pCtx, ph7_value *pTarget,` |
|     - |  955 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|     - |  956 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|     2 |  957 | `{` |
|   794 |  958 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  959 | `	SyHashEntry *pEntry;` |
|   794 |  960 | `	if( ppClass ){ *ppClass = 0; }` |
|   794 |  961 | `	if( ppMeth ){ *ppMeth = 0; }` |
|   794 |  962 | `	if( ppHost ){ *ppHost = 0; }` |
|   794 |  963 | `	if( ppClosure ){ *ppClosure = 0; }` |
|   794 |  964 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
|   305 |  965 | `		ph7_class *pClass = ReflectResolveClass(pVm, pTarget);` |
|     - |  966 | `		ph7_class_method *pMeth;` |
|   305 |  967 | `		if( pClass == 0 ){` |
|   ! 0 |  968 | `			return 0;` |
|     - |  969 | `		}` |
|   457 |  970 | `		pMeth = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|   152 |  971 | `			SyBlobLength(&pMethodArg->sBlob));` |
|   305 |  972 | `		if( pMeth == 0 ){` |
|     - |  973 | `			/* getMethods()/ReflectionClass reports a base class's PRIVATE methods on` |
|     - |  974 | `			 * the subclass (php copies them into the child's table), but private` |
|     - |  975 | `			 * methods are not inherited into the child's method table, so the plain` |
|     - |  976 | `			 * extract misses them when a ReflectionMethod obtained from the subclass` |
|     - |  977 | `			 * is re-resolved by (subclass, name). Walk the base chain to find the` |
|     - |  978 | `			 * declaring class's own copy. */` |
|   ! 0 |  979 | `			ph7_class *pWalk = pClass->pBase;` |
|   ! 0 |  980 | `			while( pWalk && pMeth == 0 ){` |
|   ! 0 |  981 | `				pMeth = PH7_ClassExtractMethod(pWalk, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|   ! 0 |  982 | `					SyBlobLength(&pMethodArg->sBlob));` |
|   ! 0 |  983 | `				pWalk = pWalk->pBase;` |
|   ! 0 |  984 | `			}` |
|   ! 0 |  985 | `		}` |
|   305 |  986 | `		if( pMeth == 0 ){` |
|   ! 0 |  987 | `			return 0;` |
|     - |  988 | `		}` |
|   305 |  989 | `		if( ppClass ){ *ppClass = pClass; }` |
|   305 |  990 | `		if( ppMeth ){ *ppMeth = pMeth; }` |
|   305 |  991 | `		return &pMeth->sFunc;` |
|     - |  992 | `	}` |
|     - |  993 | `	{` |
|   490 |  994 | `		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|   490 |  995 | `		if( pClo ){` |
|     - |  996 | `			SyString sAttr;` |
|     - |  997 | `			ph7_value *pFn;` |
|    73 |  998 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    73 |  999 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|    73 | 1000 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) < 1 ){` |
|   ! 0 | 1001 | `				return 0;` |
|     - | 1002 | `			}` |
|     - | 1003 | `			/* A closure over an object method or __invoke object` |
|     - | 1004 | `			 * (Closure::fromCallable([$obj,'m']) / fromCallable($invokeObj)) stores the` |
|     - | 1005 | `			 * bare method name in $__fn and the class in $__scope. Resolve it as a METHOD` |
|     - | 1006 | `			 * of $__scope FIRST — before the global function table — so a same-named` |
|     - | 1007 | `			 * global function does not shadow the method (the bug: $__fn "add" hitting a` |
|     - | 1008 | `			 * global add()). A plain anonymous closure's $__fn is its unique lambda name,` |
|     - | 1009 | `			 * which is not a method, so this falls through cleanly. */` |
|     - | 1010 | `			{` |
|     - | 1011 | `				SyString sScope;` |
|     - | 1012 | `				ph7_value *pScope;` |
|    73 | 1013 | `				SyStringInitFromBuf(&sScope, "__scope", 7);` |
|    73 | 1014 | `				pScope = PH7_ClassInstanceFetchAttr(pClo, &sScope);` |
|    73 | 1015 | `				if( pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0 ){` |
|    31 | 1016 | `					ph7_class *pScopeCls = PH7_VmExtractClass(pVm,` |
|    20 | 1017 | `						(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|    21 | 1018 | `					if( pScopeCls ){` |
|    31 | 1019 | `						ph7_class_method *pScopeMeth = PH7_ClassExtractMethod(pScopeCls,` |
|    20 | 1020 | `							(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    21 | 1021 | `						if( pScopeMeth ){` |
|    21 | 1022 | `							if( ppClass ){ *ppClass = pScopeCls; }` |
|    21 | 1023 | `							if( ppMeth ){ *ppMeth = pScopeMeth; }` |
|    21 | 1024 | `							if( ppClosure ){ *ppClosure = pClo; }` |
|    21 | 1025 | `							return &pScopeMeth->sFunc;` |
|     - | 1026 | `						}` |
|   ! 0 | 1027 | `					}` |
|   ! 0 | 1028 | `				}` |
|     - | 1029 | `			}` |
|    53 | 1030 | `			pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    53 | 1031 | `			if( pEntry == 0 ){` |
|     - | 1032 | `				/* A Closure over a host function (Closure::fromCallable('strlen')) */` |
|   ! 0 | 1033 | `				pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|   ! 0 | 1034 | `				if( pEntry && ppHost ){` |
|   ! 0 | 1035 | `					*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|   ! 0 | 1036 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|   ! 0 | 1037 | `				}` |
|   ! 0 | 1038 | `				return 0;` |
|     - | 1039 | `			}` |
|    53 | 1040 | `			if( ppClosure ){ *ppClosure = pClo; }` |
|    53 | 1041 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - | 1042 | `		}` |
|     - | 1043 | `	}` |
|   418 | 1044 | `	if( pTarget->iFlags & MEMOBJ_STRING ){` |
|   418 | 1045 | `		if( SyBlobLength(&pTarget->sBlob) < 1 ){` |
|   ! 0 | 1046 | `			return 0;` |
|     - | 1047 | `		}` |
|   418 | 1048 | `		pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   418 | 1049 | `		if( pEntry ){` |
|   285 | 1050 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - | 1051 | `		}` |
|   134 | 1052 | `		pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   134 | 1053 | `		if( pEntry && ppHost ){` |
|   130 | 1054 | `			*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|    64 | 1055 | `		}` |
|    66 | 1056 | `	}` |
|   134 | 1057 | `	return 0;` |
|   398 | 1058 | `}` |
|     - | 1059 | `/* Emit the shared descriptor fields of a compiled function. */` |
|   608 | 1060 | `static void ReflectFillFuncCommon(ph7_context *pCtx, ph7_value *pInfo, ph7_vm_func *pFunc)` |
|     1 | 1061 | `{` |
|     - | 1062 | `	ph7_vm_func_arg *aArg;` |
|     - | 1063 | `	ph7_value *pParams, *pStatics;` |
|   609 | 1064 | `	int bVariadic = 0;` |
|     - | 1065 | `	int bAnon;` |
|     - | 1066 | `	sxu32 n;` |
|     - | 1067 | ``	/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|     - | 1068 | `	 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|   609 | 1069 | `	bAnon = (pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|   608 | 1070 | `	if( !bAnon && SyStringLength(&pFunc->sName) > 9` |
|   328 | 1071 | `	 && (SyMemcmp(SyStringData(&pFunc->sName), "[lambda_", 8) == 0` |
|    88 | 1072 | `	  \|\| SyMemcmp(SyStringData(&pFunc->sName), "[closure_", 9) == 0) ){` |
|     5 | 1073 | `		bAnon = 1;` |
|     2 | 1074 | `	}` |
|   609 | 1075 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|   609 | 1076 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pFunc->iFlags & VM_FUNC_INTERNAL) != 0);` |
|   609 | 1077 | `	ReflectMapAddBool(pCtx, pInfo, "closure", bAnon);` |
|   609 | 1078 | `	ReflectMapAddBool(pCtx, pInfo, "fstatic", (pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);` |
|   609 | 1079 | `	ReflectMapAddBool(pCtx, pInfo, "byref", (pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|   609 | 1080 | `	ReflectMapAddBool(pCtx, pInfo, "generator", (pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|   609 | 1081 | `	ReflectMapAddBool(pCtx, pInfo, "strict", pFunc->bStrictTypes != 0);` |
|   609 | 1082 | `	if( SyStringLength(&pFunc->sFile) > 0 ){` |
|   605 | 1083 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pFunc->sFile), (int)SyStringLength(&pFunc->sFile));` |
|   303 | 1084 | `	}else{` |
|     5 | 1085 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1086 | `	}` |
|   609 | 1087 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pFunc->nLine);` |
|   609 | 1088 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pFunc->nEndLine);` |
|   609 | 1089 | `	ReflectMapAddDoc(pCtx, pInfo, &pFunc->sDoc);` |
|   609 | 1090 | `	ReflectMapAddAttrs(pCtx, pInfo, &pFunc->aAttrs);` |
|   609 | 1091 | `	if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|   166 | 1092 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", SyStringData(&pFunc->sReturnTypeName),` |
|   110 | 1093 | `			(int)SyStringLength(&pFunc->sReturnTypeName));` |
|   554 | 1094 | `	}else if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     - | 1095 | `		/* The type-text renderer omits void/never atoms (compile.c notes the` |
|     - | 1096 | `		 * root fix belongs there); name them here for getReturnType(). */` |
|     3 | 1097 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "void", sizeof("void")-1);` |
|   498 | 1098 | `	}else if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|     3 | 1099 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "never", sizeof("never")-1);` |
|     2 | 1100 | `	}else{` |
|   495 | 1101 | `		ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1102 | `	}` |
|   609 | 1103 | `	ReflectMapAddBool(pCtx, pInfo, "retnullable", (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) != 0);` |
|     - | 1104 | `	/* Parameters */` |
|   609 | 1105 | `	pParams = ph7_context_new_array(pCtx);` |
|   609 | 1106 | `	aArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|  1901 | 1107 | `	for( n = 0 ; pParams && n < SySetUsed(&pFunc->aArgs) ; n++ ){` |
|  1293 | 1108 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|  1293 | 1109 | `		if( pMeta == 0 ){ break; }` |
|  1293 | 1110 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aArg[n].sName), (int)SyStringLength(&aArg[n].sName));` |
|  1293 | 1111 | `		ReflectMapAddInt(pCtx, pMeta, "pos", (sxi64)n);` |
|  1293 | 1112 | `		ReflectMapAddBool(pCtx, pMeta, "byref", (aArg[n].iFlags & VM_FUNC_ARG_BY_REF) != 0);` |
|  1293 | 1113 | `		ReflectMapAddBool(pCtx, pMeta, "variadic", (aArg[n].iFlags & VM_FUNC_ARG_VARIADIC) != 0);` |
|     - | 1114 | `		/* The compiler never sets ARG_HAS_DEF; a default = compiled bytecode` |
|     - | 1115 | `		 * (same test the OP_CALL default-value path uses). */` |
|  1293 | 1116 | `		ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&aArg[n].aByteCode) > 0);` |
|  1293 | 1117 | `		ReflectMapAddBool(pCtx, pMeta, "nullable", (aArg[n].iFlags & VM_FUNC_ARG_NULLABLE) != 0);` |
|  1293 | 1118 | `		ReflectMapAddBool(pCtx, pMeta, "promoted", (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) != 0);` |
|  1293 | 1119 | `		if( SyStringLength(&aArg[n].sTypeName) > 0 ){` |
|   775 | 1120 | `			ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&aArg[n].sTypeName),` |
|   516 | 1121 | `				(int)SyStringLength(&aArg[n].sTypeName));` |
|   259 | 1122 | `		}else{` |
|   777 | 1123 | `			ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - | 1124 | `		}` |
|  1293 | 1125 | `		ReflectMapAddAttrs(pCtx, pMeta, &aArg[n].aAttrs);` |
|  1293 | 1126 | `		ph7_array_add_elem(pParams, 0, pMeta);` |
|  1293 | 1127 | `		if( aArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|   189 | 1128 | `			bVariadic = 1;` |
|    94 | 1129 | `		}` |
|   647 | 1130 | `	}` |
|   609 | 1131 | `	if( pParams ){` |
|   609 | 1132 | `		ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|   304 | 1133 | `	}` |
|   609 | 1134 | `	ReflectMapAddBool(pCtx, pInfo, "variadic", bVariadic);` |
|     - | 1135 | `	/* Static variables: current value when the slot was initialized (first` |
|     - | 1136 | `	 * call), otherwise the evaluated default — PHP's getStaticVariables` |
|     - | 1137 | `	 * initializes on demand and reports the same values. */` |
|   609 | 1138 | `	pStatics = ph7_context_new_array(pCtx);` |
|   609 | 1139 | `	if( pStatics ){` |
|   609 | 1140 | `		ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|   637 | 1141 | `		for( n = 0 ; n < SySetUsed(&pFunc->aStatic) ; n++ ){` |
|    29 | 1142 | `			ph7_value *pVal = 0;` |
|     - | 1143 | `			ph7_value sScratch;` |
|    29 | 1144 | `			int bScratch = 0;` |
|    29 | 1145 | `			if( aStatic[n].nIdx != SXU32_HIGH ){` |
|    11 | 1146 | `				pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);` |
|     5 | 1147 | `			}` |
|    29 | 1148 | `			if( pVal == 0 ){` |
|    19 | 1149 | `				PH7_MemObjInit(pCtx->pVm, &sScratch);` |
|    19 | 1150 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|    19 | 1151 | `					VmLocalExec(pCtx->pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|     9 | 1152 | `				}` |
|    19 | 1153 | `				pVal = &sScratch;` |
|    19 | 1154 | `				bScratch = 1;` |
|     9 | 1155 | `			}` |
|    29 | 1156 | `			ReflectMapAddDyn(pCtx, pStatics, &aStatic[n].sName, pVal);` |
|    29 | 1157 | `			if( bScratch ){` |
|    19 | 1158 | `				PH7_MemObjRelease(&sScratch);` |
|     9 | 1159 | `			}` |
|    15 | 1160 | `		}` |
|   609 | 1161 | `		ph7_array_add_strkey_elem(pInfo, "statics", pStatics);` |
|   304 | 1162 | `	}` |
|   609 | 1163 | `}` |
|     - | 1164 | `/*` |
|     - | 1165 | ` * array\|null __reflect_func_info(string\|Closure $target [, string $method])` |
|     - | 1166 | ` * Function/method/closure descriptor for the PHP layer.` |
|     - | 1167 | ` */` |
|   740 | 1168 | `static int vm_builtin_reflect_func_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 1169 | `{` |
|     - | 1170 | `	ph7_vm_func *pFunc;` |
|   742 | 1171 | `	ph7_class *pClass = 0;` |
|   742 | 1172 | `	ph7_class_method *pMeth = 0;` |
|   742 | 1173 | `	ph7_user_func *pHost = 0;` |
|   742 | 1174 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1175 | `	ph7_value *pInfo;` |
|   742 | 1176 | `	if( nArg < 1 ){` |
|   ! 0 | 1177 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1178 | `		return PH7_OK;` |
|     - | 1179 | `	}` |
|   742 | 1180 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], nArg > 1 ? apArg[1] : 0,` |
|     - | 1181 | `		&pClass, &pMeth, &pHost, &pClosure);` |
|   742 | 1182 | `	if( pFunc == 0 && pHost == 0 ){` |
|     6 | 1183 | `		ph7_result_null(pCtx);` |
|     6 | 1184 | `		return PH7_OK;` |
|     - | 1185 | `	}` |
|   738 | 1186 | `	pInfo = ph7_context_new_array(pCtx);` |
|   738 | 1187 | `	if( pInfo == 0 ){` |
|   ! 0 | 1188 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1189 | `		return PH7_OK;` |
|     - | 1190 | `	}` |
|   738 | 1191 | `	if( pFunc == 0 ){` |
|     - | 1192 | `		/* Host (C builtin) function: no parameter metadata beyond arity */` |
|   130 | 1193 | `		ph7_value *pParams = ph7_context_new_array(pCtx);` |
|   130 | 1194 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pHost->sName), (int)SyStringLength(&pHost->sName));` |
|   130 | 1195 | `		ReflectMapAddBool(pCtx, pInfo, "internal", 1);` |
|   130 | 1196 | `		ReflectMapAddBool(pCtx, pInfo, "closure", 0);` |
|   130 | 1197 | `		ReflectMapAddBool(pCtx, pInfo, "fstatic", 0);` |
|   130 | 1198 | `		ReflectMapAddBool(pCtx, pInfo, "byref", 0);` |
|   130 | 1199 | `		ReflectMapAddBool(pCtx, pInfo, "generator", 0);` |
|   130 | 1200 | `		ReflectMapAddBool(pCtx, pInfo, "strict", 0);` |
|   130 | 1201 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|   130 | 1202 | `		ReflectMapAddInt(pCtx, pInfo, "line", 0);` |
|   130 | 1203 | `		ReflectMapAddInt(pCtx, pInfo, "endline", 0);` |
|   130 | 1204 | `		ReflectMapAddBool(pCtx, pInfo, "doc", 0);` |
|     - | 1205 | `		{` |
|   130 | 1206 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|   130 | 1207 | `			if( pEmpty ){` |
|   130 | 1208 | `				ph7_array_add_strkey_elem(pInfo, "attrs", pEmpty);` |
|    64 | 1209 | `			}` |
|     - | 1210 | `		}` |
|   130 | 1211 | `		if( pHost->zRet ){` |
|   130 | 1212 | `			ReflectMapAddStr(pCtx, pInfo, "rettext", pHost->zRet, (int)SyStrlen(pHost->zRet));` |
|    66 | 1213 | `		}else{` |
|   ! 0 | 1214 | `			ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1215 | `		}` |
|   130 | 1216 | `		ReflectMapAddBool(pCtx, pInfo, "retnullable", 0);` |
|   130 | 1217 | `		if( pParams ){` |
|   130 | 1218 | `			ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|    64 | 1219 | `		}` |
|   130 | 1220 | `		ReflectMapAddBool(pCtx, pInfo, "variadic", 0);` |
|   130 | 1221 | `		ReflectMapAddInt(pCtx, pInfo, "minarg", (sxi64)pHost->nMinArg);` |
|   130 | 1222 | `		if( pHost->zSig ){` |
|   130 | 1223 | `			ReflectMapAddStr(pCtx, pInfo, "sig", pHost->zSig, (int)SyStrlen(pHost->zSig));` |
|    66 | 1224 | `		}else{` |
|   ! 0 | 1225 | `			ReflectMapAddStr(pCtx, pInfo, "sig", "", 0);` |
|     - | 1226 | `		}` |
|   130 | 1227 | `		ph7_result_value(pCtx, pInfo);` |
|   130 | 1228 | `		return PH7_OK;` |
|     - | 1229 | `	}` |
|   609 | 1230 | `	ReflectFillFuncCommon(pCtx, pInfo, pFunc);` |
|   609 | 1231 | `	ReflectMapAddInt(pCtx, pInfo, "minarg", -1);` |
|   609 | 1232 | `	if( (pFunc->iFlags & VM_FUNC_INTERNAL) && SySetUsed(&pFunc->aArgs) == 0 && pMeth == 0 ){` |
|     - | 1233 | `		/* Embedded-PHP builtin (max/min...): declared argless, actual` |
|     - | 1234 | `		 * signature comes from the static table */` |
|     5 | 1235 | `		const char *zRet = 0;` |
|     5 | 1236 | `		const char *zSig = PH7_VmBuiltinSigLookup(SyStringData(&pFunc->sName), SyStringLength(&pFunc->sName), &zRet);` |
|     5 | 1237 | `		if( zSig ){` |
|     5 | 1238 | `			ReflectMapAddStr(pCtx, pInfo, "sig", zSig, (int)SyStrlen(zSig));` |
|     2 | 1239 | `		}` |
|     5 | 1240 | `		if( zRet && SyStringLength(&pFunc->sReturnTypeName) == 0 ){` |
|     5 | 1241 | `			ReflectMapAddStr(pCtx, pInfo, "ret2", zRet, (int)SyStrlen(zRet));` |
|     2 | 1242 | `		}` |
|     2 | 1243 | `	}` |
|   609 | 1244 | `	if( pMeth && pClass ){` |
|   297 | 1245 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|   297 | 1246 | `		ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   297 | 1247 | `		ReflectMapAddStr(pCtx, pInfo, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   297 | 1248 | `		ReflectMapAddInt(pCtx, pInfo, "vis", (sxi64)pMeth->iProtection);` |
|   297 | 1249 | `		ReflectMapAddBool(pCtx, pInfo, "mstatic", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   297 | 1250 | `		ReflectMapAddBool(pCtx, pInfo, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|   297 | 1251 | `		ReflectMapAddBool(pCtx, pInfo, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   148 | 1252 | `	}` |
|   609 | 1253 | `	if( pClosure ){` |
|     - | 1254 | `		SyString sAttr;` |
|     - | 1255 | `		ph7_value *pAttr;` |
|     - | 1256 | `		ph7_value *pUsed;` |
|    69 | 1257 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|    69 | 1258 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    79 | 1259 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|    21 | 1260 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|    21 | 1261 | `			if( pKey ){` |
|    21 | 1262 | `				ph7_value_string(pKey, "this", 4);` |
|    21 | 1263 | `				ph7_array_add_elem(pInfo, pKey, pAttr);` |
|    10 | 1264 | `			}` |
|    11 | 1265 | `		}else{` |
|    49 | 1266 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|     - | 1267 | `		}` |
|    69 | 1268 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|    69 | 1269 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    69 | 1270 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|    31 | 1271 | `			ReflectMapAddStr(pCtx, pInfo, "scope", (const char *)SyBlobData(&pAttr->sBlob),` |
|    20 | 1272 | `				(int)SyBlobLength(&pAttr->sBlob));` |
|    11 | 1273 | `		}else{` |
|    49 | 1274 | `			ReflectMapAddNull(pCtx, pInfo, "scope");` |
|     - | 1275 | `		}` |
|     - | 1276 | `		/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */` |
|    69 | 1277 | `		pUsed = ph7_context_new_array(pCtx);` |
|    69 | 1278 | `		if( pUsed ){` |
|    69 | 1279 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     - | 1280 | `			sxu32 n;` |
|   127 | 1281 | `			for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; n++ ){` |
|    59 | 1282 | `				if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){` |
|    41 | 1283 | `					continue;` |
|     - | 1284 | `				}` |
|    18 | 1285 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|    10 | 1286 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|   ! 0 | 1287 | `					continue;` |
|     - | 1288 | `				}` |
|    19 | 1289 | `				if( (aEnv[n].iFlags & VM_FUNC_ARG_BY_REF) && aEnv[n].nIdx != SXU32_HIGH ){` |
|     - | 1290 | `					/* Captured by reference: report the slot's live value */` |
|     5 | 1291 | `					ph7_value *pLive = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aEnv[n].nIdx);` |
|     5 | 1292 | `					ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, pLive ? pLive : &aEnv[n].sValue);` |
|     5 | 1293 | `					continue;` |
|     - | 1294 | `				}` |
|    15 | 1295 | `				ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, &aEnv[n].sValue);` |
|     8 | 1296 | `			}` |
|    69 | 1297 | `			ph7_array_add_strkey_elem(pInfo, "used", pUsed);` |
|    34 | 1298 | `		}` |
|    34 | 1299 | `	}` |
|   609 | 1300 | `	ph7_result_value(pCtx, pInfo);` |
|   609 | 1301 | `	return PH7_OK;` |
|   372 | 1302 | `}` |
|     - | 1303 | `/*` |
|     - | 1304 | ` * mixed __reflect_param_default(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1305 | ` * Evaluate a parameter's compiled default expression.` |
|     - | 1306 | ` */` |
|    12 | 1307 | `static int vm_builtin_reflect_param_default(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1308 | `{` |
|     - | 1309 | `	ph7_vm_func *pFunc;` |
|     - | 1310 | `	ph7_vm_func_arg *pArg;` |
|     - | 1311 | `	ph7_value sValue;` |
|     - | 1312 | `	sxu32 nIdx;` |
|    13 | 1313 | `	if( nArg < 3 ){` |
|   ! 0 | 1314 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1315 | `		return PH7_OK;` |
|     - | 1316 | `	}` |
|    13 | 1317 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|    13 | 1318 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|    12 | 1319 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|    13 | 1320 | `	 \|\| SySetUsed(&pArg->aByteCode) < 1 ){` |
|   ! 0 | 1321 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1322 | `		return PH7_OK;` |
|     - | 1323 | `	}` |
|    13 | 1324 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|    13 | 1325 | `	VmLocalExec(pCtx->pVm, &pArg->aByteCode, &sValue, FALSE);` |
|    13 | 1326 | `	ph7_result_value(pCtx, &sValue);` |
|    13 | 1327 | `	PH7_MemObjRelease(&sValue);` |
|    13 | 1328 | `	return PH7_OK;` |
|     7 | 1329 | `}` |
|     - | 1330 | `/*` |
|     - | 1331 | ` * string\|null __reflect_param_defconst(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1332 | ` * When a parameter's default is a plain global-constant reference, its` |
|     - | 1333 | ` * source name; null otherwise. A constant default compiles to exactly` |
|     - | 1334 | ` * [ OP_LOADC (EXPAND) , OP_DONE ] with the name in the literal table.` |
|     - | 1335 | ` */` |
|     6 | 1336 | `static int vm_builtin_reflect_param_defconst(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1337 | `{` |
|     - | 1338 | `	ph7_vm_func *pFunc;` |
|     - | 1339 | `	ph7_vm_func_arg *pArg;` |
|     - | 1340 | `	VmInstr *aInstr;` |
|     - | 1341 | `	ph7_value *pLit;` |
|     - | 1342 | `	sxu32 nIdx;` |
|     7 | 1343 | `	if( nArg < 3 ){` |
|   ! 0 | 1344 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1345 | `		return PH7_OK;` |
|     - | 1346 | `	}` |
|     7 | 1347 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|     7 | 1348 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|     6 | 1349 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|     7 | 1350 | `	 \|\| SySetUsed(&pArg->aByteCode) != 2 ){` |
|     3 | 1351 | `		ph7_result_null(pCtx);` |
|     3 | 1352 | `		return PH7_OK;` |
|     - | 1353 | `	}` |
|     5 | 1354 | `	aInstr = (VmInstr *)SySetBasePtr(&pArg->aByteCode);` |
|     4 | 1355 | `	if( aInstr[0].iOp != PH7_OP_LOADC \|\| (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0` |
|     5 | 1356 | `	 \|\| aInstr[1].iOp != PH7_OP_DONE ){` |
|   ! 0 | 1357 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1358 | `		return PH7_OK;` |
|     - | 1359 | `	}` |
|     5 | 1360 | `	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);` |
|     5 | 1361 | `	if( pLit == 0 \|\| SyBlobLength(&pLit->sBlob) < 1 ){` |
|   ! 0 | 1362 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1363 | `		return PH7_OK;` |
|     - | 1364 | `	}` |
|     5 | 1365 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&pLit->sBlob), (int)SyBlobLength(&pLit->sBlob));` |
|     5 | 1366 | `	return PH7_OK;` |
|     4 | 1367 | `}` |
|     - | 1368 | `/*` |
|     - | 1369 | ` * mixed __reflect_invoke(mixed $target, ?string $method, ?object $this, array $args)` |
|     - | 1370 | ` * Visibility-bypassing invocation (methods dispatch by VM name; functions` |
|     - | 1371 | ` * and closures ride PH7_VmCallUserFunction like call_user_func_array).` |
|     - | 1372 | ` */` |
|    20 | 1373 | `static int vm_builtin_reflect_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1374 | `{` |
|    21 | 1375 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1376 | `	ph7_value sResult;` |
|     - | 1377 | `	SySet aCallArg;` |
|     - | 1378 | `	sxi32 rc;` |
|    21 | 1379 | `	if( nArg < 4 ){` |
|   ! 0 | 1380 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1381 | `		return PH7_OK;` |
|     - | 1382 | `	}` |
|    21 | 1383 | `	PH7_MemObjInit(pVm, &sResult);` |
|    21 | 1384 | `	sResult.nIdx = SXU32_HIGH;` |
|    21 | 1385 | `	SySetInit(&aCallArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    21 | 1386 | `	ReflectCollectArgs(pCtx, apArg[3], &aCallArg, 0);` |
|    21 | 1387 | `	if( (apArg[1]->iFlags & MEMOBJ_STRING) && SyBlobLength(&apArg[1]->sBlob) > 0 ){` |
|    11 | 1388 | `		ph7_class *pClass = 0;` |
|    11 | 1389 | `		ph7_class_method *pMeth = 0;` |
|    11 | 1390 | `		ph7_class_instance *pThis = 0;` |
|    11 | 1391 | `		ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, 0);` |
|    11 | 1392 | `		if( pMeth == 0 ){` |
|   ! 0 | 1393 | `			SySetRelease(&aCallArg);` |
|   ! 0 | 1394 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1395 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1396 | `			return PH7_OK;` |
|     - | 1397 | `		}` |
|    11 | 1398 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     7 | 1399 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     3 | 1400 | `		}` |
|     - | 1401 | `		/* Reflection ignores method visibility (PHP 8.1+); the flag is` |
|     - | 1402 | `		 * consumed by the first OP_CALL, i.e. this synthetic one. */` |
|    11 | 1403 | `		pVm->bReflectBypass = 1;` |
|    16 | 1404 | `		rc = PH7_VmCallClassMethod(pVm, pThis, pMeth, &sResult,` |
|    10 | 1405 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg));` |
|    11 | 1406 | `		pVm->bReflectBypass = 0;` |
|     6 | 1407 | `	}else{` |
|    16 | 1408 | `		rc = PH7_VmCallUserFunction(pVm, apArg[0],` |
|    10 | 1409 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg), &sResult);` |
|     - | 1410 | `	}` |
|    21 | 1411 | `	SySetRelease(&aCallArg);` |
|    21 | 1412 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 | 1413 | `		PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1414 | `		return rc;` |
|     - | 1415 | `	}` |
|    21 | 1416 | `	ph7_result_value(pCtx, &sResult);` |
|    21 | 1417 | `	PH7_MemObjRelease(&sResult);` |
|    21 | 1418 | `	return PH7_OK;` |
|    11 | 1419 | `}` |
|     - | 1420 | `/*` |
|     - | 1421 | ` * Closure __reflect_closure(mixed $target, ?string $method, ?object $this)` |
|     - | 1422 | ` * Mint a Closure for a function or method, bound and scoped like the` |
|     - | 1423 | ` * first-class-callable path.` |
|     - | 1424 | ` */` |
|     6 | 1425 | `static int vm_builtin_reflect_closure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1426 | `{` |
|     7 | 1427 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 1428 | `	ph7_class *pClass = 0;` |
|     7 | 1429 | `	ph7_class_method *pMeth = 0;` |
|     7 | 1430 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1431 | `	ph7_vm_func *pFunc;` |
|     7 | 1432 | `	if( nArg < 3 ){` |
|   ! 0 | 1433 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1434 | `		return PH7_OK;` |
|     - | 1435 | `	}` |
|     7 | 1436 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, &pClosure);` |
|     7 | 1437 | `	if( pClosure ){` |
|     - | 1438 | `		/* Already a Closure: hand the same instance back */` |
|   ! 0 | 1439 | `		return ReflectResultExistingObject(pCtx, pClosure);` |
|     - | 1440 | `	}` |
|     7 | 1441 | `	if( pMeth && pClass ){` |
|     5 | 1442 | `		ph7_class_instance *pThis = 0;` |
|     5 | 1443 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     3 | 1444 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     1 | 1445 | `		}` |
|     7 | 1446 | `		return ReflectResultObject(pCtx,` |
|     4 | 1447 | `			PH7_VmNewClosure(pVm, &pMeth->sFunc.sName, pThis, &pClass->sName));` |
|     - | 1448 | `	}` |
|     3 | 1449 | `	if( pFunc ){` |
|     3 | 1450 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &pFunc->sName, 0, 0));` |
|     - | 1451 | `	}` |
|     - | 1452 | `	/* Host function by name */` |
|   ! 0 | 1453 | `	if( apArg[0]->iFlags & MEMOBJ_STRING ){` |
|     - | 1454 | `		SyString sName;` |
|   ! 0 | 1455 | `		SyStringInitFromBuf(&sName, (const char *)SyBlobData(&apArg[0]->sBlob), SyBlobLength(&apArg[0]->sBlob));` |
|   ! 0 | 1456 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &sName, 0, 0));` |
|     - | 1457 | `	}` |
|   ! 0 | 1458 | `	ph7_result_null(pCtx);` |
|   ! 0 | 1459 | `	return PH7_OK;` |
|     4 | 1460 | `}` |
|     - | 1461 | `/*` |
|     - | 1462 | ` * Resolve a Generator object into its wrapper. Mirrors the static` |
|     - | 1463 | ` * VmGeneratorExtractCtx in vm.c: the $__ctx attribute carries the` |
|     - | 1464 | ` * ph7_generator pointer as a resource value.` |
|     - | 1465 | ` */` |
|    22 | 1466 | `static ph7_generator * ReflectGeneratorCtx(ph7_vm *pVm, ph7_value *pVal)` |
|     1 | 1467 | `{` |
|     - | 1468 | `	ph7_class_instance *pThis;` |
|     - | 1469 | `	ph7_value *pAttr;` |
|     - | 1470 | `	SyString sAttr;` |
|    23 | 1471 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVm->pGeneratorClass == 0 ){` |
|   ! 0 | 1472 | `		return 0;` |
|     - | 1473 | `	}` |
|    23 | 1474 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    23 | 1475 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|   ! 0 | 1476 | `		return 0;` |
|     - | 1477 | `	}` |
|    23 | 1478 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    23 | 1479 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    23 | 1480 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|   ! 0 | 1481 | `		return 0;` |
|     - | 1482 | `	}` |
|    23 | 1483 | `	return (ph7_generator *)pAttr->x.pOther;` |
|    12 | 1484 | `}` |
|     - | 1485 | `/*` |
|     - | 1486 | ` * array\|null __reflect_gen_info(Generator $g)` |
|     - | 1487 | ` * {state, closed, executing, kind ('fn'\|'method'), name, class?, this}` |
|     - | 1488 | ` */` |
|    16 | 1489 | `static int vm_builtin_reflect_gen_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1490 | `{` |
|    17 | 1491 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1492 | `	ph7_generator *pGen;` |
|     - | 1493 | `	ph7_exec_ctx *pExec;` |
|     - | 1494 | `	ph7_value *pInfo;` |
|    17 | 1495 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 \|\| pGen->pCtx == 0 ){` |
|   ! 0 | 1496 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1497 | `		return PH7_OK;` |
|     - | 1498 | `	}` |
|    17 | 1499 | `	pExec = pGen->pCtx;` |
|    17 | 1500 | `	pInfo = ph7_context_new_array(pCtx);` |
|    17 | 1501 | `	if( pInfo == 0 ){` |
|   ! 0 | 1502 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1503 | `		return PH7_OK;` |
|     - | 1504 | `	}` |
|    17 | 1505 | `	ReflectMapAddInt(pCtx, pInfo, "state", (sxi64)pExec->iState);` |
|    24 | 1506 | `	ReflectMapAddBool(pCtx, pInfo, "closed",` |
|    16 | 1507 | `		pExec->iState == PH7_CTX_STATE_COMPLETED \|\| pExec->iState == PH7_CTX_STATE_CLOSED);` |
|    17 | 1508 | `	ReflectMapAddBool(pCtx, pInfo, "executing", pVm->pActiveCtx == pExec);` |
|    17 | 1509 | `	if( pExec->pFunc ){` |
|    17 | 1510 | `		ph7_vm_func *pFunc = pExec->pFunc;` |
|    19 | 1511 | `		if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     5 | 1512 | `			ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|     5 | 1513 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "method", sizeof("method")-1);` |
|     5 | 1514 | `			ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|     3 | 1515 | `		}else{` |
|    13 | 1516 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "fn", sizeof("fn")-1);` |
|     - | 1517 | `		}` |
|    17 | 1518 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|     8 | 1519 | `	}` |
|     - | 1520 | `	{` |
|     - | 1521 | `		/* The coroutine frame installs $this as a frame VARIABLE (see` |
|     - | 1522 | `		 * VmFiberSetupFrame), not as pFrame->pThis — check both. */` |
|    17 | 1523 | `		ph7_value *pThisVal = 0;` |
|    17 | 1524 | `		if( pExec->pFrame ){` |
|    17 | 1525 | `			SyHashEntry *pVar = SyHashGet(&pExec->pFrame->hVar, "this", sizeof("this")-1);` |
|    17 | 1526 | `			if( pVar ){` |
|     5 | 1527 | `				ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj, (sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|     5 | 1528 | `				if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|     5 | 1529 | `					pThisVal = pSlot;` |
|     2 | 1530 | `				}` |
|     2 | 1531 | `			}` |
|    17 | 1532 | `			if( pThisVal == 0 && pExec->pFrame->pThis ){` |
|     - | 1533 | `				ph7_value sThis;` |
|   ! 0 | 1534 | `				ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 1535 | `				PH7_MemObjInit(pVm, &sThis);` |
|   ! 0 | 1536 | `				pExec->pFrame->pThis->iRef++;` |
|   ! 0 | 1537 | `				sThis.x.pOther = pExec->pFrame->pThis;` |
|   ! 0 | 1538 | `				MemObjSetType(&sThis, MEMOBJ_OBJ);` |
|   ! 0 | 1539 | `				if( pKey ){` |
|   ! 0 | 1540 | `					ph7_value_string(pKey, "this", 4);` |
|   ! 0 | 1541 | `					ph7_array_add_elem(pInfo, pKey, &sThis); /* copies (takes its own ref) */` |
|   ! 0 | 1542 | `				}` |
|   ! 0 | 1543 | `				PH7_MemObjRelease(&sThis);` |
|   ! 0 | 1544 | `				pThisVal = (ph7_value *)1; /* handled */` |
|   ! 0 | 1545 | `			}` |
|     8 | 1546 | `		}` |
|    17 | 1547 | `		if( pThisVal == 0 ){` |
|    13 | 1548 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|    11 | 1549 | `		}else if( pThisVal != (ph7_value *)1 ){` |
|     5 | 1550 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|     5 | 1551 | `			if( pKey ){` |
|     5 | 1552 | `				ph7_value_string(pKey, "this", 4);` |
|     5 | 1553 | `				ph7_array_add_elem(pInfo, pKey, pThisVal);` |
|     2 | 1554 | `			}` |
|     2 | 1555 | `		}` |
|     - | 1556 | `	}` |
|    17 | 1557 | `	ph7_result_value(pCtx, pInfo);` |
|    17 | 1558 | `	return PH7_OK;` |
|     9 | 1559 | `}` |
|     - | 1560 | `/*` |
|     - | 1561 | ` * Generator __reflect_gen_exec(Generator $g)` |
|     - | 1562 | `` * Follow `yield from` delegation to the innermost executing generator`` |
|     - | 1563 | ` * (PHP's ReflectionGenerator::getExecutingGenerator).` |
|     - | 1564 | ` */` |
|     4 | 1565 | `static int vm_builtin_reflect_gen_exec(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1566 | `{` |
|     5 | 1567 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1568 | `	ph7_generator *pGen;` |
|     - | 1569 | `	ph7_value *pCur;` |
|     5 | 1570 | `	int iDepth = 0;` |
|     5 | 1571 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 ){` |
|   ! 0 | 1572 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1573 | `		return PH7_OK;` |
|     - | 1574 | `	}` |
|     5 | 1575 | `	pCur = apArg[0];` |
|     9 | 1576 | `	while( pGen && pGen->pCtx && pGen->pCtx->iDelegateState == 3` |
|     8 | 1577 | `	 && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|     3 | 1578 | `		ph7_generator *pInner = ReflectGeneratorCtx(pVm, &pGen->pCtx->sDelegate);` |
|     3 | 1579 | `		if( pInner == 0 ){` |
|   ! 0 | 1580 | `			break;` |
|     - | 1581 | `		}` |
|     3 | 1582 | `		pCur = &pGen->pCtx->sDelegate;` |
|     3 | 1583 | `		pGen = pInner;` |
|     3 | 1584 | `		iDepth++;` |
|     1 | 1585 | `	}` |
|     5 | 1586 | `	return ReflectResultExistingObject(pCtx, (ph7_class_instance *)pCur->x.pOther);` |
|     3 | 1587 | `}` |
|     - | 1588 | `/*` |
|     - | 1589 | ` * array\|null __reflect_const_info(string $name)` |
|     - | 1590 | ` * Global-constant descriptor: {value}. Null when undefined. File/origin` |
|     - | 1591 | ` * metadata arrives with the C5 constant-metadata work.` |
|     - | 1592 | ` */` |
|    40 | 1593 | `static int vm_builtin_reflect_const_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1594 | `{` |
|    41 | 1595 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1596 | `	SyHashEntry *pEntry;` |
|     - | 1597 | `	ph7_constant *pCons;` |
|     - | 1598 | `	ph7_value *pInfo;` |
|     - | 1599 | `	ph7_value sValue;` |
|     - | 1600 | `	const char *zName;` |
|     - | 1601 | `	int nLen;` |
|    41 | 1602 | `	if( nArg < 1 ){` |
|   ! 0 | 1603 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1604 | `		return PH7_OK;` |
|     - | 1605 | `	}` |
|    41 | 1606 | `	zName = ph7_value_to_string(apArg[0], &nLen);` |
|    41 | 1607 | `	pEntry = nLen > 0 ? SyHashGet(&pVm->hConstant, (const void *)zName, (sxu32)nLen) : 0;` |
|    41 | 1608 | `	if( pEntry == 0 ){` |
|     3 | 1609 | `		ph7_result_null(pCtx);` |
|     3 | 1610 | `		return PH7_OK;` |
|     - | 1611 | `	}` |
|    39 | 1612 | `	pCons = (ph7_constant *)pEntry->pUserData;` |
|    39 | 1613 | `	pInfo = ph7_context_new_array(pCtx);` |
|    39 | 1614 | `	if( pInfo == 0 ){` |
|   ! 0 | 1615 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1616 | `		return PH7_OK;` |
|     - | 1617 | `	}` |
|    39 | 1618 | `	PH7_MemObjInit(pVm, &sValue);` |
|    39 | 1619 | `	if( pCons->xExpand ){` |
|    39 | 1620 | `		pCons->xExpand(&sValue, pCons->pUserData);` |
|    19 | 1621 | `	}` |
|     - | 1622 | `	{` |
|    39 | 1623 | `		ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|    39 | 1624 | `		if( pKey ){` |
|    39 | 1625 | `			ph7_value_string(pKey, "value", 5);` |
|    39 | 1626 | `			ph7_array_add_elem(pInfo, pKey, &sValue);` |
|    19 | 1627 | `		}` |
|     - | 1628 | `	}` |
|    39 | 1629 | `	PH7_MemObjRelease(&sValue);` |
|    39 | 1630 | `	ReflectMapAddBool(pCtx, pInfo, "internal", pCons->bUserDefined == 0);` |
|    39 | 1631 | `	if( SyStringLength(&pCons->sFile) > 0 ){` |
|    25 | 1632 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));` |
|    13 | 1633 | `	}else{` |
|    15 | 1634 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1635 | `	}` |
|    39 | 1636 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pCons->nLine);` |
|    39 | 1637 | `	ReflectMapAddAttrs(pCtx, pInfo, &pCons->aAttrs);` |
|    39 | 1638 | `	ph7_result_value(pCtx, pInfo);` |
|    39 | 1639 | `	return PH7_OK;` |
|    21 | 1640 | `}` |
|     - | 1641 | `/*` |
|     - | 1642 | ` * int\|null __reflect_ref_id(array $arr, int\|string $key)` |
|     - | 1643 | ` * The element's slot index when the element is a reference (its slot has` |
|     - | 1644 | ` * a reference-table record with at least two links), null otherwise.` |
|     - | 1645 | ` */` |
|     6 | 1646 | `static int vm_builtin_reflect_ref_id(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1647 | `{` |
|     - | 1648 | `	ph7_hashmap *pMap;` |
|     7 | 1649 | `	ph7_hashmap_node *pNode = 0;` |
|     7 | 1650 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|   ! 0 | 1651 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1652 | `		return PH7_OK;` |
|     - | 1653 | `	}` |
|     7 | 1654 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 | 1655 | `	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK \|\| pNode == 0 ){` |
|   ! 0 | 1656 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1657 | `		return PH7_OK;` |
|     - | 1658 | `	}` |
|     7 | 1659 | `	if( PH7_VmSlotRefCount(pCtx->pVm, pNode->nValIdx) < 2 ){` |
|     3 | 1660 | `		ph7_result_null(pCtx);` |
|     3 | 1661 | `		return PH7_OK;` |
|     - | 1662 | `	}` |
|     5 | 1663 | `	ph7_result_int64(pCtx, (sxi64)pNode->nValIdx);` |
|     5 | 1664 | `	return PH7_OK;` |
|     4 | 1665 | `}` |
|     - | 1666 | `/*` |
|     - | 1667 | ` * array\|null __reflect_attr_args(string $kind, mixed $target, ?string $member,` |
|     - | 1668 | ` *                                int $paramIdx, int $attrIdx)` |
|     - | 1669 | ` * Evaluate the recorded argument expressions of one declared attribute:` |
|     - | 1670 | ` * kind 'class' (target = class), 'attr' (class + property/constant name),` |
|     - | 1671 | ` * 'method' (class + method), 'fn' (function name or Closure), 'param'` |
|     - | 1672 | ` * (function spec + parameter index). Named arguments become string keys.` |
|     - | 1673 | ` */` |
|    68 | 1674 | `static int vm_builtin_reflect_attr_args(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1675 | `{` |
|    69 | 1676 | `	ph7_vm *pVm = pCtx->pVm;` |
|    69 | 1677 | `	SySet *pAttrs = 0;` |
|    69 | 1678 | `	ph7_class *pDeclCls = 0; /* class an attribute is declared on: scope for self:: in its args */` |
|     - | 1679 | `	ph7_attribute *pAttrRec;` |
|     - | 1680 | `	ph7_value *pOut;` |
|     - | 1681 | `	const char *zKind;` |
|     - | 1682 | `	int nKind;` |
|     - | 1683 | `	sxu32 nAttrIdx, n;` |
|    69 | 1684 | `	if( nArg < 5 ){` |
|   ! 0 | 1685 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1686 | `		return PH7_OK;` |
|     - | 1687 | `	}` |
|    69 | 1688 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|    69 | 1689 | `	nAttrIdx = (sxu32)ph7_value_to_int(apArg[4]);` |
|    91 | 1690 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|    45 | 1691 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|    45 | 1692 | `		if( pClass ){ pAttrs = &pClass->aAttrs; pDeclCls = pClass; }` |
|    49 | 1693 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|     5 | 1694 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|     5 | 1695 | `		ph7_class_attr *pMember = pClass ? ReflectFetchAttr(pClass, apArg[2]) : 0;` |
|     5 | 1696 | `		if( pMember ){ pAttrs = &pMember->aAttrs; pDeclCls = pClass; }` |
|    27 | 1697 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|     9 | 1698 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     9 | 1699 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|    21 | 1700 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|     9 | 1701 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], 0, 0, 0, 0, 0);` |
|     9 | 1702 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|    10 | 1703 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|     3 | 1704 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1705 | `		ph7_vm_func_arg *pParam = pFunc` |
|     2 | 1706 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|     3 | 1707 | `		if( pParam ){ pAttrs = &pParam->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|     4 | 1708 | `	}else if( nKind == 5 && SyMemcmp(zKind, "const", 5) == 0 ){` |
|     - | 1709 | ``		/* Global constant (php 8.5 attributes on `const` statements) */`` |
|     - | 1710 | `		const char *zCName;` |
|     - | 1711 | `		int nCName;` |
|     - | 1712 | `		SyHashEntry *pCEntry;` |
|     3 | 1713 | `		zCName = ph7_value_to_string(apArg[1], &nCName);` |
|     3 | 1714 | `		pCEntry = nCName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zCName, (sxu32)nCName) : 0;` |
|     3 | 1715 | `		if( pCEntry ){ pAttrs = &((ph7_constant *)pCEntry->pUserData)->aAttrs; }` |
|     1 | 1716 | `	}` |
|    68 | 1717 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|    69 | 1718 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 | 1719 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1720 | `		return PH7_OK;` |
|     - | 1721 | `	}` |
|   123 | 1722 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|    55 | 1723 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|     - | 1724 | `		ph7_value sValue;` |
|    55 | 1725 | `		PH7_MemObjInit(pVm, &sValue);` |
|    55 | 1726 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|     - | 1727 | `` 			/* Evaluate under the attribute's declaring-class scope so `self::class` `` |
|     - | 1728 | ``			 * / `self::CONST` / `parent::` resolve like php (else self:: would bind`` |
|     - | 1729 | `			 * to the reflection machinery's own class). */` |
|    55 | 1730 | `			PH7_VmExecAttrArg(pVm, &pArgRec->aByteCode, pDeclCls, &sValue);` |
|    27 | 1731 | `		}` |
|    55 | 1732 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|     5 | 1733 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|     3 | 1734 | `		}else{` |
|    51 | 1735 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|     - | 1736 | `		}` |
|    55 | 1737 | `		PH7_MemObjRelease(&sValue);` |
|    28 | 1738 | `	}` |
|    69 | 1739 | `	ph7_result_value(pCtx, pOut);` |
|    69 | 1740 | `	return PH7_OK;` |
|    35 | 1741 | `}` |
|     - | 1742 | `/*` |
|     - | 1743 | ` * The Reflection classes, in PHP. Chunk 1: exceptions, Reflector,` |
|     - | 1744 | ` * Reflection, ReflectionClass, ReflectionObject (plus get_debug_type,` |
|     - | 1745 | ` * which the TypeError messages need and PHP 8.0 ships natively).` |
|     - | 1746 | ` */` |
|     - | 1747 | `static const char zReflectLib1[] =` |
|     - | 1748 | `/* Per-class memoization of the (expensive) C class descriptor. Every Reflection*` |
|     - | 1749 | ` * accessor funnels through __rinfo()/the constructors, so without this a single` |
|     - | 1750 | ` * test run rebuilds the full descriptor of the same class hundreds of times` |
|     - | 1751 | ` * (real php caches the reflected class). Keyed by resolved class name; only` |
|     - | 1752 | ` * successful lookups are cached, so a not-yet-autoloaded class is re-queried.` |
|     - | 1753 | ` * The C builtin __reflect_class_info stays the real worker. */` |
|     - | 1754 | `"function __phl_rcinfo($oc){"` |
|     - | 1755 | `" static $c = array();"` |
|     - | 1756 | `" $k = is_object($oc) ? get_class($oc) : (string)$oc;"` |
|     - | 1757 | `" if( isset($c[$k]) ){ return $c[$k]; }"` |
|     - | 1758 | `" $info = __reflect_class_info($oc);"` |
|     - | 1759 | `" if( $info !== null ){ $c[$k] = $info; }"` |
|     - | 1760 | `" return $info;"` |
|     - | 1761 | `"}"` |
|     - | 1762 | `"function get_debug_type($value){"` |
|     - | 1763 | `" if(is_object($value)){ return get_class($value); }"` |
|     - | 1764 | `" if(is_bool($value)){ return 'bool'; }"` |
|     - | 1765 | `" if(is_int($value)){ return 'int'; }"` |
|     - | 1766 | `" if(is_float($value)){ return 'float'; }"` |
|     - | 1767 | `" if(is_string($value)){ return 'string'; }"` |
|     - | 1768 | `" if(is_array($value)){ return 'array'; }"` |
|     - | 1769 | `" if($value === null){ return 'null'; }"` |
|     - | 1770 | `" return gettype($value);"` |
|     - | 1771 | `"}"` |
|     - | 1772 | `"interface Reflector extends Stringable {}"` |
|     - | 1773 | `"class ReflectionException extends Exception {}"` |
|     - | 1774 | `"class Reflection {"` |
|     - | 1775 | `" public static function getModifierNames($modifiers){"` |
|     - | 1776 | `"  $names = array();"` |
|     - | 1777 | `"  if($modifiers & 64){ $names[] = 'abstract'; }"` |
|     - | 1778 | `"  if($modifiers & 32){ $names[] = 'final'; }"` |
|     - | 1779 | `"  if($modifiers & 1){ $names[] = 'public'; }"` |
|     - | 1780 | `"  if($modifiers & 2){ $names[] = 'protected'; }"` |
|     - | 1781 | `"  if($modifiers & 4){ $names[] = 'private'; }"` |
|     - | 1782 | `"  if($modifiers & 16){ $names[] = 'static'; }"` |
|     - | 1783 | `"  if($modifiers & 128){ $names[] = 'readonly'; }"` |
|     - | 1784 | `"  return $names;"` |
|     - | 1785 | `" }"` |
|     - | 1786 | `"}"` |
|     - | 1787 | `"class ReflectionClass implements Reflector {"` |
|     - | 1788 | `" const IS_IMPLICIT_ABSTRACT = 16;"` |
|     - | 1789 | `" const IS_EXPLICIT_ABSTRACT = 64;"` |
|     - | 1790 | `" const IS_FINAL = 32;"` |
|     - | 1791 | `" const IS_READONLY = 65536;"` |
|     - | 1792 | `" const SKIP_INITIALIZATION_ON_SERIALIZE = 8;"` |
|     - | 1793 | `" const SKIP_DESTRUCTOR = 16;"` |
|     - | 1794 | `" public $name;"` |
|     - | 1795 | `" protected $__obj = null;"` |
|     - | 1796 | `" public function __construct($objectOrClass){"` |
|     - | 1797 | `"  if(!is_object($objectOrClass) && !is_string($objectOrClass)){"` |
|     - | 1798 | `"   if(is_int($objectOrClass) \|\| is_float($objectOrClass) \|\| is_bool($objectOrClass)){"` |
|     - | 1799 | `"    $objectOrClass = (string)$objectOrClass;"` |
|     - | 1800 | `"   }else{"` |
|     - | 1801 | `"    throw new TypeError('ReflectionClass::__construct(): Argument #1 ($objectOrClass) must be of type object\|string, '.get_debug_type($objectOrClass).' given');"` |
|     - | 1802 | `"   }"` |
|     - | 1803 | `"  }"` |
|     - | 1804 | `"  $info = __phl_rcinfo($objectOrClass);"` |
|     - | 1805 | `"  if($info === null){"` |
|     - | 1806 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 1807 | `"  }"` |
|     - | 1808 | `"  $this->name = $info['name'];"` |
|     - | 1809 | `" }"` |
|     - | 1810 | `" protected function __rinfo(){ return __phl_rcinfo($this->name); }"` |
|     - | 1811 | `" public function getName(){ return $this->name; }"` |
|     - | 1812 | `" public function getShortName(){"` |
|     - | 1813 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1814 | `"  if($p === false){ return $this->name; }"` |
|     - | 1815 | `"  return substr($this->name,$p+1);"` |
|     - | 1816 | `" }"` |
|     - | 1817 | `" public function getNamespaceName(){"` |
|     - | 1818 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1819 | `"  if($p === false){ return ''; }"` |
|     - | 1820 | `"  return substr($this->name,0,$p);"` |
|     - | 1821 | `" }"` |
|     - | 1822 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 1823 | `" public function isInternal(){ $i = $this->__rinfo(); return $i['internal']; }"` |
|     - | 1824 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 1825 | `" public function isInterface(){ $i = $this->__rinfo(); return $i['interface']; }"` |
|     - | 1826 | `" public function isTrait(){ $i = $this->__rinfo(); return $i['trait']; }"` |
|     - | 1827 | `" public function isAbstract(){ $i = $this->__rinfo(); return $i['abstract']; }"` |
|     - | 1828 | `" public function isFinal(){ $i = $this->__rinfo(); return $i['final']; }"` |
|     - | 1829 | `" public function isReadOnly(){ $i = $this->__rinfo(); return $i['readonly']; }"` |
|     - | 1830 | `" public function isEnum(){ $i = $this->__rinfo(); return $i['enum']; }"` |
|     - | 1831 | `" public function isAnonymous(){ return strpos($this->name,'class@anonymous') === 0; }"` |
|     - | 1832 | `" public function getModifiers(){"` |
|     - | 1833 | `"  $i = $this->__rinfo();"` |
|     - | 1834 | `"  $m = 0;"` |
|     - | 1835 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 1836 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 1837 | `"  if($i['readonly']){ $m \|= 65536; }"` |
|     - | 1838 | `"  return $m;"` |
|     - | 1839 | `" }"` |
|     - | 1840 | `" public function getParentClass(){"` |
|     - | 1841 | `"  $i = $this->__rinfo();"` |
|     - | 1842 | `"  if($i['parent'] === null){ return false; }"` |
|     - | 1843 | `"  return new ReflectionClass($i['parent']);"` |
|     - | 1844 | `" }"` |
|     - | 1845 | `" public function getInterfaceNames(){ $i = $this->__rinfo(); return $i['interfaces']; }"` |
|     - | 1846 | `" public function getInterfaces(){"` |
|     - | 1847 | `"  $i = $this->__rinfo();"` |
|     - | 1848 | `"  $out = array();"` |
|     - | 1849 | `"  foreach($i['interfaces'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1850 | `"  return $out;"` |
|     - | 1851 | `" }"` |
|     - | 1852 | `" public function getTraitNames(){ $i = $this->__rinfo(); return $i['traits']; }"` |
|     - | 1853 | `" public function getTraits(){"` |
|     - | 1854 | `"  $i = $this->__rinfo();"` |
|     - | 1855 | `"  $out = array();"` |
|     - | 1856 | `"  foreach($i['traits'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1857 | `"  return $out;"` |
|     - | 1858 | `" }"` |
|     - | 1859 | `" public function getTraitAliases(){ return array(); }"` |
|     - | 1860 | `" public function implementsInterface($interface){"` |
|     - | 1861 | `"  if($interface instanceof ReflectionClass){ $interface = $interface->name; }"` |
|     - | 1862 | `"  $target = __phl_rcinfo($interface);"` |
|     - | 1863 | `"  if($target === null){"` |
|     - | 1864 | `"   throw new ReflectionException('Interface \"'.$interface.'\" does not exist');"` |
|     - | 1865 | `"  }"` |
|     - | 1866 | `"  if(!$target['interface']){"` |
|     - | 1867 | `"   throw new ReflectionException($target['name'].' is not an interface');"` |
|     - | 1868 | `"  }"` |
|     - | 1869 | `"  $name = $target['name'];"` |
|     - | 1870 | `"  if($this->name === $name){ return true; }"` |
|     - | 1871 | `"  $i = $this->__rinfo();"` |
|     - | 1872 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1873 | `"  return false;"` |
|     - | 1874 | `" }"` |
|     - | 1875 | `" public function isSubclassOf($class){"` |
|     - | 1876 | `"  if($class instanceof ReflectionClass){ $class = $class->name; }"` |
|     - | 1877 | `"  $target = __phl_rcinfo($class);"` |
|     - | 1878 | `"  if($target === null){"` |
|     - | 1879 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 1880 | `"  }"` |
|     - | 1881 | `"  $name = $target['name'];"` |
|     - | 1882 | `"  if($name === $this->name){ return false; }"` |
|     - | 1883 | `"  $i = $this->__rinfo();"` |
|     - | 1884 | `"  $p = $i['parent'];"` |
|     - | 1885 | `"  while($p !== null){"` |
|     - | 1886 | `"   if($p === $name){ return true; }"` |
|     - | 1887 | `"   $pi = __phl_rcinfo($p);"` |
|     - | 1888 | `"   $p = $pi['parent'];"` |
|     - | 1889 | `"  }"` |
|     - | 1890 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1891 | `"  return false;"` |
|     - | 1892 | `" }"` |
|     - | 1893 | `" public function isInstance($object){"` |
|     - | 1894 | `"  if(!is_object($object)){"` |
|     - | 1895 | `"   throw new TypeError('ReflectionClass::isInstance(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 1896 | `"  }"` |
|     - | 1897 | `"  return is_a($object,$this->name);"` |
|     - | 1898 | `" }"` |
|     - | 1899 | `" public function hasMethod($name){"` |
|     - | 1900 | `"  $i = $this->__rinfo();"` |
|     - | 1901 | `"  $l = strtolower($name);"` |
|     - | 1902 | `"  foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ return true; } }"` |
|     - | 1903 | `"  return false;"` |
|     - | 1904 | `" }"` |
|     - | 1905 | `" public function hasProperty($name){"` |
|     - | 1906 | `"  $i = $this->__rinfo();"` |
|     - | 1907 | `"  if(isset($i['props'][$name])){ return true; }"` |
|     - | 1908 | `"  if($this->__obj !== null){ return (__reflect_prop_state($this->__obj, $name) & 1) !== 0; }"` |
|     - | 1909 | `"  return false;"` |
|     - | 1910 | `" }"` |
|     - | 1911 | `" public function hasConstant($name){ $i = $this->__rinfo(); return isset($i['consts'][$name]); }"` |
|     - | 1912 | `" public function getConstant($name){"` |
|     - | 1913 | `"  $i = $this->__rinfo();"` |
|     - | 1914 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 1915 | `"  return __reflect_const_value($this->name,$name);"` |
|     - | 1916 | `" }"` |
|     - | 1917 | `" public function getConstants($filter = null){"` |
|     - | 1918 | `"  $i = $this->__rinfo();"` |
|     - | 1919 | `"  $out = array();"` |
|     - | 1920 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 1921 | `"   if($filter !== null){"` |
|     - | 1922 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 1923 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 1924 | `"   }"` |
|     - | 1925 | `"   $out[$k] = __reflect_const_value($this->name,$k);"` |
|     - | 1926 | `"  }"` |
|     - | 1927 | `"  return $out;"` |
|     - | 1928 | `" }"` |
|     - | 1929 | `" public function getStartLine(){"` |
|     - | 1930 | `"  $i = $this->__rinfo();"` |
|     - | 1931 | `"  if($i['internal']){ return false; }"` |
|     - | 1932 | `"  return $i['line'];"` |
|     - | 1933 | `" }"` |
|     - | 1934 | `" public function getEndLine(){"` |
|     - | 1935 | `"  $i = $this->__rinfo();"` |
|     - | 1936 | `"  if($i['internal']){ return false; }"` |
|     - | 1937 | `"  return $i['endline'];"` |
|     - | 1938 | `" }"` |
|     - | 1939 | `" public function getFileName(){ $i = $this->__rinfo(); return $i['file']; }"` |
|     - | 1940 | `" public function getDocComment(){ $i = $this->__rinfo(); return $i['doc']; }"` |
|     - | 1941 | `" public function isInstantiable(){"` |
|     - | 1942 | `"  $i = $this->__rinfo();"` |
|     - | 1943 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract'] \|\| $i['enum']){ return false; }"` |
|     - | 1944 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){ return false; }"` |
|     - | 1945 | `"  return true;"` |
|     - | 1946 | `" }"` |
|     - | 1947 | `" public function isCloneable(){"` |
|     - | 1948 | `"  $i = $this->__rinfo();"` |
|     - | 1949 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1950 | `"  if($i['clonevis'] !== 0 && $i['clonevis'] !== 1){ return false; }"` |
|     - | 1951 | `"  return true;"` |
|     - | 1952 | `" }"` |
|     - | 1953 | `" public function isIterable(){"` |
|     - | 1954 | `"  $i = $this->__rinfo();"` |
|     - | 1955 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1956 | `"  return $i['iterable'];"` |
|     - | 1957 | `" }"` |
|     - | 1958 | `" public function isIterateable(){ return $this->isIterable(); }"` |
|     - | 1959 | `" public function newInstance(...$args){ return $this->__rnew($args); }"` |
|     - | 1960 | `" public function newInstanceArgs(array $args = array()){ return $this->__rnew($args); }"` |
|     - | 1961 | `" protected function __rnew($args){"` |
|     - | 1962 | `"  $i = $this->__rinfo();"` |
|     - | 1963 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1964 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){"` |
|     - | 1965 | `"   throw new ReflectionException('Access to non-public constructor of class '.$this->name);"` |
|     - | 1966 | `"  }"` |
|     - | 1967 | `"  if($i['ctorvis'] === 0 && count($args) > 0){"` |
|     - | 1968 | `"   throw new ReflectionException('Class '.$this->name.' does not have a constructor, so you cannot pass any constructor arguments');"` |
|     - | 1969 | `"  }"` |
|     - | 1970 | `"  return __reflect_new_instance($this->name,$args);"` |
|     - | 1971 | `" }"` |
|     - | 1972 | `" protected function __rcheckInstantiable($i){"` |
|     - | 1973 | `"  if($i['interface']){ throw new Error('Cannot instantiate interface '.$this->name); }"` |
|     - | 1974 | `"  if($i['trait']){ throw new Error('Cannot instantiate trait '.$this->name); }"` |
|     - | 1975 | `"  if($i['abstract']){ throw new Error('Cannot instantiate abstract class '.$this->name); }"` |
|     - | 1976 | `" }"` |
|     - | 1977 | `" public function newInstanceWithoutConstructor(){"` |
|     - | 1978 | `"  $i = $this->__rinfo();"` |
|     - | 1979 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1980 | `"  return __reflect_new_no_ctor($this->name);"` |
|     - | 1981 | `" }"` |
|     - | 1982 | `" public function getStaticProperties(){"` |
|     - | 1983 | `"  $i = $this->__rinfo();"` |
|     - | 1984 | `"  $out = array();"` |
|     - | 1985 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1986 | `"   if($p['static']){ $out[$k] = __reflect_static_value($this->name,$k); }"` |
|     - | 1987 | `"  }"` |
|     - | 1988 | `"  return $out;"` |
|     - | 1989 | `" }"` |
|     - | 1990 | `" public function getStaticPropertyValue($name, ...$def){"` |
|     - | 1991 | `"  $i = $this->__rinfo();"` |
|     - | 1992 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 1993 | `"   if(count($def) > 0){ return $def[0]; }"` |
|     - | 1994 | `"   throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 1995 | `"  }"` |
|     - | 1996 | `"  return __reflect_static_value($this->name,$name);"` |
|     - | 1997 | `" }"` |
|     - | 1998 | `" public function setStaticPropertyValue($name,$value){"` |
|     - | 1999 | `"  $i = $this->__rinfo();"` |
|     - | 2000 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 2001 | `"   throw new ReflectionException('Class '.$this->name.' does not have a property named '.$name);"` |
|     - | 2002 | `"  }"` |
|     - | 2003 | `"  __reflect_static_set($this->name,$name,$value);"` |
|     - | 2004 | `" }"` |
|     - | 2005 | `" public function getDefaultProperties(){"` |
|     - | 2006 | `"  $i = $this->__rinfo();"` |
|     - | 2007 | `"  $out = array();"` |
|     - | 2008 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 2009 | `"   if($p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 2010 | `"  }"` |
|     - | 2011 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 2012 | `"   if(!$p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 2013 | `"  }"` |
|     - | 2014 | `"  return $out;"` |
|     - | 2015 | `" }"` |
|     - | 2016 | `" public function getProperty($name){"` |
|     - | 2017 | `"  $i = $this->__rinfo();"` |
|     - | 2018 | `"  if(isset($i['props'][$name])){"` |
|     - | 2019 | `"   return new ReflectionProperty($this->name, $name);"` |
|     - | 2020 | `"  }"` |
|     - | 2021 | `"  if($this->__obj !== null && (__reflect_prop_state($this->__obj, $name) & 1)){"` |
|     - | 2022 | `"   return new ReflectionProperty($this->__obj, $name);"` |
|     - | 2023 | `"  }"` |
|     - | 2024 | `"  throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 2025 | `" }"` |
|     - | 2026 | `" public function getProperties($filter = null){"` |
|     - | 2027 | `"  $i = $this->__rinfo();"` |
|     - | 2028 | `"  $out = array();"` |
|     - | 2029 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 2030 | `"   if($filter !== null){"` |
|     - | 2031 | `"    $m = ($p['vis'] === 1 ? 1 : ($p['vis'] === 2 ? 2 : 4));"` |
|     - | 2032 | `"    if($p['static']){ $m \|= 16; }"` |
|     - | 2033 | `"    if($p['readonly']){ $m \|= 128; }"` |
|     - | 2034 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 2035 | `"   }"` |
|     - | 2036 | `"   $out[] = new ReflectionProperty($this->name, $k);"` |
|     - | 2037 | `"  }"` |
|     - | 2038 | `"  if($this->__obj !== null){"` |
|     - | 2039 | `"   foreach(__reflect_dyn_props($this->__obj) as $k){"` |
|     - | 2040 | `"    if(isset($i['props'][$k])){ continue; }"` |
|     - | 2041 | `"    if($filter !== null && ($filter & 1) === 0){ continue; }"` |
|     - | 2042 | `"    $out[] = new ReflectionProperty($this->__obj, $k);"` |
|     - | 2043 | `"   }"` |
|     - | 2044 | `"  }"` |
|     - | 2045 | `"  return $out;"` |
|     - | 2046 | `" }"` |
|     - | 2047 | `" public function getMethod($name){"` |
|     - | 2048 | `"  $i = $this->__rinfo();"` |
|     - | 2049 | `"  $found = null;"` |
|     - | 2050 | `"  if(isset($i['methods'][$name])){"` |
|     - | 2051 | `"   $found = $name;"` |
|     - | 2052 | `"  }else{"` |
|     - | 2053 | `"   $l = strtolower($name);"` |
|     - | 2054 | `"   foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ $found = $k; break; } }"` |
|     - | 2055 | `"  }"` |
|     - | 2056 | `"  if($found === null){"` |
|     - | 2057 | `"   throw new ReflectionException('Method '.$this->name.'::'.$name.'() does not exist');"` |
|     - | 2058 | `"  }"` |
|     - | 2059 | `"  return new ReflectionMethod($this->name, $found);"` |
|     - | 2060 | `" }"` |
|     - | 2061 | `" public function getMethods($filter = null){"` |
|     - | 2062 | `"  $i = $this->__rinfo();"` |
|     - | 2063 | `"  $out = array();"` |
|     - | 2064 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 2065 | `"   if($filter !== null){"` |
|     - | 2066 | `"    $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2067 | `"    if($m['static']){ $mod \|= 16; }"` |
|     - | 2068 | `"    if($m['abstract']){ $mod \|= 64; }"` |
|     - | 2069 | `"    if($m['final']){ $mod \|= 32; }"` |
|     - | 2070 | `"    if(($mod & $filter) === 0){ continue; }"` |
|     - | 2071 | `"   }"` |
|     - | 2072 | `"   $out[] = new ReflectionMethod($this->name, $k);"` |
|     - | 2073 | `"  }"` |
|     - | 2074 | `"  return $out;"` |
|     - | 2075 | `" }"` |
|     - | 2076 | `" public function getConstructor(){"` |
|     - | 2077 | `"  $i = $this->__rinfo();"` |
|     - | 2078 | `"  if(isset($i['methods']['__construct'])){"` |
|     - | 2079 | `"   return new ReflectionMethod($this->name, '__construct');"` |
|     - | 2080 | `"  }"` |
|     - | 2081 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 2082 | `"   if(strtolower($k) === '__construct'){ return new ReflectionMethod($this->name, $k); }"` |
|     - | 2083 | `"  }"` |
|     - | 2084 | `"  if($i['ctorvis'] !== 0 && isset($i['methods'][$this->name])){"` |
|     - | 2085 | `"   return new ReflectionMethod($this->name, $this->name);"` |
|     - | 2086 | `"  }"` |
|     - | 2087 | `"  return null;"` |
|     - | 2088 | `" }"` |
|     - | 2089 | `" public function getReflectionConstant($name){"` |
|     - | 2090 | `"  $i = $this->__rinfo();"` |
|     - | 2091 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 2092 | `"  return new ReflectionClassConstant($this->name, $name);"` |
|     - | 2093 | `" }"` |
|     - | 2094 | `" public function getReflectionConstants($filter = null){"` |
|     - | 2095 | `"  $i = $this->__rinfo();"` |
|     - | 2096 | `"  $out = array();"` |
|     - | 2097 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 2098 | `"   if($filter !== null){"` |
|     - | 2099 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 2100 | `"    if($c['final']){ $m \|= 32; }"` |
|     - | 2101 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 2102 | `"   }"` |
|     - | 2103 | `"   $out[] = new ReflectionClassConstant($this->name, $k);"` |
|     - | 2104 | `"  }"` |
|     - | 2105 | `"  return $out;"` |
|     - | 2106 | `" }"` |
|     - | 2107 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2108 | `"  $i = $this->__rinfo();"` |
|     - | 2109 | `"  return __reflect_build_attrs($i['attrs'], array('class', $this->name, null, 0), 1, $name, $flags);"` |
|     - | 2110 | `" }"` |
|     - | 2111 | `" public function getExtensionName(){ $i = $this->__rinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2112 | `" public function getExtension(){ $i = $this->__rinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2113 | `" public function newLazyGhost($initializer, $options = 0){"` |
|     - | 2114 | `"  throw new Error('ReflectionClass::newLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2115 | `" }"` |
|     - | 2116 | `" public function newLazyProxy($factory, $options = 0){"` |
|     - | 2117 | `"  throw new Error('ReflectionClass::newLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2118 | `" }"` |
|     - | 2119 | `" public function resetAsLazyGhost($object, $initializer, $options = 0){"` |
|     - | 2120 | `"  throw new Error('ReflectionClass::resetAsLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2121 | `" }"` |
|     - | 2122 | `" public function resetAsLazyProxy($object, $factory, $options = 0){"` |
|     - | 2123 | `"  throw new Error('ReflectionClass::resetAsLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2124 | `" }"` |
|     - | 2125 | `" public function getLazyInitializer($object){ return null; }"` |
|     - | 2126 | `" public function initializeLazyObject($object){ return $object; }"` |
|     - | 2127 | `" public function markLazyObjectAsInitialized($object){ return $object; }"` |
|     - | 2128 | `" public function isUninitializedLazyObject($object){ return false; }"` |
|     - | 2129 | `" public function __toString(){ return __reflect_export_class($this); }"` |
|     - | 2130 | `"}"` |
|     - | 2131 | `"class ReflectionObject extends ReflectionClass {"` |
|     - | 2132 | `" public function __construct($object){"` |
|     - | 2133 | `"  if(!is_object($object)){"` |
|     - | 2134 | `"   throw new TypeError('ReflectionObject::__construct(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 2135 | `"  }"` |
|     - | 2136 | `"  parent::__construct($object);"` |
|     - | 2137 | `"  $this->__obj = $object;"` |
|     - | 2138 | `" }"` |
|     - | 2139 | `"}"` |
|     - | 2140 | `;` |
|     - | 2141 | `/*` |
|     - | 2142 | ` * Chunk 2: ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,` |
|     - | 2143 | ` * ReflectionParameter.` |
|     - | 2144 | ` */` |
|     - | 2145 | `static const char zReflectLib2[] =` |
|     - | 2146 | `"abstract class ReflectionFunctionAbstract implements Reflector {"` |
|     - | 2147 | `" public $name;"` |
|     - | 2148 | `" protected $__cl = null;"` |
|     - | 2149 | `" protected function __rfinfo(){"` |
|     - | 2150 | `"  if($this->__cl !== null){ return __reflect_sig_fixup(__reflect_func_info($this->__cl)); }"` |
|     - | 2151 | `"  return __reflect_sig_fixup(__reflect_func_info($this->name));"` |
|     - | 2152 | `" }"` |
|     - | 2153 | `" protected function __rftarget(){ return $this->__cl !== null ? $this->__cl : $this->name; }"` |
|     - | 2154 | `" protected function __rpspec(){ return $this->__rftarget(); }"` |
|     - | 2155 | `" public function getName(){ return $this->name; }"` |
|     - | 2156 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 2157 | `" public function getNamespaceName(){"` |
|     - | 2158 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2159 | `"  if($p === false){ return ''; }"` |
|     - | 2160 | `"  return substr($this->name,0,$p);"` |
|     - | 2161 | `" }"` |
|     - | 2162 | `" public function getShortName(){"` |
|     - | 2163 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2164 | `"  if($p === false){ return $this->name; }"` |
|     - | 2165 | `"  return substr($this->name,$p+1);"` |
|     - | 2166 | `" }"` |
|     - | 2167 | `" public function isClosure(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2168 | `" public function isGenerator(){ $i = $this->__rfinfo(); return $i['generator']; }"` |
|     - | 2169 | `" public function isVariadic(){ $i = $this->__rfinfo(); return $i['variadic']; }"` |
|     - | 2170 | `" public function returnsReference(){ $i = $this->__rfinfo(); return $i['byref']; }"` |
|     - | 2171 | `" public function isInternal(){ $i = $this->__rfinfo(); return $i['internal']; }"` |
|     - | 2172 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 2173 | `" public function isDeprecated(){ $i = $this->__rfinfo(); return __reflect_has_deprecated($i['attrs']); }"` |
|     - | 2174 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['fstatic']; }"` |
|     - | 2175 | `" public function getFileName(){ $i = $this->__rfinfo(); return $i['file']; }"` |
|     - | 2176 | `" public function getStartLine(){"` |
|     - | 2177 | `"  $i = $this->__rfinfo();"` |
|     - | 2178 | `"  if($i['internal']){ return false; }"` |
|     - | 2179 | `"  return $i['line'];"` |
|     - | 2180 | `" }"` |
|     - | 2181 | `" public function getEndLine(){"` |
|     - | 2182 | `"  $i = $this->__rfinfo();"` |
|     - | 2183 | `"  if($i['internal']){ return false; }"` |
|     - | 2184 | `"  return $i['endline'];"` |
|     - | 2185 | `" }"` |
|     - | 2186 | `" public function getDocComment(){ $i = $this->__rfinfo(); return $i['doc']; }"` |
|     - | 2187 | `" public function hasReturnType(){ $i = $this->__rfinfo(); return $i['rettext'] !== null; }"` |
|     - | 2188 | `" public function getReturnType(){ $i = $this->__rfinfo(); return __reflect_make_type($i['rettext']); }"` |
|     - | 2189 | `" public function hasTentativeReturnType(){ return false; }"` |
|     - | 2190 | `" public function getTentativeReturnType(){ return null; }"` |
|     - | 2191 | `" public function getNumberOfParameters(){"` |
|     - | 2192 | `"  $i = $this->__rfinfo();"` |
|     - | 2193 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2194 | `"  return count($i['params']);"` |
|     - | 2195 | `" }"` |
|     - | 2196 | `" public function getNumberOfRequiredParameters(){"` |
|     - | 2197 | `"  $i = $this->__rfinfo();"` |
|     - | 2198 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2199 | `"  $req = 0;"` |
|     - | 2200 | `"  $n = count($i['params']);"` |
|     - | 2201 | `"  for($k = $n - 1; $k >= 0; $k--){"` |
|     - | 2202 | `"   $p = $i['params'][$k];"` |
|     - | 2203 | `"   if(!$p['variadic'] && !$p['hasdef']){ $req = $k + 1; break; }"` |
|     - | 2204 | `"  }"` |
|     - | 2205 | `"  return $req;"` |
|     - | 2206 | `" }"` |
|     - | 2207 | `" public function getParameters(){"` |
|     - | 2208 | `"  $i = $this->__rfinfo();"` |
|     - | 2209 | `"  $out = array();"` |
|     - | 2210 | `"  $spec = $this->__rpspec();"` |
|     - | 2211 | `"  foreach($i['params'] as $p){"` |
|     - | 2212 | `"   $out[] = new ReflectionParameter($spec, $p['pos']);"` |
|     - | 2213 | `"  }"` |
|     - | 2214 | `"  return $out;"` |
|     - | 2215 | `" }"` |
|     - | 2216 | `" public function getStaticVariables(){ $i = $this->__rfinfo(); return $i['statics']; }"` |
|     - | 2217 | `" public function getClosureThis(){"` |
|     - | 2218 | `"  $i = $this->__rfinfo();"` |
|     - | 2219 | `"  return isset($i['this']) ? $i['this'] : null;"` |
|     - | 2220 | `" }"` |
|     - | 2221 | `" public function getClosureScopeClass(){"` |
|     - | 2222 | `"  $i = $this->__rfinfo();"` |
|     - | 2223 | `"  if(isset($i['scope'])){ return new ReflectionClass($i['scope']); }"` |
|     - | 2224 | `"  if(isset($i['this'])){ return new ReflectionClass(get_class($i['this'])); }"` |
|     - | 2225 | `"  return null;"` |
|     - | 2226 | `" }"` |
|     - | 2227 | `" public function getClosureCalledClass(){ return $this->getClosureScopeClass(); }"` |
|     - | 2228 | `" public function getClosureUsedVariables(){"` |
|     - | 2229 | `"  $i = $this->__rfinfo();"` |
|     - | 2230 | `"  return isset($i['used']) ? $i['used'] : array();"` |
|     - | 2231 | `" }"` |
|     - | 2232 | `" public function getExtensionName(){ $i = $this->__rfinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2233 | `" public function getExtension(){ $i = $this->__rfinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2234 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2235 | `"  $i = $this->__rfinfo();"` |
|     - | 2236 | `"  if($this instanceof ReflectionMethod){"` |
|     - | 2237 | `"   $spec = array('method', $this->class, $this->name, 0);"` |
|     - | 2238 | `"   $target = 4;"` |
|     - | 2239 | `"  }else{"` |
|     - | 2240 | `"   $spec = array('fn', $this->__rftarget(), null, 0);"` |
|     - | 2241 | `"   $target = 2;"` |
|     - | 2242 | `"  }"` |
|     - | 2243 | `"  return __reflect_build_attrs($i['attrs'], $spec, $target, $name, $flags);"` |
|     - | 2244 | `" }"` |
|     - | 2245 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2246 | `"}"` |
|     - | 2247 | `"class ReflectionFunction extends ReflectionFunctionAbstract {"` |
|     - | 2248 | `" const IS_DEPRECATED = 2048;"` |
|     - | 2249 | `" public function __construct($function){"` |
|     - | 2250 | `"  if($function instanceof Closure){"` |
|     - | 2251 | `"   $this->__cl = $function;"` |
|     - | 2252 | `"   $i = $this->__rfinfo();"` |
|     - | 2253 | `"   if($i['closure']){"` |
|     - | 2254 | `"    $f = $i['file'] === false ? '' : $i['file'];"` |
|     - | 2255 | `"    $this->name = '{closure:'.$f.':'.$i['line'].'}';"` |
|     - | 2256 | `"   }else{"` |
|     - | 2257 | `"    $this->name = $i['name'];"` |
|     - | 2258 | `"   }"` |
|     - | 2259 | `"   return;"` |
|     - | 2260 | `"  }"` |
|     - | 2261 | `"  if(!is_string($function)){"` |
|     - | 2262 | `"   throw new TypeError('ReflectionFunction::__construct(): Argument #1 ($function) must be of type Closure\|string, '.get_debug_type($function).' given');"` |
|     - | 2263 | `"  }"` |
|     - | 2264 | `"  $i = __reflect_func_info($function);"` |
|     - | 2265 | `"  if($i === null){"` |
|     - | 2266 | `"   throw new ReflectionException('Function '.$function.'() does not exist');"` |
|     - | 2267 | `"  }"` |
|     - | 2268 | `"  if($i['closure']){"` |
|     - | 2269 | `"   $this->name = '{closure:'.($i['file'] === false ? '' : $i['file']).':'.$i['line'].'}';"` |
|     - | 2270 | `"   $this->__cl = __reflect_closure($function, null, null);"` |
|     - | 2271 | `"  }else{"` |
|     - | 2272 | `"   $this->name = $i['name'];"` |
|     - | 2273 | `"  }"` |
|     - | 2274 | `" }"` |
|     - | 2275 | `" public function invoke(...$args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2276 | `" public function invokeArgs(array $args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2277 | `" public function getClosure(){"` |
|     - | 2278 | `"  if($this->__cl !== null){ return $this->__cl; }"` |
|     - | 2279 | `"  return __reflect_closure($this->name, null, null);"` |
|     - | 2280 | `" }"` |
|     - | 2281 | `" public function isAnonymous(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2282 | `" public function isDisabled(){ return false; }"` |
|     - | 2283 | `"}"` |
|     - | 2284 | `"class ReflectionMethod extends ReflectionFunctionAbstract {"` |
|     - | 2285 | `" const IS_PUBLIC = 1;"` |
|     - | 2286 | `" const IS_PROTECTED = 2;"` |
|     - | 2287 | `" const IS_PRIVATE = 4;"` |
|     - | 2288 | `" const IS_STATIC = 16;"` |
|     - | 2289 | `" const IS_FINAL = 32;"` |
|     - | 2290 | `" const IS_ABSTRACT = 64;"` |
|     - | 2291 | `" public $class;"` |
|     - | 2292 | `" public function __construct($objectOrMethod, $method = null){"` |
|     - | 2293 | `"  if($method === null){"` |
|     - | 2294 | `"   if(!is_string($objectOrMethod) \|\| strpos($objectOrMethod,'::') === false){"` |
|     - | 2295 | `"    throw new TypeError('ReflectionMethod::__construct(): Argument #1 ($objectOrMethod) must be of type object\|string, '.get_debug_type($objectOrMethod).' given');"` |
|     - | 2296 | `"   }"` |
|     - | 2297 | `"   $p = strpos($objectOrMethod,'::');"` |
|     - | 2298 | `"   $method = substr($objectOrMethod,$p+2);"` |
|     - | 2299 | `"   $objectOrMethod = substr($objectOrMethod,0,$p);"` |
|     - | 2300 | `"  }"` |
|     - | 2301 | `"  $ci = __phl_rcinfo($objectOrMethod);"` |
|     - | 2302 | `"  if($ci === null){"` |
|     - | 2303 | `"   throw new ReflectionException('Class \"'.$objectOrMethod.'\" does not exist');"` |
|     - | 2304 | `"  }"` |
|     - | 2305 | `"  $this->class = $ci['name'];"` |
|     - | 2306 | `"  $found = null;"` |
|     - | 2307 | `"  if(isset($ci['methods'][$method])){"` |
|     - | 2308 | `"   $found = $method;"` |
|     - | 2309 | `"  }else{"` |
|     - | 2310 | `"   $l = strtolower($method);"` |
|     - | 2311 | `"   foreach($ci['methods'] as $k => $m){"` |
|     - | 2312 | `"    if(strtolower($k) === $l){ $found = $k; break; }"` |
|     - | 2313 | `"   }"` |
|     - | 2314 | `"  }"` |
|     - | 2315 | `"  if($found === null){"` |
|     - | 2316 | `"   throw new ReflectionException('Method '.$this->class.'::'.$method.'() does not exist');"` |
|     - | 2317 | `"  }"` |
|     - | 2318 | `"  $this->name = $found;"` |
|     - | 2319 | `" }"` |
|     - | 2320 | `" public static function createFromMethodName($name){"` |
|     - | 2321 | `"  return new ReflectionMethod($name);"` |
|     - | 2322 | `" }"` |
|     - | 2323 | `" protected function __rfinfo(){ return __reflect_func_info($this->class, $this->name); }"` |
|     - | 2324 | `" protected function __rpspec(){ return array($this->class, $this->name); }"` |
|     - | 2325 | `" public function getDeclaringClass(){"` |
|     - | 2326 | `"  $i = $this->__rfinfo();"` |
|     - | 2327 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2328 | `" }"` |
|     - | 2329 | `" public function getModifiers(){"` |
|     - | 2330 | `"  $i = $this->__rfinfo();"` |
|     - | 2331 | `"  $m = ($i['vis'] === 1 ? 1 : ($i['vis'] === 2 ? 2 : 4));"` |
|     - | 2332 | `"  if($i['mstatic']){ $m \|= 16; }"` |
|     - | 2333 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 2334 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 2335 | `"  return $m;"` |
|     - | 2336 | `" }"` |
|     - | 2337 | `" public function isPublic(){ $i = $this->__rfinfo(); return $i['vis'] === 1; }"` |
|     - | 2338 | `" public function isProtected(){ $i = $this->__rfinfo(); return $i['vis'] === 2; }"` |
|     - | 2339 | `" public function isPrivate(){ $i = $this->__rfinfo(); return $i['vis'] === 3; }"` |
|     - | 2340 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['mstatic']; }"` |
|     - | 2341 | `" public function isAbstract(){ $i = $this->__rfinfo(); return $i['abstract']; }"` |
|     - | 2342 | `" public function isFinal(){ $i = $this->__rfinfo(); return $i['final']; }"` |
|     - | 2343 | `" public function isConstructor(){ return strtolower($this->name) === '__construct'; }"` |
|     - | 2344 | `" public function isDestructor(){ return strtolower($this->name) === '__destruct'; }"` |
|     - | 2345 | `" public function invoke($object = null, ...$args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2346 | `" public function invokeArgs($object, array $args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2347 | `" protected function __rinvoke($object, $args){"` |
|     - | 2348 | `"  $i = $this->__rfinfo();"` |
|     - | 2349 | `"  if(!$i['mstatic']){"` |
|     - | 2350 | `"   if(!is_object($object)){"` |
|     - | 2351 | `"    throw new ReflectionException('Trying to invoke non static method '.$this->class.'::'.$this->name.'() without an object');"` |
|     - | 2352 | `"   }"` |
|     - | 2353 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2354 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2355 | `"   }"` |
|     - | 2356 | `"  }else{"` |
|     - | 2357 | `"   $object = null;"` |
|     - | 2358 | `"  }"` |
|     - | 2359 | `"  return __reflect_invoke($this->class, $this->name, $object, $args);"` |
|     - | 2360 | `" }"` |
|     - | 2361 | `" public function getClosure($object = null){"` |
|     - | 2362 | `"  $i = $this->__rfinfo();"` |
|     - | 2363 | `"  if(!$i['mstatic']){"` |
|     - | 2364 | `"   if($object === null){"` |
|     - | 2365 | `"    throw new ValueError('ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods');"` |
|     - | 2366 | `"   }"` |
|     - | 2367 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2368 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2369 | `"   }"` |
|     - | 2370 | `"  }else{"` |
|     - | 2371 | `"   $object = null;"` |
|     - | 2372 | `"  }"` |
|     - | 2373 | `"  return __reflect_closure($this->class, $this->name, $object);"` |
|     - | 2374 | `" }"` |
|     - | 2375 | `" public function setAccessible($accessible){ }"` |
|     - | 2376 | `" public function hasPrototype(){ return $this->__rproto() !== null; }"` |
|     - | 2377 | `" public function getPrototype(){"` |
|     - | 2378 | `"  $p = $this->__rproto();"` |
|     - | 2379 | `"  if($p === null){"` |
|     - | 2380 | `"   throw new ReflectionException('Method '.$this->class.'::'.$this->name.' does not have a prototype');"` |
|     - | 2381 | `"  }"` |
|     - | 2382 | `"  return new ReflectionMethod($p, $this->name);"` |
|     - | 2383 | `" }"` |
|     - | 2384 | `" protected function __rproto(){"` |
|     - | 2385 | `"  $ci = __phl_rcinfo($this->class);"` |
|     - | 2386 | `"  $l = strtolower($this->name);"` |
|     - | 2387 | `"  $p = $ci['parent'];"` |
|     - | 2388 | `"  while($p !== null){"` |
|     - | 2389 | `"   $pi = __phl_rcinfo($p);"` |
|     - | 2390 | `"   foreach($pi['methods'] as $k => $m){"` |
|     - | 2391 | `"    if(strtolower($k) === $l && $m['vis'] !== 3){ return $m['decl']; }"` |
|     - | 2392 | `"   }"` |
|     - | 2393 | `"   $p = $pi['parent'];"` |
|     - | 2394 | `"  }"` |
|     - | 2395 | `"  foreach($ci['interfaces'] as $if){"` |
|     - | 2396 | `"   $ii = __phl_rcinfo($if);"` |
|     - | 2397 | `"   foreach($ii['methods'] as $k => $m){"` |
|     - | 2398 | `"    if(strtolower($k) === $l){ return $ii['name']; }"` |
|     - | 2399 | `"   }"` |
|     - | 2400 | `"  }"` |
|     - | 2401 | `"  return null;"` |
|     - | 2402 | `" }"` |
|     - | 2403 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2404 | `"}"` |
|     - | 2405 | `"class ReflectionParameter implements Reflector {"` |
|     - | 2406 | `" public $name;"` |
|     - | 2407 | `" protected $__t;"` |
|     - | 2408 | `" protected $__m = null;"` |
|     - | 2409 | `" protected $__p = 0;"` |
|     - | 2410 | `" public function __construct($function, $param){"` |
|     - | 2411 | `"  $m = null;"` |
|     - | 2412 | `"  $t = $function;"` |
|     - | 2413 | `"  if(is_array($function)){"` |
|     - | 2414 | `"   $t = $function[0];"` |
|     - | 2415 | `"   $m = $function[1];"` |
|     - | 2416 | `"   if(is_object($t)){ $t = get_class($t); }"` |
|     - | 2417 | `"  }else if(is_string($function) && strpos($function,'::') !== false){"` |
|     - | 2418 | `"   $p = strpos($function,'::');"` |
|     - | 2419 | `"   $m = substr($function,$p+2);"` |
|     - | 2420 | `"   $t = substr($function,0,$p);"` |
|     - | 2421 | `"  }"` |
|     - | 2422 | `"  if($m !== null){"` |
|     - | 2423 | `"   $rm = new ReflectionMethod($t, $m);"` |
|     - | 2424 | `"   $t = $rm->class;"` |
|     - | 2425 | `"   $m = $rm->name;"` |
|     - | 2426 | `"   $i = __reflect_func_info($t, $m);"` |
|     - | 2427 | `"  }else if($function instanceof Closure){"` |
|     - | 2428 | `"   $t = $function;"` |
|     - | 2429 | `"   $i = __reflect_func_info($function);"` |
|     - | 2430 | `"  }else{"` |
|     - | 2431 | `"   $i = __reflect_sig_fixup(__reflect_func_info($t));"` |
|     - | 2432 | `"   if($i === null){"` |
|     - | 2433 | `"    throw new ReflectionException('Function '.$t.'() does not exist');"` |
|     - | 2434 | `"   }"` |
|     - | 2435 | `"  }"` |
|     - | 2436 | `"  $found = null;"` |
|     - | 2437 | `"  if(is_int($param)){"` |
|     - | 2438 | `"   if(isset($i['params'][$param])){ $found = $i['params'][$param]; }"` |
|     - | 2439 | `"   if($found === null){"` |
|     - | 2440 | `"    throw new ReflectionException('The parameter specified by its offset could not be found');"` |
|     - | 2441 | `"   }"` |
|     - | 2442 | `"  }else{"` |
|     - | 2443 | `"   foreach($i['params'] as $pp){"` |
|     - | 2444 | `"    if($pp['name'] === $param){ $found = $pp; break; }"` |
|     - | 2445 | `"   }"` |
|     - | 2446 | `"   if($found === null){"` |
|     - | 2447 | `"    throw new ReflectionException('The parameter specified by its name could not be found');"` |
|     - | 2448 | `"   }"` |
|     - | 2449 | `"  }"` |
|     - | 2450 | `"  $this->name = $found['name'];"` |
|     - | 2451 | `"  $this->__t = $t;"` |
|     - | 2452 | `"  $this->__m = $m;"` |
|     - | 2453 | `"  $this->__p = $found['pos'];"` |
|     - | 2454 | `" }"` |
|     - | 2455 | `" protected function __rffull(){"` |
|     - | 2456 | `"  if($this->__m !== null){ return __reflect_func_info($this->__t, $this->__m); }"` |
|     - | 2457 | `"  return __reflect_sig_fixup(__reflect_func_info($this->__t));"` |
|     - | 2458 | `" }"` |
|     - | 2459 | `" protected function __rpinfo(){"` |
|     - | 2460 | `"  $i = $this->__rffull();"` |
|     - | 2461 | `"  return $i['params'][$this->__p];"` |
|     - | 2462 | `" }"` |
|     - | 2463 | `" public function getName(){ return $this->name; }"` |
|     - | 2464 | `" public function getPosition(){ return $this->__p; }"` |
|     - | 2465 | `" public function isPassedByReference(){ $p = $this->__rpinfo(); return $p['byref']; }"` |
|     - | 2466 | `" public function canBePassedByValue(){ return !$this->isPassedByReference(); }"` |
|     - | 2467 | `" public function isVariadic(){ $p = $this->__rpinfo(); return $p['variadic']; }"` |
|     - | 2468 | `" public function isPromoted(){ $p = $this->__rpinfo(); return $p['promoted']; }"` |
|     - | 2469 | `" public function isDefaultValueAvailable(){ $p = $this->__rpinfo(); return $p['hasdef']; }"` |
|     - | 2470 | `" public function isOptional(){"` |
|     - | 2471 | `"  $i = $this->__rffull();"` |
|     - | 2472 | `"  $n = count($i['params']);"` |
|     - | 2473 | `"  for($k = $this->__p; $k < $n; $k++){"` |
|     - | 2474 | `"   $p = $i['params'][$k];"` |
|     - | 2475 | `"   if(!$p['variadic'] && !$p['hasdef']){ return false; }"` |
|     - | 2476 | `"  }"` |
|     - | 2477 | `"  return true;"` |
|     - | 2478 | `" }"` |
|     - | 2479 | `" public function getDefaultValue(){"` |
|     - | 2480 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2481 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2482 | `"  }"` |
|     - | 2483 | `"  $p = $this->__rpinfo();"` |
|     - | 2484 | `"  if(isset($p['deftext'])){"` |
|     - | 2485 | `"   $s = __reflect_sig_scalar($p['deftext']);"` |
|     - | 2486 | `"   if($s[0]){ return $s[1]; }"` |
|     - | 2487 | `"   if($p['deftext'] === 'array (' \|\| strpos($p['deftext'], '[') === 0){ return array(); }"` |
|     - | 2488 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2489 | `"  }"` |
|     - | 2490 | `"  return __reflect_param_default($this->__t, $this->__m, $this->__p);"` |
|     - | 2491 | `" }"` |
|     - | 2492 | `" public function isDefaultValueConstant(){"` |
|     - | 2493 | `"  if(!$this->isDefaultValueAvailable()){ return false; }"` |
|     - | 2494 | `"  $p = $this->__rpinfo();"` |
|     - | 2495 | `"  if(isset($p['deftext'])){ return false; }"` |
|     - | 2496 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p) !== null;"` |
|     - | 2497 | `" }"` |
|     - | 2498 | `" public function getDefaultValueConstantName(){"` |
|     - | 2499 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2500 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2501 | `"  }"` |
|     - | 2502 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p);"` |
|     - | 2503 | `" }"` |
|     - | 2504 | `" public function allowsNull(){"` |
|     - | 2505 | `"  $p = $this->__rpinfo();"` |
|     - | 2506 | `"  if($p['typetext'] === null){ return true; }"` |
|     - | 2507 | `"  if($p['nullable']){ return true; }"` |
|     - | 2508 | `"  return $p['typetext'] === 'mixed' \|\| $p['typetext'] === 'null';"` |
|     - | 2509 | `" }"` |
|     - | 2510 | `" public function hasType(){ $p = $this->__rpinfo(); return $p['typetext'] !== null; }"` |
|     - | 2511 | `" public function getType(){ $p = $this->__rpinfo(); return __reflect_make_type($p['typetext']); }"` |
|     - | 2512 | `" public function getDeclaringFunction(){"` |
|     - | 2513 | `"  if($this->__m !== null){ return new ReflectionMethod($this->__t, $this->__m); }"` |
|     - | 2514 | `"  return new ReflectionFunction($this->__t);"` |
|     - | 2515 | `" }"` |
|     - | 2516 | `" public function getDeclaringClass(){"` |
|     - | 2517 | `"  if($this->__m === null){ return null; }"` |
|     - | 2518 | `"  $i = $this->__rffull();"` |
|     - | 2519 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2520 | `" }"` |
|     - | 2521 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2522 | `"  $p = $this->__rpinfo();"` |
|     - | 2523 | `"  return __reflect_build_attrs($p['attrs'], array('param', $this->__t, $this->__m, $this->__p), 32, $name, $flags);"` |
|     - | 2524 | `" }"` |
|     - | 2525 | `" public function __toString(){ return __reflect_export_param($this); }"` |
|     - | 2526 | `"}"` |
|     - | 2527 | `;` |
|     - | 2528 | `/*` |
|     - | 2529 | ` * Chunk 3: PropertyHookType, ReflectionProperty, ReflectionClassConstant.` |
|     - | 2530 | ` */` |
|     - | 2531 | `static const char zReflectLib3[] =` |
|     - | 2532 | `"enum PropertyHookType: string {"` |
|     - | 2533 | `" case Get = 'get';"` |
|     - | 2534 | `" case Set = 'set';"` |
|     - | 2535 | `"}"` |
|     - | 2536 | `"class ReflectionProperty implements Reflector {"` |
|     - | 2537 | `" const IS_PUBLIC = 1;"` |
|     - | 2538 | `" const IS_PROTECTED = 2;"` |
|     - | 2539 | `" const IS_PRIVATE = 4;"` |
|     - | 2540 | `" const IS_STATIC = 16;"` |
|     - | 2541 | `" const IS_FINAL = 32;"` |
|     - | 2542 | `" const IS_ABSTRACT = 64;"` |
|     - | 2543 | `" const IS_READONLY = 128;"` |
|     - | 2544 | `" const IS_VIRTUAL = 512;"` |
|     - | 2545 | `" const IS_PROTECTED_SET = 2048;"` |
|     - | 2546 | `" const IS_PRIVATE_SET = 4096;"` |
|     - | 2547 | `" public $name;"` |
|     - | 2548 | `" public $class;"` |
|     - | 2549 | `" protected $__dynobj = null;"` |
|     - | 2550 | `" public function __construct($class, $property){"` |
|     - | 2551 | `"  $obj = null;"` |
|     - | 2552 | `"  if(is_object($class)){ $obj = $class; }"` |
|     - | 2553 | `"  else if(!is_string($class)){"` |
|     - | 2554 | `"   throw new TypeError('ReflectionProperty::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2555 | `"  }"` |
|     - | 2556 | `"  $ci = __phl_rcinfo($class);"` |
|     - | 2557 | `"  if($ci === null){"` |
|     - | 2558 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2559 | `"  }"` |
|     - | 2560 | `"  $this->class = $ci['name'];"` |
|     - | 2561 | `"  if(isset($ci['props'][$property])){"` |
|     - | 2562 | `"   $this->name = $property;"` |
|     - | 2563 | `"   return;"` |
|     - | 2564 | `"  }"` |
|     - | 2565 | `"  if($obj !== null && (__reflect_prop_state($obj, $property) & 1)){"` |
|     - | 2566 | `"   $this->name = $property;"` |
|     - | 2567 | `"   $this->__dynobj = $obj;"` |
|     - | 2568 | `"   return;"` |
|     - | 2569 | `"  }"` |
|     - | 2570 | `"  throw new ReflectionException('Property '.$this->class.'::$'.$property.' does not exist');"` |
|     - | 2571 | `" }"` |
|     - | 2572 | `" protected function __rpmeta(){"` |
|     - | 2573 | `"  $ci = __phl_rcinfo($this->class);"` |
|     - | 2574 | `"  if(isset($ci['props'][$this->name])){ return $ci['props'][$this->name]; }"` |
|     - | 2575 | `"  return array('vis' => 1, 'static' => false, 'readonly' => false, 'hasdef' => false,"` |
|     - | 2576 | `"   'typed' => false, 'typetext' => null, 'decl' => $this->class, 'line' => 0, 'dyn' => true);"` |
|     - | 2577 | `" }"` |
|     - | 2578 | `" public function getName(){ return $this->name; }"` |
|     - | 2579 | `" public function getDeclaringClass(){"` |
|     - | 2580 | `"  $m = $this->__rpmeta();"` |
|     - | 2581 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2582 | `" }"` |
|     - | 2583 | `" public function getModifiers(){"` |
|     - | 2584 | `"  $m = $this->__rpmeta();"` |
|     - | 2585 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2586 | `"  if($m['static']){ $mod \|= 16; }"` |
|     - | 2587 | `"  if($m['readonly']){ $mod \|= 128; }"` |
|     - | 2588 | `"  return $mod;"` |
|     - | 2589 | `" }"` |
|     - | 2590 | `" public function isPublic(){ $m = $this->__rpmeta(); return $m['vis'] === 1; }"` |
|     - | 2591 | `" public function isProtected(){ $m = $this->__rpmeta(); return $m['vis'] === 2; }"` |
|     - | 2592 | `" public function isPrivate(){ $m = $this->__rpmeta(); return $m['vis'] === 3; }"` |
|     - | 2593 | `" public function isStatic(){ $m = $this->__rpmeta(); return $m['static']; }"` |
|     - | 2594 | `" public function isReadOnly(){ $m = $this->__rpmeta(); return $m['readonly']; }"` |
|     - | 2595 | `" public function isPrivateSet(){ $m = $this->__rpmeta(); return isset($m['privset']) ? $m['privset'] : false; }"` |
|     - | 2596 | `" public function isProtectedSet(){ $m = $this->__rpmeta(); return isset($m['protset']) ? $m['protset'] : false; }"` |
|     - | 2597 | `" public function isDefault(){ $m = $this->__rpmeta(); return !isset($m['dyn']); }"` |
|     - | 2598 | `" public function isDynamic(){ $m = $this->__rpmeta(); return isset($m['dyn']); }"` |
|     - | 2599 | `" public function isAbstract(){ return false; }"` |
|     - | 2600 | `" public function isFinal(){ return false; }"` |
|     - | 2601 | `" public function isVirtual(){ $m = $this->__rpmeta(); return isset($m['virtual']) ? $m['virtual'] : false; }"` |
|     - | 2602 | `" public function hasHooks(){ $m = $this->__rpmeta();"` |
|     - | 2603 | `"  return (isset($m['hookget']) && $m['hookget']) \|\| (isset($m['hookset']) && $m['hookset']); }"` |
|     - | 2604 | `" public function getHooks(){"` |
|     - | 2605 | `"  $m = $this->__rpmeta(); $h = array();"` |
|     - | 2606 | `"  if(isset($m['hookget']) && $m['hookget']){ $h['get'] = new ReflectionMethod($m['decl'], '__phl_hook_get_'.$this->name); }"` |
|     - | 2607 | `"  if(isset($m['hookset']) && $m['hookset']){ $h['set'] = new ReflectionMethod($m['decl'], '__phl_hook_set_'.$this->name); }"` |
|     - | 2608 | `"  return $h; }"` |
|     - | 2609 | `" public function hasHook($type){"` |
|     - | 2610 | `"  $t = $type instanceof PropertyHookType ? $type->value : $type;"` |
|     - | 2611 | `"  $m = $this->__rpmeta();"` |
|     - | 2612 | `"  if($t === 'get'){ return isset($m['hookget']) && $m['hookget']; }"` |
|     - | 2613 | `"  if($t === 'set'){ return isset($m['hookset']) && $m['hookset']; }"` |
|     - | 2614 | `"  return false; }"` |
|     - | 2615 | `" public function getHook($type){"` |
|     - | 2616 | `"  $t = $type instanceof PropertyHookType ? $type->value : $type;"` |
|     - | 2617 | `"  $h = $this->getHooks();"` |
|     - | 2618 | `"  return isset($h[$t]) ? $h[$t] : null; }"` |
|     - | 2619 | `" public function isLazy($object){ return false; }"` |
|     - | 2620 | `" public function setAccessible($accessible){ }"` |
|     - | 2621 | `" public function getValue($object = null){"` |
|     - | 2622 | `"  $m = $this->__rpmeta();"` |
|     - | 2623 | `"  if($m['static']){ return __reflect_static_value($this->class, $this->name); }"` |
|     - | 2624 | `"  if(!is_object($object)){"` |
|     - | 2625 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2626 | `"  }"` |
|     - | 2627 | `"  return __reflect_prop_read($object, $this->name);"` |
|     - | 2628 | `" }"` |
|     - | 2629 | `" public function setValue($objectOrValue = null, $value = null){"` |
|     - | 2630 | `"  $m = $this->__rpmeta();"` |
|     - | 2631 | `"  if($m['static']){"` |
|     - | 2632 | `"   if($value === null && $objectOrValue !== null && !is_object($objectOrValue)){"` |
|     - | 2633 | `"    __reflect_static_set($this->class, $this->name, $objectOrValue);"` |
|     - | 2634 | `"   }else{"` |
|     - | 2635 | `"    __reflect_static_set($this->class, $this->name, $value);"` |
|     - | 2636 | `"   }"` |
|     - | 2637 | `"   return;"` |
|     - | 2638 | `"  }"` |
|     - | 2639 | `"  __reflect_prop_write($objectOrValue, $this->name, $value);"` |
|     - | 2640 | `" }"` |
|     - | 2641 | `" public function getRawValue($object){ return $this->getValue($object); }"` |
|     - | 2642 | `" public function setRawValue($object, $value){ $this->setValue($object, $value); }"` |
|     - | 2643 | `" public function isInitialized($object = null){"` |
|     - | 2644 | `"  $m = $this->__rpmeta();"` |
|     - | 2645 | `"  if($m['static']){ return (__reflect_prop_state($this->class, $this->name) & 2) !== 0; }"` |
|     - | 2646 | `"  if(!is_object($object)){"` |
|     - | 2647 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2648 | `"  }"` |
|     - | 2649 | `"  return (__reflect_prop_state($object, $this->name) & 2) !== 0;"` |
|     - | 2650 | `" }"` |
|     - | 2651 | `" public function hasDefaultValue(){"` |
|     - | 2652 | `"  $m = $this->__rpmeta();"` |
|     - | 2653 | `"  if(isset($m['dyn'])){ return false; }"` |
|     - | 2654 | `"  if($m['hasdef']){ return true; }"` |
|     - | 2655 | `"  return !$m['typed'];"` |
|     - | 2656 | `" }"` |
|     - | 2657 | `" public function getDefaultValue(){"` |
|     - | 2658 | `"  $m = $this->__rpmeta();"` |
|     - | 2659 | `"  if(isset($m['dyn']) \|\| !$m['hasdef']){ return null; }"` |
|     - | 2660 | `"  return __reflect_prop_default($this->class, $this->name);"` |
|     - | 2661 | `" }"` |
|     - | 2662 | `" public function hasType(){ $m = $this->__rpmeta(); return $m['typed']; }"` |
|     - | 2663 | `" public function getType(){ $m = $this->__rpmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2664 | `" public function getSettableType(){ return $this->getType(); }"` |
|     - | 2665 | `" public function setRawValueWithoutLazyInitialization($object, $value){"` |
|     - | 2666 | `"  throw new Error('ReflectionProperty::setRawValueWithoutLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2667 | `" }"` |
|     - | 2668 | `" public function skipLazyInitialization($object){"` |
|     - | 2669 | `"  throw new Error('ReflectionProperty::skipLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2670 | `" }"` |
|     - | 2671 | `" public function getDocComment(){ $m = $this->__rpmeta(); return isset($m['doc']) ? $m['doc'] : false; }"` |
|     - | 2672 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2673 | `"  $m = $this->__rpmeta();"` |
|     - | 2674 | `"  if(!isset($m['attrs'])){ return array(); }"` |
|     - | 2675 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 8, $name, $flags);"` |
|     - | 2676 | `" }"` |
|     - | 2677 | `" public function __toString(){ return __reflect_export_prop($this); }"` |
|     - | 2678 | `"}"` |
|     - | 2679 | `"class ReflectionClassConstant implements Reflector {"` |
|     - | 2680 | `" const IS_PUBLIC = 1;"` |
|     - | 2681 | `" const IS_PROTECTED = 2;"` |
|     - | 2682 | `" const IS_PRIVATE = 4;"` |
|     - | 2683 | `" const IS_FINAL = 32;"` |
|     - | 2684 | `" public $name;"` |
|     - | 2685 | `" public $class;"` |
|     - | 2686 | `" public function __construct($class, $constant){"` |
|     - | 2687 | `"  if(!is_object($class) && !is_string($class)){"` |
|     - | 2688 | `"   throw new TypeError('ReflectionClassConstant::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2689 | `"  }"` |
|     - | 2690 | `"  $ci = __phl_rcinfo($class);"` |
|     - | 2691 | `"  if($ci === null){"` |
|     - | 2692 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2693 | `"  }"` |
|     - | 2694 | `"  $this->class = $ci['name'];"` |
|     - | 2695 | `"  if(!isset($ci['consts'][$constant])){"` |
|     - | 2696 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' does not exist');"` |
|     - | 2697 | `"  }"` |
|     - | 2698 | `"  $this->name = $constant;"` |
|     - | 2699 | `" }"` |
|     - | 2700 | `" protected function __rcmeta(){"` |
|     - | 2701 | `"  $ci = __phl_rcinfo($this->class);"` |
|     - | 2702 | `"  return $ci['consts'][$this->name];"` |
|     - | 2703 | `" }"` |
|     - | 2704 | `" public function getName(){ return $this->name; }"` |
|     - | 2705 | `" public function getValue(){ return __reflect_const_value($this->class, $this->name); }"` |
|     - | 2706 | `" public function getDeclaringClass(){"` |
|     - | 2707 | `"  $m = $this->__rcmeta();"` |
|     - | 2708 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2709 | `" }"` |
|     - | 2710 | `" public function getModifiers(){"` |
|     - | 2711 | `"  $m = $this->__rcmeta();"` |
|     - | 2712 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2713 | `"  if($m['final']){ $mod \|= 32; }"` |
|     - | 2714 | `"  return $mod;"` |
|     - | 2715 | `" }"` |
|     - | 2716 | `" public function isPublic(){ $m = $this->__rcmeta(); return $m['vis'] === 1; }"` |
|     - | 2717 | `" public function isProtected(){ $m = $this->__rcmeta(); return $m['vis'] === 2; }"` |
|     - | 2718 | `" public function isPrivate(){ $m = $this->__rcmeta(); return $m['vis'] === 3; }"` |
|     - | 2719 | `" public function isFinal(){ $m = $this->__rcmeta(); return $m['final']; }"` |
|     - | 2720 | `" public function isEnumCase(){ $m = $this->__rcmeta(); return $m['enumcase']; }"` |
|     - | 2721 | `" public function isDeprecated(){ $m = $this->__rcmeta(); return __reflect_has_deprecated($m['attrs']); }"` |
|     - | 2722 | `" public function hasType(){ $m = $this->__rcmeta(); return $m['typed']; }"` |
|     - | 2723 | `" public function getType(){ $m = $this->__rcmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2724 | `" public function getDocComment(){ $m = $this->__rcmeta(); return $m['doc']; }"` |
|     - | 2725 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2726 | `"  $m = $this->__rcmeta();"` |
|     - | 2727 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 16, $name, $flags);"` |
|     - | 2728 | `" }"` |
|     - | 2729 | `" public function __toString(){ return __reflect_export_cconst($this); }"` |
|     - | 2730 | `"}"` |
|     - | 2731 | `;` |
|     - | 2732 | `/*` |
|     - | 2733 | ` * Chunk 4: the ReflectionType family, built from the engine's canonical` |
|     - | 2734 | ` * type text ("?int", "string\|float", "(A&B)\|C" — normalized at compile` |
|     - | 2735 | ` * time). __reflect_make_type is the internal factory; PHP itself never` |
|     - | 2736 | ` * lets user code construct these, so the public constructors here are a` |
|     - | 2737 | ` * recorded PHL-only surface.` |
|     - | 2738 | ` */` |
|     - | 2739 | `static const char zReflectLib4[] =` |
|     - | 2740 | `"abstract class ReflectionType implements Stringable {"` |
|     - | 2741 | `" protected $__text = '';"` |
|     - | 2742 | `" protected $__nullable = false;"` |
|     - | 2743 | `" public function allowsNull(){ return $this->__nullable; }"` |
|     - | 2744 | `" public function __toString(){ return $this->__text; }"` |
|     - | 2745 | `"}"` |
|     - | 2746 | `"class ReflectionNamedType extends ReflectionType {"` |
|     - | 2747 | `" protected $__tname = '';"` |
|     - | 2748 | `" public function __construct($name = '', $nullable = false, $text = null){"` |
|     - | 2749 | `"  $this->__tname = $name;"` |
|     - | 2750 | `"  $l = strtolower($name);"` |
|     - | 2751 | `"  $this->__nullable = $nullable \|\| $l === 'null' \|\| $l === 'mixed';"` |
|     - | 2752 | `"  $this->__text = $text === null ? $name : $text;"` |
|     - | 2753 | `" }"` |
|     - | 2754 | `" public function getName(){ return $this->__tname; }"` |
|     - | 2755 | `" public function isBuiltin(){"` |
|     - | 2756 | `"  $l = strtolower($this->__tname);"` |
|     - | 2757 | `"  return in_array($l, array('int','float','string','bool','array','object','mixed',"` |
|     - | 2758 | `"   'void','never','null','callable','iterable','true','false'), true);"` |
|     - | 2759 | `" }"` |
|     - | 2760 | `"}"` |
|     - | 2761 | `"class ReflectionUnionType extends ReflectionType {"` |
|     - | 2762 | `" protected $__types = array();"` |
|     - | 2763 | `" public function __construct($text = '', $nullable = false, $types = array()){"` |
|     - | 2764 | `"  $this->__text = $text;"` |
|     - | 2765 | `"  $this->__nullable = $nullable;"` |
|     - | 2766 | `"  $this->__types = $types;"` |
|     - | 2767 | `" }"` |
|     - | 2768 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2769 | `"}"` |
|     - | 2770 | `"class ReflectionIntersectionType extends ReflectionType {"` |
|     - | 2771 | `" protected $__types = array();"` |
|     - | 2772 | `" public function __construct($text = '', $types = array()){"` |
|     - | 2773 | `"  $this->__text = $text;"` |
|     - | 2774 | `"  $this->__nullable = false;"` |
|     - | 2775 | `"  $this->__types = $types;"` |
|     - | 2776 | `" }"` |
|     - | 2777 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2778 | `"}"` |
|     - | 2779 | `"function __reflect_make_atom($p){"` |
|     - | 2780 | `" $nullable = false;"` |
|     - | 2781 | `" if($p !== '' && $p[0] === '?'){ $nullable = true; $p = substr($p, 1); }"` |
|     - | 2782 | `" if($p !== '' && $p[0] === '('){ $p = substr($p, 1, strlen($p) - 2); }"` |
|     - | 2783 | `" if(strpos($p, '&') !== false){"` |
|     - | 2784 | `"  $subs = array();"` |
|     - | 2785 | `"  foreach(explode('&', $p) as $s){ $subs[] = new ReflectionNamedType($s, false, $s); }"` |
|     - | 2786 | `"  return new ReflectionIntersectionType($p, $subs);"` |
|     - | 2787 | `" }"` |
|     - | 2788 | `" return new ReflectionNamedType($p, $nullable, $nullable ? '?'.$p : $p);"` |
|     - | 2789 | `"}"` |
|     - | 2790 | `"function __reflect_make_type($text){"` |
|     - | 2791 | `" if($text === null \|\| $text === ''){ return null; }"` |
|     - | 2792 | `" $nullable = false;"` |
|     - | 2793 | `" $body = $text;"` |
|     - | 2794 | `" if($body[0] === '?'){ $nullable = true; $body = substr($body, 1); }"` |
|     - | 2795 | `" $parts = array();"` |
|     - | 2796 | `" $depth = 0;"` |
|     - | 2797 | `" $cur = '';"` |
|     - | 2798 | `" $n = strlen($body);"` |
|     - | 2799 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 2800 | `"  $ch = $body[$k];"` |
|     - | 2801 | `"  if($ch === '('){ $depth++; $cur .= $ch; }"` |
|     - | 2802 | `"  else if($ch === ')'){ $depth--; $cur .= $ch; }"` |
|     - | 2803 | `"  else if($ch === '\|' && $depth === 0){ $parts[] = $cur; $cur = ''; }"` |
|     - | 2804 | `"  else{ $cur .= $ch; }"` |
|     - | 2805 | `" }"` |
|     - | 2806 | `" $parts[] = $cur;"` |
|     - | 2807 | `" if(count($parts) > 1){"` |
|     - | 2808 | `"  $nonNull = array();"` |
|     - | 2809 | `"  $hasNull = false;"` |
|     - | 2810 | `"  foreach($parts as $p){"` |
|     - | 2811 | `"   if(strtolower($p) === 'null'){ $hasNull = true; }"` |
|     - | 2812 | `"   else{ $nonNull[] = $p; }"` |
|     - | 2813 | `"  }"` |
|     - | 2814 | `"  if($hasNull && count($nonNull) === 1 && strpos($nonNull[0], '&') === false){"` |
|     - | 2815 | `"   return new ReflectionNamedType($nonNull[0], true, '?'.$nonNull[0]);"` |
|     - | 2816 | `"  }"` |
|     - | 2817 | `"  $types = array();"` |
|     - | 2818 | `"  foreach($parts as $p){ $types[] = __reflect_make_atom($p); }"` |
|     - | 2819 | `"  return new ReflectionUnionType($body, $nullable \|\| $hasNull, $types);"` |
|     - | 2820 | `" }"` |
|     - | 2821 | `" if(strpos($body, '&') !== false){ return __reflect_make_atom($body); }"` |
|     - | 2822 | `" return __reflect_make_atom($nullable ? '?'.$body : $body);"` |
|     - | 2823 | `"}"` |
|     - | 2824 | `;` |
|     - | 2825 | `/*` |
|     - | 2826 | ` * Chunk 5: ReflectionGenerator, ReflectionFiber. Executing line/file and` |
|     - | 2827 | ` * traces need runtime line tracking the VM does not have (same gap as` |
|     - | 2828 | ` * debug_backtrace's line numbers) — those throw a loud Error, recorded in` |
|     - | 2829 | ` * the plan ledger.` |
|     - | 2830 | ` */` |
|     - | 2831 | `static const char zReflectLib5[] =` |
|     - | 2832 | `"class ReflectionGenerator {"` |
|     - | 2833 | `" protected $__gen;"` |
|     - | 2834 | `" public function __construct($generator){"` |
|     - | 2835 | `"  if(!($generator instanceof Generator)){"` |
|     - | 2836 | `"   throw new TypeError('ReflectionGenerator::__construct(): Argument #1 ($generator) must be of type Generator, '.get_debug_type($generator).' given');"` |
|     - | 2837 | `"  }"` |
|     - | 2838 | `"  $this->__gen = $generator;"` |
|     - | 2839 | `" }"` |
|     - | 2840 | `" protected function __rginfo(){ return __reflect_gen_info($this->__gen); }"` |
|     - | 2841 | `" public function getFunction(){"` |
|     - | 2842 | `"  $i = $this->__rginfo();"` |
|     - | 2843 | `"  if($i['kind'] === 'method'){ return new ReflectionMethod($i['class'], $i['name']); }"` |
|     - | 2844 | `"  return new ReflectionFunction($i['name']);"` |
|     - | 2845 | `" }"` |
|     - | 2846 | `" public function getThis(){ $i = $this->__rginfo(); return isset($i['this']) ? $i['this'] : null; }"` |
|     - | 2847 | `" public function getExecutingGenerator(){ return __reflect_gen_exec($this->__gen); }"` |
|     - | 2848 | `" public function isClosed(){ $i = $this->__rginfo(); return $i['closed']; }"` |
|     - | 2849 | `" public function getExecutingLine(){"` |
|     - | 2850 | `"  throw new Error('ReflectionGenerator::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2851 | `" }"` |
|     - | 2852 | `" public function getExecutingFile(){"` |
|     - | 2853 | `"  throw new Error('ReflectionGenerator::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2854 | `" }"` |
|     - | 2855 | `" public function getTrace($options = 1){"` |
|     - | 2856 | `"  throw new Error('ReflectionGenerator::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2857 | `" }"` |
|     - | 2858 | `"}"` |
|     - | 2859 | `"class ReflectionFiber {"` |
|     - | 2860 | `" protected $__fiber;"` |
|     - | 2861 | `" public function __construct($fiber){"` |
|     - | 2862 | `"  if(!($fiber instanceof Fiber)){"` |
|     - | 2863 | `"   throw new TypeError('ReflectionFiber::__construct(): Argument #1 ($fiber) must be of type Fiber, '.get_debug_type($fiber).' given');"` |
|     - | 2864 | `"  }"` |
|     - | 2865 | `"  $this->__fiber = $fiber;"` |
|     - | 2866 | `" }"` |
|     - | 2867 | `" public function getFiber(){ return $this->__fiber; }"` |
|     - | 2868 | `" public function getCallable(){ return __reflect_prop_read($this->__fiber, '__callable'); }"` |
|     - | 2869 | `" public function getExecutingLine(){"` |
|     - | 2870 | `"  throw new Error('ReflectionFiber::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2871 | `" }"` |
|     - | 2872 | `" public function getExecutingFile(){"` |
|     - | 2873 | `"  throw new Error('ReflectionFiber::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2874 | `" }"` |
|     - | 2875 | `" public function getTrace($options = 1){"` |
|     - | 2876 | `"  throw new Error('ReflectionFiber::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2877 | `" }"` |
|     - | 2878 | `"}"` |
|     - | 2879 | `;` |
|     - | 2880 | `/*` |
|     - | 2881 | ` * Chunk 6: the long tail — ReflectionConstant (PHP 8.5), the synthetic` |
|     - | 2882 | ` * "Core" ReflectionExtension, ReflectionZendExtension (throws: no Zend` |
|     - | 2883 | ` * extensions exist), the ReflectionEnum family (throws: enums are not a` |
|     - | 2884 | ` * PHL language feature yet), and ReflectionReference.` |
|     - | 2885 | ` */` |
|     - | 2886 | `static const char zReflectLib6[] =` |
|     - | 2887 | `"class ReflectionConstant implements Reflector {"` |
|     - | 2888 | `" public $name;"` |
|     - | 2889 | `" public function __construct($name){"` |
|     - | 2890 | `"  if(!is_string($name)){"` |
|     - | 2891 | `"   throw new TypeError('ReflectionConstant::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2892 | `"  }"` |
|     - | 2893 | `"  $i = __reflect_const_info($name);"` |
|     - | 2894 | `"  if($i === null){"` |
|     - | 2895 | `"   throw new ReflectionException('Constant \"'.$name.'\" does not exist');"` |
|     - | 2896 | `"  }"` |
|     - | 2897 | `"  $this->name = $name;"` |
|     - | 2898 | `" }"` |
|     - | 2899 | `" public function getName(){ return $this->name; }"` |
|     - | 2900 | `" public function getNamespaceName(){"` |
|     - | 2901 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2902 | `"  if($p === false){ return ''; }"` |
|     - | 2903 | `"  return substr($this->name,0,$p);"` |
|     - | 2904 | `" }"` |
|     - | 2905 | `" public function getShortName(){"` |
|     - | 2906 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2907 | `"  if($p === false){ return $this->name; }"` |
|     - | 2908 | `"  return substr($this->name,$p+1);"` |
|     - | 2909 | `" }"` |
|     - | 2910 | `" public function getValue(){"` |
|     - | 2911 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2912 | `"  return $i['value'];"` |
|     - | 2913 | `" }"` |
|     - | 2914 | `" public function isDeprecated(){ return false; }"` |
|     - | 2915 | `" public function getFileName(){"` |
|     - | 2916 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2917 | `"  return $i['file'];"` |
|     - | 2918 | `" }"` |
|     - | 2919 | `" public function getExtension(){"` |
|     - | 2920 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2921 | `"  return $i['internal'] ? new ReflectionExtension('Core') : null;"` |
|     - | 2922 | `" }"` |
|     - | 2923 | `" public function getExtensionName(){"` |
|     - | 2924 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2925 | `"  return $i['internal'] ? 'Core' : false;"` |
|     - | 2926 | `" }"` |
|     - | 2927 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2928 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2929 | `"  if($i === null){ return array(); }"` |
|     - | 2930 | `"  return __reflect_build_attrs($i['attrs'], array('const', $this->name, null, 0), 64, $name, $flags);"` |
|     - | 2931 | `" }"` |
|     - | 2932 | `" public function __toString(){"` |
|     - | 2933 | `"  return 'Constant [ '.$this->name.' ]'.\"\\n\";"` |
|     - | 2934 | `" }"` |
|     - | 2935 | `"}"` |
|     - | 2936 | `"class ReflectionExtension implements Reflector {"` |
|     - | 2937 | `" public $name;"` |
|     - | 2938 | `" public function __construct($name){"` |
|     - | 2939 | `"  if(!is_string($name)){"` |
|     - | 2940 | `"   throw new TypeError('ReflectionExtension::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2941 | `"  }"` |
|     - | 2942 | `"  if(strtolower($name) !== 'core'){"` |
|     - | 2943 | `"   throw new ReflectionException('Extension \"'.$name.'\" does not exist');"` |
|     - | 2944 | `"  }"` |
|     - | 2945 | `"  $this->name = 'Core';"` |
|     - | 2946 | `" }"` |
|     - | 2947 | `" public function getName(){ return $this->name; }"` |
|     - | 2948 | `" public function getVersion(){ return phpversion(); }"` |
|     - | 2949 | `" public function getFunctions(){ return array(); }"` |
|     - | 2950 | `" public function getClasses(){ return array(); }"` |
|     - | 2951 | `" public function getClassNames(){ return array(); }"` |
|     - | 2952 | `" public function getConstants(){ return array(); }"` |
|     - | 2953 | `" public function getINIEntries(){ return array(); }"` |
|     - | 2954 | `" public function getDependencies(){ return array(); }"` |
|     - | 2955 | `" public function isPersistent(){ return true; }"` |
|     - | 2956 | `" public function isTemporary(){ return false; }"` |
|     - | 2957 | `" public function info(){ }"` |
|     - | 2958 | `" public function __toString(){"` |
|     - | 2959 | `"  return 'Extension [ extension #1 '.$this->name.' ]'.\"\\n\";"` |
|     - | 2960 | `" }"` |
|     - | 2961 | `"}"` |
|     - | 2962 | `"class ReflectionZendExtension implements Reflector {"` |
|     - | 2963 | `" public $name;"` |
|     - | 2964 | `" public function __construct($name){"` |
|     - | 2965 | `"  throw new ReflectionException('Zend Extension \"'.$name.'\" does not exist');"` |
|     - | 2966 | `" }"` |
|     - | 2967 | `" public function getName(){ return $this->name; }"` |
|     - | 2968 | `" public function __toString(){ return ''; }"` |
|     - | 2969 | `"}"` |
|     - | 2970 | `"class ReflectionEnum extends ReflectionClass {"` |
|     - | 2971 | `" public function __construct($objectOrClass){"` |
|     - | 2972 | `"  $info = __phl_rcinfo($objectOrClass);"` |
|     - | 2973 | `"  if($info === null){"` |
|     - | 2974 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 2975 | `"  }"` |
|     - | 2976 | `"  if(!$info['enum']){"` |
|     - | 2977 | `"   throw new ReflectionException('Class \"'.$info['name'].'\" is not an enum');"` |
|     - | 2978 | `"  }"` |
|     - | 2979 | `"  parent::__construct($objectOrClass);"` |
|     - | 2980 | `" }"` |
|     - | 2981 | `" public function hasCase($name){"` |
|     - | 2982 | `"  $i = $this->__rinfo();"` |
|     - | 2983 | `"  return in_array($name, $i['cases'], true);"` |
|     - | 2984 | `" }"` |
|     - | 2985 | `" public function getCase($name){"` |
|     - | 2986 | `"  if(!$this->hasCase($name)){"` |
|     - | 2987 | `"   throw new ReflectionException('Case '.$this->name.'::'.$name.' does not exist');"` |
|     - | 2988 | `"  }"` |
|     - | 2989 | `"  if($this->isBacked()){ return new ReflectionEnumBackedCase($this->name, $name); }"` |
|     - | 2990 | `"  return new ReflectionEnumUnitCase($this->name, $name);"` |
|     - | 2991 | `" }"` |
|     - | 2992 | `" public function getCases(){"` |
|     - | 2993 | `"  $i = $this->__rinfo();"` |
|     - | 2994 | `"  $out = array();"` |
|     - | 2995 | `"  foreach($i['cases'] as $c){"` |
|     - | 2996 | `"   $out[] = $this->isBacked()"` |
|     - | 2997 | `"    ? new ReflectionEnumBackedCase($this->name, $c)"` |
|     - | 2998 | `"    : new ReflectionEnumUnitCase($this->name, $c);"` |
|     - | 2999 | `"  }"` |
|     - | 3000 | `"  return $out;"` |
|     - | 3001 | `" }"` |
|     - | 3002 | `" public function isBacked(){ $i = $this->__rinfo(); return $i['enumbacking'] !== ''; }"` |
|     - | 3003 | `" public function getBackingType(){"` |
|     - | 3004 | `"  $i = $this->__rinfo();"` |
|     - | 3005 | `"  if($i['enumbacking'] === ''){ return null; }"` |
|     - | 3006 | `"  return __reflect_make_type($i['enumbacking']);"` |
|     - | 3007 | `" }"` |
|     - | 3008 | `"}"` |
|     - | 3009 | `"class ReflectionEnumUnitCase extends ReflectionClassConstant {"` |
|     - | 3010 | `" public function __construct($class, $constant){"` |
|     - | 3011 | `"  parent::__construct($class, $constant);"` |
|     - | 3012 | `"  $ci = __phl_rcinfo($class);"` |
|     - | 3013 | `"  if(!$ci['enum']){"` |
|     - | 3014 | `"   throw new ReflectionException('Class \"'.$this->class.'\" is not an enum');"` |
|     - | 3015 | `"  }"` |
|     - | 3016 | `"  $m = $this->__rcmeta();"` |
|     - | 3017 | `"  if(!$m['enumcase']){"` |
|     - | 3018 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' is not a case');"` |
|     - | 3019 | `"  }"` |
|     - | 3020 | `" }"` |
|     - | 3021 | `" public function getEnum(){ return new ReflectionEnum($this->class); }"` |
|     - | 3022 | `"}"` |
|     - | 3023 | `"class ReflectionEnumBackedCase extends ReflectionEnumUnitCase {"` |
|     - | 3024 | `" public function getBackingValue(){ return $this->getValue()->value; }"` |
|     - | 3025 | `"}"` |
|     - | 3026 | `"final class ReflectionReference {"` |
|     - | 3027 | `" protected $__id = '';"` |
|     - | 3028 | `" public function __construct(){"` |
|     - | 3029 | `"  throw new Error('Call to private ReflectionReference::__construct() from global scope');"` |
|     - | 3030 | `" }"` |
|     - | 3031 | `" public static function fromArrayElement($array, $key){"` |
|     - | 3032 | `"  if(!is_array($array)){"` |
|     - | 3033 | `"   throw new TypeError('ReflectionReference::fromArrayElement(): Argument #1 ($array) must be of type array, '.get_debug_type($array).' given');"` |
|     - | 3034 | `"  }"` |
|     - | 3035 | `"  $id = __reflect_ref_id($array, $key);"` |
|     - | 3036 | `"  if($id === null){ return null; }"` |
|     - | 3037 | `"  $r = __reflect_new_no_ctor('ReflectionReference');"` |
|     - | 3038 | `"  $r->__setId('phlref'.$id);"` |
|     - | 3039 | `"  return $r;"` |
|     - | 3040 | `" }"` |
|     - | 3041 | `" public function __setId($id){ $this->__id = $id; }"` |
|     - | 3042 | `" public function getId(){ return $this->__id; }"` |
|     - | 3043 | `"}"` |
|     - | 3044 | `;` |
|     - | 3045 | `/*` |
|     - | 3046 | ` * Chunk 7: ReflectionAttribute and the shared getAttributes() builder.` |
|     - | 3047 | ` * The spec array rides as [kind, target, member, paramIdx]; argument` |
|     - | 3048 | ` * values evaluate lazily through __reflect_attr_args (PHP semantics).` |
|     - | 3049 | ` */` |
|     - | 3050 | `static const char zReflectLib7[] =` |
|     - | 3051 | `"function __reflect_has_deprecated($meta){"` |
|     - | 3052 | `" foreach($meta as $a){"` |
|     - | 3053 | `"  if(strtolower($a['name']) === 'deprecated'){ return true; }"` |
|     - | 3054 | `" }"` |
|     - | 3055 | `" return false;"` |
|     - | 3056 | `"}"` |
|     - | 3057 | `"function __reflect_target_names($mask){"` |
|     - | 3058 | `" $parts = array();"` |
|     - | 3059 | `" foreach(array('class' => 1, 'function' => 2, 'method' => 4, 'property' => 8,"` |
|     - | 3060 | `"  'class constant' => 16, 'parameter' => 32, 'constant' => 64) as $nm => $bit){"` |
|     - | 3061 | `"  if($mask & $bit){ $parts[] = $nm; }"` |
|     - | 3062 | `" }"` |
|     - | 3063 | `" return implode(', ', $parts);"` |
|     - | 3064 | `"}"` |
|     - | 3065 | `"function __reflect_build_attrs($meta, $spec, $target, $name, $flags){"` |
|     - | 3066 | `" $out = array();"` |
|     - | 3067 | `" $counts = array();"` |
|     - | 3068 | `" foreach($meta as $a){"` |
|     - | 3069 | `"  $k = strtolower($a['name']);"` |
|     - | 3070 | `"  $counts[$k] = isset($counts[$k]) ? $counts[$k] + 1 : 1;"` |
|     - | 3071 | `" }"` |
|     - | 3072 | `" $idx = 0;"` |
|     - | 3073 | `" foreach($meta as $a){"` |
|     - | 3074 | `"  $keep = true;"` |
|     - | 3075 | `"  if($name !== null){"` |
|     - | 3076 | `"   $keep = strtolower($a['name']) === strtolower($name);"` |
|     - | 3077 | `"   if(!$keep && ($flags & 2)){"` |
|     - | 3078 | `"    $keep = is_subclass_of($a['name'], $name);"` |
|     - | 3079 | `"   }"` |
|     - | 3080 | `"  }"` |
|     - | 3081 | `"  if($keep){"` |
|     - | 3082 | `"   $r = __reflect_new_no_ctor('ReflectionAttribute');"` |
|     - | 3083 | `"   $r->__init($a['name'], $spec, $idx, $target, $counts[strtolower($a['name'])] > 1);"` |
|     - | 3084 | `"   $out[] = $r;"` |
|     - | 3085 | `"  }"` |
|     - | 3086 | `"  $idx++;"` |
|     - | 3087 | `" }"` |
|     - | 3088 | `" return $out;"` |
|     - | 3089 | `"}"` |
|     - | 3090 | `"final class ReflectionAttribute {"` |
|     - | 3091 | `" const IS_INSTANCEOF = 2;"` |
|     - | 3092 | `" protected $__name = '';"` |
|     - | 3093 | `" protected $__spec = null;"` |
|     - | 3094 | `" protected $__idx = 0;"` |
|     - | 3095 | `" protected $__target = 0;"` |
|     - | 3096 | `" protected $__rep = false;"` |
|     - | 3097 | `" public function __construct(){"` |
|     - | 3098 | `"  throw new Error('Call to private ReflectionAttribute::__construct() from global scope');"` |
|     - | 3099 | `" }"` |
|     - | 3100 | `" public function __init($name, $spec, $idx, $target, $rep){"` |
|     - | 3101 | `"  $this->__name = $name;"` |
|     - | 3102 | `"  $this->__spec = $spec;"` |
|     - | 3103 | `"  $this->__idx = $idx;"` |
|     - | 3104 | `"  $this->__target = $target;"` |
|     - | 3105 | `"  $this->__rep = $rep;"` |
|     - | 3106 | `" }"` |
|     - | 3107 | `" public function getName(){ return $this->__name; }"` |
|     - | 3108 | `" public function getTarget(){ return $this->__target; }"` |
|     - | 3109 | `" public function isRepeated(){ return $this->__rep; }"` |
|     - | 3110 | `" public function getArguments(){"` |
|     - | 3111 | `"  $a = __reflect_attr_args($this->__spec[0], $this->__spec[1], $this->__spec[2], $this->__spec[3], $this->__idx);"` |
|     - | 3112 | `"  return $a === null ? array() : $a;"` |
|     - | 3113 | `" }"` |
|     - | 3114 | `" public function newInstance(){"` |
|     - | 3115 | `"  $name = $this->__name;"` |
|     - | 3116 | `"  $ci = __phl_rcinfo($name);"` |
|     - | 3117 | `"  if($ci === null){"` |
|     - | 3118 | `"   throw new Error('Attribute class \"'.$name.'\" not found');"` |
|     - | 3119 | `"  }"` |
|     - | 3120 | `"  $name = $ci['name'];"` |
|     - | 3121 | `"  $decl = null;"` |
|     - | 3122 | `"  $didx = 0;"` |
|     - | 3123 | `"  foreach($ci['attrs'] as $a){"` |
|     - | 3124 | `"   if(strtolower($a['name']) === 'attribute'){ $decl = $didx; break; }"` |
|     - | 3125 | `"   $didx++;"` |
|     - | 3126 | `"  }"` |
|     - | 3127 | `"  if($decl === null){"` |
|     - | 3128 | `"   throw new Error('Attempting to use non-attribute class \"'.$name.'\" as attribute');"` |
|     - | 3129 | `"  }"` |
|     - | 3130 | `"  $dargs = __reflect_attr_args('class', $name, null, 0, $decl);"` |
|     - | 3131 | `"  $flags = 127;"` |
|     - | 3132 | `"  if(is_array($dargs)){"` |
|     - | 3133 | `"   if(isset($dargs[0])){ $flags = $dargs[0]; }"` |
|     - | 3134 | `"   else if(isset($dargs['flags'])){ $flags = $dargs['flags']; }"` |
|     - | 3135 | `"  }"` |
|     - | 3136 | `"  if(($flags & $this->__target) === 0){"` |
|     - | 3137 | `"   $tnames = array(1 => 'class', 2 => 'function', 4 => 'method', 8 => 'property',"` |
|     - | 3138 | `"    16 => 'class constant', 32 => 'parameter', 64 => 'constant');"` |
|     - | 3139 | `"   throw new Error('Attribute \"'.$name.'\" cannot target '.$tnames[$this->__target]"` |
|     - | 3140 | `"    .' (allowed targets: '.__reflect_target_names($flags).')');"` |
|     - | 3141 | `"  }"` |
|     - | 3142 | `"  if($this->__rep && ($flags & 128) === 0){"` |
|     - | 3143 | `"   throw new Error('Attribute \"'.$name.'\" must not be repeated');"` |
|     - | 3144 | `"  }"` |
|     - | 3145 | `"  return __reflect_new_instance($name, $this->getArguments());"` |
|     - | 3146 | `" }"` |
|     - | 3147 | `" public function __toString(){"` |
|     - | 3148 | `"  return 'Attribute [ '.$this->__name.' ]';"` |
|     - | 3149 | `" }"` |
|     - | 3150 | `"}"` |
|     - | 3151 | `;` |
|     - | 3152 | `/*` |
|     - | 3153 | ` * Chunk 8: signature-table support. Internal (C builtin) functions carry a` |
|     - | 3154 | ` * PHP-style parameter-list string; these helpers parse it into the same` |
|     - | 3155 | ` * param-meta shape user functions get, so ReflectionFunction and` |
|     - | 3156 | ` * ReflectionParameter work uniformly over builtins.` |
|     - | 3157 | ` */` |
|     - | 3158 | `static const char zReflectLib8[] =` |
|     - | 3159 | `"function __reflect_sig_split($sig){"` |
|     - | 3160 | `" $parts = array();"` |
|     - | 3161 | `" $cur = '';"` |
|     - | 3162 | `" $q = false;"` |
|     - | 3163 | `" $n = strlen($sig);"` |
|     - | 3164 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3165 | `"  $ch = $sig[$k];"` |
|     - | 3166 | `"  if($q){"` |
|     - | 3167 | `"   $cur .= $ch;"` |
|     - | 3168 | `"   if($ch === chr(92) && $k + 1 < $n){ $cur .= $sig[$k+1]; $k++; }"` |
|     - | 3169 | `"   else if($ch === chr(39)){ $q = false; }"` |
|     - | 3170 | `"  }else if($ch === chr(39)){ $q = true; $cur .= $ch; }"` |
|     - | 3171 | `"  else if($ch === ',' ){ $parts[] = trim($cur); $cur = ''; }"` |
|     - | 3172 | `"  else{ $cur .= $ch; }"` |
|     - | 3173 | `" }"` |
|     - | 3174 | `" if(trim($cur) !== ''){ $parts[] = trim($cur); }"` |
|     - | 3175 | `" return $parts;"` |
|     - | 3176 | `"}"` |
|     - | 3177 | `"function __reflect_sig_scalar($t){"` |
|     - | 3178 | `" if($t === '?'){ return array(false, null); }"` |
|     - | 3179 | `" if($t === 'NULL' \|\| $t === 'null'){ return array(true, null); }"` |
|     - | 3180 | `" if($t === 'true'){ return array(true, true); }"` |
|     - | 3181 | `" if($t === 'false'){ return array(true, false); }"` |
|     - | 3182 | `" if(is_numeric($t)){"` |
|     - | 3183 | `"  if(strpos($t, '.') === false && stripos($t, 'e') === false && strpos($t, 'x') === false){"` |
|     - | 3184 | `"   return array(true, (int)$t);"` |
|     - | 3185 | `"  }"` |
|     - | 3186 | `"  return array(true, (float)$t);"` |
|     - | 3187 | `" }"` |
|     - | 3188 | `" if(strlen($t) >= 2 && $t[0] === chr(39) && $t[strlen($t)-1] === chr(39)){"` |
|     - | 3189 | `"  $body = substr($t, 1, strlen($t) - 2);"` |
|     - | 3190 | `"  return array(true, strtr($body, array(chr(92).chr(39) => chr(39), chr(92).chr(92) => chr(92))));"` |
|     - | 3191 | `" }"` |
|     - | 3192 | `" return array(false, null);"` |
|     - | 3193 | `"}"` |
|     - | 3194 | `"function __reflect_parse_sig($sig){"` |
|     - | 3195 | `" $params = array();"` |
|     - | 3196 | `" $pos = 0;"` |
|     - | 3197 | `" foreach(__reflect_sig_split($sig) as $part){"` |
|     - | 3198 | `"  $deftext = null;"` |
|     - | 3199 | `"  $q = false;"` |
|     - | 3200 | `"  $n = strlen($part);"` |
|     - | 3201 | `"  for($k = 0; $k < $n; $k++){"` |
|     - | 3202 | `"   $ch = $part[$k];"` |
|     - | 3203 | `"   if($q){"` |
|     - | 3204 | `"    if($ch === chr(92)){ $k++; }"` |
|     - | 3205 | `"    else if($ch === chr(39)){ $q = false; }"` |
|     - | 3206 | `"   }else if($ch === chr(39)){ $q = true; }"` |
|     - | 3207 | `"   else if($ch === '=' ){"` |
|     - | 3208 | `"    $deftext = trim(substr($part, $k + 1));"` |
|     - | 3209 | `"    $part = trim(substr($part, 0, $k));"` |
|     - | 3210 | `"    break;"` |
|     - | 3211 | `"   }"` |
|     - | 3212 | `"  }"` |
|     - | 3213 | `"  $variadic = strpos($part, '...') !== false;"` |
|     - | 3214 | `"  $byref = strpos($part, '&') !== false;"` |
|     - | 3215 | `"  $d = strpos($part, '$');"` |
|     - | 3216 | `"  $name = $d === false ? $part : substr($part, $d + 1);"` |
|     - | 3217 | `"  $typetext = null;"` |
|     - | 3218 | `"  $sp = strpos($part, ' ');"` |
|     - | 3219 | `"  if($sp !== false && $d !== false && $sp < $d){ $typetext = substr($part, 0, $sp); }"` |
|     - | 3220 | `"  $nullable = $typetext !== null && ($typetext[0] === '?' \|\| stripos($typetext, 'null') !== false);"` |
|     - | 3221 | `"  $params[] = array('name' => $name, 'pos' => $pos, 'byref' => $byref,"` |
|     - | 3222 | `"   'variadic' => $variadic, 'hasdef' => $deftext !== null, 'nullable' => $nullable,"` |
|     - | 3223 | `"   'promoted' => false, 'typetext' => $typetext, 'attrs' => array(), 'deftext' => $deftext);"` |
|     - | 3224 | `"  $pos++;"` |
|     - | 3225 | `" }"` |
|     - | 3226 | `" return $params;"` |
|     - | 3227 | `"}"` |
|     - | 3228 | `"function __reflect_sig_fixup($i){"` |
|     - | 3229 | `" if($i === null){ return $i; }"` |
|     - | 3230 | `" if(isset($i['ret2'])){ $i['rettext'] = $i['ret2']; }"` |
|     - | 3231 | `" if(!isset($i['sig']) \|\| $i['sig'] === ''){ return $i; }"` |
|     - | 3232 | `" $i['params'] = __reflect_parse_sig($i['sig']);"` |
|     - | 3233 | `" $i['minarg'] = -1;"` |
|     - | 3234 | `" $v = false;"` |
|     - | 3235 | `" foreach($i['params'] as $p){ if($p['variadic']){ $v = true; } }"` |
|     - | 3236 | `" $i['variadic'] = $v;"` |
|     - | 3237 | `" return $i;"` |
|     - | 3238 | `"}"` |
|     - | 3239 | `;` |
|     - | 3240 | `/*` |
|     - | 3241 | ` * Chunk 9: PHP's Reflection export format (__toString on every Reflector).` |
|     - | 3242 | ` * Built entirely from the public reflection API of the target objects.` |
|     - | 3243 | ` */` |
|     - | 3244 | `static const char zReflectLib9[] =` |
|     - | 3245 | `"function __reflect_export_value($v){"` |
|     - | 3246 | `" if($v === null){ return 'NULL'; }"` |
|     - | 3247 | `" if($v === true){ return 'true'; }"` |
|     - | 3248 | `" if($v === false){ return 'false'; }"` |
|     - | 3249 | `" if(is_string($v)){ return chr(39).$v.chr(39); }"` |
|     - | 3250 | `" if(is_array($v)){"` |
|     - | 3251 | `"  $parts = array();"` |
|     - | 3252 | `"  $isList = true;"` |
|     - | 3253 | `"  $next = 0;"` |
|     - | 3254 | `"  foreach($v as $k => $x){"` |
|     - | 3255 | `"   if($k !== $next){ $isList = false; break; }"` |
|     - | 3256 | `"   $next++;"` |
|     - | 3257 | `"  }"` |
|     - | 3258 | `"  foreach($v as $k => $x){"` |
|     - | 3259 | `"   $parts[] = $isList ? __reflect_export_value($x)"` |
|     - | 3260 | `"    : (__reflect_export_value($k).' => '.__reflect_export_value($x));"` |
|     - | 3261 | `"  }"` |
|     - | 3262 | `"  return '['.implode(', ', $parts).']';"` |
|     - | 3263 | `" }"` |
|     - | 3264 | `" return (string)$v;"` |
|     - | 3265 | `"}"` |
|     - | 3266 | `"function __reflect_export_param($p){"` |
|     - | 3267 | `" $s = 'Parameter #'.$p->getPosition().' [ <'.($p->isOptional() ? 'optional' : 'required').'> ';"` |
|     - | 3268 | `" $t = $p->getType();"` |
|     - | 3269 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3270 | `" if($p->isPassedByReference()){ $s .= '&'; }"` |
|     - | 3271 | `" if($p->isVariadic()){ $s .= '...'; }"` |
|     - | 3272 | `" $s .= '$'.$p->getName();"` |
|     - | 3273 | `" if($p->isDefaultValueAvailable()){"` |
|     - | 3274 | `"  try{ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3275 | `"  catch(ReflectionException $e){ $s .= ' = <default>'; }"` |
|     - | 3276 | `" }"` |
|     - | 3277 | `" return $s.' ]';"` |
|     - | 3278 | `"}"` |
|     - | 3279 | `"function __reflect_export_prop($p){"` |
|     - | 3280 | `" $s = 'Property [ ';"` |
|     - | 3281 | `" $s .= $p->isPrivate() ? 'private ' : ($p->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3282 | `" if($p->isStatic()){ $s .= 'static '; }"` |
|     - | 3283 | `" if($p->isReadOnly()){ $s .= 'readonly '; }"` |
|     - | 3284 | `" $t = $p->getType();"` |
|     - | 3285 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3286 | `" $s .= '$'.$p->getName();"` |
|     - | 3287 | `" if($p->hasDefaultValue()){ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3288 | `" return $s.' ]'.chr(10);"` |
|     - | 3289 | `"}"` |
|     - | 3290 | `"function __reflect_export_cconst($c){"` |
|     - | 3291 | `" $v = $c->getValue();"` |
|     - | 3292 | `" if(is_int($v)){ $t = 'int'; }"` |
|     - | 3293 | `" else if(is_string($v)){ $t = 'string'; }"` |
|     - | 3294 | `" else if(is_float($v)){ $t = 'float'; }"` |
|     - | 3295 | `" else if(is_bool($v)){ $t = 'bool'; }"` |
|     - | 3296 | `" else if(is_array($v)){ $t = 'array'; }"` |
|     - | 3297 | `" else{ $t = 'null'; }"` |
|     - | 3298 | `" $vs = is_array($v) ? 'Array' : (is_bool($v) ? ($v ? '1' : '') : (string)$v);"` |
|     - | 3299 | `" $vis = $c->isPrivate() ? 'private' : ($c->isProtected() ? 'protected' : 'public');"` |
|     - | 3300 | `" return 'Constant [ '.$vis.' '.$t.' '.$c->name.' ] { '.$vs.' }'.chr(10);"` |
|     - | 3301 | `"}"` |
|     - | 3302 | `"function __reflect_export_fnabs($r, $indent){"` |
|     - | 3303 | `" $tags = $r->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3304 | `" if($r instanceof ReflectionMethod){"` |
|     - | 3305 | `"  if($r->isConstructor()){ $tags .= ', ctor'; }"` |
|     - | 3306 | `"  else if($r->isDestructor()){ $tags .= ', dtor'; }"` |
|     - | 3307 | `"  $decl = $r->getDeclaringClass()->name;"` |
|     - | 3308 | `"  if(strtolower($decl) !== strtolower($r->class)){ $tags .= ', inherits '.$decl; }"` |
|     - | 3309 | `"  else if($r->hasPrototype()){ $tags .= ', prototype '.$r->getPrototype()->class; }"` |
|     - | 3310 | `"  $head = 'Method [ <'.$tags.'> ';"` |
|     - | 3311 | `"  if($r->isAbstract()){ $head .= 'abstract '; }"` |
|     - | 3312 | `"  if($r->isFinal()){ $head .= 'final '; }"` |
|     - | 3313 | `"  if($r->isStatic()){ $head .= 'static '; }"` |
|     - | 3314 | `"  $head .= $r->isPrivate() ? 'private ' : ($r->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3315 | `"  $head .= 'method '.$r->name.' ]';"` |
|     - | 3316 | `" }else{"` |
|     - | 3317 | `"  $kind = $r->isClosure() ? 'Closure' : 'Function';"` |
|     - | 3318 | `"  $head = $kind.' [ <'.$tags.'> function '.$r->name.' ]';"` |
|     - | 3319 | `" }"` |
|     - | 3320 | `" $s = $head.' {'.chr(10);"` |
|     - | 3321 | `" if(!$r->isInternal()){"` |
|     - | 3322 | `"  $s .= '  @@ '.$r->getFileName().' '.$r->getStartLine().' - '.$r->getEndLine().chr(10);"` |
|     - | 3323 | `" }"` |
|     - | 3324 | `" $ps = $r->getParameters();"` |
|     - | 3325 | `" $ret = $r->getReturnType();"` |
|     - | 3326 | `" if(count($ps) > 0 \|\| $ret !== null){"` |
|     - | 3327 | `"  $s .= chr(10).'  - Parameters ['.count($ps).'] {'.chr(10);"` |
|     - | 3328 | `"  foreach($ps as $p){ $s .= '    '.__reflect_export_param($p).chr(10); }"` |
|     - | 3329 | `"  $s .= '  }'.chr(10);"` |
|     - | 3330 | `" }"` |
|     - | 3331 | `" if($ret !== null){ $s .= '  - Return [ '.(string)$ret.' ]'.chr(10); }"` |
|     - | 3332 | `" $s .= '}'.chr(10);"` |
|     - | 3333 | `" if($indent === ''){ return $s; }"` |
|     - | 3334 | `" $lines = explode(chr(10), $s);"` |
|     - | 3335 | `" $out = '';"` |
|     - | 3336 | `" $n = count($lines);"` |
|     - | 3337 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3338 | `"  if($lines[$k] === '' && $k === $n - 1){ break; }"` |
|     - | 3339 | `"  $out .= ($lines[$k] === '' ? '' : $indent.$lines[$k]).chr(10);"` |
|     - | 3340 | `" }"` |
|     - | 3341 | `" return $out;"` |
|     - | 3342 | `"}"` |
|     - | 3343 | `"function __reflect_export_class($rc){"` |
|     - | 3344 | `" $tags = $rc->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3345 | `" if($rc->isInterface()){"` |
|     - | 3346 | `"  $head = 'Interface [ <'.$tags.'> interface '.$rc->name.' ]';"` |
|     - | 3347 | `" }else{"` |
|     - | 3348 | `"  $mods = '';"` |
|     - | 3349 | `"  if($rc->isAbstract()){ $mods .= 'abstract '; }"` |
|     - | 3350 | `"  if($rc->isFinal()){ $mods .= 'final '; }"` |
|     - | 3351 | `"  $head = 'Class [ <'.$tags.'> '.$mods.'class '.$rc->name;"` |
|     - | 3352 | `"  $par = $rc->getParentClass();"` |
|     - | 3353 | `"  if($par !== false){ $head .= ' extends '.$par->name; }"` |
|     - | 3354 | `"  $ifs = $rc->getInterfaceNames();"` |
|     - | 3355 | `"  if(count($ifs) > 0){ $head .= ' implements '.implode(', ', $ifs); }"` |
|     - | 3356 | `"  $head .= ' ]';"` |
|     - | 3357 | `" }"` |
|     - | 3358 | `" $s = $head.' {'.chr(10);"` |
|     - | 3359 | `" if(!$rc->isInternal()){"` |
|     - | 3360 | `"  $s .= '  @@ '.$rc->getFileName().' '.$rc->getStartLine().'-'.$rc->getEndLine().chr(10);"` |
|     - | 3361 | `" }"` |
|     - | 3362 | `" $consts = $rc->getReflectionConstants();"` |
|     - | 3363 | `" $s .= chr(10).'  - Constants ['.count($consts).'] {'.chr(10);"` |
|     - | 3364 | `" foreach($consts as $c){ $s .= '    '.__reflect_export_cconst($c); }"` |
|     - | 3365 | `" $s .= '  }'.chr(10);"` |
|     - | 3366 | `" $sp = array();"` |
|     - | 3367 | `" $ip = array();"` |
|     - | 3368 | `" foreach($rc->getProperties() as $p){"` |
|     - | 3369 | `"  if($p->isStatic()){ $sp[] = $p; }else{ $ip[] = $p; }"` |
|     - | 3370 | `" }"` |
|     - | 3371 | `" $sm = array();"` |
|     - | 3372 | `" $im = array();"` |
|     - | 3373 | `" foreach($rc->getMethods() as $m){"` |
|     - | 3374 | `"  if($m->isStatic()){ $sm[] = $m; }else{ $im[] = $m; }"` |
|     - | 3375 | `" }"` |
|     - | 3376 | `" $s .= chr(10).'  - Static properties ['.count($sp).'] {'.chr(10);"` |
|     - | 3377 | `" foreach($sp as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3378 | `" $s .= '  }'.chr(10);"` |
|     - | 3379 | `" $s .= chr(10).'  - Static methods ['.count($sm).'] {'.chr(10);"` |
|     - | 3380 | `" $first = true;"` |
|     - | 3381 | `" foreach($sm as $m){"` |
|     - | 3382 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3383 | `"  $first = false;"` |
|     - | 3384 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3385 | `" }"` |
|     - | 3386 | `" $s .= '  }'.chr(10);"` |
|     - | 3387 | `" $s .= chr(10).'  - Properties ['.count($ip).'] {'.chr(10);"` |
|     - | 3388 | `" foreach($ip as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3389 | `" $s .= '  }'.chr(10);"` |
|     - | 3390 | `" $s .= chr(10).'  - Methods ['.count($im).'] {'.chr(10);"` |
|     - | 3391 | `" $first = true;"` |
|     - | 3392 | `" foreach($im as $m){"` |
|     - | 3393 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3394 | `"  $first = false;"` |
|     - | 3395 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3396 | `" }"` |
|     - | 3397 | `" $s .= '  }'.chr(10);"` |
|     - | 3398 | `" return $s.'}'.chr(10);"` |
|     - | 3399 | `"}"` |
|     - | 3400 | `;` |
|     - | 3401 | `/*` |
|     - | 3402 | ` * Register the __reflect_* thunks and compile the Reflection library.` |
|     - | 3403 | ` * Called from PH7_VmInit while pVm->bCompilingBuiltin is set, right after` |
|     - | 3404 | ` * the core builtin chunks (Exception and friends must exist already).` |
|     - | 3405 | ` */` |
|  3880 | 3406 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|     5 | 3407 | `{` |
|     - | 3408 | `	static const struct {` |
|     - | 3409 | `		const char *zName;` |
|     - | 3410 | `		ProchHostFunction xFunc;` |
|     - | 3411 | `	} aFunc[] = {` |
|     - | 3412 | `		{ "__reflect_class_info",     vm_builtin_reflect_class_info },` |
|     - | 3413 | `		{ "__reflect_const_value",    vm_builtin_reflect_const_value },` |
|     - | 3414 | `		{ "__reflect_static_value",   vm_builtin_reflect_static_value },` |
|     - | 3415 | `		{ "__reflect_static_set",     vm_builtin_reflect_static_set },` |
|     - | 3416 | `		{ "__reflect_prop_default",   vm_builtin_reflect_prop_default },` |
|     - | 3417 | `		{ "__reflect_new_instance",   vm_builtin_reflect_new_instance },` |
|     - | 3418 | `		{ "__reflect_new_no_ctor",    vm_builtin_reflect_new_no_ctor },` |
|     - | 3419 | `		{ "__reflect_func_info",      vm_builtin_reflect_func_info },` |
|     - | 3420 | `		{ "__reflect_param_default",  vm_builtin_reflect_param_default },` |
|     - | 3421 | `		{ "__reflect_param_defconst", vm_builtin_reflect_param_defconst },` |
|     - | 3422 | `		{ "__reflect_invoke",         vm_builtin_reflect_invoke },` |
|     - | 3423 | `		{ "__reflect_closure",        vm_builtin_reflect_closure },` |
|     - | 3424 | `		{ "__reflect_prop_read",      vm_builtin_reflect_prop_read },` |
|     - | 3425 | `		{ "__reflect_prop_write",     vm_builtin_reflect_prop_write },` |
|     - | 3426 | `		{ "__reflect_prop_state",     vm_builtin_reflect_prop_state },` |
|     - | 3427 | `		{ "__reflect_dyn_props",      vm_builtin_reflect_dyn_props },` |
|     - | 3428 | `		{ "__reflect_gen_info",       vm_builtin_reflect_gen_info },` |
|     - | 3429 | `		{ "__reflect_gen_exec",       vm_builtin_reflect_gen_exec },` |
|     - | 3430 | `		{ "__reflect_const_info",     vm_builtin_reflect_const_info },` |
|     - | 3431 | `		{ "__reflect_ref_id",         vm_builtin_reflect_ref_id },` |
|     - | 3432 | `		{ "__reflect_attr_args",      vm_builtin_reflect_attr_args },` |
|     - | 3433 | `	};` |
|     - | 3434 | `	sxu32 n;` |
|     - | 3435 | `	sxi32 rc;` |
| 85365 | 3436 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 81485 | 3437 | `		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);` |
| 40745 | 3438 | `	}` |
|  3885 | 3439 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib1, sizeof(zReflectLib1)-1);` |
|  3885 | 3440 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3441 | `		return rc;` |
|     - | 3442 | `	}` |
|  3885 | 3443 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib2, sizeof(zReflectLib2)-1);` |
|  3885 | 3444 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3445 | `		return rc;` |
|     - | 3446 | `	}` |
|  3885 | 3447 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib3, sizeof(zReflectLib3)-1);` |
|  3885 | 3448 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3449 | `		return rc;` |
|     - | 3450 | `	}` |
|  3885 | 3451 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib4, sizeof(zReflectLib4)-1);` |
|  3885 | 3452 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3453 | `		return rc;` |
|     - | 3454 | `	}` |
|  3885 | 3455 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib5, sizeof(zReflectLib5)-1);` |
|  3885 | 3456 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3457 | `		return rc;` |
|     - | 3458 | `	}` |
|  3885 | 3459 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib6, sizeof(zReflectLib6)-1);` |
|  3885 | 3460 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3461 | `		return rc;` |
|     - | 3462 | `	}` |
|  3885 | 3463 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib7, sizeof(zReflectLib7)-1);` |
|  3885 | 3464 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3465 | `		return rc;` |
|     - | 3466 | `	}` |
|  3885 | 3467 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib8, sizeof(zReflectLib8)-1);` |
|  3885 | 3468 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3469 | `		return rc;` |
|     - | 3470 | `	}` |
|  3885 | 3471 | `	return PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib9, sizeof(zReflectLib9)-1);` |
|  1945 | 3472 | `}` |
|     - | 3473 |  |
