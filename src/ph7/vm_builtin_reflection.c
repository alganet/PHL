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
/* The last of the descriptor marshalling: ReflectMapAddDyn survives because
 * ReflectAttrArgs() answers a php ARRAY whose named arguments are string keys.
 * Its Bool/Int/Str/Null/Attrs/Doc siblings went with __phl_rcinfo. */
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
/*
 * The interface list php reports for pClass: the transitive set, plus an
 * INTERFACE's own parents (`interface B extends A` lists A). Caller owns the
 * set (SySetInit with sizeof(ph7_class *)).
 */
static void ReflectInterfacesOf(ph7_class *pClass, SySet *pOut)
{
	ReflectCollectInterfaces(pClass, pOut, 0);
	if( (pClass->iFlags & PH7_CLASS_INTERFACE) && pClass->pBase ){
		ReflectAddInterface(pClass->pBase, pOut, 0);
	}
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
/* Fetch a class CONSTANT (or enum case) by name from the hConst namespace. */
static ph7_class_attr * ReflectFetchConst(ph7_class *pClass, ph7_value *pName)
{
	const char *zName;
	int nLen;
	zName = ph7_value_to_string(pName, &nLen);
	if( nLen < 1 ){
		return 0;
	}
	return PH7_ClassExtractConstant(pClass, zName, (sxu32)nLen);
}
/* Fetch a member by name from EITHER namespace (property then constant) — used
 * where the caller reflects over a member that may be either (e.g. attributes
 * attached to a property or a constant). */
static ph7_class_attr * ReflectFetchMember(ph7_class *pClass, ph7_value *pName)
{
	ph7_class_attr *pAttr = ReflectFetchAttr(pClass, pName);
	if( pAttr == 0 ){
		pAttr = ReflectFetchConst(pClass, pName);
	}
	return pAttr;
}
/*
 * ---------------------------------------------------------------------------
 * The member walk.
 *
 * php reports a class's members in ONE order — the class's own first (in
 * declaration order), then each inheritance level's, outward — and every
 * accessor that lists or looks one up has to agree with it. It used to live
 * inside the descriptor builder alone; the native ReflectionClass needs the
 * same order for getMethods()/getProperties()/getReflectionConstants() and the
 * same visibility filtering for hasMethod()/getMethod()/getConstructor(), so
 * the walk is factored out here and every one of them drives it.
 *
 * Per level the DECLARING class's own hash is iterated — a subclass hash
 * interleaves inherited pointers unpredictably — and a pointer-identity lookup
 * in the reflected class's hash drops what is not visible there (overridden
 * entries). Methods come out reversed because hMethod is still a head-insert
 * table, while hAttr/hConst insert at the tail.
 * ---------------------------------------------------------------------------
 */
#define REFLECT_MEMBER_PROP   0
#define REFLECT_MEMBER_CONST  1
#define REFLECT_MEMBER_METHOD 2

typedef struct ReflectMember ReflectMember;
struct ReflectMember
{
	int iKind;               /* REFLECT_MEMBER_* */
	SyString sKey;           /* the name php reports it under: a trait
	                          * `use T { m as n; }` alias differs from the
	                          * method's own sFunc.sName, and php reports n */
	ph7_class *pDecl;        /* declaring class */
	ph7_class_attr *pAttr;   /* property or constant (NULL for a method) */
	ph7_class_method *pMeth; /* method (NULL for a property or constant) */
};
/*
 * Collect the members php would report for pClass, in php's own order, into a
 * SySet of ReflectMember. The caller owns the set (SySetInit with
 * sizeof(ReflectMember) / SySetRelease).
 *
 * bLookup selects which of php's TWO answers is wanted. The LISTING
 * (getMethods()/getProperties()/getReflectionConstants(), bLookup = 0) hides a
 * base class's private members; the LOOKUP (bLookup = 1) does not, because php
 * keeps a parent's private in the child's tables and ReflectionMethod resolves
 * against those. The two really do disagree: hasMethod('basePriv') is true on
 * the subclass while getMethods() never mentions it.
 */
static void ReflectMembers(ph7_vm *pVm, ph7_class *pClass, SySet *pOut, int bLookup)
{
	ph7_class *aChain[REFLECT_WALK_MAX_DEPTH + 1];
	ph7_class *pWalk = pClass;
	SyHashEntry *pEntry;
	SySet aTmp;
	sxu32 nChain = 0, iLevel, nT;
	while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){
		aChain[nChain++] = pWalk;
		pWalk = pWalk->pBase;
	}
	SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));
	for( iLevel = 0 ; iLevel < nChain ; iLevel++ ){
		ph7_class *pLevel = aChain[iLevel];
		int iTab;
		/* --- Properties (hAttr) then constants/enum cases (hConst) — php's two
		 * separate member namespaces. Each table is collected and emitted
		 * independently; the CONSTANT flag still decides which kind comes out. --- */
		for( iTab = 0 ; iTab < 2 ; iTab++ ){
			SyHash *pSrcHash = iTab ? &pLevel->hConst : &pLevel->hAttr;
			SyHash *pRefHash = iTab ? &pClass->hConst : &pClass->hAttr;
			SySetReset(&aTmp);
			SyHashResetLoopCursor(pSrcHash);
			while( (pEntry = SyHashGetNextEntry(pSrcHash)) != 0 ){
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
					/* A base's PRIVATE member is not part of the subclass's
					 * surface — php reports neither a private property nor a
					 * private constant of a parent on the child. PHL's
					 * inheritance copies them down all the same, so the filter
					 * has to be here. */
					if( !bLookup && pAttr->iProtection == PH7_CLASS_PROT_PRIVATE ){ continue; }
					/* Must still be the visible member in the reflected class */
					pSub = SyHashGet(pRefHash, pEntry->pKey, pEntry->nKeyLen);
					if( pSub == 0 || pSub->pUserData != (void *)pAttr ){ continue; }
				}
				SySetPut(&aTmp, (const void *)&pEntry);
			}
			/* Forward: hAttr/hConst iterate in DECLARATION order (tail inserts),
			 * so members come out in the order php reports them. */
			for( nT = 0 ; nT < SySetUsed(&aTmp) ; nT++ ){
				SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT);
				ph7_class_attr *pAttr = (ph7_class_attr *)pE->pUserData;
				ReflectMember sMember;
				sMember.iKind = (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT)
					? REFLECT_MEMBER_CONST : REFLECT_MEMBER_PROP;
				sMember.sKey = pAttr->sName;
				sMember.pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pLevel;
				sMember.pAttr = pAttr;
				sMember.pMeth = 0;
				SySetPut(pOut, (const void *)&sMember);
			}
		}
		/* --- Methods. The reported name is the hash-entry KEY, not the
		 * function's own name (see ReflectMember::sKey). --- */
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
				/* Same rule as the members above: a base's PRIVATE method is
				 * not on the subclass's surface. `class B extends A` lists
				 * only A::q when A::p is private — php's inheritance never
				 * hands the child a private, and PH7_ClassInherit's copy-down
				 * does, so it is filtered here. */
				if( !bLookup && pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){ continue; }
				pSub = SyHashGet(&pClass->hMethod, pEntry->pKey, pEntry->nKeyLen);
				if( pSub == 0 || pSub->pUserData != (void *)pMeth ){
					/* Overridden below this level: already reported */
					continue;
				}
			}
			SySetPut(&aTmp, (const void *)&pEntry);
		}
		for( nT = SySetUsed(&aTmp) ; nT > 0 ; nT-- ){
			SyHashEntry *pE = *(SyHashEntry **)SySetAt(&aTmp, nT - 1);
			ReflectMember sMember;
			sMember.iKind = REFLECT_MEMBER_METHOD;
			SyStringInitFromBuf(&sMember.sKey, (const char *)pE->pKey, pE->nKeyLen);
			sMember.pMeth = (ph7_class_method *)pE->pUserData;
			sMember.pDecl = ReflectMethodDeclClass(pClass, sMember.pMeth);
			sMember.pAttr = 0;
			SySetPut(pOut, (const void *)&sMember);
		}
	}
	SySetRelease(&aTmp);
}
/* Does a collected member name match zName exactly? */
static int ReflectKeyIs(const ReflectMember *pM, const char *zName, int nName)
{
	return SyStringLength(&pM->sKey) == (sxu32)nName
		&& SyMemcmp(SyStringData(&pM->sKey), zName, (sxu32)nName) == 0;
}
/*
 * php's METHOD lookup, which is NOT the listing.
 *
 * getMethods() hides a base's private method, but hasMethod()/getMethod() find
 * one: Zend keeps the parent's private in the child's function table and reads
 * that table directly. PHL's inheritance copies methods down the same way, so
 * the lookup is the class's own hMethod — case-insensitively, like php.
 */
static SyHashEntry * ReflectFindMethodEntry(ph7_class *pClass, const char *zName, int nName)
{
	SyHashEntry *pEntry;
	if( nName < 1 ){
		return 0;
	}
	pEntry = SyHashGet(&pClass->hMethod, (const void *)zName, (sxu32)nName);
	if( pEntry ){
		return pEntry;
	}
	SyHashResetLoopCursor(&pClass->hMethod);
	while( (pEntry = SyHashGetNextEntry(&pClass->hMethod)) != 0 ){
		if( (int)pEntry->nKeyLen == nName
		 && SyStrnicmp((const char *)pEntry->pKey, zName, (sxu32)nName) == 0 ){
			return pEntry;
		}
	}
	return 0;
}
/*
 * The visibility of the __construct / __clone `new` and `clone` would reach, or
 * 0 when the class has none — what isInstantiable() and isCloneable() screen on.
 * The LOOKUP, not the listing: a class that inherits a private constructor is
 * still not instantiable even though getMethods() does not report one.
 */
static void ReflectCtorCloneVis(ph7_vm *pVm, ph7_class *pClass, sxi32 *piCtor, sxi32 *piClone)
{
	ph7_class_method *pMeth;
	SXUNUSED(pVm);
	*piCtor = *piClone = 0;
	pMeth = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);
	if( pMeth ){
		*piCtor = pMeth->iProtection;
	}
	pMeth = PH7_ClassExtractMethod(pClass, "__clone", sizeof("__clone")-1);
	if( pMeth ){
		*piClone = pMeth->iProtection;
	}
}
/* Where __phl_rcinfo() was: a full class DESCRIPTOR array — every constant,
 * property and method of the whole inheritance chain, marshalled once and
 * memoized on pVm->hClassInfo because the prelude classes rebuilt it once per
 * member they constructed. Every one of its readers is a native class now and
 * reads ph7_class directly, so the builder and the VM memo are both gone. */
/*
 * Collect a PHP array's values into a ph7_value* set (call arguments).
 * When ppNames is non-NULL, string keys become named arguments: a name
 * map is lazily allocated (like call_user_func_array's) with one entry
 * per collected slot, empty entries meaning positional.
 */
static sxi32 ReflectCollectArgs(ph7_context *pCtx, ph7_value *pArray, SySet *pOut, SyString **ppNames)
{
	ph7_hashmap *pMap;
	ph7_hashmap_node *pEntry;
	SyString *aNames = 0;
	sxu32 nSlot = 0;
	sxu32 n;
	if( ppNames ){
		*ppNames = 0;
	}
	if( !ph7_value_is_array(pArray) ){
		return SXRET_OK;
	}
	pMap = (ph7_hashmap *)pArray->x.pOther;
	pEntry = pMap->pFirst;
	for( n = 0 ; n < pMap->nEntry ; n++ ){
		ph7_value *pValue = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);
		if( pValue ){
			if( ppNames && pEntry->iType == HASHMAP_BLOB_NODE ){
				if( aNames == 0 ){
					aNames = (SyString *)SyMemBackendAlloc(&pCtx->pVm->sAllocator,
						pMap->nEntry * sizeof(SyString));
					if( aNames ){
						SyZero(aNames, pMap->nEntry * sizeof(SyString));
					}
				}
				if( aNames ){
					SyStringInitFromBuf(&aNames[nSlot],
						SyBlobData(&pEntry->xKey.sKey), SyBlobLength(&pEntry->xKey.sKey));
				}
			}
			SySetPut(pOut, (const void *)&pValue);
			nSlot++;
		}
		pEntry = pEntry->pPrev; /* Reverse link: insertion order */
	}
	if( ppNames ){
		*ppNames = aNames;
	}
	return SXRET_OK;
}
/*
 * Instantiate pClassName and run its constructor over pArgs (a PHP array;
 * string keys become NAMED arguments, which is how `#[Attr(x: 1)]` arrives).
 * The object lands in the call's result slot.
 *
 * ReflectionAttribute::newInstance()'s back end. It is deliberately NOT the
 * ReflectionClass one (ReflectNewInstance, below): php runs no instantiability
 * or constructor-visibility screen here — the attribute's own #[Attribute]
 * declaration is what was checked — so an abstract or private-ctor attribute
 * class reaches the engine's own Error, not a ReflectionException.
 */
static sxi32 ReflectAttrInstantiate(ph7_context *pCtx, ph7_value *pClassName, ph7_value *pArgs)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass;
	ph7_class_instance *pThis;
	ph7_class_method *pCons;
	if( (pClass = ReflectResolveClass(pVm, pClassName)) == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( VmClassStaticDeferPending(pClass) ){
		/* Instantiation materializes the static table (OP_NEW does it too), so a
		 * broken default raises BEFORE any object exists. */
		sxi32 rcMat = PH7_VmMaterializeClassStatics(pVm, pClass);
		if( rcMat != SXRET_OK ){
			return rcMat;
		}
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
		SyString *aNames = 0;
		SySetInit(&aArg, &pVm->sAllocator, sizeof(ph7_value *));
		if( pArgs ){
			ReflectCollectArgs(pCtx, pArgs, &aArg, &aNames);
		}
		if( aNames ){
			VmCallArgMap sMap;
			SyZero(&sMap,sizeof(sMap)); /* new map fields must read unset, not stack garbage */
			sMap.bHasNamed = 1;
			sMap.bIsNamespaced = 0;
			sMap.bStrict = 0;
			sMap.nTotal = SySetUsed(&aArg);
			sMap.aNames = aNames;
			rc = PH7_VmCallClassMethodMap(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),
				(ph7_value **)SySetBasePtr(&aArg), &sMap);
			SyMemBackendFree(&pVm->sAllocator, aNames);
		}else{
			rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, (int)SySetUsed(&aArg),
				(ph7_value **)SySetBasePtr(&aArg));
		}
		SySetRelease(&aArg);
		if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){
			PH7_ClassInstanceUnref(pThis);
			return rc;
		}
	}
	return ReflectResultObject(pCtx, pThis);
}
/* Where __reflect_new_no_ctor() was: the one caller was chunk 7's
 * __reflect_build_attrs, which built a ReflectionAttribute and then filled it
 * through a public __init() php does not have. C builds the instance itself
 * (ReflectAttrNew), so both are gone. */
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
			/* getMethods()/ReflectionClass reports a base class's PRIVATE methods on
			 * the subclass (php copies them into the child's table), but private
			 * methods are not inherited into the child's method table, so the plain
			 * extract misses them when a ReflectionMethod obtained from the subclass
			 * is re-resolved by (subclass, name). Walk the base chain to find the
			 * declaring class's own copy. */
			ph7_class *pWalk = pClass->pBase;
			while( pWalk && pMeth == 0 ){
				pMeth = PH7_ClassExtractMethod(pWalk, (const char *)SyBlobData(&pMethodArg->sBlob),
					SyBlobLength(&pMethodArg->sBlob));
				pWalk = pWalk->pBase;
			}
		}
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
			/* A closure over an object method or __invoke object
			 * (Closure::fromCallable([$obj,'m']) / fromCallable($invokeObj)) stores the
			 * bare method name in $__fn and the class in $__scope. Resolve it as a METHOD
			 * of $__scope FIRST — before the global function table — so a same-named
			 * global function does not shadow the method (the bug: $__fn "add" hitting a
			 * global add()). A plain anonymous closure's $__fn is its unique lambda name,
			 * which is not a method, so this falls through cleanly. */
			{
				SyString sScope;
				ph7_value *pScope;
				SyStringInitFromBuf(&sScope, "__scope", 7);
				pScope = PH7_ClassInstanceFetchAttr(pClo, &sScope);
				if( pScope && (pScope->iFlags & MEMOBJ_STRING) && SyBlobLength(&pScope->sBlob) > 0 ){
					ph7_class *pScopeCls = PH7_VmExtractClass(pVm,
						(const char *)SyBlobData(&pScope->sBlob), SyBlobLength(&pScope->sBlob), FALSE, 0);
					if( pScopeCls ){
						ph7_class_method *pScopeMeth = PH7_ClassExtractMethod(pScopeCls,
							(const char *)SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob));
						if( pScopeMeth ){
							if( ppClass ){ *ppClass = pScopeCls; }
							if( ppMeth ){ *ppMeth = pScopeMeth; }
							if( ppClosure ){ *ppClosure = pClo; }
							return &pScopeMeth->sFunc;
						}
					}
				}
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
/*
 * ---------------------------------------------------------------------------
 * The signature parser.
 *
 * A C builtin and a native method declare their parameters as ONE php-style
 * string (`"string $name, ?int $len = null"`) — the aBuiltinSig[] row or the
 * PH7_NativeClassSpec's zSig — because neither has a compiled parameter list to
 * read. Reflection needs that string as the same param-meta shape a compiled
 * function produces, so it is parsed here and the descriptor comes out of
 * __reflect_func_info() already uniform.
 *
 * This was chunk 8 of the reflection prelude (__reflect_sig_split /
 * __reflect_sig_scalar / __reflect_parse_sig / __reflect_sig_fixup), which every
 * caller had to remember to wrap around __reflect_func_info() — six call sites,
 * and the one that FORGOT is why every native method reported zero parameters
 * until 31 Jul. Producing the parsed form at the source removes the wrapper and
 * the possibility of forgetting it.
 * ---------------------------------------------------------------------------
 */
/* Trim ASCII spaces off both ends of [z, z+n). */
static void ReflectSigTrim(const char **pz, int *pn)
{
	const char *z = *pz;
	int n = *pn;
	while( n > 0 && (z[0] == ' ' || z[0] == '\t') ){
		z++;
		n--;
	}
	while( n > 0 && (z[n-1] == ' ' || z[n-1] == '\t') ){
		n--;
	}
	*pz = z;
	*pn = n;
}
/* Byte search that respects single-quoted runs (a default may be `'a,b'` or
 * `'it\'s'`), which is the whole reason the split is not SyByteFind. Answers
 * the offset of the first unquoted zWhat, or -1. */
static int ReflectSigFindUnquoted(const char *z, int n, char cWhat)
{
	int k, bQuote = 0;
	for( k = 0 ; k < n ; k++ ){
		if( bQuote ){
			if( z[k] == '\\' && k + 1 < n ){
				k++;
			}else if( z[k] == '\'' ){
				bQuote = 0;
			}
		}else if( z[k] == '\'' ){
			bQuote = 1;
		}else if( z[k] == cWhat ){
			return k;
		}
	}
	return -1;
}
/* Does [z,n) contain zNeedle? (case-sensitive; nNeedle > 0) */
static int ReflectSigHas(const char *z, int n, const char *zNeedle, int nNeedle)
{
	int k;
	for( k = 0 ; k + nNeedle <= n ; k++ ){
		if( SyMemcmp((const void *)&z[k], (const void *)zNeedle, (sxu32)nNeedle) == 0 ){
			return 1;
		}
	}
	return 0;
}
/* Case-insensitive twin, for the `null` arm of a union type text. */
static int ReflectSigHasNoCase(const char *z, int n, const char *zNeedle, int nNeedle)
{
	int k, j;
	for( k = 0 ; k + nNeedle <= n ; k++ ){
		for( j = 0 ; j < nNeedle ; j++ ){
			if( SyToLower(z[k+j]) != SyToLower(zNeedle[j]) ){
				break;
			}
		}
		if( j == nNeedle ){
			return 1;
		}
	}
	return 0;
}
/*
 * A default-value TEXT to a value, when the text denotes a scalar php can
 * reproduce. Answers 1 and fills pOut, or 0 for anything else (`[]`,
 * `array (`, a constant name) which the caller reports its own way.
 */
static int ReflectSigScalar(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)
{
	sxu8 bReal = 0;
	if( n == 1 && z[0] == '?' ){
		return 0;
	}
	if( (n == 4 && (SyMemcmp(z,"NULL",4) == 0 || SyMemcmp(z,"null",4) == 0)) ){
		ph7_value_null(pOut);
		return 1;
	}
	if( n == 4 && SyMemcmp(z,"true",4) == 0 ){
		ph7_value_bool(pOut,1);
		return 1;
	}
	if( n == 5 && SyMemcmp(z,"false",5) == 0 ){
		ph7_value_bool(pOut,0);
		return 1;
	}
	if( n >= 2 && z[0] == '\'' && z[n-1] == '\'' ){
		/* Unescape \' and \\ , the only two escapes the signature writer emits. */
		SyBlob sOut;
		int k;
		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
		for( k = 1 ; k < n - 1 ; k++ ){
			if( z[k] == '\\' && k + 1 < n - 1 && (z[k+1] == '\'' || z[k+1] == '\\') ){
				k++;
			}
			SyBlobAppend(&sOut,(const void *)&z[k],sizeof(char));
		}
		ph7_value_string(pOut,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
		SyBlobRelease(&sOut);
		return 1;
	}
	if( n > 0 && SyStrIsNumeric(z,(sxu32)n,&bReal,0) == SXRET_OK ){
		/* php's own rule for the text form: a '.', an exponent or a hex marker
		 * makes it a float, everything else an int. */
		if( bReal || ReflectSigHas(z,n,".",1)
		 || ReflectSigHasNoCase(z,n,"e",1) || ReflectSigHasNoCase(z,n,"x",1) ){
#ifndef PH7_OMIT_FLOATING_POINT
			ph7_value_double(pOut,SyStrToReal(z,(sxu32)n,0,0));
#else
			ph7_value_int64(pOut,SyStrToInt64(z,(sxu32)n,0,0));
#endif
		}else{
			sxi64 iVal = 0;
			SyStrToInt64(z,(sxu32)n,(void *)&iVal,0);
			ph7_value_int64(pOut,iVal);
		}
		return 1;
	}
	return 0;
}
/*
 * One parameter, described uniformly.
 *
 * A reflected function's parameters come from one of TWO places — a compiled
 * function's `ph7_vm_func_arg` list, or the php-style SIGNATURE STRING a C
 * builtin / native method declares — and both the descriptor array
 * (__reflect_func_info, for the chunks still in PHP) and the native
 * ReflectionParameter have to read them the same way. This struct is what they
 * agree on; `pArg` is set only on the compiled path, where a default is
 * BYTE-CODE rather than text.
 */
typedef struct ReflectParamDesc ReflectParamDesc;
struct ReflectParamDesc
{
	SyString sName;
	int iPos;
	int bByRef, bVariadic, bHasDef, bOptional, bNullable, bPromoted;
	SyString sType;            /* nByte == 0 -> untyped */
	SyString sDefText;         /* signature-declared default TEXT (nByte == 0 -> none) */
	ph7_vm_func_arg *pArg;     /* compiled parameter, or NULL for a declared one */
};
/*
 * Split a signature string on its top-level commas (a quoted default may hold
 * its own). Answers the parameter COUNT; when iWant is in range, hands back
 * that part's bytes.
 */
static int ReflectSigPart(const char *zSig, int nSig, int iWant,
	const char **pzPart, int *pnPart)
{
	int iPos = 0;
	while( nSig > 0 ){
		int iComma = ReflectSigFindUnquoted(zSig,nSig,',');
		const char *zPart = zSig;
		int nPart = iComma < 0 ? nSig : iComma;
		ReflectSigTrim(&zPart,&nPart);
		if( nPart > 0 ){
			if( iPos == iWant && pzPart ){
				*pzPart = zPart;
				*pnPart = nPart;
			}
			iPos++;
		}
		if( iComma < 0 ){
			break;
		}
		zSig += iComma + 1;
		nSig -= iComma + 1;
	}
	return iPos;
}
/* One `type &$name = default` part into the uniform description. */
static void ReflectSigDescribe(const char *z, int n, int iPos, ReflectParamDesc *pOut)
{
	const char *zDef = 0;
	int nDef = 0;
	int iEq, iDollar, iSpace;
	SyZero(pOut,sizeof(*pOut));
	pOut->iPos = iPos;
	/* `= default` splits off first: everything after the first unquoted '='. */
	iEq = ReflectSigFindUnquoted(z,n,'=');
	if( iEq >= 0 ){
		zDef = &z[iEq+1];
		nDef = n - iEq - 1;
		ReflectSigTrim(&zDef,&nDef);
		n = iEq;
		ReflectSigTrim(&z,&n);
	}
	if( zDef && nDef == 1 && zDef[0] == '?' ){
		/* `= ?` is the table's OPTIONAL-but-no-default marker, php's own shape
		 * for a parameter like ReflectionClass::getStaticPropertyValue()'s
		 * $default: isOptional() true, isDefaultValueAvailable() FALSE, so
		 * getDefaultValue() raises. Reporting it as a default (which is what
		 * a bare `hasdef` did) made that call answer NULL instead. */
		pOut->bOptional = 1;
		zDef = 0;
		nDef = 0;
	}
	pOut->bVariadic = ReflectSigHas(z,n,"...",3);
	if( pOut->bVariadic ){
		/* php: a variadic parameter never HAS a default -- it defaults to "no
		 * further arguments", which is not a value. Several signature rows still
		 * write `mixed ...$values = ?` (the table's "optional, unspecified"
		 * marker), and honouring it made isDefaultValueAvailable() true where php
		 * says false, so getDefaultValue() then threw on printf/array_merge. */
		zDef = 0;
		nDef = 0;
	}
	iDollar = ReflectSigFindUnquoted(z,n,'$');
	iSpace = ReflectSigFindUnquoted(z,n,' ');
	{
		const char *zName = iDollar < 0 ? z : &z[iDollar+1];
		int nName = iDollar < 0 ? n : n - iDollar - 1;
		SyStringInitFromBuf(&pOut->sName,zName,nName);
	}
	pOut->bByRef = ReflectSigHas(z,n,"&",1);
	pOut->bHasDef = zDef != 0;
	pOut->bOptional = pOut->bOptional || pOut->bVariadic || zDef != 0;
	if( iSpace >= 0 && iDollar >= 0 && iSpace < iDollar ){
		/* The type is whatever precedes the first space, so `?DOMNode $child`
		 * types as `?DOMNode` and an untyped `$x` types as nothing. */
		pOut->bNullable = (z[0] == '?' || ReflectSigHasNoCase(z,iSpace,"null",4));
		SyStringInitFromBuf(&pOut->sType,z,iSpace);
	}
	if( zDef ){
		SyStringInitFromBuf(&pOut->sDefText,zDef,nDef);
	}
}
/*
 * Resolve a Generator object into its wrapper. Mirrors the static
 * VmGeneratorExtractCtx in vm.c: the $__ctx attribute carries the
 * ph7_generator pointer as a resource value.
 */
static ph7_generator * ReflectGeneratorCtx(ph7_vm *pVm, ph7_value *pVal)
{
	ph7_class_instance *pThis;
	ph7_value *pAttr;
	SyString sAttr;
	if( (pVal->iFlags & MEMOBJ_OBJ) == 0 || pVm->pGeneratorClass == 0 ){
		return 0;
	}
	pThis = (ph7_class_instance *)pVal->x.pOther;
	if( pThis->pClass != pVm->pGeneratorClass ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr, "__ctx", 5);
	pAttr = PH7_ClassInstanceFetchAttr(pThis, &sAttr);
	if( pAttr == 0 || (pAttr->iFlags & MEMOBJ_RES) == 0 ){
		return 0;
	}
	return (ph7_generator *)pAttr->x.pOther;
}
/*
 * Evaluate the recorded argument expressions of one declared attribute and
 * answer them as a PHP array, or NULL when the target no longer resolves.
 *
 * apArg is the four-part SPEC a ReflectionAttribute carries — kind 'class'
 * (target = class), 'attr' (class + property/constant name), 'method' (class +
 * method), 'fn' (function name or Closure), 'param' (function spec + parameter
 * index), 'const' (global constant name) — plus which attribute of that target.
 * Named arguments become string keys.
 *
 * The values are evaluated HERE rather than when the reflector was built:
 * `#[Attr(self::SOME)]` runs php code, and php runs it at getArguments() time.
 */
static ph7_value * ReflectAttrArgs(ph7_context *pCtx, ph7_value **apArg, sxu32 nAttrIdx)
{
	ph7_vm *pVm = pCtx->pVm;
	SySet *pAttrs = 0;
	ph7_class *pDeclCls = 0; /* class an attribute is declared on: scope for self:: in its args */
	ph7_attribute *pAttrRec;
	ph7_value *pOut;
	const char *zKind;
	int nKind;
	sxu32 n;
	zKind = ph7_value_to_string(apArg[0], &nKind);
	if( nKind == 5 && SyMemcmp(zKind, "class", 5) == 0 ){
		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);
		if( pClass ){ pAttrs = &pClass->aAttrs; pDeclCls = pClass; }
	}else if( nKind == 4 && SyMemcmp(zKind, "attr", 4) == 0 ){
		ph7_class *pClass = ReflectResolveClass(pVm, apArg[1]);
		ph7_class_attr *pMember = pClass ? ReflectFetchMember(pClass, apArg[2]) : 0;
		if( pMember ){ pAttrs = &pMember->aAttrs; pDeclCls = pClass; }
	}else if( nKind == 6 && SyMemcmp(zKind, "method", 6) == 0 ){
		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);
		if( pFunc ){ pAttrs = &pFunc->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }
	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){
		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], 0, 0, 0, 0, 0);
		if( pFunc ){ pAttrs = &pFunc->aAttrs; }
	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){
		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx, apArg[1], apArg[2], 0, 0, 0, 0);
		ph7_vm_func_arg *pParam = pFunc
			? (ph7_vm_func_arg *)SySetAt(&pFunc->aArgs, (sxu32)ph7_value_to_int(apArg[3])) : 0;
		if( pParam ){ pAttrs = &pParam->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }
	}else if( nKind == 5 && SyMemcmp(zKind, "const", 5) == 0 ){
		/* Global constant (php 8.5 attributes on `const` statements) */
		const char *zCName;
		int nCName;
		SyHashEntry *pCEntry;
		zCName = ph7_value_to_string(apArg[1], &nCName);
		pCEntry = nCName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zCName, (sxu32)nCName) : 0;
		if( pCEntry ){ pAttrs = &((ph7_constant *)pCEntry->pUserData)->aAttrs; }
	}
	if( pAttrs == 0 || (pAttrRec = (ph7_attribute *)SySetAt(pAttrs, nAttrIdx)) == 0
	 || (pOut = ph7_context_new_array(pCtx)) == 0 ){
		return 0;
	}
	for( n = 0 ; n < SySetUsed(&pAttrRec->aArgs) ; n++ ){
		ph7_attr_arg *pArgRec = (ph7_attr_arg *)SySetAt(&pAttrRec->aArgs, n);
		ph7_value sValue;
		PH7_MemObjInit(pVm, &sValue);
		if( SySetUsed(&pArgRec->aByteCode) > 0 ){
			/* Evaluate under the attribute's declaring-class scope so `self::class`
			 * / `self::CONST` / `parent::` resolve like php (else self:: would bind
			 * to the reflection machinery's own class). */
			PH7_VmExecAttrArg(pVm, &pArgRec->aByteCode, pDeclCls, &sValue);
		}
		if( SyStringLength(&pArgRec->sName) > 0 ){
			ReflectMapAddDyn(pCtx, pOut, &pArgRec->sName, &sValue);
		}else{
			ph7_array_add_elem(pOut, 0, &sValue);
		}
		PH7_MemObjRelease(&sValue);
	}
	return pOut;
}
/*
 * ---------------------------------------------------------------------------
 * The ReflectionType family.
 *
 * php's four type objects are pure VALUES: a text, a nullability flag, and for
 * the composites a list of members. Nothing in userland can build one — php
 * declares no constructor on any of them and refuses `clone` — so the prelude's
 * public `__construct($name, $nullable, $text)` existed only because the factory
 * that fills them was itself PHP. The factory is C now (ReflectMakeType), so the
 * constructors are gone and the classes say what php's say.
 * ---------------------------------------------------------------------------
 */
#define RT_TEXT     "__text"
#define RT_NULLABLE "__nullable"
#define RT_TNAME    "__tname"
#define RT_TYPES    "__types"

static int vm_builtin_ReflectionType_allowsNull(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx, pThis != 0 && PH7_NativeAttrTruthy(pThis, RT_NULLABLE));
	return PH7_OK;
}
static int vm_builtin_ReflectionType_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zText = "";
	int nText = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis, RT_TEXT, &zText, &nText);
	}
	ph7_result_string(pCtx, zText, nText);
	return PH7_OK;
}
static int vm_builtin_ReflectionNamedType_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = "";
	int nName = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis, RT_TNAME, &zName, &nName);
	}
	ph7_result_string(pCtx, zName, nName);
	return PH7_OK;
}
/* php's builtin-type set, case-insensitively. Anything else is a class name. */
static int ReflectTypeIsBuiltin(const char *zName, int nName)
{
	static const char *azBuiltin[] = {
		"int","float","string","bool","array","object","mixed",
		"void","never","null","callable","iterable","true","false"
	};
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(azBuiltin) ; n++ ){
		int nWant = (int)SyStrlen(azBuiltin[n]);
		int k;
		if( nWant != nName ){
			continue;
		}
		for( k = 0 ; k < nWant ; k++ ){
			if( SyToLower(zName[k]) != azBuiltin[n][k] ){
				break;
			}
		}
		if( k == nWant ){
			return 1;
		}
	}
	return 0;
}
static int vm_builtin_ReflectionNamedType_isBuiltin(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = "";
	int nName = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis, RT_TNAME, &zName, &nName);
	}
	ph7_result_bool(pCtx, ReflectTypeIsBuiltin(zName, nName));
	return PH7_OK;
}
static int vm_builtin_ReflectionType_getTypes(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pTypes = pThis ? PH7_NativeAttr(pThis, RT_TYPES) : 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pTypes && (pTypes->iFlags & MEMOBJ_HASHMAP) ){
		ph7_result_value(pCtx, pTypes);
	}else{
		/* The declared default is a native NULL slot (a spec cannot carry an
		 * array literal), but php's getTypes() always answers a list. */
		ph7_value *pEmpty = ph7_context_new_array(pCtx);
		if( pEmpty ){
			ph7_result_value(pCtx, pEmpty);
		}
	}
	return PH7_OK;
}
/* Build one of the three concrete types with its slots filled. The caller owns
 * the reference (PH7_NativeResultObject / a list insert drops it). */
static ph7_class_instance * ReflectNewType(ph7_context *pCtx, const char *zClass,
	const char *zText, int nText, int bNullable)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);
	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;
	if( pObj == 0 ){
		return 0;
	}
	PH7_NativeSetAttrStr(pVm, pObj, RT_TEXT, zText, nText);
	PH7_NativeSetAttrBool(pVm, pObj, RT_NULLABLE, bNullable);
	return pObj;
}
/*
 * Append pType to a composite's member list, handing over the caller's reference.
 *
 * The carrier is a STACK value, deliberately: a ph7_context_new_scalar() is
 * released with the call context, and releasing a MEMOBJ_OBJ carrier unrefs the
 * instance a second time — which freed every member of a union or intersection
 * the moment its factory returned, so the list came back holding dead objects.
 * PH7_NativeResultObject hands an instance over the same way.
 */
static void ReflectTypeListAdd(ph7_context *pCtx, ph7_value *pList, ph7_class_instance *pType)
{
	ph7_value sVal;
	if( pType == 0 ){
		return;
	}
	PH7_MemObjInit(pCtx->pVm, &sVal);
	sVal.x.pOther = pType;
	sVal.iFlags = MEMOBJ_OBJ;
	ph7_array_add_elem(pList, 0, &sVal);   /* takes its own reference */
	PH7_ClassInstanceUnref(pType);
}
/* Exact, case-insensitive name test (php lower-cases before comparing). */
static int ReflectTypeNameIs(const char *z, int n, const char *zWant)
{
	int nWant = (int)SyStrlen(zWant), k;
	if( n != nWant ){
		return 0;
	}
	for( k = 0 ; k < n ; k++ ){
		if( SyToLower(z[k]) != zWant[k] ){
			return 0;
		}
	}
	return 1;
}
/*
 * A ReflectionNamedType for one name.
 *
 * bQMark says the text carried a leading '?', which is the ONLY thing that puts
 * one back on the rendered text — while `null` and `mixed` are nullable by their
 * own meaning without ever rendering a '?'. Those two rules are independent, and
 * conflating them is how `?array` came out as `array`.
 */
static ph7_class_instance * ReflectNewNamed(ph7_context *pCtx, const char *z, int n, int bQMark)
{
	ph7_class_instance *pObj;
	char zBuf[256];
	const char *zText = z;
	int nText = n;
	int bNullable = bQMark || ReflectTypeNameIs(z, n, "null") || ReflectTypeNameIs(z, n, "mixed");
	if( bQMark && n + 1 < (int)sizeof(zBuf) ){
		zBuf[0] = '?';
		SyMemcpy(z, &zBuf[1], (sxu32)n);
		zText = zBuf;
		nText = n + 1;
	}
	pObj = ReflectNewType(pCtx, "ReflectionNamedType", zText, nText, bNullable);
	if( pObj ){
		PH7_NativeSetAttrStr(pCtx->pVm, pObj, RT_TNAME, z, n);
	}
	return pObj;
}
/*
 * One ATOM of a type text: `?X`, `(A&B)` or a plain name. An intersection is
 * the only composite an atom can be, because php only nests that way.
 */
static ph7_class_instance * ReflectMakeAtom(ph7_context *pCtx, const char *z, int n)
{
	int bQMark = 0;
	if( n > 0 && z[0] == '?' ){
		bQMark = 1;
		z++;
		n--;
	}
	if( n > 1 && z[0] == '(' && z[n-1] == ')' ){
		z++;
		n -= 2;
	}
	if( ReflectSigFindUnquoted(z, n, '&') >= 0 ){
		ph7_class_instance *pObj = ReflectNewType(pCtx, "ReflectionIntersectionType", z, n, 0);
		ph7_value *pList = ph7_context_new_array(pCtx);
		const char *zCur = z;
		int nCur = n;
		if( pObj == 0 || pList == 0 ){
			return pObj;
		}
		while( nCur > 0 ){
			int iCut = ReflectSigFindUnquoted(zCur, nCur, '&');
			ReflectTypeListAdd(pCtx, pList,
				ReflectNewNamed(pCtx, zCur, iCut < 0 ? nCur : iCut, 0));
			if( iCut < 0 ){
				break;
			}
			zCur += iCut + 1;
			nCur -= iCut + 1;
		}
		PH7_NativeSetProp(pCtx->pVm, pObj, RT_TYPES, sizeof(RT_TYPES)-1, pList);
		return pObj;
	}
	return ReflectNewNamed(pCtx, z, n, bQMark);
}
/*
 * A declared type TEXT to the object php answers for it: a named type, a union,
 * or an intersection. NULL for an absent type. `X|null` collapses back to a
 * NULLABLE named type, which is what php reports (`?X`), but only when exactly
 * one non-null arm is left and it is not itself an intersection.
 */
static ph7_class_instance * ReflectMakeType(ph7_context *pCtx, const char *zText, int nText)
{
	const char *zBody = zText;
	int nBody = nText;
	int bNullable = 0, bHasNull = 0, nParts = 0, nNonNull = 0;
	const char *zLastNonNull = 0;
	int nLastNonNull = 0;
	const char *zCur;
	int nCur, iDepth, k, iStart;
	if( nText < 1 ){
		return 0;
	}
	if( zBody[0] == '?' ){
		bNullable = 1;
		zBody++;
		nBody--;
	}
	/* Split on the TOP-LEVEL '|' only: `A|(B&C)` has one at depth 0 and none
	 * inside the parentheses. */
	iDepth = 0;
	iStart = 0;
	for( k = 0 ; k <= nBody ; k++ ){
		if( k < nBody && zBody[k] == '(' ){
			iDepth++;
			continue;
		}
		if( k < nBody && zBody[k] == ')' ){
			iDepth--;
			continue;
		}
		if( k == nBody || (zBody[k] == '|' && iDepth == 0) ){
			zCur = &zBody[iStart];
			nCur = k - iStart;
			nParts++;
			if( nCur == 4 && ReflectSigHasNoCase(zCur, 4, "null", 4) ){
				bHasNull = 1;
			}else{
				nNonNull++;
				zLastNonNull = zCur;
				nLastNonNull = nCur;
			}
			iStart = k + 1;
		}
	}
	if( nParts > 1 ){
		ph7_class_instance *pObj;
		ph7_value *pList;
		if( bHasNull && nNonNull == 1
		 && ReflectSigFindUnquoted(zLastNonNull, nLastNonNull, '&') < 0 ){
			/* `X|null` IS `?X` to php. */
			char zBuf[256];
			ph7_class_instance *pNamed;
			const char *zRender = zLastNonNull;
			int nRender = nLastNonNull;
			if( nLastNonNull + 1 < (int)sizeof(zBuf) ){
				zBuf[0] = '?';
				SyMemcpy(zLastNonNull, &zBuf[1], (sxu32)nLastNonNull);
				zRender = zBuf;
				nRender = nLastNonNull + 1;
			}
			pNamed = ReflectNewType(pCtx, "ReflectionNamedType", zRender, nRender, 1);
			if( pNamed ){
				PH7_NativeSetAttrStr(pCtx->pVm, pNamed, RT_TNAME, zLastNonNull, nLastNonNull);
			}
			return pNamed;
		}
		pObj = ReflectNewType(pCtx, "ReflectionUnionType", zBody, nBody, bNullable || bHasNull);
		pList = ph7_context_new_array(pCtx);
		if( pObj == 0 || pList == 0 ){
			return pObj;
		}
		iDepth = 0;
		iStart = 0;
		for( k = 0 ; k <= nBody ; k++ ){
			if( k < nBody && zBody[k] == '(' ){
				iDepth++;
				continue;
			}
			if( k < nBody && zBody[k] == ')' ){
				iDepth--;
				continue;
			}
			if( k == nBody || (zBody[k] == '|' && iDepth == 0) ){
				ReflectTypeListAdd(pCtx, pList,
					ReflectMakeAtom(pCtx, &zBody[iStart], k - iStart));
				iStart = k + 1;
			}
		}
		PH7_NativeSetProp(pCtx->pVm, pObj, RT_TYPES, sizeof(RT_TYPES)-1, pList);
		return pObj;
	}
	if( ReflectSigFindUnquoted(zBody, nBody, '&') >= 0 ){
		return ReflectMakeAtom(pCtx, zBody, nBody);
	}
	if( bNullable ){
		char zBuf[256];
		if( nBody + 1 < (int)sizeof(zBuf) ){
			zBuf[0] = '?';
			SyMemcpy(zBody, &zBuf[1], (sxu32)nBody);
			return ReflectMakeAtom(pCtx, zBuf, nBody + 1);
		}
	}
	return ReflectMakeAtom(pCtx, zBody, nBody);
}
/*
 * Declare the four type classes. Called from PH7_VmInstallReflectionLib where
 * chunk 4 used to be compiled, so `Stringable` (a core interface) already
 * exists. PH7_CLASS_NOCLONE is php's own rule for these: they are values the
 * engine hands out, and `clone $type` is an Error, not a copy.
 */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionTypes(ph7_vm *pVm)
{
	static const PH7_NativePropDef aBaseProp[] = {
		{ RT_TEXT,     PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 } },
		{ RT_NULLABLE, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 } },
	};
	static const PH7_NativeMethodDef aBaseMethod[] = {
		{ "allowsNull", PH7_MOD_PUBLIC, "", "",       vm_builtin_ReflectionType_allowsNull },
		{ "__toString", PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionType_toString },
	};
	static const PH7_NativePropDef aNamedProp[] = {
		{ RT_TNAME, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 } },
	};
	static const PH7_NativeMethodDef aNamedMethod[] = {
		{ "getName",   PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionNamedType_getName },
		{ "isBuiltin", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionNamedType_isBuiltin },
	};
	static const PH7_NativePropDef aCompProp[] = {
		{ RT_TYPES, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
	};
	static const PH7_NativeMethodDef aCompMethod[] = {
		{ "getTypes", PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionType_getTypes },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "ReflectionType", 0, "Stringable", PH7_CLASS_ABSTRACT|PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aBaseMethod, SX_ARRAYSIZE(aBaseMethod), 0, 0, aBaseProp, SX_ARRAYSIZE(aBaseProp), 0, 0 },
		{ "ReflectionNamedType", "ReflectionType", 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aNamedMethod, SX_ARRAYSIZE(aNamedMethod), 0, 0, aNamedProp, SX_ARRAYSIZE(aNamedProp), 0, 0 },
		{ "ReflectionUnionType", "ReflectionType", 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aCompMethod, SX_ARRAYSIZE(aCompMethod), 0, 0, aCompProp, SX_ARRAYSIZE(aCompProp), 0, 0 },
		{ "ReflectionIntersectionType", "ReflectionType", 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aCompMethod, SX_ARRAYSIZE(aCompMethod), 0, 0, aCompProp, SX_ARRAYSIZE(aCompProp), 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));
}
/*
 * ---------------------------------------------------------------------------
 * The six standalone reflection classes.
 *
 * ReflectionGenerator, ReflectionFiber, ReflectionConstant, ReflectionExtension,
 * ReflectionZendExtension and ReflectionReference reflect ENGINE state rather
 * than a class hierarchy, so each was a prelude class over one or two thunks
 * whose only job was to hand that state to PHP. Their bodies are the C now, and
 * the four thunks that existed for them (__reflect_gen_info, __reflect_gen_exec,
 * __reflect_const_info, __reflect_ref_id) are gone with them.
 *
 * The ReflectionEnum family stays in the prelude: it extends ReflectionClass and
 * ReflectionClassConstant, which are still PHP.
 * ---------------------------------------------------------------------------
 */
#define RG_GEN   "__gen"
#define RF_FIBER "__fiber"
#define RR_ID    "__id"

/* Build an instance of a class that may still be PRELUDE PHP, running its
 * constructor. Answers 0 when the constructor threw (rc says which). */
static ph7_class_instance * ReflectConstruct(ph7_context *pCtx, const char *zClass,
	int nArg, ph7_value **apArg, sxi32 *pRc)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);
	ph7_class_instance *pThis;
	ph7_class_method *pCons;
	*pRc = PH7_OK;
	if( pClass == 0 ){
		return 0;
	}
	pThis = PH7_NewClassInstance(pVm, pClass);
	if( pThis == 0 ){
		return 0;
	}
	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);
	if( pCons ){
		sxi32 rc = PH7_VmCallClassMethod(pVm, pThis, pCons, 0, nArg, apArg);
		if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){
			PH7_ClassInstanceUnref(pThis);
			*pRc = rc;
			return 0;
		}
	}
	return pThis;
}
/* The instance a native reflector wraps ($this->__gen / $this->__fiber). */
static ph7_class_instance * ReflectWrapped(ph7_context *pCtx, const char *zSlot)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	return pThis ? PH7_NativeAttrObj(pThis, zSlot) : 0;
}
/* Hand back a wrapped instance the receiver already owns (a borrowed reference). */
static int ReflectResultBorrowed(ph7_context *pCtx, ph7_class_instance *pObj)
{
	ph7_value sVal;
	if( pObj == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjInit(pCtx->pVm, &sVal);
	sVal.x.pOther = pObj;
	sVal.iFlags = MEMOBJ_OBJ;
	ph7_result_value(pCtx, &sVal);   /* takes its own reference */
	return PH7_OK;
}
/*
 * ---------------------------------------------------------------------------
 * ReflectionAttribute — chunk 7.
 *
 * php's own class, plus the shared getAttributes() body that produces it. The
 * chunk it replaces also carried __reflect_target_names (folded into the
 * "cannot target" diagnostic below) and __reflect_has_deprecated, which had
 * already lost its last caller when isDeprecated() became C.
 *
 * An instance holds a SPEC, not the arguments: [kind, target, member,
 * paramIdx] names something the engine can reopen, and getArguments()
 * evaluates the recorded expressions on every call, because php does too --
 * `#[A(self::X)]` is php code and it runs when it is asked for. That is also
 * why the prelude needed a public __init(): PHP could not fill a fresh object
 * any other way. C fills it directly, so __init() (and the
 * __reflect_new_no_ctor that built the empty shell) are gone with it.
 * ---------------------------------------------------------------------------
 */
#define RA_NAME   "name"      /* php declares this one PUBLIC: `$attr->name` */
#define RA_SPEC   "__spec"    /* [kind, target, member, paramIdx] */
#define RA_IDX    "__idx"     /* which of that target's attributes this is */
#define RA_TARGET "__target"  /* the Attribute::TARGET_* bit it was found on */
#define RA_REP    "__rep"     /* the target carries more than one of this name */

/* Bound on the value exporter's own recursion. An attribute argument cannot be
 * cyclic, but an OBJECT argument's property graph can be. */
#define REFLECT_EXPORT_MAX_DEPTH 31

/* php's Attribute::TARGET_* names, in bit order (TARGET_CLASS is bit 0). */
static const char *const azReflectTarget[] = {
	"class", "function", "method", "property", "class constant", "parameter", "constant"
};

static void ReflectExportValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth);

/*
 * A string the way php's reflection prints one: single-quoted, with the
 * NON-PRINTABLE bytes escaped (php's smart_str_append_escaped). The quote
 * itself is NOT escaped — php's own output for "q'q" is 'q'q'.
 */
static void ReflectExportStr(SyBlob *pOut, const char *zIn, sxu32 nIn)
{
	static const char zHexDigit[] = "0123456789ABCDEF";
	sxu32 i;
	SyBlobAppend(pOut, "'", sizeof(char));
	for( i = 0 ; i < nIn ; i++ ){
		unsigned char c = (unsigned char)zIn[i];
		const char *zEsc = 0;
		switch( c ){
			case 0x09: zEsc = "\\t"; break;
			case 0x0A: zEsc = "\\n"; break;
			case 0x0B: zEsc = "\\v"; break;
			case 0x0C: zEsc = "\\f"; break;
			case 0x0D: zEsc = "\\r"; break;
			case 0x1B: zEsc = "\\e"; break;
			case '\\': zEsc = "\\\\"; break;
			default:   break;
		}
		if( zEsc ){
			SyBlobAppend(pOut, zEsc, sizeof("\\t")-1);
		}else if( c < 0x20 || c > 0x7E ){
			char zHex[4];
			zHex[0] = '\\';
			zHex[1] = 'x';
			zHex[2] = zHexDigit[(c >> 4) & 0x0F];
			zHex[3] = zHexDigit[c & 0x0F];
			SyBlobAppend(pOut, zHex, sizeof(zHex));
		}else{
			SyBlobAppend(pOut, (const char *)&c, sizeof(char));
		}
	}
	SyBlobAppend(pOut, "'", sizeof(char));
}
/*
 * A float the way php's reflection prints one: the plain string cast (which
 * PHL already renders php's way, 1.0E+15 and all), then php's `zero_frac` —
 * a float that came out without a fraction gets ".0" so 1.0 does not print as
 * 1. INF and NAN render as words and are left alone.
 */
static void ReflectExportReal(ph7_vm *pVm, SyBlob *pOut, ph7_real rVal)
{
	ph7_value sTmp;
	const char *zText;
	int nText, i, bPlain = 1;
	PH7_MemObjInitFromReal(pVm, &sTmp, rVal);
	zText = ph7_value_to_string(&sTmp, &nText);
	if( nText > 0 ){
		SyBlobAppend(pOut, zText, (sxu32)nText);
	}
	for( i = 0 ; i < nText ; i++ ){
		if( (zText[i] < '0' || zText[i] > '9') && !(i == 0 && zText[i] == '-') ){
			bPlain = 0;
			break;
		}
	}
	if( bPlain && nText > 0 ){
		SyBlobAppend(pOut, ".0", sizeof(".0")-1);
	}
	PH7_MemObjRelease(&sTmp);
}
/*
 * An array the way php's reflection prints one. The KEY is written only when
 * it is not the next one a list would have produced, which is how php tells
 * [1, 2] from [2 => 'x'] — php's format_default_value keeps a running
 * expected index and prints nothing while the array tracks it.
 */
static void ReflectExportArray(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth)
{
	ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;
	ph7_hashmap_node *pEntry = pMap->pFirst;
	sxi64 iExpect = 0;
	sxu32 n;
	SyBlobAppend(pOut, "[", sizeof(char));
	for( n = 0 ; n < pMap->nEntry && pEntry ; n++ ){
		ph7_value *pMember = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);
		if( n > 0 ){
			SyBlobAppend(pOut, ", ", sizeof(", ")-1);
		}
		if( pEntry->iType == HASHMAP_BLOB_NODE ){
			ReflectExportStr(pOut, (const char *)SyBlobData(&pEntry->xKey.sKey),
				SyBlobLength(&pEntry->xKey.sKey));
			SyBlobAppend(pOut, " => ", sizeof(" => ")-1);
		}else{
			if( pEntry->xKey.iKey != iExpect ){
				SyBlobFormat(pOut, "%qd => ", pEntry->xKey.iKey);
			}
			iExpect = pEntry->xKey.iKey + 1;
		}
		ReflectExportValue(pCtx, pOut, pMember, iDepth + 1);
		pEntry = pEntry->pPrev; /* Reverse link: insertion order */
	}
	SyBlobAppend(pOut, "]", sizeof(char));
}
/*
 * One value in php's reflection export syntax — the text after `= ` in a
 * parameter default and inside `Argument #0 [ … ]` in an attribute dump.
 *
 * The type dispatch is PH7_MemObjDump's, including its REAL-before-INT order:
 * an integer-valued float carries a cached int view too, and php prints 1.0.
 *
 * php renders an OBJECT argument by echoing the source expression it never
 * folded (`new \A(x: 1)`), which needs the AST. PHL evaluates attribute
 * arguments from byte-code and has only the resulting VALUE, so a non-enum
 * object is rebuilt from its own properties: same shape, not always the same
 * bytes. An enum case — the one object php's own value formatter handles —
 * matches exactly.
 */
static void ReflectExportValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth)
{
	if( pVal == 0 || iDepth > REFLECT_EXPORT_MAX_DEPTH ){
		SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);
		return;
	}
	if( (pVal->iFlags & (MEMOBJ_OBJ|MEMOBJ_NULL)) == MEMOBJ_OBJ ){
		ph7_class_instance *pObj = (ph7_class_instance *)pVal->x.pOther;
		if( pObj->pClass->iFlags & PH7_CLASS_ENUM ){
			ph7_value *pName = PH7_EnumCaseNameValue(pObj);
			/* php writes the case as the source did, so a namespaced enum comes
			 * out fully qualified (\N\E::One) and a global one bare (E::One).
			 * The separator is the only thing left of that distinction here. */
			if( SyByteFind(SyStringData(&pObj->pClass->sName),
				SyStringLength(&pObj->pClass->sName), '\\', 0) == SXRET_OK ){
				SyBlobAppend(pOut, "\\", sizeof(char));
			}
			SyBlobFormat(pOut, "%z::", &pObj->pClass->sName);
			if( pName && SyBlobLength(&pName->sBlob) > 0 ){
				SyBlobAppend(pOut, SyBlobData(&pName->sBlob), SyBlobLength(&pName->sBlob));
			}
			return;
		}
		SyBlobFormat(pOut, "new \\%z(", &pObj->pClass->sName);
		{
			SyHashEntry *pEntry;
			int nWritten = 0;
			SyHashResetLoopCursor(&pObj->hAttr);
			while((pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){
				VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
				ph7_value *pSlot;
				if( pVmAttr->pAttr->iFlags
					& (PH7_CLASS_ATTR_CONSTANT|PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_HOOK_VIRTUAL) ){
					continue; /* class-level members are not part of the object */
				}
				pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);
				if( nWritten++ ){
					SyBlobAppend(pOut, ", ", sizeof(", ")-1);
				}
				ReflectExportValue(pCtx, pOut, pSlot, iDepth + 1);
			}
		}
		SyBlobAppend(pOut, ")", sizeof(char));
		return;
	}
	if( pVal->iFlags & MEMOBJ_NULL ){
		SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);
		return;
	}
	if( pVal->iFlags & MEMOBJ_HASHMAP ){
		ReflectExportArray(pCtx, pOut, pVal, iDepth);
		return;
	}
	if( pVal->iFlags & MEMOBJ_BOOL ){
		if( pVal->x.iVal != 0 ){
			SyBlobAppend(pOut, "true", sizeof("true")-1);
		}else{
			SyBlobAppend(pOut, "false", sizeof("false")-1);
		}
		return;
	}
	if( pVal->iFlags & MEMOBJ_REAL ){
		ReflectExportReal(pCtx->pVm, pOut, pVal->rVal);
		return;
	}
	if( pVal->iFlags & MEMOBJ_INT ){
		SyBlobFormat(pOut, "%qd", pVal->x.iVal);
		return;
	}
	if( pVal->iFlags & MEMOBJ_STRING ){
		ReflectExportStr(pOut, (const char *)SyBlobData(&pVal->sBlob), SyBlobLength(&pVal->sBlob));
		return;
	}
	/* A resource, and anything else php has no export syntax for. */
	SyBlobAppend(pOut, "NULL", sizeof("NULL")-1);
}
/*
 * Build one ReflectionAttribute. pSpec is shared by every attribute of the
 * same target — it says how to reopen it, not which one this is.
 */
static ph7_class_instance * ReflectAttrNew(ph7_context *pCtx, SyString *pName,
	ph7_value *pSpec, sxu32 nIdx, int iTargetBit, int bRepeated)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = PH7_VmExtractClass(pVm, "ReflectionAttribute",
		sizeof("ReflectionAttribute")-1, FALSE, 0);
	ph7_class_instance *pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;
	if( pObj == 0 ){
		return 0;
	}
	PH7_NativeSetAttrStr(pVm, pObj, RA_NAME, SyStringData(pName), (int)SyStringLength(pName));
	PH7_NativeSetProp(pVm, pObj, RA_SPEC, sizeof(RA_SPEC)-1, pSpec);
	PH7_NativeSetAttrInt(pVm, pObj, RA_IDX, (sxi64)nIdx);
	PH7_NativeSetAttrInt(pVm, pObj, RA_TARGET, iTargetBit);
	PH7_NativeSetAttrBool(pVm, pObj, RA_REP, bRepeated);
	return pObj;
}
/*
 * Unpack the receiver's spec into the four-value vector ReflectAttrArgs takes.
 * Returns 0 when the object is not one this file built.
 */
static int ReflectAttrSpec(ph7_context *pCtx, ph7_class_instance *pThis, ph7_value **apOut)
{
	ph7_value *pSpec = pThis ? PH7_NativeAttr(pThis, RA_SPEC) : 0;
	ph7_hashmap *pMap;
	int i;
	if( pSpec == 0 || (pSpec->iFlags & MEMOBJ_HASHMAP) == 0 ){
		return 0;
	}
	pMap = (ph7_hashmap *)pSpec->x.pOther;
	for( i = 0 ; i < 4 ; i++ ){
		ph7_value sKey;
		ph7_hashmap_node *pNode = 0;
		sxi32 rc;
		PH7_MemObjInitFromInt(pCtx->pVm, &sKey, i);
		rc = PH7_HashmapLookup(pMap, &sKey, &pNode);
		PH7_MemObjRelease(&sKey);
		if( rc != SXRET_OK || pNode == 0 ){
			return 0;
		}
		apOut[i] = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pNode->nValIdx);
		if( apOut[i] == 0 ){
			return 0;
		}
	}
	return 1;
}
/* The receiver's evaluated arguments, or an empty array when the target no
 * longer resolves (what the chunk's `$a === null ? array() : $a` said). */
static ph7_value * ReflectAttrOwnArgs(ph7_context *pCtx, ph7_class_instance *pThis)
{
	ph7_value *apSpec[4];
	ph7_value *pArgs = 0;
	if( pThis && ReflectAttrSpec(pCtx, pThis, apSpec) ){
		pArgs = ReflectAttrArgs(pCtx, apSpec, (sxu32)PH7_NativeAttrInt(pThis, RA_IDX));
	}
	return pArgs ? pArgs : ph7_context_new_array(pCtx);
}
static int vm_builtin_ReflectionAttribute_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = "";
	int nName = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis, RA_NAME, &zName, &nName);
	}
	ph7_result_string(pCtx, zName, nName);
	return PH7_OK;
}
static int vm_builtin_ReflectionAttribute_getTarget(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx, pThis ? PH7_NativeAttrInt(pThis, RA_TARGET) : 0);
	return PH7_OK;
}
static int vm_builtin_ReflectionAttribute_isRepeated(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx, pThis != 0 && PH7_NativeAttrTruthy(pThis, RA_REP));
	return PH7_OK;
}
static int vm_builtin_ReflectionAttribute_getArguments(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_value *pArgs = ReflectAttrOwnArgs(pCtx, PH7_ContextThis(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pArgs == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	ph7_result_value(pCtx, pArgs);
	return PH7_OK;
}
/*
 * newInstance(): the four screens php runs before it constructs anything —
 * the attribute class exists, it is declared #[Attribute], that declaration
 * allows the target this was found on, and it allows repetition if it was
 * repeated. The messages are php's, byte for byte.
 */
static int vm_builtin_ReflectionAttribute_newInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pNameVal = pThis ? PH7_NativeAttr(pThis, RA_NAME) : 0;
	ph7_class *pClass;
	ph7_attribute *aA;
	ph7_value *pDeclArgs;
	sxu32 n, nDecl = 0;
	int bDecl = 0, iTarget, iBit;
	sxi64 iFlags = 127; /* php's TARGET_ALL: #[Attribute] with no argument */
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pNameVal == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pClass = ReflectResolveClass(pVm, pNameVal);
	if( pClass == 0 ){
		SyString sName;
		SyStringInitFromBuf(&sName, SyBlobData(&pNameVal->sBlob), SyBlobLength(&pNameVal->sBlob));
		return PH7_VmThrowException(pCtx, "Error", "Attribute class \"%z\" not found", &sName);
	}
	/* Which #[...] on the attribute class is its own #[Attribute] declaration */
	aA = (ph7_attribute *)SySetBasePtr(&pClass->aAttrs);
	for( n = 0 ; n < SySetUsed(&pClass->aAttrs) ; n++ ){
		if( SyStringLength(&aA[n].sName) == sizeof("Attribute")-1
		 && SyStrnicmp(SyStringData(&aA[n].sName), "Attribute", sizeof("Attribute")-1) == 0 ){
			nDecl = n;
			bDecl = 1;
			break;
		}
	}
	if( !bDecl ){
		return PH7_VmThrowException(pCtx, "Error",
			"Attempting to use non-attribute class \"%z\" as attribute", &pClass->sName);
	}
	/* Its argument is the target mask: positional, or named `flags:`. */
	{
		ph7_value *apSpec[4];
		ph7_value *pKind = ph7_context_new_scalar(pCtx);
		ph7_value *pMem  = ph7_context_new_scalar(pCtx);
		ph7_value *pIdx  = ph7_context_new_scalar(pCtx);
		if( pKind == 0 || pMem == 0 || pIdx == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		ph7_value_string(pKind, "class", sizeof("class")-1);
		ph7_value_null(pMem);
		ph7_value_int(pIdx, 0);
		apSpec[0] = pKind;
		apSpec[1] = pNameVal;
		apSpec[2] = pMem;
		apSpec[3] = pIdx;
		pDeclArgs = ReflectAttrArgs(pCtx, apSpec, nDecl);
	}
	if( pDeclArgs ){
		ph7_value *pFlags = ph7_array_fetch(pDeclArgs, "0", -1);
		if( pFlags == 0 ){
			pFlags = ph7_array_fetch(pDeclArgs, "flags", -1);
		}
		if( pFlags ){
			iFlags = ph7_value_to_int64(pFlags);
		}
	}
	iTarget = (int)PH7_NativeAttrInt(pThis, RA_TARGET);
	if( (iFlags & iTarget) == 0 ){
		SyBlob sAllowed;
		sxi32 rc;
		SyBlobInit(&sAllowed, &pVm->sAllocator);
		for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azReflectTarget) ; iBit++ ){
			if( (iFlags & ((sxi64)1 << iBit)) == 0 ){
				continue;
			}
			if( SyBlobLength(&sAllowed) > 0 ){
				SyBlobAppend(&sAllowed, ", ", sizeof(", ")-1);
			}
			SyBlobAppend(&sAllowed, azReflectTarget[iBit],
				(sxu32)SyStrlen(azReflectTarget[iBit]));
		}
		SyBlobAppend(&sAllowed, "", sizeof(char)); /* NUL for the %s below */
		for( iBit = 0 ; iBit < (int)SX_ARRAYSIZE(azReflectTarget) ; iBit++ ){
			if( iTarget == (1 << iBit) ){
				break;
			}
		}
		rc = PH7_VmThrowException(pCtx, "Error",
			"Attribute \"%z\" cannot target %s (allowed targets: %s)", &pClass->sName,
			iBit < (int)SX_ARRAYSIZE(azReflectTarget) ? azReflectTarget[iBit] : "",
			SyBlobData(&sAllowed));
		SyBlobRelease(&sAllowed);
		return rc;
	}
	if( PH7_NativeAttrTruthy(pThis, RA_REP) && (iFlags & 128) == 0 ){
		return PH7_VmThrowException(pCtx, "Error",
			"Attribute \"%z\" must not be repeated", &pClass->sName);
	}
	return ReflectAttrInstantiate(pCtx, pNameVal, ReflectAttrOwnArgs(pCtx, pThis));
}
/*
 * php's export text. A bare `Attribute [ Name ]` when there are no arguments;
 * otherwise the same head followed by the argument block, each argument
 * rendered in php's value-export syntax and named ones as `name = value`.
 */
static int vm_builtin_ReflectionAttribute_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pArgs = ReflectAttrOwnArgs(pCtx, pThis);
	const char *zName = "";
	int nName = 0;
	SyBlob sOut;
	sxu32 nCount = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis, RA_NAME, &zName, &nName);
	}
	if( pArgs && (pArgs->iFlags & MEMOBJ_HASHMAP) ){
		nCount = ((ph7_hashmap *)pArgs->x.pOther)->nEntry;
	}
	SyBlobInit(&sOut, &pVm->sAllocator);
	SyBlobAppend(&sOut, "Attribute [ ", sizeof("Attribute [ ")-1);
	if( nName > 0 ){
		SyBlobAppend(&sOut, zName, (sxu32)nName);
	}
	SyBlobAppend(&sOut, " ]", sizeof(" ]")-1);
	if( nCount < 1 ){
		SyBlobAppend(&sOut, "\n", sizeof(char));
	}else{
		ph7_hashmap *pMap = (ph7_hashmap *)pArgs->x.pOther;
		ph7_hashmap_node *pEntry = pMap->pFirst;
		sxu32 n;
		SyBlobFormat(&sOut, " {\n  - Arguments [%u] {\n", nCount);
		for( n = 0 ; n < nCount && pEntry ; n++ ){
			ph7_value *pMember = (ph7_value *)SySetAt(&pVm->aMemObj, pEntry->nValIdx);
			SyBlobFormat(&sOut, "    Argument #%u [ ", n);
			if( pEntry->iType == HASHMAP_BLOB_NODE ){
				SyBlobAppend(&sOut, SyBlobData(&pEntry->xKey.sKey),
					SyBlobLength(&pEntry->xKey.sKey));
				SyBlobAppend(&sOut, " = ", sizeof(" = ")-1);
			}
			ReflectExportValue(pCtx, &sOut, pMember, 0);
			SyBlobAppend(&sOut, " ]\n", sizeof(" ]\n")-1);
			pEntry = pEntry->pPrev; /* Reverse link: insertion order */
		}
		SyBlobAppend(&sOut, "  }\n}\n", sizeof("  }\n}\n")-1);
	}
	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/* The body of the two methods php declares PRIVATE and never calls. Neither is
 * reachable from php code: `new ReflectionAttribute` is the engine's own "Call
 * to private … from global scope" Error, and `clone` is refused by
 * PH7_CLASS_NOCLONE before any body runs. They exist so the class REPORTS them,
 * which is the only way php's own dump shows them too. */
static int vm_builtin_ReflectionAttribute_private(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	SXUNUSED(pCtx);
	return PH7_OK;
}
/*
 * Declare ReflectionAttribute. Called from PH7_VmInstallReflectionLib where
 * chunk 7 used to be compiled, so Reflector (chunk 1) already exists.
 *
 * PH7_CLASS_NOCLONE is php's rule for it (php declares __clone private AND
 * refuses the copy), and the class is NOT final — php 8.5 unsealed it.
 */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionAttribute(ph7_vm *pVm)
{
	static const PH7_NativeConstDef aConst[] = {
		{ "IS_INSTANCEOF", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2, 0, 0.0 },
	};
	static const PH7_NativePropDef aProp[] = {
		{ RA_NAME,   PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 } },
		/* PHL-only, and PROTECTED so php code cannot reach them: the spec that
		 * reopens the target. php holds the same state on the C struct behind the
		 * object, invisible; PHL has no hidden-slot bit yet (§7.4 (e)), so these
		 * four still show up in a var_dump where php shows only $name. */
		{ RA_SPEC,   PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0,  0.0 } },
		{ RA_IDX,    PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0,  0.0 } },
		{ RA_TARGET, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0,  0.0 } },
		{ RA_REP,    PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0,  0.0 } },
	};
	/* php's own listing order, which is what __toString() prints. */
	static const PH7_NativeMethodDef aMethod[] = {
		{ "getName",      PH7_MOD_PUBLIC,  "", "string", vm_builtin_ReflectionAttribute_getName },
		{ "getTarget",    PH7_MOD_PUBLIC,  "", "int",    vm_builtin_ReflectionAttribute_getTarget },
		{ "isRepeated",   PH7_MOD_PUBLIC,  "", "bool",   vm_builtin_ReflectionAttribute_isRepeated },
		{ "getArguments", PH7_MOD_PUBLIC,  "", "array",  vm_builtin_ReflectionAttribute_getArguments },
		{ "newInstance",  PH7_MOD_PUBLIC,  "", "object", vm_builtin_ReflectionAttribute_newInstance },
		{ "__toString",   PH7_MOD_PUBLIC,  "", "string", vm_builtin_ReflectionAttribute_toString },
		{ "__clone",      PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionAttribute_private },
		{ "__construct",  PH7_MOD_PRIVATE, "", 0,      vm_builtin_ReflectionAttribute_private },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "ReflectionAttribute", 0, "Reflector", PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aMethod, SX_ARRAYSIZE(aMethod), aConst, SX_ARRAYSIZE(aConst),
		  aProp, SX_ARRAYSIZE(aProp), 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));
}
/*
 * The shared getAttributes() body.
 *
 * Turns the target's #[...] records into ReflectionAttribute objects, applying
 * php's name / IS_INSTANCEOF filter. Argument VALUES stay lazy — what each
 * object carries is the [kind, target, member, paramIdx] spec that reopens
 * them — so a reflector declaring getAttributes() only has to say WHICH target
 * it is.
 */
static int ReflectBuildAttrs(ph7_context *pCtx, SySet *pAttrs, const char *zKind,
	ph7_value *pTarget, const char *zMember, int nMember,
	int iParamIdx, int iTargetBit, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);
	ph7_class *pFilter = 0;
	const char *zFilter = 0;
	int nFilter = 0;
	ph7_value *pSpec, *pOut;
	sxu32 n, i;
	pOut = ph7_context_new_array(pCtx);
	pSpec = ph7_context_new_array(pCtx);
	if( pOut == 0 || pSpec == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( nArg > 0 && !ph7_value_is_null(apArg[0]) ){
		zFilter = ph7_value_to_string(apArg[0], &nFilter);
		if( nFilter < 1 ){
			zFilter = 0;
		}
		/* IS_INSTANCEOF: keep a SUBCLASS of the named attribute too. The exact
		 * name still matches on its own, so this only has to answer for the rest. */
		if( zFilter && nArg > 1 && (ph7_value_to_int(apArg[1]) & 2) ){
			pFilter = ReflectResolveClass(pVm, apArg[0]);
		}
	}
	{
		ph7_value *pKind = ph7_context_new_scalar(pCtx);
		ph7_value *pMem  = ph7_context_new_scalar(pCtx);
		ph7_value *pIdx  = ph7_context_new_scalar(pCtx);
		if( pKind == 0 || pMem == 0 || pIdx == 0 ){
			return PH7_ContextMemoryError(pCtx);
		}
		ph7_value_string(pKind, zKind, -1);
		if( zMember ){
			ph7_value_string(pMem, zMember, nMember);
		}else{
			ph7_value_null(pMem);
		}
		ph7_value_int(pIdx, iParamIdx);
		ph7_array_add_elem(pSpec, 0, pKind);
		/* The target rides as a VALUE, not a name: a closure's attributes are
		 * reopened through the Closure object itself. */
		ph7_array_add_elem(pSpec, 0, pTarget);
		ph7_array_add_elem(pSpec, 0, pMem);
		ph7_array_add_elem(pSpec, 0, pIdx);
	}
	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){
		int bRepeated = 0;
		if( zFilter ){
			int bKeep = ((int)SyStringLength(&aA[n].sName) == nFilter
				&& SyStrnicmp(SyStringData(&aA[n].sName), zFilter, (sxu32)nFilter) == 0);
			if( !bKeep && pFilter ){
				ph7_class *pCand = PH7_VmExtractClass(pVm, SyStringData(&aA[n].sName),
					SyStringLength(&aA[n].sName), FALSE, 0);
				if( pCand == 0 ){
					pCand = PH7_VmTriggerAutoload(pVm, SyStringData(&aA[n].sName),
						SyStringLength(&aA[n].sName), FALSE);
				}
				bKeep = pCand != 0 && pCand != pFilter && PH7_VmInstanceOf(pCand, pFilter);
			}
			if( !bKeep ){
				continue;
			}
		}
		/* isRepeated() asks about the TARGET, not about the filtered result:
		 * two #[A] make both of them repeated even when only one is asked for. */
		for( i = 0 ; i < SySetUsed(pAttrs) ; i++ ){
			if( i != n && SyStringLength(&aA[i].sName) == SyStringLength(&aA[n].sName)
			 && SyStrnicmp(SyStringData(&aA[i].sName), SyStringData(&aA[n].sName),
				SyStringLength(&aA[n].sName)) == 0 ){
				bRepeated = 1;
				break;
			}
		}
		ReflectTypeListAdd(pCtx, pOut,
			ReflectAttrNew(pCtx, &aA[n].sName, pSpec, n, iTargetBit, bRepeated));
	}
	ph7_result_value(pCtx, pOut);
	return PH7_OK;
}
/*
 * The three "no runtime line tracking" methods. php answers a file/line/trace
 * from the executing frame; PHL has no per-instruction line record (the same
 * gap debug_backtrace() has), so it says so loudly rather than inventing one.
 */
static int ReflectUnsupported(ph7_context *pCtx, const char *zWho)
{
	return PH7_VmThrowException(pCtx, "Error",
		"%s is not supported by PHL (no runtime line tracking)", zWho);
}
#define REFLECT_UNSUPPORTED(NAME,TEXT) \
	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \
	{ \
		SXUNUSED(nArg); \
		SXUNUSED(apArg); \
		return ReflectUnsupported(pCtx, TEXT); \
	}
REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_execLine,"ReflectionGenerator::getExecutingLine()")
REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_execFile,"ReflectionGenerator::getExecutingFile()")
REFLECT_UNSUPPORTED(vm_builtin_ReflectionGenerator_trace,"ReflectionGenerator::getTrace()")
REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_execLine,"ReflectionFiber::getExecutingLine()")
REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_execFile,"ReflectionFiber::getExecutingFile()")
REFLECT_UNSUPPORTED(vm_builtin_ReflectionFiber_trace,"ReflectionFiber::getTrace()")

/* ReflectionGenerator::__construct(Generator $generator) */
static int vm_builtin_ReflectionGenerator_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	if( pThis == 0 || nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_OK;
	}
	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RG_GEN, (ph7_class_instance *)apArg[0]->x.pOther);
	return PH7_OK;
}
/* The coroutine context of the wrapped generator, or NULL. */
static ph7_exec_ctx * ReflectGenExec(ph7_context *pCtx)
{
	ph7_class_instance *pGenObj = ReflectWrapped(pCtx, RG_GEN);
	ph7_value sVal;
	ph7_generator *pGen;
	if( pGenObj == 0 ){
		return 0;
	}
	PH7_MemObjInit(pCtx->pVm, &sVal);
	sVal.x.pOther = pGenObj;
	sVal.iFlags = MEMOBJ_OBJ;
	pGen = ReflectGeneratorCtx(pCtx->pVm, &sVal);
	return pGen ? pGen->pCtx : 0;
}
static int vm_builtin_ReflectionGenerator_isClosed(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx, pExec != 0
		&& (pExec->iState == PH7_CTX_STATE_COMPLETED || pExec->iState == PH7_CTX_STATE_CLOSED));
	return PH7_OK;
}
/* ReflectionGenerator::getThis(): ?object — the receiver the generator's body
 * runs against. A coroutine frame installs it as a frame VARIABLE (see
 * VmFiberSetupFrame), not as pFrame->pThis, so both are checked. */
static int vm_builtin_ReflectionGenerator_getThis(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pExec && pExec->pFrame ){
		SyHashEntry *pVar = SyHashGet(&pExec->pFrame->hVar, "this", sizeof("this")-1);
		if( pVar ){
			ph7_value *pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj,
				(sxu32)SX_PTR_TO_INT(pVar->pUserData));
			if( pSlot && (pSlot->iFlags & MEMOBJ_OBJ) ){
				ph7_result_value(pCtx, pSlot);
				return PH7_OK;
			}
		}
		if( pExec->pFrame->pThis ){
			return ReflectResultBorrowed(pCtx, pExec->pFrame->pThis);
		}
	}
	ph7_result_null(pCtx);
	return PH7_OK;
}
/* ReflectionGenerator::getFunction(): ReflectionFunctionAbstract — still a
 * prelude class, so it is built through its own constructor. */
static int vm_builtin_ReflectionGenerator_getFunction(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_exec_ctx *pExec = ReflectGenExec(pCtx);
	ph7_vm_func *pFunc = pExec ? pExec->pFunc : 0;
	ph7_class_instance *pOut;
	ph7_value aArg[2];
	int nCtorArg = 1;
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pFunc == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjInit(pVm, &aArg[0]);
	PH7_MemObjInit(pVm, &aArg[1]);
	if( (pFunc->iFlags & VM_FUNC_CLASS_METHOD) && pFunc->pUserData ){
		ph7_class *pDecl = (ph7_class *)pFunc->pUserData;
		ph7_value_string(&aArg[0], SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));
		ph7_value_string(&aArg[1], SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));
		nCtorArg = 2;
	}else{
		ph7_value_string(&aArg[0], SyStringData(&pFunc->sName), (int)SyStringLength(&pFunc->sName));
	}
	{
		ph7_value *apCtor[2];
		apCtor[0] = &aArg[0];
		apCtor[1] = &aArg[1];
		pOut = ReflectConstruct(pCtx, nCtorArg == 2 ? "ReflectionMethod" : "ReflectionFunction",
			nCtorArg, apCtor, &rc);
	}
	PH7_MemObjRelease(&aArg[0]);
	PH7_MemObjRelease(&aArg[1]);
	if( pOut == 0 ){
		if( rc != PH7_OK ){
			return rc;
		}
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultObject(pCtx, pOut);
}
/* ReflectionGenerator::getExecutingGenerator(): Generator — follow `yield from`
 * delegation to the innermost one that is actually running. */
static int vm_builtin_ReflectionGenerator_getExecuting(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pGenObj = ReflectWrapped(pCtx, RG_GEN);
	ph7_value sVal, *pCur;
	ph7_generator *pGen;
	int iDepth = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pGenObj == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjInit(pVm, &sVal);
	sVal.x.pOther = pGenObj;
	sVal.iFlags = MEMOBJ_OBJ;
	pCur = &sVal;
	pGen = ReflectGeneratorCtx(pVm, &sVal);
	while( pGen && pGen->pCtx && pGen->pCtx->iDelegateState == 3
	 && iDepth <= REFLECT_WALK_MAX_DEPTH ){
		ph7_generator *pInner = ReflectGeneratorCtx(pVm, &pGen->pCtx->sDelegate);
		if( pInner == 0 ){
			break;
		}
		pCur = &pGen->pCtx->sDelegate;
		pGen = pInner;
		iDepth++;
	}
	return ReflectResultBorrowed(pCtx, (ph7_class_instance *)pCur->x.pOther);
}
/* ReflectionFiber::__construct(Fiber $fiber) / getFiber() / getCallable() */
static int vm_builtin_ReflectionFiber_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	if( pThis == 0 || nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_OK;
	}
	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RF_FIBER, (ph7_class_instance *)apArg[0]->x.pOther);
	return PH7_OK;
}
static int vm_builtin_ReflectionFiber_getFiber(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return ReflectResultBorrowed(pCtx, ReflectWrapped(pCtx, RF_FIBER));
}
static int vm_builtin_ReflectionFiber_getCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pFiber = ReflectWrapped(pCtx, RF_FIBER);
	ph7_value *pVal = pFiber ? PH7_NativeAttr(pFiber, "__callable") : 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pVal ){
		ph7_result_value(pCtx, pVal);
	}
	return PH7_OK;
}
/* ---- ReflectionConstant ---- */
/* The engine's record for a global constant, or NULL when undefined. */
static ph7_constant * ReflectConstEntry(ph7_vm *pVm, const char *zName, int nName)
{
	SyHashEntry *pEntry = nName > 0 ? SyHashGet(&pVm->hConstant, (const void *)zName, (sxu32)nName) : 0;
	return pEntry ? (ph7_constant *)pEntry->pUserData : 0;
}
/* The receiver's constant record, resolved from its public $name. */
static ph7_constant * ReflectConstOf(ph7_context *pCtx)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = "";
	int nName = 0;
	if( pThis ){
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	}
	return ReflectConstEntry(pCtx->pVm, zName, nName);
}
static int vm_builtin_ReflectionConstant_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	int nName = 0;
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( ReflectConstEntry(pCtx->pVm, zName, nName) == 0 ){
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Constant \"%.*s\" does not exist", nName, zName);
	}
	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", zName, nName);
	return PH7_OK;
}
static int vm_builtin_ReflectionConstant_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = "";
	int nName = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	}
	ph7_result_string(pCtx, zName, nName);
	return PH7_OK;
}
/* The namespace split php reports: everything before the last '\', and the rest. */
static int ReflectConstNsCut(const char *zName, int nName)
{
	int k;
	for( k = nName - 1 ; k >= 0 ; k-- ){
		if( zName[k] == '\\' ){
			return k;
		}
	}
	return -1;
}
static int vm_builtin_ReflectionConstant_getNamespaceName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = "";
	int nName = 0, iCut;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	}
	iCut = ReflectConstNsCut(zName, nName);
	ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);
	return PH7_OK;
}
static int vm_builtin_ReflectionConstant_getShortName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = "";
	int nName = 0, iCut;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	}
	iCut = ReflectConstNsCut(zName, nName);
	if( iCut < 0 ){
		ph7_result_string(pCtx, zName, nName);
	}else{
		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionConstant_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_constant *pCons = ReflectConstOf(pCtx);
	ph7_value sValue;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	PH7_MemObjInit(pCtx->pVm, &sValue);
	if( pCons && pCons->xExpand ){
		pCons->xExpand(&sValue, pCons->pUserData);
	}
	ph7_result_value(pCtx, &sValue);
	PH7_MemObjRelease(&sValue);
	return PH7_OK;
}
static int vm_builtin_ReflectionConstant_isDeprecated(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx, 0);
	return PH7_OK;
}
static int vm_builtin_ReflectionConstant_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_constant *pCons = ReflectConstOf(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pCons && SyStringLength(&pCons->sFile) > 0 ){
		ph7_result_string(pCtx, SyStringData(&pCons->sFile), (int)SyStringLength(&pCons->sFile));
	}else{
		ph7_result_bool(pCtx, 0);
	}
	return PH7_OK;
}
/* An engine constant belongs to the synthetic "Core" extension; a userland
 * define() belongs to none, which php reports as null / false. */
static int vm_builtin_ReflectionConstant_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_constant *pCons = ReflectConstOf(pCtx);
	ph7_class_instance *pExt;
	ph7_value sName, *apArgs[1];
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pCons == 0 || pCons->bUserDefined ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_MemObjInit(pCtx->pVm, &sName);
	ph7_value_string(&sName, "Core", 4);
	apArgs[0] = &sName;
	pExt = ReflectConstruct(pCtx, "ReflectionExtension", 1, apArgs, &rc);
	PH7_MemObjRelease(&sName);
	if( pExt == 0 ){
		if( rc != PH7_OK ){
			return rc;
		}
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultObject(pCtx, pExt);
}
static int vm_builtin_ReflectionConstant_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_constant *pCons = ReflectConstOf(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pCons && pCons->bUserDefined == 0 ){
		ph7_result_string(pCtx, "Core", 4);
	}else{
		ph7_result_bool(pCtx, 0);
	}
	return PH7_OK;
}
/* getAttributes() still routes through the prelude builder: chunk 7 owns the
 * ReflectionAttribute shape and the lazy argument evaluation. */
static int vm_builtin_ReflectionConstant_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_constant *pCons = ReflectConstOf(pCtx);
	const char *zName = "";
	int nName = 0;
	if( pThis == 0 || pCons == 0 ){
		ph7_result_value(pCtx, ph7_context_new_array(pCtx));
		return PH7_OK;
	}
	PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	{
		ph7_value sTarget;
		int rc;
		PH7_MemObjInit(pCtx->pVm, &sTarget);
		ph7_value_string(&sTarget, zName, nName);
		/* 64 = Attribute::TARGET_CONSTANT */
		rc = ReflectBuildAttrs(pCtx, &pCons->aAttrs, "const", &sTarget, 0, 0, 0, 64,
			nArg, apArg);
		PH7_MemObjRelease(&sTarget);
		return rc;
	}
}
static int vm_builtin_ReflectionConstant_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = "";
	int nName = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	}
	ph7_result_string_format(pCtx, "Constant [ %.*s ]\n", nName, zName);
	return PH7_OK;
}
/* ---- ReflectionExtension: PHL has exactly one, the synthetic "Core" ---- */
static int vm_builtin_ReflectionExtension_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	int nName = 0;
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";
	if( pThis == 0 ){
		return PH7_OK;
	}
	if( !(nName == 4 && SyToLower(zName[0]) == 'c' && SyToLower(zName[1]) == 'o'
	   && SyToLower(zName[2]) == 'r' && SyToLower(zName[3]) == 'e') ){
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Extension \"%.*s\" does not exist", nName, zName);
	}
	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", "Core", 4);
	return PH7_OK;
}
static int vm_builtin_ReflectionExtension_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = "";
	int nName = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	}
	ph7_result_string(pCtx, zName, nName);
	return PH7_OK;
}
static int vm_builtin_ReflectionExtension_getVersion(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_value sFn, sRes;
	SyString sStr;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	PH7_MemObjInit(pCtx->pVm, &sFn);
	PH7_MemObjInit(pCtx->pVm, &sRes);
	SyStringInitFromBuf(&sStr, "phpversion", sizeof("phpversion")-1);
	PH7_MemObjInitFromString(pCtx->pVm, &sFn, &sStr);
	if( PH7_VmCallUserFunction(pCtx->pVm, &sFn, 0, 0, &sRes) == SXRET_OK ){
		ph7_result_value(pCtx, &sRes);
	}
	PH7_MemObjRelease(&sFn);
	PH7_MemObjRelease(&sRes);
	return PH7_OK;
}
static int vm_builtin_ReflectionExtension_emptyArray(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_value *pList = ph7_context_new_array(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pList ){
		ph7_result_value(pCtx, pList);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionExtension_isPersistent(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx, 1);
	return PH7_OK;
}
static int vm_builtin_ReflectionExtension_isTemporary(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx, 0);
	return PH7_OK;
}
static int vm_builtin_ReflectionExtension_info(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	SXUNUSED(pCtx);
	return PH7_OK;
}
static int vm_builtin_ReflectionExtension_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = "";
	int nName = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	}
	ph7_result_string_format(pCtx, "Extension [ extension #1 %.*s ]\n", nName, zName);
	return PH7_OK;
}
/* ---- ReflectionZendExtension: none exist, so the constructor always refuses ---- */
static int vm_builtin_ReflectionZendExtension_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	int nName = 0;
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";
	return PH7_VmThrowException(pCtx, "ReflectionException",
		"Zend Extension \"%.*s\" does not exist", nName, zName);
}
static int vm_builtin_ReflectionZendExtension_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_string(pCtx, "", 0);
	return PH7_OK;
}
/* ---- ReflectionReference ---- */
/*
 * ReflectionReference::fromArrayElement(array $array, string|int $key)
 *
 * Answers a reflector only when the element IS a reference — its slot carries a
 * reference-table record with at least two links. The instance is built without
 * running the (private) constructor, exactly as php's factory does.
 */
static int vm_builtin_ReflectionReference_fromArrayElement(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_hashmap *pMap;
	ph7_hashmap_node *pNode = 0;
	ph7_class *pClass;
	ph7_class_instance *pObj;
	char zId[64];
	if( nArg < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( !ph7_value_is_array(apArg[0]) ){
		/* The shared ZPP screen leaves a SCALAR against a bare `array` parameter
		 * unscreened (it only judges array/object/null/resource pairings and
		 * scalars against class-typed ones), so the refusal php words from the
		 * declared type is written here -- as the prelude's own is_array() check
		 * did. Recorded in PLAN §2 with the rest of that gap. */
		return PH7_VmThrowException(pCtx, "TypeError",
			"ReflectionReference::fromArrayElement(): Argument #1 ($array) "
			"must be of type array, %s given", ph7_type_name(apArg[0]));
	}
	if( nArg < 2 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pMap = (ph7_hashmap *)apArg[0]->x.pOther;
	if( PH7_HashmapLookup(pMap, apArg[1], &pNode) != SXRET_OK || pNode == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( PH7_VmSlotRefCount(pVm, pNode->nValIdx) < 2 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pClass = PH7_VmExtractClass(pVm, "ReflectionReference", sizeof("ReflectionReference")-1, FALSE, 0);
	pObj = pClass ? PH7_NewClassInstance(pVm, pClass) : 0;
	if( pObj == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	/* php's ids are opaque strings; the slot index is the identity PHL has. */
	SyBufferFormat(zId, sizeof(zId), "phlref%u", (unsigned)pNode->nValIdx);
	PH7_NativeSetAttrStr(pVm, pObj, RR_ID, zId, (int)SyStrlen(zId));
	return ReflectResultObject(pCtx, pObj);
}
static int vm_builtin_ReflectionReference_getId(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zId = "";
	int nId = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis, RR_ID, &zId, &nId);
	}
	ph7_result_string(pCtx, zId, nId);
	return PH7_OK;
}
/* php declares this private and never calls it; reaching it is the diagnostic. */
static int vm_builtin_ReflectionReference_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	SXUNUSED(pCtx);
	return PH7_OK;
}
/*
 * Declare the six. Called from PH7_VmInstallReflectionLib where chunk 5 used to
 * be compiled, so Reflector (chunk 1) already exists.
 */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionSmall(ph7_vm *pVm)
{
	static const PH7_NativePropDef aGenProp[] = {
		{ RG_GEN, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
	};
	static const PH7_NativeMethodDef aGenMethod[] = {
		{ "__construct",           PH7_MOD_PUBLIC, "Generator $generator", "",
		  vm_builtin_ReflectionGenerator_construct },
		{ "getFunction",           PH7_MOD_PUBLIC, "", "ReflectionFunctionAbstract",
		  vm_builtin_ReflectionGenerator_getFunction },
		{ "getThis",               PH7_MOD_PUBLIC, "", "?object", vm_builtin_ReflectionGenerator_getThis },
		{ "getExecutingGenerator", PH7_MOD_PUBLIC, "", "Generator",
		  vm_builtin_ReflectionGenerator_getExecuting },
		{ "isClosed",              PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionGenerator_isClosed },
		{ "getExecutingLine",      PH7_MOD_PUBLIC, "", "int", vm_builtin_ReflectionGenerator_execLine },
		{ "getExecutingFile",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionGenerator_execFile },
		{ "getTrace",              PH7_MOD_PUBLIC, "int $options = 1", "array",
		  vm_builtin_ReflectionGenerator_trace },
	};
	static const PH7_NativePropDef aFiberProp[] = {
		{ RF_FIBER, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
	};
	static const PH7_NativeMethodDef aFiberMethod[] = {
		{ "__construct",      PH7_MOD_PUBLIC, "Fiber $fiber", "", vm_builtin_ReflectionFiber_construct },
		{ "getFiber",         PH7_MOD_PUBLIC, "", "Fiber",    vm_builtin_ReflectionFiber_getFiber },
		{ "getCallable",      PH7_MOD_PUBLIC, "", "callable", vm_builtin_ReflectionFiber_getCallable },
		{ "getExecutingLine", PH7_MOD_PUBLIC, "", "?int",     vm_builtin_ReflectionFiber_execLine },
		{ "getExecutingFile", PH7_MOD_PUBLIC, "", "?string",  vm_builtin_ReflectionFiber_execFile },
		{ "getTrace",         PH7_MOD_PUBLIC, "int $options = 1", "array", vm_builtin_ReflectionFiber_trace },
	};
	static const PH7_NativePropDef aNameProp[] = {
		{ "name", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 } },
	};
	static const PH7_NativeMethodDef aConstMethod[] = {
		{ "__construct",       PH7_MOD_PUBLIC, "string $name", "",
		  vm_builtin_ReflectionConstant_construct },
		{ "getName",           PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_getName },
		{ "getNamespaceName",  PH7_MOD_PUBLIC, "", "string",
		  vm_builtin_ReflectionConstant_getNamespaceName },
		{ "getShortName",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_getShortName },
		{ "getValue",          PH7_MOD_PUBLIC, "", "mixed",  vm_builtin_ReflectionConstant_getValue },
		{ "isDeprecated",      PH7_MOD_PUBLIC, "", "bool",   vm_builtin_ReflectionConstant_isDeprecated },
		{ "getFileName",       PH7_MOD_PUBLIC, "", "string|false",
		  vm_builtin_ReflectionConstant_getFileName },
		{ "getExtension",      PH7_MOD_PUBLIC, "", "?ReflectionExtension",
		  vm_builtin_ReflectionConstant_getExtension },
		{ "getExtensionName",  PH7_MOD_PUBLIC, "", "string|false",
		  vm_builtin_ReflectionConstant_getExtensionName },
		{ "getAttributes",     PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",
		  vm_builtin_ReflectionConstant_getAttributes },
		{ "__toString",        PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionConstant_toString },
	};
	static const PH7_NativeMethodDef aExtMethod[] = {
		{ "__construct",     PH7_MOD_PUBLIC, "string $name", "", vm_builtin_ReflectionExtension_construct },
		{ "getName",         PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionExtension_getName },
		{ "getVersion",      PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionExtension_getVersion },
		{ "getFunctions",    PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionExtension_emptyArray },
		{ "getConstants",    PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionExtension_emptyArray },
		{ "getINIEntries",   PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionExtension_emptyArray },
		{ "getClasses",      PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionExtension_emptyArray },
		{ "getClassNames",   PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionExtension_emptyArray },
		{ "getDependencies", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionExtension_emptyArray },
		{ "info",            PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionExtension_info },
		{ "isPersistent",    PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionExtension_isPersistent },
		{ "isTemporary",     PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionExtension_isTemporary },
		{ "__toString",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionExtension_toString },
	};
	static const PH7_NativeMethodDef aZendMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "string $name", "",
		  vm_builtin_ReflectionZendExtension_construct },
		{ "getName",     PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionExtension_getName },
		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionZendExtension_toString },
	};
	static const PH7_NativePropDef aRefProp[] = {
		{ RR_ID, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 } },
	};
	static const PH7_NativeMethodDef aRefMethod[] = {
		/* php declares the constructor PRIVATE, so `new ReflectionReference` is
		 * the engine's own "Call to private ... from global scope" Error rather
		 * than a throw the prelude had to write by hand. */
		{ "__construct",      PH7_MOD_PRIVATE, "", "", vm_builtin_ReflectionReference_construct },
		{ "fromArrayElement", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "array $array, string|int $key",
		  "?ReflectionReference", vm_builtin_ReflectionReference_fromArrayElement },
		{ "getId",            PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionReference_getId },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "ReflectionGenerator", 0, 0, PH7_CLASS_FINAL|PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aGenMethod, SX_ARRAYSIZE(aGenMethod), 0, 0, aGenProp, SX_ARRAYSIZE(aGenProp), 0, 0 },
		{ "ReflectionFiber", 0, 0, PH7_CLASS_FINAL|PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aFiberMethod, SX_ARRAYSIZE(aFiberMethod), 0, 0, aFiberProp, SX_ARRAYSIZE(aFiberProp), 0, 0 },
		{ "ReflectionConstant", 0, "Reflector", PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aConstMethod, SX_ARRAYSIZE(aConstMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0 },
		{ "ReflectionExtension", 0, "Reflector", PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aExtMethod, SX_ARRAYSIZE(aExtMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0 },
		{ "ReflectionZendExtension", 0, "Reflector", PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aZendMethod, SX_ARRAYSIZE(aZendMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0 },
		{ "ReflectionReference", 0, 0, PH7_CLASS_FINAL|PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aRefMethod, SX_ARRAYSIZE(aRefMethod), 0, 0, aRefProp, SX_ARRAYSIZE(aRefProp), 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));
}
/*
 * ---------------------------------------------------------------------------
 * Reflector, Reflection, ReflectionException, ReflectionClass, ReflectionObject.
 *
 * Chunk 1 — the core of the whole API. Its ~350 lines of PHP funnelled every
 * accessor through __phl_rcinfo(), a memoized descriptor ARRAY built by a C
 * thunk: sixty methods that each rebuilt or re-read a marshalled copy of state
 * the engine was already holding. A native method reads ph7_class directly, so
 * the descriptor is gone from this path entirely and the memo it needed with it.
 *
 * What stays behind is the prelude that has not moved yet, and these classes
 * still reach it by name where php's own object graph does: getMethod() builds
 * a ReflectionMethod, getProperty() a ReflectionProperty, getAttributes() calls
 * __reflect_build_attrs, __toString() calls __reflect_export_class. Those become
 * direct C the moment chunks 2, 3, 7 and 9 land.
 * ---------------------------------------------------------------------------
 */
#define RC_OBJ "__obj"

/* The class a ReflectionClass reflects: its `name` slot, resolved. The
 * constructor already stored the canonical name, so no autoload can be needed
 * here. */
static ph7_class * ReflectClassOf(ph7_context *pCtx)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName;
	int nName;
	if( pThis == 0 ){
		return 0;
	}
	PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	if( nName < 1 ){
		return 0;
	}
	/* PH7_NativeAttrStr borrows bytes that are NOT NUL-terminated: the length
	 * has to travel with them. */
	return PH7_VmExtractClass(pCtx->pVm, zName, (sxu32)nName, FALSE, 0);
}
/* The instance a ReflectionObject was built over, or NULL for a plain
 * ReflectionClass — what makes DYNAMIC properties visible. */
static ph7_class_instance * ReflectClassObj(ph7_context *pCtx)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	return pThis ? PH7_NativeAttrObj(pThis, RC_OBJ) : 0;
}
/* $this->name as bytes. */
static void ReflectClassName(ph7_context *pCtx, const char **pzOut, int *pnOut)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	*pzOut = "";
	*pnOut = 0;
	if( pThis ){
		PH7_NativeAttrStr(pThis, "name", pzOut, pnOut);
	}
}
/* Answer the truth of one of the reflected class's iFlags bits. */
static int ReflectClassFlag(ph7_context *pCtx, sxi32 iFlag)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	return pClass != 0 && (pClass->iFlags & iFlag) != 0;
}
#define REFLECT_CLASS_FLAG(NAME,FLAG,NEGATE) \
	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \
	{ \
		SXUNUSED(nArg); \
		SXUNUSED(apArg); \
		ph7_result_bool(pCtx, NEGATE ? !ReflectClassFlag(pCtx,FLAG) : ReflectClassFlag(pCtx,FLAG)); \
		return PH7_OK; \
	}
REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isInternal,    PH7_CLASS_INTERNAL,  0)
REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isUserDefined, PH7_CLASS_INTERNAL,  1)
REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isInterface,   PH7_CLASS_INTERFACE, 0)
REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isTrait,       PH7_CLASS_TRAIT,     0)
REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isAbstract,    PH7_CLASS_ABSTRACT,  0)
REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isFinal,       PH7_CLASS_FINAL,     0)
REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isReadOnly,    PH7_CLASS_READONLY,  0)
REFLECT_CLASS_FLAG(vm_builtin_ReflectionClass_isEnum,        PH7_CLASS_ENUM,      0)

/* Build a ReflectionClass over pTarget with its `name` already filled in: the
 * constructor would only re-resolve a class this code is holding. */
static int ReflectResultClassOf(ph7_context *pCtx, ph7_class *pTarget)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pRC;
	ph7_class_instance *pObj;
	if( pTarget == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pRC = PH7_VmExtractClass(pVm, "ReflectionClass", sizeof("ReflectionClass")-1, FALSE, 0);
	if( pRC == 0 || (pObj = PH7_NewClassInstance(pVm, pRC)) == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(&pTarget->sName),
		(int)SyStringLength(&pTarget->sName));
	PH7_NativeResultObject(pCtx, pObj);
	return PH7_OK;
}
/* The class an argument declared `ReflectionClass|string` denotes: a reflector's
 * `name` slot, or the string itself. NULL when it names nothing (after
 * autoload). */
static ph7_class * ReflectClassArg(ph7_context *pCtx, ph7_value *pArg)
{
	ph7_vm *pVm = pCtx->pVm;
	if( pArg->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pObj = (ph7_class_instance *)pArg->x.pOther;
		ph7_class *pRC = PH7_VmExtractClass(pVm, "ReflectionClass", sizeof("ReflectionClass")-1, FALSE, 0);
		if( pRC && PH7_VmInstanceOf(pObj->pClass, pRC) ){
			const char *zName;
			int nName;
			PH7_NativeAttrStr(pObj, "name", &zName, &nName);
			return nName > 0 ? PH7_VmExtractClass(pVm, zName, (sxu32)nName, FALSE, 0) : 0;
		}
	}
	return ReflectResolveClass(pVm, pArg);
}
/* ---- constructors ---- */
/* ReflectionClass::__construct(object|string $objectOrClass) */
static int vm_builtin_ReflectionClass_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class *pClass;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	pClass = ReflectResolveClass(pCtx->pVm, apArg[0]);
	if( pClass == 0 ){
		/* php reports the NAME it was handed, after the declared object|string
		 * has coerced a scalar — `new ReflectionClass(1.5)` says Class "1.5". */
		const char *zName;
		int nName;
		zName = ph7_value_to_string(apArg[0], &nName);
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Class \"%.*s\" does not exist", nName, zName);
	}
	PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", SyStringData(&pClass->sName),
		(int)SyStringLength(&pClass->sName));
	return PH7_OK;
}
/* ReflectionObject::__construct(object $object) — the same, plus the receiver
 * whose DYNAMIC properties the inherited accessors then report. */
static int vm_builtin_ReflectionObject_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	sxi32 rc;
	if( pThis == 0 || nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 ){
		return PH7_OK;
	}
	rc = vm_builtin_ReflectionClass_construct(pCtx, nArg, apArg);
	if( rc != PH7_OK ){
		return rc;
	}
	PH7_NativeSetAttrObj(pCtx->pVm, pThis, RC_OBJ, (ph7_class_instance *)apArg[0]->x.pOther);
	return PH7_OK;
}
/* ReflectionClass::__clone(): void — php declares it private, and the class is
 * uncloneable besides (PH7_CLASS_NOCLONE answers before any body runs). */
static int vm_builtin_ReflectionClass_clone(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return PH7_OK;
}
/* ---- name ---- */
static int vm_builtin_ReflectionClass_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	const char *zName;
	int nName;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ReflectClassName(pCtx, &zName, &nName);
	ph7_result_string(pCtx, zName, nName);
	return PH7_OK;
}
/* Offset just past the last namespace separator, or -1 when there is none. */
static int ReflectNsCut(const char *zName, int nName)
{
	int i;
	for( i = nName - 1 ; i >= 0 ; i-- ){
		if( zName[i] == '\\' ){
			return i;
		}
	}
	return -1;
}
static int vm_builtin_ReflectionClass_getShortName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	const char *zName;
	int nName, iCut;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ReflectClassName(pCtx, &zName, &nName);
	iCut = ReflectNsCut(zName, nName);
	if( iCut < 0 ){
		ph7_result_string(pCtx, zName, nName);
	}else{
		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_getNamespaceName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	const char *zName;
	int nName, iCut;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ReflectClassName(pCtx, &zName, &nName);
	iCut = ReflectNsCut(zName, nName);
	ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_inNamespace(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	const char *zName;
	int nName;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ReflectClassName(pCtx, &zName, &nName);
	ph7_result_bool(pCtx, ReflectNsCut(zName, nName) >= 0);
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_isAnonymous(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	const char *zName;
	int nName;
	static const char zAnon[] = "class@anonymous";
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ReflectClassName(pCtx, &zName, &nName);
	ph7_result_bool(pCtx, nName >= (int)sizeof(zAnon)-1
		&& SyMemcmp(zName, zAnon, sizeof(zAnon)-1) == 0);
	return PH7_OK;
}
/* ---- shape ---- */
static int vm_builtin_ReflectionClass_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	sxi64 iMods = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pClass ){
		if( pClass->iFlags & PH7_CLASS_ABSTRACT ){ iMods |= 64; }
		if( pClass->iFlags & PH7_CLASS_FINAL ){ iMods |= 32; }
		if( pClass->iFlags & PH7_CLASS_READONLY ){ iMods |= 65536; }
	}
	ph7_result_int64(pCtx, iMods);
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_getParentClass(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pClass == 0 || pClass->pBase == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	return ReflectResultClassOf(pCtx, pClass->pBase);
}
/* getInterfaceNames()/getInterfaces()/getTraitNames()/getTraits() — one walk,
 * four shapes. bReflector picks name-list vs {name: ReflectionClass}. */
static int ReflectClassNameList(ph7_context *pCtx, int bTraits, int bReflector)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	ph7_value *pList = ph7_context_new_array(pCtx);
	SySet aSet;
	ph7_class **apOut;
	sxu32 n, nOut;
	if( pList == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( pClass == 0 ){
		ph7_result_value(pCtx, pList);
		return PH7_OK;
	}
	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));
	if( bTraits ){
		apOut = (ph7_class **)SySetBasePtr(&pClass->aTrait);
		nOut = SySetUsed(&pClass->aTrait);
	}else{
		ReflectInterfacesOf(pClass, &aSet);
		apOut = (ph7_class **)SySetBasePtr(&aSet);
		nOut = SySetUsed(&aSet);
	}
	for( n = 0 ; n < nOut ; n++ ){
		SyString *pName = &apOut[n]->sName;
		if( bReflector ){
			/* {name: ReflectionClass} — the reflector is built here rather than
			 * through ReflectResultClassOf, which writes the RESULT slot. */
			ph7_class *pRC = PH7_VmExtractClass(pCtx->pVm, "ReflectionClass",
				sizeof("ReflectionClass")-1, FALSE, 0);
			ph7_class_instance *pObj = pRC ? PH7_NewClassInstance(pCtx->pVm, pRC) : 0;
			ph7_value *pKey = ph7_context_new_scalar(pCtx);
			ph7_value sVal;
			if( pObj == 0 || pKey == 0 ){ break; }
			PH7_NativeSetAttrStr(pCtx->pVm, pObj, "name", SyStringData(pName),
				(int)SyStringLength(pName));
			/* A STACK carrier, never a context scalar: releasing a MEMOBJ_OBJ
			 * context value would unref the instance a second time. */
			PH7_MemObjInit(pCtx->pVm, &sVal);
			sVal.x.pOther = pObj;
			sVal.iFlags = MEMOBJ_OBJ;
			ph7_value_string(pKey, SyStringData(pName), (int)SyStringLength(pName));
			ph7_array_add_elem(pList, pKey, &sVal);   /* takes its own reference */
			PH7_ClassInstanceUnref(pObj);
		}else{
			ph7_value *pVal = ph7_context_new_scalar(pCtx);
			if( pVal == 0 ){ break; }
			ph7_value_string(pVal, SyStringData(pName), (int)SyStringLength(pName));
			ph7_array_add_elem(pList, 0, pVal);
		}
	}
	SySetRelease(&aSet);
	ph7_result_value(pCtx, pList);
	return PH7_OK;
}
#define REFLECT_CLASS_LIST(NAME,TRAITS,REFLECTOR) \
	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \
	{ \
		SXUNUSED(nArg); \
		SXUNUSED(apArg); \
		return ReflectClassNameList(pCtx,TRAITS,REFLECTOR); \
	}
REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getInterfaceNames, 0, 0)
REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getInterfaces,     0, 1)
REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getTraitNames,     1, 0)
REFLECT_CLASS_LIST(vm_builtin_ReflectionClass_getTraits,         1, 1)

/* getTraitAliases(): php reports `use T { m as n; }` renames; PHL's compiler
 * installs the alias as a method and keeps no rename record, so the map is
 * empty (§7.4). */
static int vm_builtin_ReflectionClass_getTraitAliases(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_value(pCtx, ph7_context_new_array(pCtx));
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_isIterable(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	SySet aSet;
	ph7_class **apIface;
	sxu32 n;
	int bIterable = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pClass == 0
	 || (pClass->iFlags & (PH7_CLASS_INTERFACE|PH7_CLASS_TRAIT|PH7_CLASS_ABSTRACT)) ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));
	ReflectInterfacesOf(pClass, &aSet);
	apIface = (ph7_class **)SySetBasePtr(&aSet);
	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){
		if( pCtx->pVm->pTraversableClass && apIface[n] == pCtx->pVm->pTraversableClass ){
			bIterable = 1;
			break;
		}
	}
	SySetRelease(&aSet);
	ph7_result_bool(pCtx, bIterable);
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_implementsInterface(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	ph7_class *pTarget;
	SySet aSet;
	ph7_class **apIface;
	sxu32 n;
	int bYes = 0;
	if( nArg < 1 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	pTarget = ReflectClassArg(pCtx, apArg[0]);
	if( pTarget == 0 ){
		const char *zName;
		int nName;
		zName = ph7_value_to_string(apArg[0], &nName);
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Interface \"%.*s\" does not exist", nName, zName);
	}
	if( (pTarget->iFlags & PH7_CLASS_INTERFACE) == 0 ){
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"%z is not an interface", &pTarget->sName);
	}
	if( pClass == pTarget ){
		ph7_result_bool(pCtx, 1);
		return PH7_OK;
	}
	if( pClass == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));
	ReflectInterfacesOf(pClass, &aSet);
	apIface = (ph7_class **)SySetBasePtr(&aSet);
	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){
		if( apIface[n] == pTarget ){
			bYes = 1;
			break;
		}
	}
	SySetRelease(&aSet);
	ph7_result_bool(pCtx, bYes);
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_isSubclassOf(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	ph7_class *pTarget, *pWalk;
	SySet aSet;
	ph7_class **apIface;
	sxu32 n;
	int iDepth = 0, bYes = 0;
	if( nArg < 1 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	pTarget = ReflectClassArg(pCtx, apArg[0]);
	if( pTarget == 0 ){
		const char *zName;
		int nName;
		zName = ph7_value_to_string(apArg[0], &nName);
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Class \"%.*s\" does not exist", nName, zName);
	}
	/* php: a class is never a subclass of ITSELF */
	if( pClass == 0 || pClass == pTarget ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	for( pWalk = pClass->pBase ; pWalk && iDepth <= REFLECT_WALK_MAX_DEPTH ; pWalk = pWalk->pBase ){
		if( pWalk == pTarget ){
			ph7_result_bool(pCtx, 1);
			return PH7_OK;
		}
		iDepth++;
	}
	SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));
	ReflectInterfacesOf(pClass, &aSet);
	apIface = (ph7_class **)SySetBasePtr(&aSet);
	for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){
		if( apIface[n] == pTarget ){
			bYes = 1;
			break;
		}
	}
	SySetRelease(&aSet);
	ph7_result_bool(pCtx, bYes);
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_isInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	ph7_class_instance *pObj;
	if( nArg < 1 || (apArg[0]->iFlags & MEMOBJ_OBJ) == 0 || pClass == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	pObj = (ph7_class_instance *)apArg[0]->x.pOther;
	ph7_result_bool(pCtx, PH7_VmInstanceOf(pObj->pClass, pClass) != 0);
	return PH7_OK;
}
/* ---- source position ---- */
static int vm_builtin_ReflectionClass_getStartLine(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pClass == 0 || (pClass->iFlags & PH7_CLASS_INTERNAL) ){
		ph7_result_bool(pCtx, 0);
	}else{
		ph7_result_int64(pCtx, (sxi64)pClass->nLine);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_getEndLine(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pClass == 0 || (pClass->iFlags & PH7_CLASS_INTERNAL) ){
		ph7_result_bool(pCtx, 0);
	}else{
		ph7_result_int64(pCtx, (sxi64)pClass->nEndLine);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pClass && SyStringLength(&pClass->sFile) > 0 ){
		ph7_result_string(pCtx, SyStringData(&pClass->sFile), (int)SyStringLength(&pClass->sFile));
	}else{
		ph7_result_bool(pCtx, 0);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pClass && SyStringLength(&pClass->sDoc) > 0 ){
		ph7_result_string(pCtx, SyStringData(&pClass->sDoc), (int)SyStringLength(&pClass->sDoc));
	}else{
		ph7_result_bool(pCtx, 0);
	}
	return PH7_OK;
}
/* ---- instantiation ---- */
static int vm_builtin_ReflectionClass_isInstantiable(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	sxi32 iCtor, iClone;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pClass == 0
	 || (pClass->iFlags & (PH7_CLASS_INTERFACE|PH7_CLASS_TRAIT|PH7_CLASS_ABSTRACT|PH7_CLASS_ENUM)) ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	ReflectCtorCloneVis(pCtx->pVm, pClass, &iCtor, &iClone);
	ph7_result_bool(pCtx, iCtor == 0 || iCtor == PH7_CLASS_PROT_PUBLIC);
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_isCloneable(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	sxi32 iCtor, iClone;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pClass == 0
	 || (pClass->iFlags & (PH7_CLASS_INTERFACE|PH7_CLASS_TRAIT|PH7_CLASS_ABSTRACT)) ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	ReflectCtorCloneVis(pCtx->pVm, pClass, &iCtor, &iClone);
	ph7_result_bool(pCtx, iClone == 0 || iClone == PH7_CLASS_PROT_PUBLIC);
	return PH7_OK;
}
/* php's own gate, raised before any object exists. */
static sxi32 ReflectCheckInstantiable(ph7_context *pCtx, ph7_class *pClass)
{
	if( pClass->iFlags & PH7_CLASS_INTERFACE ){
		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate interface %z", &pClass->sName);
	}
	if( pClass->iFlags & PH7_CLASS_TRAIT ){
		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate trait %z", &pClass->sName);
	}
	if( pClass->iFlags & PH7_CLASS_ABSTRACT ){
		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate abstract class %z", &pClass->sName);
	}
	return PH7_OK;
}
/*
 * newInstance()/newInstanceArgs(): the checks php runs, then the constructor.
 * apCtor/nCtor are already-collected positional arguments; pNames is the
 * name map when the caller handed an array with string keys (php 8.1 accepts
 * those as NAMED constructor arguments).
 */
static int ReflectNewInstance(ph7_context *pCtx, int nCtor, ph7_value **apCtor,
	SyString *pNames)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = ReflectClassOf(pCtx);
	ph7_class_instance *pObj;
	ph7_class_method *pCons;
	sxi32 iCtorVis, iCloneVis, rc;
	if( pClass == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	rc = ReflectCheckInstantiable(pCtx, pClass);
	if( rc != PH7_OK ){
		return rc;
	}
	ReflectCtorCloneVis(pVm, pClass, &iCtorVis, &iCloneVis);
	if( iCtorVis != 0 && iCtorVis != PH7_CLASS_PROT_PUBLIC ){
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Access to non-public constructor of class %z", &pClass->sName);
	}
	if( iCtorVis == 0 && nCtor > 0 ){
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Class %z does not have a constructor, so you cannot pass any constructor arguments",
			&pClass->sName);
	}
	if( VmClassStaticDeferPending(pClass) ){
		/* Instantiation materializes the static table (OP_NEW does it too), so a
		 * broken default raises BEFORE any object exists. */
		sxi32 rcMat = PH7_VmMaterializeClassStatics(pVm, pClass);
		if( rcMat != SXRET_OK ){
			return rcMat;
		}
	}
	pObj = PH7_NewClassInstance(pVm, pClass);
	if( pObj == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pCons = PH7_ClassExtractMethod(pClass, "__construct", sizeof("__construct")-1);
	if( pCons ){
		if( pNames ){
			VmCallArgMap sMap;
			SyZero(&sMap, sizeof(sMap));
			sMap.bHasNamed = 1;
			sMap.nTotal = (sxu32)nCtor;
			sMap.aNames = pNames;
			rc = PH7_VmCallClassMethodMap(pVm, pObj, pCons, 0, nCtor, apCtor, &sMap);
		}else{
			rc = PH7_VmCallClassMethod(pVm, pObj, pCons, 0, nCtor, apCtor);
		}
		if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){
			PH7_ClassInstanceUnref(pObj);
			return rc;
		}
	}
	return ReflectResultObject(pCtx, pObj);
}
static int vm_builtin_ReflectionClass_newInstance(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	return ReflectNewInstance(pCtx, nArg, apArg, 0);
}
static int vm_builtin_ReflectionClass_newInstanceArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SySet aArg;
	SyString *aNames = 0;
	int rc;
	SySetInit(&aArg, &pCtx->pVm->sAllocator, sizeof(ph7_value *));
	if( nArg > 0 ){
		ReflectCollectArgs(pCtx, apArg[0], &aArg, &aNames);
	}
	rc = ReflectNewInstance(pCtx, (int)SySetUsed(&aArg),
		(ph7_value **)SySetBasePtr(&aArg), aNames);
	if( aNames ){
		SyMemBackendFree(&pCtx->pVm->sAllocator, aNames);
	}
	SySetRelease(&aArg);
	return rc;
}
static int vm_builtin_ReflectionClass_newInstanceWithoutConstructor(ph7_context *pCtx,
	int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pClass == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	rc = ReflectCheckInstantiable(pCtx, pClass);
	if( rc != PH7_OK ){
		return rc;
	}
	if( VmClassStaticDeferPending(pClass) ){
		sxi32 rcMat = PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);
		if( rcMat != SXRET_OK ){
			return rcMat;
		}
	}
	return ReflectResultObject(pCtx, PH7_NewClassInstance(pCtx->pVm, pClass));
}
/* ---- members ---- */
/* The modifier mask php filters a member on. */
static sxi64 ReflectVisMask(sxi32 iProt)
{
	if( iProt == PH7_CLASS_PROT_PUBLIC ){
		return 1;
	}
	return iProt == PH7_CLASS_PROT_PROTECTED ? 2 : 4;
}
static sxi64 ReflectPropModifiers(ph7_class_attr *pAttr)
{
	sxi64 iMods = ReflectVisMask(pAttr->iProtection);
	if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){ iMods |= 16; }
	if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){ iMods |= 512; }
	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){
		/* php models readonly as protected(set) and reports the bit. */
		iMods |= 128|2048;
	}
	if( pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET ){ iMods |= 2048; }
	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){
		/* private(set) cannot be widened by a subclass, so php reports it FINAL. */
		iMods |= 4096|32;
	}
	return iMods;
}
static sxi64 ReflectMethodModifiers(ph7_class_method *pMeth)
{
	sxi64 iMods = ReflectVisMask(pMeth->iProtection);
	if( pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){ iMods |= 16; }
	if( pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){ iMods |= 64; }
	if( pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods |= 32; }
	return iMods;
}
static sxi64 ReflectConstModifiers(ph7_class_attr *pAttr)
{
	sxi64 iMods = ReflectVisMask(pAttr->iProtection);
	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods |= 32; }
	return iMods;
}
/* Does the reflected object own a property under this name? (1 = exists) */
static int ReflectObjHasProp(ph7_class_instance *pObj, const char *zName, int nName)
{
	return pObj != 0 && nName > 0
		&& SyHashGet(&pObj->hAttr, (const void *)zName, (sxu32)nName) != 0;
}
static int vm_builtin_ReflectionClass_hasMethod(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	const char *zName;
	int nName;
	if( pClass == 0 || nArg < 1 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0], &nName);
	ph7_result_bool(pCtx, ReflectFindMethodEntry(pClass, zName, nName) != 0);
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_hasProperty(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	const char *zName;
	int nName, bFound = 0;
	SySet aMembers;
	sxu32 n;
	if( pClass == 0 || nArg < 1 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0], &nName);
	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));
	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);
	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){
		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);
		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zName, nName) ){
			bFound = 1;
			break;
		}
	}
	SySetRelease(&aMembers);
	/* A ReflectionObject also sees the instance's own dynamic properties */
	if( !bFound ){
		bFound = ReflectObjHasProp(ReflectClassObj(pCtx), zName, nName);
	}
	ph7_result_bool(pCtx, bFound);
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_hasConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	const char *zName;
	int nName, bFound = 0;
	SySet aMembers;
	sxu32 n;
	if( pClass == 0 || nArg < 1 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0], &nName);
	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));
	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);
	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){
		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);
		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){
			bFound = 1;
			break;
		}
	}
	SySetRelease(&aMembers);
	ph7_result_bool(pCtx, bFound);
	return PH7_OK;
}
/*
 * The value of a class constant, materializing its lazily-evaluated slot.
 *
 * php re-evaluates a failed initializer on EVERY read, so this can raise; the
 * status is returned rather than swallowed, because a native body that answers
 * PH7_OK with a throw in flight lets the caller carry on and print the NULL it
 * never should have seen.
 */
static sxi32 ReflectConstSlot(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr,
	ph7_value **ppOut)
{
	sxi32 rc = PH7_VmMaterializeClassConst(pCtx->pVm, pClass, pAttr);
	*ppOut = 0;
	if( rc != SXRET_OK ){
		return rc;
	}
	*ppOut = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);
	return SXRET_OK;
}
static int vm_builtin_ReflectionClass_getConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	const char *zName;
	int nName;
	SySet aMembers;
	sxu32 n;
	ph7_class_attr *pFound = 0;
	if( pClass == 0 || nArg < 1 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0], &nName);
	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));
	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);
	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){
		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);
		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){
			pFound = pM->pAttr;
			break;
		}
	}
	SySetRelease(&aMembers);
	if( pFound == 0 ){
		PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED,
			"ReflectionClass::getConstant() for a non-existent constant is deprecated, "
			"use ReflectionClass::hasConstant() to check if the constant exists");
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	{
		ph7_value *pVal;
		sxi32 rc = ReflectConstSlot(pCtx, pClass, pFound, &pVal);
		if( rc != SXRET_OK ){
			return rc;
		}
		if( pVal ){
			ph7_result_value(pCtx, pVal);
		}else{
			ph7_result_null(pCtx);
		}
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_getConstants(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	ph7_value *pOut = ph7_context_new_array(pCtx);
	SySet aMembers;
	sxu32 n;
	int bFilter = (nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0);
	sxi64 iFilter = bFilter ? ph7_value_to_int64(apArg[0]) : 0;
	if( pOut == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( pClass == 0 ){
		ph7_result_value(pCtx, pOut);
		return PH7_OK;
	}
	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));
	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);
	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){
		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);
		ph7_value *pVal;
		if( pM->iKind != REFLECT_MEMBER_CONST ){
			continue;
		}
		if( bFilter && (ReflectConstModifiers(pM->pAttr) & iFilter) == 0 ){
			continue;
		}
		{
			sxi32 rc = ReflectConstSlot(pCtx, pClass, pM->pAttr, &pVal);
			if( rc != SXRET_OK ){
				SySetRelease(&aMembers);
				return rc;
			}
		}
		if( pVal ){
			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, pVal);
		}
	}
	SySetRelease(&aMembers);
	ph7_result_value(pCtx, pOut);
	return PH7_OK;
}
/*
 * getMethod()/getProperty()/getReflectionConstant() build a class that is still
 * PRELUDE PHP, so they go through its constructor (ReflectConstruct). They
 * become direct C when chunks 2 and 3 land.
 */
static int ReflectResultMember(ph7_context *pCtx, const char *zClass,
	ph7_value *pTarget, const SyString *pName)
{
	ph7_value sName;
	ph7_value *apCtor[2];
	ph7_class_instance *pOut;
	sxi32 rc;
	PH7_MemObjInit(pCtx->pVm, &sName);
	ph7_value_string(&sName, SyStringData(pName), (int)SyStringLength(pName));
	apCtor[0] = pTarget;
	apCtor[1] = &sName;
	pOut = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);
	PH7_MemObjRelease(&sName);
	if( pOut == 0 ){
		if( rc != PH7_OK ){
			return rc;
		}
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultObject(pCtx, pOut);
}
/* A `ReflectionMethod($this->name, ...)`-shaped first argument. */
static void ReflectSelfName(ph7_context *pCtx, ph7_value *pOut)
{
	const char *zName;
	int nName;
	ReflectClassName(pCtx, &zName, &nName);
	PH7_MemObjInit(pCtx->pVm, pOut);
	ph7_value_string(pOut, zName, nName);
}
static int vm_builtin_ReflectionClass_getMethod(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	SyHashEntry *pEntry;
	const char *zName;
	int nName;
	SyString sFound;
	if( pClass == 0 || nArg < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0], &nName);
	pEntry = ReflectFindMethodEntry(pClass, zName, nName);
	if( pEntry == 0 ){
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Method %z::%.*s() does not exist", &pClass->sName, nName, zName);
	}
	/* The reported name is the DECLARED spelling, whatever case was asked for. */
	SyStringInitFromBuf(&sFound, (const char *)pEntry->pKey, pEntry->nKeyLen);
	{
		ph7_value sSelf;
		int rc;
		ReflectSelfName(pCtx, &sSelf);
		rc = ReflectResultMember(pCtx, "ReflectionMethod", &sSelf, &sFound);
		PH7_MemObjRelease(&sSelf);
		return rc;
	}
}
static int vm_builtin_ReflectionClass_getConstructor(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	SyHashEntry *pEntry;
	SyString sFound;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pClass == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* No PHP-4 class-name constructor: a method named like the class is a plain
	 * method (removed in 8.0), so this stays null without an explicit one. */
	pEntry = ReflectFindMethodEntry(pClass, "__construct", sizeof("__construct")-1);
	if( pEntry == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	SyStringInitFromBuf(&sFound, (const char *)pEntry->pKey, pEntry->nKeyLen);
	{
		ph7_value sSelf;
		int rc;
		ReflectSelfName(pCtx, &sSelf);
		rc = ReflectResultMember(pCtx, "ReflectionMethod", &sSelf, &sFound);
		PH7_MemObjRelease(&sSelf);
		return rc;
	}
}
/*
 * getMethods() / getProperties() / getReflectionConstants(): one member walk,
 * one reflector per surviving member. Each reflector is a prelude class built
 * through its constructor, and is appended through a STACK carrier so the list
 * takes its own reference rather than the creation one.
 */
static int ReflectMemberList(ph7_context *pCtx, int iKind, const char *zClass,
	int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	ph7_class_instance *pObj = ReflectClassObj(pCtx);
	ph7_value *pOut = ph7_context_new_array(pCtx);
	ph7_value sSelf;
	SySet aMembers;
	sxu32 n;
	int bFilter = (nArg > 0 && (apArg[0]->iFlags & MEMOBJ_NULL) == 0);
	sxi64 iFilter = bFilter ? ph7_value_to_int64(apArg[0]) : 0;
	if( pOut == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( pClass == 0 ){
		ph7_result_value(pCtx, pOut);
		return PH7_OK;
	}
	ReflectSelfName(pCtx, &sSelf);
	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));
	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);
	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){
		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);
		ph7_value sName, sVal;
		ph7_value *apCtor[2];
		ph7_class_instance *pRef;
		sxi32 rc;
		if( pM->iKind != iKind ){
			continue;
		}
		if( bFilter ){
			sxi64 iMods = iKind == REFLECT_MEMBER_METHOD ? ReflectMethodModifiers(pM->pMeth)
				: (iKind == REFLECT_MEMBER_CONST ? ReflectConstModifiers(pM->pAttr)
				                                 : ReflectPropModifiers(pM->pAttr));
			if( (iMods & iFilter) == 0 ){
				continue;
			}
		}
		PH7_MemObjInit(pCtx->pVm, &sName);
		ph7_value_string(&sName, SyStringData(&pM->sKey), (int)SyStringLength(&pM->sKey));
		apCtor[0] = &sSelf;
		apCtor[1] = &sName;
		pRef = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);
		PH7_MemObjRelease(&sName);
		if( pRef == 0 ){
			SySetRelease(&aMembers);
			PH7_MemObjRelease(&sSelf);
			if( rc != PH7_OK ){
				return rc;
			}
			ph7_result_value(pCtx, pOut);
			return PH7_OK;
		}
		PH7_MemObjInit(pCtx->pVm, &sVal);
		sVal.x.pOther = pRef;
		sVal.iFlags = MEMOBJ_OBJ;
		ph7_array_add_elem(pOut, 0, &sVal);   /* takes its own reference */
		PH7_ClassInstanceUnref(pRef);
	}
	SySetRelease(&aMembers);
	/* A ReflectionObject also reports the instance's own DYNAMIC properties,
	 * which no class declaration knows about. */
	if( iKind == REFLECT_MEMBER_PROP && pObj != 0 && (!bFilter || (iFilter & 1)) ){
		SyHashEntry *pEntry;
		ph7_value sTarget;
		PH7_MemObjInit(pCtx->pVm, &sTarget);
		sTarget.x.pOther = pObj;
		sTarget.iFlags = MEMOBJ_OBJ;
		SyHashResetLoopCursor(&pObj->hAttr);
		while( (pEntry = SyHashGetNextEntry(&pObj->hAttr)) != 0 ){
			VmClassAttr *pVmAttr = (VmClassAttr *)pEntry->pUserData;
			ph7_value sName, sVal;
			ph7_value *apCtor[2];
			ph7_class_instance *pRef;
			sxi32 rc;
			if( pVmAttr->pAttr == 0 || (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_DYNAMIC) == 0 ){
				continue;
			}
			PH7_MemObjInit(pCtx->pVm, &sName);
			ph7_value_string(&sName, SyStringData(&pVmAttr->pAttr->sName),
				(int)SyStringLength(&pVmAttr->pAttr->sName));
			apCtor[0] = &sTarget;
			apCtor[1] = &sName;
			pRef = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);
			PH7_MemObjRelease(&sName);
			if( pRef == 0 ){
				break;
			}
			PH7_MemObjInit(pCtx->pVm, &sVal);
			sVal.x.pOther = pRef;
			sVal.iFlags = MEMOBJ_OBJ;
			ph7_array_add_elem(pOut, 0, &sVal);
			PH7_ClassInstanceUnref(pRef);
		}
		/* sTarget borrows pObj and never took a reference: nothing to release. */
	}
	PH7_MemObjRelease(&sSelf);
	ph7_result_value(pCtx, pOut);
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_getMethods(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	return ReflectMemberList(pCtx, REFLECT_MEMBER_METHOD, "ReflectionMethod", nArg, apArg);
}
static int vm_builtin_ReflectionClass_getProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	return ReflectMemberList(pCtx, REFLECT_MEMBER_PROP, "ReflectionProperty", nArg, apArg);
}
static int vm_builtin_ReflectionClass_getReflectionConstants(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	return ReflectMemberList(pCtx, REFLECT_MEMBER_CONST, "ReflectionClassConstant", nArg, apArg);
}
static int vm_builtin_ReflectionClass_getProperty(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	ph7_class_instance *pObj = ReflectClassObj(pCtx);
	const char *zName;
	int nName, bFound = 0;
	SySet aMembers;
	sxu32 n;
	SyString sFound;
	if( pClass == 0 || nArg < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0], &nName);
	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));
	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);
	for( n = 0 ; n < SySetUsed(&aMembers) && !bFound ; n++ ){
		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);
		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zName, nName) ){
			sFound = pM->sKey;
			bFound = 1;
		}
	}
	SySetRelease(&aMembers);
	if( bFound ){
		ph7_value sSelf;
		int rc;
		ReflectSelfName(pCtx, &sSelf);
		rc = ReflectResultMember(pCtx, "ReflectionProperty", &sSelf, &sFound);
		PH7_MemObjRelease(&sSelf);
		return rc;
	}
	if( ReflectObjHasProp(pObj, zName, nName) ){
		/* A dynamic property: the reflector is built over the OBJECT, since the
		 * class declaration has no record of it. */
		ph7_value sTarget;
		SyString sName;
		int rc;
		PH7_MemObjInit(pCtx->pVm, &sTarget);
		sTarget.x.pOther = pObj;
		sTarget.iFlags = MEMOBJ_OBJ;
		SyStringInitFromBuf(&sName, zName, nName);
		rc = ReflectResultMember(pCtx, "ReflectionProperty", &sTarget, &sName);
		return rc;
	}
	return PH7_VmThrowException(pCtx, "ReflectionException",
		"Property %z::$%.*s does not exist", &pClass->sName, nName, zName);
}
static int vm_builtin_ReflectionClass_getReflectionConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	const char *zName;
	int nName, bFound = 0;
	SySet aMembers;
	sxu32 n;
	SyString sFound;
	if( pClass == 0 || nArg < 1 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0], &nName);
	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));
	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);
	for( n = 0 ; n < SySetUsed(&aMembers) && !bFound ; n++ ){
		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);
		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zName, nName) ){
			sFound = pM->sKey;
			bFound = 1;
		}
	}
	SySetRelease(&aMembers);
	if( !bFound ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	{
		ph7_value sSelf;
		int rc;
		ReflectSelfName(pCtx, &sSelf);
		rc = ReflectResultMember(pCtx, "ReflectionClassConstant", &sSelf, &sFound);
		PH7_MemObjRelease(&sSelf);
		return rc;
	}
}
/* ---- statics and defaults ---- */
/* Reading or writing a static through reflection materializes the class's
 * static table exactly as `C::$s` does, so a default that threw at the
 * declaration raises HERE. */
static sxi32 ReflectMaterializeStatics(ph7_context *pCtx, ph7_class *pClass)
{
	if( VmClassStaticDeferPending(pClass) ){
		return PH7_VmMaterializeClassStatics(pCtx->pVm, pClass);
	}
	return SXRET_OK;
}
static int vm_builtin_ReflectionClass_getStaticProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	ph7_value *pOut = ph7_context_new_array(pCtx);
	SySet aMembers;
	sxu32 n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pOut == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( pClass == 0 ){
		ph7_result_value(pCtx, pOut);
		return PH7_OK;
	}
	{
		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));
	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);
	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){
		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);
		ph7_value *pVal;
		SyHashEntry *pSlot;
		if( pM->iKind != REFLECT_MEMBER_PROP
		 || (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) == 0 ){
			continue;
		}
		/* An UNINITIALIZED typed static has no value to report, and php simply
		 * leaves it out rather than raising the read Error here. */
		pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pM->pAttr->nIdx, sizeof(sxu32));
		if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){
			continue;
		}
		pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pM->pAttr->nIdx);
		if( pVal ){
			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, pVal);
		}
	}
	SySetRelease(&aMembers);
	ph7_result_value(pCtx, pOut);
	return PH7_OK;
}
/* The declared STATIC property of this name, or NULL. */
static ph7_class_attr * ReflectStaticAttr(ph7_context *pCtx, ph7_class *pClass,
	const char *zName, int nName)
{
	SyHashEntry *pEntry;
	ph7_class_attr *pAttr;
	if( nName < 1 ){
		return 0;
	}
	pEntry = SyHashGet(&pClass->hAttr, (const void *)zName, (sxu32)nName);
	if( pEntry == 0 ){
		return 0;
	}
	pAttr = (ph7_class_attr *)pEntry->pUserData;
	SXUNUSED(pCtx);
	return (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ? pAttr : 0;
}
static int vm_builtin_ReflectionClass_getStaticPropertyValue(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	ph7_class_attr *pAttr;
	const char *zName;
	int nName;
	if( pClass == 0 || nArg < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	{
		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	zName = ph7_value_to_string(apArg[0], &nName);
	pAttr = ReflectStaticAttr(pCtx, pClass, zName, nName);
	if( pAttr == 0 ){
		if( nArg > 1 ){
			ph7_result_value(pCtx, apArg[1]);
			return PH7_OK;
		}
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Property %z::$%.*s does not exist", &pClass->sName, nName, zName);
	}
	{
		/* Uninitialized typed static: the same Error the VM raises on read */
		SyHashEntry *pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));
		ph7_value *pVal;
		if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){
			/* php says "Typed property" HERE and "Typed static property" from
			 * ReflectionProperty::getValue() and from `A::$s` itself — the
			 * three wordings were checked against the oracle, they really do
			 * differ by call site. */
			ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
			return PH7_VmThrowException(pCtx, "Error",
				"Typed property %z::$%z must not be accessed before initialization",
				&pDecl->sName, &pAttr->sName);
		}
		pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);
		if( pVal ){
			ph7_result_value(pCtx, pVal);
		}else{
			ph7_result_null(pCtx);
		}
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_setStaticPropertyValue(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	ph7_class_attr *pAttr;
	ph7_value *pSlot;
	const char *zName;
	int nName;
	if( pClass == 0 || nArg < 2 ){
		return PH7_OK;
	}
	{
		sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	zName = ph7_value_to_string(apArg[0], &nName);
	pAttr = ReflectStaticAttr(pCtx, pClass, zName, nName);
	if( pAttr == 0 ){
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Class %z does not have a property named %.*s", &pClass->sName, nName, zName);
	}
	pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);
	if( pSlot == 0 ){
		return PH7_OK;
	}
	{
		sxi32 rc = ReflectEnforceStore(pCtx, pAttr->nIdx, apArg[1]);
		if( rc != SXRET_OK ){
			return rc;
		}
	}
	PH7_MemObjStore(apArg[1], pSlot);
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_getDefaultProperties(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	ph7_value *pOut = ph7_context_new_array(pCtx);
	SySet aMembers;
	sxu32 n;
	int iPass;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pOut == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( pClass == 0 ){
		ph7_result_value(pCtx, pOut);
		return PH7_OK;
	}
	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));
	ReflectMembers(pCtx->pVm, pClass, &aMembers, 0);
	/* php reports the STATIC properties first, then the instance ones. */
	for( iPass = 0 ; iPass < 2 ; iPass++ ){
		for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){
			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);
			int bStatic;
			ph7_value sValue;
			if( pM->iKind != REFLECT_MEMBER_PROP ){
				continue;
			}
			bStatic = (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0;
			if( bStatic != (iPass == 0) ){
				continue;
			}
			if( (pM->pAttr->iFlags & PH7_CLASS_ATTR_TYPED)
			 && SySetUsed(&pM->pAttr->aByteCode) < 1 ){
				/* A TYPED property with no initializer has no default at all —
				 * it is uninitialized, and php leaves it out. An UNTYPED one
				 * without an initializer defaults to null and is listed. */
				continue;
			}
			PH7_MemObjInit(pCtx->pVm, &sValue);
			if( SySetUsed(&pM->pAttr->aByteCode) > 0 ){
				/* Same evaluation path the VM uses for omitted call arguments */
				VmLocalExec(pCtx->pVm, &pM->pAttr->aByteCode, &sValue, FALSE);
			}
			ReflectMapAddDyn(pCtx, pOut, &pM->sKey, &sValue);
			PH7_MemObjRelease(&sValue);
		}
	}
	SySetRelease(&aMembers);
	ph7_result_value(pCtx, pOut);
	return PH7_OK;
}
/* ---- attributes, extension, export ---- */
static int vm_builtin_ReflectionClass_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	const char *zName;
	int nName;
	if( pClass == 0 ){
		ph7_result_value(pCtx, ph7_context_new_array(pCtx));
		return PH7_OK;
	}
	ReflectClassName(pCtx, &zName, &nName);
	{
		ph7_value sTarget;
		int rc;
		PH7_MemObjInit(pCtx->pVm, &sTarget);
		ph7_value_string(&sTarget, zName, nName);
		/* 1 = Attribute::TARGET_CLASS */
		rc = ReflectBuildAttrs(pCtx, &pClass->aAttrs, "class", &sTarget, 0, 0, 0, 1,
			nArg, apArg);
		PH7_MemObjRelease(&sTarget);
		return rc;
	}
}
static int vm_builtin_ReflectionClass_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( ReflectClassFlag(pCtx, PH7_CLASS_INTERNAL) ){
		ph7_result_string(pCtx, "Core", sizeof("Core")-1);
	}else{
		ph7_result_bool(pCtx, 0);
	}
	return PH7_OK;
}
/*
 * The synthetic "Core" extension, which is the only one PHL has. Answered by
 * every reflector's getExtension() for an INTERNAL target.
 */
static int ReflectCoreExtension(ph7_context *pCtx)
{
	ph7_value sName;
	ph7_value *apCtor[1];
	ph7_class_instance *pExt;
	sxi32 rc;
	PH7_MemObjInit(pCtx->pVm, &sName);
	ph7_value_string(&sName, "Core", sizeof("Core")-1);
	apCtor[0] = &sName;
	pExt = ReflectConstruct(pCtx, "ReflectionExtension", 1, apCtor, &rc);
	PH7_MemObjRelease(&sName);
	if( pExt == 0 ){
		if( rc != PH7_OK ){
			return rc;
		}
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultObject(pCtx, pExt);
}
/*
 * __toString(): php's export format, still chunk 9 — a PHP function written
 * against the PUBLIC reflection API of its target, so it is called with `$this`.
 * bIndentArg adds the export family's second "" indent argument.
 */
static int ReflectExportSelf(ph7_context *pCtx, const char *zFn, int bIndentArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value sSelf, sIndent, sRes, sFn;
	ph7_value *apCall[2];
	SyString sStr;
	if( pThis == 0 ){
		ph7_result_string(pCtx, "", 0);
		return PH7_OK;
	}
	PH7_MemObjInit(pVm, &sSelf);
	PH7_MemObjInit(pVm, &sIndent);
	PH7_MemObjInit(pVm, &sRes);
	PH7_MemObjInit(pVm, &sFn);
	sSelf.x.pOther = pThis;
	sSelf.iFlags = MEMOBJ_OBJ;
	ph7_value_string(&sIndent, "", 0);
	SyStringInitFromBuf(&sStr, zFn, SyStrlen(zFn));
	PH7_MemObjInitFromString(pVm, &sFn, &sStr);
	apCall[0] = &sSelf;
	apCall[1] = &sIndent;
	if( PH7_VmCallUserFunction(pVm, &sFn, bIndentArg ? 2 : 1, apCall, &sRes) == SXRET_OK ){
		ph7_result_value(pCtx, &sRes);
	}
	PH7_MemObjRelease(&sFn);
	PH7_MemObjRelease(&sIndent);
	PH7_MemObjRelease(&sRes);
	/* sSelf borrows the receiver and never took a reference: not released. */
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectClassFlag(pCtx, PH7_CLASS_INTERNAL) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectCoreExtension(pCtx);
}
static int vm_builtin_ReflectionClass_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return ReflectExportSelf(pCtx, "__reflect_export_class", 0);
}
/* ---- lazy objects: PHL has none (§7.4) ---- */
static int ReflectNoLazy(ph7_context *pCtx, const char *zWho)
{
	return PH7_VmThrowException(pCtx, "Error",
		"%s is not supported by PHL (no lazy objects)", zWho);
}
#define REFLECT_NO_LAZY(NAME,TEXT) \
	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \
	{ \
		SXUNUSED(nArg); \
		SXUNUSED(apArg); \
		return ReflectNoLazy(pCtx, TEXT); \
	}
REFLECT_NO_LAZY(vm_builtin_ReflectionClass_newLazyGhost,"ReflectionClass::newLazyGhost()")
REFLECT_NO_LAZY(vm_builtin_ReflectionClass_newLazyProxy,"ReflectionClass::newLazyProxy()")
REFLECT_NO_LAZY(vm_builtin_ReflectionClass_resetAsLazyGhost,"ReflectionClass::resetAsLazyGhost()")
REFLECT_NO_LAZY(vm_builtin_ReflectionClass_resetAsLazyProxy,"ReflectionClass::resetAsLazyProxy()")
/* The four QUERIES answer what is true of a VM with no lazy objects, rather
 * than refusing: an object here is always initialized and never has one. */
static int vm_builtin_ReflectionClass_getLazyInitializer(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_null(pCtx);
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_passThroughObject(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	if( nArg > 0 ){
		ph7_result_value(pCtx, apArg[0]);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionClass_isUninitializedLazyObject(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx, 0);
	return PH7_OK;
}
/*
 * Reflection::getModifierNames(int $modifiers)
 *
 * php's own order and its own SWITCH: the visibility names come out of one
 * three-way choice and the set-visibility names out of another, so a mask with
 * two visibility bits set names NEITHER (which is what php answers).
 */
static int vm_builtin_Reflection_getModifierNames(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_value *pOut = ph7_context_new_array(pCtx);
	sxi64 iMods = nArg > 0 ? ph7_value_to_int64(apArg[0]) : 0;
	const char *azName[8];
	int nName = 0, n;
	if( pOut == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( iMods & 64 ){ azName[nName++] = "abstract"; }
	if( iMods & 32 ){ azName[nName++] = "final"; }
	if( iMods & 512 ){ azName[nName++] = "virtual"; }
	switch( iMods & (1|2|4) ){
	case 1: azName[nName++] = "public"; break;
	case 2: azName[nName++] = "protected"; break;
	case 4: azName[nName++] = "private"; break;
	default: break;
	}
	switch( iMods & (2048|4096) ){
	case 2048: azName[nName++] = "protected(set)"; break;
	case 4096: azName[nName++] = "private(set)"; break;
	default: break;
	}
	if( iMods & 16 ){ azName[nName++] = "static"; }
	if( iMods & 128 ){ azName[nName++] = "readonly"; }
	for( n = 0 ; n < nName ; n++ ){
		ph7_value *pName = ph7_context_new_scalar(pCtx);
		if( pName == 0 ){ break; }
		ph7_value_string(pName, azName[n], -1);
		ph7_array_add_elem(pOut, 0, pName);
	}
	ph7_result_value(pCtx, pOut);
	return PH7_OK;
}
/*
 * Declare chunk 1. Called from PH7_VmInstallReflectionLib where it used to be
 * compiled — before chunks 2 and 3, which name `Reflector` in their own
 * `implements` clauses.
 *
 * The method table is in php's own DECLARATION order, which is the order
 * ReflectionClass::getMethods() reports for these classes themselves.
 */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionClass(ph7_vm *pVm)
{
	static const PH7_NativePropDef aClassProp[] = {
		{ "name",  PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 } },
		/* PHL-only: the instance a ReflectionObject was built over. php keeps it
		 * out of sight; PHL has no hidden-slot bit yet (§7.4 (e)). */
		{ RC_OBJ,  PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
	};
	static const PH7_NativeMethodDef aClassMethod[] = {
		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionClass_clone },
		{ "__construct", PH7_MOD_PUBLIC, "object|string $objectOrClass", "",
		  vm_builtin_ReflectionClass_construct },
		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionClass_toString },
		{ "getName",       PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_getName },
		{ "isInternal",    PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_isInternal },
		{ "isUserDefined", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_isUserDefined },
		{ "isAnonymous",   PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_isAnonymous },
		{ "isInstantiable",PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_isInstantiable },
		{ "isCloneable",   PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_isCloneable },
		{ "getFileName",   PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_getFileName },
		{ "getStartLine",  PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_getStartLine },
		{ "getEndLine",    PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_getEndLine },
		{ "getDocComment", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_getDocComment },
		{ "getConstructor",PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_getConstructor },
		{ "hasMethod",     PH7_MOD_PUBLIC, "string $name", "", vm_builtin_ReflectionClass_hasMethod },
		{ "getMethod",     PH7_MOD_PUBLIC, "string $name", "", vm_builtin_ReflectionClass_getMethod },
		{ "getMethods",    PH7_MOD_PUBLIC, "?int $filter = null", "",
		  vm_builtin_ReflectionClass_getMethods },
		{ "hasProperty",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_ReflectionClass_hasProperty },
		{ "getProperty",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_ReflectionClass_getProperty },
		{ "getProperties", PH7_MOD_PUBLIC, "?int $filter = null", "",
		  vm_builtin_ReflectionClass_getProperties },
		{ "hasConstant",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_ReflectionClass_hasConstant },
		{ "getConstants",  PH7_MOD_PUBLIC, "?int $filter = null", "",
		  vm_builtin_ReflectionClass_getConstants },
		{ "getReflectionConstants", PH7_MOD_PUBLIC, "?int $filter = null", "",
		  vm_builtin_ReflectionClass_getReflectionConstants },
		{ "getConstant",   PH7_MOD_PUBLIC, "string $name", "", vm_builtin_ReflectionClass_getConstant },
		{ "getReflectionConstant", PH7_MOD_PUBLIC, "string $name", "",
		  vm_builtin_ReflectionClass_getReflectionConstant },
		{ "getInterfaces",     PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_getInterfaces },
		{ "getInterfaceNames", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_getInterfaceNames },
		{ "isInterface",       PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_isInterface },
		{ "getTraits",         PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_getTraits },
		{ "getTraitNames",     PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_getTraitNames },
		{ "getTraitAliases",   PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_getTraitAliases },
		{ "isTrait",           PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_isTrait },
		{ "isEnum",            PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClass_isEnum },
		{ "isAbstract",        PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_isAbstract },
		{ "isFinal",           PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_isFinal },
		{ "isReadOnly",        PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClass_isReadOnly },
		{ "getModifiers",      PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_getModifiers },
		{ "isInstance",        PH7_MOD_PUBLIC, "object $object", "",
		  vm_builtin_ReflectionClass_isInstance },
		{ "newInstance",       PH7_MOD_PUBLIC, "mixed ...$args", "",
		  vm_builtin_ReflectionClass_newInstance },
		{ "newInstanceWithoutConstructor", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionClass_newInstanceWithoutConstructor },
		{ "newInstanceArgs",   PH7_MOD_PUBLIC, "array $args = []", "",
		  vm_builtin_ReflectionClass_newInstanceArgs },
		{ "newLazyGhost",      PH7_MOD_PUBLIC, "callable $initializer, int $options = 0", "object",
		  vm_builtin_ReflectionClass_newLazyGhost },
		{ "newLazyProxy",      PH7_MOD_PUBLIC, "callable $factory, int $options = 0", "object",
		  vm_builtin_ReflectionClass_newLazyProxy },
		{ "resetAsLazyGhost",  PH7_MOD_PUBLIC,
		  "object $object, callable $initializer, int $options = 0", "void",
		  vm_builtin_ReflectionClass_resetAsLazyGhost },
		{ "resetAsLazyProxy",  PH7_MOD_PUBLIC,
		  "object $object, callable $factory, int $options = 0", "void",
		  vm_builtin_ReflectionClass_resetAsLazyProxy },
		{ "initializeLazyObject", PH7_MOD_PUBLIC, "object $object", "object",
		  vm_builtin_ReflectionClass_passThroughObject },
		{ "isUninitializedLazyObject", PH7_MOD_PUBLIC, "object $object", "bool",
		  vm_builtin_ReflectionClass_isUninitializedLazyObject },
		{ "markLazyObjectAsInitialized", PH7_MOD_PUBLIC, "object $object", "object",
		  vm_builtin_ReflectionClass_passThroughObject },
		{ "getLazyInitializer", PH7_MOD_PUBLIC, "object $object", "?callable",
		  vm_builtin_ReflectionClass_getLazyInitializer },
		{ "getParentClass",    PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_getParentClass },
		{ "isSubclassOf",      PH7_MOD_PUBLIC, "ReflectionClass|string $class", "",
		  vm_builtin_ReflectionClass_isSubclassOf },
		{ "getStaticProperties", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionClass_getStaticProperties },
		/* `mixed $default = ?` is the table's "optional, no default VALUE"
		 * marker — php's own shape here: isOptional() true,
		 * isDefaultValueAvailable() false. */
		{ "getStaticPropertyValue", PH7_MOD_PUBLIC, "string $name, mixed $default = ?", "",
		  vm_builtin_ReflectionClass_getStaticPropertyValue },
		{ "setStaticPropertyValue", PH7_MOD_PUBLIC, "string $name, mixed $value", "",
		  vm_builtin_ReflectionClass_setStaticPropertyValue },
		{ "getDefaultProperties", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionClass_getDefaultProperties },
		{ "isIterable",        PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_isIterable },
		{ "isIterateable",     PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_isIterable },
		{ "implementsInterface", PH7_MOD_PUBLIC, "ReflectionClass|string $interface", "",
		  vm_builtin_ReflectionClass_implementsInterface },
		{ "getExtension",      PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_getExtension },
		{ "getExtensionName",  PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_getExtensionName },
		{ "inNamespace",       PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_inNamespace },
		{ "getNamespaceName",  PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_getNamespaceName },
		{ "getShortName",      PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClass_getShortName },
		{ "getAttributes",     PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",
		  vm_builtin_ReflectionClass_getAttributes },
	};
	static const PH7_NativeConstDef aClassConst[] = {
		{ "IS_IMPLICIT_ABSTRACT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,    0, 0.0 },
		{ "IS_EXPLICIT_ABSTRACT", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64,    0, 0.0 },
		{ "IS_FINAL",             PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32,    0, 0.0 },
		{ "IS_READONLY",          PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 65536, 0, 0.0 },
		{ "SKIP_INITIALIZATION_ON_SERIALIZE", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 8, 0, 0.0 },
		{ "SKIP_DESTRUCTOR",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,    0, 0.0 },
	};
	static const PH7_NativeMethodDef aObjectMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "object $object", "",
		  vm_builtin_ReflectionObject_construct },
	};
	static const PH7_NativeMethodDef aReflectionMethod[] = {
		{ "getModifierNames", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "int $modifiers", "",
		  vm_builtin_Reflection_getModifierNames },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "Reflector", "Stringable", 0, PH7_CLASS_INTERFACE, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "ReflectionException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "Reflection", 0, 0, 0,
		  aReflectionMethod, SX_ARRAYSIZE(aReflectionMethod), 0, 0, 0, 0, 0, 0 },
		/* Uncloneable and unserializable in php too: `clone` is an Error and
		 * serialize() a catchable Exception naming the class. */
		{ "ReflectionClass", 0, "Reflector", PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aClassMethod, SX_ARRAYSIZE(aClassMethod),
		  aClassConst, SX_ARRAYSIZE(aClassConst),
		  aClassProp, SX_ARRAYSIZE(aClassProp), 0, 0 },
		{ "ReflectionObject", "ReflectionClass", 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aObjectMethod, SX_ARRAYSIZE(aObjectMethod), 0, 0, 0, 0, 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));
}
/*
 * ---------------------------------------------------------------------------
 * ReflectionFunctionAbstract, ReflectionFunction, ReflectionMethod,
 * ReflectionParameter.
 *
 * Chunk 2. Its accessors funnelled through __reflect_func_info(), a descriptor
 * ARRAY built per call from either a compiled ph7_vm_func or a declared
 * SIGNATURE STRING; a native method reads whichever of the two the target
 * actually has, through the one uniform description both already agreed on
 * (ReflectParamDesc / ReflectSigDescribe).
 *
 * Five thunks retire with the chunk: __reflect_func_info, __reflect_invoke,
 * __reflect_closure, __reflect_param_default and __reflect_param_defconst.
 * ---------------------------------------------------------------------------
 */
#define RF_CL "__cl"     /* ReflectionFunctionAbstract: the reflected Closure  */
#define RP_T  "__t"      /* ReflectionParameter: the function/class it belongs to */
#define RP_M  "__m"      /* ReflectionParameter: the method name, or null      */
#define RP_P  "__p"      /* ReflectionParameter: the position                  */

/* Everything a reflected function IS, resolved once per call. */
typedef struct ReflectFuncRef ReflectFuncRef;
struct ReflectFuncRef
{
	ph7_vm_func *pFunc;           /* compiled body (NULL for a pure C builtin) */
	ph7_user_func *pHost;         /* C builtin (NULL otherwise) */
	ph7_class *pClass;            /* class a METHOD was reached through */
	ph7_class_method *pMeth;      /* the method */
	ph7_class_instance *pClosure; /* the Closure being reflected, if any */
	const char *zSig;             /* declared parameter signature, or NULL */
	const char *zRet;             /* declared return type, or NULL */
};
/*
 * Resolve a callable into the reference, and work out WHICH of the two
 * parameter sources describes it: a declared signature string (a C builtin, a
 * native method, or an embedded-PHP builtin declared argless over
 * func_get_args()) wins over the compiled argument list, exactly as
 * ReflectSigFixup made it win in the descriptor.
 */
static int ReflectFuncFill(ph7_context *pCtx, ph7_value *pTarget, ph7_value *pMethodArg,
	ReflectFuncRef *pOut)
{
	SyZero(pOut, sizeof(*pOut));
	pOut->pFunc = ReflectResolveCallable(pCtx, pTarget, pMethodArg,
		&pOut->pClass, &pOut->pMeth, &pOut->pHost, &pOut->pClosure);
	if( pOut->pFunc == 0 && pOut->pHost == 0 ){
		return 0;
	}
	if( pOut->pFunc == 0 ){
		pOut->zSig = pOut->pHost->zSig;
		pOut->zRet = pOut->pHost->zRet;
		return 1;
	}
	if( (pOut->pFunc->iFlags & VM_FUNC_NATIVE) && pOut->pFunc->pNative ){
		pOut->zSig = pOut->pFunc->pNative->zSig;
		if( SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){
			pOut->zRet = pOut->pFunc->pNative->zRet;
		}
	}else if( (pOut->pFunc->iFlags & VM_FUNC_INTERNAL)
	 && SySetUsed(&pOut->pFunc->aArgs) == 0 && pOut->pMeth == 0 ){
		const char *zRet = 0;
		pOut->zSig = PH7_VmBuiltinSigLookup(SyStringData(&pOut->pFunc->sName),
			SyStringLength(&pOut->pFunc->sName), &zRet);
		if( zRet && SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){
			pOut->zRet = zRet;
		}
	}
	if( pOut->zSig && pOut->zSig[0] == '\0' ){
		/* A declared-EMPTY signature really is "no parameters", not "undescribed". */
		return 1;
	}
	return 1;
}
/* Is this receiver a ReflectionMethod (rather than a ReflectionFunction)? */
static int ReflectIsMethodReflector(ph7_context *pCtx, ph7_class_instance *pThis)
{
	ph7_class *pRM;
	if( pThis == 0 ){
		return 0;
	}
	pRM = PH7_VmExtractClass(pCtx->pVm, "ReflectionMethod", sizeof("ReflectionMethod")-1, FALSE, 0);
	return pRM != 0 && PH7_VmInstanceOf(pThis->pClass, pRM);
}
/*
 * The function `$this` reflects. A ReflectionFunction built over a Closure keeps
 * the object in `__cl` and resolves through it (its `$__fn` is a lambda name no
 * function table holds); a ReflectionMethod resolves ($this->class, $this->name).
 */
static int ReflectFuncOfThis(ph7_context *pCtx, ReflectFuncRef *pOut)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pClo;
	ph7_value sTarget, sMethod;
	const char *zName, *zClass;
	int nName, nClass, rc;
	SyZero(pOut, sizeof(*pOut));
	if( pThis == 0 ){
		return 0;
	}
	PH7_MemObjInit(pCtx->pVm, &sTarget);
	PH7_MemObjInit(pCtx->pVm, &sMethod);
	pClo = PH7_NativeAttrObj(pThis, RF_CL);
	PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	if( pClo ){
		sTarget.x.pOther = pClo;
		sTarget.iFlags = MEMOBJ_OBJ;
	}else if( ReflectIsMethodReflector(pCtx, pThis) ){
		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);
		ph7_value_string(&sTarget, zClass, nClass);
		ph7_value_string(&sMethod, zName, nName);
	}else{
		ph7_value_string(&sTarget, zName, nName);
	}
	rc = ReflectFuncFill(pCtx, &sTarget, (sMethod.iFlags & MEMOBJ_STRING) ? &sMethod : 0, pOut);
	/* sTarget only ever BORROWS the closure; releasing it would unref twice. */
	if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){
		PH7_MemObjRelease(&sTarget);
	}
	PH7_MemObjRelease(&sMethod);
	return rc;
}
/* How many parameters the target declares. */
static int ReflectParamCount(const ReflectFuncRef *pRef)
{
	if( pRef->zSig ){
		return ReflectSigPart(pRef->zSig, (int)SyStrlen(pRef->zSig), -1, 0, 0);
	}
	return pRef->pFunc ? (int)SySetUsed(&pRef->pFunc->aArgs) : 0;
}
/* Describe the iPos-th parameter. Answers 0 when there is none. */
static int ReflectParamAt(const ReflectFuncRef *pRef, int iPos, ReflectParamDesc *pOut)
{
	SyZero(pOut, sizeof(*pOut));
	if( iPos < 0 ){
		return 0;
	}
	if( pRef->zSig ){
		const char *zPart = 0;
		int nPart = 0, nTotal;
		nTotal = ReflectSigPart(pRef->zSig, (int)SyStrlen(pRef->zSig), iPos, &zPart, &nPart);
		if( iPos >= nTotal || zPart == 0 ){
			return 0;
		}
		ReflectSigDescribe(zPart, nPart, iPos, pOut);
		return 1;
	}
	if( pRef->pFunc == 0 ){
		return 0;
	}
	{
		ph7_vm_func_arg *pArg = (ph7_vm_func_arg *)SySetAt(&pRef->pFunc->aArgs, (sxu32)iPos);
		if( pArg == 0 ){
			return 0;
		}
		pOut->iPos = iPos;
		pOut->sName = pArg->sName;
		pOut->bByRef = (pArg->iFlags & VM_FUNC_ARG_BY_REF) != 0;
		pOut->bVariadic = (pArg->iFlags & VM_FUNC_ARG_VARIADIC) != 0;
		/* The compiler never sets ARG_HAS_DEF; a default IS compiled byte-code
		 * (the same test the OP_CALL default-value path uses). */
		pOut->bHasDef = SySetUsed(&pArg->aByteCode) > 0;
		pOut->bNullable = (pArg->iFlags & VM_FUNC_ARG_NULLABLE) != 0;
		pOut->bPromoted = (pArg->iFlags & VM_FUNC_ARG_PROMOTED) != 0;
		pOut->bOptional = pOut->bVariadic || pOut->bHasDef;
		pOut->sType = pArg->sTypeName;
		pOut->pArg = pArg;
		return 1;
	}
}
/* The declaring class php reports for a reflected METHOD. */
static ph7_class * ReflectFuncDeclClass(const ReflectFuncRef *pRef)
{
	if( pRef->pMeth == 0 || pRef->pClass == 0 ){
		return 0;
	}
	return ReflectMethodDeclClass(pRef->pClass, pRef->pMeth);
}
/* The declared return type TEXT, or 0. */
static int ReflectFuncRetText(const ReflectFuncRef *pRef, const char **pz, int *pn)
{
	if( pRef->zRet && pRef->zRet[0] ){
		*pz = pRef->zRet;
		*pn = (int)SyStrlen(pRef->zRet);
		return 1;
	}
	if( pRef->pFunc == 0 ){
		return 0;
	}
	if( SyStringLength(&pRef->pFunc->sReturnTypeName) > 0 ){
		*pz = SyStringData(&pRef->pFunc->sReturnTypeName);
		*pn = (int)SyStringLength(&pRef->pFunc->sReturnTypeName);
		return 1;
	}
	/* The type-text renderer omits void/never atoms (compile.c notes the root fix
	 * belongs there); name them here for getReturnType(). */
	if( pRef->pFunc->nReturnType == MEMOBJ_VOID ){
		*pz = "void";
		*pn = sizeof("void")-1;
		return 1;
	}
	if( pRef->pFunc->nReturnType == MEMOBJ_NEVER ){
		*pz = "never";
		*pn = sizeof("never")-1;
		return 1;
	}
	return 0;
}
/* Is the reflected function internal (a C builtin or an embedded-chunk one)? */
static int ReflectFuncIsInternal(const ReflectFuncRef *pRef)
{
	if( pRef->pHost ){
		return 1;
	}
	return pRef->pFunc != 0 && (pRef->pFunc->iFlags & VM_FUNC_INTERNAL) != 0;
}
/* Does this target carry a #[\Deprecated] attribute? */
static int ReflectHasDeprecated(SySet *pAttrs)
{
	ph7_attribute *aA = (ph7_attribute *)SySetBasePtr(pAttrs);
	sxu32 n;
	for( n = 0 ; n < SySetUsed(pAttrs) ; n++ ){
		if( SyStringLength(&aA[n].sName) == sizeof("deprecated")-1
		 && SyStrnicmp(SyStringData(&aA[n].sName), "deprecated", sizeof("deprecated")-1) == 0 ){
			return 1;
		}
	}
	return 0;
}
/* Resolve `$this`, or answer a default when the target has gone missing. */
#define REFLECT_FUNC_OR(REF,STMT) \
	if( !ReflectFuncOfThis(pCtx, &(REF)) ){ STMT; return PH7_OK; }

/* ---- ReflectionFunctionAbstract ---- */
static int vm_builtin_ReflectionFunc_clone(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = "";
	int nName = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	}
	ph7_result_string(pCtx, zName, nName);
	return PH7_OK;
}
/* The `name` slot's namespace split — the same three answers ReflectionClass gives. */
static int ReflectFuncNamePart(ph7_context *pCtx, int iWhat)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = "";
	int nName = 0, iCut;
	if( pThis ){
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	}
	iCut = ReflectNsCut(zName, nName);
	if( iWhat == 0 ){
		ph7_result_bool(pCtx, iCut >= 0);
	}else if( iWhat == 1 ){
		ph7_result_string(pCtx, zName, iCut < 0 ? 0 : iCut);
	}else if( iCut < 0 ){
		ph7_result_string(pCtx, zName, nName);
	}else{
		ph7_result_string(pCtx, &zName[iCut+1], nName - iCut - 1);
	}
	return PH7_OK;
}
#define REFLECT_FUNC_NAMEPART(NAME,WHAT) \
	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \
	{ \
		SXUNUSED(nArg); \
		SXUNUSED(apArg); \
		return ReflectFuncNamePart(pCtx, WHAT); \
	}
REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_inNamespace, 0)
REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_getNamespaceName, 1)
REFLECT_FUNC_NAMEPART(vm_builtin_ReflectionFunc_getShortName, 2)

static int vm_builtin_ReflectionFunc_isClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	int bAnon = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))
	if( sRef.pFunc ){
		/* A capture-free `function(){}` compiles without the CLOSURE flag but
		 * still carries the synthesized "[lambda_N]" / "[closure_N]" name. */
		bAnon = (sRef.pFunc->iFlags & VM_FUNC_CLOSURE) != 0;
		if( !bAnon && SyStringLength(&sRef.pFunc->sName) > 9
		 && (SyMemcmp(SyStringData(&sRef.pFunc->sName), "[lambda_", 8) == 0
		  || SyMemcmp(SyStringData(&sRef.pFunc->sName), "[closure_", 9) == 0) ){
			bAnon = 1;
		}
	}
	ph7_result_bool(pCtx, bAnon);
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_isDeprecated(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))
	ph7_result_bool(pCtx, sRef.pFunc != 0 && ReflectHasDeprecated(&sRef.pFunc->aAttrs));
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_isInternal(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))
	ph7_result_bool(pCtx, ReflectFuncIsInternal(&sRef));
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_isUserDefined(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))
	ph7_result_bool(pCtx, !ReflectFuncIsInternal(&sRef));
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_isGenerator(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))
	ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_GENERATOR) != 0);
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_isVariadic(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	ReflectParamDesc sDesc;
	int n, nTotal, bVariadic = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))
	nTotal = ReflectParamCount(&sRef);
	for( n = 0 ; n < nTotal ; n++ ){
		if( ReflectParamAt(&sRef, n, &sDesc) && sDesc.bVariadic ){
			bVariadic = 1;
			break;
		}
	}
	ph7_result_bool(pCtx, bVariadic);
	return PH7_OK;
}
/* isStatic(): a METHOD's own staticness, a closure's `static function () {}`. */
static int vm_builtin_ReflectionFunc_isStatic(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))
	if( sRef.pMeth ){
		ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0);
	}else{
		ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_STATIC_CL) != 0);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_returnsReference(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))
	ph7_result_bool(pCtx, sRef.pFunc != 0 && (sRef.pFunc->iFlags & VM_FUNC_REF_RETURN) != 0);
	return PH7_OK;
}
/* The Closure instance whose captured state the three getClosure* read. */
static ph7_value * ReflectClosureAttr(ReflectFuncRef *pRef, const char *zName)
{
	SyString sAttr;
	if( pRef->pClosure == 0 ){
		return 0;
	}
	SyStringInitFromBuf(&sAttr, zName, SyStrlen(zName));
	return PH7_ClassInstanceFetchAttr(pRef->pClosure, &sAttr);
}
static int vm_builtin_ReflectionFunc_getClosureThis(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	ph7_value *pAttr;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))
	pAttr = ReflectClosureAttr(&sRef, "__this");
	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){
		ph7_result_value(pCtx, pAttr);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_getClosureScopeClass(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	ph7_value *pAttr;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))
	pAttr = ReflectClosureAttr(&sRef, "__scope");
	if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){
		return ReflectResultClassOf(pCtx, PH7_VmExtractClass(pCtx->pVm,
			(const char *)SyBlobData(&pAttr->sBlob), SyBlobLength(&pAttr->sBlob), FALSE, 0));
	}
	pAttr = ReflectClosureAttr(&sRef, "__this");
	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){
		return ReflectResultClassOf(pCtx, ((ph7_class_instance *)pAttr->x.pOther)->pClass);
	}
	ph7_result_null(pCtx);
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_getClosureUsedVariables(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	ph7_value *pOut = ph7_context_new_array(pCtx);
	ph7_vm_func_closure_env *aEnv;
	sxu32 n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pOut == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( !ReflectFuncOfThis(pCtx, &sRef) || sRef.pClosure == 0 || sRef.pFunc == 0 ){
		ph7_result_value(pCtx, pOut);
		return PH7_OK;
	}
	/* use(...) imports; the implicit auto-captured $this is flagged IGNORE */
	aEnv = (ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);
	for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; n++ ){
		if( aEnv[n].iFlags & VM_FUNC_ARG_IGNORE ){
			continue;
		}
		if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1
		 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){
			continue;
		}
		if( (aEnv[n].iFlags & VM_FUNC_ARG_BY_REF) && aEnv[n].nIdx != SXU32_HIGH ){
			/* Captured by reference: report the slot's live value */
			ph7_value *pLive = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, aEnv[n].nIdx);
			ReflectMapAddDyn(pCtx, pOut, &aEnv[n].sName, pLive ? pLive : &aEnv[n].sValue);
			continue;
		}
		ReflectMapAddDyn(pCtx, pOut, &aEnv[n].sName, &aEnv[n].sValue);
	}
	ph7_result_value(pCtx, pOut);
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))
	if( sRef.pFunc && SyStringLength(&sRef.pFunc->sDoc) > 0 ){
		ph7_result_string(pCtx, SyStringData(&sRef.pFunc->sDoc), (int)SyStringLength(&sRef.pFunc->sDoc));
	}else{
		ph7_result_bool(pCtx, 0);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_getFileName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))
	if( sRef.pFunc && SyStringLength(&sRef.pFunc->sFile) > 0 ){
		ph7_result_string(pCtx, SyStringData(&sRef.pFunc->sFile), (int)SyStringLength(&sRef.pFunc->sFile));
	}else{
		ph7_result_bool(pCtx, 0);
	}
	return PH7_OK;
}
static int ReflectFuncLine(ph7_context *pCtx, int bEnd)
{
	ReflectFuncRef sRef;
	if( !ReflectFuncOfThis(pCtx, &sRef) || ReflectFuncIsInternal(&sRef) || sRef.pFunc == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	ph7_result_int64(pCtx, (sxi64)(bEnd ? sRef.pFunc->nEndLine : sRef.pFunc->nLine));
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_getStartLine(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return ReflectFuncLine(pCtx, 0);
}
static int vm_builtin_ReflectionFunc_getEndLine(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return ReflectFuncLine(pCtx, 1);
}
static int vm_builtin_ReflectionFunc_hasReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	const char *z;
	int n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))
	ph7_result_bool(pCtx, ReflectFuncRetText(&sRef, &z, &n));
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_getReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	const char *z;
	int n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))
	if( !ReflectFuncRetText(&sRef, &z, &n) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultObject(pCtx, ReflectMakeType(pCtx, z, n));
}
/* php's TENTATIVE return types are an internal-stub concept PHL has no source
 * of, so both answers are the "none" ones. */
static int vm_builtin_ReflectionFunc_hasTentativeReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx, 0);
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_getTentativeReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_null(pCtx);
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_getNumberOfParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_int(pCtx, 0))
	if( sRef.pHost && sRef.zSig == 0 ){
		/* An UNDESCRIBED builtin: the arity table is all there is. */
		ph7_result_int64(pCtx, (sxi64)sRef.pHost->nMinArg);
		return PH7_OK;
	}
	ph7_result_int(pCtx, ReflectParamCount(&sRef));
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_getNumberOfRequiredParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	ReflectParamDesc sDesc;
	int n, nTotal, nReq = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_int(pCtx, 0))
	if( sRef.pHost && sRef.zSig == 0 ){
		ph7_result_int64(pCtx, (sxi64)sRef.pHost->nMinArg);
		return PH7_OK;
	}
	nTotal = ReflectParamCount(&sRef);
	for( n = nTotal ; n > 0 ; n-- ){
		if( ReflectParamAt(&sRef, n - 1, &sDesc) && !sDesc.bVariadic && !sDesc.bOptional ){
			nReq = n;
			break;
		}
	}
	ph7_result_int(pCtx, nReq);
	return PH7_OK;
}
/* The (target, method) pair a ReflectionParameter is built over. */
static void ReflectFuncParamSpec(ph7_context *pCtx, ph7_value *pSpec)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pClo;
	const char *zName, *zClass;
	int nName, nClass;
	PH7_MemObjInit(pCtx->pVm, pSpec);
	if( pThis == 0 ){
		return;
	}
	pClo = PH7_NativeAttrObj(pThis, RF_CL);
	if( pClo ){
		pSpec->x.pOther = pClo;
		pSpec->iFlags = MEMOBJ_OBJ;
		return;
	}
	PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	if( ReflectIsMethodReflector(pCtx, pThis) ){
		/* `[class, method]`, which ReflectionParameter's constructor accepts */
		ph7_value *pList = ph7_context_new_array(pCtx);
		ph7_value *pA = ph7_context_new_scalar(pCtx);
		ph7_value *pB = ph7_context_new_scalar(pCtx);
		if( pList == 0 || pA == 0 || pB == 0 ){
			return;
		}
		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);
		ph7_value_string(pA, zClass, nClass);
		ph7_value_string(pB, zName, nName);
		ph7_array_add_elem(pList, 0, pA);
		ph7_array_add_elem(pList, 0, pB);
		PH7_MemObjStore(pList, pSpec);
		return;
	}
	ph7_value_string(pSpec, zName, nName);
}
static int vm_builtin_ReflectionFunc_getParameters(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	ph7_value *pOut = ph7_context_new_array(pCtx);
	ph7_value sSpec;
	int n, nTotal;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pOut == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( !ReflectFuncOfThis(pCtx, &sRef) ){
		ph7_result_value(pCtx, pOut);
		return PH7_OK;
	}
	nTotal = ReflectParamCount(&sRef);
	ReflectFuncParamSpec(pCtx, &sSpec);
	for( n = 0 ; n < nTotal ; n++ ){
		ph7_value sPos, sVal;
		ph7_value *apCtor[2];
		ph7_class_instance *pParam;
		sxi32 rc;
		PH7_MemObjInit(pCtx->pVm, &sPos);
		ph7_value_int(&sPos, n);
		apCtor[0] = &sSpec;
		apCtor[1] = &sPos;
		pParam = ReflectConstruct(pCtx, "ReflectionParameter", 2, apCtor, &rc);
		PH7_MemObjRelease(&sPos);
		if( pParam == 0 ){
			break;
		}
		PH7_MemObjInit(pCtx->pVm, &sVal);
		sVal.x.pOther = pParam;
		sVal.iFlags = MEMOBJ_OBJ;
		ph7_array_add_elem(pOut, 0, &sVal);   /* takes its own reference */
		PH7_ClassInstanceUnref(pParam);
	}
	if( (sSpec.iFlags & MEMOBJ_OBJ) == 0 ){
		PH7_MemObjRelease(&sSpec);
	}
	ph7_result_value(pCtx, pOut);
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_getStaticVariables(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	ph7_value *pOut = ph7_context_new_array(pCtx);
	ph7_vm_func_static_var *aStatic;
	sxu32 n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pOut == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( !ReflectFuncOfThis(pCtx, &sRef) || sRef.pFunc == 0 ){
		ph7_result_value(pCtx, pOut);
		return PH7_OK;
	}
	/* Current value when the slot was initialized (first call), otherwise the
	 * evaluated default — php's getStaticVariables initializes on demand and
	 * reports the same values. */
	aStatic = (ph7_vm_func_static_var *)SySetBasePtr(&sRef.pFunc->aStatic);
	for( n = 0 ; n < SySetUsed(&sRef.pFunc->aStatic) ; n++ ){
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
		ReflectMapAddDyn(pCtx, pOut, &aStatic[n].sName, pVal);
		if( bScratch ){
			PH7_MemObjRelease(&sScratch);
		}
	}
	ph7_result_value(pCtx, pOut);
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))
	if( ReflectFuncIsInternal(&sRef) ){
		ph7_result_string(pCtx, "Core", sizeof("Core")-1);
	}else{
		ph7_result_bool(pCtx, 0);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))
	if( !ReflectFuncIsInternal(&sRef) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectCoreExtension(pCtx);
}
static int vm_builtin_ReflectionFunc_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ReflectFuncRef sRef;
	int rc;
	if( pThis == 0 || !ReflectFuncOfThis(pCtx, &sRef) || sRef.pFunc == 0 ){
		ph7_result_value(pCtx, ph7_context_new_array(pCtx));
		return PH7_OK;
	}
	if( sRef.pMeth ){
		/* 4 = Attribute::TARGET_METHOD */
		ph7_value sTarget;
		const char *zClass, *zName;
		int nClass, nName;
		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
		PH7_MemObjInit(pCtx->pVm, &sTarget);
		ph7_value_string(&sTarget, zClass, nClass);
		rc = ReflectBuildAttrs(pCtx, &sRef.pFunc->aAttrs, "method", &sTarget,
			zName, nName, 0, 4, nArg, apArg);
		PH7_MemObjRelease(&sTarget);
		return rc;
	}
	{
		/* 2 = Attribute::TARGET_FUNCTION */
		ph7_value sTarget;
		ReflectFuncParamSpec(pCtx, &sTarget);
		rc = ReflectBuildAttrs(pCtx, &sRef.pFunc->aAttrs, "fn", &sTarget, 0, 0, 0, 2,
			nArg, apArg);
		if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){
			PH7_MemObjRelease(&sTarget);
		}
		return rc;
	}
}
/* __toString(): php's export format, still chunk 9. */
static int vm_builtin_ReflectionFunc_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return ReflectExportSelf(pCtx, "__reflect_export_fnabs", 1);
}
/* ---- ReflectionFunction ---- */
/* ReflectionFunction::__construct(Closure|string $function) */
static int vm_builtin_ReflectionFunction_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ReflectFuncRef sRef;
	ph7_class_instance *pClo;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	pClo = ReflectValueClosure(pVm, apArg[0]);
	if( !ReflectFuncFill(pCtx, apArg[0], 0, &sRef) ){
		const char *zName;
		int nName;
		if( pClo ){
			/* A Closure whose body no table holds: still a valid reflection
			 * target, so record it and let the accessors answer emptily. */
			PH7_NativeSetAttrObj(pVm, pThis, RF_CL, pClo);
			return PH7_OK;
		}
		zName = ph7_value_to_string(apArg[0], &nName);
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Function %.*s() does not exist", nName, zName);
	}
	if( pClo ){
		PH7_NativeSetAttrObj(pVm, pThis, RF_CL, pClo);
	}
	if( sRef.pFunc ){
		int bAnon = (sRef.pFunc->iFlags & VM_FUNC_CLOSURE) != 0;
		if( !bAnon && SyStringLength(&sRef.pFunc->sName) > 9
		 && (SyMemcmp(SyStringData(&sRef.pFunc->sName), "[lambda_", 8) == 0
		  || SyMemcmp(SyStringData(&sRef.pFunc->sName), "[closure_", 9) == 0) ){
			bAnon = 1;
		}
		if( bAnon ){
			/* php 8.4 names an anonymous function `{closure:FILE:LINE}` */
			char zBuf[512];
			const char *zFile = SyStringLength(&sRef.pFunc->sFile) > 0
				? SyStringData(&sRef.pFunc->sFile) : "";
			int nFile = SyStringLength(&sRef.pFunc->sFile) > 0
				? (int)SyStringLength(&sRef.pFunc->sFile) : 0;
			int nBuf = SyBufferFormat(zBuf, sizeof(zBuf), "{closure:%.*s:%u}",
				nFile, zFile, sRef.pFunc->nLine);
			PH7_NativeSetAttrStr(pVm, pThis, "name", zBuf, nBuf);
			if( pClo == 0 ){
				/* Named by its lambda name: keep a Closure so the accessors can
				 * still reach the captured scope. */
				PH7_NativeSetAttrObj(pVm, pThis, RF_CL,
					PH7_VmNewClosure(pVm, &sRef.pFunc->sName, 0, 0));
			}
			return PH7_OK;
		}
		PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sRef.pFunc->sName),
			(int)SyStringLength(&sRef.pFunc->sName));
		return PH7_OK;
	}
	PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sRef.pHost->sName),
		(int)SyStringLength(&sRef.pHost->sName));
	return PH7_OK;
}
static int vm_builtin_ReflectionFunction_isAnonymous(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	return vm_builtin_ReflectionFunc_isClosure(pCtx, nArg, apArg);
}
static int vm_builtin_ReflectionFunction_isDisabled(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx, 0);
	return PH7_OK;
}
/* The callable a ReflectionFunction invokes: the Closure it holds, or its name. */
static void ReflectFunctionCallable(ph7_context *pCtx, ph7_value *pOut)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pClo;
	const char *zName = "";
	int nName = 0;
	PH7_MemObjInit(pCtx->pVm, pOut);
	if( pThis == 0 ){
		return;
	}
	pClo = PH7_NativeAttrObj(pThis, RF_CL);
	if( pClo ){
		pOut->x.pOther = pClo;
		pOut->iFlags = MEMOBJ_OBJ;
		return;
	}
	PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	ph7_value_string(pOut, zName, nName);
}
static int ReflectFunctionInvoke(ph7_context *pCtx, int nCall, ph7_value **apCall)
{
	ph7_value sTarget, sResult;
	sxi32 rc;
	ReflectFunctionCallable(pCtx, &sTarget);
	PH7_MemObjInit(pCtx->pVm, &sResult);
	sResult.nIdx = SXU32_HIGH;
	rc = PH7_VmCallUserFunction(pCtx->pVm, &sTarget, nCall, apCall, &sResult);
	if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){
		PH7_MemObjRelease(&sTarget);
	}
	if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){
		PH7_MemObjRelease(&sResult);
		return rc;
	}
	ph7_result_value(pCtx, &sResult);
	PH7_MemObjRelease(&sResult);
	return PH7_OK;
}
static int vm_builtin_ReflectionFunction_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	return ReflectFunctionInvoke(pCtx, nArg, apArg);
}
static int vm_builtin_ReflectionFunction_invokeArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SySet aCall;
	int rc;
	SySetInit(&aCall, &pCtx->pVm->sAllocator, sizeof(ph7_value *));
	if( nArg > 0 ){
		ReflectCollectArgs(pCtx, apArg[0], &aCall, 0);
	}
	rc = ReflectFunctionInvoke(pCtx, (int)SySetUsed(&aCall), (ph7_value **)SySetBasePtr(&aCall));
	SySetRelease(&aCall);
	return rc;
}
static int vm_builtin_ReflectionFunction_getClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pClo;
	const char *zName = "";
	int nName = 0;
	SyString sName;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pClo = PH7_NativeAttrObj(pThis, RF_CL);
	if( pClo ){
		/* Already a Closure: hand the same instance back */
		return ReflectResultExistingObject(pCtx, pClo);
	}
	PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	SyStringInitFromBuf(&sName, zName, nName);
	return ReflectResultObject(pCtx, PH7_VmNewClosure(pCtx->pVm, &sName, 0, 0));
}
/* ---- ReflectionMethod ---- */
/* ReflectionMethod::__construct(object|string $objectOrMethod, ?string $method = null) */
static int vm_builtin_ReflectionMethod_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class *pClass;
	SyHashEntry *pEntry;
	ph7_value sClass, sMethod;
	const char *zMethod;
	int nMethod, rc = PH7_OK;
	if( pThis == 0 || nArg < 1 ){
		return PH7_OK;
	}
	PH7_MemObjInit(pVm, &sClass);
	PH7_MemObjInit(pVm, &sMethod);
	if( nArg < 2 || ph7_value_is_null(apArg[1]) ){
		/* One-argument form: "Class::method" */
		const char *zSpec;
		int nSpec, iSep = -1, k;
		if( (apArg[0]->iFlags & MEMOBJ_STRING) == 0 ){
			PH7_MemObjRelease(&sClass);
			PH7_MemObjRelease(&sMethod);
			return PH7_VmThrowException(pCtx, "ReflectionException",
				"The parameter class is expected to be either a string or an object");
		}
		zSpec = ph7_value_to_string(apArg[0], &nSpec);
		for( k = 0 ; k + 1 < nSpec ; k++ ){
			if( zSpec[k] == ':' && zSpec[k+1] == ':' ){
				iSep = k;
				break;
			}
		}
		if( iSep < 0 ){
			PH7_MemObjRelease(&sClass);
			PH7_MemObjRelease(&sMethod);
			return PH7_VmThrowException(pCtx, "ReflectionException",
				"Invalid method name %.*s", nSpec, zSpec);
		}
		/* php 8.3 deprecated the one-argument spelling in favour of the static
		 * factory. createFromMethodName() splits the name ITSELF and constructs
		 * with two, so it does not trip this. */
		PH7_VmThrowError(pVm, 0, E_DEPRECATED,
			"Calling ReflectionMethod::__construct() with 1 argument is deprecated, "
			"use ReflectionMethod::createFromMethodName() instead");
		ph7_value_string(&sClass, zSpec, iSep);
		ph7_value_string(&sMethod, &zSpec[iSep+2], nSpec - iSep - 2);
	}else{
		PH7_MemObjStore(apArg[0], &sClass);
		PH7_MemObjStore(apArg[1], &sMethod);
	}
	pClass = ReflectResolveClass(pVm, &sClass);
	if( pClass == 0 ){
		const char *zName;
		int nName;
		zName = ph7_value_to_string(&sClass, &nName);
		rc = PH7_VmThrowException(pCtx, "ReflectionException",
			"Class \"%.*s\" does not exist", nName, zName);
		goto Done;
	}
	zMethod = ph7_value_to_string(&sMethod, &nMethod);
	pEntry = ReflectFindMethodEntry(pClass, zMethod, nMethod);
	if( pEntry == 0 ){
		rc = PH7_VmThrowException(pCtx, "ReflectionException",
			"Method %z::%.*s() does not exist", &pClass->sName, nMethod, zMethod);
		goto Done;
	}
	{
		/* php's $class is the DECLARING class, not the one the lookup went
		 * through: `new ReflectionMethod('Kid','bm')` on an inherited method
		 * reports Base. */
		ph7_class *pDecl = ReflectMethodDeclClass(pClass, (ph7_class_method *)pEntry->pUserData);
		PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pDecl->sName),
			(int)SyStringLength(&pDecl->sName));
	}
	/* The DECLARED spelling, whatever case was asked for. */
	PH7_NativeSetAttrStr(pVm, pThis, "name", (const char *)pEntry->pKey, (int)pEntry->nKeyLen);
Done:
	PH7_MemObjRelease(&sClass);
	PH7_MemObjRelease(&sMethod);
	return rc;
}
/* ReflectionMethod::createFromMethodName(string $method): static */
static int vm_builtin_ReflectionMethod_createFromMethodName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pOut;
	ph7_value sClass, sMethod;
	ph7_value *apCtor[2];
	const char *zSpec;
	int nSpec, iSep = -1, k;
	sxi32 rc;
	if( nArg < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Split here rather than letting the constructor do it: the one-argument
	 * constructor is the DEPRECATED spelling this factory exists to replace. */
	zSpec = ph7_value_to_string(apArg[0], &nSpec);
	for( k = 0 ; k + 1 < nSpec ; k++ ){
		if( zSpec[k] == ':' && zSpec[k+1] == ':' ){
			iSep = k;
			break;
		}
	}
	if( iSep < 0 ){
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Invalid method name %.*s", nSpec, zSpec);
	}
	PH7_MemObjInit(pCtx->pVm, &sClass);
	PH7_MemObjInit(pCtx->pVm, &sMethod);
	ph7_value_string(&sClass, zSpec, iSep);
	ph7_value_string(&sMethod, &zSpec[iSep+2], nSpec - iSep - 2);
	apCtor[0] = &sClass;
	apCtor[1] = &sMethod;
	pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);
	PH7_MemObjRelease(&sClass);
	PH7_MemObjRelease(&sMethod);
	if( pOut == 0 ){
		if( rc != PH7_OK ){
			return rc;
		}
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultObject(pCtx, pOut);
}
/* The visibility/modifier predicates, all off the method record. */
static int ReflectMethodFlag(ph7_context *pCtx, int iWhat)
{
	ReflectFuncRef sRef;
	if( !ReflectFuncOfThis(pCtx, &sRef) || sRef.pMeth == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	switch( iWhat ){
	case 0: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PUBLIC); break;
	case 1: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PRIVATE); break;
	case 2: ph7_result_bool(pCtx, sRef.pMeth->iProtection == PH7_CLASS_PROT_PROTECTED); break;
	case 3: ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) != 0); break;
	default: ph7_result_bool(pCtx, (sRef.pMeth->iFlags & PH7_CLASS_ATTR_FINAL) != 0); break;
	}
	return PH7_OK;
}
#define REFLECT_METHOD_FLAG(NAME,WHAT) \
	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \
	{ \
		SXUNUSED(nArg); \
		SXUNUSED(apArg); \
		return ReflectMethodFlag(pCtx, WHAT); \
	}
REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isPublic, 0)
REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isPrivate, 1)
REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isProtected, 2)
REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isAbstract, 3)
REFLECT_METHOD_FLAG(vm_builtin_ReflectionMethod_isFinal, 4)

/* isConstructor()/isDestructor() read the NAME, not the record: a trait
 * `use T { m as __construct; }` alias is the constructor under that key. */
static int ReflectMethodNameIs(ph7_context *pCtx, const char *zWant, int nWant)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = "";
	int nName = 0;
	if( pThis ){
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	}
	ph7_result_bool(pCtx, nName == nWant && SyStrnicmp(zName, zWant, (sxu32)nWant) == 0);
	return PH7_OK;
}
static int vm_builtin_ReflectionMethod_isConstructor(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return ReflectMethodNameIs(pCtx, "__construct", sizeof("__construct")-1);
}
static int vm_builtin_ReflectionMethod_isDestructor(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return ReflectMethodNameIs(pCtx, "__destruct", sizeof("__destruct")-1);
}
static int vm_builtin_ReflectionMethod_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectFuncOfThis(pCtx, &sRef) || sRef.pMeth == 0 ){
		ph7_result_int(pCtx, 0);
		return PH7_OK;
	}
	ph7_result_int64(pCtx, ReflectMethodModifiers(sRef.pMeth));
	return PH7_OK;
}
static int vm_builtin_ReflectionMethod_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectFuncOfThis(pCtx, &sRef) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultClassOf(pCtx, ReflectFuncDeclClass(&sRef));
}
/*
 * The receiver check php runs before invoking or binding: a non-static method
 * needs an instance OF ITS DECLARING CLASS; a static one ignores whatever was
 * handed over. *ppThis is cleared for a static method.
 */
static sxi32 ReflectMethodReceiver(ph7_context *pCtx, ReflectFuncRef *pRef,
	ph7_value *pObject, ph7_class_instance **ppThis, int bClosure)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zClass = "", *zName = "";
	int nClass = 0, nName = 0;
	ph7_class *pDecl = ReflectFuncDeclClass(pRef);
	*ppThis = 0;
	if( pThis ){
		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	}
	if( pRef->pMeth && (pRef->pMeth->iFlags & PH7_CLASS_ATTR_STATIC) ){
		return PH7_OK;
	}
	if( pObject == 0 || (pObject->iFlags & MEMOBJ_OBJ) == 0 ){
		if( bClosure ){
			return PH7_VmThrowException(pCtx, "ValueError",
				"ReflectionMethod::getClosure(): Argument #1 ($object) cannot be null for non-static methods");
		}
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Trying to invoke non static method %.*s::%.*s() without an object",
			nClass, zClass, nName, zName);
	}
	*ppThis = (ph7_class_instance *)pObject->x.pOther;
	if( pDecl && !PH7_VmInstanceOf((*ppThis)->pClass, pDecl) ){
		*ppThis = 0;
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Given object is not an instance of the class this method was declared in");
	}
	return PH7_OK;
}
static int ReflectMethodInvoke(ph7_context *pCtx, ph7_value *pObject, int nCall, ph7_value **apCall)
{
	ph7_vm *pVm = pCtx->pVm;
	ReflectFuncRef sRef;
	ph7_class_instance *pRecv = 0;
	ph7_value sResult;
	sxi32 rc;
	if( !ReflectFuncOfThis(pCtx, &sRef) || sRef.pMeth == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	rc = ReflectMethodReceiver(pCtx, &sRef, pObject, &pRecv, 0);
	if( rc != PH7_OK ){
		return rc;
	}
	PH7_MemObjInit(pVm, &sResult);
	sResult.nIdx = SXU32_HIGH;
	/* Reflection ignores method visibility (PHP 8.1+); the flag is consumed by
	 * the first OP_CALL, i.e. this synthetic one. */
	pVm->bReflectBypass = 1;
	rc = PH7_VmCallClassMethod(pVm, pRecv, sRef.pMeth, &sResult, nCall, apCall);
	pVm->bReflectBypass = 0;
	if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){
		PH7_MemObjRelease(&sResult);
		return rc;
	}
	ph7_result_value(pCtx, &sResult);
	PH7_MemObjRelease(&sResult);
	return PH7_OK;
}
static int vm_builtin_ReflectionMethod_invoke(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	return ReflectMethodInvoke(pCtx, nArg > 0 ? apArg[0] : 0,
		nArg > 1 ? nArg - 1 : 0, nArg > 1 ? &apArg[1] : 0);
}
static int vm_builtin_ReflectionMethod_invokeArgs(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SySet aCall;
	int rc;
	SySetInit(&aCall, &pCtx->pVm->sAllocator, sizeof(ph7_value *));
	if( nArg > 1 ){
		ReflectCollectArgs(pCtx, apArg[1], &aCall, 0);
	}
	rc = ReflectMethodInvoke(pCtx, nArg > 0 ? apArg[0] : 0,
		(int)SySetUsed(&aCall), (ph7_value **)SySetBasePtr(&aCall));
	SySetRelease(&aCall);
	return rc;
}
static int vm_builtin_ReflectionMethod_getClosure(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	ph7_class_instance *pRecv = 0;
	sxi32 rc;
	if( !ReflectFuncOfThis(pCtx, &sRef) || sRef.pMeth == 0 || sRef.pClass == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	rc = ReflectMethodReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pRecv, 1);
	if( rc != PH7_OK ){
		return rc;
	}
	return ReflectResultObject(pCtx,
		PH7_VmNewClosure(pCtx->pVm, &sRef.pMeth->sFunc.sName, pRecv, &sRef.pClass->sName));
}
/* php 8.1 made every reflected member accessible; the setter is a no-op it kept. */
static int vm_builtin_ReflectionMethod_setAccessible(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return PH7_OK;
}
/*
 * The class this method OVERRIDES it from: the nearest base declaring a
 * same-named non-private method, else the first interface that declares one.
 */
static ph7_class * ReflectMethodPrototype(ph7_context *pCtx, ReflectFuncRef *pRef)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class *pWalk;
	const char *zName = "";
	int nName = 0, iDepth = 0;
	if( pThis == 0 || pRef->pClass == 0 ){
		return 0;
	}
	PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	for( pWalk = pRef->pClass->pBase ; pWalk && iDepth <= REFLECT_WALK_MAX_DEPTH ; pWalk = pWalk->pBase ){
		SyHashEntry *pEntry = ReflectFindMethodEntry(pWalk, zName, nName);
		if( pEntry ){
			ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;
			if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){
				return ReflectMethodDeclClass(pWalk, pMeth);
			}
		}
		iDepth++;
	}
	{
		SySet aSet;
		ph7_class **apIface;
		ph7_class *pFound = 0;
		sxu32 n;
		SySetInit(&aSet, &pCtx->pVm->sAllocator, sizeof(ph7_class *));
		ReflectInterfacesOf(pRef->pClass, &aSet);
		apIface = (ph7_class **)SySetBasePtr(&aSet);
		for( n = 0 ; n < SySetUsed(&aSet) ; n++ ){
			if( ReflectFindMethodEntry(apIface[n], zName, nName) ){
				pFound = apIface[n];
				break;
			}
		}
		SySetRelease(&aSet);
		return pFound;
	}
}
static int vm_builtin_ReflectionMethod_hasPrototype(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectFuncOfThis(pCtx, &sRef) ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	ph7_result_bool(pCtx, ReflectMethodPrototype(pCtx, &sRef) != 0);
	return PH7_OK;
}
static int vm_builtin_ReflectionMethod_getPrototype(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ReflectFuncRef sRef;
	ph7_class *pProto;
	const char *zClass = "", *zName = "";
	int nClass = 0, nName = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || !ReflectFuncOfThis(pCtx, &sRef) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);
	PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	pProto = ReflectMethodPrototype(pCtx, &sRef);
	if( pProto == 0 ){
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Method %.*s::%.*s does not have a prototype", nClass, zClass, nName, zName);
	}
	{
		ph7_value sClass, sName;
		ph7_value *apCtor[2];
		ph7_class_instance *pOut;
		sxi32 rc;
		PH7_MemObjInit(pCtx->pVm, &sClass);
		PH7_MemObjInit(pCtx->pVm, &sName);
		ph7_value_string(&sClass, SyStringData(&pProto->sName), (int)SyStringLength(&pProto->sName));
		ph7_value_string(&sName, zName, nName);
		apCtor[0] = &sClass;
		apCtor[1] = &sName;
		pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);
		PH7_MemObjRelease(&sClass);
		PH7_MemObjRelease(&sName);
		if( pOut == 0 ){
			if( rc != PH7_OK ){
				return rc;
			}
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		return ReflectResultObject(pCtx, pOut);
	}
}
/* ---- ReflectionParameter ---- */
/*
 * The function a ReflectionParameter belongs to, from its own `__t`/`__m`
 * slots. `__t` holds a Closure, a class name or a function name; `__m` the
 * method name, or null.
 */
static int ReflectParamOwner(ph7_context *pCtx, ReflectFuncRef *pRef, ReflectParamDesc *pDesc)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pT, *pM;
	int rc;
	SyZero(pRef, sizeof(*pRef));
	if( pDesc ){
		SyZero(pDesc, sizeof(*pDesc));
	}
	if( pThis == 0 ){
		return 0;
	}
	pT = PH7_NativeAttr(pThis, RP_T);
	pM = PH7_NativeAttr(pThis, RP_M);
	if( pT == 0 ){
		return 0;
	}
	rc = ReflectFuncFill(pCtx, pT, (pM && (pM->iFlags & MEMOBJ_STRING)) ? pM : 0, pRef);
	if( rc == 0 || pDesc == 0 ){
		return rc;
	}
	return ReflectParamAt(pRef, (int)PH7_NativeAttrInt(pThis, RP_P), pDesc);
}
/* ReflectionParameter::__construct($function, string|int $param) */
static int vm_builtin_ReflectionParameter_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ReflectFuncRef sRef;
	ReflectParamDesc sDesc;
	ph7_value sTarget, sMethod;
	int nTotal, iFound = -1, rc = PH7_OK;
	if( pThis == 0 || nArg < 2 ){
		return PH7_OK;
	}
	SyZero(&sDesc, sizeof(sDesc));
	PH7_MemObjInit(pVm, &sTarget);
	PH7_MemObjInit(pVm, &sMethod);
	/* `[$objOrClass, $method]`, `"C::m"` and a plain function/Closure are the
	 * three spellings php accepts. */
	if( ph7_value_is_array(apArg[0]) ){
		ph7_value *pA = ph7_array_fetch(apArg[0], "0", 1);
		ph7_value *pB = ph7_array_fetch(apArg[0], "1", 1);
		if( pA && (pA->iFlags & MEMOBJ_OBJ) ){
			ph7_class_instance *pObj = (ph7_class_instance *)pA->x.pOther;
			ph7_value_string(&sTarget, SyStringData(&pObj->pClass->sName),
				(int)SyStringLength(&pObj->pClass->sName));
		}else if( pA ){
			PH7_MemObjStore(pA, &sTarget);
		}
		if( pB ){
			PH7_MemObjStore(pB, &sMethod);
		}
	}else{
		/* A STRING is a plain function name, `"C::m"` included: php does not
		 * split one here (it reports `Function C::m() does not exist`), unlike
		 * ReflectionMethod's own one-argument form. The prelude split it and
		 * silently reflected the method. */
		PH7_MemObjStore(apArg[0], &sTarget);
	}
	if( !ReflectFuncFill(pCtx, &sTarget, (sMethod.iFlags & MEMOBJ_STRING) ? &sMethod : 0, &sRef) ){
		const char *zName;
		int nName;
		if( sMethod.iFlags & MEMOBJ_STRING ){
			const char *zM;
			int nM;
			zName = ph7_value_to_string(&sTarget, &nName);
			zM = ph7_value_to_string(&sMethod, &nM);
			rc = PH7_VmThrowException(pCtx, "ReflectionException",
				"Method %.*s::%.*s() does not exist", nName, zName, nM, zM);
		}else{
			zName = ph7_value_to_string(&sTarget, &nName);
			rc = PH7_VmThrowException(pCtx, "ReflectionException",
				"Function %.*s() does not exist", nName, zName);
		}
		goto Done;
	}
	nTotal = ReflectParamCount(&sRef);
	if( apArg[1]->iFlags & (MEMOBJ_INT|MEMOBJ_BOOL) ){
		int iWant = (int)ph7_value_to_int(apArg[1]);
		if( iWant >= 0 && iWant < nTotal && ReflectParamAt(&sRef, iWant, &sDesc) ){
			iFound = iWant;
		}
		if( iFound < 0 ){
			rc = PH7_VmThrowException(pCtx, "ReflectionException",
				"The parameter specified by its offset could not be found");
			goto Done;
		}
	}else{
		const char *zWant;
		int nWant, n;
		zWant = ph7_value_to_string(apArg[1], &nWant);
		for( n = 0 ; n < nTotal ; n++ ){
			if( ReflectParamAt(&sRef, n, &sDesc)
			 && SyStringLength(&sDesc.sName) == (sxu32)nWant
			 && SyMemcmp(SyStringData(&sDesc.sName), zWant, (sxu32)nWant) == 0 ){
				iFound = n;
				break;
			}
		}
		if( iFound < 0 ){
			rc = PH7_VmThrowException(pCtx, "ReflectionException",
				"The parameter specified by its name could not be found");
			goto Done;
		}
	}
	PH7_NativeSetAttrStr(pVm, pThis, "name", SyStringData(&sDesc.sName),
		(int)SyStringLength(&sDesc.sName));
	/* Record the CANONICAL target: the class the method really came through and
	 * its declared spelling, so a re-resolve cannot land somewhere else. */
	if( sRef.pMeth && sRef.pClass ){
		PH7_NativeSetAttrStr(pVm, pThis, RP_T, SyStringData(&sRef.pClass->sName),
			(int)SyStringLength(&sRef.pClass->sName));
		PH7_NativeSetAttrStr(pVm, pThis, RP_M, SyStringData(&sRef.pMeth->sFunc.sName),
			(int)SyStringLength(&sRef.pMeth->sFunc.sName));
	}else{
		ph7_value *pSlot = PH7_NativeAttr(pThis, RP_T);
		if( pSlot ){
			PH7_MemObjStore(&sTarget, pSlot);
		}
	}
	PH7_NativeSetAttrInt(pVm, pThis, RP_P, (sxi64)iFound);
Done:
	PH7_MemObjRelease(&sTarget);
	PH7_MemObjRelease(&sMethod);
	return rc;
}
static int vm_builtin_ReflectionParameter_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = "";
	int nName = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	}
	ph7_result_string(pCtx, zName, nName);
	return PH7_OK;
}
static int vm_builtin_ReflectionParameter_getPosition(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_int64(pCtx, pThis ? PH7_NativeAttrInt(pThis, RP_P) : 0);
	return PH7_OK;
}
/* The boolean predicates that read one flag of the description. */
static int ReflectParamFlag(ph7_context *pCtx, int iWhat)
{
	ReflectFuncRef sRef;
	ReflectParamDesc sDesc;
	int bYes = 0;
	if( ReflectParamOwner(pCtx, &sRef, &sDesc) ){
		switch( iWhat ){
		case 0: bYes = sDesc.bByRef; break;
		case 1: bYes = !sDesc.bByRef; break;
		case 2: bYes = sDesc.bVariadic; break;
		case 3: bYes = sDesc.bPromoted; break;
		case 4: bYes = sDesc.bHasDef; break;
		case 5: bYes = SyStringLength(&sDesc.sType) > 0; break;
		default:
			/* allowsNull(): untyped, nullable, or a type that INCLUDES null */
			bYes = SyStringLength(&sDesc.sType) < 1 || sDesc.bNullable
				|| ReflectTypeNameIs(SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType), "mixed")
				|| ReflectTypeNameIs(SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType), "null");
			break;
		}
	}else if( iWhat == 1 || iWhat == 6 ){
		bYes = 1;
	}
	ph7_result_bool(pCtx, bYes);
	return PH7_OK;
}
#define REFLECT_PARAM_FLAG(NAME,WHAT) \
	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \
	{ \
		SXUNUSED(nArg); \
		SXUNUSED(apArg); \
		return ReflectParamFlag(pCtx, WHAT); \
	}
REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isPassedByReference, 0)
REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_canBePassedByValue, 1)
REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isVariadic, 2)
REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isPromoted, 3)
REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_isDefaultValueAvailable, 4)
REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_hasType, 5)
REFLECT_PARAM_FLAG(vm_builtin_ReflectionParameter_allowsNull, 6)

/* isOptional(): every parameter from here on has to be omissible too. */
static int vm_builtin_ReflectionParameter_isOptional(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ReflectFuncRef sRef;
	ReflectParamDesc sDesc;
	int n, nTotal, iPos;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || !ReflectParamOwner(pCtx, &sRef, 0) ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	iPos = (int)PH7_NativeAttrInt(pThis, RP_P);
	nTotal = ReflectParamCount(&sRef);
	for( n = iPos ; n < nTotal ; n++ ){
		if( ReflectParamAt(&sRef, n, &sDesc) && !sDesc.bVariadic && !sDesc.bOptional ){
			ph7_result_bool(pCtx, 0);
			return PH7_OK;
		}
	}
	ph7_result_bool(pCtx, 1);
	return PH7_OK;
}
static int vm_builtin_ReflectionParameter_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	ReflectParamDesc sDesc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) || SyStringLength(&sDesc.sType) < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultObject(pCtx, ReflectMakeType(pCtx,
		SyStringData(&sDesc.sType), (int)SyStringLength(&sDesc.sType)));
}
/*
 * getClass()/isArray()/isCallable() — php 8.0 deprecated these in favour of
 * getType() but still declares them, still answers, and emits an E_DEPRECATED
 * naming the replacement. PHL fatalled on the call; all three are here now,
 * notice included.
 */
static void ReflectParamDeprecated(ph7_context *pCtx, const char *zWho)
{
	char zMsg[160];
	SyBufferFormat(zMsg, sizeof(zMsg),
		"Method ReflectionParameter::%s() is deprecated since 8.0, "
		"use ReflectionParameter::getType() instead", zWho);
	PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED, zMsg);
}
static int vm_builtin_ReflectionParameter_getClass(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	ReflectParamDesc sDesc;
	const char *zType;
	int nType;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ReflectParamDeprecated(pCtx, "getClass");
	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) || SyStringLength(&sDesc.sType) < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zType = SyStringData(&sDesc.sType);
	nType = (int)SyStringLength(&sDesc.sType);
	if( nType > 0 && zType[0] == '?' ){
		zType++;
		nType--;
	}
	if( ReflectTypeIsBuiltin(zType, nType) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultClassOf(pCtx, PH7_VmExtractClass(pCtx->pVm, zType, (sxu32)nType, FALSE, 0));
}
static int ReflectParamTypeIs(ph7_context *pCtx, const char *zWant)
{
	ReflectFuncRef sRef;
	ReflectParamDesc sDesc;
	int bYes = 0;
	if( ReflectParamOwner(pCtx, &sRef, &sDesc) && SyStringLength(&sDesc.sType) > 0 ){
		const char *zType = SyStringData(&sDesc.sType);
		int nType = (int)SyStringLength(&sDesc.sType);
		/* php answers true for `?array` too: the deprecated pair asks about the
		 * type ATOM, and nullability is a separate question. */
		if( zType[0] == '?' ){
			zType++;
			nType--;
		}
		bYes = ReflectTypeNameIs(zType, nType, zWant);
	}
	ph7_result_bool(pCtx, bYes);
	return PH7_OK;
}
static int vm_builtin_ReflectionParameter_isArray(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ReflectParamDeprecated(pCtx, "isArray");
	return ReflectParamTypeIs(pCtx, "array");
}
static int vm_builtin_ReflectionParameter_isCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ReflectParamDeprecated(pCtx, "isCallable");
	return ReflectParamTypeIs(pCtx, "callable");
}
static int vm_builtin_ReflectionParameter_getDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	ReflectParamDesc sDesc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) || !sDesc.bHasDef ){
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Internal error: Failed to retrieve the default value");
	}
	if( sDesc.pArg ){
		/* Compiled: the same evaluation path the VM uses for an omitted argument */
		ph7_value sValue;
		PH7_MemObjInit(pCtx->pVm, &sValue);
		VmLocalExec(pCtx->pVm, &sDesc.pArg->aByteCode, &sValue, FALSE);
		ph7_result_value(pCtx, &sValue);
		PH7_MemObjRelease(&sValue);
		return PH7_OK;
	}
	{
		/* Declared: the signature's default TEXT, reduced. */
		const char *zDef = SyStringData(&sDesc.sDefText);
		int nDef = (int)SyStringLength(&sDesc.sDefText);
		ph7_value *pVal = ph7_context_new_scalar(pCtx);
		if( pVal && ReflectSigScalar(pCtx, zDef, nDef, pVal) ){
			ph7_result_value(pCtx, pVal);
			return PH7_OK;
		}
		if( (nDef >= 1 && zDef[0] == '[')
		 || (nDef >= (int)sizeof("array (")-1
		  && SyMemcmp(zDef, "array (", sizeof("array (")-1) == 0) ){
			ph7_result_value(pCtx, ph7_context_new_array(pCtx));
			return PH7_OK;
		}
	}
	return PH7_VmThrowException(pCtx, "ReflectionException",
		"Internal error: Failed to retrieve the default value");
}
/*
 * A default that is a plain global-constant reference compiles to exactly
 * [ OP_LOADC (EXPAND), OP_DONE ] with the name in the literal table; a
 * DECLARED default is text and never a constant.
 */
static int ReflectParamDefConst(ph7_context *pCtx, ReflectParamDesc *pDesc,
	const char **pz, int *pn)
{
	VmInstr *aInstr;
	ph7_value *pLit;
	if( pDesc->pArg == 0 || SySetUsed(&pDesc->pArg->aByteCode) != 2 ){
		return 0;
	}
	aInstr = (VmInstr *)SySetBasePtr(&pDesc->pArg->aByteCode);
	if( aInstr[0].iOp != PH7_OP_LOADC || (aInstr[0].iP1 & PH7_LOADC_EXPAND) == 0
	 || aInstr[1].iOp != PH7_OP_DONE ){
		return 0;
	}
	pLit = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj, aInstr[0].iP2);
	if( pLit == 0 || SyBlobLength(&pLit->sBlob) < 1 ){
		return 0;
	}
	*pz = (const char *)SyBlobData(&pLit->sBlob);
	*pn = (int)SyBlobLength(&pLit->sBlob);
	return 1;
}
static int vm_builtin_ReflectionParameter_isDefaultValueConstant(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	ReflectParamDesc sDesc;
	const char *z;
	int n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) || !sDesc.bHasDef ){
		/* php raises here rather than answering false: asking whether a default
		 * is a constant presupposes there IS one. The prelude answered false. */
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Internal error: Failed to retrieve the default value");
	}
	ph7_result_bool(pCtx, ReflectParamDefConst(pCtx, &sDesc, &z, &n) != 0);
	return PH7_OK;
}
static int vm_builtin_ReflectionParameter_getDefaultValueConstantName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	ReflectParamDesc sDesc;
	const char *z;
	int n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) || !sDesc.bHasDef ){
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Internal error: Failed to retrieve the default value");
	}
	if( ReflectParamDefConst(pCtx, &sDesc, &z, &n) ){
		ph7_result_string(pCtx, z, n);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionParameter_getDeclaringFunction(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_value *pT, *pM;
	ph7_value *apCtor[2];
	ph7_class_instance *pOut;
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis == 0 || (pT = PH7_NativeAttr(pThis, RP_T)) == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pM = PH7_NativeAttr(pThis, RP_M);
	apCtor[0] = pT;
	apCtor[1] = pM;
	if( pM && (pM->iFlags & MEMOBJ_STRING) ){
		pOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);
	}else{
		pOut = ReflectConstruct(pCtx, "ReflectionFunction", 1, apCtor, &rc);
	}
	if( pOut == 0 ){
		if( rc != PH7_OK ){
			return rc;
		}
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultObject(pCtx, pOut);
}
static int vm_builtin_ReflectionParameter_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectParamOwner(pCtx, &sRef, 0) || sRef.pMeth == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultClassOf(pCtx, ReflectFuncDeclClass(&sRef));
}
static int vm_builtin_ReflectionParameter_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ReflectFuncRef sRef;
	ReflectParamDesc sDesc;
	ph7_value *pT, *pM;
	const char *zMember = 0;
	int nMember = 0;
	if( pThis == 0 || !ReflectParamOwner(pCtx, &sRef, &sDesc) || sDesc.pArg == 0 ){
		ph7_result_value(pCtx, ph7_context_new_array(pCtx));
		return PH7_OK;
	}
	pT = PH7_NativeAttr(pThis, RP_T);
	pM = PH7_NativeAttr(pThis, RP_M);
	if( pM && (pM->iFlags & MEMOBJ_STRING) ){
		zMember = (const char *)SyBlobData(&pM->sBlob);
		nMember = (int)SyBlobLength(&pM->sBlob);
	}
	/* 32 = Attribute::TARGET_PARAMETER */
	return ReflectBuildAttrs(pCtx, &sDesc.pArg->aAttrs, "param", pT, zMember, nMember,
		(int)PH7_NativeAttrInt(pThis, RP_P), 32, nArg, apArg);
}
static int vm_builtin_ReflectionParameter_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return ReflectExportSelf(pCtx, "__reflect_export_param", 0);
}
/*
 * Declare chunk 2. Called from PH7_VmInstallReflectionLib where it used to be
 * compiled — after chunk 1, whose `Reflector` these implement and whose
 * `ReflectionClass` they answer.
 *
 * The method tables are in php's own DECLARATION order.
 */
/*
 * PropertyHookType — php's `enum PropertyHookType: string { Get; Set; }`, the
 * argument ReflectionProperty::getHook()/hasHook() take.
 */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionHookType(ph7_vm *pVm)
{
	static const PH7_NativeEnumCase aCase[] = {
		{ "Get", { 0, 0, PH7_NATIVE_VAL_STRING, 0, "get", 0.0 } },
		{ "Set", { 0, 0, PH7_NATIVE_VAL_STRING, 0, "set", 0.0 } },
	};
	return PH7_InstallNativeEnum(&(*pVm), "PropertyHookType", MEMOBJ_STRING,
		aCase, SX_ARRAYSIZE(aCase), 0, 0);
}
PH7_PRIVATE sxi32 PH7_VmInstallReflectionFunc(ph7_vm *pVm)
{
	static const PH7_NativePropDef aAbstractProp[] = {
		{ "name",  PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 } },
		/* PHL-only: the Closure being reflected. php reaches the same state from
		 * the function record itself; PHL has no hidden-slot bit yet (§7.4 (e)). */
		{ RF_CL,   PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
	};
	static const PH7_NativeMethodDef aAbstractMethod[] = {
		{ "__clone",       PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },
		{ "inNamespace",   PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_inNamespace },
		{ "isClosure",     PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_isClosure },
		{ "isDeprecated",  PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_isDeprecated },
		{ "isInternal",    PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_isInternal },
		{ "isUserDefined", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_isUserDefined },
		{ "isGenerator",   PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_isGenerator },
		{ "isVariadic",    PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_isVariadic },
		{ "isStatic",      PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_isStatic },
		{ "getClosureThis", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_getClosureThis },
		{ "getClosureScopeClass", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionFunc_getClosureScopeClass },
		/* php answers the same class for both; PHL has no separate called-scope. */
		{ "getClosureCalledClass", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionFunc_getClosureScopeClass },
		{ "getClosureUsedVariables", PH7_MOD_PUBLIC, "", "array",
		  vm_builtin_ReflectionFunc_getClosureUsedVariables },
		{ "getDocComment", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_getDocComment },
		{ "getEndLine",    PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_getEndLine },
		{ "getExtension",  PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_getExtension },
		{ "getExtensionName", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_getExtensionName },
		{ "getFileName",   PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_getFileName },
		{ "getName",       PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_getName },
		{ "getNamespaceName", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_getNamespaceName },
		{ "getNumberOfParameters", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionFunc_getNumberOfParameters },
		{ "getNumberOfRequiredParameters", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionFunc_getNumberOfRequiredParameters },
		{ "getParameters", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_getParameters },
		{ "getShortName",  PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_getShortName },
		{ "getStartLine",  PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_getStartLine },
		{ "getStaticVariables", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionFunc_getStaticVariables },
		{ "returnsReference", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_returnsReference },
		{ "hasReturnType", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_hasReturnType },
		{ "getReturnType", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunc_getReturnType },
		{ "hasTentativeReturnType", PH7_MOD_PUBLIC, "", "bool",
		  vm_builtin_ReflectionFunc_hasTentativeReturnType },
		{ "getTentativeReturnType", PH7_MOD_PUBLIC, "", "?ReflectionType",
		  vm_builtin_ReflectionFunc_getTentativeReturnType },
		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",
		  vm_builtin_ReflectionFunc_getAttributes },
	};
	static const PH7_NativeConstDef aFunctionConst[] = {
		{ "IS_DEPRECATED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2048, 0, 0.0 },
	};
	static const PH7_NativeMethodDef aFunctionMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "Closure|string $function", "",
		  vm_builtin_ReflectionFunction_construct },
		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionFunc_toString },
		{ "isAnonymous", PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionFunction_isAnonymous },
		{ "isDisabled",  PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunction_isDisabled },
		{ "invoke",      PH7_MOD_PUBLIC, "mixed ...$args", "",
		  vm_builtin_ReflectionFunction_invoke },
		{ "invokeArgs",  PH7_MOD_PUBLIC, "array $args", "",
		  vm_builtin_ReflectionFunction_invokeArgs },
		{ "getClosure",  PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionFunction_getClosure },
	};
	static const PH7_NativePropDef aMethodProp[] = {
		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 } },
	};
	static const PH7_NativeConstDef aMethodConst[] = {
		{ "IS_STATIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16, 0, 0.0 },
		{ "IS_PUBLIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,  0, 0.0 },
		{ "IS_PROTECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,  0, 0.0 },
		{ "IS_PRIVATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,  0, 0.0 },
		{ "IS_ABSTRACT",  PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64, 0, 0.0 },
		{ "IS_FINAL",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },
	};
	static const PH7_NativeMethodDef aMethodMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "object|string $objectOrMethod, ?string $method = null", "",
		  vm_builtin_ReflectionMethod_construct },
		{ "createFromMethodName", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "string $method", "static",
		  vm_builtin_ReflectionMethod_createFromMethodName },
		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionFunc_toString },
		{ "isPublic",    PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionMethod_isPublic },
		{ "isPrivate",   PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionMethod_isPrivate },
		{ "isProtected", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionMethod_isProtected },
		{ "isAbstract",  PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionMethod_isAbstract },
		{ "isFinal",     PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionMethod_isFinal },
		{ "isConstructor", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionMethod_isConstructor },
		{ "isDestructor",  PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionMethod_isDestructor },
		{ "getClosure",  PH7_MOD_PUBLIC, "?object $object = null", "",
		  vm_builtin_ReflectionMethod_getClosure },
		{ "getModifiers", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionMethod_getModifiers },
		{ "invoke",      PH7_MOD_PUBLIC, "?object $object, mixed ...$args", "",
		  vm_builtin_ReflectionMethod_invoke },
		{ "invokeArgs",  PH7_MOD_PUBLIC, "?object $object, array $args", "",
		  vm_builtin_ReflectionMethod_invokeArgs },
		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionMethod_getDeclaringClass },
		{ "getPrototype", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionMethod_getPrototype },
		{ "hasPrototype", PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionMethod_hasPrototype },
		{ "setAccessible", PH7_MOD_PUBLIC, "bool $accessible", "",
		  vm_builtin_ReflectionMethod_setAccessible },
	};
	static const PH7_NativePropDef aParamProp[] = {
		{ "name", PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 } },
		/* PHL-only, the three that identify the parameter (§7.4 (e)) */
		{ RP_T,   PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
		{ RP_M,   PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
		{ RP_P,   PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 } },
	};
	static const PH7_NativeMethodDef aParamMethod[] = {
		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },
		/* php leaves $function UNTYPED here: it takes a name, a Closure, a
		 * `[$obj, 'm']` pair or "C::m", and no declarable union covers all four. */
		{ "__construct", PH7_MOD_PUBLIC, "$function, string|int $param", "",
		  vm_builtin_ReflectionParameter_construct },
		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionParameter_toString },
		{ "getName",     PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionParameter_getName },
		{ "isPassedByReference", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionParameter_isPassedByReference },
		{ "canBePassedByValue",  PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionParameter_canBePassedByValue },
		{ "getDeclaringFunction", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionParameter_getDeclaringFunction },
		{ "getDeclaringClass",   PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionParameter_getDeclaringClass },
		{ "getClass",    PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionParameter_getClass },
		{ "hasType",     PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionParameter_hasType },
		{ "getType",     PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionParameter_getType },
		{ "isArray",     PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionParameter_isArray },
		{ "isCallable",  PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionParameter_isCallable },
		{ "allowsNull",  PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionParameter_allowsNull },
		{ "getPosition", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionParameter_getPosition },
		{ "isOptional",  PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionParameter_isOptional },
		{ "isDefaultValueAvailable", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionParameter_isDefaultValueAvailable },
		{ "getDefaultValue", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionParameter_getDefaultValue },
		{ "isDefaultValueConstant", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionParameter_isDefaultValueConstant },
		{ "getDefaultValueConstantName", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionParameter_getDefaultValueConstantName },
		{ "isVariadic",  PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionParameter_isVariadic },
		{ "isPromoted",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionParameter_isPromoted },
		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",
		  vm_builtin_ReflectionParameter_getAttributes },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "ReflectionFunctionAbstract", 0, "Reflector", PH7_CLASS_ABSTRACT|PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aAbstractMethod, SX_ARRAYSIZE(aAbstractMethod), 0, 0,
		  aAbstractProp, SX_ARRAYSIZE(aAbstractProp), 0, 0 },
		{ "ReflectionFunction", "ReflectionFunctionAbstract", 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aFunctionMethod, SX_ARRAYSIZE(aFunctionMethod),
		  aFunctionConst, SX_ARRAYSIZE(aFunctionConst), 0, 0, 0, 0 },
		{ "ReflectionMethod", "ReflectionFunctionAbstract", 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aMethodMethod, SX_ARRAYSIZE(aMethodMethod),
		  aMethodConst, SX_ARRAYSIZE(aMethodConst),
		  aMethodProp, SX_ARRAYSIZE(aMethodProp), 0, 0 },
		{ "ReflectionParameter", 0, "Reflector", PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aParamMethod, SX_ARRAYSIZE(aParamMethod), 0, 0,
		  aParamProp, SX_ARRAYSIZE(aParamProp), 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));
}
/*
 * ---------------------------------------------------------------------------
 * ReflectionProperty and ReflectionClassConstant.
 *
 * Chunk 3, the last of the three that made up "the core". Seven thunks retire
 * with it — __reflect_prop_read, __reflect_prop_write, __reflect_prop_state,
 * __reflect_prop_default, __reflect_static_value, __reflect_static_set and
 * __reflect_const_value — because a native method reaches an instance's slot
 * table, a static's shared slot and a constant's lazy slot directly.
 * ---------------------------------------------------------------------------
 */
#define RP_DYNOBJ "__dynobj"   /* the instance a DYNAMIC property lives on */

/* skipLazyInitialization() is a no-op on an object that is not lazy, which is
 * every object PHL can build — so it answers what php answers rather than
 * refusing (§7.4). */
static int vm_builtin_ReflectionProperty_noop(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return PH7_OK;
}
/*
 * A STATIC property's shared slot, read and written the way `C::$s` is: the
 * class's static table materializes first (a default that threw at the
 * declaration raises HERE), and an uninitialized typed static is php's Error.
 */
static int ReflectStaticSlotRead(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr)
{
	SyHashEntry *pSlot;
	ph7_value *pVal;
	sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);
	if( rc != SXRET_OK ){
		return rc;
	}
	pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&pAttr->nIdx, sizeof(sxu32));
	if( pSlot && (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) ){
		ph7_class *pDecl = pAttr->pDeclClass ? pAttr->pDeclClass : pClass;
		return PH7_VmThrowException(pCtx, "Error",
			"Typed static property %z::$%z must not be accessed before initialization",
			&pDecl->sName, &pAttr->sName);
	}
	pVal = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);
	if( pVal ){
		ph7_result_value(pCtx, pVal);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
static int ReflectStaticSlotWrite(ph7_context *pCtx, ph7_class *pClass, ph7_class_attr *pAttr,
	ph7_value *pValue)
{
	ph7_value *pSlot;
	sxi32 rc = ReflectMaterializeStatics(pCtx, pClass);
	if( rc != SXRET_OK ){
		return rc;
	}
	rc = ReflectEnforceStore(pCtx, pAttr->nIdx, pValue);
	if( rc != SXRET_OK ){
		return rc;
	}
	pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pAttr->nIdx);
	if( pSlot ){
		PH7_MemObjStore(pValue, pSlot);
	}
	return PH7_OK;
}

/* What a ReflectionProperty / ReflectionClassConstant is looking at. */
typedef struct ReflectMemberRef ReflectMemberRef;
struct ReflectMemberRef
{
	ph7_class *pClass;            /* the reflected class */
	ph7_class_attr *pAttr;        /* the declared member (NULL for a dynamic property) */
	ph7_class_instance *pDynObj;  /* the instance a dynamic property lives on */
	const char *zName;            /* the member's name (borrowed from the slot) */
	int nName;
};
/*
 * Resolve `$this->class` + `$this->name` into the member. Uses the LISTING
 * answer of the member walk, which is php's: a base class's PRIVATE member is
 * not reachable by name from the subclass (`new ReflectionProperty('B','pp')`
 * raises where `B extends A` and `A::$pp` is private).
 */
static int ReflectMemberOfThis(ph7_context *pCtx, ReflectMemberRef *pOut, int iKind)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zClass;
	int nClass;
	SySet aMembers;
	sxu32 n;
	SyZero(pOut, sizeof(*pOut));
	if( pThis == 0 ){
		return 0;
	}
	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);
	PH7_NativeAttrStr(pThis, "name", &pOut->zName, &pOut->nName);
	if( nClass < 1 ){
		return 0;
	}
	pOut->pClass = PH7_VmExtractClass(pCtx->pVm, zClass, (sxu32)nClass, FALSE, 0);
	if( pOut->pClass == 0 ){
		return 0;
	}
	if( iKind == REFLECT_MEMBER_PROP ){
		pOut->pDynObj = PH7_NativeAttrObj(pThis, RP_DYNOBJ);
	}
	SySetInit(&aMembers, &pCtx->pVm->sAllocator, sizeof(ReflectMember));
	ReflectMembers(pCtx->pVm, pOut->pClass, &aMembers, 0);
	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){
		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);
		if( pM->iKind == iKind && ReflectKeyIs(pM, pOut->zName, pOut->nName) ){
			pOut->pAttr = pM->pAttr;
			break;
		}
	}
	SySetRelease(&aMembers);
	return pOut->pAttr != 0 || pOut->pDynObj != 0;
}
/* The class php reports as the member's declarer. */
static ph7_class * ReflectMemberDecl(const ReflectMemberRef *pRef)
{
	if( pRef->pAttr && pRef->pAttr->pDeclClass ){
		return pRef->pAttr->pDeclClass;
	}
	return pRef->pClass;
}
/* The instance slot record for a dynamic (or any instance-owned) property. */
static VmClassAttr * ReflectInstanceAttr(ph7_class_instance *pObj, const char *zName, int nName)
{
	SyHashEntry *pEntry;
	if( pObj == 0 || nName < 1 ){
		return 0;
	}
	pEntry = SyHashGet(&pObj->hAttr, (const void *)zName, (sxu32)nName);
	return pEntry ? (VmClassAttr *)pEntry->pUserData : 0;
}
/* ---- ReflectionProperty ---- */
/* ReflectionProperty::__construct(object|string $class, string $property) */
static int vm_builtin_ReflectionProperty_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class_instance *pObj = 0;
	ph7_class *pClass;
	const char *zProp;
	int nProp;
	SySet aMembers;
	sxu32 n;
	int bFound = 0;
	if( pThis == 0 || nArg < 2 ){
		return PH7_OK;
	}
	if( apArg[0]->iFlags & MEMOBJ_OBJ ){
		pObj = (ph7_class_instance *)apArg[0]->x.pOther;
	}
	pClass = ReflectResolveClass(pVm, apArg[0]);
	if( pClass == 0 ){
		const char *zName;
		int nName;
		zName = ph7_value_to_string(apArg[0], &nName);
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Class \"%.*s\" does not exist", nName, zName);
	}
	zProp = ph7_value_to_string(apArg[1], &nProp);
	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));
	ReflectMembers(pVm, pClass, &aMembers, 0);
	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){
		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);
		if( pM->iKind == REFLECT_MEMBER_PROP && ReflectKeyIs(pM, zProp, nProp) ){
			/* php's $class is the DECLARING class, not the one asked about. */
			pClass = pM->pDecl ? pM->pDecl : pClass;
			bFound = 1;
			break;
		}
	}
	SySetRelease(&aMembers);
	PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pClass->sName),
		(int)SyStringLength(&pClass->sName));
	if( bFound ){
		PH7_NativeSetAttrStr(pVm, pThis, "name", zProp, nProp);
		return PH7_OK;
	}
	/* Not declared: an OBJECT may still own it as a dynamic property. */
	if( ReflectInstanceAttr(pObj, zProp, nProp) ){
		PH7_NativeSetAttrStr(pVm, pThis, "name", zProp, nProp);
		PH7_NativeSetAttrObj(pVm, pThis, RP_DYNOBJ, pObj);
		return PH7_OK;
	}
	return PH7_VmThrowException(pCtx, "ReflectionException",
		"Property %z::$%.*s does not exist", &pClass->sName, nProp, zProp);
}
static int vm_builtin_ReflectionProperty_getName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = "";
	int nName = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	}
	ph7_result_string(pCtx, zName, nName);
	return PH7_OK;
}
/*
 * getMangledName(): the name php stores the slot under —  plain for a public
 * property, "\0*\0name" for a protected one and "\0Class\0name" for a private
 * one. PHL's tables are not mangled, so the name is COMPOSED here; what php
 * exposes is the spelling, not the storage.
 */
static int vm_builtin_ReflectionProperty_getMangledName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	SyBlob sOut;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) || sRef.pAttr == 0 ){
		ph7_result_string(pCtx, sRef.zName, sRef.nName);
		return PH7_OK;
	}
	if( sRef.pAttr->iProtection == PH7_CLASS_PROT_PUBLIC ){
		ph7_result_string(pCtx, sRef.zName, sRef.nName);
		return PH7_OK;
	}
	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);
	SyBlobAppend(&sOut, "\0", 1);
	if( sRef.pAttr->iProtection == PH7_CLASS_PROT_PROTECTED ){
		SyBlobAppend(&sOut, "*", 1);
	}else{
		ph7_class *pDecl = ReflectMemberDecl(&sRef);
		SyBlobAppend(&sOut, SyStringData(&pDecl->sName), SyStringLength(&pDecl->sName));
	}
	SyBlobAppend(&sOut, "\0", 1);
	SyBlobAppend(&sOut, sRef.zName, (sxu32)sRef.nName);
	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/* The boolean predicates, all off the declared attribute. */
static int ReflectPropFlag(ph7_context *pCtx, int iWhat)
{
	ReflectMemberRef sRef;
	int bYes = 0;
	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){
		ph7_class_attr *pAttr = sRef.pAttr;
		switch( iWhat ){
		case 0: bYes = pAttr->iProtection == PH7_CLASS_PROT_PUBLIC; break;
		case 1: bYes = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE; break;
		case 2: bYes = pAttr->iProtection == PH7_CLASS_PROT_PROTECTED; break;
		case 3: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) != 0; break;
		case 4: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET) != 0; break;
		case 5: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0; break;
		case 6: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0; break;
		case 7: bYes = 1; break;                                    /* isDefault */
		case 8: bYes = 0; break;                                    /* isDynamic */
		case 9: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) != 0; break;
		default:
			bYes = (pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_SET)) != 0;
			break;
		}
	}else if( iWhat == 0 || iWhat == 8 ){
		/* A dynamic property is public and, by definition, not a default one. */
		bYes = 1;
	}
	ph7_result_bool(pCtx, bYes);
	return PH7_OK;
}
#define REFLECT_PROP_FLAG(NAME,WHAT) \
	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \
	{ \
		SXUNUSED(nArg); \
		SXUNUSED(apArg); \
		return ReflectPropFlag(pCtx, WHAT); \
	}
REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPublic, 0)
REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPrivate, 1)
REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isProtected, 2)
REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isPrivateSet, 3)
REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isProtectedSet, 4)
REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isStatic, 5)
REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isReadOnly, 6)
REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isDefault, 7)
REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isDynamic, 8)
REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isVirtual, 9)
REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_hasHooks, 10)

/* php has no abstract properties outside an interface stub, and PHL none at
 * all; isLazy() answers what is true of a VM without lazy objects. */
static int vm_builtin_ReflectionProperty_false(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx, 0);
	return PH7_OK;
}
/*
 * isPromoted(): the property came from a constructor-promoted parameter. PHL
 * records promotion on the ARGUMENT (VM_FUNC_ARG_PROMOTED), not on the
 * attribute, so the constructor's parameter list is what answers.
 */
static int vm_builtin_ReflectionProperty_isPromoted(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	ph7_class_method *pCons;
	ph7_vm_func_arg *aArg;
	sxu32 n;
	int bYes = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){
		ph7_class *pDecl = ReflectMemberDecl(&sRef);
		pCons = PH7_ClassExtractMethod(pDecl, "__construct", sizeof("__construct")-1);
		if( pCons ){
			aArg = (ph7_vm_func_arg *)SySetBasePtr(&pCons->sFunc.aArgs);
			for( n = 0 ; n < SySetUsed(&pCons->sFunc.aArgs) ; n++ ){
				if( (aArg[n].iFlags & VM_FUNC_ARG_PROMOTED) == 0 ){
					continue;
				}
				if( SyStringLength(&aArg[n].sName) == (sxu32)sRef.nName
				 && SyMemcmp(SyStringData(&aArg[n].sName), sRef.zName, (sxu32)sRef.nName) == 0 ){
					bYes = 1;
					break;
				}
			}
		}
	}
	ph7_result_bool(pCtx, bYes);
	return PH7_OK;
}
static int vm_builtin_ReflectionProperty_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) || sRef.pAttr == 0 ){
		ph7_result_int(pCtx, 1);   /* a dynamic property is public */
		return PH7_OK;
	}
	ph7_result_int64(pCtx, ReflectPropModifiers(sRef.pAttr));
	return PH7_OK;
}
static int vm_builtin_ReflectionProperty_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultClassOf(pCtx, ReflectMemberDecl(&sRef));
}
static int vm_builtin_ReflectionProperty_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr
	 && SyStringLength(&sRef.pAttr->sDoc) > 0 ){
		ph7_result_string(pCtx, SyStringData(&sRef.pAttr->sDoc),
			(int)SyStringLength(&sRef.pAttr->sDoc));
	}else{
		ph7_result_bool(pCtx, 0);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionProperty_hasType(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx, ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP)
		&& sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0);
	return PH7_OK;
}
static int vm_builtin_ReflectionProperty_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) || sRef.pAttr == 0
	 || (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0
	 || SyStringLength(&sRef.pAttr->sTypeName) < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultObject(pCtx, ReflectMakeType(pCtx,
		SyStringData(&sRef.pAttr->sTypeName), (int)SyStringLength(&sRef.pAttr->sTypeName)));
}
static int vm_builtin_ReflectionProperty_hasDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) || sRef.pAttr == 0 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	if( SySetUsed(&sRef.pAttr->aByteCode) > 0 || sRef.pAttr->pNativeValue ){
		ph7_result_bool(pCtx, 1);
		return PH7_OK;
	}
	/* An UNTYPED property with no initializer still defaults to null. */
	ph7_result_bool(pCtx, (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0);
	return PH7_OK;
}
static int vm_builtin_ReflectionProperty_getDefaultValue(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	ph7_value sValue;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) || sRef.pAttr == 0
	 || SySetUsed(&sRef.pAttr->aByteCode) < 1 ){
		/* php 8.5 deprecates the question when there is no default — an
		 * UNTYPED property still has one (null), a typed one without an
		 * initializer does not. */
		if( sRef.pAttr == 0 || (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) ){
			PH7_VmThrowError(pCtx->pVm, 0, E_DEPRECATED,
				"ReflectionProperty::getDefaultValue() for a property without a default "
				"value is deprecated, use ReflectionProperty::hasDefaultValue() to check "
				"if the default value exists");
		}
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	/* Same evaluation path the VM uses for an omitted call argument */
	PH7_MemObjInit(pCtx->pVm, &sValue);
	VmLocalExec(pCtx->pVm, &sRef.pAttr->aByteCode, &sValue, FALSE);
	ph7_result_value(pCtx, &sValue);
	PH7_MemObjRelease(&sValue);
	return PH7_OK;
}
/* php 8.1 made every reflected member accessible; the setter is a no-op it kept. */
static int vm_builtin_ReflectionProperty_setAccessible(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return PH7_OK;
}
/* getValue()/setValue()/isInitialized() all need the same receiver check. */
static sxi32 ReflectPropReceiver(ph7_context *pCtx, ReflectMemberRef *pRef,
	ph7_value *pObject, ph7_class_instance **ppThis, const char *zWho)
{
	*ppThis = 0;
	SXUNUSED(pRef);
	if( pObject && (pObject->iFlags & MEMOBJ_OBJ) ){
		*ppThis = (ph7_class_instance *)pObject->x.pOther;
		return PH7_OK;
	}
	return PH7_VmThrowException(pCtx, "TypeError",
		"ReflectionProperty::%s(): Argument #1 ($object) must be provided for instance properties",
		zWho);
}
static int vm_builtin_ReflectionProperty_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	ph7_class_instance *pObj;
	VmClassAttr *pVmAttr;
	ph7_value *pValue;
	sxi32 rc;
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){
		return ReflectStaticSlotRead(pCtx, sRef.pClass, sRef.pAttr);
	}
	rc = ReflectPropReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pObj, "getValue");
	if( rc != PH7_OK ){
		return rc;
	}
	pVmAttr = ReflectInstanceAttr(pObj, sRef.zName, sRef.nName);
	if( pVmAttr == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){
		ph7_class *pDecl = pVmAttr->pAttr && pVmAttr->pAttr->pDeclClass
			? pVmAttr->pAttr->pDeclClass : pObj->pClass;
		return PH7_VmThrowException(pCtx, "Error",
			"Typed property %z::$%.*s must not be accessed before initialization",
			&pDecl->sName, sRef.nName, sRef.zName);
	}
	pValue = PH7_ClassInstanceExtractAttrValue(pObj, pVmAttr);
	if( pValue ){
		ph7_result_value(pCtx, pValue);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionProperty_setValue(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	ph7_class_instance *pObj;
	VmClassAttr *pVmAttr;
	ph7_value *pSlot;
	sxi32 rc;
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) || nArg < 1 ){
		return PH7_OK;
	}
	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){
		/* php's one-argument spelling for a static: setValue($value). Two
		 * arguments, or one OBJECT, means the instance form was used and the
		 * value is the second. */
		ph7_value *pVal = apArg[0];
		if( nArg > 1 ){
			pVal = apArg[1];
		}else if( apArg[0]->iFlags & MEMOBJ_OBJ ){
			return PH7_OK;
		}
		return ReflectStaticSlotWrite(pCtx, sRef.pClass, sRef.pAttr, pVal);
	}
	rc = ReflectPropReceiver(pCtx, &sRef, apArg[0], &pObj, "setValue");
	if( rc != PH7_OK ){
		return rc;
	}
	pVmAttr = ReflectInstanceAttr(pObj, sRef.zName, sRef.nName);
	if( pVmAttr == 0 ){
		return PH7_OK;
	}
	{
		ph7_value *pVal = nArg > 1 ? apArg[1] : 0;
		ph7_value sNull;
		PH7_MemObjInit(pCtx->pVm, &sNull);
		if( pVal == 0 ){
			pVal = &sNull;
		}
		rc = ReflectEnforceStore(pCtx, pVmAttr->nIdx, pVal);
		if( rc != SXRET_OK ){
			PH7_MemObjRelease(&sNull);
			return rc;
		}
		pSlot = (ph7_value *)SySetAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);
		if( pSlot ){
			PH7_MemObjStore(pVal, pSlot);
			pVmAttr->iState &= ~VM_CLASS_ATTR_UNINIT;
		}
		PH7_MemObjRelease(&sNull);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionProperty_isInitialized(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	ph7_class_instance *pObj;
	VmClassAttr *pVmAttr;
	sxi32 rc;
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_STATIC) ){
		SyHashEntry *pSlot;
		rc = ReflectMaterializeStatics(pCtx, sRef.pClass);
		if( rc != SXRET_OK ){
			return rc;
		}
		pSlot = SyHashGet(&pCtx->pVm->hTypedSlot, (const void *)&sRef.pAttr->nIdx, sizeof(sxu32));
		ph7_result_bool(pCtx, pSlot == 0
			|| (((VmClassAttr *)pSlot->pUserData)->iState & VM_CLASS_ATTR_UNINIT) == 0);
		return PH7_OK;
	}
	rc = ReflectPropReceiver(pCtx, &sRef, nArg > 0 ? apArg[0] : 0, &pObj, "isInitialized");
	if( rc != PH7_OK ){
		return rc;
	}
	pVmAttr = ReflectInstanceAttr(pObj, sRef.zName, sRef.nName);
	ph7_result_bool(pCtx, pVmAttr != 0 && (pVmAttr->iState & VM_CLASS_ATTR_UNINIT) == 0);
	return PH7_OK;
}
/*
 * The property HOOKS (php 8.4). PHL compiles `public $p { get => ...; }` into
 * synthesized methods named `__phl_hook_get_NAME` / `__phl_hook_set_NAME`, so
 * the reflector php answers is a ReflectionMethod over one of those.
 */
static int ReflectHookMethod(ph7_context *pCtx, ReflectMemberRef *pRef, int bSet,
	ph7_class_instance **ppOut)
{
	char zName[128];
	ph7_value sClass, sName;
	ph7_value *apCtor[2];
	ph7_class *pDecl;
	sxi32 rc;
	int nName;
	*ppOut = 0;
	if( pRef->pAttr == 0 ){
		return PH7_OK;
	}
	if( (pRef->pAttr->iFlags & (bSet ? PH7_CLASS_ATTR_HOOK_SET : PH7_CLASS_ATTR_HOOK_GET)) == 0 ){
		return PH7_OK;
	}
	pDecl = ReflectMemberDecl(pRef);
	nName = SyBufferFormat(zName, sizeof(zName), "%s%.*s",
		bSet ? "__phl_hook_set_" : "__phl_hook_get_", pRef->nName, pRef->zName);
	PH7_MemObjInit(pCtx->pVm, &sClass);
	PH7_MemObjInit(pCtx->pVm, &sName);
	ph7_value_string(&sClass, SyStringData(&pDecl->sName), (int)SyStringLength(&pDecl->sName));
	ph7_value_string(&sName, zName, nName);
	apCtor[0] = &sClass;
	apCtor[1] = &sName;
	*ppOut = ReflectConstruct(pCtx, "ReflectionMethod", 2, apCtor, &rc);
	PH7_MemObjRelease(&sClass);
	PH7_MemObjRelease(&sName);
	return rc;
}
static int vm_builtin_ReflectionProperty_getHooks(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	ph7_value *pOut = ph7_context_new_array(pCtx);
	int iHook;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pOut == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){
		ph7_result_value(pCtx, pOut);
		return PH7_OK;
	}
	for( iHook = 0 ; iHook < 2 ; iHook++ ){
		ph7_class_instance *pMeth = 0;
		ph7_value sVal, *pKey;
		sxi32 rc = ReflectHookMethod(pCtx, &sRef, iHook, &pMeth);
		if( rc != PH7_OK ){
			return rc;
		}
		if( pMeth == 0 ){
			continue;
		}
		pKey = ph7_context_new_scalar(pCtx);
		if( pKey == 0 ){
			PH7_ClassInstanceUnref(pMeth);
			break;
		}
		ph7_value_string(pKey, iHook ? "set" : "get", 3);
		PH7_MemObjInit(pCtx->pVm, &sVal);
		sVal.x.pOther = pMeth;
		sVal.iFlags = MEMOBJ_OBJ;
		ph7_array_add_elem(pOut, pKey, &sVal);   /* takes its own reference */
		PH7_ClassInstanceUnref(pMeth);
	}
	ph7_result_value(pCtx, pOut);
	return PH7_OK;
}
/* `get`/`set` out of a PropertyHookType case (or the bare string php also takes). */
static int ReflectHookKind(ph7_value *pArg)
{
	const char *z;
	int n;
	if( pArg == 0 ){
		return -1;
	}
	if( pArg->iFlags & MEMOBJ_OBJ ){
		ph7_class_instance *pCase = (ph7_class_instance *)pArg->x.pOther;
		ph7_value *pVal = PH7_NativeAttr(pCase, "value");
		if( pVal == 0 || (pVal->iFlags & MEMOBJ_STRING) == 0 ){
			return -1;
		}
		z = (const char *)SyBlobData(&pVal->sBlob);
		n = (int)SyBlobLength(&pVal->sBlob);
	}else{
		z = ph7_value_to_string(pArg, &n);
	}
	if( n == 3 && SyMemcmp(z, "get", 3) == 0 ){
		return 0;
	}
	if( n == 3 && SyMemcmp(z, "set", 3) == 0 ){
		return 1;
	}
	return -1;
}
static int vm_builtin_ReflectionProperty_hasHook(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	int iHook = ReflectHookKind(nArg > 0 ? apArg[0] : 0);
	int bYes = 0;
	if( iHook >= 0 && ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) && sRef.pAttr ){
		bYes = (sRef.pAttr->iFlags & (iHook ? PH7_CLASS_ATTR_HOOK_SET : PH7_CLASS_ATTR_HOOK_GET)) != 0;
	}
	ph7_result_bool(pCtx, bYes);
	return PH7_OK;
}
static int vm_builtin_ReflectionProperty_getHook(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	ph7_class_instance *pMeth = 0;
	int iHook = ReflectHookKind(nArg > 0 ? apArg[0] : 0);
	sxi32 rc;
	if( iHook < 0 || !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	rc = ReflectHookMethod(pCtx, &sRef, iHook, &pMeth);
	if( rc != PH7_OK ){
		return rc;
	}
	if( pMeth == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultObject(pCtx, pMeth);
}
static int vm_builtin_ReflectionProperty_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ReflectMemberRef sRef;
	ph7_value sTarget;
	const char *zClass;
	int nClass, rc;
	if( pThis == 0 || !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) || sRef.pAttr == 0 ){
		ph7_result_value(pCtx, ph7_context_new_array(pCtx));
		return PH7_OK;
	}
	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);
	PH7_MemObjInit(pCtx->pVm, &sTarget);
	ph7_value_string(&sTarget, zClass, nClass);
	/* 8 = Attribute::TARGET_PROPERTY */
	rc = ReflectBuildAttrs(pCtx, &sRef.pAttr->aAttrs, "attr", &sTarget,
		sRef.zName, sRef.nName, 0, 8, nArg, apArg);
	PH7_MemObjRelease(&sTarget);
	return rc;
}
static int vm_builtin_ReflectionProperty_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return ReflectExportSelf(pCtx, "__reflect_export_prop", 0);
}
/* ---- ReflectionClassConstant ---- */
/* ReflectionClassConstant::__construct(object|string $class, string $constant) */
static int vm_builtin_ReflectionClassConstant_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class *pClass, *pDecl = 0;
	const char *zConst;
	int nConst;
	SySet aMembers;
	sxu32 n;
	int bFound = 0;
	if( pThis == 0 || nArg < 2 ){
		return PH7_OK;
	}
	pClass = ReflectResolveClass(pVm, apArg[0]);
	if( pClass == 0 ){
		const char *zName;
		int nName;
		zName = ph7_value_to_string(apArg[0], &nName);
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Class \"%.*s\" does not exist", nName, zName);
	}
	zConst = ph7_value_to_string(apArg[1], &nConst);
	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));
	ReflectMembers(pVm, pClass, &aMembers, 0);
	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){
		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);
		if( pM->iKind == REFLECT_MEMBER_CONST && ReflectKeyIs(pM, zConst, nConst) ){
			pDecl = pM->pDecl ? pM->pDecl : pClass;
			bFound = 1;
			break;
		}
	}
	SySetRelease(&aMembers);
	if( !bFound ){
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Constant %z::%.*s does not exist", &pClass->sName, nConst, zConst);
	}
	/* php's $class is the DECLARING class, not the one asked about. */
	pClass = pDecl;
	PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pClass->sName),
		(int)SyStringLength(&pClass->sName));
	PH7_NativeSetAttrStr(pVm, pThis, "name", zConst, nConst);
	return PH7_OK;
}
static int vm_builtin_ReflectionClassConstant_getValue(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	ph7_value *pVal;
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) || sRef.pAttr == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	rc = ReflectConstSlot(pCtx, sRef.pClass, sRef.pAttr, &pVal);
	if( rc != SXRET_OK ){
		return rc;
	}
	if( pVal ){
		ph7_result_value(pCtx, pVal);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
static int ReflectConstFlag(ph7_context *pCtx, int iWhat)
{
	ReflectMemberRef sRef;
	int bYes = 0;
	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) && sRef.pAttr ){
		ph7_class_attr *pAttr = sRef.pAttr;
		switch( iWhat ){
		case 0: bYes = pAttr->iProtection == PH7_CLASS_PROT_PUBLIC; break;
		case 1: bYes = pAttr->iProtection == PH7_CLASS_PROT_PRIVATE; break;
		case 2: bYes = pAttr->iProtection == PH7_CLASS_PROT_PROTECTED; break;
		case 3: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_FINAL) != 0; break;
		case 4: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) != 0; break;
		case 5: bYes = ReflectHasDeprecated(&pAttr->aAttrs); break;
		default: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) != 0; break;
		}
	}
	ph7_result_bool(pCtx, bYes);
	return PH7_OK;
}
#define REFLECT_CONST_FLAG(NAME,WHAT) \
	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \
	{ \
		SXUNUSED(nArg); \
		SXUNUSED(apArg); \
		return ReflectConstFlag(pCtx, WHAT); \
	}
REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isPublic, 0)
REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isPrivate, 1)
REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isProtected, 2)
REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isFinal, 3)
REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isEnumCase, 4)
REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_isDeprecated, 5)
REFLECT_CONST_FLAG(vm_builtin_ReflectionClassConstant_hasType, 6)

static int vm_builtin_ReflectionClassConstant_getModifiers(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) || sRef.pAttr == 0 ){
		ph7_result_int(pCtx, 0);
		return PH7_OK;
	}
	ph7_result_int64(pCtx, ReflectConstModifiers(sRef.pAttr));
	return PH7_OK;
}
static int vm_builtin_ReflectionClassConstant_getDeclaringClass(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultClassOf(pCtx, ReflectMemberDecl(&sRef));
}
static int vm_builtin_ReflectionClassConstant_getDocComment(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) && sRef.pAttr
	 && SyStringLength(&sRef.pAttr->sDoc) > 0 ){
		ph7_result_string(pCtx, SyStringData(&sRef.pAttr->sDoc),
			(int)SyStringLength(&sRef.pAttr->sDoc));
	}else{
		ph7_result_bool(pCtx, 0);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionClassConstant_getType(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) || sRef.pAttr == 0
	 || (sRef.pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0
	 || SyStringLength(&sRef.pAttr->sTypeName) < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultObject(pCtx, ReflectMakeType(pCtx,
		SyStringData(&sRef.pAttr->sTypeName), (int)SyStringLength(&sRef.pAttr->sTypeName)));
}
static int vm_builtin_ReflectionClassConstant_getAttributes(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ReflectMemberRef sRef;
	ph7_value sTarget;
	const char *zClass;
	int nClass, rc;
	if( pThis == 0 || !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) || sRef.pAttr == 0 ){
		ph7_result_value(pCtx, ph7_context_new_array(pCtx));
		return PH7_OK;
	}
	PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);
	PH7_MemObjInit(pCtx->pVm, &sTarget);
	ph7_value_string(&sTarget, zClass, nClass);
	/* 16 = Attribute::TARGET_CLASS_CONSTANT */
	rc = ReflectBuildAttrs(pCtx, &sRef.pAttr->aAttrs, "attr", &sTarget,
		sRef.zName, sRef.nName, 0, 16, nArg, apArg);
	PH7_MemObjRelease(&sTarget);
	return rc;
}
static int vm_builtin_ReflectionClassConstant_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return ReflectExportSelf(pCtx, "__reflect_export_cconst", 0);
}
/*
 * Declare chunk 3. Called from PH7_VmInstallReflectionLib where it used to be
 * compiled — after chunks 1 and 2, whose ReflectionClass and ReflectionMethod
 * these answer, and after PropertyHookType, which hasHook()/getHook() declare.
 *
 * The method tables are in php's own DECLARATION order.
 */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionMember(ph7_vm *pVm)
{
	static const PH7_NativePropDef aMemberProp[] = {
		{ "name",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 } },
		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 } },
	};
	static const PH7_NativePropDef aPropProp[] = {
		{ "name",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 } },
		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 } },
		/* PHL-only: the instance a DYNAMIC property was reached through (§7.4 (e)) */
		{ RP_DYNOBJ, PH7_MOD_PROTECTED, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 } },
	};
	static const PH7_NativeConstDef aPropConst[] = {
		{ "IS_STATIC",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 16,   0, 0.0 },
		{ "IS_READONLY",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 128,  0, 0.0 },
		{ "IS_PUBLIC",        PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,    0, 0.0 },
		{ "IS_PROTECTED",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,    0, 0.0 },
		{ "IS_PRIVATE",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,    0, 0.0 },
		{ "IS_ABSTRACT",      PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 64,   0, 0.0 },
		{ "IS_PROTECTED_SET", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2048, 0, 0.0 },
		{ "IS_PRIVATE_SET",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4096, 0, 0.0 },
		{ "IS_VIRTUAL",       PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 512,  0, 0.0 },
		{ "IS_FINAL",         PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32,   0, 0.0 },
	};
	static const PH7_NativeMethodDef aPropMethod[] = {
		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },
		{ "__construct", PH7_MOD_PUBLIC, "object|string $class, string $property", "",
		  vm_builtin_ReflectionProperty_construct },
		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionProperty_toString },
		{ "getName",     PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionProperty_getName },
		{ "getMangledName", PH7_MOD_PUBLIC, "", "string",
		  vm_builtin_ReflectionProperty_getMangledName },
		{ "getValue",    PH7_MOD_PUBLIC, "?object $object = null", "",
		  vm_builtin_ReflectionProperty_getValue },
		{ "setValue",    PH7_MOD_PUBLIC, "mixed $objectOrValue, mixed $value = ?", "",
		  vm_builtin_ReflectionProperty_setValue },
		{ "getRawValue", PH7_MOD_PUBLIC, "object $object", "mixed",
		  vm_builtin_ReflectionProperty_getValue },
		{ "setRawValue", PH7_MOD_PUBLIC, "object $object, mixed $value", "void",
		  vm_builtin_ReflectionProperty_setValue },
		/* On a non-lazy object — the only kind PHL has — php's two lazy writers
		 * are an ordinary raw write and a no-op. */
		{ "setRawValueWithoutLazyInitialization", PH7_MOD_PUBLIC,
		  "object $object, mixed $value", "void", vm_builtin_ReflectionProperty_setValue },
		{ "skipLazyInitialization", PH7_MOD_PUBLIC, "object $object", "void",
		  vm_builtin_ReflectionProperty_noop },
		{ "isLazy",      PH7_MOD_PUBLIC, "object $object", "bool",
		  vm_builtin_ReflectionProperty_false },
		{ "isInitialized", PH7_MOD_PUBLIC, "?object $object = null", "",
		  vm_builtin_ReflectionProperty_isInitialized },
		{ "isPublic",    PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionProperty_isPublic },
		{ "isPrivate",   PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionProperty_isPrivate },
		{ "isProtected", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionProperty_isProtected },
		{ "isPrivateSet", PH7_MOD_PUBLIC, "", "bool",
		  vm_builtin_ReflectionProperty_isPrivateSet },
		{ "isProtectedSet", PH7_MOD_PUBLIC, "", "bool",
		  vm_builtin_ReflectionProperty_isProtectedSet },
		{ "isStatic",    PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionProperty_isStatic },
		{ "isReadOnly",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isReadOnly },
		{ "isDefault",   PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionProperty_isDefault },
		{ "isDynamic",   PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isDynamic },
		/* PHL has no abstract properties: the modifier only exists on an
		 * interface's hooked property stub, which PHL does not model. */
		{ "isAbstract",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_false },
		{ "isVirtual",   PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isVirtual },
		{ "isPromoted",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isPromoted },
		{ "getModifiers", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionProperty_getModifiers },
		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionProperty_getDeclaringClass },
		{ "getDocComment", PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionProperty_getDocComment },
		{ "setAccessible", PH7_MOD_PUBLIC, "bool $accessible", "",
		  vm_builtin_ReflectionProperty_setAccessible },
		{ "getType",     PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionProperty_getType },
		/* php's settable type differs from the declared one only for a hooked
		 * property with a widening `set` — which PHL does not model. */
		{ "getSettableType", PH7_MOD_PUBLIC, "", "?ReflectionType",
		  vm_builtin_ReflectionProperty_getType },
		{ "hasType",     PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionProperty_hasType },
		{ "hasDefaultValue", PH7_MOD_PUBLIC, "", "bool",
		  vm_builtin_ReflectionProperty_hasDefaultValue },
		{ "getDefaultValue", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionProperty_getDefaultValue },
		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",
		  vm_builtin_ReflectionProperty_getAttributes },
		{ "hasHooks",    PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_hasHooks },
		{ "getHooks",    PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionProperty_getHooks },
		{ "hasHook",     PH7_MOD_PUBLIC, "PropertyHookType $type", "bool",
		  vm_builtin_ReflectionProperty_hasHook },
		{ "getHook",     PH7_MOD_PUBLIC, "PropertyHookType $type", "?ReflectionMethod",
		  vm_builtin_ReflectionProperty_getHook },
		{ "isFinal",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_false },
	};
	static const PH7_NativeConstDef aConstConst[] = {
		{ "IS_PUBLIC",    PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 1,  0, 0.0 },
		{ "IS_PROTECTED", PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 2,  0, 0.0 },
		{ "IS_PRIVATE",   PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 4,  0, 0.0 },
		{ "IS_FINAL",     PH7_MOD_PUBLIC, PH7_NATIVE_VAL_INT, 32, 0, 0.0 },
	};
	static const PH7_NativeMethodDef aConstMethod[] = {
		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },
		{ "__construct", PH7_MOD_PUBLIC, "object|string $class, string $constant", "",
		  vm_builtin_ReflectionClassConstant_construct },
		{ "__toString",  PH7_MOD_PUBLIC, "", "string",
		  vm_builtin_ReflectionClassConstant_toString },
		{ "getName",     PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionProperty_getName },
		{ "getValue",    PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClassConstant_getValue },
		{ "isPublic",    PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClassConstant_isPublic },
		{ "isPrivate",   PH7_MOD_PUBLIC, "", "", vm_builtin_ReflectionClassConstant_isPrivate },
		{ "isProtected", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionClassConstant_isProtected },
		{ "isFinal",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClassConstant_isFinal },
		{ "getModifiers", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionClassConstant_getModifiers },
		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionClassConstant_getDeclaringClass },
		{ "getDocComment", PH7_MOD_PUBLIC, "", "",
		  vm_builtin_ReflectionClassConstant_getDocComment },
		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",
		  vm_builtin_ReflectionClassConstant_getAttributes },
		{ "isEnumCase",  PH7_MOD_PUBLIC, "", "bool",
		  vm_builtin_ReflectionClassConstant_isEnumCase },
		{ "isDeprecated", PH7_MOD_PUBLIC, "", "bool",
		  vm_builtin_ReflectionClassConstant_isDeprecated },
		{ "hasType",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClassConstant_hasType },
		{ "getType",     PH7_MOD_PUBLIC, "", "?ReflectionType",
		  vm_builtin_ReflectionClassConstant_getType },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "ReflectionProperty", 0, "Reflector", PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aPropMethod, SX_ARRAYSIZE(aPropMethod),
		  aPropConst, SX_ARRAYSIZE(aPropConst),
		  aPropProp, SX_ARRAYSIZE(aPropProp), 0, 0 },
		{ "ReflectionClassConstant", 0, "Reflector", PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aConstMethod, SX_ARRAYSIZE(aConstMethod),
		  aConstConst, SX_ARRAYSIZE(aConstConst),
		  aMemberProp, SX_ARRAYSIZE(aMemberProp), 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));
}
/*
 * ---------------------------------------------------------------------------
 * The ReflectionEnum family — chunk 6, and the end of __phl_rcinfo.
 *
 * Three classes that are very nearly their parents: `ReflectionEnum` IS a
 * `ReflectionClass` that refuses a non-enum, and `ReflectionEnumUnitCase` IS a
 * `ReflectionClassConstant` that refuses a constant which is not a case. What
 * is their own is the CASE list, which the engine already holds in declaration
 * order on `ph7_class::aEnumCases` — the chunk read a marshalled copy of it out
 * of `__phl_rcinfo`, the descriptor builder that dies with this conversion.
 * ---------------------------------------------------------------------------
 */

/* The enum case named zName, or NULL. A case is a class CONSTANT, so the match
 * is case-SENSITIVE: php's hasCase('hearts') is false for `case Hearts`. */
static ph7_class_attr * ReflectEnumCase(ph7_class *pClass, const char *zName, int nName)
{
	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);
	sxu32 n;
	if( nName < 1 ){
		return 0;
	}
	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){
		if( (int)SyStringLength(&apCase[n]->sName) == nName
		 && SyMemcmp(SyStringData(&apCase[n]->sName), zName, (sxu32)nName) == 0 ){
			return apCase[n];
		}
	}
	return 0;
}
/*
 * One case reflector for an already-validated case. php answers a BackedCase
 * for a backed enum and a UnitCase for a pure one, and both carry the same two
 * slots ReflectionClassConstant does — an enum cannot extend anything, so the
 * declaring class is always the enum itself.
 */
static ph7_class_instance * ReflectEnumCaseNew(ph7_context *pCtx, ph7_class *pEnum,
	const SyString *pCase)
{
	ph7_vm *pVm = pCtx->pVm;
	const char *zClass = pEnum->nEnumBacking
		? "ReflectionEnumBackedCase" : "ReflectionEnumUnitCase";
	ph7_class *pRC = PH7_VmExtractClass(pVm, zClass, (sxu32)SyStrlen(zClass), FALSE, 0);
	ph7_class_instance *pObj = pRC ? PH7_NewClassInstance(pVm, pRC) : 0;
	if( pObj == 0 ){
		return 0;
	}
	PH7_NativeSetAttrStr(pVm, pObj, "class", SyStringData(&pEnum->sName),
		(int)SyStringLength(&pEnum->sName));
	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(pCase), (int)SyStringLength(pCase));
	return pObj;
}
/* ReflectionEnum::__construct(object|string $objectOrClass) */
static int vm_builtin_ReflectionEnum_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass;
	sxi32 rc = vm_builtin_ReflectionClass_construct(pCtx, nArg, apArg);
	if( rc != PH7_OK ){
		return rc; /* "Class %s does not exist", already php's */
	}
	pClass = ReflectClassOf(pCtx);
	if( pClass && (pClass->iFlags & PH7_CLASS_ENUM) == 0 ){
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Class \"%z\" is not an enum", &pClass->sName);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionEnum_hasCase(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	const char *zName;
	int nName;
	if( pClass == 0 || nArg < 1 ){
		ph7_result_bool(pCtx, 0);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0], &nName);
	ph7_result_bool(pCtx, ReflectEnumCase(pClass, zName, nName) != 0);
	return PH7_OK;
}
/*
 * getCase(): php tells the two failures apart — a name that is a constant but
 * not a case is "X::K is not a case", one that is neither is "Case X::K does
 * not exist". The chunk answered the second for both.
 */
static int vm_builtin_ReflectionEnum_getCase(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	ph7_class_attr *pCase;
	const char *zName;
	int nName;
	if( pClass == 0 || nArg < 1 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zName = ph7_value_to_string(apArg[0], &nName);
	pCase = ReflectEnumCase(pClass, zName, nName);
	if( pCase == 0 ){
		if( nName > 0 && SyHashGet(&pClass->hConst, (const void *)zName, (sxu32)nName) ){
			return PH7_VmThrowException(pCtx, "ReflectionException",
				"%z::%.*s is not a case", &pClass->sName, nName, zName);
		}
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Case %z::%.*s does not exist", &pClass->sName, nName, zName);
	}
	return ReflectResultObject(pCtx, ReflectEnumCaseNew(pCtx, pClass, &pCase->sName));
}
static int vm_builtin_ReflectionEnum_getCases(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	ph7_value *pOut = ph7_context_new_array(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pOut == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( pClass ){
		ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);
		sxu32 n;
		for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){
			ReflectTypeListAdd(pCtx, pOut, ReflectEnumCaseNew(pCtx, pClass, &apCase[n]->sName));
		}
	}
	ph7_result_value(pCtx, pOut);
	return PH7_OK;
}
static int vm_builtin_ReflectionEnum_isBacked(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx, pClass != 0 && pClass->nEnumBacking != 0);
	return PH7_OK;
}
static int vm_builtin_ReflectionEnum_getBackingType(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	const char *zText;
	ph7_class_instance *pType;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pClass == 0 || pClass->nEnumBacking == 0 ){
		ph7_result_null(pCtx); /* a PURE enum has no backing type */
		return PH7_OK;
	}
	zText = (pClass->nEnumBacking & MEMOBJ_INT) ? "int" : "string";
	pType = ReflectMakeType(pCtx, zText, (int)SyStrlen(zText));
	if( pType == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_NativeResultObject(pCtx, pType);
	return PH7_OK;
}
/*
 * ReflectionEnumUnitCase::__construct(object|string $class, string $constant).
 *
 * The parent raises for a constant that does not exist; what is left is "not a
 * case", and php words that one WITHOUT asking whether the class is an enum at
 * all — `new ReflectionEnumUnitCase('Plain', 'K')` on an ordinary class says
 * `Constant Plain::K is not a case`, not "is not an enum" as the chunk did.
 * A non-enum's constant can never carry the ENUMCASE bit, so the one screen
 * answers both shapes.
 */
static int vm_builtin_ReflectionEnumCase_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectMemberRef sRef;
	sxi32 rc = vm_builtin_ReflectionClassConstant_construct(pCtx, nArg, apArg);
	if( rc != PH7_OK ){
		return rc;
	}
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) || sRef.pAttr == 0 ){
		return PH7_OK;
	}
	if( (sRef.pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE) == 0 ){
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Constant %z::%.*s is not a case", &sRef.pClass->sName, sRef.nName, sRef.zName);
	}
	return PH7_OK;
}
/* ReflectionEnumBackedCase::__construct() — the same, plus php's screen for a
 * PURE enum's case, which has no backing value to answer with. */
static int vm_builtin_ReflectionEnumBackedCase_construct(ph7_context *pCtx, int nArg,
	ph7_value **apArg)
{
	ReflectMemberRef sRef;
	sxi32 rc = vm_builtin_ReflectionEnumCase_construct(pCtx, nArg, apArg);
	if( rc != PH7_OK ){
		return rc;
	}
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) || sRef.pAttr == 0 ){
		return PH7_OK;
	}
	if( sRef.pClass->nEnumBacking == 0 ){
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Enum case %z::%.*s is not a backed case", &sRef.pClass->sName,
			sRef.nName, sRef.zName);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionEnumCase_getEnum(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_vm *pVm = pCtx->pVm;
	ReflectMemberRef sRef;
	ph7_class *pRC;
	ph7_class_instance *pObj;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	pRC = PH7_VmExtractClass(pVm, "ReflectionEnum", sizeof("ReflectionEnum")-1, FALSE, 0);
	pObj = pRC ? PH7_NewClassInstance(pVm, pRC) : 0;
	if( pObj == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	PH7_NativeSetAttrStr(pVm, pObj, "name", SyStringData(&sRef.pClass->sName),
		(int)SyStringLength(&sRef.pClass->sName));
	return ReflectResultObject(pCtx, pObj);
}
/* getBackingValue(): the `value` the case singleton carries. The singleton is
 * the constant's own value, so the parent's accessor materializes it. */
static int vm_builtin_ReflectionEnumCase_getBackingValue(ph7_context *pCtx, int nArg,
	ph7_value **apArg)
{
	ReflectMemberRef sRef;
	ph7_value *pVal, *pBacking;
	sxi32 rc;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) || sRef.pAttr == 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	rc = ReflectConstSlot(pCtx, sRef.pClass, sRef.pAttr, &pVal);
	if( rc != SXRET_OK ){
		return rc;
	}
	pBacking = (pVal && (pVal->iFlags & MEMOBJ_OBJ))
		? PH7_NativeAttr((ph7_class_instance *)pVal->x.pOther, "value") : 0;
	if( pBacking ){
		ph7_result_value(pCtx, pBacking);
	}else{
		ph7_result_null(pCtx);
	}
	return PH7_OK;
}
/*
 * Declare the three. Called from PH7_VmInstallReflectionLib where chunk 6 used
 * to be compiled, so both parents are installed (PH7_ClassInherit COPIES a
 * base's methods down — a native subclass needs its parent declared first).
 *
 * The uncloneable/unserializable flags are php's answer for each class and do
 * NOT come down with the inheritance: without them `clone $enumReflector`
 * reached the private __clone it inherited and reported that instead.
 */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionEnum(ph7_vm *pVm)
{
	static const PH7_NativeMethodDef aEnumMethod[] = {
		{ "__construct",    PH7_MOD_PUBLIC, "object|string $objectOrClass", "",
		  vm_builtin_ReflectionEnum_construct },
		{ "hasCase",        PH7_MOD_PUBLIC, "string $name", "bool",
		  vm_builtin_ReflectionEnum_hasCase },
		{ "getCase",        PH7_MOD_PUBLIC, "string $name", "ReflectionEnumUnitCase",
		  vm_builtin_ReflectionEnum_getCase },
		{ "getCases",       PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionEnum_getCases },
		{ "isBacked",       PH7_MOD_PUBLIC, "", "bool",  vm_builtin_ReflectionEnum_isBacked },
		{ "getBackingType", PH7_MOD_PUBLIC, "", "?ReflectionNamedType",
		  vm_builtin_ReflectionEnum_getBackingType },
	};
	static const PH7_NativeMethodDef aCaseMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "object|string $class, string $constant", "",
		  vm_builtin_ReflectionEnumCase_construct },
		{ "getEnum",     PH7_MOD_PUBLIC, "", "ReflectionEnum",
		  vm_builtin_ReflectionEnumCase_getEnum },
		/* php redeclares getValue() on the case reflector for its narrower
		 * return type; the body is the parent's. */
		{ "getValue",    PH7_MOD_PUBLIC, "", "UnitEnum",
		  vm_builtin_ReflectionClassConstant_getValue },
	};
	static const PH7_NativeMethodDef aBackedMethod[] = {
		{ "__construct",     PH7_MOD_PUBLIC, "object|string $class, string $constant", "",
		  vm_builtin_ReflectionEnumBackedCase_construct },
		{ "getBackingValue", PH7_MOD_PUBLIC, "", "string|int",
		  vm_builtin_ReflectionEnumCase_getBackingValue },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "ReflectionEnum", "ReflectionClass", 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aEnumMethod, SX_ARRAYSIZE(aEnumMethod), 0, 0, 0, 0, 0, 0 },
		{ "ReflectionEnumUnitCase", "ReflectionClassConstant", 0,
		  PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aCaseMethod, SX_ARRAYSIZE(aCaseMethod), 0, 0, 0, 0, 0, 0 },
		{ "ReflectionEnumBackedCase", "ReflectionEnumUnitCase", 0,
		  PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aBackedMethod, SX_ARRAYSIZE(aBackedMethod), 0, 0, 0, 0, 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));
}
/*
 * Install the Reflection API. There is nothing to register here any more: the
 * global `__reflect_*`/`__phl_rcinfo` thunk table this function existed for is
 * EMPTY — every one of them became a method of the class that always owned it,
 * so Reflection now adds no global name to the php namespace at all.
 */
PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)
{
	return PH7_VmInstallReflectionLib(&(*pVm));
}
