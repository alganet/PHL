# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1027/1201 lines (85.51%)

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
|  1376 |   31 | `static ph7_class * ReflectResolveClass(ph7_vm *pVm, ph7_value *pArg)` |
|     1 |   32 | `{` |
|     - |   33 | `	ph7_class *pClass;` |
|  1377 |   34 | `	pClass = PH7_VmExtractClassFromValue(pVm, pArg);` |
|  1377 |   35 | `	if( pClass == 0 && ph7_value_is_string(pArg) ){` |
|     - |   36 | `		const char *zName;` |
|     - |   37 | `		int nLen;` |
|    13 |   38 | `		zName = ph7_value_to_string(pArg, &nLen);` |
|    13 |   39 | `		if( nLen > 0 ){` |
|    13 |   40 | `			pClass = PH7_VmTriggerAutoload(pVm, zName, (sxu32)nLen, FALSE);` |
|     6 |   41 | `		}` |
|     6 |   42 | `	}` |
|  1377 |   43 | `	return pClass;` |
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
| 41310 |   63 | `static void ReflectMapAddBool(ph7_context *pCtx, ph7_value *pMap, const char *zKey, int b)` |
|     1 |   64 | `{` |
| 41311 |   65 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 41311 |   66 | `	if( p == 0 ){ return; }` |
| 41311 |   67 | `	ph7_value_bool(p, b);` |
| 41311 |   68 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
| 20656 |   69 | `}` |
| 17398 |   70 | `static void ReflectMapAddInt(ph7_context *pCtx, ph7_value *pMap, const char *zKey, sxi64 iVal)` |
|     1 |   71 | `{` |
| 17399 |   72 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 17399 |   73 | `	if( p == 0 ){ return; }` |
| 17399 |   74 | `	ph7_value_int64(p, iVal);` |
| 17399 |   75 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  8700 |   76 | `}` |
| 12660 |   77 | `static void ReflectMapAddStr(ph7_context *pCtx, ph7_value *pMap, const char *zKey,` |
|     - |   78 | `	const char *zVal, int nVal)` |
|     1 |   79 | `{` |
| 12661 |   80 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 12661 |   81 | `	if( p == 0 ){ return; }` |
| 12661 |   82 | `	ph7_value_string(p, zVal, nVal);` |
| 12661 |   83 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  6331 |   84 | `}` |
|  4292 |   85 | `static void ReflectMapAddNull(ph7_context *pCtx, ph7_value *pMap, const char *zKey)` |
|     1 |   86 | `{` |
|  4293 |   87 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  4293 |   88 | `	if( p == 0 ){ return; }` |
|  4293 |   89 | `	ph7_value_null(p);` |
|  4293 |   90 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  2147 |   91 | `}` |
|     - |   92 | `/* Add an entry under a dynamic (SyString) key. */` |
|  5022 |   93 | `static void ReflectMapAddDyn(ph7_context *pCtx, ph7_value *pMap,` |
|     - |   94 | `	const SyString *pKey, ph7_value *pVal)` |
|     1 |   95 | `{` |
|  5023 |   96 | `	ph7_value *pK = ph7_context_new_scalar(pCtx);` |
|  5023 |   97 | `	if( pK == 0 ){ return; }` |
|  5023 |   98 | `	ph7_value_string(pK, pKey->zString, (int)pKey->nByte);` |
|  5023 |   99 | `	ph7_array_add_elem(pMap, pK, pVal);` |
|  2512 |  100 | `}` |
|     - |  101 | `/* Emit the declared #[...] attributes of a target as a summary list:` |
|     - |  102 | ` * [ {name, line} ... ]. Argument values stay lazy — the PHP layer pulls` |
|     - |  103 | ` * them through __reflect_attr_args when ReflectionAttribute needs them. */` |
|  5656 |  104 | `static void ReflectMapAddAttrs(ph7_context *pCtx, ph7_value *pMap, SySet *pAttrs)` |
|     1 |  105 | `{` |
|  5657 |  106 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|  5657 |  107 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|     - |  108 | `	sxu32 n;` |
|  5657 |  109 | `	if( pList == 0 ){` |
|   ! 0 |  110 | `		return;` |
|     - |  111 | `	}` |
|  5921 |  112 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|   265 |  113 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|   265 |  114 | `		if( pMeta == 0 ){ break; }` |
|   265 |  115 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aA[n].sName), (int)SyStringLength(&aA[n].sName));` |
|   265 |  116 | `		ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)aA[n].nLine);` |
|   265 |  117 | `		ph7_array_add_elem(pList, 0, pMeta);` |
|   133 |  118 | `	}` |
|  5657 |  119 | `	ph7_array_add_strkey_elem(pMap, "attrs", pList);` |
|  2829 |  120 | `}` |
|     - |  121 | `/* Emit a doc-comment field: the text when present, else boolean false` |
|     - |  122 | ` * (getDocComment()'s exact return contract). */` |
|  4404 |  123 | `static void ReflectMapAddDoc(ph7_context *pCtx, ph7_value *pMap, const SyString *pDoc)` |
|     1 |  124 | `{` |
|  4405 |  125 | `	if( SyStringLength(pDoc) > 0 ){` |
|    63 |  126 | `		ReflectMapAddStr(pCtx, pMap, "doc", SyStringData(pDoc), (int)SyStringLength(pDoc));` |
|    32 |  127 | `	}else{` |
|  4343 |  128 | `		ReflectMapAddBool(pCtx, pMap, "doc", 0);` |
|     - |  129 | `	}` |
|  4405 |  130 | `}` |
|     - |  131 | `/*` |
|     - |  132 | ` * Append pIface (and its parents / extended interfaces) to the dedup set` |
|     - |  133 | ` * of ph7_class pointers.` |
|     - |  134 | ` */` |
|   582 |  135 | `static void ReflectAddInterface(ph7_class *pIface, SySet *pOut, int iDepth)` |
|     1 |  136 | `{` |
|     - |  137 | `	ph7_class **apKnown;` |
|     - |  138 | `	sxu32 n;` |
|   583 |  139 | `	if( pIface == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|   ! 0 |  140 | `		return;` |
|     - |  141 | `	}` |
|     - |  142 | `	/* Parents of an interface come along too (interface B extends A) */` |
|   583 |  143 | `	if( pIface->pBase ){` |
|   109 |  144 | `		ReflectAddInterface(pIface->pBase, pOut, iDepth + 1);` |
|    54 |  145 | `	}` |
|     - |  146 | `	/* Some engines record extended interfaces in aInterface as well */` |
|   583 |  147 | `	apKnown = (ph7_class **)SySetBasePtr(&pIface->aInterface);` |
|   583 |  148 | `	for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){` |
|   ! 0 |  149 | `		ReflectAddInterface(apKnown[n], pOut, iDepth + 1);` |
|   ! 0 |  150 | `	}` |
|     - |  151 | `	/* Dedup by pointer */` |
|   583 |  152 | `	apKnown = (ph7_class **)SySetBasePtr(pOut);` |
|   731 |  153 | `	for( n = 0 ; n < SySetUsed(pOut) ; n++ ){` |
|   225 |  154 | `		if( apKnown[n] == pIface ){` |
|    77 |  155 | `			return;` |
|     - |  156 | `		}` |
|    75 |  157 | `	}` |
|   507 |  158 | `	SySetPut(pOut, (const void *)&pIface);` |
|   292 |  159 | `}` |
|     - |  160 | `/*` |
|     - |  161 | ` * Collect the transitive set of interfaces implemented by pClass:` |
|     - |  162 | ` * the parent chain's interfaces first, then the class's own.` |
|     - |  163 | ` */` |
|  1190 |  164 | `static void ReflectCollectInterfaces(ph7_class *pClass, SySet *pOut, int iDepth)` |
|     1 |  165 | `{` |
|     - |  166 | `	ph7_class **apIface;` |
|     - |  167 | `	sxu32 n;` |
|  1191 |  168 | `	if( pClass == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|   ! 0 |  169 | `		return;` |
|     - |  170 | `	}` |
|  1191 |  171 | `	if( pClass->pBase ){` |
|   283 |  172 | `		ReflectCollectInterfaces(pClass->pBase, pOut, iDepth + 1);` |
|   141 |  173 | `	}` |
|  1191 |  174 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  1657 |  175 | `	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){` |
|   467 |  176 | `		ReflectAddInterface(apIface[n], pOut, iDepth + 1);` |
|   234 |  177 | `	}` |
|   596 |  178 | `}` |
|     - |  179 | `/*` |
|     - |  180 | ` * Deepest base class whose method table maps the same name to the very` |
|     - |  181 | ` * same ph7_class_method pointer: inheritance shares member pointers` |
|     - |  182 | ` * (PH7_ClassInherit), so this identifies the declaring class. Methods` |
|     - |  183 | ` * copied in from traits are not on the pBase chain and thus report the` |
|     - |  184 | ` * using class, which is what PHP reports too.` |
|     - |  185 | ` */` |
|  4664 |  186 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth)` |
|     1 |  187 | `{` |
|  4665 |  188 | `	ph7_class *pDecl = pClass;` |
|  4665 |  189 | `	ph7_class *pBase = pClass->pBase;` |
|  4665 |  190 | `	int iDepth = 0;` |
|  5745 |  191 | `	while( pBase && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
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
|  4665 |  202 | `	return pDecl;` |
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
|   920 |  233 | `static int vm_builtin_reflect_class_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  234 | `{` |
|   921 |  235 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  236 | `	ph7_class *pClass;` |
|     - |  237 | `	ph7_value *pInfo, *pConsts, *pProps, *pMethods, *pList;` |
|     - |  238 | `	SyHashEntry *pEntry;` |
|     - |  239 | `	SySet aIfaceSet;` |
|   921 |  240 | `	sxi32 iCtorVis = 0, iCloneVis = 0;` |
|   921 |  241 | `	int bIterable = 0;` |
|     - |  242 | `	sxu32 n;` |
|   921 |  243 | `	if( nArg < 1 ){` |
|   ! 0 |  244 | `		ph7_result_null(pCtx);` |
|   ! 0 |  245 | `		return PH7_OK;` |
|     - |  246 | `	}` |
|   921 |  247 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|   921 |  248 | `	if( pClass == 0 ){` |
|    13 |  249 | `		ph7_result_null(pCtx);` |
|    13 |  250 | `		return PH7_OK;` |
|     - |  251 | `	}` |
|   909 |  252 | `	pInfo = ph7_context_new_array(pCtx);` |
|   909 |  253 | `	pConsts = ph7_context_new_array(pCtx);` |
|   909 |  254 | `	pProps = ph7_context_new_array(pCtx);` |
|   909 |  255 | `	pMethods = ph7_context_new_array(pCtx);` |
|   909 |  256 | `	if( pInfo == 0 \|\| pConsts == 0 \|\| pProps == 0 \|\| pMethods == 0 ){` |
|   ! 0 |  257 | `		ph7_result_null(pCtx);` |
|   ! 0 |  258 | `		return PH7_OK;` |
|     - |  259 | `	}` |
|   909 |  260 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   909 |  261 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pClass->iFlags & PH7_CLASS_INTERNAL) != 0);` |
|   909 |  262 | `	ReflectMapAddBool(pCtx, pInfo, "interface", (pClass->iFlags & PH7_CLASS_INTERFACE) != 0);` |
|   909 |  263 | `	ReflectMapAddBool(pCtx, pInfo, "trait", (pClass->iFlags & PH7_CLASS_TRAIT) != 0);` |
|   909 |  264 | `	ReflectMapAddBool(pCtx, pInfo, "abstract", (pClass->iFlags & PH7_CLASS_ABSTRACT) != 0);` |
|   909 |  265 | `	ReflectMapAddBool(pCtx, pInfo, "final", (pClass->iFlags & PH7_CLASS_FINAL) != 0);` |
|   909 |  266 | `	ReflectMapAddBool(pCtx, pInfo, "readonly", (pClass->iFlags & PH7_CLASS_READONLY) != 0);` |
|   909 |  267 | `	ReflectMapAddBool(pCtx, pInfo, "enum", (pClass->iFlags & PH7_CLASS_ENUM) != 0);` |
|   909 |  268 | `	if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|   ! 0 |  269 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "int", (int)sizeof("int")-1);` |
|   909 |  270 | `	}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|    67 |  271 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "string", (int)sizeof("string")-1);` |
|    34 |  272 | `	}else{` |
|   843 |  273 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "", 0);` |
|     - |  274 | `	}` |
|     - |  275 | `	{` |
|     - |  276 | `		/* Enum case names in declaration order (empty list for non-enums) */` |
|   909 |  277 | `		ph7_value *pCases = ph7_context_new_array(pCtx);` |
|   909 |  278 | `		if( pCases ){` |
|   909 |  279 | `			ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     - |  280 | `			sxu32 nCase;` |
|  1069 |  281 | `			for( nCase = 0 ; nCase < SySetUsed(&pClass->aEnumCases) ; nCase++ ){` |
|   161 |  282 | `				ph7_value *pNm = ph7_context_new_scalar(pCtx);` |
|   161 |  283 | `				if( pNm ){` |
|   161 |  284 | `					ph7_value_string(pNm,apCase[nCase]->sName.zString,(int)apCase[nCase]->sName.nByte);` |
|   161 |  285 | `					ph7_array_add_elem(pCases,0,pNm);` |
|    80 |  286 | `				}` |
|    81 |  287 | `			}` |
|   909 |  288 | `			ph7_array_add_strkey_elem(pInfo,"cases",pCases);` |
|   454 |  289 | `		}` |
|     - |  290 | `	}` |
|   909 |  291 | `	if( pClass->pBase ){` |
|   418 |  292 | `		ReflectMapAddStr(pCtx, pInfo, "parent", SyStringData(&pClass->pBase->sName),` |
|   278 |  293 | `			(int)SyStringLength(&pClass->pBase->sName));` |
|   140 |  294 | `	}else{` |
|   631 |  295 | `		ReflectMapAddNull(pCtx, pInfo, "parent");` |
|     - |  296 | `	}` |
|     - |  297 | `	/* Transitive interfaces */` |
|   909 |  298 | `	SySetInit(&aIfaceSet, &pVm->sAllocator, sizeof(ph7_class *));` |
|   909 |  299 | `	ReflectCollectInterfaces(pClass, &aIfaceSet, 0);` |
|   909 |  300 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     - |  301 | `		/* An interface's own parents count as its interface list */` |
|    47 |  302 | `		if( pClass->pBase ){` |
|     9 |  303 | `			ReflectAddInterface(pClass->pBase, &aIfaceSet, 0);` |
|     4 |  304 | `		}` |
|    23 |  305 | `	}` |
|   909 |  306 | `	pList = ph7_context_new_array(pCtx);` |
|   909 |  307 | `	if( pList ){` |
|   909 |  308 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIfaceSet);` |
|  1415 |  309 | `		for( n = 0 ; n < SySetUsed(&aIfaceSet) ; n++ ){` |
|   507 |  310 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|   507 |  311 | `			if( pName == 0 ){ break; }` |
|   507 |  312 | `			ph7_value_string(pName, SyStringData(&apIface[n]->sName), (int)SyStringLength(&apIface[n]->sName));` |
|   507 |  313 | `			ph7_array_add_elem(pList, 0, pName);` |
|   507 |  314 | `			if( pVm->pTraversableClass && apIface[n] == pVm->pTraversableClass ){` |
|    13 |  315 | `				bIterable = 1;` |
|     6 |  316 | `			}` |
|   254 |  317 | `		}` |
|   909 |  318 | `		ph7_array_add_strkey_elem(pInfo, "interfaces", pList);` |
|   454 |  319 | `	}` |
|   909 |  320 | `	SySetRelease(&aIfaceSet);` |
|   909 |  321 | `	ReflectMapAddBool(pCtx, pInfo, "iterable", bIterable);` |
|     - |  322 | `	/* Used traits */` |
|   909 |  323 | `	pList = ph7_context_new_array(pCtx);` |
|   909 |  324 | `	if( pList ){` |
|   909 |  325 | `		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|   921 |  326 | `		for( n = 0 ; n < SySetUsed(&pClass->aTrait) ; n++ ){` |
|    13 |  327 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|    13 |  328 | `			if( pName == 0 ){ break; }` |
|    13 |  329 | `			ph7_value_string(pName, SyStringData(&apTrait[n]->sName), (int)SyStringLength(&apTrait[n]->sName));` |
|    13 |  330 | `			ph7_array_add_elem(pList, 0, pName);` |
|     7 |  331 | `		}` |
|   909 |  332 | `		ph7_array_add_strkey_elem(pInfo, "traits", pList);` |
|   454 |  333 | `	}` |
|     - |  334 | `	/* File / lines: no file recorded => false, like PHP internals */` |
|   909 |  335 | `	if( SyStringLength(&pClass->sFile) > 0 ){` |
|   827 |  336 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));` |
|   414 |  337 | `	}else{` |
|    83 |  338 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - |  339 | `	}` |
|   909 |  340 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pClass->nLine);` |
|   909 |  341 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pClass->nEndLine);` |
|   909 |  342 | `	ReflectMapAddDoc(pCtx, pInfo, &pClass->sDoc);` |
|   909 |  343 | `	ReflectMapAddAttrs(pCtx, pInfo, &pClass->aAttrs);` |
|     - |  344 | `	/* Members are emitted in PHP's reporting order: the class's own members` |
|     - |  345 | `	 * first (declaration order), then each inheritance level's, outward.` |
|     - |  346 | `	 * Per level we iterate the DECLARING class's own hash — subclass hashes` |
|     - |  347 | `	 * interleave inherited pointers unpredictably — and emit buffered` |
|     - |  348 | `	 * entries in reverse, because SyHash lists are LIFO. A pointer-identity` |
|     - |  349 | `	 * lookup in the reflected class's hash filters out members that are not` |
|     - |  350 | `	 * visible there (base privates, overridden entries). */` |
|     - |  351 | `	{` |
|     - |  352 | `		ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];` |
|   909 |  353 | `		ph7_class *pWalk = pClass;` |
|     - |  354 | `		SySet aTmp;` |
|   909 |  355 | `		sxu32 nChain = 0, iLevel, nT;` |
|  2099 |  356 | `		while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){` |
|  1191 |  357 | `			aChain[nChain++] = pWalk;` |
|  1191 |  358 | `			pWalk = pWalk->pBase;` |
|     1 |  359 | `		}` |
|   909 |  360 | `		SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));` |
|  2099 |  361 | `		for( iLevel = 0 ; iLevel < nChain ; iLevel++ ){` |
|  1191 |  362 | `			ph7_class *pLevel = aChain[iLevel];` |
|     - |  363 | `			/* --- Constants and properties (shared attribute table) --- */` |
|  1191 |  364 | `			SySetReset(&aTmp);` |
|  1191 |  365 | `			SyHashResetLoopCursor(&pLevel->hAttr);` |
|  4269 |  366 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hAttr)) != 0 ){` |
|  3079 |  367 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  3079 |  368 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|  3079 |  369 | `				if( iLevel == 0 ){` |
|     - |  370 | `					sxu32 j;` |
|     - |  371 | `					/* Own = declared here or by an off-chain provider (trait) */` |
|  4101 |  372 | `					for( j = 1 ; j < nChain ; j++ ){` |
|  1287 |  373 | `						if( aChain[j] == pDecl ){ break; }` |
|   584 |  374 | `					}` |
|  2935 |  375 | `					if( j < nChain ){ continue; }` |
|  1408 |  376 | `				}else{` |
|     - |  377 | `					SyHashEntry *pSub;` |
|   145 |  378 | `					if( pDecl != pLevel ){ continue; }` |
|     - |  379 | `					/* Must still be the visible member in the reflected class */` |
|   121 |  380 | `					pSub = SyHashGet(&pClass->hAttr, pEntry->pKey, pEntry->nKeyLen);` |
|   121 |  381 | `					if( pSub == 0 \|\| pSub->pUserData != (void *)pAttr ){ continue; }` |
|     - |  382 | `				}` |
|  2935 |  383 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     1 |  384 | `			}` |
|  4125 |  385 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|  2935 |  386 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|  2935 |  387 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;` |
|  2935 |  388 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|  2935 |  389 | `				ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|  2935 |  390 | `				if( pMeta == 0 ){ break; }` |
|  2935 |  391 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pAttr->iProtection);` |
|  2935 |  392 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|  2935 |  393 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pAttr->nLine);` |
|  2935 |  394 | `				ReflectMapAddDoc(pCtx, pMeta, &pAttr->sDoc);` |
|  2935 |  395 | `				ReflectMapAddAttrs(pCtx, pMeta, &pAttr->aAttrs);` |
|  2935 |  396 | `				ReflectMapAddBool(pCtx, pMeta, "typed", (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);` |
|  2935 |  397 | `				if( SyStringLength(&pAttr->sTypeName) > 0 ){` |
|   904 |  398 | `					ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&pAttr->sTypeName),` |
|   602 |  399 | `						(int)SyStringLength(&pAttr->sTypeName));` |
|   302 |  400 | `				}else{` |
|  2333 |  401 | `					ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - |  402 | `				}` |
|  2935 |  403 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|   883 |  404 | `					ReflectMapAddBool(pCtx, pMeta, "final", (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   883 |  405 | `					ReflectMapAddBool(pCtx, pMeta, "enumcase", (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0);` |
|   883 |  406 | `					ReflectMapAddDyn(pCtx, pConsts, &pAttr->sName, pMeta);` |
|   442 |  407 | `				}else{` |
|  2053 |  408 | `					ReflectMapAddBool(pCtx, pMeta, "static", (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|  2053 |  409 | `					ReflectMapAddBool(pCtx, pMeta, "readonly", (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0);` |
|  2053 |  410 | `					ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&pAttr->aByteCode) > 0);` |
|  2053 |  411 | `					ReflectMapAddDyn(pCtx, pProps, &pAttr->sName, pMeta);` |
|     - |  412 | `				}` |
|  1468 |  413 | `			}` |
|     - |  414 | `			/* --- Methods. The reported name is the hash-entry key: trait` |
|     - |  415 | `			 * aliasing installs a shallow copy under the alias name while` |
|     - |  416 | `			 * sFunc.sName keeps the original, and PHP reports the alias. --- */` |
|  1191 |  417 | `			SySetReset(&aTmp);` |
|  1191 |  418 | `			SyHashResetLoopCursor(&pLevel->hMethod);` |
|  3559 |  419 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){` |
|  2369 |  420 | `				ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  2369 |  421 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|  2369 |  422 | `				if( iLevel == 0 ){` |
|     - |  423 | `					sxu32 j;` |
|  2761 |  424 | `					for( j = 1 ; j < nChain ; j++ ){` |
|  1011 |  425 | `						if( aChain[j] == pDecl ){ break; }` |
|   372 |  426 | `					}` |
|  2019 |  427 | `					if( j < nChain ){ continue; }` |
|   876 |  428 | `				}else{` |
|     - |  429 | `					SyHashEntry *pSub;` |
|   351 |  430 | `					if( pDecl != pLevel ){ continue; }` |
|   315 |  431 | `					pSub = SyHashGet(&pClass->hMethod, pEntry->pKey, pEntry->nKeyLen);` |
|   315 |  432 | `					if( pSub == 0 ){` |
|     - |  433 | `						/* Not in the subclass table: inheritance skips private` |
|     - |  434 | `						 * methods, but PHP still reports them on the subclass` |
|     - |  435 | `						 * (Zend copies privates into the child function table). */` |
|    21 |  436 | `						if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|   ! 0 |  437 | `							continue;` |
|     1 |  438 | `						}` |
|   305 |  439 | `					}else if( pSub->pUserData != (void *)pMeth ){` |
|     - |  440 | `						/* Overridden below this level: already reported */` |
|    27 |  441 | `						continue;` |
|     - |  442 | `					}` |
|     - |  443 | `				}` |
|  2039 |  444 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     1 |  445 | `			}` |
|  3229 |  446 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|  2039 |  447 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|  2039 |  448 | `				ph7_class_method *pMeth = (ph7_class_method *)pE->pUserData;` |
|  2039 |  449 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|     - |  450 | `				ph7_value *pMeta;` |
|     - |  451 | `				SyString sKey;` |
|     - |  452 | `				int bIsAlias;` |
|  2039 |  453 | `				SyStringInitFromBuf(&sKey, (const char *)pE->pKey, pE->nKeyLen);` |
|  4077 |  454 | `				bIsAlias = (sKey.nByte != SyStringLength(&pMeth->sFunc.sName)` |
|  2038 |  455 | `				 \|\| SyMemcmp(sKey.zString, SyStringData(&pMeth->sFunc.sName), sKey.nByte) != 0);` |
|  2038 |  456 | `				if( sKey.nByte == sizeof("__construct")-1` |
|  1222 |  457 | `				 && SyMemcmp(sKey.zString, "__construct", sKey.nByte) == 0 ){` |
|   375 |  458 | `					if( iCtorVis == 0 ){` |
|   375 |  459 | `						iCtorVis = pMeth->iProtection;` |
|   187 |  460 | `					}` |
|   375 |  461 | `					if( bIsAlias ){` |
|     - |  462 | `						/* Mount-time alias for a legacy class-name constructor:` |
|     - |  463 | `						 * the method is already listed under its declared name. */` |
|   ! 0 |  464 | `						continue;` |
|     - |  465 | `					}` |
|  1852 |  466 | `				}else if( sKey.nByte == sizeof("__clone")-1` |
|   927 |  467 | `				 && SyMemcmp(sKey.zString, "__clone", sKey.nByte) == 0 ){` |
|   ! 0 |  468 | `					if( iCloneVis == 0 ){` |
|   ! 0 |  469 | `						iCloneVis = pMeth->iProtection;` |
|   ! 0 |  470 | `					}` |
|  1664 |  471 | `				}else if( iCtorVis == 0` |
|  1198 |  472 | `				 && sKey.nByte == SyStringLength(&pClass->sName)` |
|   400 |  473 | `				 && SyMemcmp(sKey.zString, SyStringData(&pClass->sName), sKey.nByte) == 0 ){` |
|     - |  474 | `					/* Legacy class-name constructor before the mount alias exists */` |
|   ! 0 |  475 | `					iCtorVis = pMeth->iProtection;` |
|   ! 0 |  476 | `				}` |
|  2039 |  477 | `				pMeta = ph7_context_new_array(pCtx);` |
|  2039 |  478 | `				if( pMeta == 0 ){ break; }` |
|  2039 |  479 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pMeth->iProtection);` |
|  2039 |  480 | `				ReflectMapAddBool(pCtx, pMeta, "static", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|  2039 |  481 | `				ReflectMapAddBool(pCtx, pMeta, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|  2039 |  482 | `				ReflectMapAddBool(pCtx, pMeta, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|  2039 |  483 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|  2039 |  484 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pMeth->nLine);` |
|  2039 |  485 | `				ReflectMapAddDyn(pCtx, pMethods, &sKey, pMeta);` |
|  1020 |  486 | `			}` |
|   596 |  487 | `		}` |
|   909 |  488 | `		SySetRelease(&aTmp);` |
|     - |  489 | `	}` |
|   909 |  490 | `	ReflectMapAddInt(pCtx, pInfo, "ctorvis", (sxi64)iCtorVis);` |
|   909 |  491 | `	ReflectMapAddInt(pCtx, pInfo, "clonevis", (sxi64)iCloneVis);` |
|   909 |  492 | `	ph7_array_add_strkey_elem(pInfo, "consts", pConsts);` |
|   909 |  493 | `	ph7_array_add_strkey_elem(pInfo, "props", pProps);` |
|   909 |  494 | `	ph7_array_add_strkey_elem(pInfo, "methods", pMethods);` |
|   909 |  495 | `	ph7_result_value(pCtx, pInfo);` |
|   909 |  496 | `	return PH7_OK;` |
|   461 |  497 | `}` |
|     - |  498 | `/*` |
|     - |  499 | ` * mixed __reflect_const_value(string $class, string $name)` |
|     - |  500 | ` * Value of a class constant. The PHP layer guarantees existence.` |
|     - |  501 | ` */` |
|    42 |  502 | `static int vm_builtin_reflect_const_value(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  503 | `{` |
|     - |  504 | `	ph7_class *pClass;` |
|     - |  505 | `	ph7_class_attr *pAttr;` |
|     - |  506 | `	ph7_value *pValue;` |
|    42 |  507 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    42 |  508 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|    43 |  509 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|   ! 0 |  510 | `		ph7_result_null(pCtx);` |
|   ! 0 |  511 | `		return PH7_OK;` |
|     - |  512 | `	}` |
|     - |  513 | `	/* Constant slots are evaluated lazily on first access */` |
|    43 |  514 | `	if( PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr) != SXRET_OK ){` |
|     - |  515 | `		/* Initializer raised: the throw is in flight; report null here */` |
|   ! 0 |  516 | `		ph7_result_null(pCtx);` |
|   ! 0 |  517 | `		return PH7_OK;` |
|     - |  518 | `	}` |
|    43 |  519 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|    43 |  520 | `	if( pValue ){` |
|    43 |  521 | `		ph7_result_value(pCtx, pValue);` |
|    22 |  522 | `	}else{` |
|   ! 0 |  523 | `		ph7_result_null(pCtx);` |
|     - |  524 | `	}` |
|    43 |  525 | `	return PH7_OK;` |
|    22 |  526 | `}` |
|     - |  527 | `/*` |
|     - |  528 | ` * mixed __reflect_static_value(string $class, string $name)` |
|     - |  529 | ` * Current value of a static property (visibility ignored).` |
|     - |  530 | ` */` |
|    12 |  531 | `static int vm_builtin_reflect_static_value(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  532 | `{` |
|     - |  533 | `	ph7_class *pClass;` |
|     - |  534 | `	ph7_class_attr *pAttr;` |
|     - |  535 | `	ph7_value *pValue;` |
|    12 |  536 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    12 |  537 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|    13 |  538 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|   ! 0 |  539 | `		ph7_result_null(pCtx);` |
|   ! 0 |  540 | `		return PH7_OK;` |
|     - |  541 | `	}` |
|     - |  542 | `	{` |
|     - |  543 | `		/* Uninitialized typed static: same Error the VM raises on read */` |
|    13 |  544 | `		SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|    13 |  545 | `		if( pSlot ){` |
|   ! 0 |  546 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|   ! 0 |  547 | `			if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|   ! 0 |  548 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|   ! 0 |  549 | `				return PH7_VmThrowException(pCtx, "Error",` |
|     - |  550 | `					"Typed static property %z::$%z must not be accessed before initialization",` |
|   ! 0 |  551 | `					&pDecl->sName, &pAttr->sName);` |
|     - |  552 | `			}` |
|   ! 0 |  553 | `		}` |
|     - |  554 | `	}` |
|    13 |  555 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|    13 |  556 | `	if( pValue ){` |
|    13 |  557 | `		ph7_result_value(pCtx, pValue);` |
|     7 |  558 | `	}else{` |
|   ! 0 |  559 | `		ph7_result_null(pCtx);` |
|     - |  560 | `	}` |
|    13 |  561 | `	return PH7_OK;` |
|     7 |  562 | `}` |
|     - |  563 | `/*` |
|     - |  564 | ` * bool __reflect_static_set(string $class, string $name, mixed $value)` |
|     - |  565 | ` * Overwrite a static property's shared slot (visibility ignored).` |
|     - |  566 | ` */` |
|     4 |  567 | `static int vm_builtin_reflect_static_set(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  568 | `{` |
|     - |  569 | `	ph7_class *pClass;` |
|     - |  570 | `	ph7_class_attr *pAttr;` |
|     - |  571 | `	ph7_value *pValue;` |
|     4 |  572 | `	if( nArg < 3 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|     4 |  573 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|     5 |  574 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|   ! 0 |  575 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  576 | `		return PH7_OK;` |
|     - |  577 | `	}` |
|     5 |  578 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|     5 |  579 | `	if( pValue == 0 ){` |
|   ! 0 |  580 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  581 | `		return PH7_OK;` |
|     - |  582 | `	}` |
|     - |  583 | `	{` |
|     5 |  584 | `		sxi32 rc = ReflectEnforceStore(pCtx, pAttr->nIdx, apArg[2]);` |
|     5 |  585 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  586 | `			return rc;` |
|     - |  587 | `		}` |
|     - |  588 | `	}` |
|     5 |  589 | `	PH7_MemObjStore(apArg[2], pValue);` |
|     5 |  590 | `	ph7_result_bool(pCtx, 1);` |
|     5 |  591 | `	return PH7_OK;` |
|     3 |  592 | `}` |
|     - |  593 | `/*` |
|     - |  594 | ` * mixed __reflect_prop_default(string $class, string $name)` |
|     - |  595 | ` * Evaluate a non-static property's compiled default expression` |
|     - |  596 | ` * (null when the property has no default).` |
|     - |  597 | ` */` |
|    24 |  598 | `static int vm_builtin_reflect_prop_default(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  599 | `{` |
|     - |  600 | `	ph7_class *pClass;` |
|     - |  601 | `	ph7_class_attr *pAttr;` |
|     - |  602 | `	ph7_value sValue;` |
|    24 |  603 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    24 |  604 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|    24 |  605 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) != 0` |
|    25 |  606 | `	 \|\| SySetUsed(&pAttr->aByteCode) < 1 ){` |
|     3 |  607 | `		ph7_result_null(pCtx);` |
|     3 |  608 | `		return PH7_OK;` |
|     - |  609 | `	}` |
|    23 |  610 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     - |  611 | `	/* Same evaluation path the VM uses for omitted call arguments */` |
|    23 |  612 | `	VmLocalExec(pCtx->pVm, &pAttr->aByteCode, &sValue, FALSE);` |
|    23 |  613 | `	ph7_result_value(pCtx, &sValue);` |
|    23 |  614 | `	PH7_MemObjRelease(&sValue);` |
|    23 |  615 | `	return PH7_OK;` |
|    13 |  616 | `}` |
|     - |  617 | `/*` |
|     - |  618 | ` * Collect a PHP array's values into a ph7_value* set (call arguments).` |
|     - |  619 | ` * When ppNames is non-NULL, string keys become named arguments: a name` |
|     - |  620 | ` * map is lazily allocated (like call_user_func_array's) with one entry` |
|     - |  621 | ` * per collected slot, empty entries meaning positional.` |
|     - |  622 | ` */` |
|    30 |  623 | `static sxi32 ReflectCollectArgs(ph7_context *pCtx, ph7_value *pArray, SySet *pOut, SyString **ppNames)` |
|     1 |  624 | `{` |
|     - |  625 | `	ph7_hashmap *pMap;` |
|     - |  626 | `	ph7_hashmap_node *pEntry;` |
|    31 |  627 | `	SyString *aNames = 0;` |
|    31 |  628 | `	sxu32 nSlot = 0;` |
|     - |  629 | `	sxu32 n;` |
|    31 |  630 | `	if( ppNames ){` |
|    11 |  631 | `		*ppNames = 0;` |
|     5 |  632 | `	}` |
|    31 |  633 | `	if( !ph7_value_is_array(pArray) ){` |
|   ! 0 |  634 | `		return SXRET_OK;` |
|     - |  635 | `	}` |
|    31 |  636 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|    31 |  637 | `	pEntry = pMap->pFirst;` |
|    71 |  638 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    41 |  639 | `		ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|    41 |  640 | `		if( pValue ){` |
|    41 |  641 | `			if( ppNames && pEntry->iType == HASHMAP_BLOB_NODE ){` |
|     3 |  642 | `				if( aNames == 0 ){` |
|     4 |  643 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     2 |  644 | `						pMap->nEntry * sizeof(SyString));` |
|     3 |  645 | `					if( aNames ){` |
|     3 |  646 | `						SyZero(aNames, pMap->nEntry * sizeof(SyString));` |
|     1 |  647 | `					}` |
|     1 |  648 | `				}` |
|     3 |  649 | `				if( aNames ){` |
|     3 |  650 | `					SyStringInitFromBuf(&aNames[nSlot],` |
|     - |  651 | `						SyBlobData(&pEntry->xKey.sKey), SyBlobLength(&pEntry->xKey.sKey));` |
|     1 |  652 | `				}` |
|     1 |  653 | `			}` |
|    41 |  654 | `			SySetPut(pOut, (const void *)&pValue);` |
|    41 |  655 | `			nSlot++;` |
|    20 |  656 | `		}` |
|    41 |  657 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|    21 |  658 | `	}` |
|    31 |  659 | `	if( ppNames ){` |
|    11 |  660 | `		*ppNames = aNames;` |
|     5 |  661 | `	}` |
|    31 |  662 | `	return SXRET_OK;` |
|    16 |  663 | `}` |
|     - |  664 | `/*` |
|     - |  665 | ` * object __reflect_new_instance(string $class, array $args)` |
|     - |  666 | ` * Instantiate and run the constructor with positional arguments.` |
|     - |  667 | ` * The PHP layer has already validated instantiability and ctor visibility.` |
|     - |  668 | ` */` |
|    14 |  669 | `static int vm_builtin_reflect_new_instance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  670 | `{` |
|    15 |  671 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  672 | `	ph7_class *pClass;` |
|     - |  673 | `	ph7_class_instance *pThis;` |
|     - |  674 | `	ph7_class_method *pCons;` |
|    15 |  675 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pVm, apArg[0])) == 0 ){` |
|   ! 0 |  676 | `		ph7_result_null(pCtx);` |
|   ! 0 |  677 | `		return PH7_OK;` |
|     - |  678 | `	}` |
|    15 |  679 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|    15 |  680 | `	if( pThis == 0 ){` |
|   ! 0 |  681 | `		ph7_result_null(pCtx);` |
|   ! 0 |  682 | `		return PH7_OK;` |
|     - |  683 | `	}` |
|    15 |  684 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|    15 |  685 | `	if( pCons ){` |
|     - |  686 | `		SySet aArg;` |
|     - |  687 | `		sxi32 rc;` |
|    11 |  688 | `		SyString *aNames = 0;` |
|    11 |  689 | `		SySetInit(&aArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    11 |  690 | `		if( nArg > 1 ){` |
|    11 |  691 | `			ReflectCollectArgs(pCtx, apArg[1], &aArg, &aNames);` |
|     5 |  692 | `		}` |
|    11 |  693 | `		if( aNames ){` |
|     - |  694 | `			VmCallArgMap sMap;` |
|     3 |  695 | `			sMap.bHasNamed = 1;` |
|     3 |  696 | `			sMap.bIsNamespaced = 0;` |
|     3 |  697 | `			sMap.bStrict = 0;` |
|     3 |  698 | `			sMap.nTotal = SySetUsed(&aArg);` |
|     3 |  699 | `			sMap.aNames = aNames;` |
|     4 |  700 | `			rc = PH7_VmCallClassMethodMap(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|     2 |  701 | `				(ph7_value **)SySetBasePtr(&aArg), &sMap);` |
|     3 |  702 | `			SyMemBackendFree(&pVm->sAllocator, aNames);` |
|     2 |  703 | `		}else{` |
|    13 |  704 | `			rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|     8 |  705 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|     - |  706 | `		}` |
|    11 |  707 | `		SySetRelease(&aArg);` |
|    11 |  708 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 |  709 | `			PH7_ClassInstanceUnref(pThis);` |
|   ! 0 |  710 | `			return rc;` |
|     - |  711 | `		}` |
|     5 |  712 | `	}` |
|    15 |  713 | `	return ReflectResultObject(pCtx, pThis);` |
|     8 |  714 | `}` |
|     - |  715 | `/*` |
|     - |  716 | ` * object __reflect_new_no_ctor(string $class)` |
|     - |  717 | ` * Instantiate without running the constructor (property defaults still` |
|     - |  718 | ` * apply — PH7_NewClassInstance builds the attribute frame).` |
|     - |  719 | ` */` |
|    52 |  720 | `static int vm_builtin_reflect_new_no_ctor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  721 | `{` |
|     - |  722 | `	ph7_class *pClass;` |
|    53 |  723 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0 ){` |
|   ! 0 |  724 | `		ph7_result_null(pCtx);` |
|   ! 0 |  725 | `		return PH7_OK;` |
|     - |  726 | `	}` |
|    53 |  727 | `	return ReflectResultObject(pCtx, PH7_NewClassInstance(pCtx->pVm, pClass));` |
|    27 |  728 | `}` |
|     - |  729 | `/*` |
|     - |  730 | ` * Typed/readonly store enforcement for reflection writes. Like the VM's` |
|     - |  731 | ` * store path, except an UNINITIALIZED readonly property may be written from` |
|     - |  732 | ` * any scope (PHP lets ReflectionProperty::setValue initialize readonly): the` |
|     - |  733 | ` * READONLY bit is masked off for the enforcement call so the set-scope check` |
|     - |  734 | ` * is skipped, while an already-initialized readonly still gets PHP's` |
|     - |  735 | ` * "Cannot modify readonly property" Error. Returns SXRET_OK/PH7_EXCEPTION/` |
|     - |  736 | ` * PH7_ABORT; the value may be coerced in place.` |
|     - |  737 | ` */` |
|    10 |  738 | `static sxi32 ReflectEnforceStore(ph7_context *pCtx, sxu32 nIdx, ph7_value *pValue)` |
|     1 |  739 | `{` |
|    11 |  740 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  741 | `	SyHashEntry *pSlot;` |
|     - |  742 | `	VmClassAttr *pVmAttr;` |
|     - |  743 | `	ph7_class_attr *pAttr;` |
|     - |  744 | `	sxi32 iSaved, rc;` |
|    11 |  745 | `	pSlot = SyHashGet(&pVm->hTypedSlot, (const void *)&nIdx, sizeof(sxu32));` |
|    11 |  746 | `	if( pSlot == 0 ){` |
|     7 |  747 | `		return SXRET_OK; /* Untyped slot: plain store */` |
|     - |  748 | `	}` |
|     5 |  749 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     5 |  750 | `	pAttr = pVmAttr->pAttr;` |
|     5 |  751 | `	if( pAttr == 0 ){` |
|   ! 0 |  752 | `		return SXRET_OK;` |
|     - |  753 | `	}` |
|     5 |  754 | `	iSaved = pAttr->iFlags;` |
|     5 |  755 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) ){` |
|   ! 0 |  756 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_READONLY;` |
|   ! 0 |  757 | `	}` |
|     5 |  758 | `	rc = PH7_VmEnforcePropStore(pVm, nIdx, pValue);` |
|     5 |  759 | `	pAttr->iFlags = iSaved;` |
|     5 |  760 | `	return rc;` |
|     6 |  761 | `}` |
|     - |  762 | `/*` |
|     - |  763 | ` * mixed __reflect_prop_read(object $obj, string $name)` |
|     - |  764 | ` * Instance property read, visibility ignored. Throws PHP's Error for an` |
|     - |  765 | ` * uninitialized typed property.` |
|     - |  766 | ` */` |
|    20 |  767 | `static int vm_builtin_reflect_prop_read(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  768 | `{` |
|     - |  769 | `	ph7_class_instance *pThis;` |
|     - |  770 | `	SyHashEntry *pEntry;` |
|     - |  771 | `	VmClassAttr *pVmAttr;` |
|     - |  772 | `	ph7_value *pValue;` |
|     - |  773 | `	const char *zName;` |
|     - |  774 | `	int nLen;` |
|    21 |  775 | `	if( nArg < 2 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  776 | `		ph7_result_null(pCtx);` |
|   ! 0 |  777 | `		return PH7_OK;` |
|     - |  778 | `	}` |
|    21 |  779 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    21 |  780 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|    21 |  781 | `	pEntry = nLen > 0 ? SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen) : 0;` |
|    21 |  782 | `	if( pEntry == 0 ){` |
|   ! 0 |  783 | `		ph7_result_null(pCtx);` |
|   ! 0 |  784 | `		return PH7_OK;` |
|     - |  785 | `	}` |
|    21 |  786 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    21 |  787 | `	if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|     3 |  788 | `		ph7_class *pDecl = pVmAttr->pAttr->pDeclClass ? pVmAttr->pAttr->pDeclClass : pThis->pClass;` |
|     4 |  789 | `		return PH7_VmThrowException(pCtx, "Error",` |
|     - |  790 | `			"Typed property %z::$%z must not be accessed before initialization",` |
|     2 |  791 | `			&pDecl->sName, &pVmAttr->pAttr->sName);` |
|     - |  792 | `	}` |
|    19 |  793 | `	pValue = PH7_ClassInstanceExtractAttrValue(pThis, pVmAttr);` |
|    19 |  794 | `	if( pValue ){` |
|    19 |  795 | `		ph7_result_value(pCtx, pValue);` |
|    10 |  796 | `	}else{` |
|   ! 0 |  797 | `		ph7_result_null(pCtx);` |
|     - |  798 | `	}` |
|    19 |  799 | `	return PH7_OK;` |
|    11 |  800 | `}` |
|     - |  801 | `/*` |
|     - |  802 | ` * bool __reflect_prop_write(object $obj, string $name, mixed $value)` |
|     - |  803 | ` * Instance property write, visibility ignored; typed and readonly rules` |
|     - |  804 | ` * enforced (see ReflectEnforceStore).` |
|     - |  805 | ` */` |
|     6 |  806 | `static int vm_builtin_reflect_prop_write(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  807 | `{` |
|     - |  808 | `	ph7_class_instance *pThis;` |
|     - |  809 | `	SyHashEntry *pEntry;` |
|     - |  810 | `	VmClassAttr *pVmAttr;` |
|     - |  811 | `	ph7_value *pValue;` |
|     - |  812 | `	const char *zName;` |
|     - |  813 | `	sxi32 rc;` |
|     - |  814 | `	int nLen;` |
|     7 |  815 | `	if( nArg < 3 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  816 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  817 | `		return PH7_OK;` |
|     - |  818 | `	}` |
|     7 |  819 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     7 |  820 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|     7 |  821 | `	pEntry = nLen > 0 ? SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen) : 0;` |
|     7 |  822 | `	if( pEntry == 0 ){` |
|   ! 0 |  823 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  824 | `		return PH7_OK;` |
|     - |  825 | `	}` |
|     7 |  826 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     7 |  827 | `	rc = ReflectEnforceStore(pCtx, pVmAttr->nIdx, apArg[2]);` |
|     7 |  828 | `	if( rc != SXRET_OK ){` |
|     3 |  829 | `		return rc;` |
|     - |  830 | `	}` |
|     5 |  831 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|     5 |  832 | `	if( pValue == 0 ){` |
|   ! 0 |  833 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  834 | `		return PH7_OK;` |
|     - |  835 | `	}` |
|     5 |  836 | `	PH7_MemObjStore(apArg[2], pValue);` |
|     5 |  837 | `	ph7_result_bool(pCtx, 1);` |
|     5 |  838 | `	return PH7_OK;` |
|     4 |  839 | `}` |
|     - |  840 | `/*` |
|     - |  841 | ` * int __reflect_prop_state(object\|string $target, string $name)` |
|     - |  842 | ` * Bitfield: 1 = exists (instance attr / static slot), 2 = initialized,` |
|     - |  843 | ` * 4 = dynamic (instance-owned, not class-declared).` |
|     - |  844 | ` */` |
|    16 |  845 | `static int vm_builtin_reflect_prop_state(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  846 | `{` |
|    17 |  847 | `	int iState = 0;` |
|     - |  848 | `	const char *zName;` |
|     - |  849 | `	int nLen;` |
|    17 |  850 | `	if( nArg < 2 ){` |
|   ! 0 |  851 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  852 | `		return PH7_OK;` |
|     - |  853 | `	}` |
|    17 |  854 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|    17 |  855 | `	if( nLen < 1 ){` |
|   ! 0 |  856 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  857 | `		return PH7_OK;` |
|     - |  858 | `	}` |
|    17 |  859 | `	if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|    17 |  860 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    17 |  861 | `		SyHashEntry *pEntry = SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen);` |
|    17 |  862 | `		if( pEntry ){` |
|    17 |  863 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    17 |  864 | `			iState \|= 1;` |
|    17 |  865 | `			if( (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|    15 |  866 | `				iState \|= 2;` |
|     7 |  867 | `			}` |
|    17 |  868 | `			if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|    11 |  869 | `				iState \|= 4;` |
|     5 |  870 | `			}` |
|     8 |  871 | `		}` |
|     9 |  872 | `	}else{` |
|   ! 0 |  873 | `		ph7_class *pClass = ReflectResolveClass(pCtx->pVm, apArg[0]);` |
|   ! 0 |  874 | `		ph7_class_attr *pAttr = pClass ? ReflectFetchAttr(pClass, apArg[1]) : 0;` |
|   ! 0 |  875 | `		if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|   ! 0 |  876 | `			SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|   ! 0 |  877 | `			iState \|= 1 \| 2;` |
|   ! 0 |  878 | `			if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|   ! 0 |  879 | `				iState &= ~2;` |
|   ! 0 |  880 | `			}` |
|   ! 0 |  881 | `		}` |
|     - |  882 | `	}` |
|    17 |  883 | `	ph7_result_int(pCtx, iState);` |
|    17 |  884 | `	return PH7_OK;` |
|     9 |  885 | `}` |
|     - |  886 | `/*` |
|     - |  887 | ` * array __reflect_dyn_props(object $obj)` |
|     - |  888 | ` * Names of the instance's runtime-added (dynamic) properties, in creation` |
|     - |  889 | ` * order (the instance attr table inserts dynamics at the tail).` |
|     - |  890 | ` */` |
|     2 |  891 | `static int vm_builtin_reflect_dyn_props(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  892 | `{` |
|     - |  893 | `	ph7_class_instance *pThis;` |
|     - |  894 | `	SyHashEntry *pEntry;` |
|     - |  895 | `	ph7_value *pList;` |
|     2 |  896 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|     3 |  897 | `	 \|\| (pList = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 |  898 | `		ph7_result_null(pCtx);` |
|   ! 0 |  899 | `		return PH7_OK;` |
|     - |  900 | `	}` |
|     3 |  901 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     3 |  902 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     7 |  903 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     5 |  904 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     5 |  905 | `		if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|     5 |  906 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|     5 |  907 | `			if( pName == 0 ){ break; }` |
|     7 |  908 | `			ph7_value_string(pName, SyStringData(&pVmAttr->pAttr->sName),` |
|     4 |  909 | `				(int)SyStringLength(&pVmAttr->pAttr->sName));` |
|     5 |  910 | `			ph7_array_add_elem(pList, 0, pName);` |
|     2 |  911 | `		}` |
|     1 |  912 | `	}` |
|     3 |  913 | `	ph7_result_value(pCtx, pList);` |
|     3 |  914 | `	return PH7_OK;` |
|     2 |  915 | `}` |
|     - |  916 | `/* Hand an EXISTING instance to the caller: takes an extra reference` |
|     - |  917 | ` * (unlike ReflectResultObject, which transfers a fresh instance's one). */` |
|     4 |  918 | `static int ReflectResultExistingObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|     1 |  919 | `{` |
|     5 |  920 | `	if( pObj == 0 ){` |
|   ! 0 |  921 | `		ph7_result_null(pCtx);` |
|   ! 0 |  922 | `		return PH7_OK;` |
|     - |  923 | `	}` |
|     5 |  924 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     5 |  925 | `	pObj->iRef++;` |
|     5 |  926 | `	pCtx->pRet->x.pOther = pObj;` |
|     5 |  927 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|     5 |  928 | `	return PH7_OK;` |
|     3 |  929 | `}` |
|     - |  930 | `/* pVal is a Closure instance? Return it, else NULL. */` |
|   430 |  931 | `static ph7_class_instance * ReflectValueClosure(ph7_vm *pVm, ph7_value *pVal)` |
|     1 |  932 | `{` |
|     - |  933 | `	ph7_class_instance *pThis;` |
|   431 |  934 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
|   391 |  935 | `		return 0;` |
|     - |  936 | `	}` |
|    41 |  937 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    41 |  938 | `	return (pThis->pClass == pVm->pClosureClass) ? pThis : 0;` |
|   216 |  939 | `}` |
|     - |  940 | `/*` |
|     - |  941 | ` * Resolve a reflection callable target into its compiled function.` |
|     - |  942 | ` *   - pMethodArg a non-empty string  -> method mode: pTarget is a class name` |
|     - |  943 | ` *     or object; outputs *ppClass and *ppMeth.` |
|     - |  944 | ` *   - pTarget a Closure              -> unwrap $__fn into hFunction; *ppClosure.` |
|     - |  945 | ` *   - pTarget a string               -> hFunction (user) or hHostFunction` |
|     - |  946 | ` *     (*ppHost set, returns NULL).` |
|     - |  947 | ` * Returns the ph7_vm_func, or NULL (host function or unresolvable).` |
|     - |  948 | ` */` |
|   710 |  949 | `static ph7_vm_func * ReflectResolveCallable(ph7_context *pCtx, ph7_value *pTarget,` |
|     - |  950 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|     - |  951 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|     1 |  952 | `{` |
|   711 |  953 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  954 | `	SyHashEntry *pEntry;` |
|   711 |  955 | `	if( ppClass ){ *ppClass = 0; }` |
|   711 |  956 | `	if( ppMeth ){ *ppMeth = 0; }` |
|   711 |  957 | `	if( ppHost ){ *ppHost = 0; }` |
|   711 |  958 | `	if( ppClosure ){ *ppClosure = 0; }` |
|   711 |  959 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
|   281 |  960 | `		ph7_class *pClass = ReflectResolveClass(pVm, pTarget);` |
|     - |  961 | `		ph7_class_method *pMeth;` |
|   281 |  962 | `		if( pClass == 0 ){` |
|   ! 0 |  963 | `			return 0;` |
|     - |  964 | `		}` |
|   421 |  965 | `		pMeth = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|   140 |  966 | `			SyBlobLength(&pMethodArg->sBlob));` |
|   281 |  967 | `		if( pMeth == 0 ){` |
|   ! 0 |  968 | `			return 0;` |
|     - |  969 | `		}` |
|   281 |  970 | `		if( ppClass ){ *ppClass = pClass; }` |
|   281 |  971 | `		if( ppMeth ){ *ppMeth = pMeth; }` |
|   281 |  972 | `		return &pMeth->sFunc;` |
|     - |  973 | `	}` |
|     - |  974 | `	{` |
|   431 |  975 | `		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|   431 |  976 | `		if( pClo ){` |
|     - |  977 | `			SyString sAttr;` |
|     - |  978 | `			ph7_value *pFn;` |
|    41 |  979 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    41 |  980 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|    41 |  981 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) < 1 ){` |
|   ! 0 |  982 | `				return 0;` |
|     - |  983 | `			}` |
|    41 |  984 | `			pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    41 |  985 | `			if( pEntry == 0 ){` |
|     - |  986 | `				/* A Closure over a host function (Closure::fromCallable('strlen')) */` |
|   ! 0 |  987 | `				pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|   ! 0 |  988 | `				if( pEntry && ppHost ){` |
|   ! 0 |  989 | `					*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|   ! 0 |  990 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|   ! 0 |  991 | `				}` |
|   ! 0 |  992 | `				return 0;` |
|     - |  993 | `			}` |
|    41 |  994 | `			if( ppClosure ){ *ppClosure = pClo; }` |
|    41 |  995 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - |  996 | `		}` |
|     - |  997 | `	}` |
|   391 |  998 | `	if( pTarget->iFlags & MEMOBJ_STRING ){` |
|   391 |  999 | `		if( SyBlobLength(&pTarget->sBlob) < 1 ){` |
|   ! 0 | 1000 | `			return 0;` |
|     - | 1001 | `		}` |
|   391 | 1002 | `		pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   391 | 1003 | `		if( pEntry ){` |
|   285 | 1004 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - | 1005 | `		}` |
|   107 | 1006 | `		pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   107 | 1007 | `		if( pEntry && ppHost ){` |
|   105 | 1008 | `			*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|    52 | 1009 | `		}` |
|    53 | 1010 | `	}` |
|   107 | 1011 | `	return 0;` |
|   356 | 1012 | `}` |
|     - | 1013 | `/* Emit the shared descriptor fields of a compiled function. */` |
|   562 | 1014 | `static void ReflectFillFuncCommon(ph7_context *pCtx, ph7_value *pInfo, ph7_vm_func *pFunc)` |
|     1 | 1015 | `{` |
|     - | 1016 | `	ph7_vm_func_arg *aArg;` |
|     - | 1017 | `	ph7_value *pParams, *pStatics;` |
|   563 | 1018 | `	int bVariadic = 0;` |
|     - | 1019 | `	int bAnon;` |
|     - | 1020 | `	sxu32 n;` |
|     - | 1021 | ``	/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|     - | 1022 | `	 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|   563 | 1023 | `	bAnon = (pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|   562 | 1024 | `	if( !bAnon && SyStringLength(&pFunc->sName) > 9` |
|   306 | 1025 | `	 && (SyMemcmp(SyStringData(&pFunc->sName), "[lambda_", 8) == 0` |
|    82 | 1026 | `	  \|\| SyMemcmp(SyStringData(&pFunc->sName), "[closure_", 9) == 0) ){` |
|     5 | 1027 | `		bAnon = 1;` |
|     2 | 1028 | `	}` |
|   563 | 1029 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|   563 | 1030 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pFunc->iFlags & VM_FUNC_INTERNAL) != 0);` |
|   563 | 1031 | `	ReflectMapAddBool(pCtx, pInfo, "closure", bAnon);` |
|   563 | 1032 | `	ReflectMapAddBool(pCtx, pInfo, "fstatic", (pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);` |
|   563 | 1033 | `	ReflectMapAddBool(pCtx, pInfo, "byref", (pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|   563 | 1034 | `	ReflectMapAddBool(pCtx, pInfo, "generator", (pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|   563 | 1035 | `	ReflectMapAddBool(pCtx, pInfo, "strict", pFunc->bStrictTypes != 0);` |
|   563 | 1036 | `	if( SyStringLength(&pFunc->sFile) > 0 ){` |
|   559 | 1037 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pFunc->sFile), (int)SyStringLength(&pFunc->sFile));` |
|   280 | 1038 | `	}else{` |
|     5 | 1039 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1040 | `	}` |
|   563 | 1041 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pFunc->nLine);` |
|   563 | 1042 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pFunc->nEndLine);` |
|   563 | 1043 | `	ReflectMapAddDoc(pCtx, pInfo, &pFunc->sDoc);` |
|   563 | 1044 | `	ReflectMapAddAttrs(pCtx, pInfo, &pFunc->aAttrs);` |
|   563 | 1045 | `	if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|   145 | 1046 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", SyStringData(&pFunc->sReturnTypeName),` |
|    96 | 1047 | `			(int)SyStringLength(&pFunc->sReturnTypeName));` |
|   515 | 1048 | `	}else if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     - | 1049 | `		/* The type-text renderer omits void/never atoms (compile.c notes the` |
|     - | 1050 | `		 * root fix belongs there); name them here for getReturnType(). */` |
|     3 | 1051 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "void", sizeof("void")-1);` |
|   466 | 1052 | `	}else if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|     3 | 1053 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "never", sizeof("never")-1);` |
|     2 | 1054 | `	}else{` |
|   463 | 1055 | `		ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1056 | `	}` |
|   563 | 1057 | `	ReflectMapAddBool(pCtx, pInfo, "retnullable", (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) != 0);` |
|     - | 1058 | `	/* Parameters */` |
|   563 | 1059 | `	pParams = ph7_context_new_array(pCtx);` |
|   563 | 1060 | `	aArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|  1815 | 1061 | `	for( n = 0 ; pParams && n < SySetUsed(&pFunc->aArgs) ; n++ ){` |
|  1253 | 1062 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|  1253 | 1063 | `		if( pMeta == 0 ){ break; }` |
|  1253 | 1064 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aArg[n].sName), (int)SyStringLength(&aArg[n].sName));` |
|  1253 | 1065 | `		ReflectMapAddInt(pCtx, pMeta, "pos", (sxi64)n);` |
|  1253 | 1066 | `		ReflectMapAddBool(pCtx, pMeta, "byref", (aArg[n].iFlags & VM_FUNC_ARG_BY_REF) != 0);` |
|  1253 | 1067 | `		ReflectMapAddBool(pCtx, pMeta, "variadic", (aArg[n].iFlags & VM_FUNC_ARG_VARIADIC) != 0);` |
|     - | 1068 | `		/* The compiler never sets ARG_HAS_DEF; a default = compiled bytecode` |
|     - | 1069 | `		 * (same test the OP_CALL default-value path uses). */` |
|  1253 | 1070 | `		ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&aArg[n].aByteCode) > 0);` |
|  1253 | 1071 | `		ReflectMapAddBool(pCtx, pMeta, "nullable", (aArg[n].iFlags & VM_FUNC_ARG_NULLABLE) != 0);` |
|  1253 | 1072 | `		ReflectMapAddBool(pCtx, pMeta, "promoted", (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) != 0);` |
|  1253 | 1073 | `		if( SyStringLength(&aArg[n].sTypeName) > 0 ){` |
|   715 | 1074 | `			ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&aArg[n].sTypeName),` |
|   476 | 1075 | `				(int)SyStringLength(&aArg[n].sTypeName));` |
|   239 | 1076 | `		}else{` |
|   777 | 1077 | `			ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - | 1078 | `		}` |
|  1253 | 1079 | `		ReflectMapAddAttrs(pCtx, pMeta, &aArg[n].aAttrs);` |
|  1253 | 1080 | `		ph7_array_add_elem(pParams, 0, pMeta);` |
|  1253 | 1081 | `		if( aArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|   183 | 1082 | `			bVariadic = 1;` |
|    91 | 1083 | `		}` |
|   627 | 1084 | `	}` |
|   563 | 1085 | `	if( pParams ){` |
|   563 | 1086 | `		ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|   281 | 1087 | `	}` |
|   563 | 1088 | `	ReflectMapAddBool(pCtx, pInfo, "variadic", bVariadic);` |
|     - | 1089 | `	/* Static variables: current value when the slot was initialized (first` |
|     - | 1090 | `	 * call), otherwise the evaluated default — PHP's getStaticVariables` |
|     - | 1091 | `	 * initializes on demand and reports the same values. */` |
|   563 | 1092 | `	pStatics = ph7_context_new_array(pCtx);` |
|   563 | 1093 | `	if( pStatics ){` |
|   563 | 1094 | `		ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|   591 | 1095 | `		for( n = 0 ; n < SySetUsed(&pFunc->aStatic) ; n++ ){` |
|    29 | 1096 | `			ph7_value *pVal = 0;` |
|     - | 1097 | `			ph7_value sScratch;` |
|    29 | 1098 | `			int bScratch = 0;` |
|    29 | 1099 | `			if( aStatic[n].nIdx != SXU32_HIGH ){` |
|    11 | 1100 | `				pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);` |
|     5 | 1101 | `			}` |
|    29 | 1102 | `			if( pVal == 0 ){` |
|    19 | 1103 | `				PH7_MemObjInit(pCtx->pVm, &sScratch);` |
|    19 | 1104 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|    19 | 1105 | `					VmLocalExec(pCtx->pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|     9 | 1106 | `				}` |
|    19 | 1107 | `				pVal = &sScratch;` |
|    19 | 1108 | `				bScratch = 1;` |
|     9 | 1109 | `			}` |
|    29 | 1110 | `			ReflectMapAddDyn(pCtx, pStatics, &aStatic[n].sName, pVal);` |
|    29 | 1111 | `			if( bScratch ){` |
|    19 | 1112 | `				PH7_MemObjRelease(&sScratch);` |
|     9 | 1113 | `			}` |
|    15 | 1114 | `		}` |
|   563 | 1115 | `		ph7_array_add_strkey_elem(pInfo, "statics", pStatics);` |
|   281 | 1116 | `	}` |
|   563 | 1117 | `}` |
|     - | 1118 | `/*` |
|     - | 1119 | ` * array\|null __reflect_func_info(string\|Closure $target [, string $method])` |
|     - | 1120 | ` * Function/method/closure descriptor for the PHP layer.` |
|     - | 1121 | ` */` |
|   668 | 1122 | `static int vm_builtin_reflect_func_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1123 | `{` |
|     - | 1124 | `	ph7_vm_func *pFunc;` |
|   669 | 1125 | `	ph7_class *pClass = 0;` |
|   669 | 1126 | `	ph7_class_method *pMeth = 0;` |
|   669 | 1127 | `	ph7_user_func *pHost = 0;` |
|   669 | 1128 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1129 | `	ph7_value *pInfo;` |
|   669 | 1130 | `	if( nArg < 1 ){` |
|   ! 0 | 1131 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1132 | `		return PH7_OK;` |
|     - | 1133 | `	}` |
|   669 | 1134 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], nArg > 1 ? apArg[1] : 0,` |
|     - | 1135 | `		&pClass, &pMeth, &pHost, &pClosure);` |
|   669 | 1136 | `	if( pFunc == 0 && pHost == 0 ){` |
|     3 | 1137 | `		ph7_result_null(pCtx);` |
|     3 | 1138 | `		return PH7_OK;` |
|     - | 1139 | `	}` |
|   667 | 1140 | `	pInfo = ph7_context_new_array(pCtx);` |
|   667 | 1141 | `	if( pInfo == 0 ){` |
|   ! 0 | 1142 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1143 | `		return PH7_OK;` |
|     - | 1144 | `	}` |
|   667 | 1145 | `	if( pFunc == 0 ){` |
|     - | 1146 | `		/* Host (C builtin) function: no parameter metadata beyond arity */` |
|   105 | 1147 | `		ph7_value *pParams = ph7_context_new_array(pCtx);` |
|   105 | 1148 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pHost->sName), (int)SyStringLength(&pHost->sName));` |
|   105 | 1149 | `		ReflectMapAddBool(pCtx, pInfo, "internal", 1);` |
|   105 | 1150 | `		ReflectMapAddBool(pCtx, pInfo, "closure", 0);` |
|   105 | 1151 | `		ReflectMapAddBool(pCtx, pInfo, "fstatic", 0);` |
|   105 | 1152 | `		ReflectMapAddBool(pCtx, pInfo, "byref", 0);` |
|   105 | 1153 | `		ReflectMapAddBool(pCtx, pInfo, "generator", 0);` |
|   105 | 1154 | `		ReflectMapAddBool(pCtx, pInfo, "strict", 0);` |
|   105 | 1155 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|   105 | 1156 | `		ReflectMapAddInt(pCtx, pInfo, "line", 0);` |
|   105 | 1157 | `		ReflectMapAddInt(pCtx, pInfo, "endline", 0);` |
|   105 | 1158 | `		ReflectMapAddBool(pCtx, pInfo, "doc", 0);` |
|     - | 1159 | `		{` |
|   105 | 1160 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|   105 | 1161 | `			if( pEmpty ){` |
|   105 | 1162 | `				ph7_array_add_strkey_elem(pInfo, "attrs", pEmpty);` |
|    52 | 1163 | `			}` |
|     - | 1164 | `		}` |
|   105 | 1165 | `		if( pHost->zRet ){` |
|   105 | 1166 | `			ReflectMapAddStr(pCtx, pInfo, "rettext", pHost->zRet, (int)SyStrlen(pHost->zRet));` |
|    53 | 1167 | `		}else{` |
|   ! 0 | 1168 | `			ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1169 | `		}` |
|   105 | 1170 | `		ReflectMapAddBool(pCtx, pInfo, "retnullable", 0);` |
|   105 | 1171 | `		if( pParams ){` |
|   105 | 1172 | `			ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|    52 | 1173 | `		}` |
|   105 | 1174 | `		ReflectMapAddBool(pCtx, pInfo, "variadic", 0);` |
|   105 | 1175 | `		ReflectMapAddInt(pCtx, pInfo, "minarg", (sxi64)pHost->nMinArg);` |
|   105 | 1176 | `		if( pHost->zSig ){` |
|   105 | 1177 | `			ReflectMapAddStr(pCtx, pInfo, "sig", pHost->zSig, (int)SyStrlen(pHost->zSig));` |
|    53 | 1178 | `		}else{` |
|   ! 0 | 1179 | `			ReflectMapAddStr(pCtx, pInfo, "sig", "", 0);` |
|     - | 1180 | `		}` |
|   105 | 1181 | `		ph7_result_value(pCtx, pInfo);` |
|   105 | 1182 | `		return PH7_OK;` |
|     - | 1183 | `	}` |
|   563 | 1184 | `	ReflectFillFuncCommon(pCtx, pInfo, pFunc);` |
|   563 | 1185 | `	ReflectMapAddInt(pCtx, pInfo, "minarg", -1);` |
|   563 | 1186 | `	if( (pFunc->iFlags & VM_FUNC_INTERNAL) && SySetUsed(&pFunc->aArgs) == 0 && pMeth == 0 ){` |
|     - | 1187 | `		/* Embedded-PHP builtin (max/min...): declared argless, actual` |
|     - | 1188 | `		 * signature comes from the static table */` |
|     5 | 1189 | `		const char *zRet = 0;` |
|     5 | 1190 | `		const char *zSig = PH7_VmBuiltinSigLookup(SyStringData(&pFunc->sName), SyStringLength(&pFunc->sName), &zRet);` |
|     5 | 1191 | `		if( zSig ){` |
|     5 | 1192 | `			ReflectMapAddStr(pCtx, pInfo, "sig", zSig, (int)SyStrlen(zSig));` |
|     2 | 1193 | `		}` |
|     5 | 1194 | `		if( zRet && SyStringLength(&pFunc->sReturnTypeName) == 0 ){` |
|     5 | 1195 | `			ReflectMapAddStr(pCtx, pInfo, "ret2", zRet, (int)SyStrlen(zRet));` |
|     2 | 1196 | `		}` |
|     2 | 1197 | `	}` |
|   563 | 1198 | `	if( pMeth && pClass ){` |
|   259 | 1199 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|   259 | 1200 | `		ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   259 | 1201 | `		ReflectMapAddStr(pCtx, pInfo, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   259 | 1202 | `		ReflectMapAddInt(pCtx, pInfo, "vis", (sxi64)pMeth->iProtection);` |
|   259 | 1203 | `		ReflectMapAddBool(pCtx, pInfo, "mstatic", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   259 | 1204 | `		ReflectMapAddBool(pCtx, pInfo, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|   259 | 1205 | `		ReflectMapAddBool(pCtx, pInfo, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   129 | 1206 | `	}` |
|   563 | 1207 | `	if( pClosure ){` |
|     - | 1208 | `		SyString sAttr;` |
|     - | 1209 | `		ph7_value *pAttr;` |
|     - | 1210 | `		ph7_value *pUsed;` |
|    41 | 1211 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|    41 | 1212 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    41 | 1213 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|   ! 0 | 1214 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 1215 | `			if( pKey ){` |
|   ! 0 | 1216 | `				ph7_value_string(pKey, "this", 4);` |
|   ! 0 | 1217 | `				ph7_array_add_elem(pInfo, pKey, pAttr);` |
|   ! 0 | 1218 | `			}` |
|   ! 0 | 1219 | `		}else{` |
|    41 | 1220 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|     - | 1221 | `		}` |
|    41 | 1222 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|    41 | 1223 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    41 | 1224 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|   ! 0 | 1225 | `			ReflectMapAddStr(pCtx, pInfo, "scope", (const char *)SyBlobData(&pAttr->sBlob),` |
|   ! 0 | 1226 | `				(int)SyBlobLength(&pAttr->sBlob));` |
|   ! 0 | 1227 | `		}else{` |
|    41 | 1228 | `			ReflectMapAddNull(pCtx, pInfo, "scope");` |
|     - | 1229 | `		}` |
|     - | 1230 | `		/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */` |
|    41 | 1231 | `		pUsed = ph7_context_new_array(pCtx);` |
|    41 | 1232 | `		if( pUsed ){` |
|    41 | 1233 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     - | 1234 | `			sxu32 n;` |
|    91 | 1235 | `			for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; n++ ){` |
|    51 | 1236 | `				if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){` |
|    33 | 1237 | `					continue;` |
|     - | 1238 | `				}` |
|    18 | 1239 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|    10 | 1240 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|   ! 0 | 1241 | `					continue;` |
|     - | 1242 | `				}` |
|    19 | 1243 | `				if( (aEnv[n].iFlags & VM_FUNC_ARG_BY_REF) && aEnv[n].nIdx != SXU32_HIGH ){` |
|     - | 1244 | `					/* Captured by reference: report the slot's live value */` |
|     5 | 1245 | `					ph7_value *pLive = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aEnv[n].nIdx);` |
|     5 | 1246 | `					ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, pLive ? pLive : &aEnv[n].sValue);` |
|     5 | 1247 | `					continue;` |
|     - | 1248 | `				}` |
|    15 | 1249 | `				ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, &aEnv[n].sValue);` |
|     8 | 1250 | `			}` |
|    41 | 1251 | `			ph7_array_add_strkey_elem(pInfo, "used", pUsed);` |
|    20 | 1252 | `		}` |
|    20 | 1253 | `	}` |
|   563 | 1254 | `	ph7_result_value(pCtx, pInfo);` |
|   563 | 1255 | `	return PH7_OK;` |
|   335 | 1256 | `}` |
|     - | 1257 | `/*` |
|     - | 1258 | ` * mixed __reflect_param_default(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1259 | ` * Evaluate a parameter's compiled default expression.` |
|     - | 1260 | ` */` |
|    12 | 1261 | `static int vm_builtin_reflect_param_default(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1262 | `{` |
|     - | 1263 | `	ph7_vm_func *pFunc;` |
|     - | 1264 | `	ph7_vm_func_arg *pArg;` |
|     - | 1265 | `	ph7_value sValue;` |
|     - | 1266 | `	sxu32 nIdx;` |
|    13 | 1267 | `	if( nArg < 3 ){` |
|   ! 0 | 1268 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1269 | `		return PH7_OK;` |
|     - | 1270 | `	}` |
|    13 | 1271 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|    13 | 1272 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|    12 | 1273 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|    13 | 1274 | `	 \|\| SySetUsed(&pArg->aByteCode) < 1 ){` |
|   ! 0 | 1275 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1276 | `		return PH7_OK;` |
|     - | 1277 | `	}` |
|    13 | 1278 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|    13 | 1279 | `	VmLocalExec(pCtx->pVm, &pArg->aByteCode, &sValue, FALSE);` |
|    13 | 1280 | `	ph7_result_value(pCtx, &sValue);` |
|    13 | 1281 | `	PH7_MemObjRelease(&sValue);` |
|    13 | 1282 | `	return PH7_OK;` |
|     7 | 1283 | `}` |
|     - | 1284 | `/*` |
|     - | 1285 | ` * string\|null __reflect_param_defconst(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1286 | ` * When a parameter's default is a plain global-constant reference, its` |
|     - | 1287 | ` * source name; null otherwise. A constant default compiles to exactly` |
|     - | 1288 | ` * [ OP_LOADC (EXPAND) , OP_DONE ] with the name in the literal table.` |
|     - | 1289 | ` */` |
|     6 | 1290 | `static int vm_builtin_reflect_param_defconst(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1291 | `{` |
|     - | 1292 | `	ph7_vm_func *pFunc;` |
|     - | 1293 | `	ph7_vm_func_arg *pArg;` |
|     - | 1294 | `	VmInstr *aInstr;` |
|     - | 1295 | `	ph7_value *pLit;` |
|     - | 1296 | `	sxu32 nIdx;` |
|     7 | 1297 | `	if( nArg < 3 ){` |
|   ! 0 | 1298 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1299 | `		return PH7_OK;` |
|     - | 1300 | `	}` |
|     7 | 1301 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|     7 | 1302 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|     6 | 1303 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|     7 | 1304 | `	 \|\| SySetUsed(&pArg->aByteCode) != 2 ){` |
|     3 | 1305 | `		ph7_result_null(pCtx);` |
|     3 | 1306 | `		return PH7_OK;` |
|     - | 1307 | `	}` |
|     5 | 1308 | `	aInstr = (VmInstr *)SySetBasePtr(&pArg->aByteCode);` |
|     4 | 1309 | `	if( aInstr[0].iOp != PH7_OP_LOADC \|\| (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0` |
|     5 | 1310 | `	 \|\| aInstr[1].iOp != PH7_OP_DONE ){` |
|   ! 0 | 1311 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1312 | `		return PH7_OK;` |
|     - | 1313 | `	}` |
|     5 | 1314 | `	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);` |
|     5 | 1315 | `	if( pLit == 0 \|\| SyBlobLength(&pLit->sBlob) < 1 ){` |
|   ! 0 | 1316 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1317 | `		return PH7_OK;` |
|     - | 1318 | `	}` |
|     5 | 1319 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&pLit->sBlob), (int)SyBlobLength(&pLit->sBlob));` |
|     5 | 1320 | `	return PH7_OK;` |
|     4 | 1321 | `}` |
|     - | 1322 | `/*` |
|     - | 1323 | ` * mixed __reflect_invoke(mixed $target, ?string $method, ?object $this, array $args)` |
|     - | 1324 | ` * Visibility-bypassing invocation (methods dispatch by VM name; functions` |
|     - | 1325 | ` * and closures ride PH7_VmCallUserFunction like call_user_func_array).` |
|     - | 1326 | ` */` |
|    20 | 1327 | `static int vm_builtin_reflect_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1328 | `{` |
|    21 | 1329 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1330 | `	ph7_value sResult;` |
|     - | 1331 | `	SySet aCallArg;` |
|     - | 1332 | `	sxi32 rc;` |
|    21 | 1333 | `	if( nArg < 4 ){` |
|   ! 0 | 1334 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1335 | `		return PH7_OK;` |
|     - | 1336 | `	}` |
|    21 | 1337 | `	PH7_MemObjInit(pVm, &sResult);` |
|    21 | 1338 | `	sResult.nIdx = SXU32_HIGH;` |
|    21 | 1339 | `	SySetInit(&aCallArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    21 | 1340 | `	ReflectCollectArgs(pCtx, apArg[3], &aCallArg, 0);` |
|    21 | 1341 | `	if( (apArg[1]->iFlags & MEMOBJ_STRING) && SyBlobLength(&apArg[1]->sBlob) > 0 ){` |
|    11 | 1342 | `		ph7_class *pClass = 0;` |
|    11 | 1343 | `		ph7_class_method *pMeth = 0;` |
|    11 | 1344 | `		ph7_class_instance *pThis = 0;` |
|    11 | 1345 | `		ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, 0);` |
|    11 | 1346 | `		if( pMeth == 0 ){` |
|   ! 0 | 1347 | `			SySetRelease(&aCallArg);` |
|   ! 0 | 1348 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1349 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1350 | `			return PH7_OK;` |
|     - | 1351 | `		}` |
|    11 | 1352 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     7 | 1353 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     3 | 1354 | `		}` |
|     - | 1355 | `		/* Reflection ignores method visibility (PHP 8.1+); the flag is` |
|     - | 1356 | `		 * consumed by the first OP_CALL, i.e. this synthetic one. */` |
|    11 | 1357 | `		pVm->bReflectBypass = 1;` |
|    16 | 1358 | `		rc = PH7_VmCallClassMethod(pVm, pThis, pMeth, &sResult,` |
|    10 | 1359 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg));` |
|    11 | 1360 | `		pVm->bReflectBypass = 0;` |
|     6 | 1361 | `	}else{` |
|    16 | 1362 | `		rc = PH7_VmCallUserFunction(pVm, apArg[0],` |
|    10 | 1363 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg), &sResult);` |
|     - | 1364 | `	}` |
|    21 | 1365 | `	SySetRelease(&aCallArg);` |
|    21 | 1366 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 | 1367 | `		PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1368 | `		return rc;` |
|     - | 1369 | `	}` |
|    21 | 1370 | `	ph7_result_value(pCtx, &sResult);` |
|    21 | 1371 | `	PH7_MemObjRelease(&sResult);` |
|    21 | 1372 | `	return PH7_OK;` |
|    11 | 1373 | `}` |
|     - | 1374 | `/*` |
|     - | 1375 | ` * Closure __reflect_closure(mixed $target, ?string $method, ?object $this)` |
|     - | 1376 | ` * Mint a Closure for a function or method, bound and scoped like the` |
|     - | 1377 | ` * first-class-callable path.` |
|     - | 1378 | ` */` |
|     6 | 1379 | `static int vm_builtin_reflect_closure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1380 | `{` |
|     7 | 1381 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 1382 | `	ph7_class *pClass = 0;` |
|     7 | 1383 | `	ph7_class_method *pMeth = 0;` |
|     7 | 1384 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1385 | `	ph7_vm_func *pFunc;` |
|     7 | 1386 | `	if( nArg < 3 ){` |
|   ! 0 | 1387 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1388 | `		return PH7_OK;` |
|     - | 1389 | `	}` |
|     7 | 1390 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, &pClosure);` |
|     7 | 1391 | `	if( pClosure ){` |
|     - | 1392 | `		/* Already a Closure: hand the same instance back */` |
|   ! 0 | 1393 | `		return ReflectResultExistingObject(pCtx, pClosure);` |
|     - | 1394 | `	}` |
|     7 | 1395 | `	if( pMeth && pClass ){` |
|     5 | 1396 | `		ph7_class_instance *pThis = 0;` |
|     5 | 1397 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     3 | 1398 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     1 | 1399 | `		}` |
|     7 | 1400 | `		return ReflectResultObject(pCtx,` |
|     4 | 1401 | `			PH7_VmNewClosure(pVm, &pMeth->sFunc.sName, pThis, &pClass->sName));` |
|     - | 1402 | `	}` |
|     3 | 1403 | `	if( pFunc ){` |
|     3 | 1404 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &pFunc->sName, 0, 0));` |
|     - | 1405 | `	}` |
|     - | 1406 | `	/* Host function by name */` |
|   ! 0 | 1407 | `	if( apArg[0]->iFlags & MEMOBJ_STRING ){` |
|     - | 1408 | `		SyString sName;` |
|   ! 0 | 1409 | `		SyStringInitFromBuf(&sName, (const char *)SyBlobData(&apArg[0]->sBlob), SyBlobLength(&apArg[0]->sBlob));` |
|   ! 0 | 1410 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &sName, 0, 0));` |
|     - | 1411 | `	}` |
|   ! 0 | 1412 | `	ph7_result_null(pCtx);` |
|   ! 0 | 1413 | `	return PH7_OK;` |
|     4 | 1414 | `}` |
|     - | 1415 | `/*` |
|     - | 1416 | ` * Resolve a Generator object into its wrapper. Mirrors the static` |
|     - | 1417 | ` * VmGeneratorExtractCtx in vm.c: the $__ctx attribute carries the` |
|     - | 1418 | ` * ph7_generator pointer as a resource value.` |
|     - | 1419 | ` */` |
|    22 | 1420 | `static ph7_generator * ReflectGeneratorCtx(ph7_vm *pVm, ph7_value *pVal)` |
|     1 | 1421 | `{` |
|     - | 1422 | `	ph7_class_instance *pThis;` |
|     - | 1423 | `	ph7_value *pAttr;` |
|     - | 1424 | `	SyString sAttr;` |
|    23 | 1425 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVm->pGeneratorClass == 0 ){` |
|   ! 0 | 1426 | `		return 0;` |
|     - | 1427 | `	}` |
|    23 | 1428 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    23 | 1429 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|   ! 0 | 1430 | `		return 0;` |
|     - | 1431 | `	}` |
|    23 | 1432 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    23 | 1433 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    23 | 1434 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|   ! 0 | 1435 | `		return 0;` |
|     - | 1436 | `	}` |
|    23 | 1437 | `	return (ph7_generator *)pAttr->x.pOther;` |
|    12 | 1438 | `}` |
|     - | 1439 | `/*` |
|     - | 1440 | ` * array\|null __reflect_gen_info(Generator $g)` |
|     - | 1441 | ` * {state, closed, executing, kind ('fn'\|'method'), name, class?, this}` |
|     - | 1442 | ` */` |
|    16 | 1443 | `static int vm_builtin_reflect_gen_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1444 | `{` |
|    17 | 1445 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1446 | `	ph7_generator *pGen;` |
|     - | 1447 | `	ph7_exec_ctx *pExec;` |
|     - | 1448 | `	ph7_value *pInfo;` |
|    17 | 1449 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 \|\| pGen->pCtx == 0 ){` |
|   ! 0 | 1450 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1451 | `		return PH7_OK;` |
|     - | 1452 | `	}` |
|    17 | 1453 | `	pExec = pGen->pCtx;` |
|    17 | 1454 | `	pInfo = ph7_context_new_array(pCtx);` |
|    17 | 1455 | `	if( pInfo == 0 ){` |
|   ! 0 | 1456 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1457 | `		return PH7_OK;` |
|     - | 1458 | `	}` |
|    17 | 1459 | `	ReflectMapAddInt(pCtx, pInfo, "state", (sxi64)pExec->iState);` |
|    24 | 1460 | `	ReflectMapAddBool(pCtx, pInfo, "closed",` |
|    16 | 1461 | `		pExec->iState == PH7_CTX_STATE_COMPLETED \|\| pExec->iState == PH7_CTX_STATE_CLOSED);` |
|    17 | 1462 | `	ReflectMapAddBool(pCtx, pInfo, "executing", pVm->pActiveCtx == pExec);` |
|    17 | 1463 | `	if( pExec->pFunc ){` |
|    17 | 1464 | `		ph7_vm_func *pFunc = pExec->pFunc;` |
|    19 | 1465 | `		if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     5 | 1466 | `			ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|     5 | 1467 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "method", sizeof("method")-1);` |
|     5 | 1468 | `			ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|     3 | 1469 | `		}else{` |
|    13 | 1470 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "fn", sizeof("fn")-1);` |
|     - | 1471 | `		}` |
|    17 | 1472 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|     8 | 1473 | `	}` |
|     - | 1474 | `	{` |
|     - | 1475 | `		/* The coroutine frame installs $this as a frame VARIABLE (see` |
|     - | 1476 | `		 * VmFiberSetupFrame), not as pFrame->pThis — check both. */` |
|    17 | 1477 | `		ph7_value *pThisVal = 0;` |
|    17 | 1478 | `		if( pExec->pFrame ){` |
|    17 | 1479 | `			SyHashEntry *pVar = SyHashGet(&pExec->pFrame->hVar, "this", sizeof("this")-1);` |
|    17 | 1480 | `			if( pVar ){` |
|     5 | 1481 | `				ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj, (sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|     5 | 1482 | `				if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|     5 | 1483 | `					pThisVal = pSlot;` |
|     2 | 1484 | `				}` |
|     2 | 1485 | `			}` |
|    17 | 1486 | `			if( pThisVal == 0 && pExec->pFrame->pThis ){` |
|     - | 1487 | `				ph7_value sThis;` |
|   ! 0 | 1488 | `				ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 1489 | `				PH7_MemObjInit(pVm, &sThis);` |
|   ! 0 | 1490 | `				pExec->pFrame->pThis->iRef++;` |
|   ! 0 | 1491 | `				sThis.x.pOther = pExec->pFrame->pThis;` |
|   ! 0 | 1492 | `				MemObjSetType(&sThis, MEMOBJ_OBJ);` |
|   ! 0 | 1493 | `				if( pKey ){` |
|   ! 0 | 1494 | `					ph7_value_string(pKey, "this", 4);` |
|   ! 0 | 1495 | `					ph7_array_add_elem(pInfo, pKey, &sThis); /* copies (takes its own ref) */` |
|   ! 0 | 1496 | `				}` |
|   ! 0 | 1497 | `				PH7_MemObjRelease(&sThis);` |
|   ! 0 | 1498 | `				pThisVal = (ph7_value *)1; /* handled */` |
|   ! 0 | 1499 | `			}` |
|     8 | 1500 | `		}` |
|    17 | 1501 | `		if( pThisVal == 0 ){` |
|    13 | 1502 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|    11 | 1503 | `		}else if( pThisVal != (ph7_value *)1 ){` |
|     5 | 1504 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|     5 | 1505 | `			if( pKey ){` |
|     5 | 1506 | `				ph7_value_string(pKey, "this", 4);` |
|     5 | 1507 | `				ph7_array_add_elem(pInfo, pKey, pThisVal);` |
|     2 | 1508 | `			}` |
|     2 | 1509 | `		}` |
|     - | 1510 | `	}` |
|    17 | 1511 | `	ph7_result_value(pCtx, pInfo);` |
|    17 | 1512 | `	return PH7_OK;` |
|     9 | 1513 | `}` |
|     - | 1514 | `/*` |
|     - | 1515 | ` * Generator __reflect_gen_exec(Generator $g)` |
|     - | 1516 | `` * Follow `yield from` delegation to the innermost executing generator`` |
|     - | 1517 | ` * (PHP's ReflectionGenerator::getExecutingGenerator).` |
|     - | 1518 | ` */` |
|     4 | 1519 | `static int vm_builtin_reflect_gen_exec(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1520 | `{` |
|     5 | 1521 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1522 | `	ph7_generator *pGen;` |
|     - | 1523 | `	ph7_value *pCur;` |
|     5 | 1524 | `	int iDepth = 0;` |
|     5 | 1525 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 ){` |
|   ! 0 | 1526 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1527 | `		return PH7_OK;` |
|     - | 1528 | `	}` |
|     5 | 1529 | `	pCur = apArg[0];` |
|     9 | 1530 | `	while( pGen && pGen->pCtx && pGen->pCtx->iDelegateState == 3` |
|     8 | 1531 | `	 && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|     3 | 1532 | `		ph7_generator *pInner = ReflectGeneratorCtx(pVm, &pGen->pCtx->sDelegate);` |
|     3 | 1533 | `		if( pInner == 0 ){` |
|   ! 0 | 1534 | `			break;` |
|     - | 1535 | `		}` |
|     3 | 1536 | `		pCur = &pGen->pCtx->sDelegate;` |
|     3 | 1537 | `		pGen = pInner;` |
|     3 | 1538 | `		iDepth++;` |
|     1 | 1539 | `	}` |
|     5 | 1540 | `	return ReflectResultExistingObject(pCtx, (ph7_class_instance *)pCur->x.pOther);` |
|     3 | 1541 | `}` |
|     - | 1542 | `/*` |
|     - | 1543 | ` * array\|null __reflect_const_info(string $name)` |
|     - | 1544 | ` * Global-constant descriptor: {value}. Null when undefined. File/origin` |
|     - | 1545 | ` * metadata arrives with the C5 constant-metadata work.` |
|     - | 1546 | ` */` |
|    36 | 1547 | `static int vm_builtin_reflect_const_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1548 | `{` |
|    37 | 1549 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1550 | `	SyHashEntry *pEntry;` |
|     - | 1551 | `	ph7_constant *pCons;` |
|     - | 1552 | `	ph7_value *pInfo;` |
|     - | 1553 | `	ph7_value sValue;` |
|     - | 1554 | `	const char *zName;` |
|     - | 1555 | `	int nLen;` |
|    37 | 1556 | `	if( nArg < 1 ){` |
|   ! 0 | 1557 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1558 | `		return PH7_OK;` |
|     - | 1559 | `	}` |
|    37 | 1560 | `	zName = ph7_value_to_string(apArg[0], &nLen);` |
|    37 | 1561 | `	pEntry = nLen > 0 ? SyHashGet(&pVm->hConstant, (const void *)zName, (sxu32)nLen) : 0;` |
|    37 | 1562 | `	if( pEntry == 0 ){` |
|     3 | 1563 | `		ph7_result_null(pCtx);` |
|     3 | 1564 | `		return PH7_OK;` |
|     - | 1565 | `	}` |
|    35 | 1566 | `	pCons = (ph7_constant *)pEntry->pUserData;` |
|    35 | 1567 | `	pInfo = ph7_context_new_array(pCtx);` |
|    35 | 1568 | `	if( pInfo == 0 ){` |
|   ! 0 | 1569 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1570 | `		return PH7_OK;` |
|     - | 1571 | `	}` |
|    35 | 1572 | `	PH7_MemObjInit(pVm, &sValue);` |
|    35 | 1573 | `	if( pCons->xExpand ){` |
|    35 | 1574 | `		pCons->xExpand(&sValue, pCons->pUserData);` |
|    17 | 1575 | `	}` |
|     - | 1576 | `	{` |
|    35 | 1577 | `		ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|    35 | 1578 | `		if( pKey ){` |
|    35 | 1579 | `			ph7_value_string(pKey, "value", 5);` |
|    35 | 1580 | `			ph7_array_add_elem(pInfo, pKey, &sValue);` |
|    17 | 1581 | `		}` |
|     - | 1582 | `	}` |
|    35 | 1583 | `	PH7_MemObjRelease(&sValue);` |
|    35 | 1584 | `	ReflectMapAddBool(pCtx, pInfo, "internal", pCons->bUserDefined == 0);` |
|    35 | 1585 | `	if( SyStringLength(&pCons->sFile) > 0 ){` |
|    21 | 1586 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));` |
|    11 | 1587 | `	}else{` |
|    15 | 1588 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1589 | `	}` |
|    35 | 1590 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pCons->nLine);` |
|    35 | 1591 | `	ph7_result_value(pCtx, pInfo);` |
|    35 | 1592 | `	return PH7_OK;` |
|    19 | 1593 | `}` |
|     - | 1594 | `/*` |
|     - | 1595 | ` * int\|null __reflect_ref_id(array $arr, int\|string $key)` |
|     - | 1596 | ` * The element's slot index when the element is a reference (its slot has` |
|     - | 1597 | ` * a reference-table record with at least two links), null otherwise.` |
|     - | 1598 | ` */` |
|     6 | 1599 | `static int vm_builtin_reflect_ref_id(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1600 | `{` |
|     - | 1601 | `	ph7_hashmap *pMap;` |
|     7 | 1602 | `	ph7_hashmap_node *pNode = 0;` |
|     7 | 1603 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|   ! 0 | 1604 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1605 | `		return PH7_OK;` |
|     - | 1606 | `	}` |
|     7 | 1607 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 | 1608 | `	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK \|\| pNode == 0 ){` |
|   ! 0 | 1609 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1610 | `		return PH7_OK;` |
|     - | 1611 | `	}` |
|     7 | 1612 | `	if( PH7_VmSlotRefCount(pCtx->pVm, pNode->nValIdx) < 2 ){` |
|     3 | 1613 | `		ph7_result_null(pCtx);` |
|     3 | 1614 | `		return PH7_OK;` |
|     - | 1615 | `	}` |
|     5 | 1616 | `	ph7_result_int64(pCtx, (sxi64)pNode->nValIdx);` |
|     5 | 1617 | `	return PH7_OK;` |
|     4 | 1618 | `}` |
|     - | 1619 | `/*` |
|     - | 1620 | ` * array\|null __reflect_attr_args(string $kind, mixed $target, ?string $member,` |
|     - | 1621 | ` *                                int $paramIdx, int $attrIdx)` |
|     - | 1622 | ` * Evaluate the recorded argument expressions of one declared attribute:` |
|     - | 1623 | ` * kind 'class' (target = class), 'attr' (class + property/constant name),` |
|     - | 1624 | ` * 'method' (class + method), 'fn' (function name or Closure), 'param'` |
|     - | 1625 | ` * (function spec + parameter index). Named arguments become string keys.` |
|     - | 1626 | ` */` |
|    36 | 1627 | `static int vm_builtin_reflect_attr_args(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1628 | `{` |
|    37 | 1629 | `	ph7_vm *pVm = pCtx->pVm;` |
|    37 | 1630 | `	SySet *pAttrs = 0;` |
|     - | 1631 | `	ph7_attribute *pAttrRec;` |
|     - | 1632 | `	ph7_value *pOut;` |
|     - | 1633 | `	const char *zKind;` |
|     - | 1634 | `	int nKind;` |
|     - | 1635 | `	sxu32 nAttrIdx, n;` |
|    37 | 1636 | `	if( nArg < 5 ){` |
|   ! 0 | 1637 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1638 | `		return PH7_OK;` |
|     - | 1639 | `	}` |
|    37 | 1640 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|    37 | 1641 | `	nAttrIdx = (sxu32)ph7_value_to_int(apArg[4]);` |
|    49 | 1642 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|    25 | 1643 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|    25 | 1644 | `		if( pClass ){ pAttrs = &pClass->aAttrs; }` |
|    27 | 1645 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|     5 | 1646 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|     5 | 1647 | `		ph7_class_attr *pMember = pClass ? ReflectFetchAttr(pClass, apArg[2]) : 0;` |
|     5 | 1648 | `		if( pMember ){ pAttrs = &pMember->aAttrs; }` |
|    12 | 1649 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|     3 | 1650 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1651 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|    10 | 1652 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|     5 | 1653 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], 0, 0, 0, 0, 0);` |
|     5 | 1654 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|     5 | 1655 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|     3 | 1656 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1657 | `		ph7_vm_func_arg *pParam = pFunc` |
|     2 | 1658 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|     3 | 1659 | `		if( pParam ){ pAttrs = &pParam->aAttrs; }` |
|     1 | 1660 | `	}` |
|    36 | 1661 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|    37 | 1662 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 | 1663 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1664 | `		return PH7_OK;` |
|     - | 1665 | `	}` |
|    71 | 1666 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|    35 | 1667 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|     - | 1668 | `		ph7_value sValue;` |
|    35 | 1669 | `		PH7_MemObjInit(pVm, &sValue);` |
|    35 | 1670 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|    35 | 1671 | `			VmLocalExec(pVm, &pArgRec->aByteCode, &sValue, FALSE);` |
|    17 | 1672 | `		}` |
|    35 | 1673 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|     5 | 1674 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|     3 | 1675 | `		}else{` |
|    31 | 1676 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|     - | 1677 | `		}` |
|    35 | 1678 | `		PH7_MemObjRelease(&sValue);` |
|    18 | 1679 | `	}` |
|    37 | 1680 | `	ph7_result_value(pCtx, pOut);` |
|    37 | 1681 | `	return PH7_OK;` |
|    19 | 1682 | `}` |
|     - | 1683 | `/*` |
|     - | 1684 | ` * The Reflection classes, in PHP. Chunk 1: exceptions, Reflector,` |
|     - | 1685 | ` * Reflection, ReflectionClass, ReflectionObject (plus get_debug_type,` |
|     - | 1686 | ` * which the TypeError messages need and PHP 8.0 ships natively).` |
|     - | 1687 | ` */` |
|     - | 1688 | `static const char zReflectLib1[] =` |
|     - | 1689 | `"function get_debug_type($value){"` |
|     - | 1690 | `" if(is_object($value)){ return get_class($value); }"` |
|     - | 1691 | `" if(is_bool($value)){ return 'bool'; }"` |
|     - | 1692 | `" if(is_int($value)){ return 'int'; }"` |
|     - | 1693 | `" if(is_float($value)){ return 'float'; }"` |
|     - | 1694 | `" if(is_string($value)){ return 'string'; }"` |
|     - | 1695 | `" if(is_array($value)){ return 'array'; }"` |
|     - | 1696 | `" if($value === null){ return 'null'; }"` |
|     - | 1697 | `" return gettype($value);"` |
|     - | 1698 | `"}"` |
|     - | 1699 | `"interface Reflector extends Stringable {}"` |
|     - | 1700 | `"class ReflectionException extends Exception {}"` |
|     - | 1701 | `"class Reflection {"` |
|     - | 1702 | `" public static function getModifierNames($modifiers){"` |
|     - | 1703 | `"  $names = array();"` |
|     - | 1704 | `"  if($modifiers & 64){ $names[] = 'abstract'; }"` |
|     - | 1705 | `"  if($modifiers & 32){ $names[] = 'final'; }"` |
|     - | 1706 | `"  if($modifiers & 1){ $names[] = 'public'; }"` |
|     - | 1707 | `"  if($modifiers & 2){ $names[] = 'protected'; }"` |
|     - | 1708 | `"  if($modifiers & 4){ $names[] = 'private'; }"` |
|     - | 1709 | `"  if($modifiers & 16){ $names[] = 'static'; }"` |
|     - | 1710 | `"  if($modifiers & 128){ $names[] = 'readonly'; }"` |
|     - | 1711 | `"  return $names;"` |
|     - | 1712 | `" }"` |
|     - | 1713 | `"}"` |
|     - | 1714 | `"class ReflectionClass implements Reflector {"` |
|     - | 1715 | `" const IS_IMPLICIT_ABSTRACT = 16;"` |
|     - | 1716 | `" const IS_EXPLICIT_ABSTRACT = 64;"` |
|     - | 1717 | `" const IS_FINAL = 32;"` |
|     - | 1718 | `" const IS_READONLY = 65536;"` |
|     - | 1719 | `" const SKIP_INITIALIZATION_ON_SERIALIZE = 8;"` |
|     - | 1720 | `" const SKIP_DESTRUCTOR = 16;"` |
|     - | 1721 | `" public $name;"` |
|     - | 1722 | `" protected $__obj = null;"` |
|     - | 1723 | `" public function __construct($objectOrClass){"` |
|     - | 1724 | `"  if(!is_object($objectOrClass) && !is_string($objectOrClass)){"` |
|     - | 1725 | `"   if(is_int($objectOrClass) \|\| is_float($objectOrClass) \|\| is_bool($objectOrClass)){"` |
|     - | 1726 | `"    $objectOrClass = (string)$objectOrClass;"` |
|     - | 1727 | `"   }else{"` |
|     - | 1728 | `"    throw new TypeError('ReflectionClass::__construct(): Argument #1 ($objectOrClass) must be of type object\|string, '.get_debug_type($objectOrClass).' given');"` |
|     - | 1729 | `"   }"` |
|     - | 1730 | `"  }"` |
|     - | 1731 | `"  $info = __reflect_class_info($objectOrClass);"` |
|     - | 1732 | `"  if($info === null){"` |
|     - | 1733 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 1734 | `"  }"` |
|     - | 1735 | `"  $this->name = $info['name'];"` |
|     - | 1736 | `" }"` |
|     - | 1737 | `" protected function __rinfo(){ return __reflect_class_info($this->name); }"` |
|     - | 1738 | `" public function getName(){ return $this->name; }"` |
|     - | 1739 | `" public function getShortName(){"` |
|     - | 1740 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1741 | `"  if($p === false){ return $this->name; }"` |
|     - | 1742 | `"  return substr($this->name,$p+1);"` |
|     - | 1743 | `" }"` |
|     - | 1744 | `" public function getNamespaceName(){"` |
|     - | 1745 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1746 | `"  if($p === false){ return ''; }"` |
|     - | 1747 | `"  return substr($this->name,0,$p);"` |
|     - | 1748 | `" }"` |
|     - | 1749 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 1750 | `" public function isInternal(){ $i = $this->__rinfo(); return $i['internal']; }"` |
|     - | 1751 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 1752 | `" public function isInterface(){ $i = $this->__rinfo(); return $i['interface']; }"` |
|     - | 1753 | `" public function isTrait(){ $i = $this->__rinfo(); return $i['trait']; }"` |
|     - | 1754 | `" public function isAbstract(){ $i = $this->__rinfo(); return $i['abstract']; }"` |
|     - | 1755 | `" public function isFinal(){ $i = $this->__rinfo(); return $i['final']; }"` |
|     - | 1756 | `" public function isReadOnly(){ $i = $this->__rinfo(); return $i['readonly']; }"` |
|     - | 1757 | `" public function isEnum(){ $i = $this->__rinfo(); return $i['enum']; }"` |
|     - | 1758 | `" public function isAnonymous(){ return strpos($this->name,'class@anonymous') === 0; }"` |
|     - | 1759 | `" public function getModifiers(){"` |
|     - | 1760 | `"  $i = $this->__rinfo();"` |
|     - | 1761 | `"  $m = 0;"` |
|     - | 1762 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 1763 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 1764 | `"  if($i['readonly']){ $m \|= 65536; }"` |
|     - | 1765 | `"  return $m;"` |
|     - | 1766 | `" }"` |
|     - | 1767 | `" public function getParentClass(){"` |
|     - | 1768 | `"  $i = $this->__rinfo();"` |
|     - | 1769 | `"  if($i['parent'] === null){ return false; }"` |
|     - | 1770 | `"  return new ReflectionClass($i['parent']);"` |
|     - | 1771 | `" }"` |
|     - | 1772 | `" public function getInterfaceNames(){ $i = $this->__rinfo(); return $i['interfaces']; }"` |
|     - | 1773 | `" public function getInterfaces(){"` |
|     - | 1774 | `"  $i = $this->__rinfo();"` |
|     - | 1775 | `"  $out = array();"` |
|     - | 1776 | `"  foreach($i['interfaces'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1777 | `"  return $out;"` |
|     - | 1778 | `" }"` |
|     - | 1779 | `" public function getTraitNames(){ $i = $this->__rinfo(); return $i['traits']; }"` |
|     - | 1780 | `" public function getTraits(){"` |
|     - | 1781 | `"  $i = $this->__rinfo();"` |
|     - | 1782 | `"  $out = array();"` |
|     - | 1783 | `"  foreach($i['traits'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1784 | `"  return $out;"` |
|     - | 1785 | `" }"` |
|     - | 1786 | `" public function getTraitAliases(){ return array(); }"` |
|     - | 1787 | `" public function implementsInterface($interface){"` |
|     - | 1788 | `"  if($interface instanceof ReflectionClass){ $interface = $interface->name; }"` |
|     - | 1789 | `"  $target = __reflect_class_info($interface);"` |
|     - | 1790 | `"  if($target === null){"` |
|     - | 1791 | `"   throw new ReflectionException('Interface \"'.$interface.'\" does not exist');"` |
|     - | 1792 | `"  }"` |
|     - | 1793 | `"  if(!$target['interface']){"` |
|     - | 1794 | `"   throw new ReflectionException($target['name'].' is not an interface');"` |
|     - | 1795 | `"  }"` |
|     - | 1796 | `"  $name = $target['name'];"` |
|     - | 1797 | `"  if($this->name === $name){ return true; }"` |
|     - | 1798 | `"  $i = $this->__rinfo();"` |
|     - | 1799 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1800 | `"  return false;"` |
|     - | 1801 | `" }"` |
|     - | 1802 | `" public function isSubclassOf($class){"` |
|     - | 1803 | `"  if($class instanceof ReflectionClass){ $class = $class->name; }"` |
|     - | 1804 | `"  $target = __reflect_class_info($class);"` |
|     - | 1805 | `"  if($target === null){"` |
|     - | 1806 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 1807 | `"  }"` |
|     - | 1808 | `"  $name = $target['name'];"` |
|     - | 1809 | `"  if($name === $this->name){ return false; }"` |
|     - | 1810 | `"  $i = $this->__rinfo();"` |
|     - | 1811 | `"  $p = $i['parent'];"` |
|     - | 1812 | `"  while($p !== null){"` |
|     - | 1813 | `"   if($p === $name){ return true; }"` |
|     - | 1814 | `"   $pi = __reflect_class_info($p);"` |
|     - | 1815 | `"   $p = $pi['parent'];"` |
|     - | 1816 | `"  }"` |
|     - | 1817 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1818 | `"  return false;"` |
|     - | 1819 | `" }"` |
|     - | 1820 | `" public function isInstance($object){"` |
|     - | 1821 | `"  if(!is_object($object)){"` |
|     - | 1822 | `"   throw new TypeError('ReflectionClass::isInstance(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 1823 | `"  }"` |
|     - | 1824 | `"  return is_a($object,$this->name);"` |
|     - | 1825 | `" }"` |
|     - | 1826 | `" public function hasMethod($name){"` |
|     - | 1827 | `"  $i = $this->__rinfo();"` |
|     - | 1828 | `"  $l = strtolower($name);"` |
|     - | 1829 | `"  foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ return true; } }"` |
|     - | 1830 | `"  return false;"` |
|     - | 1831 | `" }"` |
|     - | 1832 | `" public function hasProperty($name){"` |
|     - | 1833 | `"  $i = $this->__rinfo();"` |
|     - | 1834 | `"  if(isset($i['props'][$name])){ return true; }"` |
|     - | 1835 | `"  if($this->__obj !== null){ return (__reflect_prop_state($this->__obj, $name) & 1) !== 0; }"` |
|     - | 1836 | `"  return false;"` |
|     - | 1837 | `" }"` |
|     - | 1838 | `" public function hasConstant($name){ $i = $this->__rinfo(); return isset($i['consts'][$name]); }"` |
|     - | 1839 | `" public function getConstant($name){"` |
|     - | 1840 | `"  $i = $this->__rinfo();"` |
|     - | 1841 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 1842 | `"  return __reflect_const_value($this->name,$name);"` |
|     - | 1843 | `" }"` |
|     - | 1844 | `" public function getConstants($filter = null){"` |
|     - | 1845 | `"  $i = $this->__rinfo();"` |
|     - | 1846 | `"  $out = array();"` |
|     - | 1847 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 1848 | `"   if($filter !== null){"` |
|     - | 1849 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 1850 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 1851 | `"   }"` |
|     - | 1852 | `"   $out[$k] = __reflect_const_value($this->name,$k);"` |
|     - | 1853 | `"  }"` |
|     - | 1854 | `"  return $out;"` |
|     - | 1855 | `" }"` |
|     - | 1856 | `" public function getStartLine(){"` |
|     - | 1857 | `"  $i = $this->__rinfo();"` |
|     - | 1858 | `"  if($i['internal']){ return false; }"` |
|     - | 1859 | `"  return $i['line'];"` |
|     - | 1860 | `" }"` |
|     - | 1861 | `" public function getEndLine(){"` |
|     - | 1862 | `"  $i = $this->__rinfo();"` |
|     - | 1863 | `"  if($i['internal']){ return false; }"` |
|     - | 1864 | `"  return $i['endline'];"` |
|     - | 1865 | `" }"` |
|     - | 1866 | `" public function getFileName(){ $i = $this->__rinfo(); return $i['file']; }"` |
|     - | 1867 | `" public function getDocComment(){ $i = $this->__rinfo(); return $i['doc']; }"` |
|     - | 1868 | `" public function isInstantiable(){"` |
|     - | 1869 | `"  $i = $this->__rinfo();"` |
|     - | 1870 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract'] \|\| $i['enum']){ return false; }"` |
|     - | 1871 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){ return false; }"` |
|     - | 1872 | `"  return true;"` |
|     - | 1873 | `" }"` |
|     - | 1874 | `" public function isCloneable(){"` |
|     - | 1875 | `"  $i = $this->__rinfo();"` |
|     - | 1876 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1877 | `"  if($i['clonevis'] !== 0 && $i['clonevis'] !== 1){ return false; }"` |
|     - | 1878 | `"  return true;"` |
|     - | 1879 | `" }"` |
|     - | 1880 | `" public function isIterable(){"` |
|     - | 1881 | `"  $i = $this->__rinfo();"` |
|     - | 1882 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1883 | `"  return $i['iterable'];"` |
|     - | 1884 | `" }"` |
|     - | 1885 | `" public function isIterateable(){ return $this->isIterable(); }"` |
|     - | 1886 | `" public function newInstance(...$args){ return $this->__rnew($args); }"` |
|     - | 1887 | `" public function newInstanceArgs(array $args = array()){ return $this->__rnew($args); }"` |
|     - | 1888 | `" protected function __rnew($args){"` |
|     - | 1889 | `"  $i = $this->__rinfo();"` |
|     - | 1890 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1891 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){"` |
|     - | 1892 | `"   throw new ReflectionException('Access to non-public constructor of class '.$this->name);"` |
|     - | 1893 | `"  }"` |
|     - | 1894 | `"  if($i['ctorvis'] === 0 && count($args) > 0){"` |
|     - | 1895 | `"   throw new ReflectionException('Class '.$this->name.' does not have a constructor, so you cannot pass any constructor arguments');"` |
|     - | 1896 | `"  }"` |
|     - | 1897 | `"  return __reflect_new_instance($this->name,$args);"` |
|     - | 1898 | `" }"` |
|     - | 1899 | `" protected function __rcheckInstantiable($i){"` |
|     - | 1900 | `"  if($i['interface']){ throw new Error('Cannot instantiate interface '.$this->name); }"` |
|     - | 1901 | `"  if($i['trait']){ throw new Error('Cannot instantiate trait '.$this->name); }"` |
|     - | 1902 | `"  if($i['abstract']){ throw new Error('Cannot instantiate abstract class '.$this->name); }"` |
|     - | 1903 | `" }"` |
|     - | 1904 | `" public function newInstanceWithoutConstructor(){"` |
|     - | 1905 | `"  $i = $this->__rinfo();"` |
|     - | 1906 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1907 | `"  return __reflect_new_no_ctor($this->name);"` |
|     - | 1908 | `" }"` |
|     - | 1909 | `" public function getStaticProperties(){"` |
|     - | 1910 | `"  $i = $this->__rinfo();"` |
|     - | 1911 | `"  $out = array();"` |
|     - | 1912 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1913 | `"   if($p['static']){ $out[$k] = __reflect_static_value($this->name,$k); }"` |
|     - | 1914 | `"  }"` |
|     - | 1915 | `"  return $out;"` |
|     - | 1916 | `" }"` |
|     - | 1917 | `" public function getStaticPropertyValue($name, ...$def){"` |
|     - | 1918 | `"  $i = $this->__rinfo();"` |
|     - | 1919 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 1920 | `"   if(count($def) > 0){ return $def[0]; }"` |
|     - | 1921 | `"   throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 1922 | `"  }"` |
|     - | 1923 | `"  return __reflect_static_value($this->name,$name);"` |
|     - | 1924 | `" }"` |
|     - | 1925 | `" public function setStaticPropertyValue($name,$value){"` |
|     - | 1926 | `"  $i = $this->__rinfo();"` |
|     - | 1927 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 1928 | `"   throw new ReflectionException('Class '.$this->name.' does not have a property named '.$name);"` |
|     - | 1929 | `"  }"` |
|     - | 1930 | `"  __reflect_static_set($this->name,$name,$value);"` |
|     - | 1931 | `" }"` |
|     - | 1932 | `" public function getDefaultProperties(){"` |
|     - | 1933 | `"  $i = $this->__rinfo();"` |
|     - | 1934 | `"  $out = array();"` |
|     - | 1935 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1936 | `"   if($p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 1937 | `"  }"` |
|     - | 1938 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1939 | `"   if(!$p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 1940 | `"  }"` |
|     - | 1941 | `"  return $out;"` |
|     - | 1942 | `" }"` |
|     - | 1943 | `" public function getProperty($name){"` |
|     - | 1944 | `"  $i = $this->__rinfo();"` |
|     - | 1945 | `"  if(isset($i['props'][$name])){"` |
|     - | 1946 | `"   return new ReflectionProperty($this->name, $name);"` |
|     - | 1947 | `"  }"` |
|     - | 1948 | `"  if($this->__obj !== null && (__reflect_prop_state($this->__obj, $name) & 1)){"` |
|     - | 1949 | `"   return new ReflectionProperty($this->__obj, $name);"` |
|     - | 1950 | `"  }"` |
|     - | 1951 | `"  throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 1952 | `" }"` |
|     - | 1953 | `" public function getProperties($filter = null){"` |
|     - | 1954 | `"  $i = $this->__rinfo();"` |
|     - | 1955 | `"  $out = array();"` |
|     - | 1956 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1957 | `"   if($filter !== null){"` |
|     - | 1958 | `"    $m = ($p['vis'] === 1 ? 1 : ($p['vis'] === 2 ? 2 : 4));"` |
|     - | 1959 | `"    if($p['static']){ $m \|= 16; }"` |
|     - | 1960 | `"    if($p['readonly']){ $m \|= 128; }"` |
|     - | 1961 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 1962 | `"   }"` |
|     - | 1963 | `"   $out[] = new ReflectionProperty($this->name, $k);"` |
|     - | 1964 | `"  }"` |
|     - | 1965 | `"  if($this->__obj !== null){"` |
|     - | 1966 | `"   foreach(__reflect_dyn_props($this->__obj) as $k){"` |
|     - | 1967 | `"    if(isset($i['props'][$k])){ continue; }"` |
|     - | 1968 | `"    if($filter !== null && ($filter & 1) === 0){ continue; }"` |
|     - | 1969 | `"    $out[] = new ReflectionProperty($this->__obj, $k);"` |
|     - | 1970 | `"   }"` |
|     - | 1971 | `"  }"` |
|     - | 1972 | `"  return $out;"` |
|     - | 1973 | `" }"` |
|     - | 1974 | `" public function getMethod($name){"` |
|     - | 1975 | `"  $i = $this->__rinfo();"` |
|     - | 1976 | `"  $found = null;"` |
|     - | 1977 | `"  if(isset($i['methods'][$name])){"` |
|     - | 1978 | `"   $found = $name;"` |
|     - | 1979 | `"  }else{"` |
|     - | 1980 | `"   $l = strtolower($name);"` |
|     - | 1981 | `"   foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ $found = $k; break; } }"` |
|     - | 1982 | `"  }"` |
|     - | 1983 | `"  if($found === null){"` |
|     - | 1984 | `"   throw new ReflectionException('Method '.$this->name.'::'.$name.'() does not exist');"` |
|     - | 1985 | `"  }"` |
|     - | 1986 | `"  return new ReflectionMethod($this->name, $found);"` |
|     - | 1987 | `" }"` |
|     - | 1988 | `" public function getMethods($filter = null){"` |
|     - | 1989 | `"  $i = $this->__rinfo();"` |
|     - | 1990 | `"  $out = array();"` |
|     - | 1991 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 1992 | `"   if($filter !== null){"` |
|     - | 1993 | `"    $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 1994 | `"    if($m['static']){ $mod \|= 16; }"` |
|     - | 1995 | `"    if($m['abstract']){ $mod \|= 64; }"` |
|     - | 1996 | `"    if($m['final']){ $mod \|= 32; }"` |
|     - | 1997 | `"    if(($mod & $filter) === 0){ continue; }"` |
|     - | 1998 | `"   }"` |
|     - | 1999 | `"   $out[] = new ReflectionMethod($this->name, $k);"` |
|     - | 2000 | `"  }"` |
|     - | 2001 | `"  return $out;"` |
|     - | 2002 | `" }"` |
|     - | 2003 | `" public function getConstructor(){"` |
|     - | 2004 | `"  $i = $this->__rinfo();"` |
|     - | 2005 | `"  if(isset($i['methods']['__construct'])){"` |
|     - | 2006 | `"   return new ReflectionMethod($this->name, '__construct');"` |
|     - | 2007 | `"  }"` |
|     - | 2008 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 2009 | `"   if(strtolower($k) === '__construct'){ return new ReflectionMethod($this->name, $k); }"` |
|     - | 2010 | `"  }"` |
|     - | 2011 | `"  if($i['ctorvis'] !== 0 && isset($i['methods'][$this->name])){"` |
|     - | 2012 | `"   return new ReflectionMethod($this->name, $this->name);"` |
|     - | 2013 | `"  }"` |
|     - | 2014 | `"  return null;"` |
|     - | 2015 | `" }"` |
|     - | 2016 | `" public function getReflectionConstant($name){"` |
|     - | 2017 | `"  $i = $this->__rinfo();"` |
|     - | 2018 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 2019 | `"  return new ReflectionClassConstant($this->name, $name);"` |
|     - | 2020 | `" }"` |
|     - | 2021 | `" public function getReflectionConstants($filter = null){"` |
|     - | 2022 | `"  $i = $this->__rinfo();"` |
|     - | 2023 | `"  $out = array();"` |
|     - | 2024 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 2025 | `"   if($filter !== null){"` |
|     - | 2026 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 2027 | `"    if($c['final']){ $m \|= 32; }"` |
|     - | 2028 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 2029 | `"   }"` |
|     - | 2030 | `"   $out[] = new ReflectionClassConstant($this->name, $k);"` |
|     - | 2031 | `"  }"` |
|     - | 2032 | `"  return $out;"` |
|     - | 2033 | `" }"` |
|     - | 2034 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2035 | `"  $i = $this->__rinfo();"` |
|     - | 2036 | `"  return __reflect_build_attrs($i['attrs'], array('class', $this->name, null, 0), 1, $name, $flags);"` |
|     - | 2037 | `" }"` |
|     - | 2038 | `" public function getExtensionName(){ $i = $this->__rinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2039 | `" public function getExtension(){ $i = $this->__rinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2040 | `" public function newLazyGhost($initializer, $options = 0){"` |
|     - | 2041 | `"  throw new Error('ReflectionClass::newLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2042 | `" }"` |
|     - | 2043 | `" public function newLazyProxy($factory, $options = 0){"` |
|     - | 2044 | `"  throw new Error('ReflectionClass::newLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2045 | `" }"` |
|     - | 2046 | `" public function resetAsLazyGhost($object, $initializer, $options = 0){"` |
|     - | 2047 | `"  throw new Error('ReflectionClass::resetAsLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2048 | `" }"` |
|     - | 2049 | `" public function resetAsLazyProxy($object, $factory, $options = 0){"` |
|     - | 2050 | `"  throw new Error('ReflectionClass::resetAsLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2051 | `" }"` |
|     - | 2052 | `" public function getLazyInitializer($object){ return null; }"` |
|     - | 2053 | `" public function initializeLazyObject($object){ return $object; }"` |
|     - | 2054 | `" public function markLazyObjectAsInitialized($object){ return $object; }"` |
|     - | 2055 | `" public function isUninitializedLazyObject($object){ return false; }"` |
|     - | 2056 | `" public function __toString(){ return __reflect_export_class($this); }"` |
|     - | 2057 | `"}"` |
|     - | 2058 | `"class ReflectionObject extends ReflectionClass {"` |
|     - | 2059 | `" public function __construct($object){"` |
|     - | 2060 | `"  if(!is_object($object)){"` |
|     - | 2061 | `"   throw new TypeError('ReflectionObject::__construct(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 2062 | `"  }"` |
|     - | 2063 | `"  parent::__construct($object);"` |
|     - | 2064 | `"  $this->__obj = $object;"` |
|     - | 2065 | `" }"` |
|     - | 2066 | `"}"` |
|     - | 2067 | `;` |
|     - | 2068 | `/*` |
|     - | 2069 | ` * Chunk 2: ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,` |
|     - | 2070 | ` * ReflectionParameter.` |
|     - | 2071 | ` */` |
|     - | 2072 | `static const char zReflectLib2[] =` |
|     - | 2073 | `"abstract class ReflectionFunctionAbstract implements Reflector {"` |
|     - | 2074 | `" public $name;"` |
|     - | 2075 | `" protected $__cl = null;"` |
|     - | 2076 | `" protected function __rfinfo(){"` |
|     - | 2077 | `"  if($this->__cl !== null){ return __reflect_sig_fixup(__reflect_func_info($this->__cl)); }"` |
|     - | 2078 | `"  return __reflect_sig_fixup(__reflect_func_info($this->name));"` |
|     - | 2079 | `" }"` |
|     - | 2080 | `" protected function __rftarget(){ return $this->__cl !== null ? $this->__cl : $this->name; }"` |
|     - | 2081 | `" protected function __rpspec(){ return $this->__rftarget(); }"` |
|     - | 2082 | `" public function getName(){ return $this->name; }"` |
|     - | 2083 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 2084 | `" public function getNamespaceName(){"` |
|     - | 2085 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2086 | `"  if($p === false){ return ''; }"` |
|     - | 2087 | `"  return substr($this->name,0,$p);"` |
|     - | 2088 | `" }"` |
|     - | 2089 | `" public function getShortName(){"` |
|     - | 2090 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2091 | `"  if($p === false){ return $this->name; }"` |
|     - | 2092 | `"  return substr($this->name,$p+1);"` |
|     - | 2093 | `" }"` |
|     - | 2094 | `" public function isClosure(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2095 | `" public function isGenerator(){ $i = $this->__rfinfo(); return $i['generator']; }"` |
|     - | 2096 | `" public function isVariadic(){ $i = $this->__rfinfo(); return $i['variadic']; }"` |
|     - | 2097 | `" public function returnsReference(){ $i = $this->__rfinfo(); return $i['byref']; }"` |
|     - | 2098 | `" public function isInternal(){ $i = $this->__rfinfo(); return $i['internal']; }"` |
|     - | 2099 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 2100 | `" public function isDeprecated(){ $i = $this->__rfinfo(); return __reflect_has_deprecated($i['attrs']); }"` |
|     - | 2101 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['fstatic']; }"` |
|     - | 2102 | `" public function getFileName(){ $i = $this->__rfinfo(); return $i['file']; }"` |
|     - | 2103 | `" public function getStartLine(){"` |
|     - | 2104 | `"  $i = $this->__rfinfo();"` |
|     - | 2105 | `"  if($i['internal']){ return false; }"` |
|     - | 2106 | `"  return $i['line'];"` |
|     - | 2107 | `" }"` |
|     - | 2108 | `" public function getEndLine(){"` |
|     - | 2109 | `"  $i = $this->__rfinfo();"` |
|     - | 2110 | `"  if($i['internal']){ return false; }"` |
|     - | 2111 | `"  return $i['endline'];"` |
|     - | 2112 | `" }"` |
|     - | 2113 | `" public function getDocComment(){ $i = $this->__rfinfo(); return $i['doc']; }"` |
|     - | 2114 | `" public function hasReturnType(){ $i = $this->__rfinfo(); return $i['rettext'] !== null; }"` |
|     - | 2115 | `" public function getReturnType(){ $i = $this->__rfinfo(); return __reflect_make_type($i['rettext']); }"` |
|     - | 2116 | `" public function hasTentativeReturnType(){ return false; }"` |
|     - | 2117 | `" public function getTentativeReturnType(){ return null; }"` |
|     - | 2118 | `" public function getNumberOfParameters(){"` |
|     - | 2119 | `"  $i = $this->__rfinfo();"` |
|     - | 2120 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2121 | `"  return count($i['params']);"` |
|     - | 2122 | `" }"` |
|     - | 2123 | `" public function getNumberOfRequiredParameters(){"` |
|     - | 2124 | `"  $i = $this->__rfinfo();"` |
|     - | 2125 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2126 | `"  $req = 0;"` |
|     - | 2127 | `"  $n = count($i['params']);"` |
|     - | 2128 | `"  for($k = $n - 1; $k >= 0; $k--){"` |
|     - | 2129 | `"   $p = $i['params'][$k];"` |
|     - | 2130 | `"   if(!$p['variadic'] && !$p['hasdef']){ $req = $k + 1; break; }"` |
|     - | 2131 | `"  }"` |
|     - | 2132 | `"  return $req;"` |
|     - | 2133 | `" }"` |
|     - | 2134 | `" public function getParameters(){"` |
|     - | 2135 | `"  $i = $this->__rfinfo();"` |
|     - | 2136 | `"  $out = array();"` |
|     - | 2137 | `"  $spec = $this->__rpspec();"` |
|     - | 2138 | `"  foreach($i['params'] as $p){"` |
|     - | 2139 | `"   $out[] = new ReflectionParameter($spec, $p['pos']);"` |
|     - | 2140 | `"  }"` |
|     - | 2141 | `"  return $out;"` |
|     - | 2142 | `" }"` |
|     - | 2143 | `" public function getStaticVariables(){ $i = $this->__rfinfo(); return $i['statics']; }"` |
|     - | 2144 | `" public function getClosureThis(){"` |
|     - | 2145 | `"  $i = $this->__rfinfo();"` |
|     - | 2146 | `"  return isset($i['this']) ? $i['this'] : null;"` |
|     - | 2147 | `" }"` |
|     - | 2148 | `" public function getClosureScopeClass(){"` |
|     - | 2149 | `"  $i = $this->__rfinfo();"` |
|     - | 2150 | `"  if(isset($i['scope'])){ return new ReflectionClass($i['scope']); }"` |
|     - | 2151 | `"  if(isset($i['this'])){ return new ReflectionClass(get_class($i['this'])); }"` |
|     - | 2152 | `"  return null;"` |
|     - | 2153 | `" }"` |
|     - | 2154 | `" public function getClosureCalledClass(){ return $this->getClosureScopeClass(); }"` |
|     - | 2155 | `" public function getClosureUsedVariables(){"` |
|     - | 2156 | `"  $i = $this->__rfinfo();"` |
|     - | 2157 | `"  return isset($i['used']) ? $i['used'] : array();"` |
|     - | 2158 | `" }"` |
|     - | 2159 | `" public function getExtensionName(){ $i = $this->__rfinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2160 | `" public function getExtension(){ $i = $this->__rfinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2161 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2162 | `"  $i = $this->__rfinfo();"` |
|     - | 2163 | `"  if($this instanceof ReflectionMethod){"` |
|     - | 2164 | `"   $spec = array('method', $this->class, $this->name, 0);"` |
|     - | 2165 | `"   $target = 4;"` |
|     - | 2166 | `"  }else{"` |
|     - | 2167 | `"   $spec = array('fn', $this->__rftarget(), null, 0);"` |
|     - | 2168 | `"   $target = 2;"` |
|     - | 2169 | `"  }"` |
|     - | 2170 | `"  return __reflect_build_attrs($i['attrs'], $spec, $target, $name, $flags);"` |
|     - | 2171 | `" }"` |
|     - | 2172 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2173 | `"}"` |
|     - | 2174 | `"class ReflectionFunction extends ReflectionFunctionAbstract {"` |
|     - | 2175 | `" const IS_DEPRECATED = 2048;"` |
|     - | 2176 | `" public function __construct($function){"` |
|     - | 2177 | `"  if($function instanceof Closure){"` |
|     - | 2178 | `"   $this->__cl = $function;"` |
|     - | 2179 | `"   $i = $this->__rfinfo();"` |
|     - | 2180 | `"   if($i['closure']){"` |
|     - | 2181 | `"    $f = $i['file'] === false ? '' : $i['file'];"` |
|     - | 2182 | `"    $this->name = '{closure:'.$f.':'.$i['line'].'}';"` |
|     - | 2183 | `"   }else{"` |
|     - | 2184 | `"    $this->name = $i['name'];"` |
|     - | 2185 | `"   }"` |
|     - | 2186 | `"   return;"` |
|     - | 2187 | `"  }"` |
|     - | 2188 | `"  if(!is_string($function)){"` |
|     - | 2189 | `"   throw new TypeError('ReflectionFunction::__construct(): Argument #1 ($function) must be of type Closure\|string, '.get_debug_type($function).' given');"` |
|     - | 2190 | `"  }"` |
|     - | 2191 | `"  $i = __reflect_func_info($function);"` |
|     - | 2192 | `"  if($i === null){"` |
|     - | 2193 | `"   throw new ReflectionException('Function '.$function.'() does not exist');"` |
|     - | 2194 | `"  }"` |
|     - | 2195 | `"  if($i['closure']){"` |
|     - | 2196 | `"   $this->name = '{closure:'.($i['file'] === false ? '' : $i['file']).':'.$i['line'].'}';"` |
|     - | 2197 | `"   $this->__cl = __reflect_closure($function, null, null);"` |
|     - | 2198 | `"  }else{"` |
|     - | 2199 | `"   $this->name = $i['name'];"` |
|     - | 2200 | `"  }"` |
|     - | 2201 | `" }"` |
|     - | 2202 | `" public function invoke(...$args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2203 | `" public function invokeArgs(array $args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2204 | `" public function getClosure(){"` |
|     - | 2205 | `"  if($this->__cl !== null){ return $this->__cl; }"` |
|     - | 2206 | `"  return __reflect_closure($this->name, null, null);"` |
|     - | 2207 | `" }"` |
|     - | 2208 | `" public function isAnonymous(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2209 | `" public function isDisabled(){ return false; }"` |
|     - | 2210 | `"}"` |
|     - | 2211 | `"class ReflectionMethod extends ReflectionFunctionAbstract {"` |
|     - | 2212 | `" const IS_PUBLIC = 1;"` |
|     - | 2213 | `" const IS_PROTECTED = 2;"` |
|     - | 2214 | `" const IS_PRIVATE = 4;"` |
|     - | 2215 | `" const IS_STATIC = 16;"` |
|     - | 2216 | `" const IS_FINAL = 32;"` |
|     - | 2217 | `" const IS_ABSTRACT = 64;"` |
|     - | 2218 | `" public $class;"` |
|     - | 2219 | `" public function __construct($objectOrMethod, $method = null){"` |
|     - | 2220 | `"  if($method === null){"` |
|     - | 2221 | `"   if(!is_string($objectOrMethod) \|\| strpos($objectOrMethod,'::') === false){"` |
|     - | 2222 | `"    throw new TypeError('ReflectionMethod::__construct(): Argument #1 ($objectOrMethod) must be of type object\|string, '.get_debug_type($objectOrMethod).' given');"` |
|     - | 2223 | `"   }"` |
|     - | 2224 | `"   $p = strpos($objectOrMethod,'::');"` |
|     - | 2225 | `"   $method = substr($objectOrMethod,$p+2);"` |
|     - | 2226 | `"   $objectOrMethod = substr($objectOrMethod,0,$p);"` |
|     - | 2227 | `"  }"` |
|     - | 2228 | `"  $ci = __reflect_class_info($objectOrMethod);"` |
|     - | 2229 | `"  if($ci === null){"` |
|     - | 2230 | `"   throw new ReflectionException('Class \"'.$objectOrMethod.'\" does not exist');"` |
|     - | 2231 | `"  }"` |
|     - | 2232 | `"  $this->class = $ci['name'];"` |
|     - | 2233 | `"  $found = null;"` |
|     - | 2234 | `"  if(isset($ci['methods'][$method])){"` |
|     - | 2235 | `"   $found = $method;"` |
|     - | 2236 | `"  }else{"` |
|     - | 2237 | `"   $l = strtolower($method);"` |
|     - | 2238 | `"   foreach($ci['methods'] as $k => $m){"` |
|     - | 2239 | `"    if(strtolower($k) === $l){ $found = $k; break; }"` |
|     - | 2240 | `"   }"` |
|     - | 2241 | `"  }"` |
|     - | 2242 | `"  if($found === null){"` |
|     - | 2243 | `"   throw new ReflectionException('Method '.$this->class.'::'.$method.'() does not exist');"` |
|     - | 2244 | `"  }"` |
|     - | 2245 | `"  $this->name = $found;"` |
|     - | 2246 | `" }"` |
|     - | 2247 | `" public static function createFromMethodName($name){"` |
|     - | 2248 | `"  return new ReflectionMethod($name);"` |
|     - | 2249 | `" }"` |
|     - | 2250 | `" protected function __rfinfo(){ return __reflect_func_info($this->class, $this->name); }"` |
|     - | 2251 | `" protected function __rpspec(){ return array($this->class, $this->name); }"` |
|     - | 2252 | `" public function getDeclaringClass(){"` |
|     - | 2253 | `"  $i = $this->__rfinfo();"` |
|     - | 2254 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2255 | `" }"` |
|     - | 2256 | `" public function getModifiers(){"` |
|     - | 2257 | `"  $i = $this->__rfinfo();"` |
|     - | 2258 | `"  $m = ($i['vis'] === 1 ? 1 : ($i['vis'] === 2 ? 2 : 4));"` |
|     - | 2259 | `"  if($i['mstatic']){ $m \|= 16; }"` |
|     - | 2260 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 2261 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 2262 | `"  return $m;"` |
|     - | 2263 | `" }"` |
|     - | 2264 | `" public function isPublic(){ $i = $this->__rfinfo(); return $i['vis'] === 1; }"` |
|     - | 2265 | `" public function isProtected(){ $i = $this->__rfinfo(); return $i['vis'] === 2; }"` |
|     - | 2266 | `" public function isPrivate(){ $i = $this->__rfinfo(); return $i['vis'] === 3; }"` |
|     - | 2267 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['mstatic']; }"` |
|     - | 2268 | `" public function isAbstract(){ $i = $this->__rfinfo(); return $i['abstract']; }"` |
|     - | 2269 | `" public function isFinal(){ $i = $this->__rfinfo(); return $i['final']; }"` |
|     - | 2270 | `" public function isConstructor(){ return strtolower($this->name) === '__construct'; }"` |
|     - | 2271 | `" public function isDestructor(){ return strtolower($this->name) === '__destruct'; }"` |
|     - | 2272 | `" public function invoke($object = null, ...$args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2273 | `" public function invokeArgs($object, array $args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2274 | `" protected function __rinvoke($object, $args){"` |
|     - | 2275 | `"  $i = $this->__rfinfo();"` |
|     - | 2276 | `"  if(!$i['mstatic']){"` |
|     - | 2277 | `"   if(!is_object($object)){"` |
|     - | 2278 | `"    throw new ReflectionException('Trying to invoke non static method '.$this->class.'::'.$this->name.'() without an object');"` |
|     - | 2279 | `"   }"` |
|     - | 2280 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2281 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2282 | `"   }"` |
|     - | 2283 | `"  }else{"` |
|     - | 2284 | `"   $object = null;"` |
|     - | 2285 | `"  }"` |
|     - | 2286 | `"  return __reflect_invoke($this->class, $this->name, $object, $args);"` |
|     - | 2287 | `" }"` |
|     - | 2288 | `" public function getClosure($object = null){"` |
|     - | 2289 | `"  $i = $this->__rfinfo();"` |
|     - | 2290 | `"  if(!$i['mstatic']){"` |
|     - | 2291 | `"   if($object === null){"` |
|     - | 2292 | `"    throw new ValueError('ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods');"` |
|     - | 2293 | `"   }"` |
|     - | 2294 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2295 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2296 | `"   }"` |
|     - | 2297 | `"  }else{"` |
|     - | 2298 | `"   $object = null;"` |
|     - | 2299 | `"  }"` |
|     - | 2300 | `"  return __reflect_closure($this->class, $this->name, $object);"` |
|     - | 2301 | `" }"` |
|     - | 2302 | `" public function setAccessible($accessible){ }"` |
|     - | 2303 | `" public function hasPrototype(){ return $this->__rproto() !== null; }"` |
|     - | 2304 | `" public function getPrototype(){"` |
|     - | 2305 | `"  $p = $this->__rproto();"` |
|     - | 2306 | `"  if($p === null){"` |
|     - | 2307 | `"   throw new ReflectionException('Method '.$this->class.'::'.$this->name.' does not have a prototype');"` |
|     - | 2308 | `"  }"` |
|     - | 2309 | `"  return new ReflectionMethod($p, $this->name);"` |
|     - | 2310 | `" }"` |
|     - | 2311 | `" protected function __rproto(){"` |
|     - | 2312 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2313 | `"  $l = strtolower($this->name);"` |
|     - | 2314 | `"  $p = $ci['parent'];"` |
|     - | 2315 | `"  while($p !== null){"` |
|     - | 2316 | `"   $pi = __reflect_class_info($p);"` |
|     - | 2317 | `"   foreach($pi['methods'] as $k => $m){"` |
|     - | 2318 | `"    if(strtolower($k) === $l && $m['vis'] !== 3){ return $m['decl']; }"` |
|     - | 2319 | `"   }"` |
|     - | 2320 | `"   $p = $pi['parent'];"` |
|     - | 2321 | `"  }"` |
|     - | 2322 | `"  foreach($ci['interfaces'] as $if){"` |
|     - | 2323 | `"   $ii = __reflect_class_info($if);"` |
|     - | 2324 | `"   foreach($ii['methods'] as $k => $m){"` |
|     - | 2325 | `"    if(strtolower($k) === $l){ return $ii['name']; }"` |
|     - | 2326 | `"   }"` |
|     - | 2327 | `"  }"` |
|     - | 2328 | `"  return null;"` |
|     - | 2329 | `" }"` |
|     - | 2330 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2331 | `"}"` |
|     - | 2332 | `"class ReflectionParameter implements Reflector {"` |
|     - | 2333 | `" public $name;"` |
|     - | 2334 | `" protected $__t;"` |
|     - | 2335 | `" protected $__m = null;"` |
|     - | 2336 | `" protected $__p = 0;"` |
|     - | 2337 | `" public function __construct($function, $param){"` |
|     - | 2338 | `"  $m = null;"` |
|     - | 2339 | `"  $t = $function;"` |
|     - | 2340 | `"  if(is_array($function)){"` |
|     - | 2341 | `"   $t = $function[0];"` |
|     - | 2342 | `"   $m = $function[1];"` |
|     - | 2343 | `"   if(is_object($t)){ $t = get_class($t); }"` |
|     - | 2344 | `"  }else if(is_string($function) && strpos($function,'::') !== false){"` |
|     - | 2345 | `"   $p = strpos($function,'::');"` |
|     - | 2346 | `"   $m = substr($function,$p+2);"` |
|     - | 2347 | `"   $t = substr($function,0,$p);"` |
|     - | 2348 | `"  }"` |
|     - | 2349 | `"  if($m !== null){"` |
|     - | 2350 | `"   $rm = new ReflectionMethod($t, $m);"` |
|     - | 2351 | `"   $t = $rm->class;"` |
|     - | 2352 | `"   $m = $rm->name;"` |
|     - | 2353 | `"   $i = __reflect_func_info($t, $m);"` |
|     - | 2354 | `"  }else if($function instanceof Closure){"` |
|     - | 2355 | `"   $t = $function;"` |
|     - | 2356 | `"   $i = __reflect_func_info($function);"` |
|     - | 2357 | `"  }else{"` |
|     - | 2358 | `"   $i = __reflect_sig_fixup(__reflect_func_info($t));"` |
|     - | 2359 | `"   if($i === null){"` |
|     - | 2360 | `"    throw new ReflectionException('Function '.$t.'() does not exist');"` |
|     - | 2361 | `"   }"` |
|     - | 2362 | `"  }"` |
|     - | 2363 | `"  $found = null;"` |
|     - | 2364 | `"  if(is_int($param)){"` |
|     - | 2365 | `"   if(isset($i['params'][$param])){ $found = $i['params'][$param]; }"` |
|     - | 2366 | `"   if($found === null){"` |
|     - | 2367 | `"    throw new ReflectionException('The parameter specified by its offset could not be found');"` |
|     - | 2368 | `"   }"` |
|     - | 2369 | `"  }else{"` |
|     - | 2370 | `"   foreach($i['params'] as $pp){"` |
|     - | 2371 | `"    if($pp['name'] === $param){ $found = $pp; break; }"` |
|     - | 2372 | `"   }"` |
|     - | 2373 | `"   if($found === null){"` |
|     - | 2374 | `"    throw new ReflectionException('The parameter specified by its name could not be found');"` |
|     - | 2375 | `"   }"` |
|     - | 2376 | `"  }"` |
|     - | 2377 | `"  $this->name = $found['name'];"` |
|     - | 2378 | `"  $this->__t = $t;"` |
|     - | 2379 | `"  $this->__m = $m;"` |
|     - | 2380 | `"  $this->__p = $found['pos'];"` |
|     - | 2381 | `" }"` |
|     - | 2382 | `" protected function __rffull(){"` |
|     - | 2383 | `"  if($this->__m !== null){ return __reflect_func_info($this->__t, $this->__m); }"` |
|     - | 2384 | `"  return __reflect_sig_fixup(__reflect_func_info($this->__t));"` |
|     - | 2385 | `" }"` |
|     - | 2386 | `" protected function __rpinfo(){"` |
|     - | 2387 | `"  $i = $this->__rffull();"` |
|     - | 2388 | `"  return $i['params'][$this->__p];"` |
|     - | 2389 | `" }"` |
|     - | 2390 | `" public function getName(){ return $this->name; }"` |
|     - | 2391 | `" public function getPosition(){ return $this->__p; }"` |
|     - | 2392 | `" public function isPassedByReference(){ $p = $this->__rpinfo(); return $p['byref']; }"` |
|     - | 2393 | `" public function canBePassedByValue(){ return !$this->isPassedByReference(); }"` |
|     - | 2394 | `" public function isVariadic(){ $p = $this->__rpinfo(); return $p['variadic']; }"` |
|     - | 2395 | `" public function isPromoted(){ $p = $this->__rpinfo(); return $p['promoted']; }"` |
|     - | 2396 | `" public function isDefaultValueAvailable(){ $p = $this->__rpinfo(); return $p['hasdef']; }"` |
|     - | 2397 | `" public function isOptional(){"` |
|     - | 2398 | `"  $i = $this->__rffull();"` |
|     - | 2399 | `"  $n = count($i['params']);"` |
|     - | 2400 | `"  for($k = $this->__p; $k < $n; $k++){"` |
|     - | 2401 | `"   $p = $i['params'][$k];"` |
|     - | 2402 | `"   if(!$p['variadic'] && !$p['hasdef']){ return false; }"` |
|     - | 2403 | `"  }"` |
|     - | 2404 | `"  return true;"` |
|     - | 2405 | `" }"` |
|     - | 2406 | `" public function getDefaultValue(){"` |
|     - | 2407 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2408 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2409 | `"  }"` |
|     - | 2410 | `"  $p = $this->__rpinfo();"` |
|     - | 2411 | `"  if(isset($p['deftext'])){"` |
|     - | 2412 | `"   $s = __reflect_sig_scalar($p['deftext']);"` |
|     - | 2413 | `"   if($s[0]){ return $s[1]; }"` |
|     - | 2414 | `"   if($p['deftext'] === 'array (' \|\| strpos($p['deftext'], '[') === 0){ return array(); }"` |
|     - | 2415 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2416 | `"  }"` |
|     - | 2417 | `"  return __reflect_param_default($this->__t, $this->__m, $this->__p);"` |
|     - | 2418 | `" }"` |
|     - | 2419 | `" public function isDefaultValueConstant(){"` |
|     - | 2420 | `"  if(!$this->isDefaultValueAvailable()){ return false; }"` |
|     - | 2421 | `"  $p = $this->__rpinfo();"` |
|     - | 2422 | `"  if(isset($p['deftext'])){ return false; }"` |
|     - | 2423 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p) !== null;"` |
|     - | 2424 | `" }"` |
|     - | 2425 | `" public function getDefaultValueConstantName(){"` |
|     - | 2426 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2427 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2428 | `"  }"` |
|     - | 2429 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p);"` |
|     - | 2430 | `" }"` |
|     - | 2431 | `" public function allowsNull(){"` |
|     - | 2432 | `"  $p = $this->__rpinfo();"` |
|     - | 2433 | `"  if($p['typetext'] === null){ return true; }"` |
|     - | 2434 | `"  if($p['nullable']){ return true; }"` |
|     - | 2435 | `"  return $p['typetext'] === 'mixed' \|\| $p['typetext'] === 'null';"` |
|     - | 2436 | `" }"` |
|     - | 2437 | `" public function hasType(){ $p = $this->__rpinfo(); return $p['typetext'] !== null; }"` |
|     - | 2438 | `" public function getType(){ $p = $this->__rpinfo(); return __reflect_make_type($p['typetext']); }"` |
|     - | 2439 | `" public function getDeclaringFunction(){"` |
|     - | 2440 | `"  if($this->__m !== null){ return new ReflectionMethod($this->__t, $this->__m); }"` |
|     - | 2441 | `"  return new ReflectionFunction($this->__t);"` |
|     - | 2442 | `" }"` |
|     - | 2443 | `" public function getDeclaringClass(){"` |
|     - | 2444 | `"  if($this->__m === null){ return null; }"` |
|     - | 2445 | `"  $i = $this->__rffull();"` |
|     - | 2446 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2447 | `" }"` |
|     - | 2448 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2449 | `"  $p = $this->__rpinfo();"` |
|     - | 2450 | `"  return __reflect_build_attrs($p['attrs'], array('param', $this->__t, $this->__m, $this->__p), 32, $name, $flags);"` |
|     - | 2451 | `" }"` |
|     - | 2452 | `" public function __toString(){ return __reflect_export_param($this); }"` |
|     - | 2453 | `"}"` |
|     - | 2454 | `;` |
|     - | 2455 | `/*` |
|     - | 2456 | ` * Chunk 3: ReflectionProperty, ReflectionClassConstant.` |
|     - | 2457 | ` */` |
|     - | 2458 | `static const char zReflectLib3[] =` |
|     - | 2459 | `"class ReflectionProperty implements Reflector {"` |
|     - | 2460 | `" const IS_PUBLIC = 1;"` |
|     - | 2461 | `" const IS_PROTECTED = 2;"` |
|     - | 2462 | `" const IS_PRIVATE = 4;"` |
|     - | 2463 | `" const IS_STATIC = 16;"` |
|     - | 2464 | `" const IS_FINAL = 32;"` |
|     - | 2465 | `" const IS_ABSTRACT = 64;"` |
|     - | 2466 | `" const IS_READONLY = 128;"` |
|     - | 2467 | `" const IS_VIRTUAL = 512;"` |
|     - | 2468 | `" const IS_PROTECTED_SET = 2048;"` |
|     - | 2469 | `" const IS_PRIVATE_SET = 4096;"` |
|     - | 2470 | `" public $name;"` |
|     - | 2471 | `" public $class;"` |
|     - | 2472 | `" protected $__dynobj = null;"` |
|     - | 2473 | `" public function __construct($class, $property){"` |
|     - | 2474 | `"  $obj = null;"` |
|     - | 2475 | `"  if(is_object($class)){ $obj = $class; }"` |
|     - | 2476 | `"  else if(!is_string($class)){"` |
|     - | 2477 | `"   throw new TypeError('ReflectionProperty::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2478 | `"  }"` |
|     - | 2479 | `"  $ci = __reflect_class_info($class);"` |
|     - | 2480 | `"  if($ci === null){"` |
|     - | 2481 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2482 | `"  }"` |
|     - | 2483 | `"  $this->class = $ci['name'];"` |
|     - | 2484 | `"  if(isset($ci['props'][$property])){"` |
|     - | 2485 | `"   $this->name = $property;"` |
|     - | 2486 | `"   return;"` |
|     - | 2487 | `"  }"` |
|     - | 2488 | `"  if($obj !== null && (__reflect_prop_state($obj, $property) & 1)){"` |
|     - | 2489 | `"   $this->name = $property;"` |
|     - | 2490 | `"   $this->__dynobj = $obj;"` |
|     - | 2491 | `"   return;"` |
|     - | 2492 | `"  }"` |
|     - | 2493 | `"  throw new ReflectionException('Property '.$this->class.'::$'.$property.' does not exist');"` |
|     - | 2494 | `" }"` |
|     - | 2495 | `" protected function __rpmeta(){"` |
|     - | 2496 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2497 | `"  if(isset($ci['props'][$this->name])){ return $ci['props'][$this->name]; }"` |
|     - | 2498 | `"  return array('vis' => 1, 'static' => false, 'readonly' => false, 'hasdef' => false,"` |
|     - | 2499 | `"   'typed' => false, 'typetext' => null, 'decl' => $this->class, 'line' => 0, 'dyn' => true);"` |
|     - | 2500 | `" }"` |
|     - | 2501 | `" public function getName(){ return $this->name; }"` |
|     - | 2502 | `" public function getDeclaringClass(){"` |
|     - | 2503 | `"  $m = $this->__rpmeta();"` |
|     - | 2504 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2505 | `" }"` |
|     - | 2506 | `" public function getModifiers(){"` |
|     - | 2507 | `"  $m = $this->__rpmeta();"` |
|     - | 2508 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2509 | `"  if($m['static']){ $mod \|= 16; }"` |
|     - | 2510 | `"  if($m['readonly']){ $mod \|= 128; }"` |
|     - | 2511 | `"  return $mod;"` |
|     - | 2512 | `" }"` |
|     - | 2513 | `" public function isPublic(){ $m = $this->__rpmeta(); return $m['vis'] === 1; }"` |
|     - | 2514 | `" public function isProtected(){ $m = $this->__rpmeta(); return $m['vis'] === 2; }"` |
|     - | 2515 | `" public function isPrivate(){ $m = $this->__rpmeta(); return $m['vis'] === 3; }"` |
|     - | 2516 | `" public function isStatic(){ $m = $this->__rpmeta(); return $m['static']; }"` |
|     - | 2517 | `" public function isReadOnly(){ $m = $this->__rpmeta(); return $m['readonly']; }"` |
|     - | 2518 | `" public function isDefault(){ $m = $this->__rpmeta(); return !isset($m['dyn']); }"` |
|     - | 2519 | `" public function isDynamic(){ $m = $this->__rpmeta(); return isset($m['dyn']); }"` |
|     - | 2520 | `" public function isAbstract(){ return false; }"` |
|     - | 2521 | `" public function isFinal(){ return false; }"` |
|     - | 2522 | `" public function isVirtual(){ return false; }"` |
|     - | 2523 | `" public function isPrivateSet(){ return false; }"` |
|     - | 2524 | `" public function isProtectedSet(){ return false; }"` |
|     - | 2525 | `" public function hasHooks(){ return false; }"` |
|     - | 2526 | `" public function getHooks(){ return array(); }"` |
|     - | 2527 | `" public function hasHook($type){ return false; }"` |
|     - | 2528 | `" public function getHook($type){ return null; }"` |
|     - | 2529 | `" public function isLazy($object){ return false; }"` |
|     - | 2530 | `" public function setAccessible($accessible){ }"` |
|     - | 2531 | `" public function getValue($object = null){"` |
|     - | 2532 | `"  $m = $this->__rpmeta();"` |
|     - | 2533 | `"  if($m['static']){ return __reflect_static_value($this->class, $this->name); }"` |
|     - | 2534 | `"  if(!is_object($object)){"` |
|     - | 2535 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2536 | `"  }"` |
|     - | 2537 | `"  return __reflect_prop_read($object, $this->name);"` |
|     - | 2538 | `" }"` |
|     - | 2539 | `" public function setValue($objectOrValue = null, $value = null){"` |
|     - | 2540 | `"  $m = $this->__rpmeta();"` |
|     - | 2541 | `"  if($m['static']){"` |
|     - | 2542 | `"   if($value === null && $objectOrValue !== null && !is_object($objectOrValue)){"` |
|     - | 2543 | `"    __reflect_static_set($this->class, $this->name, $objectOrValue);"` |
|     - | 2544 | `"   }else{"` |
|     - | 2545 | `"    __reflect_static_set($this->class, $this->name, $value);"` |
|     - | 2546 | `"   }"` |
|     - | 2547 | `"   return;"` |
|     - | 2548 | `"  }"` |
|     - | 2549 | `"  __reflect_prop_write($objectOrValue, $this->name, $value);"` |
|     - | 2550 | `" }"` |
|     - | 2551 | `" public function getRawValue($object){ return $this->getValue($object); }"` |
|     - | 2552 | `" public function setRawValue($object, $value){ $this->setValue($object, $value); }"` |
|     - | 2553 | `" public function isInitialized($object = null){"` |
|     - | 2554 | `"  $m = $this->__rpmeta();"` |
|     - | 2555 | `"  if($m['static']){ return (__reflect_prop_state($this->class, $this->name) & 2) !== 0; }"` |
|     - | 2556 | `"  if(!is_object($object)){"` |
|     - | 2557 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2558 | `"  }"` |
|     - | 2559 | `"  return (__reflect_prop_state($object, $this->name) & 2) !== 0;"` |
|     - | 2560 | `" }"` |
|     - | 2561 | `" public function hasDefaultValue(){"` |
|     - | 2562 | `"  $m = $this->__rpmeta();"` |
|     - | 2563 | `"  if(isset($m['dyn'])){ return false; }"` |
|     - | 2564 | `"  if($m['hasdef']){ return true; }"` |
|     - | 2565 | `"  return !$m['typed'];"` |
|     - | 2566 | `" }"` |
|     - | 2567 | `" public function getDefaultValue(){"` |
|     - | 2568 | `"  $m = $this->__rpmeta();"` |
|     - | 2569 | `"  if(isset($m['dyn']) \|\| !$m['hasdef']){ return null; }"` |
|     - | 2570 | `"  return __reflect_prop_default($this->class, $this->name);"` |
|     - | 2571 | `" }"` |
|     - | 2572 | `" public function hasType(){ $m = $this->__rpmeta(); return $m['typed']; }"` |
|     - | 2573 | `" public function getType(){ $m = $this->__rpmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2574 | `" public function getSettableType(){ return $this->getType(); }"` |
|     - | 2575 | `" public function setRawValueWithoutLazyInitialization($object, $value){"` |
|     - | 2576 | `"  throw new Error('ReflectionProperty::setRawValueWithoutLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2577 | `" }"` |
|     - | 2578 | `" public function skipLazyInitialization($object){"` |
|     - | 2579 | `"  throw new Error('ReflectionProperty::skipLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2580 | `" }"` |
|     - | 2581 | `" public function getDocComment(){ $m = $this->__rpmeta(); return isset($m['doc']) ? $m['doc'] : false; }"` |
|     - | 2582 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2583 | `"  $m = $this->__rpmeta();"` |
|     - | 2584 | `"  if(!isset($m['attrs'])){ return array(); }"` |
|     - | 2585 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 8, $name, $flags);"` |
|     - | 2586 | `" }"` |
|     - | 2587 | `" public function __toString(){ return __reflect_export_prop($this); }"` |
|     - | 2588 | `"}"` |
|     - | 2589 | `"class ReflectionClassConstant implements Reflector {"` |
|     - | 2590 | `" const IS_PUBLIC = 1;"` |
|     - | 2591 | `" const IS_PROTECTED = 2;"` |
|     - | 2592 | `" const IS_PRIVATE = 4;"` |
|     - | 2593 | `" const IS_FINAL = 32;"` |
|     - | 2594 | `" public $name;"` |
|     - | 2595 | `" public $class;"` |
|     - | 2596 | `" public function __construct($class, $constant){"` |
|     - | 2597 | `"  if(!is_object($class) && !is_string($class)){"` |
|     - | 2598 | `"   throw new TypeError('ReflectionClassConstant::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2599 | `"  }"` |
|     - | 2600 | `"  $ci = __reflect_class_info($class);"` |
|     - | 2601 | `"  if($ci === null){"` |
|     - | 2602 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2603 | `"  }"` |
|     - | 2604 | `"  $this->class = $ci['name'];"` |
|     - | 2605 | `"  if(!isset($ci['consts'][$constant])){"` |
|     - | 2606 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' does not exist');"` |
|     - | 2607 | `"  }"` |
|     - | 2608 | `"  $this->name = $constant;"` |
|     - | 2609 | `" }"` |
|     - | 2610 | `" protected function __rcmeta(){"` |
|     - | 2611 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2612 | `"  return $ci['consts'][$this->name];"` |
|     - | 2613 | `" }"` |
|     - | 2614 | `" public function getName(){ return $this->name; }"` |
|     - | 2615 | `" public function getValue(){ return __reflect_const_value($this->class, $this->name); }"` |
|     - | 2616 | `" public function getDeclaringClass(){"` |
|     - | 2617 | `"  $m = $this->__rcmeta();"` |
|     - | 2618 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2619 | `" }"` |
|     - | 2620 | `" public function getModifiers(){"` |
|     - | 2621 | `"  $m = $this->__rcmeta();"` |
|     - | 2622 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2623 | `"  if($m['final']){ $mod \|= 32; }"` |
|     - | 2624 | `"  return $mod;"` |
|     - | 2625 | `" }"` |
|     - | 2626 | `" public function isPublic(){ $m = $this->__rcmeta(); return $m['vis'] === 1; }"` |
|     - | 2627 | `" public function isProtected(){ $m = $this->__rcmeta(); return $m['vis'] === 2; }"` |
|     - | 2628 | `" public function isPrivate(){ $m = $this->__rcmeta(); return $m['vis'] === 3; }"` |
|     - | 2629 | `" public function isFinal(){ $m = $this->__rcmeta(); return $m['final']; }"` |
|     - | 2630 | `" public function isEnumCase(){ $m = $this->__rcmeta(); return $m['enumcase']; }"` |
|     - | 2631 | `" public function isDeprecated(){ $m = $this->__rcmeta(); return __reflect_has_deprecated($m['attrs']); }"` |
|     - | 2632 | `" public function hasType(){ $m = $this->__rcmeta(); return $m['typed']; }"` |
|     - | 2633 | `" public function getType(){ $m = $this->__rcmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2634 | `" public function getDocComment(){ $m = $this->__rcmeta(); return $m['doc']; }"` |
|     - | 2635 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2636 | `"  $m = $this->__rcmeta();"` |
|     - | 2637 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 16, $name, $flags);"` |
|     - | 2638 | `" }"` |
|     - | 2639 | `" public function __toString(){ return __reflect_export_cconst($this); }"` |
|     - | 2640 | `"}"` |
|     - | 2641 | `;` |
|     - | 2642 | `/*` |
|     - | 2643 | ` * Chunk 4: the ReflectionType family, built from the engine's canonical` |
|     - | 2644 | ` * type text ("?int", "string\|float", "(A&B)\|C" — normalized at compile` |
|     - | 2645 | ` * time). __reflect_make_type is the internal factory; PHP itself never` |
|     - | 2646 | ` * lets user code construct these, so the public constructors here are a` |
|     - | 2647 | ` * recorded PHL-only surface.` |
|     - | 2648 | ` */` |
|     - | 2649 | `static const char zReflectLib4[] =` |
|     - | 2650 | `"abstract class ReflectionType implements Stringable {"` |
|     - | 2651 | `" protected $__text = '';"` |
|     - | 2652 | `" protected $__nullable = false;"` |
|     - | 2653 | `" public function allowsNull(){ return $this->__nullable; }"` |
|     - | 2654 | `" public function __toString(){ return $this->__text; }"` |
|     - | 2655 | `"}"` |
|     - | 2656 | `"class ReflectionNamedType extends ReflectionType {"` |
|     - | 2657 | `" protected $__tname = '';"` |
|     - | 2658 | `" public function __construct($name = '', $nullable = false, $text = null){"` |
|     - | 2659 | `"  $this->__tname = $name;"` |
|     - | 2660 | `"  $l = strtolower($name);"` |
|     - | 2661 | `"  $this->__nullable = $nullable \|\| $l === 'null' \|\| $l === 'mixed';"` |
|     - | 2662 | `"  $this->__text = $text === null ? $name : $text;"` |
|     - | 2663 | `" }"` |
|     - | 2664 | `" public function getName(){ return $this->__tname; }"` |
|     - | 2665 | `" public function isBuiltin(){"` |
|     - | 2666 | `"  $l = strtolower($this->__tname);"` |
|     - | 2667 | `"  return in_array($l, array('int','float','string','bool','array','object','mixed',"` |
|     - | 2668 | `"   'void','never','null','callable','iterable','true','false'), true);"` |
|     - | 2669 | `" }"` |
|     - | 2670 | `"}"` |
|     - | 2671 | `"class ReflectionUnionType extends ReflectionType {"` |
|     - | 2672 | `" protected $__types = array();"` |
|     - | 2673 | `" public function __construct($text = '', $nullable = false, $types = array()){"` |
|     - | 2674 | `"  $this->__text = $text;"` |
|     - | 2675 | `"  $this->__nullable = $nullable;"` |
|     - | 2676 | `"  $this->__types = $types;"` |
|     - | 2677 | `" }"` |
|     - | 2678 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2679 | `"}"` |
|     - | 2680 | `"class ReflectionIntersectionType extends ReflectionType {"` |
|     - | 2681 | `" protected $__types = array();"` |
|     - | 2682 | `" public function __construct($text = '', $types = array()){"` |
|     - | 2683 | `"  $this->__text = $text;"` |
|     - | 2684 | `"  $this->__nullable = false;"` |
|     - | 2685 | `"  $this->__types = $types;"` |
|     - | 2686 | `" }"` |
|     - | 2687 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2688 | `"}"` |
|     - | 2689 | `"function __reflect_make_atom($p){"` |
|     - | 2690 | `" $nullable = false;"` |
|     - | 2691 | `" if($p !== '' && $p[0] === '?'){ $nullable = true; $p = substr($p, 1); }"` |
|     - | 2692 | `" if($p !== '' && $p[0] === '('){ $p = substr($p, 1, strlen($p) - 2); }"` |
|     - | 2693 | `" if(strpos($p, '&') !== false){"` |
|     - | 2694 | `"  $subs = array();"` |
|     - | 2695 | `"  foreach(explode('&', $p) as $s){ $subs[] = new ReflectionNamedType($s, false, $s); }"` |
|     - | 2696 | `"  return new ReflectionIntersectionType($p, $subs);"` |
|     - | 2697 | `" }"` |
|     - | 2698 | `" return new ReflectionNamedType($p, $nullable, $nullable ? '?'.$p : $p);"` |
|     - | 2699 | `"}"` |
|     - | 2700 | `"function __reflect_make_type($text){"` |
|     - | 2701 | `" if($text === null \|\| $text === ''){ return null; }"` |
|     - | 2702 | `" $nullable = false;"` |
|     - | 2703 | `" $body = $text;"` |
|     - | 2704 | `" if($body[0] === '?'){ $nullable = true; $body = substr($body, 1); }"` |
|     - | 2705 | `" $parts = array();"` |
|     - | 2706 | `" $depth = 0;"` |
|     - | 2707 | `" $cur = '';"` |
|     - | 2708 | `" $n = strlen($body);"` |
|     - | 2709 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 2710 | `"  $ch = $body[$k];"` |
|     - | 2711 | `"  if($ch === '('){ $depth++; $cur .= $ch; }"` |
|     - | 2712 | `"  else if($ch === ')'){ $depth--; $cur .= $ch; }"` |
|     - | 2713 | `"  else if($ch === '\|' && $depth === 0){ $parts[] = $cur; $cur = ''; }"` |
|     - | 2714 | `"  else{ $cur .= $ch; }"` |
|     - | 2715 | `" }"` |
|     - | 2716 | `" $parts[] = $cur;"` |
|     - | 2717 | `" if(count($parts) > 1){"` |
|     - | 2718 | `"  $nonNull = array();"` |
|     - | 2719 | `"  $hasNull = false;"` |
|     - | 2720 | `"  foreach($parts as $p){"` |
|     - | 2721 | `"   if(strtolower($p) === 'null'){ $hasNull = true; }"` |
|     - | 2722 | `"   else{ $nonNull[] = $p; }"` |
|     - | 2723 | `"  }"` |
|     - | 2724 | `"  if($hasNull && count($nonNull) === 1 && strpos($nonNull[0], '&') === false){"` |
|     - | 2725 | `"   return new ReflectionNamedType($nonNull[0], true, '?'.$nonNull[0]);"` |
|     - | 2726 | `"  }"` |
|     - | 2727 | `"  $types = array();"` |
|     - | 2728 | `"  foreach($parts as $p){ $types[] = __reflect_make_atom($p); }"` |
|     - | 2729 | `"  return new ReflectionUnionType($body, $nullable \|\| $hasNull, $types);"` |
|     - | 2730 | `" }"` |
|     - | 2731 | `" if(strpos($body, '&') !== false){ return __reflect_make_atom($body); }"` |
|     - | 2732 | `" return __reflect_make_atom($nullable ? '?'.$body : $body);"` |
|     - | 2733 | `"}"` |
|     - | 2734 | `;` |
|     - | 2735 | `/*` |
|     - | 2736 | ` * Chunk 5: ReflectionGenerator, ReflectionFiber. Executing line/file and` |
|     - | 2737 | ` * traces need runtime line tracking the VM does not have (same gap as` |
|     - | 2738 | ` * debug_backtrace's line numbers) — those throw a loud Error, recorded in` |
|     - | 2739 | ` * the plan ledger.` |
|     - | 2740 | ` */` |
|     - | 2741 | `static const char zReflectLib5[] =` |
|     - | 2742 | `"class ReflectionGenerator {"` |
|     - | 2743 | `" protected $__gen;"` |
|     - | 2744 | `" public function __construct($generator){"` |
|     - | 2745 | `"  if(!($generator instanceof Generator)){"` |
|     - | 2746 | `"   throw new TypeError('ReflectionGenerator::__construct(): Argument #1 ($generator) must be of type Generator, '.get_debug_type($generator).' given');"` |
|     - | 2747 | `"  }"` |
|     - | 2748 | `"  $this->__gen = $generator;"` |
|     - | 2749 | `" }"` |
|     - | 2750 | `" protected function __rginfo(){ return __reflect_gen_info($this->__gen); }"` |
|     - | 2751 | `" public function getFunction(){"` |
|     - | 2752 | `"  $i = $this->__rginfo();"` |
|     - | 2753 | `"  if($i['kind'] === 'method'){ return new ReflectionMethod($i['class'], $i['name']); }"` |
|     - | 2754 | `"  return new ReflectionFunction($i['name']);"` |
|     - | 2755 | `" }"` |
|     - | 2756 | `" public function getThis(){ $i = $this->__rginfo(); return isset($i['this']) ? $i['this'] : null; }"` |
|     - | 2757 | `" public function getExecutingGenerator(){ return __reflect_gen_exec($this->__gen); }"` |
|     - | 2758 | `" public function isClosed(){ $i = $this->__rginfo(); return $i['closed']; }"` |
|     - | 2759 | `" public function getExecutingLine(){"` |
|     - | 2760 | `"  throw new Error('ReflectionGenerator::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2761 | `" }"` |
|     - | 2762 | `" public function getExecutingFile(){"` |
|     - | 2763 | `"  throw new Error('ReflectionGenerator::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2764 | `" }"` |
|     - | 2765 | `" public function getTrace($options = 1){"` |
|     - | 2766 | `"  throw new Error('ReflectionGenerator::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2767 | `" }"` |
|     - | 2768 | `"}"` |
|     - | 2769 | `"class ReflectionFiber {"` |
|     - | 2770 | `" protected $__fiber;"` |
|     - | 2771 | `" public function __construct($fiber){"` |
|     - | 2772 | `"  if(!($fiber instanceof Fiber)){"` |
|     - | 2773 | `"   throw new TypeError('ReflectionFiber::__construct(): Argument #1 ($fiber) must be of type Fiber, '.get_debug_type($fiber).' given');"` |
|     - | 2774 | `"  }"` |
|     - | 2775 | `"  $this->__fiber = $fiber;"` |
|     - | 2776 | `" }"` |
|     - | 2777 | `" public function getFiber(){ return $this->__fiber; }"` |
|     - | 2778 | `" public function getCallable(){ return __reflect_prop_read($this->__fiber, '__callable'); }"` |
|     - | 2779 | `" public function getExecutingLine(){"` |
|     - | 2780 | `"  throw new Error('ReflectionFiber::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2781 | `" }"` |
|     - | 2782 | `" public function getExecutingFile(){"` |
|     - | 2783 | `"  throw new Error('ReflectionFiber::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2784 | `" }"` |
|     - | 2785 | `" public function getTrace($options = 1){"` |
|     - | 2786 | `"  throw new Error('ReflectionFiber::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2787 | `" }"` |
|     - | 2788 | `"}"` |
|     - | 2789 | `;` |
|     - | 2790 | `/*` |
|     - | 2791 | ` * Chunk 6: the long tail — ReflectionConstant (PHP 8.5), the synthetic` |
|     - | 2792 | ` * "Core" ReflectionExtension, ReflectionZendExtension (throws: no Zend` |
|     - | 2793 | ` * extensions exist), the ReflectionEnum family (throws: enums are not a` |
|     - | 2794 | ` * PHL language feature yet), and ReflectionReference.` |
|     - | 2795 | ` */` |
|     - | 2796 | `static const char zReflectLib6[] =` |
|     - | 2797 | `"class ReflectionConstant implements Reflector {"` |
|     - | 2798 | `" public $name;"` |
|     - | 2799 | `" public function __construct($name){"` |
|     - | 2800 | `"  if(!is_string($name)){"` |
|     - | 2801 | `"   throw new TypeError('ReflectionConstant::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2802 | `"  }"` |
|     - | 2803 | `"  $i = __reflect_const_info($name);"` |
|     - | 2804 | `"  if($i === null){"` |
|     - | 2805 | `"   throw new ReflectionException('Constant \"'.$name.'\" does not exist');"` |
|     - | 2806 | `"  }"` |
|     - | 2807 | `"  $this->name = $name;"` |
|     - | 2808 | `" }"` |
|     - | 2809 | `" public function getName(){ return $this->name; }"` |
|     - | 2810 | `" public function getNamespaceName(){"` |
|     - | 2811 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2812 | `"  if($p === false){ return ''; }"` |
|     - | 2813 | `"  return substr($this->name,0,$p);"` |
|     - | 2814 | `" }"` |
|     - | 2815 | `" public function getShortName(){"` |
|     - | 2816 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2817 | `"  if($p === false){ return $this->name; }"` |
|     - | 2818 | `"  return substr($this->name,$p+1);"` |
|     - | 2819 | `" }"` |
|     - | 2820 | `" public function getValue(){"` |
|     - | 2821 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2822 | `"  return $i['value'];"` |
|     - | 2823 | `" }"` |
|     - | 2824 | `" public function isDeprecated(){ return false; }"` |
|     - | 2825 | `" public function getFileName(){"` |
|     - | 2826 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2827 | `"  return $i['file'];"` |
|     - | 2828 | `" }"` |
|     - | 2829 | `" public function getExtension(){"` |
|     - | 2830 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2831 | `"  return $i['internal'] ? new ReflectionExtension('Core') : null;"` |
|     - | 2832 | `" }"` |
|     - | 2833 | `" public function getExtensionName(){"` |
|     - | 2834 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2835 | `"  return $i['internal'] ? 'Core' : false;"` |
|     - | 2836 | `" }"` |
|     - | 2837 | `" public function getAttributes($name = null, $flags = 0){ return array(); }"` |
|     - | 2838 | `" public function __toString(){"` |
|     - | 2839 | `"  return 'Constant [ '.$this->name.' ]'.\"\\n\";"` |
|     - | 2840 | `" }"` |
|     - | 2841 | `"}"` |
|     - | 2842 | `"class ReflectionExtension implements Reflector {"` |
|     - | 2843 | `" public $name;"` |
|     - | 2844 | `" public function __construct($name){"` |
|     - | 2845 | `"  if(!is_string($name)){"` |
|     - | 2846 | `"   throw new TypeError('ReflectionExtension::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2847 | `"  }"` |
|     - | 2848 | `"  if(strtolower($name) !== 'core'){"` |
|     - | 2849 | `"   throw new ReflectionException('Extension \"'.$name.'\" does not exist');"` |
|     - | 2850 | `"  }"` |
|     - | 2851 | `"  $this->name = 'Core';"` |
|     - | 2852 | `" }"` |
|     - | 2853 | `" public function getName(){ return $this->name; }"` |
|     - | 2854 | `" public function getVersion(){ return phpversion(); }"` |
|     - | 2855 | `" public function getFunctions(){ return array(); }"` |
|     - | 2856 | `" public function getClasses(){ return array(); }"` |
|     - | 2857 | `" public function getClassNames(){ return array(); }"` |
|     - | 2858 | `" public function getConstants(){ return array(); }"` |
|     - | 2859 | `" public function getINIEntries(){ return array(); }"` |
|     - | 2860 | `" public function getDependencies(){ return array(); }"` |
|     - | 2861 | `" public function isPersistent(){ return true; }"` |
|     - | 2862 | `" public function isTemporary(){ return false; }"` |
|     - | 2863 | `" public function info(){ }"` |
|     - | 2864 | `" public function __toString(){"` |
|     - | 2865 | `"  return 'Extension [ extension #1 '.$this->name.' ]'.\"\\n\";"` |
|     - | 2866 | `" }"` |
|     - | 2867 | `"}"` |
|     - | 2868 | `"class ReflectionZendExtension implements Reflector {"` |
|     - | 2869 | `" public $name;"` |
|     - | 2870 | `" public function __construct($name){"` |
|     - | 2871 | `"  throw new ReflectionException('Zend Extension \"'.$name.'\" does not exist');"` |
|     - | 2872 | `" }"` |
|     - | 2873 | `" public function getName(){ return $this->name; }"` |
|     - | 2874 | `" public function __toString(){ return ''; }"` |
|     - | 2875 | `"}"` |
|     - | 2876 | `"class ReflectionEnum extends ReflectionClass {"` |
|     - | 2877 | `" public function __construct($objectOrClass){"` |
|     - | 2878 | `"  $info = __reflect_class_info($objectOrClass);"` |
|     - | 2879 | `"  if($info === null){"` |
|     - | 2880 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 2881 | `"  }"` |
|     - | 2882 | `"  if(!$info['enum']){"` |
|     - | 2883 | `"   throw new ReflectionException('Class \"'.$info['name'].'\" is not an enum');"` |
|     - | 2884 | `"  }"` |
|     - | 2885 | `"  parent::__construct($objectOrClass);"` |
|     - | 2886 | `" }"` |
|     - | 2887 | `" public function hasCase($name){"` |
|     - | 2888 | `"  $i = $this->__rinfo();"` |
|     - | 2889 | `"  return in_array($name, $i['cases'], true);"` |
|     - | 2890 | `" }"` |
|     - | 2891 | `" public function getCase($name){"` |
|     - | 2892 | `"  if(!$this->hasCase($name)){"` |
|     - | 2893 | `"   throw new ReflectionException('Case '.$this->name.'::'.$name.' does not exist');"` |
|     - | 2894 | `"  }"` |
|     - | 2895 | `"  if($this->isBacked()){ return new ReflectionEnumBackedCase($this->name, $name); }"` |
|     - | 2896 | `"  return new ReflectionEnumUnitCase($this->name, $name);"` |
|     - | 2897 | `" }"` |
|     - | 2898 | `" public function getCases(){"` |
|     - | 2899 | `"  $i = $this->__rinfo();"` |
|     - | 2900 | `"  $out = array();"` |
|     - | 2901 | `"  foreach($i['cases'] as $c){"` |
|     - | 2902 | `"   $out[] = $this->isBacked()"` |
|     - | 2903 | `"    ? new ReflectionEnumBackedCase($this->name, $c)"` |
|     - | 2904 | `"    : new ReflectionEnumUnitCase($this->name, $c);"` |
|     - | 2905 | `"  }"` |
|     - | 2906 | `"  return $out;"` |
|     - | 2907 | `" }"` |
|     - | 2908 | `" public function isBacked(){ $i = $this->__rinfo(); return $i['enumbacking'] !== ''; }"` |
|     - | 2909 | `" public function getBackingType(){"` |
|     - | 2910 | `"  $i = $this->__rinfo();"` |
|     - | 2911 | `"  if($i['enumbacking'] === ''){ return null; }"` |
|     - | 2912 | `"  return __reflect_make_type($i['enumbacking']);"` |
|     - | 2913 | `" }"` |
|     - | 2914 | `"}"` |
|     - | 2915 | `"class ReflectionEnumUnitCase extends ReflectionClassConstant {"` |
|     - | 2916 | `" public function __construct($class, $constant){"` |
|     - | 2917 | `"  parent::__construct($class, $constant);"` |
|     - | 2918 | `"  $ci = __reflect_class_info($class);"` |
|     - | 2919 | `"  if(!$ci['enum']){"` |
|     - | 2920 | `"   throw new ReflectionException('Class \"'.$this->class.'\" is not an enum');"` |
|     - | 2921 | `"  }"` |
|     - | 2922 | `"  $m = $this->__rcmeta();"` |
|     - | 2923 | `"  if(!$m['enumcase']){"` |
|     - | 2924 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' is not a case');"` |
|     - | 2925 | `"  }"` |
|     - | 2926 | `" }"` |
|     - | 2927 | `" public function getEnum(){ return new ReflectionEnum($this->class); }"` |
|     - | 2928 | `"}"` |
|     - | 2929 | `"class ReflectionEnumBackedCase extends ReflectionEnumUnitCase {"` |
|     - | 2930 | `" public function getBackingValue(){ return $this->getValue()->value; }"` |
|     - | 2931 | `"}"` |
|     - | 2932 | `"final class ReflectionReference {"` |
|     - | 2933 | `" protected $__id = '';"` |
|     - | 2934 | `" public function __construct(){"` |
|     - | 2935 | `"  throw new Error('Call to private ReflectionReference::__construct() from global scope');"` |
|     - | 2936 | `" }"` |
|     - | 2937 | `" public static function fromArrayElement($array, $key){"` |
|     - | 2938 | `"  if(!is_array($array)){"` |
|     - | 2939 | `"   throw new TypeError('ReflectionReference::fromArrayElement(): Argument #1 ($array) must be of type array, '.get_debug_type($array).' given');"` |
|     - | 2940 | `"  }"` |
|     - | 2941 | `"  $id = __reflect_ref_id($array, $key);"` |
|     - | 2942 | `"  if($id === null){ return null; }"` |
|     - | 2943 | `"  $r = __reflect_new_no_ctor('ReflectionReference');"` |
|     - | 2944 | `"  $r->__setId('phlref'.$id);"` |
|     - | 2945 | `"  return $r;"` |
|     - | 2946 | `" }"` |
|     - | 2947 | `" public function __setId($id){ $this->__id = $id; }"` |
|     - | 2948 | `" public function getId(){ return $this->__id; }"` |
|     - | 2949 | `"}"` |
|     - | 2950 | `;` |
|     - | 2951 | `/*` |
|     - | 2952 | ` * Chunk 7: ReflectionAttribute and the shared getAttributes() builder.` |
|     - | 2953 | ` * The spec array rides as [kind, target, member, paramIdx]; argument` |
|     - | 2954 | ` * values evaluate lazily through __reflect_attr_args (PHP semantics).` |
|     - | 2955 | ` */` |
|     - | 2956 | `static const char zReflectLib7[] =` |
|     - | 2957 | `"function __reflect_has_deprecated($meta){"` |
|     - | 2958 | `" foreach($meta as $a){"` |
|     - | 2959 | `"  if(strtolower($a['name']) === 'deprecated'){ return true; }"` |
|     - | 2960 | `" }"` |
|     - | 2961 | `" return false;"` |
|     - | 2962 | `"}"` |
|     - | 2963 | `"function __reflect_target_names($mask){"` |
|     - | 2964 | `" $parts = array();"` |
|     - | 2965 | `" foreach(array('class' => 1, 'function' => 2, 'method' => 4, 'property' => 8,"` |
|     - | 2966 | `"  'class constant' => 16, 'parameter' => 32, 'constant' => 64) as $nm => $bit){"` |
|     - | 2967 | `"  if($mask & $bit){ $parts[] = $nm; }"` |
|     - | 2968 | `" }"` |
|     - | 2969 | `" return implode(', ', $parts);"` |
|     - | 2970 | `"}"` |
|     - | 2971 | `"function __reflect_build_attrs($meta, $spec, $target, $name, $flags){"` |
|     - | 2972 | `" $out = array();"` |
|     - | 2973 | `" $counts = array();"` |
|     - | 2974 | `" foreach($meta as $a){"` |
|     - | 2975 | `"  $k = strtolower($a['name']);"` |
|     - | 2976 | `"  $counts[$k] = isset($counts[$k]) ? $counts[$k] + 1 : 1;"` |
|     - | 2977 | `" }"` |
|     - | 2978 | `" $idx = 0;"` |
|     - | 2979 | `" foreach($meta as $a){"` |
|     - | 2980 | `"  $keep = true;"` |
|     - | 2981 | `"  if($name !== null){"` |
|     - | 2982 | `"   $keep = strtolower($a['name']) === strtolower($name);"` |
|     - | 2983 | `"   if(!$keep && ($flags & 2)){"` |
|     - | 2984 | `"    $keep = is_subclass_of($a['name'], $name);"` |
|     - | 2985 | `"   }"` |
|     - | 2986 | `"  }"` |
|     - | 2987 | `"  if($keep){"` |
|     - | 2988 | `"   $r = __reflect_new_no_ctor('ReflectionAttribute');"` |
|     - | 2989 | `"   $r->__init($a['name'], $spec, $idx, $target, $counts[strtolower($a['name'])] > 1);"` |
|     - | 2990 | `"   $out[] = $r;"` |
|     - | 2991 | `"  }"` |
|     - | 2992 | `"  $idx++;"` |
|     - | 2993 | `" }"` |
|     - | 2994 | `" return $out;"` |
|     - | 2995 | `"}"` |
|     - | 2996 | `"final class ReflectionAttribute {"` |
|     - | 2997 | `" const IS_INSTANCEOF = 2;"` |
|     - | 2998 | `" protected $__name = '';"` |
|     - | 2999 | `" protected $__spec = null;"` |
|     - | 3000 | `" protected $__idx = 0;"` |
|     - | 3001 | `" protected $__target = 0;"` |
|     - | 3002 | `" protected $__rep = false;"` |
|     - | 3003 | `" public function __construct(){"` |
|     - | 3004 | `"  throw new Error('Call to private ReflectionAttribute::__construct() from global scope');"` |
|     - | 3005 | `" }"` |
|     - | 3006 | `" public function __init($name, $spec, $idx, $target, $rep){"` |
|     - | 3007 | `"  $this->__name = $name;"` |
|     - | 3008 | `"  $this->__spec = $spec;"` |
|     - | 3009 | `"  $this->__idx = $idx;"` |
|     - | 3010 | `"  $this->__target = $target;"` |
|     - | 3011 | `"  $this->__rep = $rep;"` |
|     - | 3012 | `" }"` |
|     - | 3013 | `" public function getName(){ return $this->__name; }"` |
|     - | 3014 | `" public function getTarget(){ return $this->__target; }"` |
|     - | 3015 | `" public function isRepeated(){ return $this->__rep; }"` |
|     - | 3016 | `" public function getArguments(){"` |
|     - | 3017 | `"  $a = __reflect_attr_args($this->__spec[0], $this->__spec[1], $this->__spec[2], $this->__spec[3], $this->__idx);"` |
|     - | 3018 | `"  return $a === null ? array() : $a;"` |
|     - | 3019 | `" }"` |
|     - | 3020 | `" public function newInstance(){"` |
|     - | 3021 | `"  $name = $this->__name;"` |
|     - | 3022 | `"  $ci = __reflect_class_info($name);"` |
|     - | 3023 | `"  if($ci === null){"` |
|     - | 3024 | `"   throw new Error('Attribute class \"'.$name.'\" not found');"` |
|     - | 3025 | `"  }"` |
|     - | 3026 | `"  $name = $ci['name'];"` |
|     - | 3027 | `"  $decl = null;"` |
|     - | 3028 | `"  $didx = 0;"` |
|     - | 3029 | `"  foreach($ci['attrs'] as $a){"` |
|     - | 3030 | `"   if(strtolower($a['name']) === 'attribute'){ $decl = $didx; break; }"` |
|     - | 3031 | `"   $didx++;"` |
|     - | 3032 | `"  }"` |
|     - | 3033 | `"  if($decl === null){"` |
|     - | 3034 | `"   throw new Error('Attempting to use non-attribute class \"'.$name.'\" as attribute');"` |
|     - | 3035 | `"  }"` |
|     - | 3036 | `"  $dargs = __reflect_attr_args('class', $name, null, 0, $decl);"` |
|     - | 3037 | `"  $flags = 127;"` |
|     - | 3038 | `"  if(is_array($dargs)){"` |
|     - | 3039 | `"   if(isset($dargs[0])){ $flags = $dargs[0]; }"` |
|     - | 3040 | `"   else if(isset($dargs['flags'])){ $flags = $dargs['flags']; }"` |
|     - | 3041 | `"  }"` |
|     - | 3042 | `"  if(($flags & $this->__target) === 0){"` |
|     - | 3043 | `"   $tnames = array(1 => 'class', 2 => 'function', 4 => 'method', 8 => 'property',"` |
|     - | 3044 | `"    16 => 'class constant', 32 => 'parameter', 64 => 'constant');"` |
|     - | 3045 | `"   throw new Error('Attribute \"'.$name.'\" cannot target '.$tnames[$this->__target]"` |
|     - | 3046 | `"    .' (allowed targets: '.__reflect_target_names($flags).')');"` |
|     - | 3047 | `"  }"` |
|     - | 3048 | `"  if($this->__rep && ($flags & 128) === 0){"` |
|     - | 3049 | `"   throw new Error('Attribute \"'.$name.'\" must not be repeated');"` |
|     - | 3050 | `"  }"` |
|     - | 3051 | `"  return __reflect_new_instance($name, $this->getArguments());"` |
|     - | 3052 | `" }"` |
|     - | 3053 | `" public function __toString(){"` |
|     - | 3054 | `"  return 'Attribute [ '.$this->__name.' ]';"` |
|     - | 3055 | `" }"` |
|     - | 3056 | `"}"` |
|     - | 3057 | `;` |
|     - | 3058 | `/*` |
|     - | 3059 | ` * Chunk 8: signature-table support. Internal (C builtin) functions carry a` |
|     - | 3060 | ` * PHP-style parameter-list string; these helpers parse it into the same` |
|     - | 3061 | ` * param-meta shape user functions get, so ReflectionFunction and` |
|     - | 3062 | ` * ReflectionParameter work uniformly over builtins.` |
|     - | 3063 | ` */` |
|     - | 3064 | `static const char zReflectLib8[] =` |
|     - | 3065 | `"function __reflect_sig_split($sig){"` |
|     - | 3066 | `" $parts = array();"` |
|     - | 3067 | `" $cur = '';"` |
|     - | 3068 | `" $q = false;"` |
|     - | 3069 | `" $n = strlen($sig);"` |
|     - | 3070 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3071 | `"  $ch = $sig[$k];"` |
|     - | 3072 | `"  if($q){"` |
|     - | 3073 | `"   $cur .= $ch;"` |
|     - | 3074 | `"   if($ch === chr(92) && $k + 1 < $n){ $cur .= $sig[$k+1]; $k++; }"` |
|     - | 3075 | `"   else if($ch === chr(39)){ $q = false; }"` |
|     - | 3076 | `"  }else if($ch === chr(39)){ $q = true; $cur .= $ch; }"` |
|     - | 3077 | `"  else if($ch === ',' ){ $parts[] = trim($cur); $cur = ''; }"` |
|     - | 3078 | `"  else{ $cur .= $ch; }"` |
|     - | 3079 | `" }"` |
|     - | 3080 | `" if(trim($cur) !== ''){ $parts[] = trim($cur); }"` |
|     - | 3081 | `" return $parts;"` |
|     - | 3082 | `"}"` |
|     - | 3083 | `"function __reflect_sig_scalar($t){"` |
|     - | 3084 | `" if($t === '?'){ return array(false, null); }"` |
|     - | 3085 | `" if($t === 'NULL' \|\| $t === 'null'){ return array(true, null); }"` |
|     - | 3086 | `" if($t === 'true'){ return array(true, true); }"` |
|     - | 3087 | `" if($t === 'false'){ return array(true, false); }"` |
|     - | 3088 | `" if(is_numeric($t)){"` |
|     - | 3089 | `"  if(strpos($t, '.') === false && stripos($t, 'e') === false && strpos($t, 'x') === false){"` |
|     - | 3090 | `"   return array(true, (int)$t);"` |
|     - | 3091 | `"  }"` |
|     - | 3092 | `"  return array(true, (float)$t);"` |
|     - | 3093 | `" }"` |
|     - | 3094 | `" if(strlen($t) >= 2 && $t[0] === chr(39) && $t[strlen($t)-1] === chr(39)){"` |
|     - | 3095 | `"  $body = substr($t, 1, strlen($t) - 2);"` |
|     - | 3096 | `"  return array(true, strtr($body, array(chr(92).chr(39) => chr(39), chr(92).chr(92) => chr(92))));"` |
|     - | 3097 | `" }"` |
|     - | 3098 | `" return array(false, null);"` |
|     - | 3099 | `"}"` |
|     - | 3100 | `"function __reflect_parse_sig($sig){"` |
|     - | 3101 | `" $params = array();"` |
|     - | 3102 | `" $pos = 0;"` |
|     - | 3103 | `" foreach(__reflect_sig_split($sig) as $part){"` |
|     - | 3104 | `"  $deftext = null;"` |
|     - | 3105 | `"  $q = false;"` |
|     - | 3106 | `"  $n = strlen($part);"` |
|     - | 3107 | `"  for($k = 0; $k < $n; $k++){"` |
|     - | 3108 | `"   $ch = $part[$k];"` |
|     - | 3109 | `"   if($q){"` |
|     - | 3110 | `"    if($ch === chr(92)){ $k++; }"` |
|     - | 3111 | `"    else if($ch === chr(39)){ $q = false; }"` |
|     - | 3112 | `"   }else if($ch === chr(39)){ $q = true; }"` |
|     - | 3113 | `"   else if($ch === '=' ){"` |
|     - | 3114 | `"    $deftext = trim(substr($part, $k + 1));"` |
|     - | 3115 | `"    $part = trim(substr($part, 0, $k));"` |
|     - | 3116 | `"    break;"` |
|     - | 3117 | `"   }"` |
|     - | 3118 | `"  }"` |
|     - | 3119 | `"  $variadic = strpos($part, '...') !== false;"` |
|     - | 3120 | `"  $byref = strpos($part, '&') !== false;"` |
|     - | 3121 | `"  $d = strpos($part, '$');"` |
|     - | 3122 | `"  $name = $d === false ? $part : substr($part, $d + 1);"` |
|     - | 3123 | `"  $typetext = null;"` |
|     - | 3124 | `"  $sp = strpos($part, ' ');"` |
|     - | 3125 | `"  if($sp !== false && $d !== false && $sp < $d){ $typetext = substr($part, 0, $sp); }"` |
|     - | 3126 | `"  $nullable = $typetext !== null && ($typetext[0] === '?' \|\| stripos($typetext, 'null') !== false);"` |
|     - | 3127 | `"  $params[] = array('name' => $name, 'pos' => $pos, 'byref' => $byref,"` |
|     - | 3128 | `"   'variadic' => $variadic, 'hasdef' => $deftext !== null, 'nullable' => $nullable,"` |
|     - | 3129 | `"   'promoted' => false, 'typetext' => $typetext, 'attrs' => array(), 'deftext' => $deftext);"` |
|     - | 3130 | `"  $pos++;"` |
|     - | 3131 | `" }"` |
|     - | 3132 | `" return $params;"` |
|     - | 3133 | `"}"` |
|     - | 3134 | `"function __reflect_sig_fixup($i){"` |
|     - | 3135 | `" if($i === null){ return $i; }"` |
|     - | 3136 | `" if(isset($i['ret2'])){ $i['rettext'] = $i['ret2']; }"` |
|     - | 3137 | `" if(!isset($i['sig']) \|\| $i['sig'] === ''){ return $i; }"` |
|     - | 3138 | `" $i['params'] = __reflect_parse_sig($i['sig']);"` |
|     - | 3139 | `" $i['minarg'] = -1;"` |
|     - | 3140 | `" $v = false;"` |
|     - | 3141 | `" foreach($i['params'] as $p){ if($p['variadic']){ $v = true; } }"` |
|     - | 3142 | `" $i['variadic'] = $v;"` |
|     - | 3143 | `" return $i;"` |
|     - | 3144 | `"}"` |
|     - | 3145 | `;` |
|     - | 3146 | `/*` |
|     - | 3147 | ` * Chunk 9: PHP's Reflection export format (__toString on every Reflector).` |
|     - | 3148 | ` * Built entirely from the public reflection API of the target objects.` |
|     - | 3149 | ` */` |
|     - | 3150 | `static const char zReflectLib9[] =` |
|     - | 3151 | `"function __reflect_export_value($v){"` |
|     - | 3152 | `" if($v === null){ return 'NULL'; }"` |
|     - | 3153 | `" if($v === true){ return 'true'; }"` |
|     - | 3154 | `" if($v === false){ return 'false'; }"` |
|     - | 3155 | `" if(is_string($v)){ return chr(39).$v.chr(39); }"` |
|     - | 3156 | `" if(is_array($v)){"` |
|     - | 3157 | `"  $parts = array();"` |
|     - | 3158 | `"  $isList = true;"` |
|     - | 3159 | `"  $next = 0;"` |
|     - | 3160 | `"  foreach($v as $k => $x){"` |
|     - | 3161 | `"   if($k !== $next){ $isList = false; break; }"` |
|     - | 3162 | `"   $next++;"` |
|     - | 3163 | `"  }"` |
|     - | 3164 | `"  foreach($v as $k => $x){"` |
|     - | 3165 | `"   $parts[] = $isList ? __reflect_export_value($x)"` |
|     - | 3166 | `"    : (__reflect_export_value($k).' => '.__reflect_export_value($x));"` |
|     - | 3167 | `"  }"` |
|     - | 3168 | `"  return '['.implode(', ', $parts).']';"` |
|     - | 3169 | `" }"` |
|     - | 3170 | `" return (string)$v;"` |
|     - | 3171 | `"}"` |
|     - | 3172 | `"function __reflect_export_param($p){"` |
|     - | 3173 | `" $s = 'Parameter #'.$p->getPosition().' [ <'.($p->isOptional() ? 'optional' : 'required').'> ';"` |
|     - | 3174 | `" $t = $p->getType();"` |
|     - | 3175 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3176 | `" if($p->isPassedByReference()){ $s .= '&'; }"` |
|     - | 3177 | `" if($p->isVariadic()){ $s .= '...'; }"` |
|     - | 3178 | `" $s .= '$'.$p->getName();"` |
|     - | 3179 | `" if($p->isDefaultValueAvailable()){"` |
|     - | 3180 | `"  try{ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3181 | `"  catch(ReflectionException $e){ $s .= ' = <default>'; }"` |
|     - | 3182 | `" }"` |
|     - | 3183 | `" return $s.' ]';"` |
|     - | 3184 | `"}"` |
|     - | 3185 | `"function __reflect_export_prop($p){"` |
|     - | 3186 | `" $s = 'Property [ ';"` |
|     - | 3187 | `" $s .= $p->isPrivate() ? 'private ' : ($p->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3188 | `" if($p->isStatic()){ $s .= 'static '; }"` |
|     - | 3189 | `" if($p->isReadOnly()){ $s .= 'readonly '; }"` |
|     - | 3190 | `" $t = $p->getType();"` |
|     - | 3191 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3192 | `" $s .= '$'.$p->getName();"` |
|     - | 3193 | `" if($p->hasDefaultValue()){ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3194 | `" return $s.' ]'.chr(10);"` |
|     - | 3195 | `"}"` |
|     - | 3196 | `"function __reflect_export_cconst($c){"` |
|     - | 3197 | `" $v = $c->getValue();"` |
|     - | 3198 | `" if(is_int($v)){ $t = 'int'; }"` |
|     - | 3199 | `" else if(is_string($v)){ $t = 'string'; }"` |
|     - | 3200 | `" else if(is_float($v)){ $t = 'float'; }"` |
|     - | 3201 | `" else if(is_bool($v)){ $t = 'bool'; }"` |
|     - | 3202 | `" else if(is_array($v)){ $t = 'array'; }"` |
|     - | 3203 | `" else{ $t = 'null'; }"` |
|     - | 3204 | `" $vs = is_array($v) ? 'Array' : (is_bool($v) ? ($v ? '1' : '') : (string)$v);"` |
|     - | 3205 | `" $vis = $c->isPrivate() ? 'private' : ($c->isProtected() ? 'protected' : 'public');"` |
|     - | 3206 | `" return 'Constant [ '.$vis.' '.$t.' '.$c->name.' ] { '.$vs.' }'.chr(10);"` |
|     - | 3207 | `"}"` |
|     - | 3208 | `"function __reflect_export_fnabs($r, $indent){"` |
|     - | 3209 | `" $tags = $r->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3210 | `" if($r instanceof ReflectionMethod){"` |
|     - | 3211 | `"  if($r->isConstructor()){ $tags .= ', ctor'; }"` |
|     - | 3212 | `"  else if($r->isDestructor()){ $tags .= ', dtor'; }"` |
|     - | 3213 | `"  $decl = $r->getDeclaringClass()->name;"` |
|     - | 3214 | `"  if(strtolower($decl) !== strtolower($r->class)){ $tags .= ', inherits '.$decl; }"` |
|     - | 3215 | `"  else if($r->hasPrototype()){ $tags .= ', prototype '.$r->getPrototype()->class; }"` |
|     - | 3216 | `"  $head = 'Method [ <'.$tags.'> ';"` |
|     - | 3217 | `"  if($r->isAbstract()){ $head .= 'abstract '; }"` |
|     - | 3218 | `"  if($r->isFinal()){ $head .= 'final '; }"` |
|     - | 3219 | `"  if($r->isStatic()){ $head .= 'static '; }"` |
|     - | 3220 | `"  $head .= $r->isPrivate() ? 'private ' : ($r->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3221 | `"  $head .= 'method '.$r->name.' ]';"` |
|     - | 3222 | `" }else{"` |
|     - | 3223 | `"  $kind = $r->isClosure() ? 'Closure' : 'Function';"` |
|     - | 3224 | `"  $head = $kind.' [ <'.$tags.'> function '.$r->name.' ]';"` |
|     - | 3225 | `" }"` |
|     - | 3226 | `" $s = $head.' {'.chr(10);"` |
|     - | 3227 | `" if(!$r->isInternal()){"` |
|     - | 3228 | `"  $s .= '  @@ '.$r->getFileName().' '.$r->getStartLine().' - '.$r->getEndLine().chr(10);"` |
|     - | 3229 | `" }"` |
|     - | 3230 | `" $ps = $r->getParameters();"` |
|     - | 3231 | `" $ret = $r->getReturnType();"` |
|     - | 3232 | `" if(count($ps) > 0 \|\| $ret !== null){"` |
|     - | 3233 | `"  $s .= chr(10).'  - Parameters ['.count($ps).'] {'.chr(10);"` |
|     - | 3234 | `"  foreach($ps as $p){ $s .= '    '.__reflect_export_param($p).chr(10); }"` |
|     - | 3235 | `"  $s .= '  }'.chr(10);"` |
|     - | 3236 | `" }"` |
|     - | 3237 | `" if($ret !== null){ $s .= '  - Return [ '.(string)$ret.' ]'.chr(10); }"` |
|     - | 3238 | `" $s .= '}'.chr(10);"` |
|     - | 3239 | `" if($indent === ''){ return $s; }"` |
|     - | 3240 | `" $lines = explode(chr(10), $s);"` |
|     - | 3241 | `" $out = '';"` |
|     - | 3242 | `" $n = count($lines);"` |
|     - | 3243 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3244 | `"  if($lines[$k] === '' && $k === $n - 1){ break; }"` |
|     - | 3245 | `"  $out .= ($lines[$k] === '' ? '' : $indent.$lines[$k]).chr(10);"` |
|     - | 3246 | `" }"` |
|     - | 3247 | `" return $out;"` |
|     - | 3248 | `"}"` |
|     - | 3249 | `"function __reflect_export_class($rc){"` |
|     - | 3250 | `" $tags = $rc->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3251 | `" if($rc->isInterface()){"` |
|     - | 3252 | `"  $head = 'Interface [ <'.$tags.'> interface '.$rc->name.' ]';"` |
|     - | 3253 | `" }else{"` |
|     - | 3254 | `"  $mods = '';"` |
|     - | 3255 | `"  if($rc->isAbstract()){ $mods .= 'abstract '; }"` |
|     - | 3256 | `"  if($rc->isFinal()){ $mods .= 'final '; }"` |
|     - | 3257 | `"  $head = 'Class [ <'.$tags.'> '.$mods.'class '.$rc->name;"` |
|     - | 3258 | `"  $par = $rc->getParentClass();"` |
|     - | 3259 | `"  if($par !== false){ $head .= ' extends '.$par->name; }"` |
|     - | 3260 | `"  $ifs = $rc->getInterfaceNames();"` |
|     - | 3261 | `"  if(count($ifs) > 0){ $head .= ' implements '.implode(', ', $ifs); }"` |
|     - | 3262 | `"  $head .= ' ]';"` |
|     - | 3263 | `" }"` |
|     - | 3264 | `" $s = $head.' {'.chr(10);"` |
|     - | 3265 | `" if(!$rc->isInternal()){"` |
|     - | 3266 | `"  $s .= '  @@ '.$rc->getFileName().' '.$rc->getStartLine().'-'.$rc->getEndLine().chr(10);"` |
|     - | 3267 | `" }"` |
|     - | 3268 | `" $consts = $rc->getReflectionConstants();"` |
|     - | 3269 | `" $s .= chr(10).'  - Constants ['.count($consts).'] {'.chr(10);"` |
|     - | 3270 | `" foreach($consts as $c){ $s .= '    '.__reflect_export_cconst($c); }"` |
|     - | 3271 | `" $s .= '  }'.chr(10);"` |
|     - | 3272 | `" $sp = array();"` |
|     - | 3273 | `" $ip = array();"` |
|     - | 3274 | `" foreach($rc->getProperties() as $p){"` |
|     - | 3275 | `"  if($p->isStatic()){ $sp[] = $p; }else{ $ip[] = $p; }"` |
|     - | 3276 | `" }"` |
|     - | 3277 | `" $sm = array();"` |
|     - | 3278 | `" $im = array();"` |
|     - | 3279 | `" foreach($rc->getMethods() as $m){"` |
|     - | 3280 | `"  if($m->isStatic()){ $sm[] = $m; }else{ $im[] = $m; }"` |
|     - | 3281 | `" }"` |
|     - | 3282 | `" $s .= chr(10).'  - Static properties ['.count($sp).'] {'.chr(10);"` |
|     - | 3283 | `" foreach($sp as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3284 | `" $s .= '  }'.chr(10);"` |
|     - | 3285 | `" $s .= chr(10).'  - Static methods ['.count($sm).'] {'.chr(10);"` |
|     - | 3286 | `" $first = true;"` |
|     - | 3287 | `" foreach($sm as $m){"` |
|     - | 3288 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3289 | `"  $first = false;"` |
|     - | 3290 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3291 | `" }"` |
|     - | 3292 | `" $s .= '  }'.chr(10);"` |
|     - | 3293 | `" $s .= chr(10).'  - Properties ['.count($ip).'] {'.chr(10);"` |
|     - | 3294 | `" foreach($ip as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3295 | `" $s .= '  }'.chr(10);"` |
|     - | 3296 | `" $s .= chr(10).'  - Methods ['.count($im).'] {'.chr(10);"` |
|     - | 3297 | `" $first = true;"` |
|     - | 3298 | `" foreach($im as $m){"` |
|     - | 3299 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3300 | `"  $first = false;"` |
|     - | 3301 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3302 | `" }"` |
|     - | 3303 | `" $s .= '  }'.chr(10);"` |
|     - | 3304 | `" return $s.'}'.chr(10);"` |
|     - | 3305 | `"}"` |
|     - | 3306 | `;` |
|     - | 3307 | `/*` |
|     - | 3308 | ` * Register the __reflect_* thunks and compile the Reflection library.` |
|     - | 3309 | ` * Called from PH7_VmInit while pVm->bCompilingBuiltin is set, right after` |
|     - | 3310 | ` * the core builtin chunks (Exception and friends must exist already).` |
|     - | 3311 | ` */` |
|  3884 | 3312 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|     5 | 3313 | `{` |
|     - | 3314 | `	static const struct {` |
|     - | 3315 | `		const char *zName;` |
|     - | 3316 | `		ProchHostFunction xFunc;` |
|     - | 3317 | `	} aFunc[] = {` |
|     - | 3318 | `		{ "__reflect_class_info",     vm_builtin_reflect_class_info },` |
|     - | 3319 | `		{ "__reflect_const_value",    vm_builtin_reflect_const_value },` |
|     - | 3320 | `		{ "__reflect_static_value",   vm_builtin_reflect_static_value },` |
|     - | 3321 | `		{ "__reflect_static_set",     vm_builtin_reflect_static_set },` |
|     - | 3322 | `		{ "__reflect_prop_default",   vm_builtin_reflect_prop_default },` |
|     - | 3323 | `		{ "__reflect_new_instance",   vm_builtin_reflect_new_instance },` |
|     - | 3324 | `		{ "__reflect_new_no_ctor",    vm_builtin_reflect_new_no_ctor },` |
|     - | 3325 | `		{ "__reflect_func_info",      vm_builtin_reflect_func_info },` |
|     - | 3326 | `		{ "__reflect_param_default",  vm_builtin_reflect_param_default },` |
|     - | 3327 | `		{ "__reflect_param_defconst", vm_builtin_reflect_param_defconst },` |
|     - | 3328 | `		{ "__reflect_invoke",         vm_builtin_reflect_invoke },` |
|     - | 3329 | `		{ "__reflect_closure",        vm_builtin_reflect_closure },` |
|     - | 3330 | `		{ "__reflect_prop_read",      vm_builtin_reflect_prop_read },` |
|     - | 3331 | `		{ "__reflect_prop_write",     vm_builtin_reflect_prop_write },` |
|     - | 3332 | `		{ "__reflect_prop_state",     vm_builtin_reflect_prop_state },` |
|     - | 3333 | `		{ "__reflect_dyn_props",      vm_builtin_reflect_dyn_props },` |
|     - | 3334 | `		{ "__reflect_gen_info",       vm_builtin_reflect_gen_info },` |
|     - | 3335 | `		{ "__reflect_gen_exec",       vm_builtin_reflect_gen_exec },` |
|     - | 3336 | `		{ "__reflect_const_info",     vm_builtin_reflect_const_info },` |
|     - | 3337 | `		{ "__reflect_ref_id",         vm_builtin_reflect_ref_id },` |
|     - | 3338 | `		{ "__reflect_attr_args",      vm_builtin_reflect_attr_args },` |
|     - | 3339 | `	};` |
|     - | 3340 | `	sxu32 n;` |
|     - | 3341 | `	sxi32 rc;` |
| 85453 | 3342 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 81569 | 3343 | `		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);` |
| 40787 | 3344 | `	}` |
|  3889 | 3345 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib1, sizeof(zReflectLib1)-1);` |
|  3889 | 3346 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3347 | `		return rc;` |
|     - | 3348 | `	}` |
|  3889 | 3349 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib2, sizeof(zReflectLib2)-1);` |
|  3889 | 3350 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3351 | `		return rc;` |
|     - | 3352 | `	}` |
|  3889 | 3353 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib3, sizeof(zReflectLib3)-1);` |
|  3889 | 3354 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3355 | `		return rc;` |
|     - | 3356 | `	}` |
|  3889 | 3357 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib4, sizeof(zReflectLib4)-1);` |
|  3889 | 3358 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3359 | `		return rc;` |
|     - | 3360 | `	}` |
|  3889 | 3361 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib5, sizeof(zReflectLib5)-1);` |
|  3889 | 3362 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3363 | `		return rc;` |
|     - | 3364 | `	}` |
|  3889 | 3365 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib6, sizeof(zReflectLib6)-1);` |
|  3889 | 3366 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3367 | `		return rc;` |
|     - | 3368 | `	}` |
|  3889 | 3369 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib7, sizeof(zReflectLib7)-1);` |
|  3889 | 3370 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3371 | `		return rc;` |
|     - | 3372 | `	}` |
|  3889 | 3373 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib8, sizeof(zReflectLib8)-1);` |
|  3889 | 3374 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3375 | `		return rc;` |
|     - | 3376 | `	}` |
|  3889 | 3377 | `	return PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib9, sizeof(zReflectLib9)-1);` |
|  1947 | 3378 | `}` |
|     - | 3379 |  |
