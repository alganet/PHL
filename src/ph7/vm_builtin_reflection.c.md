# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1021/1195 lines (85.44%)

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
| 40530 |   63 | `static void ReflectMapAddBool(ph7_context *pCtx, ph7_value *pMap, const char *zKey, int b)` |
|     1 |   64 | `{` |
| 40531 |   65 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 40531 |   66 | `	if( p == 0 ){ return; }` |
| 40531 |   67 | `	ph7_value_bool(p, b);` |
| 40531 |   68 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
| 20266 |   69 | `}` |
| 17350 |   70 | `static void ReflectMapAddInt(ph7_context *pCtx, ph7_value *pMap, const char *zKey, sxi64 iVal)` |
|     1 |   71 | `{` |
| 17351 |   72 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 17351 |   73 | `	if( p == 0 ){ return; }` |
| 17351 |   74 | `	ph7_value_int64(p, iVal);` |
| 17351 |   75 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  8676 |   76 | `}` |
| 12614 |   77 | `static void ReflectMapAddStr(ph7_context *pCtx, ph7_value *pMap, const char *zKey,` |
|     - |   78 | `	const char *zVal, int nVal)` |
|     1 |   79 | `{` |
| 12615 |   80 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 12615 |   81 | `	if( p == 0 ){ return; }` |
| 12615 |   82 | `	ph7_value_string(p, zVal, nVal);` |
| 12615 |   83 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  6308 |   84 | `}` |
|  4244 |   85 | `static void ReflectMapAddNull(ph7_context *pCtx, ph7_value *pMap, const char *zKey)` |
|     1 |   86 | `{` |
|  4245 |   87 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  4245 |   88 | `	if( p == 0 ){ return; }` |
|  4245 |   89 | `	ph7_value_null(p);` |
|  4245 |   90 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  2123 |   91 | `}` |
|     - |   92 | `/* Add an entry under a dynamic (SyString) key. */` |
|  5018 |   93 | `static void ReflectMapAddDyn(ph7_context *pCtx, ph7_value *pMap,` |
|     - |   94 | `	const SyString *pKey, ph7_value *pVal)` |
|     1 |   95 | `{` |
|  5019 |   96 | `	ph7_value *pK = ph7_context_new_scalar(pCtx);` |
|  5019 |   97 | `	if( pK == 0 ){ return; }` |
|  5019 |   98 | `	ph7_value_string(pK, pKey->zString, (int)pKey->nByte);` |
|  5019 |   99 | `	ph7_array_add_elem(pMap, pK, pVal);` |
|  2510 |  100 | `}` |
|     - |  101 | `/* Emit the declared #[...] attributes of a target as a summary list:` |
|     - |  102 | ` * [ {name, line} ... ]. Argument values stay lazy — the PHP layer pulls` |
|     - |  103 | ` * them through __reflect_attr_args when ReflectionAttribute needs them. */` |
|  5640 |  104 | `static void ReflectMapAddAttrs(ph7_context *pCtx, ph7_value *pMap, SySet *pAttrs)` |
|     1 |  105 | `{` |
|  5641 |  106 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|  5641 |  107 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|     - |  108 | `	sxu32 n;` |
|  5641 |  109 | `	if( pList == 0 ){` |
|   ! 0 |  110 | `		return;` |
|     - |  111 | `	}` |
|  5905 |  112 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|   265 |  113 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|   265 |  114 | `		if( pMeta == 0 ){ break; }` |
|   265 |  115 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aA[n].sName), (int)SyStringLength(&aA[n].sName));` |
|   265 |  116 | `		ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)aA[n].nLine);` |
|   265 |  117 | `		ph7_array_add_elem(pList, 0, pMeta);` |
|   133 |  118 | `	}` |
|  5641 |  119 | `	ph7_array_add_strkey_elem(pMap, "attrs", pList);` |
|  2821 |  120 | `}` |
|     - |  121 | `/* Emit a doc-comment field: the text when present, else boolean false` |
|     - |  122 | ` * (getDocComment()'s exact return contract). */` |
|  4388 |  123 | `static void ReflectMapAddDoc(ph7_context *pCtx, ph7_value *pMap, const SyString *pDoc)` |
|     1 |  124 | `{` |
|  4389 |  125 | `	if( SyStringLength(pDoc) > 0 ){` |
|    63 |  126 | `		ReflectMapAddStr(pCtx, pMap, "doc", SyStringData(pDoc), (int)SyStringLength(pDoc));` |
|    32 |  127 | `	}else{` |
|  4327 |  128 | `		ReflectMapAddBool(pCtx, pMap, "doc", 0);` |
|     - |  129 | `	}` |
|  4389 |  130 | `}` |
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
|   414 |  931 | `static ph7_class_instance * ReflectValueClosure(ph7_vm *pVm, ph7_value *pVal)` |
|     1 |  932 | `{` |
|     - |  933 | `	ph7_class_instance *pThis;` |
|   415 |  934 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
|   391 |  935 | `		return 0;` |
|     - |  936 | `	}` |
|    25 |  937 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    25 |  938 | `	return (pThis->pClass == pVm->pClosureClass) ? pThis : 0;` |
|   208 |  939 | `}` |
|     - |  940 | `/*` |
|     - |  941 | ` * Resolve a reflection callable target into its compiled function.` |
|     - |  942 | ` *   - pMethodArg a non-empty string  -> method mode: pTarget is a class name` |
|     - |  943 | ` *     or object; outputs *ppClass and *ppMeth.` |
|     - |  944 | ` *   - pTarget a Closure              -> unwrap $__fn into hFunction; *ppClosure.` |
|     - |  945 | ` *   - pTarget a string               -> hFunction (user) or hHostFunction` |
|     - |  946 | ` *     (*ppHost set, returns NULL).` |
|     - |  947 | ` * Returns the ph7_vm_func, or NULL (host function or unresolvable).` |
|     - |  948 | ` */` |
|   694 |  949 | `static ph7_vm_func * ReflectResolveCallable(ph7_context *pCtx, ph7_value *pTarget,` |
|     - |  950 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|     - |  951 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|     1 |  952 | `{` |
|   695 |  953 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  954 | `	SyHashEntry *pEntry;` |
|   695 |  955 | `	if( ppClass ){ *ppClass = 0; }` |
|   695 |  956 | `	if( ppMeth ){ *ppMeth = 0; }` |
|   695 |  957 | `	if( ppHost ){ *ppHost = 0; }` |
|   695 |  958 | `	if( ppClosure ){ *ppClosure = 0; }` |
|   695 |  959 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
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
|   415 |  975 | `		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|   415 |  976 | `		if( pClo ){` |
|     - |  977 | `			SyString sAttr;` |
|     - |  978 | `			ph7_value *pFn;` |
|    25 |  979 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    25 |  980 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|    25 |  981 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) < 1 ){` |
|   ! 0 |  982 | `				return 0;` |
|     - |  983 | `			}` |
|    25 |  984 | `			pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    25 |  985 | `			if( pEntry == 0 ){` |
|     - |  986 | `				/* A Closure over a host function (Closure::fromCallable('strlen')) */` |
|   ! 0 |  987 | `				pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|   ! 0 |  988 | `				if( pEntry && ppHost ){` |
|   ! 0 |  989 | `					*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|   ! 0 |  990 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|   ! 0 |  991 | `				}` |
|   ! 0 |  992 | `				return 0;` |
|     - |  993 | `			}` |
|    25 |  994 | `			if( ppClosure ){ *ppClosure = pClo; }` |
|    25 |  995 | `			return (ph7_vm_func *)pEntry->pUserData;` |
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
|   348 | 1012 | `}` |
|     - | 1013 | `/* Emit the shared descriptor fields of a compiled function. */` |
|   546 | 1014 | `static void ReflectFillFuncCommon(ph7_context *pCtx, ph7_value *pInfo, ph7_vm_func *pFunc)` |
|     1 | 1015 | `{` |
|     - | 1016 | `	ph7_vm_func_arg *aArg;` |
|     - | 1017 | `	ph7_value *pParams, *pStatics;` |
|   547 | 1018 | `	int bVariadic = 0;` |
|     - | 1019 | `	int bAnon;` |
|     - | 1020 | `	sxu32 n;` |
|     - | 1021 | ``	/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|     - | 1022 | `	 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|   547 | 1023 | `	bAnon = (pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|   546 | 1024 | `	if( !bAnon && SyStringLength(&pFunc->sName) > 9` |
|   312 | 1025 | `	 && (SyMemcmp(SyStringData(&pFunc->sName), "[lambda_", 8) == 0` |
|    85 | 1026 | `	  \|\| SyMemcmp(SyStringData(&pFunc->sName), "[closure_", 9) == 0) ){` |
|    11 | 1027 | `		bAnon = 1;` |
|     5 | 1028 | `	}` |
|   547 | 1029 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|   547 | 1030 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pFunc->iFlags & VM_FUNC_INTERNAL) != 0);` |
|   547 | 1031 | `	ReflectMapAddBool(pCtx, pInfo, "closure", bAnon);` |
|   547 | 1032 | `	ReflectMapAddBool(pCtx, pInfo, "byref", (pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|   547 | 1033 | `	ReflectMapAddBool(pCtx, pInfo, "generator", (pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|   547 | 1034 | `	ReflectMapAddBool(pCtx, pInfo, "strict", pFunc->bStrictTypes != 0);` |
|   547 | 1035 | `	if( SyStringLength(&pFunc->sFile) > 0 ){` |
|   529 | 1036 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pFunc->sFile), (int)SyStringLength(&pFunc->sFile));` |
|   265 | 1037 | `	}else{` |
|    19 | 1038 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1039 | `	}` |
|   547 | 1040 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pFunc->nLine);` |
|   547 | 1041 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pFunc->nEndLine);` |
|   547 | 1042 | `	ReflectMapAddDoc(pCtx, pInfo, &pFunc->sDoc);` |
|   547 | 1043 | `	ReflectMapAddAttrs(pCtx, pInfo, &pFunc->aAttrs);` |
|   547 | 1044 | `	if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|   145 | 1045 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", SyStringData(&pFunc->sReturnTypeName),` |
|    96 | 1046 | `			(int)SyStringLength(&pFunc->sReturnTypeName));` |
|   499 | 1047 | `	}else if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     - | 1048 | `		/* The type-text renderer omits void/never atoms (compile.c notes the` |
|     - | 1049 | `		 * root fix belongs there); name them here for getReturnType(). */` |
|     3 | 1050 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "void", sizeof("void")-1);` |
|   450 | 1051 | `	}else if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|     3 | 1052 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "never", sizeof("never")-1);` |
|     2 | 1053 | `	}else{` |
|   447 | 1054 | `		ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1055 | `	}` |
|   547 | 1056 | `	ReflectMapAddBool(pCtx, pInfo, "retnullable", (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) != 0);` |
|     - | 1057 | `	/* Parameters */` |
|   547 | 1058 | `	pParams = ph7_context_new_array(pCtx);` |
|   547 | 1059 | `	aArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|  1799 | 1060 | `	for( n = 0 ; pParams && n < SySetUsed(&pFunc->aArgs) ; n++ ){` |
|  1253 | 1061 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|  1253 | 1062 | `		if( pMeta == 0 ){ break; }` |
|  1253 | 1063 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aArg[n].sName), (int)SyStringLength(&aArg[n].sName));` |
|  1253 | 1064 | `		ReflectMapAddInt(pCtx, pMeta, "pos", (sxi64)n);` |
|  1253 | 1065 | `		ReflectMapAddBool(pCtx, pMeta, "byref", (aArg[n].iFlags & VM_FUNC_ARG_BY_REF) != 0);` |
|  1253 | 1066 | `		ReflectMapAddBool(pCtx, pMeta, "variadic", (aArg[n].iFlags & VM_FUNC_ARG_VARIADIC) != 0);` |
|     - | 1067 | `		/* The compiler never sets ARG_HAS_DEF; a default = compiled bytecode` |
|     - | 1068 | `		 * (same test the OP_CALL default-value path uses). */` |
|  1253 | 1069 | `		ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&aArg[n].aByteCode) > 0);` |
|  1253 | 1070 | `		ReflectMapAddBool(pCtx, pMeta, "nullable", (aArg[n].iFlags & VM_FUNC_ARG_NULLABLE) != 0);` |
|  1253 | 1071 | `		ReflectMapAddBool(pCtx, pMeta, "promoted", (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) != 0);` |
|  1253 | 1072 | `		if( SyStringLength(&aArg[n].sTypeName) > 0 ){` |
|   715 | 1073 | `			ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&aArg[n].sTypeName),` |
|   476 | 1074 | `				(int)SyStringLength(&aArg[n].sTypeName));` |
|   239 | 1075 | `		}else{` |
|   777 | 1076 | `			ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - | 1077 | `		}` |
|  1253 | 1078 | `		ReflectMapAddAttrs(pCtx, pMeta, &aArg[n].aAttrs);` |
|  1253 | 1079 | `		ph7_array_add_elem(pParams, 0, pMeta);` |
|  1253 | 1080 | `		if( aArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|   183 | 1081 | `			bVariadic = 1;` |
|    91 | 1082 | `		}` |
|   627 | 1083 | `	}` |
|   547 | 1084 | `	if( pParams ){` |
|   547 | 1085 | `		ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|   273 | 1086 | `	}` |
|   547 | 1087 | `	ReflectMapAddBool(pCtx, pInfo, "variadic", bVariadic);` |
|     - | 1088 | `	/* Static variables: current value when the slot was initialized (first` |
|     - | 1089 | `	 * call), otherwise the evaluated default — PHP's getStaticVariables` |
|     - | 1090 | `	 * initializes on demand and reports the same values. */` |
|   547 | 1091 | `	pStatics = ph7_context_new_array(pCtx);` |
|   547 | 1092 | `	if( pStatics ){` |
|   547 | 1093 | `		ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|   575 | 1094 | `		for( n = 0 ; n < SySetUsed(&pFunc->aStatic) ; n++ ){` |
|    29 | 1095 | `			ph7_value *pVal = 0;` |
|     - | 1096 | `			ph7_value sScratch;` |
|    29 | 1097 | `			int bScratch = 0;` |
|    29 | 1098 | `			if( aStatic[n].nIdx != SXU32_HIGH ){` |
|    11 | 1099 | `				pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);` |
|     5 | 1100 | `			}` |
|    29 | 1101 | `			if( pVal == 0 ){` |
|    19 | 1102 | `				PH7_MemObjInit(pCtx->pVm, &sScratch);` |
|    19 | 1103 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|    19 | 1104 | `					VmLocalExec(pCtx->pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|     9 | 1105 | `				}` |
|    19 | 1106 | `				pVal = &sScratch;` |
|    19 | 1107 | `				bScratch = 1;` |
|     9 | 1108 | `			}` |
|    29 | 1109 | `			ReflectMapAddDyn(pCtx, pStatics, &aStatic[n].sName, pVal);` |
|    29 | 1110 | `			if( bScratch ){` |
|    19 | 1111 | `				PH7_MemObjRelease(&sScratch);` |
|     9 | 1112 | `			}` |
|    15 | 1113 | `		}` |
|   547 | 1114 | `		ph7_array_add_strkey_elem(pInfo, "statics", pStatics);` |
|   273 | 1115 | `	}` |
|   547 | 1116 | `}` |
|     - | 1117 | `/*` |
|     - | 1118 | ` * array\|null __reflect_func_info(string\|Closure $target [, string $method])` |
|     - | 1119 | ` * Function/method/closure descriptor for the PHP layer.` |
|     - | 1120 | ` */` |
|   652 | 1121 | `static int vm_builtin_reflect_func_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1122 | `{` |
|     - | 1123 | `	ph7_vm_func *pFunc;` |
|   653 | 1124 | `	ph7_class *pClass = 0;` |
|   653 | 1125 | `	ph7_class_method *pMeth = 0;` |
|   653 | 1126 | `	ph7_user_func *pHost = 0;` |
|   653 | 1127 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1128 | `	ph7_value *pInfo;` |
|   653 | 1129 | `	if( nArg < 1 ){` |
|   ! 0 | 1130 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1131 | `		return PH7_OK;` |
|     - | 1132 | `	}` |
|   653 | 1133 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], nArg > 1 ? apArg[1] : 0,` |
|     - | 1134 | `		&pClass, &pMeth, &pHost, &pClosure);` |
|   653 | 1135 | `	if( pFunc == 0 && pHost == 0 ){` |
|     3 | 1136 | `		ph7_result_null(pCtx);` |
|     3 | 1137 | `		return PH7_OK;` |
|     - | 1138 | `	}` |
|   651 | 1139 | `	pInfo = ph7_context_new_array(pCtx);` |
|   651 | 1140 | `	if( pInfo == 0 ){` |
|   ! 0 | 1141 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1142 | `		return PH7_OK;` |
|     - | 1143 | `	}` |
|   651 | 1144 | `	if( pFunc == 0 ){` |
|     - | 1145 | `		/* Host (C builtin) function: no parameter metadata beyond arity */` |
|   105 | 1146 | `		ph7_value *pParams = ph7_context_new_array(pCtx);` |
|   105 | 1147 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pHost->sName), (int)SyStringLength(&pHost->sName));` |
|   105 | 1148 | `		ReflectMapAddBool(pCtx, pInfo, "internal", 1);` |
|   105 | 1149 | `		ReflectMapAddBool(pCtx, pInfo, "closure", 0);` |
|   105 | 1150 | `		ReflectMapAddBool(pCtx, pInfo, "byref", 0);` |
|   105 | 1151 | `		ReflectMapAddBool(pCtx, pInfo, "generator", 0);` |
|   105 | 1152 | `		ReflectMapAddBool(pCtx, pInfo, "strict", 0);` |
|   105 | 1153 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|   105 | 1154 | `		ReflectMapAddInt(pCtx, pInfo, "line", 0);` |
|   105 | 1155 | `		ReflectMapAddInt(pCtx, pInfo, "endline", 0);` |
|   105 | 1156 | `		ReflectMapAddBool(pCtx, pInfo, "doc", 0);` |
|     - | 1157 | `		{` |
|   105 | 1158 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|   105 | 1159 | `			if( pEmpty ){` |
|   105 | 1160 | `				ph7_array_add_strkey_elem(pInfo, "attrs", pEmpty);` |
|    52 | 1161 | `			}` |
|     - | 1162 | `		}` |
|   105 | 1163 | `		if( pHost->zRet ){` |
|   105 | 1164 | `			ReflectMapAddStr(pCtx, pInfo, "rettext", pHost->zRet, (int)SyStrlen(pHost->zRet));` |
|    53 | 1165 | `		}else{` |
|   ! 0 | 1166 | `			ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1167 | `		}` |
|   105 | 1168 | `		ReflectMapAddBool(pCtx, pInfo, "retnullable", 0);` |
|   105 | 1169 | `		if( pParams ){` |
|   105 | 1170 | `			ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|    52 | 1171 | `		}` |
|   105 | 1172 | `		ReflectMapAddBool(pCtx, pInfo, "variadic", 0);` |
|   105 | 1173 | `		ReflectMapAddInt(pCtx, pInfo, "minarg", (sxi64)pHost->nMinArg);` |
|   105 | 1174 | `		if( pHost->zSig ){` |
|   105 | 1175 | `			ReflectMapAddStr(pCtx, pInfo, "sig", pHost->zSig, (int)SyStrlen(pHost->zSig));` |
|    53 | 1176 | `		}else{` |
|   ! 0 | 1177 | `			ReflectMapAddStr(pCtx, pInfo, "sig", "", 0);` |
|     - | 1178 | `		}` |
|   105 | 1179 | `		ph7_result_value(pCtx, pInfo);` |
|   105 | 1180 | `		return PH7_OK;` |
|     - | 1181 | `	}` |
|   547 | 1182 | `	ReflectFillFuncCommon(pCtx, pInfo, pFunc);` |
|   547 | 1183 | `	ReflectMapAddInt(pCtx, pInfo, "minarg", -1);` |
|   547 | 1184 | `	if( (pFunc->iFlags & VM_FUNC_INTERNAL) && SySetUsed(&pFunc->aArgs) == 0 && pMeth == 0 ){` |
|     - | 1185 | `		/* Embedded-PHP builtin (max/min...): declared argless, actual` |
|     - | 1186 | `		 * signature comes from the static table */` |
|     5 | 1187 | `		const char *zRet = 0;` |
|     5 | 1188 | `		const char *zSig = PH7_VmBuiltinSigLookup(SyStringData(&pFunc->sName), SyStringLength(&pFunc->sName), &zRet);` |
|     5 | 1189 | `		if( zSig ){` |
|     5 | 1190 | `			ReflectMapAddStr(pCtx, pInfo, "sig", zSig, (int)SyStrlen(zSig));` |
|     2 | 1191 | `		}` |
|     5 | 1192 | `		if( zRet && SyStringLength(&pFunc->sReturnTypeName) == 0 ){` |
|     5 | 1193 | `			ReflectMapAddStr(pCtx, pInfo, "ret2", zRet, (int)SyStrlen(zRet));` |
|     2 | 1194 | `		}` |
|     2 | 1195 | `	}` |
|   547 | 1196 | `	if( pMeth && pClass ){` |
|   259 | 1197 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|   259 | 1198 | `		ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   259 | 1199 | `		ReflectMapAddStr(pCtx, pInfo, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   259 | 1200 | `		ReflectMapAddInt(pCtx, pInfo, "vis", (sxi64)pMeth->iProtection);` |
|   259 | 1201 | `		ReflectMapAddBool(pCtx, pInfo, "mstatic", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   259 | 1202 | `		ReflectMapAddBool(pCtx, pInfo, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|   259 | 1203 | `		ReflectMapAddBool(pCtx, pInfo, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   129 | 1204 | `	}` |
|   547 | 1205 | `	if( pClosure ){` |
|     - | 1206 | `		SyString sAttr;` |
|     - | 1207 | `		ph7_value *pAttr;` |
|     - | 1208 | `		ph7_value *pUsed;` |
|    25 | 1209 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|    25 | 1210 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    25 | 1211 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|   ! 0 | 1212 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 1213 | `			if( pKey ){` |
|   ! 0 | 1214 | `				ph7_value_string(pKey, "this", 4);` |
|   ! 0 | 1215 | `				ph7_array_add_elem(pInfo, pKey, pAttr);` |
|   ! 0 | 1216 | `			}` |
|   ! 0 | 1217 | `		}else{` |
|    25 | 1218 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|     - | 1219 | `		}` |
|    25 | 1220 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|    25 | 1221 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    25 | 1222 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|   ! 0 | 1223 | `			ReflectMapAddStr(pCtx, pInfo, "scope", (const char *)SyBlobData(&pAttr->sBlob),` |
|   ! 0 | 1224 | `				(int)SyBlobLength(&pAttr->sBlob));` |
|   ! 0 | 1225 | `		}else{` |
|    25 | 1226 | `			ReflectMapAddNull(pCtx, pInfo, "scope");` |
|     - | 1227 | `		}` |
|     - | 1228 | `		/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */` |
|    25 | 1229 | `		pUsed = ph7_context_new_array(pCtx);` |
|    25 | 1230 | `		if( pUsed ){` |
|    25 | 1231 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     - | 1232 | `			sxu32 n;` |
|    53 | 1233 | `			for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; n++ ){` |
|    29 | 1234 | `				if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){` |
|    15 | 1235 | `					continue;` |
|     - | 1236 | `				}` |
|    14 | 1237 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|     8 | 1238 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|   ! 0 | 1239 | `					continue;` |
|     - | 1240 | `				}` |
|    15 | 1241 | `				ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, &aEnv[n].sValue);` |
|     8 | 1242 | `			}` |
|    25 | 1243 | `			ph7_array_add_strkey_elem(pInfo, "used", pUsed);` |
|    12 | 1244 | `		}` |
|    12 | 1245 | `	}` |
|   547 | 1246 | `	ph7_result_value(pCtx, pInfo);` |
|   547 | 1247 | `	return PH7_OK;` |
|   327 | 1248 | `}` |
|     - | 1249 | `/*` |
|     - | 1250 | ` * mixed __reflect_param_default(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1251 | ` * Evaluate a parameter's compiled default expression.` |
|     - | 1252 | ` */` |
|    12 | 1253 | `static int vm_builtin_reflect_param_default(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1254 | `{` |
|     - | 1255 | `	ph7_vm_func *pFunc;` |
|     - | 1256 | `	ph7_vm_func_arg *pArg;` |
|     - | 1257 | `	ph7_value sValue;` |
|     - | 1258 | `	sxu32 nIdx;` |
|    13 | 1259 | `	if( nArg < 3 ){` |
|   ! 0 | 1260 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1261 | `		return PH7_OK;` |
|     - | 1262 | `	}` |
|    13 | 1263 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|    13 | 1264 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|    12 | 1265 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|    13 | 1266 | `	 \|\| SySetUsed(&pArg->aByteCode) < 1 ){` |
|   ! 0 | 1267 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1268 | `		return PH7_OK;` |
|     - | 1269 | `	}` |
|    13 | 1270 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|    13 | 1271 | `	VmLocalExec(pCtx->pVm, &pArg->aByteCode, &sValue, FALSE);` |
|    13 | 1272 | `	ph7_result_value(pCtx, &sValue);` |
|    13 | 1273 | `	PH7_MemObjRelease(&sValue);` |
|    13 | 1274 | `	return PH7_OK;` |
|     7 | 1275 | `}` |
|     - | 1276 | `/*` |
|     - | 1277 | ` * string\|null __reflect_param_defconst(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1278 | ` * When a parameter's default is a plain global-constant reference, its` |
|     - | 1279 | ` * source name; null otherwise. A constant default compiles to exactly` |
|     - | 1280 | ` * [ OP_LOADC (EXPAND) , OP_DONE ] with the name in the literal table.` |
|     - | 1281 | ` */` |
|     6 | 1282 | `static int vm_builtin_reflect_param_defconst(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1283 | `{` |
|     - | 1284 | `	ph7_vm_func *pFunc;` |
|     - | 1285 | `	ph7_vm_func_arg *pArg;` |
|     - | 1286 | `	VmInstr *aInstr;` |
|     - | 1287 | `	ph7_value *pLit;` |
|     - | 1288 | `	sxu32 nIdx;` |
|     7 | 1289 | `	if( nArg < 3 ){` |
|   ! 0 | 1290 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1291 | `		return PH7_OK;` |
|     - | 1292 | `	}` |
|     7 | 1293 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|     7 | 1294 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|     6 | 1295 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|     7 | 1296 | `	 \|\| SySetUsed(&pArg->aByteCode) != 2 ){` |
|     3 | 1297 | `		ph7_result_null(pCtx);` |
|     3 | 1298 | `		return PH7_OK;` |
|     - | 1299 | `	}` |
|     5 | 1300 | `	aInstr = (VmInstr *)SySetBasePtr(&pArg->aByteCode);` |
|     4 | 1301 | `	if( aInstr[0].iOp != PH7_OP_LOADC \|\| (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0` |
|     5 | 1302 | `	 \|\| aInstr[1].iOp != PH7_OP_DONE ){` |
|   ! 0 | 1303 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1304 | `		return PH7_OK;` |
|     - | 1305 | `	}` |
|     5 | 1306 | `	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);` |
|     5 | 1307 | `	if( pLit == 0 \|\| SyBlobLength(&pLit->sBlob) < 1 ){` |
|   ! 0 | 1308 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1309 | `		return PH7_OK;` |
|     - | 1310 | `	}` |
|     5 | 1311 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&pLit->sBlob), (int)SyBlobLength(&pLit->sBlob));` |
|     5 | 1312 | `	return PH7_OK;` |
|     4 | 1313 | `}` |
|     - | 1314 | `/*` |
|     - | 1315 | ` * mixed __reflect_invoke(mixed $target, ?string $method, ?object $this, array $args)` |
|     - | 1316 | ` * Visibility-bypassing invocation (methods dispatch by VM name; functions` |
|     - | 1317 | ` * and closures ride PH7_VmCallUserFunction like call_user_func_array).` |
|     - | 1318 | ` */` |
|    20 | 1319 | `static int vm_builtin_reflect_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1320 | `{` |
|    21 | 1321 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1322 | `	ph7_value sResult;` |
|     - | 1323 | `	SySet aCallArg;` |
|     - | 1324 | `	sxi32 rc;` |
|    21 | 1325 | `	if( nArg < 4 ){` |
|   ! 0 | 1326 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1327 | `		return PH7_OK;` |
|     - | 1328 | `	}` |
|    21 | 1329 | `	PH7_MemObjInit(pVm, &sResult);` |
|    21 | 1330 | `	sResult.nIdx = SXU32_HIGH;` |
|    21 | 1331 | `	SySetInit(&aCallArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    21 | 1332 | `	ReflectCollectArgs(pCtx, apArg[3], &aCallArg, 0);` |
|    21 | 1333 | `	if( (apArg[1]->iFlags & MEMOBJ_STRING) && SyBlobLength(&apArg[1]->sBlob) > 0 ){` |
|    11 | 1334 | `		ph7_class *pClass = 0;` |
|    11 | 1335 | `		ph7_class_method *pMeth = 0;` |
|    11 | 1336 | `		ph7_class_instance *pThis = 0;` |
|    11 | 1337 | `		ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, 0);` |
|    11 | 1338 | `		if( pMeth == 0 ){` |
|   ! 0 | 1339 | `			SySetRelease(&aCallArg);` |
|   ! 0 | 1340 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1341 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1342 | `			return PH7_OK;` |
|     - | 1343 | `		}` |
|    11 | 1344 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     7 | 1345 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     3 | 1346 | `		}` |
|     - | 1347 | `		/* Reflection ignores method visibility (PHP 8.1+); the flag is` |
|     - | 1348 | `		 * consumed by the first OP_CALL, i.e. this synthetic one. */` |
|    11 | 1349 | `		pVm->bReflectBypass = 1;` |
|    16 | 1350 | `		rc = PH7_VmCallClassMethod(pVm, pThis, pMeth, &sResult,` |
|    10 | 1351 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg));` |
|    11 | 1352 | `		pVm->bReflectBypass = 0;` |
|     6 | 1353 | `	}else{` |
|    16 | 1354 | `		rc = PH7_VmCallUserFunction(pVm, apArg[0],` |
|    10 | 1355 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg), &sResult);` |
|     - | 1356 | `	}` |
|    21 | 1357 | `	SySetRelease(&aCallArg);` |
|    21 | 1358 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 | 1359 | `		PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1360 | `		return rc;` |
|     - | 1361 | `	}` |
|    21 | 1362 | `	ph7_result_value(pCtx, &sResult);` |
|    21 | 1363 | `	PH7_MemObjRelease(&sResult);` |
|    21 | 1364 | `	return PH7_OK;` |
|    11 | 1365 | `}` |
|     - | 1366 | `/*` |
|     - | 1367 | ` * Closure __reflect_closure(mixed $target, ?string $method, ?object $this)` |
|     - | 1368 | ` * Mint a Closure for a function or method, bound and scoped like the` |
|     - | 1369 | ` * first-class-callable path.` |
|     - | 1370 | ` */` |
|     6 | 1371 | `static int vm_builtin_reflect_closure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1372 | `{` |
|     7 | 1373 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 1374 | `	ph7_class *pClass = 0;` |
|     7 | 1375 | `	ph7_class_method *pMeth = 0;` |
|     7 | 1376 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1377 | `	ph7_vm_func *pFunc;` |
|     7 | 1378 | `	if( nArg < 3 ){` |
|   ! 0 | 1379 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1380 | `		return PH7_OK;` |
|     - | 1381 | `	}` |
|     7 | 1382 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, &pClosure);` |
|     7 | 1383 | `	if( pClosure ){` |
|     - | 1384 | `		/* Already a Closure: hand the same instance back */` |
|   ! 0 | 1385 | `		return ReflectResultExistingObject(pCtx, pClosure);` |
|     - | 1386 | `	}` |
|     7 | 1387 | `	if( pMeth && pClass ){` |
|     5 | 1388 | `		ph7_class_instance *pThis = 0;` |
|     5 | 1389 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     3 | 1390 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     1 | 1391 | `		}` |
|     7 | 1392 | `		return ReflectResultObject(pCtx,` |
|     4 | 1393 | `			PH7_VmNewClosure(pVm, &pMeth->sFunc.sName, pThis, &pClass->sName));` |
|     - | 1394 | `	}` |
|     3 | 1395 | `	if( pFunc ){` |
|     3 | 1396 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &pFunc->sName, 0, 0));` |
|     - | 1397 | `	}` |
|     - | 1398 | `	/* Host function by name */` |
|   ! 0 | 1399 | `	if( apArg[0]->iFlags & MEMOBJ_STRING ){` |
|     - | 1400 | `		SyString sName;` |
|   ! 0 | 1401 | `		SyStringInitFromBuf(&sName, (const char *)SyBlobData(&apArg[0]->sBlob), SyBlobLength(&apArg[0]->sBlob));` |
|   ! 0 | 1402 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &sName, 0, 0));` |
|     - | 1403 | `	}` |
|   ! 0 | 1404 | `	ph7_result_null(pCtx);` |
|   ! 0 | 1405 | `	return PH7_OK;` |
|     4 | 1406 | `}` |
|     - | 1407 | `/*` |
|     - | 1408 | ` * Resolve a Generator object into its wrapper. Mirrors the static` |
|     - | 1409 | ` * VmGeneratorExtractCtx in vm.c: the $__ctx attribute carries the` |
|     - | 1410 | ` * ph7_generator pointer as a resource value.` |
|     - | 1411 | ` */` |
|    22 | 1412 | `static ph7_generator * ReflectGeneratorCtx(ph7_vm *pVm, ph7_value *pVal)` |
|     1 | 1413 | `{` |
|     - | 1414 | `	ph7_class_instance *pThis;` |
|     - | 1415 | `	ph7_value *pAttr;` |
|     - | 1416 | `	SyString sAttr;` |
|    23 | 1417 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVm->pGeneratorClass == 0 ){` |
|   ! 0 | 1418 | `		return 0;` |
|     - | 1419 | `	}` |
|    23 | 1420 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    23 | 1421 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|   ! 0 | 1422 | `		return 0;` |
|     - | 1423 | `	}` |
|    23 | 1424 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    23 | 1425 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    23 | 1426 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|   ! 0 | 1427 | `		return 0;` |
|     - | 1428 | `	}` |
|    23 | 1429 | `	return (ph7_generator *)pAttr->x.pOther;` |
|    12 | 1430 | `}` |
|     - | 1431 | `/*` |
|     - | 1432 | ` * array\|null __reflect_gen_info(Generator $g)` |
|     - | 1433 | ` * {state, closed, executing, kind ('fn'\|'method'), name, class?, this}` |
|     - | 1434 | ` */` |
|    16 | 1435 | `static int vm_builtin_reflect_gen_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1436 | `{` |
|    17 | 1437 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1438 | `	ph7_generator *pGen;` |
|     - | 1439 | `	ph7_exec_ctx *pExec;` |
|     - | 1440 | `	ph7_value *pInfo;` |
|    17 | 1441 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 \|\| pGen->pCtx == 0 ){` |
|   ! 0 | 1442 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1443 | `		return PH7_OK;` |
|     - | 1444 | `	}` |
|    17 | 1445 | `	pExec = pGen->pCtx;` |
|    17 | 1446 | `	pInfo = ph7_context_new_array(pCtx);` |
|    17 | 1447 | `	if( pInfo == 0 ){` |
|   ! 0 | 1448 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1449 | `		return PH7_OK;` |
|     - | 1450 | `	}` |
|    17 | 1451 | `	ReflectMapAddInt(pCtx, pInfo, "state", (sxi64)pExec->iState);` |
|    24 | 1452 | `	ReflectMapAddBool(pCtx, pInfo, "closed",` |
|    16 | 1453 | `		pExec->iState == PH7_CTX_STATE_COMPLETED \|\| pExec->iState == PH7_CTX_STATE_CLOSED);` |
|    17 | 1454 | `	ReflectMapAddBool(pCtx, pInfo, "executing", pVm->pActiveCtx == pExec);` |
|    17 | 1455 | `	if( pExec->pFunc ){` |
|    17 | 1456 | `		ph7_vm_func *pFunc = pExec->pFunc;` |
|    19 | 1457 | `		if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     5 | 1458 | `			ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|     5 | 1459 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "method", sizeof("method")-1);` |
|     5 | 1460 | `			ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|     3 | 1461 | `		}else{` |
|    13 | 1462 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "fn", sizeof("fn")-1);` |
|     - | 1463 | `		}` |
|    17 | 1464 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|     8 | 1465 | `	}` |
|     - | 1466 | `	{` |
|     - | 1467 | `		/* The coroutine frame installs $this as a frame VARIABLE (see` |
|     - | 1468 | `		 * VmFiberSetupFrame), not as pFrame->pThis — check both. */` |
|    17 | 1469 | `		ph7_value *pThisVal = 0;` |
|    17 | 1470 | `		if( pExec->pFrame ){` |
|    17 | 1471 | `			SyHashEntry *pVar = SyHashGet(&pExec->pFrame->hVar, "this", sizeof("this")-1);` |
|    17 | 1472 | `			if( pVar ){` |
|     5 | 1473 | `				ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj, (sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|     5 | 1474 | `				if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|     5 | 1475 | `					pThisVal = pSlot;` |
|     2 | 1476 | `				}` |
|     2 | 1477 | `			}` |
|    17 | 1478 | `			if( pThisVal == 0 && pExec->pFrame->pThis ){` |
|     - | 1479 | `				ph7_value sThis;` |
|   ! 0 | 1480 | `				ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 1481 | `				PH7_MemObjInit(pVm, &sThis);` |
|   ! 0 | 1482 | `				pExec->pFrame->pThis->iRef++;` |
|   ! 0 | 1483 | `				sThis.x.pOther = pExec->pFrame->pThis;` |
|   ! 0 | 1484 | `				MemObjSetType(&sThis, MEMOBJ_OBJ);` |
|   ! 0 | 1485 | `				if( pKey ){` |
|   ! 0 | 1486 | `					ph7_value_string(pKey, "this", 4);` |
|   ! 0 | 1487 | `					ph7_array_add_elem(pInfo, pKey, &sThis); /* copies (takes its own ref) */` |
|   ! 0 | 1488 | `				}` |
|   ! 0 | 1489 | `				PH7_MemObjRelease(&sThis);` |
|   ! 0 | 1490 | `				pThisVal = (ph7_value *)1; /* handled */` |
|   ! 0 | 1491 | `			}` |
|     8 | 1492 | `		}` |
|    17 | 1493 | `		if( pThisVal == 0 ){` |
|    13 | 1494 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|    11 | 1495 | `		}else if( pThisVal != (ph7_value *)1 ){` |
|     5 | 1496 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|     5 | 1497 | `			if( pKey ){` |
|     5 | 1498 | `				ph7_value_string(pKey, "this", 4);` |
|     5 | 1499 | `				ph7_array_add_elem(pInfo, pKey, pThisVal);` |
|     2 | 1500 | `			}` |
|     2 | 1501 | `		}` |
|     - | 1502 | `	}` |
|    17 | 1503 | `	ph7_result_value(pCtx, pInfo);` |
|    17 | 1504 | `	return PH7_OK;` |
|     9 | 1505 | `}` |
|     - | 1506 | `/*` |
|     - | 1507 | ` * Generator __reflect_gen_exec(Generator $g)` |
|     - | 1508 | `` * Follow `yield from` delegation to the innermost executing generator`` |
|     - | 1509 | ` * (PHP's ReflectionGenerator::getExecutingGenerator).` |
|     - | 1510 | ` */` |
|     4 | 1511 | `static int vm_builtin_reflect_gen_exec(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1512 | `{` |
|     5 | 1513 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1514 | `	ph7_generator *pGen;` |
|     - | 1515 | `	ph7_value *pCur;` |
|     5 | 1516 | `	int iDepth = 0;` |
|     5 | 1517 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 ){` |
|   ! 0 | 1518 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1519 | `		return PH7_OK;` |
|     - | 1520 | `	}` |
|     5 | 1521 | `	pCur = apArg[0];` |
|     9 | 1522 | `	while( pGen && pGen->pCtx && pGen->pCtx->iDelegateState == 3` |
|     8 | 1523 | `	 && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|     3 | 1524 | `		ph7_generator *pInner = ReflectGeneratorCtx(pVm, &pGen->pCtx->sDelegate);` |
|     3 | 1525 | `		if( pInner == 0 ){` |
|   ! 0 | 1526 | `			break;` |
|     - | 1527 | `		}` |
|     3 | 1528 | `		pCur = &pGen->pCtx->sDelegate;` |
|     3 | 1529 | `		pGen = pInner;` |
|     3 | 1530 | `		iDepth++;` |
|     1 | 1531 | `	}` |
|     5 | 1532 | `	return ReflectResultExistingObject(pCtx, (ph7_class_instance *)pCur->x.pOther);` |
|     3 | 1533 | `}` |
|     - | 1534 | `/*` |
|     - | 1535 | ` * array\|null __reflect_const_info(string $name)` |
|     - | 1536 | ` * Global-constant descriptor: {value}. Null when undefined. File/origin` |
|     - | 1537 | ` * metadata arrives with the C5 constant-metadata work.` |
|     - | 1538 | ` */` |
|    36 | 1539 | `static int vm_builtin_reflect_const_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1540 | `{` |
|    37 | 1541 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1542 | `	SyHashEntry *pEntry;` |
|     - | 1543 | `	ph7_constant *pCons;` |
|     - | 1544 | `	ph7_value *pInfo;` |
|     - | 1545 | `	ph7_value sValue;` |
|     - | 1546 | `	const char *zName;` |
|     - | 1547 | `	int nLen;` |
|    37 | 1548 | `	if( nArg < 1 ){` |
|   ! 0 | 1549 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1550 | `		return PH7_OK;` |
|     - | 1551 | `	}` |
|    37 | 1552 | `	zName = ph7_value_to_string(apArg[0], &nLen);` |
|    37 | 1553 | `	pEntry = nLen > 0 ? SyHashGet(&pVm->hConstant, (const void *)zName, (sxu32)nLen) : 0;` |
|    37 | 1554 | `	if( pEntry == 0 ){` |
|     3 | 1555 | `		ph7_result_null(pCtx);` |
|     3 | 1556 | `		return PH7_OK;` |
|     - | 1557 | `	}` |
|    35 | 1558 | `	pCons = (ph7_constant *)pEntry->pUserData;` |
|    35 | 1559 | `	pInfo = ph7_context_new_array(pCtx);` |
|    35 | 1560 | `	if( pInfo == 0 ){` |
|   ! 0 | 1561 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1562 | `		return PH7_OK;` |
|     - | 1563 | `	}` |
|    35 | 1564 | `	PH7_MemObjInit(pVm, &sValue);` |
|    35 | 1565 | `	if( pCons->xExpand ){` |
|    35 | 1566 | `		pCons->xExpand(&sValue, pCons->pUserData);` |
|    17 | 1567 | `	}` |
|     - | 1568 | `	{` |
|    35 | 1569 | `		ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|    35 | 1570 | `		if( pKey ){` |
|    35 | 1571 | `			ph7_value_string(pKey, "value", 5);` |
|    35 | 1572 | `			ph7_array_add_elem(pInfo, pKey, &sValue);` |
|    17 | 1573 | `		}` |
|     - | 1574 | `	}` |
|    35 | 1575 | `	PH7_MemObjRelease(&sValue);` |
|    35 | 1576 | `	ReflectMapAddBool(pCtx, pInfo, "internal", pCons->bUserDefined == 0);` |
|    35 | 1577 | `	if( SyStringLength(&pCons->sFile) > 0 ){` |
|    21 | 1578 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));` |
|    11 | 1579 | `	}else{` |
|    15 | 1580 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1581 | `	}` |
|    35 | 1582 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pCons->nLine);` |
|    35 | 1583 | `	ph7_result_value(pCtx, pInfo);` |
|    35 | 1584 | `	return PH7_OK;` |
|    19 | 1585 | `}` |
|     - | 1586 | `/*` |
|     - | 1587 | ` * int\|null __reflect_ref_id(array $arr, int\|string $key)` |
|     - | 1588 | ` * The element's slot index when the element is a reference (its slot has` |
|     - | 1589 | ` * a reference-table record with at least two links), null otherwise.` |
|     - | 1590 | ` */` |
|     6 | 1591 | `static int vm_builtin_reflect_ref_id(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1592 | `{` |
|     - | 1593 | `	ph7_hashmap *pMap;` |
|     7 | 1594 | `	ph7_hashmap_node *pNode = 0;` |
|     7 | 1595 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|   ! 0 | 1596 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1597 | `		return PH7_OK;` |
|     - | 1598 | `	}` |
|     7 | 1599 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 | 1600 | `	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK \|\| pNode == 0 ){` |
|   ! 0 | 1601 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1602 | `		return PH7_OK;` |
|     - | 1603 | `	}` |
|     7 | 1604 | `	if( PH7_VmSlotRefCount(pCtx->pVm, pNode->nValIdx) < 2 ){` |
|     3 | 1605 | `		ph7_result_null(pCtx);` |
|     3 | 1606 | `		return PH7_OK;` |
|     - | 1607 | `	}` |
|     5 | 1608 | `	ph7_result_int64(pCtx, (sxi64)pNode->nValIdx);` |
|     5 | 1609 | `	return PH7_OK;` |
|     4 | 1610 | `}` |
|     - | 1611 | `/*` |
|     - | 1612 | ` * array\|null __reflect_attr_args(string $kind, mixed $target, ?string $member,` |
|     - | 1613 | ` *                                int $paramIdx, int $attrIdx)` |
|     - | 1614 | ` * Evaluate the recorded argument expressions of one declared attribute:` |
|     - | 1615 | ` * kind 'class' (target = class), 'attr' (class + property/constant name),` |
|     - | 1616 | ` * 'method' (class + method), 'fn' (function name or Closure), 'param'` |
|     - | 1617 | ` * (function spec + parameter index). Named arguments become string keys.` |
|     - | 1618 | ` */` |
|    36 | 1619 | `static int vm_builtin_reflect_attr_args(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1620 | `{` |
|    37 | 1621 | `	ph7_vm *pVm = pCtx->pVm;` |
|    37 | 1622 | `	SySet *pAttrs = 0;` |
|     - | 1623 | `	ph7_attribute *pAttrRec;` |
|     - | 1624 | `	ph7_value *pOut;` |
|     - | 1625 | `	const char *zKind;` |
|     - | 1626 | `	int nKind;` |
|     - | 1627 | `	sxu32 nAttrIdx, n;` |
|    37 | 1628 | `	if( nArg < 5 ){` |
|   ! 0 | 1629 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1630 | `		return PH7_OK;` |
|     - | 1631 | `	}` |
|    37 | 1632 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|    37 | 1633 | `	nAttrIdx = (sxu32)ph7_value_to_int(apArg[4]);` |
|    49 | 1634 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|    25 | 1635 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|    25 | 1636 | `		if( pClass ){ pAttrs = &pClass->aAttrs; }` |
|    27 | 1637 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|     5 | 1638 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|     5 | 1639 | `		ph7_class_attr *pMember = pClass ? ReflectFetchAttr(pClass, apArg[2]) : 0;` |
|     5 | 1640 | `		if( pMember ){ pAttrs = &pMember->aAttrs; }` |
|    12 | 1641 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|     3 | 1642 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1643 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|    10 | 1644 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|     5 | 1645 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], 0, 0, 0, 0, 0);` |
|     5 | 1646 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|     5 | 1647 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|     3 | 1648 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1649 | `		ph7_vm_func_arg *pParam = pFunc` |
|     2 | 1650 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|     3 | 1651 | `		if( pParam ){ pAttrs = &pParam->aAttrs; }` |
|     1 | 1652 | `	}` |
|    36 | 1653 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|    37 | 1654 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 | 1655 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1656 | `		return PH7_OK;` |
|     - | 1657 | `	}` |
|    71 | 1658 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|    35 | 1659 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|     - | 1660 | `		ph7_value sValue;` |
|    35 | 1661 | `		PH7_MemObjInit(pVm, &sValue);` |
|    35 | 1662 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|    35 | 1663 | `			VmLocalExec(pVm, &pArgRec->aByteCode, &sValue, FALSE);` |
|    17 | 1664 | `		}` |
|    35 | 1665 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|     5 | 1666 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|     3 | 1667 | `		}else{` |
|    31 | 1668 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|     - | 1669 | `		}` |
|    35 | 1670 | `		PH7_MemObjRelease(&sValue);` |
|    18 | 1671 | `	}` |
|    37 | 1672 | `	ph7_result_value(pCtx, pOut);` |
|    37 | 1673 | `	return PH7_OK;` |
|    19 | 1674 | `}` |
|     - | 1675 | `/*` |
|     - | 1676 | ` * The Reflection classes, in PHP. Chunk 1: exceptions, Reflector,` |
|     - | 1677 | ` * Reflection, ReflectionClass, ReflectionObject (plus get_debug_type,` |
|     - | 1678 | ` * which the TypeError messages need and PHP 8.0 ships natively).` |
|     - | 1679 | ` */` |
|     - | 1680 | `static const char zReflectLib1[] =` |
|     - | 1681 | `"function get_debug_type($value){"` |
|     - | 1682 | `" if(is_object($value)){ return get_class($value); }"` |
|     - | 1683 | `" if(is_bool($value)){ return 'bool'; }"` |
|     - | 1684 | `" if(is_int($value)){ return 'int'; }"` |
|     - | 1685 | `" if(is_float($value)){ return 'float'; }"` |
|     - | 1686 | `" if(is_string($value)){ return 'string'; }"` |
|     - | 1687 | `" if(is_array($value)){ return 'array'; }"` |
|     - | 1688 | `" if($value === null){ return 'null'; }"` |
|     - | 1689 | `" return gettype($value);"` |
|     - | 1690 | `"}"` |
|     - | 1691 | `"interface Reflector extends Stringable {}"` |
|     - | 1692 | `"class ReflectionException extends Exception {}"` |
|     - | 1693 | `"class Reflection {"` |
|     - | 1694 | `" public static function getModifierNames($modifiers){"` |
|     - | 1695 | `"  $names = array();"` |
|     - | 1696 | `"  if($modifiers & 64){ $names[] = 'abstract'; }"` |
|     - | 1697 | `"  if($modifiers & 32){ $names[] = 'final'; }"` |
|     - | 1698 | `"  if($modifiers & 1){ $names[] = 'public'; }"` |
|     - | 1699 | `"  if($modifiers & 2){ $names[] = 'protected'; }"` |
|     - | 1700 | `"  if($modifiers & 4){ $names[] = 'private'; }"` |
|     - | 1701 | `"  if($modifiers & 16){ $names[] = 'static'; }"` |
|     - | 1702 | `"  if($modifiers & 128){ $names[] = 'readonly'; }"` |
|     - | 1703 | `"  return $names;"` |
|     - | 1704 | `" }"` |
|     - | 1705 | `"}"` |
|     - | 1706 | `"class ReflectionClass implements Reflector {"` |
|     - | 1707 | `" const IS_IMPLICIT_ABSTRACT = 16;"` |
|     - | 1708 | `" const IS_EXPLICIT_ABSTRACT = 64;"` |
|     - | 1709 | `" const IS_FINAL = 32;"` |
|     - | 1710 | `" const IS_READONLY = 65536;"` |
|     - | 1711 | `" const SKIP_INITIALIZATION_ON_SERIALIZE = 8;"` |
|     - | 1712 | `" const SKIP_DESTRUCTOR = 16;"` |
|     - | 1713 | `" public $name;"` |
|     - | 1714 | `" protected $__obj = null;"` |
|     - | 1715 | `" public function __construct($objectOrClass){"` |
|     - | 1716 | `"  if(!is_object($objectOrClass) && !is_string($objectOrClass)){"` |
|     - | 1717 | `"   if(is_int($objectOrClass) \|\| is_float($objectOrClass) \|\| is_bool($objectOrClass)){"` |
|     - | 1718 | `"    $objectOrClass = (string)$objectOrClass;"` |
|     - | 1719 | `"   }else{"` |
|     - | 1720 | `"    throw new TypeError('ReflectionClass::__construct(): Argument #1 ($objectOrClass) must be of type object\|string, '.get_debug_type($objectOrClass).' given');"` |
|     - | 1721 | `"   }"` |
|     - | 1722 | `"  }"` |
|     - | 1723 | `"  $info = __reflect_class_info($objectOrClass);"` |
|     - | 1724 | `"  if($info === null){"` |
|     - | 1725 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 1726 | `"  }"` |
|     - | 1727 | `"  $this->name = $info['name'];"` |
|     - | 1728 | `" }"` |
|     - | 1729 | `" protected function __rinfo(){ return __reflect_class_info($this->name); }"` |
|     - | 1730 | `" public function getName(){ return $this->name; }"` |
|     - | 1731 | `" public function getShortName(){"` |
|     - | 1732 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1733 | `"  if($p === false){ return $this->name; }"` |
|     - | 1734 | `"  return substr($this->name,$p+1);"` |
|     - | 1735 | `" }"` |
|     - | 1736 | `" public function getNamespaceName(){"` |
|     - | 1737 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1738 | `"  if($p === false){ return ''; }"` |
|     - | 1739 | `"  return substr($this->name,0,$p);"` |
|     - | 1740 | `" }"` |
|     - | 1741 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 1742 | `" public function isInternal(){ $i = $this->__rinfo(); return $i['internal']; }"` |
|     - | 1743 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 1744 | `" public function isInterface(){ $i = $this->__rinfo(); return $i['interface']; }"` |
|     - | 1745 | `" public function isTrait(){ $i = $this->__rinfo(); return $i['trait']; }"` |
|     - | 1746 | `" public function isAbstract(){ $i = $this->__rinfo(); return $i['abstract']; }"` |
|     - | 1747 | `" public function isFinal(){ $i = $this->__rinfo(); return $i['final']; }"` |
|     - | 1748 | `" public function isReadOnly(){ $i = $this->__rinfo(); return $i['readonly']; }"` |
|     - | 1749 | `" public function isEnum(){ $i = $this->__rinfo(); return $i['enum']; }"` |
|     - | 1750 | `" public function isAnonymous(){ return strpos($this->name,'class@anonymous') === 0; }"` |
|     - | 1751 | `" public function getModifiers(){"` |
|     - | 1752 | `"  $i = $this->__rinfo();"` |
|     - | 1753 | `"  $m = 0;"` |
|     - | 1754 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 1755 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 1756 | `"  if($i['readonly']){ $m \|= 65536; }"` |
|     - | 1757 | `"  return $m;"` |
|     - | 1758 | `" }"` |
|     - | 1759 | `" public function getParentClass(){"` |
|     - | 1760 | `"  $i = $this->__rinfo();"` |
|     - | 1761 | `"  if($i['parent'] === null){ return false; }"` |
|     - | 1762 | `"  return new ReflectionClass($i['parent']);"` |
|     - | 1763 | `" }"` |
|     - | 1764 | `" public function getInterfaceNames(){ $i = $this->__rinfo(); return $i['interfaces']; }"` |
|     - | 1765 | `" public function getInterfaces(){"` |
|     - | 1766 | `"  $i = $this->__rinfo();"` |
|     - | 1767 | `"  $out = array();"` |
|     - | 1768 | `"  foreach($i['interfaces'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1769 | `"  return $out;"` |
|     - | 1770 | `" }"` |
|     - | 1771 | `" public function getTraitNames(){ $i = $this->__rinfo(); return $i['traits']; }"` |
|     - | 1772 | `" public function getTraits(){"` |
|     - | 1773 | `"  $i = $this->__rinfo();"` |
|     - | 1774 | `"  $out = array();"` |
|     - | 1775 | `"  foreach($i['traits'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1776 | `"  return $out;"` |
|     - | 1777 | `" }"` |
|     - | 1778 | `" public function getTraitAliases(){ return array(); }"` |
|     - | 1779 | `" public function implementsInterface($interface){"` |
|     - | 1780 | `"  if($interface instanceof ReflectionClass){ $interface = $interface->name; }"` |
|     - | 1781 | `"  $target = __reflect_class_info($interface);"` |
|     - | 1782 | `"  if($target === null){"` |
|     - | 1783 | `"   throw new ReflectionException('Interface \"'.$interface.'\" does not exist');"` |
|     - | 1784 | `"  }"` |
|     - | 1785 | `"  if(!$target['interface']){"` |
|     - | 1786 | `"   throw new ReflectionException($target['name'].' is not an interface');"` |
|     - | 1787 | `"  }"` |
|     - | 1788 | `"  $name = $target['name'];"` |
|     - | 1789 | `"  if($this->name === $name){ return true; }"` |
|     - | 1790 | `"  $i = $this->__rinfo();"` |
|     - | 1791 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1792 | `"  return false;"` |
|     - | 1793 | `" }"` |
|     - | 1794 | `" public function isSubclassOf($class){"` |
|     - | 1795 | `"  if($class instanceof ReflectionClass){ $class = $class->name; }"` |
|     - | 1796 | `"  $target = __reflect_class_info($class);"` |
|     - | 1797 | `"  if($target === null){"` |
|     - | 1798 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 1799 | `"  }"` |
|     - | 1800 | `"  $name = $target['name'];"` |
|     - | 1801 | `"  if($name === $this->name){ return false; }"` |
|     - | 1802 | `"  $i = $this->__rinfo();"` |
|     - | 1803 | `"  $p = $i['parent'];"` |
|     - | 1804 | `"  while($p !== null){"` |
|     - | 1805 | `"   if($p === $name){ return true; }"` |
|     - | 1806 | `"   $pi = __reflect_class_info($p);"` |
|     - | 1807 | `"   $p = $pi['parent'];"` |
|     - | 1808 | `"  }"` |
|     - | 1809 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1810 | `"  return false;"` |
|     - | 1811 | `" }"` |
|     - | 1812 | `" public function isInstance($object){"` |
|     - | 1813 | `"  if(!is_object($object)){"` |
|     - | 1814 | `"   throw new TypeError('ReflectionClass::isInstance(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 1815 | `"  }"` |
|     - | 1816 | `"  return is_a($object,$this->name);"` |
|     - | 1817 | `" }"` |
|     - | 1818 | `" public function hasMethod($name){"` |
|     - | 1819 | `"  $i = $this->__rinfo();"` |
|     - | 1820 | `"  $l = strtolower($name);"` |
|     - | 1821 | `"  foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ return true; } }"` |
|     - | 1822 | `"  return false;"` |
|     - | 1823 | `" }"` |
|     - | 1824 | `" public function hasProperty($name){"` |
|     - | 1825 | `"  $i = $this->__rinfo();"` |
|     - | 1826 | `"  if(isset($i['props'][$name])){ return true; }"` |
|     - | 1827 | `"  if($this->__obj !== null){ return (__reflect_prop_state($this->__obj, $name) & 1) !== 0; }"` |
|     - | 1828 | `"  return false;"` |
|     - | 1829 | `" }"` |
|     - | 1830 | `" public function hasConstant($name){ $i = $this->__rinfo(); return isset($i['consts'][$name]); }"` |
|     - | 1831 | `" public function getConstant($name){"` |
|     - | 1832 | `"  $i = $this->__rinfo();"` |
|     - | 1833 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 1834 | `"  return __reflect_const_value($this->name,$name);"` |
|     - | 1835 | `" }"` |
|     - | 1836 | `" public function getConstants($filter = null){"` |
|     - | 1837 | `"  $i = $this->__rinfo();"` |
|     - | 1838 | `"  $out = array();"` |
|     - | 1839 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 1840 | `"   if($filter !== null){"` |
|     - | 1841 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 1842 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 1843 | `"   }"` |
|     - | 1844 | `"   $out[$k] = __reflect_const_value($this->name,$k);"` |
|     - | 1845 | `"  }"` |
|     - | 1846 | `"  return $out;"` |
|     - | 1847 | `" }"` |
|     - | 1848 | `" public function getStartLine(){"` |
|     - | 1849 | `"  $i = $this->__rinfo();"` |
|     - | 1850 | `"  if($i['internal']){ return false; }"` |
|     - | 1851 | `"  return $i['line'];"` |
|     - | 1852 | `" }"` |
|     - | 1853 | `" public function getEndLine(){"` |
|     - | 1854 | `"  $i = $this->__rinfo();"` |
|     - | 1855 | `"  if($i['internal']){ return false; }"` |
|     - | 1856 | `"  return $i['endline'];"` |
|     - | 1857 | `" }"` |
|     - | 1858 | `" public function getFileName(){ $i = $this->__rinfo(); return $i['file']; }"` |
|     - | 1859 | `" public function getDocComment(){ $i = $this->__rinfo(); return $i['doc']; }"` |
|     - | 1860 | `" public function isInstantiable(){"` |
|     - | 1861 | `"  $i = $this->__rinfo();"` |
|     - | 1862 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract'] \|\| $i['enum']){ return false; }"` |
|     - | 1863 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){ return false; }"` |
|     - | 1864 | `"  return true;"` |
|     - | 1865 | `" }"` |
|     - | 1866 | `" public function isCloneable(){"` |
|     - | 1867 | `"  $i = $this->__rinfo();"` |
|     - | 1868 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1869 | `"  if($i['clonevis'] !== 0 && $i['clonevis'] !== 1){ return false; }"` |
|     - | 1870 | `"  return true;"` |
|     - | 1871 | `" }"` |
|     - | 1872 | `" public function isIterable(){"` |
|     - | 1873 | `"  $i = $this->__rinfo();"` |
|     - | 1874 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1875 | `"  return $i['iterable'];"` |
|     - | 1876 | `" }"` |
|     - | 1877 | `" public function isIterateable(){ return $this->isIterable(); }"` |
|     - | 1878 | `" public function newInstance(...$args){ return $this->__rnew($args); }"` |
|     - | 1879 | `" public function newInstanceArgs(array $args = array()){ return $this->__rnew($args); }"` |
|     - | 1880 | `" protected function __rnew($args){"` |
|     - | 1881 | `"  $i = $this->__rinfo();"` |
|     - | 1882 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1883 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){"` |
|     - | 1884 | `"   throw new ReflectionException('Access to non-public constructor of class '.$this->name);"` |
|     - | 1885 | `"  }"` |
|     - | 1886 | `"  if($i['ctorvis'] === 0 && count($args) > 0){"` |
|     - | 1887 | `"   throw new ReflectionException('Class '.$this->name.' does not have a constructor, so you cannot pass any constructor arguments');"` |
|     - | 1888 | `"  }"` |
|     - | 1889 | `"  return __reflect_new_instance($this->name,$args);"` |
|     - | 1890 | `" }"` |
|     - | 1891 | `" protected function __rcheckInstantiable($i){"` |
|     - | 1892 | `"  if($i['interface']){ throw new Error('Cannot instantiate interface '.$this->name); }"` |
|     - | 1893 | `"  if($i['trait']){ throw new Error('Cannot instantiate trait '.$this->name); }"` |
|     - | 1894 | `"  if($i['abstract']){ throw new Error('Cannot instantiate abstract class '.$this->name); }"` |
|     - | 1895 | `" }"` |
|     - | 1896 | `" public function newInstanceWithoutConstructor(){"` |
|     - | 1897 | `"  $i = $this->__rinfo();"` |
|     - | 1898 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1899 | `"  return __reflect_new_no_ctor($this->name);"` |
|     - | 1900 | `" }"` |
|     - | 1901 | `" public function getStaticProperties(){"` |
|     - | 1902 | `"  $i = $this->__rinfo();"` |
|     - | 1903 | `"  $out = array();"` |
|     - | 1904 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1905 | `"   if($p['static']){ $out[$k] = __reflect_static_value($this->name,$k); }"` |
|     - | 1906 | `"  }"` |
|     - | 1907 | `"  return $out;"` |
|     - | 1908 | `" }"` |
|     - | 1909 | `" public function getStaticPropertyValue($name, ...$def){"` |
|     - | 1910 | `"  $i = $this->__rinfo();"` |
|     - | 1911 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 1912 | `"   if(count($def) > 0){ return $def[0]; }"` |
|     - | 1913 | `"   throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 1914 | `"  }"` |
|     - | 1915 | `"  return __reflect_static_value($this->name,$name);"` |
|     - | 1916 | `" }"` |
|     - | 1917 | `" public function setStaticPropertyValue($name,$value){"` |
|     - | 1918 | `"  $i = $this->__rinfo();"` |
|     - | 1919 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 1920 | `"   throw new ReflectionException('Class '.$this->name.' does not have a property named '.$name);"` |
|     - | 1921 | `"  }"` |
|     - | 1922 | `"  __reflect_static_set($this->name,$name,$value);"` |
|     - | 1923 | `" }"` |
|     - | 1924 | `" public function getDefaultProperties(){"` |
|     - | 1925 | `"  $i = $this->__rinfo();"` |
|     - | 1926 | `"  $out = array();"` |
|     - | 1927 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1928 | `"   if($p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 1929 | `"  }"` |
|     - | 1930 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1931 | `"   if(!$p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 1932 | `"  }"` |
|     - | 1933 | `"  return $out;"` |
|     - | 1934 | `" }"` |
|     - | 1935 | `" public function getProperty($name){"` |
|     - | 1936 | `"  $i = $this->__rinfo();"` |
|     - | 1937 | `"  if(isset($i['props'][$name])){"` |
|     - | 1938 | `"   return new ReflectionProperty($this->name, $name);"` |
|     - | 1939 | `"  }"` |
|     - | 1940 | `"  if($this->__obj !== null && (__reflect_prop_state($this->__obj, $name) & 1)){"` |
|     - | 1941 | `"   return new ReflectionProperty($this->__obj, $name);"` |
|     - | 1942 | `"  }"` |
|     - | 1943 | `"  throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 1944 | `" }"` |
|     - | 1945 | `" public function getProperties($filter = null){"` |
|     - | 1946 | `"  $i = $this->__rinfo();"` |
|     - | 1947 | `"  $out = array();"` |
|     - | 1948 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1949 | `"   if($filter !== null){"` |
|     - | 1950 | `"    $m = ($p['vis'] === 1 ? 1 : ($p['vis'] === 2 ? 2 : 4));"` |
|     - | 1951 | `"    if($p['static']){ $m \|= 16; }"` |
|     - | 1952 | `"    if($p['readonly']){ $m \|= 128; }"` |
|     - | 1953 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 1954 | `"   }"` |
|     - | 1955 | `"   $out[] = new ReflectionProperty($this->name, $k);"` |
|     - | 1956 | `"  }"` |
|     - | 1957 | `"  if($this->__obj !== null){"` |
|     - | 1958 | `"   foreach(__reflect_dyn_props($this->__obj) as $k){"` |
|     - | 1959 | `"    if(isset($i['props'][$k])){ continue; }"` |
|     - | 1960 | `"    if($filter !== null && ($filter & 1) === 0){ continue; }"` |
|     - | 1961 | `"    $out[] = new ReflectionProperty($this->__obj, $k);"` |
|     - | 1962 | `"   }"` |
|     - | 1963 | `"  }"` |
|     - | 1964 | `"  return $out;"` |
|     - | 1965 | `" }"` |
|     - | 1966 | `" public function getMethod($name){"` |
|     - | 1967 | `"  $i = $this->__rinfo();"` |
|     - | 1968 | `"  $found = null;"` |
|     - | 1969 | `"  if(isset($i['methods'][$name])){"` |
|     - | 1970 | `"   $found = $name;"` |
|     - | 1971 | `"  }else{"` |
|     - | 1972 | `"   $l = strtolower($name);"` |
|     - | 1973 | `"   foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ $found = $k; break; } }"` |
|     - | 1974 | `"  }"` |
|     - | 1975 | `"  if($found === null){"` |
|     - | 1976 | `"   throw new ReflectionException('Method '.$this->name.'::'.$name.'() does not exist');"` |
|     - | 1977 | `"  }"` |
|     - | 1978 | `"  return new ReflectionMethod($this->name, $found);"` |
|     - | 1979 | `" }"` |
|     - | 1980 | `" public function getMethods($filter = null){"` |
|     - | 1981 | `"  $i = $this->__rinfo();"` |
|     - | 1982 | `"  $out = array();"` |
|     - | 1983 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 1984 | `"   if($filter !== null){"` |
|     - | 1985 | `"    $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 1986 | `"    if($m['static']){ $mod \|= 16; }"` |
|     - | 1987 | `"    if($m['abstract']){ $mod \|= 64; }"` |
|     - | 1988 | `"    if($m['final']){ $mod \|= 32; }"` |
|     - | 1989 | `"    if(($mod & $filter) === 0){ continue; }"` |
|     - | 1990 | `"   }"` |
|     - | 1991 | `"   $out[] = new ReflectionMethod($this->name, $k);"` |
|     - | 1992 | `"  }"` |
|     - | 1993 | `"  return $out;"` |
|     - | 1994 | `" }"` |
|     - | 1995 | `" public function getConstructor(){"` |
|     - | 1996 | `"  $i = $this->__rinfo();"` |
|     - | 1997 | `"  if(isset($i['methods']['__construct'])){"` |
|     - | 1998 | `"   return new ReflectionMethod($this->name, '__construct');"` |
|     - | 1999 | `"  }"` |
|     - | 2000 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 2001 | `"   if(strtolower($k) === '__construct'){ return new ReflectionMethod($this->name, $k); }"` |
|     - | 2002 | `"  }"` |
|     - | 2003 | `"  if($i['ctorvis'] !== 0 && isset($i['methods'][$this->name])){"` |
|     - | 2004 | `"   return new ReflectionMethod($this->name, $this->name);"` |
|     - | 2005 | `"  }"` |
|     - | 2006 | `"  return null;"` |
|     - | 2007 | `" }"` |
|     - | 2008 | `" public function getReflectionConstant($name){"` |
|     - | 2009 | `"  $i = $this->__rinfo();"` |
|     - | 2010 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 2011 | `"  return new ReflectionClassConstant($this->name, $name);"` |
|     - | 2012 | `" }"` |
|     - | 2013 | `" public function getReflectionConstants($filter = null){"` |
|     - | 2014 | `"  $i = $this->__rinfo();"` |
|     - | 2015 | `"  $out = array();"` |
|     - | 2016 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 2017 | `"   if($filter !== null){"` |
|     - | 2018 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 2019 | `"    if($c['final']){ $m \|= 32; }"` |
|     - | 2020 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 2021 | `"   }"` |
|     - | 2022 | `"   $out[] = new ReflectionClassConstant($this->name, $k);"` |
|     - | 2023 | `"  }"` |
|     - | 2024 | `"  return $out;"` |
|     - | 2025 | `" }"` |
|     - | 2026 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2027 | `"  $i = $this->__rinfo();"` |
|     - | 2028 | `"  return __reflect_build_attrs($i['attrs'], array('class', $this->name, null, 0), 1, $name, $flags);"` |
|     - | 2029 | `" }"` |
|     - | 2030 | `" public function getExtensionName(){ $i = $this->__rinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2031 | `" public function getExtension(){ $i = $this->__rinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2032 | `" public function newLazyGhost($initializer, $options = 0){"` |
|     - | 2033 | `"  throw new Error('ReflectionClass::newLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2034 | `" }"` |
|     - | 2035 | `" public function newLazyProxy($factory, $options = 0){"` |
|     - | 2036 | `"  throw new Error('ReflectionClass::newLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2037 | `" }"` |
|     - | 2038 | `" public function resetAsLazyGhost($object, $initializer, $options = 0){"` |
|     - | 2039 | `"  throw new Error('ReflectionClass::resetAsLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2040 | `" }"` |
|     - | 2041 | `" public function resetAsLazyProxy($object, $factory, $options = 0){"` |
|     - | 2042 | `"  throw new Error('ReflectionClass::resetAsLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2043 | `" }"` |
|     - | 2044 | `" public function getLazyInitializer($object){ return null; }"` |
|     - | 2045 | `" public function initializeLazyObject($object){ return $object; }"` |
|     - | 2046 | `" public function markLazyObjectAsInitialized($object){ return $object; }"` |
|     - | 2047 | `" public function isUninitializedLazyObject($object){ return false; }"` |
|     - | 2048 | `" public function __toString(){ return __reflect_export_class($this); }"` |
|     - | 2049 | `"}"` |
|     - | 2050 | `"class ReflectionObject extends ReflectionClass {"` |
|     - | 2051 | `" public function __construct($object){"` |
|     - | 2052 | `"  if(!is_object($object)){"` |
|     - | 2053 | `"   throw new TypeError('ReflectionObject::__construct(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 2054 | `"  }"` |
|     - | 2055 | `"  parent::__construct($object);"` |
|     - | 2056 | `"  $this->__obj = $object;"` |
|     - | 2057 | `" }"` |
|     - | 2058 | `"}"` |
|     - | 2059 | `;` |
|     - | 2060 | `/*` |
|     - | 2061 | ` * Chunk 2: ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,` |
|     - | 2062 | ` * ReflectionParameter.` |
|     - | 2063 | ` */` |
|     - | 2064 | `static const char zReflectLib2[] =` |
|     - | 2065 | `"abstract class ReflectionFunctionAbstract implements Reflector {"` |
|     - | 2066 | `" public $name;"` |
|     - | 2067 | `" protected $__cl = null;"` |
|     - | 2068 | `" protected function __rfinfo(){"` |
|     - | 2069 | `"  if($this->__cl !== null){ return __reflect_sig_fixup(__reflect_func_info($this->__cl)); }"` |
|     - | 2070 | `"  return __reflect_sig_fixup(__reflect_func_info($this->name));"` |
|     - | 2071 | `" }"` |
|     - | 2072 | `" protected function __rftarget(){ return $this->__cl !== null ? $this->__cl : $this->name; }"` |
|     - | 2073 | `" protected function __rpspec(){ return $this->__rftarget(); }"` |
|     - | 2074 | `" public function getName(){ return $this->name; }"` |
|     - | 2075 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 2076 | `" public function getNamespaceName(){"` |
|     - | 2077 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2078 | `"  if($p === false){ return ''; }"` |
|     - | 2079 | `"  return substr($this->name,0,$p);"` |
|     - | 2080 | `" }"` |
|     - | 2081 | `" public function getShortName(){"` |
|     - | 2082 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2083 | `"  if($p === false){ return $this->name; }"` |
|     - | 2084 | `"  return substr($this->name,$p+1);"` |
|     - | 2085 | `" }"` |
|     - | 2086 | `" public function isClosure(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2087 | `" public function isGenerator(){ $i = $this->__rfinfo(); return $i['generator']; }"` |
|     - | 2088 | `" public function isVariadic(){ $i = $this->__rfinfo(); return $i['variadic']; }"` |
|     - | 2089 | `" public function returnsReference(){ $i = $this->__rfinfo(); return $i['byref']; }"` |
|     - | 2090 | `" public function isInternal(){ $i = $this->__rfinfo(); return $i['internal']; }"` |
|     - | 2091 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 2092 | `" public function isDeprecated(){ $i = $this->__rfinfo(); return __reflect_has_deprecated($i['attrs']); }"` |
|     - | 2093 | `" public function isStatic(){ return false; }"` |
|     - | 2094 | `" public function getFileName(){ $i = $this->__rfinfo(); return $i['file']; }"` |
|     - | 2095 | `" public function getStartLine(){"` |
|     - | 2096 | `"  $i = $this->__rfinfo();"` |
|     - | 2097 | `"  if($i['internal']){ return false; }"` |
|     - | 2098 | `"  return $i['line'];"` |
|     - | 2099 | `" }"` |
|     - | 2100 | `" public function getEndLine(){"` |
|     - | 2101 | `"  $i = $this->__rfinfo();"` |
|     - | 2102 | `"  if($i['internal']){ return false; }"` |
|     - | 2103 | `"  return $i['endline'];"` |
|     - | 2104 | `" }"` |
|     - | 2105 | `" public function getDocComment(){ $i = $this->__rfinfo(); return $i['doc']; }"` |
|     - | 2106 | `" public function hasReturnType(){ $i = $this->__rfinfo(); return $i['rettext'] !== null; }"` |
|     - | 2107 | `" public function getReturnType(){ $i = $this->__rfinfo(); return __reflect_make_type($i['rettext']); }"` |
|     - | 2108 | `" public function hasTentativeReturnType(){ return false; }"` |
|     - | 2109 | `" public function getTentativeReturnType(){ return null; }"` |
|     - | 2110 | `" public function getNumberOfParameters(){"` |
|     - | 2111 | `"  $i = $this->__rfinfo();"` |
|     - | 2112 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2113 | `"  return count($i['params']);"` |
|     - | 2114 | `" }"` |
|     - | 2115 | `" public function getNumberOfRequiredParameters(){"` |
|     - | 2116 | `"  $i = $this->__rfinfo();"` |
|     - | 2117 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2118 | `"  $req = 0;"` |
|     - | 2119 | `"  $n = count($i['params']);"` |
|     - | 2120 | `"  for($k = $n - 1; $k >= 0; $k--){"` |
|     - | 2121 | `"   $p = $i['params'][$k];"` |
|     - | 2122 | `"   if(!$p['variadic'] && !$p['hasdef']){ $req = $k + 1; break; }"` |
|     - | 2123 | `"  }"` |
|     - | 2124 | `"  return $req;"` |
|     - | 2125 | `" }"` |
|     - | 2126 | `" public function getParameters(){"` |
|     - | 2127 | `"  $i = $this->__rfinfo();"` |
|     - | 2128 | `"  $out = array();"` |
|     - | 2129 | `"  $spec = $this->__rpspec();"` |
|     - | 2130 | `"  foreach($i['params'] as $p){"` |
|     - | 2131 | `"   $out[] = new ReflectionParameter($spec, $p['pos']);"` |
|     - | 2132 | `"  }"` |
|     - | 2133 | `"  return $out;"` |
|     - | 2134 | `" }"` |
|     - | 2135 | `" public function getStaticVariables(){ $i = $this->__rfinfo(); return $i['statics']; }"` |
|     - | 2136 | `" public function getClosureThis(){"` |
|     - | 2137 | `"  $i = $this->__rfinfo();"` |
|     - | 2138 | `"  return isset($i['this']) ? $i['this'] : null;"` |
|     - | 2139 | `" }"` |
|     - | 2140 | `" public function getClosureScopeClass(){"` |
|     - | 2141 | `"  $i = $this->__rfinfo();"` |
|     - | 2142 | `"  if(isset($i['scope'])){ return new ReflectionClass($i['scope']); }"` |
|     - | 2143 | `"  if(isset($i['this'])){ return new ReflectionClass(get_class($i['this'])); }"` |
|     - | 2144 | `"  return null;"` |
|     - | 2145 | `" }"` |
|     - | 2146 | `" public function getClosureCalledClass(){ return $this->getClosureScopeClass(); }"` |
|     - | 2147 | `" public function getClosureUsedVariables(){"` |
|     - | 2148 | `"  $i = $this->__rfinfo();"` |
|     - | 2149 | `"  return isset($i['used']) ? $i['used'] : array();"` |
|     - | 2150 | `" }"` |
|     - | 2151 | `" public function getExtensionName(){ $i = $this->__rfinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2152 | `" public function getExtension(){ $i = $this->__rfinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2153 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2154 | `"  $i = $this->__rfinfo();"` |
|     - | 2155 | `"  if($this instanceof ReflectionMethod){"` |
|     - | 2156 | `"   $spec = array('method', $this->class, $this->name, 0);"` |
|     - | 2157 | `"   $target = 4;"` |
|     - | 2158 | `"  }else{"` |
|     - | 2159 | `"   $spec = array('fn', $this->__rftarget(), null, 0);"` |
|     - | 2160 | `"   $target = 2;"` |
|     - | 2161 | `"  }"` |
|     - | 2162 | `"  return __reflect_build_attrs($i['attrs'], $spec, $target, $name, $flags);"` |
|     - | 2163 | `" }"` |
|     - | 2164 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2165 | `"}"` |
|     - | 2166 | `"class ReflectionFunction extends ReflectionFunctionAbstract {"` |
|     - | 2167 | `" const IS_DEPRECATED = 2048;"` |
|     - | 2168 | `" public function __construct($function){"` |
|     - | 2169 | `"  if($function instanceof Closure){"` |
|     - | 2170 | `"   $this->__cl = $function;"` |
|     - | 2171 | `"   $i = $this->__rfinfo();"` |
|     - | 2172 | `"   if($i['closure']){"` |
|     - | 2173 | `"    $f = $i['file'] === false ? '' : $i['file'];"` |
|     - | 2174 | `"    $this->name = '{closure:'.$f.':'.$i['line'].'}';"` |
|     - | 2175 | `"   }else{"` |
|     - | 2176 | `"    $this->name = $i['name'];"` |
|     - | 2177 | `"   }"` |
|     - | 2178 | `"   return;"` |
|     - | 2179 | `"  }"` |
|     - | 2180 | `"  if(!is_string($function)){"` |
|     - | 2181 | `"   throw new TypeError('ReflectionFunction::__construct(): Argument #1 ($function) must be of type Closure\|string, '.get_debug_type($function).' given');"` |
|     - | 2182 | `"  }"` |
|     - | 2183 | `"  $i = __reflect_func_info($function);"` |
|     - | 2184 | `"  if($i === null){"` |
|     - | 2185 | `"   throw new ReflectionException('Function '.$function.'() does not exist');"` |
|     - | 2186 | `"  }"` |
|     - | 2187 | `"  if($i['closure']){"` |
|     - | 2188 | `"   $this->name = '{closure:'.($i['file'] === false ? '' : $i['file']).':'.$i['line'].'}';"` |
|     - | 2189 | `"   $this->__cl = __reflect_closure($function, null, null);"` |
|     - | 2190 | `"  }else{"` |
|     - | 2191 | `"   $this->name = $i['name'];"` |
|     - | 2192 | `"  }"` |
|     - | 2193 | `" }"` |
|     - | 2194 | `" public function invoke(...$args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2195 | `" public function invokeArgs(array $args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2196 | `" public function getClosure(){"` |
|     - | 2197 | `"  if($this->__cl !== null){ return $this->__cl; }"` |
|     - | 2198 | `"  return __reflect_closure($this->name, null, null);"` |
|     - | 2199 | `" }"` |
|     - | 2200 | `" public function isAnonymous(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2201 | `" public function isDisabled(){ return false; }"` |
|     - | 2202 | `"}"` |
|     - | 2203 | `"class ReflectionMethod extends ReflectionFunctionAbstract {"` |
|     - | 2204 | `" const IS_PUBLIC = 1;"` |
|     - | 2205 | `" const IS_PROTECTED = 2;"` |
|     - | 2206 | `" const IS_PRIVATE = 4;"` |
|     - | 2207 | `" const IS_STATIC = 16;"` |
|     - | 2208 | `" const IS_FINAL = 32;"` |
|     - | 2209 | `" const IS_ABSTRACT = 64;"` |
|     - | 2210 | `" public $class;"` |
|     - | 2211 | `" public function __construct($objectOrMethod, $method = null){"` |
|     - | 2212 | `"  if($method === null){"` |
|     - | 2213 | `"   if(!is_string($objectOrMethod) \|\| strpos($objectOrMethod,'::') === false){"` |
|     - | 2214 | `"    throw new TypeError('ReflectionMethod::__construct(): Argument #1 ($objectOrMethod) must be of type object\|string, '.get_debug_type($objectOrMethod).' given');"` |
|     - | 2215 | `"   }"` |
|     - | 2216 | `"   $p = strpos($objectOrMethod,'::');"` |
|     - | 2217 | `"   $method = substr($objectOrMethod,$p+2);"` |
|     - | 2218 | `"   $objectOrMethod = substr($objectOrMethod,0,$p);"` |
|     - | 2219 | `"  }"` |
|     - | 2220 | `"  $ci = __reflect_class_info($objectOrMethod);"` |
|     - | 2221 | `"  if($ci === null){"` |
|     - | 2222 | `"   throw new ReflectionException('Class \"'.$objectOrMethod.'\" does not exist');"` |
|     - | 2223 | `"  }"` |
|     - | 2224 | `"  $this->class = $ci['name'];"` |
|     - | 2225 | `"  $found = null;"` |
|     - | 2226 | `"  if(isset($ci['methods'][$method])){"` |
|     - | 2227 | `"   $found = $method;"` |
|     - | 2228 | `"  }else{"` |
|     - | 2229 | `"   $l = strtolower($method);"` |
|     - | 2230 | `"   foreach($ci['methods'] as $k => $m){"` |
|     - | 2231 | `"    if(strtolower($k) === $l){ $found = $k; break; }"` |
|     - | 2232 | `"   }"` |
|     - | 2233 | `"  }"` |
|     - | 2234 | `"  if($found === null){"` |
|     - | 2235 | `"   throw new ReflectionException('Method '.$this->class.'::'.$method.'() does not exist');"` |
|     - | 2236 | `"  }"` |
|     - | 2237 | `"  $this->name = $found;"` |
|     - | 2238 | `" }"` |
|     - | 2239 | `" public static function createFromMethodName($name){"` |
|     - | 2240 | `"  return new ReflectionMethod($name);"` |
|     - | 2241 | `" }"` |
|     - | 2242 | `" protected function __rfinfo(){ return __reflect_func_info($this->class, $this->name); }"` |
|     - | 2243 | `" protected function __rpspec(){ return array($this->class, $this->name); }"` |
|     - | 2244 | `" public function getDeclaringClass(){"` |
|     - | 2245 | `"  $i = $this->__rfinfo();"` |
|     - | 2246 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2247 | `" }"` |
|     - | 2248 | `" public function getModifiers(){"` |
|     - | 2249 | `"  $i = $this->__rfinfo();"` |
|     - | 2250 | `"  $m = ($i['vis'] === 1 ? 1 : ($i['vis'] === 2 ? 2 : 4));"` |
|     - | 2251 | `"  if($i['mstatic']){ $m \|= 16; }"` |
|     - | 2252 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 2253 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 2254 | `"  return $m;"` |
|     - | 2255 | `" }"` |
|     - | 2256 | `" public function isPublic(){ $i = $this->__rfinfo(); return $i['vis'] === 1; }"` |
|     - | 2257 | `" public function isProtected(){ $i = $this->__rfinfo(); return $i['vis'] === 2; }"` |
|     - | 2258 | `" public function isPrivate(){ $i = $this->__rfinfo(); return $i['vis'] === 3; }"` |
|     - | 2259 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['mstatic']; }"` |
|     - | 2260 | `" public function isAbstract(){ $i = $this->__rfinfo(); return $i['abstract']; }"` |
|     - | 2261 | `" public function isFinal(){ $i = $this->__rfinfo(); return $i['final']; }"` |
|     - | 2262 | `" public function isConstructor(){ return strtolower($this->name) === '__construct'; }"` |
|     - | 2263 | `" public function isDestructor(){ return strtolower($this->name) === '__destruct'; }"` |
|     - | 2264 | `" public function invoke($object = null, ...$args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2265 | `" public function invokeArgs($object, array $args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2266 | `" protected function __rinvoke($object, $args){"` |
|     - | 2267 | `"  $i = $this->__rfinfo();"` |
|     - | 2268 | `"  if(!$i['mstatic']){"` |
|     - | 2269 | `"   if(!is_object($object)){"` |
|     - | 2270 | `"    throw new ReflectionException('Trying to invoke non static method '.$this->class.'::'.$this->name.'() without an object');"` |
|     - | 2271 | `"   }"` |
|     - | 2272 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2273 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2274 | `"   }"` |
|     - | 2275 | `"  }else{"` |
|     - | 2276 | `"   $object = null;"` |
|     - | 2277 | `"  }"` |
|     - | 2278 | `"  return __reflect_invoke($this->class, $this->name, $object, $args);"` |
|     - | 2279 | `" }"` |
|     - | 2280 | `" public function getClosure($object = null){"` |
|     - | 2281 | `"  $i = $this->__rfinfo();"` |
|     - | 2282 | `"  if(!$i['mstatic']){"` |
|     - | 2283 | `"   if($object === null){"` |
|     - | 2284 | `"    throw new ValueError('ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods');"` |
|     - | 2285 | `"   }"` |
|     - | 2286 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2287 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2288 | `"   }"` |
|     - | 2289 | `"  }else{"` |
|     - | 2290 | `"   $object = null;"` |
|     - | 2291 | `"  }"` |
|     - | 2292 | `"  return __reflect_closure($this->class, $this->name, $object);"` |
|     - | 2293 | `" }"` |
|     - | 2294 | `" public function setAccessible($accessible){ }"` |
|     - | 2295 | `" public function hasPrototype(){ return $this->__rproto() !== null; }"` |
|     - | 2296 | `" public function getPrototype(){"` |
|     - | 2297 | `"  $p = $this->__rproto();"` |
|     - | 2298 | `"  if($p === null){"` |
|     - | 2299 | `"   throw new ReflectionException('Method '.$this->class.'::'.$this->name.' does not have a prototype');"` |
|     - | 2300 | `"  }"` |
|     - | 2301 | `"  return new ReflectionMethod($p, $this->name);"` |
|     - | 2302 | `" }"` |
|     - | 2303 | `" protected function __rproto(){"` |
|     - | 2304 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2305 | `"  $l = strtolower($this->name);"` |
|     - | 2306 | `"  $p = $ci['parent'];"` |
|     - | 2307 | `"  while($p !== null){"` |
|     - | 2308 | `"   $pi = __reflect_class_info($p);"` |
|     - | 2309 | `"   foreach($pi['methods'] as $k => $m){"` |
|     - | 2310 | `"    if(strtolower($k) === $l && $m['vis'] !== 3){ return $m['decl']; }"` |
|     - | 2311 | `"   }"` |
|     - | 2312 | `"   $p = $pi['parent'];"` |
|     - | 2313 | `"  }"` |
|     - | 2314 | `"  foreach($ci['interfaces'] as $if){"` |
|     - | 2315 | `"   $ii = __reflect_class_info($if);"` |
|     - | 2316 | `"   foreach($ii['methods'] as $k => $m){"` |
|     - | 2317 | `"    if(strtolower($k) === $l){ return $ii['name']; }"` |
|     - | 2318 | `"   }"` |
|     - | 2319 | `"  }"` |
|     - | 2320 | `"  return null;"` |
|     - | 2321 | `" }"` |
|     - | 2322 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2323 | `"}"` |
|     - | 2324 | `"class ReflectionParameter implements Reflector {"` |
|     - | 2325 | `" public $name;"` |
|     - | 2326 | `" protected $__t;"` |
|     - | 2327 | `" protected $__m = null;"` |
|     - | 2328 | `" protected $__p = 0;"` |
|     - | 2329 | `" public function __construct($function, $param){"` |
|     - | 2330 | `"  $m = null;"` |
|     - | 2331 | `"  $t = $function;"` |
|     - | 2332 | `"  if(is_array($function)){"` |
|     - | 2333 | `"   $t = $function[0];"` |
|     - | 2334 | `"   $m = $function[1];"` |
|     - | 2335 | `"   if(is_object($t)){ $t = get_class($t); }"` |
|     - | 2336 | `"  }else if(is_string($function) && strpos($function,'::') !== false){"` |
|     - | 2337 | `"   $p = strpos($function,'::');"` |
|     - | 2338 | `"   $m = substr($function,$p+2);"` |
|     - | 2339 | `"   $t = substr($function,0,$p);"` |
|     - | 2340 | `"  }"` |
|     - | 2341 | `"  if($m !== null){"` |
|     - | 2342 | `"   $rm = new ReflectionMethod($t, $m);"` |
|     - | 2343 | `"   $t = $rm->class;"` |
|     - | 2344 | `"   $m = $rm->name;"` |
|     - | 2345 | `"   $i = __reflect_func_info($t, $m);"` |
|     - | 2346 | `"  }else if($function instanceof Closure){"` |
|     - | 2347 | `"   $t = $function;"` |
|     - | 2348 | `"   $i = __reflect_func_info($function);"` |
|     - | 2349 | `"  }else{"` |
|     - | 2350 | `"   $i = __reflect_sig_fixup(__reflect_func_info($t));"` |
|     - | 2351 | `"   if($i === null){"` |
|     - | 2352 | `"    throw new ReflectionException('Function '.$t.'() does not exist');"` |
|     - | 2353 | `"   }"` |
|     - | 2354 | `"  }"` |
|     - | 2355 | `"  $found = null;"` |
|     - | 2356 | `"  if(is_int($param)){"` |
|     - | 2357 | `"   if(isset($i['params'][$param])){ $found = $i['params'][$param]; }"` |
|     - | 2358 | `"   if($found === null){"` |
|     - | 2359 | `"    throw new ReflectionException('The parameter specified by its offset could not be found');"` |
|     - | 2360 | `"   }"` |
|     - | 2361 | `"  }else{"` |
|     - | 2362 | `"   foreach($i['params'] as $pp){"` |
|     - | 2363 | `"    if($pp['name'] === $param){ $found = $pp; break; }"` |
|     - | 2364 | `"   }"` |
|     - | 2365 | `"   if($found === null){"` |
|     - | 2366 | `"    throw new ReflectionException('The parameter specified by its name could not be found');"` |
|     - | 2367 | `"   }"` |
|     - | 2368 | `"  }"` |
|     - | 2369 | `"  $this->name = $found['name'];"` |
|     - | 2370 | `"  $this->__t = $t;"` |
|     - | 2371 | `"  $this->__m = $m;"` |
|     - | 2372 | `"  $this->__p = $found['pos'];"` |
|     - | 2373 | `" }"` |
|     - | 2374 | `" protected function __rffull(){"` |
|     - | 2375 | `"  if($this->__m !== null){ return __reflect_func_info($this->__t, $this->__m); }"` |
|     - | 2376 | `"  return __reflect_sig_fixup(__reflect_func_info($this->__t));"` |
|     - | 2377 | `" }"` |
|     - | 2378 | `" protected function __rpinfo(){"` |
|     - | 2379 | `"  $i = $this->__rffull();"` |
|     - | 2380 | `"  return $i['params'][$this->__p];"` |
|     - | 2381 | `" }"` |
|     - | 2382 | `" public function getName(){ return $this->name; }"` |
|     - | 2383 | `" public function getPosition(){ return $this->__p; }"` |
|     - | 2384 | `" public function isPassedByReference(){ $p = $this->__rpinfo(); return $p['byref']; }"` |
|     - | 2385 | `" public function canBePassedByValue(){ return !$this->isPassedByReference(); }"` |
|     - | 2386 | `" public function isVariadic(){ $p = $this->__rpinfo(); return $p['variadic']; }"` |
|     - | 2387 | `" public function isPromoted(){ $p = $this->__rpinfo(); return $p['promoted']; }"` |
|     - | 2388 | `" public function isDefaultValueAvailable(){ $p = $this->__rpinfo(); return $p['hasdef']; }"` |
|     - | 2389 | `" public function isOptional(){"` |
|     - | 2390 | `"  $i = $this->__rffull();"` |
|     - | 2391 | `"  $n = count($i['params']);"` |
|     - | 2392 | `"  for($k = $this->__p; $k < $n; $k++){"` |
|     - | 2393 | `"   $p = $i['params'][$k];"` |
|     - | 2394 | `"   if(!$p['variadic'] && !$p['hasdef']){ return false; }"` |
|     - | 2395 | `"  }"` |
|     - | 2396 | `"  return true;"` |
|     - | 2397 | `" }"` |
|     - | 2398 | `" public function getDefaultValue(){"` |
|     - | 2399 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2400 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2401 | `"  }"` |
|     - | 2402 | `"  $p = $this->__rpinfo();"` |
|     - | 2403 | `"  if(isset($p['deftext'])){"` |
|     - | 2404 | `"   $s = __reflect_sig_scalar($p['deftext']);"` |
|     - | 2405 | `"   if($s[0]){ return $s[1]; }"` |
|     - | 2406 | `"   if($p['deftext'] === 'array (' \|\| strpos($p['deftext'], '[') === 0){ return array(); }"` |
|     - | 2407 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2408 | `"  }"` |
|     - | 2409 | `"  return __reflect_param_default($this->__t, $this->__m, $this->__p);"` |
|     - | 2410 | `" }"` |
|     - | 2411 | `" public function isDefaultValueConstant(){"` |
|     - | 2412 | `"  if(!$this->isDefaultValueAvailable()){ return false; }"` |
|     - | 2413 | `"  $p = $this->__rpinfo();"` |
|     - | 2414 | `"  if(isset($p['deftext'])){ return false; }"` |
|     - | 2415 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p) !== null;"` |
|     - | 2416 | `" }"` |
|     - | 2417 | `" public function getDefaultValueConstantName(){"` |
|     - | 2418 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2419 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2420 | `"  }"` |
|     - | 2421 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p);"` |
|     - | 2422 | `" }"` |
|     - | 2423 | `" public function allowsNull(){"` |
|     - | 2424 | `"  $p = $this->__rpinfo();"` |
|     - | 2425 | `"  if($p['typetext'] === null){ return true; }"` |
|     - | 2426 | `"  if($p['nullable']){ return true; }"` |
|     - | 2427 | `"  return $p['typetext'] === 'mixed' \|\| $p['typetext'] === 'null';"` |
|     - | 2428 | `" }"` |
|     - | 2429 | `" public function hasType(){ $p = $this->__rpinfo(); return $p['typetext'] !== null; }"` |
|     - | 2430 | `" public function getType(){ $p = $this->__rpinfo(); return __reflect_make_type($p['typetext']); }"` |
|     - | 2431 | `" public function getDeclaringFunction(){"` |
|     - | 2432 | `"  if($this->__m !== null){ return new ReflectionMethod($this->__t, $this->__m); }"` |
|     - | 2433 | `"  return new ReflectionFunction($this->__t);"` |
|     - | 2434 | `" }"` |
|     - | 2435 | `" public function getDeclaringClass(){"` |
|     - | 2436 | `"  if($this->__m === null){ return null; }"` |
|     - | 2437 | `"  $i = $this->__rffull();"` |
|     - | 2438 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2439 | `" }"` |
|     - | 2440 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2441 | `"  $p = $this->__rpinfo();"` |
|     - | 2442 | `"  return __reflect_build_attrs($p['attrs'], array('param', $this->__t, $this->__m, $this->__p), 32, $name, $flags);"` |
|     - | 2443 | `" }"` |
|     - | 2444 | `" public function __toString(){ return __reflect_export_param($this); }"` |
|     - | 2445 | `"}"` |
|     - | 2446 | `;` |
|     - | 2447 | `/*` |
|     - | 2448 | ` * Chunk 3: ReflectionProperty, ReflectionClassConstant.` |
|     - | 2449 | ` */` |
|     - | 2450 | `static const char zReflectLib3[] =` |
|     - | 2451 | `"class ReflectionProperty implements Reflector {"` |
|     - | 2452 | `" const IS_PUBLIC = 1;"` |
|     - | 2453 | `" const IS_PROTECTED = 2;"` |
|     - | 2454 | `" const IS_PRIVATE = 4;"` |
|     - | 2455 | `" const IS_STATIC = 16;"` |
|     - | 2456 | `" const IS_FINAL = 32;"` |
|     - | 2457 | `" const IS_ABSTRACT = 64;"` |
|     - | 2458 | `" const IS_READONLY = 128;"` |
|     - | 2459 | `" const IS_VIRTUAL = 512;"` |
|     - | 2460 | `" const IS_PROTECTED_SET = 2048;"` |
|     - | 2461 | `" const IS_PRIVATE_SET = 4096;"` |
|     - | 2462 | `" public $name;"` |
|     - | 2463 | `" public $class;"` |
|     - | 2464 | `" protected $__dynobj = null;"` |
|     - | 2465 | `" public function __construct($class, $property){"` |
|     - | 2466 | `"  $obj = null;"` |
|     - | 2467 | `"  if(is_object($class)){ $obj = $class; }"` |
|     - | 2468 | `"  else if(!is_string($class)){"` |
|     - | 2469 | `"   throw new TypeError('ReflectionProperty::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2470 | `"  }"` |
|     - | 2471 | `"  $ci = __reflect_class_info($class);"` |
|     - | 2472 | `"  if($ci === null){"` |
|     - | 2473 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2474 | `"  }"` |
|     - | 2475 | `"  $this->class = $ci['name'];"` |
|     - | 2476 | `"  if(isset($ci['props'][$property])){"` |
|     - | 2477 | `"   $this->name = $property;"` |
|     - | 2478 | `"   return;"` |
|     - | 2479 | `"  }"` |
|     - | 2480 | `"  if($obj !== null && (__reflect_prop_state($obj, $property) & 1)){"` |
|     - | 2481 | `"   $this->name = $property;"` |
|     - | 2482 | `"   $this->__dynobj = $obj;"` |
|     - | 2483 | `"   return;"` |
|     - | 2484 | `"  }"` |
|     - | 2485 | `"  throw new ReflectionException('Property '.$this->class.'::$'.$property.' does not exist');"` |
|     - | 2486 | `" }"` |
|     - | 2487 | `" protected function __rpmeta(){"` |
|     - | 2488 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2489 | `"  if(isset($ci['props'][$this->name])){ return $ci['props'][$this->name]; }"` |
|     - | 2490 | `"  return array('vis' => 1, 'static' => false, 'readonly' => false, 'hasdef' => false,"` |
|     - | 2491 | `"   'typed' => false, 'typetext' => null, 'decl' => $this->class, 'line' => 0, 'dyn' => true);"` |
|     - | 2492 | `" }"` |
|     - | 2493 | `" public function getName(){ return $this->name; }"` |
|     - | 2494 | `" public function getDeclaringClass(){"` |
|     - | 2495 | `"  $m = $this->__rpmeta();"` |
|     - | 2496 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2497 | `" }"` |
|     - | 2498 | `" public function getModifiers(){"` |
|     - | 2499 | `"  $m = $this->__rpmeta();"` |
|     - | 2500 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2501 | `"  if($m['static']){ $mod \|= 16; }"` |
|     - | 2502 | `"  if($m['readonly']){ $mod \|= 128; }"` |
|     - | 2503 | `"  return $mod;"` |
|     - | 2504 | `" }"` |
|     - | 2505 | `" public function isPublic(){ $m = $this->__rpmeta(); return $m['vis'] === 1; }"` |
|     - | 2506 | `" public function isProtected(){ $m = $this->__rpmeta(); return $m['vis'] === 2; }"` |
|     - | 2507 | `" public function isPrivate(){ $m = $this->__rpmeta(); return $m['vis'] === 3; }"` |
|     - | 2508 | `" public function isStatic(){ $m = $this->__rpmeta(); return $m['static']; }"` |
|     - | 2509 | `" public function isReadOnly(){ $m = $this->__rpmeta(); return $m['readonly']; }"` |
|     - | 2510 | `" public function isDefault(){ $m = $this->__rpmeta(); return !isset($m['dyn']); }"` |
|     - | 2511 | `" public function isDynamic(){ $m = $this->__rpmeta(); return isset($m['dyn']); }"` |
|     - | 2512 | `" public function isAbstract(){ return false; }"` |
|     - | 2513 | `" public function isFinal(){ return false; }"` |
|     - | 2514 | `" public function isVirtual(){ return false; }"` |
|     - | 2515 | `" public function isPrivateSet(){ return false; }"` |
|     - | 2516 | `" public function isProtectedSet(){ return false; }"` |
|     - | 2517 | `" public function hasHooks(){ return false; }"` |
|     - | 2518 | `" public function getHooks(){ return array(); }"` |
|     - | 2519 | `" public function hasHook($type){ return false; }"` |
|     - | 2520 | `" public function getHook($type){ return null; }"` |
|     - | 2521 | `" public function isLazy($object){ return false; }"` |
|     - | 2522 | `" public function setAccessible($accessible){ }"` |
|     - | 2523 | `" public function getValue($object = null){"` |
|     - | 2524 | `"  $m = $this->__rpmeta();"` |
|     - | 2525 | `"  if($m['static']){ return __reflect_static_value($this->class, $this->name); }"` |
|     - | 2526 | `"  if(!is_object($object)){"` |
|     - | 2527 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2528 | `"  }"` |
|     - | 2529 | `"  return __reflect_prop_read($object, $this->name);"` |
|     - | 2530 | `" }"` |
|     - | 2531 | `" public function setValue($objectOrValue = null, $value = null){"` |
|     - | 2532 | `"  $m = $this->__rpmeta();"` |
|     - | 2533 | `"  if($m['static']){"` |
|     - | 2534 | `"   if($value === null && $objectOrValue !== null && !is_object($objectOrValue)){"` |
|     - | 2535 | `"    __reflect_static_set($this->class, $this->name, $objectOrValue);"` |
|     - | 2536 | `"   }else{"` |
|     - | 2537 | `"    __reflect_static_set($this->class, $this->name, $value);"` |
|     - | 2538 | `"   }"` |
|     - | 2539 | `"   return;"` |
|     - | 2540 | `"  }"` |
|     - | 2541 | `"  __reflect_prop_write($objectOrValue, $this->name, $value);"` |
|     - | 2542 | `" }"` |
|     - | 2543 | `" public function getRawValue($object){ return $this->getValue($object); }"` |
|     - | 2544 | `" public function setRawValue($object, $value){ $this->setValue($object, $value); }"` |
|     - | 2545 | `" public function isInitialized($object = null){"` |
|     - | 2546 | `"  $m = $this->__rpmeta();"` |
|     - | 2547 | `"  if($m['static']){ return (__reflect_prop_state($this->class, $this->name) & 2) !== 0; }"` |
|     - | 2548 | `"  if(!is_object($object)){"` |
|     - | 2549 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2550 | `"  }"` |
|     - | 2551 | `"  return (__reflect_prop_state($object, $this->name) & 2) !== 0;"` |
|     - | 2552 | `" }"` |
|     - | 2553 | `" public function hasDefaultValue(){"` |
|     - | 2554 | `"  $m = $this->__rpmeta();"` |
|     - | 2555 | `"  if(isset($m['dyn'])){ return false; }"` |
|     - | 2556 | `"  if($m['hasdef']){ return true; }"` |
|     - | 2557 | `"  return !$m['typed'];"` |
|     - | 2558 | `" }"` |
|     - | 2559 | `" public function getDefaultValue(){"` |
|     - | 2560 | `"  $m = $this->__rpmeta();"` |
|     - | 2561 | `"  if(isset($m['dyn']) \|\| !$m['hasdef']){ return null; }"` |
|     - | 2562 | `"  return __reflect_prop_default($this->class, $this->name);"` |
|     - | 2563 | `" }"` |
|     - | 2564 | `" public function hasType(){ $m = $this->__rpmeta(); return $m['typed']; }"` |
|     - | 2565 | `" public function getType(){ $m = $this->__rpmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2566 | `" public function getSettableType(){ return $this->getType(); }"` |
|     - | 2567 | `" public function setRawValueWithoutLazyInitialization($object, $value){"` |
|     - | 2568 | `"  throw new Error('ReflectionProperty::setRawValueWithoutLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2569 | `" }"` |
|     - | 2570 | `" public function skipLazyInitialization($object){"` |
|     - | 2571 | `"  throw new Error('ReflectionProperty::skipLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2572 | `" }"` |
|     - | 2573 | `" public function getDocComment(){ $m = $this->__rpmeta(); return isset($m['doc']) ? $m['doc'] : false; }"` |
|     - | 2574 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2575 | `"  $m = $this->__rpmeta();"` |
|     - | 2576 | `"  if(!isset($m['attrs'])){ return array(); }"` |
|     - | 2577 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 8, $name, $flags);"` |
|     - | 2578 | `" }"` |
|     - | 2579 | `" public function __toString(){ return __reflect_export_prop($this); }"` |
|     - | 2580 | `"}"` |
|     - | 2581 | `"class ReflectionClassConstant implements Reflector {"` |
|     - | 2582 | `" const IS_PUBLIC = 1;"` |
|     - | 2583 | `" const IS_PROTECTED = 2;"` |
|     - | 2584 | `" const IS_PRIVATE = 4;"` |
|     - | 2585 | `" const IS_FINAL = 32;"` |
|     - | 2586 | `" public $name;"` |
|     - | 2587 | `" public $class;"` |
|     - | 2588 | `" public function __construct($class, $constant){"` |
|     - | 2589 | `"  if(!is_object($class) && !is_string($class)){"` |
|     - | 2590 | `"   throw new TypeError('ReflectionClassConstant::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2591 | `"  }"` |
|     - | 2592 | `"  $ci = __reflect_class_info($class);"` |
|     - | 2593 | `"  if($ci === null){"` |
|     - | 2594 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2595 | `"  }"` |
|     - | 2596 | `"  $this->class = $ci['name'];"` |
|     - | 2597 | `"  if(!isset($ci['consts'][$constant])){"` |
|     - | 2598 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' does not exist');"` |
|     - | 2599 | `"  }"` |
|     - | 2600 | `"  $this->name = $constant;"` |
|     - | 2601 | `" }"` |
|     - | 2602 | `" protected function __rcmeta(){"` |
|     - | 2603 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2604 | `"  return $ci['consts'][$this->name];"` |
|     - | 2605 | `" }"` |
|     - | 2606 | `" public function getName(){ return $this->name; }"` |
|     - | 2607 | `" public function getValue(){ return __reflect_const_value($this->class, $this->name); }"` |
|     - | 2608 | `" public function getDeclaringClass(){"` |
|     - | 2609 | `"  $m = $this->__rcmeta();"` |
|     - | 2610 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2611 | `" }"` |
|     - | 2612 | `" public function getModifiers(){"` |
|     - | 2613 | `"  $m = $this->__rcmeta();"` |
|     - | 2614 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2615 | `"  if($m['final']){ $mod \|= 32; }"` |
|     - | 2616 | `"  return $mod;"` |
|     - | 2617 | `" }"` |
|     - | 2618 | `" public function isPublic(){ $m = $this->__rcmeta(); return $m['vis'] === 1; }"` |
|     - | 2619 | `" public function isProtected(){ $m = $this->__rcmeta(); return $m['vis'] === 2; }"` |
|     - | 2620 | `" public function isPrivate(){ $m = $this->__rcmeta(); return $m['vis'] === 3; }"` |
|     - | 2621 | `" public function isFinal(){ $m = $this->__rcmeta(); return $m['final']; }"` |
|     - | 2622 | `" public function isEnumCase(){ $m = $this->__rcmeta(); return $m['enumcase']; }"` |
|     - | 2623 | `" public function isDeprecated(){ $m = $this->__rcmeta(); return __reflect_has_deprecated($m['attrs']); }"` |
|     - | 2624 | `" public function hasType(){ $m = $this->__rcmeta(); return $m['typed']; }"` |
|     - | 2625 | `" public function getType(){ $m = $this->__rcmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2626 | `" public function getDocComment(){ $m = $this->__rcmeta(); return $m['doc']; }"` |
|     - | 2627 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2628 | `"  $m = $this->__rcmeta();"` |
|     - | 2629 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 16, $name, $flags);"` |
|     - | 2630 | `" }"` |
|     - | 2631 | `" public function __toString(){ return __reflect_export_cconst($this); }"` |
|     - | 2632 | `"}"` |
|     - | 2633 | `;` |
|     - | 2634 | `/*` |
|     - | 2635 | ` * Chunk 4: the ReflectionType family, built from the engine's canonical` |
|     - | 2636 | ` * type text ("?int", "string\|float", "(A&B)\|C" — normalized at compile` |
|     - | 2637 | ` * time). __reflect_make_type is the internal factory; PHP itself never` |
|     - | 2638 | ` * lets user code construct these, so the public constructors here are a` |
|     - | 2639 | ` * recorded PHL-only surface.` |
|     - | 2640 | ` */` |
|     - | 2641 | `static const char zReflectLib4[] =` |
|     - | 2642 | `"abstract class ReflectionType implements Stringable {"` |
|     - | 2643 | `" protected $__text = '';"` |
|     - | 2644 | `" protected $__nullable = false;"` |
|     - | 2645 | `" public function allowsNull(){ return $this->__nullable; }"` |
|     - | 2646 | `" public function __toString(){ return $this->__text; }"` |
|     - | 2647 | `"}"` |
|     - | 2648 | `"class ReflectionNamedType extends ReflectionType {"` |
|     - | 2649 | `" protected $__tname = '';"` |
|     - | 2650 | `" public function __construct($name = '', $nullable = false, $text = null){"` |
|     - | 2651 | `"  $this->__tname = $name;"` |
|     - | 2652 | `"  $l = strtolower($name);"` |
|     - | 2653 | `"  $this->__nullable = $nullable \|\| $l === 'null' \|\| $l === 'mixed';"` |
|     - | 2654 | `"  $this->__text = $text === null ? $name : $text;"` |
|     - | 2655 | `" }"` |
|     - | 2656 | `" public function getName(){ return $this->__tname; }"` |
|     - | 2657 | `" public function isBuiltin(){"` |
|     - | 2658 | `"  $l = strtolower($this->__tname);"` |
|     - | 2659 | `"  return in_array($l, array('int','float','string','bool','array','object','mixed',"` |
|     - | 2660 | `"   'void','never','null','callable','iterable','true','false'), true);"` |
|     - | 2661 | `" }"` |
|     - | 2662 | `"}"` |
|     - | 2663 | `"class ReflectionUnionType extends ReflectionType {"` |
|     - | 2664 | `" protected $__types = array();"` |
|     - | 2665 | `" public function __construct($text = '', $nullable = false, $types = array()){"` |
|     - | 2666 | `"  $this->__text = $text;"` |
|     - | 2667 | `"  $this->__nullable = $nullable;"` |
|     - | 2668 | `"  $this->__types = $types;"` |
|     - | 2669 | `" }"` |
|     - | 2670 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2671 | `"}"` |
|     - | 2672 | `"class ReflectionIntersectionType extends ReflectionType {"` |
|     - | 2673 | `" protected $__types = array();"` |
|     - | 2674 | `" public function __construct($text = '', $types = array()){"` |
|     - | 2675 | `"  $this->__text = $text;"` |
|     - | 2676 | `"  $this->__nullable = false;"` |
|     - | 2677 | `"  $this->__types = $types;"` |
|     - | 2678 | `" }"` |
|     - | 2679 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2680 | `"}"` |
|     - | 2681 | `"function __reflect_make_atom($p){"` |
|     - | 2682 | `" $nullable = false;"` |
|     - | 2683 | `" if($p !== '' && $p[0] === '?'){ $nullable = true; $p = substr($p, 1); }"` |
|     - | 2684 | `" if($p !== '' && $p[0] === '('){ $p = substr($p, 1, strlen($p) - 2); }"` |
|     - | 2685 | `" if(strpos($p, '&') !== false){"` |
|     - | 2686 | `"  $subs = array();"` |
|     - | 2687 | `"  foreach(explode('&', $p) as $s){ $subs[] = new ReflectionNamedType($s, false, $s); }"` |
|     - | 2688 | `"  return new ReflectionIntersectionType($p, $subs);"` |
|     - | 2689 | `" }"` |
|     - | 2690 | `" return new ReflectionNamedType($p, $nullable, $nullable ? '?'.$p : $p);"` |
|     - | 2691 | `"}"` |
|     - | 2692 | `"function __reflect_make_type($text){"` |
|     - | 2693 | `" if($text === null \|\| $text === ''){ return null; }"` |
|     - | 2694 | `" $nullable = false;"` |
|     - | 2695 | `" $body = $text;"` |
|     - | 2696 | `" if($body[0] === '?'){ $nullable = true; $body = substr($body, 1); }"` |
|     - | 2697 | `" $parts = array();"` |
|     - | 2698 | `" $depth = 0;"` |
|     - | 2699 | `" $cur = '';"` |
|     - | 2700 | `" $n = strlen($body);"` |
|     - | 2701 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 2702 | `"  $ch = $body[$k];"` |
|     - | 2703 | `"  if($ch === '('){ $depth++; $cur .= $ch; }"` |
|     - | 2704 | `"  else if($ch === ')'){ $depth--; $cur .= $ch; }"` |
|     - | 2705 | `"  else if($ch === '\|' && $depth === 0){ $parts[] = $cur; $cur = ''; }"` |
|     - | 2706 | `"  else{ $cur .= $ch; }"` |
|     - | 2707 | `" }"` |
|     - | 2708 | `" $parts[] = $cur;"` |
|     - | 2709 | `" if(count($parts) > 1){"` |
|     - | 2710 | `"  $nonNull = array();"` |
|     - | 2711 | `"  $hasNull = false;"` |
|     - | 2712 | `"  foreach($parts as $p){"` |
|     - | 2713 | `"   if(strtolower($p) === 'null'){ $hasNull = true; }"` |
|     - | 2714 | `"   else{ $nonNull[] = $p; }"` |
|     - | 2715 | `"  }"` |
|     - | 2716 | `"  if($hasNull && count($nonNull) === 1 && strpos($nonNull[0], '&') === false){"` |
|     - | 2717 | `"   return new ReflectionNamedType($nonNull[0], true, '?'.$nonNull[0]);"` |
|     - | 2718 | `"  }"` |
|     - | 2719 | `"  $types = array();"` |
|     - | 2720 | `"  foreach($parts as $p){ $types[] = __reflect_make_atom($p); }"` |
|     - | 2721 | `"  return new ReflectionUnionType($body, $nullable \|\| $hasNull, $types);"` |
|     - | 2722 | `" }"` |
|     - | 2723 | `" if(strpos($body, '&') !== false){ return __reflect_make_atom($body); }"` |
|     - | 2724 | `" return __reflect_make_atom($nullable ? '?'.$body : $body);"` |
|     - | 2725 | `"}"` |
|     - | 2726 | `;` |
|     - | 2727 | `/*` |
|     - | 2728 | ` * Chunk 5: ReflectionGenerator, ReflectionFiber. Executing line/file and` |
|     - | 2729 | ` * traces need runtime line tracking the VM does not have (same gap as` |
|     - | 2730 | ` * debug_backtrace's line numbers) — those throw a loud Error, recorded in` |
|     - | 2731 | ` * the plan ledger.` |
|     - | 2732 | ` */` |
|     - | 2733 | `static const char zReflectLib5[] =` |
|     - | 2734 | `"class ReflectionGenerator {"` |
|     - | 2735 | `" protected $__gen;"` |
|     - | 2736 | `" public function __construct($generator){"` |
|     - | 2737 | `"  if(!($generator instanceof Generator)){"` |
|     - | 2738 | `"   throw new TypeError('ReflectionGenerator::__construct(): Argument #1 ($generator) must be of type Generator, '.get_debug_type($generator).' given');"` |
|     - | 2739 | `"  }"` |
|     - | 2740 | `"  $this->__gen = $generator;"` |
|     - | 2741 | `" }"` |
|     - | 2742 | `" protected function __rginfo(){ return __reflect_gen_info($this->__gen); }"` |
|     - | 2743 | `" public function getFunction(){"` |
|     - | 2744 | `"  $i = $this->__rginfo();"` |
|     - | 2745 | `"  if($i['kind'] === 'method'){ return new ReflectionMethod($i['class'], $i['name']); }"` |
|     - | 2746 | `"  return new ReflectionFunction($i['name']);"` |
|     - | 2747 | `" }"` |
|     - | 2748 | `" public function getThis(){ $i = $this->__rginfo(); return isset($i['this']) ? $i['this'] : null; }"` |
|     - | 2749 | `" public function getExecutingGenerator(){ return __reflect_gen_exec($this->__gen); }"` |
|     - | 2750 | `" public function isClosed(){ $i = $this->__rginfo(); return $i['closed']; }"` |
|     - | 2751 | `" public function getExecutingLine(){"` |
|     - | 2752 | `"  throw new Error('ReflectionGenerator::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2753 | `" }"` |
|     - | 2754 | `" public function getExecutingFile(){"` |
|     - | 2755 | `"  throw new Error('ReflectionGenerator::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2756 | `" }"` |
|     - | 2757 | `" public function getTrace($options = 1){"` |
|     - | 2758 | `"  throw new Error('ReflectionGenerator::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2759 | `" }"` |
|     - | 2760 | `"}"` |
|     - | 2761 | `"class ReflectionFiber {"` |
|     - | 2762 | `" protected $__fiber;"` |
|     - | 2763 | `" public function __construct($fiber){"` |
|     - | 2764 | `"  if(!($fiber instanceof Fiber)){"` |
|     - | 2765 | `"   throw new TypeError('ReflectionFiber::__construct(): Argument #1 ($fiber) must be of type Fiber, '.get_debug_type($fiber).' given');"` |
|     - | 2766 | `"  }"` |
|     - | 2767 | `"  $this->__fiber = $fiber;"` |
|     - | 2768 | `" }"` |
|     - | 2769 | `" public function getFiber(){ return $this->__fiber; }"` |
|     - | 2770 | `" public function getCallable(){ return __reflect_prop_read($this->__fiber, '__callable'); }"` |
|     - | 2771 | `" public function getExecutingLine(){"` |
|     - | 2772 | `"  throw new Error('ReflectionFiber::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2773 | `" }"` |
|     - | 2774 | `" public function getExecutingFile(){"` |
|     - | 2775 | `"  throw new Error('ReflectionFiber::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2776 | `" }"` |
|     - | 2777 | `" public function getTrace($options = 1){"` |
|     - | 2778 | `"  throw new Error('ReflectionFiber::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2779 | `" }"` |
|     - | 2780 | `"}"` |
|     - | 2781 | `;` |
|     - | 2782 | `/*` |
|     - | 2783 | ` * Chunk 6: the long tail — ReflectionConstant (PHP 8.5), the synthetic` |
|     - | 2784 | ` * "Core" ReflectionExtension, ReflectionZendExtension (throws: no Zend` |
|     - | 2785 | ` * extensions exist), the ReflectionEnum family (throws: enums are not a` |
|     - | 2786 | ` * PHL language feature yet), and ReflectionReference.` |
|     - | 2787 | ` */` |
|     - | 2788 | `static const char zReflectLib6[] =` |
|     - | 2789 | `"class ReflectionConstant implements Reflector {"` |
|     - | 2790 | `" public $name;"` |
|     - | 2791 | `" public function __construct($name){"` |
|     - | 2792 | `"  if(!is_string($name)){"` |
|     - | 2793 | `"   throw new TypeError('ReflectionConstant::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2794 | `"  }"` |
|     - | 2795 | `"  $i = __reflect_const_info($name);"` |
|     - | 2796 | `"  if($i === null){"` |
|     - | 2797 | `"   throw new ReflectionException('Constant \"'.$name.'\" does not exist');"` |
|     - | 2798 | `"  }"` |
|     - | 2799 | `"  $this->name = $name;"` |
|     - | 2800 | `" }"` |
|     - | 2801 | `" public function getName(){ return $this->name; }"` |
|     - | 2802 | `" public function getNamespaceName(){"` |
|     - | 2803 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2804 | `"  if($p === false){ return ''; }"` |
|     - | 2805 | `"  return substr($this->name,0,$p);"` |
|     - | 2806 | `" }"` |
|     - | 2807 | `" public function getShortName(){"` |
|     - | 2808 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2809 | `"  if($p === false){ return $this->name; }"` |
|     - | 2810 | `"  return substr($this->name,$p+1);"` |
|     - | 2811 | `" }"` |
|     - | 2812 | `" public function getValue(){"` |
|     - | 2813 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2814 | `"  return $i['value'];"` |
|     - | 2815 | `" }"` |
|     - | 2816 | `" public function isDeprecated(){ return false; }"` |
|     - | 2817 | `" public function getFileName(){"` |
|     - | 2818 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2819 | `"  return $i['file'];"` |
|     - | 2820 | `" }"` |
|     - | 2821 | `" public function getExtension(){"` |
|     - | 2822 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2823 | `"  return $i['internal'] ? new ReflectionExtension('Core') : null;"` |
|     - | 2824 | `" }"` |
|     - | 2825 | `" public function getExtensionName(){"` |
|     - | 2826 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2827 | `"  return $i['internal'] ? 'Core' : false;"` |
|     - | 2828 | `" }"` |
|     - | 2829 | `" public function getAttributes($name = null, $flags = 0){ return array(); }"` |
|     - | 2830 | `" public function __toString(){"` |
|     - | 2831 | `"  return 'Constant [ '.$this->name.' ]'.\"\\n\";"` |
|     - | 2832 | `" }"` |
|     - | 2833 | `"}"` |
|     - | 2834 | `"class ReflectionExtension implements Reflector {"` |
|     - | 2835 | `" public $name;"` |
|     - | 2836 | `" public function __construct($name){"` |
|     - | 2837 | `"  if(!is_string($name)){"` |
|     - | 2838 | `"   throw new TypeError('ReflectionExtension::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2839 | `"  }"` |
|     - | 2840 | `"  if(strtolower($name) !== 'core'){"` |
|     - | 2841 | `"   throw new ReflectionException('Extension \"'.$name.'\" does not exist');"` |
|     - | 2842 | `"  }"` |
|     - | 2843 | `"  $this->name = 'Core';"` |
|     - | 2844 | `" }"` |
|     - | 2845 | `" public function getName(){ return $this->name; }"` |
|     - | 2846 | `" public function getVersion(){ return phpversion(); }"` |
|     - | 2847 | `" public function getFunctions(){ return array(); }"` |
|     - | 2848 | `" public function getClasses(){ return array(); }"` |
|     - | 2849 | `" public function getClassNames(){ return array(); }"` |
|     - | 2850 | `" public function getConstants(){ return array(); }"` |
|     - | 2851 | `" public function getINIEntries(){ return array(); }"` |
|     - | 2852 | `" public function getDependencies(){ return array(); }"` |
|     - | 2853 | `" public function isPersistent(){ return true; }"` |
|     - | 2854 | `" public function isTemporary(){ return false; }"` |
|     - | 2855 | `" public function info(){ }"` |
|     - | 2856 | `" public function __toString(){"` |
|     - | 2857 | `"  return 'Extension [ extension #1 '.$this->name.' ]'.\"\\n\";"` |
|     - | 2858 | `" }"` |
|     - | 2859 | `"}"` |
|     - | 2860 | `"class ReflectionZendExtension implements Reflector {"` |
|     - | 2861 | `" public $name;"` |
|     - | 2862 | `" public function __construct($name){"` |
|     - | 2863 | `"  throw new ReflectionException('Zend Extension \"'.$name.'\" does not exist');"` |
|     - | 2864 | `" }"` |
|     - | 2865 | `" public function getName(){ return $this->name; }"` |
|     - | 2866 | `" public function __toString(){ return ''; }"` |
|     - | 2867 | `"}"` |
|     - | 2868 | `"class ReflectionEnum extends ReflectionClass {"` |
|     - | 2869 | `" public function __construct($objectOrClass){"` |
|     - | 2870 | `"  $info = __reflect_class_info($objectOrClass);"` |
|     - | 2871 | `"  if($info === null){"` |
|     - | 2872 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 2873 | `"  }"` |
|     - | 2874 | `"  if(!$info['enum']){"` |
|     - | 2875 | `"   throw new ReflectionException('Class \"'.$info['name'].'\" is not an enum');"` |
|     - | 2876 | `"  }"` |
|     - | 2877 | `"  parent::__construct($objectOrClass);"` |
|     - | 2878 | `" }"` |
|     - | 2879 | `" public function hasCase($name){"` |
|     - | 2880 | `"  $i = $this->__rinfo();"` |
|     - | 2881 | `"  return in_array($name, $i['cases'], true);"` |
|     - | 2882 | `" }"` |
|     - | 2883 | `" public function getCase($name){"` |
|     - | 2884 | `"  if(!$this->hasCase($name)){"` |
|     - | 2885 | `"   throw new ReflectionException('Case '.$this->name.'::'.$name.' does not exist');"` |
|     - | 2886 | `"  }"` |
|     - | 2887 | `"  if($this->isBacked()){ return new ReflectionEnumBackedCase($this->name, $name); }"` |
|     - | 2888 | `"  return new ReflectionEnumUnitCase($this->name, $name);"` |
|     - | 2889 | `" }"` |
|     - | 2890 | `" public function getCases(){"` |
|     - | 2891 | `"  $i = $this->__rinfo();"` |
|     - | 2892 | `"  $out = array();"` |
|     - | 2893 | `"  foreach($i['cases'] as $c){"` |
|     - | 2894 | `"   $out[] = $this->isBacked()"` |
|     - | 2895 | `"    ? new ReflectionEnumBackedCase($this->name, $c)"` |
|     - | 2896 | `"    : new ReflectionEnumUnitCase($this->name, $c);"` |
|     - | 2897 | `"  }"` |
|     - | 2898 | `"  return $out;"` |
|     - | 2899 | `" }"` |
|     - | 2900 | `" public function isBacked(){ $i = $this->__rinfo(); return $i['enumbacking'] !== ''; }"` |
|     - | 2901 | `" public function getBackingType(){"` |
|     - | 2902 | `"  $i = $this->__rinfo();"` |
|     - | 2903 | `"  if($i['enumbacking'] === ''){ return null; }"` |
|     - | 2904 | `"  return __reflect_make_type($i['enumbacking']);"` |
|     - | 2905 | `" }"` |
|     - | 2906 | `"}"` |
|     - | 2907 | `"class ReflectionEnumUnitCase extends ReflectionClassConstant {"` |
|     - | 2908 | `" public function __construct($class, $constant){"` |
|     - | 2909 | `"  parent::__construct($class, $constant);"` |
|     - | 2910 | `"  $ci = __reflect_class_info($class);"` |
|     - | 2911 | `"  if(!$ci['enum']){"` |
|     - | 2912 | `"   throw new ReflectionException('Class \"'.$this->class.'\" is not an enum');"` |
|     - | 2913 | `"  }"` |
|     - | 2914 | `"  $m = $this->__rcmeta();"` |
|     - | 2915 | `"  if(!$m['enumcase']){"` |
|     - | 2916 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' is not a case');"` |
|     - | 2917 | `"  }"` |
|     - | 2918 | `" }"` |
|     - | 2919 | `" public function getEnum(){ return new ReflectionEnum($this->class); }"` |
|     - | 2920 | `"}"` |
|     - | 2921 | `"class ReflectionEnumBackedCase extends ReflectionEnumUnitCase {"` |
|     - | 2922 | `" public function getBackingValue(){ return $this->getValue()->value; }"` |
|     - | 2923 | `"}"` |
|     - | 2924 | `"final class ReflectionReference {"` |
|     - | 2925 | `" protected $__id = '';"` |
|     - | 2926 | `" public function __construct(){"` |
|     - | 2927 | `"  throw new Error('Call to private ReflectionReference::__construct() from global scope');"` |
|     - | 2928 | `" }"` |
|     - | 2929 | `" public static function fromArrayElement($array, $key){"` |
|     - | 2930 | `"  if(!is_array($array)){"` |
|     - | 2931 | `"   throw new TypeError('ReflectionReference::fromArrayElement(): Argument #1 ($array) must be of type array, '.get_debug_type($array).' given');"` |
|     - | 2932 | `"  }"` |
|     - | 2933 | `"  $id = __reflect_ref_id($array, $key);"` |
|     - | 2934 | `"  if($id === null){ return null; }"` |
|     - | 2935 | `"  $r = __reflect_new_no_ctor('ReflectionReference');"` |
|     - | 2936 | `"  $r->__setId('phlref'.$id);"` |
|     - | 2937 | `"  return $r;"` |
|     - | 2938 | `" }"` |
|     - | 2939 | `" public function __setId($id){ $this->__id = $id; }"` |
|     - | 2940 | `" public function getId(){ return $this->__id; }"` |
|     - | 2941 | `"}"` |
|     - | 2942 | `;` |
|     - | 2943 | `/*` |
|     - | 2944 | ` * Chunk 7: ReflectionAttribute and the shared getAttributes() builder.` |
|     - | 2945 | ` * The spec array rides as [kind, target, member, paramIdx]; argument` |
|     - | 2946 | ` * values evaluate lazily through __reflect_attr_args (PHP semantics).` |
|     - | 2947 | ` */` |
|     - | 2948 | `static const char zReflectLib7[] =` |
|     - | 2949 | `"function __reflect_has_deprecated($meta){"` |
|     - | 2950 | `" foreach($meta as $a){"` |
|     - | 2951 | `"  if(strtolower($a['name']) === 'deprecated'){ return true; }"` |
|     - | 2952 | `" }"` |
|     - | 2953 | `" return false;"` |
|     - | 2954 | `"}"` |
|     - | 2955 | `"function __reflect_target_names($mask){"` |
|     - | 2956 | `" $parts = array();"` |
|     - | 2957 | `" foreach(array('class' => 1, 'function' => 2, 'method' => 4, 'property' => 8,"` |
|     - | 2958 | `"  'class constant' => 16, 'parameter' => 32, 'constant' => 64) as $nm => $bit){"` |
|     - | 2959 | `"  if($mask & $bit){ $parts[] = $nm; }"` |
|     - | 2960 | `" }"` |
|     - | 2961 | `" return implode(', ', $parts);"` |
|     - | 2962 | `"}"` |
|     - | 2963 | `"function __reflect_build_attrs($meta, $spec, $target, $name, $flags){"` |
|     - | 2964 | `" $out = array();"` |
|     - | 2965 | `" $counts = array();"` |
|     - | 2966 | `" foreach($meta as $a){"` |
|     - | 2967 | `"  $k = strtolower($a['name']);"` |
|     - | 2968 | `"  $counts[$k] = isset($counts[$k]) ? $counts[$k] + 1 : 1;"` |
|     - | 2969 | `" }"` |
|     - | 2970 | `" $idx = 0;"` |
|     - | 2971 | `" foreach($meta as $a){"` |
|     - | 2972 | `"  $keep = true;"` |
|     - | 2973 | `"  if($name !== null){"` |
|     - | 2974 | `"   $keep = strtolower($a['name']) === strtolower($name);"` |
|     - | 2975 | `"   if(!$keep && ($flags & 2)){"` |
|     - | 2976 | `"    $keep = is_subclass_of($a['name'], $name);"` |
|     - | 2977 | `"   }"` |
|     - | 2978 | `"  }"` |
|     - | 2979 | `"  if($keep){"` |
|     - | 2980 | `"   $r = __reflect_new_no_ctor('ReflectionAttribute');"` |
|     - | 2981 | `"   $r->__init($a['name'], $spec, $idx, $target, $counts[strtolower($a['name'])] > 1);"` |
|     - | 2982 | `"   $out[] = $r;"` |
|     - | 2983 | `"  }"` |
|     - | 2984 | `"  $idx++;"` |
|     - | 2985 | `" }"` |
|     - | 2986 | `" return $out;"` |
|     - | 2987 | `"}"` |
|     - | 2988 | `"final class ReflectionAttribute {"` |
|     - | 2989 | `" const IS_INSTANCEOF = 2;"` |
|     - | 2990 | `" protected $__name = '';"` |
|     - | 2991 | `" protected $__spec = null;"` |
|     - | 2992 | `" protected $__idx = 0;"` |
|     - | 2993 | `" protected $__target = 0;"` |
|     - | 2994 | `" protected $__rep = false;"` |
|     - | 2995 | `" public function __construct(){"` |
|     - | 2996 | `"  throw new Error('Call to private ReflectionAttribute::__construct() from global scope');"` |
|     - | 2997 | `" }"` |
|     - | 2998 | `" public function __init($name, $spec, $idx, $target, $rep){"` |
|     - | 2999 | `"  $this->__name = $name;"` |
|     - | 3000 | `"  $this->__spec = $spec;"` |
|     - | 3001 | `"  $this->__idx = $idx;"` |
|     - | 3002 | `"  $this->__target = $target;"` |
|     - | 3003 | `"  $this->__rep = $rep;"` |
|     - | 3004 | `" }"` |
|     - | 3005 | `" public function getName(){ return $this->__name; }"` |
|     - | 3006 | `" public function getTarget(){ return $this->__target; }"` |
|     - | 3007 | `" public function isRepeated(){ return $this->__rep; }"` |
|     - | 3008 | `" public function getArguments(){"` |
|     - | 3009 | `"  $a = __reflect_attr_args($this->__spec[0], $this->__spec[1], $this->__spec[2], $this->__spec[3], $this->__idx);"` |
|     - | 3010 | `"  return $a === null ? array() : $a;"` |
|     - | 3011 | `" }"` |
|     - | 3012 | `" public function newInstance(){"` |
|     - | 3013 | `"  $name = $this->__name;"` |
|     - | 3014 | `"  $ci = __reflect_class_info($name);"` |
|     - | 3015 | `"  if($ci === null){"` |
|     - | 3016 | `"   throw new Error('Attribute class \"'.$name.'\" not found');"` |
|     - | 3017 | `"  }"` |
|     - | 3018 | `"  $name = $ci['name'];"` |
|     - | 3019 | `"  $decl = null;"` |
|     - | 3020 | `"  $didx = 0;"` |
|     - | 3021 | `"  foreach($ci['attrs'] as $a){"` |
|     - | 3022 | `"   if(strtolower($a['name']) === 'attribute'){ $decl = $didx; break; }"` |
|     - | 3023 | `"   $didx++;"` |
|     - | 3024 | `"  }"` |
|     - | 3025 | `"  if($decl === null){"` |
|     - | 3026 | `"   throw new Error('Attempting to use non-attribute class \"'.$name.'\" as attribute');"` |
|     - | 3027 | `"  }"` |
|     - | 3028 | `"  $dargs = __reflect_attr_args('class', $name, null, 0, $decl);"` |
|     - | 3029 | `"  $flags = 127;"` |
|     - | 3030 | `"  if(is_array($dargs)){"` |
|     - | 3031 | `"   if(isset($dargs[0])){ $flags = $dargs[0]; }"` |
|     - | 3032 | `"   else if(isset($dargs['flags'])){ $flags = $dargs['flags']; }"` |
|     - | 3033 | `"  }"` |
|     - | 3034 | `"  if(($flags & $this->__target) === 0){"` |
|     - | 3035 | `"   $tnames = array(1 => 'class', 2 => 'function', 4 => 'method', 8 => 'property',"` |
|     - | 3036 | `"    16 => 'class constant', 32 => 'parameter', 64 => 'constant');"` |
|     - | 3037 | `"   throw new Error('Attribute \"'.$name.'\" cannot target '.$tnames[$this->__target]"` |
|     - | 3038 | `"    .' (allowed targets: '.__reflect_target_names($flags).')');"` |
|     - | 3039 | `"  }"` |
|     - | 3040 | `"  if($this->__rep && ($flags & 128) === 0){"` |
|     - | 3041 | `"   throw new Error('Attribute \"'.$name.'\" must not be repeated');"` |
|     - | 3042 | `"  }"` |
|     - | 3043 | `"  return __reflect_new_instance($name, $this->getArguments());"` |
|     - | 3044 | `" }"` |
|     - | 3045 | `" public function __toString(){"` |
|     - | 3046 | `"  return 'Attribute [ '.$this->__name.' ]';"` |
|     - | 3047 | `" }"` |
|     - | 3048 | `"}"` |
|     - | 3049 | `;` |
|     - | 3050 | `/*` |
|     - | 3051 | ` * Chunk 8: signature-table support. Internal (C builtin) functions carry a` |
|     - | 3052 | ` * PHP-style parameter-list string; these helpers parse it into the same` |
|     - | 3053 | ` * param-meta shape user functions get, so ReflectionFunction and` |
|     - | 3054 | ` * ReflectionParameter work uniformly over builtins.` |
|     - | 3055 | ` */` |
|     - | 3056 | `static const char zReflectLib8[] =` |
|     - | 3057 | `"function __reflect_sig_split($sig){"` |
|     - | 3058 | `" $parts = array();"` |
|     - | 3059 | `" $cur = '';"` |
|     - | 3060 | `" $q = false;"` |
|     - | 3061 | `" $n = strlen($sig);"` |
|     - | 3062 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3063 | `"  $ch = $sig[$k];"` |
|     - | 3064 | `"  if($q){"` |
|     - | 3065 | `"   $cur .= $ch;"` |
|     - | 3066 | `"   if($ch === chr(92) && $k + 1 < $n){ $cur .= $sig[$k+1]; $k++; }"` |
|     - | 3067 | `"   else if($ch === chr(39)){ $q = false; }"` |
|     - | 3068 | `"  }else if($ch === chr(39)){ $q = true; $cur .= $ch; }"` |
|     - | 3069 | `"  else if($ch === ',' ){ $parts[] = trim($cur); $cur = ''; }"` |
|     - | 3070 | `"  else{ $cur .= $ch; }"` |
|     - | 3071 | `" }"` |
|     - | 3072 | `" if(trim($cur) !== ''){ $parts[] = trim($cur); }"` |
|     - | 3073 | `" return $parts;"` |
|     - | 3074 | `"}"` |
|     - | 3075 | `"function __reflect_sig_scalar($t){"` |
|     - | 3076 | `" if($t === '?'){ return array(false, null); }"` |
|     - | 3077 | `" if($t === 'NULL' \|\| $t === 'null'){ return array(true, null); }"` |
|     - | 3078 | `" if($t === 'true'){ return array(true, true); }"` |
|     - | 3079 | `" if($t === 'false'){ return array(true, false); }"` |
|     - | 3080 | `" if(is_numeric($t)){"` |
|     - | 3081 | `"  if(strpos($t, '.') === false && stripos($t, 'e') === false && strpos($t, 'x') === false){"` |
|     - | 3082 | `"   return array(true, (int)$t);"` |
|     - | 3083 | `"  }"` |
|     - | 3084 | `"  return array(true, (float)$t);"` |
|     - | 3085 | `" }"` |
|     - | 3086 | `" if(strlen($t) >= 2 && $t[0] === chr(39) && $t[strlen($t)-1] === chr(39)){"` |
|     - | 3087 | `"  $body = substr($t, 1, strlen($t) - 2);"` |
|     - | 3088 | `"  return array(true, strtr($body, array(chr(92).chr(39) => chr(39), chr(92).chr(92) => chr(92))));"` |
|     - | 3089 | `" }"` |
|     - | 3090 | `" return array(false, null);"` |
|     - | 3091 | `"}"` |
|     - | 3092 | `"function __reflect_parse_sig($sig){"` |
|     - | 3093 | `" $params = array();"` |
|     - | 3094 | `" $pos = 0;"` |
|     - | 3095 | `" foreach(__reflect_sig_split($sig) as $part){"` |
|     - | 3096 | `"  $deftext = null;"` |
|     - | 3097 | `"  $q = false;"` |
|     - | 3098 | `"  $n = strlen($part);"` |
|     - | 3099 | `"  for($k = 0; $k < $n; $k++){"` |
|     - | 3100 | `"   $ch = $part[$k];"` |
|     - | 3101 | `"   if($q){"` |
|     - | 3102 | `"    if($ch === chr(92)){ $k++; }"` |
|     - | 3103 | `"    else if($ch === chr(39)){ $q = false; }"` |
|     - | 3104 | `"   }else if($ch === chr(39)){ $q = true; }"` |
|     - | 3105 | `"   else if($ch === '=' ){"` |
|     - | 3106 | `"    $deftext = trim(substr($part, $k + 1));"` |
|     - | 3107 | `"    $part = trim(substr($part, 0, $k));"` |
|     - | 3108 | `"    break;"` |
|     - | 3109 | `"   }"` |
|     - | 3110 | `"  }"` |
|     - | 3111 | `"  $variadic = strpos($part, '...') !== false;"` |
|     - | 3112 | `"  $byref = strpos($part, '&') !== false;"` |
|     - | 3113 | `"  $d = strpos($part, '$');"` |
|     - | 3114 | `"  $name = $d === false ? $part : substr($part, $d + 1);"` |
|     - | 3115 | `"  $typetext = null;"` |
|     - | 3116 | `"  $sp = strpos($part, ' ');"` |
|     - | 3117 | `"  if($sp !== false && $d !== false && $sp < $d){ $typetext = substr($part, 0, $sp); }"` |
|     - | 3118 | `"  $nullable = $typetext !== null && ($typetext[0] === '?' \|\| stripos($typetext, 'null') !== false);"` |
|     - | 3119 | `"  $params[] = array('name' => $name, 'pos' => $pos, 'byref' => $byref,"` |
|     - | 3120 | `"   'variadic' => $variadic, 'hasdef' => $deftext !== null, 'nullable' => $nullable,"` |
|     - | 3121 | `"   'promoted' => false, 'typetext' => $typetext, 'attrs' => array(), 'deftext' => $deftext);"` |
|     - | 3122 | `"  $pos++;"` |
|     - | 3123 | `" }"` |
|     - | 3124 | `" return $params;"` |
|     - | 3125 | `"}"` |
|     - | 3126 | `"function __reflect_sig_fixup($i){"` |
|     - | 3127 | `" if($i === null){ return $i; }"` |
|     - | 3128 | `" if(isset($i['ret2'])){ $i['rettext'] = $i['ret2']; }"` |
|     - | 3129 | `" if(!isset($i['sig']) \|\| $i['sig'] === ''){ return $i; }"` |
|     - | 3130 | `" $i['params'] = __reflect_parse_sig($i['sig']);"` |
|     - | 3131 | `" $i['minarg'] = -1;"` |
|     - | 3132 | `" $v = false;"` |
|     - | 3133 | `" foreach($i['params'] as $p){ if($p['variadic']){ $v = true; } }"` |
|     - | 3134 | `" $i['variadic'] = $v;"` |
|     - | 3135 | `" return $i;"` |
|     - | 3136 | `"}"` |
|     - | 3137 | `;` |
|     - | 3138 | `/*` |
|     - | 3139 | ` * Chunk 9: PHP's Reflection export format (__toString on every Reflector).` |
|     - | 3140 | ` * Built entirely from the public reflection API of the target objects.` |
|     - | 3141 | ` */` |
|     - | 3142 | `static const char zReflectLib9[] =` |
|     - | 3143 | `"function __reflect_export_value($v){"` |
|     - | 3144 | `" if($v === null){ return 'NULL'; }"` |
|     - | 3145 | `" if($v === true){ return 'true'; }"` |
|     - | 3146 | `" if($v === false){ return 'false'; }"` |
|     - | 3147 | `" if(is_string($v)){ return chr(39).$v.chr(39); }"` |
|     - | 3148 | `" if(is_array($v)){"` |
|     - | 3149 | `"  $parts = array();"` |
|     - | 3150 | `"  $isList = true;"` |
|     - | 3151 | `"  $next = 0;"` |
|     - | 3152 | `"  foreach($v as $k => $x){"` |
|     - | 3153 | `"   if($k !== $next){ $isList = false; break; }"` |
|     - | 3154 | `"   $next++;"` |
|     - | 3155 | `"  }"` |
|     - | 3156 | `"  foreach($v as $k => $x){"` |
|     - | 3157 | `"   $parts[] = $isList ? __reflect_export_value($x)"` |
|     - | 3158 | `"    : (__reflect_export_value($k).' => '.__reflect_export_value($x));"` |
|     - | 3159 | `"  }"` |
|     - | 3160 | `"  return '['.implode(', ', $parts).']';"` |
|     - | 3161 | `" }"` |
|     - | 3162 | `" return (string)$v;"` |
|     - | 3163 | `"}"` |
|     - | 3164 | `"function __reflect_export_param($p){"` |
|     - | 3165 | `" $s = 'Parameter #'.$p->getPosition().' [ <'.($p->isOptional() ? 'optional' : 'required').'> ';"` |
|     - | 3166 | `" $t = $p->getType();"` |
|     - | 3167 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3168 | `" if($p->isPassedByReference()){ $s .= '&'; }"` |
|     - | 3169 | `" if($p->isVariadic()){ $s .= '...'; }"` |
|     - | 3170 | `" $s .= '$'.$p->getName();"` |
|     - | 3171 | `" if($p->isDefaultValueAvailable()){"` |
|     - | 3172 | `"  try{ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3173 | `"  catch(ReflectionException $e){ $s .= ' = <default>'; }"` |
|     - | 3174 | `" }"` |
|     - | 3175 | `" return $s.' ]';"` |
|     - | 3176 | `"}"` |
|     - | 3177 | `"function __reflect_export_prop($p){"` |
|     - | 3178 | `" $s = 'Property [ ';"` |
|     - | 3179 | `" $s .= $p->isPrivate() ? 'private ' : ($p->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3180 | `" if($p->isStatic()){ $s .= 'static '; }"` |
|     - | 3181 | `" if($p->isReadOnly()){ $s .= 'readonly '; }"` |
|     - | 3182 | `" $t = $p->getType();"` |
|     - | 3183 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3184 | `" $s .= '$'.$p->getName();"` |
|     - | 3185 | `" if($p->hasDefaultValue()){ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3186 | `" return $s.' ]'.chr(10);"` |
|     - | 3187 | `"}"` |
|     - | 3188 | `"function __reflect_export_cconst($c){"` |
|     - | 3189 | `" $v = $c->getValue();"` |
|     - | 3190 | `" if(is_int($v)){ $t = 'int'; }"` |
|     - | 3191 | `" else if(is_string($v)){ $t = 'string'; }"` |
|     - | 3192 | `" else if(is_float($v)){ $t = 'float'; }"` |
|     - | 3193 | `" else if(is_bool($v)){ $t = 'bool'; }"` |
|     - | 3194 | `" else if(is_array($v)){ $t = 'array'; }"` |
|     - | 3195 | `" else{ $t = 'null'; }"` |
|     - | 3196 | `" $vs = is_array($v) ? 'Array' : (is_bool($v) ? ($v ? '1' : '') : (string)$v);"` |
|     - | 3197 | `" $vis = $c->isPrivate() ? 'private' : ($c->isProtected() ? 'protected' : 'public');"` |
|     - | 3198 | `" return 'Constant [ '.$vis.' '.$t.' '.$c->name.' ] { '.$vs.' }'.chr(10);"` |
|     - | 3199 | `"}"` |
|     - | 3200 | `"function __reflect_export_fnabs($r, $indent){"` |
|     - | 3201 | `" $tags = $r->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3202 | `" if($r instanceof ReflectionMethod){"` |
|     - | 3203 | `"  if($r->isConstructor()){ $tags .= ', ctor'; }"` |
|     - | 3204 | `"  else if($r->isDestructor()){ $tags .= ', dtor'; }"` |
|     - | 3205 | `"  $decl = $r->getDeclaringClass()->name;"` |
|     - | 3206 | `"  if(strtolower($decl) !== strtolower($r->class)){ $tags .= ', inherits '.$decl; }"` |
|     - | 3207 | `"  else if($r->hasPrototype()){ $tags .= ', prototype '.$r->getPrototype()->class; }"` |
|     - | 3208 | `"  $head = 'Method [ <'.$tags.'> ';"` |
|     - | 3209 | `"  if($r->isAbstract()){ $head .= 'abstract '; }"` |
|     - | 3210 | `"  if($r->isFinal()){ $head .= 'final '; }"` |
|     - | 3211 | `"  if($r->isStatic()){ $head .= 'static '; }"` |
|     - | 3212 | `"  $head .= $r->isPrivate() ? 'private ' : ($r->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3213 | `"  $head .= 'method '.$r->name.' ]';"` |
|     - | 3214 | `" }else{"` |
|     - | 3215 | `"  $kind = $r->isClosure() ? 'Closure' : 'Function';"` |
|     - | 3216 | `"  $head = $kind.' [ <'.$tags.'> function '.$r->name.' ]';"` |
|     - | 3217 | `" }"` |
|     - | 3218 | `" $s = $head.' {'.chr(10);"` |
|     - | 3219 | `" if(!$r->isInternal()){"` |
|     - | 3220 | `"  $s .= '  @@ '.$r->getFileName().' '.$r->getStartLine().' - '.$r->getEndLine().chr(10);"` |
|     - | 3221 | `" }"` |
|     - | 3222 | `" $ps = $r->getParameters();"` |
|     - | 3223 | `" $ret = $r->getReturnType();"` |
|     - | 3224 | `" if(count($ps) > 0 \|\| $ret !== null){"` |
|     - | 3225 | `"  $s .= chr(10).'  - Parameters ['.count($ps).'] {'.chr(10);"` |
|     - | 3226 | `"  foreach($ps as $p){ $s .= '    '.__reflect_export_param($p).chr(10); }"` |
|     - | 3227 | `"  $s .= '  }'.chr(10);"` |
|     - | 3228 | `" }"` |
|     - | 3229 | `" if($ret !== null){ $s .= '  - Return [ '.(string)$ret.' ]'.chr(10); }"` |
|     - | 3230 | `" $s .= '}'.chr(10);"` |
|     - | 3231 | `" if($indent === ''){ return $s; }"` |
|     - | 3232 | `" $lines = explode(chr(10), $s);"` |
|     - | 3233 | `" $out = '';"` |
|     - | 3234 | `" $n = count($lines);"` |
|     - | 3235 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3236 | `"  if($lines[$k] === '' && $k === $n - 1){ break; }"` |
|     - | 3237 | `"  $out .= ($lines[$k] === '' ? '' : $indent.$lines[$k]).chr(10);"` |
|     - | 3238 | `" }"` |
|     - | 3239 | `" return $out;"` |
|     - | 3240 | `"}"` |
|     - | 3241 | `"function __reflect_export_class($rc){"` |
|     - | 3242 | `" $tags = $rc->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3243 | `" if($rc->isInterface()){"` |
|     - | 3244 | `"  $head = 'Interface [ <'.$tags.'> interface '.$rc->name.' ]';"` |
|     - | 3245 | `" }else{"` |
|     - | 3246 | `"  $mods = '';"` |
|     - | 3247 | `"  if($rc->isAbstract()){ $mods .= 'abstract '; }"` |
|     - | 3248 | `"  if($rc->isFinal()){ $mods .= 'final '; }"` |
|     - | 3249 | `"  $head = 'Class [ <'.$tags.'> '.$mods.'class '.$rc->name;"` |
|     - | 3250 | `"  $par = $rc->getParentClass();"` |
|     - | 3251 | `"  if($par !== false){ $head .= ' extends '.$par->name; }"` |
|     - | 3252 | `"  $ifs = $rc->getInterfaceNames();"` |
|     - | 3253 | `"  if(count($ifs) > 0){ $head .= ' implements '.implode(', ', $ifs); }"` |
|     - | 3254 | `"  $head .= ' ]';"` |
|     - | 3255 | `" }"` |
|     - | 3256 | `" $s = $head.' {'.chr(10);"` |
|     - | 3257 | `" if(!$rc->isInternal()){"` |
|     - | 3258 | `"  $s .= '  @@ '.$rc->getFileName().' '.$rc->getStartLine().'-'.$rc->getEndLine().chr(10);"` |
|     - | 3259 | `" }"` |
|     - | 3260 | `" $consts = $rc->getReflectionConstants();"` |
|     - | 3261 | `" $s .= chr(10).'  - Constants ['.count($consts).'] {'.chr(10);"` |
|     - | 3262 | `" foreach($consts as $c){ $s .= '    '.__reflect_export_cconst($c); }"` |
|     - | 3263 | `" $s .= '  }'.chr(10);"` |
|     - | 3264 | `" $sp = array();"` |
|     - | 3265 | `" $ip = array();"` |
|     - | 3266 | `" foreach($rc->getProperties() as $p){"` |
|     - | 3267 | `"  if($p->isStatic()){ $sp[] = $p; }else{ $ip[] = $p; }"` |
|     - | 3268 | `" }"` |
|     - | 3269 | `" $sm = array();"` |
|     - | 3270 | `" $im = array();"` |
|     - | 3271 | `" foreach($rc->getMethods() as $m){"` |
|     - | 3272 | `"  if($m->isStatic()){ $sm[] = $m; }else{ $im[] = $m; }"` |
|     - | 3273 | `" }"` |
|     - | 3274 | `" $s .= chr(10).'  - Static properties ['.count($sp).'] {'.chr(10);"` |
|     - | 3275 | `" foreach($sp as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3276 | `" $s .= '  }'.chr(10);"` |
|     - | 3277 | `" $s .= chr(10).'  - Static methods ['.count($sm).'] {'.chr(10);"` |
|     - | 3278 | `" $first = true;"` |
|     - | 3279 | `" foreach($sm as $m){"` |
|     - | 3280 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3281 | `"  $first = false;"` |
|     - | 3282 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3283 | `" }"` |
|     - | 3284 | `" $s .= '  }'.chr(10);"` |
|     - | 3285 | `" $s .= chr(10).'  - Properties ['.count($ip).'] {'.chr(10);"` |
|     - | 3286 | `" foreach($ip as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3287 | `" $s .= '  }'.chr(10);"` |
|     - | 3288 | `" $s .= chr(10).'  - Methods ['.count($im).'] {'.chr(10);"` |
|     - | 3289 | `" $first = true;"` |
|     - | 3290 | `" foreach($im as $m){"` |
|     - | 3291 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3292 | `"  $first = false;"` |
|     - | 3293 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3294 | `" }"` |
|     - | 3295 | `" $s .= '  }'.chr(10);"` |
|     - | 3296 | `" return $s.'}'.chr(10);"` |
|     - | 3297 | `"}"` |
|     - | 3298 | `;` |
|     - | 3299 | `/*` |
|     - | 3300 | ` * Register the __reflect_* thunks and compile the Reflection library.` |
|     - | 3301 | ` * Called from PH7_VmInit while pVm->bCompilingBuiltin is set, right after` |
|     - | 3302 | ` * the core builtin chunks (Exception and friends must exist already).` |
|     - | 3303 | ` */` |
|  3884 | 3304 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|     5 | 3305 | `{` |
|     - | 3306 | `	static const struct {` |
|     - | 3307 | `		const char *zName;` |
|     - | 3308 | `		ProchHostFunction xFunc;` |
|     - | 3309 | `	} aFunc[] = {` |
|     - | 3310 | `		{ "__reflect_class_info",     vm_builtin_reflect_class_info },` |
|     - | 3311 | `		{ "__reflect_const_value",    vm_builtin_reflect_const_value },` |
|     - | 3312 | `		{ "__reflect_static_value",   vm_builtin_reflect_static_value },` |
|     - | 3313 | `		{ "__reflect_static_set",     vm_builtin_reflect_static_set },` |
|     - | 3314 | `		{ "__reflect_prop_default",   vm_builtin_reflect_prop_default },` |
|     - | 3315 | `		{ "__reflect_new_instance",   vm_builtin_reflect_new_instance },` |
|     - | 3316 | `		{ "__reflect_new_no_ctor",    vm_builtin_reflect_new_no_ctor },` |
|     - | 3317 | `		{ "__reflect_func_info",      vm_builtin_reflect_func_info },` |
|     - | 3318 | `		{ "__reflect_param_default",  vm_builtin_reflect_param_default },` |
|     - | 3319 | `		{ "__reflect_param_defconst", vm_builtin_reflect_param_defconst },` |
|     - | 3320 | `		{ "__reflect_invoke",         vm_builtin_reflect_invoke },` |
|     - | 3321 | `		{ "__reflect_closure",        vm_builtin_reflect_closure },` |
|     - | 3322 | `		{ "__reflect_prop_read",      vm_builtin_reflect_prop_read },` |
|     - | 3323 | `		{ "__reflect_prop_write",     vm_builtin_reflect_prop_write },` |
|     - | 3324 | `		{ "__reflect_prop_state",     vm_builtin_reflect_prop_state },` |
|     - | 3325 | `		{ "__reflect_dyn_props",      vm_builtin_reflect_dyn_props },` |
|     - | 3326 | `		{ "__reflect_gen_info",       vm_builtin_reflect_gen_info },` |
|     - | 3327 | `		{ "__reflect_gen_exec",       vm_builtin_reflect_gen_exec },` |
|     - | 3328 | `		{ "__reflect_const_info",     vm_builtin_reflect_const_info },` |
|     - | 3329 | `		{ "__reflect_ref_id",         vm_builtin_reflect_ref_id },` |
|     - | 3330 | `		{ "__reflect_attr_args",      vm_builtin_reflect_attr_args },` |
|     - | 3331 | `	};` |
|     - | 3332 | `	sxu32 n;` |
|     - | 3333 | `	sxi32 rc;` |
| 85453 | 3334 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 81569 | 3335 | `		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);` |
| 40787 | 3336 | `	}` |
|  3889 | 3337 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib1, sizeof(zReflectLib1)-1);` |
|  3889 | 3338 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3339 | `		return rc;` |
|     - | 3340 | `	}` |
|  3889 | 3341 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib2, sizeof(zReflectLib2)-1);` |
|  3889 | 3342 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3343 | `		return rc;` |
|     - | 3344 | `	}` |
|  3889 | 3345 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib3, sizeof(zReflectLib3)-1);` |
|  3889 | 3346 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3347 | `		return rc;` |
|     - | 3348 | `	}` |
|  3889 | 3349 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib4, sizeof(zReflectLib4)-1);` |
|  3889 | 3350 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3351 | `		return rc;` |
|     - | 3352 | `	}` |
|  3889 | 3353 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib5, sizeof(zReflectLib5)-1);` |
|  3889 | 3354 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3355 | `		return rc;` |
|     - | 3356 | `	}` |
|  3889 | 3357 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib6, sizeof(zReflectLib6)-1);` |
|  3889 | 3358 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3359 | `		return rc;` |
|     - | 3360 | `	}` |
|  3889 | 3361 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib7, sizeof(zReflectLib7)-1);` |
|  3889 | 3362 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3363 | `		return rc;` |
|     - | 3364 | `	}` |
|  3889 | 3365 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib8, sizeof(zReflectLib8)-1);` |
|  3889 | 3366 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3367 | `		return rc;` |
|     - | 3368 | `	}` |
|  3889 | 3369 | `	return PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib9, sizeof(zReflectLib9)-1);` |
|  1947 | 3370 | `}` |
|     - | 3371 |  |
