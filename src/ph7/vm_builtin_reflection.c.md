# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1036/1219 lines (84.99%)

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
|   634 |   31 | `static ph7_class * ReflectResolveClass(ph7_vm *pVm, ph7_value *pArg)` |
|     3 |   32 | `{` |
|     - |   33 | `	ph7_class *pClass;` |
|   637 |   34 | `	pClass = PH7_VmExtractClassFromValue(pVm, pArg);` |
|   637 |   35 | `	if( pClass == 0 && ph7_value_is_string(pArg) ){` |
|     - |   36 | `		const char *zName;` |
|     - |   37 | `		int nLen;` |
|    16 |   38 | `		zName = ph7_value_to_string(pArg, &nLen);` |
|    16 |   39 | `		if( nLen > 0 ){` |
|    16 |   40 | `			pClass = PH7_VmTriggerAutoload(pVm, zName, (sxu32)nLen, FALSE);` |
|     7 |   41 | `		}` |
|     7 |   42 | `	}` |
|   637 |   43 | `	return pClass;` |
|     3 |   44 | `}` |
|     - |   45 | `/*` |
|     - |   46 | ` * Hand a freshly created class instance to the caller. The return slot` |
|     - |   47 | ` * takes over the initial reference from PH7_NewClassInstance (iRef=1):` |
|     - |   48 | ` * no extra iRef++ here (see the synthesized-object invariant — a stray` |
|     - |   49 | ` * bump leaks the object and disables its __destruct).` |
|     - |   50 | ` */` |
|    88 |   51 | `static int ReflectResultObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|     1 |   52 | `{` |
|    89 |   53 | `	if( pObj == 0 ){` |
|   ! 0 |   54 | `		ph7_result_null(pCtx);` |
|   ! 0 |   55 | `		return PH7_OK;` |
|     - |   56 | `	}` |
|    89 |   57 | `	PH7_MemObjRelease(pCtx->pRet);` |
|    89 |   58 | `	pCtx->pRet->x.pOther = pObj;` |
|    89 |   59 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|    89 |   60 | `	return PH7_OK;` |
|    45 |   61 | `}` |
|     - |   62 | `/* --- Marshaling helpers: build the descriptor arrays handed to the PHP layer --- */` |
| 16908 |   63 | `static void ReflectMapAddBool(ph7_context *pCtx, ph7_value *pMap, const char *zKey, int b)` |
|     3 |   64 | `{` |
| 16911 |   65 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 16911 |   66 | `	if( p == 0 ){ return; }` |
| 16911 |   67 | `	ph7_value_bool(p, b);` |
| 16911 |   68 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  8457 |   69 | `}` |
|  5046 |   70 | `static void ReflectMapAddInt(ph7_context *pCtx, ph7_value *pMap, const char *zKey, sxi64 iVal)` |
|     3 |   71 | `{` |
|  5049 |   72 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  5049 |   73 | `	if( p == 0 ){ return; }` |
|  5049 |   74 | `	ph7_value_int64(p, iVal);` |
|  5049 |   75 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  2526 |   76 | `}` |
|  4902 |   77 | `static void ReflectMapAddStr(ph7_context *pCtx, ph7_value *pMap, const char *zKey,` |
|     - |   78 | `	const char *zVal, int nVal)` |
|     3 |   79 | `{` |
|  4905 |   80 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  4905 |   81 | `	if( p == 0 ){ return; }` |
|  4905 |   82 | `	ph7_value_string(p, zVal, nVal);` |
|  4905 |   83 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  2454 |   84 | `}` |
|  1598 |   85 | `static void ReflectMapAddNull(ph7_context *pCtx, ph7_value *pMap, const char *zKey)` |
|     2 |   86 | `{` |
|  1600 |   87 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  1600 |   88 | `	if( p == 0 ){ return; }` |
|  1600 |   89 | `	ph7_value_null(p);` |
|  1600 |   90 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|   801 |   91 | `}` |
|     - |   92 | `/* Add an entry under a dynamic (SyString) key. */` |
|   418 |   93 | `static void ReflectMapAddDyn(ph7_context *pCtx, ph7_value *pMap,` |
|     - |   94 | `	const SyString *pKey, ph7_value *pVal)` |
|     1 |   95 | `{` |
|   419 |   96 | `	ph7_value *pK = ph7_context_new_scalar(pCtx);` |
|   419 |   97 | `	if( pK == 0 ){ return; }` |
|   419 |   98 | `	ph7_value_string(pK, pKey->zString, (int)pKey->nByte);` |
|   419 |   99 | `	ph7_array_add_elem(pMap, pK, pVal);` |
|   210 |  100 | `}` |
|     - |  101 | `/* Emit the declared #[...] attributes of a target as a summary list:` |
|     - |  102 | ` * [ {name, line} ... ]. Argument values stay lazy — the PHP layer pulls` |
|     - |  103 | ` * them through __reflect_attr_args when ReflectionAttribute needs them. */` |
|  2172 |  104 | `static void ReflectMapAddAttrs(ph7_context *pCtx, ph7_value *pMap, SySet *pAttrs)` |
|     2 |  105 | `{` |
|  2174 |  106 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|  2174 |  107 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|     - |  108 | `	sxu32 n;` |
|  2174 |  109 | `	if( pList == 0 ){` |
|   ! 0 |  110 | `		return;` |
|     - |  111 | `	}` |
|  2274 |  112 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|   101 |  113 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|   101 |  114 | `		if( pMeta == 0 ){ break; }` |
|   101 |  115 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aA[n].sName), (int)SyStringLength(&aA[n].sName));` |
|   101 |  116 | `		ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)aA[n].nLine);` |
|   101 |  117 | `		ph7_array_add_elem(pList, 0, pMeta);` |
|    51 |  118 | `	}` |
|  2174 |  119 | `	ph7_array_add_strkey_elem(pMap, "attrs", pList);` |
|  1088 |  120 | `}` |
|     - |  121 | `/* Emit a doc-comment field: the text when present, else boolean false` |
|     - |  122 | ` * (getDocComment()'s exact return contract). */` |
|   882 |  123 | `static void ReflectMapAddDoc(ph7_context *pCtx, ph7_value *pMap, const SyString *pDoc)` |
|     2 |  124 | `{` |
|   884 |  125 | `	if( SyStringLength(pDoc) > 0 ){` |
|    21 |  126 | `		ReflectMapAddStr(pCtx, pMap, "doc", SyStringData(pDoc), (int)SyStringLength(pDoc));` |
|    11 |  127 | `	}else{` |
|   864 |  128 | `		ReflectMapAddBool(pCtx, pMap, "doc", 0);` |
|     - |  129 | `	}` |
|   884 |  130 | `}` |
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
|   154 |  164 | `static void ReflectCollectInterfaces(ph7_class *pClass, SySet *pOut, int iDepth)` |
|     2 |  165 | `{` |
|     - |  166 | `	ph7_class **apIface;` |
|     - |  167 | `	sxu32 n;` |
|   156 |  168 | `	if( pClass == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|   ! 0 |  169 | `		return;` |
|     - |  170 | `	}` |
|   156 |  171 | `	if( pClass->pBase ){` |
|    30 |  172 | `		ReflectCollectInterfaces(pClass->pBase, pOut, iDepth + 1);` |
|    14 |  173 | `	}` |
|   156 |  174 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|   194 |  175 | `	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){` |
|    40 |  176 | `		ReflectAddInterface(apIface[n], pOut, iDepth + 1);` |
|    21 |  177 | `	}` |
|    79 |  178 | `}` |
|     - |  179 | `/*` |
|     - |  180 | ` * Deepest base class whose method table maps the same name to the very` |
|     - |  181 | ` * same ph7_class_method pointer: inheritance shares member pointers` |
|     - |  182 | ` * (PH7_ClassInherit), so this identifies the declaring class. Methods` |
|     - |  183 | ` * copied in from traits are not on the pBase chain and thus report the` |
|     - |  184 | ` * using class, which is what PHP reports too.` |
|     - |  185 | ` */` |
|   736 |  186 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth)` |
|     1 |  187 | `{` |
|   737 |  188 | `	ph7_class *pDecl = pClass;` |
|   737 |  189 | `	ph7_class *pBase = pClass->pBase;` |
|   737 |  190 | `	int iDepth = 0;` |
|  1059 |  191 | `	while( pBase && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|     - |  192 | `		SyHashEntry *pEntry;` |
|   853 |  193 | `		pEntry = SyHashGet(&pBase->hMethod, (const void *)SyStringData(&pMeth->sFunc.sName),` |
|   284 |  194 | `			SyStringLength(&pMeth->sFunc.sName));` |
|   569 |  195 | `		if( pEntry == 0 \|\| (ph7_class_method *)pEntry->pUserData != pMeth ){` |
|   124 |  196 | `			break;` |
|     - |  197 | `		}` |
|   323 |  198 | `		pDecl = pBase;` |
|   323 |  199 | `		pBase = pBase->pBase;` |
|   323 |  200 | `		iDepth++;` |
|     1 |  201 | `	}` |
|   737 |  202 | `	return pDecl;` |
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
|   140 |  233 | `static int vm_builtin_reflect_class_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 |  234 | `{` |
|   143 |  235 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  236 | `	ph7_class *pClass;` |
|     - |  237 | `	ph7_value *pInfo, *pConsts, *pProps, *pMethods, *pList;` |
|     - |  238 | `	SyHashEntry *pEntry;` |
|     - |  239 | `	SySet aIfaceSet;` |
|   143 |  240 | `	sxi32 iCtorVis = 0, iCloneVis = 0;` |
|   143 |  241 | `	int bIterable = 0;` |
|     - |  242 | `	sxu32 n;` |
|   143 |  243 | `	if( nArg < 1 ){` |
|   ! 0 |  244 | `		ph7_result_null(pCtx);` |
|   ! 0 |  245 | `		return PH7_OK;` |
|     - |  246 | `	}` |
|   143 |  247 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|   143 |  248 | `	if( pClass == 0 ){` |
|    16 |  249 | `		ph7_result_null(pCtx);` |
|    16 |  250 | `		return PH7_OK;` |
|     - |  251 | `	}` |
|   128 |  252 | `	pInfo = ph7_context_new_array(pCtx);` |
|   128 |  253 | `	pConsts = ph7_context_new_array(pCtx);` |
|   128 |  254 | `	pProps = ph7_context_new_array(pCtx);` |
|   128 |  255 | `	pMethods = ph7_context_new_array(pCtx);` |
|   128 |  256 | `	if( pInfo == 0 \|\| pConsts == 0 \|\| pProps == 0 \|\| pMethods == 0 ){` |
|   ! 0 |  257 | `		ph7_result_null(pCtx);` |
|   ! 0 |  258 | `		return PH7_OK;` |
|     - |  259 | `	}` |
|   128 |  260 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   128 |  261 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pClass->iFlags & PH7_CLASS_INTERNAL) != 0);` |
|   128 |  262 | `	ReflectMapAddBool(pCtx, pInfo, "interface", (pClass->iFlags & PH7_CLASS_INTERFACE) != 0);` |
|   128 |  263 | `	ReflectMapAddBool(pCtx, pInfo, "trait", (pClass->iFlags & PH7_CLASS_TRAIT) != 0);` |
|   128 |  264 | `	ReflectMapAddBool(pCtx, pInfo, "abstract", (pClass->iFlags & PH7_CLASS_ABSTRACT) != 0);` |
|   128 |  265 | `	ReflectMapAddBool(pCtx, pInfo, "final", (pClass->iFlags & PH7_CLASS_FINAL) != 0);` |
|   128 |  266 | `	ReflectMapAddBool(pCtx, pInfo, "readonly", (pClass->iFlags & PH7_CLASS_READONLY) != 0);` |
|   128 |  267 | `	ReflectMapAddBool(pCtx, pInfo, "enum", (pClass->iFlags & PH7_CLASS_ENUM) != 0);` |
|   128 |  268 | `	if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|   ! 0 |  269 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "int", (int)sizeof("int")-1);` |
|   128 |  270 | `	}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|     3 |  271 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "string", (int)sizeof("string")-1);` |
|     2 |  272 | `	}else{` |
|   126 |  273 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "", 0);` |
|     - |  274 | `	}` |
|     - |  275 | `	{` |
|     - |  276 | `		/* Enum case names in declaration order (empty list for non-enums) */` |
|   128 |  277 | `		ph7_value *pCases = ph7_context_new_array(pCtx);` |
|   128 |  278 | `		if( pCases ){` |
|   128 |  279 | `			ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     - |  280 | `			sxu32 nCase;` |
|   134 |  281 | `			for( nCase = 0 ; nCase < SySetUsed(&pClass->aEnumCases) ; nCase++ ){` |
|     7 |  282 | `				ph7_value *pNm = ph7_context_new_scalar(pCtx);` |
|     7 |  283 | `				if( pNm ){` |
|     7 |  284 | `					ph7_value_string(pNm,apCase[nCase]->sName.zString,(int)apCase[nCase]->sName.nByte);` |
|     7 |  285 | `					ph7_array_add_elem(pCases,0,pNm);` |
|     3 |  286 | `				}` |
|     4 |  287 | `			}` |
|   128 |  288 | `			ph7_array_add_strkey_elem(pInfo,"cases",pCases);` |
|    63 |  289 | `		}` |
|     - |  290 | `	}` |
|   128 |  291 | `	if( pClass->pBase ){` |
|    38 |  292 | `		ReflectMapAddStr(pCtx, pInfo, "parent", SyStringData(&pClass->pBase->sName),` |
|    24 |  293 | `			(int)SyStringLength(&pClass->pBase->sName));` |
|    14 |  294 | `	}else{` |
|   104 |  295 | `		ReflectMapAddNull(pCtx, pInfo, "parent");` |
|     - |  296 | `	}` |
|     - |  297 | `	/* Transitive interfaces */` |
|   128 |  298 | `	SySetInit(&aIfaceSet, &pVm->sAllocator, sizeof(ph7_class *));` |
|   128 |  299 | `	ReflectCollectInterfaces(pClass, &aIfaceSet, 0);` |
|   128 |  300 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     - |  301 | `		/* An interface's own parents count as its interface list */` |
|    11 |  302 | `		if( pClass->pBase ){` |
|     3 |  303 | `			ReflectAddInterface(pClass->pBase, &aIfaceSet, 0);` |
|     1 |  304 | `		}` |
|     5 |  305 | `	}` |
|   128 |  306 | `	pList = ph7_context_new_array(pCtx);` |
|   128 |  307 | `	if( pList ){` |
|   128 |  308 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIfaceSet);` |
|   168 |  309 | `		for( n = 0 ; n < SySetUsed(&aIfaceSet) ; n++ ){` |
|    42 |  310 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|    42 |  311 | `			if( pName == 0 ){ break; }` |
|    42 |  312 | `			ph7_value_string(pName, SyStringData(&apIface[n]->sName), (int)SyStringLength(&apIface[n]->sName));` |
|    42 |  313 | `			ph7_array_add_elem(pList, 0, pName);` |
|    42 |  314 | `			if( pVm->pTraversableClass && apIface[n] == pVm->pTraversableClass ){` |
|     3 |  315 | `				bIterable = 1;` |
|     1 |  316 | `			}` |
|    22 |  317 | `		}` |
|   128 |  318 | `		ph7_array_add_strkey_elem(pInfo, "interfaces", pList);` |
|    63 |  319 | `	}` |
|   128 |  320 | `	SySetRelease(&aIfaceSet);` |
|   128 |  321 | `	ReflectMapAddBool(pCtx, pInfo, "iterable", bIterable);` |
|     - |  322 | `	/* Used traits */` |
|   128 |  323 | `	pList = ph7_context_new_array(pCtx);` |
|   128 |  324 | `	if( pList ){` |
|   128 |  325 | `		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|   130 |  326 | `		for( n = 0 ; n < SySetUsed(&pClass->aTrait) ; n++ ){` |
|     3 |  327 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|     3 |  328 | `			if( pName == 0 ){ break; }` |
|     3 |  329 | `			ph7_value_string(pName, SyStringData(&apTrait[n]->sName), (int)SyStringLength(&apTrait[n]->sName));` |
|     3 |  330 | `			ph7_array_add_elem(pList, 0, pName);` |
|     2 |  331 | `		}` |
|   128 |  332 | `		ph7_array_add_strkey_elem(pInfo, "traits", pList);` |
|    63 |  333 | `	}` |
|     - |  334 | `	/* File / lines: no file recorded => false, like PHP internals */` |
|   128 |  335 | `	if( SyStringLength(&pClass->sFile) > 0 ){` |
|   116 |  336 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));` |
|    59 |  337 | `	}else{` |
|    13 |  338 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - |  339 | `	}` |
|   128 |  340 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pClass->nLine);` |
|   128 |  341 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pClass->nEndLine);` |
|   128 |  342 | `	ReflectMapAddDoc(pCtx, pInfo, &pClass->sDoc);` |
|   128 |  343 | `	ReflectMapAddAttrs(pCtx, pInfo, &pClass->aAttrs);` |
|     - |  344 | `	/* Members are emitted in PHP's reporting order: the class's own members` |
|     - |  345 | `	 * first (declaration order), then each inheritance level's, outward.` |
|     - |  346 | `	 * Per level we iterate the DECLARING class's own hash — subclass hashes` |
|     - |  347 | `	 * interleave inherited pointers unpredictably — and emit buffered` |
|     - |  348 | `	 * entries in reverse, because SyHash lists are LIFO. A pointer-identity` |
|     - |  349 | `	 * lookup in the reflected class's hash filters out members that are not` |
|     - |  350 | `	 * visible there (base privates, overridden entries). */` |
|     - |  351 | `	{` |
|     - |  352 | `		ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];` |
|   128 |  353 | `		ph7_class *pWalk = pClass;` |
|     - |  354 | `		SySet aTmp;` |
|   128 |  355 | `		sxu32 nChain = 0, iLevel, nT;` |
|   282 |  356 | `		while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){` |
|   156 |  357 | `			aChain[nChain++] = pWalk;` |
|   156 |  358 | `			pWalk = pWalk->pBase;` |
|     2 |  359 | `		}` |
|   128 |  360 | `		SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));` |
|   282 |  361 | `		for( iLevel = 0 ; iLevel < nChain ; iLevel++ ){` |
|   156 |  362 | `			ph7_class *pLevel = aChain[iLevel];` |
|     - |  363 | `			/* --- Constants and properties (shared attribute table) --- */` |
|   156 |  364 | `			SySetReset(&aTmp);` |
|   156 |  365 | `			SyHashResetLoopCursor(&pLevel->hAttr);` |
|   376 |  366 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hAttr)) != 0 ){` |
|   221 |  367 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|   221 |  368 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|   221 |  369 | `				if( iLevel == 0 ){` |
|     - |  370 | `					sxu32 j;` |
|     - |  371 | `					/* Own = declared here or by an off-chain provider (trait) */` |
|   215 |  372 | `					for( j = 1 ; j < nChain ; j++ ){` |
|    75 |  373 | `						if( aChain[j] == pDecl ){ break; }` |
|    21 |  374 | `					}` |
|   175 |  375 | `					if( j < nChain ){ continue; }` |
|    71 |  376 | `				}else{` |
|     - |  377 | `					SyHashEntry *pSub;` |
|    47 |  378 | `					if( pDecl != pLevel ){ continue; }` |
|     - |  379 | `					/* Must still be the visible member in the reflected class */` |
|    35 |  380 | `					pSub = SyHashGet(&pClass->hAttr, pEntry->pKey, pEntry->nKeyLen);` |
|    35 |  381 | `					if( pSub == 0 \|\| pSub->pUserData != (void *)pAttr ){ continue; }` |
|     - |  382 | `				}` |
|   175 |  383 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     1 |  384 | `			}` |
|   330 |  385 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|   175 |  386 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|   175 |  387 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;` |
|   175 |  388 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|   175 |  389 | `				ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|   175 |  390 | `				if( pMeta == 0 ){ break; }` |
|   175 |  391 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pAttr->iProtection);` |
|   175 |  392 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   175 |  393 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pAttr->nLine);` |
|   175 |  394 | `				ReflectMapAddDoc(pCtx, pMeta, &pAttr->sDoc);` |
|   175 |  395 | `				ReflectMapAddAttrs(pCtx, pMeta, &pAttr->aAttrs);` |
|   175 |  396 | `				ReflectMapAddBool(pCtx, pMeta, "typed", (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);` |
|   175 |  397 | `				if( SyStringLength(&pAttr->sTypeName) > 0 ){` |
|    67 |  398 | `					ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&pAttr->sTypeName),` |
|    44 |  399 | `						(int)SyStringLength(&pAttr->sTypeName));` |
|    23 |  400 | `				}else{` |
|   131 |  401 | `					ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - |  402 | `				}` |
|   175 |  403 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|    37 |  404 | `					ReflectMapAddBool(pCtx, pMeta, "final", (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|    37 |  405 | `					ReflectMapAddBool(pCtx, pMeta, "enumcase", (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0);` |
|    37 |  406 | `					ReflectMapAddDyn(pCtx, pConsts, &pAttr->sName, pMeta);` |
|    19 |  407 | `				}else{` |
|   139 |  408 | `					ReflectMapAddBool(pCtx, pMeta, "static", (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   139 |  409 | `					ReflectMapAddBool(pCtx, pMeta, "readonly", (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0);` |
|   139 |  410 | `					ReflectMapAddBool(pCtx, pMeta, "privset", (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) != 0);` |
|   139 |  411 | `					ReflectMapAddBool(pCtx, pMeta, "protset", (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET) != 0);` |
|   139 |  412 | `					ReflectMapAddBool(pCtx, pMeta, "hookget", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) != 0);` |
|   139 |  413 | `					ReflectMapAddBool(pCtx, pMeta, "hookset", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) != 0);` |
|   139 |  414 | `					ReflectMapAddBool(pCtx, pMeta, "virtual", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) != 0);` |
|   139 |  415 | `					ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&pAttr->aByteCode) > 0);` |
|   139 |  416 | `					ReflectMapAddDyn(pCtx, pProps, &pAttr->sName, pMeta);` |
|     - |  417 | `				}` |
|    88 |  418 | `			}` |
|     - |  419 | `			/* --- Methods. The reported name is the hash-entry key: trait` |
|     - |  420 | `			 * aliasing installs a shallow copy under the alias name while` |
|     - |  421 | `			 * sFunc.sName keeps the original, and PHP reports the alias. --- */` |
|   156 |  422 | `			SySetReset(&aTmp);` |
|   156 |  423 | `			SyHashResetLoopCursor(&pLevel->hMethod);` |
|   428 |  424 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){` |
|   273 |  425 | `				ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   273 |  426 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|   273 |  427 | `				if( iLevel == 0 ){` |
|     - |  428 | `					sxu32 j;` |
|   245 |  429 | `					for( j = 1 ; j < nChain ; j++ ){` |
|   105 |  430 | `						if( aChain[j] == pDecl ){ break; }` |
|    26 |  431 | `					}` |
|   195 |  432 | `					if( j < nChain ){ continue; }` |
|    71 |  433 | `				}else{` |
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
|   195 |  449 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     1 |  450 | `			}` |
|   350 |  451 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|   195 |  452 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|   195 |  453 | `				ph7_class_method *pMeth = (ph7_class_method *)pE->pUserData;` |
|   195 |  454 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|     - |  455 | `				ph7_value *pMeta;` |
|     - |  456 | `				SyString sKey;` |
|     - |  457 | `				int bIsAlias;` |
|   195 |  458 | `				SyStringInitFromBuf(&sKey, (const char *)pE->pKey, pE->nKeyLen);` |
|   389 |  459 | `				bIsAlias = (sKey.nByte != SyStringLength(&pMeth->sFunc.sName)` |
|   194 |  460 | `				 \|\| SyMemcmp(sKey.zString, SyStringData(&pMeth->sFunc.sName), sKey.nByte) != 0);` |
|   194 |  461 | `				if( sKey.nByte == sizeof("__construct")-1` |
|   113 |  462 | `				 && SyMemcmp(sKey.zString, "__construct", sKey.nByte) == 0 ){` |
|    25 |  463 | `					if( iCtorVis == 0 ){` |
|    25 |  464 | `						iCtorVis = pMeth->iProtection;` |
|    12 |  465 | `					}` |
|    25 |  466 | `					if( bIsAlias ){` |
|     - |  467 | `						/* Mount-time alias for a legacy class-name constructor:` |
|     - |  468 | `						 * the method is already listed under its declared name. */` |
|   ! 0 |  469 | `						continue;` |
|     - |  470 | `					}` |
|   183 |  471 | `				}else if( sKey.nByte == sizeof("__clone")-1` |
|    99 |  472 | `				 && SyMemcmp(sKey.zString, "__clone", sKey.nByte) == 0 ){` |
|   ! 0 |  473 | `					if( iCloneVis == 0 ){` |
|   ! 0 |  474 | `						iCloneVis = pMeth->iProtection;` |
|   ! 0 |  475 | `					}` |
|   170 |  476 | `				}else if( iCtorVis == 0` |
|   138 |  477 | `				 && sKey.nByte == SyStringLength(&pClass->sName)` |
|    56 |  478 | `				 && SyMemcmp(sKey.zString, SyStringData(&pClass->sName), sKey.nByte) == 0 ){` |
|     - |  479 | `					/* Legacy class-name constructor before the mount alias exists */` |
|   ! 0 |  480 | `					iCtorVis = pMeth->iProtection;` |
|   ! 0 |  481 | `				}` |
|   195 |  482 | `				pMeta = ph7_context_new_array(pCtx);` |
|   195 |  483 | `				if( pMeta == 0 ){ break; }` |
|   195 |  484 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pMeth->iProtection);` |
|   195 |  485 | `				ReflectMapAddBool(pCtx, pMeta, "static", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   195 |  486 | `				ReflectMapAddBool(pCtx, pMeta, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|   195 |  487 | `				ReflectMapAddBool(pCtx, pMeta, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   195 |  488 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   195 |  489 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pMeth->nLine);` |
|   195 |  490 | `				ReflectMapAddDyn(pCtx, pMethods, &sKey, pMeta);` |
|    98 |  491 | `			}` |
|    79 |  492 | `		}` |
|   128 |  493 | `		SySetRelease(&aTmp);` |
|     - |  494 | `	}` |
|   128 |  495 | `	ReflectMapAddInt(pCtx, pInfo, "ctorvis", (sxi64)iCtorVis);` |
|   128 |  496 | `	ReflectMapAddInt(pCtx, pInfo, "clonevis", (sxi64)iCloneVis);` |
|   128 |  497 | `	ph7_array_add_strkey_elem(pInfo, "consts", pConsts);` |
|   128 |  498 | `	ph7_array_add_strkey_elem(pInfo, "props", pProps);` |
|   128 |  499 | `	ph7_array_add_strkey_elem(pInfo, "methods", pMethods);` |
|   128 |  500 | `	ph7_result_value(pCtx, pInfo);` |
|   128 |  501 | `	return PH7_OK;` |
|    73 |  502 | `}` |
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
|    38 |  628 | `static sxi32 ReflectCollectArgs(ph7_context *pCtx, ph7_value *pArray, SySet *pOut, SyString **ppNames)` |
|     1 |  629 | `{` |
|     - |  630 | `	ph7_hashmap *pMap;` |
|     - |  631 | `	ph7_hashmap_node *pEntry;` |
|    39 |  632 | `	SyString *aNames = 0;` |
|    39 |  633 | `	sxu32 nSlot = 0;` |
|     - |  634 | `	sxu32 n;` |
|    39 |  635 | `	if( ppNames ){` |
|    19 |  636 | `		*ppNames = 0;` |
|     9 |  637 | `	}` |
|    39 |  638 | `	if( !ph7_value_is_array(pArray) ){` |
|   ! 0 |  639 | `		return SXRET_OK;` |
|     - |  640 | `	}` |
|    39 |  641 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|    39 |  642 | `	pEntry = pMap->pFirst;` |
|    91 |  643 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    53 |  644 | `		ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|    53 |  645 | `		if( pValue ){` |
|    53 |  646 | `			if( ppNames && pEntry->iType == HASHMAP_BLOB_NODE ){` |
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
|    53 |  659 | `			SySetPut(pOut, (const void *)&pValue);` |
|    53 |  660 | `			nSlot++;` |
|    26 |  661 | `		}` |
|    53 |  662 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|    27 |  663 | `	}` |
|    39 |  664 | `	if( ppNames ){` |
|    19 |  665 | `		*ppNames = aNames;` |
|     9 |  666 | `	}` |
|    39 |  667 | `	return SXRET_OK;` |
|    20 |  668 | `}` |
|     - |  669 | `/*` |
|     - |  670 | ` * object __reflect_new_instance(string $class, array $args)` |
|     - |  671 | ` * Instantiate and run the constructor with positional arguments.` |
|     - |  672 | ` * The PHP layer has already validated instantiability and ctor visibility.` |
|     - |  673 | ` */` |
|    22 |  674 | `static int vm_builtin_reflect_new_instance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  675 | `{` |
|    23 |  676 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  677 | `	ph7_class *pClass;` |
|     - |  678 | `	ph7_class_instance *pThis;` |
|     - |  679 | `	ph7_class_method *pCons;` |
|    23 |  680 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pVm, apArg[0])) == 0 ){` |
|   ! 0 |  681 | `		ph7_result_null(pCtx);` |
|   ! 0 |  682 | `		return PH7_OK;` |
|     - |  683 | `	}` |
|    23 |  684 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|    23 |  685 | `	if( pThis == 0 ){` |
|   ! 0 |  686 | `		ph7_result_null(pCtx);` |
|   ! 0 |  687 | `		return PH7_OK;` |
|     - |  688 | `	}` |
|    23 |  689 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|    23 |  690 | `	if( pCons ){` |
|     - |  691 | `		SySet aArg;` |
|     - |  692 | `		sxi32 rc;` |
|    19 |  693 | `		SyString *aNames = 0;` |
|    19 |  694 | `		SySetInit(&aArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    19 |  695 | `		if( nArg > 1 ){` |
|    19 |  696 | `			ReflectCollectArgs(pCtx, apArg[1], &aArg, &aNames);` |
|     9 |  697 | `		}` |
|    19 |  698 | `		if( aNames ){` |
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
|    25 |  709 | `			rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|    16 |  710 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|     - |  711 | `		}` |
|    19 |  712 | `		SySetRelease(&aArg);` |
|    19 |  713 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 |  714 | `			PH7_ClassInstanceUnref(pThis);` |
|   ! 0 |  715 | `			return rc;` |
|     - |  716 | `		}` |
|     9 |  717 | `	}` |
|    23 |  718 | `	return ReflectResultObject(pCtx, pThis);` |
|    12 |  719 | `}` |
|     - |  720 | `/*` |
|     - |  721 | ` * object __reflect_new_no_ctor(string $class)` |
|     - |  722 | ` * Instantiate without running the constructor (property defaults still` |
|     - |  723 | ` * apply — PH7_NewClassInstance builds the attribute frame).` |
|     - |  724 | ` */` |
|    60 |  725 | `static int vm_builtin_reflect_new_no_ctor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  726 | `{` |
|     - |  727 | `	ph7_class *pClass;` |
|    61 |  728 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0 ){` |
|   ! 0 |  729 | `		ph7_result_null(pCtx);` |
|   ! 0 |  730 | `		return PH7_OK;` |
|     - |  731 | `	}` |
|    61 |  732 | `	return ReflectResultObject(pCtx, PH7_NewClassInstance(pCtx->pVm, pClass));` |
|    31 |  733 | `}` |
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
|   760 |  954 | `static ph7_vm_func * ReflectResolveCallable(ph7_context *pCtx, ph7_value *pTarget,` |
|     - |  955 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|     - |  956 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|     2 |  957 | `{` |
|   762 |  958 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  959 | `	SyHashEntry *pEntry;` |
|   762 |  960 | `	if( ppClass ){ *ppClass = 0; }` |
|   762 |  961 | `	if( ppMeth ){ *ppMeth = 0; }` |
|   762 |  962 | `	if( ppHost ){ *ppHost = 0; }` |
|   762 |  963 | `	if( ppClosure ){ *ppClosure = 0; }` |
|   762 |  964 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
|   293 |  965 | `		ph7_class *pClass = ReflectResolveClass(pVm, pTarget);` |
|     - |  966 | `		ph7_class_method *pMeth;` |
|   293 |  967 | `		if( pClass == 0 ){` |
|   ! 0 |  968 | `			return 0;` |
|     - |  969 | `		}` |
|   439 |  970 | `		pMeth = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|   146 |  971 | `			SyBlobLength(&pMethodArg->sBlob));` |
|   293 |  972 | `		if( pMeth == 0 ){` |
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
|   293 |  986 | `		if( pMeth == 0 ){` |
|   ! 0 |  987 | `			return 0;` |
|     - |  988 | `		}` |
|   293 |  989 | `		if( ppClass ){ *ppClass = pClass; }` |
|   293 |  990 | `		if( ppMeth ){ *ppMeth = pMeth; }` |
|   293 |  991 | `		return &pMeth->sFunc;` |
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
|   382 | 1031 | `}` |
|     - | 1032 | `/* Emit the shared descriptor fields of a compiled function. */` |
|   582 | 1033 | `static void ReflectFillFuncCommon(ph7_context *pCtx, ph7_value *pInfo, ph7_vm_func *pFunc)` |
|     1 | 1034 | `{` |
|     - | 1035 | `	ph7_vm_func_arg *aArg;` |
|     - | 1036 | `	ph7_value *pParams, *pStatics;` |
|   583 | 1037 | `	int bVariadic = 0;` |
|     - | 1038 | `	int bAnon;` |
|     - | 1039 | `	sxu32 n;` |
|     - | 1040 | ``	/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|     - | 1041 | `	 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|   583 | 1042 | `	bAnon = (pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|   582 | 1043 | `	if( !bAnon && SyStringLength(&pFunc->sName) > 9` |
|   312 | 1044 | `	 && (SyMemcmp(SyStringData(&pFunc->sName), "[lambda_", 8) == 0` |
|    82 | 1045 | `	  \|\| SyMemcmp(SyStringData(&pFunc->sName), "[closure_", 9) == 0) ){` |
|     5 | 1046 | `		bAnon = 1;` |
|     2 | 1047 | `	}` |
|   583 | 1048 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|   583 | 1049 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pFunc->iFlags & VM_FUNC_INTERNAL) != 0);` |
|   583 | 1050 | `	ReflectMapAddBool(pCtx, pInfo, "closure", bAnon);` |
|   583 | 1051 | `	ReflectMapAddBool(pCtx, pInfo, "fstatic", (pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);` |
|   583 | 1052 | `	ReflectMapAddBool(pCtx, pInfo, "byref", (pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|   583 | 1053 | `	ReflectMapAddBool(pCtx, pInfo, "generator", (pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|   583 | 1054 | `	ReflectMapAddBool(pCtx, pInfo, "strict", pFunc->bStrictTypes != 0);` |
|   583 | 1055 | `	if( SyStringLength(&pFunc->sFile) > 0 ){` |
|   579 | 1056 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pFunc->sFile), (int)SyStringLength(&pFunc->sFile));` |
|   290 | 1057 | `	}else{` |
|     5 | 1058 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1059 | `	}` |
|   583 | 1060 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pFunc->nLine);` |
|   583 | 1061 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pFunc->nEndLine);` |
|   583 | 1062 | `	ReflectMapAddDoc(pCtx, pInfo, &pFunc->sDoc);` |
|   583 | 1063 | `	ReflectMapAddAttrs(pCtx, pInfo, &pFunc->aAttrs);` |
|   583 | 1064 | `	if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|   145 | 1065 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", SyStringData(&pFunc->sReturnTypeName),` |
|    96 | 1066 | `			(int)SyStringLength(&pFunc->sReturnTypeName));` |
|   535 | 1067 | `	}else if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     - | 1068 | `		/* The type-text renderer omits void/never atoms (compile.c notes the` |
|     - | 1069 | `		 * root fix belongs there); name them here for getReturnType(). */` |
|     3 | 1070 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "void", sizeof("void")-1);` |
|   486 | 1071 | `	}else if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|     3 | 1072 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "never", sizeof("never")-1);` |
|     2 | 1073 | `	}else{` |
|   483 | 1074 | `		ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1075 | `	}` |
|   583 | 1076 | `	ReflectMapAddBool(pCtx, pInfo, "retnullable", (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) != 0);` |
|     - | 1077 | `	/* Parameters */` |
|   583 | 1078 | `	pParams = ph7_context_new_array(pCtx);` |
|   583 | 1079 | `	aArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|  1835 | 1080 | `	for( n = 0 ; pParams && n < SySetUsed(&pFunc->aArgs) ; n++ ){` |
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
|   583 | 1104 | `	if( pParams ){` |
|   583 | 1105 | `		ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|   291 | 1106 | `	}` |
|   583 | 1107 | `	ReflectMapAddBool(pCtx, pInfo, "variadic", bVariadic);` |
|     - | 1108 | `	/* Static variables: current value when the slot was initialized (first` |
|     - | 1109 | `	 * call), otherwise the evaluated default — PHP's getStaticVariables` |
|     - | 1110 | `	 * initializes on demand and reports the same values. */` |
|   583 | 1111 | `	pStatics = ph7_context_new_array(pCtx);` |
|   583 | 1112 | `	if( pStatics ){` |
|   583 | 1113 | `		ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|   611 | 1114 | `		for( n = 0 ; n < SySetUsed(&pFunc->aStatic) ; n++ ){` |
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
|   583 | 1134 | `		ph7_array_add_strkey_elem(pInfo, "statics", pStatics);` |
|   291 | 1135 | `	}` |
|   583 | 1136 | `}` |
|     - | 1137 | `/*` |
|     - | 1138 | ` * array\|null __reflect_func_info(string\|Closure $target [, string $method])` |
|     - | 1139 | ` * Function/method/closure descriptor for the PHP layer.` |
|     - | 1140 | ` */` |
|   714 | 1141 | `static int vm_builtin_reflect_func_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 1142 | `{` |
|     - | 1143 | `	ph7_vm_func *pFunc;` |
|   716 | 1144 | `	ph7_class *pClass = 0;` |
|   716 | 1145 | `	ph7_class_method *pMeth = 0;` |
|   716 | 1146 | `	ph7_user_func *pHost = 0;` |
|   716 | 1147 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1148 | `	ph7_value *pInfo;` |
|   716 | 1149 | `	if( nArg < 1 ){` |
|   ! 0 | 1150 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1151 | `		return PH7_OK;` |
|     - | 1152 | `	}` |
|   716 | 1153 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], nArg > 1 ? apArg[1] : 0,` |
|     - | 1154 | `		&pClass, &pMeth, &pHost, &pClosure);` |
|   716 | 1155 | `	if( pFunc == 0 && pHost == 0 ){` |
|     6 | 1156 | `		ph7_result_null(pCtx);` |
|     6 | 1157 | `		return PH7_OK;` |
|     - | 1158 | `	}` |
|   712 | 1159 | `	pInfo = ph7_context_new_array(pCtx);` |
|   712 | 1160 | `	if( pInfo == 0 ){` |
|   ! 0 | 1161 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1162 | `		return PH7_OK;` |
|     - | 1163 | `	}` |
|   712 | 1164 | `	if( pFunc == 0 ){` |
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
|   583 | 1203 | `	ReflectFillFuncCommon(pCtx, pInfo, pFunc);` |
|   583 | 1204 | `	ReflectMapAddInt(pCtx, pInfo, "minarg", -1);` |
|   583 | 1205 | `	if( (pFunc->iFlags & VM_FUNC_INTERNAL) && SySetUsed(&pFunc->aArgs) == 0 && pMeth == 0 ){` |
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
|   583 | 1217 | `	if( pMeth && pClass ){` |
|   271 | 1218 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|   271 | 1219 | `		ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   271 | 1220 | `		ReflectMapAddStr(pCtx, pInfo, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   271 | 1221 | `		ReflectMapAddInt(pCtx, pInfo, "vis", (sxi64)pMeth->iProtection);` |
|   271 | 1222 | `		ReflectMapAddBool(pCtx, pInfo, "mstatic", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   271 | 1223 | `		ReflectMapAddBool(pCtx, pInfo, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|   271 | 1224 | `		ReflectMapAddBool(pCtx, pInfo, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   135 | 1225 | `	}` |
|   583 | 1226 | `	if( pClosure ){` |
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
|   583 | 1273 | `	ph7_result_value(pCtx, pInfo);` |
|   583 | 1274 | `	return PH7_OK;` |
|   359 | 1275 | `}` |
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
|    52 | 1647 | `static int vm_builtin_reflect_attr_args(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1648 | `{` |
|    53 | 1649 | `	ph7_vm *pVm = pCtx->pVm;` |
|    53 | 1650 | `	SySet *pAttrs = 0;` |
|     - | 1651 | `	ph7_attribute *pAttrRec;` |
|     - | 1652 | `	ph7_value *pOut;` |
|     - | 1653 | `	const char *zKind;` |
|     - | 1654 | `	int nKind;` |
|     - | 1655 | `	sxu32 nAttrIdx, n;` |
|    53 | 1656 | `	if( nArg < 5 ){` |
|   ! 0 | 1657 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1658 | `		return PH7_OK;` |
|     - | 1659 | `	}` |
|    53 | 1660 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|    53 | 1661 | `	nAttrIdx = (sxu32)ph7_value_to_int(apArg[4]);` |
|    70 | 1662 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|    35 | 1663 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|    35 | 1664 | `		if( pClass ){ pAttrs = &pClass->aAttrs; }` |
|    38 | 1665 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|     5 | 1666 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|     5 | 1667 | `		ph7_class_attr *pMember = pClass ? ReflectFetchAttr(pClass, apArg[2]) : 0;` |
|     5 | 1668 | `		if( pMember ){ pAttrs = &pMember->aAttrs; }` |
|    18 | 1669 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|     3 | 1670 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1671 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|    18 | 1672 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|     9 | 1673 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], 0, 0, 0, 0, 0);` |
|     9 | 1674 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|    10 | 1675 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|     3 | 1676 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1677 | `		ph7_vm_func_arg *pParam = pFunc` |
|     2 | 1678 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|     3 | 1679 | `		if( pParam ){ pAttrs = &pParam->aAttrs; }` |
|     4 | 1680 | `	}else if( nKind == 5 && SyMemcmp(zKind, "const", 5) == 0 ){` |
|     - | 1681 | ``		/* Global constant (php 8.5 attributes on `const` statements) */`` |
|     - | 1682 | `		const char *zCName;` |
|     - | 1683 | `		int nCName;` |
|     - | 1684 | `		SyHashEntry *pCEntry;` |
|     3 | 1685 | `		zCName = ph7_value_to_string(apArg[1], &nCName);` |
|     3 | 1686 | `		pCEntry = nCName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zCName, (sxu32)nCName) : 0;` |
|     3 | 1687 | `		if( pCEntry ){ pAttrs = &((ph7_constant *)pCEntry->pUserData)->aAttrs; }` |
|     1 | 1688 | `	}` |
|    52 | 1689 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|    53 | 1690 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 | 1691 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1692 | `		return PH7_OK;` |
|     - | 1693 | `	}` |
|    99 | 1694 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|    47 | 1695 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|     - | 1696 | `		ph7_value sValue;` |
|    47 | 1697 | `		PH7_MemObjInit(pVm, &sValue);` |
|    47 | 1698 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|    47 | 1699 | `			VmLocalExec(pVm, &pArgRec->aByteCode, &sValue, FALSE);` |
|    23 | 1700 | `		}` |
|    47 | 1701 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|     5 | 1702 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|     3 | 1703 | `		}else{` |
|    43 | 1704 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|     - | 1705 | `		}` |
|    47 | 1706 | `		PH7_MemObjRelease(&sValue);` |
|    24 | 1707 | `	}` |
|    53 | 1708 | `	ph7_result_value(pCtx, pOut);` |
|    53 | 1709 | `	return PH7_OK;` |
|    27 | 1710 | `}` |
|     - | 1711 | `/*` |
|     - | 1712 | ` * The Reflection classes, in PHP. Chunk 1: exceptions, Reflector,` |
|     - | 1713 | ` * Reflection, ReflectionClass, ReflectionObject (plus get_debug_type,` |
|     - | 1714 | ` * which the TypeError messages need and PHP 8.0 ships natively).` |
|     - | 1715 | ` */` |
|     - | 1716 | `static const char zReflectLib1[] =` |
|     - | 1717 | `/* Per-class memoization of the (expensive) C class descriptor. Every Reflection*` |
|     - | 1718 | ` * accessor funnels through __rinfo()/the constructors, so without this a single` |
|     - | 1719 | ` * test run rebuilds the full descriptor of the same class hundreds of times` |
|     - | 1720 | ` * (real php caches the reflected class). Keyed by resolved class name; only` |
|     - | 1721 | ` * successful lookups are cached, so a not-yet-autoloaded class is re-queried.` |
|     - | 1722 | ` * The C builtin __reflect_class_info stays the real worker. */` |
|     - | 1723 | `"function __phl_rcinfo($oc){"` |
|     - | 1724 | `" static $c = array();"` |
|     - | 1725 | `" $k = is_object($oc) ? get_class($oc) : (string)$oc;"` |
|     - | 1726 | `" if( isset($c[$k]) ){ return $c[$k]; }"` |
|     - | 1727 | `" $info = __reflect_class_info($oc);"` |
|     - | 1728 | `" if( $info !== null ){ $c[$k] = $info; }"` |
|     - | 1729 | `" return $info;"` |
|     - | 1730 | `"}"` |
|     - | 1731 | `"function get_debug_type($value){"` |
|     - | 1732 | `" if(is_object($value)){ return get_class($value); }"` |
|     - | 1733 | `" if(is_bool($value)){ return 'bool'; }"` |
|     - | 1734 | `" if(is_int($value)){ return 'int'; }"` |
|     - | 1735 | `" if(is_float($value)){ return 'float'; }"` |
|     - | 1736 | `" if(is_string($value)){ return 'string'; }"` |
|     - | 1737 | `" if(is_array($value)){ return 'array'; }"` |
|     - | 1738 | `" if($value === null){ return 'null'; }"` |
|     - | 1739 | `" return gettype($value);"` |
|     - | 1740 | `"}"` |
|     - | 1741 | `"interface Reflector extends Stringable {}"` |
|     - | 1742 | `"class ReflectionException extends Exception {}"` |
|     - | 1743 | `"class Reflection {"` |
|     - | 1744 | `" public static function getModifierNames($modifiers){"` |
|     - | 1745 | `"  $names = array();"` |
|     - | 1746 | `"  if($modifiers & 64){ $names[] = 'abstract'; }"` |
|     - | 1747 | `"  if($modifiers & 32){ $names[] = 'final'; }"` |
|     - | 1748 | `"  if($modifiers & 1){ $names[] = 'public'; }"` |
|     - | 1749 | `"  if($modifiers & 2){ $names[] = 'protected'; }"` |
|     - | 1750 | `"  if($modifiers & 4){ $names[] = 'private'; }"` |
|     - | 1751 | `"  if($modifiers & 16){ $names[] = 'static'; }"` |
|     - | 1752 | `"  if($modifiers & 128){ $names[] = 'readonly'; }"` |
|     - | 1753 | `"  return $names;"` |
|     - | 1754 | `" }"` |
|     - | 1755 | `"}"` |
|     - | 1756 | `"class ReflectionClass implements Reflector {"` |
|     - | 1757 | `" const IS_IMPLICIT_ABSTRACT = 16;"` |
|     - | 1758 | `" const IS_EXPLICIT_ABSTRACT = 64;"` |
|     - | 1759 | `" const IS_FINAL = 32;"` |
|     - | 1760 | `" const IS_READONLY = 65536;"` |
|     - | 1761 | `" const SKIP_INITIALIZATION_ON_SERIALIZE = 8;"` |
|     - | 1762 | `" const SKIP_DESTRUCTOR = 16;"` |
|     - | 1763 | `" public $name;"` |
|     - | 1764 | `" protected $__obj = null;"` |
|     - | 1765 | `" public function __construct($objectOrClass){"` |
|     - | 1766 | `"  if(!is_object($objectOrClass) && !is_string($objectOrClass)){"` |
|     - | 1767 | `"   if(is_int($objectOrClass) \|\| is_float($objectOrClass) \|\| is_bool($objectOrClass)){"` |
|     - | 1768 | `"    $objectOrClass = (string)$objectOrClass;"` |
|     - | 1769 | `"   }else{"` |
|     - | 1770 | `"    throw new TypeError('ReflectionClass::__construct(): Argument #1 ($objectOrClass) must be of type object\|string, '.get_debug_type($objectOrClass).' given');"` |
|     - | 1771 | `"   }"` |
|     - | 1772 | `"  }"` |
|     - | 1773 | `"  $info = __phl_rcinfo($objectOrClass);"` |
|     - | 1774 | `"  if($info === null){"` |
|     - | 1775 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 1776 | `"  }"` |
|     - | 1777 | `"  $this->name = $info['name'];"` |
|     - | 1778 | `" }"` |
|     - | 1779 | `" protected function __rinfo(){ return __phl_rcinfo($this->name); }"` |
|     - | 1780 | `" public function getName(){ return $this->name; }"` |
|     - | 1781 | `" public function getShortName(){"` |
|     - | 1782 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1783 | `"  if($p === false){ return $this->name; }"` |
|     - | 1784 | `"  return substr($this->name,$p+1);"` |
|     - | 1785 | `" }"` |
|     - | 1786 | `" public function getNamespaceName(){"` |
|     - | 1787 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1788 | `"  if($p === false){ return ''; }"` |
|     - | 1789 | `"  return substr($this->name,0,$p);"` |
|     - | 1790 | `" }"` |
|     - | 1791 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 1792 | `" public function isInternal(){ $i = $this->__rinfo(); return $i['internal']; }"` |
|     - | 1793 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 1794 | `" public function isInterface(){ $i = $this->__rinfo(); return $i['interface']; }"` |
|     - | 1795 | `" public function isTrait(){ $i = $this->__rinfo(); return $i['trait']; }"` |
|     - | 1796 | `" public function isAbstract(){ $i = $this->__rinfo(); return $i['abstract']; }"` |
|     - | 1797 | `" public function isFinal(){ $i = $this->__rinfo(); return $i['final']; }"` |
|     - | 1798 | `" public function isReadOnly(){ $i = $this->__rinfo(); return $i['readonly']; }"` |
|     - | 1799 | `" public function isEnum(){ $i = $this->__rinfo(); return $i['enum']; }"` |
|     - | 1800 | `" public function isAnonymous(){ return strpos($this->name,'class@anonymous') === 0; }"` |
|     - | 1801 | `" public function getModifiers(){"` |
|     - | 1802 | `"  $i = $this->__rinfo();"` |
|     - | 1803 | `"  $m = 0;"` |
|     - | 1804 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 1805 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 1806 | `"  if($i['readonly']){ $m \|= 65536; }"` |
|     - | 1807 | `"  return $m;"` |
|     - | 1808 | `" }"` |
|     - | 1809 | `" public function getParentClass(){"` |
|     - | 1810 | `"  $i = $this->__rinfo();"` |
|     - | 1811 | `"  if($i['parent'] === null){ return false; }"` |
|     - | 1812 | `"  return new ReflectionClass($i['parent']);"` |
|     - | 1813 | `" }"` |
|     - | 1814 | `" public function getInterfaceNames(){ $i = $this->__rinfo(); return $i['interfaces']; }"` |
|     - | 1815 | `" public function getInterfaces(){"` |
|     - | 1816 | `"  $i = $this->__rinfo();"` |
|     - | 1817 | `"  $out = array();"` |
|     - | 1818 | `"  foreach($i['interfaces'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1819 | `"  return $out;"` |
|     - | 1820 | `" }"` |
|     - | 1821 | `" public function getTraitNames(){ $i = $this->__rinfo(); return $i['traits']; }"` |
|     - | 1822 | `" public function getTraits(){"` |
|     - | 1823 | `"  $i = $this->__rinfo();"` |
|     - | 1824 | `"  $out = array();"` |
|     - | 1825 | `"  foreach($i['traits'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1826 | `"  return $out;"` |
|     - | 1827 | `" }"` |
|     - | 1828 | `" public function getTraitAliases(){ return array(); }"` |
|     - | 1829 | `" public function implementsInterface($interface){"` |
|     - | 1830 | `"  if($interface instanceof ReflectionClass){ $interface = $interface->name; }"` |
|     - | 1831 | `"  $target = __phl_rcinfo($interface);"` |
|     - | 1832 | `"  if($target === null){"` |
|     - | 1833 | `"   throw new ReflectionException('Interface \"'.$interface.'\" does not exist');"` |
|     - | 1834 | `"  }"` |
|     - | 1835 | `"  if(!$target['interface']){"` |
|     - | 1836 | `"   throw new ReflectionException($target['name'].' is not an interface');"` |
|     - | 1837 | `"  }"` |
|     - | 1838 | `"  $name = $target['name'];"` |
|     - | 1839 | `"  if($this->name === $name){ return true; }"` |
|     - | 1840 | `"  $i = $this->__rinfo();"` |
|     - | 1841 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1842 | `"  return false;"` |
|     - | 1843 | `" }"` |
|     - | 1844 | `" public function isSubclassOf($class){"` |
|     - | 1845 | `"  if($class instanceof ReflectionClass){ $class = $class->name; }"` |
|     - | 1846 | `"  $target = __phl_rcinfo($class);"` |
|     - | 1847 | `"  if($target === null){"` |
|     - | 1848 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 1849 | `"  }"` |
|     - | 1850 | `"  $name = $target['name'];"` |
|     - | 1851 | `"  if($name === $this->name){ return false; }"` |
|     - | 1852 | `"  $i = $this->__rinfo();"` |
|     - | 1853 | `"  $p = $i['parent'];"` |
|     - | 1854 | `"  while($p !== null){"` |
|     - | 1855 | `"   if($p === $name){ return true; }"` |
|     - | 1856 | `"   $pi = __phl_rcinfo($p);"` |
|     - | 1857 | `"   $p = $pi['parent'];"` |
|     - | 1858 | `"  }"` |
|     - | 1859 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1860 | `"  return false;"` |
|     - | 1861 | `" }"` |
|     - | 1862 | `" public function isInstance($object){"` |
|     - | 1863 | `"  if(!is_object($object)){"` |
|     - | 1864 | `"   throw new TypeError('ReflectionClass::isInstance(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 1865 | `"  }"` |
|     - | 1866 | `"  return is_a($object,$this->name);"` |
|     - | 1867 | `" }"` |
|     - | 1868 | `" public function hasMethod($name){"` |
|     - | 1869 | `"  $i = $this->__rinfo();"` |
|     - | 1870 | `"  $l = strtolower($name);"` |
|     - | 1871 | `"  foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ return true; } }"` |
|     - | 1872 | `"  return false;"` |
|     - | 1873 | `" }"` |
|     - | 1874 | `" public function hasProperty($name){"` |
|     - | 1875 | `"  $i = $this->__rinfo();"` |
|     - | 1876 | `"  if(isset($i['props'][$name])){ return true; }"` |
|     - | 1877 | `"  if($this->__obj !== null){ return (__reflect_prop_state($this->__obj, $name) & 1) !== 0; }"` |
|     - | 1878 | `"  return false;"` |
|     - | 1879 | `" }"` |
|     - | 1880 | `" public function hasConstant($name){ $i = $this->__rinfo(); return isset($i['consts'][$name]); }"` |
|     - | 1881 | `" public function getConstant($name){"` |
|     - | 1882 | `"  $i = $this->__rinfo();"` |
|     - | 1883 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 1884 | `"  return __reflect_const_value($this->name,$name);"` |
|     - | 1885 | `" }"` |
|     - | 1886 | `" public function getConstants($filter = null){"` |
|     - | 1887 | `"  $i = $this->__rinfo();"` |
|     - | 1888 | `"  $out = array();"` |
|     - | 1889 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 1890 | `"   if($filter !== null){"` |
|     - | 1891 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 1892 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 1893 | `"   }"` |
|     - | 1894 | `"   $out[$k] = __reflect_const_value($this->name,$k);"` |
|     - | 1895 | `"  }"` |
|     - | 1896 | `"  return $out;"` |
|     - | 1897 | `" }"` |
|     - | 1898 | `" public function getStartLine(){"` |
|     - | 1899 | `"  $i = $this->__rinfo();"` |
|     - | 1900 | `"  if($i['internal']){ return false; }"` |
|     - | 1901 | `"  return $i['line'];"` |
|     - | 1902 | `" }"` |
|     - | 1903 | `" public function getEndLine(){"` |
|     - | 1904 | `"  $i = $this->__rinfo();"` |
|     - | 1905 | `"  if($i['internal']){ return false; }"` |
|     - | 1906 | `"  return $i['endline'];"` |
|     - | 1907 | `" }"` |
|     - | 1908 | `" public function getFileName(){ $i = $this->__rinfo(); return $i['file']; }"` |
|     - | 1909 | `" public function getDocComment(){ $i = $this->__rinfo(); return $i['doc']; }"` |
|     - | 1910 | `" public function isInstantiable(){"` |
|     - | 1911 | `"  $i = $this->__rinfo();"` |
|     - | 1912 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract'] \|\| $i['enum']){ return false; }"` |
|     - | 1913 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){ return false; }"` |
|     - | 1914 | `"  return true;"` |
|     - | 1915 | `" }"` |
|     - | 1916 | `" public function isCloneable(){"` |
|     - | 1917 | `"  $i = $this->__rinfo();"` |
|     - | 1918 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1919 | `"  if($i['clonevis'] !== 0 && $i['clonevis'] !== 1){ return false; }"` |
|     - | 1920 | `"  return true;"` |
|     - | 1921 | `" }"` |
|     - | 1922 | `" public function isIterable(){"` |
|     - | 1923 | `"  $i = $this->__rinfo();"` |
|     - | 1924 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1925 | `"  return $i['iterable'];"` |
|     - | 1926 | `" }"` |
|     - | 1927 | `" public function isIterateable(){ return $this->isIterable(); }"` |
|     - | 1928 | `" public function newInstance(...$args){ return $this->__rnew($args); }"` |
|     - | 1929 | `" public function newInstanceArgs(array $args = array()){ return $this->__rnew($args); }"` |
|     - | 1930 | `" protected function __rnew($args){"` |
|     - | 1931 | `"  $i = $this->__rinfo();"` |
|     - | 1932 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1933 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){"` |
|     - | 1934 | `"   throw new ReflectionException('Access to non-public constructor of class '.$this->name);"` |
|     - | 1935 | `"  }"` |
|     - | 1936 | `"  if($i['ctorvis'] === 0 && count($args) > 0){"` |
|     - | 1937 | `"   throw new ReflectionException('Class '.$this->name.' does not have a constructor, so you cannot pass any constructor arguments');"` |
|     - | 1938 | `"  }"` |
|     - | 1939 | `"  return __reflect_new_instance($this->name,$args);"` |
|     - | 1940 | `" }"` |
|     - | 1941 | `" protected function __rcheckInstantiable($i){"` |
|     - | 1942 | `"  if($i['interface']){ throw new Error('Cannot instantiate interface '.$this->name); }"` |
|     - | 1943 | `"  if($i['trait']){ throw new Error('Cannot instantiate trait '.$this->name); }"` |
|     - | 1944 | `"  if($i['abstract']){ throw new Error('Cannot instantiate abstract class '.$this->name); }"` |
|     - | 1945 | `" }"` |
|     - | 1946 | `" public function newInstanceWithoutConstructor(){"` |
|     - | 1947 | `"  $i = $this->__rinfo();"` |
|     - | 1948 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1949 | `"  return __reflect_new_no_ctor($this->name);"` |
|     - | 1950 | `" }"` |
|     - | 1951 | `" public function getStaticProperties(){"` |
|     - | 1952 | `"  $i = $this->__rinfo();"` |
|     - | 1953 | `"  $out = array();"` |
|     - | 1954 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1955 | `"   if($p['static']){ $out[$k] = __reflect_static_value($this->name,$k); }"` |
|     - | 1956 | `"  }"` |
|     - | 1957 | `"  return $out;"` |
|     - | 1958 | `" }"` |
|     - | 1959 | `" public function getStaticPropertyValue($name, ...$def){"` |
|     - | 1960 | `"  $i = $this->__rinfo();"` |
|     - | 1961 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 1962 | `"   if(count($def) > 0){ return $def[0]; }"` |
|     - | 1963 | `"   throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 1964 | `"  }"` |
|     - | 1965 | `"  return __reflect_static_value($this->name,$name);"` |
|     - | 1966 | `" }"` |
|     - | 1967 | `" public function setStaticPropertyValue($name,$value){"` |
|     - | 1968 | `"  $i = $this->__rinfo();"` |
|     - | 1969 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 1970 | `"   throw new ReflectionException('Class '.$this->name.' does not have a property named '.$name);"` |
|     - | 1971 | `"  }"` |
|     - | 1972 | `"  __reflect_static_set($this->name,$name,$value);"` |
|     - | 1973 | `" }"` |
|     - | 1974 | `" public function getDefaultProperties(){"` |
|     - | 1975 | `"  $i = $this->__rinfo();"` |
|     - | 1976 | `"  $out = array();"` |
|     - | 1977 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1978 | `"   if($p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 1979 | `"  }"` |
|     - | 1980 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1981 | `"   if(!$p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 1982 | `"  }"` |
|     - | 1983 | `"  return $out;"` |
|     - | 1984 | `" }"` |
|     - | 1985 | `" public function getProperty($name){"` |
|     - | 1986 | `"  $i = $this->__rinfo();"` |
|     - | 1987 | `"  if(isset($i['props'][$name])){"` |
|     - | 1988 | `"   return new ReflectionProperty($this->name, $name);"` |
|     - | 1989 | `"  }"` |
|     - | 1990 | `"  if($this->__obj !== null && (__reflect_prop_state($this->__obj, $name) & 1)){"` |
|     - | 1991 | `"   return new ReflectionProperty($this->__obj, $name);"` |
|     - | 1992 | `"  }"` |
|     - | 1993 | `"  throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 1994 | `" }"` |
|     - | 1995 | `" public function getProperties($filter = null){"` |
|     - | 1996 | `"  $i = $this->__rinfo();"` |
|     - | 1997 | `"  $out = array();"` |
|     - | 1998 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1999 | `"   if($filter !== null){"` |
|     - | 2000 | `"    $m = ($p['vis'] === 1 ? 1 : ($p['vis'] === 2 ? 2 : 4));"` |
|     - | 2001 | `"    if($p['static']){ $m \|= 16; }"` |
|     - | 2002 | `"    if($p['readonly']){ $m \|= 128; }"` |
|     - | 2003 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 2004 | `"   }"` |
|     - | 2005 | `"   $out[] = new ReflectionProperty($this->name, $k);"` |
|     - | 2006 | `"  }"` |
|     - | 2007 | `"  if($this->__obj !== null){"` |
|     - | 2008 | `"   foreach(__reflect_dyn_props($this->__obj) as $k){"` |
|     - | 2009 | `"    if(isset($i['props'][$k])){ continue; }"` |
|     - | 2010 | `"    if($filter !== null && ($filter & 1) === 0){ continue; }"` |
|     - | 2011 | `"    $out[] = new ReflectionProperty($this->__obj, $k);"` |
|     - | 2012 | `"   }"` |
|     - | 2013 | `"  }"` |
|     - | 2014 | `"  return $out;"` |
|     - | 2015 | `" }"` |
|     - | 2016 | `" public function getMethod($name){"` |
|     - | 2017 | `"  $i = $this->__rinfo();"` |
|     - | 2018 | `"  $found = null;"` |
|     - | 2019 | `"  if(isset($i['methods'][$name])){"` |
|     - | 2020 | `"   $found = $name;"` |
|     - | 2021 | `"  }else{"` |
|     - | 2022 | `"   $l = strtolower($name);"` |
|     - | 2023 | `"   foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ $found = $k; break; } }"` |
|     - | 2024 | `"  }"` |
|     - | 2025 | `"  if($found === null){"` |
|     - | 2026 | `"   throw new ReflectionException('Method '.$this->name.'::'.$name.'() does not exist');"` |
|     - | 2027 | `"  }"` |
|     - | 2028 | `"  return new ReflectionMethod($this->name, $found);"` |
|     - | 2029 | `" }"` |
|     - | 2030 | `" public function getMethods($filter = null){"` |
|     - | 2031 | `"  $i = $this->__rinfo();"` |
|     - | 2032 | `"  $out = array();"` |
|     - | 2033 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 2034 | `"   if($filter !== null){"` |
|     - | 2035 | `"    $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2036 | `"    if($m['static']){ $mod \|= 16; }"` |
|     - | 2037 | `"    if($m['abstract']){ $mod \|= 64; }"` |
|     - | 2038 | `"    if($m['final']){ $mod \|= 32; }"` |
|     - | 2039 | `"    if(($mod & $filter) === 0){ continue; }"` |
|     - | 2040 | `"   }"` |
|     - | 2041 | `"   $out[] = new ReflectionMethod($this->name, $k);"` |
|     - | 2042 | `"  }"` |
|     - | 2043 | `"  return $out;"` |
|     - | 2044 | `" }"` |
|     - | 2045 | `" public function getConstructor(){"` |
|     - | 2046 | `"  $i = $this->__rinfo();"` |
|     - | 2047 | `"  if(isset($i['methods']['__construct'])){"` |
|     - | 2048 | `"   return new ReflectionMethod($this->name, '__construct');"` |
|     - | 2049 | `"  }"` |
|     - | 2050 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 2051 | `"   if(strtolower($k) === '__construct'){ return new ReflectionMethod($this->name, $k); }"` |
|     - | 2052 | `"  }"` |
|     - | 2053 | `"  if($i['ctorvis'] !== 0 && isset($i['methods'][$this->name])){"` |
|     - | 2054 | `"   return new ReflectionMethod($this->name, $this->name);"` |
|     - | 2055 | `"  }"` |
|     - | 2056 | `"  return null;"` |
|     - | 2057 | `" }"` |
|     - | 2058 | `" public function getReflectionConstant($name){"` |
|     - | 2059 | `"  $i = $this->__rinfo();"` |
|     - | 2060 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 2061 | `"  return new ReflectionClassConstant($this->name, $name);"` |
|     - | 2062 | `" }"` |
|     - | 2063 | `" public function getReflectionConstants($filter = null){"` |
|     - | 2064 | `"  $i = $this->__rinfo();"` |
|     - | 2065 | `"  $out = array();"` |
|     - | 2066 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 2067 | `"   if($filter !== null){"` |
|     - | 2068 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 2069 | `"    if($c['final']){ $m \|= 32; }"` |
|     - | 2070 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 2071 | `"   }"` |
|     - | 2072 | `"   $out[] = new ReflectionClassConstant($this->name, $k);"` |
|     - | 2073 | `"  }"` |
|     - | 2074 | `"  return $out;"` |
|     - | 2075 | `" }"` |
|     - | 2076 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2077 | `"  $i = $this->__rinfo();"` |
|     - | 2078 | `"  return __reflect_build_attrs($i['attrs'], array('class', $this->name, null, 0), 1, $name, $flags);"` |
|     - | 2079 | `" }"` |
|     - | 2080 | `" public function getExtensionName(){ $i = $this->__rinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2081 | `" public function getExtension(){ $i = $this->__rinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2082 | `" public function newLazyGhost($initializer, $options = 0){"` |
|     - | 2083 | `"  throw new Error('ReflectionClass::newLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2084 | `" }"` |
|     - | 2085 | `" public function newLazyProxy($factory, $options = 0){"` |
|     - | 2086 | `"  throw new Error('ReflectionClass::newLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2087 | `" }"` |
|     - | 2088 | `" public function resetAsLazyGhost($object, $initializer, $options = 0){"` |
|     - | 2089 | `"  throw new Error('ReflectionClass::resetAsLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2090 | `" }"` |
|     - | 2091 | `" public function resetAsLazyProxy($object, $factory, $options = 0){"` |
|     - | 2092 | `"  throw new Error('ReflectionClass::resetAsLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2093 | `" }"` |
|     - | 2094 | `" public function getLazyInitializer($object){ return null; }"` |
|     - | 2095 | `" public function initializeLazyObject($object){ return $object; }"` |
|     - | 2096 | `" public function markLazyObjectAsInitialized($object){ return $object; }"` |
|     - | 2097 | `" public function isUninitializedLazyObject($object){ return false; }"` |
|     - | 2098 | `" public function __toString(){ return __reflect_export_class($this); }"` |
|     - | 2099 | `"}"` |
|     - | 2100 | `"class ReflectionObject extends ReflectionClass {"` |
|     - | 2101 | `" public function __construct($object){"` |
|     - | 2102 | `"  if(!is_object($object)){"` |
|     - | 2103 | `"   throw new TypeError('ReflectionObject::__construct(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 2104 | `"  }"` |
|     - | 2105 | `"  parent::__construct($object);"` |
|     - | 2106 | `"  $this->__obj = $object;"` |
|     - | 2107 | `" }"` |
|     - | 2108 | `"}"` |
|     - | 2109 | `;` |
|     - | 2110 | `/*` |
|     - | 2111 | ` * Chunk 2: ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,` |
|     - | 2112 | ` * ReflectionParameter.` |
|     - | 2113 | ` */` |
|     - | 2114 | `static const char zReflectLib2[] =` |
|     - | 2115 | `"abstract class ReflectionFunctionAbstract implements Reflector {"` |
|     - | 2116 | `" public $name;"` |
|     - | 2117 | `" protected $__cl = null;"` |
|     - | 2118 | `" protected function __rfinfo(){"` |
|     - | 2119 | `"  if($this->__cl !== null){ return __reflect_sig_fixup(__reflect_func_info($this->__cl)); }"` |
|     - | 2120 | `"  return __reflect_sig_fixup(__reflect_func_info($this->name));"` |
|     - | 2121 | `" }"` |
|     - | 2122 | `" protected function __rftarget(){ return $this->__cl !== null ? $this->__cl : $this->name; }"` |
|     - | 2123 | `" protected function __rpspec(){ return $this->__rftarget(); }"` |
|     - | 2124 | `" public function getName(){ return $this->name; }"` |
|     - | 2125 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 2126 | `" public function getNamespaceName(){"` |
|     - | 2127 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2128 | `"  if($p === false){ return ''; }"` |
|     - | 2129 | `"  return substr($this->name,0,$p);"` |
|     - | 2130 | `" }"` |
|     - | 2131 | `" public function getShortName(){"` |
|     - | 2132 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2133 | `"  if($p === false){ return $this->name; }"` |
|     - | 2134 | `"  return substr($this->name,$p+1);"` |
|     - | 2135 | `" }"` |
|     - | 2136 | `" public function isClosure(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2137 | `" public function isGenerator(){ $i = $this->__rfinfo(); return $i['generator']; }"` |
|     - | 2138 | `" public function isVariadic(){ $i = $this->__rfinfo(); return $i['variadic']; }"` |
|     - | 2139 | `" public function returnsReference(){ $i = $this->__rfinfo(); return $i['byref']; }"` |
|     - | 2140 | `" public function isInternal(){ $i = $this->__rfinfo(); return $i['internal']; }"` |
|     - | 2141 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 2142 | `" public function isDeprecated(){ $i = $this->__rfinfo(); return __reflect_has_deprecated($i['attrs']); }"` |
|     - | 2143 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['fstatic']; }"` |
|     - | 2144 | `" public function getFileName(){ $i = $this->__rfinfo(); return $i['file']; }"` |
|     - | 2145 | `" public function getStartLine(){"` |
|     - | 2146 | `"  $i = $this->__rfinfo();"` |
|     - | 2147 | `"  if($i['internal']){ return false; }"` |
|     - | 2148 | `"  return $i['line'];"` |
|     - | 2149 | `" }"` |
|     - | 2150 | `" public function getEndLine(){"` |
|     - | 2151 | `"  $i = $this->__rfinfo();"` |
|     - | 2152 | `"  if($i['internal']){ return false; }"` |
|     - | 2153 | `"  return $i['endline'];"` |
|     - | 2154 | `" }"` |
|     - | 2155 | `" public function getDocComment(){ $i = $this->__rfinfo(); return $i['doc']; }"` |
|     - | 2156 | `" public function hasReturnType(){ $i = $this->__rfinfo(); return $i['rettext'] !== null; }"` |
|     - | 2157 | `" public function getReturnType(){ $i = $this->__rfinfo(); return __reflect_make_type($i['rettext']); }"` |
|     - | 2158 | `" public function hasTentativeReturnType(){ return false; }"` |
|     - | 2159 | `" public function getTentativeReturnType(){ return null; }"` |
|     - | 2160 | `" public function getNumberOfParameters(){"` |
|     - | 2161 | `"  $i = $this->__rfinfo();"` |
|     - | 2162 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2163 | `"  return count($i['params']);"` |
|     - | 2164 | `" }"` |
|     - | 2165 | `" public function getNumberOfRequiredParameters(){"` |
|     - | 2166 | `"  $i = $this->__rfinfo();"` |
|     - | 2167 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2168 | `"  $req = 0;"` |
|     - | 2169 | `"  $n = count($i['params']);"` |
|     - | 2170 | `"  for($k = $n - 1; $k >= 0; $k--){"` |
|     - | 2171 | `"   $p = $i['params'][$k];"` |
|     - | 2172 | `"   if(!$p['variadic'] && !$p['hasdef']){ $req = $k + 1; break; }"` |
|     - | 2173 | `"  }"` |
|     - | 2174 | `"  return $req;"` |
|     - | 2175 | `" }"` |
|     - | 2176 | `" public function getParameters(){"` |
|     - | 2177 | `"  $i = $this->__rfinfo();"` |
|     - | 2178 | `"  $out = array();"` |
|     - | 2179 | `"  $spec = $this->__rpspec();"` |
|     - | 2180 | `"  foreach($i['params'] as $p){"` |
|     - | 2181 | `"   $out[] = new ReflectionParameter($spec, $p['pos']);"` |
|     - | 2182 | `"  }"` |
|     - | 2183 | `"  return $out;"` |
|     - | 2184 | `" }"` |
|     - | 2185 | `" public function getStaticVariables(){ $i = $this->__rfinfo(); return $i['statics']; }"` |
|     - | 2186 | `" public function getClosureThis(){"` |
|     - | 2187 | `"  $i = $this->__rfinfo();"` |
|     - | 2188 | `"  return isset($i['this']) ? $i['this'] : null;"` |
|     - | 2189 | `" }"` |
|     - | 2190 | `" public function getClosureScopeClass(){"` |
|     - | 2191 | `"  $i = $this->__rfinfo();"` |
|     - | 2192 | `"  if(isset($i['scope'])){ return new ReflectionClass($i['scope']); }"` |
|     - | 2193 | `"  if(isset($i['this'])){ return new ReflectionClass(get_class($i['this'])); }"` |
|     - | 2194 | `"  return null;"` |
|     - | 2195 | `" }"` |
|     - | 2196 | `" public function getClosureCalledClass(){ return $this->getClosureScopeClass(); }"` |
|     - | 2197 | `" public function getClosureUsedVariables(){"` |
|     - | 2198 | `"  $i = $this->__rfinfo();"` |
|     - | 2199 | `"  return isset($i['used']) ? $i['used'] : array();"` |
|     - | 2200 | `" }"` |
|     - | 2201 | `" public function getExtensionName(){ $i = $this->__rfinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2202 | `" public function getExtension(){ $i = $this->__rfinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2203 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2204 | `"  $i = $this->__rfinfo();"` |
|     - | 2205 | `"  if($this instanceof ReflectionMethod){"` |
|     - | 2206 | `"   $spec = array('method', $this->class, $this->name, 0);"` |
|     - | 2207 | `"   $target = 4;"` |
|     - | 2208 | `"  }else{"` |
|     - | 2209 | `"   $spec = array('fn', $this->__rftarget(), null, 0);"` |
|     - | 2210 | `"   $target = 2;"` |
|     - | 2211 | `"  }"` |
|     - | 2212 | `"  return __reflect_build_attrs($i['attrs'], $spec, $target, $name, $flags);"` |
|     - | 2213 | `" }"` |
|     - | 2214 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2215 | `"}"` |
|     - | 2216 | `"class ReflectionFunction extends ReflectionFunctionAbstract {"` |
|     - | 2217 | `" const IS_DEPRECATED = 2048;"` |
|     - | 2218 | `" public function __construct($function){"` |
|     - | 2219 | `"  if($function instanceof Closure){"` |
|     - | 2220 | `"   $this->__cl = $function;"` |
|     - | 2221 | `"   $i = $this->__rfinfo();"` |
|     - | 2222 | `"   if($i['closure']){"` |
|     - | 2223 | `"    $f = $i['file'] === false ? '' : $i['file'];"` |
|     - | 2224 | `"    $this->name = '{closure:'.$f.':'.$i['line'].'}';"` |
|     - | 2225 | `"   }else{"` |
|     - | 2226 | `"    $this->name = $i['name'];"` |
|     - | 2227 | `"   }"` |
|     - | 2228 | `"   return;"` |
|     - | 2229 | `"  }"` |
|     - | 2230 | `"  if(!is_string($function)){"` |
|     - | 2231 | `"   throw new TypeError('ReflectionFunction::__construct(): Argument #1 ($function) must be of type Closure\|string, '.get_debug_type($function).' given');"` |
|     - | 2232 | `"  }"` |
|     - | 2233 | `"  $i = __reflect_func_info($function);"` |
|     - | 2234 | `"  if($i === null){"` |
|     - | 2235 | `"   throw new ReflectionException('Function '.$function.'() does not exist');"` |
|     - | 2236 | `"  }"` |
|     - | 2237 | `"  if($i['closure']){"` |
|     - | 2238 | `"   $this->name = '{closure:'.($i['file'] === false ? '' : $i['file']).':'.$i['line'].'}';"` |
|     - | 2239 | `"   $this->__cl = __reflect_closure($function, null, null);"` |
|     - | 2240 | `"  }else{"` |
|     - | 2241 | `"   $this->name = $i['name'];"` |
|     - | 2242 | `"  }"` |
|     - | 2243 | `" }"` |
|     - | 2244 | `" public function invoke(...$args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2245 | `" public function invokeArgs(array $args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2246 | `" public function getClosure(){"` |
|     - | 2247 | `"  if($this->__cl !== null){ return $this->__cl; }"` |
|     - | 2248 | `"  return __reflect_closure($this->name, null, null);"` |
|     - | 2249 | `" }"` |
|     - | 2250 | `" public function isAnonymous(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2251 | `" public function isDisabled(){ return false; }"` |
|     - | 2252 | `"}"` |
|     - | 2253 | `"class ReflectionMethod extends ReflectionFunctionAbstract {"` |
|     - | 2254 | `" const IS_PUBLIC = 1;"` |
|     - | 2255 | `" const IS_PROTECTED = 2;"` |
|     - | 2256 | `" const IS_PRIVATE = 4;"` |
|     - | 2257 | `" const IS_STATIC = 16;"` |
|     - | 2258 | `" const IS_FINAL = 32;"` |
|     - | 2259 | `" const IS_ABSTRACT = 64;"` |
|     - | 2260 | `" public $class;"` |
|     - | 2261 | `" public function __construct($objectOrMethod, $method = null){"` |
|     - | 2262 | `"  if($method === null){"` |
|     - | 2263 | `"   if(!is_string($objectOrMethod) \|\| strpos($objectOrMethod,'::') === false){"` |
|     - | 2264 | `"    throw new TypeError('ReflectionMethod::__construct(): Argument #1 ($objectOrMethod) must be of type object\|string, '.get_debug_type($objectOrMethod).' given');"` |
|     - | 2265 | `"   }"` |
|     - | 2266 | `"   $p = strpos($objectOrMethod,'::');"` |
|     - | 2267 | `"   $method = substr($objectOrMethod,$p+2);"` |
|     - | 2268 | `"   $objectOrMethod = substr($objectOrMethod,0,$p);"` |
|     - | 2269 | `"  }"` |
|     - | 2270 | `"  $ci = __phl_rcinfo($objectOrMethod);"` |
|     - | 2271 | `"  if($ci === null){"` |
|     - | 2272 | `"   throw new ReflectionException('Class \"'.$objectOrMethod.'\" does not exist');"` |
|     - | 2273 | `"  }"` |
|     - | 2274 | `"  $this->class = $ci['name'];"` |
|     - | 2275 | `"  $found = null;"` |
|     - | 2276 | `"  if(isset($ci['methods'][$method])){"` |
|     - | 2277 | `"   $found = $method;"` |
|     - | 2278 | `"  }else{"` |
|     - | 2279 | `"   $l = strtolower($method);"` |
|     - | 2280 | `"   foreach($ci['methods'] as $k => $m){"` |
|     - | 2281 | `"    if(strtolower($k) === $l){ $found = $k; break; }"` |
|     - | 2282 | `"   }"` |
|     - | 2283 | `"  }"` |
|     - | 2284 | `"  if($found === null){"` |
|     - | 2285 | `"   throw new ReflectionException('Method '.$this->class.'::'.$method.'() does not exist');"` |
|     - | 2286 | `"  }"` |
|     - | 2287 | `"  $this->name = $found;"` |
|     - | 2288 | `" }"` |
|     - | 2289 | `" public static function createFromMethodName($name){"` |
|     - | 2290 | `"  return new ReflectionMethod($name);"` |
|     - | 2291 | `" }"` |
|     - | 2292 | `" protected function __rfinfo(){ return __reflect_func_info($this->class, $this->name); }"` |
|     - | 2293 | `" protected function __rpspec(){ return array($this->class, $this->name); }"` |
|     - | 2294 | `" public function getDeclaringClass(){"` |
|     - | 2295 | `"  $i = $this->__rfinfo();"` |
|     - | 2296 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2297 | `" }"` |
|     - | 2298 | `" public function getModifiers(){"` |
|     - | 2299 | `"  $i = $this->__rfinfo();"` |
|     - | 2300 | `"  $m = ($i['vis'] === 1 ? 1 : ($i['vis'] === 2 ? 2 : 4));"` |
|     - | 2301 | `"  if($i['mstatic']){ $m \|= 16; }"` |
|     - | 2302 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 2303 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 2304 | `"  return $m;"` |
|     - | 2305 | `" }"` |
|     - | 2306 | `" public function isPublic(){ $i = $this->__rfinfo(); return $i['vis'] === 1; }"` |
|     - | 2307 | `" public function isProtected(){ $i = $this->__rfinfo(); return $i['vis'] === 2; }"` |
|     - | 2308 | `" public function isPrivate(){ $i = $this->__rfinfo(); return $i['vis'] === 3; }"` |
|     - | 2309 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['mstatic']; }"` |
|     - | 2310 | `" public function isAbstract(){ $i = $this->__rfinfo(); return $i['abstract']; }"` |
|     - | 2311 | `" public function isFinal(){ $i = $this->__rfinfo(); return $i['final']; }"` |
|     - | 2312 | `" public function isConstructor(){ return strtolower($this->name) === '__construct'; }"` |
|     - | 2313 | `" public function isDestructor(){ return strtolower($this->name) === '__destruct'; }"` |
|     - | 2314 | `" public function invoke($object = null, ...$args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2315 | `" public function invokeArgs($object, array $args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2316 | `" protected function __rinvoke($object, $args){"` |
|     - | 2317 | `"  $i = $this->__rfinfo();"` |
|     - | 2318 | `"  if(!$i['mstatic']){"` |
|     - | 2319 | `"   if(!is_object($object)){"` |
|     - | 2320 | `"    throw new ReflectionException('Trying to invoke non static method '.$this->class.'::'.$this->name.'() without an object');"` |
|     - | 2321 | `"   }"` |
|     - | 2322 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2323 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2324 | `"   }"` |
|     - | 2325 | `"  }else{"` |
|     - | 2326 | `"   $object = null;"` |
|     - | 2327 | `"  }"` |
|     - | 2328 | `"  return __reflect_invoke($this->class, $this->name, $object, $args);"` |
|     - | 2329 | `" }"` |
|     - | 2330 | `" public function getClosure($object = null){"` |
|     - | 2331 | `"  $i = $this->__rfinfo();"` |
|     - | 2332 | `"  if(!$i['mstatic']){"` |
|     - | 2333 | `"   if($object === null){"` |
|     - | 2334 | `"    throw new ValueError('ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods');"` |
|     - | 2335 | `"   }"` |
|     - | 2336 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2337 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2338 | `"   }"` |
|     - | 2339 | `"  }else{"` |
|     - | 2340 | `"   $object = null;"` |
|     - | 2341 | `"  }"` |
|     - | 2342 | `"  return __reflect_closure($this->class, $this->name, $object);"` |
|     - | 2343 | `" }"` |
|     - | 2344 | `" public function setAccessible($accessible){ }"` |
|     - | 2345 | `" public function hasPrototype(){ return $this->__rproto() !== null; }"` |
|     - | 2346 | `" public function getPrototype(){"` |
|     - | 2347 | `"  $p = $this->__rproto();"` |
|     - | 2348 | `"  if($p === null){"` |
|     - | 2349 | `"   throw new ReflectionException('Method '.$this->class.'::'.$this->name.' does not have a prototype');"` |
|     - | 2350 | `"  }"` |
|     - | 2351 | `"  return new ReflectionMethod($p, $this->name);"` |
|     - | 2352 | `" }"` |
|     - | 2353 | `" protected function __rproto(){"` |
|     - | 2354 | `"  $ci = __phl_rcinfo($this->class);"` |
|     - | 2355 | `"  $l = strtolower($this->name);"` |
|     - | 2356 | `"  $p = $ci['parent'];"` |
|     - | 2357 | `"  while($p !== null){"` |
|     - | 2358 | `"   $pi = __phl_rcinfo($p);"` |
|     - | 2359 | `"   foreach($pi['methods'] as $k => $m){"` |
|     - | 2360 | `"    if(strtolower($k) === $l && $m['vis'] !== 3){ return $m['decl']; }"` |
|     - | 2361 | `"   }"` |
|     - | 2362 | `"   $p = $pi['parent'];"` |
|     - | 2363 | `"  }"` |
|     - | 2364 | `"  foreach($ci['interfaces'] as $if){"` |
|     - | 2365 | `"   $ii = __phl_rcinfo($if);"` |
|     - | 2366 | `"   foreach($ii['methods'] as $k => $m){"` |
|     - | 2367 | `"    if(strtolower($k) === $l){ return $ii['name']; }"` |
|     - | 2368 | `"   }"` |
|     - | 2369 | `"  }"` |
|     - | 2370 | `"  return null;"` |
|     - | 2371 | `" }"` |
|     - | 2372 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2373 | `"}"` |
|     - | 2374 | `"class ReflectionParameter implements Reflector {"` |
|     - | 2375 | `" public $name;"` |
|     - | 2376 | `" protected $__t;"` |
|     - | 2377 | `" protected $__m = null;"` |
|     - | 2378 | `" protected $__p = 0;"` |
|     - | 2379 | `" public function __construct($function, $param){"` |
|     - | 2380 | `"  $m = null;"` |
|     - | 2381 | `"  $t = $function;"` |
|     - | 2382 | `"  if(is_array($function)){"` |
|     - | 2383 | `"   $t = $function[0];"` |
|     - | 2384 | `"   $m = $function[1];"` |
|     - | 2385 | `"   if(is_object($t)){ $t = get_class($t); }"` |
|     - | 2386 | `"  }else if(is_string($function) && strpos($function,'::') !== false){"` |
|     - | 2387 | `"   $p = strpos($function,'::');"` |
|     - | 2388 | `"   $m = substr($function,$p+2);"` |
|     - | 2389 | `"   $t = substr($function,0,$p);"` |
|     - | 2390 | `"  }"` |
|     - | 2391 | `"  if($m !== null){"` |
|     - | 2392 | `"   $rm = new ReflectionMethod($t, $m);"` |
|     - | 2393 | `"   $t = $rm->class;"` |
|     - | 2394 | `"   $m = $rm->name;"` |
|     - | 2395 | `"   $i = __reflect_func_info($t, $m);"` |
|     - | 2396 | `"  }else if($function instanceof Closure){"` |
|     - | 2397 | `"   $t = $function;"` |
|     - | 2398 | `"   $i = __reflect_func_info($function);"` |
|     - | 2399 | `"  }else{"` |
|     - | 2400 | `"   $i = __reflect_sig_fixup(__reflect_func_info($t));"` |
|     - | 2401 | `"   if($i === null){"` |
|     - | 2402 | `"    throw new ReflectionException('Function '.$t.'() does not exist');"` |
|     - | 2403 | `"   }"` |
|     - | 2404 | `"  }"` |
|     - | 2405 | `"  $found = null;"` |
|     - | 2406 | `"  if(is_int($param)){"` |
|     - | 2407 | `"   if(isset($i['params'][$param])){ $found = $i['params'][$param]; }"` |
|     - | 2408 | `"   if($found === null){"` |
|     - | 2409 | `"    throw new ReflectionException('The parameter specified by its offset could not be found');"` |
|     - | 2410 | `"   }"` |
|     - | 2411 | `"  }else{"` |
|     - | 2412 | `"   foreach($i['params'] as $pp){"` |
|     - | 2413 | `"    if($pp['name'] === $param){ $found = $pp; break; }"` |
|     - | 2414 | `"   }"` |
|     - | 2415 | `"   if($found === null){"` |
|     - | 2416 | `"    throw new ReflectionException('The parameter specified by its name could not be found');"` |
|     - | 2417 | `"   }"` |
|     - | 2418 | `"  }"` |
|     - | 2419 | `"  $this->name = $found['name'];"` |
|     - | 2420 | `"  $this->__t = $t;"` |
|     - | 2421 | `"  $this->__m = $m;"` |
|     - | 2422 | `"  $this->__p = $found['pos'];"` |
|     - | 2423 | `" }"` |
|     - | 2424 | `" protected function __rffull(){"` |
|     - | 2425 | `"  if($this->__m !== null){ return __reflect_func_info($this->__t, $this->__m); }"` |
|     - | 2426 | `"  return __reflect_sig_fixup(__reflect_func_info($this->__t));"` |
|     - | 2427 | `" }"` |
|     - | 2428 | `" protected function __rpinfo(){"` |
|     - | 2429 | `"  $i = $this->__rffull();"` |
|     - | 2430 | `"  return $i['params'][$this->__p];"` |
|     - | 2431 | `" }"` |
|     - | 2432 | `" public function getName(){ return $this->name; }"` |
|     - | 2433 | `" public function getPosition(){ return $this->__p; }"` |
|     - | 2434 | `" public function isPassedByReference(){ $p = $this->__rpinfo(); return $p['byref']; }"` |
|     - | 2435 | `" public function canBePassedByValue(){ return !$this->isPassedByReference(); }"` |
|     - | 2436 | `" public function isVariadic(){ $p = $this->__rpinfo(); return $p['variadic']; }"` |
|     - | 2437 | `" public function isPromoted(){ $p = $this->__rpinfo(); return $p['promoted']; }"` |
|     - | 2438 | `" public function isDefaultValueAvailable(){ $p = $this->__rpinfo(); return $p['hasdef']; }"` |
|     - | 2439 | `" public function isOptional(){"` |
|     - | 2440 | `"  $i = $this->__rffull();"` |
|     - | 2441 | `"  $n = count($i['params']);"` |
|     - | 2442 | `"  for($k = $this->__p; $k < $n; $k++){"` |
|     - | 2443 | `"   $p = $i['params'][$k];"` |
|     - | 2444 | `"   if(!$p['variadic'] && !$p['hasdef']){ return false; }"` |
|     - | 2445 | `"  }"` |
|     - | 2446 | `"  return true;"` |
|     - | 2447 | `" }"` |
|     - | 2448 | `" public function getDefaultValue(){"` |
|     - | 2449 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2450 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2451 | `"  }"` |
|     - | 2452 | `"  $p = $this->__rpinfo();"` |
|     - | 2453 | `"  if(isset($p['deftext'])){"` |
|     - | 2454 | `"   $s = __reflect_sig_scalar($p['deftext']);"` |
|     - | 2455 | `"   if($s[0]){ return $s[1]; }"` |
|     - | 2456 | `"   if($p['deftext'] === 'array (' \|\| strpos($p['deftext'], '[') === 0){ return array(); }"` |
|     - | 2457 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2458 | `"  }"` |
|     - | 2459 | `"  return __reflect_param_default($this->__t, $this->__m, $this->__p);"` |
|     - | 2460 | `" }"` |
|     - | 2461 | `" public function isDefaultValueConstant(){"` |
|     - | 2462 | `"  if(!$this->isDefaultValueAvailable()){ return false; }"` |
|     - | 2463 | `"  $p = $this->__rpinfo();"` |
|     - | 2464 | `"  if(isset($p['deftext'])){ return false; }"` |
|     - | 2465 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p) !== null;"` |
|     - | 2466 | `" }"` |
|     - | 2467 | `" public function getDefaultValueConstantName(){"` |
|     - | 2468 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2469 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2470 | `"  }"` |
|     - | 2471 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p);"` |
|     - | 2472 | `" }"` |
|     - | 2473 | `" public function allowsNull(){"` |
|     - | 2474 | `"  $p = $this->__rpinfo();"` |
|     - | 2475 | `"  if($p['typetext'] === null){ return true; }"` |
|     - | 2476 | `"  if($p['nullable']){ return true; }"` |
|     - | 2477 | `"  return $p['typetext'] === 'mixed' \|\| $p['typetext'] === 'null';"` |
|     - | 2478 | `" }"` |
|     - | 2479 | `" public function hasType(){ $p = $this->__rpinfo(); return $p['typetext'] !== null; }"` |
|     - | 2480 | `" public function getType(){ $p = $this->__rpinfo(); return __reflect_make_type($p['typetext']); }"` |
|     - | 2481 | `" public function getDeclaringFunction(){"` |
|     - | 2482 | `"  if($this->__m !== null){ return new ReflectionMethod($this->__t, $this->__m); }"` |
|     - | 2483 | `"  return new ReflectionFunction($this->__t);"` |
|     - | 2484 | `" }"` |
|     - | 2485 | `" public function getDeclaringClass(){"` |
|     - | 2486 | `"  if($this->__m === null){ return null; }"` |
|     - | 2487 | `"  $i = $this->__rffull();"` |
|     - | 2488 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2489 | `" }"` |
|     - | 2490 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2491 | `"  $p = $this->__rpinfo();"` |
|     - | 2492 | `"  return __reflect_build_attrs($p['attrs'], array('param', $this->__t, $this->__m, $this->__p), 32, $name, $flags);"` |
|     - | 2493 | `" }"` |
|     - | 2494 | `" public function __toString(){ return __reflect_export_param($this); }"` |
|     - | 2495 | `"}"` |
|     - | 2496 | `;` |
|     - | 2497 | `/*` |
|     - | 2498 | ` * Chunk 3: PropertyHookType, ReflectionProperty, ReflectionClassConstant.` |
|     - | 2499 | ` */` |
|     - | 2500 | `static const char zReflectLib3[] =` |
|     - | 2501 | `"enum PropertyHookType: string {"` |
|     - | 2502 | `" case Get = 'get';"` |
|     - | 2503 | `" case Set = 'set';"` |
|     - | 2504 | `"}"` |
|     - | 2505 | `"class ReflectionProperty implements Reflector {"` |
|     - | 2506 | `" const IS_PUBLIC = 1;"` |
|     - | 2507 | `" const IS_PROTECTED = 2;"` |
|     - | 2508 | `" const IS_PRIVATE = 4;"` |
|     - | 2509 | `" const IS_STATIC = 16;"` |
|     - | 2510 | `" const IS_FINAL = 32;"` |
|     - | 2511 | `" const IS_ABSTRACT = 64;"` |
|     - | 2512 | `" const IS_READONLY = 128;"` |
|     - | 2513 | `" const IS_VIRTUAL = 512;"` |
|     - | 2514 | `" const IS_PROTECTED_SET = 2048;"` |
|     - | 2515 | `" const IS_PRIVATE_SET = 4096;"` |
|     - | 2516 | `" public $name;"` |
|     - | 2517 | `" public $class;"` |
|     - | 2518 | `" protected $__dynobj = null;"` |
|     - | 2519 | `" public function __construct($class, $property){"` |
|     - | 2520 | `"  $obj = null;"` |
|     - | 2521 | `"  if(is_object($class)){ $obj = $class; }"` |
|     - | 2522 | `"  else if(!is_string($class)){"` |
|     - | 2523 | `"   throw new TypeError('ReflectionProperty::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2524 | `"  }"` |
|     - | 2525 | `"  $ci = __phl_rcinfo($class);"` |
|     - | 2526 | `"  if($ci === null){"` |
|     - | 2527 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2528 | `"  }"` |
|     - | 2529 | `"  $this->class = $ci['name'];"` |
|     - | 2530 | `"  if(isset($ci['props'][$property])){"` |
|     - | 2531 | `"   $this->name = $property;"` |
|     - | 2532 | `"   return;"` |
|     - | 2533 | `"  }"` |
|     - | 2534 | `"  if($obj !== null && (__reflect_prop_state($obj, $property) & 1)){"` |
|     - | 2535 | `"   $this->name = $property;"` |
|     - | 2536 | `"   $this->__dynobj = $obj;"` |
|     - | 2537 | `"   return;"` |
|     - | 2538 | `"  }"` |
|     - | 2539 | `"  throw new ReflectionException('Property '.$this->class.'::$'.$property.' does not exist');"` |
|     - | 2540 | `" }"` |
|     - | 2541 | `" protected function __rpmeta(){"` |
|     - | 2542 | `"  $ci = __phl_rcinfo($this->class);"` |
|     - | 2543 | `"  if(isset($ci['props'][$this->name])){ return $ci['props'][$this->name]; }"` |
|     - | 2544 | `"  return array('vis' => 1, 'static' => false, 'readonly' => false, 'hasdef' => false,"` |
|     - | 2545 | `"   'typed' => false, 'typetext' => null, 'decl' => $this->class, 'line' => 0, 'dyn' => true);"` |
|     - | 2546 | `" }"` |
|     - | 2547 | `" public function getName(){ return $this->name; }"` |
|     - | 2548 | `" public function getDeclaringClass(){"` |
|     - | 2549 | `"  $m = $this->__rpmeta();"` |
|     - | 2550 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2551 | `" }"` |
|     - | 2552 | `" public function getModifiers(){"` |
|     - | 2553 | `"  $m = $this->__rpmeta();"` |
|     - | 2554 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2555 | `"  if($m['static']){ $mod \|= 16; }"` |
|     - | 2556 | `"  if($m['readonly']){ $mod \|= 128; }"` |
|     - | 2557 | `"  return $mod;"` |
|     - | 2558 | `" }"` |
|     - | 2559 | `" public function isPublic(){ $m = $this->__rpmeta(); return $m['vis'] === 1; }"` |
|     - | 2560 | `" public function isProtected(){ $m = $this->__rpmeta(); return $m['vis'] === 2; }"` |
|     - | 2561 | `" public function isPrivate(){ $m = $this->__rpmeta(); return $m['vis'] === 3; }"` |
|     - | 2562 | `" public function isStatic(){ $m = $this->__rpmeta(); return $m['static']; }"` |
|     - | 2563 | `" public function isReadOnly(){ $m = $this->__rpmeta(); return $m['readonly']; }"` |
|     - | 2564 | `" public function isPrivateSet(){ $m = $this->__rpmeta(); return isset($m['privset']) ? $m['privset'] : false; }"` |
|     - | 2565 | `" public function isProtectedSet(){ $m = $this->__rpmeta(); return isset($m['protset']) ? $m['protset'] : false; }"` |
|     - | 2566 | `" public function isDefault(){ $m = $this->__rpmeta(); return !isset($m['dyn']); }"` |
|     - | 2567 | `" public function isDynamic(){ $m = $this->__rpmeta(); return isset($m['dyn']); }"` |
|     - | 2568 | `" public function isAbstract(){ return false; }"` |
|     - | 2569 | `" public function isFinal(){ return false; }"` |
|     - | 2570 | `" public function isVirtual(){ $m = $this->__rpmeta(); return isset($m['virtual']) ? $m['virtual'] : false; }"` |
|     - | 2571 | `" public function hasHooks(){ $m = $this->__rpmeta();"` |
|     - | 2572 | `"  return (isset($m['hookget']) && $m['hookget']) \|\| (isset($m['hookset']) && $m['hookset']); }"` |
|     - | 2573 | `" public function getHooks(){"` |
|     - | 2574 | `"  $m = $this->__rpmeta(); $h = array();"` |
|     - | 2575 | `"  if(isset($m['hookget']) && $m['hookget']){ $h['get'] = new ReflectionMethod($m['decl'], '__phl_hook_get_'.$this->name); }"` |
|     - | 2576 | `"  if(isset($m['hookset']) && $m['hookset']){ $h['set'] = new ReflectionMethod($m['decl'], '__phl_hook_set_'.$this->name); }"` |
|     - | 2577 | `"  return $h; }"` |
|     - | 2578 | `" public function hasHook($type){"` |
|     - | 2579 | `"  $t = $type instanceof PropertyHookType ? $type->value : $type;"` |
|     - | 2580 | `"  $m = $this->__rpmeta();"` |
|     - | 2581 | `"  if($t === 'get'){ return isset($m['hookget']) && $m['hookget']; }"` |
|     - | 2582 | `"  if($t === 'set'){ return isset($m['hookset']) && $m['hookset']; }"` |
|     - | 2583 | `"  return false; }"` |
|     - | 2584 | `" public function getHook($type){"` |
|     - | 2585 | `"  $t = $type instanceof PropertyHookType ? $type->value : $type;"` |
|     - | 2586 | `"  $h = $this->getHooks();"` |
|     - | 2587 | `"  return isset($h[$t]) ? $h[$t] : null; }"` |
|     - | 2588 | `" public function isLazy($object){ return false; }"` |
|     - | 2589 | `" public function setAccessible($accessible){ }"` |
|     - | 2590 | `" public function getValue($object = null){"` |
|     - | 2591 | `"  $m = $this->__rpmeta();"` |
|     - | 2592 | `"  if($m['static']){ return __reflect_static_value($this->class, $this->name); }"` |
|     - | 2593 | `"  if(!is_object($object)){"` |
|     - | 2594 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2595 | `"  }"` |
|     - | 2596 | `"  return __reflect_prop_read($object, $this->name);"` |
|     - | 2597 | `" }"` |
|     - | 2598 | `" public function setValue($objectOrValue = null, $value = null){"` |
|     - | 2599 | `"  $m = $this->__rpmeta();"` |
|     - | 2600 | `"  if($m['static']){"` |
|     - | 2601 | `"   if($value === null && $objectOrValue !== null && !is_object($objectOrValue)){"` |
|     - | 2602 | `"    __reflect_static_set($this->class, $this->name, $objectOrValue);"` |
|     - | 2603 | `"   }else{"` |
|     - | 2604 | `"    __reflect_static_set($this->class, $this->name, $value);"` |
|     - | 2605 | `"   }"` |
|     - | 2606 | `"   return;"` |
|     - | 2607 | `"  }"` |
|     - | 2608 | `"  __reflect_prop_write($objectOrValue, $this->name, $value);"` |
|     - | 2609 | `" }"` |
|     - | 2610 | `" public function getRawValue($object){ return $this->getValue($object); }"` |
|     - | 2611 | `" public function setRawValue($object, $value){ $this->setValue($object, $value); }"` |
|     - | 2612 | `" public function isInitialized($object = null){"` |
|     - | 2613 | `"  $m = $this->__rpmeta();"` |
|     - | 2614 | `"  if($m['static']){ return (__reflect_prop_state($this->class, $this->name) & 2) !== 0; }"` |
|     - | 2615 | `"  if(!is_object($object)){"` |
|     - | 2616 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2617 | `"  }"` |
|     - | 2618 | `"  return (__reflect_prop_state($object, $this->name) & 2) !== 0;"` |
|     - | 2619 | `" }"` |
|     - | 2620 | `" public function hasDefaultValue(){"` |
|     - | 2621 | `"  $m = $this->__rpmeta();"` |
|     - | 2622 | `"  if(isset($m['dyn'])){ return false; }"` |
|     - | 2623 | `"  if($m['hasdef']){ return true; }"` |
|     - | 2624 | `"  return !$m['typed'];"` |
|     - | 2625 | `" }"` |
|     - | 2626 | `" public function getDefaultValue(){"` |
|     - | 2627 | `"  $m = $this->__rpmeta();"` |
|     - | 2628 | `"  if(isset($m['dyn']) \|\| !$m['hasdef']){ return null; }"` |
|     - | 2629 | `"  return __reflect_prop_default($this->class, $this->name);"` |
|     - | 2630 | `" }"` |
|     - | 2631 | `" public function hasType(){ $m = $this->__rpmeta(); return $m['typed']; }"` |
|     - | 2632 | `" public function getType(){ $m = $this->__rpmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2633 | `" public function getSettableType(){ return $this->getType(); }"` |
|     - | 2634 | `" public function setRawValueWithoutLazyInitialization($object, $value){"` |
|     - | 2635 | `"  throw new Error('ReflectionProperty::setRawValueWithoutLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2636 | `" }"` |
|     - | 2637 | `" public function skipLazyInitialization($object){"` |
|     - | 2638 | `"  throw new Error('ReflectionProperty::skipLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2639 | `" }"` |
|     - | 2640 | `" public function getDocComment(){ $m = $this->__rpmeta(); return isset($m['doc']) ? $m['doc'] : false; }"` |
|     - | 2641 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2642 | `"  $m = $this->__rpmeta();"` |
|     - | 2643 | `"  if(!isset($m['attrs'])){ return array(); }"` |
|     - | 2644 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 8, $name, $flags);"` |
|     - | 2645 | `" }"` |
|     - | 2646 | `" public function __toString(){ return __reflect_export_prop($this); }"` |
|     - | 2647 | `"}"` |
|     - | 2648 | `"class ReflectionClassConstant implements Reflector {"` |
|     - | 2649 | `" const IS_PUBLIC = 1;"` |
|     - | 2650 | `" const IS_PROTECTED = 2;"` |
|     - | 2651 | `" const IS_PRIVATE = 4;"` |
|     - | 2652 | `" const IS_FINAL = 32;"` |
|     - | 2653 | `" public $name;"` |
|     - | 2654 | `" public $class;"` |
|     - | 2655 | `" public function __construct($class, $constant){"` |
|     - | 2656 | `"  if(!is_object($class) && !is_string($class)){"` |
|     - | 2657 | `"   throw new TypeError('ReflectionClassConstant::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2658 | `"  }"` |
|     - | 2659 | `"  $ci = __phl_rcinfo($class);"` |
|     - | 2660 | `"  if($ci === null){"` |
|     - | 2661 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2662 | `"  }"` |
|     - | 2663 | `"  $this->class = $ci['name'];"` |
|     - | 2664 | `"  if(!isset($ci['consts'][$constant])){"` |
|     - | 2665 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' does not exist');"` |
|     - | 2666 | `"  }"` |
|     - | 2667 | `"  $this->name = $constant;"` |
|     - | 2668 | `" }"` |
|     - | 2669 | `" protected function __rcmeta(){"` |
|     - | 2670 | `"  $ci = __phl_rcinfo($this->class);"` |
|     - | 2671 | `"  return $ci['consts'][$this->name];"` |
|     - | 2672 | `" }"` |
|     - | 2673 | `" public function getName(){ return $this->name; }"` |
|     - | 2674 | `" public function getValue(){ return __reflect_const_value($this->class, $this->name); }"` |
|     - | 2675 | `" public function getDeclaringClass(){"` |
|     - | 2676 | `"  $m = $this->__rcmeta();"` |
|     - | 2677 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2678 | `" }"` |
|     - | 2679 | `" public function getModifiers(){"` |
|     - | 2680 | `"  $m = $this->__rcmeta();"` |
|     - | 2681 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2682 | `"  if($m['final']){ $mod \|= 32; }"` |
|     - | 2683 | `"  return $mod;"` |
|     - | 2684 | `" }"` |
|     - | 2685 | `" public function isPublic(){ $m = $this->__rcmeta(); return $m['vis'] === 1; }"` |
|     - | 2686 | `" public function isProtected(){ $m = $this->__rcmeta(); return $m['vis'] === 2; }"` |
|     - | 2687 | `" public function isPrivate(){ $m = $this->__rcmeta(); return $m['vis'] === 3; }"` |
|     - | 2688 | `" public function isFinal(){ $m = $this->__rcmeta(); return $m['final']; }"` |
|     - | 2689 | `" public function isEnumCase(){ $m = $this->__rcmeta(); return $m['enumcase']; }"` |
|     - | 2690 | `" public function isDeprecated(){ $m = $this->__rcmeta(); return __reflect_has_deprecated($m['attrs']); }"` |
|     - | 2691 | `" public function hasType(){ $m = $this->__rcmeta(); return $m['typed']; }"` |
|     - | 2692 | `" public function getType(){ $m = $this->__rcmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2693 | `" public function getDocComment(){ $m = $this->__rcmeta(); return $m['doc']; }"` |
|     - | 2694 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2695 | `"  $m = $this->__rcmeta();"` |
|     - | 2696 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 16, $name, $flags);"` |
|     - | 2697 | `" }"` |
|     - | 2698 | `" public function __toString(){ return __reflect_export_cconst($this); }"` |
|     - | 2699 | `"}"` |
|     - | 2700 | `;` |
|     - | 2701 | `/*` |
|     - | 2702 | ` * Chunk 4: the ReflectionType family, built from the engine's canonical` |
|     - | 2703 | ` * type text ("?int", "string\|float", "(A&B)\|C" — normalized at compile` |
|     - | 2704 | ` * time). __reflect_make_type is the internal factory; PHP itself never` |
|     - | 2705 | ` * lets user code construct these, so the public constructors here are a` |
|     - | 2706 | ` * recorded PHL-only surface.` |
|     - | 2707 | ` */` |
|     - | 2708 | `static const char zReflectLib4[] =` |
|     - | 2709 | `"abstract class ReflectionType implements Stringable {"` |
|     - | 2710 | `" protected $__text = '';"` |
|     - | 2711 | `" protected $__nullable = false;"` |
|     - | 2712 | `" public function allowsNull(){ return $this->__nullable; }"` |
|     - | 2713 | `" public function __toString(){ return $this->__text; }"` |
|     - | 2714 | `"}"` |
|     - | 2715 | `"class ReflectionNamedType extends ReflectionType {"` |
|     - | 2716 | `" protected $__tname = '';"` |
|     - | 2717 | `" public function __construct($name = '', $nullable = false, $text = null){"` |
|     - | 2718 | `"  $this->__tname = $name;"` |
|     - | 2719 | `"  $l = strtolower($name);"` |
|     - | 2720 | `"  $this->__nullable = $nullable \|\| $l === 'null' \|\| $l === 'mixed';"` |
|     - | 2721 | `"  $this->__text = $text === null ? $name : $text;"` |
|     - | 2722 | `" }"` |
|     - | 2723 | `" public function getName(){ return $this->__tname; }"` |
|     - | 2724 | `" public function isBuiltin(){"` |
|     - | 2725 | `"  $l = strtolower($this->__tname);"` |
|     - | 2726 | `"  return in_array($l, array('int','float','string','bool','array','object','mixed',"` |
|     - | 2727 | `"   'void','never','null','callable','iterable','true','false'), true);"` |
|     - | 2728 | `" }"` |
|     - | 2729 | `"}"` |
|     - | 2730 | `"class ReflectionUnionType extends ReflectionType {"` |
|     - | 2731 | `" protected $__types = array();"` |
|     - | 2732 | `" public function __construct($text = '', $nullable = false, $types = array()){"` |
|     - | 2733 | `"  $this->__text = $text;"` |
|     - | 2734 | `"  $this->__nullable = $nullable;"` |
|     - | 2735 | `"  $this->__types = $types;"` |
|     - | 2736 | `" }"` |
|     - | 2737 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2738 | `"}"` |
|     - | 2739 | `"class ReflectionIntersectionType extends ReflectionType {"` |
|     - | 2740 | `" protected $__types = array();"` |
|     - | 2741 | `" public function __construct($text = '', $types = array()){"` |
|     - | 2742 | `"  $this->__text = $text;"` |
|     - | 2743 | `"  $this->__nullable = false;"` |
|     - | 2744 | `"  $this->__types = $types;"` |
|     - | 2745 | `" }"` |
|     - | 2746 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2747 | `"}"` |
|     - | 2748 | `"function __reflect_make_atom($p){"` |
|     - | 2749 | `" $nullable = false;"` |
|     - | 2750 | `" if($p !== '' && $p[0] === '?'){ $nullable = true; $p = substr($p, 1); }"` |
|     - | 2751 | `" if($p !== '' && $p[0] === '('){ $p = substr($p, 1, strlen($p) - 2); }"` |
|     - | 2752 | `" if(strpos($p, '&') !== false){"` |
|     - | 2753 | `"  $subs = array();"` |
|     - | 2754 | `"  foreach(explode('&', $p) as $s){ $subs[] = new ReflectionNamedType($s, false, $s); }"` |
|     - | 2755 | `"  return new ReflectionIntersectionType($p, $subs);"` |
|     - | 2756 | `" }"` |
|     - | 2757 | `" return new ReflectionNamedType($p, $nullable, $nullable ? '?'.$p : $p);"` |
|     - | 2758 | `"}"` |
|     - | 2759 | `"function __reflect_make_type($text){"` |
|     - | 2760 | `" if($text === null \|\| $text === ''){ return null; }"` |
|     - | 2761 | `" $nullable = false;"` |
|     - | 2762 | `" $body = $text;"` |
|     - | 2763 | `" if($body[0] === '?'){ $nullable = true; $body = substr($body, 1); }"` |
|     - | 2764 | `" $parts = array();"` |
|     - | 2765 | `" $depth = 0;"` |
|     - | 2766 | `" $cur = '';"` |
|     - | 2767 | `" $n = strlen($body);"` |
|     - | 2768 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 2769 | `"  $ch = $body[$k];"` |
|     - | 2770 | `"  if($ch === '('){ $depth++; $cur .= $ch; }"` |
|     - | 2771 | `"  else if($ch === ')'){ $depth--; $cur .= $ch; }"` |
|     - | 2772 | `"  else if($ch === '\|' && $depth === 0){ $parts[] = $cur; $cur = ''; }"` |
|     - | 2773 | `"  else{ $cur .= $ch; }"` |
|     - | 2774 | `" }"` |
|     - | 2775 | `" $parts[] = $cur;"` |
|     - | 2776 | `" if(count($parts) > 1){"` |
|     - | 2777 | `"  $nonNull = array();"` |
|     - | 2778 | `"  $hasNull = false;"` |
|     - | 2779 | `"  foreach($parts as $p){"` |
|     - | 2780 | `"   if(strtolower($p) === 'null'){ $hasNull = true; }"` |
|     - | 2781 | `"   else{ $nonNull[] = $p; }"` |
|     - | 2782 | `"  }"` |
|     - | 2783 | `"  if($hasNull && count($nonNull) === 1 && strpos($nonNull[0], '&') === false){"` |
|     - | 2784 | `"   return new ReflectionNamedType($nonNull[0], true, '?'.$nonNull[0]);"` |
|     - | 2785 | `"  }"` |
|     - | 2786 | `"  $types = array();"` |
|     - | 2787 | `"  foreach($parts as $p){ $types[] = __reflect_make_atom($p); }"` |
|     - | 2788 | `"  return new ReflectionUnionType($body, $nullable \|\| $hasNull, $types);"` |
|     - | 2789 | `" }"` |
|     - | 2790 | `" if(strpos($body, '&') !== false){ return __reflect_make_atom($body); }"` |
|     - | 2791 | `" return __reflect_make_atom($nullable ? '?'.$body : $body);"` |
|     - | 2792 | `"}"` |
|     - | 2793 | `;` |
|     - | 2794 | `/*` |
|     - | 2795 | ` * Chunk 5: ReflectionGenerator, ReflectionFiber. Executing line/file and` |
|     - | 2796 | ` * traces need runtime line tracking the VM does not have (same gap as` |
|     - | 2797 | ` * debug_backtrace's line numbers) — those throw a loud Error, recorded in` |
|     - | 2798 | ` * the plan ledger.` |
|     - | 2799 | ` */` |
|     - | 2800 | `static const char zReflectLib5[] =` |
|     - | 2801 | `"class ReflectionGenerator {"` |
|     - | 2802 | `" protected $__gen;"` |
|     - | 2803 | `" public function __construct($generator){"` |
|     - | 2804 | `"  if(!($generator instanceof Generator)){"` |
|     - | 2805 | `"   throw new TypeError('ReflectionGenerator::__construct(): Argument #1 ($generator) must be of type Generator, '.get_debug_type($generator).' given');"` |
|     - | 2806 | `"  }"` |
|     - | 2807 | `"  $this->__gen = $generator;"` |
|     - | 2808 | `" }"` |
|     - | 2809 | `" protected function __rginfo(){ return __reflect_gen_info($this->__gen); }"` |
|     - | 2810 | `" public function getFunction(){"` |
|     - | 2811 | `"  $i = $this->__rginfo();"` |
|     - | 2812 | `"  if($i['kind'] === 'method'){ return new ReflectionMethod($i['class'], $i['name']); }"` |
|     - | 2813 | `"  return new ReflectionFunction($i['name']);"` |
|     - | 2814 | `" }"` |
|     - | 2815 | `" public function getThis(){ $i = $this->__rginfo(); return isset($i['this']) ? $i['this'] : null; }"` |
|     - | 2816 | `" public function getExecutingGenerator(){ return __reflect_gen_exec($this->__gen); }"` |
|     - | 2817 | `" public function isClosed(){ $i = $this->__rginfo(); return $i['closed']; }"` |
|     - | 2818 | `" public function getExecutingLine(){"` |
|     - | 2819 | `"  throw new Error('ReflectionGenerator::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2820 | `" }"` |
|     - | 2821 | `" public function getExecutingFile(){"` |
|     - | 2822 | `"  throw new Error('ReflectionGenerator::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2823 | `" }"` |
|     - | 2824 | `" public function getTrace($options = 1){"` |
|     - | 2825 | `"  throw new Error('ReflectionGenerator::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2826 | `" }"` |
|     - | 2827 | `"}"` |
|     - | 2828 | `"class ReflectionFiber {"` |
|     - | 2829 | `" protected $__fiber;"` |
|     - | 2830 | `" public function __construct($fiber){"` |
|     - | 2831 | `"  if(!($fiber instanceof Fiber)){"` |
|     - | 2832 | `"   throw new TypeError('ReflectionFiber::__construct(): Argument #1 ($fiber) must be of type Fiber, '.get_debug_type($fiber).' given');"` |
|     - | 2833 | `"  }"` |
|     - | 2834 | `"  $this->__fiber = $fiber;"` |
|     - | 2835 | `" }"` |
|     - | 2836 | `" public function getFiber(){ return $this->__fiber; }"` |
|     - | 2837 | `" public function getCallable(){ return __reflect_prop_read($this->__fiber, '__callable'); }"` |
|     - | 2838 | `" public function getExecutingLine(){"` |
|     - | 2839 | `"  throw new Error('ReflectionFiber::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2840 | `" }"` |
|     - | 2841 | `" public function getExecutingFile(){"` |
|     - | 2842 | `"  throw new Error('ReflectionFiber::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2843 | `" }"` |
|     - | 2844 | `" public function getTrace($options = 1){"` |
|     - | 2845 | `"  throw new Error('ReflectionFiber::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2846 | `" }"` |
|     - | 2847 | `"}"` |
|     - | 2848 | `;` |
|     - | 2849 | `/*` |
|     - | 2850 | ` * Chunk 6: the long tail — ReflectionConstant (PHP 8.5), the synthetic` |
|     - | 2851 | ` * "Core" ReflectionExtension, ReflectionZendExtension (throws: no Zend` |
|     - | 2852 | ` * extensions exist), the ReflectionEnum family (throws: enums are not a` |
|     - | 2853 | ` * PHL language feature yet), and ReflectionReference.` |
|     - | 2854 | ` */` |
|     - | 2855 | `static const char zReflectLib6[] =` |
|     - | 2856 | `"class ReflectionConstant implements Reflector {"` |
|     - | 2857 | `" public $name;"` |
|     - | 2858 | `" public function __construct($name){"` |
|     - | 2859 | `"  if(!is_string($name)){"` |
|     - | 2860 | `"   throw new TypeError('ReflectionConstant::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2861 | `"  }"` |
|     - | 2862 | `"  $i = __reflect_const_info($name);"` |
|     - | 2863 | `"  if($i === null){"` |
|     - | 2864 | `"   throw new ReflectionException('Constant \"'.$name.'\" does not exist');"` |
|     - | 2865 | `"  }"` |
|     - | 2866 | `"  $this->name = $name;"` |
|     - | 2867 | `" }"` |
|     - | 2868 | `" public function getName(){ return $this->name; }"` |
|     - | 2869 | `" public function getNamespaceName(){"` |
|     - | 2870 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2871 | `"  if($p === false){ return ''; }"` |
|     - | 2872 | `"  return substr($this->name,0,$p);"` |
|     - | 2873 | `" }"` |
|     - | 2874 | `" public function getShortName(){"` |
|     - | 2875 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2876 | `"  if($p === false){ return $this->name; }"` |
|     - | 2877 | `"  return substr($this->name,$p+1);"` |
|     - | 2878 | `" }"` |
|     - | 2879 | `" public function getValue(){"` |
|     - | 2880 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2881 | `"  return $i['value'];"` |
|     - | 2882 | `" }"` |
|     - | 2883 | `" public function isDeprecated(){ return false; }"` |
|     - | 2884 | `" public function getFileName(){"` |
|     - | 2885 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2886 | `"  return $i['file'];"` |
|     - | 2887 | `" }"` |
|     - | 2888 | `" public function getExtension(){"` |
|     - | 2889 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2890 | `"  return $i['internal'] ? new ReflectionExtension('Core') : null;"` |
|     - | 2891 | `" }"` |
|     - | 2892 | `" public function getExtensionName(){"` |
|     - | 2893 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2894 | `"  return $i['internal'] ? 'Core' : false;"` |
|     - | 2895 | `" }"` |
|     - | 2896 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2897 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2898 | `"  if($i === null){ return array(); }"` |
|     - | 2899 | `"  return __reflect_build_attrs($i['attrs'], array('const', $this->name, null, 0), 64, $name, $flags);"` |
|     - | 2900 | `" }"` |
|     - | 2901 | `" public function __toString(){"` |
|     - | 2902 | `"  return 'Constant [ '.$this->name.' ]'.\"\\n\";"` |
|     - | 2903 | `" }"` |
|     - | 2904 | `"}"` |
|     - | 2905 | `"class ReflectionExtension implements Reflector {"` |
|     - | 2906 | `" public $name;"` |
|     - | 2907 | `" public function __construct($name){"` |
|     - | 2908 | `"  if(!is_string($name)){"` |
|     - | 2909 | `"   throw new TypeError('ReflectionExtension::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2910 | `"  }"` |
|     - | 2911 | `"  if(strtolower($name) !== 'core'){"` |
|     - | 2912 | `"   throw new ReflectionException('Extension \"'.$name.'\" does not exist');"` |
|     - | 2913 | `"  }"` |
|     - | 2914 | `"  $this->name = 'Core';"` |
|     - | 2915 | `" }"` |
|     - | 2916 | `" public function getName(){ return $this->name; }"` |
|     - | 2917 | `" public function getVersion(){ return phpversion(); }"` |
|     - | 2918 | `" public function getFunctions(){ return array(); }"` |
|     - | 2919 | `" public function getClasses(){ return array(); }"` |
|     - | 2920 | `" public function getClassNames(){ return array(); }"` |
|     - | 2921 | `" public function getConstants(){ return array(); }"` |
|     - | 2922 | `" public function getINIEntries(){ return array(); }"` |
|     - | 2923 | `" public function getDependencies(){ return array(); }"` |
|     - | 2924 | `" public function isPersistent(){ return true; }"` |
|     - | 2925 | `" public function isTemporary(){ return false; }"` |
|     - | 2926 | `" public function info(){ }"` |
|     - | 2927 | `" public function __toString(){"` |
|     - | 2928 | `"  return 'Extension [ extension #1 '.$this->name.' ]'.\"\\n\";"` |
|     - | 2929 | `" }"` |
|     - | 2930 | `"}"` |
|     - | 2931 | `"class ReflectionZendExtension implements Reflector {"` |
|     - | 2932 | `" public $name;"` |
|     - | 2933 | `" public function __construct($name){"` |
|     - | 2934 | `"  throw new ReflectionException('Zend Extension \"'.$name.'\" does not exist');"` |
|     - | 2935 | `" }"` |
|     - | 2936 | `" public function getName(){ return $this->name; }"` |
|     - | 2937 | `" public function __toString(){ return ''; }"` |
|     - | 2938 | `"}"` |
|     - | 2939 | `"class ReflectionEnum extends ReflectionClass {"` |
|     - | 2940 | `" public function __construct($objectOrClass){"` |
|     - | 2941 | `"  $info = __phl_rcinfo($objectOrClass);"` |
|     - | 2942 | `"  if($info === null){"` |
|     - | 2943 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 2944 | `"  }"` |
|     - | 2945 | `"  if(!$info['enum']){"` |
|     - | 2946 | `"   throw new ReflectionException('Class \"'.$info['name'].'\" is not an enum');"` |
|     - | 2947 | `"  }"` |
|     - | 2948 | `"  parent::__construct($objectOrClass);"` |
|     - | 2949 | `" }"` |
|     - | 2950 | `" public function hasCase($name){"` |
|     - | 2951 | `"  $i = $this->__rinfo();"` |
|     - | 2952 | `"  return in_array($name, $i['cases'], true);"` |
|     - | 2953 | `" }"` |
|     - | 2954 | `" public function getCase($name){"` |
|     - | 2955 | `"  if(!$this->hasCase($name)){"` |
|     - | 2956 | `"   throw new ReflectionException('Case '.$this->name.'::'.$name.' does not exist');"` |
|     - | 2957 | `"  }"` |
|     - | 2958 | `"  if($this->isBacked()){ return new ReflectionEnumBackedCase($this->name, $name); }"` |
|     - | 2959 | `"  return new ReflectionEnumUnitCase($this->name, $name);"` |
|     - | 2960 | `" }"` |
|     - | 2961 | `" public function getCases(){"` |
|     - | 2962 | `"  $i = $this->__rinfo();"` |
|     - | 2963 | `"  $out = array();"` |
|     - | 2964 | `"  foreach($i['cases'] as $c){"` |
|     - | 2965 | `"   $out[] = $this->isBacked()"` |
|     - | 2966 | `"    ? new ReflectionEnumBackedCase($this->name, $c)"` |
|     - | 2967 | `"    : new ReflectionEnumUnitCase($this->name, $c);"` |
|     - | 2968 | `"  }"` |
|     - | 2969 | `"  return $out;"` |
|     - | 2970 | `" }"` |
|     - | 2971 | `" public function isBacked(){ $i = $this->__rinfo(); return $i['enumbacking'] !== ''; }"` |
|     - | 2972 | `" public function getBackingType(){"` |
|     - | 2973 | `"  $i = $this->__rinfo();"` |
|     - | 2974 | `"  if($i['enumbacking'] === ''){ return null; }"` |
|     - | 2975 | `"  return __reflect_make_type($i['enumbacking']);"` |
|     - | 2976 | `" }"` |
|     - | 2977 | `"}"` |
|     - | 2978 | `"class ReflectionEnumUnitCase extends ReflectionClassConstant {"` |
|     - | 2979 | `" public function __construct($class, $constant){"` |
|     - | 2980 | `"  parent::__construct($class, $constant);"` |
|     - | 2981 | `"  $ci = __phl_rcinfo($class);"` |
|     - | 2982 | `"  if(!$ci['enum']){"` |
|     - | 2983 | `"   throw new ReflectionException('Class \"'.$this->class.'\" is not an enum');"` |
|     - | 2984 | `"  }"` |
|     - | 2985 | `"  $m = $this->__rcmeta();"` |
|     - | 2986 | `"  if(!$m['enumcase']){"` |
|     - | 2987 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' is not a case');"` |
|     - | 2988 | `"  }"` |
|     - | 2989 | `" }"` |
|     - | 2990 | `" public function getEnum(){ return new ReflectionEnum($this->class); }"` |
|     - | 2991 | `"}"` |
|     - | 2992 | `"class ReflectionEnumBackedCase extends ReflectionEnumUnitCase {"` |
|     - | 2993 | `" public function getBackingValue(){ return $this->getValue()->value; }"` |
|     - | 2994 | `"}"` |
|     - | 2995 | `"final class ReflectionReference {"` |
|     - | 2996 | `" protected $__id = '';"` |
|     - | 2997 | `" public function __construct(){"` |
|     - | 2998 | `"  throw new Error('Call to private ReflectionReference::__construct() from global scope');"` |
|     - | 2999 | `" }"` |
|     - | 3000 | `" public static function fromArrayElement($array, $key){"` |
|     - | 3001 | `"  if(!is_array($array)){"` |
|     - | 3002 | `"   throw new TypeError('ReflectionReference::fromArrayElement(): Argument #1 ($array) must be of type array, '.get_debug_type($array).' given');"` |
|     - | 3003 | `"  }"` |
|     - | 3004 | `"  $id = __reflect_ref_id($array, $key);"` |
|     - | 3005 | `"  if($id === null){ return null; }"` |
|     - | 3006 | `"  $r = __reflect_new_no_ctor('ReflectionReference');"` |
|     - | 3007 | `"  $r->__setId('phlref'.$id);"` |
|     - | 3008 | `"  return $r;"` |
|     - | 3009 | `" }"` |
|     - | 3010 | `" public function __setId($id){ $this->__id = $id; }"` |
|     - | 3011 | `" public function getId(){ return $this->__id; }"` |
|     - | 3012 | `"}"` |
|     - | 3013 | `;` |
|     - | 3014 | `/*` |
|     - | 3015 | ` * Chunk 7: ReflectionAttribute and the shared getAttributes() builder.` |
|     - | 3016 | ` * The spec array rides as [kind, target, member, paramIdx]; argument` |
|     - | 3017 | ` * values evaluate lazily through __reflect_attr_args (PHP semantics).` |
|     - | 3018 | ` */` |
|     - | 3019 | `static const char zReflectLib7[] =` |
|     - | 3020 | `"function __reflect_has_deprecated($meta){"` |
|     - | 3021 | `" foreach($meta as $a){"` |
|     - | 3022 | `"  if(strtolower($a['name']) === 'deprecated'){ return true; }"` |
|     - | 3023 | `" }"` |
|     - | 3024 | `" return false;"` |
|     - | 3025 | `"}"` |
|     - | 3026 | `"function __reflect_target_names($mask){"` |
|     - | 3027 | `" $parts = array();"` |
|     - | 3028 | `" foreach(array('class' => 1, 'function' => 2, 'method' => 4, 'property' => 8,"` |
|     - | 3029 | `"  'class constant' => 16, 'parameter' => 32, 'constant' => 64) as $nm => $bit){"` |
|     - | 3030 | `"  if($mask & $bit){ $parts[] = $nm; }"` |
|     - | 3031 | `" }"` |
|     - | 3032 | `" return implode(', ', $parts);"` |
|     - | 3033 | `"}"` |
|     - | 3034 | `"function __reflect_build_attrs($meta, $spec, $target, $name, $flags){"` |
|     - | 3035 | `" $out = array();"` |
|     - | 3036 | `" $counts = array();"` |
|     - | 3037 | `" foreach($meta as $a){"` |
|     - | 3038 | `"  $k = strtolower($a['name']);"` |
|     - | 3039 | `"  $counts[$k] = isset($counts[$k]) ? $counts[$k] + 1 : 1;"` |
|     - | 3040 | `" }"` |
|     - | 3041 | `" $idx = 0;"` |
|     - | 3042 | `" foreach($meta as $a){"` |
|     - | 3043 | `"  $keep = true;"` |
|     - | 3044 | `"  if($name !== null){"` |
|     - | 3045 | `"   $keep = strtolower($a['name']) === strtolower($name);"` |
|     - | 3046 | `"   if(!$keep && ($flags & 2)){"` |
|     - | 3047 | `"    $keep = is_subclass_of($a['name'], $name);"` |
|     - | 3048 | `"   }"` |
|     - | 3049 | `"  }"` |
|     - | 3050 | `"  if($keep){"` |
|     - | 3051 | `"   $r = __reflect_new_no_ctor('ReflectionAttribute');"` |
|     - | 3052 | `"   $r->__init($a['name'], $spec, $idx, $target, $counts[strtolower($a['name'])] > 1);"` |
|     - | 3053 | `"   $out[] = $r;"` |
|     - | 3054 | `"  }"` |
|     - | 3055 | `"  $idx++;"` |
|     - | 3056 | `" }"` |
|     - | 3057 | `" return $out;"` |
|     - | 3058 | `"}"` |
|     - | 3059 | `"final class ReflectionAttribute {"` |
|     - | 3060 | `" const IS_INSTANCEOF = 2;"` |
|     - | 3061 | `" protected $__name = '';"` |
|     - | 3062 | `" protected $__spec = null;"` |
|     - | 3063 | `" protected $__idx = 0;"` |
|     - | 3064 | `" protected $__target = 0;"` |
|     - | 3065 | `" protected $__rep = false;"` |
|     - | 3066 | `" public function __construct(){"` |
|     - | 3067 | `"  throw new Error('Call to private ReflectionAttribute::__construct() from global scope');"` |
|     - | 3068 | `" }"` |
|     - | 3069 | `" public function __init($name, $spec, $idx, $target, $rep){"` |
|     - | 3070 | `"  $this->__name = $name;"` |
|     - | 3071 | `"  $this->__spec = $spec;"` |
|     - | 3072 | `"  $this->__idx = $idx;"` |
|     - | 3073 | `"  $this->__target = $target;"` |
|     - | 3074 | `"  $this->__rep = $rep;"` |
|     - | 3075 | `" }"` |
|     - | 3076 | `" public function getName(){ return $this->__name; }"` |
|     - | 3077 | `" public function getTarget(){ return $this->__target; }"` |
|     - | 3078 | `" public function isRepeated(){ return $this->__rep; }"` |
|     - | 3079 | `" public function getArguments(){"` |
|     - | 3080 | `"  $a = __reflect_attr_args($this->__spec[0], $this->__spec[1], $this->__spec[2], $this->__spec[3], $this->__idx);"` |
|     - | 3081 | `"  return $a === null ? array() : $a;"` |
|     - | 3082 | `" }"` |
|     - | 3083 | `" public function newInstance(){"` |
|     - | 3084 | `"  $name = $this->__name;"` |
|     - | 3085 | `"  $ci = __phl_rcinfo($name);"` |
|     - | 3086 | `"  if($ci === null){"` |
|     - | 3087 | `"   throw new Error('Attribute class \"'.$name.'\" not found');"` |
|     - | 3088 | `"  }"` |
|     - | 3089 | `"  $name = $ci['name'];"` |
|     - | 3090 | `"  $decl = null;"` |
|     - | 3091 | `"  $didx = 0;"` |
|     - | 3092 | `"  foreach($ci['attrs'] as $a){"` |
|     - | 3093 | `"   if(strtolower($a['name']) === 'attribute'){ $decl = $didx; break; }"` |
|     - | 3094 | `"   $didx++;"` |
|     - | 3095 | `"  }"` |
|     - | 3096 | `"  if($decl === null){"` |
|     - | 3097 | `"   throw new Error('Attempting to use non-attribute class \"'.$name.'\" as attribute');"` |
|     - | 3098 | `"  }"` |
|     - | 3099 | `"  $dargs = __reflect_attr_args('class', $name, null, 0, $decl);"` |
|     - | 3100 | `"  $flags = 127;"` |
|     - | 3101 | `"  if(is_array($dargs)){"` |
|     - | 3102 | `"   if(isset($dargs[0])){ $flags = $dargs[0]; }"` |
|     - | 3103 | `"   else if(isset($dargs['flags'])){ $flags = $dargs['flags']; }"` |
|     - | 3104 | `"  }"` |
|     - | 3105 | `"  if(($flags & $this->__target) === 0){"` |
|     - | 3106 | `"   $tnames = array(1 => 'class', 2 => 'function', 4 => 'method', 8 => 'property',"` |
|     - | 3107 | `"    16 => 'class constant', 32 => 'parameter', 64 => 'constant');"` |
|     - | 3108 | `"   throw new Error('Attribute \"'.$name.'\" cannot target '.$tnames[$this->__target]"` |
|     - | 3109 | `"    .' (allowed targets: '.__reflect_target_names($flags).')');"` |
|     - | 3110 | `"  }"` |
|     - | 3111 | `"  if($this->__rep && ($flags & 128) === 0){"` |
|     - | 3112 | `"   throw new Error('Attribute \"'.$name.'\" must not be repeated');"` |
|     - | 3113 | `"  }"` |
|     - | 3114 | `"  return __reflect_new_instance($name, $this->getArguments());"` |
|     - | 3115 | `" }"` |
|     - | 3116 | `" public function __toString(){"` |
|     - | 3117 | `"  return 'Attribute [ '.$this->__name.' ]';"` |
|     - | 3118 | `" }"` |
|     - | 3119 | `"}"` |
|     - | 3120 | `;` |
|     - | 3121 | `/*` |
|     - | 3122 | ` * Chunk 8: signature-table support. Internal (C builtin) functions carry a` |
|     - | 3123 | ` * PHP-style parameter-list string; these helpers parse it into the same` |
|     - | 3124 | ` * param-meta shape user functions get, so ReflectionFunction and` |
|     - | 3125 | ` * ReflectionParameter work uniformly over builtins.` |
|     - | 3126 | ` */` |
|     - | 3127 | `static const char zReflectLib8[] =` |
|     - | 3128 | `"function __reflect_sig_split($sig){"` |
|     - | 3129 | `" $parts = array();"` |
|     - | 3130 | `" $cur = '';"` |
|     - | 3131 | `" $q = false;"` |
|     - | 3132 | `" $n = strlen($sig);"` |
|     - | 3133 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3134 | `"  $ch = $sig[$k];"` |
|     - | 3135 | `"  if($q){"` |
|     - | 3136 | `"   $cur .= $ch;"` |
|     - | 3137 | `"   if($ch === chr(92) && $k + 1 < $n){ $cur .= $sig[$k+1]; $k++; }"` |
|     - | 3138 | `"   else if($ch === chr(39)){ $q = false; }"` |
|     - | 3139 | `"  }else if($ch === chr(39)){ $q = true; $cur .= $ch; }"` |
|     - | 3140 | `"  else if($ch === ',' ){ $parts[] = trim($cur); $cur = ''; }"` |
|     - | 3141 | `"  else{ $cur .= $ch; }"` |
|     - | 3142 | `" }"` |
|     - | 3143 | `" if(trim($cur) !== ''){ $parts[] = trim($cur); }"` |
|     - | 3144 | `" return $parts;"` |
|     - | 3145 | `"}"` |
|     - | 3146 | `"function __reflect_sig_scalar($t){"` |
|     - | 3147 | `" if($t === '?'){ return array(false, null); }"` |
|     - | 3148 | `" if($t === 'NULL' \|\| $t === 'null'){ return array(true, null); }"` |
|     - | 3149 | `" if($t === 'true'){ return array(true, true); }"` |
|     - | 3150 | `" if($t === 'false'){ return array(true, false); }"` |
|     - | 3151 | `" if(is_numeric($t)){"` |
|     - | 3152 | `"  if(strpos($t, '.') === false && stripos($t, 'e') === false && strpos($t, 'x') === false){"` |
|     - | 3153 | `"   return array(true, (int)$t);"` |
|     - | 3154 | `"  }"` |
|     - | 3155 | `"  return array(true, (float)$t);"` |
|     - | 3156 | `" }"` |
|     - | 3157 | `" if(strlen($t) >= 2 && $t[0] === chr(39) && $t[strlen($t)-1] === chr(39)){"` |
|     - | 3158 | `"  $body = substr($t, 1, strlen($t) - 2);"` |
|     - | 3159 | `"  return array(true, strtr($body, array(chr(92).chr(39) => chr(39), chr(92).chr(92) => chr(92))));"` |
|     - | 3160 | `" }"` |
|     - | 3161 | `" return array(false, null);"` |
|     - | 3162 | `"}"` |
|     - | 3163 | `"function __reflect_parse_sig($sig){"` |
|     - | 3164 | `" $params = array();"` |
|     - | 3165 | `" $pos = 0;"` |
|     - | 3166 | `" foreach(__reflect_sig_split($sig) as $part){"` |
|     - | 3167 | `"  $deftext = null;"` |
|     - | 3168 | `"  $q = false;"` |
|     - | 3169 | `"  $n = strlen($part);"` |
|     - | 3170 | `"  for($k = 0; $k < $n; $k++){"` |
|     - | 3171 | `"   $ch = $part[$k];"` |
|     - | 3172 | `"   if($q){"` |
|     - | 3173 | `"    if($ch === chr(92)){ $k++; }"` |
|     - | 3174 | `"    else if($ch === chr(39)){ $q = false; }"` |
|     - | 3175 | `"   }else if($ch === chr(39)){ $q = true; }"` |
|     - | 3176 | `"   else if($ch === '=' ){"` |
|     - | 3177 | `"    $deftext = trim(substr($part, $k + 1));"` |
|     - | 3178 | `"    $part = trim(substr($part, 0, $k));"` |
|     - | 3179 | `"    break;"` |
|     - | 3180 | `"   }"` |
|     - | 3181 | `"  }"` |
|     - | 3182 | `"  $variadic = strpos($part, '...') !== false;"` |
|     - | 3183 | `"  $byref = strpos($part, '&') !== false;"` |
|     - | 3184 | `"  $d = strpos($part, '$');"` |
|     - | 3185 | `"  $name = $d === false ? $part : substr($part, $d + 1);"` |
|     - | 3186 | `"  $typetext = null;"` |
|     - | 3187 | `"  $sp = strpos($part, ' ');"` |
|     - | 3188 | `"  if($sp !== false && $d !== false && $sp < $d){ $typetext = substr($part, 0, $sp); }"` |
|     - | 3189 | `"  $nullable = $typetext !== null && ($typetext[0] === '?' \|\| stripos($typetext, 'null') !== false);"` |
|     - | 3190 | `"  $params[] = array('name' => $name, 'pos' => $pos, 'byref' => $byref,"` |
|     - | 3191 | `"   'variadic' => $variadic, 'hasdef' => $deftext !== null, 'nullable' => $nullable,"` |
|     - | 3192 | `"   'promoted' => false, 'typetext' => $typetext, 'attrs' => array(), 'deftext' => $deftext);"` |
|     - | 3193 | `"  $pos++;"` |
|     - | 3194 | `" }"` |
|     - | 3195 | `" return $params;"` |
|     - | 3196 | `"}"` |
|     - | 3197 | `"function __reflect_sig_fixup($i){"` |
|     - | 3198 | `" if($i === null){ return $i; }"` |
|     - | 3199 | `" if(isset($i['ret2'])){ $i['rettext'] = $i['ret2']; }"` |
|     - | 3200 | `" if(!isset($i['sig']) \|\| $i['sig'] === ''){ return $i; }"` |
|     - | 3201 | `" $i['params'] = __reflect_parse_sig($i['sig']);"` |
|     - | 3202 | `" $i['minarg'] = -1;"` |
|     - | 3203 | `" $v = false;"` |
|     - | 3204 | `" foreach($i['params'] as $p){ if($p['variadic']){ $v = true; } }"` |
|     - | 3205 | `" $i['variadic'] = $v;"` |
|     - | 3206 | `" return $i;"` |
|     - | 3207 | `"}"` |
|     - | 3208 | `;` |
|     - | 3209 | `/*` |
|     - | 3210 | ` * Chunk 9: PHP's Reflection export format (__toString on every Reflector).` |
|     - | 3211 | ` * Built entirely from the public reflection API of the target objects.` |
|     - | 3212 | ` */` |
|     - | 3213 | `static const char zReflectLib9[] =` |
|     - | 3214 | `"function __reflect_export_value($v){"` |
|     - | 3215 | `" if($v === null){ return 'NULL'; }"` |
|     - | 3216 | `" if($v === true){ return 'true'; }"` |
|     - | 3217 | `" if($v === false){ return 'false'; }"` |
|     - | 3218 | `" if(is_string($v)){ return chr(39).$v.chr(39); }"` |
|     - | 3219 | `" if(is_array($v)){"` |
|     - | 3220 | `"  $parts = array();"` |
|     - | 3221 | `"  $isList = true;"` |
|     - | 3222 | `"  $next = 0;"` |
|     - | 3223 | `"  foreach($v as $k => $x){"` |
|     - | 3224 | `"   if($k !== $next){ $isList = false; break; }"` |
|     - | 3225 | `"   $next++;"` |
|     - | 3226 | `"  }"` |
|     - | 3227 | `"  foreach($v as $k => $x){"` |
|     - | 3228 | `"   $parts[] = $isList ? __reflect_export_value($x)"` |
|     - | 3229 | `"    : (__reflect_export_value($k).' => '.__reflect_export_value($x));"` |
|     - | 3230 | `"  }"` |
|     - | 3231 | `"  return '['.implode(', ', $parts).']';"` |
|     - | 3232 | `" }"` |
|     - | 3233 | `" return (string)$v;"` |
|     - | 3234 | `"}"` |
|     - | 3235 | `"function __reflect_export_param($p){"` |
|     - | 3236 | `" $s = 'Parameter #'.$p->getPosition().' [ <'.($p->isOptional() ? 'optional' : 'required').'> ';"` |
|     - | 3237 | `" $t = $p->getType();"` |
|     - | 3238 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3239 | `" if($p->isPassedByReference()){ $s .= '&'; }"` |
|     - | 3240 | `" if($p->isVariadic()){ $s .= '...'; }"` |
|     - | 3241 | `" $s .= '$'.$p->getName();"` |
|     - | 3242 | `" if($p->isDefaultValueAvailable()){"` |
|     - | 3243 | `"  try{ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3244 | `"  catch(ReflectionException $e){ $s .= ' = <default>'; }"` |
|     - | 3245 | `" }"` |
|     - | 3246 | `" return $s.' ]';"` |
|     - | 3247 | `"}"` |
|     - | 3248 | `"function __reflect_export_prop($p){"` |
|     - | 3249 | `" $s = 'Property [ ';"` |
|     - | 3250 | `" $s .= $p->isPrivate() ? 'private ' : ($p->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3251 | `" if($p->isStatic()){ $s .= 'static '; }"` |
|     - | 3252 | `" if($p->isReadOnly()){ $s .= 'readonly '; }"` |
|     - | 3253 | `" $t = $p->getType();"` |
|     - | 3254 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3255 | `" $s .= '$'.$p->getName();"` |
|     - | 3256 | `" if($p->hasDefaultValue()){ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3257 | `" return $s.' ]'.chr(10);"` |
|     - | 3258 | `"}"` |
|     - | 3259 | `"function __reflect_export_cconst($c){"` |
|     - | 3260 | `" $v = $c->getValue();"` |
|     - | 3261 | `" if(is_int($v)){ $t = 'int'; }"` |
|     - | 3262 | `" else if(is_string($v)){ $t = 'string'; }"` |
|     - | 3263 | `" else if(is_float($v)){ $t = 'float'; }"` |
|     - | 3264 | `" else if(is_bool($v)){ $t = 'bool'; }"` |
|     - | 3265 | `" else if(is_array($v)){ $t = 'array'; }"` |
|     - | 3266 | `" else{ $t = 'null'; }"` |
|     - | 3267 | `" $vs = is_array($v) ? 'Array' : (is_bool($v) ? ($v ? '1' : '') : (string)$v);"` |
|     - | 3268 | `" $vis = $c->isPrivate() ? 'private' : ($c->isProtected() ? 'protected' : 'public');"` |
|     - | 3269 | `" return 'Constant [ '.$vis.' '.$t.' '.$c->name.' ] { '.$vs.' }'.chr(10);"` |
|     - | 3270 | `"}"` |
|     - | 3271 | `"function __reflect_export_fnabs($r, $indent){"` |
|     - | 3272 | `" $tags = $r->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3273 | `" if($r instanceof ReflectionMethod){"` |
|     - | 3274 | `"  if($r->isConstructor()){ $tags .= ', ctor'; }"` |
|     - | 3275 | `"  else if($r->isDestructor()){ $tags .= ', dtor'; }"` |
|     - | 3276 | `"  $decl = $r->getDeclaringClass()->name;"` |
|     - | 3277 | `"  if(strtolower($decl) !== strtolower($r->class)){ $tags .= ', inherits '.$decl; }"` |
|     - | 3278 | `"  else if($r->hasPrototype()){ $tags .= ', prototype '.$r->getPrototype()->class; }"` |
|     - | 3279 | `"  $head = 'Method [ <'.$tags.'> ';"` |
|     - | 3280 | `"  if($r->isAbstract()){ $head .= 'abstract '; }"` |
|     - | 3281 | `"  if($r->isFinal()){ $head .= 'final '; }"` |
|     - | 3282 | `"  if($r->isStatic()){ $head .= 'static '; }"` |
|     - | 3283 | `"  $head .= $r->isPrivate() ? 'private ' : ($r->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3284 | `"  $head .= 'method '.$r->name.' ]';"` |
|     - | 3285 | `" }else{"` |
|     - | 3286 | `"  $kind = $r->isClosure() ? 'Closure' : 'Function';"` |
|     - | 3287 | `"  $head = $kind.' [ <'.$tags.'> function '.$r->name.' ]';"` |
|     - | 3288 | `" }"` |
|     - | 3289 | `" $s = $head.' {'.chr(10);"` |
|     - | 3290 | `" if(!$r->isInternal()){"` |
|     - | 3291 | `"  $s .= '  @@ '.$r->getFileName().' '.$r->getStartLine().' - '.$r->getEndLine().chr(10);"` |
|     - | 3292 | `" }"` |
|     - | 3293 | `" $ps = $r->getParameters();"` |
|     - | 3294 | `" $ret = $r->getReturnType();"` |
|     - | 3295 | `" if(count($ps) > 0 \|\| $ret !== null){"` |
|     - | 3296 | `"  $s .= chr(10).'  - Parameters ['.count($ps).'] {'.chr(10);"` |
|     - | 3297 | `"  foreach($ps as $p){ $s .= '    '.__reflect_export_param($p).chr(10); }"` |
|     - | 3298 | `"  $s .= '  }'.chr(10);"` |
|     - | 3299 | `" }"` |
|     - | 3300 | `" if($ret !== null){ $s .= '  - Return [ '.(string)$ret.' ]'.chr(10); }"` |
|     - | 3301 | `" $s .= '}'.chr(10);"` |
|     - | 3302 | `" if($indent === ''){ return $s; }"` |
|     - | 3303 | `" $lines = explode(chr(10), $s);"` |
|     - | 3304 | `" $out = '';"` |
|     - | 3305 | `" $n = count($lines);"` |
|     - | 3306 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3307 | `"  if($lines[$k] === '' && $k === $n - 1){ break; }"` |
|     - | 3308 | `"  $out .= ($lines[$k] === '' ? '' : $indent.$lines[$k]).chr(10);"` |
|     - | 3309 | `" }"` |
|     - | 3310 | `" return $out;"` |
|     - | 3311 | `"}"` |
|     - | 3312 | `"function __reflect_export_class($rc){"` |
|     - | 3313 | `" $tags = $rc->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3314 | `" if($rc->isInterface()){"` |
|     - | 3315 | `"  $head = 'Interface [ <'.$tags.'> interface '.$rc->name.' ]';"` |
|     - | 3316 | `" }else{"` |
|     - | 3317 | `"  $mods = '';"` |
|     - | 3318 | `"  if($rc->isAbstract()){ $mods .= 'abstract '; }"` |
|     - | 3319 | `"  if($rc->isFinal()){ $mods .= 'final '; }"` |
|     - | 3320 | `"  $head = 'Class [ <'.$tags.'> '.$mods.'class '.$rc->name;"` |
|     - | 3321 | `"  $par = $rc->getParentClass();"` |
|     - | 3322 | `"  if($par !== false){ $head .= ' extends '.$par->name; }"` |
|     - | 3323 | `"  $ifs = $rc->getInterfaceNames();"` |
|     - | 3324 | `"  if(count($ifs) > 0){ $head .= ' implements '.implode(', ', $ifs); }"` |
|     - | 3325 | `"  $head .= ' ]';"` |
|     - | 3326 | `" }"` |
|     - | 3327 | `" $s = $head.' {'.chr(10);"` |
|     - | 3328 | `" if(!$rc->isInternal()){"` |
|     - | 3329 | `"  $s .= '  @@ '.$rc->getFileName().' '.$rc->getStartLine().'-'.$rc->getEndLine().chr(10);"` |
|     - | 3330 | `" }"` |
|     - | 3331 | `" $consts = $rc->getReflectionConstants();"` |
|     - | 3332 | `" $s .= chr(10).'  - Constants ['.count($consts).'] {'.chr(10);"` |
|     - | 3333 | `" foreach($consts as $c){ $s .= '    '.__reflect_export_cconst($c); }"` |
|     - | 3334 | `" $s .= '  }'.chr(10);"` |
|     - | 3335 | `" $sp = array();"` |
|     - | 3336 | `" $ip = array();"` |
|     - | 3337 | `" foreach($rc->getProperties() as $p){"` |
|     - | 3338 | `"  if($p->isStatic()){ $sp[] = $p; }else{ $ip[] = $p; }"` |
|     - | 3339 | `" }"` |
|     - | 3340 | `" $sm = array();"` |
|     - | 3341 | `" $im = array();"` |
|     - | 3342 | `" foreach($rc->getMethods() as $m){"` |
|     - | 3343 | `"  if($m->isStatic()){ $sm[] = $m; }else{ $im[] = $m; }"` |
|     - | 3344 | `" }"` |
|     - | 3345 | `" $s .= chr(10).'  - Static properties ['.count($sp).'] {'.chr(10);"` |
|     - | 3346 | `" foreach($sp as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3347 | `" $s .= '  }'.chr(10);"` |
|     - | 3348 | `" $s .= chr(10).'  - Static methods ['.count($sm).'] {'.chr(10);"` |
|     - | 3349 | `" $first = true;"` |
|     - | 3350 | `" foreach($sm as $m){"` |
|     - | 3351 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3352 | `"  $first = false;"` |
|     - | 3353 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3354 | `" }"` |
|     - | 3355 | `" $s .= '  }'.chr(10);"` |
|     - | 3356 | `" $s .= chr(10).'  - Properties ['.count($ip).'] {'.chr(10);"` |
|     - | 3357 | `" foreach($ip as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3358 | `" $s .= '  }'.chr(10);"` |
|     - | 3359 | `" $s .= chr(10).'  - Methods ['.count($im).'] {'.chr(10);"` |
|     - | 3360 | `" $first = true;"` |
|     - | 3361 | `" foreach($im as $m){"` |
|     - | 3362 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3363 | `"  $first = false;"` |
|     - | 3364 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3365 | `" }"` |
|     - | 3366 | `" $s .= '  }'.chr(10);"` |
|     - | 3367 | `" return $s.'}'.chr(10);"` |
|     - | 3368 | `"}"` |
|     - | 3369 | `;` |
|     - | 3370 | `/*` |
|     - | 3371 | ` * Register the __reflect_* thunks and compile the Reflection library.` |
|     - | 3372 | ` * Called from PH7_VmInit while pVm->bCompilingBuiltin is set, right after` |
|     - | 3373 | ` * the core builtin chunks (Exception and friends must exist already).` |
|     - | 3374 | ` */` |
|  3826 | 3375 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|     5 | 3376 | `{` |
|     - | 3377 | `	static const struct {` |
|     - | 3378 | `		const char *zName;` |
|     - | 3379 | `		ProchHostFunction xFunc;` |
|     - | 3380 | `	} aFunc[] = {` |
|     - | 3381 | `		{ "__reflect_class_info",     vm_builtin_reflect_class_info },` |
|     - | 3382 | `		{ "__reflect_const_value",    vm_builtin_reflect_const_value },` |
|     - | 3383 | `		{ "__reflect_static_value",   vm_builtin_reflect_static_value },` |
|     - | 3384 | `		{ "__reflect_static_set",     vm_builtin_reflect_static_set },` |
|     - | 3385 | `		{ "__reflect_prop_default",   vm_builtin_reflect_prop_default },` |
|     - | 3386 | `		{ "__reflect_new_instance",   vm_builtin_reflect_new_instance },` |
|     - | 3387 | `		{ "__reflect_new_no_ctor",    vm_builtin_reflect_new_no_ctor },` |
|     - | 3388 | `		{ "__reflect_func_info",      vm_builtin_reflect_func_info },` |
|     - | 3389 | `		{ "__reflect_param_default",  vm_builtin_reflect_param_default },` |
|     - | 3390 | `		{ "__reflect_param_defconst", vm_builtin_reflect_param_defconst },` |
|     - | 3391 | `		{ "__reflect_invoke",         vm_builtin_reflect_invoke },` |
|     - | 3392 | `		{ "__reflect_closure",        vm_builtin_reflect_closure },` |
|     - | 3393 | `		{ "__reflect_prop_read",      vm_builtin_reflect_prop_read },` |
|     - | 3394 | `		{ "__reflect_prop_write",     vm_builtin_reflect_prop_write },` |
|     - | 3395 | `		{ "__reflect_prop_state",     vm_builtin_reflect_prop_state },` |
|     - | 3396 | `		{ "__reflect_dyn_props",      vm_builtin_reflect_dyn_props },` |
|     - | 3397 | `		{ "__reflect_gen_info",       vm_builtin_reflect_gen_info },` |
|     - | 3398 | `		{ "__reflect_gen_exec",       vm_builtin_reflect_gen_exec },` |
|     - | 3399 | `		{ "__reflect_const_info",     vm_builtin_reflect_const_info },` |
|     - | 3400 | `		{ "__reflect_ref_id",         vm_builtin_reflect_ref_id },` |
|     - | 3401 | `		{ "__reflect_attr_args",      vm_builtin_reflect_attr_args },` |
|     - | 3402 | `	};` |
|     - | 3403 | `	sxu32 n;` |
|     - | 3404 | `	sxi32 rc;` |
| 84177 | 3405 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 80351 | 3406 | `		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);` |
| 40178 | 3407 | `	}` |
|  3831 | 3408 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib1, sizeof(zReflectLib1)-1);` |
|  3831 | 3409 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3410 | `		return rc;` |
|     - | 3411 | `	}` |
|  3831 | 3412 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib2, sizeof(zReflectLib2)-1);` |
|  3831 | 3413 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3414 | `		return rc;` |
|     - | 3415 | `	}` |
|  3831 | 3416 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib3, sizeof(zReflectLib3)-1);` |
|  3831 | 3417 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3418 | `		return rc;` |
|     - | 3419 | `	}` |
|  3831 | 3420 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib4, sizeof(zReflectLib4)-1);` |
|  3831 | 3421 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3422 | `		return rc;` |
|     - | 3423 | `	}` |
|  3831 | 3424 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib5, sizeof(zReflectLib5)-1);` |
|  3831 | 3425 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3426 | `		return rc;` |
|     - | 3427 | `	}` |
|  3831 | 3428 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib6, sizeof(zReflectLib6)-1);` |
|  3831 | 3429 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3430 | `		return rc;` |
|     - | 3431 | `	}` |
|  3831 | 3432 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib7, sizeof(zReflectLib7)-1);` |
|  3831 | 3433 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3434 | `		return rc;` |
|     - | 3435 | `	}` |
|  3831 | 3436 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib8, sizeof(zReflectLib8)-1);` |
|  3831 | 3437 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3438 | `		return rc;` |
|     - | 3439 | `	}` |
|  3831 | 3440 | `	return PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib9, sizeof(zReflectLib9)-1);` |
|  1918 | 3441 | `}` |
|     - | 3442 |  |
