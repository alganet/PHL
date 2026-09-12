# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1037/1220 lines (85.00%)

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
|     3 |   32 | `{` |
|     - |   33 | `	ph7_class *pClass;` |
|   681 |   34 | `	pClass = PH7_VmExtractClassFromValue(pVm, pArg);` |
|   681 |   35 | `	if( pClass == 0 && ph7_value_is_string(pArg) ){` |
|     - |   36 | `		const char *zName;` |
|     - |   37 | `		int nLen;` |
|    16 |   38 | `		zName = ph7_value_to_string(pArg, &nLen);` |
|    16 |   39 | `		if( nLen > 0 ){` |
|    16 |   40 | `			pClass = PH7_VmTriggerAutoload(pVm, zName, (sxu32)nLen, FALSE);` |
|     7 |   41 | `		}` |
|     7 |   42 | `	}` |
|   681 |   43 | `	return pClass;` |
|     3 |   44 | `}` |
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
| 17094 |   63 | `static void ReflectMapAddBool(ph7_context *pCtx, ph7_value *pMap, const char *zKey, int b)` |
|     3 |   64 | `{` |
| 17097 |   65 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 17097 |   66 | `	if( p == 0 ){ return; }` |
| 17097 |   67 | `	ph7_value_bool(p, b);` |
| 17097 |   68 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  8550 |   69 | `}` |
|  5132 |   70 | `static void ReflectMapAddInt(ph7_context *pCtx, ph7_value *pMap, const char *zKey, sxi64 iVal)` |
|     3 |   71 | `{` |
|  5135 |   72 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  5135 |   73 | `	if( p == 0 ){ return; }` |
|  5135 |   74 | `	ph7_value_int64(p, iVal);` |
|  5135 |   75 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  2569 |   76 | `}` |
|  4970 |   77 | `static void ReflectMapAddStr(ph7_context *pCtx, ph7_value *pMap, const char *zKey,` |
|     - |   78 | `	const char *zVal, int nVal)` |
|     3 |   79 | `{` |
|  4973 |   80 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  4973 |   81 | `	if( p == 0 ){ return; }` |
|  4973 |   82 | `	ph7_value_string(p, zVal, nVal);` |
|  4973 |   83 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  2488 |   84 | `}` |
|  1614 |   85 | `static void ReflectMapAddNull(ph7_context *pCtx, ph7_value *pMap, const char *zKey)` |
|     2 |   86 | `{` |
|  1616 |   87 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  1616 |   88 | `	if( p == 0 ){ return; }` |
|  1616 |   89 | `	ph7_value_null(p);` |
|  1616 |   90 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|   809 |   91 | `}` |
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
|  2190 |  104 | `static void ReflectMapAddAttrs(ph7_context *pCtx, ph7_value *pMap, SySet *pAttrs)` |
|     2 |  105 | `{` |
|  2192 |  106 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|  2192 |  107 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|     - |  108 | `	sxu32 n;` |
|  2192 |  109 | `	if( pList == 0 ){` |
|   ! 0 |  110 | `		return;` |
|     - |  111 | `	}` |
|  2302 |  112 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|   111 |  113 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|   111 |  114 | `		if( pMeta == 0 ){ break; }` |
|   111 |  115 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aA[n].sName), (int)SyStringLength(&aA[n].sName));` |
|   111 |  116 | `		ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)aA[n].nLine);` |
|   111 |  117 | `		ph7_array_add_elem(pList, 0, pMeta);` |
|    56 |  118 | `	}` |
|  2192 |  119 | `	ph7_array_add_strkey_elem(pMap, "attrs", pList);` |
|  1097 |  120 | `}` |
|     - |  121 | `/* Emit a doc-comment field: the text when present, else boolean false` |
|     - |  122 | ` * (getDocComment()'s exact return contract). */` |
|   900 |  123 | `static void ReflectMapAddDoc(ph7_context *pCtx, ph7_value *pMap, const SyString *pDoc)` |
|     2 |  124 | `{` |
|   902 |  125 | `	if( SyStringLength(pDoc) > 0 ){` |
|    21 |  126 | `		ReflectMapAddStr(pCtx, pMap, "doc", SyStringData(pDoc), (int)SyStringLength(pDoc));` |
|    11 |  127 | `	}else{` |
|   882 |  128 | `		ReflectMapAddBool(pCtx, pMap, "doc", 0);` |
|     - |  129 | `	}` |
|   902 |  130 | `}` |
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
|     2 |  165 | `{` |
|     - |  166 | `	ph7_class **apIface;` |
|     - |  167 | `	sxu32 n;` |
|   164 |  168 | `	if( pClass == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|   ! 0 |  169 | `		return;` |
|     - |  170 | `	}` |
|   164 |  171 | `	if( pClass->pBase ){` |
|    32 |  172 | `		ReflectCollectInterfaces(pClass->pBase, pOut, iDepth + 1);` |
|    15 |  173 | `	}` |
|   164 |  174 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   202 |  175 | `	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){` |
|    40 |  176 | `		ReflectAddInterface(apIface[n], pOut, iDepth + 1);` |
|    21 |  177 | `	}` |
|    83 |  178 | `}` |
|     - |  179 | `/*` |
|     - |  180 | ` * Deepest base class whose method table maps the same name to the very` |
|     - |  181 | ` * same ph7_class_method pointer: inheritance shares member pointers` |
|     - |  182 | ` * (PH7_ClassInherit), so this identifies the declaring class. Methods` |
|     - |  183 | ` * copied in from traits are not on the pBase chain and thus report the` |
|     - |  184 | ` * using class, which is what PHP reports too.` |
|     - |  185 | ` */` |
|   758 |  186 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth)` |
|     1 |  187 | `{` |
|   759 |  188 | `	ph7_class *pDecl = pClass;` |
|   759 |  189 | `	ph7_class *pBase = pClass->pBase;` |
|   759 |  190 | `	int iDepth = 0;` |
|  1081 |  191 | `	while( pBase && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
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
|   759 |  202 | `	return pDecl;` |
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
|     3 |  234 | `{` |
|   149 |  235 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  236 | `	ph7_class *pClass;` |
|     - |  237 | `	ph7_value *pInfo, *pConsts, *pProps, *pMethods, *pList;` |
|     - |  238 | `	SyHashEntry *pEntry;` |
|     - |  239 | `	SySet aIfaceSet;` |
|   149 |  240 | `	sxi32 iCtorVis = 0, iCloneVis = 0;` |
|   149 |  241 | `	int bIterable = 0;` |
|     - |  242 | `	sxu32 n;` |
|   149 |  243 | `	if( nArg < 1 ){` |
|   ! 0 |  244 | `		ph7_result_null(pCtx);` |
|   ! 0 |  245 | `		return PH7_OK;` |
|     - |  246 | `	}` |
|   149 |  247 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|   149 |  248 | `	if( pClass == 0 ){` |
|    16 |  249 | `		ph7_result_null(pCtx);` |
|    16 |  250 | `		return PH7_OK;` |
|     - |  251 | `	}` |
|   134 |  252 | `	pInfo = ph7_context_new_array(pCtx);` |
|   134 |  253 | `	pConsts = ph7_context_new_array(pCtx);` |
|   134 |  254 | `	pProps = ph7_context_new_array(pCtx);` |
|   134 |  255 | `	pMethods = ph7_context_new_array(pCtx);` |
|   134 |  256 | `	if( pInfo == 0 \|\| pConsts == 0 \|\| pProps == 0 \|\| pMethods == 0 ){` |
|   ! 0 |  257 | `		ph7_result_null(pCtx);` |
|   ! 0 |  258 | `		return PH7_OK;` |
|     - |  259 | `	}` |
|   134 |  260 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   134 |  261 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pClass->iFlags & PH7_CLASS_INTERNAL) != 0);` |
|   134 |  262 | `	ReflectMapAddBool(pCtx, pInfo, "interface", (pClass->iFlags & PH7_CLASS_INTERFACE) != 0);` |
|   134 |  263 | `	ReflectMapAddBool(pCtx, pInfo, "trait", (pClass->iFlags & PH7_CLASS_TRAIT) != 0);` |
|   134 |  264 | `	ReflectMapAddBool(pCtx, pInfo, "abstract", (pClass->iFlags & PH7_CLASS_ABSTRACT) != 0);` |
|   134 |  265 | `	ReflectMapAddBool(pCtx, pInfo, "final", (pClass->iFlags & PH7_CLASS_FINAL) != 0);` |
|   134 |  266 | `	ReflectMapAddBool(pCtx, pInfo, "readonly", (pClass->iFlags & PH7_CLASS_READONLY) != 0);` |
|   134 |  267 | `	ReflectMapAddBool(pCtx, pInfo, "enum", (pClass->iFlags & PH7_CLASS_ENUM) != 0);` |
|   134 |  268 | `	if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|   ! 0 |  269 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "int", (int)sizeof("int")-1);` |
|   134 |  270 | `	}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|     3 |  271 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "string", (int)sizeof("string")-1);` |
|     2 |  272 | `	}else{` |
|   132 |  273 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "", 0);` |
|     - |  274 | `	}` |
|     - |  275 | `	{` |
|     - |  276 | `		/* Enum case names in declaration order (empty list for non-enums) */` |
|   134 |  277 | `		ph7_value *pCases = ph7_context_new_array(pCtx);` |
|   134 |  278 | `		if( pCases ){` |
|   134 |  279 | `			ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     - |  280 | `			sxu32 nCase;` |
|   140 |  281 | `			for( nCase = 0 ; nCase < SySetUsed(&pClass->aEnumCases) ; nCase++ ){` |
|     7 |  282 | `				ph7_value *pNm = ph7_context_new_scalar(pCtx);` |
|     7 |  283 | `				if( pNm ){` |
|     7 |  284 | `					ph7_value_string(pNm,apCase[nCase]->sName.zString,(int)apCase[nCase]->sName.nByte);` |
|     7 |  285 | `					ph7_array_add_elem(pCases,0,pNm);` |
|     3 |  286 | `				}` |
|     4 |  287 | `			}` |
|   134 |  288 | `			ph7_array_add_strkey_elem(pInfo,"cases",pCases);` |
|    66 |  289 | `		}` |
|     - |  290 | `	}` |
|   134 |  291 | `	if( pClass->pBase ){` |
|    41 |  292 | `		ReflectMapAddStr(pCtx, pInfo, "parent", SyStringData(&pClass->pBase->sName),` |
|    26 |  293 | `			(int)SyStringLength(&pClass->pBase->sName));` |
|    15 |  294 | `	}else{` |
|   108 |  295 | `		ReflectMapAddNull(pCtx, pInfo, "parent");` |
|     - |  296 | `	}` |
|     - |  297 | `	/* Transitive interfaces */` |
|   134 |  298 | `	SySetInit(&aIfaceSet, &pVm->sAllocator, sizeof(ph7_class *));` |
|   134 |  299 | `	ReflectCollectInterfaces(pClass, &aIfaceSet, 0);` |
|   134 |  300 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     - |  301 | `		/* An interface's own parents count as its interface list */` |
|    11 |  302 | `		if( pClass->pBase ){` |
|     3 |  303 | `			ReflectAddInterface(pClass->pBase, &aIfaceSet, 0);` |
|     1 |  304 | `		}` |
|     5 |  305 | `	}` |
|   134 |  306 | `	pList = ph7_context_new_array(pCtx);` |
|   134 |  307 | `	if( pList ){` |
|   134 |  308 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIfaceSet);` |
|   174 |  309 | `		for( n = 0 ; n < SySetUsed(&aIfaceSet) ; n++ ){` |
|    42 |  310 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|    42 |  311 | `			if( pName == 0 ){ break; }` |
|    42 |  312 | `			ph7_value_string(pName, SyStringData(&apIface[n]->sName), (int)SyStringLength(&apIface[n]->sName));` |
|    42 |  313 | `			ph7_array_add_elem(pList, 0, pName);` |
|    42 |  314 | `			if( pVm->pTraversableClass && apIface[n] == pVm->pTraversableClass ){` |
|     3 |  315 | `				bIterable = 1;` |
|     1 |  316 | `			}` |
|    22 |  317 | `		}` |
|   134 |  318 | `		ph7_array_add_strkey_elem(pInfo, "interfaces", pList);` |
|    66 |  319 | `	}` |
|   134 |  320 | `	SySetRelease(&aIfaceSet);` |
|   134 |  321 | `	ReflectMapAddBool(pCtx, pInfo, "iterable", bIterable);` |
|     - |  322 | `	/* Used traits */` |
|   134 |  323 | `	pList = ph7_context_new_array(pCtx);` |
|   134 |  324 | `	if( pList ){` |
|   134 |  325 | `		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|   136 |  326 | `		for( n = 0 ; n < SySetUsed(&pClass->aTrait) ; n++ ){` |
|     3 |  327 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|     3 |  328 | `			if( pName == 0 ){ break; }` |
|     3 |  329 | `			ph7_value_string(pName, SyStringData(&apTrait[n]->sName), (int)SyStringLength(&apTrait[n]->sName));` |
|     3 |  330 | `			ph7_array_add_elem(pList, 0, pName);` |
|     2 |  331 | `		}` |
|   134 |  332 | `		ph7_array_add_strkey_elem(pInfo, "traits", pList);` |
|    66 |  333 | `	}` |
|     - |  334 | `	/* File / lines: no file recorded => false, like PHP internals */` |
|   134 |  335 | `	if( SyStringLength(&pClass->sFile) > 0 ){` |
|   122 |  336 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));` |
|    62 |  337 | `	}else{` |
|    13 |  338 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - |  339 | `	}` |
|   134 |  340 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pClass->nLine);` |
|   134 |  341 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pClass->nEndLine);` |
|   134 |  342 | `	ReflectMapAddDoc(pCtx, pInfo, &pClass->sDoc);` |
|   134 |  343 | `	ReflectMapAddAttrs(pCtx, pInfo, &pClass->aAttrs);` |
|     - |  344 | `	/* Members are emitted in PHP's reporting order: the class's own members` |
|     - |  345 | `	 * first (declaration order), then each inheritance level's, outward.` |
|     - |  346 | `	 * Per level we iterate the DECLARING class's own hash — subclass hashes` |
|     - |  347 | `	 * interleave inherited pointers unpredictably — and emit buffered` |
|     - |  348 | `	 * entries in reverse, because SyHash lists are LIFO. A pointer-identity` |
|     - |  349 | `	 * lookup in the reflected class's hash filters out members that are not` |
|     - |  350 | `	 * visible there (base privates, overridden entries). */` |
|     - |  351 | `	{` |
|     - |  352 | `		ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];` |
|   134 |  353 | `		ph7_class *pWalk = pClass;` |
|     - |  354 | `		SySet aTmp;` |
|   134 |  355 | `		sxu32 nChain = 0, iLevel, nT;` |
|   296 |  356 | `		while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){` |
|   164 |  357 | `			aChain[nChain++] = pWalk;` |
|   164 |  358 | `			pWalk = pWalk->pBase;` |
|     2 |  359 | `		}` |
|   134 |  360 | `		SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));` |
|   296 |  361 | `		for( iLevel = 0 ; iLevel < nChain ; iLevel++ ){` |
|   164 |  362 | `			ph7_class *pLevel = aChain[iLevel];` |
|     - |  363 | `			/* --- Constants and properties (shared attribute table) --- */` |
|   164 |  364 | `			SySetReset(&aTmp);` |
|   164 |  365 | `			SyHashResetLoopCursor(&pLevel->hAttr);` |
|   392 |  366 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hAttr)) != 0 ){` |
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
|   344 |  385 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
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
|   164 |  422 | `			SySetReset(&aTmp);` |
|   164 |  423 | `			SyHashResetLoopCursor(&pLevel->hMethod);` |
|   444 |  424 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){` |
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
|   366 |  451 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
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
|    83 |  492 | `		}` |
|   134 |  493 | `		SySetRelease(&aTmp);` |
|     - |  494 | `	}` |
|   134 |  495 | `	ReflectMapAddInt(pCtx, pInfo, "ctorvis", (sxi64)iCtorVis);` |
|   134 |  496 | `	ReflectMapAddInt(pCtx, pInfo, "clonevis", (sxi64)iCloneVis);` |
|   134 |  497 | `	ph7_array_add_strkey_elem(pInfo, "consts", pConsts);` |
|   134 |  498 | `	ph7_array_add_strkey_elem(pInfo, "props", pProps);` |
|   134 |  499 | `	ph7_array_add_strkey_elem(pInfo, "methods", pMethods);` |
|   134 |  500 | `	ph7_result_value(pCtx, pInfo);` |
|   134 |  501 | `	return PH7_OK;` |
|    76 |  502 | `}` |
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
|   468 |  936 | `static ph7_class_instance * ReflectValueClosure(ph7_vm *pVm, ph7_value *pVal)` |
|     2 |  937 | `{` |
|     - |  938 | `	ph7_class_instance *pThis;` |
|   470 |  939 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
|   418 |  940 | `		return 0;` |
|     - |  941 | `	}` |
|    53 |  942 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    53 |  943 | `	return (pThis->pClass == pVm->pClosureClass) ? pThis : 0;` |
|   236 |  944 | `}` |
|     - |  945 | `/*` |
|     - |  946 | ` * Resolve a reflection callable target into its compiled function.` |
|     - |  947 | ` *   - pMethodArg a non-empty string  -> method mode: pTarget is a class name` |
|     - |  948 | ` *     or object; outputs *ppClass and *ppMeth.` |
|     - |  949 | ` *   - pTarget a Closure              -> unwrap $__fn into hFunction; *ppClosure.` |
|     - |  950 | ` *   - pTarget a string               -> hFunction (user) or hHostFunction` |
|     - |  951 | ` *     (*ppHost set, returns NULL).` |
|     - |  952 | ` * Returns the ph7_vm_func, or NULL (host function or unresolvable).` |
|     - |  953 | ` */` |
|   772 |  954 | `static ph7_vm_func * ReflectResolveCallable(ph7_context *pCtx, ph7_value *pTarget,` |
|     - |  955 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|     - |  956 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|     2 |  957 | `{` |
|   774 |  958 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  959 | `	SyHashEntry *pEntry;` |
|   774 |  960 | `	if( ppClass ){ *ppClass = 0; }` |
|   774 |  961 | `	if( ppMeth ){ *ppMeth = 0; }` |
|   774 |  962 | `	if( ppHost ){ *ppHost = 0; }` |
|   774 |  963 | `	if( ppClosure ){ *ppClosure = 0; }` |
|   774 |  964 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
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
|   470 |  994 | `		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|   470 |  995 | `		if( pClo ){` |
|     - |  996 | `			SyString sAttr;` |
|     - |  997 | `			ph7_value *pFn;` |
|    53 |  998 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    53 |  999 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|    53 | 1000 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) < 1 ){` |
|   ! 0 | 1001 | `				return 0;` |
|     - | 1002 | `			}` |
|    53 | 1003 | `			pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    53 | 1004 | `			if( pEntry == 0 ){` |
|     - | 1005 | `				/* A Closure over a host function (Closure::fromCallable('strlen')) */` |
|   ! 0 | 1006 | `				pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|   ! 0 | 1007 | `				if( pEntry && ppHost ){` |
|   ! 0 | 1008 | `					*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|   ! 0 | 1009 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|   ! 0 | 1010 | `				}` |
|   ! 0 | 1011 | `				return 0;` |
|     - | 1012 | `			}` |
|    53 | 1013 | `			if( ppClosure ){ *ppClosure = pClo; }` |
|    53 | 1014 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - | 1015 | `		}` |
|     - | 1016 | `	}` |
|   418 | 1017 | `	if( pTarget->iFlags & MEMOBJ_STRING ){` |
|   418 | 1018 | `		if( SyBlobLength(&pTarget->sBlob) < 1 ){` |
|   ! 0 | 1019 | `			return 0;` |
|     - | 1020 | `		}` |
|   418 | 1021 | `		pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   418 | 1022 | `		if( pEntry ){` |
|   285 | 1023 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - | 1024 | `		}` |
|   134 | 1025 | `		pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   134 | 1026 | `		if( pEntry && ppHost ){` |
|   130 | 1027 | `			*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|    64 | 1028 | `		}` |
|    66 | 1029 | `	}` |
|   134 | 1030 | `	return 0;` |
|   388 | 1031 | `}` |
|     - | 1032 | `/* Emit the shared descriptor fields of a compiled function. */` |
|   588 | 1033 | `static void ReflectFillFuncCommon(ph7_context *pCtx, ph7_value *pInfo, ph7_vm_func *pFunc)` |
|     1 | 1034 | `{` |
|     - | 1035 | `	ph7_vm_func_arg *aArg;` |
|     - | 1036 | `	ph7_value *pParams, *pStatics;` |
|   589 | 1037 | `	int bVariadic = 0;` |
|     - | 1038 | `	int bAnon;` |
|     - | 1039 | `	sxu32 n;` |
|     - | 1040 | ``	/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|     - | 1041 | `	 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|   589 | 1042 | `	bAnon = (pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|   588 | 1043 | `	if( !bAnon && SyStringLength(&pFunc->sName) > 9` |
|   318 | 1044 | `	 && (SyMemcmp(SyStringData(&pFunc->sName), "[lambda_", 8) == 0` |
|    88 | 1045 | `	  \|\| SyMemcmp(SyStringData(&pFunc->sName), "[closure_", 9) == 0) ){` |
|     5 | 1046 | `		bAnon = 1;` |
|     2 | 1047 | `	}` |
|   589 | 1048 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|   589 | 1049 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pFunc->iFlags & VM_FUNC_INTERNAL) != 0);` |
|   589 | 1050 | `	ReflectMapAddBool(pCtx, pInfo, "closure", bAnon);` |
|   589 | 1051 | `	ReflectMapAddBool(pCtx, pInfo, "fstatic", (pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);` |
|   589 | 1052 | `	ReflectMapAddBool(pCtx, pInfo, "byref", (pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|   589 | 1053 | `	ReflectMapAddBool(pCtx, pInfo, "generator", (pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|   589 | 1054 | `	ReflectMapAddBool(pCtx, pInfo, "strict", pFunc->bStrictTypes != 0);` |
|   589 | 1055 | `	if( SyStringLength(&pFunc->sFile) > 0 ){` |
|   585 | 1056 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pFunc->sFile), (int)SyStringLength(&pFunc->sFile));` |
|   293 | 1057 | `	}else{` |
|     5 | 1058 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1059 | `	}` |
|   589 | 1060 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pFunc->nLine);` |
|   589 | 1061 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pFunc->nEndLine);` |
|   589 | 1062 | `	ReflectMapAddDoc(pCtx, pInfo, &pFunc->sDoc);` |
|   589 | 1063 | `	ReflectMapAddAttrs(pCtx, pInfo, &pFunc->aAttrs);` |
|   589 | 1064 | `	if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|   145 | 1065 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", SyStringData(&pFunc->sReturnTypeName),` |
|    96 | 1066 | `			(int)SyStringLength(&pFunc->sReturnTypeName));` |
|   541 | 1067 | `	}else if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     - | 1068 | `		/* The type-text renderer omits void/never atoms (compile.c notes the` |
|     - | 1069 | `		 * root fix belongs there); name them here for getReturnType(). */` |
|     3 | 1070 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "void", sizeof("void")-1);` |
|   492 | 1071 | `	}else if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|     3 | 1072 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "never", sizeof("never")-1);` |
|     2 | 1073 | `	}else{` |
|   489 | 1074 | `		ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1075 | `	}` |
|   589 | 1076 | `	ReflectMapAddBool(pCtx, pInfo, "retnullable", (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) != 0);` |
|     - | 1077 | `	/* Parameters */` |
|   589 | 1078 | `	pParams = ph7_context_new_array(pCtx);` |
|   589 | 1079 | `	aArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|  1841 | 1080 | `	for( n = 0 ; pParams && n < SySetUsed(&pFunc->aArgs) ; n++ ){` |
|  1253 | 1081 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|  1253 | 1082 | `		if( pMeta == 0 ){ break; }` |
|  1253 | 1083 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aArg[n].sName), (int)SyStringLength(&aArg[n].sName));` |
|  1253 | 1084 | `		ReflectMapAddInt(pCtx, pMeta, "pos", (sxi64)n);` |
|  1253 | 1085 | `		ReflectMapAddBool(pCtx, pMeta, "byref", (aArg[n].iFlags & VM_FUNC_ARG_BY_REF) != 0);` |
|  1253 | 1086 | `		ReflectMapAddBool(pCtx, pMeta, "variadic", (aArg[n].iFlags & VM_FUNC_ARG_VARIADIC) != 0);` |
|     - | 1087 | `		/* The compiler never sets ARG_HAS_DEF; a default = compiled bytecode` |
|     - | 1088 | `		 * (same test the OP_CALL default-value path uses). */` |
|  1253 | 1089 | `		ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&aArg[n].aByteCode) > 0);` |
|  1253 | 1090 | `		ReflectMapAddBool(pCtx, pMeta, "nullable", (aArg[n].iFlags & VM_FUNC_ARG_NULLABLE) != 0);` |
|  1253 | 1091 | `		ReflectMapAddBool(pCtx, pMeta, "promoted", (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) != 0);` |
|  1253 | 1092 | `		if( SyStringLength(&aArg[n].sTypeName) > 0 ){` |
|   715 | 1093 | `			ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&aArg[n].sTypeName),` |
|   476 | 1094 | `				(int)SyStringLength(&aArg[n].sTypeName));` |
|   239 | 1095 | `		}else{` |
|   777 | 1096 | `			ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - | 1097 | `		}` |
|  1253 | 1098 | `		ReflectMapAddAttrs(pCtx, pMeta, &aArg[n].aAttrs);` |
|  1253 | 1099 | `		ph7_array_add_elem(pParams, 0, pMeta);` |
|  1253 | 1100 | `		if( aArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|   183 | 1101 | `			bVariadic = 1;` |
|    91 | 1102 | `		}` |
|   627 | 1103 | `	}` |
|   589 | 1104 | `	if( pParams ){` |
|   589 | 1105 | `		ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|   294 | 1106 | `	}` |
|   589 | 1107 | `	ReflectMapAddBool(pCtx, pInfo, "variadic", bVariadic);` |
|     - | 1108 | `	/* Static variables: current value when the slot was initialized (first` |
|     - | 1109 | `	 * call), otherwise the evaluated default — PHP's getStaticVariables` |
|     - | 1110 | `	 * initializes on demand and reports the same values. */` |
|   589 | 1111 | `	pStatics = ph7_context_new_array(pCtx);` |
|   589 | 1112 | `	if( pStatics ){` |
|   589 | 1113 | `		ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|   617 | 1114 | `		for( n = 0 ; n < SySetUsed(&pFunc->aStatic) ; n++ ){` |
|    29 | 1115 | `			ph7_value *pVal = 0;` |
|     - | 1116 | `			ph7_value sScratch;` |
|    29 | 1117 | `			int bScratch = 0;` |
|    29 | 1118 | `			if( aStatic[n].nIdx != SXU32_HIGH ){` |
|    11 | 1119 | `				pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);` |
|     5 | 1120 | `			}` |
|    29 | 1121 | `			if( pVal == 0 ){` |
|    19 | 1122 | `				PH7_MemObjInit(pCtx->pVm, &sScratch);` |
|    19 | 1123 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|    19 | 1124 | `					VmLocalExec(pCtx->pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|     9 | 1125 | `				}` |
|    19 | 1126 | `				pVal = &sScratch;` |
|    19 | 1127 | `				bScratch = 1;` |
|     9 | 1128 | `			}` |
|    29 | 1129 | `			ReflectMapAddDyn(pCtx, pStatics, &aStatic[n].sName, pVal);` |
|    29 | 1130 | `			if( bScratch ){` |
|    19 | 1131 | `				PH7_MemObjRelease(&sScratch);` |
|     9 | 1132 | `			}` |
|    15 | 1133 | `		}` |
|   589 | 1134 | `		ph7_array_add_strkey_elem(pInfo, "statics", pStatics);` |
|   294 | 1135 | `	}` |
|   589 | 1136 | `}` |
|     - | 1137 | `/*` |
|     - | 1138 | ` * array\|null __reflect_func_info(string\|Closure $target [, string $method])` |
|     - | 1139 | ` * Function/method/closure descriptor for the PHP layer.` |
|     - | 1140 | ` */` |
|   720 | 1141 | `static int vm_builtin_reflect_func_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 1142 | `{` |
|     - | 1143 | `	ph7_vm_func *pFunc;` |
|   722 | 1144 | `	ph7_class *pClass = 0;` |
|   722 | 1145 | `	ph7_class_method *pMeth = 0;` |
|   722 | 1146 | `	ph7_user_func *pHost = 0;` |
|   722 | 1147 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1148 | `	ph7_value *pInfo;` |
|   722 | 1149 | `	if( nArg < 1 ){` |
|   ! 0 | 1150 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1151 | `		return PH7_OK;` |
|     - | 1152 | `	}` |
|   722 | 1153 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], nArg > 1 ? apArg[1] : 0,` |
|     - | 1154 | `		&pClass, &pMeth, &pHost, &pClosure);` |
|   722 | 1155 | `	if( pFunc == 0 && pHost == 0 ){` |
|     6 | 1156 | `		ph7_result_null(pCtx);` |
|     6 | 1157 | `		return PH7_OK;` |
|     - | 1158 | `	}` |
|   718 | 1159 | `	pInfo = ph7_context_new_array(pCtx);` |
|   718 | 1160 | `	if( pInfo == 0 ){` |
|   ! 0 | 1161 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1162 | `		return PH7_OK;` |
|     - | 1163 | `	}` |
|   718 | 1164 | `	if( pFunc == 0 ){` |
|     - | 1165 | `		/* Host (C builtin) function: no parameter metadata beyond arity */` |
|   130 | 1166 | `		ph7_value *pParams = ph7_context_new_array(pCtx);` |
|   130 | 1167 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pHost->sName), (int)SyStringLength(&pHost->sName));` |
|   130 | 1168 | `		ReflectMapAddBool(pCtx, pInfo, "internal", 1);` |
|   130 | 1169 | `		ReflectMapAddBool(pCtx, pInfo, "closure", 0);` |
|   130 | 1170 | `		ReflectMapAddBool(pCtx, pInfo, "fstatic", 0);` |
|   130 | 1171 | `		ReflectMapAddBool(pCtx, pInfo, "byref", 0);` |
|   130 | 1172 | `		ReflectMapAddBool(pCtx, pInfo, "generator", 0);` |
|   130 | 1173 | `		ReflectMapAddBool(pCtx, pInfo, "strict", 0);` |
|   130 | 1174 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|   130 | 1175 | `		ReflectMapAddInt(pCtx, pInfo, "line", 0);` |
|   130 | 1176 | `		ReflectMapAddInt(pCtx, pInfo, "endline", 0);` |
|   130 | 1177 | `		ReflectMapAddBool(pCtx, pInfo, "doc", 0);` |
|     - | 1178 | `		{` |
|   130 | 1179 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|   130 | 1180 | `			if( pEmpty ){` |
|   130 | 1181 | `				ph7_array_add_strkey_elem(pInfo, "attrs", pEmpty);` |
|    64 | 1182 | `			}` |
|     - | 1183 | `		}` |
|   130 | 1184 | `		if( pHost->zRet ){` |
|   130 | 1185 | `			ReflectMapAddStr(pCtx, pInfo, "rettext", pHost->zRet, (int)SyStrlen(pHost->zRet));` |
|    66 | 1186 | `		}else{` |
|   ! 0 | 1187 | `			ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1188 | `		}` |
|   130 | 1189 | `		ReflectMapAddBool(pCtx, pInfo, "retnullable", 0);` |
|   130 | 1190 | `		if( pParams ){` |
|   130 | 1191 | `			ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|    64 | 1192 | `		}` |
|   130 | 1193 | `		ReflectMapAddBool(pCtx, pInfo, "variadic", 0);` |
|   130 | 1194 | `		ReflectMapAddInt(pCtx, pInfo, "minarg", (sxi64)pHost->nMinArg);` |
|   130 | 1195 | `		if( pHost->zSig ){` |
|   130 | 1196 | `			ReflectMapAddStr(pCtx, pInfo, "sig", pHost->zSig, (int)SyStrlen(pHost->zSig));` |
|    66 | 1197 | `		}else{` |
|   ! 0 | 1198 | `			ReflectMapAddStr(pCtx, pInfo, "sig", "", 0);` |
|     - | 1199 | `		}` |
|   130 | 1200 | `		ph7_result_value(pCtx, pInfo);` |
|   130 | 1201 | `		return PH7_OK;` |
|     - | 1202 | `	}` |
|   589 | 1203 | `	ReflectFillFuncCommon(pCtx, pInfo, pFunc);` |
|   589 | 1204 | `	ReflectMapAddInt(pCtx, pInfo, "minarg", -1);` |
|   589 | 1205 | `	if( (pFunc->iFlags & VM_FUNC_INTERNAL) && SySetUsed(&pFunc->aArgs) == 0 && pMeth == 0 ){` |
|     - | 1206 | `		/* Embedded-PHP builtin (max/min...): declared argless, actual` |
|     - | 1207 | `		 * signature comes from the static table */` |
|     5 | 1208 | `		const char *zRet = 0;` |
|     5 | 1209 | `		const char *zSig = PH7_VmBuiltinSigLookup(SyStringData(&pFunc->sName), SyStringLength(&pFunc->sName), &zRet);` |
|     5 | 1210 | `		if( zSig ){` |
|     5 | 1211 | `			ReflectMapAddStr(pCtx, pInfo, "sig", zSig, (int)SyStrlen(zSig));` |
|     2 | 1212 | `		}` |
|     5 | 1213 | `		if( zRet && SyStringLength(&pFunc->sReturnTypeName) == 0 ){` |
|     5 | 1214 | `			ReflectMapAddStr(pCtx, pInfo, "ret2", zRet, (int)SyStrlen(zRet));` |
|     2 | 1215 | `		}` |
|     2 | 1216 | `	}` |
|   589 | 1217 | `	if( pMeth && pClass ){` |
|   277 | 1218 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|   277 | 1219 | `		ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   277 | 1220 | `		ReflectMapAddStr(pCtx, pInfo, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   277 | 1221 | `		ReflectMapAddInt(pCtx, pInfo, "vis", (sxi64)pMeth->iProtection);` |
|   277 | 1222 | `		ReflectMapAddBool(pCtx, pInfo, "mstatic", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   277 | 1223 | `		ReflectMapAddBool(pCtx, pInfo, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|   277 | 1224 | `		ReflectMapAddBool(pCtx, pInfo, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   138 | 1225 | `	}` |
|   589 | 1226 | `	if( pClosure ){` |
|     - | 1227 | `		SyString sAttr;` |
|     - | 1228 | `		ph7_value *pAttr;` |
|     - | 1229 | `		ph7_value *pUsed;` |
|    49 | 1230 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|    49 | 1231 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    49 | 1232 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|   ! 0 | 1233 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 1234 | `			if( pKey ){` |
|   ! 0 | 1235 | `				ph7_value_string(pKey, "this", 4);` |
|   ! 0 | 1236 | `				ph7_array_add_elem(pInfo, pKey, pAttr);` |
|   ! 0 | 1237 | `			}` |
|   ! 0 | 1238 | `		}else{` |
|    49 | 1239 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|     - | 1240 | `		}` |
|    49 | 1241 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|    49 | 1242 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    49 | 1243 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|   ! 0 | 1244 | `			ReflectMapAddStr(pCtx, pInfo, "scope", (const char *)SyBlobData(&pAttr->sBlob),` |
|   ! 0 | 1245 | `				(int)SyBlobLength(&pAttr->sBlob));` |
|   ! 0 | 1246 | `		}else{` |
|    49 | 1247 | `			ReflectMapAddNull(pCtx, pInfo, "scope");` |
|     - | 1248 | `		}` |
|     - | 1249 | `		/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */` |
|    49 | 1250 | `		pUsed = ph7_context_new_array(pCtx);` |
|    49 | 1251 | `		if( pUsed ){` |
|    49 | 1252 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     - | 1253 | `			sxu32 n;` |
|   107 | 1254 | `			for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; n++ ){` |
|    59 | 1255 | `				if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){` |
|    41 | 1256 | `					continue;` |
|     - | 1257 | `				}` |
|    18 | 1258 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|    10 | 1259 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|   ! 0 | 1260 | `					continue;` |
|     - | 1261 | `				}` |
|    19 | 1262 | `				if( (aEnv[n].iFlags & VM_FUNC_ARG_BY_REF) && aEnv[n].nIdx != SXU32_HIGH ){` |
|     - | 1263 | `					/* Captured by reference: report the slot's live value */` |
|     5 | 1264 | `					ph7_value *pLive = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aEnv[n].nIdx);` |
|     5 | 1265 | `					ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, pLive ? pLive : &aEnv[n].sValue);` |
|     5 | 1266 | `					continue;` |
|     - | 1267 | `				}` |
|    15 | 1268 | `				ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, &aEnv[n].sValue);` |
|     8 | 1269 | `			}` |
|    49 | 1270 | `			ph7_array_add_strkey_elem(pInfo, "used", pUsed);` |
|    24 | 1271 | `		}` |
|    24 | 1272 | `	}` |
|   589 | 1273 | `	ph7_result_value(pCtx, pInfo);` |
|   589 | 1274 | `	return PH7_OK;` |
|   362 | 1275 | `}` |
|     - | 1276 | `/*` |
|     - | 1277 | ` * mixed __reflect_param_default(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1278 | ` * Evaluate a parameter's compiled default expression.` |
|     - | 1279 | ` */` |
|    12 | 1280 | `static int vm_builtin_reflect_param_default(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1281 | `{` |
|     - | 1282 | `	ph7_vm_func *pFunc;` |
|     - | 1283 | `	ph7_vm_func_arg *pArg;` |
|     - | 1284 | `	ph7_value sValue;` |
|     - | 1285 | `	sxu32 nIdx;` |
|    13 | 1286 | `	if( nArg < 3 ){` |
|   ! 0 | 1287 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1288 | `		return PH7_OK;` |
|     - | 1289 | `	}` |
|    13 | 1290 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|    13 | 1291 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|    12 | 1292 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|    13 | 1293 | `	 \|\| SySetUsed(&pArg->aByteCode) < 1 ){` |
|   ! 0 | 1294 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1295 | `		return PH7_OK;` |
|     - | 1296 | `	}` |
|    13 | 1297 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|    13 | 1298 | `	VmLocalExec(pCtx->pVm, &pArg->aByteCode, &sValue, FALSE);` |
|    13 | 1299 | `	ph7_result_value(pCtx, &sValue);` |
|    13 | 1300 | `	PH7_MemObjRelease(&sValue);` |
|    13 | 1301 | `	return PH7_OK;` |
|     7 | 1302 | `}` |
|     - | 1303 | `/*` |
|     - | 1304 | ` * string\|null __reflect_param_defconst(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1305 | ` * When a parameter's default is a plain global-constant reference, its` |
|     - | 1306 | ` * source name; null otherwise. A constant default compiles to exactly` |
|     - | 1307 | ` * [ OP_LOADC (EXPAND) , OP_DONE ] with the name in the literal table.` |
|     - | 1308 | ` */` |
|     6 | 1309 | `static int vm_builtin_reflect_param_defconst(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1310 | `{` |
|     - | 1311 | `	ph7_vm_func *pFunc;` |
|     - | 1312 | `	ph7_vm_func_arg *pArg;` |
|     - | 1313 | `	VmInstr *aInstr;` |
|     - | 1314 | `	ph7_value *pLit;` |
|     - | 1315 | `	sxu32 nIdx;` |
|     7 | 1316 | `	if( nArg < 3 ){` |
|   ! 0 | 1317 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1318 | `		return PH7_OK;` |
|     - | 1319 | `	}` |
|     7 | 1320 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|     7 | 1321 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|     6 | 1322 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|     7 | 1323 | `	 \|\| SySetUsed(&pArg->aByteCode) != 2 ){` |
|     3 | 1324 | `		ph7_result_null(pCtx);` |
|     3 | 1325 | `		return PH7_OK;` |
|     - | 1326 | `	}` |
|     5 | 1327 | `	aInstr = (VmInstr *)SySetBasePtr(&pArg->aByteCode);` |
|     4 | 1328 | `	if( aInstr[0].iOp != PH7_OP_LOADC \|\| (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0` |
|     5 | 1329 | `	 \|\| aInstr[1].iOp != PH7_OP_DONE ){` |
|   ! 0 | 1330 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1331 | `		return PH7_OK;` |
|     - | 1332 | `	}` |
|     5 | 1333 | `	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);` |
|     5 | 1334 | `	if( pLit == 0 \|\| SyBlobLength(&pLit->sBlob) < 1 ){` |
|   ! 0 | 1335 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1336 | `		return PH7_OK;` |
|     - | 1337 | `	}` |
|     5 | 1338 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&pLit->sBlob), (int)SyBlobLength(&pLit->sBlob));` |
|     5 | 1339 | `	return PH7_OK;` |
|     4 | 1340 | `}` |
|     - | 1341 | `/*` |
|     - | 1342 | ` * mixed __reflect_invoke(mixed $target, ?string $method, ?object $this, array $args)` |
|     - | 1343 | ` * Visibility-bypassing invocation (methods dispatch by VM name; functions` |
|     - | 1344 | ` * and closures ride PH7_VmCallUserFunction like call_user_func_array).` |
|     - | 1345 | ` */` |
|    20 | 1346 | `static int vm_builtin_reflect_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1347 | `{` |
|    21 | 1348 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1349 | `	ph7_value sResult;` |
|     - | 1350 | `	SySet aCallArg;` |
|     - | 1351 | `	sxi32 rc;` |
|    21 | 1352 | `	if( nArg < 4 ){` |
|   ! 0 | 1353 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1354 | `		return PH7_OK;` |
|     - | 1355 | `	}` |
|    21 | 1356 | `	PH7_MemObjInit(pVm, &sResult);` |
|    21 | 1357 | `	sResult.nIdx = SXU32_HIGH;` |
|    21 | 1358 | `	SySetInit(&aCallArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    21 | 1359 | `	ReflectCollectArgs(pCtx, apArg[3], &aCallArg, 0);` |
|    21 | 1360 | `	if( (apArg[1]->iFlags & MEMOBJ_STRING) && SyBlobLength(&apArg[1]->sBlob) > 0 ){` |
|    11 | 1361 | `		ph7_class *pClass = 0;` |
|    11 | 1362 | `		ph7_class_method *pMeth = 0;` |
|    11 | 1363 | `		ph7_class_instance *pThis = 0;` |
|    11 | 1364 | `		ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, 0);` |
|    11 | 1365 | `		if( pMeth == 0 ){` |
|   ! 0 | 1366 | `			SySetRelease(&aCallArg);` |
|   ! 0 | 1367 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1368 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1369 | `			return PH7_OK;` |
|     - | 1370 | `		}` |
|    11 | 1371 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     7 | 1372 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     3 | 1373 | `		}` |
|     - | 1374 | `		/* Reflection ignores method visibility (PHP 8.1+); the flag is` |
|     - | 1375 | `		 * consumed by the first OP_CALL, i.e. this synthetic one. */` |
|    11 | 1376 | `		pVm->bReflectBypass = 1;` |
|    16 | 1377 | `		rc = PH7_VmCallClassMethod(pVm, pThis, pMeth, &sResult,` |
|    10 | 1378 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg));` |
|    11 | 1379 | `		pVm->bReflectBypass = 0;` |
|     6 | 1380 | `	}else{` |
|    16 | 1381 | `		rc = PH7_VmCallUserFunction(pVm, apArg[0],` |
|    10 | 1382 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg), &sResult);` |
|     - | 1383 | `	}` |
|    21 | 1384 | `	SySetRelease(&aCallArg);` |
|    21 | 1385 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 | 1386 | `		PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1387 | `		return rc;` |
|     - | 1388 | `	}` |
|    21 | 1389 | `	ph7_result_value(pCtx, &sResult);` |
|    21 | 1390 | `	PH7_MemObjRelease(&sResult);` |
|    21 | 1391 | `	return PH7_OK;` |
|    11 | 1392 | `}` |
|     - | 1393 | `/*` |
|     - | 1394 | ` * Closure __reflect_closure(mixed $target, ?string $method, ?object $this)` |
|     - | 1395 | ` * Mint a Closure for a function or method, bound and scoped like the` |
|     - | 1396 | ` * first-class-callable path.` |
|     - | 1397 | ` */` |
|     6 | 1398 | `static int vm_builtin_reflect_closure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1399 | `{` |
|     7 | 1400 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 1401 | `	ph7_class *pClass = 0;` |
|     7 | 1402 | `	ph7_class_method *pMeth = 0;` |
|     7 | 1403 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1404 | `	ph7_vm_func *pFunc;` |
|     7 | 1405 | `	if( nArg < 3 ){` |
|   ! 0 | 1406 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1407 | `		return PH7_OK;` |
|     - | 1408 | `	}` |
|     7 | 1409 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, &pClosure);` |
|     7 | 1410 | `	if( pClosure ){` |
|     - | 1411 | `		/* Already a Closure: hand the same instance back */` |
|   ! 0 | 1412 | `		return ReflectResultExistingObject(pCtx, pClosure);` |
|     - | 1413 | `	}` |
|     7 | 1414 | `	if( pMeth && pClass ){` |
|     5 | 1415 | `		ph7_class_instance *pThis = 0;` |
|     5 | 1416 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     3 | 1417 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     1 | 1418 | `		}` |
|     7 | 1419 | `		return ReflectResultObject(pCtx,` |
|     4 | 1420 | `			PH7_VmNewClosure(pVm, &pMeth->sFunc.sName, pThis, &pClass->sName));` |
|     - | 1421 | `	}` |
|     3 | 1422 | `	if( pFunc ){` |
|     3 | 1423 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &pFunc->sName, 0, 0));` |
|     - | 1424 | `	}` |
|     - | 1425 | `	/* Host function by name */` |
|   ! 0 | 1426 | `	if( apArg[0]->iFlags & MEMOBJ_STRING ){` |
|     - | 1427 | `		SyString sName;` |
|   ! 0 | 1428 | `		SyStringInitFromBuf(&sName, (const char *)SyBlobData(&apArg[0]->sBlob), SyBlobLength(&apArg[0]->sBlob));` |
|   ! 0 | 1429 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &sName, 0, 0));` |
|     - | 1430 | `	}` |
|   ! 0 | 1431 | `	ph7_result_null(pCtx);` |
|   ! 0 | 1432 | `	return PH7_OK;` |
|     4 | 1433 | `}` |
|     - | 1434 | `/*` |
|     - | 1435 | ` * Resolve a Generator object into its wrapper. Mirrors the static` |
|     - | 1436 | ` * VmGeneratorExtractCtx in vm.c: the $__ctx attribute carries the` |
|     - | 1437 | ` * ph7_generator pointer as a resource value.` |
|     - | 1438 | ` */` |
|    22 | 1439 | `static ph7_generator * ReflectGeneratorCtx(ph7_vm *pVm, ph7_value *pVal)` |
|     1 | 1440 | `{` |
|     - | 1441 | `	ph7_class_instance *pThis;` |
|     - | 1442 | `	ph7_value *pAttr;` |
|     - | 1443 | `	SyString sAttr;` |
|    23 | 1444 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVm->pGeneratorClass == 0 ){` |
|   ! 0 | 1445 | `		return 0;` |
|     - | 1446 | `	}` |
|    23 | 1447 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    23 | 1448 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|   ! 0 | 1449 | `		return 0;` |
|     - | 1450 | `	}` |
|    23 | 1451 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    23 | 1452 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    23 | 1453 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|   ! 0 | 1454 | `		return 0;` |
|     - | 1455 | `	}` |
|    23 | 1456 | `	return (ph7_generator *)pAttr->x.pOther;` |
|    12 | 1457 | `}` |
|     - | 1458 | `/*` |
|     - | 1459 | ` * array\|null __reflect_gen_info(Generator $g)` |
|     - | 1460 | ` * {state, closed, executing, kind ('fn'\|'method'), name, class?, this}` |
|     - | 1461 | ` */` |
|    16 | 1462 | `static int vm_builtin_reflect_gen_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1463 | `{` |
|    17 | 1464 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1465 | `	ph7_generator *pGen;` |
|     - | 1466 | `	ph7_exec_ctx *pExec;` |
|     - | 1467 | `	ph7_value *pInfo;` |
|    17 | 1468 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 \|\| pGen->pCtx == 0 ){` |
|   ! 0 | 1469 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1470 | `		return PH7_OK;` |
|     - | 1471 | `	}` |
|    17 | 1472 | `	pExec = pGen->pCtx;` |
|    17 | 1473 | `	pInfo = ph7_context_new_array(pCtx);` |
|    17 | 1474 | `	if( pInfo == 0 ){` |
|   ! 0 | 1475 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1476 | `		return PH7_OK;` |
|     - | 1477 | `	}` |
|    17 | 1478 | `	ReflectMapAddInt(pCtx, pInfo, "state", (sxi64)pExec->iState);` |
|    24 | 1479 | `	ReflectMapAddBool(pCtx, pInfo, "closed",` |
|    16 | 1480 | `		pExec->iState == PH7_CTX_STATE_COMPLETED \|\| pExec->iState == PH7_CTX_STATE_CLOSED);` |
|    17 | 1481 | `	ReflectMapAddBool(pCtx, pInfo, "executing", pVm->pActiveCtx == pExec);` |
|    17 | 1482 | `	if( pExec->pFunc ){` |
|    17 | 1483 | `		ph7_vm_func *pFunc = pExec->pFunc;` |
|    19 | 1484 | `		if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     5 | 1485 | `			ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|     5 | 1486 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "method", sizeof("method")-1);` |
|     5 | 1487 | `			ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|     3 | 1488 | `		}else{` |
|    13 | 1489 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "fn", sizeof("fn")-1);` |
|     - | 1490 | `		}` |
|    17 | 1491 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|     8 | 1492 | `	}` |
|     - | 1493 | `	{` |
|     - | 1494 | `		/* The coroutine frame installs $this as a frame VARIABLE (see` |
|     - | 1495 | `		 * VmFiberSetupFrame), not as pFrame->pThis — check both. */` |
|    17 | 1496 | `		ph7_value *pThisVal = 0;` |
|    17 | 1497 | `		if( pExec->pFrame ){` |
|    17 | 1498 | `			SyHashEntry *pVar = SyHashGet(&pExec->pFrame->hVar, "this", sizeof("this")-1);` |
|    17 | 1499 | `			if( pVar ){` |
|     5 | 1500 | `				ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj, (sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|     5 | 1501 | `				if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|     5 | 1502 | `					pThisVal = pSlot;` |
|     2 | 1503 | `				}` |
|     2 | 1504 | `			}` |
|    17 | 1505 | `			if( pThisVal == 0 && pExec->pFrame->pThis ){` |
|     - | 1506 | `				ph7_value sThis;` |
|   ! 0 | 1507 | `				ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 1508 | `				PH7_MemObjInit(pVm, &sThis);` |
|   ! 0 | 1509 | `				pExec->pFrame->pThis->iRef++;` |
|   ! 0 | 1510 | `				sThis.x.pOther = pExec->pFrame->pThis;` |
|   ! 0 | 1511 | `				MemObjSetType(&sThis, MEMOBJ_OBJ);` |
|   ! 0 | 1512 | `				if( pKey ){` |
|   ! 0 | 1513 | `					ph7_value_string(pKey, "this", 4);` |
|   ! 0 | 1514 | `					ph7_array_add_elem(pInfo, pKey, &sThis); /* copies (takes its own ref) */` |
|   ! 0 | 1515 | `				}` |
|   ! 0 | 1516 | `				PH7_MemObjRelease(&sThis);` |
|   ! 0 | 1517 | `				pThisVal = (ph7_value *)1; /* handled */` |
|   ! 0 | 1518 | `			}` |
|     8 | 1519 | `		}` |
|    17 | 1520 | `		if( pThisVal == 0 ){` |
|    13 | 1521 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|    11 | 1522 | `		}else if( pThisVal != (ph7_value *)1 ){` |
|     5 | 1523 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|     5 | 1524 | `			if( pKey ){` |
|     5 | 1525 | `				ph7_value_string(pKey, "this", 4);` |
|     5 | 1526 | `				ph7_array_add_elem(pInfo, pKey, pThisVal);` |
|     2 | 1527 | `			}` |
|     2 | 1528 | `		}` |
|     - | 1529 | `	}` |
|    17 | 1530 | `	ph7_result_value(pCtx, pInfo);` |
|    17 | 1531 | `	return PH7_OK;` |
|     9 | 1532 | `}` |
|     - | 1533 | `/*` |
|     - | 1534 | ` * Generator __reflect_gen_exec(Generator $g)` |
|     - | 1535 | `` * Follow `yield from` delegation to the innermost executing generator`` |
|     - | 1536 | ` * (PHP's ReflectionGenerator::getExecutingGenerator).` |
|     - | 1537 | ` */` |
|     4 | 1538 | `static int vm_builtin_reflect_gen_exec(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1539 | `{` |
|     5 | 1540 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1541 | `	ph7_generator *pGen;` |
|     - | 1542 | `	ph7_value *pCur;` |
|     5 | 1543 | `	int iDepth = 0;` |
|     5 | 1544 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 ){` |
|   ! 0 | 1545 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1546 | `		return PH7_OK;` |
|     - | 1547 | `	}` |
|     5 | 1548 | `	pCur = apArg[0];` |
|     9 | 1549 | `	while( pGen && pGen->pCtx && pGen->pCtx->iDelegateState == 3` |
|     8 | 1550 | `	 && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|     3 | 1551 | `		ph7_generator *pInner = ReflectGeneratorCtx(pVm, &pGen->pCtx->sDelegate);` |
|     3 | 1552 | `		if( pInner == 0 ){` |
|   ! 0 | 1553 | `			break;` |
|     - | 1554 | `		}` |
|     3 | 1555 | `		pCur = &pGen->pCtx->sDelegate;` |
|     3 | 1556 | `		pGen = pInner;` |
|     3 | 1557 | `		iDepth++;` |
|     1 | 1558 | `	}` |
|     5 | 1559 | `	return ReflectResultExistingObject(pCtx, (ph7_class_instance *)pCur->x.pOther);` |
|     3 | 1560 | `}` |
|     - | 1561 | `/*` |
|     - | 1562 | ` * array\|null __reflect_const_info(string $name)` |
|     - | 1563 | ` * Global-constant descriptor: {value}. Null when undefined. File/origin` |
|     - | 1564 | ` * metadata arrives with the C5 constant-metadata work.` |
|     - | 1565 | ` */` |
|    40 | 1566 | `static int vm_builtin_reflect_const_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1567 | `{` |
|    41 | 1568 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1569 | `	SyHashEntry *pEntry;` |
|     - | 1570 | `	ph7_constant *pCons;` |
|     - | 1571 | `	ph7_value *pInfo;` |
|     - | 1572 | `	ph7_value sValue;` |
|     - | 1573 | `	const char *zName;` |
|     - | 1574 | `	int nLen;` |
|    41 | 1575 | `	if( nArg < 1 ){` |
|   ! 0 | 1576 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1577 | `		return PH7_OK;` |
|     - | 1578 | `	}` |
|    41 | 1579 | `	zName = ph7_value_to_string(apArg[0], &nLen);` |
|    41 | 1580 | `	pEntry = nLen > 0 ? SyHashGet(&pVm->hConstant, (const void *)zName, (sxu32)nLen) : 0;` |
|    41 | 1581 | `	if( pEntry == 0 ){` |
|     3 | 1582 | `		ph7_result_null(pCtx);` |
|     3 | 1583 | `		return PH7_OK;` |
|     - | 1584 | `	}` |
|    39 | 1585 | `	pCons = (ph7_constant *)pEntry->pUserData;` |
|    39 | 1586 | `	pInfo = ph7_context_new_array(pCtx);` |
|    39 | 1587 | `	if( pInfo == 0 ){` |
|   ! 0 | 1588 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1589 | `		return PH7_OK;` |
|     - | 1590 | `	}` |
|    39 | 1591 | `	PH7_MemObjInit(pVm, &sValue);` |
|    39 | 1592 | `	if( pCons->xExpand ){` |
|    39 | 1593 | `		pCons->xExpand(&sValue, pCons->pUserData);` |
|    19 | 1594 | `	}` |
|     - | 1595 | `	{` |
|    39 | 1596 | `		ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|    39 | 1597 | `		if( pKey ){` |
|    39 | 1598 | `			ph7_value_string(pKey, "value", 5);` |
|    39 | 1599 | `			ph7_array_add_elem(pInfo, pKey, &sValue);` |
|    19 | 1600 | `		}` |
|     - | 1601 | `	}` |
|    39 | 1602 | `	PH7_MemObjRelease(&sValue);` |
|    39 | 1603 | `	ReflectMapAddBool(pCtx, pInfo, "internal", pCons->bUserDefined == 0);` |
|    39 | 1604 | `	if( SyStringLength(&pCons->sFile) > 0 ){` |
|    25 | 1605 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));` |
|    13 | 1606 | `	}else{` |
|    15 | 1607 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1608 | `	}` |
|    39 | 1609 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pCons->nLine);` |
|    39 | 1610 | `	ReflectMapAddAttrs(pCtx, pInfo, &pCons->aAttrs);` |
|    39 | 1611 | `	ph7_result_value(pCtx, pInfo);` |
|    39 | 1612 | `	return PH7_OK;` |
|    21 | 1613 | `}` |
|     - | 1614 | `/*` |
|     - | 1615 | ` * int\|null __reflect_ref_id(array $arr, int\|string $key)` |
|     - | 1616 | ` * The element's slot index when the element is a reference (its slot has` |
|     - | 1617 | ` * a reference-table record with at least two links), null otherwise.` |
|     - | 1618 | ` */` |
|     6 | 1619 | `static int vm_builtin_reflect_ref_id(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1620 | `{` |
|     - | 1621 | `	ph7_hashmap *pMap;` |
|     7 | 1622 | `	ph7_hashmap_node *pNode = 0;` |
|     7 | 1623 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|   ! 0 | 1624 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1625 | `		return PH7_OK;` |
|     - | 1626 | `	}` |
|     7 | 1627 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 | 1628 | `	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK \|\| pNode == 0 ){` |
|   ! 0 | 1629 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1630 | `		return PH7_OK;` |
|     - | 1631 | `	}` |
|     7 | 1632 | `	if( PH7_VmSlotRefCount(pCtx->pVm, pNode->nValIdx) < 2 ){` |
|     3 | 1633 | `		ph7_result_null(pCtx);` |
|     3 | 1634 | `		return PH7_OK;` |
|     - | 1635 | `	}` |
|     5 | 1636 | `	ph7_result_int64(pCtx, (sxi64)pNode->nValIdx);` |
|     5 | 1637 | `	return PH7_OK;` |
|     4 | 1638 | `}` |
|     - | 1639 | `/*` |
|     - | 1640 | ` * array\|null __reflect_attr_args(string $kind, mixed $target, ?string $member,` |
|     - | 1641 | ` *                                int $paramIdx, int $attrIdx)` |
|     - | 1642 | ` * Evaluate the recorded argument expressions of one declared attribute:` |
|     - | 1643 | ` * kind 'class' (target = class), 'attr' (class + property/constant name),` |
|     - | 1644 | ` * 'method' (class + method), 'fn' (function name or Closure), 'param'` |
|     - | 1645 | ` * (function spec + parameter index). Named arguments become string keys.` |
|     - | 1646 | ` */` |
|    68 | 1647 | `static int vm_builtin_reflect_attr_args(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1648 | `{` |
|    69 | 1649 | `	ph7_vm *pVm = pCtx->pVm;` |
|    69 | 1650 | `	SySet *pAttrs = 0;` |
|    69 | 1651 | `	ph7_class *pDeclCls = 0; /* class an attribute is declared on: scope for self:: in its args */` |
|     - | 1652 | `	ph7_attribute *pAttrRec;` |
|     - | 1653 | `	ph7_value *pOut;` |
|     - | 1654 | `	const char *zKind;` |
|     - | 1655 | `	int nKind;` |
|     - | 1656 | `	sxu32 nAttrIdx, n;` |
|    69 | 1657 | `	if( nArg < 5 ){` |
|   ! 0 | 1658 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1659 | `		return PH7_OK;` |
|     - | 1660 | `	}` |
|    69 | 1661 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|    69 | 1662 | `	nAttrIdx = (sxu32)ph7_value_to_int(apArg[4]);` |
|    91 | 1663 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|    45 | 1664 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|    45 | 1665 | `		if( pClass ){ pAttrs = &pClass->aAttrs; pDeclCls = pClass; }` |
|    49 | 1666 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|     5 | 1667 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|     5 | 1668 | `		ph7_class_attr *pMember = pClass ? ReflectFetchAttr(pClass, apArg[2]) : 0;` |
|     5 | 1669 | `		if( pMember ){ pAttrs = &pMember->aAttrs; pDeclCls = pClass; }` |
|    27 | 1670 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|     9 | 1671 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     9 | 1672 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|    21 | 1673 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|     9 | 1674 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], 0, 0, 0, 0, 0);` |
|     9 | 1675 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|    10 | 1676 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|     3 | 1677 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1678 | `		ph7_vm_func_arg *pParam = pFunc` |
|     2 | 1679 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|     3 | 1680 | `		if( pParam ){ pAttrs = &pParam->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|     4 | 1681 | `	}else if( nKind == 5 && SyMemcmp(zKind, "const", 5) == 0 ){` |
|     - | 1682 | ``		/* Global constant (php 8.5 attributes on `const` statements) */`` |
|     - | 1683 | `		const char *zCName;` |
|     - | 1684 | `		int nCName;` |
|     - | 1685 | `		SyHashEntry *pCEntry;` |
|     3 | 1686 | `		zCName = ph7_value_to_string(apArg[1], &nCName);` |
|     3 | 1687 | `		pCEntry = nCName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zCName, (sxu32)nCName) : 0;` |
|     3 | 1688 | `		if( pCEntry ){ pAttrs = &((ph7_constant *)pCEntry->pUserData)->aAttrs; }` |
|     1 | 1689 | `	}` |
|    68 | 1690 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|    69 | 1691 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 | 1692 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1693 | `		return PH7_OK;` |
|     - | 1694 | `	}` |
|   123 | 1695 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|    55 | 1696 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|     - | 1697 | `		ph7_value sValue;` |
|    55 | 1698 | `		PH7_MemObjInit(pVm, &sValue);` |
|    55 | 1699 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|     - | 1700 | `` 			/* Evaluate under the attribute's declaring-class scope so `self::class` `` |
|     - | 1701 | ``			 * / `self::CONST` / `parent::` resolve like php (else self:: would bind`` |
|     - | 1702 | `			 * to the reflection machinery's own class). */` |
|    55 | 1703 | `			PH7_VmExecAttrArg(pVm, &pArgRec->aByteCode, pDeclCls, &sValue);` |
|    27 | 1704 | `		}` |
|    55 | 1705 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|     5 | 1706 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|     3 | 1707 | `		}else{` |
|    51 | 1708 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|     - | 1709 | `		}` |
|    55 | 1710 | `		PH7_MemObjRelease(&sValue);` |
|    28 | 1711 | `	}` |
|    69 | 1712 | `	ph7_result_value(pCtx, pOut);` |
|    69 | 1713 | `	return PH7_OK;` |
|    35 | 1714 | `}` |
|     - | 1715 | `/*` |
|     - | 1716 | ` * The Reflection classes, in PHP. Chunk 1: exceptions, Reflector,` |
|     - | 1717 | ` * Reflection, ReflectionClass, ReflectionObject (plus get_debug_type,` |
|     - | 1718 | ` * which the TypeError messages need and PHP 8.0 ships natively).` |
|     - | 1719 | ` */` |
|     - | 1720 | `static const char zReflectLib1[] =` |
|     - | 1721 | `/* Per-class memoization of the (expensive) C class descriptor. Every Reflection*` |
|     - | 1722 | ` * accessor funnels through __rinfo()/the constructors, so without this a single` |
|     - | 1723 | ` * test run rebuilds the full descriptor of the same class hundreds of times` |
|     - | 1724 | ` * (real php caches the reflected class). Keyed by resolved class name; only` |
|     - | 1725 | ` * successful lookups are cached, so a not-yet-autoloaded class is re-queried.` |
|     - | 1726 | ` * The C builtin __reflect_class_info stays the real worker. */` |
|     - | 1727 | `"function __phl_rcinfo($oc){"` |
|     - | 1728 | `" static $c = array();"` |
|     - | 1729 | `" $k = is_object($oc) ? get_class($oc) : (string)$oc;"` |
|     - | 1730 | `" if( isset($c[$k]) ){ return $c[$k]; }"` |
|     - | 1731 | `" $info = __reflect_class_info($oc);"` |
|     - | 1732 | `" if( $info !== null ){ $c[$k] = $info; }"` |
|     - | 1733 | `" return $info;"` |
|     - | 1734 | `"}"` |
|     - | 1735 | `"function get_debug_type($value){"` |
|     - | 1736 | `" if(is_object($value)){ return get_class($value); }"` |
|     - | 1737 | `" if(is_bool($value)){ return 'bool'; }"` |
|     - | 1738 | `" if(is_int($value)){ return 'int'; }"` |
|     - | 1739 | `" if(is_float($value)){ return 'float'; }"` |
|     - | 1740 | `" if(is_string($value)){ return 'string'; }"` |
|     - | 1741 | `" if(is_array($value)){ return 'array'; }"` |
|     - | 1742 | `" if($value === null){ return 'null'; }"` |
|     - | 1743 | `" return gettype($value);"` |
|     - | 1744 | `"}"` |
|     - | 1745 | `"interface Reflector extends Stringable {}"` |
|     - | 1746 | `"class ReflectionException extends Exception {}"` |
|     - | 1747 | `"class Reflection {"` |
|     - | 1748 | `" public static function getModifierNames($modifiers){"` |
|     - | 1749 | `"  $names = array();"` |
|     - | 1750 | `"  if($modifiers & 64){ $names[] = 'abstract'; }"` |
|     - | 1751 | `"  if($modifiers & 32){ $names[] = 'final'; }"` |
|     - | 1752 | `"  if($modifiers & 1){ $names[] = 'public'; }"` |
|     - | 1753 | `"  if($modifiers & 2){ $names[] = 'protected'; }"` |
|     - | 1754 | `"  if($modifiers & 4){ $names[] = 'private'; }"` |
|     - | 1755 | `"  if($modifiers & 16){ $names[] = 'static'; }"` |
|     - | 1756 | `"  if($modifiers & 128){ $names[] = 'readonly'; }"` |
|     - | 1757 | `"  return $names;"` |
|     - | 1758 | `" }"` |
|     - | 1759 | `"}"` |
|     - | 1760 | `"class ReflectionClass implements Reflector {"` |
|     - | 1761 | `" const IS_IMPLICIT_ABSTRACT = 16;"` |
|     - | 1762 | `" const IS_EXPLICIT_ABSTRACT = 64;"` |
|     - | 1763 | `" const IS_FINAL = 32;"` |
|     - | 1764 | `" const IS_READONLY = 65536;"` |
|     - | 1765 | `" const SKIP_INITIALIZATION_ON_SERIALIZE = 8;"` |
|     - | 1766 | `" const SKIP_DESTRUCTOR = 16;"` |
|     - | 1767 | `" public $name;"` |
|     - | 1768 | `" protected $__obj = null;"` |
|     - | 1769 | `" public function __construct($objectOrClass){"` |
|     - | 1770 | `"  if(!is_object($objectOrClass) && !is_string($objectOrClass)){"` |
|     - | 1771 | `"   if(is_int($objectOrClass) \|\| is_float($objectOrClass) \|\| is_bool($objectOrClass)){"` |
|     - | 1772 | `"    $objectOrClass = (string)$objectOrClass;"` |
|     - | 1773 | `"   }else{"` |
|     - | 1774 | `"    throw new TypeError('ReflectionClass::__construct(): Argument #1 ($objectOrClass) must be of type object\|string, '.get_debug_type($objectOrClass).' given');"` |
|     - | 1775 | `"   }"` |
|     - | 1776 | `"  }"` |
|     - | 1777 | `"  $info = __phl_rcinfo($objectOrClass);"` |
|     - | 1778 | `"  if($info === null){"` |
|     - | 1779 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 1780 | `"  }"` |
|     - | 1781 | `"  $this->name = $info['name'];"` |
|     - | 1782 | `" }"` |
|     - | 1783 | `" protected function __rinfo(){ return __phl_rcinfo($this->name); }"` |
|     - | 1784 | `" public function getName(){ return $this->name; }"` |
|     - | 1785 | `" public function getShortName(){"` |
|     - | 1786 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1787 | `"  if($p === false){ return $this->name; }"` |
|     - | 1788 | `"  return substr($this->name,$p+1);"` |
|     - | 1789 | `" }"` |
|     - | 1790 | `" public function getNamespaceName(){"` |
|     - | 1791 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1792 | `"  if($p === false){ return ''; }"` |
|     - | 1793 | `"  return substr($this->name,0,$p);"` |
|     - | 1794 | `" }"` |
|     - | 1795 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 1796 | `" public function isInternal(){ $i = $this->__rinfo(); return $i['internal']; }"` |
|     - | 1797 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 1798 | `" public function isInterface(){ $i = $this->__rinfo(); return $i['interface']; }"` |
|     - | 1799 | `" public function isTrait(){ $i = $this->__rinfo(); return $i['trait']; }"` |
|     - | 1800 | `" public function isAbstract(){ $i = $this->__rinfo(); return $i['abstract']; }"` |
|     - | 1801 | `" public function isFinal(){ $i = $this->__rinfo(); return $i['final']; }"` |
|     - | 1802 | `" public function isReadOnly(){ $i = $this->__rinfo(); return $i['readonly']; }"` |
|     - | 1803 | `" public function isEnum(){ $i = $this->__rinfo(); return $i['enum']; }"` |
|     - | 1804 | `" public function isAnonymous(){ return strpos($this->name,'class@anonymous') === 0; }"` |
|     - | 1805 | `" public function getModifiers(){"` |
|     - | 1806 | `"  $i = $this->__rinfo();"` |
|     - | 1807 | `"  $m = 0;"` |
|     - | 1808 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 1809 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 1810 | `"  if($i['readonly']){ $m \|= 65536; }"` |
|     - | 1811 | `"  return $m;"` |
|     - | 1812 | `" }"` |
|     - | 1813 | `" public function getParentClass(){"` |
|     - | 1814 | `"  $i = $this->__rinfo();"` |
|     - | 1815 | `"  if($i['parent'] === null){ return false; }"` |
|     - | 1816 | `"  return new ReflectionClass($i['parent']);"` |
|     - | 1817 | `" }"` |
|     - | 1818 | `" public function getInterfaceNames(){ $i = $this->__rinfo(); return $i['interfaces']; }"` |
|     - | 1819 | `" public function getInterfaces(){"` |
|     - | 1820 | `"  $i = $this->__rinfo();"` |
|     - | 1821 | `"  $out = array();"` |
|     - | 1822 | `"  foreach($i['interfaces'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1823 | `"  return $out;"` |
|     - | 1824 | `" }"` |
|     - | 1825 | `" public function getTraitNames(){ $i = $this->__rinfo(); return $i['traits']; }"` |
|     - | 1826 | `" public function getTraits(){"` |
|     - | 1827 | `"  $i = $this->__rinfo();"` |
|     - | 1828 | `"  $out = array();"` |
|     - | 1829 | `"  foreach($i['traits'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1830 | `"  return $out;"` |
|     - | 1831 | `" }"` |
|     - | 1832 | `" public function getTraitAliases(){ return array(); }"` |
|     - | 1833 | `" public function implementsInterface($interface){"` |
|     - | 1834 | `"  if($interface instanceof ReflectionClass){ $interface = $interface->name; }"` |
|     - | 1835 | `"  $target = __phl_rcinfo($interface);"` |
|     - | 1836 | `"  if($target === null){"` |
|     - | 1837 | `"   throw new ReflectionException('Interface \"'.$interface.'\" does not exist');"` |
|     - | 1838 | `"  }"` |
|     - | 1839 | `"  if(!$target['interface']){"` |
|     - | 1840 | `"   throw new ReflectionException($target['name'].' is not an interface');"` |
|     - | 1841 | `"  }"` |
|     - | 1842 | `"  $name = $target['name'];"` |
|     - | 1843 | `"  if($this->name === $name){ return true; }"` |
|     - | 1844 | `"  $i = $this->__rinfo();"` |
|     - | 1845 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1846 | `"  return false;"` |
|     - | 1847 | `" }"` |
|     - | 1848 | `" public function isSubclassOf($class){"` |
|     - | 1849 | `"  if($class instanceof ReflectionClass){ $class = $class->name; }"` |
|     - | 1850 | `"  $target = __phl_rcinfo($class);"` |
|     - | 1851 | `"  if($target === null){"` |
|     - | 1852 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 1853 | `"  }"` |
|     - | 1854 | `"  $name = $target['name'];"` |
|     - | 1855 | `"  if($name === $this->name){ return false; }"` |
|     - | 1856 | `"  $i = $this->__rinfo();"` |
|     - | 1857 | `"  $p = $i['parent'];"` |
|     - | 1858 | `"  while($p !== null){"` |
|     - | 1859 | `"   if($p === $name){ return true; }"` |
|     - | 1860 | `"   $pi = __phl_rcinfo($p);"` |
|     - | 1861 | `"   $p = $pi['parent'];"` |
|     - | 1862 | `"  }"` |
|     - | 1863 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1864 | `"  return false;"` |
|     - | 1865 | `" }"` |
|     - | 1866 | `" public function isInstance($object){"` |
|     - | 1867 | `"  if(!is_object($object)){"` |
|     - | 1868 | `"   throw new TypeError('ReflectionClass::isInstance(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 1869 | `"  }"` |
|     - | 1870 | `"  return is_a($object,$this->name);"` |
|     - | 1871 | `" }"` |
|     - | 1872 | `" public function hasMethod($name){"` |
|     - | 1873 | `"  $i = $this->__rinfo();"` |
|     - | 1874 | `"  $l = strtolower($name);"` |
|     - | 1875 | `"  foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ return true; } }"` |
|     - | 1876 | `"  return false;"` |
|     - | 1877 | `" }"` |
|     - | 1878 | `" public function hasProperty($name){"` |
|     - | 1879 | `"  $i = $this->__rinfo();"` |
|     - | 1880 | `"  if(isset($i['props'][$name])){ return true; }"` |
|     - | 1881 | `"  if($this->__obj !== null){ return (__reflect_prop_state($this->__obj, $name) & 1) !== 0; }"` |
|     - | 1882 | `"  return false;"` |
|     - | 1883 | `" }"` |
|     - | 1884 | `" public function hasConstant($name){ $i = $this->__rinfo(); return isset($i['consts'][$name]); }"` |
|     - | 1885 | `" public function getConstant($name){"` |
|     - | 1886 | `"  $i = $this->__rinfo();"` |
|     - | 1887 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 1888 | `"  return __reflect_const_value($this->name,$name);"` |
|     - | 1889 | `" }"` |
|     - | 1890 | `" public function getConstants($filter = null){"` |
|     - | 1891 | `"  $i = $this->__rinfo();"` |
|     - | 1892 | `"  $out = array();"` |
|     - | 1893 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 1894 | `"   if($filter !== null){"` |
|     - | 1895 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 1896 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 1897 | `"   }"` |
|     - | 1898 | `"   $out[$k] = __reflect_const_value($this->name,$k);"` |
|     - | 1899 | `"  }"` |
|     - | 1900 | `"  return $out;"` |
|     - | 1901 | `" }"` |
|     - | 1902 | `" public function getStartLine(){"` |
|     - | 1903 | `"  $i = $this->__rinfo();"` |
|     - | 1904 | `"  if($i['internal']){ return false; }"` |
|     - | 1905 | `"  return $i['line'];"` |
|     - | 1906 | `" }"` |
|     - | 1907 | `" public function getEndLine(){"` |
|     - | 1908 | `"  $i = $this->__rinfo();"` |
|     - | 1909 | `"  if($i['internal']){ return false; }"` |
|     - | 1910 | `"  return $i['endline'];"` |
|     - | 1911 | `" }"` |
|     - | 1912 | `" public function getFileName(){ $i = $this->__rinfo(); return $i['file']; }"` |
|     - | 1913 | `" public function getDocComment(){ $i = $this->__rinfo(); return $i['doc']; }"` |
|     - | 1914 | `" public function isInstantiable(){"` |
|     - | 1915 | `"  $i = $this->__rinfo();"` |
|     - | 1916 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract'] \|\| $i['enum']){ return false; }"` |
|     - | 1917 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){ return false; }"` |
|     - | 1918 | `"  return true;"` |
|     - | 1919 | `" }"` |
|     - | 1920 | `" public function isCloneable(){"` |
|     - | 1921 | `"  $i = $this->__rinfo();"` |
|     - | 1922 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1923 | `"  if($i['clonevis'] !== 0 && $i['clonevis'] !== 1){ return false; }"` |
|     - | 1924 | `"  return true;"` |
|     - | 1925 | `" }"` |
|     - | 1926 | `" public function isIterable(){"` |
|     - | 1927 | `"  $i = $this->__rinfo();"` |
|     - | 1928 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1929 | `"  return $i['iterable'];"` |
|     - | 1930 | `" }"` |
|     - | 1931 | `" public function isIterateable(){ return $this->isIterable(); }"` |
|     - | 1932 | `" public function newInstance(...$args){ return $this->__rnew($args); }"` |
|     - | 1933 | `" public function newInstanceArgs(array $args = array()){ return $this->__rnew($args); }"` |
|     - | 1934 | `" protected function __rnew($args){"` |
|     - | 1935 | `"  $i = $this->__rinfo();"` |
|     - | 1936 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1937 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){"` |
|     - | 1938 | `"   throw new ReflectionException('Access to non-public constructor of class '.$this->name);"` |
|     - | 1939 | `"  }"` |
|     - | 1940 | `"  if($i['ctorvis'] === 0 && count($args) > 0){"` |
|     - | 1941 | `"   throw new ReflectionException('Class '.$this->name.' does not have a constructor, so you cannot pass any constructor arguments');"` |
|     - | 1942 | `"  }"` |
|     - | 1943 | `"  return __reflect_new_instance($this->name,$args);"` |
|     - | 1944 | `" }"` |
|     - | 1945 | `" protected function __rcheckInstantiable($i){"` |
|     - | 1946 | `"  if($i['interface']){ throw new Error('Cannot instantiate interface '.$this->name); }"` |
|     - | 1947 | `"  if($i['trait']){ throw new Error('Cannot instantiate trait '.$this->name); }"` |
|     - | 1948 | `"  if($i['abstract']){ throw new Error('Cannot instantiate abstract class '.$this->name); }"` |
|     - | 1949 | `" }"` |
|     - | 1950 | `" public function newInstanceWithoutConstructor(){"` |
|     - | 1951 | `"  $i = $this->__rinfo();"` |
|     - | 1952 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1953 | `"  return __reflect_new_no_ctor($this->name);"` |
|     - | 1954 | `" }"` |
|     - | 1955 | `" public function getStaticProperties(){"` |
|     - | 1956 | `"  $i = $this->__rinfo();"` |
|     - | 1957 | `"  $out = array();"` |
|     - | 1958 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1959 | `"   if($p['static']){ $out[$k] = __reflect_static_value($this->name,$k); }"` |
|     - | 1960 | `"  }"` |
|     - | 1961 | `"  return $out;"` |
|     - | 1962 | `" }"` |
|     - | 1963 | `" public function getStaticPropertyValue($name, ...$def){"` |
|     - | 1964 | `"  $i = $this->__rinfo();"` |
|     - | 1965 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 1966 | `"   if(count($def) > 0){ return $def[0]; }"` |
|     - | 1967 | `"   throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 1968 | `"  }"` |
|     - | 1969 | `"  return __reflect_static_value($this->name,$name);"` |
|     - | 1970 | `" }"` |
|     - | 1971 | `" public function setStaticPropertyValue($name,$value){"` |
|     - | 1972 | `"  $i = $this->__rinfo();"` |
|     - | 1973 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 1974 | `"   throw new ReflectionException('Class '.$this->name.' does not have a property named '.$name);"` |
|     - | 1975 | `"  }"` |
|     - | 1976 | `"  __reflect_static_set($this->name,$name,$value);"` |
|     - | 1977 | `" }"` |
|     - | 1978 | `" public function getDefaultProperties(){"` |
|     - | 1979 | `"  $i = $this->__rinfo();"` |
|     - | 1980 | `"  $out = array();"` |
|     - | 1981 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1982 | `"   if($p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 1983 | `"  }"` |
|     - | 1984 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1985 | `"   if(!$p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 1986 | `"  }"` |
|     - | 1987 | `"  return $out;"` |
|     - | 1988 | `" }"` |
|     - | 1989 | `" public function getProperty($name){"` |
|     - | 1990 | `"  $i = $this->__rinfo();"` |
|     - | 1991 | `"  if(isset($i['props'][$name])){"` |
|     - | 1992 | `"   return new ReflectionProperty($this->name, $name);"` |
|     - | 1993 | `"  }"` |
|     - | 1994 | `"  if($this->__obj !== null && (__reflect_prop_state($this->__obj, $name) & 1)){"` |
|     - | 1995 | `"   return new ReflectionProperty($this->__obj, $name);"` |
|     - | 1996 | `"  }"` |
|     - | 1997 | `"  throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 1998 | `" }"` |
|     - | 1999 | `" public function getProperties($filter = null){"` |
|     - | 2000 | `"  $i = $this->__rinfo();"` |
|     - | 2001 | `"  $out = array();"` |
|     - | 2002 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 2003 | `"   if($filter !== null){"` |
|     - | 2004 | `"    $m = ($p['vis'] === 1 ? 1 : ($p['vis'] === 2 ? 2 : 4));"` |
|     - | 2005 | `"    if($p['static']){ $m \|= 16; }"` |
|     - | 2006 | `"    if($p['readonly']){ $m \|= 128; }"` |
|     - | 2007 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 2008 | `"   }"` |
|     - | 2009 | `"   $out[] = new ReflectionProperty($this->name, $k);"` |
|     - | 2010 | `"  }"` |
|     - | 2011 | `"  if($this->__obj !== null){"` |
|     - | 2012 | `"   foreach(__reflect_dyn_props($this->__obj) as $k){"` |
|     - | 2013 | `"    if(isset($i['props'][$k])){ continue; }"` |
|     - | 2014 | `"    if($filter !== null && ($filter & 1) === 0){ continue; }"` |
|     - | 2015 | `"    $out[] = new ReflectionProperty($this->__obj, $k);"` |
|     - | 2016 | `"   }"` |
|     - | 2017 | `"  }"` |
|     - | 2018 | `"  return $out;"` |
|     - | 2019 | `" }"` |
|     - | 2020 | `" public function getMethod($name){"` |
|     - | 2021 | `"  $i = $this->__rinfo();"` |
|     - | 2022 | `"  $found = null;"` |
|     - | 2023 | `"  if(isset($i['methods'][$name])){"` |
|     - | 2024 | `"   $found = $name;"` |
|     - | 2025 | `"  }else{"` |
|     - | 2026 | `"   $l = strtolower($name);"` |
|     - | 2027 | `"   foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ $found = $k; break; } }"` |
|     - | 2028 | `"  }"` |
|     - | 2029 | `"  if($found === null){"` |
|     - | 2030 | `"   throw new ReflectionException('Method '.$this->name.'::'.$name.'() does not exist');"` |
|     - | 2031 | `"  }"` |
|     - | 2032 | `"  return new ReflectionMethod($this->name, $found);"` |
|     - | 2033 | `" }"` |
|     - | 2034 | `" public function getMethods($filter = null){"` |
|     - | 2035 | `"  $i = $this->__rinfo();"` |
|     - | 2036 | `"  $out = array();"` |
|     - | 2037 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 2038 | `"   if($filter !== null){"` |
|     - | 2039 | `"    $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2040 | `"    if($m['static']){ $mod \|= 16; }"` |
|     - | 2041 | `"    if($m['abstract']){ $mod \|= 64; }"` |
|     - | 2042 | `"    if($m['final']){ $mod \|= 32; }"` |
|     - | 2043 | `"    if(($mod & $filter) === 0){ continue; }"` |
|     - | 2044 | `"   }"` |
|     - | 2045 | `"   $out[] = new ReflectionMethod($this->name, $k);"` |
|     - | 2046 | `"  }"` |
|     - | 2047 | `"  return $out;"` |
|     - | 2048 | `" }"` |
|     - | 2049 | `" public function getConstructor(){"` |
|     - | 2050 | `"  $i = $this->__rinfo();"` |
|     - | 2051 | `"  if(isset($i['methods']['__construct'])){"` |
|     - | 2052 | `"   return new ReflectionMethod($this->name, '__construct');"` |
|     - | 2053 | `"  }"` |
|     - | 2054 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 2055 | `"   if(strtolower($k) === '__construct'){ return new ReflectionMethod($this->name, $k); }"` |
|     - | 2056 | `"  }"` |
|     - | 2057 | `"  if($i['ctorvis'] !== 0 && isset($i['methods'][$this->name])){"` |
|     - | 2058 | `"   return new ReflectionMethod($this->name, $this->name);"` |
|     - | 2059 | `"  }"` |
|     - | 2060 | `"  return null;"` |
|     - | 2061 | `" }"` |
|     - | 2062 | `" public function getReflectionConstant($name){"` |
|     - | 2063 | `"  $i = $this->__rinfo();"` |
|     - | 2064 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 2065 | `"  return new ReflectionClassConstant($this->name, $name);"` |
|     - | 2066 | `" }"` |
|     - | 2067 | `" public function getReflectionConstants($filter = null){"` |
|     - | 2068 | `"  $i = $this->__rinfo();"` |
|     - | 2069 | `"  $out = array();"` |
|     - | 2070 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 2071 | `"   if($filter !== null){"` |
|     - | 2072 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 2073 | `"    if($c['final']){ $m \|= 32; }"` |
|     - | 2074 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 2075 | `"   }"` |
|     - | 2076 | `"   $out[] = new ReflectionClassConstant($this->name, $k);"` |
|     - | 2077 | `"  }"` |
|     - | 2078 | `"  return $out;"` |
|     - | 2079 | `" }"` |
|     - | 2080 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2081 | `"  $i = $this->__rinfo();"` |
|     - | 2082 | `"  return __reflect_build_attrs($i['attrs'], array('class', $this->name, null, 0), 1, $name, $flags);"` |
|     - | 2083 | `" }"` |
|     - | 2084 | `" public function getExtensionName(){ $i = $this->__rinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2085 | `" public function getExtension(){ $i = $this->__rinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2086 | `" public function newLazyGhost($initializer, $options = 0){"` |
|     - | 2087 | `"  throw new Error('ReflectionClass::newLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2088 | `" }"` |
|     - | 2089 | `" public function newLazyProxy($factory, $options = 0){"` |
|     - | 2090 | `"  throw new Error('ReflectionClass::newLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2091 | `" }"` |
|     - | 2092 | `" public function resetAsLazyGhost($object, $initializer, $options = 0){"` |
|     - | 2093 | `"  throw new Error('ReflectionClass::resetAsLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2094 | `" }"` |
|     - | 2095 | `" public function resetAsLazyProxy($object, $factory, $options = 0){"` |
|     - | 2096 | `"  throw new Error('ReflectionClass::resetAsLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2097 | `" }"` |
|     - | 2098 | `" public function getLazyInitializer($object){ return null; }"` |
|     - | 2099 | `" public function initializeLazyObject($object){ return $object; }"` |
|     - | 2100 | `" public function markLazyObjectAsInitialized($object){ return $object; }"` |
|     - | 2101 | `" public function isUninitializedLazyObject($object){ return false; }"` |
|     - | 2102 | `" public function __toString(){ return __reflect_export_class($this); }"` |
|     - | 2103 | `"}"` |
|     - | 2104 | `"class ReflectionObject extends ReflectionClass {"` |
|     - | 2105 | `" public function __construct($object){"` |
|     - | 2106 | `"  if(!is_object($object)){"` |
|     - | 2107 | `"   throw new TypeError('ReflectionObject::__construct(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 2108 | `"  }"` |
|     - | 2109 | `"  parent::__construct($object);"` |
|     - | 2110 | `"  $this->__obj = $object;"` |
|     - | 2111 | `" }"` |
|     - | 2112 | `"}"` |
|     - | 2113 | `;` |
|     - | 2114 | `/*` |
|     - | 2115 | ` * Chunk 2: ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,` |
|     - | 2116 | ` * ReflectionParameter.` |
|     - | 2117 | ` */` |
|     - | 2118 | `static const char zReflectLib2[] =` |
|     - | 2119 | `"abstract class ReflectionFunctionAbstract implements Reflector {"` |
|     - | 2120 | `" public $name;"` |
|     - | 2121 | `" protected $__cl = null;"` |
|     - | 2122 | `" protected function __rfinfo(){"` |
|     - | 2123 | `"  if($this->__cl !== null){ return __reflect_sig_fixup(__reflect_func_info($this->__cl)); }"` |
|     - | 2124 | `"  return __reflect_sig_fixup(__reflect_func_info($this->name));"` |
|     - | 2125 | `" }"` |
|     - | 2126 | `" protected function __rftarget(){ return $this->__cl !== null ? $this->__cl : $this->name; }"` |
|     - | 2127 | `" protected function __rpspec(){ return $this->__rftarget(); }"` |
|     - | 2128 | `" public function getName(){ return $this->name; }"` |
|     - | 2129 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 2130 | `" public function getNamespaceName(){"` |
|     - | 2131 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2132 | `"  if($p === false){ return ''; }"` |
|     - | 2133 | `"  return substr($this->name,0,$p);"` |
|     - | 2134 | `" }"` |
|     - | 2135 | `" public function getShortName(){"` |
|     - | 2136 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2137 | `"  if($p === false){ return $this->name; }"` |
|     - | 2138 | `"  return substr($this->name,$p+1);"` |
|     - | 2139 | `" }"` |
|     - | 2140 | `" public function isClosure(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2141 | `" public function isGenerator(){ $i = $this->__rfinfo(); return $i['generator']; }"` |
|     - | 2142 | `" public function isVariadic(){ $i = $this->__rfinfo(); return $i['variadic']; }"` |
|     - | 2143 | `" public function returnsReference(){ $i = $this->__rfinfo(); return $i['byref']; }"` |
|     - | 2144 | `" public function isInternal(){ $i = $this->__rfinfo(); return $i['internal']; }"` |
|     - | 2145 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 2146 | `" public function isDeprecated(){ $i = $this->__rfinfo(); return __reflect_has_deprecated($i['attrs']); }"` |
|     - | 2147 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['fstatic']; }"` |
|     - | 2148 | `" public function getFileName(){ $i = $this->__rfinfo(); return $i['file']; }"` |
|     - | 2149 | `" public function getStartLine(){"` |
|     - | 2150 | `"  $i = $this->__rfinfo();"` |
|     - | 2151 | `"  if($i['internal']){ return false; }"` |
|     - | 2152 | `"  return $i['line'];"` |
|     - | 2153 | `" }"` |
|     - | 2154 | `" public function getEndLine(){"` |
|     - | 2155 | `"  $i = $this->__rfinfo();"` |
|     - | 2156 | `"  if($i['internal']){ return false; }"` |
|     - | 2157 | `"  return $i['endline'];"` |
|     - | 2158 | `" }"` |
|     - | 2159 | `" public function getDocComment(){ $i = $this->__rfinfo(); return $i['doc']; }"` |
|     - | 2160 | `" public function hasReturnType(){ $i = $this->__rfinfo(); return $i['rettext'] !== null; }"` |
|     - | 2161 | `" public function getReturnType(){ $i = $this->__rfinfo(); return __reflect_make_type($i['rettext']); }"` |
|     - | 2162 | `" public function hasTentativeReturnType(){ return false; }"` |
|     - | 2163 | `" public function getTentativeReturnType(){ return null; }"` |
|     - | 2164 | `" public function getNumberOfParameters(){"` |
|     - | 2165 | `"  $i = $this->__rfinfo();"` |
|     - | 2166 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2167 | `"  return count($i['params']);"` |
|     - | 2168 | `" }"` |
|     - | 2169 | `" public function getNumberOfRequiredParameters(){"` |
|     - | 2170 | `"  $i = $this->__rfinfo();"` |
|     - | 2171 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2172 | `"  $req = 0;"` |
|     - | 2173 | `"  $n = count($i['params']);"` |
|     - | 2174 | `"  for($k = $n - 1; $k >= 0; $k--){"` |
|     - | 2175 | `"   $p = $i['params'][$k];"` |
|     - | 2176 | `"   if(!$p['variadic'] && !$p['hasdef']){ $req = $k + 1; break; }"` |
|     - | 2177 | `"  }"` |
|     - | 2178 | `"  return $req;"` |
|     - | 2179 | `" }"` |
|     - | 2180 | `" public function getParameters(){"` |
|     - | 2181 | `"  $i = $this->__rfinfo();"` |
|     - | 2182 | `"  $out = array();"` |
|     - | 2183 | `"  $spec = $this->__rpspec();"` |
|     - | 2184 | `"  foreach($i['params'] as $p){"` |
|     - | 2185 | `"   $out[] = new ReflectionParameter($spec, $p['pos']);"` |
|     - | 2186 | `"  }"` |
|     - | 2187 | `"  return $out;"` |
|     - | 2188 | `" }"` |
|     - | 2189 | `" public function getStaticVariables(){ $i = $this->__rfinfo(); return $i['statics']; }"` |
|     - | 2190 | `" public function getClosureThis(){"` |
|     - | 2191 | `"  $i = $this->__rfinfo();"` |
|     - | 2192 | `"  return isset($i['this']) ? $i['this'] : null;"` |
|     - | 2193 | `" }"` |
|     - | 2194 | `" public function getClosureScopeClass(){"` |
|     - | 2195 | `"  $i = $this->__rfinfo();"` |
|     - | 2196 | `"  if(isset($i['scope'])){ return new ReflectionClass($i['scope']); }"` |
|     - | 2197 | `"  if(isset($i['this'])){ return new ReflectionClass(get_class($i['this'])); }"` |
|     - | 2198 | `"  return null;"` |
|     - | 2199 | `" }"` |
|     - | 2200 | `" public function getClosureCalledClass(){ return $this->getClosureScopeClass(); }"` |
|     - | 2201 | `" public function getClosureUsedVariables(){"` |
|     - | 2202 | `"  $i = $this->__rfinfo();"` |
|     - | 2203 | `"  return isset($i['used']) ? $i['used'] : array();"` |
|     - | 2204 | `" }"` |
|     - | 2205 | `" public function getExtensionName(){ $i = $this->__rfinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2206 | `" public function getExtension(){ $i = $this->__rfinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2207 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2208 | `"  $i = $this->__rfinfo();"` |
|     - | 2209 | `"  if($this instanceof ReflectionMethod){"` |
|     - | 2210 | `"   $spec = array('method', $this->class, $this->name, 0);"` |
|     - | 2211 | `"   $target = 4;"` |
|     - | 2212 | `"  }else{"` |
|     - | 2213 | `"   $spec = array('fn', $this->__rftarget(), null, 0);"` |
|     - | 2214 | `"   $target = 2;"` |
|     - | 2215 | `"  }"` |
|     - | 2216 | `"  return __reflect_build_attrs($i['attrs'], $spec, $target, $name, $flags);"` |
|     - | 2217 | `" }"` |
|     - | 2218 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2219 | `"}"` |
|     - | 2220 | `"class ReflectionFunction extends ReflectionFunctionAbstract {"` |
|     - | 2221 | `" const IS_DEPRECATED = 2048;"` |
|     - | 2222 | `" public function __construct($function){"` |
|     - | 2223 | `"  if($function instanceof Closure){"` |
|     - | 2224 | `"   $this->__cl = $function;"` |
|     - | 2225 | `"   $i = $this->__rfinfo();"` |
|     - | 2226 | `"   if($i['closure']){"` |
|     - | 2227 | `"    $f = $i['file'] === false ? '' : $i['file'];"` |
|     - | 2228 | `"    $this->name = '{closure:'.$f.':'.$i['line'].'}';"` |
|     - | 2229 | `"   }else{"` |
|     - | 2230 | `"    $this->name = $i['name'];"` |
|     - | 2231 | `"   }"` |
|     - | 2232 | `"   return;"` |
|     - | 2233 | `"  }"` |
|     - | 2234 | `"  if(!is_string($function)){"` |
|     - | 2235 | `"   throw new TypeError('ReflectionFunction::__construct(): Argument #1 ($function) must be of type Closure\|string, '.get_debug_type($function).' given');"` |
|     - | 2236 | `"  }"` |
|     - | 2237 | `"  $i = __reflect_func_info($function);"` |
|     - | 2238 | `"  if($i === null){"` |
|     - | 2239 | `"   throw new ReflectionException('Function '.$function.'() does not exist');"` |
|     - | 2240 | `"  }"` |
|     - | 2241 | `"  if($i['closure']){"` |
|     - | 2242 | `"   $this->name = '{closure:'.($i['file'] === false ? '' : $i['file']).':'.$i['line'].'}';"` |
|     - | 2243 | `"   $this->__cl = __reflect_closure($function, null, null);"` |
|     - | 2244 | `"  }else{"` |
|     - | 2245 | `"   $this->name = $i['name'];"` |
|     - | 2246 | `"  }"` |
|     - | 2247 | `" }"` |
|     - | 2248 | `" public function invoke(...$args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2249 | `" public function invokeArgs(array $args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2250 | `" public function getClosure(){"` |
|     - | 2251 | `"  if($this->__cl !== null){ return $this->__cl; }"` |
|     - | 2252 | `"  return __reflect_closure($this->name, null, null);"` |
|     - | 2253 | `" }"` |
|     - | 2254 | `" public function isAnonymous(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2255 | `" public function isDisabled(){ return false; }"` |
|     - | 2256 | `"}"` |
|     - | 2257 | `"class ReflectionMethod extends ReflectionFunctionAbstract {"` |
|     - | 2258 | `" const IS_PUBLIC = 1;"` |
|     - | 2259 | `" const IS_PROTECTED = 2;"` |
|     - | 2260 | `" const IS_PRIVATE = 4;"` |
|     - | 2261 | `" const IS_STATIC = 16;"` |
|     - | 2262 | `" const IS_FINAL = 32;"` |
|     - | 2263 | `" const IS_ABSTRACT = 64;"` |
|     - | 2264 | `" public $class;"` |
|     - | 2265 | `" public function __construct($objectOrMethod, $method = null){"` |
|     - | 2266 | `"  if($method === null){"` |
|     - | 2267 | `"   if(!is_string($objectOrMethod) \|\| strpos($objectOrMethod,'::') === false){"` |
|     - | 2268 | `"    throw new TypeError('ReflectionMethod::__construct(): Argument #1 ($objectOrMethod) must be of type object\|string, '.get_debug_type($objectOrMethod).' given');"` |
|     - | 2269 | `"   }"` |
|     - | 2270 | `"   $p = strpos($objectOrMethod,'::');"` |
|     - | 2271 | `"   $method = substr($objectOrMethod,$p+2);"` |
|     - | 2272 | `"   $objectOrMethod = substr($objectOrMethod,0,$p);"` |
|     - | 2273 | `"  }"` |
|     - | 2274 | `"  $ci = __phl_rcinfo($objectOrMethod);"` |
|     - | 2275 | `"  if($ci === null){"` |
|     - | 2276 | `"   throw new ReflectionException('Class \"'.$objectOrMethod.'\" does not exist');"` |
|     - | 2277 | `"  }"` |
|     - | 2278 | `"  $this->class = $ci['name'];"` |
|     - | 2279 | `"  $found = null;"` |
|     - | 2280 | `"  if(isset($ci['methods'][$method])){"` |
|     - | 2281 | `"   $found = $method;"` |
|     - | 2282 | `"  }else{"` |
|     - | 2283 | `"   $l = strtolower($method);"` |
|     - | 2284 | `"   foreach($ci['methods'] as $k => $m){"` |
|     - | 2285 | `"    if(strtolower($k) === $l){ $found = $k; break; }"` |
|     - | 2286 | `"   }"` |
|     - | 2287 | `"  }"` |
|     - | 2288 | `"  if($found === null){"` |
|     - | 2289 | `"   throw new ReflectionException('Method '.$this->class.'::'.$method.'() does not exist');"` |
|     - | 2290 | `"  }"` |
|     - | 2291 | `"  $this->name = $found;"` |
|     - | 2292 | `" }"` |
|     - | 2293 | `" public static function createFromMethodName($name){"` |
|     - | 2294 | `"  return new ReflectionMethod($name);"` |
|     - | 2295 | `" }"` |
|     - | 2296 | `" protected function __rfinfo(){ return __reflect_func_info($this->class, $this->name); }"` |
|     - | 2297 | `" protected function __rpspec(){ return array($this->class, $this->name); }"` |
|     - | 2298 | `" public function getDeclaringClass(){"` |
|     - | 2299 | `"  $i = $this->__rfinfo();"` |
|     - | 2300 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2301 | `" }"` |
|     - | 2302 | `" public function getModifiers(){"` |
|     - | 2303 | `"  $i = $this->__rfinfo();"` |
|     - | 2304 | `"  $m = ($i['vis'] === 1 ? 1 : ($i['vis'] === 2 ? 2 : 4));"` |
|     - | 2305 | `"  if($i['mstatic']){ $m \|= 16; }"` |
|     - | 2306 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 2307 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 2308 | `"  return $m;"` |
|     - | 2309 | `" }"` |
|     - | 2310 | `" public function isPublic(){ $i = $this->__rfinfo(); return $i['vis'] === 1; }"` |
|     - | 2311 | `" public function isProtected(){ $i = $this->__rfinfo(); return $i['vis'] === 2; }"` |
|     - | 2312 | `" public function isPrivate(){ $i = $this->__rfinfo(); return $i['vis'] === 3; }"` |
|     - | 2313 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['mstatic']; }"` |
|     - | 2314 | `" public function isAbstract(){ $i = $this->__rfinfo(); return $i['abstract']; }"` |
|     - | 2315 | `" public function isFinal(){ $i = $this->__rfinfo(); return $i['final']; }"` |
|     - | 2316 | `" public function isConstructor(){ return strtolower($this->name) === '__construct'; }"` |
|     - | 2317 | `" public function isDestructor(){ return strtolower($this->name) === '__destruct'; }"` |
|     - | 2318 | `" public function invoke($object = null, ...$args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2319 | `" public function invokeArgs($object, array $args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2320 | `" protected function __rinvoke($object, $args){"` |
|     - | 2321 | `"  $i = $this->__rfinfo();"` |
|     - | 2322 | `"  if(!$i['mstatic']){"` |
|     - | 2323 | `"   if(!is_object($object)){"` |
|     - | 2324 | `"    throw new ReflectionException('Trying to invoke non static method '.$this->class.'::'.$this->name.'() without an object');"` |
|     - | 2325 | `"   }"` |
|     - | 2326 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2327 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2328 | `"   }"` |
|     - | 2329 | `"  }else{"` |
|     - | 2330 | `"   $object = null;"` |
|     - | 2331 | `"  }"` |
|     - | 2332 | `"  return __reflect_invoke($this->class, $this->name, $object, $args);"` |
|     - | 2333 | `" }"` |
|     - | 2334 | `" public function getClosure($object = null){"` |
|     - | 2335 | `"  $i = $this->__rfinfo();"` |
|     - | 2336 | `"  if(!$i['mstatic']){"` |
|     - | 2337 | `"   if($object === null){"` |
|     - | 2338 | `"    throw new ValueError('ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods');"` |
|     - | 2339 | `"   }"` |
|     - | 2340 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2341 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2342 | `"   }"` |
|     - | 2343 | `"  }else{"` |
|     - | 2344 | `"   $object = null;"` |
|     - | 2345 | `"  }"` |
|     - | 2346 | `"  return __reflect_closure($this->class, $this->name, $object);"` |
|     - | 2347 | `" }"` |
|     - | 2348 | `" public function setAccessible($accessible){ }"` |
|     - | 2349 | `" public function hasPrototype(){ return $this->__rproto() !== null; }"` |
|     - | 2350 | `" public function getPrototype(){"` |
|     - | 2351 | `"  $p = $this->__rproto();"` |
|     - | 2352 | `"  if($p === null){"` |
|     - | 2353 | `"   throw new ReflectionException('Method '.$this->class.'::'.$this->name.' does not have a prototype');"` |
|     - | 2354 | `"  }"` |
|     - | 2355 | `"  return new ReflectionMethod($p, $this->name);"` |
|     - | 2356 | `" }"` |
|     - | 2357 | `" protected function __rproto(){"` |
|     - | 2358 | `"  $ci = __phl_rcinfo($this->class);"` |
|     - | 2359 | `"  $l = strtolower($this->name);"` |
|     - | 2360 | `"  $p = $ci['parent'];"` |
|     - | 2361 | `"  while($p !== null){"` |
|     - | 2362 | `"   $pi = __phl_rcinfo($p);"` |
|     - | 2363 | `"   foreach($pi['methods'] as $k => $m){"` |
|     - | 2364 | `"    if(strtolower($k) === $l && $m['vis'] !== 3){ return $m['decl']; }"` |
|     - | 2365 | `"   }"` |
|     - | 2366 | `"   $p = $pi['parent'];"` |
|     - | 2367 | `"  }"` |
|     - | 2368 | `"  foreach($ci['interfaces'] as $if){"` |
|     - | 2369 | `"   $ii = __phl_rcinfo($if);"` |
|     - | 2370 | `"   foreach($ii['methods'] as $k => $m){"` |
|     - | 2371 | `"    if(strtolower($k) === $l){ return $ii['name']; }"` |
|     - | 2372 | `"   }"` |
|     - | 2373 | `"  }"` |
|     - | 2374 | `"  return null;"` |
|     - | 2375 | `" }"` |
|     - | 2376 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2377 | `"}"` |
|     - | 2378 | `"class ReflectionParameter implements Reflector {"` |
|     - | 2379 | `" public $name;"` |
|     - | 2380 | `" protected $__t;"` |
|     - | 2381 | `" protected $__m = null;"` |
|     - | 2382 | `" protected $__p = 0;"` |
|     - | 2383 | `" public function __construct($function, $param){"` |
|     - | 2384 | `"  $m = null;"` |
|     - | 2385 | `"  $t = $function;"` |
|     - | 2386 | `"  if(is_array($function)){"` |
|     - | 2387 | `"   $t = $function[0];"` |
|     - | 2388 | `"   $m = $function[1];"` |
|     - | 2389 | `"   if(is_object($t)){ $t = get_class($t); }"` |
|     - | 2390 | `"  }else if(is_string($function) && strpos($function,'::') !== false){"` |
|     - | 2391 | `"   $p = strpos($function,'::');"` |
|     - | 2392 | `"   $m = substr($function,$p+2);"` |
|     - | 2393 | `"   $t = substr($function,0,$p);"` |
|     - | 2394 | `"  }"` |
|     - | 2395 | `"  if($m !== null){"` |
|     - | 2396 | `"   $rm = new ReflectionMethod($t, $m);"` |
|     - | 2397 | `"   $t = $rm->class;"` |
|     - | 2398 | `"   $m = $rm->name;"` |
|     - | 2399 | `"   $i = __reflect_func_info($t, $m);"` |
|     - | 2400 | `"  }else if($function instanceof Closure){"` |
|     - | 2401 | `"   $t = $function;"` |
|     - | 2402 | `"   $i = __reflect_func_info($function);"` |
|     - | 2403 | `"  }else{"` |
|     - | 2404 | `"   $i = __reflect_sig_fixup(__reflect_func_info($t));"` |
|     - | 2405 | `"   if($i === null){"` |
|     - | 2406 | `"    throw new ReflectionException('Function '.$t.'() does not exist');"` |
|     - | 2407 | `"   }"` |
|     - | 2408 | `"  }"` |
|     - | 2409 | `"  $found = null;"` |
|     - | 2410 | `"  if(is_int($param)){"` |
|     - | 2411 | `"   if(isset($i['params'][$param])){ $found = $i['params'][$param]; }"` |
|     - | 2412 | `"   if($found === null){"` |
|     - | 2413 | `"    throw new ReflectionException('The parameter specified by its offset could not be found');"` |
|     - | 2414 | `"   }"` |
|     - | 2415 | `"  }else{"` |
|     - | 2416 | `"   foreach($i['params'] as $pp){"` |
|     - | 2417 | `"    if($pp['name'] === $param){ $found = $pp; break; }"` |
|     - | 2418 | `"   }"` |
|     - | 2419 | `"   if($found === null){"` |
|     - | 2420 | `"    throw new ReflectionException('The parameter specified by its name could not be found');"` |
|     - | 2421 | `"   }"` |
|     - | 2422 | `"  }"` |
|     - | 2423 | `"  $this->name = $found['name'];"` |
|     - | 2424 | `"  $this->__t = $t;"` |
|     - | 2425 | `"  $this->__m = $m;"` |
|     - | 2426 | `"  $this->__p = $found['pos'];"` |
|     - | 2427 | `" }"` |
|     - | 2428 | `" protected function __rffull(){"` |
|     - | 2429 | `"  if($this->__m !== null){ return __reflect_func_info($this->__t, $this->__m); }"` |
|     - | 2430 | `"  return __reflect_sig_fixup(__reflect_func_info($this->__t));"` |
|     - | 2431 | `" }"` |
|     - | 2432 | `" protected function __rpinfo(){"` |
|     - | 2433 | `"  $i = $this->__rffull();"` |
|     - | 2434 | `"  return $i['params'][$this->__p];"` |
|     - | 2435 | `" }"` |
|     - | 2436 | `" public function getName(){ return $this->name; }"` |
|     - | 2437 | `" public function getPosition(){ return $this->__p; }"` |
|     - | 2438 | `" public function isPassedByReference(){ $p = $this->__rpinfo(); return $p['byref']; }"` |
|     - | 2439 | `" public function canBePassedByValue(){ return !$this->isPassedByReference(); }"` |
|     - | 2440 | `" public function isVariadic(){ $p = $this->__rpinfo(); return $p['variadic']; }"` |
|     - | 2441 | `" public function isPromoted(){ $p = $this->__rpinfo(); return $p['promoted']; }"` |
|     - | 2442 | `" public function isDefaultValueAvailable(){ $p = $this->__rpinfo(); return $p['hasdef']; }"` |
|     - | 2443 | `" public function isOptional(){"` |
|     - | 2444 | `"  $i = $this->__rffull();"` |
|     - | 2445 | `"  $n = count($i['params']);"` |
|     - | 2446 | `"  for($k = $this->__p; $k < $n; $k++){"` |
|     - | 2447 | `"   $p = $i['params'][$k];"` |
|     - | 2448 | `"   if(!$p['variadic'] && !$p['hasdef']){ return false; }"` |
|     - | 2449 | `"  }"` |
|     - | 2450 | `"  return true;"` |
|     - | 2451 | `" }"` |
|     - | 2452 | `" public function getDefaultValue(){"` |
|     - | 2453 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2454 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2455 | `"  }"` |
|     - | 2456 | `"  $p = $this->__rpinfo();"` |
|     - | 2457 | `"  if(isset($p['deftext'])){"` |
|     - | 2458 | `"   $s = __reflect_sig_scalar($p['deftext']);"` |
|     - | 2459 | `"   if($s[0]){ return $s[1]; }"` |
|     - | 2460 | `"   if($p['deftext'] === 'array (' \|\| strpos($p['deftext'], '[') === 0){ return array(); }"` |
|     - | 2461 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2462 | `"  }"` |
|     - | 2463 | `"  return __reflect_param_default($this->__t, $this->__m, $this->__p);"` |
|     - | 2464 | `" }"` |
|     - | 2465 | `" public function isDefaultValueConstant(){"` |
|     - | 2466 | `"  if(!$this->isDefaultValueAvailable()){ return false; }"` |
|     - | 2467 | `"  $p = $this->__rpinfo();"` |
|     - | 2468 | `"  if(isset($p['deftext'])){ return false; }"` |
|     - | 2469 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p) !== null;"` |
|     - | 2470 | `" }"` |
|     - | 2471 | `" public function getDefaultValueConstantName(){"` |
|     - | 2472 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2473 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2474 | `"  }"` |
|     - | 2475 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p);"` |
|     - | 2476 | `" }"` |
|     - | 2477 | `" public function allowsNull(){"` |
|     - | 2478 | `"  $p = $this->__rpinfo();"` |
|     - | 2479 | `"  if($p['typetext'] === null){ return true; }"` |
|     - | 2480 | `"  if($p['nullable']){ return true; }"` |
|     - | 2481 | `"  return $p['typetext'] === 'mixed' \|\| $p['typetext'] === 'null';"` |
|     - | 2482 | `" }"` |
|     - | 2483 | `" public function hasType(){ $p = $this->__rpinfo(); return $p['typetext'] !== null; }"` |
|     - | 2484 | `" public function getType(){ $p = $this->__rpinfo(); return __reflect_make_type($p['typetext']); }"` |
|     - | 2485 | `" public function getDeclaringFunction(){"` |
|     - | 2486 | `"  if($this->__m !== null){ return new ReflectionMethod($this->__t, $this->__m); }"` |
|     - | 2487 | `"  return new ReflectionFunction($this->__t);"` |
|     - | 2488 | `" }"` |
|     - | 2489 | `" public function getDeclaringClass(){"` |
|     - | 2490 | `"  if($this->__m === null){ return null; }"` |
|     - | 2491 | `"  $i = $this->__rffull();"` |
|     - | 2492 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2493 | `" }"` |
|     - | 2494 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2495 | `"  $p = $this->__rpinfo();"` |
|     - | 2496 | `"  return __reflect_build_attrs($p['attrs'], array('param', $this->__t, $this->__m, $this->__p), 32, $name, $flags);"` |
|     - | 2497 | `" }"` |
|     - | 2498 | `" public function __toString(){ return __reflect_export_param($this); }"` |
|     - | 2499 | `"}"` |
|     - | 2500 | `;` |
|     - | 2501 | `/*` |
|     - | 2502 | ` * Chunk 3: PropertyHookType, ReflectionProperty, ReflectionClassConstant.` |
|     - | 2503 | ` */` |
|     - | 2504 | `static const char zReflectLib3[] =` |
|     - | 2505 | `"enum PropertyHookType: string {"` |
|     - | 2506 | `" case Get = 'get';"` |
|     - | 2507 | `" case Set = 'set';"` |
|     - | 2508 | `"}"` |
|     - | 2509 | `"class ReflectionProperty implements Reflector {"` |
|     - | 2510 | `" const IS_PUBLIC = 1;"` |
|     - | 2511 | `" const IS_PROTECTED = 2;"` |
|     - | 2512 | `" const IS_PRIVATE = 4;"` |
|     - | 2513 | `" const IS_STATIC = 16;"` |
|     - | 2514 | `" const IS_FINAL = 32;"` |
|     - | 2515 | `" const IS_ABSTRACT = 64;"` |
|     - | 2516 | `" const IS_READONLY = 128;"` |
|     - | 2517 | `" const IS_VIRTUAL = 512;"` |
|     - | 2518 | `" const IS_PROTECTED_SET = 2048;"` |
|     - | 2519 | `" const IS_PRIVATE_SET = 4096;"` |
|     - | 2520 | `" public $name;"` |
|     - | 2521 | `" public $class;"` |
|     - | 2522 | `" protected $__dynobj = null;"` |
|     - | 2523 | `" public function __construct($class, $property){"` |
|     - | 2524 | `"  $obj = null;"` |
|     - | 2525 | `"  if(is_object($class)){ $obj = $class; }"` |
|     - | 2526 | `"  else if(!is_string($class)){"` |
|     - | 2527 | `"   throw new TypeError('ReflectionProperty::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2528 | `"  }"` |
|     - | 2529 | `"  $ci = __phl_rcinfo($class);"` |
|     - | 2530 | `"  if($ci === null){"` |
|     - | 2531 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2532 | `"  }"` |
|     - | 2533 | `"  $this->class = $ci['name'];"` |
|     - | 2534 | `"  if(isset($ci['props'][$property])){"` |
|     - | 2535 | `"   $this->name = $property;"` |
|     - | 2536 | `"   return;"` |
|     - | 2537 | `"  }"` |
|     - | 2538 | `"  if($obj !== null && (__reflect_prop_state($obj, $property) & 1)){"` |
|     - | 2539 | `"   $this->name = $property;"` |
|     - | 2540 | `"   $this->__dynobj = $obj;"` |
|     - | 2541 | `"   return;"` |
|     - | 2542 | `"  }"` |
|     - | 2543 | `"  throw new ReflectionException('Property '.$this->class.'::$'.$property.' does not exist');"` |
|     - | 2544 | `" }"` |
|     - | 2545 | `" protected function __rpmeta(){"` |
|     - | 2546 | `"  $ci = __phl_rcinfo($this->class);"` |
|     - | 2547 | `"  if(isset($ci['props'][$this->name])){ return $ci['props'][$this->name]; }"` |
|     - | 2548 | `"  return array('vis' => 1, 'static' => false, 'readonly' => false, 'hasdef' => false,"` |
|     - | 2549 | `"   'typed' => false, 'typetext' => null, 'decl' => $this->class, 'line' => 0, 'dyn' => true);"` |
|     - | 2550 | `" }"` |
|     - | 2551 | `" public function getName(){ return $this->name; }"` |
|     - | 2552 | `" public function getDeclaringClass(){"` |
|     - | 2553 | `"  $m = $this->__rpmeta();"` |
|     - | 2554 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2555 | `" }"` |
|     - | 2556 | `" public function getModifiers(){"` |
|     - | 2557 | `"  $m = $this->__rpmeta();"` |
|     - | 2558 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2559 | `"  if($m['static']){ $mod \|= 16; }"` |
|     - | 2560 | `"  if($m['readonly']){ $mod \|= 128; }"` |
|     - | 2561 | `"  return $mod;"` |
|     - | 2562 | `" }"` |
|     - | 2563 | `" public function isPublic(){ $m = $this->__rpmeta(); return $m['vis'] === 1; }"` |
|     - | 2564 | `" public function isProtected(){ $m = $this->__rpmeta(); return $m['vis'] === 2; }"` |
|     - | 2565 | `" public function isPrivate(){ $m = $this->__rpmeta(); return $m['vis'] === 3; }"` |
|     - | 2566 | `" public function isStatic(){ $m = $this->__rpmeta(); return $m['static']; }"` |
|     - | 2567 | `" public function isReadOnly(){ $m = $this->__rpmeta(); return $m['readonly']; }"` |
|     - | 2568 | `" public function isPrivateSet(){ $m = $this->__rpmeta(); return isset($m['privset']) ? $m['privset'] : false; }"` |
|     - | 2569 | `" public function isProtectedSet(){ $m = $this->__rpmeta(); return isset($m['protset']) ? $m['protset'] : false; }"` |
|     - | 2570 | `" public function isDefault(){ $m = $this->__rpmeta(); return !isset($m['dyn']); }"` |
|     - | 2571 | `" public function isDynamic(){ $m = $this->__rpmeta(); return isset($m['dyn']); }"` |
|     - | 2572 | `" public function isAbstract(){ return false; }"` |
|     - | 2573 | `" public function isFinal(){ return false; }"` |
|     - | 2574 | `" public function isVirtual(){ $m = $this->__rpmeta(); return isset($m['virtual']) ? $m['virtual'] : false; }"` |
|     - | 2575 | `" public function hasHooks(){ $m = $this->__rpmeta();"` |
|     - | 2576 | `"  return (isset($m['hookget']) && $m['hookget']) \|\| (isset($m['hookset']) && $m['hookset']); }"` |
|     - | 2577 | `" public function getHooks(){"` |
|     - | 2578 | `"  $m = $this->__rpmeta(); $h = array();"` |
|     - | 2579 | `"  if(isset($m['hookget']) && $m['hookget']){ $h['get'] = new ReflectionMethod($m['decl'], '__phl_hook_get_'.$this->name); }"` |
|     - | 2580 | `"  if(isset($m['hookset']) && $m['hookset']){ $h['set'] = new ReflectionMethod($m['decl'], '__phl_hook_set_'.$this->name); }"` |
|     - | 2581 | `"  return $h; }"` |
|     - | 2582 | `" public function hasHook($type){"` |
|     - | 2583 | `"  $t = $type instanceof PropertyHookType ? $type->value : $type;"` |
|     - | 2584 | `"  $m = $this->__rpmeta();"` |
|     - | 2585 | `"  if($t === 'get'){ return isset($m['hookget']) && $m['hookget']; }"` |
|     - | 2586 | `"  if($t === 'set'){ return isset($m['hookset']) && $m['hookset']; }"` |
|     - | 2587 | `"  return false; }"` |
|     - | 2588 | `" public function getHook($type){"` |
|     - | 2589 | `"  $t = $type instanceof PropertyHookType ? $type->value : $type;"` |
|     - | 2590 | `"  $h = $this->getHooks();"` |
|     - | 2591 | `"  return isset($h[$t]) ? $h[$t] : null; }"` |
|     - | 2592 | `" public function isLazy($object){ return false; }"` |
|     - | 2593 | `" public function setAccessible($accessible){ }"` |
|     - | 2594 | `" public function getValue($object = null){"` |
|     - | 2595 | `"  $m = $this->__rpmeta();"` |
|     - | 2596 | `"  if($m['static']){ return __reflect_static_value($this->class, $this->name); }"` |
|     - | 2597 | `"  if(!is_object($object)){"` |
|     - | 2598 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2599 | `"  }"` |
|     - | 2600 | `"  return __reflect_prop_read($object, $this->name);"` |
|     - | 2601 | `" }"` |
|     - | 2602 | `" public function setValue($objectOrValue = null, $value = null){"` |
|     - | 2603 | `"  $m = $this->__rpmeta();"` |
|     - | 2604 | `"  if($m['static']){"` |
|     - | 2605 | `"   if($value === null && $objectOrValue !== null && !is_object($objectOrValue)){"` |
|     - | 2606 | `"    __reflect_static_set($this->class, $this->name, $objectOrValue);"` |
|     - | 2607 | `"   }else{"` |
|     - | 2608 | `"    __reflect_static_set($this->class, $this->name, $value);"` |
|     - | 2609 | `"   }"` |
|     - | 2610 | `"   return;"` |
|     - | 2611 | `"  }"` |
|     - | 2612 | `"  __reflect_prop_write($objectOrValue, $this->name, $value);"` |
|     - | 2613 | `" }"` |
|     - | 2614 | `" public function getRawValue($object){ return $this->getValue($object); }"` |
|     - | 2615 | `" public function setRawValue($object, $value){ $this->setValue($object, $value); }"` |
|     - | 2616 | `" public function isInitialized($object = null){"` |
|     - | 2617 | `"  $m = $this->__rpmeta();"` |
|     - | 2618 | `"  if($m['static']){ return (__reflect_prop_state($this->class, $this->name) & 2) !== 0; }"` |
|     - | 2619 | `"  if(!is_object($object)){"` |
|     - | 2620 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2621 | `"  }"` |
|     - | 2622 | `"  return (__reflect_prop_state($object, $this->name) & 2) !== 0;"` |
|     - | 2623 | `" }"` |
|     - | 2624 | `" public function hasDefaultValue(){"` |
|     - | 2625 | `"  $m = $this->__rpmeta();"` |
|     - | 2626 | `"  if(isset($m['dyn'])){ return false; }"` |
|     - | 2627 | `"  if($m['hasdef']){ return true; }"` |
|     - | 2628 | `"  return !$m['typed'];"` |
|     - | 2629 | `" }"` |
|     - | 2630 | `" public function getDefaultValue(){"` |
|     - | 2631 | `"  $m = $this->__rpmeta();"` |
|     - | 2632 | `"  if(isset($m['dyn']) \|\| !$m['hasdef']){ return null; }"` |
|     - | 2633 | `"  return __reflect_prop_default($this->class, $this->name);"` |
|     - | 2634 | `" }"` |
|     - | 2635 | `" public function hasType(){ $m = $this->__rpmeta(); return $m['typed']; }"` |
|     - | 2636 | `" public function getType(){ $m = $this->__rpmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2637 | `" public function getSettableType(){ return $this->getType(); }"` |
|     - | 2638 | `" public function setRawValueWithoutLazyInitialization($object, $value){"` |
|     - | 2639 | `"  throw new Error('ReflectionProperty::setRawValueWithoutLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2640 | `" }"` |
|     - | 2641 | `" public function skipLazyInitialization($object){"` |
|     - | 2642 | `"  throw new Error('ReflectionProperty::skipLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2643 | `" }"` |
|     - | 2644 | `" public function getDocComment(){ $m = $this->__rpmeta(); return isset($m['doc']) ? $m['doc'] : false; }"` |
|     - | 2645 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2646 | `"  $m = $this->__rpmeta();"` |
|     - | 2647 | `"  if(!isset($m['attrs'])){ return array(); }"` |
|     - | 2648 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 8, $name, $flags);"` |
|     - | 2649 | `" }"` |
|     - | 2650 | `" public function __toString(){ return __reflect_export_prop($this); }"` |
|     - | 2651 | `"}"` |
|     - | 2652 | `"class ReflectionClassConstant implements Reflector {"` |
|     - | 2653 | `" const IS_PUBLIC = 1;"` |
|     - | 2654 | `" const IS_PROTECTED = 2;"` |
|     - | 2655 | `" const IS_PRIVATE = 4;"` |
|     - | 2656 | `" const IS_FINAL = 32;"` |
|     - | 2657 | `" public $name;"` |
|     - | 2658 | `" public $class;"` |
|     - | 2659 | `" public function __construct($class, $constant){"` |
|     - | 2660 | `"  if(!is_object($class) && !is_string($class)){"` |
|     - | 2661 | `"   throw new TypeError('ReflectionClassConstant::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2662 | `"  }"` |
|     - | 2663 | `"  $ci = __phl_rcinfo($class);"` |
|     - | 2664 | `"  if($ci === null){"` |
|     - | 2665 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2666 | `"  }"` |
|     - | 2667 | `"  $this->class = $ci['name'];"` |
|     - | 2668 | `"  if(!isset($ci['consts'][$constant])){"` |
|     - | 2669 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' does not exist');"` |
|     - | 2670 | `"  }"` |
|     - | 2671 | `"  $this->name = $constant;"` |
|     - | 2672 | `" }"` |
|     - | 2673 | `" protected function __rcmeta(){"` |
|     - | 2674 | `"  $ci = __phl_rcinfo($this->class);"` |
|     - | 2675 | `"  return $ci['consts'][$this->name];"` |
|     - | 2676 | `" }"` |
|     - | 2677 | `" public function getName(){ return $this->name; }"` |
|     - | 2678 | `" public function getValue(){ return __reflect_const_value($this->class, $this->name); }"` |
|     - | 2679 | `" public function getDeclaringClass(){"` |
|     - | 2680 | `"  $m = $this->__rcmeta();"` |
|     - | 2681 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2682 | `" }"` |
|     - | 2683 | `" public function getModifiers(){"` |
|     - | 2684 | `"  $m = $this->__rcmeta();"` |
|     - | 2685 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2686 | `"  if($m['final']){ $mod \|= 32; }"` |
|     - | 2687 | `"  return $mod;"` |
|     - | 2688 | `" }"` |
|     - | 2689 | `" public function isPublic(){ $m = $this->__rcmeta(); return $m['vis'] === 1; }"` |
|     - | 2690 | `" public function isProtected(){ $m = $this->__rcmeta(); return $m['vis'] === 2; }"` |
|     - | 2691 | `" public function isPrivate(){ $m = $this->__rcmeta(); return $m['vis'] === 3; }"` |
|     - | 2692 | `" public function isFinal(){ $m = $this->__rcmeta(); return $m['final']; }"` |
|     - | 2693 | `" public function isEnumCase(){ $m = $this->__rcmeta(); return $m['enumcase']; }"` |
|     - | 2694 | `" public function isDeprecated(){ $m = $this->__rcmeta(); return __reflect_has_deprecated($m['attrs']); }"` |
|     - | 2695 | `" public function hasType(){ $m = $this->__rcmeta(); return $m['typed']; }"` |
|     - | 2696 | `" public function getType(){ $m = $this->__rcmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2697 | `" public function getDocComment(){ $m = $this->__rcmeta(); return $m['doc']; }"` |
|     - | 2698 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2699 | `"  $m = $this->__rcmeta();"` |
|     - | 2700 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 16, $name, $flags);"` |
|     - | 2701 | `" }"` |
|     - | 2702 | `" public function __toString(){ return __reflect_export_cconst($this); }"` |
|     - | 2703 | `"}"` |
|     - | 2704 | `;` |
|     - | 2705 | `/*` |
|     - | 2706 | ` * Chunk 4: the ReflectionType family, built from the engine's canonical` |
|     - | 2707 | ` * type text ("?int", "string\|float", "(A&B)\|C" — normalized at compile` |
|     - | 2708 | ` * time). __reflect_make_type is the internal factory; PHP itself never` |
|     - | 2709 | ` * lets user code construct these, so the public constructors here are a` |
|     - | 2710 | ` * recorded PHL-only surface.` |
|     - | 2711 | ` */` |
|     - | 2712 | `static const char zReflectLib4[] =` |
|     - | 2713 | `"abstract class ReflectionType implements Stringable {"` |
|     - | 2714 | `" protected $__text = '';"` |
|     - | 2715 | `" protected $__nullable = false;"` |
|     - | 2716 | `" public function allowsNull(){ return $this->__nullable; }"` |
|     - | 2717 | `" public function __toString(){ return $this->__text; }"` |
|     - | 2718 | `"}"` |
|     - | 2719 | `"class ReflectionNamedType extends ReflectionType {"` |
|     - | 2720 | `" protected $__tname = '';"` |
|     - | 2721 | `" public function __construct($name = '', $nullable = false, $text = null){"` |
|     - | 2722 | `"  $this->__tname = $name;"` |
|     - | 2723 | `"  $l = strtolower($name);"` |
|     - | 2724 | `"  $this->__nullable = $nullable \|\| $l === 'null' \|\| $l === 'mixed';"` |
|     - | 2725 | `"  $this->__text = $text === null ? $name : $text;"` |
|     - | 2726 | `" }"` |
|     - | 2727 | `" public function getName(){ return $this->__tname; }"` |
|     - | 2728 | `" public function isBuiltin(){"` |
|     - | 2729 | `"  $l = strtolower($this->__tname);"` |
|     - | 2730 | `"  return in_array($l, array('int','float','string','bool','array','object','mixed',"` |
|     - | 2731 | `"   'void','never','null','callable','iterable','true','false'), true);"` |
|     - | 2732 | `" }"` |
|     - | 2733 | `"}"` |
|     - | 2734 | `"class ReflectionUnionType extends ReflectionType {"` |
|     - | 2735 | `" protected $__types = array();"` |
|     - | 2736 | `" public function __construct($text = '', $nullable = false, $types = array()){"` |
|     - | 2737 | `"  $this->__text = $text;"` |
|     - | 2738 | `"  $this->__nullable = $nullable;"` |
|     - | 2739 | `"  $this->__types = $types;"` |
|     - | 2740 | `" }"` |
|     - | 2741 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2742 | `"}"` |
|     - | 2743 | `"class ReflectionIntersectionType extends ReflectionType {"` |
|     - | 2744 | `" protected $__types = array();"` |
|     - | 2745 | `" public function __construct($text = '', $types = array()){"` |
|     - | 2746 | `"  $this->__text = $text;"` |
|     - | 2747 | `"  $this->__nullable = false;"` |
|     - | 2748 | `"  $this->__types = $types;"` |
|     - | 2749 | `" }"` |
|     - | 2750 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2751 | `"}"` |
|     - | 2752 | `"function __reflect_make_atom($p){"` |
|     - | 2753 | `" $nullable = false;"` |
|     - | 2754 | `" if($p !== '' && $p[0] === '?'){ $nullable = true; $p = substr($p, 1); }"` |
|     - | 2755 | `" if($p !== '' && $p[0] === '('){ $p = substr($p, 1, strlen($p) - 2); }"` |
|     - | 2756 | `" if(strpos($p, '&') !== false){"` |
|     - | 2757 | `"  $subs = array();"` |
|     - | 2758 | `"  foreach(explode('&', $p) as $s){ $subs[] = new ReflectionNamedType($s, false, $s); }"` |
|     - | 2759 | `"  return new ReflectionIntersectionType($p, $subs);"` |
|     - | 2760 | `" }"` |
|     - | 2761 | `" return new ReflectionNamedType($p, $nullable, $nullable ? '?'.$p : $p);"` |
|     - | 2762 | `"}"` |
|     - | 2763 | `"function __reflect_make_type($text){"` |
|     - | 2764 | `" if($text === null \|\| $text === ''){ return null; }"` |
|     - | 2765 | `" $nullable = false;"` |
|     - | 2766 | `" $body = $text;"` |
|     - | 2767 | `" if($body[0] === '?'){ $nullable = true; $body = substr($body, 1); }"` |
|     - | 2768 | `" $parts = array();"` |
|     - | 2769 | `" $depth = 0;"` |
|     - | 2770 | `" $cur = '';"` |
|     - | 2771 | `" $n = strlen($body);"` |
|     - | 2772 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 2773 | `"  $ch = $body[$k];"` |
|     - | 2774 | `"  if($ch === '('){ $depth++; $cur .= $ch; }"` |
|     - | 2775 | `"  else if($ch === ')'){ $depth--; $cur .= $ch; }"` |
|     - | 2776 | `"  else if($ch === '\|' && $depth === 0){ $parts[] = $cur; $cur = ''; }"` |
|     - | 2777 | `"  else{ $cur .= $ch; }"` |
|     - | 2778 | `" }"` |
|     - | 2779 | `" $parts[] = $cur;"` |
|     - | 2780 | `" if(count($parts) > 1){"` |
|     - | 2781 | `"  $nonNull = array();"` |
|     - | 2782 | `"  $hasNull = false;"` |
|     - | 2783 | `"  foreach($parts as $p){"` |
|     - | 2784 | `"   if(strtolower($p) === 'null'){ $hasNull = true; }"` |
|     - | 2785 | `"   else{ $nonNull[] = $p; }"` |
|     - | 2786 | `"  }"` |
|     - | 2787 | `"  if($hasNull && count($nonNull) === 1 && strpos($nonNull[0], '&') === false){"` |
|     - | 2788 | `"   return new ReflectionNamedType($nonNull[0], true, '?'.$nonNull[0]);"` |
|     - | 2789 | `"  }"` |
|     - | 2790 | `"  $types = array();"` |
|     - | 2791 | `"  foreach($parts as $p){ $types[] = __reflect_make_atom($p); }"` |
|     - | 2792 | `"  return new ReflectionUnionType($body, $nullable \|\| $hasNull, $types);"` |
|     - | 2793 | `" }"` |
|     - | 2794 | `" if(strpos($body, '&') !== false){ return __reflect_make_atom($body); }"` |
|     - | 2795 | `" return __reflect_make_atom($nullable ? '?'.$body : $body);"` |
|     - | 2796 | `"}"` |
|     - | 2797 | `;` |
|     - | 2798 | `/*` |
|     - | 2799 | ` * Chunk 5: ReflectionGenerator, ReflectionFiber. Executing line/file and` |
|     - | 2800 | ` * traces need runtime line tracking the VM does not have (same gap as` |
|     - | 2801 | ` * debug_backtrace's line numbers) — those throw a loud Error, recorded in` |
|     - | 2802 | ` * the plan ledger.` |
|     - | 2803 | ` */` |
|     - | 2804 | `static const char zReflectLib5[] =` |
|     - | 2805 | `"class ReflectionGenerator {"` |
|     - | 2806 | `" protected $__gen;"` |
|     - | 2807 | `" public function __construct($generator){"` |
|     - | 2808 | `"  if(!($generator instanceof Generator)){"` |
|     - | 2809 | `"   throw new TypeError('ReflectionGenerator::__construct(): Argument #1 ($generator) must be of type Generator, '.get_debug_type($generator).' given');"` |
|     - | 2810 | `"  }"` |
|     - | 2811 | `"  $this->__gen = $generator;"` |
|     - | 2812 | `" }"` |
|     - | 2813 | `" protected function __rginfo(){ return __reflect_gen_info($this->__gen); }"` |
|     - | 2814 | `" public function getFunction(){"` |
|     - | 2815 | `"  $i = $this->__rginfo();"` |
|     - | 2816 | `"  if($i['kind'] === 'method'){ return new ReflectionMethod($i['class'], $i['name']); }"` |
|     - | 2817 | `"  return new ReflectionFunction($i['name']);"` |
|     - | 2818 | `" }"` |
|     - | 2819 | `" public function getThis(){ $i = $this->__rginfo(); return isset($i['this']) ? $i['this'] : null; }"` |
|     - | 2820 | `" public function getExecutingGenerator(){ return __reflect_gen_exec($this->__gen); }"` |
|     - | 2821 | `" public function isClosed(){ $i = $this->__rginfo(); return $i['closed']; }"` |
|     - | 2822 | `" public function getExecutingLine(){"` |
|     - | 2823 | `"  throw new Error('ReflectionGenerator::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2824 | `" }"` |
|     - | 2825 | `" public function getExecutingFile(){"` |
|     - | 2826 | `"  throw new Error('ReflectionGenerator::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2827 | `" }"` |
|     - | 2828 | `" public function getTrace($options = 1){"` |
|     - | 2829 | `"  throw new Error('ReflectionGenerator::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2830 | `" }"` |
|     - | 2831 | `"}"` |
|     - | 2832 | `"class ReflectionFiber {"` |
|     - | 2833 | `" protected $__fiber;"` |
|     - | 2834 | `" public function __construct($fiber){"` |
|     - | 2835 | `"  if(!($fiber instanceof Fiber)){"` |
|     - | 2836 | `"   throw new TypeError('ReflectionFiber::__construct(): Argument #1 ($fiber) must be of type Fiber, '.get_debug_type($fiber).' given');"` |
|     - | 2837 | `"  }"` |
|     - | 2838 | `"  $this->__fiber = $fiber;"` |
|     - | 2839 | `" }"` |
|     - | 2840 | `" public function getFiber(){ return $this->__fiber; }"` |
|     - | 2841 | `" public function getCallable(){ return __reflect_prop_read($this->__fiber, '__callable'); }"` |
|     - | 2842 | `" public function getExecutingLine(){"` |
|     - | 2843 | `"  throw new Error('ReflectionFiber::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2844 | `" }"` |
|     - | 2845 | `" public function getExecutingFile(){"` |
|     - | 2846 | `"  throw new Error('ReflectionFiber::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2847 | `" }"` |
|     - | 2848 | `" public function getTrace($options = 1){"` |
|     - | 2849 | `"  throw new Error('ReflectionFiber::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2850 | `" }"` |
|     - | 2851 | `"}"` |
|     - | 2852 | `;` |
|     - | 2853 | `/*` |
|     - | 2854 | ` * Chunk 6: the long tail — ReflectionConstant (PHP 8.5), the synthetic` |
|     - | 2855 | ` * "Core" ReflectionExtension, ReflectionZendExtension (throws: no Zend` |
|     - | 2856 | ` * extensions exist), the ReflectionEnum family (throws: enums are not a` |
|     - | 2857 | ` * PHL language feature yet), and ReflectionReference.` |
|     - | 2858 | ` */` |
|     - | 2859 | `static const char zReflectLib6[] =` |
|     - | 2860 | `"class ReflectionConstant implements Reflector {"` |
|     - | 2861 | `" public $name;"` |
|     - | 2862 | `" public function __construct($name){"` |
|     - | 2863 | `"  if(!is_string($name)){"` |
|     - | 2864 | `"   throw new TypeError('ReflectionConstant::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2865 | `"  }"` |
|     - | 2866 | `"  $i = __reflect_const_info($name);"` |
|     - | 2867 | `"  if($i === null){"` |
|     - | 2868 | `"   throw new ReflectionException('Constant \"'.$name.'\" does not exist');"` |
|     - | 2869 | `"  }"` |
|     - | 2870 | `"  $this->name = $name;"` |
|     - | 2871 | `" }"` |
|     - | 2872 | `" public function getName(){ return $this->name; }"` |
|     - | 2873 | `" public function getNamespaceName(){"` |
|     - | 2874 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2875 | `"  if($p === false){ return ''; }"` |
|     - | 2876 | `"  return substr($this->name,0,$p);"` |
|     - | 2877 | `" }"` |
|     - | 2878 | `" public function getShortName(){"` |
|     - | 2879 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2880 | `"  if($p === false){ return $this->name; }"` |
|     - | 2881 | `"  return substr($this->name,$p+1);"` |
|     - | 2882 | `" }"` |
|     - | 2883 | `" public function getValue(){"` |
|     - | 2884 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2885 | `"  return $i['value'];"` |
|     - | 2886 | `" }"` |
|     - | 2887 | `" public function isDeprecated(){ return false; }"` |
|     - | 2888 | `" public function getFileName(){"` |
|     - | 2889 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2890 | `"  return $i['file'];"` |
|     - | 2891 | `" }"` |
|     - | 2892 | `" public function getExtension(){"` |
|     - | 2893 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2894 | `"  return $i['internal'] ? new ReflectionExtension('Core') : null;"` |
|     - | 2895 | `" }"` |
|     - | 2896 | `" public function getExtensionName(){"` |
|     - | 2897 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2898 | `"  return $i['internal'] ? 'Core' : false;"` |
|     - | 2899 | `" }"` |
|     - | 2900 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2901 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2902 | `"  if($i === null){ return array(); }"` |
|     - | 2903 | `"  return __reflect_build_attrs($i['attrs'], array('const', $this->name, null, 0), 64, $name, $flags);"` |
|     - | 2904 | `" }"` |
|     - | 2905 | `" public function __toString(){"` |
|     - | 2906 | `"  return 'Constant [ '.$this->name.' ]'.\"\\n\";"` |
|     - | 2907 | `" }"` |
|     - | 2908 | `"}"` |
|     - | 2909 | `"class ReflectionExtension implements Reflector {"` |
|     - | 2910 | `" public $name;"` |
|     - | 2911 | `" public function __construct($name){"` |
|     - | 2912 | `"  if(!is_string($name)){"` |
|     - | 2913 | `"   throw new TypeError('ReflectionExtension::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2914 | `"  }"` |
|     - | 2915 | `"  if(strtolower($name) !== 'core'){"` |
|     - | 2916 | `"   throw new ReflectionException('Extension \"'.$name.'\" does not exist');"` |
|     - | 2917 | `"  }"` |
|     - | 2918 | `"  $this->name = 'Core';"` |
|     - | 2919 | `" }"` |
|     - | 2920 | `" public function getName(){ return $this->name; }"` |
|     - | 2921 | `" public function getVersion(){ return phpversion(); }"` |
|     - | 2922 | `" public function getFunctions(){ return array(); }"` |
|     - | 2923 | `" public function getClasses(){ return array(); }"` |
|     - | 2924 | `" public function getClassNames(){ return array(); }"` |
|     - | 2925 | `" public function getConstants(){ return array(); }"` |
|     - | 2926 | `" public function getINIEntries(){ return array(); }"` |
|     - | 2927 | `" public function getDependencies(){ return array(); }"` |
|     - | 2928 | `" public function isPersistent(){ return true; }"` |
|     - | 2929 | `" public function isTemporary(){ return false; }"` |
|     - | 2930 | `" public function info(){ }"` |
|     - | 2931 | `" public function __toString(){"` |
|     - | 2932 | `"  return 'Extension [ extension #1 '.$this->name.' ]'.\"\\n\";"` |
|     - | 2933 | `" }"` |
|     - | 2934 | `"}"` |
|     - | 2935 | `"class ReflectionZendExtension implements Reflector {"` |
|     - | 2936 | `" public $name;"` |
|     - | 2937 | `" public function __construct($name){"` |
|     - | 2938 | `"  throw new ReflectionException('Zend Extension \"'.$name.'\" does not exist');"` |
|     - | 2939 | `" }"` |
|     - | 2940 | `" public function getName(){ return $this->name; }"` |
|     - | 2941 | `" public function __toString(){ return ''; }"` |
|     - | 2942 | `"}"` |
|     - | 2943 | `"class ReflectionEnum extends ReflectionClass {"` |
|     - | 2944 | `" public function __construct($objectOrClass){"` |
|     - | 2945 | `"  $info = __phl_rcinfo($objectOrClass);"` |
|     - | 2946 | `"  if($info === null){"` |
|     - | 2947 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 2948 | `"  }"` |
|     - | 2949 | `"  if(!$info['enum']){"` |
|     - | 2950 | `"   throw new ReflectionException('Class \"'.$info['name'].'\" is not an enum');"` |
|     - | 2951 | `"  }"` |
|     - | 2952 | `"  parent::__construct($objectOrClass);"` |
|     - | 2953 | `" }"` |
|     - | 2954 | `" public function hasCase($name){"` |
|     - | 2955 | `"  $i = $this->__rinfo();"` |
|     - | 2956 | `"  return in_array($name, $i['cases'], true);"` |
|     - | 2957 | `" }"` |
|     - | 2958 | `" public function getCase($name){"` |
|     - | 2959 | `"  if(!$this->hasCase($name)){"` |
|     - | 2960 | `"   throw new ReflectionException('Case '.$this->name.'::'.$name.' does not exist');"` |
|     - | 2961 | `"  }"` |
|     - | 2962 | `"  if($this->isBacked()){ return new ReflectionEnumBackedCase($this->name, $name); }"` |
|     - | 2963 | `"  return new ReflectionEnumUnitCase($this->name, $name);"` |
|     - | 2964 | `" }"` |
|     - | 2965 | `" public function getCases(){"` |
|     - | 2966 | `"  $i = $this->__rinfo();"` |
|     - | 2967 | `"  $out = array();"` |
|     - | 2968 | `"  foreach($i['cases'] as $c){"` |
|     - | 2969 | `"   $out[] = $this->isBacked()"` |
|     - | 2970 | `"    ? new ReflectionEnumBackedCase($this->name, $c)"` |
|     - | 2971 | `"    : new ReflectionEnumUnitCase($this->name, $c);"` |
|     - | 2972 | `"  }"` |
|     - | 2973 | `"  return $out;"` |
|     - | 2974 | `" }"` |
|     - | 2975 | `" public function isBacked(){ $i = $this->__rinfo(); return $i['enumbacking'] !== ''; }"` |
|     - | 2976 | `" public function getBackingType(){"` |
|     - | 2977 | `"  $i = $this->__rinfo();"` |
|     - | 2978 | `"  if($i['enumbacking'] === ''){ return null; }"` |
|     - | 2979 | `"  return __reflect_make_type($i['enumbacking']);"` |
|     - | 2980 | `" }"` |
|     - | 2981 | `"}"` |
|     - | 2982 | `"class ReflectionEnumUnitCase extends ReflectionClassConstant {"` |
|     - | 2983 | `" public function __construct($class, $constant){"` |
|     - | 2984 | `"  parent::__construct($class, $constant);"` |
|     - | 2985 | `"  $ci = __phl_rcinfo($class);"` |
|     - | 2986 | `"  if(!$ci['enum']){"` |
|     - | 2987 | `"   throw new ReflectionException('Class \"'.$this->class.'\" is not an enum');"` |
|     - | 2988 | `"  }"` |
|     - | 2989 | `"  $m = $this->__rcmeta();"` |
|     - | 2990 | `"  if(!$m['enumcase']){"` |
|     - | 2991 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' is not a case');"` |
|     - | 2992 | `"  }"` |
|     - | 2993 | `" }"` |
|     - | 2994 | `" public function getEnum(){ return new ReflectionEnum($this->class); }"` |
|     - | 2995 | `"}"` |
|     - | 2996 | `"class ReflectionEnumBackedCase extends ReflectionEnumUnitCase {"` |
|     - | 2997 | `" public function getBackingValue(){ return $this->getValue()->value; }"` |
|     - | 2998 | `"}"` |
|     - | 2999 | `"final class ReflectionReference {"` |
|     - | 3000 | `" protected $__id = '';"` |
|     - | 3001 | `" public function __construct(){"` |
|     - | 3002 | `"  throw new Error('Call to private ReflectionReference::__construct() from global scope');"` |
|     - | 3003 | `" }"` |
|     - | 3004 | `" public static function fromArrayElement($array, $key){"` |
|     - | 3005 | `"  if(!is_array($array)){"` |
|     - | 3006 | `"   throw new TypeError('ReflectionReference::fromArrayElement(): Argument #1 ($array) must be of type array, '.get_debug_type($array).' given');"` |
|     - | 3007 | `"  }"` |
|     - | 3008 | `"  $id = __reflect_ref_id($array, $key);"` |
|     - | 3009 | `"  if($id === null){ return null; }"` |
|     - | 3010 | `"  $r = __reflect_new_no_ctor('ReflectionReference');"` |
|     - | 3011 | `"  $r->__setId('phlref'.$id);"` |
|     - | 3012 | `"  return $r;"` |
|     - | 3013 | `" }"` |
|     - | 3014 | `" public function __setId($id){ $this->__id = $id; }"` |
|     - | 3015 | `" public function getId(){ return $this->__id; }"` |
|     - | 3016 | `"}"` |
|     - | 3017 | `;` |
|     - | 3018 | `/*` |
|     - | 3019 | ` * Chunk 7: ReflectionAttribute and the shared getAttributes() builder.` |
|     - | 3020 | ` * The spec array rides as [kind, target, member, paramIdx]; argument` |
|     - | 3021 | ` * values evaluate lazily through __reflect_attr_args (PHP semantics).` |
|     - | 3022 | ` */` |
|     - | 3023 | `static const char zReflectLib7[] =` |
|     - | 3024 | `"function __reflect_has_deprecated($meta){"` |
|     - | 3025 | `" foreach($meta as $a){"` |
|     - | 3026 | `"  if(strtolower($a['name']) === 'deprecated'){ return true; }"` |
|     - | 3027 | `" }"` |
|     - | 3028 | `" return false;"` |
|     - | 3029 | `"}"` |
|     - | 3030 | `"function __reflect_target_names($mask){"` |
|     - | 3031 | `" $parts = array();"` |
|     - | 3032 | `" foreach(array('class' => 1, 'function' => 2, 'method' => 4, 'property' => 8,"` |
|     - | 3033 | `"  'class constant' => 16, 'parameter' => 32, 'constant' => 64) as $nm => $bit){"` |
|     - | 3034 | `"  if($mask & $bit){ $parts[] = $nm; }"` |
|     - | 3035 | `" }"` |
|     - | 3036 | `" return implode(', ', $parts);"` |
|     - | 3037 | `"}"` |
|     - | 3038 | `"function __reflect_build_attrs($meta, $spec, $target, $name, $flags){"` |
|     - | 3039 | `" $out = array();"` |
|     - | 3040 | `" $counts = array();"` |
|     - | 3041 | `" foreach($meta as $a){"` |
|     - | 3042 | `"  $k = strtolower($a['name']);"` |
|     - | 3043 | `"  $counts[$k] = isset($counts[$k]) ? $counts[$k] + 1 : 1;"` |
|     - | 3044 | `" }"` |
|     - | 3045 | `" $idx = 0;"` |
|     - | 3046 | `" foreach($meta as $a){"` |
|     - | 3047 | `"  $keep = true;"` |
|     - | 3048 | `"  if($name !== null){"` |
|     - | 3049 | `"   $keep = strtolower($a['name']) === strtolower($name);"` |
|     - | 3050 | `"   if(!$keep && ($flags & 2)){"` |
|     - | 3051 | `"    $keep = is_subclass_of($a['name'], $name);"` |
|     - | 3052 | `"   }"` |
|     - | 3053 | `"  }"` |
|     - | 3054 | `"  if($keep){"` |
|     - | 3055 | `"   $r = __reflect_new_no_ctor('ReflectionAttribute');"` |
|     - | 3056 | `"   $r->__init($a['name'], $spec, $idx, $target, $counts[strtolower($a['name'])] > 1);"` |
|     - | 3057 | `"   $out[] = $r;"` |
|     - | 3058 | `"  }"` |
|     - | 3059 | `"  $idx++;"` |
|     - | 3060 | `" }"` |
|     - | 3061 | `" return $out;"` |
|     - | 3062 | `"}"` |
|     - | 3063 | `"final class ReflectionAttribute {"` |
|     - | 3064 | `" const IS_INSTANCEOF = 2;"` |
|     - | 3065 | `" protected $__name = '';"` |
|     - | 3066 | `" protected $__spec = null;"` |
|     - | 3067 | `" protected $__idx = 0;"` |
|     - | 3068 | `" protected $__target = 0;"` |
|     - | 3069 | `" protected $__rep = false;"` |
|     - | 3070 | `" public function __construct(){"` |
|     - | 3071 | `"  throw new Error('Call to private ReflectionAttribute::__construct() from global scope');"` |
|     - | 3072 | `" }"` |
|     - | 3073 | `" public function __init($name, $spec, $idx, $target, $rep){"` |
|     - | 3074 | `"  $this->__name = $name;"` |
|     - | 3075 | `"  $this->__spec = $spec;"` |
|     - | 3076 | `"  $this->__idx = $idx;"` |
|     - | 3077 | `"  $this->__target = $target;"` |
|     - | 3078 | `"  $this->__rep = $rep;"` |
|     - | 3079 | `" }"` |
|     - | 3080 | `" public function getName(){ return $this->__name; }"` |
|     - | 3081 | `" public function getTarget(){ return $this->__target; }"` |
|     - | 3082 | `" public function isRepeated(){ return $this->__rep; }"` |
|     - | 3083 | `" public function getArguments(){"` |
|     - | 3084 | `"  $a = __reflect_attr_args($this->__spec[0], $this->__spec[1], $this->__spec[2], $this->__spec[3], $this->__idx);"` |
|     - | 3085 | `"  return $a === null ? array() : $a;"` |
|     - | 3086 | `" }"` |
|     - | 3087 | `" public function newInstance(){"` |
|     - | 3088 | `"  $name = $this->__name;"` |
|     - | 3089 | `"  $ci = __phl_rcinfo($name);"` |
|     - | 3090 | `"  if($ci === null){"` |
|     - | 3091 | `"   throw new Error('Attribute class \"'.$name.'\" not found');"` |
|     - | 3092 | `"  }"` |
|     - | 3093 | `"  $name = $ci['name'];"` |
|     - | 3094 | `"  $decl = null;"` |
|     - | 3095 | `"  $didx = 0;"` |
|     - | 3096 | `"  foreach($ci['attrs'] as $a){"` |
|     - | 3097 | `"   if(strtolower($a['name']) === 'attribute'){ $decl = $didx; break; }"` |
|     - | 3098 | `"   $didx++;"` |
|     - | 3099 | `"  }"` |
|     - | 3100 | `"  if($decl === null){"` |
|     - | 3101 | `"   throw new Error('Attempting to use non-attribute class \"'.$name.'\" as attribute');"` |
|     - | 3102 | `"  }"` |
|     - | 3103 | `"  $dargs = __reflect_attr_args('class', $name, null, 0, $decl);"` |
|     - | 3104 | `"  $flags = 127;"` |
|     - | 3105 | `"  if(is_array($dargs)){"` |
|     - | 3106 | `"   if(isset($dargs[0])){ $flags = $dargs[0]; }"` |
|     - | 3107 | `"   else if(isset($dargs['flags'])){ $flags = $dargs['flags']; }"` |
|     - | 3108 | `"  }"` |
|     - | 3109 | `"  if(($flags & $this->__target) === 0){"` |
|     - | 3110 | `"   $tnames = array(1 => 'class', 2 => 'function', 4 => 'method', 8 => 'property',"` |
|     - | 3111 | `"    16 => 'class constant', 32 => 'parameter', 64 => 'constant');"` |
|     - | 3112 | `"   throw new Error('Attribute \"'.$name.'\" cannot target '.$tnames[$this->__target]"` |
|     - | 3113 | `"    .' (allowed targets: '.__reflect_target_names($flags).')');"` |
|     - | 3114 | `"  }"` |
|     - | 3115 | `"  if($this->__rep && ($flags & 128) === 0){"` |
|     - | 3116 | `"   throw new Error('Attribute \"'.$name.'\" must not be repeated');"` |
|     - | 3117 | `"  }"` |
|     - | 3118 | `"  return __reflect_new_instance($name, $this->getArguments());"` |
|     - | 3119 | `" }"` |
|     - | 3120 | `" public function __toString(){"` |
|     - | 3121 | `"  return 'Attribute [ '.$this->__name.' ]';"` |
|     - | 3122 | `" }"` |
|     - | 3123 | `"}"` |
|     - | 3124 | `;` |
|     - | 3125 | `/*` |
|     - | 3126 | ` * Chunk 8: signature-table support. Internal (C builtin) functions carry a` |
|     - | 3127 | ` * PHP-style parameter-list string; these helpers parse it into the same` |
|     - | 3128 | ` * param-meta shape user functions get, so ReflectionFunction and` |
|     - | 3129 | ` * ReflectionParameter work uniformly over builtins.` |
|     - | 3130 | ` */` |
|     - | 3131 | `static const char zReflectLib8[] =` |
|     - | 3132 | `"function __reflect_sig_split($sig){"` |
|     - | 3133 | `" $parts = array();"` |
|     - | 3134 | `" $cur = '';"` |
|     - | 3135 | `" $q = false;"` |
|     - | 3136 | `" $n = strlen($sig);"` |
|     - | 3137 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3138 | `"  $ch = $sig[$k];"` |
|     - | 3139 | `"  if($q){"` |
|     - | 3140 | `"   $cur .= $ch;"` |
|     - | 3141 | `"   if($ch === chr(92) && $k + 1 < $n){ $cur .= $sig[$k+1]; $k++; }"` |
|     - | 3142 | `"   else if($ch === chr(39)){ $q = false; }"` |
|     - | 3143 | `"  }else if($ch === chr(39)){ $q = true; $cur .= $ch; }"` |
|     - | 3144 | `"  else if($ch === ',' ){ $parts[] = trim($cur); $cur = ''; }"` |
|     - | 3145 | `"  else{ $cur .= $ch; }"` |
|     - | 3146 | `" }"` |
|     - | 3147 | `" if(trim($cur) !== ''){ $parts[] = trim($cur); }"` |
|     - | 3148 | `" return $parts;"` |
|     - | 3149 | `"}"` |
|     - | 3150 | `"function __reflect_sig_scalar($t){"` |
|     - | 3151 | `" if($t === '?'){ return array(false, null); }"` |
|     - | 3152 | `" if($t === 'NULL' \|\| $t === 'null'){ return array(true, null); }"` |
|     - | 3153 | `" if($t === 'true'){ return array(true, true); }"` |
|     - | 3154 | `" if($t === 'false'){ return array(true, false); }"` |
|     - | 3155 | `" if(is_numeric($t)){"` |
|     - | 3156 | `"  if(strpos($t, '.') === false && stripos($t, 'e') === false && strpos($t, 'x') === false){"` |
|     - | 3157 | `"   return array(true, (int)$t);"` |
|     - | 3158 | `"  }"` |
|     - | 3159 | `"  return array(true, (float)$t);"` |
|     - | 3160 | `" }"` |
|     - | 3161 | `" if(strlen($t) >= 2 && $t[0] === chr(39) && $t[strlen($t)-1] === chr(39)){"` |
|     - | 3162 | `"  $body = substr($t, 1, strlen($t) - 2);"` |
|     - | 3163 | `"  return array(true, strtr($body, array(chr(92).chr(39) => chr(39), chr(92).chr(92) => chr(92))));"` |
|     - | 3164 | `" }"` |
|     - | 3165 | `" return array(false, null);"` |
|     - | 3166 | `"}"` |
|     - | 3167 | `"function __reflect_parse_sig($sig){"` |
|     - | 3168 | `" $params = array();"` |
|     - | 3169 | `" $pos = 0;"` |
|     - | 3170 | `" foreach(__reflect_sig_split($sig) as $part){"` |
|     - | 3171 | `"  $deftext = null;"` |
|     - | 3172 | `"  $q = false;"` |
|     - | 3173 | `"  $n = strlen($part);"` |
|     - | 3174 | `"  for($k = 0; $k < $n; $k++){"` |
|     - | 3175 | `"   $ch = $part[$k];"` |
|     - | 3176 | `"   if($q){"` |
|     - | 3177 | `"    if($ch === chr(92)){ $k++; }"` |
|     - | 3178 | `"    else if($ch === chr(39)){ $q = false; }"` |
|     - | 3179 | `"   }else if($ch === chr(39)){ $q = true; }"` |
|     - | 3180 | `"   else if($ch === '=' ){"` |
|     - | 3181 | `"    $deftext = trim(substr($part, $k + 1));"` |
|     - | 3182 | `"    $part = trim(substr($part, 0, $k));"` |
|     - | 3183 | `"    break;"` |
|     - | 3184 | `"   }"` |
|     - | 3185 | `"  }"` |
|     - | 3186 | `"  $variadic = strpos($part, '...') !== false;"` |
|     - | 3187 | `"  $byref = strpos($part, '&') !== false;"` |
|     - | 3188 | `"  $d = strpos($part, '$');"` |
|     - | 3189 | `"  $name = $d === false ? $part : substr($part, $d + 1);"` |
|     - | 3190 | `"  $typetext = null;"` |
|     - | 3191 | `"  $sp = strpos($part, ' ');"` |
|     - | 3192 | `"  if($sp !== false && $d !== false && $sp < $d){ $typetext = substr($part, 0, $sp); }"` |
|     - | 3193 | `"  $nullable = $typetext !== null && ($typetext[0] === '?' \|\| stripos($typetext, 'null') !== false);"` |
|     - | 3194 | `"  $params[] = array('name' => $name, 'pos' => $pos, 'byref' => $byref,"` |
|     - | 3195 | `"   'variadic' => $variadic, 'hasdef' => $deftext !== null, 'nullable' => $nullable,"` |
|     - | 3196 | `"   'promoted' => false, 'typetext' => $typetext, 'attrs' => array(), 'deftext' => $deftext);"` |
|     - | 3197 | `"  $pos++;"` |
|     - | 3198 | `" }"` |
|     - | 3199 | `" return $params;"` |
|     - | 3200 | `"}"` |
|     - | 3201 | `"function __reflect_sig_fixup($i){"` |
|     - | 3202 | `" if($i === null){ return $i; }"` |
|     - | 3203 | `" if(isset($i['ret2'])){ $i['rettext'] = $i['ret2']; }"` |
|     - | 3204 | `" if(!isset($i['sig']) \|\| $i['sig'] === ''){ return $i; }"` |
|     - | 3205 | `" $i['params'] = __reflect_parse_sig($i['sig']);"` |
|     - | 3206 | `" $i['minarg'] = -1;"` |
|     - | 3207 | `" $v = false;"` |
|     - | 3208 | `" foreach($i['params'] as $p){ if($p['variadic']){ $v = true; } }"` |
|     - | 3209 | `" $i['variadic'] = $v;"` |
|     - | 3210 | `" return $i;"` |
|     - | 3211 | `"}"` |
|     - | 3212 | `;` |
|     - | 3213 | `/*` |
|     - | 3214 | ` * Chunk 9: PHP's Reflection export format (__toString on every Reflector).` |
|     - | 3215 | ` * Built entirely from the public reflection API of the target objects.` |
|     - | 3216 | ` */` |
|     - | 3217 | `static const char zReflectLib9[] =` |
|     - | 3218 | `"function __reflect_export_value($v){"` |
|     - | 3219 | `" if($v === null){ return 'NULL'; }"` |
|     - | 3220 | `" if($v === true){ return 'true'; }"` |
|     - | 3221 | `" if($v === false){ return 'false'; }"` |
|     - | 3222 | `" if(is_string($v)){ return chr(39).$v.chr(39); }"` |
|     - | 3223 | `" if(is_array($v)){"` |
|     - | 3224 | `"  $parts = array();"` |
|     - | 3225 | `"  $isList = true;"` |
|     - | 3226 | `"  $next = 0;"` |
|     - | 3227 | `"  foreach($v as $k => $x){"` |
|     - | 3228 | `"   if($k !== $next){ $isList = false; break; }"` |
|     - | 3229 | `"   $next++;"` |
|     - | 3230 | `"  }"` |
|     - | 3231 | `"  foreach($v as $k => $x){"` |
|     - | 3232 | `"   $parts[] = $isList ? __reflect_export_value($x)"` |
|     - | 3233 | `"    : (__reflect_export_value($k).' => '.__reflect_export_value($x));"` |
|     - | 3234 | `"  }"` |
|     - | 3235 | `"  return '['.implode(', ', $parts).']';"` |
|     - | 3236 | `" }"` |
|     - | 3237 | `" return (string)$v;"` |
|     - | 3238 | `"}"` |
|     - | 3239 | `"function __reflect_export_param($p){"` |
|     - | 3240 | `" $s = 'Parameter #'.$p->getPosition().' [ <'.($p->isOptional() ? 'optional' : 'required').'> ';"` |
|     - | 3241 | `" $t = $p->getType();"` |
|     - | 3242 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3243 | `" if($p->isPassedByReference()){ $s .= '&'; }"` |
|     - | 3244 | `" if($p->isVariadic()){ $s .= '...'; }"` |
|     - | 3245 | `" $s .= '$'.$p->getName();"` |
|     - | 3246 | `" if($p->isDefaultValueAvailable()){"` |
|     - | 3247 | `"  try{ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3248 | `"  catch(ReflectionException $e){ $s .= ' = <default>'; }"` |
|     - | 3249 | `" }"` |
|     - | 3250 | `" return $s.' ]';"` |
|     - | 3251 | `"}"` |
|     - | 3252 | `"function __reflect_export_prop($p){"` |
|     - | 3253 | `" $s = 'Property [ ';"` |
|     - | 3254 | `" $s .= $p->isPrivate() ? 'private ' : ($p->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3255 | `" if($p->isStatic()){ $s .= 'static '; }"` |
|     - | 3256 | `" if($p->isReadOnly()){ $s .= 'readonly '; }"` |
|     - | 3257 | `" $t = $p->getType();"` |
|     - | 3258 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3259 | `" $s .= '$'.$p->getName();"` |
|     - | 3260 | `" if($p->hasDefaultValue()){ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3261 | `" return $s.' ]'.chr(10);"` |
|     - | 3262 | `"}"` |
|     - | 3263 | `"function __reflect_export_cconst($c){"` |
|     - | 3264 | `" $v = $c->getValue();"` |
|     - | 3265 | `" if(is_int($v)){ $t = 'int'; }"` |
|     - | 3266 | `" else if(is_string($v)){ $t = 'string'; }"` |
|     - | 3267 | `" else if(is_float($v)){ $t = 'float'; }"` |
|     - | 3268 | `" else if(is_bool($v)){ $t = 'bool'; }"` |
|     - | 3269 | `" else if(is_array($v)){ $t = 'array'; }"` |
|     - | 3270 | `" else{ $t = 'null'; }"` |
|     - | 3271 | `" $vs = is_array($v) ? 'Array' : (is_bool($v) ? ($v ? '1' : '') : (string)$v);"` |
|     - | 3272 | `" $vis = $c->isPrivate() ? 'private' : ($c->isProtected() ? 'protected' : 'public');"` |
|     - | 3273 | `" return 'Constant [ '.$vis.' '.$t.' '.$c->name.' ] { '.$vs.' }'.chr(10);"` |
|     - | 3274 | `"}"` |
|     - | 3275 | `"function __reflect_export_fnabs($r, $indent){"` |
|     - | 3276 | `" $tags = $r->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3277 | `" if($r instanceof ReflectionMethod){"` |
|     - | 3278 | `"  if($r->isConstructor()){ $tags .= ', ctor'; }"` |
|     - | 3279 | `"  else if($r->isDestructor()){ $tags .= ', dtor'; }"` |
|     - | 3280 | `"  $decl = $r->getDeclaringClass()->name;"` |
|     - | 3281 | `"  if(strtolower($decl) !== strtolower($r->class)){ $tags .= ', inherits '.$decl; }"` |
|     - | 3282 | `"  else if($r->hasPrototype()){ $tags .= ', prototype '.$r->getPrototype()->class; }"` |
|     - | 3283 | `"  $head = 'Method [ <'.$tags.'> ';"` |
|     - | 3284 | `"  if($r->isAbstract()){ $head .= 'abstract '; }"` |
|     - | 3285 | `"  if($r->isFinal()){ $head .= 'final '; }"` |
|     - | 3286 | `"  if($r->isStatic()){ $head .= 'static '; }"` |
|     - | 3287 | `"  $head .= $r->isPrivate() ? 'private ' : ($r->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3288 | `"  $head .= 'method '.$r->name.' ]';"` |
|     - | 3289 | `" }else{"` |
|     - | 3290 | `"  $kind = $r->isClosure() ? 'Closure' : 'Function';"` |
|     - | 3291 | `"  $head = $kind.' [ <'.$tags.'> function '.$r->name.' ]';"` |
|     - | 3292 | `" }"` |
|     - | 3293 | `" $s = $head.' {'.chr(10);"` |
|     - | 3294 | `" if(!$r->isInternal()){"` |
|     - | 3295 | `"  $s .= '  @@ '.$r->getFileName().' '.$r->getStartLine().' - '.$r->getEndLine().chr(10);"` |
|     - | 3296 | `" }"` |
|     - | 3297 | `" $ps = $r->getParameters();"` |
|     - | 3298 | `" $ret = $r->getReturnType();"` |
|     - | 3299 | `" if(count($ps) > 0 \|\| $ret !== null){"` |
|     - | 3300 | `"  $s .= chr(10).'  - Parameters ['.count($ps).'] {'.chr(10);"` |
|     - | 3301 | `"  foreach($ps as $p){ $s .= '    '.__reflect_export_param($p).chr(10); }"` |
|     - | 3302 | `"  $s .= '  }'.chr(10);"` |
|     - | 3303 | `" }"` |
|     - | 3304 | `" if($ret !== null){ $s .= '  - Return [ '.(string)$ret.' ]'.chr(10); }"` |
|     - | 3305 | `" $s .= '}'.chr(10);"` |
|     - | 3306 | `" if($indent === ''){ return $s; }"` |
|     - | 3307 | `" $lines = explode(chr(10), $s);"` |
|     - | 3308 | `" $out = '';"` |
|     - | 3309 | `" $n = count($lines);"` |
|     - | 3310 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3311 | `"  if($lines[$k] === '' && $k === $n - 1){ break; }"` |
|     - | 3312 | `"  $out .= ($lines[$k] === '' ? '' : $indent.$lines[$k]).chr(10);"` |
|     - | 3313 | `" }"` |
|     - | 3314 | `" return $out;"` |
|     - | 3315 | `"}"` |
|     - | 3316 | `"function __reflect_export_class($rc){"` |
|     - | 3317 | `" $tags = $rc->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3318 | `" if($rc->isInterface()){"` |
|     - | 3319 | `"  $head = 'Interface [ <'.$tags.'> interface '.$rc->name.' ]';"` |
|     - | 3320 | `" }else{"` |
|     - | 3321 | `"  $mods = '';"` |
|     - | 3322 | `"  if($rc->isAbstract()){ $mods .= 'abstract '; }"` |
|     - | 3323 | `"  if($rc->isFinal()){ $mods .= 'final '; }"` |
|     - | 3324 | `"  $head = 'Class [ <'.$tags.'> '.$mods.'class '.$rc->name;"` |
|     - | 3325 | `"  $par = $rc->getParentClass();"` |
|     - | 3326 | `"  if($par !== false){ $head .= ' extends '.$par->name; }"` |
|     - | 3327 | `"  $ifs = $rc->getInterfaceNames();"` |
|     - | 3328 | `"  if(count($ifs) > 0){ $head .= ' implements '.implode(', ', $ifs); }"` |
|     - | 3329 | `"  $head .= ' ]';"` |
|     - | 3330 | `" }"` |
|     - | 3331 | `" $s = $head.' {'.chr(10);"` |
|     - | 3332 | `" if(!$rc->isInternal()){"` |
|     - | 3333 | `"  $s .= '  @@ '.$rc->getFileName().' '.$rc->getStartLine().'-'.$rc->getEndLine().chr(10);"` |
|     - | 3334 | `" }"` |
|     - | 3335 | `" $consts = $rc->getReflectionConstants();"` |
|     - | 3336 | `" $s .= chr(10).'  - Constants ['.count($consts).'] {'.chr(10);"` |
|     - | 3337 | `" foreach($consts as $c){ $s .= '    '.__reflect_export_cconst($c); }"` |
|     - | 3338 | `" $s .= '  }'.chr(10);"` |
|     - | 3339 | `" $sp = array();"` |
|     - | 3340 | `" $ip = array();"` |
|     - | 3341 | `" foreach($rc->getProperties() as $p){"` |
|     - | 3342 | `"  if($p->isStatic()){ $sp[] = $p; }else{ $ip[] = $p; }"` |
|     - | 3343 | `" }"` |
|     - | 3344 | `" $sm = array();"` |
|     - | 3345 | `" $im = array();"` |
|     - | 3346 | `" foreach($rc->getMethods() as $m){"` |
|     - | 3347 | `"  if($m->isStatic()){ $sm[] = $m; }else{ $im[] = $m; }"` |
|     - | 3348 | `" }"` |
|     - | 3349 | `" $s .= chr(10).'  - Static properties ['.count($sp).'] {'.chr(10);"` |
|     - | 3350 | `" foreach($sp as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3351 | `" $s .= '  }'.chr(10);"` |
|     - | 3352 | `" $s .= chr(10).'  - Static methods ['.count($sm).'] {'.chr(10);"` |
|     - | 3353 | `" $first = true;"` |
|     - | 3354 | `" foreach($sm as $m){"` |
|     - | 3355 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3356 | `"  $first = false;"` |
|     - | 3357 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3358 | `" }"` |
|     - | 3359 | `" $s .= '  }'.chr(10);"` |
|     - | 3360 | `" $s .= chr(10).'  - Properties ['.count($ip).'] {'.chr(10);"` |
|     - | 3361 | `" foreach($ip as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3362 | `" $s .= '  }'.chr(10);"` |
|     - | 3363 | `" $s .= chr(10).'  - Methods ['.count($im).'] {'.chr(10);"` |
|     - | 3364 | `" $first = true;"` |
|     - | 3365 | `" foreach($im as $m){"` |
|     - | 3366 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3367 | `"  $first = false;"` |
|     - | 3368 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3369 | `" }"` |
|     - | 3370 | `" $s .= '  }'.chr(10);"` |
|     - | 3371 | `" return $s.'}'.chr(10);"` |
|     - | 3372 | `"}"` |
|     - | 3373 | `;` |
|     - | 3374 | `/*` |
|     - | 3375 | ` * Register the __reflect_* thunks and compile the Reflection library.` |
|     - | 3376 | ` * Called from PH7_VmInit while pVm->bCompilingBuiltin is set, right after` |
|     - | 3377 | ` * the core builtin chunks (Exception and friends must exist already).` |
|     - | 3378 | ` */` |
|  3870 | 3379 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|     5 | 3380 | `{` |
|     - | 3381 | `	static const struct {` |
|     - | 3382 | `		const char *zName;` |
|     - | 3383 | `		ProchHostFunction xFunc;` |
|     - | 3384 | `	} aFunc[] = {` |
|     - | 3385 | `		{ "__reflect_class_info",     vm_builtin_reflect_class_info },` |
|     - | 3386 | `		{ "__reflect_const_value",    vm_builtin_reflect_const_value },` |
|     - | 3387 | `		{ "__reflect_static_value",   vm_builtin_reflect_static_value },` |
|     - | 3388 | `		{ "__reflect_static_set",     vm_builtin_reflect_static_set },` |
|     - | 3389 | `		{ "__reflect_prop_default",   vm_builtin_reflect_prop_default },` |
|     - | 3390 | `		{ "__reflect_new_instance",   vm_builtin_reflect_new_instance },` |
|     - | 3391 | `		{ "__reflect_new_no_ctor",    vm_builtin_reflect_new_no_ctor },` |
|     - | 3392 | `		{ "__reflect_func_info",      vm_builtin_reflect_func_info },` |
|     - | 3393 | `		{ "__reflect_param_default",  vm_builtin_reflect_param_default },` |
|     - | 3394 | `		{ "__reflect_param_defconst", vm_builtin_reflect_param_defconst },` |
|     - | 3395 | `		{ "__reflect_invoke",         vm_builtin_reflect_invoke },` |
|     - | 3396 | `		{ "__reflect_closure",        vm_builtin_reflect_closure },` |
|     - | 3397 | `		{ "__reflect_prop_read",      vm_builtin_reflect_prop_read },` |
|     - | 3398 | `		{ "__reflect_prop_write",     vm_builtin_reflect_prop_write },` |
|     - | 3399 | `		{ "__reflect_prop_state",     vm_builtin_reflect_prop_state },` |
|     - | 3400 | `		{ "__reflect_dyn_props",      vm_builtin_reflect_dyn_props },` |
|     - | 3401 | `		{ "__reflect_gen_info",       vm_builtin_reflect_gen_info },` |
|     - | 3402 | `		{ "__reflect_gen_exec",       vm_builtin_reflect_gen_exec },` |
|     - | 3403 | `		{ "__reflect_const_info",     vm_builtin_reflect_const_info },` |
|     - | 3404 | `		{ "__reflect_ref_id",         vm_builtin_reflect_ref_id },` |
|     - | 3405 | `		{ "__reflect_attr_args",      vm_builtin_reflect_attr_args },` |
|     - | 3406 | `	};` |
|     - | 3407 | `	sxu32 n;` |
|     - | 3408 | `	sxi32 rc;` |
| 85145 | 3409 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 81275 | 3410 | `		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);` |
| 40640 | 3411 | `	}` |
|  3875 | 3412 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib1, sizeof(zReflectLib1)-1);` |
|  3875 | 3413 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3414 | `		return rc;` |
|     - | 3415 | `	}` |
|  3875 | 3416 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib2, sizeof(zReflectLib2)-1);` |
|  3875 | 3417 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3418 | `		return rc;` |
|     - | 3419 | `	}` |
|  3875 | 3420 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib3, sizeof(zReflectLib3)-1);` |
|  3875 | 3421 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3422 | `		return rc;` |
|     - | 3423 | `	}` |
|  3875 | 3424 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib4, sizeof(zReflectLib4)-1);` |
|  3875 | 3425 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3426 | `		return rc;` |
|     - | 3427 | `	}` |
|  3875 | 3428 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib5, sizeof(zReflectLib5)-1);` |
|  3875 | 3429 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3430 | `		return rc;` |
|     - | 3431 | `	}` |
|  3875 | 3432 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib6, sizeof(zReflectLib6)-1);` |
|  3875 | 3433 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3434 | `		return rc;` |
|     - | 3435 | `	}` |
|  3875 | 3436 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib7, sizeof(zReflectLib7)-1);` |
|  3875 | 3437 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3438 | `		return rc;` |
|     - | 3439 | `	}` |
|  3875 | 3440 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib8, sizeof(zReflectLib8)-1);` |
|  3875 | 3441 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3442 | `		return rc;` |
|     - | 3443 | `	}` |
|  3875 | 3444 | `	return PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib9, sizeof(zReflectLib9)-1);` |
|  1940 | 3445 | `}` |
|     - | 3446 |  |
