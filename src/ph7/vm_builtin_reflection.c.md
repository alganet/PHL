# src/ph7/vm_builtin_reflection.c

<style>code, pre { background: none !important; white-space: pre !important; width: 100% !important; display: inline-block !important; } td { border: none !important; margin-top: 0 !important; margin-bottom: 0 !important; padding-top: 0 !important; padding-bottom: 0 !important; }</style>

Coverage: 1035/1211 lines (85.47%)

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
|  1472 |   31 | `static ph7_class * ReflectResolveClass(ph7_vm *pVm, ph7_value *pArg)` |
|     2 |   32 | `{` |
|     - |   33 | `	ph7_class *pClass;` |
|  1474 |   34 | `	pClass = PH7_VmExtractClassFromValue(pVm, pArg);` |
|  1474 |   35 | `	if( pClass == 0 && ph7_value_is_string(pArg) ){` |
|     - |   36 | `		const char *zName;` |
|     - |   37 | `		int nLen;` |
|    16 |   38 | `		zName = ph7_value_to_string(pArg, &nLen);` |
|    16 |   39 | `		if( nLen > 0 ){` |
|    16 |   40 | `			pClass = PH7_VmTriggerAutoload(pVm, zName, (sxu32)nLen, FALSE);` |
|     7 |   41 | `		}` |
|     7 |   42 | `	}` |
|  1474 |   43 | `	return pClass;` |
|     2 |   44 | `}` |
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
| 54512 |   63 | `static void ReflectMapAddBool(ph7_context *pCtx, ph7_value *pMap, const char *zKey, int b)` |
|     2 |   64 | `{` |
| 54514 |   65 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 54514 |   66 | `	if( p == 0 ){ return; }` |
| 54514 |   67 | `	ph7_value_bool(p, b);` |
| 54514 |   68 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
| 27258 |   69 | `}` |
| 18418 |   70 | `static void ReflectMapAddInt(ph7_context *pCtx, ph7_value *pMap, const char *zKey, sxi64 iVal)` |
|     2 |   71 | `{` |
| 18420 |   72 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 18420 |   73 | `	if( p == 0 ){ return; }` |
| 18420 |   74 | `	ph7_value_int64(p, iVal);` |
| 18420 |   75 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  9211 |   76 | `}` |
| 13442 |   77 | `static void ReflectMapAddStr(ph7_context *pCtx, ph7_value *pMap, const char *zKey,` |
|     - |   78 | `	const char *zVal, int nVal)` |
|     2 |   79 | `{` |
| 13444 |   80 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
| 13444 |   81 | `	if( p == 0 ){ return; }` |
| 13444 |   82 | `	ph7_value_string(p, zVal, nVal);` |
| 13444 |   83 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  6723 |   84 | `}` |
|  4388 |   85 | `static void ReflectMapAddNull(ph7_context *pCtx, ph7_value *pMap, const char *zKey)` |
|     1 |   86 | `{` |
|  4389 |   87 | `	ph7_value *p = ph7_context_new_scalar(pCtx);` |
|  4389 |   88 | `	if( p == 0 ){ return; }` |
|  4389 |   89 | `	ph7_value_null(p);` |
|  4389 |   90 | `	ph7_array_add_strkey_elem(pMap, zKey, p);` |
|  2195 |   91 | `}` |
|     - |   92 | `/* Add an entry under a dynamic (SyString) key. */` |
|  5334 |   93 | `static void ReflectMapAddDyn(ph7_context *pCtx, ph7_value *pMap,` |
|     - |   94 | `	const SyString *pKey, ph7_value *pVal)` |
|     1 |   95 | `{` |
|  5335 |   96 | `	ph7_value *pK = ph7_context_new_scalar(pCtx);` |
|  5335 |   97 | `	if( pK == 0 ){ return; }` |
|  5335 |   98 | `	ph7_value_string(pK, pKey->zString, (int)pKey->nByte);` |
|  5335 |   99 | `	ph7_array_add_elem(pMap, pK, pVal);` |
|  2668 |  100 | `}` |
|     - |  101 | `/* Emit the declared #[...] attributes of a target as a summary list:` |
|     - |  102 | ` * [ {name, line} ... ]. Argument values stay lazy — the PHP layer pulls` |
|     - |  103 | ` * them through __reflect_attr_args when ReflectionAttribute needs them. */` |
|  5924 |  104 | `static void ReflectMapAddAttrs(ph7_context *pCtx, ph7_value *pMap, SySet *pAttrs)` |
|     1 |  105 | `{` |
|  5925 |  106 | `	ph7_value *pList = ph7_context_new_array(pCtx);` |
|  5925 |  107 | `	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);` |
|     - |  108 | `	sxu32 n;` |
|  5925 |  109 | `	if( pList == 0 ){` |
|   ! 0 |  110 | `		return;` |
|     - |  111 | `	}` |
|  6213 |  112 | `	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){` |
|   289 |  113 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|   289 |  114 | `		if( pMeta == 0 ){ break; }` |
|   289 |  115 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aA[n].sName), (int)SyStringLength(&aA[n].sName));` |
|   289 |  116 | `		ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)aA[n].nLine);` |
|   289 |  117 | `		ph7_array_add_elem(pList, 0, pMeta);` |
|   145 |  118 | `	}` |
|  5925 |  119 | `	ph7_array_add_strkey_elem(pMap, "attrs", pList);` |
|  2963 |  120 | `}` |
|     - |  121 | `/* Emit a doc-comment field: the text when present, else boolean false` |
|     - |  122 | ` * (getDocComment()'s exact return contract). */` |
|  4634 |  123 | `static void ReflectMapAddDoc(ph7_context *pCtx, ph7_value *pMap, const SyString *pDoc)` |
|     1 |  124 | `{` |
|  4635 |  125 | `	if( SyStringLength(pDoc) > 0 ){` |
|    63 |  126 | `		ReflectMapAddStr(pCtx, pMap, "doc", SyStringData(pDoc), (int)SyStringLength(pDoc));` |
|    32 |  127 | `	}else{` |
|  4573 |  128 | `		ReflectMapAddBool(pCtx, pMap, "doc", 0);` |
|     - |  129 | `	}` |
|  4635 |  130 | `}` |
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
|  1258 |  164 | `static void ReflectCollectInterfaces(ph7_class *pClass, SySet *pOut, int iDepth)` |
|     1 |  165 | `{` |
|     - |  166 | `	ph7_class **apIface;` |
|     - |  167 | `	sxu32 n;` |
|  1259 |  168 | `	if( pClass == 0 \|\| iDepth > REFLECT_WALK_MAX_DEPTH ){` |
|   ! 0 |  169 | `		return;` |
|     - |  170 | `	}` |
|  1259 |  171 | `	if( pClass->pBase ){` |
|   283 |  172 | `		ReflectCollectInterfaces(pClass->pBase, pOut, iDepth + 1);` |
|   141 |  173 | `	}` |
|  1259 |  174 | `	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);` |
|  1725 |  175 | `	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){` |
|   467 |  176 | `		ReflectAddInterface(apIface[n], pOut, iDepth + 1);` |
|   234 |  177 | `	}` |
|   630 |  178 | `}` |
|     - |  179 | `/*` |
|     - |  180 | ` * Deepest base class whose method table maps the same name to the very` |
|     - |  181 | ` * same ph7_class_method pointer: inheritance shares member pointers` |
|     - |  182 | ` * (PH7_ClassInherit), so this identifies the declaring class. Methods` |
|     - |  183 | ` * copied in from traits are not on the pBase chain and thus report the` |
|     - |  184 | ` * using class, which is what PHP reports too.` |
|     - |  185 | ` */` |
|  5000 |  186 | `static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth)` |
|     1 |  187 | `{` |
|  5001 |  188 | `	ph7_class *pDecl = pClass;` |
|  5001 |  189 | `	ph7_class *pBase = pClass->pBase;` |
|  5001 |  190 | `	int iDepth = 0;` |
|  6101 |  191 | `	while( pBase && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|     - |  192 | `		SyHashEntry *pEntry;` |
|  4045 |  193 | `		pEntry = SyHashGet(&pBase->hMethod, (const void *)SyStringData(&pMeth->sFunc.sName),` |
|  1348 |  194 | `			SyStringLength(&pMeth->sFunc.sName));` |
|  2697 |  195 | `		if( pEntry == 0 \|\| (ph7_class_method *)pEntry->pUserData != pMeth ){` |
|   799 |  196 | `			break;` |
|     - |  197 | `		}` |
|  1101 |  198 | `		pDecl = pBase;` |
|  1101 |  199 | `		pBase = pBase->pBase;` |
|  1101 |  200 | `		iDepth++;` |
|     1 |  201 | `	}` |
|  5001 |  202 | `	return pDecl;` |
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
|   990 |  233 | `static int vm_builtin_reflect_class_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 |  234 | `{` |
|   992 |  235 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  236 | `	ph7_class *pClass;` |
|     - |  237 | `	ph7_value *pInfo, *pConsts, *pProps, *pMethods, *pList;` |
|     - |  238 | `	SyHashEntry *pEntry;` |
|     - |  239 | `	SySet aIfaceSet;` |
|   992 |  240 | `	sxi32 iCtorVis = 0, iCloneVis = 0;` |
|   992 |  241 | `	int bIterable = 0;` |
|     - |  242 | `	sxu32 n;` |
|   992 |  243 | `	if( nArg < 1 ){` |
|   ! 0 |  244 | `		ph7_result_null(pCtx);` |
|   ! 0 |  245 | `		return PH7_OK;` |
|     - |  246 | `	}` |
|   992 |  247 | `	pClass = ReflectResolveClass(pVm, apArg[0]);` |
|   992 |  248 | `	if( pClass == 0 ){` |
|    16 |  249 | `		ph7_result_null(pCtx);` |
|    16 |  250 | `		return PH7_OK;` |
|     - |  251 | `	}` |
|   977 |  252 | `	pInfo = ph7_context_new_array(pCtx);` |
|   977 |  253 | `	pConsts = ph7_context_new_array(pCtx);` |
|   977 |  254 | `	pProps = ph7_context_new_array(pCtx);` |
|   977 |  255 | `	pMethods = ph7_context_new_array(pCtx);` |
|   977 |  256 | `	if( pInfo == 0 \|\| pConsts == 0 \|\| pProps == 0 \|\| pMethods == 0 ){` |
|   ! 0 |  257 | `		ph7_result_null(pCtx);` |
|   ! 0 |  258 | `		return PH7_OK;` |
|     - |  259 | `	}` |
|   977 |  260 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   977 |  261 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pClass->iFlags & PH7_CLASS_INTERNAL) != 0);` |
|   977 |  262 | `	ReflectMapAddBool(pCtx, pInfo, "interface", (pClass->iFlags & PH7_CLASS_INTERFACE) != 0);` |
|   977 |  263 | `	ReflectMapAddBool(pCtx, pInfo, "trait", (pClass->iFlags & PH7_CLASS_TRAIT) != 0);` |
|   977 |  264 | `	ReflectMapAddBool(pCtx, pInfo, "abstract", (pClass->iFlags & PH7_CLASS_ABSTRACT) != 0);` |
|   977 |  265 | `	ReflectMapAddBool(pCtx, pInfo, "final", (pClass->iFlags & PH7_CLASS_FINAL) != 0);` |
|   977 |  266 | `	ReflectMapAddBool(pCtx, pInfo, "readonly", (pClass->iFlags & PH7_CLASS_READONLY) != 0);` |
|   977 |  267 | `	ReflectMapAddBool(pCtx, pInfo, "enum", (pClass->iFlags & PH7_CLASS_ENUM) != 0);` |
|   977 |  268 | `	if( pClass->nEnumBacking == MEMOBJ_INT ){` |
|   ! 0 |  269 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "int", (int)sizeof("int")-1);` |
|   977 |  270 | `	}else if( pClass->nEnumBacking == MEMOBJ_STRING ){` |
|    67 |  271 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "string", (int)sizeof("string")-1);` |
|    34 |  272 | `	}else{` |
|   911 |  273 | `		ReflectMapAddStr(pCtx, pInfo, "enumbacking", "", 0);` |
|     - |  274 | `	}` |
|     - |  275 | `	{` |
|     - |  276 | `		/* Enum case names in declaration order (empty list for non-enums) */` |
|   977 |  277 | `		ph7_value *pCases = ph7_context_new_array(pCtx);` |
|   977 |  278 | `		if( pCases ){` |
|   977 |  279 | `			ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);` |
|     - |  280 | `			sxu32 nCase;` |
|  1137 |  281 | `			for( nCase = 0 ; nCase < SySetUsed(&pClass->aEnumCases) ; nCase++ ){` |
|   161 |  282 | `				ph7_value *pNm = ph7_context_new_scalar(pCtx);` |
|   161 |  283 | `				if( pNm ){` |
|   161 |  284 | `					ph7_value_string(pNm,apCase[nCase]->sName.zString,(int)apCase[nCase]->sName.nByte);` |
|   161 |  285 | `					ph7_array_add_elem(pCases,0,pNm);` |
|    80 |  286 | `				}` |
|    81 |  287 | `			}` |
|   977 |  288 | `			ph7_array_add_strkey_elem(pInfo,"cases",pCases);` |
|   488 |  289 | `		}` |
|     - |  290 | `	}` |
|   977 |  291 | `	if( pClass->pBase ){` |
|   418 |  292 | `		ReflectMapAddStr(pCtx, pInfo, "parent", SyStringData(&pClass->pBase->sName),` |
|   278 |  293 | `			(int)SyStringLength(&pClass->pBase->sName));` |
|   140 |  294 | `	}else{` |
|   699 |  295 | `		ReflectMapAddNull(pCtx, pInfo, "parent");` |
|     - |  296 | `	}` |
|     - |  297 | `	/* Transitive interfaces */` |
|   977 |  298 | `	SySetInit(&aIfaceSet, &pVm->sAllocator, sizeof(ph7_class *));` |
|   977 |  299 | `	ReflectCollectInterfaces(pClass, &aIfaceSet, 0);` |
|   977 |  300 | `	if( pClass->iFlags & PH7_CLASS_INTERFACE ){` |
|     - |  301 | `		/* An interface's own parents count as its interface list */` |
|    47 |  302 | `		if( pClass->pBase ){` |
|     9 |  303 | `			ReflectAddInterface(pClass->pBase, &aIfaceSet, 0);` |
|     4 |  304 | `		}` |
|    23 |  305 | `	}` |
|   977 |  306 | `	pList = ph7_context_new_array(pCtx);` |
|   977 |  307 | `	if( pList ){` |
|   977 |  308 | `		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIfaceSet);` |
|  1483 |  309 | `		for( n = 0 ; n < SySetUsed(&aIfaceSet) ; n++ ){` |
|   507 |  310 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|   507 |  311 | `			if( pName == 0 ){ break; }` |
|   507 |  312 | `			ph7_value_string(pName, SyStringData(&apIface[n]->sName), (int)SyStringLength(&apIface[n]->sName));` |
|   507 |  313 | `			ph7_array_add_elem(pList, 0, pName);` |
|   507 |  314 | `			if( pVm->pTraversableClass && apIface[n] == pVm->pTraversableClass ){` |
|    13 |  315 | `				bIterable = 1;` |
|     6 |  316 | `			}` |
|   254 |  317 | `		}` |
|   977 |  318 | `		ph7_array_add_strkey_elem(pInfo, "interfaces", pList);` |
|   488 |  319 | `	}` |
|   977 |  320 | `	SySetRelease(&aIfaceSet);` |
|   977 |  321 | `	ReflectMapAddBool(pCtx, pInfo, "iterable", bIterable);` |
|     - |  322 | `	/* Used traits */` |
|   977 |  323 | `	pList = ph7_context_new_array(pCtx);` |
|   977 |  324 | `	if( pList ){` |
|   977 |  325 | `		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);` |
|   989 |  326 | `		for( n = 0 ; n < SySetUsed(&pClass->aTrait) ; n++ ){` |
|    13 |  327 | `			ph7_value *pName = ph7_context_new_scalar(pCtx);` |
|    13 |  328 | `			if( pName == 0 ){ break; }` |
|    13 |  329 | `			ph7_value_string(pName, SyStringData(&apTrait[n]->sName), (int)SyStringLength(&apTrait[n]->sName));` |
|    13 |  330 | `			ph7_array_add_elem(pList, 0, pName);` |
|     7 |  331 | `		}` |
|   977 |  332 | `		ph7_array_add_strkey_elem(pInfo, "traits", pList);` |
|   488 |  333 | `	}` |
|     - |  334 | `	/* File / lines: no file recorded => false, like PHP internals */` |
|   977 |  335 | `	if( SyStringLength(&pClass->sFile) > 0 ){` |
|   895 |  336 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));` |
|   448 |  337 | `	}else{` |
|    83 |  338 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - |  339 | `	}` |
|   977 |  340 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pClass->nLine);` |
|   977 |  341 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pClass->nEndLine);` |
|   977 |  342 | `	ReflectMapAddDoc(pCtx, pInfo, &pClass->sDoc);` |
|   977 |  343 | `	ReflectMapAddAttrs(pCtx, pInfo, &pClass->aAttrs);` |
|     - |  344 | `	/* Members are emitted in PHP's reporting order: the class's own members` |
|     - |  345 | `	 * first (declaration order), then each inheritance level's, outward.` |
|     - |  346 | `	 * Per level we iterate the DECLARING class's own hash — subclass hashes` |
|     - |  347 | `	 * interleave inherited pointers unpredictably — and emit buffered` |
|     - |  348 | `	 * entries in reverse, because SyHash lists are LIFO. A pointer-identity` |
|     - |  349 | `	 * lookup in the reflected class's hash filters out members that are not` |
|     - |  350 | `	 * visible there (base privates, overridden entries). */` |
|     - |  351 | `	{` |
|     - |  352 | `		ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];` |
|   977 |  353 | `		ph7_class *pWalk = pClass;` |
|     - |  354 | `		SySet aTmp;` |
|   977 |  355 | `		sxu32 nChain = 0, iLevel, nT;` |
|  2235 |  356 | `		while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){` |
|  1259 |  357 | `			aChain[nChain++] = pWalk;` |
|  1259 |  358 | `			pWalk = pWalk->pBase;` |
|     1 |  359 | `		}` |
|   977 |  360 | `		SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));` |
|  2235 |  361 | `		for( iLevel = 0 ; iLevel < nChain ; iLevel++ ){` |
|  1259 |  362 | `			ph7_class *pLevel = aChain[iLevel];` |
|     - |  363 | `			/* --- Constants and properties (shared attribute table) --- */` |
|  1259 |  364 | `			SySetReset(&aTmp);` |
|  1259 |  365 | `			SyHashResetLoopCursor(&pLevel->hAttr);` |
|  4491 |  366 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hAttr)) != 0 ){` |
|  3233 |  367 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;` |
|  3233 |  368 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|  3233 |  369 | `				if( iLevel == 0 ){` |
|     - |  370 | `					sxu32 j;` |
|     - |  371 | `					/* Own = declared here or by an off-chain provider (trait) */` |
|  4255 |  372 | `					for( j = 1 ; j < nChain ; j++ ){` |
|  1287 |  373 | `						if( aChain[j] == pDecl ){ break; }` |
|   584 |  374 | `					}` |
|  3089 |  375 | `					if( j < nChain ){ continue; }` |
|  1485 |  376 | `				}else{` |
|     - |  377 | `					SyHashEntry *pSub;` |
|   145 |  378 | `					if( pDecl != pLevel ){ continue; }` |
|     - |  379 | `					/* Must still be the visible member in the reflected class */` |
|   121 |  380 | `					pSub = SyHashGet(&pClass->hAttr, pEntry->pKey, pEntry->nKeyLen);` |
|   121 |  381 | `					if( pSub == 0 \|\| pSub->pUserData != (void *)pAttr ){ continue; }` |
|     - |  382 | `				}` |
|  3089 |  383 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     1 |  384 | `			}` |
|  4347 |  385 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|  3089 |  386 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|  3089 |  387 | `				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;` |
|  3089 |  388 | `				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;` |
|  3089 |  389 | `				ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|  3089 |  390 | `				if( pMeta == 0 ){ break; }` |
|  3089 |  391 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pAttr->iProtection);` |
|  3089 |  392 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|  3089 |  393 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pAttr->nLine);` |
|  3089 |  394 | `				ReflectMapAddDoc(pCtx, pMeta, &pAttr->sDoc);` |
|  3089 |  395 | `				ReflectMapAddAttrs(pCtx, pMeta, &pAttr->aAttrs);` |
|  3089 |  396 | `				ReflectMapAddBool(pCtx, pMeta, "typed", (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);` |
|  3089 |  397 | `				if( SyStringLength(&pAttr->sTypeName) > 0 ){` |
|  1129 |  398 | `					ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&pAttr->sTypeName),` |
|   752 |  399 | `						(int)SyStringLength(&pAttr->sTypeName));` |
|   377 |  400 | `				}else{` |
|  2337 |  401 | `					ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - |  402 | `				}` |
|  3089 |  403 | `				if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){` |
|   883 |  404 | `					ReflectMapAddBool(pCtx, pMeta, "final", (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   883 |  405 | `					ReflectMapAddBool(pCtx, pMeta, "enumcase", (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0);` |
|   883 |  406 | `					ReflectMapAddDyn(pCtx, pConsts, &pAttr->sName, pMeta);` |
|   442 |  407 | `				}else{` |
|  2207 |  408 | `					ReflectMapAddBool(pCtx, pMeta, "static", (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|  2207 |  409 | `					ReflectMapAddBool(pCtx, pMeta, "readonly", (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0);` |
|  2207 |  410 | `					ReflectMapAddBool(pCtx, pMeta, "privset", (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) != 0);` |
|  2207 |  411 | `					ReflectMapAddBool(pCtx, pMeta, "protset", (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET) != 0);` |
|  2207 |  412 | `					ReflectMapAddBool(pCtx, pMeta, "hookget", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET) != 0);` |
|  2207 |  413 | `					ReflectMapAddBool(pCtx, pMeta, "hookset", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET) != 0);` |
|  2207 |  414 | `					ReflectMapAddBool(pCtx, pMeta, "virtual", (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) != 0);` |
|  2207 |  415 | `					ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&pAttr->aByteCode) > 0);` |
|  2207 |  416 | `					ReflectMapAddDyn(pCtx, pProps, &pAttr->sName, pMeta);` |
|     - |  417 | `				}` |
|  1545 |  418 | `			}` |
|     - |  419 | `			/* --- Methods. The reported name is the hash-entry key: trait` |
|     - |  420 | `			 * aliasing installs a shallow copy under the alias name while` |
|     - |  421 | `			 * sFunc.sName keeps the original, and PHP reports the alias. --- */` |
|  1259 |  422 | `			SySetReset(&aTmp);` |
|  1259 |  423 | `			SyHashResetLoopCursor(&pLevel->hMethod);` |
|  3805 |  424 | `			while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){` |
|  2547 |  425 | `				ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;` |
|  2547 |  426 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|  2547 |  427 | `				if( iLevel == 0 ){` |
|     - |  428 | `					sxu32 j;` |
|  2939 |  429 | `					for( j = 1 ; j < nChain ; j++ ){` |
|  1031 |  430 | `						if( aChain[j] == pDecl ){ break; }` |
|   372 |  431 | `					}` |
|  2197 |  432 | `					if( j < nChain ){ continue; }` |
|   955 |  433 | `				}else{` |
|     - |  434 | `					SyHashEntry *pSub;` |
|   351 |  435 | `					if( pDecl != pLevel ){ continue; }` |
|   315 |  436 | `					pSub = SyHashGet(&pClass->hMethod, pEntry->pKey, pEntry->nKeyLen);` |
|   315 |  437 | `					if( pSub == 0 ){` |
|     - |  438 | `						/* Not in the subclass table: inheritance skips private` |
|     - |  439 | `						 * methods, but PHP still reports them on the subclass` |
|     - |  440 | `						 * (Zend copies privates into the child function table). */` |
|   ! 0 |  441 | `						if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){` |
|   ! 0 |  442 | `							continue;` |
|   ! 0 |  443 | `						}` |
|   315 |  444 | `					}else if( pSub->pUserData != (void *)pMeth ){` |
|     - |  445 | `						/* Overridden below this level: already reported */` |
|    27 |  446 | `						continue;` |
|     - |  447 | `					}` |
|     - |  448 | `				}` |
|  2197 |  449 | `				SySetPut(&aTmp, (const void *)&pEntry);` |
|     1 |  450 | `			}` |
|  3455 |  451 | `			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){` |
|  2197 |  452 | `				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);` |
|  2197 |  453 | `				ph7_class_method *pMeth = (ph7_class_method *)pE->pUserData;` |
|  2197 |  454 | `				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|     - |  455 | `				ph7_value *pMeta;` |
|     - |  456 | `				SyString sKey;` |
|     - |  457 | `				int bIsAlias;` |
|  2197 |  458 | `				SyStringInitFromBuf(&sKey, (const char *)pE->pKey, pE->nKeyLen);` |
|  4393 |  459 | `				bIsAlias = (sKey.nByte != SyStringLength(&pMeth->sFunc.sName)` |
|  2196 |  460 | `				 \|\| SyMemcmp(sKey.zString, SyStringData(&pMeth->sFunc.sName), sKey.nByte) != 0);` |
|  2196 |  461 | `				if( sKey.nByte == sizeof("__construct")-1` |
|  1305 |  462 | `				 && SyMemcmp(sKey.zString, "__construct", sKey.nByte) == 0 ){` |
|   383 |  463 | `					if( iCtorVis == 0 ){` |
|   383 |  464 | `						iCtorVis = pMeth->iProtection;` |
|   191 |  465 | `					}` |
|   383 |  466 | `					if( bIsAlias ){` |
|     - |  467 | `						/* Mount-time alias for a legacy class-name constructor:` |
|     - |  468 | `						 * the method is already listed under its declared name. */` |
|   ! 0 |  469 | `						continue;` |
|     - |  470 | `					}` |
|  2006 |  471 | `				}else if( sKey.nByte == sizeof("__clone")-1` |
|  1002 |  472 | `				 && SyMemcmp(sKey.zString, "__clone", sKey.nByte) == 0 ){` |
|   ! 0 |  473 | `					if( iCloneVis == 0 ){` |
|   ! 0 |  474 | `						iCloneVis = pMeth->iProtection;` |
|   ! 0 |  475 | `					}` |
|  1814 |  476 | `				}else if( iCtorVis == 0` |
|  1348 |  477 | `				 && sKey.nByte == SyStringLength(&pClass->sName)` |
|   478 |  478 | `				 && SyMemcmp(sKey.zString, SyStringData(&pClass->sName), sKey.nByte) == 0 ){` |
|     - |  479 | `					/* Legacy class-name constructor before the mount alias exists */` |
|   ! 0 |  480 | `					iCtorVis = pMeth->iProtection;` |
|   ! 0 |  481 | `				}` |
|  2197 |  482 | `				pMeta = ph7_context_new_array(pCtx);` |
|  2197 |  483 | `				if( pMeta == 0 ){ break; }` |
|  2197 |  484 | `				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pMeth->iProtection);` |
|  2197 |  485 | `				ReflectMapAddBool(pCtx, pMeta, "static", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|  2197 |  486 | `				ReflectMapAddBool(pCtx, pMeta, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|  2197 |  487 | `				ReflectMapAddBool(pCtx, pMeta, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|  2197 |  488 | `				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|  2197 |  489 | `				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pMeth->nLine);` |
|  2197 |  490 | `				ReflectMapAddDyn(pCtx, pMethods, &sKey, pMeta);` |
|  1099 |  491 | `			}` |
|   630 |  492 | `		}` |
|   977 |  493 | `		SySetRelease(&aTmp);` |
|     - |  494 | `	}` |
|   977 |  495 | `	ReflectMapAddInt(pCtx, pInfo, "ctorvis", (sxi64)iCtorVis);` |
|   977 |  496 | `	ReflectMapAddInt(pCtx, pInfo, "clonevis", (sxi64)iCloneVis);` |
|   977 |  497 | `	ph7_array_add_strkey_elem(pInfo, "consts", pConsts);` |
|   977 |  498 | `	ph7_array_add_strkey_elem(pInfo, "props", pProps);` |
|   977 |  499 | `	ph7_array_add_strkey_elem(pInfo, "methods", pMethods);` |
|   977 |  500 | `	ph7_result_value(pCtx, pInfo);` |
|   977 |  501 | `	return PH7_OK;` |
|   497 |  502 | `}` |
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
|   748 |  954 | `static ph7_vm_func * ReflectResolveCallable(ph7_context *pCtx, ph7_value *pTarget,` |
|     - |  955 | `	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,` |
|     - |  956 | `	ph7_user_func **ppHost, ph7_class_instance **ppClosure)` |
|     2 |  957 | `{` |
|   750 |  958 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - |  959 | `	SyHashEntry *pEntry;` |
|   750 |  960 | `	if( ppClass ){ *ppClass = 0; }` |
|   750 |  961 | `	if( ppMeth ){ *ppMeth = 0; }` |
|   750 |  962 | `	if( ppHost ){ *ppHost = 0; }` |
|   750 |  963 | `	if( ppClosure ){ *ppClosure = 0; }` |
|   750 |  964 | `	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){` |
|   281 |  965 | `		ph7_class *pClass = ReflectResolveClass(pVm, pTarget);` |
|     - |  966 | `		ph7_class_method *pMeth;` |
|   281 |  967 | `		if( pClass == 0 ){` |
|   ! 0 |  968 | `			return 0;` |
|     - |  969 | `		}` |
|   421 |  970 | `		pMeth = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pMethodArg->sBlob),` |
|   140 |  971 | `			SyBlobLength(&pMethodArg->sBlob));` |
|   281 |  972 | `		if( pMeth == 0 ){` |
|   ! 0 |  973 | `			return 0;` |
|     - |  974 | `		}` |
|   281 |  975 | `		if( ppClass ){ *ppClass = pClass; }` |
|   281 |  976 | `		if( ppMeth ){ *ppMeth = pMeth; }` |
|   281 |  977 | `		return &pMeth->sFunc;` |
|     - |  978 | `	}` |
|     - |  979 | `	{` |
|   470 |  980 | `		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);` |
|   470 |  981 | `		if( pClo ){` |
|     - |  982 | `			SyString sAttr;` |
|     - |  983 | `			ph7_value *pFn;` |
|    53 |  984 | `			SyStringInitFromBuf(&sAttr, "__fn", 4);` |
|    53 |  985 | `			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);` |
|    53 |  986 | `			if( pFn == 0 \|\| (pFn->iFlags & MEMOBJ_STRING) == 0 \|\| SyBlobLength(&pFn->sBlob) < 1 ){` |
|   ! 0 |  987 | `				return 0;` |
|     - |  988 | `			}` |
|    53 |  989 | `			pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|    53 |  990 | `			if( pEntry == 0 ){` |
|     - |  991 | `				/* A Closure over a host function (Closure::fromCallable('strlen')) */` |
|   ! 0 |  992 | `				pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));` |
|   ! 0 |  993 | `				if( pEntry && ppHost ){` |
|   ! 0 |  994 | `					*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|   ! 0 |  995 | `					if( ppClosure ){ *ppClosure = pClo; }` |
|   ! 0 |  996 | `				}` |
|   ! 0 |  997 | `				return 0;` |
|     - |  998 | `			}` |
|    53 |  999 | `			if( ppClosure ){ *ppClosure = pClo; }` |
|    53 | 1000 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - | 1001 | `		}` |
|     - | 1002 | `	}` |
|   418 | 1003 | `	if( pTarget->iFlags & MEMOBJ_STRING ){` |
|   418 | 1004 | `		if( SyBlobLength(&pTarget->sBlob) < 1 ){` |
|   ! 0 | 1005 | `			return 0;` |
|     - | 1006 | `		}` |
|   418 | 1007 | `		pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   418 | 1008 | `		if( pEntry ){` |
|   285 | 1009 | `			return (ph7_vm_func *)pEntry->pUserData;` |
|     - | 1010 | `		}` |
|   134 | 1011 | `		pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));` |
|   134 | 1012 | `		if( pEntry && ppHost ){` |
|   130 | 1013 | `			*ppHost = (ph7_user_func *)pEntry->pUserData;` |
|    64 | 1014 | `		}` |
|    66 | 1015 | `	}` |
|   134 | 1016 | `	return 0;` |
|   376 | 1017 | `}` |
|     - | 1018 | `/* Emit the shared descriptor fields of a compiled function. */` |
|   570 | 1019 | `static void ReflectFillFuncCommon(ph7_context *pCtx, ph7_value *pInfo, ph7_vm_func *pFunc)` |
|     1 | 1020 | `{` |
|     - | 1021 | `	ph7_vm_func_arg *aArg;` |
|     - | 1022 | `	ph7_value *pParams, *pStatics;` |
|   571 | 1023 | `	int bVariadic = 0;` |
|     - | 1024 | `	int bAnon;` |
|     - | 1025 | `	sxu32 n;` |
|     - | 1026 | ``	/* A capture-free `function(){}` compiles without the CLOSURE flag but`` |
|     - | 1027 | `	 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */` |
|   571 | 1028 | `	bAnon = (pFunc->iFlags & VM_FUNC_CLOSURE) != 0;` |
|   570 | 1029 | `	if( !bAnon && SyStringLength(&pFunc->sName) > 9` |
|   306 | 1030 | `	 && (SyMemcmp(SyStringData(&pFunc->sName), "[lambda_", 8) == 0` |
|    82 | 1031 | `	  \|\| SyMemcmp(SyStringData(&pFunc->sName), "[closure_", 9) == 0) ){` |
|     5 | 1032 | `		bAnon = 1;` |
|     2 | 1033 | `	}` |
|   571 | 1034 | `	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|   571 | 1035 | `	ReflectMapAddBool(pCtx, pInfo, "internal", (pFunc->iFlags & VM_FUNC_INTERNAL) != 0);` |
|   571 | 1036 | `	ReflectMapAddBool(pCtx, pInfo, "closure", bAnon);` |
|   571 | 1037 | `	ReflectMapAddBool(pCtx, pInfo, "fstatic", (pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);` |
|   571 | 1038 | `	ReflectMapAddBool(pCtx, pInfo, "byref", (pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);` |
|   571 | 1039 | `	ReflectMapAddBool(pCtx, pInfo, "generator", (pFunc->iFlags & VM_FUNC_GENERATOR) != 0);` |
|   571 | 1040 | `	ReflectMapAddBool(pCtx, pInfo, "strict", pFunc->bStrictTypes != 0);` |
|   571 | 1041 | `	if( SyStringLength(&pFunc->sFile) > 0 ){` |
|   567 | 1042 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pFunc->sFile), (int)SyStringLength(&pFunc->sFile));` |
|   284 | 1043 | `	}else{` |
|     5 | 1044 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1045 | `	}` |
|   571 | 1046 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pFunc->nLine);` |
|   571 | 1047 | `	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pFunc->nEndLine);` |
|   571 | 1048 | `	ReflectMapAddDoc(pCtx, pInfo, &pFunc->sDoc);` |
|   571 | 1049 | `	ReflectMapAddAttrs(pCtx, pInfo, &pFunc->aAttrs);` |
|   571 | 1050 | `	if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){` |
|   145 | 1051 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", SyStringData(&pFunc->sReturnTypeName),` |
|    96 | 1052 | `			(int)SyStringLength(&pFunc->sReturnTypeName));` |
|   523 | 1053 | `	}else if( pFunc->nReturnType == MEMOBJ_VOID ){` |
|     - | 1054 | `		/* The type-text renderer omits void/never atoms (compile.c notes the` |
|     - | 1055 | `		 * root fix belongs there); name them here for getReturnType(). */` |
|     3 | 1056 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "void", sizeof("void")-1);` |
|   474 | 1057 | `	}else if( pFunc->nReturnType == MEMOBJ_NEVER ){` |
|     3 | 1058 | `		ReflectMapAddStr(pCtx, pInfo, "rettext", "never", sizeof("never")-1);` |
|     2 | 1059 | `	}else{` |
|   471 | 1060 | `		ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1061 | `	}` |
|   571 | 1062 | `	ReflectMapAddBool(pCtx, pInfo, "retnullable", (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) != 0);` |
|     - | 1063 | `	/* Parameters */` |
|   571 | 1064 | `	pParams = ph7_context_new_array(pCtx);` |
|   571 | 1065 | `	aArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);` |
|  1823 | 1066 | `	for( n = 0 ; pParams && n < SySetUsed(&pFunc->aArgs) ; n++ ){` |
|  1253 | 1067 | `		ph7_value *pMeta = ph7_context_new_array(pCtx);` |
|  1253 | 1068 | `		if( pMeta == 0 ){ break; }` |
|  1253 | 1069 | `		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aArg[n].sName), (int)SyStringLength(&aArg[n].sName));` |
|  1253 | 1070 | `		ReflectMapAddInt(pCtx, pMeta, "pos", (sxi64)n);` |
|  1253 | 1071 | `		ReflectMapAddBool(pCtx, pMeta, "byref", (aArg[n].iFlags & VM_FUNC_ARG_BY_REF) != 0);` |
|  1253 | 1072 | `		ReflectMapAddBool(pCtx, pMeta, "variadic", (aArg[n].iFlags & VM_FUNC_ARG_VARIADIC) != 0);` |
|     - | 1073 | `		/* The compiler never sets ARG_HAS_DEF; a default = compiled bytecode` |
|     - | 1074 | `		 * (same test the OP_CALL default-value path uses). */` |
|  1253 | 1075 | `		ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&aArg[n].aByteCode) > 0);` |
|  1253 | 1076 | `		ReflectMapAddBool(pCtx, pMeta, "nullable", (aArg[n].iFlags & VM_FUNC_ARG_NULLABLE) != 0);` |
|  1253 | 1077 | `		ReflectMapAddBool(pCtx, pMeta, "promoted", (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) != 0);` |
|  1253 | 1078 | `		if( SyStringLength(&aArg[n].sTypeName) > 0 ){` |
|   715 | 1079 | `			ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&aArg[n].sTypeName),` |
|   476 | 1080 | `				(int)SyStringLength(&aArg[n].sTypeName));` |
|   239 | 1081 | `		}else{` |
|   777 | 1082 | `			ReflectMapAddNull(pCtx, pMeta, "typetext");` |
|     - | 1083 | `		}` |
|  1253 | 1084 | `		ReflectMapAddAttrs(pCtx, pMeta, &aArg[n].aAttrs);` |
|  1253 | 1085 | `		ph7_array_add_elem(pParams, 0, pMeta);` |
|  1253 | 1086 | `		if( aArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){` |
|   183 | 1087 | `			bVariadic = 1;` |
|    91 | 1088 | `		}` |
|   627 | 1089 | `	}` |
|   571 | 1090 | `	if( pParams ){` |
|   571 | 1091 | `		ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|   285 | 1092 | `	}` |
|   571 | 1093 | `	ReflectMapAddBool(pCtx, pInfo, "variadic", bVariadic);` |
|     - | 1094 | `	/* Static variables: current value when the slot was initialized (first` |
|     - | 1095 | `	 * call), otherwise the evaluated default — PHP's getStaticVariables` |
|     - | 1096 | `	 * initializes on demand and reports the same values. */` |
|   571 | 1097 | `	pStatics = ph7_context_new_array(pCtx);` |
|   571 | 1098 | `	if( pStatics ){` |
|   571 | 1099 | `		ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);` |
|   599 | 1100 | `		for( n = 0 ; n < SySetUsed(&pFunc->aStatic) ; n++ ){` |
|    29 | 1101 | `			ph7_value *pVal = 0;` |
|     - | 1102 | `			ph7_value sScratch;` |
|    29 | 1103 | `			int bScratch = 0;` |
|    29 | 1104 | `			if( aStatic[n].nIdx != SXU32_HIGH ){` |
|    11 | 1105 | `				pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);` |
|     5 | 1106 | `			}` |
|    29 | 1107 | `			if( pVal == 0 ){` |
|    19 | 1108 | `				PH7_MemObjInit(pCtx->pVm, &sScratch);` |
|    19 | 1109 | `				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){` |
|    19 | 1110 | `					VmLocalExec(pCtx->pVm, &aStatic[n].aByteCode, &sScratch, FALSE);` |
|     9 | 1111 | `				}` |
|    19 | 1112 | `				pVal = &sScratch;` |
|    19 | 1113 | `				bScratch = 1;` |
|     9 | 1114 | `			}` |
|    29 | 1115 | `			ReflectMapAddDyn(pCtx, pStatics, &aStatic[n].sName, pVal);` |
|    29 | 1116 | `			if( bScratch ){` |
|    19 | 1117 | `				PH7_MemObjRelease(&sScratch);` |
|     9 | 1118 | `			}` |
|    15 | 1119 | `		}` |
|   571 | 1120 | `		ph7_array_add_strkey_elem(pInfo, "statics", pStatics);` |
|   285 | 1121 | `	}` |
|   571 | 1122 | `}` |
|     - | 1123 | `/*` |
|     - | 1124 | ` * array\|null __reflect_func_info(string\|Closure $target [, string $method])` |
|     - | 1125 | ` * Function/method/closure descriptor for the PHP layer.` |
|     - | 1126 | ` */` |
|   702 | 1127 | `static int vm_builtin_reflect_func_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     2 | 1128 | `{` |
|     - | 1129 | `	ph7_vm_func *pFunc;` |
|   704 | 1130 | `	ph7_class *pClass = 0;` |
|   704 | 1131 | `	ph7_class_method *pMeth = 0;` |
|   704 | 1132 | `	ph7_user_func *pHost = 0;` |
|   704 | 1133 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1134 | `	ph7_value *pInfo;` |
|   704 | 1135 | `	if( nArg < 1 ){` |
|   ! 0 | 1136 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1137 | `		return PH7_OK;` |
|     - | 1138 | `	}` |
|   704 | 1139 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], nArg > 1 ? apArg[1] : 0,` |
|     - | 1140 | `		&pClass, &pMeth, &pHost, &pClosure);` |
|   704 | 1141 | `	if( pFunc == 0 && pHost == 0 ){` |
|     6 | 1142 | `		ph7_result_null(pCtx);` |
|     6 | 1143 | `		return PH7_OK;` |
|     - | 1144 | `	}` |
|   700 | 1145 | `	pInfo = ph7_context_new_array(pCtx);` |
|   700 | 1146 | `	if( pInfo == 0 ){` |
|   ! 0 | 1147 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1148 | `		return PH7_OK;` |
|     - | 1149 | `	}` |
|   700 | 1150 | `	if( pFunc == 0 ){` |
|     - | 1151 | `		/* Host (C builtin) function: no parameter metadata beyond arity */` |
|   130 | 1152 | `		ph7_value *pParams = ph7_context_new_array(pCtx);` |
|   130 | 1153 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pHost->sName), (int)SyStringLength(&pHost->sName));` |
|   130 | 1154 | `		ReflectMapAddBool(pCtx, pInfo, "internal", 1);` |
|   130 | 1155 | `		ReflectMapAddBool(pCtx, pInfo, "closure", 0);` |
|   130 | 1156 | `		ReflectMapAddBool(pCtx, pInfo, "fstatic", 0);` |
|   130 | 1157 | `		ReflectMapAddBool(pCtx, pInfo, "byref", 0);` |
|   130 | 1158 | `		ReflectMapAddBool(pCtx, pInfo, "generator", 0);` |
|   130 | 1159 | `		ReflectMapAddBool(pCtx, pInfo, "strict", 0);` |
|   130 | 1160 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|   130 | 1161 | `		ReflectMapAddInt(pCtx, pInfo, "line", 0);` |
|   130 | 1162 | `		ReflectMapAddInt(pCtx, pInfo, "endline", 0);` |
|   130 | 1163 | `		ReflectMapAddBool(pCtx, pInfo, "doc", 0);` |
|     - | 1164 | `		{` |
|   130 | 1165 | `			ph7_value *pEmpty = ph7_context_new_array(pCtx);` |
|   130 | 1166 | `			if( pEmpty ){` |
|   130 | 1167 | `				ph7_array_add_strkey_elem(pInfo, "attrs", pEmpty);` |
|    64 | 1168 | `			}` |
|     - | 1169 | `		}` |
|   130 | 1170 | `		if( pHost->zRet ){` |
|   130 | 1171 | `			ReflectMapAddStr(pCtx, pInfo, "rettext", pHost->zRet, (int)SyStrlen(pHost->zRet));` |
|    66 | 1172 | `		}else{` |
|   ! 0 | 1173 | `			ReflectMapAddNull(pCtx, pInfo, "rettext");` |
|     - | 1174 | `		}` |
|   130 | 1175 | `		ReflectMapAddBool(pCtx, pInfo, "retnullable", 0);` |
|   130 | 1176 | `		if( pParams ){` |
|   130 | 1177 | `			ph7_array_add_strkey_elem(pInfo, "params", pParams);` |
|    64 | 1178 | `		}` |
|   130 | 1179 | `		ReflectMapAddBool(pCtx, pInfo, "variadic", 0);` |
|   130 | 1180 | `		ReflectMapAddInt(pCtx, pInfo, "minarg", (sxi64)pHost->nMinArg);` |
|   130 | 1181 | `		if( pHost->zSig ){` |
|   130 | 1182 | `			ReflectMapAddStr(pCtx, pInfo, "sig", pHost->zSig, (int)SyStrlen(pHost->zSig));` |
|    66 | 1183 | `		}else{` |
|   ! 0 | 1184 | `			ReflectMapAddStr(pCtx, pInfo, "sig", "", 0);` |
|     - | 1185 | `		}` |
|   130 | 1186 | `		ph7_result_value(pCtx, pInfo);` |
|   130 | 1187 | `		return PH7_OK;` |
|     - | 1188 | `	}` |
|   571 | 1189 | `	ReflectFillFuncCommon(pCtx, pInfo, pFunc);` |
|   571 | 1190 | `	ReflectMapAddInt(pCtx, pInfo, "minarg", -1);` |
|   571 | 1191 | `	if( (pFunc->iFlags & VM_FUNC_INTERNAL) && SySetUsed(&pFunc->aArgs) == 0 && pMeth == 0 ){` |
|     - | 1192 | `		/* Embedded-PHP builtin (max/min...): declared argless, actual` |
|     - | 1193 | `		 * signature comes from the static table */` |
|     5 | 1194 | `		const char *zRet = 0;` |
|     5 | 1195 | `		const char *zSig = PH7_VmBuiltinSigLookup(SyStringData(&pFunc->sName), SyStringLength(&pFunc->sName), &zRet);` |
|     5 | 1196 | `		if( zSig ){` |
|     5 | 1197 | `			ReflectMapAddStr(pCtx, pInfo, "sig", zSig, (int)SyStrlen(zSig));` |
|     2 | 1198 | `		}` |
|     5 | 1199 | `		if( zRet && SyStringLength(&pFunc->sReturnTypeName) == 0 ){` |
|     5 | 1200 | `			ReflectMapAddStr(pCtx, pInfo, "ret2", zRet, (int)SyStrlen(zRet));` |
|     2 | 1201 | `		}` |
|     2 | 1202 | `	}` |
|   571 | 1203 | `	if( pMeth && pClass ){` |
|   259 | 1204 | `		ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);` |
|   259 | 1205 | `		ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));` |
|   259 | 1206 | `		ReflectMapAddStr(pCtx, pInfo, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|   259 | 1207 | `		ReflectMapAddInt(pCtx, pInfo, "vis", (sxi64)pMeth->iProtection);` |
|   259 | 1208 | `		ReflectMapAddBool(pCtx, pInfo, "mstatic", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);` |
|   259 | 1209 | `		ReflectMapAddBool(pCtx, pInfo, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);` |
|   259 | 1210 | `		ReflectMapAddBool(pCtx, pInfo, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);` |
|   129 | 1211 | `	}` |
|   571 | 1212 | `	if( pClosure ){` |
|     - | 1213 | `		SyString sAttr;` |
|     - | 1214 | `		ph7_value *pAttr;` |
|     - | 1215 | `		ph7_value *pUsed;` |
|    49 | 1216 | `		SyStringInitFromBuf(&sAttr, "__this", 6);` |
|    49 | 1217 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    49 | 1218 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){` |
|   ! 0 | 1219 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 1220 | `			if( pKey ){` |
|   ! 0 | 1221 | `				ph7_value_string(pKey, "this", 4);` |
|   ! 0 | 1222 | `				ph7_array_add_elem(pInfo, pKey, pAttr);` |
|   ! 0 | 1223 | `			}` |
|   ! 0 | 1224 | `		}else{` |
|    49 | 1225 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|     - | 1226 | `		}` |
|    49 | 1227 | `		SyStringInitFromBuf(&sAttr, "__scope", 7);` |
|    49 | 1228 | `		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);` |
|    49 | 1229 | `		if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){` |
|   ! 0 | 1230 | `			ReflectMapAddStr(pCtx, pInfo, "scope", (const char *)SyBlobData(&pAttr->sBlob),` |
|   ! 0 | 1231 | `				(int)SyBlobLength(&pAttr->sBlob));` |
|   ! 0 | 1232 | `		}else{` |
|    49 | 1233 | `			ReflectMapAddNull(pCtx, pInfo, "scope");` |
|     - | 1234 | `		}` |
|     - | 1235 | `		/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */` |
|    49 | 1236 | `		pUsed = ph7_context_new_array(pCtx);` |
|    49 | 1237 | `		if( pUsed ){` |
|    49 | 1238 | `			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);` |
|     - | 1239 | `			sxu32 n;` |
|   107 | 1240 | `			for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; n++ ){` |
|    59 | 1241 | `				if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){` |
|    41 | 1242 | `					continue;` |
|     - | 1243 | `				}` |
|    18 | 1244 | `				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1` |
|    10 | 1245 | `				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){` |
|   ! 0 | 1246 | `					continue;` |
|     - | 1247 | `				}` |
|    19 | 1248 | `				if( (aEnv[n].iFlags & VM_FUNC_ARG_BY_REF) && aEnv[n].nIdx != SXU32_HIGH ){` |
|     - | 1249 | `					/* Captured by reference: report the slot's live value */` |
|     5 | 1250 | `					ph7_value *pLive = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aEnv[n].nIdx);` |
|     5 | 1251 | `					ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, pLive ? pLive : &aEnv[n].sValue);` |
|     5 | 1252 | `					continue;` |
|     - | 1253 | `				}` |
|    15 | 1254 | `				ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, &aEnv[n].sValue);` |
|     8 | 1255 | `			}` |
|    49 | 1256 | `			ph7_array_add_strkey_elem(pInfo, "used", pUsed);` |
|    24 | 1257 | `		}` |
|    24 | 1258 | `	}` |
|   571 | 1259 | `	ph7_result_value(pCtx, pInfo);` |
|   571 | 1260 | `	return PH7_OK;` |
|   353 | 1261 | `}` |
|     - | 1262 | `/*` |
|     - | 1263 | ` * mixed __reflect_param_default(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1264 | ` * Evaluate a parameter's compiled default expression.` |
|     - | 1265 | ` */` |
|    12 | 1266 | `static int vm_builtin_reflect_param_default(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1267 | `{` |
|     - | 1268 | `	ph7_vm_func *pFunc;` |
|     - | 1269 | `	ph7_vm_func_arg *pArg;` |
|     - | 1270 | `	ph7_value sValue;` |
|     - | 1271 | `	sxu32 nIdx;` |
|    13 | 1272 | `	if( nArg < 3 ){` |
|   ! 0 | 1273 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1274 | `		return PH7_OK;` |
|     - | 1275 | `	}` |
|    13 | 1276 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|    13 | 1277 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|    12 | 1278 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|    13 | 1279 | `	 \|\| SySetUsed(&pArg->aByteCode) < 1 ){` |
|   ! 0 | 1280 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1281 | `		return PH7_OK;` |
|     - | 1282 | `	}` |
|    13 | 1283 | `	PH7_MemObjInit(pCtx->pVm, &sValue);` |
|    13 | 1284 | `	VmLocalExec(pCtx->pVm, &pArg->aByteCode, &sValue, FALSE);` |
|    13 | 1285 | `	ph7_result_value(pCtx, &sValue);` |
|    13 | 1286 | `	PH7_MemObjRelease(&sValue);` |
|    13 | 1287 | `	return PH7_OK;` |
|     7 | 1288 | `}` |
|     - | 1289 | `/*` |
|     - | 1290 | ` * string\|null __reflect_param_defconst(string\|Closure $target, ?string $method, int $idx)` |
|     - | 1291 | ` * When a parameter's default is a plain global-constant reference, its` |
|     - | 1292 | ` * source name; null otherwise. A constant default compiles to exactly` |
|     - | 1293 | ` * [ OP_LOADC (EXPAND) , OP_DONE ] with the name in the literal table.` |
|     - | 1294 | ` */` |
|     6 | 1295 | `static int vm_builtin_reflect_param_defconst(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1296 | `{` |
|     - | 1297 | `	ph7_vm_func *pFunc;` |
|     - | 1298 | `	ph7_vm_func_arg *pArg;` |
|     - | 1299 | `	VmInstr *aInstr;` |
|     - | 1300 | `	ph7_value *pLit;` |
|     - | 1301 | `	sxu32 nIdx;` |
|     7 | 1302 | `	if( nArg < 3 ){` |
|   ! 0 | 1303 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1304 | `		return PH7_OK;` |
|     - | 1305 | `	}` |
|     7 | 1306 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);` |
|     7 | 1307 | `	nIdx = (sxu32)ph7_value_to_int(apArg[2]);` |
|     6 | 1308 | `	if( pFunc == 0 \|\| (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0` |
|     7 | 1309 | `	 \|\| SySetUsed(&pArg->aByteCode) != 2 ){` |
|     3 | 1310 | `		ph7_result_null(pCtx);` |
|     3 | 1311 | `		return PH7_OK;` |
|     - | 1312 | `	}` |
|     5 | 1313 | `	aInstr = (VmInstr *)SySetBasePtr(&pArg->aByteCode);` |
|     4 | 1314 | `	if( aInstr[0].iOp != PH7_OP_LOADC \|\| (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0` |
|     5 | 1315 | `	 \|\| aInstr[1].iOp != PH7_OP_DONE ){` |
|   ! 0 | 1316 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1317 | `		return PH7_OK;` |
|     - | 1318 | `	}` |
|     5 | 1319 | `	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);` |
|     5 | 1320 | `	if( pLit == 0 \|\| SyBlobLength(&pLit->sBlob) < 1 ){` |
|   ! 0 | 1321 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1322 | `		return PH7_OK;` |
|     - | 1323 | `	}` |
|     5 | 1324 | `	ph7_result_string(pCtx, (const char *)SyBlobData(&pLit->sBlob), (int)SyBlobLength(&pLit->sBlob));` |
|     5 | 1325 | `	return PH7_OK;` |
|     4 | 1326 | `}` |
|     - | 1327 | `/*` |
|     - | 1328 | ` * mixed __reflect_invoke(mixed $target, ?string $method, ?object $this, array $args)` |
|     - | 1329 | ` * Visibility-bypassing invocation (methods dispatch by VM name; functions` |
|     - | 1330 | ` * and closures ride PH7_VmCallUserFunction like call_user_func_array).` |
|     - | 1331 | ` */` |
|    20 | 1332 | `static int vm_builtin_reflect_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1333 | `{` |
|    21 | 1334 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1335 | `	ph7_value sResult;` |
|     - | 1336 | `	SySet aCallArg;` |
|     - | 1337 | `	sxi32 rc;` |
|    21 | 1338 | `	if( nArg < 4 ){` |
|   ! 0 | 1339 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1340 | `		return PH7_OK;` |
|     - | 1341 | `	}` |
|    21 | 1342 | `	PH7_MemObjInit(pVm, &sResult);` |
|    21 | 1343 | `	sResult.nIdx = SXU32_HIGH;` |
|    21 | 1344 | `	SySetInit(&aCallArg, &pVm->sAllocator, sizeof(ph7_value *));` |
|    21 | 1345 | `	ReflectCollectArgs(pCtx, apArg[3], &aCallArg, 0);` |
|    21 | 1346 | `	if( (apArg[1]->iFlags & MEMOBJ_STRING) && SyBlobLength(&apArg[1]->sBlob) > 0 ){` |
|    11 | 1347 | `		ph7_class *pClass = 0;` |
|    11 | 1348 | `		ph7_class_method *pMeth = 0;` |
|    11 | 1349 | `		ph7_class_instance *pThis = 0;` |
|    11 | 1350 | `		ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, 0);` |
|    11 | 1351 | `		if( pMeth == 0 ){` |
|   ! 0 | 1352 | `			SySetRelease(&aCallArg);` |
|   ! 0 | 1353 | `			PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1354 | `			ph7_result_null(pCtx);` |
|   ! 0 | 1355 | `			return PH7_OK;` |
|     - | 1356 | `		}` |
|    11 | 1357 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     7 | 1358 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     3 | 1359 | `		}` |
|     - | 1360 | `		/* Reflection ignores method visibility (PHP 8.1+); the flag is` |
|     - | 1361 | `		 * consumed by the first OP_CALL, i.e. this synthetic one. */` |
|    11 | 1362 | `		pVm->bReflectBypass = 1;` |
|    16 | 1363 | `		rc = PH7_VmCallClassMethod(pVm, pThis, pMeth, &sResult,` |
|    10 | 1364 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg));` |
|    11 | 1365 | `		pVm->bReflectBypass = 0;` |
|     6 | 1366 | `	}else{` |
|    16 | 1367 | `		rc = PH7_VmCallUserFunction(pVm, apArg[0],` |
|    10 | 1368 | `			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg), &sResult);` |
|     - | 1369 | `	}` |
|    21 | 1370 | `	SySetRelease(&aCallArg);` |
|    21 | 1371 | `	if( rc == PH7_EXCEPTION \|\| rc == PH7_ABORT ){` |
|   ! 0 | 1372 | `		PH7_MemObjRelease(&sResult);` |
|   ! 0 | 1373 | `		return rc;` |
|     - | 1374 | `	}` |
|    21 | 1375 | `	ph7_result_value(pCtx, &sResult);` |
|    21 | 1376 | `	PH7_MemObjRelease(&sResult);` |
|    21 | 1377 | `	return PH7_OK;` |
|    11 | 1378 | `}` |
|     - | 1379 | `/*` |
|     - | 1380 | ` * Closure __reflect_closure(mixed $target, ?string $method, ?object $this)` |
|     - | 1381 | ` * Mint a Closure for a function or method, bound and scoped like the` |
|     - | 1382 | ` * first-class-callable path.` |
|     - | 1383 | ` */` |
|     6 | 1384 | `static int vm_builtin_reflect_closure(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1385 | `{` |
|     7 | 1386 | `	ph7_vm *pVm = pCtx->pVm;` |
|     7 | 1387 | `	ph7_class *pClass = 0;` |
|     7 | 1388 | `	ph7_class_method *pMeth = 0;` |
|     7 | 1389 | `	ph7_class_instance *pClosure = 0;` |
|     - | 1390 | `	ph7_vm_func *pFunc;` |
|     7 | 1391 | `	if( nArg < 3 ){` |
|   ! 0 | 1392 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1393 | `		return PH7_OK;` |
|     - | 1394 | `	}` |
|     7 | 1395 | `	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, &pClosure);` |
|     7 | 1396 | `	if( pClosure ){` |
|     - | 1397 | `		/* Already a Closure: hand the same instance back */` |
|   ! 0 | 1398 | `		return ReflectResultExistingObject(pCtx, pClosure);` |
|     - | 1399 | `	}` |
|     7 | 1400 | `	if( pMeth && pClass ){` |
|     5 | 1401 | `		ph7_class_instance *pThis = 0;` |
|     5 | 1402 | `		if( apArg[2]->iFlags & MEMOBJ_OBJ ){` |
|     3 | 1403 | `			pThis = (ph7_class_instance *)apArg[2]->x.pOther;` |
|     1 | 1404 | `		}` |
|     7 | 1405 | `		return ReflectResultObject(pCtx,` |
|     4 | 1406 | `			PH7_VmNewClosure(pVm, &pMeth->sFunc.sName, pThis, &pClass->sName));` |
|     - | 1407 | `	}` |
|     3 | 1408 | `	if( pFunc ){` |
|     3 | 1409 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &pFunc->sName, 0, 0));` |
|     - | 1410 | `	}` |
|     - | 1411 | `	/* Host function by name */` |
|   ! 0 | 1412 | `	if( apArg[0]->iFlags & MEMOBJ_STRING ){` |
|     - | 1413 | `		SyString sName;` |
|   ! 0 | 1414 | `		SyStringInitFromBuf(&sName, (const char *)SyBlobData(&apArg[0]->sBlob), SyBlobLength(&apArg[0]->sBlob));` |
|   ! 0 | 1415 | `		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &sName, 0, 0));` |
|     - | 1416 | `	}` |
|   ! 0 | 1417 | `	ph7_result_null(pCtx);` |
|   ! 0 | 1418 | `	return PH7_OK;` |
|     4 | 1419 | `}` |
|     - | 1420 | `/*` |
|     - | 1421 | ` * Resolve a Generator object into its wrapper. Mirrors the static` |
|     - | 1422 | ` * VmGeneratorExtractCtx in vm.c: the $__ctx attribute carries the` |
|     - | 1423 | ` * ph7_generator pointer as a resource value.` |
|     - | 1424 | ` */` |
|    22 | 1425 | `static ph7_generator * ReflectGeneratorCtx(ph7_vm *pVm, ph7_value *pVal)` |
|     1 | 1426 | `{` |
|     - | 1427 | `	ph7_class_instance *pThis;` |
|     - | 1428 | `	ph7_value *pAttr;` |
|     - | 1429 | `	SyString sAttr;` |
|    23 | 1430 | `	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 \|\| pVm->pGeneratorClass == 0 ){` |
|   ! 0 | 1431 | `		return 0;` |
|     - | 1432 | `	}` |
|    23 | 1433 | `	pThis = (ph7_class_instance *)pVal->x.pOther;` |
|    23 | 1434 | `	if( pThis->pClass != pVm->pGeneratorClass ){` |
|   ! 0 | 1435 | `		return 0;` |
|     - | 1436 | `	}` |
|    23 | 1437 | `	SyStringInitFromBuf(&sAttr, "__ctx", 5);` |
|    23 | 1438 | `	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);` |
|    23 | 1439 | `	if( pAttr == 0 \|\| (pAttr->iFlags & MEMOBJ_RES) == 0 ){` |
|   ! 0 | 1440 | `		return 0;` |
|     - | 1441 | `	}` |
|    23 | 1442 | `	return (ph7_generator *)pAttr->x.pOther;` |
|    12 | 1443 | `}` |
|     - | 1444 | `/*` |
|     - | 1445 | ` * array\|null __reflect_gen_info(Generator $g)` |
|     - | 1446 | ` * {state, closed, executing, kind ('fn'\|'method'), name, class?, this}` |
|     - | 1447 | ` */` |
|    16 | 1448 | `static int vm_builtin_reflect_gen_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1449 | `{` |
|    17 | 1450 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1451 | `	ph7_generator *pGen;` |
|     - | 1452 | `	ph7_exec_ctx *pExec;` |
|     - | 1453 | `	ph7_value *pInfo;` |
|    17 | 1454 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 \|\| pGen->pCtx == 0 ){` |
|   ! 0 | 1455 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1456 | `		return PH7_OK;` |
|     - | 1457 | `	}` |
|    17 | 1458 | `	pExec = pGen->pCtx;` |
|    17 | 1459 | `	pInfo = ph7_context_new_array(pCtx);` |
|    17 | 1460 | `	if( pInfo == 0 ){` |
|   ! 0 | 1461 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1462 | `		return PH7_OK;` |
|     - | 1463 | `	}` |
|    17 | 1464 | `	ReflectMapAddInt(pCtx, pInfo, "state", (sxi64)pExec->iState);` |
|    24 | 1465 | `	ReflectMapAddBool(pCtx, pInfo, "closed",` |
|    16 | 1466 | `		pExec->iState == PH7_CTX_STATE_COMPLETED \|\| pExec->iState == PH7_CTX_STATE_CLOSED);` |
|    17 | 1467 | `	ReflectMapAddBool(pCtx, pInfo, "executing", pVm->pActiveCtx == pExec);` |
|    17 | 1468 | `	if( pExec->pFunc ){` |
|    17 | 1469 | `		ph7_vm_func *pFunc = pExec->pFunc;` |
|    19 | 1470 | `		if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){` |
|     5 | 1471 | `			ph7_class *pDecl = (ph7_class *)pFunc->pUserData;` |
|     5 | 1472 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "method", sizeof("method")-1);` |
|     5 | 1473 | `			ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));` |
|     3 | 1474 | `		}else{` |
|    13 | 1475 | `			ReflectMapAddStr(pCtx, pInfo, "kind", "fn", sizeof("fn")-1);` |
|     - | 1476 | `		}` |
|    17 | 1477 | `		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));` |
|     8 | 1478 | `	}` |
|     - | 1479 | `	{` |
|     - | 1480 | `		/* The coroutine frame installs $this as a frame VARIABLE (see` |
|     - | 1481 | `		 * VmFiberSetupFrame), not as pFrame->pThis — check both. */` |
|    17 | 1482 | `		ph7_value *pThisVal = 0;` |
|    17 | 1483 | `		if( pExec->pFrame ){` |
|    17 | 1484 | `			SyHashEntry *pVar = SyHashGet(&pExec->pFrame->hVar, "this", sizeof("this")-1);` |
|    17 | 1485 | `			if( pVar ){` |
|     5 | 1486 | `				ph7_value *pSlot = (ph7_value *)SySetAt(&pVm->aMemObj, (sxu32)SX_PTR_TO_INT(pVar->pUserData));` |
|     5 | 1487 | `				if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){` |
|     5 | 1488 | `					pThisVal = pSlot;` |
|     2 | 1489 | `				}` |
|     2 | 1490 | `			}` |
|    17 | 1491 | `			if( pThisVal == 0 && pExec->pFrame->pThis ){` |
|     - | 1492 | `				ph7_value sThis;` |
|   ! 0 | 1493 | `				ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|   ! 0 | 1494 | `				PH7_MemObjInit(pVm, &sThis);` |
|   ! 0 | 1495 | `				pExec->pFrame->pThis->iRef++;` |
|   ! 0 | 1496 | `				sThis.x.pOther = pExec->pFrame->pThis;` |
|   ! 0 | 1497 | `				MemObjSetType(&sThis, MEMOBJ_OBJ);` |
|   ! 0 | 1498 | `				if( pKey ){` |
|   ! 0 | 1499 | `					ph7_value_string(pKey, "this", 4);` |
|   ! 0 | 1500 | `					ph7_array_add_elem(pInfo, pKey, &sThis); /* copies (takes its own ref) */` |
|   ! 0 | 1501 | `				}` |
|   ! 0 | 1502 | `				PH7_MemObjRelease(&sThis);` |
|   ! 0 | 1503 | `				pThisVal = (ph7_value *)1; /* handled */` |
|   ! 0 | 1504 | `			}` |
|     8 | 1505 | `		}` |
|    17 | 1506 | `		if( pThisVal == 0 ){` |
|    13 | 1507 | `			ReflectMapAddNull(pCtx, pInfo, "this");` |
|    11 | 1508 | `		}else if( pThisVal != (ph7_value *)1 ){` |
|     5 | 1509 | `			ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|     5 | 1510 | `			if( pKey ){` |
|     5 | 1511 | `				ph7_value_string(pKey, "this", 4);` |
|     5 | 1512 | `				ph7_array_add_elem(pInfo, pKey, pThisVal);` |
|     2 | 1513 | `			}` |
|     2 | 1514 | `		}` |
|     - | 1515 | `	}` |
|    17 | 1516 | `	ph7_result_value(pCtx, pInfo);` |
|    17 | 1517 | `	return PH7_OK;` |
|     9 | 1518 | `}` |
|     - | 1519 | `/*` |
|     - | 1520 | ` * Generator __reflect_gen_exec(Generator $g)` |
|     - | 1521 | `` * Follow `yield from` delegation to the innermost executing generator`` |
|     - | 1522 | ` * (PHP's ReflectionGenerator::getExecutingGenerator).` |
|     - | 1523 | ` */` |
|     4 | 1524 | `static int vm_builtin_reflect_gen_exec(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1525 | `{` |
|     5 | 1526 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1527 | `	ph7_generator *pGen;` |
|     - | 1528 | `	ph7_value *pCur;` |
|     5 | 1529 | `	int iDepth = 0;` |
|     5 | 1530 | `	if( nArg < 1 \|\| (pGen = ReflectGeneratorCtx(pVm, apArg[0])) == 0 ){` |
|   ! 0 | 1531 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1532 | `		return PH7_OK;` |
|     - | 1533 | `	}` |
|     5 | 1534 | `	pCur = apArg[0];` |
|     9 | 1535 | `	while( pGen && pGen->pCtx && pGen->pCtx->iDelegateState == 3` |
|     8 | 1536 | `	 && iDepth <= REFLECT_WALK_MAX_DEPTH ){` |
|     3 | 1537 | `		ph7_generator *pInner = ReflectGeneratorCtx(pVm, &pGen->pCtx->sDelegate);` |
|     3 | 1538 | `		if( pInner == 0 ){` |
|   ! 0 | 1539 | `			break;` |
|     - | 1540 | `		}` |
|     3 | 1541 | `		pCur = &pGen->pCtx->sDelegate;` |
|     3 | 1542 | `		pGen = pInner;` |
|     3 | 1543 | `		iDepth++;` |
|     1 | 1544 | `	}` |
|     5 | 1545 | `	return ReflectResultExistingObject(pCtx, (ph7_class_instance *)pCur->x.pOther);` |
|     3 | 1546 | `}` |
|     - | 1547 | `/*` |
|     - | 1548 | ` * array\|null __reflect_const_info(string $name)` |
|     - | 1549 | ` * Global-constant descriptor: {value}. Null when undefined. File/origin` |
|     - | 1550 | ` * metadata arrives with the C5 constant-metadata work.` |
|     - | 1551 | ` */` |
|    40 | 1552 | `static int vm_builtin_reflect_const_info(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1553 | `{` |
|    41 | 1554 | `	ph7_vm *pVm = pCtx->pVm;` |
|     - | 1555 | `	SyHashEntry *pEntry;` |
|     - | 1556 | `	ph7_constant *pCons;` |
|     - | 1557 | `	ph7_value *pInfo;` |
|     - | 1558 | `	ph7_value sValue;` |
|     - | 1559 | `	const char *zName;` |
|     - | 1560 | `	int nLen;` |
|    41 | 1561 | `	if( nArg < 1 ){` |
|   ! 0 | 1562 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1563 | `		return PH7_OK;` |
|     - | 1564 | `	}` |
|    41 | 1565 | `	zName = ph7_value_to_string(apArg[0], &nLen);` |
|    41 | 1566 | `	pEntry = nLen > 0 ? SyHashGet(&pVm->hConstant, (const void *)zName, (sxu32)nLen) : 0;` |
|    41 | 1567 | `	if( pEntry == 0 ){` |
|     3 | 1568 | `		ph7_result_null(pCtx);` |
|     3 | 1569 | `		return PH7_OK;` |
|     - | 1570 | `	}` |
|    39 | 1571 | `	pCons = (ph7_constant *)pEntry->pUserData;` |
|    39 | 1572 | `	pInfo = ph7_context_new_array(pCtx);` |
|    39 | 1573 | `	if( pInfo == 0 ){` |
|   ! 0 | 1574 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1575 | `		return PH7_OK;` |
|     - | 1576 | `	}` |
|    39 | 1577 | `	PH7_MemObjInit(pVm, &sValue);` |
|    39 | 1578 | `	if( pCons->xExpand ){` |
|    39 | 1579 | `		pCons->xExpand(&sValue, pCons->pUserData);` |
|    19 | 1580 | `	}` |
|     - | 1581 | `	{` |
|    39 | 1582 | `		ph7_value *pKey = ph7_context_new_scalar(pCtx);` |
|    39 | 1583 | `		if( pKey ){` |
|    39 | 1584 | `			ph7_value_string(pKey, "value", 5);` |
|    39 | 1585 | `			ph7_array_add_elem(pInfo, pKey, &sValue);` |
|    19 | 1586 | `		}` |
|     - | 1587 | `	}` |
|    39 | 1588 | `	PH7_MemObjRelease(&sValue);` |
|    39 | 1589 | `	ReflectMapAddBool(pCtx, pInfo, "internal", pCons->bUserDefined == 0);` |
|    39 | 1590 | `	if( SyStringLength(&pCons->sFile) > 0 ){` |
|    25 | 1591 | `		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));` |
|    13 | 1592 | `	}else{` |
|    15 | 1593 | `		ReflectMapAddBool(pCtx, pInfo, "file", 0);` |
|     - | 1594 | `	}` |
|    39 | 1595 | `	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pCons->nLine);` |
|    39 | 1596 | `	ReflectMapAddAttrs(pCtx, pInfo, &pCons->aAttrs);` |
|    39 | 1597 | `	ph7_result_value(pCtx, pInfo);` |
|    39 | 1598 | `	return PH7_OK;` |
|    21 | 1599 | `}` |
|     - | 1600 | `/*` |
|     - | 1601 | ` * int\|null __reflect_ref_id(array $arr, int\|string $key)` |
|     - | 1602 | ` * The element's slot index when the element is a reference (its slot has` |
|     - | 1603 | ` * a reference-table record with at least two links), null otherwise.` |
|     - | 1604 | ` */` |
|     6 | 1605 | `static int vm_builtin_reflect_ref_id(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1606 | `{` |
|     - | 1607 | `	ph7_hashmap *pMap;` |
|     7 | 1608 | `	ph7_hashmap_node *pNode = 0;` |
|     7 | 1609 | `	if( nArg < 2 \|\| !ph7_value_is_array(apArg[0]) ){` |
|   ! 0 | 1610 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1611 | `		return PH7_OK;` |
|     - | 1612 | `	}` |
|     7 | 1613 | `	pMap = (ph7_hashmap *)apArg[0]->x.pOther;` |
|     7 | 1614 | `	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK \|\| pNode == 0 ){` |
|   ! 0 | 1615 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1616 | `		return PH7_OK;` |
|     - | 1617 | `	}` |
|     7 | 1618 | `	if( PH7_VmSlotRefCount(pCtx->pVm, pNode->nValIdx) < 2 ){` |
|     3 | 1619 | `		ph7_result_null(pCtx);` |
|     3 | 1620 | `		return PH7_OK;` |
|     - | 1621 | `	}` |
|     5 | 1622 | `	ph7_result_int64(pCtx, (sxi64)pNode->nValIdx);` |
|     5 | 1623 | `	return PH7_OK;` |
|     4 | 1624 | `}` |
|     - | 1625 | `/*` |
|     - | 1626 | ` * array\|null __reflect_attr_args(string $kind, mixed $target, ?string $member,` |
|     - | 1627 | ` *                                int $paramIdx, int $attrIdx)` |
|     - | 1628 | ` * Evaluate the recorded argument expressions of one declared attribute:` |
|     - | 1629 | ` * kind 'class' (target = class), 'attr' (class + property/constant name),` |
|     - | 1630 | ` * 'method' (class + method), 'fn' (function name or Closure), 'param'` |
|     - | 1631 | ` * (function spec + parameter index). Named arguments become string keys.` |
|     - | 1632 | ` */` |
|    52 | 1633 | `static int vm_builtin_reflect_attr_args(ph7_context *pCtx, int nArg, ph7_value **apArg)` |
|     1 | 1634 | `{` |
|    53 | 1635 | `	ph7_vm *pVm = pCtx->pVm;` |
|    53 | 1636 | `	SySet *pAttrs = 0;` |
|     - | 1637 | `	ph7_attribute *pAttrRec;` |
|     - | 1638 | `	ph7_value *pOut;` |
|     - | 1639 | `	const char *zKind;` |
|     - | 1640 | `	int nKind;` |
|     - | 1641 | `	sxu32 nAttrIdx, n;` |
|    53 | 1642 | `	if( nArg < 5 ){` |
|   ! 0 | 1643 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1644 | `		return PH7_OK;` |
|     - | 1645 | `	}` |
|    53 | 1646 | `	zKind = ph7_value_to_string(apArg[0], &nKind);` |
|    53 | 1647 | `	nAttrIdx = (sxu32)ph7_value_to_int(apArg[4]);` |
|    70 | 1648 | `	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){` |
|    35 | 1649 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|    35 | 1650 | `		if( pClass ){ pAttrs = &pClass->aAttrs; }` |
|    38 | 1651 | `	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){` |
|     5 | 1652 | `		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);` |
|     5 | 1653 | `		ph7_class_attr *pMember = pClass ? ReflectFetchAttr(pClass, apArg[2]) : 0;` |
|     5 | 1654 | `		if( pMember ){ pAttrs = &pMember->aAttrs; }` |
|    18 | 1655 | `	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){` |
|     3 | 1656 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1657 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|    18 | 1658 | `	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){` |
|     9 | 1659 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], 0, 0, 0, 0, 0);` |
|     9 | 1660 | `		if( pFunc ){ pAttrs = &pFunc->aAttrs; }` |
|    10 | 1661 | `	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){` |
|     3 | 1662 | `		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);` |
|     3 | 1663 | `		ph7_vm_func_arg *pParam = pFunc` |
|     2 | 1664 | `			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;` |
|     3 | 1665 | `		if( pParam ){ pAttrs = &pParam->aAttrs; }` |
|     4 | 1666 | `	}else if( nKind == 5 && SyMemcmp(zKind, "const", 5) == 0 ){` |
|     - | 1667 | ``		/* Global constant (php 8.5 attributes on `const` statements) */`` |
|     - | 1668 | `		const char *zCName;` |
|     - | 1669 | `		int nCName;` |
|     - | 1670 | `		SyHashEntry *pCEntry;` |
|     3 | 1671 | `		zCName = ph7_value_to_string(apArg[1], &nCName);` |
|     3 | 1672 | `		pCEntry = nCName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zCName, (sxu32)nCName) : 0;` |
|     3 | 1673 | `		if( pCEntry ){ pAttrs = &((ph7_constant *)pCEntry->pUserData)->aAttrs; }` |
|     1 | 1674 | `	}` |
|    52 | 1675 | `	if( pAttrs == 0 \|\| (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0` |
|    53 | 1676 | `	 \|\| (pOut = ph7_context_new_array(pCtx)) == 0 ){` |
|   ! 0 | 1677 | `		ph7_result_null(pCtx);` |
|   ! 0 | 1678 | `		return PH7_OK;` |
|     - | 1679 | `	}` |
|    99 | 1680 | `	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){` |
|    47 | 1681 | `		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);` |
|     - | 1682 | `		ph7_value sValue;` |
|    47 | 1683 | `		PH7_MemObjInit(pVm, &sValue);` |
|    47 | 1684 | `		if( SySetUsed(&pArgRec->aByteCode) > 0 ){` |
|    47 | 1685 | `			VmLocalExec(pVm, &pArgRec->aByteCode, &sValue, FALSE);` |
|    23 | 1686 | `		}` |
|    47 | 1687 | `		if( SyStringLength(&pArgRec->sName) > 0 ){` |
|     5 | 1688 | `			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);` |
|     3 | 1689 | `		}else{` |
|    43 | 1690 | `			ph7_array_add_elem(pOut, 0, &sValue);` |
|     - | 1691 | `		}` |
|    47 | 1692 | `		PH7_MemObjRelease(&sValue);` |
|    24 | 1693 | `	}` |
|    53 | 1694 | `	ph7_result_value(pCtx, pOut);` |
|    53 | 1695 | `	return PH7_OK;` |
|    27 | 1696 | `}` |
|     - | 1697 | `/*` |
|     - | 1698 | ` * The Reflection classes, in PHP. Chunk 1: exceptions, Reflector,` |
|     - | 1699 | ` * Reflection, ReflectionClass, ReflectionObject (plus get_debug_type,` |
|     - | 1700 | ` * which the TypeError messages need and PHP 8.0 ships natively).` |
|     - | 1701 | ` */` |
|     - | 1702 | `static const char zReflectLib1[] =` |
|     - | 1703 | `"function get_debug_type($value){"` |
|     - | 1704 | `" if(is_object($value)){ return get_class($value); }"` |
|     - | 1705 | `" if(is_bool($value)){ return 'bool'; }"` |
|     - | 1706 | `" if(is_int($value)){ return 'int'; }"` |
|     - | 1707 | `" if(is_float($value)){ return 'float'; }"` |
|     - | 1708 | `" if(is_string($value)){ return 'string'; }"` |
|     - | 1709 | `" if(is_array($value)){ return 'array'; }"` |
|     - | 1710 | `" if($value === null){ return 'null'; }"` |
|     - | 1711 | `" return gettype($value);"` |
|     - | 1712 | `"}"` |
|     - | 1713 | `"interface Reflector extends Stringable {}"` |
|     - | 1714 | `"class ReflectionException extends Exception {}"` |
|     - | 1715 | `"class Reflection {"` |
|     - | 1716 | `" public static function getModifierNames($modifiers){"` |
|     - | 1717 | `"  $names = array();"` |
|     - | 1718 | `"  if($modifiers & 64){ $names[] = 'abstract'; }"` |
|     - | 1719 | `"  if($modifiers & 32){ $names[] = 'final'; }"` |
|     - | 1720 | `"  if($modifiers & 1){ $names[] = 'public'; }"` |
|     - | 1721 | `"  if($modifiers & 2){ $names[] = 'protected'; }"` |
|     - | 1722 | `"  if($modifiers & 4){ $names[] = 'private'; }"` |
|     - | 1723 | `"  if($modifiers & 16){ $names[] = 'static'; }"` |
|     - | 1724 | `"  if($modifiers & 128){ $names[] = 'readonly'; }"` |
|     - | 1725 | `"  return $names;"` |
|     - | 1726 | `" }"` |
|     - | 1727 | `"}"` |
|     - | 1728 | `"class ReflectionClass implements Reflector {"` |
|     - | 1729 | `" const IS_IMPLICIT_ABSTRACT = 16;"` |
|     - | 1730 | `" const IS_EXPLICIT_ABSTRACT = 64;"` |
|     - | 1731 | `" const IS_FINAL = 32;"` |
|     - | 1732 | `" const IS_READONLY = 65536;"` |
|     - | 1733 | `" const SKIP_INITIALIZATION_ON_SERIALIZE = 8;"` |
|     - | 1734 | `" const SKIP_DESTRUCTOR = 16;"` |
|     - | 1735 | `" public $name;"` |
|     - | 1736 | `" protected $__obj = null;"` |
|     - | 1737 | `" public function __construct($objectOrClass){"` |
|     - | 1738 | `"  if(!is_object($objectOrClass) && !is_string($objectOrClass)){"` |
|     - | 1739 | `"   if(is_int($objectOrClass) \|\| is_float($objectOrClass) \|\| is_bool($objectOrClass)){"` |
|     - | 1740 | `"    $objectOrClass = (string)$objectOrClass;"` |
|     - | 1741 | `"   }else{"` |
|     - | 1742 | `"    throw new TypeError('ReflectionClass::__construct(): Argument #1 ($objectOrClass) must be of type object\|string, '.get_debug_type($objectOrClass).' given');"` |
|     - | 1743 | `"   }"` |
|     - | 1744 | `"  }"` |
|     - | 1745 | `"  $info = __reflect_class_info($objectOrClass);"` |
|     - | 1746 | `"  if($info === null){"` |
|     - | 1747 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 1748 | `"  }"` |
|     - | 1749 | `"  $this->name = $info['name'];"` |
|     - | 1750 | `" }"` |
|     - | 1751 | `" protected function __rinfo(){ return __reflect_class_info($this->name); }"` |
|     - | 1752 | `" public function getName(){ return $this->name; }"` |
|     - | 1753 | `" public function getShortName(){"` |
|     - | 1754 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1755 | `"  if($p === false){ return $this->name; }"` |
|     - | 1756 | `"  return substr($this->name,$p+1);"` |
|     - | 1757 | `" }"` |
|     - | 1758 | `" public function getNamespaceName(){"` |
|     - | 1759 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 1760 | `"  if($p === false){ return ''; }"` |
|     - | 1761 | `"  return substr($this->name,0,$p);"` |
|     - | 1762 | `" }"` |
|     - | 1763 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 1764 | `" public function isInternal(){ $i = $this->__rinfo(); return $i['internal']; }"` |
|     - | 1765 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 1766 | `" public function isInterface(){ $i = $this->__rinfo(); return $i['interface']; }"` |
|     - | 1767 | `" public function isTrait(){ $i = $this->__rinfo(); return $i['trait']; }"` |
|     - | 1768 | `" public function isAbstract(){ $i = $this->__rinfo(); return $i['abstract']; }"` |
|     - | 1769 | `" public function isFinal(){ $i = $this->__rinfo(); return $i['final']; }"` |
|     - | 1770 | `" public function isReadOnly(){ $i = $this->__rinfo(); return $i['readonly']; }"` |
|     - | 1771 | `" public function isEnum(){ $i = $this->__rinfo(); return $i['enum']; }"` |
|     - | 1772 | `" public function isAnonymous(){ return strpos($this->name,'class@anonymous') === 0; }"` |
|     - | 1773 | `" public function getModifiers(){"` |
|     - | 1774 | `"  $i = $this->__rinfo();"` |
|     - | 1775 | `"  $m = 0;"` |
|     - | 1776 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 1777 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 1778 | `"  if($i['readonly']){ $m \|= 65536; }"` |
|     - | 1779 | `"  return $m;"` |
|     - | 1780 | `" }"` |
|     - | 1781 | `" public function getParentClass(){"` |
|     - | 1782 | `"  $i = $this->__rinfo();"` |
|     - | 1783 | `"  if($i['parent'] === null){ return false; }"` |
|     - | 1784 | `"  return new ReflectionClass($i['parent']);"` |
|     - | 1785 | `" }"` |
|     - | 1786 | `" public function getInterfaceNames(){ $i = $this->__rinfo(); return $i['interfaces']; }"` |
|     - | 1787 | `" public function getInterfaces(){"` |
|     - | 1788 | `"  $i = $this->__rinfo();"` |
|     - | 1789 | `"  $out = array();"` |
|     - | 1790 | `"  foreach($i['interfaces'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1791 | `"  return $out;"` |
|     - | 1792 | `" }"` |
|     - | 1793 | `" public function getTraitNames(){ $i = $this->__rinfo(); return $i['traits']; }"` |
|     - | 1794 | `" public function getTraits(){"` |
|     - | 1795 | `"  $i = $this->__rinfo();"` |
|     - | 1796 | `"  $out = array();"` |
|     - | 1797 | `"  foreach($i['traits'] as $n){ $out[$n] = new ReflectionClass($n); }"` |
|     - | 1798 | `"  return $out;"` |
|     - | 1799 | `" }"` |
|     - | 1800 | `" public function getTraitAliases(){ return array(); }"` |
|     - | 1801 | `" public function implementsInterface($interface){"` |
|     - | 1802 | `"  if($interface instanceof ReflectionClass){ $interface = $interface->name; }"` |
|     - | 1803 | `"  $target = __reflect_class_info($interface);"` |
|     - | 1804 | `"  if($target === null){"` |
|     - | 1805 | `"   throw new ReflectionException('Interface \"'.$interface.'\" does not exist');"` |
|     - | 1806 | `"  }"` |
|     - | 1807 | `"  if(!$target['interface']){"` |
|     - | 1808 | `"   throw new ReflectionException($target['name'].' is not an interface');"` |
|     - | 1809 | `"  }"` |
|     - | 1810 | `"  $name = $target['name'];"` |
|     - | 1811 | `"  if($this->name === $name){ return true; }"` |
|     - | 1812 | `"  $i = $this->__rinfo();"` |
|     - | 1813 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1814 | `"  return false;"` |
|     - | 1815 | `" }"` |
|     - | 1816 | `" public function isSubclassOf($class){"` |
|     - | 1817 | `"  if($class instanceof ReflectionClass){ $class = $class->name; }"` |
|     - | 1818 | `"  $target = __reflect_class_info($class);"` |
|     - | 1819 | `"  if($target === null){"` |
|     - | 1820 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 1821 | `"  }"` |
|     - | 1822 | `"  $name = $target['name'];"` |
|     - | 1823 | `"  if($name === $this->name){ return false; }"` |
|     - | 1824 | `"  $i = $this->__rinfo();"` |
|     - | 1825 | `"  $p = $i['parent'];"` |
|     - | 1826 | `"  while($p !== null){"` |
|     - | 1827 | `"   if($p === $name){ return true; }"` |
|     - | 1828 | `"   $pi = __reflect_class_info($p);"` |
|     - | 1829 | `"   $p = $pi['parent'];"` |
|     - | 1830 | `"  }"` |
|     - | 1831 | `"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"` |
|     - | 1832 | `"  return false;"` |
|     - | 1833 | `" }"` |
|     - | 1834 | `" public function isInstance($object){"` |
|     - | 1835 | `"  if(!is_object($object)){"` |
|     - | 1836 | `"   throw new TypeError('ReflectionClass::isInstance(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 1837 | `"  }"` |
|     - | 1838 | `"  return is_a($object,$this->name);"` |
|     - | 1839 | `" }"` |
|     - | 1840 | `" public function hasMethod($name){"` |
|     - | 1841 | `"  $i = $this->__rinfo();"` |
|     - | 1842 | `"  $l = strtolower($name);"` |
|     - | 1843 | `"  foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ return true; } }"` |
|     - | 1844 | `"  return false;"` |
|     - | 1845 | `" }"` |
|     - | 1846 | `" public function hasProperty($name){"` |
|     - | 1847 | `"  $i = $this->__rinfo();"` |
|     - | 1848 | `"  if(isset($i['props'][$name])){ return true; }"` |
|     - | 1849 | `"  if($this->__obj !== null){ return (__reflect_prop_state($this->__obj, $name) & 1) !== 0; }"` |
|     - | 1850 | `"  return false;"` |
|     - | 1851 | `" }"` |
|     - | 1852 | `" public function hasConstant($name){ $i = $this->__rinfo(); return isset($i['consts'][$name]); }"` |
|     - | 1853 | `" public function getConstant($name){"` |
|     - | 1854 | `"  $i = $this->__rinfo();"` |
|     - | 1855 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 1856 | `"  return __reflect_const_value($this->name,$name);"` |
|     - | 1857 | `" }"` |
|     - | 1858 | `" public function getConstants($filter = null){"` |
|     - | 1859 | `"  $i = $this->__rinfo();"` |
|     - | 1860 | `"  $out = array();"` |
|     - | 1861 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 1862 | `"   if($filter !== null){"` |
|     - | 1863 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 1864 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 1865 | `"   }"` |
|     - | 1866 | `"   $out[$k] = __reflect_const_value($this->name,$k);"` |
|     - | 1867 | `"  }"` |
|     - | 1868 | `"  return $out;"` |
|     - | 1869 | `" }"` |
|     - | 1870 | `" public function getStartLine(){"` |
|     - | 1871 | `"  $i = $this->__rinfo();"` |
|     - | 1872 | `"  if($i['internal']){ return false; }"` |
|     - | 1873 | `"  return $i['line'];"` |
|     - | 1874 | `" }"` |
|     - | 1875 | `" public function getEndLine(){"` |
|     - | 1876 | `"  $i = $this->__rinfo();"` |
|     - | 1877 | `"  if($i['internal']){ return false; }"` |
|     - | 1878 | `"  return $i['endline'];"` |
|     - | 1879 | `" }"` |
|     - | 1880 | `" public function getFileName(){ $i = $this->__rinfo(); return $i['file']; }"` |
|     - | 1881 | `" public function getDocComment(){ $i = $this->__rinfo(); return $i['doc']; }"` |
|     - | 1882 | `" public function isInstantiable(){"` |
|     - | 1883 | `"  $i = $this->__rinfo();"` |
|     - | 1884 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract'] \|\| $i['enum']){ return false; }"` |
|     - | 1885 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){ return false; }"` |
|     - | 1886 | `"  return true;"` |
|     - | 1887 | `" }"` |
|     - | 1888 | `" public function isCloneable(){"` |
|     - | 1889 | `"  $i = $this->__rinfo();"` |
|     - | 1890 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1891 | `"  if($i['clonevis'] !== 0 && $i['clonevis'] !== 1){ return false; }"` |
|     - | 1892 | `"  return true;"` |
|     - | 1893 | `" }"` |
|     - | 1894 | `" public function isIterable(){"` |
|     - | 1895 | `"  $i = $this->__rinfo();"` |
|     - | 1896 | `"  if($i['interface'] \|\| $i['trait'] \|\| $i['abstract']){ return false; }"` |
|     - | 1897 | `"  return $i['iterable'];"` |
|     - | 1898 | `" }"` |
|     - | 1899 | `" public function isIterateable(){ return $this->isIterable(); }"` |
|     - | 1900 | `" public function newInstance(...$args){ return $this->__rnew($args); }"` |
|     - | 1901 | `" public function newInstanceArgs(array $args = array()){ return $this->__rnew($args); }"` |
|     - | 1902 | `" protected function __rnew($args){"` |
|     - | 1903 | `"  $i = $this->__rinfo();"` |
|     - | 1904 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1905 | `"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){"` |
|     - | 1906 | `"   throw new ReflectionException('Access to non-public constructor of class '.$this->name);"` |
|     - | 1907 | `"  }"` |
|     - | 1908 | `"  if($i['ctorvis'] === 0 && count($args) > 0){"` |
|     - | 1909 | `"   throw new ReflectionException('Class '.$this->name.' does not have a constructor, so you cannot pass any constructor arguments');"` |
|     - | 1910 | `"  }"` |
|     - | 1911 | `"  return __reflect_new_instance($this->name,$args);"` |
|     - | 1912 | `" }"` |
|     - | 1913 | `" protected function __rcheckInstantiable($i){"` |
|     - | 1914 | `"  if($i['interface']){ throw new Error('Cannot instantiate interface '.$this->name); }"` |
|     - | 1915 | `"  if($i['trait']){ throw new Error('Cannot instantiate trait '.$this->name); }"` |
|     - | 1916 | `"  if($i['abstract']){ throw new Error('Cannot instantiate abstract class '.$this->name); }"` |
|     - | 1917 | `" }"` |
|     - | 1918 | `" public function newInstanceWithoutConstructor(){"` |
|     - | 1919 | `"  $i = $this->__rinfo();"` |
|     - | 1920 | `"  $this->__rcheckInstantiable($i);"` |
|     - | 1921 | `"  return __reflect_new_no_ctor($this->name);"` |
|     - | 1922 | `" }"` |
|     - | 1923 | `" public function getStaticProperties(){"` |
|     - | 1924 | `"  $i = $this->__rinfo();"` |
|     - | 1925 | `"  $out = array();"` |
|     - | 1926 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1927 | `"   if($p['static']){ $out[$k] = __reflect_static_value($this->name,$k); }"` |
|     - | 1928 | `"  }"` |
|     - | 1929 | `"  return $out;"` |
|     - | 1930 | `" }"` |
|     - | 1931 | `" public function getStaticPropertyValue($name, ...$def){"` |
|     - | 1932 | `"  $i = $this->__rinfo();"` |
|     - | 1933 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 1934 | `"   if(count($def) > 0){ return $def[0]; }"` |
|     - | 1935 | `"   throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 1936 | `"  }"` |
|     - | 1937 | `"  return __reflect_static_value($this->name,$name);"` |
|     - | 1938 | `" }"` |
|     - | 1939 | `" public function setStaticPropertyValue($name,$value){"` |
|     - | 1940 | `"  $i = $this->__rinfo();"` |
|     - | 1941 | `"  if(!isset($i['props'][$name]) \|\| !$i['props'][$name]['static']){"` |
|     - | 1942 | `"   throw new ReflectionException('Class '.$this->name.' does not have a property named '.$name);"` |
|     - | 1943 | `"  }"` |
|     - | 1944 | `"  __reflect_static_set($this->name,$name,$value);"` |
|     - | 1945 | `" }"` |
|     - | 1946 | `" public function getDefaultProperties(){"` |
|     - | 1947 | `"  $i = $this->__rinfo();"` |
|     - | 1948 | `"  $out = array();"` |
|     - | 1949 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1950 | `"   if($p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 1951 | `"  }"` |
|     - | 1952 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1953 | `"   if(!$p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"` |
|     - | 1954 | `"  }"` |
|     - | 1955 | `"  return $out;"` |
|     - | 1956 | `" }"` |
|     - | 1957 | `" public function getProperty($name){"` |
|     - | 1958 | `"  $i = $this->__rinfo();"` |
|     - | 1959 | `"  if(isset($i['props'][$name])){"` |
|     - | 1960 | `"   return new ReflectionProperty($this->name, $name);"` |
|     - | 1961 | `"  }"` |
|     - | 1962 | `"  if($this->__obj !== null && (__reflect_prop_state($this->__obj, $name) & 1)){"` |
|     - | 1963 | `"   return new ReflectionProperty($this->__obj, $name);"` |
|     - | 1964 | `"  }"` |
|     - | 1965 | `"  throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"` |
|     - | 1966 | `" }"` |
|     - | 1967 | `" public function getProperties($filter = null){"` |
|     - | 1968 | `"  $i = $this->__rinfo();"` |
|     - | 1969 | `"  $out = array();"` |
|     - | 1970 | `"  foreach($i['props'] as $k => $p){"` |
|     - | 1971 | `"   if($filter !== null){"` |
|     - | 1972 | `"    $m = ($p['vis'] === 1 ? 1 : ($p['vis'] === 2 ? 2 : 4));"` |
|     - | 1973 | `"    if($p['static']){ $m \|= 16; }"` |
|     - | 1974 | `"    if($p['readonly']){ $m \|= 128; }"` |
|     - | 1975 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 1976 | `"   }"` |
|     - | 1977 | `"   $out[] = new ReflectionProperty($this->name, $k);"` |
|     - | 1978 | `"  }"` |
|     - | 1979 | `"  if($this->__obj !== null){"` |
|     - | 1980 | `"   foreach(__reflect_dyn_props($this->__obj) as $k){"` |
|     - | 1981 | `"    if(isset($i['props'][$k])){ continue; }"` |
|     - | 1982 | `"    if($filter !== null && ($filter & 1) === 0){ continue; }"` |
|     - | 1983 | `"    $out[] = new ReflectionProperty($this->__obj, $k);"` |
|     - | 1984 | `"   }"` |
|     - | 1985 | `"  }"` |
|     - | 1986 | `"  return $out;"` |
|     - | 1987 | `" }"` |
|     - | 1988 | `" public function getMethod($name){"` |
|     - | 1989 | `"  $i = $this->__rinfo();"` |
|     - | 1990 | `"  $found = null;"` |
|     - | 1991 | `"  if(isset($i['methods'][$name])){"` |
|     - | 1992 | `"   $found = $name;"` |
|     - | 1993 | `"  }else{"` |
|     - | 1994 | `"   $l = strtolower($name);"` |
|     - | 1995 | `"   foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ $found = $k; break; } }"` |
|     - | 1996 | `"  }"` |
|     - | 1997 | `"  if($found === null){"` |
|     - | 1998 | `"   throw new ReflectionException('Method '.$this->name.'::'.$name.'() does not exist');"` |
|     - | 1999 | `"  }"` |
|     - | 2000 | `"  return new ReflectionMethod($this->name, $found);"` |
|     - | 2001 | `" }"` |
|     - | 2002 | `" public function getMethods($filter = null){"` |
|     - | 2003 | `"  $i = $this->__rinfo();"` |
|     - | 2004 | `"  $out = array();"` |
|     - | 2005 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 2006 | `"   if($filter !== null){"` |
|     - | 2007 | `"    $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2008 | `"    if($m['static']){ $mod \|= 16; }"` |
|     - | 2009 | `"    if($m['abstract']){ $mod \|= 64; }"` |
|     - | 2010 | `"    if($m['final']){ $mod \|= 32; }"` |
|     - | 2011 | `"    if(($mod & $filter) === 0){ continue; }"` |
|     - | 2012 | `"   }"` |
|     - | 2013 | `"   $out[] = new ReflectionMethod($this->name, $k);"` |
|     - | 2014 | `"  }"` |
|     - | 2015 | `"  return $out;"` |
|     - | 2016 | `" }"` |
|     - | 2017 | `" public function getConstructor(){"` |
|     - | 2018 | `"  $i = $this->__rinfo();"` |
|     - | 2019 | `"  if(isset($i['methods']['__construct'])){"` |
|     - | 2020 | `"   return new ReflectionMethod($this->name, '__construct');"` |
|     - | 2021 | `"  }"` |
|     - | 2022 | `"  foreach($i['methods'] as $k => $m){"` |
|     - | 2023 | `"   if(strtolower($k) === '__construct'){ return new ReflectionMethod($this->name, $k); }"` |
|     - | 2024 | `"  }"` |
|     - | 2025 | `"  if($i['ctorvis'] !== 0 && isset($i['methods'][$this->name])){"` |
|     - | 2026 | `"   return new ReflectionMethod($this->name, $this->name);"` |
|     - | 2027 | `"  }"` |
|     - | 2028 | `"  return null;"` |
|     - | 2029 | `" }"` |
|     - | 2030 | `" public function getReflectionConstant($name){"` |
|     - | 2031 | `"  $i = $this->__rinfo();"` |
|     - | 2032 | `"  if(!isset($i['consts'][$name])){ return false; }"` |
|     - | 2033 | `"  return new ReflectionClassConstant($this->name, $name);"` |
|     - | 2034 | `" }"` |
|     - | 2035 | `" public function getReflectionConstants($filter = null){"` |
|     - | 2036 | `"  $i = $this->__rinfo();"` |
|     - | 2037 | `"  $out = array();"` |
|     - | 2038 | `"  foreach($i['consts'] as $k => $c){"` |
|     - | 2039 | `"   if($filter !== null){"` |
|     - | 2040 | `"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"` |
|     - | 2041 | `"    if($c['final']){ $m \|= 32; }"` |
|     - | 2042 | `"    if(($m & $filter) === 0){ continue; }"` |
|     - | 2043 | `"   }"` |
|     - | 2044 | `"   $out[] = new ReflectionClassConstant($this->name, $k);"` |
|     - | 2045 | `"  }"` |
|     - | 2046 | `"  return $out;"` |
|     - | 2047 | `" }"` |
|     - | 2048 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2049 | `"  $i = $this->__rinfo();"` |
|     - | 2050 | `"  return __reflect_build_attrs($i['attrs'], array('class', $this->name, null, 0), 1, $name, $flags);"` |
|     - | 2051 | `" }"` |
|     - | 2052 | `" public function getExtensionName(){ $i = $this->__rinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2053 | `" public function getExtension(){ $i = $this->__rinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2054 | `" public function newLazyGhost($initializer, $options = 0){"` |
|     - | 2055 | `"  throw new Error('ReflectionClass::newLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2056 | `" }"` |
|     - | 2057 | `" public function newLazyProxy($factory, $options = 0){"` |
|     - | 2058 | `"  throw new Error('ReflectionClass::newLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2059 | `" }"` |
|     - | 2060 | `" public function resetAsLazyGhost($object, $initializer, $options = 0){"` |
|     - | 2061 | `"  throw new Error('ReflectionClass::resetAsLazyGhost() is not supported by PHL (no lazy objects)');"` |
|     - | 2062 | `" }"` |
|     - | 2063 | `" public function resetAsLazyProxy($object, $factory, $options = 0){"` |
|     - | 2064 | `"  throw new Error('ReflectionClass::resetAsLazyProxy() is not supported by PHL (no lazy objects)');"` |
|     - | 2065 | `" }"` |
|     - | 2066 | `" public function getLazyInitializer($object){ return null; }"` |
|     - | 2067 | `" public function initializeLazyObject($object){ return $object; }"` |
|     - | 2068 | `" public function markLazyObjectAsInitialized($object){ return $object; }"` |
|     - | 2069 | `" public function isUninitializedLazyObject($object){ return false; }"` |
|     - | 2070 | `" public function __toString(){ return __reflect_export_class($this); }"` |
|     - | 2071 | `"}"` |
|     - | 2072 | `"class ReflectionObject extends ReflectionClass {"` |
|     - | 2073 | `" public function __construct($object){"` |
|     - | 2074 | `"  if(!is_object($object)){"` |
|     - | 2075 | `"   throw new TypeError('ReflectionObject::__construct(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"` |
|     - | 2076 | `"  }"` |
|     - | 2077 | `"  parent::__construct($object);"` |
|     - | 2078 | `"  $this->__obj = $object;"` |
|     - | 2079 | `" }"` |
|     - | 2080 | `"}"` |
|     - | 2081 | `;` |
|     - | 2082 | `/*` |
|     - | 2083 | ` * Chunk 2: ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,` |
|     - | 2084 | ` * ReflectionParameter.` |
|     - | 2085 | ` */` |
|     - | 2086 | `static const char zReflectLib2[] =` |
|     - | 2087 | `"abstract class ReflectionFunctionAbstract implements Reflector {"` |
|     - | 2088 | `" public $name;"` |
|     - | 2089 | `" protected $__cl = null;"` |
|     - | 2090 | `" protected function __rfinfo(){"` |
|     - | 2091 | `"  if($this->__cl !== null){ return __reflect_sig_fixup(__reflect_func_info($this->__cl)); }"` |
|     - | 2092 | `"  return __reflect_sig_fixup(__reflect_func_info($this->name));"` |
|     - | 2093 | `" }"` |
|     - | 2094 | `" protected function __rftarget(){ return $this->__cl !== null ? $this->__cl : $this->name; }"` |
|     - | 2095 | `" protected function __rpspec(){ return $this->__rftarget(); }"` |
|     - | 2096 | `" public function getName(){ return $this->name; }"` |
|     - | 2097 | `" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"` |
|     - | 2098 | `" public function getNamespaceName(){"` |
|     - | 2099 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2100 | `"  if($p === false){ return ''; }"` |
|     - | 2101 | `"  return substr($this->name,0,$p);"` |
|     - | 2102 | `" }"` |
|     - | 2103 | `" public function getShortName(){"` |
|     - | 2104 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2105 | `"  if($p === false){ return $this->name; }"` |
|     - | 2106 | `"  return substr($this->name,$p+1);"` |
|     - | 2107 | `" }"` |
|     - | 2108 | `" public function isClosure(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2109 | `" public function isGenerator(){ $i = $this->__rfinfo(); return $i['generator']; }"` |
|     - | 2110 | `" public function isVariadic(){ $i = $this->__rfinfo(); return $i['variadic']; }"` |
|     - | 2111 | `" public function returnsReference(){ $i = $this->__rfinfo(); return $i['byref']; }"` |
|     - | 2112 | `" public function isInternal(){ $i = $this->__rfinfo(); return $i['internal']; }"` |
|     - | 2113 | `" public function isUserDefined(){ return !$this->isInternal(); }"` |
|     - | 2114 | `" public function isDeprecated(){ $i = $this->__rfinfo(); return __reflect_has_deprecated($i['attrs']); }"` |
|     - | 2115 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['fstatic']; }"` |
|     - | 2116 | `" public function getFileName(){ $i = $this->__rfinfo(); return $i['file']; }"` |
|     - | 2117 | `" public function getStartLine(){"` |
|     - | 2118 | `"  $i = $this->__rfinfo();"` |
|     - | 2119 | `"  if($i['internal']){ return false; }"` |
|     - | 2120 | `"  return $i['line'];"` |
|     - | 2121 | `" }"` |
|     - | 2122 | `" public function getEndLine(){"` |
|     - | 2123 | `"  $i = $this->__rfinfo();"` |
|     - | 2124 | `"  if($i['internal']){ return false; }"` |
|     - | 2125 | `"  return $i['endline'];"` |
|     - | 2126 | `" }"` |
|     - | 2127 | `" public function getDocComment(){ $i = $this->__rfinfo(); return $i['doc']; }"` |
|     - | 2128 | `" public function hasReturnType(){ $i = $this->__rfinfo(); return $i['rettext'] !== null; }"` |
|     - | 2129 | `" public function getReturnType(){ $i = $this->__rfinfo(); return __reflect_make_type($i['rettext']); }"` |
|     - | 2130 | `" public function hasTentativeReturnType(){ return false; }"` |
|     - | 2131 | `" public function getTentativeReturnType(){ return null; }"` |
|     - | 2132 | `" public function getNumberOfParameters(){"` |
|     - | 2133 | `"  $i = $this->__rfinfo();"` |
|     - | 2134 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2135 | `"  return count($i['params']);"` |
|     - | 2136 | `" }"` |
|     - | 2137 | `" public function getNumberOfRequiredParameters(){"` |
|     - | 2138 | `"  $i = $this->__rfinfo();"` |
|     - | 2139 | `"  if($i['minarg'] >= 0){ return $i['minarg']; }"` |
|     - | 2140 | `"  $req = 0;"` |
|     - | 2141 | `"  $n = count($i['params']);"` |
|     - | 2142 | `"  for($k = $n - 1; $k >= 0; $k--){"` |
|     - | 2143 | `"   $p = $i['params'][$k];"` |
|     - | 2144 | `"   if(!$p['variadic'] && !$p['hasdef']){ $req = $k + 1; break; }"` |
|     - | 2145 | `"  }"` |
|     - | 2146 | `"  return $req;"` |
|     - | 2147 | `" }"` |
|     - | 2148 | `" public function getParameters(){"` |
|     - | 2149 | `"  $i = $this->__rfinfo();"` |
|     - | 2150 | `"  $out = array();"` |
|     - | 2151 | `"  $spec = $this->__rpspec();"` |
|     - | 2152 | `"  foreach($i['params'] as $p){"` |
|     - | 2153 | `"   $out[] = new ReflectionParameter($spec, $p['pos']);"` |
|     - | 2154 | `"  }"` |
|     - | 2155 | `"  return $out;"` |
|     - | 2156 | `" }"` |
|     - | 2157 | `" public function getStaticVariables(){ $i = $this->__rfinfo(); return $i['statics']; }"` |
|     - | 2158 | `" public function getClosureThis(){"` |
|     - | 2159 | `"  $i = $this->__rfinfo();"` |
|     - | 2160 | `"  return isset($i['this']) ? $i['this'] : null;"` |
|     - | 2161 | `" }"` |
|     - | 2162 | `" public function getClosureScopeClass(){"` |
|     - | 2163 | `"  $i = $this->__rfinfo();"` |
|     - | 2164 | `"  if(isset($i['scope'])){ return new ReflectionClass($i['scope']); }"` |
|     - | 2165 | `"  if(isset($i['this'])){ return new ReflectionClass(get_class($i['this'])); }"` |
|     - | 2166 | `"  return null;"` |
|     - | 2167 | `" }"` |
|     - | 2168 | `" public function getClosureCalledClass(){ return $this->getClosureScopeClass(); }"` |
|     - | 2169 | `" public function getClosureUsedVariables(){"` |
|     - | 2170 | `"  $i = $this->__rfinfo();"` |
|     - | 2171 | `"  return isset($i['used']) ? $i['used'] : array();"` |
|     - | 2172 | `" }"` |
|     - | 2173 | `" public function getExtensionName(){ $i = $this->__rfinfo(); return $i['internal'] ? 'Core' : false; }"` |
|     - | 2174 | `" public function getExtension(){ $i = $this->__rfinfo(); return $i['internal'] ? new ReflectionExtension('Core') : null; }"` |
|     - | 2175 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2176 | `"  $i = $this->__rfinfo();"` |
|     - | 2177 | `"  if($this instanceof ReflectionMethod){"` |
|     - | 2178 | `"   $spec = array('method', $this->class, $this->name, 0);"` |
|     - | 2179 | `"   $target = 4;"` |
|     - | 2180 | `"  }else{"` |
|     - | 2181 | `"   $spec = array('fn', $this->__rftarget(), null, 0);"` |
|     - | 2182 | `"   $target = 2;"` |
|     - | 2183 | `"  }"` |
|     - | 2184 | `"  return __reflect_build_attrs($i['attrs'], $spec, $target, $name, $flags);"` |
|     - | 2185 | `" }"` |
|     - | 2186 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2187 | `"}"` |
|     - | 2188 | `"class ReflectionFunction extends ReflectionFunctionAbstract {"` |
|     - | 2189 | `" const IS_DEPRECATED = 2048;"` |
|     - | 2190 | `" public function __construct($function){"` |
|     - | 2191 | `"  if($function instanceof Closure){"` |
|     - | 2192 | `"   $this->__cl = $function;"` |
|     - | 2193 | `"   $i = $this->__rfinfo();"` |
|     - | 2194 | `"   if($i['closure']){"` |
|     - | 2195 | `"    $f = $i['file'] === false ? '' : $i['file'];"` |
|     - | 2196 | `"    $this->name = '{closure:'.$f.':'.$i['line'].'}';"` |
|     - | 2197 | `"   }else{"` |
|     - | 2198 | `"    $this->name = $i['name'];"` |
|     - | 2199 | `"   }"` |
|     - | 2200 | `"   return;"` |
|     - | 2201 | `"  }"` |
|     - | 2202 | `"  if(!is_string($function)){"` |
|     - | 2203 | `"   throw new TypeError('ReflectionFunction::__construct(): Argument #1 ($function) must be of type Closure\|string, '.get_debug_type($function).' given');"` |
|     - | 2204 | `"  }"` |
|     - | 2205 | `"  $i = __reflect_func_info($function);"` |
|     - | 2206 | `"  if($i === null){"` |
|     - | 2207 | `"   throw new ReflectionException('Function '.$function.'() does not exist');"` |
|     - | 2208 | `"  }"` |
|     - | 2209 | `"  if($i['closure']){"` |
|     - | 2210 | `"   $this->name = '{closure:'.($i['file'] === false ? '' : $i['file']).':'.$i['line'].'}';"` |
|     - | 2211 | `"   $this->__cl = __reflect_closure($function, null, null);"` |
|     - | 2212 | `"  }else{"` |
|     - | 2213 | `"   $this->name = $i['name'];"` |
|     - | 2214 | `"  }"` |
|     - | 2215 | `" }"` |
|     - | 2216 | `" public function invoke(...$args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2217 | `" public function invokeArgs(array $args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"` |
|     - | 2218 | `" public function getClosure(){"` |
|     - | 2219 | `"  if($this->__cl !== null){ return $this->__cl; }"` |
|     - | 2220 | `"  return __reflect_closure($this->name, null, null);"` |
|     - | 2221 | `" }"` |
|     - | 2222 | `" public function isAnonymous(){ $i = $this->__rfinfo(); return $i['closure']; }"` |
|     - | 2223 | `" public function isDisabled(){ return false; }"` |
|     - | 2224 | `"}"` |
|     - | 2225 | `"class ReflectionMethod extends ReflectionFunctionAbstract {"` |
|     - | 2226 | `" const IS_PUBLIC = 1;"` |
|     - | 2227 | `" const IS_PROTECTED = 2;"` |
|     - | 2228 | `" const IS_PRIVATE = 4;"` |
|     - | 2229 | `" const IS_STATIC = 16;"` |
|     - | 2230 | `" const IS_FINAL = 32;"` |
|     - | 2231 | `" const IS_ABSTRACT = 64;"` |
|     - | 2232 | `" public $class;"` |
|     - | 2233 | `" public function __construct($objectOrMethod, $method = null){"` |
|     - | 2234 | `"  if($method === null){"` |
|     - | 2235 | `"   if(!is_string($objectOrMethod) \|\| strpos($objectOrMethod,'::') === false){"` |
|     - | 2236 | `"    throw new TypeError('ReflectionMethod::__construct(): Argument #1 ($objectOrMethod) must be of type object\|string, '.get_debug_type($objectOrMethod).' given');"` |
|     - | 2237 | `"   }"` |
|     - | 2238 | `"   $p = strpos($objectOrMethod,'::');"` |
|     - | 2239 | `"   $method = substr($objectOrMethod,$p+2);"` |
|     - | 2240 | `"   $objectOrMethod = substr($objectOrMethod,0,$p);"` |
|     - | 2241 | `"  }"` |
|     - | 2242 | `"  $ci = __reflect_class_info($objectOrMethod);"` |
|     - | 2243 | `"  if($ci === null){"` |
|     - | 2244 | `"   throw new ReflectionException('Class \"'.$objectOrMethod.'\" does not exist');"` |
|     - | 2245 | `"  }"` |
|     - | 2246 | `"  $this->class = $ci['name'];"` |
|     - | 2247 | `"  $found = null;"` |
|     - | 2248 | `"  if(isset($ci['methods'][$method])){"` |
|     - | 2249 | `"   $found = $method;"` |
|     - | 2250 | `"  }else{"` |
|     - | 2251 | `"   $l = strtolower($method);"` |
|     - | 2252 | `"   foreach($ci['methods'] as $k => $m){"` |
|     - | 2253 | `"    if(strtolower($k) === $l){ $found = $k; break; }"` |
|     - | 2254 | `"   }"` |
|     - | 2255 | `"  }"` |
|     - | 2256 | `"  if($found === null){"` |
|     - | 2257 | `"   throw new ReflectionException('Method '.$this->class.'::'.$method.'() does not exist');"` |
|     - | 2258 | `"  }"` |
|     - | 2259 | `"  $this->name = $found;"` |
|     - | 2260 | `" }"` |
|     - | 2261 | `" public static function createFromMethodName($name){"` |
|     - | 2262 | `"  return new ReflectionMethod($name);"` |
|     - | 2263 | `" }"` |
|     - | 2264 | `" protected function __rfinfo(){ return __reflect_func_info($this->class, $this->name); }"` |
|     - | 2265 | `" protected function __rpspec(){ return array($this->class, $this->name); }"` |
|     - | 2266 | `" public function getDeclaringClass(){"` |
|     - | 2267 | `"  $i = $this->__rfinfo();"` |
|     - | 2268 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2269 | `" }"` |
|     - | 2270 | `" public function getModifiers(){"` |
|     - | 2271 | `"  $i = $this->__rfinfo();"` |
|     - | 2272 | `"  $m = ($i['vis'] === 1 ? 1 : ($i['vis'] === 2 ? 2 : 4));"` |
|     - | 2273 | `"  if($i['mstatic']){ $m \|= 16; }"` |
|     - | 2274 | `"  if($i['abstract']){ $m \|= 64; }"` |
|     - | 2275 | `"  if($i['final']){ $m \|= 32; }"` |
|     - | 2276 | `"  return $m;"` |
|     - | 2277 | `" }"` |
|     - | 2278 | `" public function isPublic(){ $i = $this->__rfinfo(); return $i['vis'] === 1; }"` |
|     - | 2279 | `" public function isProtected(){ $i = $this->__rfinfo(); return $i['vis'] === 2; }"` |
|     - | 2280 | `" public function isPrivate(){ $i = $this->__rfinfo(); return $i['vis'] === 3; }"` |
|     - | 2281 | `" public function isStatic(){ $i = $this->__rfinfo(); return $i['mstatic']; }"` |
|     - | 2282 | `" public function isAbstract(){ $i = $this->__rfinfo(); return $i['abstract']; }"` |
|     - | 2283 | `" public function isFinal(){ $i = $this->__rfinfo(); return $i['final']; }"` |
|     - | 2284 | `" public function isConstructor(){ return strtolower($this->name) === '__construct'; }"` |
|     - | 2285 | `" public function isDestructor(){ return strtolower($this->name) === '__destruct'; }"` |
|     - | 2286 | `" public function invoke($object = null, ...$args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2287 | `" public function invokeArgs($object, array $args){ return $this->__rinvoke($object, $args); }"` |
|     - | 2288 | `" protected function __rinvoke($object, $args){"` |
|     - | 2289 | `"  $i = $this->__rfinfo();"` |
|     - | 2290 | `"  if(!$i['mstatic']){"` |
|     - | 2291 | `"   if(!is_object($object)){"` |
|     - | 2292 | `"    throw new ReflectionException('Trying to invoke non static method '.$this->class.'::'.$this->name.'() without an object');"` |
|     - | 2293 | `"   }"` |
|     - | 2294 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2295 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2296 | `"   }"` |
|     - | 2297 | `"  }else{"` |
|     - | 2298 | `"   $object = null;"` |
|     - | 2299 | `"  }"` |
|     - | 2300 | `"  return __reflect_invoke($this->class, $this->name, $object, $args);"` |
|     - | 2301 | `" }"` |
|     - | 2302 | `" public function getClosure($object = null){"` |
|     - | 2303 | `"  $i = $this->__rfinfo();"` |
|     - | 2304 | `"  if(!$i['mstatic']){"` |
|     - | 2305 | `"   if($object === null){"` |
|     - | 2306 | `"    throw new ValueError('ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods');"` |
|     - | 2307 | `"   }"` |
|     - | 2308 | `"   if(!is_a($object, $i['decl'])){"` |
|     - | 2309 | `"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"` |
|     - | 2310 | `"   }"` |
|     - | 2311 | `"  }else{"` |
|     - | 2312 | `"   $object = null;"` |
|     - | 2313 | `"  }"` |
|     - | 2314 | `"  return __reflect_closure($this->class, $this->name, $object);"` |
|     - | 2315 | `" }"` |
|     - | 2316 | `" public function setAccessible($accessible){ }"` |
|     - | 2317 | `" public function hasPrototype(){ return $this->__rproto() !== null; }"` |
|     - | 2318 | `" public function getPrototype(){"` |
|     - | 2319 | `"  $p = $this->__rproto();"` |
|     - | 2320 | `"  if($p === null){"` |
|     - | 2321 | `"   throw new ReflectionException('Method '.$this->class.'::'.$this->name.' does not have a prototype');"` |
|     - | 2322 | `"  }"` |
|     - | 2323 | `"  return new ReflectionMethod($p, $this->name);"` |
|     - | 2324 | `" }"` |
|     - | 2325 | `" protected function __rproto(){"` |
|     - | 2326 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2327 | `"  $l = strtolower($this->name);"` |
|     - | 2328 | `"  $p = $ci['parent'];"` |
|     - | 2329 | `"  while($p !== null){"` |
|     - | 2330 | `"   $pi = __reflect_class_info($p);"` |
|     - | 2331 | `"   foreach($pi['methods'] as $k => $m){"` |
|     - | 2332 | `"    if(strtolower($k) === $l && $m['vis'] !== 3){ return $m['decl']; }"` |
|     - | 2333 | `"   }"` |
|     - | 2334 | `"   $p = $pi['parent'];"` |
|     - | 2335 | `"  }"` |
|     - | 2336 | `"  foreach($ci['interfaces'] as $if){"` |
|     - | 2337 | `"   $ii = __reflect_class_info($if);"` |
|     - | 2338 | `"   foreach($ii['methods'] as $k => $m){"` |
|     - | 2339 | `"    if(strtolower($k) === $l){ return $ii['name']; }"` |
|     - | 2340 | `"   }"` |
|     - | 2341 | `"  }"` |
|     - | 2342 | `"  return null;"` |
|     - | 2343 | `" }"` |
|     - | 2344 | `" public function __toString(){ return __reflect_export_fnabs($this, ''); }"` |
|     - | 2345 | `"}"` |
|     - | 2346 | `"class ReflectionParameter implements Reflector {"` |
|     - | 2347 | `" public $name;"` |
|     - | 2348 | `" protected $__t;"` |
|     - | 2349 | `" protected $__m = null;"` |
|     - | 2350 | `" protected $__p = 0;"` |
|     - | 2351 | `" public function __construct($function, $param){"` |
|     - | 2352 | `"  $m = null;"` |
|     - | 2353 | `"  $t = $function;"` |
|     - | 2354 | `"  if(is_array($function)){"` |
|     - | 2355 | `"   $t = $function[0];"` |
|     - | 2356 | `"   $m = $function[1];"` |
|     - | 2357 | `"   if(is_object($t)){ $t = get_class($t); }"` |
|     - | 2358 | `"  }else if(is_string($function) && strpos($function,'::') !== false){"` |
|     - | 2359 | `"   $p = strpos($function,'::');"` |
|     - | 2360 | `"   $m = substr($function,$p+2);"` |
|     - | 2361 | `"   $t = substr($function,0,$p);"` |
|     - | 2362 | `"  }"` |
|     - | 2363 | `"  if($m !== null){"` |
|     - | 2364 | `"   $rm = new ReflectionMethod($t, $m);"` |
|     - | 2365 | `"   $t = $rm->class;"` |
|     - | 2366 | `"   $m = $rm->name;"` |
|     - | 2367 | `"   $i = __reflect_func_info($t, $m);"` |
|     - | 2368 | `"  }else if($function instanceof Closure){"` |
|     - | 2369 | `"   $t = $function;"` |
|     - | 2370 | `"   $i = __reflect_func_info($function);"` |
|     - | 2371 | `"  }else{"` |
|     - | 2372 | `"   $i = __reflect_sig_fixup(__reflect_func_info($t));"` |
|     - | 2373 | `"   if($i === null){"` |
|     - | 2374 | `"    throw new ReflectionException('Function '.$t.'() does not exist');"` |
|     - | 2375 | `"   }"` |
|     - | 2376 | `"  }"` |
|     - | 2377 | `"  $found = null;"` |
|     - | 2378 | `"  if(is_int($param)){"` |
|     - | 2379 | `"   if(isset($i['params'][$param])){ $found = $i['params'][$param]; }"` |
|     - | 2380 | `"   if($found === null){"` |
|     - | 2381 | `"    throw new ReflectionException('The parameter specified by its offset could not be found');"` |
|     - | 2382 | `"   }"` |
|     - | 2383 | `"  }else{"` |
|     - | 2384 | `"   foreach($i['params'] as $pp){"` |
|     - | 2385 | `"    if($pp['name'] === $param){ $found = $pp; break; }"` |
|     - | 2386 | `"   }"` |
|     - | 2387 | `"   if($found === null){"` |
|     - | 2388 | `"    throw new ReflectionException('The parameter specified by its name could not be found');"` |
|     - | 2389 | `"   }"` |
|     - | 2390 | `"  }"` |
|     - | 2391 | `"  $this->name = $found['name'];"` |
|     - | 2392 | `"  $this->__t = $t;"` |
|     - | 2393 | `"  $this->__m = $m;"` |
|     - | 2394 | `"  $this->__p = $found['pos'];"` |
|     - | 2395 | `" }"` |
|     - | 2396 | `" protected function __rffull(){"` |
|     - | 2397 | `"  if($this->__m !== null){ return __reflect_func_info($this->__t, $this->__m); }"` |
|     - | 2398 | `"  return __reflect_sig_fixup(__reflect_func_info($this->__t));"` |
|     - | 2399 | `" }"` |
|     - | 2400 | `" protected function __rpinfo(){"` |
|     - | 2401 | `"  $i = $this->__rffull();"` |
|     - | 2402 | `"  return $i['params'][$this->__p];"` |
|     - | 2403 | `" }"` |
|     - | 2404 | `" public function getName(){ return $this->name; }"` |
|     - | 2405 | `" public function getPosition(){ return $this->__p; }"` |
|     - | 2406 | `" public function isPassedByReference(){ $p = $this->__rpinfo(); return $p['byref']; }"` |
|     - | 2407 | `" public function canBePassedByValue(){ return !$this->isPassedByReference(); }"` |
|     - | 2408 | `" public function isVariadic(){ $p = $this->__rpinfo(); return $p['variadic']; }"` |
|     - | 2409 | `" public function isPromoted(){ $p = $this->__rpinfo(); return $p['promoted']; }"` |
|     - | 2410 | `" public function isDefaultValueAvailable(){ $p = $this->__rpinfo(); return $p['hasdef']; }"` |
|     - | 2411 | `" public function isOptional(){"` |
|     - | 2412 | `"  $i = $this->__rffull();"` |
|     - | 2413 | `"  $n = count($i['params']);"` |
|     - | 2414 | `"  for($k = $this->__p; $k < $n; $k++){"` |
|     - | 2415 | `"   $p = $i['params'][$k];"` |
|     - | 2416 | `"   if(!$p['variadic'] && !$p['hasdef']){ return false; }"` |
|     - | 2417 | `"  }"` |
|     - | 2418 | `"  return true;"` |
|     - | 2419 | `" }"` |
|     - | 2420 | `" public function getDefaultValue(){"` |
|     - | 2421 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2422 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2423 | `"  }"` |
|     - | 2424 | `"  $p = $this->__rpinfo();"` |
|     - | 2425 | `"  if(isset($p['deftext'])){"` |
|     - | 2426 | `"   $s = __reflect_sig_scalar($p['deftext']);"` |
|     - | 2427 | `"   if($s[0]){ return $s[1]; }"` |
|     - | 2428 | `"   if($p['deftext'] === 'array (' \|\| strpos($p['deftext'], '[') === 0){ return array(); }"` |
|     - | 2429 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2430 | `"  }"` |
|     - | 2431 | `"  return __reflect_param_default($this->__t, $this->__m, $this->__p);"` |
|     - | 2432 | `" }"` |
|     - | 2433 | `" public function isDefaultValueConstant(){"` |
|     - | 2434 | `"  if(!$this->isDefaultValueAvailable()){ return false; }"` |
|     - | 2435 | `"  $p = $this->__rpinfo();"` |
|     - | 2436 | `"  if(isset($p['deftext'])){ return false; }"` |
|     - | 2437 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p) !== null;"` |
|     - | 2438 | `" }"` |
|     - | 2439 | `" public function getDefaultValueConstantName(){"` |
|     - | 2440 | `"  if(!$this->isDefaultValueAvailable()){"` |
|     - | 2441 | `"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"` |
|     - | 2442 | `"  }"` |
|     - | 2443 | `"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p);"` |
|     - | 2444 | `" }"` |
|     - | 2445 | `" public function allowsNull(){"` |
|     - | 2446 | `"  $p = $this->__rpinfo();"` |
|     - | 2447 | `"  if($p['typetext'] === null){ return true; }"` |
|     - | 2448 | `"  if($p['nullable']){ return true; }"` |
|     - | 2449 | `"  return $p['typetext'] === 'mixed' \|\| $p['typetext'] === 'null';"` |
|     - | 2450 | `" }"` |
|     - | 2451 | `" public function hasType(){ $p = $this->__rpinfo(); return $p['typetext'] !== null; }"` |
|     - | 2452 | `" public function getType(){ $p = $this->__rpinfo(); return __reflect_make_type($p['typetext']); }"` |
|     - | 2453 | `" public function getDeclaringFunction(){"` |
|     - | 2454 | `"  if($this->__m !== null){ return new ReflectionMethod($this->__t, $this->__m); }"` |
|     - | 2455 | `"  return new ReflectionFunction($this->__t);"` |
|     - | 2456 | `" }"` |
|     - | 2457 | `" public function getDeclaringClass(){"` |
|     - | 2458 | `"  if($this->__m === null){ return null; }"` |
|     - | 2459 | `"  $i = $this->__rffull();"` |
|     - | 2460 | `"  return new ReflectionClass($i['decl']);"` |
|     - | 2461 | `" }"` |
|     - | 2462 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2463 | `"  $p = $this->__rpinfo();"` |
|     - | 2464 | `"  return __reflect_build_attrs($p['attrs'], array('param', $this->__t, $this->__m, $this->__p), 32, $name, $flags);"` |
|     - | 2465 | `" }"` |
|     - | 2466 | `" public function __toString(){ return __reflect_export_param($this); }"` |
|     - | 2467 | `"}"` |
|     - | 2468 | `;` |
|     - | 2469 | `/*` |
|     - | 2470 | ` * Chunk 3: PropertyHookType, ReflectionProperty, ReflectionClassConstant.` |
|     - | 2471 | ` */` |
|     - | 2472 | `static const char zReflectLib3[] =` |
|     - | 2473 | `"enum PropertyHookType: string {"` |
|     - | 2474 | `" case Get = 'get';"` |
|     - | 2475 | `" case Set = 'set';"` |
|     - | 2476 | `"}"` |
|     - | 2477 | `"class ReflectionProperty implements Reflector {"` |
|     - | 2478 | `" const IS_PUBLIC = 1;"` |
|     - | 2479 | `" const IS_PROTECTED = 2;"` |
|     - | 2480 | `" const IS_PRIVATE = 4;"` |
|     - | 2481 | `" const IS_STATIC = 16;"` |
|     - | 2482 | `" const IS_FINAL = 32;"` |
|     - | 2483 | `" const IS_ABSTRACT = 64;"` |
|     - | 2484 | `" const IS_READONLY = 128;"` |
|     - | 2485 | `" const IS_VIRTUAL = 512;"` |
|     - | 2486 | `" const IS_PROTECTED_SET = 2048;"` |
|     - | 2487 | `" const IS_PRIVATE_SET = 4096;"` |
|     - | 2488 | `" public $name;"` |
|     - | 2489 | `" public $class;"` |
|     - | 2490 | `" protected $__dynobj = null;"` |
|     - | 2491 | `" public function __construct($class, $property){"` |
|     - | 2492 | `"  $obj = null;"` |
|     - | 2493 | `"  if(is_object($class)){ $obj = $class; }"` |
|     - | 2494 | `"  else if(!is_string($class)){"` |
|     - | 2495 | `"   throw new TypeError('ReflectionProperty::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2496 | `"  }"` |
|     - | 2497 | `"  $ci = __reflect_class_info($class);"` |
|     - | 2498 | `"  if($ci === null){"` |
|     - | 2499 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2500 | `"  }"` |
|     - | 2501 | `"  $this->class = $ci['name'];"` |
|     - | 2502 | `"  if(isset($ci['props'][$property])){"` |
|     - | 2503 | `"   $this->name = $property;"` |
|     - | 2504 | `"   return;"` |
|     - | 2505 | `"  }"` |
|     - | 2506 | `"  if($obj !== null && (__reflect_prop_state($obj, $property) & 1)){"` |
|     - | 2507 | `"   $this->name = $property;"` |
|     - | 2508 | `"   $this->__dynobj = $obj;"` |
|     - | 2509 | `"   return;"` |
|     - | 2510 | `"  }"` |
|     - | 2511 | `"  throw new ReflectionException('Property '.$this->class.'::$'.$property.' does not exist');"` |
|     - | 2512 | `" }"` |
|     - | 2513 | `" protected function __rpmeta(){"` |
|     - | 2514 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2515 | `"  if(isset($ci['props'][$this->name])){ return $ci['props'][$this->name]; }"` |
|     - | 2516 | `"  return array('vis' => 1, 'static' => false, 'readonly' => false, 'hasdef' => false,"` |
|     - | 2517 | `"   'typed' => false, 'typetext' => null, 'decl' => $this->class, 'line' => 0, 'dyn' => true);"` |
|     - | 2518 | `" }"` |
|     - | 2519 | `" public function getName(){ return $this->name; }"` |
|     - | 2520 | `" public function getDeclaringClass(){"` |
|     - | 2521 | `"  $m = $this->__rpmeta();"` |
|     - | 2522 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2523 | `" }"` |
|     - | 2524 | `" public function getModifiers(){"` |
|     - | 2525 | `"  $m = $this->__rpmeta();"` |
|     - | 2526 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2527 | `"  if($m['static']){ $mod \|= 16; }"` |
|     - | 2528 | `"  if($m['readonly']){ $mod \|= 128; }"` |
|     - | 2529 | `"  return $mod;"` |
|     - | 2530 | `" }"` |
|     - | 2531 | `" public function isPublic(){ $m = $this->__rpmeta(); return $m['vis'] === 1; }"` |
|     - | 2532 | `" public function isProtected(){ $m = $this->__rpmeta(); return $m['vis'] === 2; }"` |
|     - | 2533 | `" public function isPrivate(){ $m = $this->__rpmeta(); return $m['vis'] === 3; }"` |
|     - | 2534 | `" public function isStatic(){ $m = $this->__rpmeta(); return $m['static']; }"` |
|     - | 2535 | `" public function isReadOnly(){ $m = $this->__rpmeta(); return $m['readonly']; }"` |
|     - | 2536 | `" public function isPrivateSet(){ $m = $this->__rpmeta(); return isset($m['privset']) ? $m['privset'] : false; }"` |
|     - | 2537 | `" public function isProtectedSet(){ $m = $this->__rpmeta(); return isset($m['protset']) ? $m['protset'] : false; }"` |
|     - | 2538 | `" public function isDefault(){ $m = $this->__rpmeta(); return !isset($m['dyn']); }"` |
|     - | 2539 | `" public function isDynamic(){ $m = $this->__rpmeta(); return isset($m['dyn']); }"` |
|     - | 2540 | `" public function isAbstract(){ return false; }"` |
|     - | 2541 | `" public function isFinal(){ return false; }"` |
|     - | 2542 | `" public function isVirtual(){ $m = $this->__rpmeta(); return isset($m['virtual']) ? $m['virtual'] : false; }"` |
|     - | 2543 | `" public function hasHooks(){ $m = $this->__rpmeta();"` |
|     - | 2544 | `"  return (isset($m['hookget']) && $m['hookget']) \|\| (isset($m['hookset']) && $m['hookset']); }"` |
|     - | 2545 | `" public function getHooks(){"` |
|     - | 2546 | `"  $m = $this->__rpmeta(); $h = array();"` |
|     - | 2547 | `"  if(isset($m['hookget']) && $m['hookget']){ $h['get'] = new ReflectionMethod($m['decl'], '__phl_hook_get_'.$this->name); }"` |
|     - | 2548 | `"  if(isset($m['hookset']) && $m['hookset']){ $h['set'] = new ReflectionMethod($m['decl'], '__phl_hook_set_'.$this->name); }"` |
|     - | 2549 | `"  return $h; }"` |
|     - | 2550 | `" public function hasHook($type){"` |
|     - | 2551 | `"  $t = $type instanceof PropertyHookType ? $type->value : $type;"` |
|     - | 2552 | `"  $m = $this->__rpmeta();"` |
|     - | 2553 | `"  if($t === 'get'){ return isset($m['hookget']) && $m['hookget']; }"` |
|     - | 2554 | `"  if($t === 'set'){ return isset($m['hookset']) && $m['hookset']; }"` |
|     - | 2555 | `"  return false; }"` |
|     - | 2556 | `" public function getHook($type){"` |
|     - | 2557 | `"  $t = $type instanceof PropertyHookType ? $type->value : $type;"` |
|     - | 2558 | `"  $h = $this->getHooks();"` |
|     - | 2559 | `"  return isset($h[$t]) ? $h[$t] : null; }"` |
|     - | 2560 | `" public function isLazy($object){ return false; }"` |
|     - | 2561 | `" public function setAccessible($accessible){ }"` |
|     - | 2562 | `" public function getValue($object = null){"` |
|     - | 2563 | `"  $m = $this->__rpmeta();"` |
|     - | 2564 | `"  if($m['static']){ return __reflect_static_value($this->class, $this->name); }"` |
|     - | 2565 | `"  if(!is_object($object)){"` |
|     - | 2566 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2567 | `"  }"` |
|     - | 2568 | `"  return __reflect_prop_read($object, $this->name);"` |
|     - | 2569 | `" }"` |
|     - | 2570 | `" public function setValue($objectOrValue = null, $value = null){"` |
|     - | 2571 | `"  $m = $this->__rpmeta();"` |
|     - | 2572 | `"  if($m['static']){"` |
|     - | 2573 | `"   if($value === null && $objectOrValue !== null && !is_object($objectOrValue)){"` |
|     - | 2574 | `"    __reflect_static_set($this->class, $this->name, $objectOrValue);"` |
|     - | 2575 | `"   }else{"` |
|     - | 2576 | `"    __reflect_static_set($this->class, $this->name, $value);"` |
|     - | 2577 | `"   }"` |
|     - | 2578 | `"   return;"` |
|     - | 2579 | `"  }"` |
|     - | 2580 | `"  __reflect_prop_write($objectOrValue, $this->name, $value);"` |
|     - | 2581 | `" }"` |
|     - | 2582 | `" public function getRawValue($object){ return $this->getValue($object); }"` |
|     - | 2583 | `" public function setRawValue($object, $value){ $this->setValue($object, $value); }"` |
|     - | 2584 | `" public function isInitialized($object = null){"` |
|     - | 2585 | `"  $m = $this->__rpmeta();"` |
|     - | 2586 | `"  if($m['static']){ return (__reflect_prop_state($this->class, $this->name) & 2) !== 0; }"` |
|     - | 2587 | `"  if(!is_object($object)){"` |
|     - | 2588 | `"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"` |
|     - | 2589 | `"  }"` |
|     - | 2590 | `"  return (__reflect_prop_state($object, $this->name) & 2) !== 0;"` |
|     - | 2591 | `" }"` |
|     - | 2592 | `" public function hasDefaultValue(){"` |
|     - | 2593 | `"  $m = $this->__rpmeta();"` |
|     - | 2594 | `"  if(isset($m['dyn'])){ return false; }"` |
|     - | 2595 | `"  if($m['hasdef']){ return true; }"` |
|     - | 2596 | `"  return !$m['typed'];"` |
|     - | 2597 | `" }"` |
|     - | 2598 | `" public function getDefaultValue(){"` |
|     - | 2599 | `"  $m = $this->__rpmeta();"` |
|     - | 2600 | `"  if(isset($m['dyn']) \|\| !$m['hasdef']){ return null; }"` |
|     - | 2601 | `"  return __reflect_prop_default($this->class, $this->name);"` |
|     - | 2602 | `" }"` |
|     - | 2603 | `" public function hasType(){ $m = $this->__rpmeta(); return $m['typed']; }"` |
|     - | 2604 | `" public function getType(){ $m = $this->__rpmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2605 | `" public function getSettableType(){ return $this->getType(); }"` |
|     - | 2606 | `" public function setRawValueWithoutLazyInitialization($object, $value){"` |
|     - | 2607 | `"  throw new Error('ReflectionProperty::setRawValueWithoutLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2608 | `" }"` |
|     - | 2609 | `" public function skipLazyInitialization($object){"` |
|     - | 2610 | `"  throw new Error('ReflectionProperty::skipLazyInitialization() is not supported by PHL (no lazy objects)');"` |
|     - | 2611 | `" }"` |
|     - | 2612 | `" public function getDocComment(){ $m = $this->__rpmeta(); return isset($m['doc']) ? $m['doc'] : false; }"` |
|     - | 2613 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2614 | `"  $m = $this->__rpmeta();"` |
|     - | 2615 | `"  if(!isset($m['attrs'])){ return array(); }"` |
|     - | 2616 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 8, $name, $flags);"` |
|     - | 2617 | `" }"` |
|     - | 2618 | `" public function __toString(){ return __reflect_export_prop($this); }"` |
|     - | 2619 | `"}"` |
|     - | 2620 | `"class ReflectionClassConstant implements Reflector {"` |
|     - | 2621 | `" const IS_PUBLIC = 1;"` |
|     - | 2622 | `" const IS_PROTECTED = 2;"` |
|     - | 2623 | `" const IS_PRIVATE = 4;"` |
|     - | 2624 | `" const IS_FINAL = 32;"` |
|     - | 2625 | `" public $name;"` |
|     - | 2626 | `" public $class;"` |
|     - | 2627 | `" public function __construct($class, $constant){"` |
|     - | 2628 | `"  if(!is_object($class) && !is_string($class)){"` |
|     - | 2629 | `"   throw new TypeError('ReflectionClassConstant::__construct(): Argument #1 ($class) must be of type object\|string, '.get_debug_type($class).' given');"` |
|     - | 2630 | `"  }"` |
|     - | 2631 | `"  $ci = __reflect_class_info($class);"` |
|     - | 2632 | `"  if($ci === null){"` |
|     - | 2633 | `"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"` |
|     - | 2634 | `"  }"` |
|     - | 2635 | `"  $this->class = $ci['name'];"` |
|     - | 2636 | `"  if(!isset($ci['consts'][$constant])){"` |
|     - | 2637 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' does not exist');"` |
|     - | 2638 | `"  }"` |
|     - | 2639 | `"  $this->name = $constant;"` |
|     - | 2640 | `" }"` |
|     - | 2641 | `" protected function __rcmeta(){"` |
|     - | 2642 | `"  $ci = __reflect_class_info($this->class);"` |
|     - | 2643 | `"  return $ci['consts'][$this->name];"` |
|     - | 2644 | `" }"` |
|     - | 2645 | `" public function getName(){ return $this->name; }"` |
|     - | 2646 | `" public function getValue(){ return __reflect_const_value($this->class, $this->name); }"` |
|     - | 2647 | `" public function getDeclaringClass(){"` |
|     - | 2648 | `"  $m = $this->__rcmeta();"` |
|     - | 2649 | `"  return new ReflectionClass($m['decl']);"` |
|     - | 2650 | `" }"` |
|     - | 2651 | `" public function getModifiers(){"` |
|     - | 2652 | `"  $m = $this->__rcmeta();"` |
|     - | 2653 | `"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"` |
|     - | 2654 | `"  if($m['final']){ $mod \|= 32; }"` |
|     - | 2655 | `"  return $mod;"` |
|     - | 2656 | `" }"` |
|     - | 2657 | `" public function isPublic(){ $m = $this->__rcmeta(); return $m['vis'] === 1; }"` |
|     - | 2658 | `" public function isProtected(){ $m = $this->__rcmeta(); return $m['vis'] === 2; }"` |
|     - | 2659 | `" public function isPrivate(){ $m = $this->__rcmeta(); return $m['vis'] === 3; }"` |
|     - | 2660 | `" public function isFinal(){ $m = $this->__rcmeta(); return $m['final']; }"` |
|     - | 2661 | `" public function isEnumCase(){ $m = $this->__rcmeta(); return $m['enumcase']; }"` |
|     - | 2662 | `" public function isDeprecated(){ $m = $this->__rcmeta(); return __reflect_has_deprecated($m['attrs']); }"` |
|     - | 2663 | `" public function hasType(){ $m = $this->__rcmeta(); return $m['typed']; }"` |
|     - | 2664 | `" public function getType(){ $m = $this->__rcmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"` |
|     - | 2665 | `" public function getDocComment(){ $m = $this->__rcmeta(); return $m['doc']; }"` |
|     - | 2666 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2667 | `"  $m = $this->__rcmeta();"` |
|     - | 2668 | `"  return __reflect_build_attrs($m['attrs'], array('attr', $this->class, $this->name, 0), 16, $name, $flags);"` |
|     - | 2669 | `" }"` |
|     - | 2670 | `" public function __toString(){ return __reflect_export_cconst($this); }"` |
|     - | 2671 | `"}"` |
|     - | 2672 | `;` |
|     - | 2673 | `/*` |
|     - | 2674 | ` * Chunk 4: the ReflectionType family, built from the engine's canonical` |
|     - | 2675 | ` * type text ("?int", "string\|float", "(A&B)\|C" — normalized at compile` |
|     - | 2676 | ` * time). __reflect_make_type is the internal factory; PHP itself never` |
|     - | 2677 | ` * lets user code construct these, so the public constructors here are a` |
|     - | 2678 | ` * recorded PHL-only surface.` |
|     - | 2679 | ` */` |
|     - | 2680 | `static const char zReflectLib4[] =` |
|     - | 2681 | `"abstract class ReflectionType implements Stringable {"` |
|     - | 2682 | `" protected $__text = '';"` |
|     - | 2683 | `" protected $__nullable = false;"` |
|     - | 2684 | `" public function allowsNull(){ return $this->__nullable; }"` |
|     - | 2685 | `" public function __toString(){ return $this->__text; }"` |
|     - | 2686 | `"}"` |
|     - | 2687 | `"class ReflectionNamedType extends ReflectionType {"` |
|     - | 2688 | `" protected $__tname = '';"` |
|     - | 2689 | `" public function __construct($name = '', $nullable = false, $text = null){"` |
|     - | 2690 | `"  $this->__tname = $name;"` |
|     - | 2691 | `"  $l = strtolower($name);"` |
|     - | 2692 | `"  $this->__nullable = $nullable \|\| $l === 'null' \|\| $l === 'mixed';"` |
|     - | 2693 | `"  $this->__text = $text === null ? $name : $text;"` |
|     - | 2694 | `" }"` |
|     - | 2695 | `" public function getName(){ return $this->__tname; }"` |
|     - | 2696 | `" public function isBuiltin(){"` |
|     - | 2697 | `"  $l = strtolower($this->__tname);"` |
|     - | 2698 | `"  return in_array($l, array('int','float','string','bool','array','object','mixed',"` |
|     - | 2699 | `"   'void','never','null','callable','iterable','true','false'), true);"` |
|     - | 2700 | `" }"` |
|     - | 2701 | `"}"` |
|     - | 2702 | `"class ReflectionUnionType extends ReflectionType {"` |
|     - | 2703 | `" protected $__types = array();"` |
|     - | 2704 | `" public function __construct($text = '', $nullable = false, $types = array()){"` |
|     - | 2705 | `"  $this->__text = $text;"` |
|     - | 2706 | `"  $this->__nullable = $nullable;"` |
|     - | 2707 | `"  $this->__types = $types;"` |
|     - | 2708 | `" }"` |
|     - | 2709 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2710 | `"}"` |
|     - | 2711 | `"class ReflectionIntersectionType extends ReflectionType {"` |
|     - | 2712 | `" protected $__types = array();"` |
|     - | 2713 | `" public function __construct($text = '', $types = array()){"` |
|     - | 2714 | `"  $this->__text = $text;"` |
|     - | 2715 | `"  $this->__nullable = false;"` |
|     - | 2716 | `"  $this->__types = $types;"` |
|     - | 2717 | `" }"` |
|     - | 2718 | `" public function getTypes(){ return $this->__types; }"` |
|     - | 2719 | `"}"` |
|     - | 2720 | `"function __reflect_make_atom($p){"` |
|     - | 2721 | `" $nullable = false;"` |
|     - | 2722 | `" if($p !== '' && $p[0] === '?'){ $nullable = true; $p = substr($p, 1); }"` |
|     - | 2723 | `" if($p !== '' && $p[0] === '('){ $p = substr($p, 1, strlen($p) - 2); }"` |
|     - | 2724 | `" if(strpos($p, '&') !== false){"` |
|     - | 2725 | `"  $subs = array();"` |
|     - | 2726 | `"  foreach(explode('&', $p) as $s){ $subs[] = new ReflectionNamedType($s, false, $s); }"` |
|     - | 2727 | `"  return new ReflectionIntersectionType($p, $subs);"` |
|     - | 2728 | `" }"` |
|     - | 2729 | `" return new ReflectionNamedType($p, $nullable, $nullable ? '?'.$p : $p);"` |
|     - | 2730 | `"}"` |
|     - | 2731 | `"function __reflect_make_type($text){"` |
|     - | 2732 | `" if($text === null \|\| $text === ''){ return null; }"` |
|     - | 2733 | `" $nullable = false;"` |
|     - | 2734 | `" $body = $text;"` |
|     - | 2735 | `" if($body[0] === '?'){ $nullable = true; $body = substr($body, 1); }"` |
|     - | 2736 | `" $parts = array();"` |
|     - | 2737 | `" $depth = 0;"` |
|     - | 2738 | `" $cur = '';"` |
|     - | 2739 | `" $n = strlen($body);"` |
|     - | 2740 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 2741 | `"  $ch = $body[$k];"` |
|     - | 2742 | `"  if($ch === '('){ $depth++; $cur .= $ch; }"` |
|     - | 2743 | `"  else if($ch === ')'){ $depth--; $cur .= $ch; }"` |
|     - | 2744 | `"  else if($ch === '\|' && $depth === 0){ $parts[] = $cur; $cur = ''; }"` |
|     - | 2745 | `"  else{ $cur .= $ch; }"` |
|     - | 2746 | `" }"` |
|     - | 2747 | `" $parts[] = $cur;"` |
|     - | 2748 | `" if(count($parts) > 1){"` |
|     - | 2749 | `"  $nonNull = array();"` |
|     - | 2750 | `"  $hasNull = false;"` |
|     - | 2751 | `"  foreach($parts as $p){"` |
|     - | 2752 | `"   if(strtolower($p) === 'null'){ $hasNull = true; }"` |
|     - | 2753 | `"   else{ $nonNull[] = $p; }"` |
|     - | 2754 | `"  }"` |
|     - | 2755 | `"  if($hasNull && count($nonNull) === 1 && strpos($nonNull[0], '&') === false){"` |
|     - | 2756 | `"   return new ReflectionNamedType($nonNull[0], true, '?'.$nonNull[0]);"` |
|     - | 2757 | `"  }"` |
|     - | 2758 | `"  $types = array();"` |
|     - | 2759 | `"  foreach($parts as $p){ $types[] = __reflect_make_atom($p); }"` |
|     - | 2760 | `"  return new ReflectionUnionType($body, $nullable \|\| $hasNull, $types);"` |
|     - | 2761 | `" }"` |
|     - | 2762 | `" if(strpos($body, '&') !== false){ return __reflect_make_atom($body); }"` |
|     - | 2763 | `" return __reflect_make_atom($nullable ? '?'.$body : $body);"` |
|     - | 2764 | `"}"` |
|     - | 2765 | `;` |
|     - | 2766 | `/*` |
|     - | 2767 | ` * Chunk 5: ReflectionGenerator, ReflectionFiber. Executing line/file and` |
|     - | 2768 | ` * traces need runtime line tracking the VM does not have (same gap as` |
|     - | 2769 | ` * debug_backtrace's line numbers) — those throw a loud Error, recorded in` |
|     - | 2770 | ` * the plan ledger.` |
|     - | 2771 | ` */` |
|     - | 2772 | `static const char zReflectLib5[] =` |
|     - | 2773 | `"class ReflectionGenerator {"` |
|     - | 2774 | `" protected $__gen;"` |
|     - | 2775 | `" public function __construct($generator){"` |
|     - | 2776 | `"  if(!($generator instanceof Generator)){"` |
|     - | 2777 | `"   throw new TypeError('ReflectionGenerator::__construct(): Argument #1 ($generator) must be of type Generator, '.get_debug_type($generator).' given');"` |
|     - | 2778 | `"  }"` |
|     - | 2779 | `"  $this->__gen = $generator;"` |
|     - | 2780 | `" }"` |
|     - | 2781 | `" protected function __rginfo(){ return __reflect_gen_info($this->__gen); }"` |
|     - | 2782 | `" public function getFunction(){"` |
|     - | 2783 | `"  $i = $this->__rginfo();"` |
|     - | 2784 | `"  if($i['kind'] === 'method'){ return new ReflectionMethod($i['class'], $i['name']); }"` |
|     - | 2785 | `"  return new ReflectionFunction($i['name']);"` |
|     - | 2786 | `" }"` |
|     - | 2787 | `" public function getThis(){ $i = $this->__rginfo(); return isset($i['this']) ? $i['this'] : null; }"` |
|     - | 2788 | `" public function getExecutingGenerator(){ return __reflect_gen_exec($this->__gen); }"` |
|     - | 2789 | `" public function isClosed(){ $i = $this->__rginfo(); return $i['closed']; }"` |
|     - | 2790 | `" public function getExecutingLine(){"` |
|     - | 2791 | `"  throw new Error('ReflectionGenerator::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2792 | `" }"` |
|     - | 2793 | `" public function getExecutingFile(){"` |
|     - | 2794 | `"  throw new Error('ReflectionGenerator::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2795 | `" }"` |
|     - | 2796 | `" public function getTrace($options = 1){"` |
|     - | 2797 | `"  throw new Error('ReflectionGenerator::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2798 | `" }"` |
|     - | 2799 | `"}"` |
|     - | 2800 | `"class ReflectionFiber {"` |
|     - | 2801 | `" protected $__fiber;"` |
|     - | 2802 | `" public function __construct($fiber){"` |
|     - | 2803 | `"  if(!($fiber instanceof Fiber)){"` |
|     - | 2804 | `"   throw new TypeError('ReflectionFiber::__construct(): Argument #1 ($fiber) must be of type Fiber, '.get_debug_type($fiber).' given');"` |
|     - | 2805 | `"  }"` |
|     - | 2806 | `"  $this->__fiber = $fiber;"` |
|     - | 2807 | `" }"` |
|     - | 2808 | `" public function getFiber(){ return $this->__fiber; }"` |
|     - | 2809 | `" public function getCallable(){ return __reflect_prop_read($this->__fiber, '__callable'); }"` |
|     - | 2810 | `" public function getExecutingLine(){"` |
|     - | 2811 | `"  throw new Error('ReflectionFiber::getExecutingLine() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2812 | `" }"` |
|     - | 2813 | `" public function getExecutingFile(){"` |
|     - | 2814 | `"  throw new Error('ReflectionFiber::getExecutingFile() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2815 | `" }"` |
|     - | 2816 | `" public function getTrace($options = 1){"` |
|     - | 2817 | `"  throw new Error('ReflectionFiber::getTrace() is not supported by PHL (no runtime line tracking)');"` |
|     - | 2818 | `" }"` |
|     - | 2819 | `"}"` |
|     - | 2820 | `;` |
|     - | 2821 | `/*` |
|     - | 2822 | ` * Chunk 6: the long tail — ReflectionConstant (PHP 8.5), the synthetic` |
|     - | 2823 | ` * "Core" ReflectionExtension, ReflectionZendExtension (throws: no Zend` |
|     - | 2824 | ` * extensions exist), the ReflectionEnum family (throws: enums are not a` |
|     - | 2825 | ` * PHL language feature yet), and ReflectionReference.` |
|     - | 2826 | ` */` |
|     - | 2827 | `static const char zReflectLib6[] =` |
|     - | 2828 | `"class ReflectionConstant implements Reflector {"` |
|     - | 2829 | `" public $name;"` |
|     - | 2830 | `" public function __construct($name){"` |
|     - | 2831 | `"  if(!is_string($name)){"` |
|     - | 2832 | `"   throw new TypeError('ReflectionConstant::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2833 | `"  }"` |
|     - | 2834 | `"  $i = __reflect_const_info($name);"` |
|     - | 2835 | `"  if($i === null){"` |
|     - | 2836 | `"   throw new ReflectionException('Constant \"'.$name.'\" does not exist');"` |
|     - | 2837 | `"  }"` |
|     - | 2838 | `"  $this->name = $name;"` |
|     - | 2839 | `" }"` |
|     - | 2840 | `" public function getName(){ return $this->name; }"` |
|     - | 2841 | `" public function getNamespaceName(){"` |
|     - | 2842 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2843 | `"  if($p === false){ return ''; }"` |
|     - | 2844 | `"  return substr($this->name,0,$p);"` |
|     - | 2845 | `" }"` |
|     - | 2846 | `" public function getShortName(){"` |
|     - | 2847 | `"  $p = strrpos($this->name,'\\\\');"` |
|     - | 2848 | `"  if($p === false){ return $this->name; }"` |
|     - | 2849 | `"  return substr($this->name,$p+1);"` |
|     - | 2850 | `" }"` |
|     - | 2851 | `" public function getValue(){"` |
|     - | 2852 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2853 | `"  return $i['value'];"` |
|     - | 2854 | `" }"` |
|     - | 2855 | `" public function isDeprecated(){ return false; }"` |
|     - | 2856 | `" public function getFileName(){"` |
|     - | 2857 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2858 | `"  return $i['file'];"` |
|     - | 2859 | `" }"` |
|     - | 2860 | `" public function getExtension(){"` |
|     - | 2861 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2862 | `"  return $i['internal'] ? new ReflectionExtension('Core') : null;"` |
|     - | 2863 | `" }"` |
|     - | 2864 | `" public function getExtensionName(){"` |
|     - | 2865 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2866 | `"  return $i['internal'] ? 'Core' : false;"` |
|     - | 2867 | `" }"` |
|     - | 2868 | `" public function getAttributes($name = null, $flags = 0){"` |
|     - | 2869 | `"  $i = __reflect_const_info($this->name);"` |
|     - | 2870 | `"  if($i === null){ return array(); }"` |
|     - | 2871 | `"  return __reflect_build_attrs($i['attrs'], array('const', $this->name, null, 0), 64, $name, $flags);"` |
|     - | 2872 | `" }"` |
|     - | 2873 | `" public function __toString(){"` |
|     - | 2874 | `"  return 'Constant [ '.$this->name.' ]'.\"\\n\";"` |
|     - | 2875 | `" }"` |
|     - | 2876 | `"}"` |
|     - | 2877 | `"class ReflectionExtension implements Reflector {"` |
|     - | 2878 | `" public $name;"` |
|     - | 2879 | `" public function __construct($name){"` |
|     - | 2880 | `"  if(!is_string($name)){"` |
|     - | 2881 | `"   throw new TypeError('ReflectionExtension::__construct(): Argument #1 ($name) must be of type string, '.get_debug_type($name).' given');"` |
|     - | 2882 | `"  }"` |
|     - | 2883 | `"  if(strtolower($name) !== 'core'){"` |
|     - | 2884 | `"   throw new ReflectionException('Extension \"'.$name.'\" does not exist');"` |
|     - | 2885 | `"  }"` |
|     - | 2886 | `"  $this->name = 'Core';"` |
|     - | 2887 | `" }"` |
|     - | 2888 | `" public function getName(){ return $this->name; }"` |
|     - | 2889 | `" public function getVersion(){ return phpversion(); }"` |
|     - | 2890 | `" public function getFunctions(){ return array(); }"` |
|     - | 2891 | `" public function getClasses(){ return array(); }"` |
|     - | 2892 | `" public function getClassNames(){ return array(); }"` |
|     - | 2893 | `" public function getConstants(){ return array(); }"` |
|     - | 2894 | `" public function getINIEntries(){ return array(); }"` |
|     - | 2895 | `" public function getDependencies(){ return array(); }"` |
|     - | 2896 | `" public function isPersistent(){ return true; }"` |
|     - | 2897 | `" public function isTemporary(){ return false; }"` |
|     - | 2898 | `" public function info(){ }"` |
|     - | 2899 | `" public function __toString(){"` |
|     - | 2900 | `"  return 'Extension [ extension #1 '.$this->name.' ]'.\"\\n\";"` |
|     - | 2901 | `" }"` |
|     - | 2902 | `"}"` |
|     - | 2903 | `"class ReflectionZendExtension implements Reflector {"` |
|     - | 2904 | `" public $name;"` |
|     - | 2905 | `" public function __construct($name){"` |
|     - | 2906 | `"  throw new ReflectionException('Zend Extension \"'.$name.'\" does not exist');"` |
|     - | 2907 | `" }"` |
|     - | 2908 | `" public function getName(){ return $this->name; }"` |
|     - | 2909 | `" public function __toString(){ return ''; }"` |
|     - | 2910 | `"}"` |
|     - | 2911 | `"class ReflectionEnum extends ReflectionClass {"` |
|     - | 2912 | `" public function __construct($objectOrClass){"` |
|     - | 2913 | `"  $info = __reflect_class_info($objectOrClass);"` |
|     - | 2914 | `"  if($info === null){"` |
|     - | 2915 | `"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"` |
|     - | 2916 | `"  }"` |
|     - | 2917 | `"  if(!$info['enum']){"` |
|     - | 2918 | `"   throw new ReflectionException('Class \"'.$info['name'].'\" is not an enum');"` |
|     - | 2919 | `"  }"` |
|     - | 2920 | `"  parent::__construct($objectOrClass);"` |
|     - | 2921 | `" }"` |
|     - | 2922 | `" public function hasCase($name){"` |
|     - | 2923 | `"  $i = $this->__rinfo();"` |
|     - | 2924 | `"  return in_array($name, $i['cases'], true);"` |
|     - | 2925 | `" }"` |
|     - | 2926 | `" public function getCase($name){"` |
|     - | 2927 | `"  if(!$this->hasCase($name)){"` |
|     - | 2928 | `"   throw new ReflectionException('Case '.$this->name.'::'.$name.' does not exist');"` |
|     - | 2929 | `"  }"` |
|     - | 2930 | `"  if($this->isBacked()){ return new ReflectionEnumBackedCase($this->name, $name); }"` |
|     - | 2931 | `"  return new ReflectionEnumUnitCase($this->name, $name);"` |
|     - | 2932 | `" }"` |
|     - | 2933 | `" public function getCases(){"` |
|     - | 2934 | `"  $i = $this->__rinfo();"` |
|     - | 2935 | `"  $out = array();"` |
|     - | 2936 | `"  foreach($i['cases'] as $c){"` |
|     - | 2937 | `"   $out[] = $this->isBacked()"` |
|     - | 2938 | `"    ? new ReflectionEnumBackedCase($this->name, $c)"` |
|     - | 2939 | `"    : new ReflectionEnumUnitCase($this->name, $c);"` |
|     - | 2940 | `"  }"` |
|     - | 2941 | `"  return $out;"` |
|     - | 2942 | `" }"` |
|     - | 2943 | `" public function isBacked(){ $i = $this->__rinfo(); return $i['enumbacking'] !== ''; }"` |
|     - | 2944 | `" public function getBackingType(){"` |
|     - | 2945 | `"  $i = $this->__rinfo();"` |
|     - | 2946 | `"  if($i['enumbacking'] === ''){ return null; }"` |
|     - | 2947 | `"  return __reflect_make_type($i['enumbacking']);"` |
|     - | 2948 | `" }"` |
|     - | 2949 | `"}"` |
|     - | 2950 | `"class ReflectionEnumUnitCase extends ReflectionClassConstant {"` |
|     - | 2951 | `" public function __construct($class, $constant){"` |
|     - | 2952 | `"  parent::__construct($class, $constant);"` |
|     - | 2953 | `"  $ci = __reflect_class_info($class);"` |
|     - | 2954 | `"  if(!$ci['enum']){"` |
|     - | 2955 | `"   throw new ReflectionException('Class \"'.$this->class.'\" is not an enum');"` |
|     - | 2956 | `"  }"` |
|     - | 2957 | `"  $m = $this->__rcmeta();"` |
|     - | 2958 | `"  if(!$m['enumcase']){"` |
|     - | 2959 | `"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' is not a case');"` |
|     - | 2960 | `"  }"` |
|     - | 2961 | `" }"` |
|     - | 2962 | `" public function getEnum(){ return new ReflectionEnum($this->class); }"` |
|     - | 2963 | `"}"` |
|     - | 2964 | `"class ReflectionEnumBackedCase extends ReflectionEnumUnitCase {"` |
|     - | 2965 | `" public function getBackingValue(){ return $this->getValue()->value; }"` |
|     - | 2966 | `"}"` |
|     - | 2967 | `"final class ReflectionReference {"` |
|     - | 2968 | `" protected $__id = '';"` |
|     - | 2969 | `" public function __construct(){"` |
|     - | 2970 | `"  throw new Error('Call to private ReflectionReference::__construct() from global scope');"` |
|     - | 2971 | `" }"` |
|     - | 2972 | `" public static function fromArrayElement($array, $key){"` |
|     - | 2973 | `"  if(!is_array($array)){"` |
|     - | 2974 | `"   throw new TypeError('ReflectionReference::fromArrayElement(): Argument #1 ($array) must be of type array, '.get_debug_type($array).' given');"` |
|     - | 2975 | `"  }"` |
|     - | 2976 | `"  $id = __reflect_ref_id($array, $key);"` |
|     - | 2977 | `"  if($id === null){ return null; }"` |
|     - | 2978 | `"  $r = __reflect_new_no_ctor('ReflectionReference');"` |
|     - | 2979 | `"  $r->__setId('phlref'.$id);"` |
|     - | 2980 | `"  return $r;"` |
|     - | 2981 | `" }"` |
|     - | 2982 | `" public function __setId($id){ $this->__id = $id; }"` |
|     - | 2983 | `" public function getId(){ return $this->__id; }"` |
|     - | 2984 | `"}"` |
|     - | 2985 | `;` |
|     - | 2986 | `/*` |
|     - | 2987 | ` * Chunk 7: ReflectionAttribute and the shared getAttributes() builder.` |
|     - | 2988 | ` * The spec array rides as [kind, target, member, paramIdx]; argument` |
|     - | 2989 | ` * values evaluate lazily through __reflect_attr_args (PHP semantics).` |
|     - | 2990 | ` */` |
|     - | 2991 | `static const char zReflectLib7[] =` |
|     - | 2992 | `"function __reflect_has_deprecated($meta){"` |
|     - | 2993 | `" foreach($meta as $a){"` |
|     - | 2994 | `"  if(strtolower($a['name']) === 'deprecated'){ return true; }"` |
|     - | 2995 | `" }"` |
|     - | 2996 | `" return false;"` |
|     - | 2997 | `"}"` |
|     - | 2998 | `"function __reflect_target_names($mask){"` |
|     - | 2999 | `" $parts = array();"` |
|     - | 3000 | `" foreach(array('class' => 1, 'function' => 2, 'method' => 4, 'property' => 8,"` |
|     - | 3001 | `"  'class constant' => 16, 'parameter' => 32, 'constant' => 64) as $nm => $bit){"` |
|     - | 3002 | `"  if($mask & $bit){ $parts[] = $nm; }"` |
|     - | 3003 | `" }"` |
|     - | 3004 | `" return implode(', ', $parts);"` |
|     - | 3005 | `"}"` |
|     - | 3006 | `"function __reflect_build_attrs($meta, $spec, $target, $name, $flags){"` |
|     - | 3007 | `" $out = array();"` |
|     - | 3008 | `" $counts = array();"` |
|     - | 3009 | `" foreach($meta as $a){"` |
|     - | 3010 | `"  $k = strtolower($a['name']);"` |
|     - | 3011 | `"  $counts[$k] = isset($counts[$k]) ? $counts[$k] + 1 : 1;"` |
|     - | 3012 | `" }"` |
|     - | 3013 | `" $idx = 0;"` |
|     - | 3014 | `" foreach($meta as $a){"` |
|     - | 3015 | `"  $keep = true;"` |
|     - | 3016 | `"  if($name !== null){"` |
|     - | 3017 | `"   $keep = strtolower($a['name']) === strtolower($name);"` |
|     - | 3018 | `"   if(!$keep && ($flags & 2)){"` |
|     - | 3019 | `"    $keep = is_subclass_of($a['name'], $name);"` |
|     - | 3020 | `"   }"` |
|     - | 3021 | `"  }"` |
|     - | 3022 | `"  if($keep){"` |
|     - | 3023 | `"   $r = __reflect_new_no_ctor('ReflectionAttribute');"` |
|     - | 3024 | `"   $r->__init($a['name'], $spec, $idx, $target, $counts[strtolower($a['name'])] > 1);"` |
|     - | 3025 | `"   $out[] = $r;"` |
|     - | 3026 | `"  }"` |
|     - | 3027 | `"  $idx++;"` |
|     - | 3028 | `" }"` |
|     - | 3029 | `" return $out;"` |
|     - | 3030 | `"}"` |
|     - | 3031 | `"final class ReflectionAttribute {"` |
|     - | 3032 | `" const IS_INSTANCEOF = 2;"` |
|     - | 3033 | `" protected $__name = '';"` |
|     - | 3034 | `" protected $__spec = null;"` |
|     - | 3035 | `" protected $__idx = 0;"` |
|     - | 3036 | `" protected $__target = 0;"` |
|     - | 3037 | `" protected $__rep = false;"` |
|     - | 3038 | `" public function __construct(){"` |
|     - | 3039 | `"  throw new Error('Call to private ReflectionAttribute::__construct() from global scope');"` |
|     - | 3040 | `" }"` |
|     - | 3041 | `" public function __init($name, $spec, $idx, $target, $rep){"` |
|     - | 3042 | `"  $this->__name = $name;"` |
|     - | 3043 | `"  $this->__spec = $spec;"` |
|     - | 3044 | `"  $this->__idx = $idx;"` |
|     - | 3045 | `"  $this->__target = $target;"` |
|     - | 3046 | `"  $this->__rep = $rep;"` |
|     - | 3047 | `" }"` |
|     - | 3048 | `" public function getName(){ return $this->__name; }"` |
|     - | 3049 | `" public function getTarget(){ return $this->__target; }"` |
|     - | 3050 | `" public function isRepeated(){ return $this->__rep; }"` |
|     - | 3051 | `" public function getArguments(){"` |
|     - | 3052 | `"  $a = __reflect_attr_args($this->__spec[0], $this->__spec[1], $this->__spec[2], $this->__spec[3], $this->__idx);"` |
|     - | 3053 | `"  return $a === null ? array() : $a;"` |
|     - | 3054 | `" }"` |
|     - | 3055 | `" public function newInstance(){"` |
|     - | 3056 | `"  $name = $this->__name;"` |
|     - | 3057 | `"  $ci = __reflect_class_info($name);"` |
|     - | 3058 | `"  if($ci === null){"` |
|     - | 3059 | `"   throw new Error('Attribute class \"'.$name.'\" not found');"` |
|     - | 3060 | `"  }"` |
|     - | 3061 | `"  $name = $ci['name'];"` |
|     - | 3062 | `"  $decl = null;"` |
|     - | 3063 | `"  $didx = 0;"` |
|     - | 3064 | `"  foreach($ci['attrs'] as $a){"` |
|     - | 3065 | `"   if(strtolower($a['name']) === 'attribute'){ $decl = $didx; break; }"` |
|     - | 3066 | `"   $didx++;"` |
|     - | 3067 | `"  }"` |
|     - | 3068 | `"  if($decl === null){"` |
|     - | 3069 | `"   throw new Error('Attempting to use non-attribute class \"'.$name.'\" as attribute');"` |
|     - | 3070 | `"  }"` |
|     - | 3071 | `"  $dargs = __reflect_attr_args('class', $name, null, 0, $decl);"` |
|     - | 3072 | `"  $flags = 127;"` |
|     - | 3073 | `"  if(is_array($dargs)){"` |
|     - | 3074 | `"   if(isset($dargs[0])){ $flags = $dargs[0]; }"` |
|     - | 3075 | `"   else if(isset($dargs['flags'])){ $flags = $dargs['flags']; }"` |
|     - | 3076 | `"  }"` |
|     - | 3077 | `"  if(($flags & $this->__target) === 0){"` |
|     - | 3078 | `"   $tnames = array(1 => 'class', 2 => 'function', 4 => 'method', 8 => 'property',"` |
|     - | 3079 | `"    16 => 'class constant', 32 => 'parameter', 64 => 'constant');"` |
|     - | 3080 | `"   throw new Error('Attribute \"'.$name.'\" cannot target '.$tnames[$this->__target]"` |
|     - | 3081 | `"    .' (allowed targets: '.__reflect_target_names($flags).')');"` |
|     - | 3082 | `"  }"` |
|     - | 3083 | `"  if($this->__rep && ($flags & 128) === 0){"` |
|     - | 3084 | `"   throw new Error('Attribute \"'.$name.'\" must not be repeated');"` |
|     - | 3085 | `"  }"` |
|     - | 3086 | `"  return __reflect_new_instance($name, $this->getArguments());"` |
|     - | 3087 | `" }"` |
|     - | 3088 | `" public function __toString(){"` |
|     - | 3089 | `"  return 'Attribute [ '.$this->__name.' ]';"` |
|     - | 3090 | `" }"` |
|     - | 3091 | `"}"` |
|     - | 3092 | `;` |
|     - | 3093 | `/*` |
|     - | 3094 | ` * Chunk 8: signature-table support. Internal (C builtin) functions carry a` |
|     - | 3095 | ` * PHP-style parameter-list string; these helpers parse it into the same` |
|     - | 3096 | ` * param-meta shape user functions get, so ReflectionFunction and` |
|     - | 3097 | ` * ReflectionParameter work uniformly over builtins.` |
|     - | 3098 | ` */` |
|     - | 3099 | `static const char zReflectLib8[] =` |
|     - | 3100 | `"function __reflect_sig_split($sig){"` |
|     - | 3101 | `" $parts = array();"` |
|     - | 3102 | `" $cur = '';"` |
|     - | 3103 | `" $q = false;"` |
|     - | 3104 | `" $n = strlen($sig);"` |
|     - | 3105 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3106 | `"  $ch = $sig[$k];"` |
|     - | 3107 | `"  if($q){"` |
|     - | 3108 | `"   $cur .= $ch;"` |
|     - | 3109 | `"   if($ch === chr(92) && $k + 1 < $n){ $cur .= $sig[$k+1]; $k++; }"` |
|     - | 3110 | `"   else if($ch === chr(39)){ $q = false; }"` |
|     - | 3111 | `"  }else if($ch === chr(39)){ $q = true; $cur .= $ch; }"` |
|     - | 3112 | `"  else if($ch === ',' ){ $parts[] = trim($cur); $cur = ''; }"` |
|     - | 3113 | `"  else{ $cur .= $ch; }"` |
|     - | 3114 | `" }"` |
|     - | 3115 | `" if(trim($cur) !== ''){ $parts[] = trim($cur); }"` |
|     - | 3116 | `" return $parts;"` |
|     - | 3117 | `"}"` |
|     - | 3118 | `"function __reflect_sig_scalar($t){"` |
|     - | 3119 | `" if($t === '?'){ return array(false, null); }"` |
|     - | 3120 | `" if($t === 'NULL' \|\| $t === 'null'){ return array(true, null); }"` |
|     - | 3121 | `" if($t === 'true'){ return array(true, true); }"` |
|     - | 3122 | `" if($t === 'false'){ return array(true, false); }"` |
|     - | 3123 | `" if(is_numeric($t)){"` |
|     - | 3124 | `"  if(strpos($t, '.') === false && stripos($t, 'e') === false && strpos($t, 'x') === false){"` |
|     - | 3125 | `"   return array(true, (int)$t);"` |
|     - | 3126 | `"  }"` |
|     - | 3127 | `"  return array(true, (float)$t);"` |
|     - | 3128 | `" }"` |
|     - | 3129 | `" if(strlen($t) >= 2 && $t[0] === chr(39) && $t[strlen($t)-1] === chr(39)){"` |
|     - | 3130 | `"  $body = substr($t, 1, strlen($t) - 2);"` |
|     - | 3131 | `"  return array(true, strtr($body, array(chr(92).chr(39) => chr(39), chr(92).chr(92) => chr(92))));"` |
|     - | 3132 | `" }"` |
|     - | 3133 | `" return array(false, null);"` |
|     - | 3134 | `"}"` |
|     - | 3135 | `"function __reflect_parse_sig($sig){"` |
|     - | 3136 | `" $params = array();"` |
|     - | 3137 | `" $pos = 0;"` |
|     - | 3138 | `" foreach(__reflect_sig_split($sig) as $part){"` |
|     - | 3139 | `"  $deftext = null;"` |
|     - | 3140 | `"  $q = false;"` |
|     - | 3141 | `"  $n = strlen($part);"` |
|     - | 3142 | `"  for($k = 0; $k < $n; $k++){"` |
|     - | 3143 | `"   $ch = $part[$k];"` |
|     - | 3144 | `"   if($q){"` |
|     - | 3145 | `"    if($ch === chr(92)){ $k++; }"` |
|     - | 3146 | `"    else if($ch === chr(39)){ $q = false; }"` |
|     - | 3147 | `"   }else if($ch === chr(39)){ $q = true; }"` |
|     - | 3148 | `"   else if($ch === '=' ){"` |
|     - | 3149 | `"    $deftext = trim(substr($part, $k + 1));"` |
|     - | 3150 | `"    $part = trim(substr($part, 0, $k));"` |
|     - | 3151 | `"    break;"` |
|     - | 3152 | `"   }"` |
|     - | 3153 | `"  }"` |
|     - | 3154 | `"  $variadic = strpos($part, '...') !== false;"` |
|     - | 3155 | `"  $byref = strpos($part, '&') !== false;"` |
|     - | 3156 | `"  $d = strpos($part, '$');"` |
|     - | 3157 | `"  $name = $d === false ? $part : substr($part, $d + 1);"` |
|     - | 3158 | `"  $typetext = null;"` |
|     - | 3159 | `"  $sp = strpos($part, ' ');"` |
|     - | 3160 | `"  if($sp !== false && $d !== false && $sp < $d){ $typetext = substr($part, 0, $sp); }"` |
|     - | 3161 | `"  $nullable = $typetext !== null && ($typetext[0] === '?' \|\| stripos($typetext, 'null') !== false);"` |
|     - | 3162 | `"  $params[] = array('name' => $name, 'pos' => $pos, 'byref' => $byref,"` |
|     - | 3163 | `"   'variadic' => $variadic, 'hasdef' => $deftext !== null, 'nullable' => $nullable,"` |
|     - | 3164 | `"   'promoted' => false, 'typetext' => $typetext, 'attrs' => array(), 'deftext' => $deftext);"` |
|     - | 3165 | `"  $pos++;"` |
|     - | 3166 | `" }"` |
|     - | 3167 | `" return $params;"` |
|     - | 3168 | `"}"` |
|     - | 3169 | `"function __reflect_sig_fixup($i){"` |
|     - | 3170 | `" if($i === null){ return $i; }"` |
|     - | 3171 | `" if(isset($i['ret2'])){ $i['rettext'] = $i['ret2']; }"` |
|     - | 3172 | `" if(!isset($i['sig']) \|\| $i['sig'] === ''){ return $i; }"` |
|     - | 3173 | `" $i['params'] = __reflect_parse_sig($i['sig']);"` |
|     - | 3174 | `" $i['minarg'] = -1;"` |
|     - | 3175 | `" $v = false;"` |
|     - | 3176 | `" foreach($i['params'] as $p){ if($p['variadic']){ $v = true; } }"` |
|     - | 3177 | `" $i['variadic'] = $v;"` |
|     - | 3178 | `" return $i;"` |
|     - | 3179 | `"}"` |
|     - | 3180 | `;` |
|     - | 3181 | `/*` |
|     - | 3182 | ` * Chunk 9: PHP's Reflection export format (__toString on every Reflector).` |
|     - | 3183 | ` * Built entirely from the public reflection API of the target objects.` |
|     - | 3184 | ` */` |
|     - | 3185 | `static const char zReflectLib9[] =` |
|     - | 3186 | `"function __reflect_export_value($v){"` |
|     - | 3187 | `" if($v === null){ return 'NULL'; }"` |
|     - | 3188 | `" if($v === true){ return 'true'; }"` |
|     - | 3189 | `" if($v === false){ return 'false'; }"` |
|     - | 3190 | `" if(is_string($v)){ return chr(39).$v.chr(39); }"` |
|     - | 3191 | `" if(is_array($v)){"` |
|     - | 3192 | `"  $parts = array();"` |
|     - | 3193 | `"  $isList = true;"` |
|     - | 3194 | `"  $next = 0;"` |
|     - | 3195 | `"  foreach($v as $k => $x){"` |
|     - | 3196 | `"   if($k !== $next){ $isList = false; break; }"` |
|     - | 3197 | `"   $next++;"` |
|     - | 3198 | `"  }"` |
|     - | 3199 | `"  foreach($v as $k => $x){"` |
|     - | 3200 | `"   $parts[] = $isList ? __reflect_export_value($x)"` |
|     - | 3201 | `"    : (__reflect_export_value($k).' => '.__reflect_export_value($x));"` |
|     - | 3202 | `"  }"` |
|     - | 3203 | `"  return '['.implode(', ', $parts).']';"` |
|     - | 3204 | `" }"` |
|     - | 3205 | `" return (string)$v;"` |
|     - | 3206 | `"}"` |
|     - | 3207 | `"function __reflect_export_param($p){"` |
|     - | 3208 | `" $s = 'Parameter #'.$p->getPosition().' [ <'.($p->isOptional() ? 'optional' : 'required').'> ';"` |
|     - | 3209 | `" $t = $p->getType();"` |
|     - | 3210 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3211 | `" if($p->isPassedByReference()){ $s .= '&'; }"` |
|     - | 3212 | `" if($p->isVariadic()){ $s .= '...'; }"` |
|     - | 3213 | `" $s .= '$'.$p->getName();"` |
|     - | 3214 | `" if($p->isDefaultValueAvailable()){"` |
|     - | 3215 | `"  try{ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3216 | `"  catch(ReflectionException $e){ $s .= ' = <default>'; }"` |
|     - | 3217 | `" }"` |
|     - | 3218 | `" return $s.' ]';"` |
|     - | 3219 | `"}"` |
|     - | 3220 | `"function __reflect_export_prop($p){"` |
|     - | 3221 | `" $s = 'Property [ ';"` |
|     - | 3222 | `" $s .= $p->isPrivate() ? 'private ' : ($p->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3223 | `" if($p->isStatic()){ $s .= 'static '; }"` |
|     - | 3224 | `" if($p->isReadOnly()){ $s .= 'readonly '; }"` |
|     - | 3225 | `" $t = $p->getType();"` |
|     - | 3226 | `" if($t !== null){ $s .= (string)$t.' '; }"` |
|     - | 3227 | `" $s .= '$'.$p->getName();"` |
|     - | 3228 | `" if($p->hasDefaultValue()){ $s .= ' = '.__reflect_export_value($p->getDefaultValue()); }"` |
|     - | 3229 | `" return $s.' ]'.chr(10);"` |
|     - | 3230 | `"}"` |
|     - | 3231 | `"function __reflect_export_cconst($c){"` |
|     - | 3232 | `" $v = $c->getValue();"` |
|     - | 3233 | `" if(is_int($v)){ $t = 'int'; }"` |
|     - | 3234 | `" else if(is_string($v)){ $t = 'string'; }"` |
|     - | 3235 | `" else if(is_float($v)){ $t = 'float'; }"` |
|     - | 3236 | `" else if(is_bool($v)){ $t = 'bool'; }"` |
|     - | 3237 | `" else if(is_array($v)){ $t = 'array'; }"` |
|     - | 3238 | `" else{ $t = 'null'; }"` |
|     - | 3239 | `" $vs = is_array($v) ? 'Array' : (is_bool($v) ? ($v ? '1' : '') : (string)$v);"` |
|     - | 3240 | `" $vis = $c->isPrivate() ? 'private' : ($c->isProtected() ? 'protected' : 'public');"` |
|     - | 3241 | `" return 'Constant [ '.$vis.' '.$t.' '.$c->name.' ] { '.$vs.' }'.chr(10);"` |
|     - | 3242 | `"}"` |
|     - | 3243 | `"function __reflect_export_fnabs($r, $indent){"` |
|     - | 3244 | `" $tags = $r->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3245 | `" if($r instanceof ReflectionMethod){"` |
|     - | 3246 | `"  if($r->isConstructor()){ $tags .= ', ctor'; }"` |
|     - | 3247 | `"  else if($r->isDestructor()){ $tags .= ', dtor'; }"` |
|     - | 3248 | `"  $decl = $r->getDeclaringClass()->name;"` |
|     - | 3249 | `"  if(strtolower($decl) !== strtolower($r->class)){ $tags .= ', inherits '.$decl; }"` |
|     - | 3250 | `"  else if($r->hasPrototype()){ $tags .= ', prototype '.$r->getPrototype()->class; }"` |
|     - | 3251 | `"  $head = 'Method [ <'.$tags.'> ';"` |
|     - | 3252 | `"  if($r->isAbstract()){ $head .= 'abstract '; }"` |
|     - | 3253 | `"  if($r->isFinal()){ $head .= 'final '; }"` |
|     - | 3254 | `"  if($r->isStatic()){ $head .= 'static '; }"` |
|     - | 3255 | `"  $head .= $r->isPrivate() ? 'private ' : ($r->isProtected() ? 'protected ' : 'public ');"` |
|     - | 3256 | `"  $head .= 'method '.$r->name.' ]';"` |
|     - | 3257 | `" }else{"` |
|     - | 3258 | `"  $kind = $r->isClosure() ? 'Closure' : 'Function';"` |
|     - | 3259 | `"  $head = $kind.' [ <'.$tags.'> function '.$r->name.' ]';"` |
|     - | 3260 | `" }"` |
|     - | 3261 | `" $s = $head.' {'.chr(10);"` |
|     - | 3262 | `" if(!$r->isInternal()){"` |
|     - | 3263 | `"  $s .= '  @@ '.$r->getFileName().' '.$r->getStartLine().' - '.$r->getEndLine().chr(10);"` |
|     - | 3264 | `" }"` |
|     - | 3265 | `" $ps = $r->getParameters();"` |
|     - | 3266 | `" $ret = $r->getReturnType();"` |
|     - | 3267 | `" if(count($ps) > 0 \|\| $ret !== null){"` |
|     - | 3268 | `"  $s .= chr(10).'  - Parameters ['.count($ps).'] {'.chr(10);"` |
|     - | 3269 | `"  foreach($ps as $p){ $s .= '    '.__reflect_export_param($p).chr(10); }"` |
|     - | 3270 | `"  $s .= '  }'.chr(10);"` |
|     - | 3271 | `" }"` |
|     - | 3272 | `" if($ret !== null){ $s .= '  - Return [ '.(string)$ret.' ]'.chr(10); }"` |
|     - | 3273 | `" $s .= '}'.chr(10);"` |
|     - | 3274 | `" if($indent === ''){ return $s; }"` |
|     - | 3275 | `" $lines = explode(chr(10), $s);"` |
|     - | 3276 | `" $out = '';"` |
|     - | 3277 | `" $n = count($lines);"` |
|     - | 3278 | `" for($k = 0; $k < $n; $k++){"` |
|     - | 3279 | `"  if($lines[$k] === '' && $k === $n - 1){ break; }"` |
|     - | 3280 | `"  $out .= ($lines[$k] === '' ? '' : $indent.$lines[$k]).chr(10);"` |
|     - | 3281 | `" }"` |
|     - | 3282 | `" return $out;"` |
|     - | 3283 | `"}"` |
|     - | 3284 | `"function __reflect_export_class($rc){"` |
|     - | 3285 | `" $tags = $rc->isInternal() ? 'internal:Core' : 'user';"` |
|     - | 3286 | `" if($rc->isInterface()){"` |
|     - | 3287 | `"  $head = 'Interface [ <'.$tags.'> interface '.$rc->name.' ]';"` |
|     - | 3288 | `" }else{"` |
|     - | 3289 | `"  $mods = '';"` |
|     - | 3290 | `"  if($rc->isAbstract()){ $mods .= 'abstract '; }"` |
|     - | 3291 | `"  if($rc->isFinal()){ $mods .= 'final '; }"` |
|     - | 3292 | `"  $head = 'Class [ <'.$tags.'> '.$mods.'class '.$rc->name;"` |
|     - | 3293 | `"  $par = $rc->getParentClass();"` |
|     - | 3294 | `"  if($par !== false){ $head .= ' extends '.$par->name; }"` |
|     - | 3295 | `"  $ifs = $rc->getInterfaceNames();"` |
|     - | 3296 | `"  if(count($ifs) > 0){ $head .= ' implements '.implode(', ', $ifs); }"` |
|     - | 3297 | `"  $head .= ' ]';"` |
|     - | 3298 | `" }"` |
|     - | 3299 | `" $s = $head.' {'.chr(10);"` |
|     - | 3300 | `" if(!$rc->isInternal()){"` |
|     - | 3301 | `"  $s .= '  @@ '.$rc->getFileName().' '.$rc->getStartLine().'-'.$rc->getEndLine().chr(10);"` |
|     - | 3302 | `" }"` |
|     - | 3303 | `" $consts = $rc->getReflectionConstants();"` |
|     - | 3304 | `" $s .= chr(10).'  - Constants ['.count($consts).'] {'.chr(10);"` |
|     - | 3305 | `" foreach($consts as $c){ $s .= '    '.__reflect_export_cconst($c); }"` |
|     - | 3306 | `" $s .= '  }'.chr(10);"` |
|     - | 3307 | `" $sp = array();"` |
|     - | 3308 | `" $ip = array();"` |
|     - | 3309 | `" foreach($rc->getProperties() as $p){"` |
|     - | 3310 | `"  if($p->isStatic()){ $sp[] = $p; }else{ $ip[] = $p; }"` |
|     - | 3311 | `" }"` |
|     - | 3312 | `" $sm = array();"` |
|     - | 3313 | `" $im = array();"` |
|     - | 3314 | `" foreach($rc->getMethods() as $m){"` |
|     - | 3315 | `"  if($m->isStatic()){ $sm[] = $m; }else{ $im[] = $m; }"` |
|     - | 3316 | `" }"` |
|     - | 3317 | `" $s .= chr(10).'  - Static properties ['.count($sp).'] {'.chr(10);"` |
|     - | 3318 | `" foreach($sp as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3319 | `" $s .= '  }'.chr(10);"` |
|     - | 3320 | `" $s .= chr(10).'  - Static methods ['.count($sm).'] {'.chr(10);"` |
|     - | 3321 | `" $first = true;"` |
|     - | 3322 | `" foreach($sm as $m){"` |
|     - | 3323 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3324 | `"  $first = false;"` |
|     - | 3325 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3326 | `" }"` |
|     - | 3327 | `" $s .= '  }'.chr(10);"` |
|     - | 3328 | `" $s .= chr(10).'  - Properties ['.count($ip).'] {'.chr(10);"` |
|     - | 3329 | `" foreach($ip as $p){ $s .= '    '.__reflect_export_prop($p); }"` |
|     - | 3330 | `" $s .= '  }'.chr(10);"` |
|     - | 3331 | `" $s .= chr(10).'  - Methods ['.count($im).'] {'.chr(10);"` |
|     - | 3332 | `" $first = true;"` |
|     - | 3333 | `" foreach($im as $m){"` |
|     - | 3334 | `"  if(!$first){ $s .= chr(10); }"` |
|     - | 3335 | `"  $first = false;"` |
|     - | 3336 | `"  $s .= __reflect_export_fnabs($m, '    ');"` |
|     - | 3337 | `" }"` |
|     - | 3338 | `" $s .= '  }'.chr(10);"` |
|     - | 3339 | `" return $s.'}'.chr(10);"` |
|     - | 3340 | `"}"` |
|     - | 3341 | `;` |
|     - | 3342 | `/*` |
|     - | 3343 | ` * Register the __reflect_* thunks and compile the Reflection library.` |
|     - | 3344 | ` * Called from PH7_VmInit while pVm->bCompilingBuiltin is set, right after` |
|     - | 3345 | ` * the core builtin chunks (Exception and friends must exist already).` |
|     - | 3346 | ` */` |
|  3926 | 3347 | `PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)` |
|     5 | 3348 | `{` |
|     - | 3349 | `	static const struct {` |
|     - | 3350 | `		const char *zName;` |
|     - | 3351 | `		ProchHostFunction xFunc;` |
|     - | 3352 | `	} aFunc[] = {` |
|     - | 3353 | `		{ "__reflect_class_info",     vm_builtin_reflect_class_info },` |
|     - | 3354 | `		{ "__reflect_const_value",    vm_builtin_reflect_const_value },` |
|     - | 3355 | `		{ "__reflect_static_value",   vm_builtin_reflect_static_value },` |
|     - | 3356 | `		{ "__reflect_static_set",     vm_builtin_reflect_static_set },` |
|     - | 3357 | `		{ "__reflect_prop_default",   vm_builtin_reflect_prop_default },` |
|     - | 3358 | `		{ "__reflect_new_instance",   vm_builtin_reflect_new_instance },` |
|     - | 3359 | `		{ "__reflect_new_no_ctor",    vm_builtin_reflect_new_no_ctor },` |
|     - | 3360 | `		{ "__reflect_func_info",      vm_builtin_reflect_func_info },` |
|     - | 3361 | `		{ "__reflect_param_default",  vm_builtin_reflect_param_default },` |
|     - | 3362 | `		{ "__reflect_param_defconst", vm_builtin_reflect_param_defconst },` |
|     - | 3363 | `		{ "__reflect_invoke",         vm_builtin_reflect_invoke },` |
|     - | 3364 | `		{ "__reflect_closure",        vm_builtin_reflect_closure },` |
|     - | 3365 | `		{ "__reflect_prop_read",      vm_builtin_reflect_prop_read },` |
|     - | 3366 | `		{ "__reflect_prop_write",     vm_builtin_reflect_prop_write },` |
|     - | 3367 | `		{ "__reflect_prop_state",     vm_builtin_reflect_prop_state },` |
|     - | 3368 | `		{ "__reflect_dyn_props",      vm_builtin_reflect_dyn_props },` |
|     - | 3369 | `		{ "__reflect_gen_info",       vm_builtin_reflect_gen_info },` |
|     - | 3370 | `		{ "__reflect_gen_exec",       vm_builtin_reflect_gen_exec },` |
|     - | 3371 | `		{ "__reflect_const_info",     vm_builtin_reflect_const_info },` |
|     - | 3372 | `		{ "__reflect_ref_id",         vm_builtin_reflect_ref_id },` |
|     - | 3373 | `		{ "__reflect_attr_args",      vm_builtin_reflect_attr_args },` |
|     - | 3374 | `	};` |
|     - | 3375 | `	sxu32 n;` |
|     - | 3376 | `	sxi32 rc;` |
| 86377 | 3377 | `	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){` |
| 82451 | 3378 | `		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);` |
| 41228 | 3379 | `	}` |
|  3931 | 3380 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib1, sizeof(zReflectLib1)-1);` |
|  3931 | 3381 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3382 | `		return rc;` |
|     - | 3383 | `	}` |
|  3931 | 3384 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib2, sizeof(zReflectLib2)-1);` |
|  3931 | 3385 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3386 | `		return rc;` |
|     - | 3387 | `	}` |
|  3931 | 3388 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib3, sizeof(zReflectLib3)-1);` |
|  3931 | 3389 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3390 | `		return rc;` |
|     - | 3391 | `	}` |
|  3931 | 3392 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib4, sizeof(zReflectLib4)-1);` |
|  3931 | 3393 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3394 | `		return rc;` |
|     - | 3395 | `	}` |
|  3931 | 3396 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib5, sizeof(zReflectLib5)-1);` |
|  3931 | 3397 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3398 | `		return rc;` |
|     - | 3399 | `	}` |
|  3931 | 3400 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib6, sizeof(zReflectLib6)-1);` |
|  3931 | 3401 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3402 | `		return rc;` |
|     - | 3403 | `	}` |
|  3931 | 3404 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib7, sizeof(zReflectLib7)-1);` |
|  3931 | 3405 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3406 | `		return rc;` |
|     - | 3407 | `	}` |
|  3931 | 3408 | `	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib8, sizeof(zReflectLib8)-1);` |
|  3931 | 3409 | `	if( rc != SXRET_OK ){` |
|   ! 0 | 3410 | `		return rc;` |
|     - | 3411 | `	}` |
|  3931 | 3412 | `	return PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib9, sizeof(zReflectLib9)-1);` |
|  1968 | 3413 | `}` |
|     - | 3414 |  |
