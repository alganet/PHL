# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1043/1211 lines (86.13%)

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
| 17570 |   63 | `static void ReflectMapAddBool(ph7_context *pCtx, ph7_value *pMap, const char *zKey, int b)` |
|     4 |   64 | `{` |
| 17574 |   65 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 17574 |   66 | `	if( p == 0 ){ return; }` |
| 17574 |   67 | `	ph7_value_bool(p, b);` |
| 17574 |   68 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  8789 |   69 | `}` |
|  5264 |   70 | `static void ReflectMapAddInt(ph7_context *pCtx, ph7_value *pMap, const char *zKey, sxi64 iVal)` |
|     4 |   71 | `{` |
|  5268 |   72 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  5268 |   73 | `	if( p == 0 ){ return; }` |
|  5268 |   74 | `	ph7_value_int64(p, iVal);` |
|  5268 |   75 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  2636 |   76 | `}` |
|  5172 |   77 | `static void ReflectMapAddStr(ph7_context *pCtx, ph7_value *pMap, const char *zKey,` |
|     - |   78 | `	const char *zVal, int nVal)` |
|     4 |   79 | `{` |
|  5176 |   80 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  5176 |   81 | `	if( p == 0 ){ return; }` |
|  5176 |   82 | `	ph7_value_string(p, zVal, nVal);` |
|  5176 |   83 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  2590 |   84 | `}` |
|  1632 |   85 | `static void ReflectMapAddNull(ph7_context *pCtx, ph7_value *pMap, const char *zKey)` |
|     3 |   86 | `{` |
|  1635 |   87 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  1635 |   88 | `	if( p == 0 ){ return; }` |
|  1635 |   89 | `	ph7_value_null(p);` |
|  1635 |   90 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|   819 |   91 | `}` |
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
|  2254 |  104 | `static void ReflectMapAddAttrs(ph7_context *pCtx, ph7_value *pMap, SySet *pAttrs)` |
|     3 |  105 | `{` |
|  2257 |  106 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|  2257 |  107 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|     - |  108 | `	sxu32 n;` |
|  2257 |  109 | `	if( pList == 0 ){` |
|   ! 0 |  110 | `		return;` |
|     - |  111 | `	}` |
|  2367 |  112 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|   111 |  113 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|   111 |  114 | `		if( pMeta == 0 ){ break; }` |
|   111 |  115 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aA[n].sName), (int)SyStringLength(&aA[n].sName));` |
|   111 |  116 | `		ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)aA[n].nLine);` |
|   111 |  117 | `		ph7_array_add_elem(pList, 0, pMeta);` |
|    56 |  118 | `	}` |
|  2257 |  119 | `	ph7_array_add_strkey_elem(pMap, "attrs", pList);` |
|  1130 |  120 | `}` |
|     - |  121 | `/* Emit a doc-comment field: the text when present, else boolean false` |
|     - |  122 | ` * (getDocComment()'s exact return contract). */` |
|   924 |  123 | `static void ReflectMapAddDoc(ph7_context *pCtx, ph7_value *pMap, const SyString *pDoc)` |
|     3 |  124 | `{` |
|   927 |  125 | `	if( SyStringLength(pDoc) > 0 ){` |
|    21 |  126 | `		ReflectMapAddStr(pCtx, pMap, "doc", SyStringData(pDoc), (int)SyStringLength(pDoc));` |
|    11 |  127 | `	}else{` |
|   907 |  128 | `		ReflectMapAddBool(pCtx, pMap, "doc", 0);` |
|     - |  129 | `	}` |
|   927 |  130 | `}` |
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
|     - |  385 | `			/* Forward: hAttr now iterates in DECLARATION order (its inserts are` |
|     - |  386 | `			 * tail inserts), so members come out in the order php reports them.` |
|     - |  387 | `			 * This walked aTmp backwards to undo the table's old head-insert` |
|     - |  388 | `			 * (LIFO) storage; with that reversal gone from the table, reversing` |
|     - |  389 | `			 * here would emit members back to front. The METHOD loop below keeps` |
|     - |  390 | `			 * its reverse walk — hMethod is still a head-insert table. */` |
|   344 |  391 | `			for( nT = 0 ; nT < SySetUsed(&aTmp) ; nT++ ){` |
|   181 |  392 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT);` |
|   181 |  393 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;` |
|   181 |  394 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|   181 |  395 | `				ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|   181 |  396 | `				if( pMeta == 0 ){ break; }` |
|   181 |  397 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pAttr->iProtection);` |
|   181 |  398 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   181 |  399 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pAttr->nLine);` |
|   181 |  400 | `				ReflectMapAddDoc(pCtx, pMeta, &pAttr->sDoc);` |
|   181 |  401 | `				ReflectMapAddAttrs(pCtx, pMeta, &pAttr->aAttrs);` |
|   181 |  402 | `				ReflectMapAddBool(pCtx, pMeta, "typed", (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);` |
|   181 |  403 | `				if( SyStringLength(&pAttr->sTypeName) > 0 ){` |
|    67 |  404 | `					ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&pAttr->sTypeName),` |
|    44 |  405 | `						(int)SyStringLength(&pAttr->sTypeName));` |
|    23 |  406 | `				}else{` |
|   137 |  407 | `					ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - |  408 | `				}` |
|   181 |  409 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|    41 |  410 | `					ReflectMapAddBool(pCtx, pMeta, "final", (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|    41 |  411 | `					ReflectMapAddBool(pCtx, pMeta, "enumcase", (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0);` |
|    41 |  412 | `					ReflectMapAddDyn(pCtx, pConsts, &pAttr->sName, pMeta);` |
|    21 |  413 | `				}else{` |
|   141 |  414 | `					ReflectMapAddBool(pCtx, pMeta, "static", (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   141 |  415 | `					ReflectMapAddBool(pCtx, pMeta, "readonly", (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0);` |
|   141 |  416 | `					ReflectMapAddBool(pCtx, pMeta, "privset", (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) != 0);` |
|   141 |  417 | `					ReflectMapAddBool(pCtx, pMeta, "protset", (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET) != 0);` |
|   141 |  418 | `					ReflectMapAddBool(pCtx, pMeta, "hookget", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) != 0);` |
|   141 |  419 | `					ReflectMapAddBool(pCtx, pMeta, "hookset", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) != 0);` |
|   141 |  420 | `					ReflectMapAddBool(pCtx, pMeta, "virtual", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) != 0);` |
|   141 |  421 | `					ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&pAttr->aByteCode) > 0);` |
|   141 |  422 | `					ReflectMapAddDyn(pCtx, pProps, &pAttr->sName, pMeta);` |
|     - |  423 | `				}` |
|    91 |  424 | `			}` |
|     - |  425 | `			/* --- Methods. The reported name is the hash-entry key: trait` |
|     - |  426 | `			 * aliasing installs a shallow copy under the alias name while` |
|     - |  427 | `			 * sFunc.sName keeps the original, and PHP reports the alias. --- */` |
|   164 |  428 | `			SySetReset(&aTmp);` |
|   164 |  429 | `			SyHashResetLoopCursor(&pLevel->hMethod);` |
|   444 |  430 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){` |
|   281 |  431 | `				ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|   281 |  432 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|   281 |  433 | `				if( iLevel == 0 ){` |
|     - |  434 | `					sxu32 j;` |
|   259 |  435 | `					for( j = 1 ; j < nChain ; j++ ){` |
|   111 |  436 | `						if( aChain[j] == pDecl ){ break; }` |
|    29 |  437 | `					}` |
|   203 |  438 | `					if( j < nChain ){ continue; }` |
|    75 |  439 | `				}else{` |
|     - |  440 | `					SyHashEntry *pSub;` |
|    79 |  441 | `					if( pDecl != pLevel ){ continue; }` |
|    57 |  442 | `					pSub = SyHashGet(&pClass->hMethod, pEntry->pKey, pEntry->nKeyLen);` |
|    57 |  443 | `					if( pSub == 0 ){` |
|     - |  444 | `						/* Not in the subclass table: inheritance skips private` |
|     - |  445 | `						 * methods, but PHP still reports them on the subclass` |
|     - |  446 | `						 * (Zend copies privates into the child function table). */` |
|   ! 0 |  447 | `						if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|   ! 0 |  448 | `							continue;` |
|   ! 0 |  449 | `						}` |
|    57 |  450 | `					}else if( pSub->pUserData != (void *)pMeth ){` |
|     - |  451 | `						/* Overridden below this level: already reported */` |
|     3 |  452 | `						continue;` |
|     - |  453 | `					}` |
|     - |  454 | `				}` |
|   203 |  455 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     1 |  456 | `			}` |
|   366 |  457 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|   203 |  458 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|   203 |  459 | `				ph7_class_method *pMeth = (ph7_class_method *)pE->pUserData;` |
|   203 |  460 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|     - |  461 | `				ph7_value *pMeta;` |
|     - |  462 | `				SyString sKey;` |
|     - |  463 | `				int bIsAlias;` |
|   203 |  464 | `				SyStringInitFromBuf(&sKey, (const char *)pE->pKey, pE->nKeyLen);` |
|   405 |  465 | `				bIsAlias = (sKey.nByte != SyStringLength(&pMeth->sFunc.sName)` |
|   202 |  466 | `				 \|\| SyMemcmp(sKey.zString, SyStringData(&pMeth->sFunc.sName), sKey.nByte) != 0);` |
|   202 |  467 | `				if( sKey.nByte == sizeof("__construct")-1` |
|   119 |  468 | `				 && SyMemcmp(sKey.zString, "__construct", sKey.nByte) == 0 ){` |
|    27 |  469 | `					if( iCtorVis == 0 ){` |
|    27 |  470 | `						iCtorVis = pMeth->iProtection;` |
|    13 |  471 | `					}` |
|    27 |  472 | `					if( bIsAlias ){` |
|     - |  473 | `						/* Mount-time alias for a legacy class-name constructor:` |
|     - |  474 | `						 * the method is already listed under its declared name. */` |
|   ! 0 |  475 | `						continue;` |
|     - |  476 | `					}` |
|   190 |  477 | `				}else if( sKey.nByte == sizeof("__clone")-1` |
|   102 |  478 | `				 && SyMemcmp(sKey.zString, "__clone", sKey.nByte) == 0 ){` |
|   ! 0 |  479 | `					if( iCloneVis == 0 ){` |
|   ! 0 |  480 | `						iCloneVis = pMeth->iProtection;` |
|   ! 0 |  481 | `					}` |
|   176 |  482 | `				}else if( iCtorVis == 0` |
|   144 |  483 | `				 && sKey.nByte == SyStringLength(&pClass->sName)` |
|    59 |  484 | `				 && SyMemcmp(sKey.zString, SyStringData(&pClass->sName), sKey.nByte) == 0 ){` |
|     - |  485 | `					/* Legacy class-name constructor before the mount alias exists */` |
|   ! 0 |  486 | `					iCtorVis = pMeth->iProtection;` |
|   ! 0 |  487 | `				}` |
|   203 |  488 | `				pMeta = ph7_context_new_array(pCtx);` |
|   203 |  489 | `				if( pMeta == 0 ){ break; }` |
|   203 |  490 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pMeth->iProtection);` |
|   203 |  491 | `				ReflectMapAddBool(pCtx, pMeta, "static", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   203 |  492 | `				ReflectMapAddBool(pCtx, pMeta, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|   203 |  493 | `				ReflectMapAddBool(pCtx, pMeta, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   203 |  494 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   203 |  495 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pMeth->nLine);` |
|   203 |  496 | `				ReflectMapAddDyn(pCtx, pMethods, &sKey, pMeta);` |
|   102 |  497 | `			}` |
|    83 |  498 | `		}` |
|   134 |  499 | `		SySetRelease(&aTmp);` |
|     - |  500 | `	}` |
|   134 |  501 | `	ReflectMapAddInt(pCtx, pInfo, "ctorvis", (sxi64)iCtorVis);` |
|   134 |  502 | `	ReflectMapAddInt(pCtx, pInfo, "clonevis", (sxi64)iCloneVis);` |
|   134 |  503 | `	ph7_array_add_strkey_elem(pInfo, "consts", pConsts);` |
|   134 |  504 | `	ph7_array_add_strkey_elem(pInfo, "props", pProps);` |
|   134 |  505 | `	ph7_array_add_strkey_elem(pInfo, "methods", pMethods);` |
|   134 |  506 | `	ph7_result_value(pCtx, pInfo);` |
|   134 |  507 | `	return PH7_OK;` |
|    76 |  508 | `}` |
|     - |  509 | `/*` |
|     - |  510 | ` * mixed __reflect_const_value(string $class, string $name)` |
|     - |  511 | ` * Value of a class constant. The PHP layer guarantees existence.` |
|     - |  512 | ` */` |
|    42 |  513 | `static int vm_builtin_reflect_const_value(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  514 | `{` |
|     - |  515 | `	ph7_class *pClass;` |
|     - |  516 | `	ph7_class_attr *pAttr;` |
|     - |  517 | `	ph7_value *pValue;` |
|    42 |  518 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    42 |  519 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|    43 |  520 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|   ! 0 |  521 | `		ph7_result_null(pCtx);` |
|   ! 0 |  522 | `		return PH7_OK;` |
|     - |  523 | `	}` |
|     - |  524 | `	/* Constant slots are evaluated lazily on first access */` |
|    43 |  525 | `	if( PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr) != SXRET_OK ){` |
|     - |  526 | `		/* Initializer raised: the throw is in flight; report null here */` |
|   ! 0 |  527 | `		ph7_result_null(pCtx);` |
|   ! 0 |  528 | `		return PH7_OK;` |
|     - |  529 | `	}` |
|    43 |  530 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|    43 |  531 | `	if( pValue ){` |
|    43 |  532 | `		ph7_result_value(pCtx, pValue);` |
|    22 |  533 | `	}else{` |
|   ! 0 |  534 | `		ph7_result_null(pCtx);` |
|     - |  535 | `	}` |
|    43 |  536 | `	return PH7_OK;` |
|    22 |  537 | `}` |
|     - |  538 | `/*` |
|     - |  539 | ` * mixed __reflect_static_value(string $class, string $name)` |
|     - |  540 | ` * Current value of a static property (visibility ignored).` |
|     - |  541 | ` */` |
|    12 |  542 | `static int vm_builtin_reflect_static_value(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  543 | `{` |
|     - |  544 | `	ph7_class *pClass;` |
|     - |  545 | `	ph7_class_attr *pAttr;` |
|     - |  546 | `	ph7_value *pValue;` |
|    12 |  547 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    12 |  548 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|    13 |  549 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|   ! 0 |  550 | `		ph7_result_null(pCtx);` |
|   ! 0 |  551 | `		return PH7_OK;` |
|     - |  552 | `	}` |
|     - |  553 | `	{` |
|     - |  554 | `		/* Uninitialized typed static: same Error the VM raises on read */` |
|    13 |  555 | `		SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|    13 |  556 | `		if( pSlot ){` |
|   ! 0 |  557 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|   ! 0 |  558 | `			if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|   ! 0 |  559 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|   ! 0 |  560 | `				return PH7_VmThrowException(pCtx, "Error",` |
|     - |  561 | `					"Typed static property %z::$%z must not be accessed before initialization",` |
|   ! 0 |  562 | `					&pDecl->sName, &pAttr->sName);` |
|     - |  563 | `			}` |
|   ! 0 |  564 | `		}` |
|     - |  565 | `	}` |
|    13 |  566 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|    13 |  567 | `	if( pValue ){` |
|    13 |  568 | `		ph7_result_value(pCtx, pValue);` |
|     7 |  569 | `	}else{` |
|   ! 0 |  570 | `		ph7_result_null(pCtx);` |
|     - |  571 | `	}` |
|    13 |  572 | `	return PH7_OK;` |
|     7 |  573 | `}` |
|     - |  574 | `/*` |
|     - |  575 | ` * bool __reflect_static_set(string $class, string $name, mixed $value)` |
|     - |  576 | ` * Overwrite a static property's shared slot (visibility ignored).` |
|     - |  577 | ` */` |
|     4 |  578 | `static int vm_builtin_reflect_static_set(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  579 | `{` |
|     - |  580 | `	ph7_class *pClass;` |
|     - |  581 | `	ph7_class_attr *pAttr;` |
|     - |  582 | `	ph7_value *pValue;` |
|     4 |  583 | `	if( nArg < 3 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|     4 |  584 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|     5 |  585 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|   ! 0 |  586 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  587 | `		return PH7_OK;` |
|     - |  588 | `	}` |
|     5 |  589 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|     5 |  590 | `	if( pValue == 0 ){` |
|   ! 0 |  591 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  592 | `		return PH7_OK;` |
|     - |  593 | `	}` |
|     - |  594 | `	{` |
|     5 |  595 | `		sxi32 rc = ReflectEnforceStore(pCtx, pAttr->nIdx, apArg[2]);` |
|     5 |  596 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  597 | `			return rc;` |
|     - |  598 | `		}` |
|     - |  599 | `	}` |
|     5 |  600 | `	PH7_MemObjStore(apArg[2], pValue);` |
|     5 |  601 | `	ph7_result_bool(pCtx, 1);` |
|     5 |  602 | `	return PH7_OK;` |
|     3 |  603 | `}` |
|     - |  604 | `/*` |
|     - |  605 | ` * mixed __reflect_prop_default(string $class, string $name)` |
|     - |  606 | ` * Evaluate a non-static property's compiled default expression` |
|     - |  607 | ` * (null when the property has no default).` |
|     - |  608 | ` */` |
|    24 |  609 | `static int vm_builtin_reflect_prop_default(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  610 | `{` |
|     - |  611 | `	ph7_class *pClass;` |
|     - |  612 | `	ph7_class_attr *pAttr;` |
|     - |  613 | `	ph7_value sValue;` |
|    24 |  614 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    24 |  615 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|    24 |  616 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) != 0` |
|    25 |  617 | `	 \|\| SySetUsed(&pAttr->aByteCode) < 1 ){` |
|     3 |  618 | `		ph7_result_null(pCtx);` |
|     3 |  619 | `		return PH7_OK;` |
|     - |  620 | `	}` |
|    23 |  621 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     - |  622 | `	/* Same evaluation path the VM uses for omitted call arguments */` |
|    23 |  623 | `	VmLocalExec(pCtx->pVm, &pAttr->aByteCode, &sValue, FALSE);` |
|    23 |  624 | `	ph7_result_value(pCtx, &sValue);` |
|    23 |  625 | `	PH7_MemObjRelease(&sValue);` |
|    23 |  626 | `	return PH7_OK;` |
|    13 |  627 | `}` |
|     - |  628 | `/*` |
|     - |  629 | ` * Collect a PHP array's values into a ph7_value* set (call arguments).` |
|     - |  630 | ` * When ppNames is non-NULL, string keys become named arguments: a name` |
|     - |  631 | ` * map is lazily allocated (like call_user_func_array's) with one entry` |
|     - |  632 | ` * per collected slot, empty entries meaning positional.` |
|     - |  633 | ` */` |
|    46 |  634 | `static sxi32 ReflectCollectArgs(ph7_context *pCtx, ph7_value *pArray, SySet *pOut, SyString **ppNames)` |
|     1 |  635 | `{` |
|     - |  636 | `	ph7_hashmap *pMap;` |
|     - |  637 | `	ph7_hashmap_node *pEntry;` |
|    47 |  638 | `	SyString *aNames = 0;` |
|    47 |  639 | `	sxu32 nSlot = 0;` |
|     - |  640 | `	sxu32 n;` |
|    47 |  641 | `	if( ppNames ){` |
|    27 |  642 | `		*ppNames = 0;` |
|    13 |  643 | `	}` |
|    47 |  644 | `	if( !ph7_value_is_array(pArray) ){` |
|   ! 0 |  645 | `		return SXRET_OK;` |
|     - |  646 | `	}` |
|    47 |  647 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|    47 |  648 | `	pEntry = pMap->pFirst;` |
|   107 |  649 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    61 |  650 | `		ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|    61 |  651 | `		if( pValue ){` |
|    61 |  652 | `			if( ppNames && pEntry->iType == HASHMAP_BLOB_NODE ){` |
|     3 |  653 | `				if( aNames == 0 ){` |
|     4 |  654 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     2 |  655 | `						pMap->nEntry * sizeof(SyString));` |
|     3 |  656 | `					if( aNames ){` |
|     3 |  657 | `						SyZero(aNames, pMap->nEntry * sizeof(SyString));` |
|     1 |  658 | `					}` |
|     1 |  659 | `				}` |
|     3 |  660 | `				if( aNames ){` |
|     3 |  661 | `					SyStringInitFromBuf(&aNames[nSlot],` |
|     - |  662 | `						SyBlobData(&pEntry->xKey.sKey), SyBlobLength(&pEntry->xKey.sKey));` |
|     1 |  663 | `				}` |
|     1 |  664 | `			}` |
|    61 |  665 | `			SySetPut(pOut, (const void *)&pValue);` |
|    61 |  666 | `			nSlot++;` |
|    30 |  667 | `		}` |
|    61 |  668 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|    31 |  669 | `	}` |
|    47 |  670 | `	if( ppNames ){` |
|    27 |  671 | `		*ppNames = aNames;` |
|    13 |  672 | `	}` |
|    47 |  673 | `	return SXRET_OK;` |
|    24 |  674 | `}` |
|     - |  675 | `/*` |
|     - |  676 | ` * object __reflect_new_instance(string $class, array $args)` |
|     - |  677 | ` * Instantiate and run the constructor with positional arguments.` |
|     - |  678 | ` * The PHP layer has already validated instantiability and ctor visibility.` |
|     - |  679 | ` */` |
|    30 |  680 | `static int vm_builtin_reflect_new_instance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  681 | `{` |
|    31 |  682 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  683 | `	ph7_class *pClass;` |
|     - |  684 | `	ph7_class_instance *pThis;` |
|     - |  685 | `	ph7_class_method *pCons;` |
|    31 |  686 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pVm, apArg[0])) == 0 ){` |
|   ! 0 |  687 | `		ph7_result_null(pCtx);` |
|   ! 0 |  688 | `		return PH7_OK;` |
|     - |  689 | `	}` |
|    31 |  690 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|    31 |  691 | `	if( pThis == 0 ){` |
|   ! 0 |  692 | `		ph7_result_null(pCtx);` |
|   ! 0 |  693 | `		return PH7_OK;` |
|     - |  694 | `	}` |
|    31 |  695 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|    31 |  696 | `	if( pCons ){` |
|     - |  697 | `		SySet aArg;` |
|     - |  698 | `		sxi32 rc;` |
|    27 |  699 | `		SyString *aNames = 0;` |
|    27 |  700 | `		SySetInit(&aArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    27 |  701 | `		if( nArg > 1 ){` |
|    27 |  702 | `			ReflectCollectArgs(pCtx, apArg[1], &aArg, &aNames);` |
|    13 |  703 | `		}` |
|    27 |  704 | `		if( aNames ){` |
|     - |  705 | `			VmCallArgMap sMap;` |
|     3 |  706 | `			sMap.bHasNamed = 1;` |
|     3 |  707 | `			sMap.bIsNamespaced = 0;` |
|     3 |  708 | `			sMap.bStrict = 0;` |
|     3 |  709 | `			sMap.nTotal = SySetUsed(&aArg);` |
|     3 |  710 | `			sMap.aNames = aNames;` |
|     4 |  711 | `			rc = PH7_VmCallClassMethodMap(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|     2 |  712 | `				(ph7_value **)SySetBasePtr(&aArg), &sMap);` |
|     3 |  713 | `			SyMemBackendFree(&pVm->sAllocator, aNames);` |
|     2 |  714 | `		}else{` |
|    37 |  715 | `			rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|    24 |  716 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|     - |  717 | `		}` |
|    27 |  718 | `		SySetRelease(&aArg);` |
|    27 |  719 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 |  720 | `			PH7_ClassInstanceUnref(pThis);` |
|   ! 0 |  721 | `			return rc;` |
|     - |  722 | `		}` |
|    13 |  723 | `	}` |
|    31 |  724 | `	return ReflectResultObject(pCtx, pThis);` |
|    16 |  725 | `}` |
|     - |  726 | `/*` |
|     - |  727 | ` * object __reflect_new_no_ctor(string $class)` |
|     - |  728 | ` * Instantiate without running the constructor (property defaults still` |
|     - |  729 | ` * apply — PH7_NewClassInstance builds the attribute frame).` |
|     - |  730 | ` */` |
|    68 |  731 | `static int vm_builtin_reflect_new_no_ctor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  732 | `{` |
|     - |  733 | `	ph7_class *pClass;` |
|    69 |  734 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0 ){` |
|   ! 0 |  735 | `		ph7_result_null(pCtx);` |
|   ! 0 |  736 | `		return PH7_OK;` |
|     - |  737 | `	}` |
|    69 |  738 | `	return ReflectResultObject(pCtx, PH7_NewClassInstance(pCtx->pVm, pClass));` |
|    35 |  739 | `}` |
|     - |  740 | `/*` |
|     - |  741 | ` * Typed/readonly store enforcement for reflection writes. Like the VM's` |
|     - |  742 | ` * store path, except an UNINITIALIZED readonly property may be written from` |
|     - |  743 | ` * any scope (PHP lets ReflectionProperty::setValue initialize readonly): the` |
|     - |  744 | ` * READONLY bit is masked off for the enforcement call so the set-scope check` |
|     - |  745 | ` * is skipped, while an already-initialized readonly still gets PHP's` |
|     - |  746 | ` * "Cannot modify readonly property" Error. Returns SXRET_OK/PH7_EXCEPTION/` |
|     - |  747 | ` * PH7_ABORT; the value may be coerced in place.` |
|     - |  748 | ` */` |
|    10 |  749 | `static sxi32 ReflectEnforceStore(ph7_context *pCtx, sxu32 nIdx, ph7_value *pValue)` |
|     1 |  750 | `{` |
|    11 |  751 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  752 | `	SyHashEntry *pSlot;` |
|     - |  753 | `	VmClassAttr *pVmAttr;` |
|     - |  754 | `	ph7_class_attr *pAttr;` |
|     - |  755 | `	sxi32 iSaved, rc;` |
|    11 |  756 | `	pSlot = SyHashGet(&pVm->hTypedSlot, (const void *)&nIdx, sizeof(sxu32));` |
|    11 |  757 | `	if( pSlot == 0 ){` |
|     7 |  758 | `		return SXRET_OK; /* Untyped slot: plain store */` |
|     - |  759 | `	}` |
|     5 |  760 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     5 |  761 | `	pAttr = pVmAttr->pAttr;` |
|     5 |  762 | `	if( pAttr == 0 ){` |
|   ! 0 |  763 | `		return SXRET_OK;` |
|     - |  764 | `	}` |
|     5 |  765 | `	iSaved = pAttr->iFlags;` |
|     5 |  766 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) ){` |
|   ! 0 |  767 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_READONLY;` |
|   ! 0 |  768 | `	}` |
|     5 |  769 | `	rc = PH7_VmEnforcePropStore(pVm, nIdx, pValue);` |
|     5 |  770 | `	pAttr->iFlags = iSaved;` |
|     5 |  771 | `	return rc;` |
|     6 |  772 | `}` |
|     - |  773 | `/*` |
|     - |  774 | ` * mixed __reflect_prop_read(object $obj, string $name)` |
|     - |  775 | ` * Instance property read, visibility ignored. Throws PHP's Error for an` |
|     - |  776 | ` * uninitialized typed property.` |
|     - |  777 | ` */` |
|    20 |  778 | `static int vm_builtin_reflect_prop_read(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  779 | `{` |
|     - |  780 | `	ph7_class_instance *pThis;` |
|     - |  781 | `	SyHashEntry *pEntry;` |
|     - |  782 | `	VmClassAttr *pVmAttr;` |
|     - |  783 | `	ph7_value *pValue;` |
|     - |  784 | `	const char *zName;` |
|     - |  785 | `	int nLen;` |
|    21 |  786 | `	if( nArg < 2 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  787 | `		ph7_result_null(pCtx);` |
|   ! 0 |  788 | `		return PH7_OK;` |
|     - |  789 | `	}` |
|    21 |  790 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    21 |  791 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|    21 |  792 | `	pEntry = nLen > 0 ? SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen) : 0;` |
|    21 |  793 | `	if( pEntry == 0 ){` |
|   ! 0 |  794 | `		ph7_result_null(pCtx);` |
|   ! 0 |  795 | `		return PH7_OK;` |
|     - |  796 | `	}` |
|    21 |  797 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    21 |  798 | `	if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|     3 |  799 | `		ph7_class *pDecl = pVmAttr->pAttr->pDeclClass ? pVmAttr->pAttr->pDeclClass : pThis->pClass;` |
|     4 |  800 | `		return PH7_VmThrowException(pCtx, "Error",` |
|     - |  801 | `			"Typed property %z::$%z must not be accessed before initialization",` |
|     2 |  802 | `			&pDecl->sName, &pVmAttr->pAttr->sName);` |
|     - |  803 | `	}` |
|    19 |  804 | `	pValue = PH7_ClassInstanceExtractAttrValue(pThis, pVmAttr);` |
|    19 |  805 | `	if( pValue ){` |
|    19 |  806 | `		ph7_result_value(pCtx, pValue);` |
|    10 |  807 | `	}else{` |
|   ! 0 |  808 | `		ph7_result_null(pCtx);` |
|     - |  809 | `	}` |
|    19 |  810 | `	return PH7_OK;` |
|    11 |  811 | `}` |
|     - |  812 | `/*` |
|     - |  813 | ` * bool __reflect_prop_write(object $obj, string $name, mixed $value)` |
|     - |  814 | ` * Instance property write, visibility ignored; typed and readonly rules` |
|     - |  815 | ` * enforced (see ReflectEnforceStore).` |
|     - |  816 | ` */` |
|     6 |  817 | `static int vm_builtin_reflect_prop_write(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  818 | `{` |
|     - |  819 | `	ph7_class_instance *pThis;` |
|     - |  820 | `	SyHashEntry *pEntry;` |
|     - |  821 | `	VmClassAttr *pVmAttr;` |
|     - |  822 | `	ph7_value *pValue;` |
|     - |  823 | `	const char *zName;` |
|     - |  824 | `	sxi32 rc;` |
|     - |  825 | `	int nLen;` |
|     7 |  826 | `	if( nArg < 3 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  827 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  828 | `		return PH7_OK;` |
|     - |  829 | `	}` |
|     7 |  830 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     7 |  831 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|     7 |  832 | `	pEntry = nLen > 0 ? SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen) : 0;` |
|     7 |  833 | `	if( pEntry == 0 ){` |
|   ! 0 |  834 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  835 | `		return PH7_OK;` |
|     - |  836 | `	}` |
|     7 |  837 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     7 |  838 | `	rc = ReflectEnforceStore(pCtx, pVmAttr->nIdx, apArg[2]);` |
|     7 |  839 | `	if( rc != SXRET_OK ){` |
|     3 |  840 | `		return rc;` |
|     - |  841 | `	}` |
|     5 |  842 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|     5 |  843 | `	if( pValue == 0 ){` |
|   ! 0 |  844 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  845 | `		return PH7_OK;` |
|     - |  846 | `	}` |
|     5 |  847 | `	PH7_MemObjStore(apArg[2], pValue);` |
|     5 |  848 | `	ph7_result_bool(pCtx, 1);` |
|     5 |  849 | `	return PH7_OK;` |
|     4 |  850 | `}` |
|     - |  851 | `/*` |
|     - |  852 | ` * int __reflect_prop_state(object\|string $target, string $name)` |
|     - |  853 | ` * Bitfield: 1 = exists (instance attr / static slot), 2 = initialized,` |
|     - |  854 | ` * 4 = dynamic (instance-owned, not class-declared).` |
|     - |  855 | ` */` |
|    16 |  856 | `static int vm_builtin_reflect_prop_state(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  857 | `{` |
|    17 |  858 | `	int iState = 0;` |
|     - |  859 | `	const char *zName;` |
|     - |  860 | `	int nLen;` |
|    17 |  861 | `	if( nArg < 2 ){` |
|   ! 0 |  862 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  863 | `		return PH7_OK;` |
|     - |  864 | `	}` |
|    17 |  865 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|    17 |  866 | `	if( nLen < 1 ){` |
|   ! 0 |  867 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  868 | `		return PH7_OK;` |
|     - |  869 | `	}` |
|    17 |  870 | `	if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|    17 |  871 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    17 |  872 | `		SyHashEntry *pEntry = SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen);` |
|    17 |  873 | `		if( pEntry ){` |
|    17 |  874 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    17 |  875 | `			iState \|= 1;` |
|    17 |  876 | `			if( (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|    15 |  877 | `				iState \|= 2;` |
|     7 |  878 | `			}` |
|    17 |  879 | `			if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|    11 |  880 | `				iState \|= 4;` |
|     5 |  881 | `			}` |
|     8 |  882 | `		}` |
|     9 |  883 | `	}else{` |
|   ! 0 |  884 | `		ph7_class *pClass = ReflectResolveClass(pCtx->pVm, apArg[0]);` |
|   ! 0 |  885 | `		ph7_class_attr *pAttr = pClass ? ReflectFetchAttr(pClass, apArg[1]) : 0;` |
|   ! 0 |  886 | `		if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|   ! 0 |  887 | `			SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|   ! 0 |  888 | `			iState \|= 1 \| 2;` |
|   ! 0 |  889 | `			if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|   ! 0 |  890 | `				iState &= ~2;` |
|   ! 0 |  891 | `			}` |
|   ! 0 |  892 | `		}` |
|     - |  893 | `	}` |
|    17 |  894 | `	ph7_result_int(pCtx, iState);` |
|    17 |  895 | `	return PH7_OK;` |
|     9 |  896 | `}` |
|     - |  897 | `/*` |
|     - |  898 | ` * array __reflect_dyn_props(object $obj)` |
|     - |  899 | ` * Names of the instance's runtime-added (dynamic) properties, in creation` |
|     - |  900 | ` * order (the instance attr table inserts dynamics at the tail).` |
|     - |  901 | ` */` |
|     2 |  902 | `static int vm_builtin_reflect_dyn_props(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  903 | `{` |
|     - |  904 | `	ph7_class_instance *pThis;` |
|     - |  905 | `	SyHashEntry *pEntry;` |
|     - |  906 | `	ph7_value *pList;` |
|     2 |  907 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|     3 |  908 | `	 \|\| (pList = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 |  909 | `		ph7_result_null(pCtx);` |
|   ! 0 |  910 | `		return PH7_OK;` |
|     - |  911 | `	}` |
|     3 |  912 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     3 |  913 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     7 |  914 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     5 |  915 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     5 |  916 | `		if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|     5 |  917 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|     5 |  918 | `			if( pName == 0 ){ break; }` |
|     7 |  919 | `			ph7_value_string(pName, SyStringData(&pVmAttr->pAttr->sName),` |
|     4 |  920 | `				(int)SyStringLength(&pVmAttr->pAttr->sName));` |
|     5 |  921 | `			ph7_array_add_elem(pList, 0, pName);` |
|     2 |  922 | `		}` |
|     1 |  923 | `	}` |
|     3 |  924 | `	ph7_result_value(pCtx, pList);` |
|     3 |  925 | `	return PH7_OK;` |
|     2 |  926 | `}` |
|     - |  927 | `/* Hand an EXISTING instance to the caller: takes an extra reference` |
|     - |  928 | ` * (unlike ReflectResultObject, which transfers a fresh instance's one). */` |
|     4 |  929 | `static int ReflectResultExistingObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|     1 |  930 | `{` |
|     5 |  931 | `	if( pObj == 0 ){` |
|   ! 0 |  932 | `		ph7_result_null(pCtx);` |
|   ! 0 |  933 | `		return PH7_OK;` |
|     - |  934 | `	}` |
|     5 |  935 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     5 |  936 | `	pObj->iRef++;` |
|     5 |  937 | `	pCtx->pRet->x.pOther = pObj;` |
|     5 |  938 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|     5 |  939 | `	return PH7_OK;` |
|     3 |  940 | `}` |
|     - |  941 | `/* pVal is a Closure instance? Return it, else NULL. */` |
|   492 |  942 | `static ph7_class_instance * ReflectValueClosure(ph7_vm *pVm, ph7_value *pVal)` |
|     3 |  943 | `{` |
|     - |  944 | `	ph7_class_instance *pThis;` |
|   495 |  945 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
|   418 |  946 | `		return 0;` |
|     - |  947 | `	}` |
|    78 |  948 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    78 |  949 | `	return (pThis->pClass == pVm->pClosureClass) ? pThis : 0;` |
|   249 |  950 | `}` |
|     - |  951 | `/*` |
|     - |  952 | ` * Resolve a reflection callable target into its compiled function.` |
|     - |  953 | ` *   - pMethodArg a non-empty string  -> method mode: pTarget is a class name` |
|     - |  954 | ` *     or object; outputs *ppClass and *ppMeth.` |
|     - |  955 | ` *   - pTarget a Closure              -> unwrap $__fn into hFunction; *ppClosure.` |
|     - |  956 | ` *   - pTarget a string               -> hFunction (user) or hHostFunction` |
|     - |  957 | ` *     (*ppHost set, returns NULL).` |
|     - |  958 | ` * Returns the ph7_vm_func, or NULL (host function or unresolvable).` |
|     - |  959 | ` */` |
|   796 |  960 | `static ph7_vm_func * ReflectResolveCallable(ph7_context *pCtx, ph7_value *pTarget,` |
|     - |  961 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|     - |  962 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|     3 |  963 | `{` |
|   799 |  964 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  965 | `	SyHashEntry *pEntry;` |
|   799 |  966 | `	if( ppClass ){ *ppClass = 0; }` |
|   799 |  967 | `	if( ppMeth ){ *ppMeth = 0; }` |
|   799 |  968 | `	if( ppHost ){ *ppHost = 0; }` |
|   799 |  969 | `	if( ppClosure ){ *ppClosure = 0; }` |
|   799 |  970 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
|   305 |  971 | `		ph7_class *pClass = ReflectResolveClass(pVm, pTarget);` |
|     - |  972 | `		ph7_class_method *pMeth;` |
|   305 |  973 | `		if( pClass == 0 ){` |
|   ! 0 |  974 | `			return 0;` |
|     - |  975 | `		}` |
|   457 |  976 | `		pMeth = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|   152 |  977 | `			SyBlobLength(&pMethodArg->sBlob));` |
|   305 |  978 | `		if( pMeth == 0 ){` |
|     - |  979 | `			/* getMethods()/ReflectionClass reports a base class's PRIVATE methods on` |
|     - |  980 | `			 * the subclass (php copies them into the child's table), but private` |
|     - |  981 | `			 * methods are not inherited into the child's method table, so the plain` |
|     - |  982 | `			 * extract misses them when a ReflectionMethod obtained from the subclass` |
|     - |  983 | `			 * is re-resolved by (subclass, name). Walk the base chain to find the` |
|     - |  984 | `			 * declaring class's own copy. */` |
|   ! 0 |  985 | `			ph7_class *pWalk = pClass->pBase;` |
|   ! 0 |  986 | `			while( pWalk && pMeth == 0 ){` |
|   ! 0 |  987 | `				pMeth = PH7_ClassExtractMethod(pWalk, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|   ! 0 |  988 | `					SyBlobLength(&pMethodArg->sBlob));` |
|   ! 0 |  989 | `				pWalk = pWalk->pBase;` |
|   ! 0 |  990 | `			}` |
|   ! 0 |  991 | `		}` |
|   305 |  992 | `		if( pMeth == 0 ){` |
|   ! 0 |  993 | `			return 0;` |
|     - |  994 | `		}` |
|   305 |  995 | `		if( ppClass ){ *ppClass = pClass; }` |
|   305 |  996 | `		if( ppMeth ){ *ppMeth = pMeth; }` |
|   305 |  997 | `		return &pMeth->sFunc;` |
|     - |  998 | `	}` |
|     - |  999 | `	{` |
|   495 | 1000 | `		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|   495 | 1001 | `		if( pClo ){` |
|     - | 1002 | `			SyString sAttr;` |
|     - | 1003 | `			ph7_value *pFn;` |
|    78 | 1004 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    78 | 1005 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|    78 | 1006 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) < 1 ){` |
|   ! 0 | 1007 | `				return 0;` |
|     - | 1008 | `			}` |
|     - | 1009 | `			/* A closure over an object method or __invoke object` |
|     - | 1010 | `			 * (Closure::fromCallable([$obj,'m']) / fromCallable($invokeObj)) stores the` |
|     - | 1011 | `			 * bare method name in $__fn and the class in $__scope. Resolve it as a METHOD` |
|     - | 1012 | `			 * of $__scope FIRST — before the global function table — so a same-named` |
|     - | 1013 | `			 * global function does not shadow the method (the bug: $__fn "add" hitting a` |
|     - | 1014 | `			 * global add()). A plain anonymous closure's $__fn is its unique lambda name,` |
|     - | 1015 | `			 * which is not a method, so this falls through cleanly. */` |
|     - | 1016 | `			{` |
|     - | 1017 | `				SyString sScope;` |
|     - | 1018 | `				ph7_value *pScope;` |
|    78 | 1019 | `				SyStringInitFromBuf(&sScope, "__scope", 7);` |
|    78 | 1020 | `				pScope = PH7_ClassInstanceFetchAttr(pClo, &sScope);` |
|    78 | 1021 | `				if( pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0 ){` |
|    31 | 1022 | `					ph7_class *pScopeCls = PH7_VmExtractClass(pVm,` |
|    20 | 1023 | `						(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);` |
|    21 | 1024 | `					if( pScopeCls ){` |
|    31 | 1025 | `						ph7_class_method *pScopeMeth = PH7_ClassExtractMethod(pScopeCls,` |
|    20 | 1026 | `							(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    21 | 1027 | `						if( pScopeMeth ){` |
|    21 | 1028 | `							if( ppClass ){ *ppClass = pScopeCls; }` |
|    21 | 1029 | `							if( ppMeth ){ *ppMeth = pScopeMeth; }` |
|    21 | 1030 | `							if( ppClosure ){ *ppClosure = pClo; }` |
|    21 | 1031 | `							return &pScopeMeth->sFunc;` |
|     - | 1032 | `						}` |
|   ! 0 | 1033 | `					}` |
|   ! 0 | 1034 | `				}` |
|     - | 1035 | `			}` |
|    58 | 1036 | `			pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    58 | 1037 | `			if( pEntry == 0 ){` |
|     - | 1038 | `				/* A Closure over a host function (Closure::fromCallable('strlen')) */` |
|   ! 0 | 1039 | `				pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|   ! 0 | 1040 | `				if( pEntry && ppHost ){` |
|   ! 0 | 1041 | `					*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|   ! 0 | 1042 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|   ! 0 | 1043 | `				}` |
|   ! 0 | 1044 | `				return 0;` |
|     - | 1045 | `			}` |
|    58 | 1046 | `			if( ppClosure ){ *ppClosure = pClo; }` |
|    58 | 1047 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - | 1048 | `		}` |
|     - | 1049 | `	}` |
|   418 | 1050 | `	if( pTarget->iFlags & MEMOBJ_STRING ){` |
|   418 | 1051 | `		if( SyBlobLength(&pTarget->sBlob) < 1 ){` |
|   ! 0 | 1052 | `			return 0;` |
|     - | 1053 | `		}` |
|   418 | 1054 | `		pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   418 | 1055 | `		if( pEntry ){` |
|   285 | 1056 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - | 1057 | `		}` |
|   134 | 1058 | `		pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   134 | 1059 | `		if( pEntry && ppHost ){` |
|   130 | 1060 | `			*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|    64 | 1061 | `		}` |
|    66 | 1062 | `	}` |
|   134 | 1063 | `	return 0;` |
|   401 | 1064 | `}` |
|     - | 1065 | `/* Emit the shared descriptor fields of a compiled function. */` |
|   612 | 1066 | `static void ReflectFillFuncCommon(ph7_context *pCtx, ph7_value *pInfo, ph7_vm_func *pFunc)` |
|     2 | 1067 | `{` |
|     - | 1068 | `	ph7_vm_func_arg *aArg;` |
|     - | 1069 | `	ph7_value *pParams, *pStatics;` |
|   614 | 1070 | `	int bVariadic = 0;` |
|     - | 1071 | `	int bAnon;` |
|     - | 1072 | `	sxu32 n;` |
|     - | 1073 | ``	/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|     - | 1074 | `	 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|   614 | 1075 | `	bAnon = (pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|   612 | 1076 | `	if( !bAnon && SyStringLength(&pFunc->sName) > 9` |
|   329 | 1077 | `	 && (SyMemcmp(SyStringData(&pFunc->sName), "[lambda_", 8) == 0` |
|    88 | 1078 | `	  \|\| SyMemcmp(SyStringData(&pFunc->sName), "[closure_", 9) == 0) ){` |
|     5 | 1079 | `		bAnon = 1;` |
|     2 | 1080 | `	}` |
|   614 | 1081 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|   614 | 1082 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pFunc->iFlags & VM_FUNC_INTERNAL) != 0);` |
|   614 | 1083 | `	ReflectMapAddBool(pCtx, pInfo, "closure", bAnon);` |
|   614 | 1084 | `	ReflectMapAddBool(pCtx, pInfo, "fstatic", (pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);` |
|   614 | 1085 | `	ReflectMapAddBool(pCtx, pInfo, "byref", (pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|   614 | 1086 | `	ReflectMapAddBool(pCtx, pInfo, "generator", (pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|   614 | 1087 | `	ReflectMapAddBool(pCtx, pInfo, "strict", pFunc->bStrictTypes != 0);` |
|   614 | 1088 | `	if( SyStringLength(&pFunc->sFile) > 0 ){` |
|   610 | 1089 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pFunc->sFile), (int)SyStringLength(&pFunc->sFile));` |
|   306 | 1090 | `	}else{` |
|     5 | 1091 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1092 | `	}` |
|   614 | 1093 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pFunc->nLine);` |
|   614 | 1094 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pFunc->nEndLine);` |
|   614 | 1095 | `	ReflectMapAddDoc(pCtx, pInfo, &pFunc->sDoc);` |
|   614 | 1096 | `	ReflectMapAddAttrs(pCtx, pInfo, &pFunc->aAttrs);` |
|   614 | 1097 | `	if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|   166 | 1098 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", SyStringData(&pFunc->sReturnTypeName),` |
|   110 | 1099 | `			(int)SyStringLength(&pFunc->sReturnTypeName));` |
|   559 | 1100 | `	}else if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     - | 1101 | `		/* The type-text renderer omits void/never atoms (compile.c notes the` |
|     - | 1102 | `		 * root fix belongs there); name them here for getReturnType(). */` |
|     3 | 1103 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "void", sizeof("void")-1);` |
|   503 | 1104 | `	}else if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|     3 | 1105 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "never", sizeof("never")-1);` |
|     2 | 1106 | `	}else{` |
|   500 | 1107 | `		ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1108 | `	}` |
|   614 | 1109 | `	ReflectMapAddBool(pCtx, pInfo, "retnullable", (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) != 0);` |
|     - | 1110 | `	/* Parameters */` |
|   614 | 1111 | `	pParams = ph7_context_new_array(pCtx);` |
|   614 | 1112 | `	aArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|  1906 | 1113 | `	for( n = 0 ; pParams && n < SySetUsed(&pFunc->aArgs) ; n++ ){` |
|  1293 | 1114 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|  1293 | 1115 | `		if( pMeta == 0 ){ break; }` |
|  1293 | 1116 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aArg[n].sName), (int)SyStringLength(&aArg[n].sName));` |
|  1293 | 1117 | `		ReflectMapAddInt(pCtx, pMeta, "pos", (sxi64)n);` |
|  1293 | 1118 | `		ReflectMapAddBool(pCtx, pMeta, "byref", (aArg[n].iFlags & VM_FUNC_ARG_BY_REF) != 0);` |
|  1293 | 1119 | `		ReflectMapAddBool(pCtx, pMeta, "variadic", (aArg[n].iFlags & VM_FUNC_ARG_VARIADIC) != 0);` |
|     - | 1120 | `		/* The compiler never sets ARG_HAS_DEF; a default = compiled bytecode` |
|     - | 1121 | `		 * (same test the OP_CALL default-value path uses). */` |
|  1293 | 1122 | `		ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&aArg[n].aByteCode) > 0);` |
|  1293 | 1123 | `		ReflectMapAddBool(pCtx, pMeta, "nullable", (aArg[n].iFlags & VM_FUNC_ARG_NULLABLE) != 0);` |
|  1293 | 1124 | `		ReflectMapAddBool(pCtx, pMeta, "promoted", (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) != 0);` |
|  1293 | 1125 | `		if( SyStringLength(&aArg[n].sTypeName) > 0 ){` |
|   775 | 1126 | `			ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&aArg[n].sTypeName),` |
|   516 | 1127 | `				(int)SyStringLength(&aArg[n].sTypeName));` |
|   259 | 1128 | `		}else{` |
|   777 | 1129 | `			ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - | 1130 | `		}` |
|  1293 | 1131 | `		ReflectMapAddAttrs(pCtx, pMeta, &aArg[n].aAttrs);` |
|  1293 | 1132 | `		ph7_array_add_elem(pParams, 0, pMeta);` |
|  1293 | 1133 | `		if( aArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|   189 | 1134 | `			bVariadic = 1;` |
|    94 | 1135 | `		}` |
|   647 | 1136 | `	}` |
|   614 | 1137 | `	if( pParams ){` |
|   614 | 1138 | `		ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|   306 | 1139 | `	}` |
|   614 | 1140 | `	ReflectMapAddBool(pCtx, pInfo, "variadic", bVariadic);` |
|     - | 1141 | `	/* Static variables: current value when the slot was initialized (first` |
|     - | 1142 | `	 * call), otherwise the evaluated default — PHP's getStaticVariables` |
|     - | 1143 | `	 * initializes on demand and reports the same values. */` |
|   614 | 1144 | `	pStatics = ph7_context_new_array(pCtx);` |
|   614 | 1145 | `	if( pStatics ){` |
|   614 | 1146 | `		ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|   642 | 1147 | `		for( n = 0 ; n < SySetUsed(&pFunc->aStatic) ; n++ ){` |
|    29 | 1148 | `			ph7_value *pVal = 0;` |
|     - | 1149 | `			ph7_value sScratch;` |
|    29 | 1150 | `			int bScratch = 0;` |
|    29 | 1151 | `			if( aStatic[n].nIdx != SXU32_HIGH ){` |
|    11 | 1152 | `				pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);` |
|     5 | 1153 | `			}` |
|    29 | 1154 | `			if( pVal == 0 ){` |
|    19 | 1155 | `				PH7_MemObjInit(pCtx->pVm, &sScratch);` |
|    19 | 1156 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|    19 | 1157 | `					VmLocalExec(pCtx->pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|     9 | 1158 | `				}` |
|    19 | 1159 | `				pVal = &sScratch;` |
|    19 | 1160 | `				bScratch = 1;` |
|     9 | 1161 | `			}` |
|    29 | 1162 | `			ReflectMapAddDyn(pCtx, pStatics, &aStatic[n].sName, pVal);` |
|    29 | 1163 | `			if( bScratch ){` |
|    19 | 1164 | `				PH7_MemObjRelease(&sScratch);` |
|     9 | 1165 | `			}` |
|    15 | 1166 | `		}` |
|   614 | 1167 | `		ph7_array_add_strkey_elem(pInfo, "statics", pStatics);` |
|   306 | 1168 | `	}` |
|   614 | 1169 | `}` |
|     - | 1170 | `/*` |
|     - | 1171 | ` * array\|null __reflect_func_info(string\|Closure $target [, string $method])` |
|     - | 1172 | ` * Function/method/closure descriptor for the PHP layer.` |
|     - | 1173 | ` */` |
|   744 | 1174 | `static int vm_builtin_reflect_func_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     3 | 1175 | `{` |
|     - | 1176 | `	ph7_vm_func *pFunc;` |
|   747 | 1177 | `	ph7_class *pClass = 0;` |
|   747 | 1178 | `	ph7_class_method *pMeth = 0;` |
|   747 | 1179 | `	ph7_user_func *pHost = 0;` |
|   747 | 1180 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1181 | `	ph7_value *pInfo;` |
|   747 | 1182 | `	if( nArg < 1 ){` |
|   ! 0 | 1183 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1184 | `		return PH7_OK;` |
|     - | 1185 | `	}` |
|   747 | 1186 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], nArg > 1 ? apArg[1] : 0,` |
|     - | 1187 | `		&pClass, &pMeth, &pHost, &pClosure);` |
|   747 | 1188 | `	if( pFunc == 0 && pHost == 0 ){` |
|     6 | 1189 | `		ph7_result_null(pCtx);` |
|     6 | 1190 | `		return PH7_OK;` |
|     - | 1191 | `	}` |
|   743 | 1192 | `	pInfo = ph7_context_new_array(pCtx);` |
|   743 | 1193 | `	if( pInfo == 0 ){` |
|   ! 0 | 1194 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1195 | `		return PH7_OK;` |
|     - | 1196 | `	}` |
|   743 | 1197 | `	if( pFunc == 0 ){` |
|     - | 1198 | `		/* Host (C builtin) function: no parameter metadata beyond arity */` |
|   130 | 1199 | `		ph7_value *pParams = ph7_context_new_array(pCtx);` |
|   130 | 1200 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pHost->sName), (int)SyStringLength(&pHost->sName));` |
|   130 | 1201 | `		ReflectMapAddBool(pCtx, pInfo, "internal", 1);` |
|   130 | 1202 | `		ReflectMapAddBool(pCtx, pInfo, "closure", 0);` |
|   130 | 1203 | `		ReflectMapAddBool(pCtx, pInfo, "fstatic", 0);` |
|   130 | 1204 | `		ReflectMapAddBool(pCtx, pInfo, "byref", 0);` |
|   130 | 1205 | `		ReflectMapAddBool(pCtx, pInfo, "generator", 0);` |
|   130 | 1206 | `		ReflectMapAddBool(pCtx, pInfo, "strict", 0);` |
|   130 | 1207 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|   130 | 1208 | `		ReflectMapAddInt(pCtx, pInfo, "line", 0);` |
|   130 | 1209 | `		ReflectMapAddInt(pCtx, pInfo, "endline", 0);` |
|   130 | 1210 | `		ReflectMapAddBool(pCtx, pInfo, "doc", 0);` |
|     - | 1211 | `		{` |
|   130 | 1212 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|   130 | 1213 | `			if( pEmpty ){` |
|   130 | 1214 | `				ph7_array_add_strkey_elem(pInfo, "attrs", pEmpty);` |
|    64 | 1215 | `			}` |
|     - | 1216 | `		}` |
|   130 | 1217 | `		if( pHost->zRet ){` |
|   130 | 1218 | `			ReflectMapAddStr(pCtx, pInfo, "rettext", pHost->zRet, (int)SyStrlen(pHost->zRet));` |
|    66 | 1219 | `		}else{` |
|   ! 0 | 1220 | `			ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1221 | `		}` |
|   130 | 1222 | `		ReflectMapAddBool(pCtx, pInfo, "retnullable", 0);` |
|   130 | 1223 | `		if( pParams ){` |
|   130 | 1224 | `			ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|    64 | 1225 | `		}` |
|   130 | 1226 | `		ReflectMapAddBool(pCtx, pInfo, "variadic", 0);` |
|   130 | 1227 | `		ReflectMapAddInt(pCtx, pInfo, "minarg", (sxi64)pHost->nMinArg);` |
|   130 | 1228 | `		if( pHost->zSig ){` |
|   130 | 1229 | `			ReflectMapAddStr(pCtx, pInfo, "sig", pHost->zSig, (int)SyStrlen(pHost->zSig));` |
|    66 | 1230 | `		}else{` |
|   ! 0 | 1231 | `			ReflectMapAddStr(pCtx, pInfo, "sig", "", 0);` |
|     - | 1232 | `		}` |
|   130 | 1233 | `		ph7_result_value(pCtx, pInfo);` |
|   130 | 1234 | `		return PH7_OK;` |
|     - | 1235 | `	}` |
|   614 | 1236 | `	ReflectFillFuncCommon(pCtx, pInfo, pFunc);` |
|   614 | 1237 | `	ReflectMapAddInt(pCtx, pInfo, "minarg", -1);` |
|   614 | 1238 | `	if( (pFunc->iFlags & VM_FUNC_INTERNAL) && SySetUsed(&pFunc->aArgs) == 0 && pMeth == 0 ){` |
|     - | 1239 | `		/* Embedded-PHP builtin (max/min...): declared argless, actual` |
|     - | 1240 | `		 * signature comes from the static table */` |
|     5 | 1241 | `		const char *zRet = 0;` |
|     5 | 1242 | `		const char *zSig = PH7_VmBuiltinSigLookup(SyStringData(&pFunc->sName), SyStringLength(&pFunc->sName), &zRet);` |
|     5 | 1243 | `		if( zSig ){` |
|     5 | 1244 | `			ReflectMapAddStr(pCtx, pInfo, "sig", zSig, (int)SyStrlen(zSig));` |
|     2 | 1245 | `		}` |
|     5 | 1246 | `		if( zRet && SyStringLength(&pFunc->sReturnTypeName) == 0 ){` |
|     5 | 1247 | `			ReflectMapAddStr(pCtx, pInfo, "ret2", zRet, (int)SyStrlen(zRet));` |
|     2 | 1248 | `		}` |
|     2 | 1249 | `	}` |
|   614 | 1250 | `	if( pMeth && pClass ){` |
|   297 | 1251 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|   297 | 1252 | `		ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   297 | 1253 | `		ReflectMapAddStr(pCtx, pInfo, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   297 | 1254 | `		ReflectMapAddInt(pCtx, pInfo, "vis", (sxi64)pMeth->iProtection);` |
|   297 | 1255 | `		ReflectMapAddBool(pCtx, pInfo, "mstatic", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   297 | 1256 | `		ReflectMapAddBool(pCtx, pInfo, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|   297 | 1257 | `		ReflectMapAddBool(pCtx, pInfo, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   148 | 1258 | `	}` |
|   614 | 1259 | `	if( pClosure ){` |
|     - | 1260 | `		SyString sAttr;` |
|     - | 1261 | `		ph7_value *pAttr;` |
|     - | 1262 | `		ph7_value *pUsed;` |
|    74 | 1263 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|    74 | 1264 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    84 | 1265 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|    21 | 1266 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|    21 | 1267 | `			if( pKey ){` |
|    21 | 1268 | `				ph7_value_string(pKey, "this", 4);` |
|    21 | 1269 | `				ph7_array_add_elem(pInfo, pKey, pAttr);` |
|    10 | 1270 | `			}` |
|    11 | 1271 | `		}else{` |
|    54 | 1272 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|     - | 1273 | `		}` |
|    74 | 1274 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|    74 | 1275 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    74 | 1276 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|    31 | 1277 | `			ReflectMapAddStr(pCtx, pInfo, "scope", (const char *)SyBlobData(&pAttr->sBlob),` |
|    20 | 1278 | `				(int)SyBlobLength(&pAttr->sBlob));` |
|    11 | 1279 | `		}else{` |
|    54 | 1280 | `			ReflectMapAddNull(pCtx, pInfo, "scope");` |
|     - | 1281 | `		}` |
|     - | 1282 | `		/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */` |
|    74 | 1283 | `		pUsed = ph7_context_new_array(pCtx);` |
|    74 | 1284 | `		if( pUsed ){` |
|    74 | 1285 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     - | 1286 | `			sxu32 n;` |
|   136 | 1287 | `			for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; n++ ){` |
|    64 | 1288 | `				if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){` |
|    46 | 1289 | `					continue;` |
|     - | 1290 | `				}` |
|    18 | 1291 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|    10 | 1292 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|   ! 0 | 1293 | `					continue;` |
|     - | 1294 | `				}` |
|    19 | 1295 | `				if( (aEnv[n].iFlags & VM_FUNC_ARG_BY_REF) && aEnv[n].nIdx != SXU32_HIGH ){` |
|     - | 1296 | `					/* Captured by reference: report the slot's live value */` |
|     5 | 1297 | `					ph7_value *pLive = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aEnv[n].nIdx);` |
|     5 | 1298 | `					ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, pLive ? pLive : &aEnv[n].sValue);` |
|     5 | 1299 | `					continue;` |
|     - | 1300 | `				}` |
|    15 | 1301 | `				ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, &aEnv[n].sValue);` |
|     8 | 1302 | `			}` |
|    74 | 1303 | `			ph7_array_add_strkey_elem(pInfo, "used", pUsed);` |
|    36 | 1304 | `		}` |
|    36 | 1305 | `	}` |
|   614 | 1306 | `	ph7_result_value(pCtx, pInfo);` |
|   614 | 1307 | `	return PH7_OK;` |
|   375 | 1308 | `}` |
|     - | 1309 | `/*` |
|     - | 1310 | ` * mixed __reflect_param_default(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1311 | ` * Evaluate a parameter's compiled default expression.` |
|     - | 1312 | ` */` |
|    12 | 1313 | `static int vm_builtin_reflect_param_default(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1314 | `{` |
|     - | 1315 | `	ph7_vm_func *pFunc;` |
|     - | 1316 | `	ph7_vm_func_arg *pArg;` |
|     - | 1317 | `	ph7_value sValue;` |
|     - | 1318 | `	sxu32 nIdx;` |
|    13 | 1319 | `	if( nArg < 3 ){` |
|   ! 0 | 1320 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1321 | `		return PH7_OK;` |
|     - | 1322 | `	}` |
|    13 | 1323 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|    13 | 1324 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|    12 | 1325 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|    13 | 1326 | `	 \|\| SySetUsed(&pArg->aByteCode) < 1 ){` |
|   ! 0 | 1327 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1328 | `		return PH7_OK;` |
|     - | 1329 | `	}` |
|    13 | 1330 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|    13 | 1331 | `	VmLocalExec(pCtx->pVm, &pArg->aByteCode, &sValue, FALSE);` |
|    13 | 1332 | `	ph7_result_value(pCtx, &sValue);` |
|    13 | 1333 | `	PH7_MemObjRelease(&sValue);` |
|    13 | 1334 | `	return PH7_OK;` |
|     7 | 1335 | `}` |
|     - | 1336 | `/*` |
|     - | 1337 | ` * string\|null __reflect_param_defconst(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1338 | ` * When a parameter's default is a plain global-constant reference, its` |
|     - | 1339 | ` * source name; null otherwise. A constant default compiles to exactly` |
|     - | 1340 | ` * [ OP_LOADC (EXPAND) , OP_DONE ] with the name in the literal table.` |
|     - | 1341 | ` */` |
|     6 | 1342 | `static int vm_builtin_reflect_param_defconst(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1343 | `{` |
|     - | 1344 | `	ph7_vm_func *pFunc;` |
|     - | 1345 | `	ph7_vm_func_arg *pArg;` |
|     - | 1346 | `	VmInstr *aInstr;` |
|     - | 1347 | `	ph7_value *pLit;` |
|     - | 1348 | `	sxu32 nIdx;` |
|     7 | 1349 | `	if( nArg < 3 ){` |
|   ! 0 | 1350 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1351 | `		return PH7_OK;` |
|     - | 1352 | `	}` |
|     7 | 1353 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|     7 | 1354 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|     6 | 1355 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|     7 | 1356 | `	 \|\| SySetUsed(&pArg->aByteCode) != 2 ){` |
|     3 | 1357 | `		ph7_result_null(pCtx);` |
|     3 | 1358 | `		return PH7_OK;` |
|     - | 1359 | `	}` |
|     5 | 1360 | `	aInstr = (VmInstr *)SySetBasePtr(&pArg->aByteCode);` |
|     4 | 1361 | `	if( aInstr[0].iOp != PH7_OP_LOADC \|\| (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0` |
|     5 | 1362 | `	 \|\| aInstr[1].iOp != PH7_OP_DONE ){` |
|   ! 0 | 1363 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1364 | `		return PH7_OK;` |
|     - | 1365 | `	}` |
|     5 | 1366 | `	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);` |
|     5 | 1367 | `	if( pLit == 0 \|\| SyBlobLength(&pLit->sBlob) < 1 ){` |
|   ! 0 | 1368 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1369 | `		return PH7_OK;` |
|     - | 1370 | `	}` |
|     5 | 1371 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&pLit->sBlob), (int)SyBlobLength(&pLit->sBlob));` |
|     5 | 1372 | `	return PH7_OK;` |
|     4 | 1373 | `}` |
|     - | 1374 | `/*` |
|     - | 1375 | ` * mixed __reflect_invoke(mixed $target, ?string $method, ?object $this, array $args)` |
|     - | 1376 | ` * Visibility-bypassing invocation (methods dispatch by VM name; functions` |
|     - | 1377 | ` * and closures ride PH7_VmCallUserFunction like call_user_func_array).` |
|     - | 1378 | ` */` |
|    20 | 1379 | `static int vm_builtin_reflect_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1380 | `{` |
|    21 | 1381 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1382 | `	ph7_value sResult;` |
|     - | 1383 | `	SySet aCallArg;` |
|     - | 1384 | `	sxi32 rc;` |
|    21 | 1385 | `	if( nArg < 4 ){` |
|   ! 0 | 1386 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1387 | `		return PH7_OK;` |
|     - | 1388 | `	}` |
|    21 | 1389 | `	PH7_MemObjInit(pVm, &sResult);` |
|    21 | 1390 | `	sResult.nIdx = SXU32_HIGH;` |
|    21 | 1391 | `	SySetInit(&aCallArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    21 | 1392 | `	ReflectCollectArgs(pCtx, apArg[3], &aCallArg, 0);` |
|    21 | 1393 | `	if( (apArg[1]->iFlags & MEMOBJ_STRING) && SyBlobLength(&apArg[1]->sBlob) > 0 ){` |
|    11 | 1394 | `		ph7_class *pClass = 0;` |
|    11 | 1395 | `		ph7_class_method *pMeth = 0;` |
|    11 | 1396 | `		ph7_class_instance *pThis = 0;` |
|    11 | 1397 | `		ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, 0);` |
|    11 | 1398 | `		if( pMeth == 0 ){` |
|   ! 0 | 1399 | `			SySetRelease(&aCallArg);` |
|   ! 0 | 1400 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1401 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1402 | `			return PH7_OK;` |
|     - | 1403 | `		}` |
|    11 | 1404 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     7 | 1405 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     3 | 1406 | `		}` |
|     - | 1407 | `		/* Reflection ignores method visibility (PHP 8.1+); the flag is` |
|     - | 1408 | `		 * consumed by the first OP_CALL, i.e. this synthetic one. */` |
|    11 | 1409 | `		pVm->bReflectBypass = 1;` |
|    16 | 1410 | `		rc = PH7_VmCallClassMethod(pVm, pThis, pMeth, &sResult,` |
|    10 | 1411 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg));` |
|    11 | 1412 | `		pVm->bReflectBypass = 0;` |
|     6 | 1413 | `	}else{` |
|    16 | 1414 | `		rc = PH7_VmCallUserFunction(pVm, apArg[0],` |
|    10 | 1415 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg), &sResult);` |
|     - | 1416 | `	}` |
|    21 | 1417 | `	SySetRelease(&aCallArg);` |
|    21 | 1418 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 | 1419 | `		PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1420 | `		return rc;` |
|     - | 1421 | `	}` |
|    21 | 1422 | `	ph7_result_value(pCtx, &sResult);` |
|    21 | 1423 | `	PH7_MemObjRelease(&sResult);` |
|    21 | 1424 | `	return PH7_OK;` |
|    11 | 1425 | `}` |
|     - | 1426 | `/*` |
|     - | 1427 | ` * Closure __reflect_closure(mixed $target, ?string $method, ?object $this)` |
|     - | 1428 | ` * Mint a Closure for a function or method, bound and scoped like the` |
|     - | 1429 | ` * first-class-callable path.` |
|     - | 1430 | ` */` |
|     6 | 1431 | `static int vm_builtin_reflect_closure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1432 | `{` |
|     7 | 1433 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 1434 | `	ph7_class *pClass = 0;` |
|     7 | 1435 | `	ph7_class_method *pMeth = 0;` |
|     7 | 1436 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1437 | `	ph7_vm_func *pFunc;` |
|     7 | 1438 | `	if( nArg < 3 ){` |
|   ! 0 | 1439 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1440 | `		return PH7_OK;` |
|     - | 1441 | `	}` |
|     7 | 1442 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, &pClosure);` |
|     7 | 1443 | `	if( pClosure ){` |
|     - | 1444 | `		/* Already a Closure: hand the same instance back */` |
|   ! 0 | 1445 | `		return ReflectResultExistingObject(pCtx, pClosure);` |
|     - | 1446 | `	}` |
|     7 | 1447 | `	if( pMeth && pClass ){` |
|     5 | 1448 | `		ph7_class_instance *pThis = 0;` |
|     5 | 1449 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     3 | 1450 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     1 | 1451 | `		}` |
|     7 | 1452 | `		return ReflectResultObject(pCtx,` |
|     4 | 1453 | `			PH7_VmNewClosure(pVm, &pMeth->sFunc.sName, pThis, &pClass->sName));` |
|     - | 1454 | `	}` |
|     3 | 1455 | `	if( pFunc ){` |
|     3 | 1456 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &pFunc->sName, 0, 0));` |
|     - | 1457 | `	}` |
|     - | 1458 | `	/* Host function by name */` |
|   ! 0 | 1459 | `	if( apArg[0]->iFlags & MEMOBJ_STRING ){` |
|     - | 1460 | `		SyString sName;` |
|   ! 0 | 1461 | `		SyStringInitFromBuf(&sName, (const char *)SyBlobData(&apArg[0]->sBlob), SyBlobLength(&apArg[0]->sBlob));` |
|   ! 0 | 1462 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &sName, 0, 0));` |
|     - | 1463 | `	}` |
|   ! 0 | 1464 | `	ph7_result_null(pCtx);` |
|   ! 0 | 1465 | `	return PH7_OK;` |
|     4 | 1466 | `}` |
|     - | 1467 | `/*` |
|     - | 1468 | ` * Resolve a Generator object into its wrapper. Mirrors the static` |
|     - | 1469 | ` * VmGeneratorExtractCtx in vm.c: the $__ctx attribute carries the` |
|     - | 1470 | ` * ph7_generator pointer as a resource value.` |
|     - | 1471 | ` */` |
|    22 | 1472 | `static ph7_generator * ReflectGeneratorCtx(ph7_vm *pVm, ph7_value *pVal)` |
|     1 | 1473 | `{` |
|     - | 1474 | `	ph7_class_instance *pThis;` |
|     - | 1475 | `	ph7_value *pAttr;` |
|     - | 1476 | `	SyString sAttr;` |
|    23 | 1477 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVm->pGeneratorClass == 0 ){` |
|   ! 0 | 1478 | `		return 0;` |
|     - | 1479 | `	}` |
|    23 | 1480 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    23 | 1481 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|   ! 0 | 1482 | `		return 0;` |
|     - | 1483 | `	}` |
|    23 | 1484 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    23 | 1485 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    23 | 1486 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|   ! 0 | 1487 | `		return 0;` |
|     - | 1488 | `	}` |
|    23 | 1489 | `	return (ph7_generator *)pAttr->x.pOther;` |
|    12 | 1490 | `}` |
|     - | 1491 | `/*` |
|     - | 1492 | ` * array\|null __reflect_gen_info(Generator $g)` |
|     - | 1493 | ` * {state, closed, executing, kind ('fn'\|'method'), name, class?, this}` |
|     - | 1494 | ` */` |
|    16 | 1495 | `static int vm_builtin_reflect_gen_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1496 | `{` |
|    17 | 1497 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1498 | `	ph7_generator *pGen;` |
|     - | 1499 | `	ph7_exec_ctx *pExec;` |
|     - | 1500 | `	ph7_value *pInfo;` |
|    17 | 1501 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 \|\| pGen->pCtx == 0 ){` |
|   ! 0 | 1502 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1503 | `		return PH7_OK;` |
|     - | 1504 | `	}` |
|    17 | 1505 | `	pExec = pGen->pCtx;` |
|    17 | 1506 | `	pInfo = ph7_context_new_array(pCtx);` |
|    17 | 1507 | `	if( pInfo == 0 ){` |
|   ! 0 | 1508 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1509 | `		return PH7_OK;` |
|     - | 1510 | `	}` |
|    17 | 1511 | `	ReflectMapAddInt(pCtx, pInfo, "state", (sxi64)pExec->iState);` |
|    24 | 1512 | `	ReflectMapAddBool(pCtx, pInfo, "closed",` |
|    16 | 1513 | `		pExec->iState == PH7_CTX_STATE_COMPLETED \|\| pExec->iState == PH7_CTX_STATE_CLOSED);` |
|    17 | 1514 | `	ReflectMapAddBool(pCtx, pInfo, "executing", pVm->pActiveCtx == pExec);` |
|    17 | 1515 | `	if( pExec->pFunc ){` |
|    17 | 1516 | `		ph7_vm_func *pFunc = pExec->pFunc;` |
|    19 | 1517 | `		if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     5 | 1518 | `			ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|     5 | 1519 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "method", sizeof("method")-1);` |
|     5 | 1520 | `			ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|     3 | 1521 | `		}else{` |
|    13 | 1522 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "fn", sizeof("fn")-1);` |
|     - | 1523 | `		}` |
|    17 | 1524 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|     8 | 1525 | `	}` |
|     - | 1526 | `	{` |
|     - | 1527 | `		/* The coroutine frame installs $this as a frame VARIABLE (see` |
|     - | 1528 | `		 * VmFiberSetupFrame), not as pFrame->pThis — check both. */` |
|    17 | 1529 | `		ph7_value *pThisVal = 0;` |
|    17 | 1530 | `		if( pExec->pFrame ){` |
|    17 | 1531 | `			SyHashEntry *pVar = SyHashGet(&pExec->pFrame->hVar, "this", sizeof("this")-1);` |
|    17 | 1532 | `			if( pVar ){` |
|     5 | 1533 | `				ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj, (sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|     5 | 1534 | `				if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|     5 | 1535 | `					pThisVal = pSlot;` |
|     2 | 1536 | `				}` |
|     2 | 1537 | `			}` |
|    17 | 1538 | `			if( pThisVal == 0 && pExec->pFrame->pThis ){` |
|     - | 1539 | `				ph7_value sThis;` |
|   ! 0 | 1540 | `				ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 1541 | `				PH7_MemObjInit(pVm, &sThis);` |
|   ! 0 | 1542 | `				pExec->pFrame->pThis->iRef++;` |
|   ! 0 | 1543 | `				sThis.x.pOther = pExec->pFrame->pThis;` |
|   ! 0 | 1544 | `				MemObjSetType(&sThis, MEMOBJ_OBJ);` |
|   ! 0 | 1545 | `				if( pKey ){` |
|   ! 0 | 1546 | `					ph7_value_string(pKey, "this", 4);` |
|   ! 0 | 1547 | `					ph7_array_add_elem(pInfo, pKey, &sThis); /* copies (takes its own ref) */` |
|   ! 0 | 1548 | `				}` |
|   ! 0 | 1549 | `				PH7_MemObjRelease(&sThis);` |
|   ! 0 | 1550 | `				pThisVal = (ph7_value *)1; /* handled */` |
|   ! 0 | 1551 | `			}` |
|     8 | 1552 | `		}` |
|    17 | 1553 | `		if( pThisVal == 0 ){` |
|    13 | 1554 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|    11 | 1555 | `		}else if( pThisVal != (ph7_value *)1 ){` |
|     5 | 1556 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|     5 | 1557 | `			if( pKey ){` |
|     5 | 1558 | `				ph7_value_string(pKey, "this", 4);` |
|     5 | 1559 | `				ph7_array_add_elem(pInfo, pKey, pThisVal);` |
|     2 | 1560 | `			}` |
|     2 | 1561 | `		}` |
|     - | 1562 | `	}` |
|    17 | 1563 | `	ph7_result_value(pCtx, pInfo);` |
|    17 | 1564 | `	return PH7_OK;` |
|     9 | 1565 | `}` |
|     - | 1566 | `/*` |
|     - | 1567 | ` * Generator __reflect_gen_exec(Generator $g)` |
|     - | 1568 | `` * Follow `yield from` delegation to the innermost executing generator`` |
|     - | 1569 | ` * (PHP's ReflectionGenerator::getExecutingGenerator).` |
|     - | 1570 | ` */` |
|     4 | 1571 | `static int vm_builtin_reflect_gen_exec(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1572 | `{` |
|     5 | 1573 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1574 | `	ph7_generator *pGen;` |
|     - | 1575 | `	ph7_value *pCur;` |
|     5 | 1576 | `	int iDepth = 0;` |
|     5 | 1577 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 ){` |
|   ! 0 | 1578 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1579 | `		return PH7_OK;` |
|     - | 1580 | `	}` |
|     5 | 1581 | `	pCur = apArg[0];` |
|     9 | 1582 | `	while( pGen && pGen->pCtx && pGen->pCtx->iDelegateState == 3` |
|     8 | 1583 | `	 && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|     3 | 1584 | `		ph7_generator *pInner = ReflectGeneratorCtx(pVm, &pGen->pCtx->sDelegate);` |
|     3 | 1585 | `		if( pInner == 0 ){` |
|   ! 0 | 1586 | `			break;` |
|     - | 1587 | `		}` |
|     3 | 1588 | `		pCur = &pGen->pCtx->sDelegate;` |
|     3 | 1589 | `		pGen = pInner;` |
|     3 | 1590 | `		iDepth++;` |
|     1 | 1591 | `	}` |
|     5 | 1592 | `	return ReflectResultExistingObject(pCtx, (ph7_class_instance *)pCur->x.pOther);` |
|     3 | 1593 | `}` |
|     - | 1594 | `/*` |
|     - | 1595 | ` * array\|null __reflect_const_info(string $name)` |
|     - | 1596 | ` * Global-constant descriptor: {value}. Null when undefined. File/origin` |
|     - | 1597 | ` * metadata arrives with the C5 constant-metadata work.` |
|     - | 1598 | ` */` |
|    40 | 1599 | `static int vm_builtin_reflect_const_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1600 | `{` |
|    41 | 1601 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1602 | `	SyHashEntry *pEntry;` |
|     - | 1603 | `	ph7_constant *pCons;` |
|     - | 1604 | `	ph7_value *pInfo;` |
|     - | 1605 | `	ph7_value sValue;` |
|     - | 1606 | `	const char *zName;` |
|     - | 1607 | `	int nLen;` |
|    41 | 1608 | `	if( nArg < 1 ){` |
|   ! 0 | 1609 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1610 | `		return PH7_OK;` |
|     - | 1611 | `	}` |
|    41 | 1612 | `	zName = ph7_value_to_string(apArg[0], &nLen);` |
|    41 | 1613 | `	pEntry = nLen > 0 ? SyHashGet(&pVm->hConstant, (const void *)zName, (sxu32)nLen) : 0;` |
|    41 | 1614 | `	if( pEntry == 0 ){` |
|     3 | 1615 | `		ph7_result_null(pCtx);` |
|     3 | 1616 | `		return PH7_OK;` |
|     - | 1617 | `	}` |
|    39 | 1618 | `	pCons = (ph7_constant *)pEntry->pUserData;` |
|    39 | 1619 | `	pInfo = ph7_context_new_array(pCtx);` |
|    39 | 1620 | `	if( pInfo == 0 ){` |
|   ! 0 | 1621 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1622 | `		return PH7_OK;` |
|     - | 1623 | `	}` |
|    39 | 1624 | `	PH7_MemObjInit(pVm, &sValue);` |
|    39 | 1625 | `	if( pCons->xExpand ){` |
|    39 | 1626 | `		pCons->xExpand(&sValue, pCons->pUserData);` |
|    19 | 1627 | `	}` |
|     - | 1628 | `	{` |
|    39 | 1629 | `		ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|    39 | 1630 | `		if( pKey ){` |
|    39 | 1631 | `			ph7_value_string(pKey, "value", 5);` |
|    39 | 1632 | `			ph7_array_add_elem(pInfo, pKey, &sValue);` |
|    19 | 1633 | `		}` |
|     - | 1634 | `	}` |
|    39 | 1635 | `	PH7_MemObjRelease(&sValue);` |
|    39 | 1636 | `	ReflectMapAddBool(pCtx, pInfo, "internal", pCons->bUserDefined == 0);` |
|    39 | 1637 | `	if( SyStringLength(&pCons->sFile) > 0 ){` |
|    25 | 1638 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));` |
|    13 | 1639 | `	}else{` |
|    15 | 1640 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1641 | `	}` |
|    39 | 1642 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pCons->nLine);` |
|    39 | 1643 | `	ReflectMapAddAttrs(pCtx, pInfo, &pCons->aAttrs);` |
|    39 | 1644 | `	ph7_result_value(pCtx, pInfo);` |
|    39 | 1645 | `	return PH7_OK;` |
|    21 | 1646 | `}` |
|     - | 1647 | `/*` |
|     - | 1648 | ` * int\|null __reflect_ref_id(array $arr, int\|string $key)` |
|     - | 1649 | ` * The element's slot index when the element is a reference (its slot has` |
|     - | 1650 | ` * a reference-table record with at least two links), null otherwise.` |
|     - | 1651 | ` */` |
|     6 | 1652 | `static int vm_builtin_reflect_ref_id(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1653 | `{` |
|     - | 1654 | `	ph7_hashmap *pMap;` |
|     7 | 1655 | `	ph7_hashmap_node *pNode = 0;` |
|     7 | 1656 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|   ! 0 | 1657 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1658 | `		return PH7_OK;` |
|     - | 1659 | `	}` |
|     7 | 1660 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 | 1661 | `	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK \|\| pNode == 0 ){` |
|   ! 0 | 1662 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1663 | `		return PH7_OK;` |
|     - | 1664 | `	}` |
|     7 | 1665 | `	if( PH7_VmSlotRefCount(pCtx->pVm, pNode->nValIdx) < 2 ){` |
|     3 | 1666 | `		ph7_result_null(pCtx);` |
|     3 | 1667 | `		return PH7_OK;` |
|     - | 1668 | `	}` |
|     5 | 1669 | `	ph7_result_int64(pCtx, (sxi64)pNode->nValIdx);` |
|     5 | 1670 | `	return PH7_OK;` |
|     4 | 1671 | `}` |
|     - | 1672 | `/*` |
|     - | 1673 | ` * array\|null __reflect_attr_args(string $kind, mixed $target, ?string $member,` |
|     - | 1674 | ` *                                int $paramIdx, int $attrIdx)` |
|     - | 1675 | ` * Evaluate the recorded argument expressions of one declared attribute:` |
|     - | 1676 | ` * kind 'class' (target = class), 'attr' (class + property/constant name),` |
|     - | 1677 | ` * 'method' (class + method), 'fn' (function name or Closure), 'param'` |
|     - | 1678 | ` * (function spec + parameter index). Named arguments become string keys.` |
|     - | 1679 | ` */` |
|    68 | 1680 | `static int vm_builtin_reflect_attr_args(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1681 | `{` |
|    69 | 1682 | `	ph7_vm *pVm = pCtx->pVm;` |
|    69 | 1683 | `	SySet *pAttrs = 0;` |
|    69 | 1684 | `	ph7_class *pDeclCls = 0; /* class an attribute is declared on: scope for self:: in its args */` |
|     - | 1685 | `	ph7_attribute *pAttrRec;` |
|     - | 1686 | `	ph7_value *pOut;` |
|     - | 1687 | `	const char *zKind;` |
|     - | 1688 | `	int nKind;` |
|     - | 1689 | `	sxu32 nAttrIdx, n;` |
|    69 | 1690 | `	if( nArg < 5 ){` |
|   ! 0 | 1691 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1692 | `		return PH7_OK;` |
|     - | 1693 | `	}` |
|    69 | 1694 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|    69 | 1695 | `	nAttrIdx = (sxu32)ph7_value_to_int(apArg[4]);` |
|    91 | 1696 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|    45 | 1697 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|    45 | 1698 | `		if( pClass ){ pAttrs = &pClass->aAttrs; pDeclCls = pClass; }` |
|    49 | 1699 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|     5 | 1700 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|     5 | 1701 | `		ph7_class_attr *pMember = pClass ? ReflectFetchAttr(pClass, apArg[2]) : 0;` |
|     5 | 1702 | `		if( pMember ){ pAttrs = &pMember->aAttrs; pDeclCls = pClass; }` |
|    27 | 1703 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|     9 | 1704 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     9 | 1705 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|    21 | 1706 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|     9 | 1707 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], 0, 0, 0, 0, 0);` |
|     9 | 1708 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|    10 | 1709 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|     3 | 1710 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1711 | `		ph7_vm_func_arg *pParam = pFunc` |
|     2 | 1712 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|     3 | 1713 | `		if( pParam ){ pAttrs = &pParam->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }` |
|     4 | 1714 | `	}else if( nKind == 5 && SyMemcmp(zKind, "const", 5) == 0 ){` |
|     - | 1715 | ``		/* Global constant (php 8.5 attributes on `const` statements) */`` |
|     - | 1716 | `		const char *zCName;` |
|     - | 1717 | `		int nCName;` |
|     - | 1718 | `		SyHashEntry *pCEntry;` |
|     3 | 1719 | `		zCName = ph7_value_to_string(apArg[1], &nCName);` |
|     3 | 1720 | `		pCEntry = nCName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zCName, (sxu32)nCName) : 0;` |
|     3 | 1721 | `		if( pCEntry ){ pAttrs = &((ph7_constant *)pCEntry->pUserData)->aAttrs; }` |
|     1 | 1722 | `	}` |
|    68 | 1723 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|    69 | 1724 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 | 1725 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1726 | `		return PH7_OK;` |
|     - | 1727 | `	}` |
|   123 | 1728 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|    55 | 1729 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|     - | 1730 | `		ph7_value sValue;` |
|    55 | 1731 | `		PH7_MemObjInit(pVm, &sValue);` |
|    55 | 1732 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|     - | 1733 | `` 			/* Evaluate under the attribute's declaring-class scope so `self::class` `` |
|     - | 1734 | ``			 * / `self::CONST` / `parent::` resolve like php (else self:: would bind`` |
|     - | 1735 | `			 * to the reflection machinery's own class). */` |
|    55 | 1736 | `			PH7_VmExecAttrArg(pVm, &pArgRec->aByteCode, pDeclCls, &sValue);` |
|    27 | 1737 | `		}` |
|    55 | 1738 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|     5 | 1739 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|     3 | 1740 | `		}else{` |
|    51 | 1741 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|     - | 1742 | `		}` |
|    55 | 1743 | `		PH7_MemObjRelease(&sValue);` |
|    28 | 1744 | `	}` |
|    69 | 1745 | `	ph7_result_value(pCtx, pOut);` |
|    69 | 1746 | `	return PH7_OK;` |
|    35 | 1747 | `}` |
|  3876 | 1748 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|     5 | 1749 | `{` |
|     - | 1750 | `	static const struct {` |
|     - | 1751 | `		const char *zName;` |
|     - | 1752 | `		ProchHostFunction xFunc;` |
|     - | 1753 | `	} aFunc[] = {` |
|     - | 1754 | `		{ "__reflect_class_info",     vm_builtin_reflect_class_info },` |
|     - | 1755 | `		{ "__reflect_const_value",    vm_builtin_reflect_const_value },` |
|     - | 1756 | `		{ "__reflect_static_value",   vm_builtin_reflect_static_value },` |
|     - | 1757 | `		{ "__reflect_static_set",     vm_builtin_reflect_static_set },` |
|     - | 1758 | `		{ "__reflect_prop_default",   vm_builtin_reflect_prop_default },` |
|     - | 1759 | `		{ "__reflect_new_instance",   vm_builtin_reflect_new_instance },` |
|     - | 1760 | `		{ "__reflect_new_no_ctor",    vm_builtin_reflect_new_no_ctor },` |
|     - | 1761 | `		{ "__reflect_func_info",      vm_builtin_reflect_func_info },` |
|     - | 1762 | `		{ "__reflect_param_default",  vm_builtin_reflect_param_default },` |
|     - | 1763 | `		{ "__reflect_param_defconst", vm_builtin_reflect_param_defconst },` |
|     - | 1764 | `		{ "__reflect_invoke",         vm_builtin_reflect_invoke },` |
|     - | 1765 | `		{ "__reflect_closure",        vm_builtin_reflect_closure },` |
|     - | 1766 | `		{ "__reflect_prop_read",      vm_builtin_reflect_prop_read },` |
|     - | 1767 | `		{ "__reflect_prop_write",     vm_builtin_reflect_prop_write },` |
|     - | 1768 | `		{ "__reflect_prop_state",     vm_builtin_reflect_prop_state },` |
|     - | 1769 | `		{ "__reflect_dyn_props",      vm_builtin_reflect_dyn_props },` |
|     - | 1770 | `		{ "__reflect_gen_info",       vm_builtin_reflect_gen_info },` |
|     - | 1771 | `		{ "__reflect_gen_exec",       vm_builtin_reflect_gen_exec },` |
|     - | 1772 | `		{ "__reflect_const_info",     vm_builtin_reflect_const_info },` |
|     - | 1773 | `		{ "__reflect_ref_id",         vm_builtin_reflect_ref_id },` |
|     - | 1774 | `		{ "__reflect_attr_args",      vm_builtin_reflect_attr_args },` |
|     - | 1775 | `	};` |
|     - | 1776 | `	sxu32 n;` |
| 85277 | 1777 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 81401 | 1778 | `		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);` |
| 40703 | 1779 | `	}` |
|  3881 | 1780 | `	return PH7_VmInstallReflectionLib(&(*pVm));` |
|     5 | 1781 | `}` |
|     - | 1782 |  |
