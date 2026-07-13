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

static sxi32 ReflectEnforceStore(ph7_context *pCtx, sxu32 nIdx, ph7_value *pValue);

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
				ReflectMapAddBool(pCtx, pMeta, "typed", (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);
				if( SyStringLength(&pAttr->sTypeName) > 0 ){
					ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&pAttr->sTypeName),
						(int)SyStringLength(&pAttr->sTypeName));
				}else{
					ReflectMapAddNull(pCtx, pMeta, "typetext");
				}
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
	{
		/* Uninitialized typed static: same Error the VM raises on read */
		SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));
		if( pSlot ){
			VmClassAttr *pVmAttr = (VmClassAttr *)pSlot->pUserData;
			if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){
				ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
				return PH7_VmThrowException(pCtx, "Error",
					"Typed static property %z::$%z must not be accessed before initialization",
					&pDecl->sName, &pAttr->sName);
			}
		}
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
	{
		sxi32 rc = ReflectEnforceStore(pCtx, pAttr->nIdx, apArg[2]);
		if( rc != SXRET_OK ){
			return rc;
		}
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
 * Typed/readonly store enforcement for reflection writes. Like the VM's
 * store path, except an UNINITIALIZED readonly property may be written from
 * any scope (PHP lets ReflectionProperty::setValue initialize readonly): the
 * READONLY bit is masked off for the enforcement call so the set-scope check
 * is skipped, while an already-initialized readonly still gets PHP's
 * "Cannot modify readonly property" Error. Returns SXRET_OK/PH7_EXCEPTION/
 * PH7_ABORT; the value may be coerced in place.
 */
static sxi32 ReflectEnforceStore(ph7_context *pCtx, sxu32 nIdx, ph7_value *pValue)
{
	ph7_vm *pVm = pCtx->pVm;
	SyHashEntry *pSlot;
	VmClassAttr *pVmAttr;
	ph7_class_attr *pAttr;
	sxi32 iSaved, rc;
	pSlot = SyHashGet(&pVm->hTypedSlot, (const void *)&nIdx, sizeof(sxu32));
	if( pSlot == 0 ){
		return SXRET_OK; /* Untyped slot: plain store */
	}
	pVmAttr = (VmClassAttr *)pSlot->pUserData;
	pAttr = pVmAttr->pAttr;
	if( pAttr == 0 ){
		return SXRET_OK;
	}
	iSaved = pAttr->iFlags;
	if( (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) ){
		pAttr->iFlags &= ~PH7_CLASS_ATTR_READONLY;
	}
	rc = PH7_VmEnforcePropStore(pVm, nIdx, pValue);
	pAttr->iFlags = iSaved;
	return rc;
}
/*
 * mixed __reflect_prop_read(object $obj, string $name)
 * Instance property read, visibility ignored. Throws PHP's Error for an
 * uninitialized typed property.
 */
static int vm_builtin_reflect_prop_read(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis;
	SyHashEntry *pEntry;
	VmClassAttr *pVmAttr;
	ph7_value *pValue;
	const char *zName;
	int nLen;
	if( nArg < 2 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pThis = (ph7_class_instance *)apArg[0]->x.pOther;
	zName = ph7_value_to_string(apArg[1], &nLen);
	pEntry = nLen > 0 ? SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen) : 0;
	if( pEntry == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pVmAttr = (VmClassAttr *)pEntry->pUserData;
	if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){
		ph7_class *pDecl = pVmAttr->pAttr->pDeclClass ? pVmAttr->pAttr->pDeclClass : pThis->pClass;
		return PH7_VmThrowException(pCtx, "Error",
			"Typed property %z::$%z must not be accessed before initialization",
			&pDecl->sName, &pVmAttr->pAttr->sName);
	}
	pValue = PH7_ClassInstanceExtractAttrValue(pThis, pVmAttr);
	if( pValue ){
		ph7_result_value(pCtx, pValue);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/*
 * bool __reflect_prop_write(object $obj, string $name, mixed $value)
 * Instance property write, visibility ignored; typed and readonly rules
 * enforced (see ReflectEnforceStore).
 */
static int vm_builtin_reflect_prop_write(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis;
	SyHashEntry *pEntry;
	VmClassAttr *pVmAttr;
	ph7_value *pValue;
	const char *zName;
	sxi32 rc;
	int nLen;
	if( nArg < 3 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	pThis = (ph7_class_instance *)apArg[0]->x.pOther;
	zName = ph7_value_to_string(apArg[1], &nLen);
	pEntry = nLen > 0 ? SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen) : 0;
	if( pEntry == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	pVmAttr = (VmClassAttr *)pEntry->pUserData;
	rc = ReflectEnforceStore(pCtx, pVmAttr->nIdx, apArg[2]);
	if( rc != SXRET_OK ){
		return rc;
	}
	pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);
	if( pValue == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	PH7_MemObjStore(apArg[2], pValue);
	ph7_result_bool(pCtx, 1);
	return PH7_OK;
}
/*
 * int __reflect_prop_state(object|string $target, string $name)
 * Bitfield: 1 = exists (instance attr / static slot), 2 = initialized,
 * 4 = dynamic (instance-owned, not class-declared).
 */
static int vm_builtin_reflect_prop_state(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	int iState = 0;
	const char *zName;
	int nLen;
	if( nArg < 2 ){
		ph7_result_int(pCtx, 0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[1], &nLen);
	if( nLen < 1 ){
		ph7_result_int(pCtx, 0);
		return PH7_OK;
	}
	if( apArg[0]->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pThis = (ph7_class_instance *)apArg[0]->x.pOther;
		SyHashEntry *pEntry = SyHashGet(&pThis->hAttr, (const void *)zName, (sxu32)nLen);
		if( pEntry ){
			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
			iState |= 1;
			if( (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0 ){
				iState |= 2;
			}
			if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){
				iState |= 4;
			}
		}
	}else{
		ph7_class *pClass = ReflectResolveClass(pCtx->pVm, apArg[0]);
		ph7_class_attr *pAttr = pClass ? ReflectFetchAttr(pClass, apArg[1]) : 0;
		if( pAttr && (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){
			SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));
			iState |= 1 | 2;
			if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){
				iState &= ~2;
			}
		}
	}
	ph7_result_int(pCtx, iState);
	return PH7_OK;
}
/*
 * array __reflect_dyn_props(object $obj)
 * Names of the instance's runtime-added (dynamic) properties, in creation
 * order (the instance attr table inserts dynamics at the tail).
 */
static int vm_builtin_reflect_dyn_props(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis;
	SyHashEntry *pEntry;
	ph7_value *pList;
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0
	 || (pList = ph7_context_new_array(pCtx)) == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pThis = (ph7_class_instance *)apArg[0]->x.pOther;
	SyHashResetLoopCursor(&pThis->hAttr);
	while( (pEntry = SyHashGetNextEntry(&pThis->hAttr)) != 0 ){
		VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
		if( pVmAttr->pAttr && (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) ){
			ph7_value *pName = ph7_context_new_scalar(pCtx);
			if( pName == 0 ){ break; }
			ph7_value_string(pName, SyStringData(&pVmAttr->pAttr->sName),
				(int)SyStringLength(&pVmAttr->pAttr->sName));
			ph7_array_add_elem(pList, 0, pName);
		}
	}
	ph7_result_value(pCtx, pList);
	return PH7_OK;
}
/* Hand an EXISTING instance to the caller: takes an extra reference
 * (unlike ReflectResultObject, which transfers a fresh instance's one). */
static int ReflectResultExistingObject(ph7_context *pCtx, ph7_class_instance *pObj)
{
	if( pObj == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjRelease(pCtx->pRet);
	pObj->iRef++;
	pCtx->pRet->x.pOther = pObj;
	MemObjSetType(pCtx->pRet, MEMOBJ_OBJ);
	return PH7_OK;
}
/* pVal is a Closure instance? Return it, else NULL. */
static ph7_class_instance * ReflectValueClosure(ph7_vm *pVm, ph7_value *pVal)
{
	ph7_class_instance *pThis;
	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 || pVal->x.pOther == 0 || pVm->pClosureClass == 0 ){
		return 0;
	}
	pThis = (ph7_class_instance *)pVal->x.pOther;
	return (pThis->pClass == pVm->pClosureClass) ? pThis : 0;
}
/*
 * Resolve a reflection callable target into its compiled function.
 *   - pMethodArg a non-empty string  -> method mode: pTarget is a class name
 *     or object; outputs *ppClass and *ppMeth.
 *   - pTarget a Closure              -> unwrap $__fn into hFunction; *ppClosure.
 *   - pTarget a string               -> hFunction (user) or hHostFunction
 *     (*ppHost set, returns NULL).
 * Returns the ph7_vm_func, or NULL (host function or unresolvable).
 */
static ph7_vm_func * ReflectResolveCallable(ph7_context *pCtx, ph7_value *pTarget,
	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,
	ph7_user_func **ppHost, ph7_class_instance **ppClosure)
{
	ph7_vm *pVm = pCtx->pVm;
	SyHashEntry *pEntry;
	if( ppClass ){ *ppClass = 0; }
	if( ppMeth ){ *ppMeth = 0; }
	if( ppHost ){ *ppHost = 0; }
	if( ppClosure ){ *ppClosure = 0; }
	if( pMethodArg && (pMethodArg->iFlags & MEMOBJ_STRING) && SyBlobLength(&pMethodArg->sBlob) > 0 ){
		ph7_class *pClass = ReflectResolveClass(pVm, pTarget);
		ph7_class_method *pMeth;
		if( pClass == 0 ){
			return 0;
		}
		pMeth = PH7_ClassExtractMethod(pClass, (const char *)SyBlobData(&pMethodArg->sBlob),
			SyBlobLength(&pMethodArg->sBlob));
		if( pMeth == 0 ){
			return 0;
		}
		if( ppClass ){ *ppClass = pClass; }
		if( ppMeth ){ *ppMeth = pMeth; }
		return &pMeth->sFunc;
	}
	{
		ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);
		if( pClo ){
			SyString sAttr;
			ph7_value *pFn;
			SyStringInitFromBuf(&sAttr, "__fn", 4);
			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);
			if( pFn == 0 || (pFn->iFlags & MEMOBJ_STRING) == 0 || SyBlobLength(&pFn->sBlob) < 1 ){
				return 0;
			}
			pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));
			if( pEntry == 0 ){
				/* A Closure over a host function (Closure::fromCallable('strlen')) */
				pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));
				if( pEntry && ppHost ){
					*ppHost = (ph7_user_func *)pEntry->pUserData;
					if( ppClosure ){ *ppClosure = pClo; }
				}
				return 0;
			}
			if( ppClosure ){ *ppClosure = pClo; }
			return (ph7_vm_func *)pEntry->pUserData;
		}
	}
	if( pTarget->iFlags & MEMOBJ_STRING ){
		if( SyBlobLength(&pTarget->sBlob) < 1 ){
			return 0;
		}
		pEntry = SyHashGet(&pVm->hFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));
		if( pEntry ){
			return (ph7_vm_func *)pEntry->pUserData;
		}
		pEntry = SyHashGet(&pVm->hHostFunction, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob));
		if( pEntry && ppHost ){
			*ppHost = (ph7_user_func *)pEntry->pUserData;
		}
	}
	return 0;
}
/* Emit the shared descriptor fields of a compiled function. */
static void ReflectFillFuncCommon(ph7_context *pCtx, ph7_value *pInfo, ph7_vm_func *pFunc)
{
	ph7_vm_func_arg *aArg;
	ph7_value *pParams, *pStatics;
	int bVariadic = 0;
	int bAnon;
	sxu32 n;
	/* A capture-free `function(){}` compiles without the CLOSURE flag but
	 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */
	bAnon = (pFunc->iFlags & VM_FUNC_CLOSURE) != 0;
	if( !bAnon && SyStringLength(&pFunc->sName) > 9
	 && (SyMemcmp(SyStringData(&pFunc->sName), "[lambda_", 8) == 0
	  || SyMemcmp(SyStringData(&pFunc->sName), "[closure_", 9) == 0) ){
		bAnon = 1;
	}
	ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));
	ReflectMapAddBool(pCtx, pInfo, "internal", (pFunc->iFlags & VM_FUNC_INTERNAL) != 0);
	ReflectMapAddBool(pCtx, pInfo, "closure", bAnon);
	ReflectMapAddBool(pCtx, pInfo, "byref", (pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);
	ReflectMapAddBool(pCtx, pInfo, "generator", (pFunc->iFlags & VM_FUNC_GENERATOR) != 0);
	ReflectMapAddBool(pCtx, pInfo, "strict", pFunc->bStrictTypes != 0);
	if( SyStringLength(&pFunc->sFile) > 0 ){
		ReflectMapAddStr(pCtx, pInfo, "file", SyStringData(&pFunc->sFile), (int)SyStringLength(&pFunc->sFile));
	}else{
		ReflectMapAddBool(pCtx, pInfo, "file", 0);
	}
	ReflectMapAddInt(pCtx, pInfo, "line", (sxi64)pFunc->nLine);
	ReflectMapAddInt(pCtx, pInfo, "endline", (sxi64)pFunc->nEndLine);
	if( SyStringLength(&pFunc->sReturnTypeName) > 0 ){
		ReflectMapAddStr(pCtx, pInfo, "rettext", SyStringData(&pFunc->sReturnTypeName),
			(int)SyStringLength(&pFunc->sReturnTypeName));
	}else if( pFunc->nReturnType == MEMOBJ_VOID ){
		/* The type-text renderer omits void/never atoms (compile.c notes the
		 * root fix belongs there); name them here for getReturnType(). */
		ReflectMapAddStr(pCtx, pInfo, "rettext", "void", sizeof("void")-1);
	}else if( pFunc->nReturnType == MEMOBJ_NEVER ){
		ReflectMapAddStr(pCtx, pInfo, "rettext", "never", sizeof("never")-1);
	}else{
		ReflectMapAddNull(pCtx, pInfo, "rettext");
	}
	ReflectMapAddBool(pCtx, pInfo, "retnullable", (pFunc->iFlags & VM_FUNC_RETURN_NULLABLE) != 0);
	/* Parameters */
	pParams = ph7_context_new_array(pCtx);
	aArg = (ph7_vm_func_arg *)SySetBasePtr(&pFunc->aArgs);
	for( n = 0 ; pParams && n < SySetUsed(&pFunc->aArgs) ; n++ ){
		ph7_value *pMeta = ph7_context_new_array(pCtx);
		if( pMeta == 0 ){ break; }
		ReflectMapAddStr(pCtx, pMeta, "name", SyStringData(&aArg[n].sName), (int)SyStringLength(&aArg[n].sName));
		ReflectMapAddInt(pCtx, pMeta, "pos", (sxi64)n);
		ReflectMapAddBool(pCtx, pMeta, "byref", (aArg[n].iFlags & VM_FUNC_ARG_BY_REF) != 0);
		ReflectMapAddBool(pCtx, pMeta, "variadic", (aArg[n].iFlags & VM_FUNC_ARG_VARIADIC) != 0);
		/* The compiler never sets ARG_HAS_DEF; a default = compiled bytecode
		 * (same test the OP_CALL default-value path uses). */
		ReflectMapAddBool(pCtx, pMeta, "hasdef", SySetUsed(&aArg[n].aByteCode) > 0);
		ReflectMapAddBool(pCtx, pMeta, "nullable", (aArg[n].iFlags & VM_FUNC_ARG_NULLABLE) != 0);
		ReflectMapAddBool(pCtx, pMeta, "promoted", (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) != 0);
		if( SyStringLength(&aArg[n].sTypeName) > 0 ){
			ReflectMapAddStr(pCtx, pMeta, "typetext", SyStringData(&aArg[n].sTypeName),
				(int)SyStringLength(&aArg[n].sTypeName));
		}else{
			ReflectMapAddNull(pCtx, pMeta, "typetext");
		}
		ph7_array_add_elem(pParams, 0, pMeta);
		if( aArg[n].iFlags & VM_FUNC_ARG_VARIADIC ){
			bVariadic = 1;
		}
	}
	if( pParams ){
		ph7_array_add_strkey_elem(pInfo, "params", pParams);
	}
	ReflectMapAddBool(pCtx, pInfo, "variadic", bVariadic);
	/* Static variables: current value when the slot was initialized (first
	 * call), otherwise the evaluated default — PHP's getStaticVariables
	 * initializes on demand and reports the same values. */
	pStatics = ph7_context_new_array(pCtx);
	if( pStatics ){
		ph7_vm_func_static_var *aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&pFunc->aStatic);
		for( n = 0 ; n < SySetUsed(&pFunc->aStatic) ; n++ ){
			ph7_value *pVal = 0;
			ph7_value sScratch;
			int bScratch = 0;
			if( aStatic[n].nIdx != SXU32_HIGH ){
				pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);
			}
			if( pVal == 0 ){
				PH7_MemObjInit(pCtx->pVm, &sScratch);
				if( SySetUsed(&aStatic[n].aByteCode) > 0 ){
					VmLocalExec(pCtx->pVm, &aStatic[n].aByteCode, &sScratch, FALSE);
				}
				pVal = &sScratch;
				bScratch = 1;
			}
			ReflectMapAddDyn(pCtx, pStatics, &aStatic[n].sName, pVal);
			if( bScratch ){
				PH7_MemObjRelease(&sScratch);
			}
		}
		ph7_array_add_strkey_elem(pInfo, "statics", pStatics);
	}
}
/*
 * array|null __reflect_func_info(string|Closure $target [, string $method])
 * Function/method/closure descriptor for the PHP layer.
 */
static int vm_builtin_reflect_func_info(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm_func *pFunc;
	ph7_class *pClass = 0;
	ph7_class_method *pMeth = 0;
	ph7_user_func *pHost = 0;
	ph7_class_instance *pClosure = 0;
	ph7_value *pInfo;
	if( nArg < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pFunc = ReflectResolveCallable(pCtx, apArg[0], nArg > 1 ? apArg[1] : 0,
		&pClass, &pMeth, &pHost, &pClosure);
	if( pFunc == 0 && pHost == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pInfo = ph7_context_new_array(pCtx);
	if( pInfo == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( pFunc == 0 ){
		/* Host (C builtin) function: no parameter metadata beyond arity */
		ph7_value *pParams = ph7_context_new_array(pCtx);
		ReflectMapAddStr(pCtx, pInfo, "name", SyStringData(&pHost->sName), (int)SyStringLength(&pHost->sName));
		ReflectMapAddBool(pCtx, pInfo, "internal", 1);
		ReflectMapAddBool(pCtx, pInfo, "closure", 0);
		ReflectMapAddBool(pCtx, pInfo, "byref", 0);
		ReflectMapAddBool(pCtx, pInfo, "generator", 0);
		ReflectMapAddBool(pCtx, pInfo, "strict", 0);
		ReflectMapAddBool(pCtx, pInfo, "file", 0);
		ReflectMapAddInt(pCtx, pInfo, "line", 0);
		ReflectMapAddInt(pCtx, pInfo, "endline", 0);
		ReflectMapAddNull(pCtx, pInfo, "rettext");
		ReflectMapAddBool(pCtx, pInfo, "retnullable", 0);
		if( pParams ){
			ph7_array_add_strkey_elem(pInfo, "params", pParams);
		}
		ReflectMapAddBool(pCtx, pInfo, "variadic", 0);
		ReflectMapAddInt(pCtx, pInfo, "minarg", (sxi64)pHost->nMinArg);
		ph7_result_value(pCtx, pInfo);
		return PH7_OK;
	}
	ReflectFillFuncCommon(pCtx, pInfo, pFunc);
	ReflectMapAddInt(pCtx, pInfo, "minarg", -1);
	if( pMeth && pClass ){
		ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);
		ReflectMapAddStr(pCtx, pInfo, "class", SyStringData(&pClass->sName), (int)SyStringLength(&pClass->sName));
		ReflectMapAddStr(pCtx, pInfo, "decl", SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));
		ReflectMapAddInt(pCtx, pInfo, "vis", (sxi64)pMeth->iProtection);
		ReflectMapAddBool(pCtx, pInfo, "mstatic", (pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);
		ReflectMapAddBool(pCtx, pInfo, "abstract", (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0);
		ReflectMapAddBool(pCtx, pInfo, "final", (pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0);
	}
	if( pClosure ){
		SyString sAttr;
		ph7_value *pAttr;
		ph7_value *pUsed;
		SyStringInitFromBuf(&sAttr, "__this", 6);
		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);
		if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){
			ph7_value *pKey = ph7_context_new_scalar(pCtx);
			if( pKey ){
				ph7_value_string(pKey, "this", 4);
				ph7_array_add_elem(pInfo, pKey, pAttr);
			}
		}else{
			ReflectMapAddNull(pCtx, pInfo, "this");
		}
		SyStringInitFromBuf(&sAttr, "__scope", 7);
		pAttr = PH7_ClassInstanceFetchAttr(pClosure, &sAttr);
		if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){
			ReflectMapAddStr(pCtx, pInfo, "scope", (const char *)SyBlobData(&pAttr->sBlob),
				(int)SyBlobLength(&pAttr->sBlob));
		}else{
			ReflectMapAddNull(pCtx, pInfo, "scope");
		}
		/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */
		pUsed = ph7_context_new_array(pCtx);
		if( pUsed ){
			ph7_vm_func_closure_env *aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&pFunc->aClosureEnv);
			sxu32 n;
			for( n = 0 ; n < SySetUsed(&pFunc->aClosureEnv) ; n++ ){
				if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){
					continue;
				}
				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1
				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){
					continue;
				}
				ReflectMapAddDyn(pCtx, pUsed, &aEnv[n].sName, &aEnv[n].sValue);
			}
			ph7_array_add_strkey_elem(pInfo, "used", pUsed);
		}
	}
	ph7_result_value(pCtx, pInfo);
	return PH7_OK;
}
/*
 * mixed __reflect_param_default(string|Closure $target, ?string $method, int $idx)
 * Evaluate a parameter's compiled default expression.
 */
static int vm_builtin_reflect_param_default(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm_func *pFunc;
	ph7_vm_func_arg *pArg;
	ph7_value sValue;
	sxu32 nIdx;
	if( nArg < 3 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);
	nIdx = (sxu32)ph7_value_to_int(apArg[2]);
	if( pFunc == 0 || (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0
	 || SySetUsed(&pArg->aByteCode) < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjInit(pCtx->pVm, &sValue);
	VmLocalExec(pCtx->pVm, &pArg->aByteCode, &sValue, FALSE);
	ph7_result_value(pCtx, &sValue);
	PH7_MemObjRelease(&sValue);
	return PH7_OK;
}
/*
 * string|null __reflect_param_defconst(string|Closure $target, ?string $method, int $idx)
 * When a parameter's default is a plain global-constant reference, its
 * source name; null otherwise. A constant default compiles to exactly
 * [ OP_LOADC (EXPAND) , OP_DONE ] with the name in the literal table.
 */
static int vm_builtin_reflect_param_defconst(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm_func *pFunc;
	ph7_vm_func_arg *pArg;
	VmInstr *aInstr;
	ph7_value *pLit;
	sxu32 nIdx;
	if( nArg < 3 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], 0, 0, 0, 0);
	nIdx = (sxu32)ph7_value_to_int(apArg[2]);
	if( pFunc == 0 || (pArg = (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, nIdx)) == 0
	 || SySetUsed(&pArg->aByteCode) != 2 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	aInstr = (VmInstr *)SySetBasePtr(&pArg->aByteCode);
	if( aInstr[0].iOp != PH7_OP_LOADC || (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0
	 || aInstr[1].iOp != PH7_OP_DONE ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);
	if( pLit == 0 || SyBlobLength(&pLit->sBlob) < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	ph7_result_string(pCtx, (const char *)SyBlobData(&pLit->sBlob), (int)SyBlobLength(&pLit->sBlob));
	return PH7_OK;
}
/*
 * mixed __reflect_invoke(mixed $target, ?string $method, ?object $this, array $args)
 * Visibility-bypassing invocation (methods dispatch by VM name; functions
 * and closures ride PH7_VmCallUserFunction like call_user_func_array).
 */
static int vm_builtin_reflect_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_value sResult;
	SySet aCallArg;
	sxi32 rc;
	if( nArg < 4 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjInit(pVm, &sResult);
	sResult.nIdx = SXU32_HIGH;
	SySetInit(&aCallArg, &pVm->sAllocator, sizeof(ph7_value *));
	ReflectCollectArgs(pCtx, apArg[3], &aCallArg);
	if( (apArg[1]->iFlags & MEMOBJ_STRING) && SyBlobLength(&apArg[1]->sBlob) > 0 ){
		ph7_class *pClass = 0;
		ph7_class_method *pMeth = 0;
		ph7_class_instance *pThis = 0;
		ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, 0);
		if( pMeth == 0 ){
			SySetRelease(&aCallArg);
			PH7_MemObjRelease(&sResult);
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		if( apArg[2]->iFlags & MEMOBJ_OBJ ){
			pThis = (ph7_class_instance *)apArg[2]->x.pOther;
		}
		/* Reflection ignores method visibility (PHP 8.1+); the flag is
		 * consumed by the first OP_CALL, i.e. this synthetic one. */
		pVm->bReflectBypass = 1;
		rc = PH7_VmCallClassMethod(pVm, pThis, pMeth, &sResult,
			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg));
		pVm->bReflectBypass = 0;
	}else{
		rc = PH7_VmCallUserFunction(pVm, apArg[0],
			(int)SySetUsed(&aCallArg), (ph7_value **)SySetBasePtr(&aCallArg), &sResult);
	}
	SySetRelease(&aCallArg);
	if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){
		PH7_MemObjRelease(&sResult);
		return rc;
	}
	ph7_result_value(pCtx, &sResult);
	PH7_MemObjRelease(&sResult);
	return PH7_OK;
}
/*
 * Closure __reflect_closure(mixed $target, ?string $method, ?object $this)
 * Mint a Closure for a function or method, bound and scoped like the
 * first-class-callable path.
 */
static int vm_builtin_reflect_closure(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = 0;
	ph7_class_method *pMeth = 0;
	ph7_class_instance *pClosure = 0;
	ph7_vm_func *pFunc;
	if( nArg < 3 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pFunc = ReflectResolveCallable(pCtx, apArg[0], apArg[1], &pClass, &pMeth, 0, &pClosure);
	if( pClosure ){
		/* Already a Closure: hand the same instance back */
		return ReflectResultExistingObject(pCtx, pClosure);
	}
	if( pMeth && pClass ){
		ph7_class_instance *pThis = 0;
		if( apArg[2]->iFlags & MEMOBJ_OBJ ){
			pThis = (ph7_class_instance *)apArg[2]->x.pOther;
		}
		return ReflectResultObject(pCtx,
			PH7_VmNewClosure(pVm, &pMeth->sFunc.sName, pThis, &pClass->sName));
	}
	if( pFunc ){
		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &pFunc->sName, 0, 0));
	}
	/* Host function by name */
	if( apArg[0]->iFlags & MEMOBJ_STRING ){
		SyString sName;
		SyStringInitFromBuf(&sName, (const char *)SyBlobData(&apArg[0]->sBlob), SyBlobLength(&apArg[0]->sBlob));
		return ReflectResultObject(pCtx, PH7_VmNewClosure(pVm, &sName, 0, 0));
	}
	ph7_result_null(pCtx);
	return PH7_OK;
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
" protected $__obj = null;"
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
" public function hasProperty($name){"
"  $i = $this->__rinfo();"
"  if(isset($i['props'][$name])){ return true; }"
"  if($this->__obj !== null){ return (__reflect_prop_state($this->__obj, $name) & 1) !== 0; }"
"  return false;"
" }"
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
" public function getProperty($name){"
"  $i = $this->__rinfo();"
"  if(isset($i['props'][$name])){"
"   return new ReflectionProperty($this->name, $name);"
"  }"
"  if($this->__obj !== null && (__reflect_prop_state($this->__obj, $name) & 1)){"
"   return new ReflectionProperty($this->__obj, $name);"
"  }"
"  throw new ReflectionException('Property '.$this->name.'::$'.$name.' does not exist');"
" }"
" public function getProperties($filter = null){"
"  $i = $this->__rinfo();"
"  $out = array();"
"  foreach($i['props'] as $k => $p){"
"   if($filter !== null){"
"    $m = ($p['vis'] === 1 ? 1 : ($p['vis'] === 2 ? 2 : 4));"
"    if($p['static']){ $m |= 16; }"
"    if($p['readonly']){ $m |= 128; }"
"    if(($m & $filter) === 0){ continue; }"
"   }"
"   $out[] = new ReflectionProperty($this->name, $k);"
"  }"
"  if($this->__obj !== null){"
"   foreach(__reflect_dyn_props($this->__obj) as $k){"
"    if(isset($i['props'][$k])){ continue; }"
"    if($filter !== null && ($filter & 1) === 0){ continue; }"
"    $out[] = new ReflectionProperty($this->__obj, $k);"
"   }"
"  }"
"  return $out;"
" }"
" public function getMethod($name){"
"  $i = $this->__rinfo();"
"  $found = null;"
"  if(isset($i['methods'][$name])){"
"   $found = $name;"
"  }else{"
"   $l = strtolower($name);"
"   foreach($i['methods'] as $k => $m){ if(strtolower($k) === $l){ $found = $k; break; } }"
"  }"
"  if($found === null){"
"   throw new ReflectionException('Method '.$this->name.'::'.$name.'() does not exist');"
"  }"
"  return new ReflectionMethod($this->name, $found);"
" }"
" public function getMethods($filter = null){"
"  $i = $this->__rinfo();"
"  $out = array();"
"  foreach($i['methods'] as $k => $m){"
"   if($filter !== null){"
"    $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"
"    if($m['static']){ $mod |= 16; }"
"    if($m['abstract']){ $mod |= 64; }"
"    if($m['final']){ $mod |= 32; }"
"    if(($mod & $filter) === 0){ continue; }"
"   }"
"   $out[] = new ReflectionMethod($this->name, $k);"
"  }"
"  return $out;"
" }"
" public function getConstructor(){"
"  $i = $this->__rinfo();"
"  if(isset($i['methods']['__construct'])){"
"   return new ReflectionMethod($this->name, '__construct');"
"  }"
"  foreach($i['methods'] as $k => $m){"
"   if(strtolower($k) === '__construct'){ return new ReflectionMethod($this->name, $k); }"
"  }"
"  if($i['ctorvis'] !== 0 && isset($i['methods'][$this->name])){"
"   return new ReflectionMethod($this->name, $this->name);"
"  }"
"  return null;"
" }"
" public function getReflectionConstant($name){"
"  $i = $this->__rinfo();"
"  if(!isset($i['consts'][$name])){ return false; }"
"  return new ReflectionClassConstant($this->name, $name);"
" }"
" public function getReflectionConstants($filter = null){"
"  $i = $this->__rinfo();"
"  $out = array();"
"  foreach($i['consts'] as $k => $c){"
"   if($filter !== null){"
"    $m = ($c['vis'] === 1 ? 1 : ($c['vis'] === 2 ? 2 : 4));"
"    if($c['final']){ $m |= 32; }"
"    if(($m & $filter) === 0){ continue; }"
"   }"
"   $out[] = new ReflectionClassConstant($this->name, $k);"
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
"  $this->__obj = $object;"
" }"
"}"
;
/*
 * Chunk 2: ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,
 * ReflectionParameter.
 */
static const char zReflectLib2[] =
"abstract class ReflectionFunctionAbstract implements Reflector {"
" public $name;"
" protected $__cl = null;"
" protected function __rfinfo(){"
"  if($this->__cl !== null){ return __reflect_func_info($this->__cl); }"
"  return __reflect_func_info($this->name);"
" }"
" protected function __rftarget(){ return $this->__cl !== null ? $this->__cl : $this->name; }"
" protected function __rpspec(){ return $this->__rftarget(); }"
" public function getName(){ return $this->name; }"
" public function inNamespace(){ return strrpos($this->name,'\\\\') !== false; }"
" public function getNamespaceName(){"
"  $p = strrpos($this->name,'\\\\');"
"  if($p === false){ return ''; }"
"  return substr($this->name,0,$p);"
" }"
" public function getShortName(){"
"  $p = strrpos($this->name,'\\\\');"
"  if($p === false){ return $this->name; }"
"  return substr($this->name,$p+1);"
" }"
" public function isClosure(){ $i = $this->__rfinfo(); return $i['closure']; }"
" public function isGenerator(){ $i = $this->__rfinfo(); return $i['generator']; }"
" public function isVariadic(){ $i = $this->__rfinfo(); return $i['variadic']; }"
" public function returnsReference(){ $i = $this->__rfinfo(); return $i['byref']; }"
" public function isInternal(){ $i = $this->__rfinfo(); return $i['internal']; }"
" public function isUserDefined(){ return !$this->isInternal(); }"
" public function isDeprecated(){ return false; }"
" public function isStatic(){ return false; }"
" public function getFileName(){ $i = $this->__rfinfo(); return $i['file']; }"
" public function getStartLine(){"
"  $i = $this->__rfinfo();"
"  if($i['internal']){ return false; }"
"  return $i['line'];"
" }"
" public function getEndLine(){"
"  $i = $this->__rfinfo();"
"  if($i['internal']){ return false; }"
"  return $i['endline'];"
" }"
" public function getDocComment(){ return false; }"
" public function hasReturnType(){ $i = $this->__rfinfo(); return $i['rettext'] !== null; }"
" public function getReturnType(){ $i = $this->__rfinfo(); return __reflect_make_type($i['rettext']); }"
" public function hasTentativeReturnType(){ return false; }"
" public function getTentativeReturnType(){ return null; }"
" public function getNumberOfParameters(){"
"  $i = $this->__rfinfo();"
"  if($i['minarg'] >= 0){ return $i['minarg']; }"
"  return count($i['params']);"
" }"
" public function getNumberOfRequiredParameters(){"
"  $i = $this->__rfinfo();"
"  if($i['minarg'] >= 0){ return $i['minarg']; }"
"  $req = 0;"
"  $n = count($i['params']);"
"  for($k = $n - 1; $k >= 0; $k--){"
"   $p = $i['params'][$k];"
"   if(!$p['variadic'] && !$p['hasdef']){ $req = $k + 1; break; }"
"  }"
"  return $req;"
" }"
" public function getParameters(){"
"  $i = $this->__rfinfo();"
"  $out = array();"
"  $spec = $this->__rpspec();"
"  foreach($i['params'] as $p){"
"   $out[] = new ReflectionParameter($spec, $p['pos']);"
"  }"
"  return $out;"
" }"
" public function getStaticVariables(){ $i = $this->__rfinfo(); return $i['statics']; }"
" public function getClosureThis(){"
"  $i = $this->__rfinfo();"
"  return isset($i['this']) ? $i['this'] : null;"
" }"
" public function getClosureScopeClass(){"
"  $i = $this->__rfinfo();"
"  if(isset($i['scope'])){ return new ReflectionClass($i['scope']); }"
"  if(isset($i['this'])){ return new ReflectionClass(get_class($i['this'])); }"
"  return null;"
" }"
" public function getClosureCalledClass(){ return $this->getClosureScopeClass(); }"
" public function getClosureUsedVariables(){"
"  $i = $this->__rfinfo();"
"  return isset($i['used']) ? $i['used'] : array();"
" }"
" public function getExtensionName(){ $i = $this->__rfinfo(); return $i['internal'] ? 'Core' : false; }"
" public function getAttributes($name = null, $flags = 0){ return array(); }"
" public function __toString(){"
"  return 'Function [ function '.$this->name.' ] {'.\"\\n\".'}'.\"\\n\";"
" }"
"}"
"class ReflectionFunction extends ReflectionFunctionAbstract {"
" const IS_DEPRECATED = 2048;"
" public function __construct($function){"
"  if($function instanceof Closure){"
"   $this->__cl = $function;"
"   $i = $this->__rfinfo();"
"   if($i['closure']){"
"    $f = $i['file'] === false ? '' : $i['file'];"
"    $this->name = '{closure:'.$f.':'.$i['line'].'}';"
"   }else{"
"    $this->name = $i['name'];"
"   }"
"   return;"
"  }"
"  if(!is_string($function)){"
"   throw new TypeError('ReflectionFunction::__construct(): Argument #1 ($function) must be of type Closure|string, '.get_debug_type($function).' given');"
"  }"
"  $i = __reflect_func_info($function);"
"  if($i === null){"
"   throw new ReflectionException('Function '.$function.'() does not exist');"
"  }"
"  $this->name = $i['name'];"
" }"
" public function invoke(...$args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"
" public function invokeArgs(array $args){ return __reflect_invoke($this->__rftarget(), null, null, $args); }"
" public function getClosure(){"
"  if($this->__cl !== null){ return $this->__cl; }"
"  return __reflect_closure($this->name, null, null);"
" }"
" public function isAnonymous(){ $i = $this->__rfinfo(); return $i['closure']; }"
" public function isDisabled(){ return false; }"
"}"
"class ReflectionMethod extends ReflectionFunctionAbstract {"
" const IS_PUBLIC = 1;"
" const IS_PROTECTED = 2;"
" const IS_PRIVATE = 4;"
" const IS_STATIC = 16;"
" const IS_FINAL = 32;"
" const IS_ABSTRACT = 64;"
" public $class;"
" public function __construct($objectOrMethod, $method = null){"
"  if($method === null){"
"   if(!is_string($objectOrMethod) || strpos($objectOrMethod,'::') === false){"
"    throw new TypeError('ReflectionMethod::__construct(): Argument #1 ($objectOrMethod) must be of type object|string, '.get_debug_type($objectOrMethod).' given');"
"   }"
"   $p = strpos($objectOrMethod,'::');"
"   $method = substr($objectOrMethod,$p+2);"
"   $objectOrMethod = substr($objectOrMethod,0,$p);"
"  }"
"  $ci = __reflect_class_info($objectOrMethod);"
"  if($ci === null){"
"   throw new ReflectionException('Class \"'.$objectOrMethod.'\" does not exist');"
"  }"
"  $this->class = $ci['name'];"
"  $found = null;"
"  if(isset($ci['methods'][$method])){"
"   $found = $method;"
"  }else{"
"   $l = strtolower($method);"
"   foreach($ci['methods'] as $k => $m){"
"    if(strtolower($k) === $l){ $found = $k; break; }"
"   }"
"  }"
"  if($found === null){"
"   throw new ReflectionException('Method '.$this->class.'::'.$method.'() does not exist');"
"  }"
"  $this->name = $found;"
" }"
" public static function createFromMethodName($name){"
"  return new ReflectionMethod($name);"
" }"
" protected function __rfinfo(){ return __reflect_func_info($this->class, $this->name); }"
" protected function __rpspec(){ return array($this->class, $this->name); }"
" public function getDeclaringClass(){"
"  $i = $this->__rfinfo();"
"  return new ReflectionClass($i['decl']);"
" }"
" public function getModifiers(){"
"  $i = $this->__rfinfo();"
"  $m = ($i['vis'] === 1 ? 1 : ($i['vis'] === 2 ? 2 : 4));"
"  if($i['mstatic']){ $m |= 16; }"
"  if($i['abstract']){ $m |= 64; }"
"  if($i['final']){ $m |= 32; }"
"  return $m;"
" }"
" public function isPublic(){ $i = $this->__rfinfo(); return $i['vis'] === 1; }"
" public function isProtected(){ $i = $this->__rfinfo(); return $i['vis'] === 2; }"
" public function isPrivate(){ $i = $this->__rfinfo(); return $i['vis'] === 3; }"
" public function isStatic(){ $i = $this->__rfinfo(); return $i['mstatic']; }"
" public function isAbstract(){ $i = $this->__rfinfo(); return $i['abstract']; }"
" public function isFinal(){ $i = $this->__rfinfo(); return $i['final']; }"
" public function isConstructor(){ return strtolower($this->name) === '__construct'; }"
" public function isDestructor(){ return strtolower($this->name) === '__destruct'; }"
" public function invoke($object = null, ...$args){ return $this->__rinvoke($object, $args); }"
" public function invokeArgs($object, array $args){ return $this->__rinvoke($object, $args); }"
" protected function __rinvoke($object, $args){"
"  $i = $this->__rfinfo();"
"  if(!$i['mstatic']){"
"   if(!is_object($object)){"
"    throw new ReflectionException('Trying to invoke non static method '.$this->class.'::'.$this->name.'() without an object');"
"   }"
"   if(!is_a($object, $i['decl'])){"
"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"
"   }"
"  }else{"
"   $object = null;"
"  }"
"  return __reflect_invoke($this->class, $this->name, $object, $args);"
" }"
" public function getClosure($object = null){"
"  $i = $this->__rfinfo();"
"  if(!$i['mstatic']){"
"   if($object === null){"
"    throw new ValueError('ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods');"
"   }"
"   if(!is_a($object, $i['decl'])){"
"    throw new ReflectionException('Given object is not an instance of the class this method was declared in');"
"   }"
"  }else{"
"   $object = null;"
"  }"
"  return __reflect_closure($this->class, $this->name, $object);"
" }"
" public function setAccessible($accessible){ }"
" public function hasPrototype(){ return $this->__rproto() !== null; }"
" public function getPrototype(){"
"  $p = $this->__rproto();"
"  if($p === null){"
"   throw new ReflectionException('Method '.$this->class.'::'.$this->name.' does not have a prototype');"
"  }"
"  return new ReflectionMethod($p, $this->name);"
" }"
" protected function __rproto(){"
"  $ci = __reflect_class_info($this->class);"
"  $l = strtolower($this->name);"
"  $p = $ci['parent'];"
"  while($p !== null){"
"   $pi = __reflect_class_info($p);"
"   foreach($pi['methods'] as $k => $m){"
"    if(strtolower($k) === $l && $m['vis'] !== 3){ return $m['decl']; }"
"   }"
"   $p = $pi['parent'];"
"  }"
"  foreach($ci['interfaces'] as $if){"
"   $ii = __reflect_class_info($if);"
"   foreach($ii['methods'] as $k => $m){"
"    if(strtolower($k) === $l){ return $ii['name']; }"
"   }"
"  }"
"  return null;"
" }"
" public function __toString(){"
"  return 'Method [ public method '.$this->name.' ] {'.\"\\n\".'}'.\"\\n\";"
" }"
"}"
"class ReflectionParameter implements Reflector {"
" public $name;"
" protected $__t;"
" protected $__m = null;"
" protected $__p = 0;"
" public function __construct($function, $param){"
"  $m = null;"
"  $t = $function;"
"  if(is_array($function)){"
"   $t = $function[0];"
"   $m = $function[1];"
"   if(is_object($t)){ $t = get_class($t); }"
"  }else if(is_string($function) && strpos($function,'::') !== false){"
"   $p = strpos($function,'::');"
"   $m = substr($function,$p+2);"
"   $t = substr($function,0,$p);"
"  }"
"  if($m !== null){"
"   $rm = new ReflectionMethod($t, $m);"
"   $t = $rm->class;"
"   $m = $rm->name;"
"   $i = __reflect_func_info($t, $m);"
"  }else if($function instanceof Closure){"
"   $t = $function;"
"   $i = __reflect_func_info($function);"
"  }else{"
"   $i = __reflect_func_info($t);"
"   if($i === null){"
"    throw new ReflectionException('Function '.$t.'() does not exist');"
"   }"
"  }"
"  $found = null;"
"  if(is_int($param)){"
"   if(isset($i['params'][$param])){ $found = $i['params'][$param]; }"
"   if($found === null){"
"    throw new ReflectionException('The parameter specified by its offset could not be found');"
"   }"
"  }else{"
"   foreach($i['params'] as $pp){"
"    if($pp['name'] === $param){ $found = $pp; break; }"
"   }"
"   if($found === null){"
"    throw new ReflectionException('The parameter specified by its name could not be found');"
"   }"
"  }"
"  $this->name = $found['name'];"
"  $this->__t = $t;"
"  $this->__m = $m;"
"  $this->__p = $found['pos'];"
" }"
" protected function __rffull(){"
"  if($this->__m !== null){ return __reflect_func_info($this->__t, $this->__m); }"
"  return __reflect_func_info($this->__t);"
" }"
" protected function __rpinfo(){"
"  $i = $this->__rffull();"
"  return $i['params'][$this->__p];"
" }"
" public function getName(){ return $this->name; }"
" public function getPosition(){ return $this->__p; }"
" public function isPassedByReference(){ $p = $this->__rpinfo(); return $p['byref']; }"
" public function canBePassedByValue(){ return !$this->isPassedByReference(); }"
" public function isVariadic(){ $p = $this->__rpinfo(); return $p['variadic']; }"
" public function isPromoted(){ $p = $this->__rpinfo(); return $p['promoted']; }"
" public function isDefaultValueAvailable(){ $p = $this->__rpinfo(); return $p['hasdef']; }"
" public function isOptional(){"
"  $i = $this->__rffull();"
"  $n = count($i['params']);"
"  for($k = $this->__p; $k < $n; $k++){"
"   $p = $i['params'][$k];"
"   if(!$p['variadic'] && !$p['hasdef']){ return false; }"
"  }"
"  return true;"
" }"
" public function getDefaultValue(){"
"  if(!$this->isDefaultValueAvailable()){"
"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"
"  }"
"  return __reflect_param_default($this->__t, $this->__m, $this->__p);"
" }"
" public function isDefaultValueConstant(){"
"  if(!$this->isDefaultValueAvailable()){ return false; }"
"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p) !== null;"
" }"
" public function getDefaultValueConstantName(){"
"  if(!$this->isDefaultValueAvailable()){"
"   throw new ReflectionException('Internal error: Failed to retrieve the default value');"
"  }"
"  return __reflect_param_defconst($this->__t, $this->__m, $this->__p);"
" }"
" public function allowsNull(){"
"  $p = $this->__rpinfo();"
"  if($p['typetext'] === null){ return true; }"
"  if($p['nullable']){ return true; }"
"  return $p['typetext'] === 'mixed' || $p['typetext'] === 'null';"
" }"
" public function hasType(){ $p = $this->__rpinfo(); return $p['typetext'] !== null; }"
" public function getType(){ $p = $this->__rpinfo(); return __reflect_make_type($p['typetext']); }"
" public function getDeclaringFunction(){"
"  if($this->__m !== null){ return new ReflectionMethod($this->__t, $this->__m); }"
"  return new ReflectionFunction($this->__t);"
" }"
" public function getDeclaringClass(){"
"  if($this->__m === null){ return null; }"
"  $i = $this->__rffull();"
"  return new ReflectionClass($i['decl']);"
" }"
" public function getAttributes($name = null, $flags = 0){ return array(); }"
" public function __toString(){"
"  return 'Parameter #'.$this->__p.' [ <required> $'.$this->name.' ]';"
" }"
"}"
;
/*
 * Chunk 3: ReflectionProperty, ReflectionClassConstant.
 */
static const char zReflectLib3[] =
"class ReflectionProperty implements Reflector {"
" const IS_PUBLIC = 1;"
" const IS_PROTECTED = 2;"
" const IS_PRIVATE = 4;"
" const IS_STATIC = 16;"
" const IS_FINAL = 32;"
" const IS_ABSTRACT = 64;"
" const IS_READONLY = 128;"
" const IS_VIRTUAL = 512;"
" const IS_PROTECTED_SET = 2048;"
" const IS_PRIVATE_SET = 4096;"
" public $name;"
" public $class;"
" protected $__dynobj = null;"
" public function __construct($class, $property){"
"  $obj = null;"
"  if(is_object($class)){ $obj = $class; }"
"  else if(!is_string($class)){"
"   throw new TypeError('ReflectionProperty::__construct(): Argument #1 ($class) must be of type object|string, '.get_debug_type($class).' given');"
"  }"
"  $ci = __reflect_class_info($class);"
"  if($ci === null){"
"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"
"  }"
"  $this->class = $ci['name'];"
"  if(isset($ci['props'][$property])){"
"   $this->name = $property;"
"   return;"
"  }"
"  if($obj !== null && (__reflect_prop_state($obj, $property) & 1)){"
"   $this->name = $property;"
"   $this->__dynobj = $obj;"
"   return;"
"  }"
"  throw new ReflectionException('Property '.$this->class.'::$'.$property.' does not exist');"
" }"
" protected function __rpmeta(){"
"  $ci = __reflect_class_info($this->class);"
"  if(isset($ci['props'][$this->name])){ return $ci['props'][$this->name]; }"
"  return array('vis' => 1, 'static' => false, 'readonly' => false, 'hasdef' => false,"
"   'typed' => false, 'typetext' => null, 'decl' => $this->class, 'line' => 0, 'dyn' => true);"
" }"
" public function getName(){ return $this->name; }"
" public function getDeclaringClass(){"
"  $m = $this->__rpmeta();"
"  return new ReflectionClass($m['decl']);"
" }"
" public function getModifiers(){"
"  $m = $this->__rpmeta();"
"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"
"  if($m['static']){ $mod |= 16; }"
"  if($m['readonly']){ $mod |= 128; }"
"  return $mod;"
" }"
" public function isPublic(){ $m = $this->__rpmeta(); return $m['vis'] === 1; }"
" public function isProtected(){ $m = $this->__rpmeta(); return $m['vis'] === 2; }"
" public function isPrivate(){ $m = $this->__rpmeta(); return $m['vis'] === 3; }"
" public function isStatic(){ $m = $this->__rpmeta(); return $m['static']; }"
" public function isReadOnly(){ $m = $this->__rpmeta(); return $m['readonly']; }"
" public function isDefault(){ $m = $this->__rpmeta(); return !isset($m['dyn']); }"
" public function isDynamic(){ $m = $this->__rpmeta(); return isset($m['dyn']); }"
" public function isAbstract(){ return false; }"
" public function isFinal(){ return false; }"
" public function isVirtual(){ return false; }"
" public function isPrivateSet(){ return false; }"
" public function isProtectedSet(){ return false; }"
" public function hasHooks(){ return false; }"
" public function getHooks(){ return array(); }"
" public function hasHook($type){ return false; }"
" public function getHook($type){ return null; }"
" public function isLazy($object){ return false; }"
" public function setAccessible($accessible){ }"
" public function getValue($object = null){"
"  $m = $this->__rpmeta();"
"  if($m['static']){ return __reflect_static_value($this->class, $this->name); }"
"  if(!is_object($object)){"
"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"
"  }"
"  return __reflect_prop_read($object, $this->name);"
" }"
" public function setValue($objectOrValue = null, $value = null){"
"  $m = $this->__rpmeta();"
"  if($m['static']){"
"   if($value === null && $objectOrValue !== null && !is_object($objectOrValue)){"
"    __reflect_static_set($this->class, $this->name, $objectOrValue);"
"   }else{"
"    __reflect_static_set($this->class, $this->name, $value);"
"   }"
"   return;"
"  }"
"  __reflect_prop_write($objectOrValue, $this->name, $value);"
" }"
" public function getRawValue($object){ return $this->getValue($object); }"
" public function setRawValue($object, $value){ $this->setValue($object, $value); }"
" public function isInitialized($object = null){"
"  $m = $this->__rpmeta();"
"  if($m['static']){ return (__reflect_prop_state($this->class, $this->name) & 2) !== 0; }"
"  if(!is_object($object)){"
"   throw new ReflectionException('Instance of '.$this->class.' expected, but '.get_debug_type($object).' given');"
"  }"
"  return (__reflect_prop_state($object, $this->name) & 2) !== 0;"
" }"
" public function hasDefaultValue(){"
"  $m = $this->__rpmeta();"
"  if(isset($m['dyn'])){ return false; }"
"  if($m['hasdef']){ return true; }"
"  return !$m['typed'];"
" }"
" public function getDefaultValue(){"
"  $m = $this->__rpmeta();"
"  if(isset($m['dyn']) || !$m['hasdef']){ return null; }"
"  return __reflect_prop_default($this->class, $this->name);"
" }"
" public function hasType(){ $m = $this->__rpmeta(); return $m['typed']; }"
" public function getType(){ $m = $this->__rpmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"
" public function getSettableType(){ return $this->getType(); }"
" public function getDocComment(){ return false; }"
" public function getAttributes($name = null, $flags = 0){ return array(); }"
" public function __toString(){"
"  return 'Property [ public $'.$this->name.' ]'.\"\\n\";"
" }"
"}"
"class ReflectionClassConstant implements Reflector {"
" const IS_PUBLIC = 1;"
" const IS_PROTECTED = 2;"
" const IS_PRIVATE = 4;"
" const IS_FINAL = 32;"
" public $name;"
" public $class;"
" public function __construct($class, $constant){"
"  if(!is_object($class) && !is_string($class)){"
"   throw new TypeError('ReflectionClassConstant::__construct(): Argument #1 ($class) must be of type object|string, '.get_debug_type($class).' given');"
"  }"
"  $ci = __reflect_class_info($class);"
"  if($ci === null){"
"   throw new ReflectionException('Class \"'.$class.'\" does not exist');"
"  }"
"  $this->class = $ci['name'];"
"  if(!isset($ci['consts'][$constant])){"
"   throw new ReflectionException('Constant '.$this->class.'::'.$constant.' does not exist');"
"  }"
"  $this->name = $constant;"
" }"
" protected function __rcmeta(){"
"  $ci = __reflect_class_info($this->class);"
"  return $ci['consts'][$this->name];"
" }"
" public function getName(){ return $this->name; }"
" public function getValue(){ return __reflect_const_value($this->class, $this->name); }"
" public function getDeclaringClass(){"
"  $m = $this->__rcmeta();"
"  return new ReflectionClass($m['decl']);"
" }"
" public function getModifiers(){"
"  $m = $this->__rcmeta();"
"  $mod = ($m['vis'] === 1 ? 1 : ($m['vis'] === 2 ? 2 : 4));"
"  if($m['final']){ $mod |= 32; }"
"  return $mod;"
" }"
" public function isPublic(){ $m = $this->__rcmeta(); return $m['vis'] === 1; }"
" public function isProtected(){ $m = $this->__rcmeta(); return $m['vis'] === 2; }"
" public function isPrivate(){ $m = $this->__rcmeta(); return $m['vis'] === 3; }"
" public function isFinal(){ $m = $this->__rcmeta(); return $m['final']; }"
" public function isEnumCase(){ return false; }"
" public function isDeprecated(){ return false; }"
" public function hasType(){ $m = $this->__rcmeta(); return $m['typed']; }"
" public function getType(){ $m = $this->__rcmeta(); return $m['typed'] ? __reflect_make_type($m['typetext']) : null; }"
" public function getDocComment(){ return false; }"
" public function getAttributes($name = null, $flags = 0){ return array(); }"
" public function __toString(){"
"  return 'Constant [ public '.$this->name.' ]'.\"\\n\";"
" }"
"}"
;
/*
 * Chunk 4: the ReflectionType family, built from the engine's canonical
 * type text ("?int", "string|float", "(A&B)|C" — normalized at compile
 * time). __reflect_make_type is the internal factory; PHP itself never
 * lets user code construct these, so the public constructors here are a
 * recorded PHL-only surface.
 */
static const char zReflectLib4[] =
"abstract class ReflectionType implements Stringable {"
" protected $__text = '';"
" protected $__nullable = false;"
" public function allowsNull(){ return $this->__nullable; }"
" public function __toString(){ return $this->__text; }"
"}"
"class ReflectionNamedType extends ReflectionType {"
" protected $__tname = '';"
" public function __construct($name = '', $nullable = false, $text = null){"
"  $this->__tname = $name;"
"  $l = strtolower($name);"
"  $this->__nullable = $nullable || $l === 'null' || $l === 'mixed';"
"  $this->__text = $text === null ? $name : $text;"
" }"
" public function getName(){ return $this->__tname; }"
" public function isBuiltin(){"
"  $l = strtolower($this->__tname);"
"  return in_array($l, array('int','float','string','bool','array','object','mixed',"
"   'void','never','null','callable','iterable','true','false'), true);"
" }"
"}"
"class ReflectionUnionType extends ReflectionType {"
" protected $__types = array();"
" public function __construct($text = '', $nullable = false, $types = array()){"
"  $this->__text = $text;"
"  $this->__nullable = $nullable;"
"  $this->__types = $types;"
" }"
" public function getTypes(){ return $this->__types; }"
"}"
"class ReflectionIntersectionType extends ReflectionType {"
" protected $__types = array();"
" public function __construct($text = '', $types = array()){"
"  $this->__text = $text;"
"  $this->__nullable = false;"
"  $this->__types = $types;"
" }"
" public function getTypes(){ return $this->__types; }"
"}"
"function __reflect_make_atom($p){"
" $nullable = false;"
" if($p !== '' && $p[0] === '?'){ $nullable = true; $p = substr($p, 1); }"
" if($p !== '' && $p[0] === '('){ $p = substr($p, 1, strlen($p) - 2); }"
" if(strpos($p, '&') !== false){"
"  $subs = array();"
"  foreach(explode('&', $p) as $s){ $subs[] = new ReflectionNamedType($s, false, $s); }"
"  return new ReflectionIntersectionType($p, $subs);"
" }"
" return new ReflectionNamedType($p, $nullable, $nullable ? '?'.$p : $p);"
"}"
"function __reflect_make_type($text){"
" if($text === null || $text === ''){ return null; }"
" $nullable = false;"
" $body = $text;"
" if($body[0] === '?'){ $nullable = true; $body = substr($body, 1); }"
" $parts = array();"
" $depth = 0;"
" $cur = '';"
" $n = strlen($body);"
" for($k = 0; $k < $n; $k++){"
"  $ch = $body[$k];"
"  if($ch === '('){ $depth++; $cur .= $ch; }"
"  else if($ch === ')'){ $depth--; $cur .= $ch; }"
"  else if($ch === '|' && $depth === 0){ $parts[] = $cur; $cur = ''; }"
"  else{ $cur .= $ch; }"
" }"
" $parts[] = $cur;"
" if(count($parts) > 1){"
"  $nonNull = array();"
"  $hasNull = false;"
"  foreach($parts as $p){"
"   if(strtolower($p) === 'null'){ $hasNull = true; }"
"   else{ $nonNull[] = $p; }"
"  }"
"  if($hasNull && count($nonNull) === 1 && strpos($nonNull[0], '&') === false){"
"   return new ReflectionNamedType($nonNull[0], true, '?'.$nonNull[0]);"
"  }"
"  $types = array();"
"  foreach($parts as $p){ $types[] = __reflect_make_atom($p); }"
"  return new ReflectionUnionType($body, $nullable || $hasNull, $types);"
" }"
" if(strpos($body, '&') !== false){ return __reflect_make_atom($body); }"
" return __reflect_make_atom($nullable ? '?'.$body : $body);"
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
		{ "__reflect_class_info",     vm_builtin_reflect_class_info },
		{ "__reflect_const_value",    vm_builtin_reflect_const_value },
		{ "__reflect_static_value",   vm_builtin_reflect_static_value },
		{ "__reflect_static_set",     vm_builtin_reflect_static_set },
		{ "__reflect_prop_default",   vm_builtin_reflect_prop_default },
		{ "__reflect_new_instance",   vm_builtin_reflect_new_instance },
		{ "__reflect_new_no_ctor",    vm_builtin_reflect_new_no_ctor },
		{ "__reflect_func_info",      vm_builtin_reflect_func_info },
		{ "__reflect_param_default",  vm_builtin_reflect_param_default },
		{ "__reflect_param_defconst", vm_builtin_reflect_param_defconst },
		{ "__reflect_invoke",         vm_builtin_reflect_invoke },
		{ "__reflect_closure",        vm_builtin_reflect_closure },
		{ "__reflect_prop_read",      vm_builtin_reflect_prop_read },
		{ "__reflect_prop_write",     vm_builtin_reflect_prop_write },
		{ "__reflect_prop_state",     vm_builtin_reflect_prop_state },
		{ "__reflect_dyn_props",      vm_builtin_reflect_dyn_props },
	};
	sxu32 n;
	sxi32 rc;
	for( n = 0 ; n < sizeof(aFunc)/sizeof(aFunc[0]) ; n++ ){
		ph7_create_function(&(*pVm), aFunc[n].zName, aFunc[n].xFunc, 0);
	}
	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib1, sizeof(zReflectLib1)-1);
	if( rc != SXRET_OK ){
		return rc;
	}
	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib2, sizeof(zReflectLib2)-1);
	if( rc != SXRET_OK ){
		return rc;
	}
	rc = PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib3, sizeof(zReflectLib3)-1);
	if( rc != SXRET_OK ){
		return rc;
	}
	return PH7_VmEvalBuiltinChunk(&(*pVm), zReflectLib4, sizeof(zReflectLib4)-1);
}
