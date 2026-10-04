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
static ph7_class * ReflectMethodDeclClass(ph7_class *pClass, ph7_class_method *pMeth);
/* Append pIface to the set unless it is already there (dedup by pointer). */
static void ReflectIfaceAppend(SySet *pOut, ph7_class *pIface)
{
	ph7_class **apKnown;
	sxu32 n;
	if( pIface == 0 ){
		return;
	}
	apKnown = (ph7_class **)SySetBasePtr(pOut);
	for( n = 0 ; n < SySetUsed(pOut) ; n++ ){
		if( apKnown[n] == pIface ){
			return;
		}
	}
	SySetPut(pOut, (const void *)&pIface);
}
static void ReflectFlattenIfaces(ph7_vm *pVm, ph7_class *pClass, SySet *pOut, int iDepth);
/* Append pIface's OWN flattened list, BACKWARDS — see ReflectFlattenIfaces. */
static void ReflectIfaceAppendOwn(ph7_vm *pVm, SySet *pOut, ph7_class *pIface, int iDepth)
{
	SySet aSub;
	ph7_class **ap;
	sxu32 n;
	SySetInit(&aSub, pOut->pAllocator, sizeof(ph7_class *));
	ReflectFlattenIfaces(pVm, pIface, &aSub, iDepth + 1);
	ap = (ph7_class **)SySetBasePtr(&aSub);
	for( n = SySetUsed(&aSub) ; n > 0 ; n-- ){
		ReflectIfaceAppend(pOut, ap[n-1]);
	}
	SySetRelease(&aSub);
}
/*
 * The interface list php reports for pClass, IN PHP'S ORDER — the one
 * `getInterfaceNames()`, `getInterfaces()`, `class_implements()` and the
 * export's `implements` / `extends` clause all publish. Caller owns the set
 * (SySetInit with sizeof(ph7_class *)).
 *
 * zend builds it once at link time out of two primitives, and every ordering
 * rule below is one of them (all measured against php 8.5.9, 271 internal
 * classes and interfaces agreeing row for row):
 *
 *   - `zend_do_inherit_interfaces` copies a list in BACKWARDS. That is what
 *     puts `IteratorIterator` before its own `Traversable` and `Iterator`
 *     (`RecursiveIteratorIterator` reads `OuterIterator, Traversable,
 *     Iterator`), and what makes `ErrorException` list `Throwable, Stringable`
 *     where its parent `Exception` lists them the other way round.
 *   - An INTERNAL class hands its interfaces over ONE AT A TIME
 *     (`zend_class_implements`), so each is followed immediately by its own
 *     list; a compiled one declares them as a BLOCK, so all the declared ones
 *     come first and the inherited ones after. `ArrayObject` is the first
 *     shape, `class Z implements P1, P2` the second.
 *   - The parent's list opens the answer, backwards for an internal class and
 *     for a compiled one that declares NO interface of its own (which is the
 *     `zend_do_inherit_interfaces` path again), forwards otherwise.
 *
 * This engine walked a flattened set of its own and matched php on none of
 * those: 71 of the 197 classes both engines share answered a different order.
 */
static void ReflectFlattenIfaces(ph7_vm *pVm, ph7_class *pClass, SySet *pOut, int iDepth)
{
	SySet aDecl;
	ph7_class **ap;
	sxu32 n, nDecl;
	int bInternal, bIface;
	if( pClass == 0 || iDepth > REFLECT_WALK_MAX_DEPTH ){
		return;
	}
	bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;
	bIface = (pClass->iFlags & PH7_CLASS_INTERFACE) != 0;
	/*
	 * php auto-implements Stringable for an INTERNAL class that declares its
	 * own __toString, and does it while REGISTERING the class -- before the
	 * parent's interfaces are inherited and before its own are named. That is
	 * the whole reason `CachingIterator` opens with Stringable and its
	 * subclass, which only inherits the method, does not. A compiled class
	 * gets the same auto-implement at the END of its declared list instead,
	 * which compile_class.c already spells.
	 */
	if( bInternal && !bIface ){
		SyHashEntry *pTs = SyHashGet(&pClass->hMethod,
			(const void *)"__toString", sizeof("__toString")-1);
		if( pTs && ReflectMethodDeclClass(pClass,
				(ph7_class_method *)pTs->pUserData) == pClass ){
			ReflectIfaceAppend(pOut, PH7_VmExtractClass(pVm,
				"Stringable", sizeof("Stringable")-1, FALSE, 0));
		}
	}
	/* What the class DECLARES, in declaration order. PHL keeps the first
	 * parent of an `interface B extends A, C` on the base chain and the rest
	 * in aInterface; php keeps no base chain for an interface at all. */
	SySetInit(&aDecl, pOut->pAllocator, sizeof(ph7_class *));
	if( bIface && pClass->pBase ){
		SySetPut(&aDecl, (const void *)&pClass->pBase);
	}
	ap = (ph7_class **)SySetBasePtr(&pClass->aInterface);
	for( n = 0 ; n < SySetUsed(&pClass->aInterface) ; n++ ){
		SySetPut(&aDecl, (const void *)&ap[n]);
	}
	nDecl = SySetUsed(&aDecl);
	/* The parent class's own list opens the answer. */
	if( !bIface && pClass->pBase ){
		SySet aParent;
		SySetInit(&aParent, pOut->pAllocator, sizeof(ph7_class *));
		ReflectFlattenIfaces(pVm, pClass->pBase, &aParent, iDepth + 1);
		ap = (ph7_class **)SySetBasePtr(&aParent);
		if( bInternal || nDecl == 0 ){
			for( n = SySetUsed(&aParent) ; n > 0 ; n-- ){
				ReflectIfaceAppend(pOut, ap[n-1]);
			}
		}else{
			for( n = 0 ; n < SySetUsed(&aParent) ; n++ ){
				ReflectIfaceAppend(pOut, ap[n]);
			}
		}
		SySetRelease(&aParent);
	}
	ap = (ph7_class **)SySetBasePtr(&aDecl);
	if( bInternal ){
		for( n = 0 ; n < nDecl ; n++ ){
			ReflectIfaceAppend(pOut, ap[n]);
			ReflectIfaceAppendOwn(pVm, pOut, ap[n], iDepth);
		}
	}else{
		for( n = 0 ; n < nDecl ; n++ ){
			ReflectIfaceAppend(pOut, ap[n]);
		}
		for( n = 0 ; n < nDecl ; n++ ){
			ReflectIfaceAppendOwn(pVm, pOut, ap[n], iDepth);
		}
	}
	SySetRelease(&aDecl);
}
/* The list every Reflection door asks for -- and `class_implements()`, which is
 * the same answer under another name (vm_builtin_class.c). */
PH7_PRIVATE void PH7_ReflectInterfacesOf(ph7_vm *pVm, ph7_class *pClass, SySet *pOut)
{
	ReflectFlattenIfaces(pVm, pClass, pOut, 0);
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
	/* A record the class only got from an INTERFACE belongs to the interface:
	 * php's scope for `ReflectionMethod('AbstractImpl','ifaceMethod')` is the
	 * interface that declared it, and the base walk above cannot see one
	 * (an interface is not on the pBase chain of the class implementing it). */
	if( pDecl->iFlags & PH7_CLASS_INTERFACE ){
		return pDecl;
	}
	{
		ph7_class **apIface = (ph7_class **)SySetBasePtr(&pDecl->aInterface);
		sxu32 n;
		for( n = 0 ; n < SySetUsed(&pDecl->aInterface) ; n++ ){
			SyHashEntry *pEntry = SyHashGet(&apIface[n]->hMethod,
				(const void *)SyStringData(&pMeth->sFunc.sName),
				SyStringLength(&pMeth->sFunc.sName));
			if( pEntry && (ph7_class_method *)pEntry->pUserData == pMeth ){
				return ReflectMethodDeclClass(apIface[n], pMeth);
			}
		}
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
	int bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;
	int iPart;
	sxu32 nChain = 0, iLevel, nLev, nT;
	while( pWalk && nChain < (sxu32)(REFLECT_WALK_MAX_DEPTH + 1) ){
		aChain[nChain++] = pWalk;
		pWalk = pWalk->pBase;
	}
	SySetInit(&aTmp, &pVm->sAllocator, sizeof(SyHashEntry *));
	/* PROPERTIES of an INTERNAL class come out base-first, and that is php's
	 * registration order rather than a rule of its own: a user class declares its
	 * own properties and THEN inherits (`class B extends A` reports B's before
	 * A's), while an internal one is registered against its parent and declares
	 * afterwards — so ErrorException reports Exception's five and then its own
	 * $severity. Only the property table flips: php lists an internal class's
	 * METHODS and CONSTANTS own-first like everything else, so the walk below
	 * runs the two tables in opposite level orders. */
	for( iPart = 0 ; iPart < 3 ; iPart++ ){
	  for( nLev = 0 ; nLev < nChain ; nLev++ ){
		ph7_class *pLevel;
		int iTab = iPart;
		iLevel = (iPart == 0 && bInternal) ? nChain - 1 - nLev : nLev;
		pLevel = aChain[iLevel];
		/* --- Properties (hAttr, iPart 0) and constants/enum cases (hConst,
		 * iPart 1) — php's two separate member namespaces. Each table is
		 * collected and emitted independently; the CONSTANT flag still decides
		 * which kind comes out. --- */
		if( iPart < 2 ){
			SyHash *pSrcHash = iTab ? &pLevel->hConst : &pLevel->hAttr;
			SyHash *pRefHash = iTab ? &pClass->hConst : &pClass->hAttr;
			SySetReset(&aTmp);
			SyHashResetLoopCursor(pSrcHash);
			while( (pEntry = SyHashGetNextEntry(pSrcHash)) != 0 ){
				ph7_class_attr *pAttr = (ph7_class_attr *)pEntry->pUserData;
				ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pLevel);
				if( pAttr->iFlags & PH7_CLASS_ATTR_HIDDEN ){
					/* A native class's engine slot: php holds that state in its own C
					 * struct and reports no property for it at all. */
					continue;
				}
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
				sMember.pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pLevel);
				sMember.pAttr = pAttr;
				sMember.pMeth = 0;
				SySetPut(pOut, (const void *)&sMember);
			}
			continue;
		}
		/* --- Methods. The reported name is the hash-entry KEY, not the
		 * function's own name (see ReflectMember::sKey). --- */
		SySetReset(&aTmp);
		SyHashResetLoopCursor(&pLevel->hMethod);
		while( (pEntry = SyHashGetNextEntry(&pLevel->hMethod)) != 0 ){
			ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;
			ph7_class *pDecl = ReflectMethodDeclClass(pClass, pMeth);
			/* A property HOOK is compiled into a method (__phl_hook_get_NAME /
			 * __phl_hook_set_NAME) so that inheritance and `parent::` work on
			 * it, but php has no method there at all: it reports the hook on
			 * the PROPERTY. Listing it would put a PHL-only name on a
			 * php-visible surface. */
			if( pEntry->nKeyLen > sizeof("__phl_hook_")-1
			 && SyMemcmp(pEntry->pKey, "__phl_hook_", sizeof("__phl_hook_")-1) == 0 ){
				continue;
			}
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
		ph7_value *pValue = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);
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
			PH7_ClassInstanceCtorFailed(pThis);
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
static ph7_vm_func * ReflectResolveCallable(ph7_vm *pVm, ph7_value *pTarget,
	ph7_value *pMethodArg, ph7_class **ppClass, ph7_class_method **ppMeth,
	ph7_user_func **ppHost, ph7_class_instance **ppClosure)
{
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
		if( pMeth && (pMeth->iFlags & PH7_CLASS_ATTR_FABRICATED) ){
			/* `Closure::__invoke` named over an OBJECT is the one method whose IDENTITY
			 * and whose BODY come from different places: php builds an internal method
			 * record on the Closure class and copies the closure's parameter list into
			 * it. So report the method (name, class, modifiers) and the closure's own
			 * function (parameters, return type, what invoke() runs) together. Named
			 * over a class instead, there is no closure and the declaration -- which is
			 * empty -- is all there is, which is why php's class-level `__invoke`
			 * answers zero parameters. */
			ph7_class_instance *pClo = ReflectValueClosure(pVm, pTarget);
			if( pClo ){
				ph7_vm_func *pBody = ReflectResolveCallable(pVm, pTarget, 0, 0, 0, 0, 0);
				if( pBody ){
					if( ppClass ){ *ppClass = pClass; }
					if( ppMeth ){ *ppMeth = pMeth; }
					if( ppClosure ){ *ppClosure = pClo; }
					return pBody;
				}
			}
		}
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
				pEntry = PH7_VmGetHostFunction(pVm, SyBlobData(&pFn->sBlob), SyBlobLength(&pFn->sBlob), FALSE);
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
		pEntry = PH7_VmGetUserFunction(pVm, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob), FALSE);
		if( pEntry ){
			return (ph7_vm_func *)pEntry->pUserData;
		}
		pEntry = PH7_VmGetHostFunction(pVm, SyBlobData(&pTarget->sBlob), SyBlobLength(&pTarget->sBlob), FALSE);
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
	int k;
	char cQuote = 0;
	for( k = 0 ; k < n ; k++ ){
		if( cQuote ){
			if( z[k] == '\\' && k + 1 < n ){
				k++;
			}else if( z[k] == cQuote ){
				cQuote = 0;
			}
		}else if( z[k] == '\'' || z[k] == '"' ){
			/* BOTH spellings open a run. A native method's zSig lives in C, so
			 * `string $separator = ","` is the natural way to write it -- and
			 * tracking only the single quote let the comma INSIDE that default
			 * split the parameter in two, which is how setCsvControl() came to
			 * report four parameters, one of them named `$"`. */
			cQuote = z[k];
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
 * One `C::K` (or `C::class`) term of a declared default, evaluated.
 *
 * php's stub writes these as SOURCE — `string $class = SplFileInfo::class`,
 * `int $flags = FilesystemIterator::KEY_AS_PATHNAME|…` — and the reflector then
 * answers both the text (the export line) and the VALUE (getDefaultValue()). A
 * native method's zSig is that same source, so the value has to be read out of
 * the class here; there are no compiled parameter records to hold it.
 */
static int ReflectSigClassConst(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class_attr *pAttr;
	ph7_class *pClass;
	ph7_value *pValue;
	int iSep;
	ReflectSigTrim(&z, &n);
	for( iSep = 0 ; iSep + 1 < n ; ++iSep ){
		if( z[iSep] == ':' && z[iSep+1] == ':' ){
			break;
		}
	}
	if( iSep + 1 >= n || iSep < 1 ){
		return 0;
	}
	pClass = PH7_VmExtractClass(pVm, z, (sxu32)iSep, TRUE, 0);
	if( pClass == 0 ){
		return 0;
	}
	z += iSep + 2;
	n -= iSep + 2;
	if( n == (int)sizeof("class")-1 && SyMemcmp(z, "class", sizeof("class")-1) == 0 ){
		/* `C::class` is the class NAME, and php prints the name it was DECLARED
		 * with rather than the spelling in the signature. */
		SyString *pName = &pClass->sName;
		ph7_value_string(pOut, SyStringData(pName), (int)SyStringLength(pName));
		return 1;
	}
	pAttr = PH7_ClassExtractConstant(pClass, z, (sxu32)n);
	if( pAttr == 0 || (pAttr->iFlags & PH7_CLASS_ATTR_CONSTANT) == 0 ){
		return 0;
	}
	if( pAttr->nIdx == SXU32_HIGH ){
		/* Not materialized yet: bring it into being exactly as a direct `C::K`
		 * read would. An enum CASE is a singleton with its own materializer --
		 * running the constant initializer over one answers nothing, which is
		 * why `RoundingMode::HalfAwayFromZero` could not be reduced at all. */
		sxi32 rcConst = (pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)
			? VmEnumMaterializeCase(pVm, pClass, pAttr)
			: VmClassConstEvalOnDemand(pVm, pClass, pAttr);
		if( rcConst != SXRET_OK ){
			return 0;
		}
	}
	pValue = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, pAttr->nIdx);
	if( pValue == 0 ){
		return 0;
	}
	PH7_MemObjStore(pValue, pOut);
	return 1;
}
/*
 * A GLOBAL constant named by a declared default — php's stub writes
 * `int $severity = E_ERROR` and both surfaces read it from here: the export
 * prints the NAME (rule 28: the argument was never folded, so php still has the
 * source) and getDefaultValue() answers what it expands to. Answers 0 for
 * anything that is not a plain identifier naming a defined constant, which is
 * what keeps `null`/`true`/a bare number out of this branch.
 */
static int ReflectSigIsIdent(const char *z, int n)
{
	int k;
	if( n < 1 || (z[0] != '_' && !SyisAlpha(z[0])) ){
		return 0;
	}
	for( k = 1 ; k < n ; ++k ){
		if( z[k] != '_' && z[k] != '\\' && !SyisAlphaNum(z[k]) ){
			return 0;
		}
	}
	return 1;
}
static int ReflectSigGlobalConst(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)
{
	SyHashEntry *pEntry;
	ph7_constant *pCons;
	ReflectSigTrim(&z, &n);
	if( !ReflectSigIsIdent(z, n) ){
		return 0;
	}
	pEntry = SyHashGet(&pCtx->pVm->hConstant, (const void *)z, (sxu32)n);
	if( pEntry == 0 ){
		return 0;
	}
	pCons = (ph7_constant *)pEntry->pUserData;
	if( pCons == 0 || pCons->xExpand == 0 ){
		return 0;
	}
	pCons->xExpand(pOut, pCons->pUserData);
	return 1;
}
/*
 * A constant EXPRESSION: one term, or the `|` fold php's own stubs write for a
 * flags default (`KEY_AS_PATHNAME | CURRENT_AS_FILEINFO | SKIP_DOTS`, and
 * `SQLITE3_OPEN_READWRITE | SQLITE3_OPEN_CREATE`). Either kind of name may
 * stand in it -- a class constant or a global one -- because php's stubs write
 * both, and a term is looked up as whichever it turns out to be.
 */
static int ReflectSigConstExpr(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)
{
	sxi64 iAcc = 0;
	int iStart = 0;
	int k, nTerm = 0;
	if( !ReflectSigHas(z, n, "::", 2) && !ReflectSigHas(z, n, "|", 1) ){
		return 0;
	}
	for( k = 0 ; k <= n ; ++k ){
		if( k < n && z[k] != '|' ){
			continue;
		}
		if( !ReflectSigClassConst(pCtx, &z[iStart], k - iStart, pOut)
		 && !ReflectSigGlobalConst(pCtx, &z[iStart], k - iStart, pOut) ){
			return 0;
		}
		nTerm++;
		if( k < n || nTerm > 1 ){
			/* A fold is arithmetic, so every arm has to be an integer; a lone
			 * term keeps whatever type it had (`C::class` is a string). */
			if( (pOut->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP|MEMOBJ_OBJ)) != 0 ){
				return 0;
			}
			PH7_MemObjToInteger(pOut);
			iAcc |= pOut->x.iVal;
		}
		iStart = k + 1;
	}
	if( nTerm > 1 ){
		ph7_value_int64(pOut, iAcc);
	}
	return nTerm > 0;
}
/* One hex digit's value, or -1. */
static int ReflectHexVal(char c)
{
	if( c >= '0' && c <= '9' ) return c - '0';
	if( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
	if( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
	return -1;
}
/*
 * php keeps the SOURCE of a constant EXPRESSION in a stub's default and prints
 * it back verbatim, and two more shapes of one live in the signature table
 * beside the `C::K` and `A|B` forms already handled: an integer written in a
 * RADIX (`0777` for mkdir's permissions) and a product (`2 * 1024 * 1024` for
 * SplTempFileObject's memory bound). Both are text php never reduces on the
 * page; both have to be reduced for getDefaultValue().
 */
static int ReflectSigRadixInt(const char *z, int n, sxi64 *pOut)
{
	int iRadix = 8, k = 1;
	sxi64 iVal = 0;
	ReflectSigTrim(&z, &n);
	if( n < 2 || z[0] != '0' ){
		return 0;
	}
	if( z[1] == 'x' || z[1] == 'X' ){       iRadix = 16; k = 2; }
	else if( z[1] == 'o' || z[1] == 'O' ){  iRadix = 8;  k = 2; }
	else if( z[1] == 'b' || z[1] == 'B' ){  iRadix = 2;  k = 2; }
	if( k >= n ){
		return 0;
	}
	for( ; k < n ; ++k ){
		int iDigit = ReflectHexVal(z[k]);
		if( iDigit < 0 || iDigit >= iRadix ){
			return 0;
		}
		iVal = iVal * iRadix + iDigit;
	}
	*pOut = iVal;
	return 1;
}
/* `a * b * c` over integer literals and integer constants. */
static int ReflectSigProduct(ph7_context *pCtx, const char *z, int n, ph7_value *pOut)
{
	sxi64 iAcc = 1;
	int iStart = 0, k, nTerm = 0;
	if( ReflectSigFindUnquoted(z, n, '*') < 0 ){
		return 0;
	}
	for( k = 0 ; k <= n ; ++k ){
		const char *zTerm;
		int nTermLen;
		sxi64 iVal = 0;
		sxu8 bReal = 0;
		if( k < n && z[k] != '*' ){
			continue;
		}
		zTerm = &z[iStart];
		nTermLen = k - iStart;
		ReflectSigTrim(&zTerm, &nTermLen);
		if( nTermLen < 1 ){
			return 0;
		}
		if( !ReflectSigRadixInt(zTerm, nTermLen, &iVal) ){
			ph7_value *pTerm;
			if( SyStrIsNumeric(zTerm, (sxu32)nTermLen, &bReal, 0) == SXRET_OK && !bReal ){
				SyStrToInt64(zTerm, (sxu32)nTermLen, (void *)&iVal, 0);
			}else if( (pTerm = ph7_context_new_scalar(pCtx)) != 0
			       && (ReflectSigClassConst(pCtx, zTerm, nTermLen, pTerm)
			        || ReflectSigGlobalConst(pCtx, zTerm, nTermLen, pTerm))
			       && (pTerm->iFlags & (MEMOBJ_STRING|MEMOBJ_HASHMAP|MEMOBJ_OBJ)) == 0 ){
				PH7_MemObjToInteger(pTerm);
				iVal = pTerm->x.iVal;
			}else{
				return 0;
			}
		}
		iAcc *= iVal;
		nTerm++;
		iStart = k + 1;
	}
	if( nTerm < 2 ){
		return 0;
	}
	ph7_value_int64(pOut, iAcc);
	return 1;
}
/*
 * Is this default TEXT one php prints back as SOURCE rather than as a value?
 * The `C::K` and `A|B` forms are answered by their own readers above; these
 * two are the ones a plain literal reader would silently reduce to a number.
 */
static int ReflectSigIsSourceExpr(const char *z, int n)
{
	sxi64 iIgnored = 0;
	return ReflectSigFindUnquoted(z, n, '*') >= 0
	    || ReflectSigRadixInt(z, n, &iIgnored);
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
	if( n >= 2 && (z[0] == '\'' || z[0] == '"') && z[n-1] == z[0] ){
		/* Undo the escapes the signature writer emits, which are php's own set --
		 * the same one ReflectExportStrQ puts back when it prints the line. A row
		 * cannot hold these bytes any other way: a signature is a C string, so a
		 * NUL would END it, which is why trim()'s ` \n\r\t\v\x00` had no default
		 * row at all and its parameter answered isDefaultValueAvailable() false.
		 * BOTH quote spellings are accepted because both scanners in vm_arg_check.c
		 * step over either one: a native method's zSig lives in C, so `= \"static\"`
		 * is the natural way to write Closure::bindTo's default. */
		SyBlob sOut;
		char cQuote = z[0];
		int k;
		SyBlobInit(&sOut,&pCtx->pVm->sAllocator);
		for( k = 1 ; k < n - 1 ; k++ ){
			char c = z[k];
			if( c == '\\' && k + 1 < n - 1 ){
				char e = z[k+1];
				int bTook = 1;
				switch( e ){
					case 'n': c = 0x0A; break;
					case 'r': c = 0x0D; break;
					case 't': c = 0x09; break;
					case 'v': c = 0x0B; break;
					case 'f': c = 0x0C; break;
					case 'e': c = 0x1B; break;
					case 'x': {
						int h1 = ReflectHexVal(k + 3 < n - 1 ? z[k+2] : 0);
						int h2 = ReflectHexVal(k + 3 < n - 1 ? z[k+3] : 0);
						if( h1 < 0 || h2 < 0 ){
							bTook = 0;
							break;
						}
						c = (char)((h1 << 4) | h2);
						k += 2;   /* the two hex digits; the 'x' below */
						break;
					}
					default:
						bTook = (e == cQuote || e == '\\');
						c = e;
						break;
				}
				if( bTook ){
					k++;
				}else{
					c = '\\';
				}
			}
			SyBlobAppend(&sOut,(const void *)&c,sizeof(char));
		}
		ph7_value_string(pOut,(const char *)SyBlobData(&sOut),(int)SyBlobLength(&sOut));
		SyBlobRelease(&sOut);
		return 1;
	}
	if( ReflectSigConstExpr(pCtx,z,n,pOut) ){
		return 1;
	}
	if( ReflectSigGlobalConst(pCtx,z,n,pOut) ){
		return 1;
	}
	if( ReflectSigProduct(pCtx,z,n,pOut) ){
		return 1;
	}
	{
		sxi64 iRadix = 0;
		if( ReflectSigRadixInt(z,n,&iRadix) ){
			ph7_value_int64(pOut,iRadix);
			return 1;
		}
	}
	if( n > 0 && SyStrIsNumeric(z,(sxu32)n,&bReal,0) == SXRET_OK ){
		/* php's own rule for the text form: a '.', an exponent or a hex marker
		 * makes it a float, everything else an int. */
		if( bReal || ReflectSigHas(z,n,".",1)
		 || ReflectSigHasNoCase(z,n,"e",1) || ReflectSigHasNoCase(z,n,"x",1) ){
#ifndef PH7_OMIT_FLOATING_POINT
			/* the VALUE is an out-parameter here; the return is a status, and
			 * reading it as the number made every float default 0.0 */
			sxreal rVal = 0.0;
			SyStrToReal(z,(sxu32)n,(void *)&rVal,0);
			ph7_value_double(pOut,rVal);
#else
			sxi64 iRaw = 0;
			SyStrToInt64(z,(sxu32)n,(void *)&iRaw,0);
			ph7_value_int64(pOut,iRaw);
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
 * The same reduction, for the door OUTSIDE this file that needs it.
 *
 * A declared signature's default is TEXT, and two places have to turn it into a
 * value: getDefaultValue() above, and the named-argument binder in
 * vm_arg_check.c, which materializes the parameters a named call SKIPPED
 * (`mkdir($d, recursive: true)` has to supply `$permissions`). That binder
 * carried a reader of its own -- null/true/false, `[]`, a quoted string and a
 * DECIMAL number -- so the two doors onto one value disagreed on every default
 * the small reader could not spell: `0777` bound as 777 (a directory created
 * mode 01411, which the next chdir() could not enter), and any default written
 * as a CONSTANT (`ENT_QUOTES | …`, `M_E`, `SORT_REGULAR`, `SplFileObject::class`,
 * `2 * 1024 * 1024` -- ~180 parameters) bound as "not passed", so php's own
 * `htmlspecialchars("<a>", encoding: 'UTF-8')` was an ArgumentCountError here.
 * There is one reader now and this is its door; `[]` stays with the caller
 * because the value it builds is a hashmap rather than a scalar.
 */
PH7_PRIVATE int PH7_VmSigDefaultToValue(ph7_context *pCtx,const char *z,int n,ph7_value *pOut)
{
	if( pCtx == 0 || z == 0 || n < 1 || pOut == 0 ){
		return 0;
	}
	return ReflectSigScalar(pCtx,z,n,pOut);
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
	SyString sType;            /* nByte == 0 -> untyped. For a COMPILED parameter this
	                            * points into zTypeBuf below, php's stored text with
	                            * `self`/`parent` resolved (see ReflectDeclScope). */
	char zTypeBuf[192];
	SyString sDefText;         /* signature-declared default TEXT (nByte == 0 -> none) */
	ph7_vm_func_arg *pArg;     /* compiled parameter, or NULL for a declared one */
	int bInternal;             /* owner is INTERNAL to php -- which for a COMPILED
	                            * parameter is the prelude's builtins, and decides
	                            * how php spells its default (see ReflectExportDefault) */
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
	if( n > 0 && z[0] == '~' ){
		/* The signature table's "declared here, screened by the builtin" marker
		 * (see vm_arg_check.c): php DECLARES this type and its C body asks for a
		 * tighter one, so Reflection reports what follows the marker. */
		z++;
		n--;
	}
	/* `= default` splits off first: everything after the first unquoted '='. */
	iEq = ReflectSigFindUnquoted(z,n,'=');
	if( iEq >= 0 ){
		zDef = &z[iEq+1];
		nDef = n - iEq - 1;
		ReflectSigTrim(&zDef,&nDef);
		n = iEq;
		ReflectSigTrim(&z,&n);
	}
	if( zDef && nDef == 1 && zDef[0] == '!' ){
		/* `= !` is the mirror image of `= ?` below, and it is php's own
		 * stub-versus-body mismatch in the argument COUNT rather than the type:
		 * the parameter is DECLARED with no default -- Reflection reports it
		 * required, and getNumberOfRequiredParameters() counts it -- while the
		 * C body's own argument screen lets the call omit it.
		 * `Dom\Node::insertBefore()`'s $child is the first: Reflection says two
		 * required parameters and `$p->insertBefore($n)` runs. So the marker
		 * vanishes HERE (no default, not optional) and is left standing for
		 * VmDeriveArityFromSig, which reads the `=` and lowers the minimum. */
		zDef = 0;
		nDef = 0;
	}else if( zDef && nDef == 1 && zDef[0] == '?' ){
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
		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], apArg[2], 0, 0, 0, 0);
		if( pFunc ){ pAttrs = &pFunc->aAttrs; pDeclCls = (ph7_class *)pFunc->pUserData; }
	}else if( nKind == 2 && SyMemcmp(zKind, "fn", 2) == 0 ){
		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], 0, 0, 0, 0, 0);
		if( pFunc ){ pAttrs = &pFunc->aAttrs; }
	}else if( nKind == 5 && SyMemcmp(zKind, "param", 5) == 0 ){
		ph7_vm_func *pFunc = ReflectResolveCallable(pCtx->pVm, apArg[1], apArg[2], 0, 0, 0, 0);
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
		}else if( pArgRec->pNativeValue ){
			/* A NATIVE attribute (php's own `#[Attribute(...)]` on Attribute and
			 * Deprecated): the argument is a literal, not byte-code. */
			PH7_NativeLiteralValue(pVm, pArgRec->pNativeValue, &sValue);
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
 * Declare the four type classes. Called from PH7_VmInstallReflection() where
 * chunk 4 used to be compiled, so `Stringable` (a core interface) already
 * exists. PH7_CLASS_NOCLONE is php's own rule for these: they are values the
 * engine hands out, and `clone $type` is an Error, not a copy.
 */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionTypes(ph7_vm *pVm)
{
	static const PH7_NativePropDef aBaseProp[] = {
		{ RT_TEXT,     PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },
		{ RT_NULLABLE, PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aBaseMethod[] = {
		{ "allowsNull", PH7_MOD_PUBLIC, "", "@bool",       vm_builtin_ReflectionType_allowsNull },
		{ "__toString", PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionType_toString },
	};
	static const PH7_NativePropDef aNamedProp[] = {
		{ RT_TNAME, PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aNamedMethod[] = {
		{ "getName",   PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionNamedType_getName },
		{ "isBuiltin", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionNamedType_isBuiltin },
	};
	static const PH7_NativePropDef aCompProp[] = {
		{ RT_TYPES, PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aCompMethod[] = {
		{ "getTypes", PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionType_getTypes },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "ReflectionType", 0, "Stringable", PH7_CLASS_ABSTRACT|PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aBaseMethod, SX_ARRAYSIZE(aBaseMethod), 0, 0, aBaseProp, SX_ARRAYSIZE(aBaseProp), 0, 0, 0 },
		{ "ReflectionNamedType", "ReflectionType", 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aNamedMethod, SX_ARRAYSIZE(aNamedMethod), 0, 0, aNamedProp, SX_ARRAYSIZE(aNamedProp), 0, 0, 0 },
		{ "ReflectionUnionType", "ReflectionType", 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aCompMethod, SX_ARRAYSIZE(aCompMethod), 0, 0, aCompProp, SX_ARRAYSIZE(aCompProp), 0, 0, 0 },
		{ "ReflectionIntersectionType", "ReflectionType", 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aCompMethod, SX_ARRAYSIZE(aCompMethod), 0, 0, aCompProp, SX_ARRAYSIZE(aCompProp), 0, 0, 0 },
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
			PH7_ClassInstanceCtorFailed(pThis);
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
 * A string the way php's reflection prints one, in either of php's TWO quotings.
 *
 * A USERLAND default prints single-quoted, with the NON-PRINTABLE bytes escaped
 * (php's smart_str_append_escaped) and the quote itself NOT escaped — php's own
 * output for "q'q" is 'q'q'. An INTERNAL one prints DOUBLE-quoted and escapes the
 * quote: php answers `string $enclosure = "\""` for str_getcsv where PHL used to
 * answer `'"'`. Every internal function with a string default was affected; the
 * pair was found on Closure::bindTo's `= "static"` while making that class native.
 */
static void ReflectExportStrQ(SyBlob *pOut, const char *zIn, sxu32 nIn, char cQuote)
{
	static const char zHexDigit[] = "0123456789ABCDEF";
	sxu32 i;
	SyBlobAppend(pOut, &cQuote, sizeof(char));
	for( i = 0 ; i < nIn ; i++ ){
		unsigned char c = (unsigned char)zIn[i];
		if( c == (unsigned char)cQuote && cQuote == '"' ){
			SyBlobAppend(pOut, "\\\"", sizeof("\\\"")-1);
			continue;
		}
		{
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
	}
	SyBlobAppend(pOut, &cQuote, sizeof(char));
}
/* php's userland quoting, which is what every existing caller means. */
static void ReflectExportStr(SyBlob *pOut, const char *zIn, sxu32 nIn)
{
	ReflectExportStrQ(pOut, zIn, nIn, '\'');
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
 * An array the way php's reflection prints one: a LIST — every key an integer,
 * in sequence from zero — prints no keys at all, and anything else prints one
 * for EVERY entry. That is why `[1, 'k' => 2]` comes out as
 * `[0 => 1, 'k' => 2]` while `[[1], [2 => 3]]` keeps its outer keys silent.
 */
static void ReflectExportArray(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal, int iDepth)
{
	ph7_hashmap *pMap = (ph7_hashmap *)pVal->x.pOther;
	ph7_hashmap_node *pEntry;
	sxi64 iExpect = 0;
	sxu32 n;
	int bList = 1;
	for( pEntry = pMap->pFirst, n = 0 ; n < pMap->nEntry && pEntry ; n++ ){
		if( pEntry->iType == HASHMAP_BLOB_NODE || pEntry->xKey.iKey != iExpect ){
			bList = 0;
			break;
		}
		iExpect++;
		pEntry = pEntry->pPrev; /* Reverse link: insertion order */
	}
	SyBlobAppend(pOut, "[", sizeof(char));
	for( pEntry = pMap->pFirst, n = 0 ; n < pMap->nEntry && pEntry ; n++ ){
		ph7_value *pMember = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pEntry->nValIdx);
		if( n > 0 ){
			SyBlobAppend(pOut, ", ", sizeof(", ")-1);
		}
		if( !bList ){
			if( pEntry->iType == HASHMAP_BLOB_NODE ){
				ReflectExportStr(pOut, (const char *)SyBlobData(&pEntry->xKey.sKey),
					SyBlobLength(&pEntry->xKey.sKey));
			}else{
				SyBlobFormat(pOut, "%qd", pEntry->xKey.iKey);
			}
			SyBlobAppend(pOut, " => ", sizeof(" => ")-1);
		}
		ReflectExportValue(pCtx, pOut, pMember, iDepth + 1);
		pEntry = pEntry->pPrev; /* Reverse link: insertion order */
	}
	SyBlobAppend(pOut, "]", sizeof(char));
}
/*
 * php's TYPE word in a `Constant [ ... ]` head -- for a CLASS constant, a
 * global one and an extension's listing alike. 0 means "an object", whose word
 * is its own class name and which only the caller can spell.
 */
static const char * ReflectExportTypeWord(ph7_value *pVal)
{
	if( pVal == 0 ){
		return "null";
	}
	if( (pVal->iFlags & (MEMOBJ_OBJ|MEMOBJ_NULL)) == MEMOBJ_OBJ ){
		return 0;
	}
	if( pVal->iFlags & MEMOBJ_NULL ){
		return "null";
	}
	if( pVal->iFlags & MEMOBJ_HASHMAP ){
		return "array";
	}
	if( pVal->iFlags & MEMOBJ_RES ){
		return "resource";
	}
	if( pVal->iFlags & MEMOBJ_BOOL ){
		return "bool";
	}
	if( pVal->iFlags & MEMOBJ_REAL ){
		return "float";
	}
	if( pVal->iFlags & MEMOBJ_INT ){
		return "int";
	}
	if( pVal->iFlags & MEMOBJ_STRING ){
		return "string";
	}
	return "null";
}
/* The text php prints between a constant's `{ ` and ` }`. NULL and FALSE are
 * both nothing at all, an array is `Array` and an object `Object`. */
static void ReflectExportConstValue(ph7_context *pCtx, SyBlob *pOut, ph7_value *pVal)
{
	if( pVal == 0 || (pVal->iFlags & MEMOBJ_NULL) ){
		return;
	}
	if( pVal->iFlags & MEMOBJ_HASHMAP ){
		SyBlobAppend(pOut, "Array", sizeof("Array")-1);
		return;
	}
	if( pVal->iFlags & MEMOBJ_OBJ ){
		SyBlobAppend(pOut, "Object", sizeof("Object")-1);
		return;
	}
	if( pVal->iFlags & MEMOBJ_BOOL ){
		if( pVal->x.iVal ){
			SyBlobAppend(pOut, "1", sizeof(char));
		}
		return;
	}
	{
		ph7_value sTmp;
		const char *zText;
		int nText;
		PH7_MemObjInit(pCtx->pVm, &sTmp);
		PH7_MemObjStore(pVal, &sTmp);
		zText = ph7_value_to_string(&sTmp, &nText);
		if( nText > 0 ){
			SyBlobAppend(pOut, zText, (sxu32)nText);
		}
		PH7_MemObjRelease(&sTmp);
	}
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
				if( PH7_ATTR_UNPRESENTED(pVmAttr)
				 || (pVmAttr->pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL) ){
					continue; /* class-level members are not part of the object */
				}
				pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);
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
		apOut[i] = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pNode->nValIdx);
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
			"Attempting to use non-attribute class \"%z\" as attribute", &pClass->sDisp);
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
	/* php's INTERNAL attributes carry a validator beside their mask, and the
	 * engine runs BOTH where the declaration compiles (GenStateCheckAttrPlacement)
	 * -- so a misplaced `#[\Deprecated]` or `#[\Override]` never reaches this
	 * far. What is left here is the USERLAND rule, which php checks only when
	 * someone asks: the mask below and the repetition test after it. */
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
			"Attribute \"%z\" cannot target %s (allowed targets: %s)", &pClass->sDisp,
			iBit < (int)SX_ARRAYSIZE(azReflectTarget) ? azReflectTarget[iBit] : "",
			SyBlobData(&sAllowed));
		SyBlobRelease(&sAllowed);
		return rc;
	}
	if( PH7_NativeAttrTruthy(pThis, RA_REP) && (iFlags & 128) == 0 ){
		return PH7_VmThrowException(pCtx, "Error",
			"Attribute \"%z\" must not be repeated", &pClass->sDisp);
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
			ph7_value *pMember = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, pEntry->nValIdx);
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
 * Declare ReflectionAttribute. Called from PH7_VmInstallReflection() where
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
		{ RA_NAME,   PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
		/* PHL-only, and PROTECTED so php code cannot reach them: the spec that
		 * reopens the target. php holds the same state on the C struct behind the
		 * object, invisible; PHL has no hidden-slot bit yet (recorded), so these
		 * four still show up in a var_dump where php shows only $name. */
		{ RA_SPEC,   PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL,   0, 0,  0.0 }, 0 },
		{ RA_IDX,    PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0,  0.0 }, 0 },
		{ RA_TARGET, PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,    0, 0,  0.0 }, 0 },
		{ RA_REP,    PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_BOOL,   0, 0,  0.0 }, 0 },
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
		  aProp, SX_ARRAYSIZE(aProp), 0, 0, 0 },
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
			ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj,
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
	ph7_constant *pCons = ReflectConstOf(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	/* The same fact the E_DEPRECATED at a NAMING reads, and the same one the
	 * export tags -- reading it here does not raise the notice, which is why
	 * this asks the record rather than expanding the constant. */
	ph7_result_bool(pCtx, pCons != 0 && pCons->zDeprecated != 0);
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
/* The ReflectionExtension builder every reflector's getExtension() answers with
 * -- defined further down, beside the class partition it reads. */
static int ReflectExtensionOf(ph7_context *pCtx, int iExt);
/* An engine constant belongs to the extension the partition places its name in;
 * a userland define() belongs to none, which php reports as null / false. */
static int ReflectConstExtId(ph7_context *pCtx)
{
	ph7_constant *pCons = ReflectConstOf(pCtx);
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = "";
	int nName = 0;
	if( pCons == 0 || pCons->bUserDefined ){
		return -1;
	}
	if( pThis ){
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	}
	return PH7_VmExtOfConstant(zName, nName);
}
static int vm_builtin_ReflectionConstant_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return ReflectExtensionOf(pCtx, ReflectConstExtId(pCtx));
}
static int vm_builtin_ReflectionConstant_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	int iExt = ReflectConstExtId(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( iExt < 0 ){
		ph7_result_bool(pCtx, 0);
	}else{
		ph7_result_string(pCtx, PH7_VmExtensionName(iExt), -1);
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
/* php's ZEND_ACC_NO_FILE_CACHE: the two constants whose value belongs to the
 * RUNNING process rather than to the build, so opcache must not bake them in. */
static int ReflectConstNoFileCache(const SyString *pName)
{
	static const char * const azNoCache[] = { "PHP_BINARY", "PHP_SAPI" };
	sxu32 n;
	for( n = 0 ; n < SX_ARRAYSIZE(azNoCache) ; ++n ){
		if( SyStringLength(pName) == SyStrlen(azNoCache[n])
		 && SyMemcmp(SyStringData(pName), azNoCache[n], SyStringLength(pName)) == 0 ){
			return 1;
		}
	}
	return 0;
}
/*
 * php's `Constant [ <persistent> int JSON_HEX_TAG ] { 1 }` -- one line, and the
 * same one an extension's Constants block lists. `<persistent>` is the ENGINE's
 * own: a define()d constant carries no tag at all, and one php deprecated the
 * SYMBOL of reads `<persistent, deprecated>`. The value is taken from the
 * constant's expander DIRECTLY, so asking for the export does not raise the
 * E_DEPRECATED that naming it would.
 */
static void ReflectExportGlobalConstLine(ph7_context *pCtx, SyBlob *pOut, ph7_constant *pCons)
{
	ph7_value sVal;
	const char *zType;
	PH7_MemObjInit(pCtx->pVm, &sVal);
	if( pCons->xExpand ){
		pCons->xExpand(&sVal, pCons->pUserData);
	}
	SyBlobAppend(pOut, "Constant [ ", sizeof("Constant [ ")-1);
	zType = ReflectExportTypeWord(&sVal);
	/* php's flag words, in its order. `persistent` is a MODULE's registration
	 * and a define()d constant has none; the three standard streams are the
	 * CLI SAPI's own per-REQUEST constants and have none either, which is what
	 * the resource test below says. `no_file_cache` is php's opcache flag, and
	 * it marks exactly the two constants whose value is this process's. */
	if( !pCons->bUserDefined && (sVal.iFlags & MEMOBJ_RES) == 0 ){
		if( pCons->zDeprecated ){
			SyBlobAppend(pOut, "<persistent, deprecated> ", sizeof("<persistent, deprecated> ")-1);
		}else if( ReflectConstNoFileCache(&pCons->sName) ){
			SyBlobAppend(pOut, "<persistent, no_file_cache> ",
				sizeof("<persistent, no_file_cache> ")-1);
		}else{
			SyBlobAppend(pOut, "<persistent> ", sizeof("<persistent> ")-1);
		}
	}
	if( zType ){
		SyBlobFormat(pOut, "%s ", zType);
	}else{
		SyBlobFormat(pOut, "%z ", &((ph7_class_instance *)sVal.x.pOther)->pClass->sName);
	}
	SyBlobFormat(pOut, "%z ] { ", &pCons->sName);
	ReflectExportConstValue(pCtx, pOut, &sVal);
	SyBlobAppend(pOut, " }\n", sizeof(" }\n")-1);
	PH7_MemObjRelease(&sVal);
}
static int vm_builtin_ReflectionConstant_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_constant *pCons = ReflectConstOf(pCtx);
	SyBlob sOut;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pCons == 0 ){
		ph7_result_string(pCtx, "", 0);
		return PH7_OK;
	}
	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);
	ReflectExportGlobalConstLine(pCtx, &sOut, pCons);
	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
/* ---- ReflectionExtension: one per name this build reports as loaded ---- */
/*
 * php matches the name case-insensitively and then KEEPS its own spelling, so
 * `new ReflectionExtension('DATE')` reports `date` and `spl` reports `SPL`.
 * A `phl.stub_extensions` name is loaded too and has no canonical spelling of
 * its own, so it keeps the caller's.
 */
static int vm_builtin_ReflectionExtension_construct(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	int nName = 0, iExt;
	const char *zName = nArg > 0 ? ph7_value_to_string(apArg[0], &nName) : "";
	if( pThis == 0 ){
		return PH7_OK;
	}
	iExt = PH7_VmExtensionLookup(zName, nName);
	if( iExt >= 0 ){
		const char *zCanon = PH7_VmExtensionName(iExt);
		PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", zCanon, (int)SyStrlen(zCanon));
		return PH7_OK;
	}
	if( PH7_VmExtensionIsLoaded(pCtx->pVm, zName, nName) ){
		PH7_NativeSetAttrStr(pCtx->pVm, pThis, "name", zName, nName);
		return PH7_OK;
	}
	return PH7_VmThrowException(pCtx, "ReflectionException",
		"Extension \"%.*s\" does not exist", nName, zName);
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
/*
 * The four listings an extension answers about ITSELF -- getFunctions(),
 * getClasses()/getClassNames(), getConstants() and getINIEntries(). All of them
 * are the partition walked in php's own registration order (which is not
 * alphabetical) and filtered against the live VM, so a build without one of the
 * compile-time extensions simply lists nothing for it.
 */
#define REFLECT_EXT_MAP        0   /* name => a fresh reflector over it */
#define REFLECT_EXT_NAMES      1   /* a plain LIST of the names */
#define REFLECT_EXT_VALUES     2   /* name => the value the engine holds */
typedef struct ReflectExtList ReflectExtList;
struct ReflectExtList {
	ph7_context *pCtx;
	ph7_value *pList;   /* the array being built */
	ph7_value *pVal;    /* one scratch value, reused for every entry */
	int iKind;          /* PH7_EXT_KIND_* */
	int iShape;         /* REFLECT_EXT_* */
};
/* A reflector over one internal name, built with its `name` slot already filled:
 * the constructor would only re-resolve what this walk already has. */
static int ReflectExtMakeReflector(ph7_context *pCtx, const char *zRefl,
	const char *zName, int nName, ph7_value *pOut)
{
	ph7_vm *pVm = pCtx->pVm;
	ph7_class *pClass = PH7_VmExtractClass(pVm, zRefl, (sxu32)SyStrlen(zRefl), FALSE, 0);
	ph7_class_instance *pObj;
	if( pClass == 0 || (pObj = PH7_NewClassInstance(pVm, pClass)) == 0 ){
		return 0;
	}
	PH7_NativeSetAttrStr(pVm, pObj, "name", zName, nName);
	PH7_MemObjRelease(pOut);
	pOut->x.pOther = pObj;
	pOut->iFlags = MEMOBJ_OBJ;
	return 1;
}
static int ReflectExtListStep(const char *zName, int nName, void *pData)
{
	ReflectExtList *p = (ReflectExtList *)pData;
	ph7_context *pCtx = p->pCtx;
	SyBlob sKey;
	char *zKey;
	int i, rc = 0;
	if( !PH7_VmInternalNameExists(pCtx->pVm, p->iKind, zName, nName) ){
		return 0;
	}
	/* ph7_array_add_strkey_elem() takes a NUL-terminated key, and the walk has
	 * only a name and a length -- so the key is built rather than borrowed,
	 * which is also where the fold happens. */
	SyBlobInit(&sKey, &pCtx->pVm->sAllocator);
	SyBlobAppend(&sKey, (const void *)zName, (sxu32)nName);
	SyBlobAppend(&sKey, (const void *)"", 1);
	zKey = (char *)SyBlobData(&sKey);
	if( p->iKind == PH7_EXT_KIND_FUNC ){
		/* php keys the map with the name the engine STORES, which is folded. */
		for( i = 0 ; i < nName ; ++i ){
			zKey[i] = (char)SyToLower(zKey[i]);
		}
	}
	switch( p->iShape ){
		case REFLECT_EXT_NAMES:
			ph7_value_reset_string_cursor(p->pVal);
			ph7_value_string(p->pVal, zName, nName);
			ph7_array_add_elem(p->pList, 0, p->pVal);
			SyBlobRelease(&sKey);
			return 0;
		case REFLECT_EXT_VALUES:
			if( p->iKind == PH7_EXT_KIND_INI ){
				SyBlob sVal;
				SyBlobInit(&sVal, &pCtx->pVm->sAllocator);
				PH7_VmIniGetStr(pCtx->pVm, zKey, &sVal);
				ph7_value_reset_string_cursor(p->pVal);
				if( PH7_VmIniIsUnset(pCtx->pVm, zKey) ){
					/* php shows the RAW value here, so a directive declared with
					 * no value is NULL rather than the empty string ini_get()
					 * makes of it. */
					PH7_MemObjRelease(p->pVal);
				}else{
					ph7_value_string(p->pVal, (const char *)SyBlobData(&sVal),
						(int)SyBlobLength(&sVal));
				}
				SyBlobRelease(&sVal);
			}else{
				ph7_constant *pCons = ReflectConstEntry(pCtx->pVm, zName, nName);
				PH7_MemObjRelease(p->pVal);
				if( pCons && pCons->xExpand ){
					/* Describing the table is not READING an entry: php's
					 * deprecated constants report when a program names one,
					 * and a listing is silent (get_defined_constants()'s
					 * own rule, and the same switch). */
					pCtx->pVm->bConstEnum++;
					pCons->xExpand(p->pVal, pCons->pUserData);
					pCtx->pVm->bConstEnum--;
				}
			}
			break;
		default:
			if( !ReflectExtMakeReflector(pCtx,
					p->iKind == PH7_EXT_KIND_CLASS ? "ReflectionClass" : "ReflectionFunction",
					zName, nName, p->pVal) ){
				SyBlobRelease(&sKey);
				return 0;
			}
			break;
	}
	ph7_array_add_strkey_elem(p->pList, zKey, p->pVal);
	SyBlobRelease(&sKey);
	return rc;
}
static int ReflectExtListing(ph7_context *pCtx, int iKind, int iShape)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ReflectExtList sWalk;
	const char *zName = "";
	int nName = 0, iExt;
	sWalk.pCtx = pCtx;
	sWalk.iKind = iKind;
	sWalk.iShape = iShape;
	sWalk.pList = ph7_context_new_array(pCtx);
	sWalk.pVal = ph7_context_new_scalar(pCtx);
	if( sWalk.pList == 0 || sWalk.pVal == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( pThis ){
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	}
	/* A `phl.stub_extensions` name has no id and synthesizes nothing, so its
	 * every listing is empty -- which is also what php answers for a module
	 * that registers none of that kind. */
	iExt = PH7_VmExtensionLookup(zName, nName);
	if( iExt >= 0 ){
		PH7_VmExtWalk(iExt, iKind, ReflectExtListStep, &sWalk);
	}
	ph7_result_value(pCtx, sWalk.pList);
	return PH7_OK;
}
#define REFLECT_EXT_LISTING(NAME,KIND,SHAPE) \
	static int NAME(ph7_context *pCtx, int nArg, ph7_value **apArg) \
	{ \
		SXUNUSED(nArg); \
		SXUNUSED(apArg); \
		return ReflectExtListing(pCtx, KIND, SHAPE); \
	}
REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getFunctions,
	PH7_EXT_KIND_FUNC,  REFLECT_EXT_MAP)
REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getClasses,
	PH7_EXT_KIND_CLASS, REFLECT_EXT_MAP)
REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getClassNames,
	PH7_EXT_KIND_CLASS, REFLECT_EXT_NAMES)
REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getConstants,
	PH7_EXT_KIND_CONST, REFLECT_EXT_VALUES)
REFLECT_EXT_LISTING(vm_builtin_ReflectionExtension_getINIEntries,
	PH7_EXT_KIND_INI,   REFLECT_EXT_VALUES)
static int ReflectExtDepStep(const char *zOn, const char *zKind, void *pData)
{
	ReflectExtList *p = (ReflectExtList *)pData;
	ph7_value_reset_string_cursor(p->pVal);
	ph7_value_string(p->pVal, zKind, -1);
	ph7_array_add_strkey_elem(p->pList, zOn, p->pVal);
	return 0;
}
static int vm_builtin_ReflectionExtension_getDependencies(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ReflectExtList sWalk;
	const char *zName = "";
	int nName = 0, iExt;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	sWalk.pList = ph7_context_new_array(pCtx);
	sWalk.pVal = ph7_context_new_scalar(pCtx);
	if( sWalk.pList == 0 || sWalk.pVal == 0 ){
		return PH7_ContextMemoryError(pCtx);
	}
	if( pThis ){
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	}
	iExt = PH7_VmExtensionLookup(zName, nName);
	if( iExt >= 0 ){
		sWalk.pCtx = pCtx;
		PH7_VmExtWalkDep(iExt, ReflectExtDepStep, &sWalk);
	}
	ph7_result_value(pCtx, sWalk.pList);
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
/*
 * ---------------------------------------------------------------------------
 * The EXTENSION block -- php's whole module, and the last export this engine
 * did not have (it printed one placeholder line, `Extension [ extension #1
 * name ]`). It NESTS the function and class blocks, which is why it waited for
 * them: its bytes cannot match php's until theirs do.
 *
 * php's shape, measured against 8.5.9:
 *
 *   Extension [ <persistent> extension #N name version V ] {
 *   <blank>
 *     - Dependencies { … }        each section only when it has rows,
 *   <blank>                       in this order, and each preceded by a
 *     - INI { … }                 blank line -- an extension with no rows
 *   <blank>                       at all prints `{` and `}` on consecutive
 *     - Constants [C] { … }       lines instead.
 *   <blank>
 *     - Functions { … }
 *   <blank>
 *     - Classes [K] { … }
 *   }
 *
 * A nested block is the STANDALONE export with four spaces on every non-empty
 * line -- verified byte for byte against php, which is what lets this reuse
 * the two block builders rather than threading an indent through them.
 * ---------------------------------------------------------------------------
 */
static void ReflectExportFuncByName(ph7_context *pCtx, SyBlob *pOut,
	const char *zName, int nName);
static sxi32 ReflectExportClassBlock(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass);
/* Copy pIn into pOut with nPad spaces in front of every non-empty line. */
static void ReflectExportPad(SyBlob *pOut, SyBlob *pIn, int nPad)
{
	const char *zIn = (const char *)SyBlobData(pIn);
	sxu32 nIn = SyBlobLength(pIn), i = 0;
	while( i < nIn ){
		sxu32 j = i;
		while( j < nIn && zIn[j] != '\n' ){
			j++;
		}
		if( j > i ){
			int k;
			for( k = 0 ; k < nPad ; k++ ){
				SyBlobAppend(pOut, " ", sizeof(char));
			}
			SyBlobAppend(pOut, &zIn[i], j - i);
		}
		if( j < nIn ){
			SyBlobAppend(pOut, "\n", sizeof(char));
		}
		i = j + 1;
	}
}
typedef struct ReflectExtDump ReflectExtDump;
struct ReflectExtDump {
	ph7_context *pCtx;
	SyBlob *pOut;     /* the section body, already indented */
	int iKind;
	sxu32 nRow;
};
/* php's access word for an ini directive: the whole mask is `ALL`, and
 * anything else is the set bits joined with a comma in php's own order. */
static void ReflectExtIniAccess(SyBlob *pOut, sxi32 iAccess)
{
	static const struct { sxi32 iBit; const char *zWord; } aBit[] = {
		{ 1, "USER" }, { 2, "PERDIR" }, { 4, "SYSTEM" }
	};
	sxu32 n;
	int bFirst = 1;
	if( (iAccess & 7) == 7 ){
		SyBlobAppend(pOut, "ALL", sizeof("ALL")-1);
		return;
	}
	for( n = 0 ; n < SX_ARRAYSIZE(aBit) ; ++n ){
		if( iAccess & aBit[n].iBit ){
			if( !bFirst ){
				SyBlobAppend(pOut, ",", sizeof(char));
			}
			SyBlobAppend(pOut, aBit[n].zWord, SyStrlen(aBit[n].zWord));
			bFirst = 0;
		}
	}
}
static int ReflectExtDumpStep(const char *zName, int nName, void *pData)
{
	ReflectExtDump *p = (ReflectExtDump *)pData;
	ph7_context *pCtx = p->pCtx;
	ph7_vm *pVm = pCtx->pVm;
	SyBlob sBlock;
	if( !PH7_VmInternalNameExists(pVm, p->iKind, zName, nName) ){
		return 0;
	}
	p->nRow++;
	if( p->iKind == PH7_EXT_KIND_INI ){
		SyBlob sVal, sDef;
		sxi32 iAccess = 0;
		SyBlobInit(&sVal, &pVm->sAllocator);
		SyBlobInit(&sDef, &pVm->sAllocator);
		if( PH7_VmIniDescribe(pVm, zName, (sxu32)nName, &iAccess, &sVal, &sDef) ){
			SyBlobAppend(p->pOut, "    Entry [ ", sizeof("    Entry [ ")-1);
			SyBlobAppend(p->pOut, zName, (sxu32)nName);
			SyBlobAppend(p->pOut, " <", sizeof(" <")-1);
			ReflectExtIniAccess(p->pOut, iAccess);
			SyBlobAppend(p->pOut, "> ]\n      Current = '", sizeof("> ]\n      Current = '")-1);
			SyBlobAppend(p->pOut, SyBlobData(&sVal), SyBlobLength(&sVal));
			SyBlobAppend(p->pOut, "'\n", sizeof("'\n")-1);
			/* php prints the DEFAULT only for a directive a script moved. */
			if( SyBlobLength(&sVal) != SyBlobLength(&sDef)
			 || SyMemcmp(SyBlobData(&sVal), SyBlobData(&sDef), SyBlobLength(&sVal)) != 0 ){
				SyBlobAppend(p->pOut, "      Default = '", sizeof("      Default = '")-1);
				SyBlobAppend(p->pOut, SyBlobData(&sDef), SyBlobLength(&sDef));
				SyBlobAppend(p->pOut, "'\n", sizeof("'\n")-1);
			}
			SyBlobAppend(p->pOut, "    }\n", sizeof("    }\n")-1);
		}
		SyBlobRelease(&sVal);
		SyBlobRelease(&sDef);
		return 0;
	}
	SyBlobInit(&sBlock, &pVm->sAllocator);
	if( p->iKind == PH7_EXT_KIND_CONST ){
		ph7_constant *pCons = ReflectConstEntry(pVm, zName, nName);
		if( pCons ){
			pVm->bConstEnum++;   /* describing the table is not READING an entry */
			ReflectExportGlobalConstLine(pCtx, &sBlock, pCons);
			pVm->bConstEnum--;
		}
	}else if( p->iKind == PH7_EXT_KIND_CLASS ){
		ph7_class *pClass = PH7_VmExtractClass(pVm, zName, (sxu32)nName, FALSE, 0);
		/* php separates one CLASS block from the next with a blank line, and
		 * does NOT do the same for functions or constants. */
		if( p->nRow > 1 ){
			SyBlobAppend(p->pOut, "\n", sizeof(char));
		}
		if( pClass ){
			ReflectExportClassBlock(pCtx, &sBlock, pClass);
		}
	}else{
		ReflectExportFuncByName(pCtx, &sBlock, zName, nName);
	}
	ReflectExportPad(p->pOut, &sBlock, 4);
	SyBlobRelease(&sBlock);
	return 0;
}
/* One `  - <title>[ [N]] { … }` section, written only when it has rows. */
static void ReflectExtSection(SyBlob *pOut, const char *zTitle, SyBlob *pBody,
	sxu32 nRow, int bCount)
{
	if( SyBlobLength(pBody) < 1 ){
		return;
	}
	SyBlobFormat(pOut, "\n  - %s ", zTitle);
	if( bCount ){
		SyBlobFormat(pOut, "[%u] ", nRow);
	}
	SyBlobAppend(pOut, "{\n", sizeof("{\n")-1);
	SyBlobAppend(pOut, SyBlobData(pBody), SyBlobLength(pBody));
	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);
}
static int ReflectExtDepLine(const char *zOn, const char *zKind, void *pData)
{
	ReflectExtDump *p = (ReflectExtDump *)pData;
	p->nRow++;
	SyBlobFormat(p->pOut, "    Dependency [ %s (%s) ]\n", zOn, zKind);
	return 0;
}
static void ReflectExtOneSection(ph7_context *pCtx, SyBlob *pOut, int iExt,
	int iKind, const char *zTitle, int bCount)
{
	ReflectExtDump sDump;
	SyBlob sBody;
	SyBlobInit(&sBody, &pCtx->pVm->sAllocator);
	sDump.pCtx = pCtx;
	sDump.pOut = &sBody;
	sDump.iKind = iKind;
	sDump.nRow = 0;
	PH7_VmExtWalk(iExt, iKind, ReflectExtDumpStep, &sDump);
	ReflectExtSection(pOut, zTitle, &sBody, sDump.nRow, bCount);
	SyBlobRelease(&sBody);
}
static int vm_builtin_ReflectionExtension_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_vm *pVm = pCtx->pVm;
	const char *zName = "";
	int nName = 0, iExt;
	SyBlob sOut;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pThis ){
		PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	}
	iExt = PH7_VmExtensionLookup(zName, nName);
	SyBlobInit(&sOut, &pVm->sAllocator);
	/* php's `#N` is the module's REGISTRATION index; a `phl.stub_extensions`
	 * name has no partition of its own and rides at the end of the table. */
	/* The version is the one getVersion() reports, which is php's answer for a
	 * BUNDLED module: the interpreter's own, not the extension's. */
	SyBlobFormat(&sOut, "Extension [ <persistent> extension #%d %.*s version %s ] {\n",
		iExt >= 0 ? iExt : PH7_VmExtensionCount(), nName, zName, PHP_COMPAT_VERSION);
	if( iExt >= 0 ){
		ReflectExtDump sDep;
		SyBlob sBody;
		SyBlobInit(&sBody, &pVm->sAllocator);
		sDep.pCtx = pCtx;
		sDep.pOut = &sBody;
		sDep.iKind = -1;
		sDep.nRow = 0;
		PH7_VmExtWalkDep(iExt, ReflectExtDepLine, &sDep);
		ReflectExtSection(&sOut, "Dependencies", &sBody, sDep.nRow, 0);
		SyBlobRelease(&sBody);
		ReflectExtOneSection(pCtx, &sOut, iExt, PH7_EXT_KIND_INI,   "INI",       0);
		ReflectExtOneSection(pCtx, &sOut, iExt, PH7_EXT_KIND_CONST, "Constants", 1);
		ReflectExtOneSection(pCtx, &sOut, iExt, PH7_EXT_KIND_FUNC,  "Functions", 0);
		ReflectExtOneSection(pCtx, &sOut, iExt, PH7_EXT_KIND_CLASS, "Classes",   1);
	}
	SyBlobAppend(&sOut, "}\n", sizeof("}\n")-1);
	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
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
	char zGiven[64];
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
		 * did. Recorded with the rest of that gap. */
		return PH7_VmThrowException(pCtx, "TypeError",
			"ReflectionReference::fromArrayElement(): Argument #1 ($array) "
			"must be of type array, %s given", VmValueGivenName(apArg[0],zGiven,sizeof(zGiven)));
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
 * Declare the six. Called from PH7_VmInstallReflection() where chunk 5 used to
 * be compiled, so Reflector (chunk 1) already exists.
 */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionSmall(ph7_vm *pVm)
{
	static const PH7_NativePropDef aGenProp[] = {
		{ RG_GEN, PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
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
		{ "getTrace",              PH7_MOD_PUBLIC,
		  "int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT", "array",
		  vm_builtin_ReflectionGenerator_trace },
	};
	static const PH7_NativePropDef aFiberProp[] = {
		{ RF_FIBER, PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aFiberMethod[] = {
		{ "__construct",      PH7_MOD_PUBLIC, "Fiber $fiber", "", vm_builtin_ReflectionFiber_construct },
		{ "getFiber",         PH7_MOD_PUBLIC, "", "Fiber",    vm_builtin_ReflectionFiber_getFiber },
		{ "getCallable",      PH7_MOD_PUBLIC, "", "callable", vm_builtin_ReflectionFiber_getCallable },
		{ "getExecutingLine", PH7_MOD_PUBLIC, "", "?int",     vm_builtin_ReflectionFiber_execLine },
		{ "getExecutingFile", PH7_MOD_PUBLIC, "", "?string",  vm_builtin_ReflectionFiber_execFile },
		{ "getTrace",         PH7_MOD_PUBLIC, "int $options = DEBUG_BACKTRACE_PROVIDE_OBJECT",
		  "array", vm_builtin_ReflectionFiber_trace },
	};
	static const PH7_NativePropDef aNameProp[] = {
		{ "name", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
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
		{ "getName",         PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionExtension_getName },
		{ "getVersion",      PH7_MOD_PUBLIC, "", "@?string", vm_builtin_ReflectionExtension_getVersion },
		{ "getFunctions",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getFunctions },
		{ "getConstants",    PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getConstants },
		{ "getINIEntries",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getINIEntries },
		{ "getClasses",      PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getClasses },
		{ "getClassNames",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getClassNames },
		{ "getDependencies", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionExtension_getDependencies },
		{ "info",            PH7_MOD_PUBLIC, "", "@void", vm_builtin_ReflectionExtension_info },
		{ "isPersistent",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionExtension_isPersistent },
		{ "isTemporary",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionExtension_isTemporary },
		{ "__toString",      PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionExtension_toString },
	};
	static const PH7_NativeMethodDef aZendMethod[] = {
		{ "__construct", PH7_MOD_PUBLIC, "string $name", "",
		  vm_builtin_ReflectionZendExtension_construct },
		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionExtension_getName },
		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionZendExtension_toString },
	};
	static const PH7_NativePropDef aRefProp[] = {
		{ RR_ID, PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_STRING, 0, "", 0.0 }, 0 },
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
		  aGenMethod, SX_ARRAYSIZE(aGenMethod), 0, 0, aGenProp, SX_ARRAYSIZE(aGenProp), 0, 0, 0 },
		{ "ReflectionFiber", 0, 0, PH7_CLASS_FINAL|PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aFiberMethod, SX_ARRAYSIZE(aFiberMethod), 0, 0, aFiberProp, SX_ARRAYSIZE(aFiberProp), 0, 0, 0 },
		{ "ReflectionConstant", 0, "Reflector", PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aConstMethod, SX_ARRAYSIZE(aConstMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },
		{ "ReflectionExtension", 0, "Reflector", PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aExtMethod, SX_ARRAYSIZE(aExtMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },
		{ "ReflectionZendExtension", 0, "Reflector", PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aZendMethod, SX_ARRAYSIZE(aZendMethod), 0, 0, aNameProp, SX_ARRAYSIZE(aNameProp), 0, 0, 0 },
		{ "ReflectionReference", 0, 0, PH7_CLASS_FINAL|PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aRefMethod, SX_ARRAYSIZE(aRefMethod), 0, 0, aRefProp, SX_ARRAYSIZE(aRefProp), 0, 0, 0 },
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
/*
 * zend's IMPLICIT abstract bit, which this engine did not carry: an INTERFACE
 * that declares a method is abstract, so `(new ReflectionClass('Countable'))
 * ->isAbstract()` is true where this said false for 24 of php's 25 interfaces
 * and for every userland one besides. The bit follows the METHODS rather than
 * the keyword, which is why `Traversable` -- an interface with nothing in it --
 * is php's one negative answer. Only this predicate reads it: getModifiers()
 * is 0 for an interface in php too, and the export writes `interface X`, never
 * `abstract interface X`.
 */
static int vm_builtin_ReflectionClass_isAbstract(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	int bAbstract = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( pClass ){
		bAbstract = (pClass->iFlags & PH7_CLASS_ABSTRACT) != 0
			|| ((pClass->iFlags & PH7_CLASS_INTERFACE) != 0
			 && SyHashTotalEntry(&pClass->hMethod) > 0);
	}
	ph7_result_bool(pCtx, bAbstract);
	return PH7_OK;
}
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
	/* An anonymous class is the only one whose name has two halves: php synthesizes
	 * `<prefix>@anonymous` + NUL + `file:line$hex`, and PH7_NewRawClass cuts sDisp at
	 * the NUL, so a shorter display name IS the marker. The old test read the name
	 * for a literal `class@anonymous` prefix, which php only ever writes when the
	 * class has no parent and no interface. */
	ph7_class *pClass = ReflectClassOf(pCtx);
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	ph7_result_bool(pCtx, pClass != 0 && pClass->sDisp.nByte != pClass->sName.nByte);
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
		PH7_ReflectInterfacesOf(pCtx->pVm, pClass, &aSet);
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
 * empty (recorded). */
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
	PH7_ReflectInterfacesOf(pCtx->pVm, pClass, &aSet);
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
			"%z is not an interface", &pTarget->sDisp);
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
	PH7_ReflectInterfacesOf(pCtx->pVm, pClass, &aSet);
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
	PH7_ReflectInterfacesOf(pCtx->pVm, pClass, &aSet);
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
	if( iClone != 0 ){
		/* php consults a declared __clone FIRST and answers its visibility --
		 * even when the clone_obj refusal would still answer `clone` itself, so
		 * a subclass of Exception that declares a public __clone reports TRUE
		 * and refuses anyway. php's own inconsistency, kept. */
		ph7_result_bool(pCtx, iClone == PH7_CLASS_PROT_PUBLIC);
		return PH7_OK;
	}
	/* No __clone anywhere: php's answer is whether the clone_obj handler
	 * exists. The refusal flag walks the base chain like the handler it
	 * models, so `class M extends IteratorIterator {}` reports false too. */
	ph7_result_bool(pCtx, !PH7_ClassIsUncloneable(pClass));
	return PH7_OK;
}
/*
 * php's own gate, raised before any object exists -- the one every C-side
 * instantiation asks, Reflection's newInstance() and PDO's FETCH_CLASS alike.
 */
PH7_PRIVATE sxi32 PH7_VmCheckInstantiable(ph7_context *pCtx, ph7_class *pClass)
{
	if( pClass->iFlags & PH7_CLASS_INTERFACE ){
		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate interface %z", &pClass->sDisp);
	}
	if( pClass->iFlags & PH7_CLASS_TRAIT ){
		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate trait %z", &pClass->sDisp);
	}
	if( pClass->iFlags & PH7_CLASS_ENUM ){
		/* php 8.1 names the enum rather than the FINAL class it also is. */
		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate enum %z", &pClass->sDisp);
	}
	if( pClass->iFlags & PH7_CLASS_ABSTRACT ){
		return PH7_VmThrowException(pCtx, "Error", "Cannot instantiate abstract class %z", &pClass->sDisp);
	}
	if( pClass->iFlags & PH7_CLASS_NOINSTANTIATE ){
		/* The `new` path's create_object refusal, which php raises here too —
		 * ReflectionClass::newInstance() on a Closure is the same Error, not a
		 * visibility one about its private constructor. */
		if( pClass->zNewRefusal ){
			return PH7_VmThrowException(pCtx,
				pClass->zNewRefusalClass ? pClass->zNewRefusalClass : "Error",
				"%s", pClass->zNewRefusal);
		}
		return PH7_VmThrowException(pCtx, "Error",
			"Instantiation of class %z is not allowed", &pClass->sDisp);
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
	rc = PH7_VmCheckInstantiable(pCtx, pClass);
	if( rc != PH7_OK ){
		return rc;
	}
	ReflectCtorCloneVis(pVm, pClass, &iCtorVis, &iCloneVis);
	if( iCtorVis != 0 && iCtorVis != PH7_CLASS_PROT_PUBLIC ){
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Access to non-public constructor of class %z", &pClass->sDisp);
	}
	if( iCtorVis == 0 && nCtor > 0 ){
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Class %z does not have a constructor, so you cannot pass any constructor arguments",
			&pClass->sDisp);
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
		/* Weak binding, like ReflectMethodInvoke: the frame that makes this call is
		 * ReflectionClass::newInstance(), an internal function. */
		pVm->bCallbackWeak = 1;
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
		pVm->bCallbackWeak = 0;
		if( rc == PH7_EXCEPTION || rc == PH7_ABORT ){
			PH7_ClassInstanceCtorFailed(pObj);
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
	rc = PH7_VmCheckInstantiable(pCtx, pClass);
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
/*
 * Is this property protected(set) as far as php's Reflection is concerned?
 *
 * The bit is either DECLARED or implied by `readonly` — but readonly implies it
 * only for a property whose read side is PUBLIC. A `protected readonly` or
 * `private readonly` one already writes no wider than it reads, so php adds
 * nothing (`private readonly int $v` is modifiers 132, not 2180), and an explicit
 * `private(set)` beside readonly is the set visibility, so readonly adds nothing
 * there either (`public private(set) readonly` is 4257).
 */
static int ReflectPropProtectedSet(ph7_class_attr *pAttr)
{
	if( pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET ){
		return 1;
	}
	return (pAttr->iFlags & PH7_CLASS_ATTR_READONLY)
	    && (pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET) == 0
	    && pAttr->iProtection == PH7_CLASS_PROT_PUBLIC;
}
static sxi64 ReflectPropModifiers(ph7_class_attr *pAttr)
{
	sxi64 iMods = ReflectVisMask(pAttr->iProtection);
	if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){ iMods |= 16; }
	/* php's IS_VIRTUAL is "there is no slot behind this name", and it has two
	 * sources: a HOOKED property with no backing store, and a NATIVE class's
	 * property that php fabricates from its own C struct (DatePeriod's, and
	 * BcMath\Number's value/scale). Both report virtual there. */
	if( pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_VIRTUAL|PH7_CLASS_ATTR_NATIVE_VIRTUAL) ){
		iMods |= 512;
	}
	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){ iMods |= 128; }
	if( ReflectPropProtectedSet(pAttr) ){ iMods |= 2048; }
	/* PHP 8.4's `final` PROPERTY -- IS_FINAL, the same bit a method and a class
	 * constant carry. */
	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){ iMods |= 32; }
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
	*ppOut = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);
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
	/* A FACTORY build, not a user constructor call — see ph7_vm::nReflectFactory. */
	pCtx->pVm->nReflectFactory++;
	pOut = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);
	pCtx->pVm->nReflectFactory--;
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
/*
 * A `ReflectionMethod($this->..., ...)`-shaped first argument.
 *
 * The OBJECT when this reflector was built over one (a ReflectionObject), its
 * class NAME otherwise. For every ordinary member the two are interchangeable --
 * the constructor resolves an object to its class either way -- but not for a
 * FABRICATED method: `(new ReflectionObject($c))->getMethod('__invoke')` describes
 * THE CLOSURE's parameters where `(new ReflectionClass('Closure'))
 * ->getMethod('__invoke')` has nothing to describe, and php draws that line at
 * exactly this question. Takes a reference, so the caller's release balances.
 */
static void ReflectSelfName(ph7_context *pCtx, ph7_value *pOut)
{
	ph7_class_instance *pObj = ReflectClassObj(pCtx);
	const char *zName;
	int nName;
	PH7_MemObjInit(pCtx->pVm, pOut);
	if( pObj ){
		pObj->iRef++;
		pOut->x.pOther = pObj;
		pOut->iFlags = MEMOBJ_OBJ;
		return;
	}
	ReflectClassName(pCtx, &zName, &nName);
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
			"Method %z::%.*s() does not exist", &pClass->sDisp, nName, zName);
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
		if( iKind == REFLECT_MEMBER_PROP && pObj != 0 && pM->pAttr != 0
		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT) == 0
		 && PH7_ATTR_LAZY_ABSENT(pM->pAttr,pObj) ){
			/* A ReflectionObject reflects the OBJECT, and a LAZY property php does
			 * not DECLARE (DateInterval's ten) is not on one that was never
			 * constructed -- php reports none there either. The declared-and-virtual
			 * kind (DatePeriod's seven, PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT) is
			 * reported whatever the object holds, because php declares it. */
			continue;
		}
		if( iKind == REFLECT_MEMBER_PROP && pObj == 0 && pM->pAttr != 0
		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_ONDEMAND) != 0 ){
			/* An ON-DEMAND property belongs to the objects that took it, so the
			 * CLASS's list -- which php builds from declarations and DateInterval
			 * has none of -- does not gain a name for it. */
			continue;
		}
		if( iKind == REFLECT_MEMBER_PROP && pObj != 0 && pM->pAttr != 0
		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY) != 0
		 && (pM->pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_LAZY_DEFAULT) == 0 ){
			/* ...and for the same reason a property the object HIDES or never took
			 * is not on it either: php's from-string DateInterval reflects the two
			 * names it presents, and an ordinary one has no `date_string` at all. */
			SyHashEntry *pOwn = SyHashGet(&pObj->hAttr,
				SyStringData(&pM->pAttr->sName),SyStringLength(&pM->pAttr->sName));
			if( pOwn == 0
			 || (((VmClassAttr *)pOwn->pUserData)->iState & VM_CLASS_ATTR_UNSEEN) ){
				continue;
			}
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
		pCtx->pVm->nReflectFactory++;   /* see ph7_vm::nReflectFactory */
		pRef = ReflectConstruct(pCtx, zClass, 2, apCtor, &rc);
		pCtx->pVm->nReflectFactory--;
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
		"Property %z::$%.*s does not exist", &pClass->sDisp, nName, zName);
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
		pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pM->pAttr->nIdx);
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
			"Property %z::$%.*s does not exist", &pClass->sDisp, nName, zName);
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
			ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);
			return PH7_VmThrowException(pCtx, "Error",
				"Typed property %z::$%z must not be accessed before initialization",
				&pDecl->sDisp, &pAttr->sName);
		}
		pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);
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
			"Class %z does not have a property named %.*s", &pClass->sDisp, nName, zName);
	}
	pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);
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
/*
 * The ReflectionExtension for one extension id, which is what every reflector's
 * getExtension() answers for an INTERNAL target. A target the partition has no
 * row for (iExt < 0) belongs to no extension, which is php's null.
 */
static int ReflectExtensionOf(ph7_context *pCtx, int iExt)
{
	ph7_value sName;
	ph7_value *apCtor[1];
	ph7_class_instance *pExt;
	const char *zName;
	sxi32 rc;
	if( iExt < 0 ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	zName = PH7_VmExtensionName(iExt);
	PH7_MemObjInit(pCtx->pVm, &sName);
	ph7_value_string(&sName, zName, -1);
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
 * The extension a reflected CLASS belongs to, or -1 for a userland one. A class
 * php has no row for keeps php's answer for an internal name it cannot place --
 * Core, the engine's own.
 */
static int ReflectClassExtId(ph7_class *pClass)
{
	if( pClass == 0 || (pClass->iFlags & PH7_CLASS_INTERNAL) == 0 ){
		return -1;
	}
	return PH7_VmExtOfClass(SyStringData(&pClass->sName),
		(int)SyStringLength(&pClass->sName));
}
static int vm_builtin_ReflectionClass_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	int iExt = ReflectClassExtId(ReflectClassOf(pCtx));
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	if( iExt < 0 ){
		ph7_result_bool(pCtx, 0);
	}else{
		ph7_result_string(pCtx, PH7_VmExtensionName(iExt), -1);
	}
	return PH7_OK;
}
/* php's export format (chunk 9) — defined at the END of this file, where the
 * member walk, the function reference and the parameter description it reads
 * are all in scope. */
static int ReflectExportClassSelf(ph7_context *pCtx);
static int ReflectExportFuncSelf(ph7_context *pCtx);
static int ReflectExportParamSelf(ph7_context *pCtx);
static int ReflectExportPropSelf(ph7_context *pCtx);
static int ReflectExportConstSelf(ph7_context *pCtx);
/*
 * __toString(): php's export format, still chunk 9 — a PHP function written
 * against the PUBLIC reflection API of its target, so it is called with `$this`.
 * bIndentArg adds the export family's second "" indent argument.
 */
static int vm_builtin_ReflectionClass_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return ReflectExtensionOf(pCtx, ReflectClassExtId(ReflectClassOf(pCtx)));
}
static int vm_builtin_ReflectionClass_toString(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return ReflectExportClassSelf(pCtx);
}
/* ---- lazy objects: PHL has none (recorded) ---- */
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
 * Declare chunk 1. Called from PH7_VmInstallReflection() where it used to be
 * compiled — before chunks 2 and 3, which name `Reflector` in their own
 * `implements` clauses.
 *
 * The method table is in php's own DECLARATION order, which is the order
 * ReflectionClass::getMethods() reports for these classes themselves.
 */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionClass(ph7_vm *pVm)
{
	static const PH7_NativePropDef aClassProp[] = {
		{ "name",  PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
		/* PHL-only: the instance a ReflectionObject was built over. php keeps it
		 * out of sight; PHL has no hidden-slot bit yet (recorded). */
		{ RC_OBJ,  PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aClassMethod[] = {
		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionClass_clone },
		{ "__construct", PH7_MOD_PUBLIC, "object|string $objectOrClass", "",
		  vm_builtin_ReflectionClass_construct },
		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionClass_toString },
		{ "getName",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getName },
		{ "isInternal",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInternal },
		{ "isUserDefined", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isUserDefined },
		{ "isAnonymous",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isAnonymous },
		{ "isInstantiable",PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInstantiable },
		{ "isCloneable",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isCloneable },
		{ "getFileName",   PH7_MOD_PUBLIC, "", "@string|false", vm_builtin_ReflectionClass_getFileName },
		{ "getStartLine",  PH7_MOD_PUBLIC, "", "@int|false", vm_builtin_ReflectionClass_getStartLine },
		{ "getEndLine",    PH7_MOD_PUBLIC, "", "@int|false", vm_builtin_ReflectionClass_getEndLine },
		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string|false", vm_builtin_ReflectionClass_getDocComment },
		{ "getConstructor",PH7_MOD_PUBLIC, "", "@?ReflectionMethod", vm_builtin_ReflectionClass_getConstructor },
		{ "hasMethod",     PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasMethod },
		{ "getMethod",     PH7_MOD_PUBLIC, "string $name", "@ReflectionMethod", vm_builtin_ReflectionClass_getMethod },
		{ "getMethods",    PH7_MOD_PUBLIC, "?int $filter = null", "@array",
		  vm_builtin_ReflectionClass_getMethods },
		{ "hasProperty",   PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasProperty },
		{ "getProperty",   PH7_MOD_PUBLIC, "string $name", "@ReflectionProperty", vm_builtin_ReflectionClass_getProperty },
		{ "getProperties", PH7_MOD_PUBLIC, "?int $filter = null", "@array",
		  vm_builtin_ReflectionClass_getProperties },
		{ "hasConstant",   PH7_MOD_PUBLIC, "string $name", "@bool", vm_builtin_ReflectionClass_hasConstant },
		{ "getConstants",  PH7_MOD_PUBLIC, "?int $filter = null", "@array",
		  vm_builtin_ReflectionClass_getConstants },
		{ "getReflectionConstants", PH7_MOD_PUBLIC, "?int $filter = null", "@array",
		  vm_builtin_ReflectionClass_getReflectionConstants },
		{ "getConstant",   PH7_MOD_PUBLIC, "string $name", "@mixed", vm_builtin_ReflectionClass_getConstant },
		{ "getReflectionConstant", PH7_MOD_PUBLIC, "string $name", "@ReflectionClassConstant|false",
		  vm_builtin_ReflectionClass_getReflectionConstant },
		{ "getInterfaces",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getInterfaces },
		{ "getInterfaceNames", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getInterfaceNames },
		{ "isInterface",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isInterface },
		{ "getTraits",         PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraits },
		{ "getTraitNames",     PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraitNames },
		{ "getTraitAliases",   PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionClass_getTraitAliases },
		{ "isTrait",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isTrait },
		{ "isEnum",            PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClass_isEnum },
		{ "isAbstract",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isAbstract },
		{ "isFinal",           PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isFinal },
		{ "isReadOnly",        PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClass_isReadOnly },
		{ "getModifiers",      PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionClass_getModifiers },
		{ "isInstance",        PH7_MOD_PUBLIC, "object $object", "@bool",
		  vm_builtin_ReflectionClass_isInstance },
		{ "newInstance",       PH7_MOD_PUBLIC, "mixed ...$args", "@object",
		  vm_builtin_ReflectionClass_newInstance },
		{ "newInstanceWithoutConstructor", PH7_MOD_PUBLIC, "", "@object",
		  vm_builtin_ReflectionClass_newInstanceWithoutConstructor },
		{ "newInstanceArgs",   PH7_MOD_PUBLIC, "array $args = []", "@?object",
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
		{ "getParentClass",    PH7_MOD_PUBLIC, "", "@ReflectionClass|false", vm_builtin_ReflectionClass_getParentClass },
		{ "isSubclassOf",      PH7_MOD_PUBLIC, "ReflectionClass|string $class", "@bool",
		  vm_builtin_ReflectionClass_isSubclassOf },
		{ "getStaticProperties", PH7_MOD_PUBLIC, "", "@array",
		  vm_builtin_ReflectionClass_getStaticProperties },
		/* `mixed $default = ?` is the table's "optional, no default VALUE"
		 * marker — php's own shape here: isOptional() true,
		 * isDefaultValueAvailable() false. */
		{ "getStaticPropertyValue", PH7_MOD_PUBLIC, "string $name, mixed $default = ?", "@mixed",
		  vm_builtin_ReflectionClass_getStaticPropertyValue },
		{ "setStaticPropertyValue", PH7_MOD_PUBLIC, "string $name, mixed $value", "@void",
		  vm_builtin_ReflectionClass_setStaticPropertyValue },
		{ "getDefaultProperties", PH7_MOD_PUBLIC, "", "@array",
		  vm_builtin_ReflectionClass_getDefaultProperties },
		{ "isIterable",        PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isIterable },
		{ "isIterateable",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_isIterable },
		{ "implementsInterface", PH7_MOD_PUBLIC, "ReflectionClass|string $interface", "@bool",
		  vm_builtin_ReflectionClass_implementsInterface },
		{ "getExtension",      PH7_MOD_PUBLIC, "", "@?ReflectionExtension", vm_builtin_ReflectionClass_getExtension },
		{ "getExtensionName",  PH7_MOD_PUBLIC, "", "@string|false", vm_builtin_ReflectionClass_getExtensionName },
		{ "inNamespace",       PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClass_inNamespace },
		{ "getNamespaceName",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getNamespaceName },
		{ "getShortName",      PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionClass_getShortName },
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
		{ "getModifierNames", PH7_MOD_PUBLIC|PH7_MOD_STATIC, "int $modifiers", "@array",
		  vm_builtin_Reflection_getModifierNames },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "Reflector", "Stringable", 0, PH7_CLASS_INTERFACE, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "ReflectionException", "Exception", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
		{ "Reflection", 0, 0, 0,
		  aReflectionMethod, SX_ARRAYSIZE(aReflectionMethod), 0, 0, 0, 0, 0, 0, 0 },
		/* Uncloneable and unserializable in php too: `clone` is an Error and
		 * serialize() a catchable Exception naming the class. */
		{ "ReflectionClass", 0, "Reflector", PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aClassMethod, SX_ARRAYSIZE(aClassMethod),
		  aClassConst, SX_ARRAYSIZE(aClassConst),
		  aClassProp, SX_ARRAYSIZE(aClassProp), 0, 0, 0 },
		{ "ReflectionObject", "ReflectionClass", 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aObjectMethod, SX_ARRAYSIZE(aObjectMethod), 0, 0, 0, 0, 0, 0, 0 },
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
/*
 * ReflectionMethod: the class the reflector was BUILT FOR, which is not the
 * one `$class` reports. php keeps both — `intern->ce` is the class the
 * constructor (or the ReflectionClass that handed the method out) named, while
 * the public `$class` is the method's DECLARING class — and its export tags are
 * relative to the first: a method whose declaring class is not the reflector's
 * own class prints `inherits <declaring>`. PHL had only the declaring one, so
 * every directly-built reflector lost the tag.
 */
#define RM_CE "__ce"
#define RP_T  "__t"      /* ReflectionParameter: the function/class it belongs to */
#define RP_M  "__m"      /* ReflectionParameter: the method name, or null      */
#define RP_P  "__p"      /* ReflectionParameter: the position                  */

/* Everything a reflected function IS, resolved once per call. */
typedef struct ReflectFuncRef ReflectFuncRef;
struct ReflectFuncRef
{
	ph7_vm *pVm;                  /* the VM, for the declared-type text rewrite */
	ph7_vm_func *pFunc;           /* compiled body (NULL for a pure C builtin) */
	ph7_user_func *pHost;         /* C builtin (NULL otherwise) */
	ph7_class *pClass;            /* class a METHOD was reached through */
	ph7_class_method *pMeth;      /* the method */
	ph7_class_instance *pClosure; /* the Closure being reflected, if any */
	const char *zSig;             /* declared parameter signature, or NULL */
	const char *zRet;             /* declared return type, or NULL */
	int bFabricated;              /* `Closure::__invoke`: pFunc is the CLOSURE's, borrowed for its
	                               * parameter list alone. Everything that describes where the
	                               * function came FROM -- internal or user, file, lines, module --
	                               * belongs to the fabricated method instead, which php builds as
	                               * an internal one with no module at all. */
};
/*
 * Resolve a callable into the reference, and work out WHICH of the two
 * parameter sources describes it: a declared signature string (a C builtin, a
 * native method, or an embedded-PHP builtin declared argless over
 * func_get_args()) wins over the compiled argument list, exactly as
 * ReflectSigFixup made it win in the descriptor.
 */
static int ReflectFuncFill(ph7_vm *pVm, ph7_value *pTarget, ph7_value *pMethodArg,
	ReflectFuncRef *pOut)
{
	SyZero(pOut, sizeof(*pOut));
	pOut->pVm = pVm;
	pOut->pFunc = ReflectResolveCallable(pVm, pTarget, pMethodArg,
		&pOut->pClass, &pOut->pMeth, &pOut->pHost, &pOut->pClosure);
	if( pOut->pFunc == 0 && pOut->pHost == 0 ){
		return 0;
	}
	if( pOut->pFunc == 0 ){
		pOut->zSig = pOut->pHost->zSig;
		pOut->zRet = pOut->pHost->zRet;
		return 1;
	}
	if( pOut->pMeth && (pOut->pMeth->iFlags & PH7_CLASS_ATTR_FABRICATED) ){
		/* The split above: with a closure, pFunc is the CLOSURE's, so nothing of the
		 * method's own (empty) declaration may describe it -- and where it CAME from
		 * is still the method's, which bFabricated is what says. WITHOUT one (the
		 * class-level form) there is nothing to describe at all, which php prints as
		 * a block with no parameter section rather than an empty one. */
		pOut->bFabricated = 1;
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
	int nName, nClass, rc, bMethod;
	SyZero(pOut, sizeof(*pOut));
	if( pThis == 0 ){
		return 0;
	}
	PH7_MemObjInit(pCtx->pVm, &sTarget);
	PH7_MemObjInit(pCtx->pVm, &sMethod);
	pClo = PH7_NativeAttrObj(pThis, RF_CL);
	PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	bMethod = ReflectIsMethodReflector(pCtx, pThis);
	if( pClo && bMethod ){
		/* The fabricated `Closure::__invoke`: name it over the OBJECT, which is what
		 * makes the resolver hand back the method's identity and the closure's body
		 * together (see ReflectResolveCallable). */
		sTarget.x.pOther = pClo;
		sTarget.iFlags = MEMOBJ_OBJ;
		ph7_value_string(&sMethod, zName, nName);
	}else if( pClo ){
		sTarget.x.pOther = pClo;
		sTarget.iFlags = MEMOBJ_OBJ;
	}else if( bMethod ){
		PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);
		ph7_value_string(&sTarget, zClass, nClass);
		ph7_value_string(&sMethod, zName, nName);
	}else{
		ph7_value_string(&sTarget, zName, nName);
	}
	rc = ReflectFuncFill(pCtx->pVm, &sTarget, (sMethod.iFlags & MEMOBJ_STRING) ? &sMethod : 0, pOut);
	/* sTarget only ever BORROWS the closure; releasing it would unref twice. */
	if( (sTarget.iFlags & MEMOBJ_OBJ) == 0 ){
		PH7_MemObjRelease(&sTarget);
	}
	PH7_MemObjRelease(&sMethod);
	return rc;
}
/*
 * The class `$this` was BUILT FOR (php's `intern->ce`), or NULL — a
 * ReflectionFunction has none, and so does a reflector whose class went away.
 */
static ph7_class * ReflectOwnerOfThis(ph7_context *pCtx)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	const char *zName = 0;
	int nName = 0;
	if( pThis == 0 ){
		return 0;
	}
	PH7_NativeAttrStr(pThis, RM_CE, &zName, &nName);
	if( zName == 0 || nName < 1 ){
		return 0;
	}
	return PH7_VmExtractClass(pCtx->pVm, zName, (sxu32)nName, FALSE, 0);
}
/* How many parameters the target declares. */
static int ReflectParamCount(const ReflectFuncRef *pRef)
{
	if( pRef->zSig ){
		return ReflectSigPart(pRef->zSig, (int)SyStrlen(pRef->zSig), -1, 0, 0);
	}
	return pRef->pFunc ? (int)SySetUsed(&pRef->pFunc->aArgs) : 0;
}
/*
 * The class a `self`/`parent` in this function's declared types resolves to for
 * DISPLAY: php substitutes the declaring class into the stored text at compile
 * time, and has nothing to substitute for a TRAIT method (whose text keeps the
 * keyword however many classes composed it) or for a plain function.
 * `static` and `iterable` are printed as written -- PH7_HINT_TEXT_* off.
 */
static ph7_class * ReflectDeclScope(const ReflectFuncRef *pRef)
{
	if( pRef->pFunc == 0 || (pRef->pFunc->iFlags & VM_FUNC_CLASS_METHOD) == 0 ){
		return 0; /* pUserData is a class only for a METHOD */
	}
	return VmHintScopeDeclared((ph7_class *)pRef->pFunc->pUserData);
}
/*
 * A MEMBER's declared type as php prints it: the same rewrite ReflectDeclScope
 * describes, keyed on the class that declared the member rather than the one it
 * was reached through -- so a trait's `?self` stays `?self`.
 */
static const char * ReflectMemberTypeText(ph7_vm *pVm,ph7_class_attr *pAttr,char *zBuf,sxu32 nBuf)
{
	return VmHintTextResolvedEx(pVm,&pAttr->sTypeName,
		VmHintScopeDeclared(pAttr->pDeclClass),0,zBuf,nBuf);
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
		if( pRef->bFabricated ){
			/* php copies the closure's parameter list into an INTERNAL record, and a
			 * default VALUE does not survive that copy: every optional parameter of a
			 * fabricated `Closure::__invoke` answers isDefaultValueAvailable() false
			 * and exports as `= <default>`, while staying optional. */
			pOut->bHasDef = 0;
		}
		{
			const char *zT = VmHintTextResolvedEx(pRef->pVm,&pArg->sTypeName,
				ReflectDeclScope(pRef),0,pOut->zTypeBuf,sizeof(pOut->zTypeBuf));
			SyStringInitFromBuf(&pOut->sType,zT,(sxu32)SyStrlen(zT));
		}
		pOut->pArg = pArg;
		pOut->bInternal = pRef->bFabricated
			|| (pRef->pFunc->iFlags & VM_FUNC_INTERNAL) != 0;
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
/*
 * The declared return type TEXT, or 0 — and whether php calls it TENTATIVE.
 *
 * php's stubs carry two kinds of internal return type and Reflection answers
 * differently for each: a real one is reported by getReturnType()/hasReturnType()
 * and printed `Return [ T ]`, while an `@tentative-return-type` answers NULL and
 * false from those two, is reported by getTentativeReturnType()/
 * hasTentativeReturnType(), and prints `Tentative return [ T ]`. Nearly every
 * internal SPL, date and Reflection method is the tentative kind.
 *
 * PHL has one field for both, so a leading `@` on a `zRet` — the same character
 * php's stub annotation uses — marks the tentative kind. It is stripped here, so
 * nothing downstream of this function ever sees it.
 */
static int ReflectFuncRetTextEx(const ReflectFuncRef *pRef, const char **pz, int *pn,
	int *pbTentative, char *zBuf, sxu32 nBuf)
{
	if( pbTentative ){
		*pbTentative = 0;
	}
	if( pRef->zRet && pRef->zRet[0] ){
		const char *z = pRef->zRet;
		if( z[0] == '@' ){
			if( pbTentative ){
				*pbTentative = 1;
			}
			z++;
		}
		*pz = z;
		*pn = (int)SyStrlen(z);
		return 1;
	}
	if( pRef->pFunc == 0 ){
		return 0;
	}
	if( SyStringLength(&pRef->pFunc->sReturnTypeName) > 0 ){
		*pz = VmHintTextResolvedEx(pRef->pVm,&pRef->pFunc->sReturnTypeName,
			ReflectDeclScope(pRef),0,zBuf,nBuf);
		*pn = (int)SyStrlen(*pz);
		return 1;
	}
	return 0;
}
/* The declared return type php would report from getReturnType(): a TENTATIVE one
 * is not reported there at all, which is the whole distinction. */
static int ReflectFuncRetText(const ReflectFuncRef *pRef, const char **pz, int *pn,
	char *zBuf, sxu32 nBuf)
{
	int bTentative = 0;
	if( !ReflectFuncRetTextEx(pRef, pz, pn, &bTentative, zBuf, nBuf) ){
		return 0;
	}
	return bTentative ? 0 : 1;
}
/* Is the reflected function internal (a C builtin or an embedded-chunk one)? */
static int ReflectFuncIsInternal(const ReflectFuncRef *pRef)
{
	if( pRef->pHost || pRef->bFabricated ){
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
/*
 * The same, for the three getClosure* accessors: what they read is captured on
 * the Closure OBJECT, not in a function record, so a reflector over a closure
 * nothing resolves (php's magic-method trampoline) must still answer from the
 * Closure `$this` is holding. Without this they fell out at the resolve and
 * reported no scope and no bound object for a callable php describes fully.
 */
#define REFLECT_CLOSURE_OR(REF,STMT) \
	if( !ReflectFuncOfThis(pCtx, &(REF)) ){ \
		ph7_class_instance *_pRcThis = PH7_ContextThis(pCtx); \
		SyZero(&(REF), sizeof(REF)); \
		(REF).pVm = pCtx->pVm; \
		(REF).pClosure = _pRcThis ? PH7_NativeAttrObj(_pRcThis, RF_CL) : 0; \
		if( (REF).pClosure == 0 ){ STMT; return PH7_OK; } \
	}

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
/*
 * php's deprecation for an INTERNAL name, or NULL. Userland's is the
 * #[\Deprecated] attribute; this is the engine's own table, stamped on the C
 * body a builtin and a native method share (aDeprecatedFunc[]).
 */
static const ph7_deprecated_name * ReflectFuncDeprecated(const ReflectFuncRef *pRef)
{
	if( pRef->pHost ){
		return pRef->pHost->pDeprecated;
	}
	if( pRef->pFunc && (pRef->pFunc->iFlags & VM_FUNC_NATIVE) && pRef->pFunc->pNative ){
		return pRef->pFunc->pNative->pDeprecated;
	}
	return 0;
}
static int vm_builtin_ReflectionFunc_isDeprecated(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))
	ph7_result_bool(pCtx, ReflectFuncDeprecated(&sRef) != 0
		|| (sRef.pFunc != 0 && ReflectHasDeprecated(&sRef.pFunc->aAttrs)));
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
	REFLECT_CLOSURE_OR(sRef, ph7_result_null(pCtx))
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
	REFLECT_CLOSURE_OR(sRef, ph7_result_null(pCtx))
	pAttr = ReflectClosureAttr(&sRef, "__scope");
	if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){
		/* php reports the class that DECLARED the callee, not the one the callable NAMED:
		 * `$__scope` is the CALLED scope (what `static::` answers, reported by
		 * getClosureCalledClass below), and for an inherited or trait-composed method the
		 * two differ. PH7_VmClosureScopeClass is the one rule. */
		return ReflectResultClassOf(pCtx,
			PH7_VmClosureScopeClass(pCtx->pVm, sRef.pClosure));
	}
	pAttr = ReflectClosureAttr(&sRef, "__this");
	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){
		return ReflectResultClassOf(pCtx, ((ph7_class_instance *)pAttr->x.pOther)->pClass);
	}
	ph7_result_null(pCtx);
	return PH7_OK;
}
/*
 * ReflectionFunction::getClosureCalledClass() — php's CALLED scope, the other half of the
 * pair above: the class the call goes THROUGH, which is what `static::` answers. A bound
 * closure's is its `$this`'s class; a static callable's is the class the callable NAMED. It
 * shared getClosureScopeClass()'s body while `$__scope` answered both questions; it cannot
 * now, because that one narrows a method closure to its DECLARING class.
 */
static int vm_builtin_ReflectionFunc_getClosureCalledClass(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	ph7_value *pAttr;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_CLOSURE_OR(sRef, ph7_result_null(pCtx))
	pAttr = ReflectClosureAttr(&sRef, "__this");
	if( pAttr && (pAttr->iFlags & MEMOBJ_OBJ) ){
		return ReflectResultClassOf(pCtx, ((ph7_class_instance *)pAttr->x.pOther)->pClass);
	}
	pAttr = ReflectClosureAttr(&sRef, "__scope");
	if( pAttr && (pAttr->iFlags & MEMOBJ_STRING) && SyBlobLength(&pAttr->sBlob) > 0 ){
		return ReflectResultClassOf(pCtx, PH7_VmExtractClass(pCtx->pVm,
			(const char *)SyBlobData(&pAttr->sBlob), SyBlobLength(&pAttr->sBlob), FALSE, 0));
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
			ph7_value *pLive = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, aEnv[n].nIdx);
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
	if( sRef.pFunc && !sRef.bFabricated && SyStringLength(&sRef.pFunc->sFile) > 0 ){
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
	char zRetBuf[192];
	int n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))
	ph7_result_bool(pCtx, ReflectFuncRetText(&sRef, &z, &n, zRetBuf, sizeof(zRetBuf)));
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_getReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	const char *z;
	char zRetBuf[192];
	int n;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))
	if( !ReflectFuncRetText(&sRef, &z, &n, zRetBuf, sizeof(zRetBuf)) ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultObject(pCtx, ReflectMakeType(pCtx, z, n));
}
/* A native method's `@`-marked zRet is php's `@tentative-return-type`: reported
 * HERE and by nothing else, which is what separates it from a real one. */
static int vm_builtin_ReflectionFunc_hasTentativeReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	const char *z;
	char zRetBuf[192];
	int n, bTentative = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))
	ph7_result_bool(pCtx, ReflectFuncRetTextEx(&sRef, &z, &n, &bTentative, zRetBuf, sizeof(zRetBuf)) && bTentative);
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_getTentativeReturnType(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	const char *z;
	char zRetBuf[192];
	int n, bTentative = 0;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))
	if( !ReflectFuncRetTextEx(&sRef, &z, &n, &bTentative, zRetBuf, sizeof(zRetBuf)) || !bTentative ){
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	return ReflectResultObject(pCtx, ReflectMakeType(pCtx, z, n));
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
	if( pClo && !ReflectIsMethodReflector(pCtx, pThis) ){
		pSpec->x.pOther = pClo;
		pSpec->iFlags = MEMOBJ_OBJ;
		return;
	}
	PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	if( ReflectIsMethodReflector(pCtx, pThis) ){
		/* `[class, method]`, which ReflectionParameter's constructor accepts -- and
		 * for the fabricated `Closure::__invoke` the OBJECT stands in for the class,
		 * because that pair is what resolves to the closure's parameter list while
		 * still naming a method (php's parameter reports `Closure` as its declaring
		 * class and `__invoke` as its declaring function). */
		ph7_value *pList = ph7_context_new_array(pCtx);
		ph7_value *pA = ph7_context_new_scalar(pCtx);
		ph7_value *pB = ph7_context_new_scalar(pCtx);
		if( pList == 0 || pA == 0 || pB == 0 ){
			return;
		}
		if( pClo ){
			/* pA is a CONTEXT value: the call's teardown releases it, so stamping an
			 * object into it has to take a reference the way any other holder does.
			 * (Without it the closure was unref'd once per getParameters() call and
			 * freed under its own Closure object -- a use-after-free that surfaced
			 * three hundred files into a phpstan run.) */
			pClo->iRef++;
			pA->x.pOther = pClo;
			pA->iFlags = MEMOBJ_OBJ;
		}else{
			PH7_NativeAttrStr(pThis, "class", &zClass, &nClass);
			ph7_value_string(pA, zClass, nClass);
		}
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
	/* php reports a closure's captured `use` variables here TOO, ahead of the body's
	 * own statics: it compiles both into one static-variables table, and
	 * ReflectionFunction::getStaticVariables() hands back the whole thing. PHL listed
	 * only the `static $x` ones, so a closure's captures were invisible to reflection.
	 *
	 * Pest reads exactly this to find the closure a test case wraps --
	 * `Reflection::getFunctionVariable($this->__test, 'closure')` is
	 * `(new ReflectionFunction($f))->getStaticVariables()['closure']` -- and with the
	 * captures missing it got null and EVERY test in a Pest suite failed on the
	 * TypeError that followed.
	 *
	 * `$this` is not one of them: php keeps the receiver in its own slot and never
	 * lists it as a static, while PHL carries it as an ordinary env entry. A by-REFERENCE
	 * capture reports what its slot holds NOW, not the value at closure creation. */
	{
		ph7_vm_func_closure_env *aEnv =
			(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);
		sxu32 k;
		for( k = 0 ; k < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++k ){
			ph7_value *pVal = &aEnv[k].sValue;
			if( SyStringLength(&aEnv[k].sName) == sizeof("this")-1
			 && SyMemcmp(SyStringData(&aEnv[k].sName), "this", sizeof("this")-1) == 0 ){
				continue;
			}
			if( aEnv[k].nIdx != SXU32_HIGH ){
				ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, aEnv[k].nIdx);
				if( pSlot ){
					pVal = pSlot;
				}
			}
			ReflectMapAddDyn(pCtx, pOut, &aEnv[k].sName, pVal);
		}
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
			pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, aStatic[n].nIdx);
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
/* One `key => value` entry of a presented shape, by name. */
static void ClosurePresentAdd(ph7_vm *pVm, ph7_value *pOut, const char *zKey, ph7_value *pVal)
{
	ph7_value sKey;
	PH7_MemObjInitFromString(pVm, &sKey, 0);
	PH7_MemObjStringAppend(&sKey, zKey, (sxu32)SyStrlen(zKey));
	ph7_array_add_elem(pOut, &sKey, pVal);   /* takes its OWN reference */
	PH7_MemObjRelease(&sKey);
}
/*
 * php's DEBUG presentation for a Closure (ph7_class::xPresent) —
 * `zend_closure_get_debug_info` (Zend/zend_closures.c), key for key and in php's
 * order. Every key is CONDITIONAL, which is why a plain `function(){}` shows three
 * and a bound one with captures shows five:
 *
 *   - a FAKE closure (php's ZEND_ACC_FAKE_CLOSURE — a first-class callable or
 *     `Closure::fromCallable` over a NAMED function) shows one `function` key,
 *     `Class::method` when it carries a scope and the bare name otherwise, and NO
 *     name/file/line. A real closure shows `name` (php's `{closure:SCOPE:LINE}`,
 *     which `PH7_VmFuncDisplayName` answers), `file` and an INT `line`;
 *   - `static` is the closure's own variable state — the `use` captures AND the
 *     body's `static $x` — keyed WITHOUT the `$`, and omitted when empty. php
 *     shows it before the first call too, since the initializers are compiled;
 *   - `this` is the bound receiver, present only when there is one (`bindTo`,
 *     an instance first-class callable, or the implicit capture in a method);
 *   - `parameter` is `"$name" => "<required>"|"<optional>"`, `&$name` for a by-ref
 *     parameter, omitted when the callee takes none. php's cut is
 *     `required_num_args`, which counts through the LAST required parameter — so
 *     `function($a, $b = 1, $c)` reports all THREE as `<required>`, not two.
 *
 * The non-debug half answers nothing: php's `get_properties` for a Closure is an
 * empty table, and `(array)$closure` never reaches it at all (php special-cases a
 * Closure in `convert_to_array` and wraps it as a SCALAR, `[0 => $closure]`; PHL
 * answers `[]` there — recorded).
 */
PH7_PRIVATE sxi32 PH7_ClosurePresent(ph7_vm *pVm, ph7_class_instance *pThis,
	ph7_value *pOut, int bDebug)
{
	ReflectFuncRef sRef;
	ph7_value sCarrier, sVal;
	int bFake = 1;
	if( !bDebug || pThis == 0 ){
		return SXRET_OK;
	}
	/* Rule 16's other half: this carrier never took a reference, so it must be
	 * blanked before it is released or the Closure is unref'd a second time. */
	PH7_MemObjInit(pVm, &sCarrier);
	sCarrier.x.pOther = pThis;
	MemObjSetType(&sCarrier, MEMOBJ_OBJ);
	if( !ReflectFuncFill(pVm, &sCarrier, 0, &sRef) ){
		sCarrier.x.pOther = 0;
		sCarrier.iFlags = MEMOBJ_NULL;
		PH7_MemObjRelease(&sCarrier);
		return SXRET_OK;
	}
	sCarrier.x.pOther = 0;
	sCarrier.iFlags = MEMOBJ_NULL;
	PH7_MemObjRelease(&sCarrier);
	/* php's FAKE-closure bit, read off what the callable actually resolved TO: a
	 * real closure's body is the anonymous function itself (the compiler's
	 * `{closure:...}` name, or the synthesized key that stands in for it). */
	if( sRef.pFunc && sRef.pMeth == 0 ){
		const SyString *pN = &sRef.pFunc->sName;
		if( SyStringLength(&sRef.pFunc->sClosureName) > 0
		 || (pN->nByte > 8 && SyMemcmp(pN->zString, "[lambda_", 8) == 0)
		 || (pN->nByte > 9 && SyMemcmp(pN->zString, "[closure_", 9) == 0) ){
			bFake = 0;
		}
	}
	PH7_MemObjInit(pVm, &sVal);
	if( bFake ){
		ph7_class *pScope = ReflectFuncDeclClass(&sRef);
		const SyString *pName = sRef.pFunc ? &sRef.pFunc->sName : &sRef.pHost->sName;
		PH7_MemObjInitFromString(pVm, &sVal, 0);
		if( pScope == 0 && sRef.pClass ){
			pScope = sRef.pClass;
		}
		if( pScope ){
			PH7_MemObjStringAppend(&sVal, SyStringData(&pScope->sName),
				SyStringLength(&pScope->sName));
			PH7_MemObjStringAppend(&sVal, "::", 2);
		}
		PH7_MemObjStringAppend(&sVal, SyStringData(pName), SyStringLength(pName));
		ClosurePresentAdd(pVm, pOut, "function", &sVal);
		PH7_MemObjRelease(&sVal);
	}else{
		const char *zShow;
		int nShow = PH7_VmFuncDisplayName(pVm, sRef.pFunc, &zShow);
		PH7_MemObjInitFromString(pVm, &sVal, 0);
		PH7_MemObjStringAppend(&sVal, zShow, (sxu32)(nShow > 0 ? nShow : 0));
		ClosurePresentAdd(pVm, pOut, "name", &sVal);
		PH7_MemObjRelease(&sVal);
		PH7_MemObjInitFromString(pVm, &sVal, 0);
		PH7_MemObjStringAppend(&sVal, SyStringData(&sRef.pFunc->sFile),
			SyStringLength(&sRef.pFunc->sFile));
		ClosurePresentAdd(pVm, pOut, "file", &sVal);
		PH7_MemObjRelease(&sVal);
		PH7_MemObjInitFromInt(pVm, &sVal, (sxi64)sRef.pFunc->nLine);
		ClosurePresentAdd(pVm, pOut, "line", &sVal);
		PH7_MemObjRelease(&sVal);
	}
	/* `static`: the captured `use` variables first (php compiles them into the
	 * same table, ahead of the body's own statics), then the body's `static $x`,
	 * whose value is the live slot once it exists and the compiled initializer
	 * before that — the rule getStaticVariables() already follows. */
	if( sRef.pFunc ){
		ph7_hashmap *pHm = PH7_NewHashmap(pVm, 0, 0);
		sxu32 n;
		if( pHm ){
			ph7_value sMap;
			ph7_value *pMap = &sMap;
			ph7_vm_func_closure_env *aEnv =
				(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);
			ph7_vm_func_static_var *aStatic =
				(ph7_vm_func_static_var *)SySetBasePtr(&sRef.pFunc->aStatic);
			PH7_MemObjInitFromArray(pVm, pMap, pHm);
			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++n ){
				ph7_value *pVal = &aEnv[n].sValue;
				ph7_value sKey;
				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1
				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){
					/* PHL carries the auto-captured `$this` as an env entry; php keeps
					 * it in its own `this_ptr` slot and never lists it as a static. It
					 * is reported below, under `this`. */
					continue;
				}
				if( aEnv[n].nIdx != SXU32_HIGH ){
					/* `use (&$x)`: the capture is an alias onto a pinned slot, and
					 * php reports what the slot holds NOW, not the birth value. */
					ph7_value *pSlot = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, aEnv[n].nIdx);
					if( pSlot ){
						pVal = pSlot;
					}
				}
				PH7_MemObjInitFromString(pVm, &sKey, &aEnv[n].sName);
				ph7_array_add_elem(pMap, &sKey, pVal);
				PH7_MemObjRelease(&sKey);
			}
			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aStatic) ; ++n ){
				ph7_value *pVal = 0;
				ph7_value sScratch, sKey;
				int bScratch = 0;
				if( aStatic[n].nIdx != SXU32_HIGH ){
					pVal = (ph7_value *)PH7_MemObjAt(&pVm->aMemObj, aStatic[n].nIdx);
				}
				if( pVal == 0 ){
					PH7_MemObjInit(pVm, &sScratch);
					if( SySetUsed(&aStatic[n].aByteCode) > 0 ){
						VmLocalExec(pVm, &aStatic[n].aByteCode, &sScratch, FALSE);
					}
					pVal = &sScratch;
					bScratch = 1;
				}
				PH7_MemObjInitFromString(pVm, &sKey, &aStatic[n].sName);
				ph7_array_add_elem(pMap, &sKey, pVal);
				PH7_MemObjRelease(&sKey);
				if( bScratch ){
					PH7_MemObjRelease(&sScratch);
				}
			}
			if( ph7_array_count(pMap) > 0 ){
				ClosurePresentAdd(pVm, pOut, "static", pMap);
			}
			PH7_MemObjRelease(pMap);
		}
	}
	/* `this`: the bound receiver. php keeps ONE `this_ptr` however the binding
	 * happened; PHL keeps two — an explicit bind (`bindTo`, an instance
	 * first-class callable) writes the `$__this` slot, while the IMPLICIT capture a
	 * closure written inside a method gets rides in the closure ENVIRONMENT under
	 * the name `this`. Either one is php's answer here. */
	{
		SyString sAttr;
		ph7_value *pBound;
		SyStringInitFromBuf(&sAttr, "__this", 6);
		pBound = PH7_ClassInstanceFetchAttr(pThis, &sAttr);
		if( (pBound == 0 || (pBound->iFlags & MEMOBJ_OBJ) == 0) && sRef.pFunc ){
			ph7_vm_func_closure_env *aEnv =
				(ph7_vm_func_closure_env *)SySetBasePtr(&sRef.pFunc->aClosureEnv);
			sxu32 n;
			pBound = 0;
			for( n = 0 ; n < SySetUsed(&sRef.pFunc->aClosureEnv) ; ++n ){
				if( SyStringLength(&aEnv[n].sName) == sizeof("this")-1
				 && SyMemcmp(SyStringData(&aEnv[n].sName), "this", sizeof("this")-1) == 0 ){
					pBound = &aEnv[n].sValue;
					break;
				}
			}
		}
		if( pBound && (pBound->iFlags & MEMOBJ_OBJ) && pBound->x.pOther ){
			ClosurePresentAdd(pVm, pOut, "this", pBound);
		}
	}
	/* `parameter`: php's cut is required_num_args, which runs through the LAST
	 * required parameter rather than stopping at the first optional one. */
	{
		ReflectParamDesc sDesc;
		int nArgTotal = 0, nRequired = 0, i;
		while( ReflectParamAt(&sRef, nArgTotal, &sDesc) ){
			if( !sDesc.bOptional ){
				nRequired = nArgTotal + 1;
			}
			nArgTotal++;
		}
		if( nArgTotal > 0 ){
			ph7_hashmap *pHm = PH7_NewHashmap(pVm, 0, 0);
			if( pHm ){
				ph7_value sMap;
				ph7_value *pMap = &sMap;
				PH7_MemObjInitFromArray(pVm, pMap, pHm);
				for( i = 0 ; i < nArgTotal ; ++i ){
					ph7_value sKey, sWhat;
					if( !ReflectParamAt(&sRef, i, &sDesc) ){
						break;
					}
					PH7_MemObjInitFromString(pVm, &sKey, 0);
					if( sDesc.bByRef ){
						PH7_MemObjStringAppend(&sKey, "&", 1);
					}
					PH7_MemObjStringAppend(&sKey, "$", 1);
					PH7_MemObjStringAppend(&sKey, SyStringData(&sDesc.sName),
						SyStringLength(&sDesc.sName));
					PH7_MemObjInitFromString(pVm, &sWhat, 0);
					PH7_MemObjStringAppend(&sWhat,
						i >= nRequired ? "<optional>" : "<required>",
						i >= nRequired ? sizeof("<optional>")-1 : sizeof("<required>")-1);
					ph7_array_add_elem(pMap, &sKey, &sWhat);
					PH7_MemObjRelease(&sKey);
					PH7_MemObjRelease(&sWhat);
				}
				ClosurePresentAdd(pVm, pOut, "parameter", pMap);
				PH7_MemObjRelease(pMap);
			}
		}
	}
	return SXRET_OK;
}
/*
 * A METHOD belongs to the extension its DECLARING class does -- php reports SPL
 * for a method a userland subclass inherited from ArrayObject -- and a plain
 * function to the one the partition places its own name in.
 */
static int ReflectFuncExtId(const ReflectFuncRef *pRef)
{
	const SyString *pName;
	if( !ReflectFuncIsInternal(pRef) || pRef->bFabricated ){
		/* A fabricated method belongs to no module: php answers false for
		 * getExtensionName() and NULL for getExtension(), and prints a bare
		 * `<internal>` in the export. */
		return -1;
	}
	if( pRef->pMeth ){
		return ReflectClassExtId(ReflectFuncDeclClass(pRef));
	}
	pName = pRef->pHost ? &pRef->pHost->sName : &pRef->pFunc->sName;
	return PH7_VmExtOfFunc(SyStringData(pName), (int)SyStringLength(pName));
}
static int vm_builtin_ReflectionFunc_getExtensionName(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	int iExt;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_bool(pCtx, 0))
	iExt = ReflectFuncExtId(&sRef);
	if( iExt < 0 ){
		ph7_result_bool(pCtx, 0);
	}else{
		ph7_result_string(pCtx, PH7_VmExtensionName(iExt), -1);
	}
	return PH7_OK;
}
static int vm_builtin_ReflectionFunc_getExtension(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	REFLECT_FUNC_OR(sRef, ph7_result_null(pCtx))
	return ReflectExtensionOf(pCtx, ReflectFuncExtId(&sRef));
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
	return ReflectExportFuncSelf(pCtx);
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
	if( !ReflectFuncFill(pCtx->pVm, apArg[0], 0, &sRef) ){
		const char *zName;
		int nName;
		if( pClo ){
			/* A Closure whose body no table holds -- php's magic-method TRAMPOLINE
			 * (`Closure::fromCallable([$o, 'zz'])` where the class reaches `zz`
			 * only through __call) is the shape real libraries hit. php still
			 * NAMES it: the name is the one that was asked for, which the Closure
			 * carries in $__fn, and the scope is $__scope. Leaving `name`
			 * uninitialized made every read of it a typed-property Error, and
			 * `str_contains($r->name, '{closure')` is one line of twig's filter
			 * compiler. The rest of the accessors still answer emptily: there is
			 * no body to describe, which is php's answer too (0 parameters, no
			 * file, no line). */
			SyString sAttr;
			ph7_value *pFn;
			PH7_NativeSetAttrObj(pVm, pThis, RF_CL, pClo);
			SyStringInitFromBuf(&sAttr, "__fn", 4);
			pFn = PH7_ClassInstanceFetchAttr(pClo, &sAttr);
			if( pFn && (pFn->iFlags & MEMOBJ_STRING) && SyBlobLength(&pFn->sBlob) > 0 ){
				PH7_NativeSetAttrStr(pVm, pThis, "name",
					(const char *)SyBlobData(&pFn->sBlob), (int)SyBlobLength(&pFn->sBlob));
			}
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
			/* php 8.4 names an anonymous function `{closure:SCOPE:LINE}`, where SCOPE is
			 * the enclosing function and only the FILE at top level — the compiler
			 * recorded it (sClosureName); the file form is the fallback for a closure
			 * built without one. */
			char zBuf[512];
			const char *zShow = SyStringData(&sRef.pFunc->sClosureName);
			int nShow = (int)SyStringLength(&sRef.pFunc->sClosureName);
			if( nShow < 1 ){
				const char *zFile = SyStringLength(&sRef.pFunc->sFile) > 0
					? SyStringData(&sRef.pFunc->sFile) : "";
				int nFile = (int)SyStringLength(&sRef.pFunc->sFile);
				nShow = SyBufferFormat(zBuf, sizeof(zBuf), "{closure:%.*s:%u}",
					nFile, zFile, sRef.pFunc->nLine);
				zShow = zBuf;
			}
			PH7_NativeSetAttrStr(pVm, pThis, "name", zShow, nShow);
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
			"Method %z::%.*s() does not exist", &pClass->sDisp, nMethod, zMethod);
		goto Done;
	}
	if( ((ph7_class_method *)pEntry->pUserData)->iFlags & PH7_CLASS_ATTR_FABRICATED ){
		/* php has no such row in the class's function table, so a CONSTRUCTOR asked
		 * for it by class name finds nothing: `new ReflectionMethod('Closure',
		 * '__invoke')` is a ReflectionException even though the method answers
		 * everywhere else. Given the OBJECT, php builds it -- and so does a
		 * ReflectionClass door, which is the nReflectFactory window. */
		if( (sClass.iFlags & MEMOBJ_OBJ) == 0 && pVm->nReflectFactory < 1 ){
			rc = PH7_VmThrowException(pCtx, "ReflectionException",
				"Method %z::%.*s() does not exist", &pClass->sDisp, nMethod, zMethod);
			goto Done;
		}
		if( sClass.iFlags & MEMOBJ_OBJ ){
			/* ...and what it builds describes THAT closure: park the instance where
			 * ReflectFuncOfThis already looks for a reflected Closure, so the
			 * parameter list, the return type and invoke() are the closure's own. */
			PH7_NativeSetAttrObj(pVm, pThis, RF_CL, (ph7_class_instance *)sClass.x.pOther);
		}
	}
	{
		/* php's $class is the DECLARING class, not the one the lookup went
		 * through: `new ReflectionMethod('Kid','bm')` on an inherited method
		 * reports Base. */
		ph7_class *pDecl = ReflectMethodDeclClass(pClass, (ph7_class_method *)pEntry->pUserData);
		PH7_NativeSetAttrStr(pVm, pThis, "class", SyStringData(&pDecl->sName),
			(int)SyStringLength(&pDecl->sName));
	}
	/* The class the reflector was built FOR, which the export tags read. Every
	 * ReflectionClass door that hands a method out passes its OWN name as the
	 * first constructor argument, so they all land here too. */
	PH7_NativeSetAttrStr(pVm, pThis, RM_CE, SyStringData(&pClass->sName),
		(int)SyStringLength(&pClass->sName));
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
	/* And it binds the arguments WEAKLY however strict the file that called
	 * invoke() is: php reads strict mode off the frame that MADE the call, and
	 * that frame is ReflectionMethod::invoke itself, an internal function with no
	 * strict_types of its own. PHPUnit's mock builder reaches every original
	 * constructor this way (Generator::instantiate -> getConstructor()->invokeArgs),
	 * from a file that declares strict_types=1. */
	pVm->bCallbackWeak = 1;
	rc = PH7_VmCallClassMethod(pVm, pRecv, sRef.pMeth, &sResult, nCall, apCall);
	pVm->bCallbackWeak = 0; /* clear if the dispatch never reached an OP_CALL */
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
	{
		/* php hands back a closure over the RESOLVED method and reflection is allowed past
		 * protection (8.1+), so this one must dispatch without a second visibility decision —
		 * the FCC_METHOD mark is what tells the unwrap its $__fn is a screened METHOD name. */
		ph7_class_instance *pClo = PH7_VmNewClosure(pCtx->pVm, &sRef.pMeth->sFunc.sName,
			pRecv, &sRef.pClass->sName);
		if( pClo ){
			pClo->iFlags |= VM_INSTANCE_FCC_METHOD|VM_INSTANCE_FCC_SCREENED;
		}
		return ReflectResultObject(pCtx, pClo);
	}
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
 * php's `overwrites`: the class the entry found in the PARENT's method table
 * belongs to — which is an interface when the parent only inherited it from
 * one, so `ReflectionFunction::__toString` overwrites Stringable. A method
 * that first appears in an interface the class itself implements is a
 * `prototype` instead, never an overwrite.
 */
static ph7_class * ReflectOverwritesIn(ph7_class *pClass, const char *zName, int nName)
{
	ph7_class *pWalk;
	int iDepth = 0;
	if( pClass == 0 || nName < 1 ){
		return 0;
	}
	for( pWalk = pClass->pBase ; pWalk && iDepth <= REFLECT_WALK_MAX_DEPTH ; pWalk = pWalk->pBase ){
		SyHashEntry *pEntry = ReflectFindMethodEntry(pWalk, zName, nName);
		if( pEntry ){
			ph7_class_method *pMeth = (ph7_class_method *)pEntry->pUserData;
			if( pMeth->iProtection != PH7_CLASS_PROT_PRIVATE ){
				return ReflectMethodDeclClass(pWalk, pMeth);
			}
		}
		iDepth++;
	}
	return 0;
}
/*
 * php's `prototype`: the ROOT-most declaration the entry in pClass's method
 * table answers to, or NULL.
 *
 * zend assigns it once, at link time, as `child->prototype = parent->prototype
 * ? parent->prototype : parent` — so it CHAINS past every intermediate
 * override and names the class where the contract began, not the nearest one
 * (`AppendIterator::current` is `prototype Iterator`, three classes up, where
 * this engine answered its immediate parent). Two rules ride on it, both
 * measured against php 8.5.9:
 *
 *   - An INTERFACE wins over the parent chain. zend inherits from the parent
 *     class first and implements the interfaces after, and each implementation
 *     re-assigns the prototype — so `class C extends B implements I`, with both
 *     declaring f(), reports I and not B.
 *   - A CONSTRUCTOR takes one only where the contract is really a contract:
 *     the chain link that would have STARTED it is dropped unless it is
 *     abstract (an interface's ctor is abstract too). An inherited prototype
 *     still rides through, so a ctor three deep from an abstract one keeps it.
 */
static ph7_class * ReflectPrototypeOf(ph7_context *pCtx, ph7_class *pClass,
	const char *zName, int nName, int iDepth)
{
	ph7_class *pWalk, *pDecl, *pRes = 0;
	SyHashEntry *pOwn;
	int bCtor;
	if( pClass == 0 || nName < 1 || iDepth > REFLECT_WALK_MAX_DEPTH ){
		return 0;
	}
	pOwn = ReflectFindMethodEntry(pClass, zName, nName);
	if( pOwn == 0 ){
		return 0;
	}
	bCtor = nName == sizeof("__construct")-1
		&& SyStrnicmp(zName, "__construct", sizeof("__construct")-1) == 0;
	pDecl = ReflectMethodDeclClass(pClass, (ph7_class_method *)pOwn->pUserData);
	if( pDecl != pClass ){
		/* The class did not declare this one: zend copies the record in and
		 * assigns NOTHING, so whatever prototype the record already carries is
		 * what a reflector reads here. `DOMAttr::C14N` therefore has none at
		 * all, where this engine named the nearest declaring base. */
		pRes = ReflectPrototypeOf(pCtx, pDecl, zName, nName, iDepth + 1);
	}else if( (pClass->iFlags & PH7_CLASS_INTERFACE) == 0 ){
		/* Its own declaration, checked against the parent's at link time. An
		 * interface has no such link — its parents are declared parents, and
		 * are walked with the interface list below. */
		for( pWalk = pClass->pBase ; pWalk ; pWalk = pWalk->pBase ){
			SyHashEntry *pEntry = ReflectFindMethodEntry(pWalk, zName, nName);
			ph7_class_method *pMeth;
			if( pEntry == 0 ){
				continue;
			}
			pMeth = (ph7_class_method *)pEntry->pUserData;
			if( pMeth->iProtection == PH7_CLASS_PROT_PRIVATE ){
				break;
			}
			pRes = ReflectPrototypeOf(pCtx, pWalk, zName, nName, iDepth + 1);
			if( pRes == 0 && !(bCtor && (pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT) == 0) ){
				pRes = ReflectMethodDeclClass(pWalk, pMeth);
			}
			break;
		}
	}
	/* Each interface this class DECLARES re-assigns it, the last one winning —
	 * which is why an interface beats the parent chain. PHL keeps the first
	 * parent of an INTERFACE on the base chain, so it joins the list here. */
	{
		ph7_class **apIface = (ph7_class **)SySetBasePtr(&pClass->aInterface);
		sxu32 n, nIface = SySetUsed(&pClass->aInterface);
		for( n = 0 ; n <= nIface ; n++ ){
			ph7_class *pIface;
			SyHashEntry *pEntry;
			if( n == nIface ){
				pIface = (pClass->iFlags & PH7_CLASS_INTERFACE) ? pClass->pBase : 0;
			}else{
				pIface = apIface[n];
			}
			if( pIface == 0 ){
				continue;
			}
			pEntry = ReflectFindMethodEntry(pIface, zName, nName);
			/* Nothing to check against when the class holds the interface's
			 * very own record: an interface that merely EXTENDS another copies
			 * the method in and stops, so `interface B extends A` prints
			 * `inherits A` and no prototype at all. */
			if( pEntry == 0 || pOwn->pUserData == pEntry->pUserData ){
				continue;
			}
			pRes = ReflectPrototypeOf(pCtx, pIface, zName, nName, iDepth + 1);
			if( pRes == 0 ){
				pRes = ReflectMethodDeclClass(pIface,
					(ph7_class_method *)pEntry->pUserData);
			}
		}
	}
	return pRes;
}
/* The same question asked by a ReflectionMethod receiver, which carries the
 * method name on `$this`. The prototype belongs to the entry in the class the
 * reflector was BUILT FOR, which is php's anchor for it too. */
static ph7_class * ReflectMethodPrototype(ph7_context *pCtx, ReflectFuncRef *pRef)
{
	ph7_class_instance *pThis = PH7_ContextThis(pCtx);
	ph7_class *pOwner = ReflectOwnerOfThis(pCtx);
	const char *zName = "";
	int nName = 0;
	if( pThis == 0 ){
		return 0;
	}
	PH7_NativeAttrStr(pThis, "name", &zName, &nName);
	return ReflectPrototypeOf(pCtx, pOwner ? pOwner : pRef->pClass, zName, nName, 0);
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
	rc = ReflectFuncFill(pCtx->pVm, pT, (pM && (pM->iFlags & MEMOBJ_STRING)) ? pM : 0, pRef);
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
			ph7_class_method *pM0 = pB && (pB->iFlags & MEMOBJ_STRING)
				? PH7_ClassExtractMethod(pObj->pClass,
					(const char *)SyBlobData(&pB->sBlob), SyBlobLength(&pB->sBlob))
				: 0;
			if( pM0 && (pM0->iFlags & PH7_CLASS_ATTR_FABRICATED) ){
				/* `[$closure, '__invoke']` has to keep the OBJECT: the class name alone
				 * resolves to the empty class-level declaration, and it is THIS
				 * closure's parameter list the pair is naming. Every other pair
				 * reduces to its class, which is what php reports as the parameter's
				 * declaring class either way. */
				PH7_MemObjStore(pA, &sTarget);
			}else{
				ph7_value_string(&sTarget, SyStringData(&pObj->pClass->sName),
					(int)SyStringLength(&pObj->pClass->sName));
			}
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
	if( !ReflectFuncFill(pCtx->pVm, &sTarget, (sMethod.iFlags & MEMOBJ_STRING) ? &sMethod : 0, &sRef) ){
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
	if( sRef.bFabricated && sRef.pClosure && sRef.pMeth ){
		/* Its target is the OBJECT plus the method name: re-resolving by class name
		 * would find the empty class-level declaration instead of this closure. */
		PH7_NativeSetAttrObj(pVm, pThis, RP_T, sRef.pClosure);
		PH7_NativeSetAttrStr(pVm, pThis, RP_M, SyStringData(&sRef.pMeth->sFunc.sName),
			(int)SyStringLength(&sRef.pMeth->sFunc.sName));
	}else if( sRef.pMeth && sRef.pClass ){
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
 * getType() but still declares them and still answers. The E_DEPRECATED that
 * comes with a call is raised at the CALL now, from the one table every
 * deprecated internal name is stamped from (aDeprecatedFunc[]), which is where
 * php raises it too — before the callee's own screens rather than inside it.
 */
static int vm_builtin_ReflectionParameter_getClass(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	ReflectFuncRef sRef;
	ReflectParamDesc sDesc;
	const char *zType;
	int nType;
	SXUNUSED(nArg);
	SXUNUSED(apArg);
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
	return ReflectParamTypeIs(pCtx, "array");
}
static int vm_builtin_ReflectionParameter_isCallable(ph7_context *pCtx, int nArg, ph7_value **apArg)
{
	SXUNUSED(nArg);
	SXUNUSED(apArg);
	return ReflectParamTypeIs(pCtx, "callable");
}
/*
 * The class `self`/`parent` resolve against inside a parameter's default: the one
 * that DECLARED the method (a trait's members belong to the composing class). 0 for
 * a plain function, whose defaults can name neither.
 */
static ph7_class * ReflectParamSelfClass(const ReflectFuncRef *pRef)
{
	if( pRef->pMeth == 0 ){
		return 0;
	}
	return PH7_VmMemberOwnerClass((ph7_class *)pRef->pMeth->sFunc.pUserData,
		pRef->pClass ? pRef->pClass : (ph7_class *)pRef->pMeth->sFunc.pUserData);
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
		/* Compiled: the same evaluation path the VM uses for an omitted argument --
		 * except that one runs INSIDE the call, where the frame already names the
		 * class `self::K` resolves against. Reflection has no such frame, so a
		 * default written `self::K` answered `Class "self" not found` where php
		 * answers its value. Mark the declaring class the way a member
		 * initializer's evaluation does (PH7_VmPeekDeclaringClass reads the pair). */
		ph7_value sValue;
		ph7_class *pSaveCls = pCtx->pVm->pConstEvalClass;
		void *pSaveFrame = pCtx->pVm->pConstEvalFrame;
		ph7_class *pDecl = ReflectParamSelfClass(&sRef);
		if( pDecl ){
			pCtx->pVm->pConstEvalClass = pDecl;
			pCtx->pVm->pConstEvalFrame = (void *)VmSkipExceptionFrames(pCtx->pVm->pFrame);
		}
		PH7_MemObjInit(pCtx->pVm, &sValue);
		VmLocalExec(pCtx->pVm, &sDesc.pArg->aByteCode, &sValue, FALSE);
		pCtx->pVm->pConstEvalClass = pSaveCls;
		pCtx->pVm->pConstEvalFrame = pSaveFrame;
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
 * [ OP_LOADC (EXPAND), OP_DONE ] with the name in the literal table.
 *
 * A DECLARED default is TEXT, and php answers the same two questions about one:
 * `int $type = PDO::PARAM_STR` is a constant default and its name is what the
 * stub wrote. Only a SINGLE reference counts -- the `|` fold beside it
 * evaluates to a number that no constant carries, and php answers false and
 * null for it while still printing the source in the export.
 */
static int ReflectParamDefConst(ph7_context *pCtx, ReflectParamDesc *pDesc,
	const char **pz, int *pn)
{
	VmInstr *aInstr;
	ph7_value *pLit;
	if( pDesc->pArg == 0 && pDesc->bHasDef ){
		const char *zDef = SyStringData(&pDesc->sDefText);
		int nDef = (int)SyStringLength(&pDesc->sDefText);
		ph7_value *pVal;
		ReflectSigTrim(&zDef, &nDef);
		if( nDef < 1 || ReflectSigHas(zDef, nDef, "|", 1) ){
			return 0;
		}
		pVal = ph7_context_new_scalar(pCtx);
		if( pVal == 0 ){
			return 0;
		}
		if( nDef > (int)sizeof("::class")-1
		 && SyMemcmp(&zDef[nDef - (sizeof("::class")-1)], "::class",
		             sizeof("::class")-1) == 0 ){
			/* `C::class` reads a class NAME rather than a constant, and php
			 * answers false for it while still printing it in the export. */
			return 0;
		}
		if( !ReflectSigGlobalConst(pCtx, zDef, nDef, pVal)
		 && !ReflectSigClassConst(pCtx, zDef, nDef, pVal) ){
			return 0;
		}
		*pz = zDef;
		*pn = nDef;
		return 1;
	}
	if( pDesc->pArg == 0 ){
		return 0;
	}
	if( SySetUsed(&pDesc->pArg->aByteCode) == 4 ){
		/* A CLASS constant compiles to the class name, the member name and the `::`
		 * fetch: [ LOADC <class>, LOADC <member>, MEMBER(static, bareword), DONE ].
		 * php answers true for one and names it the way the source spelled it --
		 * `self::K` stays `self::K` -- minus a leading `\`, which it drops. Only the
		 * plain global-constant shape below was recognised, so every class-constant
		 * default reported itself as no constant at all. */
		VmInstr *aCC = (VmInstr *)SySetBasePtr(&pDesc->pArg->aByteCode);
		if( aCC[0].iOp == PH7_OP_LOADC && aCC[1].iOp == PH7_OP_LOADC
		 && aCC[2].iOp == PH7_OP_MEMBER && aCC[2].iP1 == 1 && aCC[2].p3 == 0
		 && aCC[3].iOp == PH7_OP_DONE ){
			ph7_value *pCls = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj,aCC[0].iP2);
			ph7_value *pMem = (ph7_value *)SySetAt(&pCtx->pVm->aLitObj,aCC[1].iP2);
			if( pCls && pMem && SyBlobLength(&pCls->sBlob) > 0
			 && SyBlobLength(&pMem->sBlob) > 0
			 && !(SyBlobLength(&pMem->sBlob) == sizeof("class")-1
			   && SyStrnicmp((const char *)SyBlobData(&pMem->sBlob),"class",
			                 sizeof("class")-1) == 0) ){
				/* `C::class` reads a class NAME rather than a constant, and php
				 * answers false for it -- the same rule the declared-signature path
				 * above makes. */
				const char *zCls = (const char *)SyBlobData(&pCls->sBlob);
				sxu32 nCls = SyBlobLength(&pCls->sBlob);
				SyBlob *pOut = &pCtx->pVm->sReflectConstName;
				while( nCls > 0 && zCls[0] == '\\' ){
					zCls++;
					nCls--;
				}
				SyBlobReset(pOut);
				SyBlobAppend(pOut,zCls,nCls);
				SyBlobAppend(pOut,"::",sizeof("::")-1);
				SyBlobAppend(pOut,SyBlobData(&pMem->sBlob),SyBlobLength(&pMem->sBlob));
				*pz = (const char *)SyBlobData(pOut);
				*pn = (int)SyBlobLength(pOut);
				return 1;
			}
		}
		return 0;
	}
	if( SySetUsed(&pDesc->pArg->aByteCode) != 2 ){
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
	return ReflectExportParamSelf(pCtx);
}
/*
 * Declare chunk 2. Called from PH7_VmInstallReflection() where it used to be
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
		{ "name",  PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
		/* PHL-only: the Closure being reflected. php reaches the same state from
		 * the function record itself; PHL has no hidden-slot bit yet (recorded). */
		{ RF_CL,   PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aAbstractMethod[] = {
		{ "__clone",       PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },
		{ "inNamespace",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_inNamespace },
		{ "isClosure",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isClosure },
		{ "isDeprecated",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isDeprecated },
		{ "isInternal",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isInternal },
		{ "isUserDefined", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isUserDefined },
		{ "isGenerator",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isGenerator },
		{ "isVariadic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isVariadic },
		{ "isStatic",      PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_isStatic },
		{ "getClosureThis", PH7_MOD_PUBLIC, "", "@?object", vm_builtin_ReflectionFunc_getClosureThis },
		{ "getClosureScopeClass", PH7_MOD_PUBLIC, "", "@?ReflectionClass",
		  vm_builtin_ReflectionFunc_getClosureScopeClass },
		/* php answers the same class for both; PHL has no separate called-scope. */
		{ "getClosureCalledClass", PH7_MOD_PUBLIC, "", "@?ReflectionClass",
		  vm_builtin_ReflectionFunc_getClosureCalledClass },
		{ "getClosureUsedVariables", PH7_MOD_PUBLIC, "", "array",
		  vm_builtin_ReflectionFunc_getClosureUsedVariables },
		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string|false", vm_builtin_ReflectionFunc_getDocComment },
		{ "getEndLine",    PH7_MOD_PUBLIC, "", "@int|false", vm_builtin_ReflectionFunc_getEndLine },
		{ "getExtension",  PH7_MOD_PUBLIC, "", "@?ReflectionExtension", vm_builtin_ReflectionFunc_getExtension },
		{ "getExtensionName", PH7_MOD_PUBLIC, "", "@string|false", vm_builtin_ReflectionFunc_getExtensionName },
		{ "getFileName",   PH7_MOD_PUBLIC, "", "@string|false", vm_builtin_ReflectionFunc_getFileName },
		{ "getName",       PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getName },
		{ "getNamespaceName", PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getNamespaceName },
		{ "getNumberOfParameters", PH7_MOD_PUBLIC, "", "@int",
		  vm_builtin_ReflectionFunc_getNumberOfParameters },
		{ "getNumberOfRequiredParameters", PH7_MOD_PUBLIC, "", "@int",
		  vm_builtin_ReflectionFunc_getNumberOfRequiredParameters },
		{ "getParameters", PH7_MOD_PUBLIC, "", "@array", vm_builtin_ReflectionFunc_getParameters },
		{ "getShortName",  PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionFunc_getShortName },
		{ "getStartLine",  PH7_MOD_PUBLIC, "", "@int|false", vm_builtin_ReflectionFunc_getStartLine },
		{ "getStaticVariables", PH7_MOD_PUBLIC, "", "@array",
		  vm_builtin_ReflectionFunc_getStaticVariables },
		{ "returnsReference", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_returnsReference },
		{ "hasReturnType", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunc_hasReturnType },
		{ "getReturnType", PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionFunc_getReturnType },
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
		{ "isDisabled",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionFunction_isDisabled },
		{ "invoke",      PH7_MOD_PUBLIC, "mixed ...$args", "@mixed",
		  vm_builtin_ReflectionFunction_invoke },
		{ "invokeArgs",  PH7_MOD_PUBLIC, "array $args", "@mixed",
		  vm_builtin_ReflectionFunction_invokeArgs },
		{ "getClosure",  PH7_MOD_PUBLIC, "", "@Closure", vm_builtin_ReflectionFunction_getClosure },
	};
	static const PH7_NativePropDef aMethodProp[] = {
		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
		/* PHL-only: php's `intern->ce`, which no property publishes. */
		{ RM_CE,   PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
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
		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isPublic },
		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isPrivate },
		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isProtected },
		{ "isAbstract",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isAbstract },
		{ "isFinal",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isFinal },
		{ "isConstructor", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isConstructor },
		{ "isDestructor",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionMethod_isDestructor },
		{ "getClosure",  PH7_MOD_PUBLIC, "?object $object = null", "@Closure",
		  vm_builtin_ReflectionMethod_getClosure },
		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionMethod_getModifiers },
		{ "invoke",      PH7_MOD_PUBLIC, "?object $object, mixed ...$args", "@mixed",
		  vm_builtin_ReflectionMethod_invoke },
		{ "invokeArgs",  PH7_MOD_PUBLIC, "?object $object, array $args", "@mixed",
		  vm_builtin_ReflectionMethod_invokeArgs },
		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",
		  vm_builtin_ReflectionMethod_getDeclaringClass },
		{ "getPrototype", PH7_MOD_PUBLIC, "", "@ReflectionMethod", vm_builtin_ReflectionMethod_getPrototype },
		{ "hasPrototype", PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionMethod_hasPrototype },
		{ "setAccessible", PH7_MOD_PUBLIC, "bool $accessible", "@void",
		  vm_builtin_ReflectionMethod_setAccessible },
	};
	static const PH7_NativePropDef aParamProp[] = {
		{ "name", PH7_MOD_PUBLIC,    { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
		/* PHL-only, the three that identify the parameter (recorded) */
		{ RP_T,   PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ RP_M,   PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
		{ RP_P,   PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_INT,  0, 0, 0.0 }, 0 },
	};
	static const PH7_NativeMethodDef aParamMethod[] = {
		{ "__clone",     PH7_MOD_PRIVATE, "", "void", vm_builtin_ReflectionFunc_clone },
		/* php leaves $function UNTYPED here: it takes a name, a Closure, a
		 * `[$obj, 'm']` pair or "C::m", and no declarable union covers all four. */
		{ "__construct", PH7_MOD_PUBLIC, "$function, string|int $param", "",
		  vm_builtin_ReflectionParameter_construct },
		{ "__toString",  PH7_MOD_PUBLIC, "", "string", vm_builtin_ReflectionParameter_toString },
		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionParameter_getName },
		{ "isPassedByReference", PH7_MOD_PUBLIC, "", "@bool",
		  vm_builtin_ReflectionParameter_isPassedByReference },
		{ "canBePassedByValue",  PH7_MOD_PUBLIC, "", "@bool",
		  vm_builtin_ReflectionParameter_canBePassedByValue },
		{ "getDeclaringFunction", PH7_MOD_PUBLIC, "", "@ReflectionFunctionAbstract",
		  vm_builtin_ReflectionParameter_getDeclaringFunction },
		{ "getDeclaringClass",   PH7_MOD_PUBLIC, "", "@?ReflectionClass",
		  vm_builtin_ReflectionParameter_getDeclaringClass },
		{ "getClass",    PH7_MOD_PUBLIC, "", "@?ReflectionClass", vm_builtin_ReflectionParameter_getClass },
		{ "hasType",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_hasType },
		{ "getType",     PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionParameter_getType },
		{ "isArray",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isArray },
		{ "isCallable",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isCallable },
		{ "allowsNull",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_allowsNull },
		{ "getPosition", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionParameter_getPosition },
		{ "isOptional",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isOptional },
		{ "isDefaultValueAvailable", PH7_MOD_PUBLIC, "", "@bool",
		  vm_builtin_ReflectionParameter_isDefaultValueAvailable },
		{ "getDefaultValue", PH7_MOD_PUBLIC, "", "@mixed",
		  vm_builtin_ReflectionParameter_getDefaultValue },
		{ "isDefaultValueConstant", PH7_MOD_PUBLIC, "", "@bool",
		  vm_builtin_ReflectionParameter_isDefaultValueConstant },
		{ "getDefaultValueConstantName", PH7_MOD_PUBLIC, "", "@?string",
		  vm_builtin_ReflectionParameter_getDefaultValueConstantName },
		{ "isVariadic",  PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionParameter_isVariadic },
		{ "isPromoted",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionParameter_isPromoted },
		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",
		  vm_builtin_ReflectionParameter_getAttributes },
	};
	static const PH7_NativeClassSpec aSpec[] = {
		{ "ReflectionFunctionAbstract", 0, "Reflector", PH7_CLASS_ABSTRACT|PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aAbstractMethod, SX_ARRAYSIZE(aAbstractMethod), 0, 0,
		  aAbstractProp, SX_ARRAYSIZE(aAbstractProp), 0, 0, 0 },
		{ "ReflectionFunction", "ReflectionFunctionAbstract", 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aFunctionMethod, SX_ARRAYSIZE(aFunctionMethod),
		  aFunctionConst, SX_ARRAYSIZE(aFunctionConst), 0, 0, 0, 0, 0 },
		{ "ReflectionMethod", "ReflectionFunctionAbstract", 0, PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aMethodMethod, SX_ARRAYSIZE(aMethodMethod),
		  aMethodConst, SX_ARRAYSIZE(aMethodConst),
		  aMethodProp, SX_ARRAYSIZE(aMethodProp), 0, 0, 0 },
		{ "ReflectionParameter", 0, "Reflector", PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aParamMethod, SX_ARRAYSIZE(aParamMethod), 0, 0,
		  aParamProp, SX_ARRAYSIZE(aParamProp), 0, 0, 0 },
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
 * refusing (recorded). */
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
		ph7_class *pDecl = PH7_VmMemberOwnerClass(pAttr->pDeclClass,pClass);
		return PH7_VmThrowException(pCtx, "Error",
			"Typed static property %z::$%z must not be accessed before initialization",
			&pDecl->sDisp, &pAttr->sName);
	}
	pVal = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);
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
	pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pAttr->nIdx);
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
		return PH7_VmMemberOwnerClass(pRef->pAttr->pDeclClass,pRef->pClass);
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
/*
 * The instance slot a resolved ReflectionProperty addresses. A base class's
 * PRIVATE property lives under php's MANGLED storage name on every object below
 * it, so looking it up by its plain name would find the same-named property of
 * the object's OWN class instead -- reading and writing the wrong slot.
 */
static VmClassAttr * ReflectRefInstanceAttr(ph7_vm *pVm, ph7_class_instance *pObj,
	const ReflectMemberRef *pRef)
{
	if( pObj && pRef->pAttr ){
		const SyString *pKey = PH7_ClassAttrStorageName(pVm, pObj->pClass, pRef->pAttr);
		SyHashEntry *pEntry = SyHashGet(&pObj->hAttr,
			(const void *)SyStringData(pKey), SyStringLength(pKey));
		return pEntry ? (VmClassAttr *)pEntry->pUserData : 0;
	}
	return ReflectInstanceAttr(pObj, pRef->zName, pRef->nName);
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
		"Property %z::$%.*s does not exist", &pClass->sDisp, nProp, zProp);
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
		case 4: bYes = ReflectPropProtectedSet(pAttr); break;
		case 5: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0; break;
		case 6: bYes = (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) != 0; break;
		case 7: bYes = 1; break;                                    /* isDefault */
		case 8: bYes = 0; break;                                    /* isDynamic */
		case 9: bYes = (pAttr->iFlags
			& (PH7_CLASS_ATTR_HOOK_VIRTUAL|PH7_CLASS_ATTR_NATIVE_VIRTUAL)) != 0; break;
		/* isFinal: the DECLARED `final` (PHP 8.4) or the one private(set) implies
		 * -- php answers true for both, the same pair its modifier mask carries. */
		case 10: bYes = (pAttr->iFlags
			& (PH7_CLASS_ATTR_FINAL|PH7_CLASS_ATTR_PRIVATE_SET)) != 0; break;
		default:  /* 11: hasHooks */
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
REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isFinal, 10)
REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isDefault, 7)
REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isDynamic, 8)
REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_isVirtual, 9)
REFLECT_PROP_FLAG(vm_builtin_ReflectionProperty_hasHooks, 11)

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
	{
		char zType[192];
		const char *zT = ReflectMemberTypeText(pCtx->pVm,sRef.pAttr,zType,sizeof(zType));
		return ReflectResultObject(pCtx, ReflectMakeType(pCtx, zT, (int)SyStrlen(zT)));
	}
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
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) ){
		sRef.pAttr = 0;
	}
	if( sRef.pAttr && sRef.pAttr->pNativeValue && SySetUsed(&sRef.pAttr->aByteCode) < 1 ){
		/* A NATIVE property's default is a LITERAL, not byte-code — the same
		 * record `new` materializes (PH7_NativeLiteralValue). Reading only the
		 * byte-code answered NULL for every declared native default and raised
		 * php's no-default deprecation on a slot that hasDefaultValue() had just
		 * reported true for. */
		PH7_MemObjInit(pCtx->pVm, &sValue);
		PH7_NativeLiteralValue(pCtx->pVm, sRef.pAttr->pNativeValue, &sValue);
		ph7_result_value(pCtx, &sValue);
		PH7_MemObjRelease(&sValue);
		return PH7_OK;
	}
	if( sRef.pAttr == 0 || SySetUsed(&sRef.pAttr->aByteCode) < 1 ){
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
	pVmAttr = ReflectRefInstanceAttr(pCtx->pVm, pObj, &sRef);
	if( pVmAttr == 0 ){
		/* No slot: a class whose properties are its own handlers answers here,
		 * as php's does -- Reflection reads a PDORow's `queryString` through
		 * read_property exactly as `$row->queryString` does. */
		PH7_NativePropCtx sNat;
		SyString sNatName;
		ph7_value sNatVal;
		SyStringInitFromBuf(&sNatName, sRef.zName, (sxu32)sRef.nName);
		PH7_MemObjInit(pCtx->pVm, &sNatVal);
		if( PH7_ClassNativePropAsk(pObj, &sNat, PH7_NATIVE_PROP_READ, &sNatName, &sNatVal) ){
			if( sNat.zThrowClass ){
				PH7_MemObjRelease(&sNatVal);
				return PH7_VmThrowExceptionCode(pCtx, sNat.zThrowClass, sNat.iThrowCode,
					"%s", sNat.zThrowMsg);
			}
			ph7_result_value(pCtx, &sNatVal);
			PH7_MemObjRelease(&sNatVal);
			return PH7_OK;
		}
		PH7_MemObjRelease(&sNatVal);
		if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){
			/* A VIRTUAL property: php reads it through the same handler `$o->p`
			 * reaches, so Reflection answers the VALUE rather than warning that a
			 * name the class declares is undefined. A class whose handlers are its
			 * magic trio (ext/dom) is read through __get, which is the door the
			 * member opcode takes for the very same name. */
			ph7_value sMagic;
			SyString sMagicName;
			SyStringInitFromBuf(&sMagicName, sRef.zName, (sxu32)sRef.nName);
			PH7_MemObjInit(pCtx->pVm, &sMagic);
			if( PH7_ClassInstanceCallMagicMethod(pCtx->pVm, pObj->pClass, pObj,
					"__get", sizeof("__get")-1, &sMagicName, &sMagic) == SXRET_OK ){
				ph7_result_value(pCtx, &sMagic);
				PH7_MemObjRelease(&sMagic);
				return PH7_OK;
			}
			PH7_MemObjRelease(&sMagic);
			ph7_result_null(pCtx);
			return PH7_OK;
		}
		if( sRef.pAttr
		 && (sRef.pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT
		                           |PH7_CLASS_ATTR_HIDDEN)) == 0 ){
			/* A DECLARED property the object no longer holds -- unset() took it.
			 * php reads it the way `$o->p` does, and warns exactly the same:
			 * `Undefined property: C::$p`, naming the OBJECT's class. */
			VmErrorFormat(pCtx->pVm, PH7_CTX_WARNING, "Undefined property: %z::$%.*s",
				&pObj->pClass->sDisp, sRef.nName, sRef.zName);
		}
		ph7_result_null(pCtx);
		return PH7_OK;
	}
	if( pVmAttr->iState & VM_CLASS_ATTR_UNINIT ){
		ph7_class *pDecl = PH7_VmMemberOwnerClass(
			pVmAttr->pAttr ? pVmAttr->pAttr->pDeclClass : 0,pObj->pClass);
		return PH7_VmThrowException(pCtx, "Error",
			"Typed property %z::$%.*s must not be accessed before initialization",
			&pDecl->sDisp, sRef.nName, sRef.zName);
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
	pVmAttr = ReflectRefInstanceAttr(pCtx->pVm, pObj, &sRef);
	if( pVmAttr == 0 ){
		/* No slot: the write handler answers, and for a class that has one the
		 * answer is its own refusal (php's PDORow refuses Reflection's write
		 * with the same sentence `$row->p = 1` takes). */
		PH7_NativePropCtx sNat;
		SyString sNatName;
		ph7_value sNatVal;
		SyStringInitFromBuf(&sNatName, sRef.zName, (sxu32)sRef.nName);
		PH7_MemObjInit(pCtx->pVm, &sNatVal);
		if( PH7_ClassNativePropAsk(pObj, &sNat, PH7_NATIVE_PROP_WRITE, &sNatName, &sNatVal)
		 && sNat.zThrowClass ){
			PH7_MemObjRelease(&sNatVal);
			return PH7_VmThrowExceptionCode(pCtx, sNat.zThrowClass, sNat.iThrowCode,
				"%s", sNat.zThrowMsg);
		}
		PH7_MemObjRelease(&sNatVal);
		if( sRef.pAttr && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){
			/* A VIRTUAL property: php's write goes to the same handler `$o->p = v`
			 * reaches -- the class's own write_property, which refuses the read-only
			 * half of the surface with its own sentence. Creating a slot here would
			 * give the object a real property php has none of. */
			ph7_value sMagicVal, *pMagicArg = nArg > 1 ? apArg[1] : 0;
			ph7_class_method *pSet = PH7_ClassExtractMethod(pObj->pClass,
				"__set", sizeof("__set")-1);
			PH7_MemObjInit(pCtx->pVm, &sMagicVal);
			if( pMagicArg == 0 ){
				pMagicArg = &sMagicVal;
			}
			if( PH7_ClassNativePropOwns(pObj, &sNatName) ){
				PH7_NativePropCtx sStore;
				sxi32 rcSt = PH7_OK;
				if( PH7_ClassNativePropAsk(pObj, &sStore, PH7_NATIVE_PROP_STORE,
						&sNatName, pMagicArg)
				 && sStore.zThrowClass ){
					rcSt = PH7_VmThrowExceptionCode(pCtx, sStore.zThrowClass,
						sStore.iThrowCode, "%s", sStore.zThrowMsg);
				}
				PH7_MemObjRelease(&sMagicVal);
				return rcSt;
			}
			if( pSet ){
				ph7_value sMagicName, *apMagic[2];
				PH7_MemObjInitFromString(pCtx->pVm, &sMagicName, 0);
				PH7_MemObjStringAppend(&sMagicName, sRef.zName, (sxu32)sRef.nName);
				sMagicName.nIdx = SXU32_HIGH;
				apMagic[0] = &sMagicName;
				apMagic[1] = pMagicArg;
				rc = PH7_VmCallMagicMethod(pCtx->pVm, pObj, pSet, 0, 2, apMagic);
				PH7_MemObjRelease(&sMagicName);
				PH7_MemObjRelease(&sMagicVal);
				return rc == PH7_ABORT ? PH7_ABORT : PH7_OK;
			}
			PH7_MemObjRelease(&sMagicVal);
			return PH7_OK;
		}
		if( sRef.pAttr
		 && (sRef.pAttr->iFlags & (PH7_CLASS_ATTR_STATIC|PH7_CLASS_ATTR_CONSTANT
		                           |PH7_CLASS_ATTR_HIDDEN)) == 0 ){
			/* A DECLARED property unset() took away: php's write RE-CREATES it,
			 * exactly as `$o->p = v` does. PHL wrote nowhere and said nothing. */
			VmRecreateDeclaredAttr(pCtx->pVm, pObj, sRef.pAttr, &pVmAttr);
		}
		if( pVmAttr == 0 ){
			return PH7_OK;
		}
	}
	if( pVmAttr->pAttr && (pVmAttr->iState & VM_CLASS_ATTR_RDONLY) ){
		/* php's read-only handler refuses Reflection's write with the sentence
		 * `$stmt->queryString = 'x'` takes. */
		return VmThrowNativeReadOnly(pCtx->pVm, pVmAttr->pAttr);
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
		pSlot = (ph7_value *)PH7_MemObjAt(&pCtx->pVm->aMemObj, pVmAttr->nIdx);
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
	pVmAttr = ReflectRefInstanceAttr(pCtx->pVm, pObj, &sRef);
	if( pVmAttr == 0 && sRef.pAttr
	 && (sRef.pAttr->iFlags & PH7_CLASS_ATTR_NATIVE_NOSLOT) != 0 ){
		/* A VIRTUAL property has no slot to be uninitialized: php asks the
		 * has_property handler, and ext/dom's answers yes for every name it
		 * declares. */
		ph7_result_bool(pCtx, 1);
		return PH7_OK;
	}
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
	return ReflectExportPropSelf(pCtx);
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
			"Constant %z::%.*s does not exist", &pClass->sDisp, nConst, zConst);
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
	{
		char zType[192];
		const char *zT = ReflectMemberTypeText(pCtx->pVm,sRef.pAttr,zType,sizeof(zType));
		return ReflectResultObject(pCtx, ReflectMakeType(pCtx, zT, (int)SyStrlen(zT)));
	}
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
	return ReflectExportConstSelf(pCtx);
}
/*
 * Declare chunk 3. Called from PH7_VmInstallReflection() where it used to be
 * compiled — after chunks 1 and 2, whose ReflectionClass and ReflectionMethod
 * these answer, and after PropertyHookType, which hasHook()/getHook() declare.
 *
 * The method tables are in php's own DECLARATION order.
 */
PH7_PRIVATE sxi32 PH7_VmInstallReflectionMember(ph7_vm *pVm)
{
	static const PH7_NativePropDef aMemberProp[] = {
		{ "name",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
	};
	static const PH7_NativePropDef aPropProp[] = {
		{ "name",  PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
		{ "class", PH7_MOD_PUBLIC, { 0, 0, PH7_NATIVE_VAL_NONE, 0, 0, 0.0 }, "string" },
		/* PHL-only: the instance a DYNAMIC property was reached through (recorded) */
		{ RP_DYNOBJ, PH7_MOD_PROTECTED|PH7_MOD_HIDDEN, { 0, 0, PH7_NATIVE_VAL_NULL, 0, 0, 0.0 }, 0 },
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
		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionProperty_getName },
		{ "getMangledName", PH7_MOD_PUBLIC, "", "string",
		  vm_builtin_ReflectionProperty_getMangledName },
		{ "getValue",    PH7_MOD_PUBLIC, "?object $object = null", "@mixed",
		  vm_builtin_ReflectionProperty_getValue },
		{ "setValue",    PH7_MOD_PUBLIC, "mixed $objectOrValue, mixed $value = ?", "@void",
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
		{ "isInitialized", PH7_MOD_PUBLIC, "?object $object = null", "@bool",
		  vm_builtin_ReflectionProperty_isInitialized },
		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isPublic },
		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isPrivate },
		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isProtected },
		{ "isPrivateSet", PH7_MOD_PUBLIC, "", "bool",
		  vm_builtin_ReflectionProperty_isPrivateSet },
		{ "isProtectedSet", PH7_MOD_PUBLIC, "", "bool",
		  vm_builtin_ReflectionProperty_isProtectedSet },
		{ "isStatic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isStatic },
		{ "isReadOnly",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isReadOnly },
		{ "isDefault",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_isDefault },
		{ "isDynamic",   PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isDynamic },
		/* PHL has no abstract properties: the modifier only exists on an
		 * interface's hooked property stub, which PHL does not model. */
		{ "isAbstract",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_false },
		{ "isVirtual",   PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isVirtual },
		{ "isPromoted",  PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isPromoted },
		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int", vm_builtin_ReflectionProperty_getModifiers },
		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",
		  vm_builtin_ReflectionProperty_getDeclaringClass },
		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string|false", vm_builtin_ReflectionProperty_getDocComment },
		{ "setAccessible", PH7_MOD_PUBLIC, "bool $accessible", "@void",
		  vm_builtin_ReflectionProperty_setAccessible },
		{ "getType",     PH7_MOD_PUBLIC, "", "@?ReflectionType", vm_builtin_ReflectionProperty_getType },
		/* php's settable type differs from the declared one only for a hooked
		 * property with a widening `set` — which PHL does not model. */
		{ "getSettableType", PH7_MOD_PUBLIC, "", "?ReflectionType",
		  vm_builtin_ReflectionProperty_getType },
		{ "hasType",     PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionProperty_hasType },
		{ "hasDefaultValue", PH7_MOD_PUBLIC, "", "bool",
		  vm_builtin_ReflectionProperty_hasDefaultValue },
		{ "getDefaultValue", PH7_MOD_PUBLIC, "", "@mixed",
		  vm_builtin_ReflectionProperty_getDefaultValue },
		{ "getAttributes", PH7_MOD_PUBLIC, "?string $name = null, int $flags = 0", "array",
		  vm_builtin_ReflectionProperty_getAttributes },
		{ "hasHooks",    PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_hasHooks },
		{ "getHooks",    PH7_MOD_PUBLIC, "", "array", vm_builtin_ReflectionProperty_getHooks },
		{ "hasHook",     PH7_MOD_PUBLIC, "PropertyHookType $type", "bool",
		  vm_builtin_ReflectionProperty_hasHook },
		{ "getHook",     PH7_MOD_PUBLIC, "PropertyHookType $type", "?ReflectionMethod",
		  vm_builtin_ReflectionProperty_getHook },
		{ "isFinal",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionProperty_isFinal },
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
		{ "getName",     PH7_MOD_PUBLIC, "", "@string", vm_builtin_ReflectionProperty_getName },
		{ "getValue",    PH7_MOD_PUBLIC, "", "@mixed", vm_builtin_ReflectionClassConstant_getValue },
		{ "isPublic",    PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClassConstant_isPublic },
		{ "isPrivate",   PH7_MOD_PUBLIC, "", "@bool", vm_builtin_ReflectionClassConstant_isPrivate },
		{ "isProtected", PH7_MOD_PUBLIC, "", "@bool",
		  vm_builtin_ReflectionClassConstant_isProtected },
		{ "isFinal",     PH7_MOD_PUBLIC, "", "bool", vm_builtin_ReflectionClassConstant_isFinal },
		{ "getModifiers", PH7_MOD_PUBLIC, "", "@int",
		  vm_builtin_ReflectionClassConstant_getModifiers },
		{ "getDeclaringClass", PH7_MOD_PUBLIC, "", "@ReflectionClass",
		  vm_builtin_ReflectionClassConstant_getDeclaringClass },
		{ "getDocComment", PH7_MOD_PUBLIC, "", "@string|false",
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
		  aPropProp, SX_ARRAYSIZE(aPropProp), 0, 0, 0 },
		{ "ReflectionClassConstant", 0, "Reflector", PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aConstMethod, SX_ARRAYSIZE(aConstMethod),
		  aConstConst, SX_ARRAYSIZE(aConstConst),
		  aMemberProp, SX_ARRAYSIZE(aMemberProp), 0, 0, 0 },
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
			"Class \"%z\" is not an enum", &pClass->sDisp);
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
				"%z::%.*s is not a case", &pClass->sDisp, nName, zName);
		}
		return PH7_VmThrowException(pCtx, "ReflectionException",
			"Case %z::%.*s does not exist", &pClass->sDisp, nName, zName);
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
			"Constant %z::%.*s is not a case", &sRef.pClass->sDisp, sRef.nName, sRef.zName);
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
			"Enum case %z::%.*s is not a backed case", &sRef.pClass->sDisp,
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
 * Declare the three. Called from PH7_VmInstallReflection() where chunk 6 used
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
		  aEnumMethod, SX_ARRAYSIZE(aEnumMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "ReflectionEnumUnitCase", "ReflectionClassConstant", 0,
		  PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aCaseMethod, SX_ARRAYSIZE(aCaseMethod), 0, 0, 0, 0, 0, 0, 0 },
		{ "ReflectionEnumBackedCase", "ReflectionEnumUnitCase", 0,
		  PH7_CLASS_NOCLONE|PH7_CLASS_NOSERIALIZE,
		  aBackedMethod, SX_ARRAYSIZE(aBackedMethod), 0, 0, 0, 0, 0, 0, 0 },
	};
	return PH7_InstallNativeClasses(&(*pVm), aSpec, SX_ARRAYSIZE(aSpec));
}
/*
 * ---------------------------------------------------------------------------
 * php's Reflection EXPORT format — chunk 9, the last of the library's PHP.
 *
 * Every Reflector's __toString(). It is a byte-exact format with no API of its
 * own, so the chunk was written against the PUBLIC reflection API of whatever
 * it was printing — the only thing PHP could reach. All of those targets are C
 * now, so this reads the engine directly: the member walk for order and
 * visibility, ReflectParamAt for parameters, ReflectExportValue (built for the
 * attribute-argument block) for every value.
 *
 * Defined at the END of the file because it needs all of that; the five entry
 * points are forward-declared above their callers.
 * ---------------------------------------------------------------------------
 */

/*
 * "internal:<extension>" / "user" — the first tag of every function and class
 * head. php NAMES the extension there (`<internal:json>`), and this printed
 * `internal:Core` for all 878 functions and 197 classes because the engine had
 * no partition to name one from. iExt < 0 is a userland target.
 */
static void ReflectExportKind(SyBlob *pOut, int bInternal, int iExt, int bDeprecated)
{
	/* The two questions are separate: an INTERNAL target may still name no module
	 * (php's fabricated `Closure::__invoke` prints `<internal>` with nothing after
	 * it), so the extension is only appended when there is one. */
	SyBlobAppend(pOut, bInternal ? "internal" : "user", bInternal ? 8 : 4);
	/* php writes the three parts in this order and each on its own condition,
	 * so a deprecated INTERNAL name reads `<internal, deprecated:curl>` -- the
	 * extension hangs off `deprecated`, not off `internal`. */
	if( bDeprecated ){
		SyBlobAppend(pOut, ", deprecated", sizeof(", deprecated")-1);
	}
	if( bInternal && iExt >= 0 ){
		SyBlobFormat(pOut, ":%s", PH7_VmExtensionName(iExt));
	}
}
/*
 * A declared type followed by a space, or nothing at all.
 *
 * php's EXPORT spells a standalone `iterable` as the two types it stands for
 * where getType() prints `iterable` -- the one place the two renderings of a
 * declared type differ (ReflectExportIterable is that difference, named once).
 */
static const SyString * ReflectExportIterable(const SyString *pType, SyString *pOut)
{
	const char *z = pType ? SyStringData(pType) : 0;
	sxu32 n = z ? SyStringLength(pType) : 0;
	if( n == 8 && SyStrnicmp(z,"iterable",8) == 0 ){
		SyStringInitFromBuf(pOut,"Traversable|array",sizeof("Traversable|array")-1);
		return pOut;
	}
	if( n == 9 && z[0] == '?' && SyStrnicmp(&z[1],"iterable",8) == 0 ){
		SyStringInitFromBuf(pOut,"Traversable|array|null",sizeof("Traversable|array|null")-1);
		return pOut;
	}
	return pType;
}
static void ReflectExportTypeSp(SyBlob *pOut, const SyString *pType)
{
	SyString sIter;
	pType = ReflectExportIterable(pType,&sIter);
	if( pType && SyStringLength(pType) > 0 ){
		SyBlobAppend(pOut, SyStringData(pType), SyStringLength(pType));
		SyBlobAppend(pOut, " ", sizeof(char));
	}
}
/* php's visibility word for a member. */
static const char * ReflectExportVis(sxi32 iProtection)
{
	if( iProtection == PH7_CLASS_PROT_PRIVATE ){
		return "private";
	}
	return iProtection == PH7_CLASS_PROT_PROTECTED ? "protected" : "public";
}
/*
 * The text after `= ` in a parameter default.
 *
 * php prints a constant-reference default as the CONSTANT's name, not its value
 * (`$f = M_PI`) — that argument was never folded, so php still has the source
 * expression. `<default>` is what the chunk answered when the value could not
 * be produced at all, which is a declared `= ?` row in aBuiltinSig[].
 */
static void ReflectExportDefault(ph7_context *pCtx, SyBlob *pOut, ReflectParamDesc *pDesc)
{
	const char *z = 0;
	int n = 0;
	if( ReflectParamDefConst(pCtx, pDesc, &z, &n) ){
		SyBlobAppend(pOut, z, (sxu32)n);
		return;
	}
	if( pDesc->pArg ){
		ph7_value sValue;
		PH7_MemObjInit(pCtx->pVm, &sValue);
		VmLocalExec(pCtx->pVm, &pDesc->pArg->aByteCode, &sValue, FALSE);
		/* php has two spellings for the same default and picks by whether the
		 * function is INTERNAL: `null` and double quotes there, `NULL` and single
		 * quotes for a userland one. A builtin written as prelude PHP is internal
		 * to php however this engine chose to implement it, so scandir()'s
		 * `$context = NULL` and clearstatcache()'s `$filename = ''` were both
		 * printed in the wrong one. */
		if( pDesc->bInternal
		 && (sValue.iFlags & (MEMOBJ_STRING|MEMOBJ_NULL)) == MEMOBJ_STRING ){
			ReflectExportStrQ(pOut, (const char *)SyBlobData(&sValue.sBlob),
				SyBlobLength(&sValue.sBlob), '"');
		}else if( pDesc->bInternal && (sValue.iFlags & MEMOBJ_NULL) ){
			SyBlobAppend(pOut, "null", sizeof("null")-1);
		}else{
			ReflectExportValue(pCtx, pOut, &sValue, 0);
		}
		PH7_MemObjRelease(&sValue);
		return;
	}
	{
		/* A DECLARED default is signature TEXT: reduce it the way
		 * getDefaultValue() does, and fall back to php's own placeholder.
		 * Only an INTERNAL parameter reaches this branch (a compiled one carries
		 * pArg above), which is exactly the case php prints DOUBLE-quoted. */
		const char *zDef = SyStringData(&pDesc->sDefText);
		int nDef = (int)SyStringLength(&pDesc->sDefText);
		ph7_value *pVal = ph7_context_new_scalar(pCtx);
		if( ReflectSigHas(zDef, nDef, "::", 2) ){
			/* Rule 28's split, on the declared side: php's stub keeps the SOURCE of a
			 * constant-expression default, so the export prints
			 * `= SplFileInfo::class` and `= A::K | A::J` verbatim while
			 * getDefaultValue() answers what they evaluate to. */
			SyBlobAppend(pOut, zDef, (sxu32)nDef);
			return;
		}
		if( pVal && ReflectSigGlobalConst(pCtx, zDef, nDef, pVal) ){
			/* Same split for a GLOBAL constant: `= E_ERROR`, never `= 1`. */
			SyBlobAppend(pOut, zDef, (sxu32)nDef);
			return;
		}
		if( pVal && ReflectSigHas(zDef, nDef, "|", 1)
		 && ReflectSigConstExpr(pCtx, zDef, nDef, pVal) ){
			/* And for the `|` fold of them, which is the one shape whose VALUE
			 * names no constant at all: `SQLITE3_OPEN_READWRITE |
			 * SQLITE3_OPEN_CREATE` prints as itself and answers 6. */
			SyBlobAppend(pOut, zDef, (sxu32)nDef);
			return;
		}
		if( ReflectSigIsSourceExpr(zDef, nDef) ){
			/* The two arithmetic shapes: a RADIX integer and a product. php prints
			 * `0777` and `2 * 1024 * 1024`, never the 511 and 2097152 they reduce
			 * to -- and answers those numbers from getDefaultValue() all the same. */
			SyBlobAppend(pOut, zDef, (sxu32)nDef);
			return;
		}
		if( pVal && ReflectSigScalar(pCtx, zDef, nDef, pVal) ){
			if( (pVal->iFlags & (MEMOBJ_STRING|MEMOBJ_NULL)) == MEMOBJ_STRING ){
				ReflectExportStrQ(pOut, (const char *)SyBlobData(&pVal->sBlob),
					SyBlobLength(&pVal->sBlob), '"');
				return;
			}
			if( pVal->iFlags & MEMOBJ_NULL ){
				/* The same internal/user split as the quote character above: php
				 * spells an INTERNAL parameter's null default `null` and a
				 * userland one `NULL` (`= null` for every `?T $x = null` stub row). */
				SyBlobAppend(pOut, "null", sizeof("null")-1);
				return;
			}
			ReflectExportValue(pCtx, pOut, pVal, 0);
			return;
		}
		if( (nDef >= 1 && zDef[0] == '[')
		 || (nDef >= (int)sizeof("array (")-1
		  && SyMemcmp(zDef, "array (", sizeof("array (")-1) == 0) ){
			SyBlobAppend(pOut, "[]", sizeof("[]")-1);
			return;
		}
	}
	SyBlobAppend(pOut, "<default>", sizeof("<default>")-1);
}
/* `Parameter #0 [ <optional> int &...$name = 5 ]` */
static void ReflectExportParamLine(ph7_context *pCtx, SyBlob *pOut, ReflectParamDesc *pDesc)
{
	SyBlobFormat(pOut, "Parameter #%d [ <%s> ", pDesc->iPos,
		pDesc->bOptional ? "optional" : "required");
	ReflectExportTypeSp(pOut, &pDesc->sType);
	if( pDesc->bByRef ){
		SyBlobAppend(pOut, "&", sizeof(char));
	}
	if( pDesc->bVariadic ){
		SyBlobAppend(pOut, "...", sizeof("...")-1);
	}
	SyBlobAppend(pOut, "$", sizeof(char));
	SyBlobAppend(pOut, SyStringData(&pDesc->sName), SyStringLength(&pDesc->sName));
	if( pDesc->bHasDef ){
		SyBlobAppend(pOut, " = ", sizeof(" = ")-1);
		ReflectExportDefault(pCtx, pOut, pDesc);
	}else if( pDesc->bOptional && !pDesc->bVariadic ){
		/* An OPTIONAL parameter with no default a caller could read still gets
		 * an `= ` from php -- and php's own placeholder after it, since there is
		 * nothing to print. `mt_rand()`'s two bounds and `get_class()`'s
		 * $object are that shape; a VARIADIC tail is not, because its "default"
		 * is having no further arguments. */
		SyBlobAppend(pOut, " = <default>", sizeof(" = <default>")-1);
	}
	SyBlobAppend(pOut, " ]", sizeof(" ]")-1);
}
/*
 * `Property [ public protected(set) readonly int $x = 5 ]` + newline.
 *
 * The set-visibility is printed only when it DIFFERS from the get-visibility,
 * which is why a `public readonly` property shows php's implied
 * `protected(set)` and a `protected readonly` one shows nothing.
 */
static void ReflectExportPropLine(ph7_context *pCtx, SyBlob *pOut, ph7_class_attr *pAttr,
	const SyString *pKey)
{
	sxi32 iSet = pAttr->iProtection;
	SyBlobAppend(pOut, "Property [ ", sizeof("Property [ ")-1);
	/* php's modifier MASK implies two bits the declaration never wrote:
	 * private(set) implies final, and readonly implies protected(set). A property
	 * DECLARED final (PHP 8.4) prints the same word, and the two spellings do not
	 * print it twice. */
	if( pAttr->iFlags & (PH7_CLASS_ATTR_FINAL|PH7_CLASS_ATTR_PRIVATE_SET) ){
		SyBlobAppend(pOut, "final ", sizeof("final ")-1);
	}
	SyBlobFormat(pOut, "%s ", ReflectExportVis(pAttr->iProtection));
	if( pAttr->iFlags & PH7_CLASS_ATTR_PRIVATE_SET ){
		iSet = PH7_CLASS_PROT_PRIVATE;
	}else if( (pAttr->iFlags & PH7_CLASS_ATTR_PROTECTED_SET)
	 || (pAttr->iFlags & PH7_CLASS_ATTR_READONLY) ){
		iSet = PH7_CLASS_PROT_PROTECTED;
	}
	/* The set-visibility is printed only when it DIFFERS from the get one. */
	if( iSet != pAttr->iProtection ){
		SyBlobFormat(pOut, "%s(set) ", ReflectExportVis(iSet));
	}
	if( pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){
		SyBlobAppend(pOut, "static ", sizeof("static ")-1);
	}
	if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_VIRTUAL ){
		SyBlobAppend(pOut, "virtual ", sizeof("virtual ")-1);
	}
	if( pAttr->iFlags & PH7_CLASS_ATTR_READONLY ){
		SyBlobAppend(pOut, "readonly ", sizeof("readonly ")-1);
	}
	if( pAttr->iFlags & PH7_CLASS_ATTR_TYPED ){
		char zType[192];
		SyString sType;
		const char *zT = ReflectMemberTypeText(pCtx->pVm,pAttr,zType,sizeof(zType));
		SyStringInitFromBuf(&sType,zT,(sxu32)SyStrlen(zT));
		ReflectExportTypeSp(pOut, &sType);
	}
	SyBlobAppend(pOut, "$", sizeof(char));
	SyBlobAppend(pOut, SyStringData(pKey), SyStringLength(pKey));
	if( SySetUsed(&pAttr->aByteCode) > 0 || pAttr->pNativeValue
	 || (pAttr->iFlags & PH7_CLASS_ATTR_TYPED) == 0 ){
		ph7_value sValue;
		SyBlobAppend(pOut, " = ", sizeof(" = ")-1);
		PH7_MemObjInit(pCtx->pVm, &sValue);
		if( SySetUsed(&pAttr->aByteCode) > 0 ){
			VmLocalExec(pCtx->pVm, &pAttr->aByteCode, &sValue, FALSE);
		}else if( pAttr->pNativeValue ){
			PH7_NativeLiteralValue(pCtx->pVm, pAttr->pNativeValue, &sValue);
		}
		ReflectExportValue(pCtx, pOut, &sValue, 0);
		PH7_MemObjRelease(&sValue);
	}
	/* A hooked property names its hooks; php lists no method for them. */
	if( pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_SET) ){
		SyBlobAppend(pOut, " {", sizeof(" {")-1);
		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_GET ){
			SyBlobAppend(pOut, " get;", sizeof(" get;")-1);
		}
		if( pAttr->iFlags & PH7_CLASS_ATTR_HOOK_SET ){
			SyBlobAppend(pOut, " set;", sizeof(" set;")-1);
		}
		SyBlobAppend(pOut, " }", sizeof(" }")-1);
	}
	SyBlobAppend(pOut, " ]\n", sizeof(" ]\n")-1);
}
/*
 * `Constant [ final public int NAME ] { value }` + newline. The TYPE is the
 * value's, not a declared one, and an object (an enum case) prints as the word
 * Object under its own class name.
 */
static sxi32 ReflectExportConstLine(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass,
	ph7_class_attr *pAttr, const SyString *pKey)
{
	ph7_value *pVal = 0;
	sxi32 rc = ReflectConstSlot(pCtx, pClass, pAttr, &pVal);
	const char *zType;
	if( rc != SXRET_OK ){
		return rc;
	}
	SyBlobAppend(pOut, "Constant [ ", sizeof("Constant [ ")-1);
	if( pAttr->iFlags & PH7_CLASS_ATTR_FINAL ){
		SyBlobAppend(pOut, "final ", sizeof("final ")-1);
	}
	SyBlobFormat(pOut, "%s ", ReflectExportVis(pAttr->iProtection));
	if( (pAttr->iFlags & PH7_CLASS_ATTR_TYPED)
	 && SyStringLength(&pAttr->sTypeName) > 0 ){
		/* php 8.3's typed class constant prints what it DECLARED; the word below
		 * is what an untyped one's VALUE happens to be. */
		char zDecl[192];
		SyString sDecl;
		const char *zT = ReflectMemberTypeText(pCtx->pVm,pAttr,zDecl,sizeof(zDecl));
		SyStringInitFromBuf(&sDecl,zT,(sxu32)SyStrlen(zT));
		ReflectExportTypeSp(pOut, &sDecl);
	}else{
		zType = ReflectExportTypeWord(pVal);
		if( zType ){
			SyBlobFormat(pOut, "%s ", zType);
		}else{
			SyBlobFormat(pOut, "%z ", &((ph7_class_instance *)pVal->x.pOther)->pClass->sName);
		}
	}
	SyBlobAppend(pOut, SyStringData(pKey), SyStringLength(pKey));
	SyBlobAppend(pOut, " ] { ", sizeof(" ] { ")-1);
	ReflectExportConstValue(pCtx, pOut, pVal);
	SyBlobAppend(pOut, " }\n", sizeof(" }\n")-1);
	return SXRET_OK;
}
/* A native class METHOD as a function reference (ReflectFuncFill's tail, for a
 * method the member walk handed over rather than one a receiver names). */
static void ReflectFuncFromMethod(ph7_vm *pVm, ph7_class *pClass, ph7_class_method *pMeth,
	ReflectFuncRef *pOut)
{
	SyZero(pOut, sizeof(*pOut));
	pOut->pVm = pVm;
	pOut->pClass = pClass;
	pOut->pMeth = pMeth;
	pOut->pFunc = &pMeth->sFunc;
	if( (pOut->pFunc->iFlags & VM_FUNC_NATIVE) && pOut->pFunc->pNative ){
		pOut->zSig = pOut->pFunc->pNative->zSig;
		if( SyStringLength(&pOut->pFunc->sReturnTypeName) == 0 ){
			pOut->zRet = pOut->pFunc->pNative->zRet;
		}
	}
}
/*
 * The Method / Function / Closure block.
 *
 * pOwner is the class being EXPORTED when there is one: php's tags are
 * relative to it — a method it did not declare "inherits" from its declaring
 * class — and the reflector alone cannot say that, because $class is the
 * DECLARING class. `overwrites` is the other direction and needs no owner: the
 * declaring class's own parent declares the same method.
 */
static sxi32 ReflectExportFuncBlock(ph7_context *pCtx, SyBlob *pOut, ReflectFuncRef *pRef,
	const char *zIndent, ph7_class *pOwner)
{
	SyBlob sBody;
	int bInternal = ReflectFuncIsInternal(pRef);
	int bDeprecated = ReflectFuncDeprecated(pRef) != 0
		|| (pRef->pFunc != 0 && ReflectHasDeprecated(&pRef->pFunc->aAttrs));
	int nParam = ReflectParamCount(pRef);
	const char *zRet = 0;
	char zRetBuf[192];
	int nRet = 0, bHasRet, bTentRet = 0;
	sxi32 rc = SXRET_OK;
	SyBlobInit(&sBody, &pCtx->pVm->sAllocator);
	bHasRet = ReflectFuncRetTextEx(pRef, &zRet, &nRet, &bTentRet, zRetBuf, sizeof(zRetBuf));
	if( pRef->pMeth ){
		ph7_class *pDecl = ReflectFuncDeclClass(pRef);
		ph7_class *pProto;
		SyString *pName = &pRef->pMeth->sFunc.sName;
		int bInherits, bCtor;
		SyBlobAppend(&sBody, "Method [ <", sizeof("Method [ <")-1);
		/* A method names its DECLARING class's extension, which is why php's
		 * `JsonException::__construct` prints `<internal:Core, inherits
		 * Exception, ctor>` rather than json's own name. */
		ReflectExportKind(&sBody, bInternal, ReflectFuncExtId(pRef), bDeprecated);
		bInherits = (pOwner != 0 && pDecl != 0 && pDecl != pOwner);
		if( bInherits ){
			SyBlobFormat(&sBody, ", inherits %z", &pDecl->sName);
		}else if( pDecl ){
			ph7_class *pOver = ReflectOverwritesIn(pDecl,
				SyStringData(pName), (int)SyStringLength(pName));
			if( pOver ){
				SyBlobFormat(&sBody, ", overwrites %z", &pOver->sName);
			}
		}
		bCtor = SyStringLength(pName) == sizeof("__construct")-1
			&& SyStrnicmp(SyStringData(pName), "__construct", sizeof("__construct")-1) == 0;
		/* The prototype belongs to the entry in the class the export was
		 * reached THROUGH, which is php's anchor for it: a class that only
		 * inherits a method still re-assigns its prototype when it implements
		 * an interface declaring the same one. */
		pProto = ReflectPrototypeOf(pCtx, pOwner ? pOwner : pRef->pClass,
			SyStringData(pName), (int)SyStringLength(pName), 0);
		if( pProto ){
			SyBlobFormat(&sBody, ", prototype %z", &pProto->sName);
		}
		/* php closes the bracket with the ctor word, AFTER the prototype: an
		 * interface's constructor prints `prototype F1, ctor`. There is no `dtor`
		 * twin -- php's exporter prints the word for ZEND_ACC_CTOR only, so a
		 * `__destruct` reads `Method [ <user> public method __destruct ]`. */
		if( bCtor ){
			SyBlobAppend(&sBody, ", ctor", sizeof(", ctor")-1);
		}
		SyBlobAppend(&sBody, "> ", sizeof("> ")-1);
		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_ABSTRACT ){
			SyBlobAppend(&sBody, "abstract ", sizeof("abstract ")-1);
		}
		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_FINAL ){
			SyBlobAppend(&sBody, "final ", sizeof("final ")-1);
		}
		if( pRef->pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){
			SyBlobAppend(&sBody, "static ", sizeof("static ")-1);
		}
		SyBlobFormat(&sBody, "%s method %z ]", ReflectExportVis(pRef->pMeth->iProtection), pName);
	}else{
		SyBlobAppend(&sBody, pRef->pClosure ? "Closure [ <" : "Function [ <",
			pRef->pClosure ? sizeof("Closure [ <")-1 : sizeof("Function [ <")-1);
		ReflectExportKind(&sBody, bInternal, ReflectFuncExtId(pRef), bDeprecated);
		SyBlobAppend(&sBody, "> function ", sizeof("> function ")-1);
		if( pRef->pFunc ){
			SyBlobFormat(&sBody, "%z", &pRef->pFunc->sName);
		}else if( pRef->pHost ){
			SyBlobFormat(&sBody, "%z", &pRef->pHost->sName);
		}
		SyBlobAppend(&sBody, " ]", sizeof(" ]")-1);
	}
	SyBlobAppend(&sBody, " {\n", sizeof(" {\n")-1);
	if( !bInternal && pRef->pFunc ){
		SyBlobAppend(&sBody, "  @@ ", sizeof("  @@ ")-1);
		if( SyStringLength(&pRef->pFunc->sFile) > 0 ){
			SyBlobAppend(&sBody, SyStringData(&pRef->pFunc->sFile),
				SyStringLength(&pRef->pFunc->sFile));
		}
		SyBlobFormat(&sBody, " %u - %u\n", pRef->pFunc->nLine, pRef->pFunc->nEndLine);
	}
	/* php prints the parameter block for every INTERNAL function, and for a
	 * user one only when there is something to say -- with one exception, the
	 * class-level `Closure::__invoke`: php fabricates it with no parameter
	 * information at all (not an empty list), and prints neither section. */
	if( (nParam > 0 || bHasRet || bInternal)
	 && !(pRef->bFabricated && pRef->pClosure == 0) ){
		int n;
		SyBlobFormat(&sBody, "\n  - Parameters [%d] {\n", nParam);
		for( n = 0 ; n < nParam ; n++ ){
			ReflectParamDesc sDesc;
			if( !ReflectParamAt(pRef, n, &sDesc) ){
				continue;
			}
			SyBlobAppend(&sBody, "    ", sizeof("    ")-1);
			ReflectExportParamLine(pCtx, &sBody, &sDesc);
			SyBlobAppend(&sBody, "\n", sizeof(char));
		}
		SyBlobAppend(&sBody, "  }\n", sizeof("  }\n")-1);
	}
	if( bHasRet ){
		/* php's two spellings: `Return [ T ]` for a declared type, `Tentative return
		 * [ T ]` for a stub's @tentative-return-type. */
		if( bTentRet ){
			SyBlobAppend(&sBody, "  - Tentative return [ ", sizeof("  - Tentative return [ ")-1);
		}else{
			SyBlobAppend(&sBody, "  - Return [ ", sizeof("  - Return [ ")-1);
		}
		{
			/* ...and the export's own spelling of a standalone `iterable`. */
			SyString sRet, sIter;
			const SyString *pRet;
			SyStringInitFromBuf(&sRet,zRet,(sxu32)nRet);
			pRet = ReflectExportIterable(&sRet,&sIter);
			SyBlobAppend(&sBody, SyStringData(pRet), SyStringLength(pRet));
		}
		SyBlobAppend(&sBody, " ]\n", sizeof(" ]\n")-1);
	}
	SyBlobAppend(&sBody, "}\n", sizeof("}\n")-1);
	/* Indent every non-empty line, the way the chunk's explode/implode did. */
	if( zIndent == 0 || zIndent[0] == '\0' ){
		SyBlobAppend(pOut, SyBlobData(&sBody), SyBlobLength(&sBody));
	}else{
		const char *z = (const char *)SyBlobData(&sBody);
		sxu32 n = SyBlobLength(&sBody), i = 0, iStart = 0;
		sxu32 nIndent = (sxu32)SyStrlen(zIndent);
		for( i = 0 ; i < n ; i++ ){
			if( z[i] != '\n' ){
				continue;
			}
			if( i > iStart ){
				SyBlobAppend(pOut, zIndent, nIndent);
				SyBlobAppend(pOut, &z[iStart], i - iStart);
			}
			SyBlobAppend(pOut, "\n", sizeof(char));
			iStart = i + 1;
		}
	}
	SyBlobRelease(&sBody);
	return rc;
}
/* The `- Enum cases [N]` block php prints where an enum's cases would otherwise
 * be listed among its constants. A backed case shows its value UNQUOTED. */
static sxi32 ReflectExportEnumCases(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass)
{
	ph7_class_attr **apCase = (ph7_class_attr **)SySetBasePtr(&pClass->aEnumCases);
	sxu32 n;
	SyBlobFormat(pOut, "\n  - Enum cases [%u] {\n", SySetUsed(&pClass->aEnumCases));
	for( n = 0 ; n < SySetUsed(&pClass->aEnumCases) ; n++ ){
		SyBlobAppend(pOut, "    Case ", sizeof("    Case ")-1);
		SyBlobAppend(pOut, SyStringData(&apCase[n]->sName), SyStringLength(&apCase[n]->sName));
		if( pClass->nEnumBacking ){
			ph7_value *pVal = 0;
			ph7_class_instance *pObj;
			sxi32 rc = ReflectConstSlot(pCtx, pClass, apCase[n], &pVal);
			if( rc != SXRET_OK ){
				return rc;
			}
			pObj = (pVal && (pVal->iFlags & MEMOBJ_OBJ)) ? (ph7_class_instance *)pVal->x.pOther : 0;
			if( pObj ){
				ph7_value *pBacking = PH7_NativeAttr(pObj, "value");
				if( pBacking ){
					ph7_value sTmp;
					const char *zText;
					int nText;
					PH7_MemObjInit(pCtx->pVm, &sTmp);
					PH7_MemObjStore(pBacking, &sTmp);
					zText = ph7_value_to_string(&sTmp, &nText);
					SyBlobAppend(pOut, " = ", sizeof(" = ")-1);
					if( nText > 0 ){
						SyBlobAppend(pOut, zText, (sxu32)nText);
					}
					PH7_MemObjRelease(&sTmp);
				}
			}
		}
		SyBlobAppend(pOut, "\n", sizeof(char));
	}
	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);
	return SXRET_OK;
}
/* The Class / Interface / Enum block. */
static sxi32 ReflectExportClassBlock(ph7_context *pCtx, SyBlob *pOut, ph7_class *pClass)
{
	ph7_vm *pVm = pCtx->pVm;
	int bInternal = (pClass->iFlags & PH7_CLASS_INTERNAL) != 0;
	int bEnum = (pClass->iFlags & PH7_CLASS_ENUM) != 0;
	int bIface = (pClass->iFlags & PH7_CLASS_INTERFACE) != 0;
	SySet aMembers, aIface;
	sxu32 n, nConst = 0, nStaticProp = 0, nProp = 0, nStaticMeth = 0, nMeth = 0;
	sxi32 rc = SXRET_OK;
	int iPass;
	SySetInit(&aMembers, &pVm->sAllocator, sizeof(ReflectMember));
	SySetInit(&aIface, &pVm->sAllocator, sizeof(ph7_class *));
	ReflectMembers(pVm, pClass, &aMembers, 0);
	PH7_ReflectInterfacesOf(pVm, pClass, &aIface);
	/* ---- head ---- */
	if( bIface ){
		SyBlobAppend(pOut, "Interface [ <", sizeof("Interface [ <")-1);
	}else if( bEnum ){
		SyBlobAppend(pOut, "Enum [ <", sizeof("Enum [ <")-1);
	}else{
		SyBlobAppend(pOut, "Class [ <", sizeof("Class [ <")-1);
	}
	{
		int iClassExt = ReflectClassExtId(pClass);
		ReflectExportKind(pOut, iClassExt >= 0, iClassExt, 0);
	}
	SyBlobAppend(pOut, "> ", sizeof("> ")-1);
	/* php tags a class that has an iteration handler, which its Traversable
	 * implementers have — and so does anything with a HOOKED property, because
	 * that is how php 8.4 walks one. An INTERFACE never carries one: php
	 * installs the handler when a CLASS implements Traversable, so
	 * `interface I extends Iterator` prints no tag. */
	{
		int bIterable = !bIface && pVm->pTraversableClass != 0
			&& PH7_VmInstanceOf(pClass, pVm->pTraversableClass);
		for( n = 0 ; !bIterable && n < SySetUsed(&aMembers) ; n++ ){
			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);
			if( pM->iKind == REFLECT_MEMBER_PROP && pM->pAttr
			 && (pM->pAttr->iFlags & (PH7_CLASS_ATTR_HOOK_GET|PH7_CLASS_ATTR_HOOK_SET)) ){
				bIterable = 1;
			}
		}
		if( bIterable ){
			SyBlobAppend(pOut, "<iterateable> ", sizeof("<iterateable> ")-1);
		}
	}
	if( bIface ){
		SyBlobFormat(pOut, "interface %z", &pClass->sName);
	}else if( bEnum ){
		SyBlobFormat(pOut, "enum %z", &pClass->sName);
		if( pClass->nEnumBacking ){
			SyBlobFormat(pOut, ": %s", (pClass->nEnumBacking & MEMOBJ_INT) ? "int" : "string");
		}
	}else{
		if( pClass->iFlags & PH7_CLASS_ABSTRACT ){
			SyBlobAppend(pOut, "abstract ", sizeof("abstract ")-1);
		}
		if( pClass->iFlags & PH7_CLASS_FINAL ){
			SyBlobAppend(pOut, "final ", sizeof("final ")-1);
		}
		if( pClass->iFlags & PH7_CLASS_READONLY ){
			SyBlobAppend(pOut, "readonly ", sizeof("readonly ")-1);
		}
		SyBlobFormat(pOut, "class %z", &pClass->sName);
		if( pClass->pBase ){
			SyBlobFormat(pOut, " extends %z", &pClass->pBase->sName);
		}
	}
	if( SySetUsed(&aIface) > 0 ){
		ph7_class **apIface = (ph7_class **)SySetBasePtr(&aIface);
		/* An interface EXTENDS what a class implements. */
		SyBlobAppend(pOut, bIface ? " extends " : " implements ",
			bIface ? sizeof(" extends ")-1 : sizeof(" implements ")-1);
		for( n = 0 ; n < SySetUsed(&aIface) ; n++ ){
			if( n > 0 ){
				SyBlobAppend(pOut, ", ", sizeof(", ")-1);
			}
			SyBlobFormat(pOut, "%z", &apIface[n]->sName);
		}
	}
	SyBlobAppend(pOut, " ] {\n", sizeof(" ] {\n")-1);
	if( !bInternal && SyStringLength(&pClass->sFile) > 0 ){
		SyBlobAppend(pOut, "  @@ ", sizeof("  @@ ")-1);
		SyBlobAppend(pOut, SyStringData(&pClass->sFile), SyStringLength(&pClass->sFile));
		SyBlobFormat(pOut, " %u-%u\n", pClass->nLine, pClass->nEndLine);
	}
	/* ---- counts ---- */
	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){
		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);
		if( pM->iKind == REFLECT_MEMBER_CONST ){
			if( !(bEnum && (pM->pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)) ){
				nConst++;
			}
		}else if( pM->iKind == REFLECT_MEMBER_PROP ){
			if( pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC ){ nStaticProp++; }else{ nProp++; }
		}else{
			/* A FABRICATED method is absent from the class's own export, for the same
			 * reason get_class_methods() does not name it: php is printing its
			 * function table and this one is not in it. */
			if( pM->pMeth->iFlags & PH7_CLASS_ATTR_FABRICATED ){ continue; }
			if( pM->pMeth->iFlags & PH7_CLASS_ATTR_STATIC ){ nStaticMeth++; }else{ nMeth++; }
		}
	}
	/* ---- enum cases, then constants ---- */
	if( bEnum ){
		rc = ReflectExportEnumCases(pCtx, pOut, pClass);
		if( rc != SXRET_OK ){
			goto done;
		}
	}
	SyBlobFormat(pOut, "\n  - Constants [%u] {\n", nConst);
	for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){
		ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);
		if( pM->iKind != REFLECT_MEMBER_CONST
		 || (bEnum && (pM->pAttr->iFlags & PH7_CLASS_ATTR_ENUMCASE)) ){
			continue;
		}
		SyBlobAppend(pOut, "    ", sizeof("    ")-1);
		rc = ReflectExportConstLine(pCtx, pOut, pClass, pM->pAttr, &pM->sKey);
		if( rc != SXRET_OK ){
			goto done;
		}
	}
	SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);
	/* ---- properties and methods, statics first ---- */
	for( iPass = 0 ; iPass < 4 ; iPass++ ){
		int bStatic = (iPass == 0 || iPass == 1);
		int bMethods = (iPass == 1 || iPass == 3);
		int bFirst = 1;
		static const char *azTitle[] = {
			"\n  - Static properties [%u] {\n", "\n  - Static methods [%u] {\n",
			"\n  - Properties [%u] {\n", "\n  - Methods [%u] {\n"
		};
		sxu32 aCount[4];
		aCount[0] = nStaticProp;
		aCount[1] = nStaticMeth;
		aCount[2] = nProp;
		aCount[3] = nMeth;
		SyBlobFormat(pOut, azTitle[iPass], aCount[iPass]);
		for( n = 0 ; n < SySetUsed(&aMembers) ; n++ ){
			ReflectMember *pM = (ReflectMember *)SySetAt(&aMembers, n);
			int bIsStatic;
			if( pM->iKind == REFLECT_MEMBER_CONST ){
				continue;
			}
			if( bMethods != (pM->iKind == REFLECT_MEMBER_METHOD) ){
				continue;
			}
			if( bMethods && (pM->pMeth->iFlags & PH7_CLASS_ATTR_FABRICATED) ){
				continue;   /* not in the function table this is printing */
			}
			bIsStatic = bMethods ? (pM->pMeth->iFlags & PH7_CLASS_ATTR_STATIC) != 0
				: (pM->pAttr->iFlags & PH7_CLASS_ATTR_STATIC) != 0;
			if( bIsStatic != bStatic ){
				continue;
			}
			if( bMethods ){
				ReflectFuncRef sRef;
				if( !bFirst ){
					SyBlobAppend(pOut, "\n", sizeof(char));
				}
				bFirst = 0;
				ReflectFuncFromMethod(pCtx->pVm, pClass, pM->pMeth, &sRef);
				rc = ReflectExportFuncBlock(pCtx, pOut, &sRef, "    ", pClass);
				if( rc != SXRET_OK ){
					goto done;
				}
			}else{
				SyBlobAppend(pOut, "    ", sizeof("    ")-1);
				ReflectExportPropLine(pCtx, pOut, pM->pAttr, &pM->sKey);
			}
		}
		SyBlobAppend(pOut, "  }\n", sizeof("  }\n")-1);
	}
	SyBlobAppend(pOut, "}\n", sizeof("}\n")-1);
done:
	SySetRelease(&aMembers);
	SySetRelease(&aIface);
	return rc;
}
/* ---- the five __toString() entry points ---- */
static int ReflectExportClassSelf(ph7_context *pCtx)
{
	ph7_class *pClass = ReflectClassOf(pCtx);
	SyBlob sOut;
	sxi32 rc;
	if( pClass == 0 ){
		ph7_result_string(pCtx, "", 0);
		return PH7_OK;
	}
	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);
	rc = ReflectExportClassBlock(pCtx, &sOut, pClass);
	if( rc == SXRET_OK ){
		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));
	}
	SyBlobRelease(&sOut);
	return rc == SXRET_OK ? PH7_OK : rc;
}
/* The standalone Function block for a name the extension walk handed over. */
static void ReflectExportFuncByName(ph7_context *pCtx, SyBlob *pOut,
	const char *zName, int nName)
{
	ph7_value sTarget;
	ReflectFuncRef sRef;
	PH7_MemObjInit(pCtx->pVm, &sTarget);
	ph7_value_string(&sTarget, zName, nName);
	if( ReflectFuncFill(pCtx->pVm, &sTarget, 0, &sRef) ){
		ReflectExportFuncBlock(pCtx, pOut, &sRef, "", 0);
	}
	PH7_MemObjRelease(&sTarget);
}
static int ReflectExportFuncSelf(ph7_context *pCtx)
{
	ReflectFuncRef sRef;
	SyBlob sOut;
	sxi32 rc;
	if( !ReflectFuncOfThis(pCtx, &sRef) ){
		ph7_result_string(pCtx, "", 0);
		return PH7_OK;
	}
	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);
	rc = ReflectExportFuncBlock(pCtx, &sOut, &sRef, "", ReflectOwnerOfThis(pCtx));
	if( rc == SXRET_OK ){
		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));
	}
	SyBlobRelease(&sOut);
	return rc == SXRET_OK ? PH7_OK : rc;
}
static int ReflectExportParamSelf(ph7_context *pCtx)
{
	ReflectFuncRef sRef;
	ReflectParamDesc sDesc;
	SyBlob sOut;
	if( !ReflectParamOwner(pCtx, &sRef, &sDesc) ){
		ph7_result_string(pCtx, "", 0);
		return PH7_OK;
	}
	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);
	ReflectExportParamLine(pCtx, &sOut, &sDesc);
	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
static int ReflectExportPropSelf(ph7_context *pCtx)
{
	ReflectMemberRef sRef;
	SyBlob sOut;
	SyString sKey;
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_PROP) || sRef.pAttr == 0 ){
		ph7_result_string(pCtx, "", 0);
		return PH7_OK;
	}
	SyStringInitFromBuf(&sKey, sRef.zName, sRef.nName);
	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);
	ReflectExportPropLine(pCtx, &sOut, sRef.pAttr, &sKey);
	ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));
	SyBlobRelease(&sOut);
	return PH7_OK;
}
static int ReflectExportConstSelf(ph7_context *pCtx)
{
	ReflectMemberRef sRef;
	SyBlob sOut;
	SyString sKey;
	sxi32 rc;
	if( !ReflectMemberOfThis(pCtx, &sRef, REFLECT_MEMBER_CONST) || sRef.pAttr == 0 ){
		ph7_result_string(pCtx, "", 0);
		return PH7_OK;
	}
	SyStringInitFromBuf(&sKey, sRef.zName, sRef.nName);
	SyBlobInit(&sOut, &pCtx->pVm->sAllocator);
	rc = ReflectExportConstLine(pCtx, &sOut, sRef.pClass, sRef.pAttr, &sKey);
	if( rc == SXRET_OK ){
		ph7_result_string(pCtx, (const char *)SyBlobData(&sOut), (int)SyBlobLength(&sOut));
	}
	SyBlobRelease(&sOut);
	return rc == SXRET_OK ? PH7_OK : rc;
}
/*
 * Install the Reflection API — ~25 classes, all of them declared from C.
 *
 * There is no global thunk table left to register: every `__reflect_*` and
 * `__phl_rcinfo` became a method of the class that always owned it, so
 * Reflection adds no name to php's global function namespace at all. Called
 * from PH7_VmInit while pVm->bCompilingBuiltin is set, right after the core
 * builtin chunks (Exception and friends have to exist already).
 */
PH7_PRIVATE sxi32 PH7_VmInstallReflection(ph7_vm *pVm)
{
	sxi32 rc;
	/* Where chunk 1 was: Reflector, Reflection, ReflectionException,
	 * ReflectionClass and ReflectionObject are native now. Chunks 2 and 3 name
	 * `Reflector` in their own `implements` clauses, so this has to run first. */
	rc = PH7_VmInstallReflectionClass(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Where chunk 2 was: ReflectionFunctionAbstract, ReflectionFunction,
	 * ReflectionMethod and ReflectionParameter are native now. */
	rc = PH7_VmInstallReflectionFunc(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	rc = PH7_VmInstallReflectionHookType(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Where chunk 3 was: ReflectionProperty and ReflectionClassConstant are
	 * native now, and PropertyHookType is a native ENUM. */
	rc = PH7_VmInstallReflectionMember(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Where chunk 4 was: the four type classes are native now (see
	 * PH7_VmInstallReflectionTypes). Stringable exists by this point. */
	rc = PH7_VmInstallReflectionTypes(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Where chunk 5 was: ReflectionGenerator/Fiber and the four standalone
	 * classes chunk 6 used to hold are native now. Reflector (chunk 1) exists. */
	rc = PH7_VmInstallReflectionSmall(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Where chunk 6 was: the three ReflectionEnum classes are native now, and
	 * the class DESCRIPTOR they read (__phl_rcinfo) has no callers left. */
	rc = PH7_VmInstallReflectionEnum(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Where chunk 7 was: ReflectionAttribute is native now, and with it the
	 * builder (__reflect_build_attrs) and the two helpers it needed. */
	rc = PH7_VmInstallReflectionAttribute(&(*pVm));
	if( rc != SXRET_OK ){
		return rc;
	}
	/* Where chunk 9 was: the export format is ReflectExportClassSelf() and its
	 * four siblings above. Nothing of the Reflection library is PHP any more,
	 * so vm_builtin_reflection_lib.c is gone and this IS the installer. */
	return SXRET_OK;
}
