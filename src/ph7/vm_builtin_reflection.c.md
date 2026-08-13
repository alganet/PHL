# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1032/1206 lines (85.57%)

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
|  1414 |   31 | `static ph7_class * ReflectResolveClass(ph7_vm *pVm, ph7_value *pArg)` |
|     1 |   32 | `{` |
|     - |   33 | `	ph7_class *pClass;` |
|  1415 |   34 | `	pClass = PH7_VmExtractClassFromValue(pVm, pArg);` |
|  1415 |   35 | `	if( pClass == 0 && ph7_value_is_string(pArg) ){` |
|     - |   36 | `		const char *zName;` |
|     - |   37 | `		int nLen;` |
|    13 |   38 | `		zName = ph7_value_to_string(pArg, &nLen);` |
|    13 |   39 | `		if( nLen > 0 ){` |
|    13 |   40 | `			pClass = PH7_VmTriggerAutoload(pVm, zName, (sxu32)nLen, FALSE);` |
|     6 |   41 | `		}` |
|     6 |   42 | `	}` |
|  1415 |   43 | `	return pClass;` |
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
| 41618 |   63 | `static void ReflectMapAddBool(ph7_context *pCtx, ph7_value *pMap, const char *zKey, int b)` |
|     1 |   64 | `{` |
| 41619 |   65 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 41619 |   66 | `	if( p == 0 ){ return; }` |
| 41619 |   67 | `	ph7_value_bool(p, b);` |
| 41619 |   68 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
| 20810 |   69 | `}` |
| 17554 |   70 | `static void ReflectMapAddInt(ph7_context *pCtx, ph7_value *pMap, const char *zKey, sxi64 iVal)` |
|     1 |   71 | `{` |
| 17555 |   72 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 17555 |   73 | `	if( p == 0 ){ return; }` |
| 17555 |   74 | `	ph7_value_int64(p, iVal);` |
| 17555 |   75 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  8778 |   76 | `}` |
| 12784 |   77 | `static void ReflectMapAddStr(ph7_context *pCtx, ph7_value *pMap, const char *zKey,` |
|     - |   78 | `	const char *zVal, int nVal)` |
|     1 |   79 | `{` |
| 12785 |   80 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 12785 |   81 | `	if( p == 0 ){ return; }` |
| 12785 |   82 | `	ph7_value_string(p, zVal, nVal);` |
| 12785 |   83 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  6393 |   84 | `}` |
|  4332 |   85 | `static void ReflectMapAddNull(ph7_context *pCtx, ph7_value *pMap, const char *zKey)` |
|     1 |   86 | `{` |
|  4333 |   87 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  4333 |   88 | `	if( p == 0 ){ return; }` |
|  4333 |   89 | `	ph7_value_null(p);` |
|  4333 |   90 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  2167 |   91 | `}` |
|     - |   92 | `/* Add an entry under a dynamic (SyString) key. */` |
|  5050 |   93 | `static void ReflectMapAddDyn(ph7_context *pCtx, ph7_value *pMap,` |
|     - |   94 | `	const SyString *pKey, ph7_value *pVal)` |
|     1 |   95 | `{` |
|  5051 |   96 | `	ph7_value *pK = ph7_context_new_scalar(pCtx);` |
|  5051 |   97 | `	if( pK == 0 ){ return; }` |
|  5051 |   98 | `	ph7_value_string(pK, pKey->zString, (int)pKey->nByte);` |
|  5051 |   99 | `	ph7_array_add_elem(pMap, pK, pVal);` |
|  2526 |  100 | `}` |
|     - |  101 | `/* Emit the declared #[...] attributes of a target as a summary list:` |
|     - |  102 | ` * [ {name, line} ... ]. Argument values stay lazy — the PHP layer pulls` |
|     - |  103 | ` * them through __reflect_attr_args when ReflectionAttribute needs them. */` |
|  5734 |  104 | `static void ReflectMapAddAttrs(ph7_context *pCtx, ph7_value *pMap, SySet *pAttrs)` |
|     1 |  105 | `{` |
|  5735 |  106 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|  5735 |  107 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|     - |  108 | `	sxu32 n;` |
|  5735 |  109 | `	if( pList == 0 ){` |
|   ! 0 |  110 | `		return;` |
|     - |  111 | `	}` |
|  6023 |  112 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|   289 |  113 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|   289 |  114 | `		if( pMeta == 0 ){ break; }` |
|   289 |  115 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aA[n].sName), (int)SyStringLength(&aA[n].sName));` |
|   289 |  116 | `		ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)aA[n].nLine);` |
|   289 |  117 | `		ph7_array_add_elem(pList, 0, pMeta);` |
|   145 |  118 | `	}` |
|  5735 |  119 | `	ph7_array_add_strkey_elem(pMap, "attrs", pList);` |
|  2868 |  120 | `}` |
|     - |  121 | `/* Emit a doc-comment field: the text when present, else boolean false` |
|     - |  122 | ` * (getDocComment()'s exact return contract). */` |
|  4444 |  123 | `static void ReflectMapAddDoc(ph7_context *pCtx, ph7_value *pMap, const SyString *pDoc)` |
|     1 |  124 | `{` |
|  4445 |  125 | `	if( SyStringLength(pDoc) > 0 ){` |
|    63 |  126 | `		ReflectMapAddStr(pCtx, pMap, "doc", SyStringData(pDoc), (int)SyStringLength(pDoc));` |
|    32 |  127 | `	}else{` |
|  4383 |  128 | `		ReflectMapAddBool(pCtx, pMap, "doc", 0);` |
|     - |  129 | `	}` |
|  4445 |  130 | `}` |
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
|  1202 |  164 | `static void ReflectCollectInterfaces(ph7_class *pClass, SySet *pOut, int iDepth)` |
|     1 |  165 | `{` |
|     - |  166 | `	ph7_class **apIface;` |
|     - |  167 | `	sxu32 n;` |
|  1203 |  168 | `	if( pClass == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|   ! 0 |  169 | `		return;` |
|     - |  170 | `	}` |
|  1203 |  171 | `	if( pClass->pBase ){` |
|   283 |  172 | `		ReflectCollectInterfaces(pClass->pBase, pOut, iDepth + 1);` |
|   141 |  173 | `	}` |
|  1203 |  174 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  1669 |  175 | `	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){` |
|   467 |  176 | `		ReflectAddInterface(apIface[n], pOut, iDepth + 1);` |
|   234 |  177 | `	}` |
|   602 |  178 | `}` |
|     - |  179 | `/*` |
|     - |  180 | ` * Deepest base class whose method table maps the same name to the very` |
|     - |  181 | ` * same ph7_class_method pointer: inheritance shares member pointers` |
|     - |  182 | ` * (PH7_ClassInherit), so this identifies the declaring class. Methods` |
|     - |  183 | ` * copied in from traits are not on the pBase chain and thus report the` |
|     - |  184 | ` * using class, which is what PHP reports too.` |
|     - |  185 | ` */` |
|  4680 |  186 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth)` |
|     1 |  187 | `{` |
|  4681 |  188 | `	ph7_class *pDecl = pClass;` |
|  4681 |  189 | `	ph7_class *pBase = pClass->pBase;` |
|  4681 |  190 | `	int iDepth = 0;` |
|  5761 |  191 | `	while( pBase && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
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
|  4681 |  202 | `	return pDecl;` |
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
|   932 |  233 | `static int vm_builtin_reflect_class_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  234 | `{` |
|   933 |  235 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  236 | `	ph7_class *pClass;` |
|     - |  237 | `	ph7_value *pInfo, *pConsts, *pProps, *pMethods, *pList;` |
|     - |  238 | `	SyHashEntry *pEntry;` |
|     - |  239 | `	SySet aIfaceSet;` |
|   933 |  240 | `	sxi32 iCtorVis = 0, iCloneVis = 0;` |
|   933 |  241 | `	int bIterable = 0;` |
|     - |  242 | `	sxu32 n;` |
|   933 |  243 | `	if( nArg < 1 ){` |
|   ! 0 |  244 | `		ph7_result_null(pCtx);` |
|   ! 0 |  245 | `		return PH7_OK;` |
|     - |  246 | `	}` |
|   933 |  247 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|   933 |  248 | `	if( pClass == 0 ){` |
|    13 |  249 | `		ph7_result_null(pCtx);` |
|    13 |  250 | `		return PH7_OK;` |
|     - |  251 | `	}` |
|   921 |  252 | `	pInfo = ph7_context_new_array(pCtx);` |
|   921 |  253 | `	pConsts = ph7_context_new_array(pCtx);` |
|   921 |  254 | `	pProps = ph7_context_new_array(pCtx);` |
|   921 |  255 | `	pMethods = ph7_context_new_array(pCtx);` |
|   921 |  256 | `	if( pInfo == 0 \|\| pConsts == 0 \|\| pProps == 0 \|\| pMethods == 0 ){` |
|   ! 0 |  257 | `		ph7_result_null(pCtx);` |
|   ! 0 |  258 | `		return PH7_OK;` |
|     - |  259 | `	}` |
|   921 |  260 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   921 |  261 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pClass->iFlags & PH7_CLASS_INTERNAL) != 0);` |
|   921 |  262 | `	ReflectMapAddBool(pCtx, pInfo, "interface", (pClass->iFlags & PH7_CLASS_INTERFACE) != 0);` |
|   921 |  263 | `	ReflectMapAddBool(pCtx, pInfo, "trait", (pClass->iFlags & PH7_CLASS_TRAIT) != 0);` |
|   921 |  264 | `	ReflectMapAddBool(pCtx, pInfo, "abstract", (pClass->iFlags & PH7_CLASS_ABSTRACT) != 0);` |
|   921 |  265 | `	ReflectMapAddBool(pCtx, pInfo, "final", (pClass->iFlags & PH7_CLASS_FINAL) != 0);` |
|   921 |  266 | `	ReflectMapAddBool(pCtx, pInfo, "readonly", (pClass->iFlags & PH7_CLASS_READONLY) != 0);` |
|   921 |  267 | `	ReflectMapAddBool(pCtx, pInfo, "enum", (pClass->iFlags & PH7_CLASS_ENUM) != 0);` |
|   921 |  268 | `	if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|   ! 0 |  269 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "int", (int)sizeof("int")-1);` |
|   921 |  270 | `	}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|    67 |  271 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "string", (int)sizeof("string")-1);` |
|    34 |  272 | `	}else{` |
|   855 |  273 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "", 0);` |
|     - |  274 | `	}` |
|     - |  275 | `	{` |
|     - |  276 | `		/* Enum case names in declaration order (empty list for non-enums) */` |
|   921 |  277 | `		ph7_value *pCases = ph7_context_new_array(pCtx);` |
|   921 |  278 | `		if( pCases ){` |
|   921 |  279 | `			ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     - |  280 | `			sxu32 nCase;` |
|  1081 |  281 | `			for( nCase = 0 ; nCase < SySetUsed(&pClass->aEnumCases) ; nCase++ ){` |
|   161 |  282 | `				ph7_value *pNm = ph7_context_new_scalar(pCtx);` |
|   161 |  283 | `				if( pNm ){` |
|   161 |  284 | `					ph7_value_string(pNm,apCase[nCase]->sName.zString,(int)apCase[nCase]->sName.nByte);` |
|   161 |  285 | `					ph7_array_add_elem(pCases,0,pNm);` |
|    80 |  286 | `				}` |
|    81 |  287 | `			}` |
|   921 |  288 | `			ph7_array_add_strkey_elem(pInfo,"cases",pCases);` |
|   460 |  289 | `		}` |
|     - |  290 | `	}` |
|   921 |  291 | `	if( pClass->pBase ){` |
|   418 |  292 | `		ReflectMapAddStr(pCtx, pInfo, "parent", SyStringData(&pClass->pBase->sName),` |
|   278 |  293 | `			(int)SyStringLength(&pClass->pBase->sName));` |
|   140 |  294 | `	}else{` |
|   643 |  295 | `		ReflectMapAddNull(pCtx, pInfo, "parent");` |
|     - |  296 | `	}` |
|     - |  297 | `	/* Transitive interfaces */` |
|   921 |  298 | `	SySetInit(&aIfaceSet, &pVm->sAllocator, sizeof(ph7_class *));` |
|   921 |  299 | `	ReflectCollectInterfaces(pClass, &aIfaceSet, 0);` |
|   921 |  300 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     - |  301 | `		/* An interface's own parents count as its interface list */` |
|    47 |  302 | `		if( pClass->pBase ){` |
|     9 |  303 | `			ReflectAddInterface(pClass->pBase, &aIfaceSet, 0);` |
|     4 |  304 | `		}` |
|    23 |  305 | `	}` |
|   921 |  306 | `	pList = ph7_context_new_array(pCtx);` |
|   921 |  307 | `	if( pList ){` |
|   921 |  308 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIfaceSet);` |
|  1427 |  309 | `		for( n = 0 ; n < SySetUsed(&aIfaceSet) ; n++ ){` |
|   507 |  310 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|   507 |  311 | `			if( pName == 0 ){ break; }` |
|   507 |  312 | `			ph7_value_string(pName, SyStringData(&apIface[n]->sName), (int)SyStringLength(&apIface[n]->sName));` |
|   507 |  313 | `			ph7_array_add_elem(pList, 0, pName);` |
|   507 |  314 | `			if( pVm->pTraversableClass && apIface[n] == pVm->pTraversableClass ){` |
|    13 |  315 | `				bIterable = 1;` |
|     6 |  316 | `			}` |
|   254 |  317 | `		}` |
|   921 |  318 | `		ph7_array_add_strkey_elem(pInfo, "interfaces", pList);` |
|   460 |  319 | `	}` |
|   921 |  320 | `	SySetRelease(&aIfaceSet);` |
|   921 |  321 | `	ReflectMapAddBool(pCtx, pInfo, "iterable", bIterable);` |
|     - |  322 | `	/* Used traits */` |
|   921 |  323 | `	pList = ph7_context_new_array(pCtx);` |
|   921 |  324 | `	if( pList ){` |
|   921 |  325 | `		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|   933 |  326 | `		for( n = 0 ; n < SySetUsed(&pClass->aTrait) ; n++ ){` |
|    13 |  327 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|    13 |  328 | `			if( pName == 0 ){ break; }` |
|    13 |  329 | `			ph7_value_string(pName, SyStringData(&apTrait[n]->sName), (int)SyStringLength(&apTrait[n]->sName));` |
|    13 |  330 | `			ph7_array_add_elem(pList, 0, pName);` |
|     7 |  331 | `		}` |
|   921 |  332 | `		ph7_array_add_strkey_elem(pInfo, "traits", pList);` |
|   460 |  333 | `	}` |
|     - |  334 | `	/* File / lines: no file recorded => false, like PHP internals */` |
|   921 |  335 | `	if( SyStringLength(&pClass->sFile) > 0 ){` |
|   839 |  336 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));` |
|   420 |  337 | `	}else{` |
|    83 |  338 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - |  339 | `	}` |
|   921 |  340 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pClass->nLine);` |
|   921 |  341 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pClass->nEndLine);` |
|   921 |  342 | `	ReflectMapAddDoc(pCtx, pInfo, &pClass->sDoc);` |
|   921 |  343 | `	ReflectMapAddAttrs(pCtx, pInfo, &pClass->aAttrs);` |
|     - |  344 | `	/* Members are emitted in PHP's reporting order: the class's own members` |
|     - |  345 | `	 * first (declaration order), then each inheritance level's, outward.` |
|     - |  346 | `	 * Per level we iterate the DECLARING class's own hash — subclass hashes` |
|     - |  347 | `	 * interleave inherited pointers unpredictably — and emit buffered` |
|     - |  348 | `	 * entries in reverse, because SyHash lists are LIFO. A pointer-identity` |
|     - |  349 | `	 * lookup in the reflected class's hash filters out members that are not` |
|     - |  350 | `	 * visible there (base privates, overridden entries). */` |
|     - |  351 | `	{` |
|     - |  352 | `		ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];` |
|   921 |  353 | `		ph7_class *pWalk = pClass;` |
|     - |  354 | `		SySet aTmp;` |
|   921 |  355 | `		sxu32 nChain = 0, iLevel, nT;` |
|  2123 |  356 | `		while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){` |
|  1203 |  357 | `			aChain[nChain++] = pWalk;` |
|  1203 |  358 | `			pWalk = pWalk->pBase;` |
|     1 |  359 | `		}` |
|   921 |  360 | `		SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));` |
|  2123 |  361 | `		for( iLevel = 0 ; iLevel < nChain ; iLevel++ ){` |
|  1203 |  362 | `			ph7_class *pLevel = aChain[iLevel];` |
|     - |  363 | `			/* --- Constants and properties (shared attribute table) --- */` |
|  1203 |  364 | `			SySetReset(&aTmp);` |
|  1203 |  365 | `			SyHashResetLoopCursor(&pLevel->hAttr);` |
|  4301 |  366 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hAttr)) != 0 ){` |
|  3099 |  367 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  3099 |  368 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|  3099 |  369 | `				if( iLevel == 0 ){` |
|     - |  370 | `					sxu32 j;` |
|     - |  371 | `					/* Own = declared here or by an off-chain provider (trait) */` |
|  4121 |  372 | `					for( j = 1 ; j < nChain ; j++ ){` |
|  1287 |  373 | `						if( aChain[j] == pDecl ){ break; }` |
|   584 |  374 | `					}` |
|  2955 |  375 | `					if( j < nChain ){ continue; }` |
|  1418 |  376 | `				}else{` |
|     - |  377 | `					SyHashEntry *pSub;` |
|   145 |  378 | `					if( pDecl != pLevel ){ continue; }` |
|     - |  379 | `					/* Must still be the visible member in the reflected class */` |
|   121 |  380 | `					pSub = SyHashGet(&pClass->hAttr, pEntry->pKey, pEntry->nKeyLen);` |
|   121 |  381 | `					if( pSub == 0 \|\| pSub->pUserData != (void *)pAttr ){ continue; }` |
|     - |  382 | `				}` |
|  2955 |  383 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     1 |  384 | `			}` |
|  4157 |  385 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|  2955 |  386 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|  2955 |  387 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;` |
|  2955 |  388 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|  2955 |  389 | `				ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|  2955 |  390 | `				if( pMeta == 0 ){ break; }` |
|  2955 |  391 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pAttr->iProtection);` |
|  2955 |  392 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|  2955 |  393 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pAttr->nLine);` |
|  2955 |  394 | `				ReflectMapAddDoc(pCtx, pMeta, &pAttr->sDoc);` |
|  2955 |  395 | `				ReflectMapAddAttrs(pCtx, pMeta, &pAttr->aAttrs);` |
|  2955 |  396 | `				ReflectMapAddBool(pCtx, pMeta, "typed", (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);` |
|  2955 |  397 | `				if( SyStringLength(&pAttr->sTypeName) > 0 ){` |
|   928 |  398 | `					ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&pAttr->sTypeName),` |
|   618 |  399 | `						(int)SyStringLength(&pAttr->sTypeName));` |
|   310 |  400 | `				}else{` |
|  2337 |  401 | `					ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - |  402 | `				}` |
|  2955 |  403 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|   883 |  404 | `					ReflectMapAddBool(pCtx, pMeta, "final", (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   883 |  405 | `					ReflectMapAddBool(pCtx, pMeta, "enumcase", (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0);` |
|   883 |  406 | `					ReflectMapAddDyn(pCtx, pConsts, &pAttr->sName, pMeta);` |
|   442 |  407 | `				}else{` |
|  2073 |  408 | `					ReflectMapAddBool(pCtx, pMeta, "static", (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|  2073 |  409 | `					ReflectMapAddBool(pCtx, pMeta, "readonly", (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0);` |
|  2073 |  410 | `					ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&pAttr->aByteCode) > 0);` |
|  2073 |  411 | `					ReflectMapAddDyn(pCtx, pProps, &pAttr->sName, pMeta);` |
|     - |  412 | `				}` |
|  1478 |  413 | `			}` |
|     - |  414 | `			/* --- Methods. The reported name is the hash-entry key: trait` |
|     - |  415 | `			 * aliasing installs a shallow copy under the alias name while` |
|     - |  416 | `			 * sFunc.sName keeps the original, and PHP reports the alias. --- */` |
|  1203 |  417 | `			SySetReset(&aTmp);` |
|  1203 |  418 | `			SyHashResetLoopCursor(&pLevel->hMethod);` |
|  3579 |  419 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){` |
|  2377 |  420 | `				ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  2377 |  421 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|  2377 |  422 | `				if( iLevel == 0 ){` |
|     - |  423 | `					sxu32 j;` |
|  2769 |  424 | `					for( j = 1 ; j < nChain ; j++ ){` |
|  1011 |  425 | `						if( aChain[j] == pDecl ){ break; }` |
|   372 |  426 | `					}` |
|  2027 |  427 | `					if( j < nChain ){ continue; }` |
|   880 |  428 | `				}else{` |
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
|  2047 |  444 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     1 |  445 | `			}` |
|  3249 |  446 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|  2047 |  447 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|  2047 |  448 | `				ph7_class_method *pMeth = (ph7_class_method *)pE->pUserData;` |
|  2047 |  449 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|     - |  450 | `				ph7_value *pMeta;` |
|     - |  451 | `				SyString sKey;` |
|     - |  452 | `				int bIsAlias;` |
|  2047 |  453 | `				SyStringInitFromBuf(&sKey, (const char *)pE->pKey, pE->nKeyLen);` |
|  4093 |  454 | `				bIsAlias = (sKey.nByte != SyStringLength(&pMeth->sFunc.sName)` |
|  2046 |  455 | `				 \|\| SyMemcmp(sKey.zString, SyStringData(&pMeth->sFunc.sName), sKey.nByte) != 0);` |
|  2046 |  456 | `				if( sKey.nByte == sizeof("__construct")-1` |
|  1230 |  457 | `				 && SyMemcmp(sKey.zString, "__construct", sKey.nByte) == 0 ){` |
|   383 |  458 | `					if( iCtorVis == 0 ){` |
|   383 |  459 | `						iCtorVis = pMeth->iProtection;` |
|   191 |  460 | `					}` |
|   383 |  461 | `					if( bIsAlias ){` |
|     - |  462 | `						/* Mount-time alias for a legacy class-name constructor:` |
|     - |  463 | `						 * the method is already listed under its declared name. */` |
|   ! 0 |  464 | `						continue;` |
|     - |  465 | `					}` |
|  1856 |  466 | `				}else if( sKey.nByte == sizeof("__clone")-1` |
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
|  2047 |  477 | `				pMeta = ph7_context_new_array(pCtx);` |
|  2047 |  478 | `				if( pMeta == 0 ){ break; }` |
|  2047 |  479 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pMeth->iProtection);` |
|  2047 |  480 | `				ReflectMapAddBool(pCtx, pMeta, "static", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|  2047 |  481 | `				ReflectMapAddBool(pCtx, pMeta, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|  2047 |  482 | `				ReflectMapAddBool(pCtx, pMeta, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|  2047 |  483 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|  2047 |  484 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pMeth->nLine);` |
|  2047 |  485 | `				ReflectMapAddDyn(pCtx, pMethods, &sKey, pMeta);` |
|  1024 |  486 | `			}` |
|   602 |  487 | `		}` |
|   921 |  488 | `		SySetRelease(&aTmp);` |
|     - |  489 | `	}` |
|   921 |  490 | `	ReflectMapAddInt(pCtx, pInfo, "ctorvis", (sxi64)iCtorVis);` |
|   921 |  491 | `	ReflectMapAddInt(pCtx, pInfo, "clonevis", (sxi64)iCloneVis);` |
|   921 |  492 | `	ph7_array_add_strkey_elem(pInfo, "consts", pConsts);` |
|   921 |  493 | `	ph7_array_add_strkey_elem(pInfo, "props", pProps);` |
|   921 |  494 | `	ph7_array_add_strkey_elem(pInfo, "methods", pMethods);` |
|   921 |  495 | `	ph7_result_value(pCtx, pInfo);` |
|   921 |  496 | `	return PH7_OK;` |
|   467 |  497 | `}` |
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
|    38 |  623 | `static sxi32 ReflectCollectArgs(ph7_context *pCtx, ph7_value *pArray, SySet *pOut, SyString **ppNames)` |
|     1 |  624 | `{` |
|     - |  625 | `	ph7_hashmap *pMap;` |
|     - |  626 | `	ph7_hashmap_node *pEntry;` |
|    39 |  627 | `	SyString *aNames = 0;` |
|    39 |  628 | `	sxu32 nSlot = 0;` |
|     - |  629 | `	sxu32 n;` |
|    39 |  630 | `	if( ppNames ){` |
|    19 |  631 | `		*ppNames = 0;` |
|     9 |  632 | `	}` |
|    39 |  633 | `	if( !ph7_value_is_array(pArray) ){` |
|   ! 0 |  634 | `		return SXRET_OK;` |
|     - |  635 | `	}` |
|    39 |  636 | `	pMap = (ph7_hashmap *)pArray->x.pOther;` |
|    39 |  637 | `	pEntry = pMap->pFirst;` |
|    91 |  638 | `	for( n = 0 ; n < pMap->nEntry ; n++ ){` |
|    53 |  639 | `		ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);` |
|    53 |  640 | `		if( pValue ){` |
|    53 |  641 | `			if( ppNames && pEntry->iType == HASHMAP_BLOB_NODE ){` |
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
|    53 |  654 | `			SySetPut(pOut, (const void *)&pValue);` |
|    53 |  655 | `			nSlot++;` |
|    26 |  656 | `		}` |
|    53 |  657 | `		pEntry = pEntry->pPrev; /* Reverse link: insertion order */` |
|    27 |  658 | `	}` |
|    39 |  659 | `	if( ppNames ){` |
|    19 |  660 | `		*ppNames = aNames;` |
|     9 |  661 | `	}` |
|    39 |  662 | `	return SXRET_OK;` |
|    20 |  663 | `}` |
|     - |  664 | `/*` |
|     - |  665 | ` * object __reflect_new_instance(string $class, array $args)` |
|     - |  666 | ` * Instantiate and run the constructor with positional arguments.` |
|     - |  667 | ` * The PHP layer has already validated instantiability and ctor visibility.` |
|     - |  668 | ` */` |
|    22 |  669 | `static int vm_builtin_reflect_new_instance(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  670 | `{` |
|    23 |  671 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  672 | `	ph7_class *pClass;` |
|     - |  673 | `	ph7_class_instance *pThis;` |
|     - |  674 | `	ph7_class_method *pCons;` |
|    23 |  675 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pVm, apArg[0])) == 0 ){` |
|   ! 0 |  676 | `		ph7_result_null(pCtx);` |
|   ! 0 |  677 | `		return PH7_OK;` |
|     - |  678 | `	}` |
|    23 |  679 | `	pThis = PH7_NewClassInstance(pVm, pClass);` |
|    23 |  680 | `	if( pThis == 0 ){` |
|   ! 0 |  681 | `		ph7_result_null(pCtx);` |
|   ! 0 |  682 | `		return PH7_OK;` |
|     - |  683 | `	}` |
|    23 |  684 | `	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);` |
|    23 |  685 | `	if( pCons ){` |
|     - |  686 | `		SySet aArg;` |
|     - |  687 | `		sxi32 rc;` |
|    19 |  688 | `		SyString *aNames = 0;` |
|    19 |  689 | `		SySetInit(&aArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    19 |  690 | `		if( nArg > 1 ){` |
|    19 |  691 | `			ReflectCollectArgs(pCtx, apArg[1], &aArg, &aNames);` |
|     9 |  692 | `		}` |
|    19 |  693 | `		if( aNames ){` |
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
|    25 |  704 | `			rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),` |
|    16 |  705 | `				(ph7_value **)SySetBasePtr(&aArg));` |
|     - |  706 | `		}` |
|    19 |  707 | `		SySetRelease(&aArg);` |
|    19 |  708 | `		if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 |  709 | `			PH7_ClassInstanceUnref(pThis);` |
|   ! 0 |  710 | `			return rc;` |
|     - |  711 | `		}` |
|     9 |  712 | `	}` |
|    23 |  713 | `	return ReflectResultObject(pCtx, pThis);` |
|    12 |  714 | `}` |
|     - |  715 | `/*` |
|     - |  716 | ` * object __reflect_new_no_ctor(string $class)` |
|     - |  717 | ` * Instantiate without running the constructor (property defaults still` |
|     - |  718 | ` * apply — PH7_NewClassInstance builds the attribute frame).` |
|     - |  719 | ` */` |
|    60 |  720 | `static int vm_builtin_reflect_new_no_ctor(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 |  721 | `{` |
|     - |  722 | `	ph7_class *pClass;` |
|    61 |  723 | `	if( nArg < 1 \|\| (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0 ){` |
|   ! 0 |  724 | `		ph7_result_null(pCtx);` |
|   ! 0 |  725 | `		return PH7_OK;` |
|     - |  726 | `	}` |
|    61 |  727 | `	return ReflectResultObject(pCtx, PH7_NewClassInstance(pCtx->pVm, pClass));` |
|    31 |  728 | `}` |
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
|   442 |  931 | `static ph7_class_instance * ReflectValueClosure(ph7_vm *pVm, ph7_value *pVal)` |
|     1 |  932 | `{` |
|     - |  933 | `	ph7_class_instance *pThis;` |
|   443 |  934 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVal->x.pOther == 0 \|\| pVm->pClosureClass == 0 ){` |
|   391 |  935 | `		return 0;` |
|     - |  936 | `	}` |
|    53 |  937 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    53 |  938 | `	return (pThis->pClass == pVm->pClosureClass) ? pThis : 0;` |
|   222 |  939 | `}` |
|     - |  940 | `/*` |
|     - |  941 | ` * Resolve a reflection callable target into its compiled function.` |
|     - |  942 | ` *   - pMethodArg a non-empty string  -> method mode: pTarget is a class name` |
|     - |  943 | ` *     or object; outputs *ppClass and *ppMeth.` |
|     - |  944 | ` *   - pTarget a Closure              -> unwrap $__fn into hFunction; *ppClosure.` |
|     - |  945 | ` *   - pTarget a string               -> hFunction (user) or hHostFunction` |
|     - |  946 | ` *     (*ppHost set, returns NULL).` |
|     - |  947 | ` * Returns the ph7_vm_func, or NULL (host function or unresolvable).` |
|     - |  948 | ` */` |
|   722 |  949 | `static ph7_vm_func * ReflectResolveCallable(ph7_context *pCtx, ph7_value *pTarget,` |
|     - |  950 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|     - |  951 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|     1 |  952 | `{` |
|   723 |  953 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  954 | `	SyHashEntry *pEntry;` |
|   723 |  955 | `	if( ppClass ){ *ppClass = 0; }` |
|   723 |  956 | `	if( ppMeth ){ *ppMeth = 0; }` |
|   723 |  957 | `	if( ppHost ){ *ppHost = 0; }` |
|   723 |  958 | `	if( ppClosure ){ *ppClosure = 0; }` |
|   723 |  959 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
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
|   443 |  975 | `		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|   443 |  976 | `		if( pClo ){` |
|     - |  977 | `			SyString sAttr;` |
|     - |  978 | `			ph7_value *pFn;` |
|    53 |  979 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    53 |  980 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|    53 |  981 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) < 1 ){` |
|   ! 0 |  982 | `				return 0;` |
|     - |  983 | `			}` |
|    53 |  984 | `			pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    53 |  985 | `			if( pEntry == 0 ){` |
|     - |  986 | `				/* A Closure over a host function (Closure::fromCallable('strlen')) */` |
|   ! 0 |  987 | `				pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|   ! 0 |  988 | `				if( pEntry && ppHost ){` |
|   ! 0 |  989 | `					*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|   ! 0 |  990 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|   ! 0 |  991 | `				}` |
|   ! 0 |  992 | `				return 0;` |
|     - |  993 | `			}` |
|    53 |  994 | `			if( ppClosure ){ *ppClosure = pClo; }` |
|    53 |  995 | `			return (ph7_vm_func *)pEntry->pUserData;` |
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
|   362 | 1012 | `}` |
|     - | 1013 | `/* Emit the shared descriptor fields of a compiled function. */` |
|   570 | 1014 | `static void ReflectFillFuncCommon(ph7_context *pCtx, ph7_value *pInfo, ph7_vm_func *pFunc)` |
|     1 | 1015 | `{` |
|     - | 1016 | `	ph7_vm_func_arg *aArg;` |
|     - | 1017 | `	ph7_value *pParams, *pStatics;` |
|   571 | 1018 | `	int bVariadic = 0;` |
|     - | 1019 | `	int bAnon;` |
|     - | 1020 | `	sxu32 n;` |
|     - | 1021 | ``	/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|     - | 1022 | `	 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|   571 | 1023 | `	bAnon = (pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|   570 | 1024 | `	if( !bAnon && SyStringLength(&pFunc->sName) > 9` |
|   306 | 1025 | `	 && (SyMemcmp(SyStringData(&pFunc->sName), "[lambda_", 8) == 0` |
|    82 | 1026 | `	  \|\| SyMemcmp(SyStringData(&pFunc->sName), "[closure_", 9) == 0) ){` |
|     5 | 1027 | `		bAnon = 1;` |
|     2 | 1028 | `	}` |
|   571 | 1029 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|   571 | 1030 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pFunc->iFlags & VM_FUNC_INTERNAL) != 0);` |
|   571 | 1031 | `	ReflectMapAddBool(pCtx, pInfo, "closure", bAnon);` |
|   571 | 1032 | `	ReflectMapAddBool(pCtx, pInfo, "fstatic", (pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);` |
|   571 | 1033 | `	ReflectMapAddBool(pCtx, pInfo, "byref", (pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|   571 | 1034 | `	ReflectMapAddBool(pCtx, pInfo, "generator", (pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|   571 | 1035 | `	ReflectMapAddBool(pCtx, pInfo, "strict", pFunc->bStrictTypes != 0);` |
|   571 | 1036 | `	if( SyStringLength(&pFunc->sFile) > 0 ){` |
|   567 | 1037 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pFunc->sFile), (int)SyStringLength(&pFunc->sFile));` |
|   284 | 1038 | `	}else{` |
|     5 | 1039 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1040 | `	}` |
|   571 | 1041 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pFunc->nLine);` |
|   571 | 1042 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pFunc->nEndLine);` |
|   571 | 1043 | `	ReflectMapAddDoc(pCtx, pInfo, &pFunc->sDoc);` |
|   571 | 1044 | `	ReflectMapAddAttrs(pCtx, pInfo, &pFunc->aAttrs);` |
|   571 | 1045 | `	if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|   145 | 1046 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", SyStringData(&pFunc->sReturnTypeName),` |
|    96 | 1047 | `			(int)SyStringLength(&pFunc->sReturnTypeName));` |
|   523 | 1048 | `	}else if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     - | 1049 | `		/* The type-text renderer omits void/never atoms (compile.c notes the` |
|     - | 1050 | `		 * root fix belongs there); name them here for getReturnType(). */` |
|     3 | 1051 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "void", sizeof("void")-1);` |
|   474 | 1052 | `	}else if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|     3 | 1053 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "never", sizeof("never")-1);` |
|     2 | 1054 | `	}else{` |
|   471 | 1055 | `		ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1056 | `	}` |
|   571 | 1057 | `	ReflectMapAddBool(pCtx, pInfo, "retnullable", (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) != 0);` |
|     - | 1058 | `	/* Parameters */` |
|   571 | 1059 | `	pParams = ph7_context_new_array(pCtx);` |
|   571 | 1060 | `	aArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|  1823 | 1061 | `	for( n = 0 ; pParams && n < SySetUsed(&pFunc->aArgs) ; n++ ){` |
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
|   571 | 1085 | `	if( pParams ){` |
|   571 | 1086 | `		ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|   285 | 1087 | `	}` |
|   571 | 1088 | `	ReflectMapAddBool(pCtx, pInfo, "variadic", bVariadic);` |
|     - | 1089 | `	/* Static variables: current value when the slot was initialized (first` |
|     - | 1090 | `	 * call), otherwise the evaluated default — PHP's getStaticVariables` |
|     - | 1091 | `	 * initializes on demand and reports the same values. */` |
|   571 | 1092 | `	pStatics = ph7_context_new_array(pCtx);` |
|   571 | 1093 | `	if( pStatics ){` |
|   571 | 1094 | `		ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|   599 | 1095 | `		for( n = 0 ; n < SySetUsed(&pFunc->aStatic) ; n++ ){` |
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
|   571 | 1115 | `		ph7_array_add_strkey_elem(pInfo, "statics", pStatics);` |
|   285 | 1116 | `	}` |
|   571 | 1117 | `}` |
|     - | 1118 | `/*` |
|     - | 1119 | ` * array\|null __reflect_func_info(string\|Closure $target [, string $method])` |
|     - | 1120 | ` * Function/method/closure descriptor for the PHP layer.` |
|     - | 1121 | ` */` |
|   676 | 1122 | `static int vm_builtin_reflect_func_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1123 | `{` |
|     - | 1124 | `	ph7_vm_func *pFunc;` |
|   677 | 1125 | `	ph7_class *pClass = 0;` |
|   677 | 1126 | `	ph7_class_method *pMeth = 0;` |
|   677 | 1127 | `	ph7_user_func *pHost = 0;` |
|   677 | 1128 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1129 | `	ph7_value *pInfo;` |
|   677 | 1130 | `	if( nArg < 1 ){` |
|   ! 0 | 1131 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1132 | `		return PH7_OK;` |
|     - | 1133 | `	}` |
|   677 | 1134 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], nArg > 1 ? apArg[1] : 0,` |
|     - | 1135 | `		&pClass, &pMeth, &pHost, &pClosure);` |
|   677 | 1136 | `	if( pFunc == 0 && pHost == 0 ){` |
|     3 | 1137 | `		ph7_result_null(pCtx);` |
|     3 | 1138 | `		return PH7_OK;` |
|     - | 1139 | `	}` |
|   675 | 1140 | `	pInfo = ph7_context_new_array(pCtx);` |
|   675 | 1141 | `	if( pInfo == 0 ){` |
|   ! 0 | 1142 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1143 | `		return PH7_OK;` |
|     - | 1144 | `	}` |
|   675 | 1145 | `	if( pFunc == 0 ){` |
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
|   571 | 1184 | `	ReflectFillFuncCommon(pCtx, pInfo, pFunc);` |
|   571 | 1185 | `	ReflectMapAddInt(pCtx, pInfo, "minarg", -1);` |
|   571 | 1186 | `	if( (pFunc->iFlags & VM_FUNC_INTERNAL) && SySetUsed(&pFunc->aArgs) == 0 && pMeth == 0 ){` |
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
|   571 | 1198 | `	if( pMeth && pClass ){` |
|   259 | 1199 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|   259 | 1200 | `		ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   259 | 1201 | `		ReflectMapAddStr(pCtx, pInfo, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   259 | 1202 | `		ReflectMapAddInt(pCtx, pInfo, "vis", (sxi64)pMeth->iProtection);` |
|   259 | 1203 | `		ReflectMapAddBool(pCtx, pInfo, "mstatic", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   259 | 1204 | `		ReflectMapAddBool(pCtx, pInfo, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|   259 | 1205 | `		ReflectMapAddBool(pCtx, pInfo, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   129 | 1206 | `	}` |
|   571 | 1207 | `	if( pClosure ){` |
|     - | 1208 | `		SyString sAttr;` |
|     - | 1209 | `		ph7_value *pAttr;` |
|     - | 1210 | `		ph7_value *pUsed;` |
|    49 | 1211 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|    49 | 1212 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    49 | 1213 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|   ! 0 | 1214 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 1215 | `			if( pKey ){` |
|   ! 0 | 1216 | `				ph7_value_string(pKey, "this", 4);` |
|   ! 0 | 1217 | `				ph7_array_add_elem(pInfo, pKey, pAttr);` |
|   ! 0 | 1218 | `			}` |
|   ! 0 | 1219 | `		}else{` |
|    49 | 1220 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|     - | 1221 | `		}` |
|    49 | 1222 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|    49 | 1223 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    49 | 1224 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|   ! 0 | 1225 | `			ReflectMapAddStr(pCtx, pInfo, "scope", (const char *)SyBlobData(&pAttr->sBlob),` |
|   ! 0 | 1226 | `				(int)SyBlobLength(&pAttr->sBlob));` |
|   ! 0 | 1227 | `		}else{` |
|    49 | 1228 | `			ReflectMapAddNull(pCtx, pInfo, "scope");` |
|     - | 1229 | `		}` |
|     - | 1230 | `		/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */` |
|    49 | 1231 | `		pUsed = ph7_context_new_array(pCtx);` |
|    49 | 1232 | `		if( pUsed ){` |
|    49 | 1233 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     - | 1234 | `			sxu32 n;` |
|   107 | 1235 | `			for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; n++ ){` |
|    59 | 1236 | `				if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){` |
|    41 | 1237 | `					continue;` |
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
|    49 | 1251 | `			ph7_array_add_strkey_elem(pInfo, "used", pUsed);` |
|    24 | 1252 | `		}` |
|    24 | 1253 | `	}` |
|   571 | 1254 | `	ph7_result_value(pCtx, pInfo);` |
|   571 | 1255 | `	return PH7_OK;` |
|   339 | 1256 | `}` |
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
|    40 | 1547 | `static int vm_builtin_reflect_const_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1548 | `{` |
|    41 | 1549 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1550 | `	SyHashEntry *pEntry;` |
|     - | 1551 | `	ph7_constant *pCons;` |
|     - | 1552 | `	ph7_value *pInfo;` |
|     - | 1553 | `	ph7_value sValue;` |
|     - | 1554 | `	const char *zName;` |
|     - | 1555 | `	int nLen;` |
|    41 | 1556 | `	if( nArg < 1 ){` |
|   ! 0 | 1557 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1558 | `		return PH7_OK;` |
|     - | 1559 | `	}` |
|    41 | 1560 | `	zName = ph7_value_to_string(apArg[0], &nLen);` |
|    41 | 1561 | `	pEntry = nLen > 0 ? SyHashGet(&pVm->hConstant, (const void *)zName, (sxu32)nLen) : 0;` |
|    41 | 1562 | `	if( pEntry == 0 ){` |
|     3 | 1563 | `		ph7_result_null(pCtx);` |
|     3 | 1564 | `		return PH7_OK;` |
|     - | 1565 | `	}` |
|    39 | 1566 | `	pCons = (ph7_constant *)pEntry->pUserData;` |
|    39 | 1567 | `	pInfo = ph7_context_new_array(pCtx);` |
|    39 | 1568 | `	if( pInfo == 0 ){` |
|   ! 0 | 1569 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1570 | `		return PH7_OK;` |
|     - | 1571 | `	}` |
|    39 | 1572 | `	PH7_MemObjInit(pVm, &sValue);` |
|    39 | 1573 | `	if( pCons->xExpand ){` |
|    39 | 1574 | `		pCons->xExpand(&sValue, pCons->pUserData);` |
|    19 | 1575 | `	}` |
|     - | 1576 | `	{` |
|    39 | 1577 | `		ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|    39 | 1578 | `		if( pKey ){` |
|    39 | 1579 | `			ph7_value_string(pKey, "value", 5);` |
|    39 | 1580 | `			ph7_array_add_elem(pInfo, pKey, &sValue);` |
|    19 | 1581 | `		}` |
|     - | 1582 | `	}` |
|    39 | 1583 | `	PH7_MemObjRelease(&sValue);` |
|    39 | 1584 | `	ReflectMapAddBool(pCtx, pInfo, "internal", pCons->bUserDefined == 0);` |
|    39 | 1585 | `	if( SyStringLength(&pCons->sFile) > 0 ){` |
|    25 | 1586 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));` |
|    13 | 1587 | `	}else{` |
|    15 | 1588 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1589 | `	}` |
|    39 | 1590 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pCons->nLine);` |
|    39 | 1591 | `	ReflectMapAddAttrs(pCtx, pInfo, &pCons->aAttrs);` |
|    39 | 1592 | `	ph7_result_value(pCtx, pInfo);` |
|    39 | 1593 | `	return PH7_OK;` |
|    21 | 1594 | `}` |
|     - | 1595 | `/*` |
|     - | 1596 | ` * int\|null __reflect_ref_id(array $arr, int\|string $key)` |
|     - | 1597 | ` * The element's slot index when the element is a reference (its slot has` |
|     - | 1598 | ` * a reference-table record with at least two links), null otherwise.` |
|     - | 1599 | ` */` |
|     6 | 1600 | `static int vm_builtin_reflect_ref_id(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1601 | `{` |
|     - | 1602 | `	ph7_hashmap *pMap;` |
|     7 | 1603 | `	ph7_hashmap_node *pNode = 0;` |
|     7 | 1604 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|   ! 0 | 1605 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1606 | `		return PH7_OK;` |
|     - | 1607 | `	}` |
|     7 | 1608 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 | 1609 | `	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK \|\| pNode == 0 ){` |
|   ! 0 | 1610 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1611 | `		return PH7_OK;` |
|     - | 1612 | `	}` |
|     7 | 1613 | `	if( PH7_VmSlotRefCount(pCtx->pVm, pNode->nValIdx) < 2 ){` |
|     3 | 1614 | `		ph7_result_null(pCtx);` |
|     3 | 1615 | `		return PH7_OK;` |
|     - | 1616 | `	}` |
|     5 | 1617 | `	ph7_result_int64(pCtx, (sxi64)pNode->nValIdx);` |
|     5 | 1618 | `	return PH7_OK;` |
|     4 | 1619 | `}` |
|     - | 1620 | `/*` |
|     - | 1621 | ` * array\|null __reflect_attr_args(string $kind, mixed $target, ?string $member,` |
|     - | 1622 | ` *                                int $paramIdx, int $attrIdx)` |
|     - | 1623 | ` * Evaluate the recorded argument expressions of one declared attribute:` |
|     - | 1624 | ` * kind 'class' (target = class), 'attr' (class + property/constant name),` |
|     - | 1625 | ` * 'method' (class + method), 'fn' (function name or Closure), 'param'` |
|     - | 1626 | ` * (function spec + parameter index). Named arguments become string keys.` |
|     - | 1627 | ` */` |
|    52 | 1628 | `static int vm_builtin_reflect_attr_args(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1629 | `{` |
|    53 | 1630 | `	ph7_vm *pVm = pCtx->pVm;` |
|    53 | 1631 | `	SySet *pAttrs = 0;` |
|     - | 1632 | `	ph7_attribute *pAttrRec;` |
|     - | 1633 | `	ph7_value *pOut;` |
|     - | 1634 | `	const char *zKind;` |
|     - | 1635 | `	int nKind;` |
|     - | 1636 | `	sxu32 nAttrIdx, n;` |
|    53 | 1637 | `	if( nArg < 5 ){` |
|   ! 0 | 1638 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1639 | `		return PH7_OK;` |
|     - | 1640 | `	}` |
|    53 | 1641 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|    53 | 1642 | `	nAttrIdx = (sxu32)ph7_value_to_int(apArg[4]);` |
|    70 | 1643 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|    35 | 1644 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|    35 | 1645 | `		if( pClass ){ pAttrs = &pClass->aAttrs; }` |
|    38 | 1646 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|     5 | 1647 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|     5 | 1648 | `		ph7_class_attr *pMember = pClass ? ReflectFetchAttr(pClass, apArg[2]) : 0;` |
|     5 | 1649 | `		if( pMember ){ pAttrs = &pMember->aAttrs; }` |
|    18 | 1650 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|     3 | 1651 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1652 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|    18 | 1653 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|     9 | 1654 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], 0, 0, 0, 0, 0);` |
|     9 | 1655 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|    10 | 1656 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|     3 | 1657 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1658 | `		ph7_vm_func_arg *pParam = pFunc` |
|     2 | 1659 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|     3 | 1660 | `		if( pParam ){ pAttrs = &pParam->aAttrs; }` |
|     4 | 1661 | `	}else if( nKind == 5 && SyMemcmp(zKind, "const", 5) == 0 ){` |
|     - | 1662 | ``		/* Global constant (php 8.5 attributes on `const` statements) */`` |
|     - | 1663 | `		const char *zCName;` |
|     - | 1664 | `		int nCName;` |
|     - | 1665 | `		SyHashEntry *pCEntry;` |
|     3 | 1666 | `		zCName = ph7_value_to_string(apArg[1], &nCName);` |
|     3 | 1667 | `		pCEntry = nCName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zCName, (sxu32)nCName) : 0;` |
|     3 | 1668 | `		if( pCEntry ){ pAttrs = &((ph7_constant *)pCEntry->pUserData)->aAttrs; }` |
|     1 | 1669 | `	}` |
|    52 | 1670 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|    53 | 1671 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 | 1672 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1673 | `		return PH7_OK;` |
|     - | 1674 | `	}` |
|    99 | 1675 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|    47 | 1676 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|     - | 1677 | `		ph7_value sValue;` |
|    47 | 1678 | `		PH7_MemObjInit(pVm, &sValue);` |
|    47 | 1679 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|    47 | 1680 | `			VmLocalExec(pVm, &pArgRec->aByteCode, &sValue, FALSE);` |
|    23 | 1681 | `		}` |
|    47 | 1682 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|     5 | 1683 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|     3 | 1684 | `		}else{` |
|    43 | 1685 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|     - | 1686 | `		}` |
|    47 | 1687 | `		PH7_MemObjRelease(&sValue);` |
|    24 | 1688 | `	}` |
|    53 | 1689 | `	ph7_result_value(pCtx, pOut);` |
|    53 | 1690 | `	return PH7_OK;` |
|    27 | 1691 | `}` |
|     - | 1692 | `/*` |
|     - | 1693 | ` * The Reflection classes, in PHP. Chunk 1: exceptions, Reflector,` |
|     - | 1694 | ` * Reflection, ReflectionClass, ReflectionObject (plus get_debug_type,` |
|     - | 1695 | ` * which the TypeError messages need and PHP 8.0 ships natively).` |
|     - | 1696 | ` */` |
|     - | 1697 | `static const char zReflectLib1[] =` |
|     - | 1698 | `"function get_debug_type($value){"` |
|     - | 1699 | `" if(is_object($value)){ return get_class($value); }"` |
|     - | 1700 | `" if(is_bool($value)){ return 'bool'; }"` |
|     - | 1701 | `" if(is_int($value)){ return 'int'; }"` |
|     - | 1702 | `" if(is_float($value)){ return 'float'; }"` |
|     - | 1703 | `" if(is_string($value)){ return 'string'; }"` |
|     - | 1704 | `" if(is_array($value)){ return 'array'; }"` |
|     - | 1705 | `" if($value === null){ return 'null'; }"` |
|     - | 1706 | `" return gettype($value);"` |
|     - | 1707 | `"}"` |
|     - | 1708 | `"interface Reflector extends Stringable {}"` |
|     - | 1709 | `"class ReflectionException extends Exception {}"` |
|     - | 1710 | `"class Reflection {"` |
|     - | 1711 | `" public static function getModifierNames($modifiers){"` |
|     - | 1712 | `"  $names = array();"` |
|     - | 1713 | `"  if($modifiers & 64){ $names[] = 'abstract'; }"` |
|     - | 1714 | `"  if($modifiers & 32){ $names[] = 'final'; }"` |
|     - | 1715 | `"  if($modifiers & 1){ $names[] = 'public'; }"` |
|     - | 1716 | `"  if($modifiers & 2){ $names[] = 'protected'; }"` |
|     - | 1717 | `"  if($modifiers & 4){ $names[] = 'private'; }"` |
|     - | 1718 | `"  if($modifiers & 16){ $names[] = 'static'; }"` |
|     - | 1719 | `"  if($modifiers & 128){ $names[] = 'readonly'; }"` |
|     - | 1720 | `"  return $names;"` |
|     - | 1721 | `" }"` |
|     - | 1722 | `"}"` |
|     - | 1723 | `"class ReflectionClass implements Reflector {"` |
|     - | 1724 | `" const IS_IMPLICIT_ABSTRACT = 16;"` |
|     - | 1725 | `" const IS_EXPLICIT_ABSTRACT = 64;"` |
|     - | 1726 | `" const IS_FINAL = 32;"` |
|     - | 1727 | `" const IS_READONLY = 65536;"` |
|     - | 1728 | `" const SKIP_INITIALIZATION_ON_SERIALIZE = 8;"` |
|     - | 1729 | `" const SKIP_DESTRUCTOR = 16;"` |
|     - | 1730 | `" public $name;"` |
|     - | 1731 | `" protected $__obj = null;"` |
|     - | 1732 | `" public function __construct($objectOrClass){"` |
|     - | 1733 | `"  if(!is_object($objectOrClass) && !is_string($objectOrClass)){"` |
|     - | 1734 | `"   if(is_int($objectOrClass) \|\| is_float($objectOrClass) \|\| is_bool($objectOrClass)){"` |
|     - | 1735 | `"    $objectOrClass = (string)$objectOrClass;"` |
|     - | 1736 | `"   }else{"` |
|     - | 1737 | `"    throw new TypeError('ReflectionClass::__construct(): Argument #1 ($objectOrClass) must be of type object\|string, '.get_debug_type($objectOrClass).' given');"` |
|     - | 1738 | `"   }"` |
|     - | 1739 | `"  }"` |
|     - | 1740 | `"  $info = __reflect_class_info($objectOrClass);"` |
|     - | 1741 | `"  if($info === null){"` |
|     - | 1742 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 1743 | `"  }"` |
|     - | 1744 | `"  $this->name = $info['name'];"` |
|     - | 1745 | `" }"` |
|     - | 1746 | `" protected function __rinfo(){ return __reflect_class_info($this->name); }"` |
|     - | 1747 | `" public function getName(){ return $this->name; }"` |
|     - | 1748 | `" public function getShortName(){"` |
|     - | 1749 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1750 | `"  if($p === false){ return $this->name; }"` |
|     - | 1751 | `"  return substr($this->name,$p+1);"` |
|     - | 1752 | `" }"` |
|     - | 1753 | `" public function getNamespaceName(){"` |
|     - | 1754 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1755 | `"  if($p === false){ return ''; }"` |
|     - | 1756 | `"  return substr($this->name,0,$p);"` |
|     - | 1757 | `" }"` |
|     - | 1758 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 1759 | `" public function isInternal(){ $i = $this->__rinfo(); return $i['internal']; }"` |
|     - | 1760 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 1761 | `" public function isInterface(){ $i = $this->__rinfo(); return $i['interface']; }"` |
|     - | 1762 | `" public function isTrait(){ $i = $this->__rinfo(); return $i['trait']; }"` |
|     - | 1763 | `" public function isAbstract(){ $i = $this->__rinfo(); return $i['abstract']; }"` |
|     - | 1764 | `" public function isFinal(){ $i = $this->__rinfo(); return $i['final']; }"` |
|     - | 1765 | `" public function isReadOnly(){ $i = $this->__rinfo(); return $i['readonly']; }"` |
|     - | 1766 | `" public function isEnum(){ $i = $this->__rinfo(); return $i['enum']; }"` |
|     - | 1767 | `" public function isAnonymous(){ return strpos($this->name,'class@anonymous') === 0; }"` |
|     - | 1768 | `" public function getModifiers(){"` |
|     - | 1769 | `"  $i = $this->__rinfo();"` |
|     - | 1770 | `"  $m = 0;"` |
|     - | 1771 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 1772 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 1773 | `"  if($i['readonly']){ $m \|= 65536; }"` |
|     - | 1774 | `"  return $m;"` |
|     - | 1775 | `" }"` |
|     - | 1776 | `" public function getParentClass(){"` |
|     - | 1777 | `"  $i = $this->__rinfo();"` |
|     - | 1778 | `"  if($i['parent'] === null){ return false; }"` |
|     - | 1779 | `"  return new ReflectionClass($i['parent']);"` |
|     - | 1780 | `" }"` |
|     - | 1781 | `" public function getInterfaceNames(){ $i = $this->__rinfo(); return $i['interfaces']; }"` |
|     - | 1782 | `" public function getInterfaces(){"` |
|     - | 1783 | `"  $i = $this->__rinfo();"` |
|     - | 1784 | `"  $out = array();"` |
|     - | 1785 | `"  foreach($i['interfaces'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1786 | `"  return $out;"` |
|     - | 1787 | `" }"` |
|     - | 1788 | `" public function getTraitNames(){ $i = $this->__rinfo(); return $i['traits']; }"` |
|     - | 1789 | `" public function getTraits(){"` |
|     - | 1790 | `"  $i = $this->__rinfo();"` |
|     - | 1791 | `"  $out = array();"` |
|     - | 1792 | `"  foreach($i['traits'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1793 | `"  return $out;"` |
|     - | 1794 | `" }"` |
|     - | 1795 | `" public function getTraitAliases(){ return array(); }"` |
|     - | 1796 | `" public function implementsInterface($interface){"` |
|     - | 1797 | `"  if($interface instanceof ReflectionClass){ $interface = $interface->name; }"` |
|     - | 1798 | `"  $target = __reflect_class_info($interface);"` |
|     - | 1799 | `"  if($target === null){"` |
|     - | 1800 | `"   throw new ReflectionException('Interface \"'.$interface.'\" does not exist');"` |
|     - | 1801 | `"  }"` |
|     - | 1802 | `"  if(!$target['interface']){"` |
|     - | 1803 | `"   throw new ReflectionException($target['name'].' is not an interface');"` |
|     - | 1804 | `"  }"` |
|     - | 1805 | `"  $name = $target['name'];"` |
|     - | 1806 | `"  if($this->name === $name){ return true; }"` |
|     - | 1807 | `"  $i = $this->__rinfo();"` |
|     - | 1808 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1809 | `"  return false;"` |
|     - | 1810 | `" }"` |
|     - | 1811 | `" public function isSubclassOf($class){"` |
|     - | 1812 | `"  if($class instanceof ReflectionClass){ $class = $class->name; }"` |
|     - | 1813 | `"  $target = __reflect_class_info($class);"` |
|     - | 1814 | `"  if($target === null){"` |
|     - | 1815 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 1816 | `"  }"` |
|     - | 1817 | `"  $name = $target['name'];"` |
|     - | 1818 | `"  if($name === $this->name){ return false; }"` |
|     - | 1819 | `"  $i = $this->__rinfo();"` |
|     - | 1820 | `"  $p = $i['parent'];"` |
|     - | 1821 | `"  while($p !== null){"` |
|     - | 1822 | `"   if($p === $name){ return true; }"` |
|     - | 1823 | `"   $pi = __reflect_class_info($p);"` |
|     - | 1824 | `"   $p = $pi['parent'];"` |
|     - | 1825 | `"  }"` |
|     - | 1826 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1827 | `"  return false;"` |
|     - | 1828 | `" }"` |
|     - | 1829 | `" public function isInstance($object){"` |
|     - | 1830 | `"  if(!is_object($object)){"` |
|     - | 1831 | `"   throw new TypeError('ReflectionClass::isInstance(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 1832 | `"  }"` |
|     - | 1833 | `"  return is_a($object,$this->name);"` |
|     - | 1834 | `" }"` |
|     - | 1835 | `" public function hasMethod($name){"` |
|     - | 1836 | `"  $i = $this->__rinfo();"` |
|     - | 1837 | `"  $l = strtolower($name);"` |
|     - | 1838 | `"  foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ return true; } }"` |
|     - | 1839 | `"  return false;"` |
|     - | 1840 | `" }"` |
|     - | 1841 | `" public function hasProperty($name){"` |
|     - | 1842 | `"  $i = $this->__rinfo();"` |
|     - | 1843 | `"  if(isset($i['props'][$name])){ return true; }"` |
|     - | 1844 | `"  if($this->__obj !== null){ return (__reflect_prop_state($this->__obj, $name) & 1) !== 0; }"` |
|     - | 1845 | `"  return false;"` |
|     - | 1846 | `" }"` |
|     - | 1847 | `" public function hasConstant($name){ $i = $this->__rinfo(); return isset($i['consts'][$name]); }"` |
|     - | 1848 | `" public function getConstant($name){"` |
|     - | 1849 | `"  $i = $this->__rinfo();"` |
|     - | 1850 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 1851 | `"  return __reflect_const_value($this->name,$name);"` |
|     - | 1852 | `" }"` |
|     - | 1853 | `" public function getConstants($filter = null){"` |
|     - | 1854 | `"  $i = $this->__rinfo();"` |
|     - | 1855 | `"  $out = array();"` |
|     - | 1856 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 1857 | `"   if($filter !== null){"` |
|     - | 1858 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 1859 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 1860 | `"   }"` |
|     - | 1861 | `"   $out[$k] = __reflect_const_value($this->name,$k);"` |
|     - | 1862 | `"  }"` |
|     - | 1863 | `"  return $out;"` |
|     - | 1864 | `" }"` |
|     - | 1865 | `" public function getStartLine(){"` |
|     - | 1866 | `"  $i = $this->__rinfo();"` |
|     - | 1867 | `"  if($i['internal']){ return false; }"` |
|     - | 1868 | `"  return $i['line'];"` |
|     - | 1869 | `" }"` |
|     - | 1870 | `" public function getEndLine(){"` |
|     - | 1871 | `"  $i = $this->__rinfo();"` |
|     - | 1872 | `"  if($i['internal']){ return false; }"` |
|     - | 1873 | `"  return $i['endline'];"` |
|     - | 1874 | `" }"` |
|     - | 1875 | `" public function getFileName(){ $i = $this->__rinfo(); return $i['file']; }"` |
|     - | 1876 | `" public function getDocComment(){ $i = $this->__rinfo(); return $i['doc']; }"` |
|     - | 1877 | `" public function isInstantiable(){"` |
|     - | 1878 | `"  $i = $this->__rinfo();"` |
|     - | 1879 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract'] \|\| $i['enum']){ return false; }"` |
|     - | 1880 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){ return false; }"` |
|     - | 1881 | `"  return true;"` |
|     - | 1882 | `" }"` |
|     - | 1883 | `" public function isCloneable(){"` |
|     - | 1884 | `"  $i = $this->__rinfo();"` |
|     - | 1885 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1886 | `"  if($i['clonevis'] !== 0 && $i['clonevis'] !== 1){ return false; }"` |
|     - | 1887 | `"  return true;"` |
|     - | 1888 | `" }"` |
|     - | 1889 | `" public function isIterable(){"` |
|     - | 1890 | `"  $i = $this->__rinfo();"` |
|     - | 1891 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1892 | `"  return $i['iterable'];"` |
|     - | 1893 | `" }"` |
|     - | 1894 | `" public function isIterateable(){ return $this->isIterable(); }"` |
|     - | 1895 | `" public function newInstance(...$args){ return $this->__rnew($args); }"` |
|     - | 1896 | `" public function newInstanceArgs(array $args = array()){ return $this->__rnew($args); }"` |
|     - | 1897 | `" protected function __rnew($args){"` |
|     - | 1898 | `"  $i = $this->__rinfo();"` |
|     - | 1899 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1900 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){"` |
|     - | 1901 | `"   throw new ReflectionException('Access to non-public constructor of class '.$this->name);"` |
|     - | 1902 | `"  }"` |
|     - | 1903 | `"  if($i['ctorvis'] === 0 && count($args) > 0){"` |
|     - | 1904 | `"   throw new ReflectionException('Class '.$this->name.' does not have a constructor, so you cannot pass any constructor arguments');"` |
|     - | 1905 | `"  }"` |
|     - | 1906 | `"  return __reflect_new_instance($this->name,$args);"` |
|     - | 1907 | `" }"` |
|     - | 1908 | `" protected function __rcheckInstantiable($i){"` |
|     - | 1909 | `"  if($i['interface']){ throw new Error('Cannot instantiate interface '.$this->name); }"` |
|     - | 1910 | `"  if($i['trait']){ throw new Error('Cannot instantiate trait '.$this->name); }"` |
|     - | 1911 | `"  if($i['abstract']){ throw new Error('Cannot instantiate abstract class '.$this->name); }"` |
|     - | 1912 | `" }"` |
|     - | 1913 | `" public function newInstanceWithoutConstructor(){"` |
|     - | 1914 | `"  $i = $this->__rinfo();"` |
|     - | 1915 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1916 | `"  return __reflect_new_no_ctor($this->name);"` |
|     - | 1917 | `" }"` |
|     - | 1918 | `" public function getStaticProperties(){"` |
|     - | 1919 | `"  $i = $this->__rinfo();"` |
|     - | 1920 | `"  $out = array();"` |
|     - | 1921 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1922 | `"   if($p['static']){ $out[$k] = __reflect_static_value($this->name,$k); }"` |
|     - | 1923 | `"  }"` |
|     - | 1924 | `"  return $out;"` |
|     - | 1925 | `" }"` |
|     - | 1926 | `" public function getStaticPropertyValue($name, ...$def){"` |
|     - | 1927 | `"  $i = $this->__rinfo();"` |
|     - | 1928 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 1929 | `"   if(count($def) > 0){ return $def[0]; }"` |
|     - | 1930 | `"   throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 1931 | `"  }"` |
|     - | 1932 | `"  return __reflect_static_value($this->name,$name);"` |
|     - | 1933 | `" }"` |
|     - | 1934 | `" public function setStaticPropertyValue($name,$value){"` |
|     - | 1935 | `"  $i = $this->__rinfo();"` |
|     - | 1936 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 1937 | `"   throw new ReflectionException('Class '.$this->name.' does not have a property named '.$name);"` |
|     - | 1938 | `"  }"` |
|     - | 1939 | `"  __reflect_static_set($this->name,$name,$value);"` |
|     - | 1940 | `" }"` |
|     - | 1941 | `" public function getDefaultProperties(){"` |
|     - | 1942 | `"  $i = $this->__rinfo();"` |
|     - | 1943 | `"  $out = array();"` |
|     - | 1944 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1945 | `"   if($p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 1946 | `"  }"` |
|     - | 1947 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1948 | `"   if(!$p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 1949 | `"  }"` |
|     - | 1950 | `"  return $out;"` |
|     - | 1951 | `" }"` |
|     - | 1952 | `" public function getProperty($name){"` |
|     - | 1953 | `"  $i = $this->__rinfo();"` |
|     - | 1954 | `"  if(isset($i['props'][$name])){"` |
|     - | 1955 | `"   return new ReflectionProperty($this->name, $name);"` |
|     - | 1956 | `"  }"` |
|     - | 1957 | `"  if($this->__obj !== null && (__reflect_prop_state($this->__obj, $name) & 1)){"` |
|     - | 1958 | `"   return new ReflectionProperty($this->__obj, $name);"` |
|     - | 1959 | `"  }"` |
|     - | 1960 | `"  throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 1961 | `" }"` |
|     - | 1962 | `" public function getProperties($filter = null){"` |
|     - | 1963 | `"  $i = $this->__rinfo();"` |
|     - | 1964 | `"  $out = array();"` |
|     - | 1965 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1966 | `"   if($filter !== null){"` |
|     - | 1967 | `"    $m = ($p['vis'] === 1 ? 1 : ($p['vis'] === 2 ? 2 : 4));"` |
|     - | 1968 | `"    if($p['static']){ $m \|= 16; }"` |
|     - | 1969 | `"    if($p['readonly']){ $m \|= 128; }"` |
|     - | 1970 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 1971 | `"   }"` |
|     - | 1972 | `"   $out[] = new ReflectionProperty($this->name, $k);"` |
|     - | 1973 | `"  }"` |
|     - | 1974 | `"  if($this->__obj !== null){"` |
|     - | 1975 | `"   foreach(__reflect_dyn_props($this->__obj) as $k){"` |
|     - | 1976 | `"    if(isset($i['props'][$k])){ continue; }"` |
|     - | 1977 | `"    if($filter !== null && ($filter & 1) === 0){ continue; }"` |
|     - | 1978 | `"    $out[] = new ReflectionProperty($this->__obj, $k);"` |
|     - | 1979 | `"   }"` |
|     - | 1980 | `"  }"` |
|     - | 1981 | `"  return $out;"` |
|     - | 1982 | `" }"` |
|     - | 1983 | `" public function getMethod($name){"` |
|     - | 1984 | `"  $i = $this->__rinfo();"` |
|     - | 1985 | `"  $found = null;"` |
|     - | 1986 | `"  if(isset($i['methods'][$name])){"` |
|     - | 1987 | `"   $found = $name;"` |
|     - | 1988 | `"  }else{"` |
|     - | 1989 | `"   $l = strtolower($name);"` |
|     - | 1990 | `"   foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ $found = $k; break; } }"` |
|     - | 1991 | `"  }"` |
|     - | 1992 | `"  if($found === null){"` |
|     - | 1993 | `"   throw new ReflectionException('Method '.$this->name.'::'.$name.'() does not exist');"` |
|     - | 1994 | `"  }"` |
|     - | 1995 | `"  return new ReflectionMethod($this->name, $found);"` |
|     - | 1996 | `" }"` |
|     - | 1997 | `" public function getMethods($filter = null){"` |
|     - | 1998 | `"  $i = $this->__rinfo();"` |
|     - | 1999 | `"  $out = array();"` |
|     - | 2000 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 2001 | `"   if($filter !== null){"` |
|     - | 2002 | `"    $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2003 | `"    if($m['static']){ $mod \|= 16; }"` |
|     - | 2004 | `"    if($m['abstract']){ $mod \|= 64; }"` |
|     - | 2005 | `"    if($m['final']){ $mod \|= 32; }"` |
|     - | 2006 | `"    if(($mod & $filter) === 0){ continue; }"` |
|     - | 2007 | `"   }"` |
|     - | 2008 | `"   $out[] = new ReflectionMethod($this->name, $k);"` |
|     - | 2009 | `"  }"` |
|     - | 2010 | `"  return $out;"` |
|     - | 2011 | `" }"` |
|     - | 2012 | `" public function getConstructor(){"` |
|     - | 2013 | `"  $i = $this->__rinfo();"` |
|     - | 2014 | `"  if(isset($i['methods']['__construct'])){"` |
|     - | 2015 | `"   return new ReflectionMethod($this->name, '__construct');"` |
|     - | 2016 | `"  }"` |
|     - | 2017 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 2018 | `"   if(strtolower($k) === '__construct'){ return new ReflectionMethod($this->name, $k); }"` |
|     - | 2019 | `"  }"` |
|     - | 2020 | `"  if($i['ctorvis'] !== 0 && isset($i['methods'][$this->name])){"` |
|     - | 2021 | `"   return new ReflectionMethod($this->name, $this->name);"` |
|     - | 2022 | `"  }"` |
|     - | 2023 | `"  return null;"` |
|     - | 2024 | `" }"` |
|     - | 2025 | `" public function getReflectionConstant($name){"` |
|     - | 2026 | `"  $i = $this->__rinfo();"` |
|     - | 2027 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 2028 | `"  return new ReflectionClassConstant($this->name, $name);"` |
|     - | 2029 | `" }"` |
|     - | 2030 | `" public function getReflectionConstants($filter = null){"` |
|     - | 2031 | `"  $i = $this->__rinfo();"` |
|     - | 2032 | `"  $out = array();"` |
|     - | 2033 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 2034 | `"   if($filter !== null){"` |
|     - | 2035 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 2036 | `"    if($c['final']){ $m \|= 32; }"` |
|     - | 2037 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 2038 | `"   }"` |
|     - | 2039 | `"   $out[] = new ReflectionClassConstant($this->name, $k);"` |
|     - | 2040 | `"  }"` |
|     - | 2041 | `"  return $out;"` |
|     - | 2042 | `" }"` |
|     - | 2043 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2044 | `"  $i = $this->__rinfo();"` |
|     - | 2045 | `"  return __reflect_build_attrs($i['attrs'], array('class', $this->name, null, 0), 1, $name, $flags);"` |
|     - | 2046 | `" }"` |
|     - | 2047 | `" public function getExtensionName(){ $i = $this->__rinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2048 | `" public function getExtension(){ $i = $this->__rinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2049 | `" public function newLazyGhost($initializer, $options = 0){"` |
|     - | 2050 | `"  throw new Error('ReflectionClass::newLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2051 | `" }"` |
|     - | 2052 | `" public function newLazyProxy($factory, $options = 0){"` |
|     - | 2053 | `"  throw new Error('ReflectionClass::newLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2054 | `" }"` |
|     - | 2055 | `" public function resetAsLazyGhost($object, $initializer, $options = 0){"` |
|     - | 2056 | `"  throw new Error('ReflectionClass::resetAsLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2057 | `" }"` |
|     - | 2058 | `" public function resetAsLazyProxy($object, $factory, $options = 0){"` |
|     - | 2059 | `"  throw new Error('ReflectionClass::resetAsLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2060 | `" }"` |
|     - | 2061 | `" public function getLazyInitializer($object){ return null; }"` |
|     - | 2062 | `" public function initializeLazyObject($object){ return $object; }"` |
|     - | 2063 | `" public function markLazyObjectAsInitialized($object){ return $object; }"` |
|     - | 2064 | `" public function isUninitializedLazyObject($object){ return false; }"` |
|     - | 2065 | `" public function __toString(){ return __reflect_export_class($this); }"` |
|     - | 2066 | `"}"` |
|     - | 2067 | `"class ReflectionObject extends ReflectionClass {"` |
|     - | 2068 | `" public function __construct($object){"` |
|     - | 2069 | `"  if(!is_object($object)){"` |
|     - | 2070 | `"   throw new TypeError('ReflectionObject::__construct(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 2071 | `"  }"` |
|     - | 2072 | `"  parent::__construct($object);"` |
|     - | 2073 | `"  $this->__obj = $object;"` |
|     - | 2074 | `" }"` |
|     - | 2075 | `"}"` |
|     - | 2076 | `;` |
|     - | 2077 | `/*` |
|     - | 2078 | ` * Chunk 2: ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,` |
|     - | 2079 | ` * ReflectionParameter.` |
|     - | 2080 | ` */` |
|     - | 2081 | `static const char zReflectLib2[] =` |
|     - | 2082 | `"abstract class ReflectionFunctionAbstract implements Reflector {"` |
|     - | 2083 | `" public $name;"` |
|     - | 2084 | `" protected $__cl = null;"` |
|     - | 2085 | `" protected function __rfinfo(){"` |
|     - | 2086 | `"  if($this->__cl !== null){ return __reflect_sig_fixup(__reflect_func_info($this->__cl)); }"` |
|     - | 2087 | `"  return __reflect_sig_fixup(__reflect_func_info($this->name));"` |
|     - | 2088 | `" }"` |
|     - | 2089 | `" protected function __rftarget(){ return $this->__cl !== null ? $this->__cl : $this->name; }"` |
|     - | 2090 | `" protected function __rpspec(){ return $this->__rftarget(); }"` |
|     - | 2091 | `" public function getName(){ return $this->name; }"` |
|     - | 2092 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 2093 | `" public function getNamespaceName(){"` |
|     - | 2094 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2095 | `"  if($p === false){ return ''; }"` |
|     - | 2096 | `"  return substr($this->name,0,$p);"` |
|     - | 2097 | `" }"` |
|     - | 2098 | `" public function getShortName(){"` |
|     - | 2099 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2100 | `"  if($p === false){ return $this->name; }"` |
|     - | 2101 | `"  return substr($this->name,$p+1);"` |
|     - | 2102 | `" }"` |
|     - | 2103 | `" public function isClosure(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2104 | `" public function isGenerator(){ $i = $this->__rfinfo(); return $i['generator']; }"` |
|     - | 2105 | `" public function isVariadic(){ $i = $this->__rfinfo(); return $i['variadic']; }"` |
|     - | 2106 | `" public function returnsReference(){ $i = $this->__rfinfo(); return $i['byref']; }"` |
|     - | 2107 | `" public function isInternal(){ $i = $this->__rfinfo(); return $i['internal']; }"` |
|     - | 2108 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 2109 | `" public function isDeprecated(){ $i = $this->__rfinfo(); return __reflect_has_deprecated($i['attrs']); }"` |
|     - | 2110 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['fstatic']; }"` |
|     - | 2111 | `" public function getFileName(){ $i = $this->__rfinfo(); return $i['file']; }"` |
|     - | 2112 | `" public function getStartLine(){"` |
|     - | 2113 | `"  $i = $this->__rfinfo();"` |
|     - | 2114 | `"  if($i['internal']){ return false; }"` |
|     - | 2115 | `"  return $i['line'];"` |
|     - | 2116 | `" }"` |
|     - | 2117 | `" public function getEndLine(){"` |
|     - | 2118 | `"  $i = $this->__rfinfo();"` |
|     - | 2119 | `"  if($i['internal']){ return false; }"` |
|     - | 2120 | `"  return $i['endline'];"` |
|     - | 2121 | `" }"` |
|     - | 2122 | `" public function getDocComment(){ $i = $this->__rfinfo(); return $i['doc']; }"` |
|     - | 2123 | `" public function hasReturnType(){ $i = $this->__rfinfo(); return $i['rettext'] !== null; }"` |
|     - | 2124 | `" public function getReturnType(){ $i = $this->__rfinfo(); return __reflect_make_type($i['rettext']); }"` |
|     - | 2125 | `" public function hasTentativeReturnType(){ return false; }"` |
|     - | 2126 | `" public function getTentativeReturnType(){ return null; }"` |
|     - | 2127 | `" public function getNumberOfParameters(){"` |
|     - | 2128 | `"  $i = $this->__rfinfo();"` |
|     - | 2129 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2130 | `"  return count($i['params']);"` |
|     - | 2131 | `" }"` |
|     - | 2132 | `" public function getNumberOfRequiredParameters(){"` |
|     - | 2133 | `"  $i = $this->__rfinfo();"` |
|     - | 2134 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2135 | `"  $req = 0;"` |
|     - | 2136 | `"  $n = count($i['params']);"` |
|     - | 2137 | `"  for($k = $n - 1; $k >= 0; $k--){"` |
|     - | 2138 | `"   $p = $i['params'][$k];"` |
|     - | 2139 | `"   if(!$p['variadic'] && !$p['hasdef']){ $req = $k + 1; break; }"` |
|     - | 2140 | `"  }"` |
|     - | 2141 | `"  return $req;"` |
|     - | 2142 | `" }"` |
|     - | 2143 | `" public function getParameters(){"` |
|     - | 2144 | `"  $i = $this->__rfinfo();"` |
|     - | 2145 | `"  $out = array();"` |
|     - | 2146 | `"  $spec = $this->__rpspec();"` |
|     - | 2147 | `"  foreach($i['params'] as $p){"` |
|     - | 2148 | `"   $out[] = new ReflectionParameter($spec, $p['pos']);"` |
|     - | 2149 | `"  }"` |
|     - | 2150 | `"  return $out;"` |
|     - | 2151 | `" }"` |
|     - | 2152 | `" public function getStaticVariables(){ $i = $this->__rfinfo(); return $i['statics']; }"` |
|     - | 2153 | `" public function getClosureThis(){"` |
|     - | 2154 | `"  $i = $this->__rfinfo();"` |
|     - | 2155 | `"  return isset($i['this']) ? $i['this'] : null;"` |
|     - | 2156 | `" }"` |
|     - | 2157 | `" public function getClosureScopeClass(){"` |
|     - | 2158 | `"  $i = $this->__rfinfo();"` |
|     - | 2159 | `"  if(isset($i['scope'])){ return new ReflectionClass($i['scope']); }"` |
|     - | 2160 | `"  if(isset($i['this'])){ return new ReflectionClass(get_class($i['this'])); }"` |
|     - | 2161 | `"  return null;"` |
|     - | 2162 | `" }"` |
|     - | 2163 | `" public function getClosureCalledClass(){ return $this->getClosureScopeClass(); }"` |
|     - | 2164 | `" public function getClosureUsedVariables(){"` |
|     - | 2165 | `"  $i = $this->__rfinfo();"` |
|     - | 2166 | `"  return isset($i['used']) ? $i['used'] : array();"` |
|     - | 2167 | `" }"` |
|     - | 2168 | `" public function getExtensionName(){ $i = $this->__rfinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2169 | `" public function getExtension(){ $i = $this->__rfinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2170 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2171 | `"  $i = $this->__rfinfo();"` |
|     - | 2172 | `"  if($this instanceof ReflectionMethod){"` |
|     - | 2173 | `"   $spec = array('method', $this->class, $this->name, 0);"` |
|     - | 2174 | `"   $target = 4;"` |
|     - | 2175 | `"  }else{"` |
|     - | 2176 | `"   $spec = array('fn', $this->__rftarget(), null, 0);"` |
|     - | 2177 | `"   $target = 2;"` |
|     - | 2178 | `"  }"` |
|     - | 2179 | `"  return __reflect_build_attrs($i['attrs'], $spec, $target, $name, $flags);"` |
|     - | 2180 | `" }"` |
|     - | 2181 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2182 | `"}"` |
|     - | 2183 | `"class ReflectionFunction extends ReflectionFunctionAbstract {"` |
|     - | 2184 | `" const IS_DEPRECATED = 2048;"` |
|     - | 2185 | `" public function __construct($function){"` |
|     - | 2186 | `"  if($function instanceof Closure){"` |
|     - | 2187 | `"   $this->__cl = $function;"` |
|     - | 2188 | `"   $i = $this->__rfinfo();"` |
|     - | 2189 | `"   if($i['closure']){"` |
|     - | 2190 | `"    $f = $i['file'] === false ? '' : $i['file'];"` |
|     - | 2191 | `"    $this->name = '{closure:'.$f.':'.$i['line'].'}';"` |
|     - | 2192 | `"   }else{"` |
|     - | 2193 | `"    $this->name = $i['name'];"` |
|     - | 2194 | `"   }"` |
|     - | 2195 | `"   return;"` |
|     - | 2196 | `"  }"` |
|     - | 2197 | `"  if(!is_string($function)){"` |
|     - | 2198 | `"   throw new TypeError('ReflectionFunction::__construct(): Argument #1 ($function) must be of type Closure\|string, '.get_debug_type($function).' given');"` |
|     - | 2199 | `"  }"` |
|     - | 2200 | `"  $i = __reflect_func_info($function);"` |
|     - | 2201 | `"  if($i === null){"` |
|     - | 2202 | `"   throw new ReflectionException('Function '.$function.'() does not exist');"` |
|     - | 2203 | `"  }"` |
|     - | 2204 | `"  if($i['closure']){"` |
|     - | 2205 | `"   $this->name = '{closure:'.($i['file'] === false ? '' : $i['file']).':'.$i['line'].'}';"` |
|     - | 2206 | `"   $this->__cl = __reflect_closure($function, null, null);"` |
|     - | 2207 | `"  }else{"` |
|     - | 2208 | `"   $this->name = $i['name'];"` |
|     - | 2209 | `"  }"` |
|     - | 2210 | `" }"` |
|     - | 2211 | `" public function invoke(...$args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2212 | `" public function invokeArgs(array $args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2213 | `" public function getClosure(){"` |
|     - | 2214 | `"  if($this->__cl !== null){ return $this->__cl; }"` |
|     - | 2215 | `"  return __reflect_closure($this->name, null, null);"` |
|     - | 2216 | `" }"` |
|     - | 2217 | `" public function isAnonymous(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2218 | `" public function isDisabled(){ return false; }"` |
|     - | 2219 | `"}"` |
|     - | 2220 | `"class ReflectionMethod extends ReflectionFunctionAbstract {"` |
|     - | 2221 | `" const IS_PUBLIC = 1;"` |
|     - | 2222 | `" const IS_PROTECTED = 2;"` |
|     - | 2223 | `" const IS_PRIVATE = 4;"` |
|     - | 2224 | `" const IS_STATIC = 16;"` |
|     - | 2225 | `" const IS_FINAL = 32;"` |
|     - | 2226 | `" const IS_ABSTRACT = 64;"` |
|     - | 2227 | `" public $class;"` |
|     - | 2228 | `" public function __construct($objectOrMethod, $method = null){"` |
|     - | 2229 | `"  if($method === null){"` |
|     - | 2230 | `"   if(!is_string($objectOrMethod) \|\| strpos($objectOrMethod,'::') === false){"` |
|     - | 2231 | `"    throw new TypeError('ReflectionMethod::__construct(): Argument #1 ($objectOrMethod) must be of type object\|string, '.get_debug_type($objectOrMethod).' given');"` |
|     - | 2232 | `"   }"` |
|     - | 2233 | `"   $p = strpos($objectOrMethod,'::');"` |
|     - | 2234 | `"   $method = substr($objectOrMethod,$p+2);"` |
|     - | 2235 | `"   $objectOrMethod = substr($objectOrMethod,0,$p);"` |
|     - | 2236 | `"  }"` |
|     - | 2237 | `"  $ci = __reflect_class_info($objectOrMethod);"` |
|     - | 2238 | `"  if($ci === null){"` |
|     - | 2239 | `"   throw new ReflectionException('Class \"'.$objectOrMethod.'\" does not exist');"` |
|     - | 2240 | `"  }"` |
|     - | 2241 | `"  $this->class = $ci['name'];"` |
|     - | 2242 | `"  $found = null;"` |
|     - | 2243 | `"  if(isset($ci['methods'][$method])){"` |
|     - | 2244 | `"   $found = $method;"` |
|     - | 2245 | `"  }else{"` |
|     - | 2246 | `"   $l = strtolower($method);"` |
|     - | 2247 | `"   foreach($ci['methods'] as $k => $m){"` |
|     - | 2248 | `"    if(strtolower($k) === $l){ $found = $k; break; }"` |
|     - | 2249 | `"   }"` |
|     - | 2250 | `"  }"` |
|     - | 2251 | `"  if($found === null){"` |
|     - | 2252 | `"   throw new ReflectionException('Method '.$this->class.'::'.$method.'() does not exist');"` |
|     - | 2253 | `"  }"` |
|     - | 2254 | `"  $this->name = $found;"` |
|     - | 2255 | `" }"` |
|     - | 2256 | `" public static function createFromMethodName($name){"` |
|     - | 2257 | `"  return new ReflectionMethod($name);"` |
|     - | 2258 | `" }"` |
|     - | 2259 | `" protected function __rfinfo(){ return __reflect_func_info($this->class, $this->name); }"` |
|     - | 2260 | `" protected function __rpspec(){ return array($this->class, $this->name); }"` |
|     - | 2261 | `" public function getDeclaringClass(){"` |
|     - | 2262 | `"  $i = $this->__rfinfo();"` |
|     - | 2263 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2264 | `" }"` |
|     - | 2265 | `" public function getModifiers(){"` |
|     - | 2266 | `"  $i = $this->__rfinfo();"` |
|     - | 2267 | `"  $m = ($i['vis'] === 1 ? 1 : ($i['vis'] === 2 ? 2 : 4));"` |
|     - | 2268 | `"  if($i['mstatic']){ $m \|= 16; }"` |
|     - | 2269 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 2270 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 2271 | `"  return $m;"` |
|     - | 2272 | `" }"` |
|     - | 2273 | `" public function isPublic(){ $i = $this->__rfinfo(); return $i['vis'] === 1; }"` |
|     - | 2274 | `" public function isProtected(){ $i = $this->__rfinfo(); return $i['vis'] === 2; }"` |
|     - | 2275 | `" public function isPrivate(){ $i = $this->__rfinfo(); return $i['vis'] === 3; }"` |
|     - | 2276 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['mstatic']; }"` |
|     - | 2277 | `" public function isAbstract(){ $i = $this->__rfinfo(); return $i['abstract']; }"` |
|     - | 2278 | `" public function isFinal(){ $i = $this->__rfinfo(); return $i['final']; }"` |
|     - | 2279 | `" public function isConstructor(){ return strtolower($this->name) === '__construct'; }"` |
|     - | 2280 | `" public function isDestructor(){ return strtolower($this->name) === '__destruct'; }"` |
|     - | 2281 | `" public function invoke($object = null, ...$args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2282 | `" public function invokeArgs($object, array $args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2283 | `" protected function __rinvoke($object, $args){"` |
|     - | 2284 | `"  $i = $this->__rfinfo();"` |
|     - | 2285 | `"  if(!$i['mstatic']){"` |
|     - | 2286 | `"   if(!is_object($object)){"` |
|     - | 2287 | `"    throw new ReflectionException('Trying to invoke non static method '.$this->class.'::'.$this->name.'() without an object');"` |
|     - | 2288 | `"   }"` |
|     - | 2289 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2290 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2291 | `"   }"` |
|     - | 2292 | `"  }else{"` |
|     - | 2293 | `"   $object = null;"` |
|     - | 2294 | `"  }"` |
|     - | 2295 | `"  return __reflect_invoke($this->class, $this->name, $object, $args);"` |
|     - | 2296 | `" }"` |
|     - | 2297 | `" public function getClosure($object = null){"` |
|     - | 2298 | `"  $i = $this->__rfinfo();"` |
|     - | 2299 | `"  if(!$i['mstatic']){"` |
|     - | 2300 | `"   if($object === null){"` |
|     - | 2301 | `"    throw new ValueError('ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods');"` |
|     - | 2302 | `"   }"` |
|     - | 2303 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2304 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2305 | `"   }"` |
|     - | 2306 | `"  }else{"` |
|     - | 2307 | `"   $object = null;"` |
|     - | 2308 | `"  }"` |
|     - | 2309 | `"  return __reflect_closure($this->class, $this->name, $object);"` |
|     - | 2310 | `" }"` |
|     - | 2311 | `" public function setAccessible($accessible){ }"` |
|     - | 2312 | `" public function hasPrototype(){ return $this->__rproto() !== null; }"` |
|     - | 2313 | `" public function getPrototype(){"` |
|     - | 2314 | `"  $p = $this->__rproto();"` |
|     - | 2315 | `"  if($p === null){"` |
|     - | 2316 | `"   throw new ReflectionException('Method '.$this->class.'::'.$this->name.' does not have a prototype');"` |
|     - | 2317 | `"  }"` |
|     - | 2318 | `"  return new ReflectionMethod($p, $this->name);"` |
|     - | 2319 | `" }"` |
|     - | 2320 | `" protected function __rproto(){"` |
|     - | 2321 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2322 | `"  $l = strtolower($this->name);"` |
|     - | 2323 | `"  $p = $ci['parent'];"` |
|     - | 2324 | `"  while($p !== null){"` |
|     - | 2325 | `"   $pi = __reflect_class_info($p);"` |
|     - | 2326 | `"   foreach($pi['methods'] as $k => $m){"` |
|     - | 2327 | `"    if(strtolower($k) === $l && $m['vis'] !== 3){ return $m['decl']; }"` |
|     - | 2328 | `"   }"` |
|     - | 2329 | `"   $p = $pi['parent'];"` |
|     - | 2330 | `"  }"` |
|     - | 2331 | `"  foreach($ci['interfaces'] as $if){"` |
|     - | 2332 | `"   $ii = __reflect_class_info($if);"` |
|     - | 2333 | `"   foreach($ii['methods'] as $k => $m){"` |
|     - | 2334 | `"    if(strtolower($k) === $l){ return $ii['name']; }"` |
|     - | 2335 | `"   }"` |
|     - | 2336 | `"  }"` |
|     - | 2337 | `"  return null;"` |
|     - | 2338 | `" }"` |
|     - | 2339 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2340 | `"}"` |
|     - | 2341 | `"class ReflectionParameter implements Reflector {"` |
|     - | 2342 | `" public $name;"` |
|     - | 2343 | `" protected $__t;"` |
|     - | 2344 | `" protected $__m = null;"` |
|     - | 2345 | `" protected $__p = 0;"` |
|     - | 2346 | `" public function __construct($function, $param){"` |
|     - | 2347 | `"  $m = null;"` |
|     - | 2348 | `"  $t = $function;"` |
|     - | 2349 | `"  if(is_array($function)){"` |
|     - | 2350 | `"   $t = $function[0];"` |
|     - | 2351 | `"   $m = $function[1];"` |
|     - | 2352 | `"   if(is_object($t)){ $t = get_class($t); }"` |
|     - | 2353 | `"  }else if(is_string($function) && strpos($function,'::') !== false){"` |
|     - | 2354 | `"   $p = strpos($function,'::');"` |
|     - | 2355 | `"   $m = substr($function,$p+2);"` |
|     - | 2356 | `"   $t = substr($function,0,$p);"` |
|     - | 2357 | `"  }"` |
|     - | 2358 | `"  if($m !== null){"` |
|     - | 2359 | `"   $rm = new ReflectionMethod($t, $m);"` |
|     - | 2360 | `"   $t = $rm->class;"` |
|     - | 2361 | `"   $m = $rm->name;"` |
|     - | 2362 | `"   $i = __reflect_func_info($t, $m);"` |
|     - | 2363 | `"  }else if($function instanceof Closure){"` |
|     - | 2364 | `"   $t = $function;"` |
|     - | 2365 | `"   $i = __reflect_func_info($function);"` |
|     - | 2366 | `"  }else{"` |
|     - | 2367 | `"   $i = __reflect_sig_fixup(__reflect_func_info($t));"` |
|     - | 2368 | `"   if($i === null){"` |
|     - | 2369 | `"    throw new ReflectionException('Function '.$t.'() does not exist');"` |
|     - | 2370 | `"   }"` |
|     - | 2371 | `"  }"` |
|     - | 2372 | `"  $found = null;"` |
|     - | 2373 | `"  if(is_int($param)){"` |
|     - | 2374 | `"   if(isset($i['params'][$param])){ $found = $i['params'][$param]; }"` |
|     - | 2375 | `"   if($found === null){"` |
|     - | 2376 | `"    throw new ReflectionException('The parameter specified by its offset could not be found');"` |
|     - | 2377 | `"   }"` |
|     - | 2378 | `"  }else{"` |
|     - | 2379 | `"   foreach($i['params'] as $pp){"` |
|     - | 2380 | `"    if($pp['name'] === $param){ $found = $pp; break; }"` |
|     - | 2381 | `"   }"` |
|     - | 2382 | `"   if($found === null){"` |
|     - | 2383 | `"    throw new ReflectionException('The parameter specified by its name could not be found');"` |
|     - | 2384 | `"   }"` |
|     - | 2385 | `"  }"` |
|     - | 2386 | `"  $this->name = $found['name'];"` |
|     - | 2387 | `"  $this->__t = $t;"` |
|     - | 2388 | `"  $this->__m = $m;"` |
|     - | 2389 | `"  $this->__p = $found['pos'];"` |
|     - | 2390 | `" }"` |
|     - | 2391 | `" protected function __rffull(){"` |
|     - | 2392 | `"  if($this->__m !== null){ return __reflect_func_info($this->__t, $this->__m); }"` |
|     - | 2393 | `"  return __reflect_sig_fixup(__reflect_func_info($this->__t));"` |
|     - | 2394 | `" }"` |
|     - | 2395 | `" protected function __rpinfo(){"` |
|     - | 2396 | `"  $i = $this->__rffull();"` |
|     - | 2397 | `"  return $i['params'][$this->__p];"` |
|     - | 2398 | `" }"` |
|     - | 2399 | `" public function getName(){ return $this->name; }"` |
|     - | 2400 | `" public function getPosition(){ return $this->__p; }"` |
|     - | 2401 | `" public function isPassedByReference(){ $p = $this->__rpinfo(); return $p['byref']; }"` |
|     - | 2402 | `" public function canBePassedByValue(){ return !$this->isPassedByReference(); }"` |
|     - | 2403 | `" public function isVariadic(){ $p = $this->__rpinfo(); return $p['variadic']; }"` |
|     - | 2404 | `" public function isPromoted(){ $p = $this->__rpinfo(); return $p['promoted']; }"` |
|     - | 2405 | `" public function isDefaultValueAvailable(){ $p = $this->__rpinfo(); return $p['hasdef']; }"` |
|     - | 2406 | `" public function isOptional(){"` |
|     - | 2407 | `"  $i = $this->__rffull();"` |
|     - | 2408 | `"  $n = count($i['params']);"` |
|     - | 2409 | `"  for($k = $this->__p; $k < $n; $k++){"` |
|     - | 2410 | `"   $p = $i['params'][$k];"` |
|     - | 2411 | `"   if(!$p['variadic'] && !$p['hasdef']){ return false; }"` |
|     - | 2412 | `"  }"` |
|     - | 2413 | `"  return true;"` |
|     - | 2414 | `" }"` |
|     - | 2415 | `" public function getDefaultValue(){"` |
|     - | 2416 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2417 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2418 | `"  }"` |
|     - | 2419 | `"  $p = $this->__rpinfo();"` |
|     - | 2420 | `"  if(isset($p['deftext'])){"` |
|     - | 2421 | `"   $s = __reflect_sig_scalar($p['deftext']);"` |
|     - | 2422 | `"   if($s[0]){ return $s[1]; }"` |
|     - | 2423 | `"   if($p['deftext'] === 'array (' \|\| strpos($p['deftext'], '[') === 0){ return array(); }"` |
|     - | 2424 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2425 | `"  }"` |
|     - | 2426 | `"  return __reflect_param_default($this->__t, $this->__m, $this->__p);"` |
|     - | 2427 | `" }"` |
|     - | 2428 | `" public function isDefaultValueConstant(){"` |
|     - | 2429 | `"  if(!$this->isDefaultValueAvailable()){ return false; }"` |
|     - | 2430 | `"  $p = $this->__rpinfo();"` |
|     - | 2431 | `"  if(isset($p['deftext'])){ return false; }"` |
|     - | 2432 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p) !== null;"` |
|     - | 2433 | `" }"` |
|     - | 2434 | `" public function getDefaultValueConstantName(){"` |
|     - | 2435 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2436 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2437 | `"  }"` |
|     - | 2438 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p);"` |
|     - | 2439 | `" }"` |
|     - | 2440 | `" public function allowsNull(){"` |
|     - | 2441 | `"  $p = $this->__rpinfo();"` |
|     - | 2442 | `"  if($p['typetext'] === null){ return true; }"` |
|     - | 2443 | `"  if($p['nullable']){ return true; }"` |
|     - | 2444 | `"  return $p['typetext'] === 'mixed' \|\| $p['typetext'] === 'null';"` |
|     - | 2445 | `" }"` |
|     - | 2446 | `" public function hasType(){ $p = $this->__rpinfo(); return $p['typetext'] !== null; }"` |
|     - | 2447 | `" public function getType(){ $p = $this->__rpinfo(); return __reflect_make_type($p['typetext']); }"` |
|     - | 2448 | `" public function getDeclaringFunction(){"` |
|     - | 2449 | `"  if($this->__m !== null){ return new ReflectionMethod($this->__t, $this->__m); }"` |
|     - | 2450 | `"  return new ReflectionFunction($this->__t);"` |
|     - | 2451 | `" }"` |
|     - | 2452 | `" public function getDeclaringClass(){"` |
|     - | 2453 | `"  if($this->__m === null){ return null; }"` |
|     - | 2454 | `"  $i = $this->__rffull();"` |
|     - | 2455 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2456 | `" }"` |
|     - | 2457 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2458 | `"  $p = $this->__rpinfo();"` |
|     - | 2459 | `"  return __reflect_build_attrs($p['attrs'], array('param', $this->__t, $this->__m, $this->__p), 32, $name, $flags);"` |
|     - | 2460 | `" }"` |
|     - | 2461 | `" public function __toString(){ return __reflect_export_param($this); }"` |
|     - | 2462 | `"}"` |
|     - | 2463 | `;` |
|     - | 2464 | `/*` |
|     - | 2465 | ` * Chunk 3: ReflectionProperty, ReflectionClassConstant.` |
|     - | 2466 | ` */` |
|     - | 2467 | `static const char zReflectLib3[] =` |
|     - | 2468 | `"class ReflectionProperty implements Reflector {"` |
|     - | 2469 | `" const IS_PUBLIC = 1;"` |
|     - | 2470 | `" const IS_PROTECTED = 2;"` |
|     - | 2471 | `" const IS_PRIVATE = 4;"` |
|     - | 2472 | `" const IS_STATIC = 16;"` |
|     - | 2473 | `" const IS_FINAL = 32;"` |
|     - | 2474 | `" const IS_ABSTRACT = 64;"` |
|     - | 2475 | `" const IS_READONLY = 128;"` |
|     - | 2476 | `" const IS_VIRTUAL = 512;"` |
|     - | 2477 | `" const IS_PROTECTED_SET = 2048;"` |
|     - | 2478 | `" const IS_PRIVATE_SET = 4096;"` |
|     - | 2479 | `" public $name;"` |
|     - | 2480 | `" public $class;"` |
|     - | 2481 | `" protected $__dynobj = null;"` |
|     - | 2482 | `" public function __construct($class, $property){"` |
|     - | 2483 | `"  $obj = null;"` |
|     - | 2484 | `"  if(is_object($class)){ $obj = $class; }"` |
|     - | 2485 | `"  else if(!is_string($class)){"` |
|     - | 2486 | `"   throw new TypeError('ReflectionProperty::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2487 | `"  }"` |
|     - | 2488 | `"  $ci = __reflect_class_info($class);"` |
|     - | 2489 | `"  if($ci === null){"` |
|     - | 2490 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2491 | `"  }"` |
|     - | 2492 | `"  $this->class = $ci['name'];"` |
|     - | 2493 | `"  if(isset($ci['props'][$property])){"` |
|     - | 2494 | `"   $this->name = $property;"` |
|     - | 2495 | `"   return;"` |
|     - | 2496 | `"  }"` |
|     - | 2497 | `"  if($obj !== null && (__reflect_prop_state($obj, $property) & 1)){"` |
|     - | 2498 | `"   $this->name = $property;"` |
|     - | 2499 | `"   $this->__dynobj = $obj;"` |
|     - | 2500 | `"   return;"` |
|     - | 2501 | `"  }"` |
|     - | 2502 | `"  throw new ReflectionException('Property '.$this->class.'::$'.$property.' does not exist');"` |
|     - | 2503 | `" }"` |
|     - | 2504 | `" protected function __rpmeta(){"` |
|     - | 2505 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2506 | `"  if(isset($ci['props'][$this->name])){ return $ci['props'][$this->name]; }"` |
|     - | 2507 | `"  return array('vis' => 1, 'static' => false, 'readonly' => false, 'hasdef' => false,"` |
|     - | 2508 | `"   'typed' => false, 'typetext' => null, 'decl' => $this->class, 'line' => 0, 'dyn' => true);"` |
|     - | 2509 | `" }"` |
|     - | 2510 | `" public function getName(){ return $this->name; }"` |
|     - | 2511 | `" public function getDeclaringClass(){"` |
|     - | 2512 | `"  $m = $this->__rpmeta();"` |
|     - | 2513 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2514 | `" }"` |
|     - | 2515 | `" public function getModifiers(){"` |
|     - | 2516 | `"  $m = $this->__rpmeta();"` |
|     - | 2517 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2518 | `"  if($m['static']){ $mod \|= 16; }"` |
|     - | 2519 | `"  if($m['readonly']){ $mod \|= 128; }"` |
|     - | 2520 | `"  return $mod;"` |
|     - | 2521 | `" }"` |
|     - | 2522 | `" public function isPublic(){ $m = $this->__rpmeta(); return $m['vis'] === 1; }"` |
|     - | 2523 | `" public function isProtected(){ $m = $this->__rpmeta(); return $m['vis'] === 2; }"` |
|     - | 2524 | `" public function isPrivate(){ $m = $this->__rpmeta(); return $m['vis'] === 3; }"` |
|     - | 2525 | `" public function isStatic(){ $m = $this->__rpmeta(); return $m['static']; }"` |
|     - | 2526 | `" public function isReadOnly(){ $m = $this->__rpmeta(); return $m['readonly']; }"` |
|     - | 2527 | `" public function isDefault(){ $m = $this->__rpmeta(); return !isset($m['dyn']); }"` |
|     - | 2528 | `" public function isDynamic(){ $m = $this->__rpmeta(); return isset($m['dyn']); }"` |
|     - | 2529 | `" public function isAbstract(){ return false; }"` |
|     - | 2530 | `" public function isFinal(){ return false; }"` |
|     - | 2531 | `" public function isVirtual(){ return false; }"` |
|     - | 2532 | `" public function isPrivateSet(){ return false; }"` |
|     - | 2533 | `" public function isProtectedSet(){ return false; }"` |
|     - | 2534 | `" public function hasHooks(){ return false; }"` |
|     - | 2535 | `" public function getHooks(){ return array(); }"` |
|     - | 2536 | `" public function hasHook($type){ return false; }"` |
|     - | 2537 | `" public function getHook($type){ return null; }"` |
|     - | 2538 | `" public function isLazy($object){ return false; }"` |
|     - | 2539 | `" public function setAccessible($accessible){ }"` |
|     - | 2540 | `" public function getValue($object = null){"` |
|     - | 2541 | `"  $m = $this->__rpmeta();"` |
|     - | 2542 | `"  if($m['static']){ return __reflect_static_value($this->class, $this->name); }"` |
|     - | 2543 | `"  if(!is_object($object)){"` |
|     - | 2544 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2545 | `"  }"` |
|     - | 2546 | `"  return __reflect_prop_read($object, $this->name);"` |
|     - | 2547 | `" }"` |
|     - | 2548 | `" public function setValue($objectOrValue = null, $value = null){"` |
|     - | 2549 | `"  $m = $this->__rpmeta();"` |
|     - | 2550 | `"  if($m['static']){"` |
|     - | 2551 | `"   if($value === null && $objectOrValue !== null && !is_object($objectOrValue)){"` |
|     - | 2552 | `"    __reflect_static_set($this->class, $this->name, $objectOrValue);"` |
|     - | 2553 | `"   }else{"` |
|     - | 2554 | `"    __reflect_static_set($this->class, $this->name, $value);"` |
|     - | 2555 | `"   }"` |
|     - | 2556 | `"   return;"` |
|     - | 2557 | `"  }"` |
|     - | 2558 | `"  __reflect_prop_write($objectOrValue, $this->name, $value);"` |
|     - | 2559 | `" }"` |
|     - | 2560 | `" public function getRawValue($object){ return $this->getValue($object); }"` |
|     - | 2561 | `" public function setRawValue($object, $value){ $this->setValue($object, $value); }"` |
|     - | 2562 | `" public function isInitialized($object = null){"` |
|     - | 2563 | `"  $m = $this->__rpmeta();"` |
|     - | 2564 | `"  if($m['static']){ return (__reflect_prop_state($this->class, $this->name) & 2) !== 0; }"` |
|     - | 2565 | `"  if(!is_object($object)){"` |
|     - | 2566 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2567 | `"  }"` |
|     - | 2568 | `"  return (__reflect_prop_state($object, $this->name) & 2) !== 0;"` |
|     - | 2569 | `" }"` |
|     - | 2570 | `" public function hasDefaultValue(){"` |
|     - | 2571 | `"  $m = $this->__rpmeta();"` |
|     - | 2572 | `"  if(isset($m['dyn'])){ return false; }"` |
|     - | 2573 | `"  if($m['hasdef']){ return true; }"` |
|     - | 2574 | `"  return !$m['typed'];"` |
|     - | 2575 | `" }"` |
|     - | 2576 | `" public function getDefaultValue(){"` |
|     - | 2577 | `"  $m = $this->__rpmeta();"` |
|     - | 2578 | `"  if(isset($m['dyn']) \|\| !$m['hasdef']){ return null; }"` |
|     - | 2579 | `"  return __reflect_prop_default($this->class, $this->name);"` |
|     - | 2580 | `" }"` |
|     - | 2581 | `" public function hasType(){ $m = $this->__rpmeta(); return $m['typed']; }"` |
|     - | 2582 | `" public function getType(){ $m = $this->__rpmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2583 | `" public function getSettableType(){ return $this->getType(); }"` |
|     - | 2584 | `" public function setRawValueWithoutLazyInitialization($object, $value){"` |
|     - | 2585 | `"  throw new Error('ReflectionProperty::setRawValueWithoutLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2586 | `" }"` |
|     - | 2587 | `" public function skipLazyInitialization($object){"` |
|     - | 2588 | `"  throw new Error('ReflectionProperty::skipLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2589 | `" }"` |
|     - | 2590 | `" public function getDocComment(){ $m = $this->__rpmeta(); return isset($m['doc']) ? $m['doc'] : false; }"` |
|     - | 2591 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2592 | `"  $m = $this->__rpmeta();"` |
|     - | 2593 | `"  if(!isset($m['attrs'])){ return array(); }"` |
|     - | 2594 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 8, $name, $flags);"` |
|     - | 2595 | `" }"` |
|     - | 2596 | `" public function __toString(){ return __reflect_export_prop($this); }"` |
|     - | 2597 | `"}"` |
|     - | 2598 | `"class ReflectionClassConstant implements Reflector {"` |
|     - | 2599 | `" const IS_PUBLIC = 1;"` |
|     - | 2600 | `" const IS_PROTECTED = 2;"` |
|     - | 2601 | `" const IS_PRIVATE = 4;"` |
|     - | 2602 | `" const IS_FINAL = 32;"` |
|     - | 2603 | `" public $name;"` |
|     - | 2604 | `" public $class;"` |
|     - | 2605 | `" public function __construct($class, $constant){"` |
|     - | 2606 | `"  if(!is_object($class) && !is_string($class)){"` |
|     - | 2607 | `"   throw new TypeError('ReflectionClassConstant::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2608 | `"  }"` |
|     - | 2609 | `"  $ci = __reflect_class_info($class);"` |
|     - | 2610 | `"  if($ci === null){"` |
|     - | 2611 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2612 | `"  }"` |
|     - | 2613 | `"  $this->class = $ci['name'];"` |
|     - | 2614 | `"  if(!isset($ci['consts'][$constant])){"` |
|     - | 2615 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' does not exist');"` |
|     - | 2616 | `"  }"` |
|     - | 2617 | `"  $this->name = $constant;"` |
|     - | 2618 | `" }"` |
|     - | 2619 | `" protected function __rcmeta(){"` |
|     - | 2620 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2621 | `"  return $ci['consts'][$this->name];"` |
|     - | 2622 | `" }"` |
|     - | 2623 | `" public function getName(){ return $this->name; }"` |
|     - | 2624 | `" public function getValue(){ return __reflect_const_value($this->class, $this->name); }"` |
|     - | 2625 | `" public function getDeclaringClass(){"` |
|     - | 2626 | `"  $m = $this->__rcmeta();"` |
|     - | 2627 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2628 | `" }"` |
|     - | 2629 | `" public function getModifiers(){"` |
|     - | 2630 | `"  $m = $this->__rcmeta();"` |
|     - | 2631 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2632 | `"  if($m['final']){ $mod \|= 32; }"` |
|     - | 2633 | `"  return $mod;"` |
|     - | 2634 | `" }"` |
|     - | 2635 | `" public function isPublic(){ $m = $this->__rcmeta(); return $m['vis'] === 1; }"` |
|     - | 2636 | `" public function isProtected(){ $m = $this->__rcmeta(); return $m['vis'] === 2; }"` |
|     - | 2637 | `" public function isPrivate(){ $m = $this->__rcmeta(); return $m['vis'] === 3; }"` |
|     - | 2638 | `" public function isFinal(){ $m = $this->__rcmeta(); return $m['final']; }"` |
|     - | 2639 | `" public function isEnumCase(){ $m = $this->__rcmeta(); return $m['enumcase']; }"` |
|     - | 2640 | `" public function isDeprecated(){ $m = $this->__rcmeta(); return __reflect_has_deprecated($m['attrs']); }"` |
|     - | 2641 | `" public function hasType(){ $m = $this->__rcmeta(); return $m['typed']; }"` |
|     - | 2642 | `" public function getType(){ $m = $this->__rcmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2643 | `" public function getDocComment(){ $m = $this->__rcmeta(); return $m['doc']; }"` |
|     - | 2644 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2645 | `"  $m = $this->__rcmeta();"` |
|     - | 2646 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 16, $name, $flags);"` |
|     - | 2647 | `" }"` |
|     - | 2648 | `" public function __toString(){ return __reflect_export_cconst($this); }"` |
|     - | 2649 | `"}"` |
|     - | 2650 | `;` |
|     - | 2651 | `/*` |
|     - | 2652 | ` * Chunk 4: the ReflectionType family, built from the engine's canonical` |
|     - | 2653 | ` * type text ("?int", "string\|float", "(A&B)\|C" — normalized at compile` |
|     - | 2654 | ` * time). __reflect_make_type is the internal factory; PHP itself never` |
|     - | 2655 | ` * lets user code construct these, so the public constructors here are a` |
|     - | 2656 | ` * recorded PHL-only surface.` |
|     - | 2657 | ` */` |
|     - | 2658 | `static const char zReflectLib4[] =` |
|     - | 2659 | `"abstract class ReflectionType implements Stringable {"` |
|     - | 2660 | `" protected $__text = '';"` |
|     - | 2661 | `" protected $__nullable = false;"` |
|     - | 2662 | `" public function allowsNull(){ return $this->__nullable; }"` |
|     - | 2663 | `" public function __toString(){ return $this->__text; }"` |
|     - | 2664 | `"}"` |
|     - | 2665 | `"class ReflectionNamedType extends ReflectionType {"` |
|     - | 2666 | `" protected $__tname = '';"` |
|     - | 2667 | `" public function __construct($name = '', $nullable = false, $text = null){"` |
|     - | 2668 | `"  $this->__tname = $name;"` |
|     - | 2669 | `"  $l = strtolower($name);"` |
|     - | 2670 | `"  $this->__nullable = $nullable \|\| $l === 'null' \|\| $l === 'mixed';"` |
|     - | 2671 | `"  $this->__text = $text === null ? $name : $text;"` |
|     - | 2672 | `" }"` |
|     - | 2673 | `" public function getName(){ return $this->__tname; }"` |
|     - | 2674 | `" public function isBuiltin(){"` |
|     - | 2675 | `"  $l = strtolower($this->__tname);"` |
|     - | 2676 | `"  return in_array($l, array('int','float','string','bool','array','object','mixed',"` |
|     - | 2677 | `"   'void','never','null','callable','iterable','true','false'), true);"` |
|     - | 2678 | `" }"` |
|     - | 2679 | `"}"` |
|     - | 2680 | `"class ReflectionUnionType extends ReflectionType {"` |
|     - | 2681 | `" protected $__types = array();"` |
|     - | 2682 | `" public function __construct($text = '', $nullable = false, $types = array()){"` |
|     - | 2683 | `"  $this->__text = $text;"` |
|     - | 2684 | `"  $this->__nullable = $nullable;"` |
|     - | 2685 | `"  $this->__types = $types;"` |
|     - | 2686 | `" }"` |
|     - | 2687 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2688 | `"}"` |
|     - | 2689 | `"class ReflectionIntersectionType extends ReflectionType {"` |
|     - | 2690 | `" protected $__types = array();"` |
|     - | 2691 | `" public function __construct($text = '', $types = array()){"` |
|     - | 2692 | `"  $this->__text = $text;"` |
|     - | 2693 | `"  $this->__nullable = false;"` |
|     - | 2694 | `"  $this->__types = $types;"` |
|     - | 2695 | `" }"` |
|     - | 2696 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2697 | `"}"` |
|     - | 2698 | `"function __reflect_make_atom($p){"` |
|     - | 2699 | `" $nullable = false;"` |
|     - | 2700 | `" if($p !== '' && $p[0] === '?'){ $nullable = true; $p = substr($p, 1); }"` |
|     - | 2701 | `" if($p !== '' && $p[0] === '('){ $p = substr($p, 1, strlen($p) - 2); }"` |
|     - | 2702 | `" if(strpos($p, '&') !== false){"` |
|     - | 2703 | `"  $subs = array();"` |
|     - | 2704 | `"  foreach(explode('&', $p) as $s){ $subs[] = new ReflectionNamedType($s, false, $s); }"` |
|     - | 2705 | `"  return new ReflectionIntersectionType($p, $subs);"` |
|     - | 2706 | `" }"` |
|     - | 2707 | `" return new ReflectionNamedType($p, $nullable, $nullable ? '?'.$p : $p);"` |
|     - | 2708 | `"}"` |
|     - | 2709 | `"function __reflect_make_type($text){"` |
|     - | 2710 | `" if($text === null \|\| $text === ''){ return null; }"` |
|     - | 2711 | `" $nullable = false;"` |
|     - | 2712 | `" $body = $text;"` |
|     - | 2713 | `" if($body[0] === '?'){ $nullable = true; $body = substr($body, 1); }"` |
|     - | 2714 | `" $parts = array();"` |
|     - | 2715 | `" $depth = 0;"` |
|     - | 2716 | `" $cur = '';"` |
|     - | 2717 | `" $n = strlen($body);"` |
|     - | 2718 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 2719 | `"  $ch = $body[$k];"` |
|     - | 2720 | `"  if($ch === '('){ $depth++; $cur .= $ch; }"` |
|     - | 2721 | `"  else if($ch === ')'){ $depth--; $cur .= $ch; }"` |
|     - | 2722 | `"  else if($ch === '\|' && $depth === 0){ $parts[] = $cur; $cur = ''; }"` |
|     - | 2723 | `"  else{ $cur .= $ch; }"` |
|     - | 2724 | `" }"` |
|     - | 2725 | `" $parts[] = $cur;"` |
|     - | 2726 | `" if(count($parts) > 1){"` |
|     - | 2727 | `"  $nonNull = array();"` |
|     - | 2728 | `"  $hasNull = false;"` |
|     - | 2729 | `"  foreach($parts as $p){"` |
|     - | 2730 | `"   if(strtolower($p) === 'null'){ $hasNull = true; }"` |
|     - | 2731 | `"   else{ $nonNull[] = $p; }"` |
|     - | 2732 | `"  }"` |
|     - | 2733 | `"  if($hasNull && count($nonNull) === 1 && strpos($nonNull[0], '&') === false){"` |
|     - | 2734 | `"   return new ReflectionNamedType($nonNull[0], true, '?'.$nonNull[0]);"` |
|     - | 2735 | `"  }"` |
|     - | 2736 | `"  $types = array();"` |
|     - | 2737 | `"  foreach($parts as $p){ $types[] = __reflect_make_atom($p); }"` |
|     - | 2738 | `"  return new ReflectionUnionType($body, $nullable \|\| $hasNull, $types);"` |
|     - | 2739 | `" }"` |
|     - | 2740 | `" if(strpos($body, '&') !== false){ return __reflect_make_atom($body); }"` |
|     - | 2741 | `" return __reflect_make_atom($nullable ? '?'.$body : $body);"` |
|     - | 2742 | `"}"` |
|     - | 2743 | `;` |
|     - | 2744 | `/*` |
|     - | 2745 | ` * Chunk 5: ReflectionGenerator, ReflectionFiber. Executing line/file and` |
|     - | 2746 | ` * traces need runtime line tracking the VM does not have (same gap as` |
|     - | 2747 | ` * debug_backtrace's line numbers) — those throw a loud Error, recorded in` |
|     - | 2748 | ` * the plan ledger.` |
|     - | 2749 | ` */` |
|     - | 2750 | `static const char zReflectLib5[] =` |
|     - | 2751 | `"class ReflectionGenerator {"` |
|     - | 2752 | `" protected $__gen;"` |
|     - | 2753 | `" public function __construct($generator){"` |
|     - | 2754 | `"  if(!($generator instanceof Generator)){"` |
|     - | 2755 | `"   throw new TypeError('ReflectionGenerator::__construct(): Argument #1 ($generator) must be of type Generator, '.get_debug_type($generator).' given');"` |
|     - | 2756 | `"  }"` |
|     - | 2757 | `"  $this->__gen = $generator;"` |
|     - | 2758 | `" }"` |
|     - | 2759 | `" protected function __rginfo(){ return __reflect_gen_info($this->__gen); }"` |
|     - | 2760 | `" public function getFunction(){"` |
|     - | 2761 | `"  $i = $this->__rginfo();"` |
|     - | 2762 | `"  if($i['kind'] === 'method'){ return new ReflectionMethod($i['class'], $i['name']); }"` |
|     - | 2763 | `"  return new ReflectionFunction($i['name']);"` |
|     - | 2764 | `" }"` |
|     - | 2765 | `" public function getThis(){ $i = $this->__rginfo(); return isset($i['this']) ? $i['this'] : null; }"` |
|     - | 2766 | `" public function getExecutingGenerator(){ return __reflect_gen_exec($this->__gen); }"` |
|     - | 2767 | `" public function isClosed(){ $i = $this->__rginfo(); return $i['closed']; }"` |
|     - | 2768 | `" public function getExecutingLine(){"` |
|     - | 2769 | `"  throw new Error('ReflectionGenerator::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2770 | `" }"` |
|     - | 2771 | `" public function getExecutingFile(){"` |
|     - | 2772 | `"  throw new Error('ReflectionGenerator::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2773 | `" }"` |
|     - | 2774 | `" public function getTrace($options = 1){"` |
|     - | 2775 | `"  throw new Error('ReflectionGenerator::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2776 | `" }"` |
|     - | 2777 | `"}"` |
|     - | 2778 | `"class ReflectionFiber {"` |
|     - | 2779 | `" protected $__fiber;"` |
|     - | 2780 | `" public function __construct($fiber){"` |
|     - | 2781 | `"  if(!($fiber instanceof Fiber)){"` |
|     - | 2782 | `"   throw new TypeError('ReflectionFiber::__construct(): Argument #1 ($fiber) must be of type Fiber, '.get_debug_type($fiber).' given');"` |
|     - | 2783 | `"  }"` |
|     - | 2784 | `"  $this->__fiber = $fiber;"` |
|     - | 2785 | `" }"` |
|     - | 2786 | `" public function getFiber(){ return $this->__fiber; }"` |
|     - | 2787 | `" public function getCallable(){ return __reflect_prop_read($this->__fiber, '__callable'); }"` |
|     - | 2788 | `" public function getExecutingLine(){"` |
|     - | 2789 | `"  throw new Error('ReflectionFiber::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2790 | `" }"` |
|     - | 2791 | `" public function getExecutingFile(){"` |
|     - | 2792 | `"  throw new Error('ReflectionFiber::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2793 | `" }"` |
|     - | 2794 | `" public function getTrace($options = 1){"` |
|     - | 2795 | `"  throw new Error('ReflectionFiber::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2796 | `" }"` |
|     - | 2797 | `"}"` |
|     - | 2798 | `;` |
|     - | 2799 | `/*` |
|     - | 2800 | ` * Chunk 6: the long tail — ReflectionConstant (PHP 8.5), the synthetic` |
|     - | 2801 | ` * "Core" ReflectionExtension, ReflectionZendExtension (throws: no Zend` |
|     - | 2802 | ` * extensions exist), the ReflectionEnum family (throws: enums are not a` |
|     - | 2803 | ` * PHL language feature yet), and ReflectionReference.` |
|     - | 2804 | ` */` |
|     - | 2805 | `static const char zReflectLib6[] =` |
|     - | 2806 | `"class ReflectionConstant implements Reflector {"` |
|     - | 2807 | `" public $name;"` |
|     - | 2808 | `" public function __construct($name){"` |
|     - | 2809 | `"  if(!is_string($name)){"` |
|     - | 2810 | `"   throw new TypeError('ReflectionConstant::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2811 | `"  }"` |
|     - | 2812 | `"  $i = __reflect_const_info($name);"` |
|     - | 2813 | `"  if($i === null){"` |
|     - | 2814 | `"   throw new ReflectionException('Constant \"'.$name.'\" does not exist');"` |
|     - | 2815 | `"  }"` |
|     - | 2816 | `"  $this->name = $name;"` |
|     - | 2817 | `" }"` |
|     - | 2818 | `" public function getName(){ return $this->name; }"` |
|     - | 2819 | `" public function getNamespaceName(){"` |
|     - | 2820 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2821 | `"  if($p === false){ return ''; }"` |
|     - | 2822 | `"  return substr($this->name,0,$p);"` |
|     - | 2823 | `" }"` |
|     - | 2824 | `" public function getShortName(){"` |
|     - | 2825 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2826 | `"  if($p === false){ return $this->name; }"` |
|     - | 2827 | `"  return substr($this->name,$p+1);"` |
|     - | 2828 | `" }"` |
|     - | 2829 | `" public function getValue(){"` |
|     - | 2830 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2831 | `"  return $i['value'];"` |
|     - | 2832 | `" }"` |
|     - | 2833 | `" public function isDeprecated(){ return false; }"` |
|     - | 2834 | `" public function getFileName(){"` |
|     - | 2835 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2836 | `"  return $i['file'];"` |
|     - | 2837 | `" }"` |
|     - | 2838 | `" public function getExtension(){"` |
|     - | 2839 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2840 | `"  return $i['internal'] ? new ReflectionExtension('Core') : null;"` |
|     - | 2841 | `" }"` |
|     - | 2842 | `" public function getExtensionName(){"` |
|     - | 2843 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2844 | `"  return $i['internal'] ? 'Core' : false;"` |
|     - | 2845 | `" }"` |
|     - | 2846 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2847 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2848 | `"  if($i === null){ return array(); }"` |
|     - | 2849 | `"  return __reflect_build_attrs($i['attrs'], array('const', $this->name, null, 0), 64, $name, $flags);"` |
|     - | 2850 | `" }"` |
|     - | 2851 | `" public function __toString(){"` |
|     - | 2852 | `"  return 'Constant [ '.$this->name.' ]'.\"\\n\";"` |
|     - | 2853 | `" }"` |
|     - | 2854 | `"}"` |
|     - | 2855 | `"class ReflectionExtension implements Reflector {"` |
|     - | 2856 | `" public $name;"` |
|     - | 2857 | `" public function __construct($name){"` |
|     - | 2858 | `"  if(!is_string($name)){"` |
|     - | 2859 | `"   throw new TypeError('ReflectionExtension::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2860 | `"  }"` |
|     - | 2861 | `"  if(strtolower($name) !== 'core'){"` |
|     - | 2862 | `"   throw new ReflectionException('Extension \"'.$name.'\" does not exist');"` |
|     - | 2863 | `"  }"` |
|     - | 2864 | `"  $this->name = 'Core';"` |
|     - | 2865 | `" }"` |
|     - | 2866 | `" public function getName(){ return $this->name; }"` |
|     - | 2867 | `" public function getVersion(){ return phpversion(); }"` |
|     - | 2868 | `" public function getFunctions(){ return array(); }"` |
|     - | 2869 | `" public function getClasses(){ return array(); }"` |
|     - | 2870 | `" public function getClassNames(){ return array(); }"` |
|     - | 2871 | `" public function getConstants(){ return array(); }"` |
|     - | 2872 | `" public function getINIEntries(){ return array(); }"` |
|     - | 2873 | `" public function getDependencies(){ return array(); }"` |
|     - | 2874 | `" public function isPersistent(){ return true; }"` |
|     - | 2875 | `" public function isTemporary(){ return false; }"` |
|     - | 2876 | `" public function info(){ }"` |
|     - | 2877 | `" public function __toString(){"` |
|     - | 2878 | `"  return 'Extension [ extension #1 '.$this->name.' ]'.\"\\n\";"` |
|     - | 2879 | `" }"` |
|     - | 2880 | `"}"` |
|     - | 2881 | `"class ReflectionZendExtension implements Reflector {"` |
|     - | 2882 | `" public $name;"` |
|     - | 2883 | `" public function __construct($name){"` |
|     - | 2884 | `"  throw new ReflectionException('Zend Extension \"'.$name.'\" does not exist');"` |
|     - | 2885 | `" }"` |
|     - | 2886 | `" public function getName(){ return $this->name; }"` |
|     - | 2887 | `" public function __toString(){ return ''; }"` |
|     - | 2888 | `"}"` |
|     - | 2889 | `"class ReflectionEnum extends ReflectionClass {"` |
|     - | 2890 | `" public function __construct($objectOrClass){"` |
|     - | 2891 | `"  $info = __reflect_class_info($objectOrClass);"` |
|     - | 2892 | `"  if($info === null){"` |
|     - | 2893 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 2894 | `"  }"` |
|     - | 2895 | `"  if(!$info['enum']){"` |
|     - | 2896 | `"   throw new ReflectionException('Class \"'.$info['name'].'\" is not an enum');"` |
|     - | 2897 | `"  }"` |
|     - | 2898 | `"  parent::__construct($objectOrClass);"` |
|     - | 2899 | `" }"` |
|     - | 2900 | `" public function hasCase($name){"` |
|     - | 2901 | `"  $i = $this->__rinfo();"` |
|     - | 2902 | `"  return in_array($name, $i['cases'], true);"` |
|     - | 2903 | `" }"` |
|     - | 2904 | `" public function getCase($name){"` |
|     - | 2905 | `"  if(!$this->hasCase($name)){"` |
|     - | 2906 | `"   throw new ReflectionException('Case '.$this->name.'::'.$name.' does not exist');"` |
|     - | 2907 | `"  }"` |
|     - | 2908 | `"  if($this->isBacked()){ return new ReflectionEnumBackedCase($this->name, $name); }"` |
|     - | 2909 | `"  return new ReflectionEnumUnitCase($this->name, $name);"` |
|     - | 2910 | `" }"` |
|     - | 2911 | `" public function getCases(){"` |
|     - | 2912 | `"  $i = $this->__rinfo();"` |
|     - | 2913 | `"  $out = array();"` |
|     - | 2914 | `"  foreach($i['cases'] as $c){"` |
|     - | 2915 | `"   $out[] = $this->isBacked()"` |
|     - | 2916 | `"    ? new ReflectionEnumBackedCase($this->name, $c)"` |
|     - | 2917 | `"    : new ReflectionEnumUnitCase($this->name, $c);"` |
|     - | 2918 | `"  }"` |
|     - | 2919 | `"  return $out;"` |
|     - | 2920 | `" }"` |
|     - | 2921 | `" public function isBacked(){ $i = $this->__rinfo(); return $i['enumbacking'] !== ''; }"` |
|     - | 2922 | `" public function getBackingType(){"` |
|     - | 2923 | `"  $i = $this->__rinfo();"` |
|     - | 2924 | `"  if($i['enumbacking'] === ''){ return null; }"` |
|     - | 2925 | `"  return __reflect_make_type($i['enumbacking']);"` |
|     - | 2926 | `" }"` |
|     - | 2927 | `"}"` |
|     - | 2928 | `"class ReflectionEnumUnitCase extends ReflectionClassConstant {"` |
|     - | 2929 | `" public function __construct($class, $constant){"` |
|     - | 2930 | `"  parent::__construct($class, $constant);"` |
|     - | 2931 | `"  $ci = __reflect_class_info($class);"` |
|     - | 2932 | `"  if(!$ci['enum']){"` |
|     - | 2933 | `"   throw new ReflectionException('Class \"'.$this->class.'\" is not an enum');"` |
|     - | 2934 | `"  }"` |
|     - | 2935 | `"  $m = $this->__rcmeta();"` |
|     - | 2936 | `"  if(!$m['enumcase']){"` |
|     - | 2937 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' is not a case');"` |
|     - | 2938 | `"  }"` |
|     - | 2939 | `" }"` |
|     - | 2940 | `" public function getEnum(){ return new ReflectionEnum($this->class); }"` |
|     - | 2941 | `"}"` |
|     - | 2942 | `"class ReflectionEnumBackedCase extends ReflectionEnumUnitCase {"` |
|     - | 2943 | `" public function getBackingValue(){ return $this->getValue()->value; }"` |
|     - | 2944 | `"}"` |
|     - | 2945 | `"final class ReflectionReference {"` |
|     - | 2946 | `" protected $__id = '';"` |
|     - | 2947 | `" public function __construct(){"` |
|     - | 2948 | `"  throw new Error('Call to private ReflectionReference::__construct() from global scope');"` |
|     - | 2949 | `" }"` |
|     - | 2950 | `" public static function fromArrayElement($array, $key){"` |
|     - | 2951 | `"  if(!is_array($array)){"` |
|     - | 2952 | `"   throw new TypeError('ReflectionReference::fromArrayElement(): Argument #1 ($array) must be of type array, '.get_debug_type($array).' given');"` |
|     - | 2953 | `"  }"` |
|     - | 2954 | `"  $id = __reflect_ref_id($array, $key);"` |
|     - | 2955 | `"  if($id === null){ return null; }"` |
|     - | 2956 | `"  $r = __reflect_new_no_ctor('ReflectionReference');"` |
|     - | 2957 | `"  $r->__setId('phlref'.$id);"` |
|     - | 2958 | `"  return $r;"` |
|     - | 2959 | `" }"` |
|     - | 2960 | `" public function __setId($id){ $this->__id = $id; }"` |
|     - | 2961 | `" public function getId(){ return $this->__id; }"` |
|     - | 2962 | `"}"` |
|     - | 2963 | `;` |
|     - | 2964 | `/*` |
|     - | 2965 | ` * Chunk 7: ReflectionAttribute and the shared getAttributes() builder.` |
|     - | 2966 | ` * The spec array rides as [kind, target, member, paramIdx]; argument` |
|     - | 2967 | ` * values evaluate lazily through __reflect_attr_args (PHP semantics).` |
|     - | 2968 | ` */` |
|     - | 2969 | `static const char zReflectLib7[] =` |
|     - | 2970 | `"function __reflect_has_deprecated($meta){"` |
|     - | 2971 | `" foreach($meta as $a){"` |
|     - | 2972 | `"  if(strtolower($a['name']) === 'deprecated'){ return true; }"` |
|     - | 2973 | `" }"` |
|     - | 2974 | `" return false;"` |
|     - | 2975 | `"}"` |
|     - | 2976 | `"function __reflect_target_names($mask){"` |
|     - | 2977 | `" $parts = array();"` |
|     - | 2978 | `" foreach(array('class' => 1, 'function' => 2, 'method' => 4, 'property' => 8,"` |
|     - | 2979 | `"  'class constant' => 16, 'parameter' => 32, 'constant' => 64) as $nm => $bit){"` |
|     - | 2980 | `"  if($mask & $bit){ $parts[] = $nm; }"` |
|     - | 2981 | `" }"` |
|     - | 2982 | `" return implode(', ', $parts);"` |
|     - | 2983 | `"}"` |
|     - | 2984 | `"function __reflect_build_attrs($meta, $spec, $target, $name, $flags){"` |
|     - | 2985 | `" $out = array();"` |
|     - | 2986 | `" $counts = array();"` |
|     - | 2987 | `" foreach($meta as $a){"` |
|     - | 2988 | `"  $k = strtolower($a['name']);"` |
|     - | 2989 | `"  $counts[$k] = isset($counts[$k]) ? $counts[$k] + 1 : 1;"` |
|     - | 2990 | `" }"` |
|     - | 2991 | `" $idx = 0;"` |
|     - | 2992 | `" foreach($meta as $a){"` |
|     - | 2993 | `"  $keep = true;"` |
|     - | 2994 | `"  if($name !== null){"` |
|     - | 2995 | `"   $keep = strtolower($a['name']) === strtolower($name);"` |
|     - | 2996 | `"   if(!$keep && ($flags & 2)){"` |
|     - | 2997 | `"    $keep = is_subclass_of($a['name'], $name);"` |
|     - | 2998 | `"   }"` |
|     - | 2999 | `"  }"` |
|     - | 3000 | `"  if($keep){"` |
|     - | 3001 | `"   $r = __reflect_new_no_ctor('ReflectionAttribute');"` |
|     - | 3002 | `"   $r->__init($a['name'], $spec, $idx, $target, $counts[strtolower($a['name'])] > 1);"` |
|     - | 3003 | `"   $out[] = $r;"` |
|     - | 3004 | `"  }"` |
|     - | 3005 | `"  $idx++;"` |
|     - | 3006 | `" }"` |
|     - | 3007 | `" return $out;"` |
|     - | 3008 | `"}"` |
|     - | 3009 | `"final class ReflectionAttribute {"` |
|     - | 3010 | `" const IS_INSTANCEOF = 2;"` |
|     - | 3011 | `" protected $__name = '';"` |
|     - | 3012 | `" protected $__spec = null;"` |
|     - | 3013 | `" protected $__idx = 0;"` |
|     - | 3014 | `" protected $__target = 0;"` |
|     - | 3015 | `" protected $__rep = false;"` |
|     - | 3016 | `" public function __construct(){"` |
|     - | 3017 | `"  throw new Error('Call to private ReflectionAttribute::__construct() from global scope');"` |
|     - | 3018 | `" }"` |
|     - | 3019 | `" public function __init($name, $spec, $idx, $target, $rep){"` |
|     - | 3020 | `"  $this->__name = $name;"` |
|     - | 3021 | `"  $this->__spec = $spec;"` |
|     - | 3022 | `"  $this->__idx = $idx;"` |
|     - | 3023 | `"  $this->__target = $target;"` |
|     - | 3024 | `"  $this->__rep = $rep;"` |
|     - | 3025 | `" }"` |
|     - | 3026 | `" public function getName(){ return $this->__name; }"` |
|     - | 3027 | `" public function getTarget(){ return $this->__target; }"` |
|     - | 3028 | `" public function isRepeated(){ return $this->__rep; }"` |
|     - | 3029 | `" public function getArguments(){"` |
|     - | 3030 | `"  $a = __reflect_attr_args($this->__spec[0], $this->__spec[1], $this->__spec[2], $this->__spec[3], $this->__idx);"` |
|     - | 3031 | `"  return $a === null ? array() : $a;"` |
|     - | 3032 | `" }"` |
|     - | 3033 | `" public function newInstance(){"` |
|     - | 3034 | `"  $name = $this->__name;"` |
|     - | 3035 | `"  $ci = __reflect_class_info($name);"` |
|     - | 3036 | `"  if($ci === null){"` |
|     - | 3037 | `"   throw new Error('Attribute class \"'.$name.'\" not found');"` |
|     - | 3038 | `"  }"` |
|     - | 3039 | `"  $name = $ci['name'];"` |
|     - | 3040 | `"  $decl = null;"` |
|     - | 3041 | `"  $didx = 0;"` |
|     - | 3042 | `"  foreach($ci['attrs'] as $a){"` |
|     - | 3043 | `"   if(strtolower($a['name']) === 'attribute'){ $decl = $didx; break; }"` |
|     - | 3044 | `"   $didx++;"` |
|     - | 3045 | `"  }"` |
|     - | 3046 | `"  if($decl === null){"` |
|     - | 3047 | `"   throw new Error('Attempting to use non-attribute class \"'.$name.'\" as attribute');"` |
|     - | 3048 | `"  }"` |
|     - | 3049 | `"  $dargs = __reflect_attr_args('class', $name, null, 0, $decl);"` |
|     - | 3050 | `"  $flags = 127;"` |
|     - | 3051 | `"  if(is_array($dargs)){"` |
|     - | 3052 | `"   if(isset($dargs[0])){ $flags = $dargs[0]; }"` |
|     - | 3053 | `"   else if(isset($dargs['flags'])){ $flags = $dargs['flags']; }"` |
|     - | 3054 | `"  }"` |
|     - | 3055 | `"  if(($flags & $this->__target) === 0){"` |
|     - | 3056 | `"   $tnames = array(1 => 'class', 2 => 'function', 4 => 'method', 8 => 'property',"` |
|     - | 3057 | `"    16 => 'class constant', 32 => 'parameter', 64 => 'constant');"` |
|     - | 3058 | `"   throw new Error('Attribute \"'.$name.'\" cannot target '.$tnames[$this->__target]"` |
|     - | 3059 | `"    .' (allowed targets: '.__reflect_target_names($flags).')');"` |
|     - | 3060 | `"  }"` |
|     - | 3061 | `"  if($this->__rep && ($flags & 128) === 0){"` |
|     - | 3062 | `"   throw new Error('Attribute \"'.$name.'\" must not be repeated');"` |
|     - | 3063 | `"  }"` |
|     - | 3064 | `"  return __reflect_new_instance($name, $this->getArguments());"` |
|     - | 3065 | `" }"` |
|     - | 3066 | `" public function __toString(){"` |
|     - | 3067 | `"  return 'Attribute [ '.$this->__name.' ]';"` |
|     - | 3068 | `" }"` |
|     - | 3069 | `"}"` |
|     - | 3070 | `;` |
|     - | 3071 | `/*` |
|     - | 3072 | ` * Chunk 8: signature-table support. Internal (C builtin) functions carry a` |
|     - | 3073 | ` * PHP-style parameter-list string; these helpers parse it into the same` |
|     - | 3074 | ` * param-meta shape user functions get, so ReflectionFunction and` |
|     - | 3075 | ` * ReflectionParameter work uniformly over builtins.` |
|     - | 3076 | ` */` |
|     - | 3077 | `static const char zReflectLib8[] =` |
|     - | 3078 | `"function __reflect_sig_split($sig){"` |
|     - | 3079 | `" $parts = array();"` |
|     - | 3080 | `" $cur = '';"` |
|     - | 3081 | `" $q = false;"` |
|     - | 3082 | `" $n = strlen($sig);"` |
|     - | 3083 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3084 | `"  $ch = $sig[$k];"` |
|     - | 3085 | `"  if($q){"` |
|     - | 3086 | `"   $cur .= $ch;"` |
|     - | 3087 | `"   if($ch === chr(92) && $k + 1 < $n){ $cur .= $sig[$k+1]; $k++; }"` |
|     - | 3088 | `"   else if($ch === chr(39)){ $q = false; }"` |
|     - | 3089 | `"  }else if($ch === chr(39)){ $q = true; $cur .= $ch; }"` |
|     - | 3090 | `"  else if($ch === ',' ){ $parts[] = trim($cur); $cur = ''; }"` |
|     - | 3091 | `"  else{ $cur .= $ch; }"` |
|     - | 3092 | `" }"` |
|     - | 3093 | `" if(trim($cur) !== ''){ $parts[] = trim($cur); }"` |
|     - | 3094 | `" return $parts;"` |
|     - | 3095 | `"}"` |
|     - | 3096 | `"function __reflect_sig_scalar($t){"` |
|     - | 3097 | `" if($t === '?'){ return array(false, null); }"` |
|     - | 3098 | `" if($t === 'NULL' \|\| $t === 'null'){ return array(true, null); }"` |
|     - | 3099 | `" if($t === 'true'){ return array(true, true); }"` |
|     - | 3100 | `" if($t === 'false'){ return array(true, false); }"` |
|     - | 3101 | `" if(is_numeric($t)){"` |
|     - | 3102 | `"  if(strpos($t, '.') === false && stripos($t, 'e') === false && strpos($t, 'x') === false){"` |
|     - | 3103 | `"   return array(true, (int)$t);"` |
|     - | 3104 | `"  }"` |
|     - | 3105 | `"  return array(true, (float)$t);"` |
|     - | 3106 | `" }"` |
|     - | 3107 | `" if(strlen($t) >= 2 && $t[0] === chr(39) && $t[strlen($t)-1] === chr(39)){"` |
|     - | 3108 | `"  $body = substr($t, 1, strlen($t) - 2);"` |
|     - | 3109 | `"  return array(true, strtr($body, array(chr(92).chr(39) => chr(39), chr(92).chr(92) => chr(92))));"` |
|     - | 3110 | `" }"` |
|     - | 3111 | `" return array(false, null);"` |
|     - | 3112 | `"}"` |
|     - | 3113 | `"function __reflect_parse_sig($sig){"` |
|     - | 3114 | `" $params = array();"` |
|     - | 3115 | `" $pos = 0;"` |
|     - | 3116 | `" foreach(__reflect_sig_split($sig) as $part){"` |
|     - | 3117 | `"  $deftext = null;"` |
|     - | 3118 | `"  $q = false;"` |
|     - | 3119 | `"  $n = strlen($part);"` |
|     - | 3120 | `"  for($k = 0; $k < $n; $k++){"` |
|     - | 3121 | `"   $ch = $part[$k];"` |
|     - | 3122 | `"   if($q){"` |
|     - | 3123 | `"    if($ch === chr(92)){ $k++; }"` |
|     - | 3124 | `"    else if($ch === chr(39)){ $q = false; }"` |
|     - | 3125 | `"   }else if($ch === chr(39)){ $q = true; }"` |
|     - | 3126 | `"   else if($ch === '=' ){"` |
|     - | 3127 | `"    $deftext = trim(substr($part, $k + 1));"` |
|     - | 3128 | `"    $part = trim(substr($part, 0, $k));"` |
|     - | 3129 | `"    break;"` |
|     - | 3130 | `"   }"` |
|     - | 3131 | `"  }"` |
|     - | 3132 | `"  $variadic = strpos($part, '...') !== false;"` |
|     - | 3133 | `"  $byref = strpos($part, '&') !== false;"` |
|     - | 3134 | `"  $d = strpos($part, '$');"` |
|     - | 3135 | `"  $name = $d === false ? $part : substr($part, $d + 1);"` |
|     - | 3136 | `"  $typetext = null;"` |
|     - | 3137 | `"  $sp = strpos($part, ' ');"` |
|     - | 3138 | `"  if($sp !== false && $d !== false && $sp < $d){ $typetext = substr($part, 0, $sp); }"` |
|     - | 3139 | `"  $nullable = $typetext !== null && ($typetext[0] === '?' \|\| stripos($typetext, 'null') !== false);"` |
|     - | 3140 | `"  $params[] = array('name' => $name, 'pos' => $pos, 'byref' => $byref,"` |
|     - | 3141 | `"   'variadic' => $variadic, 'hasdef' => $deftext !== null, 'nullable' => $nullable,"` |
|     - | 3142 | `"   'promoted' => false, 'typetext' => $typetext, 'attrs' => array(), 'deftext' => $deftext);"` |
|     - | 3143 | `"  $pos++;"` |
|     - | 3144 | `" }"` |
|     - | 3145 | `" return $params;"` |
|     - | 3146 | `"}"` |
|     - | 3147 | `"function __reflect_sig_fixup($i){"` |
|     - | 3148 | `" if($i === null){ return $i; }"` |
|     - | 3149 | `" if(isset($i['ret2'])){ $i['rettext'] = $i['ret2']; }"` |
|     - | 3150 | `" if(!isset($i['sig']) \|\| $i['sig'] === ''){ return $i; }"` |
|     - | 3151 | `" $i['params'] = __reflect_parse_sig($i['sig']);"` |
|     - | 3152 | `" $i['minarg'] = -1;"` |
|     - | 3153 | `" $v = false;"` |
|     - | 3154 | `" foreach($i['params'] as $p){ if($p['variadic']){ $v = true; } }"` |
|     - | 3155 | `" $i['variadic'] = $v;"` |
|     - | 3156 | `" return $i;"` |
|     - | 3157 | `"}"` |
|     - | 3158 | `;` |
|     - | 3159 | `/*` |
|     - | 3160 | ` * Chunk 9: PHP's Reflection export format (__toString on every Reflector).` |
|     - | 3161 | ` * Built entirely from the public reflection API of the target objects.` |
|     - | 3162 | ` */` |
|     - | 3163 | `static const char zReflectLib9[] =` |
|     - | 3164 | `"function __reflect_export_value($v){"` |
|     - | 3165 | `" if($v === null){ return 'NULL'; }"` |
|     - | 3166 | `" if($v === true){ return 'true'; }"` |
|     - | 3167 | `" if($v === false){ return 'false'; }"` |
|     - | 3168 | `" if(is_string($v)){ return chr(39).$v.chr(39); }"` |
|     - | 3169 | `" if(is_array($v)){"` |
|     - | 3170 | `"  $parts = array();"` |
|     - | 3171 | `"  $isList = true;"` |
|     - | 3172 | `"  $next = 0;"` |
|     - | 3173 | `"  foreach($v as $k => $x){"` |
|     - | 3174 | `"   if($k !== $next){ $isList = false; break; }"` |
|     - | 3175 | `"   $next++;"` |
|     - | 3176 | `"  }"` |
|     - | 3177 | `"  foreach($v as $k => $x){"` |
|     - | 3178 | `"   $parts[] = $isList ? __reflect_export_value($x)"` |
|     - | 3179 | `"    : (__reflect_export_value($k).' => '.__reflect_export_value($x));"` |
|     - | 3180 | `"  }"` |
|     - | 3181 | `"  return '['.implode(', ', $parts).']';"` |
|     - | 3182 | `" }"` |
|     - | 3183 | `" return (string)$v;"` |
|     - | 3184 | `"}"` |
|     - | 3185 | `"function __reflect_export_param($p){"` |
|     - | 3186 | `" $s = 'Parameter #'.$p->getPosition().' [ <'.($p->isOptional() ? 'optional' : 'required').'> ';"` |
|     - | 3187 | `" $t = $p->getType();"` |
|     - | 3188 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3189 | `" if($p->isPassedByReference()){ $s .= '&'; }"` |
|     - | 3190 | `" if($p->isVariadic()){ $s .= '...'; }"` |
|     - | 3191 | `" $s .= '$'.$p->getName();"` |
|     - | 3192 | `" if($p->isDefaultValueAvailable()){"` |
|     - | 3193 | `"  try{ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3194 | `"  catch(ReflectionException $e){ $s .= ' = <default>'; }"` |
|     - | 3195 | `" }"` |
|     - | 3196 | `" return $s.' ]';"` |
|     - | 3197 | `"}"` |
|     - | 3198 | `"function __reflect_export_prop($p){"` |
|     - | 3199 | `" $s = 'Property [ ';"` |
|     - | 3200 | `" $s .= $p->isPrivate() ? 'private ' : ($p->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3201 | `" if($p->isStatic()){ $s .= 'static '; }"` |
|     - | 3202 | `" if($p->isReadOnly()){ $s .= 'readonly '; }"` |
|     - | 3203 | `" $t = $p->getType();"` |
|     - | 3204 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3205 | `" $s .= '$'.$p->getName();"` |
|     - | 3206 | `" if($p->hasDefaultValue()){ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3207 | `" return $s.' ]'.chr(10);"` |
|     - | 3208 | `"}"` |
|     - | 3209 | `"function __reflect_export_cconst($c){"` |
|     - | 3210 | `" $v = $c->getValue();"` |
|     - | 3211 | `" if(is_int($v)){ $t = 'int'; }"` |
|     - | 3212 | `" else if(is_string($v)){ $t = 'string'; }"` |
|     - | 3213 | `" else if(is_float($v)){ $t = 'float'; }"` |
|     - | 3214 | `" else if(is_bool($v)){ $t = 'bool'; }"` |
|     - | 3215 | `" else if(is_array($v)){ $t = 'array'; }"` |
|     - | 3216 | `" else{ $t = 'null'; }"` |
|     - | 3217 | `" $vs = is_array($v) ? 'Array' : (is_bool($v) ? ($v ? '1' : '') : (string)$v);"` |
|     - | 3218 | `" $vis = $c->isPrivate() ? 'private' : ($c->isProtected() ? 'protected' : 'public');"` |
|     - | 3219 | `" return 'Constant [ '.$vis.' '.$t.' '.$c->name.' ] { '.$vs.' }'.chr(10);"` |
|     - | 3220 | `"}"` |
|     - | 3221 | `"function __reflect_export_fnabs($r, $indent){"` |
|     - | 3222 | `" $tags = $r->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3223 | `" if($r instanceof ReflectionMethod){"` |
|     - | 3224 | `"  if($r->isConstructor()){ $tags .= ', ctor'; }"` |
|     - | 3225 | `"  else if($r->isDestructor()){ $tags .= ', dtor'; }"` |
|     - | 3226 | `"  $decl = $r->getDeclaringClass()->name;"` |
|     - | 3227 | `"  if(strtolower($decl) !== strtolower($r->class)){ $tags .= ', inherits '.$decl; }"` |
|     - | 3228 | `"  else if($r->hasPrototype()){ $tags .= ', prototype '.$r->getPrototype()->class; }"` |
|     - | 3229 | `"  $head = 'Method [ <'.$tags.'> ';"` |
|     - | 3230 | `"  if($r->isAbstract()){ $head .= 'abstract '; }"` |
|     - | 3231 | `"  if($r->isFinal()){ $head .= 'final '; }"` |
|     - | 3232 | `"  if($r->isStatic()){ $head .= 'static '; }"` |
|     - | 3233 | `"  $head .= $r->isPrivate() ? 'private ' : ($r->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3234 | `"  $head .= 'method '.$r->name.' ]';"` |
|     - | 3235 | `" }else{"` |
|     - | 3236 | `"  $kind = $r->isClosure() ? 'Closure' : 'Function';"` |
|     - | 3237 | `"  $head = $kind.' [ <'.$tags.'> function '.$r->name.' ]';"` |
|     - | 3238 | `" }"` |
|     - | 3239 | `" $s = $head.' {'.chr(10);"` |
|     - | 3240 | `" if(!$r->isInternal()){"` |
|     - | 3241 | `"  $s .= '  @@ '.$r->getFileName().' '.$r->getStartLine().' - '.$r->getEndLine().chr(10);"` |
|     - | 3242 | `" }"` |
|     - | 3243 | `" $ps = $r->getParameters();"` |
|     - | 3244 | `" $ret = $r->getReturnType();"` |
|     - | 3245 | `" if(count($ps) > 0 \|\| $ret !== null){"` |
|     - | 3246 | `"  $s .= chr(10).'  - Parameters ['.count($ps).'] {'.chr(10);"` |
|     - | 3247 | `"  foreach($ps as $p){ $s .= '    '.__reflect_export_param($p).chr(10); }"` |
|     - | 3248 | `"  $s .= '  }'.chr(10);"` |
|     - | 3249 | `" }"` |
|     - | 3250 | `" if($ret !== null){ $s .= '  - Return [ '.(string)$ret.' ]'.chr(10); }"` |
|     - | 3251 | `" $s .= '}'.chr(10);"` |
|     - | 3252 | `" if($indent === ''){ return $s; }"` |
|     - | 3253 | `" $lines = explode(chr(10), $s);"` |
|     - | 3254 | `" $out = '';"` |
|     - | 3255 | `" $n = count($lines);"` |
|     - | 3256 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3257 | `"  if($lines[$k] === '' && $k === $n - 1){ break; }"` |
|     - | 3258 | `"  $out .= ($lines[$k] === '' ? '' : $indent.$lines[$k]).chr(10);"` |
|     - | 3259 | `" }"` |
|     - | 3260 | `" return $out;"` |
|     - | 3261 | `"}"` |
|     - | 3262 | `"function __reflect_export_class($rc){"` |
|     - | 3263 | `" $tags = $rc->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3264 | `" if($rc->isInterface()){"` |
|     - | 3265 | `"  $head = 'Interface [ <'.$tags.'> interface '.$rc->name.' ]';"` |
|     - | 3266 | `" }else{"` |
|     - | 3267 | `"  $mods = '';"` |
|     - | 3268 | `"  if($rc->isAbstract()){ $mods .= 'abstract '; }"` |
|     - | 3269 | `"  if($rc->isFinal()){ $mods .= 'final '; }"` |
|     - | 3270 | `"  $head = 'Class [ <'.$tags.'> '.$mods.'class '.$rc->name;"` |
|     - | 3271 | `"  $par = $rc->getParentClass();"` |
|     - | 3272 | `"  if($par !== false){ $head .= ' extends '.$par->name; }"` |
|     - | 3273 | `"  $ifs = $rc->getInterfaceNames();"` |
|     - | 3274 | `"  if(count($ifs) > 0){ $head .= ' implements '.implode(', ', $ifs); }"` |
|     - | 3275 | `"  $head .= ' ]';"` |
|     - | 3276 | `" }"` |
|     - | 3277 | `" $s = $head.' {'.chr(10);"` |
|     - | 3278 | `" if(!$rc->isInternal()){"` |
|     - | 3279 | `"  $s .= '  @@ '.$rc->getFileName().' '.$rc->getStartLine().'-'.$rc->getEndLine().chr(10);"` |
|     - | 3280 | `" }"` |
|     - | 3281 | `" $consts = $rc->getReflectionConstants();"` |
|     - | 3282 | `" $s .= chr(10).'  - Constants ['.count($consts).'] {'.chr(10);"` |
|     - | 3283 | `" foreach($consts as $c){ $s .= '    '.__reflect_export_cconst($c); }"` |
|     - | 3284 | `" $s .= '  }'.chr(10);"` |
|     - | 3285 | `" $sp = array();"` |
|     - | 3286 | `" $ip = array();"` |
|     - | 3287 | `" foreach($rc->getProperties() as $p){"` |
|     - | 3288 | `"  if($p->isStatic()){ $sp[] = $p; }else{ $ip[] = $p; }"` |
|     - | 3289 | `" }"` |
|     - | 3290 | `" $sm = array();"` |
|     - | 3291 | `" $im = array();"` |
|     - | 3292 | `" foreach($rc->getMethods() as $m){"` |
|     - | 3293 | `"  if($m->isStatic()){ $sm[] = $m; }else{ $im[] = $m; }"` |
|     - | 3294 | `" }"` |
|     - | 3295 | `" $s .= chr(10).'  - Static properties ['.count($sp).'] {'.chr(10);"` |
|     - | 3296 | `" foreach($sp as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3297 | `" $s .= '  }'.chr(10);"` |
|     - | 3298 | `" $s .= chr(10).'  - Static methods ['.count($sm).'] {'.chr(10);"` |
|     - | 3299 | `" $first = true;"` |
|     - | 3300 | `" foreach($sm as $m){"` |
|     - | 3301 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3302 | `"  $first = false;"` |
|     - | 3303 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3304 | `" }"` |
|     - | 3305 | `" $s .= '  }'.chr(10);"` |
|     - | 3306 | `" $s .= chr(10).'  - Properties ['.count($ip).'] {'.chr(10);"` |
|     - | 3307 | `" foreach($ip as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3308 | `" $s .= '  }'.chr(10);"` |
|     - | 3309 | `" $s .= chr(10).'  - Methods ['.count($im).'] {'.chr(10);"` |
|     - | 3310 | `" $first = true;"` |
|     - | 3311 | `" foreach($im as $m){"` |
|     - | 3312 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3313 | `"  $first = false;"` |
|     - | 3314 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3315 | `" }"` |
|     - | 3316 | `" $s .= '  }'.chr(10);"` |
|     - | 3317 | `" return $s.'}'.chr(10);"` |
|     - | 3318 | `"}"` |
|     - | 3319 | `;` |
|     - | 3320 | `/*` |
|     - | 3321 | ` * Register the __reflect_* thunks and compile the Reflection library.` |
|     - | 3322 | ` * Called from PH7_VmInit while pVm->bCompilingBuiltin is set, right after` |
|     - | 3323 | ` * the core builtin chunks (Exception and friends must exist already).` |
|     - | 3324 | ` */` |
|  3884 | 3325 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|     5 | 3326 | `{` |
|     - | 3327 | `	static const struct {` |
|     - | 3328 | `		const char *zName;` |
|     - | 3329 | `		ProchHostFunction xFunc;` |
|     - | 3330 | `	} aFunc[] = {` |
|     - | 3331 | `		{ "__reflect_class_info",     vm_builtin_reflect_class_info },` |
|     - | 3332 | `		{ "__reflect_const_value",    vm_builtin_reflect_const_value },` |
|     - | 3333 | `		{ "__reflect_static_value",   vm_builtin_reflect_static_value },` |
|     - | 3334 | `		{ "__reflect_static_set",     vm_builtin_reflect_static_set },` |
|     - | 3335 | `		{ "__reflect_prop_default",   vm_builtin_reflect_prop_default },` |
|     - | 3336 | `		{ "__reflect_new_instance",   vm_builtin_reflect_new_instance },` |
|     - | 3337 | `		{ "__reflect_new_no_ctor",    vm_builtin_reflect_new_no_ctor },` |
|     - | 3338 | `		{ "__reflect_func_info",      vm_builtin_reflect_func_info },` |
|     - | 3339 | `		{ "__reflect_param_default",  vm_builtin_reflect_param_default },` |
|     - | 3340 | `		{ "__reflect_param_defconst", vm_builtin_reflect_param_defconst },` |
|     - | 3341 | `		{ "__reflect_invoke",         vm_builtin_reflect_invoke },` |
|     - | 3342 | `		{ "__reflect_closure",        vm_builtin_reflect_closure },` |
|     - | 3343 | `		{ "__reflect_prop_read",      vm_builtin_reflect_prop_read },` |
|     - | 3344 | `		{ "__reflect_prop_write",     vm_builtin_reflect_prop_write },` |
|     - | 3345 | `		{ "__reflect_prop_state",     vm_builtin_reflect_prop_state },` |
|     - | 3346 | `		{ "__reflect_dyn_props",      vm_builtin_reflect_dyn_props },` |
|     - | 3347 | `		{ "__reflect_gen_info",       vm_builtin_reflect_gen_info },` |
|     - | 3348 | `		{ "__reflect_gen_exec",       vm_builtin_reflect_gen_exec },` |
|     - | 3349 | `		{ "__reflect_const_info",     vm_builtin_reflect_const_info },` |
|     - | 3350 | `		{ "__reflect_ref_id",         vm_builtin_reflect_ref_id },` |
|     - | 3351 | `		{ "__reflect_attr_args",      vm_builtin_reflect_attr_args },` |
|     - | 3352 | `	};` |
|     - | 3353 | `	sxu32 n;` |
|     - | 3354 | `	sxi32 rc;` |
| 85453 | 3355 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 81569 | 3356 | `		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);` |
| 40787 | 3357 | `	}` |
|  3889 | 3358 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib1, sizeof(zReflectLib1)-1);` |
|  3889 | 3359 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3360 | `		return rc;` |
|     - | 3361 | `	}` |
|  3889 | 3362 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib2, sizeof(zReflectLib2)-1);` |
|  3889 | 3363 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3364 | `		return rc;` |
|     - | 3365 | `	}` |
|  3889 | 3366 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib3, sizeof(zReflectLib3)-1);` |
|  3889 | 3367 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3368 | `		return rc;` |
|     - | 3369 | `	}` |
|  3889 | 3370 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib4, sizeof(zReflectLib4)-1);` |
|  3889 | 3371 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3372 | `		return rc;` |
|     - | 3373 | `	}` |
|  3889 | 3374 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib5, sizeof(zReflectLib5)-1);` |
|  3889 | 3375 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3376 | `		return rc;` |
|     - | 3377 | `	}` |
|  3889 | 3378 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib6, sizeof(zReflectLib6)-1);` |
|  3889 | 3379 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3380 | `		return rc;` |
|     - | 3381 | `	}` |
|  3889 | 3382 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib7, sizeof(zReflectLib7)-1);` |
|  3889 | 3383 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3384 | `		return rc;` |
|     - | 3385 | `	}` |
|  3889 | 3386 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib8, sizeof(zReflectLib8)-1);` |
|  3889 | 3387 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3388 | `		return rc;` |
|     - | 3389 | `	}` |
|  3889 | 3390 | `	return PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib9, sizeof(zReflectLib9)-1);` |
|  1947 | 3391 | `}` |
|     - | 3392 |  |
