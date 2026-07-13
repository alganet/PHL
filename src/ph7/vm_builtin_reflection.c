/**
 * SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include "ph7int.h"
/*
 * This file implements the PHP 8.5 Reflection API.
 *
 * Following the engine's builtin-class pattern (Generator/Fiber/Closure),
 * the Reflection classes themselves are written in PHP, embedded below as
 * C string chunks and compiled at VM init by PH7_VmInstallReflection().
 * Native behavior is provided by a small set of global __reflect_* thunk
 * functions implemented here: the PHP methods forward to them, passing
 * their target (class name, object, ...) explicitly.
 *
 * The chunks are kept below 30 KB each: MSVC caps a concatenated string
 * literal at 65,535 bytes and the Windows build is real (build-aux/nmake.mk).
 */

/* Bound on hierarchy walks; matches PH7_INTERFACE_WALK_MAX_DEPTH in
 * vm_builtin_class.c. */
#define REFLECT_WALK_MAX_DEPTH 64

/*
 * Resolve a class-name string or object into a ph7_class pointer,
 * triggering autoload for unknown string names. Returns NULL when the
 * class does not exist (the PHP layer turns that into ReflectionException).
 */
static ph7_class * ReflectResolveClass(ph7_vm *pVm, ph7_value *pArg)
{
	ph7_class *pClass;
	pClass = PH7_VmExtractClassFromValue(pVm, pArg);
	if( pClass == 0 && ph7_value_is_string(pArg) ){
		const char *zName;
		int nLen;
		zName = ph7_value_to_string(pArg, &nLen);
		if( nLen > 0 ){
			pClass = PH7_VmTriggerAutoload(pVm, zName, (sxu32)nLen, FALSE);
		}
	}
	return pClass;
}
/*
 * Hand a freshly created class instance to the caller. The return slot
 * takes over the initial reference from PH7_NewClassInstance (iRef=1):
 * no extra iRef++ here (see the synthesized-object invariant — a stray
 * bump leaks the object and disables its __destruct).
 */
static int ReflectResultObject(ph7_context *pCtx, ph7_class_instance *pObj)
{
	if( pObj == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjRelease(pCtx->pRet);
	pCtx->pRet->x.pOther = pObj;
	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);
	return PH7_OK;
}
/* --- Marshaling helpers: build the descriptor arrays handed to the PHP layer --- */
static void ReflectMapAddBool(ph7_context *pCtx, ph7_value *pMap, const char *zKey, int b)
{
	ph7_value *p = ph7_context_new_scalar(pCtx);
	if( p == 0 ){ return; }
	ph7_value_bool(p, b);
	ph7_array_add_strkey_elem(pMap, zKey, p);
}
static void ReflectMapAddInt(ph7_context *pCtx, ph7_value *pMap, const char *zKey, sxi64 iVal)
{
	ph7_value *p = ph7_context_new_scalar(pCtx);
	if( p == 0 ){ return; }
	ph7_value_int64(p, iVal);
	ph7_array_add_strkey_elem(pMap, zKey, p);
}
static void ReflectMapAddStr(ph7_context *pCtx, ph7_value *pMap, const char *zKey,
	const char *zVal, int nVal)
{
	ph7_value *p = ph7_context_new_scalar(pCtx);
	if( p == 0 ){ return; }
	ph7_value_string(p, zVal, nVal);
	ph7_array_add_strkey_elem(pMap, zKey, p);
}
static void ReflectMapAddNull(ph7_context *pCtx, ph7_value *pMap, const char *zKey)
{
	ph7_value *p = ph7_context_new_scalar(pCtx);
	if( p == 0 ){ return; }
	ph7_value_null(p);
	ph7_array_add_strkey_elem(pMap, zKey, p);
}
/* Add an entry under a dynamic (SyString) key. */
static void ReflectMapAddDyn(ph7_context *pCtx, ph7_value *pMap,
	const SyString *pKey, ph7_value *pVal)
{
	ph7_value *pK = ph7_context_new_scalar(pCtx);
	if( pK == 0 ){ return; }
	ph7_value_string(pK, pKey->zString, (int)pKey->nByte);
	ph7_array_add_elem(pMap, pK, pVal);
}
/*
 * Append pIface (and its parents / extended interfaces) to the dedup set
 * of ph7_class pointers.
 */
static void ReflectAddInterface(ph7_class *pIface, SySet *pOut, int iDepth)
{
	ph7_class **apKnown;
	sxu32 n;
	if( pIface == 0 || iDepth > REFLECT_WALK_MAX_DEPTH ){
		return;
	}
	/* Parents of an interface come along too (interface B extends A) */
	if( pIface->pBase ){
		ReflectAddInterface(pIface->pBase, pOut, iDepth + 1);
	}
	/* Some engines record extended interfaces in aInterface as well */
	apKnown = (ph7_class **)SySetBasePtr(&pIface->aInterface);
	for( n = 0 ; n < SySetUsed(&pIface->aInterface) ; n++ ){
		ReflectAddInterface(apKnown[n], pOut, iDepth + 1);
	}
	/* Dedup by pointer */
	apKnown = (ph7_class **)SySetBasePtr(pOut);
	for( n = 0 ; n < SySetUsed(pOut) ; n++ ){
		if( apKnown[n] == pIface ){
			return;
		}
	}
	SySetPut(pOut, (const void *)&pIface);
}
/*
 * Collect the transitive set of interfaces implemented by pClass:
 * the parent chain's interfaces first, then the class's own.
 */
static void ReflectCollectInterfaces(ph7_class *pClass, SySet *pOut, int iDepth)
{
	ph7_class **apIface;
	sxu32 n;
	if( pClass == 0 || iDepth > REFLECT_WALK_MAX_DEPTH ){
		return;
	}
	if( pClass->pBase ){
		ReflectCollectInterfaces(pClass->pBase, pOut, iDepth + 1);
	}
	apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);
	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){
		ReflectAddInterface(apIface[n], pOut, iDepth + 1);
	}
}
/*
 * Deepest base class whose method table maps the same name to the very
 * same ph7_class_method pointer: inheritance shares member pointers
 * (PH7_ClassInherit), so this identifies the declaring class. Methods
 * copied in from traits are not on the pBase chain and thus report the
 * using class, which is what PHP reports too.
 */
static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth)
{
	ph7_class *pDecl = pClass;
	ph7_class *pBase = pClass->pBase;
	int iDepth = 0;
	while( pBase && iDepth <= REFLECT_WALK_MAX_DEPTH ){
		SyHashEntry *pEntry;
		pEntry = SyHashGet(&pBase->hMethod, (const void *)SyStringData(&pMeth->sFunc.sName),
			SyStringLength(&pMeth->sFunc.sName));
		if( pEntry == 0 || (ph7_class_method *)pEntry->pUserData != pMeth ){
			break;
		}
		pDecl = pBase;
		pBase = pBase->pBase;
		iDepth++;
	}
	return pDecl;
}
/* Fetch a class attribute (property or constant) by plain name. */
static ph7_class_attr * ReflectFetchAttr(ph7_class *pClass, ph7_value *pName)
{
	SyHashEntry *pEntry;
	const char *zName;
	int nLen;
	zName = ph7_value_to_string(pName, &nLen);
	if( nLen < 1 ){
		return 0;
	}
	pEntry = SyHashGet(&pClass->hAttr, (const void *)zName, (sxu32)nLen);
	if( pEntry == 0 ){
		return 0;
	}
	return (ph7_class_attr *)pEntry->pUserData;
}
/*
 * array|null __reflect_class_info(object|string $target)
 *
 * Full class descriptor, or null when the class cannot be resolved (after
 * an autoload attempt). Shape:
 *   name, internal, interface, trait, abstract, final, readonly, iterable (bool),
 *   parent (string|null), interfaces (list), traits (list),
 *   file (string|false), line, endline (int),
 *   ctorvis, clonevis (0 = absent, else PH7_CLASS_PROT_*),
 *   consts  {name: {vis, final, decl, line}},
 *   props   {name: {vis, static, readonly, hasdef, decl, line}},
 *   methods {name: {vis, static, abstract, final, decl, line}}
 */
static int vm_builtin_reflect_class_info(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass;
	ph7_value *pInfo, *pConsts, *pProps, *pMethods, *pList;
	SyHashEntry *pEntry;
	SySet aIfaceSet;
	sxi32 iCtorVis = 0, iCloneVis = 0;
	int bIterable = 0;
	sxu32 n;
	if( nArg < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pClass = ReflectResolveClass(pVm, apArg[0]);
	if( pClass == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pInfo = ph7_context_new_array(pCtx);
	pConsts = ph7_context_new_array(pCtx);
	pProps = ph7_context_new_array(pCtx);
	pMethods = ph7_context_new_array(pCtx);
	if( pInfo == 0 || pConsts == 0 || pProps == 0 || pMethods == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));
	ReflectMapAddBool(pCtx, pInfo, "internal", (pClass->iFlags & PH7_CLASS_INTERNAL) != 0);
	ReflectMapAddBool(pCtx, pInfo, "interface", (pClass->iFlags & PH7_CLASS_INTERFACE) != 0);
	ReflectMapAddBool(pCtx, pInfo, "trait", (pClass->iFlags & PH7_CLASS_TRAIT) != 0);
	ReflectMapAddBool(pCtx, pInfo, "abstract", (pClass->iFlags & PH7_CLASS_ABSTRACT) != 0);
	ReflectMapAddBool(pCtx, pInfo, "final", (pClass->iFlags & PH7_CLASS_FINAL) != 0);
	ReflectMapAddBool(pCtx, pInfo, "readonly", (pClass->iFlags & PH7_CLASS_READONLY) != 0);
	if( pClass->pBase ){
		ReflectMapAddStr(pCtx, pInfo, "parent", SyStringData(&pClass->pBase->sName),
			(int)SyStringLength(&pClass->pBase->sName));
	}else{
		ReflectMapAddNull(pCtx, pInfo, "parent");
	}
	/* Transitive interfaces */
	SySetInit(&aIfaceSet, &pVm->sAllocator, sizeof(ph7_class *));
	ReflectCollectInterfaces(pClass, &aIfaceSet, 0);
	if( pClass->iFlags & PH7_CLASS_INTERFACE ){
		/* An interface's own parents count as its interface list */
		if( pClass->pBase ){
			ReflectAddInterface(pClass->pBase, &aIfaceSet, 0);
		}
	}
	pList = ph7_context_new_array(pCtx);
	if( pList ){
		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIfaceSet);
		for( n = 0 ; n < SySetUsed(&aIfaceSet) ; n++ ){
			ph7_value *pName = ph7_context_new_scalar(pCtx);
			if( pName == 0 ){ break; }
			ph7_value_string(pName, SyStringData(&apIface[n]->sName), (int)SyStringLength(&apIface[n]->sName));
			ph7_array_add_elem(pList, 0, pName);
			if( pVm->pTraversableClass && apIface[n] == pVm->pTraversableClass ){
				bIterable = 1;
			}
		}
		ph7_array_add_strkey_elem(pInfo, "interfaces", pList);
	}
	SySetRelease(&aIfaceSet);
	ReflectMapAddBool(pCtx, pInfo, "iterable", bIterable);
	/* Used traits */
	pList = ph7_context_new_array(pCtx);
	if( pList ){
		ph7_class **apTrait = (ph7_class **)SySetBasePtr(&pClass->aTrait);
		for( n = 0 ; n < SySetUsed(&pClass->aTrait) ; n++ ){
			ph7_value *pName = ph7_context_new_scalar(pCtx);
			if( pName == 0 ){ break; }
			ph7_value_string(pName, SyStringData(&apTrait[n]->sName), (int)SyStringLength(&apTrait[n]->sName));
			ph7_array_add_elem(pList, 0, pName);
		}
		ph7_array_add_strkey_elem(pInfo, "traits", pList);
	}
	/* File / lines: no file recorded => false, like PHP internals */
	if( SyStringLength(&pClass->sFile) > 0 ){
		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));
	}else{
		ReflectMapAddBool(pCtx, pInfo, "file", 0);
	}
	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pClass->nLine);
	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pClass->nEndLine);
	/* Members are emitted in PHP's reporting order: the class's own members
	 * first (declaration order), then each inheritance level's, outward.
	 * Per level we iterate the DECLARING class's own hash — subclass hashes
	 * interleave inherited pointers unpredictably — and emit buffered
	 * entries in reverse, because SyHash lists are LIFO. A pointer-identity
	 * lookup in the reflected class's hash filters out members that are not
	 * visible there (base privates, overridden entries). */
	{
		ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];
		ph7_class *pWalk = pClass;
		SySet aTmp;
		sxu32 nChain = 0, iLevel, nT;
		while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){
			aChain[nChain++] = pWalk;
			pWalk = pWalk->pBase;
		}
		SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));
		for( iLevel = 0 ; iLevel < nChain ; iLevel++ ){
			ph7_class *pLevel = aChain[iLevel];
			/* --- Constants and properties (shared attribute table) --- */
			SySetReset(&aTmp);
			SyHashResetLoopCursor(&pLevel->hAttr);
			while( (pEntry = SyHashGetNextEntry(&pLevel->hAttr)) != 0 ){
				ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;
				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;
				if( iLevel == 0 ){
					sxu32 j;
					/* Own = declared here or by an off-chain provider (trait) */
					for( j = 1 ; j < nChain ; j++ ){
						if( aChain[j] == pDecl ){ break; }
					}
					if( j < nChain ){ continue; }
				}else{
					SyHashEntry *pSub;
					if( pDecl != pLevel ){ continue; }
					/* Must still be the visible member in the reflected class */
					pSub = SyHashGet(&pClass->hAttr, pEntry->pKey, pEntry->nKeyLen);
					if( pSub == 0 || pSub->pUserData != (void *)pAttr ){ continue; }
				}
				SySetPut(&aTmp, (const void *)&pEntry);
			}
			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){
				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);
				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;
				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;
				ph7_value *pMeta = ph7_context_new_array(pCtx);
				if( pMeta == 0 ){ break; }
				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pAttr->iProtection);
				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));
				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pAttr->nLine);
				if( pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT ){
					ReflectMapAddBool(pCtx, pMeta, "final", (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0);
					ReflectMapAddDyn(pCtx, pConsts, &pAttr->sName, pMeta);
				}else{
					ReflectMapAddBool(pCtx, pMeta, "static", (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0);
					ReflectMapAddBool(pCtx, pMeta, "readonly", (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0);
					ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&pAttr->aByteCode) > 0);
					ReflectMapAddDyn(pCtx, pProps, &pAttr->sName, pMeta);
				}
			}
			/* --- Methods. The reported name is the hash-entry key: trait
			 * aliasing installs a shallow copy under the alias name while
			 * sFunc.sName keeps the original, and PHP reports the alias. --- */
			SySetReset(&aTmp);
			SyHashResetLoopCursor(&pLevel->hMethod);
			while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){
				ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;
				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);
				if( iLevel == 0 ){
					sxu32 j;
					for( j = 1 ; j < nChain ; j++ ){
						if( aChain[j] == pDecl ){ break; }
					}
					if( j < nChain ){ continue; }
				}else{
					SyHashEntry *pSub;
					if( pDecl != pLevel ){ continue; }
					pSub = SyHashGet(&pClass->hMethod, pEntry->pKey, pEntry->nKeyLen);
					if( pSub == 0 ){
						/* Not in the subclass table: inheritance skips private
						 * methods, but PHP still reports them on the subclass
						 * (Zend copies privates into the child function table). */
						if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){
							continue;
						}
					}else if( pSub->pUserData != (void *)pMeth ){
						/* Overridden below this level: already reported */
						continue;
					}
				}
				SySetPut(&aTmp, (const void *)&pEntry);
			}
			for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){
				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);
				ph7_class_method *pMeth = (ph7_class_method *)pE->pUserData;
				ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);
				ph7_value *pMeta;
				SyString sKey;
				int bIsAlias;
				SyStringInitFromBuf(&sKey, (const char *)pE->pKey, pE->nKeyLen);
				bIsAlias = (sKey.nByte != SyStringLength(&pMeth->sFunc.sName)
				 || SyMemcmp(sKey.zString, SyStringData(&pMeth->sFunc.sName), sKey.nByte) != 0);
				if( sKey.nByte == sizeof("__construct")-1
				 && SyMemcmp(sKey.zString, "__construct", sKey.nByte) == 0 ){
					if( iCtorVis == 0 ){
						iCtorVis = pMeth->iProtection;
					}
					if( bIsAlias ){
						/* Mount-time alias for a legacy class-name constructor:
						 * the method is already listed under its declared name. */
						continue;
					}
				}else if( sKey.nByte == sizeof("__clone")-1
				 && SyMemcmp(sKey.zString, "__clone", sKey.nByte) == 0 ){
					if( iCloneVis == 0 ){
						iCloneVis = pMeth->iProtection;
					}
				}else if( iCtorVis == 0
				 && sKey.nByte == SyStringLength(&pClass->sName)
				 && SyMemcmp(sKey.zString, SyStringData(&pClass->sName), sKey.nByte) == 0 ){
					/* Legacy class-name constructor before the mount alias exists */
					iCtorVis = pMeth->iProtection;
				}
				pMeta = ph7_context_new_array(pCtx);
				if( pMeta == 0 ){ break; }
				ReflectMapAddInt(pCtx, pMeta, "vis", (sxi64)pMeth->iProtection);
				ReflectMapAddBool(pCtx, pMeta, "static", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);
				ReflectMapAddBool(pCtx, pMeta, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);
				ReflectMapAddBool(pCtx, pMeta, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);
				ReflectMapAddStr(pCtx, pMeta, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));
				ReflectMapAddInt(pCtx, pMeta, "line", (sxi64)pMeth->nLine);
				ReflectMapAddDyn(pCtx, pMethods, &sKey, pMeta);
			}
		}
		SySetRelease(&aTmp);
	}
	ReflectMapAddInt(pCtx, pInfo, "ctorvis", (sxi64)iCtorVis);
	ReflectMapAddInt(pCtx, pInfo, "clonevis", (sxi64)iCloneVis);
	ph7_array_add_strkey_elem(pInfo, "consts", pConsts);
	ph7_array_add_strkey_elem(pInfo, "props", pProps);
	ph7_array_add_strkey_elem(pInfo, "methods", pMethods);
	ph7_result_value(pCtx, pInfo);
	return PH7_OK;
}
/*
 * mixed __reflect_const_value(string $class, string $name)
 * Value of a class constant. The PHP layer guarantees existence.
 */
static int vm_builtin_reflect_const_value(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass;
	ph7_class_attr *pAttr;
	ph7_value *pValue;
	if( nArg < 2 || (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0
	 || (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0
	 || (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Constant slots are evaluated when the class is mounted */
	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);
	if( pValue ){
		ph7_result_value(pCtx, pValue);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/*
 * mixed __reflect_static_value(string $class, string $name)
 * Current value of a static property (visibility ignored).
 */
static int vm_builtin_reflect_static_value(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass;
	ph7_class_attr *pAttr;
	ph7_value *pValue;
	if( nArg < 2 || (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0
	 || (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0
	 || (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);
	if( pValue ){
		ph7_result_value(pCtx, pValue);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/*
 * bool __reflect_static_set(string $class, string $name, mixed $value)
 * Overwrite a static property's shared slot (visibility ignored).
 */
static int vm_builtin_reflect_static_set(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass;
	ph7_class_attr *pAttr;
	ph7_value *pValue;
	if( nArg < 3 || (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0
	 || (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0
	 || (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);
	if( pValue == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	PH7_MemObjStore(apArg[2], pValue);
	ph7_result_bool(pCtx, 1);
	return PH7_OK;
}
/*
 * mixed __reflect_prop_default(string $class, string $name)
 * Evaluate a non-static property's compiled default expression
 * (null when the property has no default).
 */
static int vm_builtin_reflect_prop_default(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass;
	ph7_class_attr *pAttr;
	ph7_value sValue;
	if( nArg < 2 || (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0
	 || (pAttr = ReflectFetchAttr(pClass, apArg[1])) == 0
	 || (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) != 0
	 || SySetUsed(&pAttr->aByteCode) < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjInit(pCtx->pVm, &sValue);
	/* Same evaluation path the VM uses for omitted call arguments */
	VmLocalExec(pCtx->pVm, &pAttr->aByteCode, &sValue, FALSE);
	ph7_result_value(pCtx, &sValue);
	PH7_MemObjRelease(&sValue);
	return PH7_OK;
}
/*
 * Collect the values of a PHP list array into a ph7_value* set
 * (positional constructor arguments).
 */
static sxi32 ReflectCollectArgs(ph7_context *pCtx, ph7_value *pArray, SySet *pOut)
{
	ph7_hashmap *pMap;
	ph7_hashmap_node *pEntry;
	sxu32 n;
	if( !ph7_value_is_array(pArray) ){
		return SXRET_OK;
	}
	pMap = (ph7_hashmap *)pArray->x.pOther;
	pEntry = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);
		if( pValue ){
			SySetPut(pOut, (const void *)&pValue);
		}
		pEntry = pEntry->pPrev; /* Reverse link: insertion order */
	}
	return SXRET_OK;
}
/*
 * object __reflect_new_instance(string $class, array $args)
 * Instantiate and run the constructor with positional arguments.
 * The PHP layer has already validated instantiability and ctor visibility.
 */
static int vm_builtin_reflect_new_instance(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass;
	ph7_class_instance *pThis;
	ph7_class_method *pCons;
	if( nArg < 1 || (pClass = ReflectResolveClass(pVm, apArg[0])) == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pThis = PH7_NewClassInstance(pVm, pClass);
	if( pThis == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);
	if( pCons ){
		SySet aArg;
		sxi32 rc;
		SySetInit(&aArg, &pVm->sAllocator, sizeof(ph7_value *));
		if( nArg > 1 ){
			ReflectCollectArgs(pCtx, apArg[1], &aArg);
		}
		rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),
			(ph7_value **)SySetBasePtr(&aArg));
		SySetRelease(&aArg);
		if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){
			PH7_ClassInstanceUnref(pThis);
			return rc;
		}
	}
	return ReflectResultObject(pCtx, pThis);
}
/*
 * object __reflect_new_no_ctor(string $class)
 * Instantiate without running the constructor (property defaults still
 * apply — PH7_NewClassInstance builds the attribute frame).
 */
static int vm_builtin_reflect_new_no_ctor(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass;
	if( nArg < 1 || (pClass = ReflectResolveClass(pCtx->pVm, apArg[0])) == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultObject(pCtx, PH7_NewClassInstance(pCtx->pVm, pClass));
}
/*
 * The Reflection classes, in PHP. Chunk 1: exceptions, Reflector,
 * Reflection, ReflectionClass, ReflectionObject (plus get_debug_type,
 * which the TypeError messages need and PHP 8.0 ships natively).
 */
static const char zReflectLib1[] =
"function get_debug_type($value){"
" if(is_object($value)){ return get_class($value); }"
" if(is_bool($value)){ return 'bool'; }"
" if(is_int($value)){ return 'int'; }"
" if(is_float($value)){ return 'float'; }"
" if(is_string($value)){ return 'string'; }"
" if(is_array($value)){ return 'array'; }"
" if($value === null){ return 'null'; }"
" return gettype($value);"
"}"
"interface Reflector extends Stringable {}"
"class ReflectionException extends Exception {}"
"class Reflection {"
" public static function getModifierNames($modifiers){"
"  $names = array();"
"  if($modifiers & 64){ $names[] = 'abstract'; }"
"  if($modifiers & 32){ $names[] = 'final'; }"
"  if($modifiers & 1){ $names[] = 'public'; }"
"  if($modifiers & 2){ $names[] = 'protected'; }"
"  if($modifiers & 4){ $names[] = 'private'; }"
"  if($modifiers & 16){ $names[] = 'static'; }"
"  if($modifiers & 128){ $names[] = 'readonly'; }"
"  return $names;"
" }"
"}"
"class ReflectionClass implements Reflector {"
" const IS_IMPLICIT_ABSTRACT = 16;"
" const IS_EXPLICIT_ABSTRACT = 64;"
" const IS_FINAL = 32;"
" const IS_READONLY = 65536;"
" const SKIP_INITIALIZATION_ON_SERIALIZE = 8;"
" const SKIP_DESTRUCTOR = 16;"
" public $name;"
" public function __construct($objectOrClass){"
"  if(!is_object($objectOrClass) && !is_string($objectOrClass)){"
"   if(is_int($objectOrClass) || is_float($objectOrClass) || is_bool($objectOrClass)){"
"    $objectOrClass = (string)$objectOrClass;"
"   }else{"
"    throw new TypeError('ReflectionClass::__construct(): Argument #1 ($objectOrClass) must be of type object|string, '.get_debug_type($objectOrClass).' given');"
"   }"
"  }"
"  $info = __reflect_class_info($objectOrClass);"
"  if($info === null){"
"   throw new ReflectionException('Class \"'.$objectOrClass.'\" does not exist');"
"  }"
"  $this->name = $info['name'];"
" }"
" protected function __rinfo(){ return __reflect_class_info($this->name); }"
" public function getName(){ return $this->name; }"
" public function getShortName(){"
"  $p = strrpos($this->name,'\\\\');"
"  if($p === false){ return $this->name; }"
"  return substr($this->name,$p+1);"
" }"
" public function getNamespaceName(){"
"  $p = strrpos($this->name,'\\\\');"
"  if($p === false){ return ''; }"
"  return substr($this->name,0,$p);"
" }"
" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"
" public function isInternal(){ $i = $this->__rinfo(); return $i['internal']; }"
" public function isUserDefined(){ return !$this->isInternal(); }"
" public function isInterface(){ $i = $this->__rinfo(); return $i['interface']; }"
" public function isTrait(){ $i = $this->__rinfo(); return $i['trait']; }"
" public function isAbstract(){ $i = $this->__rinfo(); return $i['abstract']; }"
" public function isFinal(){ $i = $this->__rinfo(); return $i['final']; }"
" public function isReadOnly(){ $i = $this->__rinfo(); return $i['readonly']; }"
" public function isEnum(){ return false; }"
" public function isAnonymous(){ return strpos($this->name,'class@anonymous') === 0; }"
" public function getModifiers(){"
"  $i = $this->__rinfo();"
"  $m = 0;"
"  if($i['abstract']){ $m |= 64; }"
"  if($i['final']){ $m |= 32; }"
"  if($i['readonly']){ $m |= 65536; }"
"  return $m;"
" }"
" public function getParentClass(){"
"  $i = $this->__rinfo();"
"  if($i['parent'] === null){ return false; }"
"  return new ReflectionClass($i['parent']);"
" }"
" public function getInterfaceNames(){ $i = $this->__rinfo(); return $i['interfaces']; }"
" public function getInterfaces(){"
"  $i = $this->__rinfo();"
"  $out = array();"
"  foreach($i['interfaces'] as $n){ $out[$n] = new ReflectionClass($n); }"
"  return $out;"
" }"
" public function getTraitNames(){ $i = $this->__rinfo(); return $i['traits']; }"
" public function getTraits(){"
"  $i = $this->__rinfo();"
"  $out = array();"
"  foreach($i['traits'] as $n){ $out[$n] = new ReflectionClass($n); }"
"  return $out;"
" }"
" public function getTraitAliases(){ return array(); }"
" public function implementsInterface($interface){"
"  if($interface instanceof ReflectionClass){ $interface = $interface->name; }"
"  $target = __reflect_class_info($interface);"
"  if($target === null){"
"   throw new ReflectionException('Interface \"'.$interface.'\" does not exist');"
"  }"
"  if(!$target['interface']){"
"   throw new ReflectionException($target['name'].' is not an interface');"
"  }"
"  $name = $target['name'];"
"  if($this->name === $name){ return true; }"
"  $i = $this->__rinfo();"
"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"
"  return false;"
" }"
" public function isSubclassOf($class){"
"  if($class instanceof ReflectionClass){ $class = $class->name; }"
"  $target = __reflect_class_info($class);"
"  if($target === null){"
"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"
"  }"
"  $name = $target['name'];"
"  if($name === $this->name){ return false; }"
"  $i = $this->__rinfo();"
"  $p = $i['parent'];"
"  while($p !== null){"
"   if($p === $name){ return true; }"
"   $pi = __reflect_class_info($p);"
"   $p = $pi['parent'];"
"  }"
"  foreach($i['interfaces'] as $n){ if($n === $name){ return true; } }"
"  return false;"
" }"
" public function isInstance($object){"
"  if(!is_object($object)){"
"   throw new TypeError('ReflectionClass::isInstance(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"
"  }"
"  return is_a($object,$this->name);"
" }"
" public function hasMethod($name){"
"  $i = $this->__rinfo();"
"  $l = strtolower($name);"
"  foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ return true; } }"
"  return false;"
" }"
" public function hasProperty($name){ $i = $this->__rinfo(); return isset($i['props'][$name]); }"
" public function hasConstant($name){ $i = $this->__rinfo(); return isset($i['consts'][$name]); }"
" public function getConstant($name){"
"  $i = $this->__rinfo();"
"  if(!isset($i['consts'][$name])){ return false; }"
"  return __reflect_const_value($this->name,$name);"
" }"
" public function getConstants($filter = null){"
"  $i = $this->__rinfo();"
"  $out = array();"
"  foreach($i['consts'] as $k => $c){"
"   if($filter !== null){"
"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"
"    if(($m & $filter) === 0){ continue; }"
"   }"
"   $out[$k] = __reflect_const_value($this->name,$k);"
"  }"
"  return $out;"
" }"
" public function getStartLine(){"
"  $i = $this->__rinfo();"
"  if($i['internal']){ return false; }"
"  return $i['line'];"
" }"
" public function getEndLine(){"
"  $i = $this->__rinfo();"
"  if($i['internal']){ return false; }"
"  return $i['endline'];"
" }"
" public function getFileName(){ $i = $this->__rinfo(); return $i['file']; }"
" public function getDocComment(){ return false; }"
" public function isInstantiable(){"
"  $i = $this->__rinfo();"
"  if($i['interface'] || $i['trait'] || $i['abstract']){ return false; }"
"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){ return false; }"
"  return true;"
" }"
" public function isCloneable(){"
"  $i = $this->__rinfo();"
"  if($i['interface'] || $i['trait'] || $i['abstract']){ return false; }"
"  if($i['clonevis'] !== 0 && $i['clonevis'] !== 1){ return false; }"
"  return true;"
" }"
" public function isIterable(){"
"  $i = $this->__rinfo();"
"  if($i['interface'] || $i['trait'] || $i['abstract']){ return false; }"
"  return $i['iterable'];"
" }"
" public function isIterateable(){ return $this->isIterable(); }"
" public function newInstance(...$args){ return $this->__rnew($args); }"
" public function newInstanceArgs(array $args = array()){ return $this->__rnew($args); }"
" protected function __rnew($args){"
"  $i = $this->__rinfo();"
"  $this->__rcheckInstantiable($i);"
"  if($i['ctorvis'] !== 0 && $i['ctorvis'] !== 1){"
"   throw new ReflectionException('Access to non-public constructor of class '.$this->name);"
"  }"
"  if($i['ctorvis'] === 0 && count($args) > 0){"
"   throw new ReflectionException('Class '.$this->name.' does not have a constructor, so you cannot pass any constructor arguments');"
"  }"
"  return __reflect_new_instance($this->name,$args);"
" }"
" protected function __rcheckInstantiable($i){"
"  if($i['interface']){ throw new Error('Cannot instantiate interface '.$this->name); }"
"  if($i['trait']){ throw new Error('Cannot instantiate trait '.$this->name); }"
"  if($i['abstract']){ throw new Error('Cannot instantiate abstract class '.$this->name); }"
" }"
" public function newInstanceWithoutConstructor(){"
"  $i = $this->__rinfo();"
"  $this->__rcheckInstantiable($i);"
"  return __reflect_new_no_ctor($this->name);"
" }"
" public function getStaticProperties(){"
"  $i = $this->__rinfo();"
"  $out = array();"
"  foreach($i['props'] as $k => $p){"
"   if($p['static']){ $out[$k] = __reflect_static_value($this->name,$k); }"
"  }"
"  return $out;"
" }"
" public function getStaticPropertyValue($name, ...$def){"
"  $i = $this->__rinfo();"
"  if(!isset($i['props'][$name]) || !$i['props'][$name]['static']){"
"   if(count($def) > 0){ return $def[0]; }"
"   throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"
"  }"
"  return __reflect_static_value($this->name,$name);"
" }"
" public function setStaticPropertyValue($name,$value){"
"  $i = $this->__rinfo();"
"  if(!isset($i['props'][$name]) || !$i['props'][$name]['static']){"
"   throw new ReflectionException('Class '.$this->name.' does not have a property named '.$name);"
"  }"
"  __reflect_static_set($this->name,$name,$value);"
" }"
" public function getDefaultProperties(){"
"  $i = $this->__rinfo();"
"  $out = array();"
"  foreach($i['props'] as $k => $p){"
"   if($p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"
"  }"
"  foreach($i['props'] as $k => $p){"
"   if(!$p['static']){ $out[$k] = __reflect_prop_default($this->name,$k); }"
"  }"
"  return $out;"
" }"
" public function getAttributes($name = null, $flags = 0){ return array(); }"
" public function getExtensionName(){ $i = $this->__rinfo(); return $i['internal'] ? 'Core' : false; }"
" public function __toString(){"
"  return 'Class [ class '.$this->name.' ] {'.\"\\n\".'}'.\"\\n\";"
" }"
"}"
"class ReflectionObject extends ReflectionClass {"
" public function __construct($object){"
"  if(!is_object($object)){"
"   throw new TypeError('ReflectionObject::__construct(): Argument #1 ($object) must be of type object, '.get_debug_type($object).' given');"
"  }"
"  parent::__construct($object);"
" }"
"}"
;
/*
 * Register the __reflect_* thunks and compile the Reflection library.
 * Called from PH7_VmInit while pVm->bCompilingBuiltin is set, right after
 * the core builtin chunks (Exception and friends must exist already).
 */
PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)
{
	static const struct {
		const char *zName;
		ProchHostFunction xFunc;
	} aFunc[] = {
		{ "__reflect_class_info",   vm_builtin_reflect_class_info },
		{ "__reflect_const_value",  vm_builtin_reflect_const_value },
		{ "__reflect_static_value", vm_builtin_reflect_static_value },
		{ "__reflect_static_set",   vm_builtin_reflect_static_set },
		{ "__reflect_prop_default", vm_builtin_reflect_prop_default },
		{ "__reflect_new_instance", vm_builtin_reflect_new_instance },
		{ "__reflect_new_no_ctor",  vm_builtin_reflect_new_no_ctor },
	};
	sxu32 n;
	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){
		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);
	}
	return PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib1, sizeof(zReflectLib1)-1);
}
