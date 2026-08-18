# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1034/1208 lines (85.60%)

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
|  1432 |   31 | `static ph7_class * ReflectResolveClass(ph7_vm *pVm, ph7_value *pArg)` |
|     1 |   32 | `{` |
|     - |   33 | `	ph7_class *pClass;` |
|  1433 |   34 | `	pClass = PH7_VmExtractClassFromValue(pVm, pArg);` |
|  1433 |   35 | `	if( pClass == 0 && ph7_value_is_string(pArg) ){` |
|     - |   36 | `		const char *zName;` |
|     - |   37 | `		int nLen;` |
|    13 |   38 | `		zName = ph7_value_to_string(pArg, &nLen);` |
|    13 |   39 | `		if( nLen > 0 ){` |
|    13 |   40 | `			pClass = PH7_VmTriggerAutoload(pVm, zName, (sxu32)nLen, FALSE);` |
|     6 |   41 | `		}` |
|     6 |   42 | `	}` |
|  1433 |   43 | `	return pClass;` |
|     1 |   44 | `}` |
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
| 46086 |   63 | `static void ReflectMapAddBool(ph7_context *pCtx, ph7_value *pMap, const char *zKey, int b)` |
|     1 |   64 | `{` |
| 46087 |   65 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 46087 |   66 | `	if( p == 0 ){ return; }` |
| 46087 |   67 | `	ph7_value_bool(p, b);` |
| 46087 |   68 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
| 23044 |   69 | `}` |
| 17686 |   70 | `static void ReflectMapAddInt(ph7_context *pCtx, ph7_value *pMap, const char *zKey, sxi64 iVal)` |
|     1 |   71 | `{` |
| 17687 |   72 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 17687 |   73 | `	if( p == 0 ){ return; }` |
| 17687 |   74 | `	ph7_value_int64(p, iVal);` |
| 17687 |   75 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  8844 |   76 | `}` |
| 12886 |   77 | `static void ReflectMapAddStr(ph7_context *pCtx, ph7_value *pMap, const char *zKey,` |
|     - |   78 | `	const char *zVal, int nVal)` |
|     1 |   79 | `{` |
| 12887 |   80 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 12887 |   81 | `	if( p == 0 ){ return; }` |
| 12887 |   82 | `	ph7_value_string(p, zVal, nVal);` |
| 12887 |   83 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  6444 |   84 | `}` |
|  4350 |   85 | `static void ReflectMapAddNull(ph7_context *pCtx, ph7_value *pMap, const char *zKey)` |
|     1 |   86 | `{` |
|  4351 |   87 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  4351 |   88 | `	if( p == 0 ){ return; }` |
|  4351 |   89 | `	ph7_value_null(p);` |
|  4351 |   90 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  2176 |   91 | `}` |
|     - |   92 | `/* Add an entry under a dynamic (SyString) key. */` |
|  5080 |   93 | `static void ReflectMapAddDyn(ph7_context *pCtx, ph7_value *pMap,` |
|     - |   94 | `	const SyString *pKey, ph7_value *pVal)` |
|     1 |   95 | `{` |
|  5081 |   96 | `	ph7_value *pK = ph7_context_new_scalar(pCtx);` |
|  5081 |   97 | `	if( pK == 0 ){ return; }` |
|  5081 |   98 | `	ph7_value_string(pK, pKey->zString, (int)pKey->nByte);` |
|  5081 |   99 | `	ph7_array_add_elem(pMap, pK, pVal);` |
|  2541 |  100 | `}` |
|     - |  101 | `/* Emit the declared #[...] attributes of a target as a summary list:` |
|     - |  102 | ` * [ {name, line} ... ]. Argument values stay lazy — the PHP layer pulls` |
|     - |  103 | ` * them through __reflect_attr_args when ReflectionAttribute needs them. */` |
|  5770 |  104 | `static void ReflectMapAddAttrs(ph7_context *pCtx, ph7_value *pMap, SySet *pAttrs)` |
|     1 |  105 | `{` |
|  5771 |  106 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|  5771 |  107 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|     - |  108 | `	sxu32 n;` |
|  5771 |  109 | `	if( pList == 0 ){` |
|   ! 0 |  110 | `		return;` |
|     - |  111 | `	}` |
|  6059 |  112 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|   289 |  113 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|   289 |  114 | `		if( pMeta == 0 ){ break; }` |
|   289 |  115 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aA[n].sName), (int)SyStringLength(&aA[n].sName));` |
|   289 |  116 | `		ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)aA[n].nLine);` |
|   289 |  117 | `		ph7_array_add_elem(pList, 0, pMeta);` |
|   145 |  118 | `	}` |
|  5771 |  119 | `	ph7_array_add_strkey_elem(pMap, "attrs", pList);` |
|  2886 |  120 | `}` |
|     - |  121 | `/* Emit a doc-comment field: the text when present, else boolean false` |
|     - |  122 | ` * (getDocComment()'s exact return contract). */` |
|  4480 |  123 | `static void ReflectMapAddDoc(ph7_context *pCtx, ph7_value *pMap, const SyString *pDoc)` |
|     1 |  124 | `{` |
|  4481 |  125 | `	if( SyStringLength(pDoc) > 0 ){` |
|    63 |  126 | `		ReflectMapAddStr(pCtx, pMap, "doc", SyStringData(pDoc), (int)SyStringLength(pDoc));` |
|    32 |  127 | `	}else{` |
|  4419 |  128 | `		ReflectMapAddBool(pCtx, pMap, "doc", 0);` |
|     - |  129 | `	}` |
|  4481 |  130 | `}` |
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
|  1220 |  164 | `static void ReflectCollectInterfaces(ph7_class *pClass, SySet *pOut, int iDepth)` |
|     1 |  165 | `{` |
|     - |  166 | `	ph7_class **apIface;` |
|     - |  167 | `	sxu32 n;` |
|  1221 |  168 | `	if( pClass == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|   ! 0 |  169 | `		return;` |
|     - |  170 | `	}` |
|  1221 |  171 | `	if( pClass->pBase ){` |
|   283 |  172 | `		ReflectCollectInterfaces(pClass->pBase, pOut, iDepth + 1);` |
|   141 |  173 | `	}` |
|  1221 |  174 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  1687 |  175 | `	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){` |
|   467 |  176 | `		ReflectAddInterface(apIface[n], pOut, iDepth + 1);` |
|   234 |  177 | `	}` |
|   611 |  178 | `}` |
|     - |  179 | `/*` |
|     - |  180 | ` * Deepest base class whose method table maps the same name to the very` |
|     - |  181 | ` * same ph7_class_method pointer: inheritance shares member pointers` |
|     - |  182 | ` * (PH7_ClassInherit), so this identifies the declaring class. Methods` |
|     - |  183 | ` * copied in from traits are not on the pBase chain and thus report the` |
|     - |  184 | ` * using class, which is what PHP reports too.` |
|     - |  185 | ` */` |
|  4704 |  186 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth)` |
|     1 |  187 | `{` |
|  4705 |  188 | `	ph7_class *pDecl = pClass;` |
|  4705 |  189 | `	ph7_class *pBase = pClass->pBase;` |
|  4705 |  190 | `	int iDepth = 0;` |
|  5785 |  191 | `	while( pBase && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
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
|  4705 |  202 | `	return pDecl;` |
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
|   950 |  233 | `static int vm_builtin_reflect_class_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  234 | `{` |
|   951 |  235 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  236 | `	ph7_class *pClass;` |
|     - |  237 | `	ph7_value *pInfo, *pConsts, *pProps, *pMethods, *pList;` |
|     - |  238 | `	SyHashEntry *pEntry;` |
|     - |  239 | `	SySet aIfaceSet;` |
|   951 |  240 | `	sxi32 iCtorVis = 0, iCloneVis = 0;` |
|   951 |  241 | `	int bIterable = 0;` |
|     - |  242 | `	sxu32 n;` |
|   951 |  243 | `	if( nArg < 1 ){` |
|   ! 0 |  244 | `		ph7_result_null(pCtx);` |
|   ! 0 |  245 | `		return PH7_OK;` |
|     - |  246 | `	}` |
|   951 |  247 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|   951 |  248 | `	if( pClass == 0 ){` |
|    13 |  249 | `		ph7_result_null(pCtx);` |
|    13 |  250 | `		return PH7_OK;` |
|     - |  251 | `	}` |
|   939 |  252 | `	pInfo = ph7_context_new_array(pCtx);` |
|   939 |  253 | `	pConsts = ph7_context_new_array(pCtx);` |
|   939 |  254 | `	pProps = ph7_context_new_array(pCtx);` |
|   939 |  255 | `	pMethods = ph7_context_new_array(pCtx);` |
|   939 |  256 | `	if( pInfo == 0 \|\| pConsts == 0 \|\| pProps == 0 \|\| pMethods == 0 ){` |
|   ! 0 |  257 | `		ph7_result_null(pCtx);` |
|   ! 0 |  258 | `		return PH7_OK;` |
|     - |  259 | `	}` |
|   939 |  260 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   939 |  261 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pClass->iFlags & PH7_CLASS_INTERNAL) != 0);` |
|   939 |  262 | `	ReflectMapAddBool(pCtx, pInfo, "interface", (pClass->iFlags & PH7_CLASS_INTERFACE) != 0);` |
|   939 |  263 | `	ReflectMapAddBool(pCtx, pInfo, "trait", (pClass->iFlags & PH7_CLASS_TRAIT) != 0);` |
|   939 |  264 | `	ReflectMapAddBool(pCtx, pInfo, "abstract", (pClass->iFlags & PH7_CLASS_ABSTRACT) != 0);` |
|   939 |  265 | `	ReflectMapAddBool(pCtx, pInfo, "final", (pClass->iFlags & PH7_CLASS_FINAL) != 0);` |
|   939 |  266 | `	ReflectMapAddBool(pCtx, pInfo, "readonly", (pClass->iFlags & PH7_CLASS_READONLY) != 0);` |
|   939 |  267 | `	ReflectMapAddBool(pCtx, pInfo, "enum", (pClass->iFlags & PH7_CLASS_ENUM) != 0);` |
|   939 |  268 | `	if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|   ! 0 |  269 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "int", (int)sizeof("int")-1);` |
|   939 |  270 | `	}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|    67 |  271 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "string", (int)sizeof("string")-1);` |
|    34 |  272 | `	}else{` |
|   873 |  273 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "", 0);` |
|     - |  274 | `	}` |
|     - |  275 | `	{` |
|     - |  276 | `		/* Enum case names in declaration order (empty list for non-enums) */` |
|   939 |  277 | `		ph7_value *pCases = ph7_context_new_array(pCtx);` |
|   939 |  278 | `		if( pCases ){` |
|   939 |  279 | `			ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     - |  280 | `			sxu32 nCase;` |
|  1099 |  281 | `			for( nCase = 0 ; nCase < SySetUsed(&pClass->aEnumCases) ; nCase++ ){` |
|   161 |  282 | `				ph7_value *pNm = ph7_context_new_scalar(pCtx);` |
|   161 |  283 | `				if( pNm ){` |
|   161 |  284 | `					ph7_value_string(pNm,apCase[nCase]->sName.zString,(int)apCase[nCase]->sName.nByte);` |
|   161 |  285 | `					ph7_array_add_elem(pCases,0,pNm);` |
|    80 |  286 | `				}` |
|    81 |  287 | `			}` |
|   939 |  288 | `			ph7_array_add_strkey_elem(pInfo,"cases",pCases);` |
|   469 |  289 | `		}` |
|     - |  290 | `	}` |
|   939 |  291 | `	if( pClass->pBase ){` |
|   418 |  292 | `		ReflectMapAddStr(pCtx, pInfo, "parent", SyStringData(&pClass->pBase->sName),` |
|   278 |  293 | `			(int)SyStringLength(&pClass->pBase->sName));` |
|   140 |  294 | `	}else{` |
|   661 |  295 | `		ReflectMapAddNull(pCtx, pInfo, "parent");` |
|     - |  296 | `	}` |
|     - |  297 | `	/* Transitive interfaces */` |
|   939 |  298 | `	SySetInit(&aIfaceSet, &pVm->sAllocator, sizeof(ph7_class *));` |
|   939 |  299 | `	ReflectCollectInterfaces(pClass, &aIfaceSet, 0);` |
|   939 |  300 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     - |  301 | `		/* An interface's own parents count as its interface list */` |
|    47 |  302 | `		if( pClass->pBase ){` |
|     9 |  303 | `			ReflectAddInterface(pClass->pBase, &aIfaceSet, 0);` |
|     4 |  304 | `		}` |
|    23 |  305 | `	}` |
|   939 |  306 | `	pList = ph7_context_new_array(pCtx);` |
|   939 |  307 | `	if( pList ){` |
|   939 |  308 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIfaceSet);` |
|  1445 |  309 | `		for( n = 0 ; n < SySetUsed(&aIfaceSet) ; n++ ){` |
|   507 |  310 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|   507 |  311 | `			if( pName == 0 ){ break; }` |
|   507 |  312 | `			ph7_value_string(pName, SyStringData(&apIface[n]->sName), (int)SyStringLength(&apIface[n]->sName));` |
|   507 |  313 | `			ph7_array_add_elem(pList, 0, pName);` |
|   507 |  314 | `			if( pVm->pTraversableClass && apIface[n] == pVm->pTraversableClass ){` |
|    13 |  315 | `				bIterable = 1;` |
|     6 |  316 | `			}` |
|   254 |  317 | `		}` |
|   939 |  318 | `		ph7_array_add_strkey_elem(pInfo, "interfaces", pList);` |
|   469 |  319 | `	}` |
|   939 |  320 | `	SySetRelease(&aIfaceSet);` |
|   939 |  321 | `	ReflectMapAddBool(pCtx, pInfo, "iterable", bIterable);` |
|     - |  322 | `	/* Used traits */` |
|   939 |  323 | `	pList = ph7_context_new_array(pCtx);` |
|   939 |  324 | `	if( pList ){` |
|   939 |  325 | `		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|   951 |  326 | `		for( n = 0 ; n < SySetUsed(&pClass->aTrait) ; n++ ){` |
|    13 |  327 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|    13 |  328 | `			if( pName == 0 ){ break; }` |
|    13 |  329 | `			ph7_value_string(pName, SyStringData(&apTrait[n]->sName), (int)SyStringLength(&apTrait[n]->sName));` |
|    13 |  330 | `			ph7_array_add_elem(pList, 0, pName);` |
|     7 |  331 | `		}` |
|   939 |  332 | `		ph7_array_add_strkey_elem(pInfo, "traits", pList);` |
|   469 |  333 | `	}` |
|     - |  334 | `	/* File / lines: no file recorded => false, like PHP internals */` |
|   939 |  335 | `	if( SyStringLength(&pClass->sFile) > 0 ){` |
|   857 |  336 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));` |
|   429 |  337 | `	}else{` |
|    83 |  338 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - |  339 | `	}` |
|   939 |  340 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pClass->nLine);` |
|   939 |  341 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pClass->nEndLine);` |
|   939 |  342 | `	ReflectMapAddDoc(pCtx, pInfo, &pClass->sDoc);` |
|   939 |  343 | `	ReflectMapAddAttrs(pCtx, pInfo, &pClass->aAttrs);` |
|     - |  344 | `	/* Members are emitted in PHP's reporting order: the class's own members` |
|     - |  345 | `	 * first (declaration order), then each inheritance level's, outward.` |
|     - |  346 | `	 * Per level we iterate the DECLARING class's own hash — subclass hashes` |
|     - |  347 | `	 * interleave inherited pointers unpredictably — and emit buffered` |
|     - |  348 | `	 * entries in reverse, because SyHash lists are LIFO. A pointer-identity` |
|     - |  349 | `	 * lookup in the reflected class's hash filters out members that are not` |
|     - |  350 | `	 * visible there (base privates, overridden entries). */` |
|     - |  351 | `	{` |
|     - |  352 | `		ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];` |
|   939 |  353 | `		ph7_class *pWalk = pClass;` |
|     - |  354 | `		SySet aTmp;` |
|   939 |  355 | `		sxu32 nChain = 0, iLevel, nT;` |
|  2159 |  356 | `		while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){` |
|  1221 |  357 | `			aChain[nChain++] = pWalk;` |
|  1221 |  358 | `			pWalk = pWalk->pBase;` |
|     1 |  359 | `		}` |
|   939 |  360 | `		SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));` |
|  2159 |  361 | `		for( iLevel = 0 ; iLevel < nChain ; iLevel++ ){` |
|  1221 |  362 | `			ph7_class *pLevel = aChain[iLevel];` |
|     - |  363 | `			/* --- Constants and properties (shared attribute table) --- */` |
|  1221 |  364 | `			SySetReset(&aTmp);` |
|  1221 |  365 | `			SyHashResetLoopCursor(&pLevel->hAttr);` |
|  4337 |  366 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hAttr)) != 0 ){` |
|  3117 |  367 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  3117 |  368 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|  3117 |  369 | `				if( iLevel == 0 ){` |
|     - |  370 | `					sxu32 j;` |
|     - |  371 | `					/* Own = declared here or by an off-chain provider (trait) */` |
|  4139 |  372 | `					for( j = 1 ; j < nChain ; j++ ){` |
|  1287 |  373 | `						if( aChain[j] == pDecl ){ break; }` |
|   584 |  374 | `					}` |
|  2973 |  375 | `					if( j < nChain ){ continue; }` |
|  1427 |  376 | `				}else{` |
|     - |  377 | `					SyHashEntry *pSub;` |
|   145 |  378 | `					if( pDecl != pLevel ){ continue; }` |
|     - |  379 | `					/* Must still be the visible member in the reflected class */` |
|   121 |  380 | `					pSub = SyHashGet(&pClass->hAttr, pEntry->pKey, pEntry->nKeyLen);` |
|   121 |  381 | `					if( pSub == 0 \|\| pSub->pUserData != (void *)pAttr ){ continue; }` |
|     - |  382 | `				}` |
|  2973 |  383 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     1 |  384 | `			}` |
|  4193 |  385 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|  2973 |  386 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|  2973 |  387 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;` |
|  2973 |  388 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|  2973 |  389 | `				ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|  2973 |  390 | `				if( pMeta == 0 ){ break; }` |
|  2973 |  391 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pAttr->iProtection);` |
|  2973 |  392 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|  2973 |  393 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pAttr->nLine);` |
|  2973 |  394 | `				ReflectMapAddDoc(pCtx, pMeta, &pAttr->sDoc);` |
|  2973 |  395 | `				ReflectMapAddAttrs(pCtx, pMeta, &pAttr->aAttrs);` |
|  2973 |  396 | `				ReflectMapAddBool(pCtx, pMeta, "typed", (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);` |
|  2973 |  397 | `				if( SyStringLength(&pAttr->sTypeName) > 0 ){` |
|   955 |  398 | `					ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&pAttr->sTypeName),` |
|   636 |  399 | `						(int)SyStringLength(&pAttr->sTypeName));` |
|   319 |  400 | `				}else{` |
|  2337 |  401 | `					ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - |  402 | `				}` |
|  2973 |  403 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|   883 |  404 | `					ReflectMapAddBool(pCtx, pMeta, "final", (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   883 |  405 | `					ReflectMapAddBool(pCtx, pMeta, "enumcase", (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0);` |
|   883 |  406 | `					ReflectMapAddDyn(pCtx, pConsts, &pAttr->sName, pMeta);` |
|   442 |  407 | `				}else{` |
|  2091 |  408 | `					ReflectMapAddBool(pCtx, pMeta, "static", (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|  2091 |  409 | `					ReflectMapAddBool(pCtx, pMeta, "readonly", (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0);` |
|  2091 |  410 | `					ReflectMapAddBool(pCtx, pMeta, "privset", (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) != 0);` |
|  2091 |  411 | `					ReflectMapAddBool(pCtx, pMeta, "protset", (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET) != 0);` |
|  2091 |  412 | `					ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&pAttr->aByteCode) > 0);` |
|  2091 |  413 | `					ReflectMapAddDyn(pCtx, pProps, &pAttr->sName, pMeta);` |
|     - |  414 | `				}` |
|  1487 |  415 | `			}` |
|     - |  416 | `			/* --- Methods. The reported name is the hash-entry key: trait` |
|     - |  417 | `			 * aliasing installs a shallow copy under the alias name while` |
|     - |  418 | `			 * sFunc.sName keeps the original, and PHP reports the alias. --- */` |
|  1221 |  419 | `			SySetReset(&aTmp);` |
|  1221 |  420 | `			SyHashResetLoopCursor(&pLevel->hMethod);` |
|  3609 |  421 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){` |
|  2389 |  422 | `				ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  2389 |  423 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|  2389 |  424 | `				if( iLevel == 0 ){` |
|     - |  425 | `					sxu32 j;` |
|  2781 |  426 | `					for( j = 1 ; j < nChain ; j++ ){` |
|  1011 |  427 | `						if( aChain[j] == pDecl ){ break; }` |
|   372 |  428 | `					}` |
|  2039 |  429 | `					if( j < nChain ){ continue; }` |
|   886 |  430 | `				}else{` |
|     - |  431 | `					SyHashEntry *pSub;` |
|   351 |  432 | `					if( pDecl != pLevel ){ continue; }` |
|   315 |  433 | `					pSub = SyHashGet(&pClass->hMethod, pEntry->pKey, pEntry->nKeyLen);` |
|   315 |  434 | `					if( pSub == 0 ){` |
|     - |  435 | `						/* Not in the subclass table: inheritance skips private` |
|     - |  436 | `						 * methods, but PHP still reports them on the subclass` |
|     - |  437 | `						 * (Zend copies privates into the child function table). */` |
|    21 |  438 | `						if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|   ! 0 |  439 | `							continue;` |
|     1 |  440 | `						}` |
|   305 |  441 | `					}else if( pSub->pUserData != (void *)pMeth ){` |
|     - |  442 | `						/* Overridden below this level: already reported */` |
|    27 |  443 | `						continue;` |
|     - |  444 | `					}` |
|     - |  445 | `				}` |
|  2059 |  446 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     1 |  447 | `			}` |
|  3279 |  448 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|  2059 |  449 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|  2059 |  450 | `				ph7_class_method *pMeth = (ph7_class_method *)pE->pUserData;` |
|  2059 |  451 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|     - |  452 | `				ph7_value *pMeta;` |
|     - |  453 | `				SyString sKey;` |
|     - |  454 | `				int bIsAlias;` |
|  2059 |  455 | `				SyStringInitFromBuf(&sKey, (const char *)pE->pKey, pE->nKeyLen);` |
|  4117 |  456 | `				bIsAlias = (sKey.nByte != SyStringLength(&pMeth->sFunc.sName)` |
|  2058 |  457 | `				 \|\| SyMemcmp(sKey.zString, SyStringData(&pMeth->sFunc.sName), sKey.nByte) != 0);` |
|  2058 |  458 | `				if( sKey.nByte == sizeof("__construct")-1` |
|  1236 |  459 | `				 && SyMemcmp(sKey.zString, "__construct", sKey.nByte) == 0 ){` |
|   383 |  460 | `					if( iCtorVis == 0 ){` |
|   383 |  461 | `						iCtorVis = pMeth->iProtection;` |
|   191 |  462 | `					}` |
|   383 |  463 | `					if( bIsAlias ){` |
|     - |  464 | `						/* Mount-time alias for a legacy class-name constructor:` |
|     - |  465 | `						 * the method is already listed under its declared name. */` |
|   ! 0 |  466 | `						continue;` |
|     - |  467 | `					}` |
|  1868 |  468 | `				}else if( sKey.nByte == sizeof("__clone")-1` |
|   933 |  469 | `				 && SyMemcmp(sKey.zString, "__clone", sKey.nByte) == 0 ){` |
|   ! 0 |  470 | `					if( iCloneVis == 0 ){` |
|   ! 0 |  471 | `						iCloneVis = pMeth->iProtection;` |
|   ! 0 |  472 | `					}` |
|  1676 |  473 | `				}else if( iCtorVis == 0` |
|  1210 |  474 | `				 && sKey.nByte == SyStringLength(&pClass->sName)` |
|   409 |  475 | `				 && SyMemcmp(sKey.zString, SyStringData(&pClass->sName), sKey.nByte) == 0 ){` |
|     - |  476 | `					/* Legacy class-name constructor before the mount alias exists */` |
|   ! 0 |  477 | `					iCtorVis = pMeth->iProtection;` |
|   ! 0 |  478 | `				}` |
|  2059 |  479 | `				pMeta = ph7_context_new_array(pCtx);` |
|  2059 |  480 | `				if( pMeta == 0 ){ break; }` |
|  2059 |  481 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pMeth->iProtection);` |
|  2059 |  482 | `				ReflectMapAddBool(pCtx, pMeta, "static", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|  2059 |  483 | `				ReflectMapAddBool(pCtx, pMeta, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|  2059 |  484 | `				ReflectMapAddBool(pCtx, pMeta, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|  2059 |  485 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|  2059 |  486 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pMeth->nLine);` |
|  2059 |  487 | `				ReflectMapAddDyn(pCtx, pMethods, &sKey, pMeta);` |
|  1030 |  488 | `			}` |
|   611 |  489 | `		}` |
|   939 |  490 | `		SySetRelease(&aTmp);` |
|     - |  491 | `	}` |
|   939 |  492 | `	ReflectMapAddInt(pCtx, pInfo, "ctorvis", (sxi64)iCtorVis);` |
|   939 |  493 | `	ReflectMapAddInt(pCtx, pInfo, "clonevis", (sxi64)iCloneVis);` |
|   939 |  494 | `	ph7_array_add_strkey_elem(pInfo, "consts", pConsts);` |
|   939 |  495 | `	ph7_array_add_strkey_elem(pInfo, "props", pProps);` |
|   939 |  496 | `	ph7_array_add_strkey_elem(pInfo, "methods", pMethods);` |
|   939 |  497 | `	ph7_result_value(pCtx, pInfo);` |
|   939 |  498 | `	return PH7_OK;` |
|   476 |  499 | `}` |
|     - |  500 | `/*` |
|     - |  501 | ` * mixed __reflect_const_value(string $class, string $name)` |
|     - |  502 | ` * Value of a class constant. The PHP layer guarantees existence.` |
|     - |  503 | ` */` |
|    42 |  504 | `static int vm_builtin_reflect_const_value(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  505 | `{` |
|     - |  506 | `	ph7_class *pClass;` |
|     - |  507 | `	ph7_class_attr *pAttr;` |
|     - |  508 | `	ph7_value *pValue;` |
|    42 |  509 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    42 |  510 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|    43 |  511 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){` |
|   ! 0 |  512 | `		ph7_result_null(pCtx);` |
|   ! 0 |  513 | `		return PH7_OK;` |
|     - |  514 | `	}` |
|     - |  515 | `	/* Constant slots are evaluated lazily on first access */` |
|    43 |  516 | `	if( PH7_VmMaterializeClassConst(pCtx->pVm,pClass,pAttr) != SXRET_OK ){` |
|     - |  517 | `		/* Initializer raised: the throw is in flight; report null here */` |
|   ! 0 |  518 | `		ph7_result_null(pCtx);` |
|   ! 0 |  519 | `		return PH7_OK;` |
|     - |  520 | `	}` |
|    43 |  521 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|    43 |  522 | `	if( pValue ){` |
|    43 |  523 | `		ph7_result_value(pCtx, pValue);` |
|    22 |  524 | `	}else{` |
|   ! 0 |  525 | `		ph7_result_null(pCtx);` |
|     - |  526 | `	}` |
|    43 |  527 | `	return PH7_OK;` |
|    22 |  528 | `}` |
|     - |  529 | `/*` |
|     - |  530 | ` * mixed __reflect_static_value(string $class, string $name)` |
|     - |  531 | ` * Current value of a static property (visibility ignored).` |
|     - |  532 | ` */` |
|    12 |  533 | `static int vm_builtin_reflect_static_value(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  534 | `{` |
|     - |  535 | `	ph7_class *pClass;` |
|     - |  536 | `	ph7_class_attr *pAttr;` |
|     - |  537 | `	ph7_value *pValue;` |
|    12 |  538 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    12 |  539 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|    13 |  540 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|   ! 0 |  541 | `		ph7_result_null(pCtx);` |
|   ! 0 |  542 | `		return PH7_OK;` |
|     - |  543 | `	}` |
|     - |  544 | `	{` |
|     - |  545 | `		/* Uninitialized typed static: same Error the VM raises on read */` |
|    13 |  546 | `		SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|    13 |  547 | `		if( pSlot ){` |
|   ! 0 |  548 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|   ! 0 |  549 | `			if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|   ! 0 |  550 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;` |
|   ! 0 |  551 | `				return PH7_VmThrowException(pCtx, "Error",` |
|     - |  552 | `					"Typed static property %z::$%z must not be accessed before initialization",` |
|   ! 0 |  553 | `					&pDecl->sName, &pAttr->sName);` |
|     - |  554 | `			}` |
|   ! 0 |  555 | `		}` |
|     - |  556 | `	}` |
|    13 |  557 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|    13 |  558 | `	if( pValue ){` |
|    13 |  559 | `		ph7_result_value(pCtx, pValue);` |
|     7 |  560 | `	}else{` |
|   ! 0 |  561 | `		ph7_result_null(pCtx);` |
|     - |  562 | `	}` |
|    13 |  563 | `	return PH7_OK;` |
|     7 |  564 | `}` |
|     - |  565 | `/*` |
|     - |  566 | ` * bool __reflect_static_set(string $class, string $name, mixed $value)` |
|     - |  567 | ` * Overwrite a static property's shared slot (visibility ignored).` |
|     - |  568 | ` */` |
|     4 |  569 | `static int vm_builtin_reflect_static_set(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  570 | `{` |
|     - |  571 | `	ph7_class *pClass;` |
|     - |  572 | `	ph7_class_attr *pAttr;` |
|     - |  573 | `	ph7_value *pValue;` |
|     4 |  574 | `	if( nArg < 3 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|     4 |  575 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|     5 |  576 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){` |
|   ! 0 |  577 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  578 | `		return PH7_OK;` |
|     - |  579 | `	}` |
|     5 |  580 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);` |
|     5 |  581 | `	if( pValue == 0 ){` |
|   ! 0 |  582 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  583 | `		return PH7_OK;` |
|     - |  584 | `	}` |
|     - |  585 | `	{` |
|     5 |  586 | `		sxi32 rc = ReflectEnforceStore(pCtx, pAttr->nIdx, apArg[2]);` |
|     5 |  587 | `		if( rc != SXRET_OK ){` |
|   ! 0 |  588 | `			return rc;` |
|     - |  589 | `		}` |
|     - |  590 | `	}` |
|     5 |  591 | `	PH7_MemObjStore(apArg[2], pValue);` |
|     5 |  592 | `	ph7_result_bool(pCtx, 1);` |
|     5 |  593 | `	return PH7_OK;` |
|     3 |  594 | `}` |
|     - |  595 | `/*` |
|     - |  596 | ` * mixed __reflect_prop_default(string $class, string $name)` |
|     - |  597 | ` * Evaluate a non-static property's compiled default expression` |
|     - |  598 | ` * (null when the property has no default).` |
|     - |  599 | ` */` |
|    24 |  600 | `static int vm_builtin_reflect_prop_default(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  601 | `{` |
|     - |  602 | `	ph7_class *pClass;` |
|     - |  603 | `	ph7_class_attr *pAttr;` |
|     - |  604 | `	ph7_value sValue;` |
|    24 |  605 | `	if( nArg < 2 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0` |
|    24 |  606 | `	 \|\| (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0` |
|    24 |  607 | `	 \|\| (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) != 0` |
|    25 |  608 | `	 \|\| SySetUsed(&pAttr->aByteCode) < 1 ){` |
|     3 |  609 | `		ph7_result_null(pCtx);` |
|     3 |  610 | `		return PH7_OK;` |
|     - |  611 | `	}` |
|    23 |  612 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|     - |  613 | `	/* Same evaluation path the VM uses for omitted call arguments */` |
|    23 |  614 | `	VmLocalExec(pCtx->pVm, &pAttr->aByteCode, &sValue, FALSE);` |
|    23 |  615 | `	ph7_result_value(pCtx, &sValue);` |
|    23 |  616 | `	PH7_MemObjRelease(&sValue);` |
|    23 |  617 | `	return PH7_OK;` |
|    13 |  618 | `}` |
|     - |  619 | `/*` |
|     - |  620 | ` * Collect a PHP array's values into a ph7_value* set (call arguments).` |
|     - |  621 | ` * When ppNames is non-NULL, string keys become named arguments: a name` |
|     - |  622 | ` * map is lazily allocated (like call_user_func_array's) with one entry` |
|     - |  623 | ` * per collected slot, empty entries meaning positional.` |
|     - |  624 | ` */` |
|    38 |  625 | `static sxi32 ReflectCollectArgs(ph7_context *pCtx, ph7_value *pArray, SySet *pOut, SyString **ppNames)` |
|     1 |  626 | `{` |
|     - |  627 | `	ph7_hashmap *pMap;` |
|     - |  628 | `	ph7_hashmap_node *pEntry;` |
|    39 |  629 | `	SyString *aNames = 0;` |
|    39 |  630 | `	sxu32 nSlot = 0;` |
|     - |  631 | `	sxu32 n;` |
|    39 |  632 | `	if( ppNames ){` |
|    19 |  633 | `		*ppNames = 0;` |
|     9 |  634 | `	}` |
|    39 |  635 | `	if( !ph7_value_is_array(pArray) ){` |
|   ! 0 |  636 | `		return SXRET_OK;` |
|     - |  637 | `	}` |
|    39 |  638 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|    39 |  639 | `	pEntry = pMap->pFirst;` |
|    91 |  640 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    53 |  641 | `		ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|    53 |  642 | `		if( pValue ){` |
|    53 |  643 | `			if( ppNames && pEntry->iType == HASHMAP_BLOB_NODE ){` |
|     3 |  644 | `				if( aNames == 0 ){` |
|     4 |  645 | `					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,` |
|     2 |  646 | `						pMap->nEntry * sizeof(SyString));` |
|     3 |  647 | `					if( aNames ){` |
|     3 |  648 | `						SyZero(aNames, pMap->nEntry * sizeof(SyString));` |
|     1 |  649 | `					}` |
|     1 |  650 | `				}` |
|     3 |  651 | `				if( aNames ){` |
|     3 |  652 | `					SyStringInitFromBuf(&aNames[nSlot],` |
|     - |  653 | `						SyBlobData(&pEntry->xKey.sKey), SyBlobLength(&pEntry->xKey.sKey));` |
|     1 |  654 | `				}` |
|     1 |  655 | `			}` |
|    53 |  656 | `			SySetPut(pOut, (const void *)&pValue);` |
|    53 |  657 | `			nSlot++;` |
|    26 |  658 | `		}` |
|    53 |  659 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|    27 |  660 | `	}` |
|    39 |  661 | `	if( ppNames ){` |
|    19 |  662 | `		*ppNames = aNames;` |
|     9 |  663 | `	}` |
|    39 |  664 | `	return SXRET_OK;` |
|    20 |  665 | `}` |
|     - |  666 | `/*` |
|     - |  667 | ` * object __reflect_new_instance(string $class, array $args)` |
|     - |  668 | ` * Instantiate and run the constructor with positional arguments.` |
|     - |  669 | ` * The PHP layer has already validated instantiability and ctor visibility.` |
|     - |  670 | ` */` |
|    22 |  671 | `static int vm_builtin_reflect_new_instance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  672 | `{` |
|    23 |  673 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  674 | `	ph7_class *pClass;` |
|     - |  675 | `	ph7_class_instance *pThis;` |
|     - |  676 | `	ph7_class_method *pCons;` |
|    23 |  677 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pVm, apArg[0])) == 0 ){` |
|   ! 0 |  678 | `		ph7_result_null(pCtx);` |
|   ! 0 |  679 | `		return PH7_OK;` |
|     - |  680 | `	}` |
|    23 |  681 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|    23 |  682 | `	if( pThis == 0 ){` |
|   ! 0 |  683 | `		ph7_result_null(pCtx);` |
|   ! 0 |  684 | `		return PH7_OK;` |
|     - |  685 | `	}` |
|    23 |  686 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|    23 |  687 | `	if( pCons ){` |
|     - |  688 | `		SySet aArg;` |
|     - |  689 | `		sxi32 rc;` |
|    19 |  690 | `		SyString *aNames = 0;` |
|    19 |  691 | `		SySetInit(&aArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    19 |  692 | `		if( nArg > 1 ){` |
|    19 |  693 | `			ReflectCollectArgs(pCtx, apArg[1], &aArg, &aNames);` |
|     9 |  694 | `		}` |
|    19 |  695 | `		if( aNames ){` |
|     - |  696 | `			VmCallArgMap sMap;` |
|     3 |  697 | `			sMap.bHasNamed = 1;` |
|     3 |  698 | `			sMap.bIsNamespaced = 0;` |
|     3 |  699 | `			sMap.bStrict = 0;` |
|     3 |  700 | `			sMap.nTotal = SySetUsed(&aArg);` |
|     3 |  701 | `			sMap.aNames = aNames;` |
|     4 |  702 | `			rc = PH7_VmCallClassMethodMap(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|     2 |  703 | `				(ph7_value **)SySetBasePtr(&aArg), &sMap);` |
|     3 |  704 | `			SyMemBackendFree(&pVm->sAllocator, aNames);` |
|     2 |  705 | `		}else{` |
|    25 |  706 | `			rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|    16 |  707 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|     - |  708 | `		}` |
|    19 |  709 | `		SySetRelease(&aArg);` |
|    19 |  710 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 |  711 | `			PH7_ClassInstanceUnref(pThis);` |
|   ! 0 |  712 | `			return rc;` |
|     - |  713 | `		}` |
|     9 |  714 | `	}` |
|    23 |  715 | `	return ReflectResultObject(pCtx, pThis);` |
|    12 |  716 | `}` |
|     - |  717 | `/*` |
|     - |  718 | ` * object __reflect_new_no_ctor(string $class)` |
|     - |  719 | ` * Instantiate without running the constructor (property defaults still` |
|     - |  720 | ` * apply — PH7_NewClassInstance builds the attribute frame).` |
|     - |  721 | ` */` |
|    60 |  722 | `static int vm_builtin_reflect_new_no_ctor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  723 | `{` |
|     - |  724 | `	ph7_class *pClass;` |
|    61 |  725 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0 ){` |
|   ! 0 |  726 | `		ph7_result_null(pCtx);` |
|   ! 0 |  727 | `		return PH7_OK;` |
|     - |  728 | `	}` |
|    61 |  729 | `	return ReflectResultObject(pCtx, PH7_NewClassInstance(pCtx->pVm, pClass));` |
|    31 |  730 | `}` |
|     - |  731 | `/*` |
|     - |  732 | ` * Typed/readonly store enforcement for reflection writes. Like the VM's` |
|     - |  733 | ` * store path, except an UNINITIALIZED readonly property may be written from` |
|     - |  734 | ` * any scope (PHP lets ReflectionProperty::setValue initialize readonly): the` |
|     - |  735 | ` * READONLY bit is masked off for the enforcement call so the set-scope check` |
|     - |  736 | ` * is skipped, while an already-initialized readonly still gets PHP's` |
|     - |  737 | ` * "Cannot modify readonly property" Error. Returns SXRET_OK/PH7_EXCEPTION/` |
|     - |  738 | ` * PH7_ABORT; the value may be coerced in place.` |
|     - |  739 | ` */` |
|    10 |  740 | `static sxi32 ReflectEnforceStore(ph7_context *pCtx, sxu32 nIdx, ph7_value *pValue)` |
|     1 |  741 | `{` |
|    11 |  742 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  743 | `	SyHashEntry *pSlot;` |
|     - |  744 | `	VmClassAttr *pVmAttr;` |
|     - |  745 | `	ph7_class_attr *pAttr;` |
|     - |  746 | `	sxi32 iSaved, rc;` |
|    11 |  747 | `	pSlot = SyHashGet(&pVm->hTypedSlot, (const void *)&nIdx, sizeof(sxu32));` |
|    11 |  748 | `	if( pSlot == 0 ){` |
|     7 |  749 | `		return SXRET_OK; /* Untyped slot: plain store */` |
|     - |  750 | `	}` |
|     5 |  751 | `	pVmAttr = (VmClassAttr *)pSlot->pUserData;` |
|     5 |  752 | `	pAttr = pVmAttr->pAttr;` |
|     5 |  753 | `	if( pAttr == 0 ){` |
|   ! 0 |  754 | `		return SXRET_OK;` |
|     - |  755 | `	}` |
|     5 |  756 | `	iSaved = pAttr->iFlags;` |
|     5 |  757 | `	if( (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) ){` |
|   ! 0 |  758 | `		pAttr->iFlags &= ~PH7_CLASS_ATTR_READONLY;` |
|   ! 0 |  759 | `	}` |
|     5 |  760 | `	rc = PH7_VmEnforcePropStore(pVm, nIdx, pValue);` |
|     5 |  761 | `	pAttr->iFlags = iSaved;` |
|     5 |  762 | `	return rc;` |
|     6 |  763 | `}` |
|     - |  764 | `/*` |
|     - |  765 | ` * mixed __reflect_prop_read(object $obj, string $name)` |
|     - |  766 | ` * Instance property read, visibility ignored. Throws PHP's Error for an` |
|     - |  767 | ` * uninitialized typed property.` |
|     - |  768 | ` */` |
|    20 |  769 | `static int vm_builtin_reflect_prop_read(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  770 | `{` |
|     - |  771 | `	ph7_class_instance *pThis;` |
|     - |  772 | `	SyHashEntry *pEntry;` |
|     - |  773 | `	VmClassAttr *pVmAttr;` |
|     - |  774 | `	ph7_value *pValue;` |
|     - |  775 | `	const char *zName;` |
|     - |  776 | `	int nLen;` |
|    21 |  777 | `	if( nArg < 2 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  778 | `		ph7_result_null(pCtx);` |
|   ! 0 |  779 | `		return PH7_OK;` |
|     - |  780 | `	}` |
|    21 |  781 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    21 |  782 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|    21 |  783 | `	pEntry = nLen > 0 ? SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen) : 0;` |
|    21 |  784 | `	if( pEntry == 0 ){` |
|   ! 0 |  785 | `		ph7_result_null(pCtx);` |
|   ! 0 |  786 | `		return PH7_OK;` |
|     - |  787 | `	}` |
|    21 |  788 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    21 |  789 | `	if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){` |
|     3 |  790 | `		ph7_class *pDecl = pVmAttr->pAttr->pDeclClass ? pVmAttr->pAttr->pDeclClass : pThis->pClass;` |
|     4 |  791 | `		return PH7_VmThrowException(pCtx, "Error",` |
|     - |  792 | `			"Typed property %z::$%z must not be accessed before initialization",` |
|     2 |  793 | `			&pDecl->sName, &pVmAttr->pAttr->sName);` |
|     - |  794 | `	}` |
|    19 |  795 | `	pValue = PH7_ClassInstanceExtractAttrValue(pThis, pVmAttr);` |
|    19 |  796 | `	if( pValue ){` |
|    19 |  797 | `		ph7_result_value(pCtx, pValue);` |
|    10 |  798 | `	}else{` |
|   ! 0 |  799 | `		ph7_result_null(pCtx);` |
|     - |  800 | `	}` |
|    19 |  801 | `	return PH7_OK;` |
|    11 |  802 | `}` |
|     - |  803 | `/*` |
|     - |  804 | ` * bool __reflect_prop_write(object $obj, string $name, mixed $value)` |
|     - |  805 | ` * Instance property write, visibility ignored; typed and readonly rules` |
|     - |  806 | ` * enforced (see ReflectEnforceStore).` |
|     - |  807 | ` */` |
|     6 |  808 | `static int vm_builtin_reflect_prop_write(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  809 | `{` |
|     - |  810 | `	ph7_class_instance *pThis;` |
|     - |  811 | `	SyHashEntry *pEntry;` |
|     - |  812 | `	VmClassAttr *pVmAttr;` |
|     - |  813 | `	ph7_value *pValue;` |
|     - |  814 | `	const char *zName;` |
|     - |  815 | `	sxi32 rc;` |
|     - |  816 | `	int nLen;` |
|     7 |  817 | `	if( nArg < 3 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){` |
|   ! 0 |  818 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  819 | `		return PH7_OK;` |
|     - |  820 | `	}` |
|     7 |  821 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     7 |  822 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|     7 |  823 | `	pEntry = nLen > 0 ? SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen) : 0;` |
|     7 |  824 | `	if( pEntry == 0 ){` |
|   ! 0 |  825 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  826 | `		return PH7_OK;` |
|     - |  827 | `	}` |
|     7 |  828 | `	pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     7 |  829 | `	rc = ReflectEnforceStore(pCtx, pVmAttr->nIdx, apArg[2]);` |
|     7 |  830 | `	if( rc != SXRET_OK ){` |
|     3 |  831 | `		return rc;` |
|     - |  832 | `	}` |
|     5 |  833 | `	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);` |
|     5 |  834 | `	if( pValue == 0 ){` |
|   ! 0 |  835 | `		ph7_result_bool(pCtx, 0);` |
|   ! 0 |  836 | `		return PH7_OK;` |
|     - |  837 | `	}` |
|     5 |  838 | `	PH7_MemObjStore(apArg[2], pValue);` |
|     5 |  839 | `	ph7_result_bool(pCtx, 1);` |
|     5 |  840 | `	return PH7_OK;` |
|     4 |  841 | `}` |
|     - |  842 | `/*` |
|     - |  843 | ` * int __reflect_prop_state(object\|string $target, string $name)` |
|     - |  844 | ` * Bitfield: 1 = exists (instance attr / static slot), 2 = initialized,` |
|     - |  845 | ` * 4 = dynamic (instance-owned, not class-declared).` |
|     - |  846 | ` */` |
|    16 |  847 | `static int vm_builtin_reflect_prop_state(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  848 | `{` |
|    17 |  849 | `	int iState = 0;` |
|     - |  850 | `	const char *zName;` |
|     - |  851 | `	int nLen;` |
|    17 |  852 | `	if( nArg < 2 ){` |
|   ! 0 |  853 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  854 | `		return PH7_OK;` |
|     - |  855 | `	}` |
|    17 |  856 | `	zName = ph7_value_to_string(apArg[1], &nLen);` |
|    17 |  857 | `	if( nLen < 1 ){` |
|   ! 0 |  858 | `		ph7_result_int(pCtx, 0);` |
|   ! 0 |  859 | `		return PH7_OK;` |
|     - |  860 | `	}` |
|    17 |  861 | `	if( apArg[0]->iFlags & MEMOBJ_OBJ ){` |
|    17 |  862 | `		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|    17 |  863 | `		SyHashEntry *pEntry = SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen);` |
|    17 |  864 | `		if( pEntry ){` |
|    17 |  865 | `			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|    17 |  866 | `			iState \|= 1;` |
|    17 |  867 | `			if( (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){` |
|    15 |  868 | `				iState \|= 2;` |
|     7 |  869 | `			}` |
|    17 |  870 | `			if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|    11 |  871 | `				iState \|= 4;` |
|     5 |  872 | `			}` |
|     8 |  873 | `		}` |
|     9 |  874 | `	}else{` |
|   ! 0 |  875 | `		ph7_class *pClass = ReflectResolveClass(pCtx->pVm, apArg[0]);` |
|   ! 0 |  876 | `		ph7_class_attr *pAttr = pClass ? ReflectFetchAttr(pClass, apArg[1]) : 0;` |
|   ! 0 |  877 | `		if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){` |
|   ! 0 |  878 | `			SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));` |
|   ! 0 |  879 | `			iState \|= 1 \| 2;` |
|   ! 0 |  880 | `			if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){` |
|   ! 0 |  881 | `				iState &= ~2;` |
|   ! 0 |  882 | `			}` |
|   ! 0 |  883 | `		}` |
|     - |  884 | `	}` |
|    17 |  885 | `	ph7_result_int(pCtx, iState);` |
|    17 |  886 | `	return PH7_OK;` |
|     9 |  887 | `}` |
|     - |  888 | `/*` |
|     - |  889 | ` * array __reflect_dyn_props(object $obj)` |
|     - |  890 | ` * Names of the instance's runtime-added (dynamic) properties, in creation` |
|     - |  891 | ` * order (the instance attr table inserts dynamics at the tail).` |
|     - |  892 | ` */` |
|     2 |  893 | `static int vm_builtin_reflect_dyn_props(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  894 | `{` |
|     - |  895 | `	ph7_class_instance *pThis;` |
|     - |  896 | `	SyHashEntry *pEntry;` |
|     - |  897 | `	ph7_value *pList;` |
|     2 |  898 | `	if( nArg < 1 \|\| (apArg[0]->iFlags & MEMOBJ_OBJ) == 0` |
|     3 |  899 | `	 \|\| (pList = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 |  900 | `		ph7_result_null(pCtx);` |
|   ! 0 |  901 | `		return PH7_OK;` |
|     - |  902 | `	}` |
|     3 |  903 | `	pThis = (ph7_class_instance *)apArg[0]->x.pOther;` |
|     3 |  904 | `	SyHashResetLoopCursor(&pThis->hAttr);` |
|     7 |  905 | `	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){` |
|     5 |  906 | `		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;` |
|     5 |  907 | `		if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){` |
|     5 |  908 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|     5 |  909 | `			if( pName == 0 ){ break; }` |
|     7 |  910 | `			ph7_value_string(pName, SyStringData(&pVmAttr->pAttr->sName),` |
|     4 |  911 | `				(int)SyStringLength(&pVmAttr->pAttr->sName));` |
|     5 |  912 | `			ph7_array_add_elem(pList, 0, pName);` |
|     2 |  913 | `		}` |
|     1 |  914 | `	}` |
|     3 |  915 | `	ph7_result_value(pCtx, pList);` |
|     3 |  916 | `	return PH7_OK;` |
|     2 |  917 | `}` |
|     - |  918 | `/* Hand an EXISTING instance to the caller: takes an extra reference` |
|     - |  919 | ` * (unlike ReflectResultObject, which transfers a fresh instance's one). */` |
|     4 |  920 | `static int ReflectResultExistingObject(ph7_context *pCtx, ph7_class_instance *pObj)` |
|     1 |  921 | `{` |
|     5 |  922 | `	if( pObj == 0 ){` |
|   ! 0 |  923 | `		ph7_result_null(pCtx);` |
|   ! 0 |  924 | `		return PH7_OK;` |
|     - |  925 | `	}` |
|     5 |  926 | `	PH7_MemObjRelease(pCtx->pRet);` |
|     5 |  927 | `	pObj->iRef++;` |
|     5 |  928 | `	pCtx->pRet->x.pOther = pObj;` |
|     5 |  929 | `	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);` |
|     5 |  930 | `	return PH7_OK;` |
|     3 |  931 | `}` |
|     - |  932 | `/* pVal is a Closure instance? Return it, else NULL. */` |
|   442 |  933 | `static ph7_class_instance * ReflectValueClosure(ph7_vm *pVm, ph7_value *pVal)` |
|     1 |  934 | `{` |
|     - |  935 | `	ph7_class_instance *pThis;` |
|   443 |  936 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
|   391 |  937 | `		return 0;` |
|     - |  938 | `	}` |
|    53 |  939 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    53 |  940 | `	return (pThis->pClass == pVm->pClosureClass) ? pThis : 0;` |
|   222 |  941 | `}` |
|     - |  942 | `/*` |
|     - |  943 | ` * Resolve a reflection callable target into its compiled function.` |
|     - |  944 | ` *   - pMethodArg a non-empty string  -> method mode: pTarget is a class name` |
|     - |  945 | ` *     or object; outputs *ppClass and *ppMeth.` |
|     - |  946 | ` *   - pTarget a Closure              -> unwrap $__fn into hFunction; *ppClosure.` |
|     - |  947 | ` *   - pTarget a string               -> hFunction (user) or hHostFunction` |
|     - |  948 | ` *     (*ppHost set, returns NULL).` |
|     - |  949 | ` * Returns the ph7_vm_func, or NULL (host function or unresolvable).` |
|     - |  950 | ` */` |
|   722 |  951 | `static ph7_vm_func * ReflectResolveCallable(ph7_context *pCtx, ph7_value *pTarget,` |
|     - |  952 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|     - |  953 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|     1 |  954 | `{` |
|   723 |  955 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  956 | `	SyHashEntry *pEntry;` |
|   723 |  957 | `	if( ppClass ){ *ppClass = 0; }` |
|   723 |  958 | `	if( ppMeth ){ *ppMeth = 0; }` |
|   723 |  959 | `	if( ppHost ){ *ppHost = 0; }` |
|   723 |  960 | `	if( ppClosure ){ *ppClosure = 0; }` |
|   723 |  961 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
|   281 |  962 | `		ph7_class *pClass = ReflectResolveClass(pVm, pTarget);` |
|     - |  963 | `		ph7_class_method *pMeth;` |
|   281 |  964 | `		if( pClass == 0 ){` |
|   ! 0 |  965 | `			return 0;` |
|     - |  966 | `		}` |
|   421 |  967 | `		pMeth = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|   140 |  968 | `			SyBlobLength(&pMethodArg->sBlob));` |
|   281 |  969 | `		if( pMeth == 0 ){` |
|   ! 0 |  970 | `			return 0;` |
|     - |  971 | `		}` |
|   281 |  972 | `		if( ppClass ){ *ppClass = pClass; }` |
|   281 |  973 | `		if( ppMeth ){ *ppMeth = pMeth; }` |
|   281 |  974 | `		return &pMeth->sFunc;` |
|     - |  975 | `	}` |
|     - |  976 | `	{` |
|   443 |  977 | `		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|   443 |  978 | `		if( pClo ){` |
|     - |  979 | `			SyString sAttr;` |
|     - |  980 | `			ph7_value *pFn;` |
|    53 |  981 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    53 |  982 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|    53 |  983 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) < 1 ){` |
|   ! 0 |  984 | `				return 0;` |
|     - |  985 | `			}` |
|    53 |  986 | `			pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    53 |  987 | `			if( pEntry == 0 ){` |
|     - |  988 | `				/* A Closure over a host function (Closure::fromCallable('strlen')) */` |
|   ! 0 |  989 | `				pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|   ! 0 |  990 | `				if( pEntry && ppHost ){` |
|   ! 0 |  991 | `					*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|   ! 0 |  992 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|   ! 0 |  993 | `				}` |
|   ! 0 |  994 | `				return 0;` |
|     - |  995 | `			}` |
|    53 |  996 | `			if( ppClosure ){ *ppClosure = pClo; }` |
|    53 |  997 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - |  998 | `		}` |
|     - |  999 | `	}` |
|   391 | 1000 | `	if( pTarget->iFlags & MEMOBJ_STRING ){` |
|   391 | 1001 | `		if( SyBlobLength(&pTarget->sBlob) < 1 ){` |
|   ! 0 | 1002 | `			return 0;` |
|     - | 1003 | `		}` |
|   391 | 1004 | `		pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   391 | 1005 | `		if( pEntry ){` |
|   285 | 1006 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - | 1007 | `		}` |
|   107 | 1008 | `		pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   107 | 1009 | `		if( pEntry && ppHost ){` |
|   105 | 1010 | `			*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|    52 | 1011 | `		}` |
|    53 | 1012 | `	}` |
|   107 | 1013 | `	return 0;` |
|   362 | 1014 | `}` |
|     - | 1015 | `/* Emit the shared descriptor fields of a compiled function. */` |
|   570 | 1016 | `static void ReflectFillFuncCommon(ph7_context *pCtx, ph7_value *pInfo, ph7_vm_func *pFunc)` |
|     1 | 1017 | `{` |
|     - | 1018 | `	ph7_vm_func_arg *aArg;` |
|     - | 1019 | `	ph7_value *pParams, *pStatics;` |
|   571 | 1020 | `	int bVariadic = 0;` |
|     - | 1021 | `	int bAnon;` |
|     - | 1022 | `	sxu32 n;` |
|     - | 1023 | ``	/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|     - | 1024 | `	 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|   571 | 1025 | `	bAnon = (pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|   570 | 1026 | `	if( !bAnon && SyStringLength(&pFunc->sName) > 9` |
|   306 | 1027 | `	 && (SyMemcmp(SyStringData(&pFunc->sName), "[lambda_", 8) == 0` |
|    82 | 1028 | `	  \|\| SyMemcmp(SyStringData(&pFunc->sName), "[closure_", 9) == 0) ){` |
|     5 | 1029 | `		bAnon = 1;` |
|     2 | 1030 | `	}` |
|   571 | 1031 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|   571 | 1032 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pFunc->iFlags & VM_FUNC_INTERNAL) != 0);` |
|   571 | 1033 | `	ReflectMapAddBool(pCtx, pInfo, "closure", bAnon);` |
|   571 | 1034 | `	ReflectMapAddBool(pCtx, pInfo, "fstatic", (pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);` |
|   571 | 1035 | `	ReflectMapAddBool(pCtx, pInfo, "byref", (pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|   571 | 1036 | `	ReflectMapAddBool(pCtx, pInfo, "generator", (pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|   571 | 1037 | `	ReflectMapAddBool(pCtx, pInfo, "strict", pFunc->bStrictTypes != 0);` |
|   571 | 1038 | `	if( SyStringLength(&pFunc->sFile) > 0 ){` |
|   567 | 1039 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pFunc->sFile), (int)SyStringLength(&pFunc->sFile));` |
|   284 | 1040 | `	}else{` |
|     5 | 1041 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1042 | `	}` |
|   571 | 1043 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pFunc->nLine);` |
|   571 | 1044 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pFunc->nEndLine);` |
|   571 | 1045 | `	ReflectMapAddDoc(pCtx, pInfo, &pFunc->sDoc);` |
|   571 | 1046 | `	ReflectMapAddAttrs(pCtx, pInfo, &pFunc->aAttrs);` |
|   571 | 1047 | `	if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|   145 | 1048 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", SyStringData(&pFunc->sReturnTypeName),` |
|    96 | 1049 | `			(int)SyStringLength(&pFunc->sReturnTypeName));` |
|   523 | 1050 | `	}else if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     - | 1051 | `		/* The type-text renderer omits void/never atoms (compile.c notes the` |
|     - | 1052 | `		 * root fix belongs there); name them here for getReturnType(). */` |
|     3 | 1053 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "void", sizeof("void")-1);` |
|   474 | 1054 | `	}else if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|     3 | 1055 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "never", sizeof("never")-1);` |
|     2 | 1056 | `	}else{` |
|   471 | 1057 | `		ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1058 | `	}` |
|   571 | 1059 | `	ReflectMapAddBool(pCtx, pInfo, "retnullable", (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) != 0);` |
|     - | 1060 | `	/* Parameters */` |
|   571 | 1061 | `	pParams = ph7_context_new_array(pCtx);` |
|   571 | 1062 | `	aArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|  1823 | 1063 | `	for( n = 0 ; pParams && n < SySetUsed(&pFunc->aArgs) ; n++ ){` |
|  1253 | 1064 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|  1253 | 1065 | `		if( pMeta == 0 ){ break; }` |
|  1253 | 1066 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aArg[n].sName), (int)SyStringLength(&aArg[n].sName));` |
|  1253 | 1067 | `		ReflectMapAddInt(pCtx, pMeta, "pos", (sxi64)n);` |
|  1253 | 1068 | `		ReflectMapAddBool(pCtx, pMeta, "byref", (aArg[n].iFlags & VM_FUNC_ARG_BY_REF) != 0);` |
|  1253 | 1069 | `		ReflectMapAddBool(pCtx, pMeta, "variadic", (aArg[n].iFlags & VM_FUNC_ARG_VARIADIC) != 0);` |
|     - | 1070 | `		/* The compiler never sets ARG_HAS_DEF; a default = compiled bytecode` |
|     - | 1071 | `		 * (same test the OP_CALL default-value path uses). */` |
|  1253 | 1072 | `		ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&aArg[n].aByteCode) > 0);` |
|  1253 | 1073 | `		ReflectMapAddBool(pCtx, pMeta, "nullable", (aArg[n].iFlags & VM_FUNC_ARG_NULLABLE) != 0);` |
|  1253 | 1074 | `		ReflectMapAddBool(pCtx, pMeta, "promoted", (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) != 0);` |
|  1253 | 1075 | `		if( SyStringLength(&aArg[n].sTypeName) > 0 ){` |
|   715 | 1076 | `			ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&aArg[n].sTypeName),` |
|   476 | 1077 | `				(int)SyStringLength(&aArg[n].sTypeName));` |
|   239 | 1078 | `		}else{` |
|   777 | 1079 | `			ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - | 1080 | `		}` |
|  1253 | 1081 | `		ReflectMapAddAttrs(pCtx, pMeta, &aArg[n].aAttrs);` |
|  1253 | 1082 | `		ph7_array_add_elem(pParams, 0, pMeta);` |
|  1253 | 1083 | `		if( aArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|   183 | 1084 | `			bVariadic = 1;` |
|    91 | 1085 | `		}` |
|   627 | 1086 | `	}` |
|   571 | 1087 | `	if( pParams ){` |
|   571 | 1088 | `		ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|   285 | 1089 | `	}` |
|   571 | 1090 | `	ReflectMapAddBool(pCtx, pInfo, "variadic", bVariadic);` |
|     - | 1091 | `	/* Static variables: current value when the slot was initialized (first` |
|     - | 1092 | `	 * call), otherwise the evaluated default — PHP's getStaticVariables` |
|     - | 1093 | `	 * initializes on demand and reports the same values. */` |
|   571 | 1094 | `	pStatics = ph7_context_new_array(pCtx);` |
|   571 | 1095 | `	if( pStatics ){` |
|   571 | 1096 | `		ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|   599 | 1097 | `		for( n = 0 ; n < SySetUsed(&pFunc->aStatic) ; n++ ){` |
|    29 | 1098 | `			ph7_value *pVal = 0;` |
|     - | 1099 | `			ph7_value sScratch;` |
|    29 | 1100 | `			int bScratch = 0;` |
|    29 | 1101 | `			if( aStatic[n].nIdx != SXU32_HIGH ){` |
|    11 | 1102 | `				pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);` |
|     5 | 1103 | `			}` |
|    29 | 1104 | `			if( pVal == 0 ){` |
|    19 | 1105 | `				PH7_MemObjInit(pCtx->pVm, &sScratch);` |
|    19 | 1106 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|    19 | 1107 | `					VmLocalExec(pCtx->pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|     9 | 1108 | `				}` |
|    19 | 1109 | `				pVal = &sScratch;` |
|    19 | 1110 | `				bScratch = 1;` |
|     9 | 1111 | `			}` |
|    29 | 1112 | `			ReflectMapAddDyn(pCtx, pStatics, &aStatic[n].sName, pVal);` |
|    29 | 1113 | `			if( bScratch ){` |
|    19 | 1114 | `				PH7_MemObjRelease(&sScratch);` |
|     9 | 1115 | `			}` |
|    15 | 1116 | `		}` |
|   571 | 1117 | `		ph7_array_add_strkey_elem(pInfo, "statics", pStatics);` |
|   285 | 1118 | `	}` |
|   571 | 1119 | `}` |
|     - | 1120 | `/*` |
|     - | 1121 | ` * array\|null __reflect_func_info(string\|Closure $target [, string $method])` |
|     - | 1122 | ` * Function/method/closure descriptor for the PHP layer.` |
|     - | 1123 | ` */` |
|   676 | 1124 | `static int vm_builtin_reflect_func_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1125 | `{` |
|     - | 1126 | `	ph7_vm_func *pFunc;` |
|   677 | 1127 | `	ph7_class *pClass = 0;` |
|   677 | 1128 | `	ph7_class_method *pMeth = 0;` |
|   677 | 1129 | `	ph7_user_func *pHost = 0;` |
|   677 | 1130 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1131 | `	ph7_value *pInfo;` |
|   677 | 1132 | `	if( nArg < 1 ){` |
|   ! 0 | 1133 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1134 | `		return PH7_OK;` |
|     - | 1135 | `	}` |
|   677 | 1136 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], nArg > 1 ? apArg[1] : 0,` |
|     - | 1137 | `		&pClass, &pMeth, &pHost, &pClosure);` |
|   677 | 1138 | `	if( pFunc == 0 && pHost == 0 ){` |
|     3 | 1139 | `		ph7_result_null(pCtx);` |
|     3 | 1140 | `		return PH7_OK;` |
|     - | 1141 | `	}` |
|   675 | 1142 | `	pInfo = ph7_context_new_array(pCtx);` |
|   675 | 1143 | `	if( pInfo == 0 ){` |
|   ! 0 | 1144 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1145 | `		return PH7_OK;` |
|     - | 1146 | `	}` |
|   675 | 1147 | `	if( pFunc == 0 ){` |
|     - | 1148 | `		/* Host (C builtin) function: no parameter metadata beyond arity */` |
|   105 | 1149 | `		ph7_value *pParams = ph7_context_new_array(pCtx);` |
|   105 | 1150 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pHost->sName), (int)SyStringLength(&pHost->sName));` |
|   105 | 1151 | `		ReflectMapAddBool(pCtx, pInfo, "internal", 1);` |
|   105 | 1152 | `		ReflectMapAddBool(pCtx, pInfo, "closure", 0);` |
|   105 | 1153 | `		ReflectMapAddBool(pCtx, pInfo, "fstatic", 0);` |
|   105 | 1154 | `		ReflectMapAddBool(pCtx, pInfo, "byref", 0);` |
|   105 | 1155 | `		ReflectMapAddBool(pCtx, pInfo, "generator", 0);` |
|   105 | 1156 | `		ReflectMapAddBool(pCtx, pInfo, "strict", 0);` |
|   105 | 1157 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|   105 | 1158 | `		ReflectMapAddInt(pCtx, pInfo, "line", 0);` |
|   105 | 1159 | `		ReflectMapAddInt(pCtx, pInfo, "endline", 0);` |
|   105 | 1160 | `		ReflectMapAddBool(pCtx, pInfo, "doc", 0);` |
|     - | 1161 | `		{` |
|   105 | 1162 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|   105 | 1163 | `			if( pEmpty ){` |
|   105 | 1164 | `				ph7_array_add_strkey_elem(pInfo, "attrs", pEmpty);` |
|    52 | 1165 | `			}` |
|     - | 1166 | `		}` |
|   105 | 1167 | `		if( pHost->zRet ){` |
|   105 | 1168 | `			ReflectMapAddStr(pCtx, pInfo, "rettext", pHost->zRet, (int)SyStrlen(pHost->zRet));` |
|    53 | 1169 | `		}else{` |
|   ! 0 | 1170 | `			ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1171 | `		}` |
|   105 | 1172 | `		ReflectMapAddBool(pCtx, pInfo, "retnullable", 0);` |
|   105 | 1173 | `		if( pParams ){` |
|   105 | 1174 | `			ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|    52 | 1175 | `		}` |
|   105 | 1176 | `		ReflectMapAddBool(pCtx, pInfo, "variadic", 0);` |
|   105 | 1177 | `		ReflectMapAddInt(pCtx, pInfo, "minarg", (sxi64)pHost->nMinArg);` |
|   105 | 1178 | `		if( pHost->zSig ){` |
|   105 | 1179 | `			ReflectMapAddStr(pCtx, pInfo, "sig", pHost->zSig, (int)SyStrlen(pHost->zSig));` |
|    53 | 1180 | `		}else{` |
|   ! 0 | 1181 | `			ReflectMapAddStr(pCtx, pInfo, "sig", "", 0);` |
|     - | 1182 | `		}` |
|   105 | 1183 | `		ph7_result_value(pCtx, pInfo);` |
|   105 | 1184 | `		return PH7_OK;` |
|     - | 1185 | `	}` |
|   571 | 1186 | `	ReflectFillFuncCommon(pCtx, pInfo, pFunc);` |
|   571 | 1187 | `	ReflectMapAddInt(pCtx, pInfo, "minarg", -1);` |
|   571 | 1188 | `	if( (pFunc->iFlags & VM_FUNC_INTERNAL) && SySetUsed(&pFunc->aArgs) == 0 && pMeth == 0 ){` |
|     - | 1189 | `		/* Embedded-PHP builtin (max/min...): declared argless, actual` |
|     - | 1190 | `		 * signature comes from the static table */` |
|     5 | 1191 | `		const char *zRet = 0;` |
|     5 | 1192 | `		const char *zSig = PH7_VmBuiltinSigLookup(SyStringData(&pFunc->sName), SyStringLength(&pFunc->sName), &zRet);` |
|     5 | 1193 | `		if( zSig ){` |
|     5 | 1194 | `			ReflectMapAddStr(pCtx, pInfo, "sig", zSig, (int)SyStrlen(zSig));` |
|     2 | 1195 | `		}` |
|     5 | 1196 | `		if( zRet && SyStringLength(&pFunc->sReturnTypeName) == 0 ){` |
|     5 | 1197 | `			ReflectMapAddStr(pCtx, pInfo, "ret2", zRet, (int)SyStrlen(zRet));` |
|     2 | 1198 | `		}` |
|     2 | 1199 | `	}` |
|   571 | 1200 | `	if( pMeth && pClass ){` |
|   259 | 1201 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|   259 | 1202 | `		ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   259 | 1203 | `		ReflectMapAddStr(pCtx, pInfo, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   259 | 1204 | `		ReflectMapAddInt(pCtx, pInfo, "vis", (sxi64)pMeth->iProtection);` |
|   259 | 1205 | `		ReflectMapAddBool(pCtx, pInfo, "mstatic", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   259 | 1206 | `		ReflectMapAddBool(pCtx, pInfo, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|   259 | 1207 | `		ReflectMapAddBool(pCtx, pInfo, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   129 | 1208 | `	}` |
|   571 | 1209 | `	if( pClosure ){` |
|     - | 1210 | `		SyString sAttr;` |
|     - | 1211 | `		ph7_value *pAttr;` |
|     - | 1212 | `		ph7_value *pUsed;` |
|    49 | 1213 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|    49 | 1214 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    49 | 1215 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|   ! 0 | 1216 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 1217 | `			if( pKey ){` |
|   ! 0 | 1218 | `				ph7_value_string(pKey, "this", 4);` |
|   ! 0 | 1219 | `				ph7_array_add_elem(pInfo, pKey, pAttr);` |
|   ! 0 | 1220 | `			}` |
|   ! 0 | 1221 | `		}else{` |
|    49 | 1222 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|     - | 1223 | `		}` |
|    49 | 1224 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|    49 | 1225 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    49 | 1226 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|   ! 0 | 1227 | `			ReflectMapAddStr(pCtx, pInfo, "scope", (const char *)SyBlobData(&pAttr->sBlob),` |
|   ! 0 | 1228 | `				(int)SyBlobLength(&pAttr->sBlob));` |
|   ! 0 | 1229 | `		}else{` |
|    49 | 1230 | `			ReflectMapAddNull(pCtx, pInfo, "scope");` |
|     - | 1231 | `		}` |
|     - | 1232 | `		/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */` |
|    49 | 1233 | `		pUsed = ph7_context_new_array(pCtx);` |
|    49 | 1234 | `		if( pUsed ){` |
|    49 | 1235 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     - | 1236 | `			sxu32 n;` |
|   107 | 1237 | `			for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; n++ ){` |
|    59 | 1238 | `				if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){` |
|    41 | 1239 | `					continue;` |
|     - | 1240 | `				}` |
|    18 | 1241 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|    10 | 1242 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|   ! 0 | 1243 | `					continue;` |
|     - | 1244 | `				}` |
|    19 | 1245 | `				if( (aEnv[n].iFlags & VM_FUNC_ARG_BY_REF) && aEnv[n].nIdx != SXU32_HIGH ){` |
|     - | 1246 | `					/* Captured by reference: report the slot's live value */` |
|     5 | 1247 | `					ph7_value *pLive = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aEnv[n].nIdx);` |
|     5 | 1248 | `					ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, pLive ? pLive : &aEnv[n].sValue);` |
|     5 | 1249 | `					continue;` |
|     - | 1250 | `				}` |
|    15 | 1251 | `				ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, &aEnv[n].sValue);` |
|     8 | 1252 | `			}` |
|    49 | 1253 | `			ph7_array_add_strkey_elem(pInfo, "used", pUsed);` |
|    24 | 1254 | `		}` |
|    24 | 1255 | `	}` |
|   571 | 1256 | `	ph7_result_value(pCtx, pInfo);` |
|   571 | 1257 | `	return PH7_OK;` |
|   339 | 1258 | `}` |
|     - | 1259 | `/*` |
|     - | 1260 | ` * mixed __reflect_param_default(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1261 | ` * Evaluate a parameter's compiled default expression.` |
|     - | 1262 | ` */` |
|    12 | 1263 | `static int vm_builtin_reflect_param_default(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1264 | `{` |
|     - | 1265 | `	ph7_vm_func *pFunc;` |
|     - | 1266 | `	ph7_vm_func_arg *pArg;` |
|     - | 1267 | `	ph7_value sValue;` |
|     - | 1268 | `	sxu32 nIdx;` |
|    13 | 1269 | `	if( nArg < 3 ){` |
|   ! 0 | 1270 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1271 | `		return PH7_OK;` |
|     - | 1272 | `	}` |
|    13 | 1273 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|    13 | 1274 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|    12 | 1275 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|    13 | 1276 | `	 \|\| SySetUsed(&pArg->aByteCode) < 1 ){` |
|   ! 0 | 1277 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1278 | `		return PH7_OK;` |
|     - | 1279 | `	}` |
|    13 | 1280 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|    13 | 1281 | `	VmLocalExec(pCtx->pVm, &pArg->aByteCode, &sValue, FALSE);` |
|    13 | 1282 | `	ph7_result_value(pCtx, &sValue);` |
|    13 | 1283 | `	PH7_MemObjRelease(&sValue);` |
|    13 | 1284 | `	return PH7_OK;` |
|     7 | 1285 | `}` |
|     - | 1286 | `/*` |
|     - | 1287 | ` * string\|null __reflect_param_defconst(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1288 | ` * When a parameter's default is a plain global-constant reference, its` |
|     - | 1289 | ` * source name; null otherwise. A constant default compiles to exactly` |
|     - | 1290 | ` * [ OP_LOADC (EXPAND) , OP_DONE ] with the name in the literal table.` |
|     - | 1291 | ` */` |
|     6 | 1292 | `static int vm_builtin_reflect_param_defconst(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1293 | `{` |
|     - | 1294 | `	ph7_vm_func *pFunc;` |
|     - | 1295 | `	ph7_vm_func_arg *pArg;` |
|     - | 1296 | `	VmInstr *aInstr;` |
|     - | 1297 | `	ph7_value *pLit;` |
|     - | 1298 | `	sxu32 nIdx;` |
|     7 | 1299 | `	if( nArg < 3 ){` |
|   ! 0 | 1300 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1301 | `		return PH7_OK;` |
|     - | 1302 | `	}` |
|     7 | 1303 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|     7 | 1304 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|     6 | 1305 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|     7 | 1306 | `	 \|\| SySetUsed(&pArg->aByteCode) != 2 ){` |
|     3 | 1307 | `		ph7_result_null(pCtx);` |
|     3 | 1308 | `		return PH7_OK;` |
|     - | 1309 | `	}` |
|     5 | 1310 | `	aInstr = (VmInstr *)SySetBasePtr(&pArg->aByteCode);` |
|     4 | 1311 | `	if( aInstr[0].iOp != PH7_OP_LOADC \|\| (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0` |
|     5 | 1312 | `	 \|\| aInstr[1].iOp != PH7_OP_DONE ){` |
|   ! 0 | 1313 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1314 | `		return PH7_OK;` |
|     - | 1315 | `	}` |
|     5 | 1316 | `	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);` |
|     5 | 1317 | `	if( pLit == 0 \|\| SyBlobLength(&pLit->sBlob) < 1 ){` |
|   ! 0 | 1318 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1319 | `		return PH7_OK;` |
|     - | 1320 | `	}` |
|     5 | 1321 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&pLit->sBlob), (int)SyBlobLength(&pLit->sBlob));` |
|     5 | 1322 | `	return PH7_OK;` |
|     4 | 1323 | `}` |
|     - | 1324 | `/*` |
|     - | 1325 | ` * mixed __reflect_invoke(mixed $target, ?string $method, ?object $this, array $args)` |
|     - | 1326 | ` * Visibility-bypassing invocation (methods dispatch by VM name; functions` |
|     - | 1327 | ` * and closures ride PH7_VmCallUserFunction like call_user_func_array).` |
|     - | 1328 | ` */` |
|    20 | 1329 | `static int vm_builtin_reflect_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1330 | `{` |
|    21 | 1331 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1332 | `	ph7_value sResult;` |
|     - | 1333 | `	SySet aCallArg;` |
|     - | 1334 | `	sxi32 rc;` |
|    21 | 1335 | `	if( nArg < 4 ){` |
|   ! 0 | 1336 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1337 | `		return PH7_OK;` |
|     - | 1338 | `	}` |
|    21 | 1339 | `	PH7_MemObjInit(pVm, &sResult);` |
|    21 | 1340 | `	sResult.nIdx = SXU32_HIGH;` |
|    21 | 1341 | `	SySetInit(&aCallArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    21 | 1342 | `	ReflectCollectArgs(pCtx, apArg[3], &aCallArg, 0);` |
|    21 | 1343 | `	if( (apArg[1]->iFlags & MEMOBJ_STRING) && SyBlobLength(&apArg[1]->sBlob) > 0 ){` |
|    11 | 1344 | `		ph7_class *pClass = 0;` |
|    11 | 1345 | `		ph7_class_method *pMeth = 0;` |
|    11 | 1346 | `		ph7_class_instance *pThis = 0;` |
|    11 | 1347 | `		ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, 0);` |
|    11 | 1348 | `		if( pMeth == 0 ){` |
|   ! 0 | 1349 | `			SySetRelease(&aCallArg);` |
|   ! 0 | 1350 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1351 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1352 | `			return PH7_OK;` |
|     - | 1353 | `		}` |
|    11 | 1354 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     7 | 1355 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     3 | 1356 | `		}` |
|     - | 1357 | `		/* Reflection ignores method visibility (PHP 8.1+); the flag is` |
|     - | 1358 | `		 * consumed by the first OP_CALL, i.e. this synthetic one. */` |
|    11 | 1359 | `		pVm->bReflectBypass = 1;` |
|    16 | 1360 | `		rc = PH7_VmCallClassMethod(pVm, pThis, pMeth, &sResult,` |
|    10 | 1361 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg));` |
|    11 | 1362 | `		pVm->bReflectBypass = 0;` |
|     6 | 1363 | `	}else{` |
|    16 | 1364 | `		rc = PH7_VmCallUserFunction(pVm, apArg[0],` |
|    10 | 1365 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg), &sResult);` |
|     - | 1366 | `	}` |
|    21 | 1367 | `	SySetRelease(&aCallArg);` |
|    21 | 1368 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 | 1369 | `		PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1370 | `		return rc;` |
|     - | 1371 | `	}` |
|    21 | 1372 | `	ph7_result_value(pCtx, &sResult);` |
|    21 | 1373 | `	PH7_MemObjRelease(&sResult);` |
|    21 | 1374 | `	return PH7_OK;` |
|    11 | 1375 | `}` |
|     - | 1376 | `/*` |
|     - | 1377 | ` * Closure __reflect_closure(mixed $target, ?string $method, ?object $this)` |
|     - | 1378 | ` * Mint a Closure for a function or method, bound and scoped like the` |
|     - | 1379 | ` * first-class-callable path.` |
|     - | 1380 | ` */` |
|     6 | 1381 | `static int vm_builtin_reflect_closure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1382 | `{` |
|     7 | 1383 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 1384 | `	ph7_class *pClass = 0;` |
|     7 | 1385 | `	ph7_class_method *pMeth = 0;` |
|     7 | 1386 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1387 | `	ph7_vm_func *pFunc;` |
|     7 | 1388 | `	if( nArg < 3 ){` |
|   ! 0 | 1389 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1390 | `		return PH7_OK;` |
|     - | 1391 | `	}` |
|     7 | 1392 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, &pClosure);` |
|     7 | 1393 | `	if( pClosure ){` |
|     - | 1394 | `		/* Already a Closure: hand the same instance back */` |
|   ! 0 | 1395 | `		return ReflectResultExistingObject(pCtx, pClosure);` |
|     - | 1396 | `	}` |
|     7 | 1397 | `	if( pMeth && pClass ){` |
|     5 | 1398 | `		ph7_class_instance *pThis = 0;` |
|     5 | 1399 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     3 | 1400 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     1 | 1401 | `		}` |
|     7 | 1402 | `		return ReflectResultObject(pCtx,` |
|     4 | 1403 | `			PH7_VmNewClosure(pVm, &pMeth->sFunc.sName, pThis, &pClass->sName));` |
|     - | 1404 | `	}` |
|     3 | 1405 | `	if( pFunc ){` |
|     3 | 1406 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &pFunc->sName, 0, 0));` |
|     - | 1407 | `	}` |
|     - | 1408 | `	/* Host function by name */` |
|   ! 0 | 1409 | `	if( apArg[0]->iFlags & MEMOBJ_STRING ){` |
|     - | 1410 | `		SyString sName;` |
|   ! 0 | 1411 | `		SyStringInitFromBuf(&sName, (const char *)SyBlobData(&apArg[0]->sBlob), SyBlobLength(&apArg[0]->sBlob));` |
|   ! 0 | 1412 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &sName, 0, 0));` |
|     - | 1413 | `	}` |
|   ! 0 | 1414 | `	ph7_result_null(pCtx);` |
|   ! 0 | 1415 | `	return PH7_OK;` |
|     4 | 1416 | `}` |
|     - | 1417 | `/*` |
|     - | 1418 | ` * Resolve a Generator object into its wrapper. Mirrors the static` |
|     - | 1419 | ` * VmGeneratorExtractCtx in vm.c: the $__ctx attribute carries the` |
|     - | 1420 | ` * ph7_generator pointer as a resource value.` |
|     - | 1421 | ` */` |
|    22 | 1422 | `static ph7_generator * ReflectGeneratorCtx(ph7_vm *pVm, ph7_value *pVal)` |
|     1 | 1423 | `{` |
|     - | 1424 | `	ph7_class_instance *pThis;` |
|     - | 1425 | `	ph7_value *pAttr;` |
|     - | 1426 | `	SyString sAttr;` |
|    23 | 1427 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVm->pGeneratorClass == 0 ){` |
|   ! 0 | 1428 | `		return 0;` |
|     - | 1429 | `	}` |
|    23 | 1430 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    23 | 1431 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|   ! 0 | 1432 | `		return 0;` |
|     - | 1433 | `	}` |
|    23 | 1434 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    23 | 1435 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    23 | 1436 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|   ! 0 | 1437 | `		return 0;` |
|     - | 1438 | `	}` |
|    23 | 1439 | `	return (ph7_generator *)pAttr->x.pOther;` |
|    12 | 1440 | `}` |
|     - | 1441 | `/*` |
|     - | 1442 | ` * array\|null __reflect_gen_info(Generator $g)` |
|     - | 1443 | ` * {state, closed, executing, kind ('fn'\|'method'), name, class?, this}` |
|     - | 1444 | ` */` |
|    16 | 1445 | `static int vm_builtin_reflect_gen_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1446 | `{` |
|    17 | 1447 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1448 | `	ph7_generator *pGen;` |
|     - | 1449 | `	ph7_exec_ctx *pExec;` |
|     - | 1450 | `	ph7_value *pInfo;` |
|    17 | 1451 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 \|\| pGen->pCtx == 0 ){` |
|   ! 0 | 1452 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1453 | `		return PH7_OK;` |
|     - | 1454 | `	}` |
|    17 | 1455 | `	pExec = pGen->pCtx;` |
|    17 | 1456 | `	pInfo = ph7_context_new_array(pCtx);` |
|    17 | 1457 | `	if( pInfo == 0 ){` |
|   ! 0 | 1458 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1459 | `		return PH7_OK;` |
|     - | 1460 | `	}` |
|    17 | 1461 | `	ReflectMapAddInt(pCtx, pInfo, "state", (sxi64)pExec->iState);` |
|    24 | 1462 | `	ReflectMapAddBool(pCtx, pInfo, "closed",` |
|    16 | 1463 | `		pExec->iState == PH7_CTX_STATE_COMPLETED \|\| pExec->iState == PH7_CTX_STATE_CLOSED);` |
|    17 | 1464 | `	ReflectMapAddBool(pCtx, pInfo, "executing", pVm->pActiveCtx == pExec);` |
|    17 | 1465 | `	if( pExec->pFunc ){` |
|    17 | 1466 | `		ph7_vm_func *pFunc = pExec->pFunc;` |
|    19 | 1467 | `		if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     5 | 1468 | `			ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|     5 | 1469 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "method", sizeof("method")-1);` |
|     5 | 1470 | `			ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|     3 | 1471 | `		}else{` |
|    13 | 1472 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "fn", sizeof("fn")-1);` |
|     - | 1473 | `		}` |
|    17 | 1474 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|     8 | 1475 | `	}` |
|     - | 1476 | `	{` |
|     - | 1477 | `		/* The coroutine frame installs $this as a frame VARIABLE (see` |
|     - | 1478 | `		 * VmFiberSetupFrame), not as pFrame->pThis — check both. */` |
|    17 | 1479 | `		ph7_value *pThisVal = 0;` |
|    17 | 1480 | `		if( pExec->pFrame ){` |
|    17 | 1481 | `			SyHashEntry *pVar = SyHashGet(&pExec->pFrame->hVar, "this", sizeof("this")-1);` |
|    17 | 1482 | `			if( pVar ){` |
|     5 | 1483 | `				ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj, (sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|     5 | 1484 | `				if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|     5 | 1485 | `					pThisVal = pSlot;` |
|     2 | 1486 | `				}` |
|     2 | 1487 | `			}` |
|    17 | 1488 | `			if( pThisVal == 0 && pExec->pFrame->pThis ){` |
|     - | 1489 | `				ph7_value sThis;` |
|   ! 0 | 1490 | `				ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 1491 | `				PH7_MemObjInit(pVm, &sThis);` |
|   ! 0 | 1492 | `				pExec->pFrame->pThis->iRef++;` |
|   ! 0 | 1493 | `				sThis.x.pOther = pExec->pFrame->pThis;` |
|   ! 0 | 1494 | `				MemObjSetType(&sThis, MEMOBJ_OBJ);` |
|   ! 0 | 1495 | `				if( pKey ){` |
|   ! 0 | 1496 | `					ph7_value_string(pKey, "this", 4);` |
|   ! 0 | 1497 | `					ph7_array_add_elem(pInfo, pKey, &sThis); /* copies (takes its own ref) */` |
|   ! 0 | 1498 | `				}` |
|   ! 0 | 1499 | `				PH7_MemObjRelease(&sThis);` |
|   ! 0 | 1500 | `				pThisVal = (ph7_value *)1; /* handled */` |
|   ! 0 | 1501 | `			}` |
|     8 | 1502 | `		}` |
|    17 | 1503 | `		if( pThisVal == 0 ){` |
|    13 | 1504 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|    11 | 1505 | `		}else if( pThisVal != (ph7_value *)1 ){` |
|     5 | 1506 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|     5 | 1507 | `			if( pKey ){` |
|     5 | 1508 | `				ph7_value_string(pKey, "this", 4);` |
|     5 | 1509 | `				ph7_array_add_elem(pInfo, pKey, pThisVal);` |
|     2 | 1510 | `			}` |
|     2 | 1511 | `		}` |
|     - | 1512 | `	}` |
|    17 | 1513 | `	ph7_result_value(pCtx, pInfo);` |
|    17 | 1514 | `	return PH7_OK;` |
|     9 | 1515 | `}` |
|     - | 1516 | `/*` |
|     - | 1517 | ` * Generator __reflect_gen_exec(Generator $g)` |
|     - | 1518 | `` * Follow `yield from` delegation to the innermost executing generator`` |
|     - | 1519 | ` * (PHP's ReflectionGenerator::getExecutingGenerator).` |
|     - | 1520 | ` */` |
|     4 | 1521 | `static int vm_builtin_reflect_gen_exec(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1522 | `{` |
|     5 | 1523 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1524 | `	ph7_generator *pGen;` |
|     - | 1525 | `	ph7_value *pCur;` |
|     5 | 1526 | `	int iDepth = 0;` |
|     5 | 1527 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 ){` |
|   ! 0 | 1528 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1529 | `		return PH7_OK;` |
|     - | 1530 | `	}` |
|     5 | 1531 | `	pCur = apArg[0];` |
|     9 | 1532 | `	while( pGen && pGen->pCtx && pGen->pCtx->iDelegateState == 3` |
|     8 | 1533 | `	 && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|     3 | 1534 | `		ph7_generator *pInner = ReflectGeneratorCtx(pVm, &pGen->pCtx->sDelegate);` |
|     3 | 1535 | `		if( pInner == 0 ){` |
|   ! 0 | 1536 | `			break;` |
|     - | 1537 | `		}` |
|     3 | 1538 | `		pCur = &pGen->pCtx->sDelegate;` |
|     3 | 1539 | `		pGen = pInner;` |
|     3 | 1540 | `		iDepth++;` |
|     1 | 1541 | `	}` |
|     5 | 1542 | `	return ReflectResultExistingObject(pCtx, (ph7_class_instance *)pCur->x.pOther);` |
|     3 | 1543 | `}` |
|     - | 1544 | `/*` |
|     - | 1545 | ` * array\|null __reflect_const_info(string $name)` |
|     - | 1546 | ` * Global-constant descriptor: {value}. Null when undefined. File/origin` |
|     - | 1547 | ` * metadata arrives with the C5 constant-metadata work.` |
|     - | 1548 | ` */` |
|    40 | 1549 | `static int vm_builtin_reflect_const_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1550 | `{` |
|    41 | 1551 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1552 | `	SyHashEntry *pEntry;` |
|     - | 1553 | `	ph7_constant *pCons;` |
|     - | 1554 | `	ph7_value *pInfo;` |
|     - | 1555 | `	ph7_value sValue;` |
|     - | 1556 | `	const char *zName;` |
|     - | 1557 | `	int nLen;` |
|    41 | 1558 | `	if( nArg < 1 ){` |
|   ! 0 | 1559 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1560 | `		return PH7_OK;` |
|     - | 1561 | `	}` |
|    41 | 1562 | `	zName = ph7_value_to_string(apArg[0], &nLen);` |
|    41 | 1563 | `	pEntry = nLen > 0 ? SyHashGet(&pVm->hConstant, (const void *)zName, (sxu32)nLen) : 0;` |
|    41 | 1564 | `	if( pEntry == 0 ){` |
|     3 | 1565 | `		ph7_result_null(pCtx);` |
|     3 | 1566 | `		return PH7_OK;` |
|     - | 1567 | `	}` |
|    39 | 1568 | `	pCons = (ph7_constant *)pEntry->pUserData;` |
|    39 | 1569 | `	pInfo = ph7_context_new_array(pCtx);` |
|    39 | 1570 | `	if( pInfo == 0 ){` |
|   ! 0 | 1571 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1572 | `		return PH7_OK;` |
|     - | 1573 | `	}` |
|    39 | 1574 | `	PH7_MemObjInit(pVm, &sValue);` |
|    39 | 1575 | `	if( pCons->xExpand ){` |
|    39 | 1576 | `		pCons->xExpand(&sValue, pCons->pUserData);` |
|    19 | 1577 | `	}` |
|     - | 1578 | `	{` |
|    39 | 1579 | `		ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|    39 | 1580 | `		if( pKey ){` |
|    39 | 1581 | `			ph7_value_string(pKey, "value", 5);` |
|    39 | 1582 | `			ph7_array_add_elem(pInfo, pKey, &sValue);` |
|    19 | 1583 | `		}` |
|     - | 1584 | `	}` |
|    39 | 1585 | `	PH7_MemObjRelease(&sValue);` |
|    39 | 1586 | `	ReflectMapAddBool(pCtx, pInfo, "internal", pCons->bUserDefined == 0);` |
|    39 | 1587 | `	if( SyStringLength(&pCons->sFile) > 0 ){` |
|    25 | 1588 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));` |
|    13 | 1589 | `	}else{` |
|    15 | 1590 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1591 | `	}` |
|    39 | 1592 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pCons->nLine);` |
|    39 | 1593 | `	ReflectMapAddAttrs(pCtx, pInfo, &pCons->aAttrs);` |
|    39 | 1594 | `	ph7_result_value(pCtx, pInfo);` |
|    39 | 1595 | `	return PH7_OK;` |
|    21 | 1596 | `}` |
|     - | 1597 | `/*` |
|     - | 1598 | ` * int\|null __reflect_ref_id(array $arr, int\|string $key)` |
|     - | 1599 | ` * The element's slot index when the element is a reference (its slot has` |
|     - | 1600 | ` * a reference-table record with at least two links), null otherwise.` |
|     - | 1601 | ` */` |
|     6 | 1602 | `static int vm_builtin_reflect_ref_id(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1603 | `{` |
|     - | 1604 | `	ph7_hashmap *pMap;` |
|     7 | 1605 | `	ph7_hashmap_node *pNode = 0;` |
|     7 | 1606 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|   ! 0 | 1607 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1608 | `		return PH7_OK;` |
|     - | 1609 | `	}` |
|     7 | 1610 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 | 1611 | `	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK \|\| pNode == 0 ){` |
|   ! 0 | 1612 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1613 | `		return PH7_OK;` |
|     - | 1614 | `	}` |
|     7 | 1615 | `	if( PH7_VmSlotRefCount(pCtx->pVm, pNode->nValIdx) < 2 ){` |
|     3 | 1616 | `		ph7_result_null(pCtx);` |
|     3 | 1617 | `		return PH7_OK;` |
|     - | 1618 | `	}` |
|     5 | 1619 | `	ph7_result_int64(pCtx, (sxi64)pNode->nValIdx);` |
|     5 | 1620 | `	return PH7_OK;` |
|     4 | 1621 | `}` |
|     - | 1622 | `/*` |
|     - | 1623 | ` * array\|null __reflect_attr_args(string $kind, mixed $target, ?string $member,` |
|     - | 1624 | ` *                                int $paramIdx, int $attrIdx)` |
|     - | 1625 | ` * Evaluate the recorded argument expressions of one declared attribute:` |
|     - | 1626 | ` * kind 'class' (target = class), 'attr' (class + property/constant name),` |
|     - | 1627 | ` * 'method' (class + method), 'fn' (function name or Closure), 'param'` |
|     - | 1628 | ` * (function spec + parameter index). Named arguments become string keys.` |
|     - | 1629 | ` */` |
|    52 | 1630 | `static int vm_builtin_reflect_attr_args(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1631 | `{` |
|    53 | 1632 | `	ph7_vm *pVm = pCtx->pVm;` |
|    53 | 1633 | `	SySet *pAttrs = 0;` |
|     - | 1634 | `	ph7_attribute *pAttrRec;` |
|     - | 1635 | `	ph7_value *pOut;` |
|     - | 1636 | `	const char *zKind;` |
|     - | 1637 | `	int nKind;` |
|     - | 1638 | `	sxu32 nAttrIdx, n;` |
|    53 | 1639 | `	if( nArg < 5 ){` |
|   ! 0 | 1640 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1641 | `		return PH7_OK;` |
|     - | 1642 | `	}` |
|    53 | 1643 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|    53 | 1644 | `	nAttrIdx = (sxu32)ph7_value_to_int(apArg[4]);` |
|    70 | 1645 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|    35 | 1646 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|    35 | 1647 | `		if( pClass ){ pAttrs = &pClass->aAttrs; }` |
|    38 | 1648 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|     5 | 1649 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|     5 | 1650 | `		ph7_class_attr *pMember = pClass ? ReflectFetchAttr(pClass, apArg[2]) : 0;` |
|     5 | 1651 | `		if( pMember ){ pAttrs = &pMember->aAttrs; }` |
|    18 | 1652 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|     3 | 1653 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1654 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|    18 | 1655 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|     9 | 1656 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], 0, 0, 0, 0, 0);` |
|     9 | 1657 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|    10 | 1658 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|     3 | 1659 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1660 | `		ph7_vm_func_arg *pParam = pFunc` |
|     2 | 1661 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|     3 | 1662 | `		if( pParam ){ pAttrs = &pParam->aAttrs; }` |
|     4 | 1663 | `	}else if( nKind == 5 && SyMemcmp(zKind, "const", 5) == 0 ){` |
|     - | 1664 | ``		/* Global constant (php 8.5 attributes on `const` statements) */`` |
|     - | 1665 | `		const char *zCName;` |
|     - | 1666 | `		int nCName;` |
|     - | 1667 | `		SyHashEntry *pCEntry;` |
|     3 | 1668 | `		zCName = ph7_value_to_string(apArg[1], &nCName);` |
|     3 | 1669 | `		pCEntry = nCName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zCName, (sxu32)nCName) : 0;` |
|     3 | 1670 | `		if( pCEntry ){ pAttrs = &((ph7_constant *)pCEntry->pUserData)->aAttrs; }` |
|     1 | 1671 | `	}` |
|    52 | 1672 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|    53 | 1673 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 | 1674 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1675 | `		return PH7_OK;` |
|     - | 1676 | `	}` |
|    99 | 1677 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|    47 | 1678 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|     - | 1679 | `		ph7_value sValue;` |
|    47 | 1680 | `		PH7_MemObjInit(pVm, &sValue);` |
|    47 | 1681 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|    47 | 1682 | `			VmLocalExec(pVm, &pArgRec->aByteCode, &sValue, FALSE);` |
|    23 | 1683 | `		}` |
|    47 | 1684 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|     5 | 1685 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|     3 | 1686 | `		}else{` |
|    43 | 1687 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|     - | 1688 | `		}` |
|    47 | 1689 | `		PH7_MemObjRelease(&sValue);` |
|    24 | 1690 | `	}` |
|    53 | 1691 | `	ph7_result_value(pCtx, pOut);` |
|    53 | 1692 | `	return PH7_OK;` |
|    27 | 1693 | `}` |
|     - | 1694 | `/*` |
|     - | 1695 | ` * The Reflection classes, in PHP. Chunk 1: exceptions, Reflector,` |
|     - | 1696 | ` * Reflection, ReflectionClass, ReflectionObject (plus get_debug_type,` |
|     - | 1697 | ` * which the TypeError messages need and PHP 8.0 ships natively).` |
|     - | 1698 | ` */` |
|     - | 1699 | `static const char zReflectLib1[] =` |
|     - | 1700 | `"function get_debug_type($value){"` |
|     - | 1701 | `" if(is_object($value)){ return get_class($value); }"` |
|     - | 1702 | `" if(is_bool($value)){ return 'bool'; }"` |
|     - | 1703 | `" if(is_int($value)){ return 'int'; }"` |
|     - | 1704 | `" if(is_float($value)){ return 'float'; }"` |
|     - | 1705 | `" if(is_string($value)){ return 'string'; }"` |
|     - | 1706 | `" if(is_array($value)){ return 'array'; }"` |
|     - | 1707 | `" if($value === null){ return 'null'; }"` |
|     - | 1708 | `" return gettype($value);"` |
|     - | 1709 | `"}"` |
|     - | 1710 | `"interface Reflector extends Stringable {}"` |
|     - | 1711 | `"class ReflectionException extends Exception {}"` |
|     - | 1712 | `"class Reflection {"` |
|     - | 1713 | `" public static function getModifierNames($modifiers){"` |
|     - | 1714 | `"  $names = array();"` |
|     - | 1715 | `"  if($modifiers & 64){ $names[] = 'abstract'; }"` |
|     - | 1716 | `"  if($modifiers & 32){ $names[] = 'final'; }"` |
|     - | 1717 | `"  if($modifiers & 1){ $names[] = 'public'; }"` |
|     - | 1718 | `"  if($modifiers & 2){ $names[] = 'protected'; }"` |
|     - | 1719 | `"  if($modifiers & 4){ $names[] = 'private'; }"` |
|     - | 1720 | `"  if($modifiers & 16){ $names[] = 'static'; }"` |
|     - | 1721 | `"  if($modifiers & 128){ $names[] = 'readonly'; }"` |
|     - | 1722 | `"  return $names;"` |
|     - | 1723 | `" }"` |
|     - | 1724 | `"}"` |
|     - | 1725 | `"class ReflectionClass implements Reflector {"` |
|     - | 1726 | `" const IS_IMPLICIT_ABSTRACT = 16;"` |
|     - | 1727 | `" const IS_EXPLICIT_ABSTRACT = 64;"` |
|     - | 1728 | `" const IS_FINAL = 32;"` |
|     - | 1729 | `" const IS_READONLY = 65536;"` |
|     - | 1730 | `" const SKIP_INITIALIZATION_ON_SERIALIZE = 8;"` |
|     - | 1731 | `" const SKIP_DESTRUCTOR = 16;"` |
|     - | 1732 | `" public $name;"` |
|     - | 1733 | `" protected $__obj = null;"` |
|     - | 1734 | `" public function __construct($objectOrClass){"` |
|     - | 1735 | `"  if(!is_object($objectOrClass) && !is_string($objectOrClass)){"` |
|     - | 1736 | `"   if(is_int($objectOrClass) \|\| is_float($objectOrClass) \|\| is_bool($objectOrClass)){"` |
|     - | 1737 | `"    $objectOrClass = (string)$objectOrClass;"` |
|     - | 1738 | `"   }else{"` |
|     - | 1739 | `"    throw new TypeError('ReflectionClass::__construct(): Argument #1 ($objectOrClass) must be of type object\|string, '.get_debug_type($objectOrClass).' given');"` |
|     - | 1740 | `"   }"` |
|     - | 1741 | `"  }"` |
|     - | 1742 | `"  $info = __reflect_class_info($objectOrClass);"` |
|     - | 1743 | `"  if($info === null){"` |
|     - | 1744 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 1745 | `"  }"` |
|     - | 1746 | `"  $this->name = $info['name'];"` |
|     - | 1747 | `" }"` |
|     - | 1748 | `" protected function __rinfo(){ return __reflect_class_info($this->name); }"` |
|     - | 1749 | `" public function getName(){ return $this->name; }"` |
|     - | 1750 | `" public function getShortName(){"` |
|     - | 1751 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1752 | `"  if($p === false){ return $this->name; }"` |
|     - | 1753 | `"  return substr($this->name,$p+1);"` |
|     - | 1754 | `" }"` |
|     - | 1755 | `" public function getNamespaceName(){"` |
|     - | 1756 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1757 | `"  if($p === false){ return ''; }"` |
|     - | 1758 | `"  return substr($this->name,0,$p);"` |
|     - | 1759 | `" }"` |
|     - | 1760 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 1761 | `" public function isInternal(){ $i = $this->__rinfo(); return $i['internal']; }"` |
|     - | 1762 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 1763 | `" public function isInterface(){ $i = $this->__rinfo(); return $i['interface']; }"` |
|     - | 1764 | `" public function isTrait(){ $i = $this->__rinfo(); return $i['trait']; }"` |
|     - | 1765 | `" public function isAbstract(){ $i = $this->__rinfo(); return $i['abstract']; }"` |
|     - | 1766 | `" public function isFinal(){ $i = $this->__rinfo(); return $i['final']; }"` |
|     - | 1767 | `" public function isReadOnly(){ $i = $this->__rinfo(); return $i['readonly']; }"` |
|     - | 1768 | `" public function isEnum(){ $i = $this->__rinfo(); return $i['enum']; }"` |
|     - | 1769 | `" public function isAnonymous(){ return strpos($this->name,'class@anonymous') === 0; }"` |
|     - | 1770 | `" public function getModifiers(){"` |
|     - | 1771 | `"  $i = $this->__rinfo();"` |
|     - | 1772 | `"  $m = 0;"` |
|     - | 1773 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 1774 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 1775 | `"  if($i['readonly']){ $m \|= 65536; }"` |
|     - | 1776 | `"  return $m;"` |
|     - | 1777 | `" }"` |
|     - | 1778 | `" public function getParentClass(){"` |
|     - | 1779 | `"  $i = $this->__rinfo();"` |
|     - | 1780 | `"  if($i['parent'] === null){ return false; }"` |
|     - | 1781 | `"  return new ReflectionClass($i['parent']);"` |
|     - | 1782 | `" }"` |
|     - | 1783 | `" public function getInterfaceNames(){ $i = $this->__rinfo(); return $i['interfaces']; }"` |
|     - | 1784 | `" public function getInterfaces(){"` |
|     - | 1785 | `"  $i = $this->__rinfo();"` |
|     - | 1786 | `"  $out = array();"` |
|     - | 1787 | `"  foreach($i['interfaces'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1788 | `"  return $out;"` |
|     - | 1789 | `" }"` |
|     - | 1790 | `" public function getTraitNames(){ $i = $this->__rinfo(); return $i['traits']; }"` |
|     - | 1791 | `" public function getTraits(){"` |
|     - | 1792 | `"  $i = $this->__rinfo();"` |
|     - | 1793 | `"  $out = array();"` |
|     - | 1794 | `"  foreach($i['traits'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1795 | `"  return $out;"` |
|     - | 1796 | `" }"` |
|     - | 1797 | `" public function getTraitAliases(){ return array(); }"` |
|     - | 1798 | `" public function implementsInterface($interface){"` |
|     - | 1799 | `"  if($interface instanceof ReflectionClass){ $interface = $interface->name; }"` |
|     - | 1800 | `"  $target = __reflect_class_info($interface);"` |
|     - | 1801 | `"  if($target === null){"` |
|     - | 1802 | `"   throw new ReflectionException('Interface \"'.$interface.'\" does not exist');"` |
|     - | 1803 | `"  }"` |
|     - | 1804 | `"  if(!$target['interface']){"` |
|     - | 1805 | `"   throw new ReflectionException($target['name'].' is not an interface');"` |
|     - | 1806 | `"  }"` |
|     - | 1807 | `"  $name = $target['name'];"` |
|     - | 1808 | `"  if($this->name === $name){ return true; }"` |
|     - | 1809 | `"  $i = $this->__rinfo();"` |
|     - | 1810 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1811 | `"  return false;"` |
|     - | 1812 | `" }"` |
|     - | 1813 | `" public function isSubclassOf($class){"` |
|     - | 1814 | `"  if($class instanceof ReflectionClass){ $class = $class->name; }"` |
|     - | 1815 | `"  $target = __reflect_class_info($class);"` |
|     - | 1816 | `"  if($target === null){"` |
|     - | 1817 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 1818 | `"  }"` |
|     - | 1819 | `"  $name = $target['name'];"` |
|     - | 1820 | `"  if($name === $this->name){ return false; }"` |
|     - | 1821 | `"  $i = $this->__rinfo();"` |
|     - | 1822 | `"  $p = $i['parent'];"` |
|     - | 1823 | `"  while($p !== null){"` |
|     - | 1824 | `"   if($p === $name){ return true; }"` |
|     - | 1825 | `"   $pi = __reflect_class_info($p);"` |
|     - | 1826 | `"   $p = $pi['parent'];"` |
|     - | 1827 | `"  }"` |
|     - | 1828 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1829 | `"  return false;"` |
|     - | 1830 | `" }"` |
|     - | 1831 | `" public function isInstance($object){"` |
|     - | 1832 | `"  if(!is_object($object)){"` |
|     - | 1833 | `"   throw new TypeError('ReflectionClass::isInstance(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 1834 | `"  }"` |
|     - | 1835 | `"  return is_a($object,$this->name);"` |
|     - | 1836 | `" }"` |
|     - | 1837 | `" public function hasMethod($name){"` |
|     - | 1838 | `"  $i = $this->__rinfo();"` |
|     - | 1839 | `"  $l = strtolower($name);"` |
|     - | 1840 | `"  foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ return true; } }"` |
|     - | 1841 | `"  return false;"` |
|     - | 1842 | `" }"` |
|     - | 1843 | `" public function hasProperty($name){"` |
|     - | 1844 | `"  $i = $this->__rinfo();"` |
|     - | 1845 | `"  if(isset($i['props'][$name])){ return true; }"` |
|     - | 1846 | `"  if($this->__obj !== null){ return (__reflect_prop_state($this->__obj, $name) & 1) !== 0; }"` |
|     - | 1847 | `"  return false;"` |
|     - | 1848 | `" }"` |
|     - | 1849 | `" public function hasConstant($name){ $i = $this->__rinfo(); return isset($i['consts'][$name]); }"` |
|     - | 1850 | `" public function getConstant($name){"` |
|     - | 1851 | `"  $i = $this->__rinfo();"` |
|     - | 1852 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 1853 | `"  return __reflect_const_value($this->name,$name);"` |
|     - | 1854 | `" }"` |
|     - | 1855 | `" public function getConstants($filter = null){"` |
|     - | 1856 | `"  $i = $this->__rinfo();"` |
|     - | 1857 | `"  $out = array();"` |
|     - | 1858 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 1859 | `"   if($filter !== null){"` |
|     - | 1860 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 1861 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 1862 | `"   }"` |
|     - | 1863 | `"   $out[$k] = __reflect_const_value($this->name,$k);"` |
|     - | 1864 | `"  }"` |
|     - | 1865 | `"  return $out;"` |
|     - | 1866 | `" }"` |
|     - | 1867 | `" public function getStartLine(){"` |
|     - | 1868 | `"  $i = $this->__rinfo();"` |
|     - | 1869 | `"  if($i['internal']){ return false; }"` |
|     - | 1870 | `"  return $i['line'];"` |
|     - | 1871 | `" }"` |
|     - | 1872 | `" public function getEndLine(){"` |
|     - | 1873 | `"  $i = $this->__rinfo();"` |
|     - | 1874 | `"  if($i['internal']){ return false; }"` |
|     - | 1875 | `"  return $i['endline'];"` |
|     - | 1876 | `" }"` |
|     - | 1877 | `" public function getFileName(){ $i = $this->__rinfo(); return $i['file']; }"` |
|     - | 1878 | `" public function getDocComment(){ $i = $this->__rinfo(); return $i['doc']; }"` |
|     - | 1879 | `" public function isInstantiable(){"` |
|     - | 1880 | `"  $i = $this->__rinfo();"` |
|     - | 1881 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract'] \|\| $i['enum']){ return false; }"` |
|     - | 1882 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){ return false; }"` |
|     - | 1883 | `"  return true;"` |
|     - | 1884 | `" }"` |
|     - | 1885 | `" public function isCloneable(){"` |
|     - | 1886 | `"  $i = $this->__rinfo();"` |
|     - | 1887 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1888 | `"  if($i['clonevis'] !== 0 && $i['clonevis'] !== 1){ return false; }"` |
|     - | 1889 | `"  return true;"` |
|     - | 1890 | `" }"` |
|     - | 1891 | `" public function isIterable(){"` |
|     - | 1892 | `"  $i = $this->__rinfo();"` |
|     - | 1893 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1894 | `"  return $i['iterable'];"` |
|     - | 1895 | `" }"` |
|     - | 1896 | `" public function isIterateable(){ return $this->isIterable(); }"` |
|     - | 1897 | `" public function newInstance(...$args){ return $this->__rnew($args); }"` |
|     - | 1898 | `" public function newInstanceArgs(array $args = array()){ return $this->__rnew($args); }"` |
|     - | 1899 | `" protected function __rnew($args){"` |
|     - | 1900 | `"  $i = $this->__rinfo();"` |
|     - | 1901 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1902 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){"` |
|     - | 1903 | `"   throw new ReflectionException('Access to non-public constructor of class '.$this->name);"` |
|     - | 1904 | `"  }"` |
|     - | 1905 | `"  if($i['ctorvis'] === 0 && count($args) > 0){"` |
|     - | 1906 | `"   throw new ReflectionException('Class '.$this->name.' does not have a constructor, so you cannot pass any constructor arguments');"` |
|     - | 1907 | `"  }"` |
|     - | 1908 | `"  return __reflect_new_instance($this->name,$args);"` |
|     - | 1909 | `" }"` |
|     - | 1910 | `" protected function __rcheckInstantiable($i){"` |
|     - | 1911 | `"  if($i['interface']){ throw new Error('Cannot instantiate interface '.$this->name); }"` |
|     - | 1912 | `"  if($i['trait']){ throw new Error('Cannot instantiate trait '.$this->name); }"` |
|     - | 1913 | `"  if($i['abstract']){ throw new Error('Cannot instantiate abstract class '.$this->name); }"` |
|     - | 1914 | `" }"` |
|     - | 1915 | `" public function newInstanceWithoutConstructor(){"` |
|     - | 1916 | `"  $i = $this->__rinfo();"` |
|     - | 1917 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1918 | `"  return __reflect_new_no_ctor($this->name);"` |
|     - | 1919 | `" }"` |
|     - | 1920 | `" public function getStaticProperties(){"` |
|     - | 1921 | `"  $i = $this->__rinfo();"` |
|     - | 1922 | `"  $out = array();"` |
|     - | 1923 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1924 | `"   if($p['static']){ $out[$k] = __reflect_static_value($this->name,$k); }"` |
|     - | 1925 | `"  }"` |
|     - | 1926 | `"  return $out;"` |
|     - | 1927 | `" }"` |
|     - | 1928 | `" public function getStaticPropertyValue($name, ...$def){"` |
|     - | 1929 | `"  $i = $this->__rinfo();"` |
|     - | 1930 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 1931 | `"   if(count($def) > 0){ return $def[0]; }"` |
|     - | 1932 | `"   throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 1933 | `"  }"` |
|     - | 1934 | `"  return __reflect_static_value($this->name,$name);"` |
|     - | 1935 | `" }"` |
|     - | 1936 | `" public function setStaticPropertyValue($name,$value){"` |
|     - | 1937 | `"  $i = $this->__rinfo();"` |
|     - | 1938 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 1939 | `"   throw new ReflectionException('Class '.$this->name.' does not have a property named '.$name);"` |
|     - | 1940 | `"  }"` |
|     - | 1941 | `"  __reflect_static_set($this->name,$name,$value);"` |
|     - | 1942 | `" }"` |
|     - | 1943 | `" public function getDefaultProperties(){"` |
|     - | 1944 | `"  $i = $this->__rinfo();"` |
|     - | 1945 | `"  $out = array();"` |
|     - | 1946 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1947 | `"   if($p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 1948 | `"  }"` |
|     - | 1949 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1950 | `"   if(!$p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 1951 | `"  }"` |
|     - | 1952 | `"  return $out;"` |
|     - | 1953 | `" }"` |
|     - | 1954 | `" public function getProperty($name){"` |
|     - | 1955 | `"  $i = $this->__rinfo();"` |
|     - | 1956 | `"  if(isset($i['props'][$name])){"` |
|     - | 1957 | `"   return new ReflectionProperty($this->name, $name);"` |
|     - | 1958 | `"  }"` |
|     - | 1959 | `"  if($this->__obj !== null && (__reflect_prop_state($this->__obj, $name) & 1)){"` |
|     - | 1960 | `"   return new ReflectionProperty($this->__obj, $name);"` |
|     - | 1961 | `"  }"` |
|     - | 1962 | `"  throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 1963 | `" }"` |
|     - | 1964 | `" public function getProperties($filter = null){"` |
|     - | 1965 | `"  $i = $this->__rinfo();"` |
|     - | 1966 | `"  $out = array();"` |
|     - | 1967 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1968 | `"   if($filter !== null){"` |
|     - | 1969 | `"    $m = ($p['vis'] === 1 ? 1 : ($p['vis'] === 2 ? 2 : 4));"` |
|     - | 1970 | `"    if($p['static']){ $m \|= 16; }"` |
|     - | 1971 | `"    if($p['readonly']){ $m \|= 128; }"` |
|     - | 1972 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 1973 | `"   }"` |
|     - | 1974 | `"   $out[] = new ReflectionProperty($this->name, $k);"` |
|     - | 1975 | `"  }"` |
|     - | 1976 | `"  if($this->__obj !== null){"` |
|     - | 1977 | `"   foreach(__reflect_dyn_props($this->__obj) as $k){"` |
|     - | 1978 | `"    if(isset($i['props'][$k])){ continue; }"` |
|     - | 1979 | `"    if($filter !== null && ($filter & 1) === 0){ continue; }"` |
|     - | 1980 | `"    $out[] = new ReflectionProperty($this->__obj, $k);"` |
|     - | 1981 | `"   }"` |
|     - | 1982 | `"  }"` |
|     - | 1983 | `"  return $out;"` |
|     - | 1984 | `" }"` |
|     - | 1985 | `" public function getMethod($name){"` |
|     - | 1986 | `"  $i = $this->__rinfo();"` |
|     - | 1987 | `"  $found = null;"` |
|     - | 1988 | `"  if(isset($i['methods'][$name])){"` |
|     - | 1989 | `"   $found = $name;"` |
|     - | 1990 | `"  }else{"` |
|     - | 1991 | `"   $l = strtolower($name);"` |
|     - | 1992 | `"   foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ $found = $k; break; } }"` |
|     - | 1993 | `"  }"` |
|     - | 1994 | `"  if($found === null){"` |
|     - | 1995 | `"   throw new ReflectionException('Method '.$this->name.'::'.$name.'() does not exist');"` |
|     - | 1996 | `"  }"` |
|     - | 1997 | `"  return new ReflectionMethod($this->name, $found);"` |
|     - | 1998 | `" }"` |
|     - | 1999 | `" public function getMethods($filter = null){"` |
|     - | 2000 | `"  $i = $this->__rinfo();"` |
|     - | 2001 | `"  $out = array();"` |
|     - | 2002 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 2003 | `"   if($filter !== null){"` |
|     - | 2004 | `"    $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2005 | `"    if($m['static']){ $mod \|= 16; }"` |
|     - | 2006 | `"    if($m['abstract']){ $mod \|= 64; }"` |
|     - | 2007 | `"    if($m['final']){ $mod \|= 32; }"` |
|     - | 2008 | `"    if(($mod & $filter) === 0){ continue; }"` |
|     - | 2009 | `"   }"` |
|     - | 2010 | `"   $out[] = new ReflectionMethod($this->name, $k);"` |
|     - | 2011 | `"  }"` |
|     - | 2012 | `"  return $out;"` |
|     - | 2013 | `" }"` |
|     - | 2014 | `" public function getConstructor(){"` |
|     - | 2015 | `"  $i = $this->__rinfo();"` |
|     - | 2016 | `"  if(isset($i['methods']['__construct'])){"` |
|     - | 2017 | `"   return new ReflectionMethod($this->name, '__construct');"` |
|     - | 2018 | `"  }"` |
|     - | 2019 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 2020 | `"   if(strtolower($k) === '__construct'){ return new ReflectionMethod($this->name, $k); }"` |
|     - | 2021 | `"  }"` |
|     - | 2022 | `"  if($i['ctorvis'] !== 0 && isset($i['methods'][$this->name])){"` |
|     - | 2023 | `"   return new ReflectionMethod($this->name, $this->name);"` |
|     - | 2024 | `"  }"` |
|     - | 2025 | `"  return null;"` |
|     - | 2026 | `" }"` |
|     - | 2027 | `" public function getReflectionConstant($name){"` |
|     - | 2028 | `"  $i = $this->__rinfo();"` |
|     - | 2029 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 2030 | `"  return new ReflectionClassConstant($this->name, $name);"` |
|     - | 2031 | `" }"` |
|     - | 2032 | `" public function getReflectionConstants($filter = null){"` |
|     - | 2033 | `"  $i = $this->__rinfo();"` |
|     - | 2034 | `"  $out = array();"` |
|     - | 2035 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 2036 | `"   if($filter !== null){"` |
|     - | 2037 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 2038 | `"    if($c['final']){ $m \|= 32; }"` |
|     - | 2039 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 2040 | `"   }"` |
|     - | 2041 | `"   $out[] = new ReflectionClassConstant($this->name, $k);"` |
|     - | 2042 | `"  }"` |
|     - | 2043 | `"  return $out;"` |
|     - | 2044 | `" }"` |
|     - | 2045 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2046 | `"  $i = $this->__rinfo();"` |
|     - | 2047 | `"  return __reflect_build_attrs($i['attrs'], array('class', $this->name, null, 0), 1, $name, $flags);"` |
|     - | 2048 | `" }"` |
|     - | 2049 | `" public function getExtensionName(){ $i = $this->__rinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2050 | `" public function getExtension(){ $i = $this->__rinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2051 | `" public function newLazyGhost($initializer, $options = 0){"` |
|     - | 2052 | `"  throw new Error('ReflectionClass::newLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2053 | `" }"` |
|     - | 2054 | `" public function newLazyProxy($factory, $options = 0){"` |
|     - | 2055 | `"  throw new Error('ReflectionClass::newLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2056 | `" }"` |
|     - | 2057 | `" public function resetAsLazyGhost($object, $initializer, $options = 0){"` |
|     - | 2058 | `"  throw new Error('ReflectionClass::resetAsLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2059 | `" }"` |
|     - | 2060 | `" public function resetAsLazyProxy($object, $factory, $options = 0){"` |
|     - | 2061 | `"  throw new Error('ReflectionClass::resetAsLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2062 | `" }"` |
|     - | 2063 | `" public function getLazyInitializer($object){ return null; }"` |
|     - | 2064 | `" public function initializeLazyObject($object){ return $object; }"` |
|     - | 2065 | `" public function markLazyObjectAsInitialized($object){ return $object; }"` |
|     - | 2066 | `" public function isUninitializedLazyObject($object){ return false; }"` |
|     - | 2067 | `" public function __toString(){ return __reflect_export_class($this); }"` |
|     - | 2068 | `"}"` |
|     - | 2069 | `"class ReflectionObject extends ReflectionClass {"` |
|     - | 2070 | `" public function __construct($object){"` |
|     - | 2071 | `"  if(!is_object($object)){"` |
|     - | 2072 | `"   throw new TypeError('ReflectionObject::__construct(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 2073 | `"  }"` |
|     - | 2074 | `"  parent::__construct($object);"` |
|     - | 2075 | `"  $this->__obj = $object;"` |
|     - | 2076 | `" }"` |
|     - | 2077 | `"}"` |
|     - | 2078 | `;` |
|     - | 2079 | `/*` |
|     - | 2080 | ` * Chunk 2: ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,` |
|     - | 2081 | ` * ReflectionParameter.` |
|     - | 2082 | ` */` |
|     - | 2083 | `static const char zReflectLib2[] =` |
|     - | 2084 | `"abstract class ReflectionFunctionAbstract implements Reflector {"` |
|     - | 2085 | `" public $name;"` |
|     - | 2086 | `" protected $__cl = null;"` |
|     - | 2087 | `" protected function __rfinfo(){"` |
|     - | 2088 | `"  if($this->__cl !== null){ return __reflect_sig_fixup(__reflect_func_info($this->__cl)); }"` |
|     - | 2089 | `"  return __reflect_sig_fixup(__reflect_func_info($this->name));"` |
|     - | 2090 | `" }"` |
|     - | 2091 | `" protected function __rftarget(){ return $this->__cl !== null ? $this->__cl : $this->name; }"` |
|     - | 2092 | `" protected function __rpspec(){ return $this->__rftarget(); }"` |
|     - | 2093 | `" public function getName(){ return $this->name; }"` |
|     - | 2094 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 2095 | `" public function getNamespaceName(){"` |
|     - | 2096 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2097 | `"  if($p === false){ return ''; }"` |
|     - | 2098 | `"  return substr($this->name,0,$p);"` |
|     - | 2099 | `" }"` |
|     - | 2100 | `" public function getShortName(){"` |
|     - | 2101 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2102 | `"  if($p === false){ return $this->name; }"` |
|     - | 2103 | `"  return substr($this->name,$p+1);"` |
|     - | 2104 | `" }"` |
|     - | 2105 | `" public function isClosure(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2106 | `" public function isGenerator(){ $i = $this->__rfinfo(); return $i['generator']; }"` |
|     - | 2107 | `" public function isVariadic(){ $i = $this->__rfinfo(); return $i['variadic']; }"` |
|     - | 2108 | `" public function returnsReference(){ $i = $this->__rfinfo(); return $i['byref']; }"` |
|     - | 2109 | `" public function isInternal(){ $i = $this->__rfinfo(); return $i['internal']; }"` |
|     - | 2110 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 2111 | `" public function isDeprecated(){ $i = $this->__rfinfo(); return __reflect_has_deprecated($i['attrs']); }"` |
|     - | 2112 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['fstatic']; }"` |
|     - | 2113 | `" public function getFileName(){ $i = $this->__rfinfo(); return $i['file']; }"` |
|     - | 2114 | `" public function getStartLine(){"` |
|     - | 2115 | `"  $i = $this->__rfinfo();"` |
|     - | 2116 | `"  if($i['internal']){ return false; }"` |
|     - | 2117 | `"  return $i['line'];"` |
|     - | 2118 | `" }"` |
|     - | 2119 | `" public function getEndLine(){"` |
|     - | 2120 | `"  $i = $this->__rfinfo();"` |
|     - | 2121 | `"  if($i['internal']){ return false; }"` |
|     - | 2122 | `"  return $i['endline'];"` |
|     - | 2123 | `" }"` |
|     - | 2124 | `" public function getDocComment(){ $i = $this->__rfinfo(); return $i['doc']; }"` |
|     - | 2125 | `" public function hasReturnType(){ $i = $this->__rfinfo(); return $i['rettext'] !== null; }"` |
|     - | 2126 | `" public function getReturnType(){ $i = $this->__rfinfo(); return __reflect_make_type($i['rettext']); }"` |
|     - | 2127 | `" public function hasTentativeReturnType(){ return false; }"` |
|     - | 2128 | `" public function getTentativeReturnType(){ return null; }"` |
|     - | 2129 | `" public function getNumberOfParameters(){"` |
|     - | 2130 | `"  $i = $this->__rfinfo();"` |
|     - | 2131 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2132 | `"  return count($i['params']);"` |
|     - | 2133 | `" }"` |
|     - | 2134 | `" public function getNumberOfRequiredParameters(){"` |
|     - | 2135 | `"  $i = $this->__rfinfo();"` |
|     - | 2136 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2137 | `"  $req = 0;"` |
|     - | 2138 | `"  $n = count($i['params']);"` |
|     - | 2139 | `"  for($k = $n - 1; $k >= 0; $k--){"` |
|     - | 2140 | `"   $p = $i['params'][$k];"` |
|     - | 2141 | `"   if(!$p['variadic'] && !$p['hasdef']){ $req = $k + 1; break; }"` |
|     - | 2142 | `"  }"` |
|     - | 2143 | `"  return $req;"` |
|     - | 2144 | `" }"` |
|     - | 2145 | `" public function getParameters(){"` |
|     - | 2146 | `"  $i = $this->__rfinfo();"` |
|     - | 2147 | `"  $out = array();"` |
|     - | 2148 | `"  $spec = $this->__rpspec();"` |
|     - | 2149 | `"  foreach($i['params'] as $p){"` |
|     - | 2150 | `"   $out[] = new ReflectionParameter($spec, $p['pos']);"` |
|     - | 2151 | `"  }"` |
|     - | 2152 | `"  return $out;"` |
|     - | 2153 | `" }"` |
|     - | 2154 | `" public function getStaticVariables(){ $i = $this->__rfinfo(); return $i['statics']; }"` |
|     - | 2155 | `" public function getClosureThis(){"` |
|     - | 2156 | `"  $i = $this->__rfinfo();"` |
|     - | 2157 | `"  return isset($i['this']) ? $i['this'] : null;"` |
|     - | 2158 | `" }"` |
|     - | 2159 | `" public function getClosureScopeClass(){"` |
|     - | 2160 | `"  $i = $this->__rfinfo();"` |
|     - | 2161 | `"  if(isset($i['scope'])){ return new ReflectionClass($i['scope']); }"` |
|     - | 2162 | `"  if(isset($i['this'])){ return new ReflectionClass(get_class($i['this'])); }"` |
|     - | 2163 | `"  return null;"` |
|     - | 2164 | `" }"` |
|     - | 2165 | `" public function getClosureCalledClass(){ return $this->getClosureScopeClass(); }"` |
|     - | 2166 | `" public function getClosureUsedVariables(){"` |
|     - | 2167 | `"  $i = $this->__rfinfo();"` |
|     - | 2168 | `"  return isset($i['used']) ? $i['used'] : array();"` |
|     - | 2169 | `" }"` |
|     - | 2170 | `" public function getExtensionName(){ $i = $this->__rfinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2171 | `" public function getExtension(){ $i = $this->__rfinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2172 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2173 | `"  $i = $this->__rfinfo();"` |
|     - | 2174 | `"  if($this instanceof ReflectionMethod){"` |
|     - | 2175 | `"   $spec = array('method', $this->class, $this->name, 0);"` |
|     - | 2176 | `"   $target = 4;"` |
|     - | 2177 | `"  }else{"` |
|     - | 2178 | `"   $spec = array('fn', $this->__rftarget(), null, 0);"` |
|     - | 2179 | `"   $target = 2;"` |
|     - | 2180 | `"  }"` |
|     - | 2181 | `"  return __reflect_build_attrs($i['attrs'], $spec, $target, $name, $flags);"` |
|     - | 2182 | `" }"` |
|     - | 2183 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2184 | `"}"` |
|     - | 2185 | `"class ReflectionFunction extends ReflectionFunctionAbstract {"` |
|     - | 2186 | `" const IS_DEPRECATED = 2048;"` |
|     - | 2187 | `" public function __construct($function){"` |
|     - | 2188 | `"  if($function instanceof Closure){"` |
|     - | 2189 | `"   $this->__cl = $function;"` |
|     - | 2190 | `"   $i = $this->__rfinfo();"` |
|     - | 2191 | `"   if($i['closure']){"` |
|     - | 2192 | `"    $f = $i['file'] === false ? '' : $i['file'];"` |
|     - | 2193 | `"    $this->name = '{closure:'.$f.':'.$i['line'].'}';"` |
|     - | 2194 | `"   }else{"` |
|     - | 2195 | `"    $this->name = $i['name'];"` |
|     - | 2196 | `"   }"` |
|     - | 2197 | `"   return;"` |
|     - | 2198 | `"  }"` |
|     - | 2199 | `"  if(!is_string($function)){"` |
|     - | 2200 | `"   throw new TypeError('ReflectionFunction::__construct(): Argument #1 ($function) must be of type Closure\|string, '.get_debug_type($function).' given');"` |
|     - | 2201 | `"  }"` |
|     - | 2202 | `"  $i = __reflect_func_info($function);"` |
|     - | 2203 | `"  if($i === null){"` |
|     - | 2204 | `"   throw new ReflectionException('Function '.$function.'() does not exist');"` |
|     - | 2205 | `"  }"` |
|     - | 2206 | `"  if($i['closure']){"` |
|     - | 2207 | `"   $this->name = '{closure:'.($i['file'] === false ? '' : $i['file']).':'.$i['line'].'}';"` |
|     - | 2208 | `"   $this->__cl = __reflect_closure($function, null, null);"` |
|     - | 2209 | `"  }else{"` |
|     - | 2210 | `"   $this->name = $i['name'];"` |
|     - | 2211 | `"  }"` |
|     - | 2212 | `" }"` |
|     - | 2213 | `" public function invoke(...$args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2214 | `" public function invokeArgs(array $args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2215 | `" public function getClosure(){"` |
|     - | 2216 | `"  if($this->__cl !== null){ return $this->__cl; }"` |
|     - | 2217 | `"  return __reflect_closure($this->name, null, null);"` |
|     - | 2218 | `" }"` |
|     - | 2219 | `" public function isAnonymous(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2220 | `" public function isDisabled(){ return false; }"` |
|     - | 2221 | `"}"` |
|     - | 2222 | `"class ReflectionMethod extends ReflectionFunctionAbstract {"` |
|     - | 2223 | `" const IS_PUBLIC = 1;"` |
|     - | 2224 | `" const IS_PROTECTED = 2;"` |
|     - | 2225 | `" const IS_PRIVATE = 4;"` |
|     - | 2226 | `" const IS_STATIC = 16;"` |
|     - | 2227 | `" const IS_FINAL = 32;"` |
|     - | 2228 | `" const IS_ABSTRACT = 64;"` |
|     - | 2229 | `" public $class;"` |
|     - | 2230 | `" public function __construct($objectOrMethod, $method = null){"` |
|     - | 2231 | `"  if($method === null){"` |
|     - | 2232 | `"   if(!is_string($objectOrMethod) \|\| strpos($objectOrMethod,'::') === false){"` |
|     - | 2233 | `"    throw new TypeError('ReflectionMethod::__construct(): Argument #1 ($objectOrMethod) must be of type object\|string, '.get_debug_type($objectOrMethod).' given');"` |
|     - | 2234 | `"   }"` |
|     - | 2235 | `"   $p = strpos($objectOrMethod,'::');"` |
|     - | 2236 | `"   $method = substr($objectOrMethod,$p+2);"` |
|     - | 2237 | `"   $objectOrMethod = substr($objectOrMethod,0,$p);"` |
|     - | 2238 | `"  }"` |
|     - | 2239 | `"  $ci = __reflect_class_info($objectOrMethod);"` |
|     - | 2240 | `"  if($ci === null){"` |
|     - | 2241 | `"   throw new ReflectionException('Class \"'.$objectOrMethod.'\" does not exist');"` |
|     - | 2242 | `"  }"` |
|     - | 2243 | `"  $this->class = $ci['name'];"` |
|     - | 2244 | `"  $found = null;"` |
|     - | 2245 | `"  if(isset($ci['methods'][$method])){"` |
|     - | 2246 | `"   $found = $method;"` |
|     - | 2247 | `"  }else{"` |
|     - | 2248 | `"   $l = strtolower($method);"` |
|     - | 2249 | `"   foreach($ci['methods'] as $k => $m){"` |
|     - | 2250 | `"    if(strtolower($k) === $l){ $found = $k; break; }"` |
|     - | 2251 | `"   }"` |
|     - | 2252 | `"  }"` |
|     - | 2253 | `"  if($found === null){"` |
|     - | 2254 | `"   throw new ReflectionException('Method '.$this->class.'::'.$method.'() does not exist');"` |
|     - | 2255 | `"  }"` |
|     - | 2256 | `"  $this->name = $found;"` |
|     - | 2257 | `" }"` |
|     - | 2258 | `" public static function createFromMethodName($name){"` |
|     - | 2259 | `"  return new ReflectionMethod($name);"` |
|     - | 2260 | `" }"` |
|     - | 2261 | `" protected function __rfinfo(){ return __reflect_func_info($this->class, $this->name); }"` |
|     - | 2262 | `" protected function __rpspec(){ return array($this->class, $this->name); }"` |
|     - | 2263 | `" public function getDeclaringClass(){"` |
|     - | 2264 | `"  $i = $this->__rfinfo();"` |
|     - | 2265 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2266 | `" }"` |
|     - | 2267 | `" public function getModifiers(){"` |
|     - | 2268 | `"  $i = $this->__rfinfo();"` |
|     - | 2269 | `"  $m = ($i['vis'] === 1 ? 1 : ($i['vis'] === 2 ? 2 : 4));"` |
|     - | 2270 | `"  if($i['mstatic']){ $m \|= 16; }"` |
|     - | 2271 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 2272 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 2273 | `"  return $m;"` |
|     - | 2274 | `" }"` |
|     - | 2275 | `" public function isPublic(){ $i = $this->__rfinfo(); return $i['vis'] === 1; }"` |
|     - | 2276 | `" public function isProtected(){ $i = $this->__rfinfo(); return $i['vis'] === 2; }"` |
|     - | 2277 | `" public function isPrivate(){ $i = $this->__rfinfo(); return $i['vis'] === 3; }"` |
|     - | 2278 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['mstatic']; }"` |
|     - | 2279 | `" public function isAbstract(){ $i = $this->__rfinfo(); return $i['abstract']; }"` |
|     - | 2280 | `" public function isFinal(){ $i = $this->__rfinfo(); return $i['final']; }"` |
|     - | 2281 | `" public function isConstructor(){ return strtolower($this->name) === '__construct'; }"` |
|     - | 2282 | `" public function isDestructor(){ return strtolower($this->name) === '__destruct'; }"` |
|     - | 2283 | `" public function invoke($object = null, ...$args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2284 | `" public function invokeArgs($object, array $args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2285 | `" protected function __rinvoke($object, $args){"` |
|     - | 2286 | `"  $i = $this->__rfinfo();"` |
|     - | 2287 | `"  if(!$i['mstatic']){"` |
|     - | 2288 | `"   if(!is_object($object)){"` |
|     - | 2289 | `"    throw new ReflectionException('Trying to invoke non static method '.$this->class.'::'.$this->name.'() without an object');"` |
|     - | 2290 | `"   }"` |
|     - | 2291 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2292 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2293 | `"   }"` |
|     - | 2294 | `"  }else{"` |
|     - | 2295 | `"   $object = null;"` |
|     - | 2296 | `"  }"` |
|     - | 2297 | `"  return __reflect_invoke($this->class, $this->name, $object, $args);"` |
|     - | 2298 | `" }"` |
|     - | 2299 | `" public function getClosure($object = null){"` |
|     - | 2300 | `"  $i = $this->__rfinfo();"` |
|     - | 2301 | `"  if(!$i['mstatic']){"` |
|     - | 2302 | `"   if($object === null){"` |
|     - | 2303 | `"    throw new ValueError('ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods');"` |
|     - | 2304 | `"   }"` |
|     - | 2305 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2306 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2307 | `"   }"` |
|     - | 2308 | `"  }else{"` |
|     - | 2309 | `"   $object = null;"` |
|     - | 2310 | `"  }"` |
|     - | 2311 | `"  return __reflect_closure($this->class, $this->name, $object);"` |
|     - | 2312 | `" }"` |
|     - | 2313 | `" public function setAccessible($accessible){ }"` |
|     - | 2314 | `" public function hasPrototype(){ return $this->__rproto() !== null; }"` |
|     - | 2315 | `" public function getPrototype(){"` |
|     - | 2316 | `"  $p = $this->__rproto();"` |
|     - | 2317 | `"  if($p === null){"` |
|     - | 2318 | `"   throw new ReflectionException('Method '.$this->class.'::'.$this->name.' does not have a prototype');"` |
|     - | 2319 | `"  }"` |
|     - | 2320 | `"  return new ReflectionMethod($p, $this->name);"` |
|     - | 2321 | `" }"` |
|     - | 2322 | `" protected function __rproto(){"` |
|     - | 2323 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2324 | `"  $l = strtolower($this->name);"` |
|     - | 2325 | `"  $p = $ci['parent'];"` |
|     - | 2326 | `"  while($p !== null){"` |
|     - | 2327 | `"   $pi = __reflect_class_info($p);"` |
|     - | 2328 | `"   foreach($pi['methods'] as $k => $m){"` |
|     - | 2329 | `"    if(strtolower($k) === $l && $m['vis'] !== 3){ return $m['decl']; }"` |
|     - | 2330 | `"   }"` |
|     - | 2331 | `"   $p = $pi['parent'];"` |
|     - | 2332 | `"  }"` |
|     - | 2333 | `"  foreach($ci['interfaces'] as $if){"` |
|     - | 2334 | `"   $ii = __reflect_class_info($if);"` |
|     - | 2335 | `"   foreach($ii['methods'] as $k => $m){"` |
|     - | 2336 | `"    if(strtolower($k) === $l){ return $ii['name']; }"` |
|     - | 2337 | `"   }"` |
|     - | 2338 | `"  }"` |
|     - | 2339 | `"  return null;"` |
|     - | 2340 | `" }"` |
|     - | 2341 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2342 | `"}"` |
|     - | 2343 | `"class ReflectionParameter implements Reflector {"` |
|     - | 2344 | `" public $name;"` |
|     - | 2345 | `" protected $__t;"` |
|     - | 2346 | `" protected $__m = null;"` |
|     - | 2347 | `" protected $__p = 0;"` |
|     - | 2348 | `" public function __construct($function, $param){"` |
|     - | 2349 | `"  $m = null;"` |
|     - | 2350 | `"  $t = $function;"` |
|     - | 2351 | `"  if(is_array($function)){"` |
|     - | 2352 | `"   $t = $function[0];"` |
|     - | 2353 | `"   $m = $function[1];"` |
|     - | 2354 | `"   if(is_object($t)){ $t = get_class($t); }"` |
|     - | 2355 | `"  }else if(is_string($function) && strpos($function,'::') !== false){"` |
|     - | 2356 | `"   $p = strpos($function,'::');"` |
|     - | 2357 | `"   $m = substr($function,$p+2);"` |
|     - | 2358 | `"   $t = substr($function,0,$p);"` |
|     - | 2359 | `"  }"` |
|     - | 2360 | `"  if($m !== null){"` |
|     - | 2361 | `"   $rm = new ReflectionMethod($t, $m);"` |
|     - | 2362 | `"   $t = $rm->class;"` |
|     - | 2363 | `"   $m = $rm->name;"` |
|     - | 2364 | `"   $i = __reflect_func_info($t, $m);"` |
|     - | 2365 | `"  }else if($function instanceof Closure){"` |
|     - | 2366 | `"   $t = $function;"` |
|     - | 2367 | `"   $i = __reflect_func_info($function);"` |
|     - | 2368 | `"  }else{"` |
|     - | 2369 | `"   $i = __reflect_sig_fixup(__reflect_func_info($t));"` |
|     - | 2370 | `"   if($i === null){"` |
|     - | 2371 | `"    throw new ReflectionException('Function '.$t.'() does not exist');"` |
|     - | 2372 | `"   }"` |
|     - | 2373 | `"  }"` |
|     - | 2374 | `"  $found = null;"` |
|     - | 2375 | `"  if(is_int($param)){"` |
|     - | 2376 | `"   if(isset($i['params'][$param])){ $found = $i['params'][$param]; }"` |
|     - | 2377 | `"   if($found === null){"` |
|     - | 2378 | `"    throw new ReflectionException('The parameter specified by its offset could not be found');"` |
|     - | 2379 | `"   }"` |
|     - | 2380 | `"  }else{"` |
|     - | 2381 | `"   foreach($i['params'] as $pp){"` |
|     - | 2382 | `"    if($pp['name'] === $param){ $found = $pp; break; }"` |
|     - | 2383 | `"   }"` |
|     - | 2384 | `"   if($found === null){"` |
|     - | 2385 | `"    throw new ReflectionException('The parameter specified by its name could not be found');"` |
|     - | 2386 | `"   }"` |
|     - | 2387 | `"  }"` |
|     - | 2388 | `"  $this->name = $found['name'];"` |
|     - | 2389 | `"  $this->__t = $t;"` |
|     - | 2390 | `"  $this->__m = $m;"` |
|     - | 2391 | `"  $this->__p = $found['pos'];"` |
|     - | 2392 | `" }"` |
|     - | 2393 | `" protected function __rffull(){"` |
|     - | 2394 | `"  if($this->__m !== null){ return __reflect_func_info($this->__t, $this->__m); }"` |
|     - | 2395 | `"  return __reflect_sig_fixup(__reflect_func_info($this->__t));"` |
|     - | 2396 | `" }"` |
|     - | 2397 | `" protected function __rpinfo(){"` |
|     - | 2398 | `"  $i = $this->__rffull();"` |
|     - | 2399 | `"  return $i['params'][$this->__p];"` |
|     - | 2400 | `" }"` |
|     - | 2401 | `" public function getName(){ return $this->name; }"` |
|     - | 2402 | `" public function getPosition(){ return $this->__p; }"` |
|     - | 2403 | `" public function isPassedByReference(){ $p = $this->__rpinfo(); return $p['byref']; }"` |
|     - | 2404 | `" public function canBePassedByValue(){ return !$this->isPassedByReference(); }"` |
|     - | 2405 | `" public function isVariadic(){ $p = $this->__rpinfo(); return $p['variadic']; }"` |
|     - | 2406 | `" public function isPromoted(){ $p = $this->__rpinfo(); return $p['promoted']; }"` |
|     - | 2407 | `" public function isDefaultValueAvailable(){ $p = $this->__rpinfo(); return $p['hasdef']; }"` |
|     - | 2408 | `" public function isOptional(){"` |
|     - | 2409 | `"  $i = $this->__rffull();"` |
|     - | 2410 | `"  $n = count($i['params']);"` |
|     - | 2411 | `"  for($k = $this->__p; $k < $n; $k++){"` |
|     - | 2412 | `"   $p = $i['params'][$k];"` |
|     - | 2413 | `"   if(!$p['variadic'] && !$p['hasdef']){ return false; }"` |
|     - | 2414 | `"  }"` |
|     - | 2415 | `"  return true;"` |
|     - | 2416 | `" }"` |
|     - | 2417 | `" public function getDefaultValue(){"` |
|     - | 2418 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2419 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2420 | `"  }"` |
|     - | 2421 | `"  $p = $this->__rpinfo();"` |
|     - | 2422 | `"  if(isset($p['deftext'])){"` |
|     - | 2423 | `"   $s = __reflect_sig_scalar($p['deftext']);"` |
|     - | 2424 | `"   if($s[0]){ return $s[1]; }"` |
|     - | 2425 | `"   if($p['deftext'] === 'array (' \|\| strpos($p['deftext'], '[') === 0){ return array(); }"` |
|     - | 2426 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2427 | `"  }"` |
|     - | 2428 | `"  return __reflect_param_default($this->__t, $this->__m, $this->__p);"` |
|     - | 2429 | `" }"` |
|     - | 2430 | `" public function isDefaultValueConstant(){"` |
|     - | 2431 | `"  if(!$this->isDefaultValueAvailable()){ return false; }"` |
|     - | 2432 | `"  $p = $this->__rpinfo();"` |
|     - | 2433 | `"  if(isset($p['deftext'])){ return false; }"` |
|     - | 2434 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p) !== null;"` |
|     - | 2435 | `" }"` |
|     - | 2436 | `" public function getDefaultValueConstantName(){"` |
|     - | 2437 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2438 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2439 | `"  }"` |
|     - | 2440 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p);"` |
|     - | 2441 | `" }"` |
|     - | 2442 | `" public function allowsNull(){"` |
|     - | 2443 | `"  $p = $this->__rpinfo();"` |
|     - | 2444 | `"  if($p['typetext'] === null){ return true; }"` |
|     - | 2445 | `"  if($p['nullable']){ return true; }"` |
|     - | 2446 | `"  return $p['typetext'] === 'mixed' \|\| $p['typetext'] === 'null';"` |
|     - | 2447 | `" }"` |
|     - | 2448 | `" public function hasType(){ $p = $this->__rpinfo(); return $p['typetext'] !== null; }"` |
|     - | 2449 | `" public function getType(){ $p = $this->__rpinfo(); return __reflect_make_type($p['typetext']); }"` |
|     - | 2450 | `" public function getDeclaringFunction(){"` |
|     - | 2451 | `"  if($this->__m !== null){ return new ReflectionMethod($this->__t, $this->__m); }"` |
|     - | 2452 | `"  return new ReflectionFunction($this->__t);"` |
|     - | 2453 | `" }"` |
|     - | 2454 | `" public function getDeclaringClass(){"` |
|     - | 2455 | `"  if($this->__m === null){ return null; }"` |
|     - | 2456 | `"  $i = $this->__rffull();"` |
|     - | 2457 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2458 | `" }"` |
|     - | 2459 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2460 | `"  $p = $this->__rpinfo();"` |
|     - | 2461 | `"  return __reflect_build_attrs($p['attrs'], array('param', $this->__t, $this->__m, $this->__p), 32, $name, $flags);"` |
|     - | 2462 | `" }"` |
|     - | 2463 | `" public function __toString(){ return __reflect_export_param($this); }"` |
|     - | 2464 | `"}"` |
|     - | 2465 | `;` |
|     - | 2466 | `/*` |
|     - | 2467 | ` * Chunk 3: ReflectionProperty, ReflectionClassConstant.` |
|     - | 2468 | ` */` |
|     - | 2469 | `static const char zReflectLib3[] =` |
|     - | 2470 | `"class ReflectionProperty implements Reflector {"` |
|     - | 2471 | `" const IS_PUBLIC = 1;"` |
|     - | 2472 | `" const IS_PROTECTED = 2;"` |
|     - | 2473 | `" const IS_PRIVATE = 4;"` |
|     - | 2474 | `" const IS_STATIC = 16;"` |
|     - | 2475 | `" const IS_FINAL = 32;"` |
|     - | 2476 | `" const IS_ABSTRACT = 64;"` |
|     - | 2477 | `" const IS_READONLY = 128;"` |
|     - | 2478 | `" const IS_VIRTUAL = 512;"` |
|     - | 2479 | `" const IS_PROTECTED_SET = 2048;"` |
|     - | 2480 | `" const IS_PRIVATE_SET = 4096;"` |
|     - | 2481 | `" public $name;"` |
|     - | 2482 | `" public $class;"` |
|     - | 2483 | `" protected $__dynobj = null;"` |
|     - | 2484 | `" public function __construct($class, $property){"` |
|     - | 2485 | `"  $obj = null;"` |
|     - | 2486 | `"  if(is_object($class)){ $obj = $class; }"` |
|     - | 2487 | `"  else if(!is_string($class)){"` |
|     - | 2488 | `"   throw new TypeError('ReflectionProperty::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2489 | `"  }"` |
|     - | 2490 | `"  $ci = __reflect_class_info($class);"` |
|     - | 2491 | `"  if($ci === null){"` |
|     - | 2492 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2493 | `"  }"` |
|     - | 2494 | `"  $this->class = $ci['name'];"` |
|     - | 2495 | `"  if(isset($ci['props'][$property])){"` |
|     - | 2496 | `"   $this->name = $property;"` |
|     - | 2497 | `"   return;"` |
|     - | 2498 | `"  }"` |
|     - | 2499 | `"  if($obj !== null && (__reflect_prop_state($obj, $property) & 1)){"` |
|     - | 2500 | `"   $this->name = $property;"` |
|     - | 2501 | `"   $this->__dynobj = $obj;"` |
|     - | 2502 | `"   return;"` |
|     - | 2503 | `"  }"` |
|     - | 2504 | `"  throw new ReflectionException('Property '.$this->class.'::$'.$property.' does not exist');"` |
|     - | 2505 | `" }"` |
|     - | 2506 | `" protected function __rpmeta(){"` |
|     - | 2507 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2508 | `"  if(isset($ci['props'][$this->name])){ return $ci['props'][$this->name]; }"` |
|     - | 2509 | `"  return array('vis' => 1, 'static' => false, 'readonly' => false, 'hasdef' => false,"` |
|     - | 2510 | `"   'typed' => false, 'typetext' => null, 'decl' => $this->class, 'line' => 0, 'dyn' => true);"` |
|     - | 2511 | `" }"` |
|     - | 2512 | `" public function getName(){ return $this->name; }"` |
|     - | 2513 | `" public function getDeclaringClass(){"` |
|     - | 2514 | `"  $m = $this->__rpmeta();"` |
|     - | 2515 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2516 | `" }"` |
|     - | 2517 | `" public function getModifiers(){"` |
|     - | 2518 | `"  $m = $this->__rpmeta();"` |
|     - | 2519 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2520 | `"  if($m['static']){ $mod \|= 16; }"` |
|     - | 2521 | `"  if($m['readonly']){ $mod \|= 128; }"` |
|     - | 2522 | `"  return $mod;"` |
|     - | 2523 | `" }"` |
|     - | 2524 | `" public function isPublic(){ $m = $this->__rpmeta(); return $m['vis'] === 1; }"` |
|     - | 2525 | `" public function isProtected(){ $m = $this->__rpmeta(); return $m['vis'] === 2; }"` |
|     - | 2526 | `" public function isPrivate(){ $m = $this->__rpmeta(); return $m['vis'] === 3; }"` |
|     - | 2527 | `" public function isStatic(){ $m = $this->__rpmeta(); return $m['static']; }"` |
|     - | 2528 | `" public function isReadOnly(){ $m = $this->__rpmeta(); return $m['readonly']; }"` |
|     - | 2529 | `" public function isPrivateSet(){ $m = $this->__rpmeta(); return isset($m['privset']) ? $m['privset'] : false; }"` |
|     - | 2530 | `" public function isProtectedSet(){ $m = $this->__rpmeta(); return isset($m['protset']) ? $m['protset'] : false; }"` |
|     - | 2531 | `" public function isDefault(){ $m = $this->__rpmeta(); return !isset($m['dyn']); }"` |
|     - | 2532 | `" public function isDynamic(){ $m = $this->__rpmeta(); return isset($m['dyn']); }"` |
|     - | 2533 | `" public function isAbstract(){ return false; }"` |
|     - | 2534 | `" public function isFinal(){ return false; }"` |
|     - | 2535 | `" public function isVirtual(){ return false; }"` |
|     - | 2536 | `" public function isPrivateSet(){ return false; }"` |
|     - | 2537 | `" public function isProtectedSet(){ return false; }"` |
|     - | 2538 | `" public function hasHooks(){ return false; }"` |
|     - | 2539 | `" public function getHooks(){ return array(); }"` |
|     - | 2540 | `" public function hasHook($type){ return false; }"` |
|     - | 2541 | `" public function getHook($type){ return null; }"` |
|     - | 2542 | `" public function isLazy($object){ return false; }"` |
|     - | 2543 | `" public function setAccessible($accessible){ }"` |
|     - | 2544 | `" public function getValue($object = null){"` |
|     - | 2545 | `"  $m = $this->__rpmeta();"` |
|     - | 2546 | `"  if($m['static']){ return __reflect_static_value($this->class, $this->name); }"` |
|     - | 2547 | `"  if(!is_object($object)){"` |
|     - | 2548 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2549 | `"  }"` |
|     - | 2550 | `"  return __reflect_prop_read($object, $this->name);"` |
|     - | 2551 | `" }"` |
|     - | 2552 | `" public function setValue($objectOrValue = null, $value = null){"` |
|     - | 2553 | `"  $m = $this->__rpmeta();"` |
|     - | 2554 | `"  if($m['static']){"` |
|     - | 2555 | `"   if($value === null && $objectOrValue !== null && !is_object($objectOrValue)){"` |
|     - | 2556 | `"    __reflect_static_set($this->class, $this->name, $objectOrValue);"` |
|     - | 2557 | `"   }else{"` |
|     - | 2558 | `"    __reflect_static_set($this->class, $this->name, $value);"` |
|     - | 2559 | `"   }"` |
|     - | 2560 | `"   return;"` |
|     - | 2561 | `"  }"` |
|     - | 2562 | `"  __reflect_prop_write($objectOrValue, $this->name, $value);"` |
|     - | 2563 | `" }"` |
|     - | 2564 | `" public function getRawValue($object){ return $this->getValue($object); }"` |
|     - | 2565 | `" public function setRawValue($object, $value){ $this->setValue($object, $value); }"` |
|     - | 2566 | `" public function isInitialized($object = null){"` |
|     - | 2567 | `"  $m = $this->__rpmeta();"` |
|     - | 2568 | `"  if($m['static']){ return (__reflect_prop_state($this->class, $this->name) & 2) !== 0; }"` |
|     - | 2569 | `"  if(!is_object($object)){"` |
|     - | 2570 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2571 | `"  }"` |
|     - | 2572 | `"  return (__reflect_prop_state($object, $this->name) & 2) !== 0;"` |
|     - | 2573 | `" }"` |
|     - | 2574 | `" public function hasDefaultValue(){"` |
|     - | 2575 | `"  $m = $this->__rpmeta();"` |
|     - | 2576 | `"  if(isset($m['dyn'])){ return false; }"` |
|     - | 2577 | `"  if($m['hasdef']){ return true; }"` |
|     - | 2578 | `"  return !$m['typed'];"` |
|     - | 2579 | `" }"` |
|     - | 2580 | `" public function getDefaultValue(){"` |
|     - | 2581 | `"  $m = $this->__rpmeta();"` |
|     - | 2582 | `"  if(isset($m['dyn']) \|\| !$m['hasdef']){ return null; }"` |
|     - | 2583 | `"  return __reflect_prop_default($this->class, $this->name);"` |
|     - | 2584 | `" }"` |
|     - | 2585 | `" public function hasType(){ $m = $this->__rpmeta(); return $m['typed']; }"` |
|     - | 2586 | `" public function getType(){ $m = $this->__rpmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2587 | `" public function getSettableType(){ return $this->getType(); }"` |
|     - | 2588 | `" public function setRawValueWithoutLazyInitialization($object, $value){"` |
|     - | 2589 | `"  throw new Error('ReflectionProperty::setRawValueWithoutLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2590 | `" }"` |
|     - | 2591 | `" public function skipLazyInitialization($object){"` |
|     - | 2592 | `"  throw new Error('ReflectionProperty::skipLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2593 | `" }"` |
|     - | 2594 | `" public function getDocComment(){ $m = $this->__rpmeta(); return isset($m['doc']) ? $m['doc'] : false; }"` |
|     - | 2595 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2596 | `"  $m = $this->__rpmeta();"` |
|     - | 2597 | `"  if(!isset($m['attrs'])){ return array(); }"` |
|     - | 2598 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 8, $name, $flags);"` |
|     - | 2599 | `" }"` |
|     - | 2600 | `" public function __toString(){ return __reflect_export_prop($this); }"` |
|     - | 2601 | `"}"` |
|     - | 2602 | `"class ReflectionClassConstant implements Reflector {"` |
|     - | 2603 | `" const IS_PUBLIC = 1;"` |
|     - | 2604 | `" const IS_PROTECTED = 2;"` |
|     - | 2605 | `" const IS_PRIVATE = 4;"` |
|     - | 2606 | `" const IS_FINAL = 32;"` |
|     - | 2607 | `" public $name;"` |
|     - | 2608 | `" public $class;"` |
|     - | 2609 | `" public function __construct($class, $constant){"` |
|     - | 2610 | `"  if(!is_object($class) && !is_string($class)){"` |
|     - | 2611 | `"   throw new TypeError('ReflectionClassConstant::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2612 | `"  }"` |
|     - | 2613 | `"  $ci = __reflect_class_info($class);"` |
|     - | 2614 | `"  if($ci === null){"` |
|     - | 2615 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2616 | `"  }"` |
|     - | 2617 | `"  $this->class = $ci['name'];"` |
|     - | 2618 | `"  if(!isset($ci['consts'][$constant])){"` |
|     - | 2619 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' does not exist');"` |
|     - | 2620 | `"  }"` |
|     - | 2621 | `"  $this->name = $constant;"` |
|     - | 2622 | `" }"` |
|     - | 2623 | `" protected function __rcmeta(){"` |
|     - | 2624 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2625 | `"  return $ci['consts'][$this->name];"` |
|     - | 2626 | `" }"` |
|     - | 2627 | `" public function getName(){ return $this->name; }"` |
|     - | 2628 | `" public function getValue(){ return __reflect_const_value($this->class, $this->name); }"` |
|     - | 2629 | `" public function getDeclaringClass(){"` |
|     - | 2630 | `"  $m = $this->__rcmeta();"` |
|     - | 2631 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2632 | `" }"` |
|     - | 2633 | `" public function getModifiers(){"` |
|     - | 2634 | `"  $m = $this->__rcmeta();"` |
|     - | 2635 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2636 | `"  if($m['final']){ $mod \|= 32; }"` |
|     - | 2637 | `"  return $mod;"` |
|     - | 2638 | `" }"` |
|     - | 2639 | `" public function isPublic(){ $m = $this->__rcmeta(); return $m['vis'] === 1; }"` |
|     - | 2640 | `" public function isProtected(){ $m = $this->__rcmeta(); return $m['vis'] === 2; }"` |
|     - | 2641 | `" public function isPrivate(){ $m = $this->__rcmeta(); return $m['vis'] === 3; }"` |
|     - | 2642 | `" public function isFinal(){ $m = $this->__rcmeta(); return $m['final']; }"` |
|     - | 2643 | `" public function isEnumCase(){ $m = $this->__rcmeta(); return $m['enumcase']; }"` |
|     - | 2644 | `" public function isDeprecated(){ $m = $this->__rcmeta(); return __reflect_has_deprecated($m['attrs']); }"` |
|     - | 2645 | `" public function hasType(){ $m = $this->__rcmeta(); return $m['typed']; }"` |
|     - | 2646 | `" public function getType(){ $m = $this->__rcmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2647 | `" public function getDocComment(){ $m = $this->__rcmeta(); return $m['doc']; }"` |
|     - | 2648 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2649 | `"  $m = $this->__rcmeta();"` |
|     - | 2650 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 16, $name, $flags);"` |
|     - | 2651 | `" }"` |
|     - | 2652 | `" public function __toString(){ return __reflect_export_cconst($this); }"` |
|     - | 2653 | `"}"` |
|     - | 2654 | `;` |
|     - | 2655 | `/*` |
|     - | 2656 | ` * Chunk 4: the ReflectionType family, built from the engine's canonical` |
|     - | 2657 | ` * type text ("?int", "string\|float", "(A&B)\|C" — normalized at compile` |
|     - | 2658 | ` * time). __reflect_make_type is the internal factory; PHP itself never` |
|     - | 2659 | ` * lets user code construct these, so the public constructors here are a` |
|     - | 2660 | ` * recorded PHL-only surface.` |
|     - | 2661 | ` */` |
|     - | 2662 | `static const char zReflectLib4[] =` |
|     - | 2663 | `"abstract class ReflectionType implements Stringable {"` |
|     - | 2664 | `" protected $__text = '';"` |
|     - | 2665 | `" protected $__nullable = false;"` |
|     - | 2666 | `" public function allowsNull(){ return $this->__nullable; }"` |
|     - | 2667 | `" public function __toString(){ return $this->__text; }"` |
|     - | 2668 | `"}"` |
|     - | 2669 | `"class ReflectionNamedType extends ReflectionType {"` |
|     - | 2670 | `" protected $__tname = '';"` |
|     - | 2671 | `" public function __construct($name = '', $nullable = false, $text = null){"` |
|     - | 2672 | `"  $this->__tname = $name;"` |
|     - | 2673 | `"  $l = strtolower($name);"` |
|     - | 2674 | `"  $this->__nullable = $nullable \|\| $l === 'null' \|\| $l === 'mixed';"` |
|     - | 2675 | `"  $this->__text = $text === null ? $name : $text;"` |
|     - | 2676 | `" }"` |
|     - | 2677 | `" public function getName(){ return $this->__tname; }"` |
|     - | 2678 | `" public function isBuiltin(){"` |
|     - | 2679 | `"  $l = strtolower($this->__tname);"` |
|     - | 2680 | `"  return in_array($l, array('int','float','string','bool','array','object','mixed',"` |
|     - | 2681 | `"   'void','never','null','callable','iterable','true','false'), true);"` |
|     - | 2682 | `" }"` |
|     - | 2683 | `"}"` |
|     - | 2684 | `"class ReflectionUnionType extends ReflectionType {"` |
|     - | 2685 | `" protected $__types = array();"` |
|     - | 2686 | `" public function __construct($text = '', $nullable = false, $types = array()){"` |
|     - | 2687 | `"  $this->__text = $text;"` |
|     - | 2688 | `"  $this->__nullable = $nullable;"` |
|     - | 2689 | `"  $this->__types = $types;"` |
|     - | 2690 | `" }"` |
|     - | 2691 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2692 | `"}"` |
|     - | 2693 | `"class ReflectionIntersectionType extends ReflectionType {"` |
|     - | 2694 | `" protected $__types = array();"` |
|     - | 2695 | `" public function __construct($text = '', $types = array()){"` |
|     - | 2696 | `"  $this->__text = $text;"` |
|     - | 2697 | `"  $this->__nullable = false;"` |
|     - | 2698 | `"  $this->__types = $types;"` |
|     - | 2699 | `" }"` |
|     - | 2700 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2701 | `"}"` |
|     - | 2702 | `"function __reflect_make_atom($p){"` |
|     - | 2703 | `" $nullable = false;"` |
|     - | 2704 | `" if($p !== '' && $p[0] === '?'){ $nullable = true; $p = substr($p, 1); }"` |
|     - | 2705 | `" if($p !== '' && $p[0] === '('){ $p = substr($p, 1, strlen($p) - 2); }"` |
|     - | 2706 | `" if(strpos($p, '&') !== false){"` |
|     - | 2707 | `"  $subs = array();"` |
|     - | 2708 | `"  foreach(explode('&', $p) as $s){ $subs[] = new ReflectionNamedType($s, false, $s); }"` |
|     - | 2709 | `"  return new ReflectionIntersectionType($p, $subs);"` |
|     - | 2710 | `" }"` |
|     - | 2711 | `" return new ReflectionNamedType($p, $nullable, $nullable ? '?'.$p : $p);"` |
|     - | 2712 | `"}"` |
|     - | 2713 | `"function __reflect_make_type($text){"` |
|     - | 2714 | `" if($text === null \|\| $text === ''){ return null; }"` |
|     - | 2715 | `" $nullable = false;"` |
|     - | 2716 | `" $body = $text;"` |
|     - | 2717 | `" if($body[0] === '?'){ $nullable = true; $body = substr($body, 1); }"` |
|     - | 2718 | `" $parts = array();"` |
|     - | 2719 | `" $depth = 0;"` |
|     - | 2720 | `" $cur = '';"` |
|     - | 2721 | `" $n = strlen($body);"` |
|     - | 2722 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 2723 | `"  $ch = $body[$k];"` |
|     - | 2724 | `"  if($ch === '('){ $depth++; $cur .= $ch; }"` |
|     - | 2725 | `"  else if($ch === ')'){ $depth--; $cur .= $ch; }"` |
|     - | 2726 | `"  else if($ch === '\|' && $depth === 0){ $parts[] = $cur; $cur = ''; }"` |
|     - | 2727 | `"  else{ $cur .= $ch; }"` |
|     - | 2728 | `" }"` |
|     - | 2729 | `" $parts[] = $cur;"` |
|     - | 2730 | `" if(count($parts) > 1){"` |
|     - | 2731 | `"  $nonNull = array();"` |
|     - | 2732 | `"  $hasNull = false;"` |
|     - | 2733 | `"  foreach($parts as $p){"` |
|     - | 2734 | `"   if(strtolower($p) === 'null'){ $hasNull = true; }"` |
|     - | 2735 | `"   else{ $nonNull[] = $p; }"` |
|     - | 2736 | `"  }"` |
|     - | 2737 | `"  if($hasNull && count($nonNull) === 1 && strpos($nonNull[0], '&') === false){"` |
|     - | 2738 | `"   return new ReflectionNamedType($nonNull[0], true, '?'.$nonNull[0]);"` |
|     - | 2739 | `"  }"` |
|     - | 2740 | `"  $types = array();"` |
|     - | 2741 | `"  foreach($parts as $p){ $types[] = __reflect_make_atom($p); }"` |
|     - | 2742 | `"  return new ReflectionUnionType($body, $nullable \|\| $hasNull, $types);"` |
|     - | 2743 | `" }"` |
|     - | 2744 | `" if(strpos($body, '&') !== false){ return __reflect_make_atom($body); }"` |
|     - | 2745 | `" return __reflect_make_atom($nullable ? '?'.$body : $body);"` |
|     - | 2746 | `"}"` |
|     - | 2747 | `;` |
|     - | 2748 | `/*` |
|     - | 2749 | ` * Chunk 5: ReflectionGenerator, ReflectionFiber. Executing line/file and` |
|     - | 2750 | ` * traces need runtime line tracking the VM does not have (same gap as` |
|     - | 2751 | ` * debug_backtrace's line numbers) — those throw a loud Error, recorded in` |
|     - | 2752 | ` * the plan ledger.` |
|     - | 2753 | ` */` |
|     - | 2754 | `static const char zReflectLib5[] =` |
|     - | 2755 | `"class ReflectionGenerator {"` |
|     - | 2756 | `" protected $__gen;"` |
|     - | 2757 | `" public function __construct($generator){"` |
|     - | 2758 | `"  if(!($generator instanceof Generator)){"` |
|     - | 2759 | `"   throw new TypeError('ReflectionGenerator::__construct(): Argument #1 ($generator) must be of type Generator, '.get_debug_type($generator).' given');"` |
|     - | 2760 | `"  }"` |
|     - | 2761 | `"  $this->__gen = $generator;"` |
|     - | 2762 | `" }"` |
|     - | 2763 | `" protected function __rginfo(){ return __reflect_gen_info($this->__gen); }"` |
|     - | 2764 | `" public function getFunction(){"` |
|     - | 2765 | `"  $i = $this->__rginfo();"` |
|     - | 2766 | `"  if($i['kind'] === 'method'){ return new ReflectionMethod($i['class'], $i['name']); }"` |
|     - | 2767 | `"  return new ReflectionFunction($i['name']);"` |
|     - | 2768 | `" }"` |
|     - | 2769 | `" public function getThis(){ $i = $this->__rginfo(); return isset($i['this']) ? $i['this'] : null; }"` |
|     - | 2770 | `" public function getExecutingGenerator(){ return __reflect_gen_exec($this->__gen); }"` |
|     - | 2771 | `" public function isClosed(){ $i = $this->__rginfo(); return $i['closed']; }"` |
|     - | 2772 | `" public function getExecutingLine(){"` |
|     - | 2773 | `"  throw new Error('ReflectionGenerator::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2774 | `" }"` |
|     - | 2775 | `" public function getExecutingFile(){"` |
|     - | 2776 | `"  throw new Error('ReflectionGenerator::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2777 | `" }"` |
|     - | 2778 | `" public function getTrace($options = 1){"` |
|     - | 2779 | `"  throw new Error('ReflectionGenerator::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2780 | `" }"` |
|     - | 2781 | `"}"` |
|     - | 2782 | `"class ReflectionFiber {"` |
|     - | 2783 | `" protected $__fiber;"` |
|     - | 2784 | `" public function __construct($fiber){"` |
|     - | 2785 | `"  if(!($fiber instanceof Fiber)){"` |
|     - | 2786 | `"   throw new TypeError('ReflectionFiber::__construct(): Argument #1 ($fiber) must be of type Fiber, '.get_debug_type($fiber).' given');"` |
|     - | 2787 | `"  }"` |
|     - | 2788 | `"  $this->__fiber = $fiber;"` |
|     - | 2789 | `" }"` |
|     - | 2790 | `" public function getFiber(){ return $this->__fiber; }"` |
|     - | 2791 | `" public function getCallable(){ return __reflect_prop_read($this->__fiber, '__callable'); }"` |
|     - | 2792 | `" public function getExecutingLine(){"` |
|     - | 2793 | `"  throw new Error('ReflectionFiber::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2794 | `" }"` |
|     - | 2795 | `" public function getExecutingFile(){"` |
|     - | 2796 | `"  throw new Error('ReflectionFiber::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2797 | `" }"` |
|     - | 2798 | `" public function getTrace($options = 1){"` |
|     - | 2799 | `"  throw new Error('ReflectionFiber::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2800 | `" }"` |
|     - | 2801 | `"}"` |
|     - | 2802 | `;` |
|     - | 2803 | `/*` |
|     - | 2804 | ` * Chunk 6: the long tail — ReflectionConstant (PHP 8.5), the synthetic` |
|     - | 2805 | ` * "Core" ReflectionExtension, ReflectionZendExtension (throws: no Zend` |
|     - | 2806 | ` * extensions exist), the ReflectionEnum family (throws: enums are not a` |
|     - | 2807 | ` * PHL language feature yet), and ReflectionReference.` |
|     - | 2808 | ` */` |
|     - | 2809 | `static const char zReflectLib6[] =` |
|     - | 2810 | `"class ReflectionConstant implements Reflector {"` |
|     - | 2811 | `" public $name;"` |
|     - | 2812 | `" public function __construct($name){"` |
|     - | 2813 | `"  if(!is_string($name)){"` |
|     - | 2814 | `"   throw new TypeError('ReflectionConstant::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2815 | `"  }"` |
|     - | 2816 | `"  $i = __reflect_const_info($name);"` |
|     - | 2817 | `"  if($i === null){"` |
|     - | 2818 | `"   throw new ReflectionException('Constant \"'.$name.'\" does not exist');"` |
|     - | 2819 | `"  }"` |
|     - | 2820 | `"  $this->name = $name;"` |
|     - | 2821 | `" }"` |
|     - | 2822 | `" public function getName(){ return $this->name; }"` |
|     - | 2823 | `" public function getNamespaceName(){"` |
|     - | 2824 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2825 | `"  if($p === false){ return ''; }"` |
|     - | 2826 | `"  return substr($this->name,0,$p);"` |
|     - | 2827 | `" }"` |
|     - | 2828 | `" public function getShortName(){"` |
|     - | 2829 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2830 | `"  if($p === false){ return $this->name; }"` |
|     - | 2831 | `"  return substr($this->name,$p+1);"` |
|     - | 2832 | `" }"` |
|     - | 2833 | `" public function getValue(){"` |
|     - | 2834 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2835 | `"  return $i['value'];"` |
|     - | 2836 | `" }"` |
|     - | 2837 | `" public function isDeprecated(){ return false; }"` |
|     - | 2838 | `" public function getFileName(){"` |
|     - | 2839 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2840 | `"  return $i['file'];"` |
|     - | 2841 | `" }"` |
|     - | 2842 | `" public function getExtension(){"` |
|     - | 2843 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2844 | `"  return $i['internal'] ? new ReflectionExtension('Core') : null;"` |
|     - | 2845 | `" }"` |
|     - | 2846 | `" public function getExtensionName(){"` |
|     - | 2847 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2848 | `"  return $i['internal'] ? 'Core' : false;"` |
|     - | 2849 | `" }"` |
|     - | 2850 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2851 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2852 | `"  if($i === null){ return array(); }"` |
|     - | 2853 | `"  return __reflect_build_attrs($i['attrs'], array('const', $this->name, null, 0), 64, $name, $flags);"` |
|     - | 2854 | `" }"` |
|     - | 2855 | `" public function __toString(){"` |
|     - | 2856 | `"  return 'Constant [ '.$this->name.' ]'.\"\\n\";"` |
|     - | 2857 | `" }"` |
|     - | 2858 | `"}"` |
|     - | 2859 | `"class ReflectionExtension implements Reflector {"` |
|     - | 2860 | `" public $name;"` |
|     - | 2861 | `" public function __construct($name){"` |
|     - | 2862 | `"  if(!is_string($name)){"` |
|     - | 2863 | `"   throw new TypeError('ReflectionExtension::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2864 | `"  }"` |
|     - | 2865 | `"  if(strtolower($name) !== 'core'){"` |
|     - | 2866 | `"   throw new ReflectionException('Extension \"'.$name.'\" does not exist');"` |
|     - | 2867 | `"  }"` |
|     - | 2868 | `"  $this->name = 'Core';"` |
|     - | 2869 | `" }"` |
|     - | 2870 | `" public function getName(){ return $this->name; }"` |
|     - | 2871 | `" public function getVersion(){ return phpversion(); }"` |
|     - | 2872 | `" public function getFunctions(){ return array(); }"` |
|     - | 2873 | `" public function getClasses(){ return array(); }"` |
|     - | 2874 | `" public function getClassNames(){ return array(); }"` |
|     - | 2875 | `" public function getConstants(){ return array(); }"` |
|     - | 2876 | `" public function getINIEntries(){ return array(); }"` |
|     - | 2877 | `" public function getDependencies(){ return array(); }"` |
|     - | 2878 | `" public function isPersistent(){ return true; }"` |
|     - | 2879 | `" public function isTemporary(){ return false; }"` |
|     - | 2880 | `" public function info(){ }"` |
|     - | 2881 | `" public function __toString(){"` |
|     - | 2882 | `"  return 'Extension [ extension #1 '.$this->name.' ]'.\"\\n\";"` |
|     - | 2883 | `" }"` |
|     - | 2884 | `"}"` |
|     - | 2885 | `"class ReflectionZendExtension implements Reflector {"` |
|     - | 2886 | `" public $name;"` |
|     - | 2887 | `" public function __construct($name){"` |
|     - | 2888 | `"  throw new ReflectionException('Zend Extension \"'.$name.'\" does not exist');"` |
|     - | 2889 | `" }"` |
|     - | 2890 | `" public function getName(){ return $this->name; }"` |
|     - | 2891 | `" public function __toString(){ return ''; }"` |
|     - | 2892 | `"}"` |
|     - | 2893 | `"class ReflectionEnum extends ReflectionClass {"` |
|     - | 2894 | `" public function __construct($objectOrClass){"` |
|     - | 2895 | `"  $info = __reflect_class_info($objectOrClass);"` |
|     - | 2896 | `"  if($info === null){"` |
|     - | 2897 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 2898 | `"  }"` |
|     - | 2899 | `"  if(!$info['enum']){"` |
|     - | 2900 | `"   throw new ReflectionException('Class \"'.$info['name'].'\" is not an enum');"` |
|     - | 2901 | `"  }"` |
|     - | 2902 | `"  parent::__construct($objectOrClass);"` |
|     - | 2903 | `" }"` |
|     - | 2904 | `" public function hasCase($name){"` |
|     - | 2905 | `"  $i = $this->__rinfo();"` |
|     - | 2906 | `"  return in_array($name, $i['cases'], true);"` |
|     - | 2907 | `" }"` |
|     - | 2908 | `" public function getCase($name){"` |
|     - | 2909 | `"  if(!$this->hasCase($name)){"` |
|     - | 2910 | `"   throw new ReflectionException('Case '.$this->name.'::'.$name.' does not exist');"` |
|     - | 2911 | `"  }"` |
|     - | 2912 | `"  if($this->isBacked()){ return new ReflectionEnumBackedCase($this->name, $name); }"` |
|     - | 2913 | `"  return new ReflectionEnumUnitCase($this->name, $name);"` |
|     - | 2914 | `" }"` |
|     - | 2915 | `" public function getCases(){"` |
|     - | 2916 | `"  $i = $this->__rinfo();"` |
|     - | 2917 | `"  $out = array();"` |
|     - | 2918 | `"  foreach($i['cases'] as $c){"` |
|     - | 2919 | `"   $out[] = $this->isBacked()"` |
|     - | 2920 | `"    ? new ReflectionEnumBackedCase($this->name, $c)"` |
|     - | 2921 | `"    : new ReflectionEnumUnitCase($this->name, $c);"` |
|     - | 2922 | `"  }"` |
|     - | 2923 | `"  return $out;"` |
|     - | 2924 | `" }"` |
|     - | 2925 | `" public function isBacked(){ $i = $this->__rinfo(); return $i['enumbacking'] !== ''; }"` |
|     - | 2926 | `" public function getBackingType(){"` |
|     - | 2927 | `"  $i = $this->__rinfo();"` |
|     - | 2928 | `"  if($i['enumbacking'] === ''){ return null; }"` |
|     - | 2929 | `"  return __reflect_make_type($i['enumbacking']);"` |
|     - | 2930 | `" }"` |
|     - | 2931 | `"}"` |
|     - | 2932 | `"class ReflectionEnumUnitCase extends ReflectionClassConstant {"` |
|     - | 2933 | `" public function __construct($class, $constant){"` |
|     - | 2934 | `"  parent::__construct($class, $constant);"` |
|     - | 2935 | `"  $ci = __reflect_class_info($class);"` |
|     - | 2936 | `"  if(!$ci['enum']){"` |
|     - | 2937 | `"   throw new ReflectionException('Class \"'.$this->class.'\" is not an enum');"` |
|     - | 2938 | `"  }"` |
|     - | 2939 | `"  $m = $this->__rcmeta();"` |
|     - | 2940 | `"  if(!$m['enumcase']){"` |
|     - | 2941 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' is not a case');"` |
|     - | 2942 | `"  }"` |
|     - | 2943 | `" }"` |
|     - | 2944 | `" public function getEnum(){ return new ReflectionEnum($this->class); }"` |
|     - | 2945 | `"}"` |
|     - | 2946 | `"class ReflectionEnumBackedCase extends ReflectionEnumUnitCase {"` |
|     - | 2947 | `" public function getBackingValue(){ return $this->getValue()->value; }"` |
|     - | 2948 | `"}"` |
|     - | 2949 | `"final class ReflectionReference {"` |
|     - | 2950 | `" protected $__id = '';"` |
|     - | 2951 | `" public function __construct(){"` |
|     - | 2952 | `"  throw new Error('Call to private ReflectionReference::__construct() from global scope');"` |
|     - | 2953 | `" }"` |
|     - | 2954 | `" public static function fromArrayElement($array, $key){"` |
|     - | 2955 | `"  if(!is_array($array)){"` |
|     - | 2956 | `"   throw new TypeError('ReflectionReference::fromArrayElement(): Argument #1 ($array) must be of type array, '.get_debug_type($array).' given');"` |
|     - | 2957 | `"  }"` |
|     - | 2958 | `"  $id = __reflect_ref_id($array, $key);"` |
|     - | 2959 | `"  if($id === null){ return null; }"` |
|     - | 2960 | `"  $r = __reflect_new_no_ctor('ReflectionReference');"` |
|     - | 2961 | `"  $r->__setId('phlref'.$id);"` |
|     - | 2962 | `"  return $r;"` |
|     - | 2963 | `" }"` |
|     - | 2964 | `" public function __setId($id){ $this->__id = $id; }"` |
|     - | 2965 | `" public function getId(){ return $this->__id; }"` |
|     - | 2966 | `"}"` |
|     - | 2967 | `;` |
|     - | 2968 | `/*` |
|     - | 2969 | ` * Chunk 7: ReflectionAttribute and the shared getAttributes() builder.` |
|     - | 2970 | ` * The spec array rides as [kind, target, member, paramIdx]; argument` |
|     - | 2971 | ` * values evaluate lazily through __reflect_attr_args (PHP semantics).` |
|     - | 2972 | ` */` |
|     - | 2973 | `static const char zReflectLib7[] =` |
|     - | 2974 | `"function __reflect_has_deprecated($meta){"` |
|     - | 2975 | `" foreach($meta as $a){"` |
|     - | 2976 | `"  if(strtolower($a['name']) === 'deprecated'){ return true; }"` |
|     - | 2977 | `" }"` |
|     - | 2978 | `" return false;"` |
|     - | 2979 | `"}"` |
|     - | 2980 | `"function __reflect_target_names($mask){"` |
|     - | 2981 | `" $parts = array();"` |
|     - | 2982 | `" foreach(array('class' => 1, 'function' => 2, 'method' => 4, 'property' => 8,"` |
|     - | 2983 | `"  'class constant' => 16, 'parameter' => 32, 'constant' => 64) as $nm => $bit){"` |
|     - | 2984 | `"  if($mask & $bit){ $parts[] = $nm; }"` |
|     - | 2985 | `" }"` |
|     - | 2986 | `" return implode(', ', $parts);"` |
|     - | 2987 | `"}"` |
|     - | 2988 | `"function __reflect_build_attrs($meta, $spec, $target, $name, $flags){"` |
|     - | 2989 | `" $out = array();"` |
|     - | 2990 | `" $counts = array();"` |
|     - | 2991 | `" foreach($meta as $a){"` |
|     - | 2992 | `"  $k = strtolower($a['name']);"` |
|     - | 2993 | `"  $counts[$k] = isset($counts[$k]) ? $counts[$k] + 1 : 1;"` |
|     - | 2994 | `" }"` |
|     - | 2995 | `" $idx = 0;"` |
|     - | 2996 | `" foreach($meta as $a){"` |
|     - | 2997 | `"  $keep = true;"` |
|     - | 2998 | `"  if($name !== null){"` |
|     - | 2999 | `"   $keep = strtolower($a['name']) === strtolower($name);"` |
|     - | 3000 | `"   if(!$keep && ($flags & 2)){"` |
|     - | 3001 | `"    $keep = is_subclass_of($a['name'], $name);"` |
|     - | 3002 | `"   }"` |
|     - | 3003 | `"  }"` |
|     - | 3004 | `"  if($keep){"` |
|     - | 3005 | `"   $r = __reflect_new_no_ctor('ReflectionAttribute');"` |
|     - | 3006 | `"   $r->__init($a['name'], $spec, $idx, $target, $counts[strtolower($a['name'])] > 1);"` |
|     - | 3007 | `"   $out[] = $r;"` |
|     - | 3008 | `"  }"` |
|     - | 3009 | `"  $idx++;"` |
|     - | 3010 | `" }"` |
|     - | 3011 | `" return $out;"` |
|     - | 3012 | `"}"` |
|     - | 3013 | `"final class ReflectionAttribute {"` |
|     - | 3014 | `" const IS_INSTANCEOF = 2;"` |
|     - | 3015 | `" protected $__name = '';"` |
|     - | 3016 | `" protected $__spec = null;"` |
|     - | 3017 | `" protected $__idx = 0;"` |
|     - | 3018 | `" protected $__target = 0;"` |
|     - | 3019 | `" protected $__rep = false;"` |
|     - | 3020 | `" public function __construct(){"` |
|     - | 3021 | `"  throw new Error('Call to private ReflectionAttribute::__construct() from global scope');"` |
|     - | 3022 | `" }"` |
|     - | 3023 | `" public function __init($name, $spec, $idx, $target, $rep){"` |
|     - | 3024 | `"  $this->__name = $name;"` |
|     - | 3025 | `"  $this->__spec = $spec;"` |
|     - | 3026 | `"  $this->__idx = $idx;"` |
|     - | 3027 | `"  $this->__target = $target;"` |
|     - | 3028 | `"  $this->__rep = $rep;"` |
|     - | 3029 | `" }"` |
|     - | 3030 | `" public function getName(){ return $this->__name; }"` |
|     - | 3031 | `" public function getTarget(){ return $this->__target; }"` |
|     - | 3032 | `" public function isRepeated(){ return $this->__rep; }"` |
|     - | 3033 | `" public function getArguments(){"` |
|     - | 3034 | `"  $a = __reflect_attr_args($this->__spec[0], $this->__spec[1], $this->__spec[2], $this->__spec[3], $this->__idx);"` |
|     - | 3035 | `"  return $a === null ? array() : $a;"` |
|     - | 3036 | `" }"` |
|     - | 3037 | `" public function newInstance(){"` |
|     - | 3038 | `"  $name = $this->__name;"` |
|     - | 3039 | `"  $ci = __reflect_class_info($name);"` |
|     - | 3040 | `"  if($ci === null){"` |
|     - | 3041 | `"   throw new Error('Attribute class \"'.$name.'\" not found');"` |
|     - | 3042 | `"  }"` |
|     - | 3043 | `"  $name = $ci['name'];"` |
|     - | 3044 | `"  $decl = null;"` |
|     - | 3045 | `"  $didx = 0;"` |
|     - | 3046 | `"  foreach($ci['attrs'] as $a){"` |
|     - | 3047 | `"   if(strtolower($a['name']) === 'attribute'){ $decl = $didx; break; }"` |
|     - | 3048 | `"   $didx++;"` |
|     - | 3049 | `"  }"` |
|     - | 3050 | `"  if($decl === null){"` |
|     - | 3051 | `"   throw new Error('Attempting to use non-attribute class \"'.$name.'\" as attribute');"` |
|     - | 3052 | `"  }"` |
|     - | 3053 | `"  $dargs = __reflect_attr_args('class', $name, null, 0, $decl);"` |
|     - | 3054 | `"  $flags = 127;"` |
|     - | 3055 | `"  if(is_array($dargs)){"` |
|     - | 3056 | `"   if(isset($dargs[0])){ $flags = $dargs[0]; }"` |
|     - | 3057 | `"   else if(isset($dargs['flags'])){ $flags = $dargs['flags']; }"` |
|     - | 3058 | `"  }"` |
|     - | 3059 | `"  if(($flags & $this->__target) === 0){"` |
|     - | 3060 | `"   $tnames = array(1 => 'class', 2 => 'function', 4 => 'method', 8 => 'property',"` |
|     - | 3061 | `"    16 => 'class constant', 32 => 'parameter', 64 => 'constant');"` |
|     - | 3062 | `"   throw new Error('Attribute \"'.$name.'\" cannot target '.$tnames[$this->__target]"` |
|     - | 3063 | `"    .' (allowed targets: '.__reflect_target_names($flags).')');"` |
|     - | 3064 | `"  }"` |
|     - | 3065 | `"  if($this->__rep && ($flags & 128) === 0){"` |
|     - | 3066 | `"   throw new Error('Attribute \"'.$name.'\" must not be repeated');"` |
|     - | 3067 | `"  }"` |
|     - | 3068 | `"  return __reflect_new_instance($name, $this->getArguments());"` |
|     - | 3069 | `" }"` |
|     - | 3070 | `" public function __toString(){"` |
|     - | 3071 | `"  return 'Attribute [ '.$this->__name.' ]';"` |
|     - | 3072 | `" }"` |
|     - | 3073 | `"}"` |
|     - | 3074 | `;` |
|     - | 3075 | `/*` |
|     - | 3076 | ` * Chunk 8: signature-table support. Internal (C builtin) functions carry a` |
|     - | 3077 | ` * PHP-style parameter-list string; these helpers parse it into the same` |
|     - | 3078 | ` * param-meta shape user functions get, so ReflectionFunction and` |
|     - | 3079 | ` * ReflectionParameter work uniformly over builtins.` |
|     - | 3080 | ` */` |
|     - | 3081 | `static const char zReflectLib8[] =` |
|     - | 3082 | `"function __reflect_sig_split($sig){"` |
|     - | 3083 | `" $parts = array();"` |
|     - | 3084 | `" $cur = '';"` |
|     - | 3085 | `" $q = false;"` |
|     - | 3086 | `" $n = strlen($sig);"` |
|     - | 3087 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3088 | `"  $ch = $sig[$k];"` |
|     - | 3089 | `"  if($q){"` |
|     - | 3090 | `"   $cur .= $ch;"` |
|     - | 3091 | `"   if($ch === chr(92) && $k + 1 < $n){ $cur .= $sig[$k+1]; $k++; }"` |
|     - | 3092 | `"   else if($ch === chr(39)){ $q = false; }"` |
|     - | 3093 | `"  }else if($ch === chr(39)){ $q = true; $cur .= $ch; }"` |
|     - | 3094 | `"  else if($ch === ',' ){ $parts[] = trim($cur); $cur = ''; }"` |
|     - | 3095 | `"  else{ $cur .= $ch; }"` |
|     - | 3096 | `" }"` |
|     - | 3097 | `" if(trim($cur) !== ''){ $parts[] = trim($cur); }"` |
|     - | 3098 | `" return $parts;"` |
|     - | 3099 | `"}"` |
|     - | 3100 | `"function __reflect_sig_scalar($t){"` |
|     - | 3101 | `" if($t === '?'){ return array(false, null); }"` |
|     - | 3102 | `" if($t === 'NULL' \|\| $t === 'null'){ return array(true, null); }"` |
|     - | 3103 | `" if($t === 'true'){ return array(true, true); }"` |
|     - | 3104 | `" if($t === 'false'){ return array(true, false); }"` |
|     - | 3105 | `" if(is_numeric($t)){"` |
|     - | 3106 | `"  if(strpos($t, '.') === false && stripos($t, 'e') === false && strpos($t, 'x') === false){"` |
|     - | 3107 | `"   return array(true, (int)$t);"` |
|     - | 3108 | `"  }"` |
|     - | 3109 | `"  return array(true, (float)$t);"` |
|     - | 3110 | `" }"` |
|     - | 3111 | `" if(strlen($t) >= 2 && $t[0] === chr(39) && $t[strlen($t)-1] === chr(39)){"` |
|     - | 3112 | `"  $body = substr($t, 1, strlen($t) - 2);"` |
|     - | 3113 | `"  return array(true, strtr($body, array(chr(92).chr(39) => chr(39), chr(92).chr(92) => chr(92))));"` |
|     - | 3114 | `" }"` |
|     - | 3115 | `" return array(false, null);"` |
|     - | 3116 | `"}"` |
|     - | 3117 | `"function __reflect_parse_sig($sig){"` |
|     - | 3118 | `" $params = array();"` |
|     - | 3119 | `" $pos = 0;"` |
|     - | 3120 | `" foreach(__reflect_sig_split($sig) as $part){"` |
|     - | 3121 | `"  $deftext = null;"` |
|     - | 3122 | `"  $q = false;"` |
|     - | 3123 | `"  $n = strlen($part);"` |
|     - | 3124 | `"  for($k = 0; $k < $n; $k++){"` |
|     - | 3125 | `"   $ch = $part[$k];"` |
|     - | 3126 | `"   if($q){"` |
|     - | 3127 | `"    if($ch === chr(92)){ $k++; }"` |
|     - | 3128 | `"    else if($ch === chr(39)){ $q = false; }"` |
|     - | 3129 | `"   }else if($ch === chr(39)){ $q = true; }"` |
|     - | 3130 | `"   else if($ch === '=' ){"` |
|     - | 3131 | `"    $deftext = trim(substr($part, $k + 1));"` |
|     - | 3132 | `"    $part = trim(substr($part, 0, $k));"` |
|     - | 3133 | `"    break;"` |
|     - | 3134 | `"   }"` |
|     - | 3135 | `"  }"` |
|     - | 3136 | `"  $variadic = strpos($part, '...') !== false;"` |
|     - | 3137 | `"  $byref = strpos($part, '&') !== false;"` |
|     - | 3138 | `"  $d = strpos($part, '$');"` |
|     - | 3139 | `"  $name = $d === false ? $part : substr($part, $d + 1);"` |
|     - | 3140 | `"  $typetext = null;"` |
|     - | 3141 | `"  $sp = strpos($part, ' ');"` |
|     - | 3142 | `"  if($sp !== false && $d !== false && $sp < $d){ $typetext = substr($part, 0, $sp); }"` |
|     - | 3143 | `"  $nullable = $typetext !== null && ($typetext[0] === '?' \|\| stripos($typetext, 'null') !== false);"` |
|     - | 3144 | `"  $params[] = array('name' => $name, 'pos' => $pos, 'byref' => $byref,"` |
|     - | 3145 | `"   'variadic' => $variadic, 'hasdef' => $deftext !== null, 'nullable' => $nullable,"` |
|     - | 3146 | `"   'promoted' => false, 'typetext' => $typetext, 'attrs' => array(), 'deftext' => $deftext);"` |
|     - | 3147 | `"  $pos++;"` |
|     - | 3148 | `" }"` |
|     - | 3149 | `" return $params;"` |
|     - | 3150 | `"}"` |
|     - | 3151 | `"function __reflect_sig_fixup($i){"` |
|     - | 3152 | `" if($i === null){ return $i; }"` |
|     - | 3153 | `" if(isset($i['ret2'])){ $i['rettext'] = $i['ret2']; }"` |
|     - | 3154 | `" if(!isset($i['sig']) \|\| $i['sig'] === ''){ return $i; }"` |
|     - | 3155 | `" $i['params'] = __reflect_parse_sig($i['sig']);"` |
|     - | 3156 | `" $i['minarg'] = -1;"` |
|     - | 3157 | `" $v = false;"` |
|     - | 3158 | `" foreach($i['params'] as $p){ if($p['variadic']){ $v = true; } }"` |
|     - | 3159 | `" $i['variadic'] = $v;"` |
|     - | 3160 | `" return $i;"` |
|     - | 3161 | `"}"` |
|     - | 3162 | `;` |
|     - | 3163 | `/*` |
|     - | 3164 | ` * Chunk 9: PHP's Reflection export format (__toString on every Reflector).` |
|     - | 3165 | ` * Built entirely from the public reflection API of the target objects.` |
|     - | 3166 | ` */` |
|     - | 3167 | `static const char zReflectLib9[] =` |
|     - | 3168 | `"function __reflect_export_value($v){"` |
|     - | 3169 | `" if($v === null){ return 'NULL'; }"` |
|     - | 3170 | `" if($v === true){ return 'true'; }"` |
|     - | 3171 | `" if($v === false){ return 'false'; }"` |
|     - | 3172 | `" if(is_string($v)){ return chr(39).$v.chr(39); }"` |
|     - | 3173 | `" if(is_array($v)){"` |
|     - | 3174 | `"  $parts = array();"` |
|     - | 3175 | `"  $isList = true;"` |
|     - | 3176 | `"  $next = 0;"` |
|     - | 3177 | `"  foreach($v as $k => $x){"` |
|     - | 3178 | `"   if($k !== $next){ $isList = false; break; }"` |
|     - | 3179 | `"   $next++;"` |
|     - | 3180 | `"  }"` |
|     - | 3181 | `"  foreach($v as $k => $x){"` |
|     - | 3182 | `"   $parts[] = $isList ? __reflect_export_value($x)"` |
|     - | 3183 | `"    : (__reflect_export_value($k).' => '.__reflect_export_value($x));"` |
|     - | 3184 | `"  }"` |
|     - | 3185 | `"  return '['.implode(', ', $parts).']';"` |
|     - | 3186 | `" }"` |
|     - | 3187 | `" return (string)$v;"` |
|     - | 3188 | `"}"` |
|     - | 3189 | `"function __reflect_export_param($p){"` |
|     - | 3190 | `" $s = 'Parameter #'.$p->getPosition().' [ <'.($p->isOptional() ? 'optional' : 'required').'> ';"` |
|     - | 3191 | `" $t = $p->getType();"` |
|     - | 3192 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3193 | `" if($p->isPassedByReference()){ $s .= '&'; }"` |
|     - | 3194 | `" if($p->isVariadic()){ $s .= '...'; }"` |
|     - | 3195 | `" $s .= '$'.$p->getName();"` |
|     - | 3196 | `" if($p->isDefaultValueAvailable()){"` |
|     - | 3197 | `"  try{ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3198 | `"  catch(ReflectionException $e){ $s .= ' = <default>'; }"` |
|     - | 3199 | `" }"` |
|     - | 3200 | `" return $s.' ]';"` |
|     - | 3201 | `"}"` |
|     - | 3202 | `"function __reflect_export_prop($p){"` |
|     - | 3203 | `" $s = 'Property [ ';"` |
|     - | 3204 | `" $s .= $p->isPrivate() ? 'private ' : ($p->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3205 | `" if($p->isStatic()){ $s .= 'static '; }"` |
|     - | 3206 | `" if($p->isReadOnly()){ $s .= 'readonly '; }"` |
|     - | 3207 | `" $t = $p->getType();"` |
|     - | 3208 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3209 | `" $s .= '$'.$p->getName();"` |
|     - | 3210 | `" if($p->hasDefaultValue()){ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3211 | `" return $s.' ]'.chr(10);"` |
|     - | 3212 | `"}"` |
|     - | 3213 | `"function __reflect_export_cconst($c){"` |
|     - | 3214 | `" $v = $c->getValue();"` |
|     - | 3215 | `" if(is_int($v)){ $t = 'int'; }"` |
|     - | 3216 | `" else if(is_string($v)){ $t = 'string'; }"` |
|     - | 3217 | `" else if(is_float($v)){ $t = 'float'; }"` |
|     - | 3218 | `" else if(is_bool($v)){ $t = 'bool'; }"` |
|     - | 3219 | `" else if(is_array($v)){ $t = 'array'; }"` |
|     - | 3220 | `" else{ $t = 'null'; }"` |
|     - | 3221 | `" $vs = is_array($v) ? 'Array' : (is_bool($v) ? ($v ? '1' : '') : (string)$v);"` |
|     - | 3222 | `" $vis = $c->isPrivate() ? 'private' : ($c->isProtected() ? 'protected' : 'public');"` |
|     - | 3223 | `" return 'Constant [ '.$vis.' '.$t.' '.$c->name.' ] { '.$vs.' }'.chr(10);"` |
|     - | 3224 | `"}"` |
|     - | 3225 | `"function __reflect_export_fnabs($r, $indent){"` |
|     - | 3226 | `" $tags = $r->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3227 | `" if($r instanceof ReflectionMethod){"` |
|     - | 3228 | `"  if($r->isConstructor()){ $tags .= ', ctor'; }"` |
|     - | 3229 | `"  else if($r->isDestructor()){ $tags .= ', dtor'; }"` |
|     - | 3230 | `"  $decl = $r->getDeclaringClass()->name;"` |
|     - | 3231 | `"  if(strtolower($decl) !== strtolower($r->class)){ $tags .= ', inherits '.$decl; }"` |
|     - | 3232 | `"  else if($r->hasPrototype()){ $tags .= ', prototype '.$r->getPrototype()->class; }"` |
|     - | 3233 | `"  $head = 'Method [ <'.$tags.'> ';"` |
|     - | 3234 | `"  if($r->isAbstract()){ $head .= 'abstract '; }"` |
|     - | 3235 | `"  if($r->isFinal()){ $head .= 'final '; }"` |
|     - | 3236 | `"  if($r->isStatic()){ $head .= 'static '; }"` |
|     - | 3237 | `"  $head .= $r->isPrivate() ? 'private ' : ($r->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3238 | `"  $head .= 'method '.$r->name.' ]';"` |
|     - | 3239 | `" }else{"` |
|     - | 3240 | `"  $kind = $r->isClosure() ? 'Closure' : 'Function';"` |
|     - | 3241 | `"  $head = $kind.' [ <'.$tags.'> function '.$r->name.' ]';"` |
|     - | 3242 | `" }"` |
|     - | 3243 | `" $s = $head.' {'.chr(10);"` |
|     - | 3244 | `" if(!$r->isInternal()){"` |
|     - | 3245 | `"  $s .= '  @@ '.$r->getFileName().' '.$r->getStartLine().' - '.$r->getEndLine().chr(10);"` |
|     - | 3246 | `" }"` |
|     - | 3247 | `" $ps = $r->getParameters();"` |
|     - | 3248 | `" $ret = $r->getReturnType();"` |
|     - | 3249 | `" if(count($ps) > 0 \|\| $ret !== null){"` |
|     - | 3250 | `"  $s .= chr(10).'  - Parameters ['.count($ps).'] {'.chr(10);"` |
|     - | 3251 | `"  foreach($ps as $p){ $s .= '    '.__reflect_export_param($p).chr(10); }"` |
|     - | 3252 | `"  $s .= '  }'.chr(10);"` |
|     - | 3253 | `" }"` |
|     - | 3254 | `" if($ret !== null){ $s .= '  - Return [ '.(string)$ret.' ]'.chr(10); }"` |
|     - | 3255 | `" $s .= '}'.chr(10);"` |
|     - | 3256 | `" if($indent === ''){ return $s; }"` |
|     - | 3257 | `" $lines = explode(chr(10), $s);"` |
|     - | 3258 | `" $out = '';"` |
|     - | 3259 | `" $n = count($lines);"` |
|     - | 3260 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3261 | `"  if($lines[$k] === '' && $k === $n - 1){ break; }"` |
|     - | 3262 | `"  $out .= ($lines[$k] === '' ? '' : $indent.$lines[$k]).chr(10);"` |
|     - | 3263 | `" }"` |
|     - | 3264 | `" return $out;"` |
|     - | 3265 | `"}"` |
|     - | 3266 | `"function __reflect_export_class($rc){"` |
|     - | 3267 | `" $tags = $rc->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3268 | `" if($rc->isInterface()){"` |
|     - | 3269 | `"  $head = 'Interface [ <'.$tags.'> interface '.$rc->name.' ]';"` |
|     - | 3270 | `" }else{"` |
|     - | 3271 | `"  $mods = '';"` |
|     - | 3272 | `"  if($rc->isAbstract()){ $mods .= 'abstract '; }"` |
|     - | 3273 | `"  if($rc->isFinal()){ $mods .= 'final '; }"` |
|     - | 3274 | `"  $head = 'Class [ <'.$tags.'> '.$mods.'class '.$rc->name;"` |
|     - | 3275 | `"  $par = $rc->getParentClass();"` |
|     - | 3276 | `"  if($par !== false){ $head .= ' extends '.$par->name; }"` |
|     - | 3277 | `"  $ifs = $rc->getInterfaceNames();"` |
|     - | 3278 | `"  if(count($ifs) > 0){ $head .= ' implements '.implode(', ', $ifs); }"` |
|     - | 3279 | `"  $head .= ' ]';"` |
|     - | 3280 | `" }"` |
|     - | 3281 | `" $s = $head.' {'.chr(10);"` |
|     - | 3282 | `" if(!$rc->isInternal()){"` |
|     - | 3283 | `"  $s .= '  @@ '.$rc->getFileName().' '.$rc->getStartLine().'-'.$rc->getEndLine().chr(10);"` |
|     - | 3284 | `" }"` |
|     - | 3285 | `" $consts = $rc->getReflectionConstants();"` |
|     - | 3286 | `" $s .= chr(10).'  - Constants ['.count($consts).'] {'.chr(10);"` |
|     - | 3287 | `" foreach($consts as $c){ $s .= '    '.__reflect_export_cconst($c); }"` |
|     - | 3288 | `" $s .= '  }'.chr(10);"` |
|     - | 3289 | `" $sp = array();"` |
|     - | 3290 | `" $ip = array();"` |
|     - | 3291 | `" foreach($rc->getProperties() as $p){"` |
|     - | 3292 | `"  if($p->isStatic()){ $sp[] = $p; }else{ $ip[] = $p; }"` |
|     - | 3293 | `" }"` |
|     - | 3294 | `" $sm = array();"` |
|     - | 3295 | `" $im = array();"` |
|     - | 3296 | `" foreach($rc->getMethods() as $m){"` |
|     - | 3297 | `"  if($m->isStatic()){ $sm[] = $m; }else{ $im[] = $m; }"` |
|     - | 3298 | `" }"` |
|     - | 3299 | `" $s .= chr(10).'  - Static properties ['.count($sp).'] {'.chr(10);"` |
|     - | 3300 | `" foreach($sp as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3301 | `" $s .= '  }'.chr(10);"` |
|     - | 3302 | `" $s .= chr(10).'  - Static methods ['.count($sm).'] {'.chr(10);"` |
|     - | 3303 | `" $first = true;"` |
|     - | 3304 | `" foreach($sm as $m){"` |
|     - | 3305 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3306 | `"  $first = false;"` |
|     - | 3307 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3308 | `" }"` |
|     - | 3309 | `" $s .= '  }'.chr(10);"` |
|     - | 3310 | `" $s .= chr(10).'  - Properties ['.count($ip).'] {'.chr(10);"` |
|     - | 3311 | `" foreach($ip as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3312 | `" $s .= '  }'.chr(10);"` |
|     - | 3313 | `" $s .= chr(10).'  - Methods ['.count($im).'] {'.chr(10);"` |
|     - | 3314 | `" $first = true;"` |
|     - | 3315 | `" foreach($im as $m){"` |
|     - | 3316 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3317 | `"  $first = false;"` |
|     - | 3318 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3319 | `" }"` |
|     - | 3320 | `" $s .= '  }'.chr(10);"` |
|     - | 3321 | `" return $s.'}'.chr(10);"` |
|     - | 3322 | `"}"` |
|     - | 3323 | `;` |
|     - | 3324 | `/*` |
|     - | 3325 | ` * Register the __reflect_* thunks and compile the Reflection library.` |
|     - | 3326 | ` * Called from PH7_VmInit while pVm->bCompilingBuiltin is set, right after` |
|     - | 3327 | ` * the core builtin chunks (Exception and friends must exist already).` |
|     - | 3328 | ` */` |
|  3884 | 3329 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|     5 | 3330 | `{` |
|     - | 3331 | `	static const struct {` |
|     - | 3332 | `		const char *zName;` |
|     - | 3333 | `		ProchHostFunction xFunc;` |
|     - | 3334 | `	} aFunc[] = {` |
|     - | 3335 | `		{ "__reflect_class_info",     vm_builtin_reflect_class_info },` |
|     - | 3336 | `		{ "__reflect_const_value",    vm_builtin_reflect_const_value },` |
|     - | 3337 | `		{ "__reflect_static_value",   vm_builtin_reflect_static_value },` |
|     - | 3338 | `		{ "__reflect_static_set",     vm_builtin_reflect_static_set },` |
|     - | 3339 | `		{ "__reflect_prop_default",   vm_builtin_reflect_prop_default },` |
|     - | 3340 | `		{ "__reflect_new_instance",   vm_builtin_reflect_new_instance },` |
|     - | 3341 | `		{ "__reflect_new_no_ctor",    vm_builtin_reflect_new_no_ctor },` |
|     - | 3342 | `		{ "__reflect_func_info",      vm_builtin_reflect_func_info },` |
|     - | 3343 | `		{ "__reflect_param_default",  vm_builtin_reflect_param_default },` |
|     - | 3344 | `		{ "__reflect_param_defconst", vm_builtin_reflect_param_defconst },` |
|     - | 3345 | `		{ "__reflect_invoke",         vm_builtin_reflect_invoke },` |
|     - | 3346 | `		{ "__reflect_closure",        vm_builtin_reflect_closure },` |
|     - | 3347 | `		{ "__reflect_prop_read",      vm_builtin_reflect_prop_read },` |
|     - | 3348 | `		{ "__reflect_prop_write",     vm_builtin_reflect_prop_write },` |
|     - | 3349 | `		{ "__reflect_prop_state",     vm_builtin_reflect_prop_state },` |
|     - | 3350 | `		{ "__reflect_dyn_props",      vm_builtin_reflect_dyn_props },` |
|     - | 3351 | `		{ "__reflect_gen_info",       vm_builtin_reflect_gen_info },` |
|     - | 3352 | `		{ "__reflect_gen_exec",       vm_builtin_reflect_gen_exec },` |
|     - | 3353 | `		{ "__reflect_const_info",     vm_builtin_reflect_const_info },` |
|     - | 3354 | `		{ "__reflect_ref_id",         vm_builtin_reflect_ref_id },` |
|     - | 3355 | `		{ "__reflect_attr_args",      vm_builtin_reflect_attr_args },` |
|     - | 3356 | `	};` |
|     - | 3357 | `	sxu32 n;` |
|     - | 3358 | `	sxi32 rc;` |
| 85453 | 3359 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 81569 | 3360 | `		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);` |
| 40787 | 3361 | `	}` |
|  3889 | 3362 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib1, sizeof(zReflectLib1)-1);` |
|  3889 | 3363 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3364 | `		return rc;` |
|     - | 3365 | `	}` |
|  3889 | 3366 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib2, sizeof(zReflectLib2)-1);` |
|  3889 | 3367 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3368 | `		return rc;` |
|     - | 3369 | `	}` |
|  3889 | 3370 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib3, sizeof(zReflectLib3)-1);` |
|  3889 | 3371 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3372 | `		return rc;` |
|     - | 3373 | `	}` |
|  3889 | 3374 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib4, sizeof(zReflectLib4)-1);` |
|  3889 | 3375 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3376 | `		return rc;` |
|     - | 3377 | `	}` |
|  3889 | 3378 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib5, sizeof(zReflectLib5)-1);` |
|  3889 | 3379 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3380 | `		return rc;` |
|     - | 3381 | `	}` |
|  3889 | 3382 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib6, sizeof(zReflectLib6)-1);` |
|  3889 | 3383 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3384 | `		return rc;` |
|     - | 3385 | `	}` |
|  3889 | 3386 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib7, sizeof(zReflectLib7)-1);` |
|  3889 | 3387 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3388 | `		return rc;` |
|     - | 3389 | `	}` |
|  3889 | 3390 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib8, sizeof(zReflectLib8)-1);` |
|  3889 | 3391 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3392 | `		return rc;` |
|     - | 3393 | `	}` |
|  3889 | 3394 | `	return PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib9, sizeof(zReflectLib9)-1);` |
|  1947 | 3395 | `}` |
|     - | 3396 |  |
